#include "TaskVehiclePark.h"

// Rage headers
#include "grcore/debugdraw.h"

// Framework headers
#include "fwmaths/geomutil.h"

// Game headers
#include "Vehicles/Planes.h"
#include "Vehicles/Submarine.h"
#include "Vehicles/heli.h"
#include "vehicleAi/driverpersonality.h"
#include "VehicleAi/task/TaskVehicleGoTo.h"
#include "vehicleAi/task/TaskVehicleGoToAutomobile.h"
#include "VehicleAi/task/TaskVehicleCruise.h"
#include "VehicleAi/task/TaskVehicleTempAction.h"
#include "VehicleAi/VehicleIntelligence.h"
#include "Vehicles/Trailer.h"
#include "Peds/PedIntelligence.h"
#include "Task/Scenario/ScenarioChainingTests.h"
#include "Task/Scenario/ScenarioManager.h"
#include "Task/Scenario/Types/TaskVehicleScenario.h"
#include "Task/Vehicle/TaskExitVehicle.h"
#include "scene/world/gameWorld.h"


AI_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()

///////////////////////////////////////////////////////////////////////////////

CTaskVehicleStop::CTaskVehicleStop(u32 Stopflags)
: m_StopFlags(Stopflags)
{
	m_Params.m_fCruiseSpeed = 0.0f;

	if (Stopflags & SF_DontTerminateWhenStopped)
	{
		m_Params.m_iDrivingFlags.SetFlag(DF_DontTerminateTaskWhenAchieved);
	}

	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_STOP);
}


///////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return	CTaskVehicleStop::Stop_OnUpdate					(CVehicle* pVehicle)
{
	if(m_StopFlags&SF_SupressBrakeLight)
	{
		pVehicle->m_nVehicleFlags.bSuppressBrakeLight = true;
	}

	const bool bDontResetSteerAngle = (m_StopFlags&SF_DontResetSteerAngle) != 0;
	if(!pVehicle->InheritsFromBike() && !pVehicle->InheritsFromQuadBike() && !pVehicle->InheritsFromAmphibiousQuadBike() && !bDontResetSteerAngle)
	{
		pVehicle->SetSteerAngle(0.0f);
	}

	float fThrottle = 0.0f;

	bool bOppressor2 = MI_BIKE_OPPRESSOR2.IsValid() && pVehicle->GetModelIndex() == MI_BIKE_OPPRESSOR2;
	if (bOppressor2)
	{
		// Brake / handbrake have virtually no effect on a hover bike. In order for this task to work, apply some throttle in the opposite direction to our movement.
		float fSpeed = DotProduct(pVehicle->GetVelocityIncludingReferenceFrame(), VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetB()));
		TUNE_GROUP_FLOAT(VEHICLE_STOP, OPPRESSOR2_OPPOSITE_THROTTLE, 0.25f, 0.0f, 1.0f, 0.01f);
		fThrottle = fSpeed > 0.0f ? -OPPRESSOR2_OPPOSITE_THROTTLE : OPPRESSOR2_OPPOSITE_THROTTLE;
	}

	pVehicle->SetThrottle(fThrottle);

	bool bShouldUseFullBrake = (m_StopFlags & SF_UseFullBrake) || bOppressor2;
	pVehicle->SetBrake(bShouldUseFullBrake ? 1.0f : 0.5f);

	pVehicle->SetHandBrake(true);

	pVehicle->GetIntelligence()->UpdateCarHasReasonToBeStopped();

	m_bMissionAchieved = true;
	pVehicle->m_nVehicleFlags.bIsThisAParkedCar = true;

	VehicleType vehicleType = pVehicle->GetVehicleType();
	if( vehicleType == VEHICLE_TYPE_SUBMARINECAR && !pVehicle->IsInSubmarineMode())
	{
		vehicleType = VEHICLE_TYPE_CAR;
	}

	if( vehicleType == VEHICLE_TYPE_AMPHIBIOUS_AUTOMOBILE || 
		vehicleType == VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE )
	{
		vehicleType = VEHICLE_TYPE_CAR;
	}

	bool mightDoTimeslicing = true;
	switch(vehicleType)
	{
	case VEHICLE_TYPE_HELI:
	case VEHICLE_TYPE_BLIMP:
		((CHeli*)pVehicle)->SetYawControl(0.0f);
		((CHeli*)pVehicle)->SetPitchControl(0.0f);
		((CHeli*)pVehicle)->SetRollControl(0.0f);
		if( pVehicle->HasContactWheels() ||
			( pVehicle->pHandling->GetSeaPlaneHandlingData() && pVehicle->m_nFlags.bPossiblyTouchesWater ) )
		{
			( (CHeli*)pVehicle )->SetThrottleControl( 0.0f );
		}
		else
			((CHeli*)pVehicle)->SetThrottleControl(0.95f);
		mightDoTimeslicing = false;
		break;
	case VEHICLE_TYPE_BOAT:
		pVehicle->SetThrottle(ComputeThrottleForSpeed(*pVehicle, 0.0f));
		pVehicle->SetBrake(1.0f);
		break;
	case VEHICLE_TYPE_PLANE:
		((CPlane*)pVehicle)->SetYawControl(0.0f);
		((CPlane*)pVehicle)->SetPitchControl(0.0f);
		((CPlane*)pVehicle)->SetRollControl(0.0f);
		((CPlane*)pVehicle)->SetThrottleControl(0.0f);
		mightDoTimeslicing = false;
		break;
	case VEHICLE_TYPE_SUBMARINE:
	case VEHICLE_TYPE_SUBMARINECAR:
		{
			{
				CSubmarineHandling* pSubHandling = ((CSubmarine*)pVehicle)->GetSubHandling();
				Assertf(pSubHandling, "Sub handling should be valid");

				pSubHandling->SetPitchControl(0.0f);

				Vector3 vVelocity = pVehicle->GetVelocity();			
				vVelocity.Normalize();
				Vector3 vDirection = VEC3V_TO_VECTOR3(pVehicle->GetVehicleForwardDirection());
				float fVelocityDot = vDirection.Dot(vVelocity);
				if(fVelocityDot > 0.9f)
				{
					pVehicle->SetThrottle(ComputeThrottleForSpeed(*pVehicle, 0.0f));
				}
				else
				{
					// B*2023637: This behaviour is only for AI drivers tasked with stopping their vehicle.
					// Don't do it for players as it makes subs spin around oddly when the ped exits the vehicle.
					if (pVehicle->GetDriver() && !pVehicle->IsDriverAPlayer())
					{
						//Rotate to align with velocity
						Vector3 vehDriveDirXY(vDirection.x, vDirection.y, 0.0f);
						vehDriveDirXY.Normalize();
						const Vector3 steeringDeltaXY(vVelocity.x, vVelocity.y, 0.0f);
						Vector3 steeringDirXY(steeringDeltaXY);
						steeringDirXY.Normalize();

						Vector3 turnVec(0.0f, 0.0f, 0.0f);
						turnVec.Cross(vehDriveDirXY, steeringDirXY);
						float leftRightControl = turnVec.z * HALF_PI;
						float fYawControl = rage::Clamp(leftRightControl, -HALF_PI, HALF_PI);
						pSubHandling->SetYawControl(fYawControl);
					}

					static float s_DiveScaler = 0.05f;
					float fDiveControl = Clamp( -vVelocity.z * s_DiveScaler, -1.0f, 1.0f);
					pSubHandling->SetDiveControl(fDiveControl);
					pVehicle->SetThrottle(0.0f);
				}
			}
		}
	default:
		break;
	}

	if(pVehicle->GetAiXYSpeed() < 0.5f)//are we stopped? or close enough :)
	{
		if(!(m_StopFlags&SF_DontTerminateWhenStopped))
		{
			return FSM_Quit;//if so quit
		}

		if(mightDoTimeslicing && (m_StopFlags & SF_EnableTimeslicingWhenStill))
		{
			// We are basically sitting still, allow timeslicing.
			pVehicle->m_nVehicleFlags.bLodCanUseTimeslicing = true;
			pVehicle->m_nVehicleFlags.bLodShouldUseTimeslicing = true;
		}
	}

	return FSM_Continue;//keep braking
}


//////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskVehicleStop::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	CVehicle *pVehicle = GetVehicle(); //Get the vehicle ptr.

	FSM_Begin
	// Stop
	FSM_State(State_Stop)
		FSM_OnUpdate
		return Stop_OnUpdate(pVehicle);
	FSM_End
}

CTask::FSM_Return	CTaskVehicleStop::ProcessPreFSM()
{
	CVehicle *pVehicle = GetVehicle(); //Get the vehicle ptr.

	// stopping cars can become become dummies. Don't know why the law enforcement check is here, this was copied from old logic in the vehicle LOD manager
	if (!pVehicle->IsLawEnforcementCar())
	{
		pVehicle->m_nVehicleFlags.bTasksAllowDummying = true;
	}

	return FSM_Continue;
}

void CTaskVehicleStop::CloneUpdate(CVehicle* pVehicle)
{
    CTaskVehicleMissionBase::CloneUpdate(pVehicle);

    pVehicle->m_nVehicleFlags.bTasksAllowDummying = true;
}

void CTaskVehicleStop::CleanUp()
{
	CVehicle *pVehicle = GetVehicle(); //Get the vehicle ptr.
	if ( pVehicle )
	{
		// prevent boats having negative throttle
		pVehicle->SetThrottle(0.0f);
	}

}

float CTaskVehicleStop::ComputeThrottleForSpeed(CVehicle& in_Vehicle, float in_Speed)
{
	Vector3 vVehicleVelocity = in_Vehicle.GetVelocity();
	Vector3 vVehicleForward = VEC3V_TO_VECTOR3(in_Vehicle.GetTransform().GetForward());
	float fSpeed = vVehicleVelocity.XYMag();

	float fForwardSpeed = 0.0f;
	if (fSpeed > SMALL_FLOAT)
	{
		Vector3 vVehicleDirection = vVehicleVelocity * 1.0f / fSpeed;
		fForwardSpeed = vVehicleDirection.Dot(vVehicleForward) * fSpeed;
	}

	float fDeltaSpeed = in_Speed - fForwardSpeed;

	static float s_SpeedScalar = 0.05f;
	return Clamp( fDeltaSpeed * s_SpeedScalar, -1.0f, 1.0f);
}


//////////////////////////////////////////////////////////////////////

#if !__FINAL
const char * CTaskVehicleStop::GetStaticStateName( s32 iState  )
{
	Assert(iState>=State_Stop&&iState<=State_Stop);
	static const char* aStateNames[] = 
	{
		"State_Stop"
	};

	return aStateNames[iState];
}
#endif


//////////////////////////////////////////////////////////////////////

CTaskVehiclePullOver::CTaskVehiclePullOver(bool bForcePullOverRightAway, ForcePulloverDirection forceDirection, bool bJustPullOverToSideOfCurrentLane)
: m_bForcePullOverRightAway(bForcePullOverRightAway)
, m_ForcePulloverInDirection(forceDirection)
, m_bJustPullOverToSideOfCurrentLane(bJustPullOverToSideOfCurrentLane)
{
	m_bRightSideOfRoad = false;
	m_bFirstUpdateInCruise = true;

	m_pFollowRoute = rage_new CVehicleFollowRouteHelper();

	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_PULL_OVER);
}

CTask::FSM_Return CTaskVehiclePullOver::ProcessPreFSM()
{
	UpdateIndicatorsForCar(GetVehicle());
	return FSM_Continue;
}

void CTaskVehiclePullOver::CleanUp()
{
	if (GetVehicle())
	{
		GetVehicle()->m_nVehicleFlags.bLeftIndicator	= false;
		GetVehicle()->m_nVehicleFlags.bRightIndicator	= false;
	}
}

//////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskVehiclePullOver::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	CVehicle *pVehicle = GetVehicle(); //Get the vehicle ptr.

	FSM_Begin

		// Cruise
		FSM_State(State_Cruise)
			FSM_OnEnter
				Cruise_OnEnter(pVehicle);
			FSM_OnUpdate
				return Cruise_OnUpdate(pVehicle);

		// HeadForTarget
		FSM_State(State_HeadForTarget)
			FSM_OnEnter
				HeadForTarget_OnEnter(pVehicle);
			FSM_OnUpdate
				return HeadForTarget_OnUpdate(pVehicle);

		FSM_State(State_Slowdown)
			FSM_OnEnter
				Slowdown_OnEnter(pVehicle);
			FSM_OnUpdate
				return Slowdown_OnUpdate(pVehicle);

		// Stop
		FSM_State(State_Stop)
			FSM_OnEnter
				Stop_OnEnter(pVehicle);
			FSM_OnUpdate
				return Stop_OnUpdate(pVehicle);
	FSM_End
}


