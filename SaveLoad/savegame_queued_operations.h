
#ifndef SAVEGAME_QUEUED_OPERATIONS_H
#define SAVEGAME_QUEUED_OPERATIONS_H


// Rage headers
#include "data/growbuffer.h"
#include "file/savegame.h"
#include "net/status.h"
#include "string/unicode.h"	//	For char16

// Game headers
#include "control/replay/ReplaySettings.h"
#include "control/replay/ReplaySupportClasses.h"
#include "control/replay/File/ReplayFileManager.h"
#include "control/replay/File/device_replay.h"
#include "SaveLoad/savegame_defines.h"
#include "SaveLoad/savegame_cloud.h"
#include "SaveLoad/savegame_filenames.h"
#include "SaveLoad/savegame_messages.h"
#include "SaveLoad/savegame_photo_buffer.h"
#include "SaveLoad/savegame_photo_metadata.h"
#include "SaveLoad/savegame_photo_unique_id.h"
#include "scene/DownloadableTextureManager.h"

#if __PPU
#define PS3_ORBIS_SWITCH(ps3,orbis) ps3
#elif RSG_ORBIS
#define PS3_ORBIS_SWITCH(ps3,orbis) orbis
#else
#define PS3_ORBIS_SWITCH(ps3,orbis)
#endif

enum eSaveQueuedOperations
{
	SAVE_QUEUED_PROFILE_LOAD,				//	There's currently no eSaveOperation for this
	SAVE_QUEUED_PROFILE_SAVE,				//	There's currently no eSaveOperation for this
#if RSG_ORBIS
	SAVE_QUEUED_SPACE_CHECKS,
	SAVE_QUEUED_PS4_DAMAGED_CHECK,
#endif	//	RSG_ORBIS
#if __LOAD_LOCAL_PHOTOS
	SAVE_QUEUED_LOAD_LOCAL_PHOTO,
#else	//	__LOAD_LOCAL_PHOTOS
	SAVE_QUEUED_LOAD_UGC_PHOTO,
#endif	//	__LOAD_LOCAL_PHOTOS
	SAVE_QUEUED_PHOTO_SAVE,
	SAVE_QUEUED_SAVE_PHOTO_FOR_MISSION_CREATOR,
	SAVE_QUEUED_LOAD_PHOTO_FOR_MISSION_CREATOR,		//	This is used to load the photo for a mission so that it can be resaved with a new UGC ID when the mission has been significantly modified
	SAVE_QUEUED_MP_STATS_LOAD,
	SAVE_QUEUED_MP_STATS_SAVE,
	SAVE_QUEUED_MANUAL_LOAD,
	SAVE_QUEUED_MANUAL_SAVE,
	SAVE_QUEUED_AUTOSAVE,
#if GTA_REPLAY
	SAVE_QUEUED_REPLAY_LOAD,
	SAVE_QUEUED_REPLAY_SAVE,
#endif // GTA_REPLAY
	SAVE_QUEUED_MISSION_REPEAT_LOAD,
	SAVE_QUEUED_MISSION_REPEAT_SAVE,
	SAVE_QUEUED_CHECK_FILE_EXISTS,
	SAVE_QUEUED_CREATE_SORTED_LIST_OF_SAVEGAMES,
#if __LOAD_LOCAL_PHOTOS
	SAVE_QUEUED_CREATE_SORTED_LIST_OF_LOCAL_PHOTOS,
	SAVE_QUEUED_UPDATE_METADATA_OF_LOCAL_AND_CLOUD_PHOTO,
	SAVE_QUEUED_UPLOAD_LOCAL_PHOTO_TO_CLOUD,
#else	//	__LOAD_LOCAL_PHOTOS
	SAVE_QUEUED_CREATE_SORTED_LIST_OF_PHOTOS,	//	There's currently no eSaveOperation for this
#endif	//	__LOAD_LOCAL_PHOTOS
	SAVE_QUEUED_UPLOAD_MUGSHOT,
	SAVE_QUEUED_DELETE_FILE,

	SAVE_QUEUED_SAVE_LOCAL_PHOTO,
	SAVE_QUEUED_LOAD_MOST_RECENT_SAVE,
#if GTA_REPLAY
	SAVE_QUEUED_SAVE_REPLAY_FILE,
	SAVE_QUEUED_LOAD_REPLAY_FILE,
	SAVE_QUEUED_LOAD_REPLAY_HEADER,
	SAVE_QUEUED_DELETE_REPLAY_CLIPS,
	SAVE_QUEUED_DELETE_REPLAY_FILE,
	SAVE_QUEUED_ENUMERATE_REPLAY_FILES,
	SAVE_QUEUED_LOAD_REPLAY_MONTAGE,
	SAVE_QUEUED_SAVE_REPLAY_MONTAGE,
	SAVE_QUEUED_REPLAY_UPDATE_FAVOURITES,
	SAVE_QUEUED_REPLAY_MULTI_DELETE,
#if USE_SAVE_DATA_MEMORY && !USE_DOWNLOAD0_FOR_AUTOSAVE_BACKUP
	SAVE_QUEUED_CREATE_BACKUP_OF_AUTOSAVE_SDM,
#endif	//	USE_SAVE_DATA_MEMORY && !USE_DOWNLOAD0_FOR_AUTOSAVE_BACKUP

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
	SAVE_QUEUED_EXPORT_SINGLE_PLAYER_SAVEGAME,
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
	SAVE_QUEUED_IMPORT_SINGLE_PLAYER_SAVEGAME,
	SAVE_QUEUED_IMPORT_SINGLE_PLAYER_SAVEGAME_INTO_FIXED_SLOT,
#endif // __ALLOW_IMPORT_OF_SP_SAVEGAMES

//	SAVE_QUEUED_AUTOLOAD	//	I still can't decide whether I should queue autoload for completeness.
							//	It should be completed before any other savegame operations are required.
							//	It seems to fit quite well in gamesessionstatemachine.cpp. I might just make 
							//	things less readable if I move it all in to this file.
#endif // GTA_REPLAY
};


// Forward declarations
class CPhotoBuffer;


class CSavegameQueuedOperation
{
	friend class CSavegameQueue;

public:
	CSavegameQueuedOperation(eSaveQueuedOperations operationType) 
		: m_Status(MEM_CARD_COMPLETE)
	{
		m_OperationType = operationType;
	}
	virtual ~CSavegameQueuedOperation() {}

	//	Return true to Pop() from queue
	virtual bool Update() = 0;
	virtual bool Shutdown() = 0;

	MemoryCardError GetStatus() { return m_Status; }
	void  ClearStatus() { m_Status = MEM_CARD_COMPLETE; }

	virtual bool IsAutosave() { return false; }

#if __BANK
	virtual const char *GetNameOfOperation() = 0;
#endif	//	__BANK

	eSaveQueuedOperations GetOperationType() { return m_OperationType; }

protected:
	MemoryCardError m_Status;

private:
	eSaveQueuedOperations m_OperationType;
};



#if RSG_ORBIS

class CSavegameQueuedOperation_SpaceCheck : public CSavegameQueuedOperation
{
	enum SpaceChecksProgress
	{
		SPACE_CHECKS_BEGIN,
		SPACE_CHECKS_PROCESS
	};

public:
	CSavegameQueuedOperation_SpaceCheck()
		: CSavegameQueuedOperation(SAVE_QUEUED_SPACE_CHECKS)
	{
	}

	virtual ~CSavegameQueuedOperation_SpaceCheck() {}

	void Init();

	//	Return true to Pop() from queue
	virtual bool Update();
	virtual bool Shutdown(){return false;}

#if __BANK
	virtual const char *GetNameOfOperation() { return "CSavegameQueuedOperation_SpaceCheck"; }
#endif	//	__BANK

	bool GetMinimumRequiredMemory() { return m_MemoryRequiredForBasicSaveGame; }

private:
	// Results
	s32					m_MemoryRequiredForBasicSaveGame;
	SpaceChecksProgress m_SpaceChecksProgress;
};

#endif // RSG_ORBIS


#if USE_DOWNLOAD0_FOR_PROFILE_BACKUP

class CSavegameQueuedOperation_ProfileLoad : public CSavegameQueuedOperation
{
	enum eProfileLoadProgress
	{
		PROFILE_LOAD_PROGRESS_BEGIN_GET_MAIN_PROFILE_MOD_TIME,
		PROFILE_LOAD_PROGRESS_CHECK_GET_MAIN_PROFILE_MOD_TIME,
		PROFILE_LOAD_PROGRESS_BEGIN_GET_BACKUP_PROFILE_MOD_TIME,
		PROFILE_LOAD_PROGRESS_CHECK_GET_BACKUP_PROFILE_MOD_TIME,
		PROFILE_LOAD_PROGRESS_BEGIN_LOAD_MOST_RECENT_PROFILE,
		PROFILE_LOAD_PROGRESS_CHECK_LOAD
	};

public:
	CSavegameQueuedOperation_ProfileLoad()
		: CSavegameQueuedOperation(SAVE_QUEUED_PROFILE_LOAD)
	{
	}

	virtual ~CSavegameQueuedOperation_ProfileLoad() {}

	void Init(s32 localIndex, void *pProfileBuffer, s32 BufferSize);

	//	Return true to Pop() from queue
	virtual bool Update();
	virtual bool Shutdown();

#if __BANK
	virtual const char *GetNameOfOperation() { return "ProfileLoad"; }
#endif	//	__BANK

