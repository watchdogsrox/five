// Title	:	Wanted.cpp
// Author	:	Gordon Speirs
// Started	:	17/04/00

// C++ headers
#include <limits>
//Rage headers
#include "math/amath.h"
// Game headers
#include "audio/policescanner.h"
#include "audio/scannermanager.h"
#include "audio/scriptaudioentity.h"
#include "audioengine/engine.h"
#include "control/gamelogic.h"
#include "control/Replay/ReplayBufferMarker.h"
#include "debug/Debug.h"
#include "debug/DebugScene.h"
#include "debug/DebugWanted.h"
#include "event/EventGroup.h"
#include "event/Events.h"
#include "event/EventScript.h"
#include "event/ShockingEvents.h"
#include "frontend/MiniMap.h"
#include "frontend/Map/BlipEnums.h"
#include "game/cheat.h"
#include "game/Crime.h"
#include "game/CrimeInformation.h"
#include "Stats/StatsMgr.h"
#include "Stats/StatsInterface.h"
#include "Stats/StatsUtils.h"
#include "game/Dispatch/DispatchData.h"
#include "game/Dispatch/DispatchHelpers.h"
#include "game/Dispatch/Incidents.h"
#include "game/Dispatch/IncidentManager.h"
#include "game/Wanted.h"
#include "game/Wanted_parser.h"
#include "game/localisation.h"
#include "modelinfo/vehiclemodelinfo.h"
#include "peds/Ped.h"
#include "peds/PedIntelligence.h"
#include "peds/PedMotionData.h"
#include "peds/PlayerInfo.h"
#include "Scene/DataFileMgr.h"
#include "scene/world/GameWorld.h"
#include "scene/scene.h"
#include "scene/EntityIterator.h"
#include "script/script.h"
#include "script/script_hud.h"
#include "task/Combat/CombatManager.h"
#include "task/Combat/TaskDamageDeath.h"
#include "task/Combat/TaskNewCombat.h"
#include "task/Combat/TaskThreatResponse.h"
#include "task/Combat/TaskWrithe.h"
#include "task/Service/Police/TaskPolicePatrol.h"
#include "text/messages.h"
#include "vehicleAi/VehicleIntelligence.h"
#include "Vehicles/Trailer.h"
#include "vehicles/vehicle.h"
//#include "modelindices.h"


#include "Network/Live/livemanager.h"
#include "network/Live/NetworkTelemetry.h"
#include "game/zones.h"

// network includes
#include "network/events/NetworkEventTypes.h"
#include "network/Objects/Entities/NetObjPlayer.h"
#include "network/players/NetGamePlayer.h"
#include "network/NetworkInterface.h"

#include "data/aes_init.h"
AES_INIT_5;

AI_OPTIMISATIONS()

extern audScannerManager g_AudioScannerManager;
u32 g_MinTimeBetweenScannerRefuelReports = 60000;

// grrr. This macro conflicts under visual studio.
#if defined (MSVC_COMPILER)
#ifdef max
#undef max
#endif //max
#endif //MSVC_COMPILER	

#if __BANK
	const char* sm_WantedLevelSetFromLocations[WL_FROM_MAX_LOCATIONS] = {"Internal", "Removing", "Test", "Network", "Cheat", "Script", "Law Response Event", 
																		 "Task NM Balance", "Task NB Brace", "Task Arrest Ped", "Task Busted",
																		 "Task Threat Response (Due To Event)", "Task Threat Response (Initial Decision)",
																		 "Task Call Police", "Reported Crime", "Broke Parole", "Internal (Evading)",};
#endif


// This file contains functions used to maintain a wanted level for
// the player (and other entities if necessary).

//////////////////////////////////////////////////////////////////////////

void CCrimeBeingQd::AddWitness(CEntity& rEntity)
{
	//Validate the parameters.
	aiAssert(!HasMaxWitnesses());
	aiAssert(!HasBeenWitnessedBy(rEntity));

	//Add the witness.
	//@@: location CCRIMEBEINGQD_ADDWITNESS
	m_Witnesses.Append().Reset(&rEntity);
}

bool CCrimeBeingQd::HasLifetimeExpired() const
{
	return NetworkInterface::GetSyncedTimeInMilliseconds() > TimeOfQing + Lifetime;
}

bool CCrimeBeingQd::HasBeenWitnessedBy(const CEntity& rEntity) const
{
	//Iterate over the witnesses.
	for(int i = 0; i < m_Witnesses.GetCount(); ++i)
	{
		//Check if the entity matches.
		if(&rEntity == m_Witnesses[i])
		{
			return true;
		}
	}

	return false;
}

bool CCrimeBeingQd::HasOnlyBeenWitnessedBy(const CEntity& rEntity) const
{
	return (m_Witnesses.GetCount() == 1 && &rEntity == (m_Witnesses[0]).Get());
}

bool CCrimeBeingQd::HasMaxWitnesses() const
{
	return m_Witnesses.IsFull();
}

bool CCrimeBeingQd::HasWitnesses() const
{
	return !m_Witnesses.IsEmpty();
}

bool CCrimeBeingQd::IsInProgress() const
{
	// All crimes are considered instantaneous now.
	return false;
}


bool CCrimeBeingQd::IsSerious() const
{
	// Consider all crimes serious right now.
	return true;
}

void CCrimeBeingQd::ResetWitnesses()
{
	//Reset the witnesses (individually).
	for(int i = 0; i < m_Witnesses.GetCount(); ++i)
	{
		m_Witnesses[i].Reset(NULL);
	}

	//Reset the witnesses.
	m_Witnesses.Reset();
}

//////////////////////////////////////////////////////////////////////////

CWanted::Overrides::Overrides()
	: m_MultiplayerHiddenEvasionTimesOverride(WANTED_LEVEL_LAST)
{
	Reset();
}

CWanted::Overrides::~Overrides()
{
}

float CWanted::Overrides::GetDifficulty() const
{
	return m_fDifficulty;
}

u32 CWanted::Overrides::GetMultiplayerHiddenEvasionTimesOverride(s32 iWantedLevel) const
{
	if (iWantedLevel >= 0 && iWantedLevel < m_MultiplayerHiddenEvasionTimesOverride.GetCount())
	{
		return m_MultiplayerHiddenEvasionTimesOverride[iWantedLevel];
	}
	return 0;
}

void CWanted::Overrides::SetMultiplayerHiddenEvasionTimesOverride(const s32 index, const u32 value)
{
	if (!aiVerifyf(index >= 0 && index < WANTED_LEVEL_LAST , "SetMultiplayerHiddenEvasionTimesOverride() - The array index (%i) is not valid.", index))
	{
		return;
	}
	m_MultiplayerHiddenEvasionTimesOverride[index] = value;
}

bool CWanted::Overrides::HasDifficulty() const
{
	//Ensure the difficulty has been set.
	if(m_fDifficulty < 0.0f)
	{
		return false;
	}
	
	return true;
}

void CWanted::Overrides::Reset()
{
	//Reset the difficulty.
	ResetDifficulty();

	ResetEvasionTimeArray();
}

void CWanted::Overrides::ResetDifficulty()
{
	//Reset the difficulty.
	m_fDifficulty = -1.0f;
}

void CWanted::Overrides::ResetEvasionTimeArray()
{
	for (u32 i = 0; i < m_MultiplayerHiddenEvasionTimesOverride.GetCount(); i++)
	{
		m_MultiplayerHiddenEvasionTimesOverride[i] = 0;
	}
}

void CWanted::Overrides::SetDifficulty(float fDifficulty)
{
	//Assert that the difficulty is valid.
	if(!aiVerifyf(fDifficulty >= 0.0f && fDifficulty <= 1.0f, "The difficulty is invalid: %.2f.", fDifficulty))
	{
		return;
	}
	
	//Set the difficulty.
	m_fDifficulty = fDifficulty;
}

//////////////////////////////////////////////////////////////////////////


eWantedLevel 	CWanted::MaximumWantedLevel = WANTED_LEVEL5;
s32				CWanted::nMaximumWantedLevel = 10000;	// Large number as the default, wanted data will set this once the data is loaded
s32				CWanted::sm_iNumCopsOnFoot = 0;
s32				CWanted::sm_iMPVisibilityScanResetTime = 30000;
bool			CWanted::bUseNewsHeliInAdditionToPolice = false;
bool			CWanted::sm_bEnableCrimeDebugOutput = false;
bool			CWanted::sm_bEnableRadiusEvasion = false;
bool			CWanted::sm_bEnableHiddenEvasion = true;
//bool			CWanted::m_DisplayPoliceCircleBlips = true;

#if __BANK
bool			CWanted::sm_bRenderWantedLevelOfVehiclePasengers = false;
#endif

CWantedBlips	CWanted::sm_WantedBlips;

#if __WIN32
#pragma warning(push)
#pragma warning(disable:4355)	//  'this' : used in base member initializer list
#endif

CWanted::Tunables CWanted::sm_Tunables;
IMPLEMENT_TUNABLES_PSO(CWanted, "CWanted", 0x6704aab3, "Wanted Tuning", "Game Logic")

// GETCRIMEINFO

bool CWanted::GetCrimeInfo(eCrimeType crimeType, Tunables::CrimeInfo& crimeInfo)
{
	atHashString crimeHash = atHashString(g_CrimeNameHashes[crimeType]); 
	CWanted::Tunables::CrimeInfo* pCrimeInfo = nullptr;

	switch (CCrime::sm_eReportCrimeMethod)
	{
	case (CCrime::REPORT_CRIME_DEFAULT):
		pCrimeInfo = sm_Tunables.m_CrimeInfosDefault.SafeGet(crimeHash);
		break;
	case (CCrime::REPORT_CRIME_ARCADE_CNC):
		pCrimeInfo = sm_Tunables.m_CrimeInfosArcadeCNC.SafeGet(crimeHash);
		break;
	default:
		break;
	}
		
	if (pCrimeInfo != nullptr)
	{
		crimeInfo = *pCrimeInfo;
		return true;
	}

	return false;
}

/*
	Returns the penalty for this crime.
	If we have no data for this crime, return is 0
	If we have data, penalty will return the wanted score or wanted star penalty for the current crime reporting method
*/
int CWanted::GetCrimePenalty(eCrimeType crimeType)
{
	int iResult = 0;

	Tunables::CrimeInfo crimeInfo;
	if (GetCrimeInfo(crimeType, crimeInfo))
	{
		switch (CCrime::sm_eReportCrimeMethod)
		{
		case (CCrime::REPORT_CRIME_DEFAULT):
			// get wanted SCORE penalty
			iResult = crimeInfo.m_WantedScorePenalty;
			break;
		case (CCrime::REPORT_CRIME_ARCADE_CNC):
			// get wanted STAR penalty
			iResult = crimeInfo.m_WantedStarPenalty;
			break;
		}
	}

	return iResult;
}

int CWanted::GetCrimeReportDelay(eCrimeType crimeType)
{
	int iResult = CRIMES_GET_REPORTED_TIME;

	Tunables::CrimeInfo crimeInfo;
	if (GetCrimeInfo(crimeType, crimeInfo))
	{
		iResult = crimeInfo.m_ReportDelay;
	}

	return iResult;
}

int CWanted::GetCrimeLifetime(eCrimeType crimeType)
{
	int iResult = CRIMES_STAY_QD_TIME;

	Tunables::CrimeInfo crimeInfo;
	if (GetCrimeInfo(crimeType, crimeInfo))
	{
		iResult = crimeInfo.m_Lifetime;
	}

	return iResult;
}

CWanted::CWanted()
		: m_Witnesses(*this)
		, CrimesBeingQd(NULL)
		, m_MaxCrimesBeingQd(0)
		, m_CrimesSuppressedThisFrame(0)
		, m_fTimeUntilEvasionStarts(-1.0f)
		, m_vInitialSearchPosition(V_ZERO)
		, m_WantedLevelLastSetFrom(WL_FROM_INTERNAL)
		, m_TimeWhenNewWantedLevelTakesEffect(0)
		, m_pPed(NULL)
		, m_DontDispatchCopsForThisPlayer(false)
		, m_bHasCopArrivalAudioPlayed(false)
		, m_bHasCopSeesWeaponAudioPlayed(false)
		, m_iTimeFirstSpotted(0)
		, m_WantedLevelOld(WANTED_CLEAN)
		, m_WantedLevelCharId(CHAR_INVALID)
		, m_iTimeBeforeAllowReporting(0)
		, m_CausedByPlayerPhysicalIndex(INVALID_PLAYER_INDEX)
		, m_uLastTimeWantedLevelClearedByScript(0)
		, m_bIsOutsideCircle(false)
{
	m_CurrentSearchAreaCentre.Zero();

#if __BANK
	m_WantedLevelLastSetFromScriptName[0] = '\0';
#endif
}

#if __WIN32
#pragma warning(pop)
#endif

CWanted::~CWanted()
{
	delete []CrimesBeingQd;
	CrimesBeingQd = NULL;

	m_pPed = NULL;
}
void CWanted::SetPed (CPed* pOwnerPed)
{
	m_pPed = pOwnerPed;
}
// Name			:	Initialise
// Purpose		:	Init CWanted stuff
// Parameters	:	None
// Returns		:	Nothing

void CWanted::Initialise()
{
	if(!CrimesBeingQd)
	{
		const int maxCrimes = 16;
		m_MaxCrimesBeingQd = maxCrimes;
		CrimesBeingQd = rage_new CCrimeBeingQd[maxCrimes];
	}

	m_nWantedLevel = 0;
	m_nNewWantedLevel = 0;
	m_nWantedLevelBeforeParole = 0;
	m_LastTimeWantedLevelChanged = 0;
	m_TimeWhenNewWantedLevelTakesEffect = 0;
	m_TimeOfParole = 0;
	m_nMaxCopsInPursuit = 0;
	m_nMaxCopCarsInPursuit = 0;
	m_nChanceOnRoadBlock = 0;
	m_PoliceBackOff = false;
	m_PoliceBackOffGarage = false;
	m_EverybodyBackOff = false;
	m_AllRandomsFlee = false;
	m_AllNeutralRandomsFlee = false;
	m_AllRandomsOnlyAttackWithGuns = false;
	m_IgnoreLowPriorityShockingEvents = false;
	m_fMultiplier = 1.0f;
	m_fDifficulty = 0.0f;
	m_fTimeSinceLastDifficultyUpdate = 0.0f;
	m_bTimeCounting = false;
	m_bStoredPoliceBackOff = false;
	m_MaintainFlashingStarAfterOffence = false;
	m_LastSpottedByPolice = Vector3(0.0f, 0.0f, 0.0f);
	m_iTimeSearchLastRefocused = 0;
	m_iTimeLastSpotted = 0;
	m_iTimeFirstSpotted = 0;
	m_iTimeHiddenEvasionStarted = 0;
	m_iTimeHiddenWhenEvasionReset = 0;
	m_iTimeInitialSearchAreaWillTimeOut = 0;
	m_uLastEvasionTime = 0;
	m_fTimeEvading = 0.0f;
	m_fTimeUntilEvasionStarts = -1.0f;
	m_TimeCurrentAmnestyEnds = 0;
	m_TimeWantedLevelCleared = fwTimer::GetTimeInMilliseconds();
	m_TimeOutsideCircle = 0;
	m_fCurrentChaseTimeCounter = 0.0f;
	m_fCurrentChaseTimeCounterMaxStar = 0.0f;
	m_bTimeCountingMaxStar = false;
	m_CenterWantedPosition = false;
	m_bShouldDelayResponse = true;
	m_bIsOutsideCircle = false;
	m_SuppressLosingWantedLevelIfHiddenThisFrame = false;
	m_AllowEvasionHUDIfDisablingHiddenEvasionThisFrame = false;
	m_HasLeftInitialSearchArea = false;
	m_bHasCopArrivalAudioPlayed = false;
	m_bHasCopSeesWeaponAudioPlayed = false;
	m_DisableCallPoliceThisFrame = false;
	m_LawPedCanAttackNonWantedPlayer = false;
	m_pTacticalAnalysis = NULL;
	m_fTimeSinceLastHostileAction = LARGE_FLOAT;
	m_uLastTimeWantedSetByScript = 0;
	m_WantedLevelLastSetFrom = WL_FROM_INTERNAL;

	m_WantedLevelOld = WANTED_CLEAN;
	//MP: ensure that the wanted level is cleared appropriately - on respawn - it was just getting set to m_WantedLevel = WANTED_CLEAN without doing additional processing that typically happens in UpdateWantedLevel
	if (NetworkInterface::IsGameInProgress() && (m_WantedLevel > WANTED_CLEAN))
	{
		wantedDebugf3("CWanted::Initialise()--m_WantedLevel[%d] set m_WantedLevel = WANTED_CLEAN--m_pPed[%p][%s][%s]",m_WantedLevel,m_pPed,m_pPed ? m_pPed->GetModelName() : "",m_pPed && m_pPed->GetNetworkObject() ? m_pPed->GetNetworkObject()->GetLogName() : "");
		CPed* pWantedPed = GetPed(); //wanted ped
		if (pWantedPed)
			UpdateWantedLevel(VEC3V_TO_VECTOR3(pWantedPed->GetTransform().GetPosition()),true,false);
	}
	m_WantedLevel = WANTED_CLEAN;
	m_WantedLevelBeforeParole = WANTED_CLEAN;
	m_WantedLevelCharId = CHAR_INVALID;

	m_CrimesSuppressedThisFrame.Reset();
	ClearQdCrimes();

	m_uTimeHeliRefuelComplete = 0;
	m_LastTimeOfScannerRefuelReport = 0;

	m_DisableRelationshipGroupHashResetThisFrame = 0;

	m_ReportedCrimes.Reset();

	sm_WantedBlips.Init();

	wantedDebugf3("CWanted::Initialise -- m_HasLeftInitialSearchArea = false");

} // end - CWanted::Initialise


//called by CDispatchData::Init
void CWanted::InitialiseStaticVariables()
{
	MaximumWantedLevel = WANTED_LEVEL5;
	nMaximumWantedLevel = CDispatchData::GetWantedLevelThreshold(MaximumWantedLevel) * 2;	// 2 or we won't be able to have 6 stars for long.
	bUseNewsHeliInAdditionToPolice = false;
	sm_bEnableCrimeDebugOutput = false;
	sm_bEnableRadiusEvasion = false;
	sm_bEnableHiddenEvasion = true;
}

#if __BANK

static u32 debugPlayerWantedLevel = 3;

void GivePlayerAWantedLevel()
{
	CPed * pPlayerPed = FindPlayerPed();
	if (debugPlayerWantedLevel > CWanted::MaximumWantedLevel)
	{
		pPlayerPed->GetPlayerWanted()->SetMaximumWantedLevel(debugPlayerWantedLevel);
	}

	pPlayerPed->GetPlayerWanted()->SetWantedLevel(VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetPosition()), debugPlayerWantedLevel, 0, WL_FROM_CHEAT);
}

// Name			:	InitWidgets
// Purpose		:	
// Parameters	:	None
// Returns		:	Nothing

void CWanted::InitWidgets()
{
	// Add the debugging widgets..
	bkBank *bank = CGameLogic::GetGameLogicBank();

	bank->PushGroup("Wanted Levels");
	{
		bank->AddToggle("Crime debug output", &sm_bEnableCrimeDebugOutput);
		bank->AddToggle("Allow evasion by exceeding radius", &sm_bEnableRadiusEvasion);
		bank->AddToggle("Allow evasion by being hidden", &sm_bEnableHiddenEvasion);
		bank->AddToggle("Render wanted level for vehicle passengers", &sm_bRenderWantedLevelOfVehiclePasengers);
		bank->AddSlider("MP visibility scan reset time", &sm_iMPVisibilityScanResetTime, 0, 500000, 1 );
		bank->AddSlider("Player Wanted Level", &debugPlayerWantedLevel, WANTED_CLEAN, WANTED_LEVEL_LAST-1, 1);
		bank->AddButton("Give player wanted level", GivePlayerAWantedLevel);
	}
	bank->PopGroup();

	bank->PushGroup("Witnesses");
	CCrimeWitnesses::AddWidgets(*bank);
	bank->PopGroup();
}

#endif // __BANK

#if DEBUG_DRAW

void CWanted::DebugRender() const
{
	if (sm_bRenderWantedLevelOfVehiclePasengers)
	{
		if (const CPed* pLocalPlayer = CGameWorld::FindLocalPlayer())
		{
			if (CVehicle* pPlayerVehicle = pLocalPlayer->GetVehiclePedInside())
			{
				for (s32 i = 0; i < pPlayerVehicle->GetSeatManager()->GetMaxSeats(); i++)
				{
					CPed* pPed = pPlayerVehicle->GetSeatManager()->GetPedInSeat(i);
					if (pPed && pPed->IsPlayer())
					{
						u32 ePassengerWantedLevel = static_cast<u32>(pPed->GetPlayerWanted()->GetWantedLevel());
						char wantedText[256];
						formatf(wantedText, "Wanted: %i", ePassengerWantedLevel);
						Vec3V vDebugPos = pPed->GetTransform().GetPosition();
						TUNE_GROUP_FLOAT(WANTED, fVehPassengerLevelDebugHeightOffset, 0.5f, -20.f, 20.f, 0.01f);
						vDebugPos = vDebugPos + VECTOR3_TO_VEC3V(VEC3V_TO_VECTOR3(pPlayerVehicle->GetTransform().GetUp()) * fVehPassengerLevelDebugHeightOffset);

						grcDebugDraw::Text(vDebugPos, Color_yellow, 0, 0, wantedText, true, 1);
					}
				}
			}
		}
	}

	//Ensure rendering is enabled.
	if(!sm_Tunables.m_Rendering.m_Enabled)
	{
		return;
	}

	//Check if we should render witnesses.
	if(sm_Tunables.m_Rendering.m_Witnesses)
	{
		m_Witnesses.DebugRender();
	}

	//Check if we should render crimes.
	if(sm_Tunables.m_Rendering.m_Crimes)
	{
		for(int i = 0; i < m_MaxCrimesBeingQd; i++)
		{
			const CCrimeBeingQd &crime = CrimesBeingQd[i];
			if(crime.CrimeType == CRIME_NONE)
			{
				continue;
			}

			Vec3V_ConstRef pos = RCC_VEC3V(crime.Coors);

			// This is close to how crimes were displayed in RDR2, but feel free to improve. /FF
			Mat34V mtx(V_IDENTITY);
			mtx.SetCol3(pos);
			grcDebugDraw::Sphere(pos, 0.1f, Color32(0xff, 0x00, 0x00), false, 1, 12);
			grcDebugDraw::Axis(mtx, 1.0f);

			char witnessText[256];
			CCrimeWitnesses::GetWitnessInfoText(witnessText, sizeof(witnessText), crime);

			const char* pTimeText = crime.HasLifetimeExpired() ? "Timing out\n" : "";

			char crimeText[256];
			formatf(crimeText, "Crime type %d\n%s\n%s", crime.CrimeType, witnessText, pTimeText);
			grcDebugDraw::Text(pos, Color_white, 0, 0, crimeText, true, 1);

			for(int i = 0; i < crime.m_Witnesses.GetCount(); ++i)
			{
				if(crime.m_Witnesses[i])
				{
					grcDebugDraw::Sphere(VEC3V_TO_VECTOR3(crime.m_Witnesses[i]->GetTransform().GetPosition()) + ZAXIS, 0.125f, Color_orange);
				}
			}
		}
	}
}

