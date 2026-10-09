#ifndef MANAGEDATABASESDIALOG_H
#define MANAGEDATABASESDIALOG_H

#include <QDialog>
#include <QList>
#include "gui/adapters/reference_database.h"

namespace Ui {
class ManageDatabasesDialog;
}

using DatabaseEntry = qualpf::gui::DatabaseEntry;

class ManageDatabasesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ManageDatabasesDialog(QWidget *parent = nullptr);
    ~ManageDatabasesDialog();

    void setDatabases(const QList<DatabaseEntry> &databases);
    QList<DatabaseEntry> databases() const;
    int activeDatabase() const;


private slots:
    void onCheckboxChanged(int row, bool checked);
    void onSelectionChanged();
    void onRenameClicked();
    void onAddClicked();
    void onCreateClicked();
    void onDeleteClicked();

private:
    Ui::ManageDatabasesDialog *ui;

    void rebuildTable();
    void registerExistingSqDatabase(const QString &sqFile, const QString &name);
    int currentSelectedRow() const;

    QList<DatabaseEntry> mDatabases;
    int mActiveIndex;
};

#endif // MANAGEDATABASESDIALOG_H
