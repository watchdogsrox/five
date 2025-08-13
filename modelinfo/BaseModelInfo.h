//
//
//    Filename: BaseModelInfo.h
//     Creator: Adam Fowler
//     $Author: $
//       $Date: $
//   $Revision: $
// Description: Base class for all model info classes
//
//
#ifndef INC_BASE_MODELINFO_H_
#define INC_BASE_MODELINFO_H_

#include "entity/archetype.h"

#if !__SPU
// C headers
#include <string.h>

// Framework headers
#include "entity/archetypemanager.h"
#include "fwtl/assetstore.h"

// Rage headers
#include "fragment/drawable.h"
#endif

#include "cloth/clothmgr.h"
#include "streaming/streamingmodule.h"
#include "audio/gameobjects.h"
#include "phbound/bound.h"
#include "scene/2dEffect.h"
#include "spatialdata/aabb.h"
#include "Animation/AnimBones.h"
#include "scene/lod/LodScale.h"

#if !__SPU
// Game headers
#include "debug/Debug.h"
#include "streaming/streamingrequest.h"
#endif

#include "modelinfo/modelinfo_channel.h"

#if !defined(PPUVIRTUAL)
#if __SPU
#define PPUVIRTUAL
#else
#define PPUVIRTUAL virtual
#endif
#endif

//forward definitions
class CCustomShaderEffectBaseType;  
class gtaFragType;
class CBuoyancyInfo;
class CWaterSample;
class CClothArchetype;

namespace rage
{
	class phArchetypeDamp;
	class phGlassInst;
	class rmcDrawable;
	class fwAssetLocation;
};

// Possible model types
enum ModelInfoType {
	MI_TYPE_NONE = AF_TYPE_USER,
	MI_TYPE_BASE,			// 1
	MI_TYPE_MLO,			// 2
	MI_TYPE_TIME,			// 3
	MI_TYPE_WEAPON,			// 4
	MI_TYPE_VEHICLE,		// 5
	MI_TYPE_PED,			// 6
	MI_TYPE_COMPOSITE,		// 7

	NUM_MI_TYPES,
	MI_HIGHEST_FACTORY_ID = NUM_MI_TYPES-1
};

// Extension types
enum ExtensionType
{
	// entity extensions
	EXT_TYPE_DOOR,
	EXT_TYPE_LIGHT,
	EXT_TYPE_SPAWN_POINT_OVERRIDE,
	EXT_TYPE_CLOTH_COLLISIONS,

	// archetype extensions
	EXT_TYPE_LADDER			= 20,			// avoiding overlap with MI_TYPES for now anyway...	
	EXT_TYPE_PROCOBJ		= 21,
	EXT_TYPE_EXPLOSION		= 22,
	EXT_TYPE_PARTICLE		= 23,
	EXT_TYPE_DECAL			= 24,
	EXT_TYPE_WINDDISTURBANCE= 25,
	EXT_TYPE_LIGHTSHAFT		= 26,
	EXT_TYPE_SCROLL			= 27,
	EXT_TYPE_SPAWNPOINT		= 28,
	EXT_TYPE_BUOYANCY		= 29,
	EXT_TYPE_AUDIO			= 30,
	EXT_TYPE_WORLDPOINT		= 31,
	EXT_TYPE_EXPRESSION		= 32,

	// Unused now:
	//	EXT_TYPE_AVOIDANCE	= 33,

	EXT_TYPE_AUDIO_COLLISIONS = 34,
	


	NUM_EXT_TYPES,
	EXT_HIGHEST_FACTORY_ID = NUM_EXT_TYPES-1
};

// Class prototypes
class CTimeInfo;
class C2dEffect;
class CTunableObjectEntry;

struct CAvoidancePointAttr;

namespace rage
{
	class phBound;
	class phBoundComposite;
}

