/// @file   TelePromptModule.cpp
/// @path   Modules/TelePromptModule/TelePromptModule.cpp
/// @author Mcaster1 / dstjohn
/// @date   2026-03-09
/// @title  M1S-TelePrompt — Scrolling Script Teleprompter Implementation
/// @purpose Implements the teleprompter scroll engine, mirror mode rendering,
///          section marker navigation, and operator control widget.
/// @reason  Pastor-facing production tool for prepared delivery.
/// @changelog
///   2026-03-09  Initial implementation

#include "TelePromptModule.h"
#include "GraphicsEngineModule.h"
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QTextEdit>
#include <QListWidget>
#include <QCheckBox>
#include <QScrollBar>
#include <QFileDialog>
#include <QInputDialog>
#include <QPainter>
#include <QSettings>
#include <QDebug>

namespace M1 {

namespace {

// ─── PrompterDisplay — the scrolling text display ───────────────────────────
/// We render the script text as a vertically scrolling view. When mirror mode
/// is enabled, we flip the entire widget horizontally for beam-splitter rigs.
class PrompterDisplay : public QWidget {
    Q_OBJECT
public:
    explicit PrompterDisplay(TelePromptModule* mod, QWidget* parent = nullptr)
        : QWidget(parent), m_module(mod)
    {
        setObjectName("PrompterDisplay");
        setMinimumSize(300, 200);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

        connect(mod, &TelePromptModule::scrollPositionChanged, this,
                [this](double) { update(); });
        connect(mod, &TelePromptModule::scriptChanged, this,
                [this]() { update(); });
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::TextAntialiasing, true);

        // We fill the background using the module's configured color
        QColor bg = m_module->backgroundColor();
        p.fillRect(rect(), bg);

        // We apply mirror transformation if enabled
        if (m_module->mirrorMode()) {
            p.translate(width(), 0);
            p.scale(-1.0, 1.0);
        }

        const int pad = 20;
        const int maxW = width() - 2 * pad;

        QFont font("Arial", m_module->fontSize());
        p.setFont(font);
        QFontMetrics fm(font);
        int lineH = fm.height() + 6;

        // We split the script into lines and wrap
        QString script = m_module->script();
        if (script.isEmpty()) {
            p.setPen(QColor(100, 100, 100));
            p.drawText(rect(), Qt::AlignCenter, "No script loaded\n\nPaste or import a script to begin");
            return;
        }

        QStringList rawLines = script.split('\n');
        QStringList wrappedLines;
        for (const QString& raw : rawLines) {
            if (raw.trimmed().isEmpty()) {
                wrappedLines.append("");
                continue;
            }
            // We word-wrap long lines
            QStringList words = raw.split(' ');
            QString current;
            for (const QString& word : words) {
                QString test = current.isEmpty() ? word : current + " " + word;
                if (fm.horizontalAdvance(test) > maxW && !current.isEmpty()) {
                    wrappedLines.append(current);
                    current = word;
                } else {
                    current = test;
                }
            }
            if (!current.isEmpty()) wrappedLines.append(current);
        }

        // We draw a center guideline
        int centerY = height() / 2;
        p.setPen(QColor(255, 60, 60, 80));
        p.drawLine(pad, centerY, width() - pad, centerY);

        // We draw the text scrolled by m_scrollPosition
        double scrollY = m_module->scrollPosition();
        int startLine = qMax(0, static_cast<int>(scrollY / lineH) - 5);

        QColor textCol = m_module->textColor();
        p.setPen(textCol);
        for (int i = startLine; i < wrappedLines.size(); ++i) {
            int y = static_cast<int>(i * lineH - scrollY + centerY);
            if (y > height() + lineH) break;
            if (y < -lineH) continue;

            // We fade text that's far from the center line
            int dist = qAbs(y - centerY);
            int alpha = qMax(40, 255 - dist * 2);
            p.setPen(QColor(textCol.red(), textCol.green(), textCol.blue(), alpha));

            p.drawText(QRect(pad, y, maxW, lineH),
                       Qt::AlignLeft | Qt::AlignVCenter, wrappedLines[i]);
        }

        // We draw the "current line" highlight
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 255, 255, 15));
        p.drawRect(0, centerY - lineH / 2, width(), lineH);
    }

private:
    TelePromptModule* m_module;
};

} // anonymous namespace

// ─── TelePromptModule implementation ─────────────────────────────────────────

