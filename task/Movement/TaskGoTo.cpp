#include "TaskGoTo.h"

#include "Camera/CamInterface.h"
#include "event/eventDamage.h"
#include "Peds/PedIntelligence.h"
#include "scene/world/GameWorldHeightMap.h"
#include "script/script_cars_and_peds.h"
#include "Task/Combat/Cover/TaskCover.h"
#include "Task/Combat/TaskCombat.h"
#include "Task/General/TaskBasic.h"
#include "Task/Motion/TaskMotionBase.h"
#include "Task/Movement/TaskGotoPoint.h"
#include "Task/Movement/TaskNavMesh.h"
#include "Task/Movement/TaskSlideToCoord.h"
#include "Task/Scenario/ScenarioManager.h"
#include "Task/Scenario/ScenarioPoint.h"
#include "Task/Scenario/Types/TaskUseScenario.h"
#include "Task/Vehicle/TaskCar.h"
#include "Task/Vehicle/TaskCarUtils.h"
#include "Task/Vehicle/TaskEnterVehicle.h"
#include "Task/Vehicle/TaskExitVehicle.h"
#include "VehicleAI/Task/TaskVehicleMissionBase.h"
#include "VehicleAI/Task/TaskVehicleGoToAutomobile.h"
#include "VehicleAI/Task/TaskVehicleGoToBoat.h"
#include "VehicleAI/task/TaskVehicleGoToPlane.h"
#include "VehicleAI/task/TaskVehicleGoToHelicopter.h"
#include "VehicleAI/task/TaskVehicleGoToLongRange.h"
#include "Vehicles/Planes.h"
// Rage headers
#include "Math/amath.h"
#include "Math/angmath.h"
#include "grcore/debugdraw.h"

AI_OPTIMISATIONS()


//********************************
// CTaskMoveFollowPointRoute
//

const float CTaskMoveFollowPointRoute::ms_fTargetRadius=0.5f;
const float CTaskMoveFollowPointRoute::ms_fSlowDownDistance=5.0f;   
CPointRoute CTaskMoveFollowPointRoute::ms_pointRoute;

CTaskMoveFollowPointRoute::CTaskMoveFollowPointRoute(
	const float fMoveBlendRatio,
	const CPointRoute& route, 
	const int iMode,
	const float fTargetRadius,
	const float fSlowDownDistance,
	const bool bMustOvershootTarget,
	const bool bUseBlending,
	const bool bStandStillAfterMove,
	const int iStartingProgress,
	const float fWaypointRadius)
	: CTaskMove(fMoveBlendRatio),
	m_iMode(iMode),
	m_iProgress(0),
	m_bMustOvershootTarget(bMustOvershootTarget),
	m_bStandStillAfterMove(bStandStillAfterMove),
	m_bUseBlending(bUseBlending),
	m_bUseRunstopInsteadOfSlowingToWalk(true),
	m_bStopExactly(false)
{
	Init(route,fTargetRadius,fSlowDownDistance,fWaypointRadius,iStartingProgress);
	SetInternalTaskType(CTaskTypes::TASK_MOVE_FOLLOW_POINT_ROUTE);
}

void CTaskMoveFollowPointRoute::Init(const CPointRoute& route,float fTargetRadius, float fSlowDownDistance, float fWaypointRadius, int iStartingProgress)
{
	m_pRoute=rage_new CPointRoute;
	SetRoute(route,fTargetRadius,fSlowDownDistance,true);
	m_iProgress = iStartingProgress;

	// If a non-negative waypoint radius is supplied then use this as the target radius for all the goto tasks we create -
	// apart from the final GoToPointAndStandStill.  Otherwise default to using the CTaskMoveGoToPoint::ms_fTargetRadius.
	if(fWaypointRadius >= 0.0f)
		m_fWaypointRadius = fWaypointRadius;
	else
		m_fWaypointRadius = CTaskMoveGoToPoint::ms_fTargetRadius;
}

CTaskMoveFollowPointRoute::~CTaskMoveFollowPointRoute()
{
	if(m_pRoute) delete m_pRoute;
}

void CTaskMoveFollowPointRoute::SetRoute(const CPointRoute& route, const float fTargetRadius, const float fSlowDownDistance, const bool bForce)
{
    if((bForce)||(*m_pRoute!=route)||(m_fTargetRadius!=fTargetRadius)||(m_fSlowDownDistance!=fSlowDownDistance))
    {
        *m_pRoute=route;
        m_fTargetRadius=fTargetRadius;
        m_fSlowDownDistance=fSlowDownDistance;
        m_iProgress=0;
        m_iRouteTraversals=0;
        m_bNewRoute=true;
    }
}

float CTaskMoveFollowPointRoute::GetDistanceLeftOnRoute(const CPed * pPed) const
{
	float fLength = 0.0f;
	// First total up path length from point we're approaching, to the end of the route
	for(s32 p=m_iProgress; p<m_pRoute->GetSize()-1; p++)
	{
		Vector3 v1 = m_pRoute->Get(p);
		Vector3 v2 = m_pRoute->Get(p+1);
		fLength += (v2-v1).Mag();
	}
	// Now add on the distance we are from the point we're approaching
	Vector3 v = m_pRoute->Get(m_iProgress) - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	fLength += v.Mag();

	return fLength;
}

void CTaskMoveFollowPointRoute::ProcessSlowDownDistance(const CPed * pPed)
{
	if(m_fSlowDownDistance > 0.0f)
	{
		const float fDistLeft = GetDistanceLeftOnRoute(pPed);
		if(fDistLeft < m_fSlowDownDistance)
		{
			const float fRatio = fDistLeft / m_fSlowDownDistance;
			const float fMBR = Lerp(fRatio, MOVEBLENDRATIO_WALK, m_fMoveBlendRatio);

			CTask * pSubTask = GetSubTask();
			if(pSubTask)
			{
				pSubTask->PropagateNewMoveSpeed(fMBR);
			}
		}
	}
}

int CTaskMoveFollowPointRoute::GetSubTaskType()
{
	int iSubTaskType=CTaskTypes::TASK_FINISHED;

    if(0==m_pRoute->GetSize())
    {
       iSubTaskType=CTaskTypes::TASK_FINISHED; 
    }
    else if((m_iProgress+1)<m_pRoute->GetSize())
    {
        iSubTaskType=CTaskTypes::TASK_MOVE_GO_TO_POINT_ON_ROUTE;
    }
    else if((m_iProgress+1)==m_pRoute->GetSize())
    {
	    if(m_bStandStillAfterMove) 
		{
	        iSubTaskType=CTaskTypes::TASK_MOVE_GO_TO_POINT_AND_STAND_STILL;
		}
		else 
		{
			iSubTaskType=CTaskTypes::TASK_MOVE_GO_TO_POINT;
		}
    }
    else if(m_iProgress==m_pRoute->GetSize())
    {       
		m_iRouteTraversals++;

    	switch(m_iMode)
    	{    		
    		case TICKET_SINGLE:
   		        iSubTaskType=CTaskTypes::TASK_FINISHED;
				break;
    		case TICKET_RETURN:
    			if(1==m_iRouteTraversals)
    			{
    				m_pRoute->Reverse();
    				m_iProgress=0;
    				iSubTaskType=GetSubTaskType();
    			}
    			else
    			{
			        iSubTaskType=CTaskTypes::TASK_FINISHED;
    			}
    			break;
    		case TICKET_SEASON:    		
   				m_pRoute->Reverse();
   				m_iProgress=0;
   				iSubTaskType=GetSubTaskType();
    			break;
			case TICKET_LOOP:
				m_iProgress=0;
   				iSubTaskType=GetSubTaskType();
				break;
			default:
				Assert(false);
				break;
    	}
    }
    else
    {
        Assert(false);
    }
    return iSubTaskType;
}



aiTask* CTaskMoveFollowPointRoute::CreateSubTask(const int iTaskType, CPed* UNUSED_PARAM(pPed))
{
    switch(iTaskType)
    {
        case CTaskTypes::TASK_MOVE_GO_TO_POINT:
		{
            return rage_new CTaskMoveGoToPoint(
            	m_fMoveBlendRatio,
            	m_pRoute->Get(m_iProgress),
				m_bStandStillAfterMove ? m_fWaypointRadius : m_fTargetRadius,
            	false
            );           
		}
        case CTaskTypes::TASK_MOVE_GO_TO_POINT_ON_ROUTE:
		{
			Assert((m_iProgress+1) < m_pRoute->GetSize());
            return rage_new CTaskMoveGoToPointOnRoute(
            	m_fMoveBlendRatio,
            	m_pRoute->Get(m_iProgress),
				m_pRoute->Get(m_iProgress+1),
				m_bUseBlending,
            	m_fWaypointRadius,
            	false,
            	m_fSlowDownDistance
            );           
		}
		case CTaskTypes::TASK_MOVE_GO_TO_POINT_AND_STAND_STILL:
		{
			float fSlowDownDistance=m_fSlowDownDistance;
			switch(m_iMode)
			{
				case TICKET_SINGLE:
					break;
				case TICKET_RETURN:
					if(0==m_iRouteTraversals)
					{
						fSlowDownDistance=0;
					}
					break;
				case TICKET_SEASON:
					fSlowDownDistance=0;
					break;
				case TICKET_LOOP:
					fSlowDownDistance=0;
					break;
				default:
					Assert(false);
					break;
			}

			CTaskMoveGoToPointAndStandStill * pTask = rage_new CTaskMoveGoToPointAndStandStill(m_fMoveBlendRatio, m_pRoute->Get(m_iProgress), m_fTargetRadius, fSlowDownDistance, false, m_bStopExactly);
			pTask->SetUseRunstopInsteadOfSlowingToWalk( GetUseRunstopInsteadOfSlowingToWalk() );

			return pTask;
		}
		case CTaskTypes::TASK_MOVE_STAND_STILL:
		{
			return rage_new CTaskMoveStandStill();
		}
        case CTaskTypes::TASK_FINISHED:
		{
            return NULL;
		}
        default:
		{
            Assert(false);
            return NULL;
		}
    } 
}    

bool CTaskMoveFollowPointRoute::HasTarget() const
{
	const CTask * pSubTask = GetSubTask();
	if(pSubTask && (
		pSubTask->GetTaskType()==CTaskTypes::TASK_MOVE_GO_TO_POINT ||
		pSubTask->GetTaskType()==CTaskTypes::TASK_MOVE_GO_TO_POINT_ON_ROUTE ||
		pSubTask->GetTaskType()==CTaskTypes::TASK_MOVE_GO_TO_POINT_AND_STAND_STILL))
		return true;

	return false;
}

//-------------------------------------------------------------------------
// Updates the status of the passed task so this tasks current status.
// Used by control movement tasks to keep the stored version they have up to date, so that
// in the case that this task is removed it can re-instate one at the same progress.
//-------------------------------------------------------------------------
void CTaskMoveFollowPointRoute::UpdateProgress( aiTask* pSameTask )
{
	Assert( pSameTask->GetTaskType() == GetTaskType() );
	CTaskMoveFollowPointRoute* pSameFollowPointTask = (CTaskMoveFollowPointRoute*)pSameTask;
	pSameFollowPointTask->SetProgress( GetProgress() );
}

CTask::FSM_Return
CTaskMoveFollowPointRoute::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

FSM_Begin
	FSM_State(State_Initial)
		FSM_OnUpdate
			return Initial_OnUpdate(pPed);
	FSM_State(State_FollowingRoute)
		FSM_OnEnter
			return FollowingRoute_OnEnter(pPed);
		FSM_OnUpdate
			return FollowingRoute_OnUpdate(pPed);
FSM_End
}

CTask::FSM_Return
CTaskMoveFollowPointRoute::Initial_OnUpdate(CPed* pPed)
{
#if !__FINAL
    m_vStartPos=VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
#endif

    m_bNewRoute = false;

    if(m_pRoute->GetSize()==m_iProgress)
    {
    	//Route has zero size so instantly finish.
		return FSM_Quit;
	}
	else if(pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
	{
		Assertf(0,"FollowPointRoute while in vehicle!");
		return FSM_Quit;
	}
	else
	{
		if(TICKET_SINGLE!=m_iMode)
		{
			int iBestProgress=-1;
			float fMinDist=FLT_MAX;
			
			//Work out the line that is closest to the ped
			//and use the first point in the line as the start progress.
			int i;
			const Vector3 vPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
			const int N=m_pRoute->GetSize();
			for(i=0;i<N;i++)
			{
				const int i0=(i+0)%N;
				const int i1=(i+1)%N;
				const Vector3& v0=m_pRoute->Get(i0);
				const Vector3& v1=m_pRoute->Get(i1);
				Vector3 w;
				w=v1-v0;
				const float l=w.Mag();
				w.Normalize();
				Vector3 vDiff;
				vDiff=vPos-v0;
				const float t=DotProduct(vDiff,w);
				if((t>0)&&(t<l))
				{
					Vector3 p(w);
					p*=t;
					p+=v0;
					vDiff=vPos-p;
					const float f2=vDiff.Mag2();
					if(f2<(fMinDist*fMinDist))
					{
						iBestProgress=i1;
						fMinDist=f2;
					}
				}			
			}
			
			//If we didn't find a suitable line then just head towards the 
			//nearest point. 
			fMinDist=FLT_MAX;
			if(-1==iBestProgress)
			{
				for(i=0;i<N;i++)
				{
					Vector3 vDiff;
					vDiff=vPos-m_pRoute->Get(i);
					const float f2=vDiff.Mag2();	
					if(f2<(fMinDist*fMinDist))
					{
						iBestProgress=i;
						fMinDist=f2;
					}
				}
			}
			
			Assert(m_iProgress>=0);
			m_iProgress=iBestProgress;
		}
		
		SetState(State_FollowingRoute);

		return FSM_Continue;
	}
}

CTask::FSM_Return
CTaskMoveFollowPointRoute::FollowingRoute_OnEnter(CPed* pPed)
{
    const int iSubTaskType = GetSubTaskType();
	aiTask * pNewTask = CreateSubTask(iSubTaskType,pPed);

	SetNewTask(pNewTask);

	return FSM_Continue;
}

CTask::FSM_Return
CTaskMoveFollowPointRoute::FollowingRoute_OnUpdate(CPed* pPed)
{
	Assert(GetSubTask() &&
		(GetSubTask()->GetTaskType()==CTaskTypes::TASK_MOVE_GO_TO_POINT_ON_ROUTE ||
			GetSubTask()->GetTaskType()==CTaskTypes::TASK_MOVE_GO_TO_POINT ||
			GetSubTask()->GetTaskType()==CTaskTypes::TASK_MOVE_GO_TO_POINT_AND_STAND_STILL));

	// If the ped is stuck we'll abort the task
	if(pPed->GetPedIntelligence()->GetEventScanner()->GetStaticMovementScanner().GetStaticCounter() > CStaticMovementScanner::ms_iStaticCounterStuckLimit)
	{
		if(GetSubTask()->MakeAbortable( ABORT_PRIORITY_URGENT, NULL))
			return FSM_Quit;
		else
			return FSM_Continue;
	}
	// If somehow route has zero points, we'll abort.  Can't see how this could happen, but it has before
	if(m_pRoute->GetSize() <= 0)
	{
		if(GetSubTask()->MakeAbortable( ABORT_PRIORITY_URGENT, NULL))
			return FSM_Quit;
		else
			return FSM_Continue;
	}

	// restart if a new route has been set
	if(m_bNewRoute)
	{
		if(GetSubTask()->MakeAbortable( ABORT_PRIORITY_URGENT, NULL))
		{
			SetState(State_Initial);
		}
		return FSM_Continue;
	}

	// Handle creating the next child task
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		if(GetSubTask()->GetTaskType()==CTaskTypes::TASK_MOVE_GO_TO_POINT &&
			(m_iProgress+1 == m_pRoute->GetSize()) && (!m_bStandStillAfterMove))
		{
			return FSM_Quit;
		}

		m_iProgress++;
		const int iSubTaskType = GetSubTaskType();

		if(iSubTaskType==CTaskTypes::TASK_FINISHED)
		{
			return FSM_Quit;
		}
		else
		{
			aiTask * pNewTask = CreateSubTask(iSubTaskType, pPed);
			SetNewTask(pNewTask);
		}
	}

	// Handle processing the slow-down distance
	// JB: Previously this was confined to the last route section; I've extended
	// it to operate across multiple route sections towards the end of the route.

	ProcessSlowDownDistance(pPed);

	return FSM_Continue;
}

#if !__FINAL
void CTaskMoveFollowPointRoute::Debug() const
{
#if DEBUG_DRAW
	Color32 col = Color32(0x80, 0xff, 0x00, 0xff);
    int i;
    for(i=0;i<m_pRoute->GetSize()-1;i++)
    {
        grcDebugDraw::Line(m_pRoute->Get(i), m_pRoute->Get(i+1), col, col);
    }
	char tmp[8];
	for(i=0;i<m_pRoute->GetSize();i++)
	{
		sprintf(tmp, "%i", i);
		grcDebugDraw::Text(m_pRoute->Get(i), col, tmp);
	}
#endif
    
    if(GetSubTask()) {
    
    	GetSubTask()->Debug();
   	}
}
#endif

// Return how far the ped is from their current route segment, or return zero if not on a route
bool
CTaskMoveFollowPointRoute::GetClosestPositionOnCurrentRouteSegment(const CPed * pPed, Vector3 & vClosestPos)
{
	Assert(m_pRoute && m_iProgress>=0 && m_iProgress < m_pRoute->GetSize());

	// Just heading towards the first point on the route?
	if(m_iProgress==0)
	{
		vClosestPos = m_pRoute->Get(0);
		return true;
	}

	const Vector3 & vLast = m_pRoute->Get(m_iProgress-1);
	const Vector3 & vCurr = m_pRoute->Get(m_iProgress);
	const Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	const Vector3 vDiff = vCurr - vLast;
	float fTVal = (vDiff.Mag2() > 0.0f) ? geomTValues::FindTValueSegToPoint(vLast, vDiff, vPedPos) : 0.0f;
	vClosestPos = vLast + ((vCurr - vLast) * fTVal);

	return true;
}






