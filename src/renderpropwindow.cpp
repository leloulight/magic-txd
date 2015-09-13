#include "mainwindow.h"
#include "renderpropwindow.h"

template <typename modeType>
struct naturalModeList : public std::list <modeType>
{
    inline naturalModeList( std::initializer_list <modeType> list ) : list( list )
    {}

    typedef decltype( modeType::mode ) mode_t;

    inline bool getNaturalFromMode( mode_t mode, QString& naturalOut ) const
    {
        const_iterator iter = std::find( this->begin(), this->end(), mode );

        if ( iter == this->end() )
            return false;

        naturalOut = iter->natural;
        return true;
    }

    inline bool getModeFromNatural( const QString& natural, mode_t& modeOut ) const
    {
        const_iterator iter = std::find( this->begin(), this->end(), natural );

        if ( iter == this->end() )
            return false;

        modeOut = iter->mode;
        return true;
    }
};

struct addrToNatural
{
    rw::eRasterStageAddressMode mode;
    QString natural;

    inline bool operator == ( const rw::eRasterStageAddressMode& right ) const
    {
        return ( right == this->mode );
    }

    inline bool operator == ( const QString& right ) const
    {
        return ( right == this->natural );
    }
};

typedef naturalModeList <addrToNatural> addrToNaturalList_t;

static const addrToNaturalList_t addrToNaturalList =
{
    { rw::RWTEXADDRESS_WRAP, "wrap" },
    { rw::RWTEXADDRESS_CLAMP, "clamp" },
    { rw::RWTEXADDRESS_MIRROR, "mirror" }
};

struct filterToNatural
{
    rw::eRasterStageFilterMode mode;
    QString natural;
    bool isMipmap;

    inline bool operator == ( const rw::eRasterStageFilterMode& right ) const
    {
        return ( right == this->mode );
    }

    inline bool operator == ( const QString& right ) const
    {
        return ( right == this->natural );
    }
};

typedef naturalModeList <filterToNatural> filterToNaturalList_t;

static const filterToNaturalList_t filterToNaturalList =
{
    { rw::RWFILTER_POINT, "point", false },
    { rw::RWFILTER_LINEAR, "linear", false },
    { rw::RWFILTER_POINT_POINT, "point_mip_point", true },
    { rw::RWFILTER_POINT_LINEAR, "point_mip_linear", true },
    { rw::RWFILTER_LINEAR_POINT, "linear_mip_point", true },
    { rw::RWFILTER_LINEAR_LINEAR, "linear_mip_linear", true }
};

inline QComboBox* createAddressingBox( void )
{
    QComboBox *addrSelect = new QComboBox();

    for ( const addrToNatural& item : addrToNaturalList )
    {
        addrSelect->addItem( item.natural );
    }

    addrSelect->setMinimumWidth( 200 );

    return addrSelect;
}

QComboBox* RenderPropWindow::createFilterBox( void ) const
{
    // We assume that the texture we are editing does not get magically modified while
    // this dialog is open.

    bool hasMipmaps = false;
    {
        if ( TexInfoWidget *texInfo = this->texInfo )
        {
            if ( rw::TextureBase *texHandle = texInfo->GetTextureHandle() )
            {
                if ( rw::Raster *texRaster = texHandle->GetRaster() )
                {
                    hasMipmaps = ( texRaster->getMipmapCount() > 1 );
                }
            }
        }
    }

    QComboBox *filterSelect = new QComboBox();

    for ( const filterToNatural& item : filterToNaturalList )
    {
        bool isMipmapProp = item.isMipmap;

        if ( isMipmapProp == hasMipmaps )
        {
            filterSelect->addItem( item.natural );
        }
    }

    return filterSelect;
}

