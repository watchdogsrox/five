
#ifndef SAVEGAME_DEFINES_H
#define SAVEGAME_DEFINES_H

#include "Control/Replay/ReplaySettings.h"
#include "file/savegame.h"
#include "rline/ugc/rlugccommon.h"
#include "stats/StatsTypes.h"

#define USE_PROFILE_SAVEGAME	(__PPU || RSG_ORBIS || RSG_DURANGO)

#if __PPU
#define __QUICK_SCAN_FOR_AUTOLOAD_AND_SLOW_SCAN_ON_ANOTHER_THREAD	(1)
#else
#define __QUICK_SCAN_FOR_AUTOLOAD_AND_SLOW_SCAN_ON_ANOTHER_THREAD	(0)
#endif


#define __ALLOW_EXPORT_OF_SP_SAVEGAMES	(RSG_DURANGO || RSG_ORBIS)
#define __ALLOW_IMPORT_OF_SP_SAVEGAMES	(0)
#define __ALLOW_SP_CREDITS				(1)

#define __REMOVE_TIME_FROM_DATE_TIME_STRINGS_IN_EXPORT_MESSAGES		(__ALLOW_EXPORT_OF_SP_SAVEGAMES && 1)

#define __ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME	(0 && (__PPU || __XENON))

#define __ALLOW_LOCAL_MP_STATS_SAVES		(0)


#define __LOAD_LOCAL_PHOTOS					(1)
#define __UPDATING_LOCAL_PHOTO_TITLE_AFFECTS_MOD_TIME		(!RSG_ORBIS)		//	On PC and Xbox One, resaving a local photo to update its Title causes its ModTime to change. It becomes the most recent photo.
																				//	I keep my list of local photos in sync by deleting the photo from the list and re-inserting it at the top.
																				//	On PS4, the ModTime doesn't seem to change when resaving the photo.


#define SAVEGAME_USES_XML !__FINAL
#define SAVEGAME_USES_PSO 1

#if SAVEGAME_USES_PSO
#define USES_PSO_ONLY(x) x
#else
#define USES_PSO_ONLY(x)
#endif


//#if __WIN32PC
//#define NUM_MANUAL_SAVE_SLOTS				(64)
//#else
#define NUM_MANUAL_SAVE_SLOTS				(15)
//#endif // __WIN32PC


#define AUTOSAVE_SLOT_FOR_EPISODE_ZERO	(NUM_MANUAL_SAVE_SLOTS)
#define NUM_AUTOSAVE_SLOTS					(1)

#define TOTAL_NUMBER_OF_SLOTS_FOR_SAVEGAMES		(NUM_MANUAL_SAVE_SLOTS + NUM_AUTOSAVE_SLOTS)

#if RSG_ORBIS
#define INDEX_OF_BACKUPSAVE_SLOT			(TOTAL_NUMBER_OF_SLOTS_FOR_SAVEGAMES)
#define NUM_BACKUPSAVE_SLOTS				(TOTAL_NUMBER_OF_SLOTS_FOR_SAVEGAMES)
#endif

#define NUM_MULTIPLAYER_STATS_SAVE_SLOTS	(MAX_NUM_MP_CLOUD_FILES)

#if __ALLOW_LOCAL_MP_STATS_SAVES
#if RSG_ORBIS
#define INDEX_OF_FIRST_MULTIPLAYER_STATS_SAVE_SLOT	(INDEX_OF_BACKUPSAVE_SLOT + NUM_BACKUPSAVE_SLOTS)
#else
#define INDEX_OF_FIRST_MULTIPLAYER_STATS_SAVE_SLOT	(TOTAL_NUMBER_OF_SLOTS_FOR_SAVEGAMES)
#endif

#define INDEX_OF_MISSION_REPEAT_SAVE_SLOT	(INDEX_OF_FIRST_MULTIPLAYER_STATS_SAVE_SLOT + NUM_MULTIPLAYER_STATS_SAVE_SLOTS)		//	Used by Andrew Minghella when repeating a mission that you've already passed
#else	//	__ALLOW_LOCAL_MP_STATS_SAVES
#if RSG_ORBIS
#define INDEX_OF_MISSION_REPEAT_SAVE_SLOT	(INDEX_OF_BACKUPSAVE_SLOT + NUM_BACKUPSAVE_SLOTS)		//	Used by Andrew Minghella when repeating a mission that you've already passed
#else
#define INDEX_OF_MISSION_REPEAT_SAVE_SLOT	(TOTAL_NUMBER_OF_SLOTS_FOR_SAVEGAMES)		//	Used by Andrew Minghella when repeating a mission that you've already passed
#endif	//	RSG_ORBIS
#endif	//	__ALLOW_LOCAL_MP_STATS_SAVES

