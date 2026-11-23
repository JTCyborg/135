#include "main_window.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDebug>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QDialog>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QAbstractButton>
#include <QAction>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
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
#include <QGroupBox>
#include <QWheelEvent>
#include <QToolButton>

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
        // QFileDialog's native Windows implementation is not exposed as a
        // QWidget, so the deterministic modal-dialog driver below cannot find it.
        // Use Qt's widget dialog for this E2E test only.
        QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs, true);
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
        auto* estimatedBh = window_->findChild<QCheckBox*>(
            QStringLiteral("construction_allowEstimatedBhModel"));
        QVERIFY2(estimatedBh, "Missing explicit estimated B-H model opt-in");
        QVERIFY(!estimatedBh->isChecked());
        QVERIFY(!estimatedBh->toolTip().isEmpty());
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

    void automaticReactorSizingAssumptionsAreExplicitAndEditable()
    {
        auto* manual = window_->findChild<QToolButton*>(
            QStringLiteral("filterManualMode"));
        auto* automatic = window_->findChild<QToolButton*>(
            QStringLiteral("filterAutomaticMode"));
        auto* assumptions = window_->findChild<QGroupBox*>(
            QStringLiteral("idealSizingAssumptionsGroup"));
        QVERIFY(manual);
        QVERIFY(automatic);
        QVERIFY(assumptions);
        QVERIFY(assumptions->isHidden());
        QTest::mouseClick(manual, Qt::LeftButton);
        QTRY_VERIFY(!assumptions->isHidden());

        const auto checkAssumption = [this](const QString& objectName, double expected) {
            auto* field = window_->findChild<QDoubleSpinBox*>(objectName);
            QVERIFY2(field, qPrintable(QStringLiteral("Missing field: ") + objectName));
            QCOMPARE(field->value(), expected);
            QCOMPARE(field->property("p135ValueOrigin").toString(),
                     QStringLiteral("PRELIMINARY_ASSUMPTION"));
            QVERIFY(!field->toolTip().isEmpty());
            const double editedValue = expected + 0.1;
            field->setValue(editedValue);
            QVERIFY(std::abs(field->value() - editedValue) < 1.0e-9);
        };

        checkAssumption(QStringLiteral("idealSizing_Bmax"), 1.2);
        checkAssumption(QStringLiteral("idealSizing_J"), 3.0);
        checkAssumption(QStringLiteral("idealSizing_Ku"), 35.0);
        checkAssumption(QStringLiteral("idealSizing_Ks"), 95.0);
        checkAssumption(QStringLiteral("idealSizing_AwAe"), 2.0);
        auto* fluxLimit = window_->findChild<QDoubleSpinBox*>(
            QStringLiteral("idealSizing_Bmax"));
        QVERIFY(fluxLimit);
        QTest::mouseClick(automatic, Qt::LeftButton);
        QTRY_VERIFY(assumptions->isHidden());
        QTest::mouseClick(manual, Qt::LeftButton);
        QTRY_VERIFY(!assumptions->isHidden());
        QVERIFY(std::abs(fluxLimit->value() - 1.3) < 1.0e-9);
        QTest::mouseClick(automatic, Qt::LeftButton);
        QTRY_VERIFY(assumptions->isHidden());
    }

    void coreMaterialBhCurveImportIsStructuredAndProvenanced()
    {
        auto* tabs = window_->findChild<QTabWidget*>();
        QVERIFY(tabs);
        tabs->setCurrentIndex(5);
        auto* create = window_->findChild<QPushButton*>(
            QStringLiteral("materialNewButton"));
        QVERIFY(create);

        QTimer::singleShot(0, [this] {
            auto* materialDialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            if (!materialDialog) {
                QFAIL("New material dialog did not open");
                return;
            }
            auto* import = materialDialog->findChild<QPushButton*>(
                QStringLiteral("importBhCurveButton"));
            auto* payload = materialDialog->findChild<QPlainTextEdit*>(
                QStringLiteral("materialPayloadJson"));
            auto* materialId = materialDialog->findChild<QLineEdit*>(
                QStringLiteral("materialId"));
            auto* coreEditor = materialDialog->findChild<QPushButton*>(
                QStringLiteral("coreMaterialEditorButton"));
            if (!import || !import->isEnabled() || !payload || !materialId ||
                !coreEditor || !coreEditor->isEnabled()) {
                materialDialog->reject();
                QFAIL("New core material dialog is missing the core editor, B-H import action or payload editor");
                return;
            }
            materialId->setText(QStringLiteral("MAT-TEST-CORE-001"));
            QTimer::singleShot(0, [this, materialDialog] {
                auto* coreDialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
                if (!coreDialog) {
                    materialDialog->reject();
                    QFAIL("Core material editor dialog did not open");
                    return;
                }
                auto* manufacturer = coreDialog->findChild<QLineEdit*>(
                    QStringLiteral("coreMaterialManufacturer"));
                auto* grade = coreDialog->findChild<QLineEdit*>(
                    QStringLiteral("coreMaterialGrade"));
                auto* density = coreDialog->findChild<QDoubleSpinBox*>(
                    QStringLiteral("coreMaterialDensity"));
                auto* family = coreDialog->findChild<QComboBox*>(
                    QStringLiteral("coreMaterialFamily"));
                if (!manufacturer || !grade || !density || !family) {
                    coreDialog->reject();
                    materialDialog->reject();
                    QFAIL("Core material editor is missing required identity fields");
                    return;
                }
                manufacturer->setText(QStringLiteral("TEST-MANUFACTURER"));
                grade->setText(QStringLiteral("TEST-CORE-GRADE"));
                density->setValue(7600.0);
                const int familyIndex = family->findData(
                    QStringLiteral("non-grain-oriented electrical steel"));
                if (familyIndex < 0) {
                    coreDialog->reject();
                    materialDialog->reject();
                    QFAIL("Non-grain-oriented electrical steel option is missing");
                    return;
                }
                family->setCurrentIndex(familyIndex);
                coreDialog->accept();
            });
            QTest::mouseClick(coreEditor, Qt::LeftButton);
            const auto basePayload = QJsonDocument::fromJson(payload->toPlainText().toUtf8());
            if (!basePayload.isObject() ||
                basePayload.object().value(QStringLiteral("schemaVersion")).toInt() != 1 ||
                !basePayload.object().value(QStringLiteral("material")).isObject()) {
                materialDialog->reject();
                QFAIL("Core material editor did not create a valid revision-loader payload");
                return;
            }

            QTimer::singleShot(0, [this, materialDialog] {
                auto* bhDialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
                if (!bhDialog) {
                    materialDialog->reject();
                    QFAIL("B-H curve import dialog did not open");
                    return;
                }
                auto* source = bhDialog->findChild<QLineEdit*>(
                    QStringLiteral("bhCurveSourceDocument"));
                auto* points = bhDialog->findChild<QPlainTextEdit*>(
                    QStringLiteral("bhCurvePoints"));
                auto* quality = bhDialog->findChild<QComboBox*>(
                    QStringLiteral("bhCurveQualityStatus"));
                auto* curveType = bhDialog->findChild<QComboBox*>(
                    QStringLiteral("bhCurveType"));
                auto* frequency = bhDialog->findChild<QDoubleSpinBox*>(
                    QStringLiteral("bhCurveFrequency"));
                auto* temperature = bhDialog->findChild<QDoubleSpinBox*>(
                    QStringLiteral("bhCurveTemperature"));
                if (!source || !points || !quality || !curveType ||
                    !frequency || !temperature) {
                    bhDialog->reject();
                    materialDialog->reject();
                    QFAIL("B-H import dialog is missing required controls");
                    return;
                }
                if (quality->currentData().toString() !=
                    QStringLiteral("APPROXIMATE_DIGITIZATION")) {
                    bhDialog->reject();
                    materialDialog->reject();
                    QFAIL("New B-H imports must default to exploratory, not verified, quality");
                    return;
                }
                source->setText(QStringLiteral("TEST-MANUFACTURER-DATASHEET-R1"));
                const int dynamicIndex = curveType->findData(
                    QStringLiteral("dynamic magnetization"));
                if (dynamicIndex < 0) {
                    bhDialog->reject();
                    materialDialog->reject();
                    QFAIL("Dynamic B-H curve type option is missing");
                    return;
                }
                curveType->setCurrentIndex(dynamicIndex);
                frequency->setValue(50.0);
                temperature->setValue(293.15);
                points->setPlainText(QStringLiteral(
                    "\uFEFFH_A_per_m, B_T\n0, 0\n100, 0.25\n200, 0.40"));
                const int manufacturerIndex = quality->findData(
                    QStringLiteral("MANUFACTURER_TABULATED"));
                if (manufacturerIndex < 0) {
                    bhDialog->reject();
                    materialDialog->reject();
                    QFAIL("Manufacturer tabulated quality option is missing");
                    return;
                }
                quality->setCurrentIndex(manufacturerIndex);
                bhDialog->accept();
            });

            QTest::mouseClick(import, Qt::LeftButton);
            const auto document = QJsonDocument::fromJson(payload->toPlainText().toUtf8());
            if (!document.isObject()) {
                materialDialog->reject();
                QFAIL("B-H import did not produce a valid JSON object");
                return;
            }
            const auto root = document.object();
            const auto curve = root.value(QStringLiteral("bhCurve")).toObject();
            if (curve.value(QStringLiteral("frequency_Hz")).toDouble() != 50.0 ||
                curve.value(QStringLiteral("points")).toArray().size() != 3 ||
                root.value(QStringLiteral("dataQuality")).toObject()
                    .value(QStringLiteral("bhCurveStatus")).toString() !=
                        QStringLiteral("MANUFACTURER_TABULATED") ||
                root.value(QStringLiteral("bhCurveProvenance")).toObject()
                    .value(QStringLiteral("sourceDocument")).toString() !=
                        QStringLiteral("TEST-MANUFACTURER-DATASHEET-R1") ||
                root.value(QStringLiteral("dataQuality")).toObject()
                    .value(QStringLiteral("designReleaseStatus")).toString() !=
                        QStringLiteral("NOT_VERIFIED")) {
                QFAIL("B-H curve data or provenance fields were not serialized as expected");
                materialDialog->reject();
                return;
            }
            auto* coreLossImport = materialDialog->findChild<QPushButton*>(
                QStringLiteral("importCoreLossTableButton"));
            if (!coreLossImport || !coreLossImport->isEnabled()) {
                materialDialog->reject();
                QFAIL("Core-loss table import action is missing or disabled");
                return;
            }
            QTimer::singleShot(0, [this, materialDialog] {
                auto* lossDialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
                if (!lossDialog) {
                    materialDialog->reject();
                    QFAIL("Core-loss import dialog did not open");
                    return;
                }
                auto* source = lossDialog->findChild<QLineEdit*>(QStringLiteral("coreLossSourceDocument"));
                auto* method = lossDialog->findChild<QLineEdit*>(QStringLiteral("coreLossMeasurementMethod"));
                auto* direction = lossDialog->findChild<QLineEdit*>(QStringLiteral("coreLossSampleDirection"));
                auto* points = lossDialog->findChild<QPlainTextEdit*>(QStringLiteral("coreLossPoints"));
                auto* quality = lossDialog->findChild<QComboBox*>(QStringLiteral("coreLossQualityStatus"));
                auto* waveform = lossDialog->findChild<QComboBox*>(QStringLiteral("coreLossWaveform"));
                auto* temperatureStatus = lossDialog->findChild<QComboBox*>(QStringLiteral("coreLossTemperatureStatus"));
                if (!source || !method || !direction || !points || !quality ||
                    !waveform || !temperatureStatus) {
                    lossDialog->reject();
                    materialDialog->reject();
                    QFAIL("Core-loss import dialog is missing provenance or data controls");
                    return;
                }
                source->setText(QStringLiteral("TEST-CORE-LOSS-DATASHEET-R1"));
                method->setText(QStringLiteral("Epstein frame test"));
                direction->setText(QStringLiteral("Rolling direction"));
                const int manufacturerIndex = quality->findData(QStringLiteral("MANUFACTURER_TABULATED"));
                if (manufacturerIndex < 0) {
                    lossDialog->reject();
                    materialDialog->reject();
                    QFAIL("Manufacturer core-loss quality option is missing");
                    return;
                }
                quality->setCurrentIndex(manufacturerIndex);
                points->setPlainText(QStringLiteral(
                    "frequency_Hz, peakFluxDensity_T, temperature_K, specificLoss_W_per_m3\n"
                    "50, 0.8, 293.15, 10000\n"
                    "50, 0.8, 333.15, 11000\n"
                    "50, 1.0, 293.15, 20000\n"
                    "50, 1.0, 333.15, 22000\n"
                    "100, 0.8, 293.15, 30000\n"
                    "100, 0.8, 333.15, 33000\n"
                    "100, 1.0, 293.15, 60000\n"
                    "100, 1.0, 333.15, 66000"));
                lossDialog->accept();
            });
            QTest::mouseClick(coreLossImport, Qt::LeftButton);
            const auto withLoss = QJsonDocument::fromJson(payload->toPlainText().toUtf8());
            const auto lossRoot = withLoss.object();
            const auto lossData = lossRoot.value(QStringLiteral("coreLossData")).toObject();
            const auto lossProvenance = lossRoot.value(QStringLiteral("coreLossProvenance")).toObject();
            if (!withLoss.isObject() ||
                lossData.value(QStringLiteral("points")).toArray().size() != 8 ||
                lossData.value(QStringLiteral("points")).toArray().at(0).toObject()
                    .value(QStringLiteral("specificLoss_W_per_m3")).toDouble() != 10000.0 ||
                lossProvenance.value(QStringLiteral("sourceDocument")).toString() !=
                    QStringLiteral("TEST-CORE-LOSS-DATASHEET-R1") ||
                lossProvenance.value(QStringLiteral("waveform")).toString().isEmpty() ||
                lossRoot.value(QStringLiteral("dataQuality")).toObject()
                    .value(QStringLiteral("coreLossStatus")).toString() !=
                    QStringLiteral("MANUFACTURER_TABULATED") ||
                lossRoot.value(QStringLiteral("dataQuality")).toObject()
                    .value(QStringLiteral("designReleaseStatus")).toString() !=
                    QStringLiteral("NOT_VERIFIED")) {
                materialDialog->reject();
                QFAIL("Core-loss grid or provenance was not serialized correctly");
                return;
            }
            materialDialog->reject();
        });

        QTest::mouseClick(create, Qt::LeftButton);
    }

    void filterInputModesAndWheelProtection()
    {
        auto* automatic = window_->findChild<QToolButton*>(
            QStringLiteral("filterAutomaticMode"));
        auto* manual = window_->findChild<QToolButton*>(
            QStringLiteral("filterManualMode"));
        auto* componentGroup = window_->findChild<QGroupBox*>(
            QStringLiteral("filterComponentManualGroup"));
        auto* advancedGroup = window_->findChild<QGroupBox*>(
            QStringLiteral("filterAdvancedManualGroup"));
        QVERIFY(automatic);
        QVERIFY(manual);
        QVERIFY(componentGroup);
        QVERIFY(advancedGroup);

        QVERIFY(automatic->isChecked());
        QVERIFY(componentGroup->isHidden());
        QVERIFY(advancedGroup->isHidden());

        QTest::mouseClick(manual, Qt::LeftButton);
        QTRY_VERIFY(!componentGroup->isHidden());
        QTRY_VERIFY(!advancedGroup->isHidden());
        QTest::mouseClick(automatic, Qt::LeftButton);
        QTRY_VERIFY(componentGroup->isHidden());
        QTRY_VERIFY(advancedGroup->isHidden());

        auto* voltage = window_->findChild<QDoubleSpinBox*>(
            QStringLiteral("filterNominalVoltageV"));
        QVERIFY(voltage);
        const double original = voltage->value();
        QWheelEvent wheel(
            QPointF(2.0, 2.0),
            QPointF(voltage->mapToGlobal(QPoint(2, 2))),
            QPoint(), QPoint(0, 120), Qt::NoButton, Qt::NoModifier,
            Qt::NoScrollPhase, false);
        QApplication::sendEvent(voltage, &wheel);
        QCOMPARE(voltage->value(), original);
    }

    void projectFilterAndConstructionConfirmationsAdvance()
    {
        auto* tabs = window_->findChild<QTabWidget*>();
        QVERIFY(tabs);

        auto clickAndWait = [this, tabs](const char* objectName, int expectedIndex) {
            auto* button = window_->findChild<QPushButton*>(QString::fromLatin1(objectName));
            QVERIFY2(button, objectName);
            QTimer::singleShot(100, this, [this, objectName] {
                if (auto* dialog = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
                    qInfo().noquote() << "[E2E-DIAG] modal while confirming"
                        << objectName << "title=" << dialog->windowTitle()
                        << "text=" << dialog->text()
                        << "informative=" << dialog->informativeText();
                    dialog->accept();
                }
            });
            QTest::mouseClick(button, Qt::LeftButton);
            QTRY_COMPARE_WITH_TIMEOUT(tabs->currentIndex(), expectedIndex, 3000);
        };

        clickAndWait("confirmProjectAndContinue", 1);
        auto* synthesize = window_->findChild<QPushButton*>(QStringLiteral("synthesizeLcBranch"));
        QVERIFY(synthesize);
        QTest::mouseClick(synthesize, Qt::LeftButton);
        qInfo().noquote() << "[E2E-DIAG] after filter synthesis"
            << "resultsNote=" << window_->findChild<QLabel*>(QStringLiteral("calculationResultsNote"))->text()
            << "resultRows=" << window_->findChild<QTableWidget*>(QStringLiteral("calculationResultsTable"))->rowCount()
            << "constructionState=" << window_->findChild<QLabel*>(QStringLiteral("constructionInputState"))->text();
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
        qInfo().noquote() << "[E2E-DIAG] after filter synthesis"
            << "resultsNote=" << window_->findChild<QLabel*>(QStringLiteral("calculationResultsNote"))->text()
            << "resultRows=" << window_->findChild<QTableWidget*>(QStringLiteral("calculationResultsTable"))->rowCount()
            << "constructionState=" << window_->findChild<QLabel*>(QStringLiteral("constructionInputState"))->text();
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

        auto* idealSizing = window_->findChild<QLabel*>(
            QStringLiteral("idealSizingResult"));
        QVERIFY2(idealSizing, "Automatic ideal sizing result label is missing");
        QTRY_VERIFY_WITH_TIMEOUT(
            idealSizing->text().contains(QStringLiteral("Gross Ae×Aw")),
            10000);
        QVERIFY(!idealSizing->text().contains(QStringLiteral("NOT CALCULATED")));

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
        qInfo().noquote() << "[E2E-DIAG] after filter synthesis"
            << "resultsNote=" << window_->findChild<QLabel*>(QStringLiteral("calculationResultsNote"))->text()
            << "resultRows=" << window_->findChild<QTableWidget*>(QStringLiteral("calculationResultsTable"))->rowCount()
            << "constructionState=" << window_->findChild<QLabel*>(QStringLiteral("constructionInputState"))->text();

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
        const auto setPlainText = [this](const QString& name, const QString& value) {
            auto* field = window_->findChild<QPlainTextEdit*>(name);
            if (!field) return false;
            field->setPlainText(value);
            return field->toPlainText() == value;
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
        QTimer::singleShot(100, this, [this] {
            if (auto* dialog = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
                qInfo().noquote() << "[E2E-DIAG] modal while confirming synthetic TEST-01"
                    << "title=" << dialog->windowTitle()
                    << "text=" << dialog->text()
                    << "informative=" << dialog->informativeText();
                dialog->accept();
            }
        });
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

        // Supply an explicit fifth-harmonic load-current injection so the
        // Construction-bound magnetic bridge receives a real branch harmonic
        // result rather than silently exercising only the fundamental.
        auto* loadCurrentHarmonics = window_->findChild<QTableWidget*>(
            QStringLiteral("filterLoadCurrentHarmonics"));
        QVERIFY(loadCurrentHarmonics);
        loadCurrentHarmonics->setRowCount(1);
        loadCurrentHarmonics->setColumnCount(5);
        loadCurrentHarmonics->setItem(0, 0, new QTableWidgetItem(QStringLiteral("E2E-LOAD-1")));
        loadCurrentHarmonics->setItem(0, 1, new QTableWidgetItem(QStringLiteral("5")));
        loadCurrentHarmonics->setItem(0, 2, new QTableWidgetItem(QStringLiteral("Negative")));
        loadCurrentHarmonics->setItem(0, 3, new QTableWidgetItem(QStringLiteral("3.25")));
        loadCurrentHarmonics->setItem(0, 4, new QTableWidgetItem(QStringLiteral("-15.0")));

        // Explicit synthetic magnetic inputs exercise the real coupled
        // three-limb operating-point solver and its Results chart. These are
        // regression-fixture values only, not manufacturer material data or
        // an engineering-approved core design.
        QVERIFY(setDouble(QStringLiteral("coreEffectiveAreaMm2"), 10000.0));
        QVERIFY(setDouble(QStringLiteral("coreMeanMagneticPathMm"), 300.0));
        QVERIFY(setDouble(QStringLiteral("coreTotalAirGapPerLimbMm"), 10.0));
        QVERIFY(setDouble(QStringLiteral("coreYokeEffectiveAreaMm2"), 10000.0));
        QVERIFY(setDouble(QStringLiteral("coreYokeSegmentPathMm"), 200.0));
        QVERIFY(setInteger(QStringLiteral("filterInductorTurnsPerPhase"), 1));
        QVERIFY(setText(QStringLiteral("coreBhCurveEvidence"),
            QStringLiteral("TEST-01 synthetic B-H curve; regression fixture only")));
        QVERIFY(setPlainText(QStringLiteral("coreBhCurvePoints"),
            QStringLiteral("0, 0\n1000, 0.1\n10000, 0.8\n50000, 1.4\n100000, 1.55")));

        auto* recalculate = window_->findChild<QPushButton*>(
            QStringLiteral("recalculateProjectButton"));
        QVERIFY(recalculate);
        QTest::mouseClick(recalculate, Qt::LeftButton);
        auto* results = window_->findChild<QTableWidget*>(
            QStringLiteral("calculationResultsTable"));
        QVERIFY(results);
        QTRY_VERIFY_WITH_TIMEOUT(results->rowCount() > 0, 10000);

        // Recalculation publishes a new immutable filter snapshot after the
        // source-impedance and harmonic inputs above were edited. Re-confirm
        // Construction against that exact snapshot before asserting any
        // revision-bound winding result; stale Construction must stay blocked.
        tabs->setCurrentIndex(2);
        QTRY_COMPARE(tabs->currentIndex(), 2);
        confirmConstruction = window_->findChild<QPushButton*>(
            QStringLiteral("confirmConstructionAndContinue"));
        QVERIFY(confirmConstruction);
        QTimer::singleShot(100, this, [this] {
            if (auto* dialog = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))
                dialog->accept();
        });
        QTest::mouseClick(confirmConstruction, Qt::LeftButton);
        QTRY_COMPARE_WITH_TIMEOUT(tabs->currentIndex(), 3, 5000);
        confirmationStatus = window_->findChild<QLabel*>(
            QStringLiteral("constructionConfirmationStatus"));
        QVERIFY(confirmationStatus);
        QVERIFY(confirmationStatus->text().contains(QStringLiteral("confirmed"), Qt::CaseInsensitive));
        QTest::mouseClick(recalculate, Qt::LeftButton);
        QTRY_VERIFY_WITH_TIMEOUT(results->rowCount() > 0, 10000);

        // The accepted Construction snapshot, pinned M235-35A material revision,
        // explicit turns and harmonic branch result reach the revision-bound
        // magnetic bridge. M235-35A is exploratory (sparse-anchor estimate), and
        // the opt-in is deliberately off: the physical result must stay
        // NOT_VERIFIED and must not publish a Construction-bound peak |B|.
        auto* calculationLog = window_->findChild<QPlainTextEdit*>(
            QStringLiteral("calculationActivityLog"));
        QVERIFY(calculationLog);
        const QString magneticLog = calculationLog->toPlainText();
        QVERIFY2(magneticLog.contains(QStringLiteral(
            "Construction-bound magnetic analysis NOT_VERIFIED")),
            qPrintable(magneticLog));
        QVERIFY2(magneticLog.contains(QStringLiteral("allowEstimatedBhModel")),
            qPrintable(magneticLog));
        QVERIFY(!magneticLog.contains(QStringLiteral(
            "[MAGNETIC] PASS — Construction TEST-01-SYNTHETIC-GEOMETRY-R1")));
        QVERIFY(!magneticLog.contains(QStringLiteral(
            "Construction TEST-01-SYNTHETIC-GEOMETRY-R1; material SEED-TK-M235-35A-R1; B-H model ESTIMATED")));

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
        QCOMPARE(impedanceCurves.size(), 2);
        const auto impedancePoints = impedanceCurves[0].toMap()
            .value(QStringLiteral("points")).toList();
        const auto gridImpedancePoints = impedanceCurves[1].toMap()
            .value(QStringLiteral("points")).toList();
        QCOMPARE(impedancePoints.size(), 1001);
        QCOMPARE(gridImpedancePoints.size(), 1001);
        QVERIFY(impedancePoints.front().toList()[0].toDouble() == 10.0);
        QVERIFY(impedancePoints.back().toList()[0].toDouble() >= 2500.0);
        const auto canonicalMarkers = impedancePlot->property("plotFrequencyMarkers").toList();
        QCOMPARE(canonicalMarkers.size(), 9);
        bool hasSeriesTuningMarker = false;
        for (const auto& markerValue : canonicalMarkers) {
            const auto marker = markerValue.toMap();
            if (!marker.value(QStringLiteral("label")).toString()
                    .contains(QStringLiteral("series tune")))
                continue;
            hasSeriesTuningMarker = true;
            QVERIFY(qAbs(marker.value(QStringLiteral("frequencyHz")).toDouble() -
                         250.0) < 1.0e-9);
        }
        QVERIFY(hasSeriesTuningMarker);
        QVERIFY(impedanceCurves[1].toMap().value(QStringLiteral("dashed")).toBool());
        for (std::size_t sample = 0; sample < impedancePoints.size(); ++sample) {
            const auto point = impedancePoints[static_cast<qsizetype>(sample)].toList();
            const auto gridPoint = gridImpedancePoints[static_cast<qsizetype>(sample)].toList();
            QCOMPARE(point.size(), 2);
            QCOMPARE(gridPoint.size(), 2);
            const double frequencyHz = point[0].toDouble();
            QCOMPARE(gridPoint[0].toDouble(), frequencyHz);
            const double plottedOhm = point[1].toDouble();
            const double plottedGridOhm = gridPoint[1].toDouble();
            const double omega = 2.0 * std::acos(-1.0) * frequencyHz;
            const std::complex<double> sourceZ{sourceR, omega * sourceL};
            const std::complex<double> filterZ{
                branchR, omega * branchL - 1.0 / (omega * branchC)
            };
            const double expectedBusOhm = std::abs(sourceZ * filterZ / (sourceZ + filterZ));
            const double expectedGridOhm = std::abs(sourceZ);
            QVERIFY(std::isfinite(plottedOhm) && plottedOhm >= 0.0);
            QVERIFY(std::isfinite(plottedGridOhm) && plottedGridOhm >= 0.0);
            QVERIFY(qAbs(plottedOhm - expectedBusOhm) < 1.0e-9);
            QVERIFY(qAbs(plottedGridOhm - expectedGridOhm) < 1.0e-9);
        }

        auto* windingCopperLossPlot = window_->findChild<QWidget*>(
            QStringLiteral("resultsWindingCopperLossPlot"));
        QVERIFY(windingCopperLossPlot);
        const auto windingLossCurves = windingCopperLossPlot->property("plotCurves").toList();
        QCOMPARE(windingLossCurves.size(), 1);
        const auto windingLossPoints = windingLossCurves.front().toMap()
            .value(QStringLiteral("points")).toList();
        QVERIFY(windingLossPoints.size() >= 2);
        bool hasFundamentalCopperLoss = false;
        bool hasFifthHarmonicCopperLoss = false;
        for (const auto& value : windingLossPoints) {
            const auto point = value.toList();
            QCOMPARE(point.size(), 2);
            QVERIFY(std::isfinite(point[0].toDouble()));
            QVERIFY(std::isfinite(point[1].toDouble()));
            QVERIFY(point[1].toDouble() >= 0.0);
            if (qAbs(point[0].toDouble() - 1.0) < 1.0e-9)
                hasFundamentalCopperLoss = true;
            if (qAbs(point[0].toDouble() - 5.0) < 1.0e-9)
                hasFifthHarmonicCopperLoss = true;
        }
        QVERIFY(hasFundamentalCopperLoss);
        QVERIFY(hasFifthHarmonicCopperLoss);
        QVERIFY(windingCopperLossPlot->toolTip().contains(QStringLiteral("SEED-CU-C10100-R1")));
        QVERIFY(windingCopperLossPlot->toolTip().contains(QStringLiteral("PRELIMINARY")));
        QVERIFY(windingCopperLossPlot->toolTip().contains(QStringLiteral("Skin, proximity")));

        auto* currentTitle = window_->findChild<QLabel*>(
            QStringLiteral("inductorCurrentPlotTitle"));
        QVERIFY(currentTitle);
        QVERIFY(currentTitle->text().contains(QStringLiteral("Reactor branch peak current")));

        auto* coreFluxPlot = window_->findChild<QWidget*>(
            QStringLiteral("resultsCoreFluxPlot"));
        QVERIFY(coreFluxPlot);
        QVERIFY(coreFluxPlot->property("plotBarChart").toBool());
        const auto fluxCurves = coreFluxPlot->property("plotCurves").toList();
        QCOMPARE(fluxCurves.size(), 7);
        // The graph must contain the seven actual section peaks returned by
        // the converged multi-harmonic magnetic operating-point solver. A
        // successful PNG capture with empty point arrays is not a passing test.
        for (std::size_t section = 0; section < fluxCurves.size(); ++section) {
            const auto curve = fluxCurves[static_cast<qsizetype>(section)].toMap();
            const auto points = curve.value(QStringLiteral("points")).toList();
            QCOMPARE(points.size(), 1);
            const auto point = points.front().toList();
            QCOMPARE(point.size(), 2);
            QCOMPARE(point[0].toDouble(), static_cast<double>(section + 1));
            QVERIFY(std::isfinite(point[1].toDouble()));
            QVERIFY(point[1].toDouble() > 0.0);
        }
        auto* harmonicTransferPlot = window_->findChild<QWidget*>(
            QStringLiteral("resultsHarmonicTransferPlot"));
        QVERIFY(harmonicTransferPlot);
        const auto transferCurves = harmonicTransferPlot->property("plotCurves").toList();
        QCOMPARE(transferCurves.size(), 3);
        const auto transferPoints = transferCurves[0].toMap()
            .value(QStringLiteral("points")).toList();
        QCOMPARE(transferPoints.size(), 1001);
        QCOMPARE(transferPoints.front().toList()[0].toDouble(), 10.0);
        QCOMPARE(transferPoints.back().toList()[0].toDouble(), 2500.0);
        const double harmonicOmega = 2.0 * std::acos(-1.0) * 250.0;
        const std::complex<double> harmonicSourceZ{sourceR, harmonicOmega * sourceL};
        const std::complex<double> harmonicFilterZ{
            branchR, harmonicOmega * branchL - 1.0 / (harmonicOmega * branchC)
        };
        const double expectedGridToLoadRatio =
            1.0 / std::abs(1.0 + harmonicSourceZ / harmonicFilterZ);
        const auto sweptPoint = std::find_if(transferPoints.begin(), transferPoints.end(),
            [](const QVariant& value) {
                const auto point = value.toList();
                return point.size() == 2 && qAbs(point[0].toDouble() - 250.0) < 1.0e-9;
            });
        QVERIFY(sweptPoint != transferPoints.end());
        const auto sweptPointValues = sweptPoint->toList();
        QVERIFY(qAbs(sweptPointValues[1].toDouble() - expectedGridToLoadRatio) < 1.0e-9);
        const auto unityPoints = transferCurves[1].toMap()
            .value(QStringLiteral("points")).toList();
        QCOMPARE(unityPoints.size(), 2);
        QCOMPARE(unityPoints.front().toList()[1].toDouble(), 1.0);
        QCOMPARE(unityPoints.back().toList()[1].toDouble(), 1.0);
        QVERIFY(transferCurves[2].toMap()
            .value(QStringLiteral("markersOnly")).toBool());
        const auto operatingPoints = transferCurves[2].toMap()
            .value(QStringLiteral("points")).toList();
        QCOMPARE(operatingPoints.size(), 1);
        const auto operatingPoint = operatingPoints.front().toList();
        QCOMPARE(operatingPoint[0].toDouble(), 250.0);
        QVERIFY(qAbs(operatingPoint[1].toDouble() - expectedGridToLoadRatio) < 1.0e-9);
        const auto* harmonicTransferTitle = window_->findChild<QLabel*>(
            QStringLiteral("harmonicTransferPlotTitle"));
        QVERIFY(harmonicTransferTitle);
        QVERIFY(harmonicTransferTitle->text().contains(QStringLiteral("K_I(f)")));

        auto* voltageSpectrumPlot = window_->findChild<QWidget*>(
            QStringLiteral("resultsVoltageSpectrumPlot"));
        QVERIFY(voltageSpectrumPlot);
        QVERIFY(voltageSpectrumPlot->property("plotBarChart").toBool());
        QCOMPARE(voltageSpectrumPlot->property("plotHorizontalReference").toDouble(), 100.0);
        QVERIFY(!voltageSpectrumPlot->property("plotHorizontalReferenceLabel").toString().isEmpty());
        const auto voltageSpectrumCurves =
            voltageSpectrumPlot->property("plotCurves").toList();
        QCOMPARE(voltageSpectrumCurves.size(), 2);
        const auto voltageBeforePoints = voltageSpectrumCurves[0].toMap()
            .value(QStringLiteral("points")).toList();
        const auto voltageAfterPoints = voltageSpectrumCurves[1].toMap()
            .value(QStringLiteral("points")).toList();
        QCOMPARE(voltageBeforePoints.size(), 2);
        QCOMPARE(voltageAfterPoints.size(), 2);
        const auto voltageFundamentalBefore = voltageBeforePoints[0].toList();
        const auto voltageFundamentalAfter = voltageAfterPoints[0].toList();
        QCOMPARE(voltageFundamentalBefore[0].toDouble(), 1.0);
        QCOMPARE(voltageFundamentalAfter[0].toDouble(), 1.0);
        QCOMPARE(voltageFundamentalBefore[1].toDouble(), 100.0);
        QCOMPARE(voltageFundamentalAfter[1].toDouble(), 100.0);
        const auto voltageBeforePoint = voltageBeforePoints[1].toList();
        const auto voltageAfterPoint = voltageAfterPoints[1].toList();
        QCOMPARE(voltageBeforePoint[0].toDouble(), 5.0);
        QCOMPARE(voltageAfterPoint[0].toDouble(), 5.0);
        const std::complex<double> injectedHarmonicCurrent =
            std::polar(3.25, -15.0 * std::acos(-1.0) / 180.0);
        const double expectedPreFilterPccVoltage =
            100.0 * std::abs(harmonicSourceZ * injectedHarmonicCurrent) /
            phaseVoltage;
        QVERIFY(qAbs(voltageBeforePoint[1].toDouble() -
                     expectedPreFilterPccVoltage) < 1.0e-8);
        QVERIFY(std::isfinite(voltageAfterPoint[1].toDouble()));
        QVERIFY(voltageAfterPoint[1].toDouble() >= 0.0);
        const auto* voltageSpectrumTitle = window_->findChild<QLabel*>(
            QStringLiteral("voltageSpectrumPlotTitle"));
        QVERIFY(voltageSpectrumTitle);
        QVERIFY(voltageSpectrumTitle->text().contains(QStringLiteral("THD_U before")));
        QVERIFY(voltageSpectrumTitle->text().contains(QStringLiteral("after")));

        auto* currentSpectrumPlot = window_->findChild<QWidget*>(
            QStringLiteral("resultsCurrentSpectrumPlot"));
        QVERIFY(currentSpectrumPlot);
        QVERIFY(currentSpectrumPlot->property("plotBarChart").toBool());
        const auto currentSpectrumCurves =
            currentSpectrumPlot->property("plotCurves").toList();
        QCOMPARE(currentSpectrumCurves.size(), 3);
        const auto loadCurrentPoints = currentSpectrumCurves[0].toMap()
            .value(QStringLiteral("points")).toList();
        QCOMPARE(loadCurrentPoints.size(), 2);
        const auto loadFundamentalPoint = loadCurrentPoints[0].toList();
        QCOMPARE(loadFundamentalPoint[0].toDouble(), 1.0);
        QVERIFY(std::isfinite(loadFundamentalPoint[1].toDouble()));
        const auto loadCurrentPoint = loadCurrentPoints[1].toList();
        QCOMPARE(loadCurrentPoint[0].toDouble(), 5.0);
        QVERIFY(qAbs(loadCurrentPoint[1].toDouble() - 3.25) < 1.0e-9);

        auto* busTitle = window_->findChild<QLabel*>(
            QStringLiteral("busImpedancePlotTitle"));
        QVERIFY(busTitle);
        QVERIFY(busTitle->text().contains(QStringLiteral("source/filter frequency scan")));
        auto* coreWaveformPlot = window_->findChild<QWidget*>(
            QStringLiteral("resultsCoreFluxWaveformPlot"));
        QVERIFY(coreWaveformPlot);
        const auto waveformCurves = coreWaveformPlot->property("plotCurves").toList();
        QCOMPARE(waveformCurves.size(), 7);
        QVERIFY(coreWaveformPlot->property("plotAllowsNegativeY").toBool());
        bool hasPositiveFlux = false;
        bool hasNegativeFlux = false;
        for (const auto& curveValue : waveformCurves) {
            const auto curve = curveValue.toMap();
            const auto points = curve.value(QStringLiteral("points")).toList();
            QVERIFY(points.size() >= 72);
            QCOMPARE(points.front().toList()[0].toDouble(), 0.0);
            for (const auto& pointValue : points) {
                const auto point = pointValue.toList();
                QCOMPARE(point.size(), 2);
                QVERIFY(std::isfinite(point[1].toDouble()));
                hasPositiveFlux = hasPositiveFlux || point[1].toDouble() > 0.0;
                hasNegativeFlux = hasNegativeFlux || point[1].toDouble() < 0.0;
            }
        }
        QVERIFY(hasPositiveFlux);
        QVERIFY(hasNegativeFlux);
        auto* coreWaveformTitle = window_->findChild<QLabel*>(
            QStringLiteral("coreFluxWaveformPlotTitle"));
        QVERIFY(coreWaveformTitle);
        QVERIFY(coreWaveformTitle->text().contains(QStringLiteral("samples")));

        auto* coreFluxTitle = window_->findChild<QLabel*>(
            QStringLiteral("coreFluxPlotTitle"));
        QVERIFY(coreFluxTitle);
        QVERIFY(coreFluxTitle->text().contains(QStringLiteral("operating-point")));
        QVERIFY(!coreFluxTitle->text().contains(QStringLiteral("NOT_VERIFIED")));

        // The exact pinned M235-35A revision must expose its automatically
        // generated 49-point exploratory B-H branch in the Results graph,
        // even though the operating-point opt-in remains disabled by default.
        auto* materialBhPlot = window_->findChild<QWidget*>(
            QStringLiteral("resultsMaterialBhPlot"));
        QVERIFY(materialBhPlot);
        const auto bhCurves = materialBhPlot->property("plotCurves").toList();
        QCOMPARE(bhCurves.size(), 2);
        const auto estimatedBhCurve = bhCurves[0].toMap();
        QVERIFY(!estimatedBhCurve.value(QStringLiteral("markersOnly")).toBool());
        const auto bhPoints = estimatedBhCurve
            .value(QStringLiteral("points")).toList();
        QCOMPARE(bhPoints.size(), 49);
        const auto firstBhPoint = bhPoints.front().toList();
        const auto lastBhPoint = bhPoints.back().toList();
        QCOMPARE(firstBhPoint.size(), 2);
        QCOMPARE(lastBhPoint.size(), 2);
        QCOMPARE(firstBhPoint[0].toDouble(), 0.0);
        QCOMPARE(firstBhPoint[1].toDouble(), 0.0);
        QCOMPARE(lastBhPoint[0].toDouble(), 10000.0);
        QVERIFY(lastBhPoint[1].toDouble() > 1.70);
        const auto sourceAnchorCurve = bhCurves[1].toMap();
        QVERIFY(sourceAnchorCurve.value(QStringLiteral("markersOnly")).toBool());
        QVERIFY(sourceAnchorCurve.value(QStringLiteral("name")).toString()
            .contains(QStringLiteral("source anchors")));
        const auto sourceAnchorPoints = sourceAnchorCurve
            .value(QStringLiteral("points")).toList();
        QCOMPARE(sourceAnchorPoints.size(), 3);
        const auto firstSourceAnchor = sourceAnchorPoints.front().toList();
        const auto lastSourceAnchor = sourceAnchorPoints.back().toList();
        QCOMPARE(firstSourceAnchor[0].toDouble(), 2500.0);
        QCOMPARE(lastSourceAnchor[0].toDouble(), 10000.0);
        QVERIFY(firstSourceAnchor[1].toDouble() > 1.49);
        QVERIFY(lastSourceAnchor[1].toDouble() > 1.70);
        auto* materialBhTitle = window_->findChild<QLabel*>(
            QStringLiteral("materialBhPlotTitle"));
        QVERIFY(materialBhTitle);
        QVERIFY(materialBhTitle->text().contains(QStringLiteral("ESTIMATED")));
        QVERIFY(materialBhTitle->text().contains(QStringLiteral("NOT_VERIFIED")));

        // Compare two persisted solver snapshots through the same History UI
        // used by an operator. The test requires actual frequency-sweep samples
        // from both records; rendering an empty diagnostic chart is not enough.
        tabs->setCurrentIndex(6);
        QTRY_COMPARE(tabs->currentIndex(), 6);
        auto* historyTable = window_->findChild<QTableWidget*>(QStringLiteral("historyTable"));
        QVERIFY(historyTable);
        auto* refreshHistory = window_->findChild<QPushButton*>(
            QStringLiteral("historyRefreshButton"));
        if (refreshHistory)
            QTest::mouseClick(refreshHistory, Qt::LeftButton);
        QTRY_VERIFY_WITH_TIMEOUT(historyTable->rowCount() >= 2, 5000);
        historyTable->clearSelection();
        // QTableWidget::selectRow() may replace the current selection depending
        // on the active selection command. Explicit Select|Rows keeps both
        // calculation snapshots selected for the comparison action.
        auto* historySelection = historyTable->selectionModel();
        QVERIFY(historySelection);
        historySelection->select(
            historyTable->model()->index(0, 0),
            QItemSelectionModel::Select | QItemSelectionModel::Rows);
        historySelection->select(
            historyTable->model()->index(1, 0),
            QItemSelectionModel::Select | QItemSelectionModel::Rows);
        QCOMPARE(historySelection->selectedRows().size(), 2);
        auto* historyMetric = window_->findChild<QComboBox*>(
            QStringLiteral("historyPlotMetric"));
        QVERIFY(historyMetric);
        historyMetric->setCurrentIndex(0);
        auto* compareHistory = window_->findChild<QPushButton*>(
            QStringLiteral("compareHistoryGraphsButton"));
        QVERIFY(compareHistory);
        QTest::mouseClick(compareHistory, Qt::LeftButton);
        auto* historyComparison = window_->findChild<QWidget*>(
            QStringLiteral("historyComparisonPlot"));
        QVERIFY(historyComparison);
        const auto comparedCurves = historyComparison->property("plotCurves").toList();
        QCOMPARE(comparedCurves.size(), 2);
        for (const auto& curveValue : comparedCurves) {
            const auto points = curveValue.toMap()
                .value(QStringLiteral("points")).toList();
            QVERIFY2(points.size() >= 2,
                "History comparison must plot at least two real solver samples per calculation.");
            for (const auto& pointValue : points) {
                const auto point = pointValue.toList();
                QCOMPARE(point.size(), 2);
                QVERIFY(std::isfinite(point[0].toDouble()) && point[0].toDouble() > 0.0);
                QVERIFY(std::isfinite(point[1].toDouble()) && point[1].toDouble() >= 0.0);
            }
        }
        auto* historyDetails = window_->findChild<QPlainTextEdit*>(
            QStringLiteral("historyDetails"));
        QVERIFY(historyDetails);
        QVERIFY(historyDetails->toPlainText().contains(QStringLiteral("Graph comparison:")));
        QVERIFY(comparedCurves[0].toMap().value(QStringLiteral("name")).toString() !=
            comparedCurves[1].toMap().value(QStringLiteral("name")).toString());

        // Repeat the same two-snapshot comparison for the second supported
        // metric so both persisted result arrays are covered by the E2E path.
        historyMetric->setCurrentIndex(1);
        QTest::mouseClick(compareHistory, Qt::LeftButton);
        const auto historyImpedanceCurves = historyComparison->property("plotCurves").toList();
        QCOMPARE(historyImpedanceCurves.size(), 2);
        for (const auto& curveValue : historyImpedanceCurves) {
            const auto points = curveValue.toMap()
                .value(QStringLiteral("points")).toList();
            QVERIFY(points.size() >= 2);
            for (const auto& pointValue : points) {
                const auto point = pointValue.toList();
                QCOMPARE(point.size(), 2);
                QVERIFY(std::isfinite(point[0].toDouble()) && point[0].toDouble() > 0.0);
                QVERIFY(std::isfinite(point[1].toDouble()) && point[1].toDouble() >= 0.0);
            }
        }
        tabs->setCurrentIndex(4);
        QTRY_COMPARE(tabs->currentIndex(), 4);

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
        const QFileInfo harmonicTransferPng(artifactRoot.filePath(
            artifactProjectId + QStringLiteral("_harmonic_transfer.png")));
        const QFileInfo voltageSpectrumPng(artifactRoot.filePath(
            artifactProjectId + QStringLiteral("_voltage_harmonic_spectrum.png")));
        const QFileInfo currentSpectrumPng(artifactRoot.filePath(
            artifactProjectId + QStringLiteral("_current_harmonic_spectrum.png")));
        const QFileInfo coreFluxPng(artifactRoot.filePath(
            artifactProjectId + QStringLiteral("_core_flux.png")));
        const QFileInfo coreFluxWaveformPng(artifactRoot.filePath(
            artifactProjectId + QStringLiteral("_core_flux_waveform.png")));
        const QFileInfo materialBhPng(artifactRoot.filePath(
            artifactProjectId + QStringLiteral("_material_bh.png")));
        const QFileInfo windingCopperLossPng(artifactRoot.filePath(
            artifactProjectId + QStringLiteral("_winding_copper_loss.png")));
        auto* historyPlot = window_->findChild<QWidget*>(
            QStringLiteral("historyComparisonPlot"));
        QVERIFY(historyPlot);
        const QFileInfo historyPng(artifactRoot.filePath(
            artifactProjectId + QStringLiteral("_history_comparison.png")));
        const std::array<QFileInfo, 10> plotFiles{
            currentPng, impedancePng, harmonicTransferPng, voltageSpectrumPng,
            currentSpectrumPng, coreFluxPng, coreFluxWaveformPng, materialBhPng,
            windingCopperLossPng, historyPng
        };
        const std::array<QWidget*, 10> plotWidgets{
            window_->findChild<QWidget*>(QStringLiteral("resultsBankCurrentPlot")),
            window_->findChild<QWidget*>(QStringLiteral("resultsBusImpedancePlot")),
            harmonicTransferPlot,
            voltageSpectrumPlot,
            currentSpectrumPlot,
            coreFluxPlot,
            coreWaveformPlot,
            window_->findChild<QWidget*>(QStringLiteral("resultsMaterialBhPlot")),
            window_->findChild<QWidget*>(QStringLiteral("resultsWindingCopperLossPlot")),
            historyPlot
        };

        // The history page is intentionally lazy in the tab widget and may not
        // receive a show/resize event during this workflow. Visit it once so its
        // deferred Debug capture runs against its final, visible geometry. Return
        // to the calculation page before validating the result plots.
        const int workflowTabBeforeArtifactCapture = tabs->currentIndex();
        tabs->setCurrentIndex(6);
        QTRY_VERIFY_WITH_TIMEOUT(historyPlot->isVisible(), 3000);
        QTest::qWait(150);

        // The earlier assertions prove that the comparison action produced two
        // real series. Re-check after the lazy History page is shown: this is
        // the exact state that the PNG capture must represent.
        const auto visibleHistoryCurves =
            historyPlot->property("plotCurves").toList();
        QCOMPARE(visibleHistoryCurves.size(), 2);
        for (const auto& curveValue : visibleHistoryCurves) {
            const auto points = curveValue.toMap()
                .value(QStringLiteral("points")).toList();
            QVERIFY2(points.size() >= 2,
                "History plot lost its solver samples before Debug capture.");
        }

        tabs->setCurrentIndex(workflowTabBeforeArtifactCapture);
        QTest::qWait(150);

        for (std::size_t i = 0; i < plotFiles.size(); ++i) {
            QVERIFY(plotWidgets[i]);
            // Capture is debounced out of resizeEvent(). Wait for the PNG to
            // exist and match the final widget geometry before asserting it;
            // checking exists() first races the deferred save.
            QTRY_VERIFY_WITH_TIMEOUT(([&] {
                if (!plotFiles[i].exists() || plotFiles[i].size() <= 100)
                    return false;
                const QImage latestImage(plotFiles[i].absoluteFilePath());
                return !latestImage.isNull() &&
                    latestImage.size() == plotWidgets[i]->size();
            })(), 5000);
            QImage image(plotFiles[i].absoluteFilePath());
            QVERIFY2(!image.isNull(), "Saved Debug plot is not a decodable PNG.");
            QVERIFY(image.width() >= 120);
            QVERIFY(image.height() >= 100);
            QCOMPARE(image.size(), plotWidgets[i]->size());
            // An empty-data plot must still be a rendered diagnostic image,
            // not a fully transparent placeholder.
            QVERIFY(image.pixelColor(image.width() / 2, image.height() / 2).alpha() > 0);
            if (i == 9) {
                // The History comparison is required to contain actual plotted
                // series, not just axes and the "No valid plot data" message.
                int chromaticPlotPixels = 0;
                const QRect plotArea(86, 46,
                    std::max(1, image.width() - 122),
                    std::max(1, image.height() - 96));
                for (int y = plotArea.top(); y < plotArea.bottom(); ++y) {
                    for (int x = plotArea.left(); x < plotArea.right(); ++x) {
                        if (image.pixelColor(x, y).saturation() > 100)
                            ++chromaticPlotPixels;
                    }
                }
                QVERIFY2(chromaticPlotPixels > 25,
                    "History comparison PNG contains no visible coloured data series.");
            }
        }
#endif
        for (const auto& curve : fluxCurves)
            QCOMPARE(curve.toMap().value(QStringLiteral("points")).toList().size(), 1);

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

    void projectArchiveSaveLoadRoundTrip()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString archivePath = directory.filePath(QStringLiteral("roundtrip.p135project"));

        auto* customer = window_->findChild<QLineEdit*>(QStringLiteral("projectCustomer"));
        auto* voltage = window_->findChild<QDoubleSpinBox*>(QStringLiteral("filterNominalVoltageV"));
        auto* spectrum = window_->findChild<QTableWidget*>(QStringLiteral("filterVoltageHarmonics"));
        auto* currentHarmonics = window_->findChild<QTableWidget*>(
            QStringLiteral("filterLoadCurrentHarmonics"));
        QVERIFY(customer);
        QVERIFY(voltage);
        QVERIFY(spectrum);
        QVERIFY(currentHarmonics);

        customer->setText(QStringLiteral("Archive round-trip customer"));
        voltage->setValue(415.5);
        spectrum->setRowCount(1);
        spectrum->setColumnCount(4);
        spectrum->setItem(0, 0, new QTableWidgetItem(QStringLiteral("5")));
        spectrum->setItem(0, 1, new QTableWidgetItem(QStringLiteral("Negative")));
        spectrum->setItem(0, 2, new QTableWidgetItem(QStringLiteral("12.5")));
        spectrum->setItem(0, 3, new QTableWidgetItem(QStringLiteral("0")));
        currentHarmonics->setRowCount(1);
        currentHarmonics->setColumnCount(5);
        currentHarmonics->setItem(0, 0, new QTableWidgetItem(QStringLiteral("LOAD-ARCHIVE-1")));
        currentHarmonics->setItem(0, 1, new QTableWidgetItem(QStringLiteral("5")));
        currentHarmonics->setItem(0, 2, new QTableWidgetItem(QStringLiteral("Negative")));
        currentHarmonics->setItem(0, 3, new QTableWidgetItem(QStringLiteral("3.25")));
        currentHarmonics->setItem(0, 4, new QTableWidgetItem(QStringLiteral("-15.0")));

        QSettings settings;
        const QString originalProjectId = settings.value(QStringLiteral("project/currentId")).toString();
        QVERIFY(!originalProjectId.isEmpty());

        QAction* saveAction = nullptr;
        QAction* loadAction = nullptr;
        for (QAction* menuAction : window_->menuBar()->actions()) {
            if (!menuAction->menu()) continue;
            for (QAction* action : menuAction->menu()->actions()) {
                if (action->text().contains(QStringLiteral("Save project")))
                    saveAction = action;
                if (action->text().contains(QStringLiteral("Load project")))
                    loadAction = action;
            }
        }
        QVERIFY(saveAction);
        QVERIFY(loadAction);

        // QFileDialog is modal; accept it from its nested event loop so this
        // exercises the same menu actions as a user without manual interaction.
        QTimer::singleShot(0, [archivePath] {
            for (QWidget* widget : QApplication::topLevelWidgets()) {
                auto* dialog = qobject_cast<QFileDialog*>(widget);
                if (!dialog || !dialog->isVisible()) continue;
                dialog->selectFile(archivePath);
                QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection);
                return;
            }
        });
        QTimer::singleShot(5000, [] {
            if (auto* dialog = qobject_cast<QFileDialog*>(QApplication::activeModalWidget()))
                dialog->reject();
        });
        saveAction->trigger();

        QFile archiveFile(archivePath);
        QVERIFY2(archiveFile.open(QIODevice::ReadOnly), "Save action did not create the project archive");
        QJsonParseError parseError;
        const QJsonDocument savedDocument = QJsonDocument::fromJson(archiveFile.readAll(), &parseError);
        QCOMPARE(parseError.error, QJsonParseError::NoError);
        QVERIFY(savedDocument.isObject());
        const QJsonObject saved = savedDocument.object();
        QCOMPARE(saved.value(QStringLiteral("format")).toString(),
            QStringLiteral("Project135.Project"));
        QCOMPARE(saved.value(QStringLiteral("formatVersion")).toInt(), 1);
        QVERIFY(saved.value(QStringLiteral("controls")).toObject().contains(
            QStringLiteral("filterVoltageHarmonics")));
        QVERIFY(saved.value(QStringLiteral("controls")).toObject().contains(
            QStringLiteral("filterLoadCurrentHarmonics")));
        QVERIFY(!saved.contains(QStringLiteral("database")));
        QVERIFY(!saved.contains(QStringLiteral("calculationHistory")));

        // Simulate a machine whose local material database does not contain the
        // archived pinned revision. The revision ID must remain visible and must
        // not be silently replaced with a seeded/default material.
        QJsonObject modifiedArchive = saved;
        QJsonObject projectSettings = saved.value(QStringLiteral("projectSettings")).toObject();
        const QString missingRevisionId = QStringLiteral("MISSING-ARCHIVED-MATERIAL-REVISION");
        projectSettings.insert(QStringLiteral("construction/construction_coreMaterial"), missingRevisionId);
        modifiedArchive.insert(QStringLiteral("projectSettings"), projectSettings);
        QJsonObject controls = saved.value(QStringLiteral("controls")).toObject();
        QJsonObject coreMaterialState = controls.value(QStringLiteral("construction_coreMaterial")).toObject();
        coreMaterialState.insert(QStringLiteral("data"), missingRevisionId);
        coreMaterialState.insert(QStringLiteral("text"), QStringLiteral("Missing archived revision"));
        controls.insert(QStringLiteral("construction_coreMaterial"), coreMaterialState);
        modifiedArchive.insert(QStringLiteral("controls"), controls);
        archiveFile.close();
        QVERIFY(archiveFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QCOMPARE(archiveFile.write(QJsonDocument(modifiedArchive).toJson(QJsonDocument::Indented)) > 0, true);
        archiveFile.close();

        customer->setText(QStringLiteral("MUTATED"));
        voltage->setValue(690.0);
        spectrum->setItem(0, 2, new QTableWidgetItem(QStringLiteral("99.0")));
        currentHarmonics->setItem(0, 3, new QTableWidgetItem(QStringLiteral("99.0")));

        QTimer::singleShot(0, [archivePath] {
            for (QWidget* widget : QApplication::topLevelWidgets()) {
                auto* dialog = qobject_cast<QFileDialog*>(widget);
                if (!dialog || !dialog->isVisible()) continue;
                dialog->selectFile(archivePath);
                QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection);
                // Loading asks for confirmation and then displays a completion message.
                QTimer::singleShot(0, [] {
                    auto* message = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
                    if (!message) return;
                    if (auto* yes = message->button(QMessageBox::Yes)) {
                        QTimer::singleShot(0, [] {
                            if (auto* done = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))
                                done->accept();
                        });
                        yes->click();
                    } else {
                        message->accept();
                    }
                });
                return;
            }
        });
        QTimer::singleShot(5000, [] {
            if (auto* dialog = qobject_cast<QFileDialog*>(QApplication::activeModalWidget()))
                dialog->reject();
            if (auto* message = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))
                message->reject();
        });
        loadAction->trigger();

        settings.sync();
        const QString loadedProjectId = settings.value(QStringLiteral("project/currentId")).toString();
        QVERIFY(!loadedProjectId.isEmpty());
        QVERIFY(loadedProjectId != originalProjectId);
        QCOMPARE(customer->text(), QStringLiteral("Archive round-trip customer"));
        QCOMPARE(voltage->value(), 415.5);
        QCOMPARE(spectrum->rowCount(), 1);
        QCOMPARE(spectrum->columnCount(), 4);
        QVERIFY(spectrum->item(0, 0));
        QVERIFY(spectrum->item(0, 1));
        QVERIFY(spectrum->item(0, 2));
        QVERIFY(spectrum->item(0, 3));
        QCOMPARE(spectrum->item(0, 0)->text(), QStringLiteral("5"));
        QCOMPARE(spectrum->item(0, 1)->text(), QStringLiteral("Negative"));
        QCOMPARE(spectrum->item(0, 2)->text(), QStringLiteral("12.5"));
        QCOMPARE(spectrum->item(0, 3)->text(), QStringLiteral("0"));
        QCOMPARE(currentHarmonics->rowCount(), 1);
        QCOMPARE(currentHarmonics->columnCount(), 5);
        QCOMPARE(currentHarmonics->item(0, 0)->text(), QStringLiteral("LOAD-ARCHIVE-1"));
        QCOMPARE(currentHarmonics->item(0, 1)->text(), QStringLiteral("5"));
        QCOMPARE(currentHarmonics->item(0, 2)->text(), QStringLiteral("Negative"));
        QCOMPARE(currentHarmonics->item(0, 3)->text(), QStringLiteral("3.25"));
        QCOMPARE(currentHarmonics->item(0, 4)->text(), QStringLiteral("-15.0"));
        auto* coreMaterial = window_->findChild<QComboBox*>(
            QStringLiteral("construction_coreMaterial"));
        QVERIFY(coreMaterial);
        QCOMPARE(coreMaterial->currentData().toString(), missingRevisionId);
        QVERIFY(coreMaterial->currentText().contains(QStringLiteral("NOT AVAILABLE")));
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
