// Title	:	RoadBlock.cpp
// Started	:	1/20/2012

// File header
#include "Game/Dispatch/RoadBlock.h"

// Rage headers
#include "fwmaths/random.h"
#include "fwsys/timer.h"

// Game headers
#include "camera/CamInterface.h"
#include "control/replay/replay.h"
#include "Cutscene/CutSceneManagerNew.h"
#include "modelinfo/VehicleModelInfo.h"
#include "network/Objects/NetworkObjectPopulationMgr.h"
#include "Objects/objectpopulation.h"
#include "Peds/ped.h"
#include "Peds/PedFactory.h"
#include "Peds/PedIntelligence.h"
#include "scene/world/GameWorld.h"
#include "Task/Animation/TaskAnims.h"
#include "vehicleAi/pathfind.h"
#include "vehicles/Automobile.h"
#include "Vehicles/vehiclepopulation.h"

AI_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// CRoadBlock
//////////////////////////////////////////////////////////////////////////

#define LARGEST_ROAD_BLOCK_CLASS sizeof(CRoadBlockSpikeStrip)
FW_INSTANTIATE_BASECLASS_POOL(CRoadBlock, CONFIGURED_FROM_FILE, atHashString("CRoadBlock",0xf72cc701), LARGEST_ROAD_BLOCK_CLASS);

CompileTimeAssert(sizeof(CRoadBlock)			<= LARGEST_ROAD_BLOCK_CLASS);
CompileTimeAssert(sizeof(CRoadBlockVehicles)	<= LARGEST_ROAD_BLOCK_CLASS);
CompileTimeAssert(sizeof(CRoadBlockSpikeStrip)	<= LARGEST_ROAD_BLOCK_CLASS);

float CRoadBlock::ms_fTimeSinceLastDespawnCheck = 0.0f;

namespace
{
	const float MIN_DISTANCE_TO_DESPAWN_SQ	= square(100.0f);
	const float ROTATION					= PI * 0.125f;
	const float SLIDE						= 1.0f;
	const float TIME_BETWEEN_DESPAWN_CHECKS	= 3.0f;
}

CRoadBlock::CRoadBlock(const CPed* pTarget, float fMinTimeToLive)
: m_aVehs()
, m_aPeds()
, m_aObjs()
, m_vPosition(V_ZERO)
, m_pTarget(pTarget)
, m_fMinTimeToLive(fMinTimeToLive)
, m_fTotalTime(0.0f)
, m_fTimeSinceLastStrayEntitiesUpdate(0.0f)
, m_uConfigFlags(0)
, m_uRunningFlags(0)
, m_fInitialDistToTarget(0.0f)
, m_fTimeOutOfRange(0.0f)
{

}

CRoadBlock::~CRoadBlock()
{

}

bool CRoadBlock::Despawn(bool bForce)
{
	//Ensure the road block can despawn.
	if(!bForce && !CanDespawn())
	{
		return false;
	}
	
	//Try to despawn the road block.
	return TryToDespawn();
}

bool CRoadBlock::HasEntity(const CEntity& rEntity) const
{
	for(s32 i = 0; i < m_aVehs.GetCount(); ++i)
	{
		if(m_aVehs[i] == &rEntity)
		{
			return true;
		}
	}

	for(s32 i = 0; i < m_aPeds.GetCount(); ++i)
	{
		if(m_aPeds[i] == &rEntity)
		{
			return true;
		}
	}

	for(s32 i = 0; i < m_aObjs.GetCount(); ++i)
	{
		if(m_aObjs[i] == &rEntity)
		{
			return true;
		}
	}

	return false;
}

bool CRoadBlock::IsEmpty() const
{
	//Ensure the vehicles are empty.
	if(!m_aVehs.IsEmpty())
	{
		return false;
	}

	//Ensure the peds are empty.
	if(!m_aPeds.IsEmpty())
	{
		return false;
	}

	//Ensure the objects are empty.
	if(!m_aObjs.IsEmpty())
	{
		return false;
	}

	return true;
}

bool CRoadBlock::RemoveVeh(CVehicle& rVehicle)
{
	//Iterate over the vehicles.
	for(s32 i = 0; i < m_aVehs.GetCount(); ++i)
	{
		//Ensure the vehicle matches.
		CVehicle* pVeh = m_aVehs[i];
		if(pVeh != &rVehicle)
		{
			continue;
		}

		//Release the vehicle.
		ReleaseVeh(*pVeh);

		//Remove the vehicle.
		m_aVehs.Delete(i--);
		
		return true;
	}

	return false;
}

void CRoadBlock::SetPersistentOwner(bool bPersistentOwner)
{
	//Ensure a network game is in progress.
	if(!NetworkInterface::IsGameInProgress())
	{
		return;
	}

	//Note: Vehicles used to be marked as persistent owner, but no longer.  They are now blocked from migrating
	//		via CanPassControl.  This was added to allow migration exceptions (such as remote peds entering the
	//		vehicle).
	
	//Mark the peds with the persistent owner flag.
	for(s32 i = 0; i < m_aPeds.GetCount(); ++i)
	{
		//Ensure the ped is valid.
		CPed* pPed = m_aPeds[i];
		if(!pPed)
		{
			continue;
		}

		//Get the network object.
		netObject* pNetObject = pPed->GetNetworkObject();
		if(!pNetObject)
		{
			continue;
		}

		//Set the persistent owner flag.
		pNetObject->SetGlobalFlag(netObject::GLOBALFLAG_PERSISTENTOWNER, bPersistentOwner);
	}
	
	//Mark the objects with the persistent owner flag.
	for(s32 i = 0; i < m_aObjs.GetCount(); ++i)
	{
		//Ensure the object is valid.
		CObject* pObj = m_aObjs[i];
		if(!pObj)
		{
			continue;
		}

		//Get the network object.
		netObject* pNetObject = pObj->GetNetworkObject();
		if(!pNetObject)
		{
			continue;
		}

		//Set the persistent owner flag.
		pNetObject->SetGlobalFlag(netObject::GLOBALFLAG_PERSISTENTOWNER, bPersistentOwner);
	}
}

CRoadBlock* CRoadBlock::CreateFromType(const CreateFromTypeInput& rTypeInput, const CreateInput& rInput)
{
	//Generate the pre-create output.
	PreCreateOutput preOutput;
	if(!PreCreate(rInput, preOutput))
	{
		return NULL;
	}
	
	//Ensure there is room to spawn another road block.
	if(!aiVerifyf(CRoadBlock::GetPool()->GetNoOfFreeSpaces() != 0, "No free space in the pool to create a new road block."))
	{
		return NULL;
	}

	//Create the road block based on the type.
	CRoadBlock* pRoadBlock = NULL;
	switch(rTypeInput.m_nType)
	{
		case RBT_Vehicles:
		{
			pRoadBlock = rage_new CRoadBlockVehicles(rTypeInput.m_pTarget, rTypeInput.m_fMinTimeToLive);
			break;
		}
		case RBT_SpikeStrip:
		{
			pRoadBlock = rage_new CRoadBlockSpikeStrip(rTypeInput.m_pTarget, rTypeInput.m_fMinTimeToLive);
			break;
		}
		default:
		{
			Assertf(false, "Unknown road block type: %d.", rTypeInput.m_nType);
			return NULL;
		}
	}
	
	//Set the position.
	pRoadBlock->SetPosition(Average(VECTOR3_TO_VEC3V(preOutput.m_vLeftCurb), VECTOR3_TO_VEC3V(preOutput.m_vRightCurb)));
	
	//Create the road block.
	if(pRoadBlock->Create(rInput, preOutput))
	{
		//Do not allow anything in the roadblock to migrate.
		pRoadBlock->SetPersistentOwner(true);
		
		return pRoadBlock;
	}
	else
	{
		//Despawn the road block.
		aiVerifyf(pRoadBlock->Despawn(true), "Unable to despawn road block.");

		//Free the road block.
		delete pRoadBlock;

		return NULL;
	}
}

