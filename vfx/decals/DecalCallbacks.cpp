///////////////////////////////////////////////////////////////////////////////
//
//  FILE:   DecalCallbacks.cpp
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

#include "DecalCallbacks.h"

// rage
#include "fragment/instance.h"
#include "fragment/type.h"
#include "grcore/viewport.h"
#include "grmodel/geometry.h"
#include "grmodel/model.h"
#include "grmodel/shader.h"
#include "physics/inst.h"
#include "phbound/boundcomposite.h"
#include "phcore/materialmgr.h"
#include "rmcore/drawable.h"
#include "rmcore/lodgroup.h"
#include "string/stringhash.h"
#include "system/dma.h"
#include "system/nelem.h"

// framework
#include "entity/entity.h"
#include "vfx/channel.h"
#include "vfx/decal/decalasynctasktype.h"
#include "vfx/decal/decalbucket.h"
#include "vfx/decal/decaldmahelpers.h"
#include "vfx/decal/decaldmatags.h"
#include "vfx/decal/decalminiidxbufwriter.h"

// game
#include "modelinfo/VehicleModelInfo.h"
#include "physics/gtaMaterialManager.h"
#include "physics/Deformable.h"
#include "renderer/HierarchyIds.h"
#include "renderer/RenderPhases/RenderPhaseStd.h"
#include "renderer/Util/ShaderUtil.h"
#include "scene/Entity.h"
#include "vfx/VfxHelper.h"
#include "vfx/decals/DecalAsyncTaskDesc.h"
#include "vfx/decals/DecalDmaTags.h"
#include "vfx/systems/VfxWheel.h"
#include "vfx/vehicleglass/VehicleGlassVertex.h"
#if !__SPU
#	include "camera/viewports/Viewport.h"
#	include "camera/viewports/ViewportManager.h"
#	include "modelinfo/PedModelInfo.h"
#	include "Peds/ped.h"
#	include "Peds/PlayerInfo.h"
#	include "Peds/rendering/PedVariationDS.h"
#	include "Peds/rendering/PedVariationPack.h"
#	include "Peds/rendering/PedVariationStream.h"
#	include "physics/gtaArchetype.h"
#	include "physics/physics.h"
#	include "renderer/rendertargets.h"
#	include "renderer/Deferred/GBuffer.h"
#	include "scene/world/GameWorld.h"
#	include "scene/world/GameWorldWaterHeight.h"
#	include "vfx/decals/DecalManager.h"
#	include "vfx/vehicleglass/VehicleGlassComponent.h"
#endif


///////////////////////////////////////////////////////////////////////////////
//  OPTIMISATIONS - TURN ON IN OPTIMISATIONS.H
///////////////////////////////////////////////////////////////////////////////

VFX_DECAL_OPTIMISATIONS()


///////////////////////////////////////////////////////////////////////////////
//  GLOBAL VARIABLES
///////////////////////////////////////////////////////////////////////////////

#if !__SPU
static CDecalCallbacks g_decalCallbacks;
decalCallbacks* g_pDecalCallbacks = &g_decalCallbacks;
#else
static char g_decalCallbacksBuf[sizeof(CDecalCallbacks)] ALIGNED(MAX(__alignof(CDecalCallbacks), 16));
decalCallbacks* g_pDecalCallbacks;
#endif


///////////////////////////////////////////////////////////////////////////////
//  CODE
///////////////////////////////////////////////////////////////////////////////

#if __SPU

	///////////////////////////////////////////////////////////////////////////
	//  SpuGlobalInit
	///////////////////////////////////////////////////////////////////////////

	void CDecalCallbacks::SpuGlobalInit()
	{
		// Global ctors don't get run on SPURS jobs, so need to run it here in
		// order to jam the vtable pointer.  When compiling as PIC, we also need
		// to manually fixup the pointers inside the vtable.  There is the
		// -mspurs-task-initiailze linker option, that should call code to do
		// this for us, but that doesn't seem to work.  It is very possible that
		// -mspurs-task-initialize didn't work simply because the hand hacking
		// of the retarted vcproj files was wrong.  This method is ok, except
		// for the reliance on NUM_SPU_VIRTUAL_FUNCTIONS being manually kept up
		// to date.
		g_pDecalCallbacks = rage_placement_new(g_decalCallbacksBuf) CDecalCallbacks;

		// Calculate the PIC offset that needs to be added to each vtable entry.
		// The ila loads the non-relocated address of the relative branch
		// target, which can the be subtracted from the "return address" stored
		// by the branch.
		register u32 picOffset;
		register qword t0, t1;
		__asm__ __volatile__
		(
			"ila    %1,.+8\n\t"
			"brsl   %2,.+4\n\t"
			"sf     %0,%1,%2"
			: "=r"(picOffset),
			  "=r"(t0),
			  "=r"(t1)
		);

		// The same SPURS job can run twice in a row on the same SPU, but with
		// the job streamer policy module relocating the job binary in the mean
		// time (https://ps3.scedev.net/support/issue/114598).  This means that
		// the vtable can already have been adjusted to a non-zero base address,
		// so we need to take that into account.  The vtable is stored
		// .data.rel.ro section.  We'll store our static there too so that it
		// will get moved around with the vtable.
		static u32 s_prevRelocation __attribute__((__section__(".data.rel.ro"))) = 0;
		const s32 adjust = picOffset - s_prevRelocation;
		s_prevRelocation = picOffset;

		// Relocate the vtable entries
		u32 *const vtable = *(u32**)g_decalCallbacksBuf;
		for (unsigned i=0; i<NUM_SPU_VIRTUAL_FUNCTIONS; ++i)
		{
			vtable[i] += adjust;
		}
	}


	///////////////////////////////////////////////////////////////////////////
	//  SpuTaskInit
	///////////////////////////////////////////////////////////////////////////

	void CDecalCallbacks::SpuTaskInit(CDecalAsyncTaskDesc *td, const fwEntity* pEntity)
	{
		const CEntity* pActualEntity = static_cast<const CEntity*>(pEntity);
		if (pActualEntity->GetIsTypeVehicle())
		{
			// Grab the model info structure
			const CVehicleModelInfo* const pVehModelInfoEa = static_cast<CVehicleModelInfo*>(pActualEntity->GetBaseModelInfo());
			decalAssert(pVehModelInfoEa);
			CompileTimeAssert(__alignof(CVehicleModelInfo) >= 4);
			CompileTimeAssert((OffsetOf(CVehicleModelInfo, m_data)&3) == 0);

			u32 structEa = sysDmaGetUInt32((u32)&pVehModelInfoEa->m_data, DMATAG_BLOCKING);
			const CVehicleStructure* const pStructureEa = (CVehicleStructure*)structEa;
			decalAssert(pStructureEa);

			// Grab the mapping from bone hierarchy indices to bone indices
			CompileTimeAssert(__alignof(CVehicleStructure) >= 16);
			CompileTimeAssert((OffsetOf(CVehicleStructure, m_decalBoneFlags)&15) == 0);
			enum { BONE_FLAGS_DMA_SIZE = (sizeof(pStructureEa->m_decalBoneFlags)+15)&~15 };
			atFixedBitSet<VEH_MAX_DECAL_BONE_FLAGS>* const pBoneFlags = (atFixedBitSet<VEH_MAX_DECAL_BONE_FLAGS>*)sysScratchAlloc(BONE_FLAGS_DMA_SIZE);
			sysDmaGet(pBoneFlags, (u32)&pStructureEa->m_decalBoneFlags, BONE_FLAGS_DMA_SIZE, DMATAG_GAME_GETBONEFLAGS);
			m_pDecalBoneFlags = pBoneFlags;
			// don't need to block on this dma completing, all dma tags will be blocked on before calling decalManager::ProcessInst
		}

#		if DECAL_MANAGER_ENABLE_PROCESSSMASHGROUP
			const u32 totalNumVerts = td->smashgroupNumVerts;
			const u32 batchNumVerts = Min<u32>(totalNumVerts, MAX_SMASH_GROUP_VERTS_PER_BATCH);
			const u32 batchSize = batchNumVerts*td->smashgroupStride;
			const u32 allocSize = (batchSize+31)&~15;
			void *const verts = sysScratchAlloc(allocSize);
			m_smashgroupVertsLs = verts;

			const u32 ea = (u32)(td->smashgroupVerts);
			const u32 dmaBeginEa = ea&~15;
			const u32 dmaEndEa = (ea+batchSize+15)&~15;
			m_smashgroupVertsOffset = ea&15;
			sysDmaLargeGet(verts, dmaBeginEa, dmaEndEa-dmaBeginEa, DMATAG_GAME_GETSMASHGROUPVTXS);
			td->smashgroupVerts = (const void*)(ea+batchSize);
#		else
			(void)td;
#		endif
	}


