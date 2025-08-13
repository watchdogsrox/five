
#include "savegame_export.h"

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES

// Rage headers
#include "file/asset.h"
#include "parser/manager.h"
#include "zlib/lz4.h"

// Framework headers
#include "fwsys/gameskeleton.h"

// Game headers
#include "frontend/WarningScreen.h"
#include "Network/Live/livemanager.h"
#include "SaveLoad/GenericGameStorage.h"
#include "SaveLoad/savegame_channel.h"
#include "SaveLoad/savegame_frontend.h"
#include "SaveLoad/savegame_users_and_devices.h"
#include "SaveLoad/SavegameScriptData/script_save_data.h"
#include "system/FileMgr.h"

//	XPARAM(noencryptsave);

#if __WIN32PC || RSG_DURANGO
XPARAM(showProfileBypass);
#endif // __WIN32PC || RSG_DURANGO


PARAM(ExportSavegameToLocalPC, "[savegame_export] Instead of uploading the savegame to the cloud, we'll save it to the X: drive on the local PC");
PARAM(SkipCheckingForPreviouslyUploadedSavegame, "[savegame_export] when uploading a Single Player savegame, don't first check if a savegame has already been uploaded");

const u32 s_MaxBufferSize_LoadedPsoData = 640000;
CSaveGameMigrationBuffer CSaveGameExport::sm_BufferContainingLoadedPsoData(s_MaxBufferSize_LoadedPsoData);

const u32 s_MaxBufferSize_SavedXmlData_Uncompressed = 2505000;	//	This might need to be increased
CSaveGameMigrationBuffer CSaveGameExport::sm_BufferContainingXmlDataForSaving_Uncompressed(s_MaxBufferSize_SavedXmlData_Uncompressed);

const u32 s_MaxBufferSize_SavedXmlData_Compressed = 2505000;	//	This might need to be increased. 
																//	It needs to be high to account for the worst-case compression.
CSaveGameMigrationBuffer CSaveGameExport::sm_BufferContainingXmlDataForSaving_Compressed(s_MaxBufferSize_SavedXmlData_Compressed);

parTree *CSaveGameExport::sm_pParTreeToBeSaved = NULL;

CSaveGameMigrationData_Code CSaveGameExport::sm_SaveGameBufferContents_Code;
CSaveGameMigrationData_Script CSaveGameExport::sm_SaveGameBufferContents_Script;

CSaveGameMigrationMetadata CSaveGameExport::sm_SaveGameMetadata;

CSaveGameExport::SaveBlockDataForExportStatus CSaveGameExport::sm_SaveBlockDataForExportStatus = SAVEBLOCKDATAFOREXPORT_IDLE;
CSaveGameExport::eSaveGameExportProgress CSaveGameExport::sm_SaveGameExportProgress = SAVEGAME_EXPORT_PROGRESS_READ_PSO_DATA_FROM_BUFFER;
bool CSaveGameExport::sm_bIsExporting = false;

const char *CSaveGameExport::sm_XMLTextDirectoryName = "X:\\";
const char *CSaveGameExport::sm_XMLTextFilename = "exported_savegame_compressed.xml";


eSaveMigrationStatus CSaveGameExport::sm_StatusOfPreviouslyUploadedSavegameCheck = SM_STATUS_NONE;
s32 CSaveGameExport::sm_UploadErrorCodeOfPreviouslyUploadedSavegameCheck = static_cast<s32>(RLSM_ERROR_NONE);
CSaveGameExport::eNetworkProblemsForSavegameExport CSaveGameExport::sm_NetworkProblemDuringCheckForPreviousUpload = SAVEGAME_EXPORT_NETWORK_PROBLEM_NONE;
bool CSaveGameExport::sm_bTransferAlreadyCompleted = false;
CSaveGameExport_DetailsOfPreviouslyUploadedSavegame CSaveGameExport::sm_DetailsOfPreviouslyUploadedSavegame;

eSaveMigrationStatus CSaveGameExport::sm_UploadStatusOfLastUploadAttempt = SM_STATUS_NONE;
s32 CSaveGameExport::sm_UploadErrorCodeOfLastUploadAttempt = static_cast<s32>(RLSM_ERROR_NONE);
CSaveGameExport::eNetworkProblemsForSavegameExport CSaveGameExport::sm_NetworkProblemDuringUpload = SAVEGAME_EXPORT_NETWORK_PROBLEM_NONE;

char CSaveGameExport::sm_ErrorCodeAsString[MAX_LENGTH_OF_ERROR_CODE_STRING];


#if __BANK

#define NUMBER_OF_NETWORK_PROBLEM_NAMES			(CSaveGameExport::SAVEGAME_EXPORT_NETWORK_PROBLEM_SOCIAL_CLUB_POLICIES_OUT_OF_DATE+1)
#define MAX_LENGTH_OF_NETWORK_PROBLEM_NAME		(40)

/////////////////////////////////////////////////////////////////////////////////////
// CSaveGameExport_DebugForUploadInfoMessage
/////////////////////////////////////////////////////////////////////////////////////

class CSaveGameExport_DebugForUploadInfoMessage
{
public:
	static void InitWidgets(bkBank *pParentBank);

	static bool GetInfoMessageOverrideEnabled() { return sm_bInfoMessageOverrideEnabled; }
	static CSaveGameExport::eNetworkProblemsForSavegameExport GetNetworkProblemOverride() { return sm_NetworkProblemOverride; }
	static bool GetTransferComplete() { return sm_bTransferComplete; }
	static bool GetCheckedForPreviousUpload() { return sm_bCheckedForPreviousUpload; }
	static bool GetHasAPreviousUpload() { return sm_bHasAPreviousUpload; }

private:
	static bool sm_bInfoMessageOverrideEnabled;
	static CSaveGameExport::eNetworkProblemsForSavegameExport sm_NetworkProblemOverride;
	static bool sm_bTransferComplete;
	static bool sm_bCheckedForPreviousUpload;
	static bool sm_bHasAPreviousUpload;

	static char sm_NetworkProblemStrings[NUMBER_OF_NETWORK_PROBLEM_NAMES][MAX_LENGTH_OF_NETWORK_PROBLEM_NAME];
	static const char *sm_NetworkProblemNames[NUMBER_OF_NETWORK_PROBLEM_NAMES];
};

bool CSaveGameExport_DebugForUploadInfoMessage::sm_bInfoMessageOverrideEnabled = false;
CSaveGameExport::eNetworkProblemsForSavegameExport CSaveGameExport_DebugForUploadInfoMessage::sm_NetworkProblemOverride = CSaveGameExport::SAVEGAME_EXPORT_NETWORK_PROBLEM_NONE;
bool CSaveGameExport_DebugForUploadInfoMessage::sm_bTransferComplete = false;
bool CSaveGameExport_DebugForUploadInfoMessage::sm_bCheckedForPreviousUpload = false;
bool CSaveGameExport_DebugForUploadInfoMessage::sm_bHasAPreviousUpload = false;

char CSaveGameExport_DebugForUploadInfoMessage::sm_NetworkProblemStrings[NUMBER_OF_NETWORK_PROBLEM_NAMES][MAX_LENGTH_OF_NETWORK_PROBLEM_NAME];
const char *CSaveGameExport_DebugForUploadInfoMessage::sm_NetworkProblemNames[NUMBER_OF_NETWORK_PROBLEM_NAMES];


void CSaveGameExport_DebugForUploadInfoMessage::InitWidgets(bkBank *pParentBank)
{
	for (u32 loop = 0; loop < NUMBER_OF_NETWORK_PROBLEM_NAMES; loop++)
	{
		CSaveGameExport::eNetworkProblemsForSavegameExport networkProblem = static_cast<CSaveGameExport::eNetworkProblemsForSavegameExport>(loop);
		const char *pNetworkProblemString = CSaveGameExport::GetNetworkProblemName(networkProblem);
		safecpy(sm_NetworkProblemStrings[loop], pNetworkProblemString, MAX_LENGTH_OF_NETWORK_PROBLEM_NAME);
		sm_NetworkProblemNames[loop] = &sm_NetworkProblemStrings[loop][0];
	}

	pParentBank->PushGroup("Override Upload Info Message");
	pParentBank->AddToggle("Info Message Override Enabled", &sm_bInfoMessageOverrideEnabled);
	pParentBank->AddCombo("Network Problem", (int*)&sm_NetworkProblemOverride, NUMBER_OF_NETWORK_PROBLEM_NAMES, &(sm_NetworkProblemNames[0]) );
	pParentBank->AddToggle("Transfer Complete", &sm_bTransferComplete);
	pParentBank->AddToggle("Checked For Previous Upload", &sm_bCheckedForPreviousUpload);
	pParentBank->AddToggle("Has A Previous Upload", &sm_bHasAPreviousUpload);
	pParentBank->PopGroup();
}

/////////////////////////////////////////////////////////////////////////////////////
// End of CSaveGameExport_DebugForUploadInfoMessage
/////////////////////////////////////////////////////////////////////////////////////


const char *GetSaveMigrationErrorCodeName(rage::rlSaveMigrationErrorCode errorCode)
{
	switch (errorCode)
	{
		case RLSM_ERROR_UNKNOWN : return "RLSM_ERROR_UNKNOWN";
		case RLSM_ERROR_NONE : return "RLSM_ERROR_NONE";
		case RLSM_ERROR_AUTHENTICATION_FAILED : return "RLSM_ERROR_AUTHENTICATION_FAILED";
		case RLSM_ERROR_MAINTENANCE : return "RLSM_ERROR_MAINTENANCE";
		case RLSM_ERROR_DOES_NOT_MATCH : return "RLSM_ERROR_DOES_NOT_MATCH";
		case RLSM_ERROR_ROCKSTAR_ACCOUNT_REQUIRED : return "RLSM_ERROR_ROCKSTAR_ACCOUNT_REQUIRED";
		case RLSM_ERROR_INVALID_TITLE : return "RLSM_ERROR_INVALID_TITLE";
		case RLSM_ERROR_DISABLED : return "RLSM_ERROR_DISABLED";
		case RLSM_ERROR_IN_PROGRESS : return "RLSM_ERROR_IN_PROGRESS";
		case RLSM_ERROR_ALREADY_COMPLETE : return "RLSM_ERROR_ALREADY_COMPLETE";
		case RLSM_ERROR_INVALID_FILEDATA : return "RLSM_ERROR_INVALID_FILEDATA";
		case RLSM_ERROR_INVALID_PARAMETER : return "RLSM_ERROR_INVALID_PARAMETER";
		case RLSM_ERROR_FILESTORAGE_UNAVAILABLE : return "RLSM_ERROR_FILESTORAGE_UNAVAILABLE";
		case RLSM_ERROR_INVALID_TOKEN : return "RLSM_ERROR_INVALID_TOKEN";
		case RLSM_ERROR_EXPIRED_TOKEN : return "RLSM_ERROR_EXPIRED_TOKEN";
		case RLSM_ERROR_EXPIRED_SAVE_MIGRATION : return "RLSM_ERROR_EXPIRED_SAVE_MIGRATION";
		case RLSM_ERROR_NOT_IN_PROGRESS : return "RLSM_ERROR_NOT_IN_PROGRESS";
		case RLSM_ERROR_NO_MIGRATION_RECORDS : return "RLSM_ERROR_NO_MIGRATION_RECORDS";
		case RLSM_ERROR_END : return "RLSM_ERROR_END";
	}

	return "<unknown>";
}

#define NUMBER_OF_SAVEGAME_MIGRATION_ERROR_CODE_NAMES	(RLSM_ERROR_END+1) // Add 1 for RLSM_ERROR_UNKNOWN = -1
#define MAX_LENGTH_OF_SAVEGAME_MIGRATION_ERROR_CODE_NAME	(40)

/////////////////////////////////////////////////////////////////////////////////////
// CSaveGameExport_DebugForPreviousUploadCheck
/////////////////////////////////////////////////////////////////////////////////////

class CSaveGameExport_DebugForPreviousUploadCheck
{
public:
	static void InitWidgets(bkBank *pParentBank);

	static bool GetSkipCheckForPreviousUpload() { return sm_bSkipCheckForPreviousUpload; }
	static bool GetErrorMessageOverrideEnabled() { return sm_bErrorMessageOverrideEnabled; }

	static bool GetHasPreviousSave() { return (sm_ErrorMessageOverride == HAS_PREVIOUS_SAVE); }
	static bool GetHadError() { return (sm_ErrorMessageOverride == HAD_ERROR); }

	static rlSaveMigrationErrorCode GetErrorCode() { return sm_ErrorCodeOverride; }

private:
	enum eErrorMessageOverride
	{
		HAS_PREVIOUS_SAVE,
		HAD_ERROR,
		NOT_CHECKED
	};

	static bool sm_bSkipCheckForPreviousUpload;
	static bool sm_bErrorMessageOverrideEnabled;
	static eErrorMessageOverride sm_ErrorMessageOverride;
	static rlSaveMigrationErrorCode sm_ErrorCodeOverride;

