///////////////////////////////////////////////////////////////////////////////
//
//  FILE:   DecalCallbacks.h
//
///////////////////////////////////////////////////////////////////////////////

#ifndef DECALS_DECALCALLBACKS_H
#define	DECALS_DECALCALLBACKS_H


///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

// rage
#include "atl/bitset.h"

// framework
#include "vfx/decal/decalcallbacks.h"

// game
#include "ModelInfo/VehicleModelInfo.h"
#include "renderer/Deferred/GBuffer.h"

///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////

#define DECAL_UNDERWATER_WATER_BODY_HEIGHT_DIFF_MIN (ScalarV(V_ONE))
#define DECAL_NUM_VEHICLE_BADGES					(4)									// remember to update the g_typeSettings initialisation if this changes

#define DECAL_HIGH_QUALITY_RANGE_MULT				(1.5f)


///////////////////////////////////////////////////////////////////////////////
//  FORWARD DECLARATIONS
///////////////////////////////////////////////////////////////////////////////

class CEntity;
class CVehicleModelInfo;
class phMaterialGta;


///////////////////////////////////////////////////////////////////////////////
//  ENUMERATIONS
///////////////////////////////////////////////////////////////////////////////

// only in here so DecalDebug.h/cpp and DecalManager.h/cpp can both reference them 
enum DecalType_e
{
	DECALTYPE_BANG								= 0,
	DECALTYPE_BLOOD_SPLAT,
	DECALTYPE_FOOTPRINT,
	DECALTYPE_GENERIC_SIMPLE_COLN,														// only probes for map and buildings
	DECALTYPE_GENERIC_COMPLEX_COLN,														// probes for objects and vehicles as well
	DECALTYPE_GENERIC_SIMPLE_COLN_LONG_RANGE,											// only probes for map and buildings
	DECALTYPE_GENERIC_COMPLEX_COLN_LONG_RANGE,											// probes for objects and vehicles as well
	DECALTYPE_POOL_LIQUID,
	DECALTYPE_SCUFF,
	DECALTYPE_TRAIL_SCRAPE,
	//DECALTYPE_TRAIL_SCRAPE_FIRST,
	DECALTYPE_TRAIL_LIQUID,
	DECALTYPE_TRAIL_SKID,
	DECALTYPE_TRAIL_GENERIC,
	DECALTYPE_VEHICLE_BADGE,
	DECALTYPE_VEHICLE_BADGE_LAST = DECALTYPE_VEHICLE_BADGE + (DECAL_NUM_VEHICLE_BADGES-1),
	DECALTYPE_WEAPON_IMPACT,
	DECALTYPE_WEAPON_IMPACT_SHOTGUN,
	DECALTYPE_WEAPON_IMPACT_SMASHGROUP,
	DECALTYPE_WEAPON_IMPACT_SMASHGROUP_SHOTGUN,
	DECALTYPE_WEAPON_IMPACT_EXPLOSION_GROUND,
	DECALTYPE_WEAPON_IMPACT_EXPLOSION_VEHICLE,
	DECALTYPE_WEAPON_IMPACT_VEHICLE,
	//DECALTYPE_WEAPON_IMPACT_PED,
	//DECALTYPE_LASER_SIGHT,
	DECALTYPE_MARKER,
	DECALTYPE_MARKER_LONG_RANGE,

	DECALTYPE_NUM
};


///////////////////////////////////////////////////////////////////////////////
//  CLASSES
///////////////////////////////////////////////////////////////////////////////

//  CDecalCallbacks  //////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////

class CDecalCallbacks : public decalCallbacks
{
	///////////////////////////////////
	// FUNCTIONS
	///////////////////////////////////

	public: ///////////////////////////

#		if __SPU
			static void SpuGlobalInit();
			void SpuTaskInit(CDecalAsyncTaskDesc *td, const fwEntity* pEntity);
#		endif

		void GlobalInit(const phMaterialGta* materialArray);

		virtual bool IsGlassShader(const grmShader* pShader);
		virtual bool IsGlassMaterial(phMaterialMgr::Id mtlId);
		virtual bool ShouldProcessCompositeBoundFlags(u32 flags);
		virtual bool ShouldProcessCompositeChildBoundFlags(u32 flags);
		virtual bool ShouldProcessDrawableBone(const fwEntity* pEntity, decalBoneHierarchyIdx boneIndex);
		virtual bool ShouldProcessShader(const grmShader* pShader, const fwEntity* pEntity);
		virtual bool ShouldProcessMaterial(u16 typeSettingsIndex, phMaterialMgr::Id mtlId);
		virtual bool ShouldBucketBeRendered(const decalBucket* pDecalBucket, decalRenderPass rp);

		virtual const rmcLod* GetLod(const fwEntity* pEntity, const rmcDrawable* pDrawable);
		virtual unsigned DecompressSmashGroup(decalAsyncTaskDescBase *taskDesc, Vec3V *pos, Vec3V *nrm);


		virtual decalAsyncTaskDescBase *AllocAsyncTaskDesk(decalAsyncTaskType type, const decalSettings& settings, const fwEntity* pEnt, const phInst* pInst);
		virtual void FreeAsyncTaskDesc(decalAsyncTaskDescBase *desc);
		virtual size_t GetAsyncTaskDescSize();

