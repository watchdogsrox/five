#ifndef LIGHT_SOURCE_H_
#define LIGHT_SOURCE_H_

// =============================================================================================== //
// INCLUDES
// =============================================================================================== //

#if !__SPU
// rage headers
#include "vector/vector3.h"

// game headers
#include "scene/2dEffect.h"
#include "scene/EntityId.h"
#include "renderer/Shadows/Shadows.h" 
#include "renderer/Lights/LightCommon.h"

#endif //!__SPU

#include "../Shadows/ShadowTypes.h"

// framework headers
#include "fwscene/world/InteriorLocation.h"
#include "fwmaths/vectorutil.h"
#include "fwutil/idgen.h"

// =============================================================================================== //
// DEFINES
// =============================================================================================== //

#define EXTEND_LIGHT_OTHER					(0.029f)
#define EXTEND_LIGHT_POINT					(0.058f)
#define INVALID_TEXTURE_KEY					(0)

#define EXTEND_LIGHT(type) (type == LIGHT_TYPE_POINT ? EXTEND_LIGHT_POINT : EXTEND_LIGHT_OTHER)

// ----------------------------------------------------------------------------------------------- //

class CLightEntity;
class CVehicle;

// ----------------------------------------------------------------------------------------------- //

// General
enum eLightGeneral
{
	LS_FALLOFF_EXPONENT= 0,
	LS_PLANE_NORMAL_X,
	LS_PLANE_NORMAL_Y,
	LS_PLANE_NORMAL_Z,
	LS_PLANE_DIST,
	LS_NUM_GENERAL,

	LS_MAX_PARAMS = 13
};

// ----------------------------------------------------------------------------------------------- //

// Spot Light
enum eLightSpot
{
	LS_SPOT_CONE_INNER_ANGLE_COS = LS_NUM_GENERAL,
	LS_SPOT_CONE_OUTER_ANGLE_COS,
	LS_SPOT_CONE_INNER_ANGLE,
	LS_SPOT_CONE_OUTER_ANGLE,
	LS_SPOT_CONE_BASE_RADIUS,
	LS_SPOT_CONE_HEIGHT,
	LS_SPOT_CONE_VEHICLE_HEADLIGHT_OFFSET,
	LS_SPOT_CONE_VEHICLE_HEADLIGHT_SPLIT_OFFSET,
	LS_SPOT_NUM,
};
CompileTimeAssert(LS_SPOT_NUM <= (int)LS_MAX_PARAMS);

// ----------------------------------------------------------------------------------------------- //

// AO Volume Light
enum eLightAO
{
	LS_VOLUME_SIZE_X = LS_NUM_GENERAL,
	LS_VOLUME_SIZE_Y,
	LS_VOLUME_SIZE_Z,
	LS_VOLUME_BASE_INTENSITY,
	LS_VOLUME_NUM
};
CompileTimeAssert(LS_VOLUME_NUM <= (int)LS_MAX_PARAMS);

// ----------------------------------------------------------------------------------------------- //

enum eLightCapsule
{
	LS_CAPSULE_EXTENT = LS_NUM_GENERAL,
	LS_CAPSULE_NUM
};
CompileTimeAssert(LS_CAPSULE_NUM <= (int)LS_MAX_PARAMS);

// ----------------------------------------------------------------------------------------------- //

#define LIGHT_ALWAYS_ON	((1<<24) - 1) // use with time flags

// ----------------------------------------------------------------------------------------------- //

enum eLightFlashiness
{
	FL_CONSTANT = 0,				// on all the time
	FL_RANDOM,						// flickers randomly
	FL_RANDOM_OVERRIDE_IF_WET,		// flickers randomly (forced if road is wet)
	FL_ONCE_SECOND,					// on/off once every second
	FL_TWICE_SECOND,				// on/off twice every second
	FL_FIVE_SECOND,					// on/off five times every second
	FL_RANDOM_FLASHINESS,			// might flicker, might not
	FL_OFF,							// always off. really only used for traffic lights
	FL_UNUSED1,						// Not used
	FL_ALARM,						// Flashes on briefly
	FL_ON_WHEN_RAINING,				// Only render when it's raining.
	FL_CYCLE_1,						// Fades in and out in so that the three of them together always are on (but cycle through colours)
	FL_CYCLE_2,						// Fades in and out in so that the three of them together always are on (but cycle through colours)
	FL_CYCLE_3,						// Fades in and out in so that the three of them together always are on (but cycle through colours)
	FL_DISCO,						// In tune with the music
	FL_CANDLE,						// Just like a candle
	FL_PLANE,						// The little lights on planes/helis. They flash briefly
	FL_FIRE,						// A more hectic version of the candle
	FL_THRESHOLD,					// experimental
	FL_ELECTRIC,					// Change colour slightly
	FL_STROBE,						// Strobe light
	FL_COUNT
};

// ----------------------------------------------------------------------------------------------- //

