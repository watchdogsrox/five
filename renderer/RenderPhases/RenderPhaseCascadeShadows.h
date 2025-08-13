// =================================================
// renderer/renderphases/renderphasecascadeshadows.h
// (c) 2011 RockstarNorth
// =================================================

#ifndef _RENDERER_RENDERPHASES_RENDERPHASECASCADESHADOWS_H_
#define _RENDERER_RENDERPHASES_RENDERPHASECASCADESHADOWS_H_

#include "renderer/renderphases/renderphase.h"
#include "../../shader_source/Lighting/Shadows/cascadeshadows_common.fxh" // for CASCADE_SHADOWS_DO_SOFT_FILTERING
#include "rmptfx/ptxconfig.h"

namespace rage { class grmShader; }
namespace rage { class grcRenderTarget; }
namespace rage { class fwScanCascadeShadowInfo; }

class CViewport;
class CEntity;

enum eCSMShadowType
{
	CSM_TYPE_CASCADES, // 4 cascades
	CSM_TYPE_QUADRANT,
	CSM_TYPE_HIGHRES_SINGLE,
	CSM_TYPE_COUNT
};

enum eCSMQualityMode
{
	CSM_QM_IN_GAME          = 0,
	CSM_QM_CUTSCENE_DEFAULT = 1,
	CSM_QM_MOONLIGHT        = 2,
	CSM_QM_COUNT
};

enum eCSMRasterizerState
{
	CSM_RS_DEFAULT    = 0,
	CSM_RS_TWO_SIDED  = 1,
	CSM_RS_CULL_FRONT = 2,
	CSM_RS_COUNT
};

enum ShadowMapMiniType
{
	SM_MINI_UNDERWATER = 0,
	SM_MINI_FOGRAY,
	SM_MINI_LAST
};

#define ALLOW_SHADOW_DISTANCE_SETTING (RSG_PC)
#define ALLOW_LONG_SHADOWS_SETTING (RSG_PC)

#define SHADOW_MATRIX_REFLECT (0 && __DEV) // experimental

#define SHADOW_ENABLE_ASYNC_CLEAR   (1 && RSG_DURANGO)

#if SHADOW_MATRIX_REFLECT
#define SHADOW_MATRIX_REFLECT_ONLY(...) __VA_ARGS__
#else
#define SHADOW_MATRIX_REFLECT_ONLY(...)
#endif

#define SHADOW_ENABLE_ALPHASHADOW		( RSG_PC || RSG_DURANGO || RSG_ORBIS )

class CRenderPhaseCascadeShadowsInterface
{
public:
	static void Init_(unsigned initMode);
	static void InitShaders();

#if RSG_PC
	static void ResetShaders();

	static void DeviceLost();
	static void DeviceReset();
#endif // RSG_PC
	static void ResetRenderThreadInfo();

	static int CascadeShadowResX();
	static int CascadeShadowResY();

	static void CreateShadowMapMini();

	static grcRenderTarget* GetShadowMap(); // used for RMPTFX_USE_PARTICLE_SHADOWS
	static grcRenderTarget* GetShadowMapVS(); // used for shaded particles

#if RMPTFX_USE_PARTICLE_SHADOWS || SHADOW_ENABLE_ALPHASHADOW
	static bool				UseAlphaShadows();	// Disable alpha shadows (particles + cables)
	static grcRenderTarget* GetAlphaShadowMap();
#if __ASSERT
static void		SetParticleShadowSetFlagThreadID(int renderThreadID);
static int		GetParticleShadowSetFlagThreadID();
static void		SetParticleShadowAccessFlagThreadID(int renderThreadID);
static int		GetParticleShadowAccessFlagThreadID();
static void		ResetParticleShadowFlagThreadIDs();
#endif
#endif // RMPTFX_USE_PARTICLE_SHADOWS

#if __D3D11 || RSG_ORBIS
	static grcRenderTarget*	GetShadowMapReadOnly();
#endif // __D3D11

#if __PS3
	static grcRenderTarget* GetShadowMapPatched();
#endif // __PS3
	static grcRenderTarget* GetShadowMapMini(ShadowMapMiniType mapType = SM_MINI_UNDERWATER);
	static const grcTexture* GetSmoothStepTexture();
	static grcTexture* GetDitherTexture();

