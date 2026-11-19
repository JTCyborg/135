#include "main_window.hpp"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("Project135 — Engineering Design");
    resize(1280, 820);

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

    auto* toolbar = addToolBar(tr("Main"));
    toolbar->addAction(tr("New project"), this, [this] {
        projectName_->clear();
        updateProjectTitle();
        tabs_->setCurrentIndex(0);
    });
    toolbar->addAction(tr("Project"), this, [this] { tabs_->setCurrentIndex(0); });
    toolbar->addAction(tr("Calculate"), this, [this] {
        tabs_->setCurrentIndex(2);
        appendLog(tr("Calculation is not enabled yet: the product-level filter model and validation are the next implementation step."));
        QMessageBox::information(this, tr("Calculation not available"),
            tr("The desktop shell is operational. Engineering filter synthesis is not yet connected; no design result has been generated."));
    });

    tabs_ = new QTabWidget(this);
    tabs_->addTab(createProjectPage(), tr("Project"));
    tabs_->addTab(createFilterPage(), tr("Filter definition"));
    tabs_->addTab(createResultsPage(), tr("Calculation"));
    tabs_->addTab(createCadPage(), tr("3D CAD"));
    setCentralWidget(tabs_);

    status_ = new QLabel(tr("Ready — desktop shell"));
    statusBar()->addPermanentWidget(status_);
    appendLog(tr("Project135 desktop started. Core remains independent from Qt."));
    updateProjectTitle();
}

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
    productFamily_->addItems({tr("Shunt passive harmonic filter"), tr("VFD output filter"), tr("Reactor / inductor"), tr("Power transformer"), tr("AFE reactor")});
    form->addRow(tr("Name"), projectName_);
    form->addRow(tr("Product family"), productFamily_);
    layout->addWidget(group);

    auto* note = new QLabel(tr("This is the first desktop application shell. Project persistence, revision history, material database and validated engineering workflows will be added in subsequent milestones."));
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
    auto* heading = new QLabel(tr("Electrical definition — initial input form"));
    heading->setStyleSheet("font-size: 20px; font-weight: 600;");
    layout->addWidget(heading);

    auto* group = new QGroupBox(tr("Network and filter targets"));
    auto* form = new QFormLayout(group);
    filterTopology_ = new QComboBox;
    filterTopology_->addItems({tr("Single-tuned LC branch"), tr("Double-tuned branch"), tr("High-pass branch"), tr("C-type branch"), tr("Multi-branch bank")});
    nominalVoltage_ = new QDoubleSpinBox;
    nominalVoltage_->setRange(1.0, 1000000.0);
    nominalVoltage_->setValue(400.0);
    nominalVoltage_->setSuffix(tr(" V"));
    nominalVoltage_->setDecimals(1);
    fundamentalFrequency_ = new QDoubleSpinBox;
    fundamentalFrequency_->setRange(1.0, 1000.0);
    fundamentalFrequency_->setValue(50.0);
    fundamentalFrequency_->setSuffix(tr(" Hz"));
    targetReactivePower_ = new QDoubleSpinBox;
    targetReactivePower_->setRange(0.0, 100000000.0);
    targetReactivePower_->setValue(100.0);
    targetReactivePower_->setSuffix(tr(" kvar"));
    targetReactivePower_->setDecimals(2);
    form->addRow(tr("Topology"), filterTopology_);
    form->addRow(tr("Nominal line voltage"), nominalVoltage_);
    form->addRow(tr("Fundamental frequency"), fundamentalFrequency_);
    form->addRow(tr("Target reactive power"), targetReactivePower_);
    layout->addWidget(group);

    auto* warning = new QLabel(tr("Input values are currently a UI draft only. No assumptions about connection, detuning, component ratings or compliance are made, and these values are not yet passed to the solver."));
    warning->setWordWrap(true);
    warning->setStyleSheet("color: #a66b00;");
    layout->addWidget(warning);

    auto* apply = new QPushButton(tr("Record inputs in session log"));
    connect(apply, &QPushButton::clicked, this, [this] {
        appendLog(tr("Draft inputs: %1; ULL=%2 V; f=%3 Hz; Qtarget=%4 kvar.")
            .arg(filterTopology_->currentText())
            .arg(nominalVoltage_->value())
            .arg(fundamentalFrequency_->value())
            .arg(targetReactivePower_->value()));
        status_->setText(tr("Draft inputs recorded — not calculated"));
    });
    layout->addWidget(apply);
    layout->addStretch();
    return page;
}

QWidget* MainWindow::createResultsPage()
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    auto* heading = new QLabel(tr("Calculation results"));
    heading->setStyleSheet("font-size: 20px; font-weight: 600;");
    layout->addWidget(heading);
    auto* table = new QTableWidget(0, 3);
    table->setHorizontalHeaderLabels({tr("Quantity"), tr("Value"), tr("Status")});
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(table);
    auto* empty = new QLabel(tr("No calculation has been performed. Results will appear here only after the product model, validation and solver integration are implemented."));
    empty->setWordWrap(true);
    layout->addWidget(empty);
    log_ = new QPlainTextEdit;
    log_->setReadOnly(true);
    log_->setMaximumBlockCount(500);
    log_->setPlaceholderText(tr("Application and engineering activity log"));
    layout->addWidget(log_);
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