#endif // __SPU


///////////////////////////////////////////////////////////////////////////////
//  GlobalInit
///////////////////////////////////////////////////////////////////////////////

void CDecalCallbacks::GlobalInit(const phMaterialGta* materialArray)
{
	m_materialArray = materialArray;
}


///////////////////////////////////////////////////////////////////////////////
//  IsNoDecalShader
///////////////////////////////////////////////////////////////////////////////

bool CDecalCallbacks::IsNoDecalShader(const grmShader* pShader)
{
	static const u32 sHashes[] =
	{
		ATSTRINGHASH("cutout_fence",                0x4fa432a7),
		ATSTRINGHASH("cutout_fence_normal",         0x063762cf),
	};
	const u32 hash = pShader->GetHashCode();
	for (unsigned i=0; i<NELEM(sHashes); ++i)
	{
		if (hash == sHashes[i])
		{
			return true;
		}
	}

	return false;
}


///////////////////////////////////////////////////////////////////////////////
//  IsVehicleShader
///////////////////////////////////////////////////////////////////////////////

bool CDecalCallbacks::IsVehicleShader(const grmShader* pShader)
{
	static const u32 sHashes[] =
	{
		ATSTRINGHASH("vehicle_licenseplate",	0x8a7a2bef),
		ATSTRINGHASH("vehicle_mesh",			0xd963b58b),
		ATSTRINGHASH("vehicle_mesh_enveff",		0xed5d39c0),
		ATSTRINGHASH("vehicle_mesh2_enveff",	0xa62a444e),
		ATSTRINGHASH("vehicle_paint1",			0xf9fb7331),
		ATSTRINGHASH("vehicle_paint1_enveff",	0x2327939b),
		ATSTRINGHASH("vehicle_paint2",			0xede85b0b),
		ATSTRINGHASH("vehicle_paint2_enveff",	0x0fe76347),
		ATSTRINGHASH("vehicle_paint3",			0xe029bf8e),
		ATSTRINGHASH("vehicle_paint3_enveff",	0xec8e929d),
		ATSTRINGHASH("vehicle_paint4",			0xc0cb80d2),
		ATSTRINGHASH("vehicle_paint4_emissive",	0x72D0AC88),
		ATSTRINGHASH("vehicle_paint4_enveff",	0xaba4513d),	
		ATSTRINGHASH("vehicle_paint5",			0xb305e547),
		ATSTRINGHASH("vehicle_paint5_enveff",	0xe498076b),
		ATSTRINGHASH("vehicle_paint6",			0xa17c4234),
		ATSTRINGHASH("vehicle_paint6_enveff",	0xc30069b8),
		ATSTRINGHASH("vehicle_paint7",			0x953229a0),
		ATSTRINGHASH("vehicle_paint7_enveff",	0x6e94ccc6),
		ATSTRINGHASH("vehicle_vehglass",		0x7c98d207),
		ATSTRINGHASH("vehicle_emissive_opaque", 0x833b47f2),
		ATSTRINGHASH("vehicle_paint8",			0x86598BEF),
		ATSTRINGHASH("vehicle_paint9",			0x77F6EF2A),
	};
	const u32 hash = pShader->GetHashCode();
	for (unsigned i=0; i<NELEM(sHashes); ++i)
	{
		if (hash == sHashes[i])
		{
			return true;
		}
	}

	return false;
}


///////////////////////////////////////////////////////////////////////////////
//  IsGlassShader
///////////////////////////////////////////////////////////////////////////////

bool CDecalCallbacks::IsGlassShader(const grmShader* pShader)
{
	static const u32 sHashes[] =
	{
		ATSTRINGHASH("glass",                       0xd9f8c9bf),
		ATSTRINGHASH("glass_displacement",          0x629c08dc),
		ATSTRINGHASH("glass_emissive",              0x1a910b87),
		ATSTRINGHASH("glass_emissivenight",         0x451700e5),
		ATSTRINGHASH("glass_env",                   0x85aba20b),
		ATSTRINGHASH("glass_pv",					0x706a3722),
		ATSTRINGHASH("glass_pv_env",                0xc5098ee2),
		ATSTRINGHASH("glass_normal_spec_reflect",   0xa587608a),
		ATSTRINGHASH("glass_reflect",               0xb680ab5c),
		ATSTRINGHASH("glass_spec",                  0xa9a6eb84),
		ATSTRINGHASH("vehicle_vehglass",            0x7c98d207),
	};
	const u32 hash = pShader->GetHashCode();
	for (unsigned i=0; i<NELEM(sHashes); ++i)
	{
		if (hash == sHashes[i])
		{
			return true;
		}
	}

	return false;
}


///////////////////////////////////////////////////////////////////////////////
//  IsGlassMaterial
///////////////////////////////////////////////////////////////////////////////

bool CDecalCallbacks::IsGlassMaterial(phMaterialMgr::Id mtlId)
{
	const phMaterialMgr::Id idx = mtlId & phMaterialMgrGta::GetMaterialMask();
#if !__SPU
	const VfxGroup_e grp = m_materialArray[idx].GetVfxGroup();
#else
	CompileTimeAssert(sizeof(__typeof__(*m_materialArray[idx].GetVfxGroupPtr())) == 4);
	const VfxGroup_e grp = (VfxGroup_e)sysDmaGetUInt32((u32)m_materialArray[idx].GetVfxGroupPtr(), DMATAG_BLOCKING);
#endif
	return phMaterialMgrGta::GetIsGlass(grp);
}


///////////////////////////////////////////////////////////////////////////////
//  ShouldProcessCompositeBound
///////////////////////////////////////////////////////////////////////////////

bool CDecalCallbacks::ShouldProcessCompositeBoundFlags(u32 flags)
{
	if ((flags & ArchetypeFlags::GTA_MAP_TYPE_WEAPON) ||
		((flags & ArchetypeFlags::GTA_OBJECT_TYPE) && (flags & ArchetypeFlags::GTA_MAP_TYPE_COVER)==0))
	{
		return true;
	}

	return false;
}


///////////////////////////////////////////////////////////////////////////////
//  ShouldProcessCompositeChildBoundFlags
///////////////////////////////////////////////////////////////////////////////

bool CDecalCallbacks::ShouldProcessCompositeChildBoundFlags(u32 flags)
{
	if ((flags & ArchetypeFlags::GTA_MAP_TYPE_WEAPON) ||
		((flags & ArchetypeFlags::GTA_OBJECT_TYPE) && (flags & ArchetypeFlags::GTA_MAP_TYPE_COVER)==0))
	{
		return true;
	}

	return false;
}


///////////////////////////////////////////////////////////////////////////////
//  ShouldProcessDrawableBone
///////////////////////////////////////////////////////////////////////////////

