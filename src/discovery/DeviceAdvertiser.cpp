// DeviceAdvertiser.cpp
#include "../pch.h"  // PCH
#include "DeviceAdvertiser.h"
#include "BonjourAdapter.h"
#include "../utils/Logger.h"

namespace aura {

DeviceAdvertiser::DeviceAdvertiser(BonjourAdapter* bonjour) : m_bonjour(bonjour) {}
DeviceAdvertiser::~DeviceAdvertiser() { shutdown(); }

void DeviceAdvertiser::init() {
    AURA_LOG_INFO("DeviceAdvertiser", "Initialised with display name: '{}'", m_displayName);
}

void DeviceAdvertiser::start() {
    if (m_running) return;
    m_running = true;
    AURA_LOG_INFO("DeviceAdvertiser",
        "Advertising delegated to MDNSService for '{}'.", m_displayName);
}

void DeviceAdvertiser::stop() {
    if (!m_running) return;
    m_running = false;
    AURA_LOG_INFO("DeviceAdvertiser", "Stopped advertising delegation.");
}

void DeviceAdvertiser::shutdown() { stop(); }

void DeviceAdvertiser::setDisplayName(const std::string& name) {
    m_displayName = name;
    if (m_running) {
        AURA_LOG_INFO("DeviceAdvertiser",
            "Display name updated to '{}' (MDNSService will advertise it).",
            m_displayName);
    }
}

} // namespace aura
