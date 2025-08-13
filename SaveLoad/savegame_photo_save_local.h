
#ifndef SAVEGAME_SAVE_PHOTO_LOCAL_H
#define SAVEGAME_SAVE_PHOTO_LOCAL_H

// Game headers
#include "SaveLoad/savegame_defines.h"

// Forward declarations
class CPhotoBuffer;
class CSavegamePhotoUniqueId;

class CSavegamePhotoSaveLocal
{
	enum eSavePhotoLocalState
	{
		SAVE_PHOTO_LOCAL_DO_NOTHING = 0,
		SAVE_PHOTO_LOCAL_BEGIN_INITIAL_CHECKS,
		SAVE_PHOTO_LOCAL_DO_INITIAL_CHECKS,
		SAVE_PHOTO_LOCAL_BEGIN_SAVE,
		SAVE_PHOTO_LOCAL_DO_SAVE,
		SAVE_PHOTO_LOCAL_BEGIN_ICON_SAVE,
		SAVE_PHOTO_LOCAL_DO_ICON_SAVE,
		SAVE_PHOTO_LOCAL_HANDLE_ERRORS
	};

public:
	//	Public functions
	static void Init(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);

	static MemoryCardError BeginSavePhotoLocal(const CSavegamePhotoUniqueId &uniqueId, const CPhotoBuffer *pPhotoBuffer);

	static MemoryCardError DoSavePhotoLocal(bool bDisplayErrors = true);

private:
	static void EndSavePhotoLocal();

private:
	//	Private data
	static eSavePhotoLocalState	ms_SavePhotoStatus;

	static const CPhotoBuffer *ms_pPhotoBuffer;
};


#endif	//	SAVEGAME_SAVE_PHOTO_LOCAL_H

