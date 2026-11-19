#include "main_window.hpp"
#include "material_database_widget.hpp"
#include "p135/infrastructure/sqlite/migration_runner.hpp"
#include "p135/infrastructure/sqlite/sqlite_material_revision_store.hpp"
#include "p135/infrastructure/sqlite/starter_material_catalog.hpp"

#include "p135/filter/synthesis.hpp"
#include "p135/filter/harmonics.hpp"
#include "p135/filter/analysis.hpp"
#include "p135/filter/network.hpp"
#include "p135/circuit/solver.hpp"

#include <QAbstractItemView>
#include <QAction>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QDir>
#include <QDateTime>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>

#include <cstdint>
#include <cstddef>
#include <cmath>
#include <string>
#include <filesystem>
#include <complex>
#include <optional>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("Project135 — Engineering Design");
    resize(1280, 820);

    const QString dataDirectory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dataDirectory.isEmpty() || !QDir().mkpath(dataDirectory)) {
        databaseError_ = tr("Cannot create the application data directory.");
    } else {
        const QString databasePath = dataDirectory + "/project135-materials.sqlite";
        const auto opened = database_.open(databasePath.toStdString());
        if (!opened) {
            databaseError_ = QString::fromStdString(opened.error().message);
        } else {
            const QString migrationPath = QCoreApplication::applicationDirPath()
                + "/migrations/001_initial_schema.sql";
            p135::infrastructure::sqlite::MigrationRunner migrations(database_);
            const auto migrated = migrations.apply(p135::infrastructure::sqlite::Migration{
                1, "initial_material_revision_store", std::filesystem::path(migrationPath.toStdString())
            });
            if (!migrated) {
                databaseError_ = QString::fromStdString(migrated.error().message);
                database_.close();
            } else {
                materialStore_ = std::make_unique<p135::infrastructure::sqlite::SqliteMaterialRevisionStore>(database_);
                const auto seeded = p135::infrastructure::sqlite::seedStarterMaterialCatalog(
                    *materialStore_, "Project135 built-in starter catalog",
                    QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs).toStdString());
                if (!seeded) {
                    databaseError_ = QString::fromStdString(seeded.error().message);
                    materialStore_.reset();
                }
            }
        }
    }

    auto* fileMenu = menuBar()->addMenu(tr("&File"));
    auto* newProject = fileMenu->addAction(tr("New project"));
    connect(newProject, &QAction::triggered, this, [this] {
        projectName_->clear();
        productFamily_->setCurrentIndex(0);
        updateProjectTitle();
        appendLog(tr("New project workspace created."));
        tabs_->setCurrentIndex(0);
    });
    fileMenu->addSeparator();
    fileMenu->addAction(tr("Exit"), this, &QWidget::close);

    auto* viewMenu = menuBar()->addMenu(tr("&View"));
    viewMenu->addAction(tr("Project"), this, [this] { tabs_->setCurrentIndex(0); });
    viewMenu->addAction(tr("Filter definition"), this, [this] { tabs_->setCurrentIndex(1); });
    viewMenu->addAction(tr("Results"), this, [this] { tabs_->setCurrentIndex(2); });
    viewMenu->addAction(tr("CAD"), this, [this] { tabs_->setCurrentIndex(3); });
    viewMenu->addAction(tr("Material database"), this, [this] { tabs_->setCurrentIndex(4); });

    auto* toolbar = addToolBar(tr("Main"));
    toolbar->addAction(tr("New project"), this, [this] {
        projectName_->clear();
        updateProjectTitle();
        tabs_->setCurrentIndex(0);
    });
    toolbar->addAction(tr("Project"), this, [this] { tabs_->setCurrentIndex(0); });
    toolbar->addAction(tr("Materials"), this, [this] { tabs_->setCurrentIndex(4); });
    toolbar->addAction(tr("Calculate"), this, [this] {
        calculateFilter();
        tabs_->setCurrentIndex(2);
    });

    tabs_ = new QTabWidget(this);
    tabs_->addTab(createProjectPage(), tr("Project"));
    tabs_->addTab(createFilterPage(), tr("Filter definition"));
    tabs_->addTab(createResultsPage(), tr("Calculation"));
    tabs_->addTab(createCadPage(), tr("3D CAD"));
    tabs_->addTab(createMaterialsPage(), tr("Materials"));
    setCentralWidget(tabs_);

    status_ = new QLabel(tr("Ready — filter synthesis available"));
    statusBar()->addPermanentWidget(status_);
    appendLog(tr("Project135 desktop started. Core and filter synthesis remain independent from Qt."));
    if (!databaseError_.isEmpty()) {
        status_->setText(tr("Material database unavailable"));
        appendLog(tr("Material database initialization failed: %1").arg(databaseError_));
    } else {
        appendLog(tr("SQLite material database initialized."));
    }
    updateProjectTitle();
}

