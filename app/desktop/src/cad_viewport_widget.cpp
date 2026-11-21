#include "cad_viewport_widget.hpp"

#include <QHBoxLayout>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QLabel>
#include <QPushButton>
#include <QPaintEngine>
#include <QPainter>
#include <QPoint>
#include <QResizeEvent>
#include <QShowEvent>
#include <QVBoxLayout>

#include <AIS_InteractiveContext.hxx>
#include <AIS_Shape.hxx>
#include <AIS_SelectionScheme.hxx>
#include <Aspect_DisplayConnection.hxx>
#include <Aspect_Handle.hxx>
#include <Graphic3d_GraphicDriver.hxx>
#include <OpenGl_GraphicDriver.hxx>
#include <Standard_Failure.hxx>
#include <V3d_View.hxx>
#include <V3d_TypeOfOrientation.hxx>
#include <V3d_Viewer.hxx>
#include <Quantity_Color.hxx>

#ifdef _WIN32
#include <WNT_Window.hxx>
#endif

#include <utility>
#include <cmath>
#include <unordered_map>

class CadViewportWidget::NativeView final : public QWidget
{
public:
    explicit NativeView(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setMouseTracking(true);
        setFocusPolicy(Qt::StrongFocus);
        setMinimumSize(480, 320);
    }

    ~NativeView() override
    {
        if (!view_.IsNull())
            view_->Remove();
    }

    QPaintEngine* paintEngine() const override
    {
        // A native paint surface is required only after OCCT has been initialized.
        // Keep the empty CAD page on Qt's normal paint engine so it cannot expose
        // pixels belonging to another QTabWidget page.
        return view_.IsNull() ? QWidget::paintEngine() : nullptr;
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        if (!view_.IsNull())
            return;
        QPainter painter(this);
        painter.fillRect(rect(), palette().brush(QPalette::Base));
        QWidget::paintEvent(event);
    }

public:
    void setItems(const std::vector<p135::cad::AssemblyPresentationDisplayItem>& items)
    {
        items_ = items;
        if (items_.empty())
        {
            if (!context_.IsNull())
            {
                context_->EraseAll(false);
                context_->UpdateCurrentViewer();
            }
            update();
            return;
        }
        if (initialize())
            displayItems();
    }

    bool setComponentVisibility(const std::string& id, bool visible)
    {
        if (context_.IsNull()) return false;
        const auto found = presentations_.find(id);
        if (found == presentations_.end()) return false;
        if (visible)
            context_->Display(found->second, false);
        else
            context_->Erase(found->second, false);
        context_->UpdateCurrentViewer();
        return true;
    }

    bool setComponentTransparency(const std::string& id, double transparency)
    {
        if (context_.IsNull() || !std::isfinite(transparency) ||
            transparency < 0.0 || transparency > 1.0) return false;
        const auto found = presentations_.find(id);
        if (found == presentations_.end()) return false;
        found->second->SetTransparency(transparency);
        context_->Redisplay(found->second, false);
        context_->UpdateCurrentViewer();
        return true;
    }

    void fitAll()
    {
        if (view_.IsNull() || items_.empty()) return;
        view_->FitAll(0.01, true);
        view_->ZFitAll();
        view_->Redraw();
    }

