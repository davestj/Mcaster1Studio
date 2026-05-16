/// @file   GraphicsEngineModule.cpp
/// @path   Modules/GraphicsEngineModule/GraphicsEngineModule.cpp
/// @author Mcaster1 / dstjohn
/// @date   2026-03-09
/// @title  M1S-GraphicsEngine — Template and Overlay Rendering Backend Implementation
/// @purpose Implements the rendering engine for all Church Surface visual output.
///          We handle text layout, background compositing, lower third overlays,
///          theme management, and output for multiple resolutions/targets.
/// @reason  Foundation layer — LyricsCaster, ScriptureCaster, AnnounceCaster, and
///          TelePrompt all call our render methods to produce display-ready frames.
/// @changelog
///   2026-03-09  Initial implementation

#include "GraphicsEngineModule.h"
#include <QPainter>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QFontMetrics>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QColorDialog>
#include <QFileDialog>
#include <QScrollArea>
#include <QSettings>
#include <QDebug>

namespace M1 {

// ─── Anonymous namespace: widget implementations ─────────────────────────────
namespace {

// ─── PreviewWidget — shows a live preview of the current render output ──────
class PreviewWidget : public QWidget {
    Q_OBJECT
public:
    explicit PreviewWidget(GraphicsEngineModule* mod, QWidget* parent = nullptr)
        : QWidget(parent), m_module(mod)
    {
        setObjectName("GraphicsPreview");
        setMinimumSize(320, 180);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

        // We show a test render on connect
        connect(mod, &GraphicsEngineModule::frameRendered, this,
                [this](const QImage& frame) {
            m_frame = frame;
            update();
        });

        connect(mod, &GraphicsEngineModule::themeChanged, this,
                [this](const QString&) { refreshPreview(); });

        // Initial preview
        QTimer::singleShot(100, this, [this]() { refreshPreview(); });
    }

