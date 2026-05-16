#include "DeviceSettingsDialog.h"
#include "SurfaceTabBar.h"
#include "ThemePalette.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QTabWidget>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QGroupBox>
#include <QSettings>
#include <QMediaDevices>
#include <QAudioDevice>
#include <QMessageBox>

// ─── Constructor ──────────────────────────────────────────────────────────────
DeviceSettingsDialog::DeviceSettingsDialog(M1::IAudioEngine* engine,
                                           SurfaceTabBar*    surfaceBar,
                                           QWidget*          parent)
    : QDialog(parent)
    , m_engine(engine)
    , m_surfaceBar(surfaceBar)
{
    setWindowTitle("Device Settings");
    setMinimumSize(560, 420);

    auto* root = new QVBoxLayout(this);
    root->setSpacing(8);
    root->setContentsMargins(12, 12, 12, 8);

    auto* tabs = new QTabWidget(this);
    buildGlobalTab();
    buildSurfaceTab();

    // ── Tabs widget (built above using m_* members) ────────────────────
    // Re-create tabs widget and add the already-built pages
    auto* globalPage  = new QWidget;
    auto* surfacePage = new QWidget;

    {
        auto* lay = new QFormLayout(globalPage);
        lay->setSpacing(8);

        m_inputCombo = new QComboBox(globalPage);
        lay->addRow("Input Device:",  m_inputCombo);

        m_outputCombo = new QComboBox(globalPage);
        lay->addRow("Output Device:", m_outputCombo);

        m_sampleRateCombo = new QComboBox(globalPage);
        for (int sr : {44100, 48000, 88200, 96000})
            m_sampleRateCombo->addItem(QString("%1 Hz").arg(sr), sr);
        lay->addRow("Sample Rate:",   m_sampleRateCombo);

        m_bufferSizeCombo = new QComboBox(globalPage);
        for (int buf : {64, 128, 256, 512, 1024, 2048})
            m_bufferSizeCombo->addItem(QString("%1 frames").arg(buf), buf);
        lay->addRow("Buffer Size:",   m_bufferSizeCombo);

        auto* refreshBtn = new QPushButton("↺ Refresh Devices", globalPage);
        refreshBtn->setFixedWidth(140);
        connect(refreshBtn, &QPushButton::clicked, this, &DeviceSettingsDialog::onRefreshDevices);
        lay->addRow("", refreshBtn);

        auto* note = new QLabel(
            "※ Supports ASIO, WASAPI, WDM-KS, DirectSound, and Mcaster1AudioPipe virtual devices.",
            globalPage);
        note->setWordWrap(true);
        note->setStyleSheet(
            QString("color:%1; font-size:12px;")
                .arg(ThemePalette::forCurrentTheme().textMuted.name()));
        lay->addRow(note);

        populateDevices();

        QSettings s("Mcaster1", "Mcaster1Studio");
        const int lastIn  = s.value("audio/inputDevice",  -1).toInt();
        const int lastOut = s.value("audio/outputDevice", -1).toInt();
        for (int i = 0; i < m_inputIndices.size();  ++i)
            if (m_inputIndices[i]  == lastIn)  { m_inputCombo->setCurrentIndex(i);  break; }
        for (int i = 0; i < m_outputIndices.size(); ++i)
            if (m_outputIndices[i] == lastOut) { m_outputCombo->setCurrentIndex(i); break; }
        m_sampleRateCombo->setCurrentIndex(
            m_sampleRateCombo->findData(s.value("audio/sampleRate", 48000).toInt()));
        m_bufferSizeCombo->setCurrentIndex(
            m_bufferSizeCombo->findData(s.value("audio/bufferSize", 512).toInt()));
    }

    {
        auto* lay = new QVBoxLayout(surfacePage);
        lay->setSpacing(4);

        auto* info = new QLabel(
            "Assign independent input/output devices per surface. "
            "Select '(Use Global)' to inherit the global device setting.\n"
            "Supports virtual audio cables and Mcaster1AudioPipe loopback devices.",
            surfacePage);
        info->setWordWrap(true);
        info->setStyleSheet(
            QString("color:%1; font-size:12px;")
                .arg(ThemePalette::forCurrentTheme().textMuted.name()));
        lay->addWidget(info);

        m_surfaceTable = new QTableWidget(surfacePage);
        m_surfaceTable->setColumnCount(3);
        m_surfaceTable->setHorizontalHeaderLabels({"Surface", "Input Device", "Output Device"});
        m_surfaceTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        m_surfaceTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        m_surfaceTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
        m_surfaceTable->verticalHeader()->setVisible(false);
        m_surfaceTable->setSelectionMode(QAbstractItemView::NoSelection);
        m_surfaceTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

        // Enumerate open surfaces
        QSettings s("Mcaster1", "Mcaster1Studio");
        const QList<QAudioDevice> qtInputs  = QMediaDevices::audioInputs();
        const QList<QAudioDevice> qtOutputs = QMediaDevices::audioOutputs();

        if (m_surfaceBar) {
            for (int t = 0; t < m_surfaceBar->count(); ++t) {
                const QString surfName = m_surfaceBar->tabText(t);
                const int row = m_surfaceTable->rowCount();
                m_surfaceTable->insertRow(row);

                auto* nameItem = new QTableWidgetItem(surfName);
                nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
                m_surfaceTable->setItem(row, 0, nameItem);

                auto* inCombo  = new QComboBox;
                auto* outCombo = new QComboBox;
                inCombo->addItem("(Use Global)", QString());
                outCombo->addItem("(Use Global)", QString());
                for (const auto& d : qtInputs)
                    inCombo->addItem(d.description(), QString::fromUtf8(d.id()));
                for (const auto& d : qtOutputs)
                    outCombo->addItem(d.description(), QString::fromUtf8(d.id()));

                // Restore saved per-surface settings
                const QString key = "audio/surface/" + surfName;
                const QString savedIn  = s.value(key + "/input").toString();
                const QString savedOut = s.value(key + "/output").toString();
                if (!savedIn.isEmpty())
                    inCombo->setCurrentIndex(
                        std::max(0, inCombo->findData(savedIn)));
                if (!savedOut.isEmpty())
                    outCombo->setCurrentIndex(
                        std::max(0, outCombo->findData(savedOut)));

                m_surfaceTable->setCellWidget(row, 1, inCombo);
                m_surfaceTable->setCellWidget(row, 2, outCombo);
            }
        }

        if (m_surfaceTable->rowCount() == 0) {
            m_surfaceTable->insertRow(0);
            auto* item = new QTableWidgetItem("No surfaces open.");
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            m_surfaceTable->setItem(0, 0, item);
        }

        lay->addWidget(m_surfaceTable, 1);
    }

    tabs->addTab(globalPage,  "Global Devices");
    tabs->addTab(surfacePage, "Surface Assignments");
    root->addWidget(tabs, 1);

    // ── Dialog buttons ────────────────────────────────────────────────
    auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(btns, &QDialogButtonBox::accepted, this, [this]() {
        // Save per-surface settings on accept
        if (m_surfaceTable && m_surfaceBar) {
            QSettings s("Mcaster1", "Mcaster1Studio");
            for (int row = 0; row < m_surfaceTable->rowCount(); ++row) {
                auto* nameItem = m_surfaceTable->item(row, 0);
                if (!nameItem) continue;
                const QString surfName = nameItem->text();
                const QString key = "audio/surface/" + surfName;
                if (auto* c = qobject_cast<QComboBox*>(m_surfaceTable->cellWidget(row, 1)))
                    s.setValue(key + "/input",  c->currentData().toString());
                if (auto* c = qobject_cast<QComboBox*>(m_surfaceTable->cellWidget(row, 2)))
                    s.setValue(key + "/output", c->currentData().toString());
            }
        }
        accept();
    });
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(btns);
}

