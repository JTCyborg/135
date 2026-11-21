#include "main_window.hpp"

#include <functional>
#include <memory>
#include <nlohmann/json.hpp>
#include "material_database_widget.hpp"
#include "cad_viewport_widget.hpp"
#include "p135/infrastructure/sqlite/migration_runner.hpp"
#include "p135/infrastructure/sqlite/sqlite_material_revision_store.hpp"
#include "p135/infrastructure/sqlite/canonical_json.hpp"
#include "p135/infrastructure/sqlite/starter_material_catalog.hpp"
#include "p135/infrastructure/sqlite/revision_bound_physical_filter_magnetic.hpp"

#include "p135/application/reactor_design_inputs.hpp"
#include "p135/application/reactor_design_graph.hpp"
#include "p135/application/filter_excitation_stage_adapter.hpp"
#include "p135/application/reactor_design_execution.hpp"
#include "p135/application/ideal_reactor_sizing.hpp"
#include "p135/filter/synthesis.hpp"
#include "p135/filter/harmonics.hpp"
#include "p135/filter/analysis.hpp"
#include "p135/filter/network.hpp"
#include "p135/circuit/solver.hpp"
#include "p135/domain/component_catalog.hpp"
#include "p135/domain/magnetic_circuit.hpp"
#include "p135/domain/three_limb_magnetic.hpp"
#include "p135/cad/three_limb_core.hpp"
#include "p135/cad/assembly_presentation.hpp"
#include "p135/cad/drawing_export.hpp"

#include <TopoDS_Iterator.hxx>
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
#include <QShowEvent>
#include <QDate>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QSettings>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QHash>
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
#include <QImage>
#include <QPdfWriter>
#include <QPageSize>
#include <QPageLayout>
#include <QDebug>
#include <QColor>
#include <QSizePolicy>
#include <QPainterPath>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QFontMetrics>
#include <QFrame>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTreeWidget>
#include <QSpinBox>
#include <QStatusBar>
#include <QStringList>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QToolBar>
#include <QToolButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <QFileInfo>
#include <QVariant>
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
#include <array>


namespace {

QString projectOutputRoot()
{
    return QStringLiteral("C:/Temp/trdos/135");
}

QString projectArtifactPath(const QString& folder, const QString& projectId,
                            const QString& suffix)
{
    return QDir(projectOutputRoot()).filePath(
        folder + QLatin1Char('/') + projectId + suffix);
}

bool exportReactorAssemblyPdf(
    const p135::domain::ReactorPhysicalGeometry& geometry,
    const QString& filePath,
    QString* error)
{
    const auto validation = p135::domain::validate(geometry);
    if (!validation) {
        if (error) *error = QString::fromStdString(validation.error().message);
        return false;
    }
    for (std::size_t i = 0; i < geometry.core.joints.size(); ++i) {
        if (geometry.core.joints[i].construction == p135::domain::CoreJointType::StepLap) {
            if (error) *error = QObject::tr("PDF Step-Lap detail is not supported; drawing was not generated to avoid inventing packet geometry.");
            return false;
        }
    }
    QPdfWriter pdf(filePath);
    pdf.setPageSize(QPageSize(QPageSize::A3));
    pdf.setPageOrientation(QPageLayout::Landscape);
    pdf.setResolution(144);
    QPainter painter(&pdf);
    if (!painter.isActive()) {
        if (error) *error = QObject::tr("Could not initialize PDF painter.");
        return false;
    }
    const QRect page = pdf.pageLayout().paintRectPixels(pdf.resolution());
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(Qt::black, 1.2));
    painter.drawRect(page.adjusted(24, 24, -24, -24));
    painter.setFont(QFont(QStringLiteral("Arial"), 18, QFont::Bold));
    painter.drawText(QRect(50, 42, page.width() - 100, 34),
        Qt::AlignLeft | Qt::AlignVCenter, QObject::tr("PROJECT135 — THREE-PHASE REACTOR"));
    painter.setFont(QFont(QStringLiteral("Arial"), 10, QFont::Bold));
    painter.setPen(QColor(170, 35, 25));
    painter.drawText(QRect(50, 80, page.width() - 100, 24),
        Qt::AlignLeft | Qt::AlignVCenter,
        QObject::tr("PRELIMINARY / NOT VERIFIED — NOT FOR MANUFACTURE"));
    painter.setPen(Qt::black);

    const auto& core = geometry.core;
    const auto mm = [](double metres) { return metres * 1000.0; };
    const double wa = mm(core.limbWidths[0].si());
    const double wb = mm(core.limbWidths[1].si());
    const double wc = mm(core.limbWidths[2].si());
    const double ab = mm(core.windowWidths[0].si());
    const double bc = mm(core.windowWidths[1].si());
    const double totalW = mm(core.coreOverallWidth.si());
    const double totalH = mm(core.coreOverallHeight.si());
    const double top = 0.5 * (mm(core.yokeThicknesses[0].si()) +
                              mm(core.yokeThicknesses[1].si()));
    const double bottom = 0.5 * (mm(core.yokeThicknesses[2].si()) +
                                 mm(core.yokeThicknesses[3].si()));
    const double windowH = std::max(1.0, totalH - top - bottom);
    const double xB = wa + ab;
    const double xC = xB + wb + bc;

    const QRectF front(70.0, 170.0, 620.0, 330.0);
    const QRectF plan(770.0, 170.0, 360.0, 330.0);
    const QRectF side(1190.0, 170.0, 250.0, 330.0);
    const auto drawCoreFront = [&](const QRectF& area) {
        const double scale = std::min(area.width() / std::max(1.0, totalW),
                                      area.height() / std::max(1.0, totalH));
        const double ox = area.center().x() - totalW * scale * 0.5;
        const double oy = area.center().y() + totalH * scale * 0.5;
        const auto rect = [&](double x, double y, double w, double h) {
            painter.drawRect(QRectF(ox + x * scale, oy - (y + h) * scale,
                                    w * scale, h * scale));
        };
        rect(0, 0, totalW, bottom);
        rect(0, totalH - top, totalW, top);
        rect(0, bottom, wa, windowH);
        rect(xB, bottom, wb, windowH);
        rect(xC, bottom, wc, windowH);
    };
    painter.setFont(QFont(QStringLiteral("Arial"), 11, QFont::Bold));
    painter.drawText(QRectF(front.left(), front.top() - 28, front.width(), 22),
                     Qt::AlignLeft, QObject::tr("A — FRONT ELEVATION"));
    painter.drawText(QRectF(plan.left(), plan.top() - 28, plan.width(), 22),
                     Qt::AlignLeft, QObject::tr("B — PLAN VIEW"));
    painter.drawText(QRectF(side.left(), side.top() - 28, side.width(), 22),
                     Qt::AlignLeft, QObject::tr("C — SIDE ELEVATION"));
    painter.setPen(QPen(QColor(35, 70, 105), 1.8));
    drawCoreFront(front);

    // Plan projection: three limb footprints and four individual yoke footprints.
    const double depth = std::max({mm(core.limbDepths[0].si()),
        mm(core.limbDepths[1].si()), mm(core.limbDepths[2].si()), 1.0});
    const double planScale = std::min(plan.width() / std::max(1.0, totalW),
                                      plan.height() / depth);
    const double px = plan.center().x() - totalW * planScale * 0.5;
    const double py = plan.center().y() - depth * planScale * 0.5;
    const std::array<double,3> widths{wa,wb,wc};
    const std::array<double,3> centers{0.0,xB,xC};
    const double yokeMidX = xB + 0.5 * wb;
    const std::array<double,4> yokeX{0.0,yokeMidX,0.0,yokeMidX};
    const std::array<double,4> yokeWidths{yokeMidX,totalW-yokeMidX,
                                          yokeMidX,totalW-yokeMidX};
    for (std::size_t i=0;i<4;++i) {
        const double yokeDepth = mm(core.yokeDepths[i].si());
        painter.setPen(QPen(QColor(105, 75, 35), 1.4));
        painter.drawRect(QRectF(px+yokeX[i]*planScale,
            py+(depth-yokeDepth)*0.5*planScale,
            yokeWidths[i]*planScale,yokeDepth*planScale));
    }
    for (std::size_t i=0;i<3;++i) {
        const double limbDepth = mm(core.limbDepths[i].si());
        painter.setPen(QPen(QColor(35, 70, 105), 1.8));
        painter.drawRect(QRectF(px+centers[i]*planScale,
            py+(depth-limbDepth)*0.5*planScale,
            widths[i]*planScale,limbDepth*planScale));
        const auto& bobbin = geometry.bobbins[i];
        const auto& winding = geometry.windingPacks[i];
        const double bodyW = mm(bobbin.innerWidth.si() + 2.0*bobbin.wallThickness.si());
        const double bodyD = mm(bobbin.innerHeight.si() + 2.0*bobbin.wallThickness.si());
        const double windingW = bodyW + 2.0*mm(bobbin.windingClearance.si()+winding.radialBuild.si());
        const double windingD = bodyD + 2.0*mm(bobbin.windingClearance.si()+winding.radialBuild.si());
        const double centerX = centers[i] + 0.5*widths[i];
        painter.setPen(QPen(QColor(150, 105, 45), 1.2, Qt::DashLine));
        painter.drawRect(QRectF(px+(centerX-0.5*bodyW)*planScale,
            py+(depth-bodyD)*0.5*planScale,bodyW*planScale,bodyD*planScale));
        painter.setPen(QPen(QColor(175, 75, 30), 1.2, Qt::DashDotLine));
        painter.drawRect(QRectF(px+(centerX-0.5*windingW)*planScale,
            py+(depth-windingD)*0.5*planScale,windingW*planScale,windingD*planScale));
    }
    painter.setPen(QPen(QColor(105, 75, 35), 1.4, Qt::DashLine));
    painter.drawRect(QRectF(px,py,totalW*planScale,depth*planScale));

    const double sideDepth = mm(core.limbDepths[1].si());
    const double sideScale = std::min(side.width()/std::max(1.0,sideDepth),
                                      side.height()/std::max(1.0,totalH));
    const QRectF sideCore(side.center().x()-sideDepth*sideScale*0.5,
        side.center().y()-totalH*sideScale*0.5,
        sideDepth*sideScale,totalH*sideScale);
    painter.setPen(QPen(QColor(35,70,105),1.8));
    painter.drawRect(sideCore);
    painter.drawLine(sideCore.left(),sideCore.top()+top*sideScale,
                     sideCore.right(),sideCore.top()+top*sideScale);
    painter.drawLine(sideCore.left(),sideCore.bottom()-bottom*sideScale,
                     sideCore.right(),sideCore.bottom()-bottom*sideScale);

    painter.setPen(Qt::black);
    painter.setFont(QFont(QStringLiteral("Arial"), 9));
    const int infoY = 545;
    painter.drawText(60, infoY, QObject::tr("Overall: %1 × %2 mm")
        .arg(totalW,0,'f',2).arg(totalH,0,'f',2));
    painter.drawText(60, infoY+22, QObject::tr("Limb widths A/B/C: %1 / %2 / %3 mm")
        .arg(wa,0,'f',2).arg(wb,0,'f',2).arg(wc,0,'f',2));
    painter.drawText(60, infoY+44, QObject::tr("Windows AB/BC: %1 / %2 mm")
        .arg(ab,0,'f',2).arg(bc,0,'f',2));
    painter.drawText(60, infoY+66, QObject::tr("Air gaps: %1; joint interfaces: %2")
        .arg(static_cast<int>(core.airGaps.size()))
        .arg(static_cast<int>(core.joints.size())));
    painter.drawText(60, infoY+88, QObject::tr("Source: %1 | Revision: %2")
        .arg(QString::fromStdString(geometry.dimensionalSource),
             QString::fromStdString(geometry.dimensionalRevision)));
    painter.drawText(60, infoY+110, QObject::tr("Product: %1")
        .arg(QString::fromStdString(geometry.productId)));
    const auto segmentLabel = [](p135::domain::CoreSegment segment) {
        switch (segment) {
        case p135::domain::CoreSegment::LimbA: return QStringLiteral("LIMB A");
        case p135::domain::CoreSegment::LimbB: return QStringLiteral("LIMB B");
        case p135::domain::CoreSegment::LimbC: return QStringLiteral("LIMB C");
        case p135::domain::CoreSegment::YokeTopAB: return QStringLiteral("YOKE TOP AB");
        case p135::domain::CoreSegment::YokeTopBC: return QStringLiteral("YOKE TOP BC");
        case p135::domain::CoreSegment::YokeBottomAB: return QStringLiteral("YOKE BOTTOM AB");
        case p135::domain::CoreSegment::YokeBottomBC: return QStringLiteral("YOKE BOTTOM BC");
        }
        return QStringLiteral("UNKNOWN");
    };
    int gapLine = 0;
    for (const auto& gap : core.airGaps) {
        const QString line = QObject::tr("AIR GAP — %1: position %2 mm, length %3 mm")
            .arg(segmentLabel(gap.segment))
            .arg(mm(gap.positionFromSegmentStart.si()), 0, 'f', 2)
            .arg(mm(gap.length.si()), 0, 'f', 2);
        painter.drawText(60, infoY + 132 + gapLine * 17, line);
        ++gapLine;
    }
    painter.setPen(QPen(Qt::black, 1.0));
    painter.drawRect(QRectF(45, page.height()-100, page.width()-90, 55));
    painter.setFont(QFont(QStringLiteral("Arial"), 8));
    painter.drawText(QRectF(55,page.height()-92,page.width()-110,40),
        Qt::AlignLeft|Qt::AlignVCenter,
        QObject::tr("PRELIMINARY GEOMETRIC DRAWING. Winding envelopes are not conductor details. "
                    "Air-gap locations are scheduled but cut-face geometry is not detailed in this PDF. "
                    "No tolerances, manufacturing instructions, thermal or magnetic approval are implied."));
    painter.end();
    return true;
}

} // namespace

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
#if defined(_DEBUG) || !defined(NDEBUG)
        debugImageSaveTimer_.setSingleShot(true);
        connect(&debugImageSaveTimer_, &QTimer::timeout, this, [this] {
            qInfo().noquote() << "[PLOT-DIAG] timer fired:"
                << objectName() << "visible=" << isVisible()
                << "size=" << size()
                << "path=" << property("debugImagePath").toString();
            saveDebugImage();
        });
#endif
    }

    void setDebugImagePath(QString path)
    {
#if defined(_DEBUG) || !defined(NDEBUG)
        setProperty("debugImagePath", std::move(path));
        scheduleDebugImageSave();
#else
        Q_UNUSED(path);
#endif
    }

    void setCurves(std::vector<PlotCurve> curves, QString xLabel, QString yLabel)
    {
        curves_ = std::move(curves);
        xLabel_ = std::move(xLabel);
        yLabel_ = std::move(yLabel);
        QVariantList serializedCurves;
        for (const auto& curve : curves_) {
            QVariantMap serializedCurve;
            serializedCurve.insert(QStringLiteral("name"), curve.name);
            QVariantList serializedPoints;
            for (const auto& [x, y] : curve.points)
                serializedPoints.push_back(QVariantList{x, y});
            serializedCurve.insert(QStringLiteral("points"), serializedPoints);
            serializedCurves.push_back(serializedCurve);
        }
        setProperty("plotCurves", serializedCurves);
        update();
#if defined(_DEBUG) || !defined(NDEBUG)
        scheduleDebugImageSave();
#endif
    }

    void setEngineeringGuides(std::vector<PlotBand> resonanceBands,
                              std::optional<double> saturationPeakCurrentA = std::nullopt,
                              std::optional<double> magneticKneeCurrentA = std::nullopt)
    {
        resonanceBands_ = std::move(resonanceBands);
        saturationPeakCurrentA_ = saturationPeakCurrentA;
        magneticKneeCurrentA_ = magneticKneeCurrentA;
        update();
#if defined(_DEBUG) || !defined(NDEBUG)
        scheduleDebugImageSave();
#endif
    }

private:
#if defined(_DEBUG) || !defined(NDEBUG)
    QTimer debugImageSaveTimer_;
    bool savingDebugImage_{};

    void scheduleDebugImageSave()
    {
        if (savingDebugImage_) return;
        // Debounce layout-driven resize events. A zero-delay callback can run
        // between parent/child layout passes and capture an intermediate width.
        // Restarting the timer guarantees the PNG is rendered after geometry
        // has remained stable for the full interval.
        debugImageSaveTimer_.start(100);
    }

    void saveDebugImage()
    {
        if (savingDebugImage_) {
            qWarning().noquote() << "[PLOT-DIAG] capture skipped: recursive save" << objectName();
            return;
        }
        const QString path = property("debugImagePath").toString();
        if (path.isEmpty() || width() < 120 || height() < 100) {
            qWarning().noquote() << "[PLOT-DIAG] capture skipped: invalid path/geometry"
                << objectName() << "path=" << path << "size=" << size()
                << "visible=" << isVisible();
            return;
        }
        const QFileInfo info(path);
        if (!QDir().mkpath(info.absolutePath())) {
            qWarning().noquote() << "[PLOT-DIAG] cannot create directory:"
                << info.absolutePath();
            return;
        }
        // QWidget::render() can synchronously deliver resize events. Keep the
        // capture guarded so those events cannot recursively start another capture.
        savingDebugImage_ = true;
        struct SaveGuard {
            bool& active;
            ~SaveGuard() { active = false; }
        } saveGuard{savingDebugImage_};

        QImage image(size(), QImage::Format_ARGB32_Premultiplied);
        image.fill(palette().color(QPalette::Base));
        QPainter imagePainter(&image);
        render(&imagePainter);
        imagePainter.end();

        // Replace the previous artifact atomically: an interrupted write must
        // never leave a truncated PNG that looks like a successfully saved plot.
        QSaveFile output(path);
        if (!output.open(QIODevice::WriteOnly) || !image.save(&output, "PNG") ||
            !output.commit()) {
            const QString error = output.errorString();
            output.cancelWriting();
            qWarning().noquote() << "[PLOT-DIAG] PNG write failed:"
                << objectName() << "path=" << path << "error=" << error;
        } else {
            qInfo().noquote() << "[PLOT-DIAG] PNG saved:"
                << objectName() << "path=" << path << "size=" << image.size();
        }
    }
#endif

