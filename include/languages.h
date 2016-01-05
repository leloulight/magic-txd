#pragma once

#include <QHash>
#include <QVector>

#include <QDirIterator>

#define CURRENT_MAGL_VERSION 1
#define MINIMUM_SUPPORTED_MAGL_VERSION 1

// TODO: Maybe move it somewhere
unsigned int GetTextWidthInPixels(QString &text, unsigned int fontSize);

class MagicLanguage {
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

class MagicLanguages {
public:
    QVector<MagicLanguage> languages;

    int currentLanguage; // index of current language in languages array, -1 if not defined

    unsigned int getNumberOfLanguages();

    QString getByKey(QString key, bool *found = NULL);

    MagicLanguages();

    void scanForLanguages(QString languagesFolder);

    bool selectLanguageByIndex(unsigned int index);

    bool selectLanguageByLanguageName(QString langName);

    bool selectLanguageByLanguageAbbr(QString abbr);

    bool selectLanguageByFileName(QString filename);
};

extern MagicLanguages ourLanguages;

#define MAGIC_TEXT(a) ourLanguages.getByKey(a)

#define MAGIC_TEXT_CHECK_IF_FOUND(a, b) ourLanguages.getByKey(a, b)