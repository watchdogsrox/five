//
// filename:	ProfileSettings.cpp
// description:	
//

// --- Include Files ------------------------------------------------------------

#include "profilesettings.h"

#if __PS3
#include <np.h>
#endif

#if __XENON
#include <xonline.h>
#endif

#if __PPU
// PS3 headers
#include <cell/error.h>
#include <sysutil/sysutil_sysparam.h>
#endif //__PPU

#if RSG_ORBIS
#include <np/np_common.h>
#endif

// Rage headers
#include "bank/bkmgr.h"
#include "bank/bank.h"
#include "string/stringhash.h"
#include "system/ipc.h"
#include "system/param.h"
#include "system/xtl.h"
#include "system/nelem.h"
#include "rline/rlgamerinfo.h"
#include "rline/rlpresence.h"
#include "fwnet/netchannel.h"

// Game headers
#include "debug/Debug.h"
#include "frontend/DisplayCalibration.h"
#include "Frontend/ui_channel.h"
#include "network/live/livemanager.h"
#include "Network/Network.h"
#include "SaveLoad/GenericGameStorage.h"
#include "SaveLoad/savegame_queue.h"
#include "fwsys/timer.h"
#include "peds/PedWeapons/PedTargetEvaluator.h"
#include "SaveLoad/savegame_new_game_checks.h"
#include "stats/StatsInterface.h"
#include "script/script.h"

// --- Defines ------------------------------------------------------------------

// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------

// --- Globals ------------------------------------------------------------------

RAGE_DEFINE_SUBCHANNEL(net, profile_settings, DIAG_SEVERITY_DEBUG3)
#undef __net_channel
#define __net_channel net_profile_settings

NOSTRIP_XPARAM(uilanguage);
NOSTRIP_PARAM(ignoreprofile, "Ignore the current profile settings");
PARAM(ignoreprofilesetting, "Ignore an individual profile settings");
PARAM(spewheistvalues, "Ignore an individual profile settings");

// --- Static Globals -----------------------------------------------------------

static rlPresence::Delegate     g_PresenceDlgt(CProfileSettings::OnPresenceEvent);

#if __XENON

#if __BANK
bool CProfileSettings::sm_bDisplayDebugText = false;
bool CProfileSettings::sm_bUpdateProfileWhenNavigatingSettingsMenu = true;
s32 CProfileSettings::sm_MinimumTimeBetweenProfileSaves = (1000*60*5);
#endif	//	__BANK

static XOVERLAPPED g_overlapped;
enum {
	XPROFILE_OPTION_VOICE_MUTED_INDEX,
	XPROFILE_OPTION_VOICE_THRU_SPEAKERS_INDEX,
	XPROFILE_OPTION_VOICE_VOLUME_INDEX,
	XPROFILE_OPTION_CONTROLLER_VIBRATION_INDEX,
	XPROFILE_TITLE_SPECIFIC1_INDEX,
	XPROFILE_GAMER_ACTION_AUTO_AIM_INDEX,
	XPROFILE_GAMER_YAXIS_INVERSION_INDEX,
	XPROFILE_GAMER_CONTROL_SENSITIVITY_INDEX,
    XPROFILE_GAMERCARD_REGION_INDEX,
	XPROFILE_NUM_SETTINGS
};
static DWORD g_ProfileSettingIds[XPROFILE_NUM_SETTINGS] = {
	XPROFILE_OPTION_VOICE_MUTED,
	XPROFILE_OPTION_VOICE_THRU_SPEAKERS,
	XPROFILE_OPTION_VOICE_VOLUME,
	XPROFILE_OPTION_CONTROLLER_VIBRATION,
	XPROFILE_TITLE_SPECIFIC1,
	XPROFILE_GAMER_ACTION_AUTO_AIM,
	XPROFILE_GAMER_YAXIS_INVERSION,
	XPROFILE_GAMER_CONTROL_SENSITIVITY,
    XPROFILE_GAMERCARD_REGION
};
static XUSER_READ_PROFILE_SETTING_RESULT* g_ProfileSettingResults=NULL;

#elif USE_PROFILE_SAVEGAME

#define TOTAL_NUM_SETTINGS 163
#define SETTINGS_VERSION	3

struct ProfileSettingBuffer
{
	s32 m_num;
	s32 m_version;
	s32 m_crc;
	CProfileSettings::CustomSetting m_settings[TOTAL_NUM_SETTINGS];

	int CalculateCrc();
};
ProfileSettingBuffer g_ProfileSettingBuffer;
static char16 g_profileName[12];
static char g_fileName[12];

#elif RSG_PC

#define TOTAL_NUM_PC_SETTINGS (205)

CProfileSettings::CustomSetting s_PcSettingsToSave[TOTAL_NUM_PC_SETTINGS];
s32 s_NumberOfPcSettingsToSave = 0;

#endif  

#if __WIN32PC
#define PC_SETTINGS_FILENAME "pc_settings.bin"
#define PC_SETTINGS_PATH "user:/" PC_SETTINGS_FILENAME
#endif

#if USE_PROFILE_SAVEGAME
int ProfileSettingBuffer::CalculateCrc()
{
	return atDataHash((const char*)&m_settings[0], sizeof(CProfileSettings::CustomSetting)*m_num, 0);
}
#endif

// --- Static class members -----------------------------------------------------

CProfileSettings* CProfileSettings::sm_Instance = NULL;

// --- Code ---------------------------------------------------------------------


