#ifndef __DEFERRED_LIGHTING_H__
#define __DEFERRED_LIGHTING_H__

// =============================================================================================== //
// INCLUDES
// =============================================================================================== //

// rage headers
#include "fwrenderer/renderlistgroup.h"
#include "grcore/device.h"
#if !__SPU
	#include "grmodel/shaderfx.h"
	#include "grmodel/shadergroupvar.h"
#endif

// game headers
#include "renderer/Lights/LightCommon.h"
#include "renderer/Deferred/DeferredConfig.h"
#include "scene/loader/MapData_Extensions.h"
#include "renderer/util/shmooFile.h"
#include "vfx/vfx_shared.h"

// =============================================================================================== //
// DEFINES
// =============================================================================================== //

#define LIGHT_VOLUME_BACKFACE_TECHNIQUE_MIN_ANGLE			(0.7f)
#define MSAA_EDGE_USE_DEPTH_COPY	(0 && MSAA_EDGE_PASS && RSG_PC)

class CHeightMapVariables;

class CLightSource;
class CLightShaft;

namespace rage {

class spdAABB;
class Matrix44;
class Vector3;
class Vector4;
class bkBank;
class grcRenderTarget;
class grcTexture;
#if __XENON
class grcTextureXenon;
#endif // __XENON
class grcViewport;
class grmShader;
class grmShaderGroup;
#if __PS3
class grcTextureGCM;
#endif
}

// =============================================================================================== //
// CLASS
// =============================================================================================== //

class DeferredLighting
{
public:

	enum HIStencilCullState
	{
		CULL_OFF = -1,
		CULL_SUBSAMPLEAA_ONLY = 0
	};

	enum LightingQuality
	{
		LQ_INGAME = 0,	// Quality mode used during gameplay
		LQ_CUTSCENE		// Quality used while cut-scens are playing
	};

	// -------------- //
	// Main functions //
	// -------------- //
#if RSG_PC
	static void DeleteShaders();
#endif
	static void InitShaders();
	static void InitRenderStates();
	static void TerminateRenderStates();

	static void	Init();
	static void	Shutdown();
	static void Update();

	// ------- //
	// General //
	// ------- //
	static grcEffectTechnique GetTechnique(const u32 lightType, MultisampleMode aaMode = MM_DEFAULT);
	static grcEffectTechnique GetTechnique(eDeferredTechnique deferredTechnique, MultisampleMode aaMode = MM_DEFAULT);
	static grmShader* GetShader(const u32 lightType);
	static grmShader* GetShader(eDeferredShaders deferredShader, MultisampleMode aaMode = MM_DEFAULT);
#if GENERATE_SHMOO
	static int GetShaderShmooIdx(eDeferredShaders deferredShader);
#else
	__forceinline static int GetShaderShmooIdx(eDeferredShaders ) { return -1; }
#endif	
	static eDeferredShaders GetShaderType(const u32 lightType);
	static s32 CalculatePass(const CLightSource *light, bool hasTexture, bool useShadows, bool useBackLighting);
	static eLightPass CalculateLightPass(u32 passFlags, u8 extraFlags);
	static void RenderDirectionalLight(s32 pass, const bool setSSAO, const MultisampleMode aaMode);
	static void RenderTiledDirectionalLight(const bool noDirLight, const MultisampleMode aaMode, const EdgeMode em);
	static void RenderTiledShadow();
	static void SetDirectionalRenderState(eDeferredShaders deferredShader, const MultisampleMode aaMode = MM_DEFAULT);
	static void Render2DDeferredLightingRect(const float x, const float y, const float w, const float h);
	static void Render2DDeferredLightingRect(const float x, const float y, const float w, const float h,
											 const float u, const float v, const float uW,const float vH);
	static grcEffectTechnique GetVolumeTechnique(const u32 lightType, const MultisampleMode aaMode = MM_DEFAULT);
	static grcEffectTechnique GetVolumeInterleaveReconstructionTechnique()
	{
		return m_techniques[MM_DEFAULT][DEFERRED_TECHNIQUE_VOLUME_INTERLEAVE_RECONSTRUCTION];
	}
	static eLightVolumePass CalculateVolumePass(const CLightSource *light, bool useShadows, bool cameraInLight);
#if __PPU && !__SPU
	static char* GetTextureOffsetGcm(grcTextureGCM *texgcm);
#endif