//////////////////////////////////////////////////////////////////////
//Cruise
//////////////////////////////////////////////////////////////////////
void		CTaskVehiclePullOver::Cruise_OnEnter(CVehicle* pVehicle)
{
	//remember whether this is the first update in this state or not, so we don't
	//exit early for not having a node list
	m_bFirstUpdateInCruise = true;

	SetNewTask( CVehicleIntelligence::CreateCruiseTask(*pVehicle, m_Params));
}


//////////////////////////////////////////////////////////////////////

CTask::FSM_Return	CTaskVehiclePullOver::Cruise_OnUpdate(CVehicle* pVehicle)
{
	if (m_bFirstUpdateInCruise)
	{
		m_bFirstUpdateInCruise = false;
		return FSM_Continue;
	}

	CTaskVehicleCruiseNew* pSubtask = static_cast<CTaskVehicleCruiseNew*>(FindSubTaskOfType(CTaskTypes::TASK_VEHICLE_CRUISE_NEW));
	CVehicleNodeList * pNodeList = pSubtask ? pSubtask->GetNodeList() : NULL;
	if(!pNodeList)
	{
		SetState(State_Stop);
		return FSM_Continue;
	}

	Vector3	TargetCoors;
	CNodeAddress oldNode, newNode, futureNode;

	Assert(pNodeList->GetTargetNodeIndex() > 0);	//this should never be 0
	oldNode = pNodeList->GetPathNodeAddr(pNodeList->GetTargetNodeIndex()-1);
	newNode = pNodeList->GetPathNodeAddr(pNodeList->GetTargetNodeIndex());
	futureNode = pNodeList->GetPathNodeAddr(rage::Min(pNodeList->GetTargetNodeIndex()+1, CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED-1));

	if(oldNode.IsEmpty() || newNode.IsEmpty() ||(!ThePaths.IsRegionLoaded(oldNode)) ||(!ThePaths.IsRegionLoaded(newNode)))
	{
		SetState(State_Stop);
		return FSM_Continue;
	}

	const CPathNode *pOldNode = ThePaths.FindNodePointer(oldNode);
	const CPathNode *pNewNode = ThePaths.FindNodePointer(newNode);

	s16 iLink = -1;
	const bool bLinkFound = ThePaths.FindNodesLinkIndexBetween2Nodes(oldNode, newNode, iLink);
	if(!bLinkFound)//can't find the link so just stop.
	{
		SetState(State_Stop);
		return FSM_Continue;
	}

	const CPathNodeLink *pLink = &ThePaths.GetNodesLink(pOldNode, iLink);

	const CPathNode *pFutureNode = 0;
	if(!futureNode.IsEmpty() && ThePaths.IsRegionLoaded(futureNode))
	{
		pFutureNode = ThePaths.FindNodePointer(futureNode);
	}

	Vector2 oldNodeCoors;
	Vector2 newNodeCoors;

	pOldNode->GetCoors2(oldNodeCoors);
	pNewNode->GetCoors2(newNodeCoors);

	Vector2	diff = newNodeCoors - oldNodeCoors;
	diff.Normalize();

	// First we wait until we have a sufficiently straight bit of road.
	// Also, don't pull over on a sliplane
	//if(pOldNode->FindNumberNonSpecialNeighbours() == 2 && pNewNode->FindNumberNonSpecialNeighbours() == 2 &&((!pFutureNode)||(pFutureNode->FindNumberNonSpecialNeighbours() == 2)))
	const CJunction* pOldJunction = !pOldNode->IsJunctionNode() ? NULL : CJunctions::FindJunctionFromNode(oldNode);
	const CJunction* pNewJunction = !pNewNode->IsJunctionNode() ? NULL : CJunctions::FindJunctionFromNode(newNode);
	const CJunction* pFutureJunction = !pFutureNode || !pFutureNode->IsJunctionNode() ? NULL : CJunctions::FindJunctionFromNode(futureNode);
	const bool bOldNodeIsNonFakeJunction = pOldJunction && !pOldJunction->IsOnlyJunctionBecauseHasSwitchedOffEntrances();
	const bool bNewNodeIsNonFakeJunction = pNewJunction && !pNewJunction->IsOnlyJunctionBecauseHasSwitchedOffEntrances();
	const bool bFutureNodeIsNonFakeJunction = pFutureJunction && !pFutureJunction->IsOnlyJunctionBecauseHasSwitchedOffEntrances();
	if (m_bForcePullOverRightAway || (!bOldNodeIsNonFakeJunction && !pOldNode->IsSlipLane() 
		&& !bNewNodeIsNonFakeJunction && !pNewNode->IsSlipLane()
		&& !pLink->IsDontUseForNavigation()
		&& (!pFutureNode || (!bFutureNodeIsNonFakeJunction && !pFutureNode->IsSlipLane()))))
	{
		m_bRightSideOfRoad = true;
		// Only if we are on a one way street will we consider stopping on the left side of the road.
		//TODO: differentiate between one way roads and roads that have been split
		if(pLink->m_1.m_LanesFromOtherNode == 0)
		{
			// Bit of a fudge to get taxis to stop near the player
			Vector3	playerCoors = CGameWorld::FindLocalPlayerCoors();
			const Vector3 vVehiclePosition = VEC3V_TO_VECTOR3(pVehicle->GetVehiclePosition());
			if(CVehicle::IsTaxiModelId(pVehicle->GetModelId()) &&((playerCoors - vVehiclePosition).Mag2() < 40.0f * 40.0f))
			{
				if(diff.Cross(Vector2(playerCoors, Vector2::kXY)-oldNodeCoors) > 0)
				{
					m_bRightSideOfRoad = false;
				}
			}
			else if(diff.Cross(Vector2(vVehiclePosition, Vector2::kXY)-oldNodeCoors) > 0)
			{
				m_bRightSideOfRoad = false;
			}
		}

		//override
		if (m_ForcePulloverInDirection == Force_Left)
		{
			m_bRightSideOfRoad = false;
		}
		else if (m_ForcePulloverInDirection == Force_Right)
		{
			m_bRightSideOfRoad = true;
		}
		CopyNodeList(pNodeList);
		SetState(State_HeadForTarget);
		return FSM_Continue;
	}

	return FSM_Continue;
}

void CTaskVehiclePullOver::CopyNodeList(const CVehicleNodeList * pNodeList)
{
	if(pNodeList)
	{
		//Assert(m_pNodeList);
		sysMemCpy(&m_NodeList, pNodeList, sizeof(CVehicleNodeList));

		Assert(m_pFollowRoute);
		m_pFollowRoute->ConstructFromNodeList(GetVehicle(), m_NodeList);
	}
}

//////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////
float CTaskVehiclePullOver::GetPointToAimFor(CVehicle *pVehicle, Vector3 *pPointToAimFor, float& fDotProductOut)
{
	CVehicleNodeList * pNodeList = pVehicle->GetIntelligence()->GetNodeList();
	Assertf(pNodeList, "CTaskVehiclePullOver::GetPointToAimFor - vehicle has no node list");
	if(!pNodeList)
		return 0.0f;

	Vector3	TargetCoors;
	CNodeAddress oldNode, newNode, futureNode;

	Assert(pNodeList->GetTargetNodeIndex() > 0);	//this should never be 0
	oldNode = pNodeList->GetPathNodeAddr(pNodeList->GetTargetNodeIndex()-1);
	newNode = pNodeList->GetPathNodeAddr(pNodeList->GetTargetNodeIndex());
	futureNode = pNodeList->GetPathNodeAddr(rage::Min(pNodeList->GetTargetNodeIndex()+1, CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED-1));

	if(oldNode.IsEmpty() || newNode.IsEmpty() ||(!ThePaths.IsRegionLoaded(oldNode)) ||(!ThePaths.IsRegionLoaded(newNode)))
	{
		SetState(State_Stop);
		return 0.0f;
	}

	const CPathNode *pOldNode = ThePaths.FindNodePointer(oldNode);
	const CPathNode *pNewNode = ThePaths.FindNodePointer(newNode);

	s16 iLink = -1;
	const bool bLinkFound = ThePaths.FindNodesLinkIndexBetween2Nodes(oldNode, newNode, iLink);
	if(!bLinkFound)//can't find the link so just stop.
	{
		SetState(State_Stop);
		return 0.0f;
	}

	const CPathNodeLink *pLink = &ThePaths.GetNodesLink(pOldNode, iLink);

	Vector2 oldNodeCoors;
	Vector2 newNodeCoors;

	pOldNode->GetCoors2(oldNodeCoors);
	pNewNode->GetCoors2(newNodeCoors);

	Vector2	diff = newNodeCoors - oldNodeCoors;
	diff.Normalize();

	Vector3 vCarForward = VEC3V_TO_VECTOR3(pVehicle->GetVehicleForwardDirection());
	vCarForward.z = 0.0f;
	fDotProductOut = vCarForward.Dot(Vector3(diff.x, diff.y, 0.0f));

	// Find the coordinates of the curb(where we want to stop)
	float roadWidthOnLeft, roadWidthOnRight;
#if __DEV
	const bool bAllLanesThoughCentre = ThePaths.bMakeAllRoadsSingleTrack ||(pLink->m_1.m_Width == ALL_LANES_THROUGH_CENTRE_FLAG_VAL);
#else
	const bool bAllLanesThoughCentre = (pLink->m_1.m_Width == ALL_LANES_THROUGH_CENTRE_FLAG_VAL);
#endif // __DEV
	ThePaths.FindRoadBoundaries(pLink->m_1.m_LanesToOtherNode, pLink->m_1.m_LanesFromOtherNode, static_cast<float>(pLink->m_1.m_Width), pLink->m_1.m_NarrowRoad, bAllLanesThoughCentre, roadWidthOnLeft, roadWidthOnRight);

	float	centreToCurbDist;
	const float fCarWidth = pVehicle->GetBoundingBoxMax().x;
	if(m_bRightSideOfRoad)
	{
		centreToCurbDist = roadWidthOnRight;
		centreToCurbDist -= fCarWidth;
		centreToCurbDist = rage::Max(0.0f, centreToCurbDist);
	}
	else
	{
		centreToCurbDist = -roadWidthOnLeft;
		centreToCurbDist += fCarWidth;
		centreToCurbDist = rage::Min(0.0f, centreToCurbDist);
	}

	Vector2 curbPoint = oldNodeCoors;
	curbPoint.x += centreToCurbDist * diff.y;
	curbPoint.y -= centreToCurbDist * diff.x;

	//draw the curb line
	//Vector3 vCurbStart(curbPoint.x, curbPoint.y, pOldNode->GetPos().z);
	//CVehicle::ms_debugDraw.AddLine(VECTOR3_TO_VEC3V(vCurbStart), VECTOR3_TO_VEC3V(vCurbStart + Vector3(diff.x, diff.y, 0.0f)), Color_yellow);

	// Find our cars' coordinate projected onto the curb line
	const Vector3 vVehiclePosition = VEC3V_TO_VECTOR3(pVehicle->GetVehiclePosition());
	Vector2	carPos = Vector2(vVehiclePosition, Vector2::kXY);
	float	distAlongLine = diff.Dot(carPos - curbPoint);
	Vector2 carOnCurbLine;
	carOnCurbLine.x = curbPoint.x + distAlongLine * diff.x;
	carOnCurbLine.y = curbPoint.y + distAlongLine * diff.y;

	//CVehicle::ms_debugDraw.AddSphere(Vec3V(carOnCurbLine.x, carOnCurbLine.y, pOldNode->GetPos().z), 0.33f, Color_green);

	float	distFromCurbLine =(carPos - carOnCurbLine).Mag();

	//...hack!
	//if we want to just pull over to the side of our current lane
	//subtract the width of a lane until distFromCurbLine < lane width
	if (m_bJustPullOverToSideOfCurrentLane)
	{
		while (distFromCurbLine > pLink->GetLaneWidth())
		{
			distFromCurbLine -= pLink->GetLaneWidth();
		}
	}

	// We try to aim for a point a bit further on the line.
	float	forwardAim = rage::Max(4.0f, distFromCurbLine * 2.3f);
	if (m_bForcePullOverRightAway && m_bJustPullOverToSideOfCurrentLane)
	{
		static dev_float fCopSirenPulloverMult = 2.0f;
		forwardAim *= fCopSirenPulloverMult;
	}

	Vector2 pointToAimFor = carOnCurbLine + diff * forwardAim;

	pPointToAimFor->x = pointToAimFor.x;
	pPointToAimFor->y = pointToAimFor.y; 
	pPointToAimFor->z = 0.0f;

	//CVehicle::ms_debugDraw.AddSphere(Vec3V(pointToAimFor.x, pointToAimFor.y, pOldNode->GetPos().z), 0.33f, Color_purple);

	// Print a bunch of debug lines.
#if __DEV
	if(CVehicleIntelligence::m_bDisplayCarAiDebugInfo && CVehicleIntelligence::AllowCarAiDebugDisplayForThisVehicle(pVehicle))
	{
		float	displayZ = vVehiclePosition.z + 2.0f;
		grcDebugDraw::Line(Vector3(carOnCurbLine.x, carOnCurbLine.y, displayZ), 
			Vector3(carOnCurbLine.x, carOnCurbLine.y, displayZ), 
			Color32(255, 0, 0, 255));
		grcDebugDraw::Line(Vector3(pPointToAimFor->x, pPointToAimFor->y, displayZ), 
			Vector3(pPointToAimFor->x, pPointToAimFor->y, displayZ), 
			Color32(255, 255, 0, 255));
		grcDebugDraw::Line(Vector3(vVehiclePosition.x, vVehiclePosition.y, displayZ), 
			Vector3(pPointToAimFor->x, pPointToAimFor->y, displayZ), 
			Color32(255, 255, 255, 255));

	}
#endif // __DEV

	return distFromCurbLine;
}