	u32 GetSizeOfLoadedProfile() { return m_SizeOfLoadedProfile; }

private:
	bool BeginGetModifiedTime(s32 slotIndex, const char* OUTPUT_ONLY(pDebugString));
	MemoryCardError CheckGetModifiedTime(u32 &modTimeHigh, u32 &modTimeLow, const char* OUTPUT_ONLY(pDebugString));

private:
	// Input
	s32 m_localIndex;
	void *m_pProfileBuffer;
	s32 m_SizeOfProfileBuffer;

	s32 m_MainProfileSlot;
	s32 m_BackupProfileSlot;

	u64 m_MainProfileModTime;
	u64 m_BackupProfileModTime;

	// Results
	u32 m_SizeOfLoadedProfile;

	eProfileLoadProgress m_ProfileLoadProgress;
};



class CSavegameQueuedOperation_ProfileSave : public CSavegameQueuedOperation
{
	enum eProfileSaveProgress
	{
		PROFILE_SAVE_PROGRESS_BEGIN_SAVE,
		PROFILE_SAVE_PROGRESS_CHECK_SAVE
	};

public:

	enum eProfileSaveDestination
	{
		SAVE_BACKUP_PROFILE_ONLY,
		SAVE_MAIN_PROFILE_ONLY,
		SAVE_BOTH_PROFILES
	};

	CSavegameQueuedOperation_ProfileSave()
		: CSavegameQueuedOperation(SAVE_QUEUED_PROFILE_SAVE)
	{
	}

	virtual ~CSavegameQueuedOperation_ProfileSave() {}

	void Init(s32 localIndex, void *pProfileBuffer, s32 BufferSize, eProfileSaveDestination saveProfileDestination = SAVE_BACKUP_PROFILE_ONLY);

	//	Return true to Pop() from queue
	virtual bool Update();
	virtual bool Shutdown();

#if __BANK
	virtual const char *GetNameOfOperation() { return "ProfileSave"; }
#endif	//	__BANK

private:
	s32 m_localIndex;
	s32 m_SlotToSaveTo;
	void *m_pProfileBuffer;
	s32 m_SizeOfProfileBuffer;

	eProfileSaveProgress m_ProfileSaveProgress;

	eProfileSaveDestination m_SaveProfileDestination;
};

#else	//	USE_DOWNLOAD0_FOR_PROFILE_BACKUP

class CSavegameQueuedOperation_ProfileLoad : public CSavegameQueuedOperation
{
	enum eProfileLoadProgress
	{
		PROFILE_LOAD_PROGRESS_BEGIN_CREATOR_CHECK,
		PROFILE_LOAD_PROGRESS_CHECK_CREATOR_CHECK,
		PROFILE_LOAD_PROGRESS_BEGIN_LOAD,
		PROFILE_LOAD_PROGRESS_CHECK_LOAD
	};

public:
	CSavegameQueuedOperation_ProfileLoad()
		: CSavegameQueuedOperation(SAVE_QUEUED_PROFILE_LOAD)
	{
	}

	virtual ~CSavegameQueuedOperation_ProfileLoad() {}

	void Init(s32 localIndex, char *pFilenameOfPS3Profile, void *pProfileBuffer, s32 BufferSize);

	//	Return true to Pop() from queue
	virtual bool Update();
	virtual bool Shutdown();

#if __BANK
	virtual const char *GetNameOfOperation() { return "ProfileLoad"; }
#endif	//	__BANK

	u32 GetSizeOfLoadedProfile() { return m_SizeOfLoadedProfile; }

private:
	// Input
	s32 m_localIndex;
	char *m_FilenameOfPS3Profile;
	void *m_pProfileBuffer;
	s32 m_SizeOfProfileBuffer;

	// Results
	u32 m_SizeOfLoadedProfile;

	eProfileLoadProgress m_ProfileLoadProgress;
};



class CSavegameQueuedOperation_ProfileSave : public CSavegameQueuedOperation
{
	static const u32 LengthOfProfileName = 12;

	enum eProfileSaveProgress
	{
		PROFILE_SAVE_PROGRESS_BEGIN_SAVE,
		PROFILE_SAVE_PROGRESS_CHECK_SAVE
	};

public:
	CSavegameQueuedOperation_ProfileSave()
		: CSavegameQueuedOperation(SAVE_QUEUED_PROFILE_SAVE)
	{
	}

	virtual ~CSavegameQueuedOperation_ProfileSave() {}

	void Init(s32 localIndex, char16 ProfileName[LengthOfProfileName], char *pFilenameOfPS3Profile, void *pProfileBuffer, s32 BufferSize);

	//	Return true to Pop() from queue
	virtual bool Update();
	virtual bool Shutdown();

#if __BANK
	virtual const char *GetNameOfOperation() { return "ProfileSave"; }
#endif	//	__BANK

private:
	// Input
	s32 m_localIndex;
	char16 m_ProfileName[LengthOfProfileName];
	char *m_FilenameOfPS3Profile;
	void *m_pProfileBuffer;
	s32 m_SizeOfProfileBuffer;

	eProfileSaveProgress m_ProfileSaveProgress;
};

#endif	//	USE_DOWNLOAD0_FOR_PROFILE_BACKUP

#if RSG_ORBIS

class CSavegameQueuedOperation_PS4DamagedCheck : public CSavegameQueuedOperation
{
public:
	CSavegameQueuedOperation_PS4DamagedCheck()
		: CSavegameQueuedOperation(SAVE_QUEUED_PS4_DAMAGED_CHECK)
	{
	}

	virtual ~CSavegameQueuedOperation_PS4DamagedCheck() {}

	void Init(s32 slotIndex);

	//	Return true to Pop() from queue
	virtual bool Update();
	virtual bool Shutdown();

#if __BANK
	virtual const char *GetNameOfOperation() { return "PS4DamagedCheck"; }
#endif	//	__BANK

private:
	// Input
	s32 m_slotIndex;
};

#endif	//	RSG_ORBIS


#if __LOAD_LOCAL_PHOTOS
class CSavegameQueuedOperation_LoadLocalPhoto : public CSavegameQueuedOperation
{
	enum eLocalPhotoLoadProgress
	{
		LOCAL_PHOTO_LOAD_PROGRESS_BEGIN_LOAD,
		LOCAL_PHOTO_LOAD_PROGRESS_CHECK_LOAD,
		LOCAL_PHOTO_LOAD_PROGRESS_REQUEST_TEXTURE_DECODE,
		LOCAL_PHOTO_LOAD_PROGRESS_WAIT_FOR_TEXTURE_DECODE,
		LOCAL_PHOTO_LOAD_PROGRESS_READ_METADATA,
		LOCAL_PHOTO_LOAD_PROGRESS_WAIT_FOR_LOADING_MESSAGE_TO_FINISH,
		LOCAL_PHOTO_LOAD_PROGRESS_HANDLE_ERRORS
	};

public:

	CSavegameQueuedOperation_LoadLocalPhoto()
		: CSavegameQueuedOperation(SAVE_QUEUED_LOAD_LOCAL_PHOTO)
	{
	}

	virtual ~CSavegameQueuedOperation_LoadLocalPhoto() {}

	void Init(const CSavegamePhotoUniqueId &uniquePhotoId, CPhotoBuffer *pPhotoBuffer);

	//    Return true to Pop() from queue
	virtual bool Update();
	virtual bool Shutdown(){return false;}

#if __BANK
	virtual const char *GetNameOfOperation() { return "LoadLocalPhoto"; }
#endif	//	__BANK

private:

	//	Private functions
	void EndLoadLocalPhoto();

	//	Private data
	eLocalPhotoLoadProgress m_LocalPhotoLoadProgress;
	CSavegamePhotoUniqueId m_UniquePhotoId;
	CPhotoBuffer *m_pBufferToStoreLoadedPhoto;

	u32 m_SizeOfJpegBuffer;
	TextureDownloadRequestHandle m_TextureDownloadRequestHandle;
};
#endif	//	__LOAD_LOCAL_PHOTOS

#if !__LOAD_LOCAL_PHOTOS
class CSavegameQueuedOperation_LoadUGCPhoto : public CSavegameQueuedOperation
{
	enum eUGCPhotoLoadProgress
	{
		UGC_PHOTO_LOAD_PROGRESS_BEGIN_PLAYER_IS_SIGNED_IN,
		UGC_PHOTO_LOAD_PROGRESS_CHECK_PLAYER_IS_SIGNED_IN,
		UGC_PHOTO_LOAD_PROGRESS_REQUEST_TEXTURE_DOWNLOAD,
		UGC_PHOTO_LOAD_PROGRESS_WAIT_FOR_TEXTURE_DOWNLOAD,
		UGC_PHOTO_LOAD_PROGRESS_WAIT_FOR_LOADING_MESSAGE_TO_FINISH,
		UGC_PHOTO_LOAD_PROGRESS_HANDLE_ERRORS
	};

public:
//	void InitCore();
//	void ShutdownCore();

	CSavegameQueuedOperation_LoadUGCPhoto()
		: CSavegameQueuedOperation(SAVE_QUEUED_LOAD_UGC_PHOTO), m_IndexWithinMergedPhotoList(-1, false), m_IndexOfPhotoFile(-1)	//	, m_pMetadataString(NULL)
	{
	}

	virtual ~CSavegameQueuedOperation_LoadUGCPhoto() {}

	void Init(const CIndexWithinMergedPhotoList &indexWithinMergedPhotoList, CIndexOfPhotoOnCloud indexOfPhotoFile);

