// 
// audio/superconductor.h
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 

#ifndef AUD_SUPERCONDUCTOR_H
#define AUD_SUPERCONDUCTOR_H

#include "vehicleconductor.h"
#include "conductorintensitymap.h"
#include "conductorsutil.h"
#include "gunfightconductor.h"

#include "fwsys/fsm.h"

#if __BANK
#include "bank/bkmgr.h"
#include "bank/bank.h"
#include "bank/combo.h"
#include "bank/slider.h"
#endif

#define USE_CONDUCTORS 1

#if USE_CONDUCTORS
#define CONDUCTORS_ONLY(x)	x
#else
#define CONDUCTORS_ONLY(x)
#endif

class audSuperConductor
{
public:
	// Init 
	void						Init();
	void						ProcessUpdate();
	//Shutdown	
	void						ShutDown();

	//Messages
	void						SendConductorMessage(const ConductorMessageData &messageData);

	audGunFightConductor		&GetGunFightConductor()	{return m_GunFightConductor;}
	audVehicleConductor			&GetVehicleConductor() {return m_VehicleConductor;}
	BANK_ONLY(static void		AddWidgets(bkBank &bank););
	void						SendOrderToConductor(Conductor conductor,ConductorsOrders order);

	// player shot
	static s32				sm_PlayerShotQSFadeOutTime;
	static s32				sm_PlayerShotQSDelayTime;
	static s32				sm_PlayerShotQSFadeInTime;
	// player in vehicle
	static s32				sm_PlayerInVehicleQSFadeOutTime;
	static s32				sm_PlayerInVehicleQSDelayTime;
	static s32				sm_PlayerInVehicleQSFadeInTime;
	// player stealth
	static s32				sm_PlayerInStealthQSFadeOutTime;
	static s32				sm_PlayerInStealthQSDelayTime;
	static s32				sm_PlayerInStealthQSFadeInTime;
	// player in interior
	static s32				sm_PlayerInInteriorQSFadeOutTime;
	static s32				sm_PlayerInInteriorQSDelayTime;
	static s32				sm_PlayerInInteriorQSFadeInTime;
	// player dead
	static s32				sm_PlayerDeadQSFadeOutTime;
	static s32				sm_PlayerDeadQSDelayTime;
	static s32				sm_PlayerDeadQSFadeInTime;
	// player underwater
	static s32				sm_UnderWaterQSFadeOutTime;
	static s32				sm_UnderWaterQSDelayTime;
	static s32				sm_UnderWaterQSFadeInTime;
	// player switch
	static s32				sm_PlayerSwitchQSFadeOutTime;
	static s32				sm_PlayerSwitchDelayTime;
	static s32				sm_PlayerSwitchFadeInTime;
	// gunfight conductor
	static s32				sm_GunfightConductorQSFadeOutTime;
	static s32				sm_GunfightConductorQSDelayTime;
	static s32				sm_GunfightConductorQSFadeInTime;
	// Loud sound
	static s32				sm_LoudSoundQSFadeOutTime;
	static s32				sm_LoudSoundQSDelayTime;
	static s32				sm_LoudSoundQSFadeInTime;
	// On Mission
	static s32				sm_OnMissionQSFadeOutTime;
	static s32				sm_OnMissionQSDelayTime;
	static s32				sm_OnMissionQSFadeInTime;

	static bool				sm_StopQSOnPlayerShot; 
	static bool				sm_StopQSOnPlayerInVehicle;
	static bool				sm_StopQSOnPlayerInStealth; 
	static bool				sm_StopQSOnPlayerInInterior; 
	static bool				sm_StopQSOnPlayerDead; 
	static bool				sm_StopQSOnUnderWater; 
	static bool				sm_StopQSOnPlayerSwitch; 
	static bool				sm_StopQSOnGunfightConductor; 
	static bool				sm_StopQSOnLoudSounds; 
	static bool				sm_StopQSOnMission; 
private:

	enum QuietSceneState
	{
		AUD_QS_STOPPING,
		AUD_QS_PLAYING,
		AUD_QS_WANTS_TO_PLAY,
		AUD_QS_MAX_STATES,
	};
	struct  ConductorsAnalyzedInfo
	{
		ConductorsInfoType info;
		EmphasizeIntensity emphasizeIntensity;
	};
	// Helpers
	void						CheckQSForOnMission();
	void						CheckQSForPlayerDead();
	void						CheckQSForPlayerInInterior();
	void						CheckForPlayerSwitch();
	void						CheckQSForPlayerDrunk();
	void						CheckQSForSpecialAbility();
	void						UpdateQuietScene();
	void						UpdateInfo();
	void						AnalizeInfo();
	void						ProcessInfo();
	void						ProcessMesage(const ConductorMessageData &messageData);
	void						AnalizeConductorInfo(Conductor conductor);
	void						MuteWorld();
	void						UnmuteWorld();
	void						ComputeQsTimesAndStop(const u32 desireFadeOut, const u32 desireDelay, const u32 desireFadeIn);

	ConductorsInfo				UpdateConductorInfo(Conductor conductor);
#if __BANK
	const char* 				GetConductorName(Conductor conductor);
	const char* 				GetInfoType(ConductorsInfoType info);
	void						SuperConductorDebugInfo();
#endif
	// Conductors.
	audGunFightConductor 										m_GunFightConductor;
	audVehicleConductor											m_VehicleConductor;
	// Conductors info
	atRangeArray<ConductorsInfo,MaxNumConductors-1>				m_ConductorsInfo;
	atRangeArray<ConductorsAnalyzedInfo,MaxNumConductors-1>		m_ConductorsAnalyzedInfo;

	audScene *													m_Scene;
	audScene *													m_QuietScene;

	QuietSceneState												m_QuietSceneState;

	f32															m_QuietSceneApply;

	u32															m_QSCurrentFadeOutTime;
	u32															m_QSCurrentDelayTime;
	u32															m_QSCurrentFadeInTime;
	u32															m_TimeInQSState;

	bool														m_WaitForPlayerAliveToPlayQS;
	bool														m_UsingDefaultQSFadeOut;
	bool														m_UsingDefaultQSDelay;
	bool														m_UsingDefaultQSFadeIn;

	static f32													sm_QuietSceneMaxApply;
	static f32													sm_SoftQuietSceneMaxApply;
	static u32													sm_QSDefaultFadeOutTime;
	static u32													sm_QSDefaultDelayTime;
	static u32													sm_QSDefaultFadeInTime;

	BANK_ONLY(static bool 										sm_ShowInfo;);
};
// Make this a singleton
#define SUPERCONDUCTOR (audNorthAudioEngine::GetSuperConductor())
#endif // AUD_SUPERCONDUCTOR_H