#define LIGHTFLAG_INTERIOR_ONLY					(1<<0)
#define LIGHTFLAG_EXTERIOR_ONLY					(1<<1)
#define LIGHTFLAG_DONT_USE_IN_CUTSCENE			(1<<2)
#define LIGHTFLAG_VEHICLE						(1<<3)
#define LIGHTFLAG_FX							(1<<4)
#define LIGHTFLAG_TEXTURE_PROJECTION			(1<<5)
#define LIGHTFLAG_CAST_SHADOWS					(1<<6)
#define LIGHTFLAG_CAST_STATIC_GEOM_SHADOWS		(1<<7)
#define LIGHTFLAG_CAST_DYNAMIC_GEOM_SHADOWS		(1<<8)
#define LIGHTFLAG_CALC_FROM_SUN					(1<<9)
#define LIGHTFLAG_ENABLE_BUZZING				(1<<10)
#define LIGHTFLAG_FORCE_BUZZING					(1<<11)
#define LIGHTFLAG_DRAW_VOLUME					(1<<12)
#define LIGHTFLAG_NO_SPECULAR					(1<<13)
#define LIGHTFLAG_BOTH_INTERIOR_AND_EXTERIOR	(1<<14)
#define LIGHTFLAG_CORONA_ONLY					(1<<15)
#define LIGHTFLAG_NOT_IN_REFLECTION				(1<<16)
#define LIGHTFLAG_ONLY_IN_REFLECTION			(1<<17)
#define LIGHTFLAG_USE_CULL_PLANE				(1<<18)
#define LIGHTFLAG_USE_VOLUME_OUTER_COLOUR		(1<<19)
#define LIGHTFLAG_CAST_HIGHER_RES_SHADOWS		(1<<20) // bump the shadow cache priority for this light so it will use hires shadow maps sooner than it's screen space size would suggest
#define LIGHTFLAG_CAST_ONLY_LOWRES_SHADOWS		(1<<21) // never use hires shadow map for this light, even if it's really up close
#define	LIGHTFLAG_FAR_LOD_LIGHT					(1<<22)
#define	LIGHTFLAG_DONT_LIGHT_ALPHA				(1<<23)
#define LIGHTFLAG_CAST_SHADOWS_IF_POSSIBLE		(1<<24)
#define LIGHTFLAG_CUTSCENE						(1<<25)
#define LIGHTFLAG_MOVING_LIGHT_SOURCE			(1<<26) // runtime flag that lets the shadow code know this light moves a lot and might self shadow, to it needs to update late (with the current frames light position)
#define LIGHTFLAG_USE_VEHICLE_TWIN				(1<<27) // used for detecting special twin lights.
#define LIGHTFLAG_FORCE_MEDIUM_LOD_LIGHT		(1<<28)
#define LIGHTFLAG_CORONA_ONLY_LOD_LIGHT			(1<<29)
#define LIGHTFLAG_DELAY_RENDER					(1<<30) // used by cutscenes to create shadow casting lights a frame early and avoid any visible pops
#define LIGHTFLAG_ALREADY_TESTED_FOR_OCCLUSION	(1<<31)	// set by ProcessLightOcclusionAsync to mark that this light has already passed the IsVisible() check so we can skip it when adding the light

#define EXTRA_LIGHTFLAG_VOL_USE_HIGH_NEARPLANE_FADE	(1<<0) // used by volume lights for applying near plane fade
#define EXTRA_LIGHTFLAG_CAUSTIC						(1<<1) // for spotlights that need caustic texture blending underwater
#define EXTRA_LIGHTFLAG_HIGH_PRIORITY				(1<<2)
#define EXTRA_LIGHTFLAG_FLIP_TEXTURE				(1<<3) // for flipping the texture coordinates for one half of twin vehicle lights
#define EXTRA_LIGHTFLAG_SOFT_SHADOWS				(1<<4)
#define EXTRA_LIGHTFLAG_USE_FADEDIST_AS_FADEVAL		(1<<5)
#define EXTRA_LIGHTFLAG_FORCE_LOWRES_VOLUME			(1<<6)
#define EXTRA_LIGHTFLAG_USE_AS_PED_ONLY				(1<<7)

#define LIGHTSHAFTFLAG_USE_SUN_LIGHT_DIRECTION	(1<<0)
#define LIGHTSHAFTFLAG_USE_SUN_LIGHT_COLOUR		(1<<1)
#define LIGHTSHAFTFLAG_UNUSED_2					(1<<2) // LIGHTSHAFTFLAG_SHADOWED
#define LIGHTSHAFTFLAG_UNUSED_3					(1<<3) // LIGHTSHAFTFLAG_PER_PIXEL_SHADOWED
#define LIGHTSHAFTFLAG_SCALE_BY_SUN_COLOUR		(1<<4)
#define LIGHTSHAFTFLAG_SCALE_BY_SUN_INTENSITY	(1<<5)
#define LIGHTSHAFTFLAG_DRAW_IN_FRONT_AND_BEHIND	(1<<6)
#define LIGHTSHAFTFLAG_UNUSED_7					(1<<7) // LIGHTSHAFTFLAG_SQUARED_OFF_ENDCAP

#define LIGHTSHAFTFLAG_DEFAULT \
	LIGHTSHAFTFLAG_USE_SUN_LIGHT_DIRECTION | \
	LIGHTSHAFTFLAG_USE_SUN_LIGHT_COLOUR | \
	LIGHTSHAFTFLAG_SCALE_BY_SUN_INTENSITY