CRoadBlock* CRoadBlock::Find(const CEntity& rEntity)
{
	//Grab the pool.
	CRoadBlock::Pool* pPool = CRoadBlock::GetPool();

	//Iterate over the road blocks.
	s32 iCount = pPool->GetSize();
	for(s32 i = 0; i < iCount; ++i)
	{
		//Grab the road block.
		CRoadBlock* pRoadBlock = pPool->GetSlot(i);
		if(!pRoadBlock)
		{
			continue;
		}

		//Check if the road block has the entity.
		if(pRoadBlock->HasEntity(rEntity))
		{
			return pRoadBlock;
		}
	}

	return NULL;
}

void CRoadBlock::OnRespawn(const CPed& rTarget)
{
	//Grab the pool.
	CRoadBlock::Pool* pPool = CRoadBlock::GetPool();

	//Iterate over the road blocks.
	s32 iCount = pPool->GetSize();
	for(s32 i = 0; i < iCount; ++i)
	{
		//Grab the road block.
		CRoadBlock* pRoadBlock = pPool->GetSlot(i);
		if(!pRoadBlock)
		{
			continue;
		}

		//Ensure the target matches.
		if(pRoadBlock->m_pTarget != &rTarget)
		{
			continue;
		}

		//Check if we are in SP.
		if(!NetworkInterface::IsGameInProgress())
		{
			//Try to despawn the road block.
			if(!pRoadBlock->Despawn(true))
			{
				continue;
			}

			//Free the road block.
			delete pRoadBlock;
		}
		else
		{
			//Ensure the road block has not dispersed.
			if(pRoadBlock->HasDispersed())
			{
				continue;
			}

			//Disperse the road block.
			pRoadBlock->Disperse();
		}
	}
}

bool CRoadBlock::PreCreate(const CreateInput& rInput, PreCreateOutput& rOutput)
{
	//Ensure there is room in the pool.
	if(CRoadBlock::GetPool()->GetNoOfFreeSpaces() == 0)
	{
		return false;
	}
	
	//Calculate the at/towards nodes.
	CNodeAddress atNode;
	CNodeAddress towardsNode;
	if(!rInput.m_AtNode.IsEmpty() && !rInput.m_TowardsNode.IsEmpty())
	{
		//Assign the nodes.
		atNode		= rInput.m_AtNode;
		towardsNode	= rInput.m_TowardsNode;
	}
	else if(rInput.m_vPosition.IsNonZero() && rInput.m_vDirection.IsNonZero())
	{
		//Ensure the closest road node to the position is valid.
		atNode = ThePaths.FindNodeClosestToCoors(rInput.m_vPosition);
		if(atNode.IsEmpty())
		{
			return false;
		}
		
		//Ensure the closest node in the direction is valid.
		CPathFind::FindNodeClosestToNodeFavorDirectionInput input(atNode, rInput.m_vDirection);
		CPathFind::FindNodeClosestToNodeFavorDirectionOutput output;
		if(!ThePaths.FindNodeClosestToNodeFavorDirection(input, output))
		{
			return false;
		}
		
		//Assign the towards node.
		towardsNode = output.m_Node;
	}
	else
	{
		aiAssertf(false, "No input node or position/direction combinations defined for road block.");
		return false;
	}

	//Ensure the node is valid.
	const CPathNode* pNode = ThePaths.FindNodePointerSafe(atNode);
	if(!pNode)
	{
		return false;
	}

	//Ensure the node has no shortcut links.  This generally means we won't be able to block the whole road.
	if(pNode->HasShortcutLinks())
	{
		return false;
	}

	//Ensure the towards node is valid.
	const CPathNode* pTowardsNode = ThePaths.FindNodePointerSafe(towardsNode);
	if(!pTowardsNode)
	{
		return false;
	}

	//Generate the coordinates of the node.
	Vector3 vNodePosition;
	pNode->GetCoors(vNodePosition);

	//Generate the coordinates of the towards node.
	Vector3 vTowardsNodePosition;
	pTowardsNode->GetCoors(vTowardsNodePosition);

	//Generate the direction.
	Vector3 vDirection = vTowardsNodePosition - vNodePosition;
	if(!vDirection.NormalizeSafeRet())
	{
		return false;
	}

	//Find the link index between the nodes.
	s16 iLink = -1;
	if(!ThePaths.FindNodesLinkIndexBetween2Nodes(pNode->m_address, pTowardsNode->m_address, iLink))
	{
		return false;
	}

	//Get the link.
	const CPathNodeLink* pLink = &(ThePaths.GetNodesLink(pNode, iLink));
	if(!pLink)
	{
		return false;
	}

	//Find the road boundaries.
	float fRoadWidthOnLeft;
	float fRoadWidthOnRight;
	ThePaths.FindRoadBoundaries(*pLink, fRoadWidthOnLeft, fRoadWidthOnRight);

	//Calculate the left curb position.
	Vector3 vLeftCurb = vNodePosition;
	vLeftCurb.x += vDirection.y * -fRoadWidthOnLeft;
	vLeftCurb.y -= vDirection.x * -fRoadWidthOnLeft;

	//Calculate the right curb position.
	Vector3 vRightCurb = vNodePosition;
	vRightCurb.x += vDirection.y * fRoadWidthOnRight;
	vRightCurb.y -= vDirection.x * fRoadWidthOnRight;

	//Generate a vector from the left curb to the right curb.
	Vector3 vLeftCurbToRightCurb = vRightCurb - vLeftCurb;

	//Generate the direction from the left curb to the right curb.
	Vector3 vDirectionLeftCurbToRightCurb = vLeftCurbToRightCurb;
	if(!vDirectionLeftCurbToRightCurb.NormalizeSafeRet())
	{
		return false;
	}

	//Generate the front direction.
	Vector3 vDirectionFront;
	vDirectionFront.x = -vDirectionLeftCurbToRightCurb.y;
	vDirectionFront.y = vDirectionLeftCurbToRightCurb.x;
	vDirectionFront.z = vDirectionLeftCurbToRightCurb.z;

	//Generate the back direction.
	Vector3 vDirectionBack = -vDirectionFront;

	//Assign all of the pre-calculated values to the output.
	rOutput.m_vLeftCurb = vLeftCurb;
	rOutput.m_vRightCurb = vRightCurb;
	rOutput.m_vLeftCurbToRightCurb = vLeftCurbToRightCurb;
	rOutput.m_vDirectionLeftCurbToRightCurb = vDirectionLeftCurbToRightCurb;
	rOutput.m_vDirectionFront = vDirectionFront;
	rOutput.m_vDirectionBack = vDirectionBack;

	return true;
}

void CRoadBlock::Init(unsigned UNUSED_PARAM(initMode))
{
	//Init the pool.
	CRoadBlock::InitPool(MEMBUCKET_GAMEPLAY);
}

void CRoadBlock::Shutdown(unsigned UNUSED_PARAM(initMode))
{
	//Shutdown the pool.
	CRoadBlock::ShutdownPool();
}

void CRoadBlock::Update()
{
#if GTA_REPLAY
	if(CReplayMgr::IsEditModeActive())
		return;
#endif

	//This is common between the population managers.
	if((CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->IsRunning()) || fwTimer::IsGamePaused())
	{
		return;
	}
	
	//Grab the pool.
	CRoadBlock::Pool* pPool = CRoadBlock::GetPool();

	//Grab the time step.
	const float fTimeStep = fwTimer::GetTimeStep();
	
	//Decrease the despawn timer.
	bool bCheckForDespawn = false;
	ms_fTimeSinceLastDespawnCheck += fTimeStep;
	if(ms_fTimeSinceLastDespawnCheck > TIME_BETWEEN_DESPAWN_CHECKS)
	{
		bCheckForDespawn = true;
		ms_fTimeSinceLastDespawnCheck = 0.0f;
	}

	//Update the road blocks.
	s32 iCount = pPool->GetSize();
	for(s32 i = 0; i < iCount; ++i)
	{
		//Grab the road block.
		CRoadBlock* pRoadBlock = pPool->GetSlot(i);
		if(!pRoadBlock)
		{
			continue;
		}

		//Update the road block.
		pRoadBlock->Update(fTimeStep);

		//Check if the road block is not empty.
		if(!pRoadBlock->IsEmpty())
		{
			//Try to despawn the road block.
			if(!bCheckForDespawn || !pRoadBlock->Despawn())
			{
				continue;
			}
		}
		
		//Free the road block.
		delete pRoadBlock;
	}
}

