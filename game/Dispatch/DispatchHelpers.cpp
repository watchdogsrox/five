// Title	:	DispatchHelpers.cpp
// Purpose	:	Defines the different dispatch helpers
// Started	:	2/22/2012

//File header
#include "game/Dispatch/DispatchHelpers.h"

//Rage headers
#include "grcore/debugdraw.h"

//Game headers
#include "camera/CamInterface.h"
#include "debug/VectorMap.h"
#include "game/dispatch/DispatchData.h"
#include "game/Dispatch/Orders.h"
#include "Network/NetworkInterface.h"
#include "Peds/ped.h"
#include "Peds/PedIntelligence.h"
#include "Peds/VehicleCombatAvoidanceArea.h"
#include "scene/world/GameWorld.h"
#include "scene/world/GameWorldHeightMap.h"
#include "vehicleAi/pathfind.h"
#include "Vehicles/Automobile.h"
#include "Vehicles/Boat.h"
#include "Vehicles/vehicle.h"
#include "Vehicles/vehiclepopulation.h"

//Parser headers
#include "game/Dispatch/DispatchHelpers_parser.h"

AI_OPTIMISATIONS()

#define IMPLEMENT_DISPATCH_HELPERS_TUNABLES(classname, hash)	IMPLEMENT_TUNABLES_PSO(classname, #classname, hash, "Dispatch Tuning", "Game Logic")

////////////////////////////////////////////////////////////////////////////////
// CDispatchSpawnProperties
////////////////////////////////////////////////////////////////////////////////

bool CDispatchSpawnProperties::sm_bRenderDebug = false;

CDispatchSpawnProperties::CDispatchSpawnProperties()
: m_aBlockingAreas()
, m_vLocation(V_ZERO)
, m_fIdealSpawnDistance(-1.0f)
{
}

CDispatchSpawnProperties::~CDispatchSpawnProperties()
{
}

CDispatchSpawnProperties& CDispatchSpawnProperties::GetInstance()
{
	static CDispatchSpawnProperties sInst;
	return sInst;
}

int CDispatchSpawnProperties::AddAngledBlockingArea(Vec3V_In vStart, Vec3V_In vEnd, const float fWidth)
{
	//Find an empty slot for the blocking area.
	for(int i = 0; i < m_aBlockingAreas.GetMaxCount(); ++i)
	{
		//Ensure the area is inactive.
		CArea& rArea = m_aBlockingAreas[i];
		if(rArea.IsActive())
		{
			continue;
		}

		//Add the blocking area.
		rArea.Set(VEC3V_TO_VECTOR3(vStart), VEC3V_TO_VECTOR3(vEnd), fWidth);

		//Return the index.
		return i;
	}

	//Note that the area could not be added.
	taskAssertf(false, "Could not add blocking area.  Storage is full, maximum is: %d.", m_aBlockingAreas.GetMaxCount());
	return -1;
}

int CDispatchSpawnProperties::AddSphereBlockingArea(Vec3V_In vCenter, const float fRadius)
{
	//Find an empty slot for the blocking area.
	for(int i = 0; i < m_aBlockingAreas.GetMaxCount(); ++i)
	{
		//Ensure the area is inactive.
		CArea& rArea = m_aBlockingAreas[i];
		if(rArea.IsActive())
		{
			continue;
		}

		//Add the blocking area.
		rArea.SetAsSphere(VEC3V_TO_VECTOR3(vCenter), fRadius);

		//Return the index.
		return i;
	}

	//Note that the area could not be added.
	taskAssertf(false, "Could not add blocking area.  Storage is full, maximum is: %d.", m_aBlockingAreas.GetMaxCount());
	return -1;
}

bool CDispatchSpawnProperties::IsPositionInBlockingArea(Vec3V_In vPosition) const
{
	//Check if the position is in any of the blocking areas.
	for(int i = 0; i < m_aBlockingAreas.GetMaxCount(); ++i)
	{
		//Ensure the area is active.
		const CArea& rArea = m_aBlockingAreas[i];
		if(!rArea.IsActive())
		{
			continue;
		}

		//Check if the position is in the blocking area.
		if(rArea.IsPointInArea(VEC3V_TO_VECTOR3(vPosition)))
		{
			return true;
		}
	}

	return false;
}

void CDispatchSpawnProperties::RemoveBlockingArea(int iIndex)
{
	//Ensure the index is valid.
	if(!taskVerifyf(iIndex >= 0 && iIndex < m_aBlockingAreas.GetMaxCount(), "Index is invalid: %d.", iIndex))
	{
		return;
	}

	//Remove the blocking area.
	m_aBlockingAreas[iIndex].Reset();
}

void CDispatchSpawnProperties::Reset()
{
	//Reset the location.
	ResetLocation();

	//Reset the ideal spawn distance.
	ResetIdealSpawnDistance();

	//Reset the blocking areas.
	ResetBlockingAreas();
}

void CDispatchSpawnProperties::ResetBlockingAreas()
{
	//Reset the blocking areas.
	for(int i = 0; i < m_aBlockingAreas.GetMaxCount(); ++i)
	{
		m_aBlockingAreas[i].Reset();
	}
}

#if __BANK
void CDispatchSpawnProperties::RenderDebug()
{
	//Ensure debug rendering is enabled.
	if(!sm_bRenderDebug)
	{
		return;
	}

	//Render the location.
	if(HasLocation())
	{
		//Grab the location.
		Vector3 vLocation = VEC3V_TO_VECTOR3(GetLocation());

		//Render the location.
		grcDebugDraw::Sphere(vLocation, 1.0f, Color_red);

		//Render the ideal spawn distance.
		if(HasIdealSpawnDistance())
		{
			//Render the ideal spawn distance.
			grcDebugDraw::Sphere(vLocation, GetIdealSpawnDistance(), Color_blue, false);
		}
	}

	//Render the blocking areas.
	for(int i = 0; i < m_aBlockingAreas.GetMaxCount(); ++i)
	{
		//Ensure the blocking area is active.
		const CArea& rArea = m_aBlockingAreas[i];
		if(!rArea.IsActive())
		{
			continue;
		}

		//Render the blocking area.
		rArea.IsPointInArea(VEC3_ZERO, true);
	}
}
#endif

////////////////////////////////////////////////////////////////////////////////
// CDispatchSpawnHelper
////////////////////////////////////////////////////////////////////////////////

CDispatchSpawnHelper::Tunables CDispatchSpawnHelper::sm_Tunables;
IMPLEMENT_DISPATCH_HELPERS_TUNABLES(CDispatchSpawnHelper, 0x0e4630f9);

CDispatchSpawnHelper::CDispatchSpawnHelper()
: m_FindNearestCarNodeHelper()
, m_DispatchNode()
, m_pIncident(NULL)
, m_fTimeSinceSearchPositionForFindNearestCarNodeHelperWasUpdated(LARGE_FLOAT)
, m_uRunningFlags(0)
{
}

CDispatchSpawnHelper::~CDispatchSpawnHelper()
{
}

bool CDispatchSpawnHelper::CanGenerateRandomDispatchPosition() const
{
	//Ensure the dispatch node is not empty.
	if(m_DispatchNode.IsEmpty())
	{
		return false;
	}

	return true;
}

bool CDispatchSpawnHelper::CanSpawnAtPosition(Vec3V_In vPosition) const
{
	//Ensure the position is not inside a blocking area.
	if(CDispatchSpawnProperties::GetInstance().IsPositionInBlockingArea(vPosition))
	{
		return false;
	}

	//Ensure the position is not inside a blocking area.
	if(CDispatchHelperVolumes::GetTunables().m_BlockingAreas.Contains(vPosition))
	{
		return false;
	}

	//Ensure the position is not inside a vehicle combat avoidance area.
	if(CVehicleCombatAvoidanceArea::IsPointInAreas(vPosition))
	{
		return false;
	}
	
	//Ensure the position is (relatively) off-screen.
	{
		//Grab the camera position.
		Vec3V vCameraPosition = VECTOR3_TO_VEC3V(camInterface::GetPos());
		
		//Check if the position is close enough to require viewport checks.
		float fMaxDistanceFromCameraForViewportChecks = sm_Tunables.m_Restrictions.m_MaxDistanceFromCameraForViewportChecks;
		ScalarV scDistSq = DistSquared(vPosition, vCameraPosition);
		ScalarV scMaxDistSq = ScalarVFromF32(square(fMaxDistanceFromCameraForViewportChecks));
		
		//Ensure the spawn position is not visible in the viewport.
		float fRadiusForViewportCheck = sm_Tunables.m_Restrictions.m_RadiusForViewportCheck;

		if(IsLessThanAll(scDistSq, scMaxDistSq))
		{	
			if(camInterface::IsSphereVisibleInGameViewport(VEC3V_TO_VECTOR3(vPosition), fRadiusForViewportCheck))
			{
				return false;
			}
		}

		//Check if a network game is in progress.
		if(NetworkInterface::IsGameInProgress())
		{
			//Ensure the spawn position is not visible to any remote player.
			if(NetworkInterface::IsVisibleToAnyRemotePlayer(VEC3V_TO_VECTOR3(vPosition), fRadiusForViewportCheck, fMaxDistanceFromCameraForViewportChecks))
			{
				return false;
			}
		}
	}

	return true;
}

bool CDispatchSpawnHelper::CanSpawnBoat() const
{
	return true;
}

bool CDispatchSpawnHelper::CanSpawnVehicle() const
{
	return CanChooseBestSpawnPoint();
}

bool CDispatchSpawnHelper::CanSpawnVehicleInFront() const
{
	//Ensure the dispatch node is valid.
	if(m_DispatchNode.IsEmpty())
	{
		return false;
	}
	
	return true;
}

bool CDispatchSpawnHelper::GenerateRandomDispatchPosition(Vec3V_InOut vPosition) const
{
	//Assert that we can generate a random dispatch position.
	aiAssert(CanGenerateRandomDispatchPosition());

	//Create the input.
	CFindNearestCarNodeHelper::GenerateRandomPositionNearRoadNodeInput input(m_DispatchNode);
	input.m_fMaxDistance = sm_Tunables.m_MaxDistanceForDispatchPosition;

	//Generate a random position near the dispatch node.
	if(!CFindNearestCarNodeHelper::GenerateRandomPositionNearRoadNode(input, vPosition))
	{
		return false;
	}

	return true;
}

const CEntity* CDispatchSpawnHelper::GetEntity() const
{
	//Ensure the incident is valid.
	const CIncident* pIncident = m_pIncident;
	if(!pIncident)
	{
		return NULL;
	}

	return pIncident->GetEntity();
}

float CDispatchSpawnHelper::GetIdealSpawnDistance() const
{
	//Check if the incident is scripted.
	const CIncident* pIncident = GetIncident();
	if(pIncident && (pIncident->GetType() == CIncident::IT_Scripted))
	{
		//Check if the incident has an ideal spawn distance.
		const CScriptIncident* pScriptIncident = static_cast<const CScriptIncident *>(pIncident);
		if(pScriptIncident->HasIdealSpawnDistance())
		{
			return pScriptIncident->GetIdealSpawnDistance();
		}
	}

	//Check if the spawn properties has an ideal spawn distance.
	if(CDispatchSpawnProperties::GetInstance().HasIdealSpawnDistance())
	{
		return CDispatchSpawnProperties::GetInstance().GetIdealSpawnDistance();
	}

	//Check if the wanted is valid.
	const CWanted* pWanted = GetWanted();
	if(pWanted)
	{
		//Calculate the ideal spawn distance.
		return pWanted->CalculateIdealSpawnDistance();
	}

	//Use the default value.
	float fIdealSpawnDistance = sm_Tunables.m_IdealSpawnDistance;
	return fIdealSpawnDistance;
}

const CPed* CDispatchSpawnHelper::GetPed() const
{
	//Ensure the entity is valid.
	const CEntity* pEntity = GetEntity();
	if(!pEntity)
	{
		return NULL;
	}

	//Ensure the entity is a ped.
	if(!pEntity->GetIsTypePed())
	{
		return NULL;
	}

	return static_cast<const CPed *>(pEntity);
}

bool CDispatchSpawnHelper::IsClosestRoadNodeWithinDistance(float fMaxDistance) const
{
	//Ensure the node is valid.
	const CPathNode* pNode = ThePaths.FindNodePointerSafe(m_DispatchNode);
	if(!pNode)
	{
		return false;
	}

	//Ensure the ped is valid.
	const CPed* pPed = GetPed();
	if(!pPed)
	{
		return false;
	}

	//Grab the node position.
	Vec3V vNodePosition;
	pNode->GetCoors(RC_VECTOR3(vNodePosition));

	//Ensure the distance is within the threshold.
	float fDistanceSq = DistSquared(pPed->GetTransform().GetPosition(), vNodePosition).Getf();
	return (fDistanceSq <= square(fMaxDistance));
}

