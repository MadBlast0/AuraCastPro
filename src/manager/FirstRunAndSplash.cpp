#include "../pch.h"  // PCH
#include "SettingsModel.h"
#include "FirstRunAndSplash.h"
#include "../utils/NetworkTools.h"
#include "../utils/AppDataInit.h"
#include "../utils/Logger.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QPropertyAnimation>
#include <QTimer>
#include <QGraphicsOpacityEffect>
#include <QFont>
#include <filesystem>

// ─── First Run Wizard ─────────────────────────────────────────────────────────

bool FirstRunWizard::runIfNeeded(SettingsModel* settings, QWidget* parent) {
    // First run = settings file does not exist
    QString settingsPath = QString::fromStdWString(
        AppDataInit::getAppDataPath()) + "\\settings.json";
    if (QFile::exists(settingsPath)) return false;

    FirstRunWizard wiz(settings, parent);
    wiz.exec();
    return true;
}

FirstRunWizard::FirstRunWizard(SettingsModel* settings, QWidget* parent)
    : QDialog(parent, Qt::Dialog | Qt::WindowTitleHint | Qt::WindowCloseButtonHint)
    , m_settings(settings) {
    setWindowTitle("Welcome to AuraCastPro");
    setFixedSize(520, 420);
    setStyleSheet(
        "QDialog { background:#0A0A0A; color:#FFFFFF; }"
        "QLabel  { color:#FFFFFF; font-size:14px; }"
        "QPushButton { background:#1E1E1E; color:#FFFFFF; border:2px solid #FFFFFF;"
        "              padding:8px 20px; font-size:13px; font-weight:bold; }"
        "QPushButton:hover { background:#FFFFFF; color:#000000; }"
        "QComboBox,QLineEdit { background:#1E1E1E; color:#FFFFFF; border:2px solid #666;"
        "                      padding:6px; font-size:13px; }"
    );

    auto* mainLayout = new QVBoxLayout(this);
    m_stack = new QStackedWidget;
    mainLayout->addWidget(m_stack);

    auto* btnRow = new QHBoxLayout;
    m_skipBtn = new QPushButton("Skip");
    m_backBtn = new QPushButton("Back");
    m_nextBtn = new QPushButton("Next →");
    m_nextBtn->setStyleSheet(
        "QPushButton { background:#FFFFFF; color:#000000; border:2px solid #FFFFFF; }"
        "QPushButton:hover { background:#CCCCCC; }");
    btnRow->addWidget(m_skipBtn);
    btnRow->addStretch();
    btnRow->addWidget(m_backBtn);
    btnRow->addWidget(m_nextBtn);
    mainLayout->addLayout(btnRow);

    setupPages();

    connect(m_nextBtn, &QPushButton::clicked, this, &FirstRunWizard::onNext);
    connect(m_backBtn, &QPushButton::clicked, this, &FirstRunWizard::onBack);
    connect(m_skipBtn, &QPushButton::clicked, this, &FirstRunWizard::onSkip);

    goToPage(0);
}

void FirstRunWizard::setupPages() {
    buildWelcomePage();
    buildNetworkPage();
    buildRecordingPage();
    buildLicensePage();
}

void FirstRunWizard::buildWelcomePage() {
    auto* w = new QWidget; auto* l = new QVBoxLayout(w);
    l->setContentsMargins(30, 30, 30, 20);
    auto* title = new QLabel("Welcome to\nAuraCastPro");
    title->setStyleSheet("font-size:28px; font-weight:900; color:#FFFFFF; line-height:1.2;");
    auto* sub = new QLabel(
        "Ultra-low-latency screen mirroring for Windows.\n\n"
        "This wizard will set up AuraCastPro in 3 quick steps.\n"
        "You can change all settings later in the Settings panel.");
    sub->setWordWrap(true);
    sub->setStyleSheet("font-size:13px; color:#AAAAAA; line-height:1.5;");
    l->addWidget(title); l->addSpacing(20); l->addWidget(sub); l->addStretch();
    m_stack->addWidget(w);
}

void FirstRunWizard::buildNetworkPage() {
    auto* w = new QWidget; auto* l = new QVBoxLayout(w);
    l->setContentsMargins(30, 30, 30, 20);
    l->addWidget(new QLabel("Network Interface"));
    auto* sub = new QLabel("Choose the network adapter your phone is connected to:");
    sub->setStyleSheet("color:#AAAAAA; font-size:12px;");
    l->addWidget(sub); l->addSpacing(12);

    m_networkCombo = new QComboBox;
    auto interfaces = aura::NetworkTools::getNetworkInterfaces();
    for (auto& iface : interfaces) {
        m_networkCombo->addItem(
            QString::fromStdString(iface.name + " (" + iface.ipv4 + ")"),
            QString::fromStdString(iface.ipv4));
    }
    if (m_networkCombo->count() == 0)
        m_networkCombo->addItem("Default (auto-detect)", "");
    l->addWidget(m_networkCombo);
    l->addStretch();
    m_stack->addWidget(w);
}