// Flags as they're used on the tool side in CBaseArchetypeDef
enum CBaseArchetypeDefLoadFlags
{
	FLAG_WET_ROAD_REFLECTION						=	(1<<0),// JWR - this doesn't seem to be used
	FLAG_DONT_FADE									=	(1<<1),// JWR - this doesn't seem to be used
	FLAG_DRAW_LAST									=	(1<<2),
	FLAG_PROP_CLIMBABLE_BY_AI						=	(1<<3),
	FLAG_SUPPRESS_HD_TXDS							=	(1<<4),
	FLAG_IS_FIXED									=	(1<<5),
	FLAG_DONT_WRITE_ZBUFFER							=	(1<<6),
	FLAG_TOUGHFORBULLETS							=	(1<<7),// JWR - this doesn't seem to be used
	FLAG_IS_GENERIC									=	(1<<8),// JWR - this doesn't seem to be used
	FLAG_HAS_ANIM									=	(1<<9),
	FLAG_HAS_UVANIM									=	(1<<10),
	FLAG_SHADOW_ONLY								=	(1<<11),
	FLAG_DAMAGE_MODEL								=	(1<<12),
	FLAG_DONT_CAST_SHADOWS							=	(1<<13),
	FLAG_CAST_TEXTURE_SHADOWS						=	(1<<14),
	FLAG_DONT_COLLIDE_WITH_FLYER					=	(1<<15),// JWR - this doesn't seem to be used
	FLAG_IS_TREE									=	(1<<16),
	FLAG_IS_TYPE_OBJECT								=	(1<<17),
	FLAG_OVERRIDE_PHYSICS_BOUNDS					=	(1<<18),
	FLAG_AUTOSTART_ANIM								=	(1<<19),
	FLAG_HAS_PRE_REFLECTED_WATER_PROXY				=	(1<<20),
	FLAG_HAS_DRAWABLE_PROXY_FOR_WATER_REFLECTIONS	=	(1<<21),
	FLAG_DOES_NOT_PROVIDE_AI_COVER					=	(1<<22),
	FLAG_DOES_NOT_PROVIDE_PLAYER_COVER				=	(1<<23),
	FLAG_IS_LADDER_DEPRECATED						=	(1<<24),
	FLAG_HAS_CLOTH									=	(1<<25),
	FLAG_DOOR_PHYSICS								=	(1<<26),
	FLAG_IS_FIXED_FOR_NAVIGATION					=	(1<<27),
	FLAG_DONT_AVOID_BY_PEDS							=	(1<<28),
	FLAG_USE_AMBIENT_SCALE							=	(1<<29),
	FLAG_IS_DEBUG									=	(1<<30),
	FLAG_HAS_ALPHA_SHADOW							=	(1<<31)
};

// base model flags
enum {
	// From archetype.h
	//MODEL_HAS_LOADED =					(1<<0),
	//MODEL_HAS_ANIMATION =					(1<<1),
	//MODEL_DRAW_LAST	=					(1<<2)
	MODEL_DONT_WRITE_ZBUFFER	=			(1<<3),
	MODEL_IS_TYPE_OBJECT =					(1<<4),				// gets created as a CObject (replaced IS_DYNAMIC flag)
	MODEL_SHADOW_PROXY =					(1<<5),			
	MODEL_LOD =								(1<<6),
	MODEL_SYNC_OBJ_IN_NET_GAME =			(1<<7),
	MODEL_HD_TEX_CAPABLE =					(1<<8),				// model uses a txd which can be upgraded to HD
	MODEL_CARRY_SCRIPTED_RT =				(1<<9),
	MODEL_HAS_ALPHA_SHADOW =				(1<<10),
	MODEL_DOES_NOT_PROVIDE_PLAYER_COVER =	(1<<11),
	MODEL_USE_AMBIENT_SCALE =				(1<<12),
	MODEL_DONT_CAST_SHADOWS =				(1<<13),
	MODEL_HAS_SPAWN_POINTS =				(1<<14),
	MODEL_DOES_NOT_PROVIDE_AI_COVER =		(1<<15),
	MODEL_IS_FIXED =						(1<<16),			// should be fixed in the world (i.e. infinite mass)
	MODEL_IS_TREE =							(1<<17),
	MODEL_CLIMBABLE_BY_AI =					(1<<18),			// object doesn't break apart easily from bullet impacts
	MODEL_USES_DOOR_PHYSICS =				(1<<19),
	MODEL_IS_FIXED_FOR_NAVIGATION =			(1<<20),			// object never moves for pathfinding purposes, even though it may fragment.  Cut navmesh around it.
	MODEL_HAS_UVANIMATION =					(1<<21),
	MODEL_NOT_AVOIDED_BY_PEDS =				(1<<22),			// never add this object into pathfinding system
#if __BANK || (__WIN32PC && !__FINAL)
	MODEL_IS_PROP =							(1<<23),
#endif
	MODEL_OVERRIDE_PHYSICS_BOUNDS =			(1<<24),
	MODEL_HAS_PRE_REFLECTED_WATER_PROXY =	(1<<25),
	// bits 26 - 31 are used by special attributes, see below
};