//*********************************
// CTaskGoToPointAnyMeans
//

CClonedGoToPointAnyMeansInfo::CClonedGoToPointAnyMeansInfo()
	: m_bHasTargetVehicle(false)
	, m_bHasTargetPosition(false)
	, m_vTarget(Vector3::ZeroType)
	, m_fMoveBlendRatio(0.0f)
	, m_fTargetRadius(0.0f)
	, m_fNavMeshCompletionRadius(0.0f)
	, m_bHasDesiredCarModel(false)
	, m_iDesiredCarModel(-1)
	, m_bGoAsFarAsPossibleIfNavMeshNotLoaded(false)
	, m_bPreferToUsePavements(false)
	, m_bUseLongRangeVehiclePathing(false)
	, m_iDrivingFlags(DMode_StopForCars)
	, m_fExtraVehToTargetDistToPreferVeh(0.0f)
	, m_fDriveStraightLineDistance(sVehicleMissionParams::DEFAULT_SWITCH_TO_STRAIGHT_LINE_DIST)
	, m_iFlags(0)
	, m_fOverrideCruiseSpeed(-1.0f)
{

}

CClonedGoToPointAnyMeansInfo::CClonedGoToPointAnyMeansInfo(s32 iState,
	const float fMoveBlendRatio, 
	const Vector3& vTarget, 
	const CVehicle* pTargetVehicle,
	const float fTargetRadius,
	const int iDesiredCarModel,
	const float fNavMeshCompletionRadius,
	const bool bGoAsFarAsPossibleIfNavMeshNotLoaded,
	const bool bPreferToUsePavements,
	const bool bUseLongRangeVehiclePathing,
	const s32 iDrivingFlags,
	const float fExtraVehToTargetDistToPreferVeh,
	const float fDriveStraightLineDistance,
	const u16 iFlags,
	const float fOverrideCruiseSpeed
	)
	: m_bHasTargetVehicle(false)
	, m_bHasTargetPosition(false)
	, m_vTarget(vTarget)
	, m_fMoveBlendRatio(fMoveBlendRatio)
	, m_fTargetRadius(fTargetRadius)
	, m_fNavMeshCompletionRadius(fNavMeshCompletionRadius)
	, m_bHasDesiredCarModel(false)
	, m_iDesiredCarModel(iDesiredCarModel)
	, m_bGoAsFarAsPossibleIfNavMeshNotLoaded(bGoAsFarAsPossibleIfNavMeshNotLoaded)
	, m_bPreferToUsePavements(bPreferToUsePavements)
	, m_bUseLongRangeVehiclePathing(bUseLongRangeVehiclePathing)
	, m_iDrivingFlags(iDrivingFlags)
	, m_fExtraVehToTargetDistToPreferVeh(fExtraVehToTargetDistToPreferVeh)
	, m_fDriveStraightLineDistance(fDriveStraightLineDistance)
	, m_iFlags(iFlags)
	, m_fOverrideCruiseSpeed(fOverrideCruiseSpeed)
{
	SetStatusFromMainTaskState(iState);

	if (!m_vTarget.IsZero())
		m_bHasTargetPosition = true;

	if (pTargetVehicle)
	{
		m_bHasTargetVehicle = true;
		m_pTargetVehicle.SetEntity((CEntity*)pTargetVehicle);
	}

	Assertf(m_bHasTargetPosition || m_bHasTargetVehicle, "GOTO_ANY_MEANS must have either target position (%s) or vehicle (%s)", 
										m_bHasTargetPosition ? "True":"False", m_bHasTargetVehicle ? "True":"False");

	if (iDesiredCarModel != -1)
	{
		m_bHasDesiredCarModel = true;
	}
}

CTask* CClonedGoToPointAnyMeansInfo::CreateLocalTask(fwEntity* UNUSED_PARAM(pEntity))
{
	CVehicle* pVehicle = NULL;

	if(m_bHasTargetVehicle)
		pVehicle = (CVehicle*) m_pTargetVehicle.GetEntity();

	CTaskGoToPointAnyMeans* pTask = rage_new CTaskGoToPointAnyMeans(m_fMoveBlendRatio,m_vTarget,pVehicle,m_fTargetRadius,m_iDesiredCarModel, m_bUseLongRangeVehiclePathing, m_iDrivingFlags, -1.0f, m_fExtraVehToTargetDistToPreferVeh, m_fDriveStraightLineDistance, m_iFlags, m_fOverrideCruiseSpeed);
	pTask->SetNavMeshCompletionRadius(m_fNavMeshCompletionRadius);
	pTask->SetGoAsFarAsPossibleIfNavMeshNotLoaded(m_bGoAsFarAsPossibleIfNavMeshNotLoaded);
	pTask->SetPreferToUsePavements(m_bPreferToUsePavements);

	return pTask;
}

void CClonedGoToPointAnyMeansInfo::Serialise(CSyncDataBase& serialiser)
{
	CSerialisedFSMTaskInfo::Serialise(serialiser);

	const unsigned SIZEOF_MOVE_BLEND_RATIO = 8;
	const unsigned SIZEOF_TARGET_RADIUS = 8;
	const unsigned SIZEOF_EXTRA_DIST = 16;
	const unsigned SIZEOF_STRIGHT_LINE_DIST = 8;
	const float MAX_TARGET_RADIUS = 2.0f;
	const float MAX_EXTRA_DIST = 100000.0f;
	const float MAX_STRAIGHT_LINE_DIST = 50.0f;
	const float MAX_CRUISE_SPEED = 200.0f;
	const unsigned SIZEOF_CARMODEL = 32;
	const unsigned SIZEOF_DRIVINGFLAGS = 32;

	// only send required info if we have a target vehicle
	SERIALISE_BOOL(serialiser, m_bHasTargetVehicle, "Has Target Vehicle");
	if(m_bHasTargetVehicle || serialiser.GetIsMaximumSizeSerialiser())
	{
		ObjectId targetID = m_pTargetVehicle.GetEntityID();
		SERIALISE_OBJECTID(serialiser, targetID, "Target Vehicle");
		m_pTargetVehicle.SetEntityID(targetID);
	}

	SERIALISE_BOOL(serialiser, m_bHasTargetPosition, "Has Target Position");
	if(m_bHasTargetPosition || serialiser.GetIsMaximumSizeSerialiser())
		SERIALISE_POSITION(serialiser, m_vTarget, "Target Position");

	SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_fMoveBlendRatio, MOVEBLENDRATIO_SPRINT, SIZEOF_MOVE_BLEND_RATIO, "Move Blend Ratio");
	SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_fTargetRadius, MAX_TARGET_RADIUS, SIZEOF_TARGET_RADIUS, "Target Radius");
	SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_fNavMeshCompletionRadius, MAX_TARGET_RADIUS, SIZEOF_TARGET_RADIUS, "Nav Mesh Comp Radius");
	SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_fExtraVehToTargetDistToPreferVeh, MAX_EXTRA_DIST, SIZEOF_EXTRA_DIST, "Extra Vehicle To Target Dist To Prefer Vehicle");
	SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_fDriveStraightLineDistance, MAX_STRAIGHT_LINE_DIST, SIZEOF_STRIGHT_LINE_DIST, "Distance at which driving task will switch to straight-line navigation");

	SERIALISE_BOOL(serialiser, m_bHasDesiredCarModel, "Has Desired Car Model");
	if(m_bHasDesiredCarModel || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, m_iDesiredCarModel, SIZEOF_CARMODEL, "Desired Car Model");
	}
	else
	{
		m_iDesiredCarModel = -1;
	}

	SERIALISE_BOOL(serialiser, m_bGoAsFarAsPossibleIfNavMeshNotLoaded, "Go Far If No NavMesh");
	SERIALISE_BOOL(serialiser, m_bPreferToUsePavements, "Prefer Pavement");
	SERIALISE_BOOL(serialiser, m_bUseLongRangeVehiclePathing, "Use Longrange vehicle pathing");
	SERIALISE_INTEGER(serialiser, m_iDrivingFlags, SIZEOF_DRIVINGFLAGS, "Driving Flags");
	SERIALISE_INTEGER(serialiser, m_iFlags, 16, "Task behaviour flags");

	SERIALISE_PACKED_FLOAT(serialiser, m_fOverrideCruiseSpeed, MAX_CRUISE_SPEED, 32, "Override Cruise Speed");
}

const float CTaskGoToPointAnyMeans::ms_fTargetRadius = CTaskMoveGoToPoint::ms_fTargetRadius;
const float CTaskGoToPointAnyMeans::ms_fOnFootDistance = 50.0f;

CTaskGoToPointAnyMeans::CTaskGoToPointAnyMeans(const float fMoveBlendRatio, const Vector3& vTarget, CVehicle* pTargetVehicle, const float fTargetRadius, const int iDesiredCarModel,
												const bool bUseLongRangeVehiclePathing, const s32 iDrivingFlags, const float fMaxRangeToShootTargets, const float fExtraVehToTargetDistToPreferVeh, const float fDriveStraightLineDistance, const u16 iFlags, float fOverrideCruiseSpeed, float fWarpTimerMS, float fTargetArriveDist
)
	: m_vTarget(vTarget)
	, m_fMoveBlendRatio(fMoveBlendRatio)
	, m_fTargetRadius(fTargetRadius)
	, m_pTargetVehicle(pTargetVehicle)
	, m_iDesiredCarModel(iDesiredCarModel)
	, m_fNavMeshCompletionRadius(0.0f)
	, m_bGoAsFarAsPossibleIfNavMeshNotLoaded(false)
	, m_bPreferToUsePavements(false)
	, m_bUseLongRangeVehiclePathing(bUseLongRangeVehiclePathing)
	, m_iDrivingFlags(iDrivingFlags)
	, m_fMaxRangeToShootTargets(fMaxRangeToShootTargets)
	, m_fTimeSinceLastClosestTargetSearch(0.0f)
	, m_fTimeInTheAir(0)
	, m_fExtraVehToTargetDistToPreferVeh(fExtraVehToTargetDistToPreferVeh)
	, m_fDriveStraightLineDistance(fDriveStraightLineDistance)
	, m_iFlags(iFlags)
	, m_fOverrideCruiseSpeed(fOverrideCruiseSpeed)
	, m_fOverrideCruiseSpeedForSync(fOverrideCruiseSpeed)
	, m_fWarpTimerMS(fWarpTimerMS)
	, m_fTimeStuckMS(0.0f)
	, m_fTargetArriveDist(fTargetArriveDist)
{
	Assertf(!m_vTarget.IsZero() || m_pTargetVehicle, "GOTO_ANY_MEANS must have valid target position or vehicle");
	SetInternalTaskType(CTaskTypes::TASK_GO_TO_POINT_ANY_MEANS);
}

CTaskGoToPointAnyMeans::CTaskGoToPointAnyMeans(const CTaskGoToPointAnyMeans& other)
	: m_vTarget(other.m_vTarget)
	, m_fMoveBlendRatio(other.m_fMoveBlendRatio)
	, m_fTargetRadius(other.m_fTargetRadius)
	, m_pTargetVehicle(other.m_pTargetVehicle)
	, m_iDesiredCarModel(other.m_iDesiredCarModel)
	, m_fNavMeshCompletionRadius(other.m_fNavMeshCompletionRadius)
	, m_bGoAsFarAsPossibleIfNavMeshNotLoaded(other.m_bGoAsFarAsPossibleIfNavMeshNotLoaded)
	, m_bPreferToUsePavements(other.m_bPreferToUsePavements)
	, m_bUseLongRangeVehiclePathing(other.m_bUseLongRangeVehiclePathing)
	, m_iDrivingFlags(other.m_iDrivingFlags)
	, m_fMaxRangeToShootTargets(other.m_fMaxRangeToShootTargets)
	, m_fTimeSinceLastClosestTargetSearch(0.0f)
	, m_fTimeInTheAir(0)
	, m_fExtraVehToTargetDistToPreferVeh(other.m_fExtraVehToTargetDistToPreferVeh)
	, m_fDriveStraightLineDistance(other.m_fDriveStraightLineDistance)
	, m_iFlags(other.m_iFlags)
	, m_fOverrideCruiseSpeed(other.m_fOverrideCruiseSpeed)
	, m_fOverrideCruiseSpeedForSync(other.m_fOverrideCruiseSpeedForSync)
	, m_fWarpTimerMS(other.m_fWarpTimerMS)
	, m_fTimeStuckMS(0.0f)
	, m_fTargetArriveDist(other.m_fTargetArriveDist)
{
	SetInternalTaskType(CTaskTypes::TASK_GO_TO_POINT_ANY_MEANS);
}

CTaskGoToPointAnyMeans::~CTaskGoToPointAnyMeans()
{
}

CTaskInfo* CTaskGoToPointAnyMeans::CreateQueriableState() const
{
	return rage_new CClonedGoToPointAnyMeansInfo(GetState(),
		m_fMoveBlendRatio, 
		m_vTarget, 
		m_pTargetVehicle,
		m_fTargetRadius,
		m_iDesiredCarModel,
		m_fNavMeshCompletionRadius,
		m_bGoAsFarAsPossibleIfNavMeshNotLoaded,
		m_bPreferToUsePavements,
		m_bUseLongRangeVehiclePathing,
		m_iDrivingFlags,
		m_fExtraVehToTargetDistToPreferVeh,
		m_fDriveStraightLineDistance,
		m_iFlags,
		m_fOverrideCruiseSpeedForSync
		);
}


CTaskFSMClone* CTaskGoToPointAnyMeans::CreateTaskForClonePed(CPed* UNUSED_PARAM(pPed))
{
	CTaskGoToPointAnyMeans* pTask = rage_new CTaskGoToPointAnyMeans(m_fMoveBlendRatio, m_vTarget, m_pTargetVehicle, m_fTargetRadius, m_iDesiredCarModel, m_bUseLongRangeVehiclePathing, m_iDrivingFlags, -1.0f, m_fExtraVehToTargetDistToPreferVeh, m_fDriveStraightLineDistance, m_iFlags, m_fOverrideCruiseSpeedForSync);
	if(pTask)
	{
		pTask->SetNavMeshCompletionRadius(m_fNavMeshCompletionRadius);
		pTask->SetGoAsFarAsPossibleIfNavMeshNotLoaded(m_bGoAsFarAsPossibleIfNavMeshNotLoaded);
		pTask->SetPreferToUsePavements(m_bPreferToUsePavements);
	}
	return pTask;
}

CTaskFSMClone* CTaskGoToPointAnyMeans::CreateTaskForLocalPed(CPed* pPed)
{
	return CreateTaskForClonePed(pPed);
}

CTask::FSM_Return CTaskGoToPointAnyMeans::UpdateClonedFSM(s32 UNUSED_PARAM(iState), CTask::FSM_Event UNUSED_PARAM(iEvent))
{
	return FSM_Quit;
}

CTask::FSM_Return CTaskGoToPointAnyMeans::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin

		FSM_State(State_Initial)
			FSM_OnEnter
				return StateInitial_OnEnter(pPed);
			FSM_OnUpdate
				return StateInitial_OnUpdate(pPed);

		FSM_State(State_EnteringVehicle)
			FSM_OnEnter
				return StateEnteringVehicle_OnEnter(pPed);
			FSM_OnUpdate
				return StateEnteringVehicle_OnUpdate(pPed);

		FSM_State(State_ExitingVehicle)
			FSM_OnEnter
				return StateExitingVehicle_OnEnter(pPed);
			FSM_OnUpdate
				return StateExitingVehicle_OnUpdate(pPed);

		FSM_State(State_VehicleTakeOff)
			FSM_OnEnter
				return StateVehicleTakeOff_OnEnter(pPed);
			FSM_OnUpdate
				return StateVehicleTakeOff_OnUpdate(pPed);

		FSM_State(State_NavigateUsingVehicle)
			FSM_OnEnter
				return StateNavigateUsingVehicle_OnEnter(pPed);
			FSM_OnUpdate
				return StateNavigateUsingVehicle_OnUpdate(pPed);

		FSM_State(State_NavigateUsingNavMesh)
			FSM_OnEnter
				return StateNavigateUsingNavMesh_OnEnter(pPed);
			FSM_OnUpdate
				return StateNavigateUsingNavMesh_OnUpdate(pPed);

		FSM_State(State_WaitBetweenNavMeshTasks)
			FSM_OnEnter
				return StateWaitBetweenNavMeshTasks_OnEnter(pPed);
			FSM_OnUpdate
				return StateWaitBetweenNavMeshTasks_OnUpdate(pPed);

	FSM_End
}

