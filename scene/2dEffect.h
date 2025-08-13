//
// file: 2dEffect.h
// description: Class holding information about 2d effects like coronas and sprites
// written by: Adam Fowler
// 
#ifndef INC_2DEFFECT_H_
#define INC_2DEFFECT_H_

#include "entity/extensionlist.h"

#if !__SPU
	// Rage headers
	#include "fwtl/pool.h"
	#include "fwtl/poolcriticalsection.h"
	#include "streaming/streamingmodule.h"

	// Game headers
	#include "debug/debug.h"
	#include "vector/vector3.h"
#endif

class CEntity;
class CLightAttrDef;

namespace rage {
	class Color32;
	class fwArchetype;
	class fwExtensionDef;
	struct ModelAudioCollisionSettings;
}

//
// 2dEffect type
//
enum e2dEffectType
{
	ET_LIGHT					= 0,
	ET_PARTICLE					= 1,
	ET_EXPLOSION				= 2,
	ET_DECAL					= 3,
	ET_WINDDISTURBANCE			= 4,
	//	ET_PEDQUEUE				= 3,
	//	ET_SUNGLARE				= 4,
	//	ET_INTERIOR				= 5,
	//	ET_ENTRYEXIT			= 6,
	//	ET_ROADSIGN				= 7,
	//	ET_TRIGGERPOINT			= 8,	//	These were used to find the correct coords for the fruit machine script to create the barrels. (GET_LEVEL_DESIGN_COORDS_FOR_OBJECT)
	//	ET_COVERPOINT			= 9,
	// ET_ESCALATOR				= 10,
	//	ET_UNUSED				= 11,
	ET_PROCEDURALOBJECTS		= 12,
	ET_COORDSCRIPT				= 13,
	ET_LADDER					= 14,
	ET_AUDIOEFFECT				= 15,
	//	ET_AUDIOFRAGMATERIAL	= 16,
	ET_SPAWN_POINT				= 17,
	ET_LIGHT_SHAFT				= 18,
	ET_SCROLLBAR				= 19,
	//	ET_WINDOW_LEDGE			= 20,
	// ET_SWAYABLE					= 21,
	ET_BUOYANCY					= 22,
	ET_EXPRESSION				= 23,
	ET_AUDIOCOLLISION			= 24,

	ET_MAX_2D_EFFECTS,

	ET_NULLEFFECT				= 255
};

class CLightEntity;
class CLightAttr;

struct CLightShaftAttr;
struct CParticleAttr;
struct CExplosionAttr;
struct CDecalAttr;
struct CWindDisturbanceAttr;
struct CProcObjAttr;
struct CWorldPointAttr;
struct CLadderInfo;
struct CAudioCollisionInfo;
struct CAudioAttr;
struct CSpawnPoint;
struct CScrollBarAttr;
struct CSwayableAttr;
struct CBuoyancyAttr;
struct CExpressionExtension;

struct VectorData
{
#if !__FINAL
	void DeclareStruct(datTypeStruct &s);
#endif // !__FINAL

	float x,y,z;
	inline void Get(Vector3 & vec) { vec.x = x; vec.y = y; vec.z = z; }
	inline Vector3 GetVector3() const { return Vector3(x, y, z); }
	inline Vector3 GetVector3() { return Vector3(x, y, z); }
	inline void Set(const Vector3 &vec) { x = vec.x; y = vec.y; z = vec.z; }
protected:
};

//
// 2d effect class
//
class C2dEffect : public fwExtension
{
public:
	virtual ~C2dEffect() {}

	virtual void InitArchetypeExtensionFromDefinition(const fwExtensionDef* definition, fwArchetype* archetype);
	virtual void CopyToDefinition(fwExtensionDef* definition);

#if !__FINAL
	void DeclareStruct(datTypeStruct &s);
#endif // !__FINAL

	Vector3 GetWorldPosition(const CEntity* pOwner) const;

#if __BANK
	void FindDebugColourAndText(Color32 *col, char *string, bool bSecondText, CEntity* pAttachedEntity );
#endif // __BANK

#if __DEV
	void Count2dEffects();

	static bool m_PrintStats;
	static void Count2dEffects(s32 type, s32 &num, s32 &numWorld);
	static void PrintDebugStats();
#endif