TelePromptModule::TelePromptModule(QObject* parent)
    : IModule(parent)
    , m_scrollTimer(new QTimer(this))
{
    // We tick at 30fps for smooth scrolling
    m_scrollTimer->setInterval(33);
    connect(m_scrollTimer, &QTimer::timeout, this, &TelePromptModule::onScrollTick);
}

TelePromptModule::~TelePromptModule() {
    shutdown();
}

void TelePromptModule::initialize() {
    qInfo() << "[TelePrompt] Initialized — font:" << m_fontSize << "pt  speed:"
            << m_scrollSpeed << "px/s  mirror:" << m_mirrorMode;
}

void TelePromptModule::shutdown() {
    m_scrollTimer->stop();
}

QWidget* TelePromptModule::createWidget(QWidget* parent) {
    auto* container = new QWidget(parent);
    container->setObjectName("TelePromptWidget");
    auto* mainLay = new QVBoxLayout(container);
    mainLay->setContentsMargins(4, 4, 4, 4);
    mainLay->setSpacing(4);

    // ── Prompter display ────────────────────────────────────────────────────
    auto* display = new PrompterDisplay(this, container);
    mainLay->addWidget(display, 3);

    // ── Control bar ─────────────────────────────────────────────────────────
    auto* controlGroup = new QGroupBox("Controls", container);
    auto* controlLay = new QGridLayout(controlGroup);
    controlLay->setContentsMargins(4, 8, 4, 4);
    int row = 0;

    // Transport buttons
    auto* transportRow = new QHBoxLayout();
    auto* scrollBtn = new QPushButton("Scroll", controlGroup);
    scrollBtn->setToolTip("Start / pause scrolling");
    scrollBtn->setCheckable(true);
    scrollBtn->setFixedHeight(36);
    auto* resetBtn = new QPushButton("Reset", controlGroup);
    resetBtn->setToolTip("Reset scroll position to top");
    resetBtn->setFixedHeight(36);
    transportRow->addWidget(scrollBtn);
    transportRow->addWidget(resetBtn);
    controlLay->addLayout(transportRow, row++, 0, 1, 4);

    // Speed control
    controlLay->addWidget(new QLabel("Speed:", controlGroup), row, 0);
    auto* speedSlider = new QSlider(Qt::Horizontal, controlGroup);
    speedSlider->setRange(10, 200);
    speedSlider->setValue(static_cast<int>(m_scrollSpeed));
    speedSlider->setToolTip("Scroll speed (pixels/second)");
    controlLay->addWidget(speedSlider, row, 1, 1, 2);
    auto* speedLabel = new QLabel(QString::number(m_scrollSpeed, 'f', 0) + " px/s", controlGroup);
    controlLay->addWidget(speedLabel, row++, 3);

    // Font size
    controlLay->addWidget(new QLabel("Font:", controlGroup), row, 0);
    auto* fontSpin = new QSpinBox(controlGroup);
    fontSpin->setRange(16, 72);
    fontSpin->setValue(m_fontSize);
    fontSpin->setSuffix(" pt");
    fontSpin->setToolTip("Prompter text font size");
    controlLay->addWidget(fontSpin, row, 1);

    // Mirror mode
    auto* mirrorCheck = new QCheckBox("Mirror", controlGroup);
    mirrorCheck->setChecked(m_mirrorMode);
    mirrorCheck->setToolTip("Enable mirror mode for beam-splitter prompter hardware");
    controlLay->addWidget(mirrorCheck, row++, 2, 1, 2);

    mainLay->addWidget(controlGroup);

    // ── Script editor (collapsible) ─────────────────────────────────────────
    auto* scriptGroup = new QGroupBox("Script", container);
    auto* scriptLay = new QVBoxLayout(scriptGroup);

    auto* scriptEdit = new QTextEdit(scriptGroup);
    scriptEdit->setObjectName("TelePromptScript");
    scriptEdit->setPlainText(m_script);
    scriptEdit->setPlaceholderText("Paste or type the sermon script here...");
    scriptEdit->setToolTip("Enter the full script text — use blank lines to separate sections");
    scriptLay->addWidget(scriptEdit, 1);

    auto* scriptBtnRow = new QHBoxLayout();
    auto* importBtn = new QPushButton("Import...", scriptGroup);
    importBtn->setToolTip("Import script from text file");
    auto* applyBtn = new QPushButton("Apply Script", scriptGroup);
    applyBtn->setToolTip("Apply the edited script to the prompter");
    scriptBtnRow->addWidget(importBtn);
    scriptBtnRow->addStretch();
    scriptBtnRow->addWidget(applyBtn);
    scriptLay->addLayout(scriptBtnRow);

    // Markers list
    auto* markerRow = new QHBoxLayout();
    auto* markerList = new QListWidget(scriptGroup);
    markerList->setMaximumHeight(80);
    markerList->setToolTip("Section markers — click to jump");
    markerRow->addWidget(markerList, 1);
    auto* addMarkerBtn = new QPushButton("+ Marker", scriptGroup);
    addMarkerBtn->setToolTip("Add a marker at the current cursor position");
    markerRow->addWidget(addMarkerBtn);
    scriptLay->addLayout(markerRow);

    mainLay->addWidget(scriptGroup, 2);

    // ── Wire signals ────────────────────────────────────────────────────────

    connect(scrollBtn, &QPushButton::clicked, this, [this, scrollBtn](bool checked) {
        if (checked) {
            if (m_paused) resumeScroll(); else startScroll();
            scrollBtn->setText("Pause");
        } else {
            pauseScroll();
            scrollBtn->setText("Scroll");
        }
    });
    connect(this, &TelePromptModule::scrollStateChanged, scrollBtn,
            [scrollBtn](bool scrolling) {
        scrollBtn->setChecked(scrolling);
        scrollBtn->setText(scrolling ? "Pause" : "Scroll");
    });

    connect(resetBtn, &QPushButton::clicked, this, [this]() {
        stopScroll();
        setScrollPosition(0);
    });

    connect(speedSlider, &QSlider::valueChanged, this, [this, speedLabel](int v) {
        setScrollSpeed(v);
        speedLabel->setText(QString::number(v) + " px/s");
    });

    connect(fontSpin, &QSpinBox::valueChanged, this, [this](int v) {
        setFontSize(v);
    });

    connect(mirrorCheck, &QCheckBox::toggled, this, &TelePromptModule::setMirrorMode);

    connect(applyBtn, &QPushButton::clicked, this, [this, scriptEdit]() {
        setScript(scriptEdit->toPlainText());
    });

    connect(importBtn, &QPushButton::clicked, this, [this, scriptEdit]() {
        QString path = QFileDialog::getOpenFileName(nullptr, "Import Script",
            {}, "Text Files (*.txt);;Word Documents (*.docx);;All Files (*)");
        if (path.isEmpty()) return;
        QFile f(path);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString text = QString::fromUtf8(f.readAll());
            scriptEdit->setPlainText(text);
            setScript(text);
        }
    });

    connect(addMarkerBtn, &QPushButton::clicked, this,
            [this, scriptEdit, markerList]() {
        int pos = scriptEdit->textCursor().position();
        bool ok;
        QString name = QInputDialog::getText(nullptr, "Marker Name",
            "Enter marker name:", QLineEdit::Normal, "", &ok);
        if (!ok || name.isEmpty()) return;
        addMarker(name, pos);
        markerList->addItem(name);
    });

    connect(markerList, &QListWidget::currentRowChanged, this,
            [this](int row) { if (row >= 0) jumpToMarker(row); });

    // We populate existing markers
    for (const auto& m : m_markers)
        markerList->addItem(m.name);

    return container;
}

