// Title	:	"scene/Entity.h"
// Author	:	Richard Jobling
// Started	:	23/10/98
//

#ifndef INC_ENTITY_H_
#define INC_ENTITY_H_

#include "atl/array.h"

// Framework headers
#include "entity/entity.h"
#include "entity/drawdata.h"
#include "fwscene/mapdata/mapdata.h"
#include "fwscene/world/InteriorLocation.h"

// Game headers
#include "audio/entitytracker.h"
#include "audio/audioentity.h"
#include "ModelInfo/ModelInfo.h"
#include "scene/entityTypes.h"
#include "scene/lod/LodDrawable.h"
#include "scene/world/VisibilityMasks.h"
#include "shaders/ShaderLib.h"
#include "control/replay/ReplaySettings.h"
#include "renderer/Renderer.h"
#include "renderer/RenderPhases/RenderPhaseEntitySelect.h"

#if GTA_REPLAY
class CReplayID;
class FrameRef;
#endif

// intended to make some of the compile time asserts more readable
#define CENTITY_INTENDED_SIZE			(16 + FWENTITY_INTENDED_SIZE)

#define STENCIL_VEHICLE_INTERIOR 1

#if STENCIL_VEHICLE_INTERIOR
#define DEFERRED_MATERIAL_INTERIOR_VEH DEFERRED_MATERIAL_SPAREOR2
#define DEFERRED_MATERIAL_NOT_INTERIOR_VEH (0xBF)
#endif


class CEntity;
struct VfxCollisionInfo_s;


namespace rage {
	class audEnvironmentGroupInterface;
	class dlCmdBase;
	class phArchetypeDamp;
	struct MixGroup;
}


#if !defined(PPUVIRTUAL)
#if __SPU
#define PPUVIRTUAL
#else
#define PPUVIRTUAL virtual
#endif
#endif

// class templates
class CGtaEntityDrawData;
class CModelInfo;
class CScene;
class CCustomShaderEffectBase;
class CEntityDrawInfo;

namespace rage
{
	class spdAABB;
	class rmcDrawable;
	class phInst;
	class phCollider;
	class fragInst;
	class fwRect;
}
//class CPhysical;

//
// AF: Please don't add flags to this structure unless you cannot add them somewhere else
// eg CPhysical. I would like to get this down to 32 flags to keep the size of CEntity
// down. I have already moved the DamagedBy flags to CPhysical as they weren't needed
// here.
//
struct CEntityFlags
{
	friend class CEntity;

public:
	u32		bRenderDamaged : 1;				// use damaged LOD models for objects with applicable damage
	u32		bUseAltFadeDistance : 1;		// this object use the alt drawable distance fade on last lod
	u32		bIsProcObject : 1;				// object is a procedural object
	u32		bIsCompEntityProcObject : 1;	// object is a proc object created by comp entity
	u32		bIsFloatingProcObject : 1;		// object is a procedural object
	u32		bLightObject : 1;				// object with lights attached
	u32		bIsFrag : 1;						// this entity is a fragment model
	u32		bInMloRoom : 1;					// object is part of an MLO interior
	u32		bCreatedProcObjects : 1;			// if this entity has active procedural objects
	u32		bPossiblyTouchesWater : 1;		// this entity is possibly affected by water rendering code and is maybe rendered as if wet, can be moved to CPhysical
	u32		bWillSpawnPeds : 1;				// Allows the object from spawning scenario peds if it has 2deffects.
	u32		bAlreadyInAudioList:2;			// all entities have this cleared when the list is reset, 1: already in list, 2: type flags set in list
	u32		bLightsIgnoreDayNightSetting : 1;
	u32		bLightsCanCastStaticShadows : 1;
	u32		bLightsCanCastDynamicShadows : 1;
	u32		bTimedEntityDelayedState:1;
	u32		bIsOnFire : 1;					// is this entity on fire
	u32		bHasExploded : 1;				// Has this guy blown up before
	u32		bHasSpawnPoints : 1;
	u32		bAddtoMotionBlurMask : 1;		// used for motion blur mask
	u32		bIsEntityProcObject : 1;		// DEV ONLY - is a procedural object that was created from another entity (rather than material)
	u32		bAlwaysPreRender : 1;
	u32		trafficLightOverride: 2;				// traffic light override state: 0: red, 1: amber, 2: green, 3: none
	u32		bNeverDummy : 1;				// can be moved to CObject
	u32		bFoundInPvs: 1;					// used by objects and dummy objects, they get flagged when in the pvs for conversion
	u32		bPreRenderedThisFrame: 1;
	u32		bUpdatingThroughAnimQueue : 1;	// used when doing ProcessControl() calls through fwAnimDirector's pre-physics queue (PRE_PHYSICS_QUEUE) - could be moved to CDynamicEntity, probably
	u32		bUseOcclusionQuery : 1;						
	u32		bUseMaxDistanceForWaterReflection : 1;
	u32		bCloseToCamera: 1;				// used by first person props, disables Z-read

	// 32 out of 32 possible bits used
};
CompileTimeAssert(sizeof(CEntityFlags) == sizeof(u32));

enum eIsVisibleModule
{
	SETISVISIBLE_MODULE_DEBUG,
	SETISVISIBLE_MODULE_CAMERA,
	SETISVISIBLE_MODULE_SCRIPT,
	SETISVISIBLE_MODULE_CUTSCENE,
	SETISVISIBLE_MODULE_GAMEPLAY,
	SETISVISIBLE_MODULE_FRONTEND,
	SETISVISIBLE_MODULE_VFX,
	SETISVISIBLE_MODULE_NETWORK,
	SETISVISIBLE_MODULE_PHYSICS,
	SETISVISIBLE_MODULE_WORLD,
	SETISVISIBLE_MODULE_DUMMY_CONVERSION,
	SETISVISIBLE_MODULE_PLAYER,
	SETISVISIBLE_MODULE_PICKUP,
	SETISVISIBLE_MODULE_FIRST_PERSON,
	SETISVISIBLE_MODULE_VEHICLE_RESPOT,
	SETISVISIBLE_MODULE_RESPAWN,
	SETISVISIBLE_MODULE_REPLAY,

