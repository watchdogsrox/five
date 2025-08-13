#include "VehicleNavigationHelper.h"

#include "physics\PhysicsHelpers.h"
#include "Vehicles/Vehicle.h"
#include "debug/DebugScene.h"
#include "scene/world/gameWorld.h"
#include "peds/Ped.h"
#include "physics\PhysicsHelpers.h"
#include "system/ControlMgr.h"
#include "vehicleAi\task\TaskVehiclePark.h"
#include "scene\world\GameWorldHeightMap.h"
#include "physics/WorldProbe/worldprobe.h"

AI_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()

int VehicleNavigationHelper::s_iMaxNumProbes = 5;

VehicleNavigationHelper::VehicleNavigationHelper() :
m_vTargetPosition(),
m_vLastValidTarget(),
m_vCurrentGoToTarget(),
m_fSearchTimer( 1.0f ),
m_fSearchTime( 1.0f ),
m_fSightDistance(100.0f),
m_pVehicle(NULL),
m_pTarget(NULL),
m_bIntermediaryGoToValid(false),
m_bIsPerformingSearch(false),
m_bStartNewSearch(false),
m_currentSearchType(eSearchStart),
m_navigationProbeResults(),
m_navigationTargets(),
m_iNumProbesUsed(0)
{
	m_navigationProbeResults = rage_new WorldProbe::CShapeTestSingleResult[s_iMaxNumProbes*2];
	m_navigationTargets = rage_new Vector3[s_iMaxNumProbes];
}

VehicleNavigationHelper::~VehicleNavigationHelper()
{
	delete[] m_navigationProbeResults;
	delete[] m_navigationTargets;
}

float VehicleNavigationHelper::ComputeTargetZToAvoidHeightMap(const Vector3& vPosition, float fSpeed,
															  const Vector3& vTargetPosition, float minHeight, 
															  fp_HeightQuery in_fpHeightQuery, bool checkCurrentPosition /* = true */,
															  bool heightAtFullDistance /* = true */)
{
	Vector3	vVehiclePos(vPosition);
	float vehicleZ = in_fpHeightQuery(vVehiclePos.x, vVehiclePos.y);
	
	vehicleZ += minHeight;

	if ( checkCurrentPosition && vVehiclePos.z < vehicleZ )
	{
		// need to go straight up
		return 10000000.0f;
	}

	Vector3 vVehicleDirXY = vTargetPosition - vVehiclePos;
	float fDistXY = vVehicleDirXY.XYMag();
	float targetZ = vTargetPosition.z;

	// initialize the target position 
	float terrainZ = in_fpHeightQuery(vTargetPosition.x, vTargetPosition.y);
	terrainZ += minHeight;
	targetZ = Max(terrainZ, targetZ);

	if ( fDistXY > 0.001f )
	{
		vVehicleDirXY.z = 0.0f;
		vVehicleDirXY *= 1/ fDistXY;
		float maxSlope = targetZ / fDistXY;
		maxSlope *= Sign(vTargetPosition.z - vVehiclePos.z);

		int iNumHeightmapFutureSamples = 10;
		for (int i = 0; i < iNumHeightmapFutureSamples; i++)
		{
			float t = (float)(i+1);// * (float)iNumHeightmapFutureSamples;

			float xyLength = fSpeed * t;
			// don't check beyond target
			if ( xyLength >= fDistXY )
			{
				break;
			}

			Vector3 vSamplePosition = vVehiclePos + vVehicleDirXY * xyLength;
			// sample heightmap
			vSamplePosition.z = in_fpHeightQuery(vSamplePosition.x, vSamplePosition.y);
			vSamplePosition.z += minHeight;

#if DEBUG_DRAW
			static bool s_DrawHeightMapTests = false;
			if ( s_DrawHeightMapTests )	grcDebugDraw::Sphere(vSamplePosition, 1.5f, Color_red, true, 1);
#endif

			// compute our slope dy/dx for sample point
			Vector3 vDelta = vSamplePosition - vVehiclePos;
			if(xyLength > SMALL_FLOAT)
			{
				float slope = vDelta.z / xyLength;

				if ( slope > maxSlope )
				{
					maxSlope = slope;

					if(heightAtFullDistance)
					{
						// compute our target z at full distance z = mx + b
						targetZ = (slope * fDistXY) + vVehiclePos.z;
					}
					else
					{
						targetZ = vSamplePosition.z;
					}				
				}
			}
		}
	}

	//make sure not to return lower than target
	targetZ = Max(targetZ, vTargetPosition.z);
	return targetZ;
}