CBoat* CDispatchSpawnHelper::SpawnBoat(fwModelId iModelId)
{
	//Assert that we can spawn a boat.
	aiAssertf(CanSpawnBoat(), "Unable to spawn boat.");
	
	//Calculate the offset.
	float fAngle = fwRandom::GetRandomNumberInRange(0.0f, TWO_PI);
	Vector3 vDirection(cos(fAngle), sin(fAngle), 0.0f);
	Vector3 vOffset = vDirection * GetIdealSpawnDistance();
	
	//Calculate the position.
	Vector3 vLocation = VEC3V_TO_VECTOR3(GetLocation());
	Vector3 vPosition = vLocation + vOffset;
	
	//Calculate the water level at the position.
	float fWaterZ;
	if(!Water::GetWaterLevelNoWaves(vPosition, &fWaterZ, POOL_DEPTH, REJECTIONABOVEWATER, NULL))
	{
		return NULL;
	}
	
	//Ensure the base model info is valid.
	CBaseModelInfo* pBaseModelInfo = CModelInfo::GetBaseModelInfo(iModelId);
	if(!aiVerifyf(pBaseModelInfo, "The base model info is invalid: %d.", iModelId.GetModelIndex()))
	{
		return NULL;
	}

	//Ensure the base model info is for a vehicle.
	if(!aiVerifyf(pBaseModelInfo->GetModelType() == MI_TYPE_VEHICLE, "The model type is invalid: %d.", pBaseModelInfo->GetModelType()))
	{
		return NULL;
	}

	//Grab the vehicle model info.
	CVehicleModelInfo* pVehicleModelInfo = static_cast<CVehicleModelInfo *>(pBaseModelInfo);
	
	//Ensure the vehicle model info is a boat.
	if(!aiVerifyf(pVehicleModelInfo->GetIsBoat(), "The vehicle model info is not a boat: %d.", iModelId.GetModelIndex()))
	{
		return NULL;
	}

	//Find the min/max XY values.
	const Vector3& vBoundingBoxMin = pVehicleModelInfo->GetBoundingBoxMin();
	const Vector3& vBoundingBoxMax = pVehicleModelInfo->GetBoundingBoxMax();
	float fMinX = vPosition.x + vBoundingBoxMin.x;
	float fMinY = vPosition.y + vBoundingBoxMin.y;
	float fMaxX = vPosition.x + vBoundingBoxMax.x;
	float fMaxY = vPosition.y + vBoundingBoxMax.y;

	//Get the maximum height for the area.
	float fMaxHeight = CGameWorldHeightMap::GetMaxHeightFromWorldHeightMap(fMinX, fMinY, fMaxX, fMaxY);

	//Ensure the water level is essentially the max height.
	//This was added to ensure we don't spawn boats on water with stuff above them.
	float fDifference = Abs(fWaterZ - fMaxHeight);
	static float s_fThreshold = 0.25f;
	if(fDifference > s_fThreshold)
	{
		return NULL;
	}
	
	//Assign the water level.
	vPosition.z = fWaterZ;
	
	//Calculate the forward vector.
	Vector3 vForward = (vLocation - vPosition);
	vForward.NormalizeSafe(YAXIS);
	
	//Calculate the up vector.
	Vector3 vUp = ZAXIS;
	
	//Calculate the side vector.
	Vector3 vSide;
	vSide.Cross(vForward, vUp);
	vSide.NormalizeSafe(XAXIS);
	
	//Re-calculate the up vector.
	vUp.Cross(vSide, vForward);
	vUp.NormalizeSafe(ZAXIS);
	
	//Calculate the matrix.
	Matrix34 mBoat;
	mBoat.a = vSide;
	mBoat.b = vForward;
	mBoat.c = vUp;
	mBoat.d = vPosition;
	
	//Generate a boat.
	CVehiclePopulation::GenerateOneEmergencyServicesVehicleWithMatrixInput input(mBoat, iModelId);
	CVehicle* pVehicle = CVehiclePopulation::GenerateOneEmergencyServicesVehicleWithMatrix(input);
	if(!pVehicle)
	{
		return NULL;
	}

	//Note that a vehicle was spawned.
	OnVehicleSpawned(*pVehicle);
	
	//Assert that we spawned a boat.
	aiAssertf(pVehicle->InheritsFromBoat(), "The vehicle (type %d) is not a boat.", pVehicle->GetVehicleType());
	
	//Grab the boat.
	CBoat* pBoat = static_cast<CBoat *>(pVehicle);

	return pBoat;
}

CVehicle* CDispatchSpawnHelper::SpawnVehicle(fwModelId iModelId, bool bDisableSwitchedOffNodes)
{
	//Assert that we can spawn a vehicle.
	aiAssertf(CanSpawnVehicle(), "Unable to spawn vehicle.");
	
	//Try to spawn a vehicle.
	for(const SpawnPoint* pSpawnPoint = ChooseBestSpawnPoint(); pSpawnPoint; pSpawnPoint = ChooseNextBestSpawnPoint())
	{
		//Ensure the node is valid.
		const CPathNode* pNode = ThePaths.FindNodePointerSafe(pSpawnPoint->m_Node);
		if(!pNode)
		{
			continue;
		}
		
		//Grab the node position.
		Vector3 vNodePosition;
		pNode->GetCoors(vNodePosition);
		
		//Ensure we can spawn at the position.
		if(!CanSpawnAtPosition(VECTOR3_TO_VEC3V(vNodePosition)))
		{
			continue;
		}

		if(bDisableSwitchedOffNodes && pNode->IsSwitchedOff())
		{
			continue;
		}

		aiAssertf(vNodePosition.z > -200.0f, "CDispatchSpawnHelper::SpawnVehicle, Generated bad spawn loacation");
		
		//Generate the input.
		CVehiclePopulation::GenerateOneEmergencyServicesCarWithRoadNodesInput input(pSpawnPoint->m_Node, pSpawnPoint->m_PreviousNode, iModelId);

		//If the dispatch vehicle cannot be placed on the road properly, it should not result in failure.
		//This is because we sometimes spawn dispatch vehicles where map collision has not been loaded in.
		//In these cases, the car cannot be placed on the road properly since the probes will not be able to
		//detect where the road is.  These cars will end up being spawned slightly inside of the road, but
		//they will be dummied, and when they reach collision, they should right themselves.
		input.m_bPlaceOnRoadProperlyCausesFailure = false;

		//Use bounding boxes instead of bounding spheres when checking for collisions.
		//We used to use bounding spheres, but this became a problem when trying to spawn on single-lane
		//roads with fire hydrants and other obstructions on the side of the road.
		input.m_bUseBoundingBoxesForCollision = true;

		//Try to clear objects out of the way.
		//No reason to block dispatch vehicles from spawning if there is ambient stuff there.
		input.m_bTryToClearAreaIfBlocked = true;

		//Generate a vehicle with the road nodes.
		CVehicle* pVehicle = CVehiclePopulation::GenerateOneEmergencyServicesCarWithRoadNodes(input);
		if(!pVehicle)
		{
			continue;
		}

		//Note that a vehicle was spawned.
		OnVehicleSpawned(*pVehicle);

		return pVehicle;
	}

	return NULL;
}

CVehicle* CDispatchSpawnHelper::SpawnVehicleInFront(const fwModelId iModelId, const bool bPulledOver)
{
	//Assert that we can spawn a vehicle in front.
	aiAssertf(CanSpawnVehicleInFront(), "Unable to spawn vehicle in front.");
	
	//Ensure the dispatch node is valid.
	const CPathNode* pDispatchNode = ThePaths.FindNodePointerSafe(m_DispatchNode);
	if(!pDispatchNode)
	{
		return NULL;
	}
	
	//Get the direction.
	Vector3 vDirection = VEC3V_TO_VECTOR3(GetDirection());

	//Generate the input arguments for the road node search.
	CPathFind::FindNodeInDirectionInput roadNodeInput(m_DispatchNode, vDirection, GetIdealSpawnDistance());

	//Follow the road direction.
	roadNodeInput.m_bFollowRoad = true;

	//If the car is going to be pulled over, it looks best if the road is straight.
	if(bPulledOver)
	{
		roadNodeInput.m_uMaxLinks = 2;
	}

	//Find a road node in the road direction.
	CPathFind::FindNodeInDirectionOutput roadNodeOutput;
	if(!ThePaths.FindNodeInDirection(roadNodeInput, roadNodeOutput))
	{
		return NULL;
	}

	//Ensure the node is valid.
	const CPathNode* pNode = ThePaths.FindNodePointerSafe(roadNodeOutput.m_Node);
	if(!pNode)
	{
		return NULL;
	}
	
	//Grab the dispatch node position.
	Vector3 vPosition;
	pDispatchNode->GetCoors(vPosition);

	//Grab the node position.
	Vector3 vNodePosition;
	pNode->GetCoors(vNodePosition);

	//Generate a vector from the position to the node.
	Vector3 vPositionToNode = vNodePosition - vPosition;

	//Generate the direction from the player to the node.
	Vector3 vPositionToNodeDirection = vPositionToNode;
	if(!vPositionToNodeDirection.NormalizeSafeRet())
	{
		return NULL;
	}

	//Ensure the node is generally in the direction.
	float fDot = vDirection.Dot(vPositionToNodeDirection);
	float fMinDot = sm_Tunables.m_MinDotForInFront;
	if(fDot < fMinDot)
	{
		return NULL;
	}

	//Ensure we can spawn at the position.
	if(!CanSpawnAtPosition(VECTOR3_TO_VEC3V(vNodePosition)))
	{
		return NULL;
	}

	//Generate a vehicle with the road nodes.
	CVehiclePopulation::GenerateOneEmergencyServicesCarWithRoadNodesInput input(roadNodeOutput.m_Node, roadNodeOutput.m_PreviousNode, iModelId);
	input.m_bUseBoundingBoxesForCollision = true;
	input.m_bTryToClearAreaIfBlocked = true;
	input.m_bPulledOver = bPulledOver;
	input.m_bFacePreviousNode = false;
	input.m_bKickStartVelocity = !bPulledOver;
	input.m_bPlaceOnRoadProperlyCausesFailure = false;
	CVehicle* pVehicle = CVehiclePopulation::GenerateOneEmergencyServicesCarWithRoadNodes(input);
	if(!pVehicle)
	{
		return NULL;
	}
	
	//Note that a vehicle was spawned.
	OnVehicleSpawned(*pVehicle);

	return pVehicle;
}

bool CDispatchSpawnHelper::FindSpawnPointInDirection(Vec3V_In vPosition, Vec3V_In vDirection, float fIdealSpawnDistance, fwFlags32 uFlags, Vec3V_InOut vSpawnPoint)
{
	//Ensure the closest road node is valid.
	CNodeAddress nodeAddress = ThePaths.FindNodeClosestToCoors(VEC3V_TO_VECTOR3(vPosition));
	if(nodeAddress.IsEmpty())
	{
		return false;
	}

	//Generate the direction.
	Vector2 vDirectionFlat(VEC3V_TO_VECTOR3(vDirection), Vector2::kXY);
	vDirectionFlat.NormalizeSafe(Vector2(0.0f, 1.0f));

	//Ensure the spawn point is valid.
	SpawnPoint spawnPoint;
	if(!FindSpawnPointInDirection(nodeAddress, vDirectionFlat, fIdealSpawnDistance, uFlags, spawnPoint))
	{
		return false;
	}

	//Ensure the node is valid.
	const CPathNode* pPathNode = ThePaths.FindNodePointerSafe(spawnPoint.m_Node);
	if(!pPathNode)
	{
		return false;
	}

	//Get the position.
	pPathNode->GetCoors(RC_VECTOR3(vSpawnPoint));

	return true;
}