RenderPropWindow::RenderPropWindow( MainWindow *mainWnd, TexInfoWidget *texInfo ) : QDialog( mainWnd )
{
    this->setWindowTitle( "Rendering Props" );
    this->setWindowFlags( this->windowFlags() & ~Qt::WindowContextHelpButtonHint );

    this->mainWnd = mainWnd;
    this->texInfo = texInfo;

    this->setAttribute( Qt::WA_DeleteOnClose );
    this->setWindowModality( Qt::WindowModal );

    // Decide what status to give the dialog first.
    rw::eRasterStageFilterMode begFilterMode = rw::RWFILTER_POINT;
    rw::eRasterStageAddressMode begUAddrMode = rw::RWTEXADDRESS_WRAP;
    rw::eRasterStageAddressMode begVAddrMode = rw::RWTEXADDRESS_WRAP;

    if ( texInfo )
    {
        if ( rw::TextureBase *texHandle = texInfo->GetTextureHandle() )
        {
            begFilterMode = texHandle->GetFilterMode();
            begUAddrMode = texHandle->GetUAddressing();
            begVAddrMode = texHandle->GetVAddressing();
        }
    }

    QVBoxLayout *rootLayout = new QVBoxLayout( this );

    rootLayout->setSizeConstraint( QLayout::SetFixedSize );

    // We want to put rows of combo boxes.
    // Best put them into a form layout.
    QFormLayout *propForm = new QFormLayout();

    QComboBox *filteringSelectBox = createFilterBox();
    {
        QString natural;

        bool gotNatural = filterToNaturalList.getNaturalFromMode( begFilterMode, natural );

        if ( gotNatural )
        {
            filteringSelectBox->setCurrentText( natural );
        }
    }

    this->filterComboBox = filteringSelectBox;

    connect( filteringSelectBox, (void (QComboBox::*)( const QString& ))&QComboBox::activated, this, &RenderPropWindow::OnAnyPropertyChange );

    propForm->addRow( new QLabel( "Filtering:" ), filteringSelectBox );

    QComboBox *uaddrSelectBox = createAddressingBox();
    {
        QString natural;

        bool gotNatural = addrToNaturalList.getNaturalFromMode( begUAddrMode, natural );

        if ( gotNatural )
        {
            uaddrSelectBox->setCurrentText( natural );
        }
    }

    this->uaddrComboBox = uaddrSelectBox;

    connect( uaddrSelectBox, (void (QComboBox::*)( const QString& ))&QComboBox::activated, this, &RenderPropWindow::OnAnyPropertyChange );

    propForm->addRow( new QLabel( "U Addressing:" ), uaddrSelectBox );

    QComboBox *vaddrSelectBox = createAddressingBox();
    {
        QString natural;

        bool gotNatural = addrToNaturalList.getNaturalFromMode( begVAddrMode, natural );

        if ( gotNatural )
        {
            vaddrSelectBox->setCurrentText( natural );
        }
    }

    this->vaddrComboBox = vaddrSelectBox;

    connect( vaddrSelectBox, (void (QComboBox::*)( const QString& ))&QComboBox::activated, this, &RenderPropWindow::OnAnyPropertyChange );

    propForm->addRow( new QLabel( "V Addressing:" ), vaddrSelectBox );

    rootLayout->addLayout( propForm );

    // Give some sort of padding between buttons and form.
    propForm->setContentsMargins( QMargins( 0, 0, 0, 10 ) );

    // And now add the usual buttons.
    QHBoxLayout *buttonRow = new QHBoxLayout();

    QPushButton *buttonSet = new QPushButton( "Set" );
    buttonRow->addWidget( buttonSet );

    this->buttonSet = buttonSet;

    connect( buttonSet, &QPushButton::clicked, this, &RenderPropWindow::OnRequestSet );

    QPushButton *buttonCancel = new QPushButton( "Cancel" );
    buttonRow->addWidget( buttonCancel );

    connect( buttonCancel, &QPushButton::clicked, this, &RenderPropWindow::OnRequestCancel );

    rootLayout->addLayout( buttonRow );

    mainWnd->renderPropDlg = this;

    // Initialize the dialog.
    this->UpdateAccessibility();
}

RenderPropWindow::~RenderPropWindow( void )
{
    this->mainWnd->renderPropDlg = NULL;
}

void RenderPropWindow::OnRequestSet( bool checked )
{
    // Update the texture.
    if ( TexInfoWidget *texInfo = this->texInfo )
    {
        if ( rw::TextureBase *texHandle = texInfo->GetTextureHandle() )
        {
            // Just set the properties that we recognize.
            {
                rw::eRasterStageFilterMode filterMode;

                bool hasProp = filterToNaturalList.getModeFromNatural( this->filterComboBox->currentText(), filterMode );

                if ( hasProp )
                {
                    texHandle->SetFilterMode( filterMode );
                }
            }

            {
                rw::eRasterStageAddressMode uaddrMode;

                bool hasProp = addrToNaturalList.getModeFromNatural( this->uaddrComboBox->currentText(), uaddrMode );
                    
                if ( hasProp )
                {
                    texHandle->SetUAddressing( uaddrMode );
                }
            }

            {
                rw::eRasterStageAddressMode vaddrMode;

                bool hasProp = addrToNaturalList.getModeFromNatural( this->vaddrComboBox->currentText(), vaddrMode );

                if ( hasProp )
                {
                    texHandle->SetVAddressing( vaddrMode );
                }
            }
        }
    }

    this->close();
}

void RenderPropWindow::OnRequestCancel( bool checked )
{
    this->close();
}

void RenderPropWindow::UpdateAccessibility( void )
{
    // Only allow setting if we actually change from the original values.
    bool allowSet = true;

    if ( TexInfoWidget *texInfo = this->texInfo )
    {
        if ( rw::TextureBase *texHandle = texInfo->GetTextureHandle() )
        {
            rw::eRasterStageFilterMode curFilterMode = texHandle->GetFilterMode();
            rw::eRasterStageAddressMode curUAddrMode = texHandle->GetUAddressing();
            rw::eRasterStageAddressMode curVAddrMode = texHandle->GetVAddressing();

            bool havePropsChanged = false;

            if ( !havePropsChanged )
            {
                rw::eRasterStageFilterMode selFilterMode;
                    
                bool hasProp = filterToNaturalList.getModeFromNatural( this->filterComboBox->currentText(), selFilterMode );

                if ( hasProp && curFilterMode != selFilterMode )
                {
                    havePropsChanged = true;
                }
            }

            if ( !havePropsChanged )
            {
                rw::eRasterStageAddressMode selUAddrMode;

                bool hasProp = addrToNaturalList.getModeFromNatural( this->uaddrComboBox->currentText(), selUAddrMode );

                if ( hasProp && curUAddrMode != selUAddrMode )
                {
                    havePropsChanged = true;
                }
            }

            if ( !havePropsChanged )
            {
                rw::eRasterStageAddressMode selVAddrMode;

                bool hasProp = addrToNaturalList.getModeFromNatural( this->vaddrComboBox->currentText(), selVAddrMode );

                if ( hasProp && curVAddrMode != selVAddrMode )
                {
                    havePropsChanged = true;
                }
            }

            if ( !havePropsChanged )
            {
                allowSet = false;
            }
        }
    }

    this->buttonSet->setDisabled( !allowSet );
}