	static const char *sm_ErrorMessageOverrideNames[];

	static char sm_ErrorCodeNameStrings[NUMBER_OF_SAVEGAME_MIGRATION_ERROR_CODE_NAMES][MAX_LENGTH_OF_SAVEGAME_MIGRATION_ERROR_CODE_NAME];
	static const char *sm_ErrorCodeNames[NUMBER_OF_SAVEGAME_MIGRATION_ERROR_CODE_NAMES];
};

bool CSaveGameExport_DebugForPreviousUploadCheck::sm_bSkipCheckForPreviousUpload = false;
bool CSaveGameExport_DebugForPreviousUploadCheck::sm_bErrorMessageOverrideEnabled = false;
CSaveGameExport_DebugForPreviousUploadCheck::eErrorMessageOverride CSaveGameExport_DebugForPreviousUploadCheck::sm_ErrorMessageOverride = HAD_ERROR;
rlSaveMigrationErrorCode CSaveGameExport_DebugForPreviousUploadCheck::sm_ErrorCodeOverride = RLSM_ERROR_NONE;

const char *CSaveGameExport_DebugForPreviousUploadCheck::sm_ErrorMessageOverrideNames[3] = {
	"HAS_PREVIOUS_SAVE",
	"HAD_ERROR",
	"NOT_CHECKED"
};

char CSaveGameExport_DebugForPreviousUploadCheck::sm_ErrorCodeNameStrings[NUMBER_OF_SAVEGAME_MIGRATION_ERROR_CODE_NAMES][MAX_LENGTH_OF_SAVEGAME_MIGRATION_ERROR_CODE_NAME];
const char *CSaveGameExport_DebugForPreviousUploadCheck::sm_ErrorCodeNames[NUMBER_OF_SAVEGAME_MIGRATION_ERROR_CODE_NAMES];

void CSaveGameExport_DebugForPreviousUploadCheck::InitWidgets(bkBank *pParentBank)
{
	for (u32 loop = 0; loop < NUMBER_OF_SAVEGAME_MIGRATION_ERROR_CODE_NAMES; loop++)
	{
		//	Subtract 1 so we get the string for RLSM_ERROR_UNKNOWN = -1
		rage::rlSaveMigrationErrorCode errorCode = static_cast<rage::rlSaveMigrationErrorCode>(loop-1);
		const char *pErrorCodeString = GetSaveMigrationErrorCodeName(errorCode);
		safecpy(sm_ErrorCodeNameStrings[loop], pErrorCodeString, MAX_LENGTH_OF_SAVEGAME_MIGRATION_ERROR_CODE_NAME);
		sm_ErrorCodeNames[loop] = &sm_ErrorCodeNameStrings[loop][0];
	}

	pParentBank->PushGroup("Override Previous Check Warning Screen");
	pParentBank->AddToggle("Skip Check For Previous Upload", &sm_bSkipCheckForPreviousUpload);
	pParentBank->AddToggle("Override Error Message", &sm_bErrorMessageOverrideEnabled);
	pParentBank->AddCombo("Error Message", (int*)&sm_ErrorMessageOverride, NELEM(sm_ErrorMessageOverrideNames), &sm_ErrorMessageOverrideNames[0]);
	pParentBank->AddCombo("Error Code", (int*)&sm_ErrorCodeOverride, NUMBER_OF_SAVEGAME_MIGRATION_ERROR_CODE_NAMES, &(sm_ErrorCodeNames[0]), -1);
	pParentBank->PopGroup();
}


/////////////////////////////////////////////////////////////////////////////////////
// CSaveGameExport_DebugForUpload
/////////////////////////////////////////////////////////////////////////////////////

class CSaveGameExport_DebugForUpload
{
public:
	static void InitWidgets(bkBank *pParentBank);

	static bool GetExportToLocalPC() { return sm_bExportToLocalPC; }
	static bool GetErrorMessageOverrideEnabled() { return sm_bErrorMessageOverrideEnabled; }
	static bool GetHadError() { return sm_bHadError; }
	static rlSaveMigrationErrorCode GetErrorCode() { return sm_ErrorCodeOverride; }

private:
	static bool sm_bExportToLocalPC;
	static bool sm_bErrorMessageOverrideEnabled;
	static bool sm_bHadError;
	static rlSaveMigrationErrorCode sm_ErrorCodeOverride;

	static char sm_ErrorCodeNameStrings[NUMBER_OF_SAVEGAME_MIGRATION_ERROR_CODE_NAMES][MAX_LENGTH_OF_SAVEGAME_MIGRATION_ERROR_CODE_NAME];
	static const char *sm_ErrorCodeNames[NUMBER_OF_SAVEGAME_MIGRATION_ERROR_CODE_NAMES];
};

bool CSaveGameExport_DebugForUpload::sm_bExportToLocalPC = false;
bool CSaveGameExport_DebugForUpload::sm_bErrorMessageOverrideEnabled = false;
bool CSaveGameExport_DebugForUpload::sm_bHadError = false;
rlSaveMigrationErrorCode CSaveGameExport_DebugForUpload::sm_ErrorCodeOverride = RLSM_ERROR_NONE;

char CSaveGameExport_DebugForUpload::sm_ErrorCodeNameStrings[NUMBER_OF_SAVEGAME_MIGRATION_ERROR_CODE_NAMES][MAX_LENGTH_OF_SAVEGAME_MIGRATION_ERROR_CODE_NAME];
const char *CSaveGameExport_DebugForUpload::sm_ErrorCodeNames[NUMBER_OF_SAVEGAME_MIGRATION_ERROR_CODE_NAMES];

void CSaveGameExport_DebugForUpload::InitWidgets(bkBank *pParentBank)
{
	for (u32 loop = 0; loop < NUMBER_OF_SAVEGAME_MIGRATION_ERROR_CODE_NAMES; loop++)
	{
		//	Subtract 1 so we get the string for RLSM_ERROR_UNKNOWN = -1
		rage::rlSaveMigrationErrorCode errorCode = static_cast<rage::rlSaveMigrationErrorCode>(loop-1);
		const char *pErrorCodeString = GetSaveMigrationErrorCodeName(errorCode);
		safecpy(sm_ErrorCodeNameStrings[loop], pErrorCodeString, MAX_LENGTH_OF_SAVEGAME_MIGRATION_ERROR_CODE_NAME);
		sm_ErrorCodeNames[loop] = &sm_ErrorCodeNameStrings[loop][0];
	}

	pParentBank->PushGroup("Override Upload Warning Screen");
	pParentBank->AddToggle("Override Error Message", &sm_bErrorMessageOverrideEnabled);
	pParentBank->AddToggle("Had Error", &sm_bHadError);
	pParentBank->AddCombo("Error Code", (int*)&sm_ErrorCodeOverride, NUMBER_OF_SAVEGAME_MIGRATION_ERROR_CODE_NAMES, &(sm_ErrorCodeNames[0]), -1);
	pParentBank->AddSeparator();
	pParentBank->AddToggle("Export Savegame To Local PC", &sm_bExportToLocalPC);
	pParentBank->PopGroup();
}
#endif // __BANK



/////////////////////////////////////////////////////////////////////////////////////
// CSaveGameExport_BackgroundCheckForPreviousUpload
/////////////////////////////////////////////////////////////////////////////////////
class CSaveGameExport_BackgroundCheckForPreviousUpload
{
public:
	static void Init(unsigned initMode);
	static void Update();

private:

	enum eBackgroundCheckForPreviousUploadState
	{
		BACKGROUND_CHECK_FOR_PREVIOUS_UPLOAD_READY_TO_START,
		BACKGROUND_CHECK_FOR_PREVIOUS_UPLOAD_CHECKING,
		BACKGROUND_CHECK_FOR_PREVIOUS_UPLOAD_ERROR,
		BACKGROUND_CHECK_FOR_PREVIOUS_UPLOAD_WAIT_TO_RETRY_AFTER_ERROR,
		BACKGROUND_CHECK_FOR_PREVIOUS_UPLOAD_DONE
	};

	static eBackgroundCheckForPreviousUploadState sm_BackgroundCheckForPreviousUpload;

	static u32 sm_NextRetryTime;
	static u32 sm_Retries;
};


CSaveGameExport_BackgroundCheckForPreviousUpload::eBackgroundCheckForPreviousUploadState CSaveGameExport_BackgroundCheckForPreviousUpload::sm_BackgroundCheckForPreviousUpload = BACKGROUND_CHECK_FOR_PREVIOUS_UPLOAD_READY_TO_START;
u32 CSaveGameExport_BackgroundCheckForPreviousUpload::sm_NextRetryTime = 0;
u32 CSaveGameExport_BackgroundCheckForPreviousUpload::sm_Retries = 0;


void CSaveGameExport_BackgroundCheckForPreviousUpload::Init(unsigned initMode)
{
	if (initMode == INIT_CORE)
	{
		sm_BackgroundCheckForPreviousUpload = BACKGROUND_CHECK_FOR_PREVIOUS_UPLOAD_READY_TO_START;
		sm_NextRetryTime = 0;
		sm_Retries = 0;
	}
}

void CSaveGameExport_BackgroundCheckForPreviousUpload::Update()
{
	switch (sm_BackgroundCheckForPreviousUpload)
	{
	case BACKGROUND_CHECK_FOR_PREVIOUS_UPLOAD_READY_TO_START :
		{
			if (Tunables::IsInstantiated() && Tunables::GetInstance().HaveTunablesLoaded())
			{
				if (NetworkInterface::IsMigrationAvailable())
				{
					// Have a separate tunable so we can disable this background check if we need to.
					// If the tunable doesn't exist then the default behavior is to allow the background check.
					if (Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("ENABLE_SAVE_MIGRATION_BACKGROUND_CHECK", 0xE8B42CC6), true))
					{
						if (CSaveGameExport::BeginCheckForPreviousUpload())
						{
							sm_BackgroundCheckForPreviousUpload = BACKGROUND_CHECK_FOR_PREVIOUS_UPLOAD_CHECKING;
						}
						else
						{
							savegameErrorf("CSaveGameExport_BackgroundCheckForPreviousUpload::Update - failed to start the background check for a previous savegame upload");
							sm_BackgroundCheckForPreviousUpload = BACKGROUND_CHECK_FOR_PREVIOUS_UPLOAD_ERROR;
						}
					}
					else
					{
						sm_BackgroundCheckForPreviousUpload = BACKGROUND_CHECK_FOR_PREVIOUS_UPLOAD_DONE;
					}
				}
				else
				{
					sm_BackgroundCheckForPreviousUpload = BACKGROUND_CHECK_FOR_PREVIOUS_UPLOAD_DONE;
				}
			}
		}
		break;

	case BACKGROUND_CHECK_FOR_PREVIOUS_UPLOAD_CHECKING :
		{
			switch (CSaveGameExport::CheckForPreviousUpload())
			{
			case MEM_CARD_COMPLETE :
				{
					savegameDebugf1("CSaveGameExport_BackgroundCheckForPreviousUpload::Update - the background check for a previous savegame upload has completed");
					sm_BackgroundCheckForPreviousUpload = BACKGROUND_CHECK_FOR_PREVIOUS_UPLOAD_DONE;
				}
				break;

			case MEM_CARD_BUSY :
				break;

			case MEM_CARD_ERROR :
				{
					if (CSaveGameExport::GetTransferAlreadyCompleted())
					{
						savegameDebugf1("CSaveGameExport_BackgroundCheckForPreviousUpload::Update - CheckForPreviousUpload() returned MEM_CARD_ERROR, but GetTransferAlreadyCompleted() returned true so assume that migration is complete");
						sm_BackgroundCheckForPreviousUpload = BACKGROUND_CHECK_FOR_PREVIOUS_UPLOAD_DONE;
					}
					else
					{
						savegameErrorf("CSaveGameExport_BackgroundCheckForPreviousUpload::Update - the background check for a previous savegame upload has failed");
						sm_BackgroundCheckForPreviousUpload = BACKGROUND_CHECK_FOR_PREVIOUS_UPLOAD_ERROR;
					}
				}
				break;
			}
		}
		break;

	case BACKGROUND_CHECK_FOR_PREVIOUS_UPLOAD_ERROR :
		{
			const unsigned maxRetries = static_cast<unsigned>(Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("SAVE_MIGRATION_BACKGROUND_CHECK_MAX_RETRIES", 0xBA35CE3B), 3));
			const unsigned retryTimeMs = static_cast<unsigned>(Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("SAVE_MIGRATION_BACKGROUND_CHECK_RETRY_MS", 0x7DE379DD), 2500));

			if (sm_Retries < maxRetries)
			{
				const unsigned timeNow = fwTimer::GetSystemTimeInMilliseconds();
				sm_NextRetryTime = timeNow + (retryTimeMs * (1 + sm_Retries));
				++sm_Retries;

				savegameDebugf1("CSaveGameExport_BackgroundCheckForPreviousUpload::Update - waiting to retry [%u]", sm_Retries);
				sm_BackgroundCheckForPreviousUpload = BACKGROUND_CHECK_FOR_PREVIOUS_UPLOAD_WAIT_TO_RETRY_AFTER_ERROR;
			}
			else
			{
				sm_BackgroundCheckForPreviousUpload = BACKGROUND_CHECK_FOR_PREVIOUS_UPLOAD_DONE;
			}
		}
		break;

	case BACKGROUND_CHECK_FOR_PREVIOUS_UPLOAD_WAIT_TO_RETRY_AFTER_ERROR :
		{
			const unsigned timeNow = fwTimer::GetSystemTimeInMilliseconds();
			if (timeNow >= sm_NextRetryTime)
			{
				savegameDebugf1("CSaveGameExport_BackgroundCheckForPreviousUpload::Update - timer has finished so retrying now");

				sm_BackgroundCheckForPreviousUpload = BACKGROUND_CHECK_FOR_PREVIOUS_UPLOAD_READY_TO_START;
				sm_NextRetryTime = 0;
			}
		}
		break;

	case BACKGROUND_CHECK_FOR_PREVIOUS_UPLOAD_DONE :
		break;
	}
}