#define LIGHTSHAFT_NEAR_CLIP_EXPONENT_DEFAULT (7.0f)


// =============================================================================================== //
// CLASS
// =============================================================================================== //

class CLightSource
{
	// ----------------------------------------------------------------------------------------------- //
	// Variables
	// ----------------------------------------------------------------------------------------------- //

public:

	enum
	{
		MAX_SHADOW_EXCLUSION_ENTITIES = 24
	};

	typedef atFixedArray<fwEntity*, MAX_SHADOW_EXCLUSION_ENTITIES> ShadowEntityArray;

private:

	Vector3				m_pos;							// Light position [[SPHERE-OPTIMISE]] -- store m_posAndRadius as Vec4V
	Vector3				m_color;						// Light color -- TODO - store m_colorAndIntensity as Vec4V

	Vector3				m_dir;							// Light direction
	Vector3				m_tanDir;						// DEFERRED LIGHTING ONLY - used for projecting textures and ambient occluders

	Vec4V				m_volOuterColourAndIntensity;	// w is intensity

	int				    m_scissorRectX;
	int				    m_scissorRectY;
	int				    m_scissorRectWidth;
	int				    m_scissorRectHeight;

	eLightType			m_type;							// type of light source
	u32					m_flags;						// used for shadows etc
	float				m_intensity;					// Light intensity
	u32					m_timeFlags:24;					// 24 time flags (one for each hour)
	u32					m_extraFlags:8;					// 8 bits for extra flags

	s32 				m_texDictID;	
	u32 				m_textureKey;					// modulation texture (place holder)

	float				m_volIntensity;					// volume intensity (brightness)
	float				m_volSizeScale;					// volume radius scale (1 means the same radius as the light)
	float				m_volOuterExponent;				// experimental

	fwUniqueObjId		m_shadowTrackingID;				// Used by shadow system to track scene lights - 0 if no shadows and tracking
	s32					m_shadowIndex;

	fwInteriorLocation	m_interiorLocation;				// interior location

	float				m_radius;						// light radius (meters) end
	float				m_params[LS_MAX_PARAMS];

	u8					m_lightFadeDistance;
	u8					m_shadowFadeDistance;
	u8					m_specularFadeDistance;
	u8					m_volumetricFadeDistance;

	float				m_shadowNearClip;
	float				m_shadowBlur;
	float				m_shadowDepthBiasScale;

	float				m_parentAlpha;

	CLightEntity*		m_entity;

	atFixedArray<entitySelectID, MAX_SHADOW_EXCLUSION_ENTITIES> m_ShadowExclusionEntities;

	// ----------------------------------------------------------------------------------------------- //

public:

	// ----------------------------------------------------------------------------------------------- //
	// Functions
	// ----------------------------------------------------------------------------------------------- //

	CLightSource() { Reset(); }

	// ----------------------------------------------------------------------------------------------- //

	CLightSource(
		eLightType t_Type, 
		int t_flags, 
		const Vector3& t_Pos, 
		const Vector3& t_Color, 
		float t_Intensity, 
		u32 t_timeFlags)
	{
		Reset();
		SetCommon(t_Type, t_flags, t_Pos, t_Color, t_Intensity, t_timeFlags);
	}

	// ----------------------------------------------------------------------------------------------- //

	void Reset();

	// ----------------------------------------------------------------------------------------------- //
	// Accessors
	// ----------------------------------------------------------------------------------------------- //

	// Flags
	bool IsCoronaOnly() const { return ((m_flags & LIGHTFLAG_CORONA_ONLY) != 0); }
	bool IsNoSpecular() const { return ((m_flags & LIGHTFLAG_NO_SPECULAR) != 0); }
	bool IsCastShadows() const { return ((m_flags & LIGHTFLAG_CAST_SHADOWS) != 0); }

	//Extra Flags
	bool IsVolumeUseHighNearPlaneFade() const { return ((m_extraFlags & EXTRA_LIGHTFLAG_VOL_USE_HIGH_NEARPLANE_FADE) != 0); }
	bool UseAsPedOnly()					const { return ((m_extraFlags & EXTRA_LIGHTFLAG_USE_AS_PED_ONLY) != 0); }

	// Spot
	float GetConeCosInnerAngle() const { return m_params[LS_SPOT_CONE_INNER_ANGLE_COS]; }
	float GetConeCosOuterAngle() const { return m_params[LS_SPOT_CONE_OUTER_ANGLE_COS]; }
	float GetConeInnerAngle() const { return m_params[LS_SPOT_CONE_INNER_ANGLE]; }
	float GetConeOuterAngle() const { return m_params[LS_SPOT_CONE_OUTER_ANGLE]; }
	float GetConeBaseRadius() const {return m_params[LS_SPOT_CONE_BASE_RADIUS]; }
	float GetConeHeight() const {return m_params[LS_SPOT_CONE_HEIGHT]; }
	float GetVehicleHeadlightOffset() const {return m_params[LS_SPOT_CONE_VEHICLE_HEADLIGHT_OFFSET];}
	float GetVehicleHeadlightSplitOffset() const {return m_params[LS_SPOT_CONE_VEHICLE_HEADLIGHT_SPLIT_OFFSET];}

