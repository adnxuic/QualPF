#include "gui/dialogs/managedatabasesdialog.h"
#include "ui_managedatabasesdialog.h"
#include "gui/dialogs/createdatabasedialog.h"
#include "gui/database/databasebuilder.h"
#include "gui/mainwindow.h"
#include "gui/reference/libcomune.h"

#include <QCheckBox>
#include <QDir>
#include <QInputDialog>
#include <QFileDialog>
#include <QHeaderView>
#include <QMessageBox>

#include <QTableWidgetItem>

ManageDatabasesDialog::ManageDatabasesDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ManageDatabasesDialog)
    , mActiveIndex(-1)
{
    ui->setupUi(this);

    ui->tableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    ui->tableWidget->verticalHeader()->setVisible(false);

    connect(ui->quitButton,   &QPushButton::clicked, this, &QDialog::accept);
    connect(ui->renameButton, &QPushButton::clicked, this, &ManageDatabasesDialog::onRenameClicked);
    connect(ui->addButton,    &QPushButton::clicked, this, &ManageDatabasesDialog::onAddClicked);
    connect(ui->createButton, &QPushButton::clicked, this, &ManageDatabasesDialog::onCreateClicked);
    connect(ui->deleteButton, &QPushButton::clicked, this, &ManageDatabasesDialog::onDeleteClicked);
    connect(ui->tableWidget,  &QTableWidget::itemSelectionChanged,
            this, &ManageDatabasesDialog::onSelectionChanged);
}

ManageDatabasesDialog::~ManageDatabasesDialog()
{
    delete ui;
}

void ManageDatabasesDialog::setDatabases(const QList<DatabaseEntry> &databases)
{
    mDatabases = databases;
    mActiveIndex = -1;
    for (int i = 0; i < mDatabases.size(); ++i) {
        if (mDatabases[i].inUse) {
            mActiveIndex = i;
            break;
        }
    }
    rebuildTable();
}

QList<DatabaseEntry> ManageDatabasesDialog::databases() const
{
    return mDatabases;
}

int ManageDatabasesDialog::activeDatabase() const
{
    return mActiveIndex;
}

void ManageDatabasesDialog::rebuildTable()
{
    ui->tableWidget->blockSignals(true);
    ui->tableWidget->setRowCount(mDatabases.size());

    for (int row = 0; row < mDatabases.size(); ++row) {
        const DatabaseEntry &db = mDatabases[row];

        // Column 0: Use (checkbox, centered)
        QWidget *checkWidget = new QWidget();
        QCheckBox *checkBox = new QCheckBox();
        checkBox->setChecked(row == mActiveIndex);
        QHBoxLayout *layout = new QHBoxLayout(checkWidget);
        layout->addWidget(checkBox);
        layout->setAlignment(Qt::AlignCenter);
        layout->setContentsMargins(0, 0, 0, 0);
        checkWidget->setLayout(layout);
        ui->tableWidget->setCellWidget(row, 0, checkWidget);

        connect(checkBox, &QCheckBox::toggled, this, [this, row](bool checked) {
            onCheckboxChanged(row, checked);
        });

        // Column 1: Database name
        QTableWidgetItem *nameItem = new QTableWidgetItem(db.name);
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        ui->tableWidget->setItem(row, 1, nameItem);

        // Column 2: Entries
        QTableWidgetItem *entriesItem = new QTableWidgetItem(QString::number(db.entries));
        entriesItem->setFlags(entriesItem->flags() & ~Qt::ItemIsEditable);
        entriesItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        ui->tableWidget->setItem(row, 2, entriesItem);

        // Column 3: Content type
        QTableWidgetItem *typeItem = new QTableWidgetItem(db.contentType);
        typeItem->setFlags(typeItem->flags() & ~Qt::ItemIsEditable);
        ui->tableWidget->setItem(row, 3, typeItem);

        // Column 4: Path — show the containing directory, not the full base path
        QTableWidgetItem *pathItem = new QTableWidgetItem(QFileInfo(db.path).path());
        pathItem->setFlags(pathItem->flags() & ~Qt::ItemIsEditable);
        ui->tableWidget->setItem(row, 4, pathItem);
    }

    ui->tableWidget->blockSignals(false);
}

