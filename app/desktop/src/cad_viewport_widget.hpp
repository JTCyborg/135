#pragma once

#include <QWidget>

#include "p135/cad/assembly_presentation.hpp"

#include <memory>
#include <vector>

class QLabel;
class QPushButton;

class CadViewportWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit CadViewportWidget(QWidget* parent = nullptr);
    ~CadViewportWidget() override;

    void setDisplayItems(
        const std::vector<p135::cad::AssemblyPresentationDisplayItem>& items);

private:
    class NativeView;
    std::unique_ptr<NativeView> view_;
    QLabel* emptyState_{};
    QPushButton* fitButton_{};
    std::vector<QPushButton*> cameraButtons_;
};