	static void CopyShadowsToMiniMap(grcTexture* noiseMap, float height);
	static void CopyShadowsToShadowMapVS();

	static int GetShadowTechniqueId();
	static int GetShadowTechniqueTextureId();
#if !__PS3
	static grmShader* GetShadowShader();
#endif // !__PS3

#if __BANK
	static void InitWidgets();
#if CASCADE_SHADOWS_DO_SOFT_FILTERING
	static void InitWidgets_SoftShadows(bkBank &bk);
#endif // CASCADE_SHADOWS_DO_SOFT_FILTERING
	static bool IsEntitySelectRequired();
#endif // __BANK

	static void Terminate() {}
	static void Update();
	static void DisableFromVisibilityScanning();
	static void ShadowReveal();
	static void LateShadowReveal();
	static void Synchronise();

#if SHADOW_ENABLE_ASYNC_CLEAR
	static void ClearShadowMapsAsync();
	static void WaitOnClearShadowMapsAsync();
#endif // SHADOW_ENABLE_ASYNC_CLEAR

#if GS_INSTANCED_SHADOWS
	static void SetInstShadowActive(bool bActive);
	static bool IsInstShadowEnabled();
#endif // GS_INSTANCED_SHADOWS

	static bool IsShadowActive();
	static bool ArePortalShadowsActive();

	static void SetDeferredLightingShaderVars();
	static void SetSharedShaderVars();
	static void SetCloudShadowParams();
#if SHADOW_MATRIX_REFLECT
	static void SetSharedShaderVarsShadowMatrixOnly(Vec4V_In reflectionPlane = Vec4V(V_ZERO));
#endif // SHADOW_MATRIX_REFLECT
	static void SetSharedShadowMap(const grcTexture* tex); // can be used with forward shaders to override the shadow map texture
	static void RestoreSharedShadowMap();

	static fwScanCascadeShadowInfo& GetScanCascadeShadowInfo();

	static void GetCascadeViewports(atArray<grcViewport>& cascadeViewports_InOut);
	static const grcViewport& GetViewport(); //main cascade shadow viewport (range encompasses all cascades, used for visibility and occlusion)
	static const grcViewport& GetCascadeViewport(int cascadeIndex);

	static bool IsUsingStandardCascades(); // if this returns false, we're doing something funky with cascades (e.g. quadrant track, cutscene etc.)

	static float GetCutoutAlphaRef();
	static bool GetCutoutAlphaToCoverage();
	static bool GetRenderFadingEnabled();
	static bool GetRenderHairEnabled();

	static float GetMinimumRecieverShadowHeight();

	static void SetRasterizerState(eCSMRasterizerState state);
	static void RestoreRasterizerState();

	static void BeginFindingNearZShadows();
	static float EndFindingNearZShadows();

#if __BANK
	static void DebugDraw_update();
	static void DebugDraw_render();
	static bool IsDebugDrawEnabled();
#endif // __BANK

	static void UpdateVisualDataSettings();
	static void UpdateTimecycleSettings();

