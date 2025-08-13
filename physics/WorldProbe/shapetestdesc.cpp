#include "physics/WorldProbe/shapetestdesc.h"

#include "scene/Entity.h"
#include "Peds/ped.h"
#include "vehicles/Automobile.h"
#include "vehicles/vehicle.h"

PHYSICS_OPTIMISATIONS()

void WorldProbe::CShapeTestDesc::AddInstanceToList(InExInstanceArray& instances, const phInst* pInstance)
{
	// Only add the instance if it exists in the physics level and isn't already in the list. It's a waste otherwise. 
	if(	pInstance && 
		pInstance->IsInLevel() && 
		instances.Find(pInstance) == -1 && 
		physicsVerifyf(!instances.IsFull(),"Trying to add too many include/exclude instances."))
	{
		physicsAssertf(pInstance != reinterpret_cast<phInst*>(0xDDDDDDDD) && pInstance != reinterpret_cast<phInst*>(0xCDCDCDCD), "Passing a non-existent physics instance into a shape test");
		instances.Push(pInstance);
	}
}

void WorldProbe::CShapeTestDesc::AddWeaponAndComponents(InExInstanceArray& instances, const CPed* pPed)
{
	const CPedWeaponManager* pWeaponMgr = pPed->GetWeaponManager();
	if(pWeaponMgr && pWeaponMgr->GetEquippedWeaponObject()
		&& pWeaponMgr->GetEquippedWeaponObject()->GetCurrentPhysicsInst())
	{
		AddInstanceToList(instances,pWeaponMgr->GetEquippedWeaponObject()->GetCurrentPhysicsInst());

		const CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
		if( pWeapon )
		{
			const CWeapon::Components& weaponComponents = pWeapon->GetComponents();
			for(s32 i = 0; i < weaponComponents.GetCount(); i++)
			{
				const CDynamicEntity* pDynEntity = weaponComponents[i]->GetDrawable();
				if( pDynEntity )
				{
					AddInstanceToList(instances,pDynEntity->GetCurrentPhysicsInst());
				}
			}
		}
	}
}

const int MAX_NUM_ALLOWED_ATTACHMENTS=10;
void WorldProbe::CShapeTestDesc::AddAttachedEntities(InExInstanceArray& instances, const CPed* pPed)
{
	// Walk the tree of objects in a breadth-first fashion and add the "phInst"s found to 'paInstsToAdd'.
	if(!pPed->GetChildAttachment())
		return;

	atQueue<CPhysical*, MAX_NUM_ALLOWED_ATTACHMENTS*2> queue;

	// Enqueue the first attach child of the ped.
	CPhysical* pAttachedPhysical = static_cast<CPhysical*>(pPed->GetChildAttachment());
	queue.Push(pAttachedPhysical);

	// Start processing the tree.
	while(!queue.IsEmpty())
	{
		// Dequeue a node and examine it.
		CPhysical*& currentNode = queue.Pop();

		// Add this node to the list of objects as long as it isn't a weapon (these are added separately).
		AddInstanceToList(instances,currentNode->GetCurrentPhysicsInst());

		// Enqueue the direct child and / or sibling of this node if they exist.
		CPhysical* pChild = static_cast<CPhysical*>(currentNode->GetChildAttachment());
		CPhysical* pSibling = static_cast<CPhysical*>(currentNode->GetSiblingAttachment());
		if(pChild)
			queue.Push(pChild);
		if(pSibling)
			queue.Push(pSibling);
	}
}

void WorldProbe::CShapeTestDesc::AddEntityToInstanceList(InExInstanceArray& instances, const CEntity* pEntity, u32 options)
{
	if(pEntity)
	{
		// Peds actually have 2 physics instances. Don't add the animated instance if we are testing against the ragdoll
		// especially as ragdoll activation leads to the deletion of the animated instance!
		if(pEntity->GetIsTypePed())
		{
			const CPed* pPed = static_cast<const CPed*>(pEntity);
			AddInstanceToList(instances,pPed->GetRagdollInst());
			AddInstanceToList(instances,pPed->GetAnimatedInst());

			if(!(options & EIEO_DONT_ADD_WEAPONS))
			{
				// Add this ped's weapon.
				AddWeaponAndComponents(instances,pPed);
			}

			// Add any entities attached to this ped unless requested not to.
			if(!(options & EIEO_DONT_ADD_PED_ATTACHMENTS))
			{
				AddAttachedEntities(instances,pPed);
			}

			// If this ped is in a car, add the car, and any other peds in the car.
			if(pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && pPed->GetMyVehicle() && pPed->GetMyVehicle()->GetFragInst())
			{
				if(!(options & EIEO_DONT_ADD_VEHICLE))
				{
					AddInstanceToList(instances,pPed->GetMyVehicle()->GetFragInst());
				}

				// Add any passengers in the car.
				if(!(options & EIEO_DONT_ADD_VEHICLE_OCCUPANTS))
				{
					for(int nPassenger=0; nPassenger<pPed->GetMyVehicle()->GetSeatManager()->GetMaxSeats(); nPassenger++)
					{
						CPed* pPassenger = pPed->GetMyVehicle()->GetSeatManager()->GetPedInSeat(nPassenger);
						if(pPassenger && pPassenger!=pPed)
						{
							AddInstanceToList(instances,pPassenger->GetRagdollInst());
							AddInstanceToList(instances,pPassenger->GetAnimatedInst());

							if(!(options & EIEO_DONT_ADD_WEAPONS))
							{
								// Add passenger weapon and components
								AddWeaponAndComponents(instances,pPassenger);
							}
						}
					}
				}
			}
		}
		else if(pEntity->GetIsTypeVehicle())
		{
			const CVehicle* pVehicle = static_cast<const CVehicle*>(pEntity);

			// Add the car's physics instance.
			AddInstanceToList(instances,pVehicle->GetFragInst());

			// Add in the dummy insts.
			if(pVehicle->InheritsFromAutomobile())
			{
				const CAutomobile* pAutomobile = static_cast<const CAutomobile*>(pVehicle);
				const phInst* pDummyInst = pAutomobile->GetDummyInst();
				if(pDummyInst)
				{
					AddInstanceToList(instances,pDummyInst);
				}
			}

			if(!(options & EIEO_DONT_ADD_VEHICLE_OCCUPANTS))
			{
				// Add the passengers in the car.
				for(u32 nPassenger=0; nPassenger < pVehicle->GetSeatManager()->GetMaxSeats(); ++nPassenger)
				{
					CPed* pPassenger = pVehicle->GetSeatManager()->GetPedInSeat(nPassenger);
					if(pPassenger)
					{
						AddInstanceToList(instances,pPassenger->GetRagdollInst());
						AddInstanceToList(instances,pPassenger->GetAnimatedInst());

						if(!(options & EIEO_DONT_ADD_WEAPONS))
						{
							// Add passenger weapon and components
							AddWeaponAndComponents(instances,pPassenger);
						}
					}
				}
			}
		}
		else
		{
			AddInstanceToList(instances,pEntity->GetCurrentPhysicsInst());
		}
	}
}