#endif	// DEBUG_DRAW


// Name			:	Reset
// Purpose		:	Clears the police pursuit too
// Parameters	:	None
// Returns		:	Nothing
void CWanted::Reset()
{
	Initialise();
}


// Name			:	Update
// Purpose		:	Update wanted data every frame
// Parameters	:	None
// Returns		:	Nothing
void CWanted::Update(const Vector3 &PlayerCoors, const CVehicle *pVeh, const bool bLocal, const bool bUpdateStats)
{
	//Update the timers.
	UpdateTimers();

	CPed* pPlayerPed = GetPed();
	Assert(pPlayerPed && pPlayerPed->IsAPlayerPed());
	CPlayerInfo* pPlayerInfo = pPlayerPed->GetPlayerInfo();
	Assert(pPlayerInfo);

	if (bLocal && m_TimeWhenNewWantedLevelTakesEffect > 0 && NetworkInterface::GetSyncedTimeInMilliseconds() >= m_TimeWhenNewWantedLevelTakesEffect)
	{
		m_nWantedLevel = m_nNewWantedLevel;
		UpdateWantedLevel(PlayerCoors, true, m_WantedLevel == WANTED_CLEAN);		// Only refocus search if player didn't have wanted level before.
		m_TimeWhenNewWantedLevelTakesEffect = 0;
	}

	DR_ONLY(debugWanted::UpdateRecording(m_WantedLevel));

	// If there is a wanted level and the player has made it outside the police cordon the wanted level is dropped.
	if (m_WantedLevel >= WANTED_LEVEL1)
	{
		u32 uTimeInMilliseconds = NetworkInterface::GetSyncedTimeInMilliseconds();

		// If script has set the center wanted position u8/bool then we center the radius position to the players position
		// If the player is driving a vehicle that is marked to always be tracked
 		if (m_CenterWantedPosition ||
			(pVeh && pVeh->m_nVehicleFlags.bPoliceFocusWillTrackCar))
 		{
			wantedDebugf3("CWanted::Update m_CenterWantedPosition || bPoliceFocusWillTrackCar --> time[%u] invoke RestartPoliceSearch",uTimeInMilliseconds);
			RestartPoliceSearch(PlayerCoors, uTimeInMilliseconds, true);
 		}

		// url:bugstar:2557284 - eTEAM_COP isn't used any more, script use teams 0-3 freely with assuming certain team types. Removing below code.

		// If this is a network game we need to loop through other players and check if they have reported sight of us
		//if(NetworkInterface::IsGameInProgress())
		//{
		//	// Get the pool and pool size then loop through it
		//	CPlayerInfo::Pool *pool = CPlayerInfo::GetPool();
		//	s32 poolIndex = pool->GetSize();

		//	while(poolIndex--)
		//	{
		//		// Verify the net players info and that it isn't equal to ours
		//		// Verify they have a ped and then go through their player info to check if they can see us
		//		CPlayerInfo *pNetPlayerInfo = pool->GetSlot(poolIndex);
		//		if( pNetPlayerInfo && pNetPlayerInfo != pPlayerInfo && pNetPlayerInfo->Team == eTEAM_COP && 
		//			pNetPlayerInfo->GetPlayerPed() && pNetPlayerInfo->GetIsPlayerVisible(pPlayerInfo->GetPhysicalPlayerIndex()) )
		//		{
		//			wantedDebugf3("CWanted::Update player isvisible set m_LastSpottedByPolice... time[%u] invoke RestartPoliceSearch",uTimeInMilliseconds);
		//			RestartPoliceSearch(PlayerCoors, uTimeInMilliseconds, true);
		//		}
		//	}
		//}

		if(!m_HasLeftInitialSearchArea)
		{
			static float s_fSearchAreaDistSq = square(20.0f);
			ScalarV scMaxDistSq = LoadScalar32IntoScalarV(s_fSearchAreaDistSq);
			bool bInitialSearchAreaTimedOut = (m_iTimeInitialSearchAreaWillTimeOut > 0 && uTimeInMilliseconds > m_iTimeInitialSearchAreaWillTimeOut);

			if( bInitialSearchAreaTimedOut ||
				IsGreaterThanAll(DistSquared(m_vInitialSearchPosition, VECTOR3_TO_VEC3V(PlayerCoors)), scMaxDistSq) ||
				CPedGeometryAnalyser::IsPedInWaterAtDepth(*m_pPed, CPedGeometryAnalyser::ms_MaxPedWaterDepthForVisibility) )
			{
				if(!HasPendingCrimesQueued())
				{
					wantedDebugf3("CWanted::Update set m_HasLeftInitialSearchArea=true");

					m_HasLeftInitialSearchArea = true;

					// Start the evasion timer if we are ready and it hasn't been triggered yet (as it won't be triggered when still in the initial area)
					if(m_fTimeUntilEvasionStarts <= 0.0f)
					{
						wantedDebugf3("CWanted::Update SetHiddenEvasionTime");
						SetHiddenEvasionTime(NetworkInterface::GetSyncedTimeInMilliseconds());
					}
				}
			}
		}

		float Dist = (m_LastSpottedByPolice - PlayerCoors).XYMag();

		if (Dist > CDispatchData::GetWantedZoneRadius(m_WantedLevel, m_pPed))
		{
			if( pPlayerPed->IsLocalPlayer() )
				m_bIsOutsideCircle = true;

			m_TimeOutsideCircle += fwTimer::GetTimeStepInMilliseconds();
			wantedDebugf3("CWanted::Update Dist[%f] m_LastSpottedByPolice[%f %f %f] PlayerCoors[%f %f %f] m_TimeOutsideCircle[%u]",Dist,m_LastSpottedByPolice.x,m_LastSpottedByPolice.y,m_LastSpottedByPolice.z,PlayerCoors.x,PlayerCoors.y,PlayerCoors.z,uTimeInMilliseconds);

			if (sm_bEnableRadiusEvasion && (m_TimeOutsideCircle >= (s32)CDispatchData::GetParoleDuration()))
			{
//				if (pVeh && pVeh->GetVehicleType() == VEHICLE_TYPE_HELI)	// If the player is in a helicopter the wanted level will never drop below 3 as it's too easy to avoid police otherwise
//				{
//					m_nWantedLevel = MIN(m_nWantedLevel, ((WANTED_LEVEL3_THRESHOLD + WANTED_LEVEL4_THRESHOLD) / 2));
//				}
//				else
				{
					if (bLocal)
						m_nWantedLevel = 0;
				}

				if (bLocal)
					UpdateWantedLevel(PlayerCoors);

				m_TimeOutsideCircle = 0;
				
				if( pPlayerPed->IsLocalPlayer() )
					m_bIsOutsideCircle = false;
			}

//			char *pText = TheText.Get("WNT_ESC");
//			CMessages::AddMessage(pText, 1000*4, 2, true, false);
		}
		else
		{
			m_TimeOutsideCircle = 0;

			if( pPlayerPed->IsLocalPlayer() )
				m_bIsOutsideCircle = false;
		}
	}
	else
	{
		if (m_HasLeftInitialSearchArea)
			wantedDebugf3("CWanted::Update -- (m_WantedLevel < WANTED_LEVEL1) -- m_HasLeftInitialSearchArea = false");

		m_HasLeftInitialSearchArea = false;
		m_iTimeFirstSpotted = 0;
		m_iTimeLastSpotted = 0;
		m_iTimeSearchLastRefocused = 0;
		m_iTimeHiddenEvasionStarted = 0;
		m_iTimeInitialSearchAreaWillTimeOut = 0;
	}

	// Make sure there is a wanted level incident in the list
	if (bLocal)
	{
		if( m_WantedLevel > WANTED_CLEAN )
		{
			if( m_wantedLevelIncident == NULL )
			{
				CWantedIncident newIncident(pPlayerPed, m_LastSpottedByPolice);
				int incidentSlot = -1;

				if (CIncidentManager::GetInstance().AddIncident(newIncident, true, &incidentSlot))
				{
					m_wantedLevelIncident = CIncidentManager::GetInstance().GetIncident(incidentSlot);
					aiAssert(m_wantedLevelIncident);
				}
			}
		}
		else
		{
			m_wantedLevelIncident = NULL;
		}
	}

	//
	// store the chase times in the stats:
	//
	if (bUpdateStats)
	{
		if (m_WantedLevel >= WANTED_LEVEL1)
		{
			if (!m_bTimeCounting)
			{
				m_fCurrentChaseTimeCounter = 0.0f;
				m_bTimeCounting = true;  // we are now counting
			}

			if (m_WantedLevel >= WANTED_LEVEL5 && !m_bTimeCountingMaxStar)
			{
				m_bTimeCountingMaxStar = true; // we are now counting
				m_fCurrentChaseTimeCounterMaxStar = 0.0f;
			}
			else if (m_WantedLevel < WANTED_LEVEL5 && m_bTimeCountingMaxStar)
			{
				m_bTimeCountingMaxStar = false;
			}

			if (pPlayerInfo && !pPlayerInfo->AreControlsDisabled() && pPlayerPed)
			{
				CVehicle* pVehicle = pPlayerPed->GetVehiclePedInside();
				if (!pVehicle || (pVehicle && VEHICLE_TYPE_TRAIN!=pVehicle->GetVehicleType()))
				{
					float fTimeStep = (float)fwTimer::GetSystemTimeStepInMilliseconds();

					if (m_bTimeCounting)
					{
						m_fCurrentChaseTimeCounter += fTimeStep;
					}

					const bool cheatHappenedRecently = CCheat::CheatHappenedRecently(CCheat::WANTED_LEVEL_DOWN_CHEAT, 600000)
						|| CCheat::CheatHappenedRecently(CCheat::ANNIHILATOR_CHEAT, 600000)
						|| CCheat::CheatHappenedRecently(CCheat::HEALTH_CHEAT, 600000);

					if(bLocal && m_bTimeCountingMaxStar && !cheatHappenedRecently)
					{
						m_fCurrentChaseTimeCounterMaxStar += fTimeStep;
						if(CStatsUtils::CanUpdateStats())
						{
							StatsInterface::IncrementStat(STAT_TOTAL_TIME_MAX_STARS.GetHash(GetCharacterIndex()), fTimeStep);
						}
					}
				}
			}
		}
		else 
		{
			if (bLocal && m_fCurrentChaseTimeCounter > 0.0f)
			{
				if(CStatsUtils::CanUpdateStats() && GetCharacterIndex() != CHAR_INVALID)
				{
					StatsInterface::IncrementStat(STAT_TOTAL_CHASE_TIME.GetHash(GetCharacterIndex()), m_fCurrentChaseTimeCounter);
					StatsInterface::SetGreater(STAT_LONGEST_CHASE_TIME.GetHash(GetCharacterIndex()), m_fCurrentChaseTimeCounter);
					StatsInterface::SetStatData(STAT_LAST_CHASE_TIME.GetHash(GetCharacterIndex()), m_fCurrentChaseTimeCounter);
				}

			}

			m_fCurrentChaseTimeCounter = 0.0f;
			m_fCurrentChaseTimeCounterMaxStar = 0.0f;
			m_bTimeCounting = false;
			m_bTimeCountingMaxStar = false;
		}
	}

	if (sm_bEnableRadiusEvasion && (NetworkInterface::GetSyncedTimeInMilliseconds() > (m_TimeOfParole + CDispatchData::GetParoleDuration())) &&
		!m_MaintainFlashingStarAfterOffence)
	{
		ClearParole();
	}


	// Updates any cops in the game with wanted acquaintances if the player is wanted
	UpdateCopPeds();

	sm_WantedBlips.UpdateCopBlips(this);  // update any cop blips

	m_Witnesses.Update();

	UpdateCrimesQ();

	
	if(bLocal && m_bStoredPoliceBackOff != PoliceBackOff())
	{
		UpdateWantedLevel(PlayerCoors, false, false);
		m_bStoredPoliceBackOff = PoliceBackOff();
	}	

	sm_WantedBlips.CreateAndUpdateRadarBlips(this);

	m_CenterWantedPosition = false;
	
	//Update the evasion.
	UpdateEvasion();
	
	//Check if we should update the difficulty.
	if(ShouldUpdateDifficulty())
	{
		//Update the difficulty.
		UpdateDifficulty();
	}

	//Update the tactical analysis.
	UpdateTacticalAnalysis();

	//Update the arrival audio based on if there is a wanted level or if any cops are in combat with our ped
	CCombatTaskManager* pCombatTaskManager = CCombatManager::GetCombatTaskManager();
	if( (m_bHasCopArrivalAudioPlayed || m_bHasCopSeesWeaponAudioPlayed) &&
		(m_WantedLevel == WANTED_CLEAN ||
		pCombatTaskManager->CountPedsInCombatWithTarget(*m_pPed, CCombatTaskManager::OF_MustBeLawEnforcement|CCombatTaskManager::OF_ReturnAfterInitialValidPed) == 0) )
	{
		m_bHasCopArrivalAudioPlayed = false;
		m_bHasCopSeesWeaponAudioPlayed = false;
	}

	//Finally at the end update some stats based on the m_WantedLevelOld
	if (bUpdateStats && CStatsUtils::CanUpdateStats())
	{
		UpdateStats( );
	}
}

void CWanted::UpdateStats( )
{
	//Update metrics/stats based on last frame wanted level change.
	if (m_WantedLevelOld != m_WantedLevel)
	{
		if (!NetworkInterface::IsInSpectatorMode() && CStatsUtils::CanUpdateStats() && GetCharacterIndex() != CHAR_INVALID)
		{
			if (m_WantedLevelOld == WANTED_CLEAN)
			{
				StatsInterface::IncrementStat(STAT_NO_TIMES_WANTED_LEVEL.GetHash(GetCharacterIndex()), 1.0f);
			}

			//Increment ATTAINED
			if (m_WantedLevelOld < m_WantedLevel)
			{
				if (m_WantedLevelOld == WANTED_CLEAN || CHAR_INVALID == m_WantedLevelCharId)
				{
					Assert(CHAR_INVALID == m_WantedLevelCharId);
					m_WantedLevelCharId = StatsInterface::GetCharacterIndex();
					Assert(CHAR_INVALID != m_WantedLevelCharId);
				}

				StatsInterface::IncrementStat(STAT_STARS_ATTAINED.GetHash(GetCharacterIndex()), (float)(m_WantedLevel - m_WantedLevelOld));
			}
			//Increment EVADED
			else if (m_WantedLevelOld > m_WantedLevel)
			{
				StatsInterface::IncrementStat(STAT_STARS_EVADED.GetHash(GetCharacterIndex()), (float)(m_WantedLevelOld - m_WantedLevel));
			}

			//Wanted level has changed
			CNetworkTelemetry::PlayerWantedLevel();
		}

		m_WantedLevelOld = m_WantedLevel;
	}

	if (m_WantedLevel == WANTED_CLEAN && m_WantedLevelCharId != CHAR_INVALID)
		m_WantedLevelCharId = CHAR_INVALID;
}

#define TIME_PASSED_TO_FLAG_AS_SEARCHING 1000
#define MP_TIME_PASSED_TO_FLAG_AS_SEARCHING 2000
#define TIME_SET_REMOTE_TO_SETUP_FLAG_AS_SEARCHING 1200
#define FORCE_HIDDEN_EVASION_SEARCHING_TIME 1500

bool CWanted::CopsAreSearching() const
{
	if(!m_HasLeftInitialSearchArea)
	{
		return false;
	}
	
	const u32 timePassedToFlagAsSearching = NetworkInterface::IsGameInProgress() ? MP_TIME_PASSED_TO_FLAG_AS_SEARCHING : TIME_PASSED_TO_FLAG_AS_SEARCHING;
	if(GetTimeSinceSearchLastRefocused() > timePassedToFlagAsSearching)
	{
		return true;
	}

	return false;
}

void CWanted::SetCopsAreSearching(const bool bValue)
{
	wantedDebugf3("CWanted::SetCopsAreSearching bValue[%d]",bValue);

	const u32 fTimeInMilliseconds = NetworkInterface::GetSyncedTimeInMilliseconds();
	if (bValue)
	{
		m_HasLeftInitialSearchArea = true;
		m_iTimeSearchLastRefocused = (fTimeInMilliseconds - TIME_SET_REMOTE_TO_SETUP_FLAG_AS_SEARCHING);
		wantedDebugf3("CWanted::SetCopsAreSearching fTimeInMilliseconds()[%u] m_HasLeftInitialSearchArea=true; m_iTimeSearchLastRefocused[%u]; m_iTimeHiddenEvasionStarted[%u]",fTimeInMilliseconds,m_iTimeSearchLastRefocused,m_HasLeftInitialSearchArea);
		if (!m_iTimeHiddenEvasionStarted)
			SetHiddenEvasionTime(fTimeInMilliseconds);
	}
	else
	{
		bool bWasHiddenEvasionTimeStarted = m_iTimeHiddenEvasionStarted > 0;

		wantedDebugf3("CWanted::SetCopsAreSearching m_HasLeftInitialSearchArea=false;");

		m_HasLeftInitialSearchArea = false;
		m_iTimeSearchLastRefocused = fTimeInMilliseconds;
		m_iTimeHiddenEvasionStarted = 0;

		if (bWasHiddenEvasionTimeStarted)
			sm_WantedBlips.DisableCones();
	}
}

#define TIME_NOT_SPOTTED_TO_SWITCH_CARS_UNSEEN 2000

// Name			:	PlayerEnteredVehicle
void CWanted::PlayerEnteredVehicle( CVehicle* pVehicle )
{
	// non law player entered a law vehicle, mark the vehicle is wanted
	if(pVehicle->IsLawEnforcementVehicle() && !m_pPed->IsLawEnforcementPed())
	{
        //@@: location CWANTED_PLAYERENTERED_VEHICLE
		pVehicle->m_nVehicleFlags.bWanted = true;
	}

	// If the player doesn't have a wanted level, entering a vehicle doesn't matter
	if (m_WantedLevel == WANTED_CLEAN)
	{
		return;
	}

	// IF the player has been seen recently, he's been seen entering this vehicle and its
	// now hot!
	if( GetTimeSinceSearchLastRefocused() < TIME_NOT_SPOTTED_TO_SWITCH_CARS_UNSEEN )
	{
		pVehicle->m_nVehicleFlags.bWanted = true;
	}

	if( pVehicle->m_nVehicleFlags.bWanted &&
		m_WantedLevel > WANTED_LEVEL2 &&
		!CTheScripts::GetPlayerIsOnAMission() )
	{
//		char *pText = TheText.Get("WNT_UCR");
//		CMessages::AddMessage(pText, -1, NULL, -1, 1000*4, 2, true, false);
		//@@: location CWANTED_PLAYERENTERED_VEHICLE_REPORT_CAR
		NA_POLICESCANNER_ENABLED_ONLY(g_PoliceScanner.ReportPoliceSpottingPlayerVehicle(pVehicle));
	}


	// Otherwise don't reset the vehicles hot status, if it isn't hot the player will now be harder to spot
}

static bool MakeAnonymousDamageCB(void* pItem, void* data)
{
	CPed* pPed = static_cast<CPed*>(pItem);
	CPed* pPlayer = static_cast<CPed*>(data);

	if (pPed && pPlayer)
	{
		if(!pPed->IsPlayer() && pPed->IsLawEnforcementPed())
		{
			if (!pPed->IsNetworkClone())
			{
				CTaskThreatResponse* pThreatResponseTask = static_cast<CTaskThreatResponse*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_THREAT_RESPONSE));
				if(pThreatResponseTask)
				{
					if (pThreatResponseTask->GetTargetPositionEntity() == pPlayer)
					{
						COrder* pOrder = pPed->GetPedIntelligence()->GetOrder();
						if (pOrder && pOrder->IsLocallyControlled())
						{
							pOrder->RemoveOrderFromEntity();
						}
					}
				}
			}

			CPedTargetting* pPedTargetting = pPed->GetPedIntelligence()->GetTargetting( false );
			if(pPedTargetting)
				pPedTargetting->RemoveThreat(pPlayer);
		}

		pPed->MakeAnonymousDamageByEntity(pPlayer);

		CTaskDyingDead* pTaskDyingDead = static_cast<CTaskDyingDead*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_DYING_DEAD));
		if(pTaskDyingDead)
		{
			pTaskDyingDead->MakeAnonymousDamageByEntity(pPlayer);
		}
	}

	return true;
}

// Name			:	UpdateWantedLevel
// Purpose		:	Update wanted level
// Parameters	:	None
// Returns		:	Nothing