// special attributes. These are exclusive (only one of them can be set)
enum {
	MODEL_ATTRIBUTE_NOTHING_SPECIAL = 0,
	MODEL_ATTRIBUTE_UNUSED1,
	MODEL_ATTRIBUTE_IS_LADDER,
	MODEL_ATTRIBUTE_IS_TRAFFIC_LIGHT,
	MODEL_ATTRIBUTE_IS_BENDABLE_PLANT,
	MODEL_ATTRIBUTE_IS_GARAGE_DOOR,
	MODEL_ATTRIBUTE_MLO_WATER_LEVEL,
	MODEL_ATTRIBUTE_IS_NORMAL_DOOR,
	MODEL_ATTRIBUTE_IS_SLIDING_DOOR,
	MODEL_ATTRIBUTE_IS_BARRIER_DOOR,
	MODEL_ATTRIBUTE_IS_SLIDING_DOOR_VERTICAL,
	MODEL_ATTRIBUTE_NOISY_BUSH,
	MODEL_ATTRIBUTE_IS_RAIL_CROSSING_DOOR,
	MODEL_ATTRIBUTE_NOISY_AND_DEFORMABLE_BUSH,
	MODEL_SINGLE_AXIS_ROTATION,
	MODEL_ATTRIBUTE_HAS_DYNAMIC_COVER_BOUND, 
	MODEL_ATTRIBUTE_RUMBLE_ON_LIGHT_COLLISION_WITH_VEHICLE,
	MODEL_ATTRIBUTE_IS_RAIL_CROSSING_LIGHT,
	MODEL_ATTRIBUTE_UNUSED18,
	MODEL_ATTRIBUTE_UNUSED19,
	MODEL_ATTRIBUTE_UNUSED20,
	MODEL_ATTRIBUTE_UNUSED21,
	MODEL_ATTRIBUTE_UNUSED22,
	MODEL_ATTRIBUTE_UNUSED23,
	MODEL_ATTRIBUTE_UNUSED24,
	MODEL_ATTRIBUTE_UNUSED25,
	MODEL_ATTRIBUTE_UNUSED26,
	MODEL_ATTRIBUTE_IS_DEBUG,
	MODEL_ATTRIBUTE_RUBBISH,
	MODEL_ATTRIBUTE_RUBBISH_ONLY_ON_BIN_DAY,
	MODEL_ATTRIBUTE_CLOCK,
	MODEL_ATTRIBUTE_IS_TREE_DEPRECATED,			// this is now a flag
	MODEL_ATTRIBUTE_IS_STREET_LIGHT,
	MODEL_ATTRIBUTE_COUNT
};
CompileTimeAssert(MODEL_ATTRIBUTE_COUNT <= 64);


// To reduce the space used up by the flags we have a list of
// attributes that are exclusive (only one of these can be set)
// These values take the following bits: 26 - 31
#define ATOMIC_L_BITSHIFT				(26)
#define ATOMIC_L_MASK					(63U << ATOMIC_L_BITSHIFT)



// vehicle upgrade flags
#define MODEL_REPLACEMENT_UPGRADE	(1<<24)
#define MODEL_UPGRADE_ID_SHIFT		(25)
#define MODEL_UPGRADE_ID			(0x7f<<MODEL_UPGRADE_ID_SHIFT)



#define MI_2DEFFECT_FLAG_FX_AMBIENT		(1<<0)
#define MI_2DEFFECT_FLAG_FX_COLLISION	(1<<1)
#define MI_2DEFFECT_FLAG_FX_SHOT		(1<<2)
#define MI_2DEFFECT_FLAG_FX_BREAK		(1<<3)
#define MI_2DEFFECT_FLAG_FX_DESTROY		(1<<4)
#define MI_2DEFFECT_FLAG_FX_ANIM		(1<<5)
#define MI_2DEFFECT_FLAG_FX_INWATER		(1<<6)
#if !__FINAL
#define MI_2DEFFECT_FLAG_FX_DISABLE_ALL	(1<<7)
#endif

#define MAX_NUM_2DEFFECTS				(180)

//
// name:		ObjectNameIdAssocation
// description:	structure for linking components (by bone name) to an id
struct ObjectNameIdAssociation{
	const char* pName;
	s32 hierId;
};

extern ModelAudioCollisionSettings * g_MacsDefault;

//
// name:		CBaseModelInfo
// description:	Base class for all modelinfo classes
class CBaseModelInfo : public fwArchetype
{
	friend class CModelInfo;
	
public:
	CBaseModelInfo() :
		m_type(MI_TYPE_BASE),
		m_lodSkelBoneMap(NULL),
		m_p2dEffectPtrs(NULL),
		m_pBuoyancyInfo(NULL)
		,m_AudioCollisionsOverride(NULL)
	{
	}

	virtual ~CBaseModelInfo();

