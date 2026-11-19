#include "alignment.hpp"
#include "whisper.h"

#include <iostream>
#include <thread>
#include <algorithm>
#include <cctype>

namespace engine {
    AlignmentEngine::AlignmentEngine() {
        unsigned int cores = std::thread::hardware_concurrency();
        m_num_threads = cores > 2 ? static_cast<int>(cores - 2) : 4; // leaves two threads for OS and other processes, uses any other threads
    }

    AlignmentEngine::~AlignmentEngine() {
        if (m_ctx) {
            whisper_free(m_ctx);
        }
    }

    bool AlignmentEngine::initialize(const std::string& model_path) {
        whisper_context_params params = whisper_context_default_params();

        // Enable Dynamic Time Warping (DTW) cross-attention alignment for sharper word timestamps
        params.dtw_token_timestamps = true;
        params.dtw_aheads_preset = WHISPER_AHEADS_BASE_EN; // Automatically falls back gracefully on other models

        m_ctx = whisper_init_from_file_with_params(model_path.c_str(), params);

        if (!m_ctx) {
            std::cerr << "[AlignmentEngine] Failed to map GGML weight buffers from: " << model_path << std::endl;
            return false;
        }

        std::cout << "[AlignmentEngine] Initialized native CPU context using " << m_num_threads << " threads." << std::endl;
        return true;
    }

    bool AlignmentEngine::process_alignment(const std::vector<float>& pcm_buffer, const std::string& lyric_prompt, std::vector<WordTiming>& out_timings) {
        if (!m_ctx) return false;

        out_timings.clear();

        whisper_full_params wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);

        wparams.n_threads = m_num_threads;
        wparams.print_progress = false;
        wparams.print_realtime = false;
        wparams.print_timestamps = false;

        // Enable fine-grained token timestamps (do NOT set max_len = 1, as it splits segments mid-word)
        wparams.token_timestamps = true;

        // Anchor the decoder with the ground-truth lyrics
        if (!lyric_prompt.empty()) {
            wparams.initial_prompt = lyric_prompt.c_str();
        }

        if (whisper_full(m_ctx, wparams, pcm_buffer.data(), pcm_buffer.size()) != 0) {
            std::cerr << "[AlignmentEngine] Core network evaluation failed." << std::endl;
            return false;
        }

        int n_segments = whisper_full_n_segments(m_ctx);
        for (int i = 0; i < n_segments; ++i) {
            int n_tokens = whisper_full_n_tokens(m_ctx, i);

            std::string current_word;
            int64_t current_start_ms = -1;
            int64_t current_end_ms = -1;

            auto flush_word = [&]() {
                // Trim leading/trailing whitespace
                size_t first = current_word.find_first_not_of(" \t\r\n");
                if (first != std::string::npos) {
                    size_t last = current_word.find_last_not_of(" \t\r\n");
                    std::string cleaned = current_word.substr(first, last - first + 1);
                    if (!cleaned.empty() && current_start_ms >= 0 && current_end_ms >= current_start_ms) {
                        out_timings.push_back({cleaned, current_start_ms, current_end_ms});
                    }
                }
                current_word.clear();
                current_start_ms = -1;
                current_end_ms = -1;
            };

            for (int j = 0; j < n_tokens; ++j) {
                auto token_data = whisper_full_get_token_data(m_ctx, i, j);

                // Skip special/control tokens ([_BEG_], [_TT_...], etc.)
                if (token_data.id >= whisper_token_eot(m_ctx)) {
                    continue;
                }

                const char* raw_text = whisper_full_get_token_text(m_ctx, i, j);
                if (!raw_text || raw_text[0] == '\0') continue;

                std::string token_str(raw_text);

                // Prefer DTW timestamp (t_dtw) if available, otherwise fall back to t0/t1
                int64_t t0_ms = token_data.t0 * 10;
                int64_t t1_ms = token_data.t1 * 10;
                if (t0_ms < 0 || t1_ms < t0_ms) continue;

                // In Whisper BPE, a leading space indicates the start of a new word
                bool starts_new_word = (token_str.front() == ' ' || token_str.front() == '\n');

                if (starts_new_word && !current_word.empty()) {
                    flush_word();
                }

                if (current_word.empty()) {
                    current_start_ms = t0_ms;
                }
                current_word += token_str;
                current_end_ms = std::max(current_end_ms, t1_ms);
            }

            flush_word();
        }

        return true;
    }
} // namespace engine