void CWanted::UpdateWantedLevel(const Vector3 &Coors, const bool bCarKnown, const bool bRefocusSeach)
{
	const eWantedLevel OldWantedLevel = m_WantedLevel;

	if (m_nWantedLevel > nMaximumWantedLevel)
	{
		m_nWantedLevel = nMaximumWantedLevel;
	}

	if (m_nWantedLevel >= CDispatchData::GetWantedLevelThreshold(WANTED_LEVEL5))
	{
		m_WantedLevel = WANTED_LEVEL5;
		m_nMaxCopCarsInPursuit = 3;
		m_nMaxCopsInPursuit = 10;
		m_nChanceOnRoadBlock = 30;
	}
	else if (m_nWantedLevel >= CDispatchData::GetWantedLevelThreshold(WANTED_LEVEL4))
	{
		m_WantedLevel = WANTED_LEVEL4;
		m_nMaxCopCarsInPursuit = 2;
		m_nMaxCopsInPursuit = 6;
		m_nChanceOnRoadBlock = 18;
	}
	else if (m_nWantedLevel >= CDispatchData::GetWantedLevelThreshold(WANTED_LEVEL3))
	{
		m_WantedLevel = WANTED_LEVEL3;
		m_nMaxCopCarsInPursuit = 2;
		m_nMaxCopsInPursuit = 4;
		m_nChanceOnRoadBlock = 12;
	}
	else if (m_nWantedLevel >= CDispatchData::GetWantedLevelThreshold(WANTED_LEVEL2))
	{
		m_WantedLevel = WANTED_LEVEL2;
		m_nMaxCopCarsInPursuit = 2;
		m_nMaxCopsInPursuit = 3;
		m_nChanceOnRoadBlock = 0;
	}
	else if (m_nWantedLevel >= CDispatchData::GetWantedLevelThreshold(WANTED_LEVEL1))
	{
		m_WantedLevel = WANTED_LEVEL1;
		m_nMaxCopCarsInPursuit = 1;
		m_nMaxCopsInPursuit = 1;
		m_nChanceOnRoadBlock = 0;
	}
	else
	{
		if (WANTED_CLEAN != m_WantedLevel)
		{
			m_ReportedCrimes.Reset();

			if((m_WantedLevelLastSetFrom == WL_FROM_SCRIPT) || (m_WantedLevelLastSetFrom == WL_FROM_NETWORK))
			{
				g_fireMan.ClearCulpritFromAllFires(m_pPed);
				CGameWorld::ClearCulpritFromAllCarFires(m_pPed);
				CShockingEventsManager::ClearAllEventsWithSourceEntity(m_pPed);
				CExplosionManager::DisableCrimeReportingForExplosions(m_pPed);
				CProjectileManager::DisableCrimeReportingForExplosions(m_pPed);
			}

			//MP: Ensure the wanted level is fully cleared from all the cops that are chasing this wanted ped.  Otherwise when the enforcement peds can still be tracking this ped
			if (NetworkInterface::IsGameInProgress())
			{
				CPed* pWantedPed = GetPed(); //wanted ped
				wantedDebugf3("CWanted::UpdateWantedLevel--(WANTED_CLEAN != m_WantedLevel)--set m_WantedLevel = WANTED_CLEAN--pWantedPed[%p] if valid invoke MakeAnonymousDamageCB",pWantedPed);
				if (pWantedPed)
				{
					CPed::GetPool()->ForAll(MakeAnonymousDamageCB, (void*) pWantedPed);
				}
			}

#if __BANK
			CTaskCombat::ms_EnemyAccuracyLog.RecordWantedLevelCleared();
#endif // __BANK
		}

		m_WantedLevel = WANTED_CLEAN;
		m_nNewWantedLevel = 0;
		m_nMaxCopCarsInPursuit = 0;
		m_nMaxCopsInPursuit = 0;
		m_nChanceOnRoadBlock = 0;
		m_fTimeSinceLastHostileAction = LARGE_FLOAT;
	}

	// Passengers comment the first time you get a wanted level.
	if( OldWantedLevel == WANTED_CLEAN && m_WantedLevel >= WANTED_LEVEL1 )
	{
		PassengersCommentOnPolice();
	}

	//Cops comment on swat being dispatched.
	if((OldWantedLevel < WANTED_LEVEL4) && (m_WantedLevel == WANTED_LEVEL4))
	{
		CopsCommentOnSwat();
	}

	// If it has changed we record the change so that HUD can flash
	if (OldWantedLevel != m_WantedLevel)
	{
		if (OldWantedLevel > m_WantedLevel)
		{
			m_iTimeFirstSpotted = 0;

			if(m_WantedLevel == WANTED_CLEAN)
			{
				// Set our WL cleared time and clear our culprit from any fires
				m_TimeWantedLevelCleared = fwTimer::GetTimeInMilliseconds();
				g_fireMan.ClearCulpritFromAllFires(m_pPed);
				CGameWorld::ClearCulpritFromAllCarFires(m_pPed);

				REPLAY_ONLY(ReplayBufferMarkerMgr::AddMarker(3000,3000,IMPORTANCE_LOWEST);)
			}
		}

		if (CScriptHud::iFakeWantedLevel == 0)  // only set it flashing if we dont have a fake wanted level
		{
			m_LastTimeWantedLevelChanged = NetworkInterface::GetSyncedTimeInMilliseconds();
		}		

		// If this is a new wanted level then we should delay the spawning of the police
		if(OldWantedLevel == WANTED_CLEAN && m_WantedLevel > WANTED_CLEAN)
		{
			// If we are getting a WL then we need to initialize the delay of law spawn (if desired) and the time until hidden evasion starts
			if(m_bShouldDelayResponse)
			{
				m_fTimeUntilEvasionStarts = CDispatchManager::GetInstance().DelayLawSpawn() + sm_Tunables.m_ExtraStartHiddenEvasionDelay;
			}
			else
			{
				m_fTimeUntilEvasionStarts = sm_Tunables.m_ExtraStartHiddenEvasionDelay;
			}
		}

		m_bShouldDelayResponse = true;

		DEV_ONLY(Displayf("UpdateWantedLevel. Old:%d New:%d (max:%d)\n", OldWantedLevel, m_WantedLevel, nMaximumWantedLevel);)
	}

	if (bRefocusSeach)
	{
		ReportPoliceSpottingPlayer(NULL, Coors, OldWantedLevel, bCarKnown);
	}

	//If back off activated then set no cops to be in pursuit.
	if(PoliceBackOff())
	{
		m_nMaxCopCarsInPursuit = 0;
		m_nMaxCopsInPursuit = 0;
		m_nChanceOnRoadBlock = 0;
	}
	
}

// Name			:	RegisterCrime
// Purpose		:	Updates wanted level according to the specified crime type
// Parameters	:	CrimeType - type of crime to register
// Returns		:	Nothing

void CWanted::RegisterCrime(eCrimeType CrimeType, const Vector3 &Pos, const CEntity* CrimeVictimId, bool bPoliceDontReallyCare, CEntity* pVictim)
{
	u32 Increment = 0;
	
	if (EvaluateCrime(CrimeType, bPoliceDontReallyCare, Increment))
	{
		CWanted::AddCrimeToQ(CrimeType, CrimeVictimId, Pos, false, bPoliceDontReallyCare, Increment, pVictim);
	}
}

// Name			:	RegisterCrime_Immediately
// Purpose		:	Updates wanted level according to the specified crime type
// Parameters	:	CrimeType - type of crime to register
// Returns		:	Nothing

void CWanted::RegisterCrime_Immediately(eCrimeType CrimeType, const Vector3 &Pos, const CEntity* CrimeVictimId, bool bPoliceDontReallyCare, CEntity* pVictim)
{
	u32 Increment = 0;
	bool bReportNow = false;

	if (EvaluateCrime(CrimeType, bPoliceDontReallyCare, Increment))
	{
		// Still put it in the Q so that it doesn't get reported by something else
		if (!CWanted::AddCrimeToQ(CrimeType, CrimeVictimId, Pos, true, bPoliceDontReallyCare, Increment, pVictim))
		{
			bReportNow = true;
		}
	}
	
	if (bReportNow)
	{
		// Report crime right here right fucking now! (If it wasn't reported already)
		ReportCrimeNow(CrimeType, Pos, bPoliceDontReallyCare, true, Increment, pVictim);
	}
	
}




// Name			:	SetWantedLevel
// Purpose		:
// Parameters	:
// Returns		:

void CWanted::SetWantedLevel(const Vector3 &Coors, s32 NewLev, s32 delay, SetWantedLevelFrom eSetWantedLevelFrom, bool bIncludeNetworkPlayers, PhysicalPlayerIndex causedByPlayerPhysicalIndex)
{
//	if (CCheat::IsCheatActive(CCheat::NOTWANTED_CHEAT))
//		return;
	
	// Set the player ID of the player that caused the new wanted level, applicable for example when this 
	// wanted level comes from a remote wanted player getting into the local player's car.
	if (NewLev == WANTED_CLEAN) // Reset causedBy if we are clearing the wanted level
	{
		if (eSetWantedLevelFrom == WL_FROM_SCRIPT)
		{
			m_uLastTimeWantedLevelClearedByScript = fwTimer::GetTimeInMilliseconds();
		}

		causedByPlayerPhysicalIndex = INVALID_PLAYER_INDEX;		
	}
	else if (causedByPlayerPhysicalIndex == INVALID_PLAYER_INDEX && m_pPed->GetNetworkObject())
	{
		causedByPlayerPhysicalIndex = m_pPed->GetNetworkObject()->GetPhysicalPlayerIndex();
	}	
	m_CausedByPlayerPhysicalIndex = causedByPlayerPhysicalIndex;

	if (NetworkInterface::IsGameInProgress() && (NewLev != WANTED_CLEAN) && m_pPed)
	{
		if (m_pPed->IsDead())
		{
			wantedDebugf3("CWanted::SetWantedLevel -- ped is dead -- disregard");
			return;
		}

		if (m_pPed->IsPlayer() && m_pPed->GetNetworkObject() && (eSetWantedLevelFrom != WL_FROM_SCRIPT)) //only enforce this if it isn't from script - script should be allowed to override this - lavalley
		{
			CNetObjPlayer* pNetObjPlayer = static_cast<CNetObjPlayer*>(m_pPed->GetNetworkObject());
			if (pNetObjPlayer && (pNetObjPlayer->GetRespawnInvincibilityTimer() > 0))
			{
				wantedDebugf3("CWanted::SetWantedLevel -- ped is respawning timer[%d] -- disregard",pNetObjPlayer->GetRespawnInvincibilityTimer());
				return;
			}
		}
	}

	if (NewLev != WANTED_CLEAN)
	{
		PlayerBrokeParole(Coors);
	}
	else if(eSetWantedLevelFrom == WL_FROM_CHEAT || eSetWantedLevelFrom == WL_FROM_SCRIPT)
	{
		SetTimeAmnestyEnds(NetworkInterface::GetSyncedTimeInMilliseconds() + sm_Tunables.m_DefaultAmnestyTime);
	}

	if (NewLev > MaximumWantedLevel)
	{
		wantedDebugf3("CWanted::SetWantedLevel -- revised -- (NewLev[%d] > MaximumWantedLevel[%d]) --> NewLev = MaximumWantedLevel",NewLev,MaximumWantedLevel);
		NewLev = MaximumWantedLevel;
	}

	if((NewLev != m_WantedLevel) || (m_WantedLevelLastSetFrom != eSetWantedLevelFrom))
	{
		AI_LOG_WITH_ARGS("CWanted::SetWantedLevel for %s ped %s (address %p) - Old: %d New: %d Delay: %d Coords: (%.2f, %.2f, %.2f) SetWantedLevelFrom: %s bIncludeNetworkPlayers:%d\n", AILogging::GetDynamicEntityIsCloneStringSafe(m_pPed), AILogging::GetDynamicEntityNameSafe(m_pPed), m_pPed, m_WantedLevel, NewLev, delay, Coors.x, Coors.y, Coors.z, sm_WantedLevelSetFromLocations[eSetWantedLevelFrom], bIncludeNetworkPlayers);
		DEV_ONLY(Displayf("SetWantedLevel. Cur:%d New:%d Delay:%d (%.2f, %.2f, %.2f) Cur-eSetWantedLevelFrom:%d New-eSetWantedLevelFrom:%d bIncludeNetworkPlayers:%d\n", m_WantedLevel, NewLev, delay, Coors.x, Coors.y, Coors.z,m_WantedLevelLastSetFrom,eSetWantedLevelFrom,bIncludeNetworkPlayers);)
#if __BANK
		wantedDebugf3("CWanted::SetWantedLevel ped:%p:%s:%s player:%s IsNetworkClone:%d Cur: %d New:%d Delay:%d (%.2f, %.2f, %.2f) Cur-eSetWantedLevelFrom:%d:%s New-eSetWantedLevelFrom:%d:%s bIncludeNetworkPlayers:%d"
			,m_pPed,m_pPed ? m_pPed->GetModelName() : "",m_pPed && m_pPed->GetNetworkObject() ? m_pPed->GetNetworkObject()->GetLogName() : ""
			,m_pPed && m_pPed->GetNetworkObject() && m_pPed->GetNetworkObject()->GetPlayerOwner() ? m_pPed->GetNetworkObject()->GetPlayerOwner()->GetGamerInfo().GetName() : "LOCALPLAYER"
			,m_pPed ? m_pPed->IsNetworkClone() : false
			,m_WantedLevel
			,NewLev
			,delay
			,Coors.x, Coors.y, Coors.z
			,m_WantedLevelLastSetFrom
			,sm_WantedLevelSetFromLocations[m_WantedLevelLastSetFrom]
			,eSetWantedLevelFrom
			,sm_WantedLevelSetFromLocations[eSetWantedLevelFrom]
			,bIncludeNetworkPlayers);
#else
		wantedDebugf3("CWanted::SetWantedLevel ped:%p:%s:%s player:%s IsNetworkClone:%d Cur: %d New:%d Delay:%d (%.2f, %.2f, %.2f) Cur-eSetWantedLevelFrom:%d New-eSetWantedLevelFrom:%d bIncludeNetworkPlayers:%d"
			,m_pPed,m_pPed ? m_pPed->GetModelName() : "",m_pPed && m_pPed->GetNetworkObject() ? m_pPed->GetNetworkObject()->GetLogName() : ""
			,m_pPed && m_pPed->GetNetworkObject() && m_pPed->GetNetworkObject()->GetPlayerOwner() ? m_pPed->GetNetworkObject()->GetPlayerOwner()->GetGamerInfo().GetName() : "LOCALPLAYER"
			,m_pPed ? m_pPed->IsNetworkClone() : false
			,m_WantedLevel
			,NewLev
			,delay
			,Coors.x, Coors.y, Coors.z
			,m_WantedLevelLastSetFrom
			,eSetWantedLevelFrom
			,bIncludeNetworkPlayers);
#endif
	}

	// Store where the WL set originated from
	m_WantedLevelLastSetFrom = eSetWantedLevelFrom;

	m_nNewWantedLevel = GetValueForWantedLevel(NewLev);

	if (bIncludeNetworkPlayers && NetworkInterface::IsGameInProgress())
	{
		bool bApplyWantedLevel = true;

		if (m_pPed->IsNetworkClone() && m_pPed->GetNetworkObject()->GetPlayerOwner())
		{
			// assume ped is a player - won't handle other peds at the moment
			Assert(m_pPed->IsPlayer());
			wantedDebugf3("CWanted::SetWantedLevel-->invoke CAlterWantedLevelEvent::Trigger 1");
            CAlterWantedLevelEvent::Trigger(*m_pPed->GetNetworkObject()->GetPlayerOwner(), NewLev, delay, causedByPlayerPhysicalIndex);
			bApplyWantedLevel = false;
		}

		// if there are any other players in the vehicle, they get the wanted points for this crime too
		if (m_pPed->IsPlayer() && m_pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
		{
			CVehicle* pVehicle = m_pPed->GetMyVehicle();

			if (AssertVerify(pVehicle))
			{
				SetWantedLevelForVehiclePassengers(pVehicle,Coors,NewLev,delay,eSetWantedLevelFrom,causedByPlayerPhysicalIndex);

				if (CTrailer* pAttachedTrailer = pVehicle->GetAttachedTrailer())
				{
					if (pAttachedTrailer->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_SET_WANTED_FOR_ATTACHED_VEH))
						SetWantedLevelForVehiclePassengers(pAttachedTrailer,Coors,NewLev,delay,eSetWantedLevelFrom,causedByPlayerPhysicalIndex);
				}

				if (pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_SET_WANTED_FOR_ATTACHED_VEH))
				{
					if (pVehicle->InheritsFromTrailer())
					{
						CTrailer* pTrailer = static_cast<CTrailer*>(pVehicle);
						if (const CVehicle* pAttachedVehicle = pTrailer->GetAttachedToParent())
						{
							SetWantedLevelForVehiclePassengers(pAttachedVehicle,Coors,NewLev,delay,eSetWantedLevelFrom,causedByPlayerPhysicalIndex);
						}
					}
				}

			}
		}

		if (!bApplyWantedLevel)
			return;
	}
	
	bool bShouldClearQdCrimes = false;
	switch (CCrime::sm_eReportCrimeMethod)
	{
	case CCrime::REPORT_CRIME_DEFAULT:
		// Otherwise it would go up immediately after reseting to 0
		bShouldClearQdCrimes = true;
		break;
	case CCrime::REPORT_CRIME_ARCADE_CNC:
		// Don't clear the queue here. UpdateCrimesQ() clears any processed and expired crimes on update
		bShouldClearQdCrimes = false;
		break;
	}
	
	if (bShouldClearQdCrimes)
	{
		ClearQdCrimes();
	}

	m_TimeWhenNewWantedLevelTakesEffect = NetworkInterface::GetSyncedTimeInMilliseconds() + delay;
	if (delay == 0)
	{
		m_nWantedLevel = m_nNewWantedLevel;
		UpdateWantedLevel(Coors);
		m_TimeWhenNewWantedLevelTakesEffect = 0;
	}
}

void CWanted::SetWantedLevelForVehiclePassengers(const CVehicle* pVehicle, const Vector3 &Coors, s32 NewLev, s32 delay, SetWantedLevelFrom eSetWantedLevelFrom, PhysicalPlayerIndex causedByPlayerPhysicalIndex)
{
	for (s32 i=0; i<pVehicle->GetSeatManager()->GetMaxSeats(); i++)
	{
		CPed* pPed = pVehicle->GetSeatManager()->GetPedInSeat(i);

		if (pPed && pPed->IsPlayer() && pPed != m_pPed)
		{
			if (pPed->IsNetworkClone() && pPed->GetNetworkObject()->GetPlayerOwner())
			{
				// give the wanted points to the other player
				wantedDebugf3("CWanted::SetWantedLevel-->invoke CAlterWantedLevelEvent::Trigger 2");
				CAlterWantedLevelEvent::Trigger(*pPed->GetNetworkObject()->GetPlayerOwner(), NewLev, delay, causedByPlayerPhysicalIndex);

				CWanted& wanted = pPed->GetPlayerInfo()->GetWanted();
				bool bReducedToZero = (wanted.m_WantedLevel && (NewLev == 0));
				if (bReducedToZero)
				{
					//ensure that the remote version of the ped wanted system is reset fully here too - because the above trigger might arrive at the owned ped and the owned ped might already be at zero
					//so then the replication of that information wouldn't get sent out - and here the wanted system for that remote ped wouldn't get reset - so just do it here too
					wantedDebugf3("CWanted::SetWantedLevel-->bReducedToZero remote ped-->local clear through wanted.SetWantedLevel(0) -- pPed[%p][%s][%s]",pPed,pPed ? pPed->GetModelName() : "",pPed && pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "");
					wanted.SetWantedLevel(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()), 0, 0, eSetWantedLevelFrom, false, causedByPlayerPhysicalIndex);
				}
			}
			else
			{
				// give the wanted points to our player
				SetWantedLevel(Coors, NewLev, delay, eSetWantedLevelFrom, false, causedByPlayerPhysicalIndex);
			}
		}
	}
};


void CWanted::CheatWantedLevel(s32 NewLev)
{
	// Already at the maximum level
	if( NewLev > WANTED_LEVEL5 )
	{
		return;
	}

	wantedDebugf3("CWanted::CheatWantedLevel - NewLev[%d], current MaximumWantedLevel[%d]", NewLev, MaximumWantedLevel);

	if (NewLev == WANTED_CLEAN && m_pPed)
	{
		g_fireMan.ClearCulpritFromAllFires(m_pPed);
		CGameWorld::ClearCulpritFromAllCarFires(m_pPed);
		CShockingEventsManager::ClearAllEventsWithSourceEntity(m_pPed);
		CWeaponDamageEvent::ClearDelayedKill();
	}

	if (NewLev > MaximumWantedLevel)
		SetMaximumWantedLevel(NewLev);
	
	SetWantedLevel(CGameWorld::FindLocalPlayerCoors(), NewLev, 0, WL_FROM_CHEAT, true, m_pPed->GetNetworkObject() ? m_pPed->GetNetworkObject()->GetPhysicalPlayerIndex() : NETWORK_INVALID_OBJECT_ID);
	UpdateWantedLevel(CGameWorld::FindLocalPlayerCoors());
}

void CWanted::SetWantedLevelNoDrop(const Vector3 &Coors, s32 NewLev, s32 delay, SetWantedLevelFrom eSetWantedLevelFrom, bool bIncludeNetworkPlayers, PhysicalPlayerIndex causedByPlayerPhysicalIndex)
{
#if !__NO_OUTPUT
	if ((Channel_wanted.FileLevel >= DIAG_SEVERITY_DEBUG3) || (Channel_wanted.TtyLevel >= DIAG_SEVERITY_DEBUG3))
	{
		if((NewLev != m_WantedLevel) || (m_WantedLevelLastSetFrom != eSetWantedLevelFrom))
		{
			s32 iDesiredValue = GetValueForWantedLevel(NewLev);
#if __BANK
			wantedDebugf3("CWanted::SetWantedLevelNoDrop ped[%p][%s][%s] NewLev[%d] m_WantedLevel[%d] iDesiredValue[%d] m_nWantedLevel[%d] m_nNewWantedLevel[%d] delay[%d] eSetWantedLevelFrom[%d][%s] bIncludeNetworkPlayers[%d] m_TimeWhenNewWantedLevelTakesEffect[%u] getsyncedtimeinmilliseconds[%u]",m_pPed,m_pPed ? m_pPed->GetModelName() : "",m_pPed && m_pPed->GetNetworkObject() ? m_pPed->GetNetworkObject()->GetLogName() : "",NewLev,m_WantedLevel,iDesiredValue,m_nWantedLevel,m_nNewWantedLevel,delay,eSetWantedLevelFrom,sm_WantedLevelSetFromLocations[eSetWantedLevelFrom],bIncludeNetworkPlayers,m_TimeWhenNewWantedLevelTakesEffect,NetworkInterface::GetSyncedTimeInMilliseconds());
#else
			wantedDebugf3("CWanted::SetWantedLevelNoDrop ped[%p][%s][%s] NewLev[%d] m_WantedLevel[%d] iDesiredValue[%d] m_nWantedLevel[%d] m_nNewWantedLevel[%d] delay[%d] eSetWantedLevelFrom[%d] bIncludeNetworkPlayers[%d] m_TimeWhenNewWantedLevelTakesEffect[%u] getsyncedtimeinmilliseconds[%u]",m_pPed,m_pPed ? m_pPed->GetModelName() : "",m_pPed && m_pPed->GetNetworkObject() ? m_pPed->GetNetworkObject()->GetLogName() : "",NewLev,m_WantedLevel,iDesiredValue,m_nWantedLevel,m_nNewWantedLevel,delay,eSetWantedLevelFrom,bIncludeNetworkPlayers,m_TimeWhenNewWantedLevelTakesEffect,NetworkInterface::GetSyncedTimeInMilliseconds());
#endif
		}
	}
#endif

	if (m_WantedLevelBeforeParole != WANTED_CLEAN)
	{
		PlayerBrokeParole(Coors);
	}

	if (NewLev <= m_WantedLevel)
	{
		wantedDebugf3("CWanted::SetWantedLevelNoDrop--(NewLev <= m_WantedLevel)-->return");
		return;
	}
	
	s32 iDesiredValue = GetValueForWantedLevel(NewLev);
	if(iDesiredValue < m_nWantedLevel)
	{
		wantedDebugf3("CWanted::SetWantedLevelNoDrop--(iDesiredValue < m_nWantedLevel)-->return");
		return;
	}

	if(m_TimeWhenNewWantedLevelTakesEffect > 0 && NetworkInterface::GetSyncedTimeInMilliseconds() < m_TimeWhenNewWantedLevelTakesEffect && iDesiredValue <= m_nNewWantedLevel)
	{
		wantedDebugf3("CWanted::SetWantedLevelNoDrop--(m_TimeWhenNewWantedLevelTakesEffect > 0 && NetworkInterface::GetSyncedTimeInMilliseconds() < m_TimeWhenNewWantedLevelTakesEffect && iDesiredValue <= m_nNewWantedLevel)-->return");
		return;
	}

	SetWantedLevel(Coors, NewLev, delay, eSetWantedLevelFrom, bIncludeNetworkPlayers, causedByPlayerPhysicalIndex);
}

///////////////////////////////////////////////////////////
// FUNCTION: SetOneStarParoleNoDrop
// PURPOSE:  If the player has zero stars this will trigger the parole thing.
//           This should be used when the player is spotted commiting a minor offence like
//			 driving on the wrong side of the road.
///////////////////////////////////////////////////////////