	//    Return true to Pop() from queue
	virtual bool Update();
	virtual bool Shutdown(){return false;}

#if __BANK
	virtual const char *GetNameOfOperation() { return "LoadUGCPhoto"; }
#endif	//	__BANK

private:

	//	Private functions
	void EndLoadBlockCloud();

	//	Private data
	eUGCPhotoLoadProgress m_UGCPhotoLoadProgress;
	CIndexWithinMergedPhotoList m_IndexWithinMergedPhotoList;
	CIndexOfPhotoOnCloud m_IndexOfPhotoFile;

//	char *m_pMetadataString;

//	s32 m_TxdSlotForRequest;

	u32 m_SizeOfJpegBuffer;
//	u32 m_SizeOfJsonBuffer;
	TextureDownloadRequestHandle m_TextureDownloadRequestHandle;

//	bool m_bWaitingForTextureDownload;
//	bool m_bTextureDownloadSucceeded;

//	bool m_bWaitingForMetadataDownload;
//	bool m_bMetadataDownloadSucceeded;
};
#endif	//	!__LOAD_LOCAL_PHOTOS


class CSavegameQueuedOperation_PhotoSave : public CSavegameQueuedOperation
{
	enum ePhotoSaveProgress
	{
		PHOTO_SAVE_PROGRESS_CREATE_METADATA_STRING,
//		PHOTO_SAVE_PROGRESS_BEGIN_CLOUD_SAVE,
//		PHOTO_SAVE_PROGRESS_CHECK_CLOUD_SAVE,

		PHOTO_SAVE_PROGRESS_BEGIN_CREATE_UGC,
		PHOTO_SAVE_PROGRESS_CHECK_CREATE_UGC,

		PHOTO_SAVE_PROGRESS_DISPLAY_UGC_ERROR
	};
public:
	CSavegameQueuedOperation_PhotoSave()
		: CSavegameQueuedOperation(SAVE_QUEUED_PHOTO_SAVE)
	{
	}

	virtual ~CSavegameQueuedOperation_PhotoSave() {}

	void Init(u8 *pJpegBuffer, u32 exactSizeOfJpegBuffer, CSavegamePhotoMetadata const &metadataToSave, const char *pTitle, const char *pDescription);

	//    Return true to Pop() from queue
	virtual bool Update();
	virtual bool Shutdown();

#if __BANK
	virtual const char *GetNameOfOperation() { return "PhotoSave"; }
#endif	//	__BANK

private:
	ePhotoSaveProgress m_PhotoSaveProgress;
	u8 *m_pJpegBuffer;
	u32 m_ExactSizeOfJpegBuffer;

	CSavegamePhotoMetadata m_MetadataToSave;

	char *m_pMetadataStringForUploadingToUGC;

	atString m_DisplayName;	//	[CSavegamePhotoMetadata::ms_MAX_TITLE_CHARS];
	atString m_Description;	//	[CSavegamePhotoMetadata::ms_MAX_DESCRIPTION_CHARS];
};


class CSavegameQueuedOperation_SavePhotoForMissionCreator : public CSavegameQueuedOperation
{
	enum eMissionCreatorPhotoSaveProgress
	{
		MISSION_CREATOR_PHOTO_SAVE_PROGRESS_BEGIN_CLOUD_SAVE,
		MISSION_CREATOR_PHOTO_SAVE_PROGRESS_CHECK_CLOUD_SAVE
	};
public:
	CSavegameQueuedOperation_SavePhotoForMissionCreator()
		: CSavegameQueuedOperation(SAVE_QUEUED_SAVE_PHOTO_FOR_MISSION_CREATOR)
	{
	}

	virtual ~CSavegameQueuedOperation_SavePhotoForMissionCreator() {}

	void Init(const char *pFilename, CPhotoBuffer *pPhotoBuffer);

	//    Return true to Pop() from queue
	virtual bool Update();
	virtual bool Shutdown(){return false;}

#if __BANK
	virtual const char *GetNameOfOperation() { return "SavePhotoForMissionCreator"; }
#endif	//	__BANK

private:
	eMissionCreatorPhotoSaveProgress m_PhotoSaveProgress;
	CPhotoBuffer *m_pPhotoBuffer;

	static const s32 MAX_LENGTH_OF_PATH_TO_MISSION_CREATOR_PHOTO = 128;
	char m_Filename[MAX_LENGTH_OF_PATH_TO_MISSION_CREATOR_PHOTO];
};


class CSavegameQueuedOperation_LoadPhotoForMissionCreator : public CSavegameQueuedOperation
{
	enum eMissionCreatorPhotoLoadProgress
	{
		MISSION_CREATOR_PHOTO_LOAD_PROGRESS_BEGIN_CLOUD_LOAD,
		MISSION_CREATOR_PHOTO_LOAD_PROGRESS_CHECK_CLOUD_LOAD
	};
public:
	CSavegameQueuedOperation_LoadPhotoForMissionCreator()
		: CSavegameQueuedOperation(SAVE_QUEUED_LOAD_PHOTO_FOR_MISSION_CREATOR)
	{
	}

	virtual ~CSavegameQueuedOperation_LoadPhotoForMissionCreator() {}

	void Init(const sRequestData &requestData, CPhotoBuffer *pPhotoBuffer, u32 maxSizeOfBuffer);

	//    Return true to Pop() from queue
	virtual bool Update();
	virtual bool Shutdown(){return false;}

#if __BANK
	virtual const char *GetNameOfOperation() { return "LoadPhotoForMissionCreator"; }
#endif	//	__BANK

private:
	eMissionCreatorPhotoLoadProgress m_PhotoLoadProgress;
	CPhotoBuffer *m_pPhotoBuffer;
	sRequestData m_RequestData; 

	u32 m_MaxSizeOfBuffer;
};


class CSavegameQueuedOperation_MPStats_Load : public CSavegameQueuedOperation
{
	enum eMPStatsLoadProgress
	{
		MP_STATS_LOAD_PROGRESS_BEGIN_CLOUD_LOAD,
#if __ALLOW_LOCAL_MP_STATS_SAVES
		MP_STATS_LOAD_PROGRESS_BEGIN_CHECK_LOCAL_FILE_EXISTS,
		MP_STATS_LOAD_PROGRESS_CHECK_LOCAL_FILE_EXISTS,
		MP_STATS_LOAD_PROGRESS_BEGIN_LOCAL_LOAD,
		MP_STATS_LOAD_PROGRESS_CHECK_LOCAL_LOAD,
#endif	//	__ALLOW_LOCAL_MP_STATS_SAVES
		MP_STATS_LOAD_PROGRESS_CHECK_CLOUD_LOAD,
		MP_STATS_LOAD_PROGRESS_DESERIALIZE_FROM_MOST_RECENT_FILE,
		MP_STATS_LOAD_PROGRESS_FREE_BOTH_BUFFERS,
		MP_STATS_LOAD_PROGRESS_CLOUD_FINISH
	};

public:
	CSavegameQueuedOperation_MPStats_Load()
		: CSavegameQueuedOperation(SAVE_QUEUED_MP_STATS_LOAD) { }

	virtual ~CSavegameQueuedOperation_MPStats_Load() { }

	void Init(const u32 saveCategory);

	// Return true to Pop() from queue
	virtual bool Update();
	virtual bool Shutdown();
	void Cancel();

#if __BANK
	virtual const char *GetNameOfOperation() { return "MPStats_Load"; }
#endif	//	__BANK

private:

// Private functions
#if __ALLOW_LOCAL_MP_STATS_SAVES
	bool ReadPosixTimeFromCloudFile(u64 &timeStamp);
	void ReadPosixTimeFromLocalFile(u64 &timeStamp);
	void ReadFullDataFromLocalFile();
#endif	//	__ALLOW_LOCAL_MP_STATS_SAVES

	void ReadFullDataFromCloudFile();


// Private data
	s32 m_IndexOfMpSave;

	//Current Operation Progress.
	eMPStatsLoadProgress m_OpProgress;

	//Type of savegame
	eTypeOfSavegame  m_SavegameType;

	//Cloud related operations.
	CSaveGameCloudData  m_Cloud;

#if __ALLOW_LOCAL_MP_STATS_SAVES
	s32 m_IndexOfSavegameFile;

	bool m_bLocalFileExists;
	bool m_bLocalFileIsDamaged;

	bool m_bLocalFileHasLoaded;
	bool m_bFailedToReadFromLocalFile;
#endif	//	__ALLOW_LOCAL_MP_STATS_SAVES

	bool m_bCloudFileHasLoaded;
	bool m_bFailedToReadFromCloudFile;
};


class CSavegameQueuedOperation_MPStats_Save : public CSavegameQueuedOperation
{
	enum eMPStatsSaveProgress
	{
#if __ALLOW_LOCAL_MP_STATS_SAVES
		MP_STATS_SAVE_PROGRESS_CREATE_LOCAL_DATA_TO_BE_SAVED,
		MP_STATS_SAVE_PROGRESS_BEGIN_LOCAL_SAVE,
		MP_STATS_SAVE_PROGRESS_LOCAL_SAVE_UPDATE,
		MP_STATS_SAVE_PROGRESS_FREE_LOCAL_SAVE_BUFFER,
#endif	//	__ALLOW_LOCAL_MP_STATS_SAVES
		MP_STATS_SAVE_PROGRESS_CREATE_CLOUD_DATA_TO_BE_SAVED,
		MP_STATS_SAVE_PROGRESS_ALLOCATE_CLOUD_SAVE_BUFFER,
		MP_STATS_SAVE_PROGRESS_BEGIN_FILL_CLOUD_SAVE_BUFFER,
		MP_STATS_SAVE_PROGRESS_FILL_CLOUD_SAVE_BUFFER,
		MP_STATS_SAVE_PROGRESS_BEGIN_CLOUD_SAVE,
		MP_STATS_SAVE_PROGRESS_CLOUD_SAVE_UPDATE,
		MP_STATS_SAVE_PROGRESS_FREE_CLOUD_SAVE_BUFFER,
		MP_STATS_SAVE_PROGRESS_FINISH
	};

public:

#if __ALLOW_LOCAL_MP_STATS_SAVES
	enum eMPStatsSaveDestination
	{
		MP_STATS_SAVE_TO_CLOUD,
		MP_STATS_SAVE_TO_CONSOLE,
		MP_STATS_SAVE_TO_CLOUD_AND_CONSOLE
	};
#endif	//	__ALLOW_LOCAL_MP_STATS_SAVES