	VectorData& GetPosDirect() { return m_posn; }
	void GetPos(Vector3& r_pos) const { r_pos.x = m_posn.x; r_pos.y = m_posn.y; r_pos.z = m_posn.z;}
	void SetPos(const Vector3& r_pos) { m_posn.x = r_pos.x; m_posn.y = r_pos.y; m_posn.z = r_pos.z;}

	virtual u8 GetType() const = 0;

	virtual CLightAttr* GetLight() { Assert(false); return NULL; }
	virtual CLightShaftAttr* GetLightShaft() { Assert(false); return NULL; }
	virtual CParticleAttr* GetParticle() { Assert(false); return NULL; }
	virtual CExplosionAttr* GetExplosion() { Assert(false); return NULL; }
	virtual CDecalAttr* GetDecal() { Assert(false); return NULL; }
	virtual CWindDisturbanceAttr* GetWindDisturbance() { Assert(false); return NULL; }
	virtual CProcObjAttr* GetProcObj() { Assert(false); return NULL; }
	virtual CWorldPointAttr* GetWorldPoint() { Assert(false); return NULL; }
	virtual CLadderInfo* GetLadder() { Assert(false); return NULL; }
	virtual CAudioCollisionInfo* GetAudioCollisionInfo() { Assert(false); return NULL; }
	virtual CAudioAttr* GetAudio() { Assert(false); return NULL; }
	virtual CSpawnPoint* GetSpawnPoint() { Assert(false); return NULL; }
	virtual CScrollBarAttr* GetScrollBar() { Assert(false); return NULL; }
	virtual CSwayableAttr* GetSwayable() { Assert(false); return NULL; }
	virtual CBuoyancyAttr* GetBuoyancy() { Assert(false); return NULL; }
	virtual CExpressionExtension* GetExpression() { Assert(false); return NULL; }

	virtual const CLightAttr* GetLight() const { Assert(false); return NULL; }
	virtual const CLightShaftAttr* GetLightShaft() const { Assert(false); return NULL; }
	virtual const CParticleAttr* GetParticle() const { Assert(false); return NULL; }
	virtual const CExplosionAttr* GetExplosion() const { Assert(false); return NULL; }
	virtual const CDecalAttr* GetDecal() const { Assert(false); return NULL; }
	virtual const CWindDisturbanceAttr* GetWindDisturbance() const { Assert(false); return NULL; }
	virtual const CProcObjAttr* GetProcObj() const { Assert(false); return NULL; }
	virtual const CWorldPointAttr* GetWorldPoint() const { Assert(false); return NULL; }
	virtual const CLadderInfo* GetLadder() const { Assert(false); return NULL; }
	virtual const CAudioCollisionInfo* GetAudioCollisionInfo() const { Assert(false); return NULL; }
	virtual const CAudioAttr* GetAudio() const { Assert(false); return NULL; }
	virtual const CSpawnPoint* GetSpawnPoint() const { Assert(false); return NULL; }
	virtual const CScrollBarAttr* GetScrollBar() const { Assert(false); return NULL; }
	virtual const CSwayableAttr* GetSwayable() const { Assert(false); return NULL; }
	virtual const CBuoyancyAttr* GetBuoyancy() const { Assert(false); return NULL; }
	virtual const CExpressionExtension* GetExpression() const { Assert(false); return NULL; }

protected:
	VectorData 	m_posn;
	
	PAD_FOR_GCC_X64_4;
};

enum eParticleType {
	PT_AMBIENT_FX,
	PT_COLLISION_FX,
	PT_SHOT_FX,
	PT_BREAK_FX,
	PT_DESTROY_FX,
	PT_ANIM_FX,
	PT_RAYFIRE_FX,
	PT_INWATER_FX,
};

enum eExplosionType {
	XT_SHOT_PT_FX,				// at shot point
	XT_BREAK_FX,
	XT_DESTROY_FX,
	XT_SHOT_GEN_FX,				// at generic point
};

enum eDecalType {
	DT_COLLISION_FX,
	DT_SHOT_FX,
	DT_BREAK_FX,
	DT_DESTROY_FX
};

enum eWindDisturbanceType {
	WT_HEMISPHERE
};

