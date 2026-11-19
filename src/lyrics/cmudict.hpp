#pragma once

#include <string>
#include <vector>
#include <unordered_map>

namespace lyrics {

    class CMUDict {
        public:
            CMUDict() = default;
            ~CMUDict() = default;

            /**
             * Loads a standard CMU Pronouncing Dictionary file (e.g., cmudict.dict)
             * Format per line: WORD  PH1 PH2 PH3 ... (ignores alternate pronunciations like WORD(2) and comments)
             */
            static bool load_from_file(const std::string& filepath);

            /**
             * Looks up a word in the loaded dictionary (or built-in fallback)
             */
            static bool lookup(const std::string& word, std::vector<std::string>& out_phonemes);

            /**
             * Determines if a phoneme token is a vowel sound (ends with stress 0, 1, or 2)
             */
            static bool is_vowel(const std::string& phoneme) {
                if (phoneme.empty()) return false;
                char last_char = phoneme.back();
                return (last_char == '0' || last_char == '1' || last_char == '2');
            }

            /**
             * Rule-based orthographic fallback: estimates syllable count and splits
             * unknown words by vowel nuclei when missing from CMUdict.
             */
            static std::vector<std::string> split_orthographic_syllables(const std::string& word);

        private:
            static std::unordered_map<std::string, std::vector<std::string>>& get_db();
            static void init_builtin_fallback(std::unordered_map<std::string, std::vector<std::string>>& db);
    };

} // namespace lyrics