void CProfileSettings::InitClass()
{
#if __XENON
	g_overlapped.hEvent = CreateEvent(NULL, false, false, NULL);
#endif
	sm_Instance = rage_new CProfileSettings;

#if USE_PROFILE_SAVEGAME
	safecpy(g_profileName, "Profile");
	safecpy(g_fileName, "Profile");
#endif

	rlPresence::AddDelegate(&g_PresenceDlgt);

	sm_Instance->Default();

	// update once to catch if mainplayer is setup
	sm_Instance->UpdateProfileSettings(true);
}

#if RSG_PC
void CProfileSettings::InitDelegator()
{
	rlPresence::AddDelegate(&g_PresenceDlgt);
}
#endif

void CProfileSettings::ShutdownClass()
{
	delete sm_Instance;
}

#if __BANK && __XENON
void CProfileSettings::InitWidgets()
{
	bkBank& bank = BANKMGR.CreateBank("Profile Settings");

	bank.AddToggle("Display Debug Text", &sm_bDisplayDebugText);
	bank.AddToggle("Update Profile When Navigating Settings Menu", &sm_bUpdateProfileWhenNavigatingSettingsMenu);
	bank.AddSlider("Minimum Time Between Profile Saves", &sm_MinimumTimeBetweenProfileSaves, 0, 300000, 1000);
}
#endif	//	__BANK && __XENON

void CProfileSettings::Read(bool personalSettingsOnly)
{
	// set personal setting only if not in the middle of a read or it is setting it to false
	if((m_readPending == false && m_reading == false) || personalSettingsOnly == false)
		m_personalSettingsOnly = personalSettingsOnly;
	m_readPending = true;
}

void CProfileSettings::Write(bool bForceWrite)
{
	if (bForceWrite || m_hasChanged)
	{
		m_hasChanged = false;
		m_writePending = true;
	}
}

void CProfileSettings::WriteNow(bool bForceWrite)
{
#if USE_PROFILE_SAVEGAME
	if (!m_bDontWriteProfile)
#endif
	{
		Write(bForceWrite);
		UpdateProfileSettings(true, true);
	}
}

bool CProfileSettings::StartRead()
{
	bool success = true;

#if __XENON
	DWORD status = ERROR_SUCCESS; 
	DWORD   resultsSize = 0;
	// This example does not initialize the values of all of the needed paramaters.
	// In this first call, explicitly pass in a null pointer and a buffer size 
	// of 0. This forces the function to set cbResults to the correct buffer 
	// size.
	status = XUserReadProfileSettings(
		0,						// title id of zero means this title
		0,			// user 0
		XPROFILE_NUM_SETTINGS,            
		&g_ProfileSettingIds[0],    
		&resultsSize,         // Pass in the address of the size variable
		NULL,
		NULL                 // This example uses the synchronous model
		);
	Assert(status == ERROR_INSUFFICIENT_BUFFER);

	Printf("CProfileSettings: Need %d\n", resultsSize);

	// If the function returns ERROR_INSUFFICIENT_BUFFER cbResults has been 
	// changed to reflect the size buffer that will be necessary for this call. Use 
	// the new size to allocate a buffer of the appropriate size.
	if (ERROR_INSUFFICIENT_BUFFER == status && resultsSize > 0)
	{
		// delete previous buffer if it exists
		if(g_ProfileSettingResults)
			delete[] (char*)g_ProfileSettingResults;
		g_ProfileSettingResults = (XUSER_READ_PROFILE_SETTING_RESULT*)rage_new char[resultsSize];

		ZeroMemory(g_ProfileSettingResults, resultsSize);

		// Next, call the function again with the exact same parameters, except 
		// this time use the modified buffer size and a pointer to a buffer that 
		// matches it.
		status = XUserReadProfileSettings(
			0,
			m_localIndex,
			XPROFILE_NUM_SETTINGS,            
			g_ProfileSettingIds, 
			&resultsSize, 
			g_ProfileSettingResults,
			&g_overlapped
			);
		Assert(status == ERROR_IO_PENDING);
	}
	return true;

#elif USE_PROFILE_SAVEGAME

#if USE_DOWNLOAD0_FOR_PROFILE_BACKUP
	m_ProfileLoadStructure.Init(m_localIndex, &g_ProfileSettingBuffer, GetSizeOfPSProfileFile());
#else
	m_ProfileLoadStructure.Init(m_localIndex, GetFilenameOfPSProfile(), &g_ProfileSettingBuffer, GetSizeOfPSProfileFile());
#endif	//	USE_DOWNLOAD0_FOR_PROFILE_BACKUP

	if (!CGenericGameStorage::PushOnToSavegameQueue(&m_ProfileLoadStructure))
	{
		Warningf("CProfileSettings::StartRead - CGenericGameStorage::PushOnToSavegameQueue failed\n");
		success = false;
	}
#elif RSG_PC
	m_valid = true;
	m_settings.clear();

	// you need to be signed in to save profile-specific data.
	char fullPath[rgsc::RGSC_MAX_PATH] = {0};
	if(g_rlPc.IsInitialized() && g_rlPc.GetProfileManager()->IsSignedIn())
	{
		if(AssertVerify(SUCCEEDED(g_rlPc.GetFileSystem()->GetTitleProfileDirectory(fullPath, true))))
		{
			safecat(fullPath, PC_SETTINGS_FILENAME);
		}
	}
	else
	{
		safecpy(fullPath, PC_SETTINGS_PATH);
	}

	fiStream* pStream = ASSET.Open(fullPath, "");
	if (pStream)
	{
		CustomSetting setting;
		while (true)
		{
			size_t read = pStream->Read(&setting, sizeof(CustomSetting));
			if (read == sizeof(CustomSetting))
			{
				m_settings.PushAndGrow(setting);
			}
			else
			{
				break;
			}
		}
		pStream->Close();
	}
	else
	{
		success = false;
	}

#else
#error "Profile Settings are not supported in this build"
#endif

#if USE_SETTINGS_MANAGER
	// Load PC settings
	BANK_ONLY(CSettingsManager::UpdateRagData());
#endif

	return success;
}

