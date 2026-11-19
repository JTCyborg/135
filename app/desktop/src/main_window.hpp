#pragma once

#include <QMainWindow>
#include <QString>

#include "p135/infrastructure/sqlite/sqlite_database.hpp"

#include <memory>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QSpinBox;
class QTableWidget;
class QTabWidget;

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
    void updateProjectTitle();
    void appendLog(const QString& message);
    void calculateFilter();

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
    QTableWidget* harmonicSpectrumTable_{};
    QPlainTextEdit* log_{};
    QTableWidget* resultsTable_{};
    QLabel* resultsNote_{};
    QLabel* status_{};
    QTabWidget* tabs_{};
    p135::infrastructure::sqlite::SqliteDatabase database_;
    std::unique_ptr<p135::infrastructure::sqlite::SqliteMaterialRevisionStore> materialStore_;
    QString databaseError_;
};