/////////////////////////////////////////////////////////////////////////////////////
// CSaveGameExport_DetailsOfPreviouslyUploadedSavegame
/////////////////////////////////////////////////////////////////////////////////////

CSaveGameExport_DetailsOfPreviouslyUploadedSavegame::CSaveGameExport_DetailsOfPreviouslyUploadedSavegame()
{
	Clear();
}

void CSaveGameExport_DetailsOfPreviouslyUploadedSavegame::Clear()
{
	strcpy(m_LastCompletedMissionName, "");
	strcpy(m_SaveTimeString, "");

	m_bHaveCheckedForPreviouslyUploadedSavegame = false;
	m_CompletionPercentage = 0.0f;
	m_LastCompletedMissionId = 0;
	m_SavePosixTime = 0;
}

void CSaveGameExport_DetailsOfPreviouslyUploadedSavegame::Set(bool bHaveChecked, u32 posixTime, float percentage, u32 missionNameHash)
{
	m_bHaveCheckedForPreviouslyUploadedSavegame = bHaveChecked;
	m_CompletionPercentage = percentage;
	m_LastCompletedMissionId = missionNameHash;
	m_SavePosixTime = posixTime;

	FillStringContainingLastMissionPassed();
	FillStringContainingSaveTime();
}

bool CSaveGameExport_DetailsOfPreviouslyUploadedSavegame::FillStringContainingLastMissionPassed()
{
	if (m_LastCompletedMissionId == 0)
	{
		safecpy(m_LastCompletedMissionName, "");
	}
	else
	{
//	Mission name
		safecpy(m_LastCompletedMissionName, TheText.Get(m_LastCompletedMissionId, "SP_LAST_MISSION_NAME"));

// I don't have enough information to determine if this is an autosave
// 		char *pAutosaveTxt = TheText.Get("AUTOS_PFIX");
// 		formatf_n( LastMissionName, "(%s) ", pAutosaveTxt );

//	Completion percentage
		char percentageString[12];
		formatf(percentageString, NELEM(percentageString), " (%.1f%%)", m_CompletionPercentage);
//		savegameDisplayf("CSaveGameExport_DetailsOfPreviouslyUploadedSavegame::FillStringContainingLastMissionPassed - percentageString is %s", percentageString);
		safecat( m_LastCompletedMissionName, percentageString );
	}

	return true;
}

bool CSaveGameExport_DetailsOfPreviouslyUploadedSavegame::FillStringContainingSaveTime()
{
	strcpy(m_SaveTimeString, "");

	if (m_SavePosixTime != 0)
	{
		u64 posixTime64 = m_SavePosixTime;
		time_t t = static_cast<time_t>(posixTime64);
		struct tm tInfo = *localtime(&t);		//	*gmtime(&t);

		CDate dateTime;
		dateTime.Set(tInfo.tm_hour, tInfo.tm_min, tInfo.tm_sec,
			tInfo.tm_mday, (tInfo.tm_mon + 1), (1900 + tInfo.tm_year) );

		if (!dateTime.ConstructStringFromDate(m_SaveTimeString, NELEM(m_SaveTimeString)))
		{
			savegameErrorf("CSaveGameExport_DetailsOfPreviouslyUploadedSavegame::FillStringContainingSaveTime - failed to construct string containing date/time");
			savegameAssertf(0, "CSaveGameExport_DetailsOfPreviouslyUploadedSavegame::FillStringContainingSaveTime - failed to construct string containing date/time");
			return false;
		}

		savegameDebugf1("CSaveGameExport_DetailsOfPreviouslyUploadedSavegame::FillStringContainingSaveTime - Save Game saved at %s", m_SaveTimeString);

		int LengthOfSaveGameDate = istrlen(m_SaveTimeString);
		Assertf( (LengthOfSaveGameDate == 0) || (LengthOfSaveGameDate == 17), "CSaveGameExport_DetailsOfPreviouslyUploadedSavegame::FillStringContainingSaveTime - expected string containing date to be 17 characters long");
		if (LengthOfSaveGameDate == 17)
		{
#if __REMOVE_TIME_FROM_DATE_TIME_STRINGS_IN_EXPORT_MESSAGES
			//	Remove the time from the date/time string
			if (savegameVerifyf(m_SaveTimeString[8] == ' ', "CSaveGameExport_DetailsOfPreviouslyUploadedSavegame::FillStringContainingSaveTime - expected the 9th character of the string containing the date/time ('%s') to be a space", m_SaveTimeString))
			{
				m_SaveTimeString[8] = '\0';
			}
			else
			{
				return false;
			}
#else // __REMOVE_TIME_FROM_DATE_TIME_STRINGS_IN_EXPORT_MESSAGES
			//	Remove the seconds from the time/date
			if (m_SaveTimeString[14] == ':')
			{
				m_SaveTimeString[14] = '\0';
			}
			else
			{
				savegameAssertf(0, "CSaveGameExport_DetailsOfPreviouslyUploadedSavegame::FillStringContainingSaveTime - expected a : followed by two digits at the end of the date/time string");
				return false;
			}
#endif // __REMOVE_TIME_FROM_DATE_TIME_STRINGS_IN_EXPORT_MESSAGES
		}
	}

	return true;
}

bool CSaveGameExport_DetailsOfPreviouslyUploadedSavegame::HasASavegameBeenPreviouslyUploaded()
{
	if (!m_bHaveCheckedForPreviouslyUploadedSavegame)
	{
		savegameErrorf("CSaveGameExport_DetailsOfPreviouslyUploadedSavegame::HasASavegameBeenPreviouslyUploaded - we haven't checked the server for a previous upload yet");
		savegameAssertf(0, "CSaveGameExport_DetailsOfPreviouslyUploadedSavegame::HasASavegameBeenPreviouslyUploaded - we haven't checked the server for a previous upload yet");
	}

	// I'm assuming that a non-zero value for m_SavePosixTime is enough to determine that a savegame has been previously uploaded.
	return (m_bHaveCheckedForPreviouslyUploadedSavegame && m_SavePosixTime != 0);
}


/////////////////////////////////////////////////////////////////////////////////////
// CSaveGameExport
/////////////////////////////////////////////////////////////////////////////////////

void CSaveGameExport::Init(unsigned initMode)
{
	sm_bIsExporting = false;
	sm_SaveBlockDataForExportStatus = SAVEBLOCKDATAFOREXPORT_IDLE;
	sm_SaveGameExportProgress = SAVEGAME_EXPORT_PROGRESS_READ_PSO_DATA_FROM_BUFFER;

	sm_StatusOfPreviouslyUploadedSavegameCheck = SM_STATUS_NONE;
	sm_UploadErrorCodeOfPreviouslyUploadedSavegameCheck = static_cast<s32>(RLSM_ERROR_NONE);
	sm_NetworkProblemDuringCheckForPreviousUpload = SAVEGAME_EXPORT_NETWORK_PROBLEM_NONE;
	sm_bTransferAlreadyCompleted = false;

	sm_UploadStatusOfLastUploadAttempt = SM_STATUS_NONE;
	sm_UploadErrorCodeOfLastUploadAttempt = static_cast<s32>(RLSM_ERROR_NONE);
	sm_NetworkProblemDuringUpload = SAVEGAME_EXPORT_NETWORK_PROBLEM_NONE;

	safecpy(sm_ErrorCodeAsString, "");

	CSaveGameExport_BackgroundCheckForPreviousUpload::Init(initMode);
}


void CSaveGameExport::Shutdown(unsigned shutdownMode)
{
	if (shutdownMode == SHUTDOWN_SESSION)
	{
		FreeAllData();
	}
}

void CSaveGameExport::Update()
{
	CSaveGameExport_BackgroundCheckForPreviousUpload::Update();
}


#if __BANK
void CSaveGameExport::InitWidgets(bkBank *pParentBank)
{
	pParentBank->PushGroup("Savegame Export");
	CSaveGameExport_DebugForUploadInfoMessage::InitWidgets(pParentBank);
	CSaveGameExport_DebugForPreviousUploadCheck::InitWidgets(pParentBank);
	CSaveGameExport_DebugForUpload::InitWidgets(pParentBank);
	pParentBank->PopGroup();
}
#endif // __BANK



bool CSaveGameExport::BeginExport()
{
	sm_SaveGameExportProgress = SAVEGAME_EXPORT_PROGRESS_READ_PSO_DATA_FROM_BUFFER;
	return true;
}


