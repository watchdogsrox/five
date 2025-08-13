//
// name:		lights.h
// description:	Interface to lighting code
// written by:	Adam Fowler
//

#ifndef INC_LIGHTS_H_
#define INC_LIGHTS_H_ 
  
// --- Include Files ------------------------------------------------------------

// framework
#include "fwscene/world/InteriorLocation.h"
#include "fwscene/stores/mapdatastore.h"

// Rage headers
#include "math/amath.h"
#include "vector/color32.h"
#include "grcore/light.h"
#include "grcore/effect_typedefs.h"

// Game headers
#include "Renderer/drawlists/drawlist.h"
#include "Renderer/RenderThread.h"
#include "Renderer/RenderSettings.h"
#include "Renderer/RenderPhases/RenderPhase.h"
#include "renderer/Lights/LightCommon.h"
#include "Renderer/Lights/LightGroup.h"
#include "renderer/Lights/LightsVisibilitySort.h"
#include "debug/Rendering/DebugDeferred.h"
#include "renderer/Deferred/DeferredConfig.h"
#include "shaders/shaderlib.h"
#include "system/system.h"

namespace rage 
{
	class rmcDrawable;
	class spdAABB;
	class grcTexture;
}

class CEntity;
class CLightEntity;
class CLightSource;
class CLightShaft;


// --- Defines ------------------------------------------------------------------
#define MAX_STORED_LIGHT_SHAFTS				(64)
#define MAX_STORED_BROKEN_LIGHTS			(128)

// this should match the define in game\shader_source\Lighting\Lights\light_common.fxh
#define USE_STENCIL_FOR_INTERIOR_EXTERIOR_CHECK (__PS3)  // xenon can't use interior exterior stencil due to it being cleared by the light stencils

#define LIGHT_STENCIL_READ_MASK_BITS			DEFERRED_MATERIAL_SPAREMASK // the 2 bits we use for marking out the lit pixels
#define LIGHT_CAMOUTSIDE_BACK_PASS_STENCIL_BIT	DEFERRED_MATERIAL_SPAREOR2  // value laid down during the back pass when the camera is not inside the light (i.e. we know there will be a front pass too)
#define LIGHT_FINAL_PASS_STENCIL_BIT			DEFERRED_MATERIAL_SPAREOR1  // the final stencil value for lit pixels. it must be a smaller value than the pass value (when masked with LIGHT_STENCIL_READ_MASK_BITS)
#define LIGHT_ONE_PASS_STENCIL_BIT				DEFERRED_MATERIAL_SPAREOR1	// the single-pass stenciling technique uses this bit to mark lighting 

#define LIGHT_VOLUME_USE_LOW_RES_EDGE_DETECTION (__XENON)

#if __XENON
	#define LIGHT_STENCIL_WRITE_MASK_BITS			0xff						// 360 has to write all bits in order to use hi stencil test
#else
	#define LIGHT_STENCIL_WRITE_MASK_BITS			LIGHT_STENCIL_READ_MASK_BITS
#endif

#define LIGHT_VOLUME_LOW_NEAR_PLANE_FADE (ScalarVConstant<FLOAT_TO_INT(0.05f)>())
#define LIGHT_VOLUME_HIGH_NEAR_PLANE_FADE (ScalarVConstant<FLOAT_TO_INT(5.0f)>())

#if USE_STENCIL_FOR_INTERIOR_EXTERIOR_CHECK
	#define LIGHT_INTERIOR_STENCIL_BIT			DEFERRED_MATERIAL_INTERIOR
#else
	#define LIGHT_INTERIOR_STENCIL_BIT			0
#endif

#define LIGHT_VOLUMES_IN_FOG (__D3D11 || RSG_ORBIS)

#define MAX_CORONA_ONLY_LIGHTS ((__D3D11 || RSG_ORBIS) ? 1280 : 384)

// ----------------------------------------------------------------------------------------------- //

class CVolumeVertexBuffer 
{
public:
	CVolumeVertexBuffer()
		: m_vtxNum(0)
		, m_vtxDcl(NULL)
		, m_vtxBuf(NULL)
	{}


	void GeneratePointSpotVolumeVB(int steps, int stacks);
	void GenerateShaftVolumeVB(int steps, int numPoints);
	int GetNumVerts()								{ return m_vtxNum;}
	grcVertexBuffer* GetVtxBuffer()					{return m_vtxBuf;}
	grcVertexDeclaration* GetVtxDeclaration()		{return m_vtxDcl;}

