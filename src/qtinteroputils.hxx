#ifndef _QT_INTEROP_UTILS_INTERNAL_
#define _QT_INTEROP_UTILS_INTERNAL_

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

    inline void selectCurrent( QComboBox *box, mode_t curMode ) const
    {
        QString nat;

        bool gotNatural = getNaturalFromMode( curMode, nat );

        if ( gotNatural )
        {
            box->setCurrentText( nat );
        }
    }

    inline void getCurrent( QComboBox *box, mode_t& modeOut ) const
    {
        QString currentText = box->currentText();

        getModeFromNatural( currentText, modeOut );
    }

    inline void putDown( QComboBox *box ) const
    {
        for ( const modeType& item : *this )
        {
            box->addItem( item.natural );
        }
    }
};

#endif //_QT_INTEROP_UTILS_INTERNAL_