MemoryCardError CSaveGameExport::UpdateExport()
{
	MemoryCardError returnValue = MEM_CARD_BUSY;
	// The loaded PSO buffer will already have been allocated and filled in by the queued operation calling the savegame_load functions

	switch (sm_SaveGameExportProgress)
	{
		case SAVEGAME_EXPORT_PROGRESS_READ_PSO_DATA_FROM_BUFFER :
			{
				// Fill the code and script data from the loaded PSO data
				if (LoadBlockData())
				{
					sm_SaveGameExportProgress = SAVEGAME_EXPORT_PROGRESS_CREATE_XML_SAVE_DATA;
				}
				else
				{
					returnValue = MEM_CARD_ERROR;
				}

				// The loaded PSO buffer is not needed after this, so free it now
				sm_BufferContainingLoadedPsoData.FreeBuffer();
			}
			break;

		case SAVEGAME_EXPORT_PROGRESS_CREATE_XML_SAVE_DATA :
			{
				// Construct RT structure for XML
				// Get the size of the required save buffer for storing the XML data
				switch (CreateSaveDataAndCalculateSize())
				{
					case MEM_CARD_COMPLETE :
						sm_SaveGameExportProgress = SAVEGAME_EXPORT_PROGRESS_ALLOCATE_UNCOMPRESSED_SAVE_BUFFER;
						break;

					case MEM_CARD_BUSY :
						savegameErrorf("CSaveGameExport::UpdateExport - didn't expect CreateSaveDataAndCalculateSize() to ever return MEM_CARD_BUSY");
						returnValue = MEM_CARD_ERROR;
						break;
					case MEM_CARD_ERROR :
						savegameErrorf("CSaveGameExport::UpdateExport - CreateSaveDataAndCalculateSize() returned MEM_CARD_ERROR");
						returnValue = MEM_CARD_ERROR;
						break;
				}
			}
			break;

		case SAVEGAME_EXPORT_PROGRESS_ALLOCATE_UNCOMPRESSED_SAVE_BUFFER :
			{
				switch (sm_BufferContainingXmlDataForSaving_Uncompressed.AllocateBuffer())
				{
					case MEM_CARD_COMPLETE :
						sm_SaveGameExportProgress = SAVEGAME_EXPORT_PROGRESS_BEGIN_WRITING_TO_SAVE_BUFFER;
						break;

					case MEM_CARD_BUSY :
						savegameDebugf1("CSaveGameExport::UpdateExport - waiting for sm_BufferContainingXmlDataForSaving_Uncompressed.AllocateBuffer() to succeed");
						break;
					case MEM_CARD_ERROR :
						savegameErrorf("SaveGameExport::UpdateExport - sm_BufferContainingXmlDataForSaving_Uncompressed.AllocateBuffer() failed");
						returnValue = MEM_CARD_ERROR;
						break;
				}
			}
			break;

		case SAVEGAME_EXPORT_PROGRESS_BEGIN_WRITING_TO_SAVE_BUFFER :
			{
				if (BeginSaveBlockData())
				{
					sm_SaveGameExportProgress = SAVEGAME_EXPORT_PROGRESS_CHECK_WRITING_TO_SAVE_BUFFER;
				}
				else
				{
					savegameErrorf("SaveGameExport::UpdateExport - BeginSaveBlockData() failed");
					EndSaveBlockData();
					returnValue = MEM_CARD_ERROR;
				}
			}
			break;

		case SAVEGAME_EXPORT_PROGRESS_CHECK_WRITING_TO_SAVE_BUFFER :
			{
				switch (sm_SaveBlockDataForExportStatus)
				{
					case SAVEBLOCKDATAFOREXPORT_IDLE :
						savegameErrorf("CSaveGameExport::UpdateExport - didn't expect status to be IDLE during the writing of the save buffer");
						EndSaveBlockData();
						returnValue = MEM_CARD_ERROR;
						break;

					case SAVEBLOCKDATAFOREXPORT_PENDING :
						// Don't do anything - still waiting for the data to finish being written
						break;

					case SAVEBLOCKDATAFOREXPORT_ERROR :
						savegameErrorf("CSaveGameExport::UpdateExport - status has been set to ERROR while writing the save buffer");
						EndSaveBlockData();
						returnValue = MEM_CARD_ERROR;
						break;

					case SAVEBLOCKDATAFOREXPORT_SUCCESS :
						savegameDebugf1("CSaveGameExport::UpdateExport - writing of the save buffer has succeeded");
						EndSaveBlockData();

						FillMetadata();
#if __BANK
						bool bExportToLocalPC = false;
						if (PARAM_ExportSavegameToLocalPC.Get())
						{
							savegameDebugf1("CSaveGameExport::UpdateExport - running with -ExportSavegameToLocalPC so write savegame to X: drive");
							bExportToLocalPC = true;
						}
						else if (CSaveGameExport_DebugForUpload::GetExportToLocalPC())
						{
							savegameDebugf1("CSaveGameExport::UpdateExport - 'Export Savegame To Local PC' Rag widget is ticked so write savegame to X: drive");
							bExportToLocalPC = true;
						}

						if (bExportToLocalPC)
						{
							sm_SaveGameExportProgress = SAVEGAME_EXPORT_PROGRESS_WRITE_SAVE_BUFFER_TO_TEXT_FILE;
						}
						else
#endif // __BANK
						{
							savegameDebugf1("CSaveGameExport::UpdateExport - moving on to SAVEGAME_EXPORT_PROGRESS_BEGIN_CHECK_FOR_PREVIOUS_UPLOAD");
							sm_SaveGameExportProgress = SAVEGAME_EXPORT_PROGRESS_BEGIN_CHECK_FOR_PREVIOUS_UPLOAD;
						}
						break;
				}
			}
			break;

		case SAVEGAME_EXPORT_PROGRESS_BEGIN_CHECK_FOR_PREVIOUS_UPLOAD :
			{
#if __BANK
				bool bSkipCheck = false;
				if (PARAM_SkipCheckingForPreviouslyUploadedSavegame.Get())
				{
					savegameDebugf1("CSaveGameExport::UpdateExport - running with -SkipCheckingForPreviouslyUploadedSavegame so moving on to SAVEGAME_EXPORT_PROGRESS_BEGIN_UPLOAD");
					bSkipCheck = true;
				}
				else if (CSaveGameExport_DebugForPreviousUploadCheck::GetSkipCheckForPreviousUpload())
				{
					savegameDebugf1("CSaveGameExport::UpdateExport - 'Skip Check For Previous Upload' Rag widget is ticked so moving on to SAVEGAME_EXPORT_PROGRESS_BEGIN_UPLOAD");
					bSkipCheck = true;
				}

				if (bSkipCheck)
				{
					sm_SaveGameExportProgress = SAVEGAME_EXPORT_PROGRESS_BEGIN_UPLOAD;
				}
				else
#endif // __BANK
				{
#if __BANK
					if (CSaveGameExport_DebugForPreviousUploadCheck::GetErrorMessageOverrideEnabled())
					{
						savegameDebugf1("CSaveGameExport::UpdateExport - 'Override Error Message' Rag widget is ticked so moving on to SAVEGAME_EXPORT_PROGRESS_DISPLAY_RESULTS_OF_CHECK_FOR_PREVIOUS_UPLOAD");
						sm_SaveGameExportProgress = SAVEGAME_EXPORT_PROGRESS_DISPLAY_RESULTS_OF_CHECK_FOR_PREVIOUS_UPLOAD;
					}
					else
#endif // __BANK
					{
						if (BeginCheckForPreviousUpload())
						{
							sm_SaveGameExportProgress = SAVEGAME_EXPORT_PROGRESS_CHECK_FOR_PREVIOUS_UPLOAD;
						}
						else
						{
							savegameErrorf("SaveGameExport::UpdateExport - BeginCheckForPreviousUpload() failed");

							sm_StatusOfPreviouslyUploadedSavegameCheck = SM_STATUS_ERROR;
							sm_UploadErrorCodeOfPreviouslyUploadedSavegameCheck = static_cast<s32>(RLSM_ERROR_UNKNOWN);
							sm_bTransferAlreadyCompleted = false;

							sm_SaveGameExportProgress = SAVEGAME_EXPORT_PROGRESS_DISPLAY_RESULTS_OF_CHECK_FOR_PREVIOUS_UPLOAD;
						}
					}
				}
			}
			break;

		case SAVEGAME_EXPORT_PROGRESS_CHECK_FOR_PREVIOUS_UPLOAD :
			{
				switch (CheckForPreviousUpload())
				{
				case MEM_CARD_COMPLETE :
					savegameDebugf1("CSaveGameExport::UpdateExport - CheckForPreviousUpload() has completed");
					sm_SaveGameExportProgress = SAVEGAME_EXPORT_PROGRESS_DISPLAY_RESULTS_OF_CHECK_FOR_PREVIOUS_UPLOAD;
					break;

				case MEM_CARD_BUSY :
//					savegameDebugf1("CSaveGameExport::UpdateExport - waiting for CheckForPreviousUpload() to finish");
					break;

				case MEM_CARD_ERROR :
					savegameErrorf("SaveGameExport::UpdateExport - CheckForPreviousUpload() failed");
					sm_SaveGameExportProgress = SAVEGAME_EXPORT_PROGRESS_DISPLAY_RESULTS_OF_CHECK_FOR_PREVIOUS_UPLOAD;
					break;
				}
			}
			break;

		case SAVEGAME_EXPORT_PROGRESS_DISPLAY_RESULTS_OF_CHECK_FOR_PREVIOUS_UPLOAD :
			{
				switch (DisplayResultsOfCheckForPreviousUpload())
				{
					case MEM_CARD_COMPLETE :
						{
							savegameDebugf1("CSaveGameExport::UpdateExport - DisplayResultsOfCheckForPreviousUpload() returned MEM_CARD_COMPLETE");
							sm_SaveGameExportProgress = SAVEGAME_EXPORT_PROGRESS_BEGIN_UPLOAD;
						}
						break;

					case MEM_CARD_BUSY :
						break;

					case MEM_CARD_ERROR :
						{
							savegameErrorf("SaveGameExport::UpdateExport - DisplayResultsOfCheckForPreviousUpload() returned MEM_CARD_ERROR");
							returnValue = MEM_CARD_ERROR;
						}
						break;
				}
			}
			break;

		case SAVEGAME_EXPORT_PROGRESS_BEGIN_UPLOAD :
			{
#if __BANK
				if (CSaveGameExport_DebugForUpload::GetErrorMessageOverrideEnabled())
				{
					savegameDebugf1("CSaveGameExport::UpdateExport - 'Override Error Message' Rag widget is ticked so moving on to SAVEGAME_EXPORT_PROGRESS_DISPLAY_UPLOAD_RESULT");
					sm_SaveGameExportProgress = SAVEGAME_EXPORT_PROGRESS_DISPLAY_UPLOAD_RESULT;
				}
				else
#endif // __BANK
				{
					if (BeginUpload())
					{
						sm_SaveGameExportProgress = SAVEGAME_EXPORT_PROGRESS_CHECK_UPLOAD;
					}
					else
					{
						savegameErrorf("SaveGameExport::UpdateExport - BeginUpload() failed");

						sm_UploadStatusOfLastUploadAttempt = SM_STATUS_ERROR;
						sm_UploadErrorCodeOfLastUploadAttempt = static_cast<s32>(RLSM_ERROR_UNKNOWN);
						sm_SaveGameExportProgress = SAVEGAME_EXPORT_PROGRESS_DISPLAY_UPLOAD_RESULT;
					}
				}
			}
			break;

		case SAVEGAME_EXPORT_PROGRESS_CHECK_UPLOAD :
			{
				switch (CheckUpload())
				{
				case MEM_CARD_COMPLETE :
					savegameDebugf1("CSaveGameExport::UpdateExport - CheckUpload() has completed");
					sm_SaveGameExportProgress = SAVEGAME_EXPORT_PROGRESS_DISPLAY_UPLOAD_RESULT;
					break;

				case MEM_CARD_BUSY :
//					savegameDebugf1("CSaveGameExport::UpdateExport - waiting for CheckUpload() to finish");
					break;

				case MEM_CARD_ERROR :
					savegameErrorf("SaveGameExport::UpdateExport - CheckUpload() failed");
					sm_SaveGameExportProgress = SAVEGAME_EXPORT_PROGRESS_DISPLAY_UPLOAD_RESULT;
					break;
				}
			}
			break;

		case SAVEGAME_EXPORT_PROGRESS_DISPLAY_UPLOAD_RESULT :
			{
				if (DisplayUploadResult())
				{
					if (sm_UploadStatusOfLastUploadAttempt == SM_STATUS_OK)
					{
						returnValue = MEM_CARD_COMPLETE;
					}
					else
					{
						returnValue = MEM_CARD_ERROR;
					}
				}
			}
			break;

			// Should this case be RSG_BANK only?
		case SAVEGAME_EXPORT_PROGRESS_WRITE_SAVE_BUFFER_TO_TEXT_FILE :
			{
				// Eventually the exported data will be uploaded to the cloud
				// For now, write it to a text file on the PC so we can check its contents.
				ASSET.PushFolder(sm_XMLTextDirectoryName);

				FileHandle fileHandle = CFileMgr::OpenFileForWriting(sm_XMLTextFilename);
				if (savegameVerifyf(CFileMgr::IsValidFileHandle(fileHandle), "CSaveGameExport::UpdateExport - failed to open text file %s on PC for writing exported savegame data to", sm_XMLTextFilename))
				{
					CFileMgr::Write(fileHandle, (const char *) sm_BufferContainingXmlDataForSaving_Compressed.GetBuffer(), sm_BufferContainingXmlDataForSaving_Compressed.GetBufferSize());
					CFileMgr::CloseFile(fileHandle);
				}
				ASSET.PopFolder();

				sm_SaveGameExportProgress = SAVEGAME_EXPORT_PROGRESS_WRITE_SAVEGAME_METADATA_TO_TEXT_FILE;
			}
			break;

		case SAVEGAME_EXPORT_PROGRESS_WRITE_SAVEGAME_METADATA_TO_TEXT_FILE :
			{
				returnValue = WriteMetadataToTextFile();
			}
			break;
	}

	if ( (returnValue == MEM_CARD_COMPLETE) || (returnValue == MEM_CARD_ERROR) )
	{
		FreeAllData();
	}

	return returnValue;
}


void CSaveGameExport::FreeAllData()
{
	sm_BufferContainingLoadedPsoData.FreeBuffer();
	sm_BufferContainingXmlDataForSaving_Uncompressed.FreeBuffer();
	sm_BufferContainingXmlDataForSaving_Compressed.FreeBuffer();

	sm_SaveGameBufferContents_Code.Delete();
	sm_SaveGameBufferContents_Script.Delete();

	FreeParTreeToBeSaved();
}


void CSaveGameExport::SetBufferSizeForLoading(u32 bufferSize)
{
	sm_BufferContainingLoadedPsoData.SetBufferSize(bufferSize);
}


MemoryCardError CSaveGameExport::AllocateBufferForLoading()
{
	return sm_BufferContainingLoadedPsoData.AllocateBuffer();
}


void CSaveGameExport::FreeBufferForLoading()
{
	sm_BufferContainingLoadedPsoData.FreeBuffer();
}


u8 *CSaveGameExport::GetBufferForLoading()
{
	return sm_BufferContainingLoadedPsoData.GetBuffer();
}


u32 CSaveGameExport::GetSizeOfBufferForLoading()
{
	return sm_BufferContainingLoadedPsoData.GetBufferSize();
}

bool CSaveGameExport::GetTransferAlreadyCompleted()
{
#if __BANK
	if (CSaveGameExport_DebugForUploadInfoMessage::GetInfoMessageOverrideEnabled())
	{
		return CSaveGameExport_DebugForUploadInfoMessage::GetTransferComplete();
	}
#endif // __BANK

	return sm_bTransferAlreadyCompleted;
}

bool CSaveGameExport::GetHaveCheckedForPreviousUpload()
{
#if __BANK
	if (CSaveGameExport_DebugForUploadInfoMessage::GetInfoMessageOverrideEnabled())
	{
		return CSaveGameExport_DebugForUploadInfoMessage::GetCheckedForPreviousUpload();
	}
#endif // __BANK

	return sm_DetailsOfPreviouslyUploadedSavegame.GetHaveCheckedForPreviouslyUploadedSavegame();
}