void CWanted::SetOneStarParoleNoDrop()
{
	if (GetWantedLevel() == WANTED_CLEAN)
	{
		if (m_WantedLevelBeforeParole == WANTED_CLEAN)
		{
			m_WantedLevelBeforeParole = WANTED_LEVEL1;
			m_nWantedLevelBeforeParole = (CDispatchData::GetWantedLevelThreshold(WANTED_LEVEL1) + CDispatchData::GetWantedLevelThreshold(WANTED_LEVEL2))/2;
			m_TimeOfParole = NetworkInterface::GetSyncedTimeInMilliseconds();
		}
	}
}

void CWanted::ClearParole()
{
	m_TimeOfParole = 0;
	m_nWantedLevelBeforeParole = 0;
	m_WantedLevelBeforeParole = WANTED_CLEAN;
	m_MaintainFlashingStarAfterOffence = false;
}

void CWanted::PlayerBrokeParole(const Vector3 &Coors)
{
	if (m_WantedLevelBeforeParole)
	{
		DEV_ONLY(Displayf("Wanted: Player broke parole:%d\n", m_WantedLevelBeforeParole);)

		s32 newWantedLevel = m_WantedLevelBeforeParole;
		ClearParole();
		SetWantedLevelNoDrop(Coors, newWantedLevel, 0, WL_FROM_BROKE_PAROLE);
	}
}



/*
void CWanted::ClearWantedLevelAndGoOnParole()
{
	StatsInterface::IncrementStat(STAT_WANTED_STARS_EVADED, (float)m_WantedLevel);  // add the current wanted level to the evasion in the stats

	m_nWantedLevelBeforeParole = FindPlayerWanted()->m_nWantedLevel;
	m_WantedLevelBeforeParole = FindPlayerWanted()->m_WantedLevel;
	m_TimeOfParole = NetworkInterface::GetSyncedTimeInMilliseconds();
	m_nWantedLevel = 0;
	m_WantedLevel = WANTED_CLEAN;
	ResetPolicePursuit();
}
*/


void CWanted::SetMaximumWantedLevel(s32 NewMaxLev)
{
	wantedDebugf3("CWanted::SetMaximumWantedLevel - NewMaxLev[%d], current MaximumWantedLevel[%d]", NewMaxLev, MaximumWantedLevel);
	switch (NewMaxLev)
	{
		case WANTED_LEVEL1 :
				MaximumWantedLevel = WANTED_LEVEL1;
				nMaximumWantedLevel = CDispatchData::GetWantedLevelThreshold(WANTED_LEVEL1) + (CDispatchData::GetWantedLevelThreshold(WANTED_LEVEL2) - CDispatchData::GetWantedLevelThreshold(WANTED_LEVEL1))/2;
				break;
				
		case WANTED_LEVEL2 :
				MaximumWantedLevel = WANTED_LEVEL2;
				nMaximumWantedLevel = CDispatchData::GetWantedLevelThreshold(WANTED_LEVEL2) + (CDispatchData::GetWantedLevelThreshold(WANTED_LEVEL3) - CDispatchData::GetWantedLevelThreshold(WANTED_LEVEL2))/2;
				break;				
				
		case WANTED_LEVEL3 :
				MaximumWantedLevel = WANTED_LEVEL3;
				nMaximumWantedLevel = CDispatchData::GetWantedLevelThreshold(WANTED_LEVEL3) + (CDispatchData::GetWantedLevelThreshold(WANTED_LEVEL4) - CDispatchData::GetWantedLevelThreshold(WANTED_LEVEL3))/2;
				break;				
				
		case WANTED_LEVEL4 :
				MaximumWantedLevel = WANTED_LEVEL4;
				nMaximumWantedLevel = CDispatchData::GetWantedLevelThreshold(WANTED_LEVEL4) + (CDispatchData::GetWantedLevelThreshold(WANTED_LEVEL5) - CDispatchData::GetWantedLevelThreshold(WANTED_LEVEL4))/2;
				break;				
				
		case WANTED_LEVEL5 :
				MaximumWantedLevel = WANTED_LEVEL5;
				nMaximumWantedLevel = CDispatchData::GetWantedLevelThreshold(WANTED_LEVEL5) + CDispatchData::GetWantedLevelThreshold(WANTED_LEVEL5)/2;
				break;					
				
		case WANTED_CLEAN :
				MaximumWantedLevel = WANTED_CLEAN;
				nMaximumWantedLevel = 0;
				break;			
				
		default :	
				Assert(0);
				break;
	}
}

bool CWanted::ShouldCompensateForSlowMovingVehicle(const CEntity* pEntity, const CEntity* pTargetEntity)
{
	//Ensure we are in MP.
	if(!NetworkInterface::IsGameInProgress())
	{
		return false;
	}

	//Ensure the entity is valid.
	if(!pEntity)
	{
		return false;
	}

	//Ensure the entity is a ped.
	if(!pEntity->GetIsTypePed())
	{
		return false;
	}
	const CPed* pPed = static_cast<const CPed *>(pEntity);

	//Ensure the target entity is valid.
	if(!pTargetEntity)
	{
		return false;
	}

	//Ensure the entity is a ped.
	if(!pTargetEntity->GetIsTypePed())
	{
		return false;
	}
	const CPed* pTargetPed = static_cast<const CPed *>(pTargetEntity);

	//Ensure the ped is random.
	if(!pPed->PopTypeIsRandom())
	{
		return false;
	}

	//Ensure the ped is law enforcement.
	if(!pPed->IsLawEnforcementPed())
	{
		return false;
	}

	//Ensure the target is a player.
	if(!pTargetPed->IsAPlayerPed())
	{
		return false;
	}

	//Ensure the target is wanted.
	if(pTargetPed->GetPlayerWanted()->GetWantedLevel() <= WANTED_CLEAN)
	{
		return false;
	}

	//Ensure the target is in a vehicle.
	if(!pTargetPed->GetIsInVehicle())
	{
		return false;
	}

	if( !pTargetPed->GetPlayerInfo() )
	{
		return false;
	}

	// If the player is moving or has moved recently, dont compensate
	static dev_float s_fTimeStill = 5.0f;
	if( pTargetPed->GetPlayerInfo()->GetTimeSinceLastDrivingAtSpeed() < s_fTimeStill )
	{
		return false;
	}

	return true;
}

bool CWanted::GetWithinAmnestyTime() const
{ 
	return m_TimeCurrentAmnestyEnds && m_TimeCurrentAmnestyEnds > NetworkInterface::GetSyncedTimeInMilliseconds();
}

void CWanted::SetHiddenEvasionTime(u32 currentTime)
{
	m_iTimeHiddenEvasionStarted = currentTime - m_iTimeHiddenWhenEvasionReset;
	m_iTimeHiddenWhenEvasionReset = 0;
}

void CWanted::ForceStartHiddenEvasion()
{
	SetHiddenEvasionTime(NetworkInterface::GetSyncedTimeInMilliseconds());

	m_fTimeUntilEvasionStarts = 0.0f;
	m_HasLeftInitialSearchArea = true;
	m_vInitialSearchPosition = m_pPed->GetTransform().GetPosition();
	m_iTimeSearchLastRefocused = MIN(m_iTimeSearchLastRefocused, (NetworkInterface::GetSyncedTimeInMilliseconds() - FORCE_HIDDEN_EVASION_SEARCHING_TIME));
}

bool CWanted::ShouldRefocusSearchForCrime(eCrimeType CrimeType, eWantedLevel oldWantedLevel)
{
	if(oldWantedLevel == WANTED_CLEAN)
	{
		return true;
	}

	if(CrimeType > CRIME_LAST_ONE_NO_REFOCUS && CrimeType != CRIME_RUNOVER_PED)
	{
		return true;
	}

	return false;
}

void CWanted::ResetHiddenEvasion(const Vector3 &PlayerCoors, u32 currentTime, bool bPlayerSeen)
{
	wantedDebugf3("CWanted::ResetHiddenEvasion--m_HasLeftInitialSearchArea=false");

	m_HasLeftInitialSearchArea = false;
	m_vInitialSearchPosition = VECTOR3_TO_VEC3V(PlayerCoors);

	if(bPlayerSeen)
	{
		m_iTimeHiddenWhenEvasionReset = 0;
		m_iTimeHiddenEvasionStarted = 0;
	}
	else if(m_iTimeHiddenEvasionStarted != 0)
	{
		s32 iTimeSpentHidden = (m_iTimeHiddenWhenEvasionReset == 0) ? (currentTime - m_iTimeHiddenEvasionStarted) : m_iTimeHiddenWhenEvasionReset;
		iTimeSpentHidden -= sm_Tunables.m_DefaultHiddenEvasionTimeReduction;

		if(iTimeSpentHidden <= 0)
		{
			m_iTimeHiddenEvasionStarted = 0;
			m_iTimeHiddenWhenEvasionReset = 0;
		}
		else
		{
			m_iTimeHiddenWhenEvasionReset = (u32)iTimeSpentHidden;
		}
	}
}

void CWanted::ResetTimeInitialSearchAreaWillTimeOut(u32 currentTime, bool bPlayerSeen)
{
	u32 iCurrentTimePlusTimeout = bPlayerSeen ? currentTime + sm_Tunables.m_InitialAreaTimeoutWhenSeen : currentTime + sm_Tunables.m_InitialAreaTimeoutWhenCrimeReported;
	m_iTimeInitialSearchAreaWillTimeOut = rage::Max(iCurrentTimePlusTimeout, m_iTimeInitialSearchAreaWillTimeOut);
}

void CWanted::RestartPoliceSearch(const Vector3 &PlayerCoors, u32 currentTime, bool bPlayerSeen)
{
	m_LastSpottedByPolice = PlayerCoors;
	m_CurrentSearchAreaCentre = VEC3V_TO_VECTOR3(CDispatchHelperPedPositionToUse::Get(*m_pPed));
	m_iTimeLastSpotted = currentTime;
	m_iTimeSearchLastRefocused = currentTime;
	ResetHiddenEvasion(PlayerCoors, currentTime, bPlayerSeen);
	ResetTimeInitialSearchAreaWillTimeOut(currentTime, bPlayerSeen);
}

u32 CWanted::GetCharacterIndex() const
{
	u32 index = m_WantedLevelCharId;

	if (index == CHAR_INVALID)
		index = StatsInterface::GetCharacterIndex();

	return index;
}

// Works out how many police units (roughly) there are at these coordinates

s32 CWanted::WorkOutPolicePresence(const Vec3V& Coors, float Radius)
{
	// Then get peds through the spatial array
	ScalarV scMaxRadius = LoadScalar32IntoScalarV( MAX(Radius, CDispatchData::GetOnScreenImmediateDetectionRange()) );
	ScalarV scOffScreenRadiusSq;

	// If the radius passed in is greater than the on screen radius, we won't need to do the extra check in the loop for off screen peds
	bool bCheckOffScreenRadius = false;
	if(Radius < CDispatchData::GetOnScreenImmediateDetectionRange())
	{
		bCheckOffScreenRadius = true;
		scOffScreenRadiusSq = LoadScalar32IntoScalarV(Radius);
		scOffScreenRadiusSq = (scOffScreenRadiusSq * scOffScreenRadiusSq);
	}

	int maxNumPeds = CPed::GetPool()->GetSize();
	CSpatialArray::FindResult* pPedResults = Alloca(CSpatialArray::FindResult, maxNumPeds);
	int numPedsFound = CPed::ms_spatialArray->FindInSphere(Coors, scMaxRadius, pPedResults, maxNumPeds);

	for(int i = 0; i < numPedsFound; i++)
	{
		CPed* pFoundPed = CPed::GetPedFromSpatialArrayNode(pPedResults[i].m_Node);

		// If our off screen radius was smaller than the on screen one and this ped is off screen then we need to check their distance
		if(bCheckOffScreenRadius && !pFoundPed->GetPedConfigFlag(CPED_CONFIG_FLAG_VisibleOnScreen))
		{
			ScalarV scDistSq = LoadScalar32IntoScalarV(pPedResults[i].m_DistanceSq);
			if(IsGreaterThanAll(scDistSq, scOffScreenRadiusSq))
			{
				continue;
			}
		}

		// If we find a valid ped then return!
		if(!pFoundPed->ShouldBeDead() && !pFoundPed->IsDeadByMelee() && pFoundPed->ShouldBehaveLikeLaw())
		{
			return 1;
		}
	}

	// First use the spatial array to check the vehicles
	int maxNumVehicles = (int) CVehicle::GetPool()->GetSize();
	CSpatialArray::FindResult* pVehicleResults = Alloca(CSpatialArray::FindResult, maxNumVehicles);
	int numVehiclesFound = CVehicle::ms_spatialArray->FindInSphere(Coors, Radius, pVehicleResults, maxNumVehicles);

	for(int i = 0; i < numVehiclesFound; i++)
	{
		// If we find a valid vehicle then return!
		CVehicle* pFoundVehicle = CVehicle::GetVehicleFromSpatialArrayNode(pVehicleResults[i].m_Node);
		if(pFoundVehicle->IsLawEnforcementVehicle() && pFoundVehicle->IsUsingPretendOccupants())
		{
			return 1;
		}
	}

	return 0;
}

// Alerts nearby police by putting them into combat with the criminal (or attempting an arrest initially)
void CWanted::AlertNearbyPolice(const Vec3V& Coors, float Radius, CPed* pCriminal, bool bAttemptArrest)
{
	wantedDebugf3("CWanted::AlertNearbyPolice Coors[%f %f %f] Radius[%f] pCriminal[%p] bAttemptArrest[%d] --> CTaskThreatResponse",Coors.GetXf(),Coors.GetYf(),Coors.GetZf(),Radius,pCriminal,bAttemptArrest);
	if(!pCriminal)
	{
		return;
	}

	CTaskThreatResponse* pThreatResponseTask = rage_new CTaskThreatResponse(pCriminal);
	pThreatResponseTask->GetConfigFlagsForCombat().ChangeFlag(CTaskCombat::ComF_ArrestTarget, bAttemptArrest);

	CEventGivePedTask giveTaskEvent(PED_TASK_PRIORITY_PRIMARY, pThreatResponseTask, false);
	giveTaskEvent.SetTunableDelay(CEventRespondedToThreat::sm_Tunables.m_MinDelayTimer, CEventRespondedToThreat::sm_Tunables.m_MaxDelayTimer);

	float fOnScreenRadius = MAX(Radius, CDispatchData::GetOnScreenImmediateDetectionRange());
	CEntityIterator entityIterator( IteratePeds | UseOnScreenPedDistance, NULL, &Coors, Radius, fOnScreenRadius);
	CDynamicEntity* pEntity = static_cast<CDynamicEntity*>(entityIterator.GetNext());
	while( pEntity )
	{
		if(pEntity->GetIsTypePed())
		{
			CPed* pPed = (CPed*)pEntity;
			if(!pPed->ShouldBeDead() && !pPed->GetBlockingOfNonTemporaryEvents() && pPed->PopTypeIsRandom() && pPed->ShouldBehaveLikeLaw())
			{
				pPed->GetPedIntelligence()->AddEvent(giveTaskEvent);
			}
		}

		pEntity = static_cast<CDynamicEntity*>(entityIterator.GetNext());
	}
}

void CWanted::TriggerCopArrivalAudioFromPed(CPed* pCopPed)
{
	if(m_bHasCopArrivalAudioPlayed)
	{
		return;
	}

	if(!pCopPed || !pCopPed->GetSpeechAudioEntity())
	{
		return;
	}

	if(pCopPed->GetVehiclePedInside())
	{
		if(pCopPed->GetVehiclePedInside()->GetVehicleType() != VEHICLE_TYPE_CAR)
		{
			return;
		}

		pCopPed->GetSpeechAudioEntity()->SayWhenSafe("COP_ARRIVAL_ANNOUNCE_MEGAPHONE");
	}
	else
	{
		pCopPed->GetSpeechAudioEntity()->SayWhenSafe("COP_ARRIVAL_ANNOUNCE");
	}

	m_bHasCopArrivalAudioPlayed = true;
}

bool CWanted::TriggerCopSeesWeaponAudio(CPed* pCopPed, bool bIsWeaponSwitch)
{
	if(m_bHasCopSeesWeaponAudioPlayed && !bIsWeaponSwitch)
	{
		return false;
	}

	if(!pCopPed)
	{
		//Find the closest alive cop with line of sight
		CEntityScannerIterator pedList = m_pPed->GetPedIntelligence()->GetNearbyPeds();
		for(CEntity* pNearbyEntity = pedList.GetFirst(); pNearbyEntity != NULL; pNearbyEntity = pedList.GetNext())
		{
			CPed* pNearbyPed = static_cast<CPed *>(pNearbyEntity);
			if(pNearbyPed->IsRegularCop() && !pNearbyPed->IsInjured() && pNearbyPed->PopTypeIsRandom() && pNearbyPed->GetSpeechAudioEntity())
			{
				CPedTargetting* pPedTargeting = pNearbyPed->GetPedIntelligence()->GetTargetting(false);
				if(pPedTargeting && pPedTargeting->GetLosStatus(m_pPed) == Los_clear)
				{
					pCopPed = pNearbyPed;
					break;
				}
			}
		}
	}
	
	if(!pCopPed || !pCopPed->GetSpeechAudioEntity())
	{
		return false;
	}

	// Set this here because we might possible return before the end of the function
	m_bHasCopSeesWeaponAudioPlayed = true;

	const CWeaponInfo* pTargetWeaponInfo = bIsWeaponSwitch ? m_pPed->GetWeaponManager()->GetEquippedWeaponInfo() : m_pPed->GetWeaponManager()->GetCurrentWeaponInfoForHeldObject();
	if(pTargetWeaponInfo && !pTargetWeaponInfo->GetIsUnarmed())
	{
		const CAmmoInfo* pAmmoInfo = pTargetWeaponInfo->GetAmmoInfo();
		if(pAmmoInfo && (pAmmoInfo->GetClassId() == CAmmoRocketInfo::GetStaticClassId()) )
		{
			pCopPed->GetSpeechAudioEntity()->SayWhenSafe("COP_SEES_ROCKET_LAUNCHER");
		}
		else if(pTargetWeaponInfo->GetHash() == WEAPONTYPE_GRENADELAUNCHER)
		{
			pCopPed->GetSpeechAudioEntity()->SayWhenSafe("COP_SEES_GRENADE_LAUNCHER");
		}
		else if(pTargetWeaponInfo->GetHash() == WEAPONTYPE_GRENADE)
		{
			pCopPed->GetSpeechAudioEntity()->SayWhenSafe("COP_SEES_GRENADE");
		}
		else if(pTargetWeaponInfo->GetHash() == WEAPONTYPE_MINIGUN)
		{
			pCopPed->GetSpeechAudioEntity()->SayWhenSafe("COP_SEES_MINI_GUN");
		}
		else if(pTargetWeaponInfo->GetIsGun())
		{
			pCopPed->GetSpeechAudioEntity()->SayWhenSafe("COP_SEES_GUN");
		}
		else
		{
			pCopPed->GetSpeechAudioEntity()->SayWhenSafe("COP_SEES_WEAPON");
		}

		return true;
	}

	return false;
}

bool CWanted::ShouldTriggerCopSeesWeaponAudioForWeaponSwitch(const CWeaponInfo* pDrawingWeaponInfo)
{
	if(!pDrawingWeaponInfo)
	{
		return false;
	}

	const CWeaponInfo* pLastWeaponInfo = m_pPed->GetWeaponManager()->GetLastEquippedWeaponInfo();
	if(pLastWeaponInfo == pDrawingWeaponInfo)
	{
		return false;
	}

	if(pLastWeaponInfo)
	{
		const CAmmoInfo* pLastAmmoInfo = pLastWeaponInfo->GetAmmoInfo();
		if( (pLastAmmoInfo && pLastAmmoInfo->GetClassId() == CAmmoRocketInfo::GetStaticClassId()) ||
			pLastWeaponInfo->GetHash() == WEAPONTYPE_GRENADELAUNCHER ||
			pLastWeaponInfo->GetHash() == WEAPONTYPE_GRENADE ||
			pLastWeaponInfo->GetHash() == WEAPONTYPE_MINIGUN )
		{
			return false;
		}

		const CAmmoInfo* pDrawingWeaponAmmoInfo = pDrawingWeaponInfo->GetAmmoInfo();
		if( (!pDrawingWeaponAmmoInfo || pDrawingWeaponAmmoInfo->GetClassId() != CAmmoRocketInfo::GetStaticClassId()) &&
			pDrawingWeaponInfo->GetHash() != WEAPONTYPE_GRENADELAUNCHER &&
			pDrawingWeaponInfo->GetHash() != WEAPONTYPE_GRENADE &&
			pDrawingWeaponInfo->GetHash() != WEAPONTYPE_MINIGUN )
		{
			return false;
		}
	}

	return true;
}

u32 CWanted::GetTimeSince(u32 iTimeLastHappenedInMs) const
{
	u32 timeSinceLastHappened = 0;
	if (iTimeLastHappenedInMs != 0)
	{
		u32 currTime = NetworkInterface::GetSyncedTimeInMilliseconds();

		// handle the time wrapping
		if (currTime < iTimeLastHappenedInMs)
		{
			timeSinceLastHappened = (MAX_UINT32 - iTimeLastHappenedInMs) + currTime;
		}
		else
		{
			timeSinceLastHappened = currTime - iTimeLastHappenedInMs;
		}

		timeSinceLastHappened = MIN(timeSinceLastHappened,  MAX_TIME_LAST_SEEN_COUNTER);
	}

	return timeSinceLastHappened;
}

u32 CWanted::GetTimeSinceLastSpotted() const
{
	return GetTimeSince(m_iTimeLastSpotted);
}

u32 CWanted::GetTimeSinceSearchLastRefocused() const
{
	return GetTimeSince(m_iTimeSearchLastRefocused);
}

u32 CWanted::GetTimeSinceHiddenEvasionStarted() const
{
	return GetTimeSince(m_iTimeHiddenEvasionStarted);
}

// Name			:	ClearQdCrimes
// Purpose		:	Goes through the crimes that have been Qd and clears them
// Parameters
// Returns

void CWanted::ClearQdCrimes()
{
	s32	C;

	for (C = 0; C < m_MaxCrimesBeingQd; C++)
	{
		CrimesBeingQd[C].CrimeType = CRIME_NONE;
		CrimesBeingQd[C].m_CrimeVictimId = NULL;
		CrimesBeingQd[C].m_Victim = NULL;
	}
}