enum ePedQueueType {
	QT_SEAT=0,
	QT_BUS_STOP,
	//	the following don't appear in Max yet - they will be added back in as and when they are needed
	QT_ATM,
	QT_PIZZA,			// shop queue
	QT_SHELTER,
	QT_LOOK_AT,
	QT_PARK,
	QT_STEP,
	//	the following should always be at the end of the list. QT_TRIGGER_SCRIPT will never appear in Max
	//	Script Queues can only be created by the script command ADD_PED_QUEUE
	QT_TRIGGER_SCRIPT,
	MAX_NUM_QUEUE_TYPES

	//	QT_ICE_CREAM_VAN,
	//	QT_INTERIOR,
};


//
// name:		Structures that can be included in unions
//
struct ColourData
{
#if !__FINAL
	void DeclareStruct(datTypeStruct &s);
#endif // !__FINAL

	u8 red, green, blue;

	inline Color32 GetColor32() const { return Color32(red, green, blue, 255); }
	inline Vector3 GetVector3() const { return Vector3((float)red/255.0f, (float)green/255.0f, (float)blue/255.0f); }

	inline void Set(Color32 col) 
	{ 
		red = col.GetRed(); 
		green = col.GetGreen();
		blue = col.GetBlue();
	}

	inline void Set(const Vector3 &col) 
	{ 
		red   = Max<u8>(0, Min<u8>((u8)(col.GetX() * 255.0f + 0.5), 255));
		green = Max<u8>(0, Min<u8>((u8)(col.GetY() * 255.0f + 0.5), 255));
		blue  = Max<u8>(0, Min<u8>((u8)(col.GetZ() * 255.0f + 0.5), 255));
	}
};

#define CORONA_DEFAULT_ZBIAS (0.5f)

//
// Attributes of a light
//
class CLightAttr : public C2dEffect
{
public:
#if !__SPU
	EXTENSIONID_DECL(CLightAttr, fwExtension);
#endif //__SPU

	CLightAttr();
	CLightAttr(datResource& rsc);

	IMPLEMENT_PLACE_INLINE(CLightAttr);

#if !__FINAL
	void Reset(const Vector3& pos, const Vector3& dir, float falloff, float coneOuter);
	void DeclareStruct(datTypeStruct &s);
#endif // !__FINAL

	u8 GetType() const { return ET_LIGHT; }
	CLightAttr* GetLight() { return this; }
	const CLightAttr* GetLight() const { return this; }

	void Set(const CLightAttrDef* attr);

	void Set(const CLightAttr *attr)
	{
		// C2dEffect data
		m_posn						= attr->m_posn;

		// General data
		m_colour					= attr->m_colour;
		m_flashiness				= attr->m_flashiness;
		m_intensity					= attr->m_intensity;
		m_flags						= attr->m_flags;
		m_boneTag					= attr->m_boneTag;
		m_lightType					= attr->m_lightType;
		m_groupId					= attr->m_groupId;
		m_timeFlags					= attr->m_timeFlags;
		m_falloff					= attr->m_falloff;
		m_falloffExponent			= attr->m_falloffExponent;
		m_cullingPlane[0]			= attr->m_cullingPlane[0];
		m_cullingPlane[1]			= attr->m_cullingPlane[1];
		m_cullingPlane[2]			= attr->m_cullingPlane[2];
		m_cullingPlane[3]			= attr->m_cullingPlane[3];
		m_shadowNearClip			= attr->m_shadowNearClip;
		m_shadowBlur				= attr->m_shadowBlur;

		// Volume data
		m_volIntensity				= attr->m_volIntensity;
		m_volSizeScale				= attr->m_volSizeScale;
		m_volOuterColour			= attr->m_volOuterColour;
		m_lightHash					= attr->m_lightHash;
		m_volOuterIntensity			= attr->m_volOuterIntensity;

		// Corona data
		m_coronaSize				= attr->m_coronaSize;
		m_volOuterExponent			= attr->m_volOuterExponent;
		m_lightFadeDistance			= attr->m_lightFadeDistance;
		m_shadowFadeDistance		= attr->m_shadowFadeDistance;
		m_specularFadeDistance		= attr->m_specularFadeDistance;
		m_volumetricFadeDistance	= attr->m_volumetricFadeDistance;
		m_coronaIntensity			= attr->m_coronaIntensity;
		m_coronaZBias				= attr->m_coronaZBias;

		// Spot data
		m_direction					= attr->m_direction;
		m_tangent					= attr->m_tangent;
		m_coneInnerAngle			= attr->m_coneInnerAngle;
		m_coneOuterAngle			= attr->m_coneOuterAngle;

		// Line data
		m_extents					= attr->m_extents;

		// Texture data
		m_projectedTextureKey		= attr->m_projectedTextureKey;

	#if RSG_BANK
		m_padding3					= attr->m_padding3;			// hack: extra flags
	#endif
	}