	// AO Volume
	float GetVolumeSizeX() const { return m_params[LS_VOLUME_SIZE_X]; }
	float GetVolumeSizeY() const { return m_params[LS_VOLUME_SIZE_Y]; }
	float GetVolumeSizeZ() const { return m_params[LS_VOLUME_SIZE_Z]; }
	float GetVolumeBaseIntensity() const { return m_params[LS_VOLUME_BASE_INTENSITY]; }
	
	// Capsule
	float GetCapsuleExtent() const { return (m_type == LIGHT_TYPE_CAPSULE) ? m_params[LS_CAPSULE_EXTENT] : 0.0f; }

	// General
	float GetRadius() const { return m_radius; }
	Vector3 GetDirection() const { return m_dir; }
	Vector3 GetTangent() const { return m_tanDir; }
	Vector3 GetPosition() const { return m_pos; }
	Vector3 GetColor() const { return m_color; }
	Vec4V_Out GetColorAndIntensity() const { return Vec4V(RCC_VEC3V(m_color), ScalarV(m_intensity)); }
	Vec3V_Out GetColorTimesIntensity() const { return RCC_VEC3V(m_color)*ScalarV(m_intensity); }
	float GetIntensity() const { return m_intensity; }
	eLightType GetType() const { return m_type; }
	u32 GetFlags() const { return m_flags; }
	u8 GetExtraFlags() const { return m_extraFlags; }
	s32 GetTextureDict() const { return m_texDictID; }
	u32 GetTextureKey() const { return m_textureKey; }
	bool GetFlipTexture() const { return GetExtraFlag(EXTRA_LIGHTFLAG_FLIP_TEXTURE); }
	float GetVolumeIntensity() const { return m_volIntensity; }
	float GetVolumeSizeScale() const { return m_volSizeScale; }
	bool HasValidTextureKey() const { return (m_textureKey != INVALID_TEXTURE_KEY); }
	bool HasValidTextureDict() const { return (m_texDictID != -1); }

	fwUniqueObjId GetShadowTrackingId() const { return m_shadowTrackingID; }
	s32 GetShadowIndex() const { return m_shadowIndex; }
	u32 GetTimeFlags() const { return m_timeFlags; }
	fwInteriorLocation GetInteriorLocation() const { return m_interiorLocation; }
	bool InInterior() const { return m_interiorLocation.IsValid(); }
	Vector4 GetPlane() const { 
		return GetFlag(LIGHTFLAG_USE_CULL_PLANE) ? 
			Vector4(m_params[LS_PLANE_NORMAL_X], m_params[LS_PLANE_NORMAL_Y], m_params[LS_PLANE_NORMAL_Z], m_params[LS_PLANE_DIST]) : 
			Vector4(0.0f, 0.0f, 0.0f, 0.0f); 
	}
	__forceinline void GetPlane(Vector4& outVector) const { 
		outVector.Zero();
		if(GetFlag(LIGHTFLAG_USE_CULL_PLANE))
		{
			outVector.Set(m_params[LS_PLANE_NORMAL_X], m_params[LS_PLANE_NORMAL_Y], m_params[LS_PLANE_NORMAL_Z], m_params[LS_PLANE_DIST]);
		}
	}
	float GetFalloffExponent() const { return m_params[LS_FALLOFF_EXPONENT]; }
	float GetBoundingSphereRadius() const; 
	Vec4V_Out GetVolOuterColour() const;
	u32 GetPassKey() const;
	u8 GetLightFadeDistance() const { return m_lightFadeDistance; }
	u8 GetShadowFadeDistance() const { return m_shadowFadeDistance; }
	u8 GetSpecularFadeDistance() const { return m_specularFadeDistance; }
	u8 GetVolumetricFadeDistance() const { return m_volumetricFadeDistance; }

	float GetShadowNearClip() const { return m_shadowNearClip; }
	float GetShadowDepthBiasScale() const { return m_shadowDepthBiasScale; }
	float GetShadowBlur() const { return m_shadowBlur; }

	bool UsesCaustic() const { return (m_extraFlags & EXTRA_LIGHTFLAG_CAUSTIC ) != 0; }

	int GetShadowExclusionEntitiesCount() const { return m_ShadowExclusionEntities.GetCount(); }
	fwEntity *GetShadowExclusionEntity(int i) const { return CEntityIDHelper::GetEntityFromID(m_ShadowExclusionEntities[i]); }

	void CopyShadowExclusionEntities(const CLightSource & srcLight)  { m_ShadowExclusionEntities=srcLight.m_ShadowExclusionEntities; }

	CLightEntity* GetLightEntity() const { return m_entity; }

	float GetParentAlpha() const { return m_parentAlpha; }

	bool IsPointInLight(const Vector3& point, const float radiusIncrease) const;

	// ----------------------------------------------------------------------------------------------- //
	// Setters
	// ----------------------------------------------------------------------------------------------- //