void ManageDatabasesDialog::onCheckboxChanged(int row, bool checked)
{
    if (!checked) {
        // Prevent unchecking without selecting another
        if (mActiveIndex == row) {
            // Re-check it silently
            QWidget *w = ui->tableWidget->cellWidget(row, 0);
            if (w) {
                QCheckBox *cb = w->findChild<QCheckBox *>();
                if (cb) {
                    cb->blockSignals(true);
                    cb->setChecked(true);
                    cb->blockSignals(false);
                }
            }
        }
        return;
    }

    // Uncheck the previously active row
    if (mActiveIndex >= 0 && mActiveIndex != row) {
        QWidget *w = ui->tableWidget->cellWidget(mActiveIndex, 0);
        if (w) {
            QCheckBox *cb = w->findChild<QCheckBox *>();
            if (cb) {
                cb->blockSignals(true);
                cb->setChecked(false);
                cb->blockSignals(false);
            }
        }
        mDatabases[mActiveIndex].inUse = false;
    }

    mActiveIndex = row;
    mDatabases[row].inUse = true;
}

int ManageDatabasesDialog::currentSelectedRow() const
{
    return ui->tableWidget->currentRow();
}

void ManageDatabasesDialog::onSelectionChanged()
{
    const bool hasSelection = (currentSelectedRow() >= 0);
    ui->renameButton->setEnabled(hasSelection);
    ui->deleteButton->setEnabled(hasSelection);
}

void ManageDatabasesDialog::onRenameClicked()
{
    const int row = currentSelectedRow();
    if (row < 0 || row >= mDatabases.size())
        return;

    const QString currentName = mDatabases[row].name;

    bool ok = false;
    const QString newName = QInputDialog::getText(
        this,
        tr("Rename Database"),
        tr("Enter a new name for the database:"),
        QLineEdit::Normal,
        currentName,
        &ok);

    if (!ok || newName.trimmed().isEmpty() || newName == currentName)
        return;

    mDatabases[row].name = newName.trimmed();

    // Update the table cell directly (no full rebuild needed)
    if (QTableWidgetItem *item = ui->tableWidget->item(row, 1))
        item->setText(newName.trimmed());

    qualpf::gui::workspaceContext(this).catalog().persist(qualpf::gui::databaseValues(mDatabases));
}

void ManageDatabasesDialog::onAddClicked()
{
    const QString sqFile = QFileDialog::getOpenFileName(
        this, tr("Add Database"), QString(),
        tr("QualX database (*.sq);;All files (*)"));

    if (sqFile.isEmpty())
        return;

    const QStringList missing = CreateDatabaseDialog::missingSqFiles(qualpf::gui::workspaceContext(this).platform(), sqFile);
    if (!missing.isEmpty()) {
        QMessageBox::critical(this, tr("Missing Files"),
            tr("The following companion files are missing:\n%1").arg(missing.join('\n')));
        return;
    }

    bool ok = false;
    const QString name = QInputDialog::getText(
        this, tr("Add Database"), tr("Enter a name for the database:"),
        QLineEdit::Normal, QFileInfo(sqFile).completeBaseName(), &ok);

    if (!ok || name.trimmed().isEmpty())
        return;

    registerExistingSqDatabase(sqFile, name.trimmed());
}

// Helper: append an entry for an existing .sq database and persist settings.
// sqFile is the full path to the .sq file; name is the display name.
void ManageDatabasesDialog::registerExistingSqDatabase(const QString &sqFile,
                                                        const QString &name)
{
    const QString base = sqFile.chopped(3);   // strip trailing ".sq"
    DatabaseEntry entry;
    entry.inUse       = mDatabases.isEmpty();
    entry.name        = name;
    const auto info = qualpf::gui::workspaceContext(this).catalog().describe(base.toStdString());
    entry.entries = info.first;
    entry.contentType = QString::fromStdString(info.second);
    entry.path        = base;
    mDatabases.append(entry);
    if (entry.inUse)
        mActiveIndex = mDatabases.size() - 1;
    qualpf::gui::workspaceContext(this).catalog().persist(qualpf::gui::databaseValues(mDatabases));
    rebuildTable();
}

