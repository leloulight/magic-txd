/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.hazards.cpp
*  PURPOSE:     Thread hazard management internals, to prevent deadlocks
*  DEVELOPERS:  Martin Turski <quiret@gmx.de>
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#include "StdInc.h"

#include "CExecutiveManager.hazards.hxx"

BEGIN_NATIVE_EXECUTIVE

executiveHazardManagerEnvRegister_t executiveHazardManagerEnvRegister;

// Hazard API implementation.
void PushHazard( CExecutiveManager *manager, hazardPreventionInterface *intf )
{
    executiveHazardManagerEnv *hazardEnv = executiveHazardManagerEnvRegister.GetPluginStruct( (CExecutiveManagerNative*)manager );

    if ( hazardEnv )
    {
        stackObjectHazardRegistry *reg = hazardEnv->GetCurrentHazardRegistry( manager );

        if ( reg )
        {
            reg->PushHazard( intf );
        }
    }
}

void PopHazard( CExecutiveManager *manager )
{
    executiveHazardManagerEnv *hazardEnv = executiveHazardManagerEnvRegister.GetPluginStruct( (CExecutiveManagerNative*)manager );

    if ( hazardEnv )
    {
        stackObjectHazardRegistry *reg = hazardEnv->GetCurrentHazardRegistry( manager );

        if ( reg )
        {
            reg->PopHazard();
        }
    }
}

void registerStackHazardManagement( void )
{
    executiveHazardManagerEnvRegister.RegisterPlugin( executiveManagerFactory );
}

END_NATIVE_EXECUTIVE