#include "main_window.hpp"
#include "material_database_widget.hpp"
#include "p135/infrastructure/sqlite/migration_runner.hpp"
#include "p135/infrastructure/sqlite/sqlite_material_revision_store.hpp"
#include "p135/infrastructure/sqlite/canonical_json.hpp"
#include "p135/infrastructure/sqlite/starter_material_catalog.hpp"

#include "p135/filter/synthesis.hpp"
#include "p135/filter/harmonics.hpp"
#include "p135/filter/analysis.hpp"
#include "p135/filter/network.hpp"
#include "p135/circuit/solver.hpp"
#include "p135/domain/component_catalog.hpp"
#include "p135/thermal/filter_bridge.hpp"

#include <QAbstractItemView>
#include <QItemSelectionModel>
#include <QAction>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QDir>
#include <QDateTime>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QSettings>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPainter>
#include <QColor>
#include <QSizePolicy>
#include <QPainterPath>
#include <QPaintEvent>
#include <QFontMetrics>
#include <QFrame>
#include <QPushButton>
#include <QSpinBox>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>
#include <QUuid>

#include <cstdint>
#include <cstddef>
#include <cmath>
#include <string>
#include <filesystem>
#include <complex>
#include <optional>
#include <vector>
#include <utility>
#include <limits>
#include <algorithm>


struct PlotCurve {
    QString name;
    std::vector<std::pair<double, double>> points;
};

struct PlotBand {
    double startHz{};
    double stopHz{};
    QString label;
};

class PlotWidget final : public QWidget
{
public:
    explicit PlotWidget(QWidget* parent = nullptr) : QWidget(parent)
    {
        setMinimumHeight(230);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setAutoFillBackground(true);
    }

    void setCurves(std::vector<PlotCurve> curves, QString xLabel, QString yLabel)
    {
        curves_ = std::move(curves);
        xLabel_ = std::move(xLabel);
        yLabel_ = std::move(yLabel);
        update();
    }

    void setEngineeringGuides(std::vector<PlotBand> resonanceBands,
                              std::optional<double> saturationPeakCurrentA = std::nullopt)
    {
        resonanceBands_ = std::move(resonanceBands);
        saturationPeakCurrentA_ = saturationPeakCurrentA;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.fillRect(rect(), palette().base());
        const QRectF area(76.0, 18.0, std::max(80, width() - 112), std::max(80, height() - 68));
        painter.setPen(palette().color(QPalette::Mid));
        painter.drawRect(area);

        double minX = std::numeric_limits<double>::max();
        double maxX = std::numeric_limits<double>::lowest();
        double minY = 0.0;
        double maxY = std::numeric_limits<double>::lowest();
        bool hasPoints = false;
        for (const auto& curve : curves_) {
            for (const auto& [x, y] : curve.points) {
                if (!std::isfinite(x) || !std::isfinite(y) || x <= 0.0) continue;
                minX = std::min(minX, x);
                maxX = std::max(maxX, x);
                maxY = std::max(maxY, y);
                hasPoints = true;
            }
        }
        if (!hasPoints || !(maxX > minX) || !(maxY > minY)) {
            painter.setPen(palette().color(QPalette::Text));
            painter.drawText(area, Qt::AlignCenter, tr("No frequency-response data available"));
            painter.drawText(QRectF(0, height() - 30, width(), 22), Qt::AlignCenter, xLabel_);
            return;
        }

        const double logMin = std::log10(minX);
        const double logMax = std::log10(maxX);
        const double yMax = std::max(maxY * 1.08,
            saturationPeakCurrentA_.value_or(0.0) * 1.08);
        const auto xPixel = [&area, logMin, logMax](double frequency) {
            return area.left() + (std::log10(frequency) - logMin) /
                (logMax - logMin) * area.width();
        };
        for (const auto& band : resonanceBands_) {
            if (!std::isfinite(band.startHz) || !std::isfinite(band.stopHz) ||
                band.startHz <= 0.0 || band.stopHz <= band.startHz ||
                band.stopHz < minX || band.startHz > maxX) continue;
            const double left = std::clamp(xPixel(std::max(band.startHz, minX)),
                area.left(), area.right());
            const double right = std::clamp(xPixel(std::min(band.stopHz, maxX)),
                area.left(), area.right());
            painter.fillRect(QRectF(left, area.top(), right - left, area.height()),
                QColor(235, 165, 35, 42));
            painter.setPen(QPen(QColor(190, 125, 20), 1.0, Qt::DashLine));
            painter.drawLine(QPointF(left, area.top()), QPointF(left, area.bottom()));
            painter.drawLine(QPointF(right, area.top()), QPointF(right, area.bottom()));
            painter.setPen(QColor(170, 105, 15));
            painter.drawText(QRectF(left + 3, area.top() + 3,
                std::max(0.0, right - left - 6), 18), Qt::AlignLeft | Qt::AlignTop, band.label);
        }
        painter.setPen(palette().color(QPalette::Mid));
        for (int tick = 0; tick <= 4; ++tick) {
            const double fraction = static_cast<double>(tick) / 4.0;
            const double y = area.bottom() - fraction * area.height();
            painter.drawLine(QPointF(area.left(), y), QPointF(area.right(), y));
            painter.drawText(QRectF(4, y - 9, 64, 18), Qt::AlignRight | Qt::AlignVCenter,
                QString::number(yMax * fraction, 'g', 4));
            const double xValue = std::pow(10.0, logMin + (logMax - logMin) * fraction);
            const double x = area.left() + fraction * area.width();
            painter.drawLine(QPointF(x, area.bottom()), QPointF(x, area.bottom() + 4));
            painter.drawText(QRectF(x - 42, area.bottom() + 6, 84, 18),
                Qt::AlignHCenter | Qt::AlignTop, QString::number(xValue, 'g', 3));
        }

        if (saturationPeakCurrentA_ && std::isfinite(*saturationPeakCurrentA_) &&
            *saturationPeakCurrentA_ > 0.0) {
            const double y = area.bottom() - (*saturationPeakCurrentA_ / yMax) * area.height();
            painter.setPen(QPen(QColor(195, 45, 45), 2.0, Qt::DashLine));
            painter.drawLine(QPointF(area.left(), y), QPointF(area.right(), y));
            painter.setPen(QColor(180, 35, 35));
            painter.drawText(QRectF(area.left() + 5, y - 19, area.width() - 10, 17),
                Qt::AlignRight | Qt::AlignVCenter,
                tr("Entered Isat reference: %1 A peak").arg(*saturationPeakCurrentA_, 0, 'g', 5));
        }

        const std::vector<QColor> colors{QColor(35, 105, 190), QColor(205, 70, 55),
            QColor(35, 145, 90), QColor(150, 85, 175)};
        int curveIndex = 0;
        int legendX = static_cast<int>(area.left()) + 8;
        for (const auto& curve : curves_) {
            QPainterPath path;
            bool started = false;
            for (const auto& [xValue, yValue] : curve.points) {
                if (!std::isfinite(xValue) || !std::isfinite(yValue) ||
                    xValue <= 0.0 || yValue < 0.0) continue;
                const double x = area.left() +
                    (std::log10(xValue) - logMin) / (logMax - logMin) * area.width();
                const double y = area.bottom() - (yValue / yMax) * area.height();
                if (!started) { path.moveTo(x, y); started = true; }
                else path.lineTo(x, y);
            }
            const QColor color = colors[static_cast<std::size_t>(curveIndex) % colors.size()];
            painter.setPen(QPen(color, 2.0));
            painter.drawPath(path);
            painter.drawLine(legendX, 10, legendX + 18, 10);
            painter.setPen(palette().color(QPalette::Text));
            const QString label = curve.name;
            painter.drawText(legendX + 23, 15, label);
            legendX += painter.fontMetrics().horizontalAdvance(label) + 42;
            ++curveIndex;
        }
        painter.setPen(palette().color(QPalette::Text));
        painter.drawText(QRectF(0, height() - 22, width(), 20), Qt::AlignCenter, xLabel_);
        painter.save();
        painter.translate(16, area.center().y());
        painter.rotate(-90);
        painter.drawText(QRectF(-area.height() / 2, -10, area.height(), 20),
            Qt::AlignCenter, yLabel_);
        painter.restore();
    }

private:
    std::vector<PlotCurve> curves_;
    QString xLabel_;
    QString yLabel_;
    std::vector<PlotBand> resonanceBands_;
    std::optional<double> saturationPeakCurrentA_;
};