void TelePromptModule::saveState(QSettings& s) {
    s.beginGroup("TelePrompt");
    s.setValue("script",        m_script);
    s.setValue("fontSize",      m_fontSize);
    s.setValue("scrollSpeed",   m_scrollSpeed);
    s.setValue("mirrorMode",    m_mirrorMode);
    s.setValue("markerCount",   m_markers.size());
    for (int i = 0; i < m_markers.size(); ++i) {
        s.setValue(QString("marker_%1_name").arg(i),   m_markers[i].name);
        s.setValue(QString("marker_%1_offset").arg(i), m_markers[i].charOffset);
    }
    s.endGroup();
}

void TelePromptModule::loadState(QSettings& s) {
    s.beginGroup("TelePrompt");
    m_script      = s.value("script").toString();
    m_fontSize    = s.value("fontSize", 32).toInt();
    m_scrollSpeed = s.value("scrollSpeed", 60.0).toDouble();
    m_mirrorMode  = s.value("mirrorMode", false).toBool();
    int count     = s.value("markerCount", 0).toInt();
    m_markers.clear();
    for (int i = 0; i < count; ++i) {
        ScriptMarker m;
        m.name       = s.value(QString("marker_%1_name").arg(i)).toString();
        m.charOffset = s.value(QString("marker_%1_offset").arg(i), 0).toInt();
        m_markers.append(m);
    }
    s.endGroup();
}