	void SetRadius(const float t_radius);
	void SetDirection(const Vector3 &t_dir) { m_dir = t_dir; }
	void SetPosition(const Vector3 &t_pos) { m_pos = t_pos; }
	void SetColor(const Vector3 &t_color) { m_color = t_color; }
	void SetIntensity(const float t_intensity) { m_intensity = t_intensity; }
	void SetType(const eLightType t_type) { m_type = t_type; }
	void SetFlags(const u32 t_flags) { m_flags = t_flags; }
	void SetExtraFlags(const u8 t_flags) { m_extraFlags = t_flags; }
	void SetTextureDict(const s32 t_texDictID) { m_texDictID = t_texDictID; }
	void SetTextureKey(const u32 t_textureKey) { m_textureKey = t_textureKey; }
	void SetVolumeIntensity(const float t_volIntensity) { m_volIntensity = t_volIntensity; }
	void SetVolumeSizeScale(const float t_volSizeScale) { m_volSizeScale = t_volSizeScale; }
	void SetShadowIndex(const s32 t_shadowIndex) { m_shadowIndex = t_shadowIndex; }
	void SetTimeFlags(const u32 t_timeFlags) { m_timeFlags = t_timeFlags; }
	void SetLightFadeDistance(const u8 t_lightFadeDistance) { m_lightFadeDistance = t_lightFadeDistance; }
	void SetShadowFadeDistance(const u8 t_shadowFadeDistance) { m_shadowFadeDistance = t_shadowFadeDistance; }
	void SetSpecularFadeDistance(const u8 t_specularFadeDistance) { m_specularFadeDistance = t_specularFadeDistance; }
	void SetVolumetricFadeDistance(const u8 t_volumetricFadeDistance) { m_volumetricFadeDistance = t_volumetricFadeDistance; }
	void SetShadowNearClip(const float t_shadowNearClip) { m_shadowNearClip = rage::Max<float>(0.01f, t_shadowNearClip); }
	void SetShadowDepthBiasScale(const float t_shadowDepthBiasScale) { m_shadowNearClip = rage::Clamp<float>(t_shadowDepthBiasScale, 0.0f, 1.0f); }
	void SetShadowBlur(const float t_shadowBlur) 
	{
		m_shadowBlur = t_shadowBlur;
		if(m_shadowBlur == 0.0f)
			ClearExtraFlag(EXTRA_LIGHTFLAG_SOFT_SHADOWS);
		else
			SetExtraFlag(EXTRA_LIGHTFLAG_SOFT_SHADOWS);
	}

	void SetCommon(
		eLightType t_Type, 
		int t_flags, 
		const Vector3& t_Pos, 
		const Vector3& t_Color,
		float t_Intensity, 
		u32 t_timeFlags);

	void SetDirTangent(const Vector3& t_Dir, const Vector3& t_tanDir);
	void SetTexture(const u32 t_texKey, const s32 t_texDictID ASSERT_ONLY(, const char* textureName = NULL));
	void SetLightVolumeIntensity(const float t_volIntensity, const float t_volSizeScale = 1.0f, Vec4V_In volOuterColourAndIntensity = Vec4V(V_ONE), const float t_volOuterExponent = 1.0f);
#if !__SPU
	void SetInInterior(const fwInteriorLocation t_intLoc);
	void SetScissorRect(int x, int y, int width, int height) { m_scissorRectX = x; m_scissorRectY = y; m_scissorRectWidth = width; m_scissorRectHeight = height;}
	void GetScissorRect(int &x, int &y, int &width, int &height) { x = m_scissorRectX; y = m_scissorRectY; width = m_scissorRectWidth; height = m_scissorRectHeight; }

	void SetShadowTrackingId(const fwUniqueObjId t_shadowTrackingID);
#endif
	void SetSpotlight(const float t_InnerConeAngle, const float t_OuterConeAngle);
	void SetVehicleHeadlightOffset(float t_offset);
	void SetVehicleHeadlightSplitOffset(float t_split);
	void SetAOVolume(const float t_sizeX, const float t_sizeY, const float t_sizeZ);
	void SetAOVolumeBaseIntensity(const float intensity);
	void SetPlane(const float nx, const float ny, const float nz, const float dist, const Matrix34& mtx);
	void SetWorldPlane(const float nx, const float ny, const float nz, const float dist);
	void SetFalloffExponent(const float exponent);
	void SetCapsule(const float t_CapsuleExtent);

	// Flag manipulation
	void SetFlag(s32 flag) { m_flags |= flag; }
	bool GetFlag(s32 flag) const { return (m_flags & flag) != 0; }
	void ClearFlag(s32 flag) { m_flags &= ~flag; }

	void SetExtraFlag(u8 flag) { m_extraFlags |= flag; }
	bool GetExtraFlag(u8 flag) const { return (m_extraFlags & flag) != 0; }
	void ClearExtraFlag(u8 flag) { m_extraFlags &= ~flag; }

	void AddToShadowExclusionList(fwEntity* pEntity)
	{
		if(Verifyf(!m_ShadowExclusionEntities.IsFull(), "Not enough room to add entity - increase MAX_SHADOW_EXCLUSION_ENTITIES"))
		{
			m_ShadowExclusionEntities.Append() = CEntityIDHelper::ComputeEntityID(pEntity);
		}
	}