bool CRoadBlock::CalculateDimensionsForModel(fwModelId iModelId, float& fWidth, float& fLength, float& fHeight) const
{
	//Ensure the model is is valid.
	if(!iModelId.IsValid())
	{
		return false;
	}

	//Ensure the base model info is valid.
	CBaseModelInfo* pBaseModelInfo = CModelInfo::GetBaseModelInfo(iModelId);
	if(!pBaseModelInfo)
	{
		return false;
	}

	//Calculate the model dimensions.
	fWidth	= (pBaseModelInfo->GetBoundingBoxMax().x - pBaseModelInfo->GetBoundingBoxMin().x);
	fLength	= (pBaseModelInfo->GetBoundingBoxMax().y - pBaseModelInfo->GetBoundingBoxMin().y);
	fHeight	= (pBaseModelInfo->GetBoundingBoxMax().z - pBaseModelInfo->GetBoundingBoxMin().z);

	return true;
}

fwModelId CRoadBlock::ChooseRandomModel(const atArray<fwModelId>& aModelIds) const
{
	//Ensure the model ids are valid.
	int iCount = aModelIds.GetCount();
	if(iCount <= 0)
	{
		return fwModelId();
	}
	
	//Choose a random model.
	return aModelIds[fwRandom::GetRandomNumberInRange(0, iCount)];
}

fwModelId CRoadBlock::ChooseRandomModelForSpace(const atArray<fwModelId>& aModelIds, float fSpace, float& fWidth, float& fLength, float& fHeight) const
{
	//Ensure the model ids are valid.
	int iCount = aModelIds.GetCount();
	if(iCount <= 0)
	{
		return fwModelId();
	}
	
	//Choose a random model that fits in the space.
	SemiShuffledSequence shuffler(iCount);
	for(int i = 0; i < iCount; ++i)
	{
		//Get the index.
		int iIndex = shuffler.GetElement(i);

		//Calculate the model dimensions.
		fwModelId iModelId = aModelIds[iIndex];
		if(!CalculateDimensionsForModel(iModelId, fWidth, fLength, fHeight))
		{
			continue;
		}
		
		//Check if the model can fit width-wise.
		if(fWidth <= fSpace)
		{
			return iModelId;
		}
		
		//Check if the model can fit length-wise.
		if(fLength <= fSpace)
		{
			return iModelId;
		}
	}
	
	return fwModelId();
}

CObject* CRoadBlock::CreateObj(const CreateObjInput& rInput)
{
	//Ensure there is room to create the object.
	if(m_aObjs.IsFull())
	{
		return NULL;
	}
	
	// query the network population manager to make sure this object can be created
	if (NetworkInterface::IsGameInProgress() && !CNetworkObjectPopulationMgr::CanRegisterObjects(0, 0, 1 ,0, 0, false))
	{
		return NULL;
	}

	//Ensure the transform is valid.
	Matrix34 mTransform;
	if(!CreateTransform(rInput, mTransform))
	{
		return NULL;
	}
	
	//Create the input.
	CObjectPopulation::CreateObjectInput input(rInput.m_iModelId, ENTITY_OWNEDBY_GAME, true);
	input.m_bForceClone = true;

	//Create the object.
	CObject* pObject = CObjectPopulation::CreateObject(input);
	if(!pObject)
	{
		return NULL;
	}

	//Set the matrix.
	pObject->SetMatrix(mTransform);

	//Place the object on the ground properly.
	pObject->PlaceOnGroundProperly();

	//Add the object to the world.
	CGameWorld::Add(pObject, CGameWorld::OUTSIDE);

	//Check if we should scan for an interior.
	if(rInput.m_bScanForInterior)
	{
		pObject->GetPortalTracker()->ScanUntilProbeTrue();
	}

	//Add the object.
	m_aObjs.Append().Reset(pObject);

	return pObject;
}

CPed* CRoadBlock::CreatePed(const CreatePedInput& rInput)
{
	//Ensure there is room to create the ped.
	if(m_aPeds.IsFull())
	{
		return NULL;
	}
	
	// query the network population manager to make sure this ped can be created
	if (NetworkInterface::IsGameInProgress() && !CNetworkObjectPopulationMgr::CanRegisterObjects(1, 0, 0 ,0, 0, false))
	{
		return NULL;
	}

	//Ensure the transform is valid.
	Matrix34 mTransform;
	if(!CreateTransform(rInput, mTransform))
	{
		return NULL;
	}

	//Set up the control type.
	const CControlledByInfo localAiControl(false, false);

	//Generate the ped with the matrix.
	CPed* pPed = CPedFactory::GetFactory()->CreatePed(localAiControl, rInput.m_iModelId, &mTransform, true, true, false);
	if(!pPed)
	{
		return NULL;
	}

	//Set up the ped properties.
	pPed->SetPedType(PEDTYPE_COP);
	pPed->PopTypeSet(POPTYPE_RANDOM_PERMANENT);
	pPed->SetDefaultDecisionMaker();
	pPed->SetCharParamsBasedOnManagerType();
	pPed->GetPedIntelligence()->SetDefaultRelationshipGroup();
	pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_ChangeFromPermanentToAmbientPopTypeOnMigration, true);
	pPed->SetVarRandom(PV_FLAG_NONE, PV_FLAG_NONE, NULL, NULL, NULL, PV_RACE_UNIVERSAL);

	//Add the ped to the world.
	CGameWorld::Add(pPed, CGameWorld::OUTSIDE);

	//Check if we should scan for an interior.
	if(rInput.m_bScanForInterior)
	{
		pPed->GetPortalTracker()->ScanUntilProbeTrue();
	}
	
	//If ped vehicle is spawned without collision around it, fix it.
	pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_ShouldFixIfNoCollision, true);
	pPed->m_nPhysicalFlags.bAllowFreezeIfNoCollision = true;

	//Add the ped.
	m_aPeds.Append().Reset(pPed);

	return pPed;
}

bool CRoadBlock::CreateTransform(const CreateTransformInput& rInput, Matrix34& mTransform) const
{
	//Ensure the base model info is valid.
	CBaseModelInfo* pBaseModelInfo = CModelInfo::GetBaseModelInfo(rInput.m_iModelId);
	if(!pBaseModelInfo)
	{
		return false;
	}

	//Calculate the up vector.
	Vector3 vUp = ZAXIS;

	//Calculate the side vector.
	Vector3 vSide;
	vSide.Cross(rInput.m_vForward, vUp);
	if(!vSide.NormalizeSafeRet())
	{
		return false;
	}

	//Re-calculate the up vector.
	//No need to normalize since the other two unit vectors
	//are guaranteed to be 90 degrees apart.
	vUp.Cross(vSide, rInput.m_vForward);

	//Adjust the position.
	//The incoming position is on the ground.
	//Move the position up a bit based on the origin.
	Vector3 vAdjustedPosition = rInput.m_vPosition;
	vAdjustedPosition.z -= pBaseModelInfo->GetBoundingBoxMin().z;

	//Generate the transform.
	mTransform.a = vSide;
	mTransform.b = rInput.m_vForward;
	mTransform.c = vUp;
	mTransform.d = vAdjustedPosition;
	
	return true;
}

