// DeviceAdvertiser.cpp
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
    buildAndRegisterAirPlay();
    buildAndRegisterCast();
    AURA_LOG_INFO("DeviceAdvertiser",
        "'{}' now visible to iOS (Screen Mirroring) and Android (Cast).", m_displayName);
}

void DeviceAdvertiser::stop() {
    if (!m_running) return;
    m_running = false;
    if (m_bonjour) m_bonjour->unregisterAll();
    AURA_LOG_INFO("DeviceAdvertiser", "Stopped broadcasting.");
}

void DeviceAdvertiser::shutdown() { stop(); }

void DeviceAdvertiser::setDisplayName(const std::string& name) {
    m_displayName = name;
    if (m_running) { stop(); start(); } // re-register with new name
}

void DeviceAdvertiser::buildAndRegisterAirPlay() {
    if (!m_bonjour) return;
    const std::string txt =
        "deviceid=AA:BB:CC:DD:EE:FF\n"
        "features=0x5A7FFFF7,0x1E\n"
        "srcvers=220.68\n"
        "flags=0x4\n"
        "model=AppleTV3,2\n"
        "pi=00000000-0000-0000-0000-000000000000\n"
        "pk=" + std::string(64, '0') + "\n";
    m_bonjour->registerService("_airplay._tcp", m_displayName, 7236, txt);
}

void DeviceAdvertiser::buildAndRegisterCast() {
    if (!m_bonjour) return;
    const std::string txt =
        "id=auracastpro0001\n"
        "md=AuraCastPro\n"
        "ve=05\n"
        "fn=" + m_displayName + "\n"
        "ca=4101\n"
        "st=0\n"
        "nf=1\n";
    m_bonjour->registerService("_googlecast._tcp", m_displayName, 8009, txt);
}

} // namespace aura
