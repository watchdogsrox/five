
#ifndef SAVEGAME_SAVE_BLOCK_H
#define SAVEGAME_SAVE_BLOCK_H

//Rage headers
#include "system/param.h"
#include "rline/rl.h"
#include "rline/ros/rlros.h"
#include "rline/ros/rlroscommon.h"

// Game headers
#include "SaveLoad/savegame_data.h"
#include "SaveLoad/savegame_defines.h"
#include "network/Network.h"


enum eSaveBlockState
{
	SAVE_BLOCK_DO_NOTHING = 0,
	SAVE_BLOCK_INITIAL_CHECKS,
	SAVE_BLOCK_BEGIN_SAVE,
	SAVE_BLOCK_DO_SAVE,
	SAVE_BLOCK_HANDLE_ERRORS
};

/*
enum eLocalBlockSaveSubstate
{
	LOCAL_BLOCK_SAVE_SUBSTATE_BEGIN_SAVE,
	LOCAL_BLOCK_SAVE_SUBSTATE_CHECK_SAVE,
#if __PPU
	LOCAL_BLOCK_SAVE_SUBSTATE_GET_MODIFIED_TIME,
#endif	//	__PPU
//	LOCAL_BLOCK_SAVE_SUBSTATE_BEGIN_ICON_SAVE,
//	LOCAL_BLOCK_SAVE_SUBSTATE_CHECK_ICON_SAVE
};
*/

enum eCloudBlockSaveSubstate
{
	CLOUD_BLOCK_SAVE_SUBSTATE_INIT,
	CLOUD_BLOCK_SAVE_SUBSTATE_UPDATE
#if __ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME
	,CLOUD_BLOCK_SAVE_SUBSTATE_INIT_JSON_SAVE,
	CLOUD_BLOCK_SAVE_SUBSTATE_UPDATE_JSON_SAVE
#endif	//	__ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME
};


// Forward declaration
class CloudFileSaveListener;

class CSavegameSaveBlock
{
private:
//	Private data
	static CloudFileSaveListener *ms_pCloudFileSaveListener;

	static eSaveBlockState	ms_SaveBlockStatus;
//	static eLocalBlockSaveSubstate ms_LocalBlockSaveSubstate;
	static eCloudBlockSaveSubstate ms_CloudBlockSaveSubstate;

	static u32 ms_SizeOfBufferToBeSaved;
	static const u8 *ms_pBufferToBeSaved;
//	static bool ms_IsCloudOperation;

#if __ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME
	static bool ms_bIsAPhoto;

	static const char *ms_pJsonData;
#endif	//	__ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME

//	Private functions
	static MemoryCardError DoSaveBlockCloud();
//	static MemoryCardError DoSaveBlock();


	static MemoryCardError BeginSaveToCloud(const char *pFilename, const u8 *pBufferToSave, u32 SizeOfBufferToSave
#if __ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME
		, bool bIsAPhoto, const char *pJsonData
#endif	//	__ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME
		);

	static void EndSaveBlockCloud();
//	static void EndSaveBlock();

public:
//	Public functions
	static void Init(unsigned initMode);
    static void Shutdown(unsigned shutdownMode);

//	static MemoryCardError BeginSaveBlock(int SlotNumber, const u8 *pBufferToSave, u32 SizeOfBufferToSave, const bool isCloudOperation, const char *pJsonData, bool bIsPhotoFile);

	static MemoryCardError BeginSaveOfMissionCreatorPhoto(const char *pFilename, const u8 *pBufferToSave, u32 SizeOfBufferToSave);

#if __ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME
	static MemoryCardError BeginUploadOfSavegameToCloud(const char *pFilename, const u8 *pBufferToSave, u32 SizeOfBufferToSave, const char *pJsonData);
#endif	//	__ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME

	static MemoryCardError SaveBlock(bool bDisplayErrors = true);
};

#endif	//	SAVEGAME_SAVE_BLOCK_H

