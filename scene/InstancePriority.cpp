/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    scene/InstancePriority.cpp
// PURPOSE : adjusts map entity instancing priority based on game behaviour and script hints
// AUTHOR :  Ian Kiigan
// CREATED : 17/04/13
//
/////////////////////////////////////////////////////////////////////////////////

#include "scene/InstancePriority.h"

#include "fwscene/stores/mapdatastore.h"
#include "game/Wanted.h"
#include "pathserver/ExportCollision.h"
#include "scene/ContinuityMgr.h"
#include "scene/world/GameWorld.h"

#if __BANK
#include "grcore/debugdraw.h"
#endif	//__BANK

#include "control/replay/replay.h"

XPARAM(useMinimumSettings);

#define HIGH_SPEED_FLIGHT_TIMEOUT (8000)

s32 CInstancePriority::ms_playMode					= CInstancePriority::PLAY_MODE_SINGLEPLAYER;
s32 CInstancePriority::ms_scriptHint				= CInstancePriority::SCRIPT_HINT_NONE;
u32 CInstancePriority::ms_entityInstancePriority	= CInstancePriority::PRIORITY_NORMAL;

// track if player was flying fast recently. may extend this to other things
bool CInstancePriority::ms_bFlyingFastRecently		= false;
u32 CInstancePriority::ms_flyingFastTimeStamp		= 0;
#if RSG_PC
bool CInstancePriority::ms_bForceLowestPriority		= false;
#endif // RSG_PC

