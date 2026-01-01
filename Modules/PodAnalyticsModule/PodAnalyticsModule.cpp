/// @file   PodAnalyticsModule.cpp
/// @path   Modules/PodAnalyticsModule/PodAnalyticsModule.cpp

#include "PodAnalyticsModule.h"
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QPushButton>
#include <QLabel>
#include <QGroupBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QFileDialog>
#include <QSettings>
#include <QPainter>
#include <QDateEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFile>
#include <QTextStream>
#include <QDate>
#include <cmath>

namespace {

// ─── Metric Summary Card ───────────────────────────────────────────────
class MetricCard : public QFrame {
public:
    MetricCard(const QString& title, const QString& objectName, QWidget* parent = nullptr)
        : QFrame(parent)
    {
        setObjectName(objectName);
        setFrameStyle(QFrame::StyledPanel | QFrame::Raised);
        auto* lay = new QVBoxLayout(this);
        lay->setContentsMargins(10, 8, 10, 8);
        lay->setSpacing(2);

        m_titleLabel = new QLabel(title);
        m_titleLabel->setObjectName(objectName + "Title");
        QFont tf = m_titleLabel->font();
        tf.setPixelSize(12);
        m_titleLabel->setFont(tf);
        m_titleLabel->setAlignment(Qt::AlignCenter);
        lay->addWidget(m_titleLabel);

        m_valueLabel = new QLabel("0");
        m_valueLabel->setObjectName(objectName + "Value");
        QFont vf = m_valueLabel->font();
        vf.setPixelSize(22);
        vf.setBold(true);
        m_valueLabel->setFont(vf);
        m_valueLabel->setAlignment(Qt::AlignCenter);
        lay->addWidget(m_valueLabel);
    }

    void setValue(const QString& val) { m_valueLabel->setText(val); }

private:
    QLabel* m_titleLabel;
    QLabel* m_valueLabel;
};

// ─── Downloads Bar Chart ───────────────────────────────────────────────
class DownloadsBarChart : public QWidget {
    Q_OBJECT
public:
    explicit DownloadsBarChart(QWidget* parent = nullptr) : QWidget(parent) {
        setObjectName("PodAnalyticsBarChart");
        setMinimumHeight(120);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

    void setData(const QList<M1::EpisodeStats>& stats) {
        m_stats = stats;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        const QRect r = rect().adjusted(40, 10, -10, -30);
        if (r.width() <= 0 || r.height() <= 0 || m_stats.isEmpty()) {
            p.setPen(QColor(0x88, 0x88, 0x88));
            QFont f = font();
            f.setPixelSize(13);
            p.setFont(f);
            p.drawText(rect(), Qt::AlignCenter, "No episode data");
            return;
        }

        // Find max downloads for scale
        int maxDl = 1;
        for (const auto& s : m_stats)
            maxDl = std::max(maxDl, s.downloads);

        // Draw axes
        p.setPen(QColor(0xaa, 0xaa, 0xaa));
        p.drawLine(r.left(), r.bottom(), r.right(), r.bottom());
        p.drawLine(r.left(), r.top(), r.left(), r.bottom());

        // Y-axis labels
        QFont lf = font();
        lf.setPixelSize(10);
        p.setFont(lf);
        for (int i = 0; i <= 4; ++i) {
            const int val = maxDl * i / 4;
            const int y = r.bottom() - (r.height() * i / 4);
            p.setPen(QColor(0x88, 0x88, 0x88));
            p.drawText(QRect(0, y - 8, r.left() - 4, 16), Qt::AlignRight | Qt::AlignVCenter,
                       QString::number(val));
            if (i > 0) {
                p.setPen(QColor(0x44, 0x44, 0x44));
                p.drawLine(r.left() + 1, y, r.right(), y);
            }
        }

        // Bar colors (cycle)
        static const QColor barColors[] = {
            QColor(0x0e, 0xa5, 0xe9), // sky blue
            QColor(0x22, 0xc5, 0x5e), // green
            QColor(0xf5, 0x9e, 0x0b), // amber
            QColor(0xef, 0x44, 0x44), // red
            QColor(0xa8, 0x55, 0xf7), // purple
            QColor(0x06, 0xb6, 0xd4), // cyan
        };
        static constexpr int nColors = sizeof(barColors) / sizeof(barColors[0]);

        const int n = m_stats.size();
        const int gap = 4;
        const int barWidth = std::max(8, (r.width() - gap * (n + 1)) / n);
        int x = r.left() + gap;

        for (int i = 0; i < n; ++i) {
            const int barH = static_cast<int>(r.height() * m_stats[i].downloads / static_cast<double>(maxDl));
            const QRect bar(x, r.bottom() - barH, barWidth, barH);

            // Fill
            QColor c = barColors[i % nColors];
            p.setBrush(c);
            p.setPen(Qt::NoPen);
            p.drawRoundedRect(bar, 2, 2);

            // Episode number label
            p.setPen(QColor(0xaa, 0xaa, 0xaa));
            p.drawText(QRect(x, r.bottom() + 2, barWidth, 20), Qt::AlignHCenter | Qt::AlignTop,
                       QString("E%1").arg(m_stats[i].episodeNumber));

            // Download count on bar
            if (barH > 16) {
                p.setPen(Qt::white);
                p.drawText(bar.adjusted(0, 2, 0, 0), Qt::AlignHCenter | Qt::AlignTop,
                           QString::number(m_stats[i].downloads));
            }

            x += barWidth + gap;
        }
    }

private:
    QList<M1::EpisodeStats> m_stats;
};

// ─── Add Episode Dialog ────────────────────────────────────────────────
class AddEpisodeDialog : public QDialog {
    Q_OBJECT
public:
    explicit AddEpisodeDialog(QWidget* parent = nullptr) : QDialog(parent) {
        setWindowTitle("Add Episode Stats");
        setObjectName("PodAnalyticsAddDialog");
        auto* form = new QFormLayout(this);

        m_titleEdit = new QLineEdit;
        m_titleEdit->setObjectName("PodAnalyticsAddTitle");
        m_titleEdit->setPlaceholderText("Episode title");
        form->addRow("Title:", m_titleEdit);

        m_numberSpin = new QSpinBox;
        m_numberSpin->setRange(1, 99999);
        m_numberSpin->setValue(1);
        form->addRow("Episode #:", m_numberSpin);

        m_dateEdit = new QDateEdit(QDate::currentDate());
        m_dateEdit->setCalendarPopup(true);
        form->addRow("Publish Date:", m_dateEdit);

        m_downloadsSpin = new QSpinBox;
        m_downloadsSpin->setRange(0, 99999999);
        form->addRow("Downloads:", m_downloadsSpin);

        m_listensSpin = new QSpinBox;
        m_listensSpin->setRange(0, 99999999);
        form->addRow("Listens:", m_listensSpin);

        m_subsSpin = new QSpinBox;
        m_subsSpin->setRange(0, 99999999);
        form->addRow("Subscribers:", m_subsSpin);

        auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        connect(btns, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);
        form->addRow(btns);
    }

