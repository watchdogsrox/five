/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    scene/playerswitch/PlayerSwitchDebug.cpp
// PURPOSE : debug widgets for testing player switch system
// AUTHOR :  Ian Kiigan
// CREATED : 02/10/12
//
/////////////////////////////////////////////////////////////////////////////////

#include "scene/playerswitch/PlayerSwitchDebug.h"

#if __BANK

#include "grcore/debugdraw.h"
#include "peds/ped.h"
#include "peds/PedFactory.h"
#include "peds/pedpopulation.h"
#include "peds/PlayerInfo.h"
#include "peds/rendering/PedVariationDebug.h"
#include "scene/FocusEntity.h"
#include "scene/lod/LodMgr.h"
#include "scene/world/GameWorld.h"

const char* CPlayerSwitchDbg::ms_apszSwitchTypes[] =
{
	"SWITCH_TYPE_AUTO",				//CPlayerSwitchInterface::SWITCH_TYPE_AUTO
	"SWITCH_TYPE_LONG",				//CPlayerSwitchInterface::SWITCH_TYPE_LONG
	"SWITCH_TYPE_MEDIUM",			//CPlayerSwitchInterface::SWITCH_TYPE_MEDIUM
	"SWITCH_TYPE_SHORT"				//CPlayerSwitchInterface::SWITCH_TYPE_SHORT
};

const char* CPlayerSwitchDbg::ms_apszMgrStateNames[] =
{
	"STATE_INTRO",					//CPlayerSwitchMgrLong::STATE_INTRO
	"STATE_PREP_DESCENT",			//CPlayerSwitchMgrLong::STATE_PREP_DESCENT
	"STATE_PREP_FOR_CUT",			//CPlayerSwitchMgrLong::STATE_PREP_FOR_CUT,
	"STATE_JUMPCUT_ASCENT",			//CPlayerSwitchMgrLong::STATE_JUMPCUT_ASCENT
	"STATE_WAITFORINPUT_INTRO",		//CPlayerSwitchMgrLong::STATE_WAITFORINPUT_INTRO
	"STATE_WAITFORINPUT",			//CPlayerSwitchMgrLong::STATE_WAITFORINPUT
	"STATE_WAITFORINPUT_OUTRO",		//CPlayerSwitchMgrLong::STATE_WAITFORINPUT_INTRO
	"STATE_PAN",					//CPlayerSwitchMgrLong::STATE_PAN
	"STATE_JUMPCUT_DESCENT",		//CPlayerSwitchMgrLong::STATE_JUMPCUT_DESCENT
	"STATE_OUTRO_HOLD",				//CPlayerSwitchMgrLong::STATE_OUTRO_HOLD
	"STATE_OUTRO_SWOOP",			//CPlayerSwitchMgrLong::STATE_OUTRO_SWOOP
	"STATE_ESTABLISHING_SHOT",		//CPlayerSwitchMgrLong::STATE_ESTABLISHING_SHOT
	"STATE_FINISHED"				//CPlayerSwitchMgrLong::STATE_FINISHED
};

Vec3V		CPlayerSwitchDbg::ms_vStartPos;
Vec3V		CPlayerSwitchDbg::ms_vEndPos;

bool		CPlayerSwitchDbg::ms_bSwitching		= false;
bool		CPlayerSwitchDbg::ms_bChangedPlayer	= false;
bool		CPlayerSwitchDbg::ms_bDestIsKnown	= false;
bool		CPlayerSwitchDbg::ms_bShowInfo		= false;
s32			CPlayerSwitchDbg::ms_debugPosIndex	= 0;
s32			CPlayerSwitchDbg::ms_selectedType	= 0;
RegdPed		CPlayerSwitchDbg::ms_oldPed;
RegdPed		CPlayerSwitchDbg::ms_newPed;

bool		CPlayerSwitchDbg::ms_bFlagSkipIntro = false;
bool		CPlayerSwitchDbg::ms_bFlagSkipOutro = false;
bool		CPlayerSwitchDbg::ms_bFlagPauseBeforePan = false;
bool		CPlayerSwitchDbg::ms_bFlagPauseBeforeOutro = false;
bool		CPlayerSwitchDbg::ms_bFlagPauseBeforeAscent = false;
bool		CPlayerSwitchDbg::ms_bFlagPauseBeforeDescent = false;
bool		CPlayerSwitchDbg::ms_bFlagSkipPan = false;
bool		CPlayerSwitchDbg::ms_bFlagUnknownDest = false;
bool		CPlayerSwitchDbg::ms_bFlagDescentOnly = false;
bool		CPlayerSwitchDbg::ms_bFlagStartFromCamPos = false;
bool		CPlayerSwitchDbg::ms_bFlagAllowSniperAimIntro = false;
bool		CPlayerSwitchDbg::ms_bFlagAllowSniperAimOutro = false;
bool		CPlayerSwitchDbg::ms_bFlagSkipTopDescent = false;
bool		CPlayerSwitchDbg::ms_bFlagSupressOutroFX = false;
bool		CPlayerSwitchDbg::ms_bFlagSupressIntroFX = false;
bool		CPlayerSwitchDbg::ms_bFlagDelayAscentFX = false;

s32			CPlayerSwitchDbg::ms_minJumps_LongRange = 0;
s32			CPlayerSwitchDbg::ms_maxJumps_LongRange = 0;
float		CPlayerSwitchDbg::ms_panSpeed_LongRange = 0.0f;
float		CPlayerSwitchDbg::ms_ceilingHeight_LongRange = 0.0f;
float		CPlayerSwitchDbg::ms_minHeightAboveGround_LongRange = 0.0f;
s32			CPlayerSwitchDbg::ms_lodLevel_LongRange = 0;
float		CPlayerSwitchDbg::ms_heightBetweenJumps_LongRange = 0.0f;
u32			CPlayerSwitchDbg::ms_timeToStayInPedPopStartupModeOnDescent_LongRange = 2200;
u32			CPlayerSwitchDbg::ms_maxTimeToWaitForScenarioAnimsToStreamIn_LongRange = 2400;
float		CPlayerSwitchDbg::ms_radiusOfScenarioAnimsSphereCheck_LongRange = 20.0f;