namespace {
bool revisionIsUserDefined(const p135::application::MaterialRevision& revision,
                           const QJsonObject& document)
{
    const QString sourceStatus = document.value(QStringLiteral("dataQuality")).toObject()
        .value(QStringLiteral("sourceStatus")).toString();
    return revision.sourceReference == "user-defined" ||
           revision.sourceReference == "user-entered-unverified" ||
           sourceStatus == QStringLiteral("USER_ENTERED_UNVERIFIED");
}

bool capacitorFromRevision(const p135::application::MaterialRevision& revision,
                           p135::domain::CapacitorComponent& capacitor)
{
    const auto document = QJsonDocument::fromJson(QByteArray::fromStdString(revision.payload));
    if (!document.isObject()) return false;
    const auto root = document.object();
    const auto component = root.value(QStringLiteral("component")).toObject();
    if (component.isEmpty()) return false;

    capacitor = {};
    capacitor.id = component.value(QStringLiteral("id")).toString().toStdString();
    capacitor.manufacturer = component.value(QStringLiteral("manufacturer")).toString().toStdString();
    capacitor.series = component.value(QStringLiteral("series")).toString().toStdString();
    capacitor.partNumber = component.value(QStringLiteral("partNumber")).toString().toStdString();
    capacitor.dataSource = revisionIsUserDefined(revision, root)
        ? p135::domain::MaterialDataSource::UserDefined
        : p135::domain::MaterialDataSource::ManufacturerDatasheet;
    capacitor.evidenceVerified =
        root.value(QStringLiteral("dataQuality")).toObject()
            .value(QStringLiteral("designReleaseStatus")).toString() == QStringLiteral("VERIFIED");
    capacitor.sourceReference = revision.sourceReference;
    capacitor.datasetRevision = revision.datasetRevision;

    const auto number = [&component](const char* key) {
        const auto value = component.value(QLatin1String(key));
        return value.isDouble() ? value.toDouble() : 0.0;
    };
    const auto numberOr = [&component](const char* key, double fallback) {
        const auto value = component.value(QLatin1String(key));
        return value.isDouble() ? value.toDouble() : fallback;
    };
    capacitor.capacitance = p135::units::Capacitance{number("capacitance_F")};
    capacitor.toleranceFraction = numberOr("tolerance_fraction",
        numberOr("capacitanceTolerance_fraction", 0.05));
    capacitor.ratedVoltage = p135::units::Voltage{
        numberOr("ratedVoltageDC_V", number("ratedVoltage_V"))};
    capacitor.voltageRatingIsAC = component.value(QStringLiteral("voltageRatingIsAC")).toBool(false);
    capacitor.minimumTemperature = p135::units::TemperatureKelvin{
        numberOr("minimumTemperature_K", 223.15)};
    capacitor.maximumTemperature = p135::units::TemperatureKelvin{
        numberOr("maximumTemperature_K", number("maximumCaseTemperature_K"))};
    const double esr = numberOr("esr_ohm", number("equivalentSeriesResistance_ohm"));
    if (esr > 0.0) capacitor.esr = p135::units::Resistance{esr};
    capacitor.esrConfirmedFrequencyIndependent =
        component.value(QStringLiteral("esr_frequency_independent_confirmed")).toBool(false);
    const auto curve = component.value(QStringLiteral("esr_frequency_curve")).toArray();
    for (const auto& value : curve) {
        const auto point = value.toObject();
        capacitor.esrFrequencyCurve.push_back({
            p135::units::Frequency{point.value(QStringLiteral("frequency_Hz")).toDouble()},
            p135::units::Resistance{point.value(QStringLiteral("esr_ohm")).toDouble()}
        });
    }
    const double esl = number("esl_H", number("equivalentSeriesInductance_H"));
    if (esl > 0.0) capacitor.esl = p135::units::Inductance{esl};
    const double ripple = number("rippleCurrent_A");
    if (ripple > 0.0) capacitor.rippleCurrent = p135::units::Current{ripple};
    return static_cast<bool>(p135::domain::validate(capacitor));
}

bool resistorFromRevision(const p135::application::MaterialRevision& revision,
                          p135::domain::ResistorComponent& resistor)
{
    const auto document = QJsonDocument::fromJson(QByteArray::fromStdString(revision.payload));
    if (!document.isObject()) return false;
    const auto root = document.object();
    const auto component = root.value(QStringLiteral("component")).toObject();
    if (component.isEmpty()) return false;

    resistor = {};
    resistor.id = component.value(QStringLiteral("id")).toString().toStdString();
    resistor.manufacturer = component.value(QStringLiteral("manufacturer")).toString().toStdString();
    resistor.series = component.value(QStringLiteral("series")).toString().toStdString();
    resistor.partNumber = component.value(QStringLiteral("partNumber")).toString().toStdString();
    resistor.dataSource = revisionIsUserDefined(revision, root)
        ? p135::domain::MaterialDataSource::UserDefined
        : p135::domain::MaterialDataSource::ManufacturerDatasheet;
    resistor.evidenceVerified =
        root.value(QStringLiteral("dataQuality")).toObject()
            .value(QStringLiteral("designReleaseStatus")).toString() == QStringLiteral("VERIFIED");
    resistor.sourceReference = revision.sourceReference;
    resistor.datasetRevision = revision.datasetRevision;
    resistor.resistance = p135::units::Resistance{
        component.value(QStringLiteral("resistance_ohm")).toDouble()};
    resistor.toleranceFraction = component.value(QStringLiteral("tolerance_fraction")).toDouble();
    resistor.ratedPower = p135::units::Power{
        component.value(QStringLiteral("ratedPower_W")).toDouble()};
    resistor.maximumWorkingVoltage = p135::units::Voltage{
        component.value(QStringLiteral("maximumWorkingVoltage_V")).toDouble()};
    resistor.minimumTemperature = p135::units::TemperatureKelvin{
        component.value(QStringLiteral("minimumTemperature_K")).toDouble()};
    resistor.maximumTemperature = p135::units::TemperatureKelvin{
        component.value(QStringLiteral("maximumTemperature_K")).toDouble()};
    const auto tcr = component.value(QStringLiteral("temperatureCoefficient_per_K"));
    if (tcr.isDouble())
        resistor.temperatureCoefficient = p135::units::TemperatureCoefficient{tcr.toDouble()};
    return static_cast<bool>(p135::domain::validate(resistor));
}
} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    QSettings settings;
    projectId_ = settings.value(QStringLiteral("project/currentId")).toString();
    if (projectId_.isEmpty()) {
        projectId_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
        settings.setValue(QStringLiteral("project/currentId"), projectId_);
    }
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
                const QString calculationMigrationPath = QCoreApplication::applicationDirPath()
                    + "/migrations/002_calculation_snapshots.sql";
                const auto calculationMigrated = migrations.apply(
                    p135::infrastructure::sqlite::Migration{
                        2, "calculation_snapshots",
                        std::filesystem::path(calculationMigrationPath.toStdString())
                    });
                if (!calculationMigrated) {
                    databaseError_ = QString::fromStdString(calculationMigrated.error().message);
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
    }

    auto* fileMenu = menuBar()->addMenu(tr("&File"));
    auto* newProject = fileMenu->addAction(tr("New project"));
    connect(newProject, &QAction::triggered, this, [this] {
        projectId_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
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
    viewMenu->addAction(tr("Calculation history"), this, [this] {
        tabs_->setCurrentIndex(5);
        refreshCalculationHistory();
    });

    auto* toolbar = addToolBar(tr("Main"));
    toolbar->addAction(tr("New project"), this, [this] {
        projectId_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
        QSettings settings;
        settings.setValue(QStringLiteral("project/currentId"), projectId_);
        projectName_->clear();
        updateProjectTitle();
        tabs_->setCurrentIndex(0);
    });
    toolbar->addAction(tr("Project"), this, [this] { tabs_->setCurrentIndex(0); });
    toolbar->addAction(tr("Materials"), this, [this] { tabs_->setCurrentIndex(4); });
    toolbar->addAction(tr("History"), this, [this] {
        tabs_->setCurrentIndex(5);
        refreshCalculationHistory();
    });
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
    tabs_->addTab(createHistoryPage(), tr("Calculation history"));
    setCentralWidget(tabs_);
    connect(tabs_, &QTabWidget::currentChanged, this, [this](int index) {
        if (index == 5) refreshCalculationHistory();
    });

    projectName_->setText(settings.value(QStringLiteral("project/name"), projectName_->text()).toString());
    const int savedFamily = productFamily_->findText(
        settings.value(QStringLiteral("project/productFamily"), productFamily_->currentText()).toString());
    if (savedFamily >= 0) productFamily_->setCurrentIndex(savedFamily);

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
        QSettings settings;
        settings.setValue(QStringLiteral("project/productFamily"), value);
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

    auto* componentGroup = new QGroupBox(tr("Selected components and installation thermal data"));
    auto* componentForm = new QFormLayout(componentGroup);
    capacitorComponent_ = new QComboBox;
    resistorComponent_ = new QComboBox;
    ambientTemperature_ = new QDoubleSpinBox;
    ambientTemperature_->setRange(-50.0, 150.0);
    ambientTemperature_->setDecimals(2);
    ambientTemperature_->setValue(25.0);
    ambientTemperature_->setSuffix(tr(" °C"));
    capacitorThermalResistance_ = new QDoubleSpinBox;
    capacitorThermalResistance_->setRange(0.0, 1000000.0);
    capacitorThermalResistance_->setDecimals(4);
    capacitorThermalResistance_->setSuffix(tr(" K/W"));
    capacitorThermalSource_ = new QLineEdit;
    capacitorThermalSource_->setPlaceholderText(tr("Rθ source: assembly datasheet or measurement"));
    resistorThermalResistance_ = new QDoubleSpinBox;
    resistorThermalResistance_->setRange(0.0, 1000000.0);
    resistorThermalResistance_->setDecimals(4);
    resistorThermalResistance_->setSuffix(tr(" K/W"));
    resistorThermalSource_ = new QLineEdit;
    resistorThermalSource_->setPlaceholderText(tr("Rθ source: mounting/heatsink measurement"));
    componentForm->addRow(tr("Thermal ambient temperature"), ambientTemperature_);
    componentForm->addRow(tr("Filter capacitor revision"), capacitorComponent_);
    componentForm->addRow(tr("Capacitor-to-ambient Rθ"), capacitorThermalResistance_);
    componentForm->addRow(tr("Capacitor Rθ evidence"), capacitorThermalSource_);
    componentForm->addRow(tr("Damping resistor revision"), resistorComponent_);
    componentForm->addRow(tr("Resistor-to-ambient Rθ"), resistorThermalResistance_);
    componentForm->addRow(tr("Resistor Rθ evidence"), resistorThermalSource_);
    auto* refreshComponents = new QPushButton(tr("Refresh component revisions"));
    componentForm->addRow(refreshComponents);
    connect(refreshComponents, &QPushButton::clicked, this, [this] { refreshComponentSelectors(); });
    layout->addWidget(componentGroup);
    refreshComponentSelectors();

    auto* note = new QLabel(tr("The target reactive power is the total three-phase fundamental-frequency value for this branch. Synthesis uses ideal C and L and derives a series damping resistor from Q factor. Optional source impedance and harmonic spectrum inputs below enable impedance-scan, THD and branch-stress estimates; component ratings, thermal derating and standards compliance remain outside this calculation."));
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

    inductorSaturationCurrent_ = new QDoubleSpinBox;
    inductorSaturationCurrent_->setRange(0.0, 10000000.0);
    inductorSaturationCurrent_->setDecimals(3);
    inductorSaturationCurrent_->setValue(0.0);
    inductorSaturationCurrent_->setSuffix(tr(" A peak / phase"));
    inductorSaturationSource_ = new QLineEdit;
    inductorSaturationSource_->setPlaceholderText(
        tr("Datasheet / manufacturer part number / test report"));
    sourceForm->addRow(tr("Inductor saturation-current reference (Isat)"),
        inductorSaturationCurrent_);
    sourceForm->addRow(tr("Isat evidence reference"), inductorSaturationSource_);
    advancedLayout->addLayout(sourceForm);

    auto* saturationHelp = new QLabel(tr(
        "The red line compares the calculated peak phase current (√2 × RMS) with the entered manufacturer Isat reference. Enter a peak-current rating per phase and its source. This is a reference comparison, not a magnetic-core saturation simulation: DC bias, core geometry, B-H curve and inductance roll-off are not modelled."));
    saturationHelp->setWordWrap(true);
    advancedLayout->addWidget(saturationHelp);

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

    auto* currentPlotTitle = new QLabel(tr("Inductor phase peak current — frequency sweep"));
    currentPlotTitle->setStyleSheet("font-weight: 600;");
    layout->addWidget(currentPlotTitle);
    resultsBankCurrentPlot_ = new PlotWidget;
    resultsBankCurrentPlot_->setCurves({}, tr("Frequency (Hz, logarithmic)"), tr("Phase peak current (A)"));
    layout->addWidget(resultsBankCurrentPlot_);

    auto* impedancePlotTitle = new QLabel(tr("Bus impedance — frequency scan"));
    impedancePlotTitle->setStyleSheet("font-weight: 600;");
    layout->addWidget(impedancePlotTitle);
    resultsBusImpedancePlot_ = new PlotWidget;
    resultsBusImpedancePlot_->setCurves({}, tr("Frequency (Hz, logarithmic)"), tr("Bus impedance (Ω)"));
    layout->addWidget(resultsBusImpedancePlot_);

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

QWidget* MainWindow::createHistoryPage()
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    auto* heading = new QLabel(tr("Calculation history"));
    heading->setStyleSheet("font-size: 20px; font-weight: 600;");
    layout->addWidget(heading);

    auto* searchRow = new QHBoxLayout;
    historySearch_ = new QLineEdit;
    historySearch_->setPlaceholderText(tr("Search by calculation ID, project, product family or author"));
    auto* refresh = new QPushButton(tr("Refresh"));
    searchRow->addWidget(historySearch_, 1);
    searchRow->addWidget(refresh);
    layout->addLayout(searchRow);

    historyTable_ = new QTableWidget(0, 5);
    historyTable_->setHorizontalHeaderLabels({
        tr("Created (UTC)"), tr("Project"), tr("Product"), tr("Calculation ID"), tr("Author")
    });
    historyTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    historyTable_->horizontalHeader()->setStretchLastSection(true);
    historyTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    historyTable_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    historyTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(historyTable_, 2);

    auto* compareRow = new QHBoxLayout;
    historyPlotMetric_ = new QComboBox;
    historyPlotMetric_->addItem(tr("Phase peak current (A)"), QStringLiteral("phasePeakCurrent_A"));
    historyPlotMetric_->addItem(tr("Bus impedance (Ω)"), QStringLiteral("busImpedance_Ohm"));
    auto* compareGraphs = new QPushButton(tr("Compare graphs of selected calculations"));
    compareRow->addWidget(historyPlotMetric_);
    compareRow->addWidget(compareGraphs);
    layout->addLayout(compareRow);
    historyComparisonPlot_ = new PlotWidget;
    historyComparisonPlot_->setCurves({}, tr("Frequency (Hz, logarithmic)"), tr("Selected metric"));
    layout->addWidget(historyComparisonPlot_);

    auto* loadInputs = new QPushButton(tr("Load selected inputs into editor"));
    layout->addWidget(loadInputs);

    historyDetails_ = new QPlainTextEdit;
    historyDetails_->setReadOnly(true);
    historyDetails_->setPlaceholderText(tr("Select a calculation to inspect its immutable inputs, results, engine versions and exact material revisions."));
    historyDetails_->setMinimumHeight(240);
    layout->addWidget(historyDetails_, 3);

    connect(refresh, &QPushButton::clicked, this, [this] { refreshCalculationHistory(); });
    connect(historySearch_, &QLineEdit::returnPressed, this, [this] { refreshCalculationHistory(); });
    connect(historyTable_, &QTableWidget::currentCellChanged, this,
        [this](int currentRow, int, int, int) { showCalculationDetails(currentRow); });
    connect(loadInputs, &QPushButton::clicked, this, [this] {
        if (historyTable_) loadCalculationInputs(historyTable_->currentRow());
    });
    connect(compareGraphs, &QPushButton::clicked, this,
        [this] { compareSelectedCalculationGraphs(); });
    connect(historyPlotMetric_, &QComboBox::currentIndexChanged, this,
        [this](int) { compareSelectedCalculationGraphs(); });
    refreshCalculationHistory();
    return page;
}

void MainWindow::refreshCalculationHistory()
{
    if (!historyTable_ || !historyDetails_) return;
    historyTable_->setRowCount(0);
    historyDetails_->clear();
    if (historyComparisonPlot_)
        historyComparisonPlot_->setCurves({}, tr("Frequency (Hz, logarithmic)"), tr("Select two calculations"));
    if (!materialStore_) {
        historyDetails_->setPlainText(tr("Calculation history is unavailable: %1").arg(databaseError_));
        return;
    }
    const auto records = materialStore_->listCalculations(
        historySearch_ ? historySearch_->text().trimmed().toStdString() : std::string{}, 500, 0);
    if (!records) {
        historyDetails_->setPlainText(tr("Cannot load calculation history: %1")
            .arg(QString::fromStdString(records.error().message)));
        return;
    }
    for (const auto& record : records.value()) {
        const int row = historyTable_->rowCount();
        historyTable_->insertRow(row);
        const QStringList values{
            QString::fromStdString(record.createdAtUtc),
            QString::fromStdString(record.projectName),
            QString::fromStdString(record.productFamily),
            QString::fromStdString(record.calculationId),
            QString::fromStdString(record.author)
        };
        for (int column = 0; column < values.size(); ++column) {
            auto* item = new QTableWidgetItem(values[column]);
            if (column == 3)
                item->setData(Qt::UserRole, QString::fromStdString(record.calculationId));
            historyTable_->setItem(row, column, item);
        }
    }
    if (historyTable_->rowCount() > 0) {
        historyTable_->selectRow(0);
        showCalculationDetails(0);
    } else {
        historyDetails_->setPlainText(tr("No saved calculations match this search."));
    }
    if (status_) status_->setText(tr("Calculation history: %1 snapshot(s)").arg(records.value().size()));
}

void MainWindow::showCalculationDetails(int row)
{
    if (!historyTable_ || !historyDetails_ || !materialStore_ ||
        row < 0 || row >= historyTable_->rowCount()) return;
    const auto* idItem = historyTable_->item(row, 3);
    if (!idItem) return;
    const auto record = materialStore_->findCalculation(
        idItem->data(Qt::UserRole).toString().toStdString());
    if (!record || !record.value()) {
        historyDetails_->setPlainText(record
            ? tr("Calculation snapshot no longer exists.")
            : tr("Cannot read calculation: %1").arg(QString::fromStdString(record.error().message)));
        return;
    }
    const auto& value = *record.value();
    QString details;
    details += tr("Calculation: %1\nProject: %2 (%3)\nProduct family: %4\n"
                  "Created (UTC): %5\nAuthor: %6\nSHA-256: %7\n")
        .arg(QString::fromStdString(value.calculationId),
             QString::fromStdString(value.projectName),
             QString::fromStdString(value.projectId),
             QString::fromStdString(value.productFamily),
             QString::fromStdString(value.createdAtUtc),
             QString::fromStdString(value.author),
             QString::fromStdString(value.contentHash));
    const auto prettyJson = [](const std::string& json) {
        const auto document = QJsonDocument::fromJson(QByteArray::fromStdString(json));
        return document.isNull() ? QString::fromStdString(json)
            : QString::fromUtf8(document.toJson(QJsonDocument::Indented));
    };
    details += tr("\n=== INPUT PARAMETERS ===\n%1").arg(prettyJson(value.inputJson));
    details += tr("\n=== RESULTS ===\n%1").arg(prettyJson(value.resultJson));
    details += tr("\n=== ENGINE VERSIONS ===\n%1").arg(prettyJson(value.engineVersionsJson));
    details += tr("\n=== EXACT MATERIAL REVISIONS ===\n");
    const auto links = materialStore_->calculationMaterials(value.calculationId);
    if (!links) {
        details += tr("Cannot load material links: %1").arg(QString::fromStdString(links.error().message));
    } else if (links.value().empty()) {
        details += tr("No material revisions were selected for this calculation.\n");
    } else {
        for (const auto& link : links.value()) {
            const auto revision = materialStore_->findRevision(link.revisionId);
            if (!revision || !revision.value()) {
                details += tr("Revision %1: unavailable (%2)\n")
                    .arg(QString::fromStdString(link.revisionId),
                         revision ? tr("not found") : QString::fromStdString(revision.error().message));
                continue;
            }
            details += tr("Usage %1 — material %2, revision %3 (r%4), type %5, dataset %6, source %7\n")
                .arg(QString::fromStdString(link.usageId),
                     QString::fromStdString(revision.value()->materialId),
                     QString::fromStdString(revision.value()->revisionId))
                .arg(revision.value()->revisionNumber)
                .arg(QString::fromStdString(revision.value()->recordType),
                     QString::fromStdString(revision.value()->datasetRevision),
                     QString::fromStdString(revision.value()->sourceReference));
        }
    }
    historyDetails_->setPlainText(details);
}

void MainWindow::compareSelectedCalculationGraphs()
{
    if (!historyTable_ || !historyComparisonPlot_ || !materialStore_ || !historyPlotMetric_)
        return;
    const auto selected = historyTable_->selectionModel()->selectedRows();
    if (selected.size() != 2) {
        historyComparisonPlot_->setCurves({}, tr("Frequency (Hz, logarithmic)"),
            tr("Select exactly two calculations"));
        return;
    }

    const QString metricKey = historyPlotMetric_->currentData().toString();
    const bool currentMetric = metricKey == QStringLiteral("phasePeakCurrent_A");
    const QString arrayKey = currentMetric ? QStringLiteral("frequencySweep")
                                            : QStringLiteral("impedanceScan");
    const QString yLabel = currentMetric ? tr("Phase peak current (A)") : tr("Bus impedance (Ω)");
    std::vector<PlotCurve> curves;
    QStringList comparisonIds;

    for (const auto& selection : selected) {
        const int row = selection.row();
        const auto* idItem = historyTable_->item(row, 3);
        if (!idItem) continue;
        const QString calculationId = idItem->data(Qt::UserRole).toString();
        const auto record = materialStore_->findCalculation(calculationId.toStdString());
        if (!record || !record.value()) continue;

        const auto document = QJsonDocument::fromJson(
            QByteArray::fromStdString(record.value()->resultJson));
        if (!document.isObject()) continue;
        const auto samples = document.object().value(arrayKey).toArray();
        PlotCurve curve;
        curve.name = QString::fromStdString(record.value()->projectName) +
            QStringLiteral(" / ") + QString::fromStdString(record.value()->createdAtUtc) +
            QStringLiteral(" [") + calculationId.left(8) + QStringLiteral("]");
        for (const auto& sampleValue : samples) {
            const auto sample = sampleValue.toObject();
            const auto frequency = sample.value(QStringLiteral("frequency_Hz"));
            const auto ordinate = sample.value(metricKey);
            if (!frequency.isDouble() || !ordinate.isDouble()) continue;
            curve.points.emplace_back(frequency.toDouble(), ordinate.toDouble());
        }
        if (curve.points.size() >= 2) {
            curves.push_back(std::move(curve));
            comparisonIds.push_back(calculationId);
        }
    }

    if (curves.size() != 2) {
        historyComparisonPlot_->setCurves({}, tr("Frequency (Hz, logarithmic)"), yLabel);
        historyDetails_->appendPlainText(tr("\nGraph comparison unavailable: one or both selected snapshots do not contain this frequency series. Older snapshots created before D43 have no stored plot points."));
        return;
    }
    historyComparisonPlot_->setCurves(std::move(curves),
        tr("Frequency (Hz, logarithmic)"), yLabel);
    historyDetails_->appendPlainText(tr("\nGraph comparison: %1 vs %2 (%3).")
        .arg(comparisonIds[0], comparisonIds[1], yLabel));
}

void MainWindow::loadCalculationInputs(int row)
{
    if (!historyTable_ || !materialStore_ || row < 0 || row >= historyTable_->rowCount())
        return;
    const auto* idItem = historyTable_->item(row, 3);
    if (!idItem) return;
    const auto record = materialStore_->findCalculation(
        idItem->data(Qt::UserRole).toString().toStdString());
    if (!record || !record.value()) {
        QMessageBox::warning(this, tr("Load calculation inputs"),
            record ? tr("The selected calculation no longer exists.")
                   : QString::fromStdString(record.error().message));
        return;
    }

    const auto document = QJsonDocument::fromJson(
        QByteArray::fromStdString(record.value()->inputJson));
    if (!document.isObject()) {
        QMessageBox::warning(this, tr("Load calculation inputs"),
            tr("The saved input snapshot is not a valid JSON object."));
        return;
    }
    const auto input = document.object();
    if (projectName_) projectName_->setText(QString::fromStdString(record.value()->projectName));
    projectId_ = QString::fromStdString(record.value()->projectId);
    const auto setNumber = [&input](const char* key, QDoubleSpinBox* control, double divisor = 1.0) {
        if (control && input.value(QLatin1String(key)).isDouble())
            control->setValue(input.value(QLatin1String(key)).toDouble() / divisor);
    };
    setNumber("nominalLineVoltage_V", nominalVoltage_);
    setNumber("fundamentalFrequency_Hz", fundamentalFrequency_);
    setNumber("targetReactivePower_var", targetReactivePower_, 1000.0);
    setNumber("qualityFactor", qualityFactor_);
    setNumber("sourceResistance_ohm", sourceResistance_);
    setNumber("sourceInductance_H", sourceInductance_);
    setNumber("inductorSaturationCurrent_A_peak", inductorSaturationCurrent_);
    if (inductorSaturationSource_)
        inductorSaturationSource_->setText(
            input.value(QStringLiteral("inductorSaturationSource")).toString());
    setNumber("ambientTemperature_C", ambientTemperature_);
    setNumber("capacitorThermalResistance_K_per_W", capacitorThermalResistance_);
    setNumber("resistorThermalResistance_K_per_W", resistorThermalResistance_);
    if (tuningHarmonicOrder_ && input.value(QStringLiteral("tuningHarmonicOrder")).isDouble())
        tuningHarmonicOrder_->setValue(input.value(QStringLiteral("tuningHarmonicOrder")).toInt());
    if (filterConnection_) {
        const int index = filterConnection_->findText(input.value(QStringLiteral("connection")).toString());
        if (index >= 0) filterConnection_->setCurrentIndex(index);
    }
    if (productFamily_) {
        const int index = productFamily_->findText(QString::fromStdString(record.value()->productFamily));
        if (index >= 0) productFamily_->setCurrentIndex(index);
    }
    if (capacitorThermalSource_)
        capacitorThermalSource_->setText(input.value(QStringLiteral("capacitorThermalSource")).toString());
    if (resistorThermalSource_)
        resistorThermalSource_->setText(input.value(QStringLiteral("resistorThermalSource")).toString());

    const auto restoreRevision = [this](QComboBox* selector, const QString& revisionId) {
        if (!selector || revisionId.isEmpty()) return;
        int index = selector->findData(revisionId);
        if (index < 0 && materialStore_) {
            const auto revision = materialStore_->findRevision(revisionId.toStdString());
            if (revision && revision.value()) {
                const QString label = QString::fromStdString(revision.value()->materialId)
                    + tr(" — r%1 (historical)").arg(revision.value()->revisionNumber);
                selector->addItem(label, revisionId);
                index = selector->count() - 1;
            }
        }
        if (index >= 0) selector->setCurrentIndex(index);
        else appendLog(tr("Saved material revision %1 is unavailable; selection was not substituted.")
            .arg(revisionId));
    };
    restoreRevision(capacitorComponent_,
        input.value(QStringLiteral("capacitorRevisionId")).toString());
    restoreRevision(resistorComponent_,
        input.value(QStringLiteral("resistorRevisionId")).toString());

    if (harmonicSpectrumTable_) {
        const auto spectrum = input.value(QStringLiteral("harmonicSpectrum")).toArray();
        harmonicSpectrumTable_->setRowCount(0);
        for (const auto& entry : spectrum) {
            const auto point = entry.toObject();
            const int newRow = harmonicSpectrumTable_->rowCount();
            harmonicSpectrumTable_->insertRow(newRow);
            const QStringList values{
                QString::number(point.value(QStringLiteral("order")).toInt()),
                point.value(QStringLiteral("sequence")).toString(),
                QString::number(point.value(QStringLiteral("voltageRms_V")).toDouble(), 'g', 12),
                QString::number(point.value(QStringLiteral("phase_deg")).toDouble(), 'g', 12)
            };
            for (int column = 0; column < values.size(); ++column)
                harmonicSpectrumTable_->setItem(newRow, column, new QTableWidgetItem(values[column]));
        }
    }

    updateProjectTitle();
    tabs_->setCurrentIndex(1);
    status_->setText(tr("Historical inputs loaded — review before running a new calculation"));
    appendLog(tr("Loaded inputs from calculation %1. Exact historical material revision IDs were restored where available.")
        .arg(QString::fromStdString(record.value()->calculationId)));
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

    std::optional<p135::filter::HarmonicAnalysisResult> harmonicResult;
    std::optional<p135::filter::FrequencyResponseResult> impedanceResult;
    QString advancedWarning;
    const double sourceResistance = sourceResistance_ ? sourceResistance_->value() : 0.0;
    const double sourceInductance = sourceInductance_
        ? sourceInductance_->value() * 1.0e-3 : 0.0;
    const bool hasSourceImpedance = sourceResistance > 0.0 || sourceInductance > 0.0;

    if (hasSourceImpedance) {
        p135::filter::FrequencyResponseRequest impedanceRequest;
        impedanceRequest.design = result.value();
        impedanceRequest.source = {
            p135::units::Resistance{sourceResistance},
            p135::units::Inductance{sourceInductance}
        };
        impedanceRequest.startFrequency =
            p135::units::Frequency{fundamentalFrequency_->value() * 0.5};
        impedanceRequest.stopFrequency =
            p135::units::Frequency{fundamentalFrequency_->value() * 15.0};
        impedanceRequest.points = 501;
        impedanceRequest.logarithmicSpacing = true;
        const auto impedance = p135::filter::analyzeFrequencyResponse(impedanceRequest);
        if (impedance) {
            impedanceResult = impedance.value();
        } else {
            advancedWarning = tr("Impedance scan NOT_VERIFIED: %1")
                .arg(QString::fromStdString(impedance.error().message));
        }
    }

    int suppliedHarmonics = 0;
    bool spectrumValid = true;
    p135::filter::HarmonicAnalysisRequest harmonicRequest;
    harmonicRequest.design = result.value();
    harmonicRequest.source = {
        p135::units::Resistance{sourceResistance},
        p135::units::Inductance{sourceInductance}
    };
    constexpr double pi = 3.1415926535897932384626433832795;
    const double fundamentalPhaseVoltage = nominalVoltage_->value() / std::sqrt(3.0);
    harmonicRequest.sourceVoltageSpectrum.push_back({
        1,
        p135::filter::HarmonicSequence::Positive,
        {fundamentalPhaseVoltage, 0.0}
    });

    if (harmonicSpectrumTable_) {
        for (int row = 0; row < harmonicSpectrumTable_->rowCount(); ++row) {
            const auto* orderItem = harmonicSpectrumTable_->item(row, 0);
            const auto* sequenceItem = harmonicSpectrumTable_->item(row, 1);
            const auto* magnitudeItem = harmonicSpectrumTable_->item(row, 2);
            const auto* phaseItem = harmonicSpectrumTable_->item(row, 3);
            if (!orderItem || !sequenceItem || !magnitudeItem || !phaseItem) {
                advancedWarning = tr("Harmonic spectrum row %1 is incomplete.").arg(row + 1);
                spectrumValid = false;
                break;
            }

            bool orderOk = false;
            bool magnitudeOk = false;
            bool phaseOk = false;
            const uint order = orderItem->text().toUInt(&orderOk);
            const double magnitude = magnitudeItem->text().toDouble(&magnitudeOk);
            const double phaseDegrees = phaseItem->text().toDouble(&phaseOk);
            if (!orderOk || order < 2 || !magnitudeOk || !phaseOk ||
                !std::isfinite(magnitude) || magnitude < 0.0 ||
                !std::isfinite(phaseDegrees)) {
                advancedWarning = tr("Harmonic spectrum row %1 has invalid order, RMS voltage or phase.")
                    .arg(row + 1);
                spectrumValid = false;
                break;
            }
            if (magnitude == 0.0)
                continue;

            p135::filter::HarmonicSequence sequence;
            if (sequenceItem->text().compare(tr("Positive"), Qt::CaseInsensitive) == 0) {
                sequence = p135::filter::HarmonicSequence::Positive;
            } else if (sequenceItem->text().compare(tr("Negative"), Qt::CaseInsensitive) == 0) {
                sequence = p135::filter::HarmonicSequence::Negative;
            } else {
                advancedWarning = tr("Harmonic spectrum row %1 sequence must be Positive or Negative.")
                    .arg(row + 1);
                spectrumValid = false;
                break;
            }

            harmonicRequest.sourceVoltageSpectrum.push_back({
                static_cast<std::uint32_t>(order),
                sequence,
                std::polar(magnitude, phaseDegrees * pi / 180.0)
            });
            ++suppliedHarmonics;
        }
    }

    if (!hasSourceImpedance && suppliedHarmonics > 0) {
        advancedWarning = tr("Harmonic analysis NOT_VERIFIED: enter source R and/or L from a network study.");
    } else if (hasSourceImpedance && suppliedHarmonics > 0 && spectrumValid) {
        const auto harmonics = p135::filter::analyzeHarmonics(harmonicRequest);
        if (harmonics) {
            harmonicResult = harmonics.value();
        } else {
            advancedWarning = tr("Harmonic analysis NOT_VERIFIED: %1")
                .arg(QString::fromStdString(harmonics.error().message));
        }
    } else if (!spectrumValid && advancedWarning.isEmpty()) {
        advancedWarning = tr("Harmonic analysis NOT_VERIFIED: invalid input spectrum.");
    } else if (suppliedHarmonics == 0 && advancedWarning.isEmpty()) {
        advancedWarning = tr("Harmonic THD NOT_VERIFIED: no non-fundamental voltage harmonics were entered.");
    }

    std::optional<p135::thermal::CapacitorThermalResult> capacitorThermalResult;
    std::optional<p135::thermal::SteadyStateResult> resistorThermalResult;
    QString componentAssessmentWarning;
    if (harmonicResult && materialStore_) {
        const QString capacitorRevisionId = capacitorComponent_
            ? capacitorComponent_->currentData().toString() : QString{};
        if (!capacitorRevisionId.isEmpty()) {
            const auto revision = materialStore_->findRevision(capacitorRevisionId.toStdString());
            if (revision && revision.value()) {
                p135::domain::CapacitorComponent capacitor;
                if (capacitorFromRevision(*revision.value(), capacitor)) {
                    p135::domain::CapacitorCatalog catalog;
                    if (catalog.add(capacitor)) {
                        p135::thermal::FilterCapacitorThermalAssignment assignment;
                        assignment.branchId = 1;
                        assignment.capacitorComponentId = capacitor.id;
                        if (capacitorThermalResistance_ && capacitorThermalResistance_->value() > 0.0)
                            assignment.thermalResistanceKPerW = capacitorThermalResistance_->value();
                        if (capacitorThermalSource_)
                            assignment.thermalResistanceSourceReference =
                                capacitorThermalSource_->text().trimmed().toStdString();
                        const auto assessment = p135::thermal::analyzeFilterCapacitorThermals(
                            *harmonicResult, {assignment}, catalog,
                            p135::units::TemperatureKelvin{
                                (ambientTemperature_ ? ambientTemperature_->value() : 25.0) + 273.15},
                            QStringLiteral("desktop-harmonic-study-%1")
                                .arg(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs))
                                .toStdString());
                        if (assessment) capacitorThermalResult = assessment.value();
                        else componentAssessmentWarning += tr(" Capacitor thermal assessment failed: %1")
                            .arg(QString::fromStdString(assessment.error().message));
                    }
                } else {
                    componentAssessmentWarning += tr(" Selected capacitor revision is not a supported typed component snapshot.");
                }
            } else {
                componentAssessmentWarning += tr(" Selected capacitor revision could not be loaded from SQLite.");
            }
        }

        const QString resistorRevisionId = resistorComponent_
            ? resistorComponent_->currentData().toString() : QString{};
        if (!resistorRevisionId.isEmpty()) {
            const auto revision = materialStore_->findRevision(resistorRevisionId.toStdString());
            if (revision && revision.value()) {
                p135::domain::ResistorComponent resistor;
                if (resistorFromRevision(*revision.value(), resistor)) {
                    p135::domain::ResistorCatalog catalog;
                    if (catalog.add(resistor)) {
                        p135::thermal::DampingResistorThermalAssignment assignment;
                        assignment.branchId = 1;
                        assignment.resistorComponentId = resistor.id;
                        if (resistorThermalResistance_ && resistorThermalResistance_->value() > 0.0)
                            assignment.thermalResistanceKPerW = resistorThermalResistance_->value();
                        if (resistorThermalSource_)
                            assignment.thermalResistanceSourceReference =
                                resistorThermalSource_->text().trimmed().toStdString();
                        const auto assessment = p135::thermal::analyzeDampingResistorThermals(
                            *harmonicResult, {assignment}, catalog,
                            p135::units::TemperatureKelvin{
                                (ambientTemperature_ ? ambientTemperature_->value() : 25.0) + 273.15},
                            QStringLiteral("desktop-harmonic-study-%1")
                                .arg(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs))
                                .toStdString());
                        if (assessment) resistorThermalResult = assessment.value();
                        else componentAssessmentWarning += tr(" Resistor thermal assessment failed: %1")
                            .arg(QString::fromStdString(assessment.error().message));
                    }
                } else {
                    componentAssessmentWarning += tr(" Selected resistor revision is not a supported typed component snapshot.");
                }
            } else {
                componentAssessmentWarning += tr(" Selected resistor revision could not be loaded from SQLite.");
            }
        }
    } else if (!harmonicResult) {
        componentAssessmentWarning = tr(" Component losses and thermal assessment NOT_VERIFIED until a valid harmonic study is available.");
    }

    const auto& branch = result.value().branches.front();
    const auto addResult = [this](const QString& name, double value, const QString& unit) {
        const int row = resultsTable_->rowCount();
        resultsTable_->insertRow(row);
        resultsTable_->setItem(row, 0, new QTableWidgetItem(name));
        resultsTable_->setItem(row, 1, new QTableWidgetItem(QString::number(value, 'g', 10)));
        resultsTable_->setItem(row, 2, new QTableWidgetItem(unit));
    };
    const auto addTextResult = [this](const QString& name, const QString& value) {
        const int row = resultsTable_->rowCount();
        resultsTable_->insertRow(row);
        resultsTable_->setItem(row, 0, new QTableWidgetItem(name));
        resultsTable_->setItem(row, 1, new QTableWidgetItem(value));
        resultsTable_->setItem(row, 2, new QTableWidgetItem(QStringLiteral("—")));
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

    if (impedanceResult) {
        addResult(tr("Source/filter impedance scan points"),
                  static_cast<double>(impedanceResult->points.size()), tr("points"));
        addResult(tr("Detected parallel-resonance candidates"),
                  static_cast<double>(impedanceResult->resonancePeaks.size()), tr("peaks"));
        if (!impedanceResult->resonancePeaks.empty()) {
            const auto* strongest = &impedanceResult->resonancePeaks.front();
            for (const auto& peak : impedanceResult->resonancePeaks) {
                if (peak.busImpedanceMagnitudeOhm > strongest->busImpedanceMagnitudeOhm)
                    strongest = &peak;
            }
            addResult(tr("Strongest resonance candidate frequency"),
                      strongest->frequency.si(), tr("Hz"));
            addResult(tr("Strongest resonance candidate bus impedance"),
                      strongest->busImpedanceMagnitudeOhm, tr("Ω"));
        }
    }

    if (capacitorThermalResult && !capacitorThermalResult->capacitors.empty()) {
        const auto& assessment = capacitorThermalResult->capacitors.front();
        addResult(tr("Selected capacitor ESR loss"),
                  assessment.esrLoss ? assessment.esrLoss->si() : 0.0,
                  assessment.esrLoss ? tr("W") : tr("NOT_VERIFIED"));
        addResult(tr("Capacitor ripple current"),
                  assessment.rippleCurrentRmsA, tr("A RMS"));
        addTextResult(tr("Capacitor ripple-current status"),
            assessment.rippleCurrentExplanation);
        if (assessment.thermal.estimatedTemperature)
            addResult(tr("Capacitor estimated temperature"),
                      assessment.thermal.estimatedTemperature->si() - 273.15, tr("°C"));
        addTextResult(tr("Capacitor thermal status"), assessment.thermal.explanation);
    }
    if (resistorThermalResult && !resistorThermalResult->nodes.empty()) {
        const auto& assessment = resistorThermalResult->nodes.front();
        addResult(tr("Damping resistor harmonic loss"),
                  assessment.loss ? assessment.loss->si() : 0.0,
                  assessment.loss ? tr("W") : tr("NOT_VERIFIED"));
        if (assessment.estimatedTemperature)
            addResult(tr("Damping resistor estimated temperature"),
                      assessment.estimatedTemperature->si() - 273.15, tr("°C"));
        addTextResult(tr("Damping resistor thermal status"), assessment.explanation);
    }

    if (harmonicResult) {
        addResult(tr("PCC voltage THD"),
                  harmonicResult->busVoltageThd * 100.0, tr("%"));
        addResult(tr("Source current THD"),
                  harmonicResult->sourceCurrentThd * 100.0, tr("%"));
        addResult(tr("PCC fundamental phase voltage"),
                  harmonicResult->busFundamentalVoltageV, tr("V RMS"));
        addResult(tr("Source fundamental phase current"),
                  harmonicResult->sourceFundamentalCurrentA, tr("A RMS"));
        if (!harmonicResult->branchStresses.empty()) {
            const auto& stress = harmonicResult->branchStresses.front();
            addResult(tr("Branch total RMS current"), stress.totalRmsCurrentA, tr("A"));
            addResult(tr("Capacitor total RMS voltage"), stress.capacitorRmsVoltageV, tr("V"));
            addResult(tr("Reactor total RMS voltage"), stress.inductorRmsVoltageV, tr("V"));
            addResult(tr("Damping resistor total RMS voltage"),
                      stress.dampingResistorRmsVoltageV, tr("V"));
            addResult(tr("Damping resistor harmonic active loss"),
                      stress.dampingResistorLossW, tr("W"));
        }
    }

    std::vector<PlotBand> resonanceBands;
    if (impedanceResult) {
        const auto& points = impedanceResult->points;
        const auto interpolateLogFrequency = [](const p135::filter::FrequencyResponsePoint& a,
                                                  const p135::filter::FrequencyResponsePoint& b,
                                                  double targetMagnitude) {
            const double ya = a.busImpedanceMagnitudeOhm;
            const double yb = b.busImpedanceMagnitudeOhm;
            if (std::abs(yb - ya) <= std::numeric_limits<double>::epsilon())
                return a.frequency.si();
            const double fraction = std::clamp((targetMagnitude - ya) / (yb - ya), 0.0, 1.0);
            return std::exp(std::log(a.frequency.si()) +
                fraction * (std::log(b.frequency.si()) - std::log(a.frequency.si())));
        };
        for (std::size_t peak = 1; peak + 1 < points.size(); ++peak) {
            const double peakMagnitude = points[peak].busImpedanceMagnitudeOhm;
            if (!(peakMagnitude > points[peak - 1].busImpedanceMagnitudeOhm &&
                  peakMagnitude >= points[peak + 1].busImpedanceMagnitudeOhm))
                continue;
            // Parallel-resonance bandwidth: boundaries at -3 dB of the impedance peak.
            const double boundary = peakMagnitude / std::sqrt(2.0);
            std::size_t left = peak;
            while (left > 0 && points[left].busImpedanceMagnitudeOhm >= boundary) --left;
            std::size_t right = peak;
            while (right + 1 < points.size() &&
                   points[right].busImpedanceMagnitudeOhm >= boundary) ++right;
            if (left == peak || right == peak || left + 1 >= points.size() || right >= points.size())
                continue;
            const double startHz = interpolateLogFrequency(points[left], points[left + 1], boundary);
            const double stopHz = interpolateLogFrequency(points[right - 1], points[right], boundary);
            if (std::isfinite(startHz) && std::isfinite(stopHz) && stopHz > startHz) {
                resonanceBands.push_back(PlotBand{
                    startHz, stopHz,
                    tr("Resonance %1 Hz").arg(points[peak].frequency.si(), 0, 'g', 4)
                });
            }
        }
    }
    const std::optional<double> saturationPeakReference =
        inductorSaturationCurrent_ && inductorSaturationCurrent_->value() > 0.0
            ? std::optional<double>{inductorSaturationCurrent_->value()} : std::nullopt;


    QString calculationSummary = tr("LC synthesis, three-phase fundamental MNA verification and ideal-source current sweep completed.");
    if (impedanceResult)
        calculationSummary += tr(" Source/filter impedance scan completed; resonance maxima are candidates, not automatic failures.");
    else
        calculationSummary += tr(" Source/filter impedance scan NOT_VERIFIED.");
    if (harmonicResult)
        calculationSummary += tr(" Harmonic response and component RMS stress were calculated from %1 entered non-fundamental voltage harmonics.")
            .arg(suppliedHarmonics);
    else
        calculationSummary += tr(" Harmonic THD and harmonic component stress NOT_VERIFIED.");
    if (!advancedWarning.isEmpty())
        calculationSummary += QStringLiteral(" ") + advancedWarning;
    if (!componentAssessmentWarning.isEmpty())
        calculationSummary += QStringLiteral(" ") + componentAssessmentWarning;
    if (capacitorThermalResult || resistorThermalResult)
        calculationSummary += tr(" Selected material revisions were resolved from the immutable SQLite catalog and evaluated by the thermal bridge.");
    calculationSummary += tr(" Models assume balanced positive/negative sequence; frequency-dependent ESR is used for the separate capacitor loss estimate but is not yet fed back into the harmonic network solution. Ripple-current correction factors, coupled thermal paths, component tolerances and standards compliance are not evaluated.");
    if (!impedanceResult)
        calculationSummary += tr(" Resonance-frequency zones NOT_VERIFIED: source impedance R/L is required.");
    else if (resonanceBands.empty())
        calculationSummary += tr(" No bounded -3 dB parallel-resonance zone was identified within the scanned frequency range.");
    else
        calculationSummary += tr(" Shaded resonance zones show the -3 dB bandwidth around local bus-impedance peaks.");
    if (!saturationPeakReference)
        calculationSummary += tr(" Inductor saturation comparison NOT_VERIFIED: enter the manufacturer's peak Isat per phase and its evidence reference.");
    else
        calculationSummary += tr(" Red dashed line is the entered Isat reference; calculated phase peak current is sqrt(2) times the AC RMS branch current. This is a rating comparison, not a magnetic saturation model.");

    if (materialStore_) {
        QJsonObject inputSnapshot;
        inputSnapshot.insert(QStringLiteral("nominalLineVoltage_V"), nominalVoltage_->value());
        inputSnapshot.insert(QStringLiteral("fundamentalFrequency_Hz"), fundamentalFrequency_->value());
        inputSnapshot.insert(QStringLiteral("targetReactivePower_var"), targetReactivePower_->value() * 1000.0);
        inputSnapshot.insert(QStringLiteral("tuningHarmonicOrder"), tuningHarmonicOrder_->value());
        inputSnapshot.insert(QStringLiteral("qualityFactor"), qualityFactor_->value());
        inputSnapshot.insert(QStringLiteral("connection"), filterConnection_->currentText());
        inputSnapshot.insert(QStringLiteral("sourceResistance_ohm"), sourceResistance);
        inputSnapshot.insert(QStringLiteral("sourceInductance_H"), sourceInductance);
        inputSnapshot.insert(QStringLiteral("inductorSaturationCurrent_A_peak"),
            inductorSaturationCurrent_ ? inductorSaturationCurrent_->value() : 0.0);
        inputSnapshot.insert(QStringLiteral("inductorSaturationSource"),
            inductorSaturationSource_ ? inductorSaturationSource_->text() : QString{});
        inputSnapshot.insert(QStringLiteral("ambientTemperature_C"),
            ambientTemperature_ ? ambientTemperature_->value() : 25.0);
        inputSnapshot.insert(QStringLiteral("capacitorThermalResistance_K_per_W"),
            capacitorThermalResistance_ ? capacitorThermalResistance_->value() : 0.0);
        inputSnapshot.insert(QStringLiteral("capacitorThermalSource"),
            capacitorThermalSource_ ? capacitorThermalSource_->text() : QString{});
        inputSnapshot.insert(QStringLiteral("resistorThermalResistance_K_per_W"),
            resistorThermalResistance_ ? resistorThermalResistance_->value() : 0.0);
        inputSnapshot.insert(QStringLiteral("resistorThermalSource"),
            resistorThermalSource_ ? resistorThermalSource_->text() : QString{});
        inputSnapshot.insert(QStringLiteral("capacitorRevisionId"),
            capacitorComponent_ ? capacitorComponent_->currentData().toString() : QString{});
        inputSnapshot.insert(QStringLiteral("resistorRevisionId"),
            resistorComponent_ ? resistorComponent_->currentData().toString() : QString{});
        QJsonArray spectrumSnapshot;
        if (harmonicSpectrumTable_) {
            for (int row = 0; row < harmonicSpectrumTable_->rowCount(); ++row) {
                QJsonObject point;
                point.insert(QStringLiteral("order"), harmonicSpectrumTable_->item(row, 0)
                    ? static_cast<double>(harmonicSpectrumTable_->item(row, 0)->text().toUInt()) : 0.0);
                point.insert(QStringLiteral("sequence"), harmonicSpectrumTable_->item(row, 1)
                    ? harmonicSpectrumTable_->item(row, 1)->text() : QString{});
                point.insert(QStringLiteral("voltageRms_V"), harmonicSpectrumTable_->item(row, 2)
                    ? harmonicSpectrumTable_->item(row, 2)->text().toDouble() : 0.0);
                point.insert(QStringLiteral("phase_deg"), harmonicSpectrumTable_->item(row, 3)
                    ? harmonicSpectrumTable_->item(row, 3)->text().toDouble() : 0.0);
                spectrumSnapshot.append(point);
            }
        }
        inputSnapshot.insert(QStringLiteral("harmonicSpectrum"), spectrumSnapshot);

        QJsonArray outputSnapshot;
        for (int row = 0; row < resultsTable_->rowCount(); ++row) {
            QJsonObject item;
            item.insert(QStringLiteral("name"), resultsTable_->item(row, 0)
                ? resultsTable_->item(row, 0)->text() : QString{});
            item.insert(QStringLiteral("value"), resultsTable_->item(row, 1)
                ? resultsTable_->item(row, 1)->text() : QString{});
            item.insert(QStringLiteral("unit"), resultsTable_->item(row, 2)
                ? resultsTable_->item(row, 2)->text() : QString{});
            outputSnapshot.append(item);
        }
        QJsonArray frequencySweepSnapshot;
        PlotCurve bankCurrentCurve{tr("Current calculation"), {}};
        for (const auto& point : sweepResult.value().points) {
            double maximumPhaseRmsCurrent = 0.0;
            for (const std::size_t index : {3U, 6U, 9U}) {
                if (index < point.result.branches.size())
                    maximumPhaseRmsCurrent = std::max(maximumPhaseRmsCurrent,
                        std::abs(point.result.branches[index].rmsCurrent));
            }
            const double phasePeakCurrent = std::sqrt(2.0) * maximumPhaseRmsCurrent;
            const double frequencyHz = point.frequency.si();
            QJsonObject sample;
            sample.insert(QStringLiteral("frequency_Hz"), frequencyHz);
            sample.insert(QStringLiteral("phasePeakCurrent_A"), phasePeakCurrent);
            frequencySweepSnapshot.append(sample);
            bankCurrentCurve.points.emplace_back(frequencyHz, phasePeakCurrent);
        }

        QJsonArray impedanceScanSnapshot;
        PlotCurve busImpedanceCurve{tr("Current calculation"), {}};
        if (impedanceResult) {
            for (const auto& point : impedanceResult->points) {
                const double frequencyHz = point.frequency.si();
                QJsonObject sample;
                sample.insert(QStringLiteral("frequency_Hz"), frequencyHz);
                sample.insert(QStringLiteral("busImpedance_Ohm"), point.busImpedanceMagnitudeOhm);
                sample.insert(QStringLiteral("filterImpedance_Ohm"), point.filterImpedanceMagnitudeOhm);
                sample.insert(QStringLiteral("sourceImpedance_Ohm"), point.sourceImpedanceMagnitudeOhm);
                impedanceScanSnapshot.append(sample);
                busImpedanceCurve.points.emplace_back(frequencyHz, point.busImpedanceMagnitudeOhm);
            }
        }

        if (resultsBankCurrentPlot_) {
            resultsBankCurrentPlot_->setCurves({bankCurrentCurve},
                tr("Frequency (Hz, logarithmic)"), tr("Phase peak current (A)"));
            resultsBankCurrentPlot_->setEngineeringGuides(resonanceBands, saturationPeakReference);
        }
        if (resultsBusImpedancePlot_) {
            resultsBusImpedancePlot_->setCurves(impedanceResult
                ? std::vector<PlotCurve>{busImpedanceCurve} : std::vector<PlotCurve>{},
                tr("Frequency (Hz, logarithmic)"), tr("Bus impedance (Ω)"));
            resultsBusImpedancePlot_->setEngineeringGuides(resonanceBands);
        }

        QJsonObject resultSnapshot;
        resultSnapshot.insert(QStringLiteral("results"), outputSnapshot);
        resultSnapshot.insert(QStringLiteral("calculationSummary"), calculationSummary);
        resultSnapshot.insert(QStringLiteral("frequencySweep"), frequencySweepSnapshot);
        resultSnapshot.insert(QStringLiteral("impedanceScan"), impedanceScanSnapshot);

        QJsonObject engineVersions;
        engineVersions.insert(QStringLiteral("application"), QStringLiteral("Project135-0.1.0"));
        engineVersions.insert(QStringLiteral("filterSynthesis"), QStringLiteral("filter-synthesis-v1"));
        engineVersions.insert(QStringLiteral("circuitSolver"), QStringLiteral("circuit-ac-mna-v1"));
        engineVersions.insert(QStringLiteral("harmonics"), QStringLiteral("harmonics-v1"));
        engineVersions.insert(QStringLiteral("thermal"), QStringLiteral("thermal-steady-v1"));
        engineVersions.insert(QStringLiteral("frequencyPlotSchema"), QStringLiteral("frequency-response-v2-resonance-isat-v1"));

        const std::string inputJson = QJsonDocument(inputSnapshot)
            .toJson(QJsonDocument::Compact).toStdString();
        const std::string resultJson = QJsonDocument(resultSnapshot)
            .toJson(QJsonDocument::Compact).toStdString();
        const std::string versionsJson = QJsonDocument(engineVersions)
            .toJson(QJsonDocument::Compact).toStdString();
        const std::string hashEnvelope = "{\\"engineVersions\\":" + versionsJson +
            ",\\"input\\":" + inputJson + ",\\"result\\":" + resultJson + "}";
        const auto canonicalEnvelope = p135::infrastructure::sqlite::canonicalizeJson(hashEnvelope);
        const auto snapshotHash = canonicalEnvelope
            ? p135::infrastructure::sqlite::sha256Hex(canonicalEnvelope.value())
            : p135::Result<std::string>::failure(canonicalEnvelope.error());

        if (snapshotHash) {
            const QString calculationId = QUuid::createUuid().toString(QUuid::WithoutBraces);
            const QString projectName = projectName_ && !projectName_->text().trimmed().isEmpty()
                ? projectName_->text().trimmed() : tr("Untitled");
            p135::infrastructure::sqlite::CalculationRecord snapshot{
                calculationId.toStdString(),
                projectId_.toStdString(),
                projectName.toStdString(),
                productFamily_ ? productFamily_->currentText().toStdString() : "unknown",
                inputJson,
                resultJson,
                versionsJson,
                snapshotHash.value(),
                "desktop-user",
                QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs).toStdString()
            };
            std::vector<p135::infrastructure::sqlite::CalculationMaterialReference> materialReferences;
            const auto addMaterialReference = [&materialReferences](QComboBox* selector) {
                if (!selector) return;
                const QString revisionId = selector->currentData().toString();
                if (revisionId.isEmpty()) return;
                materialReferences.push_back({
                    QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString(),
                    revisionId.toStdString()
                });
            };
            addMaterialReference(capacitorComponent_);
            addMaterialReference(resistorComponent_);
            const auto saved = materialStore_->recordCalculation(snapshot, materialReferences);
            if (saved) {
                calculationSummary += tr(" Immutable calculation snapshot saved: %1.")
                    .arg(calculationId);
                appendLog(tr("Calculation snapshot %1 saved with %2 exact material revision links.")
                    .arg(calculationId).arg(materialReferences.size()));
            } else {
                calculationSummary += tr(" Calculation snapshot NOT_SAVED: %1.")
                    .arg(QString::fromStdString(saved.error().message));
                appendLog(tr("Calculation snapshot save failed: %1")
                    .arg(QString::fromStdString(saved.error().message)));
            }
        } else {
            calculationSummary += tr(" Calculation snapshot NOT_SAVED: canonical JSON/hash generation failed.");
        }
    } else {
        calculationSummary += tr(" Calculation snapshot NOT_SAVED: material database is unavailable.");
    }

    // Plotting is independent of persistence: the live result remains visible even
    // when the material database is unavailable and its snapshot cannot be saved.
    PlotCurve liveCurrentCurve{tr("Current calculation"), {}};
    for (const auto& point : sweepResult.value().points) {
        double maximumPhaseRmsCurrent = 0.0;
        for (const std::size_t index : {3U, 6U, 9U}) {
            if (index < point.result.branches.size())
                maximumPhaseRmsCurrent = std::max(maximumPhaseRmsCurrent,
                    std::abs(point.result.branches[index].rmsCurrent));
        }
        liveCurrentCurve.points.emplace_back(point.frequency.si(),
            std::sqrt(2.0) * maximumPhaseRmsCurrent);
    }
    if (resultsBankCurrentPlot_) {
        resultsBankCurrentPlot_->setCurves({liveCurrentCurve},
            tr("Frequency (Hz, logarithmic)"), tr("Phase peak current (A)"));
        resultsBankCurrentPlot_->setEngineeringGuides(resonanceBands, saturationPeakReference);
    }
    PlotCurve liveBusImpedanceCurve{tr("Current calculation"), {}};
    if (impedanceResult) {
        for (const auto& point : impedanceResult->points)
            liveBusImpedanceCurve.points.emplace_back(
                point.frequency.si(), point.busImpedanceMagnitudeOhm);
    }
    if (resultsBusImpedancePlot_) {
        resultsBusImpedancePlot_->setCurves(impedanceResult
            ? std::vector<PlotCurve>{liveBusImpedanceCurve} : std::vector<PlotCurve>{},
            tr("Frequency (Hz, logarithmic)"), tr("Bus impedance (Ω)"));
        resultsBusImpedancePlot_->setEngineeringGuides(resonanceBands);
    }

    resultsNote_->setText(calculationSummary);
    status_->setText(tr("Filter synthesized and AC network solved"));
    appendLog(tr("Single-tuned branch synthesized: %1 kvar, h=%2, connection=%3.")
        .arg(targetReactivePower_->value())
        .arg(tuningHarmonicOrder_->value())
        .arg(filterConnection_->currentText()));
}


