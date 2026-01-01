#include "EncoderConfigDialog.h"
#include "EncoderSlot.h"
#include <QTabWidget>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QGroupBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QTimer>
#include <QDateTime>


// ─────────────────────────────────────────────────────────────────────────────
EncoderConfigDialog::EncoderConfigDialog(EncoderSlot* slot, QWidget* parent)
    : QDialog(parent)
    , m_slot(slot)
{
    setObjectName("EncoderConfigDialog");
    setWindowTitle("Encoder Configuration — " + slot->config().name);
    setMinimumSize(560, 480);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);

    m_tabs = new QTabWidget(this);
    buildBasicTab  (m_tabs);
    buildDspTab    (m_tabs);
    buildIcy2Tab   (m_tabs);
    buildArchiveTab(m_tabs);
    buildStatsTab  (m_tabs);
    root->addWidget(m_tabs);

    auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    root->addWidget(btns);

    connect(btns, &QDialogButtonBox::accepted, this, &EncoderConfigDialog::onAccepted);
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);

    loadFromConfig(slot->config());

    // Live stats refresh
    m_statsTimer = new QTimer(this);
    m_statsTimer->setInterval(5000);
    connect(m_statsTimer, &QTimer::timeout, this, &EncoderConfigDialog::refreshStats);
    m_statsTimer->start();
    refreshStats();
}

// ─────────────────────────────────────────────────────────────────────────────
// Tab 1 — Basic
// ─────────────────────────────────────────────────────────────────────────────
void EncoderConfigDialog::buildBasicTab(QTabWidget* tabs)
{
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    auto* w    = new QWidget;
    auto* form = new QFormLayout(w);
    form->setSpacing(5);
    form->setContentsMargins(8, 8, 8, 8);

    m_name = new QLineEdit; form->addRow("Name:", m_name);

    m_mode = new QComboBox;
    m_mode->addItems({"Radio", "Podcast", "TV"});
    form->addRow("Mode:", m_mode);

    // Input source
    m_source = new QComboBox;
    m_source->addItems({"LiveDeck Mix", "PortAudio Device", "WASAPI Loopback"});
    form->addRow("Input Source:", m_source);
    m_paDevLbl = new QLabel("Device:");
    m_paDevice = new QComboBox;
    populatePortAudioDevices();
    form->addRow(m_paDevLbl, m_paDevice);
    connect(m_source, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &EncoderConfigDialog::onSourceChanged);
    onSourceChanged(0);

    form->addRow(new QLabel("── Codec ──", w));

    m_codec = new QComboBox;
    m_codec->addItems({"MP3","Opus","OGG Vorbis","FLAC","AAC-LC","HE-AAC v1","HE-AAC v2"});
    form->addRow("Codec:", m_codec);

    m_bitrateLbl = new QLabel("Bitrate (kbps):");
    m_bitrate    = new QSpinBox; m_bitrate->setRange(8, 320); m_bitrate->setValue(128);
    form->addRow(m_bitrateLbl, m_bitrate);

    m_vbrLbl  = new QLabel("VBR Quality (0–1):");
    m_vbrQual = new QDoubleSpinBox; m_vbrQual->setRange(0.0, 1.0); m_vbrQual->setSingleStep(0.05);
    form->addRow(m_vbrLbl, m_vbrQual);

    m_sampleRate = new QSpinBox; m_sampleRate->setRange(8000, 192000); m_sampleRate->setValue(44100);
    form->addRow("Sample Rate (Hz):", m_sampleRate);

    m_channels = new QSpinBox; m_channels->setRange(1, 2); m_channels->setValue(2);
    form->addRow("Channels:", m_channels);

    m_chanMode = new QComboBox;
    m_chanMode->addItems({"Stereo","Joint Stereo","Mono"});
    m_chanMode->setCurrentIndex(1);
    form->addRow("Channel Mode:", m_chanMode);

    connect(m_codec, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &EncoderConfigDialog::onCodecChanged);
    onCodecChanged(0);

    form->addRow(new QLabel("── Server ──", w));

    // DNAS first!
    m_serverType = new QComboBox;
    m_serverType->addItems({"Mcaster1DNAS","Icecast2","Shoutcast v1","Shoutcast v2"});
    form->addRow("Server Type:", m_serverType);
    connect(m_serverType, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &EncoderConfigDialog::onServerTypeChanged);

    m_host      = new QLineEdit;       form->addRow("Host:", m_host);
    m_port      = new QSpinBox;        m_port->setRange(1, 65535); m_port->setValue(9000);
                                       form->addRow("Port:", m_port);
    m_mount     = new QLineEdit;       form->addRow("Mount:", m_mount);
    m_password  = new QLineEdit;       m_password->setEchoMode(QLineEdit::Password);
                                       form->addRow("Password:", m_password);
    m_adminUser = new QLineEdit;       form->addRow("Admin User:", m_adminUser);
    m_adminPass = new QLineEdit;       m_adminPass->setEchoMode(QLineEdit::Password);
                                       form->addRow("Admin Pass:", m_adminPass);
    m_useSsl    = new QCheckBox("Use SSL/TLS (HTTPS streaming)");
    m_useSsl->setToolTip("Enable for servers that require encrypted source connections (e.g., port 9443)");
                                       form->addRow("", m_useSsl);

    form->addRow(new QLabel("── Station Info ──", w));
    m_stationName = new QLineEdit; form->addRow("Station Name:", m_stationName);
    m_description = new QLineEdit; form->addRow("Description:", m_description);
    m_genre       = new QLineEdit; form->addRow("Genre:", m_genre);
    m_url         = new QLineEdit; form->addRow("URL:", m_url);
    m_isPublic    = new QCheckBox("List in public directory");
                                   form->addRow("", m_isPublic);

    form->addRow(new QLabel("── Reconnect ──", w));
    m_autoReconnect  = new QCheckBox("Auto-reconnect on drop");
                                     form->addRow("", m_autoReconnect);
    m_retryInterval  = new QSpinBox; m_retryInterval->setRange(5, 300); m_retryInterval->setValue(15);
    m_retryInterval->setSuffix(" s");form->addRow("Retry Interval:", m_retryInterval);
    m_maxRetries     = new QSpinBox; m_maxRetries->setRange(-1, 1000); m_maxRetries->setValue(-1);
    m_maxRetries->setSpecialValueText("∞ (infinite)");
                                     form->addRow("Max Retries:", m_maxRetries);
    m_autoStart = new QCheckBox("Auto-connect on startup");
                                     form->addRow("", m_autoStart);

    scroll->setWidget(w);
    tabs->addTab(scroll, "Basic");
}

