#ifndef DEBUG_LIGHTING_H_
#define DEBUG_LIGHTING_H_

// =============================================================================================== //
// INCLUDES
// =============================================================================================== //

// rage
#include "atl/array.h"
#include "fwscene/stores/boxstreamerassets.h"
#include "fwscene/stores/boxstreamersearch.h"
#include "vector/vector2.h"
#include "grcore/effect.h"

// game
#include "debug/Rendering/DebugRendering.h"
#include "debug/Rendering/DebugDeferred.h"
#include "renderer/Lights/LODLights.h"

#if __BANK

enum eForceVolumeAttenuationType
{
	VOLUME_ATTENUATION_DISABLE_FORCED = 0,
	VOLUME_ATTENUATION_FORCE_VERTEX,
	VOLUME_ATTENUATION_FORCE_PIXEL,
	VOLUME_ATTENUATION_NUM_FORCE_TECHNIQUES
};

enum eAmbientOverrideTypes
{
	AMBIENT_OVERRIDE_NATURAL = 0,
	AMBIENT_OVERRIDE_ARTIFICIAL_EXTERIOR,
	AMBIENT_OVERRIDE_ARTIFICLAL_INTERIOR,
	AMBIENT_OVERRIDE_DIRECTIONAL,
	AMBIENT_OVERRIDE_COUNT
};

enum eParticleOverrideTypes
{
	PARTICLE_OVERRIDE_DIRECTIONAL_MULT = 0,
	PARTICLE_OVERRIDE_AMBIENT_MULT,
	PARTICLE_OVERRIDE_EXTRA_LIGHT_MULT,
	PARTICLE_OVERRIDE_SHADOW_AMOUNT,
	PARTICLE_OVERRIDE_NORMAL_ARC,
	PARTICLE_OVERRIDE_SOFTNESS_CURVE,
	PARTICLE_OVERRIDE_SOFTNESS_SHADOW_MULT,
	PARTICLE_OVERRIDE_SOFTNESS_SHADOW_OFFSET,
	PARTICLE_OVERRIDE_COUNT
};

namespace rage
{
	class ptxShaderVar;
	typedef atArray< datOwner<ptxShaderVar> > ptxInstVars;
}

// =============================================================================================== //
// CLASS
// =============================================================================================== //

class DebugLighting : public DebugRendering
{
public:

	// Functions
	static void Init();
	static void Draw();
	static void Update();
	static void AddWidgets();

	static bool ShouldProcess(CLightSource &light);

	static float  GetDeferredDiffuseLightAmount() { return m_deferredDiffuseLightAmount; }
	static float& GetDeferredDiffuseLightAmountRef() { return m_deferredDiffuseLightAmount; }

	// Light entity debug
	static void UpdateLightEntityIndex();

	// LOD Light debug
	static void RenderLODLights();
	static void UpdateLODLights();
	static void UpdateStatsLODLights(const u32 bucketCount, const s32 lightsChanged, const u32 bucket);
	static void BucketRangeLODLights(u32 &startIndex, u32 &endIndex);
	static void BucketRangeDistantLODLights(u32 &startIndex, u32 &endIndex);
	static void OverrideLODLight(CLightSource &lightSource, u32 bucketIndex, eLodLightCategory category);
	static void LightRangeLODLights(u32 &startIndex, u32 &endIndex, const u32 totalCount);
	static void LightRangeDistantLODLights(u32 &startIndex, u32 &endIndex, const u32 totalCount);

	static void AddSearches(atArray<fwBoxStreamerSearch>& searchList, fwBoxStreamerAssetFlags supportedAssetFlags);
	
	static void EnableExtraDistantLights(Vec3V_In vPos, float fRadius)
	{
		m_bLoadExtraDistantLights=true;
		m_vExtraDistantLightsCentre = vPos;
		m_fExtraDistantLightsRadius = fRadius;
	}
	static void DisableExtraDistantLights() { m_bLoadExtraDistantLights=false; }