CTask::FSM_Return CTaskGoToPointAnyMeans::StateInitial_OnEnter(CPed * UNUSED_PARAM(pPed))
{
	// If we want to use the targeting for our threats then we need to start by registering anybody we might attack
	if( (m_iFlags & Flag_UseAiTargetingForThreats) != 0)
	{
		CPed* pPed = GetPed();
		CPedTargetting* pPedTargeting = pPed->GetPedIntelligence()->GetTargetting(true);
		if(pPedTargeting)
		{
			FindAndRegisterThreats(pPed, pPedTargeting);
		}
	}

	return FSM_Continue;
}
CTask::FSM_Return CTaskGoToPointAnyMeans::StateInitial_OnUpdate(CPed * pPed)
{
	if(m_pTargetVehicle)
	{	
		if(pPed->GetMyVehicle() && pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
		{
			SetState(State_VehicleTakeOff);
		}
		else
		{
			// don't get back in beached boats
			if ( m_pTargetVehicle->InheritsFromBoat() && CBoatIsBeachedCheck::IsBoatBeached(*m_pTargetVehicle) )
			{
				SetState(State_NavigateUsingNavMesh);
			}
			else
			{
				SetState(State_EnteringVehicle);
			}
		}
	}
	else
	{
		if(pPed->GetMyVehicle() && pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
		{
			if (pPed->GetMyVehicle()->IsDriver(pPed))
			{
				SetState(State_VehicleTakeOff);
			}
			else
			{
				SetState(State_ExitingVehicle);
			}
		}
		else
		{
			SetState(State_NavigateUsingNavMesh);
		}
	}
	return FSM_Continue;
}

CTask::FSM_Return CTaskGoToPointAnyMeans::StateEnteringVehicle_OnEnter(CPed * UNUSED_PARAM(pPed))
{
	VehicleEnterExitFlags vehicleFlags;
	SetNewTask(rage_new CTaskEnterVehicle(const_cast<CVehicle*>(m_pTargetVehicle.Get()), SR_Specific, m_pTargetVehicle->GetDriverSeat(), vehicleFlags, 0.0f, m_fMoveBlendRatio));
	return FSM_Continue;
}
CTask::FSM_Return CTaskGoToPointAnyMeans::StateEnteringVehicle_OnUpdate(CPed * pPed)
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		if(pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && pPed->GetMyVehicle())
		{
			SetState(State_VehicleTakeOff);
		}
		else
		{
			SetState(State_NavigateUsingNavMesh);
		}
	}
	else if (!IsVehicleCloseEnoughToTargetToUse(m_pTargetVehicle, pPed))
	{
		// The vehicle is now too far away. Reset it and start over.
		m_pTargetVehicle = NULL;
		if(!m_vTarget.IsZero())
		{
			SetState(State_Initial);
		}
		else
		{
			//no longer have a target vehicle or position
			return FSM_Quit;
		}		
	}
	return FSM_Continue;
}

CTask::FSM_Return CTaskGoToPointAnyMeans::StateExitingVehicle_OnEnter(CPed * pPed)
{
	Assert(pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && pPed->GetMyVehicle());
	VehicleEnterExitFlags vehicleFlags;
	SetNewTask( rage_new CTaskExitVehicle(pPed->GetMyVehicle(), vehicleFlags) );
	return FSM_Continue;
}
CTask::FSM_Return CTaskGoToPointAnyMeans::StateExitingVehicle_OnUpdate(CPed * UNUSED_PARAM(pPed))
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_NavigateUsingNavMesh);
	}
	return FSM_Continue;
}

CTask::FSM_Return CTaskGoToPointAnyMeans::StateVehicleTakeOff_OnEnter(CPed * UNUSED_PARAM(pPed))
{

	if ( m_pTargetVehicle != NULL )
	{
		if ( m_pTargetVehicle->InheritsFromPlane() )
		{
			bool bIsOnGround = !m_pTargetVehicle->IsInAir();
			if ( !bIsOnGround )
			{
				if ( !m_pTargetVehicle->IsCollisionLoadedAroundPosition())
				{
					static spdAABB s_AirportRegion(Vec3V(-2882.0f, 3454.0f, 30.0f), Vec3V(-1663.4f, 2945.0f, 90.0f));
					Vec3V vPosition = m_pTargetVehicle->GetTransform().GetPosition();
					if ( s_AirportRegion.ContainsPoint(vPosition) )
					{
						bIsOnGround = true;
					}
				}
			}

			if ( bIsOnGround )
			{
				sVehicleMissionParams params;
				params.m_iDrivingFlags = m_iDrivingFlags;
				params.SetTargetPosition(VEC3V_TO_VECTOR3(m_pTargetVehicle->GetTransform().GetPosition() + m_pTargetVehicle->GetTransform().GetForward() * ScalarV(10000.0f)));
				
				if(m_fOverrideCruiseSpeed > 0.0f)
				{
					params.m_fCruiseSpeed = m_fOverrideCruiseSpeed;
					m_fOverrideCruiseSpeed = -1.0f; 
				}
				else
				{
					params.m_fCruiseSpeed = (CPedMotionData::GetIsRunning(m_fMoveBlendRatio) || CPedMotionData::GetIsSprinting(m_fMoveBlendRatio)) ? 35.0f : 25.0f;
				}

				// arrival distance = -1.0f leads to circling
				params.m_fTargetArriveDist = -1.0f;
				SetNewTask( rage_new CTaskControlVehicle(const_cast<CVehicle*>(m_pTargetVehicle.Get()), rage_new CTaskVehicleGoToPlane(params) ) );
			}
		}
	}
	
	return FSM_Continue;
}

CTask::FSM_Return CTaskGoToPointAnyMeans::StateVehicleTakeOff_OnUpdate(CPed * UNUSED_PARAM(pPed))
{
	if(!GetIsFlagSet(aiTaskFlags::SubTaskFinished) && GetSubTask() )
	{
		if ( m_pTargetVehicle != NULL )
		{
			if ( m_pTargetVehicle->InheritsFromPlane() )
			{		
				static u32 s_RetractLandingGearTimer = 1000;
				static float s_MaxTimeInTheAir = 7.0f;
				static float s_OffsetDistance = 1000.0f;
				static float s_OffsetSlope = 0.125f;
				static float s_TakeOffSpeed = 22.0f;
				static float s_TakeOffHeight = 30.0f;
				

				Vector3 position = VEC3V_TO_VECTOR3(m_pTargetVehicle->GetTransform().GetPosition());
				Vector3 forward = VEC3V_TO_VECTOR3(m_pTargetVehicle->GetTransform().GetForward());
				float targetZ = CGameWorldHeightMap::GetMaxHeightFromWorldHeightMap(position.x, position.y) + s_TakeOffHeight;

				CPlane* pPlane = static_cast<CPlane*>(m_pTargetVehicle.Get());
				pPlane->GetPlaneIntelligence()->SetRetractLandingGearTime(fwTimer::GetTimeInMilliseconds() + s_RetractLandingGearTimer );

				if ( pPlane->IsInAir() )
				{
					m_fTimeInTheAir += fwTimer::GetTimeStep();
				}
				else
				{
					m_fTimeInTheAir = 0;
				}

				if ( m_fTimeInTheAir < s_MaxTimeInTheAir )
				{
					if ( position.z < targetZ)
					{
						if ( forward.XYMag2() >= 0.001f )
						{
							CTaskVehicleGoToPlane* pTaskVehicleGoToPlane = static_cast<CTaskVehicleGoToPlane*>(m_pTargetVehicle->GetIntelligence()->GetActiveTask());
							if(pTaskVehicleGoToPlane)
							{
								if ( m_pTargetVehicle->GetAiXYSpeed() > s_TakeOffSpeed )
								{
									forward.z = 0.0f;
									forward.Normalize();
									Vector3 offset = forward * s_OffsetDistance;
									offset.z = s_OffsetDistance * s_OffsetSlope;
									Vector3 target = position + offset;
									pTaskVehicleGoToPlane->SetTargetPosition(&target);
								}
							}
							return FSM_Continue;
						}
					}
				}
			}
		}
	}
	SetState(State_NavigateUsingVehicle);
	return FSM_Continue;
}

CTask::FSM_Return CTaskGoToPointAnyMeans::StateNavigateUsingVehicle_OnEnter(CPed * UNUSED_PARAM(pPed))
{
	// Default speed is 12 m/sec, but if the ped is in a hurry they drive at 25 m/sec

	sVehicleMissionParams params;
	params.m_iDrivingFlags = m_iDrivingFlags;
	params.SetTargetPosition(m_vTarget);

	if (m_fTargetArriveDist < 0.0f)
	{
		m_fTargetArriveDist = sVehicleMissionParams::DEFAULT_TARGET_REACHED_DIST;
	}
	params.m_fTargetArriveDist = m_fTargetArriveDist;

	bool bOverridingCruiseSpeed = false;
	if(m_fOverrideCruiseSpeed > 0.0f)
	{
		params.m_fCruiseSpeed = m_fOverrideCruiseSpeed;
		m_fOverrideCruiseSpeed = -1.0f; 
		bOverridingCruiseSpeed = true;
	}

	aiTask* pTask = NULL;
	if ( m_pTargetVehicle != NULL )
	{
		if ( m_pTargetVehicle->InheritsFromHeli() )
		{
			if(!bOverridingCruiseSpeed)
				params.m_fCruiseSpeed = (CPedMotionData::GetIsRunning(m_fMoveBlendRatio) || CPedMotionData::GetIsSprinting(m_fMoveBlendRatio)) ? 30.0f : 20.0f;				
			
			// arrival distance = -1.0f leads to hovering
			params.m_fTargetArriveDist = -1.0f;
			pTask = rage_new CTaskVehicleGoToHelicopter(params);
		}
		else if ( m_pTargetVehicle->InheritsFromPlane() )
		{
			if(!bOverridingCruiseSpeed)
				params.m_fCruiseSpeed = (CPedMotionData::GetIsRunning(m_fMoveBlendRatio) || CPedMotionData::GetIsSprinting(m_fMoveBlendRatio)) ? 35.0f : 25.0f;

			// arrival distance = -1.0f leads to circling
			params.m_fTargetArriveDist = -1.0f;
			pTask = rage_new CTaskVehicleGoToPlane(params);
		}
		else if ( m_pTargetVehicle->InheritsFromBoat() )
		{			
			CTaskVehicleGoToBoat::ConfigFlags uConfigFlags = CTaskVehicleGoToBoat::CF_StopAtEnd | CTaskVehicleGoToBoat::CF_StopAtShore | CTaskVehicleGoToBoat::CF_AvoidShore | CTaskVehicleGoToBoat::CF_PreferForward | CTaskVehicleGoToBoat::CF_NeverNavMesh | CTaskVehicleGoToBoat::CF_NeverPause;
			
			if(!bOverridingCruiseSpeed)
				params.m_fCruiseSpeed = (CPedMotionData::GetIsRunning(m_fMoveBlendRatio) || CPedMotionData::GetIsSprinting(m_fMoveBlendRatio)) ? 25.0f : 12.0f;

			pTask = rage_new CTaskVehicleGoToBoat(params, uConfigFlags);
		}
	}

	if ( !pTask )
	{
		if(!bOverridingCruiseSpeed)
			params.m_fCruiseSpeed = (CPedMotionData::GetIsRunning(m_fMoveBlendRatio) || CPedMotionData::GetIsSprinting(m_fMoveBlendRatio)) ? 25.0f : 12.0f;

		if (m_bUseLongRangeVehiclePathing)
		{
			pTask = rage_new CTaskVehicleGotoLongRange(params);
		}
		else
		{
			pTask = CVehicleIntelligence::CreateAutomobileGotoTask(params, m_fDriveStraightLineDistance);
		}
	}
	
	SetNewTask( rage_new CTaskControlVehicle(const_cast<CVehicle*>(m_pTargetVehicle.Get()), pTask) );

	return FSM_Continue;
}
CTask::FSM_Return CTaskGoToPointAnyMeans::StateNavigateUsingVehicle_OnUpdate(CPed * pPed)
{
	if (ShouldAbandonVehicle(pPed))
	{
		SetState(State_ExitingVehicle);
	}
	else if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		const CVehicle* pVehicle = pPed->GetVehiclePedInside();
		if(pVehicle)
		{
			if((m_iFlags & Flag_RemainInVehicleAtDestination)!=0)
			{
				return FSM_Quit;
			}
			else
			{
				SetState(State_ExitingVehicle);
			}
		}
		else
		{
			SetState(State_NavigateUsingNavMesh);
		}
	}

	WarpPedIfStuck(pPed);

	return FSM_Continue;
}

bool CTaskGoToPointAnyMeans::ShouldAbandonVehicle(const CPed * pPed) const
{
	if (((m_iFlags & Flag_NeverAbandonVehicle) != 0) && ((m_iFlags & Flag_NeverAbandonVehicleIfMoving) == 0))
	{
		return false;
	}

	bool bShouldAbandonVehicle = false;

	const CVehicle* pVehicle = pPed->GetVehiclePedInside();
	if(pVehicle)
	{
		bShouldAbandonVehicle = CVehicleUndriveableHelper::IsUndriveable(*pVehicle) ||
			(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_GoOnWithoutVehicleIfItIsUnableToGetBackToRoad) && IsControlledVehicleUnableToGetToRoad());

		if (((m_iFlags & Flag_NeverAbandonVehicleIfMoving) != 0) && pVehicle->GetVelocity().Mag() > 0.01f)
		{
			bShouldAbandonVehicle = false;
		}
	}

	return bShouldAbandonVehicle;
}


bool CTaskGoToPointAnyMeans::IsControlledVehicleUnableToGetToRoad() const
{
	const CTask* pSubTask = GetSubTask();
	if (pSubTask && pSubTask->GetTaskType() == CTaskTypes::TASK_CONTROL_VEHICLE)
	{
		const CTaskControlVehicle* pTaskControlVehicle = SafeCast(const CTaskControlVehicle, pSubTask);
		return pTaskControlVehicle->IsVehicleTaskUnableToGetToRoad();
	}

	return false;
}

void CTaskGoToPointAnyMeans::WarpPedIfStuck(CPed * pPed)
{
	// B*2169274: Warp timer set by TASK_GO_TO_COORD_ANY_MEANS_EXTRA_PARAMS. Warps ped to destination once timer expires.
	if (m_fWarpTimerMS != -1)
	{
		bool bIsUnableToFindRoute = false;

		// In vehicle: only warp for cars/bikes as we don't currently have a way to check if aircraft/boats are stuck.
		if (pPed->GetIsInVehicle() && pPed->GetVehiclePedInside() && (pPed->GetVehiclePedInside()->InheritsFromAutomobile() || pPed->GetVehiclePedInside()->InheritsFromBike())
			&& !pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_VEHICLE_GOTO_NAVMESH))
		{
			bIsUnableToFindRoute = true;
		}
		else
		{
			CTask * pTaskMove = ((CTaskComplexControlMovement*)GetSubTask())->GetRunningMovementTask(pPed);
			if(pTaskMove && pTaskMove->GetTaskType()==CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH && ((CTaskMoveFollowNavMesh*)pTaskMove)->IsUnableToFindRoute())
			{
				bIsUnableToFindRoute = true;
			}
		}

		if(bIsUnableToFindRoute)
		{
			// We've been stuck for more than the specified time, warp to the destination.
			if (m_fTimeStuckMS >= m_fWarpTimerMS)
			{
				// Remove ped from vehicle if necessary.
				if (pPed->GetIsInVehicle())
				{
					pPed->SetPedOutOfVehicle();
				}

				pPed->Teleport(m_vTarget, pPed->GetDesiredHeading(), true);
			}

			m_fTimeStuckMS += fwTimer::GetTimeStepInMilliseconds();
		}
		else
		{
			m_fTimeStuckMS = 0.0f;
		}
	}
}

