#include "audio_channel.h"
//#include "audiogeometry.h"
#include "audioengine/engine.h"
#include "audioengine/engineutil.h"
#include "dynamicmixer.h"
#include "game/Wanted.h"
#include "northaudioengine.h"
#include "Network/NetworkInterface.h"
#include "Peds/ped.h"
#include "profile/profiler.h"
#include "scene/playerswitch/PlayerSwitchInterface.h"
#include "scene/world/GameWorld.h"
#include "script/script.h"
#include "scriptaudioentity.h"
#include "superconductor.h"
#include "system/param.h"


AUDIO_OPTIMISATIONS()

//OPTIMISATIONS_OFF()
 

PF_PAGE(SuperConductorTimers,"SuperConductor");
PF_GROUP(SuperConductor);
PF_LINK(SuperConductorTimers, SuperConductor);
PF_TIMER(SuperConductorProcessUpdate,SuperConductor);
PF_TIMER(SuperConductorUpdateInfo,SuperConductor);
PF_TIMER(SuperConductorAnalizeInfo,SuperConductor);
PF_TIMER(SuperConductorProcessInfo,SuperConductor);

PARAM(noQuietScene, "[Audio] Disable quiet scene.");

//-------------------------------------------------------------------------------------------------------------------
//														INIT
//-------------------------------------------------------------------------------------------------------------------
f32 audSuperConductor::sm_QuietSceneMaxApply = 1.f;
f32 audSuperConductor::sm_SoftQuietSceneMaxApply = 0.25f;

// player shot
s32 audSuperConductor::sm_PlayerShotQSFadeOutTime = 100;
s32 audSuperConductor::sm_PlayerShotQSDelayTime = -1;
s32 audSuperConductor::sm_PlayerShotQSFadeInTime= -1;
// player in vehicle
s32 audSuperConductor::sm_PlayerInVehicleQSFadeOutTime = 4000;
s32 audSuperConductor::sm_PlayerInVehicleQSDelayTime = -1;
s32 audSuperConductor::sm_PlayerInVehicleQSFadeInTime = -1;
// player stealth
s32 audSuperConductor::sm_PlayerInStealthQSFadeOutTime = 5000;
s32 audSuperConductor::sm_PlayerInStealthQSDelayTime = -1;
s32 audSuperConductor::sm_PlayerInStealthQSFadeInTime = -1;
// player in interior
s32 audSuperConductor::sm_PlayerInInteriorQSFadeOutTime = 2000;
s32 audSuperConductor::sm_PlayerInInteriorQSDelayTime = -1;
s32 audSuperConductor::sm_PlayerInInteriorQSFadeInTime = -1;
// player dead
s32 audSuperConductor::sm_PlayerDeadQSFadeOutTime = 100;
s32 audSuperConductor::sm_PlayerDeadQSDelayTime = -1;
s32 audSuperConductor::sm_PlayerDeadQSFadeInTime = -1;
// player underwater
s32 audSuperConductor::sm_UnderWaterQSFadeOutTime = 100;
s32 audSuperConductor::sm_UnderWaterQSDelayTime = 0;
s32 audSuperConductor::sm_UnderWaterQSFadeInTime = 100;
// player switch
s32 audSuperConductor::sm_PlayerSwitchQSFadeOutTime = -1;
s32 audSuperConductor::sm_PlayerSwitchDelayTime = -1;
s32 audSuperConductor::sm_PlayerSwitchFadeInTime = -1;
// gunfight conductor
s32 audSuperConductor::sm_GunfightConductorQSFadeOutTime = 2000;
s32 audSuperConductor::sm_GunfightConductorQSDelayTime = -1;
s32 audSuperConductor::sm_GunfightConductorQSFadeInTime = -1;
// Loud sound
s32 audSuperConductor::sm_LoudSoundQSFadeOutTime = -1;
s32 audSuperConductor::sm_LoudSoundQSDelayTime = -1;
s32 audSuperConductor::sm_LoudSoundQSFadeInTime = -1;
// On Mission
s32 audSuperConductor::sm_OnMissionQSFadeOutTime = 2000;
s32 audSuperConductor::sm_OnMissionQSDelayTime = -1;
s32 audSuperConductor::sm_OnMissionQSFadeInTime = -1;
// default
u32 audSuperConductor::sm_QSDefaultFadeOutTime = 1000;
u32 audSuperConductor::sm_QSDefaultDelayTime = 2000;
u32 audSuperConductor::sm_QSDefaultFadeInTime = 7500;