MainWindow::~MainWindow() = default;

QWidget* MainWindow::createProjectPage()
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    auto* heading = new QLabel(tr("Engineering project"));
    heading->setStyleSheet("font-size: 22px; font-weight: 600;");
    layout->addWidget(heading);

    auto* group = new QGroupBox(tr("Project identity"));
    auto* form = new QFormLayout(group);
    projectName_ = new QLineEdit(tr("New engineering project"));
    projectName_->setPlaceholderText(tr("Project name"));
    productFamily_ = new QComboBox;
    productFamily_->addItems({tr("Shunt passive harmonic filter"), tr("VFD output filter"),
        tr("Reactor / inductor"), tr("Power transformer"), tr("AFE reactor")});
    form->addRow(tr("Name"), projectName_);
    form->addRow(tr("Product family"), productFamily_);
    layout->addWidget(group);

    auto* note = new QLabel(tr("The first supported calculation workflow is a three-phase shunt filter with single-tuned series RLC branches. Other product families remain planned and are not calculated yet."));
    note->setWordWrap(true);
    note->setStyleSheet("color: palette(mid);");
    layout->addWidget(note);
    layout->addStretch();
    connect(projectName_, &QLineEdit::textChanged, this, [this] { updateProjectTitle(); });
    connect(productFamily_, &QComboBox::currentTextChanged, this, [this](const QString& value) {
        appendLog(tr("Product family selected: %1").arg(value));
    });
    return page;
}

