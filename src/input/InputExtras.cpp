#include "InputExtras.h"
#include "AndroidControlBridge.h"
#include "../utils/Logger.h"
#include <gdiplus.h>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <thread>
#include <atomic>
#include <cstring>
#include <filesystem>

#pragma comment(lib, "gdiplus.lib")

// ─── GesturePassthrough ───────────────────────────────────────────────────────

bool GesturePassthrough::handlePointerMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM) {
    if (msg != WM_POINTERDOWN && msg != WM_POINTERUP && msg != WM_POINTERUPDATE)
        return false;

    UINT32 ptId = GET_POINTERID_WPARAM(wp);
    POINTER_INFO pi = {};
    if (!GetPointerInfo(ptId, &pi)) return false;

    POINT pt = pi.ptPixelLocation;
    ScreenToClient(hwnd, &pt);

    TouchPoint tp;
    tp.id   = ptId;
    tp.x    = (float)pt.x;
    tp.y    = (float)pt.y;
    tp.down = (msg == WM_POINTERDOWN || msg == WM_POINTERUPDATE);

    if (msg == WM_POINTERDOWN)        m_points[ptId] = tp;
    else if (msg == WM_POINTERUPDATE) m_points[ptId] = tp;
    else if (msg == WM_POINTERUP)     m_points.erase(ptId);

    processGesture();
    return true;
}

void GesturePassthrough::processGesture() {
    if (!m_bridge) return;

    if (m_points.size() == 1) {
        // ── Single finger: tap or drag ────────────────────────────────────────
        auto& p = m_points.begin()->second;
        if (p.down)
            m_bridge->sendTap((int)mapX(p.x), (int)mapY(p.y));

    } else if (m_points.size() == 2) {
        // ── Two fingers: pinch-to-zoom or scroll ──────────────────────────────
        auto it = m_points.begin();
        auto& p1 = it->second; ++it;
        auto& p2 = it->second;

        float dx = p2.x - p1.x;
        float dy = p2.y - p1.y;
        float dist = std::sqrt(dx*dx + dy*dy);

        float cx = (p1.x + p2.x) / 2.0f;
        float cy = (p1.y + p2.y) / 2.0f;

        if (m_prevTwoFingerDist > 0.0f) {
            float delta = dist - m_prevTwoFingerDist;

            // Determine if this is a pinch or a scroll
            // Pinch: fingers moving apart/together (dist changes significantly)
            // Scroll: both fingers moving in same direction (dist stable)
            float absMotion = std::abs(delta);
            bool  isPinch   = absMotion > 5.0f; // pixels threshold

            if (isPinch) {
                // Pinch-to-zoom via adb shell input swipe (two-touch simulation)
                // Send as a scale gesture centred on cx,cy
                int   dcx = (int)mapX(cx), dcy = (int)mapY(cy);
                float scale = dist / (m_prevTwoFingerDist + 0.001f);
                int   step  = (int)(50.0f * (scale - 1.0f));
                // ADB doesn't support true multi-touch pinch without root.
                // Best effort: send a swipe from centre outward/inward.
                m_bridge->sendSwipe(dcx, dcy,
                                    dcx + step, dcy + step,
                                    80); // 80ms duration
            } else {
                // Scroll: two fingers moving together → vertical scroll
                float scrollDy = (cy - m_prevTwoFingerCY);
                if (std::abs(scrollDy) > 2.0f) {
                    int   dcx = (int)mapX(cx), dcy = (int)mapY(cy);
                    int   amount = (int)(scrollDy * 3.0f); // scale factor
                    m_bridge->sendSwipe(dcx, dcy,
                                        dcx, dcy - amount,
                                        60);
                }
            }
        }

        m_prevTwoFingerDist = dist;
        m_prevTwoFingerCX   = cx;
        m_prevTwoFingerCY   = cy;

    } else if (m_points.size() == 3) {
        // ── Three fingers: Android system gesture ─────────────────────────────
        // Three-finger swipe up → recent apps (KEYCODE_APP_SWITCH = 187)
        // Three-finger swipe down → notifications (not a standard keycode)
        // Detect direction from centroid movement
        float sumX = 0, sumY = 0;
        for (auto& [id, p] : m_points) { sumX += p.x; sumY += p.y; }
        float cx3 = sumX / 3.0f;
        float cy3 = sumY / 3.0f;

        if (m_prevThreeFingerCY > 0.0f) {
            float moveDy = cy3 - m_prevThreeFingerCY;
            if (moveDy < -20.0f) {
                // Swipe up → recent apps
                m_bridge->sendKeyEvent(187); // KEYCODE_APP_SWITCH
                m_prevThreeFingerCY = 0; // suppress repeat
            } else if (moveDy > 20.0f) {
                // Swipe down → home
                m_bridge->sendKeyEvent(3); // KEYCODE_HOME
                m_prevThreeFingerCY = 0;
            }
        } else {
            m_prevThreeFingerCY = cy3;
        }

    } else {
        // Reset two-finger state when finger count changes
        m_prevTwoFingerDist = 0.0f;
        m_prevThreeFingerCY = 0.0f;
    }
}

// ─── ScreenshotCapture ────────────────────────────────────────────────────────

ScreenshotCapture::Result ScreenshotCapture::capture(const uint8_t* bgra,
                                                       uint32_t w, uint32_t h) {
    Result res;
    res.filePath = generateFilename();
    res.success  = savePNG(res.filePath, bgra, w, h);
    if (!res.success)
        res.errorMessage = "Failed to save screenshot to " + res.filePath;
    else
        LOG_INFO("Screenshot saved: {}", res.filePath);
    return res;
}

