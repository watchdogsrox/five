//
// scene/loader/maptypes.h
//
// Copyright (C) 1999-2011 Rockstar Games. All Rights Reserved.
//

#include "mapTypes.h"

// rage includes
#include "diag/art_channel.h"

//fw includes
#include "fwscene/stores/maptypesstore.h"
#include "streaming/streamingengine.h"

//game include 
#include "modelinfo/BaseModelInfo.h"
#include "modelInfo/CompEntityModelInfo.h"
#include "scene/scene_channel.h"
#include "system/memmanager.h"

//pargen includes
#include "MapData_Extensions.h"
#include "mapTypes_parser.h"

SCENE_OPTIMISATIONS();

#define PRINT_EFFECT_TYPES (0 && __BANK)

void CMapTypes::ConstructLocalExtensions(s32 mapTypeDefIndex, fwArchetype* pArchetype, fwArchetypeDef* pArchetypeDef) 
{
	Assert(pArchetype);
	Assert(pArchetypeDef);

	const int count = pArchetypeDef->m_extensions.GetCount();

	if (count == 0)
	{
		return;
	}

	CBaseModelInfo *pMI = smart_cast<CBaseModelInfo*>(pArchetype);
	Assert(pMI);
	pMI->SetNum2dEffects(count);

#if PRINT_EFFECT_TYPES
	static bool bZeroArray = true;
	static bool bCount = false;
	static atFixedArray<u32, ET_MAX_2D_EFFECTS> arrTypes;
	if(bZeroArray)
	{
		arrTypes.clear();
		for(int i=0; i < ET_MAX_2D_EFFECTS; i++)
			arrTypes.Append() = 0;
		bZeroArray = false;
		bCount = true;
	}
#endif // PRINT_EFFECT_TYPES

	for (int i = 0; i < count; ++i)
	{
		CExtensionDef* def = smart_cast<CExtensionDef*>(pArchetypeDef->m_extensions[i]);

		Assertf(CExtensionDefLightEffect::parser_GetStaticStructure()->GetNameHash() != def->parser_GetStructure()->GetNameHash(), "Unexpected light extension!");

		s32 factoryId = ms_extensionDefAssociations[def->parser_GetStructure()->GetNameHash()];

		C2dEffect* effect = NULL;

		fwArchetypeExtensionFactoryBase* factory = fwArchetypeManager::Get2dEffectFactory(factoryId);
		Assertf(factory!=NULL, "MapTypes: Extension factory association not registered for %s", def->parser_GetStructure()->GetName());

		if (!factory)
			continue;

		effect = (C2dEffect*) factory->CreateBaseItem(mapTypeDefIndex);
		FastAssert(effect);

#if PRINT_EFFECT_TYPES
		if(bCount)
		{
			arrTypes[effect->GetType()]++;
		}
#endif // PRINT_EFFECT_TYPES

		effect->InitArchetypeExtensionFromDefinition(def, pArchetype);
		C2dEffect** effectPtr;	

		if (!pArchetype)		
		{
			Assertf(false,"No construction of world2dEffectStore entries is permitted in .ityp processing");
		}
		else
		{
			if (!artVerifyf(pArchetype, "Trying to attach 2d effect to model %s (%.02f %.02f %.02f) that doesn't exist", def->m_name.GetCStr(), def->m_offsetPosition.x, def->m_offsetPosition.y, def->m_offsetPosition.z))
			{
				continue;
			}

			effectPtr = pMI->GetNewEffect();
			*effectPtr = effect;
			pMI->Add2dEffect(effectPtr);
		}
	}

#if PRINT_EFFECT_TYPES
	if(bCount)
	{
		Displayf("Loaded effects count:");
		for(int i=0; i < ET_MAX_2D_EFFECTS; i++)
		{
			if(arrTypes[i] > 0)
			{
				Displayf("\tType %d - %d", i, arrTypes[i]);
			}
		}
	}
#endif // PRINT_EFFECT_TYPES
}