#if !__FINAL
PARAM(instancepriority, "Override the instance priority for MP games");
#endif

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Init
// PURPOSE:		set initial values for everything
//////////////////////////////////////////////////////////////////////////
void CInstancePriority::Init()
{
	ms_playMode = PLAY_MODE_SINGLEPLAYER;
	ms_scriptHint = SCRIPT_HINT_NONE;

	ms_entityInstancePriority = PRIORITY_NORMAL;

	fwMapData::SetEntityPriorityCap( (fwEntityDef::ePriorityLevel)ms_entityInstancePriority );

	ms_bFlyingFastRecently = false;
	ms_flyingFastTimeStamp = 0;

#if NAVMESH_EXPORT
	if(CNavMeshDataExporter::WillExportCollision())
		ms_entityInstancePriority = PRIORITY_NORMAL;
#endif
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	SetMode
// PURPOSE:		register a change from single player to multi player or vice versa.
//				switching to multi player will cause all high detail map data to be flushed
//////////////////////////////////////////////////////////////////////////
void CInstancePriority::SetMode(s32 mode)
{
	ms_playMode = mode;
	Update();
	
	if (mode==PLAY_MODE_MULTIPLAYER)
	{
		FlushHdMapEntities();
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Update
// PURPOSE:		determine ideal instance priority for loading map data based
//				on play mode, platform, script hints, and other factors
//////////////////////////////////////////////////////////////////////////
void CInstancePriority::Update()
{
#if GTA_REPLAY
	if( CReplayMgr::IsEditModeActive() )
	{
		ms_entityInstancePriority = CReplayMgr::GetReplayFrameData().m_instancePriority;
	}
	else
#endif	// GTA_REPLAY
	{
		CheckForPlayerMovement();

		CWanted* pWanted = CGameWorld::FindLocalPlayerWanted();

		// current gen consoles
		switch (ms_playMode)
		{
		case PLAY_MODE_SINGLEPLAYER:
			{
				if ( ms_scriptHint==SCRIPT_HINT_DRIVING || ms_scriptHint==SCRIPT_HINT_SNIPING || ms_scriptHint==SCRIPT_HINT_HELICAM
					|| (pWanted && pWanted->GetWantedLevel()>=WANTED_LEVEL5) || ms_bFlyingFastRecently )
				{
					ms_entityInstancePriority = PRIORITY_LOWEST;
				}
				else if ( ms_scriptHint==SCRIPT_HINT_SHOOTING || (pWanted && pWanted->GetWantedLevel()>WANTED_LEVEL3) )
				{
					ms_entityInstancePriority = PRIORITY_LOW;
				}
				else
				{
					ms_entityInstancePriority = PRIORITY_NORMAL;
				}
			}
			break;

		case PLAY_MODE_MULTIPLAYER:
			{
#if !__FINAL
				// HACK: Override the instance priority to same MP streaming
				if (PARAM_instancepriority.Get())
				{
					u32 priority = PRIORITY_NORMAL;
					PARAM_instancepriority.Get(priority);
					if (AssertVerify(priority <= CInstancePriority::PRIORITY_HIGH))
						ms_entityInstancePriority = priority;
					else
						ms_entityInstancePriority = PRIORITY_NORMAL;
				}		
				else
#endif
				{
					ms_entityInstancePriority = PRIORITY_LOWEST;
				}
			}
			break;
		default:
			break;
		}

#if RSG_PC
		if( ms_bForceLowestPriority )
		{
			ms_entityInstancePriority = PRIORITY_LOWEST;
		}

		if (PARAM_useMinimumSettings.Get())
		{
			ms_entityInstancePriority = PRIORITY_LOWEST;
		}
#endif

#if NAVMESH_EXPORT
		// Override from navmesh exporter
		if(CNavMeshDataExporter::WillExportCollision())
			ms_entityInstancePriority = PRIORITY_NORMAL;
#endif

#if __BANK
		// override from debug widgets
		if (ms_bOverrideInstancePriority)
		{
			ms_entityInstancePriority = (u32) ms_dbgOverrideInstancePriority;
		}
#endif	//__BANK
	}

	fwMapData::SetEntityPriorityCap( (fwEntityDef::ePriorityLevel)ms_entityInstancePriority );
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	CheckForPlayerMovement
// PURPOSE:		check for high speed movement, flight etc
//////////////////////////////////////////////////////////////////////////
void CInstancePriority::CheckForPlayerMovement()
{
	u32 currentTime = fwTimer::GetTimeInMilliseconds();

	if (ms_bFlyingFastRecently)
	{
		if (g_ContinuityMgr.HighSpeedFlying())
		{
			ms_flyingFastTimeStamp = currentTime;
		}
		ms_bFlyingFastRecently = (currentTime - ms_flyingFastTimeStamp) <= HIGH_SPEED_FLIGHT_TIMEOUT;
	}
	else
	{
		if (g_ContinuityMgr.HighSpeedFlying())
		{
			ms_bFlyingFastRecently = true;
			ms_flyingFastTimeStamp = currentTime;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	FlushHdMapEntities
// PURPOSE:
//////////////////////////////////////////////////////////////////////////
void CInstancePriority::FlushHdMapEntities()
{
	g_MapDataStore.SafeRemoveAll(true);
}

#if __BANK
//////////////////////////////////////////////////////////////////////////
// DEBUG WIDGETS

bool CInstancePriority::ms_bDisplayInstanceInfo = false;
bool CInstancePriority::ms_bOverrideInstancePriority = false;
s32 CInstancePriority::ms_dbgOverrideInstancePriority = 0;

const char* apszPlayModes[] =
{
	"SINGLE PLAYER",	//PLAY_MODE_SINGLEPLAYER
	"MULTI PLAYER"		//PLAY_MODE_MULTIPLAYER
};

const char* apszScriptHints[] =
{
	"NONE",				//SCRIPT_HINT_NONE
	"SHOOTING",			//SCRIPT_HINT_SHOOTING
	"DRIVING",			//SCRIPT_HINT_DRIVING
	"SNIPING"			//SCRIPT_HINT_SNIPING
};

const char* apszPriorityLevels[] =
{
	"LOWEST",
	"LOW",
	"NORMAL",
	"HIGH"
};

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	InitWidgets
// PURPOSE:		debug widget creation
//////////////////////////////////////////////////////////////////////////
void CInstancePriority::InitWidgets(bkBank& bank)
{
	bank.PushGroup("Instance Priority");
	{
		bank.AddToggle("Display instance info", &ms_bDisplayInstanceInfo);
		bank.AddCombo("Mode", &ms_playMode, 2, &apszPlayModes[0], ChangeModeCB);
		bank.AddCombo("Hint", &ms_scriptHint, 4, &apszScriptHints[0]);
		bank.AddToggle("Override instance priority", &ms_bOverrideInstancePriority, ChangePriorityCB);
		bank.AddCombo("Mode", &ms_dbgOverrideInstancePriority, 4, &apszPriorityLevels[0], ChangePriorityCB);
	}
	bank.PopGroup();
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	UpdateDebug
// PURPOSE:		display some debug info about instance priority settings
//////////////////////////////////////////////////////////////////////////
void CInstancePriority::UpdateDebug()
{
	if (ms_bDisplayInstanceInfo)
	{
		grcDebugDraw::AddDebugOutput("Instance priority: mode=%s, hint=%s, priority=%d",
			apszPlayModes[ms_playMode],
			apszScriptHints[ms_scriptHint],
			ms_entityInstancePriority );
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	ChangeModeCB
// PURPOSE:		debug widget callback
//////////////////////////////////////////////////////////////////////////
void CInstancePriority::ChangeModeCB()
{
	SetMode(ms_playMode);
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	ChangePriorityCB
// PURPOSE:		adjust overridden priority level
//////////////////////////////////////////////////////////////////////////
void CInstancePriority::ChangePriorityCB()
{
	if (ms_bOverrideInstancePriority && (u32)ms_dbgOverrideInstancePriority!=ms_entityInstancePriority)
	{
		Update();
		FlushHdMapEntities();
	}
}

//////////////////////////////////////////////////////////////////////////

#endif	//__BANK