CTask::FSM_Return CTaskGoToPointAnyMeans::StateNavigateUsingNavMesh_OnEnter(CPed * UNUSED_PARAM(pPed))
{
	CTaskMoveFollowNavMesh * pNavMeshTask = rage_new CTaskMoveFollowNavMesh(
		m_fMoveBlendRatio, m_vTarget, m_fTargetRadius,
		CTaskMoveFollowNavMesh::ms_fSlowDownDistance,
		-1, true, false,
		NULL, m_fNavMeshCompletionRadius, true
	);
	pNavMeshTask->SetGoAsFarAsPossibleIfNavMeshNotLoaded(m_bGoAsFarAsPossibleIfNavMeshNotLoaded);
	pNavMeshTask->SetKeepToPavements(m_bPreferToUsePavements);
	
	CTaskCombatAdditionalTask* pCombatTask = NULL;
	CPed* pClosestTarget = FindBestTargetPed();
	if(pClosestTarget)
	{
		pCombatTask = rage_new CTaskCombatAdditionalTask(CTaskCombatAdditionalTask::RT_Default, pClosestTarget, VEC3V_TO_VECTOR3(pClosestTarget->GetTransform().GetPosition()));

		CPed* pPed = GetPed();
		taskAssert(pPed);

		OnRespondedToTarget(*pPed, pClosestTarget);
	}
	SetNewTask( rage_new CTaskComplexControlMovement(pNavMeshTask, pCombatTask) );

	return FSM_Continue;
}
CTask::FSM_Return CTaskGoToPointAnyMeans::StateNavigateUsingNavMesh_OnUpdate(CPed * pPed)
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//***********************************************************************************
		// Although the navmesh task has finished, it may not actually be at the target if
		// the m_bGoAsFarAsPossibleIfNavMeshNotLoaded flag was set.  Test for completion,
		// and restart the navigation after a short pause if we are still not at target.

		const float fNearbyDistSqr = 3.0f*3.0f;
		Vector3 vDiff = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) - m_vTarget;
		if(rage::Abs(vDiff.z) < 3.0f && vDiff.XYMag2() < fNearbyDistSqr)
		{
			return FSM_Quit;
		}
		else
		{
			SetState(State_WaitBetweenNavMeshTasks);
		}
	}
	else
	{
		UpdateFiringSubTask();

		//****************************************************************
		// Whilst navigation on-foot we can choose to steal a nearby car

		const bool bDoProperDriveableCheck = ((m_iFlags & Flag_ProperDriveableCheck)!=0);

		const Vector3 vDiff = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) - m_vTarget;
		if(vDiff.Mag2() > (ms_fOnFootDistance*ms_fOnFootDistance))
		{
			// If 'Flag_ConsiderAllNearbyVehicles' is specified we test all the ped's nearby vehicles
			if( (m_iFlags & Flag_ConsiderAllNearbyVehicles) != 0)
			{
				CEntityScannerIterator vehiclesList = pPed->GetPedIntelligence()->GetNearbyVehicles();
				for( CVehicle* pNearestVehicle = (CVehicle*)vehiclesList.GetFirst(); pNearestVehicle; pNearestVehicle = (CVehicle*)vehiclesList.GetNext() )
				{
					if(pNearestVehicle && pNearestVehicle->GetIsVisible() && pNearestVehicle!=m_pTargetVehicle)
					{
						if (IsVehicleCloseEnoughToTargetToUse(pNearestVehicle, pPed))
						{
							if(CCarEnterExit::IsVehicleStealable(*pNearestVehicle, *pPed, true, ((m_iFlags&Flag_IgnoreVehicleHealth)!=0) ))
							{
								if( !bDoProperDriveableCheck || pNearestVehicle->m_nVehicleFlags.bIsThisADriveableCar )
								{
									m_pTargetVehicle = pNearestVehicle;
									SetState(State_EnteringVehicle);
									break;
								}
							}
						}
					}
				}
			}
			// Otherwise the default behaviour is to test only the closest vehicle to the ped
			else
			{
				CVehicle * pNearestVehicle=pPed->GetPedIntelligence()->GetClosestVehicleInRange();
				if(pNearestVehicle && pNearestVehicle->GetIsVisible() && pNearestVehicle!=m_pTargetVehicle)
				{
					if (IsVehicleCloseEnoughToTargetToUse(pNearestVehicle, pPed))
					{
						if(CCarEnterExit::IsVehicleStealable(*pNearestVehicle, *pPed, true, ((m_iFlags&Flag_IgnoreVehicleHealth)!=0) ))
						{
							if( !bDoProperDriveableCheck || pNearestVehicle->m_nVehicleFlags.bIsThisADriveableCar )
							{
								m_pTargetVehicle = pNearestVehicle;
								SetState(State_EnteringVehicle);
							}
						}
					}
				}
			}
		}
	}

	WarpPedIfStuck(pPed);

	return FSM_Continue;
}
CTask::FSM_Return CTaskGoToPointAnyMeans::StateWaitBetweenNavMeshTasks_OnEnter(CPed * UNUSED_PARAM(pPed))
{
	SetNewTask( rage_new CTaskDoNothing(5000) );
	return FSM_Continue;
}
CTask::FSM_Return CTaskGoToPointAnyMeans::StateWaitBetweenNavMeshTasks_OnUpdate(CPed * UNUSED_PARAM(pPed))
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_NavigateUsingNavMesh);
	}
	return FSM_Continue;
}

CPed* CTaskGoToPointAnyMeans::FindBestTargetPed()
{
	ScalarV scMaxDistSq = LoadScalar32IntoScalarV(square(m_fMaxRangeToShootTargets));
	CPed* pPed = GetPed();
	Vec3V vMyPosition = pPed->GetTransform().GetPosition();

	// If we want to use the targeting then go ahead and return our best target if we have one
	if( (m_iFlags & Flag_UseAiTargetingForThreats) != 0)
	{
		CPedTargetting* pPedTargeting = pPed->GetPedIntelligence()->GetTargetting(true);
		if(pPedTargeting)
		{
			// See if we have a best target to return
			CPed* pBestTarget = pPedTargeting->GetBestTarget();
			if(pBestTarget)
			{
				return pBestTarget;
			}
			else
			{
				// If we don't have a threat we will use the logic below in a helper to find and register threats in the desired range
				FindAndRegisterThreats(pPed, pPedTargeting);
			}
		}
	}
	else
	{
		CEntityScannerIterator entityList = pPed->GetPedIntelligence()->GetNearbyPeds();
		for( CEntity* pEnt = entityList.GetFirst(); pEnt; pEnt = entityList.GetNext() )
		{
			CPed* pNearbyPed = static_cast<CPed*>(pEnt);
			if(pNearbyPed)
			{
				ScalarV scDistSq = DistSquared(vMyPosition, pNearbyPed->GetTransform().GetPosition());
				if(IsGreaterThanAll(scDistSq, scMaxDistSq))
				{
					return NULL;
				}

				if( !pNearbyPed->IsInjured() &&
					pPed->GetPedIntelligence()->IsThreatenedBy(*pNearbyPed) && 
					pPed->GetPedIntelligence()->CanAttackPed(pNearbyPed) )
				{
					return pNearbyPed;
				}
			}
		}
	}

	return NULL;
}

bool SortSpatialArrayThreats(const CSpatialArray::FindResult& a, const CSpatialArray::FindResult& b)
{
	return PositiveFloatLessThan_Unsafe(a.m_DistanceSq, b.m_DistanceSq);
}

void CTaskGoToPointAnyMeans::FindAndRegisterThreats(CPed* pPed, CPedTargetting* pPedTargeting)
{
	if(pPedTargeting && pPed)
	{
		// Use the spatial array to get our peds within the radius and then sort them by distance
		const int maxNumPeds = 128;
		CSpatialArray::FindResult results[maxNumPeds];

		int numFound = CPed::ms_spatialArray->FindInSphere(pPed->GetTransform().GetPosition(), m_fMaxRangeToShootTargets, &results[0], maxNumPeds);
		if(numFound)
		{
			std::sort(results, results + numFound, SortSpatialArrayThreats);
		}

		for(u32 i = 0; i < numFound; i++)
		{
			CPed* pTargetPed = CPed::GetPedFromSpatialArrayNode(results[i].m_Node);
			if (pTargetPed != pPed)
			{
				if( !pTargetPed->IsInjured() &&
					pPed->GetPedIntelligence()->IsThreatenedBy(*pTargetPed) && 
					pPed->GetPedIntelligence()->CanAttackPed(pTargetPed) )
				{
					pPedTargeting->RegisterThreat(pTargetPed, true, true);
				}
			}
		}
	}
}

void CTaskGoToPointAnyMeans::UpdateFiringSubTask()
{
	if(m_fMaxRangeToShootTargets <= 0.0f)
	{
		return;
	}

	static dev_float s_fTimeBetweenUpdates = 1.5f;
	m_fTimeSinceLastClosestTargetSearch += GetTimeStep();
	if(m_fTimeSinceLastClosestTargetSearch < s_fTimeBetweenUpdates)
	{
		return;
	}

	m_fTimeSinceLastClosestTargetSearch = 0.0f;

	CTask* pMainSubTask = GetSubTask();
	if(pMainSubTask && pMainSubTask->GetTaskType() == CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT)
	{
		CPed* pPed = GetPed();
		CPed* pClosestTarget = NULL;

		CTaskComplexControlMovement* pSubTask = static_cast<CTaskComplexControlMovement*>(pMainSubTask);
		if(pSubTask->GetMainSubTaskType() == CTaskTypes::TASK_ADDITIONAL_COMBAT_TASK)
		{
			CTaskCombatAdditionalTask* pFireTask = static_cast<CTaskCombatAdditionalTask*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ADDITIONAL_COMBAT_TASK));
			if(pFireTask)
			{
				pClosestTarget = FindBestTargetPed();
				if(!pClosestTarget)
				{
					pSubTask->SetNewMainSubtask(NULL);
				}
				else if(pFireTask->GetTarget() != pClosestTarget)
				{
					pSubTask->SetNewMainSubtask(rage_new CTaskCombatAdditionalTask(CTaskCombatAdditionalTask::RT_Default, pClosestTarget, VEC3V_TO_VECTOR3(pClosestTarget->GetTransform().GetPosition())));
				}
			}
		}
		else if(pSubTask->GetMainSubTaskType() == CTaskTypes::TASK_NONE)
		{
			pClosestTarget = FindBestTargetPed();
			if(pClosestTarget)
			{
				pSubTask->SetNewMainSubtask(rage_new CTaskCombatAdditionalTask(CTaskCombatAdditionalTask::RT_Default, pClosestTarget, VEC3V_TO_VECTOR3(pClosestTarget->GetTransform().GetPosition())));
			}
		}

		if (pClosestTarget)
		{
			taskAssert(pPed);
			OnRespondedToTarget(*pPed, pClosestTarget);
		}
	}
}

void CTaskGoToPointAnyMeans::OnRespondedToTarget(CPed& rPed, CPed* pTarget)
{
	if (pTarget && rPed.GetPedConfigFlag(CPED_CONFIG_FLAG_BroadcastRepondedToThreatWhenGoingToPointShooting))
	{
		if (CTaskThreatResponse::ShouldPedBroadcastRespondedToThreatEvent(rPed))
		{
			CTaskThreatResponse::BroadcastPedRespondedToThreatEvent(rPed, pTarget);
		}

		rPed.SetPedConfigFlag(CPED_CONFIG_FLAG_BroadcastRepondedToThreatWhenGoingToPointShooting, false);
	}
}

bool CTaskGoToPointAnyMeans::IsVehicleCloseEnoughToTargetToUse(CVehicle* pVehicle, CPed* pPed) const
{
	TUNE_GROUP_FLOAT(VEHICLE_DEBUG, ALWAYS_TAKE_VEHICLE_CUTOFF_DIST, 200.0f, 0.0f, 1000.0f, 1.0f);

	if (pVehicle && pPed)
	{
		const Vector3 vPedToTarget = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) - m_vTarget;

		// url:bugstar:1601270 - Ped set up with an initial go to stood still (MP only fix for GTAV, but should probably be in SP too)
		if(NetworkInterface::IsGameInProgress())
		{
			if(vPedToTarget.Mag2() > ALWAYS_TAKE_VEHICLE_CUTOFF_DIST*ALWAYS_TAKE_VEHICLE_CUTOFF_DIST)
				return true;
		}

		// url:bugstar:1204308 - only take a vehicle if doing so doesn't take us too far away from the target
		const Vector3 vVehicleToTarget = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition()) - m_vTarget;
		const float fDistPedToTarget = vPedToTarget.Mag();
		const float fDistVehicleToTarget = vVehicleToTarget.Mag();

		// Calculate an acceptable detour as the sqrt of the distance from the ped to the target (ie. at 100m, we may go 10m out our way.  15m at 200m, etc)
		const float fDetourDist = Sqrtf(fDistPedToTarget);

		return (fDistVehicleToTarget + m_fExtraVehToTargetDistToPreferVeh < fDistPedToTarget + fDetourDist);
	}
	return false;
}



//**********************************
// CTaskFollowPatrolRoute
//

const float CTaskFollowPatrolRoute::ms_fTargetRadius=0.5f;
const float CTaskFollowPatrolRoute::ms_fSlowDownDistance=5.0f;
CPatrolRoute CTaskFollowPatrolRoute::ms_patrolRoute;

CTaskFollowPatrolRoute::CTaskFollowPatrolRoute(const float fMoveBlendRatio, const CPatrolRoute& route, const int iMode, const float fTargetRadius, const float fSlowDownDistance)
	: m_iMode(iMode),
	m_fMoveBlendRatio(fMoveBlendRatio),
	m_bNewRoute(false),
	m_bWasAborted(false)
{
	m_pRoute = rage_new CPatrolRoute;
    SetRoute(route, fTargetRadius, fSlowDownDistance, true);
	SetInternalTaskType(CTaskTypes::TASK_FOLLOW_PATROL_ROUTE);
}
CTaskFollowPatrolRoute::~CTaskFollowPatrolRoute()
{
	if(m_pRoute)
		delete m_pRoute;
}
void CTaskFollowPatrolRoute::DoAbort(const AbortPriority UNUSED_PARAM(iPriority), const aiEvent* UNUSED_PARAM(pEvent))
{
	CPed *pPed = GetPed(); //Get the ped ptr.

    m_PedPositionWhenAborted = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	m_bWasAborted = true;
}

#if !__FINAL
void CTaskFollowPatrolRoute::Debug() const
{
#if DEBUG_DRAW
    grcDebugDraw::Line(m_vStartPos,m_pRoute->Get(0),Color32(0x00, 0xff, 0x00, 0xff));
    int i;
    const int N=m_pRoute->GetSize();
    for(i=0;i<N-1;i++)
    {
        grcDebugDraw::Line(m_pRoute->Get(i),m_pRoute->Get(i+1),Color32(0x00, 0xff, 0x00, 0xff));
    }
#endif
}
#endif

CTask::FSM_Return CTaskFollowPatrolRoute::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin
		FSM_State(State_Initial)
			FSM_OnEnter
				return StateInitial_OnEnter(pPed);
			FSM_OnUpdate
				return StateInitial_OnUpdate(pPed);
		FSM_State(State_FollowingPatrolRoute)
			FSM_OnEnter
				return StateFollowingPatrolRoute_OnEnter(pPed);
			FSM_OnUpdate
				return StateFollowingPatrolRoute_OnUpdate(pPed);
		FSM_State(State_NavMeshBackToRoute)
			FSM_OnEnter
				return StateNavMeshBackToRoute_OnEnter(pPed);
			FSM_OnUpdate
				return StateNavMeshBackToRoute_OnUpdate(pPed);
		FSM_State(State_PlayingClip)
			FSM_OnEnter
				return StatePlayingClip_OnEnter(pPed);
			FSM_OnUpdate
				return StatePlayingClip_OnUpdate(pPed);
	FSM_End
}

CTask::FSM_Return CTaskFollowPatrolRoute::StateInitial_OnEnter(CPed * UNUSED_PARAM(pPed))
{
	//Route has zero size so return a task that will instantly finish.
    if(m_pRoute->GetSize()==0)
    {
		return FSM_Quit;
	}
	return FSM_Continue;
}

CTask::FSM_Return CTaskFollowPatrolRoute::StateInitial_OnUpdate(CPed * pPed)
{
#if !__FINAL
    m_vStartPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
#endif

	Assert(m_iProgress==0);

    m_bNewRoute = false;
	int iBestProgress = -1;
	float fMinDist = FLT_MAX;
	
	// Work out the line that is closest to the ped
	// and use the first point in the line as the start progress.
	int i;
	const Vector3 vPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	const int N = m_pRoute->GetSize();
	for(i=0; i<N; i++)
	{
		const int i0 = (i+0)%N;
		const int i1 = (i+1)%N;
		const Vector3 & v0 = m_pRoute->Get(i0);
		const Vector3 & v1 = m_pRoute->Get(i1);
		Vector3 w = v1-v0;
		const float l = w.Mag();
		w.Normalize();

		Vector3 vDiff = vPos-v0;
		const float t = DotProduct(vDiff,w);

		if(t > 0.0f && t < l)
		{
			Vector3 p = w;
			p *= t;
			p += v0;
			vDiff = vPos-p;
			const float f2 = vDiff.Mag2();
			if(f2 < fMinDist*fMinDist)
			{
				iBestProgress = i1;
				fMinDist = f2;
			}
		}			
	}
	
	// If we didn't find a suitable line then just head towards the nearest point. 
	fMinDist = FLT_MAX;
	if(iBestProgress == -1)
	{
		for(i=0; i<N; i++)
		{
			Vector3 vDiff;
			vDiff = vPos - m_pRoute->Get(i);
			const float f2 = vDiff.Mag2();	
			if(f2 < fMinDist*fMinDist)
			{
				iBestProgress = i;
				fMinDist = f2;
			}
		}
	}
	
	Assert(m_iProgress >= 0);
	m_iProgress = iBestProgress;
	
	// If we were aborted previously, then follow a navmesh route to our next waypoint
	// and then resume following the patrol route
	if(m_bWasAborted)
	{
		m_bWasAborted = false;
		SetState(State_NavMeshBackToRoute);
	}
	else
	{
		m_bWasAborted = false;
		SetState(State_FollowingPatrolRoute);
	}
	return FSM_Continue;
}

CTask::FSM_Return CTaskFollowPatrolRoute::StateFollowingPatrolRoute_OnEnter(CPed * UNUSED_PARAM(pPed))
{
	const int iSubTaskType = GetSubTaskType();
	aiTask * pNewTask = CreateSubTask(iSubTaskType);
	if(!pNewTask)
	{
		return FSM_Quit;
	}
	SetNewTask(pNewTask);

	return FSM_Continue;
}
CTask::FSM_Return CTaskFollowPatrolRoute::StateFollowingPatrolRoute_OnUpdate(CPed * UNUSED_PARAM(pPed))
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		// Play an clip at the end of this route segment?
		if(m_iProgress < m_pRoute->GetSize() && !m_pRoute->GetAnim(m_iProgress).IsEmpty())
		{
			SetState(State_PlayingClip);
			return FSM_Continue;
		}

		m_iProgress++;

		const int iSubTaskType = GetSubTaskType();
		aiTask * pNewTask = CreateSubTask(iSubTaskType);
		if(!pNewTask)
		{
			return FSM_Quit;
		}

		SetNewTask(pNewTask);
		return FSM_Continue;
	}

    if(m_bNewRoute || m_bWasAborted)
    {
    	m_iProgress = 0;
        SetState(State_Initial);
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskFollowPatrolRoute::StatePlayingClip_OnEnter(CPed * UNUSED_PARAM(pPed))
{
	const CAnimNameDescriptor & desc = m_pRoute->GetAnim(m_iProgress);
	u32 iFlags = 0;
	iFlags |= APF_ISFINISHAUTOREMOVE;

	Assertf(desc.GetDictName().IsNotNull(), "Null clip dictionary name\n");
	Assertf(desc.GetName(), "Null clip name\n");

	s32 clipDictIndex = -1;
	if (desc.GetDictName().IsNotNull())
	{
		clipDictIndex = fwAnimManager::FindSlot(desc.GetDictName()).Get();
	}

	atHashWithStringNotFinal clipHashKey;
	if (desc.GetName())
	{
		clipHashKey = atStringHash(desc.GetName());
	}

	Assert((clipDictIndex != -1) && (clipHashKey.IsNotNull()));
	Assertf(fwAnimManager::GetClipIfExistsByDictIndex(clipDictIndex, clipHashKey.GetHash()), "Missing clip (clip dict index, clip hash key): %d, %s %u\n", clipDictIndex, clipHashKey.TryGetCStr() ? clipHashKey.TryGetCStr() : "(null)", clipHashKey.GetHash());

	CTask * pTask = rage_new CTaskRunClip(
								clipDictIndex, 
								clipHashKey.GetHash(), 
								SLOW_BLEND_IN_DELTA,
								AP_MEDIUM, 
								iFlags, 
								BONEMASK_ALL, -1, 0);
	SetNewTask(pTask);

	return FSM_Continue;
}
CTask::FSM_Return CTaskFollowPatrolRoute::StatePlayingClip_OnUpdate(CPed * UNUSED_PARAM(pPed))
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		m_iProgress++;
		SetState(State_FollowingPatrolRoute);
	}
	return FSM_Continue;
}