	NUM_VISIBLE_MODULES, 
};

class CIsVisibleExtension : public fwExtension
{
private:

	fwFlags32	m_isVisibleFlags;

public:

#if !__SPU
	EXTENSIONID_DECL(CIsVisibleExtension, fwExtension);
#endif

	CIsVisibleExtension()															{ m_isVisibleFlags.SetAllFlags(); }
	
	void SetAllIsVisibleFlags()														{ m_isVisibleFlags.SetAllFlags(); }
	void SetIsVisibleFlag(const eIsVisibleModule module, const bool value)			{ m_isVisibleFlags.ChangeFlag( BIT(module), value ); }
	bool GetIsVisibleFlag(const eIsVisibleModule module) const						{ return m_isVisibleFlags.IsFlagSet( BIT(module) ); }
	u32 GetAllIsVisibleFlags() const												{ return m_isVisibleFlags.GetAllFlags(); }
	bool AreAllIsVisibleFlagsSet() const											{ return m_isVisibleFlags.GetAllFlags() == 0xffffffff; }
	bool AreAllIsVisibleFlagsSetIgnoringModule(const eIsVisibleModule module) const	{ return (m_isVisibleFlags.GetAllFlags() | BIT(module)) == 0xffffffff;	}
};

#if !__NO_OUTPUT
#define EntityDebugfWithCallStack(pEnt, fmt,...) { if(pEnt) {static u32 lastLogTime; if ( (fwTimer::GetFrameCount() - lastLogTime) > 20 ) { char debugStr[512]; CEntity::CommonDebugStr(pEnt, debugStr); Displayf("%s" fmt, debugStr,##__VA_ARGS__); lastLogTime = fwTimer::GetFrameCount();sysStack::PrintStackTrace();}}}
#define EntityDebugfWithCallStackAlways(pEnt, fmt,...) { if(pEnt) { char debugStr[512]; CEntity::CommonDebugStr(pEnt, debugStr); Displayf("%s" fmt, debugStr,##__VA_ARGS__); sysStack::PrintStackTrace();}}
#else
#define EntityDebugfWithCallStack(pEnt, fmt,...) do {} while(false)
#define EntityDebugfWithCallStackAlways(pEnt, fmt,...) do {} while(false)
#endif //!__NO_OUTPUT

class CEntity : public fwEntity
{
	DECLARE_RTTI_DERIVED_CLASS(CEntity, fwEntity);

	friend class CGameWorld;
#if __BANK
	//allows us to change the interior loc of non dynamic entites 
	friend void BruteForceMoveCallback(void);
	friend bool ChangeInterior(void* pItem, void* data);
#endif //__BANK

#if GTA_REPLAY
	friend class CBasicEntityPacketData;
	friend class CReplayMgr;
#endif 

	////////////////////////////////////////////////////////////////////////////
	// Static members:
	////////////////////////////////////////////////////////////////////////////
public:
	static void GetEntityModifiedTargetPosition(const CEntity& entity, const Vector3& vTargeterPosition, Vector3& vEntityTargeteePosition);

	static void InitPoolLookupArray();

	// EJ: Memory Optimization
	bool HasProps() const {return GetNumProps() > 0;}
	virtual u8 GetNumProps() const {return 0;}

#if __BANK
	static void AddInstancePriorityWidget();
	static void AddPriorityWidget(bkBank *pBank);
	static bool ms_cullProps;
	static bool ms_renderOnlyProps;
#endif //#if __BANK

	enum RenderFlags {
#if HAS_RENDER_MODE_SHADOWS
		RenderFlag_USE_CUSTOMSHADOWS	= BIT0 << fwRenderFlag_USER_FLAG_SHIFT,
#endif // HAS_RENDER_MODE_SHADOWS
		RenderFlag_IS_DYNAMIC			= BIT1 << fwRenderFlag_USER_FLAG_SHIFT,
		RenderFlag_IS_PED				= BIT2 << fwRenderFlag_USER_FLAG_SHIFT,
		RenderFlag_IS_GROUND			= BIT3 << fwRenderFlag_USER_FLAG_SHIFT,
		RenderFlag_IS_BUILDING			= BIT4 << fwRenderFlag_USER_FLAG_SHIFT,
		RenderFlag_IS_SKINNED			= BIT5 << fwRenderFlag_USER_FLAG_SHIFT,
	};

	enum eHomingLockOnState {
		HLOnS_NONE,
		HLOnS_ACQUIRING,
		HLOnS_ACQUIRED,
	};

	// fill in the provided storage array with phInst collision exclusions, extracted from a source array of CEntity
	static void GeneratePhysInstExclusionList(const phInst** ppExclusionListStorage, const CEntity** ppExclusionListSource, int nSourceSize);
protected:
	static u32 g_InstancePriority;

	////////////////////////////////////////////////////////////////////////////
	// Instance members:
	////////////////////////////////////////////////////////////////////////////
public:

	CEntity(const eEntityOwnedBy ownedBy);
	virtual ~CEntity();

	PPUVIRTUAL void InitEntityFromDefinition(fwEntityDef* definition, fwArchetype* archetype, s32 mapDataDefIndex);

	virtual void SetModelId(fwModelId modelId);


#if !__SPU
	virtual fwMove *CreateMoveObject();
	virtual const fwMvNetworkDefId &GetAnimNetworkMoveInfo() const;
	virtual crFrameFilter *GetLowLODFilter() const;
#endif // !__SPU

