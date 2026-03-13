#include "../pch.h"  // PCH
#include "AudioResampler.h"
#include "../utils/Logger.h"
#include <stdexcept>
#include <cstring>

AudioResampler::~AudioResampler() { reset(); }

bool AudioResampler::init(const Format& in, const Format& out) {
    reset();
    m_in  = in;
    m_out = out;

    // Short-circuit if formats match
    if (in.sampleRate == out.sampleRate &&
        in.channels   == out.channels   &&
        in.sampleFmt  == out.sampleFmt) {
        LOG_DEBUG("AudioResampler: formats match -- passthrough mode");
        return true;
    }

    m_swr = swr_alloc();
    if (!m_swr) return false;

    // Input channel layout
    AVChannelLayout inLayout = {}, outLayout = {};
    av_channel_layout_default(&inLayout,  in.channels);
    av_channel_layout_default(&outLayout, out.channels);

    av_opt_set_chlayout(m_swr, "in_chlayout",  &inLayout,  0);
    av_opt_set_chlayout(m_swr, "out_chlayout", &outLayout, 0);
    av_opt_set_int(m_swr, "in_sample_rate",  in.sampleRate,  0);
    av_opt_set_int(m_swr, "out_sample_rate", out.sampleRate, 0);
    av_opt_set_sample_fmt(m_swr, "in_sample_fmt",  in.sampleFmt,  0);
    av_opt_set_sample_fmt(m_swr, "out_sample_fmt", out.sampleFmt, 0);

    if (swr_init(m_swr) < 0) {
        LOG_ERROR("AudioResampler: swr_init failed");
        swr_free(&m_swr);
        m_swr = nullptr;
        return false;
    }

    LOG_INFO("AudioResampler: {}Hz -> {}Hz, {} -> {} ch",
             in.sampleRate, out.sampleRate, in.channels, out.channels);
    return true;
}

void AudioResampler::reset() {
    if (m_swr) { swr_free(&m_swr); m_swr = nullptr; }
}

int AudioResampler::convert(const float* const* inData,
                              int numFrames,
                              float** outData,
                              int maxOutFrames) {
    if (!m_swr) {
        // Passthrough
        for (int c = 0; c < m_out.channels && c < m_in.channels; c++) {
            int frames = std::min(numFrames, maxOutFrames);
            std::memcpy(outData[c], inData[c], frames * sizeof(float));
        }
        return std::min(numFrames, maxOutFrames);
    }

    uint8_t* const* outBuf = reinterpret_cast<uint8_t* const*>(outData);
    const uint8_t** inBuf = reinterpret_cast<const uint8_t**>(
                                const_cast<const float**>(inData));
    int n = swr_convert(m_swr, outBuf, maxOutFrames, inBuf, numFrames);
    if (n < 0) {
        LOG_WARN("AudioResampler: swr_convert returned {}", n);
        return 0;
    }
    return n;
}

int AudioResampler::convertInterleaved(const float* inInterleaved,
                                        int numFrames,
                                        float* outInterleaved,
                                        int maxOutFrames) {
    int ch = m_in.channels;
    // De-interleave input
    m_inL.resize(numFrames);
    m_inR.resize(numFrames);
    for (int i = 0; i < numFrames; i++) {
        m_inL[i] = inInterleaved[i * ch + 0];
        if (ch > 1) m_inR[i] = inInterleaved[i * ch + 1];
        else        m_inR[i] = m_inL[i];
    }

    // Allocate output planar buffers
    m_outL.resize(maxOutFrames);
    m_outR.resize(maxOutFrames);
    m_inPtrs[0]  = m_inL.data();
    m_inPtrs[1]  = m_inR.data();
    m_outPtrs[0] = m_outL.data();
    m_outPtrs[1] = m_outR.data();

    int n = convert(m_inPtrs, numFrames, m_outPtrs, maxOutFrames);

    // Re-interleave output
    int outCh = m_out.channels;
    for (int i = 0; i < n; i++) {
        outInterleaved[i * outCh + 0] = m_outL[i];
        if (outCh > 1) outInterleaved[i * outCh + 1] = m_outR[i];
    }
    return n;
}