	// Init() and Shutdown() functions. All construction and destruction should be done in these functions. The
	// destructor and constructor for these classes should be empty
	virtual void Init();	
	virtual void InitArchetypeFromDefinition(strLocalIndex mapTypeDefIndex, fwArchetypeDef* definition, bool bHasAssetReferences);	
	virtual fwEntity* CreateEntity();
	virtual fwEntity* CreateEntityFromDefinition(const fwPropInstanceListDef* definition);
	virtual fwEntity* CreateEntityFromDefinition(const fwGrassInstanceListDef* definition);
	virtual bool WillGenerateBuilding()
	{
		return ( !GetIsTypeObject() && GetModelType()==MI_TYPE_BASE && GetClipDictionaryIndex()==-1 && !GetHasAnimation() );
	}
	virtual void ConvertLoadFlagsAndAttributes(u32 loadFlags, u32 attributes);
#if __BANK
	virtual void DebugPostInit() const;
#endif // __BANK

	virtual void Shutdown(); 
	virtual void ShutdownExtra() { }

	virtual bool CheckIsFixed() { return( GetIsFixed() || GetIsFixedForNavigation()); } 

	void CreateBuoyancyIfNeeded();

	// access functions
	u8 GetModelType() const {return m_type;}

	inline float GetLodDistance() const { return GetLodDistanceUnscaled() * g_LodScale.GetGlobalScale(); }

	inline bool GetIsTypeObject() const {return IsFlagSet(MODEL_IS_TYPE_OBJECT);}
	inline void SetIsTypeObject(u32 typeObj) {if(typeObj) SetFlag(MODEL_IS_TYPE_OBJECT); else ClearFlag(MODEL_IS_TYPE_OBJECT);}
	inline bool GetIsLod() const {return IsFlagSet(MODEL_LOD);}
	inline void SetIsLod(u32 lod) {if(lod) SetFlag(MODEL_LOD); else ClearFlag(MODEL_LOD);}
	inline bool GetSyncObjInNetworkGame() const {return IsFlagSet(MODEL_SYNC_OBJ_IN_NET_GAME);}
	inline void SetSyncObjInNetworkGame(u32 synch) {if(synch) SetFlag(MODEL_SYNC_OBJ_IN_NET_GAME); else ClearFlag(MODEL_SYNC_OBJ_IN_NET_GAME);}
	inline bool GetIsHDTxdCapable() const {return IsFlagSet(MODEL_HD_TEX_CAPABLE);}
	inline void SetIsHDTxdCapable(u32 hdTex) {if(hdTex) SetFlag(MODEL_HD_TEX_CAPABLE); else ClearFlag(MODEL_HD_TEX_CAPABLE);}
	inline bool GetCarryScriptedRT() const {return IsFlagSet(MODEL_CARRY_SCRIPTED_RT);}
	inline void SetCarryScriptedRT(u32 scriptedRT) {if(scriptedRT) SetFlag(MODEL_CARRY_SCRIPTED_RT); else ClearFlag(MODEL_CARRY_SCRIPTED_RT);}
	inline bool GetDontCastShadows() const {return IsFlagSet(MODEL_DONT_CAST_SHADOWS);}
	inline void SetDontCastShadows(u32 dont) {if(dont) SetFlag(MODEL_DONT_CAST_SHADOWS); else ClearFlag(MODEL_DONT_CAST_SHADOWS);}
	inline bool GetIsShadowProxy() const {return IsFlagSet(MODEL_SHADOW_PROXY);}
	inline void SetIsShadowProxy(u32 is) {if(is) SetFlag(MODEL_SHADOW_PROXY); else ClearFlag(MODEL_SHADOW_PROXY);}
	inline bool GetDontWriteZBuffer() const { return IsFlagSet(MODEL_DONT_WRITE_ZBUFFER); }
	inline void SetDontWriteZBuffer(u32 dont) { if(dont) SetFlag(MODEL_DONT_WRITE_ZBUFFER); else ClearFlag(MODEL_DONT_WRITE_ZBUFFER);}
	inline bool GetHasAlphaShadow() const {return IsFlagSet(MODEL_HAS_ALPHA_SHADOW);}
	inline void SetHasAlphaShadow(u32 has) { if(has) SetFlag(MODEL_HAS_ALPHA_SHADOW); else ClearFlag(MODEL_HAS_ALPHA_SHADOW);}