	template<typename varType>
	static void SetDeferredVar(eDeferredShaders deferredShader, eDeferredShaderVars deferredShaderVar, const varType &var, MultisampleMode aaMode = MM_DEFAULT);

	template<typename varType>
	static void SetDeferredGlobalVar(eDeferredShaderGlobalVars deferredShaderGlobalVar, const varType &var);

	template<typename varType>
	static void SetDeferredVarArray(eDeferredShaders deferredShader, eDeferredShaderVars deferredShaderVar, varType var, u32 count, MultisampleMode aaMode = MM_DEFAULT);

	static void SetDepthStencilState(const bool bNoDirLight, const EdgeMode em);
	static void RenderDirectionalHelper(const bool bNoDirLight, const MultisampleMode aaMode, const EdgeMode em);
	static void RenderFoliageHelper(const s32 pass, const MultisampleMode aaMode, const EdgeMode em);

	static void DefaultState();
	static void DefaultExitState();
	static void InitSetHiStencilCullState();
	static void TerminateHiStencilCullState();

	// ------ //
	// Passes //
	// ------ //
	static void ExecuteSkinLightPass(grcRenderTarget* backBuffer, const bool enablePedLight);
	static void ExecuteUISkinLightPass(grcRenderTarget* backBuffer, grcRenderTarget* backBufferCopy);
	static void ExecuteSkinLightPassForward(grcRenderTarget* pSrcTarget, grcRenderTarget* pSrcDepthTarget);
#if DEVICE_MSAA
	static void BindHdrBufferCopy();
#endif
#if MSAA_EDGE_PASS
	static bool IsEdgePassEnabled();
	static bool IsEdgePassActive();
	static bool IsEdgePassActiveForSceneLights();
	static bool IsEdgePassActiveForPostfx();
# if MSAA_EDGE_PROCESS_FADING
	static bool IsEdgeFadeProcessingActive();
# endif
	static void ExecuteEdgeMarkPass(bool isInterrior);
	static void ExecuteEdgeClearPass();
#endif

	static void ExecuteFoliageLightPass();

	static void ExecuteSnowPass();

	static void ExecuteAmbientAndDirectionalPass(const bool bDrawDefLight,  bool appliedZCullRefresh );

	static void ExecutePuddlePass();

	// ----------------- //
	// Getters / Setters //
	// ----------------- //

	//TODO: ALL OF THESE MUST SUPPORT MSAA flags... maybe just add support for putting DeferredLighting into MSAA on/off mode?
	static void	SetShaderGBufferTarget(GBufferRT index, const grcTexture* renderTarget);
	static void	SetShaderGBufferTargets(WIN32PC_ONLY(bool bindDepthCopy = false));
	static void SetShaderSSAOTarget(eDeferredShaders deferredShaders);
	static void	SetShaderDepthTargetsForDeferredVolume(const grcTexture* depthBuffer);
	static void SetShaderTargetsForVolumeInterleaveReconstruction();
	static void	SetLightShaderParams(eDeferredShaders deferredShader, const CLightSource* light, const grcTexture *texture, float radiusScale,
		const bool tangentAlignedToCamDir, const bool useHighResMesh, const MultisampleMode aaMode);
	static void SetLightShaftShaderParamsGlobal();
	static bool	SetLightShaftShaderParams(const CLightShaft* shaft);
	static void SetProjectionShaderParams(eDeferredShaders deferredShader, const MultisampleMode aaMode);
	static void GetProjectionShaderParams(Vec4V_InOut projParams, Vec3V_InOut shearProj0, Vec3V_InOut shearProj1, Vec3V_InOut shearProj2);

	static s32 GetTechniqueGroup() { return m_deferredTechniqueGroupId; }
	static s32 GetAlphaClipTechniqueGroup() { return m_deferredAlphaClipTechniqueGroupId; }
	static s32 GetSSAAlphaClipTechniqueGroup() { return m_deferredSSAAlphaClipTechniqueGroupId; }
	static s32 GetSSATechniqueGroup() { return m_deferredSubSampleAlphaTechniqueGroupId; }