    void setProjection(const V3d_TypeOfOrientation projection)
    {
        if (view_.IsNull() || items_.empty()) return;
        view_->SetProj(projection);
        fitAll();
    }

protected:
    void mousePressEvent(QMouseEvent* event) override
    {
        if (view_.IsNull()) return;
        lastMousePosition_ = event->position().toPoint();
        if (event->button() == Qt::LeftButton && !items_.empty())
        {
            view_->StartRotation(lastMousePosition_.x(), height() - lastMousePosition_.y());
            rotating_ = true;
            moved_ = false;
            event->accept();
            return;
        }
        if (event->button() == Qt::MiddleButton && !items_.empty())
        {
            panStartPosition_ = lastMousePosition_;
            view_->Pan(0, 0, 1.0, true);
            panning_ = true;
            event->accept();
            return;
        }
        QWidget::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        if (view_.IsNull()) return;
        const QPoint position = event->position().toPoint();
        if (rotating_ && (event->buttons() & Qt::LeftButton))
        {
            if (position != lastMousePosition_)
            {
                view_->Rotation(position.x(), height() - position.y());
                moved_ = true;
                lastMousePosition_ = position;
            }
            event->accept();
            return;
        }
        if (panning_ && (event->buttons() & Qt::MiddleButton))
        {
            view_->Pan(position.x() - panStartPosition_.x(),
                       panStartPosition_.y() - position.y(), 1.0, false);
            event->accept();
            return;
        }
        if (!context_.IsNull())
        {
            context_->MoveTo(position.x(), height() - position.y(), view_, true);
            event->accept();
            return;
        }
        QWidget::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton && rotating_)
        {
            rotating_ = false;
            if (!moved_ && !context_.IsNull())
            {
                const QPoint position = event->position().toPoint();
                context_->MoveTo(position.x(), height() - position.y(), view_, true);
                context_->SelectDetected(AIS_SelectionScheme_Replace);
            }
            event->accept();
            return;
        }
        if (event->button() == Qt::MiddleButton && panning_)
        {
            panning_ = false;
            event->accept();
            return;
        }
        QWidget::mouseReleaseEvent(event);
    }

    void wheelEvent(QWheelEvent* event) override
    {
        if (view_.IsNull() || items_.empty())
        {
            QWidget::wheelEvent(event);
            return;
        }
        const double factor = event->angleDelta().y() >= 0 ? 1.12 : 1.0 / 1.12;
        view_->SetZoom(factor, true);
        view_->Redraw();
        event->accept();
    }

    void showEvent(QShowEvent* event) override
    {
        QWidget::showEvent(event);
        // Do not create a native HWND/OpenGL surface for the empty CAD state.
        // The native child window can otherwise reveal stale pixels from the
        // previously visible QTabWidget page.
        if (!items_.empty() && initialize())
            displayItems();
    }

    void resizeEvent(QResizeEvent* event) override
    {
        QWidget::resizeEvent(event);
        if (!view_.IsNull())
        {
            view_->MustBeResized();
            view_->Redraw();
        }
    }

private:
    bool initialize()
    {
#ifdef _WIN32
        if (!view_.IsNull()) return true;
        if (!isVisible() || width() < 2 || height() < 2) return false;

        try
        {
            // Promote this widget to a native child only when actual geometry
            // exists. An empty CAD workspace must remain an ordinary Qt widget.
            setAttribute(Qt::WA_NativeWindow, true);
            setAttribute(Qt::WA_PaintOnScreen, true);
            setAttribute(Qt::WA_NoSystemBackground, true);
            setAttribute(Qt::WA_OpaquePaintEvent, true);
            const WId nativeHandle = winId();

            displayConnection_ = new Aspect_DisplayConnection();
            graphicDriver_ = new OpenGl_GraphicDriver(displayConnection_);
            viewer_ = new V3d_Viewer(graphicDriver_);
            viewer_->SetDefaultLights();
            viewer_->SetLightOn();

            context_ = new AIS_InteractiveContext(viewer_);
            view_ = viewer_->CreateView();
            window_ = new WNT_Window(reinterpret_cast<Aspect_Handle>(nativeHandle));
            view_->SetWindow(window_);
            if (!window_->IsMapped())
                window_->Map();
            view_->SetProj(V3d_XposYnegZpos);
            view_->Redraw();
            return true;
        }
        catch (const Standard_Failure&)
        {
            view_.Nullify();
            context_.Nullify();
            viewer_.Nullify();
            graphicDriver_.Nullify();
            displayConnection_.Nullify();
            window_.Nullify();
            return false;
        }
#else
        return false;
#endif
    }

    void displayItems()
    {
        if (context_.IsNull() || view_.IsNull()) return;

        context_->EraseAll(false);
        presentations_.clear();
        for (const auto& item : items_)
        {
            if (item.shape.IsNull()) continue;
            Handle(AIS_Shape) presentation = new AIS_Shape(item.shape);
            presentation->SetColor(Quantity_Color(
                item.baseColor[0], item.baseColor[1], item.baseColor[2], Quantity_TOC_RGB));
            presentation->SetDisplayMode(AIS_Shaded);
            if (item.transparency > 0.0)
                presentation->SetTransparency(item.transparency);
            context_->Display(presentation, false);
            presentations_.emplace(item.id, presentation);
        }
        context_->UpdateCurrentViewer();
        fitAll();
    }

    std::vector<p135::cad::AssemblyPresentationDisplayItem> items_;
    std::unordered_map<std::string, Handle(AIS_Shape)> presentations_;
    QPoint lastMousePosition_;
    QPoint panStartPosition_;
    bool rotating_{false};
    bool panning_{false};
    bool moved_{false};
    Handle(Aspect_DisplayConnection) displayConnection_;
    Handle(Graphic3d_GraphicDriver) graphicDriver_;
    Handle(V3d_Viewer) viewer_;
    Handle(AIS_InteractiveContext) context_;
    Handle(V3d_View) view_;