bool CDispatchSpawnHelper::FindSpawnPointInDirection(const CNodeAddress& rNodeAddress, const Vector2& vDirection, float fIdealSpawnDistance, fwFlags32 uFlags, SpawnPoint& rSpawnPoint)
{
	aiAssert(!rNodeAddress.IsEmpty());

	//Generate the input arguments for the road node search.
	CPathFind::FindNodeInDirectionInput roadNodeInput(rNodeAddress, Vector3(vDirection, Vector2::kXY), fIdealSpawnDistance);

	//Set the maximum distance traveled based on the ideal spawn distance.
	float fMaxDistanceTraveledMultiplier = sm_Tunables.m_MaxDistanceTraveledMultiplier;
	float fMaxDistanceTraveled = fIdealSpawnDistance * fMaxDistanceTraveledMultiplier;
	roadNodeInput.m_fMaxDistanceTraveled = fMaxDistanceTraveled;

	//Add some variance to the scoring.
	static float s_fMaxRandomVariance = 1.1f;
	roadNodeInput.m_fMaxRandomVariance = s_fMaxRandomVariance;

	//Find the closest road node to the position.
	const CPathNode* pNode = ThePaths.FindNodePointerSafe(rNodeAddress);
	if(pNode)
	{
		//Check if the node is switched off.
		if(pNode->m_2.m_switchedOff)
		{
			//If the ped is closest to a switched off node, include switched off nodes
			//in the search.  This was added to allow emergency service responses to remote locations
			//surrounded by switched off nodes.  A good example of this is the prison.

			//Include switched off nodes.
			roadNodeInput.m_bIncludeSwitchedOffNodes = true;

			//Do not include switched off nodes after a switched on has been encountered.
			//This allows the flood-fill to get itself out of an alley-way, but it will not continue to
			//fill alley-ways once it reaches a main road.
			roadNodeInput.m_bDoNotIncludeSwitchedOffNodesAfterSwitchedOnEncountered = true;

			//Use the original switched off value for the node, as opposed to the current.
			//Missions may switch off road nodes for traffic purposes, and we still
			//want to consider these off nodes as 'on'.
			roadNodeInput.m_bUseOriginalSwitchedOffValue = true;
		}

		//Check if the node is a dead end.
		if(pNode->m_2.m_deadEndness)
		{
			//Include dead end nodes.
			roadNodeInput.m_bIncludeDeadEndNodes = true;

			//Do not include dead end nodes after a non-dead end has been encountered.
			roadNodeInput.m_bDoNotIncludeDeadEndNodesAfterNonDeadEndEncountered = true;

			//Allow following of outgoing links from dead end nodes.
			roadNodeInput.m_bCanFollowOutgoingLinksFromDeadEndNodes = true;
		}
	}

	//Ensure there is a route from the spawn point to the target.
	roadNodeInput.m_bCanFollowOutgoingLinks = (uFlags.IsFlagSet(FSPIDF_CanFollowOutgoingLinks));
	roadNodeInput.m_bCanFollowIncomingLinks = true;

#if __BANK
	//Check if we should render the search.
	roadNodeInput.m_bRender = (sm_Tunables.m_Rendering.m_Enabled && sm_Tunables.m_Rendering.m_FindSpawnPointInDirection);
#endif

	//Find a road node in the road direction.
	CPathFind::FindNodeInDirectionOutput roadNodeOutput;
	if(!ThePaths.FindNodeInDirection(roadNodeInput, roadNodeOutput))
	{
		return false;
	}

	//Ensure the node is valid.
	pNode = ThePaths.FindNodePointerSafe(roadNodeOutput.m_Node);
	if(!pNode)
	{
		return false;
	}

	//Grab the position.
	Vector3 vPosition;
	pNode->GetCoors(vPosition);

	//Assign the spawn point.
	rSpawnPoint.m_Node			= roadNodeOutput.m_Node;
	rSpawnPoint.m_PreviousNode	= roadNodeOutput.m_PreviousNode;

	return true;
}

bool CDispatchSpawnHelper::FailedToFindSpawnPoint() const
{
	//Check if we failed to find a dispatch node.
	if(FailedToFindDispatchNode())
	{
		return true;
	}

	return false;
}

#if __BANK
void CDispatchSpawnHelper::RenderDebug()
{
	//Ensure rendering is enabled.
	if(!sm_Tunables.m_Rendering.m_Enabled)
	{
		return;
	}

	//Check if rendering for the dispatch node is enabled.
	if(sm_Tunables.m_Rendering.m_DispatchNode)
	{
		//Check if the node is valid.
		const CPathNode* pNode = ThePaths.FindNodePointerSafe(m_DispatchNode);
		if(pNode)
		{
			//Grab the position.
			Vector3 vPosition;
			pNode->GetCoors(vPosition);

			//Draw a sphere.
			grcDebugDraw::Sphere(vPosition, 0.5f, Color_green);
		}
	}

	//Check if rendering for the incident location is enabled.
	if(sm_Tunables.m_Rendering.m_IncidentLocation)
	{
		//Check if the incident is valid.
		const CIncident* pIncident = GetIncident();
		if(pIncident)
		{
			//Draw a sphere.
			grcDebugDraw::Sphere(pIncident->GetLocation(), 0.5f, Color_red);
		}
	}
}
#endif

bool CDispatchSpawnHelper::CanFindSpawnPointInDirection() const
{
	//Ensure the dispatch node is valid.
	if(m_DispatchNode.IsEmpty())
	{
		return false;
	}
	
	return true;
}

bool CDispatchSpawnHelper::FindSpawnPointInDirection(const Vector2& vDirection, SpawnPoint& rSpawnPoint) const
{
	//Assert that we can find a spawn point in the direction.
	aiAssertf(CanFindSpawnPointInDirection(), "Unable to find spawn point in direction.");

	return (FindSpawnPointInDirection(m_DispatchNode, vDirection, GetIdealSpawnDistance(), 0, rSpawnPoint));
}

Vec3V_Out CDispatchSpawnHelper::GetDirection() const
{
	//Ensure the entity is valid.
	const CEntity* pEntity = GetEntity();
	if(!pEntity)
	{
		return Vec3V(V_ZERO);
	}

	//Determine which entity to use when calculating the direction.
	const CEntity* pEntityForDirection = pEntity;

	//Check if the ped is valid.
	const CPed* pPed = GetPed();
	if(pPed)
	{
		//Check if the ped is in a vehicle.
		const CVehicle* pVehicle = pPed->GetVehiclePedInside();
		if(pVehicle)
		{
			pEntityForDirection = pVehicle;
		}
	}

	//Grab the entity forward.
	Vec3V vForward = pEntityForDirection->GetTransform().GetForward();

	//Check if the entity is physical.
	if(pEntityForDirection->GetIsPhysical())
	{
		//Grab the physical.
		const CPhysical* pPhysical = static_cast<const CPhysical *>(pEntityForDirection);

		//Grab the velocity.
		Vec3V vVelocity = VECTOR3_TO_VEC3V(pPhysical->GetVelocity());

		//Calculate the direction.
		Vec3V vDirection = NormalizeFastSafe(vVelocity, vForward);

		return vDirection;
	}
	else
	{
		//Use the forward vector as the direction.
		return vForward;
	}
}

Vec3V_Out CDispatchSpawnHelper::GetLocation() const
{
	//Check if the spawn properties has a location.
	if(CDispatchSpawnProperties::GetInstance().HasLocation())
	{
		return CDispatchSpawnProperties::GetInstance().GetLocation();
	}

	//Ensure the incident is valid.
	const CIncident* pIncident = m_pIncident;
	if(!aiVerifyf(pIncident, "The incident is invalid."))
	{
		return Vec3V(V_ZERO);
	}

	return VECTOR3_TO_VEC3V(pIncident->GetLocation());
}

Vec3V_Out CDispatchSpawnHelper::GetLocationForNearestCarNode() const
{
	//Grab the location.
	Vec3V vLocation = GetLocation();

	//Transform the location.
	vLocation = CDispatchHelperVolumes::GetTunables().m_LocationForNearestCarNodeOverrides.Transform(vLocation);

	return vLocation;
}

CVehicle* CDispatchSpawnHelper::GetVehiclePedEscapingIn() const
{
	//Ensure the ped is valid.
	const CPed* pPed = GetPed();
	if(!pPed)
	{
		return NULL;
	}
	
	//Ensure the ped is in a vehicle.
	CVehicle* pVehicle = pPed->GetVehiclePedInside();
	if(!pVehicle)
	{
		return NULL;
	}

	//Grab the velocity.
	Vec3V vVelocity = RCC_VEC3V(pVehicle->GetVelocity());

	//Ensure the vehicle is moving at a significant rate.
	float fMinSpeedToBeConsideredEscapingInVehicle = sm_Tunables.m_MinSpeedToBeConsideredEscapingInVehicle;
	ScalarV scVelocitySq = MagSquared(vVelocity);
	ScalarV scMinVelocitySq = ScalarVFromF32(square(fMinSpeedToBeConsideredEscapingInVehicle));
	if(IsLessThanAll(scVelocitySq, scMinVelocitySq))
	{
		return NULL;
	}

	return pVehicle;
}

const CWanted* CDispatchSpawnHelper::GetWanted() const
{
	//Ensure the ped is valid.
	const CPed* pPed = GetPed();
	if(!pPed)
	{
		return NULL;
	}

	return pPed->GetPlayerWanted();
}

bool CDispatchSpawnHelper::IsPedEscapingInVehicle() const
{
	return (GetVehiclePedEscapingIn() != NULL);
}

void CDispatchSpawnHelper::OnActivate()
{
	//Set the flag.
	m_uRunningFlags.SetFlag(RF_IsActive);
}

void CDispatchSpawnHelper::OnDeactivate()
{
	//Clear the flag.
	m_uRunningFlags.ClearFlag(RF_IsActive);

	//Reset the find nearest car node helper.
	m_FindNearestCarNodeHelper.Reset();

	//Clear the dispatch node.
	m_DispatchNode.SetEmpty();

	//Clear the incident.
	m_pIncident = NULL;

	//Clear the timers.
	m_fTimeSinceSearchPositionForFindNearestCarNodeHelperWasUpdated = LARGE_FLOAT;
}

void CDispatchSpawnHelper::OnDispatchNodeChanged()
{

}

void CDispatchSpawnHelper::OnVehicleSpawned(CVehicle& rVehicle)
{
	//Note that dispatch vehicles can't traffic jam.
	rVehicle.m_nVehicleFlags.bCantTrafficJam = true;
}

void CDispatchSpawnHelper::Update(float fTimeStep)
{
	//Assert that we are active.
	aiAssertf(IsActive(), "The spawn helper is inactive.");

	//Assert that the incident is valid.
	aiAssertf(m_pIncident, "The incident is invalid.");

	//Update the timers.
	UpdateTimers(fTimeStep);

	//Update the find nearest car node helper.
	UpdateFindNearestCarNodeHelper();
}

void CDispatchSpawnHelper::Activate(const CIncident* pIncident)
{
	//Ensure the incident is valid.
	if(!aiVerifyf(pIncident, "The incident is invalid."))
	{
		return;
	}

	//Assert that the current incident is invalid.
	aiAssertf(!m_pIncident, "The current incident is valid.");

	//Set the incident.
	m_pIncident = pIncident;

	//Note that we have activated.
	OnActivate();
}

void CDispatchSpawnHelper::Deactivate()
{
	//Assert that the incident is invalid.
	aiAssertf(!m_pIncident, "The incident is valid.");

	//Note that we have deactivated.
	OnDeactivate();
}

bool CDispatchSpawnHelper::ShouldUpdateSearchPositionForFindNearestCarNodeHelper() const
{
	//ensure we're still close to incident (could have been teleported)
	const CPathNode* pNode = ThePaths.FindNodePointerSafe(m_DispatchNode);
	if(pNode)
	{
		Vector3 vPosition;
		pNode->GetCoors(vPosition);

		static float s_fMaxDistanceForUpdate = 40000.0f; //200 meters
		if(vPosition.Dist2(VEC3V_TO_VECTOR3(GetLocation())) > s_fMaxDistanceForUpdate)
		{
			return true;
		}
	}

	//Ensure the time has surpassed the threshold.
	static float s_fMaxTimeBetweenUpdates = 2.0f;
	float fTimeSinceLastUpdate = m_fTimeSinceSearchPositionForFindNearestCarNodeHelperWasUpdated;
	if(fTimeSinceLastUpdate < s_fMaxTimeBetweenUpdates)
	{
		return false;
	}

	return true;
}

void CDispatchSpawnHelper::UpdateFindNearestCarNodeHelper()
{
	//Check if we should update the search position for the find nearest car node helper.
	if(ShouldUpdateSearchPositionForFindNearestCarNodeHelper())
	{
		//Update the search position for the find nearest car node helper.
		UpdateSearchPositionForFindNearestCarNodeHelper();
	}

	//Update the find nearest car node helper.
	m_FindNearestCarNodeHelper.Update();

	//Update the dispatch node.
	CNodeAddress nearestCarNode;
	if(m_FindNearestCarNodeHelper.GetNearestCarNode(nearestCarNode))
	{
		//Check if the dispatch node is changing.
		if(nearestCarNode != m_DispatchNode)
		{
			//Set the dispatch node.
			m_DispatchNode = nearestCarNode;

			//Note that the dispatch node changed.
			OnDispatchNodeChanged();
		}

		//Change the flag.
		m_uRunningFlags.ChangeFlag(RF_FailedToFindDispatchNode, m_DispatchNode.IsEmpty());
	}
}