	void SetLightEntity(CLightEntity *t_entity) { m_entity = t_entity; }

	void SetParentAlpha(float t_parentAlpha) { m_parentAlpha = t_parentAlpha; }
};

// ----------------------------------------------------------------------------------------------- //
// Setters
// ----------------------------------------------------------------------------------------------- //

inline void CLightSource::Reset()
{
	m_textureKey = INVALID_TEXTURE_KEY;
	m_texDictID  = -1;
	m_dir = Vector3(0.0, 0.0, 1.0);
	m_tanDir = Vector3(0.0, 1.0, 0.0);
	m_interiorLocation.MakeInvalid();
	m_volIntensity = 1.0f;
	m_volSizeScale = 1.0f;
	m_volOuterColourAndIntensity = Vec4V(V_ONE);
	m_volOuterExponent = 1.0f;

	m_lightFadeDistance = 0U;
	m_shadowFadeDistance = 0U;
	m_specularFadeDistance = 0U;
	m_volumetricFadeDistance = 0U;
	m_extraFlags = 0;

	m_radius = 1.0f;
	m_shadowTrackingID = 0;
	m_shadowIndex = -1;
	m_shadowNearClip = 0.01f;
	m_shadowDepthBiasScale = 1.0f;
	m_shadowBlur = 0.0f;

	m_ShadowExclusionEntities.Reset();

	m_scissorRectX		= -1;
	m_scissorRectY		= -1;
	m_scissorRectWidth  = -1;
	m_scissorRectHeight = -1;

	m_parentAlpha = 1.0f;
	
	m_entity = NULL;
}

// ----------------------------------------------------------------------------------------------- //

inline void CLightSource::SetRadius(const float t_radius)
{
	m_radius = t_radius;
}

// ----------------------------------------------------------------------------------------------- //

inline void CLightSource::SetCommon(
							 eLightType t_Type, 
							 int t_flags, 
							 const Vector3& t_Pos, 
							 const Vector3& t_Color, 
							 float t_Intensity, 
							 u32 t_timeFlags)
{
	m_type = t_Type;
	m_flags	= t_flags;
	m_pos = t_Pos;
	m_color	= t_Color;
	m_intensity	= t_Intensity;	
	m_timeFlags = t_timeFlags;		

	memset(m_params, 0, sizeof(float) * LS_MAX_PARAMS);
	m_params[LS_FALLOFF_EXPONENT] = 8.0f;

	if (m_flags & (LIGHTFLAG_CUTSCENE | LIGHTFLAG_VEHICLE | LIGHTFLAG_FX | LIGHTFLAG_BOTH_INTERIOR_AND_EXTERIOR))
	{
		m_flags &= ~LIGHTFLAG_INTERIOR_ONLY;
		m_flags &= ~LIGHTFLAG_EXTERIOR_ONLY;
	}
	else
	{
		// Assume light is outside (as interior not set)
		m_flags |= LIGHTFLAG_EXTERIOR_ONLY;
		m_flags &= ~LIGHTFLAG_INTERIOR_ONLY;
	}
}

// ----------------------------------------------------------------------------------------------- //

inline void CLightSource::SetDirTangent(const Vector3& t_Dir, const Vector3& t_tanDir)
{
	m_dir.Normalize(t_Dir);
	m_tanDir.Normalize(t_tanDir);
}

// ----------------------------------------------------------------------------------------------- //

inline void CLightSource::SetTexture(const u32 t_texKey, const s32 t_texDictID ASSERT_ONLY(, const char* textureName))
{
	m_textureKey = t_texKey;
	m_texDictID	= t_texDictID; 

	#if __ASSERT
	if (HasValidTextureKey() && HasValidTextureDict())
	{
		strLocalIndex idx = strLocalIndex(m_texDictID);

		fwTxd* texDict = g_TxdStore.Get(idx);
		Assertf(texDict, "Texture dictionary '%s' not found", g_TxdStore.GetName(idx));
		
		if (texDict)
		{
			grcTexture* pTexture = texDict->Lookup(m_textureKey);
			Assertf(pTexture, "Texture '%s' not present in dictionary '%s'", (textureName != NULL) ? textureName : "N/A", g_TxdStore.GetName(idx));
		}
	}
	#endif
}

// ----------------------------------------------------------------------------------------------- //

inline void CLightSource::SetLightVolumeIntensity(const float t_volIntensity, const float t_volSizeScale, Vec4V_In volOuterColourAndIntensity, const float t_volOuterExponent)
{
	m_volIntensity = t_volIntensity;
	m_volSizeScale = t_volSizeScale;
	m_volOuterColourAndIntensity = volOuterColourAndIntensity;
	m_volOuterExponent = t_volOuterExponent;
}

// ----------------------------------------------------------------------------------------------- //
#if !__SPU
// ----------------------------------------------------------------------------------------------- //

inline void CLightSource::SetInInterior(const fwInteriorLocation t_intLoc)
{
	m_interiorLocation = t_intLoc;

	if (m_interiorLocation.IsValid() && !(m_flags & (LIGHTFLAG_CUTSCENE | LIGHTFLAG_VEHICLE | LIGHTFLAG_FX | LIGHTFLAG_BOTH_INTERIOR_AND_EXTERIOR)))
	{
		m_flags |= LIGHTFLAG_INTERIOR_ONLY;
		m_flags &= ~LIGHTFLAG_EXTERIOR_ONLY;
	}
}

