#include "decoder.hpp"

#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

#include <iostream>
#include <algorithm>
#include <cmath>

namespace audio {
    bool AudioDecoder::decode_to_whisper(const std::string& file_path, std::vector<float>& out_pcm) {
        drwav wav;

        // open file handles using drwav
        if (!drwav_init_file(&wav, file_path.c_str(), nullptr)) {
            std::cerr << "[AudioDecoder] Failed to open or parse file: " << file_path << std::endl;
            return false;
        }

        // allocate an explicit memory buffer to fetch raw frame data
        uint64_t total_samples = wav.totalPCMFrameCount * wav.channels;
        std::vector<float> raw_samples(total_samples);

        // read the decoded float frames directly from disk into the vector
        drwav_read_pcm_frames_f32(&wav, wav.totalPCMFrameCount, raw_samples.data());
        drwav_uninit(&wav);

        // save some time - bypass processing if the file is already 16kHz and mono
        if (wav.sampleRate == 16000 && wav.channels == 1) {
            out_pcm = std::move(raw_samples);
            return true;
        }

        // pass data into the processing matrix to perform channel blending and sample conversion
        resample_pcm(raw_samples, wav.sampleRate, wav.channels, out_pcm);
        return true;
    }

    void AudioDecoder::resample_pcm(const std::vector<float>& input_samples, uint32_t sample_rate, uint32_t channels, std::vector<float>& output_samples) {
        // fold multi-channel configurations into a mono array
        std::vector<float> mono_samples;
        uint64_t total_frames = input_samples.size() / channels;
        mono_samples.reserve(total_frames);

        for (uint64_t i = 0; i < total_frames; ++i) {
            float sum = 0.0f;
            for (uint32_t c = 0; c < channels; ++c) {
                sum += input_samples[i * channels + c];
            }
            // arithmetic mean to flatten standard acoustic spaces channels uniformly
            mono_samples.push_back(sum / static_cast<float>(channels));
        }

        // if sampling frequencies match, complete processing early
        if (sample_rate == 16000) {
            output_samples = std::move(mono_samples);
            return;
        }

        // linear resampling so it's exactly 16kHz
        double resample_ratio = 16000.0 / static_cast<double>(sample_rate);
        uint64_t target_samples_count = std::floor(mono_samples.size() * resample_ratio);
        output_samples.resize(target_samples_count);

        for (uint64_t i = 0; i < target_samples_count; ++i) {
            double src_index = i / resample_ratio;
            uint64_t index_low = std::floor(src_index);
            uint64_t index_high = std::min(index_low + 1, mono_samples.size() - 1);

            double weight = src_index - index_low;

            // linear interpolation step across acoustic soundwave endpoints
            output_samples[i] = (1.0f - weight) * mono_samples[index_low] + weight * mono_samples[index_high];
        }
    }
} // namespace audio