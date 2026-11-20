#include "main_window.hpp"

#include <nlohmann/json.hpp>
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
#include "p135/domain/magnetic_circuit.hpp"
#include "p135/domain/three_limb_magnetic.hpp"
#include "p135/thermal/filter_bridge.hpp"

#include <QAbstractItemView>
#include <QItemSelectionModel>
#include <QAction>
#include <QComboBox>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QDir>
#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QSaveFile>
#include <QDate>
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
#include <QRegularExpression>
#include <QScrollArea>
#include <QSpinBox>
#include <QStatusBar>
#include <QStringList>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>
#include <QUuid>

#include <array>
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
                              std::optional<double> saturationPeakCurrentA = std::nullopt,
                              std::optional<double> magneticKneeCurrentA = std::nullopt)
    {
        resonanceBands_ = std::move(resonanceBands);
        saturationPeakCurrentA_ = saturationPeakCurrentA;
        magneticKneeCurrentA_ = magneticKneeCurrentA;
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
        const double yMax = std::max({maxY * 1.08,
            saturationPeakCurrentA_.value_or(0.0) * 1.08,
            magneticKneeCurrentA_.value_or(0.0) * 1.08});
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

        if (magneticKneeCurrentA_ && std::isfinite(*magneticKneeCurrentA_) &&
            *magneticKneeCurrentA_ > 0.0) {
            const double y = area.bottom() - (*magneticKneeCurrentA_ / yMax) * area.height();
            painter.setPen(QPen(QColor(125, 65, 165), 2.0, Qt::DashDotLine));
            painter.drawLine(QPointF(area.left(), y), QPointF(area.right(), y));
            painter.setPen(QColor(115, 55, 155));
            painter.drawText(QRectF(area.left() + 5, y - 19, area.width() - 10, 17),
                Qt::AlignRight | Qt::AlignVCenter,
                tr("B-H knee criterion: %1 A peak").arg(*magneticKneeCurrentA_, 0, 'g', 5));
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
    std::optional<double> magneticKneeCurrentA_;
};


class ProductTopologyWidget final : public QWidget
{
public:
    explicit ProductTopologyWidget(QWidget* parent = nullptr) : QWidget(parent)
    {
        setMinimumHeight(142);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    void setProductFamily(const QString& family)
    {
        family_ = family;
        update();
    }

    void setConnection(const QString& connection)
    {
        connection_ = connection;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.fillRect(rect(), palette().alternateBase());

        const int margin = 22;
        const int y = 67;
        const int h = 42;
        const int available = std::max(1, width() - 2 * margin);
        const QStringList labels = family_.contains("Shunt", Qt::CaseInsensitive)
            ? QStringList{tr("3-phase bus"), tr("Fuse / CB"), tr("Contactor"), tr("Reactor L"), tr("Capacitor C"), connection_}
            : family_.contains("VFD", Qt::CaseInsensitive)
                ? QStringList{tr("VFD output"), tr("Protection"), tr("Contactor"), tr("Reactor L"), tr("Motor")}
                : family_.contains("transformer", Qt::CaseInsensitive)
                    ? QStringList{tr("Supply"), tr("Protection"), tr("Primary"), tr("Transformer"), tr("Secondary")}
                    : family_.contains("AFE", Qt::CaseInsensitive)
                        ? QStringList{tr("Grid"), tr("Protection"), tr("Reactor L"), tr("AFE converter"), tr("DC link")}
                        : QStringList{tr("Electrical source"), tr("Protection"), tr("Reactor"), tr("Terminals")};

        const int count = labels.size();
        const int gap = count > 1 ? 12 : 0;
        const int boxWidth = std::max(66, (available - gap * (count - 1)) / count);
        const int totalWidth = boxWidth * count + gap * (count - 1);
        const int startX = std::max(margin, (width() - totalWidth) / 2);

        p.setPen(palette().color(QPalette::Text));
        p.drawText(QRect(12, 8, width() - 24, 22), Qt::AlignCenter,
            tr("Functional single-line preview — not a wiring diagram"));

        for (int i = 0; i < count; ++i) {
            const int x = startX + i * (boxWidth + gap);
            if (i > 0) {
                const int left = x - gap;
                p.setPen(QPen(palette().color(QPalette::Text), 1.5));
                p.drawLine(left + 1, y + h / 2, x - 4, y + h / 2);
                p.drawLine(x - 9, y + h / 2 - 4, x - 4, y + h / 2);
                p.drawLine(x - 9, y + h / 2 + 4, x - 4, y + h / 2);
            }
            const QRect box(x, y, boxWidth, h);
            p.setPen(QPen(palette().color(QPalette::Mid), 1.2));
            p.setBrush(palette().base());
            p.drawRoundedRect(box, 5, 5);
            p.setPen(palette().color(QPalette::Text));
            p.drawText(box.adjusted(4, 3, -4, -3), Qt::AlignCenter | Qt::TextWordWrap, labels[i]);
        }

        QString detail;
        if (family_.contains("Shunt", Qt::CaseInsensitive))
            detail = tr("Three-phase shunt branch • series-tuned R–L–C • termination: %1")
                .arg(connection_);
        else
            detail = tr("Conceptual topology only • calculation workflow not yet implemented for this product family");
        p.setPen(palette().color(QPalette::Mid));
        p.drawText(QRect(12, 119, width() - 24, 18), Qt::AlignCenter, detail);
    }

private:
    QString family_;
    QString connection_{tr("Wye (Y) / Delta (Δ)")};
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
    const double esl = numberOr("esl_H", number("equivalentSeriesInductance_H"));
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
        QSettings settings;
        settings.setValue(QStringLiteral("project/currentId"), projectId_);
        loadProjectSettings();
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
        loadProjectSettings();
        updateProjectTitle();
        tabs_->setCurrentIndex(0);
    });
    // Page navigation belongs to the tab bar; keep only workflow actions here.
    toolbar->addSeparator();
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
    if (auto* topologyContainer = findChild<QWidget*>(QStringLiteral("productTopologyDiagram"))) {
        if (auto* topology = dynamic_cast<ProductTopologyWidget*>(topologyContainer)) {
            topology->setConnection(filterConnection_->currentText());
            connect(filterConnection_, &QComboBox::currentTextChanged, topology,
                [topology](const QString& value) { topology->setConnection(value); });
        }
    }
    setCentralWidget(tabs_);
    connect(tabs_, &QTabWidget::currentChanged, this, [this](int index) {
        if (index == 5) refreshCalculationHistory();
    });

    loadProjectSettings();

    status_ = new QLabel(tr("Ready — filter synthesis available"));
    statusBar()->addPermanentWidget(status_);
    appendLog(tr("Project135 desktop started. Core and filter synthesis remain independent from Qt."));
    if (!databaseError_.isEmpty()) {
        status_->setText(tr("Material database unavailable"));
        status_->setToolTip(databaseError_);
        status_->setStyleSheet("color: #b36b00;");
        appendLog(tr("Material database initialization failed: %1").arg(databaseError_));
    } else {
        status_->setToolTip(tr("SQLite material catalog initialized successfully."));
        appendLog(tr("SQLite material database initialized."));
    }
    updateProjectTitle();
}


void MainWindow::saveProjectSettings()
{
    if (loadingProjectSettings_ || projectId_.isEmpty() || !projectName_ || !productFamily_) return;
    QSettings settings;
    settings.beginGroup(QStringLiteral("projects/") + projectId_);
    settings.setValue(QStringLiteral("name"), projectName_->text());
    settings.setValue(QStringLiteral("productFamily"), productFamily_->currentText());
    const auto saveLine = [this, &settings](const char* objectName, const char* key) {
        if (const auto* widget = findChild<QLineEdit*>(QString::fromLatin1(objectName)))
            settings.setValue(QString::fromLatin1(key), widget->text());
    };
    saveLine("projectCustomer", "customer");
    saveLine("projectNumber", "projectNumber");
    saveLine("projectEngineer", "engineer");
    saveLine("projectDate", "date");
    saveLine("projectRevision", "revision");
    if (const auto* w = findChild<QDoubleSpinBox*>(QStringLiteral("projectAmbientC"))) settings.setValue("ambientC", w->value());
    if (const auto* w = findChild<QSpinBox*>(QStringLiteral("projectAltitudeM"))) settings.setValue("altitudeM", w->value());
    if (const auto* w = findChild<QComboBox*>(QStringLiteral("projectInsulationClass"))) settings.setValue("insulationClass", w->currentText());
    if (const auto* w = findChild<QComboBox*>(QStringLiteral("projectIpRating"))) settings.setValue("ipRating", w->currentText());
    if (const auto* w = findChild<QComboBox*>(QStringLiteral("projectCoolingMethod"))) settings.setValue("coolingMethod", w->currentData().toString());
    if (const auto* w = findChild<QDoubleSpinBox*>(QStringLiteral("projectAirflowM3h"))) settings.setValue("airflowM3h", w->value());
    settings.endGroup();
    settings.setValue(QStringLiteral("project/currentId"), projectId_);
}