	static bool ShouldOverrideLightValue();
	static void OverrideAmbient(Vector4* amb, eAmbientOverrideTypes ambientType);
	static void OverrideDeferredLightParams(Vector4* colourAndIntensity, const CLightSource* light);
	static void OverrideForwardLightParams(Vec4V* params);
	static void OverrideDirectional(Vector4* colour);

	static void OverridePedPhoneLight(CLightSource* pedPhoneLight);

	static bool OverrideDirectional() { return m_directionalOverride; }
	static Vector3 GetDirectionalOverrideColour() { return m_directionalLightColour; }
	static float GetDirectionalOverrideIntensity() { return m_directionalLightIntensity; }
	static void OverrideParticleShaderVars(ptxInstVars* instVars);

	static void DumpParentImaps(const bool allowMultipleCalls);
	static void DumpLightEntityPoolStats();

	// Overrides
	static bool m_overrideDeferred;
	static bool m_overrideForward;

	// Deferrd
	static bool	m_drawLightShafts;
	static bool	m_drawLightShaftsAlways; // don't hide when the camera is on the "wrong" side of the quad
	static bool	m_drawLightShaftsNormal; // force light shafts to draw directly in their normal direction
	static bool	m_drawLightShaftsOffsetEnabled;
	static Vec3V m_drawLightShaftsOffset;
	static bool	m_drawSceneLightVolumes;
	static bool m_drawSceneLightVolumesAlways; // force light volumes even if conditional test says there are none
	static bool m_usePreAlphaDepthForVolumetricLight;
	static bool	m_drawAmbientVolumes;
	static bool m_drawAmbientOcclusion;

	static bool m_LightShaftNearClipEnabled;
	static float m_LightShaftNearClipExponent;

#if DEPTH_BOUNDS_SUPPORT
	static bool	m_useLightsDepthBounds;
#endif // DEPTH_BOUNDS_SUPPORT
	static Vector4 m_LightShaftParams;
	static ScalarV m_VolumeLowNearPlaneFade;
	static ScalarV m_VolumeHighNearPlaneFade;
	static bool m_testSphereAmbVol;

	static bool m_FogLightVolumesEnabled;

	static bool m_LODLightRender;
	static bool m_distantLODLightRender;
	static bool m_LODLightsEnableOcclusionTest;
	static bool m_LODLightsEnableOcclusionTestInMainThread;
	static bool m_LODLightsVisibilityOnMainThread;
	static bool m_LODLightsProcessVisibilityAsync;
	static bool m_LODLightsPerformSortDuringVisibility;
	static bool m_DistantLightsProcessVisibilityAsync;

	// LOD light debug
	static bool m_showLODLightMemUsage;
	static bool m_showLODLightUsage;
	static bool m_showLODLightHashes;
	static bool m_showLODLightTypes;

	static bool m_LODLightDebugRender;
	static bool m_LODLightPhysicalBoundsRender;
	static bool m_LODLightStreamingBoundsRender;
	static s32  m_LODLightBucketIndex;
	static s32  m_LODLightIndex;
	static char m_LODLightsString[255];
	static bool	m_LODLightCoronaEnable;
	static bool m_LODLightCoronaEnableWaterReflectionCheck;
	static u32	m_LODLightCategory;
	static Vector3 m_LODLightDebugColour[];
	static bool m_LODLightOverrideColours;

	static bool m_distantLODLightDebugRender;
	static bool m_distantLODLightPhysicalBoundsRender;
	static bool m_distantLODLightStreamingBoundsRender;
	static s32  m_distantLODLightBucketIndex;
	static s32  m_distantLODLightIndex;
	static char m_distantLODLightsString[255];
	static s32	m_distantLODLightCategory;
	
	static bool m_distantLODLightsOverrideRange;
	static float m_distantLODLightsStart;
	static float m_distantLODLightsEnd;

	static u32 m_LODLightsBucketsUsed[2];
	static u32 m_peakLODLightsBucketsUsed[2];
	