	bool GetIsAddedToWorld() const;
	bool GetIsInExterior() const;
	inline void RequestUpdateInWorld() { UpdateWorldFromEntity(); }
	inline void RequestUpdateInPhysics() { UpdatePhysicsFromEntity(); }

protected:
	// removeAndAdd used for moving (warp resets last matrix too so no collisions in between positions)
	PPUVIRTUAL void UpdatePhysicsFromEntity(bool bWarp=false);			// update entry in physics world
	void UpdateWorldFromEntity();										// update entry in game world

	PPUVIRTUAL void Add();
	PPUVIRTUAL void AddWithRect(const fwRect& rect);
	PPUVIRTUAL void Remove();
	PPUVIRTUAL void AddToInterior(fwInteriorLocation interiorLocation);
	PPUVIRTUAL void RemoveFromInterior();

	PPUVIRTUAL fwDrawData* AllocateDrawHandler(rmcDrawable* pDrawable); // called by CreateDrawable, when its time to actually add a new drawdata

	PPUVIRTUAL u32 GetMainSceneUpdateFlag() const;
	PPUVIRTUAL u32 GetStartAnimSceneUpdateFlag() const;

public:

	bool					IsWithinArea					(float x1, float y1, float x2, float y2) const;
	bool					IsWithinArea					(float x1, float y1, float z1, float x2, float y2, float z2) const;

	CBaseModelInfo* GetBaseModelInfo() const { return(static_cast<CBaseModelInfo*>(m_pArchetype)); }

	rage::audEnvironmentGroupInterface *GetAudioEnvironmentGroup(bool create = false) const;
	virtual naAudioEntity * GetAudioEntity() const;
	void SetAudioInteriorLocation(const fwInteriorLocation intLoc);
	fwInteriorLocation GetAudioInteriorLocation() const;


	inline void SetOwnedBy(const eEntityOwnedBy ownedBy);
	inline u32 GetOwnedBy() const;
	inline void SetTintIndex(const u32 tintIndex);
	inline u32 GetTintIndex() const;
	
	inline void SetNoWeaponDecals(bool bVal);
	inline bool GetNoWeaponDecals();

	inline void SetIsRetainedByInteriorProxy(bool bVal);
	inline bool GetIsRetainedByInteriorProxy(void) const;
	inline void SetRetainingInteriorProxy(s16 interiorIdx);
	inline s16 GetRetainingInteriorProxy(void) const;

	inline void SetAlwaysPreRender(bool bAlwaysPreRender);
	inline bool GetAlwaysPreRender() const;

	inline void SetPreRenderedThisFrame(bool bPreRendered);
	inline bool GetPreRenderedThisFrame() const;

	inline void SetUpdatingThroughAnimQueue(bool bUpdatingThroughAnimQueue);
	inline bool GetUpdatingThroughAnimQueue() const;

	inline void SetUseOcclusionQuery(bool bUseOcclusionQuery);
	inline bool GetUseOcclusionQuery() const;

	inline void SetTimedEntityDelayedState(const bool value);
	inline bool GetTimedEntityDelayedState() const;

	inline void SetUseMaxDistanceForWaterReflection(const bool value);
	inline bool GetUseMaxDistanceForWaterReflection() const;

	inline void SetTypeAnimatedBuilding();
	inline void SetTypeBuilding();
	inline void SetTypeVehicle();
	inline void SetTypePed();
	inline void SetTypeObject();
	inline void SetTypeDummyObject();
	inline void SetTypePortal();
	inline void SetTypeMLO();
	inline void SetTypePrtSys();
	inline void SetTypeVehicleGlassComponent();
	inline void SetTypeLight();
	inline void SetTypeComposite();
	inline void SetTypeInstanceList();
	inline void SetTypeGrassInstanceList();

	inline bool GetIsTypeBuilding() const;
	inline bool GetIsTypeAnimatedBuilding() const;
	__forceinline bool GetIsTypeVehicle() const;
	__forceinline bool GetIsTypePed() const;
	inline bool GetIsTypeObject() const;
	inline bool GetIsTypeDummyObject() const;
	inline bool GetIsTypePortal() const;
	inline bool GetIsTypeMLO() const;
	inline bool GetIsTypePrtSys() const;
	inline bool GetIsTypeLight() const;
	inline bool GetIsTypeComposite() const;
	inline bool GetIsTypeInstanceList() const;
	inline bool GetIsTypeGrassInstanceList() const;

	inline bool GetIsPhysical() const;
	inline bool GetIsDynamic() const;

	inline bool GetUseAltFadeDistance() const;
	inline void SetUseAltFadeDistance(bool value);

	inline bool IsExtremelyCloseToCamera() const;
	inline void SetExtremelyCloseToCamera(bool value);

	PPUVIRTUAL void UpdateRenderFlags();

	inline bool TreatAsPlayerForCollisions();

	bool GetIsStatic() const;
	inline bool GetIsFixedFlagSet() const;
	inline bool GetIsFixedByNetworkFlagSet() const;
	inline bool GetIsFixedUntilCollisionFlagSet() const;
	inline bool GetIsAnyFixedFlagSet() const;
	inline bool GetIsAnyManagedProcFlagSet() const;
	bool GetIsFixedRoot() const;

	void SetIsVisibleForAllModules();
	void SetIsVisibleForModule(const eIsVisibleModule module, bool bIsVisible, bool bProcessChildAttachments = false);
	bool GetIsVisibleForModule(const eIsVisibleModule module) const;
	bool GetIsVisibleIgnoringModule(const eIsVisibleModule module) const;
	inline bool GetIsVisible() const;
	void CopyIsVisibleFlagsFrom(CEntity* src, bool bProcessChildAttachments = false);

#if (__BANK && __PS3) || DRAWABLE_STATS
	u16 GetDrawableStatsContext();
#endif

	inline void SetIsSearchable(bool bIsSearchable);
	inline bool GetIsSearchable() const;

	inline void SetIplIndex(s32 index);
	inline s32 GetIplIndex() const;
	inline void SetAudioBin(u8 audioBin);
	inline u8 GetAudioBin() const;

