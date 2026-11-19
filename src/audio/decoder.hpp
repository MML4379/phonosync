#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace audio {
    class AudioDecoder {
        public:
            AudioDecoder() = default;
            ~AudioDecoder() = default;

            // disallow copying to protect large memory allocations
            AudioDecoder(const AudioDecoder&) = delete;
            AudioDecoder& operator=(const AudioDecoder&) = delete;

            /**
             * Loads an audio file, downmixes it to mono, and resamples to 16kHz.
             * @param file_path System path to the input audio file
             * @param out_pcm Vector to be populated with 32-bit mono float data
             * @return true on success, false if file format or decoding fails.
             */
            bool decode_to_whisper(const std::string& file_path, std::vector<float>& out_pcm);

        private:
            // downsample audio if source is not 16kHz
            void resample_pcm(const std::vector<float>& input_samples, uint32_t sample_rate, uint32_t channels, std::vector<float>& output_samples);
    };
} // namespace audio