bool CDecalCallbacks::ShouldProcessDrawableBone(const fwEntity* pEntity, decalBoneHierarchyIdx boneIndex)
{
	const CEntity* pActualEntity = static_cast<const CEntity*>(pEntity);
	if (pActualEntity->GetIsTypeVehicle())
	{
		// This debug flag could be made to work on the SPUs, but not worth the
		// hassle right now
#		if __BANK && !__SPU
			if (g_decalMan.GetDebugInterface().GetDisableVehicleBoneTest())
			{
				return true;
			}
#		endif

#		if !__SPU
			const CBaseModelInfo* const pBaseModelInfo = pActualEntity->GetBaseModelInfo();
			decalAssertf(dynamic_cast<const CVehicleModelInfo*>(pBaseModelInfo), "can't get vehicle model info (%d)", pActualEntity->GetModelIndex());
			const CVehicleModelInfo* const pVehModelInfo = static_cast<const CVehicleModelInfo*>(pBaseModelInfo);
			return pVehModelInfo->GetDecalBoneFlag(boneIndex.Val());
#		else
			return m_pDecalBoneFlags->IsSet(boneIndex.Val());
#		endif
	}
// 	else if (pActualEntity->GetIsTypePed())
// 	{
// 		decalAssertf(0, "trying to process a ped drawable bone");
// 		return false;
// 	}

	return true;
}


///////////////////////////////////////////////////////////////////////////////
//  ShouldProcessShader
///////////////////////////////////////////////////////////////////////////////

bool CDecalCallbacks::ShouldProcessShader(const grmShader* pShader, const fwEntity* pEntity)
{
	// only process certain shaders on vehicles
	const CEntity* pActualEntity = static_cast<const CEntity*>(pEntity);
	if (pActualEntity->GetIsTypeVehicle())
	{
		// This debug flag could be made to work on the SPUs, but not worth the
		// hassle right now
#		if __BANK && !__SPU
			if (g_decalMan.GetDebugInterface().GetDisableVehicleShaderTest())
			{
				return true;
			}
#		endif

		return IsVehicleShader(pShader);
	}
	else
	{
		return !IsNoDecalShader(pShader);
	}
}


///////////////////////////////////////////////////////////////////////////////
//  ShouldProcessMaterial
///////////////////////////////////////////////////////////////////////////////

bool CDecalCallbacks::ShouldProcessMaterial(u16 typeSettingsIndex, phMaterialMgr::Id mtlId)
{
	if (phMaterialMgrGta::GetPolyFlagNoDecal(mtlId))
	{
		return false;
	}

	if (PGTAMATERIALMGR->UnpackMtlId(mtlId)==PGTAMATERIALMGR->g_idTreeBark)
	{
		if (typeSettingsIndex==DECALTYPE_WEAPON_IMPACT_EXPLOSION_GROUND || 
			typeSettingsIndex==DECALTYPE_WEAPON_IMPACT_EXPLOSION_VEHICLE)
		{
			return false;
		}
	}

	if (PGTAMATERIALMGR->UnpackMtlId(mtlId)==PGTAMATERIALMGR->g_idPhysVehicleSpeedUp || 
		PGTAMATERIALMGR->UnpackMtlId(mtlId)==PGTAMATERIALMGR->g_idPhysVehicleSlowDown || 
		PGTAMATERIALMGR->UnpackMtlId(mtlId)==PGTAMATERIALMGR->g_idPhysVehicleRefill ||
		PGTAMATERIALMGR->UnpackMtlId( mtlId ) == PGTAMATERIALMGR->g_idPhysVehicleBoostCancel ||
		PGTAMATERIALMGR->UnpackMtlId( mtlId ) == PGTAMATERIALMGR->g_idPhysPropPlacement 
		)
	{
		return false;
	}

	// note that we don't want to be testing 'shoot through' flags here
	// the decal system should reject polys using the 'no decal' flag and should never test the 'shoot through' flag

	return true;
}



///////////////////////////////////////////////////////////////////////////////
//  ShouldBucketBeRendered
///////////////////////////////////////////////////////////////////////////////

bool CDecalCallbacks::ShouldBucketBeRendered(const decalBucket* pDecalBucket, decalRenderPass rp)
{
	if(rp == DECAL_RENDER_PASS_FORWARD)
	{
		//This check is only done for the forward decals

		vfxAssertf(pDecalBucket, "NULL Bucket passed in");
		vfxAssertf(DRAWLISTMGR->IsExecutingDrawSceneDrawList(), "We shouldn't check for interior location when not in draw scene render phase");
		return CRenderPhaseDrawSceneInterface::RenderAlphaEntityInteriorLocationCheck(pDecalBucket->GetInteriorLocation());
	}

	//if its deferred then just return true
	return true;
}

///////////////////////////////////////////////////////////////////////////////
//  GetLod
///////////////////////////////////////////////////////////////////////////////

const rmcLod* CDecalCallbacks::GetLod(const fwEntity* pEntity, const rmcDrawable* pDrawable)
{
	const rmcLodGroup& lodGroup = pDrawable->GetLodGroup();

#	if !__SPU
		decalAssert(!pEntity || dynamic_cast<const CEntity*>(pEntity));
#	endif
	const CEntity* const pCEntity = static_cast<const CEntity*>(pEntity);
	if (pCEntity && pCEntity->GetIsTypeVehicle())
	{
#		if !__SPU
#			define STR_NON_SPU_ADD_NAME(BASE_STR, NAME)     BASE_STR " (%s)", NAME
#		else
#			define STR_NON_SPU_ADD_NAME(BASE_STR, NAME)     BASE_STR
#		endif

		// this is a vehicle - return the medium vehicle lod
		BLOCKING_DMA_GET(CVehicleModelInfo, pVehModelInfo, static_cast<CVehicleModelInfo*>(pCEntity->GetBaseModelInfo()));
		if (decalVerifyf(pVehModelInfo, STR_NON_SPU_ADD_NAME("can't get vehicle model info", pVehModelInfo->GetModelName())))
		{
			if (pVehModelInfo->GetHasHDFiles())
			{
				// this vehicle uses the new system and has a separate high def drawable
				// so we want to be getting the highest lod from the standard drawable (which is the medium lod of the entire vehicle)
				if (lodGroup.ContainsLod(LOD_HIGH))
				{
					return &lodGroup.GetLod(LOD_HIGH);
				}
			}
			else
			{
				// this vehicle uses the old system and has a single model containing high, medium and low lods
				// so we want to be getting the high lod from the standard drawable (which is the medium lod of the entire vehicle)
				if (lodGroup.ContainsLod(LOD_MED))
				{
					return &lodGroup.GetLod(LOD_MED);
				}
			}

#			if __BANK
				if (DECALMGRASYNC.GetDebug()->IsEnableSpam())
				{
					decalErrorf(STR_NON_SPU_ADD_NAME("no medium vehicle lod available - decals will not work on this vehicle", pVehModelInfo->GetModelName()));
				}
#			endif
		}

#		if __BANK && !__SPU
			if (g_decalMan.GetDebugInterface().GetForceVehicleLod())
			{
				if (lodGroup.ContainsLod(LOD_HIGH))
				{
					decalErrorf(STR_NON_SPU_ADD_NAME("no medium vehicle lod available - projecting onto high lod instead", pVehModelInfo->GetModelName()));
					return &lodGroup.GetLod(LOD_HIGH);
				}
			}
#		endif

#		undef STR_NON_SPU_ADD_NAME
	}
	else if (pCEntity && pCEntity->GetIsTypePed())
	{
		// this is a ped - try to use the medium lod first
		if (lodGroup.ContainsLod(LOD_MED))
		{
			return &lodGroup.GetLod(LOD_MED);
		}
		else if (lodGroup.ContainsLod(LOD_LOW))
		{
			decalErrorf("no medium ped lod available - projecting onto low lod instead");
			return &lodGroup.GetLod(LOD_LOW);
		}
		else if (lodGroup.ContainsLod(LOD_HIGH))
		{
			decalErrorf("no medium ped lod available - projecting onto high lod instead");
			return &lodGroup.GetLod(LOD_HIGH);
		}
	}
	else
	{
		// this is not a vehicle - try to use the high lod first
		if (lodGroup.ContainsLod(LOD_HIGH))
		{
			return &lodGroup.GetLod(LOD_HIGH);
		}
		else if (lodGroup.ContainsLod(LOD_MED))
		{
			return &lodGroup.GetLod(LOD_MED);
		}
		else if (lodGroup.ContainsLod(LOD_LOW))
		{
			return &lodGroup.GetLod(LOD_LOW);
		}
	}

	return NULL;
}