	inline void ReleaseImportantSlodAssets()
	{
		if (IsProtectedBaseFlagSet(fwEntity::HAS_SLOD_REF))
		{
			CModelInfo::ClearAssetRequiredFlag(GetModelId(), STRFLAG_DONTDELETE);
			AssignProtectedBaseFlag(fwEntity::HAS_SLOD_REF, false);
		}
	}
	inline bool ImportantEmissive() const { return (m_iplIndex==ms_DowntownIplIndex || m_iplIndex==ms_VinewoodIplIndex); }

	// -portal related data-
	const fwInteriorLocation GetInteriorLocation() const;

	inline bool	GetIsInInterior() const;
	inline bool	GetAllowLoadChildren() const;
	inline void	SetAllowLoadChildren(bool set);
	virtual bool GetIsReadyForInsertion() { return(true); }

	inline bool GetIsHDTxdCapable() const;
	virtual inline bool GetIsCurrentlyHD() const;

	inline void ResetChildAlpha() { m_childAlpha = 0; }
	inline void UpdateChildAlpha(u32 alpha) { m_childAlpha = Max(alpha, m_childAlpha); }
	inline u32 GetChildAlpha() const { return m_childAlpha; }

	inline u8 GetDeferredMaterial() const
	{
		int type = GetBaseDeferredType();

		type |= GetIsInInterior() ? DEFERRED_MATERIAL_INTERIOR : 0x0;
		type |= GetLodData().IsParentOfInterior() ? DEFERRED_MATERIAL_INTERIOR : 0x0;

#if STENCIL_VEHICLE_INTERIOR
		type |= GetUseVehicleInteriorMaterial() ? DEFERRED_MATERIAL_INTERIOR_VEH : 0x0;
#endif

		return (type & 0xFF);
	}

	// There is a set function for bIsFixedWaitingForCollision so it can be kept properly private
	virtual void SetIsFixedWaitingForCollision(const bool bFixed) { AssignProtectedBaseFlag(fwEntity::IS_FIXED_UNTIL_COLLISION, bFixed);}

	bool GetShouldUseScreenDoor() const;

	virtual bool CanBeDeleted(void) const;

	// registering audio effects (2deffects) relating to this entity
	void TellAudioAboutAudioEffectsAdd();
	void TellAudioAboutAudioEffectsRemove();

	// adding/removing entity from audio mix groups: needs to be implemented at the entity level containing an audioEntity/occlusion group
	// so this version should never be called
	void AddToMixGroup(s16 mixGroupIndex);
	void RemoveFromMixGroup();
	s32 GetMixGroupIndex();

	// Returns the audTracker, if this type of entity has one
	audTracker *GetPlaceableTracker();
	const audTracker *GetPlaceableTracker()const;

#if __DEV
	void	DrawOnVectorMap(Color32 col);
#endif

	inline rmcDrawable* GetDrawable() const;
	inline u32 GetPhaseLODs() const;
	inline u32 GetLastLODIdx() const;

	PPUVIRTUAL bool CreateDrawable();
	PPUVIRTUAL void DeleteDrawable();

	PPUVIRTUAL Vector3 GetBoundCentre() const;
	PPUVIRTUAL void GetBoundCentre(Vector3& centre) const;	// quicker version
	PPUVIRTUAL float GetBoundRadius() const;
	PPUVIRTUAL float GetBoundCentreAndRadius(Vector3& centre) const; // get both in only 1 virtual function and help to simplify SPU code
	PPUVIRTUAL void GetBoundSphere(spdSphere& sphere) const;
	PPUVIRTUAL const Vector3& GetBoundingBoxMin() const;
	PPUVIRTUAL const Vector3& GetBoundingBoxMax() const;
	PPUVIRTUAL FASTRETURNCHECK(const spdAABB &) GetBoundBox(spdAABB& box) const;

	inline spdSphere GetBoundSphere() const { spdSphere sphere; GetBoundSphere(sphere); return sphere; } // convenience wrapper
	inline spdAABB GetBoundBox() const { spdAABB box; return GetBoundBox(box); } // convenience wrapper
	inline fwRect GetBoundRect() const { spdAABB tempBox; const spdAABB &box = GetBoundBox(tempBox); return fwRect(box); } // convenience wrapper
	void GetBounds(spdAABB &box, spdSphere &sphere) const;

	bool GetIsTouching(const CEntity* pEntity) const;
	bool GetIsTouching(const Vector3& centre, float radius) const;
	bool GetIsOnScreen(bool bIncludeNetworkPlayers = false) const;
	bool GetIsVisibleInViewport(s32 viewport);

	bool IsVisible() const; 		// This is called to check whether to call the render function

	u32 GetLodRandomSeed(CEntity* pLod);

	// does this entity need to go on the ProcessControl update list?
	PPUVIRTUAL bool RequiresProcessControl() const;
	PPUVIRTUAL bool ProcessControl();
	PPUVIRTUAL bool TestCollision(Matrix34* UNUSED_PARAM(pTestMat), const phInst** UNUSED_PARAM(ppExcludeList), u32 UNUSED_PARAM(numExclusions));
	PPUVIRTUAL void Teleport(const Vector3& UNUSED_PARAM(vecSetCoors), float UNUSED_PARAM(fSetHeading), bool UNUSED_PARAM(bCalledByPedTask), bool UNUSED_PARAM(bTriggerPortalRescan), bool UNUSED_PARAM(bCalledByPedTask2), bool UNUSED_PARAM(bWarp), bool UNUSED_PARAM(bKeepRagdoll), bool UNUSED_PARAM(bResetPlants));
	PPUVIRTUAL void ProcessFrozen(); // Called when the entity is frozen instead of process control

	PPUVIRTUAL ePrerenderStatus PreRender(const bool bIsVisibleInMainViewport = true);