void DeviceSettingsDialog::buildGlobalTab() {}   // content built inline in ctor
void DeviceSettingsDialog::buildSurfaceTab() {}  // content built inline in ctor

void DeviceSettingsDialog::populateDevices() {
    m_inputCombo->clear();
    m_outputCombo->clear();
    m_inputIndices.clear();
    m_outputIndices.clear();

    m_inputCombo->addItem("(None)", -1);
    m_inputIndices.append(-1);
    m_outputCombo->addItem("(None)", -1);
    m_outputIndices.append(-1);

    if (!m_engine) return;

    for (const auto& d : m_engine->inputDevices()) {
        QString name = QString("[%1] %2").arg(d.hostApi).arg(d.name);
        if (d.isDefault) name += " *";
        m_inputCombo->addItem(name, d.index);
        m_inputIndices.append(d.index);
    }
    for (const auto& d : m_engine->outputDevices()) {
        QString name = QString("[%1] %2").arg(d.hostApi).arg(d.name);
        if (d.isDefault) name += " *";
        m_outputCombo->addItem(name, d.index);
        m_outputIndices.append(d.index);
    }
}

void DeviceSettingsDialog::onRefreshDevices() {
    const int prevIn  = m_inputCombo->currentIndex();
    const int prevOut = m_outputCombo->currentIndex();
    populateDevices();
    m_inputCombo->setCurrentIndex(
        std::min(prevIn,  m_inputCombo->count()  - 1));
    m_outputCombo->setCurrentIndex(
        std::min(prevOut, m_outputCombo->count() - 1));
}

void DeviceSettingsDialog::applyGlobalSettings() {
    if (!m_engine) return;

    QSettings s("Mcaster1", "Mcaster1Studio");
    const int inDev  = m_inputCombo->currentData().toInt();
    const int outDev = m_outputCombo->currentData().toInt();
    const int sr     = m_sampleRateCombo->currentData().toInt();
    const int buf    = m_bufferSizeCombo->currentData().toInt();

    s.setValue("audio/inputDevice",  inDev);
    s.setValue("audio/outputDevice", outDev);
    s.setValue("audio/sampleRate",   sr);
    s.setValue("audio/bufferSize",   buf);

    const bool wasRunning = m_engine->isRunning();
    if (wasRunning) m_engine->stopStream();
    m_engine->openStream(inDev, outDev, sr, buf);
    if (wasRunning) m_engine->startStream();
}