///////////////////////////////////////////////////////////////////////////////
//  DecompressSmashGroup
///////////////////////////////////////////////////////////////////////////////

unsigned CDecalCallbacks::DecompressSmashGroup(decalAsyncTaskDescBase *taskDescBase, Vec3V *pos, Vec3V *nrm)
{
#	if !DECAL_MANAGER_ENABLE_PROCESSSMASHGROUP
		(void)taskDescBase;
		(void)pos;
		(void)nrm;
		return 0;

#	else
		CDecalAsyncTaskDesc *const td = static_cast<CDecalAsyncTaskDesc*>(taskDescBase);
		const u32 numVerts = td->smashgroupNumVerts;
		if (!numVerts)
		{
			return 0;
		}

		const u32 batchNumVerts = Min<u32>(numVerts, MAX_SMASH_GROUP_VERTS_PER_BATCH);
		const u32 remainingNumVerts = numVerts-batchNumVerts;
		td->smashgroupNumVerts = (u16)remainingNumVerts;

#		if __SPU
			sysDmaWaitTagStatusAll(1<<DMATAG_GAME_GETSMASHGROUPVTXS);
			const u8 *vtx0 = (u8*)m_smashgroupVertsLs + m_smashgroupVertsOffset;
#		else
			const u8 *vtx0 = (u8*)(td->smashgroupVerts);
#		endif

		const unsigned stride = td->smashgroupStride;
		for (unsigned i=0; i<batchNumVerts; i+=3)
		{
			const u8 *const vtx1 = vtx0+stride;
			const u8 *const vtx2 = vtx1+stride;

			const Vec3V vPos0 = VehicleGlassVertex_GetVertexP(vtx0);
			const Vec3V vPos1 = VehicleGlassVertex_GetVertexP(vtx1);
			const Vec3V vPos2 = VehicleGlassVertex_GetVertexP(vtx2);
			const Vec3V vNrm0 = VehicleGlassVertex_GetVertexN(vtx0);
			const Vec3V vNrm1 = VehicleGlassVertex_GetVertexN(vtx1);
			const Vec3V vNrm2 = VehicleGlassVertex_GetVertexN(vtx2);

			*pos++ = vPos0;
			*pos++ = vPos1;
			*pos++ = vPos2;
			*nrm++ = vNrm0;
			*nrm++ = vNrm1;
			*nrm++ = vNrm2;

			vtx0 = vtx2+stride;
		}

		if (remainingNumVerts)
		{
			td->smashgroupVerts = vtx0;
#			if __SPU
				const u32 nextBatchNumVerts = Min<u32>(remainingNumVerts, MAX_SMASH_GROUP_VERTS_PER_BATCH);
				const u32 batchSize = nextBatchNumVerts*stride;
				const uptr ea = (uptr)vtx0;
				const u32 dmaBeginEa = ea&~15;
				const u32 dmaEndEa = (ea+batchSize+15)&~15;
				m_smashgroupVertsOffset = ea&15;
				sysDmaLargeGet(m_smashgroupVertsLs, dmaBeginEa, dmaEndEa-dmaBeginEa, DMATAG_GAME_GETSMASHGROUPVTXS);
#			endif
		}

		return batchNumVerts;

#	endif // DECAL_MANAGER_ENABLE_PROCESSSMASHGROUP
}


