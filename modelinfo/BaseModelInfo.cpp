//
//
//    Filename: BaseModelInfo.cpp
//     Creator: Adam Fowler
//     $Author: $
//       $Date: $
//   $Revision: $
// Description: Base class for all model info classes
//
//
#include "BaseModelInfo.h"

// Rage headers
#include "diag/art_channel.h"
#include "fragment/tune.h"
#include "fragment/typecloth.h"
#include "fragment/typechild.h"
#include "fragment/typegroup.h"
#include "fwanimation/animmanager.h"
#include "fwscene/stores/blendshapestore.h"
#include "fwscene/stores/drawablestore.h"
#include "fwscene/stores/dwdstore.h"
#include "fwscene/stores/expressionsdictionarystore.h"
#include "fwscene/stores/fragmentstore.h"
#include "fwscene/stores/txdstore.h"
#include "math/vecmath.h"
#include "phbound/boundbvh.h"
#include "phbound/boundcomposite.h"
#include "phglass/glassinstance.h"
#include "profile/profiler.h"
#include "rmcore/drawable.h"
#include "streaming/packfilemanager.h"
#include "system/bootmgr.h"
#include "system/spinlock.h"

//fw headers
#include "fwscene/stores/maptypesstore.h"

#if __XENON
#include "grcore/indexbuffer.h"
#include "grcore/vertexbuffer.h"
#include "grmodel/geometry.h"
#endif

// Game headers
#include "audio/emitteraudioentity.h"
#include "audio/northaudioengine.h"
#include "cloth/clotharchetype.h"
#include "control/trafficlights.h"
#include "control/replay/ReplayMovieControllerNew.h"
#include "debug/DebugArchetypeProxy.h"
#include "game/ModelIndices.h"
#include "ModelInfo/ModelInfo.h"
#include "ModelInfo/ModelInfo_Factories.h"
#include "modelinfo/timemodelinfo.h"
#include "modelinfo/vehiclemodelinfo.h"
#include "physics/gtainst.h"
#include "physics/gtamaterial.h"
#include "physics/gtamaterialmanager.h"
#include "physics/Tunable.h"
#include "renderer/gtadrawable.h"
#include "renderer/hierarchyids.h"
#include "renderer/RenderTargetMgr.h"
#include "scene/2deffect.h"
#include "scene/loader/MapData_Buildings.h"
#include "scene/EntityBatch.h"
#include "scene/texLod.h"
#include "shaders/customshadereffectbase.h"
#include "streaming/streaming.h"
#include "tools/sectortools.h"
#include "vehicles/VehicleGadgets.h"
#include "Objects/Door.h"
#include "Objects/DoorTuning.h"
#include "objects/dummyobject.h"
#include "physics/physics.h"
#include "scene/building.h"
#include "scene/animatedbuilding.h"
#include "scene/entities/compentity.h"


//float CBaseModelInfo::ms_lodDistScale = 1.0f;
PARAM(fragtuning, "Load fragment tuning");

#if __ASSERT
	PARAM(propasserts, "Assert on prop checks");
#endif

#if __BANK
	PARAM(audiotag, "Enable audio model tagging");
#endif

#if __DEV
#endif //__DEV

SCENE_OPTIMISATIONS()

#define MIN_SIZE_FOR_NETWORK_SYNC (0.8f)
#define MIN_MASS_FOR_NETWORK_SYNC (200.0f)

#define MAX_LOD_TXD_SIZE	(10*1024*1024)

#if __DEV
	bool CBaseModelInfo::ms_bPrintWaterSampleEvents = false;
	u32 CBaseModelInfo::ms_nNumWaterSamplesInMemory = 0;
#endif
EXT_PF_PAGE(GTA_Buoyancy);
PF_GROUP(WaterSamples);
PF_LINK(GTA_Buoyancy, WaterSamples);
PF_VALUE_INT(NumWaterSamplesInMemory, WaterSamples);


int CBaseModelInfo::g_wdcascade_TechniqueGroupId = 0;
int CBaseModelInfo::g_wdcascade_shadowtexture_TechniqueGroupId = 0;

const u32 g_defaultAudioModelCollisions = ATSTRINGHASH("MOD_DEFAULT", 0x05f03e8c2);

extern const u32 g_boatCollisionHashName1 = ATSTRINGHASH("apa_mp_apa_yacht_radar_01a", 0x49566DB0);
extern const u32 g_boatCollisionHashName2 = ATSTRINGHASH("apa_mp_apa_yacht_option3_cold", 0x3CBB90AF);
extern const u32 g_boatCollisionHashName3 = ATSTRINGHASH("apa_mp_apa_yacht_option3_colb", 0x44E3A0FB);
extern const u32 g_boatCollisionHashName4 = ATSTRINGHASH("apa_mp_apa_yacht_option3_colc", 0xAEC8F4C8);
extern const u32 g_boatCollisionHashName5 = ATSTRINGHASH("apa_mp_apa_yacht_option3_cola", 0x3A628BF9);

 CBaseModelInfo::~CBaseModelInfo(void)
 {
	 if (IsStreamedArchetype())
	 {
		// cleanup the model info streaming module entry - ready for re-use
		strStreamingModule* pModelInfoStreamingModule = CModelInfo::GetStreamingModule();
		Assert(pModelInfoStreamingModule);
		strIndex modelInfoStrIndex = pModelInfoStreamingModule->GetStreamingIndex(GetStreamSlot());

		strStreamingInfoManager &infoMgr = strStreamingEngine::GetInfo();
		infoMgr.RemoveObject(modelInfoStrIndex);
		ASSERT_ONLY(bool bRet = )infoMgr.UnregisterObject(modelInfoStrIndex);
		Assert(bRet);
	 }

	 Assertf(m_pBuoyancyInfo == NULL && m_p2dEffectPtrs == NULL, "Shutdown() on an archeype wasn't called - we're leaking memory! (b=%p, 2d=%p)",
		 m_pBuoyancyInfo, m_p2dEffectPtrs);
}

//
// name:		Init
// description:	Initialise base modelinfo
void CBaseModelInfo::Init()
{
	fwArchetype::Init();
	
	m_ptfxAssetSlot = -1;

	m_fxEntityFlags = 0;
	m_pShaderEffectType = NULL;

	m_lodSkelBoneMap = NULL;
	m_lodSkelBoneNum = 0;

	m_bHasPreRenderEffects = 0;
	m_bNeverDummy = false;
	m_bLeakObjectsIntentionally = false;

	m_bHasDrawableProxyForWaterReflections = false;
}

void CBaseModelInfo::ConvertLoadFlagsAndAttributes(u32 loadFlags, u32 attributes)
{
	SetDrawLast(loadFlags & FLAG_DRAW_LAST);
	SetIsTypeObject(loadFlags & FLAG_IS_TYPE_OBJECT);
	SetDontCastShadows(loadFlags & FLAG_DONT_CAST_SHADOWS);
	SetIsShadowProxy(loadFlags & FLAG_SHADOW_ONLY);
	SetIsFixed(loadFlags & FLAG_IS_FIXED);
	SetIsTree(loadFlags & FLAG_IS_TREE);
	SetUsesDoorPhysics(loadFlags & FLAG_DOOR_PHYSICS);
	SetIsFixedForNavigation(loadFlags & FLAG_IS_FIXED_FOR_NAVIGATION);
	SetNotAvoidedByPeds(loadFlags & FLAG_DONT_AVOID_BY_PEDS);
	SetDoesNotProvideAICover(loadFlags & FLAG_DOES_NOT_PROVIDE_AI_COVER);
	SetDoesNotProvidePlayerCover(loadFlags & FLAG_DOES_NOT_PROVIDE_PLAYER_COVER);
	SetDontWriteZBuffer(loadFlags & FLAG_DONT_WRITE_ZBUFFER);
	SetIsClimbableByAI(loadFlags & FLAG_PROP_CLIMBABLE_BY_AI);
	SetOverridePhysicsBounds(loadFlags & FLAG_OVERRIDE_PHYSICS_BOUNDS);
	SetHasPreReflectedWaterProxy(loadFlags & FLAG_HAS_PRE_REFLECTED_WATER_PROXY);
	SetAutoStartAnim( (loadFlags & FLAG_AUTOSTART_ANIM)!=0 );
	SetHasAlphaShadow( (loadFlags & FLAG_HAS_ALPHA_SHADOW));

	SetNeverDummyFlag((loadFlags & FLAG_HAS_CLOTH) != 0);		//environmental cloth only works for real objects rather than dummy objects

	//I don't know how it got removed!!!!
	SetUseAmbientScale(loadFlags & FLAG_USE_AMBIENT_SCALE);

	//	GW - Animated objects seem to be causing a crash so I've turned them off until Adam can have a look at it
	SetHasAnimation(loadFlags & FLAG_HAS_ANIM);
	SetHasUvAnimation(loadFlags & FLAG_HAS_UVANIM);
	SetIsHDTxdCapable(!(loadFlags & FLAG_SUPPRESS_HD_TXDS));

	SetHasDrawableProxyForWaterReflections( (loadFlags & FLAG_HAS_DRAWABLE_PROXY_FOR_WATER_REFLECTIONS)!=0 );

	if(attributes == MODEL_ATTRIBUTE_IS_TREE_DEPRECATED)
		SetIsTree(true);
	if(loadFlags & FLAG_IS_LADDER_DEPRECATED)
		attributes = MODEL_ATTRIBUTE_IS_LADDER;
	SetAttributes(attributes);

	if(TestAttribute(MODEL_ATTRIBUTE_IS_TRAFFIC_LIGHT))
	{
		SetHasPreRenderEffects();
	}
}

void CBaseModelInfo::InitArchetypeFromDefinition(strLocalIndex mapTypeDefIndex, fwArchetypeDef* definition, bool bHasAssetReferences)
{
	fwArchetype::InitArchetypeFromDefinition(mapTypeDefIndex, definition, bHasAssetReferences);
	CBaseArchetypeDef& def = *smart_cast<CBaseArchetypeDef*>(definition);

	u32 specialAttribute = def.m_specialAttribute;

#if !__FINAL
	if(def.m_flags & FLAG_IS_DEBUG)
	{
		specialAttribute = MODEL_ATTRIBUTE_IS_DEBUG;
	}
#endif

	if (!((specialAttribute << ATOMIC_L_BITSHIFT) < (u32) BIT(31)))
	{
		Warningf("special attribute bits 5+ are set (%d) in %s (from %s.ityp)", specialAttribute, GetModelName(), g_MapTypesStore.GetName(mapTypeDefIndex));
		specialAttribute &= 31;
	}

	ConvertLoadFlagsAndAttributes(def.m_flags, specialAttribute);

#if NORTH_CLOTHS && __ASSERT
	// Bendables: flag FLAG_IS_TYPE_OBJECT (MODEL_IS_TYPE_OBJECT) MUST be set whenever MODEL_ATTRIBUTE_IS_BENDABLE_PLANT is set:
	if(GetAttributes()==MODEL_ATTRIBUTE_IS_BENDABLE_PLANT)
	{
		Assertf(GetIsTypeObject(),"IDE: Object %s is attributed as bendable plant, but its MODEL_IS_TYPE_OBJECT flag is not set.", def.m_name.c_str());
		SetIsTypeObject(TRUE);	// quick fix
	}
#endif

#if __BANK
	if(PARAM_audiotag.Get())
	{
		// CModelInfo::SetAudioCollisionSettings
		const u32 maxAudioSettingsNameLength		= 64;
		const char *audioSettingsNamePrefix	= "MOD_";
		char audioSettingsName[maxAudioSettingsNameLength];
		strncpy(audioSettingsName, audioSettingsNamePrefix, maxAudioSettingsNameLength);
		strncat(audioSettingsName, definition->m_name.GetCStr(), maxAudioSettingsNameLength);
		ModelAudioCollisionSettings * moda = audNorthAudioEngine::GetObject<ModelAudioCollisionSettings>(audStringHash(audioSettingsName));
		SetAudioCollisionSettings(moda, 0);
	}
#endif

}

fwEntity* CBaseModelInfo::CreateEntity()
{
	CEntity* entity;

	if (GetIsTypeObject())
	{
		entity = rage_new CDummyObject( ENTITY_OWNEDBY_IPL );

		if(!entity)
			modelinfoErrorf("CDummyObject pool is full");
	}
	else
	{
		if (GetModelType() == MI_TYPE_COMPOSITE){
			entity = rage_new CCompEntity( ENTITY_OWNEDBY_IPL );
			modelinfoAssertf(entity,"CCompEntity pool is full");
		}
		else if(GetClipDictionaryIndex() != -1 && GetHasAnimation())
		{
			entity = rage_new CAnimatedBuilding( ENTITY_OWNEDBY_IPL );
			modelinfoAssertf(entity,"CAnimatedBuilding pool is full");
		}
		else
		{
			entity = rage_new CBuilding( ENTITY_OWNEDBY_IPL );
			modelinfoAssertf(entity,"CBuilding pool is full");
		}
	}

	return entity;
}

fwEntity* CBaseModelInfo::CreateEntityFromDefinition(const fwPropInstanceListDef* UNUSED_PARAM(definition))
{
	return rage_new CEntityBatch( ENTITY_OWNEDBY_IPL );
}

fwEntity* CBaseModelInfo::CreateEntityFromDefinition(const fwGrassInstanceListDef* UNUSED_PARAM(definition))
{
	return rage_new CGrassBatch( ENTITY_OWNEDBY_IPL );
}

#if USE_DEBUG_ARCHETYPE_PROXIES
PARAM(CheckDynArch,"");
#endif // USE_DEBUG_ARCHETYPE_PROXIES

#if __BANK
void CBaseModelInfo::DebugPostInit() const
{
#if USE_DEBUG_ARCHETYPE_PROXIES
	if (PARAM_CheckDynArch.Get())
	{
		CDebugArchetype::CheckDebugArchetypeProxy(this);
	}
#endif // USE_DEBUG_ARCHETYPE_PROXIES
}
#endif // __BANK

//
// name:		Init
// description:	shutdown base modelinfo
void CBaseModelInfo::Shutdown()
{
	fwArchetype::Shutdown();

	modelinfoAssertf(GetNumRefs() == 0, "Can't delete %s as it is still referenced", GetModelName());

	DeleteMasterFragData();
	DeleteMasterDrawableData();

	if (m_p2dEffectPtrs){
		delete m_p2dEffectPtrs;
		m_p2dEffectPtrs = NULL;
	}

	if(m_pBuoyancyInfo)
	{
		DeleteWaterSamples();
	}
}