	void Draw();
	static void ResetLightMeshCache();
	static void BeginVolumeRender();
	static void EndVolumeRender();
private:

	void SetQuantized(Vector3 &dst, const Vector3 & src)
	{
		dst.x = rage::round(src.x*10000.0f)/10000.0f;
		dst.y = rage::round(src.y*10000.0f)/10000.0f;
		dst.z = rage::round(src.z*10000.0f)/10000.0f;
	}
	int PointSpotVolumeNumVerts(int steps, int stacks)
	{
		int numVerts = (steps + 5)*steps;

		if (stacks == 0) // point
		{
			numVerts *= 2;
		}
		else // spot
		{
			numVerts += ((steps*2) + 4)*stacks;
		}

		return numVerts*4;
	}

	int m_vtxNum;
	grcVertexBuffer *m_vtxBuf;
	grcVertexDeclaration *m_vtxDcl;

};

class LightingData
{
public:
	// the three hash arrays are used on SPU so they need to be 16-byte aligned
	atFixedArray<u32, MAX_STORED_LIGHTS> m_lightHashes;

	s16	m_BrokenLightHashesHeadIDX;
	u16	m_BrokenLightHashesCount;
	float m_lodDistScale;
	float m_sphereExpansionMult;

	atFixedArray<u32, MAX_STORED_BROKEN_LIGHTS> m_brokenLightHashes ;

	CLightSource m_directionalLight;	
	
	Vector3 m_pedLightDirection;
	Vector3 m_pedLightColour;
	float m_pedLightIntensity;

	Vec4V m_lightFadeMultipliers;

	atFixedArray<u32, MAX_CORONA_ONLY_LIGHTS> m_coronaLightHashes ;

}  ;


// ----------------------------------------------------------------------------------------------- //

namespace Lights
{
	// Variables
	extern CLightGroup m_lightingGroup;
	extern CLightSource* m_renderLights;
	extern s32 m_numRenderLights;
	extern s32 m_numRenderLightShafts;

	extern CLightShaft* m_lightShaft;
	extern s32 m_numLightShafts;
	extern CLightShaft* m_renderLightShaft;

	extern CLightSource* m_prevSceneShadowLights;
	extern CLightSource* m_sceneLights;

	extern s32 m_numPrevSceneShadowLights;
	extern s32 m_numSceneLights;

	extern LightingData lightingData;
	extern LightingData *m_currFrameInfo;

	extern CLightSource m_blackLight;
	extern DECLARE_MTR_THREAD bool s_UseLightweightTechniques;
	extern DECLARE_MTR_THREAD bool m_useHighQualityTechniques;

	enum LightweightTechniquePassType
	{ kLightPassNormal=0, kLightPassCutOut=1, kLightPassWaterRefractionAlpha = 2, kLightPassMax };

	extern DECLARE_MTR_THREAD LightweightTechniquePassType s_LightweightTechniquePassType;

	extern u32 m_sceneShadows;
	extern u32 m_forceSceneLightFlags;
	extern u32 m_forceSceneShadowTypes;

	extern bool m_bUseSSS;
	extern Vector4 m_SSSParams;
	extern Vector4 m_SSSParams2;

	extern float m_lightOverrideMaxIntensityScale;

	enum {kLightCameraInside=0,kLightCameraOutside=1};
	enum {kLightNoDepthBias=0,kLightDepthBiased=1};
	enum {kLightOnePassStencil=0, kLightTwoPassStencil=1, kLightNoStencil=2};
	enum {kLightDepthBoundsDisabled=0, kLightDepthBoundsEnabled=1};
	enum {kLightStencilAllSurfaces=0, kLightStencilInteriorOnly=1, kLightStencilExteriorOnly=2};
	enum {kLightCullPlaneCamInFront=0, kLightCullPlaneCamInBack=1};
	enum {kLightsActive0=0, kLightsActive4=1, kLightsActive8=2, kLightsActiveMax };

	extern grcBlendStateHandle m_SceneLights_B;
	extern grcDepthStencilStateHandle m_Volume_DS[3][2][2];
	extern grcRasterizerStateHandle m_Volume_R[3][2][2];
	extern grcRasterizerStateHandle m_VolumeSinglePass_R[2][2];
	extern grcRasterizerStateHandle m_VolumeSinglePassSetup_R[2][2];
	extern grcDepthStencilStateHandle m_StencilPEDOnlyGreater_DS;
	extern grcDepthStencilStateHandle m_StencilPEDOnlyLessEqual_DS;