    void refreshPreview() {
        RenderRequest req;
        req.type        = TemplateType::Lyrics;
        req.primaryText = "Amazing grace, how sweet the sound\nThat saved a wretch like me";
        req.reference   = "Amazing Grace — John Newton";
        req.attribution = "CCLI #12345";
        m_frame = m_module->renderFrame(req, QSize(960, 540));
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        p.fillRect(rect(), Qt::black);

        if (!m_frame.isNull()) {
            // We maintain 16:9 aspect ratio
            QSize targetSize = m_frame.size().scaled(size(), Qt::KeepAspectRatio);
            int x = (width()  - targetSize.width())  / 2;
            int y = (height() - targetSize.height()) / 2;
            p.drawImage(QRect(x, y, targetSize.width(), targetSize.height()), m_frame);
        }

        // Draw border
        p.setPen(QColor(60, 60, 60));
        p.drawRect(0, 0, width() - 1, height() - 1);
    }

private:
    GraphicsEngineModule* m_module;
    QImage m_frame;
};

// ─── ThemeEditorWidget — quick theme properties editor ──────────────────────
class ThemeEditorWidget : public QWidget {
    Q_OBJECT
public:
    explicit ThemeEditorWidget(GraphicsEngineModule* mod, QWidget* parent = nullptr)
        : QWidget(parent), m_module(mod)
    {
        setObjectName("ThemeEditor");
        auto* lay = new QGridLayout(this);
        lay->setContentsMargins(4, 4, 4, 4);
        lay->setSpacing(4);

        int row = 0;

        // Theme selector
        lay->addWidget(new QLabel("Active Theme:", this), row, 0);
        m_themeCombo = new QComboBox(this);
        m_themeCombo->setToolTip("Select the active visual theme");
        lay->addWidget(m_themeCombo, row, 1);
        auto* addThemeBtn = new QPushButton("+", this);
        addThemeBtn->setFixedWidth(28);
        addThemeBtn->setToolTip("Create new theme");
        lay->addWidget(addThemeBtn, row, 2);
        row++;

        // Background color
        lay->addWidget(new QLabel("Background:", this), row, 0);
        m_bgColorBtn = new QPushButton(this);
        m_bgColorBtn->setFixedSize(60, 24);
        m_bgColorBtn->setToolTip("Click to change background color");
        lay->addWidget(m_bgColorBtn, row, 1);
        row++;

        // Text color
        lay->addWidget(new QLabel("Text Color:", this), row, 0);
        m_textColorBtn = new QPushButton(this);
        m_textColorBtn->setFixedSize(60, 24);
        m_textColorBtn->setToolTip("Click to change text color");
        lay->addWidget(m_textColorBtn, row, 1);
        row++;

        // Accent color
        lay->addWidget(new QLabel("Accent:", this), row, 0);
        m_accentColorBtn = new QPushButton(this);
        m_accentColorBtn->setFixedSize(60, 24);
        m_accentColorBtn->setToolTip("Click to change accent color");
        lay->addWidget(m_accentColorBtn, row, 1);
        row++;

        // Font family
        lay->addWidget(new QLabel("Font:", this), row, 0);
        m_fontCombo = new QComboBox(this);
        m_fontCombo->addItems({"Arial", "Open Sans", "Roboto", "Georgia",
                               "Times New Roman", "Verdana", "Calibri"});
        m_fontCombo->setToolTip("Select the primary display font");
        lay->addWidget(m_fontCombo, row, 1, 1, 2);
        row++;

        // Title font size
        lay->addWidget(new QLabel("Title Size:", this), row, 0);
        m_titleSize = new QSpinBox(this);
        m_titleSize->setRange(24, 96);
        m_titleSize->setSuffix("px");
        m_titleSize->setToolTip("Title/lyrics font size in pixels");
        lay->addWidget(m_titleSize, row, 1);
        row++;

        // Body font size
        lay->addWidget(new QLabel("Body Size:", this), row, 0);
        m_bodySize = new QSpinBox(this);
        m_bodySize->setRange(16, 72);
        m_bodySize->setSuffix("px");
        m_bodySize->setToolTip("Body/verse text font size in pixels");
        lay->addWidget(m_bodySize, row, 1);
        row++;

        // Background image
        lay->addWidget(new QLabel("Background:", this), row, 0);
        auto* bgRow = new QHBoxLayout();
        m_bgImagePath = new QLineEdit(this);
        m_bgImagePath->setPlaceholderText("Background image path...");
        m_bgImagePath->setToolTip("Path to background image file");
        bgRow->addWidget(m_bgImagePath);
        auto* bgBrowse = new QPushButton("...", this);
        bgBrowse->setFixedWidth(28);
        bgBrowse->setToolTip("Browse for background image");
        bgRow->addWidget(bgBrowse);
        lay->addLayout(bgRow, row, 1, 1, 2);
        row++;

        // Logo
        lay->addWidget(new QLabel("Logo:", this), row, 0);
        auto* logoRow = new QHBoxLayout();
        m_logoPath = new QLineEdit(this);
        m_logoPath->setPlaceholderText("Church logo path...");
        m_logoPath->setToolTip("Path to church logo image file");
        logoRow->addWidget(m_logoPath);
        auto* logoBrowse = new QPushButton("...", this);
        logoBrowse->setFixedWidth(28);
        logoBrowse->setToolTip("Browse for logo image");
        logoRow->addWidget(logoBrowse);
        lay->addLayout(logoRow, row, 1, 1, 2);

        // ── Wire signals ────────────────────────────────────────────────
        connect(m_themeCombo, &QComboBox::currentTextChanged, this,
                [this](const QString& name) { m_module->setActiveTheme(name); });

        connect(addThemeBtn, &QPushButton::clicked, this, [this]() {
            VisualTheme theme;
            theme.name = QString("Theme %1").arg(m_themeCombo->count() + 1);
            m_module->addTheme(theme);
            refreshThemeList();
            m_themeCombo->setCurrentText(theme.name);
        });

        auto colorPicker = [this](QPushButton* btn, QColor& target) {
            connect(btn, &QPushButton::clicked, this, [this, btn, &target]() {
                QColor c = QColorDialog::getColor(target, this, "Select Color");
                if (c.isValid()) {
                    target = c;
                    btn->setStyleSheet(QString("background-color: %1; border: 1px solid %2;")
                        .arg(c.name(), c.darker(130).name()));
                    applyToModule();
                }
            });
        };

        // We get a mutable reference to update theme in-place
        colorPicker(m_bgColorBtn, m_editBg);
        colorPicker(m_textColorBtn, m_editText);
        colorPicker(m_accentColorBtn, m_editAccent);

        connect(m_fontCombo, &QComboBox::currentTextChanged, this,
                [this](const QString&) { applyToModule(); });
        connect(m_titleSize, &QSpinBox::valueChanged, this,
                [this]() { applyToModule(); });
        connect(m_bodySize, &QSpinBox::valueChanged, this,
                [this]() { applyToModule(); });
        connect(bgBrowse, &QPushButton::clicked, this, [this]() {
            QString path = QFileDialog::getOpenFileName(this, "Background Image",
                {}, "Images (*.png *.jpg *.jpeg *.bmp)");
            if (!path.isEmpty()) {
                m_bgImagePath->setText(path);
                applyToModule();
            }
        });
        connect(logoBrowse, &QPushButton::clicked, this, [this]() {
            QString path = QFileDialog::getOpenFileName(this, "Logo Image",
                {}, "Images (*.png *.jpg *.jpeg *.svg)");
            if (!path.isEmpty()) {
                m_logoPath->setText(path);
                applyToModule();
            }
        });

        refreshThemeList();
        loadFromTheme(m_module->activeTheme());
    }

    void refreshThemeList() {
        m_themeCombo->blockSignals(true);
        m_themeCombo->clear();
        for (const auto& name : m_module->themeNames())
            m_themeCombo->addItem(name);
        m_themeCombo->setCurrentText(m_module->activeTheme().name);
        m_themeCombo->blockSignals(false);
    }