#ifdef _WIN32
    Handle(WNT_Window) window_;
#endif
};

CadViewportWidget::CadViewportWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);

    auto* toolbar = new QHBoxLayout;
    const auto addCameraButton = [this, toolbar](const QString& label,
                                                  const V3d_TypeOfOrientation projection) {
        auto* button = new QPushButton(label, this);
        button->setEnabled(false);
        toolbar->addWidget(button);
        connect(button, &QPushButton::clicked, this, [this, projection] {
            view_->setProjection(projection);
        });
        cameraButtons_.push_back(button);
    };

    addCameraButton(tr("Iso"), V3d_XposYnegZpos);
    addCameraButton(tr("Front"), V3d_Yneg);
    addCameraButton(tr("Top"), V3d_Zpos);
    addCameraButton(tr("Side"), V3d_Xpos);

    auto* fitButton = new QPushButton(tr("Fit all"), this);
    fitButton->setEnabled(false);
    toolbar->addWidget(fitButton);
    connect(fitButton, &QPushButton::clicked, this, [this] { view_->fitAll(); });
    fitButton_ = fitButton;
    toolbar->addStretch(1);
    root->addLayout(toolbar);

    view_ = std::make_unique<NativeView>(this);
    root->addWidget(view_.get(), 1);

    emptyState_ = new QLabel(
        tr("3D model is not available. Enter verified physical core, bobbin and winding dimensions before generating CAD geometry."),
        this);
    emptyState_->setWordWrap(true);
    emptyState_->setAlignment(Qt::AlignCenter);
    emptyState_->setStyleSheet("color: palette(mid); padding: 8px;");
    root->addWidget(emptyState_);
}

CadViewportWidget::~CadViewportWidget() = default;

bool CadViewportWidget::setComponentVisibility(const std::string& componentId, bool visible)
{
    return view_->setComponentVisibility(componentId, visible);
}

bool CadViewportWidget::setComponentTransparency(
    const std::string& componentId, double transparency)
{
    return view_->setComponentTransparency(componentId, transparency);
}

void CadViewportWidget::setDisplayItems(
    const std::vector<p135::cad::AssemblyPresentationDisplayItem>& items)
{
    view_->setItems(items);
    const bool hasGeometry = !items.empty();
    emptyState_->setVisible(!hasGeometry);
    for (auto* button : cameraButtons_)
        button->setEnabled(hasGeometry);
    fitButton_->setEnabled(hasGeometry);
}
