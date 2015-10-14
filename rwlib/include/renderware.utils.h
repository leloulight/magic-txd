// Here you find useful utilities that you do not want to redo (but you can) if you are working with rwlib.

namespace utils
{

struct bufferedWarningManager : public WarningManagerInterface
{
    void OnWarning( std::string&& message ) override;

    void forward( Interface *engineInterface );

private:
    std::list <std::string> messages;
};

struct stacked_warnman_scope
{
    inline stacked_warnman_scope( Interface *engineInterface, WarningManagerInterface *newMan )
    {
        this->prevWarnMan = engineInterface->GetWarningManager();
        
        engineInterface->SetWarningManager( newMan );

        this->rwEngine = engineInterface;
    }

    inline ~stacked_warnman_scope( void )
    {
        this->rwEngine->SetWarningManager( this->prevWarnMan );
    }

private:
    Interface *rwEngine;

    WarningManagerInterface *prevWarnMan;
};

struct stacked_warnlevel_scope
{
    inline stacked_warnlevel_scope( Interface *engineInterface, int level )
    {
        this->prevLevel = engineInterface->GetWarningLevel();

        engineInterface->SetWarningLevel( level );

        this->engineInterface = engineInterface;
    }

    inline ~stacked_warnlevel_scope( void )
    {
        this->engineInterface->SetWarningLevel( this->prevLevel );
    }

private:
    Interface *engineInterface;

    int prevLevel;
};

}