	// ==================================================================================
	// WARNING!! IF YOU CHANGE THE FIELDS HERE YOU MUST ALSO KEEP IN SYNC WITH:
	// 
	// CLightAttrDef - x:\gta5\src\dev\game\scene\loader\MapData_Extensions.psc
	// CLightAttr    - x:\gta5\src\dev\game\scene\2dEffect.h
	// CLightAttr    - x:\gta5\src\dev\rage\framework\tools\src\cli\ragebuilder\gta_res.h
	// WriteLight    - x:\gta5\src\dev\game\debug\AssetAnalysis\AssetAnalysis.cpp
	// WriteLights   - X:\gta5\src\dev\tools\AssetAnalysisTool\src\AssetAnalysisTool.cpp
	// aaLightRecord - X:\gta5\src\dev\tools\AssetAnalysisTool\src\AssetAnalysisTool.h
	// ==================================================================================

	// General data
	ColourData	m_colour;
	u8			m_flashiness;
	float		m_intensity;
	u32			m_flags;
	s16			m_boneTag;
	u8			m_lightType;
	u8			m_groupId;
	u32			m_timeFlags;
	float		m_falloff;
	float		m_falloffExponent;
	float		m_cullingPlane[4];
	u8			m_shadowBlur;
 	u8			m_padding1;		// RSG_PC hack: stores EXTRA_LIGHTFLAG_HIGH_PRIORITY
 	s16			m_padding2;
	u32			m_padding3;		// hack: stores EXTRA_LIGHTFLAG_USE_AS_PED_ONLY (B*6817429 - PED only light option in flags for CS lighting)

	// Volume data
	float		m_volIntensity;
	float		m_volSizeScale;
	ColourData	m_volOuterColour;
	u8			m_lightHash;
	float		m_volOuterIntensity;

	// Corona data
	float		m_coronaSize;
	float		m_volOuterExponent;
	u8			m_lightFadeDistance;
	u8			m_shadowFadeDistance;
	u8			m_specularFadeDistance;
	u8			m_volumetricFadeDistance;
	float		m_shadowNearClip;
	float		m_coronaIntensity;
	float		m_coronaZBias;	

	// Spot data
	VectorData	m_direction;
	VectorData	m_tangent;
	float		m_coneInnerAngle;
	float		m_coneOuterAngle;

	// Line data
	VectorData	m_extents;

	// Texture data
	u32			m_projectedTextureKey;	
};

//
// Attributes of an explosion
//
struct CExplosionAttr : public C2dEffect
{
#if !__SPU
	EXTENSIONID_DECL(CExplosionAttr, fwExtension);
#endif //__SPU

	u8 GetType() const { return ET_EXPLOSION; }
	CExplosionAttr* GetExplosion() { return this; }
	const CExplosionAttr* GetExplosion() const { return this; }

	virtual void InitArchetypeExtensionFromDefinition(const fwExtensionDef* definition, fwArchetype* archetype);

	float	qX, qY, qZ, qW;
#if __SPU
	u32		m_tagHashName;
#else
	atHashWithStringNotFinal m_tagHashName;
#endif
	s32		m_explosionType;
	s32		m_boneTag;

	bool	m_ignoreDamageModel;
	bool	m_playOnParent;
	bool	m_onlyOnDamageModel;
	bool	m_allowRubberBulletShotFx;
	bool	m_allowElectricBulletShotFx;
};

//
// Attributes of a particle
//
struct CParticleAttr : public C2dEffect
{
#if !__SPU
	EXTENSIONID_DECL(CParticleAttr, fwExtension);
#endif //__SPU

	u8 GetType() const { return ET_PARTICLE; }
	CParticleAttr* GetParticle() { return this; }
	const CParticleAttr* GetParticle() const { return this; }

	virtual void InitArchetypeExtensionFromDefinition(const fwExtensionDef* definition, fwArchetype* archetype);