void MainWindow::refreshComponentSelectors()
{
    if (!materialStore_) return;
    const auto populate = [this](QComboBox* combo, const std::string& recordType,
                                 const QString& emptyLabel) {
        if (!combo) return;
        const QString previous = combo->currentData().toString();
        combo->clear();
        combo->addItem(emptyLabel, QString{});
        p135::infrastructure::sqlite::MaterialCatalogFilter filter;
        filter.recordType = recordType;
        filter.pageSize = 500;
        const auto revisions = materialStore_->searchLatestRevisions(filter);
        if (!revisions) {
            appendLog(tr("Cannot load %1 component revisions: %2")
                .arg(emptyLabel, QString::fromStdString(revisions.error().message)));
            return;
        }
        for (const auto& revision : revisions.value()) {
            combo->addItem(QString::fromStdString(revision.materialId) +
                QStringLiteral(" — r%1").arg(revision.revisionNumber),
                QString::fromStdString(revision.revisionId));
        }
        const int previousIndex = combo->findData(previous);
        if (previousIndex >= 0) combo->setCurrentIndex(previousIndex);
    };
    populate(capacitorComponent_, "capacitor", tr("— no capacitor selected —"));
    populate(resistorComponent_, "resistor", tr("— no resistor selected —"));
}

void MainWindow::updateProjectTitle()
{
    const QString name = projectName_ ? projectName_->text().trimmed() : QString{};
    setWindowTitle(QString("Project135 — %1").arg(name.isEmpty() ? tr("Untitled") : name));
    QSettings settings;
    settings.setValue(QStringLiteral("project/currentId"), projectId_);
    if (projectName_) settings.setValue(QStringLiteral("project/name"), name);
}

void MainWindow::appendLog(const QString& message)
{
    if (log_)
        log_->appendPlainText(message);
}
