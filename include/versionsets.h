#pragma once

#include <string>
#include <vector>
#include <algorithm>
#include <renderware.h>

class RwVersionSets {
public:
    enum ePlatformType {
        RWVS_PL_NOT_DEFINED,
        RWVS_PL_PC,
        RWVS_PL_PS2,
        RWVS_PL_PSP,
        RWVS_PL_XBOX,
        RWVS_PL_MOBILE,

        RWVS_PL_NUM_OF_PLATFORMS = RWVS_PL_MOBILE
    };

    enum eDataType {
        RWVS_DT_NOT_DEFINED,
        RWVS_DT_D3D8,
        RWVS_DT_D3D9,
        RWVS_DT_PS2,
        RWVS_DT_XBOX,
        RWVS_DT_AMD_COMPRESSED,
        RWVS_DT_S3TC_MOBILE,
        RWVS_DT_UNCOMPRESSED_MOBILE,
        RWVS_DT_POWERVR,
        RWVS_DT_PSP,

        RWVS_DT_NUM_OF_TYPES = RWVS_DT_PSP
    };

    // translate name from settings file to id
    static ePlatformType platformIdFromName(const char *name) {
        if (!strcmp(name, "PC"))
            return RWVS_PL_PC;
        if (!strcmp(name, "PS2"))
            return RWVS_PL_PS2;
        if (!strcmp(name, "PSP"))
            return RWVS_PL_PSP;
        if (!strcmp(name, "XBOX"))
            return RWVS_PL_XBOX;
        if (!strcmp(name, "MOBILE"))
            return RWVS_PL_MOBILE;
        return RWVS_PL_NOT_DEFINED;
    }

    static eDataType dataIdFromName(const char *name) {
        if (!strcmp(name, "D3D8"))
            return RWVS_DT_D3D8;
        if (!strcmp(name, "D3D9"))
            return RWVS_DT_D3D9;
        if (!strcmp(name, "PS2"))
            return RWVS_DT_PS2;
        if (!strcmp(name, "XBOX"))
            return RWVS_DT_XBOX;
        if (!strcmp(name, "AMD"))
            return RWVS_DT_AMD_COMPRESSED;
        if (!strcmp(name, "S3TC"))
            return RWVS_DT_S3TC_MOBILE;
        if (!strcmp(name, "UNCOMPRESSED"))
            return RWVS_DT_UNCOMPRESSED_MOBILE;
        if (!strcmp(name, "POWERVR"))
            return RWVS_DT_POWERVR;
        if (!strcmp(name, "PSP"))
            return RWVS_DT_PSP;
        return RWVS_DT_NOT_DEFINED;
    }

    // translate id to name for engine
    static const char *platformNameFromId(ePlatformType platform) {
        switch (platform) {
        case RWVS_PL_PC:
            return "PC";
        case RWVS_PL_PS2:
            return "PlayStation2";
        case RWVS_PL_PSP:
            return "PSP";
        case RWVS_PL_XBOX:
            return "XBOX";
        case RWVS_PL_MOBILE:
            return "Mobile";
        }
        return "Unknown";
    }

    static const char *dataNameFromId(eDataType dataType) {
        switch (dataType) {
        case RWVS_DT_D3D8:
            return "Direct3D8";
        case RWVS_DT_D3D9:
            return "Direct3D9";
        case RWVS_DT_PS2:
            return "PlayStation2";
        case RWVS_DT_XBOX:
            return "XBOX";
        case RWVS_DT_AMD_COMPRESSED:
            return "AMDCompressed";
        case RWVS_DT_S3TC_MOBILE:
            return "s3tc_mobile";
        case RWVS_DT_UNCOMPRESSED_MOBILE:
            return "uncompressed_mobile";
        case RWVS_DT_POWERVR:
            return "PowerVR";
        case RWVS_DT_PSP:
            return "PSP";
        }
        return "Unknown";
    }

    static eDataType dataIdFromEnginePlatformName(const char *name) {
        if (!strcmp(name, "Direct3D8"))
            return RWVS_DT_D3D8;
        if (!strcmp(name, "Direct3D9"))
            return RWVS_DT_D3D9;
        if (!strcmp(name, "PlayStation2"))
            return RWVS_DT_PS2;
        if (!strcmp(name, "XBOX"))
            return RWVS_DT_XBOX;
        if (!strcmp(name, "AMDCompressed"))
            return RWVS_DT_AMD_COMPRESSED;
        if (!strcmp(name, "s3tc_mobile"))
            return RWVS_DT_S3TC_MOBILE;
        if (!strcmp(name, "uncompressed_mobile"))
            return RWVS_DT_UNCOMPRESSED_MOBILE;
        if (!strcmp(name, "PowerVR"))
            return RWVS_DT_POWERVR;
        if (!strcmp(name, "PSP"))
            return RWVS_DT_PSP;
        return RWVS_DT_NOT_DEFINED;
    }

    static bool fileReadOneLine(FILE *file, std::string &line) {
        line.clear();
        if (feof(file))
            return false;
        int c = fgetc(file);
        while (c != EOF && c != '\r' && c != '\n') {
            line.push_back(c);
            c = fgetc(file);
        }
        if (c == '\r')
            fgetc(file);
        if (line.size() > 0 && line[0] == '#')
            return fileReadOneLine(file, line);
        return true;
    }