bool CSaveGameExport::GetHasASavegameBeenPreviouslyUploaded()
{
#if __BANK
	if (CSaveGameExport_DebugForUploadInfoMessage::GetInfoMessageOverrideEnabled())
	{
		if (CSaveGameExport_DebugForUploadInfoMessage::GetCheckedForPreviousUpload())
		{
			return CSaveGameExport_DebugForUploadInfoMessage::GetHasAPreviousUpload();
		}
	}
#endif // __BANK

	return sm_DetailsOfPreviouslyUploadedSavegame.HasASavegameBeenPreviouslyUploaded();
}

const char *CSaveGameExport::GetMissionNameOfPreviousUpload()
{
#if __BANK
	if (CSaveGameExport_DebugForUploadInfoMessage::GetInfoMessageOverrideEnabled())
	{
		if (CSaveGameExport_DebugForUploadInfoMessage::GetHasAPreviousUpload())
		{
			return "Debug Mission Name";
		}
	}
#endif // __BANK

	return sm_DetailsOfPreviouslyUploadedSavegame.GetLastCompletedMissionName();
}

const char *CSaveGameExport::GetSaveTimeStringOfPreviousUpload()
{
#if __BANK
	if (CSaveGameExport_DebugForUploadInfoMessage::GetInfoMessageOverrideEnabled())
	{
		if (CSaveGameExport_DebugForUploadInfoMessage::GetHasAPreviousUpload())
		{
			return "Debug Mission Date";
		}
	}
#endif // __BANK

	return sm_DetailsOfPreviouslyUploadedSavegame.GetSaveTimeString();
}


CSaveGameExport::eNetworkProblemsForSavegameExport CSaveGameExport::CheckForNetworkProblems()
{
#if __BANK
	if (CSaveGameExport_DebugForUploadInfoMessage::GetInfoMessageOverrideEnabled())
	{
		CSaveGameExport::eNetworkProblemsForSavegameExport networkProblemOverride = CSaveGameExport_DebugForUploadInfoMessage::GetNetworkProblemOverride();
		if (networkProblemOverride != SAVEGAME_EXPORT_NETWORK_PROBLEM_NONE)
		{
			return networkProblemOverride;
		}
	}
#endif // __BANK

	if (!NetworkInterface::IsLocalPlayerSignedIn())
	{
		return SAVEGAME_EXPORT_NETWORK_PROBLEM_SIGNED_OUT_LOCALLY;
	}

	if (!netHardware::IsLinkConnected())
	{
		return SAVEGAME_EXPORT_NETWORK_PROBLEM_NO_NETWORK_CONNECTION;
	}

	if (CLiveManager::CheckIsAgeRestricted())
	{
		return SAVEGAME_EXPORT_NETWORK_PROBLEM_AGE_RESTRICTED;
	}

	if (!NetworkInterface::IsLocalPlayerOnline())
	{
		return SAVEGAME_EXPORT_NETWORK_PROBLEM_SIGNED_OUT_ONLINE;
	}

	if (!NetworkInterface::IsOnlineRos())
	{
		return SAVEGAME_EXPORT_NETWORK_PROBLEM_SOCIAL_CLUB_UNAVAILABLE;
	}

	if (!CLiveManager::GetSocialClubMgr().IsConnectedToSocialClub())
	{
		return SAVEGAME_EXPORT_NETWORK_PROBLEM_NOT_SIGNED_IN_TO_SOCIAL_CLUB;
	}

	if (!CLiveManager::GetSocialClubMgr().IsOnlinePolicyUpToDate())
	{
		return SAVEGAME_EXPORT_NETWORK_PROBLEM_SOCIAL_CLUB_POLICIES_OUT_OF_DATE;
	}

	return SAVEGAME_EXPORT_NETWORK_PROBLEM_NONE;
}


#if !__NO_OUTPUT
const char *CSaveGameExport::GetNetworkProblemName(eNetworkProblemsForSavegameExport networkProblem)
{
	switch (networkProblem)
	{
	case SAVEGAME_EXPORT_NETWORK_PROBLEM_NONE : return "NONE";
	case SAVEGAME_EXPORT_NETWORK_PROBLEM_SIGNED_OUT_LOCALLY : return "SIGNED_OUT_LOCALLY";
	case SAVEGAME_EXPORT_NETWORK_PROBLEM_NO_NETWORK_CONNECTION : return "NO_NETWORK_CONNECTION";
	case SAVEGAME_EXPORT_NETWORK_PROBLEM_AGE_RESTRICTED : return "AGE_RESTRICTED";
	case SAVEGAME_EXPORT_NETWORK_PROBLEM_SIGNED_OUT_ONLINE : return "SIGNED_OUT_ONLINE";
	case SAVEGAME_EXPORT_NETWORK_PROBLEM_SOCIAL_CLUB_UNAVAILABLE : return "SOCIAL_CLUB_UNAVAILABLE";
	case SAVEGAME_EXPORT_NETWORK_PROBLEM_NOT_SIGNED_IN_TO_SOCIAL_CLUB : return "NOT_SIGNED_IN_TO_SOCIAL_CLUB";
	case SAVEGAME_EXPORT_NETWORK_PROBLEM_SOCIAL_CLUB_POLICIES_OUT_OF_DATE : return "SOCIAL_CLUB_POLICIES_OUT_OF_DATE";
	}

	return "<unknown>";
}
#endif // !__NO_OUTPUT


bool CSaveGameExport::LoadBlockData()
{
	bool bCheckPlayerID = true;

#if __WIN32PC || RSG_DURANGO
	// Old Comment: We do not yet support user accounts/live/social club etc. so we are faking it for now.
	Assertf(!PARAM_showProfileBypass.Get(), "User Profiles is not yet supported on PC. This WILL need update once Social Club is added and the valid user check must NOT be skipped! For now we will not check player id!");

	// Old Comment: Once Social Club has been added remove the skip player check from PC and Durango versions.
	bCheckPlayerID = false;
#endif //  __WIN32PC || RSG_DURANGO

	if (bCheckPlayerID)
	{
		if (CSavegameUsers::HasPlayerJustSignedOut())
		{
			savegameDisplayf("CSaveGameExport::LoadBlockData - Player has just signed out");
			return false;
		}

		{	//	If this game data has been loaded from a storage device then check that the player has not
			//	signed out and then signed back in using a different profile
			if (!NetworkInterface::GetActiveGamerInfo())
			{
				CSavegameUsers::ClearUniqueIDOfGamerWhoStartedTheLoad();
				savegameDisplayf("CSaveGameExport::LoadBlockData - Player is signed out");
				return false;
			}
			else if (NetworkInterface::GetActiveGamerInfo()->GetGamerId() != CSavegameUsers::GetUniqueIDOfGamerWhoStartedTheLoad())
			{
				savegameDisplayf("CSaveGameExport::LoadBlockData - Player has signed out and signed in using a different profile since the load started");
				return false;
			}
		}
	}

	// decrypt the buffer
	AES aes(AES_KEY_ID_SAVEGAME);
	aes.Decrypt(sm_BufferContainingLoadedPsoData.GetBuffer(), sm_BufferContainingLoadedPsoData.GetBufferSize());

	u8* buffer     = sm_BufferContainingLoadedPsoData.GetBuffer();
	u32 bufferSize = sm_BufferContainingLoadedPsoData.GetBufferSize();

	char memFileName[RAGE_MAX_PATH];
	fiDevice::MakeMemoryFileName(memFileName, RAGE_MAX_PATH, buffer, bufferSize, false, "");

	const int inputFormat = PARSER.FindInputFormat(*(reinterpret_cast<u32*>(buffer)));
	if (inputFormat == parManager::INVALID)
	{
		savegameErrorf("CSaveGameExport::LoadBlockData - PARSER.FindInputFormat()='parManager::INVALID'. memFileName='%s'", memFileName);
		CGenericGameStorage::SetReasonForFailureToLoadSavegame(LOAD_FAILED_REASON_FILE_CORRUPT);
		return false;
	}
	else if (inputFormat == parManager::PSO) 
	{
		savegameDebugf1("CSaveGameExport::LoadBlockData - Loading PSO-formatted save file");
		psoFile* psofile = NULL;

		psoLoadFileStatus result;
		psofile = psoLoadFile(memFileName, PSOLOAD_PREP_FOR_PARSER_LOADING, atFixedBitSet32().Set(psoFile::REQUIRE_CHECKSUM), &result);

		if (psofile)
		{
			return ReadDataFromPsoFile(psofile);
		}
		else
		{
			savegameErrorf("CSaveGameExport::LoadBlockData - Couldn't load PSO save file %s", memFileName);
			CGenericGameStorage::SetReasonForFailureToLoadSavegame(LOAD_FAILED_REASON_FILE_CORRUPT);
			return false;
		}
	}
	else if (inputFormat == parManager::XML) 
	{
		savegameErrorf("CSaveGameExport::LoadBlockData - PARSER.FindInputFormat()='parManager::XML'. memFileName='%s'", memFileName);
		CGenericGameStorage::SetReasonForFailureToLoadSavegame(LOAD_FAILED_REASON_FILE_CORRUPT);
		return false;
	}
	else
	{
		savegameErrorf("CSaveGameExport::LoadBlockData - PARSER.FindInputFormat()='parManager::%d' is Not Supported. memFileName='%s'", inputFormat, memFileName);
		CGenericGameStorage::SetReasonForFailureToLoadSavegame(LOAD_FAILED_REASON_FILE_CORRUPT);
		return false;
	}
}


// ****************************** Based on CSaveGameBuffer_SinglePlayer::ReadDataFromPsoFile ****************************************************
bool CSaveGameExport::ReadDataFromPsoFile(psoFile* psofile)
{
	bool bReturnValue = true;

	sm_SaveGameBufferContents_Code.ReadDataFromPsoFile(psofile);
	if (!sm_SaveGameBufferContents_Script.ReadDataFromPsoFile(psofile))
	{
		bReturnValue = false;
	}

	delete psofile;

	return bReturnValue;
}



#define Align16(x) (((x)+15)&~15)

static sysIpcThreadId s_ExportThreadId = sysIpcThreadIdInvalid;