	CSavegameQueuedOperation_MPStats_Save()
		: CSavegameQueuedOperation(SAVE_QUEUED_MP_STATS_SAVE) { }

	virtual ~CSavegameQueuedOperation_MPStats_Save() { }

	void Init(const u32 saveCategory
#if __ALLOW_LOCAL_MP_STATS_SAVES
		, eMPStatsSaveDestination saveDestination
#endif	//	__ALLOW_LOCAL_MP_STATS_SAVES
		);

	// Return true to Pop() from queue
	virtual bool Update();
	virtual bool Shutdown();
	void Cancel();

#if __BANK
	virtual const char *GetNameOfOperation() { return "MPStats_Save"; }
#endif	//	__BANK

private:
	//Slot index for the file save.
	s32  m_SlotIndex;

	//Current Operation Progress.
	eMPStatsSaveProgress m_OpProgress;

	//Type of savegame
	eTypeOfSavegame  m_SavegameType;

#if __ALLOW_LOCAL_MP_STATS_SAVES
	bool m_bSaveToCloud;
	bool m_bSaveToConsole;

	bool m_bLocalSaveSucceeded;
#endif	//	__ALLOW_LOCAL_MP_STATS_SAVES

	bool m_bCloudSaveSucceeded;

	//Cloud related operations.
	CSaveGameCloudData  m_Cloud;
};

class CSavegameQueuedOperation_ManualLoad : public CSavegameQueuedOperation
{
	enum eManualLoadProgress
	{
		MANUAL_LOAD_PROGRESS_BEGIN_SAVEGAME_LIST,
		MANUAL_LOAD_PROGRESS_CHECK_SAVEGAME_LIST,
		MANUAL_LOAD_PROGRESS_BEGIN_LOAD,
		MANUAL_LOAD_PROGRESS_CHECK_LOAD,
		MANUAL_LOAD_PROGRESS_BEGIN_DELETE,
		MANUAL_LOAD_PROGRESS_CHECK_DELETE,
		MANUAL_LOAD_PROGRESS_DISPLAY_DELETE_SUCCESS_MESSAGE
#if __ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME
		,MANUAL_LOAD_PROGRESS_BEGIN_LOAD_FOR_CLOUD_UPLOAD,
		MANUAL_LOAD_PROGRESS_CHECK_LOAD_FOR_CLOUD_UPLOAD,
		MANUAL_LOAD_PROGRESS_BEGIN_CLOUD_UPLOAD,
		MANUAL_LOAD_PROGRESS_CHECK_CLOUD_UPLOAD,
		MANUAL_LOAD_PROGRESS_DISPLAY_UPLOAD_SUCCESS_MESSAGE
#endif	//	__ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME
	};
public:
	CSavegameQueuedOperation_ManualLoad()
		: CSavegameQueuedOperation(SAVE_QUEUED_MANUAL_LOAD)
	{
	}

	virtual ~CSavegameQueuedOperation_ManualLoad() {}

	void Init();

	//	Return true to Pop() from queue
	virtual bool Update();
	virtual bool Shutdown(){return false;}

#if __BANK
	virtual const char *GetNameOfOperation() { return "ManualLoad"; }
#endif	//	__BANK

	void SetQuitAsSoonAsNoSavegameOperationsAreRunning() { m_bQuitAsSoonAsNoSavegameOperationsAreRunning = true; }

private:
	char m_FilenameOfFileToDelete[SAVE_GAME_MAX_FILENAME_LENGTH_OF_LOCAL_FILE];
	bool m_bQuitAsSoonAsNoSavegameOperationsAreRunning;

	eManualLoadProgress m_ManualLoadProgress;
#if RSG_ORBIS
	bool deleteBackupSave;
#endif

#if __ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME
	static const u32 OVERHEAD_FOR_JSON_CHARACTERS = 64;	//	Extra characters for field names, quotes, colons and any other extra characters that a properly-formatted json file needs
	static const u32 LENGTH_OF_JSON_STRING = SAVE_GAME_MAX_DISPLAY_NAME_LENGTH + OVERHEAD_FOR_JSON_CHARACTERS;

	char m_JsonString[LENGTH_OF_JSON_STRING];
#endif	//	__ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME

private:

#if __ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME
	bool FillJsonString(s32 savegameSlotNumber);
#endif	//	__ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME
};


#define __MANUAL_SAVE_CLOSE_PAUSE_MENU_AS_SOON_AS_POSSIBLE	(0)

class CSavegameQueuedOperation_ManualSave : public CSavegameQueuedOperation
{
	enum eManualSaveProgress
	{
		MANUAL_SAVE_PROGRESS_CREATE_SAVE_BUFFER,
		MANUAL_SAVE_PROGRESS_BEGIN_SAVEGAME_LIST,
		MANUAL_SAVE_PROGRESS_CHECK_SAVEGAME_LIST,
#if __MANUAL_SAVE_CLOSE_PAUSE_MENU_AS_SOON_AS_POSSIBLE
		MANUAL_SAVE_PROGRESS_WAIT_UNTIL_PAUSE_MENU_IS_CLOSED,
		MANUAL_SAVE_PROGRESS_WAIT_UNTIL_SPINNER_CAN_BE_DISPLAYED,
#endif	//	__MANUAL_SAVE_CLOSE_PAUSE_MENU_AS_SOON_AS_POSSIBLE
		MANUAL_SAVE_PROGRESS_CHECK_SAVE,
		MANUAL_SAVE_PROGRESS_BEGIN_DELETE,
		MANUAL_SAVE_PROGRESS_CHECK_DELETE,
		MANUAL_SAVE_PROGRESS_DISPLAY_DELETE_SUCCESS_MESSAGE
	};
public:
	CSavegameQueuedOperation_ManualSave()
		: CSavegameQueuedOperation(SAVE_QUEUED_MANUAL_SAVE)
	{
	}

	virtual ~CSavegameQueuedOperation_ManualSave() {}

	void Init();

	//	Return true to Pop() from queue
	virtual bool Update();
	virtual bool Shutdown();

#if __BANK
	virtual const char *GetNameOfOperation() { return "ManualSave"; }
#endif	//	__BANK

	void SetQuitAsSoonAsNoSavegameOperationsAreRunning() { m_bQuitAsSoonAsNoSavegameOperationsAreRunning = true; }

private:
	char m_FilenameOfFileToDelete[SAVE_GAME_MAX_FILENAME_LENGTH_OF_LOCAL_FILE];

#if __MANUAL_SAVE_CLOSE_PAUSE_MENU_AS_SOON_AS_POSSIBLE
	sysTimer m_SafetyTimeOut;
#endif	//	__MANUAL_SAVE_CLOSE_PAUSE_MENU_AS_SOON_AS_POSSIBLE

	bool m_bQuitAsSoonAsNoSavegameOperationsAreRunning;

	eManualSaveProgress m_ManualSaveProgress;
#if RSG_ORBIS
	bool deleteBackupSave;
#endif
};


class CSavegameQueuedOperation_Autosave : public CSavegameQueuedOperation
{
	enum eAutosaveProgress
	{
		AUTOSAVE_PROGRESS_CREATE_SAVE_BUFFER,
		AUTOSAVE_PROGRESS_CHECK_SAVE
	};
public:
	CSavegameQueuedOperation_Autosave()
		: CSavegameQueuedOperation(SAVE_QUEUED_AUTOSAVE)
	{
	}

	virtual ~CSavegameQueuedOperation_Autosave() {}

	void Init();

	//	Return true to Pop() from queue
	virtual bool Update();
	virtual bool Shutdown();

	virtual bool IsAutosave() { return true; }

#if __BANK
	virtual const char *GetNameOfOperation() { return "Autosave"; }
#endif	//	__BANK

private:
	eAutosaveProgress m_AutosaveProgress;
};


#if GTA_REPLAY

#if USE_SAVE_SYSTEM
class CSavegameQueuedOperation_ReplayLoad : public CSavegameQueuedOperation
{
 	enum eReplayLoadProgress
 	{
 		REPLAY_LOAD_PROGRESS_BEGIN_LOAD,
 		REPLAY_LOAD_PROGRESS_CHECK_LOAD
 	};
public:
	CSavegameQueuedOperation_ReplayLoad()
		: CSavegameQueuedOperation(SAVE_QUEUED_REPLAY_LOAD)
	{
	}

	virtual ~CSavegameQueuedOperation_ReplayLoad() {}

	void Init();

	//	Return true to Pop() from queue
	virtual bool Update();
	virtual bool Shutdown(){return false;}

#if __BANK
	virtual const char* GetNameOfOperation()	{	return "ReplayLoad";	}
#endif	//	__BANK

private:
	eReplayLoadProgress m_ManualLoadProgress;
};


