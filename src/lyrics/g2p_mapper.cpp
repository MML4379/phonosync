#include "g2p_mapper.hpp"
#include "cmudict.hpp"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace lyrics {

    namespace {
        bool is_ortho_vowel(char c) {
            char lower = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return lower == 'a' || lower == 'e' || lower == 'i' ||
                   lower == 'o' || lower == 'u' || lower == 'y';
        }

        // Normalize a token for dictionary lookup (keep letters and apostrophes)
        std::string normalize_lookup_word(const std::string& raw) {
            std::string cleaned;
            cleaned.reserve(raw.size());
            for (char c : raw) {
                unsigned char uc = static_cast<unsigned char>(c);
                if (std::isalpha(uc) || c == '\'') {
                    cleaned += static_cast<char>(std::toupper(uc));
                }
            }
            // Strip leading/trailing apostrophes if not part of contraction
            while (!cleaned.empty() && cleaned.front() == '\'') cleaned.erase(cleaned.begin());
            return cleaned;
        }
    } // namespace

    std::unordered_map<std::string, std::vector<std::string>>& CMUDict::get_db() {
        static std::unordered_map<std::string, std::vector<std::string>> db;
        static bool initialized = false;
        if (!initialized) {
            init_builtin_fallback(db);
            initialized = true;
        }
        return db;
    }

    void CMUDict::init_builtin_fallback(std::unordered_map<std::string, std::vector<std::string>>& db) {
        // Minimal built-in fallback used if external cmudict.dict isn't loaded
        db = {
            {"HELLO",   {"HH", "AH0", "L", "OW1"}},
            {"FOREVER", {"F", "ER0", "EH1", "V", "ER0"}},
            {"DARKNESS",{"D", "AA1", "R", "K", "N", "AH0", "S"}},
            {"DON'T",   {"D", "OW1", "N", "T"}},
            {"CAN'T",   {"K", "AE1", "N", "T"}},
            {"WON'T",   {"W", "OW1", "N", "T"}},
            {"I'M",     {"AY1", "M"}},
            {"YOU'RE",  {"Y", "UH1", "R"}},
            {"WE'RE",   {"W", "IH1", "R"}},
            {"THEY'RE", {"DH", "EH1", "R"}},
            {"IT'S",    {"IH1", "T", "S"}},
            {"THAT'S",  {"DH", "AE1", "T", "S"}},
            {"WHAT'S",  {"W", "AH1", "T", "S"}},
            {"LOVE",    {"L", "AH1", "V"}},
            {"HEART",   {"HH", "AA1", "R", "T"}},
            {"NIGHT",   {"N", "AY1", "T"}},
            {"LIGHT",   {"L", "AY1", "T"}},
            {"SHADOW",  {"SH", "AE1", "D", "OW2"}},
            {"SILENCE", {"S", "AY1", "L", "AH0", "N", "S"}},
            {"YESTERDAY", {"Y", "EH1", "S", "T", "ER0", "D", "EY2"}},
            {"TOMORROW",  {"T", "AH0", "M", "AA1", "R", "OW2"}},
            {"BEAUTIFUL", {"B", "Y", "UW1", "T", "AH0", "F", "AH0", "L"}}
        };
    }

    bool CMUDict::load_from_file(const std::string& filepath) {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            return false;
        }

        auto& db = get_db();
        std::string line;
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == ';') continue;

            // Strip inline comments (# ...) present in newer cmudict releases
            size_t comment_pos = line.find('#');
            if (comment_pos != std::string::npos) {
                line = line.substr(0, comment_pos);
            }

            std::istringstream iss(line);
            std::string word;
            if (!(iss >> word)) continue;

            // Skip alternate pronunciations like WORD(2) so primary pronunciation wins
            if (word.find('(') != std::string::npos) continue;

            std::transform(word.begin(), word.end(), word.begin(),
                           [](unsigned char c) { return static_cast<char>(std::toupper(c)); });

            std::vector<std::string> phonemes;
            std::string ph;
            while (iss >> ph) {
                phonemes.push_back(ph);
            }

            if (!phonemes.empty()) {
                db[std::move(word)] = std::move(phonemes);
            }
        }
        return true;
    }

    bool CMUDict::lookup(const std::string& word, std::vector<std::string>& out_phonemes) {
        std::string upper_word = normalize_lookup_word(word);
        if (upper_word.empty()) return false;

        const auto& db = get_db();
        auto it = db.find(upper_word);
        if (it != db.end()) {
            out_phonemes = it->second;
            return true;
        }

        // Handle casual lyric contractions ending in IN' (e.g., LOVIN' -> LOVING)
        if (upper_word.size() > 3 && upper_word.substr(upper_word.size() - 3) == "IN'") {
            std::string ing_form = upper_word.substr(0, upper_word.size() - 1) + "G";
            it = db.find(ing_form);
            if (it != db.end()) {
                out_phonemes = it->second;
                // Replace final NG with N
                if (!out_phonemes.empty() && out_phonemes.back() == "NG") {
                    out_phonemes.back() = "N";
                }
                return true;
            }
        }

        return false;
    }

    std::vector<std::string> CMUDict::split_orthographic_syllables(const std::string& word) {
        if (word.empty()) return {};

        // Find vowel-cluster nucleus start/end indices
        std::vector<std::pair<size_t, size_t>> nuclei;
        size_t i = 0;
        while (i < word.size()) {
            if (is_ortho_vowel(word[i])) {
                size_t start = i;
                while (i < word.size() && is_ortho_vowel(word[i])) {
                    ++i;
                }
                nuclei.push_back({start, i}); // [start, end)
            } else {
                ++i;
            }
        }

        // Ignore trailing silent 'e' if there is already at least one preceding vowel nucleus
        if (nuclei.size() > 1) {
            auto last = nuclei.back();
            char last_char = static_cast<char>(std::tolower(static_cast<unsigned char>(word[last.first])));
            // Check if the last nucleus is just a single 'e' at the end of letters (ignoring trailing punctuation)
            size_t last_alpha = word.size();
            while (last_alpha > 0 && !std::isalpha(static_cast<unsigned char>(word[last_alpha - 1]))) {
                --last_alpha;
            }
            if (last.second - last.first == 1 && last_char == 'e' && last.second == last_alpha) {
                // Keep 'le' endings like "table", "little" as their own syllable
                bool is_consonant_le = (last.first >= 2 &&
                    std::tolower(static_cast<unsigned char>(word[last.first - 1])) == 'l' &&
                    !is_ortho_vowel(word[last.first - 2]));
                if (!is_consonant_le) {
                    nuclei.pop_back();
                }
            }
        }

        if (nuclei.size() <= 1) {
            return {word};
        }

        // Split midway through consonant clusters between adjacent vowel nuclei
        std::vector<std::string> chunks;
        size_t prev_split = 0;
        for (size_t n = 0; n + 1 < nuclei.size(); ++n) {
            size_t cons_start = nuclei[n].second;
            size_t cons_end = nuclei[n + 1].first;
            size_t cons_len = cons_end - cons_start;

            // V-CV (1 consonant goes to next syllable) vs VC-CV (split in middle)
            size_t split_pos = (cons_len <= 1) ? cons_start : (cons_start + cons_len / 2);
            chunks.push_back(word.substr(prev_split, split_pos - prev_split));
            prev_split = split_pos;
        }
        chunks.push_back(word.substr(prev_split));
        return chunks;
    }

    void G2PMapper::map_words_to_syllables(const std::vector<engine::WordTiming>& raw_timings, std::vector<SyncedWord>& out_synced_words) {
        out_synced_words.clear();

        for (const auto& raw : raw_timings) {
            SyncedWord word;
            word.text = raw.text;
            word.start_ms = raw.start_ms;
            word.end_ms = raw.end_ms;

            int64_t total_duration = word.end_ms - word.start_ms;
            std::vector<std::string> phonemes;
            std::vector<std::string> text_chunks;

            if (CMUDict::lookup(word.text, phonemes)) {
                text_chunks = reconstruct_text_syllables(word.text, phonemes);
            } else {
                // Rule-based orthographic fallback for slang, names, and OOV lyrics
                text_chunks = CMUDict::split_orthographic_syllables(word.text);
            }

            if (text_chunks.empty()) {
                text_chunks.push_back(word.text);
            }

            size_t syllable_count = text_chunks.size();
            int64_t chunk_duration = total_duration / static_cast<int64_t>(syllable_count);
            int64_t current_time = word.start_ms;

            for (size_t i = 0; i < syllable_count; ++i) {
                SyllableTiming syl;
                syl.text = text_chunks[i];
                syl.start_ms = current_time;
                syl.end_ms = current_time + chunk_duration;
                syl.is_held_note = false;

                if (chunk_duration > 500 && i == syllable_count - 1) {
                    syl.is_held_note = true;
                }

                word.syllables.push_back(syl);
                current_time = syl.end_ms;
            }

            if (!word.syllables.empty()) {
                word.syllables.back().end_ms = word.end_ms;
            }

            out_synced_words.push_back(word);
        }
    }

    std::vector<std::string> G2PMapper::reconstruct_text_syllables(const std::string& word, const std::vector<std::string>& phonemes) {
        size_t target_syllables = 0;
        for (const auto& ph : phonemes) {
            if (CMUDict::is_vowel(ph)) target_syllables++;
        }

        if (target_syllables <= 1) {
            return {word};
        }

        // First try vowel-nucleus splitting and check if it matches CMUdict's exact syllable count
        std::vector<std::string> ortho_chunks = CMUDict::split_orthographic_syllables(word);
        if (ortho_chunks.size() == target_syllables) {
            return ortho_chunks;
        }

        // Fallback: proportional character slicing if orthographic vowels differ from phonetic vowels
        // (e.g., silent vowels or diphthongs)
        std::vector<std::string> chunks;
        size_t chars_per_chunk = std::max<size_t>(1, word.length() / target_syllables);
        size_t marker = 0;
        for (size_t i = 0; i < target_syllables; ++i) {
            if (i == target_syllables - 1 || marker + chars_per_chunk >= word.length()) {
                chunks.push_back(word.substr(marker));
                break;
            } else {
                chunks.push_back(word.substr(marker, chars_per_chunk));
                marker += chars_per_chunk;
            }
        }
        return chunks;
    }

} // namespace lyrics