void MainWindow::loadProjectSettings()
{
    if (projectId_.isEmpty()) return;
    loadingProjectSettings_ = true;
    QSettings settings;
    settings.beginGroup(QStringLiteral("projects/") + projectId_);
    const auto get = [&settings](const char* key, const QVariant& fallback = QVariant{}) {
        return settings.value(QString::fromLatin1(key), fallback);
    };
    if (projectName_) projectName_->setText(get("name", tr("New engineering project")).toString());
    if (productFamily_) {
        const int i = productFamily_->findText(get("productFamily", productFamily_->itemText(0)).toString());
        productFamily_->setCurrentIndex(i < 0 ? 0 : i);
    }
    const auto setLine = [this, &get](const char* objectName, const char* key, const QVariant& fallback = QVariant{}) {
        if (auto* w = findChild<QLineEdit*>(QString::fromLatin1(objectName))) w->setText(get(key, fallback).toString());
    };
    setLine("projectCustomer", "customer");
    setLine("projectNumber", "projectNumber");
    setLine("projectEngineer", "engineer");
    setLine("projectDate", "date", QDate::currentDate().toString(Qt::ISODate));
    setLine("projectRevision", "revision", QStringLiteral("A"));
    if (auto* w = findChild<QDoubleSpinBox*>(QStringLiteral("projectAmbientC"))) w->setValue(get("ambientC", 40.0).toDouble());
    if (auto* w = findChild<QSpinBox*>(QStringLiteral("projectAltitudeM"))) w->setValue(get("altitudeM", 0).toInt());
    if (auto* w = findChild<QComboBox*>(QStringLiteral("projectInsulationClass"))) {
        const int i = w->findText(get("insulationClass", w->itemText(0)).toString()); w->setCurrentIndex(i < 0 ? 0 : i);
    }
    if (auto* w = findChild<QComboBox*>(QStringLiteral("projectIpRating"))) {
        const int i = w->findText(get("ipRating", w->itemText(0)).toString()); w->setCurrentIndex(i < 0 ? 0 : i);
    }
    if (auto* w = findChild<QComboBox*>(QStringLiteral("projectCoolingMethod"))) {
        const int i = w->findData(get("coolingMethod", QStringLiteral("AN"))); w->setCurrentIndex(i < 0 ? 0 : i);
    }
    if (auto* w = findChild<QDoubleSpinBox*>(QStringLiteral("projectAirflowM3h"))) w->setValue(get("airflowM3h", 0.0).toDouble());
    settings.endGroup();
    if (auto* flow = findChild<QDoubleSpinBox*>(QStringLiteral("projectAirflowM3h"))) {
        const auto* method = findChild<QComboBox*>(QStringLiteral("projectCoolingMethod"));
        flow->setEnabled(method && method->currentData().toString() == QStringLiteral("AF"));
    }
    loadingProjectSettings_ = false;
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
    projectName_->setObjectName(QStringLiteral("projectName"));
    projectName_->setPlaceholderText(tr("Project name"));
    productFamily_ = new QComboBox;
    productFamily_->setObjectName(QStringLiteral("projectProductFamily"));
    productFamily_->addItems({tr("Shunt passive harmonic filter"), tr("VFD output filter"),
        tr("Reactor / inductor"), tr("Power transformer"), tr("AFE reactor")});
    auto* customer = new QLineEdit;
    customer->setObjectName(QStringLiteral("projectCustomer"));
    auto* projectNumber = new QLineEdit;
    projectNumber->setObjectName(QStringLiteral("projectNumber"));
    auto* engineer = new QLineEdit;
    engineer->setObjectName(QStringLiteral("projectEngineer"));
    auto* projectDate = new QLineEdit(QDate::currentDate().toString(Qt::ISODate));
    projectDate->setObjectName(QStringLiteral("projectDate"));
    auto* revision = new QLineEdit("A");
    revision->setObjectName(QStringLiteral("projectRevision"));
    form->addRow(tr("Project name"), projectName_);
    form->addRow(tr("Product family"), productFamily_);
    form->addRow(tr("Customer"), customer);
    form->addRow(tr("Project number"), projectNumber);
    form->addRow(tr("Design engineer"), engineer);
    form->addRow(tr("Project date (YYYY-MM-DD)"), projectDate);
    form->addRow(tr("Revision"), revision);
    layout->addWidget(group);

    auto* environment = new QGroupBox(tr("Installation and environmental conditions"));
    auto* environmentForm = new QFormLayout(environment);
    auto* ambient = new QDoubleSpinBox;
    ambient->setObjectName(QStringLiteral("projectAmbientC"));
    ambient->setRange(-50.0, 80.0);
    ambient->setDecimals(1);
    ambient->setValue(40.0);
    ambient->setSuffix(tr(" °C"));
    auto* altitude = new QSpinBox;
    altitude->setObjectName(QStringLiteral("projectAltitudeM"));
    altitude->setRange(0, 6000);
    altitude->setSuffix(tr(" m above sea level"));
    auto* insulationClass = new QComboBox;
    insulationClass->setObjectName(QStringLiteral("projectInsulationClass"));
    insulationClass->addItems({tr("F (155 °C)"), tr("H (180 °C)"), tr("Not specified")});
    auto* ipRating = new QComboBox;
    ipRating->setObjectName(QStringLiteral("projectIpRating"));
    ipRating->addItems({"IP00", "IP20", "IP21", "IP23", "IP31", "IP41", "IP54", "IP55", tr("Other / not specified")});
    auto* coolingMethod = new QComboBox;
    coolingMethod->setObjectName(QStringLiteral("projectCoolingMethod"));
    coolingMethod->addItem(tr("AN — natural air cooling"), "AN");
    coolingMethod->addItem(tr("AF — forced air cooling"), "AF");
    auto* airflow = new QDoubleSpinBox;
    airflow->setObjectName(QStringLiteral("projectAirflowM3h"));
    airflow->setRange(0.0, 100000.0);
    airflow->setDecimals(2);
    airflow->setSuffix(tr(" m³/h"));
    airflow->setSpecialValueText(tr("NOT SET"));
    airflow->setEnabled(false);
    airflow->setToolTip(tr("Enter the verified total airflow through the equipment enclosure, not the fan free-air rating."));
    auto* coolingStatus = new QLabel(tr("Project assumptions are saved locally per project. They are not yet part of a portable project file and do not alter the thermal solver."));
    coolingStatus->setWordWrap(true);
    coolingStatus->setStyleSheet("color: #a66b00;");
    environmentForm->addRow(tr("Maximum ambient temperature"), ambient);
    environmentForm->addRow(tr("Installation altitude"), altitude);
    environmentForm->addRow(tr("Insulation thermal class"), insulationClass);
    environmentForm->addRow(tr("Enclosure protection"), ipRating);
    environmentForm->addRow(tr("Cooling method"), coolingMethod);
    environmentForm->addRow(tr("Forced-air volume flow"), airflow);
    environmentForm->addRow(coolingStatus);
    connect(coolingMethod, &QComboBox::currentIndexChanged, this, [airflow](int index) {
        airflow->setEnabled(index == 1);
        if (index == 0) airflow->setValue(0.0);
    });
    layout->addWidget(environment);

    auto* topology = new QGroupBox(tr("Selected product topology"));
    auto* topologyLayout = new QVBoxLayout(topology);
    auto* topologyDiagram = new ProductTopologyWidget;
    topologyDiagram->setObjectName(QStringLiteral("productTopologyDiagram"));
    topologyDiagram->setProductFamily(productFamily_->currentText());
    topologyDiagram->setMinimumWidth(720);
    topologyLayout->addWidget(topologyDiagram);
    layout->addWidget(topology);

    auto* note = new QLabel(tr("The first supported calculation workflow is a three-phase shunt filter with single-tuned series RLC branches. Environmental values are project assumptions only; verify them against the installation specification before thermal or insulation design. Other product families remain planned and are not calculated yet."));
    note->setWordWrap(true);
    note->setStyleSheet("color: palette(mid);");
    layout->addWidget(note);
    layout->addStretch();
    connect(projectName_, &QLineEdit::textChanged, this, [this] { updateProjectTitle(); saveProjectSettings(); });
    connect(customer, &QLineEdit::textChanged, this, [this] { saveProjectSettings(); });
    connect(projectNumber, &QLineEdit::textChanged, this, [this] { saveProjectSettings(); });
    connect(engineer, &QLineEdit::textChanged, this, [this] { saveProjectSettings(); });
    connect(projectDate, &QLineEdit::textChanged, this, [this] { saveProjectSettings(); });
    connect(revision, &QLineEdit::textChanged, this, [this] { saveProjectSettings(); });
    connect(ambient, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this] { saveProjectSettings(); });
    connect(altitude, qOverload<int>(&QSpinBox::valueChanged), this, [this] { saveProjectSettings(); });
    connect(insulationClass, &QComboBox::currentIndexChanged, this, [this] { saveProjectSettings(); });
    connect(ipRating, &QComboBox::currentIndexChanged, this, [this] { saveProjectSettings(); });
    connect(coolingMethod, &QComboBox::currentIndexChanged, this, [this, airflow](int index) {
        airflow->setEnabled(index == 1);
        if (index == 0) airflow->setValue(0.0);
        saveProjectSettings();
    });
    connect(airflow, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this] { saveProjectSettings(); });
    connect(productFamily_, &QComboBox::currentTextChanged, this, [this, topologyDiagram](const QString& value) {
        topologyDiagram->setProductFamily(value);
        saveProjectSettings();
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

    tuningHarmonicOrder_ = new QDoubleSpinBox;
    tuningHarmonicOrder_->setRange(2.0, 100.0);
    tuningHarmonicOrder_->setDecimals(3);
    tuningHarmonicOrder_->setSingleStep(0.05);
    tuningHarmonicOrder_->setValue(5.0);
    tuningHarmonicOrder_->setToolTip(tr("Fractional tuning order is supported, e.g. 4.75 for a filter tuned below the 5th harmonic."));

    qualityFactor_ = new QDoubleSpinBox;
    qualityFactor_->setRange(0.1, 10000.0);
    qualityFactor_->setValue(50.0);
    qualityFactor_->setDecimals(2);
    qualityFactor_->setToolTip(tr(
        "Series-branch quality factor Qf = ωt·L/R, evaluated at the tuning angular frequency. "
        "It determines the series damping resistance; it is not the reactor's standalone Q."));

    form->addRow(tr("Nominal line voltage"), nominalVoltage_);
    form->addRow(tr("Fundamental frequency"), fundamentalFrequency_);
    form->addRow(tr("Branch connection"), filterConnection_);
    form->addRow(tr("Target reactive power"), targetReactivePower_);
    form->addRow(tr("Tuning harmonic order"), tuningHarmonicOrder_);
    form->addRow(tr("Series-branch quality factor Qf"), qualityFactor_);
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
    capacitorThermalResistance_->setSpecialValueText(tr("NOT SET"));
    capacitorThermalResistance_->setToolTip(tr(
        "Zero means no verified thermal resistance has been supplied. It must not be interpreted as ideal cooling."));
    capacitorThermalSource_ = new QLineEdit;
    capacitorThermalSource_->setPlaceholderText(tr("Rθ source: assembly datasheet or measurement"));
    resistorThermalResistance_ = new QDoubleSpinBox;
    resistorThermalResistance_->setRange(0.0, 1000000.0);
    resistorThermalResistance_->setDecimals(4);
    resistorThermalResistance_->setSuffix(tr(" K/W"));
    resistorThermalResistance_->setSpecialValueText(tr("NOT SET"));
    resistorThermalResistance_->setToolTip(tr(
        "Zero means no verified thermal resistance has been supplied. It must not be interpreted as ideal cooling."));
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

    auto* note = new QLabel(tr(
        "Target reactive power is the total three-phase fundamental-frequency value for this branch. "
        "Synthesis uses ideal C and L and derives series damping resistance from Qf = ωt·L/R. "
        "Source impedance and the positive-/negative-sequence voltage-source spectrum enable the "
        "balanced impedance and harmonic estimates described below. These inputs do not represent "
        "harmonic load-current injections. Component ratings, thermal derating and standards compliance "
        "remain separate assessments."));
    note->setWordWrap(true);
    note->setStyleSheet("color: #a66b00;");
    layout->addWidget(note);

    auto* advanced = new QGroupBox(tr("Source impedance, harmonics and magnetic core"));
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

    coreEffectiveAreaMm2_ = new QDoubleSpinBox;
    coreEffectiveAreaMm2_->setRange(0.0, 10000000.0);
    coreEffectiveAreaMm2_->setDecimals(3);
    coreEffectiveAreaMm2_->setSuffix(tr(" mm²"));
    coreMeanPathMm_ = new QDoubleSpinBox;
    coreMeanPathMm_->setRange(0.0, 10000000.0);
    coreMeanPathMm_->setDecimals(2);
    coreMeanPathMm_->setSuffix(tr(" mm"));
    coreAirGapMm_ = new QDoubleSpinBox;
    coreAirGapMm_->setRange(0.0, 1000000.0);
    coreAirGapMm_->setDecimals(3);
    coreAirGapMm_->setSuffix(tr(" mm"));
    coreGapEffectiveAreaMm2_ = new QDoubleSpinBox;
    coreGapEffectiveAreaMm2_->setRange(0.0, 10000000.0);
    coreGapEffectiveAreaMm2_->setDecimals(3);
    coreGapEffectiveAreaMm2_->setSuffix(tr(" mm²"));
    coreGapEffectiveAreaMm2_->setSpecialValueText(tr("Not set — no fringing correction"));
    coreYokeEffectiveAreaMm2_ = new QDoubleSpinBox;
    coreYokeEffectiveAreaMm2_->setRange(0.0, 10000000.0);
    coreYokeEffectiveAreaMm2_->setDecimals(3);
    coreYokeEffectiveAreaMm2_->setSuffix(tr(" mm²"));
    coreYokeSegmentPathMm_ = new QDoubleSpinBox;
    coreYokeSegmentPathMm_->setRange(0.0, 10000000.0);
    coreYokeSegmentPathMm_->setDecimals(2);
    coreYokeSegmentPathMm_->setSuffix(tr(" mm"));
    inductorTurns_ = new QSpinBox;
    inductorTurns_->setRange(0, 1000000);
    coreBhSource_ = new QLineEdit;
    coreBhSource_->setPlaceholderText(tr("Manufacturer datasheet / measured B-H report and revision"));
    sourceForm->addRow(tr("Core effective area Ae"), coreEffectiveAreaMm2_);
    sourceForm->addRow(tr("Mean magnetic path le"), coreMeanPathMm_);
    sourceForm->addRow(tr("Total air gap per limb"), coreAirGapMm_);
    sourceForm->addRow(tr("Effective gap area Ag,eff (optional)"), coreGapEffectiveAreaMm2_);
    sourceForm->addRow(tr("Yoke effective area Ay"), coreYokeEffectiveAreaMm2_);
    sourceForm->addRow(tr("Yoke segment path ly"), coreYokeSegmentPathMm_);
    sourceForm->addRow(tr("Inductor turns per phase N"), inductorTurns_);
    sourceForm->addRow(tr("B-H curve source"), coreBhSource_);
    advancedLayout->addLayout(sourceForm);

    auto* branchGeometryGroup = new QGroupBox(tr("Independent three-limb geometry overrides"));
    auto* branchGeometryForm = new QFormLayout(branchGeometryGroup);
    const auto addGeometryOverride = [branchGeometryForm](
        const QString& label, QCheckBox*& enabled, QDoubleSpinBox*& value,
        const QString& suffix, int decimals, double maximum) {
        auto* row = new QWidget;
        auto* layout = new QHBoxLayout(row);
        layout->setContentsMargins(0, 0, 0, 0);
        enabled = new QCheckBox(QObject::tr("Override"));
        value = new QDoubleSpinBox;
        value->setRange(0.0, maximum);
        value->setDecimals(decimals);
        value->setSuffix(suffix);
        value->setEnabled(false);
        QObject::connect(enabled, &QCheckBox::toggled, value, &QDoubleSpinBox::setEnabled);
        layout->addWidget(enabled);
        layout->addWidget(value, 1);
        branchGeometryForm->addRow(label, row);
    };
    const std::array<QString, 3> limbNames{
        tr("Limb A"), tr("Limb B"), tr("Limb C")
    };
    for (std::size_t i = 0; i < limbNames.size(); ++i) {
        addGeometryOverride(limbNames[i] + tr(" — effective area"),
            limbAreaOverrideEnabled_[i], limbAreaOverrides_[i], tr(" mm²"), 3, 10000000.0);
        addGeometryOverride(limbNames[i] + tr(" — mean path"),
            limbPathOverrideEnabled_[i], limbPathOverrides_[i], tr(" mm"), 2, 10000000.0);
        addGeometryOverride(limbNames[i] + tr(" — air gap"),
            limbGapOverrideEnabled_[i], limbGapOverrides_[i], tr(" mm"), 3, 1000000.0);
        addGeometryOverride(limbNames[i] + tr(" — effective gap area"),
            limbGapAreaOverrideEnabled_[i], limbGapAreaOverrides_[i], tr(" mm²"), 3, 10000000.0);
    }
    const std::array<QString, 4> yokeNames{
        tr("Top A-B"), tr("Top B-C"), tr("Bottom A-B"), tr("Bottom B-C")
    };
    for (std::size_t i = 0; i < yokeNames.size(); ++i) {
        addGeometryOverride(yokeNames[i] + tr(" — effective area"),
            yokeAreaOverrideEnabled_[i], yokeAreaOverrides_[i], tr(" mm²"), 3, 10000000.0);
        addGeometryOverride(yokeNames[i] + tr(" — segment path"),
            yokePathOverrideEnabled_[i], yokePathOverrides_[i], tr(" mm"), 2, 10000000.0);
    }
    branchGeometryGroup->setToolTip(tr(
        "Unchecked fields inherit the corresponding legacy symmetric geometry. "
        "Checked fields override one branch; an explicitly entered zero air gap is valid."));
    advancedLayout->addWidget(branchGeometryGroup);

    auto* dcBiasGroup = new QGroupBox(tr("Per-phase DC current bias"));
    auto* dcBiasForm = new QFormLayout(dcBiasGroup);
    const std::array<QString, 3> dcPhaseNames{tr("Phase A"), tr("Phase B"), tr("Phase C")};
    for (std::size_t i = 0; i < phaseDcBiasCurrentA_.size(); ++i) {
        phaseDcBiasCurrentA_[i] = new QDoubleSpinBox;
        phaseDcBiasCurrentA_[i]->setRange(-1000000.0, 1000000.0);
        phaseDcBiasCurrentA_[i]->setDecimals(4);
        phaseDcBiasCurrentA_[i]->setValue(0.0);
        phaseDcBiasCurrentA_[i]->setSuffix(tr(" A DC"));
        dcBiasForm->addRow(dcPhaseNames[i], phaseDcBiasCurrentA_[i]);
    }
    auto* dcBiasHelp = new QLabel(tr(
        "These values are added to the instantaneous AC phase currents only in the magnetic network. "
        "They represent externally established DC bias; the circuit AC solver does not calculate a DC operating point. "
        "This is not a hysteresis or remanence model."));
    dcBiasHelp->setWordWrap(true);
    dcBiasForm->addRow(dcBiasHelp);
    advancedLayout->addWidget(dcBiasGroup);

    coreBhCurve_ = new QPlainTextEdit;
    coreBhCurve_->setPlaceholderText(
        tr("Enter measured B-H points, one per line: H [A/m], B [T]\n"
           "Example format only (replace with actual material data):\n"
           "0, 0\n100, 0.5\n1000, 1.0"));
    coreBhCurve_->setMaximumHeight(125);
    advancedLayout->addWidget(new QLabel(tr("Core material B-H curve — H [A/m], B [T]")));
    advancedLayout->addWidget(coreBhCurve_);

    auto* saturationHelp = new QLabel(tr(
        "Red line: entered manufacturer peak-current reference. Purple line: single-path B-H knee-current estimate. For the coupled three-limb model, enter yoke area Ay and yoke segment length ly. Optional branch overrides let you specify independent A/B/C limb areas, paths and air gaps, plus each of the four yoke segment areas and paths; unchecked overrides inherit the common values. Optional effective gap areas provide a user-supplied analytical fringing approximation; no geometry-derived correction or FEM field solution is implied. Joints and hysteresis are not modelled."));
    saturationHelp->setWordWrap(true);
    advancedLayout->addWidget(saturationHelp);

    auto* spectrumHelp = new QLabel(tr(
        "Voltage-source spectrum: enter measured/calculated phase-A RMS harmonic voltage magnitudes. "
        "The fundamental positive-sequence voltage is derived from nominal line voltage; add only "
        "non-fundamental orders here. Use Positive or Negative sequence. Zero sequence is not supported. "
        "This voltage spectrum is distinct from the nonlinear-load current injection table below. "
        "Zero sequence and unbalanced phase-by-phase spectra are not supported."));
    spectrumHelp->setWordWrap(true);
    advancedLayout->addWidget(spectrumHelp);

    harmonicSpectrumTable_ = new QTableWidget(0, 4);
    harmonicSpectrumTable_->setHorizontalHeaderLabels({
        tr("Harmonic order"), tr("Sequence"), tr("Phase-A source voltage RMS (V)"), tr("Phase angle (°)")
    });
    harmonicSpectrumTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    harmonicSpectrumTable_->setMinimumHeight(120);
    harmonicSpectrumTable_->setEditTriggers(QAbstractItemView::DoubleClicked |
                                             QAbstractItemView::EditKeyPressed |
                                             QAbstractItemView::AnyKeyPressed);
    advancedLayout->addWidget(harmonicSpectrumTable_);

    auto* spectrumButtons = new QHBoxLayout;
    auto* addHarmonic = new QPushButton(tr("Add voltage harmonic"));
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

    auto* currentInjectionHelp = new QLabel(tr(
        "Nonlinear-load current injection: add one row per source and harmonic. "
        "Rows with the same source ID represent the same load at different orders. "
        "Current phasors are RMS phase-A values; angles are referenced to the phase-A voltage. "
        "Multiple source phasors at the same order and sequence are summed as complex currents."));
    currentInjectionHelp->setWordWrap(true);
    advancedLayout->addWidget(currentInjectionHelp);

    harmonicCurrentTable_ = new QTableWidget(0, 5);
    harmonicCurrentTable_->setHorizontalHeaderLabels({
        tr("Load source ID"), tr("Harmonic order"), tr("Sequence"),
        tr("Phase-A load current RMS (A)"), tr("Phase angle (°)")
    });
    harmonicCurrentTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    harmonicCurrentTable_->setMinimumHeight(120);
    harmonicCurrentTable_->setEditTriggers(QAbstractItemView::DoubleClicked |
                                            QAbstractItemView::EditKeyPressed |
                                            QAbstractItemView::AnyKeyPressed);
    advancedLayout->addWidget(harmonicCurrentTable_);

    auto* injectionButtons = new QHBoxLayout;
    auto* addInjection = new QPushButton(tr("Add load-current harmonic"));
    auto* removeInjection = new QPushButton(tr("Remove selected load harmonic"));
    injectionButtons->addWidget(addInjection);
    injectionButtons->addWidget(removeInjection);
    injectionButtons->addStretch();
    advancedLayout->addLayout(injectionButtons);
    connect(addInjection, &QPushButton::clicked, this, [this] {
        const int row = harmonicCurrentTable_->rowCount();
        harmonicCurrentTable_->insertRow(row);
        harmonicCurrentTable_->setItem(row, 0, new QTableWidgetItem(QStringLiteral("LOAD-1")));
        harmonicCurrentTable_->setItem(row, 1, new QTableWidgetItem(QStringLiteral("5")));
        harmonicCurrentTable_->setItem(row, 2, new QTableWidgetItem(QStringLiteral("Negative")));
        harmonicCurrentTable_->setItem(row, 3, new QTableWidgetItem(QStringLiteral("0")));
        harmonicCurrentTable_->setItem(row, 4, new QTableWidgetItem(QStringLiteral("0")));
    });
    connect(removeInjection, &QPushButton::clicked, this, [this] {
        const int row = harmonicCurrentTable_->currentRow();
        if (row >= 0)
            harmonicCurrentTable_->removeRow(row);
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
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setWidget(page);
    return scroll;
}

QWidget* MainWindow::createResultsPage()
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setSpacing(10);
    auto* heading = new QLabel(tr("Filter synthesis results"));
    heading->setStyleSheet("font-size: 20px; font-weight: 600;");
    layout->addWidget(heading);

    resultsNote_ = new QLabel(tr("No synthesis has been performed."));
    resultsNote_->setWordWrap(true);
    resultsNote_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    resultsNote_->setMinimumHeight(48);
    resultsNote_->setStyleSheet("QLabel { background: #fff4d6; color: #704b00; border: 1px solid #d6a43a; border-radius: 4px; padding: 8px; }");
    layout->addWidget(resultsNote_);

    resultsTable_ = new QTableWidget(0, 3);
    resultsTable_->setHorizontalHeaderLabels({tr("Parameter"), tr("Value"), tr("Unit")});
    resultsTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    resultsTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    resultsTable_->verticalHeader()->setDefaultSectionSize(27);
    resultsTable_->setMinimumHeight(290);
    resultsTable_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    layout->addWidget(resultsTable_);

    auto* currentPlotTitle = new QLabel(tr("Inductor phase peak current — frequency sweep"));
    currentPlotTitle->setStyleSheet("font-weight: 600;");
    layout->addWidget(currentPlotTitle);
    resultsBankCurrentPlot_ = new PlotWidget;
    resultsBankCurrentPlot_->setCurves({}, tr("Frequency (Hz, logarithmic)"), tr("Phase peak current (A)"));
    resultsBankCurrentPlot_->setMinimumHeight(250);
    resultsBankCurrentPlot_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    layout->addWidget(resultsBankCurrentPlot_);

    auto* coreFluxPlotTitle = new QLabel(tr("Three-limb core peak flux density — frequency sweep"));
    coreFluxPlotTitle->setStyleSheet("font-weight: 600;");
    layout->addWidget(coreFluxPlotTitle);
    resultsCoreFluxPlot_ = new PlotWidget;
    resultsCoreFluxPlot_->setCurves({}, tr("Frequency (Hz, logarithmic)"), tr("Peak flux density |B| (T)"));
    resultsCoreFluxPlot_->setMinimumHeight(250);
    resultsCoreFluxPlot_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    layout->addWidget(resultsCoreFluxPlot_);

    auto* impedancePlotTitle = new QLabel(tr("Bus impedance — frequency scan"));
    impedancePlotTitle->setStyleSheet("font-weight: 600;");
    layout->addWidget(impedancePlotTitle);
    resultsBusImpedancePlot_ = new PlotWidget;
    resultsBusImpedancePlot_->setCurves({}, tr("Frequency (Hz, logarithmic)"), tr("Bus impedance (Ω)"));
    resultsBusImpedancePlot_->setMinimumHeight(250);
    resultsBusImpedancePlot_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    layout->addWidget(resultsBusImpedancePlot_);

    auto* logHeading = new QLabel(tr("Calculation log"));
    logHeading->setStyleSheet("font-weight: 600;");
    layout->addWidget(logHeading);
    log_ = new QPlainTextEdit;
    log_->setReadOnly(true);
    log_->setMaximumBlockCount(500);
    log_->setPlaceholderText(tr("Application and engineering activity log"));
    log_->setMinimumHeight(150);
    layout->addWidget(log_);

    auto* scroll = new QScrollArea;
    scroll->setObjectName(QStringLiteral("calculationScrollArea"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setWidget(page);
    return scroll;
}

QWidget* MainWindow::createMaterialsPage()
{
    if (materialStore_)
        return new MaterialDatabaseWidget(*materialStore_);
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    auto* heading = new QLabel(tr("Material database unavailable"));
    heading->setStyleSheet("font-size: 20px; font-weight: 600; color: #b36b00;");
    layout->addWidget(heading);
    auto* message = new QLabel(tr("The SQLite material catalog did not initialize. Material-dependent component selection and material-backed design checks are unavailable.\n\nDiagnostic: %1").arg(databaseError_));
    message->setWordWrap(true);
    message->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(message);
    auto* hint = new QLabel(tr("Check that the application can write to its per-user data directory and that the migrations folder is deployed beside Project135.exe."));
    hint->setWordWrap(true);
    layout->addWidget(hint);
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
    auto* exportSelected = new QPushButton(tr("Export Selected"));
    auto* exportAll = new QPushButton(tr("Export All"));
    auto* importFile = new QPushButton(tr("Import File"));
    searchRow->addWidget(historySearch_, 1);
    searchRow->addWidget(refresh);
    searchRow->addWidget(exportSelected);
    searchRow->addWidget(exportAll);
    searchRow->addWidget(importFile);
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
    connect(exportSelected, &QPushButton::clicked, this,
        [this] { exportCalculationHistory(true); });
    connect(exportAll, &QPushButton::clicked, this,
        [this] { exportCalculationHistory(false); });
    connect(importFile, &QPushButton::clicked, this,
        [this] { importCalculationHistory(); });
    connect(historySearch_, &QLineEdit::returnPressed, this, [this] { refreshCalculationHistory(); });
    connect(historyTable_, &QTableWidget::currentCellChanged, this,
        [this](int currentRow, int, int, int) { showCalculationDetails(currentRow); });
    connect(loadInputs, &QPushButton::clicked, this, [this] {
        if (!historyTable_) return;
        int row = historyTable_->currentRow();
        if (row < 0) {
            const auto selected = historyTable_->selectionModel()->selectedRows();
            if (!selected.isEmpty()) row = selected.front().row();
        }
        if (row < 0) {
            QMessageBox::information(this, tr("Load calculation inputs"),
                tr("Select one calculation-history row first."));
            return;
        }
        loadCalculationInputs(row);
    });
    connect(compareGraphs, &QPushButton::clicked, this,
        [this] { compareSelectedCalculationGraphs(); });
    connect(historyPlotMetric_, &QComboBox::currentIndexChanged, this,
        [this](int) { compareSelectedCalculationGraphs(); });
    refreshCalculationHistory();
    return page;
}

void MainWindow::exportCalculationHistory(bool selectedOnly)
{
    if (!materialStore_ || !historyTable_) return;
    std::vector<std::string> ids;
    if (selectedOnly) {
        const auto rows = historyTable_->selectionModel()->selectedRows();
        for (const auto& row : rows) {
            const auto* item = historyTable_->item(row.row(), 3);
            if (item) ids.push_back(item->data(Qt::UserRole).toString().toStdString());
        }
        if (ids.empty() && historyTable_->currentRow() >= 0) {
            const auto* item = historyTable_->item(historyTable_->currentRow(), 3);
            if (item) ids.push_back(item->data(Qt::UserRole).toString().toStdString());
        }
        if (ids.empty()) {
            QMessageBox::information(this, tr("Export history"), tr("Select at least one calculation."));
            return;
        }
    } else {
        for (std::uint64_t offset = 0;; offset += 500) {
            const auto page = materialStore_->listCalculations({}, 500, offset);
            if (!page) {
                QMessageBox::warning(this, tr("Export history"),
                    QString::fromStdString(page.error().message));
                return;
            }
            for (const auto& record : page.value()) ids.push_back(record.calculationId);
            if (page.value().size() < 500) break;
        }
    }

    QJsonArray records;
    for (const auto& id : ids) {
        const auto found = materialStore_->findCalculation(id);
        if (!found || !found.value()) {
            QMessageBox::warning(this, tr("Export history"),
                found ? tr("Calculation %1 was not found.").arg(QString::fromStdString(id))
                      : QString::fromStdString(found.error().message));
            return;
        }
        const auto& r = *found.value();
        QJsonObject item;
        item.insert("calculationId", QString::fromStdString(r.calculationId));
        item.insert("projectId", QString::fromStdString(r.projectId));
        item.insert("projectName", QString::fromStdString(r.projectName));
        item.insert("productFamily", QString::fromStdString(r.productFamily));
        item.insert("input", QJsonDocument::fromJson(QByteArray::fromStdString(r.inputJson)).object());
        item.insert("result", QJsonDocument::fromJson(QByteArray::fromStdString(r.resultJson)).object());
        item.insert("engineVersions", QJsonDocument::fromJson(QByteArray::fromStdString(r.engineVersionsJson)).object());
        item.insert("contentHash", QString::fromStdString(r.contentHash));
        item.insert("author", QString::fromStdString(r.author));
        item.insert("createdAtUtc", QString::fromStdString(r.createdAtUtc));
        QJsonArray links;
        const auto materialLinks = materialStore_->calculationMaterials(r.calculationId);
        if (!materialLinks) {
            QMessageBox::warning(this, tr("Export history"),
                QString::fromStdString(materialLinks.error().message));
            return;
        }
        for (const auto& link : materialLinks.value()) {
            const auto revision = materialStore_->findRevision(link.revisionId);
            if (!revision || !revision.value()) {
                QMessageBox::warning(this, tr("Export history"),
                    tr("Exact material revision %1 is unavailable; export was cancelled.")
                        .arg(QString::fromStdString(link.revisionId)));
                return;
            }
            QJsonObject ref;
            ref.insert("usageId", QString::fromStdString(link.usageId));
            ref.insert("revisionId", QString::fromStdString(link.revisionId));
            links.append(ref);
        }
        item.insert("materialLinks", links);
        records.append(item);
    }

    QJsonObject archive;
    archive.insert("format", "Project135.CalculationHistory");
    archive.insert("formatVersion", 1);
    archive.insert("exportedAtUtc", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    archive.insert("calculations", records);
    const QString path = QFileDialog::getSaveFileName(this, tr("Export calculation history"),
        selectedOnly ? QStringLiteral("calculation-history.p135history")
                     : QStringLiteral("calculation-history-all.p135history"),
        tr("Project135 history (*.p135history);;JSON (*.json)"));
    if (path.isEmpty()) return;
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) ||
        file.write(QJsonDocument(archive).toJson(QJsonDocument::Indented)) < 0 ||
        !file.commit()) {
        QMessageBox::critical(this, tr("Export history"),
            tr("Cannot write history archive: %1").arg(file.errorString()));
        return;
    }
    QMessageBox::information(this, tr("Export history"),
        tr("Exported %1 calculation(s).").arg(records.size()));
}

void MainWindow::importCalculationHistory()
{
    if (!materialStore_) {
        QMessageBox::warning(this, tr("Import history"), tr("SQLite database is unavailable."));
        return;
    }
    const QString path = QFileDialog::getOpenFileName(this, tr("Import calculation history"),
        {}, tr("Project135 history (*.p135history *.json);;All files (*.*)"));
    if (path.isEmpty()) return;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(this, tr("Import history"), file.errorString());
        return;
    }
    const QByteArray archiveBytes = file.readAll();
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(archiveBytes, &parseError);
    nlohmann::json archiveJson;
    try {
        archiveJson = nlohmann::json::parse(archiveBytes.constData(),
                                            archiveBytes.constData() + archiveBytes.size());
    } catch (const nlohmann::json::exception&) {
        QMessageBox::warning(this, tr("Import history"), tr("Invalid JSON archive."));
        return;
    }
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        QMessageBox::warning(this, tr("Import history"), tr("Invalid JSON archive: %1").arg(parseError.errorString()));
        return;
    }
    const auto archive = document.object();
    if (archive.value("format").toString() != QStringLiteral("Project135.CalculationHistory") ||
        archive.value("formatVersion").toInt() != 1 ||
        !archive.value("calculations").isArray()) {
        QMessageBox::warning(this, tr("Import history"), tr("Unsupported history archive format or version."));
        return;
    }

    int imported = 0, duplicates = 0, repairedHashes = 0;
    std::size_t calculationIndex = 0;
    for (const auto& value : archive.value("calculations").toArray()) {
        const auto itemJson = archiveJson.at("calculations").at(calculationIndex++);
        if (!value.isObject()) {
            QMessageBox::warning(this, tr("Import history"), tr("Archive contains a malformed calculation entry."));
            return;
        }
        const auto item = value.toObject();
        const auto requiredString = [&item](const char* key) { return item.value(QLatin1String(key)).toString(); };
        const QString id = requiredString("calculationId");
        const QString projectId = requiredString("projectId");
        const QString projectName = requiredString("projectName");
        const QString family = requiredString("productFamily");
        const auto input = item.value("input").toObject();
        const auto result = item.value("result").toObject();
        const auto versions = item.value("engineVersions").toObject();
        if (id.isEmpty() || projectId.isEmpty() || projectName.isEmpty() || family.isEmpty() ||
            input.isEmpty() || result.isEmpty() || versions.isEmpty()) {
            QMessageBox::warning(this, tr("Import history"), tr("Calculation metadata or JSON payload is incomplete."));
            return;
        }
        // Preserve the archive's numeric JSON values for deterministic hashing.
        // QJsonDocument can reformat floating-point numbers during serialization.
        const auto& inputValue = itemJson.at("input");
        const auto& resultValue = itemJson.at("result");
        const auto& versionsValue = itemJson.at("engineVersions");
        const std::string inputJson = inputValue.dump();
        const std::string resultJson = resultValue.dump();
        const std::string versionsJson = versionsValue.dump();
        const std::string envelope = "{\"engineVersions\":" + versionsJson +
            ",\"input\":" + inputJson + ",\"result\":" + resultJson + "}";
        const auto canonical = p135::infrastructure::sqlite::canonicalizeJson(envelope);
        const auto hash = canonical ? p135::infrastructure::sqlite::sha256Hex(canonical.value())
                                   : p135::Result<std::string>::failure(canonical.error());
        if (!hash) {
            QMessageBox::warning(this, tr("Import history"),
                tr("Cannot calculate SHA-256 for calculation %1: %2")
                    .arg(id, QString::fromStdString(hash.error().message)));
            return;
        }
        const std::string archivedHash = requiredString("contentHash").toStdString();
        if (hash.value() != archivedHash) {
            const auto answer = QMessageBox::warning(this, tr("History checksum mismatch"),
                tr("The SHA-256 checksum stored in the archive does not match calculation %1.\\n\\n"
                   "Stored: %2\\nCalculated from the imported data: %3\\n\\n"
                   "Recalculate the checksum and import this record?\\n"
                   "Choose No to stop the import.")
                    .arg(id, QString::fromStdString(archivedHash),
                         QString::fromStdString(hash.value())),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (answer != QMessageBox::Yes)
                return;
            ++repairedHashes;
        }
        const auto existing = materialStore_->findCalculation(id.toStdString());
        if (!existing) {
            QMessageBox::warning(this, tr("Import history"), QString::fromStdString(existing.error().message));
            return;
        }
        if (existing.value()) { ++duplicates; continue; }

        std::vector<p135::infrastructure::sqlite::CalculationMaterialReference> refs;
        for (const auto& linkValue : item.value("materialLinks").toArray()) {
            const auto link = linkValue.toObject();
            const QString revisionId = link.value("revisionId").toString();
            const QString usageId = link.value("usageId").toString();
            const auto revision = materialStore_->findRevision(revisionId.toStdString());
            if (revisionId.isEmpty() || usageId.isEmpty() || !revision || !revision.value()) {
                QMessageBox::warning(this, tr("Import history"),
                    tr("Calculation %1 requires exact material revision %2, which is not present in this database. Import stopped; no substitute was selected.")
                        .arg(id, revisionId));
                return;
            }
            refs.push_back({usageId.toStdString(), revisionId.toStdString()});
        }
        p135::infrastructure::sqlite::CalculationRecord record{
            id.toStdString(), projectId.toStdString(), projectName.toStdString(), family.toStdString(),
            inputJson, resultJson, versionsJson, hash.value(),
            requiredString("author").toStdString(), requiredString("createdAtUtc").toStdString()
        };
        const auto saved = materialStore_->recordCalculation(record, refs);
        if (!saved) {
            QMessageBox::warning(this, tr("Import history"),
                tr("Import stopped at %1: %2").arg(id, QString::fromStdString(saved.error().message)));
            refreshCalculationHistory();
            return;
        }
        ++imported;
    }
    refreshCalculationHistory();
    QMessageBox::information(this, tr("Import history"),
        tr("Imported %1 calculation(s); skipped %2 duplicate(s); repaired %3 checksum(s).")
            .arg(imported).arg(duplicates).arg(repairedHashes));
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
    projectId_ = QString::fromStdString(record.value()->projectId);
    if (projectName_) projectName_->setText(
        input.value(QStringLiteral("projectName")).toString(
            QString::fromStdString(record.value()->projectName)));

    // Restore Project page fields when present. Missing keys are expected in older snapshots.
    const auto restoreProjectLine = [this, &input](const char* objectName, const char* key) {
        if (auto* widget = findChild<QLineEdit*>(QString::fromLatin1(objectName))) {
            const auto value = input.value(QString::fromLatin1(key));
            if (value.isString()) widget->setText(value.toString());
        }
    };
    restoreProjectLine("projectCustomer", "projectCustomer");
    restoreProjectLine("projectNumber", "projectNumber");
    restoreProjectLine("projectEngineer", "projectEngineer");
    restoreProjectLine("projectDate", "projectDate");
    restoreProjectLine("projectRevision", "projectRevision");
    if (const auto value = input.value(QStringLiteral("projectAmbientC")); value.isDouble()) {
        if (auto* widget = findChild<QDoubleSpinBox*>(QStringLiteral("projectAmbientC")))
            widget->setValue(value.toDouble());
    }
    if (const auto value = input.value(QStringLiteral("projectAltitudeM")); value.isDouble()) {
        if (auto* widget = findChild<QSpinBox*>(QStringLiteral("projectAltitudeM")))
            widget->setValue(value.toInt());
    }
    const auto restoreProjectCombo = [this, &input](const char* objectName, const char* key) {
        if (auto* widget = findChild<QComboBox*>(QString::fromLatin1(objectName))) {
            const auto value = input.value(QString::fromLatin1(key));
            if (!value.isString()) return;
            const int index = widget->findText(value.toString());
            if (index >= 0) widget->setCurrentIndex(index);
        }
    };
    restoreProjectCombo("projectInsulationClass", "projectInsulationClass");
    restoreProjectCombo("projectIpRating", "projectIpRating");
    if (const auto value = input.value(QStringLiteral("projectCoolingMethod")); value.isString()) {
        if (auto* widget = findChild<QComboBox*>(QStringLiteral("projectCoolingMethod"))) {
            const int index = widget->findData(value.toString());
            if (index >= 0) widget->setCurrentIndex(index);
        }
    }
    if (const auto value = input.value(QStringLiteral("projectAirflowM3h")); value.isDouble()) {
        if (auto* widget = findChild<QDoubleSpinBox*>(QStringLiteral("projectAirflowM3h")))
            widget->setValue(value.toDouble());
    }
    if (const auto value = input.value(QStringLiteral("projectProductFamily")); value.isString()) {
        if (productFamily_) {
            const int index = productFamily_->findText(value.toString());
            if (index >= 0) productFamily_->setCurrentIndex(index);
        }
    }
    const auto setNumber = [&input](const char* key, QDoubleSpinBox* control, double divisor = 1.0) {
        if (control && input.value(QLatin1String(key)).isDouble())
            control->setValue(input.value(QLatin1String(key)).toDouble() / divisor);
    };
    setNumber("nominalLineVoltage_V", nominalVoltage_);
    setNumber("fundamentalFrequency_Hz", fundamentalFrequency_);
    setNumber("targetReactivePower_var", targetReactivePower_, 1000.0);
    setNumber("qualityFactor", qualityFactor_);
    setNumber("sourceResistance_ohm", sourceResistance_);
    // Snapshot stores SI henries; the editor control is displayed in mH.
    setNumber("sourceInductance_H", sourceInductance_, 0.001);
    setNumber("inductorSaturationCurrent_A_peak", inductorSaturationCurrent_);
    if (inductorSaturationSource_)
        inductorSaturationSource_->setText(
            input.value(QStringLiteral("inductorSaturationSource")).toString());
    setNumber("coreEffectiveArea_mm2", coreEffectiveAreaMm2_);
    setNumber("coreMeanPath_mm", coreMeanPathMm_);
    setNumber("coreAirGap_mm", coreAirGapMm_);
    setNumber("coreGapEffectiveArea_mm2", coreGapEffectiveAreaMm2_);
    setNumber("coreYokeEffectiveArea_mm2", coreYokeEffectiveAreaMm2_);
    setNumber("coreYokeSegmentPath_mm", coreYokeSegmentPathMm_);
    const auto restoreGeometryOverrides = [&input](
        const QString& key, const auto& enabledControls, const auto& valueControls) {
        const auto values = input.value(key).toArray();
        for (std::size_t i = 0; i < enabledControls.size(); ++i) {
            const bool hasEntry = static_cast<int>(i) < values.size();
            const auto entry = hasEntry ? values.at(static_cast<int>(i)).toObject()
                                        : QJsonObject{};
            if (enabledControls[i]) {
                enabledControls[i]->setChecked(
                    hasEntry && entry.value(QStringLiteral("enabled")).toBool(false));
            }
            if (valueControls[i]) {
                valueControls[i]->setValue(
                    hasEntry ? entry.value(QStringLiteral("value")).toDouble(0.0) : 0.0);
            }
        }
    };
    restoreGeometryOverrides(QStringLiteral("limbAreaOverrides"),
        limbAreaOverrideEnabled_, limbAreaOverrides_);
    restoreGeometryOverrides(QStringLiteral("limbPathOverrides"),
        limbPathOverrideEnabled_, limbPathOverrides_);
    restoreGeometryOverrides(QStringLiteral("limbGapOverrides"),
        limbGapOverrideEnabled_, limbGapOverrides_);
    restoreGeometryOverrides(QStringLiteral("yokeAreaOverrides"),
        yokeAreaOverrideEnabled_, yokeAreaOverrides_);
    restoreGeometryOverrides(QStringLiteral("yokePathOverrides"),
        yokePathOverrideEnabled_, yokePathOverrides_);
    const auto savedDcBias = input.value(QStringLiteral("phaseDcBiasCurrent_A")).toArray();
    for (std::size_t phase = 0; phase < phaseDcBiasCurrentA_.size(); ++phase) {
        if (phaseDcBiasCurrentA_[phase]) {
            phaseDcBiasCurrentA_[phase]->setValue(
                static_cast<int>(phase) < savedDcBias.size()
                    ? savedDcBias.at(static_cast<int>(phase)).toDouble(0.0) : 0.0);
        }
    }
    if (inductorTurns_ && input.value(QStringLiteral("inductorTurns")).isDouble())
        inductorTurns_->setValue(input.value(QStringLiteral("inductorTurns")).toInt());
    if (coreBhSource_)
        coreBhSource_->setText(input.value(QStringLiteral("coreBhSource")).toString());
    if (coreBhCurve_) {
        const auto savedBh = input.value(QStringLiteral("coreBhCurve")).toArray();
        QStringList lines;
        for (const auto& entry : savedBh) {
            const auto point = entry.toObject();
            lines.push_back(QStringLiteral("%1, %2")
                .arg(point.value(QStringLiteral("H_A_per_m")).toDouble(), 0, 'g', 12)
                .arg(point.value(QStringLiteral("B_T")).toDouble(), 0, 'g', 12));
        }
        coreBhCurve_->setPlainText(lines.join(QLatin1Char('\n')));
    }
    setNumber("ambientTemperature_C", ambientTemperature_);
    setNumber("capacitorThermalResistance_K_per_W", capacitorThermalResistance_);
    setNumber("resistorThermalResistance_K_per_W", resistorThermalResistance_);
    if (tuningHarmonicOrder_ && input.value(QStringLiteral("tuningHarmonicOrder")).isDouble())
        tuningHarmonicOrder_->setValue(input.value(QStringLiteral("tuningHarmonicOrder")).toDouble());
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

    // Restore nonlinear-load current injections independently from the voltage spectrum.
    if (harmonicCurrentTable_) {
        const auto injections = input.value(QStringLiteral("harmonicCurrentInjections")).toArray();
        harmonicCurrentTable_->setRowCount(0);
        for (const auto& entry : injections) {
            const auto point = entry.toObject();
            const int newRow = harmonicCurrentTable_->rowCount();
            harmonicCurrentTable_->insertRow(newRow);
            const QStringList values{
                point.value(QStringLiteral("sourceId")).toString(),
                QString::number(point.value(QStringLiteral("order")).toInt()),
                point.value(QStringLiteral("sequence")).toString(),
                QString::number(point.value(QStringLiteral("currentRms_A")).toDouble(), 'g', 12),
                QString::number(point.value(QStringLiteral("phase_deg")).toDouble(), 'g', 12)
            };
            for (int column = 0; column < values.size(); ++column)
                harmonicCurrentTable_->setItem(newRow, column, new QTableWidgetItem(values[column]));
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
        tuningHarmonicOrder_->value(),
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

    const double sourceResistanceOhm = sourceResistance_ ? sourceResistance_->value() : 0.0;
    const double sourceInductanceH = sourceInductance_ ? sourceInductance_->value() * 1.0e-3 : 0.0;
    const auto circuitResult = p135::filter::buildCircuit(result.value(),
        p135::units::Resistance{sourceResistanceOhm},
        p135::units::Inductance{sourceInductanceH});
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

    std::optional<p135::domain::MagneticCircuitRequest> magneticCircuitBase;
    QString magneticModelWarning;
    const bool anyMagneticInput =
        (coreEffectiveAreaMm2_ && coreEffectiveAreaMm2_->value() > 0.0) ||
        (coreMeanPathMm_ && coreMeanPathMm_->value() > 0.0) ||
        (coreAirGapMm_ && coreAirGapMm_->value() > 0.0) ||
        (coreYokeEffectiveAreaMm2_ && coreYokeEffectiveAreaMm2_->value() > 0.0) ||
        (coreYokeSegmentPathMm_ && coreYokeSegmentPathMm_->value() > 0.0) ||
        (inductorTurns_ && inductorTurns_->value() > 0) ||
        (coreBhCurve_ && !coreBhCurve_->toPlainText().trimmed().isEmpty()) ||
        (coreBhSource_ && !coreBhSource_->text().trimmed().isEmpty());
    if (anyMagneticInput) {
        p135::domain::MagneticCircuitRequest request;
        request.core.effectiveArea = p135::units::Area{
            coreEffectiveAreaMm2_ ? coreEffectiveAreaMm2_->value() * 1.0e-6 : 0.0};
        request.core.meanMagneticPath = p135::units::Length{
            coreMeanPathMm_ ? coreMeanPathMm_->value() * 1.0e-3 : 0.0};
        request.core.airGap = p135::units::Length{
            coreAirGapMm_ ? coreAirGapMm_->value() * 1.0e-3 : 0.0};
        if (coreGapEffectiveAreaMm2_ && coreGapEffectiveAreaMm2_->value() > 0.0)
            request.gapEffectiveArea = p135::units::Area{coreGapEffectiveAreaMm2_->value() * 1.0e-6};
        request.turns = inductorTurns_ ? static_cast<std::uint32_t>(inductorTurns_->value()) : 0U;
        request.material.id = "USER_BH_CURVE";
        request.material.grade = "User-entered B-H curve";
        request.material.dataSource = p135::domain::MaterialDataSource::UserDefined;
        request.material.sourceReference = coreBhSource_ ? coreBhSource_->text().trimmed().toStdString() : "";
        request.material.datasetRevision = "user-entered-v1";
        bool curveValid = true;
        if (coreBhCurve_) {
            const auto lines = coreBhCurve_->toPlainText().split(QRegularExpression("[\\r\\n]+"),
                Qt::SkipEmptyParts);
            for (const auto& rawLine : lines) {
                const QString line = rawLine.trimmed();
                if (line.startsWith('#')) continue;
                const auto fields = line.split(QRegularExpression("[,;\\s]+"), Qt::SkipEmptyParts);
                bool hOk = false;
                bool bOk = false;
                if (fields.size() != 2) {
                    curveValid = false;
                    break;
                }
                const double h = fields[0].toDouble(&hOk);
                const double b = fields[1].toDouble(&bOk);
                if (!hOk || !bOk || !std::isfinite(h) || !std::isfinite(b)) {
                    curveValid = false;
                    break;
                }
                request.material.bhCurve.push_back({
                    p135::units::MagneticFieldStrength{h},
                    p135::units::FluxDensity{b}
                });
            }
        }
        if (request.core.effectiveArea.si() > 0.0 &&
            request.core.meanMagneticPath.si() > 0.0 &&
            request.turns > 0 && !request.material.sourceReference.empty() &&
            curveValid && request.material.bhCurve.size() >= 2) {
            magneticCircuitBase = std::move(request);
        } else {
            magneticModelWarning = tr("Magnetic B-H model NOT_VERIFIED: supply Ae, le, air gap, turns, at least two valid H,B points and a material source reference.");
        }
    }

    if (!anyMagneticInput)
        magneticModelWarning = tr("Magnetic B-H model NOT_VERIFIED: no core geometry and measured B-H curve were supplied.");

    std::optional<p135::domain::ThreeLimbMagneticRequest> threeLimbMagneticBase;
    QString threeLimbModelWarning;
    if (magneticCircuitBase) {
        const double yokeArea = coreYokeEffectiveAreaMm2_
            ? coreYokeEffectiveAreaMm2_->value() * 1.0e-6 : 0.0;
        const double yokePath = coreYokeSegmentPathMm_
            ? coreYokeSegmentPathMm_->value() * 1.0e-3 : 0.0;
        if (yokeArea > 0.0 && yokePath > 0.0) {
            p135::domain::ThreeLimbMagneticRequest request;
            request.geometry.limbEffectiveArea = magneticCircuitBase->core.effectiveArea;
            request.geometry.limbMeanPath = magneticCircuitBase->core.meanMagneticPath;
            request.geometry.limbAirGap = magneticCircuitBase->core.airGap;
            request.geometry.yokeEffectiveArea = p135::units::Area{yokeArea};
            request.geometry.yokeSegmentPath = p135::units::Length{yokePath};
            if (coreGapEffectiveAreaMm2_ && coreGapEffectiveAreaMm2_->value() > 0.0) {
                const p135::units::Area commonEffectiveGapArea{
                    coreGapEffectiveAreaMm2_->value() * 1.0e-6};
                request.geometry.limbGapEffectiveAreas.fill(std::optional<p135::units::Area>{commonEffectiveGapArea});
            }
            for (std::size_t i = 0; i < 3; ++i) {
                if (limbAreaOverrideEnabled_[i] && limbAreaOverrideEnabled_[i]->isChecked())
                    request.geometry.limbEffectiveAreas[i] =
                        p135::units::Area{limbAreaOverrides_[i]->value() * 1.0e-6};
                if (limbPathOverrideEnabled_[i] && limbPathOverrideEnabled_[i]->isChecked())
                    request.geometry.limbMeanPaths[i] =
                        p135::units::Length{limbPathOverrides_[i]->value() * 1.0e-3};
                if (limbGapOverrideEnabled_[i] && limbGapOverrideEnabled_[i]->isChecked())
                    request.geometry.limbAirGaps[i] =
                        p135::units::Length{limbGapOverrides_[i]->value() * 1.0e-3};
                if (limbGapAreaOverrideEnabled_[i] && limbGapAreaOverrideEnabled_[i]->isChecked())
                    request.geometry.limbGapEffectiveAreas[i] =
                        p135::units::Area{limbGapAreaOverrides_[i]->value() * 1.0e-6};
            }
            for (std::size_t i = 0; i < 4; ++i) {
                if (yokeAreaOverrideEnabled_[i] && yokeAreaOverrideEnabled_[i]->isChecked())
                    request.geometry.yokeEffectiveAreas[i] =
                        p135::units::Area{yokeAreaOverrides_[i]->value() * 1.0e-6};
                if (yokePathOverrideEnabled_[i] && yokePathOverrideEnabled_[i]->isChecked())
                    request.geometry.yokeSegmentPaths[i] =
                        p135::units::Length{yokePathOverrides_[i]->value() * 1.0e-3};
            }
            request.turns = {magneticCircuitBase->turns,
                             magneticCircuitBase->turns,
                             magneticCircuitBase->turns};
            request.material = magneticCircuitBase->material;
            threeLimbMagneticBase = std::move(request);
        } else {
            threeLimbModelWarning = tr("Coupled three-limb model NOT_VERIFIED: enter both yoke effective area Ay and one yoke segment path ly.");
        }
    }

    std::optional<double> magneticKneeCurrentA;
    std::optional<double> magneticKneeFluxDensityT;
    std::vector<std::pair<double, double>> magneticFluxDensityPoints;
    std::vector<std::array<double, 8>> threeLimbFluxDensitySweep;
    std::vector<std::array<double, 5>> threeLimbSamplingMetadata; // frequency, samples, relative change, converged, within measured B-H range
    // Thevenin R/L is now stamped in series with each phase source. The sweep
    // is source-limited, but remains a single-frequency diagnostic, not the
    // multi-harmonic operating waveform used for equipment stress.
    bool magneticSweepValid = (sourceResistanceOhm > 0.0 || sourceInductanceH > 0.0) && magneticCircuitBase.has_value();
    bool magneticSweepExtrapolated = false;
    bool threeLimbSweepValid = (sourceResistanceOhm > 0.0 || sourceInductanceH > 0.0) && threeLimbMagneticBase.has_value();
    if (sourceResistanceOhm <= 0.0 && sourceInductanceH <= 0.0 && (magneticCircuitBase || threeLimbMagneticBase)) {
        magneticModelWarning = tr("Magnetic frequency sweep NOT_VERIFIED: enter non-zero Thevenin source R or L to obtain a source-limited response.");
        threeLimbModelWarning = magneticModelWarning;
    }
    if (magneticCircuitBase) {
        auto kneeProbe = *magneticCircuitBase;
        kneeProbe.current = p135::units::Current{0.0};
        const auto kneeResult = p135::domain::analyzeMagneticCircuit(kneeProbe);
        if (!kneeResult) {
            magneticSweepValid = false;
            magneticModelWarning = tr("Magnetic B-H model NOT_VERIFIED: %1")
                .arg(QString::fromStdString(kneeResult.error().message));
        } else {
            if (kneeResult.value().kneeCurrent)
                magneticKneeCurrentA = kneeResult.value().kneeCurrent->si();
            if (kneeResult.value().kneeFluxDensity)
                magneticKneeFluxDensityT = kneeResult.value().kneeFluxDensity->si();
        }
    }
    double peakBankCurrent = 0.0;
    double peakBankCurrentFrequency = 0.0;
    double peakPhasePeakCurrent = 0.0;
    double peakPhasePeakCurrentFrequency = 0.0;
    const auto filterBranchCurrent = [&circuitResult](const p135::circuit::ElectricalResult& electrical,
                                                       const std::string& nameSuffix)
        -> std::optional<std::complex<double>> {
        const auto element = std::find_if(circuitResult.value().elements.begin(),
            circuitResult.value().elements.end(),
            [&nameSuffix](const p135::circuit::Element& candidate) {
                return candidate.kind == p135::circuit::ElementKind::Resistor &&
                       candidate.name.ends_with(nameSuffix);
            });
        if (element == circuitResult.value().elements.end()) return std::nullopt;
        const auto branch = std::find_if(electrical.branches.begin(), electrical.branches.end(),
            [element](const p135::circuit::BranchResult& candidate) {
                return candidate.component == element->id;
            });
        if (branch == electrical.branches.end()) return std::nullopt;
        return branch->rmsCurrent;
    };
    const bool isWyeConnection =
        result.value().network.connection == p135::filter::Connection::Wye;
    for (const auto& point : sweepResult.value().points) {
        std::array<std::complex<double>, 3> phasePhasors{};
        bool phaseCurrentsAvailable = false;
        if (isWyeConnection) {
            const auto a = filterBranchCurrent(point.result, " L1 R");
            const auto b = filterBranchCurrent(point.result, " L2 R");
            const auto c = filterBranchCurrent(point.result, " L3 R");
            phaseCurrentsAvailable = a && b && c;
            if (phaseCurrentsAvailable) phasePhasors = {*a, *b, *c};
        } else {
            const auto ab = filterBranchCurrent(point.result, " L1-L2 R");
            const auto bc = filterBranchCurrent(point.result, " L2-L3 R");
            const auto ca = filterBranchCurrent(point.result, " L3-L1 R");
            phaseCurrentsAvailable = ab && bc && ca;
            if (phaseCurrentsAvailable)
                phasePhasors = {*ab - *ca, *bc - *ab, *ca - *bc};
        }
        if (!phaseCurrentsAvailable) {
            magneticSweepValid = false;
            threeLimbSweepValid = false;
            magneticModelWarning = tr("Source-limited sweep NOT_VERIFIED: filter branch currents could not be mapped to the three phase conductors.");
            threeLimbModelWarning = magneticModelWarning;
            break;
        }
        double currentSquared = 0.0;
        double maximumPhaseRmsCurrent = 0.0;
        for (const auto& phaseCurrent : phasePhasors) {
            const double phaseRms = std::abs(phaseCurrent);
            currentSquared += phaseRms * phaseRms;
            maximumPhaseRmsCurrent = std::max(maximumPhaseRmsCurrent, phaseRms);
        }
        const double bankCurrent = std::sqrt(currentSquared);
        const double phasePeakCurrent = std::sqrt(2.0) * maximumPhaseRmsCurrent;
        if (bankCurrent > peakBankCurrent) {
            peakBankCurrent = bankCurrent;
            peakBankCurrentFrequency = point.frequency.si();
        }
        if (phasePeakCurrent > peakPhasePeakCurrent) {
            peakPhasePeakCurrent = phasePeakCurrent;
            peakPhasePeakCurrentFrequency = point.frequency.si();
        }
        if (magneticCircuitBase && magneticSweepValid) {
            auto magneticRequest = *magneticCircuitBase;
            magneticRequest.current = p135::units::Current{phasePeakCurrent};
            const auto magneticPoint = p135::domain::analyzeMagneticCircuit(magneticRequest);
            if (!magneticPoint) {
                magneticSweepValid = false;
                magneticModelWarning = tr("Magnetic B-H sweep NOT_VERIFIED at %1 Hz: %2")
                    .arg(point.frequency.si(), 0, 'g', 6)
                    .arg(QString::fromStdString(magneticPoint.error().message));
            } else {
                magneticFluxDensityPoints.emplace_back(
                    point.frequency.si(), magneticPoint.value().fluxDensity.si());
                magneticSweepExtrapolated = magneticSweepExtrapolated ||
                    !magneticPoint.value().withinBhCurveRange;
            }
        }
        if (threeLimbMagneticBase && threeLimbSweepValid) {
            p135::domain::ThreeLimbMagneticCycleSamplingRequest samplingRequest;
            samplingRequest.magnetic = *threeLimbMagneticBase;
            samplingRequest.phaseRmsPhasors = phasePhasors;
            for (std::size_t phase = 0; phase < 3; ++phase) {
                samplingRequest.phaseDcCurrents[phase] = p135::units::Current{
                    phaseDcBiasCurrentA_[phase] ? phaseDcBiasCurrentA_[phase]->value() : 0.0
                };
                }
            samplingRequest.relativePeakTolerance = 1.0e-3;
            samplingRequest.maximumSamples = 576;
            const auto sampledCycle =
                p135::domain::sampleThreeLimbMagneticCycle(samplingRequest);
                if (!sampledCycle) {
                    threeLimbSweepValid = false;
                    threeLimbModelWarning = tr("Coupled three-limb B-H sweep NOT_VERIFIED at %1 Hz: %2")
                        .arg(point.frequency.si(), 0, 'g', 6)
                        .arg(QString::fromStdString(sampledCycle.error().message));
                } else if (!sampledCycle.value().converged) {
                    threeLimbSamplingMetadata.push_back({
                        point.frequency.si(),
                        static_cast<double>(sampledCycle.value().samplesUsed),
                        sampledCycle.value().relativePeakChange,
                        0.0,
                        sampledCycle.value().withinBhCurveRange ? 1.0 : 0.0
                    });
                    threeLimbSweepValid = false;
                    threeLimbModelWarning = tr("Coupled three-limb B-H sweep NOT_VERIFIED at %1 Hz: peak |B| did not converge to 0.1%% by %2 samples (last relative change %3%%).")
                        .arg(point.frequency.si(), 0, 'g', 6)
                        .arg(static_cast<qulonglong>(sampledCycle.value().samplesUsed))
                        .arg(sampledCycle.value().relativePeakChange * 100.0, 0, 'g', 5);
                } else {
                    std::array<double, 8> peakDensities{};
                    peakDensities[0] = point.frequency.si();
                    for (std::size_t branch = 0; branch < 7; ++branch)
                        peakDensities[branch + 1] =
                            sampledCycle.value().peakAbsoluteFluxDensity[branch].si();
                    threeLimbFluxDensitySweep.push_back(peakDensities);
                    threeLimbSamplingMetadata.push_back({
                        point.frequency.si(),
                        static_cast<double>(sampledCycle.value().samplesUsed),
                        sampledCycle.value().relativePeakChange,
                        1.0,
                        sampledCycle.value().withinBhCurveRange ? 1.0 : 0.0
                    });
                }
            }
        }
    }
    if (magneticCircuitBase && !magneticSweepValid)
        magneticFluxDensityPoints.clear();
    if (magneticSweepExtrapolated)
        magneticModelWarning = tr("Magnetic B-H sweep EXTRAPOLATED_NOT_VERIFIED: μ0 differential-permeability continuation was used beyond the measured B-H curve.");
    if (threeLimbMagneticBase && !threeLimbSweepValid)
        threeLimbFluxDensitySweep.clear();

    std::optional<p135::filter::HarmonicAnalysisResult> harmonicResult;
    std::optional<p135::filter::FrequencyResponseResult> impedanceResult;
    QString advancedWarning;
    const double sourceResistance = sourceResistanceOhm;
    const double sourceInductance = sourceInductanceH;
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

    if (harmonicCurrentTable_) {
        for (int row = 0; row < harmonicCurrentTable_->rowCount(); ++row) {
            const auto* sourceItem = harmonicCurrentTable_->item(row, 0);
            const auto* orderItem = harmonicCurrentTable_->item(row, 1);
            const auto* sequenceItem = harmonicCurrentTable_->item(row, 2);
            const auto* magnitudeItem = harmonicCurrentTable_->item(row, 3);
            const auto* phaseItem = harmonicCurrentTable_->item(row, 4);
            if (!sourceItem || !orderItem || !sequenceItem || !magnitudeItem || !phaseItem) {
                advancedWarning = tr("Load-current injection row %1 is incomplete.").arg(row + 1);
                spectrumValid = false;
                break;
            }
            bool orderOk = false;
            bool magnitudeOk = false;
            bool phaseOk = false;
            const auto order = orderItem->text().toUInt(&orderOk);
            const double magnitude = magnitudeItem->text().toDouble(&magnitudeOk);
            const double phaseDegrees = phaseItem->text().toDouble(&phaseOk);
            const QString sourceId = sourceItem->text().trimmed();
            if (sourceId.isEmpty() || !orderOk || order == 0 || !magnitudeOk ||
                !phaseOk || !std::isfinite(magnitude) || magnitude < 0.0 ||
                !std::isfinite(phaseDegrees)) {
                advancedWarning = tr("Load-current injection row %1 has invalid source ID, order, RMS current or phase.")
                    .arg(row + 1);
                spectrumValid = false;
                break;
            }
            if (magnitude == 0.0)
                continue;
            p135::filter::HarmonicSequence sequence;
            if (sequenceItem->text().compare(tr("Positive"), Qt::CaseInsensitive) == 0)
                sequence = p135::filter::HarmonicSequence::Positive;
            else if (sequenceItem->text().compare(tr("Negative"), Qt::CaseInsensitive) == 0)
                sequence = p135::filter::HarmonicSequence::Negative;
            else {
                advancedWarning = tr("Load-current injection row %1 sequence must be Positive or Negative.")
                    .arg(row + 1);
                spectrumValid = false;
                break;
            }
            const bool matchingVoltage = std::any_of(
                harmonicRequest.sourceVoltageSpectrum.begin(),
                harmonicRequest.sourceVoltageSpectrum.end(),
                [order, sequence](const auto& voltage) {
                    return voltage.order == order && voltage.sequence == sequence;
                });
            const bool conflictingVoltage = std::any_of(
                harmonicRequest.sourceVoltageSpectrum.begin(),
                harmonicRequest.sourceVoltageSpectrum.end(),
                [order, sequence](const auto& voltage) {
                    return voltage.order == order && voltage.sequence != sequence;
                });
            if (conflictingVoltage) {
                advancedWarning = tr("Load-current injection row %1 sequence conflicts with the voltage spectrum at the same harmonic order.")
                    .arg(row + 1);
                spectrumValid = false;
                break;
            }
            if (!matchingVoltage) {
                harmonicRequest.sourceVoltageSpectrum.push_back({
                    static_cast<std::uint32_t>(order), sequence, {0.0, 0.0}
                });
            }
            harmonicRequest.loadCurrentInjections.push_back({
                sourceId.toStdString(), static_cast<std::uint32_t>(order), sequence,
                std::polar(magnitude, phaseDegrees * pi / 180.0)
            });
        }
    }

    const bool hasLoadCurrentInjection = !harmonicRequest.loadCurrentInjections.empty();
    if (!hasSourceImpedance && (suppliedHarmonics > 0 || hasLoadCurrentInjection)) {
        advancedWarning = tr("Harmonic analysis NOT_VERIFIED: enter source R and/or L from a network study.");
    } else if (hasSourceImpedance &&
               (suppliedHarmonics > 0 || hasLoadCurrentInjection || threeLimbMagneticBase.has_value()) &&
               spectrumValid) {
        const auto harmonics = p135::filter::analyzeHarmonics(harmonicRequest);
        if (harmonics) {
            harmonicResult = harmonics.value();
        } else {
            advancedWarning = tr("Harmonic analysis NOT_VERIFIED: %1")
                .arg(QString::fromStdString(harmonics.error().message));
        }
    } else if (!spectrumValid && advancedWarning.isEmpty()) {
        advancedWarning = tr("Harmonic analysis NOT_VERIFIED: invalid input spectrum.");
    } else if (suppliedHarmonics == 0 && !hasLoadCurrentInjection && advancedWarning.isEmpty()) {
        advancedWarning = tr("Harmonic THD NOT_VERIFIED: no non-fundamental voltage harmonics were entered.");
    }

    std::optional<p135::domain::ThreeLimbMagneticCycleSamplingResult> threeLimbOperatingPoint;
    QString threeLimbOperatingPointWarning;
    if (threeLimbMagneticBase) {
        if (!harmonicResult) {
            threeLimbOperatingPointWarning = tr(
                "Three-limb operating-point B NOT_VERIFIED: harmonic branch-current spectrum is unavailable; enter source R/L and a valid spectrum.");
        } else {
            p135::domain::ThreeLimbMagneticCycleSamplingRequest operatingRequest;
            operatingRequest.magnetic = *threeLimbMagneticBase;
            operatingRequest.maximumSamples = 288;
            operatingRequest.relativePeakTolerance = 1.0e-3;
            for (std::size_t phase = 0; phase < 3; ++phase) {
                operatingRequest.phaseDcCurrents[phase] = p135::units::Current{
                    phaseDcBiasCurrentA_[phase] ? phaseDcBiasCurrentA_[phase]->value() : 0.0
                };
            }
            // Sum all tuned filter branches phase-by-phase at each harmonic.
            // The resulting waveform is the actual multi-frequency operating
            // current, not the independent single-frequency resonance sweep.
            for (const auto& branchPoint : harmonicResult->branchPoints) {
                auto found = std::find_if(
                    operatingRequest.harmonicExcitations.begin(),
                    operatingRequest.harmonicExcitations.end(),
                    [&branchPoint](const auto& item) { return item.order == branchPoint.order; });
                if (found == operatingRequest.harmonicExcitations.end()) {
                    operatingRequest.harmonicExcitations.push_back({
                        branchPoint.order, branchPoint.phaseCurrentRms
                    });
                } else {
                    for (std::size_t phase = 0; phase < 3; ++phase)
                        found->phaseRmsPhasors[phase] += branchPoint.phaseCurrentRms[phase];
                }
            }
            if (operatingRequest.harmonicExcitations.empty()) {
                threeLimbOperatingPointWarning = tr(
                    "Three-limb operating-point B NOT_VERIFIED: no filter branch harmonic currents were produced.");
            } else {
                const auto operating = p135::domain::sampleThreeLimbMagneticCycle(operatingRequest);
                if (!operating) {
                    threeLimbOperatingPointWarning = tr("Three-limb operating-point B NOT_VERIFIED: %1")
                        .arg(QString::fromStdString(operating.error().message));
                } else if (!operating.value().converged) {
                    threeLimbOperatingPointWarning = tr(
                        "Three-limb operating-point B NOT_VERIFIED: peak |B| did not converge to 0.1%% by %1 samples (last relative change %2%%).")
                        .arg(static_cast<qulonglong>(operating.value().samplesUsed))
                        .arg(operating.value().relativePeakChange * 100.0, 0, 'g', 5);
                } else {
                    threeLimbOperatingPoint = operating.value();
                    if (!operating.value().withinBhCurveRange) {
                        threeLimbOperatingPointWarning = tr(
                            "Three-limb operating-point B EXTRAPOLATED_NOT_VERIFIED: one or more branches exceeded the measured B-H curve; μ0 differential-permeability continuation was used.");
                    }
                }
            }
        }
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
        auto* valueItem = new QTableWidgetItem(QString::number(value, 'g', 4));
        valueItem->setToolTip(tr("Full precision: %1").arg(QString::number(value, 'g', 15)));
        resultsTable_->setItem(row, 1, valueItem);
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
    addResult(tr("Tuning harmonic order"), branch.tuningHarmonicOrder, tr("× fundamental"));
    addResult(tr("Tuning frequency"), fundamentalFrequency_->value() *
              branch.tuningHarmonicOrder, tr("Hz"));
    addResult(tr("Target three-phase reactive power"), branch.targetReactivePower.si() / 1000.0,
              tr("kvar"));
    addResult(tr("Branch quality factor"), branch.qualityFactor, tr("—"));
    p135::circuit::Complex filterBankComplexPower{};
    for (const auto& branchResult : electricalResult.value().branches) {
        const auto element = std::find_if(circuitResult.value().elements.begin(),
            circuitResult.value().elements.end(),
            [&branchResult](const p135::circuit::Element& candidate) {
                return candidate.id == branchResult.component;
            });
        if (element == circuitResult.value().elements.end()) continue;
        if (element->name.starts_with("Thevenin") || element->name.starts_with("Source"))
            continue;
        filterBankComplexPower += branchResult.complexPowerVA;
    }
    addResult(tr("MNA filter-bank active power"), filterBankComplexPower.real(), tr("W"));
    addResult(tr("MNA filter-bank reactive power"), filterBankComplexPower.imag(), tr("var"));
    addResult(tr("MNA matrix order"),
              static_cast<double>(electricalResult.value().solver.matrixOrder), tr("unknowns"));
    addResult(tr("MNA relative residual"),
              electricalResult.value().solver.relativeResidual, tr("—"));
    const bool hasTheveninSourceImpedance =
        sourceResistanceOhm > 0.0 || sourceInductanceH > 0.0;
    addTextResult(hasTheveninSourceImpedance
            ? tr("Thevenin source-limited frequency sweep")
            : tr("Ideal-source frequency sweep"),
        hasTheveninSourceImpedance
            ? tr("FREQUENCY-RESPONSE DIAGNOSTIC — per-phase source R/L is included. This is not the multi-harmonic operating current and must not be used alone for Isat margins.")
            : tr("IDEAL-SOURCE DIAGNOSTIC — source R/L is zero. Peak currents are not operating stresses."));
    addResult(tr("Frequency-sweep peak bank current (diagnostic)"), peakBankCurrent, tr("A RMS"));
    addResult(tr("Frequency at peak bank current"), peakBankCurrentFrequency, tr("Hz"));
    addResult(tr("Frequency-sweep maximum phase current (diagnostic)"), peakPhasePeakCurrent, tr("A peak"));
    addResult(tr("Frequency at phase-current peak"), peakPhasePeakCurrentFrequency, tr("Hz"));
    addResult(tr("Frequency sweep points"), static_cast<double>(sweepResult.value().points.size()), tr("points"));
    if ((magneticKneeCurrentA && *magneticKneeCurrentA > 0.0) ||
        (inductorSaturationCurrent_ && inductorSaturationCurrent_->value() > 0.0)) {
        addTextResult(tr("Current-based saturation margins"),
            tr("NOT_VERIFIED — source impedance is not stamped into the current sweep; no operating-current margin is reported."));
    }
    if (magneticCircuitBase && magneticSweepValid && !magneticFluxDensityPoints.empty()) {
        const auto peak = std::max_element(magneticFluxDensityPoints.begin(),
            magneticFluxDensityPoints.end(), [](const auto& a, const auto& b) {
                return a.second < b.second;
            });
        addResult(tr("Peak core flux density in sweep"), peak->second, tr("T"));
        addResult(tr("Frequency at peak core flux density"), peak->first, tr("Hz"));
        if (magneticKneeFluxDensityT)
            addResult(tr("B-H knee flux density (criterion)"), *magneticKneeFluxDensityT, tr("T"));
        if (magneticKneeCurrentA)
            addResult(tr("B-H knee current (criterion)"), *magneticKneeCurrentA, tr("A peak / phase"));
        else
            appendLog(tr("B-H knee NOT_VERIFIED: supplied curve has no segment meeting the configured slope-drop criterion."));
    } else if (!magneticModelWarning.isEmpty()) {
        appendLog(magneticModelWarning);
    }
    if (magneticSweepExtrapolated)
        appendLog(magneticModelWarning);

    if (threeLimbMagneticBase && threeLimbSweepValid &&
        !threeLimbFluxDensitySweep.empty()) {
        std::array<double, 7> peakCoreDensities{};
        for (const auto& sample : threeLimbFluxDensitySweep) {
            for (std::size_t i = 0; i < 7; ++i)
                peakCoreDensities[i] = std::max(peakCoreDensities[i], sample[i + 1]);
        }
        addResult(tr("Three-limb core peak B — limb A"), peakCoreDensities[0], tr("T"));
        addResult(tr("Three-limb core peak B — limb B"), peakCoreDensities[1], tr("T"));
        addResult(tr("Three-limb core peak B — limb C"), peakCoreDensities[2], tr("T"));
        addResult(tr("Three-limb core peak B — maximum yoke segment"),
            *std::max_element(peakCoreDensities.begin() + 3, peakCoreDensities.end()), tr("T"));
        if (!threeLimbSamplingMetadata.empty()) {
            const auto worst = std::max_element(threeLimbSamplingMetadata.begin(),
                threeLimbSamplingMetadata.end(), [](const auto& a, const auto& b) {
                    return a[2] < b[2];
                });
            const auto maximumSamples = std::max_element(
                threeLimbSamplingMetadata.begin(), threeLimbSamplingMetadata.end(),
                [](const auto& a, const auto& b) { return a[1] < b[1]; });
            addResult(tr("Three-limb adaptive samples (maximum)"),
                maximumSamples->at(1), tr("samples"));
            addResult(tr("Three-limb worst successive peak |B| change"),
                worst->at(2) * 100.0, tr("%"));
            const bool allWithinMeasuredCurve = std::all_of(
                threeLimbSamplingMetadata.begin(), threeLimbSamplingMetadata.end(),
                [](const auto& item) { return item[4] != 0.0; });
            if (!allWithinMeasuredCurve)
                appendLog(tr("Coupled three-limb frequency sweep EXTRAPOLATED_NOT_VERIFIED: μ0 asymptotic continuation was used beyond the measured B-H curve."));
        }
    } else if (!threeLimbModelWarning.isEmpty()) {
        appendLog(threeLimbModelWarning);
    }

    if (threeLimbOperatingPoint) {
        const auto& peakB = threeLimbOperatingPoint->peakAbsoluteFluxDensity;
        addResult(tr("Operating-point peak B — limb A"), peakB[0].si(), tr("T"));
        addResult(tr("Operating-point peak B — limb B"), peakB[1].si(), tr("T"));
        addResult(tr("Operating-point peak B — limb C"), peakB[2].si(), tr("T"));
        addResult(tr("Operating-point peak B — top yoke A-B"), peakB[3].si(), tr("T"));
        addResult(tr("Operating-point peak B — top yoke B-C"), peakB[4].si(), tr("T"));
        addResult(tr("Operating-point peak B — bottom yoke A-B"), peakB[5].si(), tr("T"));
        addResult(tr("Operating-point peak B — bottom yoke B-C"), peakB[6].si(), tr("T"));
        addResult(tr("Operating-point magnetic samples"),
            static_cast<double>(threeLimbOperatingPoint->samplesUsed), tr("samples"));
        addResult(tr("Operating-point peak |B| convergence"),
            threeLimbOperatingPoint->relativePeakChange * 100.0, tr("%"));
        if (!threeLimbOperatingPoint->withinBhCurveRange)
            appendLog(threeLimbOperatingPointWarning);
    } else if (!threeLimbOperatingPointWarning.isEmpty()) {
        appendLog(threeLimbOperatingPointWarning);
    }

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
            QString::fromStdString(assessment.rippleCurrentExplanation));
        if (assessment.thermal.estimatedTemperature)
            addResult(tr("Capacitor estimated temperature"),
                      assessment.thermal.estimatedTemperature->si() - 273.15, tr("°C"));
        addTextResult(tr("Capacitor thermal status"), QString::fromStdString(assessment.thermal.explanation));
    }
    if (resistorThermalResult && !resistorThermalResult->nodes.empty()) {
        const auto& assessment = resistorThermalResult->nodes.front();
        addResult(tr("Damping resistor harmonic loss"),
                  assessment.loss ? assessment.loss->si() : 0.0,
                  assessment.loss ? tr("W") : tr("NOT_VERIFIED"));
        if (assessment.estimatedTemperature)
            addResult(tr("Damping resistor estimated temperature"),
                      assessment.estimatedTemperature->si() - 273.15, tr("°C"));
        addTextResult(tr("Damping resistor thermal status"), QString::fromStdString(assessment.explanation));
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
    if (threeLimbMagneticBase && threeLimbSweepValid && !threeLimbFluxDensitySweep.empty())
        calculationSummary += tr(" Coupled three-limb nonlinear magnetic network solved over 72 instantaneous samples per AC sweep frequency; peak limb and yoke flux densities are plotted and stored.");
    else if (!threeLimbModelWarning.isEmpty())
        calculationSummary += QStringLiteral(" ") + threeLimbModelWarning;
    else if (threeLimbMagneticBase && !threeLimbSweepValid)
        calculationSummary += tr(" Coupled three-limb magnetic sweep NOT_VERIFIED: %1").arg(threeLimbModelWarning);
    if (!saturationPeakReference)
        calculationSummary += tr(" Manufacturer Isat comparison NOT_VERIFIED: enter a compatible peak-current rating per phase and its evidence reference.");
    else {
        calculationSummary += tr(" Red dashed line is the entered manufacturer Isat peak-current reference.");
        if (inductorSaturationSource_ && !inductorSaturationSource_->text().trimmed().isEmpty())
            calculationSummary += tr(" Isat evidence reference recorded but not independently verified.");
        else
            calculationSummary += tr(" Isat evidence reference missing: comparison NOT_VERIFIED.");
    }
    if (!magneticModelWarning.isEmpty())
        calculationSummary += QStringLiteral(" ") + magneticModelWarning;
    else if (magneticCircuitBase && magneticSweepValid && magneticKneeCurrentA)
        calculationSummary += tr(" Purple dash-dot line is the calculated B-H knee current using the entered core geometry and B-H curve; it is a curve-slope criterion, not the manufacturer's DC-bias Isat rating.");
    else if (magneticCircuitBase && magneticSweepValid)
        calculationSummary += tr(" Magnetic operating points were calculated, but no knee segment was identified in the supplied B-H data.");

    if (materialStore_) {
        QJsonObject inputSnapshot;
        inputSnapshot.insert(QStringLiteral("nominalLineVoltage_V"), nominalVoltage_->value());
        inputSnapshot.insert(QStringLiteral("fundamentalFrequency_Hz"), fundamentalFrequency_->value());
        inputSnapshot.insert(QStringLiteral("targetReactivePower_var"), targetReactivePower_->value() * 1000.0);
        inputSnapshot.insert(QStringLiteral("tuningHarmonicOrder"), tuningHarmonicOrder_->value());
        inputSnapshot.insert(QStringLiteral("qualityFactor"), qualityFactor_->value());
        inputSnapshot.insert(QStringLiteral("connection"), filterConnection_->currentText());

        // Capture the complete Project page state in the immutable calculation snapshot.
        inputSnapshot.insert(QStringLiteral("projectName"),
            projectName_ ? projectName_->text() : QString{});
        inputSnapshot.insert(QStringLiteral("projectProductFamily"),
            productFamily_ ? productFamily_->currentText() : QString{});
        const auto saveProjectLine = [this, &inputSnapshot](const char* objectName, const char* key) {
            if (const auto* widget = findChild<QLineEdit*>(QString::fromLatin1(objectName)))
                inputSnapshot.insert(QString::fromLatin1(key), widget->text());
        };
        saveProjectLine("projectCustomer", "projectCustomer");
        saveProjectLine("projectNumber", "projectNumber");
        saveProjectLine("projectEngineer", "projectEngineer");
        saveProjectLine("projectDate", "projectDate");
        saveProjectLine("projectRevision", "projectRevision");
        if (const auto* widget = findChild<QDoubleSpinBox*>(QStringLiteral("projectAmbientC")))
            inputSnapshot.insert(QStringLiteral("projectAmbientC"), widget->value());
        if (const auto* widget = findChild<QSpinBox*>(QStringLiteral("projectAltitudeM")))
            inputSnapshot.insert(QStringLiteral("projectAltitudeM"), widget->value());
        if (const auto* widget = findChild<QComboBox*>(QStringLiteral("projectInsulationClass")))
            inputSnapshot.insert(QStringLiteral("projectInsulationClass"), widget->currentText());
        if (const auto* widget = findChild<QComboBox*>(QStringLiteral("projectIpRating")))
            inputSnapshot.insert(QStringLiteral("projectIpRating"), widget->currentText());
        if (const auto* widget = findChild<QComboBox*>(QStringLiteral("projectCoolingMethod")))
            inputSnapshot.insert(QStringLiteral("projectCoolingMethod"), widget->currentData().toString());
        if (const auto* widget = findChild<QDoubleSpinBox*>(QStringLiteral("projectAirflowM3h")))
            inputSnapshot.insert(QStringLiteral("projectAirflowM3h"), widget->value());

        inputSnapshot.insert(QStringLiteral("sourceResistance_ohm"), sourceResistance);
        inputSnapshot.insert(QStringLiteral("sourceInductance_H"), sourceInductance);
        inputSnapshot.insert(QStringLiteral("inductorSaturationCurrent_A_peak"),
            inductorSaturationCurrent_ ? inductorSaturationCurrent_->value() : 0.0);
        inputSnapshot.insert(QStringLiteral("inductorSaturationSource"),
            inductorSaturationSource_ ? inductorSaturationSource_->text() : QString{});
        inputSnapshot.insert(QStringLiteral("coreEffectiveArea_mm2"),
            coreEffectiveAreaMm2_ ? coreEffectiveAreaMm2_->value() : 0.0);
        inputSnapshot.insert(QStringLiteral("coreMeanPath_mm"),
            coreMeanPathMm_ ? coreMeanPathMm_->value() : 0.0);
        inputSnapshot.insert(QStringLiteral("coreAirGap_mm"),
            coreAirGapMm_ ? coreAirGapMm_->value() : 0.0);
        inputSnapshot.insert(QStringLiteral("coreGapEffectiveArea_mm2"),
            coreGapEffectiveAreaMm2_ ? coreGapEffectiveAreaMm2_->value() : 0.0);
        inputSnapshot.insert(QStringLiteral("coreYokeEffectiveArea_mm2"),
            coreYokeEffectiveAreaMm2_ ? coreYokeEffectiveAreaMm2_->value() : 0.0);
        inputSnapshot.insert(QStringLiteral("coreYokeSegmentPath_mm"),
            coreYokeSegmentPathMm_ ? coreYokeSegmentPathMm_->value() : 0.0);
        const auto saveGeometryOverrides = [](const auto& enabledControls,
                                               const auto& valueControls) {
            QJsonArray values;
            for (std::size_t i = 0; i < enabledControls.size(); ++i) {
                QJsonObject entry;
                entry.insert(QStringLiteral("enabled"),
                    enabledControls[i] && enabledControls[i]->isChecked());
                entry.insert(QStringLiteral("value"),
                    valueControls[i] ? valueControls[i]->value() : 0.0);
                values.append(entry);
            }
            return values;
        };
        inputSnapshot.insert(QStringLiteral("limbAreaOverrides"),
            saveGeometryOverrides(limbAreaOverrideEnabled_, limbAreaOverrides_));
        inputSnapshot.insert(QStringLiteral("limbPathOverrides"),
            saveGeometryOverrides(limbPathOverrideEnabled_, limbPathOverrides_));
        inputSnapshot.insert(QStringLiteral("limbGapOverrides"),
            saveGeometryOverrides(limbGapOverrideEnabled_, limbGapOverrides_));
        inputSnapshot.insert(QStringLiteral("yokeAreaOverrides"),
            saveGeometryOverrides(yokeAreaOverrideEnabled_, yokeAreaOverrides_));
        inputSnapshot.insert(QStringLiteral("yokePathOverrides"),
            saveGeometryOverrides(yokePathOverrideEnabled_, yokePathOverrides_));
        QJsonArray dcBiasSnapshot;
        for (const auto* control : phaseDcBiasCurrentA_)
            dcBiasSnapshot.append(control ? control->value() : 0.0);
        inputSnapshot.insert(QStringLiteral("phaseDcBiasCurrent_A"), dcBiasSnapshot);
        inputSnapshot.insert(QStringLiteral("inductorTurns"),
            inductorTurns_ ? inductorTurns_->value() : 0);
        inputSnapshot.insert(QStringLiteral("coreBhSource"),
            coreBhSource_ ? coreBhSource_->text() : QString{});
        QJsonArray coreBhSnapshot;
        if (coreBhCurve_) {
            const auto lines = coreBhCurve_->toPlainText().split(QRegularExpression("[\\r\\n]+"),
                Qt::SkipEmptyParts);
            for (const auto& rawLine : lines) {
                const QString line = rawLine.trimmed();
                if (line.startsWith('#')) continue;
                const auto fields = line.split(QRegularExpression("[,;\\s]+"), Qt::SkipEmptyParts);
                if (fields.size() != 2) continue;
                bool hOk = false, bOk = false;
                const double h = fields[0].toDouble(&hOk);
                const double b = fields[1].toDouble(&bOk);
                if (!hOk || !bOk || !std::isfinite(h) || !std::isfinite(b)) continue;
                QJsonObject point;
                point.insert(QStringLiteral("H_A_per_m"), h);
                point.insert(QStringLiteral("B_T"), b);
                coreBhSnapshot.append(point);
            }
        }
        inputSnapshot.insert(QStringLiteral("coreBhCurve"), coreBhSnapshot);
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

        QJsonArray currentInjectionSnapshot;
        if (harmonicCurrentTable_) {
            for (int row = 0; row < harmonicCurrentTable_->rowCount(); ++row) {
                const auto* sourceItem = harmonicCurrentTable_->item(row, 0);
                const auto* orderItem = harmonicCurrentTable_->item(row, 1);
                const auto* sequenceItem = harmonicCurrentTable_->item(row, 2);
                const auto* magnitudeItem = harmonicCurrentTable_->item(row, 3);
                const auto* phaseItem = harmonicCurrentTable_->item(row, 4);
                if (!sourceItem || !orderItem || !sequenceItem || !magnitudeItem || !phaseItem)
                    continue;
                QJsonObject point;
                point.insert(QStringLiteral("sourceId"), sourceItem->text());
                point.insert(QStringLiteral("order"), static_cast<int>(orderItem->text().toUInt()));
                point.insert(QStringLiteral("sequence"), sequenceItem->text());
                point.insert(QStringLiteral("currentRms_A"), magnitudeItem->text().toDouble());
                point.insert(QStringLiteral("phase_deg"), phaseItem->text().toDouble());
                currentInjectionSnapshot.append(point);
            }
        }
        inputSnapshot.insert(QStringLiteral("harmonicCurrentInjections"), currentInjectionSnapshot);

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
                tr("Frequency (Hz, logarithmic)"), tr("Source-limited diagnostic phase peak current (A)"));
            resultsBankCurrentPlot_->setEngineeringGuides(resonanceBands);
        }
        if (resultsBusImpedancePlot_) {
            resultsBusImpedancePlot_->setCurves(impedanceResult
                ? std::vector<PlotCurve>{busImpedanceCurve} : std::vector<PlotCurve>{},
                tr("Frequency (Hz, logarithmic)"), tr("Bus impedance (Ω)"));
            resultsBusImpedancePlot_->setEngineeringGuides(resonanceBands);
        }
        if (resultsCoreFluxPlot_) {
            std::vector<PlotCurve> coreCurves{
                PlotCurve{tr("Limb A |B|"), {}},
                PlotCurve{tr("Limb B |B|"), {}},
                PlotCurve{tr("Limb C |B|"), {}},
                PlotCurve{tr("Maximum yoke |B|"), {}}
            };
            if (threeLimbMagneticBase && threeLimbSweepValid) {
                for (const auto& sample : threeLimbFluxDensitySweep) {
                    for (std::size_t phase = 0; phase < 3; ++phase)
                        coreCurves[phase].points.emplace_back(sample[0], sample[phase + 1]);
                    const double maximumYoke = *std::max_element(sample.begin() + 4, sample.end());
                    coreCurves[3].points.emplace_back(sample[0], maximumYoke);
                }
            }
            resultsCoreFluxPlot_->setCurves(std::move(coreCurves),
                tr("Frequency (Hz, logarithmic)"), tr("Peak flux density |B| (T)"));
        }

        QJsonObject resultSnapshot;
        resultSnapshot.insert(QStringLiteral("results"), outputSnapshot);
        resultSnapshot.insert(QStringLiteral("calculationSummary"), calculationSummary);
        resultSnapshot.insert(QStringLiteral("frequencySweep"), frequencySweepSnapshot);
        resultSnapshot.insert(QStringLiteral("impedanceScan"), impedanceScanSnapshot);
        QJsonArray magneticSweepSnapshot;
        if (magneticCircuitBase && magneticSweepValid) {
            for (const auto& [frequencyHz, fluxDensityT] : magneticFluxDensityPoints) {
                QJsonObject sample;
                sample.insert(QStringLiteral("frequency_Hz"), frequencyHz);
                sample.insert(QStringLiteral("peakFluxDensity_T"), fluxDensityT);
                magneticSweepSnapshot.append(sample);
            }
        }
        resultSnapshot.insert(QStringLiteral("magneticSweep"), magneticSweepSnapshot);
        resultSnapshot.insert(QStringLiteral("magneticSweepWithinMeasuredBhCurve"),
            magneticSweepValid && !magneticSweepExtrapolated);
        QJsonArray threeLimbSweepSnapshot;
        if (threeLimbMagneticBase && threeLimbSweepValid) {
            for (const auto& sample : threeLimbFluxDensitySweep) {
                QJsonObject entry;
                entry.insert(QStringLiteral("frequency_Hz"), sample[0]);
                entry.insert(QStringLiteral("limbA_peak_B_T"), sample[1]);
                entry.insert(QStringLiteral("limbB_peak_B_T"), sample[2]);
                entry.insert(QStringLiteral("limbC_peak_B_T"), sample[3]);
                entry.insert(QStringLiteral("yokeTopAB_peak_B_T"), sample[4]);
                entry.insert(QStringLiteral("yokeTopBC_peak_B_T"), sample[5]);
                entry.insert(QStringLiteral("yokeBottomAB_peak_B_T"), sample[6]);
                entry.insert(QStringLiteral("yokeBottomBC_peak_B_T"), sample[7]);
                threeLimbSweepSnapshot.append(entry);
            }
        }
        resultSnapshot.insert(QStringLiteral("threeLimbMagneticSweep"), threeLimbSweepSnapshot);
        QJsonObject threeLimbOperatingSnapshot;
        if (threeLimbOperatingPoint) {
            const auto& peakB = threeLimbOperatingPoint->peakAbsoluteFluxDensity;
            threeLimbOperatingSnapshot.insert(QStringLiteral("status"),
                threeLimbOperatingPoint->withinBhCurveRange
                    ? QStringLiteral("VERIFIED_CONVERGED")
                    : QStringLiteral("EXTRAPOLATED_NOT_VERIFIED"));
            threeLimbOperatingSnapshot.insert(QStringLiteral("withinMeasuredBhCurve"),
                threeLimbOperatingPoint->withinBhCurveRange);
            threeLimbOperatingSnapshot.insert(QStringLiteral("limbA_peak_B_T"), peakB[0].si());
            threeLimbOperatingSnapshot.insert(QStringLiteral("limbB_peak_B_T"), peakB[1].si());
            threeLimbOperatingSnapshot.insert(QStringLiteral("limbC_peak_B_T"), peakB[2].si());
            threeLimbOperatingSnapshot.insert(QStringLiteral("yokeTopAB_peak_B_T"), peakB[3].si());
            threeLimbOperatingSnapshot.insert(QStringLiteral("yokeTopBC_peak_B_T"), peakB[4].si());
            threeLimbOperatingSnapshot.insert(QStringLiteral("yokeBottomAB_peak_B_T"), peakB[5].si());
            threeLimbOperatingSnapshot.insert(QStringLiteral("yokeBottomBC_peak_B_T"), peakB[6].si());
            threeLimbOperatingSnapshot.insert(QStringLiteral("samplesUsed"),
                static_cast<qint64>(threeLimbOperatingPoint->samplesUsed));
            threeLimbOperatingSnapshot.insert(QStringLiteral("relativePeakChange"),
                threeLimbOperatingPoint->relativePeakChange);
            threeLimbOperatingSnapshot.insert(QStringLiteral("relativeTolerance"), 1.0e-3);
            QJsonArray harmonicOrders;
            for (const auto& harmonic : harmonicResult->points)
                harmonicOrders.append(static_cast<qint64>(harmonic.order));
            threeLimbOperatingSnapshot.insert(QStringLiteral("harmonicOrders"), harmonicOrders);
        } else {
            threeLimbOperatingSnapshot.insert(QStringLiteral("status"), QStringLiteral("NOT_VERIFIED"));
        }
        if (!threeLimbOperatingPointWarning.isEmpty())
            threeLimbOperatingSnapshot.insert(QStringLiteral("warning"), threeLimbOperatingPointWarning);
        resultSnapshot.insert(QStringLiteral("threeLimbOperatingPoint"), threeLimbOperatingSnapshot);
        QJsonArray threeLimbSamplingSnapshot;
        for (const auto& metadata : threeLimbSamplingMetadata) {
            QJsonObject sample;
            sample.insert(QStringLiteral("frequency_Hz"), metadata[0]);
            sample.insert(QStringLiteral("samplesUsed"), metadata[1]);
            sample.insert(QStringLiteral("relativePeakChange"), metadata[2]);
            sample.insert(QStringLiteral("relativeTolerance"), 1.0e-3);
            sample.insert(QStringLiteral("converged"), metadata[3] != 0.0);
            sample.insert(QStringLiteral("withinMeasuredBhCurve"), metadata[4] != 0.0);
            threeLimbSamplingSnapshot.append(sample);
        }
        resultSnapshot.insert(QStringLiteral("threeLimbSampling"), threeLimbSamplingSnapshot);
        resultSnapshot.insert(QStringLiteral("magneticKneeCurrent_A_peak"),
            magneticKneeCurrentA.value_or(0.0));
        resultSnapshot.insert(QStringLiteral("magneticKneeFluxDensity_T"),
            magneticKneeFluxDensityT.value_or(0.0));

        QJsonObject engineVersions;
        engineVersions.insert(QStringLiteral("application"), QStringLiteral("Project135-0.1.0"));
        engineVersions.insert(QStringLiteral("filterSynthesis"), QStringLiteral("filter-synthesis-v1"));
        engineVersions.insert(QStringLiteral("circuitSolver"), QStringLiteral("circuit-ac-mna-v1"));
        engineVersions.insert(QStringLiteral("harmonics"), QStringLiteral("harmonics-v1"));
        engineVersions.insert(QStringLiteral("thermal"), QStringLiteral("thermal-steady-v1"));
        engineVersions.insert(QStringLiteral("magneticSinglePath"), QStringLiteral("nonlinear-magnetic-circuit-v1"));
        engineVersions.insert(QStringLiteral("magneticThreeLimb"), QStringLiteral("three-limb-reluctance-network-v1"));
        engineVersions.insert(QStringLiteral("magneticGeometryInput"), QStringLiteral("independent-limb-yoke-overrides-v1"));
        engineVersions.insert(QStringLiteral("magneticDcBias"), QStringLiteral("per-phase-dc-offset-cycle-sampling-v1"));
        engineVersions.insert(QStringLiteral("magneticTimeSampling"), QStringLiteral("adaptive-72-144-288-peak-B-1e-3-v2"));
        engineVersions.insert(QStringLiteral("frequencyPlotSchema"), QStringLiteral("frequency-response-v4-three-limb-magnetic-network-v1"));

        const std::string inputJson = QJsonDocument(inputSnapshot)
            .toJson(QJsonDocument::Compact).toStdString();
        const std::string resultJson = QJsonDocument(resultSnapshot)
            .toJson(QJsonDocument::Compact).toStdString();
        const std::string versionsJson = QJsonDocument(engineVersions)
            .toJson(QJsonDocument::Compact).toStdString();
        const std::string hashEnvelope = "{\"engineVersions\":" + versionsJson +
            ",\"input\":" + inputJson + ",\"result\":" + resultJson + "}";
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
            tr("Frequency (Hz, logarithmic)"), tr("Ideal-source diagnostic phase peak current (A)"));
        resultsBankCurrentPlot_->setEngineeringGuides(resonanceBands);
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

    QStringList visibleWarnings;
    visibleWarnings.push_back(tr("The frequency sweep includes the entered per-phase Thevenin R/L, but is still a single-frequency response. Do not treat its peak as the multi-harmonic operating stress; use the source-aware harmonic operating-point analysis for equipment loading."));
    if (!hasSourceImpedance)
        visibleWarnings.push_back(tr("Source impedance R/L is zero or unset. Resonance, bus-impedance scan and source-limited current response are NOT_VERIFIED."));
    if (!magneticCircuitBase && !threeLimbMagneticBase)
        visibleWarnings.push_back(tr("Core geometry, turns and a material B-H curve are not provided. Magnetic flux density and saturation checks are NOT_VERIFIED."));
    if (!harmonicResult)
        visibleWarnings.push_back(tr("No non-fundamental voltage harmonics were supplied. Harmonic distortion and harmonic component stress are NOT_VERIFIED."));
    if (!advancedWarning.isEmpty())
        visibleWarnings.push_back(advancedWarning);
    if (!componentAssessmentWarning.isEmpty())
        visibleWarnings.push_back(componentAssessmentWarning);
    if (visibleWarnings.isEmpty()) {
        resultsNote_->setText(tr("Calculation completed. Review the model assumptions and scope limitations below."));
        resultsNote_->setStyleSheet("QLabel { background: #e8f4ea; color: #205b2d; border: 1px solid #77ad80; border-radius: 4px; padding: 8px; }");
    } else {
        resultsNote_->setText(tr("⚠ INPUT / MODEL LIMITATIONS — results are preliminary and must not be treated as verified design data.\n\n%1\n\n%2")
            .arg(visibleWarnings.join(QStringLiteral("\n")))
            .arg(calculationSummary));
        resultsNote_->setStyleSheet("QLabel { background: #fff0d8; color: #704000; border: 1px solid #d18b25; border-radius: 4px; padding: 8px; }");
    }
    resultsNote_->setToolTip(calculationSummary);
    status_->setText(visibleWarnings.isEmpty()
        ? tr("Filter synthesized and AC network solved")
        : tr("Calculation completed with NOT_VERIFIED inputs — review warnings"));
    appendLog(tr("Single-tuned branch synthesized: %1 kvar, h=%2, connection=%3.")
        .arg(targetReactivePower_->value())
        .arg(tuningHarmonicOrder_->value(), 0, 'f', 3)
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
