#pragma once

#include "../lyrics/g2p_mapper.hpp"
#include <string>
#include <vector>

namespace formatter {
    class ELRCFormatter {
        public:
            ELRCFormatter() = default;
            ~ELRCFormatter() = default;

            /**
             * Compiles the in-memory syllable synchronization tree into standard Enhanced LRC format.
             * Uses reference_lyrics line breaks when provided, falling back to natural pause / word-count breaks.
             */
            bool compile_elrc(const std::vector<lyrics::SyncedWord>& synced_words,
                              const std::string& reference_lyrics,
                              size_t fallback_words_per_line,
                              std::string& out_elrc);

            /**
             * Exports the compiled ELRC data stream directly to a file on disk.
             */
            bool save_to_file(const std::string& file_path, const std::string& elrc_content);

        private:
            std::string format_timestamp(int64_t time_ms, bool use_angle_brackets);
    };
} // namespace formatter