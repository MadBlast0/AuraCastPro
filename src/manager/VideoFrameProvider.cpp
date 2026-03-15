// =============================================================================
// VideoFrameProvider.cpp -- QML Image Provider for video frames
// =============================================================================
#include "../pch.h"
#include "VideoFrameProvider.h"
#include <QImage>

namespace aura {

VideoFrameProvider::VideoFrameProvider()
    : QQuickImageProvider(QQuickImageProvider::Image) {
}

QImage VideoFrameProvider::requestImage(const QString& /*id*/, QSize* size, const QSize& requestedSize) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_currentFrame.isNull()) {
        // Return a small placeholder if no frame available
        QImage placeholder(320, 240, QImage::Format_RGB888);
        placeholder.fill(Qt::black);
        if (size) *size = placeholder.size();
        return placeholder;
    }
    
    if (size) {
        *size = m_currentFrame.size();
    }
    
    // Scale if requested
    if (requestedSize.isValid() && requestedSize != m_currentFrame.size()) {
        return m_currentFrame.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    
    return m_currentFrame;
}

void VideoFrameProvider::updateFrame(const QImage& frame) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_currentFrame = frame;
}

} // namespace aura