bool audSuperConductor::sm_StopQSOnPlayerShot = true;
bool audSuperConductor::sm_StopQSOnPlayerInVehicle = true;
bool audSuperConductor::sm_StopQSOnPlayerInStealth = true;
bool audSuperConductor::sm_StopQSOnPlayerInInterior = true;
bool audSuperConductor::sm_StopQSOnPlayerDead = true;
bool audSuperConductor::sm_StopQSOnUnderWater = true;
bool audSuperConductor::sm_StopQSOnPlayerSwitch = true;
bool audSuperConductor::sm_StopQSOnGunfightConductor = true;
bool audSuperConductor::sm_StopQSOnLoudSounds = true;
bool audSuperConductor::sm_StopQSOnMission = true;

BANK_ONLY(bool audSuperConductor::sm_ShowInfo = false;);
//-------------------------------------------------------------------------------------------------------------------
void audSuperConductor::Init()
{
	m_GunFightConductor.Init();
	m_VehicleConductor.Init();
	for ( u32 i = GunfightConductor; i < MaxNumConductors - 1 ; i++)
	{
		m_ConductorsInfo[i].info = Info_Invalid;
		m_ConductorsInfo[i].confidence = 0.f;
	}
	m_Scene = NULL;
	m_QuietScene = NULL;
	m_QuietSceneState = AUD_QS_WANTS_TO_PLAY;

	m_TimeInQSState = 0;
	m_QSCurrentFadeOutTime = 7500;
	m_QSCurrentDelayTime = 2000;
	m_QSCurrentFadeInTime = 1000;
	m_QuietSceneApply = 0.f;

	m_WaitForPlayerAliveToPlayQS = false;
	m_UsingDefaultQSFadeOut = true;
	m_UsingDefaultQSDelay = true;
	m_UsingDefaultQSFadeIn = true;
}
//-------------------------------------------------------------------------------------------------------------------
void audSuperConductor::ShutDown()
{
	m_GunFightConductor.Reset();
	m_VehicleConductor.Reset();
	if(	m_Scene )
	{
		m_Scene->Stop();
	}
	if(	m_QuietScene )
	{
		m_QuietScene->Stop();
	}
}
//-------------------------------------------------------------------------------------------------------------------
//												CONDUCTORS MESSAGES
//-------------------------------------------------------------------------------------------------------------------
void audSuperConductor::SendConductorMessage(const ConductorMessageData &messageData)
{
	switch(messageData.conductorName)
	{
		case SuperConductor:	ProcessMesage(messageData);break;
		case GunfightConductor: 
			m_GunFightConductor.ProcessMessage(messageData);break;
		case VehicleConductor:	
			m_VehicleConductor.ProcessMessage(messageData);
			break;
		default: break;
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audSuperConductor::ProcessMesage(const ConductorMessageData &messageData)
{
	

	switch(messageData.message)
	{
	case SwitchConductorsOff:
		//SwitchOff();
		break;
	case ResetConductors:
		//Reset();
		break;
	case MuteGameWorld:
		MuteWorld();
		break;
	case UnmuteGameWorld:
		UnmuteWorld();
		break;
	case StopQuietScene:
		// Fade out :
		ComputeQsTimesAndStop((u32)messageData.vExtraInfo.GetX(),(u32)messageData.vExtraInfo.GetY(),(u32)messageData.vExtraInfo.GetZ());
		break;
	default: break;
	}
}
//-------------------------------------------------------------------------------------------------------------------
//											SUPERCONDUCTOR DECISION MAKING
//-------------------------------------------------------------------------------------------------------------------
void audSuperConductor::ProcessUpdate()
{
	PF_FUNC(SuperConductorProcessUpdate);
	bool noQuietScene = false;
	if(PARAM_noQuietScene.Get())
	{
		noQuietScene = true;
	}
	if(!noQuietScene)
	{
		UpdateQuietScene();
	}
	else 
	{
		if(m_QuietScene)
		{
			m_QuietScene->Stop();
		}
	}
	UpdateInfo();
	AnalizeInfo();
	ProcessInfo();
	m_GunFightConductor.ProcessUpdate();
	m_VehicleConductor.ProcessUpdate();
#if __BANK
	SuperConductorDebugInfo();
#endif
}
//-------------------------------------------------------------------------------------------------------------------
void audSuperConductor::UpdateInfo()
{
	PF_FUNC(SuperConductorUpdateInfo);
	for ( u32 i = GunfightConductor; i < MaxNumConductors - 1 ; i++)
	{
		m_ConductorsInfo[i] = UpdateConductorInfo((Conductor)i);
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audSuperConductor::AnalizeInfo()
{
	PF_FUNC(SuperConductorAnalizeInfo);
	for ( u32 conductorIndex = GunfightConductor; conductorIndex < MaxNumConductors - 1 ; conductorIndex++)
	{
		AnalizeConductorInfo((Conductor)conductorIndex);
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audSuperConductor::ProcessInfo()
{
	PF_FUNC(SuperConductorProcessInfo);
	for ( u32 conductorIndex = GunfightConductor; conductorIndex < MaxNumConductors - 1 ; conductorIndex++)
	{
		// DOn't process the gunfight conductor info is script is overriding it.
		if( conductorIndex != GunfightConductor || (conductorIndex == GunfightConductor && !g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::ScriptForceGunfightConductorIntensity)))
		{
			switch (m_ConductorsAnalyzedInfo[conductorIndex].info)
			{
			case Info_Invalid: naAssertf(false,"Invalid Conductor info.");
				break;
			case Info_NothingToDo:
				SendOrderToConductor((Conductor)conductorIndex,Order_DoNothing);
				break;
			case Info_DoBasicJob:
				switch (m_ConductorsAnalyzedInfo[conductorIndex].emphasizeIntensity)
				{
				//case Intensity_Invalid: naWarningf("Invalid Conductor intensity.");
					//break;
				case Intensity_Low:
					SendOrderToConductor((Conductor)conductorIndex,Order_LowIntensity);
					break;
				case Intensity_Medium:
					SendOrderToConductor((Conductor)conductorIndex,Order_MediumIntensity);
					break;
				case Intensity_High:
					SendOrderToConductor((Conductor)conductorIndex,Order_HighIntensity);
					break;
				default:
					break;
				}
				break;
			case Info_DoSpecialStuffs:
				switch (m_ConductorsAnalyzedInfo[conductorIndex].emphasizeIntensity)
				{
				//case Intensity_Invalid: naWarningf("Invalid Conductor intensity.");
					//break;
				case Intensity_Low:
					SendOrderToConductor((Conductor)conductorIndex,Order_LowSpecialIntensity);
					break;
				case Intensity_Medium:
					SendOrderToConductor((Conductor)conductorIndex,Order_MediumSpecialIntensity);
					break;
				case Intensity_High:
					SendOrderToConductor((Conductor)conductorIndex,Order_HighSpecialIntensity);
					break;
				default:
					
					break;
				}
				if(conductorIndex == VehicleConductor)
				{
					SendOrderToConductor((Conductor)conductorIndex,Order_StuntJump);
				}
				break;
				// TODO: Here is the place where we can analyze one conductor to the rest of them to see if they can do things together
			default: 
				break;
			}
		}
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audSuperConductor::ComputeQsTimesAndStop(const u32 desireFadeOut, const u32 desireDelay, const u32 desireFadeIn)
{
	u32 fadeOut = (desireFadeOut != -1 ? desireFadeOut : sm_QSDefaultFadeOutTime);
	if(!m_UsingDefaultQSFadeOut)
	{
		m_QSCurrentFadeOutTime = Min (m_QSCurrentFadeOutTime,fadeOut);
	}
	else
	{
		m_QSCurrentFadeOutTime = fadeOut;
	}
	m_QSCurrentFadeOutTime = Max (m_QSCurrentFadeOutTime,1u);
	m_UsingDefaultQSFadeOut = false;
	// Delay : 
	u32 delay = (desireDelay != -1 ? desireDelay : sm_QSDefaultDelayTime);
	if(!m_UsingDefaultQSDelay)
	{
		m_QSCurrentDelayTime = Min(m_QSCurrentDelayTime,delay); // Min || Max || Override (?)
	}
	else
	{
		m_QSCurrentDelayTime = delay;
	}
	m_UsingDefaultQSDelay = false;
	// Fade in : 
	u32 fadeIn = (desireFadeIn != -1 ? desireFadeIn : sm_QSDefaultFadeInTime);
	if(!m_UsingDefaultQSFadeIn)
	{
		m_QSCurrentFadeInTime = Min (m_QSCurrentFadeInTime,fadeIn); // Min || Max || Override (?)
	}
	else
	{
		m_QSCurrentFadeInTime = fadeIn;
	}
	m_QSCurrentFadeInTime = Max (m_QSCurrentFadeInTime,1u); 
	m_UsingDefaultQSFadeIn = false;
	m_QuietSceneState = AUD_QS_STOPPING;
	m_TimeInQSState = 0;
}
//-------------------------------------------------------------------------------------------------------------------
void audSuperConductor::UpdateQuietScene()
{
	if(!NetworkInterface::IsGameInProgress())
	{
		CheckQSForOnMission();
	}
	CheckQSForPlayerDead();
	CheckQSForPlayerInInterior();
	CheckForPlayerSwitch();
	//CheckQSForPlayerDrunk();
	//CheckQSForSpecialAbility();
	f32 maxApply = sm_QuietSceneMaxApply;
	if(g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::UseQuietSceneSoftVersion) || m_GunFightConductor.UseQSSoftVersion()) 
	{
		maxApply = sm_SoftQuietSceneMaxApply;
	}
	switch(m_QuietSceneState)
	{
	case AUD_QS_WANTS_TO_PLAY:
		if( m_TimeInQSState > m_QSCurrentDelayTime )
		{
			// Start the scene if we haven't do it already.
			MixerScene * sceneSettings  = DYNAMICMIXMGR.GetObject<MixerScene>(ATSTRINGHASH("QUIET_SCENE", 0x85BA09E8)); 
			if(sceneSettings)
			{
				DYNAMICMIXER.StartScene(sceneSettings, &m_QuietScene, NULL);
			}
			if(naVerifyf(m_QuietScene,"Failed when trying to initialize the quiet scene."))
			{
				// Reset delay :
				m_QSCurrentDelayTime   = sm_QSDefaultDelayTime;
				m_UsingDefaultQSDelay = true;
				m_QuietSceneApply = 0.f;
				m_QuietScene->SetVariableValue(ATSTRINGHASH("apply", 0xE865CDE8), m_QuietSceneApply);
				m_QuietSceneState = AUD_QS_PLAYING;
				m_TimeInQSState = 0;
			}
			break;
		}
		m_TimeInQSState += audNorthAudioEngine::GetTimeStepInMs();
		break;
	case AUD_QS_PLAYING:
		if(m_QuietScene)
		{
			if(m_QuietSceneApply < maxApply)
			{
				m_QuietSceneApply += (1.f / m_QSCurrentFadeInTime ) * audNorthAudioEngine::GetTimeStepInMs();
				m_QuietSceneApply = Min(m_QuietSceneApply, 1.f);
				m_QuietScene->SetVariableValue(ATSTRINGHASH("apply", 0xE865CDE8), m_QuietSceneApply);
			}
			else if (m_QuietSceneApply > maxApply)
			{
				m_QuietSceneApply -=  (1.f / m_QSCurrentFadeInTime ) * audNorthAudioEngine::GetTimeStepInMs();
				m_QuietSceneApply = Max(m_QuietSceneApply, 0.f);
			}
			else // ==
			{
				// Reset fade in : 
				m_QSCurrentFadeInTime  = sm_QSDefaultFadeInTime;
				m_UsingDefaultQSFadeIn = true;
			}
			m_QuietScene->SetVariableValue(ATSTRINGHASH("apply", 0xE865CDE8), m_QuietSceneApply);
		}
		else
		{
			m_QuietSceneState = AUD_QS_WANTS_TO_PLAY;
			m_TimeInQSState = 0;
			break;
		}
		m_TimeInQSState += audNorthAudioEngine::GetTimeStepInMs();
		break;
	case AUD_QS_STOPPING:
		if(m_QuietScene)
		{
			if(m_QuietSceneApply > 0.f)
			{
				m_QuietSceneApply -= (1.f /  m_QSCurrentFadeOutTime ) * audNorthAudioEngine::GetTimeStepInMs();
				m_QuietSceneApply = Max(m_QuietSceneApply, 0.f);
				m_QuietScene->SetVariableValue(ATSTRINGHASH("apply", 0xE865CDE8), m_QuietSceneApply);
				m_TimeInQSState += audNorthAudioEngine::GetTimeStepInMs();
				break;
			}
			else
			{
				// Reset fade out : 
				m_QSCurrentFadeOutTime = sm_QSDefaultFadeOutTime;
				m_UsingDefaultQSFadeOut = true;
				m_QuietScene->Stop();
			}
		}
		if(!m_WaitForPlayerAliveToPlayQS)
		{
			m_QuietSceneState = AUD_QS_WANTS_TO_PLAY;
			m_TimeInQSState = 0;
		}
		m_TimeInQSState += audNorthAudioEngine::GetTimeStepInMs();
		break;
	default:
		break;
	}

}
//-------------------------------------------------------------------------------------------------------------------
//														HELPERS	
//-------------------------------------------------------------------------------------------------------------------
ConductorsInfo audSuperConductor::UpdateConductorInfo(Conductor conductor)
{
	naAssertf(conductor >= GunfightConductor && conductor < SuperConductor, "Bad conductor index.");
	ConductorsInfo info; 
	info.info = Info_Invalid;
	info.confidence = 0.f;
	switch(conductor)
	{
	case VehicleConductor:		
		info = m_VehicleConductor.UpdateInfo();
		break;
	case GunfightConductor:
		info = m_GunFightConductor.UpdateInfo();
		break;
	default: break;
	}
	return info;
}
//-------------------------------------------------------------------------------------------------------------------
void audSuperConductor::AnalizeConductorInfo(Conductor conductor)
{
	naAssertf(conductor >= GunfightConductor && conductor < SuperConductor, "Bad conductor index.");
	naAssertf(m_ConductorsInfo[conductor].info > Info_Invalid && m_ConductorsInfo[conductor].info <= Info_DoSpecialStuffs,"Invalid gunfightConductor info.");
	if (m_ConductorsInfo[conductor].info == Info_NothingToDo)
	{
		m_ConductorsAnalyzedInfo[conductor].emphasizeIntensity = Intensity_Invalid;
		m_ConductorsAnalyzedInfo[conductor].info = Info_NothingToDo;
	}
	else
	{
		switch(conductor)
		{
		case GunfightConductor:
			m_ConductorsAnalyzedInfo[GunfightConductor].emphasizeIntensity = m_ConductorsInfo[GunfightConductor].emphasizeIntensity;
			m_ConductorsAnalyzedInfo[GunfightConductor].info = m_ConductorsInfo[GunfightConductor].info;
			break;
		case VehicleConductor:	
			m_ConductorsAnalyzedInfo[VehicleConductor].emphasizeIntensity = m_ConductorsInfo[VehicleConductor].emphasizeIntensity;
			m_ConductorsAnalyzedInfo[VehicleConductor].info = m_ConductorsInfo[VehicleConductor].info;		
			break;
		default: break;
		}
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audSuperConductor::SendOrderToConductor(Conductor conductor,ConductorsOrders order)
{
	naAssertf(conductor >= GunfightConductor && conductor < SuperConductor, "Bad conductor index.");
	switch(conductor)
	{
	case VehicleConductor:		
		m_VehicleConductor.ProcessSuperConductorOrder(order);
		break;
	case GunfightConductor:
		m_GunFightConductor.ProcessSuperConductorOrder(order);
		break;
	default: break;
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audSuperConductor::MuteWorld()
{
	if(!m_Scene)
	{
		// Start the scene if we haven't do it already.
		MixerScene * sceneSettings  = DYNAMICMIXMGR.GetObject<MixerScene>(ATSTRINGHASH("MUTE_GAME_WORLD", 0x5F631270)); 
		if(sceneSettings)
		{
			DYNAMICMIXER.StartScene(sceneSettings, &m_Scene, NULL);
			naAssertf(m_Scene,"Failed when trying to initialize the mute game wolrd scene.");
		}
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audSuperConductor::UnmuteWorld()
{
	if(m_Scene)
	{
		m_Scene->Stop();
		m_Scene = NULL;
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audSuperConductor::CheckQSForOnMission()
{
	if(sm_StopQSOnMission)
	{
		if(CTheScripts::GetPlayerIsOnAMission())
		{
			ComputeQsTimesAndStop(sm_OnMissionQSFadeOutTime,sm_OnMissionQSDelayTime,sm_OnMissionQSFadeInTime);
		}
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audSuperConductor::CheckQSForPlayerDead()
{
	if(sm_StopQSOnPlayerDead)
	{
		CPed * player = CGameWorld::FindLocalPlayer();
		if(!player || (player && player->IsDead()))
		{
			ComputeQsTimesAndStop(sm_PlayerDeadQSFadeOutTime,sm_PlayerDeadQSDelayTime,sm_PlayerDeadQSFadeInTime);
		}
		else 
		{
			m_WaitForPlayerAliveToPlayQS = false;
		}
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audSuperConductor::CheckQSForPlayerInInterior()
{
	if(sm_StopQSOnPlayerInInterior && audNorthAudioEngine::GetGtaEnvironment()->AreWeInAnInterior())
	{
		const InteriorSettings* interiorSettings = audNorthAudioEngine::GetGtaEnvironment()->GetListenerInteriorSettingsPtr();

		if(interiorSettings == NULL || AUD_GET_TRISTATE_VALUE(interiorSettings->Flags, FLAG_ID_INTERIORSETTINGS_ALLOWQUIETSCENE)!=AUD_TRISTATE_TRUE)
		{
			ComputeQsTimesAndStop(sm_PlayerInInteriorQSFadeOutTime,sm_PlayerInInteriorQSDelayTime,sm_PlayerInInteriorQSFadeInTime);
		}		
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audSuperConductor::CheckForPlayerSwitch()
{
	if(sm_StopQSOnPlayerSwitch && g_PlayerSwitch.IsActive())
	{
		ComputeQsTimesAndStop(sm_PlayerSwitchQSFadeOutTime,sm_PlayerInInteriorQSDelayTime,sm_PlayerInInteriorQSFadeInTime);
	}
}

//-------------------------------------------------------------------------------------------------------------------
#if __BANK
void audSuperConductor::SuperConductorDebugInfo()
{
	if (sm_ShowInfo)
	{		
		char text[256];
		for ( u32 i = GunfightConductor; i < MaxNumConductors - 1 ; i++)
		{
			formatf(text,"%s -> info : %s confidence : %f ", GetConductorName((Conductor)i), GetInfoType(m_ConductorsInfo[i].info),m_ConductorsInfo[i].confidence);
			grcDebugDraw::AddDebugOutput(text);
		}
	}
}
//-------------------------------------------------------------------------------------------------------------------
const char* audSuperConductor::GetConductorName(Conductor conductor)
{
	switch(conductor)
	{
	case GunfightConductor:
		return "GunfightConductor";
	case VehicleConductor:		
		return "VehicleConductor";
	default: break;
	}
	return "Invalid_Conductor";
}
//-------------------------------------------------------------------------------------------------------------------
const char* audSuperConductor::GetInfoType(ConductorsInfoType info)
{	
	switch(info)
	{
	case Info_NothingToDo:
		return "Info_NothingToDo";
	case Info_DoBasicJob:		
		return "Info_DoBasicJob";
	case Info_DoSpecialStuffs:		
		return "Info_DoSpecialStuffs";
	default: break;
	}
	return "Info_Invalid";
}
//-------------------------------------------------------------------------------------------------------------------
void audSuperConductor::AddWidgets(bkBank &bank)
{
	bank.PushGroup("Conductors",false);
		bank.PushGroup("SuperConductor",false);
			bank.AddToggle("Show info", &sm_ShowInfo);
			bank.PushGroup("QuietScene",false);
				bank.PushGroup("Global - default ",false);
				bank.AddSlider("Max apply", &sm_QuietSceneMaxApply, 0.f, 1.f, 0.01f);
				bank.AddSlider("Soft max apply", &sm_SoftQuietSceneMaxApply, 0.f, 1.f, 0.01f);
					bank.AddSlider("Default fade out time (ms)", &sm_QSDefaultFadeOutTime, 0, 24000, 100);
					bank.AddSlider("Default delay time (ms)", &sm_QSDefaultDelayTime, 0, 24000, 100);
					bank.AddSlider("Default fade in time (ms)", &sm_QSDefaultFadeInTime, 0, 24000, 100);
				bank.PopGroup();
				bank.PushGroup("Fade out times",false);
					bank.PushGroup("Player shot",false);
						bank.AddToggle("Allow stop", &sm_StopQSOnPlayerShot);
						bank.AddSlider("Fade out (ms)", &sm_PlayerShotQSFadeOutTime, -1, 24000, 100);
						bank.AddSlider("Next delay (ms)", &sm_PlayerShotQSDelayTime, -1, 24000, 100);
						bank.AddSlider("Next fade in (ms)", &sm_PlayerShotQSFadeInTime, -1, 24000, 100);
					bank.PopGroup();
					bank.PushGroup("Player in vehicle",false);
						bank.AddToggle("Allow stop", &sm_StopQSOnPlayerInVehicle);
						bank.AddSlider("Fade out (ms)", &sm_PlayerInVehicleQSFadeOutTime, -1, 24000, 100);
						bank.AddSlider("Next delay (ms)", &sm_PlayerInVehicleQSDelayTime, -1, 24000, 100);
						bank.AddSlider("Next fade in (ms)", &sm_PlayerInVehicleQSFadeInTime, -1, 24000, 100);
					bank.PopGroup();
					bank.PushGroup("Player stealth",false);
						bank.AddToggle("Allow stop", &sm_StopQSOnPlayerInStealth);
						bank.AddSlider("Fade out (ms)", &sm_PlayerInStealthQSFadeOutTime, -1, 24000, 100);
						bank.AddSlider("Next delay (ms)", &sm_PlayerInStealthQSDelayTime, -1, 24000, 100);
						bank.AddSlider("Next fade in (ms)", &sm_PlayerInStealthQSFadeInTime, -1, 24000, 100);
					bank.PopGroup();
					bank.PushGroup("Player in interior",false);
						bank.AddToggle("Allow stop", &sm_StopQSOnPlayerInInterior);
						bank.AddSlider("Fade out (ms)", &sm_PlayerInInteriorQSFadeOutTime, -1, 24000, 100);
						bank.AddSlider("Next delay (ms)", &sm_PlayerInInteriorQSDelayTime, -1, 24000, 100);
						bank.AddSlider("Next fade in (ms)", &sm_PlayerInInteriorQSFadeInTime, -1, 24000, 100);
					bank.PopGroup();
					bank.PushGroup("Player dead",false);
						bank.AddToggle("Allow stop", &sm_StopQSOnPlayerDead);
						bank.AddSlider("Fade out (ms)", &sm_PlayerDeadQSFadeOutTime, -1, 24000, 100);
						bank.AddSlider("Next delay (ms)", &sm_PlayerDeadQSDelayTime, -1, 24000, 100);
						bank.AddSlider("Next fade in (ms)", &sm_PlayerDeadQSFadeInTime, -1, 24000, 100);
					bank.PopGroup();
					bank.PushGroup("UnderWater",false);
						bank.AddToggle("Allow stop", &sm_StopQSOnUnderWater);
						bank.AddSlider("Fade out (ms)", &sm_UnderWaterQSFadeOutTime, -1, 24000, 100);
						bank.AddSlider("Next delay (ms)", &sm_UnderWaterQSDelayTime, -1, 24000, 100);
						bank.AddSlider("Next fade in (ms)", &sm_UnderWaterQSFadeInTime, -1, 24000, 100);
					bank.PopGroup();
					bank.PushGroup("Player switch",false);
					bank.AddToggle("Allow stop", &sm_StopQSOnPlayerSwitch);
						bank.AddSlider("Fade out (ms)", &sm_PlayerSwitchQSFadeOutTime, -1, 24000, 100);
						bank.AddSlider("Next delay (ms)", &sm_PlayerSwitchDelayTime, -1, 24000, 100);
						bank.AddSlider("Next fade in (ms)", &sm_PlayerSwitchFadeInTime, -1, 24000, 100);
					bank.PopGroup();
					bank.PushGroup("Gunfight conductor",false);
						bank.AddToggle("Allow stop", &sm_StopQSOnGunfightConductor);
						bank.AddSlider("Fade out (ms)", &sm_GunfightConductorQSFadeOutTime, -1, 24000, 100);
						bank.AddSlider("Next delay (ms)", &sm_GunfightConductorQSDelayTime, -1, 24000, 100);
						bank.AddSlider("Next fade in (ms)", &sm_GunfightConductorQSFadeInTime, -1, 24000, 100);
					bank.PopGroup();
					bank.PushGroup("Loud sound",false);
						bank.AddToggle("Allow", &sm_StopQSOnLoudSounds);
						bank.AddSlider("Fade out (ms)", &sm_LoudSoundQSFadeOutTime, -1, 24000, 100);
						bank.AddSlider("Next delay (ms)", &sm_LoudSoundQSDelayTime, -1, 24000, 100);
						bank.AddSlider("Next fade in (ms)", &sm_LoudSoundQSFadeInTime, -1, 24000, 100);
					bank.PopGroup();
					bank.PushGroup("On Mission",false);
						bank.AddToggle("Allow stop", &sm_StopQSOnMission);
						bank.AddSlider("Fade out (ms)", &sm_OnMissionQSFadeOutTime, -1, 24000, 100);
						bank.AddSlider("Next delay (ms)", &sm_OnMissionQSDelayTime, -1, 24000, 100);
						bank.AddSlider("Next fade in (ms)", &sm_OnMissionQSFadeInTime, -1, 24000, 100);
					bank.PopGroup();

				bank.PopGroup();
			bank.PopGroup();
		bank.PopGroup();
		audGunFightConductor::AddWidgets(bank);
		audVehicleConductor::AddWidgets(bank);
	bank.PopGroup();
}
#endif
