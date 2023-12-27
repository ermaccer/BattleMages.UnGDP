// BattleMages.UnGDP.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <memory>
#include <filesystem>

#include "zlib.h"
#pragma comment(lib, "zlib.lib")

#include <Windows.h>

struct gdp_header {
    char header[4]; // GDP/1
    unsigned int dateHigh;
    unsigned int dateLow;
    int files;
    int fileInfoSize;
    int pad[4] = {};
};

struct gdp_entry {
    int entrySize;
    unsigned int dateHigh;
    unsigned int dateLow;
    int isCompressed;
    int fileOffset;
    int size;
    int rawSize;
    int pad[4] = { };
};

struct file_entry {
    gdp_entry header;
    std::string name;
};

enum eModes {
    MODE_EXTRACT = 1,
    MODE_CREATE
};


void set_file_date(std::string file, unsigned int low, unsigned int high)
{
    HANDLE hFile = CreateFileA(file.c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hFile == INVALID_HANDLE_VALUE)
    {
        std::cout << "ERROR: [WIN32] Could not open " << file.c_str() << " ERROR: " << GetLastError() << std::endl;
        return;
    }

    FILETIME ft;
    ZeroMemory(&ft, sizeof(FILETIME));

    ft.dwLowDateTime = low;
    ft.dwHighDateTime = high;

    BOOL bResult = SetFileTime(hFile, NULL, NULL, &ft);
    if (!bResult)
    {
        std::cout << "ERROR: [WIN32] Could not set date for " << file.c_str() << " ERROR: " << GetLastError() << std::endl;
        return;
    }
    CloseHandle(hFile);
}

void get_file_date(std::string file, unsigned int* low, unsigned int* high)
{
    HANDLE hFile = CreateFileA(file.c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hFile == INVALID_HANDLE_VALUE)
    {
        std::cout << "ERROR: [WIN32] Could not open " << file.c_str() << " ERROR: " << GetLastError() << std::endl;
        return;
    }

    FILETIME ft;
    ZeroMemory(&ft, sizeof(FILETIME));

    BOOL bResult = GetFileTime(hFile, NULL, NULL, &ft);
    if (!bResult)
    {
        std::cout << "ERROR: [WIN32] Could not set date for " << file.c_str() << " ERROR: " << GetLastError() << std::endl;
        return;
    }
    CloseHandle(hFile);

    *low = ft.dwLowDateTime;
    *high = ft.dwHighDateTime;
}