	inline bool GetDoesNotProvideAICover() const {return IsFlagSet(MODEL_DOES_NOT_PROVIDE_AI_COVER);}
	inline void SetDoesNotProvideAICover(u32 isGeneric) {if(isGeneric) SetFlag(MODEL_DOES_NOT_PROVIDE_AI_COVER); else ClearFlag(MODEL_DOES_NOT_PROVIDE_AI_COVER);}
	inline bool GetDoesNotProvidePlayerCover() const {return IsFlagSet(MODEL_DOES_NOT_PROVIDE_PLAYER_COVER);}
	inline void SetDoesNotProvidePlayerCover(u32 isGeneric) {if(isGeneric) SetFlag(MODEL_DOES_NOT_PROVIDE_PLAYER_COVER); else ClearFlag(MODEL_DOES_NOT_PROVIDE_PLAYER_COVER);}

	inline bool GetIsFixed() const {return IsFlagSet(MODEL_IS_FIXED);}
	inline void SetIsFixed(u32 isFixed) {if(isFixed) SetFlag(MODEL_IS_FIXED); else ClearFlag(MODEL_IS_FIXED);}
	inline bool GetIsTree() const {return IsFlagSet(MODEL_IS_TREE);}
	inline void SetIsTree(u32 isTree) {if(isTree) SetFlag(MODEL_IS_TREE); else ClearFlag(MODEL_IS_TREE);}
	inline bool GetUsesDoorPhysics() const { return IsFlagSet(MODEL_USES_DOOR_PHYSICS); }
	inline void SetUsesDoorPhysics(u32 doorPhysics) {if(doorPhysics) SetFlag(MODEL_USES_DOOR_PHYSICS); else ClearFlag(MODEL_USES_DOOR_PHYSICS);}
	inline bool GetIsFixedForNavigation() const { return IsFlagSet(MODEL_IS_FIXED_FOR_NAVIGATION); }
	inline void SetIsFixedForNavigation(u32 isFixedForNavigation) {if(isFixedForNavigation) SetFlag(MODEL_IS_FIXED_FOR_NAVIGATION); else ClearFlag(MODEL_IS_FIXED_FOR_NAVIGATION);}
	inline bool GetNotAvoidedByPeds() const { return IsFlagSet(MODEL_NOT_AVOIDED_BY_PEDS); }
	inline void SetNotAvoidedByPeds(u32 notAvoidedByPeds) {if(notAvoidedByPeds) SetFlag(MODEL_NOT_AVOIDED_BY_PEDS); else ClearFlag(MODEL_NOT_AVOIDED_BY_PEDS);}
	inline bool GetIsClimbableByAI() const { return IsFlagSet(MODEL_CLIMBABLE_BY_AI); }
	inline void SetIsClimbableByAI(u32 bClimbable) {if(bClimbable) SetFlag(MODEL_CLIMBABLE_BY_AI); else ClearFlag(MODEL_CLIMBABLE_BY_AI);}

	inline bool GetHasUvAnimation() const { return IsFlagSet(MODEL_HAS_UVANIMATION); }
	inline void SetHasUvAnimation(u32 isFixedForNavigation) {if(isFixedForNavigation) SetFlag(MODEL_HAS_UVANIMATION); else ClearFlag(MODEL_HAS_UVANIMATION);}
	inline bool GetHasSpawnPoints() const { return IsFlagSet(MODEL_HAS_SPAWN_POINTS); }
	inline void SetHasSpawnPoints(u32 hasSpawnPoints) {if(hasSpawnPoints) SetFlag(MODEL_HAS_SPAWN_POINTS); else ClearFlag(MODEL_HAS_SPAWN_POINTS);}
	inline bool GetUseAmbientScale() const {return IsFlagSet(MODEL_USE_AMBIENT_SCALE);	}
	inline void SetUseAmbientScale(u32 useAmb) {	if(useAmb) SetFlag(MODEL_USE_AMBIENT_SCALE); else ClearFlag(MODEL_USE_AMBIENT_SCALE);}
#if __BANK || (__WIN32PC && !__FINAL)
	inline bool GetIsProp() const { return IsFlagSet(MODEL_IS_PROP); }
	inline void SetIsProp(u32 isProp) {if(isProp) SetFlag(MODEL_IS_PROP); else ClearFlag(MODEL_IS_PROP);}
#endif
	inline bool GetOverridePhysicsBounds() const { return IsFlagSet(MODEL_OVERRIDE_PHYSICS_BOUNDS); }
	inline void SetOverridePhysicsBounds(u32 overrideBounds) {if(overrideBounds) SetFlag(MODEL_OVERRIDE_PHYSICS_BOUNDS); else ClearFlag(MODEL_OVERRIDE_PHYSICS_BOUNDS);}
	inline bool GetHasPreReflectedWaterProxy() const { return IsFlagSet(MODEL_HAS_PRE_REFLECTED_WATER_PROXY); }
	inline void SetHasPreReflectedWaterProxy(u32 hasPreReflectedWaterProxy) {if(hasPreReflectedWaterProxy) SetFlag(MODEL_HAS_PRE_REFLECTED_WATER_PROXY); else ClearFlag(MODEL_HAS_PRE_REFLECTED_WATER_PROXY);}

