//
// filename:	EntityId.h
// description:	
//

#ifndef INC_ENTITYID_H
#define INC_ENTITYID_H

#include "entity/entity.h"

#include "scene/EntityId_shared.h"
#include "scene/EntityTypes.h"

class CEntityIDHelper
{
public:

	inline static entitySelectID GetInvalidEntityId()			{ return (entitySelectID)ENTITYSELECT_INVALID_ID; }
	inline static bool IsEntityIdValid(entitySelectID id)		{ return GetInvalidEntityId() != id; }

	template <typename T>
	static fwEntity* GetEntityFromPool(T* pPool, u32 index);

	template <typename T>
	static entitySelectID GetEntityIDFromPool(T* pPool, const fwEntity* pEntity);

	static entitySelectID ComputeEntityID(const fwEntity *entity);

	static fwEntity* GetEntityFromID(entitySelectID id);
	static int       GetEntityShaderIndexFromID(entitySelectID id);


#if ENTITYSELECT_BIT_COUNT == 32
	static const entitySelectID ES_ENTITY_TYPE_BITS     = 4;
	static const entitySelectID ES_SHADER_INDEX_BITS    = 5;
	static const entitySelectID ES_POOL_SLOT_BITS       = 23;
#else // ENTITYSELECT_BIT_COUNT == 32
	static const entitySelectID ES_ENTITY_TYPE_BITS     = 5;
	static const entitySelectID ES_SHADER_INDEX_BITS    = 7;
	static const entitySelectID ES_POOL_SLOT_BITS       = 25;
#endif // ENTITYSELECT_BIT_COUNT == 32

	static const entitySelectID ES_ENTITY_TYPE_SHIFT    = ES_POOL_SLOT_BITS + ES_SHADER_INDEX_BITS;
	static const entitySelectID ES_SHADER_INDEX_SHIFT   = ES_POOL_SLOT_BITS;
	static const entitySelectID ES_POOL_SLOT_SHIFT      = 0;

	static const entitySelectID ES_ENTITY_TYPE_MASK     = ((entitySelectID)BIT(ES_ENTITY_TYPE_BITS ) - 1) << ES_ENTITY_TYPE_SHIFT;
	static const entitySelectID ES_SHADER_INDEX_MASK    = ((entitySelectID)BIT(ES_SHADER_INDEX_BITS) - 1) << ES_SHADER_INDEX_SHIFT;
	static const entitySelectID ES_POOL_SLOT_MASK       = ((entitySelectID)BIT(ES_POOL_SLOT_BITS   ) - 1) << ES_POOL_SLOT_SHIFT;

	static const entitySelectID ES_SHADER_INDEX_INVALID = ES_SHADER_INDEX_MASK >> ES_SHADER_INDEX_SHIFT;

	CompileTimeAssert(ES_ENTITY_TYPE_BITS + ES_SHADER_INDEX_BITS + ES_POOL_SLOT_BITS <= ENTITYSELECT_BIT_COUNT);
	CompileTimeAssert(ENTITY_TYPE_TOTAL < BIT(ES_ENTITY_TYPE_BITS));
};

template <typename T>
fwEntity* CEntityIDHelper::GetEntityFromPool(T* pPool, u32 poolSlot)
{
	Assert(pPool);

	Assertf(poolSlot < pPool->GetSize(), "Invalid entity slot %d!", poolSlot);
	if(poolSlot > pPool->GetSize())
		return NULL;

	fwEntity* pEntity = static_cast<fwEntity*>(pPool->GetSlot(poolSlot));
	if (pEntity)
	{
#if __ASSERT
		// hack to prevent assert when we access this from the renderthread
		const VerifyMainThread verifyPrev = sysMemVerifyMainThread;
		sysMemVerifyMainThread = NULL;
#endif // __ASSERT

		Assertf(pPool->IsValidPtr(pEntity), "CEntitySelect::GetEntityFromID: entity 0x%p isn't valid type %d, this is bad!", pEntity, pEntity->GetType());
		if (!pPool->IsValidPtr(pEntity))
		{
			pEntity = NULL; // nevermind, but the assert means something is not good
		}

#if __ASSERT
		// restore
		sysMemVerifyMainThread = verifyPrev;
#endif // __ASSERT
	}

	return pEntity;
}

template <typename T>
entitySelectID CEntityIDHelper::GetEntityIDFromPool(T* pPool, const fwEntity* pEntity)
{
	Assert(pPool);
	const entitySelectID entityType = pEntity->GetType();	

	entitySelectID poolSlot = (entitySelectID) pPool->GetJustIndex(pEntity);
	Assert(static_cast<fwEntity*>(pPool->GetSlot((u32)poolSlot)) == pEntity);

	Assertf(entityType < BIT(ES_ENTITY_TYPE_BITS), "entityType is 0x%08x, expected <= 0x%08x", (int)entityType, BIT(ES_ENTITY_TYPE_BITS) - 1);
	Assertf(poolSlot < BIT(ES_POOL_SLOT_BITS), "poolSlot is 0x%08x, expected <= 0x%08x", (int)poolSlot, BIT(ES_POOL_SLOT_BITS) - 1);

	const entitySelectID shaderIndex = ES_SHADER_INDEX_INVALID;
	return (entityType << ES_ENTITY_TYPE_SHIFT) | (shaderIndex << ES_SHADER_INDEX_SHIFT) | (poolSlot << ES_POOL_SLOT_SHIFT);
}

#endif // INC_ENTITYID_H