bool CProfileSettings::FinishRead(bool 
#if __XENON || USE_PROFILE_SAVEGAME
								  bWait
#endif
								  )
{
#if !__FINAL
	int ignoreProfileSetting = INVALID_PROFILE_SETTING;
	PARAM_ignoreprofilesetting.Get(ignoreProfileSetting);
#endif
#if __XENON
	DWORD status;
	DWORD result;

	status = XGetOverlappedResult(&g_overlapped, &result, bWait);
	if(status == ERROR_IO_INCOMPLETE)
		return false;
	if(status == ERROR_SUCCESS)
	{
		m_valid = true;

		if(!m_personalSettingsOnly)
		{
			if(!PARAM_ignoreprofile.Get())
			{
				// build custom setting array
				XUSER_PROFILE_SETTING& specificSettings = g_ProfileSettingResults->pSettings[XPROFILE_TITLE_SPECIFIC1_INDEX];
				Assert(specificSettings.dwSettingId == XPROFILE_TITLE_SPECIFIC1);
				int num = specificSettings.data.binary.cbData / sizeof(CustomSetting);

				m_settings.ResetCount();
				if(num)
				{
					CustomSetting* pSetting = (CustomSetting*)specificSettings.data.binary.pbData;
					while(num--)
					{
#if !__FINAL
						if(pSetting->id != ignoreProfileSetting)
#endif
						{
							m_settings.PushAndGrow(*pSetting++);
						}
					}
				}
			}

			// if settings don't exist then use the xbox settings
			if(!Exists(TARGETING_MODE))
				Set(TARGETING_MODE, (g_ProfileSettingResults->pSettings[XPROFILE_GAMER_ACTION_AUTO_AIM_INDEX].data.nData > 0) ? (int)CPedTargetEvaluator::TARGETING_OPTION_GTA_TRADITIONAL : (int)CPedTargetEvaluator::TARGETING_OPTION_FREEAIM );
			
			if(!Exists(AXIS_INVERSION))
				Set(AXIS_INVERSION, g_ProfileSettingResults->pSettings[XPROFILE_GAMER_YAXIS_INVERSION_INDEX].data.nData);
			if(!Exists(CONTROLLER_VIBRATION))
				Set(CONTROLLER_VIBRATION, g_ProfileSettingResults->pSettings[XPROFILE_OPTION_CONTROLLER_VIBRATION_INDEX].data.nData);
			if(!Exists(CONTROLLER_SENSITIVITY))
			{
				switch(g_ProfileSettingResults->pSettings[XPROFILE_GAMER_CONTROL_SENSITIVITY_INDEX].data.nData)
				{
				case XPROFILE_CONTROL_SENSITIVITY_MEDIUM:
					Set(CONTROLLER_SENSITIVITY, 1);
					break;
				case XPROFILE_CONTROL_SENSITIVITY_LOW:
					Set(CONTROLLER_SENSITIVITY, 0);
					break;
				case XPROFILE_CONTROL_SENSITIVITY_HIGH:
					Set(CONTROLLER_SENSITIVITY, 2);
					break;
				default:
					Assertf(0, "Unrecognised controller sensitivity value");
					break;
				}
			}
		}
		// always set these parameter as we can't override them in game
		Set(VOICE_THRU_SPEAKERS, g_ProfileSettingResults->pSettings[XPROFILE_OPTION_VOICE_THRU_SPEAKERS_INDEX].data.nData);
		Set(VOICE_MUTED, g_ProfileSettingResults->pSettings[XPROFILE_OPTION_VOICE_MUTED_INDEX].data.nData);
		Set(VOICE_VOLUME, g_ProfileSettingResults->pSettings[XPROFILE_OPTION_VOICE_VOLUME_INDEX].data.nData);

        Set(GAMERCARD_REGION, g_ProfileSettingResults->pSettings[XPROFILE_GAMERCARD_REGION_INDEX].data.nData);
	}
#elif USE_PROFILE_SAVEGAME
	if (bWait)
	{
		PF_PUSH_TIMEBAR("CGenericGameStorage Update");
		while (m_ProfileLoadStructure.GetStatus() == MEM_CARD_BUSY)
		{
			CGenericGameStorage::Update();
			sysIpcSleep(1);
		}
		PF_POP_TIMEBAR();
	}
	else
	{
		if (m_ProfileLoadStructure.GetStatus() == MEM_CARD_BUSY)
		{
			return false;
		}
	}

	if(!PARAM_ignoreprofile.Get())
	{
		if( (m_ProfileLoadStructure.GetSizeOfLoadedProfile() > 0) && 
			m_ProfileLoadStructure.GetSizeOfLoadedProfile() <= (u32)GetSizeOfPSProfileFile() && 
			g_ProfileSettingBuffer.m_version == SETTINGS_VERSION)
		{
			// check crc, return if it isnt correct
			if(g_ProfileSettingBuffer.m_crc != g_ProfileSettingBuffer.CalculateCrc())
			{
				m_valid = false;
				return true;
			}

			m_settings.ResetCount();
			for(s32 i=0; i<g_ProfileSettingBuffer.m_num; i++)
			{
#if !__FINAL
				if(g_ProfileSettingBuffer.m_settings[i].id != ignoreProfileSetting)
#endif
				{
					m_settings.PushAndGrow(g_ProfileSettingBuffer.m_settings[i]);
				}
			}

			gnetDebug1("Read() - Finished.");

#if !__FINAL
			if(PARAM_spewheistvalues.Get())
			{
				gnetDebug1("Value for FREEMODE_PROLOGUE_COMPLETE_CHAR0 with id='%d'.", this->GetInt(FREEMODE_PROLOGUE_COMPLETE_CHAR0));
				gnetDebug1("Value for FREEMODE_PROLOGUE_COMPLETE_CHAR1 with id='%d'.", this->GetInt(FREEMODE_PROLOGUE_COMPLETE_CHAR1));
				gnetDebug1("Value for FREEMODE_PROLOGUE_COMPLETE_CHAR2 with id='%d'.", this->GetInt(FREEMODE_PROLOGUE_COMPLETE_CHAR2));
				gnetDebug1("Value for FREEMODE_PROLOGUE_COMPLETE_CHAR3 with id='%d'.", this->GetInt(FREEMODE_PROLOGUE_COMPLETE_CHAR3));
				gnetDebug1("Value for FREEMODE_PROLOGUE_COMPLETE_CHAR4 with id='%d'.", this->GetInt(FREEMODE_PROLOGUE_COMPLETE_CHAR4));
			}
#endif // !__FINAL
		}
	}

	m_valid = true;

#elif RSG_PC
	m_valid = true;
#endif

	// Activate display calibration at startup is profile setting doesnt exist
	if(Exists(DISPLAY_GAMMA))
		CDisplayCalibration::ActivateAtStartup(false);


	sysLanguage language = g_SysService.GetSystemLanguage();

	// Possible language override
	const char* langString;
	if(PARAM_uilanguage.Get(langString))
	{
		language = (sysLanguage) TheText.GetLanguageFromName(langString);
		Clear(DISPLAY_LANGUAGE);
	}

	if(language != LANGUAGE_UNDEFINED)
	{
		if (Exists(DISPLAY_LANGUAGE))
		{
			Set(CProfileSettings::NEW_DISPLAY_LANGUAGE, GetInt(DISPLAY_LANGUAGE));
		}
		else
		{
			Set(CProfileSettings::NEW_DISPLAY_LANGUAGE, (int) language);
		}
	}

	if(m_readCallback)
		m_readCallback();

	m_hasChanged = false;
	m_personalSettingsOnly = false;

	return true;
}