	extern grcBlendStateHandle m_StencilSetup_B;
	extern grcDepthStencilStateHandle m_StencilFrontSetup_DS[2];
#if USE_STENCIL_FOR_INTERIOR_EXTERIOR_CHECK
	extern grcDepthStencilStateHandle m_StencilBackSetup_DS[3];
#else
	extern grcDepthStencilStateHandle m_StencilBackSetup_DS[1];
#endif
	extern grcDepthStencilStateHandle m_StencilDirectSetup_DS;
	extern grcDepthStencilStateHandle m_StencilBackDirectSetup_DS[2];
	extern grcDepthStencilStateHandle m_StencilFrontDirectSetup_DS[2];
	extern grcDepthStencilStateHandle m_StencilCullPlaneSetup_DS[2][2];
	extern grcDepthStencilStateHandle m_StencilSinglePassSetup_DS[2];
	
	extern grcBlendStateHandle m_AmbientScaleVolumes_B;
	extern grcDepthStencilStateHandle m_AmbientScaleVolumes_DS;
	extern grcRasterizerStateHandle m_AmbientScaleVolumes_R;

	extern grcBlendStateHandle m_VolumeFX_B;
	extern grcDepthStencilStateHandle m_VolumeFX_DS;
	extern grcRasterizerStateHandle m_VolumeFX_R;
	extern grcRasterizerStateHandle m_VolumeFXInside_R;

	extern grcDepthStencilStateHandle m_pedLight_DS;
	extern Vector3 m_pedLightDirection;
	extern float  m_pedLightDayFadeStart;
	extern float  m_pedLightDayFadeEnd;
	extern float  m_pedLightNightFadeStart;
	extern float  m_pedLightNightFadeEnd;
	extern bool	  m_fadeUpPedLight;
	extern u32	  m_fadeUpStartTime;
	extern float  m_fadeUpDuration;

	extern float m_lightFadeDistance;
	extern float m_shadowFadeDistance;
	extern float m_specularFadeDistance;
	extern float m_volumetricFadeDistance;
	extern Vec4V m_defaultFadeDistances;
	extern float m_lightCullDistanceForNonGBufLights;
	extern float m_mapLightFadeDistance;

	extern float m_endDynamicBakeMin;
	extern float m_endDynamicBakeMax;

	extern float m_startDynamicBakeMin;
	extern float m_startDynamicBakeMax;

	extern bool m_bRenderShadowedLightsWithNoShadow;

	// Functions
	void Init();
	void InitDLCCommands();
	void Shutdown();
	void Update();
	void PreSceneUpdate();
	void ClearCutsceneLightsFromPrevious();
	void UpdateBaseLights();
	void AddToDrawList();
	void SetupForceSceneLightFlags(u32 sceneShadows);
	void ProcessVisibilitySortAsync();
	void WaitForProcessVisibilityDependency();

	void SetupRenderThreadVisibleLightLists(bool cameraCut);

	inline void Synchronise() { m_lightingGroup.Synchronise(); }

	// Deferred Rendering.
	bool HasSceneLights();
	bool AnyVolumeLightsPresent();
	void AddToDrawListSceneLights(bool bRenderShadows = true);
	void RenderSceneLights(bool shadows, const bool BANK_ONLY(drawDebugCost));

	void AddToDrawListPedSkinPass(const Vector4& s2dbb);

	void AddToDrawlistVolumetricLightingFx(bool bEnableShadows);
	void RenderVolumetricLightingFx(bool bEnableShadows);

	void RenderAllVolumeLightShafts(bool bEnableShadows, const grcTexture* pDepthBuffer);
	void RenderAmbientScaleVolumes();

	spdAABB CalculatePartialDrawableBound( const CEntity& entity, int bucketMask,int lod );
	int CalculatePartialDrawableBound( spdAABB& box, const rmcDrawable* pDrawable, int bucketMask,int lod );

	void RenderSplitDrawable( const rmcDrawable* drawable, const Matrix34& mtx, int bucket, fwInteriorLocation intLoc, int lod );

	void SetupRenderThreadLights();
	void SetupDirectionalGlobals(float directionalScale);
	void SetupDirectionalAndAmbientGlobals();
	void SetupDirectionalAndAmbientGlobalsTweak(float directionalScale, float artificialIntAmbientScale, float artificialExtAmbientScale);