void ManageDatabasesDialog::onCreateClicked()
{
    CreateDatabaseDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    const QString name = dlg.databaseName();

    // COD: register an existing QualX .sq database (no build step)
    if (dlg.isCodSelected()) {
        registerExistingSqDatabase(dlg.codSqFile(), name);
        return;
    }

    // User source — .sq file: register an existing QualX database (no build step)
    if (dlg.isUserSelected() &&
            dlg.userSource() == CreateDatabaseDialog::UserSource::SqDatabase) {
        registerExistingSqDatabase(dlg.userSqFile(), name);
        return;
    }

    if (!dlg.isPdfSelected() && !dlg.isUserSelected()) {
        QMessageBox::information(this, tr("Not Implemented"),
            tr("Only the ICDD PDF and user CIF sources are currently supported."));
        return;
    }

    const QString dir      = dlg.databaseDirectory();
    const QString basePath = dir + QDir::separator() + name;

    if (!qualpf::gui::workspaceContext(this).platform().makeDirectory(dir.toStdString())) {
        QMessageBox::critical(this, tr("Error"),
            tr("Could not create directory:\n%1").arg(dir));
        return;
    }

    bool cancelled = false;

    if (dlg.isPdfSelected()) {
        const QString pdf2File = dlg.pdfFile();
        if (pdf2File.isEmpty()) {
            QMessageBox::warning(this, tr("Missing File"),
                tr("Please select a PDF-2 data file (.dat)."));
            return;
        }
        if (!DatabaseBuilder::buildPdfDatabase(qualpf::gui::workspaceContext(this).databaseBuild(), basePath, pdf2File, this, &cancelled)) {
            if (!cancelled)
                QMessageBox::critical(this, tr("Error"),
                    tr("Failed to build database at:\n%1").arg(basePath));
            return;
        }
    } else {
        // User CIF source
        const QString cifDir = dlg.userSourceFolder();
        if (cifDir.isEmpty()) {
            QMessageBox::warning(this, tr("Missing Folder"),
                tr("Please select the folder containing the CIF files."));
            return;
        }
        if (!DatabaseBuilder::buildCifDatabase(qualpf::gui::workspaceContext(this).databaseBuild(), basePath, cifDir,
                dlg.isRecursive(), this, &cancelled)) {
            if (!cancelled)
                QMessageBox::critical(this, tr("Error"),
                    tr("Failed to build CIF database at:\n%1").arg(basePath));
            return;
        }
    }

    DatabaseEntry entry;
    entry.inUse       = mDatabases.isEmpty();
    entry.name        = name;
    const auto info = qualpf::gui::workspaceContext(this).catalog().describe(basePath.toStdString());
    entry.entries = info.first;
    entry.contentType = QString::fromStdString(info.second);
    entry.path        = basePath;

    mDatabases.append(entry);
    if (entry.inUse)
        mActiveIndex = mDatabases.size() - 1;

    qualpf::gui::workspaceContext(this).catalog().persist(qualpf::gui::databaseValues(mDatabases));
    rebuildTable();
}

void ManageDatabasesDialog::onDeleteClicked()
{
    const int row = currentSelectedRow();
    if (row < 0 || row >= mDatabases.size()) {
        QMessageBox::information(this, tr("No Selection"),
            tr("Please select a database to remove."));
        return;
    }

    const DatabaseEntry &entry = mDatabases[row];

    // Single confirmation dialog with embedded "delete files" checkbox
    QMessageBox msgBox(this);
    msgBox.setWindowTitle(tr("Remove Database"));
    msgBox.setText(tr("Remove <b>%1</b> from the database list?").arg(entry.name));
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
    msgBox.setDefaultButton(QMessageBox::Cancel);

    QCheckBox *deleteFilesCheck = new QCheckBox(
        tr("Also delete database files from disk (.sq, .sq.info, .sq.infostat, .sq.search)"));
    deleteFilesCheck->setChecked(true);
    msgBox.setCheckBox(deleteFilesCheck);   // msgBox takes ownership

    if (msgBox.exec() != QMessageBox::Yes)
        return;

    const bool deleteFiles = deleteFilesCheck->isChecked();
    auto removal = qualpf::gui::workspaceContext(this).catalog().removeDraft(
        qualpf::gui::databaseValues(mDatabases), row, mActiveIndex, deleteFiles,
        [this](const qualpf::application::DatabaseRemovalResult &result) {
            mDatabases = qualpf::gui::databaseViews(result.entries);
            mActiveIndex = result.activeIndex;
            QStringList failed;
            for (const auto &path : result.failedFiles) failed << QString::fromStdString(path);
            if (!failed.isEmpty()) {
                QMessageBox::warning(this, tr("Could Not Delete Files"),
                    tr("The following files could not be deleted:\n%1").arg(failed.join('\n')));
            }
        });
    if (!removal) {
        QMessageBox::warning(this, tr("Database Busy"), tr("The database is in use by a search."));
        return;
    }
    rebuildTable();
}