#if RSG_PC
static sysIpcThreadId s_SavePcProfileThreadId = sysIpcThreadIdInvalid;

static bool s_bSavePcProfileThreadHasBeenStarted = false;
static volatile bool s_bSavePcProfileThreadIsFinished = false;
static volatile bool s_bSavePcProfileThreadFailed = false;

//	DECLARE_THREAD_FUNC
void s_SavePcProfileThreadFunc(void*)
{
	PF_PUSH_MARKER("SavePcProfile");
	s_bSavePcProfileThreadFailed = false;

	char fullPath[rgsc::RGSC_MAX_PATH] = {0};

	if(g_rlPc.IsInitialized() && g_rlPc.GetProfileManager()->IsSignedIn())
	{
		if(AssertVerify(SUCCEEDED(g_rlPc.GetFileSystem()->GetTitleProfileDirectory(fullPath, true))))
		{
			safecat(fullPath, PC_SETTINGS_FILENAME);
		}
	}
	else
	{
		safecpy(fullPath, PC_SETTINGS_PATH); // using this as a default for -nosocialclub
	}

	ASSET.CreateLeadingPath(fullPath);
	fiStream* pStream = ASSET.Create(fullPath, "");
	if (Verifyf(pStream, "Couldn't open pc_settings.bin to write."))
	{
		pStream->Write(s_PcSettingsToSave, sizeof(CProfileSettings::CustomSetting) * s_NumberOfPcSettingsToSave);
		pStream->Close();
	}
	else
	{
		s_bSavePcProfileThreadFailed = true;
	}

#if !__FINAL
	unsigned current, maxi;
	if (sysIpcEstimateStackUsage(current,maxi))
		Displayf("Save PC Profile thread used %u of %u bytes stack space.",current,maxi);
#endif

	PF_POP_MARKER();

	s_bSavePcProfileThreadIsFinished = true;
}

bool CProfileSettings::StartSavePcProfileThread()
{
	if (s_SavePcProfileThreadId != sysIpcThreadIdInvalid)
	{
		Errorf("CProfileSettings::StartSavePcProfileThread started twice. Previous thread not finished, can't start.");
		return false;
	}

	if (s_bSavePcProfileThreadHasBeenStarted)
	{
		Errorf("CProfileSettings::StartSavePcProfileThread - s_bSavePcProfileThreadHasBeenStarted is true. Is there already a save in progress?");
		return false;
	}

	s_bSavePcProfileThreadIsFinished = false;
	s_bSavePcProfileThreadFailed = false;

	s_SavePcProfileThreadId = sysIpcCreateThread(s_SavePcProfileThreadFunc, NULL, 16384, PRIO_BELOW_NORMAL, "SavePcProfileThread", 0, "SavePcProfileThread");

	if (s_SavePcProfileThreadId != sysIpcThreadIdInvalid)
	{
		s_bSavePcProfileThreadHasBeenStarted = true;
	}
	else
	{
		s_bSavePcProfileThreadHasBeenStarted = false;
	}

	return s_bSavePcProfileThreadHasBeenStarted;
}