// Name			:	AddCrimeToQ
// Purpose		:	Adds one crime to the Q if it's not already there
// Parameters
// Returns			true if this crime has already been reported (and should be ignored)
bool CWanted::AddCrimeToQ(eCrimeType ArgCrimeType, const CEntity* CrimeVictimId, const Vector3 &Coors, bool bArgAlreadyReported, bool bArgPoliceDontReallyCare, u32 Increment, CEntity* pVictim)
{
	s32	C;

	Assertf((Coors.x != 0.0f || Coors.y != 0.0f),"AddCrimeToQ at (0.0f, 0.0f)" );
	
	Assert(ArgCrimeType != CRIME_NONE);

	switch(CCrime::sm_eReportCrimeMethod)
	{
	case CCrime::REPORT_CRIME_DEFAULT:
		for (C = 0; C < m_MaxCrimesBeingQd; C++)
		{
			if (CrimesBeingQd[C].CrimeType == ArgCrimeType &&
				CrimesBeingQd[C].m_CrimeVictimId == CrimeVictimId)
			{
				if (CrimesBeingQd[C].bAlreadyReported)
				{
					return true;
				}
				if (bArgAlreadyReported)
				{
					CrimesBeingQd[C].bAlreadyReported = true;
					CrimesBeingQd[C].m_WitnessType = eWitnessedByLaw;
				}
				return false;		// Crime is already there
			}
		}

		break;

	case CCrime::REPORT_CRIME_ARCADE_CNC:
	{
		Tunables::CrimeInfo crimeInfo;
		GetCrimeInfo(ArgCrimeType, crimeInfo);

		if (crimeInfo.m_ReportDuplicates)
		{
			break;
		}

		for (C = 0; C < m_MaxCrimesBeingQd; C++)
		{
			if (CrimesBeingQd[C].CrimeType == ArgCrimeType)
			{
				return true;
			}
		}
		break;
	}
		
	}

	// Find a free one
	C = 0;
	while (C < m_MaxCrimesBeingQd && CrimesBeingQd[C].CrimeType != CRIME_NONE)
	{
		C++;
	}

	if (C < m_MaxCrimesBeingQd)
	{	// Found an empty slot
		CrimesBeingQd[C].ResetWitnesses();
		CrimesBeingQd[C].CrimeType = ArgCrimeType;
		CrimesBeingQd[C].Coors = Coors;
		CrimesBeingQd[C].TimeOfQing = NetworkInterface::GetSyncedTimeInMilliseconds();
		CrimesBeingQd[C].Lifetime = GetCrimeLifetime(ArgCrimeType);
		CrimesBeingQd[C].ReportDelay = GetCrimeReportDelay(ArgCrimeType);

		CrimesBeingQd[C].bAlreadyReported = bArgAlreadyReported;
		CrimesBeingQd[C].bPoliceDontReallyCare = bArgPoliceDontReallyCare;
		CrimesBeingQd[C].WantedScoreIncrement = Increment;

		switch (CCrime::sm_eReportCrimeMethod)
		{
		case CCrime::REPORT_CRIME_ARCADE_CNC:
			CrimesBeingQd[C].m_CrimeVictimId = nullptr;
			CrimesBeingQd[C].m_Victim = nullptr;
			CrimesBeingQd[C].m_WitnessType = eWitnessedNone;
			CrimesBeingQd[C].bMustBeWitnessed = false;
			CrimesBeingQd[C].bMustNotifyLawEnforcement = false;
			CrimesBeingQd[C].bOnlyVictimCanNotifyLawEnforcement = false;
			break;
		case CCrime::REPORT_CRIME_DEFAULT:
			CrimesBeingQd[C].m_CrimeVictimId = CrimeVictimId;
			CrimesBeingQd[C].m_WitnessType = bArgAlreadyReported ? eWitnessedByLaw : eWitnessedNone;
			CrimesBeingQd[C].m_Victim = pVictim;
			CrimesBeingQd[C].bMustBeWitnessed = CCrimeInformationManager::GetInstance().MustBeWitnessed(ArgCrimeType);
			CrimesBeingQd[C].bMustNotifyLawEnforcement = CCrimeInformationManager::GetInstance().MustNotifyLawEnforcement(ArgCrimeType);
			CrimesBeingQd[C].bOnlyVictimCanNotifyLawEnforcement = CCrimeInformationManager::GetInstance().OnlyVictimCanNotifyLawEnforcement(ArgCrimeType);
			break;
		}
		
		if(ArgCrimeType == CRIME_SHOOT_COP && m_pPed->IsPlayer())
		{
			g_AudioScannerManager.IncrementCopShot();
		}

		if(!bArgAlreadyReported)
		{
			m_Witnesses.NotifyQueueMayContainUnperceivedCrimes();
		}
	}
	return false;
}

int GetCrimeReportDelay()
{
	int iResult = 0;
	
	switch (CCrime::sm_eReportCrimeMethod)
	{
	case CCrime::REPORT_CRIME_DEFAULT:
		iResult = CRIMES_GET_REPORTED_TIME;
		break;
	case CCrime::REPORT_CRIME_ARCADE_CNC:
		iResult = CRIMES_GET_REPORTED_TIME;
		break;
	}

	return iResult;
}

// Name			:	UpdateCrimesQ
// Purpose		:	Reports the crimes that are due
// Parameters
// Returns

void CWanted::UpdateCrimesQ()
{
	for (s32 C = 0; C < m_MaxCrimesBeingQd; C++)
	{
		CCrimeBeingQd &q = CrimesBeingQd[C];
		if (q.CrimeType == CRIME_NONE)
		{
			continue;
		}

		bool shouldRemove = false;
		bool shouldReport = false;

		if(CCrimeWitnesses::GetWitnessesNeeded() && q.bMustBeWitnessed)
		{
			m_Witnesses.UpdateIsWitnessed(q);

			if(/*q.IsInProgress() &&*/ q.m_WitnessType == eWitnessedByLaw)
			{
				shouldReport = true;
			}

			if(!q.IsInProgress() && q.HasLifetimeExpired() && (q.bAlreadyReported || !m_Witnesses.HasActiveReport()))
			{
				shouldRemove = true;
			}
		}
		else
		{
			if (NetworkInterface::GetSyncedTimeInMilliseconds() > q.TimeOfQing + GetCrimeReportDelay(q.CrimeType))
			{
				shouldReport = true;
			}
			if (!q.IsInProgress() && q.HasLifetimeExpired())
			{
				shouldRemove = true;
			}
		}

		if(shouldReport)
		{
			if (!q.bAlreadyReported)
			{
				ReportCrimeNow(q.CrimeType, q.Coors, q.bPoliceDontReallyCare, true, q.WantedScoreIncrement, q.m_CrimeVictimId);
				q.bAlreadyReported = true;
			}
		}

		if(shouldRemove)
		{
			q.CrimeType = CRIME_NONE;
			q.m_Victim = NULL;
			q.m_CrimeVictimId = NULL;
		}
	}
}

bool CWanted::HasPendingCrimesQueued() const
{
	for(int i = 0; i < m_MaxCrimesBeingQd; i++)
	{
		CCrimeBeingQd &crime = CrimesBeingQd[i];
		if(crime.CrimeType != CRIME_NONE && !crime.bAlreadyReported)
		{
			return true;
		}
	}

	return false;
}

bool CWanted::HasPendingCrimesNotWitnessedByLaw() const
{
	//Iterate over the crimes.
	for(int i = 0; i < m_MaxCrimesBeingQd; i++)
	{
		//Ensure the crime is valid.
		const CCrimeBeingQd& rCrime = CrimesBeingQd[i];
		if(!rCrime.IsValid())
		{
			continue;
		}

		//Ensure the crime is not witnessed by law.
		if(rCrime.GetWitnessType() == eWitnessedByLaw)
		{
			continue;
		}

		return true;
	}

	return false;
}

void CWanted::SetAllPendingCrimesWitnessedByLaw()
{
	for(int i = 0; i < m_MaxCrimesBeingQd; i++)
	{
		CCrimeBeingQd &crime = CrimesBeingQd[i];
		if(crime.CrimeType == CRIME_NONE)
		{
			continue;
		}

		crime.m_WitnessType = eWitnessedByLaw;
	}
}

void CWanted::SetAllPendingCrimesWitnessedByPed(CPed& rPed)
{
	//Iterate over the crimes.
	for(int i = 0; i < m_MaxCrimesBeingQd; i++)
	{
		//Ensure the crime is pending.
		CCrimeBeingQd& rCrime = CrimesBeingQd[i];
		if(!rCrime.IsValid())
		{
			continue;
		}

		//Ensure the witness type is valid.
		if(rCrime.m_WitnessType == eWitnessedByLaw)
		{
			continue;
		}

		//Set the witness type.
		rCrime.m_WitnessType = eWitnessedByPeds;

		//Add the witness.
		if(!rCrime.HasMaxWitnesses() && !rCrime.HasBeenWitnessedBy(rPed))
		{
			rCrime.AddWitness(rPed);
		}
	}
}

#define PRINTWANTEDSTUFF(X) DEV_ONLY(if(sm_bEnableCrimeDebugOutput){X})

// Name			:	EvaluateCrime
// Purpose		:	To determine the increment of a crime and if should be reported
// Parameters	:	Type of crime committed, if police don't "really" care, and the reference we'll store the score increment in

bool CWanted::EvaluateCrime(eCrimeType CrimeType, bool bPoliceDontReallyCare, u32 &Increment)
{
	Increment = GetCrimePenalty(CrimeType);

	switch (CCrime::sm_eReportCrimeMethod)
	{
	case CCrime::REPORT_CRIME_DEFAULT:
		// continue on to the below
		break;
	case CCrime::REPORT_CRIME_ARCADE_CNC:
		// increment has been updated, done
		return Increment > 0;
	}

	float Multiplier = rage::Max(0.0f, m_fMultiplier);

	if (bPoliceDontReallyCare)
	{
		Multiplier *= 0.333f;
	}

	switch(CrimeType)
	{
	case CRIME_POSSESSION_GUN:
		break;

	case CRIME_FIREARM_DISCHARGE:
		Increment = (s32)(10 * Multiplier);
		PRINTWANTEDSTUFF(Displayf("CRIME_FIREARM_DISCHARGE (10):%d (Mult:%f)\n", m_nWantedLevel+Increment, Multiplier);)
		break;

	case CRIME_HIT_PED:
		Increment = (s32)(5 * Multiplier);
		PRINTWANTEDSTUFF(Displayf("CRIME_HIT_PED (5):%d (Mult:%f)\n", m_nWantedLevel+Increment, Multiplier);)

			break;

	case CRIME_HIT_COP:
		Increment = (s32)(20 * Multiplier);
		PRINTWANTEDSTUFF(Displayf("CRIME_HIT_COP (20):%d (Mult:%f)\n", m_nWantedLevel+Increment, Multiplier);)
			break;

	case CRIME_STAB_PED:
		if (CLocalisation::PunishingWantedSystem())
		{
			Increment = (s32)(50 * Multiplier);
			PRINTWANTEDSTUFF(Displayf("CRIME_STAB_PED (50):%d (Mult:%f)\n", m_nWantedLevel+Increment, Multiplier);)
		}
		else
		{
			Increment = (s32)(35 * Multiplier);
			PRINTWANTEDSTUFF(Displayf("CRIME_STAB_PED (35):%d (Mult:%f)\n", m_nWantedLevel+Increment, Multiplier);)
		}
		break;

	case CRIME_STAB_COP:
		Increment = (s32)(100 * Multiplier);
		PRINTWANTEDSTUFF(Displayf("CRIME_HIT_COP (100):%d (Mult:%f)\n", m_nWantedLevel+Increment, Multiplier);)
			break;

	case CRIME_SHOOT_PED:
		if (CLocalisation::PunishingWantedSystem())
		{
			Increment = (s32)(60 * Multiplier);
			PRINTWANTEDSTUFF(Displayf("CRIME_SHOOT_PED (60):%d (Mult:%f)\n", m_nWantedLevel+Increment, Multiplier);)
		}
		else
		{
			Increment = (s32)(35 * Multiplier);
			PRINTWANTEDSTUFF(Displayf("CRIME_SHOOT_PED (35):%d (Mult:%f)\n", m_nWantedLevel+Increment, Multiplier);)
		}
		break;

	case CRIME_SHOOT_PED_SUPPRESSED:
		if (CLocalisation::PunishingWantedSystem())
		{
			Increment = (s32)(60 * Multiplier);
			PRINTWANTEDSTUFF(Displayf("CRIME_SHOOT_PED_SUPPRESSED (60):%d (Mult:%f)\n", m_nWantedLevel+Increment, Multiplier);)
		}
		else
		{
			Increment = (s32)(35 * Multiplier);
			PRINTWANTEDSTUFF(Displayf("CRIME_SHOOT_PED_SUPPRESSED (35):%d (Mult:%f)\n", m_nWantedLevel+Increment, Multiplier);)
		}
		break;

	case CRIME_SHOOT_NONLETHAL_PED:
		if (CLocalisation::PunishingWantedSystem())
		{
			Increment = (s32)(30 * Multiplier);
			PRINTWANTEDSTUFF(Displayf("CRIME_SHOOT_NONLETHAL_PED (30):%d (Mult:%f)\n", m_nWantedLevel+Increment, Multiplier);)
		}
		else
		{
			Increment = (s32)(15 * Multiplier);
			PRINTWANTEDSTUFF(Displayf("CRIME_SHOOT_NONLETHAL_PED (15):%d (Mult:%f)\n", m_nWantedLevel+Increment, Multiplier);)
		}
		break;

	case CRIME_STEALTH_KILL_PED:
		if (CLocalisation::PunishingWantedSystem())
		{
			Increment = (s32)(40 * Multiplier);
			PRINTWANTEDSTUFF(Displayf("CRIME_STEALTH_KILL_PED (40):%d (Mult:%f)\n", m_nWantedLevel+Increment, Multiplier);)
		}
		else
		{
			Increment = (s32)(20 * Multiplier);
			PRINTWANTEDSTUFF(Displayf("CRIME_STEALTH_KILL_PED (20):%d (Mult:%f)\n", m_nWantedLevel+Increment, Multiplier);)
		}
		break;

	case CRIME_KILL_PED:
		if (CLocalisation::PunishingWantedSystem())
		{
			Increment = (s32)(40 * Multiplier);
			PRINTWANTEDSTUFF(Displayf("CRIME_KILL_PED (40):%d (Mult:%f)\n", m_nWantedLevel+Increment, Multiplier);)
		}
		else
		{
			Increment = (s32)(20 * Multiplier);
			PRINTWANTEDSTUFF(Displayf("CRIME_KILL_PED (20):%d (Mult:%f)\n", m_nWantedLevel+Increment, Multiplier);)
		}
		break;

	case CRIME_SHOOT_COP:
		Increment = (s32)(80 * Multiplier);
		PRINTWANTEDSTUFF(Displayf("CRIME_SHOOT_COP (80):%d (Mult:%f)\n", m_nWantedLevel+Increment, Multiplier);)
			break;

	case CRIME_SHOOT_NONLETHAL_COP:
		Increment = (s32)(60 * Multiplier);
		PRINTWANTEDSTUFF(Displayf("CRIME_SHOOT_NONLETHAL_COP (60):%d (Mult:%f)\n", m_nWantedLevel+Increment, Multiplier);)
			break;

	case CRIME_KILL_COP:
		break;

	case CRIME_STEALTH_KILL_COP:
		
		if(m_WantedLevel < WANTED_LEVEL3)
		{
			Increment = (CDispatchData::GetWantedLevelThreshold(WANTED_LEVEL3) - m_nWantedLevel) + 1; // add 1 just in case the rounding is bad
		}
		else
		{
			Increment = (s32)(20 * Multiplier);
		}

		PRINTWANTEDSTUFF(Displayf("CRIME_STEALTH_KILL_COP (%d):%d (Mult:%f)\n", Increment, m_nWantedLevel+Increment, Multiplier);)
			break;

	case CRIME_SUICIDE:
		Increment = (s32)(0 * Multiplier);
		break;

	case CRIME_STEAL_CAR:
	case CRIME_STEAL_VEHICLE:
		Increment = (s32)(15 * Multiplier);
		PRINTWANTEDSTUFF(Displayf("CRIME_STEAL_CAR (15):%d (Mult:%f)\n", m_nWantedLevel+Increment, Multiplier);)
			break;

	case CRIME_JACK_DEAD_PED:
		Increment = (s32)(15 * Multiplier);
		PRINTWANTEDSTUFF(Displayf("CRIME_JACK_DEAD_PED (15):%d (Mult:%f)\n", m_nWantedLevel+Increment, Multiplier);)
			break;

	case CRIME_BLOCK_POLICE_CAR:
		Increment = (s32)(3 * Multiplier);
		PRINTWANTEDSTUFF(Displayf("CRIME_BLOCK_POLICE_CAR (3):%d (Mult:%f)\n", m_nWantedLevel+Increment, Multiplier);)
			break;

	case CRIME_STAND_ON_POLICE_CAR:
		Increment = (s32)(3 * Multiplier);
		PRINTWANTEDSTUFF(Displayf("CRIME_STAND_ON_POLICE_CAR (3):%d (Mult:%f)\n", m_nWantedLevel+Increment, Multiplier);)
			break;

	case CRIME_RUN_REDLIGHT:
		Increment = (s32)(5 * Multiplier);
		PRINTWANTEDSTUFF(Displayf("CRIME_RUN_REDLIGHT (5):%d (Mult:%f)\n", m_nWantedLevel+Increment, Multiplier);)
			break;

	case CRIME_DISTURBANCE :
	case CRIME_CIVILIAN_NEEDS_ASSISTANCE:
	case CRIME_RECKLESS_DRIVING:
		// don't want this to be to much as it is used every time you hit a copcar
		Increment = (s32)(5 * Multiplier);
		PRINTWANTEDSTUFF(Displayf("CRIME_RECKLESS_DRIVING (5):%d (Mult:%f)\n", m_nWantedLevel+Increment, Multiplier);)
			break;

	case CRIME_SPEEDING:
		Increment = (s32)(5 * Multiplier);
		PRINTWANTEDSTUFF(Displayf("CRIME_SPEEDING (5):%d (Mult:%f)\n", m_nWantedLevel+Increment, Multiplier);)
			break;

	case CRIME_RUNOVER_PED:
		Increment = (s32)(18 * Multiplier);
		PRINTWANTEDSTUFF(Displayf("CRIME_RUNOVER_PED (18):%d (Mult:%f)\n", m_nWantedLevel+Increment, Multiplier);)
			break;

	case CRIME_RUNOVER_COP:
		Increment = (s32)(80 * Multiplier);
		PRINTWANTEDSTUFF(Displayf("CRIME_RUNOVER_COP (80):%d (Mult:%f)\n", m_nWantedLevel+Increment, Multiplier);)
			break;

	case CRIME_DESTROY_HELI:
	case CRIME_DESTROY_PLANE:
		Increment = (s32)(400 * Multiplier);
		PRINTWANTEDSTUFF(Displayf("CRIME_SHOOT_HELI (400):%d (Mult:%f)\n", m_nWantedLevel+Increment, Multiplier);)
			break;

	case CRIME_DESTROY_VEHICLE:
		Increment = (s32)(70 * Multiplier);
		PRINTWANTEDSTUFF(Displayf("CRIME_DESTROY_VEHICLE (70):%d (Mult:%f)\n", m_nWantedLevel+Increment, Multiplier);)
			break;

	case CRIME_PED_SET_ON_FIRE:
		if (CLocalisation::PunishingWantedSystem())
		{
			Increment = (s32)(40 * Multiplier);
			PRINTWANTEDSTUFF(Displayf("CRIME_PED_SET_ON_FIRE (40):%d (Mult:%f)\n", m_nWantedLevel+Increment, Multiplier);)
		}
		else
		{
			Increment = (s32)(20 * Multiplier);
			PRINTWANTEDSTUFF(Displayf("CRIME_PED_SET_ON_FIRE (20):%d (Mult:%f)\n", m_nWantedLevel+Increment, Multiplier);)
		}
		break;

	case CRIME_COP_SET_ON_FIRE:
		Increment = (s32)(80 * Multiplier);
		PRINTWANTEDSTUFF(Displayf("CRIME_COP_SET_ON_FIRE (80):%d (Mult:%f)\n", m_nWantedLevel+Increment, Multiplier);)
			break;

	case CRIME_CAR_SET_ON_FIRE:
		Increment = (s32)(20 * Multiplier);
		PRINTWANTEDSTUFF(Displayf("CRIME_CAR_SET_ON_FIRE (20):%d (Mult:%f)\n", m_nWantedLevel+Increment, Multiplier);)
			break;

	case CRIME_CAUSE_EXPLOSION:
		Increment = (s32)(25 * Multiplier);
		PRINTWANTEDSTUFF(Displayf("CRIME_CAUSE_EXPLOSION (25):%d (Mult:%f)\n", m_nWantedLevel+Increment, Multiplier);)
			break;

	case CRIME_VEHICLE_EXPLOSION:
		Increment = (s32)(70 * Multiplier);
		PRINTWANTEDSTUFF(Displayf("CRIME_VEHICLE_EXPLOSION (70):%d (Mult:%f)\n", m_nWantedLevel+Increment, Multiplier);)
			break;

	case CRIME_CHAIN_EXPLOSION:
		Increment = (s32)(5 * Multiplier);
		PRINTWANTEDSTUFF(Displayf("CRIME_CHAIN_EXPLOSION (5):%d (Mult:%f)\n", m_nWantedLevel+Increment, Multiplier);)
			break;

	case CRIME_DAMAGE_TO_PROPERTY:
		Increment = (s32)(2 * Multiplier);
		PRINTWANTEDSTUFF(Displayf("CRIME_DAMAGE_TO_PROPERTY (2):%d (Mult:%f)\n", m_nWantedLevel+Increment, Multiplier);)
			break;

	case CRIME_TARGET_COP:
		Increment = (s32)(0 * Multiplier);
		PRINTWANTEDSTUFF(Displayf("CRIME_TARGET_COP (0):%d (Mult:%f)\n", m_nWantedLevel+Increment, Multiplier);)
			break;

	case CRIME_DRIVE_AGAINST_TRAFFIC:
		Increment = 0;
		PRINTWANTEDSTUFF(Displayf("CRIME_DRIVE_AGAINST_TRAFFIC (0):%d (Mult:%f)\n", m_nWantedLevel+Increment, Multiplier);)
			break;

	case CRIME_RIDING_BIKE_WITHOUT_HELMET:
		Increment = 0;
		PRINTWANTEDSTUFF(Displayf("CRIME_RIDING_BIKE_WITHOUT_HELMET (0):%d (Mult:%f)\n", m_nWantedLevel+Increment, Multiplier);)
			break;

	case CRIME_SHOOT_AT_COP:
		Increment = 0;
		PRINTWANTEDSTUFF(Displayf("CRIME_SHOOT_AT_COP (0):%d (Mult:%f)\n", m_nWantedLevel+Increment, Multiplier);)
			break;

	case CRIME_SHOOT_VEHICLE:
		Increment = 15;
		PRINTWANTEDSTUFF(Displayf("CRIME_SHOOT_VEHICLE (0):%d (Mult:%f)\n", m_nWantedLevel+Increment, Multiplier);)
			break;

	case CRIME_TERRORIST_ACTIVITY:
		Increment = (s32)(25 * Multiplier);
		PRINTWANTEDSTUFF(Displayf("CRIME_TERRORIST_ACTIVITY (0):%d (Mult:%f)\n", m_nWantedLevel+Increment, Multiplier);)
			break;

	case CRIME_HASSLE:
		Increment = (s32)(55 * Multiplier);
		PRINTWANTEDSTUFF(Displayf("CRIME_HASSLE (55):%d (Mult:%f)\n", m_nWantedLevel+Increment, Multiplier);)
			break;

	default:
#if !__FINAL		
		Errorf("Undefined crime type, EvaluateCrime, Crime.cpp");
#endif			
		break;
	}

	if (Increment == 0)
		return false;

	if (GetWithinAmnestyTime() && (Increment + m_nWantedLevel) < CDispatchData::GetWantedLevelThreshold(WANTED_LEVEL2))
	{
		return false;
	}

	return true;
}

