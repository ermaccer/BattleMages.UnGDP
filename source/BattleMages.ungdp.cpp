// BattleMages.ungdp.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <iostream>
#include <fstream>
#include <memory>
#include <string>
#include <filesystem>

#include "filef.h"
#include "zlib\zlib.h"

#pragma comment (lib, "zlib/zdll.lib" )

struct bm_gdp_header
{
	char  header[4];  //GDP 0x1
	int  unk[2];
	int  files;
	int  structSize;
	char pad[16];
};

struct bm_gdp_entry {
	int structSize;
	int magicNumber;
	unsigned int unk;
	int flag;
	int offset;
	int size; // zlib
	int rawSize;
	char pad[16];
	// name
};

int main(int argc, char* argv[])
{
	if (argc == 1)
	{
		std::cout << "Battle Mages UnGDP - Extract GDP archives from BM and SoD\n"
			"Usage: ungdp <file>\n";
    }

	if (argv[1])
	{
		std::ifstream pFile(argv[1], std::ifstream::binary);

		if (!pFile)
		{
			std::cout << "ERROR: Could not open " << argv[1] << "!" << std::endl;
			return 1;
		}
		if (pFile)
		{
			bm_gdp_header header;
			pFile.read((char*)&header, sizeof(bm_gdp_header));

			if (!(header.header[3] == 0x1 && header.header[2] == 'P' && header.header[1] == 'D' && header.header[0] == 'G'))
			{
				std::cout << "ERROR: " << argv[1] << " is not a Battle Mages archive!" << std::endl;
				return 1;
			}

			std::unique_ptr<bm_gdp_entry[]> entries = std::make_unique<bm_gdp_entry[]>(header.files);
			std::unique_ptr<std::string[]>  names = std::make_unique<std::string[]>(header.files);

			for (int i = 0; i < header.files; i++)
			{
				pFile.read((char*)&entries[i], sizeof(bm_gdp_entry));
				std::getline(pFile, names[i],'\0');
			}

			for (int i = 0; i < header.files; i++)
			{
				pFile.seekg(entries[i].offset, pFile.beg);

				std::unique_ptr<char[]> dataBuff = std::make_unique<char[]>(entries[i].size);
				pFile.read(dataBuff.get(), entries[i].size);

				std::cout << "Processing: " << names[i] << std::endl;
				if (checkSlash(names[i]))
					std::experimental::filesystem::create_directories(splitString(names[i], false));

				std::ofstream oFile(names[i], std::ofstream::binary);



				if (entries[i].flag)
				{
					std::unique_ptr<char[]> uncompressedBuffer = std::make_unique<char[]>(entries[i].rawSize);


					unsigned long uncompressedSize = entries[i].rawSize;
					int zlib_output = uncompress((Bytef*)uncompressedBuffer.get(), &uncompressedSize,
						(Bytef*)dataBuff.get(), entries[i].size);


					if (zlib_output == Z_MEM_ERROR) {
						std::cout << "ERROR: ZLIB: Out of memory!" << std::endl;
						return 1;
					}
					oFile.write(uncompressedBuffer.get(), entries[i].size);
				}
				else
					oFile.write(dataBuff.get(), entries[i].size);





			}
			std::cout << "Finished." << std::endl;
		}
	}
	return 0;
}