void CProfileSettings::EndSavePcProfileThread()
{
	if (s_SavePcProfileThreadId != sysIpcThreadIdInvalid)
	{
		sysIpcWaitThreadExit(s_SavePcProfileThreadId);
		s_SavePcProfileThreadId = sysIpcThreadIdInvalid;
	}

	s_bSavePcProfileThreadHasBeenStarted = false;
}

//	Return false if a save is in progress
//	Return true if a save has just finished, or if no save is currently running
bool CProfileSettings::CheckSavePcProfileThread()
{
	if (s_bSavePcProfileThreadHasBeenStarted)
	{
		if (s_bSavePcProfileThreadIsFinished)
		{
			EndSavePcProfileThread();
			return true;
		}
		else
		{
			return false;
		}
	}

	return true;
}
#endif	//	RSG_PC


bool CProfileSettings::StartWrite(bool DOWNLOAD0_PROFILE_ONLY(ignoreStreamingState))
{
#if __XENON
	Assertf(m_valid, "Cannot write invalid profile settings");
	// if there are any custom settings. Store them in title specific slot in profile settings
	XUSER_PROFILE_SETTING& specificSettings = g_ProfileSettingResults->pSettings[XPROFILE_TITLE_SPECIFIC1_INDEX];
	Assert(specificSettings.dwSettingId == XPROFILE_TITLE_SPECIFIC1);
	specificSettings.data.type = XUSER_DATA_TYPE_BINARY;
	if(m_settings.GetCount() > 0)
	{
		specificSettings.data.binary.cbData = m_settings.GetCount() * sizeof(CustomSetting);
		specificSettings.data.binary.pbData = (BYTE*)m_settings.GetElements();
		// Data size can't be greater than 1K
		Assert(specificSettings.data.binary.cbData <= 1024);
	}

	ASSERT_ONLY(DWORD status = )XUserWriteProfileSettings(m_localIndex,
											1,
											&g_ProfileSettingResults->pSettings[XPROFILE_TITLE_SPECIFIC1_INDEX],
											&g_overlapped);
	Assert(status == ERROR_IO_PENDING);
	return true;

#elif USE_PROFILE_SAVEGAME

	// create buffer
	if (!Verifyf( m_settings.GetCount() <= TOTAL_NUM_SETTINGS, "CProfileSettings::StartWrite - Too many settings, %d of %d requested for serialization", m_settings.GetCount(), TOTAL_NUM_SETTINGS ))
	{
		for (int i = TOTAL_NUM_SETTINGS; i < m_settings.GetCount(); ++i)
		{
			gnetDebug1("Will ignore setting id: %d", m_settings[i].id);
		}
	}

	int count = ::Min(m_settings.GetCount(), TOTAL_NUM_SETTINGS);
	g_ProfileSettingBuffer.m_num = count;//m_settings.GetCount();
	g_ProfileSettingBuffer.m_version = SETTINGS_VERSION;
	sysMemCpy(g_ProfileSettingBuffer.m_settings, m_settings.GetElements(), count * sizeof(CustomSetting));
	// calculate crc before continuing
	g_ProfileSettingBuffer.m_crc = g_ProfileSettingBuffer.CalculateCrc();

#if USE_DOWNLOAD0_FOR_PROFILE_BACKUP
	m_ProfileSaveStructure.Init(m_localIndex, &g_ProfileSettingBuffer, GetSizeOfPSProfileFile(), ignoreStreamingState? CSavegameQueuedOperation_ProfileSave::SAVE_BOTH_PROFILES : CSavegameQueuedOperation_ProfileSave::SAVE_BACKUP_PROFILE_ONLY);
#else
	m_ProfileSaveStructure.Init(m_localIndex, g_profileName, GetFilenameOfPSProfile(), &g_ProfileSettingBuffer, GetSizeOfPSProfileFile());
#endif

	if (!CGenericGameStorage::PushOnToSavegameQueue(&m_ProfileSaveStructure))
	{
		Warningf("CProfileSettings::StartWrite - CGenericGameStorage::PushOnToSavegameQueue failed\n");
		return false;
	}
	
	gnetDebug1("StartWrite() - Writting");

	return true;
	
#elif RSG_PC
	// Take a snapshot of the current profile settings. The save will take a number of frames to complete.
	if (!Verifyf( m_settings.GetCount() <= TOTAL_NUM_PC_SETTINGS, "CProfileSettings::StartWrite - Too many settings, %d of %d requested for serialization", m_settings.GetCount(), TOTAL_NUM_PC_SETTINGS ))
	{
		for (int i = TOTAL_NUM_PC_SETTINGS; i < m_settings.GetCount(); ++i)
		{
			gnetDebug1("Will ignore setting id: %d", i, m_settings[i].id);
		}
	}
	s_NumberOfPcSettingsToSave = ::Min(m_settings.GetCount(), TOTAL_NUM_PC_SETTINGS);

	sysMemCpy(s_PcSettingsToSave, m_settings.GetElements(), s_NumberOfPcSettingsToSave * sizeof(CustomSetting));

	if (StartSavePcProfileThread())
	{
		return true;	//	successfully started thread
	}
	else
	{
		return false;	//	failed to start thread
	}
#elif RSG_DURANGO
	return false;
#endif
}

