#pragma once
#include "HardwareEncoder.h"
using aura::HardwareEncoder;

// Intel QuickSync Video (QSV) hardware encoder using FFmpeg h264_qsv.
// Works on Intel CPUs with integrated or discrete Xe graphics.
class QSVWrapper : public HardwareEncoder {
public:
    QSVWrapper();
    ~QSVWrapper() override;

    bool init(int width, int height, int bitrateMbps, int fps) override;
    bool encodeFrame(ID3D12Resource* frame) override;
    void getEncodedPacket(AVPacket* out) override;
    void drain() override;
    void shutdown() override;

    static bool isAvailable();

private:
    AVCodecContext* m_codecCtx = nullptr;
    AVFrame*        m_frame    = nullptr;
    AVPacket*       m_pkt      = nullptr;
    bool            m_hasPacket = false;
    int m_width = 0, m_height = 0, m_fps = 30;
};