void CBaseModelInfo::InitMasterDrawableData(u32 modelidx)
{
	SetHasLoaded(true);
	rmcDrawable* pDrawable = GetDrawable();		// lookup from internal indices 
	if (!pDrawable)
		SetHasLoaded(false);

	modelinfoAssert(pDrawable);

#if __ASSERT
	if (GetNumRefs() != 0)
	{
		// Something is wrong - we're supposed to have a reference count of 0. Let's find out what's going on here.
		strStreamingModule*	streamingModule = fwArchetypeManager::GetStreamingModule();
		fwModelId modelId((strLocalIndex(modelidx)));
		strLocalIndex objIndex = modelId.ConvertToStreamingIndex();
		strIndex archIndex = streamingModule->GetStreamingIndex(objIndex);
		int strNumRefs = streamingModule->GetNumRefs(objIndex);

		modelinfoErrorf("Reference count mismatch for %s (strIndex %d) - streaming ref count is %d. Dependencies below:", GetModelName(), archIndex.Get(), strNumRefs);
		strStreamingEngine::GetInfo().PrintDependenciesRecurse(archIndex);
		modelinfoErrorf("Dependents:");
		strStreamingEngine::GetInfo().PrintAllDependents(archIndex);
	}
#endif // __ASSERT

	modelinfoAssertf(GetNumRefs() == 0, "Trying to initialize drawable data for archetype %s, but the reference count is %d instead of zero. See TTY for details.",
		GetModelName(), GetNumRefs());

	// get bounds if we don't already have them
	rmcLodGroup& pLodGroup = GetDrawable()->GetLodGroup();
	if(GetBoundingSphereRadius() == 0.0f)
	{
		SetBoundingSphere(pLodGroup.GetCullSphere(),pLodGroup.GetCullRadius());
		
		Vector3 bbMin, bbMax;
		pLodGroup.GetBoundingBox(bbMin, bbMax);
		SetBoundingBox(spdAABB(RCC_VEC3V(bbMin), RCC_VEC3V(bbMax)));
	}

	SetupCustomShaderEffects();

	//compute draw bucket mask
	rmcLodGroup &lodGroup = pDrawable->GetLodGroup();
	lodGroup.ComputeBucketMask(pDrawable->GetShaderGroup());

	artAssertf(GetAssetParentTxdIndex() < 0 || !GetIsLod() || CStreaming::GetObjectPhysicalSize(strLocalIndex(GetAssetParentTxdIndex()), g_TxdStore.GetStreamingModuleId()) < MAX_LOD_TXD_SIZE,
		"Lod texture dictionary %s used by %s is too large %dMB",
		g_TxdStore.GetName(strLocalIndex(GetAssetParentTxdIndex())),
		GetModelName(),
		CStreaming::GetObjectPhysicalSize(strLocalIndex(GetAssetParentTxdIndex()), g_TxdStore.GetStreamingModuleId())>>10);

#if NORTH_CLOTHS
	if(GetAttributes() == MODEL_ATTRIBUTE_IS_BENDABLE_PLANT)
	{
		InitClothArchetype();
	}
#endif // NORTH_CLOTHS
	
	if( GetCarryScriptedRT() && !gRenderTargetMgr.IsNamedRenderTargetLinked(modelidx, m_scriptId))
	{
		CRenderTargetMgr::namedRendertarget const* pRT = gRenderTargetMgr.LinkNamedRendertargets(modelidx,m_scriptId);
		if(pRT)
		{
#if GTA_REPLAY
			// Fix for url:bugstar:5835869 - make sure the RT linkup is recorded
			CBaseModelInfo *modelinfo = CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(modelidx)));
			ReplayMovieControllerNew::OnLinkNamedRenderTarget(modelinfo->GetModelNameHash(), m_scriptId, pRT);
#endif // GTA_REPLAY
		}
	}

	const atArray<CLightAttr>* pLightArray = GetLightArray();
	if(pLightArray && pLightArray->GetCount() > 0)
	{
		SetHasPreRenderEffects();
	}

#if __BANK || (__WIN32PC && !__FINAL)
	SetIsProp(false);
	u16 index = 0xffff;
	strStreamingModule* module = NULL;
	switch (GetDrawableType())
	{
		case DT_FRAGMENT:
			module = &g_FragmentStore;
			index = static_cast<u16>(GetFragmentIndex());
			break;
		case DT_DRAWABLE:
			module = &g_DrawableStore;
			index = static_cast<u16>(GetDrawableIndex());
			break;
		case DT_DRAWABLEDICTIONARY:
			module = &g_DwdStore;
			index = static_cast<u16>(GetDrawDictIndex());
			break;
		default:
			break;
	} 
	if (module && index != 0xffff)
	{
		strStreamingInfo* info = module->GetStreamingInfo(strLocalIndex(index));
		if (info)
		{
			s32 imageIndex = strPackfileManager::GetImageFileIndexFromHandle(info->GetHandle()).Get();
			if (imageIndex != -1)
			{
				strStreamingFile* file = strPackfileManager::GetImageFile(imageIndex);
				if (file && file->m_contentsType == CDataFileMgr::CONTENTS_PROPS)
					SetIsProp(true);
			}
		}
	}
#endif // __BANK
}

//
// name:		DeleteDrawable
// description:	Delete the drawable for this modelinfo
void CBaseModelInfo::DeleteMasterDrawableData()
{
	modelinfoAssertf(GetNumRefs() == 0, "Trying to delete drawable data for archetype %s, but the reference count is still %d instead of zero.",
		GetModelName(), GetNumRefs());

	if (m_pShaderEffectType)
	{
		m_pShaderEffectType->RestoreModelInfoDrawable(GetDrawable());
		m_pShaderEffectType->RemoveRef();
		m_pShaderEffectType = NULL;

		SetHasLoaded(false);
	}

#if NORTH_CLOTHS
	if(GetAttributes() == MODEL_ATTRIBUTE_IS_BENDABLE_PLANT)
	{
		DeleteClothArchetype();
	}
#endif //NORTH_CLOTHS
}



//
//
//
//
bool CBaseModelInfo::SetupCustomShaderEffects()
{
	rmcDrawable *pDrawable = GetDrawable();
	if (pDrawable == NULL){
		return(false);
	}

#if __ASSERT
	extern char *CCustomShaderEffectPed_EntityName;	// debug only: defined in CCustomShaderEffectPed.cpp
	CCustomShaderEffectPed_EntityName = (char*)this->GetModelName();

	extern char* CCustomShaderEffectBase_EntityName;
	CCustomShaderEffectBase_EntityName = (char*)this->GetModelName(); // debug only: defined in CCustomShaderEffectBase.cpp
#endif //__ASSERT...

	m_pShaderEffectType = CCustomShaderEffectBaseType::SetupMasterForModelInfo(this);

	if(m_pShaderEffectType)
	{
		artAssertf(m_pShaderEffectType->AreShadersValid(pDrawable), "%s: Model has wrong shaders applied", GetModelName());
	}

	return(TRUE);
}

//
//
//
//
void CBaseModelInfo::DestroyCustomShaderEffects()
{
	if (m_pShaderEffectType)
	{
		m_pShaderEffectType->RestoreModelInfoDrawable(GetDrawable());
		m_pShaderEffectType->RemoveRef();
		m_pShaderEffectType = NULL;
	}
}

void CBaseModelInfo::SetupDynamicCoverCollisionBounds(phArchetypeDamp* pPhysicsArchetype)
{
	// This function is meant to be called on frags which have extra bounds intended as cover bounds which must
	// then be disabled entirely once the frag has broken due to having a size greater than the remaining pieces.

	physicsAssert(pPhysicsArchetype);
	physicsAssert(pPhysicsArchetype->GetBound());
	if(artVerifyf(pPhysicsArchetype->GetBound()->GetType()==phBound::COMPOSITE,
		"An object(%s) without a composite bound shouldn't be tagged with model attribute: 'MODEL_ATTRIBUTE_HAS_DYNAMIC_COVER_BOUND'.",
		GetModelName()))
	{
		// Take a copy of the archetype type / include flags to use in the case where the per-component type / include
		// flags have not been set up.
		u32 nArchTypeFlags = pPhysicsArchetype->GetTypeFlags();
		u32 nArchIncludeFlags = pPhysicsArchetype->GetIncludeFlags();
		bool bNeedToInitFlags = false;

		phBoundComposite* pCompBound = static_cast<phBoundComposite*>(pPhysicsArchetype->GetBound());
		if(!pCompBound->GetTypeAndIncludeFlags())
		{
			pCompBound->AllocateTypeAndIncludeFlags();
			bNeedToInitFlags = true;
		}
		for(int nComponent = 0; nComponent < pCompBound->GetNumBounds(); ++nComponent)
		{
			if(phBound* pBound = pCompBound->GetBound(nComponent))
			{
				for(int nMatIdx = 0; nMatIdx < pBound->GetNumMaterials(); ++nMatIdx)
				{
					phMaterialMgr::Id nMaterialId = pBound->GetMaterialId(nMatIdx);
					phMaterialMgr::Id nUnpackedMaterialId = phMaterialMgrGta::UnpackMtlId(nMaterialId);
					if(nUnpackedMaterialId == PGTAMATERIALMGR->g_idPhysDynamicCoverBound)
					{
						// We have found a dynamic cover bound, set up the type / include flags accordingly.
						if(physicsVerifyf(pCompBound->GetTypeAndIncludeFlags(), "No composite bound type/include flags array allocated for %s.", GetModelName()))
						{
							pCompBound->SetTypeFlags(nComponent, ArchetypeFlags::GTA_MAP_TYPE_COVER);
							pCompBound->SetIncludeFlags(nComponent, ArchetypeFlags::GTA_AI_TEST);

							u32 nTypeFlags = pPhysicsArchetype->GetTypeFlags();
							u32 nIncludeFlags = pPhysicsArchetype->GetIncludeFlags();
							pPhysicsArchetype->SetTypeFlags(nTypeFlags|ArchetypeFlags::GTA_MAP_TYPE_COVER);
							pPhysicsArchetype->SetIncludeFlags(nIncludeFlags|ArchetypeFlags::GTA_AI_TEST);
#if __ASSERT
							const fragPhysicsLOD* physicsLOD = GetFragType()->GetPhysics(0);
							const fragTypeChild& child = *physicsLOD->GetChild(nComponent);
							Assertf(child.GetDamagedEntity() == NULL, "Trying to set cover bounds on a damageable group '%s' of '%s'. This isn't supported.", physicsLOD->GetGroup(child.GetOwnerGroupPointerIndex())->GetDebugName(),pPhysicsArchetype->GetFilename());
#endif // __ASSERT
						}
					}
					else if(bNeedToInitFlags)
					{
						pCompBound->SetTypeFlags(nComponent, nArchTypeFlags);
						pCompBound->SetIncludeFlags(nComponent, nArchIncludeFlags);
					}
				}
			}
		}
	}
}

void CBaseModelInfo::ClearDynamicCoverCollisionBounds(phArchetypeDamp* pPhysicsArchetype)
{
	// This function is meant to be called on frags which have extra bounds intended as cover bounds which must
	// then be disabled entirely once the frag has broken due to having a size greater than the remaining pieces.

	physicsAssert(pPhysicsArchetype);
	physicsAssert(pPhysicsArchetype->GetBound());
	if(artVerifyf(pPhysicsArchetype->GetBound()->GetType()==phBound::COMPOSITE,
		"An object(%s) without a composite bound shouldn't be tagged with model attribute: 'MODEL_ATTRIBUTE_HAS_DYNAMIC_COVER_BOUND'.",
		GetModelName()))
	{
		// If we don't find any bounds with GTA_MAP_TYPE_COVER used, we can get rid of this from the archetype flags.
		bool bCoverTypeUsed = false;

		phBoundComposite* pCompBound = static_cast<phBoundComposite*>(pPhysicsArchetype->GetBound());
		physicsAssertf(pCompBound->GetTypeAndIncludeFlags(), "Model %s has no type and include flag array in its phBoundComposite.", GetModelName());
		for(int nComponent = 0; nComponent < pCompBound->GetNumBounds(); ++nComponent)
		{
			phBound* pBound = pCompBound->GetBound(nComponent);
			if(pBound)
			{
				for(int nMatIdx = 0; nMatIdx < pBound->GetNumMaterials(); ++nMatIdx)
				{
					phMaterialMgr::Id nMaterialId = pBound->GetMaterialId(nMatIdx);
					phMaterialMgr::Id nUnpackedMaterialId = phMaterialMgrGta::UnpackMtlId(nMaterialId);
					if(nUnpackedMaterialId == PGTAMATERIALMGR->g_idPhysDynamicCoverBound)
					{
						// We have found a dynamic cover bound, clear the type / include flags accordingly.
						pCompBound->SetTypeFlags(nComponent, 0);
						pCompBound->SetIncludeFlags(nComponent, 0);
					}
				}

				if(pCompBound->GetTypeFlags(nComponent)&ArchetypeFlags::GTA_MAP_TYPE_COVER)
					bCoverTypeUsed = true;
			}
		}

		if(!bCoverTypeUsed)
		{
			u32 nTypeFlags = pPhysicsArchetype->GetTypeFlags();
			pPhysicsArchetype->SetTypeFlags(nTypeFlags & ~ArchetypeFlags::GTA_MAP_TYPE_COVER);
		}
	}
}

//
// name:		SetPhysics
// description:	Set models physics bounds
//
#define DEFAULT_OBJECT_DRAG_COEFF (0.05f)
#define DEFAULT_OBJECT_DOOR_ROT_DAMP (0.01f)
//
#define MAX_NUM_POLYS_BEFORE_GETTING_NERFED 512