MemoryCardError CSaveGameExport::CreateSaveDataAndCalculateSize()
{
	parRTStructure *pRTSToSave = sm_SaveGameBufferContents_Code.ConstructRTSToSave();

	if (sm_pParTreeToBeSaved)
	{
		savegameAssertf(!PARSER.Settings().GetFlag(parSettings::USE_TEMPORARY_HEAP), "CSaveGameExport::CreateSaveDataAndCalculateSize - Graeme - I'm not dealing with AutoUseTempMemory properly");
		parManager::AutoUseTempMemory temp;

		delete sm_pParTreeToBeSaved;	//	make sure to free any memory used for a previously-allocated parTree that wasn't saved and deleted properly
	}

	fiSafeStream SafeStream(ASSET.Create("count:", ""));

	bool prevHeaderSetting = PARSER.Settings().SetFlag(parSettings::WRITE_XML_HEADER, false);
	bool prevNameSetting = PARSER.Settings().SetFlag(parSettings::ENSURE_NAMES_IN_ALL_BUILDS, true);

	{
		savegameAssertf(!PARSER.Settings().GetFlag(parSettings::USE_TEMPORARY_HEAP), "CSaveGameExport::CreateSaveDataAndCalculateSize - Graeme - I'm not dealing with AutoUseTempMemory properly");
		parManager::AutoUseTempMemory temp;

		sm_pParTreeToBeSaved = rage_new parTree;
		sm_pParTreeToBeSaved->SetRoot(PARSER.BuildTreeNodeFromStructure(*pRTSToSave, NULL));

		if (savegameVerifyf(CScriptSaveData::MainHasRegisteredScriptData(SAVEGAME_SINGLE_PLAYER), "CSaveGameExport::CreateSaveDataAndCalculateSize - expected Main script to have already registered save data before saving in xml format"))
		{
			//	Create a child tree node to contain all the save data for scripts
			parTreeNode *pRootOfTreeToBeSaved = sm_pParTreeToBeSaved->GetRoot();
			if (savegameVerifyf(pRootOfTreeToBeSaved, "CSaveGameExport::CreateSaveDataAndCalculateSize - the main tree does not have a root node"))
			{
				parTreeNode *pScriptDataNode = rage_new parTreeNode(CSaveGameData::GetNameOfMainScriptTreeNode(), true);
				if (savegameVerifyf(pScriptDataNode, "CSaveGameExport::CreateSaveDataAndCalculateSize - failed to create tree node for main script data"))
				{
					pScriptDataNode->AppendAsChildOf(pRootOfTreeToBeSaved);

					CScriptSaveData::AddActiveScriptDataToTreeToBeSaved(SAVEGAME_SINGLE_PLAYER, sm_pParTreeToBeSaved, -1);

// I don't think I need to call AddInactiveScriptDataToTreeToBeSaved
// I think "inactive script data" is an old idea that we don't use.
// Loaded script data would stay around until its associated script had started and triggered deserialization by registering its save data.
// For export, we're going to read all the PSO data and immediately write out the data as XML.
//					CScriptSaveData::AddInactiveScriptDataToTreeToBeSaved(SAVEGAME_SINGLE_PLAYER, sm_pParTreeToBeSaved, -1);
				}
			}
		}

		if (CScriptSaveData::GetNumOfScriptStructs(SAVEGAME_SINGLE_PLAYER) > 0)
		{ // If at least one DLC script has registered SP save data
			parTreeNode *pRootOfTreeToBeSaved = sm_pParTreeToBeSaved->GetRoot();
			if (savegameVerifyf(pRootOfTreeToBeSaved, "CSaveGameExport::CreateSaveDataAndCalculateSize - the main tree does not have a root node"))
			{
				// Create a node called "DLCScriptSaveData". This node will have multiple child nodes - one for each DLC script that has SP save data.
				parTreeNode *pDLCScriptDataRoot = rage_new parTreeNode(CSaveGameData::GetNameOfDLCScriptTreeNode(),true);
				if (savegameVerifyf(pDLCScriptDataRoot, "CSaveGameExport::CreateSaveDataAndCalculateSize - failed to create tree node for all DLC script data"))
				{
					pDLCScriptDataRoot->AppendAsChildOf(pRootOfTreeToBeSaved);

					for(int i = 0; i<CScriptSaveData::GetNumOfScriptStructs();++i)
					{
						if (savegameVerifyf(CScriptSaveData::DLCHasRegisteredScriptData(SAVEGAME_SINGLE_PLAYER,i), "CSaveGameExport::CreateSaveDataAndCalculateSize - expected DLC script to have already registered save data before saving in xml format"))
						{
							// Create a node for this DLC script. Add the new node as a child of the "DLCScriptSaveData" node.
							parTreeNode *pScriptDataNode = rage_new parTreeNode(CScriptSaveData::GetNameOfScriptStruct(i), true);
							if (savegameVerifyf(pScriptDataNode, "CSaveGameExport::CreateSaveDataAndCalculateSize - failed to create tree node for script data %s", CScriptSaveData::GetNameOfScriptStruct(i)))
							{
								pScriptDataNode->AppendAsChildOf(pDLCScriptDataRoot);

								CScriptSaveData::AddActiveScriptDataToTreeToBeSaved(SAVEGAME_SINGLE_PLAYER, sm_pParTreeToBeSaved, i);

// See my comment above the other AddInactiveScriptDataToTreeToBeSaved call
//								CScriptSaveData::AddInactiveScriptDataToTreeToBeSaved(SAVEGAME_SINGLE_PLAYER, sm_pParTreeToBeSaved, i);
							}
						}
					}
				} // if (savegameVerifyf(pDLCScriptDataRoot
			} // if (savegameVerifyf(pRootOfTreeToBeSaved
		} // if (CScriptSaveData::GetNumOfScriptStructs(SAVEGAME_SINGLE_PLAYER) > 0)


		if (savegameVerifyf(SafeStream, "CSaveGameExport::CreateSaveDataAndCalculateSize - failed to create stream for calculating file size"))
		{
			PARSER.SaveTree(SafeStream, sm_pParTreeToBeSaved, parManager::XML, NULL);
			SafeStream->Flush(); // don't forget this!

			sm_BufferContainingXmlDataForSaving_Uncompressed.SetBufferSize(Align16(SafeStream->Size())); // need alignment for encryption
		}
	}

	PARSER.Settings().SetFlag(parSettings::ENSURE_NAMES_IN_ALL_BUILDS, prevNameSetting);
	PARSER.Settings().SetFlag(parSettings::WRITE_XML_HEADER, prevHeaderSetting);

	return MEM_CARD_COMPLETE;
}


void CSaveGameExport::SaveBlockDataForExportThreadFunc(void* )
{
	PF_PUSH_MARKER("SaveBlockDataForExport");

	bool bSuccess = DoSaveBlockData();
	if(bSuccess)
	{
		sm_SaveBlockDataForExportStatus = SAVEBLOCKDATAFOREXPORT_SUCCESS;
	}
	else
	{
		sm_SaveBlockDataForExportStatus = SAVEBLOCKDATAFOREXPORT_ERROR;
	}

#if !__FINAL
	unsigned current, maxi;
	if (sysIpcEstimateStackUsage(current,maxi))
		savegameDisplayf("Save game thread used %u of %u bytes stack space.",current,maxi);
#endif

	PF_POP_MARKER();
}


bool CSaveGameExport::BeginSaveBlockData()
{
	if (s_ExportThreadId != sysIpcThreadIdInvalid) 
	{
		Errorf("BeginSaveBlockData started twice. Previous thread not finished, can't start.");
		return false;
	}

	FatalAssertf(sm_SaveBlockDataForExportStatus == SAVEBLOCKDATAFOREXPORT_IDLE, "CSaveGameExport::BeginSaveBlockData called twice");
	sm_SaveBlockDataForExportStatus = SAVEBLOCKDATAFOREXPORT_PENDING;
//	m_UseEncryptionForSaveBlockData = useEncryption;

	s_ExportThreadId = sysIpcCreateThread(SaveBlockDataForExportThreadFunc, NULL, 16384, PRIO_BELOW_NORMAL, "SaveBlockDataThread", 0, "SaveBlockDataThread");
	return s_ExportThreadId != sysIpcThreadIdInvalid;
}


bool CSaveGameExport::DoSaveBlockData()
{
	if (sm_pParTreeToBeSaved)
	{
		char memFileName[RAGE_MAX_PATH];
		fiDevice::MakeMemoryFileName(memFileName, RAGE_MAX_PATH, sm_BufferContainingXmlDataForSaving_Uncompressed.GetBuffer(), sm_BufferContainingXmlDataForSaving_Uncompressed.GetBufferSize(), false, "");
		fiSafeStream SafeStream(ASSET.Create(memFileName, ""));

		bool prevSetting = PARSER.Settings().SetFlag(parSettings::WRITE_XML_HEADER, false);

		{
			if (savegameVerifyf(SafeStream, "CSaveGameExport::DoSaveBlockData - failed to create stream for saving the file"))
			{
				PARSER.SaveTree(SafeStream, sm_pParTreeToBeSaved, parManager::XML, NULL);
				SafeStream->Flush(); // Make sure everything's copied to the buffer before encryption
			}

		}

		PARSER.Settings().SetFlag(parSettings::WRITE_XML_HEADER, prevSetting);

		savegameDebugf2("CSaveGameExport::DoSaveBlockData - Done saving to stream '%s'", SafeStream->GetName());
	}


	//Compress data
	savegameDisplayf("CSaveGameExport::DoSaveBlockData - size before compression = %u",  sm_BufferContainingXmlDataForSaving_Uncompressed.GetBufferSize());
	sm_SaveGameMetadata.SetUncompressedSize(sm_BufferContainingXmlDataForSaving_Uncompressed.GetBufferSize());
	if(!CompressXmlDataForSaving())
	{
		// Compression FAILED
		return false;
	}
	savegameDisplayf("CSaveGameExport::DoSaveBlockData - size after compression = %u", sm_BufferContainingXmlDataForSaving_Compressed.GetBufferSize());
	sm_SaveGameMetadata.SetCompressedSize(sm_BufferContainingXmlDataForSaving_Compressed.GetBufferSize());

	// Encrypt the buffer
	AES aes(AES_KEY_ID_GTA5SAVE_MIGRATION);
	aes.Encrypt(sm_BufferContainingXmlDataForSaving_Compressed.GetBuffer(), sm_BufferContainingXmlDataForSaving_Compressed.GetBufferSize());

	savegameDisplayf("CSaveGameExport::DoSaveBlockData - size after encryption = %u", sm_BufferContainingXmlDataForSaving_Compressed.GetBufferSize());

	return true;
}


bool CSaveGameExport::CompressXmlDataForSaving()
{
	const char *pHeaderId = "COMP";
	const u32 sizeOfHeaderId = 4;
	const u32 sizeOfHeaderFieldContainingUncompressedSize = 4;
	const u32 sizeOfHeader = (sizeOfHeaderId + sizeOfHeaderFieldContainingUncompressedSize);

	// Make sure we have enough space for worst case scenario + 8 byte header
	u32 destSize = sizeOfHeader + LZ4_compressBound(sm_BufferContainingXmlDataForSaving_Uncompressed.GetBufferSize());

	sm_BufferContainingXmlDataForSaving_Compressed.SetBufferSize(Align16(destSize)); // need alignment for encryption
	if (sm_BufferContainingXmlDataForSaving_Compressed.AllocateBuffer() != MEM_CARD_COMPLETE)
	{
		savegameErrorf("CSaveGameExport::CompressXmlDataForSaving - failed to allocate memory for compressed buffer");
		savegameAssertf(0, "CSaveGameExport::CompressXmlDataForSaving - failed to allocate memory for compressed buffer");
		return false;
	}

	u8 *pCompressedBuffer = sm_BufferContainingXmlDataForSaving_Compressed.GetBuffer();

	int compressedSize = LZ4_compress(reinterpret_cast<const char*>(sm_BufferContainingXmlDataForSaving_Uncompressed.GetBuffer()), 
		reinterpret_cast<char*>(pCompressedBuffer + sizeOfHeader), 
		sm_BufferContainingXmlDataForSaving_Uncompressed.GetBufferSize());

	if (compressedSize > 0)
	{
		memcpy(pCompressedBuffer, pHeaderId, sizeOfHeaderId);

		// Write the size of the uncompressed data to the header
		u32 sourceSizeL = sysEndian::NtoL(sm_BufferContainingXmlDataForSaving_Uncompressed.GetBufferSize());
		memcpy(pCompressedBuffer + sizeOfHeaderId, &sourceSizeL, sizeOfHeaderFieldContainingUncompressedSize);

		sm_BufferContainingXmlDataForSaving_Compressed.SetBufferSize(Align16(sizeOfHeader + compressedSize)); // need alignment for encryption
	}
	else
	{
		savegameErrorf("CSaveGameExport::CompressXmlDataForSaving - compression has failed");
		savegameAssertf(0, "CSaveGameExport::CompressXmlDataForSaving - compression has failed");
		return false;
	}

	return true;
}


void CSaveGameExport::EndSaveBlockData()
{
	if (s_ExportThreadId != sysIpcThreadIdInvalid)
	{
		sysIpcWaitThreadExit(s_ExportThreadId);
		s_ExportThreadId = sysIpcThreadIdInvalid;
	}

	sm_SaveBlockDataForExportStatus = SAVEBLOCKDATAFOREXPORT_IDLE;
}


bool CSaveGameExport::BeginCheckForPreviousUpload()
{
	sm_NetworkProblemDuringCheckForPreviousUpload = CheckForNetworkProblems();
	if (sm_NetworkProblemDuringCheckForPreviousUpload != SAVEGAME_EXPORT_NETWORK_PROBLEM_NONE)
	{
		return false;
	}

	return CSaveMigration::GetSingleplayer().RequestSinglePlayerSaveState();
}

