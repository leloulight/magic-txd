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
        errno_t errnum = fopen_s(&file, fullPath.toUtf8(), "rt");

        QString str;

        if ( errnum == 0 )
        {
            char line[512];
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
        }

        return str;
    }
};
