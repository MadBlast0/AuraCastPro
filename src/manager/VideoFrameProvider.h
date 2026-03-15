#pragma once
// =============================================================================
// VideoFrameProvider.h -- QML Image Provider for video frames
// =============================================================================
#include <QQuickImageProvider>
#include <QImage>
#include <mutex>

namespace aura {

class VideoFrameProvider : public QQuickImageProvider {
public:
    VideoFrameProvider();
    
    QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override;
    
    void updateFrame(const QImage& frame);
    
private:
    QImage m_currentFrame;
    std::mutex m_mutex;
};

} // namespace aura
