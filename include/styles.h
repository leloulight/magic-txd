#pragma once
#include <QString>
#include <stdio.h>

class styles
{
public:
    static QString get(QString appPath, QString filename)
    {
        FILE *file;
        QString fullPath = appPath + "\\" + filename;
        QString newAppPath = appPath;
        newAppPath.replace('\\', '/');
        fopen_s(&file, fullPath.toUtf8(), "rt");
        char line[512];
        QString str;
        while (fgets(line, 512, file)) {
            if (*line == '$') {
                QString tmp;
                tmp.append(&line[1]);
                tmp.replace("url(", "url(" + newAppPath + "/");
                str += tmp;
            }
            else {
                str += line;
            }
        }
        fclose(file);
        return str;
    }
};