class CSavegameQueuedOperation_ReplaySave : public CSavegameQueuedOperation
{
	enum eReplaySaveProgress
	{
		REPLAY_SAVE_PROGRESS_CREATE_SAVE_BUFFER,
		REPLAY_SAVE_PROGRESS_CHECK_SAVE
	};
public:
	CSavegameQueuedOperation_ReplaySave()
		: CSavegameQueuedOperation(SAVE_QUEUED_REPLAY_SAVE)
	{
	}

	virtual ~CSavegameQueuedOperation_ReplaySave() {}

	void Init();

	//	Return true to Pop() from queue
	virtual bool Update();
	virtual bool Shutdown(){return false;}

#if __BANK
	virtual const char* GetNameOfOperation()	{	return "ReplaySave";	}
#endif	//	__BANK

private:
	eReplaySaveProgress m_AutosaveProgress;
};
#endif	//	USE_SAVE_SYSTEM

class CSavegameQueuedOperation_EnumerateReplayFiles : public CSavegameQueuedOperation
{
	enum eReplayOpProgress
	{
		REPLAY_OP_PROGRESS_BEGIN_ENUMERATE,
		REPLAY_OP_PROGRESS_CHECK_ENUMERATE,
	};
public:
	CSavegameQueuedOperation_EnumerateReplayFiles()
		: CSavegameQueuedOperation(SAVE_QUEUED_ENUMERATE_REPLAY_FILES)
	{
	}

	virtual ~CSavegameQueuedOperation_EnumerateReplayFiles() {}

	virtual void Init(eReplayEnumerationTypes enumType, FileDataStorage* dataStorage = NULL, const char* filter = "");

	//    Return true to Pop() from queue
	virtual bool Update();
	virtual bool Shutdown(){return false;}

#if __BANK
	virtual const char *GetNameOfOperation() { return "EnumerateReplayFiles"; }
#endif	//	__BANK

private:
	eReplayOpProgress m_Progress;
	eReplayEnumerationTypes m_Enumtype;
	char m_Filter[REPLAYFILELENGTH];
	FileDataStorage *m_FileList;
};

class CSavegameQueuedOperation_ReplayUpdateFavourites : public CSavegameQueuedOperation
{
	enum eReplayOpProgress
	{
		REPLAY_OP_PROGRESS_BEGIN_UPDATE_FAVOURITES,
		REPLAY_OP_PROGRESS_CHECK_UPDATE_FAVOURITES,
	};
public:
	CSavegameQueuedOperation_ReplayUpdateFavourites()
		: CSavegameQueuedOperation(SAVE_QUEUED_REPLAY_UPDATE_FAVOURITES)
	{
	}

	virtual ~CSavegameQueuedOperation_ReplayUpdateFavourites() {}

	virtual void Init();

	//    Return true to Pop() from queue
	virtual bool Update();
	virtual bool Shutdown(){return false;}

#if __BANK
	virtual const char *GetNameOfOperation() { return "ReplayUpdateFavourites"; }
#endif	//	__BANK

private:
	eReplayOpProgress m_Progress;
};

class CSavegameQueuedOperation_ReplayMultiDelete : public CSavegameQueuedOperation
{
	enum eReplayOpProgress
	{
		REPLAY_OP_PROGRESS_BEGIN_REPLAY_MULTI_DELETE,
		REPLAY_OP_PROGRESS_CHECK_REPLAY_MULTI_DELETE,
	};
public:
	CSavegameQueuedOperation_ReplayMultiDelete()
		: CSavegameQueuedOperation(SAVE_QUEUED_REPLAY_MULTI_DELETE)
	{
	}

	virtual ~CSavegameQueuedOperation_ReplayMultiDelete() {}

	virtual void Init(const char* filter = "");

	//    Return true to Pop() from queue
	virtual bool Update();
	virtual bool Shutdown(){return false;}

#if __BANK
	virtual const char *GetNameOfOperation() { return "ReplayMultiDelete"; }
#endif	//	__BANK

private:
	eReplayOpProgress m_Progress;
	char m_Filter[REPLAYFILELENGTH];
};

class CSavegameQueuedOperation_DeleteReplayClips : public CSavegameQueuedOperation
{
	enum eReplayOpProgress
	{
		REPLAY_OP_PROGRESS_BEGIN_DELETE_CLIP,
		REPLAY_OP_PROGRESS_CHECK_DELETE_CLIP,
		REPLAY_OP_PROGRESS_BEGIN_DELETE_JPG,
		REPLAY_OP_PROGRESS_CHECK_DELETE_JPG,
		REPLAY_OP_PROGRESS_BEGIN_DELETE_LOGFILE,
		REPLAY_OP_PROGRESS_CHECK_DELETE_LOGFILE,
#if DO_REPLAY_OUTPUT_XML
		REPLAY_OP_PROGRESS_BEGIN_DELETE_XML,
		REPLAY_OP_PROGRESS_CHECK_DELETE_XML,
#endif
	};
public:
	CSavegameQueuedOperation_DeleteReplayClips()
		: CSavegameQueuedOperation(SAVE_QUEUED_DELETE_REPLAY_CLIPS)
	{
	}

	virtual ~CSavegameQueuedOperation_DeleteReplayClips() {}

	virtual void Init(const char* filePath);

	//    Return true to Pop() from queue
	virtual bool Update();
	virtual bool Shutdown(){return false;}

#if __BANK
	virtual const char *GetNameOfOperation() { return "DeleteReplayClip"; }
#endif	//	__BANK

private:
	eReplayOpProgress	m_Progress;
	char m_Path[REPLAYPATHLENGTH];
};

class CSavegameQueuedOperation_DeleteReplayFile : public CSavegameQueuedOperation
{
	enum eReplayOpProgress
	{
		REPLAY_OP_PROGRESS_BEGIN_DELETE_FILE,
		REPLAY_OP_PROGRESS_CHECK_DELETE_FILE,
	};
public:
	CSavegameQueuedOperation_DeleteReplayFile()
		: CSavegameQueuedOperation(SAVE_QUEUED_DELETE_REPLAY_FILE)
	{
	}

	virtual ~CSavegameQueuedOperation_DeleteReplayFile() {}

	virtual void Init(const char* filePath);

	//    Return true to Pop() from queue
	virtual bool Update();
	virtual bool Shutdown(){return false;}

#if __BANK
	virtual const char *GetNameOfOperation() { return "DeleteReplayFile"; }
#endif	//	__BANK

private:
	eReplayOpProgress	m_Progress;
	char m_Path[REPLAYPATHLENGTH];
};

class CSavegameQueuedOperation_LoadReplayClip : public CSavegameQueuedOperation
{
	enum eReplayOpProgress
	{
		REPLAY_OP_PROGRESS_BEGIN_LOAD,
		REPLAY_OP_PROGRESS_CHECK_LOAD
	};
public:
	CSavegameQueuedOperation_LoadReplayClip()
		: CSavegameQueuedOperation(SAVE_QUEUED_LOAD_REPLAY_FILE)
	{
	}

	virtual ~CSavegameQueuedOperation_LoadReplayClip() {}

	virtual void Init(const char* filePath, CBufferInfo& bufferInfo, tLoadStartFunc loadStartFunc = NULL);

	//    Return true to Pop() from queue
	virtual bool Update();
	virtual bool Shutdown(){return false;}

#if __BANK
	virtual const char *GetNameOfOperation() { return "LoadReplayClip"; }
#endif	//	__BANK

private:
	eReplayOpProgress m_Progress;
	char m_Path[REPLAYPATHLENGTH];
	CBufferInfo* m_BufferInfo;
	tLoadStartFunc m_LoadStartFunc;
};

class CSavegameQueuedOperation_LoadMontage : public CSavegameQueuedOperation
{
	enum eReplayOpProgress
	{
		REPLAY_OP_PROGRESS_BEGIN_LOAD,
		REPLAY_OP_PROGRESS_CHECK_LOAD
	};
public:
	CSavegameQueuedOperation_LoadMontage()
		: CSavegameQueuedOperation(SAVE_QUEUED_LOAD_REPLAY_MONTAGE)
	{
	}

	virtual ~CSavegameQueuedOperation_LoadMontage() {}

	virtual void Init(const char* filePath, CMontage *montage);

	//    Return true to Pop() from queue
	virtual bool Update();
	virtual bool Shutdown(){return false;}

#if __BANK
	virtual const char *GetNameOfOperation() { return "LoadReplayMontage"; }
#endif	//	__BANK

private:
	eReplayOpProgress m_Progress;
	char m_Path[REPLAYPATHLENGTH];
	CMontage* m_Montage;
};

class CSavegameQueuedOperation_SaveMontage : public CSavegameQueuedOperation
{
	enum eReplayOpProgress
	{
		REPLAY_OP_PROGRESS_BEGIN_SAVE,
		REPLAY_OP_PROGRESS_CHECK_SAVE
	};
public:
	CSavegameQueuedOperation_SaveMontage()
		: CSavegameQueuedOperation(SAVE_QUEUED_SAVE_REPLAY_MONTAGE)
	{
	}

	virtual ~CSavegameQueuedOperation_SaveMontage() {}

	virtual void Init(const char* filePath, CMontage *montage);