CVehicle* CRoadBlock::CreateVeh(const CreateVehInput& rInput)
{
	//Ensure there is room to create the vehicle.
	if(m_aVehs.IsFull())
	{
		return NULL;
	}
	
	// query the network population manager to make sure this vehicle can be created
	if (NetworkInterface::IsGameInProgress() && !CNetworkObjectPopulationMgr::CanRegisterObjects(0, 1, 0 ,0, 0, false))
	{
		return NULL;
	}

	//Ensure the transform is valid.
	Matrix34 mTransform;
	if(!CreateTransform(rInput, mTransform))
	{
		return NULL;
	}

	//Generate the vehicle.
	CVehiclePopulation::GenerateOneEmergencyServicesVehicleWithMatrixInput input(mTransform, rInput.m_iModelId);
	input.m_bUseBoundingBoxesForCollision = true;
	input.m_pExceptionForCollision = rInput.m_pExceptionForCollision;
	input.m_nEntityOwnedBy = ENTITY_OWNEDBY_GAME;
	input.m_nPopType = POPTYPE_RANDOM_PERMANENT;
	input.m_bTryToClearAreaIfBlocked = true;
	input.m_bSwitchEngineOn = false;
	input.m_bKickStartVelocity = false;
	input.m_bPlaceOnRoadProperlyCausesFailure = false;
	input.m_bGiveDefaultTask = false;
	CVehicle* pVehicle = CVehiclePopulation::GenerateOneEmergencyServicesVehicleWithMatrix(input);
	if(!pVehicle)
	{
		return NULL;
	}

	//Check if we should scan for an interior.
	if(rInput.m_bScanForInterior)
	{
		pVehicle->GetPortalTracker()->ScanUntilProbeTrue();
	}
	
	//If this vehicle is spawned without collision around it, fix it.
	pVehicle->m_nVehicleFlags.bShouldFixIfNoCollision = true;
	pVehicle->m_nPhysicalFlags.bAllowFreezeIfNoCollision = true;

	//Note that this vehicle is in a road block.
	pVehicle->m_nVehicleFlags.bIsInRoadBlock = true;

	//Turn on the siren, but mute the audio.
	pVehicle->TurnSirenOn(true, false);
	pVehicle->m_nVehicleFlags.SetSirenMutedByCode(true);

	//Add the vehicle.
	m_aVehs.Append().Reset(pVehicle);

	return pVehicle;
}

void CRoadBlock::Update(float fTimeStep)
{
	//Update the timers.
	UpdateTimers(fTimeStep);

	//Check if the road block should disperse.
	if(ShouldDisperse())
	{
		//Disperse the road block.
		Disperse();
	}

	//Check if we should update the stray entities.
	if(ShouldUpdateStrayEntities())
	{
		//Update the stray entities.
		UpdateStrayEntities();
	}
	
	//Update force other vehicles to stop.
	UpdateForceOtherVehiclesToStop();
}

bool CRoadBlock::CanDespawn() const
{
	//If there are no entities, we can "despawn".
	if(GetEntitiesCount() == 0)
	{
		return true;
	}
	
	//Ensure the min time to live has been exceeded.
	if(m_fTotalTime < m_fMinTimeToLive)
	{
		return false;
	}
	
	//If the target has gone away, we can despawn.
	const CPed* pTarget = m_pTarget;
	if(!pTarget)
	{
		return true;
	}

	//If target has decoy active, then we despawn.
	if (NetworkInterface::IsGameInProgress() && m_pTarget->IsPlayer() && pTarget->GetPedConfigFlag(CPED_CONFIG_FLAG_HasEstablishedDecoy))
	{
		return true;
	}
	
	//Ensure the target is far away.
	Vec3V vTargetPosition = pTarget->GetTransform().GetPosition();
	ScalarV scDistSq = DistSquared(m_vPosition, vTargetPosition);
	ScalarV scMinDistSq = ScalarVFromF32(MIN_DISTANCE_TO_DESPAWN_SQ);
	if(IsLessThanAll(scDistSq, scMinDistSq))
	{
		return false;
	}
	
	//Ensure we can despawn the vehicles.
	if(!CanDespawnVehs())
	{
		return false;
	}

	//Ensure we can despawn the peds.
	if(!CanDespawnPeds())
	{
		return false;
	}

	//Ensure we can despawn the objects.
	if(!CanDespawnObjs())
	{
		return false;
	}
	
	return true;
}

bool CRoadBlock::CanDespawnObjs() const
{
	//Ensure the objects can be despawned.
	for(s32 i = 0; i < m_aObjs.GetCount(); ++i)
	{
		//Ensure the object is valid.
		CObject* pObj = m_aObjs[i];
		if(!pObj)
		{
			continue;
		}
		
		//If the object is on-screen, we cannot despawn it.
		if(pObj->GetIsOnScreen(true))
		{
			return false;
		}
	}
	
	return true;
}

bool CRoadBlock::CanDespawnPeds() const
{
	//Ensure the peds can be despawned.
	for(s32 i = 0; i < m_aPeds.GetCount(); ++i)
	{
		//Ensure the ped is valid.
		CPed* pPed = m_aPeds[i];
		if(!pPed)
		{
			continue;
		}

		//If the ped is on-screen, we cannot despawn it.
		if(pPed->GetIsOnScreen(true))
		{
			return false;
		}
	}

	return true;
}

bool CRoadBlock::CanDespawnVehs() const
{
	//Ensure the vehicles can be despawned.
	for(s32 i = 0; i < m_aVehs.GetCount(); ++i)
	{
		//Ensure the vehicle is valid.
		CVehicle* pVeh = m_aVehs[i];
		if(!pVeh)
		{
			continue;
		}

		//If the vehicle is on-screen, we cannot despawn it.
		if(pVeh->GetIsOnScreen(true))
		{
			return false;
		}
	}

	return true;
}

void CRoadBlock::Disperse()
{
	//Assert that we have not dispersed.
	aiAssertf(!m_uRunningFlags.IsFlagSet(RF_HasDispersed), "The road block has already dispersed.");

	//Disperse the vehicles.
	DisperseVehs();

	//Disperse the peds.
	DispersePeds();

	//Disperse the objects.
	DisperseObjs();

	//Set the flag.
	m_uRunningFlags.SetFlag(RF_HasDispersed);
}

void CRoadBlock::DisperseObjs()
{
	//Release the objects.
	ReleaseObjs();
}

void CRoadBlock::DispersePeds()
{
	//Release the peds.
	ReleasePeds();
}

void CRoadBlock::DisperseVehs()
{
	//Make the vehicles drive away.
	MakeVehsDriveAway();

	//Release the vehicles.
	ReleaseVehs();
}

bool CRoadBlock::HasEntityStrayed(const CEntity& rEntity) const
{
	//Grab the entity position.
	Vec3V vEntityPosition = rEntity.GetTransform().GetPosition();

	//Ensure the distance has exceeded the threshold.
	ScalarV scDistSq = DistSquared(m_vPosition, vEntityPosition);
	ScalarV scMaxDistSq = ScalarVFromF32(square(20.0f));
	if(IsLessThanAll(scDistSq, scMaxDistSq))
	{
		return false;
	}

	return true;
}

void CRoadBlock::MakeVehsDriveAway()
{
	//Iterate over the vehicles.
	for(s32 i = 0; i < m_aVehs.GetCount(); ++i)
	{
		//Ensure the vehicle is valid.
		CVehicle* pVeh = m_aVehs[i];
		if(!pVeh)
		{
			continue;
		}

		//Don't allow vehicles with a player driver to drive away.
		if(pVeh->GetDriver() && pVeh->GetDriver()->IsPlayer())
		{
			continue;
		}

		//Ensure this vehicle will not be driven away.
		if(WillVehBeDrivenAway(*pVeh))
		{
			continue;
		}

		//Set the vehicle up with pretend occupants.
		//This should also give the vehicle a default task, so it will drive away.
		pVeh->SetUpWithPretendOccupants();

		//Turn off the siren.
		pVeh->TurnSirenOn(false, false);
	}
}

void CRoadBlock::ReleaseObj(CObject& rObj) const
{
	//Clear the persistent owner.
	if(rObj.GetNetworkObject())
	{
		rObj.GetNetworkObject()->SetGlobalFlag(netObject::GLOBALFLAG_PERSISTENTOWNER, false);
	}

	//Set the object as owned by the population.
	rObj.SetOwnedBy(ENTITY_OWNEDBY_POPULATION);
}

