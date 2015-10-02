#pragma once

struct MessageReceiver abstract
{
    virtual void OnMessage( const std::string& msg ) = 0;
    virtual void OnMessage( const std::wstring& msg ) = 0;

    virtual CFile* WrapStreamCodec( CFile *compressed ) = 0;
};