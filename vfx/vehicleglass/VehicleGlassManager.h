// ======================================
// vfx/vehicleglass/VehicleGlassManager.h
// (c) 2012-2013 RockstarNorth
// ======================================

#ifndef VEHICLEGLASSMANAGER_H
#define VEHICLEGLASSMANAGER_H

#include "vfx/vehicleglass/VehicleGlass.h"
#include "network/objects/synchronisation/syncnodes/VehicleSyncNodes.h"
// framework
#include "grcore/edgeExtractgeomspu.h"
#include "vfx/vehicleglass/vehicleglassconstructor.h"

// game
#include "vfx/vehicleglass/VehicleGlassComponent.h"
#include "control/replay/ReplaySettings.h"

// note:
// old smash manager triangle pool was 256 bytes x 1024 = 256KB
// new vehicle glass triangle pool is 208 bytes x 1260 = 255.9KB

#define VEHICLE_GLASS_MAX_TRIANGLES                   (1024*(VEHICLE_GLASS_SMASH_TEST ? 8 : 1)) // max number of smash triangles (the number in a full pool)
#define VEHICLE_GLASS_MAX_COMPONENTS                  (128*(VEHICLE_GLASS_SMASH_TEST ? 4 : 1)) // max number of smash components (the number in a full pool)
#define VEHICLE_GLASS_MAX_COMPONENT_VB                (VEHICLE_GLASS_MAX_COMPONENTS + (USE_GPU_VEHICLEDEFORMATION ? 12 : 18)) // CPU deformation - we need more because we replace the buffers each time the deformation changes
#define VEHICLE_GLASS_MAX_COLLISIONS                  (128)
#define VEHICLE_GLASS_MAX_EXPLOSIONS                  (128)
#define VEHICLE_GLASS_MAX_VERTS                       (VEHICLE_GLASS_SMASH_TEST ?  9000 :  460) // This number is per cache slot
#define VEHICLE_GLASS_MAX_INDICES                     (VEHICLE_GLASS_SMASH_TEST ? 25000 : 1400) // This number is per cache slot
#define VEHICLE_GLASS_CACHE_SIZE                      (3)
#define VEHICLE_GLASS_MAX_VIS_HITS                    (4)
#define VEHICLE_GLASS_MAX_NON_VIS_HITS                (1)
#define VEHICLE_GLASS_DETACH_RANGE                    (24.0f)
#define VEHICLE_GLASS_DETACH_RANGE_SQR                (VEHICLE_GLASS_DETACH_RANGE*VEHICLE_GLASS_DETACH_RANGE)
#define VEHICLE_GLASS_BREAK_RANGE                     (50.0f)
#define VEHICLE_GLASS_BREAK_RANGE_SQR                 (VEHICLE_GLASS_BREAK_RANGE*VEHICLE_GLASS_BREAK_RANGE)
#define VEHICLE_GLASS_RESTORE_RANGE                   (80.0f)
#define VEHICLE_GLASS_RESTORE_RANGE_SQR               (VEHICLE_GLASS_RESTORE_RANGE*VEHICLE_GLASS_RESTORE_RANGE)
#define VEHICLE_GLASS_GRAVITY                         (9.81f)
#define VEHICLE_GLASS_NEIGHBOUR_THRESHOLD             (0.00005f)
#define VEHICLE_GLASS_SMASH_SPHERE_RADIUS_MIN         (0.1f)
#define VEHICLE_GLASS_SMASH_SPHERE_RADIUS_MIN_EXPOSED (0.2f)
#define VEHICLE_GLASS_FORCE_SMASH_DETACH_ALL          (2.0f)

#define VEHICLE_GLASS_SKIP_SPHERE_ON_DETACH_DEFAULT	  (false)

#define VEHICLE_GLASS_DEFAULT_DISTANCE_FIELD_NUM_VERTS_TO_DETACH (CVehicleGlassComponent::VEHICLE_GLASS_TEST_3_VERTICES)
#define VEHICLE_GLASS_DEFAULT_DISTANCE_THRESHOLD_TO_DETACH_MIN   (0.015f)
#define VEHICLE_GLASS_DEFAULT_DISTANCE_THRESHOLD_TO_DETACH_MAX   (0.015f)

