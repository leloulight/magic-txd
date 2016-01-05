#include "mainwindow.h"

#include "languages.h"
#include <QFile>
#include <QTextStream>
#include "testmessage.h"

MagicLanguages ourLanguages;

const char *valueVars[] = {
    "_PARAM_1",        "%1",
    "_MAGIC_TXD_NAME", "Magic.TXD",
    "_AUTHOR_NAME_1",  "DK22Pac",
    "_AUTHOR_NAME_2",  "The_GTA",
};

#define NUM_VALUE_VARS 4

QString MagicLanguage::getStringFormattedWithVars(QString &string) {
    QString result = string;
    for (unsigned int i = 0; i < NUM_VALUE_VARS; i++)
        result.replace(valueVars[i * 2], valueVars[i * 2 + 1]);
    return result;
}

bool MagicLanguage::loadText() {
    QFile file(languageFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;
    QTextStream in(&file);
    in.readLine(); // skip header line
    while (!in.atEnd()) {
        QString line = in.readLine();
        if (line.size() > 0) {
            int key_start = line.indexOf(QRegExp("[\\S]"));
            if (key_start != -1 && line.at(key_start) != '#') {
                int key_end = line.indexOf(QRegExp("[\\s]"), key_start);
                if (key_end != -1) {
                    int value_start = line.indexOf(QRegExp("[\\S]"), key_end);
                    if (value_start != -1) {
                        strings.insert(getStringFormattedWithVars(line.mid(key_start, key_end - key_start)),
                            getStringFormattedWithVars(line.mid(value_start)));
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

MagicLanguages::MagicLanguages() {
    currentLanguage = -1;
}

void MagicLanguages::scanForLanguages(QString languagesFolder) {
    QDirIterator dirIt(languagesFolder);
    while (dirIt.hasNext()) {
        dirIt.next();
        if (QFileInfo(dirIt.filePath()).isFile()) {
            if (QFileInfo(dirIt.filePath()).suffix() == "magl") {
                MagicLanguage::LanguageInfo info;
                if (MagicLanguage::getLanguageInfo(dirIt.filePath(), info)) {
                    languages.resize(languages.size() + 1);
                    languages[languages.size() - 1].languageFilePath = dirIt.filePath();
                    languages[languages.size() - 1].languageFileName = dirIt.fileName();
                    languages[languages.size() - 1].info = info;
                }
            }
        }
    }
}

bool MagicLanguages::selectLanguageByIndex(unsigned int index) {
    if (getNumberOfLanguages() > 0 && index < getNumberOfLanguages()) {
        if (currentLanguage != -1)
            languages[currentLanguage].clearText();
        currentLanguage = index;
        languages[currentLanguage].loadText();
        return true;
    }
    return false;
}

bool MagicLanguages::selectLanguageByLanguageName(QString langName) {
    for (unsigned int i = 0; i < getNumberOfLanguages(); i++) {
        if (languages[i].info.name == langName)
            return selectLanguageByIndex(i);
    }
    return false;
}

bool MagicLanguages::selectLanguageByLanguageAbbr(QString abbr) {
    for (unsigned int i = 0; i < getNumberOfLanguages(); i++) {
        if (languages[i].info.abbr == abbr)
            return selectLanguageByIndex(i);
    }
    return false;
}

bool MagicLanguages::selectLanguageByFileName(QString filename) {
    for (unsigned int i = 0; i < getNumberOfLanguages(); i++) {
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