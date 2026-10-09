#include "gui/adapters/metadata.h"
#include "gui/state/workspace_context.h"
#include "gui/dialogs/searchoptionsdialog.h"
#include "ui_searchoptionsdialog.h"
#include "qualpf/application/settings_keys.h"



#include <QAbstractButton>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QMessageBox>
#include "gui/adapters/preferences.h"
#include "gui/state/workspace_context.h"
#include <QSignalBlocker>
#include <QSlider>

using namespace qualpf::application::settings;
namespace {
QVariant searchSetting(const qualpf::application::SearchSettingsProvider &provider, qualpf::application::ProductHandle source, const char *key, const QVariant &fallback) {
    return qualpf::gui::Preferences::decode(provider.option(source,key,qualpf::gui::Preferences::encode(fallback)));
}
}

static void wireSliderSpin(QSlider *sl, QDoubleSpinBox *sp)
{
    QObject::connect(sl, &QSlider::valueChanged, sp, [sp](int v) {
        QSignalBlocker b(sp);
        sp->setValue(v / 100.0);
    });
    QObject::connect(sp, &QDoubleSpinBox::valueChanged, sl, [sl](double v) {
        QSignalBlocker b(sl);
        sl->setValue(qRound(v * 100.0));
    });
}

SearchOptionsDialog::SearchOptionsDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::SearchOptionsDialog)
{
    ui->setupUi(this);

    wireSliderSpin(ui->sliderFom,       ui->spinFom);
    wireSliderSpin(ui->sliderTwothetaD, ui->spinTwothetaD);
    wireSliderSpin(ui->sliderIntensity, ui->spinIntensity);
    wireSliderSpin(ui->sliderPhases,    ui->spinPhases);
    wireSliderSpin(ui->sliderDelta,     ui->spinDelta);

    connect(ui->checkAuto, &QCheckBox::toggled,
            this, &SearchOptionsDialog::onAutoToggled);

    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, [this]() {
        saveSettings();
        accept();
    });
    connect(ui->buttonBox, &QDialogButtonBox::rejected,
            this, &QDialog::reject);
    connect(ui->buttonBox, &QDialogButtonBox::helpRequested, this, []() {
        QMessageBox::information(nullptr, QObject::tr("Help"),
            QObject::tr("Adjust the weights and options used during database search.\n\n"
                        "Min. FOM: minimum figure of merit threshold.\n"
                        "2θ/d: relative weight of 2theta/d-spacing matching.\n"
                        "Intensity: relative weight of intensity matching.\n"
                        "Phases: bias towards single- or multi-phase solutions.\n"
                        "Δ2θ: 2theta tolerance (use Auto for automatic selection)."));
    });
    connect(ui->buttonBox, &QDialogButtonBox::clicked,
            this, &SearchOptionsDialog::onButtonClicked);

    loadSettings();
    onAutoToggled(ui->checkAuto->isChecked());
}