MemoryCardError CSaveGameExport::CheckForPreviousUpload()
{
	MemoryCardError returnValue = MEM_CARD_BUSY;

	sm_UploadErrorCodeOfPreviouslyUploadedSavegameCheck = static_cast<s32>(RLSM_ERROR_NONE);
	sm_NetworkProblemDuringCheckForPreviousUpload = SAVEGAME_EXPORT_NETWORK_PROBLEM_NONE;
	sm_bTransferAlreadyCompleted = false;

	sm_StatusOfPreviouslyUploadedSavegameCheck = CSaveMigration::GetSingleplayer().GetSinglePlayerSaveStateStatus();
	switch (sm_StatusOfPreviouslyUploadedSavegameCheck)
	{
	case SM_STATUS_NONE :
		{
			savegameErrorf("CSaveGameExport::CheckForPreviousUpload - didn't expect GetSinglePlayerSaveStateStatus() to return SM_STATUS_NONE");
			savegameAssertf(0, "CSaveGameExport::CheckForPreviousUpload - didn't expect GetSinglePlayerSaveStateStatus() to return SM_STATUS_NONE");
			returnValue = MEM_CARD_ERROR;
		}
		break;

	case SM_STATUS_OK :
		{
			savegameDisplayf("CSaveGameExport::CheckForPreviousUpload - GetSinglePlayerSaveStateStatus() returned SM_STATUS_OK");

			if (CSaveMigration::GetSingleplayer().GetSinglePlayerSaveStateResult() == (s32)RLSM_ERROR_NO_MIGRATION_RECORDS)
			{
				savegameDisplayf("CSaveGameExport::CheckForPreviousUpload - GetSinglePlayerSaveStateResult() returned RLSM_ERROR_NO_MIGRATION_RECORDS");

				sm_DetailsOfPreviouslyUploadedSavegame.Set(true, 0, 0.0f, 0);
			}
			else
			{
				rlSaveMigrationRecordMetadata recordMetadata = CSaveMigration::GetSingleplayer().GetSinglePlayerSaveStateDetails();

				sm_DetailsOfPreviouslyUploadedSavegame.Set(true, recordMetadata.m_SavePosixTime, 
					recordMetadata.m_CompletionPercentage,
					recordMetadata.m_LastCompletedMissionId);
			}

			returnValue = MEM_CARD_COMPLETE;
		}
		break;

	case SM_STATUS_PENDING :
		{
			savegameDisplayf("CSaveGameExport::CheckForPreviousUpload - GetSinglePlayerSaveStateStatus() returned SM_STATUS_PENDING");
		}
		break;

	case SM_STATUS_INPROGRESS :
		{
			savegameDisplayf("CSaveGameExport::CheckForPreviousUpload - GetSinglePlayerSaveStateStatus() returned SM_STATUS_INPROGRESS");
		}
		break;

	case SM_STATUS_CANCELED :
		{
			savegameDisplayf("CSaveGameExport::CheckForPreviousUpload - GetSinglePlayerSaveStateStatus() returned SM_STATUS_CANCELED");
			returnValue = MEM_CARD_ERROR;
		}
		break;

	case SM_STATUS_ROLLEDBACK :
		{
			savegameDisplayf("CSaveGameExport::CheckForPreviousUpload - GetSinglePlayerSaveStateStatus() returned SM_STATUS_ROLLEDBACK");
			returnValue = MEM_CARD_ERROR;
		}
		break;

	case SM_STATUS_ERROR :
		{
			sm_NetworkProblemDuringCheckForPreviousUpload = CheckForNetworkProblems();

			sm_UploadErrorCodeOfPreviouslyUploadedSavegameCheck = CSaveMigration::GetSingleplayer().GetSinglePlayerSaveStateResult();
			if (sm_UploadErrorCodeOfPreviouslyUploadedSavegameCheck == (s32) RLSM_ERROR_ALREADY_COMPLETE)
			{
				sm_bTransferAlreadyCompleted = true;
			}
			savegameDisplayf("CSaveGameExport::CheckForPreviousUpload - GetSinglePlayerSaveStateStatus() returned SM_STATUS_ERROR. sm_UploadErrorCodeOfPreviouslyUploadedSavegameCheck=%d. Network Problem=%s", 
				sm_UploadErrorCodeOfPreviouslyUploadedSavegameCheck, GetNetworkProblemName(sm_NetworkProblemDuringCheckForPreviousUpload));
			returnValue = MEM_CARD_ERROR;
		}
		break;
	}

	return returnValue;
}

#if __NO_OUTPUT
#define SAVEGAME_EXPORT_CHECK_TEXT_LABEL_EXISTS(textLabel)
#else
#define SAVEGAME_EXPORT_CHECK_TEXT_LABEL_EXISTS(textLabel) \
if (!TheText.DoesTextLabelExist(textLabel))\
{\
	savegameErrorf("Can't find Key %s in the text file", textLabel);\
	savegameAssertf(0, "Can't find Key %s in the text file", textLabel);\
}
#endif


MemoryCardError CSaveGameExport::DisplayResultsOfCheckForPreviousUpload()
{
	bool bCheckHasSucceeded = (sm_StatusOfPreviouslyUploadedSavegameCheck == SM_STATUS_OK);
#if __BANK
	if (CSaveGameExport_DebugForPreviousUploadCheck::GetErrorMessageOverrideEnabled())
	{
		bCheckHasSucceeded = !CSaveGameExport_DebugForPreviousUploadCheck::GetHadError();
	}
#endif // __BANK

	if (bCheckHasSucceeded)
	{ // Check if a savegame has been upload previously (but not yet migrated)
		bool bHaveCheckedForPreviousUpload = sm_DetailsOfPreviouslyUploadedSavegame.GetHaveCheckedForPreviouslyUploadedSavegame();
#if __BANK
		if (CSaveGameExport_DebugForPreviousUploadCheck::GetErrorMessageOverrideEnabled())
		{
			bHaveCheckedForPreviousUpload = CSaveGameExport_DebugForPreviousUploadCheck::GetHasPreviousSave();
		}
#endif // __BANK

		if (bHaveCheckedForPreviousUpload)
		{
			bool bASavegameHasBeenUploaded = false;
			const char *pLastCompletedMissionName = NULL;
			const char *pSaveTimeString = NULL;

#if __BANK
			if (CSaveGameExport_DebugForPreviousUploadCheck::GetErrorMessageOverrideEnabled())
			{
				if (CSaveGameExport_DebugForPreviousUploadCheck::GetHasPreviousSave())
				{
					bASavegameHasBeenUploaded = true;
					pLastCompletedMissionName = "DebugMissionName";
					pSaveTimeString = "DebugSaveTime";
				}
			}
			else
#endif // __BANK
			{
				bASavegameHasBeenUploaded = sm_DetailsOfPreviouslyUploadedSavegame.HasASavegameBeenPreviouslyUploaded();
				pLastCompletedMissionName = sm_DetailsOfPreviouslyUploadedSavegame.GetLastCompletedMissionName();
				pSaveTimeString = sm_DetailsOfPreviouslyUploadedSavegame.GetSaveTimeString();
			}

			if (bASavegameHasBeenUploaded)
			{ // Display details of previous upload - allow player to choose whether they want to overwrite it

				SAVEGAME_EXPORT_CHECK_TEXT_LABEL_EXISTS("SG_PROC_WARN1")
				CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD,
					"PM_PANE_PROC", "SG_PROC_WARN1",FE_WARNING_YES_NO,
					false, -1,
					pLastCompletedMissionName, pSaveTimeString);

				eWarningButtonFlags result = CWarningScreen::CheckAllInput(false);

				if (result == FE_WARNING_YES)
				{ // The player does want to overwrite the previously uploaded savegame
					return MEM_CARD_COMPLETE;
				}

				if (result == FE_WARNING_NO)
				{ // The player doesn't want to overwrite the previously uploaded savegame
					return MEM_CARD_ERROR;
				}
			}
			else
			{ // No previous upload - I don't need to display any message
				return MEM_CARD_COMPLETE;
			}
		}
		else
		{
			// Display an error message if we haven't checked for a previously uploaded savegame
			SAVEGAME_EXPORT_CHECK_TEXT_LABEL_EXISTS("SG_PROC_WARN2")
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, "PM_PANE_PROC", "SG_PROC_WARN2", FE_WARNING_OK);

			eWarningButtonFlags result = CWarningScreen::CheckAllInput(false);

			if( result == FE_WARNING_OK )
			{
				return MEM_CARD_ERROR;
			}
		}
	}
	else
	{
		if (sm_NetworkProblemDuringCheckForPreviousUpload != SAVEGAME_EXPORT_NETWORK_PROBLEM_NONE)
		{ // If we've had a network problem (like player has signed out) then display that instead of the savegame migration RLSM_ERROR
			const char *pNetworkErrorMessage = GetNetworkErrorMessageText(sm_NetworkProblemDuringCheckForPreviousUpload);

			SAVEGAME_EXPORT_CHECK_TEXT_LABEL_EXISTS("SG_PROC_WARN4")
				CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, "PM_PANE_PROC", "SG_PROC_WARN4", FE_WARNING_OK,
				false, -1, pNetworkErrorMessage, NULL, true, false, (s32) sm_NetworkProblemDuringCheckForPreviousUpload);
		}
		else
		{
			s32 errorCode = sm_UploadErrorCodeOfPreviouslyUploadedSavegameCheck;
#if __BANK
			if (CSaveGameExport_DebugForPreviousUploadCheck::GetErrorMessageOverrideEnabled())
			{
				if (CSaveGameExport_DebugForPreviousUploadCheck::GetHadError())
				{
					errorCode = (s32) CSaveGameExport_DebugForPreviousUploadCheck::GetErrorCode();
				}
			}
#endif // __BANK

			if (errorCode == (s32) RLSM_ERROR_ALREADY_COMPLETE)
			{ // The player has already transferred a savegame to their gen9 platform.
				SAVEGAME_EXPORT_CHECK_TEXT_LABEL_EXISTS("SG_PROC_WARN3")
				CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, "PM_PANE_PROC", "SG_PROC_WARN3", FE_WARNING_OK);
			}
			else
			{
				const char *pErrorMessage = GetErrorMessageText(errorCode);

				SAVEGAME_EXPORT_CHECK_TEXT_LABEL_EXISTS("SG_PROC_WARN4")
				CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, "PM_PANE_PROC", "SG_PROC_WARN4", FE_WARNING_OK,
					false, -1, pErrorMessage, NULL, true, false, errorCode);
			}
		}

		eWarningButtonFlags result = CWarningScreen::CheckAllInput(false);

		if( result == FE_WARNING_OK )
		{
			return MEM_CARD_ERROR;
		}
	}

	return MEM_CARD_BUSY;
}

bool CSaveGameExport::BeginUpload()
{
	sm_NetworkProblemDuringUpload = CheckForNetworkProblems();
	if (sm_NetworkProblemDuringUpload != SAVEGAME_EXPORT_NETWORK_PROBLEM_NONE)
	{
		return false;
	}

	return CSaveMigration::GetSingleplayer().RequestUploadSingleplayerSave(sm_SaveGameMetadata.GetSavePosixTime(), 
		sm_SaveGameMetadata.GetCompletionPercentage(), 
		sm_SaveGameMetadata.GetCompletedMissionTextKeyHash(), 
		sm_BufferContainingXmlDataForSaving_Compressed.GetBuffer(), sm_BufferContainingXmlDataForSaving_Compressed.GetBufferSize());
}

MemoryCardError CSaveGameExport::CheckUpload()
{
	MemoryCardError returnValue = MEM_CARD_BUSY;

	sm_UploadErrorCodeOfLastUploadAttempt = static_cast<s32>(RLSM_ERROR_NONE);
	sm_NetworkProblemDuringUpload = SAVEGAME_EXPORT_NETWORK_PROBLEM_NONE;
	sm_UploadStatusOfLastUploadAttempt = CSaveMigration::GetSingleplayer().GetUploadSingleplayerSaveStatus();
	switch (sm_UploadStatusOfLastUploadAttempt)
	{
		case SM_STATUS_NONE :
			{
				savegameErrorf("CSaveGameExport::CheckUpload - didn't expect GetUploadSingleplayerSaveStatus() to return SM_STATUS_NONE");
				savegameAssertf(0, "CSaveGameExport::CheckUpload - didn't expect GetUploadSingleplayerSaveStatus() to return SM_STATUS_NONE");
				returnValue = MEM_CARD_ERROR;
			}
			break;

		case SM_STATUS_OK :
			{
				savegameDisplayf("CSaveGameExport::CheckUpload - GetUploadSingleplayerSaveStatus() returned SM_STATUS_OK");
				// if the upload succeeds, update sm_DetailsOfPreviouslyUploadedSavegame
				sm_DetailsOfPreviouslyUploadedSavegame.Set(true, sm_SaveGameMetadata.GetSavePosixTime(), 
					sm_SaveGameMetadata.GetCompletionPercentage(), 
					sm_SaveGameMetadata.GetCompletedMissionTextKeyHash());
				returnValue = MEM_CARD_COMPLETE;
			}
			break;

		case SM_STATUS_PENDING :
			{
				savegameDisplayf("CSaveGameExport::CheckUpload - GetUploadSingleplayerSaveStatus() returned SM_STATUS_PENDING");
			}
			break;

		case SM_STATUS_INPROGRESS :
			{
				savegameDisplayf("CSaveGameExport::CheckUpload - GetUploadSingleplayerSaveStatus() returned SM_STATUS_INPROGRESS");
			}
			break;

		case SM_STATUS_CANCELED :
			{
				savegameDisplayf("CSaveGameExport::CheckUpload - GetUploadSingleplayerSaveStatus() returned SM_STATUS_CANCELED");
				returnValue = MEM_CARD_ERROR;
			}
			break;

		case SM_STATUS_ROLLEDBACK :
			{
				savegameDisplayf("CSaveGameExport::CheckUpload - GetUploadSingleplayerSaveStatus() returned SM_STATUS_ROLLEDBACK");
				returnValue = MEM_CARD_ERROR;
			}
			break;

		case SM_STATUS_ERROR :
			{
				sm_NetworkProblemDuringUpload = CheckForNetworkProblems();
				sm_UploadErrorCodeOfLastUploadAttempt = CSaveMigration::GetSingleplayer().GetUploadSingleplayerSaveResult();
				savegameDisplayf("CSaveGameExport::CheckUpload - GetUploadSingleplayerSaveStatus() returned SM_STATUS_ERROR. sm_UploadErrorCodeOfLastUploadAttempt=%d sm_NetworkProblemDuringUpload=%s", 
					sm_UploadErrorCodeOfLastUploadAttempt, GetNetworkProblemName(sm_NetworkProblemDuringUpload));
				returnValue = MEM_CARD_ERROR;
			}
			break;
	}
	
	return returnValue;
}