	PPUVIRTUAL void StartAnimUpdate(float UNUSED_PARAM(ufTimerStep));
	PPUVIRTUAL void UpdateVelocityAndAngularVelocity(float UNUSED_PARAM(ufTimerStep));

	PPUVIRTUAL bool SetupLighting();		// in renderer.cpp
	PPUVIRTUAL void RemoveLighting(bool);	// in renderer.cpp

	inline u32 GetTrafficLightOverride() { return m_nFlags.trafficLightOverride; }
	inline void SetTrafficLightOverride(u32 tlOverride) { Assert(tlOverride<=0x3); m_nFlags.trafficLightOverride = tlOverride; }


	PPUVIRTUAL void	Update_HD_Models(void) {}		// handle requests for high detail models	

	PPUVIRTUAL void FlagToDestroyWhenNextProcessed(void);

	// Ambient lighting
	inline void SetNaturalAmbientScale(u32 scale) { m_naturalAmbientScale =  static_cast<u8>(scale); }
	inline u32 GetNaturalAmbientScale(void) const { return m_naturalAmbientScale; }

	inline void SetArtificialAmbientScale(u32 scale){ m_artificialAmbientScale = static_cast<u8>(scale); }
	inline u32 GetArtificialAmbientScale(void) const { return m_artificialAmbientScale; }

	void SetupAmbientScaleFlags();
	inline void SetUseAmbientScale(bool bUse) { m_useAmbientScale = bUse; }
	inline void SetUseDynamicAmbientScale(bool bUse) { m_useDynamicAmbientScale = bUse; }
	inline void SetSetupAmbientScaleFlag(bool bUse) { m_setupAmbientScaleFlags = bUse; }
	inline void SetCalculateAmbientScaleFlag(bool bCalculate) { m_calculateAmbientScales = bCalculate; }

	bool inline GetUseAmbientScale() const { return m_useAmbientScale; }
	bool inline GetUseDynamicAmbientScale() const { return m_useDynamicAmbientScale; }

#if STENCIL_VEHICLE_INTERIOR
	inline void SetUseVehicleInteriorMaterial(bool bUse) { m_useVehicleInteriorMaterial = bUse; }
	inline bool GetUseVehicleInteriorMaterial() const { return m_useVehicleInteriorMaterial; }
#endif

	void CalculateAmbientScales();
	virtual void CalculateDynamicAmbientScales();

	float  CalcLightCrossFadeRatio(CEntity *pEntityForDistance,float unscaledLodDist,bool treatAsLod);

	void CalculateBBProjection(Vector3 *pPoint1, Vector3 *pPoint2,Vector3 *pPoint3, Vector3 *pPoint4);

	// Lock on targeting position overridden by inherited classes, i.e. by peds to target the chest
	virtual void GetLockOnTargetAimAtPos(Vector3& aimAtPos) const;
	virtual float GetLockOnTargetableDistance() const;
	virtual void SetLockOnTargetableDistance(float fTargetableDist);
	virtual void SetHomingLockOnState( eHomingLockOnState ) {}
	virtual void SetHomingProjectileDistance( float ) {}

	// 2d effects
	void Update2dEffects(float lightFade, const atArray<C2dEffect*>* pEffectArray, const atArray<CLightAttr>* pLightArray);
	void Render2dEffects();
	C2dEffect* GetRandom2dEffect(s32 effectType, bool mustBeFree);
	C2dEffect* GetClosest2dEffect(s32 effectType, const Vector3& vPos);

	// particle effects
	void ProcessFxEntityCollision(const Vector3& pos, const Vector3& normal, const u32 componentId, float accumImpulse);
	void ProcessFxEntityShot(const Vector3& pos, const Vector3& normal, const u32 componentId, const CWeaponInfo* pWeaponInfo, CEntity* pFiringEntity);
	void ProcessFxFragmentBreak(s32 brokenBoneTag, bool isChildEntity, s32 parentBoneTag=-1);
	void ProcessFxFragmentDestroy(s32 destroyedBoneTag, Matrix34& fxMat);
	void ProcessFxAnimRegistered();
	void ProcessFxAnimTriggered();

	bool TryToExplode(CEntity* pCulprit, bool bNetworkExplosion = true);

	virtual void ProcessCollisionVfx(VfxCollisionInfo_s& vfxColnInfo);

	//void SetReducedBBForPathfind(bool bOn);
	bool IsReducedBBForPathfind() const;
	// Whether this entity is being avoided by pathserver
	bool IsInPathServer(void) const;
	// Whether this entity couldn't be added - so we should keep trying
	bool WasUnableToAddToPathServer(void);

	// GTA_RAGE_PHYSICS
	void SetPhysicsInst(phInst *inst, bool bAddPhysRef);	// set the phInst to be used
	void DeleteInst(bool bCalledFromDestructor = false);
	// is the dynamic cast really needed ? we already now it's a frag, and dynamic_cast is slow as fuck...
	PPUVIRTUAL fragInst* GetFragInst() const;// { return (m_nFlags.bIsFrag ? dynamic_cast<fragInst*>(GetCurrentPhysicsInst()) : NULL); }
	phCollider *GetCollider();
	const phCollider *GetCollider() const;
	phArchetypeDamp* GetPhysArch();
	const phArchetypeDamp* GetPhysArch() const;

	PPUVIRTUAL int  InitPhys();
	// add and remove for physics world
	PPUVIRTUAL void AddPhysics();
	PPUVIRTUAL void RemovePhysics();

	PPUVIRTUAL void GainingFragCacheEntry(fragCacheEntry& entry);
	PPUVIRTUAL void LosingFragCacheEntry();

	// Control whether this entity has any collision or not. If the optional physics instance argument is NULL, the current
	// physics inst will be used.
	PPUVIRTUAL void EnableCollision(phInst* pInst=NULL, u32 nIncludeflags = 0);
	PPUVIRTUAL void DisableCollision(phInst* pInst=NULL, bool bCompletelyDisabled=false);
	inline bool IsCollisionEnabled() const { return IsProtectedBaseFlagSet(fwEntity::USES_COLLISION); }