CTask::FSM_Return CTaskFollowPatrolRoute::StateNavMeshBackToRoute_OnEnter(CPed * UNUSED_PARAM(pPed))
{
	if(m_pRoute->GetSize() == 0)
	{
		return FSM_Quit;
	}
	else 
	{
		if(m_iProgress == m_pRoute->GetSize())
		{
			m_iProgress--;
		}
		CTask * pMoveTask = rage_new CTaskMoveFollowNavMesh(m_fMoveBlendRatio, m_pRoute->Get(m_iProgress));
		SetNewTask(rage_new CTaskComplexControlMovement(pMoveTask));
		
		return FSM_Continue;
	}
}
CTask::FSM_Return CTaskFollowPatrolRoute::StateNavMeshBackToRoute_OnUpdate(CPed * UNUSED_PARAM(pPed))
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_FollowingPatrolRoute);
	}
	return FSM_Continue;
}

void CTaskFollowPatrolRoute::SetRoute(const CPatrolRoute& route, const float fTargetRadius, const float fSlowDownDistance, const bool bForce)
{
    if(bForce || (*m_pRoute!=route) || (m_fTargetRadius!=fTargetRadius) || (m_fSlowDownDistance!=fSlowDownDistance))
    {
        *m_pRoute=route;
        m_fTargetRadius=fTargetRadius;
        m_fSlowDownDistance=fSlowDownDistance;
        m_iProgress=0;
        m_iRouteTraversals=0;
        m_bNewRoute=true;
    }
}

int CTaskFollowPatrolRoute::GetSubTaskType()
{
    int iSubTaskType = CTaskTypes::TASK_FINISHED;

    if(m_pRoute->GetSize()==0)
    {
       iSubTaskType = CTaskTypes::TASK_FINISHED; 
    }
    else if(m_iProgress+1 < m_pRoute->GetSize())
    {
        iSubTaskType = CTaskTypes::TASK_MOVE_GO_TO_POINT;
    }
    else if(m_iProgress+1 == m_pRoute->GetSize())
    {
        iSubTaskType = CTaskTypes::TASK_MOVE_GO_TO_POINT_AND_STAND_STILL;
    }
    else if(m_iProgress == m_pRoute->GetSize())
    {       
		m_iRouteTraversals++;

    	switch(m_iMode)
    	{    		
    		case TICKET_SINGLE:
   		        iSubTaskType=CTaskTypes::TASK_FINISHED;
				break;
    		case TICKET_RETURN:
    			if(m_iRouteTraversals == 1)
    			{
    				m_pRoute->Reverse();
    				m_iProgress = 0;
    				iSubTaskType = GetSubTaskType();
    			}
    			else
    			{
			        iSubTaskType = CTaskTypes::TASK_FINISHED;
    			}
    			break;
    		case TICKET_SEASON:    		
   				m_pRoute->Reverse();
   				m_iProgress = 0;
   				iSubTaskType = GetSubTaskType();
    			break;
			case TICKET_LOOP:
				m_iProgress = 0;
   				iSubTaskType = GetSubTaskType();
				break;
			default:
				Assert(false);
				break;
    	}
    }
    else
    {
        Assert(false);
    }
    return iSubTaskType;
}

aiTask* CTaskFollowPatrolRoute::CreateSubTask(const int iTaskType)
{
    switch(iTaskType)
    {
		case CTaskTypes::TASK_FINISHED:
		{
			return NULL;
		}
        case CTaskTypes::TASK_MOVE_GO_TO_POINT:
		{
            CTask * pMoveTask = rage_new CTaskMoveGoToPoint(m_fMoveBlendRatio, m_pRoute->Get(m_iProgress), m_fTargetRadius);
#ifdef RECORD_GOTO_POINT_TASKS
			CTaskCounter::GetInstance()->RecordGoToTask(atString("CTaskFollowPatrolRoute::CreateSubTask"), pMoveTask);
#endif
			return rage_new CTaskComplexControlMovement(pMoveTask);
		}    
        case CTaskTypes::TASK_MOVE_GO_TO_POINT_AND_STAND_STILL:
		{
			float fSlowDownDistance=m_fSlowDownDistance;
			switch(m_iMode)
			{
				case TICKET_SINGLE:
					break;
				case TICKET_RETURN:
					if(m_iRouteTraversals == 0)
						fSlowDownDistance = 0;
					break;
				case TICKET_SEASON:
					fSlowDownDistance = 0;
					break;
				case TICKET_LOOP:
					fSlowDownDistance = 0;
					break;
				default:
					Assert(false);
					break;
			}
			CTask * pMoveTask = rage_new CTaskMoveGoToPointAndStandStill(m_fMoveBlendRatio, m_pRoute->Get(m_iProgress), m_fTargetRadius, fSlowDownDistance);
			return rage_new CTaskComplexControlMovement(pMoveTask);
		}
        default:
		{
            Assert(false);
            return NULL;
		}
    } 
}    


//****************************************************************************
// CTaskComplexGetOutOfWater
// Will attempt to get the ped out of the water to closest position on land.
// This is done via an initial navmesh flood-fill to identity a position, and
// then a navmesh route task.

const int CTaskGetOutOfWater::ms_iMaxNumAttempts = 4;

CTaskGetOutOfWater::CTaskGetOutOfWater(const float fMaxSearchRadius, const float fMoveBlendRatio) :
	CTask(),
	m_fMaxSearchRadius(fMaxSearchRadius),
	m_fMoveBlendRatio(fMoveBlendRatio),
	m_hFloodFill(PATH_HANDLE_NULL)
	//m_iNumAttempts(0)
{
	SetInternalTaskType(CTaskTypes::TASK_GET_OUT_OF_WATER);
}
CTaskGetOutOfWater::~CTaskGetOutOfWater()
{
	if(m_hFloodFill != PATH_HANDLE_NULL)
	{
		CPathServer::CancelRequest(m_hFloodFill);
		m_hFloodFill = PATH_HANDLE_NULL;
	}
}

bool CTaskGetOutOfWater::IsValidForMotionTask(CTaskMotionBase& task) const
{
	bool isValid = false;
	
	if (task.IsOnFoot() || task.IsInWater())
		isValid = true;

	if(!isValid && GetSubTask())
	{
		//This task is not valid, but an active subtask might be.
		isValid = GetSubTask()->IsValidForMotionTask(task);
	}

	return isValid;
}

#if !__FINAL
void CTaskGetOutOfWater::Debug() const
{
#if DEBUG_DRAW
	if(m_hFloodFill != PATH_HANDLE_NULL)
	{
		grcDebugDraw::Sphere(VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition()), m_fMaxSearchRadius, Color_orange, false);
	}
	if(GetState()==State_NavigateToExitPoint)
	{
		grcDebugDraw::Line(VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition()), m_vClosestPositionOnLand, Color_plum, Color_peru);
	}
#endif
}
const char * CTaskGetOutOfWater::GetStaticStateName( s32 iState )
{
	switch(iState)
	{
		case State_Initial: return "State_Initial";
		case State_Surface: return "State_Surface";
		case State_SearchForExitPoint: return "State_SearchForExitPoint";
		case State_NavigateToExitPoint: return "State_NavigateToExitPoint";
		case State_NavigateToGroupLeader: return "State_NavigateToGroupLeader";
		case State_WaitAfterFailedSearch: return "State_WaitAfterFailedSearch";
		case State_Finished: return "State_Finished";
		default: return "UNKNOWN";		
	}
}
#endif

CTask::FSM_Return CTaskGetOutOfWater::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin
		FSM_State(State_Initial)
			FSM_OnEnter
				return StateInitial_OnEnter();
			FSM_OnUpdate
				return StateInitial_OnUpdate();
		FSM_State(State_Surface)
			FSM_OnEnter
				return StateSurface_OnEnter();
			FSM_OnUpdate
				return StateSurface_OnUpdate();
		FSM_State(State_SearchForExitPoint)
			FSM_OnEnter
				return StateSearchForExitPoint_OnEnter();
			FSM_OnUpdate
				return StateSearchForExitPoint_OnUpdate();
		FSM_State(State_NavigateToExitPoint)
			FSM_OnEnter
				return StateNavigateToExitPoint_OnEnter();
			FSM_OnUpdate
				return StateNavigateToExitPoint_OnUpdate();
		FSM_State(State_NavigateToGroupLeader)
			FSM_OnEnter
				return StateNavigateToGroupLeader_OnEnter();
			FSM_OnUpdate
				return StateNavigateToGroupLeader_OnUpdate();
		FSM_State(State_WaitAfterFailedSearch)
			FSM_OnEnter
				return StateWaitAfterFailedSearch_OnEnter();
			FSM_OnUpdate
				return StateWaitAfterFailedSearch_OnUpdate();
		FSM_State(State_Finished)
				FSM_OnEnter
				return StateFinished_OnEnter();
			FSM_OnUpdate
				return StateFinished_OnUpdate();
	FSM_End
}

CTask::FSM_Return CTaskGetOutOfWater::StateInitial_OnEnter()
{
	if(m_hFloodFill!=PATH_HANDLE_NULL)
	{
		CPathServer::CancelRequest(m_hFloodFill);
		m_hFloodFill = PATH_HANDLE_NULL;
	}
	return FSM_Continue;
}

CTask::FSM_Return CTaskGetOutOfWater::StateInitial_OnUpdate()
{
	Assert(m_hFloodFill == PATH_HANDLE_NULL);

	// Something is wrong if we have been given this task but are already on dry land.
	// NB: This used to pause momentarily
	CPed * pPed = GetPed();
	if(!pPed->GetPedResetFlag( CPED_RESET_FLAG_IsDrowning ))
	{
		// Enter State_Finished state to avoid infinite task recursion in combat task (url:bugstar:956901)
		SetState(State_Finished);
		return FSM_Continue;
	}

	// Group follower peds will try to naviagate to their leader
	CPedGroup * pGroup = pPed->GetPedsGroup();
	if(pGroup && pGroup->GetGroupMembership()->GetLeader() && pGroup->GetGroupMembership()->GetLeader() != pPed)
	{
		CPed * pLeader = pGroup->GetGroupMembership()->GetLeader();
		if(!pLeader->GetPedResetFlag( CPED_RESET_FLAG_IsDrowning ))
		{
			SetState(State_NavigateToGroupLeader);
			return FSM_Continue;
		}
	}

	{	// Check to see if we should surface or not first
		Vec3V StartPos = pPed->GetTransform().GetPosition();
		Vec3V EndPos = pPed->GetTransform().GetPosition() + Vec3V(0.f,0.f, 150.f);
		Vec3V OutPos;// = Vec3V(VEC3_ZERO);

		if (CVfxHelper::TestLineAgainstWater(StartPos, EndPos, OutPos))
		{
			if (OutPos.GetZf() > StartPos.GetZf() + 2.0f)
			{
				m_vClosestPositionOnLand = VEC3V_TO_VECTOR3(OutPos);
				SetState(State_Surface);
				return FSM_Continue;
			}
		}
	}

	SetState(State_SearchForExitPoint);
	return FSM_Continue;
}

CTask::FSM_Return CTaskGetOutOfWater::StateSurface_OnEnter()
{
	CTaskMoveGoToPoint* pTaskMove = rage_new CTaskMoveGoToPoint( m_fMoveBlendRatio, m_vClosestPositionOnLand, 1.0f, false);
	pTaskMove->SetDontExpandHeightDeltaUnderwater(true);
	pTaskMove->SetTargetHeightDelta(0.5f);

	CTaskComplexControlMovement * pCtrlMove = rage_new CTaskComplexControlMovement(pTaskMove);

	SetNewTask(pCtrlMove);
	return FSM_Continue;
}

CTask::FSM_Return CTaskGetOutOfWater::StateSurface_OnUpdate()
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_SearchForExitPoint);
		return FSM_Continue;
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskGetOutOfWater::StateNavigateToGroupLeader_OnEnter()
{
	CPed * pPed = GetPed();
	CPedGroup * pGroup = pPed->GetPedsGroup();
	if(AssertVerify(pGroup))
	{
		CPed * pLeader = pGroup->GetGroupMembership()->GetLeader();
		if(AssertVerify(pLeader))
		{
			CTaskComplexControlMovement * pCtrlMove = rage_new CTaskComplexControlMovement(
				rage_new CTaskMoveFollowNavMesh(
					m_fMoveBlendRatio, VEC3V_TO_VECTOR3(pLeader->GetTransform().GetPosition())
				)
			);
			SetNewTask(pCtrlMove);
			return FSM_Continue;
		}
	}
	SetState(State_Finished);
	return FSM_Continue;
}

CTask::FSM_Return CTaskGetOutOfWater::StateNavigateToGroupLeader_OnUpdate()
{
	Assert(GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT);

	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_Finished);
		return FSM_Continue;
	}

	CPed * pPed = GetPed();
	CTask * pTaskMove = ((CTaskComplexControlMovement*)GetSubTask())->GetRunningMovementTask(pPed);

	if(pTaskMove && pTaskMove->GetTaskType()==CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH &&
		((CTaskMoveFollowNavMesh*)pTaskMove)->IsUnableToFindRoute())
	{
		if(GetSubTask()->MakeAbortable( ABORT_PRIORITY_IMMEDIATE, NULL))
		{
			SetState(State_SearchForExitPoint);
		}
	}
	return FSM_Continue;
}

CTask::FSM_Return CTaskGetOutOfWater::StateSearchForExitPoint_OnEnter()
{
	Assert(m_hFloodFill == PATH_HANDLE_NULL);

	if(m_hFloodFill!=PATH_HANDLE_NULL)
	{
		CPathServer::CancelRequest(m_hFloodFill);
		m_hFloodFill=PATH_HANDLE_NULL;
	}

	CPed * pPed = GetPed();
	m_hFloodFill = CPathServer::RequestClosestViaFloodFill(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()), m_fMaxSearchRadius, CFloodFillRequest::EFindClosestPositionOnLand, false, true, true, pPed);

#if AI_OPTIMISATIONS_OFF
	Assertf(m_hFloodFill!=PATH_HANDLE_NULL, "CTaskComplexGetOutOfWater couldn't get flood-fill path request.");
#endif
	

	return FSM_Continue;
}

CTask::FSM_Return CTaskGetOutOfWater::StateSearchForExitPoint_OnUpdate()
{
	if( m_hFloodFill==PATH_HANDLE_NULL )
	{
		SetState(State_Finished);
		return FSM_Continue;
	}

	EPathServerRequestResult ret = CPathServer::IsRequestResultReady(m_hFloodFill);
	if(ret == ERequest_Ready)
	{
		float fClosestDistance;
		aiFatalAssertf(GetPed(), "Unexpected NULL ped");
		EPathServerErrorCode eRet = CPathServer::GetClosestFloodFillResultAndClear(m_hFloodFill, m_vClosestPositionOnLand, fClosestDistance, CFloodFillRequest::EFindClosestPositionOnLand);
		m_vClosestPositionOnLand.z += GetPed()->GetCapsuleInfo()->GetGroundToRootOffset();
		m_hFloodFill = PATH_HANDLE_NULL;

		if(eRet != PATH_FOUND)
		{
			{	// Check to see if we should surface or not first
				Vec3V StartPos = GetPed()->GetTransform().GetPosition();
				Vec3V EndPos = GetPed()->GetTransform().GetPosition() + Vec3V(0.f,0.f, 150.f);
				Vec3V OutPos;// = Vec3V(VEC3_ZERO);

				if (CVfxHelper::TestLineAgainstWater(StartPos, EndPos, OutPos))
				{
					if (OutPos.GetZf() > StartPos.GetZf() + 1.0f)
					{
						m_vClosestPositionOnLand = VEC3V_TO_VECTOR3(OutPos);
						SetState(State_Surface);
						return FSM_Continue;
					}
				}
			}

			// The search was unable to find an exit point from the water within the given radius
			SetState(State_WaitAfterFailedSearch);
			return FSM_Continue;
		}

		SetState(State_NavigateToExitPoint);
		return FSM_Continue;
	}
	else if(ret == ERequest_NotFound)
	{
		// For some reason the request handle was not found
		m_hFloodFill = PATH_HANDLE_NULL;
		return FSM_Quit;
	}

	// Continue to wait for flood-fill request to complete
	return FSM_Continue;
}