void CBaseModelInfo::SetPhysics(phArchetypeDamp* pPhysics)
{	
	fwArchetype::SetPhysics(pPhysics);

	if(pPhysics != NULL)
	{
		if(pPhysics->GetBound())
		{
#if __DEV
			if(GetBoundingSphereRadius() > 50.0f && !GetIsFixed())
				modelinfoWarningf("m_bsRadius %f > 50.0f : %s : Bounding radius too big for dynamic object", GetBoundingSphereRadius(), GetModelName());
#endif
			if(pPhysics->GetBound()->GetType()==phBound::GEOMETRY)
			{
				// 				if (!Verifyf(nPolys < MAX_NUM_POLYS_BEFORE_GETTING_NERFED, "%s has WAY TOO MANY POLYS %d in its bounds, bound is being nerfed to prevent crashes", GetModelName(), nPolys))
				// 				{
				// 					((phBoundGeometry*)pPhysics->GetBound())->ClearPolysAndVerts();
				// 				}
#if __ASSERT
 				int nPolys = ((phBoundGeometry*)pPhysics->GetBound())->GetNumPolygons();
				if(nPolys > 128)
				{
					if(PARAM_propasserts.Get())
						physicsAssertf(false, "%s has too many polys %d in its bounds", GetModelName(), nPolys);
					else
						physicsDisplayf("%s has too many polys %d in its bounds", GetModelName(), nPolys);
				}
#endif // __ASSERT		
			}
			else if(pPhysics->GetBound()->GetType()==phBound::COMPOSITE)
			{
				ASSERT_ONLY(int nTotalPolys = 0;)
				phBoundComposite* pBoundComposite = (phBoundComposite*)pPhysics->GetBound();
				for(int i=0; i<pBoundComposite->GetNumBounds(); i++)
				{
					if(pBoundComposite->GetBound(i) && pBoundComposite->GetBound(i)->GetType()==phBound::GEOMETRY)
					{
// 						int nPolys = ((phBoundGeometry*)pBoundComposite->GetBound(i))->GetNumPolygons();
// 						if (!Verifyf(nPolys < MAX_NUM_POLYS_BEFORE_GETTING_NERFED, "%s has WAY TOO MANY POLYS %d in its bounds, bound is being nerfed to prevent crashes", GetModelName(), nPolys))
// 						{
// 							((phBoundGeometry*)pBoundComposite->GetBound(i))->ClearPolysAndVerts();
// 						}
 #if __ASSERT
// 						else if(nPolys > 32)
// 						{
// 							if(PARAM_propasserts.Get())
// 								physicsAssertf(false, "%s has too many polys %d in one of its bounds", GetModelName(), nPolys);
// 							else
// 								physicsDisplayf("%s has too many polys %d in one of its bounds", GetModelName(), nPolys);
// 						}
						nTotalPolys += ((phBoundGeometry*)pBoundComposite->GetBound(i))->GetNumPolygons();
#endif // __ASSERT		
					}
#if __ASSERT
					// count other types (box, capsule, sphere) as cost of 2 polys (wild approximation)
					else
						nTotalPolys += 2;
#endif // __ASSERT		
				}
#if __ASSERT
				if(nTotalPolys > 256)
				{
					if(PARAM_propasserts.Get())
						physicsAssertf(false, "%s has too many polys (or equivalent) %d in total in its composite  bound", GetModelName(), nTotalPolys);
					else
						physicsDisplayf("%s has too many polys (or equivalent) %d in total in its composite  bound", GetModelName(), nTotalPolys);
				}
#endif // __ASSERT		
			}
		}

		if (GetIsTypeObject())
		{

			if(!GetIsFixed())
			{
#if __ASSERT
				if(pPhysics->GetBound()->GetType()==phBound::COMPOSITE)
				{
					phBoundComposite* pBoundComposite = (phBoundComposite*)pPhysics->GetBound();
					for(int i=0; i<pBoundComposite->GetNumBounds(); i++)
						modelinfoAssertf(pBoundComposite->GetBound(i)->IsConvex(), "%s:Bound is convex", GetModelName());

				}
				else
					modelinfoAssertf(pPhysics->GetBound()->IsConvex(), "%s:Bound is convex", GetModelName());
#endif


				if(GetUsesDoorPhysics())
				{
					pPhysics->GetBound()->SetCGOffset(Vec3V(V_ZERO));
				}

				const CTunableObjectEntry* pTuning = CTunableObjectManager::GetInstance().GetTuningEntry(GetModelNameHash());
				ComputeMass(pPhysics, pTuning);
				ComputeDamping(pPhysics, pTuning);
			}

			pPhysics->SetTypeFlags(ArchetypeFlags::GTA_OBJECT_TYPE);

			if(GetUsesDoorPhysics())
				pPhysics->SetIncludeFlags(ArchetypeFlags::GTA_DOOR_OBJECT_INCLUDE_TYPES);
			else
				pPhysics->SetIncludeFlags(ArchetypeFlags::GTA_OBJECT_INCLUDE_TYPES);
		}
		else if(GetModelType()==MI_TYPE_BASE || GetModelType()==MI_TYPE_MLO)
		{
#if __DEV
//			if(GetBoundingSphereRadius() > 100.0f && !GetIsFixed())
//				modelinfoWarningf("m_bsRadius %f > 100.0f : %s : Bounding radius too big for dynamic object", GetBoundingSphereRadius(), GetModelName());
#endif
			// Set the top-level type / include flags for non-composite bounds here.
			if(pPhysics->GetBound()->GetType()!=phBound::COMPOSITE)
			{
				pPhysics->SetTypeFlags(ArchetypeFlags::GTA_MAP_TYPE_MOVER|ArchetypeFlags::GTA_MAP_TYPE_WEAPON|ArchetypeFlags::GTA_MAP_TYPE_HORSE
					|ArchetypeFlags::GTA_MAP_TYPE_COVER|ArchetypeFlags::GTA_MAP_TYPE_VEHICLE);
				pPhysics->SetIncludeFlags(ArchetypeFlags::GTA_BUILDING_INCLUDE_TYPES);
			}
		}

		// Any objects coming through with this model attribute set should have its collision bound type set to GTA_FOLIAGE_TYPE.
		if(GetAttributes()==MODEL_ATTRIBUTE_NOISY_BUSH || GetAttributes()==MODEL_ATTRIBUTE_NOISY_AND_DEFORMABLE_BUSH)
		{
			pPhysics->SetTypeFlags(ArchetypeFlags::GTA_FOLIAGE_TYPE);
			pPhysics->SetIncludeFlags(ArchetypeFlags::GTA_OBJECT_INCLUDE_TYPES & ~ArchetypeFlags::GTA_WHEEL_TEST);
		}

		// setup material flags on this bound
		if(pPhysics->GetBound())
			CopyMaterialFlagsToBound(pPhysics->GetBound());

		if(GetUsesDoorPhysics())
		{
			static strStreamingObjectName modelNameNeedsSyncInNetworkGame("v_ilev_j2_door",0x4F97336B);
			if(modelNameNeedsSyncInNetworkGame.GetHash() == GetModelNameHash())
			{
				SetSyncObjInNetworkGame(true);
			}
		}

		if (GetIsTypeObject() && !GetSyncObjInNetworkGame() && !GetUsesDoorPhysics()
		&& !TestAttribute(MODEL_ATTRIBUTE_IS_TRAFFIC_LIGHT) && !TestAttribute(MODEL_ATTRIBUTE_IS_STREET_LIGHT))
		{
			if(GetNum2dEffectType(ET_EXPLOSION) > 0)
				SetSyncObjInNetworkGame(true);

			if(!GetSyncObjInNetworkGame())
			{
				if(pPhysics->GetMass() > MIN_MASS_FOR_NETWORK_SYNC)
				{
					if(pPhysics->GetBound())
					{
						Vector3 vecBBoxExtents = VEC3V_TO_VECTOR3(pPhysics->GetBound()->GetBoundingBoxMax() - pPhysics->GetBound()->GetBoundingBoxMin());
						if(vecBBoxExtents.x > MIN_SIZE_FOR_NETWORK_SYNC || vecBBoxExtents.y > MIN_SIZE_FOR_NETWORK_SYNC || vecBBoxExtents.z > MIN_SIZE_FOR_NETWORK_SYNC)
						{
							SetSyncObjInNetworkGame(true);
						}
					}
				}
			}
		}

		// Setup extra collision bounds for any props meant for use with the forklift. We give them an extra bound here which
		// covers all the small collision bounds which can cause the prop to be unstable when colliding with other objects. This
		// extra box will have its collision flags set such that it will not collide with the forklift forks (which have a
		// special archetype flag), but will collide with all other objects.
		/*if(const CTunableObjectEntry* pTuning = CTunableObjectManager::GetInstance().GetTuningEntry(GetModelNameHash()))
		{
			if(pTuning->CanBePickedUpByForklift())
			{
				CVehicleGadgetForks::CreateNewBoundsForPallet(pPhysics, pTuning->GetForkliftAttachBoxMinZ(), pTuning->GetForkliftAttachBoxMaxZ(),
					pTuning->GetForkliftAddTopAndBottomExtraBoxes());
			}
		}*/

		if(GetIsTypeObject() && pPhysics->GetBound()->GetType()==phBound::COMPOSITE)
		{
			phBoundComposite* pBoundComposite = (phBoundComposite*)pPhysics->GetBound();
			if(pBoundComposite->GetTypeAndIncludeFlags())
			{
				if(!GetUsesDoorPhysics())
				{
					u32 nTypeFlags = pPhysics->GetTypeFlags();
					u32 nNewArchetypeTypeFlags = nTypeFlags;
					u32 nIncludeFlags = pPhysics->GetIncludeFlags();
					for(int i=0; i<pBoundComposite->GetNumBounds(); ++i)
					{
						u32 nThisComponentTypeFlags = pBoundComposite->GetTypeFlags(i);
						nNewArchetypeTypeFlags |= nThisComponentTypeFlags;

						// Stair slope is incompatible with object type.
						if(nThisComponentTypeFlags==ArchetypeFlags::GTA_STAIR_SLOPE_TYPE)
						{
							pBoundComposite->SetTypeFlags(i, nThisComponentTypeFlags);
							pBoundComposite->SetIncludeFlags(i, nIncludeFlags);
						}
						else if(nThisComponentTypeFlags == (ArchetypeFlags::GTA_MAP_TYPE_COVER|ArchetypeFlags::GTA_OBJECT_TYPE)
							|| nThisComponentTypeFlags == ArchetypeFlags::GTA_MAP_TYPE_COVER)
						{
							pBoundComposite->SetTypeFlags(i, nTypeFlags|nThisComponentTypeFlags);
							pBoundComposite->SetIncludeFlags(i, nIncludeFlags & ~(ArchetypeFlags::GTA_PROJECTILE_TYPE|ArchetypeFlags::GTA_WEAPON_TEST));
						}
						else
						{
							pBoundComposite->SetTypeFlags(i, nTypeFlags|nThisComponentTypeFlags);
							pBoundComposite->SetIncludeFlags(i, nIncludeFlags);
						}
					}

					// Set the top-level type / include flags for non-composite objects here.
					pPhysics->SetTypeFlags(nNewArchetypeTypeFlags);
				}
				else
				{
					// Clear out any per-component type/include flag information for Doors.
					u32 nTypeFlags = pPhysics->GetTypeFlags();
					u32 nIncludeFlags = pPhysics->GetIncludeFlags();
					for(int i=0; i<pBoundComposite->GetNumBounds(); ++i)
					{
						pBoundComposite->SetTypeFlags(i, nTypeFlags);
						pBoundComposite->SetIncludeFlags(i, nIncludeFlags);
					}
				}
			}
		}
	}
	else	// If physArch set to null
	{
		//Displayf("CBaseModelInfo::SetPhysics Null physics for %s", GetModelName());
		DeleteWaterSamples();
	}
}


void CBaseModelInfo::CreateBuoyancyIfNeeded()
{
	if(GetPhysics() != NULL)
	{
		if((GetIsTypeObject() && !GetIsFixed())
			|| GetModelType() == MI_TYPE_PED
			|| GetModelType() == MI_TYPE_VEHICLE)
		{
			Assert(m_pBuoyancyInfo == NULL);
			InitWaterSamples();
		}
	}
}

void CBaseModelInfo::CopyMaterialFlagsToSingleBound(phBound* pBound)
{
	Assert(pBound->GetType() != phBound::COMPOSITE);
	for(int nMat = 0; nMat < pBound->GetNumMaterials(); nMat++)
	{
		phMaterialMgr::Id nMaterialId = pBound->GetMaterialId(nMat);

		Assertf(PGTAMATERIALMGR->GetPolyFlagVehicleWheel(nMaterialId) == false, "POLYFLAG_VEHICLE_WHEEL shouldn't be set on resources. Material Id(0x%" I64FMT "X) Model Name(%s)", nMaterialId, GetModelName());
		
		if(PGTAMATERIALMGR->GetMtlFlagSeeThrough(nMaterialId))
			PGTAMATERIALMGR->PackPolyFlag(nMaterialId, POLYFLAG_SEE_THROUGH);

		if(PGTAMATERIALMGR->GetMtlFlagShootThroughFx(nMaterialId))
		{
			// If this material has shoot through FX, remove POLYFLAG_SHOOT_THROUGH and add POLYFLAG_SHOOT_THROUGH_FX
			nMaterialId &= ~PGTAMATERIALMGR->GetPackedPolyFlagValue(POLYFLAG_SHOOT_THROUGH);
			PGTAMATERIALMGR->PackPolyFlag(nMaterialId,POLYFLAG_SHOOT_THROUGH_FX);
		}
		else if(PGTAMATERIALMGR->GetMtlFlagShootThrough(nMaterialId))
		{
			PGTAMATERIALMGR->PackPolyFlag(nMaterialId, POLYFLAG_SHOOT_THROUGH);
		}

		Assertf(!(PGTAMATERIALMGR->GetPolyFlagShootThrough(nMaterialId) && PGTAMATERIALMGR->GetPolyFlagShootThroughFx(nMaterialId)), "POLYFLAG_SHOOT_THROUGH and POLYFLAG_SHOOT_THROUGH_FX should be exclusive. Material Id(0x%" I64FMT "X) Model Name(%s)", nMaterialId, GetModelName());

		pBound->SetMaterial(nMaterialId, nMat);
	}
}

void CBaseModelInfo::CopyMaterialFlagsToBound(phBound* pBound)
{
	if(pBound->GetType()==phBound::COMPOSITE)
	{
		phBoundComposite* pBoundComposite = (phBoundComposite*)pBound;
		for(int nComponent = 0; nComponent < pBoundComposite->GetNumBounds(); nComponent++)
		{
			phBound* pBoundPart = pBoundComposite->GetBound(nComponent);
			if(pBoundPart)
			{
				CopyMaterialFlagsToSingleBound(pBoundPart);
			}
		}
	}
	else
	{
		CopyMaterialFlagsToSingleBound(pBound);
	}
}