	inline bool GetHasPreRenderEffects() const {return (m_pShaderEffectType != NULL || m_bHasPreRenderEffects!=0);}
	inline void SetHasPreRenderEffects() { m_bHasPreRenderEffects = 1;}

	// flags
	bool TestAttribute(u32 attribute) const { return ( (GetArchetypeFlags() & ATOMIC_L_MASK) == (attribute << ATOMIC_L_BITSHIFT) ); }
	void SetAttributes(u32 attribute) { ClearFlag(ATOMIC_L_MASK); SetFlag(attribute << ATOMIC_L_BITSHIFT); }
	inline u32 GetAttributes() const;

#if !__SPU
	inline bool GetIsLadder() const { return GetAttributes() == MODEL_ATTRIBUTE_IS_LADDER; }
#endif

	inline bool GetNeverDummyFlag() const { return m_bNeverDummy; }
	inline void SetNeverDummyFlag(bool bNeverDummy) { m_bNeverDummy = bNeverDummy; }
	inline bool GetLeakObjectsFlag() const { return m_bLeakObjectsIntentionally; }
	inline void SetLeakObjectsFlag(bool bLeakObjectsIntentionally) { m_bLeakObjectsIntentionally = bLeakObjectsIntentionally; }

	inline bool GetHasDrawableProxyForWaterReflections() const { return m_bHasDrawableProxyForWaterReflections; }
	inline void SetHasDrawableProxyForWaterReflections(bool bHasProxy) { m_bHasDrawableProxyForWaterReflections=bHasProxy; }

	// entity fx flags
	inline u32 GetHasFxEntityAmbient() const { return m_fxEntityFlags & MI_2DEFFECT_FLAG_FX_AMBIENT; }
	inline void SetHasFxEntityAmbient(bool val) {if(val) m_fxEntityFlags |= MI_2DEFFECT_FLAG_FX_AMBIENT; else m_fxEntityFlags &= ~MI_2DEFFECT_FLAG_FX_AMBIENT;}

	inline u32 GetHasFxEntityCollision() const { return m_fxEntityFlags & MI_2DEFFECT_FLAG_FX_COLLISION; }
	inline void SetHasFxEntityCollision(bool val) {if(val) m_fxEntityFlags |= MI_2DEFFECT_FLAG_FX_COLLISION; else m_fxEntityFlags &= ~MI_2DEFFECT_FLAG_FX_COLLISION;}

	inline u32 GetHasFxEntityShot() const { return m_fxEntityFlags & MI_2DEFFECT_FLAG_FX_SHOT; }
	inline void SetHasFxEntityShot(bool val) {if(val) m_fxEntityFlags |= MI_2DEFFECT_FLAG_FX_SHOT; else m_fxEntityFlags &= ~MI_2DEFFECT_FLAG_FX_SHOT;}

	inline u32 GetHasFxEntityBreak() const { return m_fxEntityFlags & MI_2DEFFECT_FLAG_FX_BREAK; }
	inline void SetHasFxEntityBreak(bool val) {if(val) m_fxEntityFlags |= MI_2DEFFECT_FLAG_FX_BREAK; else m_fxEntityFlags &= ~MI_2DEFFECT_FLAG_FX_BREAK;}

	inline u32 GetHasFxEntityDestroy() const { return m_fxEntityFlags & MI_2DEFFECT_FLAG_FX_DESTROY; }
	inline void SetHasFxEntityDestroy(bool val) {if(val) m_fxEntityFlags |= MI_2DEFFECT_FLAG_FX_DESTROY; else m_fxEntityFlags &= ~MI_2DEFFECT_FLAG_FX_DESTROY;}

	inline u32 GetHasFxEntityAnim() const { return m_fxEntityFlags & MI_2DEFFECT_FLAG_FX_ANIM; }
	inline void SetHasFxEntityAnim(bool val) {if(val) m_fxEntityFlags |= MI_2DEFFECT_FLAG_FX_ANIM; else m_fxEntityFlags &= ~MI_2DEFFECT_FLAG_FX_ANIM;}