	static u32 m_LODLightsOnMap[2];
	static u32 m_peakLODLightsOnMap[2];
	
	static u32 m_LODLightsStorage[2];
	static u32 m_peakLODLightsStorage[2];
	
	static u32 m_LODLightsProcessed[2];
	
	static u32 m_lightSize[2];
	static bool m_ForceUnshadowedVolumeLightTechnique;
	static float m_MinSpotAngleForBackFaceForce;

#if LIGHT_VOLUME_USE_LOW_RESOLUTION
	static bool  m_VolumeLightingMaskedUpsample;
#endif

	// Use this to force the lightweight shaders with capsule light support
	static bool m_bForceLightweightCapsule;
	static bool m_bForceHighQualityForward;

private:
	static void LoadLODLightCache();
	static void UnloadLODLightCache();
	static void LoadSelectedLODLightImapGroup();
	static void UnloadSelectedLODLightImapGroup();
	static void LoadLODLightImapGroupList();
	static void UpdateParticleOverride();
	static void UpdateParticleOverrideAll();

	// Variables
	static bool m_directionalEnable;
	static bool m_directionalOverride;
	static Vector3 m_directionalLightColour;
	static float m_directionalLightIntensity;
	
	static bool	m_ambientEnable[AMBIENT_OVERRIDE_COUNT];
	static bool	m_ambientOverride[AMBIENT_OVERRIDE_COUNT];
	static Vector3 m_baseAmbientColour[AMBIENT_OVERRIDE_COUNT];
	static float m_baseAmbientIntensity[AMBIENT_OVERRIDE_COUNT];
	static Vector3 m_downAmbientColour[AMBIENT_OVERRIDE_COUNT];
	static float m_downAmbientIntensity[AMBIENT_OVERRIDE_COUNT];
	
	static bool m_particleOverride[PARTICLE_OVERRIDE_COUNT];
	static float m_particleOverrideAmountFloat[PARTICLE_OVERRIDE_COUNT];
	static atHashValue m_particleVarHashes[PARTICLE_OVERRIDE_COUNT];
	static u32 m_particleOverrideType[PARTICLE_OVERRIDE_COUNT];
	static bool m_particleOverridesEnable;
	static bool m_particleOverrideAll;

	static bool	m_lightsEnable;
	static bool	m_lightsOverride;
	static Vector3 m_lightsColour;
	static float m_lightsIntensity;
	static bool m_lightsExterior;
	static bool m_lightsInterior;
	static bool m_lightsShadowCasting;
	static bool m_lightsSpot;
	static bool m_lightsPoint;
	static u32	m_lightFlagsToShow;

	// advanced
	static float m_deferredDiffuseLightAmount;

	// Light entity debug
	static bool m_lightEntityDebug;
	static s32 m_lightEntityIndex;
	static bkSlider* m_lightEntityIndexSlider;

	// extra distant light streaming
	static bool m_bLoadExtraDistantLights;
	static bool m_bLoadExtraTrackWithCamera;
	static bool m_bDisplayStreamSphere;
	static Vec3V m_vExtraDistantLightsCentre;
	static float m_fExtraDistantLightsRadius;

	static bool m_dumpedParentImaps;

	//phone light settings
	static bool m_bPedPhoneLightOverride;
	static bool m_bPedPhoneLightDirDebug;
	static Vector3 m_pedPhoneLightColor;
	static float m_pedPhoneLightIntensity;
	static float m_pedPhoneLightRadius;
	static float m_pedPhoneLightFalloffExponent;
	static float m_pedPhoneLightConeInner;
	static float m_pedPhoneLightConeOuter;

	// Map data
	static atArray<s32> ms_lodLightImapGroupIndices;
	static atArray<const char*> ms_lodLightImapGroupNames;
	static s32 ms_selectedImapComboIndex;
	static bool m_LODLightOwnedByMapData;
	static bool m_LODLightOwnedByScript;

};

#endif // __BANK

#endif
