#ifndef DEBUG_DEFERRED_H_
#define DEBUG_DEFERRED_H_

// =============================================================================================== //
// INCLUDES
// =============================================================================================== //

// rage 
#include "vector/vector4.h"
#include "grcore/effect.h"

// framework
#include "fwtl/pool.h"

// game
#include "Renderer/Deferred/DeferredConfig.h"
#include "Debug/Rendering/DebugRendering.h"
#include "renderer/Lights/TiledLighting.h"

// =============================================================================================== //
// DEFINES
// =============================================================================================== //

namespace rage
{
	class bkBank;
	class grmShader;
}

class DebugRendering;

// ----------------------------------------------------------------------------------------------- //

enum eGBufferOverride
{
	OVERRIDE_DIFFUSE = 0,
	OVERRIDE_EMISSIVE,
	OVERRIDE_SHADOW,
	OVERRIDE_AMBIENT,
	OVERRIDE_SPECULAR,
};

// ----------------------------------------------------------------------------------------------- //

enum eReloadElfs
{
	RELOAD_NONE = 0,
	RELOAD_CLASSIFICATION,
	RELOAD_SSAOSPU
};

// ----------------------------------------------------------------------------------------------- //

#if __BANK

// =============================================================================================== //
// CLASS
// =============================================================================================== //

class DebugDeferred : public DebugRendering
{
public:

	// Functions
	static void AddWidgets(bkBank *bk);
	static void AddWidgets_GBuffer(bkBank *bk);
#if TILED_LIGHTING_ENABLED
	static void AddWidgets_LightClassification(bkBank *bk);
#endif
#if __PPU
	static void AddWidgets_SpuPmElfReload(bkBank *bk);
#endif // __PPU
	static void OverrideGBufferTarget(eGBufferOverride type, const Vector4 &params, float amount);
	static void ShowGBuffer(GBufferRT index);
	static void DesaturateAlbedo(float amount, float whiteness);

	static void DiffuseRange();
	static void RenderColourSwatches();
	static void RenderMacbethChart();
	static void RenderFocusChart();

	static void RefreshAmbientValues();
	static void OverrideDynamicBakeValues(Vec4V& params);
	static void OverrideReflectionTweaks(Vector4& params);

	static void OverridePedLight(Vector3& colour, float &intensity, Vector3& direction);

	static void SetupGBuffers();
	static void Init();
	static void Draw();
	static void Update();

	static void GBufferOverride();

	static bool StencilMaskOverlaySelectedBegin(bool bIsSelectedEntity);
	static void StencilMaskOverlaySelectedEnd(bool bNeedsDepthStateRestore);
	static void StencilMaskOverlay(u8 ref, u8 mask, bool bFlipResult, Vec4V_In colour, float depth, int depthFunc);

	static void SaveViewport();
	static const grcViewport* GetViewport();

	// Variables
#if __XENON
	static bool				m_switchZPass;
#endif // __XENON

	static bool				m_timeBarToEKG;

	static s32				m_GBufferIndex;
	static bool				m_GBufferFilter[4];
	static bool				m_GBufferShowTwiddle;
	static bool				m_GBufferShowDepth;

	static bool				m_directEnable;
	static bool				m_ambientEnable;
	static bool				m_specularEnable;
	static bool				m_emissiveEnable;
	static bool				m_shadowEnable;

#if DYNAMIC_GB
	static bool				m_useLateESRAMGBufferEviction;
#endif

#if DEVICE_EQAA
	static bool				m_disableEqaaDuringCutoutPass;
#endif // DEVICE_EQAA

	static float			m_overrideDiffuse_desat;
	static bool				m_overrideDiffuse;
	static float			m_overrideDiffuse_intensity;
	static float			m_overrideDiffuse_amount;

	static bool				m_overrideEmissive;
	static float			m_overrideEmissive_intensity;
	static float			m_overrideEmissive_amount;

	static bool				m_overrideShadow;
	static float			m_overrideShadow_intensity;
	static float			m_overrideShadow_amount;

	static bool				m_overrideAmbient;
	static Vector2			m_overrideAmbientParams;

	static bool				m_overrideSpecular;
	static Vector3			m_overrideSpecularParams;

	static bool				m_forceSetAll8Lights;

	static bool				m_diffuseRangeShow;
	static float			m_diffuseRangeMin;
	static float			m_diffuseRangeMax;
	static Vector3			m_diffuseRangeLowerColour;
	static Vector3			m_diffuseRangeMidColour;
	static Vector3			m_diffuseRangeUpperColour;