//
//
// HeadForTarget
void		CTaskVehiclePullOver::HeadForTarget_OnEnter(CVehicle* pVehicle)
{
	Vector3 pointToAimFor;
	float fDot = 0.0f;
	GetPointToAimFor(pVehicle, &pointToAimFor, fDot);

	sVehicleMissionParams params;
	params.m_fCruiseSpeed = GetCruiseSpeed();
	params.SetTargetPosition(pointToAimFor);
	params.m_fTargetArriveDist = 0.0f;
	params.m_iDrivingFlags.ClearFlag(DF_SteerAroundStationaryCars);	//don't steer around parked cars if there's one in the way

	SetNewTask(rage_new CTaskVehicleGoToPointWithAvoidanceAutomobile(params));
}

bool	CTaskVehiclePullOver::HeadingIsWithinToleranceForVehicle(CVehicle* pVehicle, float fDot) const
{
	const float fDrivingAbility = CDriverPersonality::FindDriverAbility(pVehicle->GetDriver(), pVehicle);

	//simple case-based logic
	if (fDrivingAbility < 0.35f)
	{	
		return true;
	}
	else if (fDrivingAbility < 0.75f)
	{
		return fDot > 0.96f;
	}
	else
	{
		return fDot > 0.99f;
	}
}

//////////////////////////////////////////////////////////////////////
CTask::FSM_Return	CTaskVehiclePullOver::HeadForTarget_OnUpdate(CVehicle* pVehicle)
{
	Vector3 pointToAimFor;
	float fDot = 0.0f;
	float distFromCurbLine = GetPointToAimFor(pVehicle, &pointToAimFor, fDot);

	// Limit the speed depending on our distance to the curb.
	SetCruiseSpeed(rage::Min(GetCruiseSpeed(), 10.0f));
	if(distFromCurbLine < 2.0f)
	{
		SetCruiseSpeed(rage::Min(GetCruiseSpeed(), 5.0f));
	}

	//get the goto task and update it with the latest point to aim for.
	if(GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_VEHICLE_GOTO_POINT_WITH_AVOIDANCE_AUTOMOBILE)
	{
		CTaskVehicleGoTo * pGoToTask = (CTaskVehicleGoToPointWithAvoidanceAutomobile*)GetSubTask();

		pGoToTask->SetTargetPosition(&pointToAimFor);
	}

	//check whether we have reached our goal
	if(pVehicle->GetAiXYSpeed() < 0.5f || 
		(distFromCurbLine < 0.5f && HeadingIsWithinToleranceForVehicle(pVehicle, fDot)))
	{
		SetState(State_Slowdown);
	}

	return FSM_Continue;
}

//stop, but in a way that allows us to keep steering
void	CTaskVehiclePullOver::Slowdown_OnEnter(CVehicle* pVehicle)
{
	Vector3 pointToAimFor;
	float fDot = 0.0f;
	GetPointToAimFor(pVehicle, &pointToAimFor, fDot);

	sVehicleMissionParams params;
	params.m_fCruiseSpeed = 0.0f;
	params.SetTargetPosition(pointToAimFor);
	params.m_iDrivingFlags.ClearFlag(DF_SteerAroundStationaryCars);	//don't steer around parked cars if there's one in the way
	SetNewTask(rage_new CTaskVehicleGoToPointWithAvoidanceAutomobile(params));
}

aiTask::FSM_Return CTaskVehiclePullOver::Slowdown_OnUpdate(CVehicle* pVehicle)
{
	if (pVehicle->GetAiXYSpeed() < 0.25f || GetIsSubtaskFinished(CTaskTypes::TASK_VEHICLE_GOTO_POINT_WITH_AVOIDANCE_AUTOMOBILE))
	{
		SetState(State_Stop);
		return FSM_Continue;
	}

	//get the goto task and update it with the latest point to aim for.
	if(GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_VEHICLE_GOTO_POINT_WITH_AVOIDANCE_AUTOMOBILE)
	{
		CTaskVehicleGoTo * pGoToTask = (CTaskVehicleGoToPointWithAvoidanceAutomobile*)GetSubTask();

		Vector3 pointToAimFor;
		float fDot = 0.0f;
		GetPointToAimFor(pVehicle, &pointToAimFor, fDot);

		pGoToTask->SetTargetPosition(&pointToAimFor);
	}

	//CVehicle::ms_debugDraw.AddLine(pVehicle->GetVehiclePosition(), pVehicle->GetVehiclePosition() + Vec3V(0.0f, 0.0f, 5.0f), Color_orange);

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////
//State_Stop
//////////////////////////////////////////////////////////////////////
void		CTaskVehiclePullOver::Stop_OnEnter(CVehicle* UNUSED_PARAM(pVehicle))
{
	SetNewTask(rage_new CTaskVehicleStop());
}

/////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return	CTaskVehiclePullOver::Stop_OnUpdate(CVehicle* UNUSED_PARAM(pVehicle))
{
	if (GetIsSubtaskFinished(CTaskTypes::TASK_VEHICLE_STOP))
	{
		return FSM_Quit;
	}
	return FSM_Continue;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : UpdateIndicatorsForCar
// PURPOSE :  Sets the indicator bits in the Vehicle flags if the car is planning
//			  to turn off and not on a mission.
/////////////////////////////////////////////////////////////////////////////////
void CTaskVehiclePullOver::UpdateIndicatorsForCar(CVehicle* pVeh)
{
	Assert(pVeh);

	if(m_bRightSideOfRoad)//!m_bCommitted 
	{
		pVeh->m_nVehicleFlags.bRightIndicator = true;
		pVeh->m_nVehicleFlags.bLeftIndicator	= false;
	}
	else
	{
		pVeh->m_nVehicleFlags.bLeftIndicator = true;
		pVeh->m_nVehicleFlags.bRightIndicator	= false;
	}
}



//////////////////////////////////////////////////////////////////////

#if !__FINAL
const char * CTaskVehiclePullOver::GetStaticStateName( s32 iState  )
{
	Assert(iState>=State_Cruise&&iState<=State_Stop);
	static const char* aStateNames[] = 
	{
		"State_Cruise",
		"State_HeadForTarget",
		"State_Slowdown",
		"State_Stop",
	};

	return aStateNames[iState];
}
#endif

//////////////////////////////////////////////////////////////////////

CTaskVehiclePassengerExit::CTaskVehiclePassengerExit(const Vector3& target)
: m_vTarget(target)
, m_uNumPassengersToExit(0)
{
	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_PASSENGER_EXIT);
}

CTask::FSM_Return CTaskVehiclePassengerExit::ProcessPreFSM()
{
	return FSM_Continue;
}

CTask::FSM_Return CTaskVehiclePassengerExit::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	CVehicle *pVehicle = GetVehicle(); //Get the vehicle ptr.

	FSM_Begin
		FSM_State(State_GoToTarget)
		FSM_OnEnter
		GoToTarget_OnEnter(pVehicle);
	FSM_OnUpdate
		return GoToTarget_OnUpdate(pVehicle);

	FSM_State(State_Stop)
		FSM_OnEnter
		Stop_OnEnter(pVehicle);
	FSM_OnUpdate
		return Stop_OnUpdate(pVehicle);

	FSM_State(State_WaitForPassengersToExit)
		FSM_OnEnter
		WaitForPassengersToExit_OnEnter(pVehicle);
	FSM_OnUpdate
		return WaitForPassengersToExit_OnUpdate(pVehicle);
	FSM_End
}

void CTaskVehiclePassengerExit::GoToTarget_OnEnter(CVehicle* UNUSED_PARAM(pVehicle))
{
	sVehicleMissionParams params = m_Params;
	params.m_fTargetArriveDist = 1.f;
	params.SetTargetPosition(m_vTarget);
	SetNewTask(rage_new CTaskVehicleGoToPointWithAvoidanceAutomobile(params));
}

CTask::FSM_Return	CTaskVehiclePassengerExit::GoToTarget_OnUpdate(CVehicle* UNUSED_PARAM(pVehicle))
{
	if (GetIsSubtaskFinished(CTaskTypes::TASK_VEHICLE_GOTO_POINT_WITH_AVOIDANCE_AUTOMOBILE))
	{
		SetState(State_Stop);
	}

	return FSM_Continue;
}

void	CTaskVehiclePassengerExit::Stop_OnEnter(CVehicle* UNUSED_PARAM(pVehicle))
{
	SetNewTask(rage_new CTaskVehicleStop());
}

CTask::FSM_Return	CTaskVehiclePassengerExit::Stop_OnUpdate(CVehicle* UNUSED_PARAM(pVehicle))
{
	if (GetIsSubtaskFinished(CTaskTypes::TASK_VEHICLE_STOP))
	{
		SetState(State_WaitForPassengersToExit);
	}
	return FSM_Continue;
}

void				CTaskVehiclePassengerExit::WaitForPassengersToExit_OnEnter(CVehicle* pVehicle)
{
	//Figure out how many passengers should exit.
	// We'll use this scenario point's probability to determine how many passengers leave, 
	// i.e. 50% = half of passengers get off.
	m_uNumPassengersToExit = 0;
	s32 iTotalPassengers = pVehicle->GetNumberOfPassenger();
	if (iTotalPassengers > 0)
	{
		CScenarioPoint* pCurrentPoint = GetDriverScenarioPoint(pVehicle);
		if (pCurrentPoint)
		{
			float fPercentageToExit;
			if (pCurrentPoint->HasProbabilityOverride())
			{
				fPercentageToExit = pCurrentPoint->GetProbabilityOverride();
			}
			else
			{
				const float fMinPercentage = 0.25f;
				const float fMaxPercentage = 0.75f;
				fPercentageToExit = fwRandom::GetRandomNumberInRange(fMinPercentage, fMaxPercentage);
			}
			m_uNumPassengersToExit = static_cast<u32>(ceilf(fPercentageToExit * static_cast<float>(iTotalPassengers)));

		}
	}

	//Tell the passengers to get off.
	if (m_uNumPassengersToExit > 0)
	{
		m_exitingPeds.Reserve(m_uNumPassengersToExit);
		atArray<int> aiSeatIndecesToExit;
		pVehicle->PickRandomPassengers(m_uNumPassengersToExit, aiSeatIndecesToExit);

		for (int iSeatIndex = 0; iSeatIndex < aiSeatIndecesToExit.size(); ++iSeatIndex)
		{
			CPed* pPedToExit = pVehicle->GetPedInSeat(aiSeatIndecesToExit[iSeatIndex]);
			GivePedExitTask(pPedToExit, pVehicle);
			RegdPed regdExitingPed(pPedToExit);
			m_exitingPeds.Push(regdExitingPed);
		}
	}
}

CTask::FSM_Return  CTaskVehiclePassengerExit::WaitForPassengersToExit_OnUpdate(CVehicle* UNUSED_PARAM(pVehicle))
{
	// Keep track of passengers as they leave.
	for (int scan = m_exitingPeds.size() - 1; scan >= 0; --scan)
	{
		if (m_exitingPeds[scan] == NULL)
		{
			m_exitingPeds.DeleteFast(scan);
		}
		else if ( !m_exitingPeds[scan]->GetPedIntelligence()->HasEventOfType(EVENT_GIVE_PED_TASK) &&
			NULL == m_exitingPeds[scan]->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_EXIT_VEHICLE) )
		{
			// This ped has exited, so delete it off the list.
			m_exitingPeds.DeleteFast(scan);
		}
	}

	if (m_exitingPeds.size() > 0)
	{	
		return FSM_Continue;
	}
	else
	{
		return FSM_Quit;
	}
}