#define NUM_MISSION_REPEAT_SAVE_SLOTS		(1)


#if GTA_REPLAY

	#define INDEX_OF_REPLAYSAVE_SLOT			(INDEX_OF_MISSION_REPEAT_SAVE_SLOT + NUM_MISSION_REPEAT_SAVE_SLOTS)
	#define NUM_REPLAYSAVE_SLOTS				(1)

	#if USE_PROFILE_SAVEGAME
		#define INDEX_OF_PS3_PROFILE_SLOT			(INDEX_OF_REPLAYSAVE_SLOT + NUM_REPLAYSAVE_SLOTS)
		#define NUM_PS3_PROFILE_SLOTS				(1)

		#if USE_DOWNLOAD0_FOR_PROFILE_BACKUP
			#define INDEX_OF_BACKUP_PROFILE_SLOT			(INDEX_OF_PS3_PROFILE_SLOT + NUM_PS3_PROFILE_SLOTS)
			#define NUM_BACKUP_PROFILE_SLOTS				(1)

			#define INDEX_OF_FIRST_LOCAL_PHOTO			(INDEX_OF_BACKUP_PROFILE_SLOT + NUM_BACKUP_PROFILE_SLOTS)
		#else	//	USE_DOWNLOAD0_FOR_PROFILE_BACKUP
			#define INDEX_OF_FIRST_LOCAL_PHOTO			(INDEX_OF_PS3_PROFILE_SLOT + NUM_PS3_PROFILE_SLOTS)
		#endif	//	USE_DOWNLOAD0_FOR_PROFILE_BACKUP

	#else	//	USE_PROFILE_SAVEGAME
		#define INDEX_OF_FIRST_LOCAL_PHOTO			(INDEX_OF_REPLAYSAVE_SLOT + NUM_REPLAYSAVE_SLOTS)
	#endif	//	USE_PROFILE_SAVEGAME

#else	//	 GTA_REPLAY

	#if USE_PROFILE_SAVEGAME
		#define INDEX_OF_PS3_PROFILE_SLOT			(INDEX_OF_MISSION_REPEAT_SAVE_SLOT + NUM_MISSION_REPEAT_SAVE_SLOTS)
		#define NUM_PS3_PROFILE_SLOTS				(1)

		#if USE_DOWNLOAD0_FOR_PROFILE_BACKUP
			#define INDEX_OF_BACKUP_PROFILE_SLOT			(INDEX_OF_PS3_PROFILE_SLOT + NUM_PS3_PROFILE_SLOTS)
			#define NUM_BACKUP_PROFILE_SLOTS				(1)

			#define INDEX_OF_FIRST_LOCAL_PHOTO			(INDEX_OF_BACKUP_PROFILE_SLOT + NUM_BACKUP_PROFILE_SLOTS)
		#else	//	USE_DOWNLOAD0_FOR_PROFILE_BACKUP
			#define INDEX_OF_FIRST_LOCAL_PHOTO			(INDEX_OF_PS3_PROFILE_SLOT + NUM_PS3_PROFILE_SLOTS)
		#endif	//	USE_DOWNLOAD0_FOR_PROFILE_BACKUP

	#else	//	USE_PROFILE_SAVEGAME
		#define INDEX_OF_FIRST_LOCAL_PHOTO			(INDEX_OF_MISSION_REPEAT_SAVE_SLOT + NUM_MISSION_REPEAT_SAVE_SLOTS)
	#endif	//	USE_PROFILE_SAVEGAME

#endif	//	GTA_REPLAY

#define NUMBER_OF_LOCAL_PHOTOS				(96)

#define MAX_NUM_EXPECTED_SAVE_FILES		(INDEX_OF_FIRST_LOCAL_PHOTO + NUMBER_OF_LOCAL_PHOTOS)
#define MAX_NUM_SAVES_TO_SORT_INTO_SLOTS	(INDEX_OF_FIRST_LOCAL_PHOTO)		//	The filenames of local photos won't contain an index. Instead they'll have a random number. So I can't sort them like I can with savegames.
																				//	In CSavegameList::SortSaveGameSlotsAndSizes, I'll just ignore the photos.


#define INDEX_OF_FIRST_LEVEL	(1)	//	Alpine Level is 1 in Jimmy. The main game in GTA4 had index 0


enum eSavegameFileType
{
	SG_FILE_TYPE_UNKNOWN,
	SG_FILE_TYPE_SAVEGAME,
#if __ALLOW_LOCAL_MP_STATS_SAVES
	SG_FILE_TYPE_MULTIPLAYER_STATS,
#endif	//	__ALLOW_LOCAL_MP_STATS_SAVES
	SG_FILE_TYPE_MISSION_REPEAT,	//	Used by Andrew Minghella

#if GTA_REPLAY
	SG_FILE_TYPE_REPLAY,
#endif	//	GTA_REPLAY

#if USE_PROFILE_SAVEGAME
	SG_FILE_TYPE_PS3_PROFILE,

#if USE_DOWNLOAD0_FOR_PROFILE_BACKUP
	SG_FILE_TYPE_BACKUP_PROFILE,
#endif	//	USE_DOWNLOAD0_FOR_PROFILE_BACKUP

#endif	//	USE_PROFILE_SAVEGAME

