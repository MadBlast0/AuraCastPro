#pragma once
#include <cstdint>
#include <vector>
#include <functional>

extern "C" {
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
}

// Sample rate / format converter using FFmpeg libswresample.
// Converts device audio to WASAPI format (48000 Hz, float32, stereo).
class AudioResampler {
public:
    struct Format {
        int    sampleRate    = 44100;
        int    channels      = 2;
        AVSampleFormat sampleFmt = AV_SAMPLE_FMT_FLTP;
    };

    AudioResampler() = default;
    ~AudioResampler();

    // Init with input and output formats
    bool init(const Format& in, const Format& out);
    void reset();

    // Convert numFrames of input samples.
    // Returns number of output frames written into outBuf.
    int convert(const float* const* inData, int numFrames,
                float** outData, int maxOutFrames);

    // Convenience: convert interleaved input
    int convertInterleaved(const float* inInterleaved,
                           int numFrames,
                           float* outInterleaved,
                           int maxOutFrames);

    bool isInitialised() const { return m_swr != nullptr; }
    const Format& outputFormat() const { return m_out; }

private:
    SwrContext* m_swr = nullptr;
    Format m_in, m_out;

    // Planar temp buffers
    std::vector<float> m_inL, m_inR;
    std::vector<float> m_outL, m_outR;
    const float* m_inPtrs[2]  = {};
    float*       m_outPtrs[2] = {};
};