    static void removeSpaces(std::string &line) {
        line.erase(std::remove(line.begin(), line.end(), ' '), line.end());
        line.erase(std::remove(line.begin(), line.end(), '\t'), line.end());
    }

    static bool extractValue(const char *valueType, std::string &line, std::string &value) {
        value.clear();
        size_t start = line.find('(');
        size_t end = line.find(')');
        size_t strbeg = line.find_first_not_of(" \t");
        if (start != std::string::npos && end != std::string::npos && end > start) {
            if (!line.compare(strbeg, strlen(valueType), valueType)) {
                value = line.substr(start + 1, end - start - 1);
                return true;
            }
        }
        return false;
    }

    class Set {
    public:
        class Platform {
        public:
            ePlatformType platformType;
            rw::LibraryVersion version;
            std::vector<eDataType> availableDataTypes;

            void readInfo(FILE *file) {
                std::string line, value;
                RwVersionSets::fileReadOneLine(file, line);
                RwVersionSets::extractValue("RWVER", line, value);
                std::replace(value.begin(), value.end(), '.', ' ');
                unsigned int libMajor, libMinor, revMajor, revMinor;
                sscanf(value.c_str(), "%d %d %d %d", &libMajor, &libMinor, &revMajor, &revMinor);
                version.rwLibMajor = libMajor;
                version.rwLibMinor = libMinor;
                version.rwRevMajor = revMajor;
                version.rwRevMinor = revMinor;
                RwVersionSets::fileReadOneLine(file, line);
                RwVersionSets::extractValue("RWBUILD", line, value);
                unsigned int buildNumber;
                if (value.size() > 2 && value[0] == '0' && value[1] == 'x')
                    sscanf(&value.c_str()[2], "%X", &buildNumber);
                else if(value.size() > 0)
                    sscanf(value.c_str(), "%d", &buildNumber);
                else
                    buildNumber = 0xFFFF;
                version.buildNumber = buildNumber;
                RwVersionSets::fileReadOneLine(file, line);
                RwVersionSets::extractValue("DATATYPE", line, value);
                removeSpaces(value);
                if (value.size() > 0) {
                    std::string type;
                    eDataType dataType;
                    size_t start = 0;
                    size_t end = value.find(',', 0);
                    while (end != std::string::npos) {
                        std::string substr;
                        substr = value.substr(start, end - start);
                        dataType = RwVersionSets::dataIdFromName(substr.c_str());
                        if (dataType != RWVS_DT_NOT_DEFINED)
                            availableDataTypes.push_back(dataType);
                        start = end + 1;
                        end = value.find(',', start);
                    }
                    dataType = RwVersionSets::dataIdFromName(&value.c_str()[start]);
                    if (dataType != RWVS_DT_NOT_DEFINED)
                        availableDataTypes.push_back(dataType);
                }
            }
        };
        char name[64];
        std::vector<Platform> availablePlatforms;
    };

    std::vector<Set> sets;

    void addSet(const char *Name) {
        sets.resize(sets.size() + 1);
        strncpy(sets[sets.size() - 1].name, Name, 63);
        sets[sets.size() - 1].name[63] = '\0';
    }

    void readSetsFile(const wchar_t *filename) {
        FILE *file = _wfopen(filename, L"r");
        if (file) {
            std::string line, value;
            fileReadOneLine(file, line);
            while (RwVersionSets::extractValue("SET", line, value)) {
                addSet(value.c_str());
                while (fileReadOneLine(file, line) && RwVersionSets::extractValue("PLATFORM", line, value)) {
                    ePlatformType platform = RwVersionSets::platformIdFromName(value.c_str());
                    if (platform != RWVS_PL_NOT_DEFINED) {
                        RwVersionSets::Set& currentSet = sets[ sets.size() - 1 ];
                        unsigned int numPlatforms = currentSet.availablePlatforms.size();
                        currentSet.availablePlatforms.resize(numPlatforms + 1);
                        RwVersionSets::Set::Platform& currentPlatform = currentSet.availablePlatforms[numPlatforms];
                        currentPlatform.platformType = platform;
                        currentPlatform.readInfo(file);
                    }
                    else {
                        fileReadOneLine(file, line);
                        fileReadOneLine(file, line);
                        fileReadOneLine(file, line);
                    }
                }
            }
        }
    }

    bool matchSet(rw::LibraryVersion &libVersion, eDataType dataTypeId, int &setIndex, int &platformIndex, int &dataTypeIndex) {
        for (unsigned int set = 0; set < sets.size(); set++) {
            const RwVersionSets::Set& currentSet = sets[ set ];

            for (unsigned int p = 0; p < currentSet.availablePlatforms.size(); p++) {
                const RwVersionSets::Set::Platform& platform = currentSet.availablePlatforms[ p ];

                if (platform.version.rwLibMajor == libVersion.rwLibMajor
                    && platform.version.rwLibMinor == libVersion.rwLibMinor
                    && platform.version.rwRevMajor == libVersion.rwRevMajor
                    && platform.version.rwRevMinor == libVersion.rwRevMinor)
                {
                    for (unsigned int d = 0; d < platform.availableDataTypes.size(); d++) {
                        if (platform.availableDataTypes[d] == dataTypeId) {
                            setIndex = set;
                            platformIndex = p;
                            dataTypeIndex = d;
                            return true;
                        }
                    }
                }
            }
        }
        return false;
    }
};