#define VEHICLE_GLASS_DEFAULT_CRACK_TEXTURE_AMOUNT      (0.6f)
#define VEHICLE_GLASS_DEFAULT_CRACK_TEXTURE_SCALE       (20.0f)
#define VEHICLE_GLASS_DEFAULT_CRACK_TEXTURE_SCALE_FP    (20.0f)
#define VEHICLE_GLASS_DEFAULT_CRACK_TEXTURE_BUMP_AMOUNT (0.3f)
#define VEHICLE_GLASS_DEFAULT_CRACK_TEXTURE_BUMPINESS   (1.0f)
#define VEHICLE_GLASS_DEFAULT_CRACK_TINT                (Vec4V(73.0f/255.0f, 98.0f/255.0f, 105.0f/255.0f, 1.0f))
#define VEHICLE_GLASS_DEFAULT_FALLING_TINT_SCALE        (20.0f)

#if VEHICLE_GLASS_CLUMP_NOISE
#define VEHICLE_GLASS_CLUMP_NOISE_BUFFER_COUNT (8)
#endif // VEHICLE_GLASS_CLUMP_NOISE

#if VEHICLE_GLASS_DEV
enum eVGWSource
{
	VGW_SOURCE_NONE,
	VGW_SOURCE_DEBUG_STORAGE_WINDOW_PROXY,
	VGW_SOURCE_DEBUG_STORAGE_WINDOW_DATA,
	VGW_SOURCE_VEHICLE_MODEL_WINDOW_DATA,
	VGW_SOURCE_COUNT
};
#endif // VEHICLE_GLASS_DEV


class CVehicleGlassManager
{
private:
	void Reset(bool bClearFlags);

public:
	void Init(unsigned initMode);
	void Shutdown(unsigned shutdownMode);

	void PrepareVehicleRpfSwitch();
	void CompleteVehicleRpfSwitch();

	void UpdateSafe(float deltaTime);
	void UpdateSafeWhilePaused();
	void UpdateBrokenPart(CEntity* pOldEntity, CEntity* pNewEntity, int componentId);
	void Render();
	void RenderManual(CVehicleGlassComponent* pComponent);
	void AddToRenderList(s32 entityListIndex);

	void CleanupComponentEntities();

	void SmashCollision(CEntity* pEntity, int componentId, float force, Vec3V_In vNormal, bool bDetachAll = false);
	void SmashExplosion(CEntity* pEntity, Vec3V_In vPos, float radius, float force);

	void StoreCollision(const VfxCollInfo_s& info, bool bSmash = false, int smashTestModelIndex = -1);
	void StoreExplosion(const VfxExpInfo_s& info, bool bSmash = false);
	CVehicleGlassComponent* CreateComponent(CPhysical* pPhysical, int componentId);
	void RemoveComponent(const CPhysical* pPhysical, int componentId = -1); // deferred until update
	void FreeUpComponentPool();
	void RemoveFurthestComponent(const CPhysical* pPhysicalToIgnore, const CVehicleGlassComponent* pComponentToIgnore);
	void RemoveAllGlass(CVehicle* pVehicle);

	void ApplyVehicleDamage(const CVehicle* pVehicle);
	
	void ConvertibleRoofAnimStart(CVehicle* pVehicle);

	static void PreRender2(CPhysical* pPhysical);

	static bool IsComponentSmashableGlass(const CPhysical* pPhysical, int componentId);
	static bool CanComponentBreak(const CPhysical* pPhysical, int componentId);

	static phBound* GetVehicleGlassWindowBound(const CPhysical* pPhysical, int componentId);

	static const CVehicleGlassVB* GetVehicleGlassComponentStaticVB(const CPhysical* pPhysical, int componentId);
	static Vec4V_Out GetVehicleGlassComponentSmashSphere(const CPhysical* pPhysical, int componentId, bool bLocal);
	static const CVehicleGlassComponent* GetVehicleGlassComponent(const CPhysical* pPhysical, int componentId);
	
#if GTA_REPLAY
	class audSound** GetAnyFallLoopSound(); // what is this for?
#endif // GTA_REPLAY

#if __PS3
	int GetEdgeVertexDataBufferSize() const { return m_edgeVertexDataBufferSize; }
	void* GetEdgeVertexDataBuffer();
	int GetEdgeIndexDataBufferSize() const { return m_edgeIndexDataBufferSize; }
	u16* GetEdgeIndexDataBuffer();

