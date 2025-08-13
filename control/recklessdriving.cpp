
/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    recklessdriving.cpp
// PURPOSE : Some code to detect the player driving recklessly.
// AUTHOR :  Obbe.
// CREATED : 2/8/06
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef _RECKLESSDRIVING_H_
	#include "control/recklessdriving.h"
#endif

// rage includes
#include "grcore/debugdraw.h"
#include "fwvehicleai/pathfindtypes.h"

// game includes
#include "ai/stats.h"
#include "peds/ped.h"
#include "peds/PlayerSpecialAbility.h"
#include "control/trafficlights.h"
#include "vehicleai/pathfind.h"
#include "vehicles/bike.h"
#include "physics/gtamaterial.h"
#include "physics/gtamaterialmanager.h"
#include "modelinfo/vehiclemodelinfo.h"
#include "stats/StatsMgr.h"

using namespace AIStats;

AI_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : CPlayerInfo::UpdateRecklessDriving
// PURPOSE :  Detects whether the player has been performing dangerous driving
//			  and sets corresponding variables.
/////////////////////////////////////////////////////////////////////////////////
const float sfPlayerRunTrafficLightSpeed = 5.0f;

void CPlayerInfo::UpdateRecklessDriving()
{
	PF_FUNC(UpdateRecklessDriving);

	Assert(m_pPlayerPed);

	bool bNodeSearchDone = false;

	// We can only be accused of reckless driving if we're actually driving a vehicle
	if(m_pPlayerPed->GetMyVehicle())
	{
		m_pPlayerPed->GetMyVehicle()->m_nVehicleFlags.bShouldBeBeepedAt = false;
		m_pPlayerPed->GetMyVehicle()->m_nVehicleFlags.bPlayerDrivingAgainstTraffic = false;
	}

	if(	m_pPlayerPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) &&
		m_pPlayerPed->GetMyVehicle() &&
		(m_pPlayerPed->GetMyVehicle()->GetDriver() == m_pPlayerPed) &&
		((m_pPlayerPed->GetMyVehicle()->GetVehicleType() == VEHICLE_TYPE_CAR) || (m_pPlayerPed->GetMyVehicle()->GetVehicleType() == VEHICLE_TYPE_BIKE)|| (m_pPlayerPed->GetMyVehicle()->GetVehicleType() == VEHICLE_TYPE_QUADBIKE) || (m_pPlayerPed->GetMyVehicle()->GetVehicleType() == VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE)))
	{
		CVehicle* pPlayerVeh = m_pPlayerPed->GetMyVehicle();
		Assert(pPlayerVeh);

		m_bPlayerOnWideRoad = false;

		const Vector3& vPlayerVehVelocity = pPlayerVeh->GetVelocity();
		if(vPlayerVehVelocity.Mag2() > sfPlayerRunTrafficLightSpeed*sfPlayerRunTrafficLightSpeed)
		{
			CNodeAddress	Node1, Node2;
			s16 regionOut = -1;
			s8 nearestLane = 0;

			// First find the nearest node segment.
			bNodeSearchDone = true;
			m_bPlayerOnHighway = false;
			m_PlayerRoadNode = EmptyNodeAddress;
			m_iPlayerRoadLinkLocalIndex = -1;

			// check for drifting
			float totalSideSlip = 0.f;
			for (s32 i = 0; i < pPlayerVeh->GetNumWheels(); ++i)
			{
				CWheel* wheel = pPlayerVeh->GetWheel(i);
				totalSideSlip += wheel->GetSideSlipAngle();
			}
			if (totalSideSlip > 5.f)
			{
				CPlayerSpecialAbilityManager::ChargeEvent(ACET_DRIFTING, m_pPlayerPed, totalSideSlip);
			}

			const Vector3 vPlayerVehPosition = VEC3V_TO_VECTOR3(pPlayerVeh->GetVehiclePosition());
			const Vector3 vPlayerVehForwardDir = VEC3V_TO_VECTOR3(pPlayerVeh->GetVehicleForwardDirection());

			if(ThePaths.FindNodePairToMatchVehicle(vPlayerVehPosition, vPlayerVehForwardDir, Node1, Node2, false, regionOut, nearestLane, DEFAULT_REJECT_JOIN_TO_ROAD_DIST, false, m_RecklessDrivingHintFrom, m_RecklessDrivingHintTo))
			{
				m_RecklessDrivingHintFrom = Node1;
				m_RecklessDrivingHintTo = Node2;
			}
			else
			{
				m_RecklessDrivingHintFrom.SetEmpty();
				m_RecklessDrivingHintTo.SetEmpty();
			}

			if(!Node1.IsEmpty())// We actually found some node.
			{
				Vector3 Coors1;
				const CPathNode* pNode1 = ThePaths.FindNodePointer(Node1);
				Assert(pNode1);
				if (pNode1->IsHighway())
				{
					m_bPlayerOnHighway = true;
				}
				pNode1->GetCoors(Coors1);

				Vector3 Coors2;
				Assert(!Node2.IsEmpty());
				const CPathNode* pNode2 = ThePaths.FindNodePointer(Node2);
				Assert(pNode2);
				pNode2->GetCoors(Coors2);
			
#if __BANK
				// Display the line we have found
				if (ms_bDisplayRecklessDriving)
				{
					grcDebugDraw::Line(Coors1+Vector3(0.0f, 0.0f, 1.0f), Coors2+Vector3(0.0f, 0.0f, 1.0f), Color32( 255, 255, 255, 255 ));
					//CVehicle::ms_debugDraw.AddLine(VECTOR3_TO_VEC3V(Coors1+Vector3(0.0f, 0.0f, 1.0f)), VECTOR3_TO_VEC3V(Coors2+Vector3(0.0f, 0.0f, 1.0f)), Color_white);
				}
#endif // __BANK

				// Make sure we travel from node1 to node2
				Vector3 linkDir = Coors2 - Coors1;
				linkDir.Normalize();
				float Velocity = linkDir.Dot(vPlayerVehVelocity);
				if (Velocity < 0.0f)
				{
					CNodeAddress Temp = Node1;
					Node1 = Node2;
					Node2 = Temp;

					const CPathNode * pTmpNode = pNode1;
					pNode1 = pNode2;
					pNode2 = pTmpNode;

					Vector3	TempV = Coors1;
					Coors1 = Coors2;
					Coors2 = TempV;

					linkDir = -linkDir;
					Velocity = -Velocity;
				}

				// We have to recalculate the link as the nodes may have switched round.
				s16 iLink = -1;
				const bool bLinkFound = ThePaths.FindNodesLinkIndexBetween2Nodes(Node1, Node2, iLink);

				if(bLinkFound)
				{
					const CPathNodeLink* pLink = &ThePaths.GetNodesLink(pNode1, iLink);
					Assert(pLink);

					//save this off
					m_PlayerRoadNode = Node1;
					m_iPlayerRoadLinkLocalIndex = iLink;

	//				if (pLink->m_1.m_LanesToOtherNode + pLink->m_1.m_LanesFromOtherNode >= 3)
					if (pLink->m_1.m_LanesToOtherNode >= 2 || pLink->m_1.m_LanesFromOtherNode >= 2)
					{
						m_bPlayerOnWideRoad = true;
					}

					// Is there a traffic light on this link?
					// Make sure player travels at a decent speed in the direction past the traffic light
					if (pNode2->IsJunctionNode() && Velocity > sfPlayerRunTrafficLightSpeed)
					{
						Vector3 MidPoint = (Coors1 + Coors2) * 0.5f;
						if ((vPlayerVehPosition - MidPoint).Dot(linkDir) >= 0.0f)
						{
							if ((pPlayerVeh->GetPreviousPosition() - MidPoint).Dot(linkDir) < 0.0f)
							{
								const CJunction* pJunction = CJunctions::FindJunctionFromNode(Node2);
								if (pJunction)
								{
									const s32 iEntrance = pJunction->FindEntranceIndexWithNode(Node1);
									if (iEntrance >= 0)
									{
										bool bGoingRightOnRed = false;
										CVehicleNodeList tempNodeList;

										const s16 iRegionLinkIndex = (s16)ThePaths.GetNodesRegionLinkIndex(pNode1, iLink);
										tempNodeList.Set(0, Node1, iRegionLinkIndex, nearestLane);
										tempNodeList.SetPathNodeAddr(1, Node2);
										tempNodeList.SetTargetNodeIndex(1);
										ETrafficLightCommand iCmd = pJunction->GetTrafficLightCommand(pPlayerVeh, iEntrance, bGoingRightOnRed, false, &tempNodeList);

										//we don't care about going right on red here--if the player's turning right 
										//we shouldn't honk at him
										if (iCmd == TRAFFICLIGHT_COMMAND_STOP)
										{
											m_LastTimePlayerRanLight = fwTimer::GetTimeInMilliseconds();
											//CCrime::ReportCrime(CRIME_RUN_REDLIGHT, NULL, m_pPlayerPed);		1 star light wanted level was confusing. Removed
										}
									}
								}
							}
						}
					}

					// Work out whether we drive against the flow of traffic.
					if (Velocity > 5.0f)
					{		// Make sure we go at a decent speed.
						// Make sure car is sufficiently alligned with road (don't want to trigger on side roads)
						if (rage::Abs(DotProduct(linkDir, VEC3V_TO_VECTOR3(pPlayerVeh->GetTransform().GetB()))) > 0.7f)
						{
							if (pLink->m_1.m_LanesToOtherNode == 0 && pLink->m_1.m_LanesFromOtherNode != 0)
							{
								m_LastTimePlayerDroveAgainstTraffic = fwTimer::GetTimeInMilliseconds();
	//							CCrime::ReportCrime(CRIME_DRIVE_AGAINST_TRAFFIC, NULL, m_pPlayerPed);				1 star light wanted level was confusing. Removed
	//							HaveSomeoneBeepAtPlayer();
								CPlayerSpecialAbilityManager::ChargeEvent(ACET_DRIVING_TOWARDS_ONCOMING, m_pPlayerPed);
							}
							else if (pLink->m_1.m_LanesFromOtherNode != 0 && !pLink->IsSingleTrack())
							{	// If we are to the left of the center line we go against traffic.
								Vector3 SideVec;
								SideVec.x = linkDir.y;
								SideVec.y = -linkDir.x;
								SideVec.z = 0.0f;
								SideVec.Normalize();
								float SideDist = (vPlayerVehPosition - Coors1).Dot(SideVec);
								if (SideDist < -2.5f)
								{
									m_LastTimePlayerDroveAgainstTraffic = fwTimer::GetTimeInMilliseconds();
		//							CCrime::ReportCrime(CRIME_DRIVE_AGAINST_TRAFFIC, NULL, m_pPlayerPed);
									CPlayerSpecialAbilityManager::ChargeEvent(ACET_DRIVING_TOWARDS_ONCOMING, m_pPlayerPed);
								}
							}
						}
					}
				}

			}
		}

		// Work out whether we're driving on the pavement
		// can do this for any vehicle type now
		bool bOnPavement = true;
		const int playerVehNumWheels = pPlayerVeh->GetNumWheels();
		for (s32 i=0; i<playerVehNumWheels; i++)
		{
			if (!pPlayerVeh->GetWheel(i)->GetIsTouching())
			{
				bOnPavement = false;
			}
			else
			{
				bOnPavement = bOnPavement && pPlayerVeh->GetWheel(i)->GetIsTouchingPavement();
			}
		}

		static dev_float s_fVelThresholdForDrivingOnPavementSqr = 1.0f * 1.0f;
		if (bOnPavement && vPlayerVehVelocity.Mag2() > s_fVelThresholdForDrivingOnPavementSqr)
		{
			m_LastTimePlayerDroveOnPavement = fwTimer::GetTimeInMilliseconds();
		}

		const CCollisionHistory* pCollisionHistory = pPlayerVeh->GetFrameCollisionHistory();
		if(pCollisionHistory->GetNumCollidedEntities() > 0)
		{
			const CCollisionRecord* pCurrent;

			//Buildings
			if(pCollisionHistory->HasCollidedWithAnyOfTypes(ENTITY_TYPE_MASK_BUILDING | ENTITY_TYPE_MASK_ANIMATED_BUILDING))
			{
				m_LastTimePlayerHitBuilding = fwTimer::GetTimeInMilliseconds();
			}

			//Vehicles
			if(pCollisionHistory->HasCollidedWithAnyOfTypes(ENTITY_TYPE_MASK_VEHICLE))
			{
				m_LastTimePlayerHitCar = fwTimer::GetTimeInMilliseconds();
			}

			//Peds
			pCurrent = pCollisionHistory->GetFirstPedCollisionRecord();
			while(pCurrent)
			{
				if(pCurrent->m_pRegdCollisionEntity.Get() != static_cast<CEntity*>(m_pPlayerPed.Get()))
					m_LastTimePlayerHitPed = fwTimer::GetTimeInMilliseconds();

				pCurrent = pCurrent->GetNext();
			}

			//Objects
			pCurrent = pCollisionHistory->GetFirstObjectCollisionRecord();
			while(pCurrent)
			{
				CObject* pObject = (CObject*)pCurrent->m_pRegdCollisionEntity.Get();

				if(pObject)
				{
					// Make sure object is of a certain size
					if (Verifyf(pObject->GetModelIndex() != fwModelId::MI_INVALID, "Collided entity has an invalid model index"))
					{
						CBaseModelInfo* pModelInfo = pObject->GetBaseModelInfo();
						Assert(pModelInfo);
						if (pModelInfo->GetBoundingBoxMax().z - pModelInfo->GetBoundingBoxMin().z > 1.5f)
						{
							m_LastTimePlayerHitObject = fwTimer::GetTimeInMilliseconds();
							break;
						}
					}
				}

				pCurrent = pCurrent->GetNext();
			}
		}

		TUNE_GROUP_INT(RECKLESS_DRIVING, HonkForRunningLightsWithin, 2000, 0, 100000, 1);
		TUNE_GROUP_INT(RECKLESS_DRIVING, HonkForDriveOnPavementWithin, 500, 0, 100000, 1);	
		//TUNE_GROUP_INT(RECKLESS_DRIVING, HonkForHitAnotherVehicleWithin, 500, 0, 100000, 1);
		if (fwTimer::GetTimeInMilliseconds()-m_LastTimePlayerRanLight < HonkForRunningLightsWithin ||
			fwTimer::GetTimeInMilliseconds()-m_LastTimePlayerDroveOnPavement < HonkForDriveOnPavementWithin)
		{
			m_pPlayerPed->GetMyVehicle()->m_nVehicleFlags.bShouldBeBeepedAt = true;
		}

		TUNE_GROUP_INT(RECKLESS_DRIVING, HonkForDriveAgainstTrafficWithin, 2000, 0, 100000, 1);
		if (fwTimer::GetTimeInMilliseconds()-m_LastTimePlayerDroveAgainstTraffic < HonkForDriveAgainstTrafficWithin)
		{
			m_pPlayerPed->GetMyVehicle()->m_nVehicleFlags.bPlayerDrivingAgainstTraffic = true;
		}
	}

		// If we haven't found the nearest node yet we will once in a while do a node scan to find out whether the player is on a highway (this reduces
		// the ped population by 50%)
	if(!bNodeSearchDone && ((fwTimer::GetSystemFrameCount() & 127) == 95))
	{
		CNodeAddress	Node1, Node2;
		s16 regionOut = -1;
		s8 nearestLane = 0;
		m_bPlayerOnHighway = false;
		m_PlayerRoadNode = EmptyNodeAddress;
		m_iPlayerRoadLinkLocalIndex = -1;

		const Vector3 vPlayerPosition = VEC3V_TO_VECTOR3(m_pPlayerPed->GetTransform().GetPosition());
		const Vector3 vPlayerForwardDir = VEC3V_TO_VECTOR3(m_pPlayerPed->GetTransform().GetForward());

		//ThePaths.FindNodePairWithLineClosestToCoors(VEC3V_TO_VECTOR3(m_pPlayerPed->GetTransform().GetPosition()), &Node1, &Node2, &Orientation, 0.0f, 15.0f, false, false, 0);
		ThePaths.FindNodePairToMatchVehicle(vPlayerPosition, vPlayerForwardDir, Node1, Node2, false, regionOut, nearestLane);
		if (!Node1.IsEmpty())
		{
			const CPathNode* pNode1 = ThePaths.FindNodePointer(Node1);
			if (pNode1->IsHighway())
			{
				m_bPlayerOnHighway = true;
			}

			m_PlayerRoadNode = Node1;
			m_iPlayerRoadLinkLocalIndex = regionOut - pNode1->m_startIndexOfLinks;
		}
	}

	// Test whether there are nodes at our height. If not we should drop the height requirement when creating vehicles.
	if((fwTimer::GetSystemFrameCount() & 127) == 27)
	{
		m_bNodesNearbyInZ = ThePaths.IsNodeNearbyInZ(VEC3V_TO_VECTOR3(m_pPlayerPed->GetTransform().GetPosition()));
	}

	// Print out some debug stuff.