	SG_FILE_TYPE_LOCAL_PHOTO
#if RSG_ORBIS
	,SG_FILE_TYPE_SAVEGAME_BACKUP
#endif
};


enum MemoryCardError
{
	MEM_CARD_COMPLETE,
	MEM_CARD_BUSY,
	MEM_CARD_ERROR
};


enum eSaveOperation
{
	OPERATION_NONE,
	OPERATION_SCANNING_CONSOLE_FOR_LOADING_SAVEGAMES,		//	Only set by CSavegameFrontEnd::SaveGameListScreenSetup - load menu
	OPERATION_SCANNING_CONSOLE_FOR_SAVING_SAVEGAMES,		//	Only set by CSavegameFrontEnd::SaveGameListScreenSetup - manual save menu

//	OPERATION_SCANNING_CLOUD_FOR_LOADING_PHOTOS,			//	Do I need to call CGenericGameStorage::SetSaveOperation()
//	OPERATION_SCANNING_CLOUD_FOR_SAVING_PHOTOS,				//	for these somewhere?

	OPERATION_LOADING_SAVEGAME_FROM_CONSOLE,	//	This also handles the autoloading of the most recent file found by CSavegameAutoload
	OPERATION_LOADING_REPLAY_SAVEGAME_FROM_CONSOLE,
	OPERATION_LOADING_MPSTATS_FROM_CLOUD,
	OPERATION_LOADING_PHOTO_FROM_CLOUD,

	OPERATION_SAVING_SAVEGAME_TO_CONSOLE,
	OPERATION_SAVING_MISSION_REPEAT_SAVEGAME_TO_CONSOLE,
	OPERATION_SAVING_REPLAY_SAVEGAME_TO_CONSOLE,
	OPERATION_AUTOSAVING,
	OPERATION_SAVING_MPSTATS_TO_CLOUD,
	OPERATION_SAVING_PHOTO_TO_CLOUD,

#if __ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME
	OPERATION_UPLOADING_SAVEGAME_TO_CLOUD,
#endif	//	__ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME

#if RSG_ORBIS
	OPERATION_CHECKING_FOR_FREE_SPACE_AT_START_OF_GAME,
#endif
	OPERATION_CHECKING_FOR_FREE_SPACE,	//	Only set by CSaveConfirmationMessage
	OPERATION_CHECKING_IF_FILE_EXISTS,
	OPERATION_DELETING_LOCAL,
	OPERATION_DELETING_CLOUD,
	OPERATION_SAVING_LOCAL_PHOTO,
	OPERATION_LOADING_LOCAL_PHOTO,
	OPERATION_ENUMERATING_PHOTOS,		//	called by CSavegameQueuedOperation_CreateSortedListOfLocalPhotos
	OPERATION_ENUMERATING_SAVEGAMES,
#if __ALLOW_LOCAL_MP_STATS_SAVES
	,OPERATION_SAVING_MPSTATS_SAVEGAME_TO_CONSOLE,
	OPERATION_LOADING_MPSTATS_SAVEGAME_FROM_CONSOLE
#endif	//	__ALLOW_LOCAL_MP_STATS_SAVES

	//	There is also a CSavegameSlowPS3Scan which fills in the array inside CSavegameList. It could be considered
	//	another operation even though it doesn't have an entry in this enum.
};


enum eTypeOfSavegame
{
	SAVEGAME_SINGLE_PLAYER,
	SAVEGAME_MULTIPLAYER_CHARACTER,
	SAVEGAME_MULTIPLAYER_COMMON,
	SAVEGAME_MAX_TYPES
};

#define __MP_SCRIPT_DATA_IS_SAVED_IN_SAVEGAME_MULTIPLAYER_COMMON		(0)		//	if this is 0 then MP script data is stored in SAVEGAME_MULTIPLAYER_CHARACTER

#if __MP_SCRIPT_DATA_IS_SAVED_IN_SAVEGAME_MULTIPLAYER_COMMON
	#define SAVEGAME_MULTIPLAYER_SLOT_CONTAINING_MP_SCRIPT_DATA		(SAVEGAME_MULTIPLAYER_COMMON)
#else
	#define SAVEGAME_MULTIPLAYER_SLOT_CONTAINING_MP_SCRIPT_DATA		(SAVEGAME_MULTIPLAYER_CHARACTER)