void CDispatchSpawnHelper::UpdateSearchPositionForFindNearestCarNodeHelper()
{
	//Grab the location.
	Vec3V vLocation = GetLocationForNearestCarNode();

	//Set the search position.
	m_FindNearestCarNodeHelper.SetSearchPosition(VEC3V_TO_VECTOR3(vLocation));

	//Clear the timer.
	m_fTimeSinceSearchPositionForFindNearestCarNodeHelperWasUpdated = 0.0f;
}

void CDispatchSpawnHelper::UpdateTimers(float fTimeStep)
{
	//Update the timers.
	m_fTimeSinceSearchPositionForFindNearestCarNodeHelperWasUpdated += fTimeStep;
}

////////////////////////////////////////////////////////////////////////////////
// CDispatchBasicSpawnHelper
////////////////////////////////////////////////////////////////////////////////

CDispatchBasicSpawnHelper::CDispatchBasicSpawnHelper()
: CDispatchSpawnHelper()
, m_SpawnPoint()
{
}

CDispatchBasicSpawnHelper::~CDispatchBasicSpawnHelper()
{
}

bool CDispatchBasicSpawnHelper::CanChooseBestSpawnPoint() const
{
	return CanFindSpawnPointInDirection();
}

const CDispatchSpawnHelper::SpawnPoint* CDispatchBasicSpawnHelper::ChooseBestSpawnPoint()
{
	//Assert that we can choose the best spawn point.
	aiAssertf(CanChooseBestSpawnPoint(), "Unable to choose best spawn point.");
	
	//Generate a random angle.
	float fAngle = fwRandom::GetRandomNumberInRange(-PI, PI);
	
	//Calculate the direction.
	Vector2 vDirection(cos(fAngle), sin(fAngle));
	
	//Find a spawn point in the direction.
	m_SpawnPoint.Reset();
	if(!FindSpawnPointInDirection(vDirection, m_SpawnPoint))
	{
		return NULL;
	}
	
	return &m_SpawnPoint;
}

const CDispatchSpawnHelper::SpawnPoint* CDispatchBasicSpawnHelper::ChooseNextBestSpawnPoint()
{
	return NULL;
}

void CDispatchBasicSpawnHelper::OnDeactivate()
{
	//Call the base class version.
	CDispatchSpawnHelper::OnDeactivate();

	//Reset the spawn point.
	m_SpawnPoint.Reset();
}

////////////////////////////////////////////////////////////////////////////////
// CDispatchAdvancedSpawnHelper
////////////////////////////////////////////////////////////////////////////////

CDispatchAdvancedSpawnHelper::Tunables CDispatchAdvancedSpawnHelper::sm_Tunables;
IMPLEMENT_DISPATCH_HELPERS_TUNABLES(CDispatchAdvancedSpawnHelper, 0x5184478f);

#if __BANK
CDebugDrawStore CDispatchAdvancedSpawnHelper::ms_DebugDrawStore(20);
#endif

CDispatchAdvancedSpawnHelper::CDispatchAdvancedSpawnHelper()
: CDispatchSpawnHelper()
, m_aSpawnPoints()
, m_aDispatchVehicles()
, m_fTimeSinceLastInvalidDispatchVehiclesInvalidated(0.0f)
, m_fTimeSinceLastDispatchVehiclesMarkedForDespawn(0.0f)
, m_fTimeSinceLastValidSpawnPointWasFound(0.0f)
, m_uAdvancedRunningFlags(0)
, m_uIndex(0)
{
}

CDispatchAdvancedSpawnHelper::~CDispatchAdvancedSpawnHelper()
{
}

void CDispatchAdvancedSpawnHelper::TrackDispatchVehicle(CVehicle& rVehicle, u8 uConfigFlags)
{
	//Find an empty dispatch vehicle.
	for(int i = 0; i < sNumDispatchVehicles; ++i)
	{
		//Ensure the dispatch vehicle is invalid.
		DispatchVehicle& rDispatchVehicle = m_aDispatchVehicles[i];
		if(rDispatchVehicle.GetIsValid())
		{
			continue;
		}

		//Set the vehicle.
		rDispatchVehicle.Set(&rVehicle, uConfigFlags);
		
		//Nothing else to do.
		return;
	}
}

#if __BANK
void CDispatchAdvancedSpawnHelper::RenderDebug()
{
	//Call the base class version.
	CDispatchSpawnHelper::RenderDebug();

	//Ensure rendering is enabled.
	if(!sm_Tunables.m_Rendering.m_Enabled)
	{
		return;
	}

	//Render the debug draw store.
	ms_DebugDrawStore.Render();
}
#endif

bool CDispatchAdvancedSpawnHelper::CanChooseNextBestSpawnPoint() const
{
	//Ensure we are ready.
	if(!IsReady())
	{
		return false;
	}
	
	return true;
}

bool CDispatchAdvancedSpawnHelper::CanFindNextSpawnPoint() const
{
	//Ensure the dispatch node is valid.
	if(m_DispatchNode.IsEmpty())
	{
		return false;
	}
	
	return true;
}

bool CDispatchAdvancedSpawnHelper::CanScoreSpawnPoints() const
{
	//Ensure we are ready.
	if(!IsReady())
	{
		return false;
	}

	return true;
}

ScalarV_Out CDispatchAdvancedSpawnHelper::FindDistSqToClosestDispatchVehicle(Vec3V_In vPosition) const
{
	//Find the closest dispatch vehicle.
	ScalarV scClosestDistSq(V_FLT_MAX);
	for(int i = 0; i < sNumDispatchVehicles; ++i)
	{
		//Ensure the dispatch vehicle is valid.
		const DispatchVehicle& rDispatchVehicle = m_aDispatchVehicles[i];
		if(!rDispatchVehicle.GetIsValid())
		{
			continue;
		}

		//Ensure the vehicle is valid.
		const CVehicle* pVehicle = rDispatchVehicle.m_pVehicle;
		if(!pVehicle)
		{
			continue;
		}

		//Ensure the distance is closer.
		ScalarV scDistSq = DistSquared(vPosition, pVehicle->GetTransform().GetPosition());
		if(IsGreaterThanAll(scDistSq, scClosestDistSq))
		{
			continue;
		}

		//Assign the distance.
		scClosestDistSq = scDistSq;
	}

	return scClosestDistSq;
}

void CDispatchAdvancedSpawnHelper::FindNextSpawnPoint()
{
	//Assert that we can find the next spawn point.
	aiAssertf(CanFindNextSpawnPoint(), "Unable to find next spawn point.");
	
	//Find a spawn point for the index.
	FindSpawnPoint(m_uIndex++);

	//Check if the index has reached the end.
	if(m_uIndex == sNumSpawnPoints)
	{
		//Reset the index.
		m_uIndex = 0;
		
		//Set the ready flag.
		m_uAdvancedRunningFlags.SetFlag(ARF_IsReady);
		
#if __BANK
		//Cache the debug data.
		CacheDebug();
#endif
	}
}

void CDispatchAdvancedSpawnHelper::FindSpawnPoint(const u8 uIndex)
{
	//Ensure the index is valid.
	if(!aiVerifyf(uIndex < sNumSpawnPoints, "Index is out of bounds: %d", uIndex))
	{
		return;
	}

	//Grab the spawn point.
	AdvancedSpawnPoint& rSpawnPoint = m_aSpawnPoints[uIndex];

	//Invalidate the spawn point.
	rSpawnPoint.m_bValid = false;

	//Generate the direction to search based on the index.
	static const float sNumSpawnPointsInverse = 1.0f / sNumSpawnPoints;
	float fDegrees = 360.0f * sNumSpawnPointsInverse * uIndex;
	float fRadians = fDegrees * DtoR;
	Vector2 vDirection(cos(fRadians), sin(fRadians));
	
	//Find the spawn point in the direction.
	if(!FindSpawnPointInDirection(vDirection, rSpawnPoint.m_SpawnPoint))
	{
		return;
	}
	
	//Ensure the spawn point does not already exist.
	for(int i = 0; i < sNumSpawnPoints; ++i)
	{
		//Ensure the other spawn point is valid.
		const AdvancedSpawnPoint& rOtherSpawnPoint = m_aSpawnPoints[i];
		if(!rOtherSpawnPoint.m_bValid)
		{
			continue;
		}
		
		//Ensure the other node does not match.
		if(rOtherSpawnPoint.m_SpawnPoint.m_Node != rSpawnPoint.m_SpawnPoint.m_Node)
		{
			continue;
		}
		
		//This spawn point has already been added.
		return;
	}
	
	//Note that the spawn point is valid.
	rSpawnPoint.m_bValid = true;

	//Clear the timer.
	m_fTimeSinceLastValidSpawnPointWasFound = 0.0f;
}

bool CDispatchAdvancedSpawnHelper::HasValidSpawnPoint() const
{
	//Iterate over the spawn points.
	for(int i = 0; i < sNumSpawnPoints; ++i)
	{
		if(m_aSpawnPoints[i].m_bValid)
		{
			return true;
		}
	}

	return false;
}

void CDispatchAdvancedSpawnHelper::ForceDespawn(CVehicle& rVehicle)
{
	//Grab the seat manager.
	const CSeatManager* pSeatManager = rVehicle.GetSeatManager();
	aiAssertf(pSeatManager, "Seat manager is invalid.");
	
	//Iterate over the passengers.
	for(s32 i = 0; i < MAX_VEHICLE_SEATS; i++)
	{
		//Ensure the ped is valid.
		CPed* pPed = pSeatManager->GetPedInSeat(i);
		if(!pPed)
		{
			continue;
		}
		
		//Ensure the order is valid.
		COrder* pOrder = pPed->GetPedIntelligence()->GetOrder();
		if(!pOrder)
		{
			continue;
		}
		
		//Flag the order for deletion.
		if (pOrder->IsLocallyControlled())
		{
			pOrder->SetFlagForDeletion(true);
		}
	}
	
	//Remove the vehicle.
	CVehiclePopulation::RemoveVeh(&rVehicle, true, CVehiclePopulation::Removal_None);
}

void CDispatchAdvancedSpawnHelper::InvalidateDispatchVehicles()
{
	//Invalidate the dispatch vehicles.
	for(int i = 0; i < sNumDispatchVehicles; ++i)
	{
		m_aDispatchVehicles[i].Invalidate();
	}
}

void CDispatchAdvancedSpawnHelper::InvalidateInvalidDispatchVehicles()
{
	//Invalidate the invalid dispatch vehicles.
	for(int i = 0; i < sNumDispatchVehicles; ++i)
	{
		//Ensure the dispatch vehicle is valid.
		DispatchVehicle& rDispatchVehicle = m_aDispatchVehicles[i];
		if(!rDispatchVehicle.GetIsValid())
		{
			continue;
		}

		//Check if the vehicle is valid.
		const CVehicle* pVehicle = rDispatchVehicle.m_pVehicle;
		if(!pVehicle)
		{
			//Invalidate the dispatch vehicle.
			rDispatchVehicle.Invalidate();
			continue;
		}
		
		//Check if the vehicle must have a driver.
		if(!rDispatchVehicle.GetCanHaveNoDriver())
		{
			//Check if the vehicle has a driver.
			const CPed* pDriver = pVehicle->GetDriver();
			if(!pDriver)
			{
				//Invalidate the dispatch vehicle.
				rDispatchVehicle.Invalidate();
				continue;
			}

			//Check if the driver has an order.
			COrder* pOrder = pDriver->GetPedIntelligence()->GetOrder();
			if(!pOrder)
			{
				//Invalidate the dispatch vehicle.
				rDispatchVehicle.Invalidate();
				continue;
			}
			
			//Check if the order is not flagged to be deleted.
			if(pOrder->GetFlagForDeletion())
			{
				//Invalidate the dispatch vehicle.
				rDispatchVehicle.Invalidate();
				continue;
			}
		}
	}
	
	//Note that invalid dispatch vehicles have been invalidated.
	OnInvalidDispatchVehiclesInvalidated();
}

void CDispatchAdvancedSpawnHelper::InvalidateSpawnPoints()
{
	//Invalidate the spawn points.
	for(int i = 0; i < sNumSpawnPoints; ++i)
	{
		m_aSpawnPoints[i].m_bValid = false;
	}
	
#if __BANK
	//Cache the debug data.
	CacheDebug();
#endif
}