		virtual u32 GetIncludeFlags();
		virtual phLevelNew* GetPhysicsLevel();
		virtual fwEntity* GetEntityFromInst(phInst* pInst);
		virtual phInst* GetPhysInstFromEntity(const fwEntity* pEntity);
		virtual ScalarV_Out GetEntityBoundRadius(const fwEntity* pEntity);
		virtual size_t GetEntitySize(const fwEntity* pEntity);
		virtual size_t GetPhysInstSize(const fwEntity* pEntity, const phInst* pPhInst);
		virtual const rmcDrawable* GetDrawable(const fwEntity* pEntity, const phInst* pInst, bool onlyApplyToGlass, bool dontApplyToGlass);
		virtual	void GetMatrixFromBoneIndex(Mat34V_InOut boneMtx, bool* isDamaged, const fwEntity* pEntity, decalBoneHierarchyIdx boneIndex);
		virtual	void GetMatrixFromBoneIndex_Skinned(Mat34V_InOut boneMtx, bool* isDamaged, const fwEntity* pEntity, decalBoneHierarchyIdx boneIndex);
		virtual	void GetMatrixFromBoneIndex_SmashGroup(Mat34V_InOut boneMtx, const fwEntity* pEntity, decalBoneHierarchyIdx boneIndex);
		virtual bool ShouldProcessInst(const phInst* pInst, const bool bApplyToBreakable);
		virtual void PatchModdedDrawables(unsigned* numDrawables, unsigned* damagedDrawableMask, const rmcDrawable** pDrawables, unsigned numSkelMtxs, const Vec4V* skelMtxs, DECAL_BONEREMAPLUT(Task,Hierarchy) skelBones, u8* skelDrawables, const fwEntity* pEntity);

		virtual void ApplyVehicleSuspensionToMtx(Mat34V_InOut vMtx, const fwEntity* pEntity);

		virtual void BeginRenderDynamic(bool bIsForwardPass);
		virtual void EndRenderDynamic(bool bIsForwardPass);
		virtual const grcTexture* GetDepthTexture();
		virtual const grcTexture* GetNormalTexture();

		virtual void InitGlobalRender();
		virtual void ShutdownGlobalRender();
		virtual void InitBucketForwardRender(Vec4V_In vCentreAndRadius, fwInteriorLocation interiorLocation);
		virtual void SetGlobals(float natural, float artificial, bool isInInterior);
		virtual void GetAmbientFromEntity(const fwEntity* pEntity, float &natural, float &artificial, bool &inInterior);

		virtual Vector4 CalculateProjectionParams(const grcViewport* pViewport = NULL);

		virtual Vec4V_Out GetCameraPosAndFOVScale();
		virtual float GetDistSqrToViewport(Vec3V_In vPosition);
		virtual ScalarV_Out GetDistSqrToViewportV(Vec3V_In vPosition);
		virtual const grcViewport* GetCurrentActiveGameViewport();

		virtual bool IsEntityVisible(const fwEntity* pEntity);
		virtual void GetEntityInfo(const fwEntity* pEntity, u16 typeSettingsIndex, s16 roomId, bool& isPersistent, fwInteriorLocation& interiorLocation, bool& shouldProcess);

		virtual void GetDamageData(const fwEntity* pEntity, fwTexturePayload** ppDamageMap, float& damageRadius, float& damageMult);

		virtual bool IsParticleUpdateThread();

		virtual bool IsUnderwater(Vec3V_In vTestPos);
		virtual bool PerformUnderwaterCheck(Vec3V_In vTestPos);

		virtual bool IsAllowedToRecycle(u16 typeSettingsIndex);

#			if HACK_GTA4_MODELINFOIDX_ON_SPU && USE_EDGE
		virtual int GetModelInfoType(u32 modelIndex);
#			endif

		virtual float GetDistanceMult(u16 typeSettingsIndex);

#if RSG_PC
		virtual void ResetShaders() {};
#endif

		virtual void DecalMerged(int oldDecalId, int newDecalId);
		virtual void LiquidUVMultChanged(int decalId, float uvMult, Vec3V_In pos);

	private: //////////////////////////

#		if !__SPU
			bool ShouldProcessDrawable(const phInst* pInst, const CEntity* pEntity, const rmcDrawable* pDrawable, bool onlyApplyToGlass, bool dontApplyToGlass);
#		endif


		bool IsNoDecalShader(const grmShader* pShader);
		bool IsVehicleShader(const grmShader* pShader);

		const phMaterialGta* m_materialArray;

#		if CREATE_TARGETS_FOR_DYNAMIC_DECALS
			grcRenderTarget *m_pCopyOfGBuffer1;
#		endif

#		if __SPU
			atFixedBitSet<VEH_MAX_DECAL_BONE_FLAGS>* m_pDecalBoneFlags;

#			if DECAL_MANAGER_ENABLE_PROCESSSMASHGROUP
				void   *m_smashgroupVertsLs;
				u32     m_smashgroupVertsOffset;
#			endif
#		endif
};


#endif // DECALS_DECALCALLBACKS_H