s32	CWanted::GetValueForWantedLevel(s32 desiredWantedLevel)
{
	// The update to the m_WantedLevel variable happens in UpdateWantedLevel
	switch (desiredWantedLevel)
	{
		case WANTED_LEVEL1 :
			return CDispatchData::GetWantedLevelThreshold(WANTED_LEVEL1) + 20;

		case WANTED_LEVEL2 :
			return CDispatchData::GetWantedLevelThreshold(WANTED_LEVEL2) + 20;

		case WANTED_LEVEL3 :
			return CDispatchData::GetWantedLevelThreshold(WANTED_LEVEL3) + 20;

		case WANTED_LEVEL4 :
			return CDispatchData::GetWantedLevelThreshold(WANTED_LEVEL4) + 20;

		case WANTED_LEVEL5 :
			return CDispatchData::GetWantedLevelThreshold(WANTED_LEVEL5) + 20;

		case WANTED_CLEAN :
			return 0;		

		default :	
			Assert(0);
	}

	return 0;
}

// Resets any flags that need to be refreshed every frame.
void CWanted::ResetFlags()
{
	m_AllRandomsFlee = false;
	m_AllNeutralRandomsFlee = false;
	m_AllRandomsOnlyAttackWithGuns = false;
	m_LawPedCanAttackNonWantedPlayer = false;
}

// Some crimes are considered resisting arrest and we should let any nearby cops who are in the arrest task know of it
void CWanted::SetResistingArrest(eCrimeType crimeType)
{
	if(crimeType < CRIME_LAST_MINOR_CRIME)
	{
		return;
	}

	if( crimeType == CRIME_HASSLE || crimeType == CRIME_BLOCK_POLICE_CAR || crimeType == CRIME_STAND_ON_POLICE_CAR || crimeType == CRIME_SUICIDE ||
		crimeType == CRIME_DISTURBANCE || crimeType == CRIME_CIVILIAN_NEEDS_ASSISTANCE )
	{
		return;
	}

	CEntityScannerIterator pedList = m_pPed->GetPedIntelligence()->GetNearbyPeds();
	for(CEntity* pNearbyEntity = pedList.GetFirst(); pNearbyEntity != NULL; pNearbyEntity = pedList.GetNext())
	{
		CPed* pNearbyPed = static_cast<CPed *>(pNearbyEntity);
		if(pNearbyPed->IsLawEnforcementPed())
		{
			CTaskArrestPed* pArrestTask = static_cast<CTaskArrestPed*>(pNearbyPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ARREST_PED));
			if(pArrestTask)
			{
				pArrestTask->SetTargetResistingArrest();
				return;
			}
		}
	}
}

// Name			:	ReportCrimeNow
// Purpose		:	Reports one crime
// Parameters
// Returns

void CWanted::ReportCrimeNow(eCrimeType CrimeType, const Vector3 &Coors, bool bPoliceDontReallyCare, bool bIncludeNetworkPlayers, u32 Increment, const CEntity* pVictim, PhysicalPlayerIndex causedByPlayerPhysicalIndex)
{
	wantedDebugf3("CWanted::ReportCrimeNow CrimeType[%d] Coors[%f %f %f] bPoliceDontReallyCare[%d] bIncludeNetworkPlayers[%d] Increment[%d]",CrimeType,Coors.x,Coors.y,Coors.z,bPoliceDontReallyCare,bIncludeNetworkPlayers,Increment);

	if(sm_bEnableCrimeDebugOutput)
	{
		DEV_ONLY(Displayf("Wanted: ReportCrimeNow:%d\n", CrimeType);)
	}

	Assertf((Coors.x != 0.0f || Coors.y != 0.0f),"ReportCrimeNow at (0.0f, 0.0f)" );

	switch (CCrime::sm_eReportCrimeMethod)
	{
	case CCrime::REPORT_CRIME_DEFAULT:
		
		// for default reporting, evaluate the crime then break to logic below
		if(Increment == 0)
		{
			if(!EvaluateCrime(CrimeType, bPoliceDontReallyCare, Increment))
			{
				return;
			}
		}
		break;
	case CCrime::REPORT_CRIME_ARCADE_CNC:
		// for CnC, crime has already been evaluated so as long as increment is > 0, 
		if (Increment > 0)
		{
			u32 oldWantedLevel = GetWantedLevel();
			u32 newWantedLevel = oldWantedLevel + Increment;
			SetWantedLevel(VEC3V_TO_VECTOR3(m_pPed->GetTransform().GetPosition()), newWantedLevel, 0, WL_FROM_SCRIPT);
			// crime reported event
			GetEventGlobalGroup()->Add(CEventCrimeReported(CrimeType, m_WantedLevel > oldWantedLevel));
		}
		return;
	}

	if (causedByPlayerPhysicalIndex == INVALID_PLAYER_INDEX && m_pPed->GetNetworkObject())
	{
		causedByPlayerPhysicalIndex = m_pPed->GetNetworkObject()->GetPhysicalPlayerIndex();
	}	
	m_CausedByPlayerPhysicalIndex = causedByPlayerPhysicalIndex;

	if (bIncludeNetworkPlayers && NetworkInterface::IsGameInProgress())
	{
		bool bApplyWantedLevel = true;

		if (m_pPed->IsNetworkClone() && m_pPed->GetNetworkObject()->GetPlayerOwner())
		{
			// assume ped is a player - won't handle other peds at the moment
			Assert(m_pPed->IsPlayer());
			wantedDebugf3("CWanted::ReportCrimeNow--IsNetworkClone-->invoke CAlterWantedLevelEvent::Trigger 1");
            CAlterWantedLevelEvent::Trigger(*m_pPed->GetNetworkObject()->GetPlayerOwner(), CrimeType, bPoliceDontReallyCare, causedByPlayerPhysicalIndex);
			bApplyWantedLevel = false;
		}

		// if there are any other players in the vehicle, they get the wanted points for this crime too
		if (m_pPed->IsPlayer() && m_pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
		{
			CVehicle* pVehicle = m_pPed->GetMyVehicle();

			if (AssertVerify(pVehicle))
			{
				ReportCrimeNowForVehiclePassengers(pVehicle,Coors,CrimeType,bPoliceDontReallyCare,causedByPlayerPhysicalIndex);

				if (CTrailer* pAttachedTrailer = pVehicle->GetAttachedTrailer())
				{
					if (pAttachedTrailer->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_SET_WANTED_FOR_ATTACHED_VEH))
						ReportCrimeNowForVehiclePassengers(pAttachedTrailer,Coors,CrimeType,bPoliceDontReallyCare,causedByPlayerPhysicalIndex);
				}

				if (pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_SET_WANTED_FOR_ATTACHED_VEH))
				{
					if (pVehicle->InheritsFromTrailer())
					{
						CTrailer* pTrailer = static_cast<CTrailer*>(pVehicle);
						if (const CVehicle* pAttachedVehicle = pTrailer->GetAttachedToParent())
						{
							ReportCrimeNowForVehiclePassengers(pAttachedVehicle,Coors,CrimeType,bPoliceDontReallyCare,causedByPlayerPhysicalIndex);
						}
					}
				}
			}
		}

		if (!bApplyWantedLevel)
			return;
	}

	if(m_ReportedCrimes.IsFull())
	{
		m_ReportedCrimes.PopEnd();
	}

	sReportedCrime newCrime(CrimeType, fwTimer::GetTimeInMilliseconds(), Increment, pVictim);
	m_ReportedCrimes.PushTop(newCrime);

	eWantedLevel oldWantedLevel = m_WantedLevel;
	m_nWantedLevel += Increment;

	// If we're still on parole any crime will make us go back to the pre-parole level.
	if (CrimeType > CRIME_LAST_MINOR_CRIME)
	{
		if (m_WantedLevelBeforeParole > WANTED_CLEAN)
		{
			m_nWantedLevel = MAX(m_nWantedLevel, m_nWantedLevelBeforeParole);
			ClearParole();
		}
	}

	// Don't report the new vehicle if stealing a car, sounds wrong but trust me it's right!
	UpdateWantedLevel(Coors, (CrimeType != CRIME_STEAL_CAR && CrimeType != CRIME_STEAL_VEHICLE), ShouldRefocusSearchForCrime(CrimeType, oldWantedLevel));

	// if this crime has affected the wanted level then report it to the police scanner
	if(m_pPed->IsLocalPlayer())
	{
		NA_POLICESCANNER_ENABLED_ONLY(GetEventGlobalGroup()->Add(CEventCrimeReported(CrimeType, m_WantedLevel > oldWantedLevel)));

		if(m_WantedLevel  > oldWantedLevel)
		{
			AI_LOG_WITH_ARGS("CWanted::ReportCrimeNow for %s ped %s (address %p) - Old: %d New: %d SetWantedLevelFrom: %s\n", AILogging::GetDynamicEntityIsCloneStringSafe(m_pPed), AILogging::GetDynamicEntityNameSafe(m_pPed), m_pPed, oldWantedLevel, m_WantedLevel, sm_WantedLevelSetFromLocations[WL_FROM_REPORTED_CRIME]);
			wantedDebugf3("CWanted::ReportCrimeNow Cur: %d New:%d Cur-eSetWantedLevelFrom:%d New-eSetWantedLevelFrom:%d", oldWantedLevel, m_WantedLevel, m_WantedLevelLastSetFrom, WL_FROM_REPORTED_CRIME);
			m_WantedLevelLastSetFrom = WL_FROM_REPORTED_CRIME;
			NA_POLICESCANNER_ENABLED_ONLY(g_PoliceScanner.ReportPlayerCrime(Coors, CrimeType));
		}
	}
}

void CWanted::ReportCrimeNowForVehiclePassengers(const CVehicle* pVehicle, const Vector3 &Coors, eCrimeType CrimeType, bool bPoliceDontReallyCare, PhysicalPlayerIndex causedByPlayerPhysicalIndex)
{
	for (s32 i=0; i<pVehicle->GetSeatManager()->GetMaxSeats(); i++)
	{
		CPed* pPed = pVehicle->GetSeatManager()->GetPedInSeat(i);
		if (pPed && pPed->IsPlayer() && pPed != m_pPed)
		{
			if (pPed->IsNetworkClone() && pPed->GetNetworkObject()->GetPlayerOwner())
			{
				// give the wanted points to the other player
				wantedDebugf3("CWanted::ReportCrimeNow--IsNetworkClone-->invoke CAlterWantedLevelEvent::Trigger 2");
				CAlterWantedLevelEvent::Trigger(*pPed->GetNetworkObject()->GetPlayerOwner(), CrimeType, bPoliceDontReallyCare, causedByPlayerPhysicalIndex);
			}
			else
			{
				// give the wanted points to our player
				ReportCrimeNow(CrimeType, Coors, bPoliceDontReallyCare, false, causedByPlayerPhysicalIndex);
			}
		}
	}
}


// Name			:	ReportPoliceSpottingPlayer
// Purpose		:	When a police officer spots the player this function should be called.
//					It defines the circle from which the player needs to escape to have his wanted level erased.
// Parameters
// Returns

void CWanted::ReportPoliceSpottingPlayer(CPed* pSpotter, const Vector3 &Coors, s32 oldWantedLevel, const bool bCarKnown, const bool bPlayerSeen)
{
	wantedDebugf3("CWanted::ReportPoliceSpottingPlayer oldWantedLevel[%d] bCarKnown[%d] bPlayerSeen[%d]",oldWantedLevel,bCarKnown,bPlayerSeen);

	// If the player doesn't have a wanted level being spotted is of no consequence.
	if (m_WantedLevel == WANTED_CLEAN)
	{
		return;
	}

	// B*2343618: Prevent newly spawned police reporting if script have specfiied a blocking timer using ALLOW_EVASION_HUD_IF_DISABLING_HIDDEN_EVASION_THIS_FRAME.
	if (pSpotter && pSpotter->IsCopPed() && m_iTimeBeforeAllowReporting > 0 && ( (s32)(fwTimer::GetTimeInMilliseconds() - pSpotter->GetCreationTime()) < m_iTimeBeforeAllowReporting) )
	{
		return;
	}

	Assert( m_pPed );

	// Make sure the player is still within sense range of the ped. May not be the case as los results may sit there for a while.
	if (pSpotter)
	{
		float fSenseRange = pSpotter->GetPedIntelligence()->GetPedPerception().GetSeeingRange();
		if( pSpotter->GetVehiclePedInside() && m_pPed->GetVehiclePedInside() )
			fSenseRange = MAX(fSenseRange,80.0f);

		ScalarV scDiffSq = DistSquared(pSpotter->GetTransform().GetPosition(), RCC_VEC3V(Coors));
		ScalarV scMaxRangeSq = ScalarVFromF32(fSenseRange);
		scMaxRangeSq = (scMaxRangeSq * scMaxRangeSq);
		if ( IsGreaterThanAll(scDiffSq, scMaxRangeSq) )
		{
			return;
		}
	}

	// Only bother player with messages with wanted level >= 3
	if (m_WantedLevel >= WANTED_LEVEL1)
	{
		bool bForcePoliceScanner = false;
		if (oldWantedLevel >= WANTED_LEVEL1)
		{
			float Dist = (Coors - m_LastSpottedByPolice).XYMag();
			if (Dist > 60.0f)
			{
				bForcePoliceScanner = true;
			}
		}

		NA_POLICESCANNER_ENABLED_ONLY(g_PoliceScanner.ReportPoliceSpottingPlayer(Coors, oldWantedLevel, bCarKnown, bForcePoliceScanner));
	}

	// We've just been spotted by the police after being hidden for a while,
	if( GetTimeSinceSearchLastRefocused() > 2000 )
	{
		// Buddies let the player know
		if(m_WantedLevel > oldWantedLevel)
		{
			PassengersCommentOnPolice();
		}

		if(pSpotter)
		{
			pSpotter->NewSay("SUSPECT_SPOTTED");
		}

		REPLAY_ONLY(ReplayBufferMarkerMgr::AddMarker(3500,4000,IMPORTANCE_LOW);)
	}

	u32 uTimeInMilliseconds = NetworkInterface::GetSyncedTimeInMilliseconds();

	wantedDebugf3("CWanted::ReportPoliceSpottingPlayer --> invoke ResetHiddenEvasion");
	ResetHiddenEvasion(Coors, uTimeInMilliseconds, m_iTimeSearchLastRefocused == 0 || bPlayerSeen);
	ResetTimeInitialSearchAreaWillTimeOut(uTimeInMilliseconds, bPlayerSeen);

	m_LastSpottedByPolice = Coors;
	m_CurrentSearchAreaCentre = VEC3V_TO_VECTOR3(CDispatchHelperPedPositionToUse::Get(*m_pPed));
	m_iTimeSearchLastRefocused = uTimeInMilliseconds;

	if(bPlayerSeen)
	{
		if(m_iTimeFirstSpotted == 0)
		{
			m_iTimeFirstSpotted = uTimeInMilliseconds;
		}
		else if( m_WantedLevel == WANTED_LEVEL1 &&
				(uTimeInMilliseconds - m_iTimeFirstSpotted) > sm_Tunables.m_MaxTimeTargetVehicleMoving )
		{
			const CVehicle* pMyVehicle = m_pPed->GetVehiclePedInside();
			if(pMyVehicle && pMyVehicle->GetVelocity().Mag2() > square(CTaskCombat::ms_Tunables.m_MaxSpeedToStartJackingVehicle))
			{
				SetWantedLevelNoDrop(m_LastSpottedByPolice, WANTED_LEVEL2, WANTED_CHANGE_DELAY_INSTANT, WL_FROM_INTERNAL_EVADING, true);
			}
		}

		g_AudioScannerManager.SetPlayerSpottedTime(pSpotter);

		m_iTimeLastSpotted = uTimeInMilliseconds;
		m_fTimeUntilEvasionStarts = -1.0f;
	}

	sm_WantedBlips.CreateAndUpdateRadarBlips(this);

	// If the player is in a vehicle, he's been seen in this vehicle and its now hot!
	if( bCarKnown )
	{
		CVehicle* pMyVehicle = m_pPed->GetVehiclePedInside();
		if( pMyVehicle )
		{
			pMyVehicle->m_nVehicleFlags.bWanted = true;
		}
	}
}


void CWanted::ReportWitnessSuccessful(CEntity* pEntity)
{
	m_Witnesses.ReportWitnessSuccessful(pEntity);
}


void CWanted::ReportWitnessUnsuccessful(CEntity* pEntity)
{
	m_Witnesses.ReportWitnessUnsuccessful(pEntity);
}

//-------------------------------------------------------------------------
// Updates all present cop peds, if the player is wanted, sets acquaintances
// accordingly
//-------------------------------------------------------------------------
void CWanted::UpdateRelationshipStatus( CRelationshipGroup* pRelGroup )
{
	if(pRelGroup == NULL || CRelationshipManager::s_pPlayerGroup == NULL)
	{
		return;
	}

	if( GetWantedLevel() >= WANTED_LEVEL1 )
	{
		pRelGroup->SetRelationship(ACQUAINTANCE_TYPE_PED_WANTED, CRelationshipManager::s_pPlayerGroup);
	}
	else
	{
		if(pRelGroup->GetName().GetHash() != m_DisableRelationshipGroupHashResetThisFrame)
		{
			pRelGroup->ClearRelationship(CRelationshipManager::s_pPlayerGroup);
		}
	}
}

void CWanted::UpdateCopPeds( void )
{
	UpdateRelationshipStatus(CRelationshipManager::s_pCopGroup);
	UpdateRelationshipStatus(CRelationshipManager::s_pArmyGroup);
	UpdateRelationshipStatus(CRelationshipManager::s_pSecurityGuardGroup);

	sm_iNumCopsOnFoot = 0;

	//! Reset once consumed.
	m_DisableRelationshipGroupHashResetThisFrame = 0;
}


//-------------------------------------------------------------------------
// Returns the highest effective wanted level of the local player and any remote players in his car
//-------------------------------------------------------------------------
eWantedLevel CWanted::GetWantedLevelIncludingVehicleOccupants(Vector3 *lastSpottedByPolice)
{
    eWantedLevel highestWantedLevel = WANTED_CLEAN;

	CPed * localPlayer = FindPlayerPed();
    {
 
        if(localPlayer)
        {
            highestWantedLevel = localPlayer->GetPlayerWanted()->GetWantedLevel();

            if(lastSpottedByPolice)
            {
                *lastSpottedByPolice = localPlayer->GetPlayerWanted()->m_LastSpottedByPolice;
            }
        }
    }

    return highestWantedLevel;
}

void CWanted::PassengersCommentOnPolice()
{
	u32 currentAudioTime = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);

	if(currentAudioTime - m_uLastTimeWantedSetByScript < 2000)
		return;

	if(m_pPed && m_pPed->IsAPlayerPed() && m_pPed->GetMyVehicle() &&  m_pPed->GetMyVehicle()->GetSeatManager() && m_pPed->GetSpeechAudioEntity())
	{
		CVehicle* vehicle = m_pPed->GetMyVehicle();
		if(m_pPed->GetSpeechAudioEntity()->GetIsHavingANonPausedConversation())
		{
			g_ScriptAudioEntity.RegisterWantedLevelIncrease(vehicle);
			return;
		}

		bool notACar = vehicle->GetVehicleType() != VEHICLE_TYPE_CAR && 
			vehicle->GetVehicleType() != VEHICLE_TYPE_BIKE &&
			vehicle->GetVehicleType() != VEHICLE_TYPE_QUADBIKE &&
			vehicle->GetVehicleType() != VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE;
		if(notACar)
			return;

		const bool bConversedRecently = (m_pPed->GetSpeechAudioEntity()->GetTimeLastInScriptedConversation() + 3000) > currentAudioTime;
		if(!bConversedRecently)
		{
			s32 maxSeats = vehicle->GetSeatManager()->GetMaxSeats();
			if(maxSeats > 0)
			{
				int startSeat = audEngineUtil::GetRandomInteger() % maxSeats;
				for( s32 iSeat = 0; iSeat < maxSeats; iSeat++ )
				{
					int index = (startSeat + iSeat) % maxSeats;
					CPed* pPed = vehicle->GetSeatManager()->GetPedInSeat(index);
					if( pPed && pPed->GetSpeechAudioEntity())
					{
						pPed->GetSpeechAudioEntity()->SetToPlayGetWantedLevel();
					}
				}
			}
		}
		
	}
}

void CWanted::CopsCommentOnSwat()
{
	//Find the closest alive cop.
	CEntityScannerIterator pedList = m_pPed->GetPedIntelligence()->GetNearbyPeds();
	for(CEntity* pNearbyEntity = pedList.GetFirst(); pNearbyEntity != NULL; pNearbyEntity = pedList.GetNext())
	{
		CPed* pNearbyPed = static_cast<CPed *>(pNearbyEntity);
		if(pNearbyPed->IsRegularCop() && !pNearbyPed->IsInjured() && pNearbyPed->GetSpeechAudioEntity())
		{
			//Say the audio.
			pNearbyPed->GetSpeechAudioEntity()->SayWhenSafe("REQUEST_NOOSE");
			break;
		}
	}
}

float CWanted::CalculateChancesToForceWaitInFront() const
{
	//Grab the difficulty.
	float fDifficulty = GetDifficulty();

	return (sm_Tunables.m_Difficulty.m_Spawning.m_ChancesToForceWaitInFront.CalculateValue(fDifficulty));
}

void CWanted::CalculateDespawnLimits(DespawnLimits& rLimits) const
{
	//Grab the difficulty.
	float fDifficulty = GetDifficulty();

	//Calculate the max facing threshold.
	rLimits.m_fMaxFacingThreshold = sm_Tunables.m_Difficulty.m_Despawning.m_MaxFacingThreshold.CalculateValue(fDifficulty);
	
	//Calculate the max moving threshold.
	rLimits.m_fMaxMovingThreshold = sm_Tunables.m_Difficulty.m_Despawning.m_MaxMovingThreshold.CalculateValue(fDifficulty);
	
	//Calculate the min distance to be considered lagging behind.
	rLimits.m_fMinDistanceToBeConsideredLaggingBehind = sm_Tunables.m_Difficulty.m_Despawning.m_MinDistanceToBeConsideredLaggingBehind.CalculateValue(fDifficulty);
	
	//Calculate the min distance to check clumped.
	rLimits.m_fMinDistanceToCheckClumped = sm_Tunables.m_Difficulty.m_Despawning.m_MinDistanceToCheckClumped.CalculateValue(fDifficulty);
	
	//Calculate the max distance to be considered clumped.
	rLimits.m_fMaxDistanceToBeConsideredClumped = sm_Tunables.m_Difficulty.m_Despawning.m_MaxDistanceToBeConsideredClumped.CalculateValue(fDifficulty);
}