CScenarioPoint* CTaskVehiclePassengerExit::GetDriverScenarioPoint(const CVehicle* pVehicle) const
{
	if (pVehicle)
	{
		CPed* pDriver = pVehicle->GetDriver();
		if (pDriver && pDriver->GetPedIntelligence())
		{
			CTaskUseVehicleScenario* pVehicleScenarioTask = static_cast<CTaskUseVehicleScenario*>(pDriver->GetPedIntelligence()->FindTaskByType(CTaskTypes::TASK_USE_VEHICLE_SCENARIO));
			if (pVehicleScenarioTask)
			{
				return pVehicleScenarioTask->GetScenarioPoint();
			}
		}
	}
	return NULL;
}

void CTaskVehiclePassengerExit::GivePedExitTask(CPed* pPed, CVehicle* pVehicle) const
{
	if (aiVerify(pPed && pPed->GetPedIntelligence()))
	{
		VehicleEnterExitFlags vehicleFlags;
		vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::ExitToWalk);
		CTask* pLeaveCarTask = rage_new CTaskExitVehicle(pVehicle, vehicleFlags);
		CEventGivePedTask givePedTask(PED_TASK_PRIORITY_EVENT_RESPONSE_TEMP, pLeaveCarTask);
		pPed->GetPedIntelligence()->AddEvent(givePedTask);
	}
}

/////////////////////////////////////////////////////////////////////
//	CTaskVehicleParkNew
/////////////////////////////////////////////////////////////////////

CTaskVehicleParkNew::Tunables CTaskVehicleParkNew::sm_Tunables;
IMPLEMENT_VEHICLE_AI_TASK_TUNABLES(CTaskVehicleParkNew, 0xff036ca8);

// TODO: Would be nice to move these into CTaskVehicleParkNew::Tunables.
dev_float CTaskVehicleParkNew::ms_fCruiseSpeedToParkBoat = 6.0f;
dev_float CTaskVehicleParkNew::ms_fCruiseSpeedToPark = 4.0f;
dev_float CTaskVehicleParkNew::ms_fParkingSpaceHalfLength = 2.0f;
dev_float CTaskVehicleParkNew::ms_fParkingSpaceHalfWidth = 1.2f;
dev_float CTaskVehicleParkNew::ms_fPositionXTolerance = 0.55f;
dev_float CTaskVehicleParkNew::ms_fPositionYTolerance = 0.75f;
dev_float CTaskVehicleParkNew::ms_fPositionXToleranceBike = 1.1f;
dev_float CTaskVehicleParkNew::ms_fPositionYToleranceBike = 1.5f;
dev_float CTaskVehicleParkNew::ms_fPositionXTolerancePlane = 4.0f;
dev_float CTaskVehicleParkNew::ms_fPositionYTolerancePlane = 16.0f;
dev_u16 CTaskVehicleParkNew::ms_iParkingStuckTime = 1000;

CTaskVehicleParkNew::CTaskVehicleParkNew(const sVehicleMissionParams& params, const Vector3& direction, const ParkType parkType
	, const float toleranceRadians, const bool bKeepLightsOn)
: CTaskVehicleMissionBase(params)
, m_SpaceDir(direction)
, m_ParkType(parkType)
, m_HeadingToleranceRadians(toleranceRadians)
, m_NumTimesParkingSpaceBlocked(0)
, m_bKeepLightsOnAfterParking(bKeepLightsOn)
, m_iMaxPathSearchDistance(-1)
, m_bDoTestsForBlockedSpace(false)
{
	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_PARK_NEW);
}

CTaskVehicleParkNew::~CTaskVehicleParkNew()
{
}

aiTask* CTaskVehicleParkNew::Copy() const
{
	CTaskVehicleParkNew* pTaskCopy = rage_new CTaskVehicleParkNew(m_Params, m_SpaceDir, m_ParkType, m_HeadingToleranceRadians, m_bKeepLightsOnAfterParking);
	if(pTaskCopy)
	{
		pTaskCopy->SetMaxPathSearchDistance(GetMaxPathSearchDistance());
		pTaskCopy->SetDoTestsForBlockedSpace(GetDoTestsForBlockedSpace());
	}
	return pTaskCopy;
}

CTask::FSM_Return CTaskVehicleParkNew::ProcessPreFSM()
{
	CVehicle *pVehicle = GetVehicle(); //Get the vehicle ptr.
	if (pVehicle)
	{
		pVehicle->m_nVehicleFlags.bPreventBeingSuperDummyThisFrame = true;
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskVehicleParkNew::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CVehicle *pVehicle = GetVehicle(); 

	FSM_Begin
		// Start
		FSM_State(State_Start)
			FSM_OnEnter
				Start_OnEnter(pVehicle);
			FSM_OnUpdate
				return Start_OnUpdate(pVehicle);

		// Navigate
		FSM_State(State_Navigate)
			FSM_OnEnter
				Navigate_OnEnter(pVehicle);
			FSM_OnUpdate
				return Navigate_OnUpdate(pVehicle);

		// ForwardIntoSpace
		FSM_State(State_ForwardIntoSpace)
			FSM_OnEnter
				ForwardIntoSpace_OnEnter(pVehicle);
			FSM_OnUpdate
				return ForwardIntoSpace_OnUpdate(pVehicle);

		// BackIntoSpace
		FSM_State(State_BackIntoSpace)
			FSM_OnEnter
				BackIntoSpace_OnEnter(pVehicle);
			FSM_OnUpdate
				return BackIntoSpace_OnUpdate(pVehicle);

		// BackAwayFromSpace
		FSM_State(State_BackAwayFromSpace)
			FSM_OnEnter
				BackAwayFromSpace_OnEnter(pVehicle);
			FSM_OnUpdate
				return BackAwayFromSpace_OnUpdate(pVehicle);

		// PullForwardParallel
		FSM_State(State_PullForward)
			FSM_OnEnter
				PullForward_OnEnter(pVehicle);
			FSM_OnUpdate
				return PullForward_OnUpdate(pVehicle);

		FSM_State(State_PullOver)
			FSM_OnEnter
				PullOver_OnEnter(pVehicle);
			FSM_OnUpdate
				return PullOver_OnUpdate(pVehicle);

		FSM_State(State_PassengerExit)
			FSM_OnEnter
				PassengerExit_OnEnter(pVehicle);
			FSM_OnUpdate
				return PassengerExit_OnUpdate(pVehicle);

		// Stop
		FSM_State(State_Stop)
			FSM_OnEnter
				Stop_OnEnter(pVehicle);
			FSM_OnUpdate
				return Stop_OnUpdate(pVehicle);

		// Failed
		FSM_State(State_Failed)
			FSM_OnEnter
				Failed_OnEnter();
			FSM_OnUpdate
				return Failed_OnUpdate();

	FSM_End
}

#if !__FINAL
const char * CTaskVehicleParkNew::GetStaticStateName( s32 iState  )
{
	Assert(iState>=State_Start&&iState<=State_Failed);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_Navigate",
		"State_ForwardIntoSpace",
		"State_BackIntoSpace",
		"State_BackAwayFromSpace",
		"State_PullForward",		//before backing in, pull up in front of the space
		"State_PullOver",
		"State_PassengerExit",
		"State_Stop",
		"State_Failed"
	};

	return aStateNames[iState];
}

void CTaskVehicleParkNew::Debug() const
{
#if DEBUG_DRAW
	//draw the parking space
	Vector3 vSpaceNormal(m_SpaceDir.y, -m_SpaceDir.x, 0.0f);
	Vector3 vSpaceNormalScaled = vSpaceNormal * ms_fParkingSpaceHalfWidth;
	Vector3 vSpaceDirScaled = m_SpaceDir * ms_fParkingSpaceHalfLength;
	Vector3 p1 = m_Params.GetTargetPosition() + vSpaceDirScaled + vSpaceNormalScaled;
	Vector3 p2 = m_Params.GetTargetPosition() + -vSpaceDirScaled + vSpaceNormalScaled;
	Vector3 p3 = m_Params.GetTargetPosition() + -vSpaceDirScaled + -vSpaceNormalScaled;
	Vector3 p4 = m_Params.GetTargetPosition() + vSpaceDirScaled + -vSpaceNormalScaled;

	grcDebugDraw::Line(p1, p2, Color_green3);
	grcDebugDraw::Line(p2, p3, Color_green3);
	grcDebugDraw::Line(p3, p4, Color_green3);
	grcDebugDraw::Line(p4, p1, Color_green3);

	//look through our subtasks and find a point to draw if we're navigating toward one
	aiTask* pSubtask = FindSubTaskOfType(CTaskTypes::TASK_VEHICLE_GOTO_POINT_WITH_AVOIDANCE_AUTOMOBILE);
	if (pSubtask)
	{
		const Vector3* pTargetPos = static_cast<CTaskVehicleGoToPointWithAvoidanceAutomobile*>(pSubtask)->GetTargetPosition();
		if (pTargetPos)
		{
			grcDebugDraw::Sphere(*pTargetPos, 0.5f, Color_WhiteSmoke, false);
		}
	}

#endif //DEBUG_DRAW
}
#endif	//!__FINAL

//Start
void		CTaskVehicleParkNew::Start_OnEnter(CVehicle* /*pVehicle*/)
{
}

CTask::FSM_Return	CTaskVehicleParkNew::Start_OnUpdate(CVehicle* /*pVehicle*/)
{
	m_SpaceDir.NormalizeSafe(ORIGIN);
	SetState(State_Navigate);
	return FSM_Continue;
}

void		CTaskVehicleParkNew::PullOver_OnEnter(CVehicle* /*pVehicle*/)
{
	const bool bPullOverImmediate = m_ParkType == Park_PullOverImmediate;
	SetNewTask(rage_new CTaskVehiclePullOver(bPullOverImmediate));
}

CTask::FSM_Return	CTaskVehicleParkNew::PullOver_OnUpdate(CVehicle* /*pVehicle*/)
{
	if (!GetSubTask() || GetIsSubtaskFinished(CTaskTypes::TASK_VEHICLE_PULL_OVER))
	{
		SetState(State_Stop);
	}

	return FSM_Continue;
}

void CTaskVehicleParkNew::PassengerExit_OnEnter(CVehicle* pVehicle)
{
	Vector3 vTargetPos;
	FindTargetCoors(pVehicle, vTargetPos);
	SetNewTask(rage_new CTaskVehiclePassengerExit(vTargetPos));
}

CTask::FSM_Return	CTaskVehicleParkNew::PassengerExit_OnUpdate(CVehicle* UNUSED_PARAM(pVehicle))
{
	if (!GetSubTask() || GetIsSubtaskFinished(CTaskTypes::TASK_VEHICLE_PASSENGER_EXIT))
	{
		SetState(State_Stop);
	}
	return FSM_Continue;
}

static dev_float s_fCoarseNavigationDistanceXY = 20.0f;
static dev_float s_fCoarseNavigationDistanceZ = 10.0f;

//normalize the space's heading vector, and align it with the car
void CTaskVehicleParkNew::FixUpParkingSpaceDirection(CVehicle* pVehicle)
{
	if (m_ParkType == Park_Perpendicular_NoseIn
		|| m_ParkType == Park_Perpendicular_BackIn
		)
	{
		Vector3 vDeltaToCar = m_Params.GetTargetPosition() - VEC3V_TO_VECTOR3(pVehicle->GetVehiclePosition());

		//m_SpaceDir's direction may be forward or backward
		if (m_SpaceDir.Dot(vDeltaToCar) < 0.0f)
		{
			m_SpaceDir.Negate();
		}
	}
	else if (m_ParkType == Park_Parallel
			|| m_ParkType == Park_LeaveParallelSpace
			|| m_ParkType == Park_PullOver
			|| m_ParkType == Park_PullOverImmediate
			|| m_ParkType == Park_BackOutPerpendicularSpace)
	{
		Vector3 vCarHeading = VEC3V_TO_VECTOR3(pVehicle->GetVehicleForwardDirection());
		if (m_SpaceDir.Dot(vCarHeading) < 0.0f)
		{
			m_SpaceDir.Negate();
		}
	}
}

