
/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    Restart.cpp
// PURPOSE : Handles restarting the player after he has been arrested or killed
// AUTHOR :  Graeme
// CREATED : 02-08-00
//
/////////////////////////////////////////////////////////////////////////////////

#include "ai/navmesh/navmeshstore.h"
#include "ai/navmesh/pathserverbase.h"
#include "pathserver/PathServer.h"
#include "vehicleAi/pathFind.h"
#include "control/restart.h"
#include "core/game.h"
#include "Stats/StatsMgr.h"
#include "game/zones.h"
#include "network/players/NetworkPlayerMgr.h"
#include "peds/ped.h"
#include "peds/PedIntelligence.h"
#include "math/angmath.h"
#include "scene/world/gameWorld.h"
#include "scene/streamer/streamvolume.h"
#include "vector/vector2.h"
#include "Stats/StatsInterface.h"
#include "camera/CamInterface.h"
#include "camera/debug/DebugDirector.h"
#include "camera/helpers/Frame.h"
#include "physics/physics.h"

#include "fwscene/stores/boxstreamer.h"

AI_OPTIMISATIONS()

RestartPoint CRestart::HospitalRestarts[MAX_HOSPITAL_RESTARTS];
s32 CRestart::NumberOfHospitalRestarts;

RestartPoint CRestart::PoliceRestarts[MAX_POLICE_RESTARTS];
s32 CRestart::NumberOfPoliceRestarts;

SaveHouseStruct CRestart::SaveHouses[MAX_SAVE_HOUSES];
s32 CRestart::NumberOfSaveHouses;

bool CRestart::ms_bOverrideSavehouse = false;
Vector3 CRestart::ms_vCoordsOfSavehouseOverride;
float CRestart::ms_fHeadingOfSavehouseOverride = 0.0f;

Vector3 CRestart::NetworkRestartPoints[MAX_NETWORK_RESTARTS];
float CRestart::NetworkRestartHeadings[MAX_NETWORK_RESTARTS];
u32 CRestart::NetworkRestartTimes[MAX_NETWORK_RESTARTS];
s32 CRestart::NetworkRestartTeams[MAX_NETWORK_RESTARTS];
s32 CRestart::NumberOfNetworkRestarts;

bool CRestart::bOverrideRestart;
Vector3 CRestart::OverridePosition;
float CRestart::OverrideHeading;
bool CRestart::bPausePlayerRespawnAfterDeathArrest = false;

bool	CRestart::bSuppressFadeOutAfterDeath = false;
bool	CRestart::bSuppressFadeOutAfterArrest = false;
bool	CRestart::bSuppressFadeInAfterDeathArrest = false;

Vector3	CRestart::ExtraHospitalRestartCoors;
float	CRestart::ExtraHospitalRestartRadius;
float	CRestart::ExtraHospitalRestartHeading;

Vector3	CRestart::ExtraPoliceStationRestartCoors;
float	CRestart::ExtraPoliceStationRestartRadius;
float	CRestart::ExtraPoliceStationRestartHeading;

bool	CRestart::bOverrideRespawnBasePointForMission;
Vector3	CRestart::OverrideRespawnBasePointForMission;

bool CRestart::ms_bIgnoreNextRestart = false;


void CRestart::ResetData(void)
{
	u16 loop;

	for (loop = 0; loop < MAX_HOSPITAL_RESTARTS; loop++)
	{
		HospitalRestarts[loop].m_Coords = Vector3(0.0f, 0.0f, 0.0f);
		HospitalRestarts[loop].m_Heading = 0.0f;
		HospitalRestarts[loop].m_bEnabled = false;
	}

	for (loop = 0; loop < MAX_POLICE_RESTARTS; loop++)
	{
		PoliceRestarts[loop].m_Coords = Vector3(0.0f, 0.0f, 0.0f);
		PoliceRestarts[loop].m_Heading = 0.0f;
		PoliceRestarts[loop].m_bEnabled = false;
	}

	for (loop = 0; loop < MAX_SAVE_HOUSES; loop++)
	{
		SaveHouses[loop].Coords = Vector3(0.0f, 0.0f, 0.0f);
		SaveHouses[loop].Heading = 0.0f;
		SaveHouses[loop].RoomName[0] = '\0';
		SaveHouses[loop].WhenToUse = 0;
		SaveHouses[loop].m_PlayerModelNameHash = 0;
		SaveHouses[loop].m_bEnabled = false;
		SaveHouses[loop].m_bAvailableForAutosaves = false;
	}

	NumberOfHospitalRestarts = 0;
	NumberOfPoliceRestarts = 0;
	NumberOfSaveHouses = 0;

	ms_bOverrideSavehouse = false;
	ms_vCoordsOfSavehouseOverride.Zero();
	ms_fHeadingOfSavehouseOverride = 0.0f;


	NumberOfNetworkRestarts = 0;

	bOverrideRestart = FALSE;
	OverridePosition = Vector3(0.0f, 0.0f, 0.0f);
	OverrideHeading = 0.0f;
	bPausePlayerRespawnAfterDeathArrest = false;
	
	bSuppressFadeOutAfterDeath = false;
	bSuppressFadeOutAfterArrest = false;
	bSuppressFadeInAfterDeathArrest = false;

	ExtraHospitalRestartRadius = 0.0f;
	ExtraPoliceStationRestartRadius = 0.0f;

	bOverrideRespawnBasePointForMission = false;

	ms_bIgnoreNextRestart = false;
}

void CRestart::Init(unsigned initMode)
{
    if(initMode == INIT_SESSION)
    {
	    ResetData();
    }
}