	void UseLightsInArea(const spdAABB& box, const bool inInterior, const bool forceSetAll8Lights, const bool reflectionPhase, const bool respectHighPriority = false);

#if DEPTH_BOUNDS_SUPPORT
		bool SetupDepthBounds(const CLightSource *lightSource);
		void DisableDepthBounds();
#endif //DEPTH_BOUNDS_SUPPORT

	__forceinline void ResetLightsUsed() { m_lightingGroup.SetActiveCount(0); }
	void ResetSceneLights();
	void DrawDefLight(
		const CLightSource *light, 
		const eDeferredShaders deferredShader,
		const grcEffectTechnique technique, 
		const s32 pass, 
		bool useHighResMesh, 
		bool isShadowCaster,
		MultisampleMode aaMode);
	
	void RegisterBrokenLight(const CEntity *pEntity, s32 lightID);
	void RegisterBrokenLight(u32 hash);
	void RemoveRegisteredBrokenLight(u32 hash);

#if GTA_REPLAY
	bool IsLightBroken(u32 hash);
#endif


	bool AddSceneLight(CLightSource& sceneLight, const CLightEntity *lightEntity = NULL, bool addToPreviouslights=false);

	CLightShaft* AddLightShaft(BANK_ONLY(CLightEntity* entity));

	inline void ResetLightingGlobals(void) { Lights::m_lightingGroup.ResetLightingGlobals();}
	
	inline void SetFakeLighting(float ambientScalar, float directionalScalar, const Matrix34 &camMtx) { Lights::m_lightingGroup.SetFakeLighting(ambientScalar, directionalScalar, camMtx);}

	inline void RenderShadowedLightsWithNoShadow(const bool show) { m_bRenderShadowedLightsWithNoShadow = show; }  

	void SetupRenderthreadFrameInfo();
	void ClearRenderthreadFrameInfo();

