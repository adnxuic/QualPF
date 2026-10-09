#include "gui/dialogs/inferred_peak_dialog.h"
#include "gui/state/workspace_context.h"
#include "gui/state/preprocessing_product.h"
#include "gui/dialogs/preprocessing_dialog.h"
#include "gui/mainwindow.h"
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>

namespace qualpf::gui {
bool editInferredPeak(MainWindow &window, double canonical) {
  auto allowed = gui::peakEditEntryStatus(window.context());
  if (!allowed) {
    window.setStatusMessage(QString::fromStdString(allowed.error().reason));
    return false;
  }
  QDialog dialog(&window);
  dialog.setObjectName("inferredPeakDialog");
  dialog.setWindowTitle(QObject::tr("Add inferred peak"));
  auto *layout = new QFormLayout(&dialog);
  auto *result = new QLabel(&dialog);
  result->setObjectName("inferredMeasurement");
  result->setWordWrap(true);
  layout->addRow(result);
  auto *buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
  layout->addRow(buttons);
  PreprocessingDialog editor(dialog, &window);
  editor.request.inferred_graph = canonical;
  if (editor.preview()) {
    result->setText(QObject::tr(
        "Peak inferred from the current pattern. Apply to keep this preview."));
  } else
    result->setText(QObject::tr(
        "The peak could not be inferred. No changes were applied."));
  QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
    if (editor.commit())
      dialog.accept();
  });
  QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog,
                   &QDialog::reject);
  return dialog.exec() == QDialog::Accepted;
}
} // namespace qualpf::gui
