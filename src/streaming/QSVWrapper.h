#pragma once
#include "HardwareEncoder.h"
using aura::HardwareEncoder;

// FFmpeg forward declarations
extern "C" {
struct AVCodecContext;
struct AVFrame;
struct AVPacket;
}

// Intel QuickSync Video (QSV) hardware encoder using FFmpeg h264_qsv.
// Works on Intel CPUs with integrated or discrete Xe graphics.
class QSVWrapper : public HardwareEncoder {
public:
    QSVWrapper();
    ~QSVWrapper();

    bool init(int width, int height, int bitrateMbps, int fps);
    bool encodeFrame(ID3D12Resource* frame);
    void getEncodedPacket(AVPacket* out);
    void drain();
    void shutdown();

    static bool isAvailable();

private:
    AVCodecContext* m_codecCtx = nullptr;
    AVFrame*        m_frame    = nullptr;
    AVPacket*       m_pkt      = nullptr;
    bool            m_hasPacket = false;
    int m_width = 0, m_height = 0, m_fps = 30;
};