bool CDispatchAdvancedSpawnHelper::IsWithinRangeOfDispatchVehicles(const CVehicle& rVehicle, float fRadius, u32 uCount) const
{
	//Ensure the count is valid.
	if(uCount == 0)
	{
		return true;
	}

	//Square the radius.
	ScalarV scRadiusSq = ScalarVFromF32(square(fRadius));

	//Iterate over the dispatch vehicles.
	for(int i = 0; i < sNumDispatchVehicles; ++i)
	{
		//Ensure the dispatch vehicle is valid.
		const DispatchVehicle& rDispatchVehicle = m_aDispatchVehicles[i];
		if(!rDispatchVehicle.GetIsValid())
		{
			continue;
		}

		//Ensure the vehicle is valid.
		const CVehicle* pVehicle = rDispatchVehicle.m_pVehicle;
		if(!pVehicle)
		{
			continue;
		}

		//Ensure the vehicles do not match.
		if(pVehicle == &rVehicle)
		{
			continue;
		}

		//Ensure the distance is within the radius.
		ScalarV scDistSq = DistSquared(pVehicle->GetTransform().GetPosition(), rVehicle.GetTransform().GetPosition());
		if(IsGreaterThanAll(scDistSq, scRadiusSq))
		{
			continue;
		}

		//Decrease the count.
		uCount--;
		if(uCount == 0)
		{
			return true;
		}
	}

	return false;
}

void CDispatchAdvancedSpawnHelper::MarkDispatchVehiclesForDespawn()
{
	//Ensure the vehicle that the ped is escaping in is valid.
	const CVehicle* pPedVehicle = GetVehiclePedEscapingIn();
	if(!pPedVehicle)
	{
		return;
	}
	
	//Grab the ped vehicle position.
	Vec3V vPedVehiclePosition = pPedVehicle->GetTransform().GetPosition();
	
	//Grab the ped vehicle forward vector.
	Vec3V vPedVehicleForward = pPedVehicle->GetTransform().GetForward();
	
	//Grab the ped vehicle velocity.
	Vec3V vPedVehicleVelocity = RCC_VEC3V(pPedVehicle->GetVelocity());
	
	//Calculate the ped vehicle velocity direction.
	Vec3V vPedVehicleVelocityDirection = NormalizeFastSafe(vPedVehicleVelocity, Vec3V(V_ZERO));
	
	//Keep track of the despawn limits.
	CWanted::DespawnLimits limits;
	
	//Check if the wanted is valid.
	const CWanted* pWanted = GetWanted();
	if(pWanted)
	{
		//Calculate the despawn limits.
		pWanted->CalculateDespawnLimits(limits);
	}
	
	//Iterate over the dispatch vehicles.
	for(int i = 0; i < sNumDispatchVehicles; ++i)
	{
		//Ensure the dispatch vehicle is valid.
		DispatchVehicle& rDispatchVehicle = m_aDispatchVehicles[i];
		if(!rDispatchVehicle.GetIsValid())
		{
			continue;
		}
		
		//Ensure the vehicle can be despawned.
		if(rDispatchVehicle.GetCantDespawn())
		{
			continue;
		}

		//Ensure the vehicle is valid.
		CVehicle* pVehicle = rDispatchVehicle.m_pVehicle;
		if(!pVehicle)
		{
			continue;
		}
		
		//Check if the network object is valid.
		netObject* pNetObj = pVehicle->GetNetworkObject();
		if(pNetObj)
		{
			//Ensure the network object can be deleted.
			if(!pNetObj->CanDelete())
			{
				continue;
			}
		}
		
		//Ensure the vehicle is not a clone.
		if(pVehicle->IsNetworkClone())
		{
			continue;
		}
		
		//Grab the vehicle position.
		Vec3V vVehiclePosition = pVehicle->GetTransform().GetPosition();
		
		//Generate a direction from the ped vehicle to the vehicle.
		Vec3V vPedVehicleToVehicle = Subtract(vVehiclePosition, vPedVehiclePosition);
		Vec3V vPedVehicleToVehicleDirection = NormalizeFastSafe(vPedVehicleToVehicle, Vec3V(V_ZERO));
		
		//Ensure the ped vehicle is facing away from the vehicle.
		ScalarV scPedVehicleFacingAwayFromVehicle = Dot(vPedVehicleForward, vPedVehicleToVehicleDirection);
		ScalarV scFacingThreshold = ScalarVFromF32(limits.m_fMaxFacingThreshold);
		if(IsGreaterThanAll(scPedVehicleFacingAwayFromVehicle, scFacingThreshold))
		{
			continue;
		}
		
		//Ensure the ped vehicle is moving away from the vehicle.
		ScalarV scPedVehicleMovingAwayFromVehicle = Dot(vPedVehicleVelocityDirection, vPedVehicleToVehicleDirection);
		ScalarV scMovingThreshold = ScalarVFromF32(limits.m_fMaxMovingThreshold);
		if(IsGreaterThanAll(scPedVehicleMovingAwayFromVehicle, scMovingThreshold))
		{
			continue;
		}
		
		//Ensure the vehicle is off-screen.
		if(pVehicle->GetIsOnScreen(true))
		{
			continue;
		}
		
		//Calculate the distance (squared) between the vehicles.
		ScalarV scDistSq = DistSquared(vVehiclePosition, vPedVehiclePosition);
		
		//Check if the vehicle should be despawned because it is lagging behind.
		{
			//Ensure the distance exceeds the threshold.
			static const ScalarV scMinDistSq = ScalarVFromF32(square(limits.m_fMinDistanceToBeConsideredLaggingBehind));
			if(IsGreaterThanAll(scDistSq, scMinDistSq))
			{
				//Force despawn the vehicle.
				ForceDespawn(*pVehicle);
				continue;
			}
		}
		
		//Check if the vehicle should be despawned because it is clumped together with other dispatch vehicles.
		{
			//Ensure the distance exceeds the threshold.
			static const ScalarV scMinDistSq = ScalarVFromF32(square(limits.m_fMinDistanceToCheckClumped));
			if(IsGreaterThanAll(scDistSq, scMinDistSq))
			{
				//Ensure we are within a certain range of other dispatch vehicles.
				if(IsWithinRangeOfDispatchVehicles(*pVehicle, limits.m_fMaxDistanceToBeConsideredClumped, 1))
				{
					//Force despawn the vehicle.
					ForceDespawn(*pVehicle);
					continue;
				}
			}
		}
	}
	
	//Note that vehicles have been marked for despawn.
	OnDispatchVehiclesMarkedForDespawn();
}

void CDispatchAdvancedSpawnHelper::OnDispatchVehiclesMarkedForDespawn()
{
	//Clear the timer.
	m_fTimeSinceLastDispatchVehiclesMarkedForDespawn = 0.0f;
}

void CDispatchAdvancedSpawnHelper::OnInvalidDispatchVehiclesInvalidated()
{
	//Clear the timer.
	m_fTimeSinceLastInvalidDispatchVehiclesInvalidated = 0.0f;
}

void CDispatchAdvancedSpawnHelper::ScoreSpawnPoints()
{
	//Assert that we can score the spawn points.
	aiAssertf(CanScoreSpawnPoints(), "Unable to score spawn points.");
	
	//Grab the ped position.
	Vec3V vPedPosition = GetLocation();
	
	//Grab the ped direction.
	Vec3V vPedDirection = GetDirection();
	
	//Only use the direction if the ped is escaping in a vehicle.
	bool bUseDirection = IsPedEscapingInVehicle();
	
	//Keep track of the spawn score weights.
	CWanted::SpawnScoreWeights weights;

	//Check if the wanted is valid.
	const CWanted* pWanted = GetWanted();
	if(pWanted)
	{
		//Calculate the spawn score weights.
		pWanted->CalculateSpawnScoreWeights(weights);
	}

	//Score the spawn points.
	for(int i = 0; i < sNumSpawnPoints; ++i)
	{	
		//Grab the spawn point.
		AdvancedSpawnPoint& rSpawnPoint = m_aSpawnPoints[i];

		//Initialize the score.
		rSpawnPoint.m_fScore = 0.0f;

		//Ensure the spawn point is valid.
		if(!rSpawnPoint.m_bValid)
		{
			continue;
		}

		//Ensure the node is valid.
		const CPathNode* pNode = ThePaths.FindNodePointerSafe(rSpawnPoint.m_SpawnPoint.m_Node);
		if(!pNode)
		{
			continue;
		}

		//Score the spawn point.
		float fSpawnPointDistance	= 0.0f;
		float fSpawnPointDirection	= 0.0f;
		float fSpawnPointRandomness	= 0.0f;
		{
			//Grab the position.
			Vec3V vPosition;
			pNode->GetCoors(RC_VECTOR3(vPosition));
			
			//Find the distance to the closest dispatch vehicle, squared.
			ScalarV scDistSqToClosestDispatchVehicle = FindDistSqToClosestDispatchVehicle(vPosition);

			//Clamp and divide the distance by the maximum to get a value between 0 ... 1.
			static ScalarV scMaxDistSqToClosestDispatchPosition = ScalarVFromF32(square(2.0f * GetIdealSpawnDistance()));
			static ScalarV scMaxDistSqToClosestDispatchPositionInv = Invert(scMaxDistSqToClosestDispatchPosition);
			scDistSqToClosestDispatchVehicle = Clamp(scDistSqToClosestDispatchVehicle, ScalarV(V_ZERO), scMaxDistSqToClosestDispatchPosition);
			ScalarV scDistSqToClosestDispatchVehicleNormalized = Scale(scDistSqToClosestDispatchVehicle, scMaxDistSqToClosestDispatchPositionInv);

			//The distance is now normalized from 0 ... 1, where 0 is very close to another dispatch and 1 is very far away.
			//We want dispatch to spawn far away from each other to create an effective perimeter, so favor higher numbers.
			fSpawnPointDistance = scDistSqToClosestDispatchVehicleNormalized.Getf();

			//Check if we should use the direction.
			if(bUseDirection)
			{
				//Generate the vector from the ped to the spawn point.
				Vec3V vPedToSpawnPoint = Subtract(vPosition, vPedPosition);

				//Generate the direction.
				Vec3V vDirection = NormalizeFastSafe(vPedToSpawnPoint, Vec3V(V_ZERO));

				//Generate the dot product between the direction and the ped direction.
				//This value will range from -1 ... 1.
				ScalarV scDot = Dot(vDirection, vPedDirection);

				//Normalize the dot product from 0 ... 1.
				ScalarV scDotN = AddScaled(ScalarV(V_HALF), scDot, ScalarV(V_HALF));

				//Assign the value.
				fSpawnPointDirection = scDotN.Getf();
			}

			//Generate a random number.
			fSpawnPointRandomness = fwRandom::GetRandomNumberInRange(0.0f, 1.0f);

			//Build the final score from the weights.
			rSpawnPoint.m_fScore = (weights.m_fDistance * fSpawnPointDistance) + (weights.m_fDirection * fSpawnPointDirection) + (weights.m_fRandomness * fSpawnPointRandomness);
		}
	}
}

bool CDispatchAdvancedSpawnHelper::ShouldInvalidateInvalidDispatchVehicles() const
{
	//Check if the time has exceeded the threshold.
	float fTimeBetweenInvalidateInvalidDispatchVehicles = sm_Tunables.m_TimeBetweenInvalidateInvalidDispatchVehicles;
	if(m_fTimeSinceLastInvalidDispatchVehiclesInvalidated >= fTimeBetweenInvalidateInvalidDispatchVehicles)
	{
		return true;
	}

	return false;
}

bool CDispatchAdvancedSpawnHelper::ShouldMarkDispatchVehiclesForDespawn() const
{
	//Check if the time has exceeded the threshold.
	float fTimeBetweenMarkDispatchVehiclesForDespawn = sm_Tunables.m_TimeBetweenMarkDispatchVehiclesForDespawn;
	if(m_fTimeSinceLastDispatchVehiclesMarkedForDespawn >= fTimeBetweenMarkDispatchVehiclesForDespawn)
	{
		return true;
	}

	return false;
}

void CDispatchAdvancedSpawnHelper::UpdateAdvancedTimers(const float fTimeStep)
{
	//Update the timers.
	m_fTimeSinceLastInvalidDispatchVehiclesInvalidated	+= fTimeStep;
	m_fTimeSinceLastDispatchVehiclesMarkedForDespawn	+= fTimeStep;
	m_fTimeSinceLastValidSpawnPointWasFound				+= fTimeStep;
}

void CDispatchAdvancedSpawnHelper::UpdateSpawnPoints()
{
	//Ensure we can find the next spawn point.
	if(!CanFindNextSpawnPoint())
	{
		return;
	}
	
	//Find the next spawn point.
	FindNextSpawnPoint();
}