	bool GetIsCollisionCompletelyDisabled();
	void SetIsCollisionCompletelyDisabled();

	// fill in the provided storage array with collision exclusions, up to the provided max storage
	// returns the actual number of exclusions
	// Pass in the type and include flags of the test to avoid adding unnecessary things to the exclusion list
	PPUVIRTUAL void GeneratePhysExclusionList(const CEntity** ppExclusionListStorage, int& nStorageUsed, int nMaxStorageSize,u32 iIncludeFlags, u32 iTypeFlags, const CEntity* pOptionalExclude=NULL) const;

	// returns the physics instance include flags that the entity is initially set up with. If the optional physics instance argument is NULL, the current
	// physics inst will be used.
	virtual u32 GetDefaultInstanceIncludeFlags(phInst* pInst=NULL) const;

	void SetMaxSpeed(float fMaxSpeed);

	//NETWORK_OBJECT_BASECLASS_DECL();
	// PPUVIRTUAL entity functions 
	PPUVIRTUAL void UpdateNetwork();
	PPUVIRTUAL void ProcessControlNetwork();
	void SetNetworkVisibility(bool bVisible, const char* reason);

	void ChangeVisibilityMask(u16 flags, bool value, bool bProcessChildAttachments);

	void SwapModel(u32 newModelIndex);
	void SetHidden(bool bHidden);

#if __BANK
	bool GetGuid(u32& guid);
#else
	bool GetGuid(u32&) { return false; }
#endif	//__DEV

#if __BANK
	void GetIsVisibleInfo(char* buf, const int maxLen) const;
	static void ResetNetworkVisFlagCallStackData();
	static void TraceNetworkVisFlagsRemovePlayer(u16 objectID);
#endif

#if !__NO_OUTPUT
	void ProcessFailmarkVisibility() const;
	void CommonDebugStr(CEntity* pEnt, char * debugStr); 
#endif

	virtual void SetHasExploded(bool *bNetworkExplosion = NULL);

#if GTA_REPLAY
	void	SetCreationFrameRef(const FrameRef& frameRef);
	bool	GetCreationFrameRef(FrameRef& frameRef) const;
	bool	GetUsedInReplay() const;
#endif

#if !__NO_OUTPUT
	const char* GetLogName() const;
#endif // !__FINAL

private:
	__forceinline void Update2dEffect(const C2dEffect& effect, int index, float lightFade, bool hasFxEntityAmbient, bool hasFxEntityInWater);

	//////////////////////////////////////////////////////////////////////////

public: // AF: Why the fuck is this public
	CEntityFlags		m_nFlags;							// 4 bytes

	static atRangeArray<fwBasePool *, ENTITY_TYPE_TOTAL> ms_EntityPools;

private:
	u32					m_naturalAmbientScale		:8;
	u32					m_artificialAmbientScale	:8;
	u32					m_useAmbientScale			:1;	
	u32					m_useDynamicAmbientScale	:1;
	u32					m_setupAmbientScaleFlags	:1;
	u32					m_bIsInteriorStaticCollider	:1;		// need this to avoid expense in portal tracker probes
	u32					m_calculateAmbientScales	:1;		// optimization flag. We don't need to calculate ambient scales if they get passed in from another object
	u32					m_debugPriority				:2;
	u32					m_debugIsFixed				:1;
	u32					m_childAlpha				:8;

	u32					m_audioBin					:8;
	u32					m_iplIndex					:16;	// used to define which IPL file object is in
	u32					m_ownedBy					:5;
	u32					m__padb003					:3;

	//	PLEASE NOTE!
	//		This flag was needed by the CEntityBatch system to tell if a linked entity was rendered or not. Unfortunately, there is no
	//		great place to reset this bit for all entities. In the case of CEntityBatch, the class has a way to directly access linked 
	//		entities, so it was able to efficiently reset this value at the beginning of the frame.
	//		So unless you are accessing this from an entity that is directly linked to to a CEntityBatch, or have added some other way
	//		to reset this flag, it may very likely be invalid.
	u32					m_bRenderedInGBufThisFrame	:1;
#if STENCIL_VEHICLE_INTERIOR
	u32					m_useVehicleInteriorMaterial:1;
#endif
	u32					m_tintIndex					:6;
	u32					m_noWeaponDecals			:1;
	u32					m_baseDeferredType			:8;
	u32					m__padb025					:15;

	// 30 bits of padding spare here

public:
	u32					GetBaseDeferredType() const { return m_baseDeferredType; }
	void				SetBaseDeferredType(u32 type) { m_baseDeferredType = type; }
	void				SetIsInteriorStaticCollider(bool val)			{ m_bIsInteriorStaticCollider = val; }
	bool				GetIsInteriorStaticCollider() const				{ return(m_bIsInteriorStaticCollider); }

	void				SetAddedToDrawListThisFrame(u32 drawListType, bool wasAdded = true);
	bool				GetAddedToDrawListThisFrame(u32 drawListType) const;

#if __BANK
	virtual void		SetDebugPriority(int priority, bool bIsFixed)	{ m_debugPriority = (u32)priority; m_debugIsFixed = bIsFixed ? 1 : 0; }
	int					GetDebugPriority() const						{ return (int)m_debugPriority; }
	bool				GetDebugIsFixed() const							{ return (bool)m_debugIsFixed; }
	bool				GetIsUsedInMP() const							{ return m_debugPriority <= (u32)fwMapData::GetEntityPriorityCap() || m_debugIsFixed; }
#endif

	static u32 ms_DowntownIplIndex;
	static u32 ms_VinewoodIplIndex;
};

