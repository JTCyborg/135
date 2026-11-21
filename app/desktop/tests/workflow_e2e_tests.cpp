#include "main_window.hpp"

#include <QComboBox>
#include <QCoreApplication>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTest>
#include <QTableWidget>
#include <QStringList>
#include <QWidget>

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
        window_ = std::make_unique<MainWindow>();
        window_->show();
        QTest::qWait(50);
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

        auto* confirmCalculation = window_->findChild<QPushButton*>(
            QStringLiteral("confirmCalculationDesign"));
        QVERIFY(confirmCalculation);
        QTest::mouseClick(confirmCalculation, Qt::LeftButton);
        QTRY_COMPARE_WITH_TIMEOUT(tabs->currentIndex(), 4, 5000);

        QVERIFY(window_->findChild<QPushButton*>(
            QStringLiteral("generatePreliminaryCad")));
    }

    void twoDimensionalDrawingStageIsPresent()
    {
        const auto* drawingAction = window_->findChild<QPushButton*>(
            QStringLiteral("generateCadDrawings"));
        QEXPECT_FAIL("", "The 2D drawing generator/export is not implemented yet.", Continue);
        QVERIFY2(drawingAction,
            "Expected a 2D drawing generation action on the CAD page.");
    }
};

QTEST_MAIN(DesktopWorkflowE2ETests)
#include "workflow_e2e_tests.moc"