void CBaseModelInfo::ComputeMass(phArchetypeDamp* pPhysicsArchetype, const CTunableObjectEntry* pTuning) const
{
	float fDensity = 1.0f;
	int iNumMats = pPhysicsArchetype->GetBound()->GetNumMaterials();
	modelinfoAssertf(iNumMats, "Bound has no materials : \"%s\"", GetModelName());
	if(iNumMats > 0)
	{
		// Don't want the material of the first component bound dominating the overall mass calculation. Instead we
		// average the density of all materials in the composite, weighting each one by the volume of its bound.
		if(pPhysicsArchetype->GetBound()->GetType()==phBound::COMPOSITE)
		{
			float fTotalVolume = 0.0f;
			phBoundComposite* pCompBound = static_cast<phBoundComposite*>(pPhysicsArchetype->GetBound());
			fDensity = 0.0f;
			for(int nComponent = 0; nComponent < pCompBound->GetNumBounds(); ++nComponent)
			{
				phBound* pBound = pCompBound->GetBound(nComponent);
				float fVolume = pBound->GetVolume();
				// Take the density from the first material on the bound.
				phMaterialGta* pMat = (phMaterialGta*)&(pBound->GetMaterial(0));
				fDensity += pMat->GetDensity()*fVolume;
				fTotalVolume += fVolume;
			}

			// Compute the weighted average density.
			fDensity /= fTotalVolume;
		}
		else
		{
			phMaterialGta *pMat = (phMaterialGta *)&(pPhysicsArchetype->GetBound()->GetMaterial(0));
			fDensity = pMat->GetDensity();
		}
	}

	float fVolume = pPhysicsArchetype->GetBound()->GetComputeVolume();
	modelinfoAssertf(fVolume > 0.0f || GetIsFixed(), "%s:Negative volume calculated for dynamic object", GetModelName());
	fVolume = fabs(fVolume);
	float fMass = fVolume * fDensity;

	if(GetUsesDoorPhysics())
	{
		bool bBigDoor = pPhysicsArchetype->GetBound()->GetBoundingBoxMax().GetXf() - pPhysicsArchetype->GetBound()->GetBoundingBoxMin().GetXf() > 2.0f;

		if(bBigDoor)
			fMass *= 0.3f;
		else
			fMass *= 0.1f;

		//See if there is a door mass multiplier
		u8 type = CDoor::DetermineDoorType(this, GetBoundingBoxMin(), GetBoundingBoxMax());
		const CDoorTuning& tuneData = CDoorTuningManager::GetInstance().GetTuningForDoorModel(GetModelNameHash(), type);
		fMass *= tuneData.m_MassMultiplier;
		pPhysicsArchetype->GetBound()->SetCGOffset(Vec3V(V_ZERO));

		if(bBigDoor)
			fMass = rage::Clamp(fMass, 50.0f, 100.0f);
		else
			fMass = rage::Clamp(fMass, 35.0f, 70.0f);
	}

	// Check for a tuning override
	if(pTuning)
	{
		if(pTuning->GetMass() >= 0)
		{
			fMass = pTuning->GetMass();
		}
	}

	// Set the mass and angular inertia of the archetype
	pPhysicsArchetype->SetMassOnly(fMass);
	pPhysicsArchetype->SetAngInertia(pPhysicsArchetype->GetBound()->GetComputeAngularInertia(fMass));

	// Check if we should offset the centre of mass for this object.
	if(pTuning)
	{
		if(pTuning->GetCentreOfMassOffset().IsNonZero())
		{
			pPhysicsArchetype->GetBound()->SetCGOffset(RCC_VEC3V(pTuning->GetCentreOfMassOffset()));
		}
	}
}

bool CBaseModelInfo::ComputeDamping(phArchetypeDamp* pPhysicsArchetype, const CTunableObjectEntry* pTuning) const
{
	Vector3 vecDragCoef;
	vecDragCoef.Set(DEFAULT_OBJECT_DRAG_COEFF);
	pPhysicsArchetype->ActivateDamping(phArchetypeDamp::LINEAR_V2, vecDragCoef);

	if(GetUsesDoorPhysics())
	{
		vecDragCoef.Set(DEFAULT_OBJECT_DOOR_ROT_DAMP);
		pPhysicsArchetype->ActivateDamping(phArchetypeDamp::ANGULAR_V, vecDragCoef);
	}

	// Check for a tuning override
	bool bAppliedTuning = false;
	if(pTuning)
	{
		Vector3 vZero(0,0,0);
		if(pTuning->GetLinearSpeedDamping().IsGreaterOrEqualThan(vZero))
		{
			pPhysicsArchetype->ActivateDamping(phArchetypeDamp::LINEAR_C,pTuning->GetLinearSpeedDamping());
			bAppliedTuning = true;
		}
		if(pTuning->GetLinearVelocityDamping().IsGreaterOrEqualThan(vZero))
		{
			pPhysicsArchetype->ActivateDamping(phArchetypeDamp::LINEAR_V,pTuning->GetLinearVelocityDamping());
			bAppliedTuning = true;
		}
		if(pTuning->GetLinearVelocitySquaredDamping().IsGreaterOrEqualThan(vZero))
		{
			pPhysicsArchetype->ActivateDamping(phArchetypeDamp::LINEAR_V2,pTuning->GetLinearVelocitySquaredDamping());
			bAppliedTuning = true;
		}

		if(pTuning->GetAngularSpeedDamping().IsGreaterOrEqualThan(vZero))
		{
			pPhysicsArchetype->ActivateDamping(phArchetypeDamp::ANGULAR_C,pTuning->GetAngularSpeedDamping());
			bAppliedTuning = true;
		}
		if(pTuning->GetAngularVelocityDamping().IsGreaterOrEqualThan(vZero))
		{
			pPhysicsArchetype->ActivateDamping(phArchetypeDamp::ANGULAR_V,pTuning->GetAngularVelocityDamping());
			bAppliedTuning = true;
		}
		if(pTuning->GetAngularVelocitySquaredDamping().IsGreaterOrEqualThan(vZero))
		{
			pPhysicsArchetype->ActivateDamping(phArchetypeDamp::ANGULAR_V2,pTuning->GetAngularVelocitySquaredDamping());
			bAppliedTuning = true;
		}
	}

	return bAppliedTuning;
}


//
// name:		CBaseModelInfo::SetDrawableDependciesAsDirty
// description:	Tag the entry in the drawable or fragment store as dirty (which ever one is used by this model info)
void CBaseModelInfo::SetDrawableDependenciesAsDirty(bool markTexDict)

{
	switch (GetDrawableType()) {
		case DT_FRAGMENT:
			modelinfoAssert(GetFragmentIndex() != INVALID_FRAG_IDX);
			CStreaming::SetDoNotDefrag(strLocalIndex(GetFragmentIndex()), g_FragmentStore.GetStreamingModuleId());
			break;
		case DT_DRAWABLE:
			modelinfoAssert(GetDrawableIndex() != INVALID_DRAWABLE_IDX);
			CStreaming::SetDoNotDefrag(strLocalIndex(GetDrawableIndex()), g_DrawableStore.GetStreamingModuleId());
			break;
		default:
			modelinfoAssertf(false,"Can't set dependency as dirty for this model info : %s", GetModelName());
	}
	
	u32 txdIndex = GetAssetParentTxdIndex();
	if ( markTexDict && txdIndex != -1)
	{
		CStreaming::SetDoNotDefrag(strLocalIndex(txdIndex), g_TxdStore.GetStreamingModuleId());
	}
}


void CBaseModelInfo::BumpDrawableRefCount()
{
	switch (GetDrawableType()) {
		case DT_FRAGMENT:
			modelinfoAssert(GetFragmentIndex() != INVALID_FRAG_IDX);
			g_FragmentStore.AddRef(strLocalIndex(GetFragmentIndex()), REF_OTHER);
			break;
		case DT_DRAWABLE:
			modelinfoAssert(GetDrawableIndex() != INVALID_DRAWABLE_IDX);
			g_DrawableStore.AddRef(strLocalIndex(GetDrawableIndex()), REF_OTHER);
			break;
		default:
			modelinfoAssertf(false,"Can't Get dependencies for this model info : %s", GetModelName());
	}
}

int CBaseModelInfo::GetDrawableRefCount()
{
	switch (GetDrawableType()) {
		case DT_FRAGMENT:
			modelinfoAssert(GetFragmentIndex() != INVALID_FRAG_IDX);
			return g_FragmentStore.GetNumRefs(strLocalIndex(GetFragmentIndex()));
		case DT_DRAWABLE:
			modelinfoAssert(GetDrawableIndex() != INVALID_DRAWABLE_IDX);
			return g_DrawableStore.GetNumRefs(strLocalIndex(GetDrawableIndex()));
		default:
			modelinfoAssertf(false,"Can't Get ref count for this model info : %s", GetModelName());
	}

	return -1;
}

void CBaseModelInfo::ReleaseDrawable()
{
	switch (GetDrawableType()) {
		case DT_FRAGMENT:
			modelinfoAssert(GetFragmentIndex() != INVALID_FRAG_IDX);
			g_FragmentStore.RemoveRef(strLocalIndex(GetFragmentIndex()), REF_OTHER);
			break;
		case DT_DRAWABLE:
			modelinfoAssert(GetDrawableIndex() != INVALID_DRAWABLE_IDX);
			g_DrawableStore.RemoveRef(strLocalIndex(GetDrawableIndex()), REF_OTHER);
			break;
		default:
			modelinfoAssertf(false,"Can't RemoveRef for this model info : %s", GetModelName());
	}
}

