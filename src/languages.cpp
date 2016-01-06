#include "mainwindow.h"

#include "languages.h"
#include <QFile>
#include <QTextStream>
#include "testmessage.h"

#include "guiserialization.hxx"

#include "languages.hxx"

// Since there can be only one instance of Magic.TXD per application, we can use a global.
// Here for easy access :3
// RIP OOP. OK OK m8, you get your global memory.
MagicLanguages ourLanguages;

struct magicLanguagesMainWindowEnv : public magicSerializationProvider
{
    inline void Initialize( MainWindow *mainWnd )
    {
        ourLanguages.Initialize( mainWnd );

        // Register us as serialization extension.
        RegisterMainWindowSerialization( mainWnd, MAGICSERIALIZE_LANGUAGE, this );
    }

    inline void Shutdown( MainWindow *mainWnd )
    {
        // Unregister us again.
        UnregisterMainWindowSerialization( mainWnd, MAGICSERIALIZE_LANGUAGE );

        ourLanguages.Shutdown( mainWnd );
    }

    void Load( MainWindow *mainWnd, rw::BlockProvider& configBlock ) override
    {
        ourLanguages.Load( mainWnd, configBlock );
    }

    void Save( const MainWindow *mainWnd, rw::BlockProvider& configBlock ) const override
    {
        ourLanguages.Save( mainWnd, configBlock );
    }
};

static PluginDependantStructRegister <magicLanguagesMainWindowEnv, mainWindowFactory_t> magicLanguagesMainWindowRegister;

void InitializeMagicLanguages( void )
{
    magicLanguagesMainWindowRegister.RegisterPlugin( mainWindowFactory );
}

bool RegisterTextLocalizationItem( MainWindow *mainWnd, magicTextLocalizationItem *provider )
{
    bool success = false;
    {
        // Register this.
        ourLanguages.culturalItems.push_back( provider );

        // If we have a loaded language, notify the item.
        if ( ourLanguages.currentLanguage != -1 )
        {
            provider->updateContent( mainWnd );
        }

        success = true;
    }

    return success;
}

bool UnregisterTextLocalizationItem( MainWindow *mainWnd, magicTextLocalizationItem *provider )
{
    bool success = false;
    {
        // Try to find this item.
        MagicLanguages::localizations_t::const_iterator iter = std::find( ourLanguages.culturalItems.begin(), ourLanguages.culturalItems.end(), provider );

        if ( iter != ourLanguages.culturalItems.end() )
        {
            // Remove it.
            ourLanguages.culturalItems.erase( iter );

            success = true;
        }
    }

    return success;
}

QString getLanguageItemByKey( MainWindow *mainWnd, QString token, bool *found )  // RIP mainWnd param
{
    return ourLanguages.getByKey( token, found );
}

struct magic_value_item_t
{
    const char *key;
    const char *value;
};

static magic_value_item_t valueVars[] =
{
    { "_PARAM_1",        "%1" },
    { "_MAGIC_TXD_NAME", "Magic.TXD" },
    { "_AUTHOR_NAME_1",  "DK22Pac" },
    { "_AUTHOR_NAME_2",  "The_GTA" },
};

QString MagicLanguage::getStringFormattedWithVars(QString &string) {
    QString result = string;
    for (unsigned int i = 0; i < NUMELMS(valueVars); i++)
    {
        const magic_value_item_t& valuePair = valueVars[ i ];
        result.replace( valuePair.key, valuePair.value );
    }
    return result;
}

static const QRegExp regExp1("[\\S]");
static const QRegExp regExp2("[\\s]");

bool MagicLanguage::loadText()
{
    QFile file(languageFilePath);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    QTextStream in(&file);
    in.readLine(); // skip header line

    while (!in.atEnd())
    {
        QString line = in.readLine();
        if (line.size() > 0)
        {
            int key_start = line.indexOf(regExp1);
            if (key_start != -1 && line.at(key_start) != '#')
            {
                int key_end = line.indexOf(regExp2, key_start);
                if (key_end != -1)
                {
                    int value_start = line.indexOf(regExp1, key_end);
                    if (value_start != -1)
                    {
                        strings.insert(
                            getStringFormattedWithVars(line.mid(key_start, key_end - key_start)),
                            getStringFormattedWithVars(line.mid(value_start))
                        );
                        //TestMessage(L"key: \"%s\" value: \"%s\"", getStringFormattedWithVars(line.mid(key_start, key_end - key_start)).toStdWString().c_str(),
                        //    getStringFormattedWithVars(line.mid(value_start)).toStdWString().c_str());
                    }
                }
            }
        }
    }
    return true;
}