#if __BANK
void CDispatchAdvancedSpawnHelper::CacheDebug()
{
	//Clear all the drawables.
	ms_DebugDrawStore.ClearAll();
	
	//Add the spawn points.
	for(int i = 0; i < sNumSpawnPoints; ++i)
	{
		//Ensure the spawn point is valid.
		const AdvancedSpawnPoint& rSpawnPoint = m_aSpawnPoints[i];
		if(!rSpawnPoint.m_bValid)
		{
			continue;
		}
		
		//Ensure the node is valid.
		const CPathNode* pNode = ThePaths.FindNodePointerSafe(rSpawnPoint.m_SpawnPoint.m_Node);
		if(!pNode)
		{
			continue;
		}
		
		//Get the coordinates.
		Vector3 vCoors;
		pNode->GetCoors(vCoors);
		
		//Add a circle to the vector map.
		ms_DebugDrawStore.AddVectorMapCircle(VECTOR3_TO_VEC3V(vCoors), 5.0f, Color_green);
	}
}
#endif

bool CDispatchAdvancedSpawnHelper::CanChooseBestSpawnPoint() const
{
	//Ensure we can score the spawn points.
	if(!CanScoreSpawnPoints())
	{
		return false;
	}
	
	//Ensure we can choose the next best spawn point.
	if(!CanChooseNextBestSpawnPoint())
	{
		return false;
	}
	
	return true;
}

const CDispatchSpawnHelper::SpawnPoint* CDispatchAdvancedSpawnHelper::ChooseBestSpawnPoint()
{
	//Assert that we can choose the best spawn point.
	aiAssertf(CanChooseBestSpawnPoint(), "Unable to choose best spawn point.");
	
	//Score the spawn points.
	ScoreSpawnPoints();

	//Iterate over the spawn points.
	for(int i = 0; i < sNumSpawnPoints; ++i)
	{
		//Clear the visited flag.
		m_aSpawnPoints[i].m_bVisited = false;
	}

	return ChooseNextBestSpawnPoint();
}

const CDispatchSpawnHelper::SpawnPoint* CDispatchAdvancedSpawnHelper::ChooseNextBestSpawnPoint()
{
	//Assert that we can choose the next best spawn point.
	aiAssertf(CanChooseNextBestSpawnPoint(), "Unable to choose next best spawn point.");
	
	//Iterate over the spawn points.
	AdvancedSpawnPoint* pBestSpawnPoint = NULL;
	for(int i = 0; i < sNumSpawnPoints; ++i)
	{
		//Ensure the spawn point is valid.
		AdvancedSpawnPoint& rSpawnPoint = m_aSpawnPoints[i];
		if(!rSpawnPoint.m_bValid)
		{
			continue;
		}

		//Ensure the spawn point has not been visited.
		if(rSpawnPoint.m_bVisited)
		{
			continue;
		}

		//Ensure the spawn point is better.
		if(pBestSpawnPoint && (rSpawnPoint.m_fScore <= pBestSpawnPoint->m_fScore))
		{
			continue;
		}

		//Assign the best spawn point.
		pBestSpawnPoint = &rSpawnPoint;
	}

	//Ensure the best spawn point is valid.
	if(!pBestSpawnPoint)
	{
		return NULL;
	}

	//Mark the spawn point as visited.
	pBestSpawnPoint->m_bVisited = true;

	return &pBestSpawnPoint->m_SpawnPoint;
}

bool CDispatchAdvancedSpawnHelper::FailedToFindSpawnPoint() const
{
	//Check if the time since we last found a valid spawn point has exceeded the threshold.
	static float s_fMinTime = 10.0f;
	if(m_fTimeSinceLastValidSpawnPointWasFound > s_fMinTime)
	{
		return true;
	}

	return CDispatchSpawnHelper::FailedToFindSpawnPoint();
}

void CDispatchAdvancedSpawnHelper::OnDeactivate()
{
	//Call the base class version.
	CDispatchSpawnHelper::OnDeactivate();

	//Invalidate the spawn points.
	InvalidateSpawnPoints();

	//Invalidate the dispatch vehicles.
	InvalidateDispatchVehicles();

	//Clear the timers.
	m_fTimeSinceLastInvalidDispatchVehiclesInvalidated	= 0.0f;
	m_fTimeSinceLastDispatchVehiclesMarkedForDespawn	= 0.0f;
	m_fTimeSinceLastValidSpawnPointWasFound				= 0.0f;

	//Clear the ready flag.
	m_uAdvancedRunningFlags.ClearFlag(ARF_IsReady);

	//Clear the index.
	m_uIndex = 0;
}

void CDispatchAdvancedSpawnHelper::OnVehicleSpawned(CVehicle& rVehicle)
{
	//Call the base class version.
	CDispatchSpawnHelper::OnVehicleSpawned(rVehicle);
	
	//Track the vehicle.
	TrackDispatchVehicle(rVehicle);
}

void CDispatchAdvancedSpawnHelper::Update(float fTimeStep)
{
	//Call the base class version.
	CDispatchSpawnHelper::Update(fTimeStep);

	//Update the advanced timers.
	UpdateAdvancedTimers(fTimeStep);

	//Update the spawn points.
	UpdateSpawnPoints();

	//Invalidate the invalid dispatch vehicles.
	if(ShouldInvalidateInvalidDispatchVehicles())
	{
		InvalidateInvalidDispatchVehicles();
	}

	//Mark vehicles for despawn.
	if(ShouldMarkDispatchVehiclesForDespawn())
	{
		MarkDispatchVehiclesForDespawn();
	}
}

////////////////////////////////////////////////////////////////////////////////
// CDispatchSpawnHelpers
////////////////////////////////////////////////////////////////////////////////

CDispatchSpawnHelpers::CDispatchSpawnHelpers()
: m_aAdvancedSpawnHelpers()
, m_aBasicSpawnHelpers()
{

}

CDispatchSpawnHelpers::~CDispatchSpawnHelpers()
{

}

CDispatchSpawnHelper* CDispatchSpawnHelpers::FindSpawnHelperForEntity(const CEntity& rEntity)
{
	//Iterate over the advanced spawn helpers.
	int iNumSpawnHelpers = m_aAdvancedSpawnHelpers.GetMaxCount();
	for(int i = 0; i < iNumSpawnHelpers; ++i)
	{
		//Check if the incident matches.
		CDispatchAdvancedSpawnHelper& rSpawnHelper = m_aAdvancedSpawnHelpers[i];
		if(&rEntity == rSpawnHelper.GetEntity())
		{
			return &rSpawnHelper;
		}
	}

	//Iterate over the basic spawn helpers.
	iNumSpawnHelpers = m_aBasicSpawnHelpers.GetMaxCount();
	for(int i = 0; i < iNumSpawnHelpers; ++i)
	{
		//Check if the incident matches.
		CDispatchBasicSpawnHelper& rSpawnHelper = m_aBasicSpawnHelpers[i];
		if(&rEntity == rSpawnHelper.GetEntity())
		{
			return &rSpawnHelper;
		}
	}

	return NULL;
}

CDispatchSpawnHelper* CDispatchSpawnHelpers::FindSpawnHelperForIncident(const CIncident& rIncident)
{
	//Iterate over the advanced spawn helpers.
	int iNumSpawnHelpers = m_aAdvancedSpawnHelpers.GetMaxCount();
	for(int i = 0; i < iNumSpawnHelpers; ++i)
	{
		//Check if the incident matches.
		CDispatchAdvancedSpawnHelper& rSpawnHelper = m_aAdvancedSpawnHelpers[i];
		if(&rIncident == rSpawnHelper.GetIncident())
		{
			return &rSpawnHelper;
		}
	}

	//Iterate over the basic spawn helpers.
	iNumSpawnHelpers = m_aBasicSpawnHelpers.GetMaxCount();
	for(int i = 0; i < iNumSpawnHelpers; ++i)
	{
		//Check if the incident matches.
		CDispatchBasicSpawnHelper& rSpawnHelper = m_aBasicSpawnHelpers[i];
		if(&rIncident == rSpawnHelper.GetIncident())
		{
			return &rSpawnHelper;
		}
	}

	return NULL;
}

void CDispatchSpawnHelpers::Update(float fTimeStep)
{
	//Deactivate the invalid spawn helpers.
	DeactivateInvalidSpawnHelpers();

	//Set incidents for the spawn helpers.
	SetIncidentsForSpawnHelpers();
	
	//Update the spawn helpers.
	UpdateSpawnHelpers(fTimeStep);
}

#if __BANK
void CDispatchSpawnHelpers::RenderDebug()
{
	//Iterate over the advanced spawn helpers.
	int iNumSpawnHelpers = m_aAdvancedSpawnHelpers.GetMaxCount();
	for(int i = 0; i < iNumSpawnHelpers; ++i)
	{
		//Ensure the spawn helper is active.
		CDispatchAdvancedSpawnHelper& rSpawnHelper = m_aAdvancedSpawnHelpers[i];
		if(!rSpawnHelper.IsActive())
		{
			continue;
		}

		//Debug the spawn helper.
		rSpawnHelper.RenderDebug();
	}

	//Iterate over the basic spawn helpers.
	iNumSpawnHelpers = m_aBasicSpawnHelpers.GetMaxCount();
	for(int i = 0; i < iNumSpawnHelpers; ++i)
	{
		//Ensure the spawn helper is active.
		CDispatchBasicSpawnHelper& rSpawnHelper = m_aBasicSpawnHelpers[i];
		if(!rSpawnHelper.IsActive())
		{
			continue;
		}

		//Debug the spawn helper.
		rSpawnHelper.RenderDebug();
	}
}
#endif

void CDispatchSpawnHelpers::DeactivateInvalidSpawnHelpers()
{
	//Iterate over the advanced spawn helpers.
	int iNumSpawnHelpers = m_aAdvancedSpawnHelpers.GetMaxCount();
	for(int i = 0; i < iNumSpawnHelpers; ++i)
	{
		//Ensure the spawn helper is active.
		CDispatchAdvancedSpawnHelper& rSpawnHelper = m_aAdvancedSpawnHelpers[i];
		if(!rSpawnHelper.IsActive())
		{
			continue;
		}

		//Ensure the spawn helper should be active.
		if(rSpawnHelper.ShouldBeActive())
		{
			continue;
		}

		//Deactivate the spawn helper.
		rSpawnHelper.Deactivate();
	}

	//Iterate over the basic spawn helpers.
	iNumSpawnHelpers = m_aBasicSpawnHelpers.GetMaxCount();
	for(int i = 0; i < iNumSpawnHelpers; ++i)
	{
		//Ensure the spawn helper is active.
		CDispatchBasicSpawnHelper& rSpawnHelper = m_aBasicSpawnHelpers[i];
		if(!rSpawnHelper.IsActive())
		{
			continue;
		}

		//Ensure the spawn helper should be active.
		if(rSpawnHelper.ShouldBeActive())
		{
			continue;
		}

		//Deactivate the spawn helper.
		rSpawnHelper.Deactivate();
	}
}

CDispatchAdvancedSpawnHelper* CDispatchSpawnHelpers::FindInactiveAdvancedSpawnHelper()
{
	//Iterate over the advanced spawn helpers.
	int iNumSpawnHelpers = m_aAdvancedSpawnHelpers.GetMaxCount();
	for(int i = 0; i < iNumSpawnHelpers; ++i)
	{
		//Ensure the spawn helper is inactive.
		CDispatchAdvancedSpawnHelper& rSpawnHelper = m_aAdvancedSpawnHelpers[i];
		if(rSpawnHelper.IsActive())
		{
			continue;
		}

		return &rSpawnHelper;
	}

	return NULL;
}

CDispatchBasicSpawnHelper* CDispatchSpawnHelpers::FindInactiveBasicSpawnHelper()
{
	//Iterate over the basic spawn helpers.
	int iNumSpawnHelpers = m_aBasicSpawnHelpers.GetMaxCount();
	for(int i = 0; i < iNumSpawnHelpers; ++i)
	{
		//Ensure the spawn helper is active.
		CDispatchBasicSpawnHelper& rSpawnHelper = m_aBasicSpawnHelpers[i];
		if(rSpawnHelper.IsActive())
		{
			continue;
		}

		return &rSpawnHelper;
	}

	return NULL;
}