	//    Return true to Pop() from queue
	virtual bool Update();
	virtual bool Shutdown(){return false;}

#if __BANK
	virtual const char *GetNameOfOperation() { return "SaveReplayMontage"; }
#endif	//	__BANK

private:
	eReplayOpProgress m_Progress;
	char m_Path[REPLAYPATHLENGTH];
	CMontage* m_Montage;
};


class CSavegameQueuedOperation_LoadReplayHeader : public CSavegameQueuedOperation
{
	enum eReplayOpProgress
	{
		REPLAY_OP_PROGRESS_BEGIN_LOAD,
		REPLAY_OP_PROGRESS_CHECK_LOAD
	};
public:
	CSavegameQueuedOperation_LoadReplayHeader()
		: CSavegameQueuedOperation(SAVE_QUEUED_LOAD_REPLAY_HEADER)
	{
	}

	virtual ~CSavegameQueuedOperation_LoadReplayHeader() {}

	virtual void Init(const char* filepath);

	//    Return true to Pop() from queue
	virtual bool Update();
	virtual bool Shutdown(){return false;}

#if __BANK
	virtual const char *GetNameOfOperation() { return "LoadReplayHeader"; }
#endif	//	__BANK

	float GetClipLength() { return m_ClipLength; }
	const ReplayHeader* GetHeader() { return m_Header; }

private:
	eReplayOpProgress	m_Progress;
	char				m_Path[REPLAYPATHLENGTH];
	const ReplayHeader*	m_Header;
	float				m_ClipLength;
};

#if RSG_ORBIS

class CSavegameQueuedOperation_TransferReplayFile : public CSavegameQueuedOperation
{
	enum eReplayOpProgress
	{
		REPLAY_OP_PROGRESS_BEGIN_TRANSFER,
		REPLAY_OP_PROGRESS_CHECK_TRANSFER
	};
public:
	CSavegameQueuedOperation_TransferReplayFile()
		: CSavegameQueuedOperation(SAVE_QUEUED_SAVE_REPLAY_FILE)
	{
	}

	virtual ~CSavegameQueuedOperation_TransferReplayFile() {}

	void Init(const char* fileName, void* pSrc, u32 bufferSize, bool saveIcon, bool overwrite);

	//    Return true to Pop() from queue
	virtual bool Update();
	virtual bool Shutdown(){return false;}

#if __BANK
	virtual const char *GetNameOfOperation() { return "TransferReplayFile"; }
#endif	//	__BANK

private:
	s32 m_localIndex;
	eReplayOpProgress m_Progress;
	void* m_Buffer;
	u32 m_BufferSize;
	char m_FileName[REPLAYPATHLENGTH];
	char16 m_DisplayName[REPLAYPATHLENGTH];
	bool m_OverWrite;
};

#endif // RSG_ORBIS

#endif // GTA_REPLAY


class CSavegameQueuedOperation_MissionRepeatLoad : public CSavegameQueuedOperation
{
	enum eMissionRepeatLoadProgress
	{
		MISSION_REPEAT_LOAD_PROGRESS_PRE_LOAD,
		MISSION_REPEAT_LOAD_PROGRESS_BEGIN_LOAD,
		MISSION_REPEAT_LOAD_PROGRESS_CHECK_LOAD
	};

public:
	CSavegameQueuedOperation_MissionRepeatLoad()
		: CSavegameQueuedOperation(SAVE_QUEUED_MISSION_REPEAT_LOAD)
	{
	}

	virtual ~CSavegameQueuedOperation_MissionRepeatLoad() {}

	void Init();

	//	Return true to Pop() from queue
	virtual bool Update();
	virtual bool Shutdown(){return false;}

#if __BANK
	virtual const char* GetNameOfOperation()	{	return "MissionRepeatLoad";	}
#endif	//	__BANK

private:
	eMissionRepeatLoadProgress m_MissionRepeatLoadProgress;
};


class CSavegameQueuedOperation_MissionRepeatSave : public CSavegameQueuedOperation
{
	enum eMissionRepeatSaveProgress
	{
		MISSION_REPEAT_SAVE_PROGRESS_DISPLAY_WARNING_SCREEN,
		MISSION_REPEAT_SAVE_PROGRESS_CREATE_SAVE_BUFFER,
		MISSION_REPEAT_SAVE_PROGRESS_CHECK_SAVE
	};
public:
	CSavegameQueuedOperation_MissionRepeatSave()
		: CSavegameQueuedOperation(SAVE_QUEUED_MISSION_REPEAT_SAVE)
	{
	}

	virtual ~CSavegameQueuedOperation_MissionRepeatSave() {}

	void Init(bool bBenchmarkTest);

	//	Return true to Pop() from queue
	virtual bool Update();
	virtual bool Shutdown(){return false;}

#if __BANK
	virtual const char* GetNameOfOperation()	{	return "MissionRepeatSave";	}
#endif	//	__BANK

private:
	eMissionRepeatSaveProgress m_MissionRepeatSaveProgress;
	bool m_bBenchmarkTest;		//	Controls whether warning messages should mention PC Benchmark Tests or Mission Replays
};


class CSavegameQueuedOperation_CheckFileExists : public CSavegameQueuedOperation
{
	enum eCheckFileExistsProgress
	{
		CHECK_FILE_EXISTS_PROGRESS_BEGIN,
		CHECK_FILE_EXISTS_PROGRESS_CHECK
	};
public:
	CSavegameQueuedOperation_CheckFileExists()
		: CSavegameQueuedOperation(SAVE_QUEUED_CHECK_FILE_EXISTS)
	{
	}

	virtual ~CSavegameQueuedOperation_CheckFileExists() {}

	void Init(s32 SlotIndex);

	//    Return true to Pop() from queue
	virtual bool Update();
	virtual bool Shutdown(){return false;}

#if __BANK
	virtual const char *GetNameOfOperation() { return "CheckFileExists"; }
#endif	//	__BANK

	void GetFileExists(bool &bFileExists, bool &bFileIsDamaged) { bFileExists = m_bFileExists; bFileIsDamaged = m_bFileIsDamaged; }

private:
	// Input
	s32 m_SlotIndex;

	// Results
	bool m_bFileExists;
	bool m_bFileIsDamaged;

	eCheckFileExistsProgress m_CheckFileExistsProgress;
};


class CSavegameQueuedOperation_CreateSortedListOfSavegames : public CSavegameQueuedOperation
{
	enum eCreateSortedListOfSavegamesProgress
	{
		CREATE_SORTED_LIST_OF_SAVEGAMES_BEGIN,
		CREATE_SORTED_LIST_OF_SAVEGAMES_CHECK,
		CREATE_SORTED_LIST_OF_SAVEGAMES_HANDLE_ERRORS
	};

public:
	CSavegameQueuedOperation_CreateSortedListOfSavegames()
		: CSavegameQueuedOperation(SAVE_QUEUED_CREATE_SORTED_LIST_OF_SAVEGAMES)
	{
	}

	virtual ~CSavegameQueuedOperation_CreateSortedListOfSavegames() {}

	void Init(bool bDisplayWarningScreens);

	//    Return true to Pop() from queue
	virtual bool Update();
	virtual bool Shutdown(){return false;}

#if __BANK
	virtual const char *GetNameOfOperation() { return "CreateSortedListOfSavegames"; }
#endif	//	__BANK

private:

	//	Private functions
	void EndCreatedSortedList();

	//	Private data
	eCreateSortedListOfSavegamesProgress m_CreateSortedListOfSavegamesProgress;
	bool m_bDisplayWarningScreens;
};



#if __LOAD_LOCAL_PHOTOS
class CSavegameQueuedOperation_CreateSortedListOfLocalPhotos : public CSavegameQueuedOperation
{
	enum eCreateSortedListOfLocalPhotosProgress
	{
		CREATE_SORTED_LIST_OF_LOCAL_PHOTOS_BEGIN_CLOUD_ENUMERATION,
		CREATE_SORTED_LIST_OF_LOCAL_PHOTOS_BEGIN_LOCAL_ENUMERATION,
		CREATE_SORTED_LIST_OF_LOCAL_PHOTOS_CHECK_LOCAL_ENUMERATION,
		CREATE_SORTED_LIST_OF_LOCAL_PHOTOS_CHECK_CLOUD_ENUMERATION,
//		CREATE_SORTED_LIST_OF_PHOTOS_CREATE_MERGED_LIST_OF_LOCAL_AND_CLOUD_PHOTOS,
		CREATE_SORTED_LIST_OF_LOCAL_PHOTOS_HANDLE_ERRORS
	};

public:
	CSavegameQueuedOperation_CreateSortedListOfLocalPhotos()
		: CSavegameQueuedOperation(SAVE_QUEUED_CREATE_SORTED_LIST_OF_LOCAL_PHOTOS)
	{
	}

	virtual ~CSavegameQueuedOperation_CreateSortedListOfLocalPhotos() {}

	void Init(bool bDisplayWarningScreens);

	//    Return true to Pop() from queue
	virtual bool Update();
	virtual bool Shutdown(){return false;}

#if __BANK
	virtual const char *GetNameOfOperation() { return "CreateSortedListOfLocalPhotos"; }
#endif	//	__BANK

private:

	//	Private functions
	void EndCreatedSortedList();

	//	Private data
	eCreateSortedListOfLocalPhotosProgress m_CreateSortedListOfLocalPhotosProgress;
	bool m_bDisplayWarningScreens;

	bool m_bCloudEnumerationHasBeenStarted;
	bool m_bLocalEnumerationHasBeenStarted;

	bool m_bHadCloudError;
	bool m_bHadLocalError;
};

