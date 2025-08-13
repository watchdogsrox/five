// Class header.
#include "physics/Tunable.h"

// Framework headers:

// Game headers:
#include "debug/DebugScene.h"
#include "Objects/object.h"
#include "physics/physics_channel.h"
#include "scene/Entity.h"
#include "scene/datafilemgr.h"

// Parser headers:
#include "physics/Tunable_parser.h"

// RAGE headers:
#include "bank/bank.h"
#include "physics/levelnew.h"

PHYSICS_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CTunableObjectEntry methods:
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CTunableObjectManager methods:
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Static manager singleton object instance.
CTunableObjectManager CTunableObjectManager::m_instance;

void CTunableObjectManager::Load(const char* fileName)
{
	if(Verifyf(!m_instance.m_TunableObjects.GetCount(), "Tunable object data have already been loaded."))
	{
		// Reset / delete the existing data before loading.
		m_instance.m_TunableObjects.Reset();

		PARSER.LoadObject(fileName, "meta", m_instance);
	}
}

const CTunableObjectEntry* CTunableObjectManager::GetTuningEntry(atHashWithStringNotFinal hName) const
{
	return m_TunableObjects.SafeGet(hName);
}

#if __BANK
static bool _RetuneObjectsCB(void* pItem, void* pData)
{
	physicsAssert(pItem);

	atArray<atHashString>& oldTunableNames = *(atArray<atHashWithStringNotFinal>*)pData;

	CObject* pObject = static_cast<CObject*>(pItem);
	CBaseModelInfo* baseModelInfo = pObject->GetBaseModelInfo();
	atHashString modelNameHash = baseModelInfo->GetModelNameHash();

	const CTunableObjectEntry* pEntry = CTunableObjectManager::GetInstance().GetTuningEntry(modelNameHash);
	if(pEntry || (oldTunableNames.Find(modelNameHash) != -1))
	{
		phArchetypeDamp* archetype = const_cast<fwArchetype*>(pObject->GetArchetype())->GetPhysics();
		if(Verifyf(archetype,"Trying to tune non-physical object"))
		{
			// Recompute the archetype's mass and damping since this object's tuning could've changed
			baseModelInfo->ComputeMass(archetype, pEntry);

			// Reset the damping because ComputeDamping only overwrites certain damping values, we don't want to keep old damping
			archetype->ActivateDamping(phArchetypeDamp::LINEAR_C,Vector3(0,0,0));
			archetype->ActivateDamping(phArchetypeDamp::LINEAR_V,Vector3(0,0,0));
			archetype->ActivateDamping(phArchetypeDamp::LINEAR_V2,Vector3(0,0,0));
			archetype->ActivateDamping(phArchetypeDamp::ANGULAR_C,Vector3(0,0,0));
			archetype->ActivateDamping(phArchetypeDamp::ANGULAR_V,Vector3(0,0,0));
			archetype->ActivateDamping(phArchetypeDamp::ANGULAR_V2,Vector3(0,0,0));
			baseModelInfo->ComputeDamping(archetype, pEntry);

			baseModelInfo->ReloadWaterSamples();

			// Update the collider from the archetype, if it exists
			phCollider* collider = NULL;
			if(phInst* inst = pObject->GetCurrentPhysicsInst())
			{
				u16 levelIndex = inst->GetLevelIndex();
				if(PHLEVEL->IsInLevel(levelIndex) && PHLEVEL->IsActive(levelIndex))
				{
					collider = (phCollider*)(PHLEVEL->GetUserData(levelIndex));
					collider->InitInertia();
				}
			}
		}
	}

	return true;
}

void CTunableObjectManager::Reload()
{
	//store hashes for all existing tunableobjects
	atArray<atHashWithStringNotFinal> old_keys;
	for(int i = 0; i < m_instance.m_TunableObjects.GetCount(); ++i)
	{
		old_keys.PushAndGrow(m_instance.m_TunableObjects.GetKey(i)->GetHash());
	}

	// Reload the tunable objects from the meta file
	m_instance.m_TunableObjects.Reset();
	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::TUNABLE_OBJECTS_FILE);
	CTunableObjectManager::Load(pData->m_filename);

	// Retune all objects in the game
	CObject::GetPool()->ForAll(_RetuneObjectsCB, &old_keys);
}
#endif // __BANK

#if __BANK
void CTunableObjectManager::AddWidgetsToBank(bkBank& bank)
{
	bank.AddButton("Reload tunable metadata", Reload);
}
#endif //__BANK
