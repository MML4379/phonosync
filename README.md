# Phonosync

A high-performance CLI tool that uses local machine learning through [whisper.cpp](https://github.com/ggml-org/whisper.cpp) and phonetic dictionaries to automatically align audio with text down to the syllable. Developers and creators can use it to generate syllable-synced Enhanced LRC (`.elrc` / `.lrc`) files for music players, karaoke setups, visualizers, and video captions.

## How It Works

1. **Audio Conditioning (`src/audio/`)**: Decodes input `.wav` audio into a 16 kHz mono 32-bit float PCM buffer via `dr_wav`.
2. **ML Token Alignment (`src/engine/`)**: Runs local Whisper inference with Dynamic Time Warping (DTW) cross-attention timestamps, using your plain-text lyrics as prompt conditioning and merging BPE subword tokens into full words.
3. **Grapheme-to-Phoneme Mapping (`src/lyrics/`)**: Looks up words in the [CMU Pronouncing Dictionary](https://github.com/cmusphinx/cmudict) (`cmudict.dict`) to count vowel nuclei and split words into syllables, falling back to an orthographic vowel-cluster splitter for slang, names, and out-of-vocabulary terms.
4. **Enhanced LRC Serialization (`src/formatter/`)**: Outputs standard `[mm:ss.xx]` line timestamps and inline `<mm:ss.xx>` syllable timestamps while preserving line breaks from your reference lyrics.

## Prerequisites

- **C++17 Compiler**: Clang++ (LLVM), GCC, or MSVC
- **CMake**: Version 3.20 or newer
- **Build Tool**: Ninja, Make, or Visual Studio Build Tools
- **Whisper GGML Model**: Any standard (non-distilled) Whisper `.bin` model such as `ggml-base.en.bin`, `ggml-small.en.bin`, `ggml-medium.en.bin`, or `ggml-large-v3.bin`

## Setup & Building

### 1. Clone the Repository

If you haven't vendored `whisper.cpp` yet, clone it into `third_party/whisper`:

```bash
git clone https://github.com/ggml-org/whisper.cpp.git third_party/whisper
```

### 2. Download a Model & Dictionary

Create `models/` and `data/` directories in the project root and fetch a GGML model along with the CMU Pronouncing Dictionary:

```bash
mkdir -p models data

# Download a Whisper GGML model (e.g., small.en or large-v3)
curl -L https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-small.en.bin -o models/ggml-small.en.bin

# Download the CMU Pronouncing Dictionary (~134k words)
curl -L https://raw.githubusercontent.com/cmusphinx/cmudict/master/cmudict.dict -o data/cmudict.dict
```

> **Note:** Avoid v1 `distil-whisper` models, as they do not emit token-level timestamps.

### 3. Compile

Configure and build a static release binary with CMake:

```bash
cmake -S . -B build -G "Ninja" -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

*(On Windows with LLVM/Clang, run these commands from a Visual Studio Developer PowerShell so the Windows SDK and CRT libraries are on your path.)*

## Usage

```bash
./build/phonosync <model_path> <audio_path> <lyrics_txt> <output_elrc> [cmudict_path]
```

### Arguments

| Argument | Required | Description |
| :--- | :---: | :--- |
| `model_path` | Yes | Path to a Whisper `.bin` model file (e.g., `models/ggml-small.en.bin`). |
| `audio_path` | Yes | Path to the input `.wav` audio file (16 kHz mono PCM). |
| `lyrics_txt` | Yes | Plain-text lyrics file used to condition Whisper and preserve line breaks. |
| `output_elrc` | Yes | Destination path for the generated Enhanced LRC file (`.elrc` or `.lrc`). |
| `cmudict_path` | No | Custom path to `cmudict.dict`. Auto-detected from `./` or `data/` if omitted. |

### Example

If your source track is an MP3 or stereo WAV, convert it to 16 kHz mono PCM first using `ffmpeg`:

```bash
ffmpeg -i song.mp3 -ar 16000 -ac 1 -c:a pcm_s16le song.wav
```

Then run `phonosync`:

```bash
./build/phonosync models/ggml-small.en.bin song.wav lyrics.txt song.lrc
```

### Sample Output

```text
[00:18.40] "No <00:18.75>time <00:19.10>for <00:19.35>good<00:19.70>bye", <00:20.40>he <00:20.65>said<00:21.30>
[00:22.10] As <00:22.35>he <00:22.55>fa<00:22.85>ded <00:23.15>a<00:23.45>way<00:24.10>
```

> **Tip for Media Players:** Most desktop players (such as foobar2000 with `foo_openlyrics` or `foo_uie_eslyric`, and MusicBee) automatically recognize Enhanced LRC `<mm:ss.xx>` tags when saved with a `.lrc` extension alongside the audio file.

## License

GNU Affero General Public License v3.0