class CSavegameQueuedOperation_UpdateMetadataOfLocalAndCloudPhoto : public CSavegameQueuedOperation
{
	enum eUpdateMetadataProgress
	{
		UPDATE_METADATA_PROGRESS_BEGIN_LOAD,
		UPDATE_METADATA_PROGRESS_CHECK_LOAD,
		//	overwrite the title (could also overwrite the metadata and description if we need to in the future)
		UPDATE_METADATA_PROGRESS_BEGIN_SAVE,
		UPDATE_METADATA_PROGRESS_CHECK_SAVE,
		UPDATE_METADATA_PROGRESS_BEGIN_UPDATE_UGC_TITLE,
		UPDATE_METADATA_PROGRESS_CHECK_UPDATE_UGC_TITLE,
		UPDATE_METADATA_PROGRESS_WAIT_FOR_LOADING_MESSAGE_TO_FINISH,
		UPDATE_METADATA_PROGRESS_HANDLE_ERRORS
	};

public:
	CSavegameQueuedOperation_UpdateMetadataOfLocalAndCloudPhoto()
		: CSavegameQueuedOperation(SAVE_QUEUED_UPDATE_METADATA_OF_LOCAL_AND_CLOUD_PHOTO)
	{
	}

	virtual ~CSavegameQueuedOperation_UpdateMetadataOfLocalAndCloudPhoto() {}

	void Init(const CSavegamePhotoUniqueId &uniquePhotoId, CPhotoBuffer *pPhotoBuffer);

	//    Return true to Pop() from queue
	virtual bool Update();
	virtual bool Shutdown(){return false;}

#if __BANK
	virtual const char *GetNameOfOperation() { return "UpdateMetadataOfLocalAndCloudPhoto"; }
#endif	//	__BANK

private:
	eUpdateMetadataProgress m_UpdateMetadataProgress;

	CSavegamePhotoUniqueId m_UniquePhotoId;
	CPhotoBuffer *m_pBufferToStoreLoadedPhoto;

	//	Currently the only thing that the player can update after saving a photo is its title
	//	so I don't really need to update its metadata or description string
	atString m_NewTitle;
};


class CSavegameQueuedOperation_UploadLocalPhotoToCloud : public CSavegameQueuedOperation
{
	enum eUploadLocalPhotoToCloudProgress
	{
		LOCAL_PHOTO_UPLOAD_PROGRESS_CHECK_NUMBER_OF_UPLOADED_PHOTOS,
		LOCAL_PHOTO_UPLOAD_PROGRESS_DISPLAY_WARNING_OLDEST_DELETE,
		LOCAL_PHOTO_UPLOAD_PROGRESS_WAIT_FOR_DELETION_TO_FINISH,
		LOCAL_PHOTO_UPLOAD_PROGRESS_BEGIN_LOAD,
		LOCAL_PHOTO_UPLOAD_PROGRESS_CHECK_LOAD,
		LOCAL_PHOTO_UPLOAD_PROGRESS_BEGIN_CREATE_UGC,
		LOCAL_PHOTO_UPLOAD_PROGRESS_CHECK_CREATE_UGC,
		LOCAL_PHOTO_UPLOAD_CHECK_LIMELIGHT_UPLOAD,
		LOCAL_PHOTO_UPLOAD_PROGRESS_CHECK_PUBLISH,
//		LOCAL_PHOTO_UPLOAD_PROGRESS_WAIT_FOR_LOADING_MESSAGE_TO_FINISH,
		LOCAL_PHOTO_UPLOAD_PROGRESS_HANDLE_ERRORS
	};

public:
	CSavegameQueuedOperation_UploadLocalPhotoToCloud()
		: CSavegameQueuedOperation(SAVE_QUEUED_UPLOAD_LOCAL_PHOTO_TO_CLOUD)
	{
	}

	virtual ~CSavegameQueuedOperation_UploadLocalPhotoToCloud() {}

	void Init(const CSavegamePhotoUniqueId &uniquePhotoId, CPhotoBuffer *pPhotoBuffer);

	//    Return true to Pop() from queue
	virtual bool Update();
	virtual bool Shutdown(){return false;}

	const char *GetUGCContentId() const { return m_ContentIdOfUGCPhoto; }
	const char *GetLimelightAuthToken() const { return m_LimelightStagingAuthToken; }
	const char *GetLimelightFilename() const { return m_LimelightStagingFileName; }

	int GetNetStatusResultCode() const { return m_NetStatusResultCode; }


#if __BANK
	virtual const char *GetNameOfOperation() { return "UploadLocalPhotoToCloud"; }
#endif	//	__BANK

private:
	//	Private functions
	bool BeginCdnUpload(SaveGameDialogCode &returnErrorCode, const char* requestResponseHeaderValueToGet);

	void EndLocalPhotoUpload();

private:
	//	Private data
	char m_ContentIdOfUGCPhoto[RLUGC_MAX_CONTENTID_CHARS];

	u64 m_UploadPosixTime;

	char m_CdnUploadResponseHeaderValueBuffer[128];
	char m_LimelightStagingFileName[128];
	char m_LimelightStagingAuthToken[64];

	int m_NetStatusResultCode;

	eUploadLocalPhotoToCloudProgress m_UploadLocalPhotoProgress;

	netStatus m_LimelightUploadStatus;

	CSavegamePhotoUniqueId m_UniquePhotoId;
	CPhotoBuffer *m_pBufferContainingLocalPhoto;
};

#else	//	__LOAD_LOCAL_PHOTOS

class CSavegameQueuedOperation_CreateSortedListOfPhotos : public CSavegameQueuedOperation
{
	enum eCreateSortedListOfPhotosProgress
	{
		CREATE_SORTED_LIST_OF_PHOTOS_REQUEST_CLOUD_DIRECTORY_CONTENTS,
		CREATE_SORTED_LIST_OF_PHOTOS_WAIT_FOR_CLOUD_DIRECTORY_REQUEST,
		CREATE_SORTED_LIST_OF_PHOTOS_CREATE_MERGED_LIST_OF_LOCAL_AND_CLOUD_PHOTOS,
		CREATE_SORTED_LIST_OF_PHOTOS_HANDLE_ERRORS
	};

public:
	CSavegameQueuedOperation_CreateSortedListOfPhotos()
		: CSavegameQueuedOperation(SAVE_QUEUED_CREATE_SORTED_LIST_OF_PHOTOS)
	{
	}

	virtual ~CSavegameQueuedOperation_CreateSortedListOfPhotos() {}

	void Init(bool bOnlyGetNumberOfPhotos, bool bDisplayWarningScreens);

	//    Return true to Pop() from queue
	virtual bool Update();
	virtual bool Shutdown(){return false;}

#if __BANK
	virtual const char *GetNameOfOperation() { return "CreateSortedListOfPhotos"; }
#endif	//	__BANK

private:
	// Input
	eCreateSortedListOfPhotosProgress m_CreateSortedListOfPhotosProgress;
	bool m_bOnlyGetNumberOfPhotos;
	bool m_bDisplayWarningScreens;
};
#endif	//	__LOAD_LOCAL_PHOTOS


class CSavegameQueuedOperation_UploadMugshot : public CSavegameQueuedOperation
{
	enum eUploadMugshotProgress
	{
		MUGSHOT_UPLOAD_PROGRESS_BEGIN,
		MUGSHOT_UPLOAD_PROGRESS_CHECK
	};

public:
	CSavegameQueuedOperation_UploadMugshot() 
		: CSavegameQueuedOperation(SAVE_QUEUED_UPLOAD_MUGSHOT)
	{
	}

	virtual ~CSavegameQueuedOperation_UploadMugshot() {}

	void Init(CPhotoBuffer *pPhotoBuffer, s32 mugshotCharacterIndex);

	//    Return true to Pop() from queue
	virtual bool Update();
	virtual bool Shutdown(){return false;}

#if __BANK
	virtual const char *GetNameOfOperation() { return "UploadMugshot"; }
#endif	//	__BANK

private:
//	void EndMugshotUpload();

private:
	//	Private data
	eUploadMugshotProgress m_UploadMugshotProgress;

	CPhotoBuffer *m_pBufferContainingMugshot;
	s32 m_MugshotCharacterIndex;
};


class CSavegameQueuedOperation_DeleteFile : public CSavegameQueuedOperation
{
	enum eDeleteFileProgress
	{
		DELETE_FILE_PROGRESS_BEGIN,
		DELETE_FILE_PROGRESS_CHECK
	};

public:
	CSavegameQueuedOperation_DeleteFile()
		: CSavegameQueuedOperation(SAVE_QUEUED_DELETE_FILE)
	{
	}

	virtual ~CSavegameQueuedOperation_DeleteFile() {}

	void Init(const char *pFilename, bool bCloudFile, bool bIsAPhoto);

	//    Return true to Pop() from queue
	virtual bool Update();
	virtual bool Shutdown(){return false;}

#if __BANK
	virtual const char *GetNameOfOperation() { return "DeleteFile"; }
#endif	//	__BANK

private:
	char m_FilenameOfFileToDelete[SAVE_GAME_MAX_FILENAME_LENGTH_OF_CLOUD_FILE];		//	Use the max of SAVE_GAME_MAX_FILENAME_LENGTH_OF_LOCAL_FILE and SAVE_GAME_MAX_FILENAME_LENGTH_OF_CLOUD_FILE here
	bool m_bCloudFile;
	bool m_bIsAPhoto;

	eDeleteFileProgress m_DeleteFileProgress;
};