bool CWanted::CalculateHeliRefuel(float& fTimeBefore, float& fDelay) const
{
	//Ensure the tunables are valid.
	const Tunables::WantedLevel* pTunables = GetTunablesForWantedLevel(m_WantedLevel);
	if(!pTunables)
	{
		return false;
	}

	//Ensure refuel is enabled.
	if(!pTunables->m_Difficulty.m_Helis.m_Refuel.m_Enabled)
	{
		return false;
	}

	//Set the values.
	fTimeBefore = pTunables->m_Difficulty.m_Helis.m_Refuel.m_TimeBefore;
	fDelay		= pTunables->m_Difficulty.m_Helis.m_Refuel.m_Delay;

	return true;
}

bool CWanted::AreHelisRefueling() const
{
	//Ensure heli refuel is enabled.
	float fTimeBefore;
	float fDelay;
	if(!CalculateHeliRefuel(fTimeBefore, fDelay))
	{
		return false;
	}

	//Ensure the helis need to refuel.
	if(m_uTimeHeliRefuelComplete == 0)
	{
		return false;
	}

	return (NetworkInterface::GetSyncedTimeInMilliseconds() < m_uTimeHeliRefuelComplete);
}

float CWanted::CalculateDifficulty() const
{
	//Ensure the ped is valid.
	if(!m_pPed)
	{
		return 0.0f;
	}
	
	//Ensure the tunables are valid.
	const Tunables::WantedLevel* pTunables = GetTunablesForWantedLevel(m_WantedLevel);
	if(!pTunables)
	{
		return 0.0f;
	}
	
	//Get the difficulty from the wanted level.
	float fDifficultyFromWantedLevel = pTunables->m_Difficulty.m_Calculation.m_FromWantedLevel;
	
	//Calculate the difficulty based on the last spotted distance.
	float fDifficultyFromLastSpottedDistance = 0.5f;
	{
		//Check if we care about the radius.
		if(sm_bEnableRadiusEvasion)
		{
			//Grab the distance from the spot the ped was last spotted.
			float fLastSpottedDistance = (VEC3V_TO_VECTOR3(m_pPed->GetTransform().GetPosition()) - m_LastSpottedByPolice).XYMag();

			//Grab the radius for the wanted level.
			float fRadius = CDispatchData::GetWantedZoneRadius(m_WantedLevel, m_pPed);
			if(fRadius > 0.0f)
			{
				fDifficultyFromLastSpottedDistance = 1.0f - (fLastSpottedDistance / fRadius);
			}
		}
	}
	
	//Calculate the difficulty from randomness.
	float fDifficultyFromRandomness = fwRandom::GetRandomNumberInRange(0.0f, 1.0f);
	
	//Calculate the difficulty.
	float fDifficulty = 0.0f;
	fDifficulty += (pTunables->m_Difficulty.m_Calculation.m_Weights.m_WantedLevel			* fDifficultyFromWantedLevel);
	fDifficulty += (pTunables->m_Difficulty.m_Calculation.m_Weights.m_LastSpottedDistance	* fDifficultyFromLastSpottedDistance);
	fDifficulty += (pTunables->m_Difficulty.m_Calculation.m_Weights.m_Randomness			* fDifficultyFromRandomness);

	//Check if decay is not disabled.
	bool bIsDecayDisabled = (pTunables->m_Difficulty.m_Calculation.m_Decay.m_DisableWhenOffMission && !CTheScripts::GetPlayerIsOnAMission());
	if(!bIsDecayDisabled)
	{
		//Get the time evading for max difficulty decay.
		float fTimeEvadingForMaxDifficultyDecay = pTunables->m_Difficulty.m_Calculation.m_Decay.m_TimeEvadingForMaxValue;

		//Get the max difficulty decay.
		float fMaxDifficultyDecay = pTunables->m_Difficulty.m_Calculation.m_Decay.m_MaxValue;

		//Calculate the difficulty decay.
		float fTimeEvading = Clamp(m_fTimeEvading, 0.0f, fTimeEvadingForMaxDifficultyDecay);
		float fLerp = fTimeEvading / fTimeEvadingForMaxDifficultyDecay;
		float fDifficultyDecay = Lerp(fLerp, 0.0f, fMaxDifficultyDecay);

		//Decay the difficulty.
		fDifficulty *= (1.0f - fDifficultyDecay);
	}
	
	//Clamp the difficulty.
	fDifficulty = Clamp(fDifficulty, 0.0f, 1.0f);
	
	return fDifficulty;
}

float CWanted::CalculateIdealSpawnDistance() const
{
	//Grab the difficulty.
	float fDifficulty = GetDifficulty();

	//Calculate the ideal spawn distance.
	float fIdealSpawnDistance = sm_Tunables.m_Difficulty.m_Spawning.m_IdealDistance.CalculateValue(fDifficulty);

	return fIdealSpawnDistance;
}

void CWanted::CalculatePedAttributes(const CPed& rPed, PedAttributes& rAttributes) const
{
	//Keep track of the attributes.
	const Tunables::Difficulty::Peds::Attributes* pAttributes = NULL;
	
	//Check the ped type.
	switch(rPed.GetPedType())
	{
		case PEDTYPE_COP:
		{
			pAttributes = &sm_Tunables.m_Difficulty.m_Peds.m_Cops;
			break;
		}
		case PEDTYPE_SWAT:
		{
			pAttributes = &sm_Tunables.m_Difficulty.m_Peds.m_Swat;
			break;
		}
		case PEDTYPE_ARMY:
		{
			pAttributes = &sm_Tunables.m_Difficulty.m_Peds.m_Army;
			break;
		}
		default:
		{
			return;
		}
	}
	
	//Keep track of the situation.
	const Tunables::Difficulty::Peds::Attributes::Situations::Situation* pSituation = NULL;
	
	//Calculate the situation.
	const CVehicle* pVehicle = rPed.GetVehiclePedInside();
	if(!pVehicle)
	{
		//Use the default situation.
		pSituation = &pAttributes->m_Situations.m_Default;
	}
	else
	{
		//Check if the vehicle is a heli.
		if(pVehicle->InheritsFromHeli())
		{
			pSituation = &pAttributes->m_Situations.m_InHeli;
		}
		//Check if the vehicle is a boat.
		else if(pVehicle->InheritsFromBoat())
		{
			//Use the boat situation.
			pSituation = &pAttributes->m_Situations.m_InBoat;
		}
		else
		{
			//Use the vehicle situation.
			pSituation = &pAttributes->m_Situations.m_InVehicle;
		}
	}
	
	//Grab the difficulty.
	float fDifficulty = GetDifficulty();

	//Calculate the shoot rate modifier.
	rAttributes.m_fShootRateModifier = pSituation->m_ShootRateModifier.CalculateValue(fDifficulty);
	
	//Calculate the weapon accuracy.
	rAttributes.m_fWeaponAccuracy = pSituation->m_WeaponAccuracy.CalculateValue(fDifficulty);

	//Apply the accuracy modifier.
	if(pSituation->m_WeaponAccuracyModifierForEvasiveMovement != 1.0f)
	{
		//Check if the target is being evasive.
		bool bIsBeingEvasive = (m_pPed->GetMotionData()->GetIsRunning() || m_pPed->GetPedResetFlag(CPED_RESET_FLAG_IsClimbing) ||
			(m_pPed->GetIsInVehicle() && (m_pPed->GetVehiclePedInside()->GetVelocity().XYMag2() > square(5.0f))));
		if(bIsBeingEvasive)
		{
			rAttributes.m_fWeaponAccuracy *= pSituation->m_WeaponAccuracyModifierForEvasiveMovement;
		}
	}

	//Apply the accuracy modifier.
	if(pSituation->m_WeaponAccuracyModifierForOffScreen != 1.0f)
	{
		//Check if the ped is off-screen.
		if(!rPed.GetPedConfigFlag(CPED_CONFIG_FLAG_VisibleOnScreen))
		{
			rAttributes.m_fWeaponAccuracy *= pSituation->m_WeaponAccuracyModifierForOffScreen;
		}
	}

	//Apply the accuracy modifier.
	if(pSituation->m_WeaponAccuracyModifierForAimedAt != 1.0f)
	{
		//Check if the ped is being targeted.
		if((m_pPed->GetPlayerInfo()->GetTargeting().GetTarget() == &rPed) ||
			(pVehicle && (CTimeHelpers::GetTimeSince(pVehicle->GetIntelligence()->GetLastTimePlayerAimedAtOrAround()) < 5000)))
		{
			rAttributes.m_fWeaponAccuracy *= pSituation->m_WeaponAccuracyModifierForAimedAt;
		}
	}

	//Apply the accuracy modifier for when the target is in a slow moving vehicle.
	if(ShouldCompensateForSlowMovingVehicle(&rPed, rPed.GetPedIntelligence()->GetQueriableInterface()->GetHostileTarget()))
	{
		static dev_float s_fAccuracyModifierForSlowMovingVehicle = 1.25f;
		rAttributes.m_fWeaponAccuracy *= s_fAccuracyModifierForSlowMovingVehicle;
	}
	
	//Calculate the heli speed modifier.
	bool bUseFullHeliSpeed = (m_pPed->GetIsInVehicle() && m_pPed->GetVehiclePedInside()->GetIsAircraft() && !CTheScripts::GetPlayerIsOnAMission());
	rAttributes.m_fHeliSpeedModifier = !bUseFullHeliSpeed ? pAttributes->m_HeliSpeedModifier.CalculateValue(fDifficulty) : 1.0f;
	
	//Calculate the sense range.
	rAttributes.m_fSensesRange = pSituation->m_SensesRange.CalculateValue(fDifficulty);
	
	//Calculate the identification range.
	rAttributes.m_fIdentificationRange = pSituation->m_IdentificationRange.CalculateValue(fDifficulty);
	
	//Check if we are wanted.
	if(m_WantedLevel > WANTED_CLEAN)
	{
		//Calculate the automobile speed modifier.
		rAttributes.m_fAutomobileSpeedModifier = pAttributes->m_AutomobileSpeedModifier.CalculateValue(fDifficulty);

		//Calculate whether we can do drivebys.
		float fMinForDrivebys = pSituation->m_MinForDrivebys;
		if(fMinForDrivebys <= 0.0f)
		{
			rAttributes.m_bCanDoDrivebys = true;
		}
		else if(fMinForDrivebys >= 1.0f)
		{
			rAttributes.m_bCanDoDrivebys = false;
		}
		else
		{
			rAttributes.m_bCanDoDrivebys = (fDifficulty >= fMinForDrivebys);
		}
	}
	else
	{
		//Reset the automobile speed modifier.
		rAttributes.m_fAutomobileSpeedModifier = 1.0f;

		//Keep the existing value for drive-bys.
		rAttributes.m_bCanDoDrivebys = rPed.GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_CanDoDrivebys);
	}
}

void CWanted::CalculateSpawnScoreWeights(SpawnScoreWeights& rWeights) const
{
	//Grab the difficulty.
	float fDifficulty = GetDifficulty();
	
	//Calculate the distance.
	rWeights.m_fDistance = sm_Tunables.m_Difficulty.m_Spawning.m_Scoring.m_Weights.m_Distance.CalculateValue(fDifficulty);

	//Calculate the direction.
	rWeights.m_fDirection = sm_Tunables.m_Difficulty.m_Spawning.m_Scoring.m_Weights.m_Direction.CalculateValue(fDifficulty);

	//Calculate the randomness.
	rWeights.m_fRandomness = sm_Tunables.m_Difficulty.m_Spawning.m_Scoring.m_Weights.m_Randomness.CalculateValue(fDifficulty);
}

float CWanted::CalculateTimeBetweenSpawnAttemptsModifier() const
{
	//Grab the difficulty.
	float fDifficulty = GetDifficulty();
	
	return sm_Tunables.m_Difficulty.m_Dispatch.m_TimeBetweenSpawnAttemptsModifier.CalculateValue(fDifficulty);
}

float CWanted::GetDifficulty() const
{
	//Check if there is an override.
	if(m_Overrides.HasDifficulty())
	{
		//Return the override.
		return m_Overrides.GetDifficulty();
	}
	
	return m_fDifficulty;
}

bool CWanted::HasBeenSpotted() const
{
	return (m_iTimeLastSpotted != 0);
}

void CWanted::OnHeliRefuel(float fDelay)
{
	//Set the time.
	m_uTimeHeliRefuelComplete = NetworkInterface::GetSyncedTimeInMilliseconds() + (u32)(fDelay * 1000.0f);
	u32 audioTime = g_AudioEngine.GetTimeInMilliseconds();
	if(audioTime - m_LastTimeOfScannerRefuelReport > g_MinTimeBetweenScannerRefuelReports && g_AudioScannerManager.CanPlayRefuellingReport())
	{
		g_AudioScannerManager.TriggerScriptedReport("PS_AIR_SUPPORT_REFUELLING");
		m_LastTimeOfScannerRefuelReport = audioTime;
	}
	g_AudioScannerManager.DecrementHelisInChase();
}

bool CWanted::ShouldUpdateDifficulty() const
{
	//Ensure the timer has expired.
	if(m_fTimeSinceLastDifficultyUpdate < sm_Tunables.m_Timers.m_TimeBetweenDifficultyUpdates)
	{
		return false;
	}
	
	return true;
}

void CWanted::UpdateDifficulty()
{
	//Calculate the difficulty.
	m_fDifficulty = CalculateDifficulty();
	
	//Clear the timer.
	m_fTimeSinceLastDifficultyUpdate = 0.0f;
}

void CWanted::UpdateEvasion()
{
	//Ensure the wanted level exceeds clean.
	if(m_WantedLevel > WANTED_CLEAN)
	{
		//Ensure the ped is valid.
		if(!m_pPed)
		{
			return;
		}

		// Update our hidden evasion start timer and initialize the hidden evasion if needed
		if(m_fTimeUntilEvasionStarts > 0.0f)
		{
			if(m_wantedLevelIncident && m_wantedLevelIncident->HasArrivedResources())
			{
				m_fTimeUntilEvasionStarts = -1.0f;
			}
			else{
				// If our start timer runs out and the time hidden evasion started is still 0 then we can set it
				m_fTimeUntilEvasionStarts -= fwTimer::GetTimeStep();
			}

			if(m_fTimeUntilEvasionStarts <= 0.0f && m_HasLeftInitialSearchArea)
			{
				SetHiddenEvasionTime(NetworkInterface::GetSyncedTimeInMilliseconds());
			}
		}

		// If we haven't been seen in a while then we should get rid of our wanted level
		if( sm_bEnableHiddenEvasion && !m_SuppressLosingWantedLevelIfHiddenThisFrame )
		{
			if(m_HasLeftInitialSearchArea && GetTimeSinceHiddenEvasionStarted() > CDispatchData::GetHiddenEvasionTime(m_WantedLevel))
			{
				SetWantedLevel(VEC3V_TO_VECTOR3(m_pPed->GetTransform().GetPosition()), WANTED_CLEAN, 0, WL_FROM_INTERNAL);
			}
		}

		//Grab the current time.
		u32 uCurrentTime = NetworkInterface::GetSyncedTimeInMilliseconds();

		//Keep track of whether we are considered evading this frame.
		bool bEvadingThisFrame = false;

		//We are considered evading this frame if we have been spotted once, but not for a while.
		if(!bEvadingThisFrame)
		{
			//Check if we have been spotted once.
			if(m_iTimeSearchLastRefocused > 0)
			{
				//Check if we have not been spotted for a while.
				TUNE_GROUP_FLOAT(WANTED, fTimeSinceLastSpottedToBeConsideredEvading, 5.0f, 0.0f, 60.0f, 1.0f);
				bEvadingThisFrame = (m_iTimeSearchLastRefocused + (u32)(fTimeSinceLastSpottedToBeConsideredEvading * 1000.0f) < uCurrentTime);
			}
		}

		//We are considered evading this frame if we are in a vehicle and moving quickly, or running.
		if(!bEvadingThisFrame)
		{
			//Check if we are in a vehicle.
			const CVehicle* pVehicle = m_pPed->GetVehiclePedInside();
			if(pVehicle)
			{
				//Check if we are moving quickly.
				TUNE_GROUP_FLOAT(WANTED, fMinVehicleSpeedToBeConsideredEvading, 5.0f, 0.0f, 50.0f, 1.0f);
				float fSpeedSq = pVehicle->GetVelocity().Mag2();
				float fMinSpeedSq = square(fMinVehicleSpeedToBeConsideredEvading);
				bEvadingThisFrame = (fSpeedSq >= fMinSpeedSq);
			}
			else
			{
				//Check if we are running.
				bEvadingThisFrame = (m_pPed->GetMotionData()->GetCurrentMbrY() > MOVEBLENDRATIO_WALK);
			}
		}

		//Update the last evasion time if we are evading this frame.
		if(bEvadingThisFrame)
		{
			//Update the last evasion time.
			m_uLastEvasionTime = uCurrentTime;
		}

		//Keep track of whether we are evading.
		bool bEvading = bEvadingThisFrame;

		//We are considered evading if we have made an evasion attempt in the last X seconds.
		if(!bEvading)
		{
			TUNE_GROUP_FLOAT(WANTED, fTimeSinceLastEvasionToBeConsideredEvading, 5.0f, 0.0f, 60.0f, 1.0f);
			bEvading = (m_uLastEvasionTime + (u32)(fTimeSinceLastEvasionToBeConsideredEvading * 1000.0f) >= uCurrentTime);
		}

		//Check if we are evading.
		if(bEvading)
		{
			//Update the time we have been evading.
			m_fTimeEvading += fwTimer::GetTimeStep();
		}
		else
		{
			//Clear the time we have been evading.
			m_fTimeEvading = 0.0f;
		}
	}
	else
	{
		//Clear the time we have been evading.
		m_fTimeEvading = 0.0f;

		// Clear the hidden evasion timer so it's not out of sync with when we have fresh WL
		m_fTimeUntilEvasionStarts = -1.0f;
	}

	m_SuppressLosingWantedLevelIfHiddenThisFrame = false;
	m_AllowEvasionHUDIfDisablingHiddenEvasionThisFrame = false;
}

void CWanted::UpdateTacticalAnalysis()
{
	//Check if we are not wanted.
	if(m_WantedLevel == WANTED_CLEAN)
	{
		//Check if we have a tactical analysis.
		if(m_pTacticalAnalysis)
		{
			//Release the tactical analysis.
			m_pTacticalAnalysis->Release();

			//Clear the tactical analysis.
			m_pTacticalAnalysis = NULL;
		}
	}
	else
	{
		//Check if we don't have a tactical analysis.
		if(!m_pTacticalAnalysis)
		{
			//Request a tactical analysis.
			m_pTacticalAnalysis = CTacticalAnalysis::Request(*m_pPed);
		}
	}
}

void CWanted::UpdateTimers()
{
	//Grab the time step.
	float fTimeStep = fwTimer::GetTimeStep();
	
	//Update the timers.
	m_fTimeSinceLastDifficultyUpdate	+= fTimeStep;
	m_fTimeSinceLastHostileAction		+= fTimeStep;
}

void CWanted::InitHiddenEvasionForPed(const CPed* pPed)
{
	//Ensure the ped is valid.
	if(!pPed)
	{
		return;
	}

	//Ensure the ped is a player.
	if(!pPed->IsPlayer())
	{
		return;
	}

	//Ensure the wanted is valid.
	CWanted* pWanted = pPed->GetPlayerWanted();
	if(!pWanted)
	{
		return;
	}

	// If the time last spotted is still 0 then we can set it
	if(pWanted->m_iTimeHiddenEvasionStarted == 0)
	{
		pWanted->SetHiddenEvasionTime(NetworkInterface::GetSyncedTimeInMilliseconds());
	}
}

void CWanted::RegisterHostileAction(const CPed* pPed)
{
	//Ensure the ped is valid.
	if(!pPed)
	{
		return;
	}

	//Ensure the ped is a player.
	if(!pPed->IsPlayer())
	{
		return;
	}

	//Ensure the wanted is valid.
	CWanted* pWanted = pPed->GetPlayerWanted();
	if(!pWanted)
	{
		return;
	}

	//Clear the time since the last hostile action.
	pWanted->m_fTimeSinceLastHostileAction = 0.0f;
}

const CWanted::Tunables::WantedLevel* CWanted::GetTunablesForWantedLevel(eWantedLevel nWantedLevel)
{
	//Check the wanted level.
	switch(nWantedLevel)
	{
		case WANTED_CLEAN:
		{
			return &sm_Tunables.m_WantedClean;
		}
		case WANTED_LEVEL1:
		{
			return &sm_Tunables.m_WantedLevel1;
		}
		case WANTED_LEVEL2:
		{
			return &sm_Tunables.m_WantedLevel2;
		}
		case WANTED_LEVEL3:
		{
			return &sm_Tunables.m_WantedLevel3;
		}
		case WANTED_LEVEL4:
		{
			return &sm_Tunables.m_WantedLevel4;
		}
		case WANTED_LEVEL5:
		{
			return &sm_Tunables.m_WantedLevel5;
		}
		default:
		{
			aiAssertf(false, "Unknown wanted level: %d.", nWantedLevel);
			return NULL;
		}
	}
}

// --- CWantedBlips ------------------------------------------------------------------------------------------------------------

CWantedBlips::CWantedBlips()
{
	m_aCircleRadarBlip = INVALID_BLIP_ID;
	m_PoliceRadarBlips = true;
	ResetBlipSpriteToUseForCopPeds();
	m_beingAttacked = false;
	m_beingAttackedTimer = 0;
}

void CWantedBlips::Init()
{
	if (m_aCircleRadarBlip != INVALID_BLIP_ID)
	{
		if (CMiniMap::IsBlipIdInUse(m_aCircleRadarBlip))
		{
			CMiniMapBlip *pBlip = CMiniMap::GetBlip(m_aCircleRadarBlip);
			if (pBlip)
			{
				CMiniMap::RemoveBlip(pBlip);
			}
		}
		m_aCircleRadarBlip = INVALID_BLIP_ID;
	}
	m_beingAttacked = false;
	m_beingAttackedTimer = 0;
}

int CWantedBlips::GetBlipForPed(CPed* pPed)
{
	CEntityPoolIndexForBlip index(pPed, BLIP_TYPE_COP);	//	Graeme - does it matter if I use BLIP_TYPE_COP or BLIP_TYPE_CHAR? They should both look up the CPed pool

	// check to see if we have already blipped this ped:
	for (s32 iCount = 0; iCount < m_CopBlips.GetCount(); iCount++)
	{
		if (m_CopBlips[iCount].iPedPoolIndex == index)
		{
			// ... but lest update this blip with latest status: (if he was in a vehicle but leaves he needs to change his blip)
			CMiniMapBlip *pBlip = CMiniMap::GetBlip(m_CopBlips[iCount].iBlipId);
			if (pBlip && CMiniMap::FindBlipEntity(pBlip) == pPed)
			{
				m_CopBlips[iCount].m_lastFrameValid = fwTimer::GetFrameCount();
				return m_CopBlips[iCount].iBlipId;
			}
		}
	}
	return INVALID_BLIP_ID;
}

