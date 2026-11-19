#include "elrc.hpp"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace formatter {

    namespace {
        // Normalize a word to lowercase alphanumeric + apostrophe for matching against lyrics.txt
        std::string normalize_token(const std::string& s) {
            std::string out;
            out.reserve(s.size());
            for (char c : s) {
                unsigned char uc = static_cast<unsigned char>(c);
                if (std::isalnum(uc) || c == '\'') {
                    out += static_cast<char>(std::tolower(uc));
                }
            }
            return out;
        }

        // Check if a token is pure punctuation or a trailing contraction fragment (e.g., "'s", "'t", ",")
        bool attaches_to_previous(const std::string& text) {
            if (text.empty()) return false;
            if (text.front() == '\'') return true;
            for (char c : text) {
                if (std::isalnum(static_cast<unsigned char>(c))) return false;
            }
            return true; // Pure punctuation like ",", ".", "!", "?"
        }

        // Parse reference lyrics into lines of normalized words to preserve original line structure
        std::vector<std::vector<std::string>> parse_reference_lines(const std::string& reference_lyrics) {
            std::vector<std::vector<std::string>> lines;
            std::istringstream stream(reference_lyrics);
            std::string line;
            while (std::getline(stream, line)) {
                std::istringstream word_stream(line);
                std::string raw_word;
                std::vector<std::string> norm_words;
                while (word_stream >> raw_word) {
                    std::string norm = normalize_token(raw_word);
                    if (!norm.empty()) {
                        norm_words.push_back(norm);
                    }
                }
                if (!norm_words.empty()) {
                    lines.push_back(std::move(norm_words));
                }
            }
            return lines;
        }
    } // namespace

    std::string ELRCFormatter::format_timestamp(int64_t time_ms, bool use_angle_brackets) {
        if (time_ms < 0) time_ms = 0;

        int64_t total_seconds = time_ms / 1000;
        int64_t minutes = total_seconds / 60;
        int64_t seconds = total_seconds % 60;
        int64_t hundredths = (time_ms % 1000) / 10; // Centiseconds (00-99)

        std::ostringstream oss;
        char open_b = use_angle_brackets ? '<' : '[';
        char close_b = use_angle_brackets ? '>' : ']';

        oss << open_b
            << std::setw(2) << std::setfill('0') << minutes << ":"
            << std::setw(2) << std::setfill('0') << seconds << "."
            << std::setw(2) << std::setfill('0') << hundredths
            << close_b;

        return oss.str();
    }

    bool ELRCFormatter::compile_elrc(const std::vector<lyrics::SyncedWord>& synced_words,
                                     const std::string& reference_lyrics,
                                     size_t fallback_words_per_line,
                                     std::string& out_elrc) {
        if (synced_words.empty()) return false;

        auto ref_lines = parse_reference_lines(reference_lyrics);
        size_t ref_line_idx = 0;
        size_t ref_word_idx = 0;

        std::ostringstream buffer;
        size_t line_word_count = 0;
        bool line_started = false;

        for (size_t i = 0; i < synced_words.size(); ++i) {
            const auto& word = synced_words[i];
            bool attach_prev = attaches_to_previous(word.text);

            // Start a new line with [mm:ss.xx]
            if (!line_started) {
                buffer << format_timestamp(word.start_ms, false) << " ";
                line_started = true;
                line_word_count = 0;
            } else if (!attach_prev) {
                // Space between words, but NO space before punctuation or contractions ("'s")
                buffer << " ";
            }

            // Emit standard Enhanced LRC inline <mm:ss.xx> tags before every syllable
            for (size_t j = 0; j < word.syllables.size(); ++j) {
                const auto& syllable = word.syllables[j];

                // For the very first word of a line, [mm:ss.xx] already marks word.start_ms,
                // so only emit <mm:ss.xx> on interior syllables (j > 0) or subsequent words (line_word_count > 0)
                if (j > 0 || line_word_count > 0) {
                    buffer << format_timestamp(syllable.start_ms, true);
                }

                buffer << syllable.text;

                // Standard ELRC represents held notes by placing the syllable's end timestamp tag
                // after the text (no literal '~' character in the lyrics)
                if (syllable.is_held_note) {
                    buffer << format_timestamp(syllable.end_ms, true);
                }
            }

            if (!attach_prev) {
                line_word_count++;
            }

            // Decide whether to break the line after this word
            bool should_break = false;
            std::string norm_current = normalize_token(word.text);

            if (!ref_lines.empty() && ref_line_idx < ref_lines.size() && !norm_current.empty()) {
                // Advance cursor in reference lyrics if current word matches expected word
                if (ref_lines[ref_line_idx][ref_word_idx] == norm_current) {
                    ref_word_idx++;
                    if (ref_word_idx >= ref_lines[ref_line_idx].size()) {
                        should_break = true;
                        ref_line_idx++;
                        ref_word_idx = 0;
                    }
                } else if (ref_word_idx + 1 < ref_lines[ref_line_idx].size() &&
                           ref_lines[ref_line_idx][ref_word_idx + 1] == norm_current) {
                    // Whisper skipped 1 word in the reference line; resync cursor
                    ref_word_idx += 2;
                    if (ref_word_idx >= ref_lines[ref_line_idx].size()) {
                        should_break = true;
                        ref_line_idx++;
                        ref_word_idx = 0;
                    }
                }
            }

            // Fallback line break on long vocal pauses (> 800ms gap to next word) or max word count
            if (!should_break && i + 1 < synced_words.size()) {
                int64_t gap_ms = synced_words[i + 1].start_ms - word.end_ms;
                bool next_attaches = attaches_to_previous(synced_words[i + 1].text);
                if (!next_attaches && (gap_ms > 800 || line_word_count >= fallback_words_per_line)) {
                    should_break = true;
                    if (ref_line_idx < ref_lines.size() && ref_word_idx > 0) {
                        ref_line_idx++;
                        ref_word_idx = 0;
                    }
                }
            }

            if (should_break || i == synced_words.size() - 1) {
                buffer << "\n";
                line_started = false;
                line_word_count = 0;
            }
        }

        out_elrc = buffer.str();
        return true;
    }

    bool ELRCFormatter::save_to_file(const std::string& file_path, const std::string& elrc_content) {
        std::ofstream out_file(file_path, std::ios::out | std::ios::trunc);
        if (!out_file.is_open()) {
            std::cerr << "[ELRCFormatter] Failed to open file path for writing: " << file_path << std::endl;
            return false;
        }

        out_file << elrc_content;
        out_file.close();
        return true;
    }

} // namespace formatter