	static bool				m_swatchesEnable;
	static bool				m_swatches2DEnable;
	static bool				m_swatches3DEnable;
	static float			m_swatchesNumX;
	static float			m_swatchesNumY;
	static float			m_swatchesSpacing;
	static float			m_swatchesCentreX;
	static float			m_swatchesCentreY;
	static atArray<Color32> m_swatchesColours;
	static float			m_swatchesSize;
	static float			m_chartOffset;

	static bool				m_focusChartEnable;
	static grcTexture*		m_focusChartTexture;
	static u16				m_focusChartMipLevel;
	static float			m_focusChartOffsetX;
	static float			m_focusChartOffsetY;

	static float			m_lightVolumeExperimental;

	static bool				m_decalDepthWrite;

#if __PPU
	static bool				m_spupmEnableReloadingElf;
	static bool				m_spupmDoReloadElf;
	static char				m_spupmReloadElfName[];
	static u32				m_spupmReloadElfSlot;
#endif // __PPU

	static s32				m_classificationThread;
	static bool				m_enableTiledRendering;

	static bool	            m_enableSkinPass;
	static bool				m_enableSkinPassNG;
	static Vector4          m_skinPassParams;
	
	static bool				m_enablePedLight;
	static bool				m_enablePedLightOverride;
	static Vector3			m_pedLightDirection;
	static Vector3			m_pedLightColour;
	static float			m_pedLightIntensity;
	static float			m_pedLightFadeUpTime;

	static bool             m_enableDynamicAmbientBakeOverride;
	static Vector4          m_dynamicBakeParams;

	static Vector3          m_SubSurfaceColorTweak;
	static bool	            m_enableFoliageLightPass;
	static bool	            m_useTexturedLights;
	static bool	            m_useGeomPassStencil;
	static bool             m_useLightsHiStencil;
	static bool				m_useSinglePassStencil;

	static bool				m_enableCubeMapReflection;
	static bool				m_copyCubeToParaboloid;
	static bool				m_enableCubeMapDebugMode;

	static bool				m_forceTiledDirectional;

	static bool				m_overrideReflectionMapFreeze;
	static bool				m_freezeReflectionMap;

	static bool				m_enableReflectiobTweakOverride;
	static Vector4			m_reflectionTweaks;

	static float            m_ReflectionBlurThreshold;

	static float			m_reflectionLodRangeMultiplier;

	static grcDepthStencilStateHandle m_override_DS;
	static grcRasterizerStateHandle m_override_R;

	static grcBlendStateHandle m_overrideDiffuse_B;
	static grcBlendStateHandle m_overrideDiffuseDesat_B;
	static grcBlendStateHandle m_overrideEmissive_B;
	static grcBlendStateHandle m_overrideShadow_B;
	static grcBlendStateHandle m_overrideAmbient_B;
	static grcBlendStateHandle m_overrideSpecular_B;

	static grcBlendStateHandle m_overrideAll_B;

	static bool m_bForceHighQualityLighting;

#if MSAA_EDGE_PASS
	static bool				m_EnableEdgePass;
# if MSAA_EDGE_PROCESS_FADING
	static bool				m_EnableEdgeFadeProcessing;
# endif
	static bool				m_IgnoreEdgeDetection;
	static bool				m_OldEdgeDetection;
	static bool				m_AlternativeEdgeDetection;
	static bool				m_AggressiveEdgeDetection;
	static bool				m_DebugEdgePassColor;
	static bool				m_OptimizeShadowReveal;
	static bool				m_EnableShadowRevealEdge;
	static bool				m_EnableShadowRevealFace;
	static bool				m_OptimizeShadowProcessing;
	static bool				m_EnableShadowProcessingEdge;
	static bool				m_EnableShadowProcessingFace;
	static bool				m_OptimizeDirectional;
	static bool				m_EnableDirectionalEdge0;
	static bool				m_EnableDirectionalEdge1;
	static bool				m_EnableDirectionalFace0;
	static bool				m_EnableDirectionalFace1;
	static bool				m_OptimizeDirectionalFoliage;
	static bool				m_EnableDirectionalFoliageEdge;
	static bool				m_EnableDirectionalFoliageFace;
	static unsigned			m_OptimizeSceneLights;
	static unsigned			m_OptimizeDepthEffects;
	static unsigned			m_OptimizeDofBlend;
	static unsigned			m_OptimizeComposite;
	static Vector4			m_edgeMarkParams;
#endif //MSAA_EDGE_PASS
};

// ----------------------------------------------------------------------------------------------- //

#endif // __BANK

#endif