    M1::EpisodeStats result() const {
        M1::EpisodeStats s;
        s.episodeTitle     = m_titleEdit->text();
        s.episodeNumber    = m_numberSpin->value();
        s.publishDate      = m_dateEdit->date().toString(Qt::ISODate);
        s.downloads        = m_downloadsSpin->value();
        s.listens          = m_listensSpin->value();
        s.subscribersAtTime = m_subsSpin->value();
        return s;
    }

    void setEpisodeNumber(int n) { m_numberSpin->setValue(n); }

private:
    QLineEdit* m_titleEdit;
    QSpinBox*  m_numberSpin;
    QDateEdit* m_dateEdit;
    QSpinBox*  m_downloadsSpin;
    QSpinBox*  m_listensSpin;
    QSpinBox*  m_subsSpin;
};

// ─── Main Analytics Widget ─────────────────────────────────────────────
class PodAnalyticsWidget : public QWidget {
    Q_OBJECT
public:
    explicit PodAnalyticsWidget(M1::PodAnalyticsModule* mod, QWidget* parent = nullptr)
        : QWidget(parent), m_mod(mod)
    {
        setObjectName("PodAnalyticsWidget");
        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(8, 8, 8, 8);
        root->setSpacing(6);

        // ── Summary cards row ──────────────────────────────────────────
        auto* cardsRow = new QHBoxLayout;
        cardsRow->setSpacing(6);
        m_totalDlCard   = new MetricCard("Total Downloads", "PodAnalyticsTotalDl");
        m_avgDlCard     = new MetricCard("Avg / Episode",   "PodAnalyticsAvgDl");
        m_subsCard      = new MetricCard("Subscribers",     "PodAnalyticsSubs");
        m_growthCard    = new MetricCard("Growth Trend",    "PodAnalyticsGrowth");
        cardsRow->addWidget(m_totalDlCard);
        cardsRow->addWidget(m_avgDlCard);
        cardsRow->addWidget(m_subsCard);
        cardsRow->addWidget(m_growthCard);
        root->addLayout(cardsRow);

        // ── Bar chart ──────────────────────────────────────────────────
        m_chart = new DownloadsBarChart;
        root->addWidget(m_chart, 1);

        // ── Episode stats table ────────────────────────────────────────
        m_table = new QTableWidget(0, 6);
        m_table->setObjectName("PodAnalyticsTable");
        m_table->setHorizontalHeaderLabels({"Ep#", "Title", "Date", "Downloads", "Listens", "Subscribers"});
        m_table->horizontalHeader()->setStretchLastSection(true);
        m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_table->setSelectionMode(QAbstractItemView::SingleSelection);
        m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_table->verticalHeader()->setVisible(false);
        m_table->setMinimumHeight(100);
        root->addWidget(m_table, 1);

        // ── Buttons row ────────────────────────────────────────────────
        auto* btnRow = new QHBoxLayout;
        btnRow->setSpacing(6);

        auto* addBtn = new QPushButton("+ Add Episode");
        addBtn->setObjectName("PodAnalyticsAddBtn");
        addBtn->setToolTip("Add episode performance data");
        connect(addBtn, &QPushButton::clicked, this, &PodAnalyticsWidget::onAddEpisode);
        btnRow->addWidget(addBtn);

        auto* removeBtn = new QPushButton("Remove");
        removeBtn->setObjectName("PodAnalyticsRemoveBtn");
        removeBtn->setToolTip("Remove selected episode");
        connect(removeBtn, &QPushButton::clicked, this, &PodAnalyticsWidget::onRemoveEpisode);
        btnRow->addWidget(removeBtn);

        btnRow->addStretch();

        auto* exportBtn = new QPushButton("Export CSV...");
        exportBtn->setObjectName("PodAnalyticsExportBtn");
        exportBtn->setToolTip("Export all episode data to CSV file");
        connect(exportBtn, &QPushButton::clicked, this, &PodAnalyticsWidget::onExportCsv);
        btnRow->addWidget(exportBtn);

        root->addLayout(btnRow);

        // ── v2 stub label ──────────────────────────────────────────────
        auto* stubLabel = new QLabel("API integration available in v2");
        stubLabel->setObjectName("PodAnalyticsStubLabel");
        QFont sf = stubLabel->font();
        sf.setPixelSize(11);
        sf.setItalic(true);
        stubLabel->setFont(sf);
        stubLabel->setAlignment(Qt::AlignRight);
        root->addWidget(stubLabel);

        connect(mod, &M1::PodAnalyticsModule::statsChanged, this, &PodAnalyticsWidget::refresh);
        refresh();
    }

private slots:
    void refresh() {
        const auto stats = m_mod->allStats();

        // Update summary cards
        m_totalDlCard->setValue(QString::number(m_mod->totalDownloads()));
        m_avgDlCard->setValue(QString::number(m_mod->averageDownloads()));
        m_subsCard->setValue(QString::number(m_mod->subscriberCount()));

        // Growth trend: compare last two episodes' subscriber counts
        if (stats.size() >= 2) {
            const int prev = stats[stats.size() - 2].subscribersAtTime;
            const int curr = stats[stats.size() - 1].subscribersAtTime;
            if (prev > 0) {
                const double pct = 100.0 * (curr - prev) / static_cast<double>(prev);
                const QString sign = pct >= 0 ? "+" : "";
                m_growthCard->setValue(sign + QString::number(pct, 'f', 1) + "%");
            } else {
                m_growthCard->setValue(curr > 0 ? "+100%" : "0%");
            }
        } else {
            m_growthCard->setValue("--");
        }

        // Update table
        m_table->setRowCount(stats.size());
        for (int i = 0; i < stats.size(); ++i) {
            const auto& s = stats[i];
            m_table->setItem(i, 0, new QTableWidgetItem(QString::number(s.episodeNumber)));
            m_table->setItem(i, 1, new QTableWidgetItem(s.episodeTitle));
            m_table->setItem(i, 2, new QTableWidgetItem(s.publishDate));
            m_table->setItem(i, 3, new QTableWidgetItem(QString::number(s.downloads)));
            m_table->setItem(i, 4, new QTableWidgetItem(QString::number(s.listens)));
            m_table->setItem(i, 5, new QTableWidgetItem(QString::number(s.subscribersAtTime)));
        }

        // Update chart
        m_chart->setData(stats);
    }