// Navigate
void		CTaskVehicleParkNew::Navigate_OnEnter(CVehicle* /*pVehicle*/)
{
	//drive using the road network until we're relatively close
	sVehicleMissionParams params = m_Params;
	params.m_fTargetArriveDist = 0.0f;
	CTaskVehicleMissionBase* pSubTask = CVehicleIntelligence::CreateAutomobileGotoTask(params, 0);
	if(pSubTask && pSubTask->GetTaskType() == CTaskTypes::TASK_VEHICLE_GOTO_AUTOMOBILE_NEW && m_iMaxPathSearchDistance >= 0)
	{
		CTaskVehicleGoToAutomobileNew* pGoToTask = static_cast<CTaskVehicleGoToAutomobileNew*>(pSubTask);
		pGoToTask->SetMaxPathSearchDistance(m_iMaxPathSearchDistance);
	}
	SetNewTask(pSubTask);

	// Reset the test helper in case it was doing something already for some reason.
	m_SpaceFreeFromObstructionsHelper.ResetTest();

	// Set up a new test, if enabled.
	CVehicle *pVehicle = GetVehicle();
	if(m_bDoTestsForBlockedSpace && pVehicle)
	{
		CBaseModelInfo* pModelInfo = pVehicle->GetBaseModelInfo();
		if(pModelInfo)
		{
			Mat34V mtrx;
			const Vec3V posV = VECTOR3_TO_VEC3V(m_Params.GetTargetPosition());
			const Vec3V dirOrigV = VECTOR3_TO_VEC3V(m_SpaceDir);
			CParkingSpaceFreeFromObstructionsTestHelper::ComputeParkingSpaceMatrix(mtrx, posV, dirOrigV);

			CParkingSpaceFreeTestVehInfo vehInfo;
			vehInfo.m_ModelInfo = pModelInfo;

			m_SpaceFreeFromObstructionsHelper.StartTest(vehInfo, mtrx, pVehicle);
		}
	}
}

CTask::FSM_Return	CTaskVehicleParkNew::Navigate_OnUpdate(CVehicle* pVehicle)
{
	Vector3 vTargetPos;
	FindTargetCoors(pVehicle, vTargetPos);

	// Update the blocked space test, if there is one.
	if(m_bDoTestsForBlockedSpace && m_SpaceFreeFromObstructionsHelper.IsTestActive())
	{
		m_SpaceFreeFromObstructionsHelper.Update();
		bool spaceFree = false;
		if(!m_SpaceFreeFromObstructionsHelper.GetTestResults(spaceFree))
		{
			// Waiting for the test to be done, don't allow us to
			// switch states just yet.
			return FSM_Continue;
		}
	}

	const Vector3 vBonnetPos = VEC3V_TO_VECTOR3(CVehicleFollowRouteHelper::GetVehicleBonnetPosition(pVehicle, IsDrivingFlagSet(DF_DriveInReverse)));
	if (!GetSubTask() || GetIsSubtaskFinished(CVehicleIntelligence::GetAutomobileGotoTaskType())
		|| ((vTargetPos - vBonnetPos).XYMag2() < s_fCoarseNavigationDistanceXY*s_fCoarseNavigationDistanceXY
		&& fabsf(vTargetPos.z - vBonnetPos.z) < s_fCoarseNavigationDistanceZ )
		)
	{

		// Handle test results.
		if(m_bDoTestsForBlockedSpace && m_SpaceFreeFromObstructionsHelper.IsTestActive())
		{
			bool spaceFree = false;
			if(m_SpaceFreeFromObstructionsHelper.GetTestResults(spaceFree))
			{
				// Check if the space is blocked.
				if(!spaceFree)
				{
					// If we have already done enough attempts, fail.
					if(m_NumTimesParkingSpaceBlocked >= sm_Tunables.m_ParkingSpaceBlockedMaxAttempts)
					{
						SetState(State_Failed);
						return FSM_Continue;
					}

					// Increase the blocked count and hang out in the Stop state until
					// the next attempt.
					m_NumTimesParkingSpaceBlocked++;
					SetState(State_Stop);
					return FSM_Continue;
				}
			}
		}

		//taskAssertf( vBonnetPos.Dist2(vTargetPos) < s_iCoarseNavigationDistance*s_iCoarseNavigationDistance
		//	, "CTaskVehicleParkNew::Navigate_OnUpdate--GotoPoint finished without getting us within the specified distance to target!");

		FixUpParkingSpaceDirection(pVehicle);

		//aiTask* pTask = NULL;
		//CTaskVehicleGoToAutomobileNew* pGotoTaskNew = NULL;

		switch (m_ParkType)
		{
		case Park_Parallel:
			if (HelperIsAnythingBlockingEntryToSpace(pVehicle))
			{
				SetState(State_PullForward);
			}
			else
			{
				SetState(State_ForwardIntoSpace);
			}
			break;
		case Park_Perpendicular_NoseIn:
			SetState(State_PullForward);
			break;
		case Park_Perpendicular_BackIn:
			SetState(State_PullForward);
			break;
		case Park_PullOver:
		case Park_PullOverImmediate:

// 			pTask = FindSubTaskOfType(CVehicleIntelligence::GetAutomobileGotoTaskType());
// 	
// 			pGotoTaskNew = pTask ? static_cast<CTaskVehicleGoToAutomobileNew*>(pTask) : NULL;
// 			if (pGotoTaskNew)
// 			{
// 				pGotoTaskNew->CopyNodeList(m_pNodeList);
// 			}
			

			SetState(State_PullOver);
			break;
		case Park_LeaveParallelSpace:
			SetState(State_BackAwayFromSpace);
			break;
		case Park_BackOutPerpendicularSpace:
			SetState(State_BackAwayFromSpace);
			break;
		case Park_PassengerExit:
			SetState(State_PassengerExit);
			break;
		default:
			taskAssertf(0, "Invalid or Non-supported ParkType!");
			break;
		}
	}

	return FSM_Continue;
}

//ForwardIntoSpace
void		CTaskVehicleParkNew::ForwardIntoSpace_OnEnter(CVehicle* pVehicle)
{
	Vector3 vSteerPos;
	HelperGetPositionToSteerTo(pVehicle, vSteerPos);

	sVehicleMissionParams params;
	params.m_fCruiseSpeed = GetCruiseSpeedForDistanceToTarget(pVehicle);
	params.m_iDrivingFlags = GetDrivingFlags();
	params.m_iDrivingFlags.ClearFlag(DF_StopForCars);
	params.m_iDrivingFlags.ClearFlag(DF_StopForPeds);
	params.m_iDrivingFlags.ClearFlag(DF_SteerAroundObjects);
	params.m_iDrivingFlags.ClearFlag(DF_SteerAroundStationaryCars);
	params.m_iDrivingFlags.ClearFlag(DF_SwerveAroundAllCars);
	params.m_iDrivingFlags.ClearFlag(DF_SteerAroundPeds);
	params.m_fTargetArriveDist = 0.0f;
	params.SetTargetPosition(vSteerPos);
	SetNewTask(rage_new  CTaskVehicleGoToPointWithAvoidanceAutomobile(params, false));


	if ( pVehicle->InheritsFromBoat() )
	{
		if ( pVehicle->GetIntelligence()->GetBoatAvoidanceHelper() )
		{
			pVehicle->GetIntelligence()->GetBoatAvoidanceHelper()->SetSlowDownEnabled(false);
			pVehicle->GetIntelligence()->GetBoatAvoidanceHelper()->SetSteeringEnabled(false);
		}
	}
}

CTask::FSM_Return	CTaskVehicleParkNew::ForwardIntoSpace_OnUpdate(CVehicle* pVehicle)
{
	if ((!GetSubTask() || GetIsSubtaskFinished(CTaskTypes::TASK_VEHICLE_GOTO_POINT_WITH_AVOIDANCE_AUTOMOBILE))
		&& GetTimeInState() > 0.0f)
	{
		SetState(State_Stop);
		return FSM_Continue;
	}

	CTaskVehicleGoToPointWithAvoidanceAutomobile* pGoToTask = NULL;
	if(GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_VEHICLE_GOTO_POINT_WITH_AVOIDANCE_AUTOMOBILE)
	{
		pGoToTask = static_cast<CTaskVehicleGoToPointWithAvoidanceAutomobile*>(GetSubTask());
	}

	//if we're pulling forward in a parallel park, allow us to go in front of the line a bit
	//so we can smooth out our position, then back up straight into it
	const float fAmountAllowedInFrontOfSpace = m_ParkType == Park_Parallel && !IsCurrentPositionWithinXTolerance(pVehicle)
		? pVehicle->GetBoundingBoxMax().y
		: 0.0f;


	bool bAllowStop = true;

	if ( pVehicle->InheritsFromBoat() )
	{
		if (pVehicle->GetIntelligence()->GetBoatAvoidanceHelper() )
		{
			if ( pVehicle->GetIntelligence()->GetBoatAvoidanceHelper()->IsBlocked() )
			{
				bAllowStop = false;
				if ( pVehicle->GetIntelligence()->GetBoatAvoidanceHelper()->IsBeached() )
				{
					bAllowStop = true;
				}
			}
		}
	}

	//bool bFacingForward = false;
	const float fDistInFrontOfSpace = GetDistanceInFrontOfSpace(pVehicle);
	if (bAllowStop
		&& (fDistInFrontOfSpace > fAmountAllowedInFrontOfSpace 
		|| (HelperIsVehicleStopped(pVehicle) && pGoToTask && pGoToTask->GetCruiseSpeed() <= 0.0f)
		|| pVehicle->GetIntelligence()->MillisecondsNotMoving > ms_iParkingStuckTime) )
	{
		SetState(State_Stop);
		return FSM_Continue;
	}

	if (pGoToTask)
	{
		pGoToTask->SetCruiseSpeed(GetCruiseSpeedForDistanceToTarget(pVehicle));

		Vector3 vSteerPos;
		HelperGetPositionToSteerTo(pVehicle, vSteerPos);
		pGoToTask->SetTargetPosition(&vSteerPos);
	}

// #if __DEV
// 
// 	//debugging
// 	Vector3 vDest(Vector3::ZeroType);
// 	CTaskVehicleGoToPointWithAvoidanceAutomobile* pSubtask1 = static_cast<CTaskVehicleGoToPointWithAvoidanceAutomobile*>
// 		(FindSubTaskOfType(CTaskTypes::TASK_VEHICLE_GOTO_POINT_WITH_AVOIDANCE_AUTOMOBILE));
// 	if (pSubtask1)
// 	{
// 		vDest = *pSubtask1->GetTargetPosition();
// 		grcDebugDraw::Sphere(vDest, 0.5f, Color_red4, false);
// 		grcDebugDraw::Sphere(m_vTargetPos, 0.5f, Color_green2, false);
// 
// 		Vector3 frontPos, rearPos;
// 		HelperGetParkingSpaceFrontPosition(frontPos);
// 		HelperGetParkingSpaceRearPosition(rearPos);
// 		grcDebugDraw::Sphere(frontPos, 0.3f, Color_plum4, false);
// 		grcDebugDraw::Sphere(rearPos, 0.3f, Color_blue3, false);
// 	}
// 
// #endif //__DEV

	return FSM_Continue;
}

//BackIntoSpace
void		CTaskVehicleParkNew::BackIntoSpace_OnEnter(CVehicle* pVehicle)
{
	Vector3 vSteerPos;
	HelperGetPositionToSteerTo(pVehicle, vSteerPos);

	sVehicleMissionParams params;
	params.m_fCruiseSpeed = GetCruiseSpeedForDistanceToTarget(pVehicle);
	params.m_iDrivingFlags = GetDrivingFlags()|DF_DriveInReverse;
	params.m_iDrivingFlags.ClearFlag(DF_StopForCars);
	params.m_iDrivingFlags.ClearFlag(DF_StopForPeds);
	params.m_iDrivingFlags.ClearFlag(DF_SteerAroundObjects);
	params.m_iDrivingFlags.ClearFlag(DF_SteerAroundStationaryCars);
	params.m_iDrivingFlags.ClearFlag(DF_SwerveAroundAllCars);
	params.m_iDrivingFlags.ClearFlag(DF_SteerAroundPeds);
	params.m_fTargetArriveDist = 0.0f;
	params.SetTargetPosition(vSteerPos);
	SetNewTask(rage_new CTaskVehicleGoToPointWithAvoidanceAutomobile(params, false));
}