QWidget* MainWindow::createFilterPage()
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    auto* heading = new QLabel(tr("Three-phase shunt filter — single-tuned branch"));
    heading->setStyleSheet("font-size: 20px; font-weight: 600;");
    layout->addWidget(heading);

    auto* group = new QGroupBox(tr("Network and branch parameters"));
    auto* form = new QFormLayout(group);

    nominalVoltage_ = new QDoubleSpinBox;
    nominalVoltage_->setRange(1.0, 1000000.0);
    nominalVoltage_->setValue(400.0);
    nominalVoltage_->setSuffix(tr(" V L-L"));
    nominalVoltage_->setDecimals(1);

    fundamentalFrequency_ = new QDoubleSpinBox;
    fundamentalFrequency_->setRange(1.0, 1000.0);
    fundamentalFrequency_->setValue(50.0);
    fundamentalFrequency_->setSuffix(tr(" Hz"));

    targetReactivePower_ = new QDoubleSpinBox;
    targetReactivePower_->setRange(0.001, 100000000.0);
    targetReactivePower_->setValue(100.0);
    targetReactivePower_->setSuffix(tr(" kvar total"));
    targetReactivePower_->setDecimals(3);

    filterConnection_ = new QComboBox;
    filterConnection_->addItem(tr("Wye (Y)"), static_cast<int>(p135::filter::Connection::Wye));
    filterConnection_->addItem(tr("Delta (Δ)"), static_cast<int>(p135::filter::Connection::Delta));

    tuningHarmonicOrder_ = new QSpinBox;
    tuningHarmonicOrder_->setRange(2, 100);
    tuningHarmonicOrder_->setValue(5);

    qualityFactor_ = new QDoubleSpinBox;
    qualityFactor_->setRange(0.1, 10000.0);
    qualityFactor_->setValue(50.0);
    qualityFactor_->setDecimals(2);

    form->addRow(tr("Nominal line voltage"), nominalVoltage_);
    form->addRow(tr("Fundamental frequency"), fundamentalFrequency_);
    form->addRow(tr("Branch connection"), filterConnection_);
    form->addRow(tr("Target reactive power"), targetReactivePower_);
    form->addRow(tr("Tuning harmonic order"), tuningHarmonicOrder_);
    form->addRow(tr("Branch quality factor"), qualityFactor_);
    layout->addWidget(group);

    auto* note = new QLabel(tr("The target reactive power is the total three-phase fundamental-frequency value for this branch. The synthesis uses ideal C and L and derives a series damping resistor from Q factor. Capacitor ratings, network impedance, harmonic spectrum, resonance interaction and compliance are not evaluated in this stage."));
    note->setWordWrap(true);
    note->setStyleSheet("color: #a66b00;");
    layout->addWidget(note);

    auto* advanced = new QGroupBox(tr("Source impedance and harmonic spectrum"));
    auto* advancedLayout = new QVBoxLayout(advanced);
    auto* sourceForm = new QFormLayout;

    sourceResistance_ = new QDoubleSpinBox;
    sourceResistance_->setRange(0.0, 1000000.0);
    sourceResistance_->setDecimals(6);
    sourceResistance_->setValue(0.0);
    sourceResistance_->setSuffix(tr(" Ω"));
    sourceInductance_ = new QDoubleSpinBox;
    sourceInductance_->setRange(0.0, 1000000.0);
    sourceInductance_->setDecimals(6);
    sourceInductance_->setValue(0.0);
    sourceInductance_->setSuffix(tr(" mH"));
    sourceForm->addRow(tr("Thevenin source resistance"), sourceResistance_);
    sourceForm->addRow(tr("Thevenin source inductance"), sourceInductance_);
    advancedLayout->addLayout(sourceForm);

    auto* spectrumHelp = new QLabel(tr("Enter measured/calculated harmonic phase-A RMS voltage magnitudes. The fundamental is derived from nominal line voltage; add only non-fundamental orders here. Use Positive or Negative sequence. Zero sequence is not supported."));
    spectrumHelp->setWordWrap(true);
    advancedLayout->addWidget(spectrumHelp);

    harmonicSpectrumTable_ = new QTableWidget(0, 4);
    harmonicSpectrumTable_->setHorizontalHeaderLabels({
        tr("Order"), tr("Sequence"), tr("RMS voltage (V)"), tr("Phase (°)")
    });
    harmonicSpectrumTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    harmonicSpectrumTable_->setMinimumHeight(120);
    harmonicSpectrumTable_->setEditTriggers(QAbstractItemView::DoubleClicked |
                                             QAbstractItemView::EditKeyPressed |
                                             QAbstractItemView::AnyKeyPressed);
    advancedLayout->addWidget(harmonicSpectrumTable_);

    auto* spectrumButtons = new QHBoxLayout;
    auto* addHarmonic = new QPushButton(tr("Add harmonic"));
    auto* removeHarmonic = new QPushButton(tr("Remove selected"));
    spectrumButtons->addWidget(addHarmonic);
    spectrumButtons->addWidget(removeHarmonic);
    spectrumButtons->addStretch();
    advancedLayout->addLayout(spectrumButtons);
    connect(addHarmonic, &QPushButton::clicked, this, [this] {
        const int row = harmonicSpectrumTable_->rowCount();
        harmonicSpectrumTable_->insertRow(row);
        harmonicSpectrumTable_->setItem(row, 0, new QTableWidgetItem(QStringLiteral("5")));
        harmonicSpectrumTable_->setItem(row, 1, new QTableWidgetItem(QStringLiteral("Negative")));
        harmonicSpectrumTable_->setItem(row, 2, new QTableWidgetItem(QStringLiteral("0")));
        harmonicSpectrumTable_->setItem(row, 3, new QTableWidgetItem(QStringLiteral("0")));
    });
    connect(removeHarmonic, &QPushButton::clicked, this, [this] {
        const int row = harmonicSpectrumTable_->currentRow();
        if (row >= 0)
            harmonicSpectrumTable_->removeRow(row);
    });

    auto* sourceWarning = new QLabel(tr("Source impedance is deliberately unset by default. Enter values from a short-circuit study or measurement before running resonance and harmonic calculations; zero/zero leaves these analyses NOT_VERIFIED."));
    sourceWarning->setWordWrap(true);
    sourceWarning->setStyleSheet("color: #a66b00;");
    advancedLayout->addWidget(sourceWarning);
    layout->addWidget(advanced);

    auto* calculate = new QPushButton(tr("Synthesize LC branch"));
    connect(calculate, &QPushButton::clicked, this, [this] {
        calculateFilter();
        tabs_->setCurrentIndex(2);
    });
    layout->addWidget(calculate);
    layout->addStretch();
    return page;
}

QWidget* MainWindow::createResultsPage()
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    auto* heading = new QLabel(tr("Filter synthesis results"));
    heading->setStyleSheet("font-size: 20px; font-weight: 600;");
    layout->addWidget(heading);

    resultsTable_ = new QTableWidget(0, 3);
    resultsTable_->setHorizontalHeaderLabels({tr("Parameter"), tr("Value"), tr("Unit")});
    resultsTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    resultsTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(resultsTable_);

    resultsNote_ = new QLabel(tr("No synthesis has been performed."));
    resultsNote_->setWordWrap(true);
    layout->addWidget(resultsNote_);

    log_ = new QPlainTextEdit;
    log_->setReadOnly(true);
    log_->setMaximumBlockCount(500);
    log_->setPlaceholderText(tr("Application and engineering activity log"));
    layout->addWidget(log_);
    return page;
}