void SetModelInfoFlags_(CBaseModelInfo* pModelInfo, u32 flags, u32 specialAttribute)
{
	pModelInfo->ConvertLoadFlagsAndAttributes(flags, specialAttribute);
}

void CMapTypes::ConstructCompositeEntities(fwMapTypesContents* /*contents*/)
{
	for(u32 i=0; i<m_compositeEntityTypes.GetCount(); i++){
		CCompositeEntityType& compEntityType = m_compositeEntityTypes[i];

		// handle generic model info data
		CCompEntityModelInfo* pCEModelInfo;
		pCEModelInfo = CModelInfo::AddCompEntityModel(compEntityType.GetName());
		sceneAssert(pCEModelInfo);

		if (pCEModelInfo){
			pCEModelInfo->SetLodDistance(compEntityType.GetLodDist());
			SetModelInfoFlags_(pCEModelInfo, compEntityType.GetFlags(), compEntityType.GetSpecialAttribute());

			pCEModelInfo->SetBoundingBoxMin(compEntityType.GetBBMin());
			pCEModelInfo->SetBoundingBoxMax(compEntityType.GetBBMax());
			pCEModelInfo->SetBoundingSphere(compEntityType.GetBSCentre(), compEntityType.GetBSRadius());
		}

 		pCEModelInfo->SetStartModelHash(atStringHash(compEntityType.GetStartModel()));
 		pCEModelInfo->SetEndModelHash(atStringHash(compEntityType.GetEndModel()));

		pCEModelInfo->SetStartImapFile(compEntityType.GetStartImapFile());
		pCEModelInfo->SetEndImapFile(compEntityType.GetEndImapFile());

		pCEModelInfo->SetPtFxAssetName(compEntityType.GetPtFxAssetName());

		bool bUsesImapForStart = (pCEModelInfo->GetStartImapFile().GetHash() != 0);
		bool bUsesImapForEnd = (pCEModelInfo->GetEndImapFile().GetHash() != 0);
		Assertf(bUsesImapForStart == bUsesImapForEnd,"Rayfire %s : .imap groups _must_ be used for both start and end states.",compEntityType.GetName());

		pCEModelInfo->SetUseImapForStartAndEnd(bUsesImapForStart && bUsesImapForEnd);

		atArray<CCompEntityAnims>& compEntityAnims = compEntityType.GetAnimData();

		Assert(compEntityAnims.GetCount() > 0);

		pCEModelInfo->CopyAnimsData(compEntityAnims);

		pCEModelInfo->SetDrawableTypeAsAssetless();

		Assert(pCEModelInfo->GetModelType() == MI_TYPE_COMPOSITE);
	}
}

void CMapTypes::PreConstruct(s32 mapTypeDefIndex, fwMapTypesContents* UNUSED_PARAM(contents))
{
	// in the event of an archetype emergency...
	if ( (m_archetypes.GetCount() + fwArchetypeManager::GetCurrentNumRegisteredArchetypes()) > fwArchetypeManager::GetMaxArchetypeIndex() )
	{
		Displayf("*************************");
		Displayf("** Streaming interests **");
		g_strStreamingInterface->DumpStreamingInterests();

		Assertf(false,"Archetype limit detected - emergency flush of map data triggered");

		int curFlags = g_MapTypesStore.GetStreamingFlags(strLocalIndex(mapTypeDefIndex));
		g_MapTypesStore.SetRequiredFlag(mapTypeDefIndex, STRFLAG_DONTDELETE);

		g_MapDataStore.RemoveUnrequired(true);
		gRenderThreadInterface.Flush();											// allow protected archetypes to time out
		g_MapTypesStore.RemoveUnrequired();										// remove unwanted streamed archetype files

		g_MapTypesStore.ClearRequiredFlag(mapTypeDefIndex, STRFLAG_DONTDELETE);
		g_MapTypesStore.SetFlag(strLocalIndex(mapTypeDefIndex), curFlags);

		Assert( curFlags == g_MapTypesStore.GetStreamingFlags(strLocalIndex(mapTypeDefIndex)));

		if ( (m_archetypes.GetCount() + fwArchetypeManager::GetCurrentNumRegisteredArchetypes()) > fwArchetypeManager::GetMaxArchetypeIndex() )
		{
			Assertf(false, "Still too many archetypes. Crash.");
		}
	}
}