protected:
    void showEvent(QShowEvent* event) override
    {
        QWidget::showEvent(event);
#if defined(_DEBUG) || !defined(NDEBUG)
        // Results/history pages are initially hidden behind the workflow tabs.
        // Their first capture may be skipped before they receive a usable size;
        // retry as soon as the containing page becomes visible.
        scheduleDebugImageSave();
#endif
    }

    void resizeEvent(QResizeEvent* event) override
    {
        QWidget::resizeEvent(event);
#if defined(_DEBUG) || !defined(NDEBUG)
        scheduleDebugImageSave();
#endif
    }

    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.fillRect(rect(), palette().base());
        // Reserve a wider left gutter for both the rotated Y-axis title and tick labels.
        const QRectF area(86.0, 18.0, std::max(80, width() - 122), std::max(80, height() - 68));
        painter.setPen(palette().color(QPalette::Mid));
        painter.drawRect(area);

        double minX = std::numeric_limits<double>::max();
        double maxX = std::numeric_limits<double>::lowest();
        double minY = 0.0;
        double maxY = std::numeric_limits<double>::lowest();
        bool hasPoints = false;
        for (const auto& curve : curves_) {
            for (const auto& [x, y] : curve.points) {
                if (!std::isfinite(x) || !std::isfinite(y) || x <= 0.0 || y < 0.0) continue;
                minX = std::min(minX, x);
                maxX = std::max(maxX, x);
                maxY = std::max(maxY, y);
                hasPoints = true;
            }
        }
        if (!hasPoints) {
            painter.setPen(palette().color(QPalette::Text));
            painter.drawText(area, Qt::AlignCenter, tr("No valid frequency-response data available"));
            painter.drawText(QRectF(0, height() - 30, width(), 22), Qt::AlignCenter, xLabel_);
            return;
        }

        double logMin = std::log10(minX);
        double logMax = std::log10(maxX);
        if (!(logMax > logMin)) {
            logMin -= 0.5;
            logMax += 0.5;
        }
        const double dataYMax = maxY > 0.0 ? maxY * 1.08 : 1.0;
        const double yMax = std::max({dataYMax,
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
            painter.drawText(QRectF(26, y - 9, 56, 18), Qt::AlignRight | Qt::AlignVCenter,
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
            std::size_t validPointCount = 0;
            QPointF onlyPoint;
            for (const auto& [xValue, yValue] : curve.points) {
                if (!std::isfinite(xValue) || !std::isfinite(yValue) ||
                    xValue <= 0.0 || yValue < 0.0) {
                    started = false;
                    continue;
                }
                const double x = area.left() +
                    (std::log10(xValue) - logMin) / (logMax - logMin) * area.width();
                const double y = area.bottom() - (yValue / yMax) * area.height();
                onlyPoint = QPointF(x, y);
                ++validPointCount;
                if (!started) { path.moveTo(x, y); started = true; }
                else path.lineTo(x, y);
            }
            const QColor color = colors[static_cast<std::size_t>(curveIndex) % colors.size()];
            painter.save();
            painter.setClipRect(area);
            painter.setPen(QPen(color, 2.0));
            painter.drawPath(path);
            if (validPointCount == 1) {
                painter.setBrush(color);
                painter.drawEllipse(onlyPoint, 3.5, 3.5);
                painter.setBrush(Qt::NoBrush);
            }
            painter.restore();
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
        painter.translate(12, area.center().y());
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
    viewMenu->addAction(tr("Construction"), this, [this] { tabs_->setCurrentIndex(2); });
    viewMenu->addAction(tr("Calculation & design"), this, [this] { tabs_->setCurrentIndex(3); });
    viewMenu->addAction(tr("CAD"), this, [this] { tabs_->setCurrentIndex(4); });
    viewMenu->addAction(tr("Material database"), this, [this] { tabs_->setCurrentIndex(5); });
    viewMenu->addAction(tr("Calculation history"), this, [this] {
        tabs_->setCurrentIndex(6);
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
        tabs_->setCurrentIndex(3);
    });

    tabs_ = new QTabWidget(this);
    tabs_->addTab(createProjectPage(), tr("Project"));
    tabs_->addTab(createFilterPage(), tr("Filter definition"));
    tabs_->addTab(createConstructionPage(), tr("Construction"));
    tabs_->addTab(createResultsPage(), tr("Calculation & design"));
    tabs_->addTab(createCadPage(), tr("3D CAD"));
    tabs_->addTab(createMaterialsPage(), tr("Materials database"));
    tabs_->addTab(createHistoryPage(), tr("Calculation history"));

    // Stacking factor is a single engineering input exposed in two workflow contexts.
    // Construction stores Ks as a fraction; first-pass criteria displays it as percent.
    auto* constructionKs = findChild<QDoubleSpinBox*>(QStringLiteral("construction_stackingFactor"));
    auto* sizingKs = findChild<QDoubleSpinBox*>(QStringLiteral("idealSizing_Ks"));
    if (constructionKs && sizingKs) {
        if (constructionKs->value() > 0.0)
            sizingKs->setValue(constructionKs->value() * 100.0);
        else if (sizingKs->value() > 0.0)
            constructionKs->setValue(sizingKs->value() / 100.0);
        connect(constructionKs, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [sizingKs](double value) {
                const QSignalBlocker blocker(sizingKs);
                sizingKs->setValue(value * 100.0);
            });
        connect(sizingKs, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [constructionKs](double value) {
                const QSignalBlocker blocker(constructionKs);
                constructionKs->setValue(value / 100.0);
            });
    }
    if (auto* topologyContainer = findChild<QWidget*>(QStringLiteral("productTopologyDiagram"))) {
        if (auto* topology = dynamic_cast<ProductTopologyWidget*>(topologyContainer)) {
            topology->setConnection(filterConnection_->currentText());
            connect(filterConnection_, &QComboBox::currentTextChanged, topology,
                [topology](const QString& value) { topology->setConnection(value); });
        }
    }
    setCentralWidget(tabs_);
    connect(tabs_, &QTabWidget::currentChanged, this, [this](int index) {
        if (index == 6) refreshCalculationHistory();
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
    settings.beginGroup(QStringLiteral("filter"));
    if (ambientTemperature_) settings.setValue(QStringLiteral("thermalAmbientC"), ambientTemperature_->value());
    if (!settings.contains(QStringLiteral("thermalAmbientSource")))
        settings.setValue(QStringLiteral("thermalAmbientSource"), QStringLiteral("INHERITED_FROM_PROJECT"));
    settings.endGroup();
    settings.beginGroup(QStringLiteral("construction"));
    for (auto* widget : findChildren<QWidget*>()) {
        const QString key = widget->objectName();
        if (!key.startsWith(QStringLiteral("construction_"))) continue;
        if (const auto* combo = qobject_cast<QComboBox*>(widget))
            settings.setValue(key, combo->currentData());
        else if (const auto* number = qobject_cast<QDoubleSpinBox*>(widget))
            settings.setValue(key, number->value());
        else if (const auto* integer = qobject_cast<QSpinBox*>(widget))
            settings.setValue(key, integer->value());
        else if (const auto* line = qobject_cast<QLineEdit*>(widget))
            settings.setValue(key, line->text());
        else if (const auto* textEdit = qobject_cast<QPlainTextEdit*>(widget))
            settings.setValue(key, textEdit->toPlainText());
        else if (const auto* check = qobject_cast<QCheckBox*>(widget))
            settings.setValue(key, check->isChecked());
    }
    settings.endGroup();
    settings.endGroup();
    settings.setValue(QStringLiteral("project/currentId"), projectId_);
}

void MainWindow::loadProjectSettings()
{
    if (projectId_.isEmpty()) return;
    latestFilterSnapshotId_.clear();
    latestFilterBranchId_ = 0;
    constructionInputsDirty_ = true;
    acceptedConstructionInputs_.reset();
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
    const double projectAmbientC = get("ambientC", 40.0).toDouble();
    settings.beginGroup(QStringLiteral("filter"));
    const QString filterAmbientSource = settings.value(QStringLiteral("thermalAmbientSource")).toString();
    if (ambientTemperature_) {
        if (filterAmbientSource == QStringLiteral("MANUAL") && settings.contains(QStringLiteral("thermalAmbientC")))
            ambientTemperature_->setValue(settings.value(QStringLiteral("thermalAmbientC")).toDouble());
        else {
            ambientTemperature_->setValue(projectAmbientC);
            settings.setValue(QStringLiteral("thermalAmbientC"), projectAmbientC);
            settings.setValue(QStringLiteral("thermalAmbientSource"), QStringLiteral("INHERITED_FROM_PROJECT"));
        }
    }
    settings.endGroup();
    settings.endGroup();
    settings.beginGroup(QStringLiteral("projects/") + projectId_ + QStringLiteral("/construction"));
    for (auto* widget : findChildren<QWidget*>()) {
        const QString key = widget->objectName();
        if (!key.startsWith(QStringLiteral("construction_"))) continue;
        if (auto* combo = qobject_cast<QComboBox*>(widget)) {
            if (!settings.contains(key)) {
                combo->setCurrentIndex(0);
                continue;
            }
            const QVariant stored = settings.value(key);
            int index = combo->findData(stored);
            // Keep an exact historical material revision selectable even if a newer revision
            // exists or the material identity has since been archived.
            if (index < 0 && materialStore_ &&
                (key == QStringLiteral("construction_coreMaterial") ||
                 key == QStringLiteral("construction_conductorMaterial") ||
                 key == QStringLiteral("construction_insulationMaterial"))) {
                const auto revision = materialStore_->findRevision(stored.toString().toStdString());
                if (revision && revision.value()) {
                    const auto& item = *revision.value();
                    combo->addItem(QString::fromStdString(item.materialId) +
                        QStringLiteral(" — r%1 (pinned revision)").arg(item.revisionNumber),
                        QString::fromStdString(item.revisionId));
                    index = combo->count() - 1;
                }
            }
            if (index >= 0) combo->setCurrentIndex(index);
        } else if (auto* number = qobject_cast<QDoubleSpinBox*>(widget)) {
            number->setValue(settings.contains(key) ? settings.value(key).toDouble() : number->minimum());
        } else if (auto* integer = qobject_cast<QSpinBox*>(widget)) {
            integer->setValue(settings.contains(key) ? settings.value(key).toInt() : integer->minimum());
        } else if (auto* line = qobject_cast<QLineEdit*>(widget)) {
            line->setText(settings.value(key).toString());
        } else if (auto* textEdit = qobject_cast<QPlainTextEdit*>(widget)) {
            textEdit->setPlainText(settings.value(key).toString());
        } else if (auto* check = qobject_cast<QCheckBox*>(widget)) {
            check->setChecked(settings.contains(key) ? settings.value(key).toBool()
                : false);
        }
    }
    settings.endGroup();
    if (auto* flow = findChild<QDoubleSpinBox*>(QStringLiteral("projectAirflowM3h"))) {
        const auto* method = findChild<QComboBox*>(QStringLiteral("projectCoolingMethod"));
        flow->setEnabled(method && method->currentData().toString() == QStringLiteral("AF"));
    }
    // Inherit Project environmental inputs unless the user has overridden Construction.
    const QString projectCooling = get("coolingMethod", QStringLiteral("AN")).toString();
    auto* constructionAmbient = findChild<QDoubleSpinBox*>(QStringLiteral("construction_ambient"));
    auto* constructionCooling = findChild<QComboBox*>(QStringLiteral("construction_cooling"));
    const QString ambientSourceKey = QStringLiteral("construction/construction_ambient_source");
    const QString coolingSourceKey = QStringLiteral("construction/construction_cooling_source");
    if (constructionAmbient &&
        settings.value(ambientSourceKey).toString() != QStringLiteral("MANUAL")) {
        constructionAmbient->setValue(projectAmbientC);
        settings.setValue(ambientSourceKey, QStringLiteral("INHERITED_FROM_PROJECT"));
    }
    if (constructionCooling &&
        settings.value(coolingSourceKey).toString() != QStringLiteral("MANUAL")) {
        QString coolingData;
        if (projectCooling == QStringLiteral("AN")) coolingData = QStringLiteral("natural-air");
        else if (projectCooling == QStringLiteral("AF")) coolingData = QStringLiteral("forced-air");
        const int index = constructionCooling->findData(coolingData);
        constructionCooling->setCurrentIndex(index >= 0 ? index : 0);
        settings.setValue(coolingSourceKey, QStringLiteral("INHERITED_FROM_PROJECT"));
    }
    // Project insulation class is retained as context; it is not a winding temperature limit.
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

    auto* confirmationRow = new QHBoxLayout;
    auto* confirmationStatus = new QLabel(tr("Project data has not been confirmed."));
    confirmationStatus->setObjectName(QStringLiteral("projectConfirmationStatus"));
    confirmationStatus->setWordWrap(true);
    auto* confirmProject = new QPushButton(tr("Confirm project and continue"));
    confirmProject->setObjectName(QStringLiteral("confirmProjectAndContinue"));
    confirmationRow->addWidget(confirmationStatus, 1);
    confirmationRow->addWidget(confirmProject);
    layout->addLayout(confirmationRow);
    layout->addStretch();

    connect(confirmProject, &QPushButton::clicked, this, [this, page, confirmationStatus,
        projectDate, revision, coolingMethod, airflow] {
        const QString projectName = projectName_->text().trimmed();
        if (projectName.isEmpty()) {
            QMessageBox::warning(this, tr("Project data is incomplete"),
                tr("Enter a project name before confirming this tab."));
            projectName_->setFocus();
            return;
        }
        const QDate parsedDate = QDate::fromString(projectDate->text().trimmed(), Qt::ISODate);
        if (!parsedDate.isValid() ||
            parsedDate.toString(Qt::ISODate) != projectDate->text().trimmed()) {
            QMessageBox::warning(this, tr("Invalid project date"),
                tr("Enter the project date in YYYY-MM-DD format."));
            projectDate->setFocus();
            return;
        }
        if (revision->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, tr("Missing project revision"),
                tr("Enter a project revision, for example A."));
            revision->setFocus();
            return;
        }
        if (coolingMethod->currentData().toString() == QStringLiteral("AF") &&
            airflow->value() <= 0.0) {
            QMessageBox::warning(this, tr("Missing airflow"),
                tr("Forced-air cooling is selected. Enter a verified airflow greater than zero."));
            airflow->setFocus();
            return;
        }

        saveProjectSettings();

        QJsonObject fields;
        QJsonObject fieldLabels;
        QJsonObject fieldUnits;
        const auto* dimensionSource = findChild<QLineEdit*>(QStringLiteral("construction_dimensionalSource"));
        const auto* dimensionRevision = findChild<QLineEdit*>(QStringLiteral("construction_dimensionalRevision"));
        const bool hasDimensionSource = dimensionSource && !dimensionSource->text().trimmed().isEmpty();
        const bool hasDimensionRevision = dimensionRevision && !dimensionRevision->text().trimmed().isEmpty();
        if (hasDimensionSource != hasDimensionRevision) {
            QMessageBox::warning(this, tr("Incomplete dimensional provenance"),
                tr("Enter both the physical-dimensions source and its revision, or leave both empty."));
            return;
        }
        const QHash<QString, QString> labels{
            {QStringLiteral("projectName"), tr("Project name")},
            {QStringLiteral("projectProductFamily"), tr("Product family")},
            {QStringLiteral("projectCustomer"), tr("Customer")},
            {QStringLiteral("projectNumber"), tr("Project number")},
            {QStringLiteral("projectEngineer"), tr("Design engineer")},
            {QStringLiteral("projectDate"), tr("Project date")},
            {QStringLiteral("projectRevision"), tr("Revision")},
            {QStringLiteral("projectAmbientC"), tr("Maximum ambient temperature")},
            {QStringLiteral("projectAltitudeM"), tr("Installation altitude")},
            {QStringLiteral("projectInsulationClass"), tr("Insulation thermal class")},
            {QStringLiteral("projectIpRating"), tr("Enclosure protection")},
            {QStringLiteral("projectCoolingMethod"), tr("Cooling method")},
            {QStringLiteral("projectAirflowM3h"), tr("Forced-air volume flow")}
        };
        const QHash<QString, QString> units{
            {QStringLiteral("projectAmbientC"), QStringLiteral("°C")},
            {QStringLiteral("projectAltitudeM"), QStringLiteral("m above sea level")},
            {QStringLiteral("projectAirflowM3h"), QStringLiteral("m³/h")}
        };
        for (QWidget* widget : page->findChildren<QWidget*>()) {
            const QString key = widget->objectName();
            // Qt creates unnamed/internal editor children (for example qt_spinbox_lineedit).
            // They are implementation details, not independent project fields.
            if (key.isEmpty() || key.startsWith(QStringLiteral("qt_"))) continue;
            bool captured = false;
            if (const auto* line = qobject_cast<const QLineEdit*>(widget)) {
                fields.insert(key, line->text());
                captured = true;
            } else if (const auto* combo = qobject_cast<const QComboBox*>(widget)) {
                QJsonObject value;
                value.insert(QStringLiteral("text"), combo->currentText());
                value.insert(QStringLiteral("index"), combo->currentIndex());
                value.insert(QStringLiteral("data"), QJsonValue::fromVariant(combo->currentData()));
                fields.insert(key, value);
                captured = true;
            } else if (const auto* number = qobject_cast<const QDoubleSpinBox*>(widget)) {
                fields.insert(key, number->value());
                captured = true;
            } else if (const auto* integer = qobject_cast<const QSpinBox*>(widget)) {
                fields.insert(key, integer->value());
                captured = true;
            } else if (const auto* check = qobject_cast<const QCheckBox*>(widget)) {
                fields.insert(key, check->isChecked());
                captured = true;
            } else if (const auto* text = qobject_cast<const QPlainTextEdit*>(widget)) {
                fields.insert(key, text->toPlainText());
                captured = true;
            } else if (const auto* table = qobject_cast<const QTableWidget*>(widget)) {
                QJsonArray rows;
                for (int row = 0; row < table->rowCount(); ++row) {
                    QJsonArray cells;
                    for (int column = 0; column < table->columnCount(); ++column) {
                        const auto* item = table->item(row, column);
                        cells.append(item ? QJsonValue(item->text()) : QJsonValue(QJsonValue::Null));
                    }
                    rows.append(cells);
                }
                fields.insert(key, rows);
                captured = true;
            }
            if (captured && labels.contains(key)) {
                fieldLabels.insert(key, labels.value(key));
            }
            if (captured && units.contains(key)) {
                fieldUnits.insert(key, units.value(key));
            }
        }

        for (int yoke = 0; yoke < 4; ++yoke) {
            const QString yokeLabel = yoke == 0 ? tr("Top yoke A-B") :
                yoke == 1 ? tr("Top yoke B-C") :
                yoke == 2 ? tr("Bottom yoke A-B") : tr("Bottom yoke B-C");
            for (int gap = 0; gap < 3; ++gap) {
                const QString suffix = QString::number(yoke + 1) + QStringLiteral("_") +
                    QString::number(gap + 1);
                auto* position = findChild<QDoubleSpinBox*>(
                    QStringLiteral("construction_yokeAirGapPosition_") + suffix);
                auto* length = findChild<QDoubleSpinBox*>(
                    QStringLiteral("construction_yokeAirGapLength_") + suffix);
                const bool hasPosition = position && position->value() > position->minimum();
                const bool hasLength = length && length->value() > length->minimum();
                if (hasPosition != hasLength) {
                    QMessageBox::warning(this, tr("Incomplete yoke air-gap definition"),
                        tr("%1 air gap %2 requires both position and length.")
                            .arg(yokeLabel).arg(gap + 1));
                    if (position) position->setFocus();
                    return;
                }
            }
        }

        for (int limb = 0; limb < 3; ++limb) {
            const QString limbSuffix = QString::number(limb + 1);
            auto* count = findChild<QSpinBox*>(QStringLiteral("construction_airGapCount_") + limbSuffix);
            double enteredGapTotalMm = 0.0;
            int enteredGapCount = 0;
            for (int gap = 0; gap < 3; ++gap) {
                const QString suffix = limbSuffix + QStringLiteral("_") + QString::number(gap + 1);
                auto* position = findChild<QDoubleSpinBox*>(
                    QStringLiteral("construction_airGapPosition_") + suffix);
                const auto* length = findChild<QDoubleSpinBox*>(
                    QStringLiteral("construction_airGapLength_") + suffix);
                const bool hasPosition = position && position->value() > position->minimum();
                const bool hasLength = length && length->value() > length->minimum();
                if (hasPosition != hasLength) {
                    QMessageBox::warning(this, tr("Incomplete air-gap definition"),
                        tr("Limb %1 air gap %2 requires both position and length.")
                            .arg(QChar('A' + limb)).arg(gap + 1));
                    if (position) position->setFocus();
                    return;
                }
                if (hasPosition && hasLength) {
                    ++enteredGapCount;
                    enteredGapTotalMm += length->value();
                }
            }
            if (count && count->value() != enteredGapCount) {
                QMessageBox::warning(this, tr("Air-gap count mismatch"),
                    tr("Limb %1 declares %2 air gaps but %3 complete position/length records are entered.")
                        .arg(QChar('A' + limb)).arg(count->value()).arg(enteredGapCount));
                count->setFocus();
                return;
            }
            auto* total = findChild<QDoubleSpinBox*>(
                QStringLiteral("construction_totalAirGap_") + limbSuffix);
            if (count && total && count->value() > 0 && total->value() <= total->minimum()) {
                QMessageBox::warning(this, tr("Missing total air-gap length"),
                    tr("Limb %1 has a declared discrete air-gap count; its total length is also required.")
                        .arg(QChar('A' + limb)));
                total->setFocus();
                return;
            }
            if (count && total && count->value() == 0 && total->value() > total->minimum()) {
                QMessageBox::warning(this, tr("Missing air-gap count"),
                    tr("Limb %1 has a total air-gap length but no discrete air-gap count.")
                        .arg(QChar('A' + limb)));
                count->setFocus();
                return;
            }
            if (total && total->value() > total->minimum() && enteredGapCount > 0 &&
                std::abs(total->value() - enteredGapTotalMm) > 0.001) {
                QMessageBox::warning(this, tr("Air-gap total mismatch"),
                    tr("Limb %1 individual gap lengths sum to %2 mm, but the declared total is %3 mm.")
                        .arg(QChar('A' + limb)).arg(enteredGapTotalMm, 0, 'f', 3)
                        .arg(total->value(), 0, 'f', 3));
                total->setFocus();
                return;
            }
        }

        int selectedJointCount = 0;
        for (int joint = 0; joint < 8; ++joint) {
            const auto* control = findChild<QComboBox*>(
                QStringLiteral("construction_coreJoint_") + QString::number(joint + 1));
            if (control && !control->currentData().toString().isEmpty()) ++selectedJointCount;
        }
        if (selectedJointCount != 0 && selectedJointCount != 8) {
            QMessageBox::warning(this, tr("Incomplete core joint construction"),
                tr("Specify the construction type for all eight limb-to-yoke interfaces, or leave all eight unspecified."));
            return;
        }

        auto* ductCountControl = findChild<QSpinBox*>(QStringLiteral("construction_coolingDuctCount"));
        const int declaredDuctCount = ductCountControl ? ductCountControl->value() : 0;
        int completeDuctCount = 0;
        for (int duct = 0; duct < 3; ++duct) {
            const QString suffix = QString::number(duct + 1);
            const auto* position = findChild<QDoubleSpinBox*>(
                QStringLiteral("construction_coolingDuctPosition_") + suffix);
            const auto* thickness = findChild<QDoubleSpinBox*>(
                QStringLiteral("construction_coolingDuctThickness_") + suffix);
            const auto* material = findChild<QLineEdit*>(
                QStringLiteral("construction_coolingDuctSpacerRevision_") + suffix);
            const auto* airflowControl = findChild<QComboBox*>(
                QStringLiteral("construction_coolingDuctAirflow_") + suffix);
            const bool any = (position && position->value() > position->minimum()) ||
                (thickness && thickness->value() > thickness->minimum()) ||
                (material && !material->text().trimmed().isEmpty()) ||
                (airflowControl && !airflowControl->currentData().toString().isEmpty());
            const bool complete = position && position->value() > position->minimum() &&
                thickness && thickness->value() > thickness->minimum() &&
                material && !material->text().trimmed().isEmpty() &&
                airflowControl && !airflowControl->currentData().toString().isEmpty();
            if (any && !complete) {
                QMessageBox::warning(this, tr("Incomplete cooling duct"),
                    tr("Cooling duct %1 requires position, thickness, spacer material revision and airflow direction.")
                        .arg(duct + 1));
                return;
            }
            if (complete) ++completeDuctCount;
        }
        if (declaredDuctCount != completeDuctCount) {
            QMessageBox::warning(this, tr("Cooling duct count mismatch"),
                tr("Declared cooling duct count is %1, but %2 complete duct records are entered.")
                    .arg(declaredDuctCount).arg(completeDuctCount));
            if (ductCountControl) ductCountControl->setFocus();
            return;
        }

        QJsonObject snapshot;
        snapshot.insert(QStringLiteral("schemaVersion"), 1);
        snapshot.insert(QStringLiteral("tabId"), QStringLiteral("project"));
        snapshot.insert(QStringLiteral("tabTitle"), QStringLiteral("Project"));
        snapshot.insert(QStringLiteral("projectId"), projectId_);
        snapshot.insert(QStringLiteral("confirmedAtUtc"),
            QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
        snapshot.insert(QStringLiteral("buildConfiguration"),
#ifdef _DEBUG
            QStringLiteral("Debug")
#else
            QStringLiteral("Release")
#endif
        );
        snapshot.insert(QStringLiteral("fields"), fields);
        snapshot.insert(QStringLiteral("fieldLabels"), fieldLabels);
        snapshot.insert(QStringLiteral("fieldUnits"), fieldUnits);
        snapshot.insert(QStringLiteral("topologyPreview"),
            tr("Three-phase shunt branch: series-tuned R-L-C, termination Wye (Y)"));

#ifdef _DEBUG
        const QString outputDirectory = QStringLiteral("C:/Temp/trdos/135");
        if (!QDir().mkpath(outputDirectory)) {
            QMessageBox::critical(this, tr("Cannot save project snapshot"),
                tr("Cannot create output directory: %1").arg(outputDirectory));
            return;
        }
        QSaveFile output(QDir(outputDirectory).filePath(QStringLiteral("01_project.json")));
        if (!output.open(QIODevice::WriteOnly)) {
            QMessageBox::critical(this, tr("Cannot save project snapshot"), output.errorString());
            return;
        }
        const QByteArray json = QJsonDocument(snapshot).toJson(QJsonDocument::Indented);
        if (output.write(json) != json.size() || !output.commit()) {
            QMessageBox::critical(this, tr("Cannot save project snapshot"),
                tr("Writing the project snapshot failed: %1").arg(output.errorString()));
            return;
        }
        confirmationStatus->setText(tr("Confirmed. Debug snapshot saved: C:/Temp/trdos/135/01_project.json"));
        appendLog(tr("Project tab confirmed; snapshot saved to C:/Temp/trdos/135/01_project.json."));
#else
        confirmationStatus->setText(tr("Project data confirmed."));
        appendLog(tr("Project tab confirmed (Release build; debug snapshot is disabled)."));
#endif
        tabs_->setCurrentIndex(1);
    });

    connect(projectName_, &QLineEdit::textChanged, this, [this] { updateProjectTitle(); saveProjectSettings(); });
    connect(customer, &QLineEdit::textChanged, this, [this] { saveProjectSettings(); });
    connect(projectNumber, &QLineEdit::textChanged, this, [this] { saveProjectSettings(); });
    connect(engineer, &QLineEdit::textChanged, this, [this] { saveProjectSettings(); });
    connect(projectDate, &QLineEdit::textChanged, this, [this] { saveProjectSettings(); });
    connect(revision, &QLineEdit::textChanged, this, [this] { saveProjectSettings(); });
    connect(ambient, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double value) {
        if (loadingProjectSettings_) return;
        saveProjectSettings();
        QSettings settings;
        settings.beginGroup(QStringLiteral("projects/") + projectId_ + QStringLiteral("/filter"));
        const bool manuallyOverridden =
            settings.value(QStringLiteral("thermalAmbientSource")).toString() == QStringLiteral("MANUAL");
        if (!manuallyOverridden && ambientTemperature_) {
            {
                const QSignalBlocker blocker(ambientTemperature_);
                ambientTemperature_->setValue(value);
            }
            settings.setValue(QStringLiteral("thermalAmbientC"), value);
            settings.setValue(QStringLiteral("thermalAmbientSource"), QStringLiteral("INHERITED_FROM_PROJECT"));
        }
        settings.endGroup();
        if (!manuallyOverridden) saveProjectSettings();
    });
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
    auto* tuningPreview = new QLabel;
    tuningPreview->setObjectName(QStringLiteral("filterTuningPreview"));
    tuningPreview->setStyleSheet(QStringLiteral("color: #26734d; font-weight: 600;"));
    tuningPreview->setToolTip(tr("For a series reactor, p = 1 / h² × 100%. The tuning frequency is h × f₁."));
    const auto updateTuningPreview = [this, tuningPreview] {
        const double order = tuningHarmonicOrder_->value();
        const double frequency = order * fundamentalFrequency_->value();
        const double detuningPercent = 100.0 / (order * order);
        tuningPreview->setText(tr("f_res = %1 Hz  (p = %2%)")
            .arg(frequency, 0, 'f', 2)
            .arg(detuningPercent, 0, 'f', 2));
    };
    connect(tuningHarmonicOrder_, qOverload<double>(&QDoubleSpinBox::valueChanged),
        this, [updateTuningPreview](double) { updateTuningPreview(); });
    connect(fundamentalFrequency_, qOverload<double>(&QDoubleSpinBox::valueChanged),
        this, [updateTuningPreview](double) { updateTuningPreview(); });
    updateTuningPreview();
    form->addRow(tr("Calculated tuning frequency / detuning"), tuningPreview);
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
    ambientTemperature_->setObjectName(QStringLiteral("filterThermalAmbientTemperatureC"));
    capacitorComponent_->setObjectName(QStringLiteral("filterCapacitorRevision"));
    capacitorThermalResistance_->setObjectName(QStringLiteral("filterCapacitorToAmbientThermalResistanceKPerW"));
    capacitorThermalSource_->setObjectName(QStringLiteral("filterCapacitorThermalEvidence"));
    resistorComponent_->setObjectName(QStringLiteral("filterDampingResistorRevision"));
    resistorThermalResistance_->setObjectName(QStringLiteral("filterResistorToAmbientThermalResistanceKPerW"));
    resistorThermalSource_->setObjectName(QStringLiteral("filterResistorThermalEvidence"));
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
    connect(ambientTemperature_, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
        [this](double) {
            if (loadingProjectSettings_ || projectId_.isEmpty()) return;
            QSettings settings;
            settings.beginGroup(QStringLiteral("projects/") + projectId_ + QStringLiteral("/filter"));
            settings.setValue(QStringLiteral("thermalAmbientSource"), QStringLiteral("MANUAL"));
            settings.endGroup();
            saveProjectSettings();
        });
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
    sourceResistance_->setObjectName(QStringLiteral("filterSourceTheveninResistanceOhm"));
    sourceInductance_->setObjectName(QStringLiteral("filterSourceTheveninInductanceMh"));
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
    inductorSaturationCurrent_->setObjectName(QStringLiteral("filterInductorSaturationCurrentAPeak"));
    inductorSaturationSource_->setObjectName(QStringLiteral("filterInductorSaturationEvidence"));
    coreEffectiveAreaMm2_->setObjectName(QStringLiteral("coreEffectiveAreaMm2"));
    coreMeanPathMm_->setObjectName(QStringLiteral("coreMeanMagneticPathMm"));
    coreAirGapMm_->setObjectName(QStringLiteral("coreTotalAirGapPerLimbMm"));
    coreGapEffectiveAreaMm2_->setObjectName(QStringLiteral("coreEffectiveGapAreaMm2"));
    coreYokeEffectiveAreaMm2_->setObjectName(QStringLiteral("coreYokeEffectiveAreaMm2"));
    coreYokeSegmentPathMm_->setObjectName(QStringLiteral("coreYokeSegmentPathMm"));
    inductorTurns_->setObjectName(QStringLiteral("filterInductorTurnsPerPhase"));
    coreBhSource_->setObjectName(QStringLiteral("coreBhCurveEvidence"));
    sourceForm->addRow(tr("Core effective area Ae"), coreEffectiveAreaMm2_);
    sourceForm->addRow(tr("Mean magnetic path le"), coreMeanPathMm_);
    sourceForm->addRow(tr("Total air gap per limb"), coreAirGapMm_);
    sourceForm->addRow(tr("Effective gap area Ag,eff (optional)"), coreGapEffectiveAreaMm2_);
    sourceForm->addRow(tr("Yoke effective area Ay"), coreYokeEffectiveAreaMm2_);
    sourceForm->addRow(tr("Yoke segment path ly"), coreYokeSegmentPathMm_);
    sourceForm->addRow(tr("Inductor turns per phase N"), inductorTurns_);
    sourceForm->addRow(tr("B-H curve source"), coreBhSource_);
    advancedLayout->addLayout(sourceForm);

    auto* branchGeometryToggle = new QToolButton;
    branchGeometryToggle->setObjectName(QStringLiteral("toggleAsymmetricGeometryOverrides"));
    branchGeometryToggle->setText(tr("Advanced: Asymmetric limb & yoke overrides"));
    branchGeometryToggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    branchGeometryToggle->setArrowType(Qt::RightArrow);
    branchGeometryToggle->setCheckable(true);
    branchGeometryToggle->setChecked(false);
    advancedLayout->addWidget(branchGeometryToggle);
    auto* branchGeometryGroup = new QGroupBox(tr("Independent three-limb geometry overrides"));
    branchGeometryGroup->setObjectName(QStringLiteral("asymmetricGeometryOverridesPanel"));
    branchGeometryGroup->setVisible(false);
    auto* branchGeometryForm = new QFormLayout(branchGeometryGroup);
    const auto addGeometryOverride = [branchGeometryForm](
        const QString& label, const QString& key, QCheckBox*& enabled, QDoubleSpinBox*& value,
        const QString& suffix, int decimals, double maximum) {
        auto* row = new QWidget;
        auto* layout = new QHBoxLayout(row);
        layout->setContentsMargins(0, 0, 0, 0);
        enabled = new QCheckBox(QObject::tr("Override"));
        enabled->setObjectName(key + QStringLiteral("OverrideEnabled"));
        value = new QDoubleSpinBox;
        value->setObjectName(key + QStringLiteral("OverrideValue"));
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
        const QString limbKey = QStringLiteral("coreLimb%1").arg(QChar(static_cast<ushort>('A' + i)));
        addGeometryOverride(limbNames[i] + tr(" — effective area"), limbKey + QStringLiteral("EffectiveAreaMm2"),
            limbAreaOverrideEnabled_[i], limbAreaOverrides_[i], tr(" mm²"), 3, 10000000.0);
        addGeometryOverride(limbNames[i] + tr(" — mean path"), limbKey + QStringLiteral("MeanPathMm"),
            limbPathOverrideEnabled_[i], limbPathOverrides_[i], tr(" mm"), 2, 10000000.0);
        addGeometryOverride(limbNames[i] + tr(" — air gap"), limbKey + QStringLiteral("AirGapMm"),
            limbGapOverrideEnabled_[i], limbGapOverrides_[i], tr(" mm"), 3, 1000000.0);
        addGeometryOverride(limbNames[i] + tr(" — effective gap area"), limbKey + QStringLiteral("EffectiveGapAreaMm2"),
            limbGapAreaOverrideEnabled_[i], limbGapAreaOverrides_[i], tr(" mm²"), 3, 10000000.0);
    }
    const std::array<QString, 4> yokeNames{
        tr("Top A-B"), tr("Top B-C"), tr("Bottom A-B"), tr("Bottom B-C")
    };
    const std::array<QString, 4> yokeKeys{QStringLiteral("TopAB"), QStringLiteral("TopBC"), QStringLiteral("BottomAB"), QStringLiteral("BottomBC")};
    for (std::size_t i = 0; i < yokeNames.size(); ++i) {
        const QString yokeKey = QStringLiteral("coreYoke%1").arg(yokeKeys[i]);
        addGeometryOverride(yokeNames[i] + tr(" — effective area"), yokeKey + QStringLiteral("EffectiveAreaMm2"),
            yokeAreaOverrideEnabled_[i], yokeAreaOverrides_[i], tr(" mm²"), 3, 10000000.0);
        addGeometryOverride(yokeNames[i] + tr(" — segment path"), yokeKey + QStringLiteral("SegmentPathMm"),
            yokePathOverrideEnabled_[i], yokePathOverrides_[i], tr(" mm"), 2, 10000000.0);
    }
    branchGeometryGroup->setToolTip(tr(
        "Unchecked fields inherit the corresponding legacy symmetric geometry. "
        "Checked fields override one branch; an explicitly entered zero air gap is valid."));
    advancedLayout->addWidget(branchGeometryGroup);
    connect(branchGeometryToggle, &QToolButton::toggled, this,
        [branchGeometryToggle, branchGeometryGroup](bool expanded) {
            branchGeometryToggle->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
            branchGeometryGroup->setVisible(expanded);
        });

    auto* dcBiasGroup = new QGroupBox(tr("Per-phase DC current bias"));
    auto* dcBiasForm = new QFormLayout(dcBiasGroup);
    const std::array<QString, 3> dcPhaseNames{tr("Phase A"), tr("Phase B"), tr("Phase C")};
    for (std::size_t i = 0; i < phaseDcBiasCurrentA_.size(); ++i) {
        phaseDcBiasCurrentA_[i] = new QDoubleSpinBox;
        phaseDcBiasCurrentA_[i]->setObjectName(QStringLiteral("corePhase%1DcBiasCurrentA").arg(QChar(static_cast<ushort>('A' + i))));
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
    coreBhCurve_->setObjectName(QStringLiteral("coreBhCurvePoints"));
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
    harmonicSpectrumTable_->setObjectName(QStringLiteral("filterVoltageHarmonics"));
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
    harmonicCurrentTable_->setObjectName(QStringLiteral("filterLoadCurrentHarmonics"));
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

    // Stable identifiers for the principal filter-definition inputs and editable tables.
    nominalVoltage_->setObjectName(QStringLiteral("filterNominalVoltageV"));
    fundamentalFrequency_->setObjectName(QStringLiteral("filterFundamentalFrequencyHz"));
    filterConnection_->setObjectName(QStringLiteral("filterConnection"));
    targetReactivePower_->setObjectName(QStringLiteral("filterTargetReactivePowerKvar"));
    tuningHarmonicOrder_->setObjectName(QStringLiteral("filterTuningHarmonicOrder"));
    qualityFactor_->setObjectName(QStringLiteral("filterQualityFactor"));
    harmonicSpectrumTable_->setObjectName(QStringLiteral("filterVoltageHarmonics"));
    harmonicCurrentTable_->setObjectName(QStringLiteral("filterLoadCurrentHarmonics"));

    auto* calculate = new QPushButton(tr("Synthesize LC branch"));
    calculate->setObjectName(QStringLiteral("synthesizeLcBranch"));
    connect(calculate, &QPushButton::clicked, this, [this] {
        calculateFilter();
        // Synthesis is not confirmation; only the explicit confirmation control advances.
    });
    layout->addWidget(calculate);

    auto* confirmationRow = new QHBoxLayout;
    auto* confirmationStatus = new QLabel(tr("Filter definition has not been confirmed."));
    confirmationStatus->setObjectName(QStringLiteral("filterDefinitionConfirmationStatus"));
    confirmationStatus->setWordWrap(true);
    auto* confirmFilter = new QPushButton(tr("Confirm filter definition and continue"));
    confirmFilter->setObjectName(QStringLiteral("confirmFilterDefinitionAndContinue"));
    confirmationRow->addWidget(confirmationStatus, 1);
    confirmationRow->addWidget(confirmFilter);
    layout->addLayout(confirmationRow);

    connect(confirmFilter, &QPushButton::clicked, this,
        [this, page, confirmationStatus] {
        const auto validPositive = [](double value) {
            return std::isfinite(value) && value > 0.0;
        };
        if (!validPositive(nominalVoltage_->value())) {
            QMessageBox::warning(this, tr("Invalid nominal voltage"),
                tr("Enter a nominal line-to-line voltage greater than zero."));
            nominalVoltage_->setFocus();
            return;
        }
        if (!validPositive(fundamentalFrequency_->value())) {
            QMessageBox::warning(this, tr("Invalid fundamental frequency"),
                tr("Enter a fundamental frequency greater than zero."));
            fundamentalFrequency_->setFocus();
            return;
        }
        if (!validPositive(targetReactivePower_->value())) {
            QMessageBox::warning(this, tr("Invalid target reactive power"),
                tr("Enter a target reactive power greater than zero."));
            targetReactivePower_->setFocus();
            return;
        }
        if (!validPositive(tuningHarmonicOrder_->value()) ||
            tuningHarmonicOrder_->value() < 2.0) {
            QMessageBox::warning(this, tr("Invalid tuning order"),
                tr("Enter a tuning harmonic order of at least 2."));
            tuningHarmonicOrder_->setFocus();
            return;
        }
        if (!validPositive(qualityFactor_->value())) {
            QMessageBox::warning(this, tr("Invalid quality factor"),
                tr("Enter a series-branch quality factor greater than zero."));
            qualityFactor_->setFocus();
            return;
        }
        if (filterConnection_->currentIndex() < 0) {
            QMessageBox::warning(this, tr("Missing branch connection"),
                tr("Select Wye or Delta connection."));
            filterConnection_->setFocus();
            return;
        }

        QJsonObject fields;
        QJsonObject fieldLabels;
        QJsonObject fieldUnits;
        QHash<QString, QString> labels{
            {QStringLiteral("filterNominalVoltageV"), tr("Nominal line voltage")},
            {QStringLiteral("filterFundamentalFrequencyHz"), tr("Fundamental frequency")},
            {QStringLiteral("filterConnection"), tr("Branch connection")},
            {QStringLiteral("filterTargetReactivePowerKvar"), tr("Target reactive power")},
            {QStringLiteral("filterTuningHarmonicOrder"), tr("Tuning harmonic order")},
            {QStringLiteral("filterQualityFactor"), tr("Series-branch quality factor")},
            {QStringLiteral("filterThermalAmbientTemperatureC"), tr("Thermal ambient temperature")},
            {QStringLiteral("filterCapacitorRevision"), tr("Filter capacitor revision")},
            {QStringLiteral("filterCapacitorToAmbientThermalResistanceKPerW"), tr("Capacitor-to-ambient thermal resistance")},
            {QStringLiteral("filterCapacitorThermalEvidence"), tr("Capacitor thermal-resistance evidence")},
            {QStringLiteral("filterDampingResistorRevision"), tr("Damping resistor revision")},
            {QStringLiteral("filterResistorToAmbientThermalResistanceKPerW"), tr("Resistor-to-ambient thermal resistance")},
            {QStringLiteral("filterResistorThermalEvidence"), tr("Resistor thermal-resistance evidence")},
            {QStringLiteral("filterSourceTheveninResistanceOhm"), tr("Thevenin source resistance")},
            {QStringLiteral("filterSourceTheveninInductanceMh"), tr("Thevenin source inductance")},
            {QStringLiteral("filterInductorSaturationCurrentAPeak"), tr("Inductor saturation-current reference")},
            {QStringLiteral("filterInductorSaturationEvidence"), tr("Inductor saturation-current evidence")},
            {QStringLiteral("coreEffectiveAreaMm2"), tr("Core effective area Ae")},
            {QStringLiteral("coreMeanMagneticPathMm"), tr("Mean magnetic path le")},
            {QStringLiteral("coreTotalAirGapPerLimbMm"), tr("Total air gap per limb")},
            {QStringLiteral("coreEffectiveGapAreaMm2"), tr("Effective gap area Ag,eff")},
            {QStringLiteral("coreYokeEffectiveAreaMm2"), tr("Yoke effective area Ay")},
            {QStringLiteral("coreYokeSegmentPathMm"), tr("Yoke segment path ly")},
            {QStringLiteral("filterInductorTurnsPerPhase"), tr("Inductor turns per phase")},
            {QStringLiteral("coreBhCurveEvidence"), tr("B-H curve source/evidence")},
            {QStringLiteral("coreBhCurvePoints"), tr("Core material B-H curve points")},
            {QStringLiteral("filterVoltageHarmonics"), tr("Voltage-source harmonic spectrum")},
            {QStringLiteral("filterLoadCurrentHarmonics"), tr("Load-current harmonic injections")}
        };
        QHash<QString, QString> units{
            {QStringLiteral("filterNominalVoltageV"), QStringLiteral("V line-to-line")},
            {QStringLiteral("filterFundamentalFrequencyHz"), QStringLiteral("Hz")},
            {QStringLiteral("filterConnection"), QStringLiteral("enumerated value")},
            {QStringLiteral("filterTargetReactivePowerKvar"), QStringLiteral("kvar total")},
            {QStringLiteral("filterCapacitorRevision"), QStringLiteral("revision identifier")},
            {QStringLiteral("filterCapacitorThermalEvidence"), QStringLiteral("text reference")},
            {QStringLiteral("filterDampingResistorRevision"), QStringLiteral("manufacturer / model revision")},
            {QStringLiteral("filterResistorThermalEvidence"), QStringLiteral("text reference")},
            {QStringLiteral("filterInductorSaturationEvidence"), QStringLiteral("text reference")},
            {QStringLiteral("coreBhCurveEvidence"), QStringLiteral("text reference")},
            {QStringLiteral("filterTuningHarmonicOrder"), QStringLiteral("harmonic order")},
            {QStringLiteral("filterQualityFactor"), QStringLiteral("dimensionless")},
            {QStringLiteral("filterThermalAmbientTemperatureC"), QStringLiteral("°C")},
            {QStringLiteral("filterCapacitorToAmbientThermalResistanceKPerW"), QStringLiteral("K/W")},
            {QStringLiteral("filterResistorToAmbientThermalResistanceKPerW"), QStringLiteral("K/W")},
            {QStringLiteral("filterSourceTheveninResistanceOhm"), QStringLiteral("Ω")},
            {QStringLiteral("filterSourceTheveninInductanceMh"), QStringLiteral("mH")},
            {QStringLiteral("filterInductorSaturationCurrentAPeak"), QStringLiteral("A peak per phase")},
            {QStringLiteral("coreEffectiveAreaMm2"), QStringLiteral("mm²")},
            {QStringLiteral("coreMeanMagneticPathMm"), QStringLiteral("mm")},
            {QStringLiteral("coreTotalAirGapPerLimbMm"), QStringLiteral("mm")},
            {QStringLiteral("coreEffectiveGapAreaMm2"), QStringLiteral("mm²")},
            {QStringLiteral("coreYokeEffectiveAreaMm2"), QStringLiteral("mm²")},
            {QStringLiteral("coreYokeSegmentPathMm"), QStringLiteral("mm")},
            {QStringLiteral("filterInductorTurnsPerPhase"), QStringLiteral("turns")},
            {QStringLiteral("corePhaseADcBiasCurrentA"), QStringLiteral("A DC")},
            {QStringLiteral("corePhaseBDcBiasCurrentA"), QStringLiteral("A DC")},
            {QStringLiteral("corePhaseCDcBiasCurrentA"), QStringLiteral("A DC")},
            {QStringLiteral("coreBhCurvePoints"), QStringLiteral("H: A/m; B: T")},
            {QStringLiteral("filterVoltageHarmonics"), QStringLiteral("order; sequence; V RMS; degrees")},
            {QStringLiteral("filterLoadCurrentHarmonics"), QStringLiteral("source ID; order; sequence; A RMS; degrees")}
        };
        const auto addOverrideMetadata = [&labels, &units](const QString& key,
                                                            const QString& label,
                                                            const QString& unit) {
            labels.insert(key + QStringLiteral("OverrideEnabled"), tr("%1 — override enabled").arg(label));
            labels.insert(key + QStringLiteral("OverrideValue"), tr("%1 — override value").arg(label));
            units.insert(key + QStringLiteral("OverrideEnabled"), QStringLiteral("boolean"));
            units.insert(key + QStringLiteral("OverrideValue"), unit);
        };
        const std::array<QString, 3> limbMetadataNames{QStringLiteral("A"), QStringLiteral("B"), QStringLiteral("C")};
        for (const auto& phase : limbMetadataNames) {
            const QString prefix = QStringLiteral("coreLimb%1").arg(phase);
            addOverrideMetadata(prefix + QStringLiteral("EffectiveAreaMm2"), tr("Limb %1 effective area").arg(phase), QStringLiteral("mm²"));
            addOverrideMetadata(prefix + QStringLiteral("MeanPathMm"), tr("Limb %1 mean magnetic path").arg(phase), QStringLiteral("mm"));
            addOverrideMetadata(prefix + QStringLiteral("AirGapMm"), tr("Limb %1 air gap").arg(phase), QStringLiteral("mm"));
            addOverrideMetadata(prefix + QStringLiteral("EffectiveGapAreaMm2"), tr("Limb %1 effective gap area").arg(phase), QStringLiteral("mm²"));
        }
        const std::array<QString, 4> yokeMetadataNames{QStringLiteral("TopAB"), QStringLiteral("TopBC"), QStringLiteral("BottomAB"), QStringLiteral("BottomBC")};
        for (const auto& segment : yokeMetadataNames) {
            const QString prefix = QStringLiteral("coreYoke%1").arg(segment);
            addOverrideMetadata(prefix + QStringLiteral("EffectiveAreaMm2"), tr("Yoke %1 effective area").arg(segment), QStringLiteral("mm²"));
            addOverrideMetadata(prefix + QStringLiteral("SegmentPathMm"), tr("Yoke %1 segment path").arg(segment), QStringLiteral("mm"));
        }
        for (const auto& phase : limbMetadataNames)
            labels.insert(QStringLiteral("corePhase%1DcBiasCurrentA").arg(phase),
                tr("Phase %1 DC bias current").arg(phase));
        for (const QString& key : {QStringLiteral("filterConnection"),
                                   QStringLiteral("filterCapacitorRevision"),
                                   QStringLiteral("filterDampingResistorRevision"),
                                   QStringLiteral("coreBhCurveEvidence"),
                                   QStringLiteral("filterInductorSaturationEvidence")})
            units.insert(key, QStringLiteral("text / selection"));
        int unnamedInputCount = 0;
        QStringList unnamedInputTypes;
        for (QWidget* widget : page->findChildren<QWidget*>()) {
            const bool isInput = qobject_cast<QDoubleSpinBox*>(widget) ||
                qobject_cast<QSpinBox*>(widget) || qobject_cast<QComboBox*>(widget) ||
                qobject_cast<QLineEdit*>(widget) || qobject_cast<QCheckBox*>(widget) ||
                qobject_cast<QPlainTextEdit*>(widget) || qobject_cast<QTableWidget*>(widget);
            if (isInput && widget->objectName().isEmpty()) {
                ++unnamedInputCount;
                unnamedInputTypes.push_back(QString::fromLatin1(widget->metaObject()->className()));
            }
        }
        if (unnamedInputCount != 0) {
            QMessageBox::critical(this, tr("Filter fields are not identified"),
                tr("%1 input controls have no stable field identifier. Confirmation was stopped to avoid losing or mislabelling data. Types: %2")
                    .arg(unnamedInputCount).arg(unnamedInputTypes.join(QStringLiteral(", "))));
            return;
        }
        for (QWidget* widget : page->findChildren<QWidget*>()) {
            QString key = widget->objectName();
            // Ignore Qt's private editor children; they are not independent inputs.
            if (key.startsWith(QStringLiteral("qt_"))) continue;
            bool captured = false;
            if (const auto* line = qobject_cast<const QLineEdit*>(widget)) {
                fields.insert(key, line->text());
                captured = true;
            } else if (const auto* combo = qobject_cast<const QComboBox*>(widget)) {
                QJsonObject value;
                value.insert(QStringLiteral("text"), combo->currentText());
                value.insert(QStringLiteral("index"), combo->currentIndex());
                value.insert(QStringLiteral("data"), QJsonValue::fromVariant(combo->currentData()));
                fields.insert(key, value);
                captured = true;
            } else if (const auto* number = qobject_cast<const QDoubleSpinBox*>(widget)) {
                fields.insert(key, number->value());
                captured = true;
            } else if (const auto* integer = qobject_cast<const QSpinBox*>(widget)) {
                fields.insert(key, integer->value());
                captured = true;
            } else if (const auto* check = qobject_cast<const QCheckBox*>(widget)) {
                fields.insert(key, check->isChecked());
                captured = true;
            } else if (const auto* textEdit = qobject_cast<const QPlainTextEdit*>(widget)) {
                fields.insert(key, textEdit->toPlainText());
                captured = true;
            } else if (const auto* table = qobject_cast<const QTableWidget*>(widget)) {
                QJsonArray rows;
                for (int row = 0; row < table->rowCount(); ++row) {
                    QJsonArray cells;
                    for (int column = 0; column < table->columnCount(); ++column) {
                        const auto* item = table->item(row, column);
                        cells.append(item ? QJsonValue(item->text()) : QJsonValue(QJsonValue::Null));
                    }
                    rows.append(cells);
                }
                fields.insert(key, rows);
                captured = true;
            }
            if (captured) {
                if (!labels.contains(key) || !units.contains(key)) {
                    QMessageBox::critical(this, tr("Filter field metadata is incomplete"),
                        tr("The field '%1' has no engineering label or unit metadata. Confirmation was stopped.")
                            .arg(key));
                    return;
                }
                fieldLabels.insert(key, labels.value(key));
                fieldUnits.insert(key, units.value(key));
            }
        }

        QJsonObject snapshot;
        snapshot.insert(QStringLiteral("schemaVersion"), 1);
        snapshot.insert(QStringLiteral("tabId"), QStringLiteral("filter_definition"));
        snapshot.insert(QStringLiteral("tabTitle"), QStringLiteral("Filter definition"));
        snapshot.insert(QStringLiteral("projectId"), projectId_);
        snapshot.insert(QStringLiteral("confirmedAtUtc"),
            QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
        snapshot.insert(QStringLiteral("buildConfiguration"),
#ifdef _DEBUG
            QStringLiteral("Debug")
#else
            QStringLiteral("Release")
#endif
        );
        snapshot.insert(QStringLiteral("fields"), fields);
        snapshot.insert(QStringLiteral("fieldLabels"), fieldLabels);
        snapshot.insert(QStringLiteral("fieldUnits"), fieldUnits);
        QJsonObject inputAvailability;
        inputAvailability.insert(QStringLiteral("voltageHarmonicSpectrum"),
            harmonicSpectrumTable_->rowCount() == 0 ? QStringLiteral("NOT_PROVIDED") : QStringLiteral("PROVIDED"));
        inputAvailability.insert(QStringLiteral("loadCurrentHarmonicSpectrum"),
            harmonicCurrentTable_->rowCount() == 0 ? QStringLiteral("NOT_PROVIDED") : QStringLiteral("PROVIDED"));
        snapshot.insert(QStringLiteral("inputAvailability"), inputAvailability);
        QJsonObject lineage;
        QSettings lineageSettings;
        lineageSettings.beginGroup(QStringLiteral("projects/") + projectId_ + QStringLiteral("/filter"));
        const bool manualAmbient = lineageSettings.value(QStringLiteral("thermalAmbientSource")).toString() == QStringLiteral("MANUAL");
        lineage.insert(QStringLiteral("thermalAmbientTemperatureSource"), manualAmbient
            ? QStringLiteral("USER_OVERRIDE")
            : QStringLiteral("INHERITED_FROM_PROJECT: Project.ambientC"));
        lineageSettings.endGroup();
        snapshot.insert(QStringLiteral("lineage"), lineage);

#ifdef _DEBUG
        const QString outputDirectory = QStringLiteral("C:/Temp/trdos/135");
        if (!QDir().mkpath(outputDirectory)) {
            QMessageBox::critical(this, tr("Cannot save filter snapshot"),
                tr("Cannot create the project output directory: %1").arg(outputDirectory));
            return;
        }
        QSaveFile output(QDir(outputDirectory).filePath(QStringLiteral("02_filter_definition.json")));
        if (!output.open(QIODevice::WriteOnly)) {
            QMessageBox::critical(this, tr("Cannot save filter snapshot"), output.errorString());
            return;
        }
        const QByteArray json = QJsonDocument(snapshot).toJson(QJsonDocument::Indented);
        if (output.write(json) != json.size() || !output.commit()) {
            QMessageBox::critical(this, tr("Cannot save filter snapshot"),
                tr("Writing the filter snapshot failed: %1").arg(output.errorString()));
            return;
        }
        confirmationStatus->setText(tr("Confirmed. Debug snapshot saved: C:/Temp/trdos/135/02_filter_definition.json"));
        appendLog(tr("Filter definition tab confirmed; snapshot saved to C:/Temp/trdos/135/02_filter_definition.json."));
#else
        confirmationStatus->setText(tr("Filter definition confirmed."));
        appendLog(tr("Filter definition tab confirmed (Release build; debug snapshot is disabled)."));
#endif
        tabs_->setCurrentIndex(2);
    });

    layout->addStretch();
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setWidget(page);
    return scroll;
}

QWidget* MainWindow::createConstructionPage()
{
    auto* page = new QWidget;
    auto* root = new QVBoxLayout(page);
    root->setSpacing(10);

    auto* heading = new QLabel(tr("Reactor construction inputs"));
    heading->setStyleSheet("font-size: 20px; font-weight: 600;");
    root->addWidget(heading);
    auto* intro = new QLabel(tr(
        "This page is populated from the ideal design calculation using exact revisions in the material database. "
        "A value is marked AUTO only when the available electrical and material data support it. "
        "Calculated values can then be overridden; missing geometry, ratings or thermal data remain NOT_PROVIDED "
        "instead of being replaced with guessed defaults."));
    intro->setWordWrap(true);
    root->addWidget(intro);

    auto* coreGroup = new QGroupBox(tr("1. Magnetic core"));
    auto* coreForm = new QFormLayout(coreGroup);
    auto* coreMaterial = new QComboBox;
    coreMaterial->setObjectName(QStringLiteral("construction_coreMaterial"));
    auto* coreGeometry = new QComboBox;
    coreGeometry->setObjectName(QStringLiteral("construction_coreGeometry"));
    coreGeometry->addItem(tr("— NOT_PROVIDED —"), QString{});
    coreGeometry->addItem(tr("Three-limb core"), QStringLiteral("three-limb"));
    coreGeometry->addItem(tr("Three single-phase cores"), QStringLiteral("three-single-phase"));
    coreGeometry->addItem(tr("EI core"), QStringLiteral("ei"));
    coreGeometry->addItem(tr("UI core"), QStringLiteral("ui"));
    coreGeometry->addItem(tr("Toroidal"), QStringLiteral("toroidal"));
    coreGeometry->addItem(tr("Strip-wound"), QStringLiteral("strip-wound"));
    coreGeometry->addItem(tr("Custom"), QStringLiteral("custom"));
    auto* stackingFactor = new QDoubleSpinBox;
    stackingFactor->setObjectName(QStringLiteral("construction_stackingFactor"));
    stackingFactor->setRange(0.0, 1.0);
    stackingFactor->setDecimals(4);
    stackingFactor->setSpecialValueText(tr("NOT_PROVIDED"));
    stackingFactor->setValue(0.0);
    coreForm->addRow(tr("Core material revision"), coreMaterial);
    coreForm->addRow(tr("Core geometry family"), coreGeometry);
    coreForm->addRow(tr("Stacking factor"), stackingFactor);

    const auto addCoreLength = [coreForm](const QString& objectName, const QString& label) {
        auto* field = new QDoubleSpinBox;
        field->setObjectName(objectName);
        field->setRange(0.0, 10000.0);
        field->setDecimals(3);
        field->setSuffix(QObject::tr(" mm"));
        field->setSpecialValueText(QObject::tr("NOT_PROVIDED"));
        coreForm->addRow(label, field);
        return field;
    };
    for (int limb = 0; limb < 3; ++limb) {
        const QString suffix = QString::number(limb + 1);
        addCoreLength(QStringLiteral("construction_limbWidth_") + suffix,
            tr("Limb %1 width").arg(QChar('A' + limb)));
        addCoreLength(QStringLiteral("construction_limbDepth_") + suffix,
            tr("Limb %1 stack depth").arg(QChar('A' + limb)));
    }
    for (int window = 0; window < 2; ++window) {
        const QString suffix = QString::number(window + 1);
        addCoreLength(QStringLiteral("construction_windowWidth_") + suffix,
            tr("Window %1 clear width").arg(window + 1));
        addCoreLength(QStringLiteral("construction_windowHeight_") + suffix,
            tr("Window %1 clear height").arg(window + 1));
    }
    for (int yoke = 0; yoke < 4; ++yoke) {
        const QString suffix = QString::number(yoke + 1);
        addCoreLength(QStringLiteral("construction_yokeThickness_") + suffix,
            tr("Yoke segment %1 thickness").arg(yoke + 1));
        addCoreLength(QStringLiteral("construction_yokeDepth_") + suffix,
            tr("Yoke segment %1 stack depth").arg(yoke + 1));
    }
    for (int spacing = 0; spacing < 3; ++spacing) {
        const QString suffix = QString::number(spacing + 1);
        const QString pair = spacing == 0 ? tr("A-B") : spacing == 1 ? tr("B-C") : tr("A-C");
        addCoreLength(QStringLiteral("construction_limbCenterSpacing_") + suffix,
            tr("Limb center spacing %1").arg(pair));
    }
    addCoreLength(QStringLiteral("construction_coreOverallHeight"), tr("Overall core height"));
    addCoreLength(QStringLiteral("construction_coreOverallWidth"), tr("Overall core width"));

    for (int limb = 0; limb < 3; ++limb) {
        const QString limbSuffix = QString::number(limb + 1);
        for (int gap = 0; gap < 3; ++gap) {
            const QString gapSuffix = QString::number(gap + 1);
            addCoreLength(QStringLiteral("construction_airGapPosition_") + limbSuffix +
                QStringLiteral("_") + gapSuffix,
                tr("Limb %1 air gap %2 position from lower end").arg(QChar('A' + limb)).arg(gap + 1));
            addCoreLength(QStringLiteral("construction_airGapLength_") + limbSuffix +
                QStringLiteral("_") + gapSuffix,
                tr("Limb %1 air gap %2 length").arg(QChar('A' + limb)).arg(gap + 1));
        }
    }

    for (int limb = 0; limb < 3; ++limb) {
        auto* count = new QSpinBox;
        count->setObjectName(QStringLiteral("construction_airGapCount_") + QString::number(limb + 1));
        count->setRange(0, 3);
        count->setSpecialValueText(tr("NOT_PROVIDED"));
        coreForm->addRow(tr("Limb %1 discrete air-gap count").arg(QChar('A' + limb)), count);
        addCoreLength(QStringLiteral("construction_totalAirGap_") + QString::number(limb + 1),
            tr("Limb %1 total air-gap length").arg(QChar('A' + limb)));
    }
    const std::array<QString, 4> yokeGapLabels{
        tr("Top yoke A-B"), tr("Top yoke B-C"),
        tr("Bottom yoke A-B"), tr("Bottom yoke B-C")
    };
    for (int yoke = 0; yoke < 4; ++yoke) {
        for (int gap = 0; gap < 3; ++gap) {
            const QString suffix = QString::number(yoke + 1) + QStringLiteral("_") +
                QString::number(gap + 1);
            addCoreLength(QStringLiteral("construction_yokeAirGapPosition_") + suffix,
                tr("%1 air gap %2 position from left end").arg(yokeGapLabels[static_cast<std::size_t>(yoke)]).arg(gap + 1));
            addCoreLength(QStringLiteral("construction_yokeAirGapLength_") + suffix,
                tr("%1 air gap %2 length").arg(yokeGapLabels[static_cast<std::size_t>(yoke)]).arg(gap + 1));
        }
    }
    const std::array<QString, 8> jointLabels{
        tr("Top A-B to limb A"), tr("Top A-B to limb B"),
        tr("Top B-C to limb B"), tr("Top B-C to limb C"),
        tr("Bottom A-B to limb A"), tr("Bottom A-B to limb B"),
        tr("Bottom B-C to limb B"), tr("Bottom B-C to limb C")
    };
    for (std::size_t joint = 0; joint < jointLabels.size(); ++joint) {
        auto* construction = new QComboBox;
        construction->setObjectName(QStringLiteral("construction_coreJoint_") + QString::number(joint + 1));
        construction->addItem(tr("— NOT_PROVIDED —"), QString{});
        construction->addItem(tr("Butt joint"), QStringLiteral("butt"));
        construction->addItem(tr("Step-Lap (packet schedule required; editor not yet available)"),
            QStringLiteral("step-lap"));
        coreForm->addRow(tr("Joint %1 construction").arg(jointLabels[joint]), construction);
    }
    auto* laminationConstruction = new QLineEdit;
    laminationConstruction->setObjectName(QStringLiteral("construction_laminationConstruction"));
    laminationConstruction->setPlaceholderText(tr("Packet / lamination construction description — NOT_PROVIDED"));
    coreForm->addRow(tr("Lamination construction description"), laminationConstruction);
    auto* dimensionalSource = new QLineEdit;
    dimensionalSource->setObjectName(QStringLiteral("construction_dimensionalSource"));
    dimensionalSource->setPlaceholderText(tr("Drawing / datasheet / measurement reference — NOT_PROVIDED"));
    coreForm->addRow(tr("Physical dimensions source"), dimensionalSource);
    auto* dimensionalRevision = new QLineEdit;
    dimensionalRevision->setObjectName(QStringLiteral("construction_dimensionalRevision"));
    dimensionalRevision->setPlaceholderText(tr("Document revision / measurement issue — NOT_PROVIDED"));
    coreForm->addRow(tr("Physical dimensions revision"), dimensionalRevision);
    auto* jointNote = new QLabel(tr(
        "For each Step-Lap interface, enter one packet per line using: "
        "joint number (1–8), stack offset, packet depth, overlap into limb, "
        "overlap into yoke, direction. Lengths are in mm; direction is "
        "limb-into-yoke or yoke-into-limb. Packets must cover the complete stack depth "
        "contiguously and alternate direction. The current OCCT core builder still rejects Step-Lap: packet data is captured and validated, but interleaved solid generation is not yet implemented."));
    jointNote->setWordWrap(true);
    coreForm->addRow(jointNote);
    auto* stepLapSchedule = new QPlainTextEdit;
    stepLapSchedule->setObjectName(QStringLiteral("construction_stepLapPacketSchedule"));
    stepLapSchedule->setPlaceholderText(tr(
        "joint,offset_mm,depth_mm,overlap_limb_mm,overlap_yoke_mm,direction\n"
        "1,...,...,...,...,limb-into-yoke\n"
        "1,...,...,...,...,yoke-into-limb"));
    stepLapSchedule->setMaximumHeight(110);
    coreForm->addRow(tr("Step-Lap packet schedule"), stepLapSchedule);
    root->addWidget(coreGroup);

    auto* windingGroup = new QGroupBox(tr("2. Winding and conductor"));
    auto* windingForm = new QFormLayout(windingGroup);
    auto* conductorMaterial = new QComboBox;
    conductorMaterial->setObjectName(QStringLiteral("construction_conductorMaterial"));
    auto* conductorShape = new QComboBox;
    conductorShape->setObjectName(QStringLiteral("construction_conductorShape"));
    conductorShape->addItem(tr("— NOT_PROVIDED —"), QString{});
    conductorShape->addItem(tr("Round wire"), QStringLiteral("round"));
    conductorShape->addItem(tr("Rectangular wire"), QStringLiteral("rectangular"));
    conductorShape->addItem(tr("Foil"), QStringLiteral("foil"));
    conductorShape->addItem(tr("Litz wire"), QStringLiteral("litz"));
    conductorShape->addItem(tr("Busbar"), QStringLiteral("busbar"));
    auto* windingTechnology = new QComboBox;
    windingTechnology->setObjectName(QStringLiteral("construction_windingTechnology"));
    windingTechnology->addItem(tr("— NOT_PROVIDED —"), QString{});
    windingTechnology->addItem(tr("Layer"), QStringLiteral("layer"));
    windingTechnology->addItem(tr("Sectional"), QStringLiteral("sectional"));
    windingTechnology->addItem(tr("Disc"), QStringLiteral("disc"));
    windingTechnology->addItem(tr("Foil"), QStringLiteral("foil"));
    windingTechnology->addItem(tr("Multilayer"), QStringLiteral("multilayer"));
    windingTechnology->addItem(tr("Litz"), QStringLiteral("litz"));
    auto* turns = new QSpinBox;
    turns->setObjectName(QStringLiteral("construction_turns"));
    turns->setRange(0, 1000000);
    turns->setSpecialValueText(tr("AUTO / NOT CALCULATED"));
    auto* turnsPerLayer = new QSpinBox;
    turnsPerLayer->setObjectName(QStringLiteral("construction_turnsPerLayer"));
    turnsPerLayer->setRange(0, 1000000);
    turnsPerLayer->setSpecialValueText(tr("NOT_PROVIDED"));
    auto* layers = new QSpinBox;
    layers->setObjectName(QStringLiteral("construction_layers"));
    layers->setRange(0, 100000);
    layers->setSpecialValueText(tr("NOT_PROVIDED"));
    auto* sections = new QSpinBox;
    sections->setObjectName(QStringLiteral("construction_sections"));
    sections->setRange(0, 10000);
    sections->setSpecialValueText(tr("NOT_PROVIDED"));
    auto* parallelConductors = new QSpinBox;
    parallelConductors->setObjectName(QStringLiteral("construction_parallelConductors"));
    parallelConductors->setRange(0, 10000);
    parallelConductors->setSpecialValueText(tr("NOT_PROVIDED"));
    parallelConductors->setValue(0);
    auto* conductorArea = new QDoubleSpinBox;
    conductorArea->setObjectName(QStringLiteral("construction_conductorArea"));
    conductorArea->setRange(0.0, 1000000.0);
    conductorArea->setDecimals(4);
    conductorArea->setSuffix(tr(" mm²"));
    conductorArea->setSpecialValueText(tr("NOT_PROVIDED"));
    auto* conductorWidth = new QDoubleSpinBox;
    conductorWidth->setObjectName(QStringLiteral("construction_conductorWidth"));
    conductorWidth->setRange(0.0, 100000.0);
    conductorWidth->setDecimals(4);
    conductorWidth->setSuffix(tr(" mm"));
    conductorWidth->setSpecialValueText(tr("NOT_PROVIDED"));
    auto* conductorThickness = new QDoubleSpinBox;
    conductorThickness->setObjectName(QStringLiteral("construction_conductorThickness"));
    conductorThickness->setRange(0.0, 100000.0);
    conductorThickness->setDecimals(4);
    conductorThickness->setSuffix(tr(" mm"));
    conductorThickness->setSpecialValueText(tr("NOT_PROVIDED"));
    windingForm->addRow(tr("Conductor material revision"), conductorMaterial);
    windingForm->addRow(tr("Conductor cross-section"), conductorShape);
    windingForm->addRow(tr("Winding technology"), windingTechnology);
    windingForm->addRow(tr("Turns per phase"), turns);
    windingForm->addRow(tr("Turns per layer"), turnsPerLayer);
    windingForm->addRow(tr("Number of layers"), layers);
    windingForm->addRow(tr("Number of sections"), sections);
    windingForm->addRow(tr("Parallel conductors"), parallelConductors);
    windingForm->addRow(tr("Bare conductor area"), conductorArea);
    windingForm->addRow(tr("Bare width (rectangular / foil)"), conductorWidth);
    windingForm->addRow(tr("Bare thickness (rectangular / foil)"), conductorThickness);
    const auto makeConductorDimension = [](const QString& name) {
        auto* field = new QDoubleSpinBox;
        field->setObjectName(name);
        field->setRange(0.0, 100000.0);
        field->setDecimals(4);
        field->setSuffix(QObject::tr(" mm"));
        field->setSpecialValueText(QObject::tr("NOT_APPLICABLE"));
        return field;
    };
    auto* bareDiameter = makeConductorDimension(QStringLiteral("construction_conductorBareDiameter"));
    auto* insulatedDiameter = makeConductorDimension(QStringLiteral("construction_conductorInsulatedDiameter"));
    auto* conductorInsulationBuild = makeConductorDimension(QStringLiteral("construction_conductorInsulationBuild"));
    auto* bareDiameterLabel = new QLabel(tr("Bare diameter (round)"));
    auto* insulatedDiameterLabel = new QLabel(tr("Insulated diameter (round)"));
    auto* conductorInsulationLabel = new QLabel(tr("Conductor insulation build"));
    windingForm->addRow(bareDiameterLabel, bareDiameter);
    windingForm->addRow(insulatedDiameterLabel, insulatedDiameter);
    windingForm->addRow(conductorInsulationLabel, conductorInsulationBuild);
    const auto updateConductorApplicability = [conductorShape, bareDiameter, insulatedDiameter,
        conductorInsulationBuild, conductorWidth, conductorThickness,
        bareDiameterLabel, insulatedDiameterLabel, conductorInsulationLabel] {
        const bool round = conductorShape->currentData().toString() == QStringLiteral("round");
        bareDiameter->setVisible(round);
        insulatedDiameter->setVisible(round);
        bareDiameterLabel->setVisible(round);
        insulatedDiameterLabel->setVisible(round);
        conductorInsulationBuild->setVisible(!round);
        conductorInsulationLabel->setVisible(!round);
        conductorWidth->setVisible(!round);
        conductorThickness->setVisible(!round);
    };
    connect(conductorShape, qOverload<int>(&QComboBox::currentIndexChanged), page,
        [updateConductorApplicability](int) { updateConductorApplicability(); });
    updateConductorApplicability();
    auto* layerNote = new QLabel(tr(
        "Turns = turns per layer × layers is applicable only to layer-based winding constructions. "
        "For sectional, disc and foil windings, layer count and turn distribution require their own construction rules."));
    layerNote->setWordWrap(true);
    layerNote->setStyleSheet("color: palette(mid);");
    windingForm->addRow(layerNote);
    root->addWidget(windingGroup);

    auto* insulationGroup = new QGroupBox(tr("3. Insulation and bobbin"));
    auto* insulationForm = new QFormLayout(insulationGroup);
    auto* insulationMaterial = new QComboBox;
    insulationMaterial->setObjectName(QStringLiteral("construction_insulationMaterial"));
    auto* bobbinMaterial = new QLineEdit;
    bobbinMaterial->setObjectName(QStringLiteral("construction_bobbinMaterial"));
    bobbinMaterial->setPlaceholderText(tr("Material ID / grade — NOT_PROVIDED until selected"));
    auto* bobbinUsage = new QComboBox;
    bobbinUsage->setObjectName(QStringLiteral("construction_bobbinUsage"));
    bobbinUsage->addItem(tr("— NOT_PROVIDED —"), QString{});
    bobbinUsage->addItem(tr("Yes"), true);
    bobbinUsage->addItem(tr("No"), false);
    auto* turnInsulation = new QDoubleSpinBox;
    turnInsulation->setObjectName(QStringLiteral("construction_turnInsulation"));
    turnInsulation->setRange(0.0, 10000.0);
    turnInsulation->setDecimals(4);
    turnInsulation->setSuffix(tr(" mm"));
    turnInsulation->setSpecialValueText(tr("NOT_PROVIDED"));
    auto* layerInsulation = new QDoubleSpinBox;
    layerInsulation->setObjectName(QStringLiteral("construction_layerInsulation"));
    layerInsulation->setRange(0.0, 10000.0);
    layerInsulation->setDecimals(4);
    layerInsulation->setSuffix(tr(" mm"));
    layerInsulation->setSpecialValueText(tr("NOT_PROVIDED"));
    auto* mainInsulation = new QDoubleSpinBox;
    mainInsulation->setObjectName(QStringLiteral("construction_mainInsulation"));
    mainInsulation->setRange(0.0, 10000.0);
    mainInsulation->setDecimals(4);
    mainInsulation->setSuffix(tr(" mm"));
    mainInsulation->setSpecialValueText(tr("NOT_PROVIDED"));
    insulationForm->addRow(tr("Insulation material revision"), insulationMaterial);
    insulationForm->addRow(tr("Turn insulation build"), turnInsulation);
    insulationForm->addRow(tr("Layer insulation build"), layerInsulation);
    insulationForm->addRow(tr("Main insulation build"), mainInsulation);
    insulationForm->addRow(tr("Bobbin used"), bobbinUsage);
    insulationForm->addRow(tr("Bobbin material"), bobbinMaterial);
    const auto addBobbinLength = [insulationForm](const QString& objectName, const QString& label) {
        auto* field = new QDoubleSpinBox;
        field->setObjectName(objectName);
        field->setRange(0.0, 10000.0);
        field->setDecimals(3);
        field->setSuffix(QObject::tr(" mm"));
        field->setSpecialValueText(QObject::tr("NOT_PROVIDED"));
        insulationForm->addRow(label, field);
        return field;
    };
    for (int phase = 0; phase < 3; ++phase) {
        const QString suffix = QString::number(phase + 1);
        const QString phaseName = QString(QChar('A' + phase));
        addBobbinLength(QStringLiteral("construction_bobbinInnerWidth_") + suffix,
            tr("Bobbin %1 inner width").arg(phaseName));
        addBobbinLength(QStringLiteral("construction_bobbinInnerHeight_") + suffix,
            tr("Bobbin %1 inner height").arg(phaseName));
        addBobbinLength(QStringLiteral("construction_bobbinAxialLength_") + suffix,
            tr("Bobbin %1 axial length").arg(phaseName));
        addBobbinLength(QStringLiteral("construction_bobbinWallThickness_") + suffix,
            tr("Bobbin %1 wall thickness").arg(phaseName));
        addBobbinLength(QStringLiteral("construction_bobbinFlangeThickness_") + suffix,
            tr("Bobbin %1 flange thickness").arg(phaseName));
        addBobbinLength(QStringLiteral("construction_bobbinFlangeOuterWidth_") + suffix,
            tr("Bobbin %1 flange outer width").arg(phaseName));
        addBobbinLength(QStringLiteral("construction_bobbinFlangeOuterHeight_") + suffix,
            tr("Bobbin %1 flange outer height").arg(phaseName));
        addBobbinLength(QStringLiteral("construction_bobbinWindingClearance_") + suffix,
            tr("Bobbin %1 winding clearance").arg(phaseName));
        addBobbinLength(QStringLiteral("construction_windingRadialBuild_") + suffix,
            tr("Winding %1 radial build").arg(phaseName));
        addBobbinLength(QStringLiteral("construction_windingAxialHeight_") + suffix,
            tr("Winding %1 axial height").arg(phaseName));
        addBobbinLength(QStringLiteral("construction_windingEndClearance_") + suffix,
            tr("Winding %1 end clearance").arg(phaseName));
    }
    root->addWidget(insulationGroup);

    auto* thermalGroup = new QGroupBox(tr("4. Cooling and thermal limits"));
    auto* thermalForm = new QFormLayout(thermalGroup);
    auto* cooling = new QComboBox;
    cooling->setObjectName(QStringLiteral("construction_cooling"));
    cooling->addItem(tr("— NOT_PROVIDED —"), QString{});
    cooling->addItem(tr("Natural air"), QStringLiteral("natural-air"));
    cooling->addItem(tr("Forced air"), QStringLiteral("forced-air"));
    cooling->addItem(tr("Liquid"), QStringLiteral("liquid"));
    auto* ambient = new QDoubleSpinBox;
    ambient->setObjectName(QStringLiteral("construction_ambient"));
    ambient->setRange(-50.0, 200.0);
    ambient->setDecimals(2);
    ambient->setSuffix(tr(" °C"));
    ambient->setSpecialValueText(tr("NOT_PROVIDED"));
    ambient->setValue(-50.0);
    auto* thermalClass = new QComboBox;
    thermalClass->setObjectName(QStringLiteral("construction_insulationThermalClass"));
    thermalClass->addItem(tr("— NOT_PROVIDED —"), QString{});
    thermalClass->addItem(QStringLiteral("A"), QStringLiteral("A"));
    thermalClass->addItem(QStringLiteral("E"), QStringLiteral("E"));
    thermalClass->addItem(QStringLiteral("B"), QStringLiteral("B"));
    thermalClass->addItem(QStringLiteral("F"), QStringLiteral("F"));
    thermalClass->addItem(QStringLiteral("H"), QStringLiteral("H"));
    auto* thermalStandard = new QLineEdit;
    thermalStandard->setObjectName(QStringLiteral("construction_insulationThermalStandard"));
    thermalStandard->setPlaceholderText(tr("Standard reference — NOT_PROVIDED"));
    auto* thermalReferenceTemperature = new QDoubleSpinBox;
    thermalReferenceTemperature->setObjectName(QStringLiteral("construction_insulationClassReferenceTemperature"));
    thermalReferenceTemperature->setRange(-50.0, 300.0);
    thermalReferenceTemperature->setDecimals(2);
    thermalReferenceTemperature->setSuffix(tr(" °C"));
    thermalReferenceTemperature->setSpecialValueText(tr("NOT_PROVIDED"));
    thermalReferenceTemperature->setValue(-50.0);
    auto* thermalSafetyMargin = new QDoubleSpinBox;
    thermalSafetyMargin->setObjectName(QStringLiteral("construction_insulationSafetyMargin"));
    thermalSafetyMargin->setRange(0.0, 200.0);
    thermalSafetyMargin->setDecimals(2);
    thermalSafetyMargin->setSuffix(tr(" K"));
    thermalSafetyMargin->setSpecialValueText(tr("NOT_PROVIDED"));
    auto* maximumWindingTemperature = new QDoubleSpinBox;
    maximumWindingTemperature->setObjectName(QStringLiteral("construction_maximumWindingTemperature"));
    maximumWindingTemperature->setRange(-50.0, 300.0);
    maximumWindingTemperature->setDecimals(2);
    maximumWindingTemperature->setSuffix(tr(" °C"));
    maximumWindingTemperature->setSpecialValueText(tr("NOT_PROVIDED"));
    maximumWindingTemperature->setValue(-50.0);
    thermalForm->addRow(tr("Cooling method"), cooling);
    thermalForm->addRow(tr("Ambient temperature"), ambient);
    thermalForm->addRow(tr("Insulation thermal class reference"), thermalClass);
    thermalForm->addRow(tr("Thermal class standard"), thermalStandard);
    thermalForm->addRow(tr("Thermal class reference temperature"), thermalReferenceTemperature);
    thermalForm->addRow(tr("Thermal design safety margin"), thermalSafetyMargin);
    thermalForm->addRow(tr("Maximum winding temperature (design limit)"), maximumWindingTemperature);

    auto* ductCount = new QSpinBox;
    ductCount->setObjectName(QStringLiteral("construction_coolingDuctCount"));
    ductCount->setRange(0, 3);
    ductCount->setSpecialValueText(tr("NOT_PROVIDED"));
    thermalForm->addRow(tr("Radial cooling duct count"), ductCount);
    for (int duct = 0; duct < 3; ++duct) {
        const QString suffix = QString::number(duct + 1);
        auto* position = new QDoubleSpinBox;
        position->setObjectName(QStringLiteral("construction_coolingDuctPosition_") + suffix);
        position->setRange(0.0, 10000.0);
        position->setDecimals(3);
        position->setSuffix(tr(" mm"));
        position->setSpecialValueText(tr("NOT_PROVIDED"));
        thermalForm->addRow(tr("Cooling duct %1 position from winding bottom").arg(duct + 1), position);

        auto* thickness = new QDoubleSpinBox;
        thickness->setObjectName(QStringLiteral("construction_coolingDuctThickness_") + suffix);
        thickness->setRange(0.0, 1000.0);
        thickness->setDecimals(3);
        thickness->setSuffix(tr(" mm"));
        thickness->setSpecialValueText(tr("NOT_PROVIDED"));
        thermalForm->addRow(tr("Cooling duct %1 thickness").arg(duct + 1), thickness);

        auto* spacerRevision = new QLineEdit;
        spacerRevision->setObjectName(QStringLiteral("construction_coolingDuctSpacerRevision_") + suffix);
        spacerRevision->setPlaceholderText(tr("Exact material revision ID — NOT_PROVIDED"));
        thermalForm->addRow(tr("Cooling duct %1 spacer revision").arg(duct + 1), spacerRevision);

        auto* airflow = new QComboBox;
        airflow->setObjectName(QStringLiteral("construction_coolingDuctAirflow_") + suffix);
        airflow->addItem(tr("— NOT_PROVIDED —"), QString{});
        airflow->addItem(tr("Axial"), QStringLiteral("axial"));
        airflow->addItem(tr("Radial"), QStringLiteral("radial"));
        airflow->addItem(tr("Transverse"), QStringLiteral("transverse"));
        thermalForm->addRow(tr("Cooling duct %1 airflow direction").arg(duct + 1), airflow);
    }
    root->addWidget(thermalGroup);

    auto* catalogStatus = new QLabel;
    catalogStatus->setWordWrap(true);
    auto* refresh = new QPushButton(tr("Refresh material choices"));
    auto* openCatalog = new QPushButton(tr("Open material database"));
    auto* actionRow = new QHBoxLayout;
    actionRow->addWidget(refresh);
    actionRow->addWidget(openCatalog);
    actionRow->addStretch();
    root->addLayout(actionRow);
    root->addWidget(catalogStatus);
    root->addStretch();

    const auto populateMaterial = [this](QComboBox* combo, const std::string& recordType) {
        const QString previousRevision = combo->currentData().toString();
        combo->clear();
        combo->addItem(tr("— NOT_PROVIDED —"), QString{});
        if (!materialStore_) return std::size_t{0};
        p135::infrastructure::sqlite::MaterialCatalogFilter filter;
        filter.recordType = recordType;
        filter.pageSize = 500;
        const auto revisions = materialStore_->searchLatestRevisions(filter);
        if (!revisions) return std::size_t{0};
        for (const auto& revision : revisions.value()) {
            const auto archived = materialStore_->isArchived(revision.materialId);
            if (!archived || archived.value()) continue;
            combo->addItem(QString::fromStdString(revision.materialId) +
                QStringLiteral(" — r%1").arg(revision.revisionNumber),
                QString::fromStdString(revision.revisionId));
        }
        int selected = combo->findData(previousRevision);
        if (!previousRevision.isEmpty() && selected < 0) {
            const auto pinned = materialStore_->findRevision(previousRevision.toStdString());
            if (pinned && pinned.value()) {
                const auto& revision = *pinned.value();
                combo->addItem(QString::fromStdString(revision.materialId) +
                    QStringLiteral(" — r%1 (pinned revision)").arg(revision.revisionNumber),
                    QString::fromStdString(revision.revisionId));
                selected = combo->count() - 1;
            }
        }
        if (selected >= 0) combo->setCurrentIndex(selected);
        return static_cast<std::size_t>(combo->count() - 1);
    };
    const auto refreshChoices = [=] {
        const auto coreCount = populateMaterial(coreMaterial, "core-material");
        const auto conductorCount = populateMaterial(conductorMaterial, "conductor-material");
        const auto insulationCount = populateMaterial(insulationMaterial, "insulation-material");
        catalogStatus->setText(tr("Active material revisions — core: %1, conductor: %2, insulation: %3. "
            "Selections are revision-pinned; they are not yet connected to the physical sizing solver.")
            .arg(coreCount).arg(conductorCount).arg(insulationCount));
    };
    QObject::connect(refresh, &QPushButton::clicked, page, refreshChoices);
    QObject::connect(openCatalog, &QPushButton::clicked, page, [this] {
        if (tabs_) tabs_->setCurrentIndex(5);
    });
    refreshChoices();

    auto* constructionState = new QLabel(tr("AWAITING IDEAL DESIGN CALCULATION — material and physical parameters will be populated from the calculation and database."));
    constructionState->setObjectName(QStringLiteral("constructionInputState"));
    constructionState->setWordWrap(true);
    constructionState->setTextInteractionFlags(Qt::TextSelectableByMouse);
    constructionState->setStyleSheet("color: #a66b00; font-weight: 600;");
    root->addWidget(constructionState);

    // Persist construction inputs and invalidate their capture on every edit.
    for (auto* widget : page->findChildren<QWidget*>()) {
        if (auto* combo = qobject_cast<QComboBox*>(widget)) {
            connect(combo, qOverload<int>(&QComboBox::currentIndexChanged), this,
                    [this](int) { saveProjectSettings(); markConstructionInputsChanged(); });
        } else if (auto* number = qobject_cast<QDoubleSpinBox*>(widget)) {
            connect(number, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
                    [this](double) { saveProjectSettings(); markConstructionInputsChanged(); });
        } else if (auto* integer = qobject_cast<QSpinBox*>(widget)) {
            connect(integer, qOverload<int>(&QSpinBox::valueChanged), this,
                    [this](int) { saveProjectSettings(); markConstructionInputsChanged(); });
        } else if (auto* line = qobject_cast<QLineEdit*>(widget)) {
            connect(line, &QLineEdit::textChanged, this,
                    [this](const QString&) { saveProjectSettings(); markConstructionInputsChanged(); });
        } else if (auto* textEdit = qobject_cast<QPlainTextEdit*>(widget)) {
            connect(textEdit, &QPlainTextEdit::textChanged, this,
                    [this] { saveProjectSettings(); markConstructionInputsChanged(); });
        } else if (auto* check = qobject_cast<QCheckBox*>(widget)) {
            connect(check, &QCheckBox::toggled, this,
                    [this](bool) { saveProjectSettings(); markConstructionInputsChanged(); });
        }
    }

    auto* scope = new QLabel(tr(
        "Material selections are pinned to exact immutable database revisions. The calculation currently selects "
        "compatible material candidates automatically; complete physical sizing is withheld wherever the database "
        "does not contain the required geometry, B-H, thermal or insulation-system evidence."));
    scope->setWordWrap(true);
    scope->setStyleSheet("color: palette(mid);");
    root->addWidget(scope);

    auto* confirmationRow = new QHBoxLayout;
    auto* confirmationStatus = new QLabel(tr("Construction has not been confirmed."));
    confirmationStatus->setObjectName(QStringLiteral("constructionConfirmationStatus"));
    confirmationStatus->setWordWrap(true);
    auto* confirmConstruction = new QPushButton(tr("Confirm construction and continue"));
    confirmConstruction->setObjectName(QStringLiteral("confirmConstructionAndContinue"));
    confirmationRow->addWidget(confirmationStatus, 1);
    confirmationRow->addWidget(confirmConstruction);
    root->addLayout(confirmationRow);

    connect(confirmConstruction, &QPushButton::clicked, this,
        [this, page, confirmationStatus] {
        QJsonObject fields;
        QJsonObject fieldLabels;
        QJsonObject fieldUnits;
        const QHash<QString, QString> labels{
            {QStringLiteral("construction_coreMaterial"), tr("Core material revision")},
            {QStringLiteral("construction_coreGeometry"), tr("Core geometry family")},
            {QStringLiteral("construction_stackingFactor"), tr("Core stacking factor")},
            {QStringLiteral("construction_conductorMaterial"), tr("Conductor material revision")},
            {QStringLiteral("construction_conductorShape"), tr("Conductor cross-section shape")},
            {QStringLiteral("construction_windingTechnology"), tr("Winding technology")},
            {QStringLiteral("construction_turns"), tr("Turns per phase")},
            {QStringLiteral("construction_turnsPerLayer"), tr("Turns per layer")},
            {QStringLiteral("construction_layers"), tr("Winding layers")},
            {QStringLiteral("construction_parallelConductors"), tr("Parallel conductors")},
            {QStringLiteral("construction_conductorArea"), tr("Bare conductor area")},
            {QStringLiteral("construction_conductorWidth"), tr("Bare conductor width")},
            {QStringLiteral("construction_conductorThickness"), tr("Bare conductor thickness")},
            {QStringLiteral("construction_insulationMaterial"), tr("Insulation material revision")},
            {QStringLiteral("construction_bobbinMaterial"), tr("Bobbin material")},
            {QStringLiteral("construction_bobbinUsage"), tr("Bobbin usage")},
            {QStringLiteral("construction_turnInsulation"), tr("Turn insulation build")},
            {QStringLiteral("construction_layerInsulation"), tr("Layer insulation build")},
            {QStringLiteral("construction_mainInsulation"), tr("Main insulation build")},
            {QStringLiteral("construction_cooling"), tr("Cooling method")},
            {QStringLiteral("construction_ambient"), tr("Ambient temperature")},
            {QStringLiteral("construction_maximumWindingTemperature"), tr("Maximum winding temperature")}
        };
        const QHash<QString, QString> units{
            {QStringLiteral("construction_coreMaterial"), QStringLiteral("material revision ID")},
            {QStringLiteral("construction_coreGeometry"), QStringLiteral("enumerated value")},
            {QStringLiteral("construction_stackingFactor"), QStringLiteral("dimensionless")},
            {QStringLiteral("construction_conductorMaterial"), QStringLiteral("material revision ID")},
            {QStringLiteral("construction_conductorShape"), QStringLiteral("enumerated value")},
            {QStringLiteral("construction_windingTechnology"), QStringLiteral("enumerated value")},
            {QStringLiteral("construction_turns"), QStringLiteral("turns")},
            {QStringLiteral("construction_turnsPerLayer"), QStringLiteral("turns")},
            {QStringLiteral("construction_layers"), QStringLiteral("layers")},
            {QStringLiteral("construction_parallelConductors"), QStringLiteral("conductors")},
            {QStringLiteral("construction_conductorArea"), QStringLiteral("mm²")},
            {QStringLiteral("construction_conductorWidth"), QStringLiteral("mm")},
            {QStringLiteral("construction_conductorThickness"), QStringLiteral("mm")},
            {QStringLiteral("construction_insulationMaterial"), QStringLiteral("material revision ID")},
            {QStringLiteral("construction_bobbinMaterial"), QStringLiteral("material / grade")},
            {QStringLiteral("construction_bobbinUsage"), QStringLiteral("boolean")},
            {QStringLiteral("construction_turnInsulation"), QStringLiteral("mm")},
            {QStringLiteral("construction_layerInsulation"), QStringLiteral("mm")},
            {QStringLiteral("construction_mainInsulation"), QStringLiteral("mm")},
            {QStringLiteral("construction_cooling"), QStringLiteral("enumerated value")},
            {QStringLiteral("construction_ambient"), QStringLiteral("°C")},
            {QStringLiteral("construction_maximumWindingTemperature"), QStringLiteral("°C")}
        };
        for (auto* widget : page->findChildren<QWidget*>()) {
            const QString key = widget->objectName();
            if (!key.startsWith(QStringLiteral("construction_"))) continue;
            QJsonValue value;
            if (const auto* combo = qobject_cast<QComboBox*>(widget)) {
                const QVariant data = combo->currentData();
                value = data.isValid() && !data.toString().isEmpty()
                    ? QJsonValue(data.toString()) : QJsonValue(QStringLiteral("NOT_PROVIDED"));
            } else if (const auto* number = qobject_cast<QDoubleSpinBox*>(widget)) {
                const double v = number->value();
                value = (v == number->minimum() || !std::isfinite(v))
                    ? QJsonValue(QStringLiteral("NOT_PROVIDED")) : QJsonValue(v);
            } else if (const auto* integer = qobject_cast<QSpinBox*>(widget)) {
                value = integer->value() == integer->minimum()
                    ? QJsonValue(QStringLiteral("NOT_PROVIDED")) : QJsonValue(integer->value());
            } else if (const auto* line = qobject_cast<QLineEdit*>(widget)) {
                value = line->text().trimmed().isEmpty()
                    ? QJsonValue(QStringLiteral("NOT_PROVIDED")) : QJsonValue(line->text().trimmed());
            } else if (const auto* textEdit = qobject_cast<QPlainTextEdit*>(widget)) {
                value = textEdit->toPlainText().trimmed().isEmpty()
                    ? QJsonValue(QStringLiteral("NOT_PROVIDED")) : QJsonValue(textEdit->toPlainText());
            } else if (const auto* check = qobject_cast<QCheckBox*>(widget)) {
                value = check->isChecked();
            } else continue;
            fields.insert(key, value);
            fieldLabels.insert(key, labels.value(key, key));
            QString unit = units.value(key);
            if (unit.isEmpty()) {
                if (key.contains(QStringLiteral("airGapCount")) ||
                    key.contains(QStringLiteral("coolingDuctCount")) ||
                    key.contains(QStringLiteral("construction_sections")))
                    unit = QStringLiteral("count");
                else if (key.contains(QStringLiteral("coreJoint_")) ||
                         key.contains(QStringLiteral("laminationConstruction")) ||
                         key.contains(QStringLiteral("SpacerRevision")) ||
                         key.contains(QStringLiteral("Airflow_")))
                    unit = QStringLiteral("reference");
                else if (key.contains(QStringLiteral("SafetyMargin"))) unit = QStringLiteral("K");
                else if (key.contains(QStringLiteral("ThermalClass")) || key.contains(QStringLiteral("ThermalStandard")))
                    unit = QStringLiteral("reference");
                else if (key.contains(QStringLiteral("Overall")) || key.contains(QStringLiteral("limb")) ||
                         key.contains(QStringLiteral("window")) || key.contains(QStringLiteral("yoke")) ||
                         key.contains(QStringLiteral("airGap")) || key.contains(QStringLiteral("coolingDuct")) ||
                         key.contains(QStringLiteral("bobbin")) ||
                         key.contains(QStringLiteral("windingRadial")) || key.contains(QStringLiteral("windingAxial")) ||
                         key.contains(QStringLiteral("windingEnd")))
                    unit = QStringLiteral("mm");
                else unit = QStringLiteral("NOT_APPLICABLE");
            }
            fieldUnits.insert(key, unit);
        }

        const auto positiveIfProvided = [this](const QString& key, const QString& label) {
            auto* value = findChild<QDoubleSpinBox*>(key);
            if (!value || value->value() == value->minimum()) return true;
            if (std::isfinite(value->value()) && value->value() > 0.0) return true;
            QMessageBox::warning(this, tr("Invalid construction input"),
                tr("%1 must be greater than zero when provided.").arg(label));
            value->setFocus();
            return false;
        };
        for (const auto& item : {
            std::pair{QStringLiteral("construction_stackingFactor"), tr("Stacking factor")},
            std::pair{QStringLiteral("construction_conductorArea"), tr("Conductor area")},
            std::pair{QStringLiteral("construction_conductorWidth"), tr("Conductor width")},
            std::pair{QStringLiteral("construction_conductorThickness"), tr("Conductor thickness")},
            std::pair{QStringLiteral("construction_turnInsulation"), tr("Turn insulation build")},
            std::pair{QStringLiteral("construction_layerInsulation"), tr("Layer insulation build")},
            std::pair{QStringLiteral("construction_mainInsulation"), tr("Main insulation build")}
        }) if (!positiveIfProvided(item.first, item.second)) return;

        int selectedJointCount = 0;
        for (int joint = 0; joint < 8; ++joint) {
            const auto* control = findChild<QComboBox*>(
                QStringLiteral("construction_coreJoint_") + QString::number(joint + 1));
            if (control && !control->currentData().toString().isEmpty()) ++selectedJointCount;
        }
        if (selectedJointCount != 0 && selectedJointCount != 8) {
            QMessageBox::warning(this, tr("Incomplete core joint construction"),
                tr("Specify the construction type for all eight limb-to-yoke interfaces, or leave all eight unspecified."));
            return;
        }

        std::array<int, 8> stepLapPacketCounts{};
        if (auto* schedule = findChild<QPlainTextEdit*>(
                QStringLiteral("construction_stepLapPacketSchedule"))) {
            const auto lines = schedule->toPlainText().split('\n');
            for (int lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
                const QString line = lines[lineIndex].trimmed();
                if (line.isEmpty() || line.startsWith('#')) continue;
                const auto columns = line.split(',');
                if (columns.size() != 6) {
                    QMessageBox::warning(this, tr("Invalid Step-Lap packet schedule"),
                        tr("Line %1 must contain six comma-separated fields.").arg(lineIndex + 1));
                    schedule->setFocus();
                    return;
                }
                bool jointOk = false;
                const int jointNumber = columns[0].trimmed().toInt(&jointOk);
                if (!jointOk || jointNumber < 1 || jointNumber > 8) {
                    QMessageBox::warning(this, tr("Invalid Step-Lap packet schedule"),
                        tr("Line %1 must reference joint number 1 through 8.").arg(lineIndex + 1));
                    schedule->setFocus();
                    return;
                }
                std::array<double, 4> values{};
                bool validValues = true;
                for (int column = 0; column < 4; ++column) {
                    bool ok = false;
                    values[static_cast<std::size_t>(column)] = columns[column + 1].trimmed().toDouble(&ok);
                    validValues = validValues && ok &&
                        std::isfinite(values[static_cast<std::size_t>(column)]);
                }
                const QString direction = columns[5].trimmed().toLower();
                if (!validValues || values[0] < 0.0 || values[1] <= 0.0 ||
                    values[2] <= 0.0 || values[3] <= 0.0 ||
                    (direction != QStringLiteral("limb-into-yoke") &&
                     direction != QStringLiteral("yoke-into-limb"))) {
                    QMessageBox::warning(this, tr("Invalid Step-Lap packet schedule"),
                        tr("Line %1 requires non-negative offset, positive depth/overlaps and a valid direction.")
                            .arg(lineIndex + 1));
                    schedule->setFocus();
                    return;
                }
                ++stepLapPacketCounts[static_cast<std::size_t>(jointNumber - 1)];
            }
        }
        for (int joint = 0; joint < 8; ++joint) {
            const auto* control = findChild<QComboBox*>(
                QStringLiteral("construction_coreJoint_") + QString::number(joint + 1));
            const QString construction = control ? control->currentData().toString() : QString{};
            if (construction == QStringLiteral("step-lap") &&
                stepLapPacketCounts[static_cast<std::size_t>(joint)] == 0) {
                QMessageBox::warning(this, tr("Missing Step-Lap packet schedule"),
                    tr("Joint %1 is Step-Lap but has no packet records.").arg(joint + 1));
                return;
            }
            if (construction == QStringLiteral("butt") &&
                stepLapPacketCounts[static_cast<std::size_t>(joint)] != 0) {
                QMessageBox::warning(this, tr("Unexpected Step-Lap packet schedule"),
                    tr("Joint %1 is Butt but has Step-Lap packet records.").arg(joint + 1));
                return;
            }
        }


        const auto constructionInputs = collectReactorDesignInputs();
        const auto constructionValidation = p135::application::validate(constructionInputs);
        if (!constructionValidation) {
            QMessageBox::warning(this, tr("Invalid construction inputs"),
                QString::fromStdString(constructionValidation.error().message));
            return;
        }

        QJsonObject snapshot;
        snapshot.insert(QStringLiteral("schemaVersion"), 1);
        snapshot.insert(QStringLiteral("tabId"), QStringLiteral("construction"));
        snapshot.insert(QStringLiteral("tabTitle"), QStringLiteral("Construction"));
        snapshot.insert(QStringLiteral("projectId"), projectId_);
        snapshot.insert(QStringLiteral("confirmedAtUtc"),
            QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
        snapshot.insert(QStringLiteral("buildConfiguration"),
#ifdef _DEBUG
            QStringLiteral("Debug")
#else
            QStringLiteral("Release")
#endif
        );
        snapshot.insert(QStringLiteral("fields"), fields);
        snapshot.insert(QStringLiteral("fieldLabels"), fieldLabels);
        snapshot.insert(QStringLiteral("fieldUnits"), fieldUnits);
        QJsonObject lineage;
        lineage.insert(QStringLiteral("filterSnapshotId"),
            latestFilterSnapshotId_.isEmpty() ? QStringLiteral("NOT_AVAILABLE") : latestFilterSnapshotId_);
        lineage.insert(QStringLiteral("filterBranchId"),
            latestFilterBranchId_ == 0 ? QJsonValue(QStringLiteral("NOT_AVAILABLE"))
                                       : QJsonValue(static_cast<qint64>(latestFilterBranchId_)));
        lineage.insert(QStringLiteral("physicalSizingStatus"), QStringLiteral("NOT_CONNECTED_TO_SOLVER"));
        QSettings lineageSettings;
        lineageSettings.beginGroup(QStringLiteral("projects/") + projectId_ + QStringLiteral("/construction"));
        lineage.insert(QStringLiteral("constructionAmbientSource"),
            lineageSettings.value(QStringLiteral("construction_ambient_source")).toString() == QStringLiteral("MANUAL")
                ? QStringLiteral("USER_OVERRIDE") : QStringLiteral("INHERITED_FROM_PROJECT: Project.ambientC"));
        lineage.insert(QStringLiteral("constructionCoolingSource"),
            lineageSettings.value(QStringLiteral("construction_cooling_source")).toString() == QStringLiteral("MANUAL")
                ? QStringLiteral("USER_OVERRIDE") : QStringLiteral("INHERITED_FROM_PROJECT: Project.coolingMethod"));
        lineageSettings.endGroup();
        const auto* insulationClass = findChild<QComboBox*>(QStringLiteral("projectInsulationClass"));
        lineage.insert(QStringLiteral("projectInsulationClass"),
            insulationClass ? insulationClass->currentText() : QStringLiteral("NOT_PROVIDED"));
        lineage.insert(QStringLiteral("maximumWindingTemperatureStatus"), QStringLiteral("NOT_PROVIDED"));
        snapshot.insert(QStringLiteral("lineage"), lineage);
        snapshot.insert(QStringLiteral("physicalSizingStatus"), QStringLiteral("NOT_CONNECTED_TO_SOLVER"));

#ifdef _DEBUG
        const QString outputDirectory = QStringLiteral("C:/Temp/trdos/135");
        if (!QDir().mkpath(outputDirectory)) {
            QMessageBox::critical(this, tr("Cannot save construction snapshot"),
                tr("Cannot create the project output directory: %1").arg(outputDirectory));
            return;
        }
        QSaveFile output(QDir(outputDirectory).filePath(QStringLiteral("03_construction.json")));
        if (!output.open(QIODevice::WriteOnly)) {
            QMessageBox::critical(this, tr("Cannot save construction snapshot"), output.errorString());
            return;
        }
        const QByteArray json = QJsonDocument(snapshot).toJson(QJsonDocument::Indented);
        if (output.write(json) != json.size() || !output.commit()) {
            QMessageBox::critical(this, tr("Cannot save construction snapshot"),
                tr("Writing the construction snapshot failed: %1").arg(output.errorString()));
            return;
        }
        confirmationStatus->setText(tr("Confirmed. Debug snapshot saved: C:/Temp/trdos/135/03_construction.json"));
        appendLog(tr("Construction tab confirmed; snapshot saved to C:/Temp/trdos/135/03_construction.json."));
#else
        confirmationStatus->setText(tr("Construction confirmed (Release build; debug snapshot is disabled)."));
        appendLog(tr("Construction tab confirmed (Release build; debug snapshot is disabled)."));
#endif
        if (auto* state = findChild<QLabel*>(QStringLiteral("constructionInputState"))) {
            state->setText(tr("CONSTRUCTION INPUTS CONFIRMED — physical sizing: NOT_CONNECTED_TO_SOLVER."));
            state->setStyleSheet("color: palette(text); font-weight: 600;");
        }
        acceptedConstructionInputs_ = constructionInputs;
        constructionInputsDirty_ = false;
        appendLog(tr("[CONSTRUCTION] Accepted immutable input capture linked to filter snapshot %1 / branch %2.")
            .arg(latestFilterSnapshotId_).arg(latestFilterBranchId_));
        tabs_->setCurrentIndex(3);
    });

    auto* scroll = new QScrollArea;
    scroll->setObjectName(QStringLiteral("constructionScrollArea"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
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

    auto* designGroup = new QGroupBox(tr("Construction design parameters"));
    auto* designLayout = new QVBoxLayout(designGroup);
    auto* designStatus = new QLabel(tr(
        "Physical sizing is not yet calculated. The existing magnetic solver analyses a supplied core geometry; "
        "it does not synthesize physical dimensions from filter requirements. Values are deliberately left unavailable "
        "rather than populated with guessed dimensions."));
    designStatus->setWordWrap(true);
    designStatus->setTextInteractionFlags(Qt::TextSelectableByMouse);
    designLayout->addWidget(designStatus);

    auto* idealCriteria = new QGroupBox(tr("Ideal first-pass sizing criteria (user supplied)"), designGroup);
    auto* idealCriteriaLayout = new QFormLayout(idealCriteria);
    const auto addIdealCriterion = [idealCriteria](const QString& objectName, double maximum,
                                                    int decimals, const QString& suffix) {
        auto* control = new QDoubleSpinBox(idealCriteria);
        control->setObjectName(objectName);
        control->setRange(0.0, maximum);
        control->setDecimals(decimals);
        control->setSingleStep(decimals <= 2 ? 0.01 : 0.1);
        control->setSpecialValueText(QObject::tr("NOT PROVIDED"));
        control->setSuffix(suffix);
        control->setValue(0.0);
        return control;
    };
    idealCriteriaLayout->addRow(tr("Allowable peak flux density Bmax:"), 
        addIdealCriterion(QStringLiteral("idealSizing_Bmax"), 2.0, 4, tr(" T")));
    idealCriteriaLayout->addRow(tr("Allowable current density J:"), 
        addIdealCriterion(QStringLiteral("idealSizing_J"), 100.0, 3, tr(" A/mm²")));
    idealCriteriaLayout->addRow(tr("Window utilization Ku:"), 
        addIdealCriterion(QStringLiteral("idealSizing_Ku"), 100.0, 2, tr(" %")));
    idealCriteriaLayout->addRow(tr("Stacking factor Ks:"),
        addIdealCriterion(QStringLiteral("idealSizing_Ks"), 100.0, 2, tr(" %")));
    idealCriteriaLayout->addRow(tr("Window / core area ratio Aw/Ae:"),
        addIdealCriterion(QStringLiteral("idealSizing_AwAe"), 1000.0, 4, QString{}));
    designLayout->addWidget(idealCriteria);

    auto* idealSizingResult = new QLabel(tr("Ideal area-product estimate: NOT CALCULATED — enter all five criteria and recalculate."));
    idealSizingResult->setObjectName(QStringLiteral("idealSizingResult"));
    idealSizingResult->setWordWrap(true);
    idealSizingResult->setTextInteractionFlags(Qt::TextSelectableByMouse);
    designLayout->addWidget(idealSizingResult);

    auto* designTable = new QTableWidget(0, 4, designGroup);
    designTable->setObjectName(QStringLiteral("constructionDesignTable"));
    designTable->setHorizontalHeaderLabels({
        tr("Parameter"), tr("Calculated"), tr("Accepted"), tr("State")
    });
    designTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    designTable->verticalHeader()->setDefaultSectionSize(26);
    designTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    const QStringList designParameters{
        tr("Limb width A / B / C"),
        tr("Core stack depth"),
        tr("Window width left / right"),
        tr("Window height left / right"),
        tr("Yoke thickness top / bottom"),
        tr("Air gap per limb"),
        tr("Turns per phase"),
        tr("Winding conductor cross-section"),
        tr("Minimum gross area product Ae×Aw"),
        tr("Equivalent square area scale"),
        tr("Estimated gross core area Ae"),
        tr("Estimated effective core area"),
        tr("Estimated window area Aw"),
        tr("Ideal minimum turns per reactor"),
        tr("Ideal total equivalent air gap")
    };
    for (const auto& parameter : designParameters) {
        const int row = designTable->rowCount();
        designTable->insertRow(row);
        designTable->setItem(row, 0, new QTableWidgetItem(parameter));
        designTable->setItem(row, 1, new QTableWidgetItem(tr("NOT CALCULATED")));
        designTable->setItem(row, 2, new QTableWidgetItem(tr("—")));
        designTable->setItem(row, 3, new QTableWidgetItem(tr("NOT_VERIFIED")));
    }
    designTable->setMinimumHeight(250);
    designLayout->addWidget(designTable);

    auto* missingInputs = new QLabel(tr(
        "Required for a physically grounded automatic design: core material B-H and loss data; phase RMS/peak and harmonic currents; "
        "winding voltage and insulation requirements; conductor current-density or thermal design criterion; winding technology and fill constraints; "
        "cooling/ambient limits; and construction rules. Missing data must remain explicit."));
    missingInputs->setWordWrap(true);
    missingInputs->setStyleSheet("color: #a66b00;");
    designLayout->addWidget(missingInputs);

    auto* recalculateProject = new QPushButton(tr("Recalculate project"));
    recalculateProject->setObjectName(QStringLiteral("recalculateProjectButton"));
    connect(recalculateProject, &QPushButton::clicked, this, [this, designStatus] {
        calculateFilter();
        designStatus->setText(tr(
            "Electrical filter synthesis was refreshed. Physical geometry, winding sizing and coupled electro-magnetic-thermal "
            "design remain NOT_CALCULATED because the inverse physical-design solver and required design inputs are not yet available."));
        appendLog(tr("Project recalculation requested: electrical synthesis executed; physical design remains NOT_CALCULATED."));
    });
    designLayout->addWidget(recalculateProject);
    layout->addWidget(designGroup);

    resultsTable_ = new QTableWidget(0, 3);
    resultsTable_->setObjectName(QStringLiteral("calculationResultsTable"));
    resultsTable_->setHorizontalHeaderLabels({tr("Parameter"), tr("Value"), tr("Unit")});
    resultsTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    resultsTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    resultsTable_->verticalHeader()->setDefaultSectionSize(27);
    resultsTable_->setMinimumHeight(290);
    resultsTable_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    layout->addWidget(resultsTable_);

    auto* currentPlotTitle = new QLabel(tr("Reactor branch peak current — frequency sweep"));
    currentPlotTitle->setObjectName(QStringLiteral("inductorCurrentPlotTitle"));
    currentPlotTitle->setStyleSheet("font-weight: 600;");
    layout->addWidget(currentPlotTitle);
    resultsBankCurrentPlot_ = new PlotWidget;
    resultsBankCurrentPlot_->setObjectName(QStringLiteral("resultsBankCurrentPlot"));
    resultsBankCurrentPlot_->setProperty("debugImagePath", projectArtifactPath(QStringLiteral("Debug/Plots"), projectId_, QStringLiteral("_reactor_current.png")));
    resultsBankCurrentPlot_->setCurves({}, tr("Frequency (Hz, logarithmic)"), tr("Reactor branch peak current (A)"));
    resultsBankCurrentPlot_->setMinimumHeight(250);
    resultsBankCurrentPlot_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    layout->addWidget(resultsBankCurrentPlot_);

    auto* coreFluxPlotTitle = new QLabel(tr("Three-limb core peak flux density — manual diagnostic frequency sweep"));
    coreFluxPlotTitle->setStyleSheet("font-weight: 600;");
    coreFluxPlotTitle->setToolTip(tr(
        "This chart uses the legacy manual Ae/le/B-H frequency-sweep model. "
        "The Construction-revision-bound harmonic operating-point result is a different "
        "calculation and is not plotted on this frequency axis."));
    layout->addWidget(coreFluxPlotTitle);
    resultsCoreFluxPlot_ = new PlotWidget;
    resultsCoreFluxPlot_->setObjectName(QStringLiteral("resultsCoreFluxPlot"));
    resultsCoreFluxPlot_->setProperty("debugImagePath", projectArtifactPath(QStringLiteral("Debug/Plots"), projectId_, QStringLiteral("_core_flux.png")));
    resultsCoreFluxPlot_->setCurves({}, tr("Frequency (Hz, logarithmic)"), tr("Peak flux density |B| (T)"));
    resultsCoreFluxPlot_->setMinimumHeight(250);
    resultsCoreFluxPlot_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    layout->addWidget(resultsCoreFluxPlot_);

    auto* impedancePlotTitle = new QLabel(tr("Bus impedance — frequency scan"));
    impedancePlotTitle->setStyleSheet("font-weight: 600;");
    layout->addWidget(impedancePlotTitle);
    resultsBusImpedancePlot_ = new PlotWidget;
    resultsBusImpedancePlot_->setObjectName(QStringLiteral("resultsBusImpedancePlot"));
    resultsBusImpedancePlot_->setProperty("debugImagePath", projectArtifactPath(QStringLiteral("Debug/Plots"), projectId_, QStringLiteral("_bus_impedance.png")));
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

    auto* calculationConfirmationRow = new QHBoxLayout;
    auto* calculationConfirmationStatus = new QLabel(
        tr("Calculation & design has not been confirmed."));
    calculationConfirmationStatus->setObjectName(QStringLiteral("calculationDesignConfirmationStatus"));
    calculationConfirmationStatus->setWordWrap(true);
    auto* confirmCalculation = new QPushButton(tr("Confirm calculation & design"));
    confirmCalculation->setObjectName(QStringLiteral("confirmCalculationDesign"));
    calculationConfirmationRow->addWidget(calculationConfirmationStatus, 1);
    calculationConfirmationRow->addWidget(confirmCalculation);
    layout->addLayout(calculationConfirmationRow);

    connect(confirmCalculation, &QPushButton::clicked, this,
        [this, calculationConfirmationStatus] {
        if (!resultsTable_ || !resultsTable_->property("calculationRunAvailable").toBool() ||
            resultsTable_->rowCount() == 0) {
            QMessageBox::warning(this, tr("Calculation not ready"),
                tr("Run a successful calculation before confirming this tab."));
            return;
        }
        if (latestFilterSnapshotId_.isEmpty()) {
            QMessageBox::warning(this, tr("Calculation snapshot unavailable"),
                tr("The calculation has no saved immutable filter snapshot. Check the calculation log and database."));
            return;
        }
        QJsonObject snapshot;
        snapshot.insert(QStringLiteral("schemaVersion"), 1);
        snapshot.insert(QStringLiteral("tabId"), QStringLiteral("calculation_design"));
        snapshot.insert(QStringLiteral("tabTitle"), QStringLiteral("Calculation & design"));
        snapshot.insert(QStringLiteral("projectId"), projectId_);
        snapshot.insert(QStringLiteral("confirmedAtUtc"),
            QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
        snapshot.insert(QStringLiteral("buildConfiguration"),
#ifdef _DEBUG
            QStringLiteral("Debug")
#else
            QStringLiteral("Release")
#endif
        );
        QJsonObject inputs;
        inputs.insert(QStringLiteral("nominalVoltage_V"), nominalVoltage_ ? nominalVoltage_->value() : 0.0);
        inputs.insert(QStringLiteral("fundamentalFrequency_Hz"), fundamentalFrequency_ ? fundamentalFrequency_->value() : 0.0);
        inputs.insert(QStringLiteral("connection"), filterConnection_ ? filterConnection_->currentText() : QString{});
        inputs.insert(QStringLiteral("targetReactivePower_kvar"), targetReactivePower_ ? targetReactivePower_->value() : 0.0);
        inputs.insert(QStringLiteral("tuningHarmonicOrder"), tuningHarmonicOrder_ ? tuningHarmonicOrder_->value() : 0.0);
        inputs.insert(QStringLiteral("qualityFactor"), qualityFactor_ ? qualityFactor_->value() : 0.0);
        inputs.insert(QStringLiteral("sourceResistance_ohm"), sourceResistance_ ? sourceResistance_->value() : 0.0);
        inputs.insert(QStringLiteral("sourceInductance_mH"), sourceInductance_ ? sourceInductance_->value() : 0.0);
        snapshot.insert(QStringLiteral("inputs"), inputs);
        QJsonArray results;
        for (int row = 0; row < resultsTable_->rowCount(); ++row) {
            QJsonObject item;
            item.insert(QStringLiteral("parameter"), resultsTable_->item(row, 0)
                ? resultsTable_->item(row, 0)->text() : QString{});
            item.insert(QStringLiteral("value"), resultsTable_->item(row, 1)
                ? resultsTable_->item(row, 1)->text() : QString{});
            item.insert(QStringLiteral("unit"), resultsTable_->item(row, 2)
                ? resultsTable_->item(row, 2)->text() : QString{});
            results.append(item);
        }
        snapshot.insert(QStringLiteral("results"), results);
        snapshot.insert(QStringLiteral("warnings"), resultsNote_ ? resultsNote_->text() : QString{});
        snapshot.insert(QStringLiteral("calculationStatus"), status_ ? status_->text() : QStringLiteral("NOT_AVAILABLE"));
        const bool sourceLimited = sourceResistance_ && sourceInductance_ &&
            (sourceResistance_->value() > 0.0 || sourceInductance_->value() > 0.0);
        snapshot.insert(QStringLiteral("currentResponseStatus"), sourceLimited
            ? QStringLiteral("SOURCE_IMPEDANCE_INCLUDED_SINGLE_FREQUENCY_DIAGNOSTIC")
            : QStringLiteral("IDEAL_SOURCE_DIAGNOSTIC_NOT_OPERATIONAL_STRESS"));
        QJsonObject lineage;
        lineage.insert(QStringLiteral("filterSnapshotId"), latestFilterSnapshotId_);
        lineage.insert(QStringLiteral("filterBranchId"), static_cast<qint64>(latestFilterBranchId_));
        lineage.insert(QStringLiteral("constructionPhysicalSizingStatus"), QStringLiteral("NOT_CONNECTED_TO_SOLVER"));
        lineage.insert(QStringLiteral("physicalSizingStatus"), QStringLiteral("NOT_CALCULATED"));
        lineage.insert(QStringLiteral("verificationStatus"),
            resultsNote_ && resultsNote_->text().contains(QStringLiteral("NOT_VERIFIED"))
                ? QStringLiteral("NOT_VERIFIED") : QStringLiteral("REVIEW_REQUIRED"));
        QJsonArray materialRevisions;
        const auto appendRevision = [this, &materialRevisions](const QString& objectName,
                                                               const QString& role) {
            const auto* selector = findChild<QComboBox*>(objectName);
            if (!selector || selector->currentData().toString().isEmpty()) return;
            QJsonObject reference;
            reference.insert(QStringLiteral("role"), role);
            reference.insert(QStringLiteral("revisionId"), selector->currentData().toString());
            reference.insert(QStringLiteral("display"), selector->currentText());
            materialRevisions.append(reference);
        };
        appendRevision(QStringLiteral("construction_coreMaterial"), QStringLiteral("core"));
        appendRevision(QStringLiteral("construction_conductorMaterial"), QStringLiteral("conductor"));
        appendRevision(QStringLiteral("construction_insulationMaterial"), QStringLiteral("insulation"));
        appendRevision(QStringLiteral("filterCapacitorRevision"), QStringLiteral("capacitor"));
        appendRevision(QStringLiteral("filterDampingResistorRevision"), QStringLiteral("damping_resistor"));
        lineage.insert(QStringLiteral("materialRevisions"), materialRevisions);
        snapshot.insert(QStringLiteral("lineage"), lineage);
#ifdef _DEBUG
        const QString outputDirectory = QStringLiteral("C:/Temp/trdos/135");
        if (!QDir().mkpath(outputDirectory)) {
            QMessageBox::critical(this, tr("Cannot save calculation snapshot"),
                tr("Cannot create the project output directory: %1").arg(outputDirectory));
            return;
        }
        QSaveFile output(QDir(outputDirectory).filePath(QStringLiteral("04_calculation_design.json")));
        if (!output.open(QIODevice::WriteOnly)) {
            QMessageBox::critical(this, tr("Cannot save calculation snapshot"), output.errorString());
            return;
        }
        const QByteArray json = QJsonDocument(snapshot).toJson(QJsonDocument::Indented);
        if (output.write(json) != json.size() || !output.commit()) {
            QMessageBox::critical(this, tr("Cannot save calculation snapshot"),
                tr("Writing the calculation snapshot failed: %1").arg(output.errorString()));
            return;
        }
        calculationConfirmationStatus->setText(
            tr("Confirmed. Debug snapshot saved: C:/Temp/trdos/135/04_calculation_design.json"));
#else
        calculationConfirmationStatus->setText(tr("Confirmed. Release snapshot generation is disabled."));
#endif
        appendLog(tr("Calculation & design confirmed. Physical sizing remains NOT_CONNECTED_TO_SOLVER."));
        if (tabs_) tabs_->setCurrentIndex(4);
    });

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
    historyPlotMetric_->addItem(tr("Reactor branch peak current (A)"), QStringLiteral("reactorBranchPeakCurrent_A"));
    historyPlotMetric_->addItem(tr("Bus impedance (Ω)"), QStringLiteral("busImpedance_Ohm"));
    auto* compareGraphs = new QPushButton(tr("Compare graphs of selected calculations"));
    compareRow->addWidget(historyPlotMetric_);
    compareRow->addWidget(compareGraphs);
    layout->addLayout(compareRow);
    historyComparisonPlot_ = new PlotWidget;
    historyComparisonPlot_->setObjectName(QStringLiteral("historyComparisonPlot"));
    historyComparisonPlot_->setProperty("debugImagePath", projectArtifactPath(QStringLiteral("Debug/Plots"), projectId_, QStringLiteral("_history_comparison.png")));
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
                tr("The SHA-256 checksum stored in the archive does not match calculation %1.\n\n"
                   "Stored: %2\nCalculated from the imported data: %3\n\n"
                   "Recalculate the checksum and import this record?\n"
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
    const bool currentMetric = metricKey == QStringLiteral("reactorBranchPeakCurrent_A");
    const QString arrayKey = currentMetric ? QStringLiteral("frequencySweep")
                                            : QStringLiteral("impedanceScan");
    const QString yLabel = currentMetric ? tr("Reactor branch peak current (A)") : tr("Bus impedance (Ω)");
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
        historyDetails_->appendPlainText(tr("\nGraph comparison unavailable: one or both selected snapshots do not contain this frequency series. Older snapshots do not contain the corrected reactor-branch current series."));
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
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);

    auto* heading = new QLabel(tr("3D CAD workspace"));
    heading->setStyleSheet("font-size: 20px; font-weight: 600;");
    layout->addWidget(heading);

    auto* status = new QLabel(tr(
        "CAD preview is built only from explicit Construction dimensions with source/revision traceability. "
        "The preview is preliminary geometry, not magnetic, thermal, dielectric or manufacturing approval."));
    status->setObjectName(QStringLiteral("cadGenerationStatus"));
    status->setWordWrap(true);
    status->setTextInteractionFlags(Qt::TextSelectableByMouse);
    status->setStyleSheet("color: palette(mid);");
    layout->addWidget(status);
    auto* generateCad = new QPushButton(tr("Generate preliminary 3D model from Construction"));
    generateCad->setObjectName(QStringLiteral("generatePreliminaryCad"));
    layout->addWidget(generateCad);
    auto* exportStep = new QPushButton(tr("Export B-Rep checked assembly to STEP"));
    exportStep->setObjectName(QStringLiteral("exportCadStep"));
    exportStep->setEnabled(false);
    layout->addWidget(exportStep);
    auto* exportDrawing = new QPushButton(tr("Generate and export preliminary 2D drawing (DXF)"));
    exportDrawing->setObjectName(QStringLiteral("generateCadDrawings"));
    exportDrawing->setEnabled(false);
    layout->addWidget(exportDrawing);

    auto* workspace = new QSplitter(Qt::Horizontal, page);
    workspace->setChildrenCollapsible(false);

    auto* structurePanel = new QWidget(workspace);
    structurePanel->setMinimumWidth(250);
    structurePanel->setMaximumWidth(360);
    auto* structureLayout = new QVBoxLayout(structurePanel);
    structureLayout->setContentsMargins(0, 0, 8, 0);
    auto* structureTitle = new QLabel(tr("Assembly structure"), structurePanel);
    structureTitle->setStyleSheet("font-weight: 600;");
    structureLayout->addWidget(structureTitle);

    auto* tree = new QTreeWidget(structurePanel);
    tree->setObjectName(QStringLiteral("cadAssemblyTree"));
    tree->setColumnCount(2);
    tree->setHeaderLabels({tr("Component"), tr("State")});
    tree->setRootIsDecorated(true);
    tree->setAlternatingRowColors(true);
    tree->setMinimumHeight(240);
    auto* placeholder = new QTreeWidgetItem(tree,
        {tr("No generated assembly"), QStringLiteral("NOT_CALCULATED")});
    placeholder->setToolTip(1, tr("Generate a preliminary assembly from confirmed Construction inputs."));
    structureLayout->addWidget(tree, 1);

    auto* transparencyLabel = new QLabel(tr("Selected component transparency"), structurePanel);
    structureLayout->addWidget(transparencyLabel);
    auto* transparency = new QDoubleSpinBox(structurePanel);
    transparency->setObjectName(QStringLiteral("cadComponentTransparency"));
    transparency->setRange(0.0, 1.0);
    transparency->setSingleStep(0.05);
    transparency->setDecimals(2);
    transparency->setValue(0.0);
    transparency->setSuffix(tr(" opacity loss"));
    transparency->setEnabled(false);
    structureLayout->addWidget(transparency);

    auto* propertiesTitle = new QLabel(tr("Component properties"), structurePanel);
    propertiesTitle->setStyleSheet("font-weight: 600;");
    structureLayout->addWidget(propertiesTitle);
    auto* properties = new QLabel(tr(
        "No preliminary CAD has been generated for the current Construction revision."));
    properties->setWordWrap(true);
    properties->setMinimumHeight(72);
    properties->setFrameShape(QFrame::StyledPanel);
    properties->setMargin(8);
    structureLayout->addWidget(properties);

    auto* viewportPanel = new QWidget(workspace);
    auto* viewportLayout = new QVBoxLayout(viewportPanel);
    viewportLayout->setContentsMargins(0, 0, 0, 0);
    auto* viewport = new CadViewportWidget(viewportPanel);
    viewport->setObjectName(QStringLiteral("cadViewportWidget"));
    viewportLayout->addWidget(viewport, 1);

    auto assemblyModel = std::make_shared<p135::cad::AssemblyPresentationNode>();
    auto physicalGeometry = std::make_shared<std::optional<p135::domain::ReactorPhysicalGeometry>>();
    const auto rebuildCadDisplay = [assemblyModel, viewport] {
        viewport->setDisplayItems(p135::cad::collectAssemblyPresentationDisplayItems(*assemblyModel));
    };
    connect(tree, &QTreeWidget::itemChanged, this,
        [assemblyModel, rebuildCadDisplay](QTreeWidgetItem* item, int column) {
        if (column != 0) return;
        const auto id = item->data(0, Qt::UserRole).toString();
        if (id.isEmpty()) return;
        if (p135::cad::setAssemblyComponentVisibility(
                *assemblyModel, id.toStdString(), item->checkState(0) == Qt::Checked))
            rebuildCadDisplay();
    });
    connect(tree, &QTreeWidget::currentItemChanged, this,
        [assemblyModel, transparency, properties](QTreeWidgetItem* current, QTreeWidgetItem*) {
        const auto id = current ? current->data(0, Qt::UserRole).toString() : QString{};
        if (id.isEmpty()) { transparency->setEnabled(false); return; }
        std::function<const p135::cad::AssemblyPresentationNode*(const p135::cad::AssemblyPresentationNode&)> findNode;
        findNode = [&findNode, &id](const p135::cad::AssemblyPresentationNode& node)
            -> const p135::cad::AssemblyPresentationNode* {
            if (node.id == id.toStdString()) return &node;
            for (const auto& child : node.children)
                if (const auto* found = findNode(child)) return found;
            return nullptr;
        };
        if (const auto* node = findNode(*assemblyModel)) {
            transparency->blockSignals(true);
            transparency->setValue(node->transparency);
            transparency->blockSignals(false);
            transparency->setEnabled(true);
            const QString representation =
                node->representation == p135::cad::AssemblyGeometryRepresentation::SimplifiedCoreSegment
                    ? QObject::tr("Simplified core segment")
                    : node->representation == p135::cad::AssemblyGeometryRepresentation::WindingPackEnvelope
                        ? QObject::tr("Winding envelope (not detailed conductor geometry)")
                        : QObject::tr("Physical solid");
            properties->setText(QObject::tr(
                "%1\nID: %2\nRepresentation: %3\nVisibility: %4\nTransparency: %5")
                .arg(QString::fromStdString(node->displayName),
                     QString::fromStdString(node->id), representation,
                     node->visible ? QObject::tr("Visible") : QObject::tr("Hidden"),
                     QString::number(node->transparency, 'f', 2)));
        } else {
            transparency->setEnabled(false);
            properties->setText(QObject::tr("No component selected."));
        }
    });
    connect(transparency, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
        [assemblyModel, rebuildCadDisplay, tree](double value) {
        auto* item = tree->currentItem();
        if (!item) return;
        const auto id = item->data(0, Qt::UserRole).toString();
        if (id.isEmpty()) return;
        if (p135::cad::setAssemblyComponentTransparency(*assemblyModel, id.toStdString(), value))
            rebuildCadDisplay();
    });

    connect(generateCad, &QPushButton::clicked, this,
        [this, status, properties, viewport, generateCad, exportStep, exportDrawing, tree, transparency, assemblyModel, physicalGeometry] {
        if (constructionInputsDirty_) {
            status->setText(tr("CAD is blocked: confirm Construction again after the latest input change."));
            return;
        }
        if (latestFilterSnapshotId_.isEmpty() || latestFilterBranchId_ == 0) {
            status->setText(tr("CAD is blocked: no accepted filter calculation snapshot is available."));
            return;
        }
        if (!acceptedConstructionInputs_) {
            status->setText(tr("CAD is blocked: no accepted Construction input revision is available. Confirm Construction first."));
            return;
        }
        const auto physical = p135::application::makeReactorPhysicalGeometry(
            *acceptedConstructionInputs_, projectId_.toStdString());
        if (!physical) {
            const auto& error = physical.error();
            status->setText(tr("CAD is waiting for explicit physical inputs: %1").arg(
                QString::fromStdString(error.message)));
            appendLog(tr("[CAD] PRELIMINARY MODEL NOT BUILT: %1").arg(
                QString::fromStdString(error.message)));
            return;
        }
        const auto presentation = p135::cad::buildThreeLimbReactorPresentation(physical.value());
        if (!presentation) {
            status->setText(tr("CAD generation failed: %1").arg(
                QString::fromStdString(presentation.error().message)));
            appendLog(tr("[CAD] BUILD FAILED: %1").arg(
                QString::fromStdString(presentation.error().message)));
            return;
        }
        *assemblyModel = presentation.value();
        *physicalGeometry = physical.value();
        {
            const QSignalBlocker blocker(tree);
            tree->clear();
            std::function<void(const p135::cad::AssemblyPresentationNode&, QTreeWidgetItem*)> appendNode;
            appendNode = [&appendNode, tree](const p135::cad::AssemblyPresentationNode& node, QTreeWidgetItem* parent) {
                auto* item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(tree);
                item->setText(0, QString::fromStdString(node.displayName));
                item->setText(1, QStringLiteral("BUILT"));
                item->setData(0, Qt::UserRole, QString::fromStdString(node.id));
                item->setFlags(item->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsSelectable | Qt::ItemIsEnabled);
                item->setCheckState(0, node.visible ? Qt::Checked : Qt::Unchecked);
                const QString detail = node.representation == p135::cad::AssemblyGeometryRepresentation::SimplifiedCoreSegment
                    ? QObject::tr("Simplified core segment")
                    : node.representation == p135::cad::AssemblyGeometryRepresentation::WindingPackEnvelope
                        ? QObject::tr("Winding envelope — not detailed conductor geometry")
                        : QObject::tr("Physical solid");
                item->setToolTip(0, detail);
                for (const auto& child : node.children) appendNode(child, item);
                item->setExpanded(true);
            };
            appendNode(*assemblyModel, nullptr);
        }
        tree->setCurrentItem(tree->topLevelItem(0));
        const auto items = p135::cad::collectAssemblyPresentationDisplayItems(*assemblyModel);
        if (items.empty()) {
            status->setText(tr("CAD generation returned no displayable solids."));
            return;
        }
        viewport->setDisplayItems(items);

        // Automatically persist the accepted physical assembly and its drawing
        // beside the project workspace. Manual export actions remain available.
        const QString cadDirectory = QDir(projectOutputRoot()).filePath(QStringLiteral("CAD"));
        const QString drawingDirectory = QDir(projectOutputRoot()).filePath(QStringLiteral("Drawings"));
        bool automaticStepSaved = false;
        bool automaticPdfSaved = false;
        const bool cadDirectoryReady = QDir().mkpath(cadDirectory);
        const bool drawingDirectoryReady = QDir().mkpath(drawingDirectory);
        if (!cadDirectoryReady || !drawingDirectoryReady) {
            const QString message = tr("Automatic CAD output directories could not be created under C:/Temp/trdos/135.");
            status->setText(message);
            appendLog(QStringLiteral("[ARTIFACT] ERROR: ") + message);
        } else {
            const QString stepPath = QDir(cadDirectory).filePath(projectId_ + QStringLiteral("_assembly.step"));
            const auto stepResult = p135::cad::exportAssemblyStep(*assemblyModel, stepPath.toStdString());
            if (stepResult) {
                automaticStepSaved = true;
                appendLog(tr("[ARTIFACT] STEP AUTO-SAVED: %1").arg(stepPath));
            } else {
                appendLog(tr("[ARTIFACT] STEP AUTO-SAVE FAILED: %1").arg(
                    QString::fromStdString(stepResult.error().message)));
            }
            const QString pdfPath = QDir(drawingDirectory).filePath(projectId_ + QStringLiteral("_assembly.pdf"));
            QString pdfError;
            if (exportReactorAssemblyPdf(physical.value(), pdfPath, &pdfError)) {
                automaticPdfSaved = true;
                appendLog(tr("[ARTIFACT] PDF AUTO-SAVED: %1").arg(pdfPath));
            } else {
                appendLog(tr("[ARTIFACT] PDF AUTO-SAVE FAILED: %1").arg(pdfError));
            }
        }

        if (auto* generatedRoot = tree->topLevelItem(0))
            generatedRoot->setText(1, QStringLiteral("PRELIMINARY"));
        properties->setText(tr(
            "PRELIMINARY / NOT_VERIFIED. Source: %1; revision: %2. Core segments are simplified solids; "
            "windings are envelopes, not detailed conductor geometry.")
            .arg(QString::fromStdString(physical.value().dimensionalSource),
                 QString::fromStdString(physical.value().dimensionalRevision)));
        status->setText(tr("Preliminary CAD generated. Automatic STEP: %1; PDF: %2. Output root: C:/Temp/trdos/135. Physical and manufacturing validation remain separate.")
            .arg(automaticStepSaved ? tr("saved") : tr("FAILED — see log"),
                 automaticPdfSaved ? tr("saved") : tr("FAILED — see log")));
        generateCad->setEnabled(false);
        exportStep->setEnabled(true);
        exportDrawing->setEnabled(true);
        appendLog(tr("[CAD] PRELIMINARY ASSEMBLY BUILT: %1 display solids").arg(items.size()));
    });

    connect(exportStep, &QPushButton::clicked, this,
        [this, status, assemblyModel] {
        QFileDialog dialog(this, tr("Export STEP assembly"),
            projectId_ + QStringLiteral(".step"));
        dialog.setAcceptMode(QFileDialog::AcceptSave);
        dialog.setNameFilter(tr("STEP files (*.step *.stp)"));
        dialog.setDefaultSuffix(QStringLiteral("step"));
        // A widget-backed dialog keeps the workflow accessible to UI automation
        // and behaves consistently across platforms.
        dialog.setOption(QFileDialog::DontUseNativeDialog, true);
        if (dialog.exec() != QDialog::Accepted) return;
        const QString path = dialog.selectedFiles().value(0);
        if (path.isEmpty()) return;
        const auto result = p135::cad::exportAssemblyStep(*assemblyModel, path.toStdString());
        if (!result) {
            status->setText(tr("STEP export failed: %1").arg(QString::fromStdString(result.error().message)));
            appendLog(tr("[CAD] STEP EXPORT FAILED: %1").arg(QString::fromStdString(result.error().message)));
            QMessageBox::warning(this, tr("STEP export failed"), QString::fromStdString(result.error().message));
            return;
        }
        status->setText(tr("B-Rep checked assembly exported to %1").arg(path));
        appendLog(tr("[CAD] STEP EXPORT OK: %1").arg(path));
    });

    connect(exportDrawing, &QPushButton::clicked, this,
        [this, status, physicalGeometry] {
        if (!physicalGeometry->has_value()) {
            status->setText(tr("2D drawing export is blocked: no current physical CAD revision is available."));
            return;
        }
        QFileDialog dialog(this, tr("Export preliminary 2D drawing"),
            projectId_ + QStringLiteral(".dxf"));
        dialog.setAcceptMode(QFileDialog::AcceptSave);
        dialog.setNameFilter(tr("DXF drawings (*.dxf)"));
        dialog.setDefaultSuffix(QStringLiteral("dxf"));
        dialog.setOption(QFileDialog::DontUseNativeDialog, true);
        if (dialog.exec() != QDialog::Accepted) return;
        const QString path = dialog.selectedFiles().value(0);
        if (path.isEmpty()) return;
        const auto result = p135::cad::exportReactorAssemblyDxf(
            **physicalGeometry, path.toStdString());
        if (!result) {
            status->setText(tr("2D drawing export failed: %1").arg(
                QString::fromStdString(result.error().message)));
            appendLog(tr("[CAD] DXF EXPORT FAILED: %1").arg(
                QString::fromStdString(result.error().message)));
            QMessageBox::warning(this, tr("DXF export failed"),
                QString::fromStdString(result.error().message));
            return;
        }
        status->setText(tr("Preliminary three-view DXF drawing exported to %1").arg(path));
        appendLog(tr("[CAD] PRELIMINARY DXF EXPORT OK: %1").arg(path));
    });

    workspace->addWidget(structurePanel);
    workspace->addWidget(viewportPanel);
    workspace->setStretchFactor(0, 0);
    workspace->setStretchFactor(1, 1);
    workspace->setSizes({300, 900});
    layout->addWidget(workspace, 1);

    auto* note = new QLabel(tr(
        "This page does not accept dimensions and does not run independent calculations. "
        "The 3D model will be rebuilt only from the accepted physical geometry of a consistent project revision. "
        "A valid CAD solid alone will not imply magnetic, thermal, dielectric, manufacturing or standards approval."));
    note->setWordWrap(true);
    note->setStyleSheet("color: palette(mid);");
    layout->addWidget(note);
    return page;
}

p135::application::ReactorDesignInputs MainWindow::collectReactorDesignInputs() const
{
    using p135::application::DesignInputOrigin;
    using p135::application::MaterialRevisionSelection;
    p135::application::ReactorDesignInputs inputs;
    inputs.sourceFilterSnapshotId = latestFilterSnapshotId_.toStdString();
    inputs.filterBranchId = latestFilterBranchId_;

    const auto comboValue = [this](const char* name) -> std::optional<std::string> {
        const auto* control = findChild<QComboBox*>(QString::fromLatin1(name));
        if (!control) return std::nullopt;
        const QString value = control->currentData().toString().trimmed();
        if (value.isEmpty()) return std::nullopt;
        return value.toStdString();
    };
    const auto material = [&comboValue](const char* name)
        -> std::optional<MaterialRevisionSelection> {
        const auto revision = comboValue(name);
        if (!revision) return std::nullopt;
        return MaterialRevisionSelection{*revision, DesignInputOrigin::UserSelected};
    };
    const auto positiveDouble = [this](const char* name) -> std::optional<double> {
        const auto* control = findChild<QDoubleSpinBox*>(QString::fromLatin1(name));
        if (!control || control->value() <= 0.0) return std::nullopt;
        return control->value();
    };
    const auto positiveCount = [this](const char* name) -> std::optional<std::uint32_t> {
        const auto* control = findChild<QSpinBox*>(QString::fromLatin1(name));
        if (!control || control->value() <= 0) return std::nullopt;
        return static_cast<std::uint32_t>(control->value());
    };
    const auto lengthMetres = [&positiveDouble](const char* name) -> std::optional<double> {
        const auto value = positiveDouble(name);
        return value ? std::optional<double>{*value * 1.0e-3} : std::nullopt;
    };

    inputs.coreMaterial = material("construction_coreMaterial");
    inputs.conductorMaterial = material("construction_conductorMaterial");
    inputs.insulationMaterial = material("construction_insulationMaterial");
    inputs.coreGeometryFamily = comboValue("construction_coreGeometry");
    if (const auto* lamination = findChild<QLineEdit*>(QStringLiteral("construction_laminationConstruction"));
        lamination && !lamination->text().trimmed().isEmpty())
        inputs.coreLaminationConstruction = lamination->text().trimmed().toStdString();
    if (const auto* source = findChild<QLineEdit*>(QStringLiteral("construction_dimensionalSource"));
        source && !source->text().trimmed().isEmpty())
        inputs.dimensionalSource = source->text().trimmed().toStdString();
    if (const auto* revision = findChild<QLineEdit*>(QStringLiteral("construction_dimensionalRevision"));
        revision && !revision->text().trimmed().isEmpty())
        inputs.dimensionalRevision = revision->text().trimmed().toStdString();
    inputs.conductorShape = comboValue("construction_conductorShape");
    inputs.windingTechnology = comboValue("construction_windingTechnology");
    inputs.coolingMethod = comboValue("construction_cooling");
    inputs.stackingFactor = positiveDouble("construction_stackingFactor");

    if (const auto* control = findChild<QDoubleSpinBox*>(QStringLiteral("construction_ambient"));
        control && control->value() > -50.0)
        inputs.ambientTemperatureK = control->value() + 273.15;
    if (const auto* control = findChild<QDoubleSpinBox*>(
            QStringLiteral("construction_maximumWindingTemperature"));
        control && control->value() > -50.0)
        inputs.maximumWindingTemperatureK = control->value() + 273.15;

    if (const auto* control = findChild<QComboBox*>(QStringLiteral("construction_bobbinUsage"));
        control && control->currentIndex() > 0)
        inputs.bobbinUsed = control->currentData().toBool();
    if (const auto* control = findChild<QLineEdit*>(
            QStringLiteral("construction_bobbinMaterial"))) {
        const QString value = control->text().trimmed();
        if (!value.isEmpty()) inputs.bobbinMaterialId = value.toStdString();
    }

    inputs.turnsPerPhase = positiveCount("construction_turns");
    inputs.turnsPerLayer = positiveCount("construction_turnsPerLayer");
    inputs.windingLayers = positiveCount("construction_layers");
    inputs.windingSections = positiveCount("construction_sections");
    inputs.parallelConductors = positiveCount("construction_parallelConductors");
    if (const auto value = positiveDouble("construction_conductorArea"))
        inputs.conductorCrossSectionM2 = *value * 1.0e-6;
    if (inputs.conductorShape && *inputs.conductorShape == "round") {
        inputs.conductorBareDiameterM = lengthMetres("construction_conductorBareDiameter");
        inputs.conductorInsulatedDiameterM = lengthMetres("construction_conductorInsulatedDiameter");
    } else {
        inputs.conductorWidthM = lengthMetres("construction_conductorWidth");
        inputs.conductorThicknessM = lengthMetres("construction_conductorThickness");
    }
    inputs.conductorInsulationBuildM = lengthMetres("construction_conductorInsulationBuild");
    inputs.turnInsulationBuildM = lengthMetres("construction_turnInsulation");
    inputs.layerInsulationBuildM = lengthMetres("construction_layerInsulation");
    inputs.mainInsulationBuildM = lengthMetres("construction_mainInsulation");
    if (const auto* classControl = findChild<QComboBox*>(QStringLiteral("construction_insulationThermalClass"));
        classControl && !classControl->currentData().toString().isEmpty()) {
        p135::application::InsulationTemperatureReference reference;
        reference.thermalClass = classControl->currentData().toString().toStdString();
        if (const auto* standard = findChild<QLineEdit*>(QStringLiteral("construction_insulationThermalStandard")))
            reference.standardReference = standard->text().trimmed().toStdString();
        if (const auto* referenceTemperature = findChild<QDoubleSpinBox*>(
                QStringLiteral("construction_insulationClassReferenceTemperature"));
            referenceTemperature && referenceTemperature->value() > -50.0)
            reference.referenceTemperatureK = referenceTemperature->value() + 273.15;
        if (const auto* margin = findChild<QDoubleSpinBox*>(QStringLiteral("construction_insulationSafetyMargin")))
            reference.safetyMarginK = margin->value();
        if (const auto* maxTemp = findChild<QDoubleSpinBox*>(QStringLiteral("construction_maximumWindingTemperature"));
            maxTemp && maxTemp->value() > -50.0)
            reference.maximumDesignWindingTemperatureK = maxTemp->value() + 273.15;
        reference.origin = DesignInputOrigin::UserSelected;
        inputs.insulationTemperatureReference = std::move(reference);
    }

    const auto coreLength = [this](const QString& name) -> std::optional<double> {
        const auto* field = findChild<QDoubleSpinBox*>(name);
        if (!field || field->value() <= 0.0) return std::nullopt;
        return field->value() * 1.0e-3;
    };
    const auto collectLengthArray = [&coreLength](const QString& prefix, std::size_t count)
        -> std::optional<std::vector<double>> {
        std::vector<double> values;
        values.reserve(count);
        for (std::size_t i = 0; i < count; ++i) {
            const auto value = coreLength(prefix + QString::number(i + 1));
            if (!value) return std::nullopt;
            values.push_back(*value);
        }
        return values;
    };
    if (const auto values = collectLengthArray(QStringLiteral("construction_limbWidth_"), 3))
        inputs.limbWidthsM = std::array<double, 3>{(*values)[0], (*values)[1], (*values)[2]};
    if (const auto values = collectLengthArray(QStringLiteral("construction_limbDepth_"), 3))
        inputs.limbStackDepthsM = std::array<double, 3>{(*values)[0], (*values)[1], (*values)[2]};
    if (const auto values = collectLengthArray(QStringLiteral("construction_windowWidth_"), 2))
        inputs.windowWidthsM = std::array<double, 2>{(*values)[0], (*values)[1]};
    if (const auto values = collectLengthArray(QStringLiteral("construction_windowHeight_"), 2))
        inputs.windowHeightsM = std::array<double, 2>{(*values)[0], (*values)[1]};
    if (const auto values = collectLengthArray(QStringLiteral("construction_yokeThickness_"), 4))
        inputs.yokeThicknessesM = std::array<double, 4>{(*values)[0], (*values)[1], (*values)[2], (*values)[3]};
    if (const auto values = collectLengthArray(QStringLiteral("construction_yokeDepth_"), 4))
        inputs.yokeDepthsM = std::array<double, 4>{(*values)[0], (*values)[1], (*values)[2], (*values)[3]};

    if (const auto values = collectLengthArray(QStringLiteral("construction_limbCenterSpacing_"), 3))
        inputs.limbCenterSpacingsM = std::array<double, 3>{(*values)[0], (*values)[1], (*values)[2]};
    inputs.coreOverallHeightM = coreLength(QStringLiteral("construction_coreOverallHeight"));
    inputs.coreOverallWidthM = coreLength(QStringLiteral("construction_coreOverallWidth"));

    std::array<std::string, 8> jointConstructions{};
    bool anyJointConstruction = false;
    bool allJointConstructions = true;
    for (std::size_t joint = 0; joint < jointConstructions.size(); ++joint) {
        const auto value = comboValue(("construction_coreJoint_" + std::to_string(joint + 1)).c_str());
        if (value) {
            jointConstructions[joint] = *value;
            anyJointConstruction = true;
        } else allJointConstructions = false;
    }
    if (anyJointConstruction && allJointConstructions)
        inputs.coreJointConstructions = jointConstructions;

    std::array<std::vector<p135::application::CoreJointStepLapPacketInput>, 8> stepLapPackets;
    if (const auto* schedule = findChild<QPlainTextEdit*>(
            QStringLiteral("construction_stepLapPacketSchedule"))) {
        const auto lines = schedule->toPlainText().split('\n');
        for (int lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
            const QString line = lines[lineIndex].trimmed();
            if (line.isEmpty() || line.startsWith('#')) continue;
            const auto columns = line.split(',');
            if (columns.size() != 6) continue; // Confirmation validation reports malformed rows.
            bool jointOk = false;
            const int jointNumber = columns[0].trimmed().toInt(&jointOk);
            if (!jointOk || jointNumber < 1 || jointNumber > 8) continue;
            std::array<double, 4> values{};
            bool validRow = true;
            for (int column = 0; column < 4; ++column) {
                bool ok = false;
                values[static_cast<std::size_t>(column)] = columns[column + 1].trimmed().toDouble(&ok);
                validRow = validRow && ok && std::isfinite(values[static_cast<std::size_t>(column)]);
            }
            const QString direction = columns[5].trimmed().toLower();
            if (!validRow || (direction != QStringLiteral("limb-into-yoke") &&
                              direction != QStringLiteral("yoke-into-limb"))) continue;
            auto& packets = stepLapPackets[static_cast<std::size_t>(jointNumber - 1)];
            packets.push_back({
                values[0] * 1.0e-3, values[1] * 1.0e-3,
                values[2] * 1.0e-3, values[3] * 1.0e-3,
                direction.toStdString()
            });
        }
    }
    inputs.coreJointStepLapPackets = std::move(stepLapPackets);

    for (std::uint32_t limb = 0; limb < 3; ++limb) {
        for (std::uint32_t gap = 0; gap < 3; ++gap) {
            const QString suffix = QString::number(limb + 1) + QStringLiteral("_") + QString::number(gap + 1);
            const auto position = coreLength(QStringLiteral("construction_airGapPosition_") + suffix);
            const auto length = coreLength(QStringLiteral("construction_airGapLength_") + suffix);
            if (position.has_value() != length.has_value()) continue;
            if (position && length)
                inputs.airGaps.push_back({limb, *position, *length});
        }
    }
    for (std::uint32_t yoke = 0; yoke < 4; ++yoke) {
        for (std::uint32_t gap = 0; gap < 3; ++gap) {
            const QString suffix = QString::number(yoke + 1) + QStringLiteral("_") +
                QString::number(gap + 1);
            const auto position = coreLength(QStringLiteral("construction_yokeAirGapPosition_") + suffix);
            const auto length = coreLength(QStringLiteral("construction_yokeAirGapLength_") + suffix);
            if (position && length)
                inputs.airGaps.push_back({yoke + 3, *position, *length});
        }
    }

    const auto collectPhaseArray = [&collectLengthArray](const QString& prefix)
        -> std::optional<std::array<double, 3>> {
        const auto values = collectLengthArray(prefix, 3);
        if (!values) return std::nullopt;
        return std::array<double, 3>{(*values)[0], (*values)[1], (*values)[2]};
    };
    inputs.bobbinInnerWidthsM = collectPhaseArray(QStringLiteral("construction_bobbinInnerWidth_"));
    inputs.bobbinInnerHeightsM = collectPhaseArray(QStringLiteral("construction_bobbinInnerHeight_"));
    inputs.bobbinAxialLengthsM = collectPhaseArray(QStringLiteral("construction_bobbinAxialLength_"));
    inputs.bobbinWallThicknessesM = collectPhaseArray(QStringLiteral("construction_bobbinWallThickness_"));
    inputs.bobbinFlangeThicknessesM = collectPhaseArray(QStringLiteral("construction_bobbinFlangeThickness_"));
    inputs.bobbinFlangeOuterWidthsM = collectPhaseArray(QStringLiteral("construction_bobbinFlangeOuterWidth_"));
    inputs.bobbinFlangeOuterHeightsM = collectPhaseArray(QStringLiteral("construction_bobbinFlangeOuterHeight_"));
    inputs.bobbinWindingClearancesM = collectPhaseArray(QStringLiteral("construction_bobbinWindingClearance_"));
    inputs.windingRadialBuildsM = collectPhaseArray(QStringLiteral("construction_windingRadialBuild_"));
    inputs.windingAxialHeightsM = collectPhaseArray(QStringLiteral("construction_windingAxialHeight_"));
    inputs.windingEndClearancesM = collectPhaseArray(QStringLiteral("construction_windingEndClearance_"));

    if (const auto* count = findChild<QSpinBox*>(QStringLiteral("construction_coolingDuctCount"));
        count && count->value() > 0) {
        inputs.radialCoolingDuctCount = static_cast<std::uint32_t>(count->value());
        for (int duct = 0; duct < count->value(); ++duct) {
            const QString suffix = QString::number(duct + 1);
            const auto position = coreLength(QStringLiteral("construction_coolingDuctPosition_") + suffix);
            const auto thickness = coreLength(QStringLiteral("construction_coolingDuctThickness_") + suffix);
            const auto* materialRevision = findChild<QLineEdit*>(
                QStringLiteral("construction_coolingDuctSpacerRevision_") + suffix);
            const auto* airflow = findChild<QComboBox*>(
                QStringLiteral("construction_coolingDuctAirflow_") + suffix);
            if (position && thickness && materialRevision && airflow &&
                !materialRevision->text().trimmed().isEmpty() && !airflow->currentData().toString().isEmpty()) {
                inputs.coolingDucts.push_back({
                    *position, *thickness,
                    materialRevision->text().trimmed().toStdString(),
                    airflow->currentData().toString().toStdString()
                });
            }
        }
    }

    std::array<std::uint32_t, 3> gapCounts{};
    std::array<double, 3> totalGaps{};
    bool hasGapCount = false;
    bool hasTotalGap = false;
    bool completeGapCounts = true;
    bool completeTotalGaps = true;
    for (std::size_t i = 0; i < 3; ++i) {
        const QString suffix = QString::number(i + 1);
        const auto* count = findChild<QSpinBox*>(QStringLiteral("construction_airGapCount_") + suffix);
        const auto total = coreLength(QStringLiteral("construction_totalAirGap_") + suffix);
        if (count) {
            gapCounts[i] = static_cast<std::uint32_t>(count->value());
            hasGapCount = hasGapCount || count->value() > 0;
        } else completeGapCounts = false;
        if (total) {
            totalGaps[i] = *total;
            hasTotalGap = true;
        } else completeTotalGaps = false;
    }
    if (hasGapCount && completeGapCounts) inputs.airGapCountPerLimb = gapCounts;
    if (hasTotalGap && completeTotalGaps) inputs.totalAirGapLengthPerLimbM = totalGaps;
    return inputs;
}

void MainWindow::markConstructionInputsChanged()
{
    if (loadingProjectSettings_) return;
    constructionInputsDirty_ = true;
    acceptedConstructionInputs_.reset();
    if (auto* generate = findChild<QPushButton*>(QStringLiteral("generatePreliminaryCad")))
        generate->setEnabled(true);
    if (auto* exportStep = findChild<QPushButton*>(QStringLiteral("exportCadStep")))
        exportStep->setEnabled(false);
    if (auto* exportDrawing = findChild<QPushButton*>(QStringLiteral("generateCadDrawings")))
        exportDrawing->setEnabled(false);
    if (auto* viewport = findChild<CadViewportWidget*>(QStringLiteral("cadViewportWidget")))
        viewport->setDisplayItems({});
    if (auto* cadStatus = findChild<QLabel*>(QStringLiteral("cadGenerationStatus")))
        cadStatus->setText(tr("CAD preview invalidated: Construction inputs changed. Confirm the new revision and regenerate."));
    if (auto* tree = findChild<QTreeWidget*>(QStringLiteral("cadAssemblyTree"))) {
        std::function<void(QTreeWidgetItem*)> markStale;
        markStale = [&markStale](QTreeWidgetItem* item) {
            item->setText(1, QStringLiteral("STALE"));
            for (int i = 0; i < item->childCount(); ++i) markStale(item->child(i));
        };
        for (int i = 0; i < tree->topLevelItemCount(); ++i)
            markStale(tree->topLevelItem(i));
    }
    if (auto* state = findChild<QLabel*>(QStringLiteral("constructionInputState"))) {
        state->setText(tr("CONSTRUCTION INPUTS CHANGED — previous design-input capture is stale. Run the project calculation to create a new linked snapshot."));
        state->setStyleSheet("color: #a66b00; font-weight: 600;");
    }
    const QString objectName = sender() ? sender()->objectName() : QString{};
    if (objectName == QStringLiteral("construction_ambient") ||
        objectName == QStringLiteral("construction_cooling")) {
        QSettings settings;
        settings.setValue(QStringLiteral("projects/") + projectId_ +
            QStringLiteral("/construction/") + objectName + QStringLiteral("_source"),
            QStringLiteral("MANUAL"));
    }
    QString stage;
    if (objectName == QStringLiteral("construction_coreMaterial")) stage = QStringLiteral("input.core.material");
    else if (objectName == QStringLiteral("construction_coreGeometry")) stage = QStringLiteral("input.core.geometry");
    else if (objectName == QStringLiteral("construction_stackingFactor")) stage = QStringLiteral("input.core.stacking_factor");
    else if (objectName == QStringLiteral("construction_conductorMaterial")) stage = QStringLiteral("input.winding.conductor_material");
    else if (objectName == QStringLiteral("construction_conductorShape") ||
             objectName == QStringLiteral("construction_conductorArea") ||
             objectName == QStringLiteral("construction_conductorWidth") ||
             objectName == QStringLiteral("construction_conductorThickness") ||
             objectName == QStringLiteral("construction_parallelConductors"))
        stage = QStringLiteral("input.winding.conductor_geometry");
    else if (objectName == QStringLiteral("construction_turns") ||
             objectName == QStringLiteral("construction_turnsPerLayer") ||
             objectName == QStringLiteral("construction_layers"))
        stage = QStringLiteral("input.winding.turns");
    else if (objectName == QStringLiteral("construction_insulationMaterial") ||
             objectName == QStringLiteral("construction_turnInsulation") ||
             objectName == QStringLiteral("construction_layerInsulation") ||
             objectName == QStringLiteral("construction_mainInsulation"))
        stage = QStringLiteral("input.winding.insulation");
    else if (objectName == QStringLiteral("construction_cooling")) stage = QStringLiteral("input.thermal.cooling");
    else if (objectName == QStringLiteral("construction_ambient")) stage = QStringLiteral("input.thermal.ambient");
    else if (objectName == QStringLiteral("construction_maximumWindingTemperature"))
        stage = QStringLiteral("input.thermal.maximum_winding_temperature");
    else if (objectName == QStringLiteral("construction_windingTechnology"))
        stage = QStringLiteral("input.winding.technology");
    else if (objectName == QStringLiteral("construction_bobbinMaterial") ||
             objectName == QStringLiteral("construction_bobbinUsage"))
        stage = QStringLiteral("input.winding.bobbin");
    else if (objectName.startsWith(QStringLiteral("construction_limbWidth_")) ||
             objectName.startsWith(QStringLiteral("construction_limbDepth_")) ||
             objectName.startsWith(QStringLiteral("construction_windowWidth_")) ||
             objectName.startsWith(QStringLiteral("construction_windowHeight_")) ||
             objectName.startsWith(QStringLiteral("construction_yokeThickness_")) ||
             objectName.startsWith(QStringLiteral("construction_yokeDepth_")) ||
             objectName.startsWith(QStringLiteral("construction_limbCenterSpacing_")) ||
             objectName.startsWith(QStringLiteral("construction_coreOverall")) ||
             objectName.startsWith(QStringLiteral("construction_airGapCount_")) ||
             objectName.startsWith(QStringLiteral("construction_totalAirGap_")) ||
             objectName.startsWith(QStringLiteral("construction_airGapPosition_")) ||
             objectName.startsWith(QStringLiteral("construction_airGapLength_")) ||
             objectName.startsWith(QStringLiteral("construction_yokeAirGapPosition_")) ||
             objectName.startsWith(QStringLiteral("construction_yokeAirGapLength_")))
        stage = QStringLiteral("input.core.physical_geometry");
    else if (objectName.startsWith(QStringLiteral("construction_coreJoint_")))
        stage = QStringLiteral("input.core.joints");
    else if (objectName.startsWith(QStringLiteral("construction_coolingDuct")) ||
             objectName == QStringLiteral("construction_coolingDuctCount"))
        stage = QStringLiteral("input.thermal.cooling_ducts");
    else if (objectName.startsWith(QStringLiteral("construction_bobbin")) ||
             objectName.startsWith(QStringLiteral("construction_windingRadial")) ||
             objectName.startsWith(QStringLiteral("construction_windingAxial")) ||
             objectName.startsWith(QStringLiteral("construction_windingEnd")))
        stage = QStringLiteral("input.winding.physical_geometry");
    else if (objectName == QStringLiteral("construction_insulationThermalClass") ||
             objectName == QStringLiteral("construction_insulationThermalStandard") ||
             objectName == QStringLiteral("construction_insulationClassReferenceTemperature") ||
             objectName == QStringLiteral("construction_insulationSafetyMargin"))
        stage = QStringLiteral("input.thermal.insulation_reference");
    if (!stage.isEmpty()) {
        const auto plan = p135::application::planReactorRecalculation({stage.toStdString()});
        if (plan) {
            QStringList stages;
            for (const auto& id : plan.value().orderedStages)
                stages.push_back(QString::fromStdString(id));
            appendLog(tr("Dependency plan for %1: %2. Execution is not yet connected.")
                .arg(stage, stages.join(QStringLiteral(" → "))));
        } else {
            appendLog(tr("Cannot build dependency plan: %1")
                .arg(QString::fromStdString(plan.error().message)));
        }
    }
    appendLog(tr("Construction input changed; previous captured design inputs are stale. Physical sizing has not been recalculated."));
}

void MainWindow::calculateFilter()
{
    if (resultsTable_) resultsTable_->setProperty("calculationRunAvailable", false);
    if (auto* confirmation = findChild<QLabel*>(QStringLiteral("calculationDesignConfirmationStatus")))
        confirmation->setText(tr("Calculation changed — confirmation is required again."));
    if (!nominalVoltage_ || !fundamentalFrequency_ || !targetReactivePower_ ||
        !filterConnection_ || !tuningHarmonicOrder_ || !qualityFactor_ ||
        !resultsTable_ || !resultsNote_) {
        return;
    }

    if (auto* output = findChild<QLabel*>(QStringLiteral("idealSizingResult"))) {
        output->setText(tr("Ideal area-product estimate: awaiting a successful calculation."));
    }
    if (auto* table = findChild<QTableWidget*>(QStringLiteral("constructionDesignTable"))) {
        for (int row = 0; row < table->rowCount(); ++row) {
            const auto* parameter = table->item(row, 0);
            if (parameter && (parameter->text() == tr("Minimum gross area product Ae×Aw") ||
                              parameter->text() == tr("Equivalent square area scale") ||
                              parameter->text() == tr("Estimated gross core area Ae") ||
                              parameter->text() == tr("Estimated effective core area") ||
                              parameter->text() == tr("Estimated window area Aw") ||
                              parameter->text() == tr("Ideal minimum turns per reactor") ||
                              parameter->text() == tr("Ideal total equivalent air gap"))) {
                table->setItem(row, 1, new QTableWidgetItem(tr("NOT CALCULATED")));
                table->setItem(row, 3, new QTableWidgetItem(tr("NOT_VERIFIED")));
            }
        }
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

    // Capture synthesized electrical excitation as the first executable pipeline stage.
    // Dependent physical stages are invalidated and remain NOT_VERIFIED until typed adapters exist.
    const auto excitationArtifact =
        p135::application::makeFilterExcitationStageArtifact(result.value());
    if (!excitationArtifact) {
        appendLog(tr("Pipeline stage filter.excitation NOT_VERIFIED: %1")
            .arg(QString::fromStdString(excitationArtifact.error().message)));
    } else {
        const auto excitationPlan =
            p135::application::planReactorRecalculation({"input.filter.excitation"});
        if (!excitationPlan) {
            appendLog(tr("Pipeline planning failed for filter.excitation: %1")
                .arg(QString::fromStdString(excitationPlan.error().message)));
        } else {
            const auto stagePosition = std::find(
                excitationPlan.value().orderedStages.begin(),
                excitationPlan.value().orderedStages.end(),
                std::string("filter.excitation"));
            if (stagePosition == excitationPlan.value().orderedStages.end()) {
                appendLog(tr("Pipeline plan did not contain filter.excitation; stage remains NOT_VERIFIED."));
            } else {
                const p135::application::ReactorRecalculationPlan supportedPlan{{
                    "filter.excitation"
                }};
                const auto execution = p135::application::executeReactorDesignPlan(
                    supportedPlan, reactorDesignExecutionState_,
                    [&excitationArtifact](const std::string& stageId)
                        -> p135::Result<std::string> {
                        if (stageId != "filter.excitation")
                            return p135::Result<std::string>::failure({
                                p135::ErrorCode::InvalidInput,
                                "No typed adapter is registered for this reactor design stage.", {}
                            });
                        return p135::Result<std::string>::success(excitationArtifact.value());
                    });
                if (!execution) {
                    appendLog(tr("Pipeline execution setup failed: %1")
                        .arg(QString::fromStdString(execution.error().message)));
                } else if (!execution.value().committed) {
                    appendLog(tr("Pipeline stage filter.excitation NOT_VERIFIED: %1")
                        .arg(QString::fromStdString(execution.value().failureMessage)));
                } else {
                    // The graph plan is the authoritative invalidation list. Never
                    // retain stale downstream physical artifacts after new excitation.
                    auto& accepted = reactorDesignExecutionState_.accepted;
                    std::size_t invalidatedStages = 0;
                    for (const auto& stageId : excitationPlan.value().orderedStages) {
                        if (stageId == "filter.excitation") continue;
                        accepted.stages[stageId] = {
                            p135::application::ReactorStageStatus::NotVerified, {}, {}
                        };
                        ++invalidatedStages;
                    }
                    accepted.verified = false;
                    appendLog(tr("Pipeline revision %1 committed: filter.excitation VALID; %2 dependent physical stages NOT_VERIFIED.")
                        .arg(static_cast<qulonglong>(accepted.number))
                        .arg(static_cast<qulonglong>(invalidatedStages)));
                }
            }
        }
    }

    // The construction page is a calculated output first. Resolve material
    // candidates from exact immutable database revisions after electrical synthesis.
    // Existing user selections are treated as FIXED and are never overwritten.
    QStringList automaticMaterialNotes;
    if (materialStore_) {
        const auto selectDatabaseCandidate = [this, &automaticMaterialNotes](
            const char* objectName, const std::string& recordType,
            const std::string& preferredMaterialId, const QString& role) {
            auto* combo = findChild<QComboBox*>(QString::fromLatin1(objectName));
            if (!combo || !combo->currentData().toString().isEmpty()) return;
            p135::infrastructure::sqlite::MaterialCatalogFilter filter;
            filter.recordType = recordType;
            filter.pageSize = 500;
            const auto revisions = materialStore_->searchLatestRevisions(filter);
            if (!revisions) {
                automaticMaterialNotes.push_back(role + tr(": database query failed"));
                return;
            }
            const auto found = std::find_if(revisions.value().begin(), revisions.value().end(),
                [&preferredMaterialId](const auto& revision) {
                    return revision.materialId == preferredMaterialId;
                });
            if (found == revisions.value().end()) {
                automaticMaterialNotes.push_back(role + tr(": compatible catalog revision not found"));
                return;
            }
            const QSignalBlocker blocker(combo);
            const QString revisionId = QString::fromStdString(found->revisionId);
            int index = combo->findData(revisionId);
            if (index < 0) {
                combo->addItem(QString::fromStdString(found->materialId) +
                    QStringLiteral(" — r%1 [AUTO]").arg(found->revisionNumber), revisionId);
                index = combo->count() - 1;
            }
            combo->setCurrentIndex(index);
            automaticMaterialNotes.push_back(role + QStringLiteral(": ") +
                QString::fromStdString(found->materialId) +
                QStringLiteral(" / ") + revisionId);
        };

        // M235-35A is the starter catalogue's only electrical-steel record
        // carrying manufacturer maximum-loss points explicitly at 50 Hz.
        // It has no continuous B-H curve, so this is a material candidate,
        // not a verified magnetic design release.
        if (std::abs(fundamentalFrequency_->value() - 50.0) < 1.0e-6) {
            selectDatabaseCandidate("construction_coreMaterial", "core-material",
                "MAT-TK-POWERCORE-M235-35A", tr("50 Hz electrical-steel candidate"));
        } else {
            automaticMaterialNotes.push_back(tr("Core material candidate: no starter steel revision has a loss rating at %1 Hz; no 50 Hz data reused.")
                .arg(fundamentalFrequency_->value(), 0, 'g', 6));
        }
        selectDatabaseCandidate("construction_conductorMaterial", "conductor-material",
            "MAT-CU-C11000", tr("Conductor material candidate"));
        selectDatabaseCandidate("construction_insulationMaterial", "insulation-material",
            "MAT-DUPONT-NOMEX-410-50UM", tr("Turn insulation candidate"));
    }
    if (auto* constructionState = findChild<QLabel*>(QStringLiteral("constructionInputState"))) {
        if (!automaticMaterialNotes.isEmpty()) {
            constructionState->setText(
                tr("IDEAL MATERIAL CANDIDATES SELECTED FROM DATABASE\n%1\n"
                   "Physical sizing remains NOT_VERIFIED: the selected electrical-steel revision "
                   "has 50 Hz maximum-loss points but no continuous B-H curve; the database does "
                   "not yet provide a complete three-limb core stock geometry, winding window, "
                   "thermal boundary conditions and insulation coordination needed to calculate "
                   "dimensions, turns and conductor section without assumptions.")
                    .arg(automaticMaterialNotes.join(QStringLiteral("\n"))));
            constructionState->setStyleSheet("color: #a66b00; font-weight: 600;");
        }
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

    // Resolve winding-current component IDs by element name. Branch-result vector
    // offsets are not stable when Thevenin source elements are inserted.
    std::array<p135::circuit::ComponentId, 3> filterCurrentComponents{};
    const std::array<std::string, 3> windingSuffixes =
        specification.network.connection == p135::filter::Connection::Wye
            ? std::array<std::string, 3>{"L1", "L2", "L3"}
            : std::array<std::string, 3>{"L1-L2", "L2-L3", "L3-L1"};
    for (std::size_t phase = 0; phase < windingSuffixes.size(); ++phase) {
        const std::string expectedName =
            result.value().branches.front().name + " " + windingSuffixes[phase] + " R";
        const auto element = std::find_if(circuitResult.value().elements.begin(),
            circuitResult.value().elements.end(), [&expectedName](const auto& candidate) {
                return candidate.kind == p135::circuit::ElementKind::Resistor &&
                    candidate.name == expectedName;
            });
        if (element == circuitResult.value().elements.end()) {
            const QString message = tr("Cannot resolve filter winding-current component: %1")
                .arg(QString::fromStdString(expectedName));
            appendLog(message);
            resultsNote_->setText(message);
            status_->setText(tr("Plot data mapping failed"));
            return;
        }
        filterCurrentComponents[phase] = element->id;
    }
    const auto maximumFilterBranchRmsCurrent =
        [&filterCurrentComponents](const p135::circuit::ElectricalResult& electrical)
            -> std::optional<double> {
        double maximum = 0.0;
        for (const auto componentId : filterCurrentComponents) {
            const auto branch = std::find_if(electrical.branches.begin(),
                electrical.branches.end(), [componentId](const auto& candidate) {
                    return candidate.component == componentId;
                });
            if (branch == electrical.branches.end()) return std::nullopt;
            maximum = std::max(maximum, std::abs(branch->rmsCurrent));
        }
        return maximum;
    };

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
            const auto lines = coreBhCurve_->toPlainText().split(QRegularExpression("[\\r\n]+"),
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
    std::size_t threeLimbFailedFrequencyCount = 0;
    QString firstThreeLimbSweepFailure;
    const auto filterBranchCurrent = [&circuitResult](const p135::circuit::ElectricalResult& electrical,
                                                       const std::string& nameSuffix)
        -> std::optional<std::complex<double>> {
        std::complex<double> totalCurrent{};
        bool found = false;
        for (const auto& element : circuitResult.value().elements) {
            if (element.kind != p135::circuit::ElementKind::Resistor ||
                !element.name.ends_with(nameSuffix))
                continue;
            const auto branch = std::find_if(electrical.branches.begin(), electrical.branches.end(),
                [&element](const p135::circuit::BranchResult& candidate) {
                    return candidate.component == element.id;
                });
            if (branch == electrical.branches.end()) continue;
            totalCurrent += branch->rmsCurrent;
            found = true;
        }
        if (!found) return std::nullopt;
        return totalCurrent;
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
                ++threeLimbFailedFrequencyCount;
                const QString failure = tr("%1 Hz: %2")
                    .arg(point.frequency.si(), 0, 'g', 6)
                    .arg(QString::fromStdString(sampledCycle.error().message));
                if (firstThreeLimbSweepFailure.isEmpty())
                    firstThreeLimbSweepFailure = failure;
                threeLimbSamplingMetadata.push_back({
                    point.frequency.si(), 0.0, 0.0, 0.0, 0.0
                });
            } else if (!sampledCycle.value().converged) {
                ++threeLimbFailedFrequencyCount;
                if (firstThreeLimbSweepFailure.isEmpty()) {
                    firstThreeLimbSweepFailure = tr("%1 Hz: peak |B| did not converge to 0.1%% by %2 samples (last relative change %3%%).")
                        .arg(point.frequency.si(), 0, 'g', 6)
                        .arg(static_cast<qulonglong>(sampledCycle.value().samplesUsed))
                        .arg(sampledCycle.value().relativePeakChange * 100.0, 0, 'g', 5);
                }
                threeLimbSamplingMetadata.push_back({
                    point.frequency.si(),
                    static_cast<double>(sampledCycle.value().samplesUsed),
                    sampledCycle.value().relativePeakChange,
                    0.0,
                    sampledCycle.value().withinBhCurveRange ? 1.0 : 0.0
                });
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
    if (magneticCircuitBase && !magneticSweepValid)
        magneticFluxDensityPoints.clear();
    if (magneticSweepExtrapolated)
        magneticModelWarning = tr("Magnetic B-H sweep EXTRAPOLATED_NOT_VERIFIED: μ0 differential-permeability continuation was used beyond the measured B-H curve.");
    if (threeLimbMagneticBase && threeLimbSweepValid &&
        threeLimbFluxDensitySweep.empty()) {
        threeLimbSweepValid = false;
        threeLimbModelWarning = threeLimbFailedFrequencyCount > 0
            ? tr("Coupled three-limb B-H sweep NOT_VERIFIED: all %1 frequency points failed; first failure: %2")
                .arg(static_cast<qulonglong>(threeLimbFailedFrequencyCount))
                .arg(firstThreeLimbSweepFailure)
            : tr("Coupled three-limb B-H sweep NOT_VERIFIED: no frequency points were produced.");
    } else if (threeLimbMagneticBase && threeLimbFailedFrequencyCount > 0) {
        threeLimbModelWarning = tr("Coupled three-limb B-H sweep PARTIAL: %1 frequency points failed; %2 successful points retained. First failure: %3")
            .arg(static_cast<qulonglong>(threeLimbFailedFrequencyCount))
            .arg(static_cast<qulonglong>(threeLimbFluxDensitySweep.size()))
            .arg(firstThreeLimbSweepFailure);
    }
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
    const bool acceptedPhysicalMagneticCandidate =
        acceptedConstructionInputs_.has_value() && !constructionInputsDirty_ &&
        materialStore_ && acceptedConstructionInputs_->coreMaterial.has_value() &&
        acceptedConstructionInputs_->stackingFactor.has_value() &&
        acceptedConstructionInputs_->turnsPerPhase.has_value() &&
        acceptedConstructionInputs_->dimensionalSource.has_value() &&
        acceptedConstructionInputs_->dimensionalRevision.has_value() &&
        acceptedConstructionInputs_->sourceFilterSnapshotId == latestFilterSnapshotId_.toStdString() &&
        acceptedConstructionInputs_->filterBranchId == latestFilterBranchId_;
    if (!hasSourceImpedance && (suppliedHarmonics > 0 || hasLoadCurrentInjection)) {
        advancedWarning = tr("Harmonic analysis NOT_VERIFIED: enter source R and/or L from a network study.");
    } else if (hasSourceImpedance &&
               (suppliedHarmonics > 0 || hasLoadCurrentInjection || threeLimbMagneticBase.has_value() || acceptedPhysicalMagneticCandidate) &&
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

    // Revision-bound magnetic analysis uses the exact accepted Construction
    // core dimensions and exact selected material revision. It is deliberately
    // separate from the legacy manual Ae/le diagnostic controls above.
    if (acceptedPhysicalMagneticCandidate && harmonicResult) {
        const auto& accepted = *acceptedConstructionInputs_;
        const auto physicalCore = p135::application::makeThreeLimbCorePhysicalGeometry(accepted);
        if (!physicalCore) {
            appendLog(tr("[MAGNETIC] NOT_VERIFIED — accepted Construction core geometry: %1")
                .arg(QString::fromStdString(physicalCore.error().message)));
        } else {
            p135::application::PhysicalFilterReactorMagneticRequest magneticRequest;
            magneticRequest.physicalCore = physicalCore.value();
            magneticRequest.stackingFactor = *accepted.stackingFactor;
            magneticRequest.dimensionalSource = *accepted.dimensionalSource;
            magneticRequest.dimensionalRevision = *accepted.dimensionalRevision;
            magneticRequest.filterMagnetic.filterBranchId = accepted.filterBranchId;
            magneticRequest.filterMagnetic.harmonicAnalysis = *harmonicResult;
            magneticRequest.filterMagnetic.magnetic.turns = {
                *accepted.turnsPerPhase, *accepted.turnsPerPhase, *accepted.turnsPerPhase
            };
            magneticRequest.filterMagnetic.relativePeakTolerance = 1.0e-3;
            magneticRequest.filterMagnetic.maximumSamples = 576;
            const auto magnetic = p135::infrastructure::sqlite::
                analyzePhysicalFilterReactorMagneticCycleWithMaterialRevision(
                    *materialStore_, accepted.coreMaterial->revisionId, magneticRequest);
            if (!magnetic) {
                appendLog(tr("[MAGNETIC] NOT_VERIFIED — Construction revision %1; material %2: %3")
                    .arg(QString::fromStdString(accepted.dimensionalRevision.value()))
                    .arg(QString::fromStdString(accepted.coreMaterial->revisionId))
                    .arg(QString::fromStdString(magnetic.error().message)));
            } else {
                const auto& cycle = magnetic.value().magneticAnalysis.magneticAnalysis.magneticCycle;
                double maximumFluxDensity = 0.0;
                for (const auto& density : cycle.peakAbsoluteFluxDensity)
                    maximumFluxDensity = std::max(maximumFluxDensity, density.si());
                const QString magneticState = cycle.converged
                    ? tr("PASS") : tr("NOT_VERIFIED — peak flux density did not converge to the requested tolerance");
                appendLog(tr("[MAGNETIC] %1 — Construction %2; material %3; max |B|=%4 T; samples=%5; relative peak change=%6.")
                    .arg(magneticState)
                    .arg(QString::fromStdString(accepted.dimensionalRevision.value()))
                    .arg(QString::fromStdString(magnetic.value().materialRevision.revisionId))
                    .arg(maximumFluxDensity, 0, 'g', 7)
                    .arg(static_cast<qulonglong>(cycle.samplesUsed))
                    .arg(cycle.relativePeakChange, 0, 'g', 6));
            }
        }
    } else if (acceptedPhysicalMagneticCandidate) {
        appendLog(tr("[MAGNETIC] NOT_VERIFIED — harmonic branch currents are unavailable; no magnetic result was produced."));
    }

    // First-pass ideal area-product bound. Inputs are explicit user criteria.
    // When available, branch harmonic RMS currents are combined by RSS and a
    // triangle-inequality peak envelope is used; otherwise the fundamental-only
    // estimate is explicitly labelled. This is not a thermal or geometry solver.
    if (auto* output = findChild<QLabel*>(QStringLiteral("idealSizingResult"))) {
        if (auto* table = findChild<QTableWidget*>(QStringLiteral("constructionDesignTable"))) {
            for (int row = 0; row < table->rowCount(); ++row) {
                const auto* parameter = table->item(row, 0);
                if (!parameter) continue;
                if (parameter->text() == tr("Minimum gross area product Ae×Aw") ||
                    parameter->text() == tr("Equivalent square area scale") ||
                    parameter->text() == tr("Estimated gross core area Ae") ||
                    parameter->text() == tr("Estimated effective core area") ||
                    parameter->text() == tr("Estimated window area Aw") ||
                    parameter->text() == tr("Ideal minimum turns per reactor") ||
                    parameter->text() == tr("Ideal total equivalent air gap")) {
                    table->setItem(row, 1, new QTableWidgetItem(tr("NOT CALCULATED")));
                    table->setItem(row, 3, new QTableWidgetItem(tr("NOT_VERIFIED")));
                }
            }
        }
        auto* bmax = findChild<QDoubleSpinBox*>(QStringLiteral("idealSizing_Bmax"));
        auto* density = findChild<QDoubleSpinBox*>(QStringLiteral("idealSizing_J"));
        auto* utilization = findChild<QDoubleSpinBox*>(QStringLiteral("idealSizing_Ku"));
        auto* stacking = findChild<QDoubleSpinBox*>(QStringLiteral("idealSizing_Ks"));
        auto* areaRatio = findChild<QDoubleSpinBox*>(QStringLiteral("idealSizing_AwAe"));
        if (bmax && density && utilization && stacking && areaRatio &&
            !result.value().branches.empty() && bmax->value() > 0.0 &&
            density->value() > 0.0 && utilization->value() > 0.0 &&
            stacking->value() > 0.0 && areaRatio->value() > 0.0) {
            const auto branchIt = std::find_if(result.value().branches.begin(),
                result.value().branches.end(), [this](const auto& branch) {
                    return branch.id == latestFilterBranchId_;
                });
            const auto& branch = branchIt != result.value().branches.end()
                ? *branchIt : result.value().branches.front();
            const double phaseVoltage = branch.phaseVoltage.si();
            const double inductance = branch.inductance.si();
            const double branchPower = branch.targetReactivePower.si();
            if (phaseVoltage > 0.0 && inductance > 0.0 && branchPower > 0.0) {
                const double fundamentalRmsCurrent = branchPower / (3.0 * phaseVoltage);
                double rmsCurrent = fundamentalRmsCurrent;
                double peakCurrentBound = std::sqrt(2.0) * fundamentalRmsCurrent;
                bool harmonicSpectrumUsed = false;
                if (harmonicResult) {
                    std::array<double, 3> phaseRmsSquared{};
                    std::array<double, 3> phasePeakEnvelope{};
                    for (const auto& point : harmonicResult->branchPoints) {
                        if (point.branchId != branch.id) continue;
                        for (std::size_t phase = 0; phase < 3; ++phase) {
                            const double magnitude = std::abs(point.phaseCurrentRms[phase]);
                            phaseRmsSquared[phase] += magnitude * magnitude;
                            phasePeakEnvelope[phase] += std::sqrt(2.0) * magnitude;
                        }
                    }
                    const double spectrumRms = std::sqrt(*std::max_element(
                        phaseRmsSquared.begin(), phaseRmsSquared.end()));
                    const double spectrumPeakBound = *std::max_element(
                        phasePeakEnvelope.begin(), phasePeakEnvelope.end());
                    if (spectrumRms > 0.0 && spectrumPeakBound > 0.0) {
                        rmsCurrent = spectrumRms;
                        peakCurrentBound = spectrumPeakBound;
                        harmonicSpectrumUsed = true;
                    }
                }
                const auto estimate = p135::application::calculateIdealReactorAreaProduct({
                    inductance, peakCurrentBound, rmsCurrent,
                    bmax->value(), density->value() * 1.0e6,
                    utilization->value() / 100.0, stacking->value() / 100.0,
                    areaRatio->value()
                });
                if (estimate) {
                    const auto& value = estimate.value();
                    if (auto* table = findChild<QTableWidget*>(QStringLiteral("constructionDesignTable"))) {
                        for (int row = 0; row < table->rowCount(); ++row) {
                            const auto* parameter = table->item(row, 0);
                            if (!parameter) continue;
                            if (parameter->text() == tr("Minimum gross area product Ae×Aw")) {
                                table->setItem(row, 1, new QTableWidgetItem(
                                    QString::number(value.minimumGrossAreaProductM4 * 1.0e12, 'g', 6)));
                                table->setItem(row, 3, new QTableWidgetItem(tr("IDEAL LOWER BOUND")));
                            } else if (parameter->text() == tr("Equivalent square area scale")) {
                                table->setItem(row, 1, new QTableWidgetItem(
                                    QString::number(value.equivalentSquareSideM * 1.0e3, 'g', 6) + tr(" mm")));
                                table->setItem(row, 3, new QTableWidgetItem(tr("REFERENCE ONLY")));
                            } else if (parameter->text() == tr("Estimated gross core area Ae")) {
                                table->setItem(row, 1, new QTableWidgetItem(
                                    QString::number(value.estimatedGrossCoreAreaM2 * 1.0e6, 'g', 6) + tr(" mm²")));
                                table->setItem(row, 3, new QTableWidgetItem(tr("IDEALIZED")));
                            } else if (parameter->text() == tr("Estimated effective core area")) {
                                table->setItem(row, 1, new QTableWidgetItem(
                                    QString::number(value.estimatedEffectiveCoreAreaM2 * 1.0e6, 'g', 6) + tr(" mm²")));
                                table->setItem(row, 3, new QTableWidgetItem(tr("IDEALIZED")));
                            } else if (parameter->text() == tr("Estimated window area Aw")) {
                                table->setItem(row, 1, new QTableWidgetItem(
                                    QString::number(value.estimatedWindowAreaM2 * 1.0e6, 'g', 6) + tr(" mm²")));
                                table->setItem(row, 3, new QTableWidgetItem(tr("IDEALIZED")));
                            } else if (parameter->text() == tr("Ideal minimum turns per reactor")) {
                                table->setItem(row, 1, new QTableWidgetItem(
                                    QString::number(static_cast<qulonglong>(value.idealMinimumTurns))));
                                table->setItem(row, 3, new QTableWidgetItem(tr("IDEALIZED")));
                            } else if (parameter->text() == tr("Ideal total equivalent air gap")) {
                                table->setItem(row, 1, new QTableWidgetItem(
                                    QString::number(value.idealTotalAirGapM * 1.0e3, 'g', 6) + tr(" mm")));
                                table->setItem(row, 3, new QTableWidgetItem(tr("IDEALIZED")));
                            }
                        }
                    }
                    const QString sizingHeader = harmonicSpectrumUsed
                        ? tr("IDEAL LOWER BOUND (harmonic RMS + conservative peak envelope)\\n")
                        : tr("IDEAL LOWER BOUND (fundamental current only; harmonic spectrum unavailable)\\n");
                    output->setText(sizingHeader + tr(
                        "Gross Ae×Aw ≥ %1 mm⁴\\n"
                        "Equivalent square side ≥ %2 mm\\n"
                        "Stored peak energy = %3 J\\n"
                        "Design current: Irms=%8 A; Ipeak bound=%9 A\\n"
                        "Gross Ae=%10 mm²; effective Ae=%11 mm²; window Aw=%12 mm²\\n"
                        "Ideal turns=%13; equivalent total gap=%14 mm; Aw/Ae=%15\\n"
                        "Assumptions: Bmax=%4 T; J=%5 A/mm²; Ku=%6%; Ks=%7%.\\n"
                        "Peak current uses the sum of harmonic magnitudes (conservative envelope). "
                        "This is NOT a selected core or winding design; core loss, gap geometry, "
                        "insulation, cooling and temperature rise are not included.")
                        .arg(value.minimumGrossAreaProductM4 * 1.0e12, 0, 'g', 6)
                        .arg(value.equivalentSquareSideM * 1.0e3, 0, 'g', 6)
                        .arg(value.storedPeakEnergyJ, 0, 'g', 6)
                        .arg(value.assumedAllowablePeakFluxDensityT, 0, 'g', 5)
                        .arg(value.assumedAllowableCurrentDensityApm2 / 1.0e6, 0, 'g', 5)
                        .arg(value.assumedWindowUtilization * 100.0, 0, 'g', 5)
                        .arg(value.assumedStackingFactor * 100.0, 0, 'g', 5)
                        .arg(rmsCurrent, 0, 'g', 6)
                        .arg(peakCurrentBound, 0, 'g', 6)
                        .arg(value.estimatedGrossCoreAreaM2 * 1.0e6, 0, 'g', 6)
                        .arg(value.estimatedEffectiveCoreAreaM2 * 1.0e6, 0, 'g', 6)
                        .arg(value.estimatedWindowAreaM2 * 1.0e6, 0, 'g', 6)
                        .arg(static_cast<qulonglong>(value.idealMinimumTurns))
                        .arg(value.idealTotalAirGapM * 1.0e3, 0, 'g', 6)
                        .arg(value.assumedWindowToCoreAreaRatio, 0, 'g', 6));
                } else {
                    output->setText(tr("Ideal area-product estimate failed: %1")
                        .arg(QString::fromStdString(estimate.error().message)));
                }
            } else {
                output->setText(tr("Ideal area-product estimate unavailable: synthesized branch data is incomplete."));
            }
        } else {
            output->setText(tr("Ideal area-product estimate: NOT CALCULATED — enter all five criteria and recalculate."));
        }
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
            tr("NOT_VERIFIED — the source-limited frequency sweep is diagnostic, not the multi-harmonic operating-current result; no Isat margin is inferred from it."));
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
                [](const auto& item) { return item[3] == 0.0 || item[4] != 0.0; });
            if (!allWithinMeasuredCurve)
                appendLog(tr("Coupled three-limb frequency sweep EXTRAPOLATED_NOT_VERIFIED: μ0 asymptotic continuation was used beyond the measured B-H curve."));
        }
        if (!threeLimbModelWarning.isEmpty())
            appendLog(threeLimbModelWarning);
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
    // The legacy single-path knee current is based on phase-conductor current.
    // It is directly comparable to winding current for Wye, but not Delta.
    const std::optional<double> magneticKneePlotReference =
        result.value().network.connection == p135::filter::Connection::Wye
            ? magneticKneeCurrentA : std::nullopt;


    QString calculationSummary = tr("LC synthesis, three-phase fundamental MNA verification and ideal-source current sweep completed. Construction material candidates are selected from exact SQLite revisions; core geometry, turns, conductor sizing and thermal design remain NOT_VERIFIED until their required source data and physical sizing solver are available.");
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

        // Preserve the exact construction configuration alongside this calculation.
        // This is traceability metadata only: the current filter solver does not consume it.
        QJsonObject constructionSnapshot;
        std::vector<std::string> constructionMaterialRevisionIds;
        for (auto* widget : findChildren<QWidget*>()) {
            const QString objectName = widget->objectName();
            if (!objectName.startsWith(QStringLiteral("construction_"))) continue;
            const QString field = objectName.mid(QStringLiteral("construction_").size());
            if (const auto* combo = qobject_cast<QComboBox*>(widget)) {
                constructionSnapshot.insert(field, combo->currentData().toString());
                if ((field == QStringLiteral("coreMaterial") ||
                     field == QStringLiteral("conductorMaterial") ||
                     field == QStringLiteral("insulationMaterial")) &&
                    !combo->currentData().toString().isEmpty()) {
                    constructionMaterialRevisionIds.push_back(
                        combo->currentData().toString().toStdString());
                }
            } else if (const auto* number = qobject_cast<QDoubleSpinBox*>(widget)) {
                constructionSnapshot.insert(field, number->value());
            } else if (const auto* integer = qobject_cast<QSpinBox*>(widget)) {
                constructionSnapshot.insert(field, integer->value());
            } else if (const auto* line = qobject_cast<QLineEdit*>(widget)) {
                constructionSnapshot.insert(field, line->text());
            } else if (const auto* textEdit = qobject_cast<QPlainTextEdit*>(widget)) {
                constructionSnapshot.insert(field, textEdit->toPlainText());
            } else if (const auto* check = qobject_cast<QCheckBox*>(widget)) {
                constructionSnapshot.insert(field, check->isChecked());
            }
        }
        inputSnapshot.insert(QStringLiteral("constructionInputs"), constructionSnapshot);
        inputSnapshot.insert(QStringLiteral("constructionSizingStatus"),
            QStringLiteral("AUTO_MATERIAL_CANDIDATES_SELECTED_PHYSICAL_SIZING_NOT_VERIFIED"));

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
            const auto lines = coreBhCurve_->toPlainText().split(QRegularExpression("[\\r\n]+"),
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
            const auto maximumBranchRms = maximumFilterBranchRmsCurrent(point.result);
            if (!maximumBranchRms) {
                appendLog(tr("Frequency-sweep point is missing a filter winding-current result; plot data rejected."));
                resultsNote_->setText(tr("Frequency-sweep current mapping failed; no current curve was generated."));
                status_->setText(tr("Plot data mapping failed"));
                return;
            }
            const double branchPeakCurrent = std::sqrt(2.0) * *maximumBranchRms;
            const double frequencyHz = point.frequency.si();
            QJsonObject sample;
            sample.insert(QStringLiteral("frequency_Hz"), frequencyHz);
            sample.insert(QStringLiteral("reactorBranchPeakCurrent_A"), branchPeakCurrent);
            frequencySweepSnapshot.append(sample);
            bankCurrentCurve.points.emplace_back(frequencyHz, branchPeakCurrent);
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
                tr("Frequency (Hz, logarithmic)"),
                hasSourceImpedance
                    ? tr("Source-impedance-included diagnostic reactor branch peak current (A)")
                    : tr("Ideal-source diagnostic reactor branch peak current (A) — NOT operational stress"));
            resultsBankCurrentPlot_->setEngineeringGuides(
                resonanceBands, saturationPeakReference, magneticKneePlotReference);
        }
        if (auto* title = findChild<QLabel*>(QStringLiteral("inductorCurrentPlotTitle"))) {
            title->setText(hasSourceImpedance
                ? tr("Reactor branch peak current — source-impedance-included diagnostic sweep")
                : tr("Reactor branch peak current — ideal-source diagnostic only (NOT operational stress)"));
            title->setToolTip(hasSourceImpedance
                ? tr("The entered Thevenin R/L is included. This remains a single-frequency diagnostic, not a multi-harmonic operating waveform.")
                : tr("Thevenin source R/L is zero or unset. The curve is an ideal-voltage-source response and must not be interpreted as a network-limited or operating current."));
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
        resultSnapshot.insert(QStringLiteral("plotDataVersion"), 2);
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
            for (const auto& revisionId : constructionMaterialRevisionIds) {
                materialReferences.push_back({
                    QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString(),
                    revisionId
                });
            }
            const auto saved = materialStore_->recordCalculation(snapshot, materialReferences);
            if (saved) {
                latestFilterSnapshotId_ = calculationId;
                latestFilterBranchId_ = 1;
                calculationSummary += tr(" Immutable calculation snapshot saved: %1.")
                    .arg(calculationId);
                appendLog(tr("Calculation snapshot %1 saved with %2 exact material revision links.")
                    .arg(calculationId).arg(materialReferences.size()));

                const auto designInputs = collectReactorDesignInputs();
                const auto inputStatus = p135::application::validate(designInputs);
                if (inputStatus) {
                    constructionInputsDirty_ = false;
                    if (auto* state = findChild<QLabel*>(QStringLiteral("constructionInputState"))) {
                        state->setText(tr("INPUTS CAPTURED — filter snapshot %1 / branch %2. Physical sizing: NOT_CONNECTED_TO_SOLVER.").arg(latestFilterSnapshotId_).arg(latestFilterBranchId_));
                        state->setStyleSheet("color: palette(text); font-weight: 600;");
                    }
                    appendLog(tr("Construction inputs captured against filter snapshot %1 / branch %2. "
                                 "Physical sizing remains NOT_CONNECTED_TO_SOLVER.")
                        .arg(latestFilterSnapshotId_).arg(latestFilterBranchId_));
                } else {
                    constructionInputsDirty_ = true;
                    if (auto* state = findChild<QLabel*>(QStringLiteral("constructionInputState")))
                        state->setText(tr("INPUTS NOT CAPTURED — %1").arg(QString::fromStdString(inputStatus.error().message)));
                    appendLog(tr("Construction inputs are incomplete or invalid: %1")
                        .arg(QString::fromStdString(inputStatus.error().message)));
                }
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
        const auto maximumBranchRms = maximumFilterBranchRmsCurrent(point.result);
        if (!maximumBranchRms) {
            appendLog(tr("Live frequency-sweep point is missing a filter winding-current result; plot data rejected."));
            resultsNote_->setText(tr("Frequency-sweep current mapping failed; no current curve was generated."));
            status_->setText(tr("Plot data mapping failed"));
            return;
        }
        liveCurrentCurve.points.emplace_back(point.frequency.si(),
            std::sqrt(2.0) * *maximumBranchRms);
    }
    if (resultsBankCurrentPlot_) {
        resultsBankCurrentPlot_->setCurves({liveCurrentCurve},
            tr("Frequency (Hz, logarithmic)"), tr("Reactor branch peak current (A)"));
        resultsBankCurrentPlot_->setEngineeringGuides(
            resonanceBands, saturationPeakReference, magneticKneePlotReference);
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
    if (hasSourceImpedance) {
        visibleWarnings.push_back(tr("The frequency sweep includes the entered per-phase Thevenin R/L, but is still a single-frequency diagnostic response. Do not treat its peak as multi-harmonic operating stress."));
    } else {
        visibleWarnings.push_back(tr("Source impedance R/L is zero or unset. The plotted current is an ideal-voltage-source diagnostic only; it is NOT source-limited and NOT operational stress. Resonance, bus-impedance scan and source-limited current response are NOT_VERIFIED."));
    }
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
    if (resultsTable_) resultsTable_->setProperty("calculationRunAvailable", true);
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
#if defined(_DEBUG) || !defined(NDEBUG)
    if (resultsBankCurrentPlot_)
        resultsBankCurrentPlot_->setDebugImagePath(
            projectArtifactPath(QStringLiteral("Debug/Plots"), projectId_, QStringLiteral("_reactor_current.png")));
    if (resultsBusImpedancePlot_)
        resultsBusImpedancePlot_->setDebugImagePath(
            projectArtifactPath(QStringLiteral("Debug/Plots"), projectId_, QStringLiteral("_bus_impedance.png")));
    if (resultsCoreFluxPlot_)
        resultsCoreFluxPlot_->setDebugImagePath(
            projectArtifactPath(QStringLiteral("Debug/Plots"), projectId_, QStringLiteral("_core_flux.png")));
    if (historyComparisonPlot_)
        historyComparisonPlot_->setDebugImagePath(
            projectArtifactPath(QStringLiteral("Debug/Plots"), projectId_, QStringLiteral("_history_comparison.png")));
#endif
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