bool CProfileSettings::FinishWrite(bool 
#if __XENON || USE_PROFILE_SAVEGAME || RSG_PC
								   bWait
#endif
								   )
{
#if __XENON
	DWORD status;
	DWORD result;

	status = XGetOverlappedResult(&g_overlapped, &result, bWait);
	if(status == ERROR_IO_INCOMPLETE)
		return false;

	if(status == ERROR_SUCCESS)
		m_timeOfLastWrite = fwTimer::GetSystemTimeInMilliseconds();

#elif USE_PROFILE_SAVEGAME
	if (bWait)
	{
		PF_PUSH_TIMEBAR("CGenericGameStorage Update");
		while (m_ProfileSaveStructure.GetStatus() == MEM_CARD_BUSY)
		{
			CGenericGameStorage::Update();
			sysIpcSleep(1);
		}
		PF_POP_TIMEBAR();
	}
	else
	{
		if (m_ProfileSaveStructure.GetStatus() == MEM_CARD_BUSY)
		{
			return false;
		}
	}
#elif RSG_PC
	if (bWait)
	{
		PF_PUSH_TIMEBAR("CProfileSettings::FinishWrite");
		while (CheckSavePcProfileThread() == false)
		{
			sysIpcSleep(1);
		}
		PF_POP_TIMEBAR();
	}
	else
	{
		if (CheckSavePcProfileThread() == false)
		{
			return false;
		}
	}
#endif

	gnetDebug1("FinishWrite() - done.");

	return true;
}

#if USE_DOWNLOAD0_FOR_PROFILE_BACKUP
void CProfileSettings::UpdateMainProfileSave()
{
	if (m_bMainProfileSaveIsRequired)
	{
		if (!CNetwork::IsNetworkOpen() && !CNetwork::HasCalledFirstEntryOpen())		//	Don't save the profile to the savegame folder during multiplayer
		{
			if (CGenericGameStorage::IsSafeToUseSaveLibrary())
			{
				if (m_MainProfileSaveStructure.GetStatus() != MEM_CARD_BUSY)
				{
					if (CSavegameQueue::IsEmpty())
					{	//	To give the main profile save the best chance of completing quickly, only push it on to the queue if the queue is empty.
						//	Any delay in starting this save increases the likelihood of the game getting back into a state where IsSafeToUseSaveLibrary() would have returned false.
						m_MainProfileSaveStructure.Init(m_localIndex, &g_ProfileSettingBuffer, GetSizeOfPSProfileFile(), CSavegameQueuedOperation_ProfileSave::SAVE_MAIN_PROFILE_ONLY);
						if (Verifyf(CGenericGameStorage::PushOnToSavegameQueue(&m_MainProfileSaveStructure), "CProfileSettings::UpdateMainProfileSave - failed to push a main profile save on to the savegame queue"))
						{
							m_bMainProfileSaveIsRequired = false;
						}
					}
				}
			}
		}
	}
}
#endif	//	USE_DOWNLOAD0_FOR_PROFILE_BACKUP

void CProfileSettings::UpdateProfileSettings(bool bWait, bool ORBIS_ONLY(ignoreStreamingState))
{
	PF_AUTO_PUSH_TIMEBAR("CProfileSettings UpdateProfileSettings");

#if __WIN32PC && (!__FINAL || defined(MASTER_NO_SCUI))
	if((g_rlPc.IsInitialized() == false))
	{
		// for cases where -nosocialclub is used, we won't have a profile signed in
		if (!m_valid)
		{
			Read();
		}
	}
	else
#endif
	{
		// Check if we have a main gamer setup
		const rlGamerInfo* pGamerInfo = NetworkInterface::GetActiveGamerInfo();
		if(pGamerInfo)
		{
			// If gamer id has changed then read profile settings
			if(pGamerInfo->GetGamerId() != m_gamerId)
			{
				m_gamerId = pGamerInfo->GetGamerId();
				m_localIndex = pGamerInfo->GetLocalIndex();
				Read();
			}
		}
		else
		{
			m_localIndex = -1;
			m_gamerId.Clear();
			m_readPending = false;
			m_writePending = false;
			// Revert to default profile settings if a profile has been loaded
			if(m_valid)
			{
				Default();
				m_valid = false;
			}
		}

		// If gamer id is invalid then disable any pending read/writes
		if(!m_gamerId.IsValid())
		{
			m_readPending = false;
			m_writePending = false;
			return;
		}
	}

#if USE_DOWNLOAD0_FOR_PROFILE_BACKUP
	UpdateMainProfileSave();
#endif	//	USE_DOWNLOAD0_FOR_PROFILE_BACKUP

#if DEBUG_DRAW && __BANK && __XENON
	if (sm_bDisplayDebugText)
	{
		s32 timeTillNextProfileWrite = 0;
		if (m_timeOfLastWrite > 0)
		{
			timeTillNextProfileWrite = (m_timeOfLastWrite + sm_MinimumTimeBetweenProfileSaves) - fwTimer::GetSystemTimeInMilliseconds();
			if (timeTillNextProfileWrite < 0)
			{
				timeTillNextProfileWrite = 0;
			}
		}
		grcDebugDraw::AddDebugOutput("timeTillNextProfileWrite = %d %s", timeTillNextProfileWrite, m_writePending?"Write Pending":"");
	}
#endif	//	DEBUG_DRAW && __BANK && __XENON

	if(m_reading)
	{
		if(FinishRead(bWait))
			m_reading = false;
	}
	else if(m_writing)
	{
		if(FinishWrite(bWait))
		{
			m_writing = false;
		}
	}
	else if(m_readPending)
	{
#if RSG_DURANGO
		if(CLiveManager::SignedInIntoNewProfileAfterSuspendMode())
		{
			gnetDebug1("UpdateProfileSettings - The session is just about to restart because the player has signed out of one profile and into a different profile. Delay the profile settings read until the beginning of the new session");
		}
		else
#endif	//	RSG_DURANGO
		{
			if (CSystem::IsThisThreadId(SYS_THREAD_UPDATE))
			{
				if(StartRead())
					m_reading = true;
				m_readPending = false;
			}
			else
			{
				gnetDebug1("UpdateProfileSettings - we should only call StartRead() on the Update thread");
			}
		}
	}
	// Can only write profile once every 5 minutes. If time of last write is zero then profile settings have never been set
	else if(m_writePending 
		&& (fwTimer::GetSystemTimeInMilliseconds() > m_timeOfLastWrite + sm_MinimumTimeBetweenProfileSaves || m_timeOfLastWrite == 0)
#if USE_PROFILE_SAVEGAME
#if RSG_ORBIS
		&& (CSavegameNewGameChecks::HaveFreeSpaceChecksCompleted())
#if !USE_DOWNLOAD0_FOR_PROFILE_BACKUP
		&& (CGenericGameStorage::IsSafeToUseSaveLibrary() || ignoreStreamingState)
#endif	//	!USE_DOWNLOAD0_FOR_PROFILE_BACKUP
#endif	//	RSG_ORBIS
		&& (!m_bDontWriteProfile)
#endif	//	USE_PROFILE_SAVEGAME
		)
	{
#if RSG_ORBIS
		if(StartWrite(ignoreStreamingState))
#else
		if(StartWrite())
#endif
		{
			m_writing = true;
		}
		m_writePending = false;
	}

	// If update has been passed that it has to wait then finish anything we started
	if(bWait)
	{
		if(m_reading)
		{
			if(FinishRead(bWait))
				m_reading = false;
			}
		else if(m_writing)
		{
			if(FinishWrite(bWait))
			{
				m_writing = false;
			}
		}
	}
}