// ─────────────────────────────────────────────────────────────────────────────
// Tab 2 — DSP
// ─────────────────────────────────────────────────────────────────────────────
void EncoderConfigDialog::buildDspTab(QTabWidget* tabs)
{
    auto* w    = new QWidget;
    auto* vbox = new QVBoxLayout(w);
    vbox->setContentsMargins(8, 8, 8, 8);
    vbox->setSpacing(6);

    m_dspEnabled = new QCheckBox("Enable per-slot DSP processing");
    vbox->addWidget(m_dspEnabled);
    connect(m_dspEnabled, &QCheckBox::toggled, this, &EncoderConfigDialog::onDspToggled);

    // EQ
    auto* eqGrp  = new QGroupBox("Equaliser");
    auto* eqForm = new QFormLayout(eqGrp);
    m_eqPreset   = new QComboBox;
    m_eqPreset->addItems({"flat","broadcast","spoken_word","classic_rock","country","modern_rock"});
    eqForm->addRow("EQ Preset:", m_eqPreset);
    vbox->addWidget(eqGrp);

    // AGC
    m_agcGroup   = new QGroupBox("AGC / Compressor");
    auto* agcForm = new QFormLayout(m_agcGroup);
    m_agcEnabled  = new QCheckBox("Enable AGC");      agcForm->addRow("", m_agcEnabled);
    m_agcInputGain = new QDoubleSpinBox; m_agcInputGain->setRange(-24, 24); m_agcInputGain->setSuffix(" dB");
    agcForm->addRow("Input Gain:", m_agcInputGain);
    m_agcThreshold = new QDoubleSpinBox; m_agcThreshold->setRange(-60, 0); m_agcThreshold->setSuffix(" dBFS");
    m_agcThreshold->setValue(-18);  agcForm->addRow("Threshold:", m_agcThreshold);
    m_agcRatio     = new QDoubleSpinBox; m_agcRatio->setRange(1, 20); m_agcRatio->setSingleStep(0.5);
    m_agcRatio->setSuffix(":1"); m_agcRatio->setValue(4);
    agcForm->addRow("Ratio:", m_agcRatio);
    m_agcAttack    = new QDoubleSpinBox; m_agcAttack->setRange(0.5, 200); m_agcAttack->setSuffix(" ms");
    m_agcAttack->setValue(10);     agcForm->addRow("Attack:", m_agcAttack);
    m_agcRelease   = new QDoubleSpinBox; m_agcRelease->setRange(10, 2000); m_agcRelease->setSuffix(" ms");
    m_agcRelease->setValue(200);   agcForm->addRow("Release:", m_agcRelease);
    m_agcMakeup    = new QDoubleSpinBox; m_agcMakeup->setRange(-12, 24); m_agcMakeup->setSuffix(" dB");
    agcForm->addRow("Makeup Gain:", m_agcMakeup);
    m_agcLimiter   = new QDoubleSpinBox; m_agcLimiter->setRange(-12, 0); m_agcLimiter->setSuffix(" dBFS");
    m_agcLimiter->setValue(-1);    agcForm->addRow("Limiter Ceiling:", m_agcLimiter);
    connect(m_agcEnabled, &QCheckBox::toggled, this, &EncoderConfigDialog::onAgcToggled);
    vbox->addWidget(m_agcGroup);

    // PTT duck
    auto* pttGrp  = new QGroupBox("PTT Voice-over Duck");
    auto* pttForm = new QFormLayout(pttGrp);
    m_pttDuck     = new QCheckBox("Attenuate stream when PTT mic active");
    pttForm->addRow("", m_pttDuck);
    m_pttAttenDb  = new QDoubleSpinBox; m_pttAttenDb->setRange(-40, 0); m_pttAttenDb->setSuffix(" dB");
    m_pttAttenDb->setValue(-12);   pttForm->addRow("Attenuation:", m_pttAttenDb);
    vbox->addWidget(pttGrp);

    vbox->addStretch();
    tabs->addTab(w, "DSP");
}

