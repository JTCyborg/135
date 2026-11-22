#pragma once

#include <QMainWindow>
#include <QString>

#include <array>
#include <cstdint>
#include <optional>

#include "p135/infrastructure/sqlite/sqlite_database.hpp"
#include "p135/application/reactor_design_inputs.hpp"
#include "p135/application/reactor_design_execution.hpp"

#include <memory>

class QComboBox;
class QCheckBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QSpinBox;
class QTableWidget;
class QTabWidget;
class PlotWidget;

namespace p135::infrastructure::sqlite { class SqliteMaterialRevisionStore; }

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private:
    QWidget* createProjectPage();
    QWidget* createFilterPage();
    QWidget* createConstructionPage();
    QWidget* createResultsPage();
    QWidget* createCadPage();
    QWidget* createMaterialsPage();
    QWidget* createHistoryPage();
    void refreshCalculationHistory();
    void showCalculationDetails(int row);
    void loadCalculationInputs(int row);
    void compareSelectedCalculationGraphs();
    void exportCalculationHistory(bool selectedOnly);
    void importCalculationHistory();
    void updateProjectTitle();
    void saveProjectSettings();
    void loadProjectSettings();
    void saveProjectArchive();
    void loadProjectArchive();
    void appendLog(const QString& message);
    void calculateFilter();
    [[nodiscard]] p135::application::ReactorDesignInputs collectReactorDesignInputs() const;
    void refreshComponentSelectors();
    void markConstructionInputsChanged();

    QLineEdit* projectName_{};
    QComboBox* productFamily_{};
    QComboBox* filterConnection_{};
    QDoubleSpinBox* tuningHarmonicOrder_{};
    QDoubleSpinBox* qualityFactor_{};
    QDoubleSpinBox* nominalVoltage_{};
    QDoubleSpinBox* fundamentalFrequency_{};
    QDoubleSpinBox* targetReactivePower_{};
    QDoubleSpinBox* sourceResistance_{};
    QDoubleSpinBox* sourceInductance_{};
    QDoubleSpinBox* inductorSaturationCurrent_{};
    QLineEdit* inductorSaturationSource_{};
    QDoubleSpinBox* coreEffectiveAreaMm2_{};
    QDoubleSpinBox* coreMeanPathMm_{};
    QDoubleSpinBox* coreAirGapMm_{};
    QDoubleSpinBox* coreGapEffectiveAreaMm2_{};
    QDoubleSpinBox* coreYokeEffectiveAreaMm2_{};
    QDoubleSpinBox* coreYokeSegmentPathMm_{};
    std::array<QCheckBox*, 3> limbAreaOverrideEnabled_{};
    std::array<QCheckBox*, 3> limbPathOverrideEnabled_{};
    std::array<QCheckBox*, 3> limbGapOverrideEnabled_{};
    std::array<QCheckBox*, 3> limbGapAreaOverrideEnabled_{};
    std::array<QCheckBox*, 4> yokeAreaOverrideEnabled_{};
    std::array<QCheckBox*, 4> yokePathOverrideEnabled_{};
    std::array<QDoubleSpinBox*, 3> limbAreaOverrides_{};
    std::array<QDoubleSpinBox*, 3> limbPathOverrides_{};
    std::array<QDoubleSpinBox*, 3> limbGapOverrides_{};
    std::array<QDoubleSpinBox*, 3> limbGapAreaOverrides_{};
    std::array<QDoubleSpinBox*, 4> yokeAreaOverrides_{};
    std::array<QDoubleSpinBox*, 4> yokePathOverrides_{};
    std::array<QDoubleSpinBox*, 3> phaseDcBiasCurrentA_{};
    QSpinBox* inductorTurns_{};
    QPlainTextEdit* coreBhCurve_{};
    QLineEdit* coreBhSource_{};
    QTableWidget* harmonicSpectrumTable_{};
    QTableWidget* harmonicCurrentTable_{};
    QComboBox* capacitorComponent_{};
    QComboBox* resistorComponent_{};
    QDoubleSpinBox* capacitorThermalResistance_{};
    QLineEdit* capacitorThermalSource_{};
    QDoubleSpinBox* resistorThermalResistance_{};
    QDoubleSpinBox* ambientTemperature_{};
    QLineEdit* resistorThermalSource_{};
    QPlainTextEdit* log_{};
    QTableWidget* resultsTable_{};
    PlotWidget* resultsBankCurrentPlot_{};
    PlotWidget* resultsBusImpedancePlot_{};
    PlotWidget* resultsCoreFluxPlot_{};
    PlotWidget* historyComparisonPlot_{};
    QComboBox* historyPlotMetric_{};
    QTableWidget* historyTable_{};
    QPlainTextEdit* historyDetails_{};
    QLineEdit* historySearch_{};
    QLabel* resultsNote_{};
    QLabel* status_{};
    QTabWidget* tabs_{};
    p135::infrastructure::sqlite::SqliteDatabase database_;
    std::unique_ptr<p135::infrastructure::sqlite::SqliteMaterialRevisionStore> materialStore_;
    QString databaseError_;
    QString projectId_;
    bool loadingProjectSettings_{};
    QString latestFilterSnapshotId_;
    std::uint32_t latestFilterBranchId_{};
    bool constructionInputsDirty_{true};
    std::optional<p135::application::ReactorDesignInputs> acceptedConstructionInputs_;
    p135::application::ReactorDesignExecutionState reactorDesignExecutionState_;
};