void CRoadBlock::ReleaseObjs()
{
	//Iterate over the objects.
	for(s32 i = 0; i < m_aObjs.GetCount(); ++i)
	{
		//Ensure the object is valid.
		CObject* pObj = m_aObjs[i];
		if(!pObj)
		{
			continue;
		}

		//Release the object.
		ReleaseObj(*pObj);
	}

	//Reset the objects.
	m_aObjs.Reset();
}

void CRoadBlock::ReleasePed(CPed& rPed) const
{
	//Clear the persistent owner.
	if(rPed.GetNetworkObject())
	{
		rPed.GetNetworkObject()->SetGlobalFlag(netObject::GLOBALFLAG_PERSISTENTOWNER, false);
	}

	//Set the ped as part of the random, ambient population.
	rPed.PopTypeSet(POPTYPE_RANDOM_AMBIENT);
}

void CRoadBlock::ReleasePeds()
{
	//Iterate over the peds.
	for(s32 i = 0; i < m_aPeds.GetCount(); ++i)
	{
		//Ensure the ped is valid.
		CPed* pPed = m_aPeds[i];
		if(!pPed)
		{
			continue;
		}

		//Release the ped.
		ReleasePed(*pPed);
	}

	//Reset the peds.
	m_aPeds.Reset();
}

void CRoadBlock::ReleaseVeh(CVehicle& rVeh) const
{
	//Note that the vehicle is no longer in a road block.
	rVeh.m_nVehicleFlags.bIsInRoadBlock = false;

	//Do not mute the siren audio.
	rVeh.m_nVehicleFlags.SetSirenMutedByCode(false);

	//Set the vehicle as owned by the population.
	rVeh.SetOwnedBy(ENTITY_OWNEDBY_POPULATION);

	//Set the vehicle as part of the random, ambient population.
	rVeh.PopTypeSet(POPTYPE_RANDOM_AMBIENT);

	//Sync the pop type when the vehicle migrates.
	rVeh.m_nVehicleFlags.bSyncPopTypeOnMigrate = true;
}

void CRoadBlock::ReleaseVehs()
{
	//Iterate over the vehicles.
	for(s32 i = 0; i < m_aVehs.GetCount(); ++i)
	{
		//Ensure the vehicle is valid.
		CVehicle* pVeh = m_aVehs[i];
		if(!pVeh)
		{
			continue;
		}

		//Release the vehicle.
		ReleaseVeh(*pVeh);
	}

	//Reset the vehicles.
	m_aVehs.Reset();
}

bool CRoadBlock::ShouldDisperse() const
{
	//Ensure the flag is not set.
	if(m_uRunningFlags.IsFlagSet(RF_HasDispersed))
	{
		return false;
	}

	//Check if the target is invalid.
	if(!m_pTarget)
	{
		return true;
	}

	if(NetworkInterface::IsGameInProgress())
	{
		if(m_pTarget->IsDead() || m_pTarget->IsInjured())
		{
			return true;
		}

		static float s_fOutofRangeTimeToDisperse = 7.5f;
		if(m_fTimeOutOfRange > s_fOutofRangeTimeToDisperse)
		{
			return true;
		}
		
		//! Auto disperse if we go beyond reasonable sync distance.
		Vector3 vDist = VEC3V_TO_VECTOR3(m_pTarget->GetTransform().GetPosition()) - VEC3V_TO_VECTOR3(m_vPosition);
		if(vDist.Mag2() > rage::square(m_fInitialDistToTarget * 1.2f) )
		{
			return true;
		}
	}

	//Check if the flag is set.
	if(m_uConfigFlags.IsFlagSet(CF_DisperseIfTargetIsNotWanted))
	{
		//Check if the target does not have a wanted structure.
		const CWanted* pWanted = m_pTarget->GetPlayerWanted();
		if(!pWanted)
		{
			return true;
		}

		//Check if the player is not wanted.
		if(pWanted->GetWantedLevel() == WANTED_CLEAN)
		{
			return true;
		}
	}

	return false;
}

bool CRoadBlock::ShouldUpdateStrayEntities() const
{
	//Ensure the time has exceeded the threshold.
	static float s_fTimeBetweenStrayEntitiesUpdates = 1.0f;
	if(m_fTimeSinceLastStrayEntitiesUpdate < s_fTimeBetweenStrayEntitiesUpdates)
	{
		return false;
	}

	return true;
}

bool CRoadBlock::TryToDespawn()
{
	//Try to despawn the vehicles.
	if(!TryToDespawnVehs())
	{
		return false;
	}
	
	//Try to despawn the peds.
	if(!TryToDespawnPeds())
	{
		return false;
	}
	
	//Try to despawn the objects.
	if(!TryToDespawnObjs())
	{
		return false;
	}
	
	return true;
}

bool CRoadBlock::TryToDespawnObjs()
{
	//Ensure the objects can be despawned.
	for(s32 i = 0; i < m_aObjs.GetCount(); ++i)
	{
		//Ensure the object is valid.
		CObject* pObj = m_aObjs[i];
		if(!pObj)
		{
			continue;
		}

		//Release the object.
		ReleaseObj(*pObj);
		
		//Despawn the object.
		CObjectPopulation::DestroyObject(pObj);
	}
	
	return true;
}

bool CRoadBlock::TryToDespawnPeds()
{
	//Ensure the peds can be despawned.
	for(s32 i = 0; i < m_aPeds.GetCount(); ++i)
	{
		//Ensure the ped is valid.
		CPed* pPed = m_aPeds[i];
		if(!pPed)
		{
			continue;
		}

		//Release the ped.
		ReleasePed(*pPed);
		
		//Despawn the ped.
		CPedFactory::GetFactory()->DestroyPed(pPed);
	}
	
	return true;
}

bool CRoadBlock::TryToDespawnVehs()
{
	//Despawn the vehicles.
	for(s32 i = 0; i < m_aVehs.GetCount(); ++i)
	{
		//Ensure the vehicle is valid.
		CVehicle* pVeh = m_aVehs[i];
		if(!pVeh)
		{
			continue;
		}

		//Release the vehicle.
		ReleaseVeh(*pVeh);
		
		//Despawn the vehicle.
		CVehiclePopulation::RemoveVeh(pVeh, true, CVehiclePopulation::Removal_None);
	}
	
	return true;
}

void CRoadBlock::UpdateForceOtherVehiclesToStop()
{
	//Iterate over the vehicles.
	for(s32 i = 0; i < m_aVehs.GetCount(); ++i)
	{
		//Ensure the vehicle is valid.
		CVehicle* pVeh = m_aVehs[i];
		if(!pVeh)
		{
			continue;
		}
		
		//Ensure the vehicle has no driver.
		if(pVeh->GetDriver())
		{
			continue;
		}
		
		//Force other vehicles to stop for this vehicle.
		pVeh->m_nVehicleFlags.bForceOtherVehiclesToStopForThisVehicle = true;
	}
}

void CRoadBlock::UpdateStrayEntities()
{
	//Update the stray vehicles.
	UpdateStrayVehs();

	//Update the stray peds.
	UpdateStrayPeds();

	//Update the stray objects.
	UpdateStrayObjs();

	//Clear the time since we last updated the stray entities.
	m_fTimeSinceLastStrayEntitiesUpdate = 0.0f;
}

void CRoadBlock::UpdateStrayObjs()
{
	//Iterate over the objs.
	for(s32 i = 0; i < m_aObjs.GetCount(); ++i)
	{
		//Ensure the object is valid.
		CObject* pObj = m_aObjs[i];
		if(!pObj)
		{
			continue;
		}

		//Ensure the object has strayed from the road block.
		if(!HasEntityStrayed(*pObj))
		{
			continue;
		}

		//Release the object.
		ReleaseObj(*pObj);

		//Remove the object.
		m_aObjs.Delete(i--);
	}
}

void CRoadBlock::UpdateStrayPeds()
{
	//Iterate over the peds.
	for(s32 i = 0; i < m_aPeds.GetCount(); ++i)
	{
		//Ensure the ped is valid.
		CPed* pPed = m_aPeds[i];
		if(!pPed)
		{
			continue;
		}

		//Ensure the ped has strayed from the road block.
		if(!HasEntityStrayed(*pPed))
		{
			continue;
		}

		//Release the ped.
		ReleasePed(*pPed);

		//Remove the ped.
		m_aPeds.Delete(i--);
	}
}