//
// name:		CBaseModelInfo::SetFragType
// description:	
//void CBaseModelInfo::SetFragType(gtaFragType *UNUSED_PARAM(pType))
void CBaseModelInfo::InitMasterFragData(u32 modelIdx)
{
	SetHasLoaded(true);
	gtaFragType* pFragType = static_cast<gtaFragType *>(GetFragType());		// lookup from store
	if (!pFragType)
		SetHasLoaded(false);

	modelinfoAssert(pFragType);

	SetupCustomShaderEffects();

	// fragments shouldn't be fixed, otherwise they can't fragment, so what's the point of them being fragments
	if(GetIsFixed())
		SetIsFixed(false);

	Assertf(IsFiniteAll(VECTOR3_TO_VEC3V(pFragType->GetPhysics(0)->GetUnbrokenCGOffset())) &&
			IsFiniteAll(VECTOR3_TO_VEC3V(pFragType->GetPhysics(0)->GetOriginalRootCGOffset())) &&
			IsFiniteAll(VECTOR3_TO_VEC3V(pFragType->GetPhysics(0)->GetRootCGOffset())), "Fragment '%s' loaded in with non-finite CG.", pFragType->GetTuneName());

	phArchetypeDamp *pArchetype = pFragType->GetPhysics(0)->GetArchetype();
	if(pArchetype)
	{
		if(pArchetype->GetBound())
		{
			UpdateBoundingVolumes(*pArchetype->GetBound());
		}

		if (GetIsTypeObject() || GetModelType() == MI_TYPE_VEHICLE)
		{
			if(pFragType->GetNumEnvCloths()>0)
			{
				Assert( pFragType->GetTypeEnvCloth(0) );
				const clothInstanceTuning* pClothTuning = pFragType->GetTypeEnvCloth(0)->m_Cloth.GetTuning();
				const bool activateOnHit = pClothTuning ? pClothTuning->GetFlag( clothInstanceTuning::CLOTH_TUNE_ACTIVATE_ON_HIT ): false;

				if( activateOnHit )
				{
					pArchetype->SetTypeFlags(ArchetypeFlags::GTA_OBJECT_TYPE);
					pArchetype->SetIncludeFlags(ArchetypeFlags::GTA_OBJECT_INCLUDE_TYPES);
				}
				else
				{
					pArchetype->SetTypeFlags(ArchetypeFlags::GTA_ENVCLOTH_OBJECT_TYPE);
					pArchetype->SetIncludeFlags(ArchetypeFlags::GTA_ENVCLOTH_OBJECT_INCLUDE_TYPES);
				}
			}
			else if(pFragType->GetNumGlassPaneModelInfos()>0)
			{
				pArchetype->SetTypeFlags(ArchetypeFlags::GTA_OBJECT_TYPE | ArchetypeFlags::GTA_GLASS_TYPE | ArchetypeFlags::GTA_UNSMASHED_TYPE);
				pArchetype->SetIncludeFlags(ArchetypeFlags::GTA_OBJECT_INCLUDE_TYPES);

				phBound* pBound = pArchetype->GetBound();
				Assert(pBound);
				if(phBound::IsTypeComposite(pBound->GetType()))
				{
					// Only glass parts of composites should be marked as such
					phBoundComposite* pBoundComposite = static_cast<phBoundComposite*>(pBound);
					bool createdTypeIncludeFlags = false;
					if(pBoundComposite->GetTypeAndIncludeFlags() == NULL)
					{
						createdTypeIncludeFlags = true;
						pBoundComposite->AllocateTypeAndIncludeFlags();
					}
					const fragPhysicsLOD* physicsLOD = pFragType->GetPhysics(0);
					for(int componentIndex = 0; componentIndex < pBoundComposite->GetNumBounds(); ++componentIndex)
					{
						// If the composite already had type include flags just add these type flags on, otherwise start from 0. We don't want to 
						//   stomp mover/weapon flags.
						u32 oldTypeFlags = createdTypeIncludeFlags ? 0 : pBoundComposite->GetTypeFlags(componentIndex);
						if(physicsLOD->GetGroup(physicsLOD->GetChild(componentIndex)->GetOwnerGroupPointerIndex())->GetMadeOfGlass())
						{
							pBoundComposite->SetTypeFlags(componentIndex, oldTypeFlags | ArchetypeFlags::GTA_OBJECT_TYPE | ArchetypeFlags::GTA_GLASS_TYPE | ArchetypeFlags::GTA_UNSMASHED_TYPE);
						}
						else
						{
							pBoundComposite->SetTypeFlags(componentIndex, oldTypeFlags | ArchetypeFlags::GTA_OBJECT_TYPE);
						}
					}
				}
			}
			else
			{
				u32 nTypeFlags = ArchetypeFlags::GTA_OBJECT_TYPE;

				//Horrible hack but they want these to hit map type mover
				if(( GetModelNameHash() == MI_PROP_STUNT_TUBE_01.GetName().GetHash() ||
					GetModelNameHash() == MI_PROP_STUNT_TUBE_02.GetName().GetHash() || 
					GetModelNameHash() == MI_PROP_STUNT_TUBE_03.GetName().GetHash() || 
					GetModelNameHash() == MI_PROP_STUNT_TUBE_04.GetName().GetHash() || 
					GetModelNameHash() == MI_PROP_STUNT_TUBE_05.GetName().GetHash() ) )
				{
					nTypeFlags = ArchetypeFlags::GTA_MAP_TYPE_MOVER|ArchetypeFlags::GTA_OBJECT_TYPE;
				}

				pArchetype->SetTypeFlags(nTypeFlags);
				pArchetype->SetIncludeFlags(ArchetypeFlags::GTA_OBJECT_INCLUDE_TYPES);
			}

			// Do this last so that doors with glass panels behave correctly in the world.
			if(GetUsesDoorPhysics())
			{
				u32 nDoorTypeFlags = ArchetypeFlags::GTA_OBJECT_TYPE;
				u32 nDoorIncludeFlags = ArchetypeFlags::GTA_DOOR_OBJECT_INCLUDE_TYPES;
				pArchetype->AddTypeFlags(nDoorTypeFlags);
				pArchetype->SetIncludeFlags(nDoorIncludeFlags);

				// We don't want doors getting map type flags applied to child parts.
				phBound* pBound = pArchetype->GetBound();
				Assert(pBound);
				if(pBound->GetType() == phBound::COMPOSITE)
				{
					phBoundComposite* pBoundComp = static_cast<phBoundComposite*>(pBound);
					if(pBoundComp->GetTypeAndIncludeFlags())
					{
						for(int i = 0; i < pBoundComp->GetNumBounds(); ++i)
						{
							// Set the type and include flags for this component.
							pBoundComp->SetTypeFlags(i, nDoorTypeFlags);
							pBoundComp->SetIncludeFlags(i, nDoorIncludeFlags);
						}
					}
				}
			}

			Vector3 vecDragCoef;
			vecDragCoef.Set(DEFAULT_OBJECT_DRAG_COEFF);
			pArchetype->ActivateDamping(phArchetypeDamp::LINEAR_V2, vecDragCoef);
		}
		else if(GetModelType()==MI_TYPE_BASE)
		{
			pArchetype->SetTypeFlags(ArchetypeFlags::GTA_MAP_TYPE_MOVER|ArchetypeFlags::GTA_MAP_TYPE_WEAPON);
			pArchetype->SetIncludeFlags(ArchetypeFlags::GTA_BUILDING_INCLUDE_TYPES);
		}

		// setup material flags on this bound
		if(pArchetype->GetBound())
			CopyMaterialFlagsToBound(pArchetype->GetBound());

		if( NetworkInterface::IsGameInProgress() &&	(	GetModelNameHash() == g_boatCollisionHashName5
			||	GetModelNameHash() == g_boatCollisionHashName1
			||	GetModelNameHash() == g_boatCollisionHashName2
			||	GetModelNameHash() == g_boatCollisionHashName3
			||	GetModelNameHash() == g_boatCollisionHashName4
			)
		  )
		{
			u32 typeFlags = ArchetypeFlags::GTA_MAP_TYPE_COVER | ArchetypeFlags::GTA_OBJECT_TYPE;
			pArchetype->SetTypeFlags( typeFlags );

			u32 includeFlags = ArchetypeFlags::GTA_VEHICLE_NON_BVH_TYPE | ArchetypeFlags::GTA_VEHICLE_BVH_TYPE | ArchetypeFlags::GTA_PED_TYPE
				| ArchetypeFlags::GTA_RAGDOLL_TYPE| ArchetypeFlags::GTA_HORSE_TYPE| ArchetypeFlags::GTA_HORSE_RAGDOLL_TYPE;
			pArchetype->SetIncludeFlags( includeFlags );
		}
	}

#if __BANK
	if(PARAM_fragtuning.Get())
	{
		// Store a pointer to the name, which is owned by a ConstString that is the key to the type map
		gtaFragType* tuningType = NULL;

		//m_pFragType->SetTuneName(GetModelName());

		const char* tuneName = pFragType->GetTuneName();
		if (const char* baseName = strrchr(tuneName, '/'))
		{
			tuneName = baseName + 1;
		}

		if (fragTune::IsInstantiated() && FRAGTUNE->GetForcedTuningPath())
		{
			ASSET.PushFolder(FRAGTUNE->GetForcedTuningPath());
		}
		else
		{
			ASSET.PushFolder("$/tune/types/fragments");
		}
		fiStream* tuningStream = ASSET.Open(tuneName, "tune", true, true);
		ASSET.PopFolder();

		if(tuningStream)
		{
			fiAsciiTokenizer tuningTok;
			tuningTok.Init(tuneName, tuningStream);

	//		sysMemStartTemp();
			tuningType = rage_new gtaFragType;
			tuningType->LoadASCII(tuningTok, tuneName, false);
	//		sysMemEndTemp();

			pFragType->ApplyTuningData(tuningType, NULL);

			tuningStream->Close();
	//		sysMemStartTemp();
			delete tuningType;
	//		sysMemEndTemp();
		}
	}
#endif

	const atArray<CLightAttr>* pLightArray = GetLightArray();
	if(pLightArray && pLightArray->GetCount() > 0)
	{
		SetHasPreRenderEffects();
	}

	// for the moment, fragments with a single child that are supposed to dissapear when dead aren't working
	// they only work if they get put into the fragment cache (which sometimes happens because of the projected texture stuff)
	// lets just force it to happen for now, so its consistant at least!
	if(pFragType->GetPhysics(0)->GetNumChildGroups()==1 && pFragType->GetPhysics(0)->GetAllGroups()[0]->GetDisappearsWhenDead())
	{
		pFragType->SetNeedsCacheEntryToActivate();
	}

	fragDrawable* pFragDrawable=pFragType->GetCommonDrawable(); 

	rmcLodGroup &lodGroup = pFragDrawable->GetLodGroup();
	lodGroup.ComputeBucketMask(pFragDrawable->GetShaderGroup());

#if __ASSERT
	const float MASS_LOWER_BOUND = 0.01f;
	modelinfoAssertf(pFragType->GetPhysics(0)->GetArchetype()->GetMass() > MASS_LOWER_BOUND, "%s: Fragment whole has very small mass (%g)", GetModelName(), pFragType->GetPhysics(0)->GetArchetype()->GetMass());
	for(int i=0; i<pFragType->GetPhysics(0)->GetNumChildren(); i++)
	{
		modelinfoAssertf(pFragType->GetPhysics(0)->GetAllChildren()[i]->GetUndamagedMass() > MASS_LOWER_BOUND, "%s: Fragment child %d has very small mass (%g)", GetModelName(), i, pFragType->GetPhysics(0)->GetAllChildren()[i]->GetUndamagedMass());
		modelinfoAssertf(pFragType->GetPhysics(0)->GetAllChildren()[i]->GetDamagedEntity() == NULL || pFragType->GetPhysics(0)->GetAllChildren()[i]->GetDamagedMass() > MASS_LOWER_BOUND, "%s: Fragment damaged child %d has very small mass (%g)", GetModelName(), i, pFragType->GetPhysics(0)->GetAllChildren()[i]->GetDamagedMass());
	}
#endif // __ASSERT	

	// don't do the same checks for vehicles and peds
	if(GetModelType()==MI_TYPE_BASE && pArchetype && pArchetype->GetBound()->GetType()==phBound::COMPOSITE && !GetIsFixed())
	{
		ASSERT_ONLY(int nTotalPolys = 0;)
		phBoundComposite* pBoundComposite = (phBoundComposite*)pArchetype->GetBound();
		for(int i=0; i<pBoundComposite->GetNumBounds(); i++)
		{
			if(pBoundComposite->GetBound(i) && pBoundComposite->GetBound(i)->GetType()==phBound::GEOMETRY)
			{
				int nPolys = ((phBoundGeometry*)pBoundComposite->GetBound(i))->GetNumPolygons();
				if (!Verifyf(nPolys < MAX_NUM_POLYS_BEFORE_GETTING_NERFED, "%s has WAY TOO MANY POLYS %d in its bounds, bound is being nerfed to prevent crashes", GetModelName(), nPolys))
				{
					((phBoundGeometry*)pBoundComposite->GetBound(i))->ClearPolysAndVerts();
				}
#if __ASSERT
				else if(((phBoundGeometry*)pBoundComposite->GetBound(i))->GetNumPolygons() > 32)
				{
					if(PARAM_propasserts.Get())
						physicsAssertf(false, "%s has too many polys %d in one of its bounds", GetModelName(), nPolys);
					else
						physicsDisplayf("%s has too many polys %d in one of its bounds", GetModelName(), nPolys);
				}
				nTotalPolys += ((phBoundGeometry*)pBoundComposite->GetBound(i))->GetNumPolygons();
#endif // __ASSERT	
			}
			// count other types (box, capsule, sphere) as cost of 2 polys (wild approximation)
#if __ASSERT
			else
				nTotalPolys += 2;
#endif // __ASSERT	
		}
#if __ASSERT
		if(nTotalPolys > 128)
		{
			if(PARAM_propasserts.Get())
				physicsAssertf(false, "%s has too many polys (or equivalent) %d in total in its composite  bound", GetModelName(), nTotalPolys);
			else
				physicsDisplayf("%s has too many polys (or equivalent) %d in total in its composite  bound", GetModelName(), nTotalPolys);
		}
#endif // __ASSERT	
	}

#if SECTOR_TOOLS_EXPORT
	// If we have enabled our Sector Tools on the command line then we
	// prevent fragment objects breaking apart.  We also lock any hinges
	// to ensure props etc. don't flap around.
	if ( CSectorTools::GetEnabled() )
	{
		for ( int i=0; i<GetFragType()->GetPhysics(0)->GetNumChildGroups(); ++i )
		{
			GetFragType()->GetPhysics(0)->GetAllGroups()[i]->SetStrength( -1.0f );
			GetFragType()->GetPhysics(0)->GetAllGroups()[i]->SetLatchStrength( -1.0f );
		}
	}
#endif // SECTOR_TOOLS_EXPORT

	if (GetIsTypeObject() && !GetSyncObjInNetworkGame() && !GetUsesDoorPhysics()
	&& !TestAttribute(MODEL_ATTRIBUTE_IS_TRAFFIC_LIGHT) && !TestAttribute(MODEL_ATTRIBUTE_IS_STREET_LIGHT))
	{
		if(GetNum2dEffectType(ET_EXPLOSION) > 0)
			SetSyncObjInNetworkGame(true);

		if(!GetSyncObjInNetworkGame())
		{
			float fGroupMassMax = 0.0f;
			for(int i=0; i<GetFragType()->GetPhysics(0)->GetNumChildGroups(); i++)
			{
				if(GetFragType()->GetPhysics(0)->GetAllGroups()[i]->GetTotalUndamagedMass() > fGroupMassMax)
					fGroupMassMax = GetFragType()->GetPhysics(0)->GetAllGroups()[i]->GetTotalUndamagedMass();
			}

			if(fGroupMassMax > MIN_MASS_FOR_NETWORK_SYNC)
			{
				phBoundComposite* pBoundComp = static_cast<phBoundComposite*>(GetFragType()->GetPhysics(0)->GetArchetype()->GetBound());
				for(int i=0; i<pBoundComp->GetNumBounds(); i++)
				{
					if(pBoundComp->GetBound(i))
					{
						Vector3 vecBBoxExtents = VEC3V_TO_VECTOR3(pBoundComp->GetBound(i)->GetBoundingBoxMax() - pBoundComp->GetBound(i)->GetBoundingBoxMin());
						if(vecBBoxExtents.x > MIN_SIZE_FOR_NETWORK_SYNC || vecBBoxExtents.x > MIN_SIZE_FOR_NETWORK_SYNC || vecBBoxExtents.x > MIN_SIZE_FOR_NETWORK_SYNC)
						{
							SetSyncObjInNetworkGame(true);
							break;
						}
					}
				}
			}
		}
	}
	
	if(TestAttribute(MODEL_ATTRIBUTE_IS_TRAFFIC_LIGHT))
	{
		CTrafficLights::SetupModelInfo(this);
	}
	else if(TestAttribute(MODEL_ATTRIBUTE_IS_RAIL_CROSSING_LIGHT))
	{
		CRailwayCrossingLights::SetupModelInfo(this);
	}
	else if(TestAttribute(MODEL_ATTRIBUTE_IS_RAIL_CROSSING_DOOR))
	{
		CRailwayBarrierLights::SetupModelInfo(this);
	}

	// If this object has dynamic cover bounds, let's search for them and set their collision appropriately.
	if(TestAttribute(MODEL_ATTRIBUTE_HAS_DYNAMIC_COVER_BOUND) && pArchetype && pArchetype->GetBound())
	{
		SetupDynamicCoverCollisionBounds(pArchetype);
	}

	const bool bModelNeedsBuoyancy = (GetIsTypeObject() && !GetIsFixed())
		|| GetModelType() == MI_TYPE_VEHICLE
		|| GetModelType() == MI_TYPE_PED
		|| GetModelType() == MI_TYPE_WEAPON;

	Assert(GetModelType()!=MI_TYPE_NONE);
	Assertf(!m_pBuoyancyInfo || m_pBuoyancyInfo->m_WaterSamples, "m_pBuoyancyInfo=0x%p, m_WaterSamples=0x%p", m_pBuoyancyInfo, m_pBuoyancyInfo->m_WaterSamples);
	if(m_pBuoyancyInfo == NULL && bModelNeedsBuoyancy)
	{
		InitWaterSamples();
	}

	if( GetCarryScriptedRT() && !gRenderTargetMgr.IsNamedRenderTargetLinked(modelIdx, m_scriptId))
	{
		gRenderTargetMgr.LinkNamedRendertargets(modelIdx,m_scriptId);
	}

	// If this model has glass update the frame information
	if(pFragType && pFragType->GetNumGlassPaneModelInfos() > 0)
	{
		u16 frameFlags = 15 << 2;
		const CTunableObjectEntry* pTunableObj = CTunableObjectManager::GetInstance().GetTuningEntry(GetModelNameHash());
		if(pTunableObj)
		{
			int iGlassFlags = pTunableObj->GetGlassFrameFlags();
			frameFlags = (u16)(iGlassFlags << 2);
		}
		for(int i = 0; i < pFragType->GetNumGlassPaneModelInfos(); i++)
		{
			bgPaneModelInfoBase* pModelInfo = pFragType->GetAllGlassPaneModelInfos()[i];
			pModelInfo->m_flags |= frameFlags;
		}
	}

	//---GTA deathmatch/race creator TitleUpdate hack---
	// Need to prevent this prop from activating unless bumped or it'll break apart. Can't update resources
	//   in titleupdate so we need to hack this in. 
	if(pFragType && GetModelNameHash() == atHashValue("prop_offroad_barrel02",0xc4932af2))
	{
		// Probably unnecessary check
		if(pFragType->GetPhysics(0))
		{
			pFragType->GetPhysics(0)->SetMinMoveForce(0.01f);
		}
	}
	//--------------------------------------------------

#if __BANK || (__WIN32PC && !__FINAL)
	SetIsProp(false);
	u16 index = 0xffff;
	strStreamingModule* module = NULL;
	switch (GetDrawableType())
	{
	case DT_FRAGMENT:
		module = &g_FragmentStore;
		index = static_cast<u16>(GetFragmentIndex());
		break;
	case DT_DRAWABLE:
		module = &g_DrawableStore;
		index = static_cast<u16>(GetDrawableIndex());
		break;
	case DT_DRAWABLEDICTIONARY:
		module = &g_DwdStore;
		index = static_cast<u16>(GetDrawDictIndex());
		break;
	default:
		break;
	} 
	if (module && index != 0xffff)
	{
		strStreamingInfo* info = module->GetStreamingInfo(strLocalIndex(index));
		if (info)
		{
			s32 imageIndex = strPackfileManager::GetImageFileIndexFromHandle(info->GetHandle()).Get();
			if (imageIndex != -1)
			{
				strStreamingFile* file = strPackfileManager::GetImageFile(imageIndex);
				if (file && file->m_contentsType == CDataFileMgr::CONTENTS_PROPS)
					SetIsProp(true);
			}
		}
	}
#endif // __BANK
}

