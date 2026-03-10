#pragma once
#include <QDialog>
#include <QStackedWidget>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QLineEdit>
#include <QSplashScreen>
#include <QProgressBar>
#include <functional>
#include <string>

class SettingsModel;

// ─── First Run Wizard ─────────────────────────────────────────────────────────

class FirstRunWizard : public QDialog {
    Q_OBJECT
public:
    explicit FirstRunWizard(SettingsModel* settings, QWidget* parent = nullptr);

    // Returns true if wizard was shown and completed (or skipped)
    static bool runIfNeeded(SettingsModel* settings, QWidget* parent = nullptr);

private slots:
    void onNext();
    void onBack();
    void onSkip();
    void onBrowseFolder();

private:
    void setupPages();
    void buildWelcomePage();
    void buildNetworkPage();
    void buildRecordingPage();
    void buildLicensePage();
    void goToPage(int idx);
    void finish();

    SettingsModel*   m_settings;
    QStackedWidget*  m_stack;
    QPushButton*     m_nextBtn;
    QPushButton*     m_backBtn;
    QPushButton*     m_skipBtn;
    QComboBox*       m_networkCombo;
    QLineEdit*       m_folderEdit;
    QLineEdit*       m_licenseEdit;
    QLabel*          m_diskSpaceLabel;
    int              m_currentPage = 0;
    static constexpr int kPageCount = 4;
};

// ─── Splash Screen ────────────────────────────────────────────────────────────

class SplashScreen : public QSplashScreen {
    Q_OBJECT
public:
    explicit SplashScreen(const QPixmap& pixmap, QWidget* parent = nullptr);

    void setProgress(int percent, const QString& message);
    void finishWithFade(QWidget* mainWindow);

    // Step list for SubsystemInitialiser
    struct Step { int percent; const char* message; };
    static const Step kSteps[];
    static constexpr int kStepCount = 8;

signals:
    void progressChanged(int percent, QString message);

private:
    QLabel*       m_messageLabel  = nullptr;
    QProgressBar* m_progressBar   = nullptr;
    int           m_currentPct    = 0;
};
