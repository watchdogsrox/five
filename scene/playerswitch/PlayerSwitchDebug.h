/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    scene/playerswitch/PlayerSwitchDebug.h
// PURPOSE : debug widgets for testing player switch system
// AUTHOR :  Ian Kiigan
// CREATED : 02/10/12
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef _SCENE_PLAYERSWITCH_PLAYERSWITCHDEBUG_H_
#define _SCENE_PLAYERSWITCH_PLAYERSWITCHDEBUG_H_

#if __BANK

#include "bank/bank.h"
#include "bank/bkmgr.h"
#include "scene/playerswitch/PlayerSwitchInterface.h"

class CPlayerSwitchDbg
{
public:
	static void	InitWidgets(bkBank& bank);
	static void Update();

	static void Shutdown(u32 /*shutdownMode*/) { StopSwitchCB(); }

private:
	static void Start(Vec3V_In vDestPos);
	static void StartWithoutDest();
	static void Stop();
	static void Abort();
	static void TestSwitchCB();
	static void TestMPSwitchPt1CB();
	static void TestMPSwitchPt2CB();
	static void StopSwitchCB();
	static void ChangeShortRangeSettingsCB();
	static void ChangeMediumRangeSettingsCB();
	static void ChangeLongRangeSettingsCB();
	static void ChangedControlFlagCB();
	static void SaveSettingsCB();
	static void LoadSettingsCB();
	static void SyncWithCurrentSettings();

	static RegdPed	ms_oldPed;
	static RegdPed	ms_newPed;
	static bool		ms_bSwitching;
	static s32		ms_selectedType;
	static bool		ms_bShowInfo;
	static Vec3V	ms_vStartPos;
	static Vec3V	ms_vEndPos;
	static s32		ms_debugPosIndex;
	static bool		ms_bChangedPlayer;
	static bool		ms_bDestIsKnown;

	static s32		ms_minJumps_LongRange;
	static s32		ms_maxJumps_LongRange;
	static float	ms_panSpeed_LongRange;
	static float	ms_ceilingHeight_LongRange;
	static float	ms_minHeightAboveGround_LongRange;
	static s32		ms_lodLevel_LongRange;
	static float	ms_heightBetweenJumps_LongRange;
	static u32		ms_timeToStayInPedPopStartupModeOnDescent_LongRange;
	static u32		ms_maxTimeToWaitForScenarioAnimsToStreamIn_LongRange;
	static float	ms_radiusOfScenarioAnimsSphereCheck_LongRange;
	
	

	static float	ms_range_MediumRange;
	static s32		ms_minJumps_MediumRange;
	static s32		ms_maxJumps_MediumRange;
	static float	ms_panSpeed_MediumRange;
	static float	ms_ceilingHeight_MediumRange;
	static float	ms_minHeightAboveGround_MediumRange;
	static s32		ms_lodLevel_MediumRange;
	static float	ms_heightBetweenJumps_MediumRange;
	static float	ms_skipPanRange_MediumRange;
	static u32		ms_timeToStayInPedPopStartupModeOnDescent_MediumRange;
	static u32		ms_maxTimeToWaitForScenarioAnimsToStreamIn_MediumRange;
	static float	ms_radiusOfScenarioAnimsSphereCheck_MediumRange;

	static float	ms_range_ShortRange;

	static bool		ms_bFlagSkipIntro;
	static bool		ms_bFlagSkipOutro;
	static bool		ms_bFlagPauseBeforePan;
	static bool		ms_bFlagPauseBeforeOutro;
	static bool		ms_bFlagPauseBeforeAscent;
	static bool		ms_bFlagPauseBeforeDescent;
	static bool		ms_bFlagSkipPan;
	static bool		ms_bFlagUnknownDest;
	static bool		ms_bFlagDescentOnly;
	static bool		ms_bFlagStartFromCamPos;
	static bool		ms_bFlagAllowSniperAimIntro;
	static bool		ms_bFlagAllowSniperAimOutro;
	static bool		ms_bFlagSkipTopDescent;
	static bool		ms_bFlagSupressOutroFX;
	static bool		ms_bFlagSupressIntroFX;
	static bool		ms_bFlagDelayAscentFX;
	static s32		ms_currentEstablishingShot;

	static atArray<const char*> ms_establishingShots;

	static const char* ms_apszSwitchTypes[CPlayerSwitchInterface::SWITCH_TYPE_TOTAL];
	static const char* ms_apszMgrStateNames[CPlayerSwitchMgrLong::STATE_TOTAL];
};

#endif	//__BANK

#endif	//_SCENE_PLAYERSWITCH_PLAYERSWITCHDEBUG_H_