	static grcTexture* GetVolumeTexture();

#if __XENON
	static void SetExtraRefMapState(bool enable);
	static void SetExtraTAAState(float flag);
#endif
	static void SetReflectionOcclusionParams();
	static void SetHiStencilCullState(HIStencilCullState type);

#if !__FINAL
	static void	SetDebugLightingParams();
	static void	SetDebugLightingParamsV(Vec4f_In params);
#endif // !__FINAL

	static void PushCutscenePedTechnique();
	static void PopCutscenePedTechnique();

	// ---------- //
	// Pass setup //
	// ---------- //
	static void BeginCausticsPass();
	static void EndCausticsPass();

	static void	PrepareGeometryPass();
	static void	FinishGeometryPass();

	static void	PrepareDecalPass(bool fadePass);
	static void	FinishDecalPass();

	static void	PrepareFadePass();
	static void	FinishFadePass();

	static void	PrepareCutoutPass(bool useSubSampleAlpha = true, bool forceClip=false, bool disableEQAA=false);
	static void	FinishCutoutPass();

	static void	PrepareSSAPass(bool useSubSampleAlpha = true, bool forceClip=false);
	static void	FinishSSAPass();
	
	static void PrepareTreePass();
	static void	FinishTreePass();

	static void PrepareDefaultPass(fwRenderPassId BANK_ONLY(id));
	static void FinishDefaultPass();
	
#if __XENON
	static void	PrepareGeometryPassTiled();
	static void	FinishGeometryPassTiled(bool bSkipGBuffer2LastTileResolve=false);

	static void RestoreDepthStencil(); 
#endif // __XENON

	/**
	 * This function selects the quality mode for light/shadow.
	 */
	static void QualityModeSelect();

	/**
	 * Get the currently selected lighting mode.
	 */
	static LightingQuality GetLighingQuality() { return m_LightingQuality; }

#if SSA_USES_CONDITIONALRENDER
	// Allows us to conditionally run the SSA Combine pass in PostFX based on if any SSA pixels are visible
	static void BeginSSAConditionalQuery();
	static void EndSSAConditionalQuery();
	static void BeginSSAConditionalRender();
	static void EndSSAConditionalRender();
	static bool HasValidSSAQuery() { return m_bSSAConditionalQueryIsValid; } // We don't have a valid query during screen shots.
#endif 

public: 

	static grmShader* m_deferredShaders[MM_TOTAL][NUM_DEFERRD_SHADERS];
#if DEVICE_MSAA
	static bool SupportMSAAandNonMSAAShaders() { return m_bSupportMSAAandNonMSAA; }
#endif


#if GENERATE_SHMOO
	static int m_deferredShadersShmoos[NUM_DEFERRD_SHADERS];
#endif // GENERATE_SHMOO
	static grcEffectVar m_deferredShaderVars[MM_TOTAL][NUM_DEFERRD_SHADERS][NUM_DEFERRED_SHADER_VARS];
	static grcEffectGlobalVar m_deferredShaderGlobalVars[NUM_DEFERRED_SHADER_GLOBAL_VARS];
	static grcEffectTechnique m_shaderLightShaftVolumeTechniques[CExtensionDefLightShaftVolumeType_NUM_ENUMS];
	static grcEffectTechnique m_techniques[MM_TOTAL][DEFERRED_TECHNIQUE_NUM_TECHNIQUES];
#if MSAA_EDGE_PASS
# if MSAA_EDGE_USE_DEPTH_COPY
	static grcRenderTarget* m_edgeMarkDepthCopy;
# endif
# if __BANK
	static grcRenderTarget* m_edgeMarkDebugTarget;
# endif
# if MSAA_EDGE_PROCESS_FADING
	static grcRenderTarget*	m_edgeCopyTarget;
	static grcEffectVar m_edgeCopyTextureVar;
# endif //MSAA_EDGE_PROCESS_FADING
	static grcEffectTechnique m_edgeMarkTechniqueIdx;
	static grcEffectVar m_edgeMarkParamsVar;
	static grcBlendStateHandle m_edgePass_B;
	static grcDepthStencilStateHandle m_edgePass_DS;
	static grcRasterizerStateHandle m_edgePass_R;
#endif //MSAA_EDGE_PASS