float		CPlayerSwitchDbg::ms_range_MediumRange = 0.0f;
s32			CPlayerSwitchDbg::ms_minJumps_MediumRange = 0;
s32			CPlayerSwitchDbg::ms_maxJumps_MediumRange = 0;
float		CPlayerSwitchDbg::ms_panSpeed_MediumRange = 0.0f;
float		CPlayerSwitchDbg::ms_ceilingHeight_MediumRange = 0.0f;
float		CPlayerSwitchDbg::ms_minHeightAboveGround_MediumRange = 0.0f;
s32			CPlayerSwitchDbg::ms_lodLevel_MediumRange = 0;
float		CPlayerSwitchDbg::ms_heightBetweenJumps_MediumRange = 0.0f;
float		CPlayerSwitchDbg::ms_skipPanRange_MediumRange = 0.0f;
u32			CPlayerSwitchDbg::ms_timeToStayInPedPopStartupModeOnDescent_MediumRange = 1600;
u32			CPlayerSwitchDbg::ms_maxTimeToWaitForScenarioAnimsToStreamIn_MediumRange = 1850;
float		CPlayerSwitchDbg::ms_radiusOfScenarioAnimsSphereCheck_MediumRange = 15.0f;

float		CPlayerSwitchDbg::ms_range_ShortRange = 0.0f;

atArray<const char*> CPlayerSwitchDbg::ms_establishingShots;
s32			CPlayerSwitchDbg::ms_currentEstablishingShot = 0;

static		bkCombo* pEstablishingShotCombo(NULL);

#define NUM_DEBUG_POSITIONS	(17)
Vec3V avDebugPositions[NUM_DEBUG_POSITIONS] =
{
	Vec3V(1428.1f, 6603.4f, 10.8f),			// north tip of the map
	Vec3V(-1521.2f, -3295.5f, 14.4f),		// south tip of the map
	Vec3V(-230.52f, 6156.41f, 31.22f),		// rural bank apartment
	Vec3V(-1215.72f, 5388.72f, 2.61f),		// beach
	Vec3V(-2063.67f, 2991.56f, 32.82f),		// airfield
	Vec3V(223.14f, 3169.76f, 42.36f),		// shack
	Vec3V(-410.00f, 1060.24f, 323.86f),		// observatory
	Vec3V(-189.81f, -632.81f, 48.68f),		// downtown
	Vec3V(127.3f, -732.0f, 262.9f),			// FBI building
	Vec3V(-837.2f, -78.5f, 42.7f),			// magdemo - rockford hills
	Vec3V(1970.7f, 3819.0f, 33.4f),			// magdemo - trevor's trailer
	Vec3V(-1175.3f, -1573.8f, 4.4f),		// magdemo - muscle beach
	Vec3V(1425.1f, -1816.5f, 68.6f),		// magdemo - michael driving
	Vec3V(-564.3f, 5202.3f, 92.2f),			// test snipe
	Vec3V(-500.0, 5287.1f, 80.7f),			// test gas tank
	Vec3V(-1153.5f,-1370.7f,5.1f),			// Beach Carpark
	Vec3V(44.7f, -810.9f, 22.8f)			// Tunnel

};