void VehicleNavigationHelper::HelperExcludeCollisionEntities(WorldProbe::CShapeTestDesc& o_ShapeTest, const CEntity& in_Entity)
{
	if ( in_Entity.GetIsTypeVehicle() )
	{
		const CVehicle& vehicle = static_cast<const CVehicle&>(in_Entity);	
		const CEntity* pEntity = vehicle.GetEntityBeingTowed();
		if ( pEntity )
		{
			o_ShapeTest.AddExludeInstance(pEntity->GetCurrentPhysicsInst());
			HelperExcludeCollisionEntities(o_ShapeTest, *pEntity);
		}
	}

	// grab the attachments
	CEntity* pChild = static_cast<CEntity*>(in_Entity.GetChildAttachment());
	while ( pChild )
	{
		o_ShapeTest.AddExludeInstance(pChild->GetCurrentPhysicsInst());
		HelperExcludeCollisionEntities(o_ShapeTest, *pChild);
		pChild = static_cast<CEntity*>(pChild->GetSiblingAttachment());				
	}
}

bool VehicleNavigationHelper::CanNavigateOverObject(const Vector3& collisionPoint, const Vector3& end, int numProbes /*= 10*/, float depthOffset /*= -5.0f */)
{
#if __DEV
	static bool s_drawDebug = false;
#endif

	Vector3 intersectionPoint(0.0f, 0.0f, 0.0f);
	//do a check from target to our collide point, to get the end of the object between us
	if(WorldCollisionTest(intersectionPoint, end, collisionPoint, NULL))
	{
		bool doVerticalTests = false;

		if(doVerticalTests)
		{
			Vector3 offset = intersectionPoint - collisionPoint;
			Vector3 probeOffset = offset / (float)numProbes;
#if __DEV
			if(s_drawDebug)
			{
				grcDebugDraw::Sphere(collisionPoint, 0.5f, Color_green, true, -1);
				grcDebugDraw::Sphere(intersectionPoint, 0.5f, Color_red, true, -1);
				grcDebugDraw::Line(collisionPoint, intersectionPoint, Color_green, -1);
			}
#endif // __DEV

			for (int i = 0; i < numProbes; i++)
			{
				Vector3 vSamplePosition = collisionPoint + (probeOffset * (float)(i+1));
				vSamplePosition.z = 100.0f;

				float fWaterHeight;
				if(!Water::GetWaterLevelNoWaves(vSamplePosition, &fWaterHeight, POOL_DEPTH, 999999.9f, NULL))
				{
#if __DEV
					if(s_drawDebug)
					{
						Vector3 vEndPos = vSamplePosition;
						vEndPos.z = vSamplePosition.z - 200.0f;
						grcDebugDraw::Line(vSamplePosition, vEndPos, Color_red, -1);
					}
#endif // __DEV

					return false;
				}

#if __DEV
				if(s_drawDebug)
				{
					Vector3 vEndPos = vSamplePosition;
					vEndPos.z = fWaterHeight + depthOffset;
					grcDebugDraw::Line(vSamplePosition, vEndPos, Color_green, -1);
				}
#endif // __DEV
			}
		}
		else
		{
			float fWaterHeight;
			if(Water::GetWaterLevelNoWaves(collisionPoint, &fWaterHeight, POOL_DEPTH, 999999.9f, NULL))
			{
				fWaterHeight += depthOffset;
				Vector3 vStart = collisionPoint;
				vStart.z = fWaterHeight;
				Vector3 vEnd = intersectionPoint;
				vEnd.z = fWaterHeight;

				Vector3 tmpPoint(0.0f, 0.0f, 0.0f);
				//check along surface for any objects
				if(WorldCollisionTest(tmpPoint, vStart, vEnd, NULL))
				{
					return false;
				}
			}
		}
	}

	return true;
}

bool VehicleNavigationHelper::CheckHasLineOfSight(const Vector3& vStart, const Vector3& vEnd, const CEntity* pVehicle, const CEntity* pTargetVehicle, Vector3& collidePoint)
{
	WorldProbe::CShapeTestProbeDesc probeDesc;
	WorldProbe::CShapeTestFixedResults<> probeResult;
	probeDesc.SetResultsStructure(&probeResult);
	probeDesc.SetStartAndEnd(vStart, vEnd);
	if(pVehicle)
	{
		probeDesc.SetExcludeEntity(pVehicle);
	}
	if(pTargetVehicle)
	{
		probeDesc.SetExcludeEntity(pTargetVehicle);
	}

	probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);

	if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
	{
		collidePoint = probeResult[0].GetHitPosition();
		return false;
	}

	return true;
}

