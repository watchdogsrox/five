
/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    junctions.h
// PURPOSE : When cars cross paths on a junction this code will stop one of the two from entering the junction. 
// AUTHOR :  Obbe.
// CREATED : 16/4/07
//
/////////////////////////////////////////////////////////////////////////////////

#include "ai/stats.h"
#include "core/game.h"
#include "camera/CamInterface.h"
#include "camera\debug\DebugDirector.h"
#include "camera\helpers\Frame.h"
#include "control/trafficlights.h"
#include "game/ModelIndices.h"
#include "grcore/debugdraw.h"
#include "Objects/Door.h"
#include "parser/manager.h"
#include "parser/psofile.h"
#include "parser/psoparserbuilder.h"
#include "parser/visitorxml.h"
#include "scene/DataFileMgr.h"
#include "scene/world/gameworld.h"
#include "VehicleAi/driverpersonality.h"
#include "vehicleAi/junctions.h"
#include "vehicleAi/vehicleintelligence.h"
#include "vehicleAi/Task/TaskVehicleCruise.h"
#include "vehicleAi/Task/TaskVehicleTempAction.h"
#include "vehicleAi/Task/TaskVehicleGoToAutomobile.h"
#include "vehicles/train.h"
#include "vehicles/vehicle.h"
#include "vehicles/vehiclepopulation.h"
#include "vehicles/virtualroad.h"

// Parser files
#include "Junctions_parser.h"

AI_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()

#define JUNCTIONTL_DEBUG 0

#define LEFT_TURN_THRESHOLD -0.35f

atRangeArray<float, JUNCTIONS_MAX_NUMBER> CJunctions::m_TimesliceTimeSteps;
atRangeArray<u8, JUNCTIONS_MAX_NUMBER> CJunctions::m_TimesliceFramesSinceUpdate;
int CJunctions::m_TimesliceUpdateIndex = 0;
int CJunctions::m_TimesliceUpdatePeriod = 4;
int CJunctions::m_TimesliceJunctionsInUse = JUNCTIONS_MAX_NUMBER;
int CJunctions::m_TimesliceJunctionsInUseCounting = 0;
bool CJunctions::m_TimesliceEnabled = true;
unsigned CJunctions::m_LastNetworkUpdateTime = 0;

namespace AIStats
{
	EXT_PF_TIMER(AddVehicleToJunction);
}
using namespace AIStats;

bool IsSlipLaneJunctionNode(const CNodeAddress junctionNode);

CJunctionTemplate::CJunctionTemplate()
{
	m_iFlags = 0;
	for(int j=0; j<MAX_JUNCTION_NODES_PER_JUNCTION; j++)
	{
		m_vJunctionNodePositions[j].Zero();
	}
	m_iNumEntrances = 0;
	m_iNumPhases = 0;
	m_iNumTrafficLightLocations = 0;
	m_iNumJunctionNodes = 0;

	m_vJunctionMin.Zero();
	m_vJunctionMax.Zero();

	for(s32 e=0; e<MAX_ROADS_INTO_JUNCTION; e++)
	{
		memset(&m_Entrances[e], 0, sizeof(CEntrance));
		m_Entrances[e].m_iLeftFilterLanePhase = -1;
		m_Entrances[e].m_bCanTurnRightOnRedLight = false;
		m_Entrances[e].m_bLeftLaneIsAheadOnly = false;
		m_Entrances[e].m_bRightLaneIsRightOnly = false;
	}

	float fStartTime = 0.0f;
	const float fDefaultDuration = 10.0f;
	for(s32 p=0; p<MAX_ROADS_INTO_JUNCTION; p++)
	{
		m_PhaseTimings[p].m_fStartTime = fStartTime;
		m_PhaseTimings[p].m_fDuration = fDefaultDuration;
		fStartTime += fDefaultDuration;
	}
	
	m_fSearchDistance = 35.0f;
}


atRangeArray<CJunction,JUNCTIONS_MAX_NUMBER> CJunctions::m_aJunctions;
CJunctionTemplateArray	* CJunctions::m_JunctionTemplates = NULL;
float					CJunctions::m_fTimeSinceLastScan = 0.0f;

bank_bool CJunctions::ms_bInstanceJunctionsAroundPlayer = true;
const float CJunctions::ms_fScriptCommandJunctionProximitySqr = 40.0f*40.0f;
bank_float CJunctions::ms_fInstanceJunctionDist = 125.0f;
bank_float CJunctions::ms_fScanFrequency = 1.0f;
const float CJunctions::ms_fJunctionNodePosEps = 1.5f;
const float CJunctions::ms_fEntranceNodePosEps = 0.25f;
#if __JUNCTION_EDITOR
char CJunctions::ms_JunctionEditorXmlFilename[RAGE_MAX_PATH];
#endif

bank_float CJunction::ms_fMaxDistanceFromTrainTrack = 40.0f;
bank_float CJunction::ms_fScanForRailwayBarriersRange = 22.0f;
bank_s32 CJunction::ms_iScanForRailwayBarriersFreqMs = 3000;
bank_s32 CJunction::ms_iScanForApproachingTrainsFreqMs = 3000;
bank_float CJunction::ms_fDurationRatioForSafeCrossing = 0.85f;
bank_u32 CJunction::ms_uRailwayLightPulseDuration = 1000;

bank_float CJunction::ms_LightPhaseDurationMultiplierEmpty = 0.5f;
bank_float CJunction::ms_LightPhaseDurationMultiplierLow = 1.0f;
bank_float CJunction::ms_LightPhaseDurationMultiplierHigh = 2.0f;
bank_float CJunction::ms_LightPhaseDurationMultiplierExtreme = 2.5f;
bank_u32 CJunction::ms_LightPhaseDurationMultiplierLowThreshold = 3;
bank_u32 CJunction::ms_LightPhaseDurationMultiplierHighThreshold = 8;
bank_u32 CJunction::ms_LightPhaseDurationMultiplierExtremeThreshold = 14;

bank_float CJunction::ms_fMaxTimeExtensionForCrossingPeds = 3.0f;

const float CJunction::ms_fRailCrossingCloseRatio = 0.1f;
const float CJunction::ms_fRailCrossingOpenRatio = 0.85f;

#if __BANK
bool CJunctions::m_bDebug				= false;
bool CJunctions::m_bDebugWaitForTraffic	= false;
bool CJunctions::m_bDebugText			= false;
bool CJunctions::m_bDebugTimeslicing	= false;
bool CJunctions::m_bDebugLights			= false;
bool CJunctions::m_bDisableProcessing	= false;
#endif

bool PathfindFindJunctionNodeCB(CPathNode * pNode, void * UNUSED_PARAM(pData))
{
	if(pNode->IsWaterNode() || pNode->IsPedNode() || pNode->IsParkingNode() || pNode->IsOpenSpaceNode())
		return false;

	if(!pNode->IsJunctionNode())
		return false;

	return true;
}
bool PathfindFindNonJunctionNodeCB(CPathNode * pNode, void * UNUSED_PARAM(pData))
{
	if(pNode->IsWaterNode() || pNode->IsPedNode() || pNode->IsParkingNode() || pNode->IsOpenSpaceNode())
		return false;

	if(pNode->IsJunctionNode())
		return false;

	return true;
}