CTask::FSM_Return CTaskGetOutOfWater::StateNavigateToExitPoint_OnEnter()
{
	Assert(m_hFloodFill==PATH_HANDLE_NULL);

	CTaskComplexControlMovement * pCtrlMove = rage_new CTaskComplexControlMovement(
		rage_new CTaskMoveFollowNavMesh(
			m_fMoveBlendRatio, m_vClosestPositionOnLand
		)
	);
	SetNewTask(pCtrlMove);
	return FSM_Continue;
}

CTask::FSM_Return CTaskGetOutOfWater::StateNavigateToExitPoint_OnUpdate()
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_Finished);
		return FSM_Continue;
	}
	return FSM_Continue;
}

CTask::FSM_Return CTaskGetOutOfWater::StateWaitAfterFailedSearch_OnEnter()
{
	Assert(m_hFloodFill==PATH_HANDLE_NULL);

	// B*2566490: Kill non-mission peds if they get into CTaskGetOutOfWater::State_WaitAfterFailedSearch while in combat. 
	CPed *pPed = GetPed();
	if (!pPed->PopTypeIsMission() && pPed->GetPedIntelligence() && pPed->GetPedIntelligence()->GetAlertnessState() == CPedIntelligence::AS_MustGoToCombat)
	{
		CEventDamage tempDamageEvent(NULL, fwTimer::GetTimeInMilliseconds(), WEAPONTYPE_DROWNING);
		CPedDamageCalculator damageCalculator(NULL, 9999.9f, WEAPONTYPE_DROWNING, 0, false);
		damageCalculator.ApplyDamageAndComputeResponse(pPed, tempDamageEvent.GetDamageResponseData(), CPedDamageCalculator::DF_None);
	}

	CTaskDoNothing * pTask = rage_new CTaskDoNothing(10000);
	SetNewTask(pTask);
	return FSM_Continue;
}

CTask::FSM_Return CTaskGetOutOfWater::StateWaitAfterFailedSearch_OnUpdate()
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		return FSM_Quit;
	}
	return FSM_Continue;
}

CTask::FSM_Return CTaskGetOutOfWater::StateFinished_OnEnter()
{
	CTaskDoNothing * pTask = rage_new CTaskDoNothing(1);
	SetNewTask(pTask);
	return FSM_Continue;
}

CTask::FSM_Return CTaskGetOutOfWater::StateFinished_OnUpdate()
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		return FSM_Quit;
	}
	return FSM_Continue;
}

//********************************
// CTaskMoveAroundCoverPoints
//

CTaskMoveAroundCoverPoints::CTaskMoveAroundCoverPoints(
	Direction eDirection, 
	const Vector3& vLocalCoverPos, 
	LinkedNode iLinkedNode,
	const bool bForcePedTowardsCover, 
	const bool bForceSingleUpdateOfPedPos,
	const Vector3* pvDirectionToTarget )
: CTaskMove(MOVEBLENDRATIO_WALK),
m_vTarget(0.0f, 0.0f, 0.0f),
m_vLocalTargetPosition(vLocalCoverPos),
m_vDirectionToTarget(0.0f, 0.0f, 0.0f),
m_eDirection( eDirection ),
m_bForcePedTowardsCover(bForcePedTowardsCover),
m_bForceSingleUpdateOfPedOffset(bForceSingleUpdateOfPedPos),
m_iLinkedNode(iLinkedNode)
{
	if( pvDirectionToTarget )
	{
		m_vDirectionToTarget = *pvDirectionToTarget;
	}
	SetInternalTaskType(CTaskTypes::TASK_MOVE_AROUND_COVERPOINTS);
}

CTaskMoveAroundCoverPoints::~CTaskMoveAroundCoverPoints()
{

}
#if !__FINAL
void
CTaskMoveAroundCoverPoints::Debug() const
{
	if(GetSubTask())
	{
		GetSubTask()->Debug();
	}
}
#endif

//-------------------------------------------------------------------------
// Returns the current target position
//-------------------------------------------------------------------------
Vector3 CTaskMoveAroundCoverPoints::GetTarget(void) const
{
	return m_vTarget;
}

#define MOVE_AROUND_COVER_TARGET_RADIUS (0.05f)

//-------------------------------------------------------------------------
// Returns how close this task is trying to get to the target
//-------------------------------------------------------------------------
float	CTaskMoveAroundCoverPoints::GetTargetRadius(void) const
{
	return MOVE_AROUND_COVER_TARGET_RADIUS;
}
//-------------------------------------------------------------------------
// Returns how close this task is trying to get to the target
//-------------------------------------------------------------------------
bool CTaskMoveAroundCoverPoints::CalculateLocalTargetPosition( CPed* pPed, Vector3& vLocalTargetPosition, s32 iDesiredRotationalMovement, LinkedNode& iLinkedNode, const bool bForcePosition )
{
	Vector3 vLocalPosition;
	pPed->GetCoverPoint()->GetLocalPosition( vLocalPosition );

	if( iDesiredRotationalMovement == 0 )
	{
		return false;
	}

	iLinkedNode = NO_LINK;
	if( pPed->GetLocalOffsetToCoverPoint().Mag2() < rage::square( 0.10f ) && !bForcePosition )
	{
		return false;
	}
	if( pPed->GetCoverPoint()->GetType() == CCoverPoint::COVTYPE_BIG_OBJECT ||
		pPed->GetCoverPoint()->GetType() == CCoverPoint::COVTYPE_VEHICLE  )
	{
		vLocalTargetPosition = vLocalPosition;
	}
	else
	{
		vLocalTargetPosition.Zero();
	}

	return true;
}

#define ROTATIONAL_MOVEMENT (EIGHTH_PI)

//-------------------------------------------------------------------------
// Calculates a target position for moving around the cover
//-------------------------------------------------------------------------
bool CTaskMoveAroundCoverPoints::CalculateTargetPosition( CPed* pPed )
{
	bool bReturn = false;
	// Work out the direction toward the cover
	Vector3 vCoverCenterPosition;
	pPed->GetCoverPoint()->GetCoverPointPosition(vCoverCenterPosition);
	Vector3 vDirectionToTarget = vCoverCenterPosition - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	if( m_vDirectionToTarget.x != 0.0f || m_vDirectionToTarget.y != 0.0f )
	{
		vDirectionToTarget = m_vDirectionToTarget;
	}

	const bool bPlayersLocalCoverpoint = pPed->IsLocalPlayer() && pPed->GetPlayerInfo()->GetDynamicCoverPoint() == pPed->GetCoverPoint();

	// BIG OBJECTS have multiple cover points, head around the outside bounding box of the object until
	// a new coverpoint is reached, then switch to using that coverpoint if possible
	if( !bPlayersLocalCoverpoint && (
		pPed->GetCoverPoint()->GetType() == CCoverPoint::COVTYPE_BIG_OBJECT ||
		pPed->GetCoverPoint()->GetType() == CCoverPoint::COVTYPE_VEHICLE ||
		pPed->GetCoverPoint()->GetType() == CCoverPoint::COVTYPE_POINTONMAP ||
		pPed->GetCoverPoint()->GetType() == CCoverPoint::COVTYPE_SCRIPTED))
	{
		Vector3 vLocalPosition;;
		Vector3 vLocalTargetPos = m_vLocalTargetPosition;
		pPed->GetCoverPoint()->GetLocalPosition( vLocalPosition );
		
		if( !m_bForcePedTowardsCover || m_bForceSingleUpdateOfPedOffset )
		{
			m_bForceSingleUpdateOfPedOffset = false;
			// Get the local position and direction
			if( pPed->GetCoverPoint()->m_pEntity )
			{
				// Transform the local target pos into world co-ordinates
				m_vTarget = VEC3V_TO_VECTOR3(pPed->GetCoverPoint()->m_pEntity->GetTransform().Transform3x3(VECTOR3_TO_VEC3V(vLocalTargetPos)));
				m_vTarget += VEC3V_TO_VECTOR3(pPed->GetCoverPoint()->m_pEntity->GetTransform().GetPosition());

				// Record the peds local offset to the coverpoint, so this can be used to keep 
				// them in the correct place.
				Vector3 vLocalOffsetToCoverPoint = VEC3V_TO_VECTOR3(pPed->GetCoverPoint()->m_pEntity->GetTransform().UnTransform( pPed->GetTransform().GetPosition() ));
				pPed->SetLocalOffsetToCoverPoint(vLocalOffsetToCoverPoint);
			}
			else
			{
				m_vTarget = vLocalPosition + vLocalTargetPos;
				pPed->SetLocalOffsetToCoverPoint( VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) - vLocalPosition );
				vLocalPosition.Zero();
			}

			// Bring the local ped offset to be in between the 2 coverpoints:
			Vector3 vFromToA = vLocalTargetPos - vLocalPosition;
			float fMaxLength = vFromToA.Mag();
			if( vFromToA.Mag2() < 0.05f )
			{
				// Use a vector perpendicular to the coverpoints direction instead
				Vector3 vDir = pPed->GetCoverPoint()->GetLocalDirectionVector();

				Vector3 vUpAxis = ZAXIS;
				if( pPed->GetCoverPoint()->m_pEntity )
				{
					CoverOrientation orientation = CCachedObjectCoverManager::GetUpAxis((CPhysical*)pPed->GetCoverPoint()->m_pEntity.Get());

					if(orientation == CO_XUp || orientation == CO_XDown)
					{
						vUpAxis = XAXIS;
					}
					else if(orientation == CO_YUp || orientation == CO_YDown)
					{
						vUpAxis = YAXIS;
					}
				}

				vFromToA.Cross( vDir, vUpAxis );
				// The max distance is the already calculated distance from the cover
				fMaxLength = (vLocalTargetPos - pPed->GetLocalOffsetToCoverPoint()).XYMag();
			}
			else
			{
				vFromToA.Normalize();
			}
			float fDot = vFromToA.Dot(pPed->GetLocalOffsetToCoverPoint() - vLocalPosition);
			fDot = MIN( fMaxLength, fDot );
			pPed->SetLocalOffsetToCoverPoint( vFromToA * fDot );


			// Don't allow peds to move from the initial cover position
			if( !pPed->IsPlayer() )
			{
				pPed->SetLocalOffsetToCoverPoint( VEC3_ZERO );
			}
		}
		if( m_bForcePedTowardsCover )
		{
			// Transform the local target pos into world co-ordinates
			if( pPed->GetCoverPoint()->m_pEntity )
			{
				m_vTarget = VEC3V_TO_VECTOR3(pPed->GetCoverPoint()->m_pEntity->GetTransform().Transform3x3(VECTOR3_TO_VEC3V(pPed->GetLocalOffsetToCoverPoint() + vLocalPosition)));
				m_vTarget += VEC3V_TO_VECTOR3(pPed->GetCoverPoint()->m_pEntity->GetTransform().GetPosition());
			}
			else
			{
				pPed->GetCoverPoint()->GetLocalPosition( vLocalPosition );
				m_vTarget = pPed->GetLocalOffsetToCoverPoint() + vLocalPosition;
			}
		}
	}

	// Work out the desired direction to move in
	s32 iDesiredRotationalMovement = 0;
	if( pPed->IsPlayer() && !m_bForcePedTowardsCover )
	{
		iDesiredRotationalMovement = CalculateDesiredDirection(pPed);

		// Stop the task if no movement is requested or if the movement is in the wrong direction
		if( iDesiredRotationalMovement == 0 )
		{
			bReturn = true;
		}
		else if( m_eDirection == LEFT_ONLY && iDesiredRotationalMovement > 0 )
		{
			bReturn = true;
		}
		else if( m_eDirection == RIGHT_ONLY && iDesiredRotationalMovement < 0 )
		{
			bReturn = true;
		}
	}

	// Circular objects are the easiest to navigate around,
	// simply head to a point slightly further round the circle
	if( pPed->GetCoverPoint()->GetType() == CCoverPoint::COVTYPE_OBJECT )
	{
		vDirectionToTarget.Normalize();
		Vector3 vDirectionPerpendicularToTarget = vDirectionToTarget;
		// The desired heading will eventually be perpendicular to the 
		// 
		// Rotate the direction to the target round in the direction requested and head toward that point
		if( iDesiredRotationalMovement < 0 )
		{
			vDirectionToTarget.RotateZ(-ROTATIONAL_MOVEMENT);
			vDirectionPerpendicularToTarget.RotateZ(HALF_PI - EIGHTH_PI);
		}
		else if( iDesiredRotationalMovement > 0 )
		{
			vDirectionToTarget.RotateZ(ROTATIONAL_MOVEMENT);
			vDirectionPerpendicularToTarget.RotateZ(-HALF_PI + EIGHTH_PI);
		}
		CCover::FindCoordinatesCoverPoint( pPed->GetCoverPoint(), pPed, vDirectionToTarget, m_vTarget );

		pPed->SetDesiredHeading( VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) + vDirectionPerpendicularToTarget );
	}

	// If the player is cycling round a dynamic cover point, update it.
	if( bPlayersLocalCoverpoint )
	{
		if( GetSubTask() )
		{
			Vector3 vTestPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
			// Use the collision capsule because it isn't affected by leg ik
			if( pPed->GetCurrentPhysicsInst() )
				vTestPos.z = pPed->GetCurrentPhysicsInst()->GetMatrix().GetM23f(); // AI_NEW_VEC_LIB
// 			if( !CDynamicCoverHelper::TestForDynamicCover( pPed, vTestPos, true, -999.0f, true ) )
// 			{
// 				return true;
// 			}

			pPed->GetCoverPoint()->ReserveCoverPointForPed(pPed);
			const bool bWantsToMoveRight = iDesiredRotationalMovement > 0;
			const bool bWantsToMoveLeft = iDesiredRotationalMovement < 0;
			const bool bCanMoveRight = pPed->GetPlayerInfo()->DynamicCoverCanMoveRight();
			const bool bCanMoveLeft = pPed->GetPlayerInfo()->DynamicCoverCanMoveLeft();
			const bool bCanMoveAboutDynamicCover = bPlayersLocalCoverpoint && 
					( m_bForcePedTowardsCover || (bWantsToMoveRight && bCanMoveRight) || (bWantsToMoveLeft && bCanMoveLeft ) );

			if( !bCanMoveAboutDynamicCover )
			{
				return true;
			}
		}		

		static float TARGET_SCALE = 1.0f;
		Vector3 vDir = pPed->GetCoverPoint()->GetLocalDirectionVector();
		pPed->GetCoverPoint()->GetCoverPointPosition(m_vTarget);
		if( m_eDirection == LEFT_ONLY )
		{
			vDir.RotateZ(HALF_PI);
			vDir.Scale(TARGET_SCALE);
			m_vTarget += vDir;
		}
		else if( m_eDirection == RIGHT_ONLY )
		{
			vDir.RotateZ(-HALF_PI);
			vDir.Scale(TARGET_SCALE);
			m_vTarget += vDir;
		}
		const Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		const float fDesiredHeading = fwAngle::GetRadianAngleBetweenPoints(m_vTarget.x, m_vTarget.y, vPedPos.x, vPedPos.y);
		pPed->SetDesiredHeading( fDesiredHeading );
	}


	return bReturn;
}

CTask::FSM_Return
CTaskMoveAroundCoverPoints::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin
		FSM_State(State_Initial)
			FSM_OnUpdate
				return Intitial_OnUpdate(pPed);
		FSM_State(State_MovingToCoverPoint)
			FSM_OnEnter
				return MovingToCoverPoint_OnEnter(pPed);
			FSM_OnUpdate
				return MovingToCoverPoint_OnUpdate(pPed);
	FSM_End
}

CTask::FSM_Return
CTaskMoveAroundCoverPoints::Intitial_OnUpdate(CPed * pPed)
{
	// This is the first time round, make sure the task has a valid cover point.
	Assert(pPed->GetCoverPoint());

	CalculateTargetPosition(pPed);
	pPed->SetIsStrafing(true);

	SetState(State_MovingToCoverPoint);

	return FSM_Continue;
}

CTask::FSM_Return
CTaskMoveAroundCoverPoints::MovingToCoverPoint_OnEnter(CPed * UNUSED_PARAM(pPed))
{
	SetNewTask(rage_new CTaskMoveGoToPoint( m_fMoveBlendRatio, m_vTarget, MOVE_AROUND_COVER_TARGET_RADIUS, false ) );
	return FSM_Continue;
}
CTask::FSM_Return
CTaskMoveAroundCoverPoints::MovingToCoverPoint_OnUpdate(CPed * pPed)
{
	static dev_float STOP_MOVE_SPEED = 1.0f;

	Assert(GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_MOVE_GO_TO_POINT);

	if( pPed->GetCoverPoint() == NULL)
	{
		GetSubTask()->MakeAbortable( CTask::ABORT_PRIORITY_IMMEDIATE, NULL);
		return FSM_Quit;
	}

	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		// Switch to the new cover point if within a proximity
		if( !m_bForcePedTowardsCover &&
			( m_vTarget - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) ).XYMag2() < 0.25f &&
			m_iLinkedNode != NO_LINK )
		{
			// IF the cover point is in the center, allow the ped to continue moving

			// Work out the desired direction to move in
			const s32 iDesiredRotationalMovement = CalculateDesiredDirection(pPed);
			if( CalculateLocalTargetPosition( pPed, m_vLocalTargetPosition, iDesiredRotationalMovement, m_iLinkedNode ) )
			{
				CalculateTargetPosition(pPed);
				if( ( m_vTarget - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) ).XYMag2() > 0.25f )
				{
					SetNewTask( rage_new CTaskMoveGoToPoint( m_fMoveBlendRatio, m_vTarget, MOVE_AROUND_COVER_TARGET_RADIUS, false ) );
				}
			}
		}

		if(!GetNewTask())
			return FSM_Quit;
	}


	pPed->SetIsStrafing(true);

	CTaskMoveGoToPoint* pMoveSubTask = ((CTaskMoveGoToPoint*)GetSubTask());

	// If the task has decided it should terminate, only
	// terminate once the ped has stopped
	bool bStop = CalculateTargetPosition(pPed);
	if( (pMoveSubTask && CPedMotionData::GetIsStill(pMoveSubTask->GetMoveBlendRatio())) || bStop )
	{
		if( pMoveSubTask && pPed->GetVelocity().Mag2() > rage::square(STOP_MOVE_SPEED) &&
			pPed->GetCoverPoint()->GetUsage() == CCoverPoint::COVUSE_WALLTOBOTH )
		{
			CTaskMoveGoToPoint* pMoveSubTask ((CTaskMoveGoToPoint*)GetSubTask());
			pMoveSubTask->SetMoveBlendRatio(MOVEBLENDRATIO_STILL);
			return FSM_Continue;
		}
		else
		{
			GetSubTask()->MakeAbortable( CTask::ABORT_PRIORITY_IMMEDIATE, NULL );
			return FSM_Quit;
		}
	}

	pMoveSubTask->SetTarget( pPed, VECTOR3_TO_VEC3V(m_vTarget) );

	float fMoveBlendRatio = MOVEBLENDRATIO_WALK;
	
	if (pPed->IsAPlayerPed())
	{
		static bool s_bUseFastCoverMovement = false;

		if (s_bUseFastCoverMovement)
		{
			const bool bUseButtonBashing = !NetworkInterface::IsGameInProgress() && !NetworkInterface::IsInMPTutorial();
	
			float fSprintResult = pPed->GetPlayerInfo()->ControlButtonSprint(CPlayerInfo::SPRINT_ON_FOOT, bUseButtonBashing);

			if(fSprintResult > 0.0f)
			{
				fMoveBlendRatio = MOVEBLENDRATIO_RUN;
			}
		}
	}

	pMoveSubTask->SetMoveBlendRatio(fMoveBlendRatio);

	return FSM_Continue;
}