    void loadFromTheme(const VisualTheme& t) {
        m_editBg     = t.backgroundColor;
        m_editText   = t.textColor;
        m_editAccent = t.accentColor;

        m_bgColorBtn->setStyleSheet(
            QString("background-color: %1; border: 1px solid %2;")
                .arg(t.backgroundColor.name(), t.backgroundColor.darker(130).name()));
        m_textColorBtn->setStyleSheet(
            QString("background-color: %1; border: 1px solid %2;")
                .arg(t.textColor.name(), t.textColor.darker(130).name()));
        m_accentColorBtn->setStyleSheet(
            QString("background-color: %1; border: 1px solid %2;")
                .arg(t.accentColor.name(), t.accentColor.darker(130).name()));

        m_fontCombo->setCurrentText(t.fontFamily);
        m_titleSize->setValue(t.titleFontSize);
        m_bodySize->setValue(t.bodyFontSize);
        m_bgImagePath->setText(t.backgroundImagePath);
        m_logoPath->setText(t.logoPath);
    }

    void applyToModule() {
        VisualTheme t = m_module->activeTheme();
        t.backgroundColor      = m_editBg;
        t.textColor             = m_editText;
        t.accentColor           = m_editAccent;
        t.fontFamily            = m_fontCombo->currentText();
        t.titleFontSize         = m_titleSize->value();
        t.bodyFontSize          = m_bodySize->value();
        t.backgroundImagePath   = m_bgImagePath->text();
        t.logoPath              = m_logoPath->text();
        m_module->addTheme(t);
        m_module->setActiveTheme(t.name);
    }

private:
    GraphicsEngineModule* m_module;
    QComboBox*   m_themeCombo      = nullptr;
    QPushButton* m_bgColorBtn      = nullptr;
    QPushButton* m_textColorBtn    = nullptr;
    QPushButton* m_accentColorBtn  = nullptr;
    QComboBox*   m_fontCombo       = nullptr;
    QSpinBox*    m_titleSize       = nullptr;
    QSpinBox*    m_bodySize        = nullptr;
    QLineEdit*   m_bgImagePath     = nullptr;
    QLineEdit*   m_logoPath        = nullptr;