bool MagicLanguage::getLanguageInfo(QString filepath, LanguageInfo &info) {
    QFile file(filepath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        if (!in.atEnd()) {
            QString line = in.readLine();
            QStringList strList = line.split('|');

            // MAGL|1|English|English|ENG|Magic.TXD Team

            if (strList.size() >= 6 && strList[0] == "MAGL"){
                int version = strList[1].toInt();
                if (version >= MINIMUM_SUPPORTED_MAGL_VERSION && version <= CURRENT_MAGL_VERSION) {
                    info.version = version;
                    info.name = strList[2];
                    info.nameInOriginal = strList[3];
                    info.abbr = strList[4];
                    info.authors = strList[5];

                    //TestMessage(L"vesrion: %d, name: %s, original: %s, abbr: %s, authors: %s", info.version,
                    //    info.name.toStdWString().c_str(), info.nameInOriginal.toStdWString().c_str(), 
                    //    info.abbr.toStdWString().c_str(), info.authors.toStdWString().c_str());

                    return true;
                }
            }
        }
    }
    return false;
}

void MagicLanguage::clearText() {
    strings.clear();
}

QString MagicLanguage::keyNotDefined(QString key) {
    if (key.size() > 0)
        return "N_" + key;
    else
        return "EMPTY_KEY";
}

QString MagicLanguage::getText(QString key, bool *found) {
    auto i = strings.find(key);
    if (i != strings.end()) {
        if (found)
            *found = true;
        return i.value();
    }
    else {
        if (found)
            *found = false;
        return keyNotDefined(key);
    }
}

unsigned int MagicLanguages::getNumberOfLanguages() {
    return languages.size();
}

QString MagicLanguages::getByKey(QString key, bool *found) {
    if (currentLanguage != -1)
        return languages[currentLanguage].getText(key, found);
    else {
        if (found)
            *found = false;
        return MagicLanguage::keyNotDefined(key);
    }
}

void MagicLanguages::scanForLanguages(QString languagesFolder)
{
    QDirIterator dirIt(languagesFolder);

    while (dirIt.hasNext())
    {
        dirIt.next();

        QFileInfo fileInfo( dirIt.fileInfo() );

        if (fileInfo.isFile())
        {
            if (fileInfo.suffix() == "magl")
            {
                MagicLanguage::LanguageInfo info;
                QString filePath = dirIt.filePath();

                if (MagicLanguage::getLanguageInfo(filePath, info))
                {
                    int newLangIndex = languages.size();

                    languages.resize(newLangIndex + 1);

                    MagicLanguage& theLang = languages[ newLangIndex ];
                    theLang.languageFilePath = filePath;
                    theLang.languageFileName = dirIt.fileName();
                    theLang.info = info;
                }
            }
        }
    }
}

void MagicLanguages::updateLanguageContext( void )
{
    // We want to update all language sensitive items.
    MainWindow *mainWnd = this->mainWnd;

    for ( magicTextLocalizationItem *localItem : this->culturalItems )
    {
        // Go ahead and tell our customer!
        localItem->updateContent( mainWnd );
    }
}

bool MagicLanguages::selectLanguageByIndex(unsigned int index) {
    const unsigned int numLanguages = getNumberOfLanguages();
    if (numLanguages > 0 && index < numLanguages) {
        if (currentLanguage != -1)
            languages[currentLanguage].clearText();
        currentLanguage = index;
        languages[currentLanguage].loadText();

        // Update GUI.
        updateLanguageContext();
        return true;
    }
    return false;
}

bool MagicLanguages::selectLanguageByLanguageName(QString langName) {
    const unsigned int numLanguages = getNumberOfLanguages();
    for (unsigned int i = 0; i < numLanguages; i++) {
        if (languages[i].info.name == langName)
            return selectLanguageByIndex(i);
    }
    return false;
}

bool MagicLanguages::selectLanguageByLanguageAbbr(QString abbr) {
    const unsigned int numLanguages = getNumberOfLanguages();
    for (unsigned int i = 0; i < numLanguages; i++) {
        if (languages[i].info.abbr == abbr)
            return selectLanguageByIndex(i);
    }
    return false;
}

bool MagicLanguages::selectLanguageByFileName(QString filename) {
    const unsigned int numLanguages = getNumberOfLanguages();
    for (unsigned int i = 0; i < numLanguages; i++) {
        if (languages[i].languageFileName == filename)
            return selectLanguageByIndex(i);
    }
    return false;
}

unsigned int GetTextWidthInPixels(QString &text, unsigned int fontSize) {
    QFont font("Segoe UI Light");
    font.setPixelSize(fontSize);
    QFontMetrics fm(font);
    return fm.width(text);
}