bool VehicleNavigationHelper::WorldCollisionTest(Vector3& intersectionPoint_out, const Vector3& vStart, const Vector3 &vEnd, const CEntity* pException)
{
	WorldProbe::CShapeTestProbeDesc probeDesc;
	WorldProbe::CShapeTestFixedResults<> probeResult;
	probeDesc.SetResultsStructure(&probeResult);
	probeDesc.SetStartAndEnd(vStart, vEnd);
	if(pException)
	{
		probeDesc.SetExcludeEntity(pException);
	}

	probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);

	if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
	{
		intersectionPoint_out = probeResult[0].GetHitPosition();
		return true;
	}

	return false;
}

void VehicleNavigationHelper::LimitEndByDistance(const Vector3& startPos, Vector3& endPos, float distanceLimit)
{
	Vector3 lineDir = endPos - startPos;
	float lineDist2 = lineDir.Mag2();
	if(lineDist2 > distanceLimit*distanceLimit)
	{
		lineDir.Normalize();
		endPos = startPos + (lineDir * distanceLimit);
	}
}

bool VehicleNavigationHelper::CanNavigateStraightTo(const Vector3& currentPos, const Vector3& targetPos)
{
	Vector3 outPos = targetPos;
	LimitEndByDistance(currentPos, outPos, m_fSightDistance);

	Vector3 collidePoint;
	if(!CheckHasLineOfSight(currentPos,outPos, m_pVehicle, m_pTarget, collidePoint))
	{
		if(!m_pVehicle->InheritsFromSubmarine() && !m_pVehicle->InheritsFromSubmarineCar())
		{
			return true;
		}

		//subs can't navigate to objects that are out the water
		return CanNavigateOverObject(collidePoint, outPos);
	}

	return true;
}

void VehicleNavigationHelper::Update(float fTimeStep, const Vector3& targetPos, bool forceUpdate /*= false*/)
{
	Assertf(m_pVehicle,"Invalid vehicle pointer when updating navigation");

	Vector3	vehPos (VEC3V_TO_VECTOR3(m_pVehicle->GetTransform().GetPosition()));

	m_vTargetPosition = targetPos;

	//we've arrived
	if ((m_vTargetPosition - vehPos).Mag() < m_fArrivalDistance)
	{
		return;
	}

	bool shouldRecheck = forceUpdate;
	bool atCurrentTarget = false;
	//we're at our current target, will need to recheck
	if (m_bIntermediaryGoToValid && (m_vCurrentGoToTarget - vehPos).Mag() < m_fArrivalDistance)
	{
		shouldRecheck = true;
		atCurrentTarget = true;
	}

	if( m_bIsPerformingSearch)
	{
		UpdateSearchResults();
	}
	else
	{
		m_fSearchTime -= fTimeStep;
		if(m_fSearchTime <= 0.0f || shouldRecheck)
		{
			m_fSearchTime = m_fSearchTimer;

			if( CanNavigateStraightTo(vehPos, m_vTargetPosition) )
			{
				m_vCurrentGoToTarget = targetPos;
				m_vLastValidTarget = targetPos;
			}
			else
			{
				if(atCurrentTarget || !m_bIntermediaryGoToValid)
				{
					//haven't got temp goto, or at current goto but still can't see target - find a new target
					CalculateNewIntermediaryTarget(vehPos);
				}
				else
				{
					//we are going to a intermediary point - can we get there
					if( m_bIntermediaryGoToValid )
					{
						if( !CanNavigateStraightTo(vehPos, m_vCurrentGoToTarget))
						{
							m_bIntermediaryGoToValid = false;

							//we can't get to our last intermediary point, need to recheck
							CalculateNewIntermediaryTarget(vehPos);
						}
					}
				}
			}

			if( m_bIsPerformingSearch)
			{
				UpdateSearchResults();
			}
		}
	}
}