    QColor m_editBg, m_editText, m_editAccent;
};

} // anonymous namespace

// ─── GraphicsEngineModule implementation ─────────────────────────────────────

GraphicsEngineModule::GraphicsEngineModule(QObject* parent)
    : IModule(parent)
{
    // We create the default "Standard Sunday" theme
    VisualTheme standard;
    standard.name = "Standard Sunday";
    m_themes.insert(standard.name, standard);
    m_activeTheme = standard;

    // We also create a few built-in themes
    VisualTheme christmas;
    christmas.name            = "Christmas";
    christmas.backgroundColor = QColor(20, 0, 0);
    christmas.accentColor     = QColor(220, 38, 38);
    christmas.textColor       = QColor(255, 255, 255);
    m_themes.insert(christmas.name, christmas);

    VisualTheme easter;
    easter.name            = "Easter";
    easter.backgroundColor = QColor(30, 10, 40);
    easter.accentColor     = QColor(147, 51, 234);
    easter.textColor       = QColor(255, 255, 255);
    m_themes.insert(easter.name, easter);

    VisualTheme special;
    special.name            = "Special Event";
    special.backgroundColor = QColor(0, 20, 40);
    special.accentColor     = QColor(14, 165, 233);
    special.textColor       = QColor(255, 255, 255);
    m_themes.insert(special.name, special);

    // Default render target
    RenderTarget projector;
    projector.type = RenderTarget::Type::Projector;
    projector.size = QSize(1920, 1080);
    projector.name = "Main Projector";
    m_targets.append(projector);
}

GraphicsEngineModule::~GraphicsEngineModule() = default;

void GraphicsEngineModule::initialize() {
    // We pre-cache the background and logo images
    if (!m_activeTheme.backgroundImagePath.isEmpty()) {
        m_backgroundImage.load(m_activeTheme.backgroundImagePath);
    }
    if (!m_activeTheme.logoPath.isEmpty()) {
        m_logoImage.load(m_activeTheme.logoPath);
    }
    qInfo() << "[GraphicsEngine] Initialized — active theme:" << m_activeTheme.name
            << " targets:" << m_targets.size();
}

void GraphicsEngineModule::shutdown() {
    qInfo() << "[GraphicsEngine] Shutdown";
}

QWidget* GraphicsEngineModule::createWidget(QWidget* parent) {
    auto* container = new QWidget(parent);
    container->setObjectName("GraphicsEngineWidget");
    auto* vlay = new QVBoxLayout(container);
    vlay->setContentsMargins(4, 4, 4, 4);
    vlay->setSpacing(4);

    // Preview at top
    auto* preview = new PreviewWidget(this, container);
    preview->setMinimumHeight(160);
    vlay->addWidget(preview, 2);

    // Theme editor below
    auto* scroll = new QScrollArea(container);
    scroll->setWidgetResizable(true);
    auto* editor = new ThemeEditorWidget(this, scroll);
    scroll->setWidget(editor);
    vlay->addWidget(scroll, 3);

    return container;
}

void GraphicsEngineModule::saveState(QSettings& s) {
    s.beginGroup("GraphicsEngine");
    s.setValue("activeTheme", m_activeTheme.name);
    s.setValue("themeCount", m_themes.size());

    int idx = 0;
    for (auto it = m_themes.constBegin(); it != m_themes.constEnd(); ++it, ++idx) {
        const QString prefix = QString("theme_%1/").arg(idx);
        const auto& t = it.value();
        s.setValue(prefix + "name",                t.name);
        s.setValue(prefix + "backgroundColor",     t.backgroundColor.name());
        s.setValue(prefix + "textColor",           t.textColor.name());
        s.setValue(prefix + "accentColor",         t.accentColor.name());
        s.setValue(prefix + "subtitleColor",       t.subtitleColor.name());
        s.setValue(prefix + "fontFamily",          t.fontFamily);
        s.setValue(prefix + "titleFontSize",       t.titleFontSize);
        s.setValue(prefix + "bodyFontSize",        t.bodyFontSize);
        s.setValue(prefix + "subtitleFontSize",    t.subtitleFontSize);
        s.setValue(prefix + "logoPath",            t.logoPath);
        s.setValue(prefix + "backgroundImagePath", t.backgroundImagePath);
        s.setValue(prefix + "backgroundVideoPath", t.backgroundVideoPath);
        s.setValue(prefix + "backgroundOpacity",   t.backgroundOpacity);
        s.setValue(prefix + "transitionMs",        t.transitionMs);
        s.setValue(prefix + "textPaddingPx",       t.textPaddingPx);
    }
    s.endGroup();
}

void GraphicsEngineModule::loadState(QSettings& s) {
    s.beginGroup("GraphicsEngine");
    const int count = s.value("themeCount", 0).toInt();
    if (count > 0) {
        m_themes.clear();
        for (int i = 0; i < count; ++i) {
            const QString prefix = QString("theme_%1/").arg(i);
            VisualTheme t;
            t.name                = s.value(prefix + "name", "Theme").toString();
            t.backgroundColor     = QColor(s.value(prefix + "backgroundColor", "#00001e").toString());
            t.textColor           = QColor(s.value(prefix + "textColor", "#ffffff").toString());
            t.accentColor         = QColor(s.value(prefix + "accentColor", "#3b82f6").toString());
            t.subtitleColor       = QColor(s.value(prefix + "subtitleColor", "#c8c8c8").toString());
            t.fontFamily          = s.value(prefix + "fontFamily", "Arial").toString();
            t.titleFontSize       = s.value(prefix + "titleFontSize", 48).toInt();
            t.bodyFontSize        = s.value(prefix + "bodyFontSize", 36).toInt();
            t.subtitleFontSize    = s.value(prefix + "subtitleFontSize", 24).toInt();
            t.logoPath            = s.value(prefix + "logoPath").toString();
            t.backgroundImagePath = s.value(prefix + "backgroundImagePath").toString();
            t.backgroundVideoPath = s.value(prefix + "backgroundVideoPath").toString();
            t.backgroundOpacity   = s.value(prefix + "backgroundOpacity", 0.3f).toFloat();
            t.transitionMs        = s.value(prefix + "transitionMs", 500).toInt();
            t.textPaddingPx       = s.value(prefix + "textPaddingPx", 40).toInt();
            m_themes.insert(t.name, t);
        }
    }

    const QString activeN = s.value("activeTheme", "Standard Sunday").toString();
    if (m_themes.contains(activeN))
        m_activeTheme = m_themes[activeN];
    else if (!m_themes.isEmpty())
        m_activeTheme = m_themes.first();

    s.endGroup();
}

// ─── Theme management ────────────────────────────────────────────────────────

void GraphicsEngineModule::setActiveTheme(const QString& name) {
    if (m_themes.contains(name)) {
        m_activeTheme = m_themes[name];

        // We re-cache images when theme changes
        m_backgroundImage = QImage();
        m_logoImage = QImage();
        if (!m_activeTheme.backgroundImagePath.isEmpty())
            m_backgroundImage.load(m_activeTheme.backgroundImagePath);
        if (!m_activeTheme.logoPath.isEmpty())
            m_logoImage.load(m_activeTheme.logoPath);

        emit themeChanged(name);
        qInfo() << "[GraphicsEngine] Active theme changed to:" << name;
    }
}

void GraphicsEngineModule::addTheme(const VisualTheme& theme) {
    m_themes.insert(theme.name, theme);
}

void GraphicsEngineModule::removeTheme(const QString& name) {
    if (name == m_activeTheme.name) return; // We can not remove the active theme
    m_themes.remove(name);
}

QStringList GraphicsEngineModule::themeNames() const {
    return m_themes.keys();
}

void GraphicsEngineModule::addRenderTarget(const RenderTarget& target) {
    m_targets.append(target);
}

// ─── Rendering implementation ────────────────────────────────────────────────

void GraphicsEngineModule::paintBackground(QPainter& p, const QSize& size) {
    // Solid color base
    p.fillRect(0, 0, size.width(), size.height(), m_activeTheme.backgroundColor);

    // We overlay the background image if available
    if (!m_backgroundImage.isNull()) {
        p.setOpacity(m_activeTheme.backgroundOpacity);
        QImage scaled = m_backgroundImage.scaled(size, Qt::KeepAspectRatioByExpanding,
                                                  Qt::SmoothTransformation);
        int x = (size.width()  - scaled.width())  / 2;
        int y = (size.height() - scaled.height()) / 2;
        p.drawImage(x, y, scaled);
        p.setOpacity(1.0);
    }

    // Subtle vignette effect for depth
    QRadialGradient vignette(size.width() / 2.0, size.height() / 2.0,
                             qMax(size.width(), size.height()) * 0.7);
    vignette.setColorAt(0.0, QColor(0, 0, 0, 0));
    vignette.setColorAt(1.0, QColor(0, 0, 0, 120));
    p.fillRect(0, 0, size.width(), size.height(), vignette);
}

void GraphicsEngineModule::paintLogo(QPainter& p, const QSize& size) {
    if (m_logoImage.isNull()) return;

    // We place the logo in the top-right corner, sized at 8% of the output height
    int logoH = size.height() / 12;
    QImage scaled = m_logoImage.scaled(logoH * 2, logoH, Qt::KeepAspectRatio,
                                        Qt::SmoothTransformation);
    int x = size.width() - scaled.width() - m_activeTheme.textPaddingPx;
    int y = m_activeTheme.textPaddingPx / 2;
    p.setOpacity(0.8);
    p.drawImage(x, y, scaled);
    p.setOpacity(1.0);
}

void GraphicsEngineModule::paintLowerThird(QPainter& p, const QSize& size,
                                            const QString& line1,
                                            const QString& line2)
{
    const int barH = m_activeTheme.lowerThirdHeight;
    const int y = size.height() - barH;

    // We draw the semi-transparent bar with a gradient accent stripe
    p.fillRect(0, y, size.width(), barH, m_activeTheme.lowerThirdBgColor);

    // Accent stripe at top of lower third
    QLinearGradient stripe(0, y, size.width(), y);
    stripe.setColorAt(0.0, m_activeTheme.accentColor);
    stripe.setColorAt(1.0, QColor(m_activeTheme.accentColor.red(),
                                   m_activeTheme.accentColor.green(),
                                   m_activeTheme.accentColor.blue(), 60));
    p.fillRect(0, y, size.width(), 4, stripe);

    const int pad = m_activeTheme.textPaddingPx;

    // Name line (larger, bold)
    QFont nameFont(m_activeTheme.fontFamily, m_activeTheme.lowerThirdFontSize + 4, QFont::Bold);
    p.setFont(nameFont);
    p.setPen(m_activeTheme.lowerThirdTextColor);
    p.drawText(QRect(pad, y + 12, size.width() - 2 * pad, barH / 2),
               Qt::AlignLeft | Qt::AlignVCenter, line1);

    // Title/role line (smaller, regular)
    if (!line2.isEmpty()) {
        QFont titleFont(m_activeTheme.fontFamily, m_activeTheme.lowerThirdFontSize, QFont::Normal);
        p.setFont(titleFont);
        p.setPen(m_activeTheme.subtitleColor);
        p.drawText(QRect(pad, y + barH / 2, size.width() - 2 * pad, barH / 2 - 8),
                   Qt::AlignLeft | Qt::AlignVCenter, line2);
    }
}

QStringList GraphicsEngineModule::wrapText(const QString& text, const QFont& font,
                                            int maxWidth) const
{
    QFontMetrics fm(font);
    QStringList result;
    // We first split on explicit newlines, then wrap long lines
    const QStringList lines = text.split('\n');
    for (const QString& line : lines) {
        if (fm.horizontalAdvance(line) <= maxWidth) {
            result.append(line);
            continue;
        }
        // Word wrap
        QStringList words = line.split(' ');
        QString current;
        for (const QString& word : words) {
            QString test = current.isEmpty() ? word : current + " " + word;
            if (fm.horizontalAdvance(test) > maxWidth && !current.isEmpty()) {
                result.append(current);
                current = word;
            } else {
                current = test;
            }
        }
        if (!current.isEmpty()) result.append(current);
    }
    return result;
}

QImage GraphicsEngineModule::renderFrame(const RenderRequest& req, const QSize& targetSize) {
    QImage frame(targetSize, QImage::Format_ARGB32_Premultiplied);
    frame.fill(Qt::transparent);

    switch (req.type) {
        case TemplateType::Lyrics:
            frame = renderLyrics(req.primaryText, req.reference,
                                  req.attribution, targetSize);
            break;
        case TemplateType::Scripture:
            frame = renderScripture(req.primaryText, req.reference,
                                     req.secondaryText, targetSize);
            break;
        case TemplateType::SermonPoint:
            frame = renderSermonPoint(req.primaryText, req.reference, targetSize);
            break;
        case TemplateType::TitleCard:
            frame = renderTitleCard(req.primaryText, req.secondaryText, targetSize);
            break;
        case TemplateType::Blank:
            frame = renderBlank(targetSize);
            break;
        case TemplateType::Countdown:
            frame = renderCountdown(req.primaryText, req.secondaryText, targetSize);
            break;
        case TemplateType::LowerThird:
            frame = renderBlank(targetSize);
            {
                QPainter p(&frame);
                p.setRenderHint(QPainter::TextAntialiasing, true);
                paintLowerThird(p, targetSize, req.lowerThirdLine1, req.lowerThirdLine2);
            }
            break;
        default:
            frame = renderBlank(targetSize);
            break;
    }

    // We apply lower third overlay if requested
    if (req.showLowerThird && req.type != TemplateType::LowerThird) {
        QPainter p(&frame);
        p.setRenderHint(QPainter::TextAntialiasing, true);
        paintLowerThird(p, targetSize, req.lowerThirdLine1, req.lowerThirdLine2);
    }

    // We apply logo if requested
    if (req.showLogo) {
        QPainter p(&frame);
        paintLogo(p, targetSize);
    }

    // We apply global opacity if specified
    if (req.opacity < 1.0f) {
        QImage faded(targetSize, QImage::Format_ARGB32_Premultiplied);
        faded.fill(Qt::black);
        QPainter p(&faded);
        p.setOpacity(req.opacity);
        p.drawImage(0, 0, frame);
        frame = faded;
    }

    emit frameRendered(frame);
    return frame;
}

QImage GraphicsEngineModule::renderLyrics(const QString& text, const QString& songTitle,
                                           const QString& ccli, const QSize& size)
{
    QImage frame(size, QImage::Format_ARGB32_Premultiplied);
    QPainter p(&frame);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    paintBackground(p, size);

    const int pad = m_activeTheme.textPaddingPx;
    const int maxW = size.width() - 2 * pad;

    // Main lyrics text — large, centered, with text shadow
    QFont lyricFont(m_activeTheme.fontFamily, m_activeTheme.titleFontSize, QFont::Bold);
    QStringList lines = wrapText(text, lyricFont, maxW);

    QFontMetrics fm(lyricFont);
    int lineH = fm.height() + m_activeTheme.lineSpacingPx;
    int totalH = lines.size() * lineH;
    int startY = (size.height() - totalH) / 2;

    // Text shadow
    p.setFont(lyricFont);
    p.setPen(QColor(0, 0, 0, 160));
    for (int i = 0; i < lines.size(); ++i) {
        QRect r(pad + 2, startY + i * lineH + 2, maxW, lineH);
        p.drawText(r, Qt::AlignHCenter | Qt::AlignVCenter, lines[i]);
    }

    // Main text
    p.setPen(m_activeTheme.textColor);
    for (int i = 0; i < lines.size(); ++i) {
        QRect r(pad, startY + i * lineH, maxW, lineH);
        p.drawText(r, Qt::AlignHCenter | Qt::AlignVCenter, lines[i]);
    }

    // Song title at bottom-left
    if (!songTitle.isEmpty()) {
        QFont refFont(m_activeTheme.fontFamily, m_activeTheme.subtitleFontSize);
        p.setFont(refFont);
        p.setPen(m_activeTheme.subtitleColor);
        p.drawText(QRect(pad, size.height() - 60, maxW / 2, 24),
                   Qt::AlignLeft | Qt::AlignVCenter, songTitle);
    }

    // CCLI at bottom-right
    if (!ccli.isEmpty()) {
        QFont ccliFont(m_activeTheme.fontFamily, 14);
        p.setFont(ccliFont);
        p.setPen(QColor(m_activeTheme.subtitleColor.red(),
                         m_activeTheme.subtitleColor.green(),
                         m_activeTheme.subtitleColor.blue(), 150));
        p.drawText(QRect(size.width() / 2, size.height() - 40, maxW / 2, 20),
                   Qt::AlignRight | Qt::AlignVCenter, ccli);
    }

    return frame;
}

QImage GraphicsEngineModule::renderScripture(const QString& text, const QString& reference,
                                              const QString& translation, const QSize& size)
{
    QImage frame(size, QImage::Format_ARGB32_Premultiplied);
    QPainter p(&frame);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    paintBackground(p, size);

    const int pad = m_activeTheme.textPaddingPx;
    const int maxW = size.width() - 2 * pad;

    // Scripture text — slightly smaller than lyrics, italic
    QFont scriptFont(m_activeTheme.fontFamily, m_activeTheme.bodyFontSize);
    scriptFont.setItalic(true);
    QStringList lines = wrapText(text, scriptFont, maxW);

    QFontMetrics fm(scriptFont);
    int lineH = fm.height() + m_activeTheme.lineSpacingPx;
    int totalH = lines.size() * lineH;
    int startY = (size.height() - totalH) / 2 - 20;

    // Text shadow
    p.setFont(scriptFont);
    p.setPen(QColor(0, 0, 0, 160));
    for (int i = 0; i < lines.size(); ++i) {
        QRect r(pad + 2, startY + i * lineH + 2, maxW, lineH);
        p.drawText(r, Qt::AlignHCenter | Qt::AlignVCenter, lines[i]);
    }

    // Main text
    p.setPen(m_activeTheme.textColor);
    for (int i = 0; i < lines.size(); ++i) {
        QRect r(pad, startY + i * lineH, maxW, lineH);
        p.drawText(r, Qt::AlignHCenter | Qt::AlignVCenter, lines[i]);
    }

    // Reference line below text (e.g., "John 3:16 NIV")
    QString refLine = reference;
    if (!translation.isEmpty()) refLine += " " + translation;

    QFont refFont(m_activeTheme.fontFamily, m_activeTheme.subtitleFontSize, QFont::Bold);
    p.setFont(refFont);
    p.setPen(m_activeTheme.accentColor);
    int refY = startY + totalH + 20;
    p.drawText(QRect(pad, refY, maxW, 40), Qt::AlignHCenter | Qt::AlignVCenter, refLine);

    return frame;
}

QImage GraphicsEngineModule::renderScriptureDual(const QString& text1, const QString& ref1,
                                                  const QString& trans1,
                                                  const QString& text2, const QString& ref2,
                                                  const QString& trans2, const QSize& size)
{
    QImage frame(size, QImage::Format_ARGB32_Premultiplied);
    QPainter p(&frame);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    paintBackground(p, size);

    const int pad = m_activeTheme.textPaddingPx;
    const int halfW = size.width() / 2;
    const int maxW = halfW - pad * 2;

    // Divider line
    p.setPen(QColor(m_activeTheme.accentColor.red(),
                     m_activeTheme.accentColor.green(),
                     m_activeTheme.accentColor.blue(), 80));
    p.drawLine(halfW, pad * 2, halfW, size.height() - pad * 2);

    // We render each translation in its half
    auto renderHalf = [&](const QString& text, const QString& ref,
                          const QString& trans, int xOffset) {
        QFont scriptFont(m_activeTheme.fontFamily, m_activeTheme.bodyFontSize - 4);
        scriptFont.setItalic(true);
        QStringList lines = wrapText(text, scriptFont, maxW);

        QFontMetrics fm(scriptFont);
        int lineH = fm.height() + m_activeTheme.lineSpacingPx;
        int totalH = lines.size() * lineH;
        int startY = (size.height() - totalH) / 2 - 20;

        p.setFont(scriptFont);
        p.setPen(m_activeTheme.textColor);
        for (int i = 0; i < lines.size(); ++i) {
            QRect r(xOffset + pad, startY + i * lineH, maxW, lineH);
            p.drawText(r, Qt::AlignHCenter | Qt::AlignVCenter, lines[i]);
        }

        // Reference
        QString refLine = ref;
        if (!trans.isEmpty()) refLine += " " + trans;
        QFont refFont(m_activeTheme.fontFamily, m_activeTheme.subtitleFontSize - 2, QFont::Bold);
        p.setFont(refFont);
        p.setPen(m_activeTheme.accentColor);
        p.drawText(QRect(xOffset + pad, startY + totalH + 12, maxW, 30),
                   Qt::AlignHCenter | Qt::AlignVCenter, refLine);
    };

    renderHalf(text1, ref1, trans1, 0);
    renderHalf(text2, ref2, trans2, halfW);

    return frame;
}

QImage GraphicsEngineModule::renderSermonPoint(const QString& point, const QString& number,
                                                const QSize& size)
{
    QImage frame(size, QImage::Format_ARGB32_Premultiplied);
    QPainter p(&frame);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    paintBackground(p, size);

    const int pad = m_activeTheme.textPaddingPx;
    const int maxW = size.width() - 2 * pad;

    // Number/bullet in accent color (large)
    if (!number.isEmpty()) {
        QFont numFont(m_activeTheme.fontFamily, m_activeTheme.titleFontSize + 8, QFont::Bold);
        p.setFont(numFont);
        p.setPen(m_activeTheme.accentColor);
        p.drawText(QRect(pad, size.height() / 2 - 80, maxW, 60),
                   Qt::AlignLeft | Qt::AlignVCenter, number);
    }

    // Point text
    QFont pointFont(m_activeTheme.fontFamily, m_activeTheme.bodyFontSize, QFont::Bold);
    QStringList lines = wrapText(point, pointFont, maxW);

    QFontMetrics fm(pointFont);
    int lineH = fm.height() + m_activeTheme.lineSpacingPx;
    int startY = number.isEmpty() ? (size.height() - lines.size() * lineH) / 2
                                  : size.height() / 2;

    p.setFont(pointFont);
    p.setPen(m_activeTheme.textColor);
    for (int i = 0; i < lines.size(); ++i) {
        QRect r(pad, startY + i * lineH, maxW, lineH);
        p.drawText(r, Qt::AlignLeft | Qt::AlignVCenter, lines[i]);
    }

    return frame;
}

QImage GraphicsEngineModule::renderLowerThird(const QString& name, const QString& title,
                                               const QSize& size)
{
    QImage frame(size, QImage::Format_ARGB32_Premultiplied);
    frame.fill(Qt::transparent);
    QPainter p(&frame);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    paintLowerThird(p, size, name, title);
    return frame;
}

QImage GraphicsEngineModule::renderTitleCard(const QString& title, const QString& subtitle,
                                              const QSize& size)
{
    QImage frame(size, QImage::Format_ARGB32_Premultiplied);
    QPainter p(&frame);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    paintBackground(p, size);
    paintLogo(p, size);

    const int pad = m_activeTheme.textPaddingPx;
    const int maxW = size.width() - 2 * pad;

    // Title — large, centered
    QFont titleFont(m_activeTheme.fontFamily, m_activeTheme.titleFontSize + 4, QFont::Bold);
    p.setFont(titleFont);

    // Shadow
    p.setPen(QColor(0, 0, 0, 180));
    p.drawText(QRect(pad + 3, size.height() / 2 - 60 + 3, maxW, 80),
               Qt::AlignHCenter | Qt::AlignVCenter, title);

    p.setPen(m_activeTheme.textColor);
    p.drawText(QRect(pad, size.height() / 2 - 60, maxW, 80),
               Qt::AlignHCenter | Qt::AlignVCenter, title);

    // Accent line
    int lineY = size.height() / 2 + 20;
    int lineW = qMin(400, size.width() / 3);
    QLinearGradient lineGrad(size.width() / 2 - lineW / 2, lineY,
                             size.width() / 2 + lineW / 2, lineY);
    lineGrad.setColorAt(0.0, Qt::transparent);
    lineGrad.setColorAt(0.3, m_activeTheme.accentColor);
    lineGrad.setColorAt(0.7, m_activeTheme.accentColor);
    lineGrad.setColorAt(1.0, Qt::transparent);
    p.setPen(Qt::NoPen);
    p.setBrush(lineGrad);
    p.drawRect(size.width() / 2 - lineW / 2, lineY, lineW, 3);

    // Subtitle
    if (!subtitle.isEmpty()) {
        QFont subFont(m_activeTheme.fontFamily, m_activeTheme.subtitleFontSize);
        p.setFont(subFont);
        p.setPen(m_activeTheme.subtitleColor);
        p.drawText(QRect(pad, lineY + 16, maxW, 40),
                   Qt::AlignHCenter | Qt::AlignVCenter, subtitle);
    }

    return frame;
}

QImage GraphicsEngineModule::renderBlank(const QSize& size) {
    QImage frame(size, QImage::Format_ARGB32_Premultiplied);
    frame.fill(Qt::black);
    return frame;
}

QImage GraphicsEngineModule::renderCountdown(const QString& timeStr, const QString& label,
                                              const QSize& size)
{
    QImage frame(size, QImage::Format_ARGB32_Premultiplied);
    QPainter p(&frame);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    paintBackground(p, size);

    const int maxW = size.width() - 2 * m_activeTheme.textPaddingPx;

    // Large countdown digits
    QFont countFont(m_activeTheme.fontFamily, m_activeTheme.titleFontSize * 2, QFont::Bold);
    p.setFont(countFont);

    // Shadow
    p.setPen(QColor(0, 0, 0, 180));
    p.drawText(QRect(m_activeTheme.textPaddingPx + 3, size.height() / 2 - 80 + 3, maxW, 120),
               Qt::AlignHCenter | Qt::AlignVCenter, timeStr);

    p.setPen(m_activeTheme.textColor);
    p.drawText(QRect(m_activeTheme.textPaddingPx, size.height() / 2 - 80, maxW, 120),
               Qt::AlignHCenter | Qt::AlignVCenter, timeStr);

    // Label below
    if (!label.isEmpty()) {
        QFont lblFont(m_activeTheme.fontFamily, m_activeTheme.subtitleFontSize);
        p.setFont(lblFont);
        p.setPen(m_activeTheme.subtitleColor);
        p.drawText(QRect(m_activeTheme.textPaddingPx, size.height() / 2 + 50, maxW, 40),
                   Qt::AlignHCenter | Qt::AlignVCenter, label);
    }

    return frame;
}

} // namespace M1

// ─── Plugin C ABI exports ────────────────────────────────────────────────────
static Mcaster1PluginInfo s_graphicsInfo = {
    MCASTER1_PLUGIN_API_VERSION,
    "com.mcaster1.church.graphics",
    "Graphics Engine",
    "1.0.0",
    "church",
    "module",
    "Mcaster1",
    "Template and overlay rendering backend — themes, text layout, lower thirds, multi-resolution output"
};

extern "C" {
MCASTER1_PLUGIN_API Mcaster1PluginInfo* mcaster1_graphics_plugin_info() { return &s_graphicsInfo; }
MCASTER1_PLUGIN_API IModule* mcaster1_graphics_create_module(IModuleHost*) {
    return new M1::GraphicsEngineModule();
}
MCASTER1_PLUGIN_API void mcaster1_graphics_destroy_module(IModule* m) { delete m; }
}

#include "GraphicsEngineModule.moc"