void CRestart::Shutdown(unsigned /*shutdownMode*/)
{
	ResetData();
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : CRestart::AddHospitalRestartPoint
// PURPOSE :  Adds a new point to the array of Hospital Restart points
// INPUTS :	Vector describing the position of the point, player heading when restarted
/////////////////////////////////////////////////////////////////////////////////
s32 CRestart::AddHospitalRestartPoint(const Vector3 &NewPoint, const float NewHeading, s32 WhenToUse)
{
	if (Verifyf(NumberOfHospitalRestarts < (MAX_HOSPITAL_RESTARTS - 1), "Too many hospital restarts"))
	{
		HospitalRestarts[NumberOfHospitalRestarts].m_Coords = NewPoint;
		HospitalRestarts[NumberOfHospitalRestarts].m_Heading = NewHeading;
		HospitalRestarts[NumberOfHospitalRestarts].m_WhenToUse = WhenToUse;
		HospitalRestarts[NumberOfHospitalRestarts].m_bEnabled = true;
		NumberOfHospitalRestarts++;
		return (NumberOfHospitalRestarts-1);
	}
	return -1;
}

void CRestart::DisableHospitalRestartPoint(const s32 index, const bool bDisable)
{
	if (Verifyf(index >= 0 && index < NumberOfHospitalRestarts, "Hospital restart Index is not valid"))
	{
		HospitalRestarts[index].m_bEnabled = !bDisable;
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : CRestart::AddPoliceRestartPoint
// PURPOSE :  Adds a new point to the array of Police Restart points
// INPUTS :	Vector describing the position of the point, player heading when restarted
/////////////////////////////////////////////////////////////////////////////////
s32 CRestart::AddPoliceRestartPoint(const Vector3 &NewPoint, const float NewHeading, s32 WhenToUse)
{
	if (Verifyf(NumberOfPoliceRestarts < (MAX_POLICE_RESTARTS - 1), "Too many police restarts"))
	{
		PoliceRestarts[NumberOfPoliceRestarts].m_Coords = NewPoint;
		PoliceRestarts[NumberOfPoliceRestarts].m_Heading = NewHeading;
		PoliceRestarts[NumberOfPoliceRestarts].m_WhenToUse = WhenToUse;
		PoliceRestarts[NumberOfPoliceRestarts].m_bEnabled = true;
		NumberOfPoliceRestarts++;
		return (NumberOfPoliceRestarts-1);
	}
	return -1;
}

void CRestart::DisablePoliceRestartPoint(const s32 index, const bool bDisable)
{
	if (Verifyf(index >= 0 && index < NumberOfPoliceRestarts, "Police restart Index is not valid"))
	{
		PoliceRestarts[index].m_bEnabled = !bDisable;
	}
}

int CRestart::AddSaveHouse(const Vector3 &NewPoint, const float NewHeading, const char *pRoomName, s32 WhenToUse, u32 playerModelNameHash)
{
	if (Verifyf(NumberOfSaveHouses < MAX_SAVE_HOUSES, "CRestart::AddSaveHouse - Too many Save Houses"))
	{
		SaveHouses[NumberOfSaveHouses].Coords = NewPoint;
		SaveHouses[NumberOfSaveHouses].Heading = NewHeading;
		strncpy(SaveHouses[NumberOfSaveHouses].RoomName, pRoomName, MAX_LENGTH_OF_ROOM_NAME);
		SaveHouses[NumberOfSaveHouses].WhenToUse = WhenToUse;
		SaveHouses[NumberOfSaveHouses].m_PlayerModelNameHash = playerModelNameHash;
		SaveHouses[NumberOfSaveHouses].m_bEnabled = true;
		SaveHouses[NumberOfSaveHouses].m_bAvailableForAutosaves = true;
		NumberOfSaveHouses++;

		return (NumberOfSaveHouses-1);
	}

	return -1;
}

void CRestart::SetSaveHouseEnabled(int SaveHouseIndex, bool bNewEnabledFlag, bool bAvailableForAutosaves)
{
	if ( (SaveHouseIndex >= 0) && (SaveHouseIndex < NumberOfSaveHouses) )
	{
		SaveHouses[SaveHouseIndex].m_bEnabled = bNewEnabledFlag;
		SaveHouses[SaveHouseIndex].m_bAvailableForAutosaves = bAvailableForAutosaves;
	}
	else
	{
		Assertf(0, "CRestart::SetSaveHouseEnabled - SaveHouseIndex is out of range");
	}
}

void CRestart::SetSavehouseOverride(bool bOverride, const Vector3 &vecCoords, float fHeading)
{
	ms_bOverrideSavehouse = bOverride;
	ms_vCoordsOfSavehouseOverride = vecCoords;
	ms_fHeadingOfSavehouseOverride = fHeading;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : AddNetworkRestartPoint
// PURPOSE :  Adds a new point to the array of Network Restart points
// INPUTS :	Vector describing the position of the point, player heading when restarted
/////////////////////////////////////////////////////////////////////////////////

void CRestart::AddNetworkRestartPoint(const Vector3 &NewPoint, const float NewHeading, const int Team)
{
	Assertf(NumberOfNetworkRestarts < (MAX_NETWORK_RESTARTS - 1), "Too many network restarts");

	NetworkRestartPoints[NumberOfNetworkRestarts] = NewPoint;
	NetworkRestartHeadings[NumberOfNetworkRestarts] = NewHeading;
	NetworkRestartTimes[NumberOfNetworkRestarts] = 0;
	NetworkRestartTeams[NumberOfNetworkRestarts] = Team;
	NumberOfNetworkRestarts++;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : RemoveAllNetworkRestartPoints
// PURPOSE :  Remove all of the network restart points that have been set up so far
// INPUTS :	
/////////////////////////////////////////////////////////////////////////////////

void CRestart::RemoveAllNetworkRestartPoints()
{
	NumberOfNetworkRestarts=0;
}



/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindNetworkRestartPoint
// PURPOSE :  Goes through all of the restart points and identifies the one that is
//			  furthest away from any of the players.
// INPUTS :
/////////////////////////////////////////////////////////////////////////////////

#define COOL_DOWN_TIME (20000)		// Only consider a restart point this long after it has been used.

bool CRestart::FindNetworkRestartPoint(Vector3 *pResultCoors, float *pResultHeading, int Team)
{
	float	BiggestDistToClosestPlayer = 0.0f;
	s32	BestRestartPoint = -1;
	float	BiggestDistToClosestPlayer_HotPoint = 0.0f;
	s32	BestRestartPoint_HotPoint = -1;

	for (s32 i = 0; i < NumberOfNetworkRestarts; i++)
	{
		float	ClosestDist = 9999999.9f;

		// First we make sure that the restart point belongs to the right team.
		if (Team < 0 || NetworkRestartTeams[i] < 0 || Team == NetworkRestartTeams[i])
		{
			unsigned                 numRemotePhysicalPlayers = netInterface::GetNumRemotePhysicalPlayers();
            const netPlayer * const *remotePhysicalPlayers    = netInterface::GetRemotePhysicalPlayers();

	        for(unsigned index = 0; index < numRemotePhysicalPlayers; index++)
            {
		        const CNetGamePlayer *pPlayer = SafeCast(const CNetGamePlayer, remotePhysicalPlayers[index]);

                if(pPlayer->GetPlayerPed())
                {
				    float	Dist = (VEC3V_TO_VECTOR3(pPlayer->GetPlayerPed()->GetTransform().GetPosition()) - NetworkRestartPoints[i]).XYMag();

				    ClosestDist = rage::Min(ClosestDist, Dist);
                }
			}

			if (fwTimer::GetTimeInMilliseconds() > NetworkRestartTimes[i] + COOL_DOWN_TIME)
			{
				if (ClosestDist > BiggestDistToClosestPlayer)
				{
					BiggestDistToClosestPlayer = ClosestDist;
					BestRestartPoint = i;
				}
			}
			else
			{
				if (ClosestDist > BiggestDistToClosestPlayer_HotPoint)
				{
					BiggestDistToClosestPlayer_HotPoint = ClosestDist;
					BestRestartPoint_HotPoint = i;
				}
			}
		}
	}
		// First we try to pick a point that hasn't been used recently.
	if (BestRestartPoint >= 0)
	{
		*pResultCoors = NetworkRestartPoints[BestRestartPoint];
		*pResultHeading = NetworkRestartHeadings[BestRestartPoint];
		NetworkRestartTimes[BestRestartPoint] = fwTimer::GetTimeInMilliseconds();
		return true;
	}
		// Now also consider points that have been used in the last 20 seconds or so.
	if (BestRestartPoint_HotPoint >= 0)
	{
		*pResultCoors = NetworkRestartPoints[BestRestartPoint_HotPoint];
		*pResultHeading = NetworkRestartHeadings[BestRestartPoint_HotPoint];
		NetworkRestartTimes[BestRestartPoint_HotPoint] = fwTimer::GetTimeInMilliseconds();
		return true;
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindNetworkRestartPointForScript
// PURPOSE :  Goes through all of the restart points and identifies the one that is
//			  furthest away from any of the players.
// INPUTS :
/////////////////////////////////////////////////////////////////////////////////

void CRestart::FindNetworkRestartPointForScript(Vector3 *pResultCoors, float *pResultHeading, int Team)
{
#if __ASSERT
	bool bResult = 
#endif
		FindNetworkRestartPoint(pResultCoors, pResultHeading, Team);

	Assert(bResult);
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : CRestart::FindClosestHospitalRestartPoint
// PURPOSE :  Finds the nearest Hospital Restart point to the players current 
//				position (and returns a suitable ped creation point nearby)
// OUTPUTS :  Point to restart the player at, player's heading when restarted
// RETURNS :  False when spawn point returned uses the one passed in
/////////////////////////////////////////////////////////////////////////////////
bool CRestart::FindClosestHospitalRestartPoint(const Vector3& TestPoint, float fPlayerCurrentHeading, Vector3 *ClosestPoint, float *pHeading)
{
	float ClosestMag, DiffDist;
	s16 ClosestIndex = -1;
	u16 loop;
	Vector3 Diff;
	Vector3 TestPoint2 = TestPoint;

	if (ms_bIgnoreNextRestart)
	{
		*ClosestPoint = TestPoint;
		*pHeading = fPlayerCurrentHeading;
		bOverrideRestart = false;
		ms_bIgnoreNextRestart = false;
		return false;
	}

	if (bOverrideRestart)
	{	// If the restart point has been overridden then use the override
		//	point instead of choosing a hospital one
		*ClosestPoint = OverridePosition;
		*pHeading = OverrideHeading;
		bOverrideRestart = FALSE;
		return true;
	}
	else
	{
			// This can be used by the level designers to make the player spawn near the point he took the mission
		if (bOverrideRespawnBasePointForMission)
		{
			TestPoint2 = OverrideRespawnBasePointForMission;
			bOverrideRespawnBasePointForMission = false;
		}

		if (ExtraHospitalRestartRadius > 0.0f &&
			 Vector2(TestPoint2.x - ExtraHospitalRestartCoors.x, TestPoint2.y - ExtraHospitalRestartCoors.y).Mag() < ExtraHospitalRestartRadius)
		{
			*ClosestPoint = ExtraHospitalRestartCoors;
			*pHeading = ExtraHospitalRestartHeading;
			return true;
		}

//		int MapAreaForTestPoint = CMapAreas::GetAreaIndexFromPosition(&TestPoint2); rage conv

		// Choose the closest hospital restart point to the player
		ClosestMag = 9999999.9f;

		//	Try to find the closest hospital restart point in the same zone	
		for (loop = 0; loop < NumberOfHospitalRestarts; loop++)
		{
			if( !HospitalRestarts[loop].m_bEnabled )
				continue;

			if (HospitalRestarts[loop].m_WhenToUse > StatsInterface::GetIntStat(STAT_CITIES_PASSED))
				continue;

			Diff = TestPoint2 - HospitalRestarts[loop].m_Coords;
			DiffDist = Diff.Mag();
			
			// The following bit of code favours respawn points that are in the same area as the player currently is.
//			if(MapAreaForTestPoint != LEVEL_GENERIC && rage conv
//				MapAreaForTestPoint != CMapAreas::GetAreaIndexFromPosition(&HospitalRestartPoints[loop]))
//				DiffDist *= 6.0f;
				
			if (DiffDist < ClosestMag)
			{
				ClosestMag = DiffDist;
				ClosestIndex = loop;
			}
		}

		if (ClosestIndex < 0)
		{		// We didn't find a restart point. Presumably not set by script.
			// Just return a point near the current coordinates
			Assertf(0, "Couldn't find a hospital restart zone near the player");
			// don't want to return an uninitialised vec, so set to current position in the absence of path nodes
			*ClosestPoint = TestPoint;
			*pHeading = 0.0f;
			return false;
		}
		else
		{
			*ClosestPoint = HospitalRestarts[ClosestIndex].m_Coords;
			*pHeading = HospitalRestarts[ClosestIndex].m_Heading;
		}
	}

	return true;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : CRestart::FindClosestPoliceRestartPoint
// PURPOSE :  Finds the nearest Police Restart point to the players current 
//				position (and returns a suitable ped creation point nearby)
// OUTPUTS :  Point to restart the player at, player's heading when restarted
/////////////////////////////////////////////////////////////////////////////////
void CRestart::FindClosestPoliceRestartPoint(const Vector3& TestPoint, float fPlayerCurrentHeading, Vector3 *ClosestPoint, float *pHeading)
{
	float ClosestMag, DiffDist;
	s16 ClosestIndex = -1;
	u16 loop;
	Vector3 TestPoint2 = TestPoint;
	Vector3 Diff;

	if (ms_bIgnoreNextRestart)
	{
		*ClosestPoint = TestPoint;
		*pHeading = fPlayerCurrentHeading;
		bOverrideRestart = false;
		ms_bIgnoreNextRestart = false;
		return;
	}

	if (bOverrideRestart)
	{	// If the restart point has been overridden then use the override
		//	point instead of choosing a police one
		*ClosestPoint = OverridePosition;
		*pHeading = OverrideHeading;
		bOverrideRestart = FALSE;
		return;
	}
	else
	{
			// This can be used by the level designers to make the player spawn near the point he took the mission
		if (bOverrideRespawnBasePointForMission)
		{
			TestPoint2 = OverrideRespawnBasePointForMission;
			bOverrideRespawnBasePointForMission = false;
		}

		if (ExtraPoliceStationRestartRadius > 0.0f &&
			Vector2(TestPoint2.x - ExtraPoliceStationRestartCoors.x, TestPoint2.y - ExtraPoliceStationRestartCoors.y).Mag() < ExtraPoliceStationRestartRadius)
		{
			*ClosestPoint = ExtraPoliceStationRestartCoors;
			*pHeading = ExtraPoliceStationRestartHeading;
			return;
		}

//		eMapArea MapAreaForTestPoint = CMapAreas::GetAreaIndexFromPosition(&TestPoint2); rage conv

		// Choose the closest police restart point to the player
		ClosestMag = 99999.9f;

		//	Try to find the closest police restart point in the same zone
		for (loop = 0; loop < NumberOfPoliceRestarts; loop++)
		{
			if( !PoliceRestarts[loop].m_bEnabled )
				continue;

			if (PoliceRestarts[loop].m_WhenToUse > StatsInterface::GetIntStat(STAT_CITIES_PASSED))
				continue;

			Diff = TestPoint2 - PoliceRestarts[loop].m_Coords;
			DiffDist = Diff.Mag();
			
			// The following bit of code favours respawn points that are in the same area as the player currently is.
//			if(MapAreaForTestPoint != LEVEL_GENERIC && rage conv
//				MapAreaForTestPoint != CMapAreas::GetAreaIndexFromPosition(&PoliceRestartPoints[loop]))
//				DiffDist *= 6.0f;
				
			if (DiffDist < ClosestMag)
			{
				ClosestMag = DiffDist;
				ClosestIndex = loop;
			}
		}
			
		if (ClosestIndex < 0)
		{		// We didn't find a restart point. Presumably not set by script.
			Assertf(0, "Couldn't find a police restart zone near the player");
			// don't want to return an uninitialised vec, so set to current position in the absence of path nodes
			*ClosestPoint = TestPoint;
			*pHeading = 0.0f;
		}
		else
		{
			*ClosestPoint = PoliceRestarts[ClosestIndex].m_Coords;
			*pHeading = PoliceRestarts[ClosestIndex].m_Heading;
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : CRestart::OverrideNextRestart
// PURPOSE :  Sets a new point to be used next time the player is restarted
//				The closest Hospital or Police Restart point is ignored
// INPUT :  Restart point to use next time the player is killed or arrested
//			Player's heading when restarted
/////////////////////////////////////////////////////////////////////////////////
void CRestart::OverrideNextRestart(const Vector3 &OverridePoint, const float OverrideHdng)
{
	OverridePosition = OverridePoint;
	OverrideHeading = OverrideHdng;
	bOverrideRestart = TRUE;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : CRestart::CancelOverrideRestart
// PURPOSE :  If an override restart point has previously been set then it will 
//				be ignored and when the player dies or is arrested, the closest 
//				 Hospital or Police Restart point will be used
/////////////////////////////////////////////////////////////////////////////////
void CRestart::CancelOverrideRestart(void)
{
	bOverrideRestart = FALSE;
}

void CRestart::SetIgnoreNextRestart(bool bIgnore)
{
	ms_bIgnoreNextRestart = bIgnore;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : SetRespawnBasePointForDurationOfMission
// PURPOSE :  Sometimes a mission takes the player somewhere else. This command allows
//			  level designers to fix a respawn point for the duration of 1 mission.
// OUTPUTS :
/////////////////////////////////////////////////////////////////////////////////

void CRestart::SetRespawnPointForDurationOfMission(const Vector3& Coors)
{
	bOverrideRespawnBasePointForMission = true;
	OverrideRespawnBasePointForMission = Coors;
}

void CRestart::ClearRespawnPointForDurationOfMission()
{
	bOverrideRespawnBasePointForMission = false;
}


const float CNetRespawnMgr::ms_fSearchMinDistDefault	= 0.0f;
const float CNetRespawnMgr::ms_fSearchMaxDistDefault	= 50.0f;
const float CNetRespawnMgr::ms_fSearchMaxDistCutoff		= 100.0f;
const float CNetRespawnMgr::ms_fSearchHeadingNone = -1000.0f;
const Vector3 CNetRespawnMgr::ms_vSearchTargetPosNone(10000.0f, 10000.0f, 10000.0f);

float	CNetRespawnMgr::ms_fMaxDistanceForBunchingPlayers = 60.0f;
float	CNetRespawnMgr::ms_fMinDistanceForScoringEnemyPlayersFarther = 0.0f;
float	CNetRespawnMgr::ms_fMaxDistanceForScoringEnemyPlayersFarther = 100.0f;
float	CNetRespawnMgr::ms_fMinDistanceForScoringFriendlyPlayersCloser = 0.0f;
float	CNetRespawnMgr::ms_fIdealDistanceForScoringFriendlyPlayersCloser = 20.0f;
float	CNetRespawnMgr::ms_fMaxDistanceForScoringFriendlyPlayersCloser = 150.0f;
float	CNetRespawnMgr::ms_fMinDistanceForScoringEnemyAiFarther = 0.0f;
float	CNetRespawnMgr::ms_fMaxDistanceForScoringEnemyAiFarther = 100.0f;
float	CNetRespawnMgr::ms_fMinDistanceForScoringFriendlyAiCloser = 0.0f;
float	CNetRespawnMgr::ms_fIdealDistanceForScoringFriendlyAiCloser = 20.0f;
float	CNetRespawnMgr::ms_fMaxDistanceForScoringFriendlyAiCloser = 150.0f;

float	CNetRespawnMgr::ms_fMinScoreFOV = 0.0f;
float	CNetRespawnMgr::ms_fMinScoreProximityToOrigin = 0.0f;
float	CNetRespawnMgr::ms_fMinScoreTeamBunchingDirection = -1.0f;
float	CNetRespawnMgr::ms_fMinScoreTeamBunchingPosition = 0.0f;
float	CNetRespawnMgr::ms_fMinScoreEnemyPlayersFarther = -5.0f;
float	CNetRespawnMgr::ms_fMinScoreFriendlyPlayersCloser = 0.0f;
float	CNetRespawnMgr::ms_fMinScoreFriendlyPlayersTooClose = -2.0f;
float	CNetRespawnMgr::ms_fMinScoreEnemyAiFarther = -5.0f;
float	CNetRespawnMgr::ms_fMinScoreFriendlyAiCloser = 0.0f;
float	CNetRespawnMgr::ms_fMinScoreFriendlyAiTooClose = -2.0f;
float	CNetRespawnMgr::ms_fMinScoreRandomness = 0.0f;
float	CNetRespawnMgr::ms_fMaxScoreFOV = 1.0f;
float	CNetRespawnMgr::ms_fMaxScoreProximityToOrigin = 1.0f;
float	CNetRespawnMgr::ms_fMaxScoreTeamBunchingDirection = 1.0f;
float	CNetRespawnMgr::ms_fMaxScoreTeamBunchingPosition = 1.5f;
float	CNetRespawnMgr::ms_fMaxScoreEnemyPlayersFarther = 0.0f;
float	CNetRespawnMgr::ms_fMaxScoreFriendlyPlayersCloser = 1.5f;
float	CNetRespawnMgr::ms_fMaxScoreFriendlyPlayersTooClose = 0.0f;
float	CNetRespawnMgr::ms_fMaxScoreEnemyAiFarther = 0.0f;
float	CNetRespawnMgr::ms_fMaxScoreFriendlyAiCloser = 1.5f;
float	CNetRespawnMgr::ms_fMaxScoreFriendlyAiTooClose = 0.0f;
float	CNetRespawnMgr::ms_fMaxScoreRandomness = 1.0f;

#if __BANK
bool	CNetRespawnMgr::ms_bDebugUseAngledArea = false;
bool	CNetRespawnMgr::ms_bAdvancedUsePlayerAsPed = true;
bool	CNetRespawnMgr::ms_bAdvancedUsePlayerAsOrigin = true;
bool	CNetRespawnMgr::ms_bAdvancedUseCameraAsOrigin = false;
bool	CNetRespawnMgr::ms_bAdvancedUsePlayerAsTarget = false;
bool	CNetRespawnMgr::ms_bAdvancedUseCameraAsTarget = false;
float	CNetRespawnMgr::ms_fAdvancedMinSearchDistance = 0.0f;
float	CNetRespawnMgr::ms_fAdvancedMaxSearchDistance = 50.0f;
bool	CNetRespawnMgr::ms_bAdvancedFlagScriptCommand = false;
bool	CNetRespawnMgr::ms_bAdvancedFlagIgnoreTargetHeading = false;
bool	CNetRespawnMgr::ms_bAdvancedFlagPreferCloseToSpawnOrigin = false;
bool	CNetRespawnMgr::ms_bAdvancedFlagMaySpawnInInterior = true;
bool	CNetRespawnMgr::ms_bAdvancedFlagMaySpawnInExterior = true;
bool	CNetRespawnMgr::ms_bAdvancedFlagPreferWideFOV = true;
bool	CNetRespawnMgr::ms_bAdvancedFlagPreferTeamBunching = true;
bool	CNetRespawnMgr::ms_bAdvancedFlagPreferEnemyPlayersFarther = true;
bool	CNetRespawnMgr::ms_bAdvancedFlagPreferFriendlyPlayersCloser = true;
bool	CNetRespawnMgr::ms_bAdvancedFlagPreferEnemyAiFarther = true;
bool	CNetRespawnMgr::ms_bAdvancedFlagPreferFriendlyAiCloser = true;
bool	CNetRespawnMgr::ms_bAdvancedFlagPreferRandomness = true;
bool	CNetRespawnMgr::ms_bDrawResultForPedPositionProjectionTesting = false;
float	CNetRespawnMgr::ms_fTimeForPedPositionProjectionTesting = 3.0f;
Vector3	CNetRespawnMgr::ms_vResultForPedPositionProjectionTesting = VEC3_ZERO;

CNetRespawnMgr::OverridePositions CNetRespawnMgr::ms_aEnemyPlayerPositionsOverride;
CNetRespawnMgr::OverridePositions CNetRespawnMgr::ms_aFriendlyPlayerPositionsOverride;
CNetRespawnMgr::OverridePositions CNetRespawnMgr::ms_aEnemyAiPositionsOverride;
CNetRespawnMgr::OverridePositions CNetRespawnMgr::ms_aFriendlyAiPositionsOverride;

CNetRespawnMgr::TSpawnRecord CNetRespawnMgr::ms_SpawnRecord;
#endif

RegdConstPed CNetRespawnMgr::m_pPed;
Vector3 CNetRespawnMgr::m_vSearchOrigin = VEC3_ZERO;
Vector3 CNetRespawnMgr::m_vSearchTargetPos = VEC3_ZERO;
float CNetRespawnMgr::m_fSearchOptimalHeading = CNetRespawnMgr::ms_fSearchHeadingNone;
float CNetRespawnMgr::m_fSearchMinDist = 0.0f;
float CNetRespawnMgr::m_fSearchMaxDist = 0.0f;
int CNetRespawnMgr::m_iTeam = -1;
CScriptAngledArea CNetRespawnMgr::m_SearchAngledArea;
Vector3 CNetRespawnMgr::m_vSearchMins;
Vector3 CNetRespawnMgr::m_vSearchMaxs;
s32 CNetRespawnMgr::m_iSearchVolumeType;
fwFlags16 CNetRespawnMgr::m_uFlags = 0;
u32 CNetRespawnMgr::m_iSearchStartTimeMs = 0;
u32 CNetRespawnMgr::m_iSeachNumFrames = 0;

Vector3 CNetRespawnMgr::m_vLargestBunchOfEnemyPlayersPosition = VEC3_ZERO;
Vector3 CNetRespawnMgr::m_vLargestBunchOfFriendlyPlayersPosition = VEC3_ZERO;

bool CNetRespawnMgr::m_bSearchActive = false;
bool CNetRespawnMgr::m_bSearchComplete = false;
bool CNetRespawnMgr::m_bSearchSuccessful = false;

atRangeArray<TNavMeshIndex, CNetRespawnMgr::ms_iMaxStoredNavmeshes> CNetRespawnMgr::m_Timeslice_StoredNavmeshes;
s32 CNetRespawnMgr::m_Timeslice_iCurrentNavmesh;
s32 CNetRespawnMgr::m_Timeslice_iCurrentPoly;

s32 CNetRespawnMgr::m_iNumBestSpawnPoints = 0;
atRangeArray<CNetRespawnMgr::TSpawnPoint, CNetRespawnMgr::ms_iMaxSpawnPoints + 1> CNetRespawnMgr::m_BestSpawnPoints;

float CNetRespawnMgr::m_fCachedNavmeshLoadDistance = -1.0f;
const float CNetRespawnMgr::ms_fZCutoff = 500.0f;

CNetRespawnMgr::CachedPositions CNetRespawnMgr::m_PlayerPositionsFromNetwork;
CNetRespawnMgr::CachedPositions CNetRespawnMgr::m_AiPositionsFromWorld;

#if __BANK
bool CNetRespawnMgr::m_bDebugSearch = false;
bool CNetRespawnMgr::m_bDebugScores = false;
bool CNetRespawnMgr::ms_bUsePlayerOverrides = false;
bool CNetRespawnMgr::ms_bUseAiOverrides = false;
#endif

#define RESPAWNMGR_NUM_HEADINGS					8
#define SCRIPTERS_FORGOT_TO_CANCEL_SEARCH_MS	10000

CNetRespawnMgr::CNetRespawnMgr()
{

}
CNetRespawnMgr::~CNetRespawnMgr()
{

}
// NAME : Init
// PURPOSE : Initialises the system
void CNetRespawnMgr::Init(unsigned int UNUSED_PARAM(initMode))
{

}
// NAME : Shutdown
// PURPOSE : Shuts down the system
void CNetRespawnMgr::Shutdown(unsigned int UNUSED_PARAM(shutdownMode))
{

}
// NAME : Update
// PURPOSE : Updates down the system
void CNetRespawnMgr::Update()
{
	if(m_bSearchActive)
	{
		if(StreamAroundSearchOrigin())
		{
			UpdateSearch();
		}
	}
	else
	{
		u32 iTimeDelta = fwTimer::GetTimeInMilliseconds() - m_iSearchStartTimeMs;
		if(iTimeDelta > SCRIPTERS_FORGOT_TO_CANCEL_SEARCH_MS && CPathServer::GetIsNavMeshRegionRequired(NMR_NetworkRespawnMgr))
		{
			Assertf(false, "Script forgot to cancel network respawn manager; automatically unloading requested navmesh regions.");

			CPathServer::RemoveNavMeshRegion(NMR_NetworkRespawnMgr, THREAD_INVALID);
		}
	}

	// Display the best candidates so far
#if __BANK && DEBUG_DRAW
	if(m_bDebugSearch)
	{
		//Check if we are using player overrides.
		if(ms_bUsePlayerOverrides)
		{
			//Draw the enemy player positions.
			for(int i = 0; i < ms_aEnemyPlayerPositionsOverride.GetCount(); ++i)
			{
				grcDebugDraw::Capsule(VECTOR3_TO_VEC3V(ms_aEnemyPlayerPositionsOverride[i].m_vPosition + ZAXIS), VECTOR3_TO_VEC3V(ms_aEnemyPlayerPositionsOverride[i].m_vPosition - ZAXIS), 0.25f, Color_red, false);
			}
			
			//Draw the friendly player positions.
			for(int i = 0; i < ms_aFriendlyPlayerPositionsOverride.GetCount(); ++i)
			{
				grcDebugDraw::Capsule(VECTOR3_TO_VEC3V(ms_aFriendlyPlayerPositionsOverride[i].m_vPosition + ZAXIS), VECTOR3_TO_VEC3V(ms_aFriendlyPlayerPositionsOverride[i].m_vPosition - ZAXIS), 0.25f, Color_green, false);
			}
		}
		
		//Check if we are using AI overrides.
		if(ms_bUseAiOverrides)
		{
			//Draw the enemy AI positions.
			for(int i = 0; i < ms_aEnemyAiPositionsOverride.GetCount(); ++i)
			{
				grcDebugDraw::Capsule(VECTOR3_TO_VEC3V(ms_aEnemyAiPositionsOverride[i].m_vPosition + ZAXIS), VECTOR3_TO_VEC3V(ms_aEnemyAiPositionsOverride[i].m_vPosition - ZAXIS), 0.125f, Color_red, false);
			}

			//Draw the friendly AI positions.
			for(int i = 0; i < ms_aFriendlyAiPositionsOverride.GetCount(); ++i)
			{
				grcDebugDraw::Capsule(VECTOR3_TO_VEC3V(ms_aFriendlyAiPositionsOverride[i].m_vPosition + ZAXIS), VECTOR3_TO_VEC3V(ms_aFriendlyAiPositionsOverride[i].m_vPosition - ZAXIS), 0.125f, Color_green, false);
			}
		}
		
		//Draw the best bunch of enemy players position.
		if(m_vLargestBunchOfEnemyPlayersPosition.IsNonZero())
		{
			grcDebugDraw::Sphere(m_vLargestBunchOfEnemyPlayersPosition, 1.0f, Color_red, false);
		}
		
		//Draw the best bunch of friendly players position.
		if(m_vLargestBunchOfFriendlyPlayersPosition.IsNonZero())
		{
			grcDebugDraw::Sphere(m_vLargestBunchOfFriendlyPlayersPosition, 1.0f, Color_green, false);
		}
		
		//Draw the spawn points.
		for(int s=0; s<m_iNumBestSpawnPoints; s++)
		{
			TSpawnPoint sp = m_BestSpawnPoints[s];
			Vector3 vHdg(-rage::Sinf(sp.m_fHeading), rage::Cosf(sp.m_fHeading), 0.0f);
			
			//Calculate the color.
			//We want the color to range from green (best) to red (worst).
			Color32 color(0, 0, 0, 255);
			
			//Calculate the lerp value.
			//This will range from 0 (best) to 1 (worst).
			float fLerp = (float)s / (float)m_iNumBestSpawnPoints;
			
			//Calculate the red component.
			color.SetRed(Lerp(fLerp, 0, 255));
			
			//Calculate the green component.
			color.SetGreen(Lerp(fLerp, 255, 0));

			grcDebugDraw::Line(sp.m_vPos, sp.m_vPos+(ZAXIS*3.0f), color, color, 1);
			grcDebugDraw::Arrow(
				VECTOR3_TO_VEC3V(sp.m_vPos+(ZAXIS*3.0f)),
				VECTOR3_TO_VEC3V(sp.m_vPos+(ZAXIS*3.0f)+(vHdg*2.0f)),
				0.75f,
				color,
				1);
				
			//Check if we should debug scores.
			if(m_bDebugScores)
			{
				//Create storage for the strings.
				char scoresText[64];
				
				//Generate the coordinate.
				Vector3 vCoords = sp.m_vPos + (ZAXIS * 4.0f);
				
				//Generate the color.
				Color32 color = CRGBA(255, 128, 128, 255);
				
				//Keep track of the line.
				int iLine = 0;
				
				formatf(scoresText, "Total: %.2f", sp.m_Score.m_fTotal);
				grcDebugDraw::Text(vCoords, color, 0, iLine * grcDebugDraw::GetScreenSpaceTextHeight(), scoresText);
				++iLine;
				
				//Change the color.
				color = CRGBA(255, 192, 128, 255);
				
				formatf(scoresText, "FOV: %.2f", sp.m_Score.m_fFOV);
				grcDebugDraw::Text(vCoords, color, 0, iLine * grcDebugDraw::GetScreenSpaceTextHeight(), scoresText);
				++iLine;
				
				formatf(scoresText, "Proximity To Origin: %.2f", sp.m_Score.m_fProximityToOrigin);
				grcDebugDraw::Text(vCoords, color, 0, iLine * grcDebugDraw::GetScreenSpaceTextHeight(), scoresText);
				++iLine;
				
				formatf(scoresText, "Team Bunching: %.2f", sp.m_Score.m_fTeamBunching);
				grcDebugDraw::Text(vCoords, color, 0, iLine * grcDebugDraw::GetScreenSpaceTextHeight(), scoresText);
				++iLine;
				
				formatf(scoresText, "Enemy Players Farther: %.2f", sp.m_Score.m_fEnemyPlayersFarther);
				grcDebugDraw::Text(vCoords, color, 0, iLine * grcDebugDraw::GetScreenSpaceTextHeight(), scoresText);
				++iLine;
				
				formatf(scoresText, "Friendly Players Closer: %.2f", sp.m_Score.m_fFriendlyPlayersCloser);
				grcDebugDraw::Text(vCoords, color, 0, iLine * grcDebugDraw::GetScreenSpaceTextHeight(), scoresText);
				++iLine;
				
				formatf(scoresText, "Enemy AI Farther: %.2f", sp.m_Score.m_fEnemyAiFarther);
				grcDebugDraw::Text(vCoords, color, 0, iLine * grcDebugDraw::GetScreenSpaceTextHeight(), scoresText);
				++iLine;

				formatf(scoresText, "Friendly AI Closer: %.2f", sp.m_Score.m_fFriendlyAiCloser);
				grcDebugDraw::Text(vCoords, color, 0, iLine * grcDebugDraw::GetScreenSpaceTextHeight(), scoresText);
				++iLine;
				
				formatf(scoresText, "Randomness: %.2f", sp.m_Score.m_fRandomness);
				grcDebugDraw::Text(vCoords, color, 0, iLine * grcDebugDraw::GetScreenSpaceTextHeight(), scoresText);
				++iLine;
			}
		}
	}

	if(ms_bDrawResultForPedPositionProjectionTesting)
	{
		grcDebugDraw::Sphere(ms_vResultForPedPositionProjectionTesting, 1.0f, Color_purple);
	}
#endif
}

// NAME : StartSearch
// PURPOSE : Start a search with the given parameters (spherical area)
void CNetRespawnMgr::StartSearch(const Vector3 & vOrigin, const Vector3 & vTargetPos, const float fMinDist, const float fMaxDist, const fwFlags16 uFlags)
{
	//Create the input.
	StartSearchInput input(vOrigin);
	input.m_vTargetPos = vTargetPos;
	input.m_fMinDist = fMinDist;
	input.m_fMaxDist = fMaxDist;
	input.m_uFlags = uFlags;
	
	//Start the search.
	StartSearch(input);
}

// NAME : StartSearch
// PURPOSE : Start a search with the given parameters (angled area)
void CNetRespawnMgr::StartSearch(const Vector3 & vAngledAreaPt1, const Vector3 & vAngledAreaPt2, const float fAngledAreaWidth, const Vector3 & vTargetPos, const fwFlags16 uFlags)
{
	//Create the input.
	StartSearchInput input(vAngledAreaPt1, vAngledAreaPt2, fAngledAreaWidth);
	input.m_vTargetPos = vTargetPos;
	input.m_uFlags = uFlags;

	//Start the search.
	StartSearch(input);
}

// NAME : StartSearch
// PURPOSE : Start a search with the given parameters
void CNetRespawnMgr::StartSearch(const StartSearchInput& rInput)
{
	Assert(!m_bSearchActive);

	m_iSearchStartTimeMs = fwTimer::GetTimeInMilliseconds();
	m_iSeachNumFrames = 2;

	m_pPed = rInput.m_pPed;

	switch(rInput.m_iSearchVolumeType)
	{
		case SEARCH_VOLUME_SPHERE:
		{
			m_iSearchVolumeType = SEARCH_VOLUME_SPHERE;
			
			m_vSearchOrigin = rInput.m_vOrigin;
			m_fSearchMinDist = Min(rInput.m_fMinDist, ms_fSearchMaxDistCutoff);
			m_fSearchMaxDist = Min(rInput.m_fMaxDist, ms_fSearchMaxDistCutoff);
			m_vSearchMins = m_vSearchOrigin - Vector3(m_fSearchMaxDist, m_fSearchMaxDist, m_fSearchMaxDist);
			m_vSearchMaxs = m_vSearchOrigin + Vector3(m_fSearchMaxDist, m_fSearchMaxDist, m_fSearchMaxDist);
			break;
		}
		case SEARCH_VOLUME_ANGLED_AREA:
		{
			m_iSearchVolumeType = SEARCH_VOLUME_ANGLED_AREA;

			Vector3 vSearchExtents[2];
			m_SearchAngledArea.Set(rInput.m_vAngledAreaPt1, rInput.m_vAngledAreaPt2, rInput.m_fAngledAreaWidth, vSearchExtents);
			// Expand minmax so we have chance of visiting floor polygons if designers have placed points on ground
			vSearchExtents[0].z = Min(rInput.m_vAngledAreaPt1.z, rInput.m_vAngledAreaPt2.z) - 1.0f;
			vSearchExtents[1].z = Max(rInput.m_vAngledAreaPt1.z, rInput.m_vAngledAreaPt2.z) + 0.5f;
			m_vSearchMins = vSearchExtents[0];
			m_vSearchMaxs = vSearchExtents[1];
			// Derive the search origin & max distance from the min/max of the angled area
			m_vSearchOrigin = (m_vSearchMins + m_vSearchMaxs) * 0.5f;
			m_fSearchMaxDist = (vSearchExtents[0] - m_vSearchOrigin).Mag();
			break;
		}
		default:
		{
			FastAssert(false);
			break;
		}
	}

	m_vSearchTargetPos = rInput.m_uFlags.IsFlagSet(FLAG_IGNORE_TARGET_HEADING) ? ms_vSearchTargetPosNone : rInput.m_vTargetPos;

	m_iTeam = rInput.m_iTeam;
	m_uFlags = rInput.m_uFlags;
	
	if (m_uFlags.IsFlagSet(FLAG_IGNORE_Z))
	{
		m_vSearchMins.z = -MINMAX_MAX_FLOAT_VAL;
		m_vSearchMaxs.z = MINMAX_MAX_FLOAT_VAL;
	}
	else
	{
		m_vSearchMins.z = m_vSearchOrigin.z - ms_fZCutoff;
		m_vSearchMaxs.z = m_vSearchOrigin.z + ms_fZCutoff;	
	}

	//If a ped is specified, but no team, assign the ped's team.
	if(m_iTeam < 0 && m_pPed)
	{
		m_iTeam = GetNetTeamForPed(*m_pPed);
	}

	m_bSearchActive = true;
	m_bSearchComplete = false;
	m_bSearchSuccessful = false;

	m_iNumBestSpawnPoints = 0;

	for(int n=0; n<m_Timeslice_StoredNavmeshes.GetMaxCount(); n++)
		m_Timeslice_StoredNavmeshes[n] = NAVMESH_NAVMESH_INDEX_NONE;
	m_Timeslice_iCurrentNavmesh = -1;
	m_Timeslice_iCurrentPoly = -1;

	m_fCachedNavmeshLoadDistance = CPathServer::GetNavMeshLoadDistance();
	CPathServer::SetNavMeshLoadDistance(m_fCachedNavmeshLoadDistance/2.0f);

	CPathServer::AddNavMeshRegion( NMR_NetworkRespawnMgr, THREAD_INVALID, m_vSearchOrigin.x, m_vSearchOrigin.y, m_fSearchMaxDist );
}
// NAME : CancelSearch
// PURPOSE : Abort current search
void CNetRespawnMgr::CancelSearch()
{
	m_bSearchActive = false;
	m_bSearchComplete = false;
	m_bSearchSuccessful = false;

	if(m_fCachedNavmeshLoadDistance >= 0.0f)
		CPathServer::SetNavMeshLoadDistance(m_fCachedNavmeshLoadDistance);

	if( CPathServer::GetIsNavMeshRegionRequired( NMR_NetworkRespawnMgr) )
	{
		CPathServer::RemoveNavMeshRegion( NMR_NetworkRespawnMgr, THREAD_INVALID );
	}

	m_fCachedNavmeshLoadDistance = -1.0f;
}

// NAME : FindLargestBunchOfPlayers
// PURPOSE : Finds the largest bunch of players
bool CNetRespawnMgr::FindLargestBunchOfPlayers(bool bFriendly, Vector3& vPosition)
{
	//Find the enemy and friendly player positions.
	Positions enemyPlayerPositions;
	Positions friendlyPlayerPositions;
	FindEnemyAndFriendlyPlayerPositions(enemyPlayerPositions, friendlyPlayerPositions);
	
	//Choose the positions.
	Positions& rPositions = bFriendly ? friendlyPlayerPositions : enemyPlayerPositions;
	
	//Keep track of the largest bunch.
	struct Bunch
	{
		Bunch()
			: m_iSize(0)
			, m_vPosition(VEC3_ZERO)
		{

		}

		int		m_iSize;
		Vector3	m_vPosition;
	};
	Bunch largestBunch;
	
	//Iterate over the positions.
	for(int i = 0; i < rPositions.GetCount(); ++i)
	{
		//Grab the start position.
		Vector3 vStartPosition = rPositions[i].m_vPosition;
		
		//Start a bunch.
		int iBunchSize = 1;
		Vector3 vTotalBunchPosition = vStartPosition;

		//Iterate over the positions.
		for(int j = 0; j < rPositions.GetCount(); ++j)
		{
			//Do not count the same position twice.
			if(i == j)
			{
				continue;
			}
			
			//Grab the other position.
			Vector3 vOtherPosition = rPositions[j].m_vPosition;

			//Ensure the distance is within the threshold.
			float fDistSq = vStartPosition.Dist2(vOtherPosition);
			float fMaxDistSq = square(ms_fMaxDistanceForBunchingPlayers);
			if(fDistSq > fMaxDistSq)
			{
				continue;
			}
			
			//Add to the bunch.
			iBunchSize++;
			vTotalBunchPosition += vOtherPosition;
		}

		//Ensure the bunch is the larger.
		if(iBunchSize <= largestBunch.m_iSize)
		{
			continue;
		}

		//Assign the largest bunch.
		largestBunch.m_iSize = iBunchSize;
		largestBunch.m_vPosition = vTotalBunchPosition / (float)iBunchSize;
	}

	//Ensure the largest bunch is valid.
	if(largestBunch.m_iSize <= 0)
	{
		return false;
	}

	//Assign the position.
	vPosition = largestBunch.m_vPosition;

	return true;
}

// NAME : IsSearchActive
// PURPOSE : Returns whether a search is currently active
bool CNetRespawnMgr::IsSearchActive()
{
	return m_bSearchActive;
}
// NAME : IsSearchComplete
// PURPOSE : Returns whether the search has completed processing
bool CNetRespawnMgr::IsSearchComplete()
{
	return m_bSearchComplete;
}
// NAME : HasSearchFailed
// PURPOSE : Returns whether the search has completed unsuccessfully
bool CNetRespawnMgr::WasSearchSuccessful()
{
	Assertf(!m_bSearchActive, "script called WasSearchSuccessful() when search was still active.");
	Assertf(m_bSearchComplete, "script called WasSearchSuccessful() when search was not yet complete.");

	return m_bSearchSuccessful;
}
// NAME : GetNumSearchResults
// PURPOSE : Returns the number of search results found
int CNetRespawnMgr::GetNumSearchResults()
{
	Assertf(!m_bSearchActive, "script called GetSearchResults() when search was still active.");
	Assertf(m_bSearchComplete, "script called GetSearchResults() when search was not yet complete.");
	Assertf(m_bSearchSuccessful, "script called GetSearchResults() when search was not successful.");

	return m_iNumBestSpawnPoints;
}
// NAME : GetSearchResults
// PURPOSE : Retrieves the search results
void CNetRespawnMgr::GetSearchResults(const int iIndex, Vector3 & vSpawnPosOut, float & fSpawnHeadingOut)
{
	Assertf(!m_bSearchActive, "script called GetSearchResults() when search was still active.");
	Assertf(m_bSearchComplete, "script called GetSearchResults() when search was not yet complete.");
	Assertf(m_bSearchSuccessful, "script called GetSearchResults() when search was not successful.");

	Assert(iIndex >= 0 && iIndex < m_iNumBestSpawnPoints);

	vSpawnPosOut = m_BestSpawnPoints[iIndex].m_vPos;
	fSpawnHeadingOut = m_BestSpawnPoints[iIndex].m_fHeading;
}
// NAME : GetSearchResultFlags
// PURPOSE : Retrieves the flags associated with a spawn point result
s32 CNetRespawnMgr::GetSearchResultFlags(const int iIndex)
{
	Assertf(!m_bSearchActive, "script called GetSearchResultFlags() when search was still active.");
	Assertf(m_bSearchComplete, "script called GetSearchResultFlags() when search was not yet complete.");
	Assertf(m_bSearchSuccessful, "script called GetSearchResultFlags() when search was not successful.");

	Assert(iIndex >= 0 && iIndex < m_iNumBestSpawnPoints);

	return m_BestSpawnPoints[iIndex].m_iFlags;
}
// NAME : StreamAroundSearchOrigin
// PURPOSE : Requests/streams collision & navmesh around the search origin
//			 Returns true once all the required data has been streamed into memory
bool CNetRespawnMgr::StreamAroundSearchOrigin()
{
	static bool bStreamNavmesh = true;

	bool bNavmeshComplete = !bStreamNavmesh;

	if(bStreamNavmesh)
	{
		bNavmeshComplete = CPathServer::AreAllNavMeshRegionsLoaded();
	}

	return (bNavmeshComplete);
}
// NAME : UpdateSearch
// PURPOSE : Internally updates the search process
bool CNetRespawnMgr::UpdateSearch()
{
	Assert(m_bSearchActive);
	Assert(!m_bSearchComplete);

	// Sanity-check the search duration; if designers forget to cancel a respawn search it'll stuff up the game beyond belief..
	u32 iTimeDelta = fwTimer::GetTimeInMilliseconds() - m_iSearchStartTimeMs;
	if(m_uFlags.IsFlagSet(FLAG_SCRIPT_COMMAND) && iTimeDelta > SCRIPTERS_FORGOT_TO_CANCEL_SEARCH_MS)
	{
		Assertf(false, "CNetRespawnMgr : Script respawn search has been going on for over 10 secs.  Did you forget to cancel it with \"NETWORK_CANCEL_RESPAWN_SEARCH\"?  Quitting search now..");
		CancelSearch();
		return false;
	}
	
	//Update the bunches.
	UpdateLargestBunches();

	LOCK_NAVMESH_DATA


	// Countdown at start
	if(m_iSeachNumFrames > 0)
	{
		m_iSeachNumFrames--;
		return true;
	}

	if(m_Timeslice_iCurrentNavmesh == -1)
	{
		s32 iNumMeshes = 0;
		aiNavMeshStore * pNavMeshStore = fwPathServer::GetNavMeshStore(kNavDomainRegular);
		for(s32 n=0; n<pNavMeshStore->GetMaxMeshIndex(); n++)
		{
			CNavMesh * pNavMesh = pNavMeshStore->GetMeshByIndex(n);
			if(pNavMesh && geomBoxes::TestAABBtoAABB(m_vSearchMins, m_vSearchMaxs, pNavMesh->GetQuadTree()->m_Mins, pNavMesh->GetQuadTree()->m_Maxs) )
			{
				m_Timeslice_StoredNavmeshes[iNumMeshes++] = n;

				if(iNumMeshes >= m_Timeslice_StoredNavmeshes.GetMaxCount())
				{
					break;
				}
			}
		}
		m_Timeslice_iCurrentNavmesh = 0;
		m_Timeslice_iCurrentPoly = 0;
	}

	static dev_s32 iNumPolysPerIteration_Default = 250;
	static dev_s32 iNumPolysPerIteration_Faded = 2500;

	dev_s32 iNumPolysPerIteration = (camInterface::GetFadeLevel() > 0.9f) ? iNumPolysPerIteration_Faded : iNumPolysPerIteration_Default;

	if(m_Timeslice_iCurrentNavmesh < ms_iMaxStoredNavmeshes &&
		m_Timeslice_StoredNavmeshes[m_Timeslice_iCurrentNavmesh] != NAVMESH_NAVMESH_INDEX_NONE)
	{
		TShortMinMax searchMinMax;
		searchMinMax.SetFloat(
			Max(m_vSearchMins.x, -MINMAX_MAX_FLOAT_VAL),
			Max(m_vSearchMins.y, -MINMAX_MAX_FLOAT_VAL),
			Max(m_vSearchMins.z, -MINMAX_MAX_FLOAT_VAL),
			Max(m_vSearchMaxs.x, MINMAX_MAX_FLOAT_VAL),
			Max(m_vSearchMaxs.y, MINMAX_MAX_FLOAT_VAL),
			Max(m_vSearchMaxs.z, MINMAX_MAX_FLOAT_VAL));

		CNavMesh * pNavMesh = fwPathServer::GetNavMeshFromIndex(m_Timeslice_StoredNavmeshes[m_Timeslice_iCurrentNavmesh], kNavDomainRegular);
		if(!pNavMesh)
		{
			m_Timeslice_iCurrentNavmesh++;
			m_Timeslice_iCurrentPoly = 0;
		}
		else
		{
			s32 iNumIters = iNumPolysPerIteration;
			while(m_Timeslice_iCurrentPoly < pNavMesh->GetNumPolys() && iNumIters > 0)
			{
				TNavMeshPoly * pPoly = pNavMesh->GetPoly(m_Timeslice_iCurrentPoly);

				if(pPoly->GetIsNetworkSpawnCandidate() && searchMinMax.Intersects(pPoly->m_MinMax))
				{
					if( (!m_uFlags.IsFlagSet(FLAG_MAY_SPAWN_IN_INTERIOR) &&  pPoly->GetIsInterior()) ||
						(!m_uFlags.IsFlagSet(FLAG_MAY_SPAWN_IN_EXTERIOR) && !pPoly->GetIsInterior()))
					{
						m_Timeslice_iCurrentPoly++;
						continue;
					}

					Vector3 vCentroid;
					pNavMesh->GetPolyCentroid(m_Timeslice_iCurrentPoly, vCentroid);

					// Test candidate point for containment within our spawn volume
					bool bWithinVolume = false;
					switch(m_iSearchVolumeType)
					{
						case SEARCH_VOLUME_SPHERE:
						{
							float fDistSqr = (vCentroid - m_vSearchOrigin).XYMag2();
							if(fDistSqr > m_fSearchMinDist*m_fSearchMinDist && fDistSqr < m_fSearchMaxDist*m_fSearchMaxDist)
							{
								bWithinVolume = true;
							}
							break;
						}
						case SEARCH_VOLUME_ANGLED_AREA:
						{
							bWithinVolume = m_SearchAngledArea.TestPoint(vCentroid);
							break;
						}
					}
					
					// Ensure valid, and evaluate spawn point
					if(bWithinVolume)
					{
						// Only decrement num iterations when we actually do some work (below)
						iNumIters--;

						bool bCanUsePoly = false;

						// hack fix until artists stop putting terrain up in the fucking air, underneath the cutoff height we've all agreed upon
						if(Abs(vCentroid.z - m_vSearchOrigin.z) < ms_fZCutoff || m_uFlags.IsFlagSet(FLAG_IGNORE_Z)) 
						{
							//Evaluate the spawn point.
							TSpawnPoint spawnPoint;
							spawnPoint.m_vPos = vCentroid;
							if(pPoly->GetIsPavement())
								spawnPoint.m_iFlags |= TSpawnPoint::FLAG_PAVEMENT;
							if(pPoly->GetIsInterior())
								spawnPoint.m_iFlags |= TSpawnPoint::FLAG_INTERIOR;
							
							if(EvaluateSpawnPoint(pPoly, spawnPoint))
							{
								bCanUsePoly = true;
								
								//Insert the spawn point.
								InsertSpawnPoint(spawnPoint);
							}
						}

						if(bCanUsePoly)
						{
							#if __BANK && DEBUG_DRAW
							if(m_bDebugSearch)
							{
								grcDebugDraw::Line(vCentroid, vCentroid+ZAXIS, Color_green, Color_green, 10);
							}
							#endif
						}
						else
						{
							#if __BANK && DEBUG_DRAW
							if(m_bDebugSearch)
								grcDebugDraw::Line(vCentroid, vCentroid+ZAXIS, Color_red, Color_red, 10);
							#endif
						}
					}
				}

				m_Timeslice_iCurrentPoly++;
			}

			if(m_Timeslice_iCurrentPoly >= pNavMesh->GetNumPolys())
			{
				m_Timeslice_iCurrentNavmesh++;
				m_Timeslice_iCurrentPoly = 0;
			}
		}
	}
	else
	{
		m_bSearchActive = false;
		m_bSearchComplete = true;
		m_bSearchSuccessful = (m_iNumBestSpawnPoints > 0);

		m_Timeslice_iCurrentNavmesh = -1;
		m_Timeslice_iCurrentPoly = -1;

		CPathServer::SetNavMeshLoadDistance(m_fCachedNavmeshLoadDistance);

		if( CPathServer::GetIsNavMeshRegionRequired( NMR_NetworkRespawnMgr) )
		{
			CPathServer::RemoveNavMeshRegion( NMR_NetworkRespawnMgr, THREAD_INVALID );
		}

#if __BANK
		if(m_bSearchSuccessful)
		{
			AddSpawnRecord();
		}
#endif
	}

	UNLOCK_NAVMESH_DATA

	return !m_bSearchActive;
}

bool CNetRespawnMgr::InsertSpawnPoint(const TSpawnPoint& rSpawnPoint)
{
	s32 s,s2;
	for(s=0; s<m_iNumBestSpawnPoints; s++)
	{
		if(rSpawnPoint.m_Score.m_fTotal > m_BestSpawnPoints[s].m_Score.m_fTotal)
		{
			for(s2=m_iNumBestSpawnPoints; s2>s; s2--)
			{
				m_BestSpawnPoints[s2] = m_BestSpawnPoints[s2-1];
			}
			m_BestSpawnPoints[s] = rSpawnPoint;

			if(m_iNumBestSpawnPoints < ms_iMaxSpawnPoints)
				m_iNumBestSpawnPoints++;
			return true;
		}
	}
	if(m_iNumBestSpawnPoints < ms_iMaxSpawnPoints)
	{
		m_BestSpawnPoints[m_iNumBestSpawnPoints] = rSpawnPoint;

		m_iNumBestSpawnPoints++;
		return true;
	}

	return false;
}

float CNetRespawnMgr::FindFieldOfViewAtPosition(const Vector3 & vPos, TNavMeshPoly * pPoly, const float fBaseHeading)
{
	const float fRequiredClearDist = Min(15.0f, SHORT_LINE_OF_SIGHT_MAXDIST-1.0f);
	const float fMaxTestAngle = PI;
	const float fTestAngleInc = fMaxTestAngle / ((float)RESPAWNMGR_NUM_HEADINGS);

	float fAngleMod = 0.0f;

	while(fAngleMod < fMaxTestAngle)
	{
		// Left test
		float fAngle = fwAngle::LimitRadianAngle(fBaseHeading+fAngleMod);
		Vector2 vTestDir = Vector2(-rage::Sinf(fAngle), rage::Cosf(fAngle)) * fRequiredClearDist;

		TNavMeshPoly * pEndPoly = CPathServer::TestShortLineOfSightImmediate(vPos, vTestDir, pPoly, NULL, NULL, NULL, kNavDomainRegular);
		if(!pEndPoly)
			break;

		// Right test
		if(fAngleMod != 0.0f)
		{
			fAngle = fwAngle::LimitRadianAngle(fBaseHeading-fAngleMod);
			vTestDir = Vector2(-rage::Sinf(fAngle), rage::Cosf(fAngle)) * fRequiredClearDist;

			pEndPoly = CPathServer::TestShortLineOfSightImmediate(vPos, vTestDir, pPoly, NULL, NULL, NULL, kNavDomainRegular);
			if(!pEndPoly)
				break;
		}

		// Step
		fAngleMod += fTestAngleInc;
	}

	return fAngleMod;
}

// NAME : EvaluateSpawnPoint
// PURPOSE : Evalulates the spawn point.
bool CNetRespawnMgr::EvaluateSpawnPoint(TNavMeshPoly* pPoly, TSpawnPoint& rSpawnPoint)
{
	//Evaluate the FOV and heading.
	if(!EvaluateFOVAndHeading(pPoly, rSpawnPoint))
	{
		return false;
	}
	
	//Evaluate the proximity to the origin.
	EvaluateProximityToOrigin(rSpawnPoint);
	
	//Evaluate the team bunching.
	EvaluateTeamBunching(rSpawnPoint);
	
	//Evaluate the player positions.
	EvaluatePlayerPositions(rSpawnPoint);
	
	//Evaluate the AI positions.
	EvaluateAiPositions(rSpawnPoint);
	
	//Evaluate the randomness.
	EvaluateRandomness(rSpawnPoint);

	return true;
}

void CNetRespawnMgr::AddPosition(Positions& rPositions, const CPed& rPed, bool bUseProjectedPosition)
{
	//Assert that the positions are not full.
	aiAssert(!rPositions.IsFull());

	//Add the position.
	Position& rPosition = rPositions.Append();
	rPosition.Reset();

	//Check if we should not use the projected position.
	if(!bUseProjectedPosition)
	{
		//Set the position.
		rPosition.m_vPosition = VEC3V_TO_VECTOR3(rPed.GetTransform().GetPosition());
	}
	else
	{
		//Calculate the projected position.
		float fTime = 1.0f;
		float fDistanceFromStart = 0.0f;
		rPosition.m_vPosition = VEC3V_TO_VECTOR3(CalculateProjectedPosition(rPed, fTime, fDistanceFromStart));
	}

	//Set the flags.
	if(rPed.GetIsInVehicle() && rPed.GetVehiclePedInside()->GetIsAircraft())
	{
		rPosition.m_uFlags.SetFlag(Position::IsInAircraft);
	}
}

Vec3V_Out CNetRespawnMgr::CalculateProjectedPosition(const CPed& rPed, float fTime, float& fDistanceFromStart)
{
	//Note: This code was ported straight from MP3.
	//		No changes have been made except where API's differed.

	Vector3 projectedposition(Vector3::ZeroType);
	fDistanceFromStart = 0.f;

	//Get the position.
	Vector3 pos = VEC3V_TO_VECTOR3(rPed.GetTransform().GetPosition());

	//Set the projectedposition to the current position as a baseline
	projectedposition = pos;

	//Get the velocity.
	Vector3 velocity = rPed.GetVelocity();

	const float velocityProcessingThreshold = 0.1f;
	float velocityMag = velocity.Mag();
	if (velocityMag > velocityProcessingThreshold)
	{
		const float partspersecond = 5.f;
		int parts = (int) (fTime * partspersecond);

		bool bProceed = true;

		Vector3 vDeltaPart = (velocity * (1.f/partspersecond));

		float fDeltaZ = rPed.IsProne() ? 0.2f : rPed.GetIsCrouching() ? 0.6f : 1.f;

		Vector3 vPrevStepPos = pos;
		Vector3 vNextStepPos = pos;

		Vector3 hitpos(Vector3::ZeroType);

		while (bProceed && parts > 0)
		{
			vPrevStepPos = vNextStepPos;
			vNextStepPos += vDeltaPart;

			{
				const int numIsects = 8; //reduced from 16, as we get the closest within the first 8
				phIntersection testIntersections[numIsects];

				u32 nFlags = ArchetypeFlags::GTA_ALL_MAP_TYPES | ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_PED_TYPE | ArchetypeFlags::GTA_OBJECT_TYPE;

				const float fCylinderRadius = 0.2f;
				int nNumResults = CPhysics::GetLevel()->TestCapsule(vPrevStepPos, vNextStepPos, fCylinderRadius, testIntersections, NULL, nFlags, TYPE_FLAGS_ALL, phLevelBase::STATE_FLAGS_ALL, true, numIsects);

				int nUseIntersection = -1;
				for(int i=0; i<nNumResults; i++)
				{
					if(testIntersections[i].IsAHit())
					{
						// check we've got the first intersection along the probe
						if(nUseIntersection < 0 || testIntersections[i].GetT() < testIntersections[nUseIntersection].GetT())
						{
							nUseIntersection = i;
						}
					}
					else
						break;
				}

				if(nUseIntersection > -1)
				{
					//Need to position a little back from the point of intersection to allow place for body to be at
					hitpos = VEC3V_TO_VECTOR3(testIntersections[nUseIntersection].GetPosition());
					Vector3 backdir = vPrevStepPos - hitpos;
					backdir.NormalizeFast();

					const float fEstimatedPedRadius = 0.5f;
					vNextStepPos = hitpos + (backdir * fEstimatedPedRadius);
					projectedposition = vNextStepPos;
					bProceed = false;
				}
			}

			if (bProceed)
			{
				{
					const int numIsects = 8; //reduced from 16, as we get the closest within the first 8
					phIntersection testIntersections[numIsects];
					phSegment testSegment;

					Vector3 vNextStepPosDown = vNextStepPos;
					vNextStepPosDown.z -= 10.f;

					testSegment.Set(vNextStepPos, vNextStepPosDown);

					u32 nFlags = ArchetypeFlags::GTA_ALL_MAP_TYPES | ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_PED_TYPE | ArchetypeFlags::GTA_OBJECT_TYPE;
					int nNumResults = CPhysics::GetLevel()->TestProbe(testSegment, testIntersections, rPed.GetCurrentPhysicsInst(), nFlags, TYPE_FLAGS_ALL, phLevelBase::STATE_FLAGS_ALL, numIsects);

					int nUseIntersection = -1;
					for(int i=0; i<nNumResults; i++)
					{
						if(testIntersections[i].IsAHit())
						{
							// check we've got the first intersection along the probe
							if(nUseIntersection < 0 || testIntersections[i].GetT() < testIntersections[nUseIntersection].GetT())
							{
								nUseIntersection = i;
							}
						}
						else
							break;
					}

					if(nUseIntersection > -1)
					{
						hitpos = VEC3V_TO_VECTOR3(testIntersections[nUseIntersection].GetPosition());
						vNextStepPos = hitpos;
						vNextStepPos.z += fDeltaZ;
					}
				}
			}

			parts--;
		}

		if (bProceed)
			projectedposition = vNextStepPos;

		fDistanceFromStart = (projectedposition - pos).Mag();
	}

	return VECTOR3_TO_VEC3V(projectedposition);
}

void CNetRespawnMgr::EvaluateAiPositions(TSpawnPoint& rSpawnPoint)
{
	//Ensure we care about the AI.
	bool bCareAboutEnemies		= m_uFlags.IsFlagSet(FLAG_PREFER_ENEMY_AI_FARTHER);
	bool bCareAboutFriendlies	= m_uFlags.IsFlagSet(FLAG_PREFER_FRIENDLY_AI_CLOSER);
	if(!bCareAboutEnemies && !bCareAboutFriendlies)
	{
		return;
	}

	//Create the input.
	EvaluatePositionsInput input;

	//Assign the care flags.
	input.m_bCareAboutEnemies		= bCareAboutEnemies;
	input.m_bCareAboutFriendlies	= bCareAboutFriendlies;

	//Find the closest enemy and friendly AI.
	FindClosestEnemyAndFriendlyAi(rSpawnPoint.m_vPos, input.m_fClosestEnemyDistSq, input.m_fClosestFriendlyDistSq);

	//Assign the distances.
	input.m_fMinDistanceForScoringEnemiesFarther		= ms_fMinDistanceForScoringEnemyAiFarther;
	input.m_fMaxDistanceForScoringEnemiesFarther		= ms_fMaxDistanceForScoringEnemyAiFarther;
	input.m_fMinDistanceForScoringFriendliesCloser		= ms_fMinDistanceForScoringFriendlyAiCloser;
	input.m_fIdealDistanceForScoringFriendliesCloser	= ms_fIdealDistanceForScoringFriendlyAiCloser;
	input.m_fMaxDistanceForScoringFriendliesCloser		= ms_fMaxDistanceForScoringFriendlyAiCloser;

	//Assign the scores.
	input.m_fMinScoreEnemiesFarther		= ms_fMinScoreEnemyAiFarther;
	input.m_fMaxScoreEnemiesFarther		= ms_fMaxScoreEnemyAiFarther;
	input.m_fMinScoreFriendliesCloser	= ms_fMinScoreFriendlyAiCloser;
	input.m_fMaxScoreFriendliesCloser	= ms_fMaxScoreFriendlyAiCloser;
	input.m_fMinScoreFriendliesTooClose = ms_fMinScoreFriendlyAiTooClose;
	input.m_fMaxScoreFriendliesTooClose = ms_fMaxScoreFriendlyAiTooClose;

	//Create the output.
	EvaluatePositionsOutput output;

	//Evaluate the positions.
	EvaluatePositions(input, output);

#if __BANK
	rSpawnPoint.m_Score.m_fEnemyAiFarther = output.m_fScoreEnemiesFarther;
	rSpawnPoint.m_Score.m_fFriendlyAiCloser = output.m_fScoreFriendliesCloser;
#endif

	//Add the score.
	rSpawnPoint.m_Score.m_fTotal += output.m_fScoreEnemiesFarther;
	rSpawnPoint.m_Score.m_fTotal += output.m_fScoreFriendliesCloser;
}

bool CNetRespawnMgr::EvaluateFOVAndHeading(TNavMeshPoly* pPoly, TSpawnPoint& rSpawnPoint)
{
	float fChosenHeading = ms_fSearchHeadingNone;
	float fFOV = 0.0f;

	//-------------------------------------------------------------------------
	// If we have a target position, then evaluate the FOV towards this target

	if(! m_vSearchTargetPos.IsClose(ms_vSearchTargetPosNone, 0.1f) )
	{
		const Vector3 vToTarget = m_vSearchTargetPos - rSpawnPoint.m_vPos;
		fChosenHeading = fwAngle::LimitRadianAngleSafe( fwAngle::GetRadianAngleBetweenPoints( vToTarget.x, vToTarget.y, 0.0f, 0.0f ) );

		fFOV = FindFieldOfViewAtPosition(rSpawnPoint.m_vPos, pPoly, fChosenHeading);
	}

	//----------------------------------------------------------------------------
	// If we have a target heading, then evaluate the FOV in that given direction

	else if( m_fSearchOptimalHeading != ms_fSearchHeadingNone )
	{
		fChosenHeading = m_fSearchOptimalHeading;
		fFOV = FindFieldOfViewAtPosition(rSpawnPoint.m_vPos, pPoly, fChosenHeading);
	}

	//-------------------------------------------------------------------------------------
	// Otherwise evaluate N different directions, and choose the one with the widest FOV

	else
	{
		int iBestHeading = -1;
		float fBestHeadingFOV = 0.0f;

		float fHeadingFOV[RESPAWNMGR_NUM_HEADINGS];
		for(int h=0; h<RESPAWNMGR_NUM_HEADINGS; h++)
			fHeadingFOV[h] = 0.0f;

		int h;
		for(h=0; h<RESPAWNMGR_NUM_HEADINGS; h++)
		{
			float fBaseHeading = (h > 0) ? (TWO_PI / ((float)RESPAWNMGR_NUM_HEADINGS)) * ((float)h) : 0.0f;

			fHeadingFOV[h] = FindFieldOfViewAtPosition(rSpawnPoint.m_vPos, pPoly, fBaseHeading);

			if(fHeadingFOV[h] > fBestHeadingFOV)
			{
				fBestHeadingFOV = fHeadingFOV[h];
				iBestHeading = h;
			}
		}

		// If no headings passed the test then return false
		if(iBestHeading == -1)
		{
			return false;
		}

		// pick randomly from the angles with the widest FOV
		int iBestHeadings[RESPAWNMGR_NUM_HEADINGS];
		int iNumBestHeadings = 0;
		for(h=0; h<RESPAWNMGR_NUM_HEADINGS; h++)
		{
			if(fHeadingFOV[h] == fBestHeadingFOV)
				iBestHeadings[iNumBestHeadings++] = h;
		}
		Assert(iNumBestHeadings > 0);

		int iRandomHeading = iBestHeadings[ fwRandom::GetRandomNumberInRange(0, iNumBestHeadings) ];
		fChosenHeading = (iRandomHeading > 0) ? (TWO_PI / ((float)RESPAWNMGR_NUM_HEADINGS)) * ((float)iRandomHeading) : 0.0f;
		fFOV = fBestHeadingFOV;
	}

	//Assign the heading.
	rSpawnPoint.m_fHeading = fChosenHeading;

	//Ensure we care about a wide FOV.
	if(!m_uFlags.IsFlagSet(FLAG_PREFER_WIDE_FOV))
	{
		return true;
	}

	//At this point, the FOV should range from 0 ... PI.
	//Normalize the FOV.
	static float sPiInverted = 1.0f / PI;
	float fNormal = fFOV * sPiInverted;

	//Calculate the score.
	fNormal = Clamp(fNormal, 0.0f, 1.0f);
	float fScore = Lerp(fNormal, ms_fMinScoreFOV, ms_fMaxScoreFOV);
	
#if __BANK
	rSpawnPoint.m_Score.m_fFOV = fScore;
#endif
	
	//Add the score.
	rSpawnPoint.m_Score.m_fTotal += fScore;
	
	return true;
}

void CNetRespawnMgr::EvaluatePlayerPositions(TSpawnPoint& rSpawnPoint)
{
	//Ensure we care about the players.
	bool bCareAboutEnemies		= m_uFlags.IsFlagSet(FLAG_PREFER_ENEMY_PLAYERS_FARTHER);
	bool bCareAboutFriendlies	= m_uFlags.IsFlagSet(FLAG_PREFER_FRIENDLY_PLAYERS_CLOSER);
	if(!bCareAboutEnemies && !bCareAboutFriendlies)
	{
		return;
	}
	
	//Create the input.
	EvaluatePositionsInput input;
	
	//Assign the care flags.
	input.m_bCareAboutEnemies		= bCareAboutEnemies;
	input.m_bCareAboutFriendlies	= bCareAboutFriendlies;

	//Find the closest enemy and friendly players.
	FindClosestEnemyAndFriendlyPlayers(rSpawnPoint.m_vPos, input.m_fClosestEnemyDistSq, input.m_fClosestFriendlyDistSq);
	
	//Assign the distances.
	input.m_fMinDistanceForScoringEnemiesFarther		= ms_fMinDistanceForScoringEnemyPlayersFarther;
	input.m_fMaxDistanceForScoringEnemiesFarther		= ms_fMaxDistanceForScoringEnemyPlayersFarther;
	input.m_fMinDistanceForScoringFriendliesCloser		= ms_fMinDistanceForScoringFriendlyPlayersCloser;
	input.m_fIdealDistanceForScoringFriendliesCloser	= ms_fIdealDistanceForScoringFriendlyPlayersCloser;
	input.m_fMaxDistanceForScoringFriendliesCloser		= ms_fMaxDistanceForScoringFriendlyPlayersCloser;
	
	//Assign the scores.
	input.m_fMinScoreEnemiesFarther		= ms_fMinScoreEnemyPlayersFarther;
	input.m_fMaxScoreEnemiesFarther		= ms_fMaxScoreEnemyPlayersFarther;
	input.m_fMinScoreFriendliesCloser	= ms_fMinScoreFriendlyPlayersCloser;
	input.m_fMaxScoreFriendliesCloser	= ms_fMaxScoreFriendlyPlayersCloser;
	input.m_fMinScoreFriendliesTooClose = ms_fMinScoreFriendlyPlayersTooClose;
	input.m_fMaxScoreFriendliesTooClose = ms_fMaxScoreFriendlyPlayersTooClose;
	
	//Create the output.
	EvaluatePositionsOutput output;
	
	//Evaluate the positions.
	EvaluatePositions(input, output);
	
#if __BANK
	rSpawnPoint.m_Score.m_fEnemyPlayersFarther = output.m_fScoreEnemiesFarther;
	rSpawnPoint.m_Score.m_fFriendlyPlayersCloser = output.m_fScoreFriendliesCloser;
#endif

	//Add the score.
	rSpawnPoint.m_Score.m_fTotal += output.m_fScoreEnemiesFarther;
	rSpawnPoint.m_Score.m_fTotal += output.m_fScoreFriendliesCloser;
}

void CNetRespawnMgr::EvaluatePositions(const EvaluatePositionsInput& rInput, EvaluatePositionsOutput& rOutput)
{
	//Check if we care about enemies being farther away.
	rOutput.m_fScoreEnemiesFarther = 0.0f;
	if(rInput.m_bCareAboutEnemies)
	{
		//Check if the closest enemy is valid.
		if(rInput.m_fClosestEnemyDistSq > 0.0f)
		{
			//Clamp the distance to values we care about.
			float fMinDistSq = square(rInput.m_fMinDistanceForScoringEnemiesFarther);
			float fMaxDistSq = square(rInput.m_fMaxDistanceForScoringEnemiesFarther);
			float fDistSq = Clamp(rInput.m_fClosestEnemyDistSq, fMinDistSq, fMaxDistSq);

			//Normalize the value.
			float fNormal = (fDistSq - fMinDistSq) / (fMaxDistSq - fMinDistSq);

			//Calculate the score.
			fNormal = Clamp(fNormal, 0.0f, 1.0f);
			rOutput.m_fScoreEnemiesFarther = Lerp(fNormal, rInput.m_fMinScoreEnemiesFarther, rInput.m_fMaxScoreEnemiesFarther);
		}
	}

	//Check if we care about friendlies being closer.
	rOutput.m_fScoreFriendliesCloser = 0.0f;
	if(rInput.m_bCareAboutFriendlies)
	{
		//Check if the closest friendly is valid.
		if(rInput.m_fClosestFriendlyDistSq > 0.0f)
		{	
			//Check if the distance has exceeded the ideal distance.
			float fDistSq = rInput.m_fClosestFriendlyDistSq;
			float fIdealDistSq = square(rInput.m_fIdealDistanceForScoringFriendliesCloser);
			if(fDistSq >= fIdealDistSq)
			{
				//Clamp the distance to values we care about.
				float fMaxDistSq = square(rInput.m_fMaxDistanceForScoringFriendliesCloser);
				fDistSq = Clamp(fDistSq, fIdealDistSq, fMaxDistSq);

				//Normalize the value.
				float fNormal = (fDistSq - fIdealDistSq) / (fMaxDistSq - fIdealDistSq);

				//Flip the value, since values closer to the ideal are better and should therefore be scored higher.
				fNormal = 1.0f - fNormal;

				//Calculate the score.
				fNormal = Clamp(fNormal, 0.0f, 1.0f);
				rOutput.m_fScoreFriendliesCloser = Lerp(fNormal, rInput.m_fMinScoreFriendliesCloser, rInput.m_fMaxScoreFriendliesCloser);
			}
			else
			{
				//Clamp the distance to values we care about.
				float fMinDistSq = square(rInput.m_fMinDistanceForScoringFriendliesCloser);
				fDistSq = Clamp(fDistSq, fMinDistSq, fIdealDistSq);

				//Normalize the value.
				float fNormal = (fDistSq - fMinDistSq) / (fIdealDistSq - fMinDistSq);

				//Calculate the score.
				fNormal = Clamp(fNormal, 0.0f, 1.0f);
				rOutput.m_fScoreFriendliesCloser = Lerp(fNormal, rInput.m_fMinScoreFriendliesTooClose, rInput.m_fMaxScoreFriendliesTooClose);
			}
		}
	}
}

void CNetRespawnMgr::EvaluateProximityToOrigin(TSpawnPoint& rSpawnPoint)
{
	//Ensure we care about being close to the origin.
	if(!m_uFlags.IsFlagSet(FLAG_PREFER_CLOSE_TO_SPAWN_ORIGIN))
	{
		return;
	}

	//Calculate the distance from the origin.
	float fDistSq = rSpawnPoint.m_vPos.Dist2(m_vSearchOrigin);

	//Clamp the distance to values we care about.
	float fMinDistSq = square(m_fSearchMinDist);
	float fMaxDistSq = square(m_fSearchMaxDist);
	fDistSq = Clamp(fDistSq, fMinDistSq, fMaxDistSq);

	//Normalize the value from 0 ... 1.
	float fNormal = (fDistSq - fMinDistSq) / (fMaxDistSq - fMinDistSq);

	//Flip the value, since values closer to the minimum are better and should therefore be scored higher.
	fNormal = 1.0f - fNormal;

	//Calculate the score.
	fNormal = Clamp(fNormal, 0.0f, 1.0f);
	float fScore = Lerp(fNormal, ms_fMinScoreProximityToOrigin, ms_fMaxScoreProximityToOrigin);
	
#if __BANK
	rSpawnPoint.m_Score.m_fProximityToOrigin = fScore;
#endif

	//Add the score.
	rSpawnPoint.m_Score.m_fTotal += fScore;
}

void CNetRespawnMgr::EvaluateRandomness(TSpawnPoint& rSpawnPoint)
{
	//Ensure we care about randomness.
	if(!m_uFlags.IsFlagSet(FLAG_PREFER_RANDOMNESS))
	{
		return;
	}

	//Choose a random value.
	float fNormal = fwRandom::GetRandomNumberInRange(0.0f, 1.0f);

	//Calculate the score.
	fNormal = Clamp(fNormal, 0.0f, 1.0f);
	float fScore = Lerp(fNormal, ms_fMinScoreRandomness, ms_fMaxScoreRandomness);
	
#if __BANK
	rSpawnPoint.m_Score.m_fRandomness = fScore;
#endif

	//Add the score.
	rSpawnPoint.m_Score.m_fTotal += fScore;
}

void CNetRespawnMgr::EvaluateTeamBunching(TSpawnPoint& rSpawnPoint)
{
	//Ensure we care about team bunching.
	if(!m_uFlags.IsFlagSet(FLAG_PREFER_TEAM_BUNCHING))
	{
		return;
	}

	//Ensure the largest bunch of enemy player position is valid.
	if(!m_vLargestBunchOfEnemyPlayersPosition.IsNonZero())
	{
		return;
	}

	//Ensure the largest bunch of friendly player position is valid.
	if(!m_vLargestBunchOfFriendlyPlayersPosition.IsNonZero())
	{
		return;
	}

	//Calculate the direction from the friendly players to the position.
	Vector3 vFriendlyPlayersToPosition = rSpawnPoint.m_vPos - m_vLargestBunchOfFriendlyPlayersPosition;
	Vector3 vFriendlyPlayersToPositionDirection = vFriendlyPlayersToPosition;
	vFriendlyPlayersToPositionDirection.NormalizeSafe(VEC3_ZERO);

	//Calculate the direction from the enemy players to the friendly players.
	Vector3 vEnemyPlayersToFriendlyPlayers = m_vLargestBunchOfFriendlyPlayersPosition - m_vLargestBunchOfEnemyPlayersPosition;
	Vector3 vEnemyPlayersToFriendlyPlayersDirection = vEnemyPlayersToFriendlyPlayers;
	vEnemyPlayersToFriendlyPlayersDirection.NormalizeSafe(VEC3_ZERO);

	//Calculate the normal for the direction.
	float fNormalDirection = vFriendlyPlayersToPositionDirection.Dot(vEnemyPlayersToFriendlyPlayersDirection);
	fNormalDirection += 1.0f;
	fNormalDirection *= 0.5f;

	//Calculate the score for the direction.
	float fScoreDirection = Lerp(fNormalDirection, ms_fMinScoreTeamBunchingDirection, ms_fMaxScoreTeamBunchingDirection);

	//Calculate the distance from the friendlies.
	float fDistSq = vFriendlyPlayersToPosition.Mag2();

	//Clamp the distance to values we care about.
	float fIdealDistSq = square(ms_fIdealDistanceForScoringFriendlyPlayersCloser);
	fDistSq = Clamp(fDistSq, 0.0f, fIdealDistSq);

	//Calculate the normal for the position.
	float fNormalPosition = fDistSq / fIdealDistSq;

	//Calculate the score for the position.
	float fScorePosition = Lerp(fNormalPosition, ms_fMinScoreTeamBunchingPosition, ms_fMaxScoreTeamBunchingPosition);

	//Calculate the score.
	float fScore = fScoreDirection + fScorePosition;
	
#if __BANK
	rSpawnPoint.m_Score.m_fTeamBunching = fScore;
#endif

	//Add the score.
	rSpawnPoint.m_Score.m_fTotal += fScore;
}

void CNetRespawnMgr::FindClosestEnemyAndFriendlyAi(const Vector3& vPos, float& fClosestEnemyDistSq, float& fClosestFriendlyDistSq)
{
	//Initialize the distances.
	fClosestEnemyDistSq		= FLT_MAX;
	fClosestFriendlyDistSq	= FLT_MAX;

	//Find the enemy and friendly AI positions.
	Positions enemyAiPositions;
	Positions friendlyAiPositions;
	FindEnemyAndFriendlyAiPositions(enemyAiPositions, friendlyAiPositions);

	//Find the closest enemy and friendly positions.
	FindClosestEnemyAndFriendlyPositions(vPos, enemyAiPositions, friendlyAiPositions, fClosestEnemyDistSq, fClosestFriendlyDistSq);
}

void CNetRespawnMgr::FindClosestEnemyAndFriendlyPlayers(const Vector3& vPos, float& fClosestEnemyDistSq, float& fClosestFriendlyDistSq)
{
	//Initialize the distances.
	fClosestEnemyDistSq		= FLT_MAX;
	fClosestFriendlyDistSq	= FLT_MAX;
	
	//Find the enemy and friendly player positions.
	Positions enemyPlayerPositions;
	Positions friendlyPlayerPositions;
	FindEnemyAndFriendlyPlayerPositions(enemyPlayerPositions, friendlyPlayerPositions);
	
	//Find the closest enemy and friendly positions.
	FindClosestEnemyAndFriendlyPositions(vPos, enemyPlayerPositions, friendlyPlayerPositions, fClosestEnemyDistSq, fClosestFriendlyDistSq);
}

void CNetRespawnMgr::FindClosestEnemyAndFriendlyPositions(const Vector3& vPos, const Positions& rEnemyPositions, const Positions& rFriendlyPositions, float& fClosestEnemyDistSq, float& fClosestFriendlyDistSq)
{	
	//Find the closest enemy.
	for(int i = 0; i < rEnemyPositions.GetCount(); ++i)
	{
		//Get the position.
		const Position& rPosition = rEnemyPositions[i];

		//Calculate the distance.
		float fDistSq = !rPosition.UseFlatDistance() ? (vPos - rPosition.m_vPosition).Mag2() : 
			(vPos - rPosition.m_vPosition).XYMag2();
		if(fDistSq >= fClosestEnemyDistSq)
		{
			continue;
		}
		
		//Assign the distance.
		fClosestEnemyDistSq = fDistSq;
	}
	
	//Find the closest friendly.
	for(int i = 0; i < rFriendlyPositions.GetCount(); ++i)
	{
		//Get the position.
		const Position& rPosition = rFriendlyPositions[i];

		//Calculate the distance.
		float fDistSq = !rPosition.UseFlatDistance() ? (vPos - rPosition.m_vPosition).Mag2() : 
			(vPos - rPosition.m_vPosition).XYMag2();
		if(fDistSq >= fClosestFriendlyDistSq)
		{
			continue;
		}

		//Assign the distance.
		fClosestFriendlyDistSq = fDistSq;
	}
}

void CNetRespawnMgr::FindEnemyAndFriendlyAiPositions(Positions& rEnemyAiPositions, Positions& rFriendlyAiPositions)
{
#if __BANK
	//Check if we are using AI overrides.
	if(ms_bUseAiOverrides)
	{
		//Find the enemy and friendly AI positions from the overrides.
		FindEnemyAndFriendlyAiPositionsFromOverrides(rEnemyAiPositions, rFriendlyAiPositions);
	}
	else
#endif
	{
		//Find the enemy and friendly AI positions from the world.
		FindEnemyAndFriendlyAiPositionsFromWorld(rEnemyAiPositions, rFriendlyAiPositions);
	}
}

void CNetRespawnMgr::FindEnemyAndFriendlyAiPositionsFromWorld(Positions& rEnemyAiPositions, Positions& rFriendlyAiPositions)
{
	//Check if the cache is valid.
	if(m_AiPositionsFromWorld.IsValid())
	{
		//Set the positions.
		rEnemyAiPositions		= m_AiPositionsFromWorld.m_EnemyPositions;
		rFriendlyAiPositions	= m_AiPositionsFromWorld.m_FriendlyPositions;

		//Nothing else to do.
		return;
	}

	//Ensure the ped is valid.
	if(!m_pPed)
	{
		return;
	}

	//Ensure we care about the AI.
	if(!m_uFlags.IsFlagSet(FLAG_PREFER_ENEMY_AI_FARTHER) && !m_uFlags.IsFlagSet(FLAG_PREFER_FRIENDLY_AI_CLOSER))
	{
		return;
	}

	//Calculate the range.
	float fRange = m_fSearchMaxDist;

	//Calculate the expansion.
	float fExpansion = 0.0f;
	fExpansion = Max(fExpansion, ms_fMaxDistanceForScoringEnemyPlayersFarther);
	fExpansion = Max(fExpansion, ms_fMaxDistanceForScoringFriendlyPlayersCloser);
	fExpansion = Max(fExpansion, ms_fMaxDistanceForScoringEnemyAiFarther);
	fExpansion = Max(fExpansion, ms_fMaxDistanceForScoringFriendlyAiCloser);

	//Expand the range.
	fRange += fExpansion;

	//Find all of the peds within range of the search origin.
	s32 iCount = 0;
	static const int s_iMaxResults = 32;
	CEntity* pResults[s_iMaxResults];
	CGameWorld::FindObjectsInRange(m_vSearchOrigin, fRange, true, &iCount, s_iMaxResults, pResults, false, false, true, false, false);

	//Iterate over the peds.
	for(int i = 0; i < iCount; ++i)
	{
		//Ensure the entity is valid.
		const CEntity* pEntity = pResults[i];
		if(!pEntity)
		{
			continue;
		}

		//Ensure the entity is a ped.
		if(!aiVerifyf(pEntity->GetIsTypePed(), "Entity is not a ped."))
		{
			continue;
		}

		//Grab the ped.
		const CPed* pPed = static_cast<const CPed *>(pEntity);

		//Ensure the ped is not a player.
		if(pPed->IsPlayer())
		{
			continue;
		}
		
		//Generate the friendly state.
		bool bFriendly;
		
		//Check if the ped is friendly.
		if(pPed->GetPedIntelligence()->IsFriendlyWith(*m_pPed))
		{
			bFriendly = true;
		}
		//Check if the ped is an enemy.
		else if(pPed->GetPedIntelligence()->IsThreatenedBy(*m_pPed))
		{
			bFriendly = false;
		}
		//The ped is neutral.
		else
		{
			continue;
		}
		
		//Choose the positions.
		Positions& rPositions = bFriendly ? rFriendlyAiPositions : rEnemyAiPositions;
		
		//Ensure the positions are not full.
		if(rPositions.IsFull())
		{
			continue;
		}
		
		//Add the position.
		AddPosition(rPositions, *pPed, false);
	}

	//Cache the data.
	m_AiPositionsFromWorld.m_EnemyPositions		= rEnemyAiPositions;
	m_AiPositionsFromWorld.m_FriendlyPositions	= rFriendlyAiPositions;
	m_AiPositionsFromWorld.m_uTimeLastUpdated	= fwTimer::GetTimeInMilliseconds();
}

void CNetRespawnMgr::FindEnemyAndFriendlyPlayerPositions(Positions& rEnemyPlayerPositions, Positions& rFriendlyPlayerPositions)
{
#if __BANK
	//Check if we are using player overrides.
	if(ms_bUsePlayerOverrides)
	{
		//Find the enemy and friendly player positions from the overrides.
		FindEnemyAndFriendlyPlayerPositionsFromOverrides(rEnemyPlayerPositions, rFriendlyPlayerPositions);
	}
	else
#endif
	{
		//Find the enemy and friendly player positions from the network.
		FindEnemyAndFriendlyPlayerPositionsFromNetwork(rEnemyPlayerPositions, rFriendlyPlayerPositions);
	}
}

void CNetRespawnMgr::FindEnemyAndFriendlyPlayerPositionsFromNetwork(Positions& rEnemyPlayerPositions, Positions& rFriendlyPlayerPositions)
{
	//Check if the cache is valid.
	if(m_PlayerPositionsFromNetwork.IsValid())
	{
		//Set the positions.
		rEnemyPlayerPositions		= m_PlayerPositionsFromNetwork.m_EnemyPositions;
		rFriendlyPlayerPositions	= m_PlayerPositionsFromNetwork.m_FriendlyPositions;

		//Nothing else to do.
		return;
	}

	//The team must be valid for anyone to be considered friendly.
	//This will happen if no team or ped is specified.
	//This can also happen in games like deathmatch, where no one is assigned a team.
	bool bCanBeFriendly = (m_iTeam >= 0);
	
	//Get the net player for the ped.
	CNetGamePlayer* pNetPlayer = m_pPed ? GetNetPlayerForPed(*m_pPed) : NULL;
	
	//Iterate over the players.
	unsigned                 numPhysicalPlayers = netInterface::GetNumPhysicalPlayers();
    const netPlayer * const *allPhysicalPlayers = netInterface::GetAllPhysicalPlayers();

    for(unsigned index = 0; index < numPhysicalPlayers; index++)
    {
        const CNetGamePlayer *pOtherPlayer = SafeCast(const CNetGamePlayer, allPhysicalPlayers[index]);

		//Ensure the ped is valid.
		const CPed* pOtherPed = pOtherPlayer->GetPlayerPed();
		if(!pOtherPed)
		{
			continue;
		}

		//Ensure the other ped is not the ped.
		if(pOtherPed == m_pPed)
		{
			continue;
		}

		//Ensure the other ped is not injured.
		if(pOtherPed->IsInjured())
		{
			continue;
		}

		//Ensure the other ped is visible.
		if(!pOtherPed->GetIsVisible())
		{
			continue;
		}
		
		//Ensure the players are not in different sessions.
		if(pNetPlayer && NetworkInterface::ArePlayersInDifferentTutorialSessions(*pNetPlayer, *pOtherPlayer))
		{
			continue;
		}
		
		//Grab the team.
		int iOtherTeam = pOtherPlayer->GetTeam();

		//Check if the other ped is friendly.
		bool bFriendly = bCanBeFriendly && (m_iTeam == iOtherTeam);
		
		//Choose the positions.
		Positions& rPositions = bFriendly ? rFriendlyPlayerPositions : rEnemyPlayerPositions;
		
		//Ensure the positions are not full.
		if(rPositions.IsFull())
		{
			continue;
		}
		
		//Add the position.
		AddPosition(rPositions, *pOtherPed, true);
	}

	//Cache the data.
	m_PlayerPositionsFromNetwork.m_EnemyPositions		= rEnemyPlayerPositions;
	m_PlayerPositionsFromNetwork.m_FriendlyPositions	= rFriendlyPlayerPositions;
	m_PlayerPositionsFromNetwork.m_uTimeLastUpdated		= fwTimer::GetTimeInMilliseconds();
}

CNetGamePlayer* CNetRespawnMgr::GetNetPlayerForPed(const CPed& rPed)
{
	//Ensure the net object is valid.
	netObject* pNetObject = rPed.GetNetworkObject();
	if(!aiVerifyf(pNetObject, "The ped must have a valid net object to get the net player."))
	{
		return NULL;
	}
	
	//Ensure the network object type is a player or ped.
	NetworkObjectType nNetworkObjectType = pNetObject->GetObjectType();
	if(!aiVerifyf(nNetworkObjectType == NET_OBJ_TYPE_PED || nNetworkObjectType == NET_OBJ_TYPE_PLAYER, "The ped has an unexpected network object type: %d.", nNetworkObjectType))
	{
		return NULL;
	}
	
	//Grab the ped network object.
	CNetObjPed* pNetObjPed = static_cast<CNetObjPed *>(pNetObject);
	
	return pNetObjPed->GetPlayerOwner();
}

int CNetRespawnMgr::GetNetTeamForPed(const CPed& rPed)
{
	//Ensure the net player is valid.
	CNetGamePlayer* pNetPlayer = GetNetPlayerForPed(rPed);
	if(!aiVerifyf(pNetPlayer, "The ped must have a valid net player to get the team."))
	{
		return -1;
	}
	
	return pNetPlayer->GetTeam();
}

void CNetRespawnMgr::UpdateLargestBunchOfEnemyPlayers()
{
	//Clear the largest bunch of enemy players.
	m_vLargestBunchOfEnemyPlayersPosition.Zero();
	
	//Ensure we care about team bunching.
	if(!m_uFlags.IsFlagSet(FLAG_PREFER_TEAM_BUNCHING))
	{
		return;
	}
	
	//Find the best bunch of enemy players.
	FindLargestBunchOfPlayers(false, m_vLargestBunchOfEnemyPlayersPosition);
}

void CNetRespawnMgr::UpdateLargestBunchOfFriendlyPlayers()
{
	//Clear the largest bunch of friendly players.
	m_vLargestBunchOfFriendlyPlayersPosition.Zero();

	//Ensure we care about team bunching.
	if(!m_uFlags.IsFlagSet(FLAG_PREFER_TEAM_BUNCHING))
	{
		return;
	}

	//Find the best bunch of friendly players.
	FindLargestBunchOfPlayers(true, m_vLargestBunchOfFriendlyPlayersPosition);
}

void CNetRespawnMgr::UpdateLargestBunches()
{
	//Update the largest bunch of enemy players.
	UpdateLargestBunchOfEnemyPlayers();
	
	//Update the largest bunch of friendly players.
	UpdateLargestBunchOfFriendlyPlayers();
}

#if __BANK
void CNetRespawnMgr::AddSpawnRecord()
{
	ms_SpawnRecord.m_vOrigin = m_vSearchOrigin;
	ms_SpawnRecord.m_vTarget = m_vSearchTargetPos;
	ms_SpawnRecord.m_fMinDistance = m_fSearchMinDist;
	ms_SpawnRecord.m_fMaxDistance = m_fSearchMaxDist;
	ms_SpawnRecord.m_uFlags = m_uFlags;

	ms_SpawnRecord.m_pPed = m_pPed;
	ms_SpawnRecord.m_iTeam = m_iTeam;
	ms_SpawnRecord.m_BestSpawnPoint = m_BestSpawnPoints[0];

	ms_SpawnRecord.m_aEnemyPlayerPositions.Reset();	
	ms_SpawnRecord.m_aFriendlyPlayerPositions.Reset();
	FindEnemyAndFriendlyPlayerPositions(ms_SpawnRecord.m_aEnemyPlayerPositions, ms_SpawnRecord.m_aFriendlyPlayerPositions);
	
	ms_SpawnRecord.m_aEnemyAiPositions.Reset();	
	ms_SpawnRecord.m_aFriendlyAiPositions.Reset();
	FindEnemyAndFriendlyAiPositions(ms_SpawnRecord.m_aEnemyAiPositions, ms_SpawnRecord.m_aFriendlyAiPositions);
}

void CNetRespawnMgr::CopyEnemyAndFriendlyPositionsFromOverrides(const OverridePositions& rEnemyOverridePositions, const OverridePositions& rFriendlyOverridePositions, Positions& rEnemyPositions, Positions& rFriendlyPositions)
{
	//Iterate over the enemy override positions.
	for(int i = 0; i < rEnemyOverridePositions.GetCount(); ++i)
	{
		//Ensure the enemy positions are not full.
		if(rEnemyPositions.IsFull())
		{
			break;
		}

		//Add the position.
		rEnemyPositions.Append() = rEnemyOverridePositions[i];
	}

	//Iterate over the friendly override positions.
	for(int i = 0; i < rFriendlyOverridePositions.GetCount(); ++i)
	{
		//Ensure the friendly positions are not full.
		if(rFriendlyPositions.IsFull())
		{
			break;
		}

		//Add the position.
		rFriendlyPositions.Append() = rFriendlyOverridePositions[i];
	}
}

void CNetRespawnMgr::FindEnemyAndFriendlyAiPositionsFromOverrides(Positions& rEnemyAiPositions, Positions& rFriendlyAiPositions)
{
	//Copy the enemy and friendly AI positions from the overrides.
	CopyEnemyAndFriendlyPositionsFromOverrides(ms_aEnemyAiPositionsOverride, ms_aFriendlyAiPositionsOverride, rEnemyAiPositions, rFriendlyAiPositions);
}

void CNetRespawnMgr::FindEnemyAndFriendlyPlayerPositionsFromOverrides(Positions& rEnemyPlayerPositions, Positions& rFriendlyPlayerPositions)
{
	//Copy the enemy and friendly player positions from the overrides.
	CopyEnemyAndFriendlyPositionsFromOverrides(ms_aEnemyPlayerPositionsOverride, ms_aFriendlyPlayerPositionsOverride, rEnemyPlayerPositions, rFriendlyPlayerPositions);
}
#endif

#if __BANK

void DumpLastSpawnRecord()
{
	//Ensure the search is not active.
	if(CNetRespawnMgr::IsSearchActive())
	{
		Displayf("Search is active.");
		return;
	}
	
	//Ensure the search was successful.
	if(!CNetRespawnMgr::WasSearchSuccessful())
	{
		Displayf("Search was unsuccessful.");
		return;
	}
	
	Displayf("Origin: %.2f, %.2f, %.2f", CNetRespawnMgr::ms_SpawnRecord.m_vOrigin.x, CNetRespawnMgr::ms_SpawnRecord.m_vOrigin.y, CNetRespawnMgr::ms_SpawnRecord.m_vOrigin.z);
	Displayf("Target: %.2f, %.2f, %.2f", CNetRespawnMgr::ms_SpawnRecord.m_vTarget.x, CNetRespawnMgr::ms_SpawnRecord.m_vTarget.y, CNetRespawnMgr::ms_SpawnRecord.m_vTarget.z);
	Displayf("Min Distance: %.2f", CNetRespawnMgr::ms_SpawnRecord.m_fMinDistance);
	Displayf("Max Distance: %.2f", CNetRespawnMgr::ms_SpawnRecord.m_fMaxDistance);
	Displayf("Flags:");
	if(CNetRespawnMgr::ms_SpawnRecord.m_uFlags & CNetRespawnMgr::FLAG_SCRIPT_COMMAND)					{ Displayf("   Script Command"); }
	if(CNetRespawnMgr::ms_SpawnRecord.m_uFlags & CNetRespawnMgr::FLAG_IGNORE_TARGET_HEADING)			{ Displayf("   Ignore Target Heading"); }
	if(CNetRespawnMgr::ms_SpawnRecord.m_uFlags & CNetRespawnMgr::FLAG_PREFER_CLOSE_TO_SPAWN_ORIGIN)		{ Displayf("   Prefer Close To Spawn Origin"); }
	if(CNetRespawnMgr::ms_SpawnRecord.m_uFlags & CNetRespawnMgr::FLAG_MAY_SPAWN_IN_INTERIOR)			{ Displayf("   May Spawn In Interior"); }
	if(CNetRespawnMgr::ms_SpawnRecord.m_uFlags & CNetRespawnMgr::FLAG_MAY_SPAWN_IN_EXTERIOR)			{ Displayf("   May Spawn In Exterior"); }
	if(CNetRespawnMgr::ms_SpawnRecord.m_uFlags & CNetRespawnMgr::FLAG_PREFER_WIDE_FOV)					{ Displayf("   Prefer Wide FOV"); }
	if(CNetRespawnMgr::ms_SpawnRecord.m_uFlags & CNetRespawnMgr::FLAG_PREFER_TEAM_BUNCHING)				{ Displayf("   Prefer Team Bunching"); }
	if(CNetRespawnMgr::ms_SpawnRecord.m_uFlags & CNetRespawnMgr::FLAG_PREFER_ENEMY_PLAYERS_FARTHER)		{ Displayf("   Prefer Enemy Players Farther"); }
	if(CNetRespawnMgr::ms_SpawnRecord.m_uFlags & CNetRespawnMgr::FLAG_PREFER_FRIENDLY_PLAYERS_CLOSER)	{ Displayf("   Prefer Friendly Players Closer"); }
	if(CNetRespawnMgr::ms_SpawnRecord.m_uFlags & CNetRespawnMgr::FLAG_PREFER_ENEMY_AI_FARTHER)			{ Displayf("   Prefer Enemy AI Farther"); }
	if(CNetRespawnMgr::ms_SpawnRecord.m_uFlags & CNetRespawnMgr::FLAG_PREFER_FRIENDLY_AI_CLOSER)		{ Displayf("   Prefer Friendly AI Closer"); }
	if(CNetRespawnMgr::ms_SpawnRecord.m_uFlags & CNetRespawnMgr::FLAG_PREFER_RANDOMNESS)				{ Displayf("   Prefer Randomness"); }
	
	Displayf("Ped: %s", CNetRespawnMgr::ms_SpawnRecord.m_pPed ? CNetRespawnMgr::ms_SpawnRecord.m_pPed->GetModelName() : "None");
	Displayf("Team: %d", CNetRespawnMgr::ms_SpawnRecord.m_iTeam);
	
	Displayf("Best Spawn Point:");
	Displayf("   Position: %.2f, %.2f, %.2f", CNetRespawnMgr::ms_SpawnRecord.m_BestSpawnPoint.m_vPos.x, CNetRespawnMgr::ms_SpawnRecord.m_BestSpawnPoint.m_vPos.y, CNetRespawnMgr::ms_SpawnRecord.m_BestSpawnPoint.m_vPos.z);
	Displayf("   Heading: %.2f", CNetRespawnMgr::ms_SpawnRecord.m_BestSpawnPoint.m_fHeading);
	Displayf("   Score:");
	Displayf("      Total: %.2f", CNetRespawnMgr::ms_SpawnRecord.m_BestSpawnPoint.m_Score.m_fTotal);
#if __BANK
	Displayf("         FOV: %.2f", CNetRespawnMgr::ms_SpawnRecord.m_BestSpawnPoint.m_Score.m_fFOV);
	Displayf("         Proximity To Origin: %.2f", CNetRespawnMgr::ms_SpawnRecord.m_BestSpawnPoint.m_Score.m_fProximityToOrigin);
	Displayf("         Team Bunching: %.2f", CNetRespawnMgr::ms_SpawnRecord.m_BestSpawnPoint.m_Score.m_fTeamBunching);
	Displayf("         Enemy Players Farther: %.2f", CNetRespawnMgr::ms_SpawnRecord.m_BestSpawnPoint.m_Score.m_fEnemyPlayersFarther);
	Displayf("         Friendly Players Closer: %.2f", CNetRespawnMgr::ms_SpawnRecord.m_BestSpawnPoint.m_Score.m_fFriendlyPlayersCloser);
	Displayf("         Enemy AI Farther: %.2f", CNetRespawnMgr::ms_SpawnRecord.m_BestSpawnPoint.m_Score.m_fEnemyAiFarther);
	Displayf("         Friendly AI Closer: %.2f", CNetRespawnMgr::ms_SpawnRecord.m_BestSpawnPoint.m_Score.m_fFriendlyAiCloser);
	Displayf("         Randomness: %.2f", CNetRespawnMgr::ms_SpawnRecord.m_BestSpawnPoint.m_Score.m_fRandomness);
#endif

	Displayf("Enemy Players:");
	for(int i = 0; i < CNetRespawnMgr::ms_SpawnRecord.m_aEnemyPlayerPositions.GetCount(); ++i)
	{
		Displayf("   Position: %.2f, %.2f, %.2f", CNetRespawnMgr::ms_SpawnRecord.m_aEnemyPlayerPositions[i].m_vPosition.x, CNetRespawnMgr::ms_SpawnRecord.m_aEnemyPlayerPositions[i].m_vPosition.y, CNetRespawnMgr::ms_SpawnRecord.m_aEnemyPlayerPositions[i].m_vPosition.z);
	}
	
	Displayf("Friendly Players:");
	for(int i = 0; i < CNetRespawnMgr::ms_SpawnRecord.m_aFriendlyPlayerPositions.GetCount(); ++i)
	{
		Displayf("   Position: %.2f, %.2f, %.2f", CNetRespawnMgr::ms_SpawnRecord.m_aFriendlyPlayerPositions[i].m_vPosition.x, CNetRespawnMgr::ms_SpawnRecord.m_aFriendlyPlayerPositions[i].m_vPosition.y, CNetRespawnMgr::ms_SpawnRecord.m_aFriendlyPlayerPositions[i].m_vPosition.z);
	}
	
	Displayf("Enemy AI:");
	for(int i = 0; i < CNetRespawnMgr::ms_SpawnRecord.m_aEnemyAiPositions.GetCount(); ++i)
	{
		Displayf("   Position: %.2f, %.2f, %.2f", CNetRespawnMgr::ms_SpawnRecord.m_aEnemyAiPositions[i].m_vPosition.x, CNetRespawnMgr::ms_SpawnRecord.m_aEnemyAiPositions[i].m_vPosition.y, CNetRespawnMgr::ms_SpawnRecord.m_aEnemyAiPositions[i].m_vPosition.z);
	}

	Displayf("Friendly AI:");
	for(int i = 0; i < CNetRespawnMgr::ms_SpawnRecord.m_aFriendlyAiPositions.GetCount(); ++i)
	{
		Displayf("   Position: %.2f, %.2f, %.2f", CNetRespawnMgr::ms_SpawnRecord.m_aFriendlyAiPositions[i].m_vPosition.x, CNetRespawnMgr::ms_SpawnRecord.m_aFriendlyAiPositions[i].m_vPosition.y, CNetRespawnMgr::ms_SpawnRecord.m_aFriendlyAiPositions[i].m_vPosition.z);
	}
}

void PlaceEnemyAiPositionAtCameraPosition()
{
	//Ensure the enemy AI positions are not full.
	if(CNetRespawnMgr::ms_aEnemyAiPositionsOverride.IsFull())
	{
		//Remove the first element.
		CNetRespawnMgr::ms_aEnemyAiPositionsOverride.Pop();
	}

	//Add an enemy AI position.
	CNetRespawnMgr::Position& rPosition = CNetRespawnMgr::ms_aEnemyAiPositionsOverride.Append();
	rPosition.Reset();
	rPosition.m_vPosition = camInterface::GetPos();
}

void PlaceEnemyPlayerPositionAtCameraPosition()
{
	//Ensure the enemy player positions are not full.
	if(CNetRespawnMgr::ms_aEnemyPlayerPositionsOverride.IsFull())
	{
		//Remove the first element.
		CNetRespawnMgr::ms_aEnemyPlayerPositionsOverride.Pop();
	}
	
	//Add an enemy player position.
	CNetRespawnMgr::Position& rPosition = CNetRespawnMgr::ms_aEnemyPlayerPositionsOverride.Append();
	rPosition.Reset();
	rPosition.m_vPosition = camInterface::GetPos();
}

void PlaceFriendlyAiPositionAtCameraPosition()
{
	//Ensure the friendly AI positions are not full.
	if(CNetRespawnMgr::ms_aFriendlyAiPositionsOverride.IsFull())
	{
		//Remove the first element.
		CNetRespawnMgr::ms_aFriendlyAiPositionsOverride.Pop();
	}

	//Add a friendly AI position.
	CNetRespawnMgr::Position& rPosition = CNetRespawnMgr::ms_aFriendlyAiPositionsOverride.Append();
	rPosition.Reset();
	rPosition.m_vPosition = camInterface::GetPos();
}

void PlaceFriendlyPlayerPositionAtCameraPosition()
{
	//Ensure the friendly player positions are not full.
	if(CNetRespawnMgr::ms_aFriendlyPlayerPositionsOverride.IsFull())
	{
		//Remove the first element.
		CNetRespawnMgr::ms_aFriendlyPlayerPositionsOverride.Pop();
	}

	//Add a friendly player position.
	CNetRespawnMgr::Position& rPosition = CNetRespawnMgr::ms_aFriendlyPlayerPositionsOverride.Append();
	rPosition.Reset();
	rPosition.m_vPosition = camInterface::GetPos();
}

void ResetEnemyAiPositions()
{
	CNetRespawnMgr::ms_aEnemyAiPositionsOverride.Reset();
}

void ResetEnemyPlayerPositions()
{
	CNetRespawnMgr::ms_aEnemyPlayerPositionsOverride.Reset();
}

void ResetFriendlyAiPositions()
{
	CNetRespawnMgr::ms_aFriendlyAiPositionsOverride.Reset();
}

void ResetFriendlyPlayerPositions()
{
	CNetRespawnMgr::ms_aFriendlyPlayerPositionsOverride.Reset();
}

void TestPedPositionProjection()
{
	//Calculate the projected position.
	float fTime = CNetRespawnMgr::ms_fTimeForPedPositionProjectionTesting;
	CNetRespawnMgr::ms_vResultForPedPositionProjectionTesting = VEC3V_TO_VECTOR3(CNetRespawnMgr::CalculateProjectedPositionForPlayer(fTime));
	
	//Draw the result.
	CNetRespawnMgr::ms_bDrawResultForPedPositionProjectionTesting = true;
}

void TestRespawn()
{
	//Create the input parameters.
	CNetRespawnMgr::StartSearchInput input(VEC3_ZERO);
	
	//Assign the ped.
	if(CNetRespawnMgr::ms_bAdvancedUsePlayerAsPed)
	{
		input.m_pPed = CGameWorld::FindLocalPlayer();
	}
	
	//Calculate the origin.
	if(CNetRespawnMgr::ms_bAdvancedUsePlayerAsOrigin)
	{
		input.m_vOrigin = CPlayerInfo::ms_cachedMainPlayerPos;
	}
	else if(CNetRespawnMgr::ms_bAdvancedUseCameraAsOrigin)
	{
		input.m_vOrigin = camInterface::GetPos();
	}
	else
	{
		input.m_vOrigin = VEC3_ZERO;
	}
	
	//Calculate the target.
	if(CNetRespawnMgr::ms_bAdvancedUsePlayerAsTarget)
	{
		input.m_vTargetPos = CPlayerInfo::ms_cachedMainPlayerPos;
	}
	else if(CNetRespawnMgr::ms_bAdvancedUseCameraAsTarget)
	{
		input.m_vTargetPos = camInterface::GetPos();
	}
	else
	{
		input.m_vTargetPos = CNetRespawnMgr::ms_vSearchTargetPosNone;
	}
	
	//Calculate the min search distance.
	input.m_fMinDist = CNetRespawnMgr::ms_fAdvancedMinSearchDistance;
	
	//Calculate the max search distance.
	input.m_fMaxDist = CNetRespawnMgr::ms_fAdvancedMaxSearchDistance;
	
	//Calculate the flags.
	if(CNetRespawnMgr::ms_bAdvancedFlagScriptCommand)
	{
		input.m_uFlags.SetFlag(CNetRespawnMgr::FLAG_SCRIPT_COMMAND);
	}
	if(CNetRespawnMgr::ms_bAdvancedFlagIgnoreTargetHeading)
	{
		input.m_uFlags.SetFlag(CNetRespawnMgr::FLAG_IGNORE_TARGET_HEADING);
	}
	if(CNetRespawnMgr::ms_bAdvancedFlagPreferCloseToSpawnOrigin)
	{
		input.m_uFlags.SetFlag(CNetRespawnMgr::FLAG_PREFER_CLOSE_TO_SPAWN_ORIGIN);
	}
	if(CNetRespawnMgr::ms_bAdvancedFlagMaySpawnInInterior)
	{
		input.m_uFlags.SetFlag(CNetRespawnMgr::FLAG_MAY_SPAWN_IN_INTERIOR);
	}
	if(CNetRespawnMgr::ms_bAdvancedFlagMaySpawnInExterior)
	{
		input.m_uFlags.SetFlag(CNetRespawnMgr::FLAG_MAY_SPAWN_IN_EXTERIOR);
	}
	if(CNetRespawnMgr::ms_bAdvancedFlagPreferWideFOV)
	{
		input.m_uFlags.SetFlag(CNetRespawnMgr::FLAG_PREFER_WIDE_FOV);
	}
	if(CNetRespawnMgr::ms_bAdvancedFlagPreferTeamBunching)
	{
		input.m_uFlags.SetFlag(CNetRespawnMgr::FLAG_PREFER_TEAM_BUNCHING);
	}
	if(CNetRespawnMgr::ms_bAdvancedFlagPreferEnemyPlayersFarther)
	{
		input.m_uFlags.SetFlag(CNetRespawnMgr::FLAG_PREFER_ENEMY_PLAYERS_FARTHER);
	}
	if(CNetRespawnMgr::ms_bAdvancedFlagPreferFriendlyPlayersCloser)
	{
		input.m_uFlags.SetFlag(CNetRespawnMgr::FLAG_PREFER_FRIENDLY_PLAYERS_CLOSER);
	}
	if(CNetRespawnMgr::ms_bAdvancedFlagPreferEnemyAiFarther)
	{
		input.m_uFlags.SetFlag(CNetRespawnMgr::FLAG_PREFER_ENEMY_AI_FARTHER);
	}
	if(CNetRespawnMgr::ms_bAdvancedFlagPreferFriendlyAiCloser)
	{
		input.m_uFlags.SetFlag(CNetRespawnMgr::FLAG_PREFER_FRIENDLY_AI_CLOSER);
	}
	if(CNetRespawnMgr::ms_bAdvancedFlagPreferRandomness)
	{
		input.m_uFlags.SetFlag(CNetRespawnMgr::FLAG_PREFER_RANDOMNESS);
	}

	

	// Toggle use of angled area volume?
	if(CNetRespawnMgr::ms_bDebugUseAngledArea)
	{
		input.m_iSearchVolumeType = CNetRespawnMgr::SEARCH_VOLUME_ANGLED_AREA;

		input.m_vAngledAreaPt1 = CPhysics::g_vClickedPos[0];
		input.m_vAngledAreaPt2 = CPhysics::g_vClickedPos[1];
		input.m_fAngledAreaWidth = CNetRespawnMgr::ms_fAdvancedMinSearchDistance;
	}
	// Otherwise its a spherical volume
	else
	{
		input.m_iSearchVolumeType = CNetRespawnMgr::SEARCH_VOLUME_SPHERE;
	}

	//Start the search.
	CNetRespawnMgr::StartSearch(input);
}

void StopTestingRespawn()
{
	CNetRespawnMgr::CancelSearch();
}

Vec3V_Out CNetRespawnMgr::CalculateProjectedPositionForPlayer(float fTime)
{
	//Ensure the player is valid.
	const CPed* pPed = CGameWorld::FindLocalPlayer();
	if(!pPed)
	{
		return Vec3V(V_ZERO);
	}
	
	//Calculate the projected position.
	float fDistanceFromStart = 0.0f;
	return CalculateProjectedPosition(*pPed, fTime, fDistanceFromStart);
}

void CNetRespawnMgr::InitWidgets()
{
	bkBank * bank = BANKMGR.FindBank("Game Logic");
	Assertf(bank, "Couldn't find \'Game Logic\' bank; this needs to be initialised before CNetRespawnMgr");

	bank->PushGroup("Net Respawn Mgr");
	
		bank->PushGroup("Options");
		
			bank->AddToggle("Use Player As Ped", &ms_bAdvancedUsePlayerAsPed);
			bank->AddToggle("Use Player As Origin", &ms_bAdvancedUsePlayerAsOrigin);
			bank->AddToggle("Use Camera As Origin", &ms_bAdvancedUseCameraAsOrigin);
			bank->AddToggle("Use Player As Target", &ms_bAdvancedUsePlayerAsTarget);
			bank->AddToggle("Use Camera As Target", &ms_bAdvancedUseCameraAsTarget);
			bank->AddSlider("Min Search Distance", &ms_fAdvancedMinSearchDistance, 0.0f, 100.0f, 1.0f);
			bank->AddSlider("Max Search Distance", &ms_fAdvancedMaxSearchDistance, 0.0f, 100.0f, 1.0f);
			bank->AddToggle("Use Angled Area (defined by measuring tool and min seach dist)", &ms_bDebugUseAngledArea);
			
		bank->PopGroup();
		
		bank->PushGroup("Flags");
		
			bank->AddToggle("Script Command", &ms_bAdvancedFlagScriptCommand);
			bank->AddToggle("Ignore Target Heading", &ms_bAdvancedFlagIgnoreTargetHeading);
			bank->AddToggle("Prefer Close To Spawn Origin", &ms_bAdvancedFlagPreferCloseToSpawnOrigin);
			bank->AddToggle("May Spawn In Interior", &ms_bAdvancedFlagMaySpawnInInterior);
			bank->AddToggle("May Spawn In Exterior", &ms_bAdvancedFlagMaySpawnInExterior);
			bank->AddToggle("Prefer Wide FOV", &ms_bAdvancedFlagPreferWideFOV);
			bank->AddToggle("Prefer Team Bunching", &ms_bAdvancedFlagPreferTeamBunching);
			bank->AddToggle("Prefer Enemy Players Farther", &ms_bAdvancedFlagPreferEnemyPlayersFarther);
			bank->AddToggle("Prefer Friendly Players Closer", &ms_bAdvancedFlagPreferFriendlyPlayersCloser);
			bank->AddToggle("Prefer Enemy AI Farther", &ms_bAdvancedFlagPreferEnemyAiFarther);
			bank->AddToggle("Prefer Friendly AI Closer", &ms_bAdvancedFlagPreferFriendlyAiCloser);
			bank->AddToggle("Prefer Randomness", &ms_bAdvancedFlagPreferRandomness);
			
		bank->PopGroup();
		
		bank->PushGroup("Scoring Tuning");
		
			bank->AddSlider("Max Distance For Bunching Players", &ms_fMaxDistanceForBunchingPlayers, 0.0f, 300.0f, 1.0f);
			bank->AddSlider("Min Distance For Enemy Players Farther", &ms_fMinDistanceForScoringEnemyPlayersFarther, 0.0f, 300.0f, 1.0f);
			bank->AddSlider("Max Distance For Enemy Players Farther", &ms_fMaxDistanceForScoringEnemyPlayersFarther, 0.0f, 300.0f, 1.0f);
			bank->AddSlider("Min Distance For Friendly Players Closer", &ms_fMinDistanceForScoringFriendlyPlayersCloser, 0.0f, 300.0f, 1.0f);
			bank->AddSlider("Ideal Distance For Friendly Players Closer", &ms_fIdealDistanceForScoringFriendlyPlayersCloser, 0.0f, 300.0f, 1.0f);
			bank->AddSlider("Max Distance For Friendly Players Closer", &ms_fMaxDistanceForScoringFriendlyPlayersCloser, 0.0f, 300.0f, 1.0f);
			bank->AddSlider("Min Distance For Enemy AI Farther", &ms_fMinDistanceForScoringEnemyAiFarther, 0.0f, 300.0f, 1.0f);
			bank->AddSlider("Max Distance For Enemy AI Farther", &ms_fMaxDistanceForScoringEnemyAiFarther, 0.0f, 300.0f, 1.0f);
			bank->AddSlider("Min Distance For Friendly AI Closer", &ms_fMinDistanceForScoringFriendlyAiCloser, 0.0f, 300.0f, 1.0f);
			bank->AddSlider("Ideal Distance For Friendly AI Closer", &ms_fIdealDistanceForScoringFriendlyAiCloser, 0.0f, 300.0f, 1.0f);
			bank->AddSlider("Max Distance For Friendly AI Closer", &ms_fMaxDistanceForScoringFriendlyAiCloser, 0.0f, 300.0f, 1.0f);
		
		bank->PopGroup();
		
		bank->PushGroup("Scoring Weights");
		
			bank->AddSlider("Min Score FOV", &ms_fMinScoreFOV, -10.0f, 10.0f, 0.1f);
			bank->AddSlider("Max Score FOV", &ms_fMaxScoreFOV, -10.0f, 10.0f, 0.1f);
			bank->AddSlider("Min Score Proximity To Origin", &ms_fMinScoreProximityToOrigin, -10.0f, 10.0f, 0.1f);
			bank->AddSlider("Max Score Proximity To Origin", &ms_fMaxScoreProximityToOrigin, -10.0f, 10.0f, 0.1f);
			bank->AddSlider("Min Score Team Bunching Direction", &ms_fMinScoreTeamBunchingDirection, -10.0f, 10.0f, 0.1f);
			bank->AddSlider("Max Score Team Bunching Direction", &ms_fMaxScoreTeamBunchingDirection, -10.0f, 10.0f, 0.1f);
			bank->AddSlider("Min Score Team Bunching Position", &ms_fMinScoreTeamBunchingPosition, -10.0f, 10.0f, 0.1f);
			bank->AddSlider("Max Score Team Bunching Position", &ms_fMaxScoreTeamBunchingPosition, -10.0f, 10.0f, 0.1f);
			bank->AddSlider("Min Score Enemy Players Farther", &ms_fMinScoreEnemyPlayersFarther, -10.0f, 10.0f, 0.1f);
			bank->AddSlider("Max Score Enemy Players Farther", &ms_fMaxScoreEnemyPlayersFarther, -10.0f, 10.0f, 0.1f);
			bank->AddSlider("Min Score Friendly Players Closer", &ms_fMinScoreFriendlyPlayersCloser, -10.0f, 10.0f, 0.1f);
			bank->AddSlider("Max Score Friendly Players Closer", &ms_fMaxScoreFriendlyPlayersCloser, -10.0f, 10.0f, 0.1f);
			bank->AddSlider("Min Score Friendly Players Too Close", &ms_fMinScoreFriendlyPlayersTooClose, -10.0f, 10.0f, 0.1f);
			bank->AddSlider("Max Score Friendly Players Too Close", &ms_fMaxScoreFriendlyPlayersTooClose, -10.0f, 10.0f, 0.1f);
			bank->AddSlider("Min Score Enemy AI Farther", &ms_fMinScoreEnemyAiFarther, -10.0f, 10.0f, 0.1f);
			bank->AddSlider("Max Score Enemy AI Farther", &ms_fMaxScoreEnemyAiFarther, -10.0f, 10.0f, 0.1f);
			bank->AddSlider("Min Score Friendly AI Closer", &ms_fMinScoreFriendlyAiCloser, -10.0f, 10.0f, 0.1f);
			bank->AddSlider("Max Score Friendly AI Closer", &ms_fMaxScoreFriendlyAiCloser, -10.0f, 10.0f, 0.1f);
			bank->AddSlider("Min Score Friendly AI Too Close", &ms_fMinScoreFriendlyAiTooClose, -10.0f, 10.0f, 0.1f);
			bank->AddSlider("Max Score Friendly AI Too Close", &ms_fMaxScoreFriendlyAiTooClose, -10.0f, 10.0f, 0.1f);
			bank->AddSlider("Min Score Randomness", &ms_fMinScoreRandomness, -10.0f, 10.0f, 0.1f);
			bank->AddSlider("Max Score Randomness", &ms_fMaxScoreRandomness, -10.0f, 10.0f, 0.1f);
		
		bank->PopGroup();
		
		bank->PushGroup("Player Overrides");

			bank->AddToggle("Use Player Overrides", &ms_bUsePlayerOverrides);
			bank->AddButton("Place Enemy Player Position At Camera Position", PlaceEnemyPlayerPositionAtCameraPosition);
			bank->AddButton("Reset Enemy Player Positions", ResetEnemyPlayerPositions);
			bank->AddButton("Place Friendly Player Position At Camera Position", PlaceFriendlyPlayerPositionAtCameraPosition);
			bank->AddButton("Reset Friendly Player Positions", ResetFriendlyPlayerPositions);

		bank->PopGroup();
		
		bank->PushGroup("AI Overrides");

			bank->AddToggle("Use AI Overrides", &ms_bUseAiOverrides);
			bank->AddButton("Place Enemy AI Position At Camera Position", PlaceEnemyAiPositionAtCameraPosition);
			bank->AddButton("Reset Enemy AI Positions", ResetEnemyAiPositions);
			bank->AddButton("Place Friendly AI Position At Camera Position", PlaceFriendlyAiPositionAtCameraPosition);
			bank->AddButton("Reset Friendly AI Positions", ResetFriendlyAiPositions);

		bank->PopGroup();

		bank->PushGroup("Ped Position Projection Testing");

		bank->AddSlider("Time", &ms_fTimeForPedPositionProjectionTesting, 0.0f, 10.0f, 0.1f);
		bank->AddToggle("Draw Result", &ms_bDrawResultForPedPositionProjectionTesting);
		bank->AddButton("Test Ped Position Projection", TestPedPositionProjection);

		bank->PopGroup();
		
		bank->AddButton("Test Respawn", TestRespawn);
		bank->AddButton("Stop Test", StopTestingRespawn);
		bank->AddButton("Dump Last Spawn Record", DumpLastSpawnRecord);
		bank->AddToggle("Debug Search", &m_bDebugSearch);
		bank->AddToggle("Debug Scores", &m_bDebugScores);

	bank->PopGroup();
}
#endif