    void onAddEpisode() {
        AddEpisodeDialog dlg(this);
        const auto stats = m_mod->allStats();
        if (!stats.isEmpty())
            dlg.setEpisodeNumber(stats.last().episodeNumber + 1);
        if (dlg.exec() == QDialog::Accepted) {
            m_mod->addEpisodeStats(dlg.result());
        }
    }

    void onRemoveEpisode() {
        const int row = m_table->currentRow();
        if (row >= 0)
            m_mod->removeEpisodeStats(row);
    }

    void onExportCsv() {
        const QString path = QFileDialog::getSaveFileName(this, "Export CSV",
            QString(), "CSV Files (*.csv)");
        if (path.isEmpty()) return;
        if (!m_mod->exportCsv(path))
            emit m_mod->moduleError("Failed to export CSV to " + path);
    }

private:
    M1::PodAnalyticsModule* m_mod;
    MetricCard*       m_totalDlCard;
    MetricCard*       m_avgDlCard;
    MetricCard*       m_subsCard;
    MetricCard*       m_growthCard;
    DownloadsBarChart* m_chart;
    QTableWidget*     m_table;
};

} // anonymous namespace

#include "PodAnalyticsModule.moc"

namespace M1 {

PodAnalyticsModule::PodAnalyticsModule(QObject* parent) : IModule(parent) {}

void PodAnalyticsModule::initialize() {}
void PodAnalyticsModule::shutdown() {}

QWidget* PodAnalyticsModule::createWidget(QWidget* parent) {
    return new PodAnalyticsWidget(this, parent);
}

void PodAnalyticsModule::addEpisodeStats(const EpisodeStats& stats) {
    m_stats.append(stats);
    emit statsChanged();
}

void PodAnalyticsModule::removeEpisodeStats(int index) {
    if (index >= 0 && index < m_stats.size()) {
        m_stats.removeAt(index);
        emit statsChanged();
    }
}

int PodAnalyticsModule::totalDownloads() const {
    int total = 0;
    for (const auto& s : m_stats)
        total += s.downloads;
    return total;
}

int PodAnalyticsModule::averageDownloads() const {
    if (m_stats.isEmpty()) return 0;
    return totalDownloads() / m_stats.size();
}

int PodAnalyticsModule::subscriberCount() const {
    if (m_stats.isEmpty()) return 0;
    // Return the most recent subscriber count
    return m_stats.last().subscribersAtTime;
}

bool PodAnalyticsModule::exportCsv(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QTextStream out(&file);
    out << "Episode Number,Title,Publish Date,Downloads,Listens,Subscribers\n";
    for (const auto& s : m_stats) {
        // Escape commas/quotes in title
        QString title = s.episodeTitle;
        if (title.contains(',') || title.contains('"')) {
            title.replace('"', "\"\"");
            title = '"' + title + '"';
        }
        out << s.episodeNumber << ','
            << title << ','
            << s.publishDate << ','
            << s.downloads << ','
            << s.listens << ','
            << s.subscribersAtTime << '\n';
    }
    return true;
}

void PodAnalyticsModule::saveState(QSettings& s) {
    s.setValue("episodeCount", m_stats.size());
    for (int i = 0; i < m_stats.size(); ++i) {
        const QString prefix = QString("ep/%1/").arg(i);
        s.setValue(prefix + "title",       m_stats[i].episodeTitle);
        s.setValue(prefix + "number",      m_stats[i].episodeNumber);
        s.setValue(prefix + "publishDate", m_stats[i].publishDate);
        s.setValue(prefix + "downloads",   m_stats[i].downloads);
        s.setValue(prefix + "listens",     m_stats[i].listens);
        s.setValue(prefix + "subscribers", m_stats[i].subscribersAtTime);
    }
}

void PodAnalyticsModule::loadState(QSettings& s) {
    m_stats.clear();
    const int count = s.value("episodeCount", 0).toInt();
    for (int i = 0; i < count; ++i) {
        const QString prefix = QString("ep/%1/").arg(i);
        EpisodeStats ep;
        ep.episodeTitle     = s.value(prefix + "title").toString();
        ep.episodeNumber    = s.value(prefix + "number", i + 1).toInt();
        ep.publishDate      = s.value(prefix + "publishDate").toString();
        ep.downloads        = s.value(prefix + "downloads", 0).toInt();
        ep.listens          = s.value(prefix + "listens", 0).toInt();
        ep.subscribersAtTime = s.value(prefix + "subscribers", 0).toInt();
        m_stats.append(ep);
    }
    emit statsChanged();
}

} // namespace M1
