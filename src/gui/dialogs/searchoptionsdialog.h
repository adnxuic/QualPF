#ifndef SEARCHOPTIONSDIALOG_H
#define SEARCHOPTIONSDIALOG_H

#include <QDialog>
#include "qualpf/application/product_values.h"
#include "qualpf/application/product_session.h"
#include "qualpf/application/workspace_ports.h"

namespace Ui { class SearchOptionsDialog; }
class QAbstractButton;

class SearchOptionsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SearchOptionsDialog(QWidget *parent = nullptr);
    ~SearchOptionsDialog();

    // Static helpers — read saved settings without creating the UI
    static qualpf::application::MetadataObject snapshotSettings(qualpf::application::ProductHandle source, const qualpf::application::SearchSettingsProvider &provider);
    static double savedMinFom(qualpf::application::ProductHandle source, const qualpf::application::SearchSettingsProvider &provider);
    static int    savedMaxEntries(qualpf::application::ProductHandle source, const qualpf::application::SearchSettingsProvider &provider);
    static bool   savedCheckStrongest(qualpf::application::ProductHandle source, const qualpf::application::SearchSettingsProvider &provider);
    static bool   savedCheckDeleted(qualpf::application::ProductHandle source, const qualpf::application::SearchSettingsProvider &provider);
    static double savedWeight2thetaD(qualpf::application::ProductHandle source, const qualpf::application::SearchSettingsProvider &provider);
    static double savedWeightIntensity(qualpf::application::ProductHandle source, const qualpf::application::SearchSettingsProvider &provider);
    static double savedWeightPhases(qualpf::application::ProductHandle source, const qualpf::application::SearchSettingsProvider &provider);
    static double savedDelta2theta(qualpf::application::ProductHandle source, const qualpf::application::SearchSettingsProvider &provider);
    static bool   savedResidualSearching(qualpf::application::ProductHandle source, const qualpf::application::SearchSettingsProvider &provider);

    double minFom()              const;
    double weight2thetaD()       const;
    double weightIntensity()     const;
    double weightPhases()        const;
    double delta2theta()         const;
    bool   delta2thetaAuto()     const;
    bool   residualSearching()   const;
    bool   checkStrongestPeaks() const;
    bool   checkDeletedCards()   const;
    int    maxEntries()          const;

private slots:
    void onButtonClicked(QAbstractButton *button);
    void onAutoToggled(bool checked);

private:
    void loadSettings();
    void saveSettings();
    void resetToDefaults();

    Ui::SearchOptionsDialog *ui;
};

#endif // SEARCHOPTIONSDIALOG_H