void WorldProbe::CShapeTestDesc::SetInstanceListFromEntities(InExInstanceArray& instances, const CEntity* const * ppEntities, int numEntities, u32 options)
{
	physicsAssert(ppEntities != NULL || numEntities == 0);
	instances.Reset();
	if(ppEntities)
	{
		for(int entityIndex = 0; entityIndex < numEntities; ++entityIndex)
		{
			AddEntityToInstanceList(instances,ppEntities[entityIndex],options);
		}
	}
}

void WorldProbe::CShapeTestDesc::SetInstanceList(InExInstanceArray& instances, const phInst* const *  ppInstances, int numInstances)
{
	physicsAssert(ppInstances != NULL || numInstances == 0);
	instances.Reset();
	if(ppInstances)
	{
		for(int instanceIndex = 0; instanceIndex < numInstances; ++instanceIndex)
		{
			AddInstanceToList(instances,ppInstances[instanceIndex]);
		}
	}
}

void WorldProbe::CShapeTestDesc::SetIncludeInstance(const phInst* pIncludeInstance)
{
	m_aIncludeInstList.Reset();
	AddInstanceToList(m_aIncludeInstList,pIncludeInstance);
}
void WorldProbe::CShapeTestDesc::SetIncludeInstances(const phInst* const *ppIncludeInsts, const int nNumIncludeInsts)
{
	SetInstanceList(m_aIncludeInstList,ppIncludeInsts,nNumIncludeInsts);
}
void WorldProbe::CShapeTestDesc::SetIncludeEntity(const CEntity* const pIncludeEnt, const u32 nOptions)
{
	m_aIncludeInstList.Reset();
	AddEntityToInstanceList(m_aIncludeInstList,pIncludeEnt,nOptions);
}
void WorldProbe::CShapeTestDesc::SetIncludeEntities(const CEntity** const ppIncludeEnts, const int nNumIncludeEnts, const u32 nOptions)
{
	SetInstanceListFromEntities(m_aIncludeInstList,ppIncludeEnts,nNumIncludeEnts,nOptions);
}

void WorldProbe::CShapeTestDesc::AddExludeInstance(const phInst* const pExcludeInstance)
{
	AddInstanceToList(m_aExcludeInstList,pExcludeInstance);
}

void WorldProbe::CShapeTestDesc::SetExcludeInstance(const phInst* const pExcludeInstance)
{
	m_aExcludeInstList.Reset();
	AddInstanceToList(m_aExcludeInstList,pExcludeInstance);
}
void WorldProbe::CShapeTestDesc::SetExcludeInstances(const phInst* const *ppExcludeInsts, const int nNumExcludeInsts)
{
	SetInstanceList(m_aExcludeInstList,ppExcludeInsts,nNumExcludeInsts);
}
void WorldProbe::CShapeTestDesc::SetExcludeEntity(const CEntity* const pExcludeEnt, const u32 nOptions)
{
	m_aExcludeInstList.Reset();
	AddEntityToInstanceList(m_aExcludeInstList,pExcludeEnt,nOptions);
}
void WorldProbe::CShapeTestDesc::SetExcludeEntities(const CEntity** const ppExcludeEnts, const int nNumExcludeEnts, const u32 nOptions)
{
	SetInstanceListFromEntities(m_aExcludeInstList,ppExcludeEnts,nNumExcludeEnts,nOptions);
}
void WorldProbe::CShapeTestDesc::AddExcludeEntity(const CEntity* const pExcludeEnt, const u32 nOptions)
{
	AddEntityToInstanceList(m_aExcludeInstList,pExcludeEnt,nOptions);
}