	float	qX, qY, qZ, qW;
#if __SPU
	u32		m_tagHashName;
#else
	atHashWithStringNotFinal m_tagHashName;
#endif
	s32		m_fxType;			// collision effect, ambient effect
	s32		m_boneTag;
	float	m_scale;
	s32		m_probability;
	bool	m_hasTint;
	u8		m_tintR;
	u8		m_tintG;
	u8		m_tintB;
	u8		m_tintA;

	bool	m_ignoreDamageModel;
	bool	m_playOnParent;
	bool	m_onlyOnDamageModel;
	bool	m_allowRubberBulletShotFx;
	bool	m_allowElectricBulletShotFx;
};

//
// Attributes of a decal
//
struct CDecalAttr : public C2dEffect
{
#if !__SPU
	EXTENSIONID_DECL(CDecalAttr, fwExtension);
#endif //__SPU

	u8 GetType() const { return ET_DECAL; }
	CDecalAttr* GetDecal() { return this; }
	const CDecalAttr* GetDecal() const { return this; }

	virtual void InitArchetypeExtensionFromDefinition(const fwExtensionDef* definition, fwArchetype* archetype);

	float	qX, qY, qZ, qW;
#if __SPU
	u32		m_tagHashName;
#else
	atHashWithStringNotFinal m_tagHashName;
#endif
	s32		m_decalType;			// collision decal, shot decal
	s32		m_boneTag;
	float	m_scale;
	s32		m_probability;

	bool	m_ignoreDamageModel;
	bool	m_playOnParent;
	bool	m_onlyOnDamageModel;
	bool	m_allowRubberBulletShotFx;
	bool	m_allowElectricBulletShotFx;
};

//
// Attributes of a wind disturbance
//
struct CWindDisturbanceAttr : public C2dEffect
{
#if !__SPU
	EXTENSIONID_DECL(CWindDisturbanceAttr, fwExtension);
#endif //__SPU

	u8 GetType() const { return ET_WINDDISTURBANCE; }
	CWindDisturbanceAttr* GetWindDisturbance() { return this; }
	const CWindDisturbanceAttr* GetWindDisturbance() const { return this; }

	virtual void InitArchetypeExtensionFromDefinition(const fwExtensionDef* definition, fwArchetype* archetype);

	float	qX, qY, qZ, qW;
	s32		m_disturbanceType;			// hemisphere... 
	s32		m_boneTag;
	float	m_sizeX, m_sizeY, m_sizeZ, m_sizeW;
	float	m_strength;

	bool	m_strengthMultipliesGlobalWind;
};


//
// Attributes of a buoyant model
//
struct CBuoyancyAttr : public C2dEffect
{
#if !__SPU
	EXTENSIONID_DECL(CBuoyancyAttr, fwExtension);
#endif //__SPU

	virtual void InitArchetypeExtensionFromDefinition(const fwExtensionDef* definition, fwArchetype* archetype);

	u8 GetType() const { return ET_BUOYANCY; }
	CBuoyancyAttr* GetBuoyancy() { return this; }
	const CBuoyancyAttr* GetBuoyancy() const { return this; }

};


//
// procedural object attribute
//
struct CProcObjAttr	: public C2dEffect
{
#if !__SPU
	EXTENSIONID_DECL(CProcObjAttr, fwExtension);
#endif //__SPU

	u8 GetType() const { return ET_PROCEDURALOBJECTS; }	
	CProcObjAttr* GetProcObj() { return this; }
	const CProcObjAttr* GetProcObj() const { return this; }

	virtual void InitArchetypeExtensionFromDefinition(const fwExtensionDef* definition, fwArchetype* archetype);

	float radiusInner;
	float radiusOuter;
	u32 objHash;
	float spacing;
	float minScale;
	float maxScale;
	float minScaleZ;
	float maxScaleZ;
	float zOffsetMin;
	float zOffsetMax;
	bool isAligned;
};

#define MAX_NUMBER_OF_EXTRA_WORLD_POINT_COORDS	(4)

#if !__SPU
struct CWorldPointAttr : public C2dEffect
{
	EXTENSIONID_DECL(CWorldPointAttr, fwExtension);