	void PrepareCache();
	void InvalidateCache();
	bool SetModelCachedThisFrame(const grmGeometryEdge* pGeomEdge, u32 modelIndex, int geomIndex);
	int GetModelCache(const grmGeometryEdge* pGeomEdge, u32 modelIndex, int geomIndex);
	bool AddModelCache(const grmGeometryEdge* pGeomEdge, u32 modelIndex, int geomIndex);
	u8* GetCacheBoneData(int cacheIdx, int& BoneOffset1, int& BoneOffset2, int& BoneOffsetPoint, int& BoneIndexOffset, int& BoneIndexStride);
	int GetCacheNumIndices(int cacheIdx);
	int GetCacheNumVertices(int cacheIdx);
	const u16* GetCacheIndices(int cacheIdx);
	const Vec3V* GetCachePositions(int cacheIdx);
	const Vec4V* GetCacheTexcoords(int cacheIdx);
	const Vec3V* GetCacheNormals(int cacheIdx);
	const Vec4V* GetCacheColors(int cacheIdx);
#endif // __PS3

private:

#if __PS3
	bool PrepareCacheForHit(const CPhysical* pPhysical, int componentId = -1);
#endif // __PS3

	CVehicleGlassComponent* ProcessCollision(VfxCollInfo_s& info, bool bSmash);
	CVehicleGlassComponent* ProcessExplosion(VfxExpInfo_s& info, bool bSmash);

	CVehicleGlassComponent* FindComponent(const CPhysical* pPhysical, int componentId);
	
#if VEHICLE_GLASS_CLUMP_NOISE
	void GenerateLowDiscrepancyClumpNoise();
#endif // VEHICLE_GLASS_CLUMP_NOISE

#if __ASSERT
	void CheckGroups();
#endif // __ASSERT

public:
#if __BANK
	void UpdateDebug(); // currently just updates the smash test (which needs to be called from a specific place)
	void RenderDebug() const;
	void InitWidgets();
	bool GetUseHDDrawable() const { return m_useHDDrawable; }
	bool GetIsAllGlassWeak() const;
	static CVehicle* GetSmashWindowTargetVehicle();
	static CVehicleGlassComponent* GetSmashWindowTargetComponent();
#endif // __BANK

private:
	int            m_numCollisionInfos;
	VfxCollInfo_s* m_collisionInfos;
	int            m_numExplosionInfos;
	VfxExpInfo_s*  m_explosionInfos;
#if __DEV
	u8*            m_collisionSmashFlags;
	u8*            m_explosionSmashFlags;
#endif // __DEV

	vfxPoolT<CVehicleGlassTriangle,VEHICLE_GLASS_MAX_TRIANGLES>*   m_trianglePool;
	vfxPoolT<CVehicleGlassComponent,VEHICLE_GLASS_MAX_COMPONENTS>* m_componentPool;
	vfxListT<CVehicleGlassComponent>                               m_componentList;
	vfxPoolT<CVehicleGlassVB,VEHICLE_GLASS_MAX_COMPONENT_VB>*      m_componentVBPool;
	vfxListT<CVehicleGlassVB>                                      m_componentVBList; // List with all the static VB currently in use
	vfxListT<CVehicleGlassVB>                                      m_componentCooldownVBList ; // List with all the static VB in cooldown for delete

	atArray<CVehicleGlassComponentEntity*>	m_componentEntities;

#if __PS3
	int   m_edgeVertexDataBufferSize;
	void* m_edgeVertexDataBuffer;
	int   m_edgeIndexDataBufferSize;
	u16*  m_edgeIndexDataBuffer;

	typedef struct  
	{
		Vec4V* edgeVertexStreams[CExtractGeomParams::obvIdxMax] ;

		// Mesh model identification
		const grmGeometryEdge* pGeomEdge;
		u32 modelIndex;
		int geomIndex;

		u32 lastFrameUsed; // Last frame this cache entry was used on

		// Pointers to raw data storage
		void* vertexDataBuffer;
		u16* indexDataBuffer;

		// Data extraction members
		u8  BoneIndexesAndWeights[2048];
		int BoneOffset1;
		int BoneOffset2;
		int BoneOffsetPoint;
		int BoneIndexOffset;
		int BoneIndexStride;

#if HACK_GTA4_MODELINFOIDX_ON_SPU
		CGta4DbgSpuInfoStruct gtaSpuInfoStruct;
#endif // HACK_GTA4_MODELINFOIDX_ON_SPU

		int numExtractedIndices;
		int numExtractedVerts;

		// Pointer to the geometry extraction job handle
		u32* m_pJobHandle;
	} EDGE_CACHE_SLOT;
	EDGE_CACHE_SLOT m_edgeDataCache[VEHICLE_GLASS_CACHE_SIZE];

#endif // __PS3

#if VEHICLE_GLASS_CLUMP_NOISE
	u8* m_clumpNoiseData;
#endif // VEHICLE_GLASS_CLUMP_NOISE

#if __BANK
	mutable int m_numTriangles;
	mutable int m_numComponents;
	mutable int m_totalVBMemory;
	mutable int m_maxVBMemory;
	bool m_testMPMemCap;

