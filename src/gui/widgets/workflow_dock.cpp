#include "gui/widgets/workflow_dock.h"
#include "gui/support/workflow_confirmation.h"
#include "qualpf/application/workflow.h"
#include <QDialog>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <type_traits>
namespace qualpf::gui {
using namespace application;
namespace {
QString displayValue(const MetadataValue &value) {
    return std::visit(
        [](const auto &v) -> QString {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, std::string>)
                return QString::fromStdString(v);
            else if constexpr (std::is_same_v<T, bool>)
                return v ? QStringLiteral("true") : QStringLiteral("false");
            else if constexpr (std::is_same_v<T, std::int64_t>)
                return QString::number(qlonglong(v));
            else if constexpr (std::is_same_v<T, std::uint64_t>)
                return QString::number(qulonglong(v));
            else if constexpr (std::is_same_v<T, double>)
                return QString::number(v, 'g', 17);
            else if constexpr (std::is_same_v<T, MetadataArray>) {
                QStringList parts;
                for (const auto &entry : v)
                    parts << displayValue(entry);
                return "[" + parts.join(", ") + "]";
            } else if constexpr (std::is_same_v<T, std::nullptr_t>)
                return QStringLiteral("null");
            else
                return {};
        },
        value.data);
}
QString statusName(WorkflowStepStatus status) {
    switch (status) {
    case WorkflowStepStatus::not_started:
        return QObject::tr("Not started");
    case WorkflowStepStatus::running:
        return QObject::tr("Running");
    case WorkflowStepStatus::completed:
        return QObject::tr("Completed");
    case WorkflowStepStatus::skipped:
        return QObject::tr("Fixed configuration");
    case WorkflowStepStatus::failed:
        return QObject::tr("Failed");
    case WorkflowStepStatus::cancelled:
        return QObject::tr("Cancelled");
    case WorkflowStepStatus::invalidated:
        return QObject::tr("Invalidated");
    }
    return {};
}
} // namespace
WorkflowDock::WorkflowDock(Workspace &workspace, QWidget *parent)
    : QDockWidget(tr("Workflow — Current Project"), parent), workspace_(workspace) {
    setObjectName("workflowDock");
    auto *body = new QWidget(this);
    auto *layout = new QVBoxLayout(body);
    steps_ = new QTreeWidget(body);
    steps_->setObjectName("workflowSteps");
    steps_->setRootIsDecorated(false);
    steps_->setSelectionMode(QAbstractItemView::SingleSelection);
    steps_->setHeaderLabels({tr("Step"), tr("Source"), tr("Status")});
    steps_->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    layout->addWidget(steps_);
    status_ = new QLabel(body);
    status_->setObjectName("workflowRunStatus");
    status_->setWordWrap(true);
    layout->addWidget(status_);
    auto *buttons = new QHBoxLayout;
    auto *details = new QPushButton(tr("Details"), body);
    details->setObjectName("workflowDetails");
    buttons->addWidget(details);
    buttons->addStretch();
    undo_ = new QPushButton(tr("Undo Selected"), body);
    undo_->setObjectName("workflowUndo");
    buttons->addWidget(undo_);
    layout->addLayout(buttons);
    setWidget(body);
    connect(details, &QPushButton::clicked, this, &WorkflowDock::showDetails);
    connect(undo_, &QPushButton::clicked, this, &WorkflowDock::undoSelected);
    connect(steps_, &QTreeWidget::itemSelectionChanged, this, [this] {
        auto *item = steps_->currentItem();
        undo_->setEnabled(item && !workspace_.searches().running() && !workspace_.searches().closing() &&
                          bool(workspace_.prepareRollback(item->data(0, Qt::UserRole).toULongLong())));
    });
    subscription_ = workspace_.session().subscribe([this](auto) { refresh(); });
    refresh();
}
WorkflowDock::~WorkflowDock() {
    subscription_.reset();
    disconnect(steps_, nullptr, this, nullptr);
}
void WorkflowDock::refresh() {
    const auto source = workspace_.session().snapshot();
    const auto selected =
        steps_->currentItem() ? steps_->currentItem()->data(0, Qt::UserRole).toULongLong() : 0;
    steps_->clear();
    for (const auto &record : source->workflow.records) {
        auto *item = new QTreeWidgetItem(
            steps_,
            {QString::fromUtf8(workflowStepName(record.step)),
             record.manual ? tr("Manual preprocessing")
                           : tr("Automatic · %1").arg(QString::fromUtf8(workflowName(record.workflow))),
             statusName(record.status)});
        item->setData(0, Qt::UserRole, qulonglong(record.id));
        if (record.id == selected)
            steps_->setCurrentItem(item);
    }
    undo_->setEnabled(false);
    if (steps_->currentItem())
        undo_->setEnabled(
            bool(workspace_.prepareRollback(steps_->currentItem()->data(0, Qt::UserRole).toULongLong())));
    const auto activity =
        workspace_.searches().running() ? workspace_.searches().activity() : source->workflow.last_attempt;
    if (activity)
        status_->setText(tr("%1 · %2 — %3\n%4")
                             .arg(QString::fromUtf8(workflowName(activity->target.workflow)),
                                  QString::fromUtf8(workflowStepName(activity->target.step)),
                                  statusName(activity->status), QString::fromStdString(activity->detail)));
    else if (workspace_.searches().running())
        status_->setText(tr("Running — changes and undo are unavailable until the task finishes."));
    else
        status_->setText(source->workflow.records.empty()
                             ? tr("No workflow steps recorded. Import data and run a step.")
                             : tr("Undo is available for completed steps from this session."));
}
void WorkflowDock::undoSelected() {
    if (!steps_->currentItem())
        return;
    auto rollback = workspace_.prepareRollback(steps_->currentItem()->data(0, Qt::UserRole).toULongLong());
    if (!rollback) {
        status_->setText(QString::fromStdString(rollback.error().message));
        return;
    }
    if (rollback.value().affected.size() > 1 &&
        !confirmWorkflowImpact(this, rollback.value().source, rollback.value().affected,
                               tr("Undo the selected step?")))
        return;
    auto status = workspace_.commitRollback(rollback.value());
    if (!status)
        status_->setText(QString::fromStdString(status.error().message));
    else
        emit resultsChanged();
}
void WorkflowDock::showDetails() {
    if (!steps_->currentItem())
        return;
    const auto source = workspace_.session().snapshot();
    const auto id = steps_->currentItem()->data(0, Qt::UserRole).toULongLong();
    for (const auto &r : source->workflow.records)
        if (r.id == id) {
            QDialog dialog(this);
            dialog.setWindowTitle(tr("Workflow step details"));
            dialog.resize(620, 400);
            auto *layout = new QVBoxLayout(&dialog);
            auto *text = new QPlainTextEdit(&dialog);
            text->setReadOnly(true);
            QStringList dependencies;
            for (auto d : r.dependencies)
                dependencies << QString::number(d);
            QString details = tr("Step: %1\nWorkflow: %2\nStatus: %3\nRecord: %4\nDependencies: "
                                 "%5\nInput: %6\n\n")
                                  .arg(QString::fromUtf8(workflowStepName(r.step)),
                                       QString::fromUtf8(workflowName(r.workflow)), statusName(r.status))
                                  .arg(r.id)
                                  .arg(dependencies.join(", "),
                                       r.input ? QString::fromStdString(r.input->source_id) : QString());
            std::function<void(const MetadataObject &, QString)> append = [&](const auto &object,
                                                                              QString prefix) {
                for (const auto &[key, value] : object) {
                    if (auto nested = std::get_if<MetadataObject>(&value.data))
                        append(*nested, prefix + QString::fromStdString(key) + ".");
                    else
                        details += prefix + QString::fromStdString(key) + ": " + displayValue(value) + "\n";
                }
            };
            append(r.actual_parameters, {});
            details += QString::fromStdString(r.detail);
            text->setPlainText(details);
            layout->addWidget(text);
            auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
            connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
            layout->addWidget(buttons);
            dialog.exec();
            break;
        }
}
} // namespace qualpf::gui