CTask::FSM_Return	CTaskVehicleParkNew::BackIntoSpace_OnUpdate(CVehicle* pVehicle)
{
	if ((!GetSubTask() || GetIsSubtaskFinished(CTaskTypes::TASK_VEHICLE_GOTO_POINT_WITH_AVOIDANCE_AUTOMOBILE))
		&& GetTimeInState() > 0.0f)
	{
		SetState(State_Stop);
		return FSM_Continue;
	}

	CTaskVehicleGoToPointWithAvoidanceAutomobile * pGoToTask = NULL;
	if(GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_VEHICLE_GOTO_POINT_WITH_AVOIDANCE_AUTOMOBILE)
	{
		pGoToTask = static_cast<CTaskVehicleGoToPointWithAvoidanceAutomobile*>(GetSubTask());
	}

	if ((pGoToTask && pGoToTask->GetCruiseSpeed() <= 0.0f && HelperIsVehicleStopped(pVehicle))
		|| pVehicle->GetIntelligence()->MillisecondsNotMoving > ms_iParkingStuckTime )
	{
		SetState(State_Stop);
		return FSM_Continue;
	}

	if (pGoToTask)
	{
		pGoToTask->SetCruiseSpeed(GetCruiseSpeedForDistanceToTarget(pVehicle));

		Vector3 vSteerPos;
		HelperGetPositionToSteerTo(pVehicle, vSteerPos);
		pGoToTask->SetTargetPosition(&vSteerPos);
	}
	return FSM_Continue;
}

//BackAwayFromSpace
void		CTaskVehicleParkNew::BackAwayFromSpace_OnEnter(CVehicle* pVehicle)
{
	Vector3 vBackUpPosition(Vector3::ZeroType);
	HelperGetBackAwayFromSpacePosition(pVehicle, vBackUpPosition);

	sVehicleMissionParams params;
	params.m_fCruiseSpeed = GetCruiseSpeedForDistanceToTarget(pVehicle);
	params.m_iDrivingFlags = GetDrivingFlags()|DF_DriveInReverse;
	params.m_fTargetArriveDist = 1.0f;
	params.SetTargetPosition(vBackUpPosition);
	SetNewTask(rage_new  CTaskVehicleGoToPointWithAvoidanceAutomobile(params, false));
}

CTask::FSM_Return	CTaskVehicleParkNew::BackAwayFromSpace_OnUpdate(CVehicle* pVehicle)
{
	if ((!GetSubTask() || GetIsSubtaskFinished(CTaskTypes::TASK_VEHICLE_GOTO_POINT_WITH_AVOIDANCE_AUTOMOBILE))
		&& GetTimeInState() > 0.0f)
	{
		SetState(State_Stop);
		return FSM_Continue;
	}

	if(GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_VEHICLE_GOTO_POINT_WITH_AVOIDANCE_AUTOMOBILE)
	{
		CTaskVehicleGoToPointWithAvoidanceAutomobile * pGoToTask = (CTaskVehicleGoToPointWithAvoidanceAutomobile*)GetSubTask();
		pGoToTask->SetCruiseSpeed(GetCruiseSpeedForDistanceToTarget(pVehicle));

		Vector3 vBackUpPosition(Vector3::ZeroType);
		HelperGetBackAwayFromSpacePosition(pVehicle, vBackUpPosition);
		pGoToTask->SetTargetPosition(&vBackUpPosition);
	}

	if (m_ParkType == Park_Perpendicular_NoseIn)
	{
		//early out if we're roughly halfway out of the space and facing the right direction
		//bool bFacingForward = false;
		const float fDistanceInFrontOfSpace = GetDistanceInFrontOfSpace(pVehicle);
		if ((IsCurrentHeadingWithinTolerance(pVehicle) 
			&& fDistanceInFrontOfSpace < -ms_fParkingSpaceHalfLength
			&& IsCurrentPositionWithinXTolerance(pVehicle))
			|| fDistanceInFrontOfSpace < -(2.0f * ms_fParkingSpaceHalfLength + 3.0f))
		{
			SetState(State_Stop);
			return FSM_Continue;
		}
	}

// #if __DEV
// 
// 	//debugging
// 	Vector3 vDest(Vector3::ZeroType);
// 	CTaskVehicleGoToPointWithAvoidanceAutomobile* pSubtask1 = static_cast<CTaskVehicleGoToPointWithAvoidanceAutomobile*>
// 		(FindSubTaskOfType(CTaskTypes::TASK_VEHICLE_GOTO_POINT_WITH_AVOIDANCE_AUTOMOBILE));
// 	if (pSubtask1)
// 	{
// 		vDest = *pSubtask1->GetTargetPosition();
// 		grcDebugDraw::Sphere(vDest, 0.5f, Color_red4, false);
// 	}
// 
// #endif //__DEV

	return FSM_Continue;
}

//PullForwardParallel
void		CTaskVehicleParkNew::PullForward_OnEnter(CVehicle* pVehicle)
{
	static dev_float s_fParallelZOffset = 9.0f;
	static dev_float s_fParallelXOffset = 3.5f;
	//Approach an intermediate position before pulling into the space
	Vector3 vApproachPosition(Vector3::ZeroType);
	const bool bShouldUseBackInOffsets = m_ParkType == Park_Perpendicular_BackIn && GetPreviousState() == State_Navigate;
	const float fApproachTolerance = pVehicle->InheritsFromPlane() ? 5.0f : 1.0f;

	if (m_ParkType == Park_Perpendicular_NoseIn || bShouldUseBackInOffsets)
	{
		//ok, new algorithm. use however far away we are by the time we get in here as our Z offset
		//and the X offset should be the minimum between the X offset from the space and (trig incoming)
		//(Z offset) / tan(max steering angle at current speed)
		Vector3 vRearPos;
		HelperGetParkingSpaceRearPosition(vRearPos);
		const Vector3 vBonnetPos = VEC3V_TO_VECTOR3(CVehicleFollowRouteHelper::GetVehicleBonnetPosition(pVehicle, false));
 		const float fActualZOffset = (vBonnetPos - vRearPos).Dot(-m_SpaceDir);
		const float fTurnRadius = pVehicle->GetIntelligence()->GetTurnRadiusAtCurrentSpeed();

		//get a vector normal to the parking space, and point it toward the car
		Vector3 vSpaceNormal(m_SpaceDir.y, -m_SpaceDir.x, 0.0f);
		Vector3 vDeltaToCar = vBonnetPos - vRearPos;
		if (vSpaceNormal.Dot(vDeltaToCar) < 0.0f)
		{
			vSpaceNormal.Negate();
		}
		const float fActualXOffset = (vBonnetPos - vRearPos).Dot(vSpaceNormal);

		//add the Z direction
		vApproachPosition = vRearPos + (-m_SpaceDir * fActualZOffset);

		//add the X direction if needed. if the z offset is large enough we won't need any
		//X offset
		//const float fMaxXOffset = rage::Max(fTurnRadius - fApproachTolerance, 0.0f);
		const float fMinXOffsetRequired = rage::Max(fTurnRadius - fActualZOffset, ms_fParkingSpaceHalfWidth);

		vApproachPosition += vSpaceNormal * rage::Min(fActualXOffset, fMinXOffsetRequired);
	}
	else if (m_ParkType == Park_Perpendicular_BackIn)
	{
		HelperGetBackAwayFromSpacePosition(pVehicle, vApproachPosition);
	}
	else if (m_ParkType == Park_Parallel)
	{
		vApproachPosition = m_Params.GetTargetPosition() + (m_SpaceDir * s_fParallelZOffset);
		Vector3 vSpaceNormal(m_SpaceDir.y, -m_SpaceDir.x, 0.0f);
		Vector3 vDeltaToCar = VEC3V_TO_VECTOR3(pVehicle->GetVehiclePosition()) - vApproachPosition;
		if (vSpaceNormal.Dot(vDeltaToCar) < 0.0f)
		{
			vSpaceNormal.Negate();
		}
		vApproachPosition += (vSpaceNormal * s_fParallelXOffset);
	}
	else
	{
		taskAssertf(0, "Parktype not supported yet :(");
	}

	//drive to the point
	sVehicleMissionParams params;
	params.m_fCruiseSpeed = GetCruiseSpeedForDistanceToTarget(pVehicle);
	params.m_iDrivingFlags = GetDrivingFlags();
	params.m_fTargetArriveDist = fApproachTolerance;
	params.SetTargetPosition(vApproachPosition);
	SetNewTask(rage_new  CTaskVehicleGoToPointWithAvoidanceAutomobile(params));
}

CTask::FSM_Return	CTaskVehicleParkNew::PullForward_OnUpdate(CVehicle* pVehicle)
{
	if (!GetSubTask() || GetIsSubtaskFinished(CTaskTypes::TASK_VEHICLE_GOTO_POINT_WITH_AVOIDANCE_AUTOMOBILE))
	{
		if (m_ParkType == Park_Perpendicular_NoseIn)
		{
			SetState(State_ForwardIntoSpace);
			return FSM_Continue;
		}
		else if (m_ParkType == Park_Parallel)
		{
			SetState(State_BackIntoSpace);
			return FSM_Continue;
		}
		else if (m_ParkType == Park_Perpendicular_BackIn)
		{
			SetState(State_BackIntoSpace);
			return FSM_Continue;
		}
	}

	if (m_ParkType == Park_Perpendicular_BackIn)
	{
		//early out if we're roughly halfway out of the space and facing the right direction
		//bool bFacingForward = false;
		if ((IsCurrentHeadingWithinTolerance(pVehicle) 
			&& GetDistanceInFrontOfSpace(pVehicle) < -ms_fParkingSpaceHalfLength
			&& IsCurrentPositionWithinXTolerance(pVehicle))
			|| pVehicle->GetIntelligence()->MillisecondsNotMoving > ms_iParkingStuckTime
			/*|| GetDistanceInFrontOfSpace(pVehicle) < -(2.0f * ms_fParkingSpaceHalfLength + 3.0f)*/)
		{
			SetState(State_BackIntoSpace);
			return FSM_Continue;
		}
	}

	if(GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_VEHICLE_GOTO_POINT_WITH_AVOIDANCE_AUTOMOBILE)
	{
		CTaskVehicleGoToPointWithAvoidanceAutomobile * pGoToTask = (CTaskVehicleGoToPointWithAvoidanceAutomobile*)GetSubTask();
		pGoToTask->SetCruiseSpeed(GetCruiseSpeedForDistanceToTarget(pVehicle));
	}

// #if __DEV
// 
// 	
// 	//debugging
// 	Vector3 vDest(Vector3::ZeroType);
// 	CTaskVehicleGoToPointWithAvoidanceAutomobile* pSubtask1 = static_cast<CTaskVehicleGoToPointWithAvoidanceAutomobile*>
// 		(FindSubTaskOfType(CTaskTypes::TASK_VEHICLE_GOTO_POINT_WITH_AVOIDANCE_AUTOMOBILE));
// 	if (pSubtask1)
// 	{
// 		vDest = *pSubtask1->GetTargetPosition();
// 	}
// 
// 	Vector3 vDest2(Vector3::ZeroType);
// 	CTaskVehicleGoToPointAutomobile* pSubtask2 = static_cast<CTaskVehicleGoToPointAutomobile*>
// 		(FindSubTaskOfType(CTaskTypes::TASK_VEHICLE_GOTO_POINT_AUTOMOBILE));
// 	if (pSubtask2)
// 	{
// 		vDest2 = *pSubtask2->GetTargetPosition();
// 	}
// 
// 	grcDebugDraw::Sphere(vDest, 0.5f, Color_red4, false);
// 	grcDebugDraw::Sphere(m_vTargetPos, 0.5f, Color_green3, false);
// 	grcDebugDraw::Sphere(vDest2, 0.5f, Color_yellow1, false);
// 
// #endif //__DEV

	return FSM_Continue;
}

//Stop
void		CTaskVehicleParkNew::Stop_OnEnter(CVehicle* /*pVehicle*/)
{
	SetNewTask(rage_new CTaskVehicleStop(CTaskVehicleStop::SF_DontResetSteerAngle));
}

