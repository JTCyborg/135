#pragma once

#include <QMainWindow>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QTabWidget;

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    QWidget* createProjectPage();
    QWidget* createFilterPage();
    QWidget* createResultsPage();
    QWidget* createCadPage();
    void updateProjectTitle();
    void appendLog(const QString& message);

    QLineEdit* projectName_{};
    QComboBox* productFamily_{};
    QComboBox* filterTopology_{};
    QDoubleSpinBox* nominalVoltage_{};
    QDoubleSpinBox* fundamentalFrequency_{};
    QDoubleSpinBox* targetReactivePower_{};
    QPlainTextEdit* log_{};
    QLabel* status_{};
    QTabWidget* tabs_{};
};