#if !__SPU

	///////////////////////////////////////////////////////////////////////////
	//  AllocAsyncTaskDesk
	///////////////////////////////////////////////////////////////////////////

	decalAsyncTaskDescBase *CDecalCallbacks::AllocAsyncTaskDesk(decalAsyncTaskType type, const decalSettings& settings, const fwEntity* pEnt, const phInst* pInst)
	{
		(void) pInst;
		(void) pEnt;

		const CVehicleGlassVB *vehicleGlassVB = NULL; // 360 compiler complains (incorrectly) this can be used initialized if you remove the =NULL
		if (type == DECAL_DO_PROCESSSMASHGROUP)
		{
			vehicleGlassVB = ((const CVehicleGlassComponent*)(settings.user.bucketSettings.pSmashGroup))->GetStaticVB();
			if (!vehicleGlassVB)
			{
				decalSpamf("no vehicle glass vertex buffer");
				return NULL;
			}
		}

		CDecalAsyncTaskDesc* const td = static_cast<CDecalAsyncTaskDesc*>(g_decalMan.AllocAsyncTaskDesk());
		if (Likely(td))
		{
			if (type == DECAL_DO_PROCESSSMASHGROUP)
			{
				td->smashgroupVerts     = vehicleGlassVB->GetRawVertices();
				td->smashgroupNumVerts  = (u16)vehicleGlassVB->GetVertexCount();
				td->smashgroupStride    = (u16)vehicleGlassVB->GetVertexStride();
			}
			else
			{
				td->materialArray = m_materialArray;
#				if __ASSERT
					td->debugFwArchetype = pEnt->GetArchetype();
#				endif
			}
		}
		return td;
	}


	///////////////////////////////////////////////////////////////////////////
	//  FreeAsyncTaskDesc
	///////////////////////////////////////////////////////////////////////////

	void CDecalCallbacks::FreeAsyncTaskDesc(decalAsyncTaskDescBase *desc)
	{
		g_decalMan.FreeAsyncTaskDesc(desc);
	}


	///////////////////////////////////////////////////////////////////////////
	//  GetAsyncTaskDescSize
	///////////////////////////////////////////////////////////////////////////

	size_t CDecalCallbacks::GetAsyncTaskDescSize()
	{
		return sizeof(CDecalAsyncTaskDesc);
	}


	///////////////////////////////////////////////////////////////////////////
	//  GetIncludeFlags
	///////////////////////////////////////////////////////////////////////////

	u32 CDecalCallbacks::GetIncludeFlags()
	{
		return  ArchetypeFlags::GTA_MAP_TYPE_WEAPON |
		   	  	ArchetypeFlags::GTA_VEHICLE_TYPE |
	//		  	ArchetypeFlags::GTA_PED_TYPE |
	//		  	ArchetypeFlags::GTA_RAGDOLL_TYPE |
			  	ArchetypeFlags::GTA_OBJECT_TYPE |
			  	ArchetypeFlags::GTA_GLASS_TYPE;
	}


	///////////////////////////////////////////////////////////////////////////
	//  GetPhysicsLevel
	///////////////////////////////////////////////////////////////////////////

	phLevelNew* CDecalCallbacks::GetPhysicsLevel()
	{
		return CPhysics::GetLevel();
	}


	///////////////////////////////////////////////////////////////////////////
	//  GetEntityFromInst
	///////////////////////////////////////////////////////////////////////////

	fwEntity* CDecalCallbacks::GetEntityFromInst(phInst* pInst)
	{
		CEntity* pEntity = CPhysics::GetEntityFromInst(pInst);
		return pEntity;
	}


	///////////////////////////////////////////////////////////////////////////
	//  GetPhysInstFromEntity
	///////////////////////////////////////////////////////////////////////////

	phInst* CDecalCallbacks::GetPhysInstFromEntity(const fwEntity* pEntity)
	{
		const CEntity* pActualEntity = static_cast<const CEntity*>(pEntity);
		if (pActualEntity->GetIsTypePed())
		{
			return static_cast<const CPed*>(pEntity)->GetRagdollInst();
		}
		else
		{
			return pEntity->GetCurrentPhysicsInst();
		}
	}


	///////////////////////////////////////////////////////////////////////////
	//  GetEntityBoundsRadius
	///////////////////////////////////////////////////////////////////////////

	ScalarV_Out CDecalCallbacks::GetEntityBoundRadius(const fwEntity* pEntity)
	{
		// Buildings do not necissarily have any model info, so need to get the
		// bounds from the collision in that case.  Don't want to use the
		// collision in the general case though, as for other entity types, it
		// can change over time (eg. frags).  For entities where the collision
		// does change, that is not a problem for our purposes here with decals,
		// since the decals are applied in bind pose.
		const fwArchetype *const pFwArchetype = pEntity->GetArchetype();
		Vec3V center;
		ScalarV radius;
		if (pFwArchetype)
		{
			const Vec4V sphere = pFwArchetype->GetBoundingSphereV4();
			center = sphere.GetXYZ();
			radius = SplatW(sphere);
		}
		else
		{
			const phInst *const pPhInst = pEntity->GetCurrentPhysicsInst();
			Assert(pPhInst);
			const phArchetype *const pPhArchetype = pPhInst->GetArchetype();
			Assert(pPhArchetype);
			const phBound *const pPhBound = pPhArchetype->GetBound();
			Assert(pPhBound);
			center = pPhBound->GetCentroidOffset();
			radius = pPhBound->GetRadiusAroundCentroidV();
		}
		const ScalarV radiusExpand = Mag(center);
		return radius + radiusExpand;
	}


	///////////////////////////////////////////////////////////////////////////
	//  GetEntitySize
	///////////////////////////////////////////////////////////////////////////

	size_t CDecalCallbacks::GetEntitySize(const fwEntity* pEntity)
	{
		const CEntity* const pCEntity = static_cast<const CEntity*>(pEntity);
		decalAssert(dynamic_cast<const CEntity*>(pEntity));
		if (pCEntity->GetIsDynamic())
		{
			const CDynamicEntity* const pDynamicEntity = static_cast<const CDynamicEntity*>(pCEntity);
			decalAssert(dynamic_cast<const CDynamicEntity*>(pCEntity));
			if (pDynamicEntity->GetIsTypeVehicle())
			{
				return sizeof(CVehicle);
			}
			return sizeof(CDynamicEntity);
		}
		return sizeof(CEntity);
	}


	///////////////////////////////////////////////////////////////////////////
	//  GetPhysInstSize
	///////////////////////////////////////////////////////////////////////////

	size_t CDecalCallbacks::GetPhysInstSize(const fwEntity* pEntity, const phInst* pPhInst)
	{
		(void)pPhInst;
		const fragInst *const pFragInst = pEntity->GetFragInst();
		if (pFragInst)
		{
			decalAssert(pFragInst == pPhInst);
			return sizeof(fragInst);
		}
		return sizeof(phInst);
	}


	///////////////////////////////////////////////////////////////////////////
	//  GetDrawable
	///////////////////////////////////////////////////////////////////////////

	const rmcDrawable* CDecalCallbacks::GetDrawable(const fwEntity* pEntity, const phInst* pInst, bool onlyApplyToGlass, bool dontApplyToGlass)
	{
		// get the entity from the physics inst
		const CEntity *const pCEntity = static_cast<const CEntity*>(pEntity);
		rmcDrawable* pDrawable = pCEntity->GetDrawable();
		if (pDrawable)
		{
			if (ShouldProcessDrawable(pInst, pCEntity, pDrawable, onlyApplyToGlass, dontApplyToGlass))
			{
				return pDrawable;
			}
		}

		return NULL;
	}


	///////////////////////////////////////////////////////////////////////////
	//  ShouldProcessDrawable
	///////////////////////////////////////////////////////////////////////////

	bool CDecalCallbacks::ShouldProcessDrawable(const phInst* pInst, const CEntity* pEntity, const rmcDrawable* pDrawable, bool onlyApplyToGlass, bool dontApplyToGlass)
	{
#		if __BANK
			if (g_decalMan.GetDebugInterface().GetAlwaysUseDrawableIfAvailable())
			{
				return true;
			}
#		endif

		// check if this is a frag
		bool isFrag = false;
		const fragInst* pFragInst = dynamic_cast<const fragInst*>(pInst);
		if (pFragInst)
		{
			isFrag = true;
		}

		// get the lod
		const rmcLod* pLod = GetLod(pEntity, pDrawable);
		if (pLod==NULL)
		{
			if (isFrag)
			{
				// not all children of fragments will have lods
				return true;
			}
			else
			{
				// non fragments should always have lods
				decalAssertf(0, "non fragment found without an lod");
				return false;
			}
		}

		// count the verts
		s32 numVerts = 0;

		s32 modelCount = pLod->GetCount();
		for (s32 j=0; j<modelCount; j++)
		{
			grmModel* pLodModel = pLod->GetModel(j);

			// go through the models geometry groups
			const decalBoneHierarchyIdx boneIndex(pLodModel->GetMatrixIndex());
			if (!ShouldProcessDrawableBone(pEntity, boneIndex))
			{
				continue;
			}

			s32 geomCount = pLodModel->GetGeometryCount();
			for (s32 i=0; i<geomCount; i++)
			{
				int shaderId = pLodModel->GetShaderIndex(i);
				grmShader& shader = pDrawable->GetShaderGroup().GetShader(shaderId);

				// check if the shader is glass and respect the settings passed in
				bool isGlassShader = IsGlassShader(&shader);
				if ((onlyApplyToGlass && !isGlassShader) ||
					(dontApplyToGlass && isGlassShader))
				{
					continue;
				}

				if (!ShouldProcessShader(&shader, pEntity))
				{
					continue;
				}

				grmGeometry& geom = pLodModel->GetGeometry(i);

				// get the vertex buffer
				numVerts += geom.GetVertexCount();
			}
		}

		return numVerts<1000;
	}


	///////////////////////////////////////////////////////////////////////////
	//  GetMatrixFromBoneIndex
	///////////////////////////////////////////////////////////////////////////

	void CDecalCallbacks::GetMatrixFromBoneIndex(Mat34V_InOut boneMtx, bool* isDamaged, const fwEntity* pEntity, decalBoneHierarchyIdx boneIndex)
	{
		const CEntity* pActualEntity = static_cast<const CEntity*>(pEntity);
		CVfxHelper::GetMatrixFromBoneIndex(boneMtx, isDamaged, pActualEntity, boneIndex.Val());
	}


	///////////////////////////////////////////////////////////////////////////
	//  GetMatrixFromBoneIndex_Skinned
	///////////////////////////////////////////////////////////////////////////

	void CDecalCallbacks::GetMatrixFromBoneIndex_Skinned(Mat34V_InOut boneMtx, bool* isDamaged, const fwEntity* pEntity, decalBoneHierarchyIdx boneIndex)
	{
		const CEntity* pActualEntity = static_cast<const CEntity*>(pEntity);
		CVfxHelper::GetMatrixFromBoneIndex_Skinned(boneMtx, isDamaged, pActualEntity, boneIndex.Val());
	}


	///////////////////////////////////////////////////////////////////////////
	//  GetMatrixFromBoneIndex_SmashGroup
	///////////////////////////////////////////////////////////////////////////

	void CDecalCallbacks::GetMatrixFromBoneIndex_SmashGroup(Mat34V_InOut boneMtx, const fwEntity* pEntity, decalBoneHierarchyIdx boneIndex)
	{
		const CEntity* pActualEntity = static_cast<const CEntity*>(pEntity);
		CVfxHelper::GetMatrixFromBoneIndex_Smash(boneMtx, pActualEntity, boneIndex.Val());
	}


	///////////////////////////////////////////////////////////////////////////
	//  ShouldProcessInst
	///////////////////////////////////////////////////////////////////////////

	bool CDecalCallbacks::ShouldProcessInst(const phInst* pInst, const bool bApplyToBreakable)
	{
	// 	const CEntity* pEntity = CPhysics::GetEntityFromInst(pInst);
	// 	if (pEntity->GetIsTypePed())
	// 	{
	// 		return false;
	// 	}

		// don't let any decals be applied to mutliplayer vehicles that are being 'respotted'
		if (NetworkInterface::IsGameInProgress() )
		{
			const CEntity* pEntity = CPhysics::GetEntityFromInst(pInst);
			if (pEntity && pEntity->GetIsTypeVehicle())
			{
				const CVehicle* pVehicle = static_cast<const CVehicle*>(pEntity);
				if (pVehicle && pVehicle->IsBeingRespotted())
				{
					return false;
				}
			}
		}

		if (pInst->IsInLevel())
		{
			const u32 typeFlags = CPhysics::GetLevel()->GetInstanceTypeFlags(pInst->GetLevelIndex());
			bool isWeaponMapType = (typeFlags & ArchetypeFlags::GTA_MAP_TYPE_WEAPON) > 0;
			//	bool isMoverMapType = (typeFlags & ArchetypeFlags::GTA_MAP_TYPE_MOVER) > 0;
			//	bool isCameraMapType = (typeFlags & ArchetypeFlags::GTA_MAP_TYPE_CAMERA) > 0;
			bool isObjectType = (typeFlags & ArchetypeFlags::GTA_OBJECT_TYPE) > 0;
			bool isVehicleType = (typeFlags & ArchetypeFlags::GTA_VEHICLE_TYPE) > 0;
			bool isGlassType = (typeFlags & ArchetypeFlags::GTA_GLASS_TYPE) > 0;

			if (isWeaponMapType || isObjectType || isVehicleType || isGlassType)
			{
				return true;
			}
		}
#	if DECAL_PROCESS_BREAKABLES
		else
		{
			// handle breakable glass that is broken
			if (bApplyToBreakable)
			{
				const fragInst* pFragInst = dynamic_cast<const fragInst*>(pInst);
				if(pFragInst)
				{
					if (fragCacheEntry* pCacheEntry = pFragInst->GetCacheEntry())
					{
						if (fragHierarchyInst* pHierInst = pCacheEntry->GetHierInst())
						{
							// Find at least one valid glass handle
							for (int glassIndex=0; glassIndex!=pHierInst->numGlassInfos; glassIndex++)
							{
								bgGlassHandle handle = pHierInst->glassInfos[glassIndex].handle;
								if (handle != bgGlassHandle_Invalid)
								{
									return true;
								}
							}
						}
					}
				}
			}
		}
#	else
		(void)bApplyToBreakable;
#	endif // DECAL_PROCESS_BREAKABLES

		return false;
	}


	///////////////////////////////////////////////////////////////////////////
	//  PatchModdedDrawables
	///////////////////////////////////////////////////////////////////////////

	void CDecalCallbacks::PatchModdedDrawables(unsigned* numDrawables, unsigned* damagedDrawableMask, const rmcDrawable** pDrawables, unsigned numSkelMtxs, const Vec4V* skelMtxs, DECAL_BONEREMAPLUT(Task,Hierarchy) skelBones, u8* skelDrawables, const fwEntity* pEntity)
	{
		// TODO: Support modded vehicles by updating the drawable index array
		(void) numDrawables;
		(void) damagedDrawableMask;
		(void) pDrawables;
		(void) numSkelMtxs;
		(void) skelMtxs;
		(void) skelBones;
		(void) skelDrawables;
		(void) pEntity;

// 			else if (pEntity->GetIsTypeVehicle())
// 			{
// 				*pIsHardSkinned = true;
// 
// 				int numDrawables = 1;
// 				ppDrawables[0] = pFragType->GetCommonDrawable();
// 
// 				const CVehicle* pVehicle = static_cast<const CVehicle*>(pEntity);
// 				if (pVehicle && pVehicle->IsModded()) 
// 				{
// 					CVehicleVariationInstance vehVariationInst = pVehicle->GetVariationInstance();
// 					const u8* pMods = vehVariationInst.GetMods();
// 					for (u8 i=0; i<VMT_RENDERABLE; i++)
// 					{
// 						if (pMods[i]!=INVALID_MOD)
// 						{
// 							CVehicleStreamRenderGfx* pModRenderGfx = vehVariationInst.GetVehicleRenderGfx();
// 							if (pModRenderGfx)
// 							{
// 								// get bone index info
// 								s32 modHierarchId = vehVariationInst.GetBone(i);
// 								decalBoneHierarchyIdx modBoneIndex = pVehicle->GetBoneIndex((eHierarchyId)modHierarchId);
// 								modBoneIndex = modBoneIndex;
// 
// 								// get the drawable
// 								fragType* pModFragType = pModRenderGfx->GetFrag(i);
// 								if (pModFragType)
// 								{
// 									if (numDrawables >= maxDrawables)
// 									{
// 										decalAssert(numDrawables == maxDrawables);
// 										return -1;
// 									}
// 									ppDrawables[numDrawables++] = pModFragType->GetCommonDrawable();
// 
// 								}
// 							}
// 						}
// 					}
// 				}
// 
// 				return numDrawables;
// 			}
	}


	///////////////////////////////////////////////////////////////////////////
	//  PatchModdedDrawables
	///////////////////////////////////////////////////////////////////////////

	void CDecalCallbacks::ApplyVehicleSuspensionToMtx(Mat34V_InOut vMtx, const fwEntity* pEntity)
	{		
		CVfxHelper::ApplyVehicleSuspensionToMtx(vMtx, pEntity);
	}


	///////////////////////////////////////////////////////////////////////////
	//  BeginRenderDynamic
	///////////////////////////////////////////////////////////////////////////