void CDispatchSpawnHelpers::SetIncidentsForSpawnHelpers()
{
	//Iterate over the incidents.
	int iNumIncidents = CIncidentManager::GetInstance().GetMaxCount();
	for(int i = 0; i < iNumIncidents; ++i)
	{
		//Ensure the incident is valid.
		CIncident* pIncident = CIncidentManager::GetInstance().GetIncident(i);
		if(!pIncident)
		{
			continue;
		}

		//Ensure the incident is locally controlled.
		if(!pIncident->IsLocallyControlled())
		{
			continue;
		}

		//Ensure this incident has not been assigned a spawn helper.
		if(FindSpawnHelperForIncident(*pIncident))
		{
			continue;
		}

		//Check if we should use an advanced spawn helper.
		if(ShouldIncidentUseAdvancedSpawnHelper(*pIncident))
		{
			//Find an inactive advanced spawn helper.
			CDispatchAdvancedSpawnHelper* pSpawnHelper = FindInactiveAdvancedSpawnHelper();
			if(aiVerifyf(pSpawnHelper, "No advanced spawn helpers free."))
			{
				//Activate the spawn helper.
				pSpawnHelper->Activate(pIncident);
			}
		}
		else
		{
			//Find an inactive basic spawn helper.
			CDispatchBasicSpawnHelper* pSpawnHelper = FindInactiveBasicSpawnHelper();
			if(aiVerifyf(pSpawnHelper, "No basic spawn helpers free."))
			{
				//Activate the spawn helper.
				pSpawnHelper->Activate(pIncident);
			}
		}
	}
}

bool CDispatchSpawnHelpers::ShouldIncidentUseAdvancedSpawnHelper(const CIncident& rIncident) const
{
	//Ensure the entity is valid.
	const CEntity* pEntity = rIncident.GetEntity();
	if(!pEntity)
	{
		return false;
	}
	
	//Ensure the entity is a ped.
	if(!pEntity->GetIsTypePed())
	{
		return false;
	}
	
	//Ensure the ped is a player.
	const CPed* pPed = static_cast<const CPed *>(pEntity);
	if(!pPed->IsPlayer())
	{
		return false;
	}

	return true;
}

void CDispatchSpawnHelpers::UpdateSpawnHelpers(float fTimeStep)
{
	//Iterate over the advanced spawn helpers.
	int iNumSpawnHelpers = m_aAdvancedSpawnHelpers.GetMaxCount();
	for(int i = 0; i < iNumSpawnHelpers; ++i)
	{
		//Ensure the spawn helper is active.
		CDispatchSpawnHelper& rSpawnHelper = m_aAdvancedSpawnHelpers[i];
		if(!rSpawnHelper.IsActive())
		{
			continue;
		}

		//Update the spawn helper.
		rSpawnHelper.Update(fTimeStep);
	}

	//Iterate over the basic spawn helpers.
	iNumSpawnHelpers = m_aBasicSpawnHelpers.GetMaxCount();
	for(int i = 0; i < iNumSpawnHelpers; ++i)
	{
		//Ensure the spawn helper is active.
		CDispatchSpawnHelper& rSpawnHelper = m_aBasicSpawnHelpers[i];
		if(!rSpawnHelper.IsActive())
		{
			continue;
		}

		//Update the spawn helper.
		rSpawnHelper.Update(fTimeStep);
	}
}

////////////////////////////////////////////////////////////////////////////////
// CDispatchHelperSearch
////////////////////////////////////////////////////////////////////////////////

CDispatchHelperSearch::CDispatchHelperSearch()
{

}

CDispatchHelperSearch::~CDispatchHelperSearch()
{

}

bool CDispatchHelperSearch::Calculate(const CPed& rPed, const CPed& rTarget, const atArray<Constraints>& rConstraints, Vec3V_InOut rPosition)
{
	//Calculate the time since the ped was last spotted.
	float fTimeSinceLastSpotted = CalculateTimeSincePedWasLastSpotted(rTarget);

	//Choose the constraints to use.
	const Constraints* pConstraints = ChooseConstraints(rConstraints, fTimeSinceLastSpotted);
	if(!pConstraints)
	{
		return false;
	}

	//Calculate the search center.
	Vec3V vCenter = CalculateSearchCenter(rPed, rTarget, *pConstraints);

	//Calculate the max radius.
	float fMaxRadius = pConstraints->CalculateMaxRadius(fTimeSinceLastSpotted);

	//Calculate the max height.
	float fMaxHeight = pConstraints->m_MaxHeight;

	//Calculate the max angle.
	float fMaxAngle = pConstraints->m_MaxAngle;

	//Calculate the enclosed search region.
	const CDispatchHelperVolumes::Tunables::EnclosedSearchRegion* pEnclosedSearchRegion = NULL;
	if(pConstraints->m_UseEnclosedSearchRegions)
	{
		//Check if the enclosed search region is valid.
		pEnclosedSearchRegion = CDispatchHelperVolumes::GetTunables().m_EnclosedSearchRegions.Get(rTarget, vCenter);
		if(pEnclosedSearchRegion)
		{
			//Update the radius, so that we have a little better chance to land inside the area.
			float fDiameter = 2.0f * pEnclosedSearchRegion->GetRadius();
			fMaxRadius = Min(fMaxRadius, fDiameter);
		}
	}

	//Calculate the search position.
	static dev_s32 s_iMaxTries = 10;
	for(int i = 0; i < s_iMaxTries; ++i)
	{
		//Calculate the distance.
		float fDistance = fwRandom::GetRandomNumberInRange(0.0f, fMaxRadius);

		//Calculate the height.
		float fHeight = fwRandom::GetRandomNumberInRange(-fMaxHeight, fMaxHeight);

		//Calculate the angle.
		float fAngle = fwRandom::GetRandomNumberInRange(-fMaxAngle, fMaxAngle);

		//Calculate the offset.
		Vec3V vOffset(0.0f, fDistance, fHeight);
		vOffset = RotateAboutZAxis(vOffset, ScalarVFromF32(fAngle));

		//Calculate the position.
		rPosition = Add(vCenter, vOffset);

		//Ensure the position is not outside the enclosed search region.
		if(pEnclosedSearchRegion && !pEnclosedSearchRegion->ContainsPosition(rPosition))
		{
			continue;
		}

		return true;
	}
	
	return false;
}

Vec3V_Out CDispatchHelperSearch::CalculateSearchCenter(const CPed& rPed, const CPed& rTarget, const Constraints& rConstraints)
{
	//Check if we should try to use the last seen position.
	const CWanted* pWanted = rTarget.GetPlayerWanted();
	if(!pWanted || rConstraints.m_UseLastSeenPosition)
	{
		//Check if the targeting is valid.
		CPedTargetting* pTargeting = rPed.GetPedIntelligence()->GetTargetting(false);
		if(pTargeting)
		{
			//Check if the last seen position is valid.
			Vec3V vLastSeenPosition;
			if(pTargeting->GetTargetLastSeenPos(&rTarget, RC_VECTOR3(vLastSeenPosition)))
			{
				return vLastSeenPosition;
			}
		}
	}

	//Check if the wanted is valid.
	if(pWanted)
	{
		return VECTOR3_TO_VEC3V(pWanted->m_CurrentSearchAreaCentre);
	}

	return rTarget.GetTransform().GetPosition();
}

float CDispatchHelperSearch::CalculateTimeSincePedWasLastSpotted(const CPed& rPed)
{
	//Check if the wanted is valid.
	const CWanted* pWanted = rPed.GetPlayerWanted();
	if(pWanted)
	{
		u32 uTimeSinceLastSpotted = pWanted->GetTimeSinceSearchLastRefocused();
		return (float)uTimeSinceLastSpotted / 1000.0f;
	}
	else
	{
		//This is hard-coded for now, since this information is not kept around anywhere.
		return 10.0f;
	}
}

const CDispatchHelperSearch::Constraints* CDispatchHelperSearch::ChooseConstraints(const atArray<Constraints>& rConstraints, float fTimeSinceLastSpotted)
{
	//Iterate over the constraints.
	const Constraints* pDefault = NULL;
	for(int i = 0; i < rConstraints.GetCount(); ++i)
	{
		const Constraints* pConstraints = &rConstraints[i];
		if((fTimeSinceLastSpotted >= pConstraints->m_MinTimeSinceLastSpotted) &&
			(fTimeSinceLastSpotted <= pConstraints->m_MaxTimeSinceLastSpotted))
		{
			return pConstraints;
		}
		else if(pConstraints->m_UseByDefault)
		{
			pDefault = pConstraints;
		}
	}

	return pDefault;
}

////////////////////////////////////////////////////////////////////////////////
// CDispatchHelperSearchOnFoot
////////////////////////////////////////////////////////////////////////////////

CDispatchHelperSearchOnFoot::Tunables CDispatchHelperSearchOnFoot::sm_Tunables;
IMPLEMENT_DISPATCH_HELPERS_TUNABLES(CDispatchHelperSearchOnFoot, 0x9b3bbfc7);

CDispatchHelperSearchOnFoot::CDispatchHelperSearchOnFoot()
{
}

CDispatchHelperSearchOnFoot::~CDispatchHelperSearchOnFoot()
{
}

bool CDispatchHelperSearchOnFoot::IsResultReady(bool& bWasSuccessful, Vec3V_InOut vPosition)
{
	//Check if the search is done.
	SearchStatus nSearchStatus;
	static const int s_iMaxNumPositions = 1;
	Vector3 aPositions[1];
	s32 iNumPositions = 0;
	if(m_NavmeshClosestPositionHelper.GetSearchResults(nSearchStatus, iNumPositions, aPositions, s_iMaxNumPositions))
	{
		//Check if the search was successful.
		if((nSearchStatus == SS_SearchSuccessful) && (iNumPositions > 0))
		{
			//The search was successful.
			bWasSuccessful = true;

			//Set the position.
			vPosition = VECTOR3_TO_VEC3V(aPositions[0]);
		}
		else
		{
			//The search was not successful.
			bWasSuccessful = false;
		}

		return true;
	}
	else
	{
		return false;
	}
}

bool CDispatchHelperSearchOnFoot::Start(const CPed& rPed, const CPed& rTarget)
{
	//Call the search version.
	Vec3V vPosition;
	if(!CDispatchHelperSearch::Calculate(rPed, rTarget, sm_Tunables.m_Constraints, vPosition))
	{
		return false;
	}

	//Start the search.
	if(!m_NavmeshClosestPositionHelper.StartClosestPositionSearch(VEC3V_TO_VECTOR3(vPosition), sm_Tunables.m_MaxDistanceFromNavMesh,
		CNavmeshClosestPositionHelper::Flag_ConsiderDynamicObjects|CNavmeshClosestPositionHelper::Flag_ConsiderExterior|
		CNavmeshClosestPositionHelper::Flag_ConsiderInterior|CNavmeshClosestPositionHelper::Flag_ConsiderOnlyLand))
	{
		return false;
	}

	return true;
}

bool CDispatchHelperSearchOnFoot::Calculate(const CPed& rPed, const CPed& rTarget, Vec3V_InOut rPosition)
{
	//Call the search version.
	Vec3V vPosition;
	if(!CDispatchHelperSearch::Calculate(rPed, rTarget, sm_Tunables.m_Constraints, vPosition))
	{
		return false;
	}
	
	//Get the closest position.
	if(CPathServer::GetClosestPositionForPed(RCC_VECTOR3(vPosition), RC_VECTOR3(rPosition), sm_Tunables.m_MaxDistanceFromNavMesh) == ENoPositionFound)
	{
		return false;
	}
	
	return true;
}

float CDispatchHelperSearchOnFoot::GetMoveBlendRatio(const CPed& UNUSED_PARAM(rPed))
{
	//Calculate a move/blend ratio.
	static dev_float s_fMinMoveBlendRatio = 1.9f;
	static dev_float s_fMaxMoveBlendRatio = 2.1f;
	float fMoveBlendRatio = fwRandom::GetRandomNumberInRange(s_fMinMoveBlendRatio, s_fMaxMoveBlendRatio);

	return fMoveBlendRatio;
}

////////////////////////////////////////////////////////////////////////////////
// CDispatchHelperSearchInAutomobile
////////////////////////////////////////////////////////////////////////////////

CDispatchHelperSearchInAutomobile::Tunables CDispatchHelperSearchInAutomobile::sm_Tunables;
IMPLEMENT_DISPATCH_HELPERS_TUNABLES(CDispatchHelperSearchInAutomobile, 0x62c2a24f);

CDispatchHelperSearchInAutomobile::CDispatchHelperSearchInAutomobile()
{
}

CDispatchHelperSearchInAutomobile::~CDispatchHelperSearchInAutomobile()
{
}

bool CDispatchHelperSearchInAutomobile::Calculate(const CPed& rPed, const CPed& rTarget, Vec3V_InOut rPosition)
{
	//Call the search version.
	Vec3V vPosition;
	if(!CDispatchHelperSearch::Calculate(rPed, rTarget, sm_Tunables.m_Constraints, vPosition))
	{
		return false;
	}
	
	//Ensure the closest road node is valid.
	const CPathNode* pNode = ThePaths.FindNodePointerSafe(ThePaths.FindNodeClosestToCoors(VEC3V_TO_VECTOR3(vPosition), sm_Tunables.m_MaxDistanceFromRoadNode));
	if(!pNode)
	{
		return false;
	}
	
	//Get the coordinates.
	pNode->GetCoors(RC_VECTOR3(rPosition));
	
	return true;
}