SearchOptionsDialog::~SearchOptionsDialog()
{
    delete ui;
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------

void SearchOptionsDialog::onAutoToggled(bool checked)
{
    if (checked)
        ui->spinDelta->setValue(qualpf::gui::workspaceContext(this).searchSession().snapshot()->automaticDelta);
    ui->sliderDelta->setDisabled(checked);
    ui->spinDelta->setDisabled(checked);
}

void SearchOptionsDialog::onButtonClicked(QAbstractButton *button)
{
    if (ui->buttonBox->buttonRole(button) == QDialogButtonBox::ResetRole)
        resetToDefaults();
}

// ---------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------

void SearchOptionsDialog::resetToDefaults()
{
    ui->spinFom->setValue(0.35);
    ui->spinTwothetaD->setValue(0.50);
    ui->spinIntensity->setValue(0.50);
    ui->spinPhases->setValue(0.50);
    ui->checkAuto->setChecked(true);
    ui->checkResidual->setChecked(true);
    ui->checkStrongest->setChecked(true);
    ui->checkDeleted->setChecked(true);
    ui->lineEditMaxEntries->setText(QStringLiteral("3000"));
    onAutoToggled(true);
}

void SearchOptionsDialog::loadSettings()
{
    const auto source = qualpf::gui::workspaceContext(this).workspace().session().snapshot();
    ui->spinFom->setValue(      searchSetting(qualpf::gui::workspaceContext(this).settings(), source, SK_FOM,       0.35).toDouble());
    ui->spinTwothetaD->setValue(searchSetting(qualpf::gui::workspaceContext(this).settings(), source, SK_2THETA,    0.50).toDouble());
    ui->spinIntensity->setValue(searchSetting(qualpf::gui::workspaceContext(this).settings(), source, SK_INT,       0.50).toDouble());
    ui->spinPhases->setValue(   searchSetting(qualpf::gui::workspaceContext(this).settings(), source, SK_PHASES,    0.50).toDouble());
    ui->spinDelta->setValue(    searchSetting(qualpf::gui::workspaceContext(this).settings(), source, SK_DELTA,     0.08).toDouble());
    ui->checkAuto->setChecked(  searchSetting(qualpf::gui::workspaceContext(this).settings(), source, SK_AUTO,      true).toBool());
    ui->checkResidual->setChecked( searchSetting(qualpf::gui::workspaceContext(this).settings(), source, SK_RESIDUAL,  true).toBool());
    ui->checkStrongest->setChecked(searchSetting(qualpf::gui::workspaceContext(this).settings(), source, SK_STRONGEST, true).toBool());
    ui->checkDeleted->setChecked(  searchSetting(qualpf::gui::workspaceContext(this).settings(), source, SK_DELETED,   true).toBool());
    ui->lineEditMaxEntries->setText(searchSetting(qualpf::gui::workspaceContext(this).settings(), source, SK_MAXENT, 3000).toString());
}

void SearchOptionsDialog::saveSettings()
{
    if (qualpf::gui::workspaceContext(this).workspace().searches().running() || qualpf::gui::workspaceContext(this).workspace().searches().closing()) return;
    const auto input = qualpf::gui::workspaceContext(this).searchSession().snapshot();
    auto next = *input;
    next.searchOptions = {{SK_FOM, ui->spinFom->value()}, {SK_2THETA, ui->spinTwothetaD->value()},
        {SK_INT, ui->spinIntensity->value()}, {SK_PHASES, ui->spinPhases->value()}, {SK_DELTA, ui->spinDelta->value()},
        {SK_AUTO, ui->checkAuto->isChecked()}, {SK_RESIDUAL, ui->checkResidual->isChecked()},
        {SK_STRONGEST, ui->checkStrongest->isChecked()}, {SK_DELETED, ui->checkDeleted->isChecked()},
        {SK_MAXENT, ui->lineEditMaxEntries->text().toInt()}};
    const double delta = ui->checkAuto->isChecked() ? next.automaticDelta : ui->spinDelta->value();
    auto peaks = qualpf::gui::peaksWithTolerance(next.peaks, delta);
    if (!peaks) { qWarning("Search tolerance update failed"); return; }
    next.peaks = std::move(peaks).value();
    next.peakTolerance = delta;
    for (auto &card : next.candidates) card.invalidateFom();
    auto committed = qualpf::gui::workspaceContext(this).searchSession().commit(input, std::move(next));
    if (!committed) { qWarning("Search settings were not published"); return; }
    qualpf::gui::Preferences preferences(qualpf::gui::workspaceContext(this).preferences());
    const auto applied = qualpf::gui::workspaceContext(this).searchSession().snapshot()->searchOptions;
    for (const auto &[key, value] : applied.toObject()) preferences.setValue(QString::fromStdString(key), qualpf::gui::metadataVariant(value));
}

// ---------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------

qualpf::application::MetadataObject SearchOptionsDialog::snapshotSettings(qualpf::application::ProductHandle source, const qualpf::application::SearchSettingsProvider &provider)
{
    return {{SK_FOM, savedMinFom(source,provider)}, {SK_2THETA, savedWeight2thetaD(source,provider)}, {SK_INT, savedWeightIntensity(source,provider)},
            {SK_PHASES, savedWeightPhases(source,provider)}, {SK_DELTA, searchSetting(provider,source, SK_DELTA, 0.08).toDouble()},
            {SK_AUTO, searchSetting(provider,source, SK_AUTO, true).toBool()}, {SK_RESIDUAL, savedResidualSearching(source,provider)},
            {SK_STRONGEST, savedCheckStrongest(source,provider)}, {SK_DELETED, savedCheckDeleted(source,provider)}, {SK_MAXENT, savedMaxEntries(source,provider)}};
}

double SearchOptionsDialog::savedMinFom(qualpf::application::ProductHandle source, const qualpf::application::SearchSettingsProvider &provider)      { return searchSetting(provider,source, SK_FOM,       0.35).toDouble(); }
int    SearchOptionsDialog::savedMaxEntries(qualpf::application::ProductHandle source, const qualpf::application::SearchSettingsProvider &provider)  { return searchSetting(provider,source, SK_MAXENT,   3000).toInt(); }
bool   SearchOptionsDialog::savedCheckStrongest(qualpf::application::ProductHandle source, const qualpf::application::SearchSettingsProvider &provider) { return searchSetting(provider,source, SK_STRONGEST, true).toBool(); }
bool   SearchOptionsDialog::savedCheckDeleted(qualpf::application::ProductHandle source, const qualpf::application::SearchSettingsProvider &provider)   { return searchSetting(provider,source, SK_DELETED,   true).toBool(); }
double SearchOptionsDialog::savedWeight2thetaD(qualpf::application::ProductHandle source, const qualpf::application::SearchSettingsProvider &provider)  { return searchSetting(provider,source, SK_2THETA,    0.50).toDouble(); }
double SearchOptionsDialog::savedWeightIntensity(qualpf::application::ProductHandle source, const qualpf::application::SearchSettingsProvider &provider){ return searchSetting(provider,source, SK_INT,       0.50).toDouble(); }
double SearchOptionsDialog::savedWeightPhases(qualpf::application::ProductHandle source, const qualpf::application::SearchSettingsProvider &provider)   { return searchSetting(provider,source, SK_PHASES,    0.50).toDouble(); }
double SearchOptionsDialog::savedDelta2theta(qualpf::application::ProductHandle source, const qualpf::application::SearchSettingsProvider &provider)
{
    if (searchSetting(provider,source, SK_AUTO, true).toBool())
        return source->automaticDelta;
    return searchSetting(provider,source, SK_DELTA, 0.08).toDouble();
}
bool SearchOptionsDialog::savedResidualSearching(qualpf::application::ProductHandle source, const qualpf::application::SearchSettingsProvider &provider) { return searchSetting(provider,source, SK_RESIDUAL, true).toBool(); }

double SearchOptionsDialog::minFom()              const { return ui->spinFom->value(); }
double SearchOptionsDialog::weight2thetaD()       const { return ui->spinTwothetaD->value(); }
double SearchOptionsDialog::weightIntensity()     const { return ui->spinIntensity->value(); }
double SearchOptionsDialog::weightPhases()        const { return ui->spinPhases->value(); }
double SearchOptionsDialog::delta2theta()         const { return ui->spinDelta->value(); }
bool   SearchOptionsDialog::delta2thetaAuto()     const { return ui->checkAuto->isChecked(); }
bool   SearchOptionsDialog::residualSearching()   const { return ui->checkResidual->isChecked(); }
bool   SearchOptionsDialog::checkStrongestPeaks() const { return ui->checkStrongest->isChecked(); }
bool   SearchOptionsDialog::checkDeletedCards()   const { return ui->checkDeleted->isChecked(); }
int    SearchOptionsDialog::maxEntries()          const { return ui->lineEditMaxEntries->text().toInt(); }