#if CREATE_TARGETS_FOR_DYNAMIC_DECALS
	void CDecalCallbacks::BeginRenderDynamic(bool bIsForwardPass)
	{
		if( bIsForwardPass )
		{
			const grcRenderTarget *depthTarget = NULL;
			const grcRenderTarget *renderTarget = NULL;
				
			if(GRCDEVICE.IsReadOnlyDepthAllowed() == false)
			{
				// We`ll need to copy the depth buffer.
				CRenderTargets::CopyDepthBuffer(CRenderTargets::GetDepthBuffer(), CRenderTargets::GetDepthBufferCopy());
				depthTarget = CRenderTargets::GetDepthBufferCopy();
			}
			else
			{
				// Use the read only one.
				depthTarget = CRenderTargets::GetDepthBuffer_ReadOnly();
			}

			renderTarget = CRenderTargets::GetBackBuffer();

			// Re-lock the render targets using the read only depth.
			grcTextureFactory::GetInstance().LockRenderTarget(0,renderTarget,depthTarget);
		}
		else
		{
			// Make a copy of the gbuffer1 (the normals), we don`t expect the GBuffer to be resolved.
			m_pCopyOfGBuffer1 = GBuffer::CopyGBuffer(GBUFFER_RT_0, false);

			const grcRenderTarget *depthTarget = NULL;
			const grcRenderTarget *rendertargets[grcmrtColorCount] = {0};

			rendertargets[GBUFFER_RT_0] = GBuffer::GetTarget(GBUFFER_RT_0);
			rendertargets[GBUFFER_RT_1] = GBuffer::GetTarget(GBUFFER_RT_1);
			rendertargets[GBUFFER_RT_2] = GBuffer::GetTarget(GBUFFER_RT_2);
			rendertargets[GBUFFER_RT_3] = GBuffer::GetTarget(GBUFFER_RT_3);

			if(GRCDEVICE.IsReadOnlyDepthAllowed() == false)
			{
				// We`ll need to copy the depth buffer.
				CRenderTargets::CopyDepthBuffer(CRenderTargets::GetDepthBuffer(), CRenderTargets::GetDepthBufferCopy());
				depthTarget = CRenderTargets::GetDepthBufferCopy();
			}
			else
			{
				// Use the read only one.
				depthTarget = CRenderTargets::GetDepthBuffer_ReadOnly();
			}

			// Re-lock the render targets using the read only depth.
			grcTextureFactory::GetInstance().LockMRT(rendertargets, depthTarget);
		}
	}
#else
	void CDecalCallbacks::BeginRenderDynamic(bool /*bIsForwardPass*/)
	{
#if RSG_PC
			decalAssertf(0, "CDecalCallbacks::BeginRenderDynamic()...Rendering dynamic decals without necessary GBuffer copies (probably).");
#endif	//RSG_PC, CREATE_TARGETS_FOR_DYNAMIC_DECALS
	}
