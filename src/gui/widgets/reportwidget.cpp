#include "qualpf/application/domain_inputs.h"
#include "gui/state/workspace_context.h"
#include "gui/widgets/reportwidget.h"
#include "ui_reportwidget.h"
#include "config.h"
#include "gui/widgets/dbresultswidget.h"   // cardColor
#include "gui/dialogs/searchoptionsdialog.h"

#include <QPainter>
#include <QBuffer>
#include <QDateTime>
#include <QFileInfo>
#include <QPrinter>



// Tries to identify the X-ray anode from wavelength Ka1 (Å).
static QString anodeFromWavelength(float wave)
{
    struct { const char *name; float lambda; } table[] = {
        { "Cr", 2.28975f }, { "Fe", 1.93735f }, { "Co", 1.79026f },
        { "Cu", float(qualpf::application::defaultWavelength()) }, { "Mo", 0.71073f }, { "Ag", 0.55941f }
    };
    for (const auto &e : table)
        if (qAbs(wave - e.lambda) < 0.005f)
            return QString::fromLatin1(e.name);
    return {};
}

ReportWidget::ReportWidget(QWidget *parent)
    : QWidget(parent), ui(new Ui::ReportWidget)
{
    ui->setupUi(this);
}

ReportWidget::~ReportWidget()
{
    delete ui;
}

// ── Public API ────────────────────────────────────────────────────────────────

void ReportWidget::updateReport(const ExperimentalPeaks &ep,
                                const QVector<CardType> &resultCards,
                                int maxCards)
{
    m_ep       = ep;
    m_cards    = resultCards;
    m_maxCards = maxCards;
    generateHtml();
}

void ReportWidget::setPeakSearchSettings(const qualpf::gui::PeakSearchPresentationOptions &s)
{
    m_pkSettings = s;
}

void ReportWidget::setRestraintsInfo(bool hasRestraints, const QStringList &active)
{
    m_hasRestraints    = hasRestraints;
    m_activeRestraints = active;
}

void ReportWidget::updateQuantitative(const qualpf::application::QuantificationResult &result)
{
    m_quantification = result;
    generateHtml();
}

void ReportWidget::clearQuantitative()
{
    m_quantification = {};
    generateHtml();
}

void ReportWidget::clear()
{
    m_ep       = ExperimentalPeaks();
    m_cards.clear();
    m_maxCards = 100;
    m_quantification = {};
    m_pkSettings = {};
    m_hasRestraints = false;
    m_activeRestraints.clear();
    generateHtml();
}

// ── Off-screen pie chart rendering ───────────────────────────────────────────

QPixmap ReportWidget::renderPieChart(const QVector<QPair<QColor, double>> &slices, int size)
{
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::white);

    if (slices.isEmpty())
        return pixmap;

    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);

    const int margin = 8;
    const QRectF rect(margin, margin, size - 2 * margin, size - 2 * margin);

    double startAngle = 90.0; // 12 o'clock
    for (const auto &slice : slices) {
        const double span = slice.second / 100.0 * 360.0;
        p.setBrush(QBrush(slice.first));
        p.setPen(QPen(Qt::white, 1));
        p.drawPie(rect, qRound(startAngle * 16), qRound(-span * 16));
        startAngle -= span;
    }
    return pixmap;
}

// ── HTML generation ───────────────────────────────────────────────────────────

