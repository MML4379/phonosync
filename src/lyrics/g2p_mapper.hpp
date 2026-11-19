#pragma once

#include "../engine/alignment.hpp"
#include <string>
#include <vector>
#include <cstdint>

namespace lyrics {
    struct SyllableTiming {
        std::string text;
        int64_t start_ms;
        int64_t end_ms;
        bool is_held_note;
    };

    struct SyncedWord {
        std::string text;
        int64_t start_ms;
        int64_t end_ms;
        std::vector<SyllableTiming> syllables;
    };

    class G2PMapper {
        public:
            G2PMapper() = default;
            ~G2PMapper() = default;

            /**
             * Converts raw aligned word sequences into syllable timing profiles.
             * @param raw_timings Calculated word bounds
             * @param out_synced_words Target tree matrix containing syllable nodes and held statuses
             */
            void map_words_to_syllables(const std::vector<engine::WordTiming>& raw_timings, std::vector<SyncedWord>& out_synced_words);

        private:
            // Group raw phonemes into text syllables
            std::vector<std::string> reconstruct_text_syllables(const std::string& word, const std::vector<std::string>& phonemes);
    };    
} // namespace lyrics