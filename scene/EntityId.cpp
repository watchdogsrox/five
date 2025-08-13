//
// filename:	EntityId.cpp
// description:	
//

#include "EntityId.h"
#include "Objects/object.h"
#include "Pickups/Pickup.h"
#include "Vehicles/Vehicle.h"

//	Bug 190155 - GraemeW - I had to add this enum because there currently is no ENTITY_TYPE_PICKUP
//	We need a way to differentiate between objects and pickups
//	I think John Gurney will be adding ENTITY_TYPE_PICKUP, see bug 200271 ..
//	Once that is done, set ENTITY_TYPE_OBJECT_PICKUP_HANDLED_SEPARATELY to 0
#define ENTITY_TYPE_OBJECT_PICKUP_HANDLED_SEPARATELY (1)

#if ENTITY_TYPE_OBJECT_PICKUP_HANDLED_SEPARATELY
enum eEntitySelectPoolType
{
	ENTITY_TYPE_OBJECT_PICKUP = ENTITY_TYPE_TOTAL
};
#endif // ENTITY_TYPE_OBJECT_PICKUP_HANDLED_SEPARATELY

entitySelectID CEntityIDHelper::ComputeEntityID(const fwEntity *entity)
{
	entitySelectID entityID = GetInvalidEntityId();

	if(Verifyf(entity, "WARNING: NULL entity pointer passed in to CEntityIDHelper::ComputeEntityID()!"))
	{
		entitySelectID entityType = entity->GetType();
		Assertf(entityType < ENTITY_TYPE_TOTAL, "EntityType is %d - this means the fwEntity* is bad! The game might crash now...", (int)entityType);

		fwBasePool* pPool = NULL;

#if ENTITY_TYPE_OBJECT_PICKUP_HANDLED_SEPARATELY
		if (entityType == ENTITY_TYPE_OBJECT && ((CObject*)entity)->m_nObjectFlags.bIsPickUp)
		{
			entityType = ENTITY_TYPE_OBJECT_PICKUP;
			pPool = CPickup::GetPool();
		}
		else
#endif // ENTITY_TYPE_OBJECT_PICKUP_HANDLED_SEPARATELY
		{
			// HACK: Vehicle and Object aren't derived from fwBasePool anymore!
			if (entityType == ENTITY_TYPE_OBJECT)
				return GetEntityIDFromPool(CObject::GetPool(), entity);
			else if (entityType == ENTITY_TYPE_VEHICLE)
				return GetEntityIDFromPool(CVehicle::GetPool(), entity);
			else
			{
				pPool = GetEntityPoolFromType((u32)entityType);
			}
		}

		if (pPool)
		{
			entitySelectID poolSlot = pPool->GetIndex(entity);
			Assert(static_cast<fwEntity*>(pPool->GetAt((u32)poolSlot)) == entity);

			Assertf(entityType < BIT(ES_ENTITY_TYPE_BITS), "entityType is 0x%08x, expected <= 0x%08x", (int)entityType, BIT(ES_ENTITY_TYPE_BITS) - 1);
			Assertf(poolSlot < BIT(ES_POOL_SLOT_BITS), "poolSlot is 0x%08x, expected <= 0x%08x", (int)poolSlot, BIT(ES_POOL_SLOT_BITS) - 1);

			const entitySelectID shaderIndex = ES_SHADER_INDEX_INVALID;

			entityID = (entityType << ES_ENTITY_TYPE_SHIFT) | (shaderIndex << ES_SHADER_INDEX_SHIFT) | (poolSlot << ES_POOL_SLOT_SHIFT);
		}
	}

	return entityID;
}

fwEntity *CEntityIDHelper::GetEntityFromID(entitySelectID id)
{
	fwEntity *pEntity = NULL;

	if(IsEntityIdValid(id))
	{
		const entitySelectID entityType = (id & ES_ENTITY_TYPE_MASK) >> ES_ENTITY_TYPE_SHIFT;
		const entitySelectID poolSlot   = (id & ES_POOL_SLOT_MASK  ) >> ES_POOL_SLOT_SHIFT;

		fwBasePool* pPool = NULL;

		// hack to get around asserts when entityType=231 for some reason .. the old code used to just return NULL if the type wasn't valid
#if ENTITY_TYPE_OBJECT_PICKUP_HANDLED_SEPARATELY
		// ENTITY_TYPE_OBJECT_PICKUP is ENTITY_TYPE_TOTAL, so skipping the test in this case.
		if( entityType != ENTITY_TYPE_OBJECT_PICKUP )
#endif // ENTITY_TYPE_OBJECT_PICKUP_HANDLED_SEPARATELY
		{
			if (entityType <= ENTITY_TYPE_NOTHING ||
				entityType >= ENTITY_TYPE_TOTAL)
			{
				return NULL;
			}
		}

#if ENTITY_TYPE_OBJECT_PICKUP_HANDLED_SEPARATELY
		if (entityType == ENTITY_TYPE_OBJECT_PICKUP)
		{
			pPool = CPickup::GetPool();
		}
		else
#endif // ENTITY_TYPE_OBJECT_PICKUP_HANDLED_SEPARATELY
		{
			// HACK: Vehicle and Object aren't derived from fwBasePool anymore!
			if (entityType == ENTITY_TYPE_OBJECT)
				return GetEntityFromPool(CObject::GetPool(), (u32)poolSlot);
			else if (entityType == ENTITY_TYPE_VEHICLE)
				return GetEntityFromPool(CVehicle::GetPool(), (u32)poolSlot);
			else 
			{
				pPool = GetEntityPoolFromType((u32)entityType);
			}			
		}

		if (pPool && (poolSlot>>8) < pPool->GetSize() )
		{
			pEntity = static_cast<fwEntity*>(pPool->GetAt((u32)poolSlot));
			if (pEntity)
			{
				Assertf(pPool->IsValidPtr(pEntity), "CEntityIdHelper::GetEntityFromID: entity 0x%p isn't valid type %d, this is bad!", pEntity, (int)entityType);
				if (!pPool->IsValidPtr(pEntity))
				{
					pEntity = NULL; // nevermind, but the assert means something is not good
				}
			}
		}
	}

	return pEntity;
}

int CEntityIDHelper::GetEntityShaderIndexFromID(entitySelectID id)
{
	if (IsEntityIdValid(id))
	{
		const entitySelectID shaderIndex = (id & ES_SHADER_INDEX_MASK) >> ES_SHADER_INDEX_SHIFT;

		if (shaderIndex != ES_SHADER_INDEX_INVALID)
		{
			return (int)shaderIndex;
		}
	}

	return -1;
}
