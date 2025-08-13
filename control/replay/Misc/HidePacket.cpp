#include "HidePacket.h"

#if GTA_REPLAY

#include "vectormath/vec3v.h"

#include "scene/building.h"
#include "scene/world/GameWorld.h"
#include "vfx/misc/LODLightManager.h"				// for hash

//====================================================================================
//====================================================================================

// Currently unused, left incase it may be required later
u32 CutsceneNonRPObjectHide::GetModelAndPositionHash(CEntity *pEntity)
{
	u32	hashArray[4];	// Modelnamehash, x, y, z
	hashArray[0] = pEntity->GetBaseModelInfo()->GetModelNameHash();
	s32	*pVecStore = (s32*)&hashArray[1];
	Vec3V	pos = pEntity->GetTransform().GetPosition();
	pos = pos * ScalarV(10.0f);
	pVecStore[0] = static_cast<s32>(pos.GetXf());
	pVecStore[1] = static_cast<s32>(pos.GetYf());
	pVecStore[2] = static_cast<s32>(pos.GetZf());
	return CLODLightManager::hash(hashArray, 4, 0);
}

//====================================================================================
//====================================================================================


void	CPacketCutsceneNonRPObjectHide::Extract(const CEventInfo<void>& eventInfo) const
{
	Vector3 searchPosition;
	m_CutsceneHide.searchPos.Load(searchPosition);
	CEntity *pEntity = FindEntity(m_CutsceneHide.modelNameHash, m_CutsceneHide.searchRadius, searchPosition);
	if(pEntity)
	{
		if(eventInfo.GetPlaybackFlags().GetState() & REPLAY_DIRECTION_FWD)
		{
			// Playing forwards
			pEntity->SetIsVisibleForModule(SETISVISIBLE_MODULE_REPLAY, !m_CutsceneHide.shouldHide);
		}
		else
		{
			// Playing backwards
			pEntity->SetIsVisibleForModule(SETISVISIBLE_MODULE_REPLAY, m_CutsceneHide.shouldHide);
		}
	}
}

CEntity *CPacketCutsceneNonRPObjectHide::FindEntity(const u32 modelNameHash, float searchRadius, Vector3 &searchPos)
{
	ReplayClosestObjectToHideSearchStruct findStruct;

	findStruct.pEntity = NULL;
	findStruct.closestDistanceSquared = searchRadius * searchRadius;	//	remember it's squared distance
	findStruct.coordToCalculateDistanceFrom = searchPos;
	findStruct.modelNameHashToCheck = modelNameHash;

	//	Check range
	spdSphere testSphere(RCC_VEC3V(	findStruct.coordToCalculateDistanceFrom), ScalarV(searchRadius));
	fwIsSphereIntersecting searchSphere(testSphere);
	CGameWorld::ForAllEntitiesIntersecting(&searchSphere, GetClosestObjectCB, (void*) &findStruct, (ENTITY_TYPE_MASK_BUILDING|ENTITY_TYPE_MASK_DUMMY_OBJECT|ENTITY_TYPE_MASK_OBJECT), (SEARCH_LOCATION_EXTERIORS|SEARCH_LOCATION_INTERIORS));

	return findStruct.pEntity;
}

bool CPacketCutsceneNonRPObjectHide::GetClosestObjectCB(CEntity* pEntity, void* data)
{
	ReplayClosestObjectToHideSearchStruct *pInfo = reinterpret_cast<ReplayClosestObjectToHideSearchStruct*>(data);

	if(pEntity && pEntity->GetOwnedBy() != ENTITY_OWNEDBY_REPLAY)
	{
		CBaseModelInfo *pModelInfo = pEntity->GetBaseModelInfo();
		if(pModelInfo)
		{
			if( pModelInfo->GetModelNameHash() == pInfo->modelNameHashToCheck )
			{
				// It's the right model
				//Get the distance between the centre of the locate and the entities position
				Vector3 DiffVector = VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition()) - pInfo->coordToCalculateDistanceFrom;
				float ObjDistanceSquared = DiffVector.Mag2();

				//If the the distance is less than the radius store this as our entity
				if (ObjDistanceSquared < pInfo->closestDistanceSquared)
				{
					pInfo->pEntity = pEntity;
					pInfo->closestDistanceSquared = ObjDistanceSquared;
				}
			}
		}
	}
	return true;
}

#endif	//GTA_REPLAY