int main(int argc, char* argv[])
{
    bool _s_switch = false;
    std::string o_param;


    if (argc == 1)
    {
        std::cout << "Battle Mages UnGDP - Extract GDP archives from BM and BM: SoD by ermaccer\n"
            << "Usage: ungdp <optional params> <file/folder>\n"
            << "    -s	Skip file dates.\n"
            << "    -o <name> Specify output folder or file.\n";
        return 0;
    }

    // params
    for (int i = 1; i < argc - 1; i++)
    {
        if (argv[i][0] != '-' || strlen(argv[i]) != 2) {
            return 1;
        }
        switch (argv[i][1])
        {
        case 's': _s_switch = true;
            break;
        case 'o':
            i++;
            o_param = argv[i];
            break;
        default:
            std::cout << "ERROR: Param does not exist: " << argv[i] << std::endl;
            return 0;
            break;
        }
    }

    std::string input;

    if (argc > 2)
        input = argv[argc - 1];
    else if (argc == 2)
        input = argv[1];

    if (input.empty())
        return 0;

    int mode = 0;

    if (!std::filesystem::exists(input))
    {
        std::cout << "ERROR: " << input << " does not exist!" << std::endl;
        return 1;
    }

    if (std::filesystem::is_directory(input))
        mode = MODE_CREATE;
    else
        mode = MODE_EXTRACT;

    if (mode == MODE_EXTRACT)
    {
        std::ifstream pFile(input, std::ifstream::binary);
        if (!pFile)
        {
            std::cout << "ERROR: Could not open " << input << "!" << std::endl;
            return 1;
        }

        gdp_header gdp;
        pFile.read((char*)&gdp, sizeof(gdp_header));

        if (!(gdp.header[3] == 0x1 && gdp.header[2] == 'P' && gdp.header[1] == 'D' && gdp.header[0] == 'G'))
        {
            std::cout << "ERROR: " << input << " is not a valid Battle Mages archive!" << std::endl;
            return 1;
        }

        std::string outputFolder = input;
        outputFolder += "_out";

        if (!o_param.empty())
            outputFolder = o_param;

        std::filesystem::create_directory(outputFolder);
        std::filesystem::current_path(outputFolder);

        std::vector<file_entry> vFiles;

        for (int i = 0; i < gdp.files; i++)
        {
            file_entry file;
            pFile.read((char*)&file.header, sizeof(gdp_entry));
            std::getline(pFile, file.name, '\0');
            vFiles.push_back(file);
        }

        for (auto& f : vFiles)
        {
            std::cout << "Processing: " << f.name << std::endl;

            auto fnd = f.name.find_last_of("/\\");

            if (!(fnd == std::string::npos))
            {
                std::string folderPath;
                folderPath = f.name.substr(0, fnd);
                std::filesystem::create_directories(folderPath);
            }

            std::ofstream oFile(f.name, std::ofstream::binary);

            int dataSize = f.header.size;
            auto dataBuff = std::make_unique<char[]>(dataSize);

            pFile.seekg(f.header.fileOffset, pFile.beg);
            pFile.read(dataBuff.get(), dataSize);

            if (f.header.isCompressed)
            {
                int rawSize = f.header.rawSize;
                auto rawBuff = std::make_unique<char[]>(rawSize);

                int zlib_result = uncompress((Bytef*)rawBuff.get(), (uLongf*)&rawSize, (Bytef*)dataBuff.get(), dataSize);
                if (zlib_result < 0)
                {
                    std::cout << "ERROR: Failed to decompress file " << f.name << "! Error: " << zlib_result << std::endl;
                    return 1;
                }
                oFile.write(rawBuff.get(), rawSize);
            }
            else
                oFile.write(dataBuff.get(), dataSize);

            oFile.close();
            if (!_s_switch)
            set_file_date(f.name, f.header.dateLow, f.header.dateHigh);

        }
        pFile.close();
        std::cout << "Finished." << std::endl;
    }

    if (mode == MODE_CREATE)
    {
        std::string outputFile = input;
        outputFile += ".gdp";

        if (!o_param.empty())
            outputFile = o_param;

        std::ofstream oFile(outputFile, std::ofstream::binary);

        std::filesystem::path folder(input);
        std::filesystem::current_path(folder);

        std::vector<file_entry> vFiles;

        std::cout << "Scanning folder " << folder.string() << "." << std::endl;

        for (auto& file : std::filesystem::recursive_directory_iterator("."))
        {
            if (!file.is_directory())
            {
                file_entry f;
                f.name = file.path().string();
                vFiles.push_back(f);
            }
                
        }

        for (auto& file : vFiles)
        {
            auto fnd = file.name.find_first_of(".\\");
            if (!(fnd == std::string::npos))
                file.name = file.name.substr(fnd + strlen(".\\"));
        }


        std::cout << "Found " << vFiles.size() << " files." << std::endl;

        SYSTEMTIME st;
        ZeroMemory(&st, sizeof(SYSTEMTIME));
        GetSystemTime(&st);

        FILETIME ft;
        ZeroMemory(&ft, sizeof(FILETIME));
        SystemTimeToFileTime(&st, &ft);

        gdp_header gdp;
        gdp.header[0] = 'G';
        gdp.header[1] = 'D';
        gdp.header[2] = 'P';
        gdp.header[3] = 0x1;

        gdp.files = vFiles.size();
        gdp.dateLow = ft.dwLowDateTime;
        gdp.dateHigh = ft.dwHighDateTime;
        
        int fileInfoSize = 0;

        for (auto& file : vFiles)
        {
            fileInfoSize += sizeof(gdp_entry);
            fileInfoSize += file.name.length() + 1;
        }

        gdp.fileInfoSize = fileInfoSize;
   
        oFile.write((char*)&gdp, sizeof(gdp_header));
        std::cout << "Header saved." << std::endl;
        int baseOffset = gdp.fileInfoSize + sizeof(gdp_header);

        for (auto& file : vFiles)
        {
            int fileSize = std::filesystem::file_size(file.name);

            gdp_entry entry;
            ZeroMemory(&entry, sizeof(gdp_entry));

            unsigned int dateLow = 0;
            unsigned int dateHigh = 0;
            get_file_date(file.name, &dateLow, &dateHigh);

            entry.entrySize = sizeof(gdp_entry) + file.name.length() + 1;
            entry.size = entry.rawSize = fileSize;
            entry.fileOffset = baseOffset;
            entry.isCompressed = 0;
            entry.dateLow = dateLow;
            entry.dateHigh = dateHigh;

            oFile.write((char*)&entry, sizeof(entry));
            
            std::string fixedName = file.name;
            std::replace(fixedName.begin(), fixedName.end(), '\\', '/');


            oFile.write((char*)fixedName.data(), fixedName.length());
            char nullChar = 0x00;
            oFile.write((char*)&nullChar, sizeof(char));

            baseOffset += fileSize;
        }
        std::cout << "File info saved." << std::endl;
        for (auto& file : vFiles)
        {
            std::cout << "Processing: " << file.name << std::endl;
            int fileSize = std::filesystem::file_size(file.name);

            auto dataBuff = std::make_unique<char[]>(fileSize);
            
            std::ifstream pFile(file.name, std::ifstream::binary);
            if (!pFile)
            {
                std::cout << "ERROR: Failed to open file " << file.name << "!" << std::endl;
                return 1;
            }

            pFile.read(dataBuff.get(), fileSize);
            pFile.close();
            oFile.write(dataBuff.get(), fileSize);
        }
        std::cout << "Finished." << std::endl;
        oFile.close();
    }
    
    return 0;
}

