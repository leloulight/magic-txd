#pragma once

#include <QDialog>
#include <QFormLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QGroupBox>
#include <QRadioButton>
#include <QPushButton>
#include <QCheckBox>
#include <QPainter>

class MainWindow;

class TexAddDialog : public QDialog
{
    struct pixelPreviewWidget : public QWidget
    {
        inline pixelPreviewWidget( TexAddDialog *owner )
        {
            this->setContentsMargins( 0, 0, 0, 0 );

            this->owner = owner;
        }

        void paintEvent( QPaintEvent *evt ) override
        {
            QPainter thePainter( this );

            thePainter.drawPixmap( 0, 0, this->width(), this->height(), owner->pixelsToAdd );

            QWidget::paintEvent( evt );
        }

        QSize sizeHint( void ) const override
        {
            return QSize( 100, 50 );
        }

        TexAddDialog *owner;
    };

public:
    TexAddDialog( MainWindow *mainWnd, QPixmap pixels );

private:
    QPixmap pixelsToAdd;
};