#endif

enum eReasonForFailureToLoadSavegame
{
	//This is not a failure
	LOAD_FAILED_REASON_NONE,

	//Failure codes
	LOAD_FAILED_REASON_FAILED_TO_LOAD,
	LOAD_FAILED_REASON_FILE_CORRUPT,
	LOAD_FAILED_REASON_SERVER_TIMEOUT,
	LOAD_FAILED_REASON_SERVER_ERROR,
	LOAD_FAILED_REASON_FILE_NOT_FOUND,
	LOAD_FAILED_REASON_DIRTY_CLOUD_READ,
	LOAD_FAILED_REASON_DIRTY_PROFILE_STAT_READ,
	LOAD_FAILED_REASON_REFRESH_SAVEMIGRATION_STATUS,

	LOAD_FAILED_REASON_MAX
};

enum eHttpStatusCodes
{
	 HTTP_CODE_CLIENTCLOSEDREQUEST = 499  //Client Closed Request
	,HTTP_CODE_FILENOTFOUND        = 404 //File Not Found
	,HTTP_CODE_REQUESTTIMEOUT      = 408 //Request Timeout
	,HTTP_CODE_TOOMANYREQUESTS     = 429 //Too Many Requests
	,HTTP_CODE_ANYSERVERERROR      = 500 //Any HTTP server error
	,HTTP_CODE_GATEWAYTIMEOUT      = 504 //Gateway Timeout
	,HTTP_CODE_NETCONNECTTIMEOUT   = 599 //Network connect timeout error
};

class CIndexOfPhotoOnCloud
{
public:
	explicit CIndexOfPhotoOnCloud(s32 nIndex) : m_nIndex(nIndex) {}

	s32 GetIntegerValue() { return m_nIndex; }

	bool IsValid() { return m_nIndex != -1; }
	void SetInvalid() { m_nIndex = -1; }

private:
	s32 m_nIndex;
};

#if !__LOAD_LOCAL_PHOTOS
class CIndexWithinMergedPhotoList
{
public:
	explicit CIndexWithinMergedPhotoList(s32 nIndex, bool bIsMaximised) : m_nIndex(nIndex), m_bIsMaximised(bIsMaximised) {}

	s32 GetIntegerValue() const { return m_nIndex; }
	bool GetIsMaximised() const { return m_bIsMaximised; }

	bool IsValid() const { return m_nIndex != -1; }
	void SetInvalid() { m_nIndex = -1; }

	bool operator == (const CIndexWithinMergedPhotoList &other) const { return ( (m_nIndex == other.m_nIndex) && (m_bIsMaximised == other.m_bIsMaximised) ); }
	bool operator != (const CIndexWithinMergedPhotoList &other) const { return ( (m_nIndex != other.m_nIndex) || (m_bIsMaximised != other.m_bIsMaximised) ); }
private:
	s32 m_nIndex;
	bool m_bIsMaximised;
};
#endif	//	!__LOAD_LOCAL_PHOTOS


class CUndeletedEntryInMergedPhotoList
{
public:
	explicit CUndeletedEntryInMergedPhotoList(s32 nIndex, bool bIsMaximised) : m_nIndex(nIndex), m_bIsMaximised(bIsMaximised) {}

	s32 GetIntegerValue() const { return m_nIndex; }
	bool GetIsMaximised() const { return m_bIsMaximised; }

	bool IsValid() const { return m_nIndex != -1; }
private:
	s32 m_nIndex;
	bool m_bIsMaximised;
};


class CEntryInMergedPhotoListForDeletion
{
public:
	explicit CEntryInMergedPhotoListForDeletion(s32 nIndex) : m_nIndex(nIndex) {}

	s32 GetIntegerValue() const { return m_nIndex; }

	bool IsValid() const { return m_nIndex != -1; }
	void SetInvalid() { m_nIndex = -1; }
private:
	s32 m_nIndex;
};


struct sRequestData 
{
    sRequestData()
        : m_nFileID(0)
        , m_nFileVersion(0)
        , m_nLanguage(RLSC_LANGUAGE_UNKNOWN)
    {
        m_PathToFileOnCloud[0] = '\0';
    }

    sRequestData& operator=(const sRequestData& other)
    {
        safecpy(m_PathToFileOnCloud, other.m_PathToFileOnCloud);
        m_nFileID = other.m_nFileID;
        m_nFileVersion = other.m_nFileVersion;
        m_nLanguage = other.m_nLanguage;

        return *this;
    }

    static const s32 c_sMaxLengthOfFilename = 128;
    char m_PathToFileOnCloud[c_sMaxLengthOfFilename];
    int m_nFileID;
    int m_nFileVersion;
    rlScLanguage m_nLanguage;
};

#endif	//	SAVEGAME_DEFINES_H