CTask::FSM_Return	CTaskVehicleParkNew::Stop_OnUpdate(CVehicle* pVehicle)
{
	static dev_float s_fMinimumTimeBeforeTryingAgainSeconds = 0.2f;
	const bool bTimeElapsed = GetTimeInState() >= s_fMinimumTimeBeforeTryingAgainSeconds;

	if (bTimeElapsed && GetIsSubtaskFinished(CTaskTypes::TASK_VEHICLE_STOP))
	{
		if (IsReadyToQuit(pVehicle))
		{
			//for now just hang out in this state so dudes don't go back to
			//cruising after they park
			//......that sounded really dirty
			//return FSM_Continue;

			//not sure if PullOver implies idling on the curb or not--
			//existing code does that so we'll do the same for now
			if (m_ParkType != Park_PullOver 
				&& m_ParkType != Park_PullOverImmediate
				&& m_ParkType != Park_LeaveParallelSpace
				&& m_ParkType != Park_BackOutPerpendicularSpace)
			{
				if (ShouldTurnOffEngineAndLights(pVehicle) && !m_bKeepLightsOnAfterParking)
				{
					pVehicle->m_nVehicleFlags.bLightsOn = false;
					pVehicle->SwitchEngineOff();
				}
				CScenarioManager::GiveScenarioToDrivingCar(pVehicle);
			}
			
			if (m_ParkType == Park_Parallel)
			{
				pVehicle->m_nVehicleFlags.bIsParkedParallelly = true;
			}
			else if (m_ParkType == Park_Perpendicular_NoseIn)
			{
				pVehicle->GetIntelligence()->bCarHasToReverseFirst = true;
				pVehicle->m_nVehicleFlags.bIsParkedPerpendicularly = true;
			}
			else if (m_ParkType == Park_Perpendicular_BackIn)
			{
				pVehicle->m_nVehicleFlags.bIsParkedPerpendicularly = true;
			}

			m_bMissionAchieved = true;
			return FSM_Quit;
		}	
		else if (GetPreviousState() == State_ForwardIntoSpace && m_ParkType == Park_Parallel)
		{
			SetState(State_BackIntoSpace);
		}
		else if (GetPreviousState() == State_ForwardIntoSpace)
		{
			SetState(State_BackAwayFromSpace);
		}
		else if (GetPreviousState() == State_BackAwayFromSpace)
		{
			SetState(State_ForwardIntoSpace);
		}
		else if (GetPreviousState() == State_BackIntoSpace  && m_ParkType == Park_Perpendicular_BackIn)
		{
			SetState(State_PullForward);
		}
		else if (GetPreviousState() == State_BackIntoSpace && m_ParkType == Park_Parallel)
		{
			SetState(State_ForwardIntoSpace);
		}
		else if (GetPreviousState() == State_PassengerExit && m_ParkType == Park_PassengerExit)
		{
			m_bMissionAchieved = true;
			return FSM_Quit;
		}
		else if(m_bDoTestsForBlockedSpace && GetPreviousState() == State_Navigate)
		{
			// Check if enough time has passed since the last blocked space test.
			// If so, go back to Navigate.
			if(GetTimeInState() >= sm_Tunables.m_ParkingSpaceBlockedWaitTimePerAttempt)
			{
				// Note: if the test takes multiple frames, it would probably be best
				// to set up the test here already and wait in this state until the results
				// are in, or add a new state for that.
				SetState(State_Navigate);
			}
		}
		else
		{
			taskAssertf(0, "Somehow we jumped to State_Stop from an unsupported state???");
		}
	}

	return FSM_Continue;
}


void CTaskVehicleParkNew::Failed_OnEnter()
{
}


CTask::FSM_Return CTaskVehicleParkNew::Failed_OnUpdate()
{
	// Note: we could potentially just FSM_Quit here, but I had some difficulty in getting
	// the ped tasks detect the failure, the vehicle task got removed too early for that.

	// As safety, we still quit after some time.
	if(GetTimeInState() >= 2.0f)
	{
		return FSM_Quit;
	}

	return FSM_Continue;
}


bool CTaskVehicleParkNew::IsCurrentHeadingWithinTolerance(CVehicle* pVehicle) const
{
	const float fVehicleHeading = pVehicle->GetTransform().GetHeading();

	const bool bIsPerpendicularBackIn = m_ParkType == Park_Perpendicular_BackIn;
	const Vector3 vSpaceDir = bIsPerpendicularBackIn ? -m_SpaceDir : m_SpaceDir;
	const float fSpaceHeading = fwAngle::LimitRadianAngleSafe(fwAngle::GetATanOfXY(vSpaceDir.x, vSpaceDir.y) - (DtoR * 90.0f));

	//grcDebugDraw::Arrow(pVehicle->GetVehiclePosition(), pVehicle->GetVehiclePosition() + (pVehicle->GetVehicleForwardDirection() * ScalarV(V_FIVE)), 1.0f, Color_yellow1);
	//grcDebugDraw::Arrow(VECTOR3_TO_VEC3V(m_vTargetPos), VECTOR3_TO_VEC3V(m_vTargetPos+(m_SpaceDir*3.0f)), 1.0f, Color_pink1);

	float fDiff = rage::Abs(fSpaceHeading-fVehicleHeading);
	fDiff = fwAngle::LimitRadianAngle(fDiff);

	const float fToleranceRadiansToUse = pVehicle->InheritsFromPlane() ? PI : m_HeadingToleranceRadians;

	return rage::Abs(fDiff) <= fToleranceRadiansToUse;
}

bool CTaskVehicleParkNew::IsCurrentPositionWithinTolerance(CVehicle* pVehicle) const
{
	return IsCurrentPositionWithinXTolerance(pVehicle) && IsCurrentPositionWithinYTolerance(pVehicle);
	//return DistSquared(pVehicle->GetVehiclePosition(), VECTOR3_TO_VEC3V(m_vTargetPos)).Getf() <= ms_fTargetAchieveDistance*ms_fTargetAchieveDistance;
}

bool CTaskVehicleParkNew::IsReadyToQuit(CVehicle* pVehicle) const
{
	taskAssert(GetState() == State_Stop);
	if (m_ParkType == Park_PullOver
		|| m_ParkType == Park_PullOverImmediate)
	{
		return true;
	}

	if ((m_ParkType == Park_LeaveParallelSpace || m_ParkType == Park_BackOutPerpendicularSpace)
		&& GetPreviousState() == State_BackAwayFromSpace)
	{
		return true;
	}

	//early out if we weren't trying to actually enter the space on this move
	if (GetPreviousState() == State_BackAwayFromSpace)
	{
		return false;
	}
	
	if (!IsCurrentHeadingWithinTolerance(pVehicle))
	{
		return false;
	}

	if (!IsCurrentPositionWithinTolerance(pVehicle))
	{
		return false;
	}

	return true;
}

void CTaskVehicleParkNew::HelperGetParkingSpaceFrontPosition(Vector3& vPositionOut) const
{
	vPositionOut = m_Params.GetTargetPosition() + (m_SpaceDir * ms_fParkingSpaceHalfLength);
}

void CTaskVehicleParkNew::HelperGetParkingSpaceRearPosition(Vector3& vPositionOut) const
{
	vPositionOut = m_Params.GetTargetPosition() + (m_SpaceDir * -ms_fParkingSpaceHalfLength);
}

bool CTaskVehicleParkNew::HelperUseReverseSpace() const
{
	return m_ParkType == Park_Parallel && GetState() == State_BackIntoSpace;
}

bool CTaskVehicleParkNew::HelperUseRearBonnetPos() const
{
	return GetState() == State_BackIntoSpace;
}

float CTaskVehicleParkNew::GetDistanceInFrontOfSpace(CVehicle* pVehicle) const
{
	Vector3 vPosition;

	const bool bReverseSpace = HelperUseReverseSpace();

	if (bReverseSpace)
	{
		HelperGetParkingSpaceRearPosition(vPosition);
	}
	else
	{
		HelperGetParkingSpaceFrontPosition(vPosition);
	}
	
	const Vector3 vVehicleNavPos = VEC3V_TO_VECTOR3(CVehicleFollowRouteHelper::GetVehicleBonnetPosition(pVehicle, HelperUseRearBonnetPos()));
	Vector3 vDeltaFromSpace = vVehicleNavPos - vPosition;

	float dotProduct = vDeltaFromSpace.Dot((bReverseSpace ? -1.0f : 1.0f ) * m_SpaceDir);

	if (bReverseSpace)
	{
		static dev_float s_fExtraParallelReverseDistance = 3.0f;
		dotProduct -= s_fExtraParallelReverseDistance;
	}
	return dotProduct;
}

bool CTaskVehicleParkNew::IsCurrentPositionWithinXTolerance(CVehicle* pVehicle) const
{
	const bool bUseRearPosForVehicle = HelperUseRearBonnetPos();
	const Vector3 vVehicleNavPos = VEC3V_TO_VECTOR3(CVehicleFollowRouteHelper::GetVehicleBonnetPosition(pVehicle, bUseRearPosForVehicle));

	Vector3 vTargetPos;
	if (HelperUseReverseSpace())
	{
		HelperGetParkingSpaceRearPosition(vTargetPos);
	}
	else
	{
		HelperGetParkingSpaceFrontPosition(vTargetPos);
	}

	const Vector3 vDeltaFromSpace = vVehicleNavPos - vTargetPos;

	//rotate the parking space heading 90 degrees
	Vector3 vSpaceNormal(m_SpaceDir.y, -m_SpaceDir.x, 0.0f);

	const float debugDotProduct = vDeltaFromSpace.Dot(vSpaceNormal);

	//grcDebugDraw::Sphere(m_vTargetPos, 0.3f, Color_green, false);
	//grcDebugDraw::Sphere(m_vTargetPos + (vSpaceNormal * ms_fPositionXTolerance), 0.3f, Color_green2, false);
	//grcDebugDraw::Sphere(m_vTargetPos + (vSpaceNormal * -ms_fPositionXTolerance), 0.3f, Color_green2, false);

	float fXTolerance = ms_fPositionXTolerance;
	if (pVehicle->InheritsFromBike() || pVehicle->InheritsFromQuadBike() || pVehicle->InheritsFromAmphibiousQuadBike())
	{
		fXTolerance = ms_fPositionXToleranceBike;
	}
	else if (pVehicle->InheritsFromPlane())
	{
		fXTolerance = ms_fPositionXTolerancePlane;
	}

	return fabsf(debugDotProduct) <= fXTolerance;
}

bool CTaskVehicleParkNew::IsCurrentPositionWithinYTolerance(CVehicle* pVehicle) const
{
	const bool bUseRearPosForVehicle = HelperUseRearBonnetPos();
	const Vector3 vVehicleNavPos = VEC3V_TO_VECTOR3(CVehicleFollowRouteHelper::GetVehicleBonnetPosition(pVehicle, bUseRearPosForVehicle));

	Vector3 vTargetPos;
	if (HelperUseReverseSpace())
	{
		HelperGetParkingSpaceRearPosition(vTargetPos);
	}
	else
	{
		HelperGetParkingSpaceFrontPosition(vTargetPos);
	}

	const Vector3 vDeltaFromSpace = vVehicleNavPos - vTargetPos;

	const float debugDotProduct = vDeltaFromSpace.Dot(m_SpaceDir);

	float fYTolerance = ms_fPositionYTolerance;
	if (pVehicle->InheritsFromBike() || pVehicle->InheritsFromQuadBike() || pVehicle->InheritsFromAmphibiousQuadBike())
	{
		fYTolerance = ms_fPositionYToleranceBike;
	}
	else if (pVehicle->InheritsFromPlane())
	{
		fYTolerance = ms_fPositionYTolerancePlane;
	}

	return fabsf(debugDotProduct) <= fYTolerance;
}

