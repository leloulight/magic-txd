#pragma once

#include "tools/shared.h"

#include "qtinteroputils.hxx"

// Put more special shared things here, which are shared across Magic.TXD tools.
namespace toolshare
{

struct platformToNatural
{
    rwkind::eTargetPlatform mode;
    QString natural;

    inline bool operator == ( const decltype( mode )& right ) const
    {
        return ( right == this->mode );
    }

    inline bool operator == ( const QString& right ) const
    {
        return ( right == this->natural );
    }
};

typedef naturalModeList <platformToNatural> platformToNaturalList_t;

static platformToNaturalList_t platformToNaturalList =
{
    { rwkind::PLATFORM_PC, "PC" },
    { rwkind::PLATFORM_PS2, "PS2" },
    { rwkind::PLATFORM_XBOX, "XBOX" },
    { rwkind::PLATFORM_DXT_MOBILE, "S3TC mobile" },
    { rwkind::PLATFORM_PVR, "PowerVR" },
    { rwkind::PLATFORM_ATC, "AMD TC" },
    { rwkind::PLATFORM_UNC_MOBILE, "uncomp. mobile" }
};

struct gameToNatural
{
    rwkind::eTargetGame mode;
    QString natural;

    inline bool operator == ( const decltype( mode )& right ) const
    {
        return ( right == this->mode );
    }

    inline bool operator == ( const QString& right ) const
    {
        return ( right == this->natural );
    }
};

typedef naturalModeList <gameToNatural> gameToNaturalList_t;

static gameToNaturalList_t gameToNaturalList =
{
    { rwkind::GAME_GTA3, "GTA III" },
    { rwkind::GAME_GTAVC, "GTA VC" },
    { rwkind::GAME_GTASA, "GTA SA" },
    { rwkind::GAME_MANHUNT, "Manhunt" },
    { rwkind::GAME_BULLY, "Bully" }
};

inline void createTargetConfigurationComponents(
    QLayout *rootLayout,
    rwkind::eTargetPlatform curPlatform, rwkind::eTargetGame curGame,
    QComboBox*& gameSelBoxOut,
    QComboBox*& platSelBoxOut
)
{
    // Now a target format selection group.
    QHBoxLayout *platformGroup = new QHBoxLayout();

    platformGroup->addWidget( new QLabel( "Platform:" ) );

    QComboBox *platformSelBox = new QComboBox();

    // We have a fixed list of platforms here.
    platformToNaturalList.putDown( platformSelBox );

    platSelBoxOut = platformSelBox;

    // Select current.
    platformToNaturalList.selectCurrent( platformSelBox, curPlatform );

    platformGroup->addWidget( platformSelBox );

    rootLayout->addItem( platformGroup );

    QHBoxLayout *gameGroup = new QHBoxLayout();

    gameGroup->addWidget( new QLabel( "Game:" ) );

    QComboBox *gameSelBox = new QComboBox();

    // Add a fixed list of known games.
    gameToNaturalList.putDown( gameSelBox );

    gameSelBoxOut = gameSelBox;

    gameToNaturalList.selectCurrent( gameSelBox, curGame );

    gameGroup->addWidget( gameSelBox );

    rootLayout->addItem( gameGroup );
}

};