#if RAGE_INSTANCED_TECH
	#if ENABLE_MATRIX_MEMBER
	CompileTimeAssertSize(CEntity, CENTITY_INTENDED_SIZE, 208);
	#else
	CompileTimeAssertSize(CEntity, CENTITY_INTENDED_SIZE, 136);
	#endif //ENABLE_MATRIX_MEMBER
#else
	#if ENABLE_MATRIX_MEMBER
	CompileTimeAssertSize(CEntity, CENTITY_INTENDED_SIZE, 200);
	#else
	CompileTimeAssertSize(CEntity, CENTITY_INTENDED_SIZE, 128);	
	#endif //ENABLE_MATRIX_MEMBER
#endif
CompileTimeAssertSize(CEntityFlags, 4, 4);


namespace rage {
	class fragInst;
};


/////////////////////////////////////////////////////////////////////////////
//  CEntity Inline definitions.
/////////////////////////////////////////////////////////////////////////////

inline void CEntity::SetIsRetainedByInteriorProxy(bool bVal) { SetRetainedFlag( bVal ); }
inline bool CEntity::GetIsRetainedByInteriorProxy(void) const { return IsRetainedFlagSet(); }
inline void CEntity::SetRetainingInteriorProxy(s16 interiorIdx) { SetRetainingListId( interiorIdx );}
inline s16 CEntity::GetRetainingInteriorProxy(void) const { return static_cast< s16 >( GetRetainingListId() ); }

inline void CEntity::SetOwnedBy(const eEntityOwnedBy ownedBy) { m_ownedBy = ownedBy; }
inline u32 CEntity::GetOwnedBy() const { return m_ownedBy; }

inline void CEntity::SetTintIndex(const u32 tintIndex) { Assert(tintIndex < 64); m_tintIndex = tintIndex; }
inline u32 CEntity::GetTintIndex() const { return m_tintIndex; }

inline void CEntity::SetNoWeaponDecals(bool bVal) { m_noWeaponDecals = bVal; }
inline bool CEntity::GetNoWeaponDecals() { return m_noWeaponDecals; }

inline void CEntity::SetAlwaysPreRender(bool bAlwaysPreRender) { m_nFlags.bAlwaysPreRender = bAlwaysPreRender; }
inline bool CEntity::GetAlwaysPreRender() const { return m_nFlags.bAlwaysPreRender; }

inline void CEntity::SetPreRenderedThisFrame(bool bPreRendered) { m_nFlags.bPreRenderedThisFrame = bPreRendered; }
inline bool CEntity::GetPreRenderedThisFrame() const { return m_nFlags.bPreRenderedThisFrame; }

inline void CEntity::SetUpdatingThroughAnimQueue(bool bUpdatingThroughAnimQueue) { m_nFlags.bUpdatingThroughAnimQueue = bUpdatingThroughAnimQueue; } 
inline bool CEntity::GetUpdatingThroughAnimQueue() const { return m_nFlags.bUpdatingThroughAnimQueue; }

inline void CEntity::SetUseOcclusionQuery(bool bUseOcclusionQuery) { m_nFlags.bUseOcclusionQuery = bUseOcclusionQuery; } 
inline bool CEntity::GetUseOcclusionQuery() const { return m_nFlags.bUseOcclusionQuery; }

inline void CEntity::SetTimedEntityDelayedState(const bool value) {m_nFlags.bTimedEntityDelayedState = value ? 1 : 0;}
inline bool CEntity::GetTimedEntityDelayedState() const {return m_nFlags.bTimedEntityDelayedState == 1;}

inline void CEntity::SetUseMaxDistanceForWaterReflection(const bool value) {m_nFlags.bUseMaxDistanceForWaterReflection = value ? 1 : 0;}
inline bool CEntity::GetUseMaxDistanceForWaterReflection() const {return m_nFlags.bUseMaxDistanceForWaterReflection;}

inline void CEntity::SetTypeAnimatedBuilding() { SetType(ENTITY_TYPE_ANIMATED_BUILDING); }
inline void CEntity::SetTypeBuilding() { SetType(ENTITY_TYPE_BUILDING); }
inline void CEntity::SetTypeVehicle() { SetType(ENTITY_TYPE_VEHICLE); }
inline void CEntity::SetTypePed() { SetType(ENTITY_TYPE_PED); }
inline void CEntity::SetTypeObject() { SetType(ENTITY_TYPE_OBJECT); }
inline void CEntity::SetTypeDummyObject() { SetType(ENTITY_TYPE_DUMMY_OBJECT); }
inline void CEntity::SetTypePortal() { SetType(ENTITY_TYPE_PORTAL); }
inline void CEntity::SetTypeMLO() { SetType(ENTITY_TYPE_MLO); }
inline void CEntity::SetTypePrtSys() { SetType(ENTITY_TYPE_PARTICLESYSTEM); }
inline void CEntity::SetTypeVehicleGlassComponent() { SetType(ENTITY_TYPE_VEHICLEGLASSCOMPONENT); }
inline void CEntity::SetTypeLight() { SetType(ENTITY_TYPE_LIGHT); }
inline void CEntity::SetTypeComposite() { SetType(ENTITY_TYPE_COMPOSITE); }
inline void CEntity::SetTypeInstanceList() { SetType(ENTITY_TYPE_INSTANCE_LIST); }
inline void CEntity::SetTypeGrassInstanceList() { SetType(ENTITY_TYPE_GRASS_INSTANCE_LIST); }