// ─────────────────────────────────────────────────────────────────────────────
// Tab 3 — ICY 2.2
// ─────────────────────────────────────────────────────────────────────────────
void EncoderConfigDialog::buildIcy2Tab(QTabWidget* tabs)
{
    auto* outer = new QWidget;
    auto* ovbox = new QVBoxLayout(outer);
    ovbox->setContentsMargins(6, 6, 6, 6);

    m_icy2Warning = new QLabel("⚠  ICY 2.2 fields are only sent when Server Type = Mcaster1DNAS.");
    m_icy2Warning->setStyleSheet("color:#e8a828; font-size:9px; padding:4px;");
    m_icy2Warning->setWordWrap(true);
    ovbox->addWidget(m_icy2Warning);

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    auto* w    = new QWidget;
    auto* vbox = new QVBoxLayout(w);
    vbox->setSpacing(4);

    auto addGroup = [&](const QString& title,
                        const QList<QPair<QString, QString>>& fields) {
        auto* grp  = new QGroupBox(title);
        auto* form = new QFormLayout(grp);
        form->setSpacing(3);
        for (const auto& [key, label] : fields) {
            auto* edit = new QLineEdit;
            edit->setPlaceholderText(key);
            form->addRow(label + ":", edit);
            m_icy2Edits[key] = edit;
        }
        vbox->addWidget(grp);
    };

    addGroup("Station & Show", {
        {"icy2-station-id",    "Station ID"},
        {"icy2-station-logo",  "Station Logo URL"},
        {"icy2-show-title",    "Show Title"},
        {"icy2-show-start",    "Show Start"},
        {"icy2-show-end",      "Show End"},
        {"icy2-next-show",     "Next Show"},
        {"icy2-schedule-url",  "Schedule URL"},
        {"icy2-auto-dj",       "Auto DJ Name"},
        {"icy2-playlist-name", "Playlist Name"},
    });

    addGroup("DJ & Host", {
        {"icy2-dj-handle", "DJ Handle"},
        {"icy2-dj-bio",    "DJ Bio"},
        {"icy2-dj-genre",  "DJ Genre"},
        {"icy2-dj-rating", "DJ Rating"},
    });

    addGroup("Social & Discovery", {
        {"icy2-creator-handle",    "Creator Handle"},
        {"icy2-social-twitter",    "Twitter / X"},
        {"icy2-social-twitch",     "Twitch"},
        {"icy2-social-instagram",  "Instagram"},
        {"icy2-social-tiktok",     "TikTok"},
        {"icy2-social-youtube",    "YouTube"},
        {"icy2-social-facebook",   "Facebook"},
        {"icy2-social-linkedin",   "LinkedIn"},
        {"icy2-social-linktree",   "Linktree"},
        {"icy2-emoji",             "Emoji"},
        {"icy2-hashtags",          "Hashtags"},
    });

    addGroup("Engagement", {
        {"icy2-request-url",         "Request URL"},
        {"icy2-chat-url",            "Chat URL"},
        {"icy2-tip-url",             "Tip URL"},
        {"icy2-events-url",          "Events URL"},
        {"icy2-crosspost-platforms", "Crosspost Platforms"},
        {"icy2-cdn-region",          "CDN Region"},
        {"icy2-relay-origin",        "Relay Origin"},
    });

    addGroup("Compliance & Legal", {
        {"icy2-license-type",      "License Type"},
        {"icy2-license-territory", "License Territory"},
        {"icy2-geo-region",        "Geo Region"},
        {"icy2-notice-text",       "Notice Text"},
        {"icy2-notice-url",        "Notice URL"},
        {"icy2-notice-expires",    "Notice Expires"},
        {"icy2-loudness-lufs",     "Loudness (LUFS)"},
    });

    addGroup("Video & Simulcast", {
        {"icy2-video-type",       "Video Type"},
        {"icy2-video-link",       "Video Link"},
        {"icy2-video-platform",   "Video Platform"},
        {"icy2-video-codec",      "Video Codec"},
        {"icy2-video-fps",        "Video FPS"},
        {"icy2-video-resolution", "Resolution"},
    });

    vbox->addStretch();
    scroll->setWidget(w);
    ovbox->addWidget(scroll);
    tabs->addTab(outer, "ICY 2.2");
}

