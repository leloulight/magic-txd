#pragma once
#include <QString>
#include <stdio.h>

class styles
{
public:
    static QString get(QString filename)
    {
        FILE *file;
        fopen_s(&file, filename.toUtf8(), "rt");
        char line[512];
        QString str;
        while(fgets(line, 512, file))
            str += line;
        fclose(file);
        return str;
    }
};