//
// name:		CBaseModelInfo::DeleteFragType
// description:	
void CBaseModelInfo::DeleteMasterFragData()
{
	if (m_pShaderEffectType)
	{
#if __ASSERT
		if (GetNumRefs() != 0)
		{
			modelinfoErrorf("Non-zero refcount (%d) on %s upon deletion. List of dependencies:", GetNumRefs(), GetModelName());

			strStreamingModule*	streamingModule = fwArchetypeManager::GetStreamingModule();

			u32 objIndex = fwArchetypeManager::GetArchetypeIndex(GetModelNameHash());
			strIndex archIndex = streamingModule->GetStreamingIndex(strLocalIndex(objIndex));
			strStreamingEngine::GetInfo().PrintAllDependents(archIndex);

	        modelinfoAssertf(false, "Model %s still has a reference count of %d as we're trying to delete it. See TTY for dependencies.", GetModelName(), GetNumRefs());
		}
#endif // __ASSERT
		m_pShaderEffectType->RestoreModelInfoDrawable(GetDrawable());
		m_pShaderEffectType->RemoveRef();
		m_pShaderEffectType = NULL;
	}

	if(GetDrawableType() == DT_FRAGMENT)
	{
		modelinfoAssert(GetFragmentIndex() != INVALID_FRAG_IDX);

		//delete m_pFragType;
		//m_pFragType = NULL;
		SetHasLoaded(false);

		DeleteWaterSamples();
	}
}

void CBaseModelInfo::Init2dEffects() 
{
	if (m_p2dEffectPtrs){
		m_p2dEffectPtrs->Reset();			// should I do this?
	}
}

s32 CBaseModelInfo::GetNum2dEffectType(e2dEffectType type) const
{
	int count = 0;

	if(type != ET_LIGHT)
	{
		int numPtrs = m_p2dEffectPtrs ? m_p2dEffectPtrs->GetCount() : 0;
		for(int i = 0; i < numPtrs; i++)
		{
			if ((*m_p2dEffectPtrs)[i]->GetType() == type)
			{
				count++;
			}
		}
	}
	else
	{
		GET_2D_EFFECTS(this);
		for(int i = 0; i < numEffects; i++)
		{
			if(a2dEffects[i]->GetType() == type)
			{
				count++;
			}
		}
	}

	return count;
}

const ModelAudioCollisionSettings*CBaseModelInfo::GetAudioCollisionSettings()
{
	if(m_AudioCollisionsOverride)
	{
		return m_AudioCollisionsOverride;
	}

	int numPtrs = m_p2dEffectPtrs ? m_p2dEffectPtrs->GetCount() : 0;
	for(int i = 0; i < numPtrs; i++)
	{
		if ((*m_p2dEffectPtrs)[i]->GetType() == ET_AUDIOCOLLISION)
		{
			return (*m_p2dEffectPtrs)[i]->GetAudioCollisionInfo()->settings;
		}
	}

	return g_MacsDefault;
}

void CBaseModelInfo::SetAudioCollisionSettings(const ModelAudioCollisionSettings * settings, u32 UNUSED_PARAM(hash))
{
	m_AudioCollisionsOverride = settings;
}


s32 CBaseModelInfo::GetNum2dEffectLights() const
{
	gtaFragType* pFragType = static_cast<gtaFragType *>(GetFragType());
	if(pFragType)
		return (0 + pFragType->m_lights.GetCount());

	gtaDrawable* pDrawable = (gtaDrawable*)GetDrawable();

	if(pDrawable)
		return 0 + pDrawable->m_lights.GetCount();

	return 0;
}


s32 CBaseModelInfo::GetNum2dEffects() const
{
	u32 num2dEffects = 0;

	if (m_p2dEffectPtrs){
		num2dEffects = m_p2dEffectPtrs->GetCount();
	}

	gtaFragType* pFragType = static_cast<gtaFragType *>(GetFragType());
	if(pFragType)
		return (num2dEffects + pFragType->m_lights.GetCount());

	gtaDrawable* pDrawable = (gtaDrawable*)GetDrawable();

	if(pDrawable)
		return num2dEffects + pDrawable->m_lights.GetCount();

	return num2dEffects;
}

bool CBaseModelInfo::Has2dEffects() const
{
	if (m_p2dEffectPtrs && m_p2dEffectPtrs->GetCount())
	{
		return true;
	}

	gtaFragType* pFragType = static_cast<gtaFragType *>(GetFragType());
	if(pFragType)
		return (pFragType->m_lights.GetCount() > 0);

	gtaDrawable* pDrawable = (gtaDrawable*)GetDrawable();
	if(pDrawable)
		return (pDrawable->m_lights.GetCount() > 0);

	return false;
}

const atArray<C2dEffect*>* CBaseModelInfo::Get2dEffectsNoLights()
{
	return m_p2dEffectPtrs;
}

const atArray<CLightAttr>* CBaseModelInfo::GetLightArray()
{
	gtaFragType* pFragType = static_cast<gtaFragType *>(GetFragType());
	if(pFragType)
	{
		return &pFragType->m_lights;
	}

	gtaDrawable* pDrawable = (gtaDrawable*)GetDrawable();
	if(pDrawable)
	{
		return &pDrawable->m_lights;
	}

	return NULL;
}

void CBaseModelInfo::Get2dEffects(atArray<C2dEffect*>& effects) const
{
	modelinfoFatalAssertf(GetNum2dEffects() < MAX_NUM_2DEFFECTS, "Model found with %d effects - Need to increase MAX_NUM_2DEFFECTS (currently %d)", GetNum2dEffects(), MAX_NUM_2DEFFECTS);

	u32 numEffects = 0;
	if (m_p2dEffectPtrs){
		numEffects = m_p2dEffectPtrs->GetCount();

		for(int i = 0; i < numEffects; i++)
		{
			effects.Push((*m_p2dEffectPtrs)[i]);
		}
	}

	gtaFragType* pFragType = static_cast<gtaFragType *>(GetFragType());
	if(pFragType)
	{
		int numLights = pFragType->m_lights.GetCount();

		for(int i = 0; i < numLights; i++)
		{
			effects.Push(&(pFragType->m_lights[i]));
		}

		return;
	}

	gtaDrawable* pDrawable = (gtaDrawable*)GetDrawable();
	if(pDrawable)
	{
		int numLights = pDrawable->m_lights.GetCount();

		for(int i = 0; i < numLights; i++)
		{
			effects.Push(&(pDrawable->m_lights[i]));
		}
	}
}

C2dEffect* CBaseModelInfo::Get2dEffect(s32 i) 
{
	u32 num2dEffects = 0;

	if (m_p2dEffectPtrs){
		num2dEffects = m_p2dEffectPtrs->GetCount();
		if(i < num2dEffects)
		{
			return((*m_p2dEffectPtrs)[i]);
		}
	}

	gtaFragType* pFragType = static_cast<gtaFragType *>(GetFragType());
	if((pFragType != NULL) && (i >= num2dEffects))
	{
		if(i - num2dEffects < pFragType->m_lights.GetCount())
			return &(pFragType->m_lights[i - num2dEffects]);
		else
			return NULL;
	}
	
	gtaDrawable* pDrawable = (gtaDrawable*)GetDrawable();

	if((pDrawable != NULL) && (i >= num2dEffects))
	{
		s32 index = i - num2dEffects;

		if((index >= 0) && (index < pDrawable->m_lights.GetCount()))
		{
			return &(pDrawable->m_lights[i - num2dEffects]);
		}
		else
		{
			return NULL;
		}
	}

	return NULL;
}

C2dEffect** CBaseModelInfo::GetNewEffect()
{ 
	modelinfoAssertf(m_p2dEffectPtrs, "No effect array is initialised for this archetype : %s", GetModelName());
	modelinfoAssertf(m_p2dEffectPtrs->GetCount() < m_p2dEffectPtrs->GetCapacity(), "%s:Too many 2d effects on this modelinfo", GetModelName());

	return(&(m_p2dEffectPtrs->Append()));
}

void CBaseModelInfo::Add2dEffect(C2dEffect** ppEffect)
{
	u8 effectType = (*ppEffect)->GetType();

	// set has spawn point flag
	if(effectType == ET_SPAWN_POINT)
		SetHasSpawnPoints(true);

	if (effectType == ET_LIGHT || effectType == ET_LIGHT_SHAFT || effectType == ET_SCROLLBAR || effectType == ET_WINDDISTURBANCE || 
		(effectType==ET_PARTICLE && ((*ppEffect)->GetParticle()->m_fxType==PT_AMBIENT_FX || (*ppEffect)->GetParticle()->m_fxType==PT_INWATER_FX)))
	{
		SetHasPreRenderEffects();
	}
}

s32 CBaseModelInfo::GetNum2dEffectsNoLights(void) const
{
	if (m_p2dEffectPtrs){
		return(m_p2dEffectPtrs->GetCount());
	} else {
		return(0);
	}
}

void CBaseModelInfo::SetNum2dEffects(u32 numEffects)
{
	modelinfoAssertf(!m_p2dEffectPtrs, "2D effect array is already initialised for this archetype : %s", GetModelName());

	m_p2dEffectPtrs = rage_new atArray<C2dEffect*>;
	Assert(m_p2dEffectPtrs);
	m_p2dEffectPtrs->Reset();
	m_p2dEffectPtrs->Reserve(numEffects);
}

// check the txd chain - check each slot for a valid HDTxd index & if any found then set HDTexCapable flag for this type
void CBaseModelInfo::CheckForHDTxdSlots(void){

	// ConvertLoadFlags() will set this to true if FLAG_SUPPRESS_HD_TXDS is not set
	if (!GetIsHDTxdCapable())
	{
		return;
	}

	// Reset it to false before we search for HD Texture dictionaries.
	SetIsHDTxdCapable(false);

	// 	check the base asset for HD mapping
	eStoreType type = STORE_ASSET_INVALID;
	s32 slot = -1;

	switch(GetDrawableType()){
		case(DT_DRAWABLE):
			type = STORE_ASSET_DRB;
			slot = GetDrawableIndex();
			break;
		case(DT_DRAWABLEDICTIONARY):
			type = STORE_ASSET_DWD;
			slot = GetDrawDictIndex();
			break;
		case(DT_FRAGMENT):
			type = STORE_ASSET_FRG;
			slot = GetFragmentIndex();
			break;
		default:
			return;
	}

 	if (CTexLod::HasAssetHDMapping(type, slot)){
 		SetIsHDTxdCapable(true);
 		return;
 	}

	// check txd chain for any txd with HD mapping
	strLocalIndex txdIndex = strLocalIndex(GetAssetParentTxdIndex());

	while(txdIndex != -1){
		Assertf(CTexLod::HasAssetHDMapping(STORE_ASSET_TXD, txdIndex.Get()) == g_TxdStore.GetIsHDCapable(txdIndex), "found cache mismatch for %s", (g_TxdStore.GetSlot(txdIndex))->m_name.GetCStr()); 
		if(g_TxdStore.GetIsHDCapable(txdIndex)) {
			SetIsHDTxdCapable(true);
			break;
		}
		txdIndex = g_TxdStore.GetParentTxdSlot(txdIndex);
	}
}

fwAssetLocation CBaseModelInfo::GetAssetLocation(void) const{

	// 	check the base asset for HD mapping
	eStoreType type = STORE_ASSET_INVALID;
	s32 slot = -1;

	switch(GetDrawableType()){
		case(DT_DRAWABLE):
			type = STORE_ASSET_DRB;
			slot = GetDrawableIndex();
			break;
		case(DT_DRAWABLEDICTIONARY):
			type = STORE_ASSET_DWD;
			slot = GetDrawDictIndex();
			break;
		case(DT_FRAGMENT):
			type = STORE_ASSET_FRG;
			slot = GetFragmentIndex();
			break;
		default:
			break;
	}

	return(fwAssetLocation(type, slot));
}

// --- CTimeModelInfo ----------------------------------------------------------

CTimeInfo* CTimeInfo::FindOtherTimeModel(const char* pName)
{
	char otherName[256];
	char* pIdentifier;
	u32 i;
	CBaseModelInfo* pModelInfo;
	CTimeInfo* pTimeInfo=NULL;
	
	safecpy(otherName, pName);
	pIdentifier = strstr(otherName, "_nt");
	if(pIdentifier)
	{
		strncpy(pIdentifier, "_dy", 4);
	}
	else
	{
		pIdentifier = strstr(otherName, "_dy");
		if(pIdentifier)
		{
			strncpy(pIdentifier, "_nt", 4);
		}
		else
			return NULL;
	}

	u32 key = atStringHash(otherName);
	// Go through list of models and compare name with model names
	const u32 maxModelInfos = CModelInfo::GetMaxModelInfos();
	for(i=0; i<maxModelInfos; i++)
	{
		pModelInfo = CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(i)));
		if(pModelInfo)
		{
			pTimeInfo = pModelInfo->GetTimeInfo();
			if(pTimeInfo)
			{
				if(pModelInfo->GetHashKey() == key)
				{
					break;
				}
			}	
		}	
	}

	if(i != maxModelInfos)
	{
		modelinfoAssertf((pTimeInfo->m_hoursOnOff ^ m_hoursOnOff) == 0x00ffffff, "%s:Times do not correspond with the other time model", pName);
		m_otherModel = i;
		return pTimeInfo;
	}
	return NULL;
}