	bool m_RenderVehicleGlassAlphaBucket;
	bool m_RenderDebugCentroid;
	int   m_smashWindowHierarchyId;
	float m_smashWindowForce;
	bool  m_smashWindowDetachAll;
	bool  m_smashWindowShowBoneMatrix;
	bool  m_smashWindowUpdateTessellation; // re-apply tessellation to selected vehicle when rag widgets change
	bool  m_smashWindowUpdateTessellationAll; // re-apply tessellation on all vehicles
	u32   m_smashWindowUpdateRandomSeed;
	Vec4V m_smashWindowCurrentSmashSphere;
#if VEHICLE_GLASS_VERBOSE_DEBUG_OUTPUT
	bool  m_verbose;
	bool  m_verboseHit;
#endif // VEHICLE_GLASS_VERBOSE_DEBUG_OUTPUT
	bool  m_showInfo;
	int   m_maxActiveComponents;
	int   m_maxAttachedTriangles;
	int   m_maxDetachedTriangles;
	int   m_maxAttachedTrianglesPerComponent;
	int   m_maxDetachedTrianglesPerComponent;
#if !VEHICLE_GLASS_COMPRESSED_VERTICES
	int   m_testNormalQuantisationBits;
#endif // !VEHICLE_GLASS_COMPRESSED_VERTICES
	bool  m_useHDDrawable;
#if USE_EDGE
	bool m_bInvalideateGeomCache;
	bool m_bReuseExtractedGeom;
	bool m_bExtractGeomEarly;
#endif // USE_EDGE
	bool  m_forceVisible;
	bool  m_useStaticVB;
	bool  m_sort;
	bool  m_sortReverse;
	bool  m_disableVehicleGlass;
	bool  m_disableRender;
	bool  m_disableFrontFaceRender;
	bool  m_disableBackFaceRender;
	bool  m_disablePosUpdate;
	bool  m_disableSpinAxis;
	bool  m_disableCanDetachTest;
	bool  m_disableDetaching;
	bool  m_ignoreRemoveAllGlass;
	bool  m_detachAllOnExplosion;
#if VEHICLE_GLASS_RANDOM_DETACH
	bool  m_randomDetachEnabled;
	float m_randomDetachThreshold;
#endif // VEHICLE_GLASS_RANDOM_DETACH
	bool  m_tessellationEnabled;
	bool  m_tessellationSphereTest;
	bool  m_tessellationNormalise;
	float m_tessellationScaleMin;
	float m_tessellationScaleMax;
	float m_tessellationSpreadMin;
	float m_tessellationSpreadMax;
	float m_vehicleGlassBreakRange;
	float m_vehicleGlassDetachRange;
#if VEHICLE_GLASS_DEV
	int   m_distanceFieldSource; // eVGWSource
	bool  m_distanceFieldUncompressed;
	bool  m_distanceFieldOutputFiles; // dump debug files when constructing distance fields (very slow, but useful)
#endif // VEHICLE_GLASS_DEV
	int   m_distanceFieldNumVerts;
	float m_distanceFieldThresholdMin;
#if VEHICLE_GLASS_CLUMP_NOISE
	float m_distanceFieldThresholdMax;
	float m_distanceFieldThresholdPhase; // debugging
	float m_distanceFieldThresholdPeriod; // debugging
	bool  m_clumpNoiseEnabled;
	int   m_clumpNoiseSize;
	int   m_clumpNoiseBuckets;
	int   m_clumpNoiseBucketIsolate; // debugging
	int   m_clumpNoiseGlobalOffset; // debugging
	bool  m_clumpNoiseDebugDrawGraphs; // debugging
	bool  m_clumpNoiseDebugDrawAngle; // debugging
	float m_clumpSpread; // 0=no spread, 1=full spread within bucket
	float m_clumpExponentBias;
	float m_clumpRadiusMin;
	float m_clumpRadiusMax;
	float m_clumpHeightMin;
	float m_clumpHeightMax;
#endif // VEHICLE_GLASS_CLUMP_NOISE
	int   m_sphereTestDetachNumVerts;
	bool  m_skipSphereTestOnDetach;
	bool  m_skipDistanceFieldTestOnExtract;
	float m_crackTextureAmount;
	float m_crackTextureScale; // Crack texture scale in normal camera mode
	float m_crackTextureScaleFP; // Crack texture scale in first person camera mode
	float m_crackTextureBumpAmount;
	float m_crackTextureBumpiness;
	Vec4V m_crackTint;
	float m_fallingTintScale;
	bool  m_allowFindGeometryWithoutWindow;
	bool  m_impactEffectParticle;
	bool  m_groundEffectAudio;
	bool  m_groundEffectParticle;
	bool  m_allGlassIsWeak;
	bool  m_onlyApplyDecals;
	bool  m_neverApplyDecals;
	bool  m_deferHits;
#if !USE_GPU_VEHICLEDEFORMATION
	bool  m_applyVehicleDamageOnUpdate;
	bool  m_forceVehicleDamageUpdate;
#endif // !USE_GPU_VEHICLEDEFORMATION
	bool  m_smashWindowsWhenConvertibleRoofAnimPlays;
	bool  m_renderDebug;
	bool  m_renderDebugErrorsOnly;
	bool  m_renderDebugTriangleNormals;
	bool  m_renderDebugVertNormals;
	bool  m_renderDebugExposedVert;
	bool  m_renderDebugSmashSphere;
	bool  m_renderDebugExplosionSmashSphereHistory;
	bool  m_renderDebugOffsetTriangle;
	float m_renderDebugOffsetSpread;
	float m_renderDebugFieldOpacity;
	float m_renderDebugFieldOffset;
	bool  m_renderDebugFieldColours;
	bool  m_renderDebugTriCount;
	bool  m_renderGroundPlane;
	float m_timeScale;
#endif // __BANK