// ─── Script management ───────────────────────────────────────────────────────

void TelePromptModule::setScript(const QString& text) {
    m_script = text;
    m_scrollPosition = 0;
    emit scriptChanged();
    emit scrollPositionChanged(0);
    qInfo() << "[TelePrompt] Script loaded —" << text.length() << "chars";
}

void TelePromptModule::addMarker(const QString& name, int charOffset) {
    m_markers.append({name, charOffset});
}

void TelePromptModule::removeMarker(int index) {
    if (index >= 0 && index < m_markers.size())
        m_markers.removeAt(index);
}

// ─── Scroll control ─────────────────────────────────────────────────────────

void TelePromptModule::startScroll() {
    m_scrolling = true;
    m_paused = false;
    m_scrollTimer->start();
    emit scrollStateChanged(true);
    qInfo() << "[TelePrompt] Scroll started at" << m_scrollSpeed << "px/s";
}

void TelePromptModule::stopScroll() {
    m_scrolling = false;
    m_paused = false;
    m_scrollTimer->stop();
    m_scrollPosition = 0;
    emit scrollStateChanged(false);
    emit scrollPositionChanged(0);
}

void TelePromptModule::pauseScroll() {
    m_scrolling = false;
    m_paused = true;
    m_scrollTimer->stop();
    emit scrollStateChanged(false);
}

void TelePromptModule::resumeScroll() {
    m_scrolling = true;
    m_paused = false;
    m_scrollTimer->start();
    emit scrollStateChanged(true);
}

void TelePromptModule::setScrollSpeed(double pixelsPerSecond) {
    m_scrollSpeed = qBound(5.0, pixelsPerSecond, 300.0);
    emit scrollSpeedChanged(m_scrollSpeed);
}

void TelePromptModule::setScrollPosition(double position) {
    m_scrollPosition = qMax(0.0, position);
    emit scrollPositionChanged(m_scrollPosition);
}

void TelePromptModule::jumpToMarker(int markerIndex) {
    if (markerIndex < 0 || markerIndex >= m_markers.size()) return;
    // We estimate the pixel position from the character offset
    // (rough approximation: ~40 chars per line, lineH ~40px)
    int charOffset = m_markers[markerIndex].charOffset;
    double estimatedLine = charOffset / 40.0;
    double estimatedY = estimatedLine * (m_fontSize + 10);
    setScrollPosition(estimatedY);
    emit markerReached(markerIndex);
}

// ─── Display settings ────────────────────────────────────────────────────────

void TelePromptModule::setFontSize(int sizePt) {
    m_fontSize = qBound(16, sizePt, 72);
}

void TelePromptModule::setMirrorMode(bool mirror) {
    m_mirrorMode = mirror;
    emit scriptChanged(); // We trigger redraw
}

void TelePromptModule::setTextColor(const QColor& color) { m_textColor = color; }
void TelePromptModule::setBackgroundColor(const QColor& color) { m_bgColor = color; }

// ─── Scroll tick ─────────────────────────────────────────────────────────────

void TelePromptModule::onScrollTick() {
    // We advance the scroll position based on speed and tick interval (33ms)
    double deltaMs = 33.0;
    double deltaPx = m_scrollSpeed * (deltaMs / 1000.0);
    m_scrollPosition += deltaPx;
    emit scrollPositionChanged(m_scrollPosition);
}

} // namespace M1

// ─── Plugin C ABI exports ────────────────────────────────────────────────────
static Mcaster1PluginInfo s_telepromptInfo = {
    MCASTER1_PLUGIN_API_VERSION,
    "com.mcaster1.church.teleprompt",
    "TelePrompter",
    "1.0.0",
    "church",
    "module",
    "Mcaster1",
    "Scrolling script teleprompter — adjustable speed, mirror mode, section markers"
};

extern "C" {
MCASTER1_PLUGIN_API Mcaster1PluginInfo* mcaster1_teleprompt_plugin_info() { return &s_telepromptInfo; }
MCASTER1_PLUGIN_API IModule* mcaster1_teleprompt_create_module(IModuleHost*) {
    return new M1::TelePromptModule();
}
MCASTER1_PLUGIN_API void mcaster1_teleprompt_destroy_module(IModule* m) { delete m; }
}

#include "TelePromptModule.moc"
