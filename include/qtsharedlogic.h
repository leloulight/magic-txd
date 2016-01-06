#pragma once

#include <QPushButton>
#include <QLineEdit>
#include <QFileDialog>
#include <QHBoxLayout>

#include "languages.h"

// Put very generic shared things in this file.

namespace qtshared
{
    struct PathBrowseButton : public QPushButton, public simpleLocalizationItem
    {
        inline PathBrowseButton( MainWindow *mainWnd, QLineEdit *lineEdit ) : QPushButton(), simpleLocalizationItem( mainWnd, "Tools.Browse" )
        {
            this->theEdit = lineEdit;

            connect( this, &QPushButton::clicked, this, &PathBrowseButton::OnBrowsePath );

            Init();
        }

        inline ~PathBrowseButton( void )
        {
            Shutdown();
        }

        void doText( QString text ) override
        {
            this->setText( text );
        }

    public slots:
        void OnBrowsePath( bool checked )
        {
            QString selPath =
                QFileDialog::getExistingDirectory(
                    this, getLanguageItemByKey( mainWnd, "Tools.BrwDesc" ),
                    theEdit->text()
                );

            if ( selPath.isEmpty() == false )
            {
                this->theEdit->setText( selPath );
            }
        }

    private:
        QLineEdit *theEdit;
    };

    inline QLayout* createPathSelectGroup( MainWindow *mainWnd, QString begStr, QLineEdit*& editOut )
    {
        QHBoxLayout *pathRow = new QHBoxLayout();

        QLineEdit *pathEdit = new QLineEdit( begStr );

        pathRow->addWidget( pathEdit );

        QPushButton *pathBrowseButton = new PathBrowseButton( mainWnd, pathEdit );

        pathRow->addWidget( pathBrowseButton );

        editOut = pathEdit;

        return pathRow;
    }
    
    inline QLayout* createMipmapGenerationGroup( MainWindow *mainWnd, QObject *parent, bool isEnabled, int curMipMax, QCheckBox*& propGenMipmapsOut, QLineEdit*& editMaxMipLevelOut )
    {
        QHBoxLayout *genMipGroup = new QHBoxLayout();

        QCheckBox *propGenMipmaps = CreateCheckBoxL( mainWnd, "Tools.GenMips" );

        propGenMipmaps->setChecked( isEnabled );

        propGenMipmapsOut = propGenMipmaps;

        genMipGroup->addWidget( propGenMipmaps );

        QHBoxLayout *mipMaxLevelGroup = new QHBoxLayout();

        mipMaxLevelGroup->setAlignment( Qt::AlignRight );

        mipMaxLevelGroup->addWidget( CreateLabelL( mainWnd, "Tools.MaxMips" ), 0, Qt::AlignRight );

        QLineEdit *maxMipLevelEdit = new QLineEdit( QString( "%1" ).arg( curMipMax ) );

        QIntValidator *maxMipLevelVal = new QIntValidator( 0, 32, parent );

        maxMipLevelEdit->setValidator( maxMipLevelVal );

        editMaxMipLevelOut = maxMipLevelEdit;

        maxMipLevelEdit->setMaximumWidth( 40 );

        mipMaxLevelGroup->addWidget( maxMipLevelEdit );

        genMipGroup->addLayout( mipMaxLevelGroup );

        return genMipGroup;
    }

    inline QLayout* createGameRootInputOutputForm( MainWindow *mainWnd, const std::wstring& curGameRoot, const std::wstring& curOutputRoot, QLineEdit*& editGameRootOut, QLineEdit*& editOutputRootOut )
    {
        QFormLayout *basicPathForm = new QFormLayout();

        QLayout *gameRootLayout = qtshared::createPathSelectGroup( mainWnd, QString::fromStdWString( curGameRoot ), editGameRootOut );
        QLayout *outputRootLayout = qtshared::createPathSelectGroup( mainWnd, QString::fromStdWString( curOutputRoot ), editOutputRootOut );

        basicPathForm->addRow( CreateLabelL(mainWnd, "Tools.GameRt"), gameRootLayout );
        basicPathForm->addRow( CreateLabelL(mainWnd, "Tools.Output"), outputRootLayout );

        return basicPathForm;
    }
};