	u8 GetType() const { return ET_COORDSCRIPT; }
	CWorldPointAttr* GetWorldPoint() { return this; }
	const CWorldPointAttr* GetWorldPoint() const { return this; }

	void Reset(bool bResetScriptBrainToLoad);

	strStreamingObjectName m_scriptName;	// name of script to trigger when the player approaches the coords
	VectorData extraCoords[MAX_NUMBER_OF_EXTRA_WORLD_POINT_COORDS];
	float	   extraCoordOrientations[MAX_NUMBER_OF_EXTRA_WORLD_POINT_COORDS+1];
	s8	   num_of_extra_coords;
	s16	   BrainStatus;
	s16	   ScriptBrainToLoad;
	u32	   LastUpdatedInFrame;
};
#endif

struct CLadderInfo : public C2dEffect
{
#if !__SPU
	EXTENSIONID_DECL(CLadderInfo, fwExtension);
#endif //__SPU

	u8 GetType() const { return ET_LADDER; }
	CLadderInfo* GetLadder() { return this; }
	const CLadderInfo* GetLadder() const { return this; }

	virtual void InitArchetypeExtensionFromDefinition(const fwExtensionDef* definition, fwArchetype* archetype);

	VectorData vBottom;			// The bottom of the ladder
	VectorData vTop;			// The top of the ladder (the x,y will be the same as vBottom)
	VectorData vNormal;			// A vector heading away from the climbable surface of the ladder
	atHashString ladderdef;		
	bool bCanGetOffAtTop;		// Whether it leads anywhere

	// NB : in future we could have another value for the climbing type - (ladder climbing, rock climbing, etc)
};

struct CAudioCollisionInfo : public C2dEffect
{
#if !__SPU
	EXTENSIONID_DECL(CAudioCollisionInfo, fwExtension);
#endif //__SPU

	u8 GetType() const { return ET_AUDIOCOLLISION; }
	CAudioCollisionInfo* GetAudioCollisionInfo() { return this; }
	const CAudioCollisionInfo* GetAudioCollisionInfo() const { return this; }

	virtual void InitArchetypeExtensionFromDefinition(const fwExtensionDef* definition, fwArchetype* archetype);

	atHashString settingsHash;
	ModelAudioCollisionSettings * settings;
};

struct CAudioAttr : public C2dEffect
{
#if !__SPU
	EXTENSIONID_DECL(CAudioAttr, fwExtension);
#endif //__SPU

	virtual void InitArchetypeExtensionFromDefinition(const fwExtensionDef* definition, fwArchetype* archetype);

	u8 GetType() const { return ET_AUDIOEFFECT; }
	CAudioAttr* GetAudio() { return this; }
	const CAudioAttr* GetAudio() const { return this; }

	float qx;
	float qy;
	float qz;
	float qw;
	s32		effectindex;	// Index of the effect.
};

struct CSpawnPoint : public C2dEffect
{
	typedef enum
	{
		IS_Unknown = 0,
		IS_Outside,
		IS_Inside,
	} InteriorState;

	enum AvailabilityMpSp
	{
		kBoth,		// Scenario exists both in single player and multiplayer.
		kOnlySp,	// Only in single player.
		kOnlyMp		// Only in multiplayer.
	};

	// PURPOSE:	Special values for iTimeTillPedLeaves.
	enum
	{
		kTimeTillPedLeavesNone = 0,			// No override.
		kTimeTillPedLeavesMax = 254,		// Max possible finite time.
		kTimeTillPedLeavesInfinite = 255	// Stay a while... stay forever.
	};

#if !__SPU
	EXTENSIONID_DECL(CSpawnPoint, fwExtension);
#endif //__SPU

	virtual void InitArchetypeExtensionFromDefinition(const fwExtensionDef* definition, fwArchetype* archetype);
	virtual void CopyToDefinition(fwExtensionDef * definition);
	void Reset();

	u8 GetType() const { return ET_SPAWN_POINT; }
	Vector3 GetSpawnPointDirection(CEntity* pOwner) const;
	CSpawnPoint* GetSpawnPoint() { return this; }
	const CSpawnPoint* GetSpawnPoint() const { return this; }

	bool HasTimeTillPedLeaves() const
	{	return iTimeTillPedLeaves != kTimeTillPedLeavesNone;	}

	float GetTimeTillPedLeaves() const;
	void SetTimeTillPedLeaves(float timeInSeconds);