std::string ScreenshotCapture::generateFilename() const {
    auto now  = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    struct tm tm_buf;
    localtime_s(&tm_buf, &time);

    std::ostringstream ss;
    ss << m_folder << "\\AuraCastPro_Screenshot_"
       << std::put_time(&tm_buf, "%Y-%m-%d_%H-%M-%S") << ".png";
    return ss.str();
}

bool ScreenshotCapture::savePNG(const std::string& path,
                                  const uint8_t* bgra,
                                  uint32_t w, uint32_t h) {
    // Init GDI+
    Gdiplus::GdiplusStartupInput gsi;
    ULONG_PTR token;
    Gdiplus::GdiplusStartup(&token, &gsi, nullptr);

    bool ok = false;
    {
        Gdiplus::Bitmap bmp(w, h, PixelFormat32bppARGB);
        Gdiplus::BitmapData bmpData;
        Gdiplus::Rect rect(0, 0, w, h);
        bmp.LockBits(&rect, Gdiplus::ImageLockModeWrite, PixelFormat32bppARGB, &bmpData);

        // BGRA → ARGB row by row
        for (uint32_t y = 0; y < h; y++) {
            const uint8_t* src = bgra + y * w * 4;
            uint8_t* dst = (uint8_t*)bmpData.Scan0 + y * bmpData.Stride;
            std::memcpy(dst, src, w * 4);
        }
        bmp.UnlockBits(&bmpData);

        // Find PNG encoder
        UINT numEncoders = 0, size = 0;
        Gdiplus::GetImageEncodersSize(&numEncoders, &size);
        std::vector<uint8_t> buf(size);
        auto* encoders = reinterpret_cast<Gdiplus::ImageCodecInfo*>(buf.data());
        Gdiplus::GetImageEncoders(numEncoders, size, encoders);

        CLSID pngClsid = {};
        for (UINT i = 0; i < numEncoders; i++) {
            std::wstring mime(encoders[i].MimeType);
            if (mime == L"image/png") { pngClsid = encoders[i].Clsid; break; }
        }

        std::wstring wpath(path.begin(), path.end());
        ok = (bmp.Save(wpath.c_str(), &pngClsid) == Gdiplus::Ok);
    }
    Gdiplus::GdiplusShutdown(token);
    return ok;
}

// ─── ClipboardBridge ─────────────────────────────────────────────────────────

void ClipboardBridge::start(HWND hwnd) {
    m_hwnd = hwnd;
    if (hwnd) AddClipboardFormatListener(hwnd);
    m_running = true;
    m_thread = std::thread([this]{
        while (m_running.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(2));
            if (!m_running.load()) break;
            pollAndroidClipboard();
        }
    });
}

void ClipboardBridge::stop() {
    m_running = false;
    if (m_hwnd) RemoveClipboardFormatListener(m_hwnd);
    if (m_thread.joinable()) m_thread.join();
}

void ClipboardBridge::onWindowsClipboardChanged() {
    std::string text = getWindowsClipboard();
    if (text.empty() || text == m_lastWindows) return;
    m_lastWindows = text;
    sendToAndroid(text);
    LOG_DEBUG("ClipboardBridge: Sent {} chars to Android", text.size());
}

void ClipboardBridge::pollAndroidClipboard() {
    if (m_serial.empty()) return;
    // This requires the Clipper app on Android or root access
    std::string result = runAdb("shell am broadcast -a clipper.get 2>nul");
    if (result.empty() || result == m_lastAndroid) return;
    m_lastAndroid = result;
    LOG_DEBUG("ClipboardBridge: Android clipboard changed ({} chars)", result.size());
    if (m_onChanged) m_onChanged(result);
}

bool ClipboardBridge::sendToAndroid(const std::string& text) {
    if (m_serial.empty() || m_adb.empty()) return false;
    // Escape single quotes
    std::string escaped = text;
    size_t pos = 0;
    while ((pos = escaped.find('\'', pos)) != std::string::npos) {
        escaped.replace(pos, 1, "\\'");
        pos += 2;
    }
    std::string cmd = "-s " + m_serial +
                      " shell am broadcast -a clipper.set -e text '" + escaped + "'";
    runAdb(cmd);
    return true;
}

std::string ClipboardBridge::getWindowsClipboard() {
    if (!OpenClipboard(nullptr)) return {};
    HANDLE hData = GetClipboardData(CF_UNICODETEXT);
    std::string result;
    if (hData) {
        wchar_t* wText = static_cast<wchar_t*>(GlobalLock(hData));
        if (wText) {
            std::wstring ws(wText);
            result.assign(ws.begin(), ws.end());
            GlobalUnlock(hData);
        }
    }
    CloseClipboard();
    return result;
}

std::string ClipboardBridge::runAdb(const std::string& args) {
    if (m_adb.empty()) return {};
    std::string cmd = "\"" + m_adb + "\" " + args + " 2>nul";
    char buf[1024] = {};
    std::string out;
    FILE* f = _popen(cmd.c_str(), "r");
    if (!f) return {};
    while (fgets(buf, sizeof(buf), f)) out += buf;
    _pclose(f);
    while (!out.empty() && (out.back() == '\n' || out.back() == '\r'))
        out.pop_back();
    return out;
}
