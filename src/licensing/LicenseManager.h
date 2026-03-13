#pragma once
// =============================================================================
// LicenseManager.h -- License tier management + QObject for QML binding.
// FIXED: Added QObject + Q_PROPERTYs (tierName, isValid, canUse4K etc.)
//        so QML can bind to licenseManager.tierName etc. directly.
// =============================================================================
#include <QObject>
#include <QString>
#include <string>
#include <functional>
#include <memory>

namespace aura {

enum class LicenseTier { Free, Pro, Business };

struct LicenseInfo {
    LicenseTier tier{LicenseTier::Free};
    std::string email;
    std::string key;
    std::string expiryDate;
    bool        isValid{false};
    bool        isExpired{false};
};

class LicenseManager : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString tierName       READ tierName       NOTIFY licenseChanged)
    Q_PROPERTY(bool    isValid        READ isValid        NOTIFY licenseChanged)
    Q_PROPERTY(bool    isExpired      READ isExpired      NOTIFY licenseChanged)
    Q_PROPERTY(QString email          READ email          NOTIFY licenseChanged)
    Q_PROPERTY(QString expiryDate     READ expiryDate     NOTIFY licenseChanged)
    Q_PROPERTY(bool    canUse4K       READ canUse4K       NOTIFY licenseChanged)
    Q_PROPERTY(bool    canRecord      READ canRecord      NOTIFY licenseChanged)
    Q_PROPERTY(bool    canStreamRTMP  READ canStreamRTMP  NOTIFY licenseChanged)
    Q_PROPERTY(bool    showsWatermark READ showsWatermark NOTIFY licenseChanged)

public:
    explicit LicenseManager(QObject* parent = nullptr);
    ~LicenseManager();

    void init();
    void shutdown();

    bool activate(const std::string& key, const std::string& email);
    Q_INVOKABLE void activateFromQml(const QString& key, const QString& email);
    Q_INVOKABLE void deactivate();

    LicenseTier currentTier() const;
    const LicenseInfo& licenseInfo() const { return m_license; }

    // Task 156: Convenience aliases used by cloud/licensing modules that use
    // the task-spec method names directly.
    bool activateLicense(const std::string& key) {
        return activate(key, std::string());
    }
    LicenseTier getCurrentTier() const { return currentTier(); }

    // Q_PROPERTY getters
    QString tierName()      const;
    bool    isValid()       const { return m_license.isValid; }
    bool    isExpired()     const { return m_license.isExpired; }
    QString email()         const { return QString::fromStdString(m_license.email); }
    QString expiryDate()    const { return QString::fromStdString(m_license.expiryDate); }
    bool    canUse4K()      const { return m_license.tier >= LicenseTier::Pro; }
    bool    canRecord()     const { return m_license.tier >= LicenseTier::Pro; }
    bool    canUseVirtualCamera() const { return m_license.tier >= LicenseTier::Pro; }
    bool    canStreamRTMP() const { return m_license.tier >= LicenseTier::Business; }
    bool    canConnectMultiple() const { return m_license.tier >= LicenseTier::Business; }
    bool    showsWatermark() const { return m_license.tier == LicenseTier::Free; }

signals:
    void licenseChanged();
    void activationSucceeded(const QString& tierName);
    void activationFailed(const QString& reason);

private:
    LicenseInfo m_license;
    struct Impl;
    std::unique_ptr<Impl> m_impl;

    bool validateSignature(const std::string& key) const;
    void loadSavedLicense();
    void saveLicense();
};

} // namespace aura