	static CHeightMapVariables	m_ReflectionOcculsionParams;

	static grcRenderTarget* m_paraboloidReflectionMap;

	// State blocks
	static grcDepthStencilStateHandle m_defaultState_DS;

	static grcRasterizerStateHandle m_defaultExitState_R;
	static grcDepthStencilStateHandle m_defaultExitState_DS;
	static grcBlendStateHandle m_defaultExitState_B;

	static grcDepthStencilStateHandle m_geometryPass_DS;
	static grcBlendStateHandle m_geometryPass_B;

	static grcBlendStateHandle m_decalPass_B;
	static grcDepthStencilStateHandle m_decalPass_DS;

	static grcBlendStateHandle m_fadePass_B;
	static grcDepthStencilStateHandle m_fadePass_DS;
#if MSAA_EDGE_PROCESS_FADING
	static grcDepthStencilStateHandle m_fadePassAA_DS;
#endif

	static grcBlendStateHandle m_cutoutPass_B;
	static grcDepthStencilStateHandle m_cutoutPass_DS;

	static grcDepthStencilStateHandle m_treePass_DS;

	static grcDepthStencilStateHandle m_defaultPass_DS;
	static grcBlendStateHandle m_defaultPass_B;

	static grcDepthStencilStateHandle m_causticsPass_DS;

#if !__XENON
	static grcBlendStateHandle m_causticsDepthPass_B;
	static grcDepthStencilStateHandle m_causticsDepthPass_DS;
#endif //__XENON

	static grcBlendStateHandle m_directionalPass_B;
	static grcDepthStencilStateHandle m_directionalFullPass_DS;
	static grcDepthStencilStateHandle m_directionalAmbientPass_DS;
	static grcDepthStencilStateHandle m_directionalNoStencilPass_DS;
	static grcRasterizerStateHandle m_directionalPass_R;
#if MSAA_EDGE_PASS
	static grcDepthStencilStateHandle m_directionalEdgeMaskEqualPass_DS;	// mask only edge bit for equality
	static grcDepthStencilStateHandle m_directionalEdgeAllEqualPass_DS;		// mask edge bit and the type (3bits) for equality
	static grcDepthStencilStateHandle m_directionalEdgeNotEqualPass_DS;		// mask edge bit and the type (3bits) for non-equality
	static grcDepthStencilStateHandle m_directionalEdgeBit0EqualPass_DS;	// mask edge bit and bit0 for equality
	static grcDepthStencilStateHandle m_directionalEdgeBit2EqualPass_DS;	// mask edge bit and bit2 for equality
#endif //MSAA_EDGE_PASS

	static grcDepthStencilStateHandle	m_foliagePrePass_DS;
	static grcDepthStencilStateHandle	m_foliageMainPass_DS;

	static grcBlendStateHandle m_foliagePass_B;
	static grcDepthStencilStateHandle m_foliagePass_DS;
	static grcDepthStencilStateHandle m_foliageNoStencilPass_DS;
	static grcRasterizerStateHandle m_foliagePass_R;

	static grcBlendStateHandle m_skinPass_B;
	static grcDepthStencilStateHandle m_skinPass_DS;
	static grcDepthStencilStateHandle m_skinPass_UI_DS;
	static grcDepthStencilStateHandle m_skinNoStencilPass_DS;
	static grcRasterizerStateHandle m_skinPass_R;
	static grcDepthStencilStateHandle m_skinPassForward_DS;

	static grcDepthStencilStateHandle m_tiledLighting_DS;

	static grcBlendStateHandle m_SingleSampleSSABlendState;
	static grcBlendStateHandle m_SSABlendState;

protected:

	static grcTexture* m_volumeTexture;

	static Vector4 m_defaultRimLightingParams;
	static float m_defaultRimLightingMainColourIntensity;
	static float m_defaultRimLightingOffAngleColourIntensity;

	static DECLARE_MTR_THREAD s32 m_previousGroupId;
	static s32 m_deferredTechniqueGroupId;
	static s32 m_deferredCutsceneTechniqueGroupId;
	static DECLARE_MTR_THREAD s32 m_prevDeferredPedTechnique;
	static s32 m_deferredAlphaClipTechniqueGroupId;
	static s32 m_deferredSSAAlphaClipTechniqueGroupId;

