#include "gui/dialogs/manual_peak_dialog.h"
#include "gui/state/workspace_context.h"
#include "gui/dialogs/preprocessing_dialog.h"
#include "gui/mainwindow.h"
#include "gui/state/preprocessing_product.h"
#include <QComboBox>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>

namespace qualpf::gui {
bool editManualPeak(MainWindow &window, std::optional<std::size_t> row, std::optional<double> position, bool table) {
    auto allowed=gui::peakEditEntryStatus(window.context());
    if(!allowed) {window.setStatusMessage(QString::fromStdString(allowed.error().reason));return false;}
    auto source = window.context().searchSession().snapshot();
    if (source->editing || (row && *row >= std::size_t(source->peaks.tth.size())))
        return false;
    QDialog dialog(&window);
    dialog.setObjectName(table?"tablePeakDialog":"manualPeakDialog");
    dialog.setWindowTitle(table ? (row?QObject::tr("Modify table peak"):QObject::tr("Add table peak")) :
        (row?QObject::tr("Modify measured peak"):QObject::tr("Add measured peak")));
    auto *layout = new QFormLayout(&dialog);
    auto *help = new QLabel(
        QObject::tr("Enter measured values. No width or intensity is inferred.\n"
                    "Applying this edit invalidates the detection candidate pool and Search-Match results."),
        &dialog);
    help->setText(table ? QObject::tr("Enter a coordinate and intensity. Choose a measured width or infer only the width from neighboring peaks.") :
        QObject::tr("Enter measured values. No width or intensity is inferred. Search results are cleared only when matching inputs change."));
    help->setWordWrap(true);
    layout->addRow(help);
    QLineEdit *fields[4];
    const char *names[] = {"manualPosition", "manualIntensity", "manualFwhm", "manualArea"};
    const QString labels[] = {QObject::tr("2 theta (degrees)"), QObject::tr("Intensity"),
                              QObject::tr("Measured FWHM (degrees)"), QObject::tr("Integrated intensity")};
    for (int i = 0; i < 4; ++i) {
        fields[i] = new QLineEdit(&dialog);
        fields[i]->setObjectName(names[i]);
        layout->addRow(labels[i], fields[i]);
    }
    QComboBox *representation=nullptr;
    QCheckBox *inferWidth=nullptr;
    QLineEdit *alternateCoordinate=nullptr;
    if(table) {
        representation=new QComboBox(&dialog);representation->setObjectName("tableRepresentation");
        representation->addItems({QObject::tr("2 theta"),QObject::tr("d spacing")});
        layout->insertRow(1,QObject::tr("Coordinate representation"),representation);
        alternateCoordinate=new QLineEdit(&dialog);
        alternateCoordinate->setObjectName("tableAlternateCoordinate");
        alternateCoordinate->setReadOnly(true);
        layout->addRow(QObject::tr("Derived alternate coordinate"),alternateCoordinate);
        inferWidth=new QCheckBox(QObject::tr("Infer width from neighboring peaks"),&dialog);
        inferWidth->setObjectName("tableInferWidth");layout->addRow(inferWidth);
    }
    if (position)
        fields[0]->setText(QString::number(*position, 'g', 17));
    if (row) {
        const auto &p = source->peaks;
        fields[0]->setText(QString::number(p.tth[*row], 'g', 17));
        fields[1]->setText(QString::number(p.intensity[*row], 'g', 17));
        fields[2]->setText(QString::number(p.fwhm[*row], 'g', 17));
        if (p.integratedIntensity)
            fields[3]->setText(QString::number((*p.integratedIntensity)[*row], 'g', 17));
    }
    // The historical all-or-unknown area column cannot represent mixed rows.
    fields[3]->setEnabled(source->peaks.tth.empty() || source->peaks.integratedIntensity.has_value());
    fields[3]->setPlaceholderText(fields[3]->isEnabled()
                                      ? QObject::tr("Required when the snapshot contains areas")
                                      : QObject::tr("Unknown in this historical snapshot"));
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addRow(buttons);
    PreprocessingDialog editor(dialog, &window);
    if (source->peaks.valid)
        editor.request.wavelength = WavelengthAngstrom{source->peaks.wave};
    auto preview = [&] {
        if (alternateCoordinate) alternateCoordinate->clear();
        application::ManualPeakChange change;
        change.operation = row ? application::ManualPeakChange::Operation::modify
                               : application::ManualPeakChange::Operation::add;
        if (row)
            change.rows.push_back(*row);
        double *values[] = {&change.two_theta, &change.intensity, &change.fwhm};
        for (int i = 0; i < 3; ++i) {
            if(i==2 && inferWidth && inferWidth->isChecked()) continue;
            bool ok = false;
            *values[i] = fields[i]->text().toDouble(&ok);
            if (!ok) {
                editor.rejectPreview("Enter all measured peak values before Apply.");
                return;
            }
        }
        if (fields[3]->isEnabled() && !fields[3]->text().isEmpty()) {
            bool ok;
            change.integrated_intensity = fields[3]->text().toDouble(&ok);
            if (!ok) {
                editor.rejectPreview("Invalid integrated intensity");
                return;
            }
        }
        if(table) {
            application::TablePeakChange input;
            input.row=change;input.coordinate=change.two_theta;
            input.representation=representation->currentIndex()==0?application::PeakCoordinateRepresentation::two_theta:
                application::PeakCoordinateRepresentation::d_spacing;
            input.infer_width=inferWidth->isChecked();editor.request.table=input;
        } else editor.request.manual = change;
        if (editor.preview() && table) {
            const auto &p = editor.state()->peaks;
            const auto &coordinates = representation->currentIndex() == 0 ? p.tth : p.d;
            const auto &alternate = representation->currentIndex() == 0 ? p.d : p.tth;
            const auto at = coordinates.indexOf(change.two_theta);
            if (at >= 0) alternateCoordinate->setText(QString::number(alternate[at], 'g', 17));
        }
    };
    for (auto *field : fields)
        QObject::connect(field, &QLineEdit::textChanged, &dialog, preview);
    if(table) {
        QObject::connect(representation,&QComboBox::currentIndexChanged,&dialog,[&](int index){
            if (auto *label = qobject_cast<QLabel *>(layout->labelForField(fields[0])))
                label->setText(index == 0 ? QObject::tr("2 theta (degrees)") : QObject::tr("d spacing (angstrom)"));
            preview();
        });
        QObject::connect(inferWidth,&QCheckBox::toggled,&dialog,[&](bool infer){fields[2]->setEnabled(!infer);preview();});
    }
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        if (window.confirmWorkflowReplacement(application::WorkflowStep::peaks) && editor.commit())
            dialog.accept();
    });
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    preview();
    return dialog.exec() == QDialog::Accepted;
}
void recoverPreprocessing(MainWindow &window) {
    const auto source = window.context().searchSession().snapshot();
    if (source->editing) {
        window.setStatusMessage(QObject::tr("Close the current preprocessing edit first."));
        return;
    }
    QDialog dialog(&window);
    dialog.setObjectName("preprocessingRecoveryDialog");
    dialog.setWindowTitle(QObject::tr("Restart preprocessing"));
    auto *layout = new QFormLayout(&dialog);
    auto *label = new QLabel(
        source->pattern.original_observed
            ? QObject::tr("Restart from the stored original intensity. Applying clears background, "
                          "smoothing, peaks and Search-Match results.\n"
                          "An undefined detection can still fail. Cancel it and adjust conditions or enter "
                          "measured peaks explicitly.")
            : QObject::tr(
                  "This project has no stored original intensity. Its missing history stays unknown.\n"
                  "Close this dialog and use File > Import diffraction pattern to select the original data "
                  "file.\n"
                  "Saved selected peaks can be edited explicitly, but cannot reconstruct a detection "
                  "candidate pool."),
        &dialog);
    label->setWordWrap(true);
    layout->addRow(label);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addRow(buttons);
    PreprocessingDialog editor(dialog, &window);
    editor.request.restart_from_original = true;
    editor.preview();
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        if (editor.commit())
            dialog.accept();
    });
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    dialog.exec();
}
} // namespace qualpf::gui
