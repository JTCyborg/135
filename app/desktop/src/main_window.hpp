#pragma once

#include <QMainWindow>
#include <QString>

#include <array>

#include "p135/infrastructure/sqlite/sqlite_database.hpp"

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
    QWidget* createResultsPage();
    QWidget* createCadPage();
    QWidget* createMaterialsPage();
    QWidget* createHistoryPage();
    void refreshCalculationHistory();
    void showCalculationDetails(int row);
    void loadCalculationInputs(int row);
    void compareSelectedCalculationGraphs();
    void updateProjectTitle();
    void appendLog(const QString& message);
    void calculateFilter();
    void refreshComponentSelectors();

    QLineEdit* projectName_{};
    QComboBox* productFamily_{};
    QComboBox* filterConnection_{};
    QSpinBox* tuningHarmonicOrder_{};
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
    QDoubleSpinBox* coreYokeEffectiveAreaMm2_{};
    QDoubleSpinBox* coreYokeSegmentPathMm_{};
    std::array<QCheckBox*, 3> limbAreaOverrideEnabled_{};
    std::array<QCheckBox*, 3> limbPathOverrideEnabled_{};
    std::array<QCheckBox*, 3> limbGapOverrideEnabled_{};
    std::array<QCheckBox*, 4> yokeAreaOverrideEnabled_{};
    std::array<QCheckBox*, 4> yokePathOverrideEnabled_{};
    std::array<QDoubleSpinBox*, 3> limbAreaOverrides_{};
    std::array<QDoubleSpinBox*, 3> limbPathOverrides_{};
    std::array<QDoubleSpinBox*, 3> limbGapOverrides_{};
    std::array<QDoubleSpinBox*, 4> yokeAreaOverrides_{};
    std::array<QDoubleSpinBox*, 4> yokePathOverrides_{};
    std::array<QDoubleSpinBox*, 3> phaseDcBiasCurrentA_{};
    QSpinBox* inductorTurns_{};
    QPlainTextEdit* coreBhCurve_{};
    QLineEdit* coreBhSource_{};
    QTableWidget* harmonicSpectrumTable_{};
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
};