//
// name:		CProfileSettings::OnPresenceEvent
// description:	This is called whenever there is a user presence event. We are only interested in the profile change event
#if __XENON || RSG_DURANGO
void CProfileSettings::OnPresenceEvent(const rlPresenceEvent* evt)
#else
void CProfileSettings::OnPresenceEvent(const rlPresenceEvent* )
#endif
{
#if __XENON || RSG_DURANGO
	if(PRESENCE_EVENT_PROFILE_CHANGED == evt->GetId())
	{
		// If we receive a profile change event then read profile settings again
		if(NetworkInterface::GetActiveGamerInfo() && 
			NetworkInterface::GetActiveGamerInfo()->GetGamerId() == evt->m_ProfileChanged->m_GamerInfo.GetGamerId())
		{
			Displayf("PRESENCE_EVENT_PROFILE_CHANGED - GamerId = %lu - about to call Read", evt->m_ProfileChanged->m_GamerInfo.GetGamerId().GetId());
			CProfileSettings::GetInstance().Read(true);
		}
	}
#endif
}

void CProfileSettings::Default()
{
	m_settings.ResetCount();

	if(m_readCallback)
		m_readCallback();
}

//
// name:		CProfileSettings::Exists
// description:	Returns if a profile setting exists
bool CProfileSettings::Exists(ProfileSettingId id)
{
	return (FindSetting(id) != -1);
}

//
// name:		CProfileSettings::Clear
// description:	Remove a setting
void CProfileSettings::Clear(ProfileSettingId id)
{
	s32 index = FindSetting(id);
	if(index != -1)
	{
		m_hasChanged = true;
		m_settings.DeleteFast(index);
	}
}

bool CProfileSettings::IsReading() const
{
    return m_reading || m_readPending;
}

bool CProfileSettings::IsWriting() const
{
    return m_writing || m_writePending;
}

//
// name:		CProfileSettings::FindSetting
// description:	Find a setting in the list
s32 CProfileSettings::FindSetting(ProfileSettingId id)
{
	for(s32 i=0; i<m_settings.GetCount(); i++)
	{
		if(m_settings[i].id == id)
			return i;
	}
	return -1;
}