void FirstRunWizard::buildRecordingPage() {
    auto* w = new QWidget; auto* l = new QVBoxLayout(w);
    l->setContentsMargins(30, 30, 30, 20);
    l->addWidget(new QLabel("Recording Folder"));

    m_folderEdit  = new QLineEdit;
    QString defPath = QDir::homePath() + "/Videos/AuraCastPro";
    m_folderEdit->setText(defPath);

    auto* browseBtn = new QPushButton("Browse…");
    auto* row = new QHBoxLayout;
    row->addWidget(m_folderEdit); row->addWidget(browseBtn);
    l->addLayout(row);

    m_diskSpaceLabel = new QLabel;
    m_diskSpaceLabel->setStyleSheet("color:#AAAAAA; font-size:12px;");
    l->addWidget(m_diskSpaceLabel);
    l->addStretch();

    connect(browseBtn, &QPushButton::clicked, this, &FirstRunWizard::onBrowseFolder);
    m_stack->addWidget(w);
}

void FirstRunWizard::buildLicensePage() {
    auto* w = new QWidget; auto* l = new QVBoxLayout(w);
    l->setContentsMargins(30, 30, 30, 20);
    l->addWidget(new QLabel("License"));
    auto* sub = new QLabel(
        "AuraCastPro is free to use with 1080p mirroring.\n\n"
        "Upgrade to Pro for 4K mirroring, streaming output,\n"
        "and no watermark.");
    sub->setStyleSheet("color:#AAAAAA; font-size:12px;");
    sub->setWordWrap(true);
    l->addWidget(sub); l->addSpacing(16);

    m_licenseEdit = new QLineEdit;
    m_licenseEdit->setPlaceholderText("License key (optional) — XXXX-XXXX-XXXX-XXXX");
    l->addWidget(m_licenseEdit);
    m_nextBtn->setText("Start Using AuraCastPro");
    l->addStretch();
    m_stack->addWidget(w);
}

void FirstRunWizard::goToPage(int idx) {
    m_currentPage = idx;
    m_stack->setCurrentIndex(idx);
    m_backBtn->setEnabled(idx > 0);
    m_nextBtn->setText(idx == kPageCount - 1 ? "Start Using AuraCastPro" : "Next →");
}

void FirstRunWizard::onNext() {
    if (m_currentPage == kPageCount - 1) { finish(); return; }
    goToPage(m_currentPage + 1);
}
void FirstRunWizard::onBack()  { if (m_currentPage > 0) goToPage(m_currentPage - 1); }
void FirstRunWizard::onSkip()  { finish(); }

void FirstRunWizard::onBrowseFolder() {
    QString dir = QFileDialog::getExistingDirectory(this, "Choose Recording Folder",
                                                    m_folderEdit->text());
    if (!dir.isEmpty()) m_folderEdit->setText(dir);
}

void FirstRunWizard::finish() {
    // Apply selected settings
    if (m_networkCombo)
        m_settings->setProperty("selectedNetworkInterface",
                                  m_networkCombo->currentData().toString());
    if (m_folderEdit)
        m_settings->setProperty("outputFolder", m_folderEdit->text());
    if (m_licenseEdit && !m_licenseEdit->text().isEmpty())
        m_settings->setProperty("pendingLicenseKey", m_licenseEdit->text());

    LOG_INFO("FirstRunWizard: Completed");
    accept();
}

// ─── SplashScreen ────────────────────────────────────────────────────────────

const SplashScreen::Step SplashScreen::kSteps[] = {
    { 10, "Initialising logger…"          },
    { 20, "Checking hardware…"            },
    { 30, "Loading settings…"             },
    { 50, "Starting network services…"    },
    { 70, "Initialising GPU renderer…"    },
    { 80, "Starting audio services…"      },
    { 90, "Starting device discovery…"    },
    {100, "Ready."                         },
};

SplashScreen::SplashScreen(const QPixmap& pixmap, QWidget* parent)
    : QSplashScreen(pixmap, Qt::WindowStaysOnTopHint) {
    Q_UNUSED(parent)
    setFixedSize(pixmap.size().isEmpty() ? QSize(640, 360) : pixmap.size());

    // Progress bar overlay at bottom
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(false);
    m_progressBar->setGeometry(0, height() - 6, width(), 6);
    m_progressBar->setStyleSheet(
        "QProgressBar { background:#1E1E1E; border:none; }"
        "QProgressBar::chunk { background:#FFFFFF; }");

    m_messageLabel = new QLabel(this);
    m_messageLabel->setGeometry(20, height() - 36, width() - 40, 24);
    m_messageLabel->setStyleSheet("color:#AAAAAA; font-size:11px; background:transparent;");
}

void SplashScreen::setProgress(int pct, const QString& msg) {
    m_currentPct = pct;
    m_progressBar->setValue(pct);
    m_messageLabel->setText(msg);
    repaint();
    QApplication::processEvents();
}

void SplashScreen::finishWithFade(QWidget* mainWindow) {
    auto* effect = new QGraphicsOpacityEffect(this);
    setGraphicsEffect(effect);
    auto* anim = new QPropertyAnimation(effect, "opacity");
    anim->setDuration(400);
    anim->setStartValue(1.0);
    anim->setEndValue(0.0);
    connect(anim, &QPropertyAnimation::finished, this, [this, mainWindow](){
        finish(mainWindow);
    });
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

#include "FirstRunAndSplash.moc"