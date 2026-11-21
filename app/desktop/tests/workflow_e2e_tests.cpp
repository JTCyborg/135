#include "main_window.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDebug>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QImage>
#include <QDir>
#include <QApplication>
#include <QTemporaryDir>
#include <QTimer>
#include <QLineEdit>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTest>
#include <QTableWidget>
#include <QTreeWidget>
#include <QStringList>
#include <QWidget>

#include <array>
#include <optional>
#include <complex>
#include <cmath>
#include <fstream>
#include <iterator>
#include <string>

#include <BRepCheck_Analyzer.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <STEPControl_Reader.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>

class DesktopWorkflowE2ETests final : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<MainWindow> window_;

private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("Project135Tests"));
        QCoreApplication::setApplicationName(QStringLiteral("DesktopWorkflowE2E"));
        QStandardPaths::setTestModeEnabled(true);
        QSettings settings;
        settings.clear();
    }

    void init()
    {
        qInfo().noquote() << "[E2E-DIAG] init: before MainWindow construction";
        window_ = std::make_unique<MainWindow>();
        qInfo().noquote() << "[E2E-DIAG] init: MainWindow constructed";
        qInfo().noquote() << "[E2E-DIAG] init: before show()";
        window_->show();
        qInfo().noquote() << "[E2E-DIAG] init: show() returned";
        QTest::qWait(50);
        qInfo().noquote() << "[E2E-DIAG] init: event wait completed";
    }

    void cleanup()
    {
        window_.reset();
        QSettings settings;
        settings.clear();
    }

    void workflowPagesAndEntryPointsExist()
    {
        auto* tabs = window_->findChild<QTabWidget*>();
        QVERIFY2(tabs, "Main workflow tab widget was not created");
        QCOMPARE(tabs->count(), 7);
        QCOMPARE(tabs->tabText(0), QStringLiteral("Project"));
        QCOMPARE(tabs->tabText(1), QStringLiteral("Filter definition"));
        QCOMPARE(tabs->tabText(2), QStringLiteral("Construction"));
        QCOMPARE(tabs->tabText(3), QStringLiteral("Calculation & design"));
        QCOMPARE(tabs->tabText(4), QStringLiteral("3D CAD"));
        QCOMPARE(tabs->tabText(5), QStringLiteral("Materials database"));
        QCOMPARE(tabs->tabText(6), QStringLiteral("Calculation history"));

        const QStringList requiredButtons{
            QStringLiteral("confirmProjectAndContinue"),
            QStringLiteral("confirmFilterDefinitionAndContinue"),
            QStringLiteral("confirmConstructionAndContinue"),
            QStringLiteral("recalculateProjectButton"),
            QStringLiteral("confirmCalculationDesign"),
            QStringLiteral("generatePreliminaryCad")
        };
        for (const QString& name : requiredButtons) {
            auto* button = window_->findChild<QPushButton*>(name);
            QVERIFY2(button, qPrintable(QStringLiteral("Missing workflow action: ") + name));
        }
    }

    void everyWorkflowTabCanBeSelected()
    {
        auto* tabs = window_->findChild<QTabWidget*>();
        QVERIFY(tabs);
        QCOMPARE(tabs->count(), 7);

        for (int index = 0; index < tabs->count(); ++index) {
            tabs->setCurrentIndex(index);
            QTRY_COMPARE(tabs->currentIndex(), index);
            QVERIFY2(tabs->widget(index),
                qPrintable(QStringLiteral("Workflow page %1 is missing").arg(index)));
        }
    }

    void inputControlsHaveStableIdentifiers()
    {
        auto* tabs = window_->findChild<QTabWidget*>();
        QVERIFY(tabs);

        // The first three tabs are the editable input stages. Every user-editable
        // control must have a stable objectName so snapshots and automation address
        // the same field rather than relying on translated labels or widget order.
        for (int tabIndex = 0; tabIndex < 3; ++tabIndex) {
            QWidget* page = tabs->widget(tabIndex);
            QVERIFY(page);
            for (QWidget* control : page->findChildren<QWidget*>()) {
                const bool editable =
                    qobject_cast<QLineEdit*>(control) ||
                    qobject_cast<QComboBox*>(control) ||
                    qobject_cast<QDoubleSpinBox*>(control) ||
                    qobject_cast<QSpinBox*>(control) ||
                    qobject_cast<QCheckBox*>(control) ||
                    qobject_cast<QTableWidget*>(control) ||
                    (qobject_cast<QPlainTextEdit*>(control) &&
                     !qobject_cast<QPlainTextEdit*>(control)->isReadOnly());
                if (!editable) continue;
                QVERIFY2(!control->objectName().isEmpty(),
                    qPrintable(QStringLiteral("Unnamed input on tab %1: %2")
                        .arg(tabIndex).arg(control->metaObject()->className())));
            }
        }
    }

    void projectFilterAndConstructionConfirmationsAdvance()
    {
        auto* tabs = window_->findChild<QTabWidget*>();
        QVERIFY(tabs);

        auto clickAndWait = [this, tabs](const char* objectName, int expectedIndex) {
            auto* button = window_->findChild<QPushButton*>(QString::fromLatin1(objectName));
            QVERIFY2(button, objectName);
            QTest::mouseClick(button, Qt::LeftButton);
            QTRY_COMPARE_WITH_TIMEOUT(tabs->currentIndex(), expectedIndex, 3000);
        };

        clickAndWait("confirmProjectAndContinue", 1);
        auto* synthesize = window_->findChild<QPushButton*>(QStringLiteral("synthesizeLcBranch"));
        QVERIFY(synthesize);
        QTest::mouseClick(synthesize, Qt::LeftButton);
        clickAndWait("confirmFilterDefinitionAndContinue", 2);
        clickAndWait("confirmConstructionAndContinue", 3);
    }

    void calculationAndCadStagesAreReachable()
    {
        auto* tabs = window_->findChild<QTabWidget*>();
        QVERIFY(tabs);

        QTest::mouseClick(window_->findChild<QPushButton*>(
            QStringLiteral("confirmProjectAndContinue")), Qt::LeftButton);
        QTRY_COMPARE(tabs->currentIndex(), 1);
        auto* synthesize = window_->findChild<QPushButton*>(QStringLiteral("synthesizeLcBranch"));
        QVERIFY(synthesize);
        QTest::mouseClick(synthesize, Qt::LeftButton);
        QTest::mouseClick(window_->findChild<QPushButton*>(
            QStringLiteral("confirmFilterDefinitionAndContinue")), Qt::LeftButton);
        QTRY_COMPARE(tabs->currentIndex(), 2);
        QTest::mouseClick(window_->findChild<QPushButton*>(
            QStringLiteral("confirmConstructionAndContinue")), Qt::LeftButton);
        QTRY_COMPARE(tabs->currentIndex(), 3);

        auto* recalculate = window_->findChild<QPushButton*>(
            QStringLiteral("recalculateProjectButton"));
        QVERIFY(recalculate);
        QTest::mouseClick(recalculate, Qt::LeftButton);

        auto* results = window_->findChild<QTableWidget*>(QStringLiteral("calculationResultsTable"));
        QVERIFY2(results, "Calculation results table was not created");
        QTRY_VERIFY_WITH_TIMEOUT(results->rowCount() > 0, 10000);

        // TEST-01 numeric regression: 400 V L-L, 50 Hz, 100 kvar, Wye,
        // 5th-harmonic tuning and Qf=50. These values are calculated from
        // the independent series-RLC sizing equations, not copied from UI text.
        const auto resultValue = [results](const QString& label) -> std::optional<double> {
            for (int row = 0; row < results->rowCount(); ++row) {
                const auto* name = results->item(row, 0);
                const auto* value = results->item(row, 1);
                if (!name || !value || name->text() != label) continue;
                bool parsed = false;
                const double number = value->text().toDouble(&parsed);
                if (parsed) return number;
                return std::nullopt;
            }
            return std::nullopt;
        };
        const auto phaseVoltage = resultValue(QStringLiteral("Phase RMS voltage"));
        const auto capacitance = resultValue(QStringLiteral("Capacitance per phase"));
        const auto inductance = resultValue(QStringLiteral("Inductance per phase"));
        const auto resistance = resultValue(QStringLiteral("Series damping resistance"));
        QVERIFY2(phaseVoltage.has_value(), "TEST-01 phase voltage result is missing or non-numeric");
        QVERIFY2(capacitance.has_value(), "TEST-01 capacitance result is missing or non-numeric");
        QVERIFY2(inductance.has_value(), "TEST-01 inductance result is missing or non-numeric");
        QVERIFY2(resistance.has_value(), "TEST-01 damping resistance result is missing or non-numeric");
        QVERIFY(qAbs(*phaseVoltage - 230.9401077) <= 0.1);
        QVERIFY(qAbs(*capacitance - 1909.892474) <= 1.0);
        QVERIFY(qAbs(*inductance - 0.212202907) <= 0.0001);
        QVERIFY(qAbs(*resistance - 0.006666551) <= 0.000001);

        auto* confirmCalculation = window_->findChild<QPushButton*>(
            QStringLiteral("confirmCalculationDesign"));
        QVERIFY(confirmCalculation);
        QTest::mouseClick(confirmCalculation, Qt::LeftButton);
        QTRY_COMPARE_WITH_TIMEOUT(tabs->currentIndex(), 4, 5000);

        auto* generateCad = window_->findChild<QPushButton*>(
            QStringLiteral("generatePreliminaryCad"));
        QVERIFY(generateCad);
        QTest::mouseClick(generateCad, Qt::LeftButton);

        // This workflow intentionally has no fabricated physical dimensions.
        // The CAD adapter must reject the incomplete Construction revision and
        // leave the placeholder tree/STEP export untouched rather than showing
        // a successful but invented assembly.
        auto* cadStatus = window_->findChild<QLabel*>(QStringLiteral("cadGenerationStatus"));
        QVERIFY(cadStatus);
        QTRY_VERIFY_WITH_TIMEOUT(
            cadStatus->text().contains(QStringLiteral("explicit physical inputs")), 3000);
        auto* assemblyTree = window_->findChild<QTreeWidget*>(QStringLiteral("cadAssemblyTree"));
        QVERIFY(assemblyTree);
        QCOMPARE(assemblyTree->topLevelItemCount(), 1);
        QCOMPARE(assemblyTree->topLevelItem(0)->text(0), QStringLiteral("No generated assembly"));
        auto* exportStep = window_->findChild<QPushButton*>(QStringLiteral("exportCadStep"));
        QVERIFY(exportStep);
        QVERIFY(!exportStep->isEnabled());
    }

    void syntheticTest01ConstructionBuildsOcctAssembly()
    {
        auto* tabs = window_->findChild<QTabWidget*>();
        QVERIFY(tabs);

        // Enter the normal workflow through the UI. The geometry below is a
        // deterministic synthetic CAD regression fixture, NOT a manufacturing
        // drawing or an engineering-approved reactor design.
        auto* projectConfirm = window_->findChild<QPushButton*>(
            QStringLiteral("confirmProjectAndContinue"));
        QVERIFY(projectConfirm);
        QTest::mouseClick(projectConfirm, Qt::LeftButton);
        QTRY_COMPARE(tabs->currentIndex(), 1);

        auto* synthesize = window_->findChild<QPushButton*>(
            QStringLiteral("synthesizeLcBranch"));
        QVERIFY(synthesize);
        QTest::mouseClick(synthesize, Qt::LeftButton);

        auto* filterConfirm = window_->findChild<QPushButton*>(
            QStringLiteral("confirmFilterDefinitionAndContinue"));
        QVERIFY(filterConfirm);
        QTest::mouseClick(filterConfirm, Qt::LeftButton);
        QTRY_COMPARE(tabs->currentIndex(), 2);

        const auto selectCombo = [this](const QString& name, const QVariant& data) {
            auto* combo = window_->findChild<QComboBox*>(name);
            if (!combo) return false;
            const int index = combo->findData(data);
            if (index < 0) return false;
            combo->setCurrentIndex(index);
            return combo->currentData() == data;
        };
        const auto setDouble = [this](const QString& name, double value) {
            auto* field = window_->findChild<QDoubleSpinBox*>(name);
            if (!field) return false;
            field->setValue(value);
            return qAbs(field->value() - value) < 1.0e-9;
        };
        const auto setInteger = [this](const QString& name, int value) {
            auto* field = window_->findChild<QSpinBox*>(name);
            if (!field) return false;
            field->setValue(value);
            return field->value() == value;
        };
        const auto setText = [this](const QString& name, const QString& value) {
            auto* field = window_->findChild<QLineEdit*>(name);
            if (!field) return false;
            field->setText(value);
            return field->text() == value;
        };

        // Pin exact, seeded material revisions. No "latest" or free-text
        // material aliases are accepted by this fixture.
        QVERIFY(selectCombo(QStringLiteral("construction_coreMaterial"),
            QStringLiteral("SEED-TK-M235-35A-R1")));
        QVERIFY(selectCombo(QStringLiteral("construction_conductorMaterial"),
            QStringLiteral("SEED-CU-C10100-R1")));
        QVERIFY(selectCombo(QStringLiteral("construction_insulationMaterial"),
            QStringLiteral("SEED-DUPONT-NOMEX410-R1")));
        QVERIFY(selectCombo(QStringLiteral("construction_coreGeometry"),
            QStringLiteral("three-limb")));
        QVERIFY(selectCombo(QStringLiteral("construction_conductorShape"),
            QStringLiteral("round")));
        QVERIFY(selectCombo(QStringLiteral("construction_windingTechnology"),
            QStringLiteral("layer")));
        QVERIFY(selectCombo(QStringLiteral("construction_bobbinUsage"), true));
        QVERIFY(selectCombo(QStringLiteral("construction_cooling"),
            QStringLiteral("natural-air")));
        QVERIFY(selectCombo(QStringLiteral("construction_insulationThermalClass"),
            QStringLiteral("B")));

        QVERIFY(setDouble(QStringLiteral("construction_stackingFactor"), 0.95));
        QVERIFY(setText(QStringLiteral("construction_laminationConstruction"),
            QStringLiteral("Uniform 40 mm stack; eight butt interfaces; synthetic E2E fixture only")));
        QVERIFY(setText(QStringLiteral("construction_dimensionalSource"),
            QStringLiteral("TEST-01 synthetic CAD regression fixture; not a source engineering drawing")));
        QVERIFY(setText(QStringLiteral("construction_dimensionalRevision"),
            QStringLiteral("TEST-01-SYNTHETIC-GEOMETRY-R1")));
        QVERIFY(setText(QStringLiteral("construction_bobbinMaterial"),
            QStringLiteral("TEST-01 synthetic insulating bobbin material; properties NOT_VERIFIED")));

        // Three-limb core: all values are explicit millimetres. The dimensions
        // satisfy the domain stack-up equations and equal-depth Butt interfaces.
        for (int i = 1; i <= 3; ++i) {
            QVERIFY(setDouble(QStringLiteral("construction_limbWidth_%1").arg(i), 60.0));
            QVERIFY(setDouble(QStringLiteral("construction_limbDepth_%1").arg(i), 40.0));
        }
        for (int i = 1; i <= 2; ++i) {
            QVERIFY(setDouble(QStringLiteral("construction_windowWidth_%1").arg(i), 80.0));
            QVERIFY(setDouble(QStringLiteral("construction_windowHeight_%1").arg(i), 190.0));
        }
        for (int i = 1; i <= 4; ++i) {
            QVERIFY(setDouble(QStringLiteral("construction_yokeThickness_%1").arg(i), 30.0));
            QVERIFY(setDouble(QStringLiteral("construction_yokeDepth_%1").arg(i), 40.0));
        }
        QVERIFY(setDouble(QStringLiteral("construction_limbCenterSpacing_1"), 140.0));
        QVERIFY(setDouble(QStringLiteral("construction_limbCenterSpacing_2"), 140.0));
        QVERIFY(setDouble(QStringLiteral("construction_limbCenterSpacing_3"), 280.0));
        QVERIFY(setDouble(QStringLiteral("construction_coreOverallHeight"), 250.0));
        QVERIFY(setDouble(QStringLiteral("construction_coreOverallWidth"), 340.0));
        for (int i = 1; i <= 8; ++i) {
            QVERIFY(selectCombo(QStringLiteral("construction_coreJoint_%1").arg(i),
                QStringLiteral("butt")));
        }

        // Explicit winding/conductor inputs. These values make the UI fixture
        // deterministic; they are not used to claim a verified electrical or
        // thermal design.
        QVERIFY(setInteger(QStringLiteral("construction_turns"), 20));
        QVERIFY(setInteger(QStringLiteral("construction_turnsPerLayer"), 10));
        QVERIFY(setInteger(QStringLiteral("construction_layers"), 2));
        QVERIFY(setInteger(QStringLiteral("construction_sections"), 1));
        QVERIFY(setInteger(QStringLiteral("construction_parallelConductors"), 1));
        QVERIFY(setDouble(QStringLiteral("construction_conductorArea"), 3.1416));
        QVERIFY(setDouble(QStringLiteral("construction_conductorBareDiameter"), 2.0));
        QVERIFY(setDouble(QStringLiteral("construction_conductorInsulatedDiameter"), 2.2));
        QVERIFY(setDouble(QStringLiteral("construction_conductorInsulationBuild"), 0.1));
        QVERIFY(setDouble(QStringLiteral("construction_turnInsulation"), 0.05));
        QVERIFY(setDouble(QStringLiteral("construction_layerInsulation"), 0.2));
        QVERIFY(setDouble(QStringLiteral("construction_mainInsulation"), 1.0));

        // Bobbin and winding envelopes: all three phases use the same declared
        // test dimensions. Axial and radial clearances satisfy the OCCT adapter.
        for (int i = 1; i <= 3; ++i) {
            QVERIFY(setDouble(QStringLiteral("construction_bobbinInnerWidth_%1").arg(i), 64.0));
            QVERIFY(setDouble(QStringLiteral("construction_bobbinInnerHeight_%1").arg(i), 46.0));
            QVERIFY(setDouble(QStringLiteral("construction_bobbinAxialLength_%1").arg(i), 168.0));
            QVERIFY(setDouble(QStringLiteral("construction_bobbinWallThickness_%1").arg(i), 3.0));
            QVERIFY(setDouble(QStringLiteral("construction_bobbinFlangeThickness_%1").arg(i), 4.0));
            QVERIFY(setDouble(QStringLiteral("construction_bobbinFlangeOuterWidth_%1").arg(i), 72.0));
            QVERIFY(setDouble(QStringLiteral("construction_bobbinFlangeOuterHeight_%1").arg(i), 54.0));
            QVERIFY(setDouble(QStringLiteral("construction_bobbinWindingClearance_%1").arg(i), 2.0));
            QVERIFY(setDouble(QStringLiteral("construction_windingRadialBuild_%1").arg(i), 5.0));
            QVERIFY(setDouble(QStringLiteral("construction_windingAxialHeight_%1").arg(i), 140.0));
            QVERIFY(setDouble(QStringLiteral("construction_windingEndClearance_%1").arg(i), 10.0));
        }

        // One explicit limb-A air gap exercises the DXF cut-face projection,
        // the air-gap schedule and the additional physical core solid.
        QVERIFY(setInteger(QStringLiteral("construction_airGapCount_1"), 1));
        QVERIFY(setDouble(QStringLiteral("construction_totalAirGap_1"), 2.0));
        QVERIFY(setDouble(QStringLiteral("construction_airGapPosition_1_1"), 80.0));
        QVERIFY(setDouble(QStringLiteral("construction_airGapLength_1_1"), 2.0));
        QVERIFY(setDouble(QStringLiteral("construction_yokeAirGapPosition_1_1"), 40.0));
        QVERIFY(setDouble(QStringLiteral("construction_yokeAirGapLength_1_1"), 2.0));

        // Explicit thermal metadata; no cooling ducts or other air gaps are
        // invented. The single limb-A gap above is part of this synthetic fixture.
        QVERIFY(setDouble(QStringLiteral("construction_ambient"), 40.0));
        QVERIFY(setText(QStringLiteral("construction_insulationThermalStandard"),
            QStringLiteral("IEC 60085 (test metadata)")));
        QVERIFY(setDouble(QStringLiteral("construction_insulationClassReferenceTemperature"), 130.0));
        QVERIFY(setDouble(QStringLiteral("construction_insulationSafetyMargin"), 10.0));
        QVERIFY(setDouble(QStringLiteral("construction_maximumWindingTemperature"), 100.0));

        auto* confirmConstruction = window_->findChild<QPushButton*>(
            QStringLiteral("confirmConstructionAndContinue"));
        QVERIFY(confirmConstruction);
        QTest::mouseClick(confirmConstruction, Qt::LeftButton);
        QTRY_COMPARE_WITH_TIMEOUT(tabs->currentIndex(), 3, 5000);
        auto* confirmationStatus = window_->findChild<QLabel*>(
            QStringLiteral("constructionConfirmationStatus"));
        QVERIFY(confirmationStatus);
        QVERIFY(confirmationStatus->text().contains(QStringLiteral("confirmed"), Qt::CaseInsensitive));

        // Exercise the current plot with a non-zero Thevenin model. This changes
        // circuit element ordering and catches accidental fixed-index current reads.
        auto* sourceResistance = window_->findChild<QDoubleSpinBox*>(
            QStringLiteral("filterSourceTheveninResistanceOhm"));
        auto* sourceInductance = window_->findChild<QDoubleSpinBox*>(
            QStringLiteral("filterSourceTheveninInductanceMh"));
        QVERIFY(sourceResistance);
        QVERIFY(sourceInductance);
        sourceResistance->setValue(0.25);
        sourceInductance->setValue(1.2);

        auto* recalculate = window_->findChild<QPushButton*>(
            QStringLiteral("recalculateProjectButton"));
        QVERIFY(recalculate);
        QTest::mouseClick(recalculate, Qt::LeftButton);
        auto* results = window_->findChild<QTableWidget*>(
            QStringLiteral("calculationResultsTable"));
        QVERIFY(results);
        QTRY_VERIFY_WITH_TIMEOUT(results->rowCount() > 0, 10000);

        auto* currentPlot = window_->findChild<QWidget*>(
            QStringLiteral("resultsBankCurrentPlot"));
        QVERIFY(currentPlot);
        const auto currentCurves = currentPlot->property("plotCurves").toList();
        QCOMPARE(currentCurves.size(), 1);
        const auto currentPoints = currentCurves.front().toMap()
            .value(QStringLiteral("points")).toList();
        QCOMPARE(currentPoints.size(), 151);
        const double sourceR = 0.25;
        const double sourceL = 1.2e-3;
        const double phaseVoltage = 400.0 / std::sqrt(3.0);
        const double branchR = 0.00666655092793528;
        const double branchL = 0.0002122029067109523;
        const double branchC = 0.0019098924743825543;
        const auto expectedBranchPeak = [&](double frequencyHz) {
            const double omega = 2.0 * std::acos(-1.0) * frequencyHz;
            const std::complex<double> totalImpedance{
                sourceR + branchR,
                omega * (sourceL + branchL) - 1.0 / (omega * branchC)
            };
            return std::sqrt(2.0) * phaseVoltage / std::abs(totalImpedance);
        };
        for (const auto& pointValue : currentPoints) {
            const auto point = pointValue.toList();
            QCOMPARE(point.size(), 2);
            const double frequencyHz = point[0].toDouble();
            const double plottedPeakA = point[1].toDouble();
            QVERIFY(std::isfinite(frequencyHz) && frequencyHz > 0.0);
            QVERIFY(std::isfinite(plottedPeakA) && plottedPeakA >= 0.0);
            QVERIFY(qAbs(plottedPeakA - expectedBranchPeak(frequencyHz)) < 1.0e-6);
        }

        auto* impedancePlot = window_->findChild<QWidget*>(
            QStringLiteral("resultsBusImpedancePlot"));
        QVERIFY(impedancePlot);
        const auto impedanceCurves = impedancePlot->property("plotCurves").toList();
        QCOMPARE(impedanceCurves.size(), 1);
        const auto impedancePoints = impedanceCurves.front().toMap()
            .value(QStringLiteral("points")).toList();
        QCOMPARE(impedancePoints.size(), 501);
        for (const auto& pointValue : impedancePoints) {
            const auto point = pointValue.toList();
            QCOMPARE(point.size(), 2);
            const double frequencyHz = point[0].toDouble();
            const double plottedOhm = point[1].toDouble();
            const double omega = 2.0 * std::acos(-1.0) * frequencyHz;
            const std::complex<double> sourceZ{sourceR, omega * sourceL};
            const std::complex<double> filterZ{
                branchR, omega * branchL - 1.0 / (omega * branchC)
            };
            const double expectedBusOhm = std::abs(sourceZ * filterZ / (sourceZ + filterZ));
            QVERIFY(std::isfinite(plottedOhm) && plottedOhm >= 0.0);
            QVERIFY(qAbs(plottedOhm - expectedBusOhm) < 1.0e-9);
        }

        auto* currentTitle = window_->findChild<QLabel*>(
            QStringLiteral("inductorCurrentPlotTitle"));
        QVERIFY(currentTitle);
        QVERIFY(currentTitle->text().contains(QStringLiteral("Reactor branch peak current")));

        auto* coreFluxPlot = window_->findChild<QWidget*>(
            QStringLiteral("resultsCoreFluxPlot"));
        QVERIFY(coreFluxPlot);
        const auto fluxCurves = coreFluxPlot->property("plotCurves").toList();
        QCOMPARE(fluxCurves.size(), 4);

#if defined(_DEBUG) || !defined(NDEBUG)
        QSettings artifactSettings;
        const QString artifactProjectId =
            artifactSettings.value(QStringLiteral("project/currentId")).toString();
        QVERIFY(!artifactProjectId.isEmpty());
        const QDir artifactRoot(QStringLiteral("C:/Temp/trdos/135/Debug/Plots"));
        const QFileInfo currentPng(artifactRoot.filePath(
            artifactProjectId + QStringLiteral("_reactor_current.png")));
        const QFileInfo impedancePng(artifactRoot.filePath(
            artifactProjectId + QStringLiteral("_bus_impedance.png")));
        const QFileInfo coreFluxPng(artifactRoot.filePath(
            artifactProjectId + QStringLiteral("_core_flux.png")));
        auto* historyPlot = window_->findChild<QWidget*>(
            QStringLiteral("historyComparisonPlot"));
        QVERIFY(historyPlot);
        const QFileInfo historyPng(artifactRoot.filePath(
            artifactProjectId + QStringLiteral("_history_comparison.png")));
        const std::array<QFileInfo, 4> plotFiles{
            currentPng, impedancePng, coreFluxPng, historyPng
        };
        const std::array<QWidget*, 4> plotWidgets{
            window_->findChild<QWidget*>(QStringLiteral("resultsBankCurrentPlot")),
            window_->findChild<QWidget*>(QStringLiteral("resultsBusImpedancePlot")),
            coreFluxPlot, historyPlot
        };
        for (std::size_t i = 0; i < plotFiles.size(); ++i) {
            QVERIFY2(plotFiles[i].exists() && plotFiles[i].size() > 100,
                "A Debug plot PNG was not automatically saved.");
            QVERIFY(plotWidgets[i]);
            QImage image(plotFiles[i].absoluteFilePath());
            QVERIFY2(!image.isNull(), "Saved Debug plot is not a decodable PNG.");
            QVERIFY(image.width() >= 120);
            QVERIFY(image.height() >= 100);
            QCOMPARE(image.size(), plotWidgets[i]->size());
            // An empty-data plot must still be a rendered diagnostic image,
            // not a fully transparent placeholder.
            QVERIFY(image.pixelColor(image.width() / 2, image.height() / 2).alpha() > 0);
        }
#endif
        for (const auto& curve : fluxCurves)
            QVERIFY(curve.toMap().value(QStringLiteral("points")).toList().isEmpty());

        auto* confirmCalculation = window_->findChild<QPushButton*>(
            QStringLiteral("confirmCalculationDesign"));
        QVERIFY(confirmCalculation);
        QTest::mouseClick(confirmCalculation, Qt::LeftButton);
        QTRY_COMPARE_WITH_TIMEOUT(tabs->currentIndex(), 4, 5000);

        auto* generateCad = window_->findChild<QPushButton*>(
            QStringLiteral("generatePreliminaryCad"));
        QVERIFY(generateCad);
        auto* cadStatus = window_->findChild<QLabel*>(
            QStringLiteral("cadGenerationStatus"));
        QVERIFY(cadStatus);
        const QString initialCadStatus = cadStatus->text();
        QTest::mouseClick(generateCad, Qt::LeftButton);
        QTRY_VERIFY_WITH_TIMEOUT(cadStatus->text() != initialCadStatus, 5000);
        QVERIFY2(cadStatus->text().contains(QStringLiteral("Preliminary CAD generated")),
            qPrintable(QStringLiteral("CAD generation did not succeed. Actual status: %1")
                .arg(cadStatus->text())));

        auto* assemblyTree = window_->findChild<QTreeWidget*>(
            QStringLiteral("cadAssemblyTree"));
        QVERIFY(assemblyTree);
        QCOMPARE(assemblyTree->topLevelItemCount(), 1);
        auto* root = assemblyTree->topLevelItem(0);
        QVERIFY(root);
        QCOMPARE(root->childCount(), 2);
        auto* core = root->child(0);
        auto* windingGroup = root->child(1);
        QVERIFY(core);
        QVERIFY(windingGroup);
        QCOMPARE(core->childCount(), 7); // Three limbs and four yoke segments.
        QCOMPARE(windingGroup->childCount(), 3);
        for (int phase = 0; phase < 3; ++phase) {
            auto* phaseNode = windingGroup->child(phase);
            QVERIFY(phaseNode);
            QCOMPARE(phaseNode->childCount(), 2); // Bobbin + winding envelope.
        }
        auto* exportStep = window_->findChild<QPushButton*>(
            QStringLiteral("exportCadStep"));
        QVERIFY(exportStep);
        QVERIFY(exportStep->isEnabled());

        QSettings cadArtifactSettings;
        const QString cadArtifactProjectId =
            cadArtifactSettings.value(QStringLiteral("project/currentId")).toString();
        QVERIFY(!cadArtifactProjectId.isEmpty());
        const QString automaticStepPath = QDir(QStringLiteral("C:/Temp/trdos/135/CAD"))
            .filePath(cadArtifactProjectId + QStringLiteral("_assembly.step"));
        const QString automaticPdfPath = QDir(QStringLiteral("C:/Temp/trdos/135/Drawings"))
            .filePath(cadArtifactProjectId + QStringLiteral("_assembly.pdf"));
        QVERIFY2(QFileInfo::exists(automaticStepPath),
            "CAD generation did not automatically save the STEP assembly.");
        QVERIFY2(QFileInfo(automaticStepPath).size() > 0,
            "Automatically saved STEP assembly is empty.");
        QVERIFY2(QFileInfo::exists(automaticPdfPath),
            "CAD generation did not automatically save the PDF drawing.");
        QVERIFY2(QFileInfo(automaticPdfPath).size() > 100,
            "Automatically saved PDF drawing is empty.");
        QFile pdfFile(automaticPdfPath);
        QVERIFY(pdfFile.open(QIODevice::ReadOnly));
        QVERIFY(pdfFile.read(5).startsWith("%PDF-"));

        // Exercise the real UI export action. The timer accepts the widget-backed
        // save dialog with a deterministic temporary path; the export itself
        // still runs through MainWindow's production click handler.
        QTemporaryDir exportDirectory;
        QVERIFY2(exportDirectory.isValid(), "Could not create temporary STEP export directory");
        const QString stepPath = exportDirectory.filePath(QStringLiteral("TEST-01-synthetic.step"));
        QTimer::singleShot(0, [stepPath] {
            for (QWidget* widget : QApplication::topLevelWidgets()) {
                auto* dialog = qobject_cast<QFileDialog*>(widget);
                if (!dialog || !dialog->isVisible()) continue;
                dialog->selectFile(stepPath);
                QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection);
                return;
            }
        });
        QTest::mouseClick(exportStep, Qt::LeftButton);

        const QFileInfo stepFile(stepPath);
        QVERIFY2(stepFile.exists(), "STEP export action did not create the requested file");
        QVERIFY2(stepFile.size() > 0, "STEP export produced an empty file");

        STEPControl_Reader stepReader;
        QCOMPARE(stepReader.ReadFile(stepPath.toStdString().c_str()), IFSelect_RetDone);
        QVERIFY(stepReader.TransferRoots() > 0);
        const TopoDS_Shape importedAssembly = stepReader.OneShape();
        QVERIFY(!importedAssembly.IsNull());
        QVERIFY2(BRepCheck_Analyzer(importedAssembly).IsValid(),
            "STEP round-trip produced an invalid B-Rep");

        std::size_t importedSolidCount = 0;
        for (TopExp_Explorer it(importedAssembly, TopAbs_SOLID); it.More(); it.Next())
            ++importedSolidCount;
        QCOMPARE(importedSolidCount, std::size_t(15)); // 9 core solids (limb A and top yoke AB each split once) + 3 bobbins + 3 winding envelopes.

        QVERIFY2(cadStatus->text().contains(QStringLiteral("exported to")),
            qPrintable(QStringLiteral("Unexpected STEP export status: ") + cadStatus->text()));

        // Generate the 2D drawing through the actual CAD page action. The
        // DXF must contain the three orthographic views and revision disclaimer.
        auto* drawingAction = window_->findChild<QPushButton*>(
            QStringLiteral("generateCadDrawings"));
        QVERIFY(drawingAction);
        QVERIFY(drawingAction->isEnabled());
        const QString dxfPath = exportDirectory.filePath(QStringLiteral("TEST-01-synthetic.dxf"));
        QTimer::singleShot(0, [dxfPath] {
            for (QWidget* widget : QApplication::topLevelWidgets()) {
                auto* dialog = qobject_cast<QFileDialog*>(widget);
                if (!dialog || !dialog->isVisible()) continue;
                dialog->selectFile(dxfPath);
                QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection);
                return;
            }
        });
        QTest::mouseClick(drawingAction, Qt::LeftButton);
        const QFileInfo dxfFile(dxfPath);
        QVERIFY2(dxfFile.exists(), "2D drawing action did not create the requested DXF file");
        QVERIFY2(dxfFile.size() > 0, "2D drawing export produced an empty DXF file");
        std::ifstream dxfStream(dxfPath.toStdString(), std::ios::binary);
        QVERIFY(dxfStream.good());
        const std::string dxfContent(
            (std::istreambuf_iterator<char>(dxfStream)), std::istreambuf_iterator<char>());
        QVERIFY(dxfContent.find("PROJECT135 - THREE-PHASE REACTOR") != std::string::npos);
        QVERIFY(dxfContent.find("A - FRONT ELEVATION") != std::string::npos);
        QVERIFY(dxfContent.find("B - PLAN VIEW") != std::string::npos);
        QVERIFY(dxfContent.find("C - SIDE ELEVATION") != std::string::npos);
        QVERIFY(dxfContent.find("$INSUNITS") != std::string::npos);
        QVERIFY(dxfContent.find("OVERALL WIDTH") != std::string::npos);
        QVERIFY(dxfContent.find("OVERALL HEIGHT") != std::string::npos);
        QVERIFY(dxfContent.find("LIMB A WIDTH") != std::string::npos);
        QVERIFY(dxfContent.find("LIMB B WIDTH") != std::string::npos);
        QVERIFY(dxfContent.find("LIMB C WIDTH") != std::string::npos);
        QVERIFY(dxfContent.find("WINDOW AB") != std::string::npos);
        QVERIFY(dxfContent.find("WINDOW BC") != std::string::npos);
        QVERIFY(dxfContent.find("Yoke thickness T1/T2/B1/B2") != std::string::npos);
        QVERIFY(dxfContent.find("YOKE TOP AB PLAN") != std::string::npos);
        QVERIFY(dxfContent.find("YOKE TOP BC PLAN") != std::string::npos);
        QVERIFY(dxfContent.find("YOKE BOTTOM AB PLAN") != std::string::npos);
        QVERIFY(dxfContent.find("YOKE BOTTOM BC PLAN") != std::string::npos);
        QVERIFY(dxfContent.find("AIR GAP SCHEDULE (2)") != std::string::npos);
        QVERIFY(dxfContent.find("LIMB A - position 80.000000 mm; length 2.000000 mm") != std::string::npos);
        QVERIFY(dxfContent.find("YOKE TOP AB - position 40.000000 mm; length 2.000000 mm") != std::string::npos);
        QVERIFY(dxfContent.find("Limb stack depths A/B/C") != std::string::npos);
        QVERIFY(dxfContent.find("BOBBIN B AXIAL LENGTH") != std::string::npos);
        QVERIFY(dxfContent.find("BOBBIN B DEPTH") != std::string::npos);
        QVERIFY(dxfContent.find("WINDING_ENVELOPE") != std::string::npos);
        QVERIFY(dxfContent.find("PRELIMINARY / NOT VERIFIED") != std::string::npos);
        QVERIFY(dxfContent.find("TEST-01-SYNTHETIC-GEOMETRY-R1") != std::string::npos);
        QVERIFY(cadStatus->text().contains(QStringLiteral("DXF")));
    }

    void twoDimensionalDrawingStageIsPresent()
    {
        const auto* drawingAction = window_->findChild<QPushButton*>(
            QStringLiteral("generateCadDrawings"));
        QVERIFY2(drawingAction,
            "Expected a 2D drawing generation action on the CAD page.");
        QVERIFY(!drawingAction->isEnabled());
    }
};

QTEST_MAIN(DesktopWorkflowE2ETests)
#include "workflow_e2e_tests.moc"