void CRoadBlock::UpdateStrayVehs()
{
	//Iterate over the vehicles.
	for(s32 i = 0; i < m_aVehs.GetCount(); ++i)
	{
		//Ensure the vehicle is valid.
		CVehicle* pVeh = m_aVehs[i];
		if(!pVeh)
		{
			continue;
		}

		//Ensure the vehicle has strayed from the road block.
		if(!HasEntityStrayed(*pVeh))
		{
			continue;
		}

		//Release the vehicle.
		ReleaseVeh(*pVeh);

		//Remove the vehicle.
		m_aVehs.Delete(i--);
	}
}

void CRoadBlock::UpdateTimers(float fTimeStep)
{
	//Add to the total time.
	m_fTotalTime += fTimeStep;

	//Add to the time since the stray entities were last updated.
	m_fTimeSinceLastStrayEntitiesUpdate += fTimeStep;

	if(m_pTarget)
	{
		bool bOutsideOfRange = false;
		Vector3 vDist = VEC3V_TO_VECTOR3(m_pTarget->GetTransform().GetPosition()) - VEC3V_TO_VECTOR3(m_vPosition);
		float fDistSq = vDist.Mag2();
		if(fDistSq > square(m_fInitialDistToTarget)  )
		{
			bOutsideOfRange = true;
		}

		//! If owner is outside of 100m, and a remote player is closer by at least 25m, then we allow road block to disperse after a certain amount of time
		static dev_float s_fRemotePlayerRangeDist = 100.0f;
		static dev_float s_fRemoteToLocalPlayerDist = 25.0f;
		if(!bOutsideOfRange && (fDistSq > rage::square(s_fRemotePlayerRangeDist)) )
		{
			unsigned numRemotePhysicalPlayers = netInterface::GetNumRemotePhysicalPlayers();
			const netPlayer * const *remotePhysicalPlayers = netInterface::GetRemotePhysicalPlayers();

			for(unsigned index = 0; index < numRemotePhysicalPlayers; index++)
			{
				const CNetGamePlayer *remotePlayer = SafeCast(const CNetGamePlayer, remotePhysicalPlayers[index]);
				if(remotePlayer && remotePlayer->GetPlayerPed())
				{
					Vector3 vDiffRemote = VEC3V_TO_VECTOR3(remotePlayer->GetPlayerPed()->GetTransform().GetPosition() - m_vPosition);
					float fDistRemoteSq = vDiffRemote.Mag2();

					Vector3 vDiffRemoteToTarget = VEC3V_TO_VECTOR3(remotePlayer->GetPlayerPed()->GetTransform().GetPosition() - m_pTarget->GetTransform().GetPosition());
					float fDistRemoteToTargetSq = vDiffRemoteToTarget.Mag2();

					if( (fDistRemoteSq < fDistSq) && //closer than owner.
						(fDistRemoteToTargetSq > rage::square(s_fRemoteToLocalPlayerDist)) ) //further away from owner by at least 25m.
					{
						bOutsideOfRange = true;
					}
				}
			}
		}

		if(bOutsideOfRange)
		{
			m_fTimeOutOfRange += fTimeStep;
		}
		else
		{
			m_fTimeOutOfRange = 0.0f;
		}
	}
}

