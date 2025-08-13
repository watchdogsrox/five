// Title	:	Building.cpp
// Author	:	Richard Jobling
// Started	:	22/06/99

#include "scene/building.h"
#include "scene/world/GameWorld.h"
#include "scene/world/VisibilityMasks.h"
#include "streaming/streamingengine.h"
#include "ModelInfo/ModelInfo.h"
#include "ModelInfo/ModelInfo_Factories.h"

#if __DEV
// required for pool full CB
#include "scene/FocusEntity.h"
#include "scene/LoadScene.h"
#include "world/GameWorld.h"
#include "scene/streamer/StreamVolume.h"
#include "fwscene/stores/staticboundsstore.h"
#endif

#include "streaming/streamingdebug.h"
#include "fwscene/stores/mapdatastore.h"

FW_INSTANTIATE_CLASS_POOL_SPILLOVER(CBuilding, CONFIGURED_FROM_FILE, 0.16f, atHashString("Building",0x5411e15c));

// DEBUG!! -AC, Temporary until I know current size of CEntity...
////update if CEntity changes or CBuilding
////theres 25000 of these so be aware of the ram implications
//#if __PPU
////did you add something to CBuilding, be aware that adding anything will add 16 bytes to CBuilding
//CompileTimeAssert(sizeof(CBuilding) == CENTITY_INTENDED_SIZE+0);
//#else
////did you add something to CBuilding, be aware that adding anything will add 16 bytes to CBuilding
//CompileTimeAssert(sizeof(CBuilding) == CENTITY_INTENDED_SIZE+0);
//#endif
// END DEBUG!!

// name:		CBuilding::CBuilding
// description:	Constructor for a building
CBuilding::CBuilding(const eEntityOwnedBy ownedBy)
	: CEntity( ownedBy )
{
	SetTypeBuilding();
	SetVisibilityType( VIS_ENTITY_HILOD_BUILDING );

	EnableCollision();
	SetBaseFlag(fwEntity::IS_FIXED);

	//TellAudioAboutAudioEffectsAdd();			// Tells the audio an entity has been created that may have audio effects on it.
}

//
// name:		CBuilding::CBuilding
// description:	Constructor for a building
CBuilding::~CBuilding()
{
	REPLAY_ONLY(CReplayMgr::RemoveBuilding(this);)

	TellAudioAboutAudioEffectsRemove();			// Tells the audio an entity has been removed that may have audio effects on it.
}

void CBuilding::InitEntityFromDefinition(fwEntityDef* definition, fwArchetype* archetype, s32 mapDataDefIndex)
{
	CEntity::InitEntityFromDefinition(definition, archetype, mapDataDefIndex);

	TellAudioAboutAudioEffectsAdd();

	if (definition->m_lodLevel!=LODTYPES_DEPTH_HD && definition->m_lodLevel!=LODTYPES_DEPTH_ORPHANHD)
		SetVisibilityType(VIS_ENTITY_LOLOD_BUILDING);

	if (GetBaseModelInfo()->GetIsTree())
		SetVisibilityType(VIS_ENTITY_TREE);

	REPLAY_ONLY(CReplayMgr::AddBuilding(this);)
}

bool CBuilding::CreateDrawable()
{
	// give building physics if required
	if ( GetLodData().IsHighDetail() && IsBaseFlagSet(fwEntity::HAS_PHYSICS_DICT) && !IsBaseFlagSet(fwEntity::NO_INSTANCED_PHYS) && !GetCurrentPhysicsInst() )
	{
		InitPhys();
		if (GetCurrentPhysicsInst())
		{
			AddPhysics();
		}
	}
	return CEntity::CreateDrawable();
}

#if __BANK
template<> void fwPool<CBuilding>::PoolFullCallback() 
{
	Displayf("********************************************");
	Displayf("** BUILDING POOL FULL CALLBACK OUTPUT **");

	g_strStreamingInterface->DumpStreamingInterests();

	// NOTE: This is SOMETIMES helpful, but not often because it blocks the previous line of debuggery

	CStreamingDebug::DisplayObjectsInMemory(NULL, false, g_MapDataStore.GetStreamingModuleId(),(1 << STRINFO_LOADED));

#if 0
	Displayf("Model, Type, ID, Index, (xPos, yPos)");
	for (s32 i = 0; i < CBuilding::GetPool()->GetSize(); ++i)
	{
		CBuilding* pEntity = CBuilding::GetPool()->GetSlot(i);
		if (pEntity)
		{
			if (pEntity->GetArchetype())
			{
				fwModelId id = pEntity->GetModelId();
				Vector3 pos = pEntity->GetPreviousPosition();
	#if __ASSERT			
				const char* pszTypeFilename = id.GetTypeFileName();
	#else
				const char* pszTypeFilename = "unknown type";
	#endif
				Displayf("%s, %s, %d, %d, (%f, %f)", pEntity->GetModelName(), pszTypeFilename, id.ConvertToU32(), pEntity->GetModelIndex(), pos.GetX(), pos.GetY());
			}
		}
	}

	Displayf("Model, Bounds, Index, (xPos, yPos)");
	for (s32 i = 0; i < CBuilding::GetPool()->GetSize(); ++i)
	{
		CBuilding* pEntity = CBuilding::GetPool()->GetSlot(i);
		if (pEntity)
		{
			if (!pEntity->GetArchetype())
			{
				Vector3 pos = pEntity->GetPreviousPosition();
				const char* entityName = "static dummy";
#if __ASSERT			
				const char* boundsFilename = g_StaticBoundsStore.GetName(pEntity->GetIplIndex());
#else
				const char* boundsFilename = "unknown bounds";
#endif
				Displayf("%s, %s, %d, (%f, %f)", entityName, boundsFilename, pEntity->GetIplIndex(), pos.GetX(), pos.GetY());
			}
		}
	}
#endif
}
#endif // __BANK

//////////////////////////////////////////////////////////////////////////////
// NAME :     AssertBuildingPointerValid
// FUNCTION : Will assert if the building pointer is not valid or the building is not in
//            the world.
//////////////////////////////////////////////////////////////////////////////

#ifdef DEBUG
void AssertBuildingPointerValid(CBuilding *pBuilding)
{
    Assert(IsBuildingPointerValid(pBuilding));
}
#endif

bool IsBuildingPointerValid(CBuilding *pBuilding)
{
    if(!pBuilding)
        return false;

	if (CBuilding::GetPool()->IsValidPtr(pBuilding))
		return true;
	return false;
}

#if __BANK

bool RegenDLAlphasCB(void* pItem, void* UNUSED_PARAM(data))
{
	CBuilding* pEntity = static_cast<CBuilding*>(pItem);

	if (pEntity && pEntity->GetDrawable()){
		DLC(CDrawEntityDC, (pEntity));
	}

	return (true);
}

// regeneration the drawlist cache for all building objects
void CBuilding::RegenDLAlphas(void)
{
	CBuilding::GetPool()->ForAll(RegenDLAlphasCB, NULL);
}

#endif //__BANK