#if __BANK
	if (ms_bDisplayRecklessDriving)
	{
		grcDebugDraw::AddDebugOutput("Time since player hit car: %d", GetTimeSincePlayerHitCar());
		grcDebugDraw::AddDebugOutput("Time since player hit ped: %d", GetTimeSincePlayerHitPed());
		grcDebugDraw::AddDebugOutput("Time since player hit building: %d", GetTimeSincePlayerHitBuilding());
		grcDebugDraw::AddDebugOutput("Time since player hit object: %d", GetTimeSincePlayerHitObject());
		grcDebugDraw::AddDebugOutput("Time since player drove on pavement: %d", GetTimeSincePlayerDroveOnPavement());
		grcDebugDraw::AddDebugOutput("Time since player ran light: %d", GetTimeSincePlayerRanLight());
		grcDebugDraw::AddDebugOutput("Time since player drove against traffic: %d", GetTimeSincePlayerDroveAgainstTraffic());
	}
#endif
}

static const float NEAR_MISS_MARGIN = 0.9f;
bool DoBoundsIntersect(const phBound& boundA, Mat34V_In wldMtxA, const phBound& boundB, Mat34V_In wldMtxB)
{
	// get the centre and half width of both bounds
	Vec3V vHalfWidthsA, vHalfWidthsB;
	Vec3V vCentreA, vCentreB;
	boundA.GetBoundingBoxHalfWidthAndCenter(vHalfWidthsA, vCentreA);
	boundB.GetBoundingBoxHalfWidthAndCenter(vHalfWidthsB, vCentreB);

	// apply margin
	Vec3V vMargin = Vec3VFromF32(NEAR_MISS_MARGIN);
	vHalfWidthsA += vMargin;
	vHalfWidthsB += vMargin;

	vHalfWidthsA.SetW(ScalarV(V_ZERO));
	vHalfWidthsB.SetW(ScalarV(V_ZERO));

	// adjust the matrices so that they are positioned in the centre of the bounding box
	Mat34V mtxA = wldMtxA;
	Vec3V offsetA = Transform3x3(mtxA, vCentreA);
	mtxA.SetCol3(mtxA.GetCol3() + offsetA);

	Mat34V mtxB = wldMtxB;
	Vec3V offsetB = Transform3x3(mtxB, vCentreB);
	mtxB.SetCol3(mtxB.GetCol3() + offsetB);

	// calculate matrix A relative to matrix B
	Mat34V invParentMtx;
	InvertTransformOrtho(invParentMtx, mtxB);
	Mat34V ARelativeToBMtx;
	Transform(ARelativeToBMtx, invParentMtx, mtxA);

	// test if the two bounds intersect
	return geomBoxes::TestBoxToBoxOBB(RCC_VECTOR3(vHalfWidthsA), RCC_VECTOR3(vHalfWidthsB), RCC_MATRIX34(ARelativeToBMtx));
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : NearVehicleMiss
// PURPOSE :  Records a near vehicle miss when player is driving
/////////////////////////////////////////////////////////////////////////////////
void CPlayerInfo::NearVehicleMiss(const CVehicle* playerVehicle, const CVehicle* otherVehicle, bool trackPreciseNearMiss /* = false */)
{
	// make sure it's the player info is correct
	CPed* playerDriver = playerVehicle->GetDriver();
	if (!playerDriver)
	{
		return;
	}

	if (playerDriver->GetPlayerInfo() != this)
	{
		return;
	}

	static float PLAYER_MINIMUM_SPEED = 6.0f;
	static float PLAYER_MINIMUM_SPEED_FOR_PRECISE = 9.0f;

	// early out if player drives too slow
	if(trackPreciseNearMiss)
	{
		if(playerVehicle->GetVelocity().Mag2() < rage::square(PLAYER_MINIMUM_SPEED_FOR_PRECISE))
		{
			return;
		}
	}
	else
	{
		if(playerVehicle->GetVelocity().Mag2() < rage::square(PLAYER_MINIMUM_SPEED))
		{
			return;
		}
	}

	// early out if player has crashed in the last second
	u32 mostRecentCollisionsTime = playerVehicle->GetFrameCollisionHistory()->GetMostRecentCollisionTime();
	if ( mostRecentCollisionsTime != COLLISION_HISTORY_TIMER_NEVER_HIT 
		&& fwTimer::GetTimeInMilliseconds() < mostRecentCollisionsTime + CStatsMgr::NEAR_MISS_TIMER)
	{
		return;
	}
	

	if(!trackPreciseNearMiss)
	{
		//ignore trailers - parent vehicle will be the near miss and stationary cars (e.g. parked)
		if(otherVehicle->GetVelocity().Mag2() <= 0.0001f || otherVehicle->InheritsFromTrailer())
		{
			return;
		}
	}
	else
	{
		if(otherVehicle->GetDriver() == NULL)
		{
			return;
		}
	}

	// go through entire ring buffer and make sure the vehicle hasn't been added recently
	u8 next = m_nearVehiclePut;
    do
	{
		if (m_nearVehicles[next].veh == otherVehicle && m_nearVehicles[next].time + 2000 > fwTimer::GetTimeInMilliseconds())
		{
			return;
		}

		next = (next - 1) & NEAR_VEHICLE_MASK;
	} while (next != m_nearVehiclePut);

    // if we got this far we have a new vehicle as a near miss, or an old one that's still close
    // this could trigger if we drive along side a vehicle for a long time at a high speed
	Assert(!otherVehicle->GetIsInPopulationCache());
    Assert(otherVehicle->GetCurrentPhysicsInst()->GetArchetype());
	phBound* otherBound = otherVehicle->GetCurrentPhysicsInst()->GetArchetype()->GetBound();

	Assert(playerVehicle->GetCurrentPhysicsInst()->GetArchetype());
	phBound* playerBound = playerVehicle->GetCurrentPhysicsInst()->GetArchetype()->GetBound();

	if (DoBoundsIntersect(*playerBound, playerVehicle->GetMatrix(), *otherBound, otherVehicle->GetMatrix()))
    {
		float velDiffSqr = (playerVehicle->GetVelocity() - otherVehicle->GetVelocity()).Mag2();
        CPlayerSpecialAbilityManager::ChargeEvent(ACET_NEAR_VEHICLE_MISS, playerVehicle->GetDriver(), velDiffSqr, mostRecentCollisionsTime);

		if(trackPreciseNearMiss)
		{
			bool hasAvoidedANewVehicle = true;
			for(int i = 0; i < NUMBER_OF_DRIVERS_NEAR_MISS; i++)
			{
				if(playerDriver->GetPlayerInfo()->m_lastNearMissedVehicles[i] == otherVehicle)
				{
					hasAvoidedANewVehicle = false;
					break;
				}
			}
			
			if(hasAvoidedANewVehicle)
			{
				if(m_nearMissedDriverQueueIndex > NUMBER_OF_DRIVERS_NEAR_MISS - 1)
				{
					m_nearMissedDriverQueueIndex = 0;
				}
				playerDriver->GetPlayerInfo()->m_lastNearMissedVehicles[m_nearMissedDriverQueueIndex] = otherVehicle;
				m_nearMissedDriverQueueIndex++;
				CStatsMgr::RegisterVehicleNearMissPrecise(playerVehicle->GetDriver(), fwTimer::GetTimeInMilliseconds());
			}
		}
		else
		{
			CStatsMgr::RegisterVehicleNearMiss(playerVehicle->GetDriver(), mostRecentCollisionsTime);

			m_nearVehiclePut = (m_nearVehiclePut + 1) & NEAR_VEHICLE_MASK;
			m_nearVehicles[m_nearVehiclePut].veh = otherVehicle;
			m_nearVehicles[m_nearVehiclePut].time = fwTimer::GetTimeInMilliseconds();
		}

		static float s_fMinSpeedToSay = 20.0f;
		static u32 s_uMinTimeDrivingAgainstTrafficToSay = 500;

		if (GetTimeSincePlayerDroveAgainstTraffic() < s_uMinTimeDrivingAgainstTrafficToSay)
		{
			if (playerVehicle->GetVelocity().Mag2() >= s_fMinSpeedToSay * s_fMinSpeedToSay)
			{
				static float s_fChancesToSay = 0.15f;
				playerDriver->RandomlySay("NEAR_MISS_VEHICLE", s_fChancesToSay);
			}
		}
    }
}

const CVehicle* CPlayerInfo::GetLastNearMissVehicle() const
{
    const CVehicle* retVeh = NULL;
    if (m_nearVehicles[m_nearVehiclePut].veh && m_nearVehicles[m_nearVehiclePut].time + 2000 > fwTimer::GetTimeInMilliseconds())
    {
        retVeh = m_nearVehicles[m_nearVehiclePut].veh;
    }
    return retVeh;
}

u32 CPlayerInfo::GetTimeSinceLastNearMiss() const
{
    if (m_nearVehicles[m_nearVehiclePut].veh)
    {
        return fwTimer::GetTimeInMilliseconds() - m_nearVehicles[m_nearVehiclePut].time;
    }
    return ~(u32)0;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : GetTimeSincePlayerHitCar, GetTimeSincePlayerHitPed etc
// PURPOSE :  Access functions for the reckless driving.
/////////////////////////////////////////////////////////////////////////////////

u32 CPlayerInfo::GetTimeSincePlayerHitCar()
{
	if (m_LastTimePlayerHitCar)
	{
		return fwTimer::GetTimeInMilliseconds() - m_LastTimePlayerHitCar;
	}
	return ~(u32)0;
}

u32 CPlayerInfo::GetTimeSincePlayerHitPed()
{
	if (m_LastTimePlayerHitPed)
	{
		return fwTimer::GetTimeInMilliseconds() - m_LastTimePlayerHitPed;
	}
	return ~(u32)0;
}

u32 CPlayerInfo::GetTimeSincePlayerHitBuilding()
{
	if (m_LastTimePlayerHitBuilding)
	{
		return fwTimer::GetTimeInMilliseconds() - m_LastTimePlayerHitBuilding;
	}
	return ~(u32)0;
}

u32 CPlayerInfo::GetTimeSincePlayerHitObject()
{
	if (m_LastTimePlayerHitObject)
	{
		return fwTimer::GetTimeInMilliseconds() - m_LastTimePlayerHitObject;
	}
	return ~(u32)0;
}

u32 CPlayerInfo::GetTimeSincePlayerDroveOnPavement()
{
	if (m_LastTimePlayerDroveOnPavement)
	{
		return fwTimer::GetTimeInMilliseconds() - m_LastTimePlayerDroveOnPavement;
	}
	return ~(u32)0;
}

u32 CPlayerInfo::GetTimeSincePlayerRanLight()
{
	if (m_LastTimePlayerRanLight)
	{
		return fwTimer::GetTimeInMilliseconds() - m_LastTimePlayerRanLight;
	}
	return ~(u32)0;
}

u32 CPlayerInfo::GetTimeSincePlayerDroveAgainstTraffic()
{
	if (m_LastTimePlayerDroveAgainstTraffic)
	{
		return fwTimer::GetTimeInMilliseconds() - m_LastTimePlayerDroveAgainstTraffic;
	}
	return ~(u32)0;
}

