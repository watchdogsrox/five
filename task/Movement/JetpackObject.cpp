#include "Task/Movement/JetpackObject.h"
#include "entity/archetypemanager.h"
#include "ModelInfo/PedModelInfo.h"
#include "ai/task/taskchannel.h"
#include "objects/objectpopulation.h"
#include "scene/physical.h"
#include "Objects/object.h"
#include "Task/Default/TaskPlayer.h"
#include "Task/Movement/TaskJetpack.h"
#include "vfx/ptfx/ptfxmanager.h"
#include "pickups/PickupManager.h"
#include "Task/Default/TaskPlayer.h"

AI_OPTIMISATIONS()

#if ENABLE_JETPACK

FW_INSTANTIATE_BASECLASS_POOL(CJetpack, CJetpackManager::nMaxJetpacks, atHashString("CJetpack",0x7c8b7f98), sizeof(CJetpack));

void CJetpackManager::Init(unsigned /*initMode*/)
{
	CJetpack::InitPool(MEMBUCKET_GAMEPLAY);
}

void CJetpackManager::Shutdown(unsigned /*shutdownMode*/)
{
	CJetpack::Pool *pJetpackPool = CJetpack::GetPool();

	CJetpack* pJetpack;
	s32 i = pJetpackPool->GetSize();

	while(i--)
	{
		pJetpack = pJetpackPool->GetSlot(i);

		if(pJetpack)
		{
			delete pJetpack;
		}
	}

	CJetpack::ShutdownPool();
}

CJetpack * CJetpackManager::CreateJetpackContainer()
{
	return rage_new CJetpack();
}

void CJetpackManager::CreateJetpackPickup(const Matrix34 &initialTransform, CPed *pDropPed)
{
	CPickup *pPickup = CPickupManager::CreatePickup(PICKUP_JETPACK, initialTransform, NULL, true);
	if(pPickup)
	{
		Vec3V vInheritedVelocity(CTaskPlayerOnFoot::sm_Tunables.m_JetpackData.m_VelocityInheritance.m_X,
			CTaskPlayerOnFoot::sm_Tunables.m_JetpackData.m_VelocityInheritance.m_Y,
			CTaskPlayerOnFoot::sm_Tunables.m_JetpackData.m_VelocityInheritance.m_Z);

		//Transform the velocity into world coordinates.  Note that I'm using the ped
		//transform and not the prop, since the prop transform is funky in some cases.
		Vec3V vVelocity = pDropPed->GetTransform().Transform3x3(vInheritedVelocity);

		//Include the ped's velocity.
		Vector3 vNewVelocity = pDropPed->GetVelocity() + VEC3V_TO_VECTOR3(vVelocity);

		//Set the velocity.
		pPickup->SetVelocity(vNewVelocity);

		//Don't collide with drop ped until clear.
		pPickup->SetNoCollision(pDropPed, NO_COLLISION_RESET_WHEN_NO_IMPACTS);
	}
}

CJetpack::CJetpack()
: m_pJetpackObject(NULL)
, m_iJetpackModelIndex(fwModelId::MI_INVALID)
{
}

CJetpack::~CJetpack()
{
	DestroyObject();
}