class CSavegameQueuedOperation_SaveLocalPhoto : public CSavegameQueuedOperation
{
	enum eSaveLocalPhotoProgress
	{
		SAVE_LOCAL_PHOTO_PROGRESS_BEGIN,
		SAVE_LOCAL_PHOTO_PROGRESS_CHECK

		//	Need to check that we don't already have the maximum amount of local photos
	};

public:
	CSavegameQueuedOperation_SaveLocalPhoto()
		: CSavegameQueuedOperation(SAVE_QUEUED_SAVE_LOCAL_PHOTO)
	{
	}

	virtual ~CSavegameQueuedOperation_SaveLocalPhoto() {}

	void Init(const CSavegamePhotoUniqueId &uniqueId, const CPhotoBuffer *pPhotoBuffer);

	//    Return true to Pop() from queue
	virtual bool Update();
	virtual bool Shutdown(){return false;}

#if __BANK
	virtual const char *GetNameOfOperation() { return "SaveLocalPhoto"; }
#endif	//	__BANK

private:
	CSavegamePhotoUniqueId m_UniqueIdForFilename;
	const CPhotoBuffer *m_pBufferToBeSaved;

	eSaveLocalPhotoProgress m_SaveLocalPhotoProgress;
};


class CSavegameQueuedOperation_LoadMostRecentSave : public CSavegameQueuedOperation
{
	enum eLoadMostRecentSaveProgress
	{
		LOAD_PROGRESS_CHECK_IF_SIGNED_IN,
		LOAD_PROGRESS_ENUMERATE_SAVEGAMES,
		LOAD_PROGRESS_FIND_MOST_RECENT_SAVE,
		LOAD_PROGRESS_BEGIN_LOAD,
		LOAD_PROGRESS_CHECK_LOAD
	};

public:
	CSavegameQueuedOperation_LoadMostRecentSave()
		: CSavegameQueuedOperation(SAVE_QUEUED_LOAD_MOST_RECENT_SAVE)
	{
	}

	virtual ~CSavegameQueuedOperation_LoadMostRecentSave() {}

	void Init();

	//    Return true to Pop() from queue
	virtual bool Update();
	virtual bool Shutdown(){return false;}

#if __BANK
	virtual const char *GetNameOfOperation() { return "LoadMostRecentSave"; }
#endif	//	__BANK

private:
	eLoadMostRecentSaveProgress m_LoadMostRecentSaveProgress;
	s32 m_IndexOfMostRecentSave;
};


#if USE_SAVE_DATA_MEMORY && !USE_DOWNLOAD0_FOR_AUTOSAVE_BACKUP
class CSavegameQueuedOperation_CreateBackupOfAutosaveSDM : public CSavegameQueuedOperation
{
	enum eAutosaveBackupProgress
	{
		AUTOSAVE_BACKUP_PROGRESS_BEGIN_LOAD,
		AUTOSAVE_BACKUP_PROGRESS_CHECK_LOAD,
		AUTOSAVE_BACKUP_PROGRESS_BEGIN_SAVE,
		AUTOSAVE_BACKUP_PROGRESS_CHECK_SAVE
	};

public:
	CSavegameQueuedOperation_CreateBackupOfAutosaveSDM()
		: CSavegameQueuedOperation(SAVE_QUEUED_CREATE_BACKUP_OF_AUTOSAVE_SDM)
	{
	}

	virtual ~CSavegameQueuedOperation_CreateBackupOfAutosaveSDM() {}

	void Init();

	//    Return true to Pop() from queue
	virtual bool Update();
	virtual bool Shutdown(){return false;}

#if __BANK
	virtual const char *GetNameOfOperation() { return "CreateBackupOfAutosaveSDM"; }
#endif	//	__BANK

private:
	int m_SlotToLoadFrom;
	int m_SlotToSaveTo;

	eAutosaveBackupProgress m_BackupProgress;
};
#endif	//	USE_SAVE_DATA_MEMORY && !USE_DOWNLOAD0_FOR_AUTOSAVE_BACKUP


#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
// Based on CSavegameQueuedOperation_ManualLoad
class CSavegameQueuedOperation_ExportSinglePlayerSavegame : public CSavegameQueuedOperation
{
	enum eExportSpSaveProgress
	{
		EXPORT_SP_SAVE_PROGRESS_BEGIN_SAVEGAME_LIST,
		EXPORT_SP_SAVE_PROGRESS_CHECK_SAVEGAME_LIST,
		EXPORT_SP_SAVE_PROGRESS_BEGIN_LOAD,
		EXPORT_SP_SAVE_PROGRESS_CHECK_LOAD,
		EXPORT_SP_SAVE_PROGRESS_BEGIN_EXPORT,
		EXPORT_SP_SAVE_PROGRESS_CHECK_EXPORT
	};
public:
	CSavegameQueuedOperation_ExportSinglePlayerSavegame()
		: CSavegameQueuedOperation(SAVE_QUEUED_EXPORT_SINGLE_PLAYER_SAVEGAME)
	{
	}

	virtual ~CSavegameQueuedOperation_ExportSinglePlayerSavegame() {}

	void Init();

	//	Return true to Pop() from queue
	virtual bool Update();
	virtual bool Shutdown(){return false;}

#if __BANK
	virtual const char *GetNameOfOperation() { return "ExportSPSave"; }
#endif	//	__BANK

	void SetQuitAsSoonAsNoSavegameOperationsAreRunning() { m_bQuitAsSoonAsNoSavegameOperationsAreRunning = true; }

private:
	bool m_bQuitAsSoonAsNoSavegameOperationsAreRunning;
	bool m_bNeedToCallEndGenericLoad;

	eExportSpSaveProgress m_ExportSpSaveProgress;
};
#endif // #if __ALLOW_EXPORT_OF_SP_SAVEGAMES


#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
// Based on CSavegameQueuedOperation_ManualSave
class CSavegameQueuedOperation_ImportSinglePlayerSavegame : public CSavegameQueuedOperation
{
	enum eImportSpSaveProgress
	{
		IMPORT_SP_SAVE_PROGRESS_BEGIN_SAVEGAME_LIST,
		IMPORT_SP_SAVE_PROGRESS_CHECK_SAVEGAME_LIST,
		IMPORT_SP_SAVE_PROGRESS_BEGIN_IMPORT,
		IMPORT_SP_SAVE_PROGRESS_CHECK_IMPORT,
		IMPORT_SP_SAVE_PROGRESS_DISPLAY_RESULT_MESSAGE
	};
public:
	CSavegameQueuedOperation_ImportSinglePlayerSavegame()
		: CSavegameQueuedOperation(SAVE_QUEUED_IMPORT_SINGLE_PLAYER_SAVEGAME)
	{
	}

	virtual ~CSavegameQueuedOperation_ImportSinglePlayerSavegame() {}

	void Init();

	//	Return true to Pop() from queue
	virtual bool Update();
	virtual bool Shutdown(){return false;}

#if __BANK
	virtual const char *GetNameOfOperation() { return "ImportSPSave"; }
#endif	//	__BANK

	void SetQuitAsSoonAsNoSavegameOperationsAreRunning() { m_bQuitAsSoonAsNoSavegameOperationsAreRunning = true; }

private:
	bool m_bQuitAsSoonAsNoSavegameOperationsAreRunning;
	bool m_bImportSucceeded;

	eImportSpSaveProgress m_ImportSpSaveProgress;
};


class CSavegameQueuedOperation_ImportSinglePlayerSavegameIntoFixedSlot : public CSavegameQueuedOperation
{
	enum eImportSpSaveIntoFixedSlotProgress
	{
		IMPORT_SP_SAVE_INTO_FIXED_SLOT_PROGRESS_BEGIN_IMPORT,
		IMPORT_SP_SAVE_INTO_FIXED_SLOT_PROGRESS_CHECK_IMPORT,
		IMPORT_SP_SAVE_INTO_FIXED_SLOT_PROGRESS_BEGIN_LOAD,
		IMPORT_SP_SAVE_INTO_FIXED_SLOT_PROGRESS_CHECK_LOAD,
		IMPORT_SP_SAVE_INTO_FIXED_SLOT_PROGRESS_DISPLAY_ERROR_MESSAGE
	};
public:
	CSavegameQueuedOperation_ImportSinglePlayerSavegameIntoFixedSlot()
		: CSavegameQueuedOperation(SAVE_QUEUED_IMPORT_SINGLE_PLAYER_SAVEGAME_INTO_FIXED_SLOT)
	{
	}

	virtual ~CSavegameQueuedOperation_ImportSinglePlayerSavegameIntoFixedSlot() {}

	void Init();
	bool CopyDataImmediately(const u8 *pData, u32 dataSize);

	//	Return true to Pop() from queue
	virtual bool Update();
	virtual bool Shutdown() { return false; }

#if __BANK
	virtual const char *GetNameOfOperation() { return "ImportSPSaveFixedSlot"; }
#endif	//	__BANK

	void SetQuitAsSoonAsNoSavegameOperationsAreRunning() { m_bQuitAsSoonAsNoSavegameOperationsAreRunning = true; }

private:
	bool m_bQuitAsSoonAsNoSavegameOperationsAreRunning;
	bool m_bImportSucceeded;
	bool m_bLoadingOfImportedSaveSucceeded;

	eImportSpSaveIntoFixedSlotProgress m_ImportSpSaveIntoFixedSlotProgress;
};

#endif // __ALLOW_IMPORT_OF_SP_SAVEGAMES

#endif	//	SAVEGAME_QUEUED_OPERATIONS_H

