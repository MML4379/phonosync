#pragma once

#include <string>
#include <vector>
#include <cstdint>

// forward declaration of internal whisper context to keep headers hidden
struct whisper_context;

namespace engine {
    struct WordTiming {
        std::string text;
        int64_t start_ms;
        int64_t end_ms;
    };

    class AlignmentEngine {
        public:
            AlignmentEngine();
            ~AlignmentEngine();

            // prevent copy semantics to protect the raw model pointers
            AlignmentEngine(const AlignmentEngine&) = delete;
            AlignmentEngine& operator=(const AlignmentEngine&) = delete;

            /**
             * Initializes the Whisper/GGML runtime context
             * @param model_path Path to the quantized binary Whisper model weights file (e.g., ggml-base.en.bin)
             * @return true on successful allocation, false if weights fail to map.
             */
            bool initialize(const std::string& model_path);

            /**
             * Runs hardware-accelerated CPU inference to align text timing
             * @param pcm_buffer Contextually conditioned 16kHz audio samples
             * @param lyric_prompt Plain text of full lyrics to force network alignment
             * @param out_timings Target vector to populate with computed word markers
             * @return true on successful inference completion
             */
            bool process_alignment(const std::vector<float>& pcm_buffer, const std::string& lyric_prompt, std::vector<WordTiming>& out_timings);

        private:
            whisper_context* m_ctx = nullptr;
            int m_num_threads = 4;
    };
} // namespace engine