void CJunctionEntrance::Clear()
{
	m_vPositionOfNode.Zero();
	m_Node.SetEmpty();
	m_iPhase = -1;
	m_EntryStopDistance = 0.0f;
	m_vEntryDir.Zero();
	m_bCanTurnRightOnRedLight = false;
	m_bLeftLaneIsAheadOnly = false;
	m_bRightLaneIsRightOnly = false;
	m_bIsGiveWay = false;
	m_bIsSwitchedOff = false;
	m_bIgnoreBackedUpExitsOnStraight = false;
	m_bHasPlayer = false;
	m_iLeftFilterPhase = -1;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : Clear
// PURPOSE :  Removes all cars from a junction and clears pre-calculated data
/////////////////////////////////////////////////////////////////////////////////

void CJunction::Clear()
{
	m_HighestVehicleArrayElementUsed = -1;

	m_iNumJunctionNodes = 0;
	m_iNumEntrances = 0;
	m_iNumLightPhases = 0;
	m_iLightPhase = 0;
	m_fLightTimeRemaining = 0.0f;
#if __BANK
	m_fLightPhaseDurationMultiplier = 1.0f;
#endif
	m_iNumLightPhaseVehicles = 0;
	m_iLRUTime = 0;
	m_iLastBarrierScanTime = 0;
	m_iLastApproachingTrainScanTime = 0;
	m_uLastRailwayLightPulseTime = 0;

	m_RailwayCrossingLightState = RAILWAY_CROSSING_LIGHT_OFF;

	m_uNumActivelyCrossingPeds = 0;
	m_uNumWaitingForLightPeds = 0;

	m_iTemplateIndex = -1;

	m_bMalfunctioning = false;
	m_bErrorBindingJunction = false;
	m_bIsRailwayCrossing = false;
	m_bCanSkipPedPhase = false;
	m_bRailwayBarriersShouldBeDown = false;
	m_bRailwayBarriersAreFullyRaised = false;
	m_HasPedCrossingPhase = false;
	m_bExtendedTimeForCrossingPeds = false;
	m_iRequiredByScriptId = THREAD_INVALID;
	m_bIsOnlyJunctionBecauseHasSwitchedOffEntrances = false;

	m_iCycleOffsetMs = 0;
	m_fCycleScale = 1.0f;

	s32 i;

	for(i=0; i<MAX_JUNCTION_NODES_PER_JUNCTION; i++)
	{
		m_JunctionNodes[i].SetEmpty();
	}
	for(i=0; i<JUNCTION_MAX_RAILWAY_TRACKS; i++)
	{
		m_pTrainTrackNodes[i] = NULL;
	}
	for(i=0; i<JUNCTION_MAX_RAILWAY_BARRIERS; i++)
	{
		m_RailwayBarriers[i] = NULL;
	}
	for (i = 0; i < JUNCTION_MAX_VEHICLES; i++)
	{
		m_pVehicles[i] = NULL;
	}
	for(i=0; i<MAX_ROADS_INTO_JUNCTION; i++)
	{
		m_Entrances[i].Clear();
	}

	m_iVirtualJunctionIndex = -1;

	RemoveTrafficLights();
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : GetNumberOfCars
// PURPOSE :  Goes through the array and counts the number of cars involved
/////////////////////////////////////////////////////////////////////////////////

s32	CJunction::GetNumberOfCars()
{
	s32	result=0;
	for (s32 i = 0; i < JUNCTION_MAX_VEHICLES; i++)
	{
		if (m_pVehicles[i])
		{
			result++;
		}
	}
	return result;
}

//---------------------------------------------------------------------------------
// GetLightStatusForPedCrossing
// Returns the lights status for peds at this junction
// Optionally consider the remaining time of the current phase to consider the light red or green
s32 CJunction::GetLightStatusForPedCrossing(bool bConsiderTimeRemaining) const
{
	// Check all of the entrances for this junction
	for(int e=0; e < m_iNumEntrances; e++)
	{
		// If any of them are active, then traffic is flowing
		if( m_Entrances[e].m_iPhase == m_iLightPhase )
		{
			// return LIGHT_GREEN so the ped waits
			return LIGHT_GREEN;
		}
	}

	// Otherwise all entrances are stopped for peds to cross:
	//
	// Check if we are not considering time remaining,
	// OR there is enough of the full phase time remaining for a safe crossing
	if( !bConsiderTimeRemaining || m_fLightTimeRemaining > m_LightTiming[m_iLightPhase].m_fDuration * ms_fDurationRatioForSafeCrossing )
	{
		// return LIGHT_RED, the ped may cross
		return LIGHT_RED;
	}
	
	// return LIGHT_GREEN so the ped waits
	return LIGHT_GREEN;
}

void CJunction::SetToMalfunction(bool b)
{
	Assert(m_iTemplateIndex != -1);
	if(m_iTemplateIndex != -1)
	{
		m_bMalfunctioning = b;
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : Update
// PURPOSE :  Updates a junction. May also render debug stuff.
/////////////////////////////////////////////////////////////////////////////////

void	CJunction::Update(float timeStep)
{
	if( m_iNumLightPhases > 0 && !ShouldCarsStopForTrain() )
	{
		Assert(m_iTemplateIndex != -1);

		m_fLightTimeRemaining -= timeStep;

		if(!NetworkInterface::IsGameInProgress() && m_fLightTimeRemaining <= 0.0f && m_iNumLightPhaseVehicles == 0 && !m_bExtendedTimeForCrossingPeds && m_uNumActivelyCrossingPeds > 0)
		{
			m_fLightTimeRemaining += ms_fMaxTimeExtensionForCrossingPeds;
			m_bExtendedTimeForCrossingPeds = true;
		}

		while(m_fLightTimeRemaining <= 0.0f || (m_bExtendedTimeForCrossingPeds && m_uNumActivelyCrossingPeds == 0))
		{
			m_bExtendedTimeForCrossingPeds = false;

			if(m_bMalfunctioning)
			{
				m_iLightPhase = fwRandom::GetRandomNumberInRange(0, m_iNumLightPhases);
				m_fLightTimeRemaining = fwRandom::GetRandomNumberInRange(0.1f, 2.0f);
			}
			else
			{
				s32 iInitialPhase = m_iLightPhase;
				u32 iPhaseEntrances = 0;
				u32 iPhaseVehicles = 0;
				float fMultiplier = 1.0f;

				float fTimeOverage = m_fLightTimeRemaining;
				
				m_fLightTimeRemaining = 0.0f;

				while(m_fLightTimeRemaining == 0.0f)
				{
					m_iLightPhase++;
					if(m_iLightPhase >= m_iNumLightPhases)
						m_iLightPhase = 0;

					if( !NetworkInterface::IsGameInProgress() && CanSkipPedPhase() )
					{
						m_iLightPhase++;
						if(m_iLightPhase >= m_iNumLightPhases)
							m_iLightPhase = 0;
					}

					m_fLightTimeRemaining = m_LightTiming[m_iLightPhase].m_fDuration;

					iPhaseEntrances = 0;
					iPhaseVehicles = 0;

					for (s32 e = 0; e < m_iNumEntrances; e++)
					{
						if(m_Entrances[e].m_iPhase == m_iLightPhase || m_Entrances[e].m_iLeftFilterPhase == m_iLightPhase)
						{
							++iPhaseEntrances;

							if(m_Entrances[e].m_iNumVehicles > iPhaseVehicles)
							{
								iPhaseVehicles = m_Entrances[e].m_iNumVehicles;
							}
						}
					}

					if(iPhaseEntrances > 0)
					{
						static const float fAmberTime = ((float)LIGHTDURATION_AMBER) / 1000.0f;

						fMultiplier = 1.0f;

						if(!NetworkInterface::IsGameInProgress())
						{
							if(iPhaseVehicles == 0)
							{
								fMultiplier = ms_LightPhaseDurationMultiplierEmpty;
							}
							else if(iPhaseVehicles <= ms_LightPhaseDurationMultiplierLowThreshold)
							{
								fMultiplier = ms_LightPhaseDurationMultiplierLow;
							}
							else if(iPhaseVehicles >= ms_LightPhaseDurationMultiplierExtremeThreshold)
							{
								fMultiplier = ms_LightPhaseDurationMultiplierExtreme;
							}
							else if(iPhaseVehicles >= ms_LightPhaseDurationMultiplierHighThreshold)
							{
								fMultiplier = ms_LightPhaseDurationMultiplierHigh;
							}
						}

						if(fMultiplier == 0.0f)
						{
							m_fLightTimeRemaining = 0.0f;
						}
						else if(fMultiplier != 1.0f)
						{
							m_fLightTimeRemaining = ((m_fLightTimeRemaining - fAmberTime) * fMultiplier) + fAmberTime;
						}
					}

					if(m_iLightPhase == iInitialPhase)
					{
						if(m_fLightTimeRemaining == 0.0f)
						{
							m_fLightTimeRemaining = m_LightTiming[m_iLightPhase].m_fDuration;
						}

						break;
					}
				}

				// If we went below 0, remove this time from the next phase
				m_fLightTimeRemaining += fTimeOverage;

				m_iNumLightPhaseVehicles = iPhaseVehicles;
#if __BANK
				m_fLightPhaseDurationMultiplier = fMultiplier;
#endif // __BANK
			}
		}
	}

	for(s32 n=0; n<m_iNumJunctionNodes; n++)
	{
		if(m_JunctionNodes[n].IsEmpty())
			return;
	}

	m_iLRUTime = fwTimer::GetTimeInMilliseconds();

	//---------------------------
	// Update railway crossings

	if(m_bIsRailwayCrossing)
	{
		UpdateRailwayCrossing(timeStep);
	}

	//-----------------------------------------
	// Remove distant vehicles from junction

	const float fForceRemovalRangeSqr = 100.0f*100.0f;

	s32 i;
	for (i=0; i<=m_HighestVehicleArrayElementUsed; i++)
	{
		CVehicle * pVeh = m_pVehicles[i];
		if(pVeh)
		{
			if( GetIsMalfunctioning() ||
				(VEC3V_TO_VECTOR3(pVeh->GetTransform().GetPosition()) - m_vJunctionCenter).Mag2() > fForceRemovalRangeSqr)
			{
				const bool bPrimaryJunction = pVeh->GetIntelligence()->GetJunction() == this;

				if (bPrimaryJunction)
				{
					pVeh->GetIntelligence()->ResetCachedJunctionInfo();
				}

				// null out the ref-ptr
				m_pVehicles[i] = NULL;
			}
		}
	}

	// Removes the gaps that removed vehicles left.
	TidyJunction();			

	if (m_HighestVehicleArrayElementUsed < 0 && !GetRequiredByScript() )
	{		
		// Junction is empty. and not required by a mission script
		return;
	}
	Assert(m_HighestVehicleArrayElementUsed < JUNCTION_MAX_VEHICLES);

	// if multiple cars approach this junction only the first one gets the green light.

	static float dists[JUNCTION_MAX_VEHICLES];
	static CNodeAddress	entryNodes[JUNCTION_MAX_VEHICLES];
	static CNodeAddress	exitNodes[JUNCTION_MAX_VEHICLES];
	static s32 iEntryLanes[JUNCTION_MAX_VEHICLES];
	static s32 iExitLanes[JUNCTION_MAX_VEHICLES];
	static bool bDontConsider[JUNCTION_MAX_VEHICLES];
	static s32 iEntrances[JUNCTION_MAX_VEHICLES];
	static bool bRedLights[JUNCTION_MAX_VEHICLES];

	for(int v=0; v<JUNCTION_MAX_VEHICLES; v++)
	{
		bDontConsider[v] = false;
		iEntrances[v] = -1;
	}

	for (s32 e = 0; e < m_iNumEntrances; e++)
	{
		m_Entrances[e].m_iNumVehicles = 0;
		m_Entrances[e].m_bHasPlayer = false;
	}

	for (s32 c = 0; c <= m_HighestVehicleArrayElementUsed; c++)
	{
		CVehicle * pVeh = m_pVehicles[c];
		Assert(pVeh);

		if(pVeh == CVehiclePopulation::ms_PlayersJunctionNodeVehicle)
		{
			s32 iEntranceIndex = FindEntranceIndexWithNode(CVehiclePopulation::ms_PlayersJunctionEntranceNode);

			if (iEntranceIndex != -1)
			{
				if (!pVeh->GetIntelligence()->GetCarWeAreBehind())
				{
					m_Entrances[iEntranceIndex].m_iNumVehicles += pVeh->GetIntelligence()->m_NumCarsBehindUs + 1;
				}

				if (!m_Entrances[iEntranceIndex].m_bHasPlayer)
				{
					m_Entrances[iEntranceIndex].m_iNumVehicles += ms_LightPhaseDurationMultiplierHighThreshold / 2;
					m_Entrances[iEntranceIndex].m_bHasPlayer = true;
				}
			}

			continue;
		}

		const bool bPrimaryJunction = pVeh->GetIntelligence()->GetJunction() == this;

		if (bPrimaryJunction)
		{
			dists[c] = -LARGE_FLOAT;
			entryNodes[c] = pVeh->GetIntelligence()->GetJunctionEntranceNode();
			exitNodes[c] = pVeh->GetIntelligence()->GetJunctionExitNode();
			iEntryLanes[c] = pVeh->GetIntelligence()->GetJunctionEntranceLane();
			iExitLanes[c] = pVeh->GetIntelligence()->GetJunctionExitLane();
			bRedLights[c] = CVehicleJunctionHelper::ApproachingRedLight(pVeh);
		}

		// Calculate the distance of this car to the Junction.
		if (bPrimaryJunction && CalculateDistanceToJunction(pVeh, &iEntrances[c], dists[c]))
		{
			// At the moment, FindCarWeAreBehind() occasionally divines that cars are behind each other, breaking m_NumCarsBehindUs
			// Using a large (incorrect) number for m_iNumVehicles is mostly harmless -- disabling assert for the time being
			//Assertf(m_Entrances[iEntrances[c]].m_iNumVehicles < 100, "Sanity check -- %i vehicles at a single entrance?", m_Entrances[iEntrances[c]].m_iNumVehicles);

			if (!pVeh->GetIntelligence()->GetCarWeAreBehind())
			{
				m_Entrances[iEntrances[c]].m_iNumVehicles += pVeh->GetIntelligence()->m_NumCarsBehindUs + 1;
			}

			if (!m_Entrances[iEntrances[c]].m_bHasPlayer && pVeh->ContainsPlayer())
			{
				m_Entrances[iEntrances[c]].m_iNumVehicles += ms_LightPhaseDurationMultiplierHighThreshold / 2;
				m_Entrances[iEntrances[c]].m_bHasPlayer = true;
			}
		}
		else
		{
			bDontConsider[c] = true;

			if (bPrimaryJunction)
			{
				pVeh->GetIntelligence()->ResetCachedJunctionInfo();
			}

			bool bSecondayJunction = pVeh->GetIntelligence()->GetPreviousJunction() == this;

			if (bSecondayJunction)
			{
				dists[c] = -LARGE_FLOAT;
				entryNodes[c] = pVeh->GetIntelligence()->GetPreviousJunctionEntranceNode();
				exitNodes[c] = pVeh->GetIntelligence()->GetPreviousJunctionExitNode();
				iEntryLanes[c] = pVeh->GetIntelligence()->GetPreviousJunctionEntranceLane();
				iExitLanes[c] = pVeh->GetIntelligence()->GetPreviousJunctionExitLane();
				iEntrances[c] = FindEntranceIndexWithNode(entryNodes[c]);
				bRedLights[c] = false;

				const CPathNode* pExitNode = ThePaths.FindNodePointerSafe(pVeh->GetIntelligence()->GetPreviousJunctionExitNode());

				if (!pExitNode || (pExitNode && IsTrue(MagSquared(VECTOR3_TO_VEC3V(pExitNode->GetPos()) - pVeh->GetVehiclePosition()) >= ScalarV(225.0f))))
				{
					RemoveVehicleFromSlot(c);
					if(!pVeh->GetIntelligence()->GetJunction())
					{
						pVeh->GetIntelligence()->ResetCachedJunctionInfo();
						pVeh->GetIntelligence()->SetJunctionCommand(JUNCTION_COMMAND_NOT_ON_JUNCTION);
					}

					// Since this vehicle has now been removed, step back to process the slot again.
					// Note: relies on RemoveVehicleFromSlot() not touching any elements before this,
					// which would invalidate our temporary per-vehicle arrays.
					c--;
				}
			}
			else
			{
				RemoveVehicleFromSlot(c);
				if(!pVeh->GetIntelligence()->GetJunction())
				{
					pVeh->GetIntelligence()->ResetCachedJunctionInfo();
					pVeh->GetIntelligence()->SetJunctionCommand(JUNCTION_COMMAND_NOT_ON_JUNCTION);
				}

				// Since this vehicle has now been removed, step back to process the slot again.
				c--;
			}
		}
	}

	//---------------------------------------------------
	// Process setting the vehicles' 'junction command'
	while (1)
	{
		u32 iEarliestArrivalTime = UINT_MAX;
		s32 c = -1;

		for (s32 m = 0; m <= m_HighestVehicleArrayElementUsed; m++)
		{
			if (m_pVehicles[m] == CVehiclePopulation::ms_PlayersJunctionNodeVehicle || m_pVehicles[m]->IsNetworkClone())
			{
				continue;
			}

			if (!bDontConsider[m])
			{
				u32 iArrivalTime = m_pVehicles[m]->GetIntelligence()->GetJunctionArrivalTime();

				if (iArrivalTime <= iEarliestArrivalTime)
				{
					iEarliestArrivalTime = iArrivalTime;
					c = m;
				}
			}
		}

		if (c < 0)
		{
			break;
		}

		bDontConsider[c] = true;

		CVehicle * pVeh = m_pVehicles[c];

		const float fDist = dists[c];

		TUNE_GROUP_FLOAT(JUNCTION_ENTRANCE_ZONES, fStopZoneDistBase, 10.0f, 0.0f, 50.0f, 1.0f);
		TUNE_GROUP_FLOAT(JUNCTION_ENTRANCE_ZONES, fGoZoneDist, 3.0f, 0.0f, 20.0f, 1.0f);
		TUNE_GROUP_FLOAT(JUNCTION_ENTRANCE_ZONES, fAlreadyOnJunctionDist, -3.0f, -20.0f, 0.0f, 1.0f);
		const float fStopZoneDist = fStopZoneDistBase * Max(1.0f, CVehicleIntelligence::FindSpeedMultiplierWithSpeedFromNodes(pVeh->GetIntelligence()->SpeedFromNodes));

		const bool bInStopZone = fDist < fStopZoneDist;
		const bool bInGoZone = fDist < fGoZoneDist;

		const s8 iGoCommand = bInGoZone ? static_cast<s8>(JUNCTION_COMMAND_GO) : static_cast<s8>(JUNCTION_COMMAND_APPROACHING);
		const s8 iWaitForTrafficCommand = bInStopZone ? static_cast<s8>(JUNCTION_COMMAND_WAIT_FOR_TRAFFIC) : static_cast<s8>(JUNCTION_COMMAND_APPROACHING);

		if (bInGoZone && pVeh->GetIntelligence()->GetJunctionArrivalTime() == UINT_MAX)
		{
			pVeh->GetIntelligence()->SetJunctionCommand(JUNCTION_COMMAND_WAIT_FOR_TRAFFIC);
			pVeh->GetIntelligence()->RecordJunctionArrivalTime();
		}

		if(pVeh->GetIntelligence()->GetShouldObeyTrafficLights())
		{
			// Vehicles that are currently stopped by traffic lights are told to wait
			if (fDist < fStopZoneDist && fDist != -LARGE_FLOAT)
			{
				//----------------------------------------------------------------------
				// See whether traffic lights for this junction are stop or go.
				// This checks lights cycle for templated junctions, or falls back to
				// old cardinal axis code for non-templated ones.

				//first check if we're approaching a stop sign
				//assume that we don't ever have a stop sign + stop light,
				//so if we find a stop sign ignore the light
				bool bGiveWayEntrance = m_Entrances[iEntrances[c]].m_bIsGiveWay;

				bool bGoingRightOnRed = false;
				const ETrafficLightCommand iCommand = GetTrafficLightCommand(pVeh, iEntrances[c], bGoingRightOnRed);

				pVeh->GetIntelligence()->SetTrafficLightCommand(iCommand);

				const bool bShouldGiveWay = bGiveWayEntrance && !CDriverPersonality::RunsStopSigns(pVeh->GetDriver(), pVeh);

				if (bShouldGiveWay)
				{
					if (!VehicleRouteClashesWithOtherCarOnJunction(c, entryNodes, exitNodes, dists, bGoingRightOnRed, iEntryLanes, iExitLanes, iEntrances, bRedLights))
					{
						pVeh->GetIntelligence()->SetJunctionCommand(iGoCommand);
					}
					else
					{
						pVeh->GetIntelligence()->SetJunctionCommand(iWaitForTrafficCommand);
					}
				}
				else if (!bGiveWayEntrance)
				{
					//float speedMult;

					switch(iCommand)
					{
						case TRAFFICLIGHT_COMMAND_GO:
						{
							if (!VehicleRouteClashesWithOtherCarOnJunction(c, entryNodes, exitNodes, dists, bGoingRightOnRed, iEntryLanes, iExitLanes, iEntrances, bRedLights))
							{
								//test for obstructions now even if we're in a templated junction
								pVeh->GetIntelligence()->SetJunctionCommand(iGoCommand);
#if __BANK
								if (CJunctions::m_bDebug)
								{
									grcDebugDraw::Sphere(VEC3V_TO_VECTOR3(pVeh->GetTransform().GetPosition()), 2.0f, Color_white, true);
								}
#endif
							}
							else
							{
								pVeh->GetIntelligence()->SetJunctionCommand(iWaitForTrafficCommand);
							}

							break;
						}
						case TRAFFICLIGHT_COMMAND_AMBERLIGHT:
						{
							const u32 turnDir = pVeh->GetIntelligence()->GetJunctionTurnDirection();

							if( !CDriverPersonality::RunsAmberLights(pVeh->GetDriver(), pVeh, turnDir) && fDist > fAlreadyOnJunctionDist )
							{
								pVeh->GetIntelligence()->SetJunctionCommand(JUNCTION_COMMAND_WAIT_FOR_LIGHTS);
							}
							else if (VehicleRouteClashesWithOtherCarOnJunction(c, entryNodes, exitNodes, dists, bGoingRightOnRed, iEntryLanes, iExitLanes, iEntrances, bRedLights))
							{
								//test for obstructions now even if we're in a templated junction
								pVeh->GetIntelligence()->SetJunctionCommand(iWaitForTrafficCommand);
							}
							else
							{
								pVeh->GetIntelligence()->SetJunctionCommand(iGoCommand);
#if __BANK
								if (CJunctions::m_bDebug)
								{
									grcDebugDraw::Sphere(VEC3V_TO_VECTOR3(pVeh->GetTransform().GetPosition()), 2.0f, Color_orange, true);
								}
#endif
							}

							break;
						}
						case TRAFFICLIGHT_COMMAND_STOP:
						{
#if __BANK
							if (CJunctions::m_bDebug)
							{
								grcDebugDraw::Sphere(VEC3V_TO_VECTOR3(pVeh->GetTransform().GetPosition()), 1.0f, Color_magenta, true);
							}
#endif
							if (fDist <= fAlreadyOnJunctionDist && !VehicleRouteClashesWithOtherCarOnJunction(c, entryNodes, exitNodes, dists, bGoingRightOnRed, iEntryLanes, iExitLanes, iEntrances, bRedLights))
							{
								// Cars that are already on the junction (perhaps pushed there) are told to just go.
								pVeh->GetIntelligence()->SetJunctionCommand(JUNCTION_COMMAND_GO);
#if __BANK
								if (CJunctions::m_bDebug)
								{
									grcDebugDraw::Sphere(VEC3V_TO_VECTOR3(pVeh->GetTransform().GetPosition()), 2.0f, Color_green, true);
								}
#endif
							}
							else
							{
								pVeh->GetIntelligence()->SetJunctionCommand(JUNCTION_COMMAND_WAIT_FOR_LIGHTS);
							}

							break;
						}

						case TRAFFICLIGHT_COMMAND_FILTER_LEFT:
						case TRAFFICLIGHT_COMMAND_FILTER_RIGHT:
						case TRAFFICLIGHT_COMMAND_FILTER_MIDDLE:
						{
							// If we have been given this command by the traffic lights, then we are in the right hand-lane.
							// We need to adjust the vehicles nodes to ensure that it takes the right-hand turn.
							if(pVeh->GetIntelligence()->GetJunctionCommand() != iGoCommand ||
								pVeh->GetIntelligence()->GetJunctionFilter() == JUNCTION_FILTER_NONE)
							{
								//test for obstructions now even if we're in a templated junction
								if (!VehicleRouteClashesWithOtherCarOnJunction(c, entryNodes, exitNodes, dists, bGoingRightOnRed, iEntryLanes, iExitLanes, iEntrances, bRedLights))
								{
									pVeh->GetIntelligence()->SetJunctionCommand(iGoCommand);
#if __BANK
									if (CJunctions::m_bDebug)
									{
										grcDebugDraw::Sphere(VEC3V_TO_VECTOR3(pVeh->GetTransform().GetPosition()), 2.0f, Color_blue, true);
									}
#endif
								}
								else
								{
									pVeh->GetIntelligence()->SetJunctionCommand(iWaitForTrafficCommand);
								}

								switch(iCommand)
								{
								case TRAFFICLIGHT_COMMAND_FILTER_LEFT:
									pVeh->GetIntelligence()->SetJunctionFilter(JUNCTION_FILTER_LEFT);
									break;
								case TRAFFICLIGHT_COMMAND_FILTER_MIDDLE:
									pVeh->GetIntelligence()->SetJunctionFilter(JUNCTION_FILTER_MIDDLE);
									break;
								case TRAFFICLIGHT_COMMAND_FILTER_RIGHT:
									pVeh->GetIntelligence()->SetJunctionFilter(JUNCTION_FILTER_RIGHT);
									break;
								default: Assertf(false, "WTF?"); break;
								}
							}

							break;
						}
						default:
						{
							Assertf(false, "Case not handled!!");
							break;
						}
					}
				}
				else
				{
					pVeh->GetIntelligence()->SetJunctionCommand(iGoCommand);
#if __BANK
					if (CJunctions::m_bDebug)
					{
						grcDebugDraw::Sphere(VEC3V_TO_VECTOR3(pVeh->GetTransform().GetPosition()), 2.0f, Color_yellow, true);
					}
#endif
				}
			}
			else
			{
				pVeh->GetIntelligence()->SetJunctionCommand(JUNCTION_COMMAND_APPROACHING);
			}
		}
		else
		{
			pVeh->GetIntelligence()->SetJunctionCommand(iGoCommand);
#if __BANK
			if (CJunctions::m_bDebug)
			{
				grcDebugDraw::Sphere(VEC3V_TO_VECTOR3(pVeh->GetTransform().GetPosition()), 2.0f, Color_red, true);
			}
#endif
		}
	}
}

bool CJunction::CanSkipPedPhase()
{
	TUNE_GROUP_BOOL(JUNCTION, AllowSkipPedPhase, true);
	if(AllowSkipPedPhase && m_uNumWaitingForLightPeds == 0 && m_uNumActivelyCrossingPeds == 0 && GetCanSkipPedPhase())
	{
		for(int e = 0; e < m_iNumEntrances; e++)
		{
			// If any of them are active, then traffic is flowing
			if( m_Entrances[e].m_iPhase == m_iLightPhase )
			{
				return false;
			}
		}

 		return true;
	}

	return false;
}

ETrafficLightCommand CJunction::GetTrafficLightCommand(const CVehicle * pVehicle, const int iEntrance, bool& bGoingRightOnRedOut, const bool bAllowGoIfPastLine, const CVehicleNodeList* pNodeList, const bool bAllowReturnAmberLightCycle) const
{
	static const float fAmberTime = ((float)LIGHTDURATION_AMBER) / 1000.0f;

	bGoingRightOnRedOut = false;

	// If this vehicle has no entrance, then just go for it! (error?)
	// What has probably happened is that this vehicle was created inside the
	// junction, and thus has no history nodes from which to locate its entrance node..
	if(iEntrance==-1)
	{
		return TRAFFICLIGHT_COMMAND_GO;
	}

	if(m_iTemplateIndex != -1)
	{
		// Stop all traffic if train is approaching, or barriers are not fully raised
		// NB: What about vehicles which are already on this junction?
		if(ShouldCarsStopForTrain())
		{
			return TRAFFICLIGHT_COMMAND_STOP;
		}

		//const CVehicleNodeList*	pNodeList = pVehicle->GetIntelligence()->GetNodeList();
		if (!pNodeList)
		{
			pNodeList = pVehicle->GetIntelligence()->GetNodeList();
		}
		if (!pNodeList)
		{
			return TRAFFICLIGHT_COMMAND_GO;
		}
		//we can't assume iTargetNode is the junction here, so find the junction in our nodelist
		s32 iJunctionNodeIndex = pNodeList->GetTargetNodeIndex();
		for (int i = 1; i < CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED; i++)
		{
			if (!pNodeList->GetPathNodeAddr(i).IsEmpty() && ContainsJunctionNode(pNodeList->GetPathNodeAddr(i)))
			{
				iJunctionNodeIndex = i;
				break;
			}
		}
		
		const s32 iEntranceNode = iJunctionNodeIndex - 1;

		if(pNodeList && iEntranceNode >= 0 && !pNodeList->GetPathNodeAddr(iEntranceNode).IsEmpty() && !pNodeList->GetPathNodeAddr(iJunctionNodeIndex).IsEmpty())
		{
			const s32 iLink = pNodeList->GetPathLinkIndex(iEntranceNode);
			const CPathNodeLink * pLink = ThePaths.FindLinkPointerSafe(pNodeList->GetPathNodeAddr(iEntranceNode).GetRegion(), iLink);
			const CPathNode* pEntranceNode = pNodeList->GetPathNode(iEntranceNode);

			// Special cases for multi-lane junction entrances..
			// Left filter lanes (NB: there are also single-lane left filters, dealt with elsewhere)
			// Right filter lanes
			if(pLink)
			{
				const s32 iCurrLane = pNodeList->GetPathLaneIndex(iEntranceNode);
				const s32 iNumLanes = pLink->m_1.m_LanesToOtherNode;

				// If our entrance is a multi-lane road and has a left-filter lane,
				// and it's the correct lights phase for this filter lane lane..
				if(iCurrLane == 0 && iNumLanes > 1 && m_Entrances[iEntrance].m_iLeftFilterPhase != -1)
				{
					if(m_Entrances[iEntrance].m_iLeftFilterPhase == m_iLightPhase)
					{
						return (bAllowReturnAmberLightCycle && m_fLightTimeRemaining < fAmberTime) ?
							TRAFFICLIGHT_COMMAND_AMBERLIGHT : TRAFFICLIGHT_COMMAND_FILTER_LEFT;
					}
					else
					{
						return TRAFFICLIGHT_COMMAND_STOP;
					}
				}

				// If our entrance is a single-lane road, which is also a left-filter only lane
				// and it's also the correct lights phase
				if(iNumLanes == 1 && m_Entrances[iEntrance].m_iLeftFilterPhase == m_iLightPhase)
				{
					return (bAllowReturnAmberLightCycle && m_fLightTimeRemaining < fAmberTime) ?
						TRAFFICLIGHT_COMMAND_AMBERLIGHT : TRAFFICLIGHT_COMMAND_FILTER_LEFT;
				}
	
				// Some junctions entrances always allow cars to turn right on a red, regardless of light phase
				const s32 iRightHandLane = iNumLanes - 1;
				const bool bEntranceHasNoRightFlag = pEntranceNode && pEntranceNode->m_1.m_cannotGoRight;
				if(!bEntranceHasNoRightFlag && iNumLanes >= 1 && iCurrLane == iRightHandLane)
				{
					//if the right lane can only go right, force either a right turn or a stop
					const CPathNodeRouteSearchHelper* pRouteSearchHelper = pVehicle->GetIntelligence()->GetRouteSearchHelper();
					const bool bAvoidTurns = pRouteSearchHelper ? pRouteSearchHelper->ShouldAvoidTurns(): false;
					mthRandom rnd(pVehicle->GetRandomSeed() + m_iTemplateIndex); 
					const bool bWouldGoRight = (bool)((rnd.GetInt() >> 8) & 1);
					const bool bWantsToGoRight = bWouldGoRight && !bAvoidTurns; 
					if (bWantsToGoRight || m_Entrances[iEntrance].m_bRightLaneIsRightOnly)
					{
						if (m_Entrances[iEntrance].m_bCanTurnRightOnRedLight || m_Entrances[iEntrance].m_iPhase == m_iLightPhase)
						{
							bGoingRightOnRedOut = m_Entrances[iEntrance].m_iPhase != m_iLightPhase;
							return (bAllowReturnAmberLightCycle 
								&& m_Entrances[iEntrance].m_iPhase == m_iLightPhase 
								&& m_fLightTimeRemaining < fAmberTime) ?
								TRAFFICLIGHT_COMMAND_AMBERLIGHT : TRAFFICLIGHT_COMMAND_FILTER_RIGHT;
						}
						else
						{
							return TRAFFICLIGHT_COMMAND_STOP;
						}
					}
					else
					{
						//we want to go straight through
						if (m_Entrances[iEntrance].m_iPhase == m_iLightPhase)
						{
							return (bAllowReturnAmberLightCycle && m_fLightTimeRemaining < fAmberTime) ?
								TRAFFICLIGHT_COMMAND_AMBERLIGHT : TRAFFICLIGHT_COMMAND_FILTER_MIDDLE;
						}
						else
						{
							return TRAFFICLIGHT_COMMAND_STOP;
						}
					}
				}

				// We're in the middle lane approaching the junction. Stay in lane.
				if(iNumLanes >= 3 && iCurrLane > 0 && iCurrLane < iNumLanes-1)
				{
					if(m_Entrances[iEntrance].m_iPhase == m_iLightPhase)
					{
						return (bAllowReturnAmberLightCycle && m_fLightTimeRemaining < fAmberTime) ?
							TRAFFICLIGHT_COMMAND_AMBERLIGHT : TRAFFICLIGHT_COMMAND_FILTER_MIDDLE;
					}
				}
				if(iNumLanes >= 1 && iCurrLane == 0 && m_Entrances[iEntrance].m_bLeftLaneIsAheadOnly)
				{
					if(m_Entrances[iEntrance].m_iPhase == m_iLightPhase)
					{
						return (bAllowReturnAmberLightCycle && m_fLightTimeRemaining < fAmberTime) ?
							TRAFFICLIGHT_COMMAND_AMBERLIGHT : TRAFFICLIGHT_COMMAND_FILTER_MIDDLE;
					}
					else
					{
						return TRAFFICLIGHT_COMMAND_STOP;
					}
				}
			}
		}

		// If we got here, and our entrance is on the current phase - then we should go
		if(m_Entrances[iEntrance].m_iPhase == m_iLightPhase)
		{
			return (bAllowReturnAmberLightCycle && m_fLightTimeRemaining < fAmberTime) ?
				TRAFFICLIGHT_COMMAND_AMBERLIGHT : TRAFFICLIGHT_COMMAND_GO;
		}

		// If all the above checks have failed, then we stop
		return TRAFFICLIGHT_COMMAND_STOP;
	}

	// Fall through to original behaviour automatically-created junctions
	else
	{
		eTrafficLightColour lightCol = LIGHT_UNKNOWN;
		const bool bStop = CTrafficLights::ShouldCarStopForLightNode(pVehicle, m_Entrances[iEntrance].m_Node, &lightCol, bAllowGoIfPastLine);

		if(bAllowReturnAmberLightCycle && lightCol == LIGHT_AMBER)
		{
			return TRAFFICLIGHT_COMMAND_AMBERLIGHT;
		}
		else
		{
			return bStop ? TRAFFICLIGHT_COMMAND_STOP : TRAFFICLIGHT_COMMAND_GO;
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : VehicleRouteClashesWithOtherCarOnJunction
// PURPOSE :  Finds out what the entry and exit nodes are for this car for this junction.
//			  Then goes through the cars already green lighted on this junction and
//			  decides whether there is a clash. (If so this car should be prevented from
//			  entering the junction)
/////////////////////////////////////////////////////////////////////////////////

bool CJunction::VehicleRouteClashesWithOtherCarOnJunction(const s32 thisVehicleIndex, const CNodeAddress *pEntryNodes, const CNodeAddress *pExitNodes, const float * fDistances, const bool bGoingRightOnRed, s32* iEntryLanes, s32* iExitLanes, const s32* iEntrances, const bool* bRedLights)
{
	const s32 &iThisEntranceIndex = iEntrances[thisVehicleIndex];

	if (iThisEntranceIndex == -1 || (IsOnlyJunctionBecauseHasSwitchedOffEntrances() && !m_Entrances[iThisEntranceIndex].m_bIsSwitchedOff))
	{
		return false;
	}

	const CVehicle* pThisVeh = m_pVehicles[thisVehicleIndex];
	const CNodeAddress &thisEntryNode = pEntryNodes[thisVehicleIndex];
	const CNodeAddress &thisExitNode = pExitNodes[thisVehicleIndex];

	if (!pThisVeh || thisEntryNode.IsEmpty() || thisExitNode.IsEmpty())
	{
		return false;		// If any of the nodes are empty we can't be sure and let the car go through.
	}

	const CPathNode *pThisEntryNode = ThePaths.FindNodePointerSafe(thisEntryNode);
	const CPathNode *pThisExitNode = ThePaths.FindNodePointerSafe(thisExitNode);

	if (!pThisEntryNode || !pThisExitNode)
	{
		return false;
	}

	const s32 &iThisEntryLane = iEntryLanes[thisVehicleIndex];
	const s32 &iThisExitLane = iExitLanes[thisVehicleIndex];

	if (iThisEntryLane == -1 || iThisExitLane == -1)
	{
		return false;
	}

	CVehicleIntelligence* pThisIntelligence = pThisVeh->GetIntelligence();

#if __ASSERT
	//pThisIntelligence->VerifyCachedNodeList();
#endif // __ASSERT

	if (pThisIntelligence->GetJunctionCommand() == JUNCTION_COMMAND_GO)
	{
		return false;
	}

	const float &fThisDistance = fDistances[thisVehicleIndex];
	const u32 iThisArrivalTime = pThisIntelligence->GetJunctionArrivalTime();

	const Vector3 vThisVeh = VEC3V_TO_VECTOR3(pThisVeh->GetVehiclePosition());
	const Vector3 &vThisExit = pThisIntelligence->GetJunctionExitPosition();

	if (iThisEntranceIndex >= 0 && m_Entrances[iThisEntranceIndex].m_bIsGiveWay)
	{
		for (s32 i = 0; i < JUNCTION_MAX_VEHICLES; i++)
		{
			if (i != thisVehicleIndex)
			{
				const CVehicle * pOtherVeh = m_pVehicles[i];

				if (pOtherVeh)
				{
					const float &fOtherDistance = fDistances[i];

					if (fThisDistance > -5.0f && fOtherDistance > -5.0f)
					{
						const CVehicleIntelligence* pOtherIntelligence = pOtherVeh->GetIntelligence();
						const u32 iOtherArrivalTime = pOtherIntelligence->GetJunctionArrivalTime();

						if (pOtherIntelligence->GetJunction() == this && (pOtherIntelligence->GetJunctionCommand() == JUNCTION_COMMAND_GO || iThisArrivalTime > iOtherArrivalTime))
						{
#if __BANK
							if (CJunctions::m_bDebugWaitForTraffic)
							{
								const Vector3 vOtherVeh = VEC3V_TO_VECTOR3(pOtherVeh->GetVehiclePosition());
								grcDebugDraw::Line(vThisVeh, vOtherVeh, Color_green, Color_green);
								grcDebugDraw::Line(vThisVeh, vThisExit, Color_red, Color_red);
								grcDebugDraw::Sphere(vThisVeh, 2.2f, Color_green, false);
							}
#endif
							return true;
						}
					}
				}
			}
		}
	}

	Vector3 vToThisExit = vThisExit - vThisVeh;
	vToThisExit.z = 0.0f;
	vToThisExit.NormalizeSafe();

	const Vector3 vToThisExitRight(vToThisExit.y, -vToThisExit.x, 0.0f);

	const u32 iThisTurnDirection = pThisIntelligence->GetJunctionTurnDirection();

	bool bAllowAlternateExitLane = pThisVeh->PopTypeGet() == POPTYPE_RANDOM_AMBIENT && HasTrafficLightNodes();
	bool bClashes[3] = { false, false, false };
	bool bResetArrivalTime[3] = { false, false, false };

	for (s32 i = 0; i < JUNCTION_MAX_VEHICLES; i++)
	{
		if (i != thisVehicleIndex)
		{
			const CVehicle * pOtherVeh = m_pVehicles[i];

			if (pOtherVeh)
			{
				// Although vehicles are removed from junctions when they have no nodelist - its possible that
				// due to frame ordering, vehicles with no nodelist are still present here.
				CVehicleIntelligence* pOtherIntelligence = pOtherVeh->GetIntelligence();
				const CVehicleNodeList* pOtherNodeList = pOtherIntelligence->GetNodeList();

				if (!pOtherNodeList)
				{
					continue;
				}

				const CNodeAddress &otherEntryNode = pEntryNodes[i];
				const CNodeAddress &otherExitNode = pExitNodes[i];

				if (otherEntryNode.IsEmpty() || otherExitNode.IsEmpty())
				{
					continue;		// If any of the nodes are empty we can't be sure and let the car go through.
				}

				const CPathNode *pOtherEntryNode = ThePaths.FindNodePointerSafe(otherEntryNode);
				const CPathNode *pOtherExitNode = ThePaths.FindNodePointerSafe(otherExitNode);

				if (!pOtherEntryNode || !pOtherExitNode)
				{
					continue;
				}

				const s32 &iOtherEntryLane = iEntryLanes[i];
				const s32 &iOtherExitLane = iExitLanes[i];
				const s32 &iOtherEntranceIndex = iEntrances[i];

				if (iOtherEntryLane == -1 || iOtherExitLane == -1 || iOtherEntranceIndex == -1)
				{
					continue;
				}

				CVehicle *pBlockingVeh = pOtherIntelligence->m_pCarThatIsBlockingUs;

				if (pBlockingVeh == pThisVeh)
				{
					continue;
				}

				const bool bOtherPrimaryJunction = pOtherIntelligence->GetJunction() == this;

				const u32 iOtherArrivalTime = pOtherIntelligence->GetJunctionArrivalTime();
				const bool &bOtherRedLight = bRedLights[i];
				const bool &bOtherGiveWay = m_Entrances[iOtherEntranceIndex].m_bIsGiveWay;
				const float &fOtherDistance = fDistances[i];
				const Vector3 vOtherVeh = VEC3V_TO_VECTOR3(pOtherVeh->GetVehiclePosition());

				const bool bSameEntranceNode = otherEntryNode == thisEntryNode;

				if (!bSameEntranceNode && !bOtherRedLight && bOtherPrimaryJunction)
				{
					const u32 iOtherTurnDirection = pOtherIntelligence->GetJunctionTurnDirection();

					if (iThisTurnDirection == BIT_TURN_LEFT && iOtherTurnDirection == BIT_TURN_LEFT && pOtherIntelligence->GetJunctionCommand() == JUNCTION_COMMAND_GO)
					{
						return true;
					}

					bool bCheckForIntersect = iThisArrivalTime >= iOtherArrivalTime && pOtherIntelligence->GetJunctionCommand() == JUNCTION_COMMAND_GO;

					if (!bCheckForIntersect 
						&& iThisTurnDirection == BIT_TURN_LEFT 
						&& pThisIntelligence->GetJunctionCommand() == JUNCTION_COMMAND_WAIT_FOR_TRAFFIC 
						&& pOtherIntelligence->GetJunctionCommand() == JUNCTION_COMMAND_WAIT_FOR_TRAFFIC 
						&& !bOtherGiveWay 
						&& fwRandom::GetRandomNumberInRange(0, 3) > 0)
					{
						bCheckForIntersect = true;
#if __BANK
						if (CJunctions::m_bDebugWaitForTraffic)
						{
							grcDebugDraw::Sphere(vThisVeh, 2.1f, Color_black, false);
						}
#endif
					}

					if (bCheckForIntersect)
					{
						Vector3 vToOtherVeh = vOtherVeh - vThisVeh;
						vToOtherVeh.z = 0.0f;
						vToOtherVeh.NormalizeSafe();
						
						if (vToThisExit.Dot(vToOtherVeh) > 0.0f)
						{
							const Vector3 &vOtherExit = pOtherIntelligence->GetJunctionExitPosition();

							Vector3 vToOtherExit = vOtherExit - vThisVeh;
							vToOtherExit.z = 0.0f;
							vToOtherExit.NormalizeSafe();

							const float fOtherVehDot = vToThisExitRight.Dot(vToOtherVeh);
							const float fOtherExitDot = vToThisExitRight.Dot(vToOtherExit);

							if ((fOtherVehDot > 0.0f && fOtherExitDot < 0.0f) || (fOtherVehDot < 0.0f && fOtherExitDot > 0.0f) || fOtherExitDot == 0.0f)
							{
								if (iThisArrivalTime < iOtherArrivalTime)
								{
									pThisIntelligence->RecordJunctionArrivalTime();
								}
#if __BANK
								if (CJunctions::m_bDebugWaitForTraffic)
								{
									grcDebugDraw::Line(vThisVeh, vOtherVeh, Color_white, Color_white);
									grcDebugDraw::Line(vThisVeh, vThisExit, Color_red, Color_red);
									grcDebugDraw::Sphere(vThisVeh, 2.2f, Color_white, false);
								}
#endif
								return true;
							}
						}
					}
				}

				if (m_Entrances[iThisEntranceIndex].m_bIgnoreBackedUpExitsOnStraight && iThisTurnDirection == BIT_TURN_STRAIGHT_ON)
				{
#if __BANK
					if (CJunctions::m_bDebugWaitForTraffic)
					{
						grcDebugDraw::Sphere(vThisVeh, 2.1f, Color_orange, false);
					}
#endif
					continue;
				}

				const bool bSameEntrance = bSameEntranceNode && iOtherEntryLane == iThisEntryLane;
				const bool bSameExitNode = otherExitNode == thisExitNode;

				if (iThisTurnDirection == BIT_TURN_LEFT && bSameEntrance && !bSameExitNode && bOtherPrimaryJunction && fOtherDistance < fThisDistance)
				{
					bClashes[1] = true;
					bAllowAlternateExitLane = false;
					bResetArrivalTime[1] = true;

#if __BANK
					if (CJunctions::m_bDebugWaitForTraffic)
					{
						grcDebugDraw::Sphere(vThisVeh, 2.1f, Color_purple, false);
						grcDebugDraw::Line(vThisVeh, vOtherVeh, Color_magenta, Color_magenta);
					}
#endif

					continue;
				}

				s32 iExitLaneOffset = iOtherExitLane - iThisExitLane;
				u32 iClashIndex = iExitLaneOffset + 1;
				
				if (!bSameExitNode || Abs(iExitLaneOffset) > 1)
				{
					// If the other vehicle isn't heading toward an exit lane that we might consider, continue
					continue;
				}

				const float fOtherSpeedSquared = pOtherVeh->GetVelocity().Mag2();
				const bool bOtherGoingSlow = fOtherSpeedSquared < 9.0f;
				bool bOtherSlowingDownForCar = false;
				const bool bOtherSlowingDownForPed = pOtherIntelligence->GetSlowingDownForPed();

				u32 iDepth = 1;

				while (pBlockingVeh && pBlockingVeh != pThisVeh && iDepth <= 5)
				{
					CVehicleIntelligence *pBlockingIntelligence = pBlockingVeh->GetIntelligence();

					if (pBlockingIntelligence->GetJunction() != this && pBlockingIntelligence->GetPreviousJunction() != this)
					{
						break;
					}

					const float fBlockingSpeedSquared = pBlockingVeh->GetVelocity().Mag2();

					if (fBlockingSpeedSquared < 100.0f)
					{
						bOtherSlowingDownForCar = true;
						break;
					}

					pBlockingVeh = pBlockingIntelligence->m_pCarThatIsBlockingUs;
					++iDepth;
				}

				const bool bOtherStopped = bOtherGoingSlow | bOtherSlowingDownForCar | bOtherSlowingDownForPed;

#if __BANK
				Color32 debugOtherStoppedColor = Color_white;

				if (bOtherStopped && CJunctions::m_bDebugWaitForTraffic)
				{
					if (bOtherSlowingDownForCar)
					{
						debugOtherStoppedColor = Color_green;
					}
					else if (bOtherSlowingDownForPed)
					{
						debugOtherStoppedColor = Color_cyan;
					}
					else if (bOtherGoingSlow)
					{
						debugOtherStoppedColor = Color_red;
					}
				}
#endif

				// If we are turning right on red, and coming from the same entrance as another vehicle
				if (bGoingRightOnRed && bSameEntrance)
				{
					// That is in front of us
					if (fOtherDistance < fThisDistance)
					{
						// Wait
						bClashes[iClashIndex] = true;
						bResetArrivalTime[iClashIndex] |= bOtherStopped;
#if __BANK
						if (CJunctions::m_bDebugWaitForTraffic)
						{
							grcDebugDraw::Sphere(vThisVeh, 2.2f, Color_yellow, false);
							grcDebugDraw::Line(vThisVeh, vOtherVeh, Color_magenta, Color_magenta);
						}
#endif
					}

					continue;
				}

				if (bSameEntrance)
				{
					if (fOtherDistance < fThisDistance && bOtherStopped && fOtherDistance > -10.0f)
					{
						bClashes[iClashIndex] = true;
						bResetArrivalTime[iClashIndex] |= bOtherStopped;
#if __BANK
						if (CJunctions::m_bDebugWaitForTraffic)
						{
							grcDebugDraw::Sphere(vThisVeh, 2.2f, Color_red, false);
							grcDebugDraw::Line(vThisVeh, vOtherVeh, Color_magenta, Color_magenta);
							grcDebugDraw::Sphere(vOtherVeh + Vector3(0.0f, 0.0f, 5.0f), 0.5f, debugOtherStoppedColor);
						}
#endif
					}

					continue;
				}
				else
				{
					const bool bOtherPassingThroughJunction = (!bOtherPrimaryJunction && bOtherStopped) || (bOtherPrimaryJunction && pOtherIntelligence->GetJunctionCommand() == JUNCTION_COMMAND_GO);
					const bool bOtherReadyToPassThroughJunction = iOtherArrivalTime != UINT_MAX && !bOtherRedLight;

					if (bGoingRightOnRed && (bOtherPassingThroughJunction || bOtherReadyToPassThroughJunction))
					{
						bAllowAlternateExitLane = false;
					}

					if (bOtherPassingThroughJunction)
					{
						bClashes[iClashIndex] = true;
						bResetArrivalTime[iClashIndex] |= bOtherStopped;
#if __BANK
						if (CJunctions::m_bDebugWaitForTraffic)
						{
							grcDebugDraw::Sphere(vThisVeh, 2.2f, Color_cyan, false);
							grcDebugDraw::Line(vThisVeh, vOtherVeh, Color_magenta, Color_magenta);

							if (bOtherStopped)
							{
								grcDebugDraw::Sphere(vOtherVeh + Vector3(0.0f, 0.0f, 5.0f), 0.5f, debugOtherStoppedColor);
							}
						}
#endif
					}
					else if (bOtherReadyToPassThroughJunction && iExitLaneOffset != 0)
					{
						bClashes[iClashIndex] = true;
#if __BANK
						if (CJunctions::m_bDebugWaitForTraffic)
						{
							grcDebugDraw::Sphere(vThisVeh, 2.2f, Color_pink, false);
							grcDebugDraw::Line(vThisVeh, vOtherVeh, Color_magenta, Color_magenta);
						}
#endif
					}

					continue;
				}
			}
		}
	}

	u32 iClashIndex = 1;

	if (bAllowAlternateExitLane && bClashes[iClashIndex])
	{
		CVehicleNodeList* pNodeList = pThisIntelligence->GetNodeList();

		if (pNodeList)
		{
			const s32 iExitIndex = pNodeList->FindNodeIndex(thisExitNode);

			if (iExitIndex >= 0)
			{
				// If the route point immediately following our exit has a required lane, disallow alternate lane exits
				if (iExitIndex + 1 < CVehicleFollowRouteHelper::MAX_ROUTE_SIZE)
				{
					const CVehicleFollowRouteHelper* pFollowRouteHelper = pThisIntelligence->GetFollowRouteHelper();

					if (pFollowRouteHelper)
					{
						const CRoutePoint& nextRoutePoint = pFollowRouteHelper->GetRoutePoints()[iExitIndex + 1];

						if (!nextRoutePoint.GetNodeAddress().IsEmpty() && nextRoutePoint.m_iRequiredLaneIndex != -1)
						{
							bAllowAlternateExitLane = false;
#if __BANK
							if (CJunctions::m_bDebugWaitForTraffic)
							{
								grcDebugDraw::Sphere(vThisVeh + Vector3(0.0f, 0.0f, 5.0f), 1.0f, Color_red, false);
							}
#endif
						}
					}
				}

				if (bAllowAlternateExitLane)
				{
					const s16 iLinkIndex = pNodeList->GetPathLinkIndex(iExitIndex);

					if (iLinkIndex >= 0)
					{
						Assert(ThePaths.IsRegionLoaded(thisExitNode));

						const u32 iRegionIndex = thisExitNode.GetRegion();

						CPathNodeLink *pLink = ThePaths.FindLinkPointerSafe(iRegionIndex, iLinkIndex);

						if (pLink)
						{
							s32 iNewExitLaneOffset = 0;

							if (!bClashes[0] && iThisExitLane == 0)
							{
								bClashes[0] = true;
							}

							if (!bClashes[2] && iThisExitLane == static_cast<s32>(pLink->m_1.m_LanesToOtherNode) - 1)
							{
								bClashes[2] = true;
							}

							if (!bClashes[0] && !bClashes[2])
							{
								iNewExitLaneOffset = fwRandom::GetRandomTrueFalse() ? -1 : 1;
							}
							else if (!bClashes[0])
							{
								iNewExitLaneOffset = -1;
							}
							else if (!bClashes[2])
							{
								iNewExitLaneOffset = 1;
							}

							if (iNewExitLaneOffset != 0)
							{
								s8 iNewExitLane = static_cast<s8>(iThisExitLane + iNewExitLaneOffset);

								CTaskVehicleMissionBase *pTask = pThisIntelligence->GetActiveTask();

								if (pTask && pTask->GetTaskType() == CTaskTypes::TASK_VEHICLE_CRUISE_NEW)
								{
									pNodeList->SetPathLaneIndex(iExitIndex, iNewExitLane);
									iExitLanes[thisVehicleIndex] = iNewExitLane;

									static_cast<CTaskVehicleCruiseNew*>(pTask)->ConstructDefaultFollowRouteFromNodeList();

									iClashIndex = 1 + iNewExitLaneOffset;
#if __BANK
									if (CJunctions::m_bDebugWaitForTraffic)
									{
										grcDebugDraw::Sphere(vThisVeh + Vector3(0.0f, 0.0f, 5.0f), 1.0f, Color_green, false);
									}
#endif
								}
							}
						}
					}
				}
			}
		}
	}

	if (bClashes[iClashIndex])
	{
		if (bResetArrivalTime[iClashIndex])
		{
			pThisIntelligence->RecordJunctionArrivalTime();
		}
#if __BANK
		if (CJunctions::m_bDebugWaitForTraffic)
		{
			grcDebugDraw::Sphere(vThisVeh, 2.3f, Color_magenta, false);
		}
#endif
	}

	return bClashes[iClashIndex];
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindEntryAndExitNodes
// PURPOSE :  For this particular vehicle on this particular junction we identify
//			  the node going into and coming out of this junction.
/////////////////////////////////////////////////////////////////////////////////

bool CJunction::FindEntryAndExitNodes(const CVehicle* const pVeh, CNodeAddress &PrevNode, CNodeAddress &NextNode, s32& iPrevLane, s32& iNextLane) const
{
	CVehicleNodeList* pNodeList = pVeh->GetIntelligence()->GetNodeList();
	
	if (!pNodeList)
	{
		return false;
	}

	PrevNode.SetEmpty();
	NextNode.SetEmpty();
	iPrevLane = -1;
	iNextLane = -1;

	for (s32 i = 0; i < CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED; ++i)
	{
		for (s32 j = 0; j < m_iNumJunctionNodes; ++j)
		{
			if (pNodeList->GetPathNodeAddr(i) == m_JunctionNodes[j])
			{
				for (s32 k = i - 1; k >= 0; --k)
				{
					if (FindEntranceIndexWithNode(pNodeList->GetPathNodeAddr(k)) != -1)
					{
						PrevNode = pNodeList->GetPathNodeAddr(k);
						iPrevLane = pNodeList->GetPathLaneIndex(k);
						break;
					}
				}

				for (s32 k = i + 1; k < CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED; ++k)
				{
					if (FindEntranceIndexWithNode(pNodeList->GetPathNodeAddr(k)) != -1)
					{
						NextNode = pNodeList->GetPathNodeAddr(k);
						iNextLane = pNodeList->GetPathLaneIndex(k);
						break;
					}
				}

				return true;
			}
		}
	}

	return false;
}

//find the entry and exit direction of the junction, taking into account pre- and post-links if the
//interior junction links are ignore nav
bool CJunction::FindEntryAndExitDirs(const CVehicle* const pVeh, Vector2& vEntryDir, Vector2& vExitDir) const
{
	CVehicleNodeList * pNodeList = pVeh->GetIntelligence()->GetNodeList();
	if(!pNodeList)
		return false;

	CNodeAddress PrevNode, NextNode;

	s32 iJunctionIndex = -1;

	for (s32 n=0; n<CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED; n++)
	{
		if (iJunctionIndex >= 0)
		{
			break;
		}
		for(s32 i=0; i<m_iNumJunctionNodes; i++)
		{
			if(pNodeList->GetPathNodeAddr(n) == m_JunctionNodes[i])
			{
				iJunctionIndex = n;
				break;
			}
		}
	}

	//need at least an entrance node
	if (iJunctionIndex <= 0 || iJunctionIndex >= CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED-1)
	{
		return false;
	}

	const CPathNode* pEntryNode = pNodeList->GetPathNode(iJunctionIndex-1);
	if (!pEntryNode)
	{
		return false;
	}
	const CPathNode* pJunctionNode = pNodeList->GetPathNode(iJunctionIndex);
	if (!pJunctionNode)
	{
		return false;
	}

	const CPathNode* pExitNode = pNodeList->GetPathNode(iJunctionIndex+1);
	if (!pExitNode)
	{
		return false;
	}

	Vector2 vEntryStart, vEntryEnd;
	pEntryNode->GetCoors2(vEntryStart);
	pJunctionNode->GetCoors2(vEntryEnd);

	const CPathNodeLink& rEntryLink = *ThePaths.FindLinkPointerSafe(pEntryNode->GetAddrRegion(),pNodeList->GetPathLinkIndex(iJunctionIndex-1));
	if (iJunctionIndex > 1 && rEntryLink.IsDontUseForNavigation())
	{
		const CPathNode* pPreEntryNode = pNodeList->GetPathNode(iJunctionIndex-2);
		if (pPreEntryNode)
		{
			//entry link is a nonav and we've got a previous node in our list.
			//reset entry start and end nodes
			//luckily we've already got the end node
			pEntryNode->GetCoors2(vEntryEnd);

			pPreEntryNode->GetCoors2(vEntryStart);
		}
	}

	Vector2 vExitStart, vExitEnd;
	pJunctionNode->GetCoors2(vExitStart);
	pExitNode->GetCoors2(vExitEnd);

	const CPathNodeLink& rExitLink = *ThePaths.FindLinkPointerSafe(pJunctionNode->GetAddrRegion(),pNodeList->GetPathLinkIndex(iJunctionIndex));
	if (iJunctionIndex < CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED-2 && rExitLink.IsDontUseForNavigation())
	{
		const CPathNode* pPostExitNode = pNodeList->GetPathNode(iJunctionIndex + 2);
		if (pPostExitNode)
		{
			pExitNode->GetCoors2(vExitStart);
			pPostExitNode->GetCoors2(vExitEnd);
		}
	}

	vEntryDir = vEntryEnd - vEntryStart;
	vEntryDir.Normalize();
	vExitDir = vExitEnd - vExitStart;
	vExitDir.Normalize();

	return true;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindNodeIndexWithNode
// PURPOSE :  Given the entry node we find the index of this node within the junction.
/////////////////////////////////////////////////////////////////////////////////

s32	CJunction::FindEntranceIndexWithNode(CNodeAddress entryNode) const
{
	for (s32 c = 0; c < m_iNumEntrances; c++)
	{
		if (entryNode == m_Entrances[c].m_Node)
		{
			return c;
		}
	}
//	Assertf(0, "Couldn't find node in junction entry node list");
	return -1;
}

float CJunction::HelperCalcDistanceToEntrance(const Vector3& vPos, s32 iEntranceIndex)
{
	const Vector2 vDir = m_Entrances[iEntranceIndex].m_vEntryDir;
	const float d = vDir.Dot( Vector2(m_Entrances[iEntranceIndex].m_vPositionOfNode, Vector2::kXY) );

	float dist = vDir.Dot( Vector2(vPos, Vector2::kXY) );
	dist -= d;

	if (m_iTemplateIndex != -1)
	{
		dist -= m_Entrances[iEntranceIndex].m_EntryStopDistance;
	}

	return dist;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : CalculateDistanceToJunction
// PURPOSE :  Calculates the distance of this car to the junction.
/////////////////////////////////////////////////////////////////////////////////

bool CJunction::CalculateDistanceToJunction(const CVehicle* const pVeh, s32 * iEntranceIndex, float & dist)
{
	const CVehicleNodeList* pNodeList = pVeh->GetIntelligence()->GetNodeList();

	if (!pNodeList)
	{
		return false;
	}

	Vector3 vVehPosition = VEC3V_TO_VECTOR3(CVehicleFollowRouteHelper::GetVehicleBonnetPosition(pVeh, false));

	const CNodeAddress& entranceAddress = pVeh->GetIntelligence()->GetJunctionEntranceNode();

	if (!entranceAddress.IsEmpty())
	{
		const CNodeAddress& exitAddress = pVeh->GetIntelligence()->GetJunctionExitNode();
		const s32 currentIndex = pNodeList->GetTargetNodeIndex();

		if (pNodeList->FindNodeIndex(entranceAddress) >= currentIndex || pNodeList->FindNodeIndex(exitAddress) >= currentIndex)
		{
			*iEntranceIndex = FindEntranceIndexWithNode(entranceAddress);

			if (*iEntranceIndex != -1)
			{
				dist = HelperCalcDistanceToEntrance(vVehPosition, *iEntranceIndex);
				return true;
			}
		}
	}

	return false;
}

float	CJunction::GetTimeToJunction(const CVehicle* const pVeh, const float fDist) const
{
	if (!pVeh)
	{
		return FLT_MAX;
	}

	//get the vehicle's velocity toward the junction
	Vector3 vVehToJunction = GetJunctionCenter() - VEC3V_TO_VECTOR3(pVeh->GetVehiclePosition());
	vVehToJunction.NormalizeFast();

	Vector3 vVelocity = pVeh->GetVelocity();
	const float fSpeedInDirection = vVelocity.Dot(vVehToJunction);

	//if we're already there, we're already there
	if (fDist <= 0.0f)
	{
		return 0.0f;
	}

	//if we're not already there but moving away, return a really high value so nobody yields
	if (fSpeedInDirection < SMALL_FLOAT)
	{
		return FLT_MAX;
	}

	const float fTime = fDist / fSpeedInDirection;

	return fTime;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : RemoveVehicleFromSlot
// PURPOSE :  Removes a specific vehicle from this junction.
/////////////////////////////////////////////////////////////////////////////////

void	CJunction::RemoveVehicleFromSlot(s32 slot)
{
	m_pVehicles[slot] = NULL;

		// Make sure there are no gaps in vehicle list.
	if (slot == m_HighestVehicleArrayElementUsed)
	{
		m_HighestVehicleArrayElementUsed--;
	}
	else
	{
		TidyJunction();
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : RemoveVehicle
// PURPOSE :  Removes a specific vehicle from this junction.
/////////////////////////////////////////////////////////////////////////////////

void	CJunction::RemoveVehicle(CVehicle *pVeh)
{
	for (s32 i=0; i<JUNCTION_MAX_VEHICLES; i++)
	{
		if (m_pVehicles[i] == pVeh)
		{
			RemoveVehicleFromSlot(i);
			return;
		}
	}
	//Assertf(0, "Vehicle is being removed from a junction it wasn't in to begin with\n");
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : AddVehicle
// PURPOSE :  Adds a specific vehicle to this junction.
/////////////////////////////////////////////////////////////////////////////////

void	CJunction::AddVehicle(CVehicle *pVeh)
{
	Assertf(pVeh->GetStatus() != STATUS_WRECKED, "Adding a wrecked vehicle to a junction!");
	if(pVeh->GetStatus() == STATUS_WRECKED)
		return;

	for(s32 s=0; s<=m_HighestVehicleArrayElementUsed; s++)
	{
		if(m_pVehicles[s] == pVeh)
		{
			aiWarningf("CJunction::AddVehicle() - trying to add a vehicle which is already a member of this junction");
			return;
		}
	}

	if(GetIsMalfunctioning())
		return;

	for (s32 i=0; i<JUNCTION_MAX_VEHICLES; i++)
	{
		if (!m_pVehicles[i])
		{
			m_pVehicles[i] = pVeh;
			pVeh->GetIntelligence()->SetJunctionCommand(JUNCTION_COMMAND_APPROACHING);
			m_HighestVehicleArrayElementUsed = rage::Max(m_HighestVehicleArrayElementUsed, i);

			// Set the frames since update to the maximum for this junction - this will
			// ensure that it gets updated at the next available opportunity.
			const int junctionIndex = ptrdiff_t_to_int(this - &CJunctions::m_aJunctions[0]);
			Assert(junctionIndex >= 0 && junctionIndex < JUNCTIONS_MAX_NUMBER);
			CJunctions::m_TimesliceFramesSinceUpdate[junctionIndex] = 0xff;

			return;
		}
	}
	Assertf(0, "Vehicle is being added to a junction that is full\n");
}

bool CJunction::IsVehicleAddedToJunction(const CVehicle *pVeh) const
{
	for (u32 i = 0; i < JUNCTION_MAX_VEHICLES; ++i)
	{
		if (m_pVehicles[i] == pVeh)
		{
			return true;
		}
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : TidyJunction
// PURPOSE :  Removes the gaps in the junction.
//			  Vehicles may have gotten removed and would have left a gap.
//			  Fill in these gaps so that the junction is nice and tidy again.
/////////////////////////////////////////////////////////////////////////////////

void	CJunction::TidyJunction()
{
	s32 i;
	for (i=0; i<=m_HighestVehicleArrayElementUsed; i++)
	{
		if (!m_pVehicles[i])
		{
			// We found an empty slot. Find the highest entry at the moment to put in here.
			s32	highestOccupiedSlot = JUNCTION_MAX_VEHICLES-1;
			while (!m_pVehicles[highestOccupiedSlot] && highestOccupiedSlot > 0)
			{
				highestOccupiedSlot--;
			}

			if (highestOccupiedSlot > i)
			{
				// Fill in our empty spot.
				m_pVehicles[i] = m_pVehicles[highestOccupiedSlot];

				// Clear up our highest slot
				m_pVehicles[highestOccupiedSlot] = NULL;

				m_HighestVehicleArrayElementUsed = highestOccupiedSlot-1;
			}
			else
			{
				m_HighestVehicleArrayElementUsed = i-1;
				break;
			}
		}
	}

#if __ASSERT
	for (; i < JUNCTION_MAX_VEHICLES; i++)
	{
		Assert(!m_pVehicles[i]);
	}
#endif

}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : AddTrafficLight
// PURPOSE :  Adds a specific traffic light to this junction.
/////////////////////////////////////////////////////////////////////////////////

int	CJunction::AddTrafficLight(CEntity *pTL)
{
	int idx = -1;
	for (s32 i=0; i<JUNCTION_MAX_TRAFFICLIGHTS; i++)
	{
		if (!m_TrafficLights[i])
		{
			idx = i;
		}
		
		if( m_TrafficLights[i] == pTL)
		{
			return i;
		}
	}
	
	if( idx != -1 )
	{
		m_TrafficLights[idx] = pTL;
	}
	
	Assertf(idx != -1, "traffic light is being added to a junction that is full\n");
	return idx;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : AddTrafficLight
// PURPOSE :  Replace a specific traffic light in this junction.
/////////////////////////////////////////////////////////////////////////////////

void CJunction::SetTrafficLight(int idx, CEntity *pTL)
{
	//Assert(idx < JUNCTION_MAX_TRAFFICLIGHTS);
	if (!AssertVerify(idx < JUNCTION_MAX_TRAFFICLIGHTS && idx >= 0))
	{
		return;
	}

	m_TrafficLights[idx] = pTL;
}

dev_float FTLdotPMax = -0.8f;
#define MAX_TEMP_ENTRANCES 4

int	CJunction::FindTrafficLightEntranceIds(const Vector3 &vDir, bool isSingleLight, atRangeArray<int,4> &entranceIds)
{
	int entranceCount = 0;
	bool simpleJunction = GetNumEntrances() == 2;

	// hard coding junction some insane junctions...
	// Sorry about this... these junctions are locked down for GTAV though... and seriously just look at them
	// there's no way to make these work... -jkinz
	if( m_iTemplateIndex == 77 )
	{
		entranceCount = ConnectLightToJunction77(vDir,entranceIds);
	}
	else if( m_iTemplateIndex == 109 )
	{
		entranceCount = ConnectLightToJunction109(vDir,isSingleLight,entranceIds);
	}

	// these vars are used in the case that we can't find an entrance for a light
	atRangeArray<int,MAX_TEMP_ENTRANCES> tempEntranceIds;
	float lowestDotProduct = 1.0f;
	int tempEntranceCount = 0;

	// if the entrance count is 0 here, this is not a hardcoded entrance so go ahead and try and find the entrances
	if(entranceCount == 0)
	{
		for(int i=0;i<GetNumEntrances();i++)
		{
			const CJunctionEntrance &entrance = GetEntrance(i);
			const CPathNode * entranceNode = ThePaths.FindNodePointerSafe(entrance.m_Node);
			if( entranceNode && entranceNode->IsTrafficLight() ) 
			{ // Ignore all non traffic light flags.
				Vector3 pos = entranceNode->GetPos();
				Vector3 avgDirection( 0.0f, 0.0f, 0.0f );
			
				// calculate the average direction of all links coming from this entrance node
				for(s32 l=0; l<entranceNode->NumLinks(); l++)
				{
					const CPathNodeLink & link = ThePaths.GetNodesLink(entranceNode, l);
				
					if(link.m_1.m_bDontUseForNavigation)
						continue; // Ignore
				
					if( simpleJunction == false && ContainsJunctionNode(link.m_OtherNode) )
						continue; // Ignore link going to junction.
					
					const CPathNode * nextNode = ThePaths.FindNodePointerSafe(link.m_OtherNode);
					if(nextNode)
					{
						Vector3 direction = nextNode->GetPos() - pos;
						direction.NormalizeFast();

						avgDirection += direction;
					}
				}
	
				avgDirection.Normalize();
				float dotP = DotProduct(avgDirection, vDir);

	#if JUNCTIONTL_DEBUG
				grcDebugDraw::Cross(RCC_VEC3V(pos),0.25f,Color32(0xff,0x00,0xff));
				Vector3 posDir = pos + avgDirection;
				grcDebugDraw::Arrow(RCC_VEC3V(pos),RCC_VEC3V(posDir),0.2f,Color32(0xff,0x00,0xff));
	#endif // JUNCTIONTL_DEBUG
					
				if( dotP < FTLdotPMax && entranceCount < entranceIds.GetMaxCount() )
				{
	#if JUNCTIONTL_DEBUG
					grcDebugDraw::Cross(RCC_VEC3V(pos),0.25f,Color32(0x00,0xff,0x00));
	#endif // JUNCTIONTL_DEBUG

					// it's entirely possible that there be a crazy junction where multiple entrances
					// can get inside this threshold.  We don't want that, so lets make sure we only
					// take the lowest possible one.
					if( dotP < lowestDotProduct && lowestDotProduct > FTLdotPMax )
					{
						// if the dot product is lower, that means this is even more of an "opposite" entrance
						// than what we had stored beforehand, so lets whipe out all the old entrances and start again
						entranceCount = 0;
						lowestDotProduct = dotP;
						memset(&entranceIds[0], 0, sizeof(int) * MAX_TEMP_ENTRANCES);
					}

					entranceIds[entranceCount++] = i;
				}
				else if( entranceCount == 0 )
				{
					// if there haven't been any entrances found yet, start storing up the data
					// for the "best fit" entrances if, in the end, we don't find any
					if( IsClose( dotP, lowestDotProduct, FLT_EPSILON ) )
					{
						tempEntranceIds[tempEntranceCount++] = i;
					}
					else if( dotP < lowestDotProduct )
					{
						// if the dot product is lower, that means this is even more of an "opposite" entrance
						// than what we had stored beforehand, so lets whipe out all the old entrances and start again
						tempEntranceCount = 0;
						lowestDotProduct = dotP;
						memset(&tempEntranceIds[0], 0, sizeof(int) * MAX_TEMP_ENTRANCES);

						tempEntranceIds[tempEntranceCount++] = i;
					}
				}
			}
		}

		// if we found no entrances, lets takea look at the closest matches for consideration...
		if( entranceCount == 0 && tempEntranceCount != 0 )
		{
			entranceCount = tempEntranceCount;

			for( int i = 0; i < tempEntranceCount; i++ )
			{
				entranceIds[i] = tempEntranceIds[i];
			}
		}
	}

	if( entranceCount )
	{
		// We deal from the inside entrance to the outside one, so first is the filterleft, then the rest, unless we're looking at a one light setup.
		for(int i=0;i<entranceCount;i++)
		{
			const CJunctionEntrance &entranceI = GetEntrance(entranceIds[i]);
			const bool isFilterLeftLaneI = (entranceI.m_iLeftFilterPhase != -1);
			for(int j=i+1;j<entranceCount;j++)
			{
				const CJunctionEntrance &entranceJ = GetEntrance(entranceIds[j]);
				const bool isFilterLeftLaneJ = (entranceJ.m_iLeftFilterPhase != -1);
				
				// It does make sense, promise.
				const bool greaterThan = !isSingleLight && ( false == isFilterLeftLaneI && true == isFilterLeftLaneJ );
				const bool lessThan = isSingleLight && ( true == isFilterLeftLaneI && false == isFilterLeftLaneJ );
				
				if( lessThan || greaterThan )
				{
					int tmp = entranceIds[i];
					entranceIds[i] = entranceIds[j];
					entranceIds[j] = tmp;
				}
			}
		}
		
#if JUNCTIONTL_DEBUG
		for(int i=0;i<entranceCount;i++)
		{
			const CJunctionEntrance &entrance = GetEntrance(entranceIds[i]);
			CPathNode * entranceNode = ThePaths.FindNodePointerSafe(entrance.m_Node);
			Vector3 pos = entranceNode->GetPos();
			char msg[32];
			sprintf(msg,"%d",i);
			grcDebugDraw::Text(pos,Color32(0xFFFFFFFF),msg);
		}		
#endif // JUNCTIONTL_DEBUG		
	}
	
	return entranceCount;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : ConnectLightToJunction77
// PURPOSE :  Hard code some of the connections to entrances for junction 77
/////////////////////////////////////////////////////////////////////////////////
int	CJunction::ConnectLightToJunction77(const Vector3 &vDir,atRangeArray<int,4> &entranceIds)
{
	memset(&entranceIds[0], 0, sizeof(int) * MAX_TEMP_ENTRANCES);

	if( vDir.IsEqual(Vector3(-0.77038019f, -0.63758480f, 0.0f)) )
	{
		entranceIds[0] = 6;
		entranceIds[1] = 5;
		return 2;
	}
	else if( vDir.IsEqual(Vector3(-0.66093146f, -0.75044632f, 0.0f)))
	{
		entranceIds[0] = 7;
		return 1;
	}
	else if( vDir.IsEqual(Vector3(0.66667127f, 0.74535191f, 0.0f)))
	{
		entranceIds[0] = 1;
		entranceIds[1] = 2;
		return 2;
	}

	return 0;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : ConnectLightToJunction109
// PURPOSE :  Hard code some of the connections to entrances for junction 109
/////////////////////////////////////////////////////////////////////////////////
int	CJunction::ConnectLightToJunction109(const Vector3 &vDir, bool isSingleLight, atRangeArray<int,4> &entranceIds)
{
	memset(&entranceIds[0], 0, sizeof(int) * MAX_TEMP_ENTRANCES);

	if( vDir.IsEqual(Vector3(0.10591994f, 0.99437469f, 0.0f)) )
	{
		entranceIds[0] = 5;
		return 1;
	}
	else if( isSingleLight )
	{
		entranceIds[0] = 4;
		return 1;
	}

	return 0;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : RemoveTrafficLights
// PURPOSE :  Remove any remaining links to trafficlights.
/////////////////////////////////////////////////////////////////////////////////
void CJunction::RemoveTrafficLights()
{
	for(s32 i=0; i<JUNCTION_MAX_TRAFFICLIGHTS; i++)
	{
		if( m_TrafficLights[i] )
		{
			CTrafficLights::RemoveTrafficLightInfo(m_TrafficLights[i]);
			m_TrafficLights[i] = NULL;
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : RemoveTrafficLight
// PURPOSE :  Remove the link to the specified traffic light
/////////////////////////////////////////////////////////////////////////////////
void CJunction::RemoveTrafficLight(CEntity *pTL)
{
	for(s32 i=0; i<JUNCTION_MAX_TRAFFICLIGHTS; i++)
	{
		if( m_TrafficLights[i] == pTL )
		{
			m_TrafficLights[i] = NULL;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////
// FUNCTION : GetTrafficLightSearchDistance
// PURPOSE :  Get the radius for the traffic light search.
////////////////////////////////////////////////////////////////////////////////////
float CJunction::GetTrafficLightSearchDistance()
{
	if(m_iTemplateIndex == -1)
		return 35.0f;
		
	CJunctionTemplate & temp = CJunctions::GetJunctionTemplate(m_iTemplateIndex);
	return temp.m_fSearchDistance == 0.0f ? 35.0f : temp.m_fSearchDistance;
}

////////////////////////////////////////////////////////////////////////////////////
// FUNCTION : GetNumTrafficLightLocations
// PURPOSE :  Get number of traffic light locations, if any
////////////////////////////////////////////////////////////////////////////////////
s32 CJunction::GetNumTrafficLightLocations()
{
	if(m_iTemplateIndex == -1)
		return 0;
	CJunctionTemplate & temp = CJunctions::GetJunctionTemplate(m_iTemplateIndex);
	return temp.m_iNumTrafficLightLocations;
}

////////////////////////////////////////////////////////////////////////////////////
// FUNCTION : GetTrafficLightLocation
// PURPOSE :  Get specified traffic light location
////////////////////////////////////////////////////////////////////////////////////
void CJunction::GetTrafficLightLocation(s32 iIndex, Vector3 & vPosition)
{
	Assert(m_iTemplateIndex != -1);
	CJunctionTemplate & temp = CJunctions::GetJunctionTemplate(m_iTemplateIndex);

	Assert(iIndex >= 0 && iIndex < temp.m_iNumTrafficLightLocations);
	temp.m_TrafficLightLocations[iIndex].GetAsVec3(vPosition);
}

bool CJunction::IsPosWithinBounds(const Vector3& vPos, const float fZTolerance) const
{
 	const bool bX = (m_vJunctionMax.x >= vPos.x) & (m_vJunctionMin.x <= vPos.x);
 	const bool bY = (m_vJunctionMax.y >= vPos.y) & (m_vJunctionMin.y <= vPos.y);
 	const bool bZ = (m_vJunctionMax.z + fZTolerance >= vPos.z) 
 		& (m_vJunctionMin.z - fZTolerance <= vPos.z);
 	return (bX & bY & bZ);
}

bool CJunction::IsPosWithinBoundsZOnly(const Vector3& vPos, const float fZTolerance) const
{
	const bool bZ = (m_vJunctionMax.z + fZTolerance >= vPos.z) 
		& (m_vJunctionMin.z - fZTolerance <= vPos.z);
	return bZ;
}

////////////////////////////////////////////////////////////////////////////////////
// FUNCTION : ScanForPedCrossing
// PURPOSE :  Scan for any ped crossing nodes
////////////////////////////////////////////////////////////////////////////////////
void CJunction::ScanForPedCrossing(const CPathFind& pathData)
{
	// Default to some reasonable minimum scan range
	const float kMinScanDist = 20.0f;
	float fScanDistance = kMinScanDist;

	// Compute the longest distance from junction center to entrance nodes
	float fLargestDistToEntranceSq = 0.0f;
	for(int e=0; e < m_iNumEntrances; e++ )
	{
		// some nodes may be left at the default value of vector zero
		if( !m_Entrances[e].m_vPositionOfNode.IsZero() )
		{
			// Compare distance squared from junction center to entrance
			// and keep track of the largest value
			float fDistToEntranceSq = m_vJunctionCenter.Dist2(m_Entrances[e].m_vPositionOfNode);
			if( fDistToEntranceSq > fLargestDistToEntranceSq )
			{
				fLargestDistToEntranceSq = fDistToEntranceSq;
			}
		}
	}
	// If at least one entrance was found farther away than the minimum
	if( fLargestDistToEntranceSq > rage::square(kMinScanDist) )
	{
		// Set the scan distance with extra padding since crossings will be farther away than entrances
		const float kPadFactor = 1.50f;
		fScanDistance = Sqrtf(fLargestDistToEntranceSq) * kPadFactor;
	}
	
	// Check for special ped crossing in range
	CNodeAddress* prevCrossingStart = NULL;
	CNodeAddress* prevCrossingEnd = NULL;
	CPathFind::CFindPedNodeParams searchParams(m_vJunctionCenter, fScanDistance, prevCrossingStart, prevCrossingEnd);
	searchParams.m_SpecialFunctions.Push(SPECIAL_PED_CROSSING);
	CNodeAddress adr = pathData.FindPedNodeClosestToCoors(searchParams);
	if(!adr.IsEmpty())
	{
		m_HasPedCrossingPhase = true;
	}
}

//------------------------------------------------------------------------------------
// NAME : IsRailwayBarrier
// PURPOSE : Helper function which returns whether pObj is a railway barrier object

bool IsRailwayBarrier(CObject * pObj)
{
	if (pObj->IsADoor()) {
		CDoor* door = static_cast<CDoor*>(pObj);
		int iDoorType = door->GetDoorType();
		if (iDoorType == CDoor::DOOR_TYPE_RAIL_CROSSING_BARRIER) {
			return true;
		}
	}
	return false;
}

//----------------------------------------------------------------
// NAME : LowerRailwayBarrier
// PURPOSE : Lowers the railway barrier, if not already lowered
// Returns true if the barrier is in its fully lowered state

bool CJunction::LowerRailwayBarrier(CDoor * pBarrier)
{
	if (pBarrier->GetDoorOpenRatio() >= CJunction::ms_fRailCrossingOpenRatio) {
		return true;
	} 
	else 
	{
		// Delay the opening of the barrier
		// This number will always be same as it's seeded in the constructor of CDynamicEntity pBarrier
		if(m_RailwayTimeSinceSignal > pBarrier->GetRandomNumberInRangeFromSeed(0.0f,0.8f))
		{
			pBarrier->OpenDoor();
		}
		return false;
	}
}

//----------------------------------------------------------------
// NAME : RaiseRailwayBarrier
// PURPOSE : Raises the railway barrier, if not already raised
// Returns true if the barrier is in its fully raised state

bool CJunction::RaiseRailwayBarrier(CDoor * pBarrier)
{
	if (pBarrier->GetDoorOpenRatio() <= CJunction::ms_fRailCrossingCloseRatio) {
		return true;
	} 
	else 
	{
		// Delay the opening of the barrier
		// This number will always be same as it's seeded in the constructor of CDynamicEntity pBarrier
		if(m_RailwayTimeSinceSignal > pBarrier->GetRandomNumberInRangeFromSeed(0.0f,0.8f))
		{
			pBarrier->CloseDoor();
		}
	
		return false;
	}
}

//----------------------------------------------------------------------------
// NAME : IdentifyRailwayBarrierCB
// PURPOSE : Callback which identifies & adds railway barriers to a junction

bool CJunction::IdentifyRailwayBarrierCB(CEntity * pEntity, void * pData)
{
	if(pEntity->GetIsTypeObject())
	{
		if(IsRailwayBarrier((CObject*)pEntity))
		{
			CJunction * pJunction = (CJunction*)pData;
			if(pJunction->AddRailwayBarrier((CObject*)pEntity))
			{
				// If this junction has reached its capacity, return false to halt enumeration
				return false;
			}
		}
	}
	return true;
}

//------------------------------------------------------------
// NAME : AddRailwayBarrier
// PURPOSE : Add a railway crossing barrier to this junction
// Returns false if at full capacity

bool CJunction::AddRailwayBarrier(CObject * pBarrier)
{
	Assert(m_iTemplateIndex!=-1 && m_bIsRailwayCrossing);

	for(s32 b=0; b<JUNCTION_MAX_RAILWAY_BARRIERS; b++)
	{
		if(!m_RailwayBarriers[b])
		{
			//When adding a railway barrier, reset the timer to force a scan for coming trains
			m_iLastApproachingTrainScanTime = 0;

			m_RailwayBarriers[b] = pBarrier;
			return (b==JUNCTION_MAX_RAILWAY_BARRIERS-1);
		}
	}
	Assertf(false, "Junction at (%.1f, %.1f, %.1f) has too many railway crossing barriers in its vicinity (max is %i)", m_vJunctionCenter.x, m_vJunctionCenter.y, m_vJunctionCenter.z, JUNCTION_MAX_RAILWAY_BARRIERS);
	return false;
}

//--------------------------------------------------------------------------------------------
// NAME : ScanForRailwayBarriers
// PURPOSE : Scan the world for nearby railway barriers
// This is called periodically for all junctions which are marked as 'm_bIsRailwayCrossing'
// It needs to be done repeatedly to account for objects streaming in/out as the player moves

void CJunction::ScanForRailwayBarriers()
{
	for(s32 b=0; b<JUNCTION_MAX_RAILWAY_BARRIERS; b++)
		m_RailwayBarriers[b] = NULL;

	fwIsSphereIntersecting sphere(spdSphere(RCC_VEC3V(GetJunctionCenter()), ScalarV(ms_fScanForRailwayBarriersRange)));
	CGameWorld::ForAllEntitiesIntersecting(&sphere, IdentifyRailwayBarrierCB, (void*)this, ENTITY_TYPE_MASK_OBJECT, SEARCH_LOCATION_EXTERIORS);
}

//-------------------------------------------------------------------------------------------
// NAME : ShouldCarsStopForTrain
// PURPOSE : Helper function to determine whether traffic should stop for a train crossing

bool CJunction::ShouldCarsStopForTrain() const
{
	return m_bIsRailwayCrossing && (m_bRailwayBarriersShouldBeDown || !m_bRailwayBarriersAreFullyRaised);
}

//----------------------------------------------------------
// NAME : ScanForApproachingTrains
// PURPOSE : Determine whether there is a train approaching

void CJunction::ScanForApproachingTrains()
{
	for(s32 t=0; t<JUNCTION_MAX_RAILWAY_TRACKS; t++)
	{
		if(m_pTrainTrackNodes[t])
		{
			CTrain* pTrain = CTrain::IsTrainApproachingNode(m_pTrainTrackNodes[t]);
			if(pTrain)
			{
				m_bRailwayBarriersShouldBeDown = true;
				break;
			}
		}
	}
}

//------------------------------------------------------------------------
// NAME : UpdateRailwayCrossing
// PURPOSE : For a railway-crossing junction, perform the per frame update

void CJunction::UpdateRailwayCrossing(float timeStep)
{
	Assert(m_iTemplateIndex!=-1 && m_bIsRailwayCrossing);

	if(fwTimer::GetTimeInMilliseconds()-m_iLastBarrierScanTime > ms_iScanForRailwayBarriersFreqMs)
	{
		m_iLastBarrierScanTime = fwTimer::GetTimeInMilliseconds();

		ScanForRailwayBarriers();
	}
	if(fwTimer::GetTimeInMilliseconds()-m_iLastApproachingTrainScanTime > ms_iScanForApproachingTrainsFreqMs)
	{
		m_iLastApproachingTrainScanTime = fwTimer::GetTimeInMilliseconds();

		m_bRailwayBarriersShouldBeDown = false;
		ScanForApproachingTrains();
	}

	// If barriers should be down, then set flag to indicate that barriers are no longer raised
	if(m_bRailwayBarriersShouldBeDown)
	{
		m_bRailwayBarriersAreFullyRaised = false;
	}

	s32 iNumBarriers = 0;
	s32 iNumInCorrectState = 0;

	//Increment the delay timer for rail way barriers
	m_RailwayTimeSinceSignal += timeStep;

	for(s32 b=0; b<JUNCTION_MAX_RAILWAY_BARRIERS; b++)
	{
		CObject * pBarrier = m_RailwayBarriers[b];
		
		if(pBarrier)
		{
			AssertMsg(pBarrier->IsADoor(), "Adding a non door object to the railway barrier array!");
			CDoor * pDoor = static_cast<CDoor*>(pBarrier);
			iNumBarriers++;

			if(m_bRailwayBarriersShouldBeDown)
			{
				if( LowerRailwayBarrier(pDoor) )
					iNumInCorrectState++;
			}
			else
			{
				if( RaiseRailwayBarrier(pDoor) )
					iNumInCorrectState++;
			}
		}
	}
	if(iNumBarriers == iNumInCorrectState)
	{
		// If all barriers are in the correct state then reset the timer
		m_RailwayTimeSinceSignal = 0;

		// If all barriers are raised then set flag which will allow traffic to pass through again
		if(!m_bRailwayBarriersShouldBeDown)
		{
			m_bRailwayBarriersAreFullyRaised = true;
		}
	}

	UpdateRailwayCrossingLightPulse();
}

//------------------------------------------------------------------------
// NAME : UpdateRailwayCrossingLightPulse
// PURPOSE : Update the light pulse state for railway crossings

void CJunction::UpdateRailwayCrossingLightPulse()
{
	if (m_bRailwayBarriersAreFullyRaised)
	{
		m_RailwayCrossingLightState = RAILWAY_CROSSING_LIGHT_OFF;
		m_uLastRailwayLightPulseTime = 0;
	}
	else
	{
		switch (m_RailwayCrossingLightState)
		{
		case RAILWAY_CROSSING_LIGHT_OFF:

			m_RailwayCrossingLightState = RAILWAY_CROSSING_LIGHT_PULSE_LEFT;
			m_uLastRailwayLightPulseTime = fwTimer::GetTimeInMilliseconds();
			break;


		case RAILWAY_CROSSING_LIGHT_PULSE_LEFT:
		case RAILWAY_CROSSING_LIGHT_PULSE_RIGHT:

			while ((m_uLastRailwayLightPulseTime + ms_uRailwayLightPulseDuration) < fwTimer::GetTimeInMilliseconds())
			{
				m_uLastRailwayLightPulseTime += ms_uRailwayLightPulseDuration;

				if (m_RailwayCrossingLightState == RAILWAY_CROSSING_LIGHT_PULSE_LEFT)
				{
					m_RailwayCrossingLightState = RAILWAY_CROSSING_LIGHT_PULSE_RIGHT;
				}
				else
				{
					m_RailwayCrossingLightState = RAILWAY_CROSSING_LIGHT_PULSE_LEFT;
				}
			}
			break;

		default:
			AssertMsg(false, "CJunction::UpdateRailwayCrossingLightPulse() - Unhandled state.");
			break;
		}
	}
}

//-----------------------------------------------------------------------------------------------------
// NAME : ScanForRailwayNode
// PURPOSE : For a junction marked as 'm_bIsRailwayCrossing' this scans for the closest
// railway nodes to the junction center on each track within 'CJunction::ms_fMaxDistanceFromTrainTrack'
// This need be done only once when the junction is instanced, and since railway tracks are always in
// memory there is no risk of the pointers becoming stale.
// Due to the layout of train tracks, the same track can cross any given junction more than once - so
// we must be aware of this.  To make things manageable, I am assuming that when this happens the
// tracks will be going in opposite directions, and there will not be a third occuranve.  This is why
// I check twice for the closest node on each track (which a heading constraint the 2nd time)

void CJunction::ScanForRailwayNodes()
{
	Assert(m_iTemplateIndex!=-1 && m_bIsRailwayCrossing);

	for(s32 i=0; i<JUNCTION_MAX_RAILWAY_TRACKS; i++)
		m_pTrainTrackNodes[i] = NULL;

	static dev_float sMaxHeightDiff = 4.0f;

	s32 iCount=0;
	for(s32 t=0; t<MAX_TRAIN_TRACKS_PER_LEVEL; t++)
	{
		float fDist = FLT_MAX;
		float fFirstHeading = 0.0f;
		Vector3 vPos;

		// Find the first possible crossing of this track
		s32 iNode = CTrain::FindClosestNodeOnTrack(GetJunctionCenter(), t, NULL, &fDist, &fFirstHeading, &vPos);
		bool bInRange = (iNode != -1 && fDist < CJunction::ms_fMaxDistanceFromTrainTrack && Abs(vPos.z-GetJunctionCenter().z)<sMaxHeightDiff );
		if(bInRange)
		{
			CTrainTrack * pTrack = CTrain::GetTrainTrack(t);
			CTrainTrackNode * pTrackNode = pTrack->GetNode(iNode);
			m_pTrainTrackNodes[iCount++] = pTrackNode;
			if(iCount==JUNCTION_MAX_RAILWAY_TRACKS)
				break;

			// Find a second possible crossing, limiting the check to the opposite heading to above
			const float fOppositeHdg = fwAngle::LimitRadianAngle(fFirstHeading + PI);
			iNode = CTrain::FindClosestNodeOnTrack(GetJunctionCenter(), t, &fOppositeHdg, &fDist);
			bInRange = (iNode != -1 && fDist < CJunction::ms_fMaxDistanceFromTrainTrack);
			if(bInRange)
			{
				pTrack = CTrain::GetTrainTrack(t);
				pTrackNode = pTrack->GetNode(iNode);
				Assertf(pTrackNode != m_pTrainTrackNodes[iCount], "Same track node found twice");
				m_pTrainTrackNodes[iCount++] = pTrackNode;
				if(iCount==JUNCTION_MAX_RAILWAY_TRACKS)
					break;
			}
		}
	}

	Assertf(iCount>0, "Junction at (%.1f,%.1f,%.1f) is marked as railway-crossing, but there are no train-track nodes within %.1fm",
		GetJunctionCenter().x, GetJunctionCenter().y, GetJunctionCenter().z, CJunction::ms_fMaxDistanceFromTrainTrack);
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : Deploy
// PURPOSE :  Initialises this Junction.
/////////////////////////////////////////////////////////////////////////////////

bool CJunction::Deploy(CNodeAddress node, bool bIsBuildPaths, CPathFind& pathData)
{
	m_iLRUTime = fwTimer::GetTimeInMilliseconds();

	//**************************************************************************************
	// See if we have a junction 'template' for this junction node.
	// A junction template is a hand-authored arrangement of entrances, phases, etc, set up
	// using the in-game Junction Editor.  If we do have such a template, then use it to
	// initialise this CJunction.  If not, then fall through to the old system.


#define STOP_DISTANCE_MARGIN (3.0f)
	
	Vector2 junctionCoors, entranceCoors;

	// This shouldn't be needed.
	// It would also be a good place if wanted the junction to search for traffic lights
	// rather than the way we're doing it now.
	RemoveTrafficLights();
	
	sEntryNodeInfo aEntryNodeInfos[MAX_ROADS_INTO_JUNCTION];
	
	m_iNumEntrances = CJunctions::BindJunctionTemplate(*this, node, aEntryNodeInfos, MAX_ROADS_INTO_JUNCTION, pathData);

	if(m_iNumEntrances == 0 && m_iTemplateIndex != -1)
	{
		Clear();
		return false;
	}

	//----------------------------------------------------------------------------
	// If we found a template, then the entry infos will be filled in correctly.
	// We will need to set up the custom light timings.

	if(m_iTemplateIndex != -1)
	{
		CJunctionTemplate & temp = CJunctions::GetJunctionTemplate(m_iTemplateIndex);

		m_iNumLightPhases = temp.m_iNumPhases;
		for(s32 l=0; l<m_iNumLightPhases; l++)
		{
			m_LightTiming[l].m_fDuration = temp.m_PhaseTimings[l].m_fDuration;
		}

		m_iCycleOffsetMs = 0;
		m_fCycleScale = 1.0f;
	}
	else
	{
		Clear();

		// In this old code path, it looks like we don't completely fill in the
		// sEntryNodeInfo objects, so clear them out first (could do this manually in the
		// loop below, not sure which way is faster).
		sysMemSet(aEntryNodeInfos, 0, sizeof(sEntryNodeInfo)*MAX_ROADS_INTO_JUNCTION);

		m_iNumJunctionNodes = 1;
		m_JunctionNodes[0] = node;
	}

	const s32 iNumTemplatedEntrances = m_iNumEntrances;

	for (s32 i = 0; i < m_iNumJunctionNodes; ++i)
	{
		const CPathNode *pJunctionNode = pathData.FindNodePointer(m_JunctionNodes[i]);

		if (m_iTemplateIndex == -1 && i == 0)
		{
			Vector3 vJunctionPos;
			pJunctionNode->GetCoors(vJunctionPos);
			SetJunctionCenter(vJunctionPos);
		}

		for (u32 j = 0; j < pJunctionNode->NumLinks(); j++)
		{
			Assertf(m_iNumEntrances < MAX_ROADS_INTO_JUNCTION, "Junction at (%.1f, %.1f, %.1f)", GetJunctionCenter().x, GetJunctionCenter().y, GetJunctionCenter().z );
			if (m_iNumEntrances < MAX_ROADS_INTO_JUNCTION)		// There is one freaky junction on that map that has 7 neighbours.
			{
				CNodeAddress neighbour = pathData.GetNodesLinkedNodeAddr(pJunctionNode, j);

				bool bDuplicate = false;

				for (s32 k = 0; k < m_iNumEntrances; ++k)
				{
					if (neighbour == aEntryNodeInfos[k].EntryNode)
					{
						bDuplicate = true;
						break;
					}
				}

				if (bDuplicate)
				{
					continue;
				}

				if (pathData.IsRegionLoaded(neighbour))
				{
					const CPathNode *pEntranceNode = pathData.FindNodePointer(neighbour);
					const CPathNodeLink &entranceLink = pathData.GetNodesLink(pJunctionNode, j);

					if (entranceLink.IsShortCut())
					{
						continue;
					}

					pJunctionNode->GetCoors2(junctionCoors);
					pEntranceNode->GetCoors2(entranceCoors);

					if (entranceLink.IsDontUseForNavigation())
					{
						// Look further for an alternate link to grab usable data from (heading, etc.)
						const CPathNode *pAlternateEntranceNode = NULL;

						for (u32 k = 0; k < pEntranceNode->NumLinks(); ++k)
						{
							CNodeAddress alternateEntranceCandidate = pathData.GetNodesLinkedNodeAddr(pEntranceNode, k);

							if (pathData.IsRegionLoaded(alternateEntranceCandidate))
							{
								const CPathNode *pAlternateEntranceCandidate = pathData.FindNodePointer(alternateEntranceCandidate);

								if (pAlternateEntranceCandidate && pAlternateEntranceCandidate != pJunctionNode && !pAlternateEntranceCandidate->IsSwitchedOff())
								{
									const CPathNodeLink &alternateEntranceLink = pathData.GetNodesLink(pEntranceNode, k);

									if (!alternateEntranceLink.IsShortCut() && !alternateEntranceLink.IsDontUseForNavigation())
									{
										if (pAlternateEntranceNode != NULL)
										{
											// We've found more than one alternate link, meaning we have multiple possible entry orientations.  Give up.
											pAlternateEntranceNode = NULL;
											break;
										}

										pAlternateEntranceNode = pAlternateEntranceCandidate;
									}
								}
							}
							else
							{
								break;
							}
						}

						if (pAlternateEntranceNode)
						{
							pEntranceNode->GetCoors2(junctionCoors);
							pAlternateEntranceNode->GetCoors2(entranceCoors);
						}
					}

					sEntryNodeInfo& e = aEntryNodeInfos[m_iNumEntrances];

					e.EntryDir = entranceCoors - junctionCoors;
					e.EntryDir.Normalize();

					e.vNodePos = pEntranceNode->GetPos();

					e.orientation = rage::Atan2f(-e.EntryDir.x, e.EntryDir.y);

					e.EntryNode = neighbour;

					e.lanesToJunction = pathData.GetNodesLink(pJunctionNode, j).m_1.m_LanesFromOtherNode;
					e.lanesFromJunction = pathData.GetNodesLink(pJunctionNode, j).m_1.m_LanesToOtherNode;

					m_iNumEntrances++;
				}
			}
		}
	}

	//----------------------------------------------
	// Sort the connecting nodes in clockwise order

	bool bChange = true;
	while (bChange)
	{
		bChange = false;
		for (s32 e = iNumTemplatedEntrances; e < m_iNumEntrances-1; e++)
		{
			if (aEntryNodeInfos[e].orientation > aEntryNodeInfos[e+1].orientation)
			{
				sEntryNodeInfo	temp = aEntryNodeInfos[e];
				aEntryNodeInfos[e] = aEntryNodeInfos[e+1];
				aEntryNodeInfos[e+1] = temp;

				bChange = true;
			}
		}
	}

	//---------------------------------------------
	// Copy the required fields into the junction
	m_bHasTrafficLightNodes = false;
	m_bHasGiveWayNodes = false;
	u32  nSwitchedOffEntrances = 0;

	Assertf(JUNCTION_MAX_LIGHT_PHASES <= 8, "Potential CJunctionEntrance::m_iPhase and m_iLeftFilterPhase overflow");
	
#define PARALLEL_ENTRANCE_TOLERANCE 20.0f
	bool bAllEntrancesParallel = true;
	s32 iBaseParallelEntrance = -1;

	for (s32 i = 0; i < m_iNumEntrances; i++)
	{
		CJunctionEntrance & entrance = m_Entrances[i];
		entrance.m_Node = aEntryNodeInfos[i].EntryNode;
		entrance.m_iPhase = aEntryNodeInfos[i].phase;
		entrance.m_vEntryDir = aEntryNodeInfos[i].EntryDir;
		entrance.m_vPositionOfNode = aEntryNodeInfos[i].vNodePos;
		entrance.m_iLeftFilterPhase = aEntryNodeInfos[i].leftLaneFilterPhase;
		entrance.m_bCanTurnRightOnRedLight = aEntryNodeInfos[i].canTurnRightOnRedLight != 0;
		entrance.m_bLeftLaneIsAheadOnly = aEntryNodeInfos[i].leftLaneIsAheadOnly != 0;
		entrance.m_bRightLaneIsRightOnly = aEntryNodeInfos[i].rightLaneIsRightOnly != 0;

		const CPathNode * pEntranceNode = pathData.FindNodePointerSafe(entrance.m_Node);

		entrance.m_bIsGiveWay = pEntranceNode->IsGiveWay();
		entrance.m_bIsSwitchedOff = pEntranceNode->IsSwitchedOff() && pEntranceNode->m_2.m_switchedOffOriginal;

		if (aEntryNodeInfos[i].lanesToJunction > 0)
		{
			m_bHasTrafficLightNodes |= pEntranceNode->IsTrafficLight();
		}

		if (entrance.m_bIsSwitchedOff)
		{
			nSwitchedOffEntrances++;
		}
		else
		{
			if (aEntryNodeInfos[i].lanesToJunction > 0)
			{
				m_bHasGiveWayNodes |= entrance.m_bIsGiveWay;
			}

			if (bAllEntrancesParallel && !entrance.m_bIsGiveWay)
			{
				if (iBaseParallelEntrance == -1)
				{
					iBaseParallelEntrance = i;
				}
				else
				{
					const float fDifference = Abs(SubtractAngleShorter(aEntryNodeInfos[iBaseParallelEntrance].orientation, aEntryNodeInfos[i].orientation));

					bAllEntrancesParallel = fDifference <= PARALLEL_ENTRANCE_TOLERANCE * DtoR || fDifference >= (180.0f - PARALLEL_ENTRANCE_TOLERANCE) * DtoR;
				}
			}
		}
	}

	if (bAllEntrancesParallel)
	{
		for (s32 i = 0; i < m_iNumEntrances; i++)
		{
			m_Entrances[i].m_bIgnoreBackedUpExitsOnStraight = !m_Entrances[i].m_bIsSwitchedOff && !m_Entrances[i].m_bIsGiveWay;
		}
	}

	//would this have been a junction if not for switched-off
	//entrances? if so, we don't want to stop for cars stopped
	//in the middle, or do dirty flag avoidance
	CPathNode * pNode = pathData.FindNodePointer(node);

	m_bIsOnlyJunctionBecauseHasSwitchedOffEntrances = false;

	if(pNode)
	{
		bool bCanUseSpecialFunction = !pNode->HasSpecialFunction() || pNode->IsFalseJunction();

		if(bCanUseSpecialFunction)
		{
			pNode->ClearFalseJunction();
		}

		s32 iActiveEntrances = m_iNumEntrances - nSwitchedOffEntrances;

		if (m_iTemplateIndex == -1 
			&& !m_bHasTrafficLightNodes
			&& !m_bHasGiveWayNodes
			&& (iActiveEntrances <= 2 || bAllEntrancesParallel))
		{
			if (bCanUseSpecialFunction)
			{
				pNode->SetFalseJunction();
			}

			m_bIsOnlyJunctionBecauseHasSwitchedOffEntrances = true;
		}
	}

	//-----------------------------
	// Set up stopping distances

	if(m_iTemplateIndex != -1)
	{
		// If we're using a junction template, then stopping distances are hand edited
		CJunctionTemplate & temp = CJunctions::GetJunctionTemplate(m_iTemplateIndex);
		for (s32 e = 0; e < m_iNumEntrances; e++)
		{
			m_Entrances[e].m_EntryStopDistance = temp.m_Entrances[e].m_fStoppingDistance;
		}
	}
	else
	{
		// Otherwise we autogenerate them..
		// Now that all neighbouring nodes have been identified we can work out at what
		// distance the cars should stop for each entry point.
		for (s32 e = 0; e < m_iNumEntrances; e++)
		{
			CJunctionEntrance & entrance = m_Entrances[e];
			entrance.m_EntryStopDistance = STOP_DISTANCE_MARGIN + LANEWIDTH * 0.5f;

			for (s32 ee = 0; ee < m_iNumEntrances; ee++)
			{
				if (e != ee)
				{
					float	dot = Dot(aEntryNodeInfos[e].EntryDir, aEntryNodeInfos[ee].EntryDir);
					if (rage::Abs(dot) < 0.5f)	// Make sure the roads are sufficiently perpendicular
					{
						float	lanes = 0.5f;

						if (aEntryNodeInfos[ee].lanesToJunction == 0)
						{
							lanes = aEntryNodeInfos[ee].lanesFromJunction * 0.5f;
						}
						else if (aEntryNodeInfos[ee].lanesFromJunction == 0)
						{
							lanes = aEntryNodeInfos[ee].lanesToJunction * 0.5f;
						}
						else
						{
							if (aEntryNodeInfos[e].EntryDir.Cross(aEntryNodeInfos[ee].EntryDir) < 0.0f)
							{		// turn left (?)
								lanes = (float)aEntryNodeInfos[ee].lanesToJunction;
							}
							else
							{		// turn right (?)
								lanes = (float)aEntryNodeInfos[ee].lanesFromJunction;
							}
						}

						entrance.m_EntryStopDistance = rage::Max(entrance.m_EntryStopDistance, (lanes * LANEWIDTH + STOP_DISTANCE_MARGIN) );
					}
				}
			}
		}
	}

	// If this is a railway crossing junction, scan for the nearest track node
	if(m_bIsRailwayCrossing)
	{
		ScanForRailwayNodes();
	}

	// If this isn't a template then scan for ped nodes
	if (m_iTemplateIndex == -1)
	{
		ScanForPedCrossing(pathData);

		// And now that we know if it has ped crossing, see if there's any adjustments to the phase we need to make
		CJunctions::FindAutoJunctionAdjustments(RCC_VEC3V(m_vJunctionCenter), GetHasPedCrossingPhase(), m_iCycleOffsetMs, m_fCycleScale);
	}

	//calculate worldspace AABB
	m_vJunctionMin = m_vJunctionCenter;
	m_vJunctionMax = m_vJunctionCenter;
	for (int i = 0; i < m_iNumJunctionNodes; i++)
	{
		const CPathNode* pJnNode = pathData.FindNodePointerSafe(m_JunctionNodes[i]);
		Assert(pJnNode);
		const Vector3& vJnPos = pJnNode->GetPos();
		m_vJunctionMin.x = rage::Min(m_vJunctionMin.x, vJnPos.x);
		m_vJunctionMin.y = rage::Min(m_vJunctionMin.y, vJnPos.y);
		m_vJunctionMin.z = rage::Min(m_vJunctionMin.z, vJnPos.z);
		m_vJunctionMax.x = rage::Max(m_vJunctionMax.x, vJnPos.x);
		m_vJunctionMax.y = rage::Max(m_vJunctionMax.y, vJnPos.y);
		m_vJunctionMax.z = rage::Max(m_vJunctionMax.z, vJnPos.z);
	}
	for (int i = 0; i < m_iNumEntrances; i++)
	{
		const CJunctionEntrance& jnEntrance = GetEntrance(i);
		m_vJunctionMin.x = rage::Min(m_vJunctionMin.x, jnEntrance.m_vPositionOfNode.x);
		m_vJunctionMin.y = rage::Min(m_vJunctionMin.y, jnEntrance.m_vPositionOfNode.y);
		m_vJunctionMin.z = rage::Min(m_vJunctionMin.z, jnEntrance.m_vPositionOfNode.z);
		m_vJunctionMax.x = rage::Max(m_vJunctionMax.x, jnEntrance.m_vPositionOfNode.x);
		m_vJunctionMax.y = rage::Max(m_vJunctionMax.y, jnEntrance.m_vPositionOfNode.y);
		m_vJunctionMax.z = rage::Max(m_vJunctionMax.z, jnEntrance.m_vPositionOfNode.z);
	}

	if (!bIsBuildPaths && CVirtualRoad::ms_bEnableVirtualJunctionHeightmaps
		&& !m_JunctionNodes[0].IsEmpty() && ThePaths.IsRegionLoaded(m_JunctionNodes[0]))
	{
		const s32* piVirtualJunctionIndex = ThePaths.apRegions[m_JunctionNodes[0].GetRegion()]->JunctionMap.JunctionMap.SafeGet(m_JunctionNodes[0].RegionAndIndex());
		m_iVirtualJunctionIndex = piVirtualJunctionIndex != NULL ? *piVirtualJunctionIndex : -1;  
	}

	return true;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : RemoveWaitingForLightPed
// PURPOSE :  Decrement waiting peds counter
/////////////////////////////////////////////////////////////////////////////////

void CJunction::RemoveWaitingForLightPed()
{ 
	if( m_uNumWaitingForLightPeds > 0 )
	{
		m_uNumWaitingForLightPeds--; 
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : RemoveActivelyCrossingPed
// PURPOSE :  Decrement crossing peds counter
/////////////////////////////////////////////////////////////////////////////////

void CJunction::RemoveActivelyCrossingPed()
{ 
	if( m_uNumActivelyCrossingPeds > 0 )
	{
		m_uNumActivelyCrossingPeds--; 
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : InitLevel
// PURPOSE :  Clears all junctions.
/////////////////////////////////////////////////////////////////////////////////

void	CJunctions::Init(unsigned initMode)
{
#if __BANK
	m_bDebug = false;
#endif

	m_LastNetworkUpdateTime = NetworkInterface::IsGameInProgress() ? NetworkInterface::GetSyncedTimeInMilliseconds() : 0;

    if(initMode == INIT_SESSION)
    {
	    for (s32 i = 0; i < JUNCTIONS_MAX_NUMBER; i++)
	    {
			m_aJunctions[i].Clear();
	    }

		Assert(m_JunctionTemplates == NULL);
		m_JunctionTemplates = rage_new CJunctionTemplateArray();
		Assert(m_JunctionTemplates);

		LoadJunctionTemplates();

		m_fTimeSinceLastScan = 0.0f;
    }
}

void	CJunctions::Shutdown(unsigned shutdownMode)
{
	//m_JunctionTemplates->m_Entries.Reset();

	if(shutdownMode == SHUTDOWN_SESSION)
	{
		Assert(m_JunctionTemplates);
		delete m_JunctionTemplates;
		m_JunctionTemplates = NULL;
	}
}

#if __BANK
void CJunctions::InitWidgets()
{
	bkBank * pBank = BANKMGR.FindBank("Vehicle AI and Nodes");
	Assertf(pBank, "Where's the 'Vehicle AI and Nodes' bank gone, eh?");
	if(!pBank)
		return;

	pBank->PushGroup("Junctions");

		pBank->AddToggle("Debug Junctions", &CJunctions::m_bDebug);
		pBank->AddToggle("Debug Wait For Traffic", &CJunctions::m_bDebugWaitForTraffic);
		pBank->AddToggle("Debug Junction Text", &CJunctions::m_bDebugText);
		pBank->AddToggle("Debug Junction Timeslicing", &CJunctions::m_bDebugTimeslicing);
		pBank->AddToggle("Debug Lights", &CJunctions::m_bDebugLights);
		pBank->AddButton("Advance lights phase of closest", CJunctions::Debug_AdvanceClosestLights);
		pBank->AddButton("Malfunction closest", CJunctions::Debug_MalfunctionClosest);
		pBank->AddToggle("Disable Processing", &CJunctions::m_bDisableProcessing);
		pBank->AddToggle("Enable Timeslicing", &CJunctions::m_TimesliceEnabled);
		pBank->AddSlider("Timeslicing Period", &CJunctions::m_TimesliceUpdatePeriod, 1, 30, 1);
		pBank->AddButton("Remove All Junctions", RemoveAllJunctions);
		pBank->AddToggle("Instance junctions around local player", &CJunctions::ms_bInstanceJunctionsAroundPlayer);
		pBank->AddSlider("Junctions scan frequency", &CJunctions::ms_fScanFrequency, 1.0f, 10.0f, 0.5f);
		pBank->AddSlider("Junctions scan distance", &CJunctions::ms_fInstanceJunctionDist, 0.0f, 500.0f, 1.0);
		
		pBank->PushGroup("Ped crossing");
		  pBank->AddSlider("Safe duration ratio", &CJunction::ms_fDurationRatioForSafeCrossing, 0.0f, 1.0f, 0.01f);
		  pBank->AddSlider("Max time extension", &CJunction::ms_fMaxTimeExtensionForCrossingPeds, 0.0f, 10.0f, 0.5f);
		pBank->PopGroup();

		pBank->PushGroup("Light phase duration multipliers");
			pBank->AddSlider("Empty multiplier", &CJunction::ms_LightPhaseDurationMultiplierEmpty, 0.0f, 5.0f, 0.05f);
			pBank->AddSlider("Low threshold (vehicles waiting)", &CJunction::ms_LightPhaseDurationMultiplierLowThreshold, 0, 30, 1);
			pBank->AddSlider("Low multiplier", &CJunction::ms_LightPhaseDurationMultiplierLow, 0.0f, 5.0f, 0.05f);
			pBank->AddSlider("High threshold (vehicles waiting)", &CJunction::ms_LightPhaseDurationMultiplierHighThreshold, 0, 30, 1);
			pBank->AddSlider("High multiplier", &CJunction::ms_LightPhaseDurationMultiplierHigh, 0.0f, 5.0f, 0.05f);
			pBank->AddSlider("Extreme threshold (vehicles waiting)", &CJunction::ms_LightPhaseDurationMultiplierExtremeThreshold, 0, 30, 1);
			pBank->AddSlider("Extreme multiplier", &CJunction::ms_LightPhaseDurationMultiplierExtreme, 0.0f, 5.0f, 0.05f);
		pBank->PopGroup();

		pBank->PushGroup("Railway crossings");
		  pBank->AddSlider("Max distance from track", &CJunction::ms_fMaxDistanceFromTrainTrack, 0.0f, 150.0f, 1.0);
		  pBank->AddSlider("Barriers scan distance", &CJunction::ms_fScanForRailwayBarriersRange, 0.0f, 150.0f, 1.0);
		  pBank->AddSlider("Barriers scan freq ms", &CJunction::ms_iScanForRailwayBarriersFreqMs, 0, 20000, 100);
		  pBank->AddSlider("Scan for approaching trains freq ms", &CJunction::ms_iScanForApproachingTrainsFreqMs, 0, 20000, 100);
		pBank->PopGroup();

	pBank->PopGroup();
}
void CJunctions::RemoveAllJunctions()
{
	for(int j=0; j<JUNCTIONS_MAX_NUMBER; j++)
	{
		m_aJunctions[j].Clear();
	}

	// Also clear all junction references from the vehicles in the pool
	CVehicle::Pool *VehiclePool = CVehicle::GetPool();
	CVehicle* pVehicle;
	s32 v = (s32) VehiclePool->GetSize();

	while(v--)
	{
		pVehicle = VehiclePool->GetSlot(v);
		if(pVehicle)
		{	
			pVehicle->GetIntelligence()->ResetCachedJunctionInfo();
		}	
	}
}
CJunction * CJunctions::Debug_GetClosestJunction()
{
	camDebugDirector & debugDirector = camInterface::GetDebugDirector();
	const Vector3 vOrigin = debugDirector.IsFreeCamActive() ? debugDirector.GetFreeCamFrame().GetPosition() : CPlayerInfo::ms_cachedMainPlayerPos;

	int iClosestJunction = -1;
	float fClosestJunctionDist = FLT_MAX;
	for(int j=0; j<JUNCTIONS_MAX_NUMBER; j++)
	{
		if(m_aJunctions[j].GetNumJunctionNodes() > 0)
		{
			const Vector3 vToJunction = m_aJunctions[j].GetJunctionCenter() - vOrigin;
			const float fDist = vToJunction.Mag();
			if(fDist < fClosestJunctionDist)
			{
				fClosestJunctionDist = fDist;
				iClosestJunction = j;
			}
		}
	}
	if(iClosestJunction != -1)
	{
		return &m_aJunctions[iClosestJunction];
	}
	return NULL;
}
void CJunctions::Debug_AdvanceClosestLights()
{
	CJunction * pJunction = Debug_GetClosestJunction();
	if(pJunction)
	{
		pJunction->SetLightTimeRemaining(0.0f);
	}
}
void CJunctions::Debug_MalfunctionClosest()
{
	CJunction * pJunction = Debug_GetClosestJunction();
	if(pJunction)
	{
		pJunction->SetToMalfunction( !pJunction->GetIsMalfunctioning() );
	}
}

#define VISUALISE_JUNCTION_DEBUG_RANGE				80.0f
#define VISUALISE_JUNCTION_DEBUG_RANGE_SQR			(80.0f * 80.0f)

void CJunctions::RenderDebug()
{
	char	tempString[128];
	int iTextHeight = grcDebugDraw::GetScreenSpaceTextHeight();

	if( CJunctions::m_bDebugText || CJunctions::m_bDebug )
	{
		CVehicle::Pool *VehiclePool = CVehicle::GetPool();
		CVehicle* pVehicle;
		s32 v = (s32) VehiclePool->GetSize();

		while(v--)
		{
			pVehicle = VehiclePool->GetSlot(v);
			if(pVehicle)
			{	
				const Vector3 vVehiclePosition = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition());
				Vector3 vDiff = vVehiclePosition - camInterface::GetPos();
				float fDist = vDiff.Mag();
				if(fDist < VISUALISE_JUNCTION_DEBUG_RANGE) 
				{
					if (CJunctions::m_bDebugText)
					{
						sprintf(tempString, "Junction Command:%s\n",  CVehicleIntelligence::GetJunctionCommandName(pVehicle->GetIntelligence()->GetJunctionCommand()));
						grcDebugDraw::Text(vVehiclePosition, Color32(255, 255, 0), 0, iTextHeight, tempString);

						sprintf(tempString, "Filter Command:%s\n",  CVehicleIntelligence::GetJunctionFilterName(pVehicle->GetIntelligence()->GetJunctionFilter()));
						grcDebugDraw::Text(vVehiclePosition, Color32(255, 255, 0), 0, iTextHeight*2, tempString);
					}
					else if (CJunctions::m_bDebug)
					{
						sprintf(tempString, "%s\n",  CVehicleIntelligence::GetJunctionCommandShortName(pVehicle->GetIntelligence()->GetJunctionCommand()));
						grcDebugDraw::Text(vVehiclePosition, Color32(255, 255, 0), 0, iTextHeight, tempString);

						sprintf(tempString, "%s\n",  CVehicleIntelligence::GetJunctionFilterShortName(pVehicle->GetIntelligence()->GetJunctionFilter()));
						grcDebugDraw::Text(vVehiclePosition, Color32(255, 255, 0), 0, iTextHeight*2, tempString);
					}
				}
			}
		}

		s32 iNumActive = 0;
		s32 i;
		for (i = 0; i < JUNCTIONS_MAX_NUMBER; i++)
		{
			CJunction & junction = m_aJunctions[i];

			if(junction.GetNumberOfCars() > 0)
				iNumActive++;

			if( junction.GetNumJunctionNodes() > 0 &&
				!junction.GetJunctionNode(0).IsEmpty() &&
				ThePaths.IsRegionLoaded(junction.GetJunctionNode(0)))
			{
				const CPathNode	* pNode = ThePaths.FindNodePointer(junction.GetJunctionNode(0));
				if(pNode)
				{
					Vector3		crs;
					pNode->GetCoors(crs);

					int iYOffset = 0;
					if (CJunctions::m_bDebug)
					{
						grcDebugDraw::Line(crs, crs + Vector3(0.0f,0.0f,5.0f), Color32(255, 0, 255, 255), Color32(255, 255, 0, 255));
						if (junction.IsOnlyJunctionBecauseHasSwitchedOffEntrances())
						{
							sprintf(tempString, "FakeJunction");
						}
						grcDebugDraw::Text(crs + Vector3(0.f,0.f,3.0f), Color32(255, 255, 0), 0, iYOffset, tempString);
						iYOffset += iTextHeight;
					}

					if (CJunctions::m_bDebugText)
					{
						sprintf(tempString, "Junction:%" PTRDIFFTFMT "d #Cars:%d #Peds C/W:%u/%u", &junction - CJunctions::m_aJunctions.GetElements(), junction.GetNumberOfCars(), junction.GetNumActivelyCrossingPeds(), junction.GetNumWaitingForLightPed());
						grcDebugDraw::Text(crs + Vector3(0.f,0.f,3.0f), Color32(255, 255, 0), 0, iYOffset, tempString);
						iYOffset += iTextHeight;
					}
					else if (CJunctions::m_bDebug)
					{
						sprintf(tempString, "%d : %d : %d", junction.GetNumberOfCars(), junction.GetNumActivelyCrossingPeds(), junction.GetNumWaitingForLightPed());
						grcDebugDraw::Text(crs + Vector3(0.f,0.f,3.0f), Color32(255, 255, 0), 0, iYOffset, tempString);
						iYOffset += iTextHeight;
					}

					if(junction.GetTemplateIndex() != -1)
					{
						if (CJunctions::m_bDebugText)
						{
							sprintf(tempString, "Template : %i", junction.GetTemplateIndex());
							grcDebugDraw::Text(crs + Vector3(0.f,0.f,3.0f), Color32(255, 255, 0), 0, iYOffset, tempString);
							iYOffset += iTextHeight;

							sprintf(tempString, "Light Phase: %i (%.1f)", junction.GetLightPhase(), junction.GetLightTimeRemaining());
							grcDebugDraw::Text(crs + Vector3(0.f,0.f,3.0f), Color32(255, 255, 0), 0, iYOffset, tempString);
							iYOffset += iTextHeight;

							sprintf(tempString, "Light Phase Duration Multiplier: %.1f", junction.GetLightPhaseDurationMultiplier());
							grcDebugDraw::Text(crs + Vector3(0.f,0.f,3.0f), Color32(255, 255, 0), 0, iYOffset, tempString);
							iYOffset += iTextHeight;

							sprintf(tempString, "Multiplier Vehicles: %u", junction.GetNumLightPhaseVehicles());
							grcDebugDraw::Text(crs + Vector3(0.f,0.f,3.0f), Color32(255, 255, 0), 0, iYOffset, tempString);
							iYOffset += iTextHeight;
						}

						if(junction.GetIsRailwayCrossing())
						{
							const Vector3 vOffsetZ = ZAXIS*0.5f;
							const Vector3 vPosAboveJunction = junction.GetJunctionCenter()+vOffsetZ;

							// Draw lines to all train-tracks
							for(s32 t=0; t<CJunction::JUNCTION_MAX_RAILWAY_TRACKS; t++)
							{
								CTrainTrackNode * pNode = junction.GetRailwayTrainTrackNode(t);
								if(pNode)
								{
									s32 iTrack = CTrain::GetTrackIndexFromNode(pNode);
									if (CJunctions::m_bDebug)
									{
										grcDebugDraw::Line(vPosAboveJunction, pNode->GetCoors()+vOffsetZ, Color_orange, Color_coral);
										grcDebugDraw::Sphere(pNode->GetCoors()+vOffsetZ, 0.25f, Color_coral);
									}

									if (CJunctions::m_bDebugText)
									{
										sprintf(tempString, "track %i", iTrack);
										grcDebugDraw::Text(pNode->GetCoors()+vOffsetZ, Color_orange, tempString);
									}
								}
							}

							if (CJunctions::m_bDebug)
							{
								// Draw lines to all barriers
								for(s32 b=0; b<CJunction::JUNCTION_MAX_RAILWAY_BARRIERS; b++)
								{
									CObject * pBarrier = junction.GetRailwayBarrier(b);
									if(pBarrier)
									{
										CDoor* pBar = static_cast<CDoor*>(pBarrier);
										float fRatio = pBar->GetDoorOpenRatio();
										const Vector3 vBarrierPos = VEC3V_TO_VECTOR3(pBarrier->GetTransform().GetPosition())+ZAXIS;
										grcDebugDraw::Line(vPosAboveJunction, vBarrierPos, Color_white, Color_red);
										grcDebugDraw::Sphere(vBarrierPos, 0.25f, Color_white);
										sprintf(tempString, "barrier obj: 0x%p\n ratio:%f ", pBarrier, fRatio);
										grcDebugDraw::Text(vBarrierPos, Color_white, tempString);
									}
								}
							}

							if (CJunctions::m_bDebugText)
							{
								// Draw desired/actual barrier states
								sprintf(tempString, "Railway Crossing");
								grcDebugDraw::Text(crs, Color_red, 0, iYOffset, tempString);
								iYOffset += iTextHeight;

								sprintf(tempString, "Barrier state (%s), desired (%s)",
									junction.GetRailwayBarriersAreFullyRaised() ? "raised" : "lowered",
									junction.GetRailwayBarriersShouldBeDown() ? "lowered" : "raised");
								grcDebugDraw::Text(crs, Color_red, 0, iYOffset, tempString);
								iYOffset += iTextHeight;
							}
						}
					}
					else if (CJunctions::m_bDebugText)
					{
						if( junction.GetHasPedCrossingPhase() )
						{
							sprintf(tempString, "Template : none, HasPedPhase[true]");
						}
						else
						{
							sprintf(tempString, "Template : none, HasPedPhase[false]");
						}
						grcDebugDraw::Text(crs + Vector3(0.f,0.f,3.0f), Color32(255, 255, 0), 0, iYOffset, tempString);
						iYOffset += iTextHeight;


						CAutoJunctionAdjustment* adj = FindAutoJunctionAdjustmentData(RCC_VEC3V(junction.GetJunctionCenter()));
						if (adj)
						{
							formatf(tempString, "Adj Offset %f, Dur %f", adj->m_fCycleOffset, adj->m_fCycleDuration);
							grcDebugDraw::Text(crs + Vector3(0.f,0.f,3.0f), Color32(255, 255, 0), 0, iYOffset, tempString);
							iYOffset += iTextHeight;
						}
					}


					for (s32 i = 0; i < CJunction::JUNCTION_MAX_VEHICLES; i++)
					{
						const CVehicle *pVeh = junction.GetVehicle(i);

						if (pVeh)
						{
							const CVehicleIntelligence *pIntelligence = pVeh->GetIntelligence();

							Vector3 vVehPos = VEC3V_TO_VECTOR3(pVeh->GetTransform().GetPosition());
							if (CJunctions::m_bDebug)
							{
								grcDebugDraw::Line(vVehPos, crs + Vector3(0.0f,0.0f,2.0f), Color32(255, 255, 255, 128));
							}

							int iY = iTextHeight * 3;

							s32 iEntrance;
							float dist;

							if (pIntelligence->GetJunction() == &junction && junction.CalculateDistanceToJunction(pVeh, &iEntrance, dist))
							{
								iEntrance = junction.FindEntranceIndexWithNode(pIntelligence->GetJunctionEntranceNode());
								s32 iEntranceLane = pIntelligence->GetJunctionEntranceLane();
								s32 iExit = junction.FindEntranceIndexWithNode(pIntelligence->GetJunctionExitNode());
								s32 iExitLane = pIntelligence->GetJunctionExitLane();

								if (CJunctions::m_bDebugText)
								{
									sprintf(tempString, "dist to junction: %.1f, curr phase: %i\n", dist, junction.GetLightPhase());
									grcDebugDraw::Text(vVehPos, Color_yellow, 0, iY, tempString);
									iY += iTextHeight;

									if (iEntrance != -1 && iEntrance < junction.GetNumEntrances())
									{
										sprintf(tempString, "phase: %i, left-filter phase: %i\n", junction.GetEntrance(iEntrance).m_iPhase, junction.GetEntrance(iEntrance).m_iLeftFilterPhase);
										grcDebugDraw::Text(vVehPos, Color_yellow, 0, iY, tempString);
										iY += iTextHeight;

										sprintf(tempString, "cars behind us: %i", pIntelligence->m_NumCarsBehindUs);
										grcDebugDraw::Text(vVehPos, Color_yellow, 0, iY, tempString);
										iY += iTextHeight;

										sprintf(tempString, "entrance: %i, lane: %i", iEntrance, iEntranceLane);
										grcDebugDraw::Text(vVehPos, Color_yellow, 0, iY, tempString);
										iY += iTextHeight;

										sprintf(tempString, "exit: %i, lane: %i", iExit, iExitLane);
										grcDebugDraw::Text(vVehPos, Color_yellow, 0, iY, tempString);
										iY += iTextHeight;
									}

									const CPathNode* pExitNode = ThePaths.FindNodePointerSafe(pIntelligence->GetJunctionExitNode());

									if (pExitNode)
									{
										ScalarV fDistanceToExit = Mag(VECTOR3_TO_VEC3V(pExitNode->GetPos()) - pVeh->GetVehiclePosition());

										sprintf(tempString, "distance to exit: %.1f\n", fDistanceToExit.Getf());
										grcDebugDraw::Text(vVehPos, Color_yellow, 0, iY, tempString);
										iY += iTextHeight;
									}
								}
								else if (CJunctions::m_bDebug)
								{
									sprintf(tempString, "%.1f\n", dist);
									grcDebugDraw::Text(vVehPos, Color_yellow, 0, iY, tempString);
									iY += iTextHeight;

									sprintf(tempString, "%.1f\n", junction.GetTimeToJunction(pVeh, dist));
									grcDebugDraw::Text(vVehPos, Color_yellow, 0, iY, tempString);
									iY += iTextHeight;
								}
							}
							
							if ((pIntelligence->GetJunction() == &junction) || (!pIntelligence->GetJunction() && pIntelligence->GetPreviousJunction() == &junction))
							{
								const CPathNode* pSecondaryExitNode = ThePaths.FindNodePointerSafe(pIntelligence->GetPreviousJunctionExitNode());

								if (pSecondaryExitNode)
								{
									const CJunction *pJunction = FindJunctionFromNode(pIntelligence->GetPreviousJunctionNode());

									if(pJunction)
									{
										s32 iPreviousExit = pJunction->FindEntranceIndexWithNode(pIntelligence->GetPreviousJunctionExitNode());
										s32 iPreviousExitLane = pIntelligence->GetPreviousJunctionExitLane();

										sprintf(tempString, "previous exit: %i, lane: %i", iPreviousExit, iPreviousExitLane);
										grcDebugDraw::Text(vVehPos, Color_yellow, 0, iY, tempString);
										iY += iTextHeight;
									}

									ScalarV fDistanceFromExit = Mag(VECTOR3_TO_VEC3V(pSecondaryExitNode->GetPos()) - pVeh->GetVehiclePosition());

									sprintf(tempString, "distance from previous exit: %.1f\n", fDistanceFromExit.Getf());
									grcDebugDraw::Text(vVehPos, Color_yellow, 0, iY, tempString);
									iY += iTextHeight;
								}

								const CVehicleNodeList* pNodeList = pVeh->GetIntelligence()->GetNodeList();

								if (pNodeList)
								{
									const s32 iNodeIndex = pNodeList->GetTargetNodeIndex();

									if (iNodeIndex >= 0)
									{
										const s8 iLaneIndex = pNodeList->GetPathLaneIndex(iNodeIndex);

										sprintf(tempString, "current node: %i, lane: %i\n", iNodeIndex, iLaneIndex);
										grcDebugDraw::Text(vVehPos, Color_yellow, 0, iY, tempString);
										iY += iTextHeight;
									}
								}
							}
						}
					}

					// Go through the entrynodes and display a line where we think the cars should stop.
					for (s32 j = 0; j < junction.GetNumEntrances(); j++)
					{
						CJunctionEntrance & entrance = junction.GetEntrance(j);
						CNodeAddress neighbourNode = entrance.m_Node;
						if (ThePaths.IsRegionLoaded(neighbourNode))
						{
							Vector3 neighbourCoors = crs;
							neighbourCoors.x += entrance.m_vEntryDir.x * entrance.m_EntryStopDistance;
							neighbourCoors.y += entrance.m_vEntryDir.y * entrance.m_EntryStopDistance;

							if (CJunctions::m_bDebug)
							{
								grcDebugDraw::Line(neighbourCoors + Vector3(0.0f,0.0f,2.0f), crs + Vector3(0.0f,0.0f,2.0f), Color32(0, 128, 255, 128));
								grcDebugDraw::Line(neighbourCoors - Vector3(0.0f,0.0f,1.0f), neighbourCoors + Vector3(0.0f,0.0f,4.0f), Color32(0, 128, 255, 255));

								// Draw entrance position and entry direction
								Vector3 vDebugEntrancePos = entrance.m_vPositionOfNode + Vector3(0.0f,0.0f,0.5f);
								Vector3 vDebugEntryDirPos = vDebugEntrancePos + (3.0f * Vector3(entrance.m_vEntryDir, Vector2::kXY));
								grcDebugDraw::Sphere( vDebugEntrancePos, 0.8f, Color_white, false);
								grcDebugDraw::Arrow( VECTOR3_TO_VEC3V(vDebugEntrancePos), VECTOR3_TO_VEC3V(vDebugEntryDirPos), 0.5f, Color_white);
							}

							if (CJunctions::m_bDebugText)
							{
								sprintf(tempString, "%d\n", j);
								grcDebugDraw::Text(neighbourCoors + Vector3(0.f,0.f,2.0f), Color32(0, 128, 255, 128), tempString);
							}
						}
					}
				}
			}
		}

		grcDebugDraw::AddDebugOutput(Color_white, "Num active junctions : %i", iNumActive);
	}

	if( m_bDebugLights )
	{
		DebugRenderLights();
	}
}

void CJunctions::DebugRenderLights()
{
	camDebugDirector & debugDirector = camInterface::GetDebugDirector();
	const Vector3 vOrigin = debugDirector.IsFreeCamActive() ? debugDirector.GetFreeCamFrame().GetPosition() : CPlayerInfo::ms_cachedMainPlayerPos;

	for(int j=0; j<JUNCTIONS_MAX_NUMBER; j++)
	{
		CJunction & junction = m_aJunctions[j];
		if( junction.GetNumJunctionNodes()==0 || junction.GetJunctionNode(0).IsEmpty() || junction.GetTemplateIndex()==-1 || junction.GetErrorBindingJunction() )
			continue;

		const CPathNode * pJunctionNode = ThePaths.FindNodePointerSafe( junction.GetJunctionNode(0) );
		if (!pJunctionNode)
		{
			continue;
		}
		Vector3 vJunctionPos;
		pJunctionNode->GetCoors(vJunctionPos);

		if( (vOrigin - vJunctionPos).Mag2() > 100.0f*100.0f)
			continue;

		for(int e=0; e<junction.GetNumEntrances(); e++)
		{
			CJunctionEntrance & entrance = junction.GetEntrance(e);
			const CPathNode * pEntranceNode = ThePaths.FindNodePointerSafe(entrance.m_Node);
			if(pEntranceNode)
			{
				// We have the entrance node.
				// Now we need to find the link to one of the junction nodes.
				for(int jn=0; jn<junction.GetNumJunctionNodes(); jn++)
				{
					pJunctionNode = ThePaths.FindNodePointerSafe( junction.GetJunctionNode(jn) );
					if(pJunctionNode)
					{
						s16 iLinkIndex;
						if( ThePaths.FindNodesLinkIndexBetween2Nodes( pEntranceNode->m_address, pJunctionNode->m_address, iLinkIndex ) )
						{
							const CPathNodeLink & link = ThePaths.GetNodesLink( pEntranceNode, iLinkIndex );

							const int iNumLanes = link.m_1.m_LanesToOtherNode;

							for(int l=0; l<iNumLanes; l++)
							{
								Vector3 vLaneOffset( link.InitialLaneCenterOffset() + ( link.GetLaneWidth() * l ), 0.0f, 0.0f );

								float fEntranceOrientation = fwAngle::GetRadianAngleBetweenPoints(entrance.m_vEntryDir.x, entrance.m_vEntryDir.y, 0.0f, 0.0f);
								Matrix34 rotMat;
								rotMat.Identity();
								rotMat.MakeRotateZ(fEntranceOrientation);
								rotMat.Transform(vLaneOffset);

								Vector3 vEntryDir( entrance.m_vEntryDir.x, entrance.m_vEntryDir.y, 0.0f );
								Vector3 vLightPos = entrance.m_vPositionOfNode + (vEntryDir * entrance.m_EntryStopDistance);
								vLightPos -= vLaneOffset;

								// We should now have a position directly on the stopping position for this lane

								static const float fAmberTime = ((float)LIGHTDURATION_AMBER) / 1000.0f;

								const float fTimeLeft = junction.GetLightTimeRemaining();
								eTrafficLightColour iLight = LIGHT_RED;

								if( l == 0 )
								{
									if( entrance.m_iLeftFilterPhase == -1 && junction.GetLightPhase() == entrance.m_iPhase )
									{
										iLight = (fTimeLeft < fAmberTime) ? LIGHT_AMBER : LIGHT_GREEN;
									}
									else if ( entrance.m_iLeftFilterPhase != -1 && entrance.m_iLeftFilterPhase == junction.GetLightPhase() )
									{
										iLight = (fTimeLeft < fAmberTime) ? LIGHT_AMBER : LIGHT_GREEN;
									}

								}
								else if( junction.GetLightPhase() == entrance.m_iPhase )
								{
									iLight = (fTimeLeft < fAmberTime) ? LIGHT_AMBER : LIGHT_GREEN;
								}
	
								Color32 iLightCol;
								bool bSolid = true;
								if (l == iNumLanes-1 && entrance.m_bRightLaneIsRightOnly)
								{
									bSolid = false;
								}

								if( l == iNumLanes-1 && entrance.m_bCanTurnRightOnRedLight )
								{
									// Display slightly darker for filter-right
									switch(iLight)
									{
										default:
										case LIGHT_GREEN:
											iLightCol = Color32(0, 110, 0);
											break;
										case LIGHT_AMBER:
											iLightCol = Color32(110, 50, 0);
											break;
										case LIGHT_RED:
											iLightCol = Color32(110, 0, 0);
											break;
									}
								}
								else
								{
									// Display slightly darker for filter-right
									switch(iLight)
									{
										default:
										case LIGHT_GREEN:
											iLightCol = Color32(0, 255, 0);
											break;
										case LIGHT_AMBER:
											iLightCol = Color32(255, 128, 0);
											break;
										case LIGHT_RED:
											iLightCol = Color32(255, 0, 0);
											break;
									}
								}
								

								grcDebugDraw::Sphere( vLightPos + Vector3(0.0f,0.0f,4.0f), 0.3f, iLightCol, bSolid );
							}

							// We've found the link from this entrance into the junction; quit the loop
							break;
						}
					}
				}
			}
		}
	}
}


#endif

#if __JUNCTION_EDITOR

//--------------------------------------------------------------------------
// GetJunctionTemplateAtPosition
// The position passed in must be the location of one of the junction nodes

CJunctionTemplate * CJunctions::GetJunctionTemplateAtPosition(const Vector3 & vPos)
{
	for(s32 j=0; j<CJunctions::GetNumJunctionTemplates(); j++)
	{
		if(m_JunctionTemplates->Get(j).m_iFlags & CJunctionTemplate::Flag_NonEmpty)
		{
			for(s32 n=0; n<m_JunctionTemplates->Get(j).m_iNumJunctionNodes; n++)
			{
				if(m_JunctionTemplates->Get(j).m_vJunctionNodePositions[n].IsClose(vPos, 0.25f))
					return &m_JunctionTemplates->Get(j);
			}
		}
	}
	return NULL;
}
s32 CJunctions::GetJunctionUsingTemplate(const s32 iTemplate)
{
	for(int j=0; j<JUNCTIONS_MAX_NUMBER; j++)
	{
		if(m_aJunctions[j].GetNumJunctionNodes() > 0 && !m_aJunctions[j].GetJunctionNode(0).IsEmpty() && m_aJunctions[j].GetTemplateIndex()==iTemplate )
		{
			return j;
		}
	}
	return -1;
}

CAutoJunctionAdjustment& CJunctions::FindOrCreateAutoJunctionAdjustment(CJunction& junction)
{
	CAutoJunctionAdjustment* adj = FindAutoJunctionAdjustmentData(RCC_VEC3V(junction.GetJunctionCenter()));
	if (adj)
	{
		return *adj;
	}
	else
	{
		CAutoJunctionAdjustment newAdj;
		newAdj.m_vLocation = RCC_VEC3V(junction.GetJunctionCenter());
		newAdj.m_fCycleDuration = (junction.GetHasPedCrossingPhase() ? LIGHTDURATION_CYCLETIME_PED : LIGHTDURATION_CYCLETIME) / 1000.0f;
		newAdj.m_fCycleOffset = 0.0f;
		m_JunctionTemplates->m_AutoJunctionAdjustments.Push(newAdj);
		return m_JunctionTemplates->m_AutoJunctionAdjustments.Top();
	}
}

bool CJunctions::DeleteAutoJunctionAdjustment(CJunction& junction)
{
	CAutoJunctionAdjustment* adj = FindAutoJunctionAdjustmentData(RCC_VEC3V(junction.GetJunctionCenter()));
	if (adj)
	{
		int index = (int)(adj - m_JunctionTemplates->m_AutoJunctionAdjustments.begin());
		m_JunctionTemplates->m_AutoJunctionAdjustments.DeleteFast(index);
		return true;
	}
	return false;
}

#endif

#if __BANK
CJunctionTemplate * CJunctions::GetJunctionTemplateContainingEntrance(const Vector3 & vNodePos)
{
	for(s32 j=0; j<CJunctions::GetNumJunctionTemplates(); j++)
	{
		if(m_JunctionTemplates->Get(j).m_iFlags & CJunctionTemplate::Flag_NonEmpty)
		{
			for(s32 n=0; n<m_JunctionTemplates->Get(j).m_iNumEntrances; n++)
			{
				CJunctionTemplate::CEntrance & entrance = m_JunctionTemplates->Get(j).m_Entrances[n];

				float fEntranceNodeDistXZ = (vNodePos - entrance.m_vNodePosition).XYMag();
				if(fEntranceNodeDistXZ < CJunctions::ms_fEntranceNodePosEps &&
					IsClose(vNodePos.z, entrance.m_vNodePosition.z, 3.0f))
				{
					return &m_JunctionTemplates->Get(j);
				}
			}
		}
	}
	return NULL;
}
#endif

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : Update
// PURPOSE :  Updates the junctions. May also render debug stuff.
/////////////////////////////////////////////////////////////////////////////////
void	CJunctions::Update()
{
#if __BANK
	if(m_bDisableProcessing)
		return;
#endif

	PF_PUSH_TIMEBAR_DETAIL("Junctions:ScanAndInstance");

	CPed * pPlayer = CGameWorld::FindLocalPlayer();
	if(pPlayer && ms_bInstanceJunctionsAroundPlayer)
	{
		CVehicle *vehicle = pPlayer->GetVehiclePedInside();
		Vector3 velocity( 0.0f, 0.0f, 0.0f );

		// if we are in a ground vehicle, add the velocity of the player to the center of where we are going to update
		// just in case we are coming up on a junction really fast that doesn't yet have traffic around it.  This helps
		// solve the problem of traffic lights popping in a couple frames too late after they are already visible.
		if( vehicle && vehicle->GetIsLandVehicle() )
		{
			velocity = pPlayer->GetVelocity();
		}

		ScanAndInstanceJunctionsNearOrigin( VEC3V_TO_VECTOR3( pPlayer->GetTransform().GetPosition() ) + velocity );
	}

	PF_POP_TIMEBAR_DETAIL();

	PF_PUSH_TIMEBAR_DETAIL("Junctions:Update");

	// Only update junctions if the game isn't paused (used to be in CJunction::Update()).
	if(!fwTimer::IsGamePaused())
	{
		// Add this frame's time step to the array of update times per junction, and increase
		// the frame count for each junction.
		float timeStep = fwTimer::GetTimeStep();
		if( NetworkInterface::IsGameInProgress())
		{
			unsigned iNetworkTime = NetworkInterface::GetSyncedTimeInMilliseconds();			
			if(m_LastNetworkUpdateTime == 0 || iNetworkTime < m_LastNetworkUpdateTime)
			{
				m_LastNetworkUpdateTime = iNetworkTime;
			}
			timeStep = (float)(iNetworkTime - m_LastNetworkUpdateTime) / 1000.0f;
			m_LastNetworkUpdateTime = iNetworkTime;
		}
		else
		{
			m_LastNetworkUpdateTime = 0;
		}

		for(int i = 0; i < JUNCTIONS_MAX_NUMBER; i++)
		{
			m_TimesliceTimeSteps[i] += timeStep;
			m_TimesliceFramesSinceUpdate[i] = (u8)Min((int)m_TimesliceFramesSinceUpdate[i] + 1, 0xff);
		}

		if(m_TimesliceEnabled)
		{
			// Determine which junction we will start to update.
			int index = m_TimesliceUpdateIndex;

			int lastUpdateIndex = -1;

			// Determine how many junctions with vehicles that we should update. This is the number
			// of total junctions with vehicles divided by the update period, rounded up.
			int numRemainingUpdates = Max((m_TimesliceJunctionsInUse + m_TimesliceUpdatePeriod - 1)/m_TimesliceUpdatePeriod, 1);

			// We will always consider something about each junction, so count from 0 to JUNCTIONS_MAX_NUMBER.
			for(int cnt = 0; cnt < JUNCTIONS_MAX_NUMBER; cnt++)
			{
				bool shouldUpdate = false;
				bool countAsUpdate = false;

				// Check if we have any full updates left this frame.
				if(numRemainingUpdates > 0)
				{
					// Update this junction, and count it as an actual update if it had vehicles.
					shouldUpdate = true;
					countAsUpdate = m_aJunctions[index].MayHaveVehicles();
				}
				// Check if it's been too long since this junction got any sort of update
				// (this also includes if it had a vehicle added).
				else if(m_TimesliceFramesSinceUpdate[index] >= m_TimesliceUpdatePeriod)
				{
					shouldUpdate = true;
				}

				// Do the update if we decided to do so.
				if(shouldUpdate)
				{
#if (__BANK) && (DEBUG_DRAW)
					if(m_bDebugTimeslicing)
					{
						Color32 col;
						if(m_aJunctions[index].MayHaveVehicles())
						{
							col = Color_yellow;
						}
						else
						{
							col = Color_DarkGreen;
						}
						grcDebugDraw::Sphere(m_aJunctions[index].GetJunctionCenter(), 5.0f, col, false, -1);
					}
#endif	// (__BANK) && (DEBUG_DRAW)

					// Pass in the time step we have accumulated in the array for this junction,
					// to make sure we account for all time that has elapsed, regardless of when it
					// last updated, and reset the frame count and update time.
					m_aJunctions[index].Update(m_TimesliceTimeSteps[index]);
					m_TimesliceTimeSteps[index] = 0.0f;
					m_TimesliceFramesSinceUpdate[index] = 0;
				}

				// Check if we still have any full updates left (not counting the one we may have just done).
				if(numRemainingUpdates > 0)
				{
					lastUpdateIndex = index;

					// Count how many junctions have vehicles.
					if(m_aJunctions[index].MayHaveVehicles())
					{
						m_TimesliceJunctionsInUseCounting++;
					}

					// If we are about to wrap around, copy m_TimesliceJunctionsInUseCounting
					// to m_TimesliceJunctionsInUse, to determine how many junctions with vehicles
					// we should update on the following frames.
					if(index == JUNCTIONS_MAX_NUMBER - 1)
					{
						m_TimesliceJunctionsInUse = m_TimesliceJunctionsInUseCounting;
						m_TimesliceJunctionsInUseCounting = 0;
					}

					// If this junction had vehicles, decrement the number of updates we can do.
					if(countAsUpdate)
					{
						numRemainingUpdates--;
					}
				}

				// Increase the junction index and let it wrap if needed.
				index++;
				if(index == JUNCTIONS_MAX_NUMBER)
				{
					index = 0;
				}
			}

			// Compute where we will start doing full updates on the next frame.
			int nextFirstUpdateIndex = lastUpdateIndex + 1;
			if(nextFirstUpdateIndex == JUNCTIONS_MAX_NUMBER)
			{
				nextFirstUpdateIndex = 0;
			}
			m_TimesliceUpdateIndex = nextFirstUpdateIndex;
		}
		else
		{
			m_TimesliceUpdateIndex = 0;
			for (s32 i = 0; i < JUNCTIONS_MAX_NUMBER; i++)
			{
				m_aJunctions[i].Update(m_TimesliceTimeSteps[i]);
				m_TimesliceTimeSteps[i] = 0.0f;
				m_TimesliceFramesSinceUpdate[i] = 0;
			}
		}
	}

	PF_POP_TIMEBAR_DETAIL();

// 	PF_PUSH_TIMEBAR_DETAIL("Junctions:UpdateJunctionSampling");
// 	CVirtualRoad::UpdateJunctionSampling();
// 	PF_POP_TIMEBAR_DETAIL();
}

void CJunctions::OnScriptTerminate(scrThreadId iScriptThreadId)
{
	for(int j=0; j<JUNCTIONS_MAX_NUMBER; j++)
	{
		if(m_aJunctions[j].GetRequiredByScript() == iScriptThreadId)
		{
			m_aJunctions[j].SetRequiredByScript(THREAD_INVALID);
		}
	}
}


Vector3 g_vScanJunctionNodeOrigin;
CNodeAddress g_NearbyJunctionsToInstanciate[JUNCTIONS_MAX_NUMBER];
int g_iMaxNumNearbyJunctions = JUNCTIONS_MAX_NUMBER;
int g_iNumNearbyJunctions = 0;

bool CJunctions::ScanJunctionNodeFn(CPathNode * pNode, void * UNUSED_PARAM(pData))
{
	//don't instance switched off junction nodes solely based on distance.
	//there are parking garages with a really high density of junctions
	//that can blow our junction pool (currently 56)

	if(pNode->IsJunctionNode())
	{
		Vector3 vNodePos;
		pNode->GetCoors(vNodePos);
		if((vNodePos-g_vScanJunctionNodeOrigin).Mag2() < CJunctions::ms_fInstanceJunctionDist*CJunctions::ms_fInstanceJunctionDist)
		{
			if(!CJunctions::FindJunctionFromNode(pNode->m_address))
			{
				g_NearbyJunctionsToInstanciate[g_iNumNearbyJunctions++] = pNode->m_address;
				if(g_iNumNearbyJunctions >= g_iMaxNumNearbyJunctions)
					return false;
			}
		}
	}
	return true;
}

void CJunctions::ScanAndInstanceJunctionsNearOrigin(const Vector3 & vOrigin)
{
	PF_FUNC(ScanAndInstanceJunctionsNearOrigin);

	m_fTimeSinceLastScan += fwTimer::GetTimeStep();
	if(m_fTimeSinceLastScan < ms_fScanFrequency)
		return;

	m_fTimeSinceLastScan = 0.0f;

	if(ms_fInstanceJunctionDist > 0.0f)
	{
		//----------------------------------------------------------------------
		// Clear out any deserted junctions which are outside of our scan range

		for (int i = 0; i < JUNCTIONS_MAX_NUMBER; i++)
		{
			if (m_aJunctions[i].GetNumJunctionNodes() > 0 &&
				m_aJunctions[i].GetNumberOfCars() == 0 &&
				!m_aJunctions[i].GetRailwayBarriersShouldBeDown() &&
				(vOrigin-m_aJunctions[i].GetJunctionCenter()).Mag2() > ms_fInstanceJunctionDist*ms_fInstanceJunctionDist)
			{
				m_aJunctions[i].Clear();
			}
		}

		g_vScanJunctionNodeOrigin = vOrigin;
		g_iNumNearbyJunctions = 0;
		g_iMaxNumNearbyJunctions = JUNCTIONS_MAX_NUMBER - CountNumJunctionsInUse();

		if(g_iMaxNumNearbyJunctions == 0)	// No space for any more junctions?
			return;

		const Vector3 vMin = vOrigin - Vector3(ms_fInstanceJunctionDist, ms_fInstanceJunctionDist, ms_fInstanceJunctionDist);
		const Vector3 vMax = vOrigin + Vector3(ms_fInstanceJunctionDist, ms_fInstanceJunctionDist, ms_fInstanceJunctionDist);

		ThePaths.ForAllNodesInArea(vMin, vMax, ScanJunctionNodeFn, NULL);

		for(int j=0; j<g_iNumNearbyJunctions; j++)
		{
			const CNodeAddress & junctionNode = g_NearbyJunctionsToInstanciate[j];
			Assert(!junctionNode.IsEmpty());

			// Since templated junctions may contain multiple 'J' junction nodes, ensure
			// we haven't already created this junction during this loop - we might have gathered
			// multiple nodes belonging to the same junction up above.

			if(!FindJunctionFromNode(junctionNode))
			{
				for (int i = 0; i < JUNCTIONS_MAX_NUMBER; i++)
				{
					if(m_aJunctions[i].GetNumJunctionNodes() == 0)
					{
						m_aJunctions[i].Deploy(junctionNode);
						m_TimesliceTimeSteps[i] = 0.0f;
						m_TimesliceFramesSinceUpdate[i] = 0;
						break;
					}
				}
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : RemoveVehicleFromJunction
// PURPOSE :  Removes the vehicle from the junction it belongs to.
/////////////////////////////////////////////////////////////////////////////////

void	CJunctions::RemoveVehicleFromJunction(CVehicle *pVeh, CNodeAddress nodeAddress)
{
	for (s32 i = 0; i < JUNCTIONS_MAX_NUMBER; i++)
	{
		if (m_aJunctions[i].ContainsJunctionNode(nodeAddress))
		{
			m_aJunctions[i].RemoveVehicle(pVeh);
			return;
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : AddVehicleToJunction
// PURPOSE :  Adds the vehicle to the junction it belongs to.
//			  If junction doesn't exist already it is created.
/////////////////////////////////////////////////////////////////////////////////

CJunction*	CJunctions::AddVehicleToJunction(CVehicle *pVeh, CNodeAddress nodeAddress, CJunction* pJunctionIfKnown)
{
	PF_FUNC(AddVehicleToJunction);

	if(pJunctionIfKnown)
	{
		Assert(pJunctionIfKnown->ContainsJunctionNode(nodeAddress));
		Assert(FindJunctionFromNode(nodeAddress) == pJunctionIfKnown);
		pJunctionIfKnown->AddVehicle(pVeh);
		return pJunctionIfKnown;
	}

	CPed * pPlayer = CGameWorld::FindLocalPlayer();
	Assert(pPlayer);

	const Vector3 vPlayerPos = VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition());
	s32 i;

	//-----------------------------------------------------------
	// See if we already have a junction instanced for this node

#if __ASSERT
	for (i = 0; i < JUNCTIONS_MAX_NUMBER; i++)
	{
		CJunction& junction = m_aJunctions[i];
		if (junction.ContainsJunctionNode(nodeAddress))
		{
			Assertf(0, "Adding vehicle to junction that already exists, but without passing in a junction pointer.");
			break;
		}
	}
#endif	// __ASSERT

	//--------------------------------------------------------------------
	// Try to replace an unallocated junction (with zero junction nodes)

	const u32 iCurrTime = fwTimer::GetTimeInMilliseconds();

	u32 iMaxTimeDelta = 0;
	int iBestJunction = -1;

	for (i = 0; i < JUNCTIONS_MAX_NUMBER; i++)
	{
		if (m_aJunctions[i].GetNumJunctionNodes()==0)
		{
			const u32 iDelta = iCurrTime - m_aJunctions[i].GetLastUsedTime();
			if(iDelta >= iMaxTimeDelta)
			{
				iMaxTimeDelta = iDelta;
				iBestJunction = i;
			}
		}
	}
	if(iBestJunction != -1)
	{
		CJunction& junction = m_aJunctions[iBestJunction];
		junction.Clear();
		if(junction.Deploy(nodeAddress))
		{
			m_TimesliceTimeSteps[iBestJunction] = 0.0f;
			m_TimesliceFramesSinceUpdate[iBestJunction] = 0;
			junction.AddVehicle(pVeh);
			return &junction;
		}
	}

	//------------------------------------------------------------------
	// Try to replace the furthest empty junction (with zero vehicles)

	float fMaxDistSqr = -FLT_MAX;
	iBestJunction = -1;

	for (i = 0; i < JUNCTIONS_MAX_NUMBER; i++)
	{
		if (m_aJunctions[i].GetNumberOfCars() == 0)
		{
			const float fDistSqr = (vPlayerPos-m_aJunctions[i].GetJunctionCenter()).Mag2();
			if(fDistSqr > fMaxDistSqr)
			{
				fMaxDistSqr = fDistSqr;
				iBestJunction = i;
			}
		}
	}

	Assertf(fMaxDistSqr >= 0.0f, "Unable to find any empty junction to add new car to");

	if(fMaxDistSqr > ms_fInstanceJunctionDist*ms_fInstanceJunctionDist)
	{
		CJunction& junction = m_aJunctions[iBestJunction];
		junction.Clear();
		if(junction.Deploy(nodeAddress))
		{
			m_TimesliceTimeSteps[iBestJunction] = 0.0f;
			m_TimesliceFramesSinceUpdate[iBestJunction] = 0;
			junction.AddVehicle(pVeh);
			return &junction;
		}
#if __ASSERT
		else
		{
			Assertf(0, "Found junction far enough away, but CJunction::Deploy failed");
		}
#endif //__ASSERT
	}

	//Assertf(fMaxDistSqr < 0.0f, "Found an empty junction, but it's not further away than ms_fInstanceJunctionDist (required2: %.2f, actual2: %.2f)"
	//	, ms_fInstanceJunctionDist*ms_fInstanceJunctionDist, fMaxDistSqr);

	return NULL;
}

bool CJunctions::CreateTemplatedJunctionForScript(scrThreadId iScriptThreadId, const Vector3 & vJunctionPos)
{
	int iExistingJunction = GetJunctionAtPosition(vJunctionPos);
	if(!iExistingJunction)
	{
		int t;
		for(t=0; t<JUNCTIONS_MAX_TEMPLATES; t++)
		{
			CJunctionTemplate & junc = m_JunctionTemplates->Get(t);
			Vector3 vMid = (junc.m_vJunctionMin+junc.m_vJunctionMax)*0.5f;
			if( (vMid - vJunctionPos).Mag2() < ms_fScriptCommandJunctionProximitySqr)
			{
				break;
			}
		}
		Assertf(t != JUNCTIONS_MAX_TEMPLATES, "No junction template exists for a junction at this location.");
		if(t == JUNCTIONS_MAX_TEMPLATES)
			return false;

		int j;
		for(j=0; j<JUNCTIONS_MAX_NUMBER; j++)
		{
			if(m_aJunctions[j].GetNumJunctionNodes()==0)
				break;
		}
		Assertf(j != JUNCTIONS_MAX_NUMBER, "No free junctions available - all are in use.");
		if(j == JUNCTIONS_MAX_NUMBER)
			return false;

		CJunctionTemplate & junc = m_JunctionTemplates->Get(t);
		Vector3 vNodePos = junc.m_vJunctionNodePositions[0];
		CNodeAddress nodeAddr = ThePaths.FindNodeClosestToCoors(vNodePos, PathfindFindJunctionNodeCB, NULL, 10.0f);

		const CPathNode * pJunctionNode = ThePaths.FindNodePointerSafe(nodeAddr);
		Assertf(pJunctionNode, "Road nodes are not loaded at the location of this junction.");
		if(!pJunctionNode)
			return false;

		m_aJunctions[j].Deploy(nodeAddr);
		m_TimesliceTimeSteps[j] = 0.0f;
		m_TimesliceFramesSinceUpdate[j] = 0;
		m_aJunctions[j].SetRequiredByScript(iScriptThreadId);
		return true;
	}

	CJunction * pJunction = GetJunctionByIndex(iExistingJunction);
	Assert(pJunction);
	if(!pJunction)
		return false;

	Assertf(pJunction->GetTemplateIndex()!=-1, "Junction is not templated");
	if(pJunction->GetTemplateIndex()==-1)
		return false;

	return true;
}

s32 CJunctions::GetLightStatusForPed(const Vector3 & vJunctionSearchPosition, const Vector3 & vCrossingDir, bool bConsiderTimeRemaining, int& cachedJunctionIndex)
{
	// We are determining which junction index to use, if any
	int iJunctionIndex = -1;

	// If a valid cached junction index was passed in
	if( cachedJunctionIndex != -1 )
	{
		// then use it directly
		iJunctionIndex = cachedJunctionIndex;
	}

	// If no junction index is set yet
	if( iJunctionIndex == -1 )
	{
		// Find the closest junction index
		const float fSearchDistanceSq = 80.0f * 80.0f;
		dev_bool bIgnoreFakeJunctions = true;
		iJunctionIndex = CJunctions::GetJunctionAtPositionUsingJunctionMinMax(vJunctionSearchPosition, fSearchDistanceSq, bIgnoreFakeJunctions);
	}

	// Check to see if a template junction was set
	if( iJunctionIndex != -1 && m_aJunctions[iJunctionIndex].GetTemplateIndex() != -1 )
	{
		// update cached variable
		cachedJunctionIndex = iJunctionIndex;

		// Return the light status for the given ped for this template junction
		return m_aJunctions[iJunctionIndex].GetLightStatusForPedCrossing(bConsiderTimeRemaining);
	}

	// at this point we either have no junction or we have a non-template junction:

	// Set safe time ratio as appropriate
	float fSafeTimeRatio = 0.0f;
	if( bConsiderTimeRemaining )
	{
		fSafeTimeRatio = CJunction::ms_fDurationRatioForSafeCrossing;
	}

	// Check if a non-template junction was set
	if( iJunctionIndex != -1 )
	{
		// Get the junction for the index
		CJunction* pJunction = CJunctions::GetJunctionByIndex(iJunctionIndex);
		if(pJunction)
		{
			// update output variable
			cachedJunctionIndex = iJunctionIndex;
			
			// Return the light status for the given crossing direction and this auto-junction
			return CTrafficLights::LightForPeds(pJunction->GetAutoJunctionCycleOffset(), pJunction->GetAutoJunctionCycleScale(), vCrossingDir.x, vCrossingDir.y, pJunction->GetHasPedCrossingPhase(), fSafeTimeRatio);
		}
	}
	
	// Otherwise we have no junction and will fall back to the global traffic light pattern:
	
	// update output variable
	cachedJunctionIndex = -1;
	
	// Return the global default light status for the given crossing direction
	return CTrafficLights::LightForPeds(0, 1.0f, vCrossingDir.x, vCrossingDir.y, false, fSafeTimeRatio);
}

bool CJunctions::IsPedNearToJunction(const Vector3 & vPedPos)
{
	s32 j;
	for(j=0; j<JUNCTIONS_MAX_NUMBER; j++)
	{
		if(m_aJunctions[j].GetNumEntrances() > 0)
		{
			if((m_aJunctions[j].GetJunctionCenter() - vPedPos).XYMag2() < ms_fScriptCommandJunctionProximitySqr)
			{
				return true;
			}
		}
	}

	return false;
}

CJunction * CJunctions::FindJunctionFromNode(const CNodeAddress & junctionNode)
{
	if(junctionNode.IsEmpty())
		return NULL;

	CPathNode * pNode = ThePaths.FindNodePointerSafe(junctionNode);

	if(pNode && !pNode->IsJunctionNode())
	{
		return NULL;
	}

	s32 j;
	for(j=0; j<JUNCTIONS_MAX_NUMBER; j++)
	{
		if(m_aJunctions[j].ContainsJunctionNode(junctionNode))
			return &m_aJunctions[j];
	}
	return NULL;
}

s32 CJunctions::GetJunctionAtPosition(const Vector3 & vPos, float maxDistSqr, bool bIgnoreFakeJunctions)
{
	int iClosest = -1;
	float fLeastDistSqr = FLT_MAX;
	
	for(int j=0; j<JUNCTIONS_MAX_NUMBER; j++)
	{
		if(bIgnoreFakeJunctions && m_aJunctions[j].IsOnlyJunctionBecauseHasSwitchedOffEntrances() )
		{
			continue;
		}

		if( m_aJunctions[j].GetNumJunctionNodes() > 0 && !m_aJunctions[j].GetJunctionNode(0).IsEmpty() )
		{
			const float fDistSqr = (m_aJunctions[j].GetJunctionCenter()-vPos).Mag2();

			if(fDistSqr < maxDistSqr && fDistSqr < fLeastDistSqr)
			{
				iClosest = j;
				fLeastDistSqr = fDistSqr;
			}
		}
	}

	return iClosest;
}

s32 CJunctions::GetJunctionAtPositionUsingJunctionMinMax(const Vector3& vPos, float fMaxDistSqr, bool bIgnoreFakeJunctions)
{
	int iClosest = -1;
	float fLeastDistSqr = FLT_MAX;
	
	for(int j=0; j<JUNCTIONS_MAX_NUMBER; j++)
	{
		if(bIgnoreFakeJunctions && m_aJunctions[j].IsOnlyJunctionBecauseHasSwitchedOffEntrances())
		{
			continue;
		}

		if(m_aJunctions[j].GetNumJunctionNodes() > 0 && !m_aJunctions[j].GetJunctionNode(0).IsEmpty())
		{
			const Vector3& vMin = m_aJunctions[j].GetJunctionMin();
			const Vector3& vMax = m_aJunctions[j].GetJunctionMax();

			if(vPos.x >= vMin.x && vPos.x <= vMax.x && vPos.y >= vMin.y && vPos.y <= vMax.y)
			{
				iClosest = j;
				break;
			}

			const Vector3 vPosToMin = vPos - vMin;
			const Vector3 vPosToMax = vPos - vMax;

			const float fXDist = Min(Abs(vPosToMin.x), Abs(vPosToMax.x));
			const float fYDist = Min(Abs(vPosToMin.y), Abs(vPosToMax.y));

			const float fDistSqr = (fXDist * fXDist) + (fYDist * fYDist);

			if(fDistSqr < fMaxDistSqr && fDistSqr < fLeastDistSqr)
			{
				iClosest = j;
				fLeastDistSqr = fDistSqr;
			}
		}
	}

	return iClosest;
}

s32 CJunctions::GetJunctionAtPositionForTrafficLight(const Vector3 & vPos, const Vector3 & vDir, bool isSingleLight)
{
	int iClosest = -1;
	float fLeastDistSqr = 500.0f * 500.0f; // made up number.
	atRangeArray<int,4> entranceIds; // We deal with a maximum of 4 entrances per junction/traffic lights.
	
	for(int j=0; j<JUNCTIONS_MAX_NUMBER; j++)
	{
		if( m_aJunctions[j].HasTrafficLightNodes() )
		{
			if( m_aJunctions[j].GetNumJunctionNodes() > 0 && !m_aJunctions[j].GetJunctionNode(0).IsEmpty() )
			{
				const float fDistSqr = (m_aJunctions[j].GetJunctionCenter()-vPos).Mag2();
				const float fDistSrch = m_aJunctions[j].GetTrafficLightSearchDistance();
				const float fDistSrchSqr = fDistSrch*fDistSrch;
				
				int templateIdx = m_aJunctions[j].GetTemplateIndex();
				bool isWithinDist = (fDistSqr < fDistSrchSqr) && (fDistSqr < fLeastDistSqr) && (abs(vPos.z - m_aJunctions[j].GetJunctionCenter().z) < 4.0f);
				
				if(templateIdx == -1 && isWithinDist && m_aJunctions[j].HasTrafficLightNodes() )
				{
					if( m_aJunctions[j].FindTrafficLightEntranceIds(vDir,isSingleLight,entranceIds) != 0 )
					{
						iClosest = j;
						fLeastDistSqr = fDistSqr;
					}
				}
				else if (templateIdx != -1)
				{
					if(fDistSqr < fDistSrchSqr*4.0f)
					{
						const CJunctionTemplate &junctionTemplate = CJunctions::GetJunctionTemplate(templateIdx);
						
						for(int i=0;i<junctionTemplate.m_iNumTrafficLightLocations;i++)
						{
							Vector3 tlPos;
							junctionTemplate.m_TrafficLightLocations[i].GetAsVec3(tlPos);
							if( (tlPos-vPos).Mag2() <= 6.0f * 6.0f )
							{
								return j;
							}
						}
					}
					
					if(isWithinDist) 
					{
						if( m_aJunctions[j].FindTrafficLightEntranceIds(vDir,isSingleLight,entranceIds) != 0 )
						{
							iClosest = j;
							fLeastDistSqr = fDistSqr;
						}
					}
				}
			}
		}
	}

	return iClosest;
}

s32 CJunctions::GetJunctionAtPositionForRailwayCrossing(const Vector3 & vPos)
{
	int iClosest = -1;
	float fLeastDistSqr = FLT_MAX;
	const float fMaxDistSqr = 500.0f;

	for(int j=0; j<JUNCTIONS_MAX_NUMBER; j++)
	{
		if( m_aJunctions[j].GetIsRailwayCrossing() )
		{
			if( m_aJunctions[j].GetNumJunctionNodes() > 0 && !m_aJunctions[j].GetJunctionNode(0).IsEmpty() )
			{
				const float fDistSqr = (m_aJunctions[j].GetJunctionCenter()-vPos).Mag2();

				if(fDistSqr < fMaxDistSqr && fDistSqr < fLeastDistSqr)
				{
					iClosest = j;
					fLeastDistSqr = fDistSqr;
				}
			}
		}
	}

	return iClosest;
}

s32 CJunctions::GetNumJunctionTemplates()
{
	return m_JunctionTemplates->m_Entries.GetCount();
}
s32 CJunctions::GetMaxNumJunctionTemplates()
{
	return m_JunctionTemplates->m_Entries.GetMaxCount();
}

CJunction *	CJunctions::GetJunctionByIndex(s32 i)
{
	Assertf(i >= 0 && i <JUNCTIONS_MAX_NUMBER, "Junction index %i is out of range", i);
	if(i >= 0 && i <JUNCTIONS_MAX_NUMBER)
	{
		if(m_aJunctions[i].GetNumJunctionNodes()>0)
		{
			return &m_aJunctions[i];
		}
	}
	return NULL;
}

int CJunctions::CountNumJunctionsInUse()
{
	int iCount = 0;
	for(int i=0; i<JUNCTIONS_MAX_NUMBER; i++)
	{
		if(m_aJunctions[i].GetNumJunctionNodes() > 0)
			iCount++;
	}
	return iCount;
}

//	LoadJunctionTemplates
//	Use the pargen code to load all the junction templates for the current level.
//	This is called from CJunctions::Init(), in turn from InitSession

bool CJunctions::LoadJunctionTemplates()
{

	bool bLoadedOk = false;

	//-----------------------------------------------------------------
	// If we have a "junctions.pso" file, then attempt to load this

	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::JUNCTION_TEMPLATES_PSO_FILE);
	if(pData && DATAFILEMGR.IsValid(pData))
	{
		psoFile * pPsoFile = psoLoadFile(pData->m_filename, PSOLOAD_PREP_FOR_PARSER_LOADING, atFixedBitSet32().Set(psoFile::IGNORE_CHECKSUM));
		if(pPsoFile)
		{
			bLoadedOk = psoLoadObject(*pPsoFile, *m_JunctionTemplates);
			Assertf(bLoadedOk, "Error loading junctions.pso");
			delete pPsoFile;
		}
	}
	
	if(!bLoadedOk)
	{
		//----------------------------------------------
		// Otherwise look for a "junctions.xml" file

		pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::JUNCTION_TEMPLATES_FILE);
		if(pData && DATAFILEMGR.IsValid(pData))
		{
			parSettings settings = parSettings::sm_StandardSettings;
			settings.SetFlag(parSettings::PRELOAD_FILE, false);

			bLoadedOk = PARSER.LoadObject(pData->m_filename, "xml", *m_JunctionTemplates, &settings);
			Assertf(bLoadedOk, "Error loading junctions.xml");
		}
	}

	// Post-process
	if(bLoadedOk)
	{
		s32 iLoadedSize = m_JunctionTemplates->m_Entries.GetCount();
		m_JunctionTemplates->m_Entries.SetCount(JUNCTIONS_MAX_TEMPLATES);
		while(iLoadedSize < JUNCTIONS_MAX_TEMPLATES)
		{
			m_JunctionTemplates->m_Entries[iLoadedSize].m_iFlags = 0;
			iLoadedSize++;
		}
	}

	return bLoadedOk;
}

#if __JUNCTION_EDITOR

void CJunctions::RefreshAllJunctions()
{
	CVehiclePopulation::RemoveAllVehsHard();
	RemoveAllJunctions();
}

//	SaveJunctionTemplates
//	Write out junction templates using pargen kewlness
void CJunctions::EditorSaveJunctionTemplates()
{
	/*
	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::JUNCTION_TEMPLATES_FILE);
	if(pData && DATAFILEMGR.IsValid(pData))
	{
		// Check that file is writable?
		bool bSavedOk = PARSER.SaveObject(pData->m_filename, "xml", &m_JunctionTemplates);

		Assertf( bSavedOk, "\n\"%s\" did not save properly.\nThis is most likely because it is not writable\nPlease ensure you have it checked out in perforce, and try again\n", pData->m_filename );
	}
	*/

	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::JUNCTION_TEMPLATES_PSO_FILE);
	if(pData && DATAFILEMGR.IsValid(pData))
	{
		// Check that file is writable?
		Verifyf(psoSaveObject(pData->m_filename, m_JunctionTemplates), "\n\"%s\" did not save properly.\nThis is most likely because it is not writable\nPlease ensure you have it checked out in perforce, and try again\n", pData->m_filename );
	}
}

void CJunctions::EditorSaveJunctionTemplatesXml()
{
	// Check that file is writable?
	fiStream* stream = ASSET.Create(ms_JunctionEditorXmlFilename, "");
	if (stream)
	{
		parXmlWriterVisitor vis(stream);
		vis.Visit(*m_JunctionTemplates);
		stream->Close();
	}
	else
	{
		Assertf(0, "\n\"%s\" did not save properly.\nThis is most likely because it is not writable\nPlease ensure you have it checked out in perforce, and try again\n",  ms_JunctionEditorXmlFilename);
	}
}


void CJunctions::EditorLoadJunctionTemplates()
{
	bool bLoadedOk = false;

	//-----------------------------------------------------------------
	// If we have a "junctions.pso" file, then attempt to load this

	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::JUNCTION_TEMPLATES_PSO_FILE);
	if(pData && DATAFILEMGR.IsValid(pData))
	{
		psoFile * pPsoFile = psoLoadFile(pData->m_filename, PSOLOAD_PREP_FOR_PARSER_LOADING, atFixedBitSet32().Set(psoFile::REQUIRE_CHECKSUM));
		Assertf(pPsoFile, "Couldn't locate junctions.pso");
		if(pPsoFile)
		{
			bLoadedOk = psoLoadObject(*pPsoFile, *m_JunctionTemplates);
			Assertf(bLoadedOk, "Error loading junctions.pso");
			delete pPsoFile;
		}
	}
	else
	{
		Assertf(false, "Couldn't locate junctions.pso");
	}

	// Post-process
	if(bLoadedOk)
	{
		s32 iLoadedSize = m_JunctionTemplates->m_Entries.GetCount();
		m_JunctionTemplates->m_Entries.SetCount(JUNCTIONS_MAX_TEMPLATES);
		while(iLoadedSize < JUNCTIONS_MAX_TEMPLATES)
		{
			m_JunctionTemplates->m_Entries[iLoadedSize].m_iFlags = 0;
			iLoadedSize++;
		}
	}
}

void CJunctions::EditorLoadJunctionTemplatesXml()
{
	parTree* tree = NULL;
	{
		USE_DEBUG_MEMORY();
		tree = PARSER.LoadTree(CJunctions::ms_JunctionEditorXmlFilename, "");
	}

	bool bLoadedOk = tree && tree->GetRoot() && PARSER.LoadObject(tree->GetRoot(), *m_JunctionTemplates);

	{
		USE_DEBUG_MEMORY();
		delete tree;
	}

	// Post-process
	if(bLoadedOk)
	{
		s32 iLoadedSize = m_JunctionTemplates->m_Entries.GetCount();
		m_JunctionTemplates->m_Entries.SetCount(JUNCTIONS_MAX_TEMPLATES);
		while(iLoadedSize < JUNCTIONS_MAX_TEMPLATES)
		{
			m_JunctionTemplates->m_Entries[iLoadedSize].m_iFlags = 0;
			iLoadedSize++;
		}
	}
	else
	{
		Assertf(0, "\n\"%s\" did not load properly.\nIs the filename correct?\n",  ms_JunctionEditorXmlFilename);
	}
}

#endif //__JUNCTION_EDITOR

s32 CJunctions::BindJunctionTemplate(CJunction & junction, const CNodeAddress & junctionNode, CJunction::sEntryNodeInfo * entryInfos, const s32 iMaxEntryInfos
									 , const CPathFind& pathData)
{
	if(junctionNode.IsEmpty() || !pathData.IsRegionLoaded(junctionNode))
		return 0;

	const CPathNode * pInputJunctionNode = pathData.FindNodePointerSafe(junctionNode);
	if(!pInputJunctionNode)
		return 0;

	Vector3 vNodeCoords;
	pInputJunctionNode->GetCoors(vNodeCoords);

	//-----------------------------------------------------
	// Match template by looking for central junction node

	s32 t,n;
	bool bFoundJunction = false;

	for(t=0; t<CJunctions::GetNumJunctionTemplates(); t++)
	{
		CJunctionTemplate & temp = m_JunctionTemplates->Get(t);
		if(temp.m_iFlags & CJunctionTemplate::Flag_NonEmpty)
		{
			for(n=0; n<temp.m_iNumJunctionNodes; n++)
			{
				if(temp.m_vJunctionNodePositions[n].IsClose(vNodeCoords, ms_fJunctionNodePosEps))
				{
					bFoundJunction = true;
					break;
				}
			}
			if(bFoundJunction)
				break;
		}
	}
	if(!bFoundJunction)
		return 0;


	//------------------------
	// Get junction template

	CJunctionTemplate & temp = m_JunctionTemplates->Get(t);

	//---------------------------------------------------------
	// Ensure that all the nodes for this junction are loaded

	if( !pathData.AreNodesLoadedForArea(temp.m_vJunctionMin.x, temp.m_vJunctionMax.x, temp.m_vJunctionMin.y, temp.m_vJunctionMax.y) )
	{
		return 0;
	}

	//------------------------------------------------
	// Now attempt to match up all the junction nodes

	for(n=0; n<temp.m_iNumJunctionNodes; n++)
	{
		CNodeAddress iNode = pathData.FindNodeClosestToCoors(temp.m_vJunctionNodePositions[n], PathfindFindJunctionNodeCB, NULL, ms_fJunctionNodePosEps);
		if(!iNode.IsEmpty())
		{
			junction.SetJunctionNode(n, iNode);
		}
		else
		{
			Assertf(false, "ERROR - couldn't bind all junction nodes from template [%i] at (%.1f, %.1f, %.1f)", t, temp.m_vJunctionNodePositions[n].x, temp.m_vJunctionNodePositions[n].y, temp.m_vJunctionNodePositions[n].z);
			junction.SetErrorBindingJunction(true);
			return 0;
		}
	}

	junction.SetIsRailwayCrossing((temp.m_iFlags & CJunctionTemplate::Flag_RailwayCrossing)!=0);
	junction.SetCanSkipPedPhase((temp.m_iFlags & CJunctionTemplate::Flag_DisableSkipPedLightPhase)==0);

	junction.SetNumJunctionNodes(temp.m_iNumJunctionNodes);
	junction.SetNumEntrances(temp.m_iNumEntrances);
	junction.SetNumLightPhases(temp.m_iNumPhases);

	Assert(temp.m_iNumEntrances < iMaxEntryInfos);

	junction.SetJunctionCenter((temp.m_vJunctionMin + temp.m_vJunctionMax)*0.5f);

	//------------------------------------------------
	// Now attempt to match up all the entrance nodes

	s32 e;
	for(e=0; e<temp.m_iNumEntrances && e<iMaxEntryInfos; e++)
	{
		CJunctionTemplate::CEntrance & entrance = temp.m_Entrances[e];

		CNodeAddress entNode = pathData.FindNodeClosestToCoors(entrance.m_vNodePosition, 10.0f);
		if(entNode.IsEmpty())
		{
			Assertf(false, "Couldn't match entrance node %i for junction template [%i] at (%.1f, %.1f, %.1f). Road node has been moved.", e, t, junction.GetJunctionCenter().x, junction.GetJunctionCenter().y, junction.GetJunctionCenter().z);
			junction.SetErrorBindingJunction(true);
			return 0;
		}
		if(!pathData.IsRegionLoaded(entNode))
		{
			Assertf(false, "Entrance node %i for junction template [%i] at (%.1f, %.1f, %.1f) is not loaded", e, t, junction.GetJunctionCenter().x, junction.GetJunctionCenter().y, junction.GetJunctionCenter().z);
			junction.SetErrorBindingJunction(true);
			return 0;
		}
		const CPathNode * pEntNode = pathData.FindNodePointerSafe(entNode);
		Assert(pEntNode);

		Vector3 vEntPos;
		pEntNode->GetCoors(vEntPos);

		float fEntranceNodeDistXZ = (vEntPos - entrance.m_vNodePosition).XYMag();
		if(fEntranceNodeDistXZ > ms_fEntranceNodePosEps)
		{
#if 1 //AI_OPTIMISATIONS_OFF
			Assertf(false, "Entrance road node %i doesn't match for junction template [%i]\nNode XYZ (%.2f, %.2f, %.2f), Entrance XYZ (%.2f, %.2f, %.2f).. That's %.2fm away.\nRoad node need to be returned to this position, or junction re-edited.",
				e, t,
				entrance.m_vNodePosition.x, entrance.m_vNodePosition.y, entrance.m_vNodePosition.z,
				vEntPos.x, vEntPos.y, vEntPos.z,
				fEntranceNodeDistXZ
				);
#endif
			junction.SetErrorBindingJunction(true);
			return 0;
		}

		entryInfos[e].EntryNode = entNode;
		entryInfos[e].vNodePos = vEntPos;

		for(n=0; n<junction.GetNumJunctionNodes(); n++)
		{
			const CNodeAddress iJuncNode = junction.GetJunctionNode(n);
			const CPathNode * pJunctionNode = pathData.FindNodePointerSafe(iJuncNode);

			s16 iLink = -1;
			if(pathData.FindNodesLinkIndexBetween2Nodes(iJuncNode, pEntNode->m_address, iLink) && iLink != -1)
			{
				const CPathNode * pConnectedJuncNode = pathData.FindNodePointerSafe(iJuncNode);
				if(pConnectedJuncNode)
				{
					Vector3 vConnectedJuncPos;
					pConnectedJuncNode->GetCoors(vConnectedJuncPos);

					cos_and_sin(entryInfos[e].EntryDir.y, entryInfos[e].EntryDir.x, entrance.m_fOrientation);
					entryInfos[e].EntryDir.x = -entryInfos[e].EntryDir.x;
					entryInfos[e].orientation = entrance.m_fOrientation;

					entryInfos[e].leftLaneFilterPhase = entrance.m_iLeftFilterLanePhase;
					entryInfos[e].canTurnRightOnRedLight = entrance.m_bCanTurnRightOnRedLight;				
					entryInfos[e].leftLaneIsAheadOnly = entrance.m_bLeftLaneIsAheadOnly;
					entryInfos[e].rightLaneIsRightOnly = entrance.m_bRightLaneIsRightOnly;

					const CPathNodeLink & link = pathData.GetNodesLink(pJunctionNode, iLink);
					entryInfos[e].lanesToJunction = link.m_1.m_LanesFromOtherNode;
					entryInfos[e].lanesFromJunction = link.m_1.m_LanesToOtherNode;
					entryInfos[e].phase = temp.m_Entrances[e].m_iPhase;
					break;
				}
			}
		}
		if(n == junction.GetNumJunctionNodes())
		{
			Assertf(false, "Couldn't find link from junction node to entrance node (must be adjacent nodes!), in junction template [%i] at (%.1f, %.1f, %.1f)", t, junction.GetJunctionCenter().x, junction.GetJunctionCenter().y, junction.GetJunctionCenter().z);
			junction.SetErrorBindingJunction(true);
			return 0;
		}
	}

	junction.SetTemplateIndex(t);

	Assertf(temp.m_iNumPhases > 0, "Junction template [%i] at (%.1f, %.1f, %.1f) has zero light phases",
		t, junction.GetJunctionCenter().x,junction.GetJunctionCenter().y,junction.GetJunctionCenter().z);


	//-----------------------------------------------------------------------------------------------------
	// Total up the duration of all the light phases, and then set the initial time & phase based upon the
	// network time modulo this total.. This should ensure that all junction phases sync in multiplayer

	int p;
	float fTotalDuration = 0.0;
	for(p=0; p< temp.m_iNumPhases; p++)
		fTotalDuration += temp.m_PhaseTimings[p].m_fDuration;

	const int iTotalDuration = ((int)(fTotalDuration * 1000.0f));
	int iPhaseOffsetMs = (int)(temp.m_fPhaseOffset * 1000.0f);
	const int iInitialTimeMs = (iTotalDuration > 0) ?
		(NetworkInterface::GetSyncedTimeInMilliseconds() + iPhaseOffsetMs) % iTotalDuration : 0;
	float fInitialTime = ((float)iInitialTimeMs) / 1000.0f;

	for(p=0; p<temp.m_iNumPhases; p++)
	{
		if(fInitialTime <= temp.m_PhaseTimings[p].m_fDuration)
		{
			junction.SetLightPhase(p);
			junction.SetLightTimeRemaining( temp.m_PhaseTimings[p].m_fDuration - fInitialTime );
			break;
		}
		else
		{
			fInitialTime -= temp.m_PhaseTimings[p].m_fDuration;
		}
	}
	if( p == temp.m_iNumPhases)
	{
		Assertf(false, "Couldn't find a starting phase for junction template [%i] at (%.1f, %.1f, %.1f), this junction probably has no light phases", t, junction.GetJunctionCenter().x, junction.GetJunctionCenter().y, junction.GetJunctionCenter().z);

		junction.SetLightPhase(0);
		junction.SetLightTimeRemaining( FLT_MAX );
	}

	return temp.m_iNumEntrances;
}

// Check to see if this junction needs special cycle settings
CAutoJunctionAdjustment* CJunctions::FindAutoJunctionAdjustmentData(Vec3V_In vPos)
{
	for(u32 n = 0; n < m_JunctionTemplates->m_AutoJunctionAdjustments.GetCount(); n++)
	{
		CAutoJunctionAdjustment& adj = m_JunctionTemplates->m_AutoJunctionAdjustments[n];
		if (IsCloseAll(vPos, adj.m_vLocation, ScalarV(ms_fJunctionNodePosEps)))
		{
			return &adj;
		}
	}
	return NULL;
}

void CJunctions::FindAutoJunctionAdjustments(Vec3V_In vPos, bool hasPedPhase, s32& outOffset, float& outScale)
{
	CAutoJunctionAdjustment* adj = FindAutoJunctionAdjustmentData(vPos);
	if (adj)
	{
		outOffset = (int)(adj->m_fCycleOffset * 1000.0f);
		float originalTime = (float)(hasPedPhase ? LIGHTDURATION_CYCLETIME_PED : LIGHTDURATION_CYCLETIME);
		outScale = originalTime / (adj->m_fCycleDuration * 1000.0f);
	}
	else
	{
		outOffset = 0;
		outScale = 1.0f;
	}
}


bool IsSlipLaneJunctionNode(const CNodeAddress junctionNode)
{
	const CPathNode* pJunctionNode = ThePaths.FindNodePointerSafe(junctionNode);
	if (!pJunctionNode)
	{
		return false;
	}

	return pJunctionNode->m_2.m_slipJunction && !pJunctionNode->IsJunctionNode();

	//return CPathNodeRouteSearchHelper::GetSlipLaneNodeLinkIndex(pJunctionNode, prevNode, ThePaths) >= 0;
}

// bool IsSlipLaneJunctionNodeOld(const CNodeAddress junctionNode, const CNodeAddress prevNode)
// {
// 	if(prevNode.IsEmpty())
// 		return false;
// 
// 	CPathNode * pJunctionNode = ThePaths.FindNodePointerSafe(junctionNode);
// 	if(pJunctionNode && pJunctionNode->NumLinks()==3)
// 	{
// 		const CPathNodeLink * links[3] =
// 		{
// 			&ThePaths.GetNodesLink(pJunctionNode, 0),
// 			&ThePaths.GetNodesLink(pJunctionNode, 1),
// 			&ThePaths.GetNodesLink(pJunctionNode, 2)
// 		};
// 
// 		const int iLinkBack =
// 			(links[0]->m_OtherNode == prevNode) ? 0 : (links[1]->m_OtherNode == prevNode) ? 1 : 2;
// 
// 		if(iLinkBack==0 && (links[1]->m_1.m_LanesFromOtherNode == 0 || links[2]->m_1.m_LanesFromOtherNode == 0))
// 			return true;
// 
// 		if(iLinkBack==1 && (links[0]->m_1.m_LanesFromOtherNode == 0 || links[2]->m_1.m_LanesFromOtherNode == 0))
// 			return true;
// 
// 		if(iLinkBack==2 && (links[0]->m_1.m_LanesFromOtherNode == 0 || links[1]->m_1.m_LanesFromOtherNode == 0))
// 			return true;
// 	}
// 
// 	return false;
// }

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : UpdateJunctions
// PURPOSE :  Update the junction that this car belongs to.
/////////////////////////////////////////////////////////////////////////////////

void CVehicle::UpdateJunctions()
{
	CVehicleIntelligence* pIntel = GetIntelligence();

	//reset this now, and if we are approaching the junction w siren
	//at the end of this update, set it to true there.
	m_nVehicleFlags.bPreviousApproachingJunctionWithSiren = m_nVehicleFlags.bApproachingJunctionWithSiren;
	m_nVehicleFlags.bApproachingJunctionWithSiren = false;

	CVehicleNodeList * pNodeList = pIntel->GetNodeList();
	if(!pNodeList)
	{
		return;
	}

	s32 iOldNode = pNodeList->GetTargetNodeIndex() - 1;
	if( iOldNode < 0 )
		return;

	CNodeAddress newJunction;
	int iNewJunctionIndex = -1;
	CJunction* pNewJunction = NULL;

	switch(GetStatus())
	{
		case STATUS_PHYSICS:
		case STATUS_PLAYER:
		{
			bool waitingAtJunction = false;

			CTaskVehicleMissionBase *carTask = pIntel->GetActiveTask();
			CTaskVehicleCruiseNew *newCruiseTask = NULL;
			const int carTaskType = carTask ? carTask->GetTaskType() : CTaskTypes::TASK_INVALID_ID;
			if(carTask && (carTaskType == CTaskTypes::TASK_VEHICLE_CRUISE_NEW || carTaskType == CTaskTypes::TASK_VEHICLE_GOTO_AUTOMOBILE_NEW))
			{
				newCruiseTask = static_cast<CTaskVehicleCruiseNew*>(carTask);
			}

			if (newCruiseTask && newCruiseTask->GetState() == CTaskVehicleCruiseNew::State_StopForJunction)
			{	
				// If the car is waiting it shouldn't be holding up other traffic.
				// Only for non-templated junctions.  Templated ones handle this implicitly.
				CJunction * pJunction = pIntel->GetJunction();
				if(!pJunction /*|| pJunction->GetTemplateIndex()==-1*/)
				{
					newJunction.SetEmpty();
					pNewJunction = NULL;
				}
				else
				{
					newJunction = pIntel->GetJunctionNode();
					pNewJunction = pJunction;
				}

				waitingAtJunction = true;
			}

			if(waitingAtJunction == false)
			{
				s32 iMaxNode = rage::Min(pNodeList->GetTargetNodeIndex()+4, pNodeList->FindLastGoodNode());

				for (s32 i = iOldNode; i < iMaxNode; ++i)
				{
					const CNodeAddress& nodeAddress = pNodeList->GetPathNodeAddr(i);

					if (nodeAddress.IsEmpty())
					{
						break;
					}

					if (nodeAddress == pIntel->GetPreviousJunctionNode())
					{
						iOldNode = i + 1;
					}
					else if (nodeAddress == pIntel->GetJunctionNode())
					{
						iOldNode = i;
						break;
					}
				}

				for (s32 n = iOldNode; n < iMaxNode; n++)
				{
					if (pNodeList->GetPathNodeAddr(n).IsEmpty())
					{
						break;
					}
					if ( pNodeList->GetPathNode(n) && pNodeList->GetPathNode(n)->IsJunctionNode())
					{
						//const bool bIsSliplaneJn = n > 0 && IsSlipLaneJunctionNode(pNodeList->GetPathNodeAddr(n), pNodeList->GetPathNodeAddr(n-1)); 
						const CPathNode* pJnNode = pNodeList->GetPathNode(n);
						const bool bIsSliplaneJn = pJnNode->m_2.m_slipJunction && !pJnNode->IsJunctionNode();
						if(!bIsSliplaneJn)
						{
							newJunction = pNodeList->GetPathNodeAddr(n);

							// Quite often, what we find here will be the same as the junction we already have
							// stored. If so, we should also have the CJunction pointer, so we don't need to
							// call FindJunctionFromNode(), which is fairly expensive.
							if (newJunction == pIntel->GetJunctionNode())
							{
								pNewJunction = pIntel->GetJunction();

								Assert(pNewJunction);

								if (pNewJunction->IsOnlyJunctionBecauseHasSwitchedOffEntrances())
								{
									const CNodeAddress& junctionEntrance = pIntel->GetJunctionEntranceNode();

									if (!junctionEntrance.IsEmpty())
									{
										const s32 iEntranceIndex = pNewJunction->FindEntranceIndexWithNode(junctionEntrance);

										if (iEntranceIndex >= 0 && !pNewJunction->GetEntrance(iEntranceIndex).m_bIsSwitchedOff)
										{
											continue;
										}
									}
								}

#if __ASSERT
								Vector3 vJCoords;
								pJnNode->GetCoors(vJCoords);
								Assertf(pNewJunction == CJunctions::FindJunctionFromNode(newJunction), "newJunction [%i:%i] pJnNode [%i:%i] (coords: %.1f, %.1f, %.1f)", newJunction.GetRegion(), newJunction.GetIndex(), pJnNode->GetAddrRegion(), pJnNode->GetAddrIndex(), vJCoords.x, vJCoords.y, vJCoords.z);
#endif
							}
							else
							{
								pNewJunction = CJunctions::FindJunctionFromNode(newJunction);
							}

							iNewJunctionIndex = n;
							break;
						}
					}
				}
			}
			break;
		}
	}

	if (!newJunction.IsEmpty() && (!pNewJunction || pNewJunction != pIntel->GetJunction()))
	{
		CJunction* pAddedToJunction = CJunctions::AddVehicleToJunction(this, newJunction, pNewJunction);

		if (!pNewJunction)
		{
			pNewJunction = pAddedToJunction;
		}

		if (pNewJunction && pNewJunction->IsVehicleAddedToJunction(this))
		{
			if (!pIntel->GetJunctionNode().IsEmpty())
			{
				this->GetIntelligence()->ResetCachedJunctionInfo();
			}

			pIntel->SetJunctionCommand(JUNCTION_COMMAND_APPROACHING);
			pIntel->SetJunctionFilter(JUNCTION_FILTER_NONE);
			pIntel->SetHasFixedUpPathForCurrentJunction(false);
			pIntel->SetJunctionNode(newJunction, pNewJunction);
			this->GetIntelligence()->CacheJunctionInfo();

#if __ASSERT
			const CNodeAddress& entranceAddress = pIntel->GetJunctionEntranceNode();

			static bool bAlreadyPrintedNodelistOnce = false;	//so we don't spam mp games
			if (!bAlreadyPrintedNodelistOnce && entranceAddress.IsEmpty() && iNewJunctionIndex != 0)
			{
				bAlreadyPrintedNodelistOnce = true;

				Displayf("iNewJunctionIndex: %d", iNewJunctionIndex);
				Displayf("iNewJunction: %d:%d", newJunction.GetRegion(), newJunction.GetIndex());
				for (s32 i = 0; i < CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED; i++)
				{
					Displayf("aNodes[%d]=%d:%d aLinks[%d]=%d\n", i, pNodeList->GetPathNodeAddr(i).GetRegion(),pNodeList->GetPathNodeAddr(i).GetIndex(), i, pNodeList->GetPathLinkIndex(i));
				}
			}
#if ((AI_OPTIMISATIONS_OFF) || (AI_VEHICLE_OPTIMISATIONS_OFF))
			Assertf(!entranceAddress.IsEmpty() || iNewJunctionIndex == 0, "CVehicle::UpdateJunctions found a valid new junction but no entrance!");
#endif
#endif //__ASSERT
		}
	}

	if (m_nVehicleFlags.GetIsSirenOn() && pIntel->GetJunctionCommand() == JUNCTION_COMMAND_GO
		&& pIntel->GetJunction() && !pIntel->GetJunction()->IsOnlyJunctionBecauseHasSwitchedOffEntrances()
		&& pNodeList->GetPathNode(pNodeList->GetTargetNodeIndex())
		&& pNodeList->GetPathNodeAddr(pNodeList->GetTargetNodeIndex()) == pIntel->GetJunctionNode() )
	{
		m_nVehicleFlags.bApproachingJunctionWithSiren = true;
	}
}