s32 CTaskMoveAroundCoverPoints::CalculateDesiredDirection( CPed* pPed )
{
	if( pPed->IsPlayer() && !m_bForcePedTowardsCover && pPed->GetCoverPoint() )
	{
		Vector3 vToTarget = m_vTarget - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		vToTarget.z = 0.0f;
		vToTarget.Normalize();
		Vector3 vCoverHeading = VEC3V_TO_VECTOR3(pPed->GetCoverPoint()->GetCoverDirectionVector(&RCC_VEC3V(vToTarget)));
		CControl *pControl = pPed->GetControlFromPlayer();
		Vector2 vecStick(pControl->GetPedWalkLeftRight().GetNorm() * 127.0f, -pControl->GetPedWalkUpDown().GetNorm() * 127.0f);
		vecStick.Rotate(camInterface::GetHeading());
		vecStick.Rotate(-rage::Atan2f(-vCoverHeading.x, vCoverHeading.y));
		if( vecStick.x > MIN_STICK_INPUT_TO_MOVE_IN_COVER )
			return 1;
		else if( vecStick.x < -MIN_STICK_INPUT_TO_MOVE_IN_COVER )
			return -1;
	}
	return 0;
}



//****************************************************************************
//	CTaskMoveCrowdAroundLocation
//	Movement task which gets a ped to approach & stand around a location at
//	a given radius.  Ped will be aware of other peds doing the same task
//	nearby, and will attempt to maintain an even spacing with them.

const float CTaskMoveCrowdAroundLocation::ms_fDefaultRadius = 5.0f;
const float CTaskMoveCrowdAroundLocation::ms_fDefaultSpacing = (PI * (ms_fDefaultRadius+ms_fDefaultRadius)) / 8.0f;
const float CTaskMoveCrowdAroundLocation::ms_fRequiredDistFromTarget = 0.5f;
const float CTaskMoveCrowdAroundLocation::ms_fGoToPointDist = 3.0f;
const float CTaskMoveCrowdAroundLocation::ms_fTargetRadiusForCloseNavMeshTask = 0.5f;
const float CTaskMoveCrowdAroundLocation::ms_fLooseCrowdAroundHeadingThresholdDegrees = 30.0f;

CTaskMoveCrowdAroundLocation::CTaskMoveCrowdAroundLocation(const Vector3 & vLocation, const float fRadius, const float UNUSED_PARAM(fSpacing)) :
	CTaskMove(MOVEBLENDRATIO_RUN),
	m_vLocation(vLocation),
	m_vCrowdRoundPos(vLocation),
	m_fRadius(fRadius),
	m_bWalkedIntoSomething(false),
	m_vLocationWhenWalkedIntoSomething(vLocation),
	m_bUseLineTestsToStopPedWalkingOverEdge(false),
	m_bRemainingWithinCrowdRadiusIsAcceptable(false)
{
	SetInternalTaskType(CTaskTypes::TASK_MOVE_CROWD_AROUND_LOCATION);
}

CTaskMoveCrowdAroundLocation::~CTaskMoveCrowdAroundLocation()
{

}

#if !__FINAL
void
CTaskMoveCrowdAroundLocation::Debug() const
{
#if DEBUG_DRAW
	Vector3 vX(0.3f, 0.0f, 0.0f);
	Vector3 vY(0.0f, 0.3f, 0.0f);
	Color32 col(255, 128, 0, 255);

	// Draw a little cross at m_vCrowdRoundPos
	grcDebugDraw::Line(m_vCrowdRoundPos - vX, m_vCrowdRoundPos + vX, col);
	grcDebugDraw::Line(m_vCrowdRoundPos - vY, m_vCrowdRoundPos + vY, col);
#endif

	if(GetSubTask()) GetSubTask()->Debug();
}
#endif

Vector3 CTaskMoveCrowdAroundLocation::GetTarget(void) const { return m_vLocation; }
float CTaskMoveCrowdAroundLocation::GetTargetRadius(void) const { return m_fRadius; }

void CTaskMoveCrowdAroundLocation::SetTarget(const CPed * UNUSED_PARAM(pPed), Vec3V_In vTarget, const bool UNUSED_PARAM(bForce))
{
	m_vLocation = VEC3V_TO_VECTOR3(vTarget);
}
void CTaskMoveCrowdAroundLocation::SetTargetRadius(const float fRadius)	{m_fRadius = fRadius;}


void
CTaskMoveCrowdAroundLocation::CalcCrowdRoundPosition(const CPed * pPed)
{
	Vector3 vToPed = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) - m_vLocation;
	vToPed.Normalize();

	m_vCrowdRoundPos = m_vLocation + (vToPed * m_fRadius);
}

void
CTaskMoveCrowdAroundLocation::AdjustCrowdRndPosForOtherPeds(const CPed * pPed)
{
	// Only do this when we're standing still at m_vCrowdRoundPos
	if(GetSubTask() && (GetSubTask()->GetTaskType()!=CTaskTypes::TASK_MOVE_STAND_STILL &&
		GetSubTask()->GetTaskType()!=CTaskTypes::TASK_MOVE_GO_TO_POINT))
		return;

	static const float fDistTooClose = 1.0f;
	Vector3 vAdjustment(0.0f,0.0f,0.0f);

	const CEntityScannerIterator entityList = pPed->GetPedIntelligence()->GetNearbyPeds();
	for( const CEntity* pEnt = entityList.GetFirst(); pEnt; pEnt = entityList.GetNext() )
	{
		const CPed * pOtherPed = static_cast<const CPed*>(pEnt);
		if(pOtherPed->IsDead() || pOtherPed->IsFatallyInjured())
			continue;

		// Sum up vectors away from all peds which are too close
		Vector3 vDiff = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition() - pOtherPed->GetTransform().GetPosition());
		float fDiffSqr = vDiff.Mag2();
		if(fDiffSqr < (fDistTooClose*fDistTooClose))
		{
			if(fDiffSqr > 0.01f)
			{
				float fInvScale = fDistTooClose - Sqrtf(fDiffSqr);
				vDiff.Normalize();
				vDiff *= fInvScale;
				vAdjustment += vDiff;
			}
		}
	}

	if(!vAdjustment.Mag2())
		return;

	// Now normalize & scale by a small value, so peds don't break out of the goto point task
	static const float fScale = 1.0f;
	vAdjustment *= fScale;

	m_vCrowdRoundPos += vAdjustment;
	Vector3 vFromCentre = m_vCrowdRoundPos - m_vLocation;
	vFromCentre.Normalize();
	vFromCentre *= m_fRadius;

	m_vCrowdRoundPos = m_vLocation + vFromCentre;
}

bool
CTaskMoveCrowdAroundLocation::IsPedInVicinityOfCrowd(const CPed * pPed)
{
	float fCrowdDist = m_fRadius + 5.0f;
	Vector3 vToTarget = m_vLocation - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	float fDistSqr = vToTarget.Mag2();
	return(fDistSqr < (fCrowdDist*fCrowdDist));
}

bool CTaskMoveCrowdAroundLocation::ShouldUpdatedDesiredHeading() const
{
	const CPed* pPed = GetPed();

	// Most peds always update the desired heading.
	if (!pPed->GetTaskData().GetIsFlagSet(CTaskFlags::UseLooseCrowdAroundMetrics))
	{
		return true;
	}

	float fCurrentHeading = pPed->GetCurrentHeading();

	// Compute our current heading offset.
	float fTargetHeading = fwAngle::GetRadianAngleBetweenPoints(m_vLocation.x, m_vLocation.y, pPed->GetTransform().GetPosition().GetXf(), pPed->GetTransform().GetPosition().GetYf());

	// Check if it is big enough.
	if (fabs(SubtractAngleShorter(fCurrentHeading, fTargetHeading)) > DtoR * ms_fLooseCrowdAroundHeadingThresholdDegrees)
	{
		return true;
	}

	return false;
}

CTask::FSM_Return
CTaskMoveCrowdAroundLocation::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin
		FSM_State(State_Initial)
			FSM_OnUpdate
				return Initial_OnUpdate(pPed);
		FSM_State(State_MovingToCrowdLocation)
			FSM_OnEnter
				return MovingToCrowdLocation_OnEnter(pPed);
			FSM_OnUpdate
				return MovingToCrowdLocation_OnUpdate(pPed);
		FSM_State(State_CrowdingAroundLocation)
			FSM_OnEnter
				return CrowdingAroundLocation_OnEnter(pPed);
			FSM_OnUpdate
				return CrowdingAroundLocation_OnUpdate(pPed);
	FSM_End
}

CTask::FSM_Return
CTaskMoveCrowdAroundLocation::Initial_OnUpdate(CPed* pPed)
{
	if(!IsPedInVicinityOfCrowd(pPed))
	{
		SetState(State_MovingToCrowdLocation);
	}
	else
	{
		SetState(State_CrowdingAroundLocation);
		return FSM_Continue;
	}

	return FSM_Continue;
}

CTask::FSM_Return
CTaskMoveCrowdAroundLocation::MovingToCrowdLocation_OnEnter(CPed* pPed)
{
	SetNewTask( CreateSubTask(CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH, pPed) );
	return FSM_Continue;
}

CTask::FSM_Return
CTaskMoveCrowdAroundLocation::MovingToCrowdLocation_OnUpdate(CPed* pPed)
{
	Assert(GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH);

	m_bWalkedIntoSomething = false;

	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		if(!IsPedInVicinityOfCrowd(pPed))
		{
			SetNewTask( CreateSubTask(CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH, pPed) );
		}
		else
		{
			SetState(State_CrowdingAroundLocation);
		}
	}

	return FSM_Continue;
}

CTask::FSM_Return
CTaskMoveCrowdAroundLocation::CrowdingAroundLocation_OnEnter(CPed* pPed)
{
	CalcCrowdRoundPosition(pPed);
	AdjustCrowdRndPosForOtherPeds(pPed);

	if(m_bRemainingWithinCrowdRadiusIsAcceptable && (m_vLocation - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) ).Mag() < m_fRadius)
	{
		SetNewTask( CreateSubTask(CTaskTypes::TASK_MOVE_STAND_STILL, pPed) );
	}
	else
	{
		SetNewTask( CreateSubTask(CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH, pPed) );
	}

	return FSM_Continue;
}

CTask::FSM_Return
CTaskMoveCrowdAroundLocation::CrowdingAroundLocation_OnUpdate(CPed* pPed)
{
	// If sufficiently far from crowd center then we enter 'State_MovingToCrowdLocation'
	if(!IsPedInVicinityOfCrowd(pPed))
	{
		m_bWalkedIntoSomething = false;
		SetState(State_MovingToCrowdLocation);
		return FSM_Continue;
	}

	if (ShouldUpdatedDesiredHeading())
	{
		pPed->SetDesiredHeading(m_vLocation);
	}

	CalcCrowdRoundPosition(pPed);
	AdjustCrowdRndPosForOtherPeds(pPed);

	//**********************************************************
	// Break out of the goto point task if we're too far away

	const Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

	if(GetSubTask()->GetTaskType()==CTaskTypes::TASK_MOVE_GO_TO_POINT)
	{
		// If static count has reached a certain point, we are probably walking into a wall.
		// Let the ped stop here, until the m_vCrowdRoundPos has changed at which point try to move again.
		if(pPed->GetPedIntelligence()->GetEventScanner()->GetStaticMovementScanner().GetStaticCounter() > 20)
		{
			pPed->GetPedIntelligence()->GetEventScanner()->GetStaticMovementScanner().ResetStaticCounter();
			m_bWalkedIntoSomething = true;
			m_vLocationWhenWalkedIntoSomething = m_vLocation;
		}

		// If we're at correct position, then stand still
		Vector3 vDiff = m_vCrowdRoundPos - vPedPos;
		float fDistSqr = vDiff.Mag2();
		if(fDistSqr > (ms_fGoToPointDist+1.0f)*(ms_fGoToPointDist+1.0f) || m_bWalkedIntoSomething)
		{
			if(GetSubTask()->MakeAbortable( ABORT_PRIORITY_URGENT, NULL))
			{
				//pNewSubTask = CreateNextSubTask(pPed);
				return FSM_Continue;
			}
		}
		else
		{
			// Update the goto point task
			((CTaskMoveGoToPoint*)GetSubTask())->SetTarget(pPed, VECTOR3_TO_VEC3V(m_vCrowdRoundPos));
		}
	}

	//**********************************************************************************
	// Break out of the stand still task if we're too far away from the crowd-round pos

	if(GetSubTask()->GetTaskType()==CTaskTypes::TASK_MOVE_STAND_STILL && !m_bRemainingWithinCrowdRadiusIsAcceptable)
	{
		bool bBreakOutOfStandStill = false;

		if(m_bWalkedIntoSomething)
		{
			static const float fLocChangedEpsSqr = 1.0f*1.0f;
			Vector3 vDiff = m_vLocationWhenWalkedIntoSomething - m_vLocation;
			if(vDiff.Mag2() > fLocChangedEpsSqr)
			{
				bBreakOutOfStandStill = true;
			}
		}
		else
		{
			// If we're at correct position, then stand still
			static const float fBreakOutDistSrq = (ms_fRequiredDistFromTarget+0.25f)*(ms_fRequiredDistFromTarget+0.25f);
			Vector3 vDiff = m_vCrowdRoundPos - vPedPos;
			float fDistSqr = vDiff.Mag2();
			if(fDistSqr > fBreakOutDistSrq)
			{
				bBreakOutOfStandStill = true;
			}
		}

		if(bBreakOutOfStandStill)
		{
			if(GetSubTask()->MakeAbortable( ABORT_PRIORITY_URGENT, NULL))
			{
				SetNewTask( CreateSubTask(CTaskTypes::TASK_MOVE_GO_TO_POINT, pPed) );
				return FSM_Continue;
			}
		}
	}

	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		// If we have already walked into something, then check to see whether we should move again
		if(m_bWalkedIntoSomething)
		{
			static const float fLocChangedEpsSqr = 1.0f*1.0f;
			Vector3 vDiff = m_vLocationWhenWalkedIntoSomething - m_vLocation;
			if(vDiff.Mag2() < fLocChangedEpsSqr)
			{
				SetNewTask( CreateSubTask(CTaskTypes::TASK_MOVE_STAND_STILL, pPed) );
				return FSM_Continue;
			}
		}

		m_bWalkedIntoSomething = false;

		CalcCrowdRoundPosition(pPed);
		AdjustCrowdRndPosForOtherPeds(pPed);

		const Vector3 vecPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		// If we're at correct position, then stand still
		const Vector3 vDiff = m_vCrowdRoundPos - vecPedPos;
		const float fDistSqr = vDiff.XYMag2();

		if(m_bRemainingWithinCrowdRadiusIsAcceptable && (m_vLocation - vecPedPos).Mag() < m_fRadius)
		{
			SetNewTask( CreateSubTask(CTaskTypes::TASK_MOVE_STAND_STILL, pPed) );
		}
		else if(fDistSqr < ms_fRequiredDistFromTarget*ms_fRequiredDistFromTarget)
		{
			SetNewTask( CreateSubTask(CTaskTypes::TASK_MOVE_STAND_STILL, pPed) );
		}
		else if(fDistSqr < ms_fGoToPointDist*ms_fGoToPointDist && !m_bRemainingWithinCrowdRadiusIsAcceptable)
		{
			// If really close to our target pos then do a goto point
			SetNewTask( CreateSubTask(CTaskTypes::TASK_MOVE_GO_TO_POINT, pPed) );
		}
		else
		{
			// Otherwise use navmesh task
			SetNewTask( CreateSubTask(CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH, pPed) );
		}
	}

	return FSM_Continue;
}