bool CSaveGameExport::DisplayUploadResult()
{
	bool bUploadHasSucceeded = (sm_UploadStatusOfLastUploadAttempt == SM_STATUS_OK);
#if __BANK
	if (CSaveGameExport_DebugForUpload::GetErrorMessageOverrideEnabled())
	{
		bUploadHasSucceeded = !CSaveGameExport_DebugForUpload::GetHadError();
	}
#endif // __BANK

	if (bUploadHasSucceeded)
	{
		SAVEGAME_EXPORT_CHECK_TEXT_LABEL_EXISTS("SG_PROC_SUCCESS")
		CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, "PM_PANE_PROC", "SG_PROC_SUCCESS", FE_WARNING_OK);
	}
	else
	{
		if (sm_NetworkProblemDuringUpload != SAVEGAME_EXPORT_NETWORK_PROBLEM_NONE)
		{ // If we've had a network problem (like player has signed out) then display that instead of the savegame migration RLSM_ERROR
			const char *pNetworkErrorMessage = GetNetworkErrorMessageText(sm_NetworkProblemDuringUpload);

			SAVEGAME_EXPORT_CHECK_TEXT_LABEL_EXISTS("SG_PROC_FAIL")
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, "PM_PANE_PROC", "SG_PROC_FAIL", FE_WARNING_OK,
				false, -1, pNetworkErrorMessage, NULL, true, false, (s32) sm_NetworkProblemDuringUpload);
		}
		else
		{
			s32 errorCode = sm_UploadErrorCodeOfLastUploadAttempt;

#if __BANK
			if (CSaveGameExport_DebugForUpload::GetErrorMessageOverrideEnabled())
			{
				if (CSaveGameExport_DebugForUpload::GetHadError())
				{
					errorCode = (s32) CSaveGameExport_DebugForUpload::GetErrorCode();
				}
			}
#endif // __BANK

			const char *pErrorMessage = GetErrorMessageText(errorCode);

			SAVEGAME_EXPORT_CHECK_TEXT_LABEL_EXISTS("SG_PROC_FAIL")
				CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, "PM_PANE_PROC", "SG_PROC_FAIL", FE_WARNING_OK,
				false, -1, pErrorMessage, NULL, true, false, errorCode);
		}
	}

	eWarningButtonFlags result = CWarningScreen::CheckAllInput(false);

	if( result == FE_WARNING_OK )
	{
		return true;
	}

	return false;
}

void CSaveGameExport::FillMetadata()
{
	s32 hashOfLastMissionPassed = sm_SaveGameBufferContents_Code.GetHashOfNameOfLastMissionPassed();
	u32 hashOfLastMissionPassed_U32 = static_cast<u32>(hashOfLastMissionPassed);
	sm_SaveGameMetadata.SetCompletedMissionTextKeyHash(hashOfLastMissionPassed_U32);

	float fPercentageComplete = sm_SaveGameBufferContents_Code.GetSinglePlayerCompletionPercentage();
	sm_SaveGameMetadata.SetCompletionPercentage(fPercentageComplete);

	s32 savegameSlot = CSavegameFrontEnd::GetSaveGameSlotToExport();
	CDate savedDate;
	savedDate = CSavegameList::GetSaveTime(savegameSlot);
	sm_SaveGameMetadata.SetSavePosixTime(savedDate);

	char saveGameName[SAVE_GAME_MAX_DISPLAY_NAME_LENGTH];
	safecpy(saveGameName, CSavegameList::GetNameOfSavedGameForMenu(savegameSlot));
	sm_SaveGameMetadata.SetCompletedMissionLocalizedText(saveGameName);
}

MemoryCardError CSaveGameExport::WriteMetadataToTextFile()
{
	if (sm_SaveGameMetadata.SaveToLocalFile())
	{
		return MEM_CARD_COMPLETE;
	}

	return MEM_CARD_ERROR;
}


void CSaveGameExport::FreeParTreeToBeSaved()
{
	if (sm_pParTreeToBeSaved)
	{
		savegameAssertf(!PARSER.Settings().GetFlag(parSettings::USE_TEMPORARY_HEAP), "CSaveGameExport::FreeParTreeToBeSaved - Graeme - I'm not dealing with AutoUseTempMemory properly");
		parManager::AutoUseTempMemory temp;

		delete sm_pParTreeToBeSaved;
		sm_pParTreeToBeSaved = NULL;
	}
}


const char *CSaveGameExport::GetErrorMessageText(s32 errorCode)
{
	if ( (errorCode >= (s32) RLSM_ERROR_UNKNOWN) && (errorCode < (s32) RLSM_ERROR_END) )
	{
		rage::rlSaveMigrationErrorCode rlsmErrorCode = static_cast<rage::rlSaveMigrationErrorCode>(errorCode);

		u32 hashOfTextKey = 0;

		switch (rlsmErrorCode)
		{
			case RLSM_ERROR_UNKNOWN : 
			case RLSM_ERROR_NONE : 
			case RLSM_ERROR_END : 
				hashOfTextKey = ATSTRINGHASH("SG_PROC_ERROR_UNKN", 0x22463D84);
				break;

			case RLSM_ERROR_AUTHENTICATION_FAILED : 
				hashOfTextKey = ATSTRINGHASH("SG_PROC_ERROR_AUTH", 0xBD7B781);
				break;

			case RLSM_ERROR_MAINTENANCE : 
				hashOfTextKey = ATSTRINGHASH("SG_PROC_ERROR_MAINT", 0x587205B6);
				break;

			case RLSM_ERROR_DOES_NOT_MATCH : 
				hashOfTextKey = ATSTRINGHASH("SG_PROC_ERROR_NO_MATCH", 0xCB2BF342);
				break;

			case RLSM_ERROR_ROCKSTAR_ACCOUNT_REQUIRED : 
				hashOfTextKey = ATSTRINGHASH("SG_PROC_ERROR_ACC", 0x64B96FAA);
				break;

			case RLSM_ERROR_INVALID_TITLE : 
				hashOfTextKey = ATSTRINGHASH("SG_PROC_ERROR_INV_TITL", 0x2C9C0C3);
				break;

			case RLSM_ERROR_DISABLED : 
				hashOfTextKey = ATSTRINGHASH("SG_PROC_ERROR_DISABLED", 0x653905F8);
				break;

			case RLSM_ERROR_IN_PROGRESS : 
				hashOfTextKey = ATSTRINGHASH("SG_PROC_ERROR_IN_PROG", 0xDF5624DC);
				break;

			case RLSM_ERROR_ALREADY_COMPLETE : 
				hashOfTextKey = ATSTRINGHASH("SG_PROC_ERROR_COMP", 0x3B996AC8);
				break;

			case RLSM_ERROR_INVALID_FILEDATA : 
				hashOfTextKey = ATSTRINGHASH("SG_PROC_ERROR_INV_FILE", 0xE1FCD008);
				break;

			case RLSM_ERROR_INVALID_PARAMETER : 
				hashOfTextKey = ATSTRINGHASH("SG_PROC_ERROR_INV_PARAM", 0xE2151503);
				break;

			case RLSM_ERROR_FILESTORAGE_UNAVAILABLE : 
				hashOfTextKey = ATSTRINGHASH("SG_PROC_ERROR_STOR_UNAV", 0x16D7A6FB);
				break;

			case RLSM_ERROR_INVALID_TOKEN : 
				hashOfTextKey = ATSTRINGHASH("SG_PROC_ERROR_INV_TOK", 0x4BBEB8A8);
				break;

			case RLSM_ERROR_EXPIRED_TOKEN : 
				hashOfTextKey = ATSTRINGHASH("SG_PROC_ERROR_EXP_TOK", 0x276F9635);
				break;

			case RLSM_ERROR_EXPIRED_SAVE_MIGRATION : 
				hashOfTextKey = ATSTRINGHASH("SG_PROC_ERROR_EXP_MIG", 0x9E8C7114);
				break;

			case RLSM_ERROR_NOT_IN_PROGRESS : 
				hashOfTextKey = ATSTRINGHASH("SG_PROC_ERROR_NOT_PROG", 0xAF129AFA);
				break;

			case RLSM_ERROR_NO_MIGRATION_RECORDS : 
				hashOfTextKey = ATSTRINGHASH("SG_PROC_ERROR_NO_REC", 0x3C7C0A30);
				break;
		}

		if (!TheText.DoesTextLabelExist(hashOfTextKey))
		{
			savegameErrorf("CSaveGameExport::GetErrorMessageText - Can't find Key 0x%x in the text file. errorCode=%d", hashOfTextKey, errorCode);
			savegameAssertf(0, "CSaveGameExport::GetErrorMessageText - Can't find Key 0x%x in the text file. errorCode=%d", hashOfTextKey, errorCode);
		}

		const char *pErrorMessage = TheText.Get(hashOfTextKey, "");
		if (!pErrorMessage)
		{
			savegameErrorf("CSaveGameExport::GetErrorMessageText - Can't find Key 0x%x in the text file. errorCode=%d", hashOfTextKey, errorCode);
			savegameAssertf(pErrorMessage, "CSaveGameExport::GetErrorMessageText - Can't find Key 0x%x in the text file. errorCode=%d", hashOfTextKey, errorCode);
		}
		return pErrorMessage;
	}

	// If the errorCode doesn't correspond with an entry in the rlSaveMigrationErrorCode enum
	//	then it's probably an HTTP error (see the netHttpStatusCodes enum in http.h).
	// We'll just display the integer as a string.
	formatf(sm_ErrorCodeAsString, "%d", errorCode);
	return sm_ErrorCodeAsString;
}


const char *CSaveGameExport::GetNetworkErrorMessageText(eNetworkProblemsForSavegameExport networkProblem)
{
	u32 hashOfTextKey = 0;

	switch (networkProblem)
	{
		case SAVEGAME_EXPORT_NETWORK_PROBLEM_NONE :
			// Can't reach this state
			break;

		case SAVEGAME_EXPORT_NETWORK_PROBLEM_SIGNED_OUT_LOCALLY :
			hashOfTextKey = ATSTRINGHASH("SG_PROC_NET_1", 0x1AF4BAA2);
			break;

		case SAVEGAME_EXPORT_NETWORK_PROBLEM_NO_NETWORK_CONNECTION :
			hashOfTextKey = ATSTRINGHASH("SG_PROC_NET_2", 0x29850C);
			break;

		case SAVEGAME_EXPORT_NETWORK_PROBLEM_AGE_RESTRICTED :
			hashOfTextKey = ATSTRINGHASH("SG_PROC_NET_3", 0x2DE1607B);
			break;

		case SAVEGAME_EXPORT_NETWORK_PROBLEM_SIGNED_OUT_ONLINE :
			hashOfTextKey = ATSTRINGHASH("SG_PROC_NET_4", 0x240F4CD7);
			break;

		case SAVEGAME_EXPORT_NETWORK_PROBLEM_SOCIAL_CLUB_UNAVAILABLE :
			hashOfTextKey = ATSTRINGHASH("SG_PROC_NET_5", 0xD7F734A8);
			break;

		case SAVEGAME_EXPORT_NETWORK_PROBLEM_NOT_SIGNED_IN_TO_SOCIAL_CLUB :
			hashOfTextKey = ATSTRINGHASH("SG_PROC_NET_6", 0xC5C59045);
			break;

		case SAVEGAME_EXPORT_NETWORK_PROBLEM_SOCIAL_CLUB_POLICIES_OUT_OF_DATE :
			hashOfTextKey = ATSTRINGHASH("SG_PROC_NET_7", 0xFC9CFDF3);
			break;
	}

	if (!TheText.DoesTextLabelExist(hashOfTextKey))
	{
		savegameErrorf("CSaveGameExport::GetNetworkErrorMessageText - Can't find Key 0x%x in the text file. networkProblem=%s", hashOfTextKey, GetNetworkProblemName(networkProblem));
		savegameAssertf(0, "CSaveGameExport::GetNetworkErrorMessageText - Can't find Key 0x%x in the text file. networkProblem=%s", hashOfTextKey, GetNetworkProblemName(networkProblem));
	}

	const char *pErrorMessage = TheText.Get(hashOfTextKey, "");
	if (!pErrorMessage)
	{
		savegameErrorf("CSaveGameExport::GetNetworkErrorMessageText - Can't find Key 0x%x in the text file. networkProblem=%s", hashOfTextKey, GetNetworkProblemName(networkProblem));
		savegameAssertf(pErrorMessage, "CSaveGameExport::GetNetworkErrorMessageText - Can't find Key 0x%x in the text file. networkProblem=%s", hashOfTextKey, GetNetworkProblemName(networkProblem));
	}
	return pErrorMessage;
}

#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