QWidget* MainWindow::createMaterialsPage()
{
    if (materialStore_)
        return new MaterialDatabaseWidget(*materialStore_);
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    auto* message = new QLabel(tr("Material database could not be initialized: %1").arg(databaseError_));
    message->setWordWrap(true);
    layout->addWidget(message);
    layout->addStretch();
    return page;
}

QWidget* MainWindow::createCadPage()
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    auto* heading = new QLabel(tr("3D CAD workspace"));
    heading->setStyleSheet("font-size: 20px; font-weight: 600;");
    layout->addWidget(heading);
    auto* placeholder = new QLabel(tr("OCCT viewport is not connected yet. The future 3D model will be generated from the engineering domain model; CAD geometry will not be used as the source of electrical parameters."));
    placeholder->setAlignment(Qt::AlignCenter);
    placeholder->setWordWrap(true);
    placeholder->setMinimumHeight(320);
    placeholder->setStyleSheet("border: 1px dashed palette(mid); padding: 24px;");
    layout->addWidget(placeholder);
    layout->addStretch();
    return page;
}

void MainWindow::calculateFilter()
{
    if (!nominalVoltage_ || !fundamentalFrequency_ || !targetReactivePower_ ||
        !filterConnection_ || !tuningHarmonicOrder_ || !qualityFactor_ ||
        !resultsTable_ || !resultsNote_) {
        return;
    }

    if (productFamily_ && productFamily_->currentIndex() != 0) {
        const QString message = tr("Calculation is currently implemented only for Shunt Passive Harmonic Filter.");
        resultsNote_->setText(message);
        status_->setText(tr("Selected product family is not implemented yet"));
        QMessageBox::information(this, tr("Calculation not available"), message);
        return;
    }

    p135::filter::FilterSpecification specification;
    specification.network.lineToLineVoltage = p135::units::Voltage{nominalVoltage_->value()};
    specification.network.fundamentalFrequency =
        p135::units::Frequency{fundamentalFrequency_->value()};
    specification.network.connection = static_cast<p135::filter::Connection>(
        filterConnection_->currentData().toInt());
    specification.branches.push_back(p135::filter::SingleTunedBranchSpecification{
        1,
        "Single-tuned branch",
        static_cast<std::uint32_t>(tuningHarmonicOrder_->value()),
        p135::units::Power{targetReactivePower_->value() * 1000.0},
        qualityFactor_->value()
    });

    const auto result = p135::filter::synthesize(specification);
    resultsTable_->setRowCount(0);

    if (!result) {
        const auto& error = result.error();
        resultsNote_->setText(tr("Synthesis failed: %1").arg(QString::fromStdString(error.message)));
        status_->setText(tr("Filter synthesis failed"));
        appendLog(tr("Filter synthesis failed: %1").arg(QString::fromStdString(error.message)));
        QMessageBox::warning(this, tr("Invalid filter definition"), resultsNote_->text());
        return;
    }

    const auto circuitResult = p135::filter::buildCircuit(result.value());
    if (!circuitResult) {
        const QString message = QString::fromStdString(circuitResult.error().message);
        resultsNote_->setText(tr("Circuit generation failed: %1").arg(message));
        status_->setText(tr("Circuit generation failed"));
        appendLog(tr("Circuit generation failed: %1").arg(message));
        QMessageBox::warning(this, tr("Circuit generation failed"), message);
        return;
    }

    const auto electricalResult = p135::circuit::solve(p135::circuit::Study{
        circuitResult.value(),
        p135::circuit::AnalysisKind::AcSteadyState,
        p135::units::Frequency{fundamentalFrequency_->value()}
    });
    if (!electricalResult) {
        const QString message = QString::fromStdString(electricalResult.error().message);
        resultsNote_->setText(tr("AC network solution failed: %1").arg(message));
        status_->setText(tr("AC network solution failed"));
        appendLog(tr("AC network solution failed: %1").arg(message));
        QMessageBox::warning(this, tr("Circuit solver failed"), message);
        return;
    }

    p135::circuit::FrequencySweep sweepRequest;
    sweepRequest.study = p135::circuit::Study{
        circuitResult.value(),
        p135::circuit::AnalysisKind::AcSteadyState,
        p135::units::Frequency{fundamentalFrequency_->value()}
    };
    sweepRequest.startFrequency = p135::units::Frequency{fundamentalFrequency_->value() * 0.5};
    sweepRequest.stopFrequency = p135::units::Frequency{fundamentalFrequency_->value() * 15.0};
    sweepRequest.points = 151;
    sweepRequest.spacing = p135::circuit::SweepSpacing::Logarithmic;
    const auto sweepResult = p135::circuit::sweep(sweepRequest);
    if (!sweepResult) {
        const QString message = QString::fromStdString(sweepResult.error().message);
        resultsNote_->setText(tr("Frequency response calculation failed: %1").arg(message));
        status_->setText(tr("Frequency sweep failed"));
        appendLog(tr("Frequency sweep failed: %1").arg(message));
        QMessageBox::warning(this, tr("Frequency sweep failed"), message);
        return;
    }

    double peakBankCurrent = 0.0;
    double peakBankCurrentFrequency = 0.0;
    for (const auto& point : sweepResult.value().points) {
        double currentSquared = 0.0;
        // For the current single-branch Wye/Delta model, the three phase
        // series-branch currents are the R elements at indices 3, 6 and 9.
        for (const std::size_t index : {3U, 6U, 9U}) {
            if (index < point.result.branches.size())
                currentSquared += std::norm(point.result.branches[index].rmsCurrent);
        }
        const double bankCurrent = std::sqrt(currentSquared);
        if (bankCurrent > peakBankCurrent) {
            peakBankCurrent = bankCurrent;
            peakBankCurrentFrequency = point.frequency.si();
        }
    }

    const auto& branch = result.value().branches.front();
    const auto addResult = [this](const QString& name, double value, const QString& unit) {
        const int row = resultsTable_->rowCount();
        resultsTable_->insertRow(row);
        resultsTable_->setItem(row, 0, new QTableWidgetItem(name));
        resultsTable_->setItem(row, 1, new QTableWidgetItem(QString::number(value, 'g', 10)));
        resultsTable_->setItem(row, 2, new QTableWidgetItem(unit));
    };

    addResult(tr("Phase RMS voltage"), branch.phaseVoltage.si(), tr("V"));
    addResult(tr("Capacitance per phase"), branch.capacitance.si() * 1.0e6, tr("µF"));
    addResult(tr("Inductance per phase"), branch.inductance.si() * 1.0e3, tr("mH"));
    addResult(tr("Series damping resistance"), branch.dampingResistance.si(), tr("Ω"));
    addResult(tr("Tuning frequency"), fundamentalFrequency_->value() *
              static_cast<double>(branch.tuningHarmonicOrder), tr("Hz"));
    addResult(tr("Target three-phase reactive power"), branch.targetReactivePower.si() / 1000.0,
              tr("kvar"));
    addResult(tr("Branch quality factor"), branch.qualityFactor, tr("—"));
    addResult(tr("MNA filter-bank active power"), electricalResult.value().powerBalance.totalActivePowerW, tr("W"));
    addResult(tr("MNA filter-bank reactive power"),
              [&electricalResult] {
                  double q = 0.0;
                  for (std::size_t i = 3; i < electricalResult.value().branches.size(); ++i)
                      q += electricalResult.value().branches[i].complexPowerVA.imag();
                  return q;
              }(), tr("var"));
    addResult(tr("MNA matrix order"),
              static_cast<double>(electricalResult.value().solver.matrixOrder), tr("unknowns"));
    addResult(tr("MNA relative residual"),
              electricalResult.value().solver.relativeResidual, tr("—"));
    addResult(tr("Peak three-phase bank current in sweep"), peakBankCurrent, tr("A RMS"));
    addResult(tr("Frequency at peak bank current"), peakBankCurrentFrequency, tr("Hz"));
    addResult(tr("Frequency sweep points"), static_cast<double>(sweepResult.value().points.size()), tr("points"));

    resultsNote_->setText(tr("LC branch synthesis, balanced three-phase AC MNA verification at the fundamental frequency and a logarithmic 0.5–15× fundamental-frequency current sweep completed. The peak current is the maximum RMS vector sum of the three phase branch currents in the ideal-source model; it is not a harmonic attenuation or compliance result. Source impedance, background harmonic voltage, feeder impedance, capacitor/reactor stress ratings, losses, temperature and standards checks are not yet evaluated."));
    status_->setText(tr("Filter synthesized and AC network solved"));
    appendLog(tr("Single-tuned branch synthesized: %1 kvar, h=%2, connection=%3.")
        .arg(targetReactivePower_->value())
        .arg(tuningHarmonicOrder_->value())
        .arg(filterConnection_->currentText()));
}

void MainWindow::updateProjectTitle()
{
    const QString name = projectName_ ? projectName_->text().trimmed() : QString{};
    setWindowTitle(QString("Project135 — %1").arg(name.isEmpty() ? tr("Untitled") : name));
}

void MainWindow::appendLog(const QString& message)
{
    if (log_)
        log_->appendPlainText(message);
}