// ----------------------------------------------------------------------------------------------- //

inline void CLightSource::SetShadowTrackingId(const fwUniqueObjId t_shadowTrackingID)
{
	m_shadowTrackingID = t_shadowTrackingID;

	Assertf(t_shadowTrackingID != -1, "Use 0, not -1 for the shadow tracking ID for lights with no shadow" ); // -1 keeps sneeking in, but there is not point in testing for 0 and -1

	if (m_shadowTrackingID != 0 && (m_type == LIGHT_TYPE_POINT || m_type == LIGHT_TYPE_SPOT) )
	{
		m_shadowIndex = CParaboloidShadow::UpdateLight(*this);
	}
	else
	{
		m_shadowIndex = -1;
	}
}

// ----------------------------------------------------------------------------------------------- //
#endif // !__SPU
// ----------------------------------------------------------------------------------------------- //

inline void CLightSource::SetSpotlight(const float t_InnerConeAngle, const float t_OuterConeAngle)
{
	static dev_float debugSmallDelta=1.0f;
	const float minConeAngle = 1.f;

	float outerConeAngle = rage::Clamp(t_OuterConeAngle, minConeAngle, 90.0f - debugSmallDelta);
	float innerConeAngle = rage::Clamp(t_InnerConeAngle, minConeAngle, outerConeAngle - debugSmallDelta);

	m_params[LS_SPOT_CONE_INNER_ANGLE] = innerConeAngle;
	m_params[LS_SPOT_CONE_OUTER_ANGLE] = outerConeAngle;

	m_params[LS_SPOT_CONE_INNER_ANGLE_COS] = Cosf(m_params[LS_SPOT_CONE_INNER_ANGLE] * DtoR);
	m_params[LS_SPOT_CONE_OUTER_ANGLE_COS] = Cosf(m_params[LS_SPOT_CONE_OUTER_ANGLE] * DtoR);

	const float cosAngle = m_params[LS_SPOT_CONE_OUTER_ANGLE_COS];
	const float tanAngle = sqrtf(1.0f - cosAngle*cosAngle) / cosAngle;

	m_params[LS_SPOT_CONE_HEIGHT] = m_radius * cosAngle;
	m_params[LS_SPOT_CONE_BASE_RADIUS] = m_params[LS_SPOT_CONE_HEIGHT] * tanAngle; // same as m_radius * sinAngle
}

inline void CLightSource::SetVehicleHeadlightOffset(float t_offset)
{
	m_params[LS_SPOT_CONE_VEHICLE_HEADLIGHT_OFFSET] = t_offset;

}

inline void CLightSource::SetVehicleHeadlightSplitOffset(float t_split)
{
	m_params[LS_SPOT_CONE_VEHICLE_HEADLIGHT_SPLIT_OFFSET] = t_split;
}

// ----------------------------------------------------------------------------------------------- //

inline void CLightSource::SetAOVolume(const float t_sizeX, const float t_sizeY, const float t_sizeZ)
{
	m_params[LS_VOLUME_SIZE_X] = t_sizeX;
	m_params[LS_VOLUME_SIZE_Y] = t_sizeY;
	m_params[LS_VOLUME_SIZE_Z] = t_sizeZ;
}

inline void CLightSource::SetAOVolumeBaseIntensity(const float intensity)
{
	m_params[LS_VOLUME_BASE_INTENSITY] = intensity;
}

// ----------------------------------------------------------------------------------------------- //

__forceinline void CLightSource::SetCapsule(const float t_CapsuleExtent)
{
	m_params[LS_CAPSULE_EXTENT] = t_CapsuleExtent;
}

// ----------------------------------------------------------------------------------------------- //

inline void CLightSource::SetPlane(const float nx, const float ny, const float nz, const float dist, const Matrix34& mtx)
{
	if ((m_flags & LIGHTFLAG_USE_CULL_PLANE) != 0)
	{
		Vector3 planeNormal = Vector3(nx, ny, nz);
		mtx.Transform3x3(planeNormal);

		const float offset = dist;
		const Vector3 planeOrigin = GetPosition() - planeNormal * offset; 

		m_params[LS_PLANE_NORMAL_X] = planeNormal.GetX();
		m_params[LS_PLANE_NORMAL_Y] = planeNormal.GetY();
		m_params[LS_PLANE_NORMAL_Z] = planeNormal.GetZ();
		m_params[LS_PLANE_DIST] = -planeOrigin.Dot(planeNormal);
	}
}

// ----------------------------------------------------------------------------------------------- //

inline void CLightSource::SetWorldPlane(const float nx, const float ny, const float nz, const float dist)
{
	m_params[LS_PLANE_NORMAL_X] = nx;
	m_params[LS_PLANE_NORMAL_Y] = ny;
	m_params[LS_PLANE_NORMAL_Z] = nz;
	m_params[LS_PLANE_DIST] = dist;
}


// ----------------------------------------------------------------------------------------------- //

