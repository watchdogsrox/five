

#ifndef SAVEGAME_GET_FILE_SIZE_H
#define SAVEGAME_GET_FILE_SIZE_H

#include "SaveLoad/savegame_defines.h"

class CSavegameGetFileSize
{
public:
//	Public functions
	static MemoryCardError GetFileSize(const char *pFilename, bool bCalculateSizeOnDisk, u64 &fileSize);

};

#endif	//	SAVEGAME_GET_FILE_SIZE_H