void CMapTypes::PostConstruct(s32 mapTypeDefIndex, fwMapTypesContents* contents)
{
	ConstructCompositeEntities(contents);

 	fwMapTypesDef* pDef = g_MapTypesStore.GetSlot(strLocalIndex(mapTypeDefIndex));
 	if (pDef && !pDef->GetIsPermanent()){
 		//CModelInfo::PostLoad();			// fixup all the asset references

		fwArchetypeManager::ForAllArchetypesInFile(mapTypeDefIndex, CModelInfo::PostLoad);

		//Displayf("post construct : %d\n", mapTypeDefIndex);
 	}
}

// for a given type file, allocate out the storage required by all the dynamic factories which are needed by the
// types in that file
void CMapTypes::AllocArchetypeStorage(s32 mapTypeDefIndex){

	// establish the sizes of storage required for the archetypes for this file
	u32*	typeCounts = rage_new(u32[NUM_MI_TYPES]);
	memset(typeCounts, 0, NUM_MI_TYPES*sizeof(u32));

	u32*	extensionCounts = rage_new(u32[NUM_EXT_TYPES]);
	memset(extensionCounts, 0, NUM_EXT_TYPES*sizeof(u32));

	const int count = m_archetypes.GetCount();
	for (int i = 0; i < count; ++i)
	{
		fwArchetypeDef* definition = m_archetypes[i];
		s32 factoryId = ms_archetypeDefAssociations[definition->parser_GetStructure()->GetNameHash()];
		typeCounts[factoryId]++;

		// need to alloc storage for extensions inside each archetype
		u32 extensionCount = definition->m_extensions.GetCount();
		if (extensionCount > 0){
			for(u32 j= 0; j< extensionCount; j++){
				s32 factoryId = ms_extensionDefAssociations[definition->m_extensions[j]->parser_GetStructure()->GetNameHash()];
					extensionCounts[factoryId]++;
			}
		}
	}

	// need to allocate storage for extensions independent of archetypes
	u32 extensionCount = m_extensions.GetCount();
	if (extensionCount > 0){
		for(u32 j= 0; j< extensionCount; j++){
			s32 factoryId = ms_extensionDefAssociations[m_extensions[j]->parser_GetStructure()->GetNameHash()];
			extensionCounts[factoryId]++;
		}
	}

	{
#if (__PS3 || __XENON) && !__TOOL && !__RESOURCECOMPILER
		sysMemAutoUseFragCacheMemory mem;
#endif
		for (u32 i =0; i<NUM_MI_TYPES; i++)
		{		
			if (typeCounts[i] != 0)
			{
				fwArchetypeManager::GetArchetypeFactory(i)->AddStorageBlock(mapTypeDefIndex, typeCounts[i]);
			}
		}
	}

	for (u32 i =0; i<NUM_EXT_TYPES; i++)
	{		
		if (extensionCounts[i] != 0)
		{
			fwArchetypeManager::Get2dEffectFactory(i)->AddStorageBlock(mapTypeDefIndex, extensionCounts[i]);
		}
	}

	delete typeCounts;
	delete extensionCounts;
}

//---- CCompositeEntityType ------
CCompositeEntityType::CCompositeEntityType()
{
	Init("");
}

void CCompositeEntityType::Init(const char* name)
{
	strncpy(m_Name, name, MAPTYPE_NAMELEN);
	m_NameHash = atStringHash(m_Name); 
	m_StartModel[0] = '\0';
	m_EndModel[0] = '\0';
}

void CCompositeEntityType::SetName(const char* name)
{
	strncpy(m_Name, name, MAPTYPE_NAMELEN);
	m_NameHash = atStringHash(m_Name); 
}



