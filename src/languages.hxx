#pragma once

#include "guiserialization.hxx"

class MagicLanguage
{
public:
    QString languageFilePath;
    QString languageFileName;

    struct LanguageInfo {
        // basic parameters
        unsigned int version;
        QString name;
        QString nameInOriginal;
        QString abbr;
        QString authors;
    } info;

    QHash<QString, QString> strings;

    bool lastSearchSuccesfull;

    static QString getStringFormattedWithVars(QString &string);

    static QString keyNotDefined(QString key);

    static bool getLanguageInfo(QString filepath, LanguageInfo &info);

    bool loadText();

    void clearText();

    QString getText(QString key, bool *found = NULL);
};

class MagicLanguages
{
public:
    unsigned int getNumberOfLanguages();

    QString getByKey(QString key, bool *found = NULL);

    void scanForLanguages(QString languagesFolder);

    void updateLanguageContext();

    bool selectLanguageByIndex(unsigned int index);

    bool selectLanguageByLanguageName(QString langName);

    bool selectLanguageByLanguageAbbr(QString abbr);

    bool selectLanguageByFileName(QString filename);

    inline MagicLanguages( void )
    {
        this->mainWnd = NULL;

        this->currentLanguage = -1;
    }

    inline void Initialize( MainWindow *mainWnd )
    {
        this->mainWnd = mainWnd;

        // read languages
        scanForLanguages(mainWnd->makeAppPath("languages"));

        if (mainWnd->lastLanguageFileName.isEmpty() || !selectLanguageByFileName(mainWnd->lastLanguageFileName)) // try to select previously saved language
        {
            if (!selectLanguageByLanguageName(DEFAULT_LANGUAGE)) // then try to set the language to default
            {
                selectLanguageByIndex(0); // ok, enough
            }
        }
    }

    inline void Shutdown( MainWindow *mainWnd )
    {
        this->mainWnd = NULL;
    }

    void Load( MainWindow *mainWnd, rw::BlockProvider& configBlock )
    {
        std::wstring langfile_str;

        RwReadUnicodeString( configBlock, langfile_str );

        mainWnd->lastLanguageFileName = QString::fromStdWString( langfile_str );

        // Load the language.
        selectLanguageByFileName( mainWnd->lastLanguageFileName );
    }

    void Save( const MainWindow *mainWnd, rw::BlockProvider& configBlock ) const
    {
        RwWriteUnicodeString( configBlock, mainWnd->lastLanguageFileName.toStdWString() );
    }

    MainWindow *mainWnd;

    QVector<MagicLanguage> languages;

    int currentLanguage; // index of current language in languages array, -1 if not defined

    typedef std::list <magicTextLocalizationItem*> localizations_t;

    localizations_t culturalItems;
};

extern MagicLanguages ourLanguages;