#endif


	///////////////////////////////////////////////////////////////////////////
	//  EndRenderDynamic
	///////////////////////////////////////////////////////////////////////////

#if CREATE_TARGETS_FOR_DYNAMIC_DECALS
	void CDecalCallbacks::EndRenderDynamic(bool bIsForwardPass)
	{
		if( bIsForwardPass )
		{
			grcResolveFlags noResolve;

			// We don`t want to resolve the targets yet.
			noResolve.MipMap = false;
			noResolve.NeedResolve = false;

			grcTextureFactory::GetInstance().UnlockRenderTarget(0,&noResolve);
		}
		else
		{
			grcResolveFlags noResolve;
			grcResolveFlagsMrt resolveFlagsMRT;

			// We don`t want to resolve the targets yet.
			noResolve.MipMap = false;
			noResolve.NeedResolve = false;

			for(int i=0; i<rage::grcmrtColorCount; i++)
				resolveFlagsMRT[i] = &noResolve;

			grcTextureFactory::GetInstance().UnlockMRT(&resolveFlagsMRT);
		}
	}
#else
	void CDecalCallbacks::EndRenderDynamic(bool /*bIsForwardPass*/)
	{
		/* No Op */
	}
#endif // CREATE_TARGETS_FOR_DYNAMIC_DECALS


	///////////////////////////////////////////////////////////////////////////
	//  GetDepthTexture
	///////////////////////////////////////////////////////////////////////////

	const grcTexture* CDecalCallbacks::GetDepthTexture()
	{
#if __PPU
		return CRenderTargets::GetDepthBufferAsColor();
#else
		return CRenderTargets::GetDepthBuffer();
#endif
	}


	///////////////////////////////////////////////////////////////////////////
	//  GetNormalTexture
	///////////////////////////////////////////////////////////////////////////

	const grcTexture* CDecalCallbacks::GetNormalTexture()
	{
	#if CREATE_TARGETS_FOR_DYNAMIC_DECALS
		return m_pCopyOfGBuffer1;
	#else
		return GBuffer::GetTarget(GBUFFER_RT_1);
	#endif
	}


	///////////////////////////////////////////////////////////////////////////
	//  InitGlobalRender
	///////////////////////////////////////////////////////////////////////////

	void CDecalCallbacks::InitGlobalRender()
	{
		CShaderLib::SetGlobalNaturalAmbientScale(1.0f);
		CShaderLib::SetGlobalArtificialAmbientScale(1.0f);
		CShaderLib::SetGlobalEmissiveScale(Lights::GetRenderDirectionalLight().GetIntensity());
	}


	///////////////////////////////////////////////////////////////////////////
	//  ShutdownGlobalRender
	///////////////////////////////////////////////////////////////////////////

	void CDecalCallbacks::ShutdownGlobalRender()
	{
		CShaderLib::SetGlobalEmissiveScale(1.0f);
	}


	///////////////////////////////////////////////////////////////////////////
	//  InitBucketForwardRender
	///////////////////////////////////////////////////////////////////////////

	void CDecalCallbacks::InitBucketForwardRender(Vec4V_In vCentreAndRadius, fwInteriorLocation interiorLocation)
	{
		const Vec3V centre = vCentreAndRadius.GetXYZ();
		const Vec3V extent = Vec3V(vCentreAndRadius.GetW());
		const Vec3V boxMin = centre - extent;
		const Vec3V boxMax = centre + extent;

		Lights::UseLightsInArea(spdAABB(boxMin, boxMax), false, false, false);
		CShaderLib::SetGlobalInInterior(interiorLocation.IsValid());
	}


	///////////////////////////////////////////////////////////////////////////
	//  SetGlobals
	///////////////////////////////////////////////////////////////////////////

	void CDecalCallbacks::SetGlobals(float natural, float artificial, bool isInInterior)
	{
		CShaderLib::SetGlobalNaturalAmbientScale(natural);
		CShaderLib::SetGlobalArtificialAmbientScale(artificial);
		CShaderLib::SetGlobalInInterior(isInInterior);
	}


	///////////////////////////////////////////////////////////////////////////
	//  GetAmbientFromEntity
	///////////////////////////////////////////////////////////////////////////
	void CDecalCallbacks::GetAmbientFromEntity(const fwEntity* fwEnt, float &natural, float &artificial, bool &inInterior)
	{
		const CEntity *pEntity = static_cast<const CEntity *>(fwEnt);
		if( pEntity != NULL )
		{
			inInterior = pEntity->GetInteriorLocation().IsValid();

			if( !pEntity->GetIsTypeBuilding() || inInterior )
			{
				natural    = CShaderLib::DivideBy255(pEntity->GetNaturalAmbientScale());
				artificial = CShaderLib::DivideBy255(pEntity->GetArtificialAmbientScale());
			}
			else
			{
				natural    = 1.0f;
				artificial = 0.0f;
			}
		}
	}

	///////////////////////////////////////////////////////////////////////////
	//  CalculateProjectionParams
	///////////////////////////////////////////////////////////////////////////

	Vector4 CDecalCallbacks::CalculateProjectionParams(const grcViewport* pViewport)
	{
		return ShaderUtil::CalculateProjectionParams(pViewport);
	}


	///////////////////////////////////////////////////////////////////////////
	//  GetCameraPosAndFOVScale
	///////////////////////////////////////////////////////////////////////////

	Vec4V_Out CDecalCallbacks::GetCameraPosAndFOVScale()
	{
		return Vec4V(CVfxHelper::GetGameCamPos(), CVfxHelper::GetGameCamFOVScale());
	}


	///////////////////////////////////////////////////////////////////////////
	//  GetDistSqrToViewport
	///////////////////////////////////////////////////////////////////////////

	float CDecalCallbacks::GetDistSqrToViewport(Vec3V_In vPosition)
	{
		return CVfxHelper::GetDistSqrToCamera(vPosition);
	}


	///////////////////////////////////////////////////////////////////////////
	//  GetDistSqrToViewportV
	///////////////////////////////////////////////////////////////////////////

	ScalarV_Out CDecalCallbacks::GetDistSqrToViewportV(Vec3V_In vPosition)
	{
		return CVfxHelper::GetDistSqrToCameraV(vPosition);
	}


	///////////////////////////////////////////////////////////////////////////
	//  GetCurrentActiveGameViewport
	///////////////////////////////////////////////////////////////////////////
	const grcViewport* CDecalCallbacks::GetCurrentActiveGameViewport()
	{
		CViewport* pVp = gVpMan.GetGameViewport();
		if(pVp && pVp->IsActive())
		{
			return &(pVp->GetGrcViewport());
		}
		else
		{
			// if we reached here, then current game viewport was not active
			decalAssertf(true, "no viewport found");
			return NULL;
		}
	}


	///////////////////////////////////////////////////////////////////////////
	//  IsEntityVisible
	///////////////////////////////////////////////////////////////////////////

	bool CDecalCallbacks::IsEntityVisible(const fwEntity* pEntity)
	{
		const CEntity* pActualEntity = static_cast<const CEntity*>(pEntity);
		if (pActualEntity)
		{
			if (pActualEntity->GetIsVisible()==false)
			{
				return false;
			}
		}

		return true;
	}


	///////////////////////////////////////////////////////////////////////////
	//  GetEntityInfo
	///////////////////////////////////////////////////////////////////////////

	void CDecalCallbacks::GetEntityInfo(const fwEntity* pEntity, u16 typeSettingsIndex, s16 roomId, bool& isPersistent, fwInteriorLocation& interiorLocation, bool& shouldProcess)
	{
		// persistent flag
		isPersistent = false;

		CEntity* pPlayerPed = CGameWorld::GetMainPlayerInfo()->GetPlayerPed();
		if (pEntity && pEntity==pPlayerPed)
		{
			isPersistent =  true;
		}

		CEntity* pPlayerVehicleEntity = CGameWorld::GetMainPlayerInfo()->GetPlayerPed()->GetMyVehicle();
		if (pEntity && pEntity==pPlayerVehicleEntity)
		{
			isPersistent =  true;
		}

		if (decalSettingsManager::GetTypeSettings(typeSettingsIndex)->isPersistent)
		{
			isPersistent = true;
		}

		// interior flag
		const CEntity* pActualEntity = static_cast<const CEntity*>(pEntity);

		VfxInteriorTest_e interiorTestResult = CVfxHelper::GetInteriorLocationFromStaticBounds(pActualEntity, roomId, interiorLocation);
		if(interiorTestResult == ITR_NOT_STATIC_BOUNDS)
		{
			interiorLocation = pActualEntity->GetInteriorLocation();
		}

		// don't process deformable props
		shouldProcess = true;
		CDeformableObjectManager& refDeformableObjMgr = CDeformableObjectManager::GetInstance();
		if (pActualEntity)
		{
			const CBaseModelInfo* pBaseModelInfo = pActualEntity->GetBaseModelInfo();
			if (pBaseModelInfo && refDeformableObjMgr.IsModelDeformable(pBaseModelInfo->GetModelNameHash()))
			{
				shouldProcess = false;
			}
		}

		// don't process non frag insts where a frag inst is available
		phInst* pInst = g_decalCallbacks.GetPhysInstFromEntity(pActualEntity);
		if (pActualEntity->m_nFlags.bIsFrag && !IsFragInst(pInst))
		{
			shouldProcess = false;
		}

		// don't process trees
// 		CBaseModelInfo* pBaseModelInfo = pActualEntity->GetBaseModelInfo();
// 		if (pBaseModelInfo)
// 		{
// 			if (pBaseModelInfo->GetIsTree())
// 			{
// 				shouldProcess = false;
// 			}
// 		}
	}


	///////////////////////////////////////////////////////////////////////////
	//  GetDamageData
	///////////////////////////////////////////////////////////////////////////

	void CDecalCallbacks::GetDamageData(const fwEntity* pEntity, fwTexturePayload** ppDamageMap, float& damageRadius, float& damageMult)
	{
		if (decalVerifyf(pEntity, "invalid entity passed"))
		{
			const CVehicle* pVehicle = NULL;

			const CEntity* pActualEntity = static_cast<const CEntity*>(pEntity);
			if (pActualEntity && pActualEntity->GetIsTypeVehicle())
			{
				// it is a vehicle - store the damage texture and bound radius for rendering
				pVehicle = static_cast<const CVehicle*>(pActualEntity);
			}
			else
			{
				if (pActualEntity->GetIsTypeObject())
				{
					const CObject* pActualObject = static_cast<const CObject*>(pEntity);
					if (pActualObject->GetFragParent() && pActualObject->GetFragParent()->GetIsTypeVehicle())
					{
						pVehicle = static_cast<const CVehicle*>(pActualObject->GetFragParent());
					}
				}
			}

			if (pVehicle)
			{
				if (decalVerifyf(pVehicle->GetVehicleDamage(), "invalid vehicle damage on vehicle"))
				{
					if (decalVerifyf(pVehicle->GetVehicleDamage()->GetDeformation(), "invalid deformation on vehicle"))
					{
						*ppDamageMap = pVehicle->GetVehicleDamage()->GetDeformation()->GetDamagePayload();

						damageRadius = pVehicle->GetVehicleDamage()->GetDeformation()->GetBoundRadiusScaled();
						damageMult   = pVehicle->GetVehicleDamage()->GetDeformation()->GetDamageMultiplier();
						return;
					}
				}
			}
		}

		// default if problems were encountered
		*ppDamageMap = NULL;
		damageRadius = 0.0f;
	}

	///////////////////////////////////////////////////////////////////////////
	//  IsParticleUpdateThread
	///////////////////////////////////////////////////////////////////////////

	bool CDecalCallbacks::IsParticleUpdateThread()
	{
		return (sysThreadType::GetCurrentThreadType() & SYS_THREAD_PARTICLES) != 0;
	}


	///////////////////////////////////////////////////////////////////////////
	//  PerformUnderwaterCheck
	///////////////////////////////////////////////////////////////////////////

	bool CDecalCallbacks::PerformUnderwaterCheck(Vec3V_In vTestPos)
	{
		//check if decal is close enough to any water body height wise
		if( BANK_ONLY(g_decalMan.GetDebugInterface().IsWaterBodyCheckEnabled() && ) 
			IsLessThanAll(vTestPos.GetZ() - ScalarV(CGameWorldWaterHeight::GetWaterHeightAtPos(vTestPos)), BANK_SWITCH(g_decalMan.GetDebugInterface().GetWaterBodyCheckHeightDiffMin(), DECAL_UNDERWATER_WATER_BODY_HEIGHT_DIFF_MIN)))
		{
			return true;
		}

		return false;

	}


	///////////////////////////////////////////////////////////////////////////
	//  IsUnderwater
	///////////////////////////////////////////////////////////////////////////

	bool CDecalCallbacks::IsUnderwater(Vec3V_In vTestPos)
	{
// 		float waterDepth;
// 		CVfxHelper::GetWaterDepth(vTestPos, waterDepth);
// 		return waterDepth>0.0f;

		float oceanWaterZ = 0.0f;
		bool foundOceanWater = CVfxHelper::GetOceanWaterZ(vTestPos, oceanWaterZ);
		if (foundOceanWater)
		{
			float oceanWaterDepth = Max(0.0f, oceanWaterZ-vTestPos.GetZf());
			return oceanWaterDepth>0.0f;
		}

		return false;
	}


	///////////////////////////////////////////////////////////////////////////
	//  IsAllowedToRecycle
	///////////////////////////////////////////////////////////////////////////

	bool CDecalCallbacks::IsAllowedToRecycle(u16 typeSettingsIndex)
	{
		if (g_decalMan.GetDisablePetrolDecalsRecycling() && (typeSettingsIndex==DECALTYPE_TRAIL_LIQUID || typeSettingsIndex==DECALTYPE_POOL_LIQUID))
		{
			return false;
		}

		return true;
	}