void ReportWidget::generateHtml()
{
    QString html =
        "<html><body style='font-size:9pt'>"
        "<style>table{border-collapse:collapse} th,td{padding:3px} "
        "th{background:#e0e0e0}</style>";

    // ── Title ───────────────────────────────────────────────────────────────
    html += QString("<h2 style='margin-bottom:4px'>%1 Phase Identification Results</h2>")
                .arg(APP_DISPLAY_NAME);

    // ── Experimental data info ──────────────────────────────────────────────
    {
        const auto snapshot = qualpf::gui::workspaceContext(this).searchSession().snapshot();
        const auto &pattern = snapshot->pattern;
        const auto npoints = pattern.observed.size();
        const auto nwave = pattern.radiation.lines.size();
        const int radtype = snapshot->projectMetadata["radtype"].toInt();
        const double tmin = pattern.calculation_range ? pattern.calculation_range->minimum.value : 0;
        const double tmax = pattern.calculation_range ? pattern.calculation_range->maximum.value : 0;
        const double tstep = npoints > 1 ? (pattern.two_theta.back().value - pattern.two_theta.front().value) / (npoints - 1) : 0;
        QVector<double> wave;
        for (const auto &line : pattern.radiation.lines) wave.append(line.wavelength.value);
        const auto back_sub = pattern.processing.background_subtracted;
        const auto alpha2_sub = pattern.processing.kalpha2_subtracted;
        const QString fullPath = QString::fromStdString(pattern.source_id);
        const QFileInfo fi(fullPath);

        auto infoRow = [](const QString &label, const QString &val) {
            return QString("<tr><td style='padding-right:12px'>%1</td>"
                           "<td><b>%2</b></td></tr>").arg(label, val);
        };

        html += "<h3>Experimental Data</h3><table border='0' cellspacing='2'>";
        html += infoRow("Filename",   fi.fileName().isEmpty() ? tr("-") : fi.fileName().toHtmlEscaped());
        html += infoRow("File path",  fi.absolutePath().isEmpty() ? tr("-") : fi.absolutePath().toHtmlEscaped());

        const auto file = qualpf::gui::workspaceContext(this).platform().fileInformation(pattern.source_id);
        if (file.exists)
            html += infoRow("Data collected",
                            file.lastModifiedMilliseconds
                                ? QDateTime::fromMSecsSinceEpoch(*file.lastModifiedMilliseconds).toString("dd/MM/yyyy HH:mm:ss")
                                : QString{});

        if (npoints > 0) {
            html += infoRow("Data range",
                            QString("%1&deg; to %2&deg;")
                                .arg(tmin, 0, 'f', 3).arg(tmax, 0, 'f', 3));
            html += infoRow("Number of points", QString::number(npoints));
            html += infoRow("Step size",        QString::number(tstep, 'f', 4));
        }

        html += infoRow("Alpha2 subtracted", alpha2_sub ? (*alpha2_sub ? tr("Yes") : tr("No")) : tr("Unknown"));
        html += infoRow("Background subtr.", back_sub ? (*back_sub ? tr("Yes") : tr("No")) : tr("Unknown"));

        if (nwave > 0) {
            // radtype: 0=X-ray, 1=Neutron, 2=Electron
            QString radLabel;
            if (radtype == 1)       radLabel = tr("Neutron");
            else if (radtype == 2)  radLabel = tr("Electron");
            else {
                const QString anode = anodeFromWavelength(wave[0]);
                radLabel = anode.isEmpty()
                    ? QString()
                    : anode + ": ";
            }
            QString waveStr;
            for (int i = 0; i < nwave; ++i) {
                if (!waveStr.isEmpty()) waveStr += ", ";
                if (radtype == 0) {
                    const QString anode = anodeFromWavelength(wave[i]);
                    if (!anode.isEmpty()) waveStr += anode + ": ";
                }
                waveStr += QString("%1 &Aring;").arg(wave[i], 0, 'f', 6);
            }
            if (radtype == 1 || radtype == 2)
                waveStr = radLabel + waveStr;
            html += infoRow("Radiation Wavelength", waveStr);
        }

        html += "</table>";
    }

    // ── Experimental peaks ──────────────────────────────────────────────────
    html += "<h3>Experimental Peaks</h3>";
    if (m_ep.tth.isEmpty()) {
        html += "<p><i>No experimental peaks loaded.</i></p>";
    } else {
        html += "<table border='1'>"
                "<tr><th>2&#x3B8; (&deg;)</th><th>Intensity</th></tr>";
        for (int i = 0; i < m_ep.tth.size(); ++i)
            html += QString("<tr><td align='right'>%1</td>"
                            "<td align='right'>%2</td></tr>")
                        .arg(m_ep.tth[i],       0, 'f', 4)
                        .arg(m_ep.intensity[i], 0, 'f', 1);
        html += "</table>";
    }

    // ── Peak Search Options ─────────────────────────────────────────────────
    html += "<h3>Peak Search Options</h3>";
    {
        html += "<table border='0'>";
        auto pkRow = [](const QString &label, const QString &val) {
            return QString("<tr><td style='padding-right:12px'>%1</td>"
                           "<td><b>%2</b></td></tr>").arg(label, val);
        };
        html += pkRow("Search range",
                      QString("%1&deg; to %2&deg;")
                          .arg(m_pkSettings.minSearch, 0, 'f', 2)
                          .arg(m_pkSettings.maxSearch, 0, 'f', 2));
        html += pkRow("Intensity threshold",
                      QString::number(m_pkSettings.threshold, 'f', 2) + " %");
        html += pkRow("Sensitivity",
                      QString::number(m_pkSettings.sensitivity));
        html += "</table>";
    }

    // ── Search options ──────────────────────────────────────────────────────
    html += "<h3>Search Options</h3>"
            "<table border='0'>";
    auto optRow = [](const QString &label, const QString &val) {
        return QString("<tr><td>%1</td><td><b>%2</b></td></tr>").arg(label, val);
    };
    html += optRow("Min FOM",          QString::number(SearchOptionsDialog::savedMinFom(qualpf::gui::workspaceContext(this).workspace().session().snapshot(), qualpf::gui::workspaceContext(this).settings()), 'f', 3));
    html += optRow("Weight 2&#x3B8;",  QString::number(SearchOptionsDialog::savedWeight2thetaD(qualpf::gui::workspaceContext(this).workspace().session().snapshot(), qualpf::gui::workspaceContext(this).settings()), 'f', 2));
    html += optRow("Weight Intensity", QString::number(SearchOptionsDialog::savedWeightIntensity(qualpf::gui::workspaceContext(this).workspace().session().snapshot(), qualpf::gui::workspaceContext(this).settings()), 'f', 2));
    html += optRow("Weight Phases",    QString::number(SearchOptionsDialog::savedWeightPhases(qualpf::gui::workspaceContext(this).workspace().session().snapshot(), qualpf::gui::workspaceContext(this).settings()), 'f', 2));
    html += optRow("Delta 2&#x3B8;",   QString::number(SearchOptionsDialog::savedDelta2theta(qualpf::gui::workspaceContext(this).workspace().session().snapshot(), qualpf::gui::workspaceContext(this).settings()), 'f', 4));
    html += optRow("Residual Search",  SearchOptionsDialog::savedResidualSearching(qualpf::gui::workspaceContext(this).workspace().session().snapshot(), qualpf::gui::workspaceContext(this).settings()) ? "Yes" : "No");
    html += optRow("Max Entries",      QString::number(SearchOptionsDialog::savedMaxEntries(qualpf::gui::workspaceContext(this).workspace().session().snapshot(), qualpf::gui::workspaceContext(this).settings())));
    if (m_hasRestraints)
        html += optRow("Restraints", m_activeRestraints.join(", ").toHtmlEscaped());
    else
        html += optRow("Restraints", "None");
    html += "</table>";

    // ── Result cards ────────────────────────────────────────────────────────
    const int shown = qMin(m_cards.size(), m_maxCards);
    html += QString("<h3>Search Results &ndash; %1 shown of %2</h3>")
                .arg(shown).arg(m_cards.size());
    if (m_cards.isEmpty()) {
        html += "<p><i>No results.</i></p>";
    } else {
        html += "<table border='1'>"
                "<tr><th>#</th><th>ID</th>"
                "<th>Chemical Name</th><th>Formula</th>"
                "<th>FOM</th><th>Scale</th></tr>";
        for (int i = 0; i < shown; ++i) {
            const CardType &card = m_cards[i];
            QString name = card.getChemicalName();
            if (!card.getMineralName().isEmpty())
                name += " [" + card.getMineralName() + "]";
            const QString bg = cardColor(card.getId(), this).name();
            html += QString("<tr>"
                            "<td align='right'>%1</td>"
                            "<td bgcolor='%2'>%3</td>"
                            "<td>%4</td><td>%5</td>"
                            "<td align='right'>%6</td>"
                            "<td align='right'>%7</td>"
                            "</tr>")
                        .arg(QString::number(i + 1), bg, card.getId().toHtmlEscaped(),
                             name.toHtmlEscaped(), card.getChemicalFormula().toHtmlEscaped(),
                             card.isFomCalculated() ? QString::number(card.getFom(),   'f', 4) : "-",
                             card.isFomCalculated() ? QString::number(card.getScale(), 'f', 4) : "-");
        }
        html += "</table>";
    }

    // Quantification is the same immutable Application result shown by the panel.
    const auto &q = m_quantification;
    if (!q.phases.empty()) {
        const bool pfsm = q.method == qualpf::application::QuantificationMethod::pfsm_rir;
        const auto escaped = [](const std::string &v) { return QString::fromStdString(v).toHtmlEscaped(); };
        const auto number = [](std::optional<double> v, int precision = 2) {
            return v ? QString::number(*v, 'f', precision) : QStringLiteral("-");
        };
        html += pfsm ? "<h3>Quantitative analysis · PFSM/RIR</h3>" : "<h3>Quantitative Analysis · CSM/RIR</h3>";
        html += QString("<p>%1 · revision %2 · generation %3 · database %4</p>")
            .arg(q.available() ? "Available" : "Unavailable").arg(q.revision).arg(q.generation).arg(q.database_version);
        if (q.rwp_net) html += "<p>Joint Rwp (net): " + QString::number(*q.rwp_net, 'g', 17) + "</p>";
        for (const auto &issue : q.issues)
            html += "<p>" + escaped(issue.phase_id) + " " + escaped(issue.reason) + ": " + escaped(issue.message) + "</p>";
        for (const auto &note : q.qualifications) html += "<p>" + escaped(note) + "</p>";
        if (q.available()) {
            QVector<QPair<QColor, double>> slices;
            for (const auto &phase : q.phases) slices.append({cardColor(QString::fromStdString(phase.id), this), *phase.weight_percent});
            QByteArray ba; QBuffer buf(&ba); buf.open(QIODevice::WriteOnly);
            renderPieChart(slices, 280).save(&buf, "PNG");
            html += "<img src='data:image/png;base64," + QString::fromLatin1(ba.toBase64()) + "' width='280' height='280'/><br>";
        }
        html += "<table border='1'><tr><th>ID</th><th>Name</th><th>wt.%</th>";
        if (pfsm) html += "<th>Scale</th><th>Shift °2θ</th>";
        html += "<th>Effective RIR</th><th>RIR source</th><th>Reference RIR</th></tr>";
        for (const auto &phase : q.phases) {
            html += "<tr><td>" + escaped(phase.id) + "</td><td>" + escaped(phase.name) +
                "</td><td>" + number(phase.weight_percent) + "</td>";
            if (pfsm) html += "<td>" + number(phase.scale, 5) + "</td><td>" + number(phase.shift_degrees, 5) + "</td>";
            html += "<td>" + number(phase.rir) + "</td><td>" + escaped(phase.rir_source) +
                (phase.rir_revised ? " (Cu Kα revision)" : "") + "</td><td>" + escaped(phase.original_rir) + "</td></tr>";
        }
        html += "</table>";
    }

    html += "</body></html>";
    ui->textBrowser->setHtml(html);
}

void ReportWidget::print(QPrinter *printer)
{
    ui->textBrowser->print(printer);
}