	float	m_fDirection;
	u32		m_uiModelSetHash;
	atFixedBitSet<32> m_Flags;	// Flags from CScenarioPointFlags.
	u16		iType : 9;			// The type of the spawnpoint, from SpawnPointTypes
	s8		iUses;				// The number of peds/dummy peds using them
	u8		iScenarioGroup;		// 0, or one plus the index of the scenario group it belongs to
	u8		iTimeStartOverride;
	u8		iTimeEndOverride;
	u8		iTimeTillPedLeaves;	// Time until leaving this point, overriding CScenarioInfo::m_TimeTilPedLeaves (use accessors to account for special values).
	u8		iInteriorState:3;	// Whether this point is inside or outside, ignored for attached points
	u8		iAvailableInMpSp:2;	// Whether it's available in multiplayer, single player, or both (value from enum AvailabilityMpSp).
	u8		m_bExtendedRange:1;	// Corresponds to CScenarioPoint::m_bExtendedRange.
	u8		m_bShortRange:1;	// Corresponds to CScenarioPoint::m_bShortRange
	u8		m_bHighPriority:1;	// Corresponds to CScenarioPoint::m_bHighPriority.
#if __BANK
	bool	bEditable : 1;		// Whether this point is editable (parented to the world, from scenario point file)
	bool	bFree : 1;			// Whether this point is free (used by editor)
#endif // __BANK
};

#if __BANK
class CLightShaftAttrExt
{
public:
	void Init()
	{
		m_drawNormalDirection = false;
		m_scaleByInteriorFill = true;
		m_gradientAlignToQuad = false;
		m_gradientColour      = Vec3V(V_ONE);
		m_shaftOffset         = Vec3V(V_ZERO);
		m_shaftRadius         = 1.0f;
	}

	bool  m_drawNormalDirection; // if true, shaft direction is forced to normal direction
	bool  m_scaleByInteriorFill; // if false, don't scale intensity by pIntInst->GetDaylightFillScale()
	bool  m_gradientAlignToQuad; // align density gradient to quad (instead of along shaft direction)
	Vec3V m_gradientColour;      // applies to gradient density types
	Vec3V m_shaftOffset;
	float m_shaftRadius;
};
#endif // __BANK

class CLightShaft
{
public:
	void Init();

	// ==================================================================================
	// WARNING!! IF YOU CHANGE THE FIELDS HERE YOU MUST ALSO KEEP IN SYNC WITH:
	// 
	// CExtensionDefLightShaft - x:\gta5\src\dev\game\scene\loader\MapData_Extensions.psc
	// CLightShaft             - x:\gta5\src\dev\game\scene\2dEffect.h
	// 
	// also density type and volume type must match:
	// 
	// x:\gta5\src\dev\game\scene\loader\MapData_Extensions.psc
	// x:\gta5\src\dev\game\renderer\Lights\LightCommon.h
	// ==================================================================================
	Vec3V m_corners[4];
	Vec3V m_direction;
	float m_directionAmount; // could store this in m_direction.w, but we don't have very many of these
	float m_length;
	float m_fadeInTimeStart;
	float m_fadeInTimeEnd;
	float m_fadeOutTimeStart;
	float m_fadeOutTimeEnd;
	float m_fadeDistanceStart;
	float m_fadeDistanceEnd;
	Vec4V m_colour;
	float m_intensity;
	u8    m_flashiness; // eLightFlashiness (FL_*)
	u32   m_flags; // LIGHTSHAFTFLAG_*
	int   m_densityType; // CExtensionDefLightShaftDensityType
	int   m_volumeType; // CExtensionDefLightShaftVolumeType
	float m_softness;

#if __BANK
	CLightShaftAttrExt m_attrExt;
	CLightEntity* m_entity;
#endif // __BANK
};

struct CLightShaftAttr : public C2dEffect
{
#if !__SPU
	EXTENSIONID_DECL(CLightShaftAttr, fwExtension);
#endif //__SPU

	u8 GetType() const { return ET_LIGHT_SHAFT; }
	CLightShaftAttr* GetLightShaft() { return this; }
	const CLightShaftAttr* GetLightShaft() const { return this; }

	virtual void InitArchetypeExtensionFromDefinition(const fwExtensionDef* definition, fwArchetype* archetype);