//
// name:		CProfileSettings::FindSettingPtr
// description:	Find a setting in the list
CProfileSettings::CustomSetting* CProfileSettings::FindSettingPtr(ProfileSettingId id)
{
	s32 index = FindSetting(id);
	if(index != -1)
		return &m_settings[index];
	return NULL;
}
//
// name:		CProfileSettings::SetCustom
// description:	Get/Set for custom settings. 
void CProfileSettings::Set(ProfileSettingId id, int value)
{
	// if setting exists and it is the same then return. Doesn't set the hasChanged flag
	if(Exists(id) && GetInt(id) == value)
		return;
	m_hasChanged = true;

#if !__FINAL
	if(PARAM_spewheistvalues.Get())
	{
		if (FREEMODE_PROLOGUE_COMPLETE_CHAR0 == id)
		{
			gnetDebug1("%s:Set() for FREEMODE_PROLOGUE_COMPLETE_CHAR0 with old='%d', new='%d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), this->GetInt(FREEMODE_PROLOGUE_COMPLETE_CHAR0), value);
			rage::sysStack::PrintStackTrace();
		}
		else if (FREEMODE_PROLOGUE_COMPLETE_CHAR1 == id)
		{
			gnetDebug1("%s:Set() for FREEMODE_PROLOGUE_COMPLETE_CHAR1 with old='%d', new='%d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), this->GetInt(FREEMODE_PROLOGUE_COMPLETE_CHAR1), value);
			rage::sysStack::PrintStackTrace();
		}
		else if (FREEMODE_PROLOGUE_COMPLETE_CHAR2 == id)
		{
			gnetDebug1("%s:Set() for FREEMODE_PROLOGUE_COMPLETE_CHAR2 with old='%d', new='%d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), this->GetInt(FREEMODE_PROLOGUE_COMPLETE_CHAR2), value);
			rage::sysStack::PrintStackTrace();
		}
		else if (FREEMODE_PROLOGUE_COMPLETE_CHAR3 == id)
		{
			gnetDebug1("%s:Set() for FREEMODE_PROLOGUE_COMPLETE_CHAR3 with old='%d', new='%d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), this->GetInt(FREEMODE_PROLOGUE_COMPLETE_CHAR3), value);
			rage::sysStack::PrintStackTrace();
		}
		else if (FREEMODE_PROLOGUE_COMPLETE_CHAR4 == id)
		{
			gnetDebug1("%s:Set() for FREEMODE_PROLOGUE_COMPLETE_CHAR4 with old='%d', new='%d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), this->GetInt(FREEMODE_PROLOGUE_COMPLETE_CHAR4), value);
			rage::sysStack::PrintStackTrace();
		}
	}
#endif // !__FINAL

	StatsInterface::UpdateProfileSetting(id);

	CustomSetting* pSetting = FindSettingPtr(id);
	if(pSetting)
		pSetting->ivalue = value;
	else
	{
		CustomSetting setting;
		setting.id = id;
		setting.ivalue = value;
		m_settings.PushAndGrow(setting);
	}
}
void CProfileSettings::Set(ProfileSettingId id, float value)
{
	// if setting exists and it is the same then return. Doesn't set the hasChanged flag
	if(Exists(id) && GetFloat(id) == value)
		return;
	m_hasChanged = true;

	CustomSetting* pSetting = FindSettingPtr(id);
	if(pSetting)
		pSetting->fvalue = value;
	else
	{
		CustomSetting setting;
		setting.id = id;
		setting.fvalue = value;
		m_settings.PushAndGrow(setting);
	}
}
int CProfileSettings::GetInt(ProfileSettingId id)
{
	CustomSetting* pSetting = FindSettingPtr(id);
	if(pSetting)
		return pSetting->ivalue;
	return 0;
}
float CProfileSettings::GetFloat(ProfileSettingId id)
{
	CustomSetting* pSetting = FindSettingPtr(id);
	if(pSetting)
		return pSetting->fvalue;
	return 0.0f;
}


#if USE_PROFILE_SAVEGAME
char *CProfileSettings::GetFilenameOfPSProfile()
{
	return &g_fileName[0];
}

char16 *CProfileSettings::GetDisplayNameOfPSProfile()
{
	return &g_profileName[0];
}

int CProfileSettings::GetSizeOfPSProfileFile()
{
	return sizeof(g_ProfileSettingBuffer);
}
#endif


bool CProfileSettings::GetCountry(char (&countryCode)[3])
{
    // Make sure the result is null terminated
    countryCode[2] = '\0';

#if __PS3
	SceNpCountryCode nPCountryCode;
	int lang=0;
	int err = sceNpManagerGetAccountRegion(&nPCountryCode, &lang);
	if(err == 0) // Ok
	{
		countryCode[0] = nPCountryCode.data[0];
		countryCode[1] = nPCountryCode.data[1];
        return true;
	}
	else
	{
		uiErrorf("CProfileSettings::GetCountry - sceNpManagerGetAccountRegion returned an error 0x%x", err);
	}
#elif __XENON
    //For the 360, country comes from a profile setting
    CompileTimeAssert(XONLINE_COUNTRY_UNKNOWN == 0);
    CompileTimeAssert(XONLINE_COUNTRY_MAX <= 255);
    const int regionCode = GetInt(GAMERCARD_REGION);
    if (regionCode != 0 &&
        Verifyf(regionCode <= 255, "XONLINE_COUNTRY is larger than expected (%d)", regionCode))
    {
        struct LiveCountryMapping
        {
            u8 m_LiveCountryCode;
            char m_IsoCountryCode[2];
        };
        
        static const LiveCountryMapping liveCountryMappings[] = {
            #include "LiveCountryMappings.h"
        };

        for (unsigned k = 0; k < COUNTOF(liveCountryMappings); k++)
        {
            if (liveCountryMappings[k].m_LiveCountryCode == (u8)regionCode)
            {
                countryCode[0] = liveCountryMappings[k].m_IsoCountryCode[0];
                countryCode[1] = liveCountryMappings[k].m_IsoCountryCode[1];

                return true;
            }
        }

        //Emit an error if we didn't find a mapping
        uiErrorf("CProfileSettings::GetCountry - No XONLINE_COUNTRY mapping found for %d", regionCode);
    }
    else
    {
        uiErrorf("CProfileSettings::GetCountry - Failed to retrieve region from profile");
    }
#elif RSG_ORBIS
	if(RL_VERIFY_LOCAL_GAMER_INDEX(m_localIndex))
	{
		return g_rlNp.GetCountryCode(m_localIndex, countryCode);
	}
#elif RSG_DURANGO
	if(g_SysService.GetSystemCountry(countryCode))
	{
		return true;
	}
#elif RSG_PC
	// This shouldn't be called on PC. The SCUI (Social Club UI) requires the user to choose their country from a list.
	Assert(false);
#else
    Assert(false);
#endif

    return false;
}

int CProfileSettings::GetReplayTutorialFlags()
{
	int result = 0;

	if( Exists( ROCKSTAR_EDITOR_TUTORIAL_FLAGS ) )
	{
		result = GetInt( ROCKSTAR_EDITOR_TUTORIAL_FLAGS );
	}
	
	return result;
}

void CProfileSettings::UpdateReplayTutorialFlags( s32 const tutorialFlag )
{
	int const c_existingFlags = GetReplayTutorialFlags();
	int const c_resultingFlags = c_existingFlags | tutorialFlag;
	Set( ROCKSTAR_EDITOR_TUTORIAL_FLAGS, c_resultingFlags );
}
