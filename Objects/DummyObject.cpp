// Title	:	DummyObject.cpp
// Author	:	Richard Jobling
// Started	:	20/04/2000

// Framework headers
#include "fwscene/world/worldmgr.h"
#include "fwscene/stores/mapdatastore.h"

// Rage headers
#if __ASSERT
#include "diag/art_channel.h"
#endif

// Game headers 
#include "DummyObject.h"
#include "Door.h"
#include "Object.h"
#include "PathServer/ExportCollision.h"
#include "scene/world/GameWorld.h"
#include "script/script.h"
#include "script/script_brains.h"
#include "renderer/Lights/LightEntity.h"

#if __BANK
#include "streaming/streamingengine.h"
#endif

FW_INSTANTIATE_CLASS_POOL_SPILLOVER(CDummyObject, CONFIGURED_FROM_FILE, 0.24f, atHashString("Dummy Object",0xf17288d6)); // JW - was 7000

CDummyObject::CDummyObject(const eEntityOwnedBy ownedBy)
	: CEntity( ownedBy )
{
	SetTypeDummyObject();
	SetVisibilityType( VIS_ENTITY_DUMMY_OBJECT );
}

CDummyObject::CDummyObject(const eEntityOwnedBy ownedBy, class CObject* pObject)
	: CEntity( ownedBy )
{
	SetVisibilityType( VIS_ENTITY_DUMMY_OBJECT );

	// pass tint index to dummy object (before CreateDrawable()):
	SetTintIndex(pObject->GetTintIndex());

	SetModelId(pObject->GetModelId());
	if(pObject->GetDrawable())
	{
		CreateDrawable();
	}
	pObject->DeleteDrawable();
	
	SetMatrix(MAT34V_TO_MATRIX34(pObject->GetMatrix()));
	SetIplIndex(pObject->GetIplIndex());
}

CDummyObject::~CDummyObject()
{
	if (GetBaseModelInfo()->GetUsesDoorPhysics())
	{
		CDoor::RemoveDoorFromList(this);
	}
}

void CDummyObject::Hide(const bool visible, const bool skipUpdateWorldFromEntity)
{
	if ( GetExtension<CIsVisibleExtension>() )
		SetIsVisibleForModule( SETISVISIBLE_MODULE_DUMMY_CONVERSION, visible );
	else
	{
		AssignBaseFlag( fwEntity::IS_VISIBLE, visible );

		if(!skipUpdateWorldFromEntity)
		{
			UpdateWorldFromEntity();
		}
	}
}

void CDummyObject::ForceIsVisibleExtension()
{
	if ( GetExtension<CIsVisibleExtension>() )
		return;

	CIsVisibleExtension*	ext = rage_new CIsVisibleExtension;
	GetExtensionList().Add( *ext );
	ext->SetIsVisibleFlag( SETISVISIBLE_MODULE_DUMMY_CONVERSION, IsBaseFlagSet(fwEntity::IS_VISIBLE) );
}

void CDummyObject::InitEntityFromDefinition(fwEntityDef* definition, fwArchetype* archetype, s32 mapDataDefIndex)
{
	definition->m_scaleXY = 1.0f;
	definition->m_scaleZ = 1.0f;

	CEntity::InitEntityFromDefinition(definition, archetype, mapDataDefIndex);

	CBaseModelInfo*	 baseModelInfo = smart_cast< CBaseModelInfo* >(archetype);
	if (baseModelInfo->GetUsesDoorPhysics())
	{
		CDoor::AddDoorToList(this);
#if __ASSERT
		if (definition->m_priorityLevel != fwEntityDef::PRI_REQUIRED)
		{
			artWarningf( "Door %s at (%f,%f,%f) has priority different that PRI_REQUIRED, it will not appear in certain game modes",
				archetype->GetModelName(), definition->m_position.GetX(), definition->m_position.GetY(), definition->m_position.GetZ() );
		}
#endif
	}

	if (!baseModelInfo->GetHasDrawableProxyForWaterReflections())
	{
		bool bHasBuoyancy = false;

		GET_2D_EFFECTS_NOLIGHTS(baseModelInfo);
		for (int i = 0; i < iNumEffects; i++)
		{
			const C2dEffect* pEffect = (*pa2dEffects)[i];

			if (pEffect->GetType() == ET_BUOYANCY)
			{
				bHasBuoyancy = true;
				break;
			}
		}

		m_nFlags.bUseMaxDistanceForWaterReflection = !bHasBuoyancy;
	}

	//////////////////////////////////////////////////////////////////////////
	// hacky effort to pin specific model types, ensure they get promoted to objects right away.
	// these are special cases in the map, windmills, pumpjacks etc. IanK
	m_nFlags.bNeverDummy = baseModelInfo->GetNeverDummyFlag();
	if (baseModelInfo->GetNeverDummyFlag())
	{
		CObjectPopulation::RegisterDummyObjectMarkedAsNeverDummy(this);
	}
	//////////////////////////////////////////////////////////////////////////

#if GTA_REPLAY
	GenerateHash(definition->m_position);
#endif // GTA_REPLAY
}