//PURPOSE: Return if ped is attacking the player
bool CWantedBlips::IsPedAttackingPlayer(CPed* pPed, CPed* pWantedPed)
{
	bool bFoundPedInCombatWithPlayer = false;

	// Add blips for non-mission peds attacking the player
	bool bInCombat = false;
	CEntity* pTargetEntity = NULL;

	if(pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_COMBAT))
	{
		bInCombat = true;
		pTargetEntity = pPed->GetPedIntelligence()->GetQueriableInterface()->GetTargetForTaskType(CTaskTypes::TASK_COMBAT);
	}
	else if(pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_SHARK_ATTACK, true))
	{
		bInCombat = true;
		pTargetEntity = pPed->GetPedIntelligence()->GetQueriableInterface()->GetTargetForTaskType(CTaskTypes::TASK_SHARK_ATTACK);
	}
	else
	{
		CClonedWritheInfo* pWritheInfo = static_cast<CClonedWritheInfo*>(pPed->GetPedIntelligence()->GetQueriableInterface()->FindTaskInfoIfCurrentlyRunning(CTaskTypes::TASK_WRITHE, true));
		if(pWritheInfo && !pWritheInfo->GetDieInLoop())
		{
			s32 iWritheState = pWritheInfo->GetState();
			if( iWritheState == CTaskWrithe::State_Loop || iWritheState == CTaskWrithe::State_FromShootFromGround ||
				iWritheState == CTaskWrithe::State_ShootFromGround || iWritheState == CTaskWrithe::State_ToShootFromGround )
			{
				bInCombat = true;
				pTargetEntity = pWritheInfo->GetTarget();
			}
		}
	}

	if(bInCombat)
	{
		if(pTargetEntity && pTargetEntity->GetIsTypePed() && static_cast<CPed*>(pTargetEntity)->IsLocalPlayer())
		{
			// If within 200m of wanted ped flag as in combat:
			Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
			Vector3 vWantedPedPos = VEC3V_TO_VECTOR3(pWantedPed->GetTransform().GetPosition());
			float fDist = (vPedPos - vWantedPedPos).XYMag2();
			if(fDist < 200.0f*200.0f)
				bFoundPedInCombatWithPlayer   = true;
		}
		else
		{
			//Check if we have registered the target as a threat.
			const CPedTargetting* pTargeting = pPed->GetPedIntelligence()->GetTargetting(false);
			if(pTargeting && pTargeting->FindTarget(pWantedPed))
			{
				bFoundPedInCombatWithPlayer   = true;
			}
		}
	}
	return bFoundPedInCombatWithPlayer;
}

//-------------------------------------------------------------------------
// Updates all blips on cop peds - adds or removes them when necessary
// I have tried to avoid having a flag in the CPed class by storing a list
// of "cop blips" in CWanted.
//-------------------------------------------------------------------------
void CWantedBlips::UpdateCopBlips(CWanted *pWanted)
{
	const int PED_POOL_FRAME_OFFSET = 10;
	CPed* pWantedPed = pWanted->GetPed();

	//
	// add/remove any cop blips if we have a wanted level:
	//
	//if (wantedLevel >= WANTED_LEVEL1 || CScriptHud::iFakeWantedLevel > 0)
	{
		eWantedLevel realWantedLevel = pWanted->GetWantedLevel();
		bool bCopsSearching = CNewHud::GetHudWantedCopsAreSearching(pWanted);
		int wantedLevel = CNewHud::GetHudWantedLevel(pWanted, bCopsSearching);
		bool bWanted = (wantedLevel >= WANTED_LEVEL1);
		float fWantedLevelRadius = 0.0f;
		Vector3 vWantedLevelCentre;

		// if we havent got a real wanted level but the scripters have set a fake one then use the fake details:
		if (realWantedLevel >= WANTED_LEVEL1)
		{
			fWantedLevelRadius = CDispatchData::GetWantedZoneRadius(realWantedLevel, pWantedPed);
			vWantedLevelCentre = pWanted->m_CurrentSearchAreaCentre;
		}
		else  // otherwise must be the fake wanted level
		{
			fWantedLevelRadius = CDispatchData::GetWantedZoneRadius(CScriptHud::iFakeWantedLevel, pWantedPed);
			vWantedLevelCentre = VEC3V_TO_VECTOR3(pWantedPed->GetTransform().GetPosition());//CScriptHud::vFakeWantedLevelPos;
		}

		//bool bIgnoreRadiusForBlip = !CWanted::sm_bEnableRadiusEvasion;

		CPed::Pool *pPedPool = CPed::GetPool();

		if (m_PoliceRadarBlips && pWantedPed)
		{
			s32 i = (pPedPool->GetSize()- fwTimer::GetFrameCount() % PED_POOL_FRAME_OFFSET) - 1;

			while(i >= 0)
			{
				CPed *pPed = pPedPool->GetSlot(i);

				i -= PED_POOL_FRAME_OFFSET;

				if(pPed == NULL || pPed->GetIsDeadOrDying() || pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DontBlip) || pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DontBlipNotSynced) )
					continue;

				// find out if this ped is in a vehicle, and if so make sure we only blip the 1 vehicle
				if (pPed->GetIsInVehicle())
				{
					if (!pPed->GetIsDrivingVehicle())
						continue;  // if is not the driver then assume we have already added it (as we always add the driver)
				}

				if (pPed->IsLawEnforcementPed())
				{
					if(!bWanted)
						continue;

					if(NetworkInterface::IsGameInProgress() && pPed->PopTypeIsRandom() &&
						pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_SMART_FLEE, true))
					{
						continue;
					}

					if(!pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DontBlipCop)/* &&
						!pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_BlippedByScript)*/)
					{
						// add cop blip onto minimap:
						//Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
						//Vector3 vPlayerPos = VEC3V_TO_VECTOR3(pWantedPed->GetTransform().GetPosition());
						//float fDist = (vPedPos - vPlayerPos).XYMag();

						//if (bIgnoreRadiusForBlip || fDist <= fWantedLevelRadius)
						{
							int blipForThisCop = GetBlipForPed(pPed);
							if(blipForThisCop != INVALID_BLIP_ID)
							{
								CMiniMapBlip *pBlip = CMiniMap::GetBlip(blipForThisCop);
								if (!pPed->IsArmyPed())
								{
									if (pPed->GetIsInVehicle() && pPed->GetVehiclePedInside()->GetVehicleType() == VEHICLE_TYPE_HELI)
									{
										if (CMiniMap::GetBlipObjectNameId(pBlip) != BLIP_POLICE_HELI_SPIN)
										{
											CMiniMap::SetBlipParameter(BLIP_CHANGE_OBJECT_NAME, blipForThisCop, BLIP_POLICE_HELI_SPIN);
											CMiniMap::SetBlipParameter(BLIP_CHANGE_SCALE, blipForThisCop, 0.8f);
											CMiniMap::SetBlipParameter(BLIP_CHANGE_SHORTRANGE, blipForThisCop, false);
											CMiniMap::SetBlipParameter(BLIP_CHANGE_SHOW_HEIGHT, blipForThisCop, false);
										}
									}
									else
									{
										if (CMiniMap::GetBlipObjectNameId(pBlip) != m_BlipSpriteToUseForCopPeds)
										{
											CMiniMap::SetBlipParameter(BLIP_CHANGE_OBJECT_NAME, blipForThisCop, m_BlipSpriteToUseForCopPeds);
											CMiniMap::SetBlipParameter(BLIP_CHANGE_SCALE, blipForThisCop, m_fBlipScaleToUseForCopPeds);
											CMiniMap::SetBlipParameter(BLIP_CHANGE_SHORTRANGE, blipForThisCop, false);  // blip must stay on side of radar as per LB 1156212
											CMiniMap::SetBlipParameter(BLIP_CHANGE_SHOW_HEIGHT, blipForThisCop, false);
										}
									}
								}
							}
							else if(!pPed->GetCreatedBlip()) // if not blipped then blip it
							{
								CopBlipListStruct newCopBlip;

								newCopBlip.m_lastFrameValid = fwTimer::GetFrameCount();

								if (pPed->IsArmyPed())
								{
									if(!IsPedAttackingPlayer(pPed, pWantedPed))
										continue;

									newCopBlip.iPedPoolIndex = CEntityPoolIndexForBlip(pPed, BLIP_TYPE_CHAR);
									newCopBlip.iBlipId = CMiniMap::CreateBlip(true, BLIP_TYPE_CHAR, newCopBlip.iPedPoolIndex, BLIP_DISPLAY_BOTH, "cop blip");
									// make sure we are using donuts for blips in MP
									if(NetworkInterface::IsGameInProgress())
										CMiniMap::SetBlipParameter(BLIP_CHANGE_OBJECT_NAME, newCopBlip.iBlipId, RADAR_TRACE_AI); 
									CMiniMap::SetBlipParameter(BLIP_CHANGE_COLOUR, newCopBlip.iBlipId, BLIP_COLOUR_RED);
									CMiniMap::SetBlipParameter(BLIP_CHANGE_SCALE, newCopBlip.iBlipId, 0.7f);
									CMiniMapBlip* pBlip = CMiniMap::GetBlip(newCopBlip.iBlipId);
									if(pBlip)
										CMiniMap::SetFlag(pBlip, BLIP_FLAG_HIDDEN_ON_LEGEND);
								}
								else 
								{ 
									newCopBlip.iPedPoolIndex = CEntityPoolIndexForBlip(pPed, BLIP_TYPE_COP);
									newCopBlip.iBlipId = CMiniMap::CreateBlip(true, BLIP_TYPE_COP, newCopBlip.iPedPoolIndex, BLIP_DISPLAY_BOTH, "cop blip");
									if (pPed->GetIsInVehicle() && pPed->GetVehiclePedInside()->GetVehicleType() == VEHICLE_TYPE_HELI)
									{
										CMiniMap::SetBlipParameter(BLIP_CHANGE_OBJECT_NAME, newCopBlip.iBlipId, BLIP_POLICE_HELI_SPIN);
										CMiniMap::SetBlipParameter(BLIP_CHANGE_SCALE, newCopBlip.iBlipId, 0.8f);
										CMiniMap::SetBlipParameter(BLIP_CHANGE_SHORTRANGE, newCopBlip.iBlipId, false);
										CMiniMap::SetBlipParameter(BLIP_CHANGE_SHOW_HEIGHT, newCopBlip.iBlipId, false);
									}
									else
									{
										CMiniMap::SetBlipParameter(BLIP_CHANGE_OBJECT_NAME, newCopBlip.iBlipId, m_BlipSpriteToUseForCopPeds);
										CMiniMap::SetBlipParameter(BLIP_CHANGE_SCALE, newCopBlip.iBlipId, m_fBlipScaleToUseForCopPeds);
										CMiniMap::SetBlipParameter(BLIP_CHANGE_SHORTRANGE, newCopBlip.iBlipId, false);  // blip must stay on side of radar as per LB 1156212
										CMiniMap::SetBlipParameter(BLIP_CHANGE_SHOW_HEIGHT, newCopBlip.iBlipId, false);
									}
								}
								blipForThisCop = newCopBlip.iBlipId;
								CMiniMap::SetBlipParameter(BLIP_CHANGE_ALPHA, blipForThisCop, 0);

								m_CopBlips.PushAndGrow(newCopBlip);
								//							Displayf("COP_BLIP: Added a cop blip %d with ped index %d.   Max size: %d", newCopBlip.iBlipId, newCopBlip.iPedPoolIndex, m_CopBlips.GetCount());

							}
							if(blipForThisCop != INVALID_BLIP_ID)
							{
								if (bCopsSearching && !pWantedPed->GetIsDeadOrDying())
								{
									CMiniMap::SetBlipParameter(BLIP_CHANGE_SHOW_CONE, blipForThisCop, true);  // cone
								}
								else
								{
									CMiniMap::SetBlipParameter(BLIP_CHANGE_SHOW_CONE, blipForThisCop, false);  // no cone
								}
							}
						}
					}
				}
				else //  check for non law enforcement enemy peds attacking the player
				{
					// Allow script to force security peds to be blipped on map if ped is wanted.
					bool bForceBlipSecurityPeds = pPed->IsSecurityPed() && pWantedPed->IsLocalPlayer() && pWantedPed->GetPedConfigFlag(CPED_CONFIG_FLAG_ForceBlipSecurityPedsIfPlayerIsWanted); 
					if(IsPedAttackingPlayer(pPed, pWantedPed) || bForceBlipSecurityPeds) // is ped in combat with the player 
					{
						// If ped isnt searching for player set player to attacked
						if(!pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_SEARCH))
							m_beingAttackedTimer = fwTimer::GetFrameCount();

						if(!pPed->PopTypeIsMission() && 
							((!CTheScripts::GetPlayerIsOnAMission() && 
							!CTheScripts::GetPlayerIsOnARandomEvent()) ||
							pPed->IsSecurityPed())) // Do not blip peds while you are on a mission unless they are security peds
						{
							// Check to see if we should blip passengers
							bool inVehicle = pPed->GetIsInVehicle();
							bool isPassengerInAlreadyBlippedVehicle = false;
							if(inVehicle)
							{
								CVehicle* pVehicle = pPed->GetMyVehicle();
								CPed* pDriver = NULL;
								if(pVehicle)
									pDriver = pVehicle->GetDriver();

								// if not driver
								if(pPed != pDriver)
								{
									// If the driver is already attacking the player then set to not blip passenger
									if(IsPedAttackingPlayer(pDriver, pWantedPed))
										isPassengerInAlreadyBlippedVehicle = true;
								}
							}

							if(!isPassengerInAlreadyBlippedVehicle)
							{
								int blipForThisEnemy = GetBlipForPed(pPed);
								if(blipForThisEnemy != INVALID_BLIP_ID)
								{
									if(NetworkInterface::IsGameInProgress())
										CMiniMap::SetBlipParameter(BLIP_CHANGE_OBJECT_NAME, blipForThisEnemy, /*inVehicle ? BLIP_LEVEL :*/ RADAR_TRACE_AI); // AF: Use same blip regardless of whether in vehicle or not
									CMiniMap::SetBlipParameter(BLIP_CHANGE_SCALE, blipForThisEnemy, inVehicle ? 1.0f : 0.7f);
									CMiniMap::SetBlipParameter(BLIP_CHANGE_COLOUR, blipForThisEnemy, BLIP_COLOUR_RED);
								}
								else if(!pPed->GetCreatedBlip())
								{
									CopBlipListStruct newCopBlip;

									newCopBlip.iPedPoolIndex = CEntityPoolIndexForBlip(pPed, BLIP_TYPE_CHAR);
									newCopBlip.iBlipId = CMiniMap::CreateBlip(true, BLIP_TYPE_CHAR, newCopBlip.iPedPoolIndex, BLIP_DISPLAY_BOTH, "enemy blip");
									newCopBlip.m_lastFrameValid = fwTimer::GetFrameCount();
									m_CopBlips.PushAndGrow(newCopBlip);

									int blipForThisEnemy = newCopBlip.iBlipId;

									if( NetworkInterface::IsGameInProgress() )
									{
										CMiniMap::SetBlipParameter(BLIP_CHANGE_OBJECT_NAME, blipForThisEnemy, RADAR_TRACE_AI);
									}
									CMiniMap::SetBlipParameter(BLIP_CHANGE_SCALE, blipForThisEnemy, inVehicle ? 1.0f : 0.7f);
									CMiniMap::SetBlipParameter(BLIP_CHANGE_COLOUR, blipForThisEnemy, BLIP_COLOUR_RED);
									CMiniMap::SetBlipParameter(BLIP_CHANGE_ALPHA, blipForThisEnemy, 0);
									CMiniMap::SetBlipParameter(BLIP_CHANGE_SHORTRANGE, blipForThisEnemy, false);  // blip must stay on side of radar as per LB 1156212
									CMiniMap::SetBlipParameter(BLIP_CHANGE_SHOW_HEIGHT, blipForThisEnemy, true);
									CMiniMapBlip* pBlip = CMiniMap::GetBlip(blipForThisEnemy);
									if(pBlip)
										CMiniMap::SetFlag(pBlip, BLIP_FLAG_HIDDEN_ON_LEGEND);
								}
							}
						}
					}
				}
			}	//	while(i--)
		}	//	if (m_PoliceRadarBlips)
	}

	for(int i=0; i<m_CopBlips.GetCount(); i++)
	{
		CMiniMapBlip *pBlip = CMiniMap::GetBlip(m_CopBlips[i].iBlipId);
		// If blip hasnt been updated in last 10 frames, delete it
		if(fwTimer::GetFrameCount() - m_CopBlips[i].m_lastFrameValid >= PED_POOL_FRAME_OFFSET)
		{
			if (pBlip)
			{
				CEntity* pEntity = CMiniMap::FindBlipEntity(pBlip);
				if(pEntity && pEntity->GetIsPhysical())
				{
					smart_cast<CPhysical*>(pEntity)->SetCreatedBlip(false);
				}
				CMiniMap::RemoveBlip(pBlip);
			} 
			m_CopBlips.DeleteFast(i);
			i--;
		}
		else
		{
			TUNE_GROUP_FLOAT(WANTED_RADIUS, innerRadius, 400.0f, 0.0f, 1000.0f, 1.0f);
			TUNE_GROUP_FLOAT(WANTED_RADIUS, outerRadius, 600.0f, 0.0f, 1000.0f, 1.0f);
			TUNE_GROUP_FLOAT(WANTED_RADIUS, minAlpha, 64.0f, 0.0f, 255.0f, 0.05f);
			TUNE_GROUP_INT(WANTED_RADIUS, fadeAmount, 12, 1, 32, 1);

			float fTargetAlpha = minAlpha;

			CEntity* pEntity = NULL;
			CMiniMapBlip *pBlip = CMiniMap::GetBlip(m_CopBlips[i].iBlipId);
			if(pBlip)
				pEntity = CMiniMap::FindBlipEntity(pBlip);
			if(pEntity)
			{
				Vector3 vPedPos = VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition());
				Vector3 vPlayerPos = VEC3V_TO_VECTOR3(pWantedPed->GetTransform().GetPosition());
				float fDist = (vPedPos - vPlayerPos).XYMag();

				// calculate target alpha
				if(fDist > outerRadius)
					fTargetAlpha = minAlpha;
				else if(fDist > innerRadius)
					fTargetAlpha = (255.0f * (outerRadius - fDist) / (outerRadius - innerRadius)) + (minAlpha * (fDist - innerRadius) / (outerRadius - innerRadius));
				else
					fTargetAlpha = 255.0f;
			}

			// fade in blip
			int targetAlpha = (int)fTargetAlpha;
			int alpha = CMiniMap::GetBlipAlphaValue(pBlip);
			if(alpha != targetAlpha)
				CMiniMap::SetBlipParameter(BLIP_CHANGE_ALPHA, m_CopBlips[i].iBlipId, ::Min(alpha + fadeAmount, targetAlpha));
		}
	}

	if(fwTimer::GetFrameCount() - m_beingAttackedTimer < PED_POOL_FRAME_OFFSET && m_beingAttackedTimer > 0)
		m_beingAttacked = true;
	else
		m_beingAttacked = false;
}

void CWantedBlips::DisableCones()
{
	wantedDebugf3("CWantedBlips::DisableCones");
	for(int i=0; i<m_CopBlips.GetCount(); i++)
	{
		CMiniMapBlip *pBlip = CMiniMap::GetBlip(m_CopBlips[i].iBlipId);
		if (pBlip)
		{
			wantedDebugf3("CWantedBlips::DisableCones");
			CMiniMap::SetBlipParameter(BLIP_CHANGE_SHOW_CONE, m_CopBlips[i].iBlipId, false);  // no cone
		}
	}
}

void CWantedBlips::CreateAndUpdateRadarBlips(CWanted* pWanted)
{
	eWantedLevel effectiveWantedLevel = pWanted->GetWantedLevelIncludingVehicleOccupants(NULL);

	// If clean remove wanted circle
	if (effectiveWantedLevel == WANTED_CLEAN && CScriptHud::iFakeWantedLevel == WANTED_CLEAN)
	{	
		if (m_aCircleRadarBlip != INVALID_BLIP_ID)
		{
			// Tidy up the radar blips.
			CMiniMapBlip *pBlip = CMiniMap::GetBlip(m_aCircleRadarBlip);
			if (pBlip)
			{
				CMiniMap::RemoveBlip(pBlip);
			}

			m_aCircleRadarBlip = INVALID_BLIP_ID;
		}
		return;
	}

	if (CWanted::sm_bEnableRadiusEvasion && (pWanted->GetWantedLevel() > 0 || CScriptHud::iFakeWantedLevel > 0))
	{
		Vector3 lastSpottedByPolice;
		eWantedLevel effectiveWantedLevel = WANTED_CLEAN;
		float	blipRadius = 0.0f;

		if (pWanted->GetWantedLevel() > 0)
		{
			effectiveWantedLevel = pWanted->GetWantedLevelIncludingVehicleOccupants(&lastSpottedByPolice);
			blipRadius = CDispatchData::GetWantedZoneRadius(effectiveWantedLevel, pWanted->GetPed());
		}
		else
		{
			// if we havent got a real wanted level but the scripters have set a fake one then use the fake details:
			if (CScriptHud::iFakeWantedLevel > 0)
			{
				effectiveWantedLevel = (eWantedLevel)CScriptHud::iFakeWantedLevel;
				lastSpottedByPolice = CScriptHud::vFakeWantedLevelPos;
				blipRadius = CDispatchData::GetWantedZoneRadius(effectiveWantedLevel, pWanted->GetPed());
			}
		}

		Vector3 BlipCoors = lastSpottedByPolice;// + CWanted::GetWantedZoneRadius(m_WantedLevel, m_pPed) * Vector3(rage::Sinf(Angle), rage::Cosf(Angle), 0.0f);

		if (m_aCircleRadarBlip == INVALID_BLIP_ID && BlipCoors != ORIGIN)
		{
			eBLIP_TYPE blipType = BLIP_TYPE_RADIUS;
			m_aCircleRadarBlip = CMiniMap::CreateBlip(true, blipType, BlipCoors, BLIP_DISPLAY_BLIPONLY, "Wanted circle");
		}

		Assert(CMiniMap::IsBlipIdInUse(m_aCircleRadarBlip));

		if (m_aCircleRadarBlip != INVALID_BLIP_ID && CMiniMap::IsBlipIdInUse(m_aCircleRadarBlip))
		{
			CMiniMap::SetBlipParameter(BLIP_CHANGE_POSITION, m_aCircleRadarBlip, BlipCoors);

			CMiniMap::SetBlipParameter(BLIP_CHANGE_SCALE, m_aCircleRadarBlip, blipRadius);

			static bool bPoliceColour = false;
			static u32 iCurrentColourCounter = fwTimer::GetSystemTimeInMilliseconds();

			u32 iColourSwapTimeMs = 500;

			if (pWanted->CopsAreSearching())
				iColourSwapTimeMs = 250;

			if (fwTimer::GetSystemTimeInMilliseconds() - iCurrentColourCounter > iColourSwapTimeMs)
			{
				iCurrentColourCounter = fwTimer::GetSystemTimeInMilliseconds();
				bPoliceColour = !bPoliceColour;
			}

			s32 iColourId;

			if (bPoliceColour)
				iColourId = BLIP_COLOUR_POLICE_RADAR_RED;
			else
				iColourId = BLIP_COLOUR_POLICE_RADAR_BLUE;

			CMiniMap::SetBlipParameter(BLIP_CHANGE_COLOUR, m_aCircleRadarBlip, iColourId);
		}
	}
}