	// common state blocks
	static grcRasterizerStateHandle   ms_rasterizerStateHandle[3]; // back face culling, front face culling
	static grcDepthStencilStateHandle ms_depthStencilStateHandle;
	static grcBlendStateHandle        ms_blendStateHandle;

	friend class CVehicleGlassSmashTestManager;
	friend class CVehicleGlassComponent;
	friend class CVehicleGlassVB;
};

// NOTE -- this function does not actually do a proper sphere-triangle intersection, it just tests for intersection with the vertices
inline bool VehicleGlassIsTriangleInRange(Vec3V_In centre, ScalarV_In radiusSqr, Vec3V_In p0, Vec3V_In p1, Vec3V_In p2)
{
	const ScalarV distSqr0 = MagSquared(p0 - centre);
	const ScalarV distSqr1 = MagSquared(p1 - centre);
	const ScalarV distSqr2 = MagSquared(p2 - centre);

	return IsLessThanOrEqualAll(Min(distSqr0, distSqr1, distSqr2), radiusSqr) != 0;
}

extern CVehicleGlassManager g_vehicleGlassMan;

// TODO -- these are all local, they should be in CVehicleGlassManager
extern dev_float VEHICLEGLASSPOLY_GRAVITY_SCALE;
extern dev_float VEHICLEGLASSPOLY_ROTATION_SPEED;

extern dev_float VEHICLEGLASSPOLY_VEL_MULT_COLN;
extern dev_float VEHICLEGLASSPOLY_VEL_MULT_EXPL;
extern dev_float VEHICLEGLASSPOLY_VEL_RAND_COLN;
extern dev_float VEHICLEGLASSPOLY_VEL_RAND_EXPL;
extern dev_float VEHICLEGLASSPOLY_VEL_DAMP;

extern dev_float VEHICLEGLASSGROUP_RADIUS_MULT;
extern dev_float VEHICLEGLASSGROUP_EXPOSED_SMASH_RADIUS;
extern dev_float VEHICLEGLASSGROUP_SMASH_RADIUS;

extern dev_float VEHICLEGLASSFORCE_NOT_USED;
extern dev_float VEHICLEGLASSFORCE_WEAPON_IMPACT;
extern dev_float VEHICLEGLASSFORCE_SIREN_SMASH;
extern dev_float VEHICLEGLASSFORCE_KICK_ELBOW_WINDOW;
extern dev_float VEHICLEGLASSFORCE_WINDOW_DEFORM;
extern dev_float VEHICLEGLASSFORCE_NO_TEXTURE_THRESH;

extern dev_float VEHICLEGLASSTHRESH_PHYSICAL_COLN_MIN_IMPULSE;
extern dev_float VEHICLEGLASSTHRESH_PHYSICAL_COLN_MAX_IMPULSE;

extern dev_float VEHICLEGLASSTHRESH_VEH_SPEED_MIN;
extern dev_float VEHICLEGLASSTHRESH_VEH_SPEED_MAX;

extern dev_float VEHICLEGLASSTESSELLATIONSCALE_FORCE_SMASH;
extern dev_float VEHICLEGLASSTESSELLATIONSCALE_EXPLOSION;

#endif // VEHICLEGLASSMANAGER_H
