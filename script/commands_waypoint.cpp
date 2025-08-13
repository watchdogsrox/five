//
// filename:	commands_waypoint.cpp
// description:
//

// C headers
// Rage headers
#include "fwcontrol\WaypointRecordingRoute.h"
#include "script\wrapper.h"
// Game headers
#include "peds\AssistedMovement.h"
#include "peds\ped.h"
#include "peds\pedintelligence.h"
#include "peds/Ped.h"
#include "script/script_cars_and_peds.h"
#include "script\script_helper.h"
#include "vehicleAi/task/TaskVehicleFollowRecording.h"
#include "task\Movement\TaskFollowWaypointRecording.h"
#include "task/Vehicle/TaskCar.h"
#include "task/Default/TaskPlayer.h"

AI_OPTIMISATIONS()
//OPTIMISATIONS_OFF()
namespace waypoint_commands
{

	//********************************************
	// Waypoint recording/playback commands

	void CommandRequestWaypointRecording(const char * pRecordingName)
	{
		Assertf(CWaypointRecording::CheckWaypointRecordingName(pRecordingName), "%s : REQUEST_WAYPOINT_RECORDING - Waypoint recording name \'%s\' is not a valid name (must not include path)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pRecordingName);
		if(!CWaypointRecording::CheckWaypointRecordingName(pRecordingName))
			return;

		CWaypointRecording::RequestRecording(pRecordingName, CTheScripts::GetCurrentGtaScriptThread()->GetThreadId());
	}
	bool CommandGetIsWaypointRecordingLoaded(const char * pRecordingName)
	{
		Assertf(CWaypointRecording::CheckWaypointRecordingName(pRecordingName), "%s : GET_IS_WAYPOINT_RECORDING_LOADED - Waypoint recording name \'%s\' is not a valid name (must not include path)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pRecordingName);
		if(!CWaypointRecording::CheckWaypointRecordingName(pRecordingName))
			return false;

		return CWaypointRecording::IsRecordingLoaded(pRecordingName);
	}
	void CommandRemoveWaypointRecording(const char * pRecordingName)
	{
		Assertf(CWaypointRecording::CheckWaypointRecordingName(pRecordingName), "%s : REMOVE_WAYPOINT_RECORDING - Waypoint recording name \'%s\' is not a valid name (must not include path)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pRecordingName);
		if(!CWaypointRecording::CheckWaypointRecordingName(pRecordingName))
			return;

		CWaypointRecording::RemoveRecording(pRecordingName, CTheScripts::GetCurrentGtaScriptThread()->GetThreadId());
	}
	void CommandUseWaypointRecordingAsAssistedMovementRoute(const char * pRecordingName, bool bUse, float fPathWidth, float fTension)
	{
		Assertf(CWaypointRecording::CheckWaypointRecordingName(pRecordingName), "%s : USE_WAYPOINT_RECORDING_AS_ASSISTED_MOVEMENT_ROUTE - Waypoint recording name \'%s\' is not a valid name (must not include path)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pRecordingName);
		if(!CWaypointRecording::CheckWaypointRecordingName(pRecordingName))
			return;

		Assert(fTension >= 0.0f && fTension <= 1.0f);
		Assert(fPathWidth > 0.0f);

		Assertf(CWaypointRecording::IsRecordingLoaded(pRecordingName), "%s : USE_WAYPOINT_RECORDING_AS_ASSISTED_MOVEMENT_ROUTE - Recording \'%s\' isn't loaded", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pRecordingName);
		if(CWaypointRecording::IsRecordingLoaded(pRecordingName))
		{
			::CWaypointRecordingRoute * pRoute = CWaypointRecording::GetRecording(pRecordingName);
			Assert(pRoute);
			CLoadedWaypointRecording * pRecordingInfo = CWaypointRecording::GetLoadedWaypointRecordingInfo(pRoute);
			Assert(pRecordingInfo);

			Assertf(pRoute->GetSize() >= 2, "USE_WAYPOINT_RECORDING_AS_ASSISTED_MOVEMENT_ROUTE - this waypoint recording has less than 2 waypoints.  cannot make an assisted route from this!");

			if(bUse)
			{
				pRoute->SetFlags( pRoute->GetFlags() | ::CWaypointRecordingRoute::RouteFlag_AssistedMovement );

				pRecordingInfo->SetAssistedMovementPathWidth(fPathWidth);
				pRecordingInfo->SetAssistedMovementPathTension(fTension*2.0f);
			}
			else
			{
				pRoute->SetFlags( pRoute->GetFlags() & ~::CWaypointRecordingRoute::RouteFlag_AssistedMovement );
			}

			CAssistedMovementRouteStore::Process(FindPlayerPed(), true);
		}
	}
	bool CommandWaypointRecordingGetNumPoints(const char * pRecordingName, int & iNumPoints)
	{
		iNumPoints = 0;

		Assertf(CWaypointRecording::CheckWaypointRecordingName(pRecordingName), "%s : WAYPOINT_RECORDING_GET_NUM_POINTS - Waypoint recording name \'%s\' is not a valid name (must not include path)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pRecordingName);
		if(!CWaypointRecording::CheckWaypointRecordingName(pRecordingName))
			return false;

		Assertf(CWaypointRecording::IsRecordingLoaded(pRecordingName), "%s : WAYPOINT_RECORDING_GET_NUM_POINTS - Recording \'%s\' isn't loaded", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pRecordingName);
		if(CWaypointRecording::IsRecordingLoaded(pRecordingName))
		{
			::CWaypointRecordingRoute * pRoute = CWaypointRecording::GetRecording(pRecordingName);
			Assert(pRoute);

			iNumPoints = pRoute->GetSize();
			return true;
		}

		return false;
	}
	bool CommandWaypointRecordingGetCoord(const char * pRecordingName, int iPoint, Vector3 & vCoord)
	{
		Assertf(CWaypointRecording::CheckWaypointRecordingName(pRecordingName), "%s : WAYPOINT_RECORDING_GET_COORD - Waypoint recording name \'%s\' is not a valid name (must not include path)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pRecordingName);
		if(!CWaypointRecording::CheckWaypointRecordingName(pRecordingName))
			return false;

		Assertf(CWaypointRecording::IsRecordingLoaded(pRecordingName), "%s : WAYPOINT_RECORDING_GET_COORD - Recording \'%s\' isn't loaded", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pRecordingName);
		if(CWaypointRecording::IsRecordingLoaded(pRecordingName))
		{
			::CWaypointRecordingRoute * pRoute = CWaypointRecording::GetRecording(pRecordingName);
			Assert(pRoute);

			//Assert(iPoint >= 0 && iPoint < pRoute->GetSize());
			if(iPoint >= 0 && iPoint < pRoute->GetSize())
			{
				::CWaypointRecordingRoute::RouteData & data = pRoute->GetWaypoint(iPoint);
				vCoord = data.GetPosition();
				return true;
			}		

			return false;
		}

		return false;
	}
	float CommandWaypointRecordingGetSpeedAtPoint(const char * pRecordingName, int iPoint)
	{
		Assertf(CWaypointRecording::CheckWaypointRecordingName(pRecordingName), "%s : WAYPOINT_RECORDING_GET_SPEED_AT_POINT- Waypoint recording name \'%s\' is not a valid name (must not include path)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pRecordingName);
		if(!CWaypointRecording::CheckWaypointRecordingName(pRecordingName))
			return -1.0f;

		Assertf(CWaypointRecording::IsRecordingLoaded(pRecordingName), "%s : WAYPOINT_RECORDING_GET_SPEED_AT_POINT - Recording \'%s\' isn't loaded", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pRecordingName);
		if(CWaypointRecording::IsRecordingLoaded(pRecordingName))
		{
			::CWaypointRecordingRoute * pRoute = CWaypointRecording::GetRecording(pRecordingName);
			Assert(pRoute);

			//Assert(iPoint >= 0 && iPoint < pRoute->GetSize());
			if(iPoint >= 0 && iPoint < pRoute->GetSize())
			{
				::CWaypointRecordingRoute::RouteData & data = pRoute->GetWaypoint(iPoint);
				//vCoord = data.GetPosition();
				//return true;
				return data.GetSpeed();
			}		
		}

		return -1.0f;
	}
	bool CommandWaypointRecordingGetClosestWaypoint(const char * pRecordingName, const scrVector & scrVecPos, int & iOutWaypoint)
	{
		Vector3 vPos(scrVecPos);
		Assertf(CWaypointRecording::CheckWaypointRecordingName(pRecordingName), "%s : WAYPOINT_RECORDING_GET_CLOSEST_WAYPOINT - Waypoint recording name \'%s\' is not a valid name (must not include path)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pRecordingName);
		if(!CWaypointRecording::CheckWaypointRecordingName(pRecordingName))
			return false;

		Assertf(CWaypointRecording::IsRecordingLoaded(pRecordingName), "%s : WAYPOINT_RECORDING_GET_CLOSEST_WAYPOINT - Recording \'%s\' isn't loaded", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pRecordingName);
		if(CWaypointRecording::IsRecordingLoaded(pRecordingName))
		{
			::CWaypointRecordingRoute * pRoute = CWaypointRecording::GetRecording(pRecordingName);
			Assert(pRoute);

			iOutWaypoint = pRoute->GetClosestWaypointIndex(vPos);
			if(iOutWaypoint >= 0)
				return true;
		}

		return false;
	}
	void CommandTaskFollowWaypointRecording(int iPedID, const char * pRecordingName, int iStartProgress, int iFlags, int iTargetProgress)
	{
		Assertf(CWaypointRecording::CheckWaypointRecordingName(pRecordingName), "%s : TASK_FOLLOW_WAYPOINT_RECORDING - Waypoint recording name \'%s\' is not a valid name (must not include path)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pRecordingName);
		if(!CWaypointRecording::CheckWaypointRecordingName(pRecordingName))
			return;

		Assertf(CWaypointRecording::IsRecordingLoaded(pRecordingName), "%s : TASK_FOLLOW_WAYPOINT_RECORDING - the recording \'%s\' isn't loaded.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pRecordingName);

		if(CWaypointRecording::IsRecordingLoaded(pRecordingName))
		{
			::CWaypointRecordingRoute * pRoute = CWaypointRecording::GetRecording(pRecordingName);
			CTaskFollowWaypointRecording * pTaskWpt = rage_new CTaskFollowWaypointRecording(pRoute, iStartProgress, iFlags, iTargetProgress);
			CScriptPeds::GivePedScriptedTask(iPedID, pTaskWpt, SCRIPT_TASK_FOLLOW_WAYPOINT_ROUTE,
				"TASK_FOLLOW_WAYPOINT_RECORDING");
		}
	}
	CTaskFollowWaypointRecording * WaypointPlaybackHelper_GetTask(int PedIndex, const char * ASSERT_ONLY(pScriptCommand), bool ASSERT_ONLY(bAssertIfTaskNotFound)=true)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

		if (!pPed)
		{
			return 0;
		}

		CTask * pTask = pPed->GetPedIntelligence()->FindTaskPrimaryByType(CTaskTypes::TASK_FOLLOW_WAYPOINT_RECORDING);

#if __ASSERT
		if(!pTask && bAssertIfTaskNotFound)
		{
			CTask * pPrimaryTask = pPed->GetPedIntelligence()->GetTaskAtPriority(PED_TASK_PRIORITY_PRIMARY);
			CTask * pActiveTask = pPed->GetPedIntelligence()->GetTaskActive();
			CTask * pDefaultTask = pPed->GetPedIntelligence()->GetTaskActive();

			Displayf("WaypointPlaybackHelper_GetTask :\n");
			Displayf("Primary task = %s, Active task = %s, Default task = %s\n",
				pPrimaryTask ? pPrimaryTask->GetName() : "NONE",
				pActiveTask ? pActiveTask->GetName() : "NONE",
				pDefaultTask ? pDefaultTask->GetName() : "NONE");

			Assertf(pTask, "%s: %s - CTaskFollowWaypointRecording isn't running on this ped", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pScriptCommand);
		}
#endif

		if(pTask)
		{
			Assertf(pTask->GetTaskType()==CTaskTypes::TASK_FOLLOW_WAYPOINT_RECORDING, "%s: %s - CTaskFollowWaypointRecording isn't running on this ped", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pScriptCommand);
			if(pTask->GetTaskType()==CTaskTypes::TASK_FOLLOW_WAYPOINT_RECORDING)
			{
				return (CTaskFollowWaypointRecording*)pTask;
			}
		}
		return NULL;
	}

	bool CommandIsWaypointPlaybackGoingOnForPed(int PedIndex)
	{
		CTaskFollowWaypointRecording * pTask = WaypointPlaybackHelper_GetTask(PedIndex, "IS_WAYPOINT_PLAYBACK_GOING_ON_FOR_PED", false);
		if(pTask)
		{
			return true;
		}
		return false;
	}

	int CommandGetPedWaypointProgress(int iPedID)
	{
		CTaskFollowWaypointRecording * pTask = WaypointPlaybackHelper_GetTask(iPedID, "GET_PED_WAYPOINT_PROGRESS");
		if(pTask)
		{
			return ((CTaskFollowWaypointRecording*)pTask)->GetProgress();
		}
		return 0;
	}
	void CommandSetPedWaypointProgress(int iPedID, int iProgress)
	{
		CTaskFollowWaypointRecording * pTask = WaypointPlaybackHelper_GetTask(iPedID, "SET_PED_WAYPOINT_PROGRESS");
		if(pTask)
		{
			return ((CTaskFollowWaypointRecording*)pTask)->SetProgress(iProgress);
		}
	}

	float CommandGetPedWaypointDistance(int iPedID)
	{
		CTaskFollowWaypointRecording * pTask = WaypointPlaybackHelper_GetTask(iPedID, "GET_PED_WAYPOINT_DISTANCE");
		if(pTask)
		{
			return ((CTaskFollowWaypointRecording*)pTask)->GetDistanceAlongRoute();
		}
		return 0.0f;
	}

	bool CommandSetPedWaypointRouteOffset(int iPedID, const scrVector & vOffset)
	{
		CTaskFollowWaypointRecording * pTask = WaypointPlaybackHelper_GetTask(iPedID, "SET_PED_WAYPOINT_ROUTE_OFFSET");
		if(pTask)
		{
			((CTaskFollowWaypointRecording*)pTask)->SetRouteOffset(vOffset);
			return true;
		}
		return false;
	}

	float CommandGetWaypointDistanceAlongRoute(const char * pRecordingName, int iWaypoint)
	{
		Assertf(CWaypointRecording::CheckWaypointRecordingName(pRecordingName), "%s : GET_WAYPOINT_DISTANCE_ALONG_ROUTE - Waypoint recording name \'%s\' is not a valid name (must not include path)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pRecordingName);
		if(!CWaypointRecording::CheckWaypointRecordingName(pRecordingName))
			return false;

		Assertf(CWaypointRecording::IsRecordingLoaded(pRecordingName), "%s : GET_WAYPOINT_DISTANCE_ALONG_ROUTE - Recording \'%s\' isn't loaded", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pRecordingName);
		if(CWaypointRecording::IsRecordingLoaded(pRecordingName))
		{
			::CWaypointRecordingRoute * pRoute = CWaypointRecording::GetRecording(pRecordingName);
			Assert(pRoute);

			return pRoute->GetWaypointDistanceFromStart(iWaypoint);
		}
		return 0.0f;
	}

	// TODO: Replace this by a version which takes flags, and a structure.  Its likely that
	// if the waypoint routes are popular, then there will be many ways in which scripters
	// will wish to interrupt the route-following task.
	void CommandWaypointPlaybackPause(int iPedID, bool bFacePlayer, bool bStopBeforeOrientating)
	{
		CTaskFollowWaypointRecording * pTask = WaypointPlaybackHelper_GetTask(iPedID, "WAYPOINT_PLAYBACK_PAUSE");
		if(pTask)
		{
			pTask->SetPause(bFacePlayer, bStopBeforeOrientating);
		}
	}
	bool CommandWaypointPlaybackGetIsPaused(int iPedID)
	{
		CTaskFollowWaypointRecording * pTask = WaypointPlaybackHelper_GetTask(iPedID, "WAYPOINT_PLAYBACK_GET_IS_PAUSED");
		if(pTask)
		{
			return pTask->GetIsPaused();
		}
		return false;
	}
	void CommandWaypointPlaybackResume(int iPedID, bool bAchieveHeadingFirst, int iProgressToContinueFrom, int iTimeBeforeResumingMs)
	{
		CTaskFollowWaypointRecording * pTask = WaypointPlaybackHelper_GetTask(iPedID, "WAYPOINT_PLAYBACK_RESUME");
		if(pTask)
		{
			((CTaskFollowWaypointRecording*)pTask)->SetResume(bAchieveHeadingFirst, iProgressToContinueFrom, iTimeBeforeResumingMs);
		}
	}

	void CommandWaypointPlaybackUseDefaultSpeed(int iPedID)
	{
		CTaskFollowWaypointRecording * pTask = WaypointPlaybackHelper_GetTask(iPedID, "WAYPOINT_PLAYBACK_USE_DEFAULT_SPEED");
		if(pTask)
		{
			pTask->UseDefaultMoveBlendRatio();
		}
	}
	void CommandWaypointPlaybackOverrideSpeed(int iPedID, float fMBR, bool bAllowSlowingForCorners)
	{
		CTaskFollowWaypointRecording * pTask = WaypointPlaybackHelper_GetTask(iPedID, "WAYPOINT_PLAYBACK_OVERRIDE_SPEED");
		if(pTask)
		{
			Assertf(fMBR >= MOVEBLENDRATIO_STILL && fMBR <= MOVEBLENDRATIO_SPRINT, "%s: WAYPOINT_PLAYBACK_OVERRIDE_SPEED - MoveBlendRatio must be between 0.0 (still) and 3.0 (sprint)", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			fMBR = Clamp(fMBR, MOVEBLENDRATIO_STILL, MOVEBLENDRATIO_SPRINT);
			pTask->SetOverrideMoveBlendRatio(fMBR, bAllowSlowingForCorners);
		}
	}
	void CommandWaypointPlaybackStartAimingAtPed(int iPedID, int iOtherPedID, bool bUseRunAndGun)
	{
		CTaskFollowWaypointRecording * pTask = WaypointPlaybackHelper_GetTask(iPedID, "WAYPOINT_PLAYBACK_START_AIMING_AT_PED");
		if(pTask)
		{
			const CPed * pOtherPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(iOtherPedID);

			if (pOtherPed)
			{
				pTask->StartAimingAtEntity(pOtherPed, bUseRunAndGun);
			}
		}
	}
	void CommandWaypointPlaybackStartAimingAtEntity(int iPedID, int iEntityID, bool bUseRunAndGun)
	{
		CTaskFollowWaypointRecording * pTask = WaypointPlaybackHelper_GetTask(iPedID, "WAYPOINT_PLAYBACK_START_AIMING_AT_ENTITY");
		if(pTask)
		{
			const CPhysical* pEntity = CTheScripts::GetEntityToQueryFromGUID<CPhysical>(iEntityID);

			if (pEntity)
			{
				pTask->StartAimingAtEntity(pEntity, bUseRunAndGun);
			}
		}
	}
	void CommandWaypointPlaybackStartAimingAtCoord(int iPedID, const scrVector & scrVecCoord, bool bUseRunAndGun)
	{
		Vector3 vCoord(scrVecCoord);
		CTaskFollowWaypointRecording * pTask = WaypointPlaybackHelper_GetTask(iPedID, "WAYPOINT_PLAYBACK_START_AIMING_AT_COORD");
		if(pTask)
		{
			pTask->StartAimingAtPos(vCoord, bUseRunAndGun);
		}
	}
	void CommandWaypointPlaybackStartShootingAtPed(int iPedID, int iOtherPedID, bool bUseRunAndGun, int FiringPatternHash)
	{
		CTaskFollowWaypointRecording * pTask = WaypointPlaybackHelper_GetTask(iPedID, "WAYPOINT_PLAYBACK_START_SHOOTING_AT_PED");
		if(pTask)
		{
			const CPed * pOtherPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(iOtherPedID);

			if (pOtherPed)
			{
				u32 uFiringPatternHash = (u32)FiringPatternHash;
				if(uFiringPatternHash == 0)
				{
					const CPed * pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(iPedID);
					uFiringPatternHash = pPed ? pPed->GetPedIntelligence()->GetCombatBehaviour().GetFiringPatternHash().GetHash() : 0;
				}

				pTask->StartShootingAtEntity(pOtherPed, bUseRunAndGun, uFiringPatternHash);
			}
		}
	}
	void CommandWaypointPlaybackStartShootingAtEntity(int iPedID, int iEntityID, bool bUseRunAndGun, int FiringPatternHash)
	{
		CTaskFollowWaypointRecording * pTask = WaypointPlaybackHelper_GetTask(iPedID, "WAYPOINT_PLAYBACK_START_SHOOTING_AT_ENTITY");
		if(pTask)
		{
			const CPhysical* pEntity = CTheScripts::GetEntityToQueryFromGUID<CPhysical>(iEntityID);

			if (pEntity)
			{
				u32 uFiringPatternHash = (u32)FiringPatternHash;
				if(uFiringPatternHash == 0)
				{
					const CPed * pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(iPedID);
					uFiringPatternHash = pPed ? pPed->GetPedIntelligence()->GetCombatBehaviour().GetFiringPatternHash().GetHash() : 0;
				}

				pTask->StartShootingAtEntity(pEntity, bUseRunAndGun, uFiringPatternHash);
			}
		}
	}
	void CommandWaypointPlaybackStartShootingAtCoord(int iPedID, const scrVector & scrVecCoord, bool bUseRunAndGun, int FiringPatternHash)
	{
		Vector3 vCoord(scrVecCoord);
		CTaskFollowWaypointRecording * pTask = WaypointPlaybackHelper_GetTask(iPedID, "WAYPOINT_PLAYBACK_START_SHOOTING_AT_COORD");
		if(pTask)
		{
			u32 uFiringPatternHash = (u32)FiringPatternHash;
			if(uFiringPatternHash == 0)
			{
				const CPed * pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(iPedID);
				uFiringPatternHash = pPed ? pPed->GetPedIntelligence()->GetCombatBehaviour().GetFiringPatternHash().GetHash() : 0;
			}
			pTask->StartShootingAtPos(vCoord, bUseRunAndGun, uFiringPatternHash);
		}
	}
	void CommandWaypointPlaybackStopAimingOrShooting(int iPedID)
	{
		CTaskFollowWaypointRecording * pTask = WaypointPlaybackHelper_GetTask(iPedID, "WAYPOINT_PLAYBACK_STOP_AIMING_OR_SHOOTING");
		if(pTask)
		{
			pTask->StopAimingOrShooting();
		}
	}



	void CommandAssistedMovementRequestRoute(const char * pRouteName)
	{
		const u32 iNameHash = atStringHash(pRouteName);
		CAssistedMovementRouteStore::GetRouteToggles().TurnOnThisRoute(iNameHash, CTheScripts::GetCurrentGtaScriptThread()->GetThreadId());

		CPed * pPlayer = FindPlayerPed();
		if(pPlayer)
		{
			CTaskMovePlayer * pTaskMovePlayer = (CTaskMovePlayer*)pPlayer->GetPedIntelligence()->FindMovementTaskByType(CTaskTypes::TASK_MOVE_PLAYER);
			if(pTaskMovePlayer)
				pTaskMovePlayer->GetAssistedMovementControl()->SetJustTeleported();
		}
	}
	void CommandAssistedMovementRemoveRoute(const char * pRouteName)
	{
		const u32 iNameHash = atStringHash(pRouteName);
		CAssistedMovementRouteStore::GetRouteToggles().TurnOffThisRoute(iNameHash, CTheScripts::GetCurrentGtaScriptThread()->GetThreadId());
	}
	bool CommandAssistedMovementIsRouteLoaded(const char * pRouteName)
	{
		Assert(pRouteName && *pRouteName);
		u32 iHash = atStringHash(pRouteName);
		return (CAssistedMovementRouteStore::GetRouteByNameHash(iHash) != NULL);
	}
	int CommandAssistedMovementGetRouteProperties(const char * pRouteName)
	{
		s32 iScriptFlags = 0;
		Assert(pRouteName && *pRouteName);

		u32 iHash = atStringHash(pRouteName);
		CAssistedMovementRoute * pRoute = CAssistedMovementRouteStore::GetRouteByNameHash(iHash);
		if(SCRIPT_VERIFY(pRoute, "named route is not loaded"))
		{
			if(pRoute->GetFlags() & CAssistedMovementRoute::RF_IsActiveWhenStrafing)
				iScriptFlags |= EASSISTED_ROUTE_ACTIVE_WHEN_STRAFING;
			if(pRoute->GetFlags() & CAssistedMovementRoute::RF_DisableInForwardsDirection)
				iScriptFlags |= EASSISTED_ROUTE_DISABLE_IN_FORWARDS_DIRECTION;
			if(pRoute->GetFlags() & CAssistedMovementRoute::RF_DisableInReverseDirection)
				iScriptFlags |= EASSISTED_ROUTE_DISABLE_IN_REVERSE_DIRECTION;
		}
		return iScriptFlags;
	}
	void CommandAssistedMovementSetRouteProperties(const char * pRouteName, int iScriptFlags)
	{
		Assert(pRouteName && *pRouteName);

		u32 iHash = atStringHash(pRouteName);
		CAssistedMovementRoute * pRoute = CAssistedMovementRouteStore::GetRouteByNameHash(iHash);
		if(SCRIPT_VERIFY(pRoute, "named route is not loaded"))
		{
			if((iScriptFlags & EASSISTED_ROUTE_ACTIVE_WHEN_STRAFING)!=0)
			{
				pRoute->SetFlags( pRoute->GetFlags() | CAssistedMovementRoute::RF_IsActiveWhenStrafing );
			}
			else
			{
				pRoute->SetFlags( pRoute->GetFlags() & ~CAssistedMovementRoute::RF_IsActiveWhenStrafing );
			}

			if((iScriptFlags & EASSISTED_ROUTE_DISABLE_IN_FORWARDS_DIRECTION)!=0)
			{
				pRoute->SetFlags( pRoute->GetFlags() | CAssistedMovementRoute::RF_DisableInForwardsDirection );
			}
			else
			{
				pRoute->SetFlags( pRoute->GetFlags() & ~CAssistedMovementRoute::RF_DisableInForwardsDirection );
			}

			if((iScriptFlags & EASSISTED_ROUTE_DISABLE_IN_REVERSE_DIRECTION)!=0)
			{
				pRoute->SetFlags( pRoute->GetFlags() | CAssistedMovementRoute::RF_DisableInReverseDirection );
			}
			else
			{
				pRoute->SetFlags( pRoute->GetFlags() & ~CAssistedMovementRoute::RF_DisableInReverseDirection );
			}
		}
	}
	void CommandAssistedMovementOverrideLoadDistanceThisFrame(float fDistance)
	{
		Assertf(fDistance > 0.0f && fDistance < 200.0f, "Value out of range.");

		CAssistedMovementRouteStore::SetStreamingLoadDistance(fDistance);
	}

	////////////////////////////////////
	// VEHICLE WAYPOINT RECORDING STUFF
	////////////////////////////////////

	void CommandTaskVehicleFollowWaypointRecording(int iPedID, int iVehicleID, const char * pRecordingName, int iDrivingFlags, int iStartProgress, int iFlags, int iTargetProgress, float fMaxSpeed, bool bLoop, float fTargetArriveDistance)
	{
		Assertf(CWaypointRecording::CheckWaypointRecordingName(pRecordingName), "%s : TASK_VEHICLE_FOLLOW_WAYPOINT_RECORDING - Waypoint recording name \'%s\' is not a valid name (must not include path)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pRecordingName);
		if(!CWaypointRecording::CheckWaypointRecordingName(pRecordingName))
			return;

		Assertf(CWaypointRecording::IsRecordingLoaded(pRecordingName), "%s : TASK_VEHICLE_FOLLOW_WAYPOINT_RECORDING - the recording \'%s\' isn't loaded.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pRecordingName);

		CVehicle* pVehicle = NULL;

		if(SCRIPT_VERIFY(NULL_IN_SCRIPTING_LANGUAGE != iVehicleID, "TASK_VEHICLE_FOLLOW_WAYPOINT_RECORDING - You must specify a vehicle!"))
		{
			pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(iVehicleID);
		}

		if(!pVehicle)
		{
			return;
		}

		if(CWaypointRecording::IsRecordingLoaded(pRecordingName))
		{
			::CWaypointRecordingRoute * pRoute = CWaypointRecording::GetRecording(pRecordingName);
			//CTaskFollowWaypointRecording * pTaskWpt = rage_new CTaskFollowWaypointRecording(pRoute, iStartProgress, iFlags, iTargetProgress);
			//CScriptPeds::GivePedScriptedTask(iVehicleID, pTaskWpt, SCRIPT_TASK_FOLLOW_WAYPOINT_ROUTE,
			//	"TASK_FOLLOW_WAYPOINT_RECORDING");
			sVehicleMissionParams params;
			params.m_iDrivingFlags = iDrivingFlags;
			if (fMaxSpeed > 0.0f)
			{
				params.m_fCruiseSpeed = fMaxSpeed;
			}
			else
			{
				params.m_fCruiseSpeed = 64.0f;
			}
			params.m_fTargetArriveDist = fTargetArriveDistance;

			SCRIPT_ASSERTF(pRoute->GetSize() > 1, "TASK_VEHICLE_FOLLOW_WAYPOINT_RECORDING - Route %s doesn't have enough points!", pRecordingName);

			CTaskVehicleFollowWaypointRecording* pFollowWaypointTask = rage_new CTaskVehicleFollowWaypointRecording(pRoute, params, iStartProgress, iFlags, iTargetProgress);
			if (pFollowWaypointTask)
			{
				pFollowWaypointTask->SetLoopPlayback(bLoop);
			}

			CTask* pTask = rage_new CTaskControlVehicle(pVehicle, pFollowWaypointTask);
			CScriptPeds::GivePedScriptedTask( iPedID, pTask, SCRIPT_TASK_VEHICLE_FOLLOW_WAYPOINT_RECORDING, "TASK_VEHICLE_FOLLOW_WAYPOINT_RECORDING");
		}
	}

	CTaskVehicleFollowWaypointRecording * VehicleWaypointPlaybackHelper_GetTask(int iVehicleID, const char * ASSERT_ONLY(pScriptCommand))
	{
		CVehicle* pVehicle = NULL;
		if(SCRIPT_VERIFY(NULL_IN_SCRIPTING_LANGUAGE != iVehicleID, "TASK_VEHICLE_FOLLOW_WAYPOINT_RECORDING - You must specify a vehicle!"))
		{
			pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(iVehicleID);
		}

		if(!pVehicle)
		{
			return NULL;
		}	

		CTask * pTask = static_cast<CTask*>(pVehicle->GetIntelligence()->GetTaskManager()->FindTaskByTypeActive(VEHICLE_TASK_TREE_PRIMARY, CTaskTypes::TASK_VEHICLE_FOLLOW_WAYPOINT_RECORDING));
		if(pTask)
		{
			Assertf(pTask->GetTaskType()==CTaskTypes::TASK_VEHICLE_FOLLOW_WAYPOINT_RECORDING, "%s: %s - CTaskVehicleFollowWaypointRecording isn't running on this vehicle", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pScriptCommand);
			if(pTask->GetTaskType()==CTaskTypes::TASK_VEHICLE_FOLLOW_WAYPOINT_RECORDING)
			{
				return (CTaskVehicleFollowWaypointRecording*)pTask;
			}
		}
		return NULL;
	}

	bool CommandIsWaypointPlaybackGoingOnForVehicle(int iVehicleID)
	{
		CTaskVehicleFollowWaypointRecording * pTask = VehicleWaypointPlaybackHelper_GetTask(iVehicleID, "IS_WAYPOINT_PLAYBACK_GOING_ON_FOR_VEHICLE");
		if(pTask)
		{
			return true;
		}
		return false;
	}

	int CommandGetVehicleWaypointProgress(int iVehicleID)
	{
		CTaskVehicleFollowWaypointRecording * pTask = VehicleWaypointPlaybackHelper_GetTask(iVehicleID, "GET_VEHICLE_WAYPOINT_PROGRESS");
		if(pTask)
		{
			return ((CTaskVehicleFollowWaypointRecording*)pTask)->GetProgress();
		}
		return 0;
	}
	int CommandGetVehicleWaypointTargetPoint(int iVehicleID)
	{
		CTaskVehicleFollowWaypointRecording * pTask = VehicleWaypointPlaybackHelper_GetTask(iVehicleID, "GET_VEHICLE_WAYPOINT_TARGET_POINT");
		if(pTask)
		{
			const int iWaypointProgress = pTask->GetProgress();
			const int iTargetPoint = pTask->GetFollowRouteHelper()->GetCurrentTargetPoint();
			return iWaypointProgress + iTargetPoint;
			//return ((CTaskVehicleFollowWaypointRecording*)pTask)->GetProgress();
		}
		return 0;
	}
	// TODO: Replace this by a version which takes flags, and a structure.  Its likely that
	// if the waypoint routes are popular, then there will be many ways in which scripters
	// will wish to interrupt the route-following task.
	void CommandVehicleWaypointPlaybackPause(int iVehicleID)
	{
		CTaskVehicleFollowWaypointRecording * pTask = VehicleWaypointPlaybackHelper_GetTask(iVehicleID, "VEHICLE_WAYPOINT_PLAYBACK_PAUSE");
		if(pTask)
		{
			pTask->SetPause();
		}
	}
	bool CommandVehicleWaypointPlaybackGetIsPaused(int iVehicleID)
	{
		CTaskVehicleFollowWaypointRecording * pTask = VehicleWaypointPlaybackHelper_GetTask(iVehicleID, "VEHICLE_WAYPOINT_PLAYBACK_GET_IS_PAUSED");
		if(pTask)
		{
			return pTask->GetIsPaused();
		}
		return false;
	}
	void CommandVehicleWaypointPlaybackResume(int iVehicleID)
	{
		CTaskVehicleFollowWaypointRecording * pTask = VehicleWaypointPlaybackHelper_GetTask(iVehicleID, "VEHICLE_WAYPOINT_PLAYBACK_RESUME");
		if(pTask)
		{
			((CTaskVehicleFollowWaypointRecording*)pTask)->SetResume();
		}
	}

	void CommandVehicleWaypointPlaybackUseDefaultSpeed(int iVehicleID)
	{
		CTaskVehicleFollowWaypointRecording * pTask = VehicleWaypointPlaybackHelper_GetTask(iVehicleID, "VEHICLE_WAYPOINT_PLAYBACK_USE_DEFAULT_SPEED");
		if(pTask)
		{
			pTask->UseDefaultSpeed();
		}
	}
	void CommandVehicleWaypointPlaybackOverrideSpeed(int iVehicleID, float fSpeed)
	{
		CTaskVehicleFollowWaypointRecording * pTask = VehicleWaypointPlaybackHelper_GetTask(iVehicleID, "VEHICLE_WAYPOINT_PLAYBACK_OVERRIDE_SPEED");
		if(pTask)
		{
			pTask->SetOverrideSpeed(fSpeed);
		}
	}
}