// ─────────────────────────────────────────────────────────────────────────────
// Tab 4 — Archive
// ─────────────────────────────────────────────────────────────────────────────
void EncoderConfigDialog::buildArchiveTab(QTabWidget* tabs)
{
    auto* w    = new QWidget;
    auto* form = new QFormLayout(w);
    form->setContentsMargins(8, 8, 8, 8);
    form->setSpacing(6);

    m_archiveEnabled = new QCheckBox("Enable archive recording");
    form->addRow("", m_archiveEnabled);
    connect(m_archiveEnabled, &QCheckBox::toggled, this, &EncoderConfigDialog::onArchiveToggled);

    auto* dirRow = new QHBoxLayout;
    m_archiveDir = new QLineEdit;
    dirRow->addWidget(m_archiveDir);
    auto* browseBtn = new QPushButton("Browse…");
    browseBtn->setFixedWidth(60);
    browseBtn->setStyleSheet("QPushButton { background:#1e3a5f; color:#9ec8f4;"
                             "border:1px solid #2a5a8f; padding:2px 6px; font-size:9px; }"
                             "QPushButton:hover { background:#2a5080; }");
    dirRow->addWidget(browseBtn);
    form->addRow("Directory:", dirRow);
    connect(browseBtn, &QPushButton::clicked, this, &EncoderConfigDialog::onBrowseArchive);

    m_archiveWav = new QCheckBox("Record WAV (always enabled when archiving)");
    m_archiveWav->setEnabled(false);
    m_archiveWav->setChecked(true);
    form->addRow("", m_archiveWav);

    m_archiveMp3 = new QCheckBox("Record MP3 alongside WAV");
#ifndef HAVE_LAME
    m_archiveMp3->setEnabled(false);
    m_archiveMp3->setToolTip("Requires LAME MP3 encoder (not compiled in)");
#endif
    form->addRow("", m_archiveMp3);

    auto* hint = new QLabel("Files saved as:  {station_name}_{YYYY-MM-DD}_{HH-MM-SS}.wav/.mp3");
    hint->setStyleSheet("color:#6a9abf; font-size:8px;");
    form->addRow("", hint);

    tabs->addTab(w, "Archive");
}