fwFlags32 CDispatchHelperSearchInAutomobile::GetDrivingFlags()
{
	static fwFlags32 s_uFlags = (DMode_AvoidCars | DF_AdjustCruiseSpeedBasedOnRoadSpeed | DF_PreventJoinInRoadDirectionWhenMoving);
	return s_uFlags;
}

////////////////////////////////////////////////////////////////////////////////
// CDispatchHelperSearchInBoat
////////////////////////////////////////////////////////////////////////////////

CDispatchHelperSearchInBoat::Tunables CDispatchHelperSearchInBoat::sm_Tunables;
IMPLEMENT_DISPATCH_HELPERS_TUNABLES(CDispatchHelperSearchInBoat, 0xae3ca253);

CDispatchHelperSearchInBoat::CDispatchHelperSearchInBoat()
{
}

CDispatchHelperSearchInBoat::~CDispatchHelperSearchInBoat()
{
}

bool CDispatchHelperSearchInBoat::Calculate(const CPed& rPed, const CPed& rTarget, Vec3V_InOut rPosition)
{
	//Call the search version.
	if(!CDispatchHelperSearch::Calculate(rPed, rTarget, sm_Tunables.m_Constraints, rPosition))
	{
		return false;
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////
// CDispatchHelperSearchInHeli
////////////////////////////////////////////////////////////////////////////////

CDispatchHelperSearchInHeli::Tunables CDispatchHelperSearchInHeli::sm_Tunables;
IMPLEMENT_DISPATCH_HELPERS_TUNABLES(CDispatchHelperSearchInHeli, 0x456919b0);

CDispatchHelperSearchInHeli::CDispatchHelperSearchInHeli()
{
}

CDispatchHelperSearchInHeli::~CDispatchHelperSearchInHeli()
{
}

bool CDispatchHelperSearchInHeli::Calculate(const CPed& rPed, const CPed& rTarget, Vec3V_InOut rPosition)
{
	//Call the search version.
	if(!CDispatchHelperSearch::Calculate(rPed, rTarget, sm_Tunables.m_Constraints, rPosition))
	{
		return false;
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////
// CDispatchHelperVolumes
////////////////////////////////////////////////////////////////////////////////

CDispatchHelperVolumes::Tunables CDispatchHelperVolumes::sm_Tunables;
IMPLEMENT_DISPATCH_HELPERS_TUNABLES(CDispatchHelperVolumes, 0x8aa1e6c7);

Vec3V_Out CDispatchHelperVolumes::Tunables::LocationForNearestCarNodeOverrides::Transform(Vec3V_In vLocation) const
{
	//Iterate over the areas.
	for(int i = 0; i < m_Areas.GetCount(); ++i)
	{
		//Check if the location is in the area.
		const AreaWithPosition& rArea = m_Areas[i];
		if(rArea.m_Area.IsPointInArea(VEC3V_TO_VECTOR3(vLocation)))
		{
			return rArea.m_Position.ToVec3V();
		}
	}

	return vLocation;
}

#if __BANK
void CDispatchHelperVolumes::Tunables::LocationForNearestCarNodeOverrides::RenderDebug()
{
	//Iterate over the areas.
	for(int i = 0; i < m_Areas.GetCount(); ++i)
	{
		//Draw the area and position.
		const AreaWithPosition& rArea = m_Areas[i];
		rArea.m_Area.IsPointInArea(VEC3_ZERO, true);
		grcDebugDraw::Sphere(rArea.m_Position.ToVector3(), 1.0f, Color_red);
	}
}
#endif

const CDispatchHelperVolumes::Tunables::EnclosedSearchRegion* CDispatchHelperVolumes::Tunables::EnclosedSearchRegions::Get(const CPed& rTarget) const
{
	return Get(rTarget, rTarget.GetTransform().GetPosition());
}

const CDispatchHelperVolumes::Tunables::EnclosedSearchRegion* CDispatchHelperVolumes::Tunables::EnclosedSearchRegions::Get(const CPed& rTarget, Vec3V_In vPosition) const
{
	//Check if the interior is valid.
	const EnclosedSearchRegion* pRegion = GetInterior(rTarget);
	if(pRegion)
	{
		return pRegion;
	}

	//Iterate over the areas.
	for(int i = 0; i < m_Areas.GetCount(); ++i)
	{
		//Ensure the position is in the area.
		const EnclosedSearchRegion& rArea = m_Areas[i];
		if(!rArea.ContainsPosition(vPosition))
		{
			continue;
		}

		return &rArea;
	}

	return NULL;
}

const CDispatchHelperVolumes::Tunables::EnclosedSearchRegion* CDispatchHelperVolumes::Tunables::EnclosedSearchRegions::GetInterior(const CPed& rTarget) const
{
	//Ensure the target is in an interior.
	if(!rTarget.GetIsInInterior())
	{
		return NULL;
	}

	//Ensure the interior is valid.
	const CInteriorInst* pInterior = rTarget.GetPortalTracker()->GetInteriorInst();
	if(!pInterior)
	{
		return NULL;
	}

	//Create a region.
	static EnclosedSearchRegion region;
	region.m_AABB = pInterior->GetBoundBox(region.m_AABB);
	region.m_Area.Reset();
	region.m_uFlags = UseAABB;

	return &region;
}

#if __BANK
void CDispatchHelperVolumes::Tunables::EnclosedSearchRegions::RenderDebug()
{
	//Iterate over the areas.
	for(int i = 0; i < m_Areas.GetCount(); ++i)
	{
		//Draw the area.
		m_Areas[i].m_Area.IsPointInArea(VEC3_ZERO, true);
	}
}
#endif

bool CDispatchHelperVolumes::Tunables::BlockingAreas::Contains(Vec3V_In vPosition) const
{
	//Iterate over the areas.
	for(int i = 0; i < m_Areas.GetCount(); ++i)
	{
		//Ensure the position is in the area.
		if(!m_Areas[i].IsPointInArea(VEC3V_TO_VECTOR3(vPosition)))
		{
			continue;
		}

		return true;
	}

	return false;
}

#if __BANK
void CDispatchHelperVolumes::Tunables::BlockingAreas::RenderDebug()
{
	//Iterate over the areas.
	for(int i = 0; i < m_Areas.GetCount(); ++i)
	{
		//Draw the area.
		m_Areas[i].IsPointInArea(VEC3_ZERO, true);
	}
}
#endif

#if __BANK
void CDispatchHelperVolumes::RenderDebug()
{
	//Ensure rendering is enabled.
	if(!sm_Tunables.m_Rendering.m_Enabled)
	{
		return;
	}

	//Check if rendering for location for nearest car node overrides is enabled.
	if(sm_Tunables.m_Rendering.m_LocationForNearestCarNodeOverrides)
	{
		sm_Tunables.m_LocationForNearestCarNodeOverrides.RenderDebug();
	}

	//Check if rendering for enclosed search regions is enabled.
	if(sm_Tunables.m_Rendering.m_EnclosedSearchRegions)
	{
		sm_Tunables.m_EnclosedSearchRegions.RenderDebug();
	}

	//Check if rendering for blocking areas is enabled.
	if(sm_Tunables.m_Rendering.m_BlockingAreas)
	{
		sm_Tunables.m_BlockingAreas.RenderDebug();
	}
}
#endif

/////////////////////////////////////////////////////////////////////////////////////

bool CDispatchHelperForIncidentLocationOnFoot::IsResultReady(bool& bWasSuccessful, Vec3V_InOut vPosition)
{
	//Check if the search is done.
	SearchStatus nSearchStatus;
	static const int s_iMaxNumPositions = 1;
	Vector3 aPositions[1];
	s32 iNumPositions = 0;
	if(m_NavmeshClosestPositionHelper.GetSearchResults(nSearchStatus, iNumPositions, aPositions, s_iMaxNumPositions))
	{
		//Check if the search was successful.
		if((nSearchStatus == SS_SearchSuccessful) && (iNumPositions > 0))
		{
			//The search was successful.
			bWasSuccessful = true;

			//Set the position.
			vPosition = VECTOR3_TO_VEC3V(aPositions[0]);
		}
		else
		{
			//The search was not successful.
			bWasSuccessful = false;
		}

		return true;
	}
	else
	{
		return false;
	}
}

bool CDispatchHelperForIncidentLocationOnFoot::Start(const CPed& rPed)
{
	//Start the search.
	static dev_float s_fMaxDistance = 40.0f;
	if(!m_NavmeshClosestPositionHelper.StartClosestPositionSearch(VEC3V_TO_VECTOR3(GetPositionForPed(rPed)), s_fMaxDistance,
		CNavmeshClosestPositionHelper::Flag_ConsiderDynamicObjects|CNavmeshClosestPositionHelper::Flag_ConsiderExterior|
		CNavmeshClosestPositionHelper::Flag_ConsiderInterior|CNavmeshClosestPositionHelper::Flag_ConsiderOnlyLand|
		CNavmeshClosestPositionHelper::Flag_ConsiderOnlyNonIsolated))
	{
		return false;
	}

	return true;
}

bool CDispatchHelperForIncidentLocationOnFoot::Calculate(const CPed& rPed, Vec3V_InOut vPosition)
{
	//Ensure the closest position is valid.
	static dev_float s_fMaxDistance = 40.0f;
	if(CPathServer::GetClosestPositionForPed(VEC3V_TO_VECTOR3(GetPositionForPed(rPed)), RC_VECTOR3(vPosition), s_fMaxDistance,
		GetClosestPos_OnlyNonIsolated|GetClosestPos_ExtraThorough|GetClosestPos_PreferSameHeight) == ENoPositionFound)
	{
		return false;
	}

	return true;
}

Vec3V_Out CDispatchHelperForIncidentLocationOnFoot::GetPositionForPed(const CPed& rPed)
{
	//Grab the order.
	const COrder* pOrder = rPed.GetPedIntelligence()->GetOrder();
	aiAssert(pOrder && pOrder->GetIsValid());

	//Calculate the max variance.
	static dev_float s_fMaxVarianceInside	= 3.0f;
	static dev_float s_fMaxVarianceOutside	= 10.0f;
	float fMaxVariance = (rPed.GetIsInInterior() ?
		s_fMaxVarianceInside : s_fMaxVarianceOutside);

	//Generate a random location.
	Vec3V vPosition(V_ZERO);
	vPosition.SetXf(pOrder->GetIncident()->GetLocation().x + fwRandom::GetRandomNumberInRange(-fMaxVariance, fMaxVariance));
	vPosition.SetYf(pOrder->GetIncident()->GetLocation().y + fwRandom::GetRandomNumberInRange(-fMaxVariance, fMaxVariance));
	vPosition.SetZf(pOrder->GetIncident()->GetLocation().z);

	return vPosition;
}

/////////////////////////////////////////////////////////////////////////////////////

float CDispatchHelperForIncidentLocationInVehicle::GetCruiseSpeed(const CPed& rPed)
{
	static dev_float s_fCruiseSpeed = 15.0f;
	static dev_float s_fArmyCruiseSpeed = 25.0f;
	if(rPed.GetMyVehicle()->GetVehicleType() == VEHICLE_TYPE_BOAT)
	{
		return 40.0f;
	}
	return rPed.IsArmyPed() ? s_fArmyCruiseSpeed : s_fCruiseSpeed;
}

fwFlags32 CDispatchHelperForIncidentLocationInVehicle::GetDrivingFlags()
{
	static fwFlags32 s_uFlags = (DMode_AvoidCars_Reckless | DF_SteerAroundPeds | DF_AdjustCruiseSpeedBasedOnRoadSpeed | DF_PreventJoinInRoadDirectionWhenMoving);
	return s_uFlags;
}

float CDispatchHelperForIncidentLocationInVehicle::GetTargetArriveDist()
{
	static dev_float s_fTargetArriveDist = 15.0f;
	return s_fTargetArriveDist;
}

/////////////////////////////////////////////////////////////////////////////////////

Vec3V_Out CDispatchHelperPedPositionToUse::Get(const CPed& rPed)
{
	//Check if we should use the last nav mesh intersesction.
	bool bUseLastNavMeshIntersection = (rPed.GetPedResetFlag(CPED_RESET_FLAG_IsClimbing));
	if(bUseLastNavMeshIntersection)
	{
		//Check if the last nav mesh intersection is valid.
		Vec3V vPosition;
		if(CLastNavMeshIntersectionHelper::Get(rPed, vPosition))
		{
			return vPosition;
		}
	}

	return (rPed.GetTransform().GetPosition());
}
