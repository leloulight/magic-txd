#include "textureviewport.h"
#include "mainwindow.h"

TexViewportWidget::TexViewportWidget(MainWindow *MainWnd)
{
    this->mainWnd = MainWnd;
}

void TexViewportWidget::resizeEvent(QResizeEvent *resEvent)
{
    QScrollArea::resizeEvent(resEvent);
    if (mainWnd)
        mainWnd->updateTextureViewport();
}