// ─────────────────────────────────────────────────────────────────────────────
// Tab 5 — Stats
// ─────────────────────────────────────────────────────────────────────────────
void EncoderConfigDialog::buildStatsTab(QTabWidget* tabs)
{
    auto* w    = new QWidget;
    auto* form = new QFormLayout(w);
    form->setContentsMargins(8, 8, 8, 8);
    form->setSpacing(5);

    m_statState     = new QLabel("–"); form->addRow("Status:",    m_statState);
    m_statUptime    = new QLabel("–"); form->addRow("Uptime:",    m_statUptime);
    m_statBytes     = new QLabel("–"); form->addRow("Bytes Sent:",m_statBytes);
    m_statListeners = new QLabel("–"); form->addRow("Listeners:", m_statListeners);

    m_statLog = new QListWidget;
    m_statLog->setMaximumHeight(180);
    form->addRow("Log:", m_statLog);

    // Connect slot status messages to log
    connect(m_slot, &EncoderSlot::statusMessage, this, [this](const QString& msg) {
        m_statLog->insertItem(0, QDateTime::currentDateTime().toString("hh:mm:ss") + "  " + msg);
        while (m_statLog->count() > 50)
            delete m_statLog->takeItem(m_statLog->count() - 1);
    });

    tabs->addTab(w, "Stats");
}

// ─────────────────────────────────────────────────────────────────────────────
// Slot handlers
// ─────────────────────────────────────────────────────────────────────────────
void EncoderConfigDialog::onCodecChanged(int idx)
{
    using C = EncoderConfig::Codec;
    const C codec = static_cast<C>(idx);
    const bool vbrMode = (codec == C::Vorbis);
    m_bitrateLbl->setVisible(!vbrMode);
    m_bitrate   ->setVisible(!vbrMode);
    m_vbrLbl    ->setVisible(vbrMode);
    m_vbrQual   ->setVisible(vbrMode);
}

void EncoderConfigDialog::onServerTypeChanged(int idx)
{
    // DNAS default port 9000; others 8000
    m_port->setValue(idx == 0 ? 9000 : 8000);
    updateIcy2TabState();
}

void EncoderConfigDialog::onSourceChanged(int idx)
{
    const bool pa = (idx == 1);
    m_paDevLbl->setVisible(pa);
    m_paDevice ->setVisible(pa);
}

void EncoderConfigDialog::onDspToggled(bool /*enabled*/)
{
    // Nothing extra — controls remain visible but DSP is bypassed if unchecked
}

void EncoderConfigDialog::onAgcToggled(bool enabled)
{
    Q_UNUSED(enabled); // Group box controls remain accessible; AGC just won't run
}

void EncoderConfigDialog::onArchiveToggled(bool enabled)
{
    m_archiveDir->setEnabled(enabled);
    m_archiveMp3->setEnabled(enabled);
}

void EncoderConfigDialog::onBrowseArchive()
{
    const QString dir = QFileDialog::getExistingDirectory(this, "Select Archive Directory",
                                                          m_archiveDir->text());
    if (!dir.isEmpty()) m_archiveDir->setText(dir);
}

void EncoderConfigDialog::updateIcy2TabState()
{
    const bool isDnas = (m_serverType->currentIndex() == 0);  // DNAS = index 0
    m_icy2Warning->setVisible(!isDnas);
    for (auto* edit : m_icy2Edits)   edit->setEnabled(isDnas);
    for (auto* cb   : m_icy2Checks)  cb->setEnabled(isDnas);
}