	static DECLARE_MTR_THREAD s32 m_previousGroupAlphaClipId;
	static s32 m_deferredSubSampleAlphaTechniqueGroupId;
	static s32 m_deferredSubSampleWriteAlphaTechniqueGroupId;

#if DEVICE_EQAA
	static DECLARE_MTR_THREAD bool m_eqaaDisabledForCutout;
#endif


	static grcEffectVar	m_shaderLightParameterID_deferredLightVolumeParams;

#if __XENON || __D3D11 || RSG_ORBIS
	static grcEffectVar	m_shaderLightParameterID_GBufferStencilTexture;
#endif //__XENON...

#if (__D3D11 || RSG_ORBIS)
	static grcEffectVar m_shaderDownsampledDepthSampler;
#endif // (__D3D11 || RSG_ORBIS)

#if ENABLE_PED_PASS_AA_SOURCE
	static grcEffectVar m_shaderLightTextureAA;
#endif // ENABLE_PED_PASS_AA_SOURCE

	static grcEffectVar m_shaderLightParameterID_skinColourTweak;
	static grcEffectVar m_shaderLightParameterID_skinParams;
	
	static grcEffectVar m_shaderLightParameterID_rimLightingParams;
	static grcEffectVar m_shaderLightParameterID_rimLightingMainColourAndIntensity;
	static grcEffectVar m_shaderLightParameterID_rimLightingOffAngleColourAndIntensity;
	static grcEffectVar m_shaderLightParameterID_rimLightingLightDirection;

	static grcEffectVar	m_shaderLightParameterID_ParabTexture;
	static grcEffectVar	m_shaderLightParameterID_ParabDepthTexture;

#if !__FINAL
	static grcEffectVar m_debugLightingParamsID;
#endif // !__FINAL

	static grcEffectVar	m_shaderVolumeParameterID_deferredVolumePosition;
	static grcEffectVar	m_shaderVolumeParameterID_deferredVolumeDirection;
	static grcEffectVar	m_shaderVolumeParameterID_deferredVolumeTangentXAndShaftRadius;
	static grcEffectVar m_shaderVolumeParameterID_deferredVolumeTangentYAndShaftLength;
	static grcEffectVar	m_shaderVolumeParameterID_deferredVolumeColour;
	static grcEffectVar	m_shaderVolumeParameterID_deferredVolumeShaftPlanes;
	static grcEffectVar	m_shaderVolumeParameterID_deferredVolumeShaftGradient;
	static grcEffectVar	m_shaderVolumeParameterID_deferredVolumeShaftGradientColourInv;
	static grcEffectVar m_shaderVolumeParameterID_deferredVolumeShaftCompositeMtx;
	static grcEffectVar	m_shaderVolumeParameterID_deferredVolumeDepthBuffer;
	static grcEffectVar m_shaderVolumeParameterID_deferredVolumeOffscreenBuffer;
#if LIGHT_VOLUME_USE_LOW_RESOLUTION
	static grcEffectVar m_shaderVolumeParameterID_deferredVolumeLowResDepthBuffer;
#endif

#if __BANK
	static grcEffectVar m_shaderVolumeParameterID_GBufferTextureDepth;
#endif

	static grcViewport*	m_pPreviousViewport;
	static grcViewport*	m_pOrthoViewport;
	static bool	m_bIsViewportPerspective;

#if DEVICE_MSAA
	static bool m_bSupportMSAAandNonMSAA;
#endif

	/**
	 * Currently selected lighting quality.
	 */
	static LightingQuality m_LightingQuality;

#if SSA_USES_CONDITIONALRENDER
	// Wraps all of SSA rendering in a conditional query so we can skip the SSA Combine pass in PostFX if
	// no SSA pixels end up being visible.
	static grcConditionalQuery m_SSAConditionalQuery;
	static bool                m_bSSAConditionalQueryIsValid;
#endif // SSA_USES_CONDITIONALRENDER
};

#define DEFERRED_LIGHTING_DEFAULT_subsurfaceColorTweak Vector3(1.f,0.8471f,0.659f)

#endif // __DEFERRED_LIGHTING_H__...