	void Set(const CLightShaftAttr* attr)
	{
		m_data = attr->m_data;
	}

	CLightShaft m_data;
};


//
// Scrollbar as they would appear in times square.
// The m_posn in C2dEffect is the basepoint (say top left corner)
// The height is the height downwards to the bottom of the bar (say 1 meter)
// The 8 points are the extra points so that the scrollbar can go around corners.
struct CScrollBarAttr : public C2dEffect
{
#if !__SPU
	EXTENSIONID_DECL(CScrollBarAttr, fwExtension);
#endif //__SPU

	u8 GetType() const { return ET_SCROLLBAR; }
	CScrollBarAttr* GetScrollBar() { return this; }
	const CScrollBarAttr* GetScrollBar() const { return this; }

#define MAX_POINTS_IN_SCROLLBAR	(32)	// was 14

	VectorData	m_centrePos;			// calculated by game
	float		m_radius;				// calculated by game
	float		m_height;
	s16		m_pointsX[MAX_POINTS_IN_SCROLLBAR];
	s16		m_pointsY[MAX_POINTS_IN_SCROLLBAR];
	s8		m_numPoints;			// should be at least 1 and at most 16
	s8		m_scrollBarType;		// Financial, theater etc

	s8		m_valuesCalculated;		// calculated by game:	Should always be false
};

struct CNullEffect : public C2dEffect
{
	u8 GetType() const { return ET_NULLEFFECT; }
};

class CPed;

#define PEDQUEUE_DISABLED (1<<0)

struct CPedQueue
{
	Vector3 	m_posn;
	VectorData 	m_queueDir;			// direction the queue faces
	VectorData 	m_useDir;			// direction faced when using the attractor
	VectorData   m_forwardDir;		// attractors' forward direction (can only be seen infront of this)
	u8   m_type; 			// sub type of attractor (shop queue, atm queue etc)
	u8 	m_interest;			// probability of using the attractor (as a %)
	u8	m_lookAt;			// probability of looking at the attractor instead of fully using it (as a %)
	u8	m_flags;			// flags mo fo
	u8	m_MaxSlots;			// how many peds can be in the queue
	s8	m_ScriptQueueType;	//	can be used to determine if a particular ped should attempt to use the queue
	VectorData m_vecScriptQueueAlternativeHead;
	float m_fScriptQueueAlternativeHeadRadiusSquared;

	//	GW 05/09/06 - queues no longer trigger scripts
	// char	m_scriptName[8];	// name of script to trigger at attractor (trigger script attractors only)
};

//
// Expression extension
//

struct CExpressionExtension : public C2dEffect
{
#if !__SPU
	EXTENSIONID_DECL(CExpressionExtension, fwExtension);
#endif //__SPU

	atFinalHashString	m_expressionDictionaryName;
	atFinalHashString	m_expressionName;
	atFinalHashString	m_creatureMetadataName;

	void InitArchetypeExtensionFromDefinition(const fwExtensionDef* definition, fwArchetype* archetype);

	u8 GetType() const { return ET_EXPRESSION; }
	CExpressionExtension* GetExpression() { return this; }
	const CExpressionExtension* GetExpression() const { return this; }
};


#if	__BANK

class C2dEffectDebugRenderer
{
public:

	C2dEffectDebugRenderer() {}
	~C2dEffectDebugRenderer() {}

	static void InitWidgets(bkBank& bank);
	static void Update();

private:

	static void DebugDraw2DEffects();
	static void Render2dEffect(CEntity *pEntity, C2dEffect *pEffect);
	static void DrawCoordinateFrame(Vector3 &pos);

	static atFixedArray<int, ET_MAX_2D_EFFECTS>	m_TallyCounts;
	static void Tally2DEffects();
	static void	TallyEffect(C2dEffect *pEffect);
	static void DumpEffectTally();
	static int	GetEffectSize(u8 type);


	static bool		m_bEnabled;
	static float	m_fDrawRadius;
	static bool		m_bRenderEntity2dEffects;
	static bool		m_bRenderWorld2dEffects;
	static bool		m_bRenderDummy2dEffects;
	static bool		m_bRenderObject2dEffects;
	static bool		m_bRenderBuilding2dEffects;
	static bool		m_bRenderABuilding2dEffects;

};

#endif	//__BANK

#endif