#if __BANK
template<> void fwPool<CDummyObject>::PoolFullCallback() 
{
	Displayf("********************************************");
	Displayf("** DUMMYOBJECT POOL FULL CALLBACK OUTPUT **");

	g_strStreamingInterface->DumpStreamingInterests();
}
#endif // __BANK

//
// name:		CreateObject
// description:	Remove related object and re-enable the dummy
CObject* CDummyObject::CreateObject()
{
	CObject* pObject = CObjectPopulation::CreateObject(this, ENTITY_OWNEDBY_RANDOM);;

#if __FINAL
	if(pObject == NULL)
		return NULL;
#endif

#if GTA_REPLAY
	// Check if this object has been marked as destroyed in replay, and remove from mark, since it's just been created/re-created/un-destroyed
	if(pObject)
	{
		CReplayMgr::RecordRemadeMapObject(pObject);
	}
#endif // GTA_REPLAY

#if NAVMESH_EXPORT
	if(!CNavMeshDataExporter::IsExportingCollision())
#endif
	{
		if (pObject)
		{
			CTheScripts::GetScriptsForBrains().CheckAndApplyIfNewEntityNeedsScript(pObject, CScriptsForBrains::OBJECT_STREAMED);	//	, NULL);
		}
	}

	Hide( false );
	DisableCollision();

	// pass tint index back to parent object:
	pObject->SetTintIndex(GetTintIndex());

	return pObject;
}

//
// name:		RemoveObject
// description:	Remove related object and re-enable the dummy
void CDummyObject::UpdateFromObject(CObject* pObject, const bool skipUpdateWorldFromEntity)
{
	Hide( true, skipUpdateWorldFromEntity );
	EnableCollision();
	
	bool bCalculateAmbientScales = true;
	// see if the object has moved (if it has, we can't use it's ambient scales)
	if(VEC3V_TO_VECTOR3(pObject->GetTransform().GetPosition()).Dist2(VEC3V_TO_VECTOR3(GetTransform().GetPosition())) < .01)
	{
		bCalculateAmbientScales = false; // don't need to calculate
	}
	
	// pass tint index to dummy object (before CreateDrawable() - so CSE is created with right tint):
	SetTintIndex(pObject->GetTintIndex());

	if(bCalculateAmbientScales)
	{
		CreateDrawable();
	}
	else
	{
		// Use the ambient scale from the real object
		SetCalculateAmbientScaleFlag(false);
		{
			SetArtificialAmbientScale(pObject->GetArtificialAmbientScale());
			SetNaturalAmbientScale(pObject->GetNaturalAmbientScale());	

			CreateDrawable();
		}
		SetCalculateAmbientScaleFlag(true);
	}

	pObject->DeleteDrawable();
	
	// I am assuming IPLs that are loaded from script would want to keep their 
	// damage state setup
	if(!INSTANCE_STORE.IsScriptManaged(strLocalIndex(pObject->GetIplIndex())))
	{
		m_nFlags.bRenderDamaged = pObject->m_nFlags.bRenderDamaged;
		Hide( pObject->GetIsVisible(), skipUpdateWorldFromEntity );
		pObject->IsCollisionEnabled() ? EnableCollision() : DisableCollision();
	}

}

void CDummyObject::Add()
{
	Add(GetBoundRect());
}

//
// name:		CDummyObject::Add
// description:	Add dummy object to world
void CDummyObject::Add(const fwRect& rect)
{
	//Assert(GetInsertionState() != IS_BEGIN);	// cannot add entities to world without establishing insertion data

	gWorldMgr.AddToWorld(this, rect);
}

//
// name:		CDummyObject::Remove
// description:	Remove dummy object from world
void CDummyObject::Remove()
{
	gWorldMgr.RemoveFromWorld(this);
}

//
// name:		CDummyObject::Enable
// description:	Enables this dummy in the world
void CDummyObject::Enable()
{
	Hide(true, true);
	SetIsSearchable(true);
	CGameWorld::PostAdd( this );
}

// name:		CDummyObject::Disable
// description:	Disables this dummy in the world
void CDummyObject::Disable()
{
	Hide(false, true);
	SetIsSearchable(false);
	CGameWorld::PostRemove( this );
}

#if GTA_REPLAY
void CDummyObject::GenerateHash(const Vector3 &position)
{
	m_hash = CReplayMgr::GenerateObjectHash(position, GetArchetype()->GetModelNameHash());
}
#endif // GTA_REPLAY
