#ifndef RESTRAINTSDIALOG_H
#define RESTRAINTSDIALOG_H

#include <QDialog>
#include "qualpf/application/product_values.h"
#include "qualpf/application/algorithms.h"
#include <QStringList>

namespace Ui {
class RestraintsDialog;
}

class PeriodicTableWidget;
class QCheckBox;
class QLineEdit;
class QRadioButton;
class QPushButton;
class QSpinBox;

class RestraintsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RestraintsDialog(QWidget *parent = nullptr);
    ~RestraintsDialog();

    // --- Composition tab accessors ---
    QStringList compositionSymbols()  const;
    QString     compositionFormula()  const;
    bool        isExactComposition()  const;
    bool        isContainsAny()       const;
    int         compositionMinSpecies() const;
    int         compositionMaxSpecies() const;

    // --- Entries tab accessors ---
    QStringList entryIds() const;

    // --- Properties tab accessors ---
    QString     chemicalName()      const;
    QString     colorString()        const;
    QStringList colorStrings()       const;
    double      densityCalc()       const;
    double      densityMeas()       const;
    double      densityTolerance()  const;

    qualpf::application::MetadataValue snapshot() const;
    void restore(const qualpf::application::MetadataValue &state);
    bool hasRestraints() const;
    void setMergeEnabled(bool enabled);
    void setSearchEnabled(bool enabled);
    qualpf::application::SearchMethod selectedSearchMethod() const;
    void setSelectedSearchMethod(qualpf::application::SearchMethod);
    void setBusy(bool);
    void setBrowseEnabled(bool);

    // --- Subfiles tab accessors ---
    QStringList subfilesCodes() const;

    // --- Symmetry tab accessors ---
    QStringList crystalSystemStrings() const;
    QStringList spaceGroupStrings()    const;

    struct CellQuery {
        double values[6] = {-1,-1,-1,-1,-1,-1};  // a,b,c,alpha,beta,gamma (-1=not set)
        double lenTol = 0;
        double angTol = 0;
    };
    CellQuery cellQuery() const;

signals:
    void loadCardsRequested();
    void loadAndMergeCardsRequested();
    void searchWithRestraintsRequested(qualpf::application::SearchMethod method);
    void cancelAllRestraintsRequested();

private slots:
    void onHelpClicked();
    void onCancelAllRestraintsClicked();
    void onSymbolListClicked();
    void onAvailableColorsClicked();

    // Composition tab
    void onCompositionSelectionChanged(const QStringList &symbols);
    void onCompositionClearClicked();
    void onSpecialModeToggled(bool checked);

    // Subfiles tab
    void onSubfilesClearAll();
    void onSubfilesSelectAll();

private:
    void setupCompositionTab();
    void setupSubfilesTab();
    QString currentOperator() const;

    // Shows a two-column (Value / Count) picker dialog and appends selections
    // (separated by " ; ") to targetEdit.
    void showValuePickerDialog(const QString &title,
                               const QString &colHeader,
                               const QList<QPair<QString,int>> &rows,
                               QLineEdit *targetEdit);

    Ui::RestraintsDialog *ui;

    // Composition tab widgets
    PeriodicTableWidget *m_periodicTable      = nullptr;
    QLineEdit           *m_formulaEdit        = nullptr;
    QRadioButton        *m_radioAnd           = nullptr;
    QRadioButton        *m_radioOr            = nullptr;
    QRadioButton        *m_radioNot           = nullptr;
    QRadioButton        *m_radioExact         = nullptr;
    QRadioButton        *m_radioContainsAny   = nullptr;
    QPushButton         *m_clearButton        = nullptr;
    QSpinBox            *m_minSpecies         = nullptr;
    QSpinBox            *m_maxSpecies         = nullptr;
    QPushButton *m_csm = nullptr, *m_pfsm = nullptr;
    bool m_busy = false, m_continuous = false, m_browseEnabled = true, m_mergeEnabled = false;
    void updateSearchAdmission();

    QStringList m_lastSelectedSymbols;

    // Subfiles tab widgets
    QList<QCheckBox*> m_subfileChecks;
    QStringList       m_subfileCodes;
};


#endif // RESTRAINTSDIALOG_H
