
#ifndef SAVEGAME_LOAD_PHOTO_LOCAL_H
#define SAVEGAME_LOAD_PHOTO_LOCAL_H

// Game headers
#include "SaveLoad/savegame_defines.h"

// Forward declarations
class CPhotoBuffer;
class CSavegamePhotoUniqueId;


class CSavegamePhotoLoadLocal
{
	enum eLoadPhotoLocalState
	{
		LOAD_PHOTO_LOCAL_DO_NOTHING = 0,
		LOAD_PHOTO_LOCAL_BEGIN_INITIAL_CHECKS,
		LOAD_PHOTO_LOCAL_DO_INITIAL_CHECKS,
		LOAD_PHOTO_LOCAL_GET_FILE_SIZE,
		LOAD_PHOTO_LOCAL_ALLOCATE_BUFFER,				//	Leave it to the caller to free the allocated buffer
		LOAD_PHOTO_LOCAL_BEGIN_CREATOR_CHECK,
		LOAD_PHOTO_LOCAL_DO_CREATOR_CHECK,
		LOAD_PHOTO_LOCAL_BEGIN_LOAD,
		LOAD_PHOTO_LOCAL_DO_LOAD,
//		LOAD_PHOTO_LOCAL_REQUEST_TEXTURE_DECODE,		//	Maybe these two should be done in the load photo queued operation. Or Init() can take a flag to say whether to do this or not
//		LOAD_PHOTO_LOCAL_WAIT_FOR_TEXTURE_DECODE,		
//		LOAD_PHOTO_LOCAL_WAIT_FOR_LOADING_MESSAGE_TO_FINISH,
		LOAD_PHOTO_LOCAL_HANDLE_ERRORS
	};

public:

	//	Public functions
	static void Init(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);

	static MemoryCardError BeginLocalPhotoLoad(const CSavegamePhotoUniqueId &uniqueId, CPhotoBuffer *pPhotoBuffer);

	static MemoryCardError DoLocalPhotoLoad(bool bDisplayErrors = true);

private:
	static void EndLocalPhotoLoad();

private:
	//	Private data
	static eLoadPhotoLocalState		ms_LoadLocalPhotoStatus;

	static CPhotoBuffer *ms_pPhotoBuffer;
	static u32 m_SizeOfAllocatedBuffer;
};


#endif	//	SAVEGAME_LOAD_PHOTO_LOCAL_H