#	if HACK_GTA4_MODELINFOIDX_ON_SPU && USE_EDGE

		///////////////////////////////////////////////////////////////////////
		//  GetModelInfoType
		///////////////////////////////////////////////////////////////////////

		int CDecalCallbacks::GetModelInfoType(u32 modelIndex)
		{
			return CModelInfo::GetBaseModelInfo(fwModelId(modelIndex))->GetModelType();
		}

#	endif // HACK_GTA4_MODELINFOIDX_ON_SPU


	///////////////////////////////////////////////////////////////////////////
	//  GetDistanceMult
	///////////////////////////////////////////////////////////////////////////

	float CDecalCallbacks::GetDistanceMult(u16 typeSettingsIndex)
	{
		float mult = 1.0f;

		if (typeSettingsIndex==(u16)DECALTYPE_TRAIL_SKID)
		{
			mult *= g_vfxWheel.GetWheelSkidmarkRangeScale();
		}

		if (CFileLoader::GetPtfxSetting()>=CSettings::High)
		{
			return mult * DECAL_HIGH_QUALITY_RANGE_MULT;
		}
		
		return mult;
	}


	///////////////////////////////////////////////////////////////////////////
	//  DecalMerged
	///////////////////////////////////////////////////////////////////////////

	void CDecalCallbacks::DecalMerged(int oldDecalId, int newDecalId)
	{
		g_decalMan.DecalMerged(oldDecalId, newDecalId);
	}

	///////////////////////////////////////////////////////////////////////////
	//  LiquidUVMultChanged
	///////////////////////////////////////////////////////////////////////////

	void CDecalCallbacks::LiquidUVMultChanged(int decalId, float uvMult, Vec3V_In pos)
	{
		g_decalMan.LiquidUVMultChanged(decalId, uvMult, pos);
	}
	

#endif // !__SPU