	inline u32 GetHasFxEntityInWater() const { return m_fxEntityFlags & MI_2DEFFECT_FLAG_FX_INWATER; }
	inline void SetHasFxEntityInWater(bool val) {if(val) m_fxEntityFlags |= MI_2DEFFECT_FLAG_FX_INWATER; else m_fxEntityFlags &= ~MI_2DEFFECT_FLAG_FX_INWATER;}

#if !__FINAL
	inline u32 GetHasFxEntityDisableAll() const { return m_fxEntityFlags & MI_2DEFFECT_FLAG_FX_DISABLE_ALL; }
	inline void SetHasFxEntityDisableAll(bool val) {if(val) m_fxEntityFlags |= MI_2DEFFECT_FLAG_FX_DISABLE_ALL; else m_fxEntityFlags &= ~MI_2DEFFECT_FLAG_FX_DISABLE_ALL;}
#endif

	inline CBuoyancyInfo* GetBuoyancyInfo() const { return m_pBuoyancyInfo;}

	virtual void SetPhysics(phArchetypeDamp* pPhysicsArchetype);
	void CopyMaterialFlagsToBound(phBound* pBound);
	void CopyMaterialFlagsToSingleBound(phBound* pBound);

	void SetupDynamicCoverCollisionBounds(phArchetypeDamp* pPhysicsArchetype);
	void ClearDynamicCoverCollisionBounds(phArchetypeDamp* pPhysicsArchetype);

	void ComputeMass(phArchetypeDamp* pPhysicsArchetype, const CTunableObjectEntry* pTuning) const;
	bool ComputeDamping(phArchetypeDamp* pPhysicsArchetype, const CTunableObjectEntry* pTuning) const;

	// 2d effect access
	s32 GetNum2dEffects() const;
	bool Has2dEffects() const;
	C2dEffect* Get2dEffect(s32 i);
	C2dEffect* Get2dEffectNoLights(s32 i);
	void Add2dEffect(C2dEffect** ppEffect);
	void Init2dEffects();
	s32 GetNum2dEffectType(e2dEffectType type) const;
	s32 GetNum2dEffectLights() const;
	void Get2dEffects(atArray<C2dEffect*>& effects) const;
	const atArray<C2dEffect*>* Get2dEffectsNoLights();
	const atArray<CLightAttr>* GetLightArray();

	// Helper macros for getting the 2d effects array
#define GET_2D_EFFECTS(pBaseModelInfo)\
	C2dEffect *arrayContents[MAX_NUM_2DEFFECTS]; \
	atUserArray<C2dEffect*> a2dEffects(arrayContents,MAX_NUM_2DEFFECTS); \
	pBaseModelInfo->Get2dEffects(a2dEffects); \
	int numEffects = a2dEffects.GetCount(); 
#define GET_2D_EFFECTS_NOLIGHTS(pBaseModelInfo)\
	const atArray<C2dEffect*>* pa2dEffects = pBaseModelInfo->Get2dEffectsNoLights();\
	int iNumEffects = pa2dEffects ? pa2dEffects->GetCount() : 0;

	s32 GetNum2dEffectsNoLights() const;
	void SetNum2dEffects(u32 numEffects);
	C2dEffect** GetNewEffect();

	//Audio collision properties
	const ModelAudioCollisionSettings* GetAudioCollisionSettings();
	void SetAudioCollisionSettings(const ModelAudioCollisionSettings * settings, u32 hash);

	virtual const u16* GetLodSkeletonMap() const { return m_lodSkelBoneMap; }
	virtual u16 GetLodSkeletonBoneNum() const { return m_lodSkelBoneNum; }
	void InitLodSkeletonMap(const crSkeletonData* skelData, const crSkeletonData* lodSkelData, const CBaseModelInfo* slodMI);
	void InitLodSkeletonMap(const crSkeletonData* skelData, const atArray<int>& skinnedBones);
	void ShutdownLodSkeletonMap();
    s32 GetLodSkeletonBoneIndex(s32 boneIndex);
	
	bool GetGeneratesWindAudio() const;

	// ---

	//  drawable/fragtype funcs (mainly to add the shader effect & setup a ptr to the result)
	virtual void InitMasterDrawableData(u32 modelIdx);
	virtual void DeleteMasterDrawableData();

	virtual void InitMasterFragData(u32 modelIdx);
	virtual void DeleteMasterFragData();

	gtaFragType* GetFragType() const		{ return (gtaFragType *) fwArchetype::GetFragType(); }

	void SetDrawableDependenciesAsDirty(bool markTexDict = false);
	void BumpDrawableRefCount();
	void ReleaseDrawable();
	int GetDrawableRefCount();
	
	// ---

	// time stuff
	virtual CTimeInfo* GetTimeInfo() {return NULL;}

	virtual s32 GetPropsFileIndex()		 const {return -1;}