void VehicleNavigationHelper::UpdateSearchResults()
{
	Assertf(m_bIsPerformingSearch, "Trying to check search results when not searching");

	if(!m_bStartNewSearch)
	{
		bool resultsReady = true;
		for(int i = 0; i < m_iNumProbesUsed*2; ++i)
		{
			resultsReady &= m_navigationProbeResults[i].GetResultsReady();
		}

		if(resultsReady)
		{
			//check results
			bool positionValid = false;

			int validIndex;
			for(validIndex = 0; validIndex < m_iNumProbesUsed; ++validIndex)
			{
				int otherIndex = validIndex + m_iNumProbesUsed;
				//check current to new, and new to original are both clear
				if ( !m_navigationProbeResults[validIndex][0].GetHitDetected() && 
					 !m_navigationProbeResults[otherIndex][0].GetHitDetected())
				{
					positionValid = true;
					break;
				}
				else
				{
					positionValid = false;

					if(m_pVehicle->InheritsFromSubmarine() || m_pVehicle->InheritsFromSubmarineCar())
					{
						positionValid = true;
						if(m_navigationProbeResults[validIndex][0].GetHitDetected())
						{
							positionValid &= CanNavigateOverObject(m_navigationProbeResults[validIndex][0].GetHitPosition(), m_vTargetPosition);
						}

						if(positionValid && m_navigationProbeResults[otherIndex][0].GetHitDetected())
						{
							positionValid &= CanNavigateOverObject(m_navigationProbeResults[otherIndex][0].GetHitPosition(), m_vTargetPosition);
						}

						if( positionValid)
						{
							break;
						}
					}
				}
			}

			if(positionValid)
			{
				m_vCurrentGoToTarget = m_navigationTargets[validIndex];
				m_bIntermediaryGoToValid = true;
				m_bIsPerformingSearch = false;
			}
			else
			{
				//try a more detailed test
				if( m_currentSearchType == eSearchBasic)
				{
					m_currentSearchType = eSearchCircle;
					m_bStartNewSearch = true;
				}
				else
				{
					m_currentSearchType = eSearchStart;

					//we really can't find out how to get there, just try going to actual target
					m_vCurrentGoToTarget = m_vTargetPosition;
					m_bIntermediaryGoToValid = true;
					m_bIsPerformingSearch = false;
				}
			}	

			//clear tests
			for(int i = 0; i < m_iNumProbesUsed*2; ++i)
			{
				m_navigationProbeResults[i].Reset();
			}
		}
	}	
	
	if(m_bStartNewSearch)
	{
		m_bStartNewSearch = false;
		spdSphere compositeSphere = m_pVehicle->GetBoundSphere();
		const Vector3 vVehiclePos = VEC3V_TO_VECTOR3(compositeSphere.GetCenter());
		
		switch (m_currentSearchType)
		{
		case eSearchBasic: //checks positions at normal to collide point
			{
				m_iNumProbesUsed = 2;
				Vector3 collidePoint(0.0f, 0.0f, 0.0f);
				//get obstruction position
				if(WorldCollisionTest(collidePoint, vVehiclePos, m_vTargetPosition, m_pTarget))
				{
					Vector3 toObstruction = collidePoint - vVehiclePos;
					toObstruction.Normalize();
					Vector3 toObstructionRight = Vector3(toObstruction.GetY(), -toObstruction.GetX(), toObstruction.GetZ());

					float rightDistance = 10.0f; //base on distance or speed or something
					for(int i = 0; i < m_iNumProbesUsed; i++)
					{
						WorldProbe::CShapeTestProbeDesc probe;
						probe.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
						probe.SetExcludeInstance(m_pVehicle->GetCurrentPhysicsInst());
						probe.SetResultsStructure(&m_navigationProbeResults[i]);
						probe.SetMaxNumResultsToUse(1);
						WorldProbe::CShapeTestProbeDesc probe2;
						probe2.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
						probe2.SetExcludeInstance(m_pVehicle->GetCurrentPhysicsInst());
						probe2.SetResultsStructure(&m_navigationProbeResults[i + m_iNumProbesUsed]);
						probe2.SetMaxNumResultsToUse(1);

						Vector3 newTargetPos = collidePoint + (toObstruction * rightDistance ) + 
												(toObstructionRight * ( i == 0 ? rightDistance : -rightDistance));

						m_navigationTargets[i] = newTargetPos;
						probe.SetStartAndEnd(newTargetPos, m_vTargetPosition); //new target to original
						probe2.SetStartAndEnd(vVehiclePos, newTargetPos); //current position to new target

// #if DEBUG_DRAW
// 						grcDebugDraw::Line(VECTOR3_TO_VEC3V(vVehiclePos),VECTOR3_TO_VEC3V(newTargetPos), Color_blue, 5);
// 						grcDebugDraw::Line(VECTOR3_TO_VEC3V(newTargetPos),VECTOR3_TO_VEC3V(m_vTargetPosition), Color_red, 5);
// #endif

						WorldProbe::GetShapeTestManager()->SubmitTest(probe, WorldProbe::PERFORM_ASYNCHRONOUS_TEST);
						WorldProbe::GetShapeTestManager()->SubmitTest(probe2, WorldProbe::PERFORM_ASYNCHRONOUS_TEST);
					}
				}
				break;
			}
		case eSearchCircle: //checks positions around the target position
			{
				m_iNumProbesUsed = s_iMaxNumProbes;

				float circleRadius = 10.0f; //base on distance or speed or something
				for(int i = 0; i < m_iNumProbesUsed; i++)
				{
					WorldProbe::CShapeTestProbeDesc probe;
					probe.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
					probe.SetExcludeInstance(m_pVehicle->GetCurrentPhysicsInst());
					probe.SetResultsStructure(&m_navigationProbeResults[i]);
					probe.SetMaxNumResultsToUse(1);
					WorldProbe::CShapeTestProbeDesc probe2;
					probe2.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
					probe2.SetExcludeInstance(m_pVehicle->GetCurrentPhysicsInst());
					probe2.SetResultsStructure(&m_navigationProbeResults[i + m_iNumProbesUsed]);
					probe2.SetMaxNumResultsToUse(1);

					//spread out points around circle
					//pre-compute these
					float angle = PI*2.0f / m_iNumProbesUsed * i;
					Vector3 direction ( cosf(angle), sinf(angle), 0.0f);

					Vector3 newTargetPos = vVehiclePos + (direction * circleRadius);
					m_navigationTargets[i] = newTargetPos;
					probe.SetStartAndEnd(newTargetPos, m_vTargetPosition);
					probe2.SetStartAndEnd(vVehiclePos, newTargetPos);

// #if DEBUG_DRAW
// 					grcDebugDraw::Line(VECTOR3_TO_VEC3V(vVehiclePos),VECTOR3_TO_VEC3V(newTargetPos), Color_blue, 5);
// 					grcDebugDraw::Line(VECTOR3_TO_VEC3V(newTargetPos),VECTOR3_TO_VEC3V(m_vTargetPosition), Color_red, 5);
// #endif

					WorldProbe::GetShapeTestManager()->SubmitTest(probe, WorldProbe::PERFORM_ASYNCHRONOUS_TEST);
					WorldProbe::GetShapeTestManager()->SubmitTest(probe2, WorldProbe::PERFORM_ASYNCHRONOUS_TEST);
				}
				break;
			}
			default:
				break;
		}
	}
}