	inline const CLightSource&		GetRenderDirectionalLight()	{ Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER)); return m_currFrameInfo->m_directionalLight; }
	inline const CLightSource&		GetUpdateDirectionalLight()	{ Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));  return lightingData.m_directionalLight; }
	inline void						DisableDirectionalLightFromVisibilityScanning()	{ Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));  lightingData.m_directionalLight.ClearFlag(LIGHTFLAG_CAST_SHADOWS); }

	inline atFixedArray<u32, MAX_STORED_LIGHTS>& GetRenderLightHashes() { Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER)); return m_currFrameInfo->m_lightHashes; }
	inline atFixedArray<u32, MAX_STORED_LIGHTS>& GetUpdateLightHashes() { Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE)); return lightingData.m_lightHashes; }
	
	inline LightingData &GetRenderLightingData() { return *m_currFrameInfo; }
	inline LightingData &GetUpdateLightingData() { return lightingData; }

	inline void  SetUpdateLodDistScale(float distScale) { Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE)); lightingData.m_lodDistScale = distScale; }
	inline float GetUpdateLodDistScale() { Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE)); return lightingData.m_lodDistScale; }
	inline float GetRenderLodDistScale() { Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER)); return m_currFrameInfo->m_lodDistScale; }

	inline void  SetUpdateSphereExpansionMult(float mult) { Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE)); lightingData.m_sphereExpansionMult = mult; }
	inline float GetUpdateSphereExpansionMult() { Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE)); return lightingData.m_sphereExpansionMult; }
	inline float GetRenderSphereExpansionMult() { Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER)); return m_currFrameInfo->m_sphereExpansionMult; }

	inline Vec4V GetUpdateLightFadeMultipliers() { Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE)); return lightingData.m_lightFadeMultipliers; } 
	inline Vec4V GetRenderLightFadeMultipliers() { Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER)); return m_currFrameInfo->m_lightFadeMultipliers; } 

	Vector4 CalculateLightConsts(eLightType type, float radius, const bool useHighResMesh);
	void CalculateLightParams(const CLightSource *light, Vector4 *lightParams);

	void BeginLightRender();
	void ResetLightMeshCache();
	void EndLightRender();
	void RenderVolumeLight(int type, bool useHighResMesh, bool isVolumeFXLight=false, bool isShadowCaster=false);
	void RenderVolumeLightShaft(bool useHighResMesh);

	grcVertexBuffer* GetLightMesh(u32 index);

	void RenderPedLight();
	void StartPedLightFadeUp(const float seconds);
	void ResetPedLight();

	void SetupDepthForVolumetricLights();
	void InitOffscreenVolumeLightsRender();
	void ShutDownOffscreenVolumeLightsRender(const grcRenderTarget *pFullResColorTarget, const grcRenderTarget* pFullResDepthTarget);

	void SetHighQualityTechniques(bool bUseHighQuality);
	void SetUseSceneShadows(u32 sceneShadows);
	
	u32 GetNumSceneLights();

	void BeginLightweightTechniques();
	void SelectLightweightTechniques(s32 activeCount, bool bUseHighQuality = false);
	void SetLightweightTechniquePassType(LightweightTechniquePassType passType);
	LightweightTechniquePassType GetLightweightTechniquePassType();
	void EndLightweightTechniques();

	void GetBound( const CLightSource& lgt, spdAABB& bound, float minExtraRadius = 0.0f, eDeferredShadowType shadowType=DEFERRED_SHADOWTYPE_NONE);
	float CalculateTimeFade(const u32 timeFlags);
	float CalculateTimeFade(float fadeInTimeStart, float fadeInTimeEnd, float fadeOutTimeStart, float fadeOutTimeEnd);

	float GetDiffuserIntensity(const Vector3 &lightDir);
	float GetReflectorIntensity(const Vector3 &lightDir);
	void CalculateFromAmbient(CLightSource &light);

	inline float GetLightFadeDistance() { return m_lightFadeDistance; }
	inline float GetShadowFadeDistance() { return m_shadowFadeDistance; }
	inline float GetSpecularFadeDistance() { return m_specularFadeDistance; }
	inline float GetVolumetricFadeDistance() { return m_volumetricFadeDistance; }
	inline float GetNonGBufLightCutoffDistance() { return m_lightCullDistanceForNonGBufLights; }

	void SetMapLightFadeDistance(float mix);
	void CalculateFadeValues(const CLightSource *light, Vec3V_In cameraPosition, Vec4V_InOut out_fadeValues);

	void CalculateDynamicBakeBoostAndWetness();

	void UpdateVisualDataSettings();

	enum
	{
		LIGHT_CATEGORY_NONE = -1,
		LIGHT_CATEGORY_SMALL,
		LIGHT_CATEGORY_MEDIUM,
		LIGHT_CATEGORY_LARGE,
		LIGHT_CATEGORY_MAX
	};

	int	CalculateLightCategory(u8 lightType, float falloff, float intensity, float capsuleExtentX, u32 lightAttrFlags);
	float CalculateAdjustedLightFade(
		const float lightFade, 
		const CLightEntity *lightEntity, 
		const CLightSource &lightSource, 
		const bool useParentAlpha);

	// Light stencil setup and use
	inline int GetLightStencilPasses() { return DeferredLighting__m_useLightsHiStencil ? (DeferredLighting__m_useSinglePassStencil ? 1 : 2) : 0; }
	void SetupLightStencil(bool bCameraInside, int nStencilPass, int nInteriorExteriorCheck=kLightStencilAllSurfaces, bool bUseDepthBias=false);
	void DrawLightStencil(const CLightSource *light, const eDeferredShaders deferredShader, const grcEffectTechnique technique, const s32 shaderPass, bool useHighResMesh, bool cameraInside, int stencilPass, int interiorExteriorCheck, bool needsDepthBias);
	void DrawCullPlaneStencil(const CLightSource *light, const eDeferredShaders deferredShader, const grcEffectTechnique technique);
	void UseLightStencil( bool bUseStencilTest, bool bCameraInside, bool bUseDepthBias, bool bUseDepthBounds, bool bIsPedLight, EdgeMode edgeMode);

	// General volume stencil setup and use
	void SetupVolumeStencil( bool cameraInside, int stencilPass );
	void UseVolumeStencil();
	void SetupDirectStencil();
	
	// Sets up light override dlc commands.
	void ApplyLightOveride(const CEntity *entity);
	void UnApplyLightOveride(const CEntity *entity);

	void SetLightOverrideMaxIntensityScale(float maxIntensityScale);
	float GetLightOverrideMaxIntensityScale();

#if __BANK
	void AddWidgets(bkBank &bank);
#endif
};

__forceinline void Lights::AddToDrawlistVolumetricLightingFx(bool bEnableShadows)
{
	Lights::SetupRenderThreadLights();
	DLC_Add(RenderVolumetricLightingFx, bEnableShadows);
}

__forceinline void Lights::AddToDrawListSceneLights(bool bRenderShadows)
{
	Lights::SetupRenderThreadLights();
	DLC_Add(RenderSceneLights, bRenderShadows, false);
}

// --- Globals ------------------------------------------------------------------

#endif // !INC_LIGHTS_H_