	void SetPtFxAssetSlot(const s32 ptfxAssetSlot) {m_ptfxAssetSlot = (s16)ptfxAssetSlot;}
	inline s32 GetPtFxAssetSlot() const {return m_ptfxAssetSlot;}		

	// Scriptid ( used for named rendertarget)
	void SetScriptId(unsigned int scriptId) { m_scriptId = scriptId; }
	u32 GetScriptId() { return m_scriptId; }

	// ---
	void CheckForHDTxdSlots(void);		// check txd chain for this model & set HDTxd flag if any are found
	fwAssetLocation GetAssetLocation(void) const;

	// Expression sets. Get only, as it's currently only supported for certain types
	virtual fwMvExpressionSetId GetExpressionSet() const {return EXPRESSION_SET_ID_INVALID;}
public:

	static phBound* GetLargestBoundFromComposite(phBoundComposite* pCompositeBound);

	inline CCustomShaderEffectBaseType*	GetCustomShaderEffectType() const { return m_pShaderEffectType; }

	bool CreatePhysicsArchetype();
	void DeletePhysicsArchetype();
	void AddRefToDrawablePhysics();
	void RemoveRefFromDrawablePhysics();

#if __BANK
	void	ReloadWaterSamples();
#endif // __BANK

protected:

	bool SetupCustomShaderEffects();
	void DestroyCustomShaderEffects();	

	virtual void InitWaterSamples();
	void	DeleteWaterSamples();

#if NORTH_CLOTHS
	// This is for bendy plant stuff
	// This is NOT rage environment cloth
	void	InitClothArchetype();
	void	DeleteClothArchetype();
#endif //NORTH_CLOTHS

	// Recursive function to generate water samples for an object
	static int GenerateWaterSamplesFromBoundR(CWaterSample* pWaterSamples,int nNumSamplesInArray,phBound* pBound, int iCurrentComponent);

	CCustomShaderEffectBaseType* m_pShaderEffectType;			// +80

	CBuoyancyInfo* m_pBuoyancyInfo;								// +84

	const ModelAudioCollisionSettings * m_AudioCollisionsOverride;	// +88

	u32 m_scriptId;												// +92

	u16* m_lodSkelBoneMap;										// +96
	u16 m_lodSkelBoneNum;										// +100

	s16 m_ptfxAssetSlot;										// +102

	u8 m_fxEntityFlags;											// +104
	u8 m_type : 5;												// +105		
	u8 m_bHasPreRenderEffects : 1;
	u8 m_bHasDrawableProxyForWaterReflections : 1;
	u8 m_tbdGeneralFlag : 1;

	// these are really specific to CObjects so could be separated out into an extension in the future
	u32 m_bLeakObjectsIntentionally : 1;
	u32 m_bNeverDummy : 1;
	u32 m_tbdObjectFlags : 30;
	//////////////////////////////////////////////////////////////////////////

	atArray<C2dEffect*>*		m_p2dEffectPtrs;

	static int g_wdcascade_TechniqueGroupId;
	static int g_wdcascade_shadowtexture_TechniqueGroupId;

#if __DEV
	static bool ms_bPrintWaterSampleEvents;
	static u32 ms_nNumWaterSamplesInMemory;
#endif
};

#if	!__SPU

inline u32 CBaseModelInfo::GetAttributes() const
{
	return ( (GetArchetypeFlags() & ATOMIC_L_MASK) >> ATOMIC_L_BITSHIFT);
}



//#include "system/findsize.h"
//FindSize(CBaseModelInfo); // 112
//#include "system/findoffset.h"
//FindOffset(CBaseModelInfo, m_pShaderEffectType);		// 80
//FindOffset(CBaseModelInfo, m_pBuoyancyInfo);			// 84
//FindOffset(CBaseModelInfo, m_audioCollisionSettings);	// 88
//FindOffset(CBaseModelInfo, m_scriptId);					// 92
//FindOffset(CBaseModelInfo, m_lodSkelBoneMap);			// 96
//FindOffset(CBaseModelInfo, m_lodSkelBoneNum);			// 100
//FindOffset(CBaseModelInfo, m_nAvoidancePoint2dEffect);	// 102
//FindOffset(CBaseModelInfo, m_ptfxAssetSlot);			// 104
//FindOffset(CBaseModelInfo, m_fxEntityFlags);			// 106
//FindOffset(CBaseModelInfo, m_type);						// 107
//FindOffset(CBaseModelInfo, m_p2dEffectPtrs);			// 108
CompileTimeAssertSize( CBaseModelInfo, 112, 176 );

#endif //!__SPU...

#endif // INC_BASE_MODELINFO_H_