static dev_float BASE_BUOYANCY_CONSTANT = 1.1f;
static dev_float BASE_DRAG_MULT = 50.0f;
static dev_float MIN_FLOATER_SUBMERGE_DEPTH = 0.3f;
static dev_float BASE_SINK_MULTIPLIER = 0.6f;
static dev_float BASE_SINK_DENSITY_THRESHOLD = 1000.0f;

void CBaseModelInfo::InitWaterSamples()
{
#if __DEV
	if(ms_bPrintWaterSampleEvents)
	{
		modelinfoDisplayf("[Buoyancy] Creating water samples for model %s", GetModelName());
	}
#endif

	modelinfoAssertf(m_pBuoyancyInfo == NULL,"InitWaterSamples() called when water samples already exist");
	if(m_pBuoyancyInfo != NULL)
	{
		return;
	}

	// going to allocate our own water samples
	CWaterSample tempWaterSamples[MAX_WATER_SAMPLES];

	phBound* pBound = NULL;
	phBound* pBoundForSampleGeneration = NULL;
	phArchetypeDamp *pPhysics = GetPhysics();

	int nComponent = 0;
	gtaFragType* pFragType = static_cast<gtaFragType*>(GetFragType());
	const CTunableObjectEntry* pTuning = CTunableObjectManager::GetInstance().GetTuningEntry(GetModelNameHash());
	if(pTuning && pTuning->GetBoundIndexForWaterSamples() >= 0)
	{
		int nComponentIndexForWaterSamples = pTuning->GetBoundIndexForWaterSamples();
		phBoundComposite* pCompBound = NULL;
		if(pFragType)
		{
			pCompBound = pFragType->GetPhysics(0)->GetCompositeBounds();
		}
		else if(pPhysics && pPhysics->GetBound() && pPhysics->GetBound()->GetType() == phBound::COMPOSITE)
		{
			pCompBound = static_cast<phBoundComposite*>(pPhysics->GetBound());
		}
		else
		{
			modelinfoAssert(false);
		}

		if(pCompBound)
		{
			if(modelinfoVerifyf(nComponentIndexForWaterSamples < pCompBound->GetNumBounds(),
				"<boundIndexForWaterSamples> is set to %d for %s which is greater than total number of bounds %d",
				nComponentIndexForWaterSamples, GetModelName(), pCompBound->GetNumBounds()))
			{
				nComponent = nComponentIndexForWaterSamples;
				pBound = pBoundForSampleGeneration = pCompBound->GetBound(nComponent);
			}
			else
			{
				pBound = pBoundForSampleGeneration = NULL;
			}
		}
	}

	if(!pBound || !pBoundForSampleGeneration)
	{
		if(pFragType)
		{
			pBound = pBoundForSampleGeneration = pFragType->GetPhysics(0)->GetCompositeBounds();
		}
		else if(pPhysics)
		{
			pBound = pBoundForSampleGeneration = pPhysics->GetBound();
			if(pBound && pBound->GetType() == phBound::COMPOSITE)
			{
				// For non-frag objects just use the biggest component to generate samples
				pBoundForSampleGeneration = GetLargestBoundFromComposite(static_cast<phBoundComposite*>(pBound));
				nComponent = pBoundForSampleGeneration->GetIndexFromBound();
			}
		}
		else
		{
			modelinfoAssert(false);
		}
	}
	int nNumSamplesNeeded = GenerateWaterSamplesFromBoundR(tempWaterSamples,MAX_WATER_SAMPLES,pBoundForSampleGeneration,nComponent);
	if(nNumSamplesNeeded == 0)
	{
		//modelinfoAssertf(false,"Water sample generation failed for object %s",GetModelName()); // cloth ends up with no buoyancy info
		return;
	}

	m_pBuoyancyInfo = rage_new CBuoyancyInfo;
	if(m_pBuoyancyInfo==NULL)
	{
		modelinfoAssertf(false,"Could not allocate buoyancy info");
		return;
	}
	modelinfoAssert(m_pBuoyancyInfo->m_WaterSamples == NULL);
	modelinfoAssert(m_pBuoyancyInfo->m_nNumWaterSamples == 0);
	m_pBuoyancyInfo->m_WaterSamples = rage_new CWaterSample[nNumSamplesNeeded];
#if __DEV
	ms_nNumWaterSamplesInMemory += m_pBuoyancyInfo->m_nNumWaterSamples;
	PF_SET(NumWaterSamplesInMemory, ms_nNumWaterSamplesInMemory);
#endif // __DEV
	if(m_pBuoyancyInfo->m_WaterSamples==NULL)
	{
		modelinfoAssertf(false,"Could not allocate buoyancy samples");
		return;
	}
	m_pBuoyancyInfo->m_nNumWaterSamples = (s16)nNumSamplesNeeded;

	float fSizeTotal = 0.0f;

	if(pFragType)
	{
		// Setup frag type

		// Need to balance buoyancy of each sample so that when object breaks up it still floats
		// Sample buoyancy = mass fraction / size fraction
		// Where mass fraction = mass of componenet / total mass
		// sample fraction = no of samples / total samples

		const float fTotalMass = pFragType->GetPhysics(0)->GetTotalUndamagedMass();
		const int nNumChildren = pFragType->GetPhysics(0)->GetNumChildren();

		float* fMassFraction = Alloca(float, nNumChildren);
		float* fSizeFraction = Alloca(float, nNumChildren);	// Number of samples per component
		modelinfoAssert(fMassFraction && fSizeFraction);

		for(int iChildIndex = 0; iChildIndex < nNumChildren; iChildIndex++)
		{
			fMassFraction[iChildIndex] = pFragType->GetPhysics(0)->GetAllChildren()[iChildIndex]->GetUndamagedMass() / fTotalMass;			
			fSizeFraction[iChildIndex] = 0.0f;
		}
		for(int iSampleIndex = 0; iSampleIndex < nNumSamplesNeeded; iSampleIndex++)
		{
			modelinfoAssert(tempWaterSamples[iSampleIndex].m_nComponent < nNumChildren);
			fSizeFraction[tempWaterSamples[iSampleIndex].m_nComponent] += tempWaterSamples[iSampleIndex].m_fSize; 
			fSizeTotal += tempWaterSamples->m_fSize;
		}

		for(int iSampleIndex = 0; iSampleIndex < nNumSamplesNeeded; iSampleIndex++)
		{
			const int nChildIndex = tempWaterSamples[iSampleIndex].m_nComponent;
			tempWaterSamples[iSampleIndex].m_fBuoyancyMult *= fSizeTotal * fMassFraction[nChildIndex]/
			fSizeFraction[nChildIndex];
			m_pBuoyancyInfo->m_WaterSamples[iSampleIndex].Set(tempWaterSamples[iSampleIndex]);
		}
	}
	else
	{
		// Setup Non-frag type
		for(int iSampleIndex = 0; iSampleIndex < nNumSamplesNeeded; iSampleIndex++)
		{
			m_pBuoyancyInfo->m_WaterSamples[iSampleIndex].Set(tempWaterSamples[iSampleIndex]);
			fSizeTotal += tempWaterSamples->m_fSize;
		}
	}

	float fBuoyancyConstant = BASE_BUOYANCY_CONSTANT;
	bool bFloater = false;
	float fTargetFloatPos = 0.0f;

	GET_2D_EFFECTS_NOLIGHTS(this);
	for(int i=0; i < iNumEffects && bFloater == false; i++)
	{
		C2dEffect *pEffect = (*pa2dEffects)[i];
		if(pEffect->GetType()==ET_BUOYANCY)
		{
			// get the height the object is supposed to float at
			Vector3 vFloatPos;
			pEffect->GetPos(vFloatPos);
			fTargetFloatPos = vFloatPos.z;
			bFloater = true;
		}
	}

	if(bFloater)
	{
		fBuoyancyConstant = (GetBoundingBoxMax().z - GetBoundingBoxMin().z) / rage::Max(MIN_FLOATER_SUBMERGE_DEPTH, fTargetFloatPos - GetBoundingBoxMin().z);
		static dev_bool bChangeCG = true;
		if(bChangeCG)
		{
			// Check if we should offset the centre of mass for this object.
			if(pTuning)
			{
				if(pTuning->GetCentreOfMassOffset().IsNonZero())
				{
					pBound->SetCGOffset(RCC_VEC3V(pTuning->GetCentreOfMassOffset()));
				}
			}
			else
			{
				Vector3 vecCgOffset = VEC3V_TO_VECTOR3(pBound->GetCGOffset());
				static dev_float fMagicNumber = 0.6f;
				vecCgOffset.z = fMagicNumber*(fTargetFloatPos + GetBoundingBoxMin().z);
				pBound->SetCGOffset(RCC_VEC3V(vecCgOffset));
			}
		}
	}

	// Check if we should scale the buoyancy constant for this object.
	if(pTuning)
	{
		if(pTuning->GetBuoyancyFactor() != 1.0f)
		{
			fBuoyancyConstant *= pTuning->GetBuoyancyFactor();
		}
	}

	m_pBuoyancyInfo->m_fBuoyancyConstant = fBuoyancyConstant * -GRAVITY / fSizeTotal;
	m_pBuoyancyInfo->m_fDragMultXY = m_pBuoyancyInfo->m_fDragMultZUp = m_pBuoyancyInfo->m_fDragMultZDown = BASE_DRAG_MULT * -GRAVITY / fSizeTotal;

	if(pTuning && pTuning->GetBuoyancyDragFactor() != 1.0f)
	{
		float fMultiplier = pTuning->GetBuoyancyDragFactor();
		m_pBuoyancyInfo->m_fDragMultXY *= fMultiplier;
		m_pBuoyancyInfo->m_fDragMultZUp *= fMultiplier;
		m_pBuoyancyInfo->m_fDragMultZDown *= fMultiplier;
	}
}

void CBaseModelInfo::DeleteWaterSamples()
{
	if(m_pBuoyancyInfo)
	{
#if __DEV
		if(ms_bPrintWaterSampleEvents)
		{
			modelinfoDisplayf("[Buoyancy] Deleting water samples for model %s", GetModelName());
		}
		ms_nNumWaterSamplesInMemory -= m_pBuoyancyInfo->m_nNumWaterSamples;
		PF_SET(NumWaterSamplesInMemory, ms_nNumWaterSamplesInMemory);
#endif

		delete m_pBuoyancyInfo;
		m_pBuoyancyInfo = NULL;
	}
}

bool CBaseModelInfo::GetGeneratesWindAudio() const
{
	if(!m_p2dEffectPtrs)
	{
		return false;
	}

	atArray<C2dEffect*>& effectPtrs = *m_p2dEffectPtrs;
	int numPtrs = effectPtrs.GetCount();	
	for(int i = 0; i < numPtrs; i++)
	{
		if (effectPtrs[i]->GetType() == ET_AUDIOEFFECT)
		{
			if(g_EmitterAudioEntity.GetGeneratesWindAudio(effectPtrs[i]->GetAudio()))
			{
				return true;
			}
		}
	}

	return false;

}

void CBaseModelInfo::InitLodSkeletonMap(const crSkeletonData* skelData, const crSkeletonData* lodSkelData, const CBaseModelInfo* ASSERT_ONLY(slodMI))
{
	if (!skelData || !lodSkelData)
		return;

	if (!skelData->HasBoneIds() || !lodSkelData->HasBoneIds())
		return;

	const u32 numLodBones = lodSkelData->GetNumBones();
	if (numLodBones <= 0)
		return;

	Assertf(skelData->GetNumBones() >= numLodBones, "Lod skeleton (%s) has more bones than original skeleton (%s)! (%d >= %d)", slodMI->GetModelName(), GetModelName(), skelData->GetNumBones(), numLodBones);

	m_lodSkelBoneNum = (u16)numLodBones;
	if (m_lodSkelBoneMap)
		delete[] m_lodSkelBoneMap;

	m_lodSkelBoneMap = rage_aligned_new(16) u16[m_lodSkelBoneNum];

	Assert(m_lodSkelBoneNum < 256);
	for (u8 i = 0; i < m_lodSkelBoneNum; ++i)
	{
		u16 lodBoneId = lodSkelData->GetBoneData(i)->GetBoneId();
        // if a bone in the lod skeleton isn't found in the original skeleton it will be skinned to previous bone (or 0 if this is the first one)
		s32 boneIndex = 0;
		if (!Verifyf(skelData->ConvertBoneIdToIndex(lodBoneId, boneIndex), "Couldn't find bone index for id %d (%s) from lod skeleton (%s) in original skeleton! (%s)", lodBoneId, lodSkelData->GetBoneData(i)->GetName(), slodMI->GetModelName(), GetModelName()))
		{
			boneIndex = (i > 0) ? m_lodSkelBoneMap[i - 1] : 0;
		}
		Assert(boneIndex < 65536);
		m_lodSkelBoneMap[i] = boneIndex & 0xffff;
	}
}

void CBaseModelInfo::InitLodSkeletonMap(const crSkeletonData* skelData, const atArray<int>& skinnedBones)
{
	if (!skelData)
		return;

	const int numLodBones = skinnedBones.GetCount();
	if (numLodBones <= 0)
		return;

	Assertf(skelData->GetNumBones() >= numLodBones, "Lod skeleton has more bones than original skeleton for '%s'! (%d >= %d)", GetModelName(), skelData->GetNumBones(), numLodBones);

	m_lodSkelBoneNum = (u16)numLodBones;
	if (m_lodSkelBoneMap)
		delete[] m_lodSkelBoneMap;

	m_lodSkelBoneMap = rage_aligned_new(16) u16[m_lodSkelBoneNum];

	Assert(m_lodSkelBoneNum < 256);
	for (u8 i = 0; i < m_lodSkelBoneNum; ++i)
	{
		s32 boneIndex = skinnedBones[i];
		Assert(boneIndex < 65536);
		m_lodSkelBoneMap[i] = boneIndex & 0xffff;
	}
}

void CBaseModelInfo::ShutdownLodSkeletonMap()
{
	if (m_lodSkelBoneMap)
	{
		delete[] m_lodSkelBoneMap;
		m_lodSkelBoneMap = NULL;
	}
	m_lodSkelBoneNum = 0;
}

s32 CBaseModelInfo::GetLodSkeletonBoneIndex(s32 boneIndex)
{
    s32 lodBoneIndex = boneIndex;

    if (m_lodSkelBoneMap)
    {
        for (s32 i = 0; i < m_lodSkelBoneNum; ++i)
        {
            if ((s32)m_lodSkelBoneMap[i] == boneIndex)
            {
                lodBoneIndex = i;
                break;
            }
        }
    }

    return lodBoneIndex;
}


