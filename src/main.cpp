#include "audio/decoder.hpp"
#include "engine/alignment.hpp"
#include "lyrics/cmudict.hpp"
#include "lyrics/g2p_mapper.hpp"
#include "formatter/elrc.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

std::string read_text_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

int main(int argc, char* argv[]) {
    if (argc < 5) {
        std::cout << "=========================================================================\n"
                  << "|          Phonosync CLI - Syllable-level Lyric Aligner Engine          |\n"
                  << "| Usage:                                                                |\n"
                  << "|   ./phonosync <model_path> <audio_path> <lyrics_txt> <output_elrc> [cmudict_path] |\n"
                  << "| Example:                                                              |\n"
                  << "|   ./phonosync ggml-base.en.bin song.wav lyrics.txt sync.elrc          |\n"
                  << "=========================================================================\n";
        return 1;
    }

    std::string model_path = argv[1];
    std::string audio_path = argv[2];
    std::string lyrics_path = argv[3];
    std::string output_path = argv[4];
    std::string dict_path = (argc >= 6) ? argv[5] : "";

    std::cout << "[Phonosync] Starting Phonosync Execution Pipeline...\n";

    // Load CMU Pronouncing Dictionary (explicit path or default search locations)
    bool dict_loaded = false;
    if (!dict_path.empty()) {
        dict_loaded = lyrics::CMUDict::load_from_file(dict_path);
    } else {
        dict_loaded = lyrics::CMUDict::load_from_file("cmudict.dict") ||
                      lyrics::CMUDict::load_from_file("data/cmudict.dict") ||
                      lyrics::CMUDict::load_from_file("../data/cmudict.dict");
    }

    if (dict_loaded) {
        std::cout << "[Phonosync] Loaded external CMU Pronouncing Dictionary.\n";
    } else {
        std::cout << "[Phonosync] [INFO] External cmudict.dict not found; using built-in core + orthographic fallback.\n";
    }

    // Stage 1 - Audio Ingestion Layer
    std::cout << "[Phonosync] Decoding and conditioning audio file...\n";
    audio::AudioDecoder decoder;
    std::vector<float> pcm_buffer;

    if (!decoder.decode_to_whisper(audio_path, pcm_buffer)) {
        std::cerr << "[Phonosync] [!] Audio ingestion phase aborted.\n";
        return -1;
    }
    std::cout << "[Phonosync] Audio file decode success. Audio buffer allocated: "
              << (pcm_buffer.size() * sizeof(float)) / (1024 * 1024) << " MB ("
              << pcm_buffer.size() << " samples)\n";

    std::string plain_lyrics = read_text_file(lyrics_path);
    if (plain_lyrics.empty()) {
        std::cerr << "[Phonosync] [WARN] Plain text lyric file is empty or missing. "
                  << "Whisper will run unconstrained, which creates a higher risk of hallucination.\n";
    }

    // Stage 2 - ML Alignment Engine Layer
    std::cout << "[Phonosync] Loading GGML/Whisper model and initializing runtime context...\n";
    engine::AlignmentEngine alignment_engine;
    if (!alignment_engine.initialize(model_path)) {
        std::cerr << "[Phonosync] [!] ML Engine context initialization aborted.\n";
        return -2;
    }

    std::cout << "[Phonosync] Running hardware-accelerated token-level cross-attention tracking...\n";
    std::vector<engine::WordTiming> raw_word_timings;
    if (!alignment_engine.process_alignment(pcm_buffer, plain_lyrics, raw_word_timings)) {
        std::cerr << "[Phonosync] [!] ML inference process aborted.\n";
        return -3;
    }
    std::cout << "[Phonosync] Successfully ran ML isolation. Isolated " << raw_word_timings.size() << " merged word-tokens.\n";

    // Stage 3 - Phonetic sub-syllable splitting layer
    std::cout << "[Phonosync] Processing Grapheme to Phoneme mappings via CMUDict...\n";
    lyrics::G2PMapper phonetic_mapper;
    std::vector<lyrics::SyncedWord> fully_synced_words;

    phonetic_mapper.map_words_to_syllables(raw_word_timings, fully_synced_words);
    std::cout << "            Successfully mapped phonemes. Syllable structural boundaries and held notes processed.\n";

    // Stage 4 - File layout formatter serialization layer
    std::cout << "[Phonosync] Compiling in-memory structures into Enhanced LRC data streams...\n";
    formatter::ELRCFormatter elrc_formatter;
    std::string final_elrc_output;

    size_t fallback_words_per_line = 10;
    if (!elrc_formatter.compile_elrc(fully_synced_words, plain_lyrics, fallback_words_per_line, final_elrc_output)) {
        std::cerr << "[Phonosync] [!] Formatting stream compilation aborted.\n";
        return -4;
    }

    std::cout << "            Writing file descriptor data to disk: " << output_path << std::endl;
    if (!elrc_formatter.save_to_file(output_path, final_elrc_output)) {
        std::cerr << "[Phonosync] [!] Disk file IO write sequence failed.\n";
        return -5;
    }

    std::cout << "\n"
              << "================================================\n"
              << "| Execution completed successfully!            |\n"
              << "| File saved -> " << output_path << "          |\n"
              << "================================================\n";

    return 0;
}