bool CRoadBlock::WillVehBeDrivenAway(const CVehicle& rVehicle) const
{
	//Iterate over the peds.
	for(s32 i = 0; i < m_aPeds.GetCount(); ++i)
	{
		//Ensure the ped is valid.
		CPed* pPed = m_aPeds[i];
		if(!pPed)
		{
			continue;
		}

		//Ensure the ped is alive.
		if(pPed->IsInjured())
		{
			continue;
		}

		//Ensure the ped's vehicle matches.
		if(pPed->GetMyVehicle() != &rVehicle)
		{
			continue;
		}

		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
// CRoadBlockVehicles
//////////////////////////////////////////////////////////////////////////

CRoadBlockVehicles::CRoadBlockVehicles(const CPed* pTarget, float fMinTimeToLive)
: CRoadBlock(pTarget, fMinTimeToLive)
{

}

CRoadBlockVehicles::~CRoadBlockVehicles()
{

}

bool CRoadBlockVehicles::Create(const CreateInput& rInput, const PreCreateOutput& rPreOutput)
{
	//Initialize the position.
	Vector3 vPosition = rPreOutput.m_vLeftCurb;

	//Initialize the remaining space.
	float fRemainingSpace = rPreOutput.m_vLeftCurb.Dist(rPreOutput.m_vRightCurb);
	
	//Initialize the last vehicle created.
	CVehicle* pLastVehicleCreated = NULL;
	
	//Initialize the object offset.
	float fObjOffset = 0.0f;

	//Initialize the flags.
	bool bHasCreatedVehicleFacingAway = false;

	// maximum unfilled space on the road
	static dev_float s_fMaxRemainingSpace = 1.5f;

	//Check if we should scan for an interior.
	const CPathNode* pAtNode = ThePaths.FindNodePointerSafe(rInput.m_AtNode);
	bool bScanForInterior = (pAtNode ? pAtNode->m_2.m_inTunnel : false);

	//Generate the vehicles and peds.
	for(u32 i = 0; i < rInput.m_uMaxVehs; ++i)
	{
		//Choose a random vehicle model, based on the remaining space.
		float fVehWidth;
		float fVehLength;
		float fVehHeight;
		fwModelId iVehModelId = ChooseRandomModelForSpace(rInput.m_aVehModelIds, fRemainingSpace, fVehWidth, fVehLength, fVehHeight);
		if(!iVehModelId.IsValid())
		{
			//Clear the remaining space.
			fRemainingSpace = 0.0f;

			//No vehicle can fit in the remaining space.
			break;
		}

		// check to see if the space is too wide for our max cars to fill - we don't want to create any more cars if we're going to fail anyway
		if(((fVehLength * (rInput.m_uMaxVehs-i)) + (rInput.m_fVehSpacing * (rInput.m_uMaxVehs-i))) < (fRemainingSpace-s_fMaxRemainingSpace))
		{
			// we're going to fail to fill the space required, abort early			
			return false;
		}

		
		//Keep track of the space to be used for the vehicle.
		float fVehSpace = 0.0f;
		
		//Keep track of the vehicle's forward vector.
		Vector3 vVehicleForward = VEC3_ZERO;
		
		//Keep track of the rotation multiplier.
		float fRotationMultiplier = 0.0f;

		//Check if we should create this vehicle facing away.
		static dev_float s_fChancesToCreateVehicleFacingAway = 0.35f;
		bool bCreateVehicleFacingAway = !bHasCreatedVehicleFacingAway && (i > 0) &&
			(fwRandom::GetRandomNumberInRange(0.0f, 1.0f) < s_fChancesToCreateVehicleFacingAway);
		bHasCreatedVehicleFacingAway |= bCreateVehicleFacingAway;
		
		//Prefer placing the vehicles length-wise.
		if((fVehLength <= fRemainingSpace) && !bCreateVehicleFacingAway)
		{
			//Assign the vehicle space.
			fVehSpace = fVehLength;
			
			//Assign the forward vector.
			vVehicleForward = rPreOutput.m_vDirectionLeftCurbToRightCurb;
			
			//Assign the rotation multiplier.
			fRotationMultiplier = 1.0f;
		}
		//There is not enough room to place the vehicle length-wise, but there is enough room width-wise.
		else
		{
			//Use the remaining space as the vehicle space.
			//This will place the vehicle in the center of the remaining space.
			fVehSpace = fRemainingSpace;

			//Assign the forward vector.
			//This will place the vehicle width-wise.
			vVehicleForward = !bCreateVehicleFacingAway ? rPreOutput.m_vDirectionFront : rPreOutput.m_vDirectionBack;
			
			//Assign the rotation multiplier.
			//If the remaining space is equal to the width, the rotation multiplier should be 0.
			//If the remaining space is equal to the length, the rotation multiplier should be 1.
			float fRange = fVehLength - fVehWidth;
			if((fRange > FLT_EPSILON) && !bCreateVehicleFacingAway)
			{
				fRotationMultiplier = Clamp((fRemainingSpace - fVehWidth) / fRange, 0.0f, 1.0f);
			}
			else
			{
				fRotationMultiplier = 0.0f;
			}
		}
		
		//Calculate the half vehicle space.
		float fHalfVehSpace = fVehSpace * 0.5f;
		
		//Advance the position.
		vPosition += (rPreOutput.m_vDirectionLeftCurbToRightCurb * fHalfVehSpace);
		fRemainingSpace -= fHalfVehSpace;
		
		//Calculate the vehicle position.
		Vector3 vVehiclePosition = vPosition;

		//Calculate half the vehicle space plus the vehicle spacing.
		float fHalfVehSpacePlusSpacing = fHalfVehSpace + rInput.m_fVehSpacing;

		//Advance the position.
		vPosition += rPreOutput.m_vDirectionLeftCurbToRightCurb * fHalfVehSpacePlusSpacing;
		fRemainingSpace -= fHalfVehSpacePlusSpacing;

		//Generate the rotation.
		float fRotation = (fwRandom::GetRandomNumberInRange(-ROTATION, ROTATION) * fRotationMultiplier) + (fwRandom::GetRandomTrueFalse() ? PI : 0.0f);

		//Rotate the vehicle.
		vVehicleForward.RotateZ(fRotation);

		//Generate the slide.
		float fSlide = fwRandom::GetRandomNumberInRange(-SLIDE, SLIDE);

		//Slide the vehicle.
		vVehiclePosition.x += (rPreOutput.m_vDirectionFront.x * fSlide);
		vVehiclePosition.y += (rPreOutput.m_vDirectionFront.y * fSlide);

		//Create the vehicle.
		CreateVehInput vInput(vVehiclePosition, vVehicleForward, iVehModelId);
		vInput.m_pExceptionForCollision = pLastVehicleCreated;
		vInput.m_bScanForInterior = bScanForInterior;
		pLastVehicleCreated = CreateVeh(vInput);
		if(!pLastVehicleCreated)
		{
			//Something went wrong, just abort.
			return false;
		}

		//Calculate the vehicle XY space.
		float fVehSpaceXY = Max(fVehWidth, fVehLength);
		float fHalfVehSpaceXY = fVehSpaceXY * 0.5f;

		//Choose a random ped model.
		fwModelId iPedModelId = ChooseRandomModel(rInput.m_aPedModelIds);
		if(!iPedModelId.IsValid())
		{
			//Something went wrong, just abort.
			return false;
		}

		//Calculate the model dimensions for the ped.
		float fPedWidth = 0.0f;
		float fPedLength = 0.0f;
		float fPedHeight = 0.0f;
		if(!CalculateDimensionsForModel(iPedModelId, fPedWidth, fPedLength, fPedHeight))
		{
			//Something went wrong, just abort.
			return false;
		}

		//Calculate the ped offset.
		float fPedOffset = fHalfVehSpaceXY + rInput.m_fPedSpacing;

		//Calculate the ped forward vector.
		Vector3 vPedForward = rPreOutput.m_vDirectionFront;
		vPedForward.z = 0.0f;
		if(!vPedForward.NormalizeSafeRet())
		{
			//Something went wrong, just abort.
			return false;
		}

		//Check if the ped should be placed in front of the vehicle.
		bool bPlacePedInFront = (fPedHeight <= fVehHeight);

		//Calculate the ped position.
		Vector3 vPedPosition = vVehiclePosition + ((!bPlacePedInFront ? rPreOutput.m_vDirectionBack : rPreOutput.m_vDirectionFront) * fPedOffset);

		//Create the ped.
		CreatePedInput pInput(vPedPosition, vPedForward, iPedModelId);
		pInput.m_bScanForInterior = bScanForInterior;
		CPed* pPed = CreatePed(pInput);
		if(!pPed)
		{
			//Something went wrong, just abort.
			return false;
		}
		
		//Set the ped's vehicle.
		pPed->SetMyVehicle(pLastVehicleCreated);

		//Calculate this object offset.
		float fThisObjectOffset = fSlide + fHalfVehSpaceXY + (bPlacePedInFront ? fPedOffset : 0.0f) + rInput.m_fObjSpacing;

		//Calculate the object offset.
		fObjOffset = Max(fObjOffset, fThisObjectOffset);
	}
	
	//Ensure at least one vehicle was created.
	u32 uVehs = GetVehCount();
	if(uVehs == 0)
	{
		return false;
	}

	//Ensure there is not too much space remaining.	
	if(fRemainingSpace > s_fMaxRemainingSpace)
	{
		return false;
	}
	
	//Choose a random number of objects to spawn.
	u32 uObjs = 1 + fwRandom::GetRandomNumberInRange(0, uVehs + 1);

	//Spawn the objects.
	for(int i = 0; i < uObjs; ++i)
	{
		//Choose a random point on the line between the left/right curbs.
		Vector3 vPos = rPreOutput.m_vLeftCurb + (rPreOutput.m_vLeftCurbToRightCurb * fwRandom::GetRandomNumberInRange(0.0f, 1.0f));

		//Move the object in front of the vehicles.
		vPos += (rPreOutput.m_vDirectionFront * fObjOffset);
		
		//Choose a random object model.
		fwModelId iObjModelId = rInput.m_aObjModelIds[fwRandom::GetRandomNumberInRange(0, rInput.m_aObjModelIds.GetCount())];

		//Create the object.
		CreateObjInput oInput(vPos, rPreOutput.m_vDirectionFront, iObjModelId);
		oInput.m_bScanForInterior = bScanForInterior;
		if(!CreateObj(oInput))
		{
			//Something went wrong, just abort.
			return false;
		}
	}

	if(m_pTarget)
	{
		Vector3 vDistToTarget = VEC3V_TO_VECTOR3(m_pTarget->GetTransform().GetPosition()) - VEC3V_TO_VECTOR3(m_vPosition);
		m_fInitialDistToTarget = vDistToTarget.Mag();
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
// CRoadBlockSpikeStrip
//////////////////////////////////////////////////////////////////////////

CRoadBlockSpikeStrip::CRoadBlockSpikeStrip(const CPed* pTarget, float fMinTimeToLive)
: CRoadBlock(pTarget, fMinTimeToLive)
, m_bPopTires(false)
{

}

CRoadBlockSpikeStrip::~CRoadBlockSpikeStrip()
{

}

bool CRoadBlockSpikeStrip::ShouldPopTires() const
{
	//This was added to decrease the likelihood of road blocks very far away from the target
	//causing tires on cop cars to pop.  We decided that the spike strips should still pop
	//the tires of cop cars close to the player, so this seemed the best way to enforce that.

	//Ensure the target is valid.
	const CPed* pPed = m_pTarget;
	if(!pPed)
	{
		return false;
	}
	
	//Grab the ped position.
	Vec3V vPedPosition = pPed->GetTransform().GetPosition();

	//Calculate the distance.
	ScalarV scDistSq = DistSquared(vPedPosition, m_vPosition);

	//Check if we are within the distance to always pop tires.
	static float s_fDistanceToAlwaysPopTires = 25.0f;
	ScalarV scDistanceToAlwaysPopTiresSq = ScalarVFromF32(square(s_fDistanceToAlwaysPopTires));
	if(IsLessThanAll(scDistSq, scDistanceToAlwaysPopTiresSq))
	{
		return true;
	}

	//Check if we are within the distance to pop tires when on-screen.
	static float s_fDistanceToPopTiresWhenOnScreen = 50.0f;
	ScalarV scDistanceToPopTiresWhenOnScreenSq = ScalarVFromF32(square(s_fDistanceToPopTiresWhenOnScreen));
	if(IsLessThanAll(scDistSq, scDistanceToPopTiresWhenOnScreenSq))
	{
		//Check if the position is on the player's screen.
		static float s_fRadius = 5.0f;
		if(camInterface::IsSphereVisibleInGameViewport(VEC3V_TO_VECTOR3(m_vPosition), s_fRadius))
		{
			return true;
		}

		//Check if the position is on a network player's screen.
		if(NetworkInterface::IsGameInProgress() && NetworkInterface::IsVisibleToAnyRemotePlayer(VEC3V_TO_VECTOR3(m_vPosition), s_fRadius, s_fDistanceToPopTiresWhenOnScreen))
		{
			return true;
		}
	}

	return false;
}

bool CRoadBlockSpikeStrip::Create(const CreateInput& rInput, const PreCreateOutput& rPreOutput)
{
	//Choose a random vehicle model.
	fwModelId iVehModelId = ChooseRandomModel(rInput.m_aVehModelIds);
	if(!iVehModelId.IsValid())
	{
		//Something went wrong, just abort.
		return false;
	}

	//Calculate the model dimensions for the vehicle.
	float fVehWidth = 0.0f;
	float fVehLength = 0.0f;
	float fVehHeight = 0.0f;
	if(!CalculateDimensionsForModel(iVehModelId, fVehWidth, fVehLength, fVehHeight))
	{
		//Something went wrong, just abort.
		return false;
	}

	//Check if we should scan for an interior.
	const CPathNode* pAtNode = ThePaths.FindNodePointerSafe(rInput.m_AtNode);
	bool bScanForInterior = (pAtNode ? pAtNode->m_2.m_inTunnel : false);

	//Calculate the half vehicle length.
	float fHalfVehLength = fVehLength * 0.5f;

	//Initialize the position.
	Vector3 vPosition = rPreOutput.m_vLeftCurb;
	
	//Advance the position.
	Vector3 vHalfVehOffset = (rPreOutput.m_vDirectionLeftCurbToRightCurb * fHalfVehLength);
	vPosition += vHalfVehOffset;

	//Calculate the vehicle forward vector.
	Vector3 vVehicleForward = rPreOutput.m_vDirectionLeftCurbToRightCurb;

	//Generate the rotation.
	float fRotation = fwRandom::GetRandomNumberInRange(-ROTATION, ROTATION) + (fwRandom::GetRandomTrueFalse() ? PI : 0.0f);

	//Rotate the vehicle.
	vVehicleForward.RotateZ(fRotation);

	//Calculate the vehicle position.
	Vector3 vVehiclePosition = vPosition;

	//Create the vehicle.
	CreateVehInput vInput(vVehiclePosition, vVehicleForward, iVehModelId);
	vInput.m_bScanForInterior = bScanForInterior;
	CVehicle* pVehicle = CreateVeh(vInput);
	if(!pVehicle)
	{
		//Something went wrong, just abort.
		return false;
	}

	//Calculate the vehicle XY space.
	float fVehSpaceXY = Max(fVehWidth, fVehLength);
	float fHalfVehSpaceXY = fVehSpaceXY * 0.5f;

	//Choose a random ped model.
	fwModelId iPedModelId = ChooseRandomModel(rInput.m_aPedModelIds);
	if(!iPedModelId.IsValid())
	{
		//Something went wrong, just abort.
		return false;
	}

	//Calculate the model dimensions for the ped.
	float fPedWidth = 0.0f;
	float fPedLength = 0.0f;
	float fPedHeight = 0.0f;
	if(!CalculateDimensionsForModel(iPedModelId, fPedWidth, fPedLength, fPedHeight))
	{
		//Something went wrong, just abort.
		return false;
	}

	//Calculate the ped offset.
	float fPedOffset = fHalfVehSpaceXY + rInput.m_fPedSpacing;

	//Calculate the ped forward vector.
	Vector3 vPedForward = rPreOutput.m_vDirectionFront;
	vPedForward.z = 0.0f;
	if(!vPedForward.NormalizeSafeRet())
	{
		//Something went wrong, just abort.
		return false;
	}

	//Check if the ped should be placed in front of the vehicle.
	bool bPlacePedInFront = (fPedHeight <= fVehHeight);

	//Calculate the ped position.
	Vector3 vPedPosition = vVehiclePosition + ((!bPlacePedInFront ? rPreOutput.m_vDirectionBack : rPreOutput.m_vDirectionFront) * fPedOffset);

	//Create the ped.
	CreatePedInput pInput(vPedPosition, vPedForward, iPedModelId);
	pInput.m_bScanForInterior = bScanForInterior;
	CPed* pPed = CreatePed(pInput);
	if(!pPed)
	{
		//Something went wrong, just abort.
		return false;
	}
	
	//Set the ped's vehicle.
	pPed->SetMyVehicle(pVehicle);

	//Advance the position to the end of the vehicle.
	vPosition += vHalfVehOffset;
	
	//Spike strips should only have one model.
	fwModelId iObjModelId = rInput.m_aObjModelIds[0];
	
	//Ensure the model info is valid.
	CBaseModelInfo* pObjModelInfo = CModelInfo::GetBaseModelInfo(iObjModelId);
	if(!pObjModelInfo)
	{
		//Something went wrong, just abort.
		return false;
	}
	
	//Calculate the object length.
	float fObjLen = (pObjModelInfo->GetBoundingBoxMax().y - pObjModelInfo->GetBoundingBoxMin().y);

	//Calculate the half object length.
	float fHalfObjLen = fObjLen * 0.5f;
	
	//Initialize the remaining space.
	float fRemainingSpace = vPosition.Dist(rPreOutput.m_vRightCurb);

	//Create the objects.
	for(u32 i = 0; i < sMaxObjs; ++i)
	{
		//Ensure there is enough remaining space.
		//Allow the objects to creep up on the sides of the road ~1m.
		if(fRemainingSpace + 1.0f < fObjLen)
		{
			//The space has been filled, do not create any more objects.
			break;
		}

		//Advance the position.
		vPosition += (rPreOutput.m_vDirectionLeftCurbToRightCurb * fHalfObjLen);
		fRemainingSpace -= fHalfObjLen;
		
		//Calculate the object forward vector.
		Vector3 vObjectForward = rPreOutput.m_vDirectionLeftCurbToRightCurb;

		//Calculate the object position.
		Vector3 vObjectPosition = vPosition;
		
		//Calculate half the object length plus the object spacing.
		float fHalfObjLenPlusSpacing = fHalfObjLen + rInput.m_fObjSpacing;

		//Advance the position.
		vPosition += (rPreOutput.m_vDirectionLeftCurbToRightCurb * fHalfObjLenPlusSpacing);
		fRemainingSpace -= fHalfObjLenPlusSpacing;

		//Create the object.
		CreateObjInput oInput(vObjectPosition, vObjectForward, iObjModelId);
		oInput.m_bScanForInterior = bScanForInterior;
		CObject* pObject = CreateObj(oInput);
		if(!pObject)
		{
			//Something went wrong, just abort.
			return false;
		}

		//Fix the spike strips in place.
		pObject->SetBaseFlag(fwEntity::IS_FIXED);
	}

	if(m_pTarget)
	{
		Vector3 vDistToTarget = VEC3V_TO_VECTOR3(m_pTarget->GetTransform().GetPosition()) - VEC3V_TO_VECTOR3(m_vPosition);
		m_fInitialDistToTarget = vDistToTarget.Mag();
	}

	return true;
}

void CRoadBlockSpikeStrip::Update(float fTimeStep)
{
	//Call the base class version.
	CRoadBlock::Update(fTimeStep);
	
	//Check if the road block should pop tires.
	bool bShouldPopTires = ShouldPopTires();
	
	//Ensure the value is changing.
	if(m_bPopTires == bShouldPopTires)
	{
		return;
	}
	
	//Assign the value.
	m_bPopTires = bShouldPopTires;
	
	//Iterate over the spike strips.
	for(s32 i = 0; i < m_aObjs.GetCount(); ++i)
	{
		//Ensure the object is valid.
		CObject* pObj = m_aObjs[i];
		if(!pObj)
		{
			continue;
		}
		
		//Set the pop tires flag.
		pObj->m_nObjectFlags.bPopTires = m_bPopTires;
	}
}