inline void CLightSource::SetFalloffExponent(const float exponent)
{
	m_params[LS_FALLOFF_EXPONENT] = (exponent == 0.0f) ? 1e-6f : exponent;
}

// ----------------------------------------------------------------------------------------------- //

inline float CLightSource::GetBoundingSphereRadius() const
{
	float radius = ((m_type == LIGHT_TYPE_CAPSULE) ? m_radius + (m_params[LS_CAPSULE_EXTENT] * 0.5f) : m_radius);
	return radius + radius * EXTEND_LIGHT(m_type) * (m_type == LIGHT_TYPE_SPOT ? 2.0f : 1.0f);
}

// ----------------------------------------------------------------------------------------------- //

inline Vec4V_Out CLightSource::GetVolOuterColour() const
{
	if (m_flags & LIGHTFLAG_USE_VOLUME_OUTER_COLOUR)
	{
		return Vec4V(m_volOuterColourAndIntensity.GetXYZ() * m_volOuterColourAndIntensity.GetW(), ScalarV(m_volOuterExponent));
	}

	return Vec4V(RCC_VEC3V(m_color), ScalarV(V_ONE));
}

// ----------------------------------------------------------------------------------------------- //

inline u32 CLightSource::GetPassKey() const
{
	const u32 bitMask = (
		LIGHTFLAG_NO_SPECULAR | 
		LIGHTFLAG_INTERIOR_ONLY | 
		LIGHTFLAG_EXTERIOR_ONLY | 
		LIGHTFLAG_CAST_SHADOWS | 
		LIGHTFLAG_TEXTURE_PROJECTION |
		LIGHTFLAG_USE_VEHICLE_TWIN);
	return (m_flags & bitMask);
}

// ----------------------------------------------------------------------------------------------- //

inline bool CLightSource::IsPointInLight(const Vector3& point, const float radiusIncrease) const
{
	bool inLight = false;
	const float extend_light = EXTEND_LIGHT(m_type);
	const float extraRadius =  m_radius * extend_light + radiusIncrease; // EXTEND_LIGHT is needed because the actual geometry is faceted, so it is larger than the actual cone/sphere
	const float radius = m_radius + extraRadius;

	// hemispheres lights (which can be less than 90 degrees) currently render with more 
	// of the sphere, so we need to use that bounds or we end up inside the light when 
	// this says we're outside
	if (m_type == LIGHT_TYPE_SPOT && m_params[LS_SPOT_CONE_OUTER_ANGLE_COS] >= 0.01f)  // this should be removed when hemisphere light render optimal volumes
	{
		// first do a basic radius test, since the camera is outside nearly all lights this bails early (plus it handles the end of the cone)
		float sphereTestRadius = radius + m_radius * extend_light; // We're extending radius again? It was already extended above... is this right?
		Vector3 pos = m_pos - m_dir*(m_radius * extend_light); // match the shader vert calc for the quick test
		if (pos.Dist2(point)>sphereTestRadius*sphereTestRadius)
			return false;

		// Now do a real sphere to light ray test.
		// (cone sphere algorithm from http://www.geometrictools.com/Documentation/IntersectionSphereCone.pdf)
		float coneCos = m_params[LS_SPOT_CONE_OUTER_ANGLE_COS];
		float coneSin = coneCos * (m_params[LS_SPOT_CONE_BASE_RADIUS]/m_params[LS_SPOT_CONE_HEIGHT]);

		Vector3 U = m_pos - ( extraRadius / coneSin ) * m_dir;
		Vector3 D = point - U;
		float dsqr = D.Dot(D);
		float e = m_dir.Dot(D);

		if( (e > 0.0f) && (e*e >= dsqr*coneCos*coneCos) )
		{
			D = point - m_pos;
			dsqr = D.Dot(D);
			e = -m_dir.Dot(D);

			if( (e > 0.0f) && (e*e >= dsqr*coneSin*coneSin) )
				return dsqr <= ( extraRadius * extraRadius );
			else
				return true;
		}
		return false;
	}
	else if (m_type == LIGHT_TYPE_CAPSULE && m_params[LS_CAPSULE_EXTENT] > 0.0f)
	{
		const float extent = m_params[LS_CAPSULE_EXTENT] + 2.0f * radius;
		const float extentSqr = extent * extent;

		const Vector3 pt1 = m_pos + (m_dir * (m_params[LS_CAPSULE_EXTENT] * 0.5f + radius));
		const Vector3 pt2 = m_pos - (m_dir * (m_params[LS_CAPSULE_EXTENT] * 0.5f + radius));

		const Vector3 d = pt2 - pt1;
		const Vector3 pd = point - pt1;

		const float pdDotd = pd.Dot(d);
		const float pdDotPd = pd.Dot(pd);
		const float dsq = pdDotPd - (pdDotd * pdDotd / extentSqr);

		inLight = (pdDotd >= 0.0f && pdDotd <= extentSqr && dsq <= radius*radius);
	}
	else
	{
		const Vector3 distVec = point - m_pos;
		const float radius = m_radius + extraRadius;
		inLight = (distVec.Mag2() < radius * radius);
	}

	return inLight;
}

// ----------------------------------------------------------------------------------------------- //

#endif