	static void Script_InitSession();
	static void Script_SetShadowType(eCSMShadowType type);
	static void Script_SetCascadeBounds(int cascadeIndex, bool bEnabled, float x, float y, float z, float radiusScale, bool lerpToDisabled = false, float lerpTime = 0.0f);
	static void Script_GetCascadeBounds(int cascadeIndex, bool &bEnabled, float &x, float &y, float &z, float &radiusScale, bool &lerpToDisabled, float &lerpTime);
	static void Script_SetCascadeBoundsHFOV(float degrees);
	static void Script_SetCascadeBoundsVFOV(float degrees);
	static void Script_SetCascadeBoundsScale(float scale);
	static void Script_SetEntityTrackerScale(float scale);
	static void Script_SetWorldHeightUpdate(bool bEnable);
	static void Script_SetWorldHeightMinMax(float h0, float h1);
	static void Script_SetRecvrHeightUpdate(bool bEnable);
	static void Script_SetRecvrHeightMinMax(float h0, float h1);
	static void Script_SetDitherRadiusScale(float scale);
	static void Script_SetDepthBias(bool bEnable, float depthBias);
	static void Script_SetSlopeBias(bool bEnable, float slopeBias);
	static void Script_SetShadowSampleType(const char* typeStr);
	static void Script_SetShadowQualityMode(eCSMQualityMode qm);
	static void Script_SetAircraftMode(bool on);
	static void Script_SetDynamicDepthMode(bool on);
	static void Script_EnableDynamicDepthModeInCutscenes(bool on);
	static void Script_SetFlyCameraMode(bool on); // for scripted fly camera
	static void Script_SetSplitZExpWeight(float v); // for scripted fly camera
	static void Script_SetBoundPosition(float v);

	static void Cutscene_SetShadowSampleType( u32 hashVal);
	static bool IsSampleFilterSyncable();
	static void SetDynamicDepthValue( float v);
	static void SetEntityTrackerActive(bool bActive);

	static void SetSoftShadowProperties();
	static void SetParticleShadowsEnabled(bool bEnabled);

	static void SetScreenSizeCheckEnabled(bool bEnabled);

	static void SetSyncFilterEnabled(bool bEnabled);

	static u32 GetClipPlaneCount();
	static Vec4V_Out GetClipPlane(u32 index);
	static const Vec4V *GetClipPlaneArray();	//Useful for setting shader vars.

	//Quality Settings
	static void SetGrassEnabled(bool enabled);

#if GTA_REPLAY
	static bool IsUsingAircraftShadows();
#endif
	
#if ALLOW_SHADOW_DISTANCE_SETTING
	static void SetDistanceMultiplierSetting(float distanceMultiplier, float splitZStart, float splitZEnd, float aircraftExpWeight);
#endif // ALLOW_SHADOW_DISTANCE_SETTING
#if ALLOW_LONG_SHADOWS_SETTING
	static void SetShadowDirectionClamp(bool bEnabled);
#endif // ALLOW_LONG_SHADOW_SETTING
};

class CRenderPhaseCascadeShadows : public CRenderPhaseScanned
{
public:
	CRenderPhaseCascadeShadows(CViewport* pGameViewport);
	virtual ~CRenderPhaseCascadeShadows() {}

	virtual void UpdateViewport();
	virtual void BuildRenderList();
	virtual void BuildDrawList();
	void		 BuildDrawListAlphaShadows();
	virtual u32  GetCullFlags() const;

	void         ShadowDrawListPrologue(u32 drawListType);
	void         ShadowDrawListEpilogue(u32 drawListType);

	static void	 ComputeFograyFadeRange(float &fadeStart, float &fadeEnd);

	static void SetCurrentExecutingCascade(int cascade)	{ FastAssert(sysThreadType::IsRenderThread()); s_CurrentExecutingCascade = cascade; }
	static void SetCurrentBuildingCascade(int cascade)	{ FastAssert(sysThreadType::IsUpdateThread()); s_CurrentBuildingCascade = cascade; }
	static int GetCurrentExecutingCascade()				{ FastAssert(sysThreadType::IsRenderThread()); return s_CurrentExecutingCascade; }
	static int GetCurrentBuildingCascade()				{ FastAssert(sysThreadType::IsUpdateThread()); return s_CurrentBuildingCascade; }

	static void UpdateRasterUpdateId();

#if RSG_PC
	static bool IsNeedResolve();
#endif

private:
	static __THREAD int s_CurrentExecutingCascade;
	static int s_CurrentBuildingCascade;
};

#endif // _RENDERER_RENDERPHASES_RENDERPHASECASCADESHADOWS_H_