const char* apszPosNames[NUM_DEBUG_POSITIONS] =
{
	"North tip",
	"South tip",
	"Rural Bank Apartment",
	"Beach",
	"Air field",
	"Shack",
	"Observatory",
	"Downtown",
	"FBI Building",
	"MD: Rockford Hills",
	"MD: Trevor",
	"MD: Muscle beach",
	"MD: Michael",
	"Test snipe",
	"Test gastank",
	"Beach Carpark",
	"Tunnel"

};

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	InitWidgets
// PURPOSE:		adds debug widgets for testing player switch
//////////////////////////////////////////////////////////////////////////
void CPlayerSwitchDbg::InitWidgets(rage::bkBank &bank)
{
	SyncWithCurrentSettings();

	bank.PushGroup("Player Switch");
	{
		bank.AddToggle("Show debug info", &ms_bShowInfo);
		bank.AddSeparator("break");

		bank.AddCombo("Type", &ms_selectedType, CPlayerSwitchInterface::SWITCH_TYPE_TOTAL, &ms_apszSwitchTypes[0]);
		bank.AddCombo("Test pos", &ms_debugPosIndex, NUM_DEBUG_POSITIONS, &apszPosNames[0]);

		bank.AddToggle("FLAG_SKIP_INTRO", &ms_bFlagSkipIntro, ChangedControlFlagCB);
		bank.AddToggle("FLAG_SKIP_OUTRO", &ms_bFlagSkipOutro, ChangedControlFlagCB);
		bank.AddToggle("FLAG_PAUSE_BEFORE_PAN", &ms_bFlagPauseBeforePan, ChangedControlFlagCB);
		bank.AddToggle("FLAG_PAUSE_BEFORE_OUTRO", &ms_bFlagPauseBeforeOutro, ChangedControlFlagCB);
		bank.AddToggle("FLAG_PAUSE_BEFORE_ASCENT", &ms_bFlagPauseBeforeAscent, ChangedControlFlagCB);
		bank.AddToggle("FLAG_PAUSE_BEFORE_DESCENT", &ms_bFlagPauseBeforeDescent, ChangedControlFlagCB);
		bank.AddToggle("FLAG_SKIP_PAN", &ms_bFlagSkipPan, ChangedControlFlagCB);
		bank.AddToggle("FLAG_UNKNOWN_DEST", &ms_bFlagUnknownDest, ChangedControlFlagCB);
		bank.AddToggle("FLAG_DESCENT_ONLY", &ms_bFlagDescentOnly, ChangedControlFlagCB);
		bank.AddToggle("FLAG_START_FROM_CAMPOS", &ms_bFlagStartFromCamPos, ChangedControlFlagCB);
		bank.AddToggle("FLAG_ALLOW_SNIPER_AIM_INTRO", &ms_bFlagAllowSniperAimIntro, ChangedControlFlagCB);
		bank.AddToggle("FLAG_ALLOW_SNIPER_AIM_OUTRO", &ms_bFlagAllowSniperAimOutro, ChangedControlFlagCB);
		bank.AddToggle("FLAG_SKIP_TOP_DESCENT", &ms_bFlagSkipTopDescent, ChangedControlFlagCB);
		bank.AddToggle("FLAG_SUPPRESS_OUTRO_FX", &ms_bFlagSupressOutroFX, ChangedControlFlagCB);
		bank.AddToggle("FLAG_SUPPRESS_INTRO_FX", &ms_bFlagSupressIntroFX, ChangedControlFlagCB);
		bank.AddToggle("FLAG_DELAY_ASCENT_FX", &ms_bFlagDelayAscentFX, ChangedControlFlagCB);

		bank.AddButton("Start Switch", TestSwitchCB);
		bank.AddButton("Start MP Switch Pt1", TestMPSwitchPt1CB);
		bank.AddButton("Start MP Switch Pt2", TestMPSwitchPt2CB);
		bank.AddButton("Stop Switch", StopSwitchCB);
		bank.AddSeparator("break");

		pEstablishingShotCombo = bank.AddCombo("Establishing shot", &ms_currentEstablishingShot, ms_establishingShots.GetCount(), &ms_establishingShots[0]);
		bank.AddSeparator("break");

		bank.PushGroup("Long range settings");
		{
			bank.AddSlider("Num jump cuts (min)", &ms_minJumps_LongRange, 1, 10, 1, ChangeLongRangeSettingsCB);
			bank.AddSlider("Num jump cuts (max)", &ms_maxJumps_LongRange, 1, 10, 1, ChangeLongRangeSettingsCB);
			bank.AddSlider("Pan speed", &ms_panSpeed_LongRange, 1.0f, 5000.0f, 0.1f, ChangeLongRangeSettingsCB);
			bank.AddSlider("Min Ceiling Height", &ms_ceilingHeight_LongRange, 1.0f, 2000.0f, 0.1f, ChangeLongRangeSettingsCB);
			bank.AddSlider("Min Height Above Ground", &ms_minHeightAboveGround_LongRange, 1.0f, 2000.0f, 0.1f, ChangeLongRangeSettingsCB);
			bank.AddCombo("Lod Level", &ms_lodLevel_LongRange, LODTYPES_DEPTH_TOTAL, &fwLodData::ms_apszLevelNames[0], ChangeLongRangeSettingsCB);
			bank.AddSlider("Height between jumps", &ms_heightBetweenJumps_LongRange, 1.0f, 1000.0f, 0.1f, ChangeLongRangeSettingsCB);
			bank.AddSlider("Max time to stay in PedPop StartupMode during Descent", &ms_timeToStayInPedPopStartupModeOnDescent_LongRange, 0, 10000, 1, ChangeLongRangeSettingsCB);
			bank.AddSlider("Max time to wait for scenario peds to stream anims", &ms_maxTimeToWaitForScenarioAnimsToStreamIn_LongRange, 0, 10000, 1, ChangeLongRangeSettingsCB);			
			bank.AddSlider("Radius of scenario ped streaming check", &ms_radiusOfScenarioAnimsSphereCheck_LongRange, 0.0f, 10000.0f, 0.00001f, ChangeLongRangeSettingsCB);
		}
		bank.PopGroup();

		bank.PushGroup("Medium range settings");
		{
			bank.AddSlider("Range", &ms_range_MediumRange, 1, 1000, 1, ChangeMediumRangeSettingsCB);
			bank.AddSlider("Num jump cuts (min)", &ms_minJumps_MediumRange, 1, 10, 1, ChangeMediumRangeSettingsCB);
			bank.AddSlider("Num jump cuts (max)", &ms_maxJumps_MediumRange, 1, 10, 1, ChangeMediumRangeSettingsCB);
			bank.AddSlider("Pan speed", &ms_panSpeed_MediumRange, 1.0f, 5000.0f, 0.1f, ChangeMediumRangeSettingsCB);
			bank.AddSlider("Min Ceiling Height", &ms_ceilingHeight_MediumRange, 1.0f, 2000.0f, 1.0f, ChangeMediumRangeSettingsCB);
			bank.AddSlider("Min Height Above Ground", &ms_minHeightAboveGround_MediumRange, 1.0f, 2000.0f, 0.1f, ChangeMediumRangeSettingsCB);
			bank.AddCombo("Lod Level", &ms_lodLevel_MediumRange, LODTYPES_DEPTH_TOTAL, &fwLodData::ms_apszLevelNames[0], ChangeMediumRangeSettingsCB);
			bank.AddSlider("Height between jumps", &ms_heightBetweenJumps_MediumRange, 1.0f, 1000.0f, 0.1f, ChangeMediumRangeSettingsCB);
			bank.AddSlider("Range to auto-skip pan", &ms_skipPanRange_MediumRange, 1.0f, 1000.0f, 0.1f, ChangeMediumRangeSettingsCB);
			bank.AddSlider("Max time to stay in PedPop StartupMode during Descent", &ms_timeToStayInPedPopStartupModeOnDescent_MediumRange, 0, 10000, 1, ChangeMediumRangeSettingsCB);
			bank.AddSlider("Max time to wait for scenario peds to stream anims", &ms_maxTimeToWaitForScenarioAnimsToStreamIn_MediumRange, 0, 10000, 1, ChangeMediumRangeSettingsCB);			
			bank.AddSlider("Radius of scenario ped streaming check", &ms_radiusOfScenarioAnimsSphereCheck_MediumRange, 0.0f, 10000.0f, 0.00001f, ChangeMediumRangeSettingsCB);
		}
		bank.PopGroup();

		bank.PushGroup("Short range settings");
		{
			bank.AddSlider("Range", &ms_range_ShortRange, 1, 100, 1, ChangeShortRangeSettingsCB);
		}
		bank.PopGroup();

		bank.AddSeparator("");
		bank.AddTitle(PLAYERSWITCH_SETTINGS_FILE);
		bank.AddButton("Save settings", datCallback(SaveSettingsCB));
		bank.AddButton("Load settings", datCallback(LoadSettingsCB));
	}
	bank.PopGroup();
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Start
// PURPOSE:		starts a player switch to the specified ped
//////////////////////////////////////////////////////////////////////////
void CPlayerSwitchDbg::Start(Vec3V_In vDestPos)
{
	if (!ms_bSwitching)
	{
		CPed* pPlayerPed = FindPlayerPed();
		if (pPlayerPed)
		{
			//CPedVariationDebug::CycleTypeId();
			CPed::CreateBank();
			CPedVariationDebug::CreatePedCB();

			ms_vStartPos = pPlayerPed->GetTransform().GetPosition();
			ms_vEndPos = vDestPos;

			// create new ped, teleport it to desired location
			ms_oldPed = pPlayerPed;
			ms_newPed = CPedFactory::GetLastCreatedPed();
			ms_newPed->PopTypeSet(POPTYPE_MISSION);
			ms_newPed->Teleport(VEC3V_TO_VECTOR3(ms_vEndPos), 0.0f, false, true);

			// freeze both peds
			ms_oldPed->DisableCollision(NULL, true);
			ms_oldPed->SetFixedPhysics(true);
			ms_newPed->DisableCollision(NULL, true);
			ms_newPed->SetFixedPhysics(true);

			// construct control flags
			s32 controlFlags = 0;
			if (ms_bFlagSkipIntro)
			{
				controlFlags |= CPlayerSwitchInterface::CONTROL_FLAG_SKIP_INTRO;
			}
			if (ms_bFlagSkipOutro)
			{
				controlFlags |= CPlayerSwitchInterface::CONTROL_FLAG_SKIP_OUTRO;
			}
			if (ms_bFlagPauseBeforePan)
			{
				controlFlags |= CPlayerSwitchInterface::CONTROL_FLAG_PAUSE_BEFORE_PAN;
			}
			if (ms_bFlagPauseBeforeOutro)
			{
				controlFlags |= CPlayerSwitchInterface::CONTROL_FLAG_PAUSE_BEFORE_OUTRO;
			}
			if (ms_bFlagPauseBeforeAscent)
			{
				controlFlags |= CPlayerSwitchInterface::CONTROL_FLAG_PAUSE_BEFORE_ASCENT;
			}
			if (ms_bFlagPauseBeforeDescent)
			{
				controlFlags |= CPlayerSwitchInterface::CONTROL_FLAG_PAUSE_BEFORE_DESCENT;
			}
			if (ms_bFlagSkipPan)
			{
				controlFlags |= CPlayerSwitchInterface::CONTROL_FLAG_SKIP_PAN;
			}
			if (ms_bFlagUnknownDest)
			{
				controlFlags |= CPlayerSwitchInterface::CONTROL_FLAG_UNKNOWN_DEST;
			}
			if (ms_bFlagDescentOnly)
			{
				controlFlags |= CPlayerSwitchInterface::CONTROL_FLAG_DESCENT_ONLY;
			}
			if (ms_bFlagStartFromCamPos)
			{
				controlFlags |= CPlayerSwitchInterface::CONTROL_FLAG_START_FROM_CAMPOS;
			}
			if (ms_bFlagAllowSniperAimIntro)
			{
				controlFlags |= CPlayerSwitchInterface::CONTROL_FLAG_ALLOW_SNIPER_AIM_INTRO;
			}
			if (ms_bFlagAllowSniperAimOutro)
			{
				controlFlags |= CPlayerSwitchInterface::CONTROL_FLAG_ALLOW_SNIPER_AIM_OUTRO;
			}
			if (ms_bFlagSkipTopDescent)
			{
				controlFlags |= CPlayerSwitchInterface::CONTROL_FLAG_SKIP_TOP_DESCENT;
			}
			if (ms_bFlagSupressOutroFX)
			{
				controlFlags |= CPlayerSwitchInterface::CONTROL_FLAG_SUPPRESS_OUTRO_FX;
			}
			if (ms_bFlagSupressIntroFX)
			{
				controlFlags |= CPlayerSwitchInterface::CONTROL_FLAG_SUPPRESS_INTRO_FX;
			}
			if (ms_bFlagDelayAscentFX)
			{
				controlFlags |= CPlayerSwitchInterface::CONTROL_FLAG_DELAY_ASCENT_FX;
			}

			// start switch
			g_PlayerSwitch.Start(ms_selectedType, *ms_oldPed, *ms_newPed, controlFlags);

			ms_bChangedPlayer = false;
			ms_bSwitching = true;
			ms_bDestIsKnown = true;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	StartWithoutDest
// PURPOSE:		start a switch where the dest is not yet known
//////////////////////////////////////////////////////////////////////////
void CPlayerSwitchDbg::StartWithoutDest()
{
	if (!ms_bSwitching)
	{
		CPed* pPlayerPed = FindPlayerPed();
		if (pPlayerPed)
		{
			ms_vStartPos = pPlayerPed->GetTransform().GetPosition();
			ms_vEndPos = ms_vStartPos;

			ms_oldPed = pPlayerPed;
			ms_newPed = pPlayerPed;

			// freeze ped
			ms_oldPed->DisableCollision(NULL, true);
			ms_oldPed->SetFixedPhysics(true);
		
			s32 controlFlags = 0;
			if (ms_bFlagSkipIntro)
			{
				controlFlags |= CPlayerSwitchInterface::CONTROL_FLAG_SKIP_INTRO;
			}
			if (ms_bFlagSkipOutro)
			{
				controlFlags |= CPlayerSwitchInterface::CONTROL_FLAG_SKIP_OUTRO;
			}
			if (ms_bFlagPauseBeforePan)
			{
				controlFlags |= CPlayerSwitchInterface::CONTROL_FLAG_PAUSE_BEFORE_PAN;
			}
			if (ms_bFlagPauseBeforeOutro)
			{
				controlFlags |= CPlayerSwitchInterface::CONTROL_FLAG_PAUSE_BEFORE_OUTRO;
			}
			if (ms_bFlagPauseBeforeAscent)
			{
				controlFlags |= CPlayerSwitchInterface::CONTROL_FLAG_PAUSE_BEFORE_ASCENT;
			}
			if (ms_bFlagPauseBeforeDescent)
			{
				controlFlags |= CPlayerSwitchInterface::CONTROL_FLAG_PAUSE_BEFORE_DESCENT;
			}
			if (ms_bFlagSkipPan)
			{
				controlFlags |= CPlayerSwitchInterface::CONTROL_FLAG_SKIP_PAN;
			}
			if (ms_bFlagUnknownDest)
			{
				controlFlags |= CPlayerSwitchInterface::CONTROL_FLAG_UNKNOWN_DEST;
			}
			if (ms_bFlagDescentOnly)
			{
				controlFlags |= CPlayerSwitchInterface::CONTROL_FLAG_DESCENT_ONLY;
			}
			if (ms_bFlagStartFromCamPos)
			{
				controlFlags |= CPlayerSwitchInterface::CONTROL_FLAG_START_FROM_CAMPOS;
			}
			if (ms_bFlagAllowSniperAimIntro)
			{
				controlFlags |= CPlayerSwitchInterface::CONTROL_FLAG_ALLOW_SNIPER_AIM_INTRO;
			}
			if (ms_bFlagAllowSniperAimOutro)
			{
				controlFlags |= CPlayerSwitchInterface::CONTROL_FLAG_ALLOW_SNIPER_AIM_OUTRO;
			}
			if (ms_bFlagSkipTopDescent)
			{
				controlFlags |= CPlayerSwitchInterface::CONTROL_FLAG_SKIP_TOP_DESCENT;
			}
			if (ms_bFlagSupressOutroFX)
			{
				controlFlags |= CPlayerSwitchInterface::CONTROL_FLAG_SUPPRESS_OUTRO_FX;
			}
			if (ms_bFlagSupressIntroFX)
			{
				controlFlags |= CPlayerSwitchInterface::CONTROL_FLAG_SUPPRESS_INTRO_FX;
			}
			if (ms_bFlagDelayAscentFX)
			{
				controlFlags |= CPlayerSwitchInterface::CONTROL_FLAG_DELAY_ASCENT_FX;
			}

			// start switch
			g_PlayerSwitch.Start(ms_selectedType, *ms_oldPed, *ms_oldPed, controlFlags | CPlayerSwitchInterface::CONTROL_FLAG_UNKNOWN_DEST);

			ms_bChangedPlayer = false;
			ms_bSwitching = true;
			ms_bDestIsKnown = false;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Update
// PURPOSE:		updates the switch, if active, and checks for completion
//////////////////////////////////////////////////////////////////////////
void CPlayerSwitchDbg::Update()
{
	// display debug info
	if ( ms_bShowInfo && g_PlayerSwitch.IsActive() )
	{
		s32 switchType = g_PlayerSwitch.GetSwitchType();
		const CPlayerSwitchMgrBase* pMgr = g_PlayerSwitch.GetMgr(switchType);

		// calculate distance between start and end
		Vec3V vStart = pMgr->GetStartPos();
		Vec3V vDest = pMgr->GetDestPos();
		const ScalarV dist2d = DistXYFast(vStart, vDest);
		const ScalarV dist3d = Dist(vStart, vDest);

		// display type of switch and distance covered
		grcDebugDraw::AddDebugOutput(
			"Active switch %s from (%4.2f, %4.2f, %4.2f) to (%4.2f, %4.2f, %4.2f)",
			ms_apszSwitchTypes[switchType],
			vStart.GetXf(), vStart.GetYf(), vStart.GetZf(),
			vDest.GetXf(), vDest.GetYf(), vDest.GetZf() );
		grcDebugDraw::AddDebugOutput("... a distance of %4.2fm (3D) or %4.2fm (2D)", dist3d.Getf(),	dist2d.Getf());

		// params
		const CPlayerSwitchParams params = pMgr->GetParams();
		grcDebugDraw::AddDebugOutput("PARAM: numJumpsUp=%d", params.m_numJumpsAscent);
		grcDebugDraw::AddDebugOutput("PARAM: numJumpsDown=%d", params.m_numJumpsDescent);
		grcDebugDraw::AddDebugOutput("PARAM: panSpeed=%d", params.m_fPanSpeed);
		grcDebugDraw::AddDebugOutput("PARAM: ceilingHeight=%d", params.m_fCeilingHeight);
		grcDebugDraw::AddDebugOutput("PARAM: ceilingLodLevel=%s", fwLodData::ms_apszLevelNames[params.m_ceilingLodLevel]);
		grcDebugDraw::AddDebugOutput("PARAM: controlFlags:0x%x", params.m_controlFlags);

		switch (switchType)
		{
		case CPlayerSwitchInterface::SWITCH_TYPE_LONG:
		case CPlayerSwitchInterface::SWITCH_TYPE_MEDIUM:
			{
				const CPlayerSwitchMgrLong* pLongSwitchMgr = (CPlayerSwitchMgrLong*) pMgr;
				u32 currentMgrState = pLongSwitchMgr->GetState();
				grcDebugDraw::AddDebugOutput("Current switch manager state=%s, currentJump=%d", ms_apszMgrStateNames[currentMgrState], pLongSwitchMgr->m_currentState.GetJumpCutIndex());

				if (pLongSwitchMgr->m_streamer.IsStreaming())
				{
					s32 streamingState = pLongSwitchMgr->m_streamer.GetState();
					grcDebugDraw::AddDebugOutput("Streaming for state=%s, currentJump=%d", ms_apszMgrStateNames[streamingState], pLongSwitchMgr->m_streamer.GetCutIndex());
					if (pLongSwitchMgr->m_streamer.CanTimeout())
					{
						grcDebugDraw::AddDebugOutput("Streaming timeout=%d", pLongSwitchMgr->m_streamer.GetTimeout());
					}
				}

				if (pLongSwitchMgr->m_pEstablishingShotData)
				{
					grcDebugDraw::AddDebugOutput("Establishing shot: %s", pLongSwitchMgr->m_pEstablishingShotData->m_Name.GetCStr());
				}
			}
			break;

		case CPlayerSwitchInterface::SWITCH_TYPE_SHORT:
			break;

		default:
			break;
		}
		
		grcDebugDraw::AddDebugOutput("SLOD mode: %s", (g_LodMgr.IsSlodModeActive() ? "Y" : "N") ) ;

	}

	// check for debug-initiated switch completion
	if ( ms_bSwitching )
	{
		if (!ms_bChangedPlayer && !CFocusEntityMgr::GetMgr().IsPlayerPed() && ms_bDestIsKnown)
		{
			CGameWorld::ChangePlayerPed(*ms_oldPed, *ms_newPed);
			ms_bChangedPlayer = true;
		}

		if (!g_PlayerSwitch.IsActive())
		{
			if (!ms_bChangedPlayer)
			{
				CGameWorld::ChangePlayerPed(*ms_oldPed, *ms_newPed);
				ms_bChangedPlayer = true;
			}
			Stop();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Stop
// PURPOSE:		end active switch, sets player control to new ped
//////////////////////////////////////////////////////////////////////////
void CPlayerSwitchDbg::Stop()
{
	if (ms_bSwitching)
	{
		Assert(ms_newPed);

		ms_oldPed->EnableCollision();
		ms_oldPed->SetFixedPhysics(false);
		CPedFactory::GetFactory()->DestroyPed(ms_oldPed);

		if (ms_newPed)
		{
		    ms_newPed->EnableCollision();
		    ms_newPed->SetFixedPhysics(false);
		}

		ms_bSwitching = false;
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Abort
// PURPOSE:
//////////////////////////////////////////////////////////////////////////
void CPlayerSwitchDbg::Abort()
{
	if (ms_bSwitching)
	{
		if (ms_oldPed)
		{
			ms_oldPed->EnableCollision();
			ms_oldPed->SetFixedPhysics(false);

			CPed* pPlayerPed = FindPlayerPed();
			if (pPlayerPed && pPlayerPed!=ms_oldPed)
			{
				CGameWorld::ChangePlayerPed(*pPlayerPed, *ms_oldPed);

				if (ms_newPed && ms_newPed!=ms_oldPed)
				{
					ms_newPed->EnableCollision();
					ms_newPed->SetFixedPhysics(false);
					CPedFactory::GetFactory()->DestroyPed(ms_newPed);
				}
			}
		}
		ms_bSwitching = false;
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	TestSwitchCB
// PURPOSE:		move ped and perform switch, for debugging switcher
//////////////////////////////////////////////////////////////////////////
void CPlayerSwitchDbg::TestSwitchCB()
{
	const Vec3V vPos = avDebugPositions[ms_debugPosIndex];
	if (!ms_bSwitching)
	{
		Start(vPos);

		if (ms_currentEstablishingShot && ms_bSwitching && g_PlayerSwitch.GetSwitchType()!=CPlayerSwitchInterface::SWITCH_TYPE_SHORT)
		{
			// set establishing shot
			atHashString shotName = CPlayerSwitchEstablishingShot::ms_store.m_ShotList[ms_currentEstablishingShot-1].m_Name;
			g_PlayerSwitch.GetLongRangeMgr().SetEstablishingShot(shotName);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	TestMPSwitchPt1CB
// PURPOSE:		start ascent part of switch, for transition to MP (unknown dest)
//////////////////////////////////////////////////////////////////////////
void CPlayerSwitchDbg::TestMPSwitchPt1CB()
{
	if (!ms_bSwitching)
	{
		StartWithoutDest();

		if (ms_currentEstablishingShot && ms_bSwitching && g_PlayerSwitch.GetSwitchType()!=CPlayerSwitchInterface::SWITCH_TYPE_SHORT)
		{
			// set establishing shot
			atHashString shotName = CPlayerSwitchEstablishingShot::ms_store.m_ShotList[ms_currentEstablishingShot-1].m_Name;
			g_PlayerSwitch.GetLongRangeMgr().SetEstablishingShot(shotName);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	TestMPSwitchPt2CB
// PURPOSE:		start pan and descent part of switch, for transition to MP
//////////////////////////////////////////////////////////////////////////
void CPlayerSwitchDbg::TestMPSwitchPt2CB()
{
	if (ms_bSwitching && !ms_bDestIsKnown)
	{
		ms_vEndPos = avDebugPositions[ms_debugPosIndex];

		CPed::CreateBank();
		CPedVariationDebug::CreatePedCB();
		ms_newPed = CPedFactory::GetLastCreatedPed();
		ms_newPed->PopTypeSet(POPTYPE_MISSION);
		ms_newPed->Teleport(VEC3V_TO_VECTOR3(ms_vEndPos), 0.0f, false, true);
		ms_newPed->DisableCollision(NULL, true);
		ms_newPed->SetFixedPhysics(true);

		g_PlayerSwitch.SetDest(*ms_newPed);

		ms_bDestIsKnown = true;
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	StopSwitchCB
// PURPOSE:		abort active switch
//////////////////////////////////////////////////////////////////////////
void CPlayerSwitchDbg::StopSwitchCB()
{
	if (ms_bSwitching)
	{
		g_PlayerSwitch.Stop();
		Abort();
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	ChangeShortRangeSettingsCB
// PURPOSE:		write back changed settings for short range switch
//////////////////////////////////////////////////////////////////////////
void CPlayerSwitchDbg::ChangeShortRangeSettingsCB()
{
	CPlayerSwitchSettings& shortRangeSettings = g_PlayerSwitch.GetSettings(CPlayerSwitchInterface::SWITCH_TYPE_SHORT);
	shortRangeSettings.SetRange(ms_range_ShortRange);
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	ChangeMediumRangeSettingsCB
// PURPOSE:		write back changed settings for medium range switch
//////////////////////////////////////////////////////////////////////////
void CPlayerSwitchDbg::ChangeMediumRangeSettingsCB()
{
	CPlayerSwitchSettings& mediumRangeSettings = g_PlayerSwitch.GetSettings(CPlayerSwitchInterface::SWITCH_TYPE_MEDIUM);

	mediumRangeSettings.SetRange(ms_range_MediumRange);
	mediumRangeSettings.SetNumJumpsMin(ms_minJumps_MediumRange);
	mediumRangeSettings.SetNumJumpsMax(ms_maxJumps_MediumRange);
	mediumRangeSettings.SetCeilingLodLevel(ms_lodLevel_MediumRange);
	mediumRangeSettings.SetCeilingHeight(ms_ceilingHeight_MediumRange);
	mediumRangeSettings.SetPanSpeed(ms_panSpeed_MediumRange);
	mediumRangeSettings.SetMinHeightAbove(ms_minHeightAboveGround_MediumRange);
	mediumRangeSettings.SetHeightBetweenJumps(ms_heightBetweenJumps_MediumRange);
	mediumRangeSettings.SetSkipPanRange(ms_skipPanRange_MediumRange);
	mediumRangeSettings.SetMaxTimeToStayInStartupModeOnLongRangeDescent(ms_timeToStayInPedPopStartupModeOnDescent_MediumRange);
	mediumRangeSettings.SetScenarioAnimsTimeout(ms_maxTimeToWaitForScenarioAnimsToStreamIn_MediumRange);
	mediumRangeSettings.SetRadiusOfScenarioAnimsCheck(ms_radiusOfScenarioAnimsSphereCheck_MediumRange);
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	CahngeLongRangeSettingsCB
// PURPOSE:		write back changed settings for long range switch
//////////////////////////////////////////////////////////////////////////
void CPlayerSwitchDbg::ChangeLongRangeSettingsCB()
{
	CPlayerSwitchSettings& longRangeSettings = g_PlayerSwitch.GetSettings(CPlayerSwitchInterface::SWITCH_TYPE_LONG);

	longRangeSettings.SetNumJumpsMin(ms_minJumps_LongRange);
	longRangeSettings.SetNumJumpsMax(ms_maxJumps_LongRange);
	longRangeSettings.SetCeilingLodLevel(ms_lodLevel_LongRange);
	longRangeSettings.SetCeilingHeight(ms_ceilingHeight_LongRange);
	longRangeSettings.SetPanSpeed(ms_panSpeed_LongRange);
	longRangeSettings.SetMinHeightAbove(ms_minHeightAboveGround_LongRange);
	longRangeSettings.SetHeightBetweenJumps(ms_heightBetweenJumps_LongRange);
	longRangeSettings.SetMaxTimeToStayInStartupModeOnLongRangeDescent(ms_timeToStayInPedPopStartupModeOnDescent_LongRange);
	longRangeSettings.SetRadiusOfScenarioAnimsCheck(ms_radiusOfScenarioAnimsSphereCheck_LongRange);
	longRangeSettings.SetScenarioAnimsTimeout(ms_maxTimeToWaitForScenarioAnimsToStreamIn_LongRange);
}


//////////////////////////////////////////////////////////////////////////
// FUNCTION:	ChangedControlFlagCB
// PURPOSE:		one of the control flags has changed, update mgr flags if required
//////////////////////////////////////////////////////////////////////////
void CPlayerSwitchDbg::ChangedControlFlagCB()
{
	if (g_PlayerSwitch.IsActive())
	{
		s32 switchType = g_PlayerSwitch.GetSwitchType();
		CPlayerSwitchMgrBase* pMgr = g_PlayerSwitch.GetMgr(switchType);
		if (pMgr)
		{		
			pMgr->SetControlFlag(CPlayerSwitchInterface::CONTROL_FLAG_SKIP_INTRO, ms_bFlagSkipIntro);
			pMgr->SetControlFlag(CPlayerSwitchInterface::CONTROL_FLAG_SKIP_OUTRO, ms_bFlagSkipOutro);
			pMgr->SetControlFlag(CPlayerSwitchInterface::CONTROL_FLAG_PAUSE_BEFORE_PAN, ms_bFlagPauseBeforePan);
			pMgr->SetControlFlag(CPlayerSwitchInterface::CONTROL_FLAG_PAUSE_BEFORE_OUTRO, ms_bFlagPauseBeforeOutro);
			pMgr->SetControlFlag(CPlayerSwitchInterface::CONTROL_FLAG_PAUSE_BEFORE_ASCENT, ms_bFlagPauseBeforeAscent);
			pMgr->SetControlFlag(CPlayerSwitchInterface::CONTROL_FLAG_PAUSE_BEFORE_DESCENT, ms_bFlagPauseBeforeDescent);
			pMgr->SetControlFlag(CPlayerSwitchInterface::CONTROL_FLAG_SKIP_PAN, ms_bFlagSkipPan);
			pMgr->SetControlFlag(CPlayerSwitchInterface::CONTROL_FLAG_UNKNOWN_DEST, ms_bFlagUnknownDest);
			pMgr->SetControlFlag(CPlayerSwitchInterface::CONTROL_FLAG_DESCENT_ONLY, ms_bFlagDescentOnly);
			pMgr->SetControlFlag(CPlayerSwitchInterface::CONTROL_FLAG_START_FROM_CAMPOS, ms_bFlagStartFromCamPos);
			pMgr->SetControlFlag(CPlayerSwitchInterface::CONTROL_FLAG_ALLOW_SNIPER_AIM_INTRO, ms_bFlagAllowSniperAimIntro);
			pMgr->SetControlFlag(CPlayerSwitchInterface::CONTROL_FLAG_ALLOW_SNIPER_AIM_OUTRO, ms_bFlagAllowSniperAimOutro);
			pMgr->SetControlFlag(CPlayerSwitchInterface::CONTROL_FLAG_SKIP_TOP_DESCENT, ms_bFlagSkipTopDescent);
			pMgr->SetControlFlag(CPlayerSwitchInterface::CONTROL_FLAG_SUPPRESS_OUTRO_FX, ms_bFlagSupressOutroFX);
			pMgr->SetControlFlag(CPlayerSwitchInterface::CONTROL_FLAG_SUPPRESS_INTRO_FX, ms_bFlagSupressIntroFX);
			pMgr->SetControlFlag(CPlayerSwitchInterface::CONTROL_FLAG_DELAY_ASCENT_FX, ms_bFlagDelayAscentFX);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	SaveSettingsCB
// PURPOSE:		rag callback for saving out player switch settings to metadata
//////////////////////////////////////////////////////////////////////////
void CPlayerSwitchDbg::SaveSettingsCB()
{
	g_PlayerSwitch.SaveSettings();
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	LoadSettingsCB
// PURPOSE:		rag callback for (re) loading player switch settings from metadata
//////////////////////////////////////////////////////////////////////////
void CPlayerSwitchDbg::LoadSettingsCB()
{
	if (!g_PlayerSwitch.IsActive())
	{
		g_PlayerSwitch.LoadSettings();

		SyncWithCurrentSettings();

		if (pEstablishingShotCombo)
		{
			pEstablishingShotCombo->UpdateCombo("Establishing shot", &ms_currentEstablishingShot, ms_establishingShots.GetCount(), &ms_establishingShots[0]);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	SyncWithCurrentSetings
// PURPOSE:
//////////////////////////////////////////////////////////////////////////
void CPlayerSwitchDbg::SyncWithCurrentSettings()
{
	CPlayerSwitchSettings& longRangeSettings = g_PlayerSwitch.GetSettings(CPlayerSwitchInterface::SWITCH_TYPE_LONG);

	ms_minJumps_LongRange					= longRangeSettings.GetNumJumpsMin();
	ms_maxJumps_LongRange					= longRangeSettings.GetNumJumpsMax();
	ms_panSpeed_LongRange					= longRangeSettings.GetPanSpeed();
	ms_ceilingHeight_LongRange				= longRangeSettings.GetCeilingHeight();
	ms_minHeightAboveGround_LongRange		= longRangeSettings.GetMinHeightAbove();
	ms_heightBetweenJumps_LongRange			= longRangeSettings.GetHeightBetweenJumps();
	ms_lodLevel_LongRange					= (s32) longRangeSettings.GetCeilingLodLevel();

	CPlayerSwitchSettings& mediumRangeSettings = g_PlayerSwitch.GetSettings(CPlayerSwitchInterface::SWITCH_TYPE_MEDIUM);

	ms_range_MediumRange					= mediumRangeSettings.GetRange();
	ms_minJumps_MediumRange					= mediumRangeSettings.GetNumJumpsMin();
	ms_maxJumps_MediumRange					= mediumRangeSettings.GetNumJumpsMax();
	ms_panSpeed_MediumRange					= mediumRangeSettings.GetPanSpeed();
	ms_ceilingHeight_MediumRange			= mediumRangeSettings.GetCeilingHeight();
	ms_minHeightAboveGround_MediumRange		= mediumRangeSettings.GetMinHeightAbove();
	ms_heightBetweenJumps_MediumRange		= mediumRangeSettings.GetHeightBetweenJumps();
	ms_lodLevel_MediumRange					= (s32) mediumRangeSettings.GetCeilingLodLevel();
	ms_skipPanRange_MediumRange				= mediumRangeSettings.GetSkipPanRange();

	CPlayerSwitchSettings& shortRangeSettings = g_PlayerSwitch.GetSettings(CPlayerSwitchInterface::SWITCH_TYPE_SHORT);

	ms_range_ShortRange			= shortRangeSettings.GetRange();

	ms_establishingShots.Reset();
	ms_establishingShots.Grow() = "none";
	for (s32 i=0; i<CPlayerSwitchEstablishingShot::ms_store.m_ShotList.GetCount(); i++)
	{
		ms_establishingShots.Grow() = CPlayerSwitchEstablishingShot::ms_store.m_ShotList[i].m_Name.GetCStr();
	}
	ms_currentEstablishingShot = 0;
}

#endif	//__BANK