void CTaskVehicleParkNew::HelperGetPositionToSteerTo(CVehicle* pVehicle, Vector3& vPositionOut) const
{
	//project N meters out in front of the car, and find the closest position
	//on the line segment defining the parking space
	static dev_float s_fDistInFrontOfCar = 7.0f;
	static dev_float s_fDistInFrontOfCarParallel = 3.0f;
	Vector3 vFrontPos, vRearPos, vParkingSpaceSegment, vSegmentStartPos;

	HelperGetParkingSpaceFrontPosition(vFrontPos);
	HelperGetParkingSpaceRearPosition(vRearPos);

	Vector3 vSpaceNormal(m_SpaceDir.y, -m_SpaceDir.x, 0.0f);
	
	//figure out which segment to use
	const float fVehicleHeading = pVehicle->GetTransform().GetHeading();
	const float fSpaceHeading = fwAngle::LimitRadianAngleSafe(fwAngle::GetATanOfXY(m_SpaceDir.x, m_SpaceDir.y) - (DtoR * 90.0f));

	const bool bBackupParallel = m_ParkType == Park_Parallel && GetState() == State_BackIntoSpace;
	const bool bBackupPerpendicular = m_ParkType == Park_Perpendicular_BackIn;
	//const bool bForwardPerpendicular = m_ParkType == Park_Perpendicular_NoseIn;

if (/*bForwardPerpendicular ||*/ IsCurrentPositionWithinXTolerance(pVehicle) && IsCurrentHeadingWithinTolerance(pVehicle))
	{
		//more or less there, use the straight line
		if (bBackupParallel)
		{
			vParkingSpaceSegment = vRearPos - vFrontPos;
			vSegmentStartPos = vFrontPos;
		}
		else
		{
			vParkingSpaceSegment = vFrontPos - vRearPos;
			vSegmentStartPos = vRearPos;
		}

	}
	//else if (bBackupPerpendicular && fVehicleHeading < fSpaceHeading || !bBackupPerpendicular && fVehicleHeading > fSpaceHeading)
	else if ((((bBackupPerpendicular || bBackupParallel) && (VEC3V_TO_VECTOR3(pVehicle->GetVehiclePosition()) - m_Params.GetTargetPosition()).Dot(vSpaceNormal) > 0.0f))
		|| ((!bBackupPerpendicular && !bBackupParallel) && fVehicleHeading > fSpaceHeading))
	{
		//we're to the right of the space
		vSegmentStartPos = vRearPos + -vSpaceNormal * ms_fParkingSpaceHalfWidth;

		if (bBackupParallel)
		{
			vParkingSpaceSegment = (vFrontPos + -vSpaceNormal * ms_fParkingSpaceHalfWidth) - vRearPos;
			vParkingSpaceSegment.Negate();
		}
		else 
		{
			vParkingSpaceSegment = vFrontPos - vSegmentStartPos;
		}
	}
	else
	{
		//to the left of the space
		vSegmentStartPos = vRearPos + vSpaceNormal * ms_fParkingSpaceHalfWidth;

		if (bBackupParallel)
		{
			vParkingSpaceSegment = (vFrontPos + vSpaceNormal * ms_fParkingSpaceHalfWidth) - vRearPos;
			vParkingSpaceSegment.Negate();
		}
		else
		{
			vParkingSpaceSegment = vFrontPos - vSegmentStartPos;
		}
	}

	//we need to continue steering toward the line even past the extents of the parking space
	vParkingSpaceSegment *= 2.0f;

	Vector3 vVehicleForward = VEC3V_TO_VECTOR3(pVehicle->GetVehicleForwardDirection());
	if (m_ParkType == Park_Perpendicular_BackIn || bBackupParallel)
	{
		vVehicleForward.Negate();
	}

	Vector3 vSearchPosition = VEC3V_TO_VECTOR3(pVehicle->GetVehiclePosition()) + vVehicleForward * (bBackupParallel ? s_fDistInFrontOfCarParallel : s_fDistInFrontOfCar);

	vPositionOut = VEC3V_TO_VECTOR3( geomPoints::FindClosestPointSegToPoint(VECTOR3_TO_VEC3V(vSegmentStartPos), VECTOR3_TO_VEC3V(vParkingSpaceSegment), VECTOR3_TO_VEC3V(vSearchPosition)) );

//#if __DEV
	//grcDebugDraw::Sphere(vPositionOut, 0.5f, Color_red);
	//grcDebugDraw::Sphere(vSearchPosition, 0.5f, Color_blue3);
	//grcDebugDraw::Arrow(VECTOR3_TO_VEC3V(vSegmentStartPos + Vector3(0.0f, 0.0f, 0.1f))
	//	, VECTOR3_TO_VEC3V(vSegmentStartPos + vParkingSpaceSegment + Vector3(0.0f, 0.0f, 0.1f)), 0.5f, Color_red);
//#endif //__DEV
}

float CTaskVehicleParkNew::GetCruiseSpeedForDistanceToTarget(CVehicle* pVehicle) const
{
	//if we're already there, and trying to enter  the space, go ahead and stop
	if ( !pVehicle->InheritsFromBoat() &&
		( GetState() == State_ForwardIntoSpace || GetState() == State_BackIntoSpace ) )
	{
		//if we're done, just stop
// 		if (IsCurrentPositionWithinTolerance(pVehicle)
// 			&& IsCurrentHeadingWithinTolerance(pVehicle))
// 		{
// 			return 0.0f;
// 		}

		float fDistInFrontOfSpace = GetDistanceInFrontOfSpace(pVehicle);
		static dev_float s_fSpeedMultiplier = 1.1f;
		const float fExtraPaddingForNonSuccesfulParallelParks = m_ParkType == Park_Parallel && !IsCurrentPositionWithinXTolerance(pVehicle)
			? pVehicle->GetBoundingBoxMax().y
			: 0.0f;

		fDistInFrontOfSpace -= fExtraPaddingForNonSuccesfulParallelParks;

		if (fDistInFrontOfSpace > (2.0f * -ms_fParkingSpaceHalfLength))
		{
			static dev_float s_fSpeedFloor = 1.0f;
			const float retVal = -(s_fSpeedMultiplier * fDistInFrontOfSpace) + (s_fSpeedFloor * 0.5f);
			return retVal >= s_fSpeedFloor ? retVal : 0.0f;
		}
	}

	//if the user passed in a slower speed than what we computed, don't speed up
	return rage::Min(pVehicle->InheritsFromBoat() ? ms_fCruiseSpeedToParkBoat : ms_fCruiseSpeedToPark, GetCruiseSpeed());
}

bool CTaskVehicleParkNew::HelperIsVehicleStopped(CVehicle* pVehicle) const
{
	return pVehicle->GetVelocity().XYMag2() <= 0.05f*0.05f;
}

void CTaskVehicleParkNew::HelperGetBackAwayFromSpacePosition(CVehicle* pVehicle, Vector3& vPositionOut) const
{
	//depending on what type of action we're doing, we'll back up different amounts
	//just behind the space for parallel parking
	//a few meters back for perpendicular
	//way back (maybe all the way to the road network?) for leaving a perpendicular space
	
	if (m_ParkType == Park_Parallel || m_ParkType == Park_LeaveParallelSpace)
	{
		vPositionOut = m_Params.GetTargetPosition() + (-m_SpaceDir * 1.0f);
	}
	else if (m_ParkType == Park_Perpendicular_NoseIn)
	{
		//TODO: base this on the amount our heading is off from the target
		//back up less if we're only a few degrees off
		Vector3 vSpaceNormal(m_SpaceDir.y, -m_SpaceDir.x, 0.0f);
		const float fBackupLength = rage::Max(2.0f, pVehicle->GetBoundingBoxMax().y - pVehicle->GetBoundingBoxMin().y);
		Vector3 vCenter = VEC3V_TO_VECTOR3(pVehicle->GetVehiclePosition()) + (-m_SpaceDir * fBackupLength);
		const float fXTolerance = (!pVehicle->InheritsFromBike() && !pVehicle->InheritsFromQuadBike() && !pVehicle->InheritsFromAmphibiousQuadBike()) ? ms_fPositionXTolerance : ms_fPositionXToleranceBike;
		Vector3 vLeftPos = vCenter + -vSpaceNormal * fXTolerance * 2.0f;
		Vector3 vToRightPos = vSpaceNormal * fXTolerance * 4.0f;
		Vector3 vSearchPos = VEC3V_TO_VECTOR3(pVehicle->GetVehiclePosition()) + (VEC3V_TO_VECTOR3(-pVehicle->GetVehicleForwardDirection()) * 5.0f);

		vPositionOut = VEC3V_TO_VECTOR3( geomPoints::FindClosestPointSegToPoint(VECTOR3_TO_VEC3V(vLeftPos), VECTOR3_TO_VEC3V(vToRightPos), VECTOR3_TO_VEC3V(vSearchPos)) );
	}
	else if (m_ParkType == Park_BackOutPerpendicularSpace || m_ParkType == Park_Perpendicular_BackIn)
	{
		//TODO: base this on the size of the car and/or distance to road network
		vPositionOut = m_Params.GetTargetPosition() + (-m_SpaceDir * 8.0f);
	}

}

//test the area immediately behind the parking space for vehicles and other objects

bool CTaskVehicleParkNew::HelperIsAnythingBlockingEntryToSpace(CVehicle* pVehicle) const
{
	const Vector3 vSearchPos = m_Params.GetTargetPosition() + (-m_SpaceDir * 2.0f * ms_fParkingSpaceHalfLength);
	const float fSearchRadiusSqr = ms_fParkingSpaceHalfLength * ms_fParkingSpaceHalfLength;

	CEntityScannerIterator entityList = pVehicle->GetIntelligence()->GetVehicleScanner().GetIterator();
	for( CEntity* pEntity = entityList.GetFirst(); pEntity; pEntity = entityList.GetNext() )
	{
		Assert(pEntity->GetIsTypeVehicle());
		CVehicle *pVehObstacle =(CVehicle *)pEntity;

		// Ignore trailers we're towing
		if(pVehObstacle->InheritsFromTrailer())
		{
			// check whether the trailer is attached to the parent vehicle
			CTrailer* pTrailer = static_cast<CTrailer*>(pVehObstacle);
			if(pTrailer->GetAttachParent() == pVehicle)
			{
				continue;
			}
		}

		if ((VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition()) - vSearchPos).XYMag2() < fSearchRadiusSqr)
		{
			return true;
		}
	}

	//const CEntityScannerIterator objectList = pVehicle->GetIntelligence()->GetObjectScanner().GetIterator();
	for( const CEntity* pEntity = entityList.GetFirst(); pEntity; pEntity = entityList.GetNext() )
	{
		if(!pEntity->IsCollisionEnabled())
		{
			continue;
		}

		//don't care about small stuff (this is the same threshold we use when determining whether to avoid a car)
		if (pEntity->GetBoundingBoxMax().z < 0.9f)
		{
			continue;
		}

		if ((VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition()) - vSearchPos).XYMag2() < fSearchRadiusSqr)
		{
			return true;
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////////////////////

CTaskVehicleReactToCopSiren::CTaskVehicleReactToCopSiren(ReactToCopSirenType reactionType, u32 nWaitDuration)
: m_ReactionType(reactionType)
, m_nWaitDuration(nWaitDuration)
{
	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_REACT_TO_COP_SIREN);
}

void CTaskVehicleReactToCopSiren::CleanUp()
{
	if (aiVerify(GetVehicle()))
	{
		GetVehicle()->m_nVehicleFlags.bDisablePretendOccupants = m_bPreviousDisablePretendOccupantsCached;
	}
}

CTask::FSM_Return CTaskVehicleReactToCopSiren::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	CVehicle *pVehicle = GetVehicle(); //Get the vehicle ptr.

	FSM_Begin
		// Start
		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate(pVehicle);

		// Pullover
		FSM_State(State_Pullover)
			FSM_OnEnter
				Pullover_OnEnter(pVehicle);
			FSM_OnUpdate
				return Pullover_OnUpdate(pVehicle);

		// Wait
		FSM_State(State_Wait)
			FSM_OnEnter
				Wait_OnEnter(pVehicle);
			FSM_OnUpdate
				return Wait_OnUpdate(pVehicle);
	FSM_End
}

CTask::FSM_Return CTaskVehicleReactToCopSiren::Start_OnUpdate(CVehicle* pVehicle)
{
	m_bPreviousDisablePretendOccupantsCached = pVehicle->m_nVehicleFlags.bDisablePretendOccupants;

	if (m_ReactionType == React_JustWait)
	{
		SetState(State_Wait);
	}
	else
	{
		SetState(State_Pullover);
	}
	return FSM_Continue;
}

void CTaskVehicleReactToCopSiren::Pullover_OnEnter(CVehicle* /*pVehicle*/)
{
	aiAssert(m_ReactionType == React_PullOverLeft || m_ReactionType == React_PullOverRight);

	CTaskVehiclePullOver::ForcePulloverDirection pulloverDir = m_ReactionType == React_PullOverLeft 
		? CTaskVehiclePullOver::Force_Left 
		: CTaskVehiclePullOver::Force_Right;
	SetNewTask(rage_new CTaskVehiclePullOver(true, pulloverDir, true));
}

CTask::FSM_Return CTaskVehicleReactToCopSiren::Pullover_OnUpdate(CVehicle* /*pVehicle*/)
{
	if (GetIsSubtaskFinished(CTaskTypes::TASK_VEHICLE_PULL_OVER))
	{	
		SetState(State_Wait);
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////
//State_Wait
//////////////////////////////////////////////////////////////////////
void		CTaskVehicleReactToCopSiren::Wait_OnEnter(CVehicle* UNUSED_PARAM(pVehicle))
{
	SetNewTask(rage_new CTaskVehicleWait(NetworkInterface::GetSyncedTimeInMilliseconds() + m_nWaitDuration));
}

/////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return	CTaskVehicleReactToCopSiren::Wait_OnUpdate(CVehicle* UNUSED_PARAM(pVehicle))
{
	if (GetIsSubtaskFinished(CTaskTypes::TASK_VEHICLE_WAIT))
	{
		return FSM_Quit;
	}
	return FSM_Continue;
}

#if !__FINAL
const char * CTaskVehicleReactToCopSiren::GetStaticStateName( s32 iState  )
{
	Assert(iState>=State_Start && iState<=State_Wait);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_Pullover",
		"State_Wait",
	};

	return aStateNames[iState];
}
#endif
