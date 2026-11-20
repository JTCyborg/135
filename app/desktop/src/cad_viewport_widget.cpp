#include "cad_viewport_widget.hpp"

#include <QHBoxLayout>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QLabel>
#include <QPushButton>
#include <QPaintEngine>
#include <QResizeEvent>
#include <QShowEvent>
#include <QVBoxLayout>

#include <AIS_InteractiveContext.hxx>
#include <AIS_Shape.hxx>
#include <Aspect_DisplayConnection.hxx>
#include <Aspect_Handle.hxx>
#include <Graphic3d_GraphicDriver.hxx>
#include <OpenGl_GraphicDriver.hxx>
#include <Standard_Failure.hxx>
#include <V3d_View.hxx>
#include <V3d_Viewer.hxx>

#ifdef _WIN32
#include <WNT_Window.hxx>
#endif

#include <utility>

class CadViewportWidget::NativeView final : public QWidget
{
public:
    explicit NativeView(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setAttribute(Qt::WA_NativeWindow);
        setAttribute(Qt::WA_PaintOnScreen);
        setAttribute(Qt::WA_NoSystemBackground);
        setAttribute(Qt::WA_OpaquePaintEvent);
        setMinimumSize(480, 320);
    }

    ~NativeView() override
    {
        if (!view_.IsNull())
            view_->Remove();
    }

    QPaintEngine* paintEngine() const override { return nullptr; }

    void setItems(const std::vector<p135::cad::AssemblyPresentationDisplayItem>& items)
    {
        items_ = items;
        if (initialize())
            displayItems();
    }

    void fitAll()
    {
        if (view_.IsNull() || items_.empty()) return;
        view_->FitAll(0.01, Standard_True);
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
            view_->Pan(0, 0, 1.0, Standard_True);
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
            view_->Pan(position.x() - lastMousePosition_.x(),
                       lastMousePosition_.y() - position.y(), 1.0, Standard_False);
            lastMousePosition_ = position;
            event->accept();
            return;
        }
        if (!context_.IsNull())
        {
            context_->MoveTo(position.x(), height() - position.y(), view_, Standard_True);
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
                context_->MoveTo(position.x(), height() - position.y(), view_, Standard_True);
                context_->Select(Standard_True);
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
        view_->SetZoom(factor, Standard_True);
        view_->Redraw();
        event->accept();
    }

    void showEvent(QShowEvent* event) override
    {
        QWidget::showEvent(event);
        if (initialize() && !items_.empty())
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
            displayConnection_ = new Aspect_DisplayConnection();
            graphicDriver_ = new OpenGl_GraphicDriver(displayConnection_);
            viewer_ = new V3d_Viewer(graphicDriver_);
            viewer_->SetDefaultLights();
            viewer_->SetLightOn();

            context_ = new AIS_InteractiveContext(viewer_);
            view_ = viewer_->CreateView();
            window_ = new WNT_Window(static_cast<Aspect_Handle>(winId()));
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

        context_->EraseAll(Standard_False);
        for (const auto& item : items_)
        {
            if (item.shape.IsNull()) continue;
            Handle(AIS_Shape) presentation = new AIS_Shape(item.shape);
            if (item.transparency > 0.0)
                presentation->SetTransparency(item.transparency);
            context_->Display(presentation, Standard_False);
        }
        context_->UpdateCurrentViewer();
        fitAll();
    }

    std::vector<p135::cad::AssemblyPresentationDisplayItem> items_;
    QPoint lastMousePosition_;
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