void EncoderConfigDialog::populatePortAudioDevices()
{
    m_paDevice->clear();
    m_paDevice->addItem("Default Input Device", -1);
    // PortAudio enumeration requires Pa_Initialize(); skip if not available.
    // In a full build, iterate Pa_GetDeviceCount() / Pa_GetDeviceInfo().
}

// ─────────────────────────────────────────────────────────────────────────────
// Load / save config
// ─────────────────────────────────────────────────────────────────────────────
void EncoderConfigDialog::loadFromConfig(const EncoderConfig& cfg)
{
    m_name->setText(cfg.name);
    m_mode->setCurrentIndex(static_cast<int>(cfg.mode));
    m_source->setCurrentIndex(static_cast<int>(cfg.source));
    m_codec->setCurrentIndex(static_cast<int>(cfg.codec));
    m_bitrate->setValue(cfg.bitrate);
    m_vbrQual->setValue(static_cast<double>(cfg.vbrQuality));
    m_sampleRate->setValue(cfg.sampleRate);
    m_channels->setValue(cfg.channels);
    m_chanMode->setCurrentIndex(static_cast<int>(cfg.channelMode));
    m_serverType->setCurrentIndex(static_cast<int>(cfg.serverType));
    m_host->setText(cfg.host);
    m_port->setValue(cfg.port);
    m_mount->setText(cfg.mount);
    m_password->setText(cfg.password);
    m_adminUser->setText(cfg.adminUser);
    m_adminPass->setText(cfg.adminPass);
    m_stationName->setText(cfg.stationName);
    m_description->setText(cfg.description);
    m_genre->setText(cfg.genre);
    m_url->setText(cfg.url);
    m_isPublic->setChecked(cfg.isPublic);
    m_useSsl->setChecked(cfg.useSsl);
    m_autoReconnect->setChecked(cfg.autoReconnect);
    m_retryInterval->setValue(cfg.retryIntervalSec);
    m_maxRetries->setValue(cfg.maxRetries);
    m_autoStart->setChecked(cfg.autoStart);

    m_dspEnabled->setChecked(cfg.dspEnabled);
    m_eqPreset->setCurrentText(cfg.eqPreset);
    m_agcEnabled->setChecked(cfg.agcEnabled);
    m_agcInputGain->setValue(static_cast<double>(cfg.agcInputGainDb));
    m_agcThreshold->setValue(static_cast<double>(cfg.agcThresholdDb));
    m_agcRatio->setValue(static_cast<double>(cfg.agcRatio));
    m_agcAttack->setValue(static_cast<double>(cfg.agcAttackMs));
    m_agcRelease->setValue(static_cast<double>(cfg.agcReleaseMs));
    m_agcMakeup->setValue(static_cast<double>(cfg.agcMakeupGainDb));
    m_agcLimiter->setValue(static_cast<double>(cfg.agcLimiterDb));
    m_pttDuck->setChecked(cfg.pttDuckEnabled);
    m_pttAttenDb->setValue(static_cast<double>(cfg.pttDuckAttenDb));

    m_archiveEnabled->setChecked(cfg.archiveEnabled);
    m_archiveDir->setText(cfg.archiveDir);
    m_archiveWav->setChecked(cfg.archiveWav);
    m_archiveMp3->setChecked(cfg.archiveMp3);

    // ICY 2.2 fields
    for (auto it = cfg.icy2Fields.constBegin(); it != cfg.icy2Fields.constEnd(); ++it) {
        if (m_icy2Edits.contains(it.key()))
            m_icy2Edits[it.key()]->setText(it.value());
    }

    onCodecChanged(m_codec->currentIndex());
    onSourceChanged(m_source->currentIndex());
    updateIcy2TabState();
}