bool CJetpack::Request(u32 jetpackName)
{
	//Check if the jetpack model index is invalid.
	strLocalIndex iJetpackModelIndex = strLocalIndex(fwModelId::MI_INVALID);

	fwModelId iModelId;
	CModelInfo::GetBaseModelInfoFromHashKey(jetpackName, &iModelId);
	iJetpackModelIndex = iModelId.GetModelIndex();

	if(m_iJetpackModelIndex == fwModelId::MI_INVALID || (iJetpackModelIndex != m_iJetpackModelIndex) )
	{
		//! Release model if we have requested it already.
		if(GetObject())
			DestroyObject();

		m_iJetpackModelIndex = iJetpackModelIndex;
		m_ModelRequestHelper.Release();
	}

	//Ensure the jetpack model is valid.
	if(taskVerifyf(iModelId.IsValid(), "Jetpack model is invalid."))
	{
		//Check if the jetpack model request is not valid.
		if(!m_ModelRequestHelper.IsValid())
		{
			//Stream the jetpack model.
			strLocalIndex transientLocalIdx = CModelInfo::AssignLocalIndexToModelInfo(iModelId);
			m_ModelRequestHelper.Request(transientLocalIdx, CModelInfo::GetStreamingModuleId(), STRFLAG_PRIORITY_LOAD);
		}
	}

	m_HoverClipSetRequestHelper.Request(fwMvClipSetId(CTaskJetpack::sm_Tunables.m_JetpackClipsets.m_HoverClipSetHash));

	//! Only load flight mode anims if it is enabled.
	if(CTaskJetpack::sm_Tunables.m_FlightControlParams.m_bEnabled)
	{
		m_FlightClipSetRequestHelper.Request(fwMvClipSetId(CTaskJetpack::sm_Tunables.m_JetpackClipsets.m_FlightClipSetHash));
	}

	// load particle assets
	strLocalIndex ptfxSlotId = ptfxManager::GetAssetStore().FindSlotFromHashKey(ATSTRINGHASH("gad_clifford", 0x0532c35d));
	if (Verifyf(ptfxSlotId>strLocalIndex(-1), "cannot find particle asset"))
	{
		m_PtFxRequestHelper.Request(ptfxSlotId, ptfxManager::GetAssetStore().GetStreamingModuleId(), STRFLAG_PRIORITY_LOAD);
	}

	return true;
}

bool CJetpack::CreateObject()
{
	//Ensure the model id is valid.
	fwModelId iModelId(m_iJetpackModelIndex);
	if(!taskVerifyf(iModelId.IsValid(), "Invalid jetpack model"))
	{
		return false;
	}

	//Create the input.
	CObjectPopulation::CreateObjectInput input(iModelId, ENTITY_OWNEDBY_GAME, true);
	input.m_bForceClone = true;

	//Create the jetpack object.
	taskAssert(!m_pJetpackObject);
	m_pJetpackObject = CObjectPopulation::CreateObject(input);
	if(!taskVerifyf(m_pJetpackObject, "Failed to create jetpack"))
	{
		return false;
	}

	//Prevent the camera shapetests from detecting this object, as we don't want the camera to pop if it passes near to the camera
	//or collision root position.
	phInst* pPropPhysicsInst = m_pJetpackObject->GetCurrentPhysicsInst();
	if(pPropPhysicsInst)
	{
		phArchetype* pPropArchetype = pPropPhysicsInst->GetArchetype();
		if(pPropArchetype)
		{
			//NOTE: This include flag change will apply to all new instances that use this archetype until it is streamed out.
			//We can live with this for camera purposes, but it's worth noting in case it causes a problem.
			pPropArchetype->RemoveIncludeFlags(ArchetypeFlags::GTA_CAMERA_TEST);
		}
	}

	return true;
}

bool CJetpack::DestroyObject()
{
	// release particle assets
	strLocalIndex slot = ptfxManager::GetAssetStore().FindSlotFromHashKey(atStringHash("gad_clifford"));
	ptfxManager::GetAssetStore().ClearRequiredFlag(slot.Get(), STRFLAG_DONTDELETE);

	m_ModelRequestHelper.Release();
	m_PtFxRequestHelper.Release();

	//Ensure the Jetpack is valid.
	if(!m_pJetpackObject)
	{
		return false;
	}

	m_pJetpackObject->SetBaseFlag(fwEntity::REMOVE_FROM_WORLD);

	return true;
}

bool CJetpack::HasLoaded()
{
	bool bModelLoaded = m_ModelRequestHelper.HasLoaded();

	bool bAnimsLoaded = m_HoverClipSetRequestHelper.Request(fwMvClipSetId(CTaskJetpack::sm_Tunables.m_JetpackClipsets.m_HoverClipSetHash));

	//! Only load flight mode anims if it is enabled.
	if(CTaskJetpack::sm_Tunables.m_FlightControlParams.m_bEnabled)
	{
		if(!m_FlightClipSetRequestHelper.Request(fwMvClipSetId(CTaskJetpack::sm_Tunables.m_JetpackClipsets.m_FlightClipSetHash)))
		{
			bAnimsLoaded = false;
		}
	}

	// load particle assets
	bool bPtFxLoaded = m_PtFxRequestHelper.HasLoaded();

	return bModelLoaded && bAnimsLoaded && bPtFxLoaded;
}

#endif //# ENABLE_JETPACK