CTask *
CTaskMoveCrowdAroundLocation::CreateSubTask(const int iSubTaskType, CPed* pPed)
{
	switch(iSubTaskType)
	{
		case CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH:
		{
			// If close to the crowd then we want to walk, and our target is the position we calculated at the desired radius from the centre location
			if(IsPedInVicinityOfCrowd(pPed))
			{
				CTaskMoveFollowNavMesh * pNavTask = rage_new CTaskMoveFollowNavMesh(MOVEBLENDRATIO_WALK, m_vCrowdRoundPos, ms_fTargetRadiusForCloseNavMeshTask);
				return pNavTask;
			}
			// Otherwise we run to the location, and use the central position as the target.  Use the m_fRadius value (plus a little, to account for stopping-distance)
			else
			{
				CTaskMoveFollowNavMesh * pNavTask = rage_new CTaskMoveFollowNavMesh(MOVEBLENDRATIO_RUN, m_vLocation, m_fRadius + 1.0f);
				return pNavTask;
			}
		}
		case CTaskTypes::TASK_MOVE_GO_TO_POINT:
		{
			CTaskMoveGoToPoint * pGotoTask = rage_new CTaskMoveGoToPoint(MOVEBLENDRATIO_WALK, m_vCrowdRoundPos, 0.25f);
			pGotoTask->SetUseLineTestForDrops(m_bUseLineTestsToStopPedWalkingOverEdge);
			return pGotoTask;
		}
		case CTaskTypes::TASK_MOVE_STAND_STILL:
		{
			CTaskMoveStandStill * pStandTask = rage_new CTaskMoveStandStill(2000);
			
			return pStandTask;
		}
	}
	Assert(0);
	return NULL;
}

//****************************************************************
// CTaskGoToScenario
//****************************************************************

// Instance of tunable task params
CTaskGoToScenario::Tunables CTaskGoToScenario::sm_Tunables;	
IMPLEMENT_MOVEMENT_TASK_TUNABLES(CTaskGoToScenario, 0x944f36ea);

CTaskGoToScenario::CTaskGoToScenario(const TaskSettings & settings )
: m_Settings( settings )
, m_fCloseTimer(0.0f)
, m_fBrokenPointTimer(0.0f)
, m_bUpdateTarget(false)
, m_bEntryClipLoaded(false)
{
	Quaternion qTargetRotation;
	qTargetRotation.FromEulers(Vector3(0.0f,0.0f,settings.GetHeading()), "xyz");
	m_PlayAttachedClipHelper.SetTarget(settings.GetTarget(), qTargetRotation);
	SetInternalTaskType(CTaskTypes::TASK_GO_TO_SCENARIO);
}

CTaskGoToScenario::~CTaskGoToScenario()
{
	if(m_pAdditionalTaskDuringApproach)
	{
		delete m_pAdditionalTaskDuringApproach;
		m_pAdditionalTaskDuringApproach = NULL;
	}
}

aiTask *CTaskGoToScenario::Copy() const
{
	CTaskGoToScenario *clone = rage_new CTaskGoToScenario( m_Settings );

	if(m_pAdditionalTaskDuringApproach)
	{
		aiTask* pCopiedAdditionalTask = m_pAdditionalTaskDuringApproach->Copy();
		taskAssert(dynamic_cast<CTask*>(pCopiedAdditionalTask));
		clone->SetAdditionalTaskDuringApproach(static_cast<CTask*>(pCopiedAdditionalTask));
	}

	return clone;

}

void CTaskGoToScenario::CleanUp()
{
}

void CTaskGoToScenario::SetAdditionalTaskDuringApproach(const CTask* task)
{
	if(m_pAdditionalTaskDuringApproach)
	{
		delete m_pAdditionalTaskDuringApproach;
		m_pAdditionalTaskDuringApproach = NULL;
	}

	m_pAdditionalTaskDuringApproach = task;
}

bool CTaskGoToScenario::CalculateGoToPositionAndHeading()
{
	// Get our clip dictionary and try to stream it in
	s32 iClipDict = m_Settings.GetClipDict();
	if (iClipDict > 0 && m_ClipRequestHelper.RequestClipsByIndex( iClipDict ) )
	{
		// Once streamed get the clip at the index we have
		const crClip* pClip = fwAnimManager::GetClipIfExistsByDictIndex(iClipDict, m_Settings.GetClipHash() );
		if (pClip)
		{
			// Compute our ideal start position and orientation
			Vector3 vStartPos;
			Quaternion qStartOrientation;
			m_PlayAttachedClipHelper.ComputeIdealStartSituation(pClip, vStartPos, qStartOrientation);
			m_Settings.SetTarget( vStartPos );
			Vector3 vEulers;
			qStartOrientation.ToEulers(vEulers, "xyz");
			m_Settings.SetHeading( fwAngle::LimitRadianAngle(vEulers.z) );
			return true;
		}
	}
	return false;
}

bool CTaskGoToScenario::IsClose() const
{
	Vec3V vPedPos = GetPed()->GetTransform().GetPosition();
	Vec3V vPointPos = VECTOR3_TO_VEC3V(m_Settings.GetTarget());

	// Flatten.
	vPedPos.SetZ(ScalarV(V_ZERO));
	vPointPos.SetZ(ScalarV(V_ZERO));

	if (IsLessThanAll(DistSquared(vPedPos, vPointPos), ScalarV(sm_Tunables.m_ClosePointDistanceSquared)))
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool CTaskGoToScenario::CanBlendOut() const
{
	float PedHeadingDiffStartBlend = sm_Tunables.m_HeadingDiffStartBlendDegrees * DtoR;
	float PedPositionDiffStartBlend = sm_Tunables.m_PositionDiffStartBlend;

	float PedHeadingDiff = SubtractAngleShorter(GetPed()->GetCurrentHeading(), m_Settings.GetHeading());
	if (abs(PedHeadingDiff) >= PedHeadingDiffStartBlend)
		return false;

	const CPed* pPed = GetPed();
	Vector3 PedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	Vector3 Target(m_Settings.GetTarget());
	if(pPed->GetCapsuleInfo())
		Target.z += pPed->GetCapsuleInfo()->GetGroundToRootOffset();
	if (PedPos.Dist2(Target) > PedPositionDiffStartBlend * PedPositionDiffStartBlend)
		return false;

	// Traffe TODO: see if we can remove this once james fixed some stuff
	CTaskMotionBase* pCurrentMotionTask = GetPed()->GetCurrentMotionTask();
	if (pCurrentMotionTask)
	{
		bool bIsStopClipRunning = false;
		float fStoppingDist = pCurrentMotionTask->GetStoppingDistance(m_Settings.GetHeading(), &bIsStopClipRunning);
		if (bIsStopClipRunning && abs(fStoppingDist) > 0.1f)
			return false;
	}

	return true;
}

CTask::FSM_Return CTaskGoToScenario::ProcessPreFSM()
{
	// if we have an clip index and have not yet loaded it go ahead and try
	if( m_Settings.GetClipHash() > 0 && !m_bEntryClipLoaded)
	{
		if(CalculateGoToPositionAndHeading())
		{
			// We'll need to update our target (if we are currently in the go to state) and remember we loaded the clip
			m_bUpdateTarget = true;
			m_bEntryClipLoaded = true;
		}
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskGoToScenario::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

		FSM_State(State_GoToPosition)
			FSM_OnEnter
				GoToPosition_OnEnter();
			FSM_OnUpdate
				return GoToPosition_OnUpdate();

		FSM_State(State_SlideToCoord)
			FSM_OnEnter
				SlideToCoord_OnEnter();
			FSM_OnUpdate
				return SlideToCoord_OnUpdate();

		FSM_State(State_Failed)
			FSM_OnUpdate
				return FSM_Quit;

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

void CTaskGoToScenario::GoToPosition_OnEnter()
{
	CPed* pPed = GetPed();

	// clear out the update target variable because if it is true it means we already have the most up to date position
	m_bUpdateTarget = false;
	m_WaitTimer.Unset();

	// Use the speed from the TaskSettings.
	const float mbr = m_Settings.GetMoveBlendRatio();

	CTaskMove* pSubtask = NULL;

	// Check if swimming, otherwise follow navmesh.
	CTaskMotionBase* motionTask = pPed->GetPrimaryMotionTask();
	if (motionTask && motionTask->IsSwimming())
	{
		// Sea.
		float fRadius = CTaskMoveGoToPoint::ms_fTargetRadius;

		if (pPed->GetPedIntelligence()->GetNavCapabilities().IsFlagSet(CPedNavCapabilityInfo::FLAG_PREFER_NEAR_WATER_SURFACE))
		{
			// Help sharks hit their points a little bit better.
			fRadius = sm_Tunables.m_PreferNearWaterSurfaceArrivalRadius;
		}
		pSubtask = rage_new CTaskMoveGoToPoint(mbr, m_Settings.GetTarget(), fRadius);
	}
	else if (pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_UseGoToPointForScenarioNavigation ))
	{
		pSubtask = rage_new CTaskMoveGoToPointStandStillAchieveHeading(mbr, m_Settings.GetTarget(), m_Settings.GetHeading(),CTaskMoveGoToPoint::ms_fTargetRadius);
	}
	else
	{
		// Land.
		//if we are ignoring the slide to position, then don't try to position exactly
		CTaskMoveFollowNavMesh* pNavMeshTask;
		if (m_Settings.IsSet( TaskSettings::IgnoreSlideToPosition ))
		{
			pNavMeshTask = rage_new CTaskMoveFollowNavMesh(mbr, m_Settings.GetTarget());
			pNavMeshTask->SetSlideToCoordAtEnd(false);
			pNavMeshTask->SetStopExactly(false);
		}
		else
		{
			pNavMeshTask = rage_new CTaskMoveFollowNavMesh(mbr, m_Settings.GetTarget(), sm_Tunables.m_ExactStopTargetRadius);
		}

		// Tell the task to start at the 1st route point.  This avoids peds getting stuck if they think they should start further along the route.
		// (Took this setup from seater task) // CWR
		pNavMeshTask->SetStartAtFirstRoutePoint(true);
		pNavMeshTask->SetDontUseClimbs(true);
		pNavMeshTask->SetDontUseDrops(true);

		// Activating the perfect walkstops
		pNavMeshTask->SetDontAdjustTargetPosition(true);
		pNavMeshTask->SetSlideToCoordAtEnd(false);

		if (!m_Settings.IsSet(TaskSettings::IgnoreStopHeading))
		{
			pNavMeshTask->SetTargetStopHeading(m_Settings.GetHeading());
			pNavMeshTask->SetLenientAchieveHeadingForExactStop(true);
		}

		// Only keep moving if the ped is already in motion.
		if (pPed->GetMotionData()->GetCurrentMbrY() > MOVEBLENDRATIO_STILL)
		{
			pNavMeshTask->SetKeepMovingWhilstWaitingForPath(true);
		}

		pSubtask = pNavMeshTask;
	}

	m_Settings.Set(TaskSettings::IgnoreSlideToPosition, true);

	CTask* pAdditionalTask = NULL;
	if(m_pAdditionalTaskDuringApproach)
	{
		aiTask* pCopy = m_pAdditionalTaskDuringApproach->Copy();
		taskAssert(dynamic_cast<CTask*>(pCopy));
		pAdditionalTask = static_cast<CTask*>(pCopy);
	}

	SetNewTask(rage_new CTaskComplexControlMovement(pSubtask, pAdditionalTask));
}

CTask::FSM_Return CTaskGoToScenario::GoToPosition_OnUpdate()
{
	// Check if the ped is close to the desination..
	if (IsClose())
	{
		m_fCloseTimer += GetTimeStep();
	}
	else
	{
		m_fCloseTimer = 0.0f;
	}

	if (m_fCloseTimer > sm_Tunables.m_ClosePointCounterMax)
	{
		// We were close to the destination for too long without actually getting there, consider this a failure.
		SetState(State_Failed);
		return FSM_Continue;
	}

	m_fBrokenPointTimer += GetTimeStep();

	if (m_fBrokenPointTimer > sm_Tunables.m_TimeBetweenBrokenPointChecks)
	{
		m_fBrokenPointTimer = 0.0f;
		CTask* pParentTask = GetParent();
		if (pParentTask && pParentTask->GetTaskType() == CTaskTypes::TASK_USE_SCENARIO)
		{
			CTaskUseScenario* pTaskScenarioParent = static_cast<CTaskUseScenario*>(pParentTask);
			const CScenarioPoint* pPoint = pTaskScenarioParent->GetScenarioPoint();

			if (pPoint && CScenarioManager::IsScenarioPointAttachedEntityUprootedOrBroken(*pPoint))
			{
				// The point has broken up or been moved.  Fail to prevent the ped from sitting down in a broken or tipped over chair.
				SetState(State_Failed);
				return FSM_Continue;
			}
		}
	}

	// Check if we have a valid subtask and if it is of the right type
	if (GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT &&	// Check if we are in the follow navmesh task
		(static_cast<CTaskComplexControlMovement*>(GetSubTask()))->GetBackupCopyOfMovementSubtask()->GetTaskType() == CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH )
	{
		// Get the movement task
		CTaskMoveFollowNavMesh* pNavmeshTask = (CTaskMoveFollowNavMesh*)((CTaskComplexControlMovement*)GetSubTask())->GetRunningMovementTask(GetPed());
		if (pNavmeshTask)
		{
			// if we need to update our target go ahead and do so
			if(m_bUpdateTarget)
			{
				m_WaitTimer.Unset();
				pNavmeshTask->SetTarget(GetPed(), VECTOR3_TO_VEC3V(m_Settings.GetTarget()), true);
				
				if (!m_Settings.IsSet(TaskSettings::IgnoreStopHeading))
				{
					pNavmeshTask->SetTargetStopHeading(m_Settings.GetHeading());
				}

				m_bUpdateTarget = false;
			}
			// If we have not found a route and we've exhausted our options, set us to the fail state
			else if( !pNavmeshTask->IsFollowingNavMeshRoute() &&
				pNavmeshTask->GetNavMeshRouteMethod() >= CTaskMoveFollowNavMesh::eRouteMethod_LastMethodWhichAvoidsObjects)
			{
				SetState(State_Failed);
				return FSM_Continue;
			}
		}
	}
	else if(m_bUpdateTarget)
	{
		// Restart the state if the subtask no longer exists and we need to update our target
		SetFlag(aiTaskFlags::RestartCurrentState);
		return FSM_Continue;
	}

	// When the task finished we need to wait to see if we get an updated position
	if(GetIsSubtaskFinished(CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT))
	{
		// if we have a desired clip and have not yet loaded the entry clip we will wait for a few seconds
		if(m_Settings.IsSet( TaskSettings::UsesEntryClip ) && !m_bEntryClipLoaded)
		{
			// If the timer is not set then make sure to set it
			if(!m_WaitTimer.IsSet())
			{
				TUNE_GROUP_INT(GOTO_SCENARIO_DEBUG, waitTime, 2781, 0, 30000, 1000);
				m_WaitTimer.Set(fwTimer::GetTimeInMilliseconds(), waitTime);
			}
			else if(m_WaitTimer.IsOutOfTime())
			{
				// if the timer runs out we fail because we our current position/heading does not include the clip offset
				SetState(State_Failed);
			}
		}
		else
		{
			static dev_bool bSkipSlide = false;
			if ( bSkipSlide || m_Settings.IsSet( TaskSettings::IgnoreSlideToPosition ))
			{
				SetState(State_Finish);
			}
			else
			{
				// slide to our coord if we finished the task and we've already loaded the clip (meaning we got to the desired offset)
				SetState(State_SlideToCoord);
			}
		}
	}

	CPed* pPed = GetPed();
	if (CScenarioSpeedHelper::ShouldUseSpeedMatching(*pPed))
	{
		CPed* pAmbientFriend = pPed->GetPedIntelligence()->GetAmbientFriend();
		if (pAmbientFriend)
		{
			float fMBR = CScenarioSpeedHelper::GetMBRToMatchSpeed(*pPed, *pAmbientFriend, VECTOR3_TO_VEC3V(m_Settings.GetTarget()));

			CTask* pTaskMovement = pPed->GetPedIntelligence()->GetActiveMovementTask();

			if (pTaskMovement)
			{
				pTaskMovement->PropagateNewMoveSpeed(fMBR);
			}
		}
	}

	return FSM_Continue;
}

void CTaskGoToScenario::SetNewTarget(const Vector3& vPosition, float fHeading)
{
	// Change the target point to start a new search.
	m_Settings.SetTarget(vPosition);
	m_Settings.SetHeading(fHeading);
	m_bUpdateTarget = true;
}

void CTaskGoToScenario::SlideToCoord_OnEnter()
{
	// Start our slide to coord task
	static float SLIDE_SPEED = 100.0f;
	SetNewTask(rage_new CTaskSlideToCoord(m_Settings.GetTarget(), m_Settings.GetHeading(), SLIDE_SPEED));
}

CTask::FSM_Return CTaskGoToScenario::SlideToCoord_OnUpdate()
{
	// If we've reached the position and the clips have loaded, play sit down clip
	if (GetIsSubtaskFinished(CTaskTypes::TASK_SLIDE_TO_COORD))
	{
		SetState(State_Finish);
	}

	return FSM_Continue;
}

#if !__FINAL
void CTaskGoToScenario::Debug() const
{
}

const char * CTaskGoToScenario::GetStaticStateName( s32 iState )
{
	taskAssert( iState >= State_GoToPosition && iState <= State_Finish);

	switch (iState)
	{
		case State_GoToPosition :	return "State_GoToPosition";
		case State_SlideToCoord :	return "State_SlideToCoord";
		case State_Failed		:	return "State_Failed";
		case State_Finish		:	return "State_Finish";	
		default:					taskAssertf(0,"Unhandled state name");
	}

	return "State_Invalid";
}
#endif