void VehicleNavigationHelper::CalculateNewIntermediaryTarget(const Vector3& currentPos)
{
	bool useLastKnown = UseLastKnownOverSearch(currentPos);
	if(useLastKnown)
	{
		m_vCurrentGoToTarget = m_vLastValidTarget;
		m_bIntermediaryGoToValid = true;
	}
	else
	{
		m_bIsPerformingSearch = true;
		m_currentSearchType = eSearchBasic;
		m_bStartNewSearch = true;
	}
}

bool VehicleNavigationHelper::UseLastKnownOverSearch(const Vector3& currentPos)
{
	if(CanNavigateStraightTo(currentPos, m_vLastValidTarget))
	{
		//some more detailed checks - take into account time, direction etc
		return true;
	}

	return false;
}

void VehicleNavigationHelper::Init(const CVehicle* vehicle, const CEntity* pTarget, const Vector3& currentTarget)
{
	m_pVehicle = vehicle;
	m_pTarget = pTarget;

	m_vCurrentGoToTarget = currentTarget;
	m_vLastValidTarget = currentTarget;
	m_vTargetPosition = currentTarget;
}

/////////////////////////////////////////////////////////////////////////////////////
#if !__FINAL
void VehicleNavigationHelper::Debug() const
{
#if DEBUG_DRAW
	grcDebugDraw::Sphere(VECTOR3_TO_VEC3V(m_vTargetPosition), 0.1f, Color_blue, false);
	
	if(m_bIntermediaryGoToValid)
	{
		grcDebugDraw::Sphere(VECTOR3_TO_VEC3V(m_vCurrentGoToTarget), 0.1f, Color_green, false);
		if(!m_vLastValidTarget.IsEqual(m_vCurrentGoToTarget))
		{
			grcDebugDraw::Sphere(VECTOR3_TO_VEC3V(m_vLastValidTarget), 0.1f, Color_red, false);
		}	
	}
#endif

}
#endif