#if NORTH_CLOTHS

void CBaseModelInfo::InitClothArchetype()
{
	// Need a skeleton to create a cloth
	Assertf(GetExtension<CClothArchetype>() == NULL, "CBaseModelInfo already has cloth data");
	
	rmcDrawable* pDrawable = GetDrawable();
	Assertf(pDrawable, "Can't call InitClothArchetype without a loaded drawable");

	if(pDrawable && pDrawable->GetSkeletonData())
	{
		// Lookup plant tuning data
		const CPlantTuning& plantTuningData=CPlantTuning::GetPlantTuneData(GetHashKey());

		CClothArchetype* pClothArchetype = CClothArchetypeCreator::CreatePlant(*pDrawable->GetSkeletonData(), plantTuningData);
		Assertf(pClothArchetype, "Failed to create cloth archetype for model %s", GetModelName());
		GetExtensionList().Add(*pClothArchetype);
	}
}

void CBaseModelInfo::DeleteClothArchetype()
{
	GetExtensionList().Destroy(CClothArchetype::GetAutoId());
}
#endif // NORTH_CLOTHS

static dev_float MIN_WATER_SAMPLE_SIZE = 0.1f;

int CBaseModelInfo::GenerateWaterSamplesFromBoundR(CWaterSample* pWaterSamples,int nNumSamplesInArray,phBound* pBound, int iCurrentComponent)
{
	modelinfoAssert(pWaterSamples);
	modelinfoAssert(pBound);
	if(!pBound)
	{
		return 0;
	}
	else if(pBound->GetType() == phBound::COMPOSITE)
	{
		int nNumSamplesUsed = 0;
		phBoundComposite* pComposite = static_cast<phBoundComposite*>(pBound);
		for(int iCompositeIndex = 0; iCompositeIndex < pComposite->GetNumBounds(); iCompositeIndex++)
		{
			if(pComposite->GetBound(iCompositeIndex))
				nNumSamplesUsed += GenerateWaterSamplesFromBoundR(&(pWaterSamples[nNumSamplesUsed]),nNumSamplesInArray - nNumSamplesUsed, pComposite->GetBound(iCompositeIndex), iCompositeIndex);
		}

		return nNumSamplesUsed;

	}
	else
	{
		if(nNumSamplesInArray == 0)
		{
			return 0;
		}

		// min 2 samples in each direction so it doesn't just roll about in water
		int aNumSamples[3] = {1, 1, 2};

		// need to look at size of object and decide how many samples to use in each direction
		const Vector3 vBBoxMin = VEC3V_TO_VECTOR3(pBound->GetBoundingBoxMin());
		const Vector3 vBBoxMax = VEC3V_TO_VECTOR3(pBound->GetBoundingBoxMax());

		Vector3 vecSize = vBBoxMax - vBBoxMin;
		float fWaterSampleLength = 0.5f*vecSize.x;

		fWaterSampleLength = rage::Max(fWaterSampleLength,MIN_WATER_SAMPLE_SIZE);
		
		// Check material of object. If high density (e.g. stone, concrete) then lower buoyancy
		float fBuoyancyMult = 1.0f;
		phMaterialGta* pMat = (phMaterialGta*)&(pBound->GetMaterial(0));
		if(pMat && pMat->GetDensity() > BASE_SINK_DENSITY_THRESHOLD)
		{
			fBuoyancyMult *= BASE_SINK_MULTIPLIER;
		}


		for(int i=0; i<3; i++)
		{
			float fSize = *(&vecSize.x + i);	// Choose x,y,z of vecSize depending on i
			if(fSize > 6.0f)
			{
				aNumSamples[i] = (int)(fSize / 2.0f);
				if(aNumSamples[i] > 5)
					aNumSamples[i] = 5;
			}
			else if(fSize > 1.0f)
				aNumSamples[i] = 3;
			else if(fSize > 0.3f)
				aNumSamples[i] = 2;

			if(fWaterSampleLength * (float)aNumSamples[i]  > 0.5f*fSize)
				fWaterSampleLength = 0.5f*fSize /(float)aNumSamples[i];
		}

		int nNumSamplesToUse = (s16)(aNumSamples[0] * aNumSamples[1] * aNumSamples[2]);

		// check we didn't create too many sample points
		if(nNumSamplesToUse > nNumSamplesInArray)
		{
			int nSamplesPerSide = (int)rage::Powf((float)nNumSamplesInArray, 1.0f / 3.0f);
			modelinfoAssert(nSamplesPerSide > 0);

			// clamp all but the biggest side to 3 points (5 is max per side normally)
			int nBiggestSide = (vecSize.x > vecSize.y) ? 0 : 1;
			if(vecSize.z > vecSize.x && vecSize.z > vecSize.y)
				nBiggestSide = 2;

			for(int i=0; i<3; i++)
			{
				if(i!=nBiggestSide && aNumSamples[i] > nSamplesPerSide)
					aNumSamples[i] = nSamplesPerSide;
			}
			nNumSamplesToUse = (s16)(aNumSamples[0] * aNumSamples[1] * aNumSamples[2]);
			if(nNumSamplesToUse > nNumSamplesInArray)
			{
				// Forced to clamp biggest side as well
				aNumSamples[nBiggestSide] = nSamplesPerSide;
			}
		}

		modelinfoAssertf(nNumSamplesInArray <= MAX_WATER_SAMPLES, "Object automatic water sample generation made too many points");

		// set them up
		Vector3 vecStart = vBBoxMin;
		Vector3 vecStep(vecSize.x / (float)aNumSamples[0], vecSize.y / (float)aNumSamples[1], vecSize.z / (float)aNumSamples[2]);

		int n = 0;
		Vector3 vecPos;
		int nNumSamplesUsed = 0;

		vecPos[0] = vecStart[0] + 0.5f*vecStep[0];
		for(int i=0; i<aNumSamples[0]; i++, vecPos[0] += vecStep[0])
		{
			vecPos[1] = vecStart[1] + 0.5f*vecStep[1];
			for(int j=0; j<aNumSamples[1]; j++, vecPos[1] += vecStep[1])
			{
				vecPos[2] = vecStart[2] + 0.5f*vecStep[2];
				for(int k=0; k<aNumSamples[2]; k++, vecPos[2] += vecStep[2], n++)
				{
					modelinfoAssert(n < nNumSamplesInArray);
					// want to be very careful we don't overflow the array
					if(n < nNumSamplesInArray)
					{
						pWaterSamples[n].m_vSampleOffset.Set(vecPos);
						pWaterSamples[n].m_fSize = fWaterSampleLength;
						pWaterSamples[n].m_nComponent = static_cast<u16>(iCurrentComponent);
						pWaterSamples[n].m_fBuoyancyMult = fBuoyancyMult;
						nNumSamplesUsed ++;
					}
				}
			}
		}
		return nNumSamplesUsed;
	}	
}

phBound* CBaseModelInfo::GetLargestBoundFromComposite(phBoundComposite* pCompositeBound)
{
	modelinfoAssert(pCompositeBound);
	int iLargestBound = 0;
	float fLargestVolume = 0.0f;
	for(int iCompositeIndex = 0; iCompositeIndex < pCompositeBound->GetNumBounds(); iCompositeIndex++ )
	{
		phBound* pBoundPart = pCompositeBound->GetBound(iCompositeIndex);

		if(!pBoundPart)
		{
			continue;
		}

		if(pBoundPart->GetType() == phBound::COMPOSITE)
		{
			pBoundPart = GetLargestBoundFromComposite(static_cast<phBoundComposite*>(pBoundPart));
		}

		const float fVolume = pBoundPart->GetComputeVolume();
		if(fVolume > fLargestVolume)
		{
			fLargestVolume = fVolume;
			iLargestBound = iCompositeIndex;
		}
	}
	
	phBound* pLargestBound = pCompositeBound->GetBound(iLargestBound);
	if(pLargestBound == NULL)
	{
		modelinfoAssertf(false, "This composite bound has no parts");
		pLargestBound = pCompositeBound;
	}
	else
	{
		pLargestBound->SetIndexInBound(iLargestBound);
	}

	return pLargestBound;
}

// create phArchetype if required, return true if it exists or if was created successfully
bool CBaseModelInfo::CreatePhysicsArchetype()
{
	if (HasPhysics())
	{
		return true;
	}

	if (GetHasLoaded() && GetHasBoundInDrawable())
	{
		gtaDrawable* pDrawable = (gtaDrawable*) GetDrawable();
		if (pDrawable && pDrawable->m_pPhBound)
		{
			// create phArchetype and set it up
			phArchetypeDamp* pPhArch = rage_new phArchetypeDamp();
			if (pPhArch)
			{
				if (GetDrawableType()==DT_DRAWABLE)
				{
					g_DrawableStore.AddRef(strLocalIndex(GetDrawableIndex()), REF_OTHER);
// 					char refString[16];
// 					g_DrawableStore.GetRefCountString(GetDrawableIndex(), refString, sizeof(refString));
// 					Displayf("Physics reference to %s raised to %s", GetModelName(), refString);
				}

				GetDynamicComponentPtr()->ResetPhysicsArchRefs();

				pPhArch->SetBound(pDrawable->m_pPhBound);			
				DEV_ONLY( pPhArch->SetFilename(GetModelName()); )
				pPhArch->AddRef();
				CPhysics::AdditionalSetupModelInfoPhysics(*pPhArch);
				SetPhysics(pPhArch);	// also calls UpdateBoundingVolumes()

				AddRef();

				phBound* pBound = pPhArch->GetBound();

				if( pBound->GetType() == phBound::COMPOSITE )
				{
					phBoundComposite* pCompBound = static_cast<phBoundComposite*>( pBound );

					for(int nComponent = 0; nComponent < pCompBound->GetNumBounds(); ++nComponent)
					{
						if(phBound* pBound = pCompBound->GetBound(nComponent))
						{
							for(int nMatIdx = 0; nMatIdx < pBound->GetNumMaterials(); ++nMatIdx)
							{
								phMaterialMgr::Id nMaterialId = PGTAMATERIALMGR->UnpackMtlId( pBound->GetMaterialId(nMatIdx) );

								if( nMaterialId == PGTAMATERIALMGR->g_idPhysVehicleSpeedUp ||
									nMaterialId == PGTAMATERIALMGR->g_idPhysVehicleSlowDown ||
									nMaterialId == PGTAMATERIALMGR->g_idPhysVehicleRefill ||
									nMaterialId == PGTAMATERIALMGR->g_idPhysVehicleBoostCancel ||
									nMaterialId == PGTAMATERIALMGR->g_idPhysPropPlacement 
									)
								{
									pCompBound->SetIncludeFlags( nComponent, ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_SCRIPT_TEST );
								}

								if( nMaterialId == PGTAMATERIALMGR->g_idPhysVehicleTyreBurst )
								{
									pCompBound->SetIncludeFlags( nComponent, ArchetypeFlags::GTA_WHEEL_TEST );
								}
							}
						}
					}
				}
				else
				{
					for(int nMatIdx = 0; nMatIdx < pBound->GetNumMaterials(); ++nMatIdx)
					{
						phMaterialMgr::Id nMaterialId = PGTAMATERIALMGR->UnpackMtlId( pBound->GetMaterialId(nMatIdx) );

						if( nMaterialId == PGTAMATERIALMGR->g_idPhysVehicleSpeedUp ||
							nMaterialId == PGTAMATERIALMGR->g_idPhysVehicleSlowDown ||
							nMaterialId == PGTAMATERIALMGR->g_idPhysVehicleRefill ||
							nMaterialId == PGTAMATERIALMGR->g_idPhysVehicleBoostCancel ||
							nMaterialId == PGTAMATERIALMGR->g_idPhysPropPlacement 
							)
						{
							pPhArch->SetIncludeFlags( ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_SCRIPT_TEST );
						}

						if( nMaterialId == PGTAMATERIALMGR->g_idPhysVehicleTyreBurst )
						{
							pPhArch->SetIncludeFlags( ArchetypeFlags::GTA_WHEEL_TEST );
						}
					}
				}

				return true;
			}
		}
	}
	
	return false;
}

// destroy phArchetype
void CBaseModelInfo::DeletePhysicsArchetype()
{
	Assertf(HasPhysics(), "No phArchetype to delete for %s", GetModelName());

	if ( GetDynamicComponentPtr() && Verifyf(GetHasBoundInDrawable(), "Deleting phArchetype for %s but it doesn't have any bound data in its drawable", GetModelName()) )
	{
		phArchetypeDamp* pPhArchetype = GetPhysics();

		if (pPhArchetype)
		{
			pPhArchetype->Release(false);

			Assertf( GetDynamicComponentPtr()->GetNumPhysicsArchRefs()==0, "Deleting phArchetype for %s but it has %d refs", GetModelName(), GetDynamicComponentPtr()->GetNumPhysicsArchRefs() );

			delete pPhArchetype;
			SetPhysics(NULL);

			if (GetDrawableType()==DT_DRAWABLE)
			{
				g_DrawableStore.RemoveRef(strLocalIndex(GetDrawableIndex()), REF_OTHER);
// 				char refString[16];
// 				g_DrawableStore.GetRefCountString(GetDrawableIndex(), refString, sizeof(refString));
// 				Displayf("Physics reference to %s dropped to %s", GetModelName(), refString);
			}

			RemoveRef();
		}
	}
}

// increment references to phArchetype to prevent it being deleted
void CBaseModelInfo::AddRefToDrawablePhysics()
{
	if ( GetDynamicComponentPtr() )
	{

		Assertf(HasPhysics(), "Adding a ref to drawable bounds for %s but no phArchetype exists", GetModelName());
		Assertf(GetHasBoundInDrawable(), "Adding a ref to drawable bounds for %s but it doesn't have any", GetModelName());

		GetDynamicComponentPtr()->AddPhysicsArchRef();
	}
}

// decrement references to phArchetype and delete if num refs hit 0 as a result
void CBaseModelInfo::RemoveRefFromDrawablePhysics()
{
	if ( GetDynamicComponentPtr() != NULL )
	{
		GetDynamicComponentPtr()->RemovePhysicsArchRef();

		if (GetDynamicComponentPtr()->GetNumPhysicsArchRefs() == 0)
		{
			DeletePhysicsArchetype();
		}
	}
}

#if __BANK
void CBaseModelInfo::ReloadWaterSamples()
{
	DeleteWaterSamples();
	InitWaterSamples();
}
#endif // __BANK