inline bool CEntity::GetIsTypeBuilding() const { return (GetType() == ENTITY_TYPE_BUILDING); }
inline bool CEntity::GetIsTypeAnimatedBuilding() const { return (GetType() == ENTITY_TYPE_ANIMATED_BUILDING); }
__forceinline bool CEntity::GetIsTypeVehicle() const { return (GetType() == ENTITY_TYPE_VEHICLE); }
__forceinline bool CEntity::GetIsTypePed() const { return (GetType() == ENTITY_TYPE_PED); }
inline bool CEntity::GetIsTypeObject() const { return (GetType() == ENTITY_TYPE_OBJECT); }
inline bool CEntity::GetIsTypeDummyObject() const { return (GetType() == ENTITY_TYPE_DUMMY_OBJECT); }
inline bool CEntity::GetIsTypePortal() const { return (GetType() == ENTITY_TYPE_PORTAL); }
inline bool CEntity::GetIsTypeMLO() const { return (GetType() == ENTITY_TYPE_MLO); }
inline bool CEntity::GetIsTypePrtSys() const  { return (GetType() == ENTITY_TYPE_PARTICLESYSTEM); }
inline bool CEntity::GetIsTypeLight() const  { return (GetType() == ENTITY_TYPE_LIGHT); }
inline bool CEntity::GetIsTypeComposite() const { return (GetType() == ENTITY_TYPE_COMPOSITE); }
inline bool CEntity::GetIsTypeInstanceList() const { return (GetType() == ENTITY_TYPE_INSTANCE_LIST); }
inline bool CEntity::GetIsTypeGrassInstanceList() const { return (GetType() == ENTITY_TYPE_GRASS_INSTANCE_LIST); }

__forceinline bool CEntity::GetIsPhysical() const{ return (GetType() > ENTITY_TYPE_ANIMATED_BUILDING && GetType() < ENTITY_TYPE_DUMMY_OBJECT); }
inline bool CEntity::GetIsDynamic() const{ return (GetType() > ENTITY_TYPE_BUILDING && GetType() < ENTITY_TYPE_DUMMY_OBJECT); }

inline bool CEntity::GetUseAltFadeDistance() const { return m_nFlags.bUseAltFadeDistance; }
inline void CEntity::SetUseAltFadeDistance(bool value) { m_nFlags.bUseAltFadeDistance = value; }

inline bool CEntity::IsExtremelyCloseToCamera() const { return m_nFlags.bCloseToCamera; }
inline void CEntity::SetExtremelyCloseToCamera(bool value) { m_nFlags.bCloseToCamera = value; }

inline bool CEntity::GetIsFixedFlagSet() const { return ((GetBaseFlags() & fwEntity::IS_FIXED) != 0); }
inline bool CEntity::GetIsFixedByNetworkFlagSet() const { return ((GetBaseFlags() & fwEntity::IS_FIXED_BY_NETWORK) != 0); }
inline bool CEntity::GetIsFixedUntilCollisionFlagSet() const { return IsProtectedBaseFlagSet( fwEntity::IS_FIXED_UNTIL_COLLISION); }
inline bool CEntity::GetIsAnyFixedFlagSet() const { return (((GetBaseFlags() & (fwEntity::IS_FIXED | fwEntity::IS_FIXED_BY_NETWORK)) != 0) || IsProtectedBaseFlagSet( fwEntity::IS_FIXED_UNTIL_COLLISION)); }
inline bool CEntity::GetIsAnyManagedProcFlagSet() const { return (m_nFlags.bIsProcObject || m_nFlags.bIsCompEntityProcObject); }


inline bool CEntity::GetIsVisible() const { return IsBaseFlagSet(fwEntity::IS_VISIBLE);}

inline void CEntity::SetIsSearchable(bool bIsSearchable) { AssignBaseFlag(fwEntity::IS_SEARCHABLE, bIsSearchable); UpdateWorldFromEntity(); }
inline bool CEntity::GetIsSearchable() const { return IsBaseFlagSet(fwEntity::IS_SEARCHABLE);}

inline void CEntity::SetIplIndex(s32 index) { m_iplIndex = (u32)index; }
inline s32	CEntity::GetIplIndex() const { return (s32)m_iplIndex; }

inline u8	CEntity::GetAudioBin() const { return m_audioBin; }
inline void CEntity::SetAudioBin(u8 audioBin) { m_audioBin = audioBin; }

inline bool CEntity::GetIsInInterior() const {return m_nFlags.bInMloRoom;}

inline bool CEntity::GetIsHDTxdCapable() const { FastAssert(m_pArchetype); return static_cast<CBaseModelInfo*>(m_pArchetype)->GetIsHDTxdCapable();}
inline bool CEntity::GetIsCurrentlyHD() const { return false; }

inline rmcDrawable*	CEntity::GetDrawable() const {if(m_pDrawHandler) return m_pDrawHandler->GetDrawable(); else return NULL;}
inline u32 CEntity::GetPhaseLODs() const {if(m_pDrawHandler) return m_pDrawHandler->GetPhaseLODs(); else return LODDrawable::LODFLAG_NONE_ALL;}
inline u32 CEntity::GetLastLODIdx() const {if(m_pDrawHandler) return m_pDrawHandler->GetLastLODIdx(); else return 0;}

#if !__FINAL
void AssertEntityPointerValid(CEntity *pEntity);
#else
#define AssertEntityPointerValid( A )
#endif

#if !__FINAL
void AssertEntityPointerValid_NotInWorld(CEntity *pEntity);
#else
#define AssertEntityPointerValid_NotInWorld( A )
#endif

bool IsEntityPointerValid(CEntity *pEntity);


#if !__SPU
inline fragInst* CEntity::GetFragInst() const
{ 
	return (m_nFlags.bIsFrag ? (fragInst*)(GetCurrentPhysicsInst()) : NULL); 
}
#endif
 

inline fwBasePool* GetEntityPoolFromType(int entityType)
{
	Assertf(entityType != ENTITY_TYPE_VEHICLE && entityType != ENTITY_TYPE_OBJECT, "Vehicles and Objects are stored in sysMemPoolAllocator now. Going to crash now...");

	fwBasePool *result = CEntity::ms_EntityPools[entityType];
#if !ENTITYSELECT_ENABLED_BUILD
	FastAssert(result);
#endif

	return result;
}

const char* GetEntityTypeStr(int entityType);

#endif	// INC_ENTITY_H_
