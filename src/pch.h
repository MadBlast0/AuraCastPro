#pragma once

// ── Windows & DirectX ────────────────────────────────────────────────────────
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
// Winsock2 MUST be included before windows.h to avoid macro conflicts
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <wrl/client.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <dxgidebug.h>

// ── Qt ───────────────────────────────────────────────────────────────────────
#ifdef QT_CORE_LIB
#include <QObject>
#include <QApplication>
#include <QString>
#include <QStringList>
#include <QDebug>
#include <QTimer>
#include <QThread>
#include <QMutex>
#include <QMutexLocker>
#include <QVariant>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#endif

// ── Boost ────────────────────────────────────────────────────────────────────
#include <boost/asio.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>

// ── spdlog ───────────────────────────────────────────────────────────────────
#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

// ── nlohmann JSON ────────────────────────────────────────────────────────────
#include <nlohmann/json.hpp>

// ── OpenSSL ──────────────────────────────────────────────────────────────────
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <openssl/bn.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>

// ── FFmpeg ───────────────────────────────────────────────────────────────────
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
#include <libswresample/swresample.h>
}

// ── Standard Library ─────────────────────────────────────────────────────────
#include <string>
#include <vector>
#include <array>
#include <map>
#include <unordered_map>
#include <set>
#include <queue>
#include <deque>
#include <memory>
#include <functional>
#include <algorithm>
#include <chrono>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <optional>
#include <variant>
#include <span>
#include <cstdint>
#include <cstring>
#include <cassert>
#include <stdexcept>
#include <filesystem>