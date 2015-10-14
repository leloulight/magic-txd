#include "StdInc.h"

// This file contains utilities that do not depend on the internal state of the RenderWare framework.

namespace rw
{

namespace utils
{

void bufferedWarningManager::OnWarning( std::string&& msg )
{
    // We can take the string directly.
    this->messages.push_back( std::move( msg ) );
}

void bufferedWarningManager::forward( Interface *engineInterface )
{
    // Move away the warnings from us.
    std::list <std::string> warnings_to_push = std::move( this->messages );

    // We give all warnings to the engine.
    for ( std::string& msg : warnings_to_push )
    {
        engineInterface->PushWarning( std::move( msg ) );
    }
}

};

};