EncoderConfig EncoderConfigDialog::readConfig() const
{
    EncoderConfig cfg = m_slot->config();  // start with existing (preserves slotId etc.)

    cfg.name          = m_name->text();
    cfg.mode          = static_cast<EncoderConfig::Mode>(m_mode->currentIndex());
    cfg.source        = static_cast<EncoderConfig::Source>(m_source->currentIndex());
    cfg.paDeviceIndex = m_paDevice->currentData().toInt();
    cfg.codec         = static_cast<EncoderConfig::Codec>(m_codec->currentIndex());
    cfg.bitrate       = m_bitrate->value();
    cfg.vbrQuality    = static_cast<float>(m_vbrQual->value());
    cfg.sampleRate    = m_sampleRate->value();
    cfg.channels      = m_channels->value();
    cfg.channelMode   = static_cast<EncoderConfig::ChannelMode>(m_chanMode->currentIndex());
    cfg.serverType    = static_cast<EncoderConfig::ServerType>(m_serverType->currentIndex());
    cfg.host          = m_host->text();
    cfg.port          = m_port->value();
    cfg.mount         = m_mount->text();
    cfg.password      = m_password->text();
    cfg.adminUser     = m_adminUser->text();
    cfg.adminPass     = m_adminPass->text();
    cfg.stationName   = m_stationName->text();
    cfg.description   = m_description->text();
    cfg.genre         = m_genre->text();
    cfg.url           = m_url->text();
    cfg.isPublic      = m_isPublic->isChecked();
    cfg.useSsl           = m_useSsl->isChecked();
    cfg.autoReconnect    = m_autoReconnect->isChecked();
    cfg.retryIntervalSec = m_retryInterval->value();
    cfg.maxRetries       = m_maxRetries->value();
    cfg.autoStart        = m_autoStart->isChecked();

    cfg.dspEnabled      = m_dspEnabled->isChecked();
    cfg.eqPreset        = m_eqPreset->currentText();
    cfg.agcEnabled      = m_agcEnabled->isChecked();
    cfg.agcInputGainDb  = static_cast<float>(m_agcInputGain->value());
    cfg.agcThresholdDb  = static_cast<float>(m_agcThreshold->value());
    cfg.agcRatio        = static_cast<float>(m_agcRatio->value());
    cfg.agcAttackMs     = static_cast<float>(m_agcAttack->value());
    cfg.agcReleaseMs    = static_cast<float>(m_agcRelease->value());
    cfg.agcMakeupGainDb = static_cast<float>(m_agcMakeup->value());
    cfg.agcLimiterDb    = static_cast<float>(m_agcLimiter->value());
    cfg.pttDuckEnabled  = m_pttDuck->isChecked();
    cfg.pttDuckAttenDb  = static_cast<float>(m_pttAttenDb->value());

    cfg.archiveEnabled  = m_archiveEnabled->isChecked();
    cfg.archiveDir      = m_archiveDir->text();
    cfg.archiveWav      = m_archiveWav->isChecked();
    cfg.archiveMp3      = m_archiveMp3->isChecked();

    cfg.icy2Fields.clear();
    for (auto it = m_icy2Edits.constBegin(); it != m_icy2Edits.constEnd(); ++it)
        if (!it.value()->text().isEmpty())
            cfg.icy2Fields[it.key()] = it.value()->text();

    return cfg;
}

void EncoderConfigDialog::onAccepted()
{
    m_slot->configure(readConfig());
    accept();
}

// ─────────────────────────────────────────────────────────────────────────────
// Stats refresh
// ─────────────────────────────────────────────────────────────────────────────
void EncoderConfigDialog::refreshStats()
{
    using S = EncoderSlot::State;
    const S st = m_slot->state();
    const QString stateNames[] = {
        "Idle","Starting","Connecting","Streaming","Reconnecting","Sleep","Error"};
    m_statState->setText(stateNames[static_cast<int>(st)]);

    const int secs = m_slot->connectedSecs();
    m_statUptime->setText(st == S::Streaming
        ? QString("%1:%2:%3")
            .arg(secs/3600,   2, 10, QChar('0'))
            .arg((secs%3600)/60, 2, 10, QChar('0'))
            .arg(secs%60,     2, 10, QChar('0'))
        : "–");

    const qint64 bytes = m_slot->bytesSent();
    m_statBytes->setText(bytes > 0 ? QString("%1 bytes").arg(bytes) : "–");
}
