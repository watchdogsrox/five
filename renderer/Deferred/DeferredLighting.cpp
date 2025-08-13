// =============================================================================================== //
// INCLUDES
// =============================================================================================== //

// rage headers
#include "grcore/allocscope.h"
#include "grcore/image.h"
#include "grcore/device.h"
#include "grcore/im.h"
#include "grcore/stateblock.h"
#include "grcore/texture.h"
#include "grcore/viewport.h"
#include "grcore/config.h"
#include "grcore/effect_values.h"
#include "grcore/effect_typedefs.h"
#include "grcore/quads.h"
#if __PPU
#include "grcore/wrapper_gcm.h"
#include "grcore/texturegcm.h"
#endif
#include "grmodel/shader.h"
#include "grmodel/shaderfx.h"
#include "grmodel/shadergroup.h"
#include "rmcore/drawable.h"
#include "system/param.h"
#include "system/nelem.h"
#include "system/xtl.h"
#include "system/pad.h"
#include "system/filemgr.h"
#include "system/system.h"
#include "system/memory.h"
#include "vector/vector3.h"
#include "bank/bank.h"
#include "file/asset.h"
#include "Math/amath.h"

#if __D3D
	#include "grcore/wrapper_d3d.h"
	#include "system/d3d9.h"
	#if __XENON
		#include "system/xtl.h"
		#define DBG 0			// yuck
		#include <xgraphics.h>
	#endif
#endif

// framework headers
#include "fwmaths/random.h"
#include "fwmaths/vector.h"
#include "fwmaths/angle.h"
#include "fwmaths/vectorutil.h"
#include "fwscene/stores/txdstore.h"
#include "fwsys/timer.h"

// game headers
#include "camera/CamInterface.h"
#include "camera/cutscene/CutsceneDirector.h"
#include "camera/viewports/Viewport.h"
#include "camera/viewports/ViewportManager.h"
#include "cutscene/CutSceneManagerNew.h"
#include "debug/BudgetDisplay.h"
#include "debug/debug.h"
#include "debug/debugglobals.h"
#include "debug/Rendering/DebugDeferred.h"
#include "debug/Rendering/DebugLighting.h"
#include "debug/TiledScreenCapture.h"
#include "game/clock.h"			//CClock::ms_nGameClockHours
#include "game/modelindices.h"
#include "game/weather.h"
#include "modelinfo/VehicleModelInfo.h"
#include "peds/Ped.h"
#include "physics/GtaArchetype.h"
#include "physics/WorldProbe/worldprobe.h"
#include "scene/world/gameWorld.h"
#include "scene/FileLoader.h"
#include "Shaders/ShaderLib.h"
#include "streaming/streaming.h"
#include "vehicles/Boat.h"
#include "vehicles/vehicle.h"
#include "renderer/Deferred/DeferredLighting.h"
#include "renderer/Deferred/GBuffer.h"
#include "renderer/lights/lights.h"
#include "renderer/lights/LightSource.h"
#include "renderer/Lights/TiledLighting.h"
#include "renderer/PostProcessFX.h"
#include "renderer/RenderPhases/RenderPhaseCascadeShadows.h"
#include "renderer/RenderPhases/RenderPhaseHeightMap.h"
#include "renderer/RenderPhases/RenderPhaseReflection.h"
#include "renderer/rendertargets.h"
#include "renderer/river.h"
#include "renderer/sprite2d.h"		//CSprite2d::DrawTxRect()...
#include "renderer/SpuPM/SpuPmMgr.h"
#include "renderer/SSAO.h"
#include "renderer/Util/ShaderUtil.h"
#include "renderer/Util/Util.h"
#include "renderer/water.h"
#include "renderer/zonecull.h"
#include "renderer/RenderListBuilder.h"
#include "TimeCycle/TimeCycle.h"
#include "TimeCycle/TimeCycleConfig.h"
#include "vfx/misc/Puddles.h"
#include "vfx/VfxHelper.h"

// =============================================================================================== //
// DEFINES
// =============================================================================================== //

RENDER_OPTIMISATIONS()

#define DUMP_GBUFFER_TARGETS	(0 && __DEV)
// ----------------------------------------------------------------------------------------------- //

//DECAL FLAGS
#define DECAL_BLEND_COLOUR (0x1<<0)
#define DECAL_BLEND_SPEC (0x1<<1)
#define DECAL_BLEND_AMB (0x1<<2)
#define DECAL_WRITE_DEPTH (0x1<<3)
#define DECAL_WRITE_NORM (0x1<<4)
#define DECAL_ALPHA_TO_MASK_WRITE_ALL (0x1<<5) //assumes write all

// ----------------------------------------------------------------------------------------------- //

// Static member creation/initialization.
grcTexture*	DeferredLighting::m_volumeTexture = NULL;

s32	DECLARE_MTR_THREAD DeferredLighting::m_previousGroupId	= - 1;
s32	DeferredLighting::m_deferredTechniqueGroupId;
s32 DeferredLighting::m_deferredCutsceneTechniqueGroupId= - 1;
s32	DeferredLighting::m_deferredAlphaClipTechniqueGroupId = -1;
s32	DeferredLighting::m_deferredSSAAlphaClipTechniqueGroupId = -1;

s32	DECLARE_MTR_THREAD DeferredLighting::m_prevDeferredPedTechnique	= - 1;

s32	DECLARE_MTR_THREAD DeferredLighting::m_previousGroupAlphaClipId = -1;
s32 DeferredLighting::m_deferredSubSampleWriteAlphaTechniqueGroupId = -1;
s32 DeferredLighting::m_deferredSubSampleAlphaTechniqueGroupId = -1;

grmShader* DeferredLighting::m_deferredShaders[MM_TOTAL][NUM_DEFERRD_SHADERS];

#if DEVICE_EQAA
bool DECLARE_MTR_THREAD DeferredLighting::m_eqaaDisabledForCutout = false;
#endif // DEVICE_EQAA

#if GENERATE_SHMOO
int DeferredLighting::m_deferredShadersShmoos[NUM_DEFERRD_SHADERS];
#endif // GENERATE_SHMOO

grcEffectVar DeferredLighting::m_deferredShaderVars[MM_TOTAL][NUM_DEFERRD_SHADERS][NUM_DEFERRED_SHADER_VARS];
grcEffectGlobalVar DeferredLighting::m_deferredShaderGlobalVars[NUM_DEFERRED_SHADER_GLOBAL_VARS];

const char* m_deferredShaderVarNames[NUM_DEFERRED_SHADER_VARS] =
{
	"deferredLightParams",
	"deferredLightTexture",
	"deferredLightTexture1",
	"deferredLightTexture2",
	"deferredLightScreenSize",
	"deferredProjectionParams",
	"deferredPerspectiveShearParams0",
	"deferredPerspectiveShearParams1",
	"deferredPerspectiveShearParams2",
	"deferredLightVolumeParams",
	"dLocalShadowData",
};

const char* m_deferredShaderGlobalVarNames[] =
{
	"gbufferTexture0Global",
	"gbufferTexture1Global",
	"gbufferTexture2Global",
	"gbufferTexture3Global",
	"gbufferTextureDepthGlobal",
#if __XENON || __D3D11 || RSG_ORBIS
	"gbufferStencilTextureGlobal",
#endif
#if DEVICE_EQAA
	"gbufferFragmentMask0Global",
	"gbufferFragmentMask1Global",
	"gbufferFragmentMask2Global",
	"gbufferFragmentMask3Global",
#endif // DEVICE_EQAA
};
CompileTimeAssert( NELEM(m_deferredShaderGlobalVarNames) == NUM_DEFERRED_SHADER_GLOBAL_VARS );

const bool m_deferredShaderGlobalVarRequired[] =
{
	true,
	true,
	true,
	true,
	true,
#if __XENON || __D3D11 || RSG_ORBIS
	true,
#endif
#if DEVICE_EQAA
	false,
	false,
	false,
	false,
#endif // DEVICE_EQAA
};
CompileTimeAssert( NELEM(m_deferredShaderGlobalVarRequired) == NUM_DEFERRED_SHADER_GLOBAL_VARS );

grcEffectTechnique DeferredLighting::m_shaderLightShaftVolumeTechniques[CExtensionDefLightShaftVolumeType_NUM_ENUMS];
grcEffectTechnique DeferredLighting::m_techniques[MM_TOTAL][DEFERRED_TECHNIQUE_NUM_TECHNIQUES];

#if MSAA_EDGE_PASS
# if MSAA_EDGE_USE_DEPTH_COPY
grcRenderTarget*			DeferredLighting::m_edgeMarkDepthCopy;
# endif //MSAA_EDGE_USE_DEPTH_COPY
# if __DEV
grcRenderTarget*			DeferredLighting::m_edgeMarkDebugTarget;
# endif //__BANK
# if MSAA_EDGE_PROCESS_FADING
grcRenderTarget*			DeferredLighting::m_edgeCopyTarget;
grcEffectVar				DeferredLighting::m_edgeCopyTextureVar;
# endif //MSAA_EDGE_PROCESS_FADING
grcEffectTechnique			DeferredLighting::m_edgeMarkTechniqueIdx;
grcEffectVar				DeferredLighting::m_edgeMarkParamsVar;
grcRasterizerStateHandle	DeferredLighting::m_edgePass_R;
grcDepthStencilStateHandle	DeferredLighting::m_edgePass_DS;
grcBlendStateHandle			DeferredLighting::m_edgePass_B;
#endif //MSAA_EDGE_PASS

grcEffectVar DeferredLighting::m_shaderLightParameterID_skinColourTweak;
grcEffectVar DeferredLighting::m_shaderLightParameterID_skinParams;

#if __D3D11 || RSG_ORBIS
grcEffectVar DeferredLighting::m_shaderDownsampledDepthSampler=grcevNONE;
#endif	

#if ENABLE_PED_PASS_AA_SOURCE
grcEffectVar DeferredLighting::m_shaderLightTextureAA=grcevNONE;
#endif // ENABLE_PED_PASS_AA_SOURCE

#if !__FINAL
grcEffectVar DeferredLighting::m_debugLightingParamsID;
#endif // !__FINAL

grcEffectVar DeferredLighting::m_shaderVolumeParameterID_deferredVolumePosition;
grcEffectVar DeferredLighting::m_shaderVolumeParameterID_deferredVolumeDirection;
grcEffectVar DeferredLighting::m_shaderVolumeParameterID_deferredVolumeTangentXAndShaftRadius;
grcEffectVar DeferredLighting::m_shaderVolumeParameterID_deferredVolumeTangentYAndShaftLength;
grcEffectVar DeferredLighting::m_shaderVolumeParameterID_deferredVolumeColour;
grcEffectVar DeferredLighting::m_shaderVolumeParameterID_deferredVolumeShaftPlanes;
grcEffectVar DeferredLighting::m_shaderVolumeParameterID_deferredVolumeShaftGradient;
grcEffectVar DeferredLighting::m_shaderVolumeParameterID_deferredVolumeShaftGradientColourInv;
grcEffectVar DeferredLighting::m_shaderVolumeParameterID_deferredVolumeShaftCompositeMtx;
grcEffectVar DeferredLighting::m_shaderVolumeParameterID_deferredVolumeDepthBuffer;
grcEffectVar DeferredLighting::m_shaderVolumeParameterID_deferredVolumeOffscreenBuffer;
#if LIGHT_VOLUME_USE_LOW_RESOLUTION
grcEffectVar DeferredLighting::m_shaderVolumeParameterID_deferredVolumeLowResDepthBuffer;
#endif


#if __BANK
grcEffectVar DeferredLighting::m_shaderVolumeParameterID_GBufferTextureDepth;
#endif


grcViewport* DeferredLighting::m_pPreviousViewport = NULL;
grcViewport* DeferredLighting::m_pOrthoViewport	= NULL;
bool DeferredLighting::m_bIsViewportPerspective = false;

grcRenderTarget* DeferredLighting::m_paraboloidReflectionMap = NULL;

grcDepthStencilStateHandle DeferredLighting::m_defaultState_DS;

grcDepthStencilStateHandle DeferredLighting::m_defaultExitState_DS;
grcBlendStateHandle	DeferredLighting::m_defaultExitState_B;
grcRasterizerStateHandle DeferredLighting::m_defaultExitState_R;

grcDepthStencilStateHandle DeferredLighting::m_geometryPass_DS;
grcBlendStateHandle DeferredLighting::m_geometryPass_B;

grcBlendStateHandle DeferredLighting::m_decalPass_B;
grcDepthStencilStateHandle DeferredLighting::m_decalPass_DS;

grcBlendStateHandle DeferredLighting::m_fadePass_B;
grcDepthStencilStateHandle DeferredLighting::m_fadePass_DS;
#if MSAA_EDGE_PROCESS_FADING
grcDepthStencilStateHandle DeferredLighting::m_fadePassAA_DS;
#endif

grcBlendStateHandle DeferredLighting::m_cutoutPass_B;
grcDepthStencilStateHandle DeferredLighting::m_cutoutPass_DS;

grcDepthStencilStateHandle DeferredLighting::m_treePass_DS;

grcDepthStencilStateHandle DeferredLighting::m_defaultPass_DS;
grcBlendStateHandle DeferredLighting::m_defaultPass_B;

grcBlendStateHandle DeferredLighting::m_directionalPass_B;
grcDepthStencilStateHandle DeferredLighting::m_directionalFullPass_DS;
grcDepthStencilStateHandle DeferredLighting::m_directionalAmbientPass_DS;
grcDepthStencilStateHandle DeferredLighting::m_directionalNoStencilPass_DS;
grcRasterizerStateHandle DeferredLighting::m_directionalPass_R;
#if MSAA_EDGE_PASS
grcDepthStencilStateHandle DeferredLighting::m_directionalEdgeMaskEqualPass_DS;
grcDepthStencilStateHandle DeferredLighting::m_directionalEdgeAllEqualPass_DS;
grcDepthStencilStateHandle DeferredLighting::m_directionalEdgeNotEqualPass_DS;
grcDepthStencilStateHandle DeferredLighting::m_directionalEdgeBit0EqualPass_DS;
grcDepthStencilStateHandle DeferredLighting::m_directionalEdgeBit2EqualPass_DS;
#endif //MSAA_EDGE_PASS

grcDepthStencilStateHandle	DeferredLighting::m_foliagePrePass_DS;
grcDepthStencilStateHandle	DeferredLighting::m_foliageMainPass_DS;

grcBlendStateHandle DeferredLighting::m_foliagePass_B;
grcDepthStencilStateHandle DeferredLighting::m_foliagePass_DS;
grcDepthStencilStateHandle DeferredLighting::m_foliageNoStencilPass_DS;
grcRasterizerStateHandle DeferredLighting::m_foliagePass_R;

grcBlendStateHandle DeferredLighting::m_skinPass_B;
grcDepthStencilStateHandle DeferredLighting::m_skinPass_DS;
grcDepthStencilStateHandle DeferredLighting::m_skinPass_UI_DS;
grcDepthStencilStateHandle DeferredLighting::m_skinPassForward_DS;
grcDepthStencilStateHandle DeferredLighting::m_skinNoStencilPass_DS;
grcRasterizerStateHandle DeferredLighting::m_skinPass_R;

grcDepthStencilStateHandle DeferredLighting::m_tiledLighting_DS;


grcBlendStateHandle DeferredLighting::m_SingleSampleSSABlendState;
grcBlendStateHandle DeferredLighting::m_SSABlendState;

DeferredLighting::LightingQuality DeferredLighting::m_LightingQuality;

#if SSA_USES_CONDITIONALRENDER
grcConditionalQuery DeferredLighting::m_SSAConditionalQuery = 0;
bool DeferredLighting::m_bSSAConditionalQueryIsValid = false;
#endif // SSA_USES_CONDITIONALRENDER

#if DEVICE_MSAA
bool DeferredLighting::m_bSupportMSAAandNonMSAA = false;
#endif

PARAM(pcTiledLighting, "Enables tiled lighting (PC defaults to non-tiled)");

// ----------------------------------------------------------------------------------------------- //

#if __WIN32PC
#define CheckVar(x)
#else
#define CheckVar(x) Assert(x)
#endif

#if RSG_PC
void DeferredLighting::DeleteShaders()
{
	for (int aaMode = 0; aaMode != MM_TOTAL; ++aaMode)
	{
#if DEVICE_MSAA
		if (!m_bSupportMSAAandNonMSAA && aaMode != MM_DEFAULT)
			continue;
#endif
		for (int es = 0; es != NUM_DEFERRD_SHADERS; ++es)
		{
			// Look-up all the shared vars for all the deferred shaders
			for (u32 j = 0; j < NUM_DEFERRED_SHADER_VARS; j++)
			{
				m_deferredShaderVars[aaMode][es][j] = grcevNONE;
			}
			// Delete shaders
			if (m_deferredShaders[aaMode][es])
			{
				delete m_deferredShaders[aaMode][es];
				m_deferredShaders[aaMode][es] = NULL;
			}
		}
	}

	for(u32 i = 0; i < NUM_DEFERRED_SHADER_GLOBAL_VARS; i++)
	{
		m_deferredShaderGlobalVars[i] = grcegvNONE;
	}

	memset(&m_techniques, NULL, sizeof(m_techniques));
}
#endif //RSG_PC

void DeferredLighting::InitShaders()
{
	memset(&m_deferredShaders, 0, sizeof(m_deferredShaders));

	// Create and set up the shaders.
	ASSET.PushFolder("common:/shaders");

	// Use the non-msaa shaders for 4s1f EQAA
	MSAA_ONLY( m_bSupportMSAAandNonMSAA = GRCDEVICE.GetMSAA() > 1 );

	// Create and load the shader.
	for (int es = 0; es != NUM_DEFERRD_SHADERS; ++es)
	{
		m_deferredShaders[MM_DEFAULT][es] = grmShaderFactory::GetInstance().Create();
	}
	
#if DEVICE_MSAA
	if ( m_bSupportMSAAandNonMSAA )
	{
		m_deferredShaders[MM_SUPER_SAMPLE][DEFERRED_SHADER_LIGHTING]	->Load("deferred_lightingMS");
		m_deferredShaders[MM_SUPER_SAMPLE][DEFERRED_SHADER_AMBIENT]		->Load("deferred_ambientMS");
		m_deferredShaders[MM_SUPER_SAMPLE][DEFERRED_SHADER_DIRECTIONAL]	->Load("directionalMS");
		m_deferredShaders[MM_SUPER_SAMPLE][DEFERRED_SHADER_SPOT]		->Load("spotMS");
		m_deferredShaders[MM_SUPER_SAMPLE][DEFERRED_SHADER_POINT]		->Load("pointMS");
		m_deferredShaders[MM_SUPER_SAMPLE][DEFERRED_SHADER_CAPSULE]		->Load("capsuleMS");
		m_deferredShaders[MM_SUPER_SAMPLE][DEFERRED_SHADER_TILED]		->Load("tiled_lightingMS");


		(m_deferredShaders[MM_SINGLE][DEFERRED_SHADER_LIGHTING]		= grmShaderFactory::GetInstance().Create())->Load("deferred_lighting");
		(m_deferredShaders[MM_SINGLE][DEFERRED_SHADER_VOLUME]		= NULL);
		(m_deferredShaders[MM_SINGLE][DEFERRED_SHADER_DIRECTIONAL]	= grmShaderFactory::GetInstance().Create())->Load("directional");
		(m_deferredShaders[MM_SINGLE][DEFERRED_SHADER_SPOT]			= grmShaderFactory::GetInstance().Create())->Load("spot");
		(m_deferredShaders[MM_SINGLE][DEFERRED_SHADER_POINT]		= grmShaderFactory::GetInstance().Create())->Load("point");
		(m_deferredShaders[MM_SINGLE][DEFERRED_SHADER_CAPSULE]		= grmShaderFactory::GetInstance().Create())->Load("capsule");
		(m_deferredShaders[MM_SINGLE][DEFERRED_SHADER_TILED]		= NULL);
		(m_deferredShaders[MM_SINGLE][DEFERRED_SHADER_AMBIENT]		= NULL);

#if MSAA_EDGE_PASS
		(m_deferredShaders[MM_TEXTURE_READS_ONLY][DEFERRED_SHADER_DIRECTIONAL]	= grmShaderFactory::GetInstance().Create())->Load("directionalMS0");
		(m_deferredShaders[MM_TEXTURE_READS_ONLY][DEFERRED_SHADER_LIGHTING]		= grmShaderFactory::GetInstance().Create())->Load("deferred_lightingMS0");
		(m_deferredShaders[MM_TEXTURE_READS_ONLY][DEFERRED_SHADER_SPOT]			= grmShaderFactory::GetInstance().Create())->Load("spotMS0");
		(m_deferredShaders[MM_TEXTURE_READS_ONLY][DEFERRED_SHADER_POINT]		= grmShaderFactory::GetInstance().Create())->Load("pointMS0");
		(m_deferredShaders[MM_TEXTURE_READS_ONLY][DEFERRED_SHADER_CAPSULE]		= grmShaderFactory::GetInstance().Create())->Load("capsuleMS0");
		(m_deferredShaders[MM_TEXTURE_READS_ONLY][DEFERRED_SHADER_TILED]		= grmShaderFactory::GetInstance().Create())->Load("tiled_lightingMS0");
		m_edgeMarkTechniqueIdx	= m_deferredShaders[MM_TEXTURE_READS_ONLY][DEFERRED_SHADER_LIGHTING]->LookupTechnique("EdgeMark");
		m_edgeMarkParamsVar		= m_deferredShaders[MM_TEXTURE_READS_ONLY][DEFERRED_SHADER_LIGHTING]->LookupVar("EdgeMarkParams");
		m_edgeCopyTextureVar	= m_deferredShaders[MM_TEXTURE_READS_ONLY][DEFERRED_SHADER_LIGHTING]->LookupVar("StencilCopy");
#endif //MSAA_EDGE_PASS
	}
	else
#endif	//DEVICE_MSAA
	{
		m_deferredShaders[MM_DEFAULT][DEFERRED_SHADER_LIGHTING]		->Load("deferred_lighting");
		m_deferredShaders[MM_DEFAULT][DEFERRED_SHADER_AMBIENT]		->Load("deferred_ambient");
		m_deferredShaders[MM_DEFAULT][DEFERRED_SHADER_DIRECTIONAL]	->Load("directional");
		m_deferredShaders[MM_DEFAULT][DEFERRED_SHADER_SPOT]			->Load("spot");
		m_deferredShaders[MM_DEFAULT][DEFERRED_SHADER_POINT]		->Load("point");
		m_deferredShaders[MM_DEFAULT][DEFERRED_SHADER_CAPSULE]		->Load("capsule");
		m_deferredShaders[MM_DEFAULT][DEFERRED_SHADER_TILED]		->Load("tiled_lighting");
	}

	m_deferredShaders[MM_DEFAULT][DEFERRED_SHADER_VOLUME]->Load("deferred_volume");

#if __BANK
	m_deferredShaders[MM_DEFAULT][DEFERRED_SHADER_DEBUG] = grmShaderFactory::GetInstance().Create();
	m_deferredShaders[MM_DEFAULT][DEFERRED_SHADER_DEBUG]->Load("debug_rendering");
#endif


	// only for tiled_lighting until further notice.
#if GENERATE_SHMOO
	for(int i=0;i<NUM_DEFERRD_SHADERS;i++)
	{
		m_deferredShadersShmoos[i] = -1;
	}	
#endif	
	GENSHMOO_ONLY(m_deferredShadersShmoos[DEFERRED_SHADER_TILED] = ) ShmooHandling::Register("tiled_lighting",m_deferredShaders[MM_DEFAULT][DEFERRED_SHADER_TILED],true,0.0f);

	ASSET.PopFolder();

	for(u32 i = 0; i < NUM_DEFERRED_SHADER_GLOBAL_VARS; i++)
	{
		m_deferredShaderGlobalVars[i] = grcEffect::LookupGlobalVar(m_deferredShaderGlobalVarNames[i], m_deferredShaderGlobalVarRequired[i]);
	}

	// Look-up all the shared vars for all the deferred shaders
	for (int aaMode = 0; aaMode != MM_TOTAL; ++aaMode)
	{
#if DEVICE_MSAA
		if (!m_bSupportMSAAandNonMSAA && aaMode != MM_DEFAULT)
			continue;
#endif
		for (u32 i = 0; i < NUM_DEFERRD_SHADERS; i++)
		{
			for (u32 j = 0; j < NUM_DEFERRED_SHADER_VARS; j++)
			{
				if (m_deferredShaders[aaMode][i])
				{
					m_deferredShaderVars[aaMode][i][j] = m_deferredShaders[aaMode][i]->LookupVar(m_deferredShaderVarNames[j], false);
				}
			}
		}
	}

#if __D3D11 || RSG_ORBIS
	m_shaderDownsampledDepthSampler = m_deferredShaders[MM_DEFAULT][DEFERRED_SHADER_TILED]->LookupVar("downsampledDepthSampler");
#endif
#if ENABLE_PED_PASS_AA_SOURCE
	m_shaderLightTextureAA = m_deferredShaders[MM_DEFAULT][DEFERRED_SHADER_LIGHTING]->LookupVar("deferredLightTextureAA");
#endif

	m_shaderLightParameterID_skinColourTweak = m_deferredShaders[MM_DEFAULT][DEFERRED_SHADER_LIGHTING]->LookupVar("skinColourTweak", true);
	m_shaderLightParameterID_skinParams = m_deferredShaders[MM_DEFAULT][DEFERRED_SHADER_LIGHTING]->LookupVar("skinParams", true);

#if !__FINAL
	m_debugLightingParamsID = m_deferredShaders[MM_DEFAULT][DEFERRED_SHADER_DIRECTIONAL]->LookupVar("DebugLightingParams");
#endif // !__FINAL

	// Volume shading techniques
	{
		grmShader *const volume = m_deferredShaders[MM_DEFAULT][DEFERRED_SHADER_VOLUME];
		m_shaderVolumeParameterID_deferredVolumePosition				= volume->LookupVar("deferredVolumePosition", true);
		m_shaderVolumeParameterID_deferredVolumeDirection				= volume->LookupVar("deferredVolumeDirection", true);
		m_shaderVolumeParameterID_deferredVolumeTangentXAndShaftRadius	= volume->LookupVar("deferredVolumeTangentXAndShaftRadius", true);
		m_shaderVolumeParameterID_deferredVolumeTangentYAndShaftLength	= volume->LookupVar("deferredVolumeTangentYAndShaftLength", true);
		m_shaderVolumeParameterID_deferredVolumeColour					= volume->LookupVar("deferredVolumeColour", true);
		m_shaderVolumeParameterID_deferredVolumeShaftPlanes				= volume->LookupVar("deferredVolumeShaftPlanes", true);
		m_shaderVolumeParameterID_deferredVolumeShaftGradient			= volume->LookupVar("deferredVolumeShaftGradient", true);
		m_shaderVolumeParameterID_deferredVolumeShaftGradientColourInv	= volume->LookupVar("deferredVolumeShaftGradientColourInv", true);
		m_shaderVolumeParameterID_deferredVolumeShaftCompositeMtx		= volume->LookupVar("deferredVolumeShaftCompositeMtx", true);
		m_shaderVolumeParameterID_deferredVolumeDepthBuffer				= volume->LookupVar("deferredVolumeDepthBuffer", true);
		m_shaderVolumeParameterID_deferredVolumeOffscreenBuffer			= volume->LookupVar("gVolumeLightsTexture");
	#if LIGHT_VOLUME_USE_LOW_RESOLUTION
		m_shaderVolumeParameterID_deferredVolumeLowResDepthBuffer		= volume->LookupVar("gLowResDepthTexture");
	#endif
	}

	// Deferred lighting techniques: Spot, Point, Capsule
	for (int aaMode = 0; aaMode != MM_TOTAL; ++aaMode)
	{
#if DEVICE_MSAA
		if (!m_bSupportMSAAandNonMSAA && aaMode != MM_DEFAULT)
			continue;
#endif
		m_techniques[aaMode][DEFERRED_TECHNIQUE_SPOTCM]			= m_deferredShaders[aaMode][DEFERRED_SHADER_SPOT]	->LookupTechnique("spotCM");
		m_techniques[aaMode][DEFERRED_TECHNIQUE_SPOTCM_VOLUME]	= m_deferredShaders[aaMode][DEFERRED_SHADER_SPOT]	->LookupTechnique("spotCM_volume");
		m_techniques[aaMode][DEFERRED_TECHNIQUE_POINTCM]		= m_deferredShaders[aaMode][DEFERRED_SHADER_POINT]	->LookupTechnique("pointCM");
		m_techniques[aaMode][DEFERRED_TECHNIQUE_POINTCM_VOLUME]	= m_deferredShaders[aaMode][DEFERRED_SHADER_POINT]	->LookupTechnique("pointCM_volume");
		m_techniques[aaMode][DEFERRED_TECHNIQUE_CAPSULE]		= m_deferredShaders[aaMode][DEFERRED_SHADER_CAPSULE]->LookupTechnique("capsule");
		m_techniques[aaMode][DEFERRED_TECHNIQUE_CAPSULE_VOLUME]	= m_deferredShaders[aaMode][DEFERRED_SHADER_CAPSULE]->LookupTechnique("capsule_volume");
	}

	// Deferred lighting techniques: Ambient, Directional, Tiled
	m_techniques[MM_DEFAULT][DEFERRED_TECHNIQUE_AMBIENT_VOLUME]		= m_deferredShaders[MM_DEFAULT][DEFERRED_SHADER_LIGHTING]	->LookupTechnique("ambientScaleVolume");
	m_techniques[MM_DEFAULT][DEFERRED_TECHNIQUE_DIRECTIONAL]		= m_deferredShaders[MM_DEFAULT][DEFERRED_SHADER_DIRECTIONAL]->LookupTechnique("directional");
	m_techniques[MM_DEFAULT][DEFERRED_TECHNIQUE_TILED_DIRECTIONAL]	= m_deferredShaders[MM_DEFAULT][DEFERRED_SHADER_TILED]		->LookupTechnique("tiled_directional");
	m_techniques[MM_DEFAULT][DEFERRED_TECHNIQUE_TILED_AMBIENT]		= m_deferredShaders[MM_DEFAULT][DEFERRED_SHADER_TILED]		->LookupTechnique("tiled_ambient");
	m_techniques[MM_DEFAULT][DEFERRED_TECHNIQUE_TILED_SHADOW]		= m_deferredShaders[MM_DEFAULT][DEFERRED_SHADER_TILED]		->LookupTechnique("tiled_shadow");
	m_techniques[MM_DEFAULT][DEFERRED_TECHNIQUE_AMBIENT_LIGHT]		= m_deferredShaders[MM_DEFAULT][DEFERRED_SHADER_AMBIENT]	->LookupTechnique("ambientLight");

#if DEVICE_MSAA
	if ( m_bSupportMSAAandNonMSAA )
	{
		m_techniques[MM_SINGLE][DEFERRED_TECHNIQUE_DIRECTIONAL]		= m_deferredShaders[MM_SINGLE][DEFERRED_SHADER_DIRECTIONAL]	->LookupTechnique("directional");
		m_techniques[MM_SINGLE][DEFERRED_TECHNIQUE_SSS_SKIN]		= m_deferredShaders[MM_SINGLE][DEFERRED_SHADER_LIGHTING]	->LookupTechnique("sss_skin");
#if MSAA_EDGE_PASS
		m_techniques[MM_TEXTURE_READS_ONLY][DEFERRED_TECHNIQUE_DIRECTIONAL]			= m_deferredShaders[MM_TEXTURE_READS_ONLY][DEFERRED_SHADER_DIRECTIONAL]	->LookupTechnique("directional");
		m_techniques[MM_TEXTURE_READS_ONLY][DEFERRED_TECHNIQUE_TILED_DIRECTIONAL]	= m_deferredShaders[MM_TEXTURE_READS_ONLY][DEFERRED_SHADER_TILED]		->LookupTechnique("tiled_directional");
		m_techniques[MM_TEXTURE_READS_ONLY][DEFERRED_TECHNIQUE_TILED_AMBIENT]		= m_deferredShaders[MM_TEXTURE_READS_ONLY][DEFERRED_SHADER_TILED]		->LookupTechnique("tiled_ambient");
		m_techniques[MM_TEXTURE_READS_ONLY][DEFERRED_TECHNIQUE_TILED_SHADOW]		= m_deferredShaders[MM_TEXTURE_READS_ONLY][DEFERRED_SHADER_TILED]		->LookupTechnique("tiled_shadow");
#endif //MSAA_EDGE_PASS
	}
#endif


	m_techniques[MM_DEFAULT][DEFERRED_TECHNIQUE_SSS_SKIN] = m_deferredShaders[MM_DEFAULT][DEFERRED_SHADER_LIGHTING]->LookupTechnique("sss_skin");

	m_techniques[MM_DEFAULT][DEFERRED_TECHNIQUE_VOLUME_INTERLEAVE_RECONSTRUCTION] = m_deferredShaders[MM_DEFAULT][DEFERRED_SHADER_VOLUME]->LookupTechnique("volume_Interleave_Reconstruction");
	m_techniques[MM_DEFAULT][DEFERRED_TECHNIQUE_SNOW] = m_deferredShaders[MM_DEFAULT][DEFERRED_SHADER_LIGHTING]->LookupTechnique("snow");

	m_techniques[MM_DEFAULT][DEFERRED_TECHNIQUE_FOLIAGE_PREPROCESS] = m_deferredShaders[MM_DEFAULT][DEFERRED_SHADER_LIGHTING]->LookupTechnique("foliage_preprocess");

	// Deferred volume lighting techniques
	m_shaderLightShaftVolumeTechniques[LIGHTSHAFT_VOLUMETYPE_SHAFT] = m_deferredShaders[MM_DEFAULT][DEFERRED_SHADER_VOLUME]->LookupTechnique("volumeShaft_SHAFT");
	m_shaderLightShaftVolumeTechniques[LIGHTSHAFT_VOLUMETYPE_CYLINDER] = m_deferredShaders[MM_DEFAULT][DEFERRED_SHADER_VOLUME]->LookupTechnique("volumeShaft_CYLINDER");

	// Try to find the default deferred technique group.
	m_deferredTechniqueGroupId = grmShaderFx::FindTechniqueGroupId("deferred");

	m_deferredAlphaClipTechniqueGroupId = grmShaderFx::FindTechniqueGroupId("deferredalphaclip");
	m_deferredSSAAlphaClipTechniqueGroupId = grmShaderFx::FindTechniqueGroupId("deferredsubsamplealphaclip");
	m_deferredSubSampleAlphaTechniqueGroupId = grmShaderFx::FindTechniqueGroupId("deferredsubsamplealpha");
	m_deferredSubSampleWriteAlphaTechniqueGroupId = grmShaderFx::FindTechniqueGroupId("deferredsubsamplewritealpha");

	// cutscene optimisation technique
	m_deferredCutsceneTechniqueGroupId = grmShaderFx::FindTechniqueGroupId("deferredcutscene");

	Assert(-1 != m_deferredTechniqueGroupId);
	Assert(-1 != m_deferredCutsceneTechniqueGroupId);
	Assert(-1 != m_deferredAlphaClipTechniqueGroupId);
	Assert(-1 != m_deferredSSAAlphaClipTechniqueGroupId);
	Assert(-1 != m_deferredSubSampleAlphaTechniqueGroupId);
	Assert(-1 != m_deferredSubSampleWriteAlphaTechniqueGroupId);
	// Setup render state blocks
}

void DeferredLighting::InitRenderStates()
{
		// Default entry state
	grcDepthStencilStateDesc defaultStateDS;
	defaultStateDS.DepthFunc = rage::FixupDepthDirection(grcRSV::CMP_LESSEQUAL);
	m_defaultState_DS = grcStateBlock::CreateDepthStencilState(defaultStateDS,"m_defaultState_DS");

	// Exit state
	grcRasterizerStateDesc defaultExitStateR;
	defaultExitStateR.CullMode = grcRSV::CULL_NONE;
	m_defaultExitState_R = grcStateBlock::CreateRasterizerState(defaultExitStateR,"m_defaultExitState_R");
	
	grcBlendStateDesc defaultExitStateB;
	defaultExitStateB.BlendRTDesc[0].BlendEnable = TRUE;
	defaultExitStateB.BlendRTDesc[0].DestBlend = grcRSV::BLEND_INVSRCALPHA;
	defaultExitStateB.BlendRTDesc[0].SrcBlend = grcRSV::BLEND_SRCALPHA;
	m_defaultExitState_B = grcStateBlock::CreateBlendState(defaultExitStateB,"m_defaultExitState_B");
	
	grcDepthStencilStateDesc defaultExitStateDS;
	defaultExitStateDS.DepthFunc = rage::FixupDepthDirection(grcRSV::CMP_LESSEQUAL);
	m_defaultExitState_DS = grcStateBlock::CreateDepthStencilState(defaultExitStateDS,"m_defaultExitState_DS");

	const int colorMask = grcRSV::COLORWRITEENABLE_RGB;

	// Geometry Pass
	grcDepthStencilStateDesc geometryPassDS;
	geometryPassDS.StencilEnable = TRUE;
	geometryPassDS.FrontFace.StencilPassOp = grcRSV::STENCILOP_REPLACE;
	geometryPassDS.BackFace.StencilPassOp = grcRSV::STENCILOP_REPLACE;
	geometryPassDS.FrontFace.StencilFunc = grcRSV::CMP_ALWAYS;
	geometryPassDS.BackFace.StencilFunc = grcRSV::CMP_ALWAYS;
	geometryPassDS.DepthFunc = rage::FixupDepthDirection(grcRSV::CMP_LESSEQUAL);
	m_geometryPass_DS = grcStateBlock::CreateDepthStencilState(geometryPassDS,"m_geometryPass_DS");

	grcBlendStateDesc geometryPassB;
	geometryPassB.IndependentBlendEnable = TRUE;
	geometryPassB.BlendRTDesc[GBUFFER_RT_0].RenderTargetWriteMask = colorMask;
	geometryPassB.BlendRTDesc[GBUFFER_RT_1].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_ALL;
	geometryPassB.BlendRTDesc[GBUFFER_RT_2].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_ALL;
	geometryPassB.BlendRTDesc[GBUFFER_RT_3].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_ALL;
	m_geometryPass_B = grcStateBlock::CreateBlendState(geometryPassB,"m_geometryPass_B");

	// Decal Pass
	grcDepthStencilStateDesc decalPassDS;
	decalPassDS.DepthFunc = rage::FixupDepthDirection(grcRSV::CMP_LESSEQUAL);
	decalPassDS.DepthWriteMask = FALSE;
	m_decalPass_DS = grcStateBlock::CreateDepthStencilState(decalPassDS,"m_decalPass_DS");

	grcBlendStateDesc decalPassB;
	decalPassB.IndependentBlendEnable = TRUE;
	decalPassB.BlendRTDesc[GBUFFER_RT_0].BlendEnable = TRUE;
	decalPassB.BlendRTDesc[GBUFFER_RT_0].DestBlend = grcRSV::BLEND_INVSRCALPHA;
	decalPassB.BlendRTDesc[GBUFFER_RT_0].SrcBlend = grcRSV::BLEND_SRCALPHA;
	decalPassB.BlendRTDesc[GBUFFER_RT_0].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_RGB;
	decalPassB.BlendRTDesc[GBUFFER_RT_1].BlendEnable = TRUE;
	decalPassB.BlendRTDesc[GBUFFER_RT_1].DestBlend = grcRSV::BLEND_INVSRCALPHA;
	decalPassB.BlendRTDesc[GBUFFER_RT_1].SrcBlend = grcRSV::BLEND_SRCALPHA;	
	decalPassB.BlendRTDesc[GBUFFER_RT_1].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_NONE;
	decalPassB.BlendRTDesc[GBUFFER_RT_2].BlendEnable = TRUE;
	decalPassB.BlendRTDesc[GBUFFER_RT_2].DestBlend = grcRSV::BLEND_INVSRCALPHA;
	decalPassB.BlendRTDesc[GBUFFER_RT_2].SrcBlend = grcRSV::BLEND_SRCALPHA;
	decalPassB.BlendRTDesc[GBUFFER_RT_2].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_RED + grcRSV::COLORWRITEENABLE_GREEN;
	decalPassB.BlendRTDesc[GBUFFER_RT_3].BlendEnable = TRUE;
	decalPassB.BlendRTDesc[GBUFFER_RT_3].DestBlend = grcRSV::BLEND_INVSRCALPHA;
	decalPassB.BlendRTDesc[GBUFFER_RT_3].SrcBlend = grcRSV::BLEND_SRCALPHA;
	decalPassB.BlendRTDesc[GBUFFER_RT_3].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_RGB;
	m_decalPass_B = grcStateBlock::CreateBlendState(decalPassB,"m_decalPass_B");

	// Fade Pass
	grcDepthStencilStateDesc fadePassDS;
	fadePassDS.StencilEnable = TRUE;
	fadePassDS.StencilWriteMask = 0xF;
	fadePassDS.FrontFace.StencilPassOp = grcRSV::STENCILOP_REPLACE;
	fadePassDS.BackFace.StencilPassOp = fadePassDS.FrontFace.StencilPassOp;
	fadePassDS.DepthFunc = rage::FixupDepthDirection(grcRSV::CMP_LESSEQUAL);
	m_fadePass_DS = grcStateBlock::CreateDepthStencilState(fadePassDS,"m_fadePass_DS");
#if MSAA_EDGE_PROCESS_FADING
	fadePassDS.StencilWriteMask |= EDGE_FLAG;
	m_fadePassAA_DS = grcStateBlock::CreateDepthStencilState(fadePassDS,"m_fadePassAA_DS");
#endif //MSAA_EDGE_PROCESS_FADING

	grcBlendStateDesc fadePassB;
	fadePassB.IndependentBlendEnable = TRUE;
	fadePassB.BlendRTDesc[GBUFFER_RT_0].RenderTargetWriteMask = colorMask;
	fadePassB.BlendRTDesc[GBUFFER_RT_1].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_ALL;//RGB
	fadePassB.BlendRTDesc[GBUFFER_RT_2].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_ALL;//RGB
	fadePassB.BlendRTDesc[GBUFFER_RT_3].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_ALL;//NONE
	m_fadePass_B = grcStateBlock::CreateBlendState(fadePassB,"m_fadePass_B");

	// Cutout Pass
	grcBlendStateDesc cutoutPassB;
	cutoutPassB.IndependentBlendEnable = TRUE;
	cutoutPassB.BlendRTDesc[GBUFFER_RT_0].RenderTargetWriteMask = colorMask;
	cutoutPassB.BlendRTDesc[GBUFFER_RT_1].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_ALL;//RGB; // go ahead and overwrite twiddle
	cutoutPassB.BlendRTDesc[GBUFFER_RT_2].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_ALL;
	cutoutPassB.BlendRTDesc[GBUFFER_RT_3].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_ALL; //RGB - need for fresnel 
	cutoutPassB.AlphaToCoverageEnable = (GRCDEVICE.GetMSAA() ? TRUE : FALSE);
#if DEVICE_EQAA
	cutoutPassB.AlphaToMaskOffsets = GRCDEVICE.GetMSAA().m_uFragments == 1 && false ?	//enable dithering for S/1 modes? No
		grcRSV::ALPHATOMASKOFFSETS_DITHERED : grcRSV::ALPHATOMASKOFFSETS_SOLID;
#endif //DEVICE_EQAA
	m_cutoutPass_B = grcStateBlock::CreateBlendState(cutoutPassB,"m_cutoutPass_B");

	grcDepthStencilStateDesc cutoutPassDS;
	cutoutPassDS.StencilEnable = TRUE;
	cutoutPassDS.FrontFace.StencilPassOp = grcRSV::STENCILOP_REPLACE;
	cutoutPassDS.BackFace = cutoutPassDS.FrontFace;
	cutoutPassDS.DepthFunc = rage::FixupDepthDirection(grcRSV::CMP_LESSEQUAL);
	m_cutoutPass_DS= grcStateBlock::CreateDepthStencilState(cutoutPassDS,"m_cutoutPass_DS");

#if STENCIL_VEHICLE_INTERIOR
	grcDepthStencilStateDesc treePassDS;

	treePassDS.DepthFunc = rage::FixupDepthDirection(grcRSV::CMP_LESS);
	treePassDS.StencilEnable = TRUE;
	treePassDS.StencilReadMask = DEFERRED_MATERIAL_INTERIOR_VEH;
	treePassDS.StencilWriteMask = DEFERRED_MATERIAL_NOT_INTERIOR_VEH;
	treePassDS.FrontFace.StencilFunc = grcRSV::CMP_EQUAL;
	treePassDS.FrontFace.StencilFailOp = grcRSV::STENCILOP_KEEP;
	treePassDS.FrontFace.StencilPassOp = grcRSV::STENCILOP_REPLACE;
	treePassDS.BackFace = treePassDS.FrontFace;

	m_treePass_DS = grcStateBlock::CreateDepthStencilState(treePassDS);
#endif

	// Default
	grcDepthStencilStateDesc defaultPassDS;
	defaultPassDS.StencilEnable = TRUE;
	defaultPassDS.FrontFace.StencilPassOp = grcRSV::STENCILOP_REPLACE;
	defaultPassDS.BackFace.StencilPassOp = grcRSV::STENCILOP_REPLACE;
	defaultPassDS.DepthFunc = rage::FixupDepthDirection(grcRSV::CMP_LESSEQUAL);
	m_defaultPass_DS = grcStateBlock::CreateDepthStencilState(defaultPassDS);

	grcBlendStateDesc defaultPassB;
	defaultPassB.IndependentBlendEnable = TRUE;
	defaultPassB.BlendRTDesc[GBUFFER_RT_0].RenderTargetWriteMask = colorMask;
	defaultPassB.BlendRTDesc[GBUFFER_RT_1].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_ALL;
	defaultPassB.BlendRTDesc[GBUFFER_RT_2].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_ALL;
	defaultPassB.BlendRTDesc[GBUFFER_RT_3].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_ALL;
	m_defaultPass_B = grcStateBlock::CreateBlendState(defaultPassB, "m_defaultPass_B");

	// Ambient Directional Pass
	grcBlendStateDesc directionalPassBlendState;
	directionalPassBlendState.BlendRTDesc[0].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_RGB;
	m_directionalPass_B = grcStateBlock::CreateBlendState(directionalPassBlendState,"m_directionalPass_B");

	grcDepthStencilStateDesc directionalFullPassDepthStencilState;
	directionalFullPassDepthStencilState.DepthEnable = FALSE;
	directionalFullPassDepthStencilState.DepthWriteMask = FALSE;
	directionalFullPassDepthStencilState.DepthFunc = grcRSV::CMP_LESS;
	directionalFullPassDepthStencilState.StencilEnable = TRUE;
	directionalFullPassDepthStencilState.StencilWriteMask = 0;
	directionalFullPassDepthStencilState.StencilReadMask = 0xfB;
	directionalFullPassDepthStencilState.FrontFace.StencilFunc = grcRSV::CMP_NOTEQUAL;
	directionalFullPassDepthStencilState.BackFace.StencilFunc = grcRSV::CMP_NOTEQUAL;
	m_directionalFullPass_DS = grcStateBlock::CreateDepthStencilState(directionalFullPassDepthStencilState,"m_directionalFullPass_DS");

	grcDepthStencilStateDesc directionalAmbientPassDepthStencilState;
	directionalAmbientPassDepthStencilState.DepthEnable = FALSE;
	directionalAmbientPassDepthStencilState.DepthWriteMask = FALSE;
	directionalAmbientPassDepthStencilState.DepthFunc = grcRSV::CMP_LESS;
	directionalAmbientPassDepthStencilState.StencilEnable = TRUE;
	directionalAmbientPassDepthStencilState.StencilWriteMask = 0;
	directionalAmbientPassDepthStencilState.StencilReadMask = 0xff;
	directionalAmbientPassDepthStencilState.FrontFace.StencilFunc = grcRSV::CMP_NOTEQUAL;
	directionalAmbientPassDepthStencilState.BackFace.StencilFunc = grcRSV::CMP_NOTEQUAL;
	m_directionalAmbientPass_DS = grcStateBlock::CreateDepthStencilState(directionalAmbientPassDepthStencilState,"m_directionalAmbientPass_DS");

	grcDepthStencilStateDesc directionalNoStencilPassDepthStencilState;
	directionalNoStencilPassDepthStencilState.DepthEnable = FALSE;
	directionalNoStencilPassDepthStencilState.DepthWriteMask = FALSE;
	directionalNoStencilPassDepthStencilState.DepthFunc = grcRSV::CMP_LESS;
	m_directionalNoStencilPass_DS = grcStateBlock::CreateDepthStencilState(directionalNoStencilPassDepthStencilState,"m_directionalNoStencilPass_DS");

#if MSAA_EDGE_PASS
	grcDepthStencilStateDesc directionalEdgePassDSS;
	directionalEdgePassDSS.DepthEnable = FALSE;
	directionalEdgePassDSS.DepthWriteMask = FALSE;
	directionalEdgePassDSS.StencilEnable = TRUE;
	directionalEdgePassDSS.StencilWriteMask = 0;
	directionalEdgePassDSS.StencilReadMask = EDGE_FLAG;
	directionalEdgePassDSS.FrontFace.StencilFunc = grcRSV::CMP_EQUAL;
	directionalEdgePassDSS.BackFace.StencilFunc = grcRSV::CMP_EQUAL;
	m_directionalEdgeMaskEqualPass_DS = grcStateBlock::CreateDepthStencilState(directionalEdgePassDSS, "m_directionalEdgeMaskEqualPass_DS");
	directionalEdgePassDSS.StencilReadMask = EDGE_FLAG | 0x7;
	m_directionalEdgeAllEqualPass_DS = grcStateBlock::CreateDepthStencilState(directionalEdgePassDSS, "m_directionalEdgeAllEqualPass_DS");
	directionalEdgePassDSS.FrontFace.StencilFunc = grcRSV::CMP_NOTEQUAL;
	directionalEdgePassDSS.BackFace.StencilFunc = grcRSV::CMP_NOTEQUAL;
	m_directionalEdgeNotEqualPass_DS = grcStateBlock::CreateDepthStencilState(directionalEdgePassDSS, "m_directionalEdgeNotEqualPass_DS");
	directionalEdgePassDSS.StencilReadMask = EDGE_FLAG | 0x1;
	directionalEdgePassDSS.FrontFace.StencilFunc = grcRSV::CMP_EQUAL;
	directionalEdgePassDSS.BackFace.StencilFunc = grcRSV::CMP_EQUAL;
	m_directionalEdgeBit0EqualPass_DS = grcStateBlock::CreateDepthStencilState(directionalEdgePassDSS, "m_directionalEdgeBit0EqualPass_DS");
	directionalEdgePassDSS.StencilReadMask = EDGE_FLAG | 0x4;
	m_directionalEdgeBit2EqualPass_DS = grcStateBlock::CreateDepthStencilState(directionalEdgePassDSS, "m_directionalEdgeBit2EqualPass_DS");
#endif //MSAA_EDGE_PASS

	grcRasterizerStateDesc directionalPassRasterizerState;
	directionalPassRasterizerState.CullMode = grcRSV::CULL_NONE;
	m_directionalPass_R = grcStateBlock::CreateRasterizerState(directionalPassRasterizerState,"m_directionalPass_R");

	// Foliage PrePass:
	grcDepthStencilStateDesc foliagePrePassDSS;
	foliagePrePassDSS.DepthEnable = FALSE;
	foliagePrePassDSS.DepthWriteMask = FALSE;
	foliagePrePassDSS.DepthFunc = grcRSV::CMP_LESS;
	foliagePrePassDSS.StencilEnable = TRUE;
	foliagePrePassDSS.StencilReadMask = DEFERRED_MATERIAL_TYPE_MASK;
	foliagePrePassDSS.StencilWriteMask = DEFERRED_MATERIAL_SPECIALBIT;
	foliagePrePassDSS.FrontFace.StencilFunc			= grcRSV::CMP_EQUAL;
	foliagePrePassDSS.FrontFace.StencilFailOp		= grcRSV::STENCILOP_KEEP;
	foliagePrePassDSS.FrontFace.StencilDepthFailOp	= grcRSV::STENCILOP_KEEP;
	foliagePrePassDSS.FrontFace.StencilPassOp		= grcRSV::STENCILOP_REPLACE;
	foliagePrePassDSS.BackFace.StencilFunc			= grcRSV::CMP_EQUAL;
	foliagePrePassDSS.BackFace.StencilFailOp		= grcRSV::STENCILOP_KEEP;
	foliagePrePassDSS.BackFace.StencilDepthFailOp	= grcRSV::STENCILOP_KEEP;
	foliagePrePassDSS.BackFace.StencilPassOp		= grcRSV::STENCILOP_REPLACE;
	m_foliagePrePass_DS = grcStateBlock::CreateDepthStencilState(foliagePrePassDSS,"m_foliagePrePass_DS");

	grcDepthStencilStateDesc foliageMainPassDSS;
	foliageMainPassDSS.DepthEnable = FALSE;
	foliageMainPassDSS.DepthWriteMask = FALSE;
	foliageMainPassDSS.DepthFunc = grcRSV::CMP_LESS;
	foliageMainPassDSS.StencilEnable = TRUE;
	foliageMainPassDSS.StencilReadMask = DEFERRED_MATERIAL_TREE|DEFERRED_MATERIAL_SPECIALBIT;
	foliageMainPassDSS.StencilWriteMask = DEFERRED_MATERIAL_SPECIALBIT;
	foliageMainPassDSS.FrontFace.StencilFunc		= grcRSV::CMP_EQUAL;
	foliageMainPassDSS.FrontFace.StencilFailOp		= grcRSV::STENCILOP_KEEP;
	foliageMainPassDSS.FrontFace.StencilDepthFailOp	= grcRSV::STENCILOP_KEEP;
	foliageMainPassDSS.FrontFace.StencilPassOp		= grcRSV::STENCILOP_ZERO;
	foliageMainPassDSS.BackFace.StencilFunc			= grcRSV::CMP_EQUAL;
	foliageMainPassDSS.BackFace.StencilFailOp		= grcRSV::STENCILOP_KEEP;
	foliageMainPassDSS.BackFace.StencilDepthFailOp	= grcRSV::STENCILOP_KEEP;
	foliageMainPassDSS.BackFace.StencilPassOp		= grcRSV::STENCILOP_ZERO;
	m_foliageMainPass_DS = grcStateBlock::CreateDepthStencilState(foliageMainPassDSS,"m_foliageMainPass_DS");

	// Foliage Pass
	grcBlendStateDesc foliagePassBlendState;
	foliagePassBlendState.BlendRTDesc[0].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_ALL;  
	m_foliagePass_B = grcStateBlock::CreateBlendState(foliagePassBlendState,"m_foliagePass_B");
	
	grcDepthStencilStateDesc foliagePassDepthStencilState;
	foliagePassDepthStencilState.DepthEnable = FALSE;
	foliagePassDepthStencilState.DepthWriteMask = FALSE;
	foliagePassDepthStencilState.DepthFunc = grcRSV::CMP_LESS;
	foliagePassDepthStencilState.StencilEnable = TRUE;
	foliagePassDepthStencilState.StencilWriteMask = 0;
	foliagePassDepthStencilState.StencilReadMask = 0x07;
	foliagePassDepthStencilState.FrontFace.StencilFunc = grcRSV::CMP_EQUAL;
	foliagePassDepthStencilState.BackFace.StencilFunc = grcRSV::CMP_EQUAL;
	m_foliagePass_DS = grcStateBlock::CreateDepthStencilState(foliagePassDepthStencilState,"m_foliagePass_DS");

	grcDepthStencilStateDesc foliageNoStencilPassDepthStencilState;
	foliageNoStencilPassDepthStencilState.DepthEnable = FALSE;
	foliageNoStencilPassDepthStencilState.DepthWriteMask = FALSE;
	foliageNoStencilPassDepthStencilState.DepthFunc = grcRSV::CMP_LESS;
	m_foliageNoStencilPass_DS = grcStateBlock::CreateDepthStencilState(foliageNoStencilPassDepthStencilState,"m_foliageNoStencilPass_DS");

	grcRasterizerStateDesc foliagePassRasterizerState;
	foliagePassRasterizerState.CullMode = grcRSV::CULL_NONE;
	m_foliagePass_R = grcStateBlock::CreateRasterizerState(foliagePassRasterizerState,"m_foliagePass_R");

	// Skin pass
	grcBlendStateDesc skinPassBlendState;
	skinPassBlendState.BlendRTDesc[0].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_RGB;
	skinPassBlendState.BlendRTDesc[0].BlendEnable = true;
	skinPassBlendState.BlendRTDesc[0].SrcBlend = grcRSV::BLEND_SRCALPHA;
	skinPassBlendState.BlendRTDesc[0].DestBlend = grcRSV::BLEND_INVSRCALPHA;
	skinPassBlendState.BlendRTDesc[0].BlendOp = grcRSV::BLENDOP_ADD;
	m_skinPass_B = grcStateBlock::CreateBlendState(skinPassBlendState,"m_skinPass_B");

	grcDepthStencilStateDesc skinPassDepthStencilState;
	skinPassDepthStencilState.DepthEnable = FALSE;
	skinPassDepthStencilState.DepthWriteMask = FALSE;
	skinPassDepthStencilState.DepthFunc = grcRSV::CMP_LESS;
	skinPassDepthStencilState.StencilEnable = TRUE;
	skinPassDepthStencilState.StencilWriteMask = 0;
	skinPassDepthStencilState.StencilReadMask = 0x07;
	skinPassDepthStencilState.FrontFace.StencilFunc = grcRSV::CMP_EQUAL;
	skinPassDepthStencilState.BackFace.StencilFunc = grcRSV::CMP_EQUAL;
	m_skinPass_DS = grcStateBlock::CreateDepthStencilState(skinPassDepthStencilState,"m_skinPass_DS");

	skinPassDepthStencilState.FrontFace.StencilFunc = grcRSV::CMP_NOTEQUAL;
	skinPassDepthStencilState.BackFace.StencilFunc = grcRSV::CMP_NOTEQUAL;
	m_skinPass_UI_DS = grcStateBlock::CreateDepthStencilState(skinPassDepthStencilState,"m_skinPass_UI_DS");

	grcDepthStencilStateDesc dsdTest;
	dsdTest.DepthEnable = TRUE;
	dsdTest.DepthWriteMask = FALSE;
	dsdTest.DepthFunc = grcRSV::CMP_ALWAYS;
	dsdTest.StencilEnable = TRUE;
	dsdTest.StencilReadMask = 0xff;
	dsdTest.StencilWriteMask = 0x00;
	dsdTest.FrontFace.StencilFunc = grcRSV::CMP_NOTEQUAL;
	dsdTest.BackFace.StencilFunc = grcRSV::CMP_NOTEQUAL;
	m_skinPassForward_DS = grcStateBlock::CreateDepthStencilState(dsdTest,"m_skinPassForward_DS");

	grcDepthStencilStateDesc skinNoStencilPassDepthStencilState;
	skinNoStencilPassDepthStencilState.DepthEnable = FALSE;
	skinNoStencilPassDepthStencilState.DepthWriteMask = FALSE;
	skinNoStencilPassDepthStencilState.DepthFunc = grcRSV::CMP_LESS;
	m_skinNoStencilPass_DS = grcStateBlock::CreateDepthStencilState(skinNoStencilPassDepthStencilState,"m_skinNoStencilPass_DS");

	grcRasterizerStateDesc skinPassRasterizerState;
	skinPassRasterizerState.CullMode = grcRSV::CULL_NONE;
	m_skinPass_R = grcStateBlock::CreateRasterizerState(skinPassRasterizerState,"m_skinPass_R");

	// Tiled lighting
	grcDepthStencilStateDesc tiledLightingDepthStencilState;
	tiledLightingDepthStencilState.DepthEnable = FALSE;
	tiledLightingDepthStencilState.DepthWriteMask = FALSE;
	tiledLightingDepthStencilState.DepthFunc = grcRSV::CMP_LESS;
	tiledLightingDepthStencilState.StencilEnable = TRUE;
	tiledLightingDepthStencilState.StencilWriteMask = 0;
	tiledLightingDepthStencilState.StencilReadMask = 0xfB;
	tiledLightingDepthStencilState.FrontFace.StencilFunc = grcRSV::CMP_NOTEQUAL;
	tiledLightingDepthStencilState.BackFace.StencilFunc = grcRSV::CMP_NOTEQUAL;
	m_tiledLighting_DS = grcStateBlock::CreateDepthStencilState(tiledLightingDepthStencilState,"m_tiledLighting_DS");

	// Sub Sample Alpha
	grcBlendStateDesc bsDescSingleSampleSSA;
	bsDescSingleSampleSSA.BlendRTDesc[GBUFFER_RT_0].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_ALL;
	bsDescSingleSampleSSA.BlendRTDesc[GBUFFER_RT_1].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_ALL;
	bsDescSingleSampleSSA.BlendRTDesc[GBUFFER_RT_2].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_ALL;
	bsDescSingleSampleSSA.BlendRTDesc[GBUFFER_RT_3].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_ALL; // should be RGB?
	// we could use alpha-to-coverage for SSA, however it does not produce better results (see B#590459)
	bsDescSingleSampleSSA.AlphaToCoverageEnable = (0 && GRCDEVICE.GetMSAA() ? TRUE : FALSE);

	m_SingleSampleSSABlendState = grcStateBlock::CreateBlendState(bsDescSingleSampleSSA,"m_SingleSampleSSABlendState");
	Assert(m_SingleSampleSSABlendState != grcStateBlock::BS_Invalid);

	grcBlendStateDesc bsDescSSA = bsDescSingleSampleSSA;

	bsDescSSA.IndependentBlendEnable = TRUE;
	bsDescSSA.BlendRTDesc[0].RenderTargetWriteMask	= grcRSV::COLORWRITEENABLE_RGB;

	m_SSABlendState = grcStateBlock::CreateBlendState(bsDescSSA,"m_SSABlendState");
	Assert(m_SSABlendState != grcStateBlock::BS_Invalid);

#if MSAA_EDGE_PASS
	m_edgePass_R = grcStateBlock::RS_NoBackfaceCull;
	grcDepthStencilStateDesc edgePassDsd;
	edgePassDsd.DepthEnable = false;
	edgePassDsd.StencilEnable = true;
	edgePassDsd.StencilWriteMask = EDGE_FLAG;
	edgePassDsd.StencilReadMask = 0xFF;
	edgePassDsd.FrontFace.StencilPassOp = grcRSV::STENCILOP_REPLACE;
	edgePassDsd.BackFace.StencilPassOp  = grcRSV::STENCILOP_REPLACE;
# if MSAA_EDGE_PROCESS_FADING
	// no benefit since we are discarding in the shader -> stencil goes after PS
	edgePassDsd.StencilReadMask = EDGE_FLAG;
	edgePassDsd.FrontFace.StencilFunc = grcRSV::CMP_NOTEQUAL;
# else
	edgePassDsd.StencilReadMask = 0xFF;
# endif //MSAA_EDGE_PROCESS_FADING
	edgePassDsd.FrontFace.StencilPassOp = grcRSV::STENCILOP_REPLACE;
	edgePassDsd.BackFace = edgePassDsd.FrontFace;
	m_edgePass_DS = grcStateBlock::CreateDepthStencilState(edgePassDsd, "m_edgePass_DS");
	m_edgePass_B = grcStateBlock::BS_Default_WriteMaskNone;
#endif //MSAA_EDGE_PASS

#if SSA_USES_CONDITIONALRENDER
	// Create the conditional query for SSA passes
	m_SSAConditionalQuery = GRCDEVICE.CreateConditionalQuery();
	Assert( m_SSAConditionalQuery != 0 );
#endif // SSA_USES_CONDITIONALRENDER
	
	// Other setup
	InitSetHiStencilCullState();
}

void DeferredLighting::TerminateRenderStates()
{
	grcStateBlock::DestroyDepthStencilState(m_defaultState_DS);
	grcStateBlock::DestroyRasterizerState(m_defaultExitState_R);
	grcStateBlock::DestroyBlendState(m_defaultExitState_B);
	grcStateBlock::DestroyDepthStencilState(m_defaultExitState_DS);
	grcStateBlock::DestroyDepthStencilState(m_geometryPass_DS);
	grcStateBlock::DestroyBlendState(m_geometryPass_B);
	grcStateBlock::DestroyDepthStencilState(m_decalPass_DS);
	grcStateBlock::DestroyBlendState(m_decalPass_B);
	grcStateBlock::DestroyDepthStencilState(m_fadePass_DS);
#if MSAA_EDGE_PROCESS_FADING
	grcStateBlock::DestroyDepthStencilState(m_fadePassAA_DS);
#endif
	grcStateBlock::DestroyBlendState(m_fadePass_B);
	grcStateBlock::DestroyBlendState(m_cutoutPass_B);
	grcStateBlock::DestroyDepthStencilState(m_cutoutPass_DS);
	grcStateBlock::DestroyDepthStencilState(m_treePass_DS);
	grcStateBlock::DestroyDepthStencilState(m_defaultPass_DS);
	grcStateBlock::DestroyBlendState(m_defaultPass_B);
	grcStateBlock::DestroyBlendState(m_directionalPass_B);
	grcStateBlock::DestroyDepthStencilState(m_directionalFullPass_DS);
	grcStateBlock::DestroyDepthStencilState(m_directionalAmbientPass_DS);
	grcStateBlock::DestroyDepthStencilState(m_directionalNoStencilPass_DS);
#if MSAA_EDGE_PASS
	grcStateBlock::DestroyDepthStencilState(m_directionalEdgeMaskEqualPass_DS);
	grcStateBlock::DestroyDepthStencilState(m_directionalEdgeAllEqualPass_DS);
	grcStateBlock::DestroyDepthStencilState(m_directionalEdgeNotEqualPass_DS);
	grcStateBlock::DestroyDepthStencilState(m_directionalEdgeBit0EqualPass_DS);
	grcStateBlock::DestroyDepthStencilState(m_directionalEdgeBit2EqualPass_DS);
	grcStateBlock::DestroyDepthStencilState(m_edgePass_DS);
#endif // MSAA_EDGE_PASS
	
	grcStateBlock::DestroyDepthStencilState(m_foliagePrePass_DS);
	grcStateBlock::DestroyDepthStencilState(m_foliageMainPass_DS);
	
	grcStateBlock::DestroyRasterizerState(m_directionalPass_R);
	grcStateBlock::DestroyBlendState(m_foliagePass_B);
	grcStateBlock::DestroyDepthStencilState(m_foliagePass_DS);
	grcStateBlock::DestroyDepthStencilState(m_foliageNoStencilPass_DS);
	grcStateBlock::DestroyRasterizerState(m_foliagePass_R);
	grcStateBlock::DestroyBlendState(m_skinPass_B);
	grcStateBlock::DestroyDepthStencilState(m_skinPass_DS);
	grcStateBlock::DestroyDepthStencilState(m_skinPass_UI_DS);
	grcStateBlock::DestroyDepthStencilState(m_skinPassForward_DS);
	grcStateBlock::DestroyDepthStencilState(m_skinNoStencilPass_DS);
	grcStateBlock::DestroyRasterizerState(m_skinPass_R);
	grcStateBlock::DestroyDepthStencilState(m_tiledLighting_DS);
	grcStateBlock::DestroyBlendState(m_SingleSampleSSABlendState);
	grcStateBlock::DestroyBlendState(m_SSABlendState);

	TerminateHiStencilCullState();
}

void DeferredLighting::Init()
{
	sysMemUseMemoryBucket b(MEMBUCKET_RENDER);

	InitShaders();
	InitRenderStates();

	GBuffer::Init(); 
	PuddlePassSingleton::Instantiate();
	
	m_volumeTexture = CShaderLib::LookupTexture("volume");
#if MSAA_EDGE_PASS
	grcTextureFactory::CreateParams params;
# if MSAA_EDGE_USE_DEPTH_COPY
	params.Format = grctfD32FS8;
	params.Multisample = GRCDEVICE.GetMSAA();
	m_edgeMarkDepthCopy = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "Depth Buffer Copy for EdgeMark pass", grcrtDepthBuffer,
			GRCDEVICE.GetWidth(), GRCDEVICE.GetHeight(), 40, &params, 0, m_edgeMarkDepthCopy);
# endif //MSAA_EDGE_USE_DEPTH_COPY
# if __DEV
	params.Format = grctfA8R8G8B8;
	params.Multisample = grcDevice::MSAA_None;
	m_edgeMarkDebugTarget = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "Edge Mark", grcrtPermanent,
		GRCDEVICE.GetWidth(), GRCDEVICE.GetHeight(), 32, &params, 0, m_edgeMarkDebugTarget);
# endif // __BANK
# if MSAA_EDGE_PROCESS_FADING
	params.Format = grctfL8;
	params.Multisample = grcDevice::MSAA_None;
	m_edgeCopyTarget = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "Edge Copy", grcrtPermanent,
		GRCDEVICE.GetWidth(), GRCDEVICE.GetHeight(), 8, &params, 0, m_edgeCopyTarget);
# endif //MSAA_EDGE_PROCESS_FADING
#endif //MSAA_EDGE_PASS
}


void DeferredLighting::PushCutscenePedTechnique(){
	Assert(m_prevDeferredPedTechnique == -1 );
	int currentTech = grmShaderFx::GetForcedTechniqueGroupId();
	if ( currentTech == m_deferredTechniqueGroupId)
	{		
		m_prevDeferredPedTechnique = m_deferredTechniqueGroupId;
		Assert(m_prevDeferredPedTechnique != -1 );
		grmShaderFx::SetForcedTechniqueGroupId( m_deferredCutsceneTechniqueGroupId );
	}
}
void DeferredLighting::PopCutscenePedTechnique(){
	if (m_prevDeferredPedTechnique != -1)
	{
		Assert( m_prevDeferredPedTechnique == m_deferredTechniqueGroupId);
		grmShaderFx::SetForcedTechniqueGroupId( m_prevDeferredPedTechnique );
		m_prevDeferredPedTechnique = -1;
	}
}

#if SSA_USES_CONDITIONALRENDER
void DeferredLighting::BeginSSAConditionalQuery() 
{ 
	PIXBegin(0,"SSA Pass"); 
	m_bSSAConditionalQueryIsValid = false;
	GRCDEVICE.BeginConditionalQuery(m_SSAConditionalQuery); 
}

void DeferredLighting::EndSSAConditionalQuery() 
{ 
	Assert(!m_bSSAConditionalQueryIsValid);
	GRCDEVICE.EndConditionalQuery(m_SSAConditionalQuery); 
	m_bSSAConditionalQueryIsValid = true; 
	PIXEnd(); // "SSA Pass"
}

void DeferredLighting::BeginSSAConditionalRender() 
{
	if (AssertVerify(m_bSSAConditionalQueryIsValid)) 
		GRCDEVICE.BeginConditionalRender(m_SSAConditionalQuery); 
}

void DeferredLighting::EndSSAConditionalRender() 
{ 
	if (AssertVerify(m_bSSAConditionalQueryIsValid)) 
		GRCDEVICE.EndConditionalRender(m_SSAConditionalQuery); 

	m_bSSAConditionalQueryIsValid = false;
}
#endif // SSA_USES_CONDITIONALRENDER

// ----------------------------------------------------------------------------------------------- //

#if !__FINAL
void DeferredLighting::SetDebugLightingParamsV(Vec4f_In params)
{
	m_deferredShaders[MM_DEFAULT][DEFERRED_SHADER_DIRECTIONAL]->SetVar(m_debugLightingParamsID, params);
}
#endif // !__FINAL

// ----------------------------------------------------------------------------------------------- //

#if !__FINAL
void DeferredLighting::SetDebugLightingParams()
{
	m_deferredShaders[MM_DEFAULT][DEFERRED_SHADER_DIRECTIONAL]->SetVar(m_debugLightingParamsID, Vec4f(
		1.0f, // not used anymore, but keep as 1 until everyone's rebuilt shaders
		BANK_SWITCH(DebugLighting::GetDeferredDiffuseLightAmount(), 1.0f),
		1.0f, // not used anymore, but keep as 1 until everyone's rebuilt shaders
		0.0f
	));
}
#endif // !__FINAL

#if __XENON
static bool debugSwitchZPassModeChange = true;
#endif

// ----------------------------------------------------------------------------------------------- //
#if __PPU && !__SPU
// ----------------------------------------------------------------------------------------------- //

char* DeferredLighting::GetTextureOffsetGcm(grcTextureGCM *texgcm)
{
	char* finalTexAddr=NULL;

	CellGcmTexture *cellTex = (CellGcmTexture*)texgcm->GetTexturePtr();
	Assert(cellTex);
	cellGcmIoOffsetToAddress(cellTex->offset, (void**)&finalTexAddr);
	Assert(finalTexAddr);

	return(finalTexAddr);
}		

// ----------------------------------------------------------------------------------------------- //
#endif
// ----------------------------------------------------------------------------------------------- //

template<typename varType>
void DeferredLighting::SetDeferredVar(eDeferredShaders deferredShader, eDeferredShaderVars deferredShaderVar, const varType &var, MultisampleMode aaMode)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));		// this command is forbidden outwith the render thread

#if DEVICE_MSAA
	if (!m_bSupportMSAAandNonMSAA)
		aaMode = MM_DEFAULT;
#endif

	m_deferredShaders[aaMode][deferredShader]->SetVar(m_deferredShaderVars[aaMode][deferredShader][deferredShaderVar], var);
}

// forcing compiler to generate code for grcTexture*
typedef grcTexture* PTexture;
typedef grcRenderTarget* PRenderTarget;
template void DeferredLighting::SetDeferredVar(eDeferredShaders,eDeferredShaderVars,const PTexture&, MultisampleMode aaMode);
template void DeferredLighting::SetDeferredVar(eDeferredShaders,eDeferredShaderVars,const PRenderTarget&, MultisampleMode aaMode);

// ----------------------------------------------------------------------------------------------- //

template<typename varType>
void DeferredLighting::SetDeferredGlobalVar(eDeferredShaderGlobalVars deferredShaderGlobalVar, const varType &var)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));		// this command is forbidden outwith the render thread
	grcEffect::SetGlobalVar(m_deferredShaderGlobalVars[deferredShaderGlobalVar], var);
}

// forcing compiler to generate code for grcTexture*
typedef grcTexture* PTexture;
typedef grcRenderTarget* PRenderTarget;
template void DeferredLighting::SetDeferredGlobalVar(eDeferredShaderGlobalVars,const PTexture&);
template void DeferredLighting::SetDeferredGlobalVar(eDeferredShaderGlobalVars,const PRenderTarget&);

// ----------------------------------------------------------------------------------------------- //

template<typename varType>
void DeferredLighting::SetDeferredVarArray(eDeferredShaders deferredShader, eDeferredShaderVars deferredShaderVar, varType var, u32 count, MultisampleMode aaMode)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));		// this command is forbidden outwith the render thread

#if DEVICE_MSAA
	if (!m_bSupportMSAAandNonMSAA)
		aaMode = MM_DEFAULT;
#endif

	m_deferredShaders[aaMode][deferredShader]->SetVar(m_deferredShaderVars[aaMode][deferredShader][deferredShaderVar], var, count);
}

// ----------------------------------------------------------------------------------------------- //

void DeferredLighting::DefaultState()
{
	grcStateBlock::SetRasterizerState(grcStateBlock::RS_Default);
	grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
	grcStateBlock::SetDepthStencilState(m_defaultState_DS);
}

// ----------------------------------------------------------------------------------------------- //

void DeferredLighting::DefaultExitState()
{
	grcStateBlock::SetDepthStencilState(m_defaultExitState_DS);
	grcStateBlock::SetBlendState(m_defaultExitState_B);
	grcStateBlock::SetRasterizerState(m_defaultExitState_R);

	grcViewport::SetCurrent(NULL);
}

// ----------------------------------------------------------------------------------------------- //

/******************************************************************************************************************
Following GBuffer Samplers are set to global because there are many light shaders that use them.
If they are local, there is a good chance that they get alloted different sampler slots in each of
the shaders. By declaring them global, we assign specific slots to these samplers, and so it will 
always be in the same slot for all shaders that use these samplers. Now we can set them once before
we start rendering the lights, instead of resetting them for each light source. We save around 0.15ms 
on the 360 with this change. Following is how the slots are assigned:

GBuffer 0			= s7
GBuffer 1			= s8
GBuffer 2			= s9
GBuffer 3			= s10
GBuffer Stencil		= s11
GBuffer Depth		= s12

They have the word "Global" appended to their names because this will make sure there is no conflict with other
shaders that use the same name for a local sampler. 
********************************************************************************************************************/
void DeferredLighting::SetShaderGBufferTarget(GBufferRT index, const grcTexture* texture)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));

	switch (index)
	{
	case GBUFFER_RT_0: // Color texture
	case GBUFFER_RT_1:
	case GBUFFER_RT_2:
	case GBUFFER_RT_3:
		grcEffect::SetGlobalVar(m_deferredShaderGlobalVars[DEFERRED_SHADER_GBUFFER_0_GLOBAL + index-GBUFFER_RT_0], texture);
		break;
	case GBUFFER_DEPTH: // Depth texture:
		grcEffect::SetGlobalVar(m_deferredShaderGlobalVars[DEFERRED_SHADER_GBUFFER_DEPTH_GLOBAL], texture);
		break;
#if __XENON || __D3D11 || RSG_ORBIS
	case GBUFFER_STENCIL: // Stencil texture:
		grcEffect::SetGlobalVar(m_deferredShaderGlobalVars[DEFERRED_SHADER_GBUFFER_STENCIL_GLOBAL], texture);
		break;
#endif //__XENON || __D3D11 || RSG_ORBIS
#if DEVICE_EQAA
	case GBUFFER_FMASK_0: // Fragment mask texture:
	case GBUFFER_FMASK_1:
	case GBUFFER_FMASK_2:
	case GBUFFER_FMASK_3:
		grcEffect::SetGlobalVar(m_deferredShaderGlobalVars[DEFERRED_SHADER_GBUFFER_0_FMASK_GLOBAL + index-GBUFFER_FMASK_0], texture);
		break;
#endif // DEVICE_MSAA
	default:
		Assertf(0, "Unknown Gbuffer target %d", index);
	}
}

// ----------------------------------------------------------------------------------------------- //

void DeferredLighting::SetShaderGBufferTargets(WIN32PC_ONLY(bool bindDepthCopy))
{
	SetShaderGBufferTarget(GBUFFER_RT_0, GBuffer::GetTarget(GBUFFER_RT_0));
	SetShaderGBufferTarget(GBUFFER_RT_1, GBuffer::GetTarget(GBUFFER_RT_1));
	SetShaderGBufferTarget(GBUFFER_RT_2, GBuffer::GetTarget(GBUFFER_RT_2));
	SetShaderGBufferTarget(GBUFFER_RT_3, GBuffer::GetTarget(GBUFFER_RT_3));

#if __PPU
	SetShaderGBufferTarget(GBUFFER_DEPTH, CRenderTargets::GetDepthBufferAsColor());
#else // __PPU
#if RSG_PC
	if (GRCDEVICE.GetDxFeatureLevel() <= 1000 && bindDepthCopy)
		SetShaderGBufferTarget(GBUFFER_DEPTH, CRenderTargets::GetDepthBufferCopy());
	else
		SetShaderGBufferTarget(GBUFFER_DEPTH, CRenderTargets::GetDepthBuffer());
#else
	SetShaderGBufferTarget(GBUFFER_DEPTH, CRenderTargets::GetDepthBuffer());
#endif
#endif // __PPU

#if __XENON
	SetShaderGBufferTarget(GBUFFER_STENCIL, CRenderTargets::GetDepthBufferAsColor());
#elif __D3D11 || RSG_ORBIS
#if RSG_PC
	if (GRCDEVICE.GetDxFeatureLevel() <= 1000 && bindDepthCopy)
		SetShaderGBufferTarget(GBUFFER_STENCIL, CRenderTargets::GetDepthBufferCopy_Stencil());
	else
		SetShaderGBufferTarget(GBUFFER_STENCIL, CRenderTargets::GetDepthBuffer_Stencil());
#else
	SetShaderGBufferTarget(GBUFFER_STENCIL, CRenderTargets::GetDepthBuffer_Stencil());
#endif
#endif

#if DEVICE_EQAA
	for (GBufferRT i=GBUFFER_FMASK_0; GRCDEVICE.IsEQAA() && i<=GBUFFER_FMASK_3; i=static_cast<GBufferRT>(i+1))
	{
		SetShaderGBufferTarget(i, GBuffer::GetFragmentMaskTexture(i));
	}
#endif // DEVICE_EQAA
}

// ----------------------------------------------------------------------------------------------- //

void DeferredLighting::SetShaderSSAOTarget(eDeferredShaders deferredShaders)
{
	SetDeferredVar(deferredShaders, DEFERRED_SHADER_LIGHT_TEXTURE2,	SSAO::GetSSAOTexture());
}

// ----------------------------------------------------------------------------------------------- //

void DeferredLighting::SetShaderDepthTargetsForDeferredVolume(const grcTexture* depthBuffer)
{
	m_deferredShaders[MM_DEFAULT][DEFERRED_SHADER_VOLUME]->SetVar(m_shaderVolumeParameterID_deferredVolumeDepthBuffer, depthBuffer);
}

// ----------------------------------------------------------------------------------------------- //

void DeferredLighting::SetShaderTargetsForVolumeInterleaveReconstruction()
{
#if LIGHT_VOLUME_USE_LOW_RESOLUTION
		m_deferredShaders[MM_DEFAULT][DEFERRED_SHADER_VOLUME]->SetVar(m_shaderVolumeParameterID_deferredVolumeOffscreenBuffer, CRenderTargets::GetVolumeOffscreenBuffer());
		m_deferredShaders[MM_DEFAULT][DEFERRED_SHADER_VOLUME]->SetVar(m_shaderVolumeParameterID_deferredVolumeLowResDepthBuffer, CRenderTargets::GetDepthBufferQuarter());
#else
	m_deferredShaders[MM_DEFAULT][DEFERRED_SHADER_VOLUME]->SetVar(m_shaderVolumeParameterID_deferredVolumeOffscreenBuffer, CRenderTargets::GetVolumeOffscreenBuffer());
#endif
}
// ----------------------------------------------------------------------------------------------- //

void DeferredLighting::Shutdown()
{
#if SSA_USES_CONDITIONALRENDER
	GRCDEVICE.DestroyConditionalQuery(m_SSAConditionalQuery);
#endif // SSA_USES_CONDITIONALRENDER
}

// ----------------------------------------------------------------------------------------------- //

void DeferredLighting::Update()
{
	QualityModeSelect();
}
 
// ----------------------------------------------------------------------------------------------- //

grcEffectTechnique DeferredLighting::GetTechnique(const u32 lightType, MultisampleMode aaMode)
{
#if DEVICE_MSAA
	if (!m_bSupportMSAAandNonMSAA)
		aaMode = MM_DEFAULT;
#endif
	grcEffectTechnique *const t = m_techniques[aaMode];

	switch(lightType)
	{
		case LIGHT_TYPE_SPOT: return t[DEFERRED_TECHNIQUE_SPOTCM];
		case LIGHT_TYPE_POINT: return t[DEFERRED_TECHNIQUE_POINTCM];
		case LIGHT_TYPE_CAPSULE: return t[DEFERRED_TECHNIQUE_CAPSULE];
		case LIGHT_TYPE_DIRECTIONAL: return t[DEFERRED_TECHNIQUE_DIRECTIONAL];
		default: break;
	}

	Assertf(false, "Tried to look-up light technique for light %d, which doesn't exist", lightType);
	return grcetNONE;
}

// ----------------------------------------------------------------------------------------------- //

grcEffectTechnique DeferredLighting::GetTechnique(eDeferredTechnique deferredTechnique, MultisampleMode aaMode)
{
#if DEVICE_MSAA
	if (!m_bSupportMSAAandNonMSAA)
		aaMode = MM_DEFAULT;
#endif
	
	return m_techniques[aaMode][deferredTechnique];
}


// ----------------------------------------------------------------------------------------------- //

grmShader* DeferredLighting::GetShader(const u32 lightType)
{
	switch(lightType)
	{
		case LIGHT_TYPE_SPOT: return m_deferredShaders[MM_DEFAULT][DEFERRED_SHADER_SPOT];
		case LIGHT_TYPE_POINT: return m_deferredShaders[MM_DEFAULT][DEFERRED_SHADER_POINT];
		case LIGHT_TYPE_CAPSULE: return m_deferredShaders[MM_DEFAULT][DEFERRED_SHADER_CAPSULE];
		case LIGHT_TYPE_DIRECTIONAL: return m_deferredShaders[MM_DEFAULT][DEFERRED_SHADER_DIRECTIONAL];
		default:
		{
			Assertf(false, "Tried to look-up shader for light %d, which doesn't exist", lightType);
			return NULL;
		}
	}
}

// ----------------------------------------------------------------------------------------------- //

grmShader* DeferredLighting::GetShader(eDeferredShaders deferredShader, MultisampleMode aaMode)
{
#if DEVICE_MSAA
	if (!m_bSupportMSAAandNonMSAA)
		aaMode = MM_DEFAULT;
#endif
	
	return m_deferredShaders[aaMode][deferredShader];
}

// ----------------------------------------------------------------------------------------------- //

#if GENERATE_SHMOO
int DeferredLighting::GetShaderShmooIdx(eDeferredShaders deferredShader)
{
	return m_deferredShadersShmoos[deferredShader];
}
#endif

// ----------------------------------------------------------------------------------------------- //

eDeferredShaders DeferredLighting::GetShaderType(const u32 lightType)
{
	switch(lightType)
	{
		case LIGHT_TYPE_SPOT: return DEFERRED_SHADER_SPOT;
		case LIGHT_TYPE_POINT: return DEFERRED_SHADER_POINT;
		case LIGHT_TYPE_CAPSULE: return DEFERRED_SHADER_CAPSULE;
		case LIGHT_TYPE_DIRECTIONAL: return DEFERRED_SHADER_DIRECTIONAL;
		default:
		{
			Errorf("Tried to look-up shader type for light %d, which doesn't exist", lightType);
			return DEFERRED_SHADER_LIGHTING;
		}
	}
}

// ----------------------------------------------------------------------------------------------- //

grcEffectTechnique DeferredLighting::GetVolumeTechnique(const u32 lightType, const MultisampleMode aaMode)
{
	switch(lightType)
	{
		case LIGHT_TYPE_SPOT: return GetTechnique(DEFERRED_TECHNIQUE_SPOTCM_VOLUME, aaMode);
		case LIGHT_TYPE_POINT: return GetTechnique(DEFERRED_TECHNIQUE_POINTCM_VOLUME, aaMode);
		case LIGHT_TYPE_CAPSULE: return GetTechnique(DEFERRED_TECHNIQUE_CAPSULE_VOLUME, aaMode);
		default:
		{
			Assertf(false, "Tried to look-up volume light technique for light %d, which doesn't exist", lightType);
			return grcetNONE;
		}
	}
}

// ----------------------------------------------------------------------------------------------- //
s32 DeferredLighting::CalculatePass(const CLightSource *light, bool hasTexture, bool useShadows, bool useBackLighting )
{
	s32 pass = -1;

	switch(light->GetType())
	{
		case LIGHT_TYPE_POINT:
		case LIGHT_TYPE_SPOT:
		case LIGHT_TYPE_CAPSULE:
		{
			u32 passKey = light->GetPassKey();
			if (!useShadows) { passKey &= ~LIGHTFLAG_CAST_SHADOWS; }
			if (!hasTexture) { passKey &= ~LIGHTFLAG_TEXTURE_PROJECTION; }

			// Don't support textures or shadows lights for capsules
			if (light->GetType() == LIGHT_TYPE_CAPSULE)
			{
				passKey &= ~LIGHTFLAG_CAST_SHADOWS;
				passKey &= ~LIGHTFLAG_TEXTURE_PROJECTION;
			}
			// spot lights can do caustics. light above water & camera below water. 
			u8 extraFlag = 0;
			float waterLevel = Water::GetWaterLevel();
			if( 
				//Caustics spots
				(light->UsesCaustic() && 
				light->GetType() == LIGHT_TYPE_SPOT && 
				Water::IsCameraUnderwater()  && 
				(light->GetPosition().z-2.0f) > waterLevel 
				&& !(passKey & LIGHTFLAG_USE_VEHICLE_TWIN))
				//Soft shadowed lights
				|| (light->GetExtraFlags() & EXTRA_LIGHTFLAG_SOFT_SHADOWS)
				)
			{
				extraFlag = light->GetExtraFlags();
			}

			pass = CalculateLightPass(passKey, extraFlag);
			return pass;
		}

		case LIGHT_TYPE_DIRECTIONAL:
		{
			const bool noDirectional = (light->GetIntensity() == 0.0f) || (!useShadows);
			const bool useUnderwater = Water::IsCameraUnderwater();

			pass = DIRECTIONALPASS_STANDARD;   

			if (useUnderwater)
				pass = DIRECTIONALPASS_UNDERWATER;

			if (noDirectional)
				pass = DIRECTIONALPASS_AMBIENT;

			if (pass == DIRECTIONALPASS_STANDARD && useBackLighting)
				pass = Lights::m_bUseSSS ? DIRECTIONALPASS_SCATTER : DIRECTIONALPASS_BACKLIT;
#if __BANK
			if (TiledScreenCapture::IsEnabledOrthographic())
				pass = DIRECTIONALPASS_ORTHOGRAPHIC;
#endif // __BANK

			return pass;
		}

		case LIGHT_TYPE_AO_VOLUME:
		case LIGHT_TYPE_NONE:
		{
			return pass;		
		}
	}

	return pass;
}

// ----------------------------------------------------------------------------------------------- //

eLightPass DeferredLighting::CalculateLightPass(u32 passFlags, u8 extraFlags)
{
	bool softShadows = (extraFlags & EXTRA_LIGHTFLAG_SOFT_SHADOWS) != 0;

	if( extraFlags & EXTRA_LIGHTFLAG_CAUSTIC )
	{
		switch(passFlags)
		{
			//for spot lights (caustic & vehicle twin)
			case (0): return LIGHTPASS_STANDARD_CAUSTIC;
			case (LIGHTFLAG_CAST_SHADOWS): return softShadows ? LIGHTPASS_SHADOW_CAUSTIC_SOFT : LIGHTPASS_SHADOW_CAUSTIC;
			case (LIGHTFLAG_CAST_SHADOWS | LIGHTFLAG_TEXTURE_PROJECTION): return softShadows ? LIGHTPASS_SHADOW_TEXTURE_CAUSTIC_SOFT : LIGHTPASS_SHADOW_TEXTURE_CAUSTIC;
			case (LIGHTFLAG_CAST_SHADOWS | LIGHTFLAG_TEXTURE_PROJECTION | LIGHTFLAG_NO_SPECULAR): return softShadows ? LIGHTPASS_SHADOW_TEXTURE_FILLER_CAUSTIC_SOFT : LIGHTPASS_SHADOW_TEXTURE_FILLER_CAUSTIC;
			case (LIGHTFLAG_CAST_SHADOWS | LIGHTFLAG_TEXTURE_PROJECTION | LIGHTFLAG_EXTERIOR_ONLY): return softShadows ? LIGHTPASS_SHADOW_TEXTURE_EXTERIOR_CAUSTIC_SOFT : LIGHTPASS_SHADOW_TEXTURE_EXTERIOR_CAUSTIC;
			case (LIGHTFLAG_CAST_SHADOWS | LIGHTFLAG_TEXTURE_PROJECTION | LIGHTFLAG_EXTERIOR_ONLY | LIGHTFLAG_NO_SPECULAR): return softShadows ? LIGHTPASS_SHADOW_TEXTURE_EXTERIOR_FILLER_CAUSTIC_SOFT : LIGHTPASS_SHADOW_TEXTURE_EXTERIOR_FILLER_CAUSTIC;
			case (LIGHTFLAG_CAST_SHADOWS | LIGHTFLAG_TEXTURE_PROJECTION | LIGHTFLAG_INTERIOR_ONLY): return softShadows ? LIGHTPASS_SHADOW_TEXTURE_INTERIOR_CAUSTIC_SOFT : LIGHTPASS_SHADOW_TEXTURE_INTERIOR_CAUSTIC;
			case (LIGHTFLAG_CAST_SHADOWS | LIGHTFLAG_TEXTURE_PROJECTION | LIGHTFLAG_INTERIOR_ONLY | LIGHTFLAG_NO_SPECULAR): return softShadows ? LIGHTPASS_SHADOW_TEXTURE_INTERIOR_FILLER_CAUSTIC_SOFT : LIGHTPASS_SHADOW_TEXTURE_INTERIOR_FILLER_CAUSTIC;
			case (LIGHTFLAG_CAST_SHADOWS | LIGHTFLAG_EXTERIOR_ONLY): return softShadows ? LIGHTPASS_SHADOW_EXTERIOR_CAUSTIC_SOFT : LIGHTPASS_SHADOW_EXTERIOR_CAUSTIC;
			case (LIGHTFLAG_CAST_SHADOWS | LIGHTFLAG_NO_SPECULAR): return softShadows ? LIGHTPASS_SHADOW_FILLER_CAUSTIC_SOFT : LIGHTPASS_SHADOW_FILLER_CAUSTIC;
			case (LIGHTFLAG_CAST_SHADOWS | LIGHTFLAG_EXTERIOR_ONLY | LIGHTFLAG_NO_SPECULAR): return softShadows ? LIGHTPASS_SHADOW_EXTERIOR_FILLER_CAUSTIC_SOFT : LIGHTPASS_SHADOW_EXTERIOR_FILLER_CAUSTIC;
			case (LIGHTFLAG_NO_SPECULAR): return LIGHTPASS_FILLER_CAUSTIC;
			case (LIGHTFLAG_NO_SPECULAR | LIGHTFLAG_EXTERIOR_ONLY): return LIGHTPASS_FILLER_EXTERIOR_CAUSTIC;
			case (LIGHTFLAG_NO_SPECULAR | LIGHTFLAG_TEXTURE_PROJECTION): return LIGHTPASS_FILLER_TEXTURE_CAUSTIC;
			case (LIGHTFLAG_NO_SPECULAR | LIGHTFLAG_TEXTURE_PROJECTION | LIGHTFLAG_EXTERIOR_ONLY): return LIGHTPASS_FILLER_TEXTURE_EXTERIOR_CAUSTIC;
			case (LIGHTFLAG_EXTERIOR_ONLY): return LIGHTPASS_EXTERIOR_CAUSTIC;
			case (LIGHTFLAG_TEXTURE_PROJECTION): return LIGHTPASS_TEXTURE_CAUSTIC;
			case (LIGHTFLAG_TEXTURE_PROJECTION | LIGHTFLAG_EXTERIOR_ONLY): return LIGHTPASS_TEXTURE_EXTERIOR_CAUSTIC;

			default: 
			{
				#if __ASSERT
					Assertf(false, "Tried to create a caustic light type that we do not support (Pass - %d)", passFlags);
				#endif
				return LIGHTPASS_STANDARD;
			}
		}
	}
	else
	{
		switch(passFlags)
		{
			case 0: return LIGHTPASS_STANDARD;

			case (LIGHTFLAG_CAST_SHADOWS): return softShadows ? LIGHTPASS_SHADOW_SOFT : LIGHTPASS_SHADOW;
			case (LIGHTFLAG_CAST_SHADOWS | LIGHTFLAG_TEXTURE_PROJECTION): return softShadows ? LIGHTPASS_SHADOW_TEXTURE_SOFT : LIGHTPASS_SHADOW_TEXTURE;
			case (LIGHTFLAG_CAST_SHADOWS | LIGHTFLAG_TEXTURE_PROJECTION | LIGHTFLAG_NO_SPECULAR): return softShadows ? LIGHTPASS_SHADOW_TEXTURE_FILLER_SOFT : LIGHTPASS_SHADOW_TEXTURE_FILLER;
			case (LIGHTFLAG_CAST_SHADOWS | LIGHTFLAG_TEXTURE_PROJECTION | LIGHTFLAG_EXTERIOR_ONLY): return softShadows ? LIGHTPASS_SHADOW_TEXTURE_EXTERIOR_SOFT : LIGHTPASS_SHADOW_TEXTURE_EXTERIOR;
			case (LIGHTFLAG_CAST_SHADOWS | LIGHTFLAG_TEXTURE_PROJECTION | LIGHTFLAG_EXTERIOR_ONLY | LIGHTFLAG_NO_SPECULAR): return softShadows ? LIGHTPASS_SHADOW_TEXTURE_EXTERIOR_FILLER_SOFT : LIGHTPASS_SHADOW_TEXTURE_EXTERIOR_FILLER;
			case (LIGHTFLAG_CAST_SHADOWS | LIGHTFLAG_TEXTURE_PROJECTION | LIGHTFLAG_INTERIOR_ONLY): return softShadows ? LIGHTPASS_SHADOW_TEXTURE_INTERIOR_SOFT : LIGHTPASS_SHADOW_TEXTURE_INTERIOR;
			case (LIGHTFLAG_CAST_SHADOWS | LIGHTFLAG_TEXTURE_PROJECTION | LIGHTFLAG_INTERIOR_ONLY | LIGHTFLAG_NO_SPECULAR): return softShadows ? LIGHTPASS_SHADOW_TEXTURE_INTERIOR_FILLER_SOFT : LIGHTPASS_SHADOW_TEXTURE_INTERIOR_FILLER;
			case (LIGHTFLAG_CAST_SHADOWS | LIGHTFLAG_INTERIOR_ONLY): return softShadows ? LIGHTPASS_SHADOW_INTERIOR_SOFT : LIGHTPASS_SHADOW_INTERIOR;
			case (LIGHTFLAG_CAST_SHADOWS | LIGHTFLAG_EXTERIOR_ONLY): return softShadows ? LIGHTPASS_SHADOW_EXTERIOR_SOFT : LIGHTPASS_SHADOW_EXTERIOR;
			case (LIGHTFLAG_CAST_SHADOWS | LIGHTFLAG_NO_SPECULAR): return softShadows ? LIGHTPASS_SHADOW_FILLER_SOFT : LIGHTPASS_SHADOW_FILLER;
			case (LIGHTFLAG_CAST_SHADOWS | LIGHTFLAG_EXTERIOR_ONLY | LIGHTFLAG_NO_SPECULAR): return softShadows ? LIGHTPASS_SHADOW_EXTERIOR_FILLER_SOFT : LIGHTPASS_SHADOW_EXTERIOR_FILLER;
			case (LIGHTFLAG_CAST_SHADOWS | LIGHTFLAG_INTERIOR_ONLY | LIGHTFLAG_NO_SPECULAR): return softShadows ? LIGHTPASS_SHADOW_INTERIOR_FILLER_SOFT : LIGHTPASS_SHADOW_INTERIOR_FILLER;

			case (LIGHTFLAG_NO_SPECULAR): return LIGHTPASS_FILLER;
			case (LIGHTFLAG_NO_SPECULAR | LIGHTFLAG_INTERIOR_ONLY): return LIGHTPASS_FILLER_INTERIOR;
			case (LIGHTFLAG_NO_SPECULAR | LIGHTFLAG_EXTERIOR_ONLY): return LIGHTPASS_FILLER_EXTERIOR;
			case (LIGHTFLAG_NO_SPECULAR | LIGHTFLAG_TEXTURE_PROJECTION): return LIGHTPASS_FILLER_TEXTURE;
			case (LIGHTFLAG_NO_SPECULAR | LIGHTFLAG_TEXTURE_PROJECTION | LIGHTFLAG_INTERIOR_ONLY): return LIGHTPASS_FILLER_TEXTURE_INTERIOR;
			case (LIGHTFLAG_NO_SPECULAR | LIGHTFLAG_TEXTURE_PROJECTION | LIGHTFLAG_EXTERIOR_ONLY): return LIGHTPASS_FILLER_TEXTURE_EXTERIOR;

			case (LIGHTFLAG_INTERIOR_ONLY): return LIGHTPASS_INTERIOR;

			case (LIGHTFLAG_EXTERIOR_ONLY): return LIGHTPASS_EXTERIOR;

			case (LIGHTFLAG_TEXTURE_PROJECTION): return LIGHTPASS_TEXTURE;
			case (LIGHTFLAG_TEXTURE_PROJECTION | LIGHTFLAG_INTERIOR_ONLY): return LIGHTPASS_TEXTURE_INTERIOR;
			case (LIGHTFLAG_TEXTURE_PROJECTION | LIGHTFLAG_EXTERIOR_ONLY): return LIGHTPASS_TEXTURE_EXTERIOR;

			case (LIGHTFLAG_USE_VEHICLE_TWIN): return LIGHTPASS_VEHICLE_TWIN_STANDARD;
			case (LIGHTFLAG_USE_VEHICLE_TWIN | LIGHTFLAG_CAST_SHADOWS): return softShadows ? LIGHTPASS_VEHICLE_TWIN_SHADOW_SOFT : LIGHTPASS_VEHICLE_TWIN_SHADOW;
			case (LIGHTFLAG_USE_VEHICLE_TWIN | LIGHTFLAG_TEXTURE_PROJECTION): return LIGHTPASS_VEHICLE_TWIN_TEXTURE;
			case (LIGHTFLAG_USE_VEHICLE_TWIN | LIGHTFLAG_CAST_SHADOWS | LIGHTFLAG_TEXTURE_PROJECTION): return softShadows ? LIGHTPASS_VEHICLE_TWIN_SHADOW_TEXTURE_SOFT : LIGHTPASS_VEHICLE_TWIN_SHADOW_TEXTURE;

			default: 
			{
				#if __ASSERT
				char buffer[512];
				sprintf(buffer, "");
				if ((passFlags & (1 << 0 )) != 0) sprintf(buffer, "%s | %s", buffer, "Interior only");
				if ((passFlags & (1 << 1 )) != 0) sprintf(buffer, "%s | %s", buffer, "Exterior only");
				if ((passFlags & (1 << 5 )) != 0) sprintf(buffer, "%s | %s", buffer, "Texture projection");
				if ((passFlags & (1 << 6 )) != 0) sprintf(buffer, "%s | %s", buffer, "Shadow");
				if ((passFlags & (1 << 13)) != 0) sprintf(buffer, "%s | %s", buffer, "No specular");
				if ((passFlags & (1 << 27)) != 0) sprintf(buffer, "%s | %s", buffer, "Twin vehicle");
				Assertf(false, "Light with %s not supported (Pass - %d)", buffer, passFlags);
				#endif
				
				return LIGHTPASS_STANDARD;
			}
		}
	}
}

// ----------------------------------------------------------------------------------------------- //

ScalarV LightBoundsPixelCoveragePercent(const spdAABB& lightBounds)
{
//	const Vec3V bmin = lightBounds.GetMin();
//	const Vec3V bmax = lightBounds.GetMax();
	const Vec3V extents = lightBounds.GetExtent();
	const ScalarV volume = extents.GetX() * extents.GetY() * extents.GetZ();

	const ScalarV radiusSphere = Pow(volume, ScalarV(V_THIRD));

	const grcViewport* grcVP = gVpMan.GetCurrentGameGrcViewport();
	const Vec3V eye = grcVP->GetCameraPosition();
//	const Vec3V eyeDir = grcVP->GetCameraMtx().c();

	const Mat44V worldToScreenMtx = grcVP->GetCompositeMtx();
	const Vec2V screenResolution = Vec2V(grcVP->GetScreenMtx().GetCol3().GetXY());

	
	// compute roughly how large the light is in screen pixels
	return screenResolution.GetX() * radiusSphere / (ScalarVFromF32(grcVP->GetTanHFOV()) * 
		Abs(Dist(eye, lightBounds.GetCenter())));

	
}

// ----------------------------------------------------------------------------------------------- //

eLightVolumePass DeferredLighting::CalculateVolumePass(const CLightSource *light, bool useShadows, bool cameraInLight)
{
	//Debug tool for switching between shadowed and unshadowed volumes
	BANK_ONLY( useShadows = DebugLighting::m_ForceUnshadowedVolumeLightTechnique ? false : useShadows );

	eLightVolumePass volumePass;
	const s32 flags = light->GetFlags();
	bool useBackFaceTechnique = false;
		
	if (light->GetType() == LIGHT_TYPE_SPOT) 
	{
		if(light->GetConeCosOuterAngle() < BANK_SWITCH(DebugLighting::m_MinSpotAngleForBackFaceForce, LIGHT_VOLUME_BACKFACE_TECHNIQUE_MIN_ANGLE))
		{
			// Disable front face technique for really wide spot lights because it just does not look good
			// Triangles in front face becomes really large, and since we perform the attenuation in the vertex
			// shader, a lot of lighting info goes missing if the triangle is close to camera.
			useBackFaceTechnique =  true;
		}
		else
		{
			useBackFaceTechnique = cameraInLight;
		}
	}
	else
	{
		//use back face technique for point lights
		useBackFaceTechnique = true;
	}

	// Using shadow pass only for spot or point lights
	if ((light->GetType() == LIGHT_TYPE_SPOT || light->GetType() == LIGHT_TYPE_POINT) && useShadows && 
		(flags & (LIGHTFLAG_CAST_STATIC_GEOM_SHADOWS | LIGHTFLAG_CAST_DYNAMIC_GEOM_SHADOWS)) != 0)
	{
		volumePass = useBackFaceTechnique ? LIGHTVOLUMEPASS_SHADOW : LIGHTVOLUMEPASS_OUTSIDE_SHADOW;
	}
	else
	{
		volumePass =  useBackFaceTechnique ? LIGHTVOLUMEPASS_STANDARD : LIGHTVOLUMEPASS_OUTSIDE;
	}


#if SUPPORT_HQ_VOLUMETRIC_LIGHTS
	if ( CSettingsManager::GetInstance().GetSettings().m_graphics.m_ShadowQuality == CSettings::Ultra)
	{
		volumePass = static_cast<eLightVolumePass>( volumePass + LIGHTVOLUMEPASS_STANDARD_HQ );
	}
#endif

	return volumePass;
}

void DeferredLighting::SetDirectionalRenderState(eDeferredShaders deferredShader, const MultisampleMode aaMode)
{
	// Setup ambient globals
	Lights::m_lightingGroup.SetAmbientLightingGlobals();

	// Set the light params into the shader.
	const CLightSource& dirLight = Lights::GetRenderDirectionalLight();

	// Setup the directional light
	SetLightShaderParams(deferredShader, &dirLight, NULL, 1.0f, false, false, aaMode);
	SetProjectionShaderParams(deferredShader, aaMode);

	// Setup shadowing variables
	CRenderPhaseCascadeShadowsInterface::SetSharedShaderVars();	
}

// ----------------------------------------------------------------------------------------------- //
#if __BANK
// ----------------------------------------------------------------------------------------------- //

const char* GetPassName(const u32 passIndex, const bool noDirLight)
{
	if (noDirLight)
	{
		switch(passIndex)
		{
			case 0: return "Ambient + Reflections";
			case 1: return "Ambient only";
			default: return "Unknown";
		}
	}
	else
	{
		switch(passIndex)
		{
			case 0: return "Dir | Spec | Refl | Amb";
			case 1: return "Refl | Amb";
			case 2: return "Amb";
			case 3: return "Dir | Amb";
			case 4: return "Dir | Spec | Amb";
			case 5: return "Dir | Spec | Refl | Penumb | Amb";
			case 6: return "Dir | Penumb | Amb";
			case 7: return "Dir | Spec | Penumb | Amb";
			default: return "Unknown";
		}
	}
}

// ----------------------------------------------------------------------------------------------- //
#endif
// ----------------------------------------------------------------------------------------------- //

void DeferredLighting::RenderTiledDirectionalLight(const bool noDirLight, const MultisampleMode aaMode, const EdgeMode em)
{
#if TILED_LIGHTING_ENABLED

	SetDirectionalRenderState(DEFERRED_SHADER_TILED, aaMode);
	SetProjectionShaderParams(DEFERRED_SHADER_TILED, aaMode);
	DeferredLighting::SetShaderGBufferTargets();

	grcStateBlock::SetStates(grcStateBlock::RS_NoBackfaceCull, grcStateBlock::DSS_IgnoreDepth, grcStateBlock::BS_Default);
#if MSAA_EDGE_PASS
	grcStateBlock::SetDepthStencilState(
		em == EM_IGNORE ? grcStateBlock::DSS_IgnoreDepth : m_directionalEdgeMaskEqualPass_DS,
		em == EM_EDGE0 ? EDGE_FLAG : 0);
#else
	(void)em;
#endif //MSAA_EDGE_PASS

	// Render tiles
	GRCDEVICE.SetVertexDeclaration(CTiledLighting::GetTileVertDecl());
	GRCDEVICE.SetStreamSource(0, *CTiledLighting::GetTileVertBuffer(), 0, CTiledLighting::GetTileVertBuffer()->GetVertexStride());
#if TILED_LIGHTING_INSTANCED_TILES
	GRCDEVICE.SetStreamSource(1, *CTiledLighting::GetTileInstanceBuffer(), 0, CTiledLighting::GetTileInstanceBuffer()->GetVertexStride());
#else
	GRCDEVICE.SetStreamSource(1, *CTiledLighting::GetTileInfoVertBuffer(), 0, CTiledLighting::GetTileInfoVertBuffer()->GetVertexStride());
#endif //TILED_LIGHTING_INSTANCED_TILES

	u32 passStart = 0;
	u32 passEnd = 5;
	grcEffectTechnique currentTechnique = m_techniques[aaMode][DEFERRED_TECHNIQUE_TILED_DIRECTIONAL];

	if (noDirLight)
	{
		passStart = 0;
		passEnd = 2;
		currentTechnique = m_techniques[aaMode][DEFERRED_TECHNIQUE_TILED_AMBIENT];
	}

	Assert(m_shaderDownsampledDepthSampler != grcevNONE);
	m_deferredShaders[aaMode][DEFERRED_SHADER_TILED]->SetVar(m_shaderDownsampledDepthSampler,CTiledLighting::GetClassificationTexture());
	SetDeferredVar(DEFERRED_SHADER_TILED, DEFERRED_SHADER_LIGHT_TEXTURE2,	SSAO::GetSSAOTexture(), aaMode);

	if (m_deferredShaders[aaMode][DEFERRED_SHADER_TILED]->BeginDraw(grmShader::RMC_DRAW, true, currentTechnique))
	{
		for (u32 i = passStart; i < passEnd; i++)
		{
			#if __BANK && !__GAMETOOL
				const char* passName = GetPassName(i, noDirLight);
				PF_PUSH_MARKER(passName);
			#endif
			
			GENSHMOO_ONLY(ShmooHandling::BeginShmoo(m_deferredShadersShmoos[DEFERRED_SHADER_TILED], m_deferredShaders[aaMode][DEFERRED_SHADER_TILED], i));

			m_deferredShaders[aaMode][DEFERRED_SHADER_TILED]->Bind(i); 
			#if TILED_LIGHTING_INSTANCED_TILES
				GRCDEVICE.DrawInstancedPrimitive(drawTris, 6, CTiledLighting::GetTotalNumTiles(), 0, 0);
			#else
				GRCDEVICE.DrawPrimitive(drawTris,	0, CTiledLighting::GetTotalNumTiles()*6);
			#endif //TILED_LIGHTING_INSTANCED_TILES
			m_deferredShaders[aaMode][DEFERRED_SHADER_TILED]->UnBind(); 
			
			GENSHMOO_ONLY(ShmooHandling::EndShmoo(m_deferredShadersShmoos[DEFERRED_SHADER_TILED]));
			#if __BANK && !__GAMETOOL
				PF_POP_MARKER();
			#endif
		}

		m_deferredShaders[aaMode][DEFERRED_SHADER_TILED]->EndDraw();
	}

	GRCDEVICE.ClearStreamSource(1);
	GRCDEVICE.ClearStreamSource(0);

#else
	(void)noDirLight;
	(void)em;
#endif
}


void DeferredLighting::RenderTiledShadow()
{
#if TILED_LIGHTING_ENABLED

	SetDirectionalRenderState(DEFERRED_SHADER_TILED);

	// Render tiles
	// Set world to worldToShadowSpace
	const grcViewport& worldToShadowViewport = CRenderPhaseCascadeShadowsInterface::GetCascadeViewport(0);
	Mat34V worldToShadowMtx = worldToShadowViewport.GetCameraMtx();
	grcWorldMtx( worldToShadowMtx );

	GRCDEVICE.SetVertexDeclaration(CTiledLighting::GetTileVertDecl());
	GRCDEVICE.SetStreamSource(0, *CTiledLighting::GetTileVertBuffer(), 0, CTiledLighting::GetTileVertBuffer()->GetVertexStride());
	#if TILED_LIGHTING_INSTANCED_TILES
		GRCDEVICE.SetStreamSource(1, *CTiledLighting::GetTileInstanceBuffer(), 0, CTiledLighting::GetTileInstanceBuffer()->GetVertexStride());
	#else
		GRCDEVICE.SetStreamSource(1, *CTiledLighting::GetTileInfoVertBuffer(), 0, CTiledLighting::GetTileInfoVertBuffer()->GetVertexStride());
	#endif //TILED_LIGHTING_INSTANCED_TILES

	grmShader* tiledShader = m_deferredShaders[MM_DEFAULT][DEFERRED_SHADER_TILED];
	Assert(m_shaderDownsampledDepthSampler != grcevNONE);
	tiledShader->SetVar(m_shaderDownsampledDepthSampler,CTiledLighting::GetClassificationTexture());

	if (tiledShader->BeginDraw(grmShader::RMC_DRAW, true,  m_techniques[MM_DEFAULT][DEFERRED_TECHNIQUE_TILED_SHADOW]))
	{
		GENSHMOO_ONLY(ShmooHandling::BeginShmoo(m_deferredShadersShmoos[DEFERRED_SHADER_TILED], tiledShader, 0));

		tiledShader->Bind(0);
		#if TILED_LIGHTING_INSTANCED_TILES
			GRCDEVICE.DrawInstancedPrimitive(drawTris, 6, CTiledLighting::GetTotalNumTiles(), 0, 0);
		#else
			GRCDEVICE.DrawPrimitive(drawTris,	0, CTiledLighting::GetTotalNumTiles()*6);
		#endif //TILED_LIGHTING_INSTANCED_TILES
		tiledShader->UnBind(); 
		GENSHMOO_ONLY(ShmooHandling::EndShmoo(m_deferredShadersShmoos[DEFERRED_SHADER_TILED]));
		tiledShader->EndDraw();
	}

#if !(__D3D11 || RSG_ORBIS)
	GRCDEVICE.ClearStreamSource(1);
#endif // !(__D3D11 || RSG_ORBIS)
	GRCDEVICE.ClearStreamSource(0);

#endif
}


// ----------------------------------------------------------------------------------------------- //

void DeferredLighting::RenderDirectionalLight(s32 pass, const bool setSSAO, const MultisampleMode aaMode)
{
	SetDeferredVar(DEFERRED_SHADER_DIRECTIONAL, DEFERRED_SHADER_LIGHT_TEXTURE,	Water::GetNoiseTexture(), aaMode);
	SetDeferredVar(DEFERRED_SHADER_DIRECTIONAL, DEFERRED_SHADER_LIGHT_TEXTURE1,	Water::GetCausticTexture(), aaMode);
	SetDeferredVar(DEFERRED_SHADER_DIRECTIONAL, DEFERRED_SHADER_LIGHT_TEXTURE2,	(setSSAO) ? SSAO::GetSSAOTexture() : grcTexture::NoneWhite, aaMode);

#if REFLECTION_CUBEMAP_SAMPLING
	CRenderPhaseReflection::SetReflectionMap();
#endif

	if (ShaderUtil::StartShader("Directional Light", GetShader(DEFERRED_SHADER_DIRECTIONAL, aaMode), GetTechnique(DEFERRED_TECHNIQUE_DIRECTIONAL, aaMode), pass)) 
	{
		Render2DDeferredLightingRect(0.0f, 0.0f, 1.0f, 1.0f);
		ShaderUtil::EndShader(GetShader(DEFERRED_SHADER_DIRECTIONAL, aaMode));
	}
}

// ----------------------------------------------------------------------------------------------- //

void DeferredLighting::Render2DDeferredLightingRect(const float x, const float y, const float w, const float h)
{
	float x0 = (x);
	float y0 = (y);
	float x1 = (x + w);
	float y1 = (y + h);
	Color32 white(255, 255, 255, 255);

#if RSG_XENON || RSG_ORBIS
	grcBegin(drawRects,3);
	grcVertex(x0, y0, 0.0f, 0.0f, 0.0f, -1.0f, white, x0, y0);
	grcVertex(x1, y0, 0.0f, 0.0f, 0.0f, -1.0f, white, x1, y0);
	grcVertex(x0, y1, 0.0f, 0.0f, 0.0f, -1.0f, white, x0, y1);
	grcEnd();
#else
	grcBegin(drawTriStrip,4);
	grcVertex(x0, y0, 0.0f, 0.0f, 0.0f, -1.0f, white, x0, y0);
	grcVertex(x1, y0, 0.0f, 0.0f, 0.0f, -1.0f, white, x1, y0);
	grcVertex(x0, y1, 0.0f, 0.0f, 0.0f, -1.0f, white, x0, y1);
	grcVertex(x1, y1, 0.0f, 0.0f, 0.0f, -1.0f, white, x1, y1);
	grcEnd();
#endif
}

void DeferredLighting::Render2DDeferredLightingRect(const float x, const float y, const float w, const float h,
													const float u, const float v, const float uW,const float vH)
{
	float x0 = (x);
	float y0 = (y);
	float x1 = (x + w);
	float y1 = (y + h);

	float u0 = u;
	float v0 = v;
	float u1 = u + uW;
	float v1 = v + vH;
	Color32 white(255, 255, 255, 255);

#if RSG_XENON || RSG_ORBIS
	grcBegin(drawRects,3);
	grcVertex(x0, y0, 0.0f, 0.0f, 0.0f, -1.0f, white, u0, v0);
	grcVertex(x1, y0, 0.0f, 0.0f, 0.0f, -1.0f, white, u1, v0);
	grcVertex(x0, y1, 0.0f, 0.0f, 0.0f, -1.0f, white, u0, v1);
	grcEnd();
#else
	grcBegin(drawTriStrip,4);
	grcVertex(x0, y0, 0.0f, 0.0f, 0.0f, -1.0f, white, u0, v0);
	grcVertex(x1, y0, 0.0f, 0.0f, 0.0f, -1.0f, white, u1, v0);
	grcVertex(x0, y1, 0.0f, 0.0f, 0.0f, -1.0f, white, u0, v1);
	grcVertex(x1, y1, 0.0f, 0.0f, 0.0f, -1.0f, white, u1, v1);
	grcEnd();
#endif
}
// ----------------------------------------------------------------------------------------------- //

void DeferredLighting::PrepareGeometryPass()
{
	// Make sure that the render state is set to make any later
	// rendering behaviour predictable
	grcBindTexture(NULL);
	
	grcStateBlock::SetBlendState(m_geometryPass_B);
	grcStateBlock::SetRasterizerState(grcStateBlock::RS_Default);

	if (DeferredLighting__m_useGeomPassStencil == false)
	{
		grcStateBlock::SetDepthStencilState(CShaderLib::DSS_Default_Invert);
	}
	else
	{
		grcStateBlock::SetDepthStencilState(m_geometryPass_DS, DEFERRED_MATERIAL_DEFAULT);
	}

	// Bind and enable vertex & fragment programs.
	m_previousGroupId = grmShaderFx::GetForcedTechniqueGroupId();
	grmShaderFx::SetForcedTechniqueGroupId(m_deferredTechniqueGroupId);
	CShaderLib::SetGlobalEmissiveScale(1.0f);

#if EFFECT_STENCIL_REF_MASK
	// limit the in-shader DSS blocks to not affect the edge flag
	grcEffect::SetStencilRefMask(static_cast<u8>(~EDGE_FLAG));
#endif //EFFECT_STENCIL_REF_MASK

}// end of PrepareGeometryPass()...

// ----------------------------------------------------------------------------------------------- //
#if __XENON
// ----------------------------------------------------------------------------------------------- //

void DeferredLighting::PrepareGeometryPassTiled()
{
	//lock all gbuffers
	GBuffer::LockTargets();

	D3DVECTOR4 ClearColor = { 0.0f, 0.0f, 0.0f, 0.0f };

	D3DRECT pTilingRects[NUM_TILES];
	GBuffer::CalculateTiles(pTilingRects, NUM_TILES);

	u32 flags = D3DTILING_SKIP_FIRST_TILE_CLEAR;

#if __BANK
	if (DebugDeferred::m_switchZPass)
		flags |= D3DTILING_ONE_PASS_ZPASS|D3DTILING_FIRST_TILE_INHERITS_DEPTH_BUFFER;

	if (debugSwitchZPassModeChange != DebugDeferred::m_switchZPass)
	{
		if (DebugDeferred::m_switchZPass) 
			GRCDEVICE.GetCurrent()->SetScreenExtentQueryMode(D3DSEQM_CULLED);
		else
			GRCDEVICE.GetCurrent()->SetScreenExtentQueryMode(D3DSEQM_PRECLIP);

	#if __DEV
		debugSwitchZPassModeChange = DebugDeferred::m_switchZPass;
	#endif
	}
#endif

	GRCDEVICE.GetCurrent()->BeginTiling( flags, NUM_TILES, pTilingRects, &ClearColor, 0.0f, 0L );

	DWORD dwClearFlags = D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL;
	if ( PostFX::UseSubSampledAlpha() )
	{
		dwClearFlags |= D3DCLEAR_TARGET0;
	}

	GRCDEVICE.GetCurrent()->SetPredication( D3DPRED_TILE(0) );
	GRCDEVICE.GetCurrent()->Clear( 0, NULL, dwClearFlags, D3DCOLOR_COLORVALUE(0,0,0,0), 0.0f, 0L );
	GRCDEVICE.GetCurrent()->SetPredication( 0 );

#if __BANK
	if (DebugDeferred::m_switchZPass)
		GRCDEVICE.GetCurrent()->BeginZPass(0);
#endif

	
	// viewport gets trashed by BeginTiling
	D3DVIEWPORT9 vp;
	CHECK_HRESULT(GRCDEVICE.GetCurrent()->GetViewport(&vp));
	vp.MinZ = GRCDEVICE.FixViewportDepth(vp.MinZ);
	vp.MaxZ = GRCDEVICE.FixViewportDepth(vp.MaxZ);
	CHECK_HRESULT(GRCDEVICE.GetCurrent()->SetViewport(&vp));

	PrepareGeometryPass();
}

// ----------------------------------------------------------------------------------------------- //

void DeferredLighting::FinishGeometryPassTiled(bool bSkipGBuffer2LastTileResolve /*=false*/)
{
	FinishGeometryPass();

	#if __BANK
	if (DebugDeferred::m_switchZPass)
		GRCDEVICE.GetCurrent()->EndZPass();
	#endif

	// resolve
	D3DVECTOR4 ClearColor      = { 0.0f, 0.0f, 0.0f, 0.0f };

	D3DRECT pTilingRects[NUM_TILES];
	GBuffer::CalculateTiles(pTilingRects, NUM_TILES);

	grcRenderTarget **targets = GBuffer::GetTargets();

	for( int i = 0; i < NUM_TILES; ++i )
	{
		// Set predication to tile i.
		GRCDEVICE.GetCurrent()->SetPredication( D3DPRED_TILE( i ) );

		// Destination point is the upper left corner of the tiling rect.
		D3DPOINT* pDestPoint = (D3DPOINT*)&pTilingRects[i];

		// Resolve render targets 0, 1, 2 and 3, also clear them.
		CHECK_HRESULT( GRCDEVICE.GetCurrent()->Resolve( D3DRESOLVE_RENDERTARGET0 | D3DRESOLVE_FRAGMENT0 | D3DRESOLVE_CLEARRENDERTARGET, &pTilingRects[i], static_cast<D3DTexture*>(targets[0]->GetTexturePtr()), pDestPoint, 0, 0, &ClearColor, 0.0f, 0L, NULL ) );
		if (bSkipGBuffer2LastTileResolve && i==(NUM_TILES-1)) 
		{
			// Skip the last Gbuffer2 tile resolve, we'll get it after cascaded shadow reveal. This remains resident in EDRAM until it is resolved
			// during cascaded shadow reveal. We enable basic protection so that we don't accidentally step on this EDRAM region in the mean time.
			IDirect3DSurface9 *pSurface2=NULL;
			CHECK_HRESULT( GRCDEVICE.GetCurrent()->GetRenderTarget(2,&pSurface2) );
			PROTECT_EDRAM_SURFACE( pSurface2 );
			SAFE_RELEASE(pSurface2);
		}
		else
		{
			CHECK_HRESULT( GRCDEVICE.GetCurrent()->Resolve( D3DRESOLVE_RENDERTARGET2 | D3DRESOLVE_FRAGMENT0 | D3DRESOLVE_CLEARRENDERTARGET,	&pTilingRects[i], static_cast<D3DTexture*>(targets[2]->GetTexturePtr()), pDestPoint, 0, 0, &ClearColor, 0.0f, 0L, NULL ) );
		}
		CHECK_HRESULT( GRCDEVICE.GetCurrent()->Resolve( D3DRESOLVE_RENDERTARGET3 | D3DRESOLVE_FRAGMENT0 | D3DRESOLVE_CLEARRENDERTARGET, &pTilingRects[i], static_cast<D3DTexture*>(targets[3]->GetTexturePtr()), pDestPoint, 0, 0, &ClearColor, 0.0f, 0L, NULL ) );
	}

	GRCDEVICE.GetCurrent()->SetPredication( 0 );
	GRCDEVICE.GetCurrent()->EndTiling( 0, NULL, NULL, &ClearColor, 1.0f, 0L, NULL );
	GBuffer::UnlockTargets();

	//PostFX::CopyStencil();
}

// ----------------------------------------------------------------------------------------------- //

// Restore depth/stencil on Xbox360 at the start of the lighting phase
void DeferredLighting::RestoreDepthStencil()
{
	// Do the fast restore if we'll be rendering scene lights (since they'll have to do the slow restore anyway
	// and no one will use stencil before the scene lights).
	const bool bUseFastRestore = DeferredLighting__m_useLightsHiStencil && Lights::HasSceneLights();
	const bool bRestore3Bits = !bUseFastRestore;
	GBuffer::RestoreDepthOptimized(GBuffer::GetDepthTargetAsColor(), bRestore3Bits); 
}


// ----------------------------------------------------------------------------------------------- //
#endif //XENON
// ----------------------------------------------------------------------------------------------- //

void DeferredLighting::FinishGeometryPass(void)
{
	grcStateBlock::SetDepthStencilState(CShaderLib::DSS_Default_Invert);

#if __D3D && __XENON
	GRCDEVICE.GetCurrent()->SetRenderState(D3DRS_HISTENCILENABLE, FALSE);
	GRCDEVICE.GetCurrent()->SetRenderState(D3DRS_HISTENCILWRITEENABLE, FALSE);
#endif

#if DUMP_GBUFFER_TARGETS
	static bool done = false;
	if (!g_RenderThreadIndex && !done && grcSetupInstance && GRCDEVICE.GetMSAA())
	{
		done = true;
		grcTextureFactory::CreateParams params;
		params.Format = grctfA8R8G8B8;
		grcRenderTarget *const resolved = grcTextureFactory::GetInstance().CreateRenderTarget( "GBuffer0_Resolved",	grcrtPermanent, GRCDEVICE.GetWidth(), GRCDEVICE.GetHeight(), 32, &params);
		
# if SUPPORT_RENDERTARGET_DUMP
		grcSetupInstance->TakeRenderTargetShotsNow();
		//GBuffer::LockTargets();
		//GBuffer::UnlockTargets();
		static_cast<grcRenderTargetGNM*>( GBuffer::GetTarget(GBUFFER_RT_0) )->Resolve( resolved );
		grcSetupInstance->UntakeRenderTargetShots();
# else
		GBuffer::GetTarget(GBUFFER_RT_0)->SaveTarget();
		resolved->SaveTarget();
# endif	//SUPPORT_RENDERTARGET_DUMP
		delete resolved;
	}
#endif // DUMP_GBUFFER_TARGETS

	grmShaderFx::SetForcedTechniqueGroupId(m_previousGroupId);

#if EFFECT_STENCIL_REF_MASK
	// revert the ref mask
	grcEffect::SetStencilRefMask(0xFF);
#endif
}// end of FinishGeometryPass()...

// ----------------------------------------------------------------------------------------------- //

void DeferredLighting::PrepareDecalPass(bool fadePass)
{
	PF_PUSH_MARKER(fadePass ? "Decal Fade Pass" : "Decal Pass");
	PF_PUSH_TIMEBAR(fadePass ? "Decal Fade Pass" : "Decal Pass");

	River::SetDecalPassGlobalVars();

	grcStateBlock::SetBlendState(m_decalPass_B);
	grcStateBlock::SetDepthStencilState(m_decalPass_DS);
	grcStateBlock::SetRasterizerState(grcStateBlock::RS_Default);

#if !RSG_ORBIS && !RSG_DURANGO
	if (fadePass && CShaderLib::UsesStippleFades())
	{
		// On DX11 we can't use the D3DRS_MULTISAMPLEMASK for fade without MSAA (as on 360) so for the fade pass
		// it needs to be manually inserted into the shader itself. For DX11, as we don't want to add it to 
		// all shaders, it's been added to the alphaclip passes and we force the technique here.
		m_previousGroupAlphaClipId = grmShaderFx::GetForcedTechniqueGroupId();
		grmShaderFx::SetForcedTechniqueGroupId(m_deferredAlphaClipTechniqueGroupId);
	}
#else
	(void) fadePass;
#endif // DEVICE_MSAA
}

// ----------------------------------------------------------------------------------------------- //

void DeferredLighting::FinishDecalPass()
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	PF_POP_TIMEBAR();
	PF_POP_MARKER();

#if !RSG_ORBIS && !RSG_DURANGO
	if (CShaderLib::UsesStippleFades())
	{
		grmShaderFx::SetForcedTechniqueGroupId(m_previousGroupAlphaClipId);
	}
#endif // DEVICE_MSAA
}

// ----------------------------------------------------------------------------------------------- //

void DeferredLighting::PrepareFadePass()
{
	PF_PUSH_MARKER("Fade Pass");
	PF_PUSH_TIMEBAR("Fade Pass");

	grcStateBlock::SetBlendState(m_fadePass_B);
	grcStateBlock::SetRasterizerState(grcStateBlock::RS_Default);

#if MSAA_EDGE_PROCESS_FADING
	if (IsEdgeFadeProcessingActive())
	{
		CShaderLib::SetForcedEdge(true);
		grcStateBlock::SetDepthStencilState(m_fadePassAA_DS, 0);
	}else
#endif //MSAA_EDGE_PROCESS_FADING
	{
		grcStateBlock::SetDepthStencilState(m_fadePass_DS, 0);
	}

#if !RSG_ORBIS && !RSG_DURANGO
	if (CShaderLib::UsesStippleFades())
	{
		// On DX11 we can't use the D3DRS_MULTISAMPLEMASK for fade withough MSAA (as on 360) so for the fade pass.
		// It needs to be manually inserted into the shader itself. For DX11, as we don't want to add it to 
		// all shaders, it's been added to the alphaclip passes and we force the technique here.
		m_previousGroupAlphaClipId = grmShaderFx::GetForcedTechniqueGroupId();
		grmShaderFx::SetForcedTechniqueGroupId(m_deferredAlphaClipTechniqueGroupId);
	}
#endif // !RSG_ORBIS
}

// ----------------------------------------------------------------------------------------------- //

void DeferredLighting::FinishFadePass()
{
	PF_POP_TIMEBAR();
	PF_POP_MARKER();

#if MSAA_EDGE_PROCESS_FADING
	CShaderLib::SetForcedEdge(false);
#endif //MSAA_EDGE_PROCESS_FADING

#if !RSG_ORBIS && !RSG_DURANGO
	grmShaderFx::SetForcedTechniqueGroupId(m_previousGroupAlphaClipId);
#endif // !RSG_ORBIS
}

// ----------------------------------------------------------------------------------------------- //

void DeferredLighting::PrepareSSAPass( bool PS3_ONLY(useSubSampleAlpha), bool PS3_ONLY(forceClip) )
{
	#if __PS3
	if (!useSubSampleAlpha && PostFX::UseSubSampledAlpha() && !forceClip)
	{
		GBuffer::UnlockTargets();
		GBuffer::LockSingleTarget(GBUFFER_RT_0, true);
	}
	#endif // __PS3
}

// ----------------------------------------------------------------------------------------------- //

void DeferredLighting::FinishSSAPass()
{
#if __PS3
	if (PostFX::GetMarkingSubSampledAlphaSamples())
	{
		GBuffer::UnlockSingleTarget(GBUFFER_RT_0);
		GBuffer::LockTargets();
	}
#endif // _PS3
}

// ----------------------------------------------------------------------------------------------- //
void DeferredLighting::PrepareCutoutPass( bool useSubSampleAlpha, bool forceClip, bool EQAA_ONLY(disableEQAA) )
{
	grcStateBlock::SetBlendState(m_cutoutPass_B);
	grcStateBlock::SetDepthStencilState(m_cutoutPass_DS);
	grcStateBlock::SetRasterizerState(grcStateBlock::RS_Default);

	m_previousGroupAlphaClipId = -2;

	if ( PostFX::UseSubSampledAlpha() && !forceClip)
	{
		PostFX::SetMarkingSubSampleAlpha(!useSubSampleAlpha);

		m_previousGroupAlphaClipId = grmShaderFx::GetForcedTechniqueGroupId();
		if (useSubSampleAlpha)
		{
			PF_PUSH_TIMEBAR("SSA Diffuse Fragments");
			PF_PUSH_MARKER("SSA Diffuse Fragments");
			grcStateBlock::SetBlendState( PostFX::UseSinglePassSSA() ? m_SingleSampleSSABlendState : m_SSABlendState );
			grmShaderFx::SetForcedTechniqueGroupId(m_deferredSubSampleAlphaTechniqueGroupId);
		}
		else
		{
			PF_PUSH_TIMEBAR("SSA Alpha Fragments");
			PF_PUSH_MARKER("SSA Alpha Fragments");
			grmShaderFx::SetForcedTechniqueGroupId(m_deferredSubSampleWriteAlphaTechniqueGroupId);
		}
	}
	else
	{
		PostFX::SetMarkingSubSampleAlpha(false);

		PF_PUSH_TIMEBAR("Cutout Pass");
		PF_PUSH_MARKER("Cutout Pass");
		m_previousGroupAlphaClipId = grmShaderFx::GetForcedTechniqueGroupId();
		grmShaderFx::SetForcedTechniqueGroupId(m_deferredAlphaClipTechniqueGroupId);

		#if MSAA_EDGE_PROCESS_FADING
			CShaderLib::SetForcedEdge(IsEdgeFadeProcessingActive());
		#endif //MSAA_EDGE_PROCESS_FADING

		#if DEVICE_EQAA
			if ( BANK_SWITCH(DebugDeferred::m_disableEqaaDuringCutoutPass,true) && disableEQAA &&
				 GRCDEVICE.GetMSAA() && (GRCDEVICE.GetMSAA().m_uSamples > GRCDEVICE.GetMSAA().m_uFragments) )
			{
				// Disable EQAA temporarily for the cutout pass, once we're finishes we will re-enable it
				m_eqaaDisabledForCutout = true;
				GRCDEVICE.SetAACount( 1, 1, 1 );
			}
			else 
			{
				m_eqaaDisabledForCutout = false;
			}
		#endif // DEVICE_EQAA
	}
}

// ----------------------------------------------------------------------------------------------- //

void DeferredLighting::FinishCutoutPass()
{
	PF_POP_MARKER();
	PF_POP_TIMEBAR();
	if ( m_previousGroupAlphaClipId != -2)
	{
		grmShaderFx::SetForcedTechniqueGroupId(m_previousGroupAlphaClipId);
	}

	#if MSAA_EDGE_PROCESS_FADING
		CShaderLib::SetForcedEdge(false);
	#endif //MSAA_EDGE_PROCESS_FADING

	#if DEVICE_EQAA
		// EQAA was disabled temporarily for the cutout pass, now that we're done we need to re-enable it
		if ( m_eqaaDisabledForCutout )
		{
			GRCDEVICE.SetAACount( GRCDEVICE.GetMSAA().m_uSamples, GRCDEVICE.GetMSAA().m_uFragments, GRCDEVICE.GetMSAA().m_uFragments );
			m_eqaaDisabledForCutout = false;
		}
	#endif // DEVICE_EQAA
}


// ----------------------------------------------------------------------------------------------- //

void DeferredLighting::PrepareTreePass()
{
#if STENCIL_VEHICLE_INTERIOR
	grcStateBlock::SetDepthStencilState(m_treePass_DS);
	grcStateBlock::SetRasterizerState(grcStateBlock::RS_Default);
	grcStateBlock::SetBlendState(m_defaultPass_B);

#if MSAA_EDGE_PROCESS_FADING
	CShaderLib::SetForcedEdge(IsEdgeFadeProcessingActive());
#endif //MSAA_EDGE_PROCESS_FADING

	PF_PUSH_MARKER("Default Pass - Tree");
#if __BANK
	PF_PUSH_TIMEBAR("Default Pass - Tree");
#endif
#endif // STENCIL_VEHICLE_INTERIOR
}
 
// ----------------------------------------------------------------------------------------------- //

void DeferredLighting::FinishTreePass()
{
#if STENCIL_VEHICLE_INTERIOR
#if MSAA_EDGE_PROCESS_FADING
	CShaderLib::SetForcedEdge(false);
#endif //MSAA_EDGE_PROCESS_FADING
#if __BANK
	PF_POP_TIMEBAR();
#endif // __BANK
	PF_POP_MARKER();
#endif // STENCIL_VEHICLE_INTERIOR
}


// ----------------------------------------------------------------------------------------------- //

void DeferredLighting::PrepareDefaultPass(fwRenderPassId BANK_ONLY(id))
{
	grcStateBlock::SetDepthStencilState(m_defaultPass_DS);
	grcStateBlock::SetRasterizerState(grcStateBlock::RS_Default);
	grcStateBlock::SetBlendState(m_defaultPass_B);

	#if __BANK
		static const char* rpassStrTable[] =
		{
			"Default Pass - Visible",
			"Default Pass - Lod",
			"Default Pass - Cutout",
			"Default Pass - Decal",
			"Default Pass - Fading",
			"Default Pass - Alpha",
			"Default Pass - Water",
			"Default Pass - Tree",
		};
		CompileTimeAssert(NELEM(rpassStrTable) == RPASS_NUM_RENDER_PASSES);
		PF_PUSH_MARKER(rpassStrTable[id]);
		PF_PUSH_TIMEBAR(rpassStrTable[id]);
	#else
		PF_PUSH_MARKER("Default Pass");
	#endif
}

// ----------------------------------------------------------------------------------------------- //

void DeferredLighting::FinishDefaultPass()
{
#if __BANK
	PF_POP_TIMEBAR();
#endif // __BANK
	PF_POP_MARKER();
}

// ----------------------------------------------------------------------------------------------- //
#if MSAA_EDGE_PASS
bool DeferredLighting::IsEdgePassEnabled()
{
	return GRCDEVICE.GetMSAA()>1
		// Cutting off AMD pre-GCN hardware due to an acknowledged bug with stencil compression
		&& GRCDEVICE.IsReadOnlyDepthAllowed();
}

bool DeferredLighting::IsEdgePassActive()
{
	return IsEdgePassEnabled() BANK_ONLY(&& DebugDeferred::m_EnableEdgePass)
		// Portal shadow optimization uses both SPAREOR1 and SPAREOR2 stencil flags
		&& !CRenderPhaseCascadeShadowsInterface::ArePortalShadowsActive();
}

bool DeferredLighting::IsEdgePassActiveForSceneLights()
{
	return IsEdgePassActive()
		// Scene lights use both SPAREOR1 and SPAREOR2 flags for stencil culling
		&& !DeferredLighting__m_useLightsHiStencil;
}

bool DeferredLighting::IsEdgePassActiveForPostfx()
{
	return IsEdgePassActiveForSceneLights();
}

# if MSAA_EDGE_PROCESS_FADING
bool DeferredLighting::IsEdgeFadeProcessingActive()
{
	return IsEdgePassActive() && !CShaderLib::UsesStippleFades() BANK_ONLY(&& DebugDeferred::m_EnableEdgeFadeProcessing);
}
# endif //MSAA_EDGE_PROCESS_FADING

static void DrawQuad()
{
# if FAST_QUAD_SUPPORT
	FastQuad::Draw(true);
# else
	grcDrawSingleQuadf(0.f,0.f,1.f,1.f, 0.0f, 0.0, 0.0, 0.0f, 0.0f, Color32(0));
# endif
}

enum EdgePass
{
	EP_EMPTY,
	EP_SHOW,
	EP_MARK_DERIVE,
	EP_MARK_SAMPLE1,
	EP_MARK_SAMPLE2,
	EP_MARK_SAMPLE4,
	EP_MARK_SAMPLE8,
	EP_MARK_SAMPLE16,
	EP_MARK_SAMPLE4_OLD,
	EP_MARK_SAMPLE8_OLD,
	EP_MARK_SAMPLE_HEAVY,
	EP_COLLECT2,
	EP_COLLECT4,
	EP_COLLECT8,
};

void DeferredLighting::ExecuteEdgeMarkPass(bool isInterrior)
{
	if (!IsEdgePassActive() BANK_ONLY(|| DebugDeferred::m_IgnoreEdgeDetection))
	{
		return;
	}

	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	grcRenderTarget *stencilTarget = CRenderTargets::GetDepthBuffer();
	grmShader *const shader = m_deferredShaders[MM_TEXTURE_READS_ONLY][DEFERRED_SHADER_LIGHTING];

	SetShaderGBufferTargets();
	EdgePass pass = EP_EMPTY;

#if MSAA_EDGE_PROCESS_FADING
	bool bUseEdgeProcessing = !isInterrior && IsEdgeFadeProcessingActive();
	switch (GRCDEVICE.GetMSAA())
	{
	case 2: pass = EP_COLLECT2; break;
	case 4: pass = EP_COLLECT4; break;
	case 8: pass = EP_COLLECT8; break;
	default: bUseEdgeProcessing = false;
	}
	if (bUseEdgeProcessing)
	{
		// Collect (with 'OR') all stencil samples into the copy target
		grcStateBlock::SetStates(grcStateBlock::RS_NoBackfaceCull, grcStateBlock::DSS_IgnoreDepth, grcStateBlock::BS_Default);
		grcTextureFactory::GetInstance().LockRenderTarget(0, m_edgeCopyTarget, NULL);
			ShaderUtil::StartShader("Edge - Collect", shader, m_edgeMarkTechniqueIdx, pass);
				DrawQuad();
			ShaderUtil::EndShader(shader);
		grcTextureFactory::GetInstance().UnlockRenderTarget(0);
		shader->SetVar(m_edgeCopyTextureVar, m_edgeCopyTarget);
	}else
	{
		shader->SetVar(m_edgeCopyTextureVar, grcTexture::NoneBlack);
	}
#else
	(void)isInterrior;
#endif //MSAA_EDGE_PROCESS_FADING

#if MSAA_EDGE_USE_DEPTH_COPY
	CRenderTargets::CopyDepthBuffer(stencilTarget, m_edgeMarkDepthCopy);
	SetShaderGBufferTarget(GBUFFER_DEPTH, m_edgeMarkDepthCopy);
#elif RSG_PC
	// avoiding RT conflict
	const grcTexture *depthTexture = NULL;
	if (GRCDEVICE.IsReadOnlyDepthAllowed())
	{
		depthTexture = CRenderTargets::GetDepthBuffer();
		stencilTarget = CRenderTargets::GetDepthBufferCopy();
	}else
	{
		depthTexture = CRenderTargets::GetDepthBufferCopy(true);
	}
	SetShaderGBufferTarget(GBUFFER_DEPTH, depthTexture);
	//SetShaderGBufferTarget(GBUFFER_STENCIL, depthTexture);
#endif //RSG_PC

	grcStateBlock::SetRasterizerState(m_edgePass_R);
	grcStateBlock::SetDepthStencilState(m_edgePass_DS, EDGE_FLAG);
	grcStateBlock::SetBlendState(m_skinPass_B);

	shader->SetVar(m_edgeMarkParamsVar,
		BANK_SWITCH(DebugDeferred::m_edgeMarkParams, DEFERRED_LIGHTING_DEFAULT_edgeMarkParams));

	pass = EP_MARK_DERIVE;
	switch (GRCDEVICE.GetMSAA())
	{
	case 2: pass = EP_MARK_SAMPLE2; break;
	case 4: pass = BANK_ONLY(DebugDeferred::m_OldEdgeDetection ? EP_MARK_SAMPLE4_OLD :) EP_MARK_SAMPLE4; break;
	case 8: pass = BANK_ONLY(DebugDeferred::m_OldEdgeDetection ? EP_MARK_SAMPLE8_OLD :) EP_MARK_SAMPLE8; break;
	case 16: pass = EP_MARK_SAMPLE16; break;
	}
#if __BANK
	if (DebugDeferred::m_AlternativeEdgeDetection)
		pass = EP_MARK_DERIVE;
	if (DebugDeferred::m_AggressiveEdgeDetection)
		pass = EP_MARK_SAMPLE_HEAVY;
#endif //__BANK

	grcTextureFactory::GetInstance().LockRenderTarget(0, NULL, stencilTarget);
		ShaderUtil::StartShader("Edge - Mark", shader, m_edgeMarkTechniqueIdx, pass);
			DrawQuad();
		ShaderUtil::EndShader(shader);
	grcTextureFactory::GetInstance().UnlockRenderTarget(0);

	SetShaderGBufferTargets();	//restore GBufferTargets

# if __DEV
	if (DebugDeferred::m_DebugEdgePassColor)
	{
		grcStateBlock::SetStates(grcStateBlock::RS_NoBackfaceCull, grcStateBlock::DSS_IgnoreDepth, grcStateBlock::BS_Default);
		grcTextureFactory::GetInstance().LockRenderTarget(0, m_edgeMarkDebugTarget, NULL);
			ShaderUtil::StartShader("Edge - Show", shader, m_edgeMarkTechniqueIdx, EP_SHOW);
				DrawQuad();
			ShaderUtil::EndShader(shader);
		grcTextureFactory::GetInstance().UnlockRenderTarget(0);
	}
# endif //BANK
}

void DeferredLighting::ExecuteEdgeClearPass()
{
	if (!IsEdgePassActive() BANK_ONLY(|| DebugDeferred::m_IgnoreEdgeDetection))
	{
		return;
	}

	grcRenderTarget *stencilTarget = CRenderTargets::GetDepthBuffer();

	grcStateBlock::SetRasterizerState(m_edgePass_R);
	grcStateBlock::SetDepthStencilState(m_edgePass_DS, 0);
	grcStateBlock::SetBlendState(m_skinPass_B);

	grcTextureFactory::GetInstance().LockRenderTarget(0, NULL, stencilTarget);

	ShaderUtil::StartShader("Edge - Clear", m_deferredShaders[MM_DEFAULT][DEFERRED_SHADER_LIGHTING], m_edgeMarkTechniqueIdx, EP_EMPTY);
	DrawQuad();
	ShaderUtil::EndShader(m_deferredShaders[MM_DEFAULT][DEFERRED_SHADER_LIGHTING]);

	grcTextureFactory::GetInstance().UnlockRenderTarget(0);
}

#endif //MSAA_EDGE_PASS
// ----------------------------------------------------------------------------------------------- //

void DeferredLighting::ExecuteSkinLightPass(grcRenderTarget* backBuffer, const bool enablePedLight)
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

#if RSG_PC
	if(GRCDEVICE.GetMSAA()>1 || (GRCDEVICE.GetDxFeatureLevel() <= 1000))
	{
		CRenderTargets::UnlockSceneRenderTargets();
		CRenderTargets::LockSceneRenderTargets();
	}
#endif
	if (enablePedLight)
	{
		Lights::RenderPedLight();
#if DEVICE_GPU_WAIT
		GRCDEVICE.GpuWaitOnPreviousWrites();
#endif
	}

#if __BANK
	if (!DebugDeferred::m_enableSkinPass)
		return;
#endif
#if RSG_PC
	if (GRCDEVICE.GetManufacturer() == INTEL && CSettingsManager::GetInstance().GetSettings().m_graphics.m_PostFX < CSettings::High)
		return;
#endif

#if DEVICE_MSAA && !ENABLE_PED_PASS_AA_SOURCE
	if(GRCDEVICE.GetMSAA()>1)
	{
		// SSS_SKIN samples from the resolved buffer so resolve after RenderPedLight but before SSS_SKIN
		CRenderTargets::ResolveBackBuffer(true);
#if RSG_PC
		if (GRCDEVICE.GetDxFeatureLevel() < 1100)
			SetShaderGBufferTarget(GBUFFER_DEPTH, CRenderTargets::GetDepthBuffer());
#endif
	}
#endif

	PF_PUSH_TIMEBAR_DETAIL("Skin");

	grmShader* currentShader = GetShader(DEFERRED_SHADER_LIGHTING);

	grcStateBlock::SetBlendState(m_skinPass_B);
	grcStateBlock::SetRasterizerState(m_skinPass_R);

	if (backBuffer == NULL)
	{
		backBuffer = XENON_SWITCH( CRenderTargets::GetBackBuffer7e3toInt(true), CRenderTargets::GetBackBuffer() ); 
	}
	
	grcRenderTarget* backBufferSrc = backBuffer;
	grcRenderTarget* depthRT = NULL;

#if DEVICE_MSAA
	if(GRCDEVICE.GetMSAA()>1)
	{
		#if ENABLE_PED_PASS_AA_SOURCE
			currentShader->SetVar( m_shaderLightTextureAA, CRenderTargets::GetBackBufferCopyAA(true) );
		#else
			backBufferSrc = CRenderTargets::GetBackBufferCopy(false);
		#endif
	}
#if RSG_PC
	else
	{
		grcRenderTarget* depthCopyRT = CRenderTargets::GetDepthBufferCopy();
		if (GRCDEVICE.GetDxFeatureLevel() <= 1000)		
		{
			depthCopyRT = CRenderTargets::GetDepthBuffer();
			SetShaderGBufferTarget(GBUFFER_DEPTH, CRenderTargets::GetDepthBufferCopy());
		}
		// Source/Destination can not be the same so do SSS lighting to Back buffer copy and restore to back buffer afterwards		
		CRenderTargets::UnlockSceneRenderTargets();
		depthRT = (GRCDEVICE.IsReadOnlyDepthAllowed()) ? CRenderTargets::GetDepthBuffer_ReadOnly() : depthCopyRT;
		grcTextureFactory::GetInstance().LockRenderTarget(0, CRenderTargets::GetBackBufferCopy(false), depthRT);
		GRCDEVICE.Clear(true, Color32(0, 0, 0, 0), false, 0.0f, false, 0);
	}
#endif // RSG_PC
#endif // DEVICE_MSAA

	// Skin setup
	currentShader->SetVar(
		m_shaderLightParameterID_skinColourTweak, 
		BANK_SWITCH(DebugDeferred::m_SubSurfaceColorTweak, DEFERRED_LIGHTING_DEFAULT_subsurfaceColorTweak));

	currentShader->SetVar(
		m_shaderLightParameterID_skinParams, 
		BANK_SWITCH(DebugDeferred::m_skinPassParams, DEFERRED_LIGHTING_DEFAULT_skinPassParams));

	SetDeferredVar(DEFERRED_SHADER_LIGHTING, DEFERRED_SHADER_LIGHT_TEXTURE, backBufferSrc);
	
#if __BANK	
	u32 skinPassPC = (DebugDeferred::m_enableSkinPassNG) ? 4 : 2;
	u32 skinPassNG = (DebugDeferred::m_enableSkinPassNG) ? 3 : 0;

#if RSG_PC
	skinPassPC += (CSettingsManager::GetInstance().GetSettings().m_graphics.m_PostFX >= CSettings::Ultra && DebugDeferred::m_enableSkinPassNG) ? 1 : 0;
#endif
#else
	u32 skinPassPC = 4;

#if RSG_PC
	if (CSettingsManager::GetInstance().GetSettings().m_graphics.m_PostFX >= CSettings::Ultra)
		skinPassPC = 5;
#endif

	u32 skinPassNG = 3;
#endif

	if(DeferredLighting__m_useGeomPassStencil)
	{
		grcStateBlock::SetDepthStencilState(m_skinPass_DS, DEFERRED_MATERIAL_PED);
	}
	else
	{
		grcStateBlock::SetDepthStencilState(m_skinNoStencilPass_DS);
	}

	if (ShaderUtil::StartShader("Ped Pass", currentShader, m_techniques[MM_DEFAULT][DEFERRED_TECHNIQUE_SSS_SKIN], (depthRT != NULL) ? skinPassPC : skinPassNG))
	{
		Render2DDeferredLightingRect(0.0f, 0.0f, 1.0, 1.0f);
		ShaderUtil::EndShader(currentShader);	
	}

	grcStateBlock::SetDepthStencilState(CShaderLib::DSS_Default_Invert);
#if !__WIN32PC && !RSG_DURANGO && !RSG_ORBIS
	CRenderTargets::ClearStencilCull(DEFERRED_MATERIAL_PED);
#endif

#if RSG_PC
	if (depthRT != NULL)
	{
		if (GRCDEVICE.GetDxFeatureLevel() <= 1000)		
		{
			SetShaderGBufferTarget(GBUFFER_DEPTH, CRenderTargets::GetDepthBuffer());
		}
		// Restore Skin Copy onto back buffer
		grcTextureFactory::GetInstance().UnlockRenderTarget(0);
		CRenderTargets::CopyTarget(CRenderTargets::GetBackBufferCopy(false), backBuffer, depthRT, DEFERRED_MATERIAL_PED );
		CRenderTargets::LockSceneRenderTargets_DepthCopy();
	}
#endif // RSG_PC

	PF_POP_TIMEBAR_DETAIL();
}


// ----------------------------------------------------------------------------------------------- //

void DeferredLighting::ExecuteUISkinLightPass(grcRenderTarget* backBuffer, grcRenderTarget* backBufferCopy)
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

#if __BANK
	if (!DebugDeferred::m_enableSkinPass)
		return;
#endif

	grmShader* currentShader = DeferredLighting::GetShader(DEFERRED_SHADER_LIGHTING, MM_SINGLE);

	grcStateBlock::SetDepthStencilState(m_skinPass_UI_DS, DEFERRED_MATERIAL_CLEAR);
	grcStateBlock::SetBlendState(m_skinPass_B);
	grcStateBlock::SetRasterizerState(m_skinPass_R);

	// Skin setup
	currentShader->SetVar(
		m_shaderLightParameterID_skinColourTweak, 
		BANK_SWITCH(DebugDeferred::m_SubSurfaceColorTweak, DEFERRED_LIGHTING_DEFAULT_subsurfaceColorTweak));

	currentShader->SetVar(
		m_shaderLightParameterID_skinParams, 
		BANK_SWITCH(DebugDeferred::m_skinPassParams, DEFERRED_LIGHTING_DEFAULT_skinPassParams));

#if __XENON
	CHECK_HRESULT(GRCDEVICE.GetCurrent()->SetRenderState(D3DRS_HISTENCILENABLE, TRUE));
	CHECK_HRESULT(GRCDEVICE.GetCurrent()->SetRenderState(D3DRS_HISTENCILWRITEENABLE, FALSE));
	CHECK_HRESULT(GRCDEVICE.GetCurrent()->SetRenderState(D3DRS_HISTENCILFUNC, D3DHSCMP_NOTEQUAL));
	CHECK_HRESULT(GRCDEVICE.GetCurrent()->SetRenderState(D3DRS_HISTENCILREF, 0xFF));
#endif

	SetDeferredVar(DEFERRED_SHADER_LIGHTING, DEFERRED_SHADER_LIGHT_TEXTURE, (backBufferCopy ? backBufferCopy : backBuffer), MM_SINGLE);

	if (ShaderUtil::StartShader("Ped Pass", currentShader, GetTechnique(DEFERRED_TECHNIQUE_SSS_SKIN, MM_SINGLE), 1))
	{
		Render2DDeferredLightingRect(0.0f, 0.0f, 1.0, 1.0f);
		ShaderUtil::EndShader(currentShader);	
	}
	
	grcStateBlock::SetDepthStencilState(CShaderLib::DSS_Default_Invert);
}


void DeferredLighting::ExecutePuddlePass()
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	PF_PUSH_TIMEBAR_DETAIL("PuddlePass");
#if RSG_PC
	CShaderLib::SetStereoParams(Vector4(0,0,1,0));
#endif
	SetProjectionShaderParams(DEFERRED_SHADER_LIGHTING, MM_DEFAULT);

	PuddlePassSingleton::InstanceRef().Draw(  GetShader(DEFERRED_SHADER_LIGHTING) );
#if RSG_PC
	CShaderLib::SetStereoParams(Vector4(0,0,0,0));
#endif
	PF_POP_TIMEBAR_DETAIL();
}


// ----------------------------------------------------------------------------------------------- //

void DeferredLighting::ExecuteSkinLightPassForward(grcRenderTarget* pSrcColourTarget, grcRenderTarget* pSrcDepthTarget)
{
#if __BANK
	if (!DebugDeferred::m_enableSkinPass)
		return;
#endif

	PF_PUSH_TIMEBAR_DETAIL("Skin Forward");

	grmShader* currentShader = GetShader(DEFERRED_SHADER_LIGHTING);

	if(DeferredLighting__m_useGeomPassStencil)
	{
		grcStateBlock::SetDepthStencilState(m_skinPassForward_DS, DEFERRED_MATERIAL_CLEAR);
	}
	else
	{
		grcStateBlock::SetDepthStencilState(m_skinNoStencilPass_DS);
	}
	grcStateBlock::SetBlendState(m_skinPass_B);
	grcStateBlock::SetRasterizerState(m_skinPass_R);

#if DEVICE_MSAA && ENABLE_PED_PASS_AA_SOURCE
	currentShader->SetVar( m_shaderLightTextureAA, CRenderTargets::GetBackBufferCopyAA(true) );
#endif

	grcRenderTarget* backBuff = NULL;
	backBuff = pSrcColourTarget;
	SetDeferredGlobalVar(DEFERRED_SHADER_GBUFFER_DEPTH_GLOBAL, pSrcDepthTarget);

	// Skin setup
	currentShader->SetVar(
		m_shaderLightParameterID_skinColourTweak, 
		BANK_SWITCH(DebugDeferred::m_SubSurfaceColorTweak, DEFERRED_LIGHTING_DEFAULT_subsurfaceColorTweak));
	currentShader->SetVar(
		m_shaderLightParameterID_skinParams, 
		BANK_SWITCH(DebugDeferred::m_skinPassParams, DEFERRED_LIGHTING_DEFAULT_skinPassParams));

	SetDeferredVar(DEFERRED_SHADER_LIGHTING, DEFERRED_SHADER_LIGHT_TEXTURE, backBuff);

	if (ShaderUtil::StartShader("Ped Pass", currentShader, m_techniques[MM_DEFAULT][DEFERRED_TECHNIQUE_SSS_SKIN], 1))
	{
		Render2DDeferredLightingRect(0.0f, 0.0f, 1.0, 1.0f);
		ShaderUtil::EndShader(currentShader);	
	}

	if(DeferredLighting__m_useGeomPassStencil)
	{
		grcStateBlock::SetDepthStencilState(CShaderLib::DSS_Default_Invert);
	}

	PF_POP_TIMEBAR_DETAIL();

}

// ----------------------------------------------------------------------------------------------- //
void DeferredLighting::RenderFoliageHelper(const s32 pass, const MultisampleMode aaMode, const EdgeMode em)
{
#if MSAA_EDGE_PASS
	if(DeferredLighting__m_useGeomPassStencil)
	{
		grcStateBlock::SetDepthStencilState(
			em == EM_IGNORE ? m_foliagePass_DS : m_directionalEdgeAllEqualPass_DS,
			em == EM_EDGE0  ? DEFERRED_MATERIAL_TREE | EDGE_FLAG : DEFERRED_MATERIAL_TREE);
	}
	else
	{
		grcStateBlock::SetDepthStencilState(
			em == EM_IGNORE ? m_foliageNoStencilPass_DS : m_directionalEdgeMaskEqualPass_DS,
			em == EM_EDGE0  ? EDGE_FLAG : 0);
	}
#else //MSAA_EDGE_PASS
	(void)em;
	if(DeferredLighting__m_useGeomPassStencil)
	{
#if __XENON || __PS3
		CRenderTargets::RefreshStencilCull(true, DEFERRED_MATERIAL_TREE, 0x07 PS3_ONLY(, false));
#endif		
	#if RSG_PC
		grcStateBlock::SetDepthStencilState(m_foliagePass_DS, DEFERRED_MATERIAL_TREE);
	#else
		grcStateBlock::SetDepthStencilState(m_foliageMainPass_DS, DEFERRED_MATERIAL_TREE|DEFERRED_MATERIAL_SPECIALBIT);
	#endif
	}
	else
	{
		grcStateBlock::SetDepthStencilState(m_foliageNoStencilPass_DS);
	}
#endif //MSAA_EDGE_PASS

	SetDirectionalRenderState(DEFERRED_SHADER_DIRECTIONAL, aaMode);
	RenderDirectionalLight(pass, true, aaMode);
}

void DeferredLighting::ExecuteFoliageLightPass()
{
	#if __BANK
		if (!DebugDeferred::m_enableFoliageLightPass)
			return;
	#endif // __BANK

	PF_PUSH_TIMEBAR_DETAIL("Foliage");

	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	const CLightSource &dirLight = Lights::GetRenderDirectionalLight();
	
	if(dirLight.GetIntensity() > 0.0f)
	{
		grcStateBlock::SetBlendState(m_foliagePass_B);		
		grcStateBlock::SetRasterizerState(m_foliagePass_R);

		DeferredLighting::SetShaderGBufferTargets();
#if RSG_PC
		if (GRCDEVICE.GetDxFeatureLevel() <= 1000)
		{
			SetShaderGBufferTarget(GBUFFER_DEPTH, CRenderTargets::GetDepthBufferCopy());
			SetShaderGBufferTarget(GBUFFER_STENCIL, CRenderTargets::GetDepthBufferCopy_Stencil());
		}		
#endif
		s32 pass = CalculatePass(&dirLight, false, dirLight.IsCastShadows(), true);

#if MSAA_EDGE_PASS
		const bool bUseEdgePass = IsEdgePassActive() BANK_ONLY(&& DebugDeferred::m_OptimizeDirectionalFoliage);
		if (bUseEdgePass)
		{
			BANK_ONLY(if (DebugDeferred::m_EnableDirectionalFoliageEdge))
				RenderFoliageHelper(pass, MM_SUPER_SAMPLE, EM_EDGE0);
			BANK_ONLY(if (DebugDeferred::m_EnableDirectionalFoliageFace))
				RenderFoliageHelper(pass, MM_TEXTURE_READS_ONLY, EM_FACE0);
		}else
#else
		const bool bUseEdgePass = false;
#endif //MSAA_EDGE_PASS
		{
			RenderFoliageHelper(pass, MM_DEFAULT, EM_IGNORE);
		}

		if( DeferredLighting__m_useGeomPassStencil || bUseEdgePass)
		{
			grcStateBlock::SetDepthStencilState(CShaderLib::DSS_Default_Invert);
#if __XENON || __PS3
			CRenderTargets::ClearStencilCull(DEFERRED_MATERIAL_TREE);
#endif
		}
	}

	PF_POP_TIMEBAR_DETAIL();
}

// ----------------------------------------------------------------------------------------------- //

void DeferredLighting::ExecuteSnowPass()
{
	PF_PUSH_MARKER("Snow");

#if RSG_PC
	grcRenderTarget* snowRT = VideoResManager::IsSceneScaled() ? CRenderTargets::GetOffscreenBuffer3() : CRenderTargets::GetOffscreenBuffer2();
	grcTextureFactory::GetInstance().LockRenderTarget(0, snowRT, NULL);
#else
	grcTextureFactory::GetInstance().LockRenderTarget(0, GBuffer::GetTarget(GBUFFER_RT_0), NULL);
#endif

	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	grcStateBlock::SetStates(grcStateBlock::RS_NoBackfaceCull, grcStateBlock::DSS_IgnoreDepth, grcStateBlock::BS_Default);
	
	DeferredLighting::SetShaderGBufferTargets();
	SetDeferredVar(DEFERRED_SHADER_LIGHTING, DEFERRED_SHADER_LIGHT_TEXTURE2, SSAO::GetSSAOTexture());

	grmShader* currentShader = GetShader(DEFERRED_SHADER_LIGHTING);

	if (ShaderUtil::StartShader("Snow Pass", currentShader, m_techniques[MM_DEFAULT][DEFERRED_TECHNIQUE_SNOW], 0))
	{
		Render2DDeferredLightingRect(0.0f, 0.0f, 1.0, 1.0f);
		ShaderUtil::EndShader(currentShader);	
	}

	grcResolveFlags flags;
	flags.NeedResolve = false;
	grcTextureFactory::GetInstance().UnlockRenderTarget(0, &flags);

#if RSG_PC
	CRenderTargets::CopyTarget(snowRT, GBuffer::GetTarget(GBUFFER_RT_0));
#endif

	#if __XENON
		grcTextureFactory::GetInstance().LockRenderTarget(0, GBuffer::GetTarget(GBUFFER_RT_0), NULL);
		flags.NeedResolve = true;
		flags.ClearColor = true;
		grcTextureFactory::GetInstance().UnlockRenderTarget(0, &flags);
	#endif

	PF_POP_MARKER();
}

// ----------------------------------------------------------------------------------------------- //
void DeferredLighting::SetDepthStencilState(const bool bNoDirLight, const EdgeMode em)
{
#if MSAA_EDGE_PASS
	if(DeferredLighting__m_useGeomPassStencil)
	{
		struct State
		{
				grcDepthStencilStateHandle *handle;
				int refValue;
		};

		if (BANK_ONLY(DebugDeferred::m_enableFoliageLightPass &&) !bNoDirLight)
		{
			const State states[] =
			{
				{&m_directionalFullPass_DS, DEFERRED_MATERIAL_TREE},
				{&m_directionalEdgeBit0EqualPass_DS, EDGE_FLAG | 0x0},	//Edge0: xy0
				{&m_directionalEdgeAllEqualPass_DS, EDGE_FLAG | 0x1},	//Edge1: 001
				{&m_directionalEdgeBit0EqualPass_DS, 0 | 0x0},	//Face0: xy0
				{&m_directionalEdgeAllEqualPass_DS, 0 | 0x1},	//Face1: 001
			};
			CompileTimeAssert(NELEM(states) == EM_TOTAL);

			grcStateBlock::SetDepthStencilState(*states[em].handle, states[em].refValue);
		}
		else
		{
			const State states[] =
			{
				{&m_directionalAmbientPass_DS, DEFERRED_MATERIAL_CLEAR},
				{&m_directionalEdgeBit2EqualPass_DS, EDGE_FLAG | 0x0},	//Edge0: 0yz
				{&m_directionalEdgeAllEqualPass_DS, EDGE_FLAG | 0x4},	//Edge1: 100
				{&m_directionalEdgeBit2EqualPass_DS, 0 | 0x0},	//Face0: 0yz
				{&m_directionalEdgeAllEqualPass_DS, 0 | 0x4},	//Face1: 100
			};
			CompileTimeAssert(NELEM(states) == EM_TOTAL);

			grcStateBlock::SetDepthStencilState(*states[em].handle, states[em].refValue);
		}
	}
	else
	{
		grcStateBlock::SetDepthStencilState(
			em == EM_IGNORE ? m_directionalNoStencilPass_DS : m_directionalEdgeMaskEqualPass_DS,
			em == EM_EDGE0 ? EDGE_FLAG : 0);
	}
#else //MSAA_EDGE_PASS
	(void)em;
	if(DeferredLighting__m_useGeomPassStencil)
	{
		// use stencil non-zero for sunlight
		#if __PPU || __XENON
		if (BANK_ONLY(DebugDeferred::m_enableFoliageLightPass &&) !bNoDirLight PS3_ONLY(&& !appliedZCullRefresh))
		{
			CRenderTargets::RefreshStencilCull(false, DEFERRED_MATERIAL_CLEAR, 0xFB PS3_ONLY(, false));
		}
		#endif //__PPU || __XENON

		if (BANK_ONLY(DebugDeferred::m_enableFoliageLightPass && !(CutSceneManager::GetInstance()->IsPlaying() && CutSceneManager::GetInstance()->IsIsolating()) && ) !bNoDirLight)
		{
		#if RSG_PC
			grcStateBlock::SetDepthStencilState(m_directionalFullPass_DS, DEFERRED_MATERIAL_TREE);
		#else
			grcStateBlock::SetDepthStencilState(m_directionalFullPass_DS, DEFERRED_MATERIAL_TREE|DEFERRED_MATERIAL_SPECIALBIT);
		#endif
		}
		else
		{
			grcStateBlock::SetDepthStencilState(m_directionalAmbientPass_DS, DEFERRED_MATERIAL_CLEAR);
		}
	}
	else
	{
		grcStateBlock::SetDepthStencilState(m_directionalNoStencilPass_DS, 0);
	}
#endif //MSAA_EDGE_PASS
}

void DeferredLighting::RenderDirectionalHelper(const bool bNoDirLight, const MultisampleMode aaMode, const EdgeMode em)
{
	#if TILED_LIGHTING_ENABLED && (__XENON || __PS3 || __D3D11 || RSG_ORBIS)
	if ((BANK_SWITCH(DebugDeferred::m_enableTiledRendering, !(__D3D11 || RSG_ORBIS)) && !Water::IsCameraUnderwater()) BANK_ONLY( || DebugDeferred::m_forceTiledDirectional))
	{
		RenderTiledDirectionalLight(bNoDirLight, aaMode, em);
	}
	else
	#endif
	{
		SetDepthStencilState(bNoDirLight, em);
		const s32 pass = CalculatePass(&Lights::GetRenderDirectionalLight(), false, !bNoDirLight, false);
		CompileTimeAssert(DIRECTIONALPASS_NUM_PASSES < 255);	// must fit into u8
		SetDirectionalRenderState(DEFERRED_SHADER_DIRECTIONAL, aaMode);
		RenderDirectionalLight(pass, true, aaMode);
	}
}

void DeferredLighting::ExecuteAmbientAndDirectionalPass(const bool bDrawDefLight, bool PPU_ONLY(appliedZCullRefresh) )
{
	PF_PUSH_TIMEBAR_DETAIL("Directional Light");

	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	const CLightSource &dirLight = Lights::GetRenderDirectionalLight();

	const bool bNoDirLight	= ((dirLight.GetIntensity() == 0.0f) || 
							    !dirLight.IsCastShadows());

	if(bDrawDefLight)
	{
	#if !RSG_PC
		// foliage prepass only for Orbis/Durango - BS#2069821:
		grcStateBlock::SetBlendState(grcStateBlock::BS_Default_WriteMaskNone);
		grcStateBlock::SetDepthStencilState(m_foliagePrePass_DS, DEFERRED_MATERIAL_TREE|DEFERRED_MATERIAL_SPECIALBIT);
		grmShader* currentShader = GetShader(DEFERRED_SHADER_LIGHTING);
		if (ShaderUtil::StartShader("Foliage PreProcess", currentShader, m_techniques[MM_DEFAULT][DEFERRED_TECHNIQUE_FOLIAGE_PREPROCESS], 0))
		{
			Render2DDeferredLightingRect(0.0f, 0.0f, 1.0, 1.0f);
			ShaderUtil::EndShader(currentShader);	
		}
		grcStateBlock::SetDepthStencilState(CShaderLib::DSS_Default_Invert);
	#endif

		grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
		#if  __XENON 
			CHECK_HRESULT(GRCDEVICE.GetCurrent()->SetRenderState(D3DRS_HISTENCILENABLE, TRUE));
			CHECK_HRESULT(GRCDEVICE.GetCurrent()->SetRenderState(D3DRS_HISTENCILWRITEENABLE, FALSE));
			CHECK_HRESULT(GRCDEVICE.GetCurrent()->SetRenderState(D3DRS_HISTENCILFUNC, D3DHSCMP_NOTEQUAL));
			if (BANK_ONLY(DebugDeferred::m_enableFoliageLightPass &&) !bNoDirLight)
			{
				CHECK_HRESULT(GRCDEVICE.GetCurrent()->SetRenderState(D3DRS_HISTENCILREF, 0xFB));
			}
			else
			{
				CHECK_HRESULT(GRCDEVICE.GetCurrent()->SetRenderState(D3DRS_HISTENCILREF, 0xFF));
			}
			CHECK_HRESULT(GRCDEVICE.GetCurrent()->FlushHiZStencil(D3DFHZS_ASYNCHRONOUS));
			if(Water::UseHQWaterRendering())//this prevents corruption from being visible in the water depth occluded areas.
				GRCDEVICE.Clear(true, Color32(0x0), false, 0.0f, 0);
		#endif
		grcStateBlock::SetRasterizerState(m_directionalPass_R);
		SetShaderGBufferTargets();

#if MSAA_EDGE_PASS
		const bool bUseEdgePass = IsEdgePassActive() BANK_ONLY(&& DebugDeferred::m_OptimizeDirectional);
		if (bUseEdgePass)
		{
			// need additional passes to cover tree/non-tree and ambient/non-ambient cases
			BANK_ONLY(if (DebugDeferred::m_EnableDirectionalEdge0))
			{
				RenderDirectionalHelper(bNoDirLight, MM_SUPER_SAMPLE, EM_EDGE0);
			}
			if (DeferredLighting__m_useGeomPassStencil BANK_ONLY(&& DebugDeferred::m_EnableDirectionalEdge1))
			{
				RenderDirectionalHelper(bNoDirLight, MM_SUPER_SAMPLE, EM_EDGE1);
			}

			BANK_ONLY(if (DebugDeferred::m_EnableDirectionalFace0))
			{
				RenderDirectionalHelper(bNoDirLight, MM_TEXTURE_READS_ONLY, EM_FACE0);
			}

			if (DeferredLighting__m_useGeomPassStencil BANK_ONLY(&& DebugDeferred::m_EnableDirectionalFace1))
			{
				RenderDirectionalHelper(bNoDirLight, MM_TEXTURE_READS_ONLY, EM_FACE1);
			}
		}else
#else
		const bool bUseEdgePass = false;
#endif //MSAA_EDGE_PASS
		{
			RenderDirectionalHelper(bNoDirLight, MM_DEFAULT, EM_IGNORE);
		}

		if(DeferredLighting__m_useGeomPassStencil || bUseEdgePass)
		{
			#if __XENON || __PS3
			if (BANK_ONLY(DebugDeferred::m_enableFoliageLightPass &&) !bNoDirLight PS3_ONLY(&& !appliedZCullRefresh))
			{
				CRenderTargets::ClearStencilCull(DEFERRED_MATERIAL_CLEAR);
			}
			#endif
			grcStateBlock::SetDepthStencilState(CShaderLib::DSS_Default_Invert);
		}

		#if __XENON
			CHECK_HRESULT(GRCDEVICE.GetCurrent()->SetRenderState(D3DRS_HISTENCILENABLE, FALSE));
		#endif
	}

	if ( false) 
		RenderTiledShadow();
	PF_POP_TIMEBAR_DETAIL();
}

// ----------------------------------------------------------------------------------------------- //

void DeferredLighting::QualityModeSelect()
{
	m_LightingQuality = LQ_INGAME;
	if(camInterface::GetCutsceneDirector().IsCutScenePlaying())
	{
		m_LightingQuality = LQ_CUTSCENE;
	}

#if __BANK
	if(DebugDeferred::m_bForceHighQualityLighting)
	{
		m_LightingQuality = LQ_CUTSCENE;
	}
#endif // __BANK
}

// ----------------------------------------------------------------------------------------------- //
void DeferredLighting::GetProjectionShaderParams(Vec4V_InOut projParamsOut, Vec3V_InOut shearProj0, Vec3V_InOut shearProj1, Vec3V_InOut shearProj2)
{
	const grcViewport *const vp = grcViewport::GetCurrent();
	const Vector4 projParams = ShaderUtil::CalculateProjectionParams(vp);
	const Vector2 shear = vp->GetPerspectiveShear();
	Mat34V_ConstRef invView = vp->GetCameraMtx();

	// Pre-combining constants, to avoid constant ops in "GetEyeVec()" in the shaders
	// Originally was:
	//  	const float2 projPos = (signedScreenPos + deferredPerspectiveShearParams.xy) * deferredProjectionParams.xy;
	//      return float4(mul( float4(projPos,-1,0), gViewInverse ).xyz, 1);
	// 
	// After factoring, it is now only a 3x3 transform instead of add + scale + add + scale + 4x4 transform
	float sxpx = shear.x * projParams.x;
	float sypy = shear.y * projParams.y;
	shearProj0 = Vec3V(projParams.x * invView[0][0], projParams.y * invView[1][0], sxpx * invView[0][0] + sypy * invView[1][0] - invView[2][0]);
	shearProj1 = Vec3V(projParams.x * invView[0][1], projParams.y * invView[1][1], sxpx * invView[0][1] + sypy * invView[1][1] - invView[2][1]);
	shearProj2 = Vec3V(projParams.x * invView[0][2], projParams.y * invView[1][2], sxpx * invView[0][2] + sypy * invView[1][2] - invView[2][2]);
	projParamsOut = VECTOR4_TO_VEC4V(projParams);
}

// ----------------------------------------------------------------------------------------------- //

void DeferredLighting::SetProjectionShaderParams(eDeferredShaders deferredShader, const MultisampleMode aaMode)
{
	// Set screen size
	const float screenWidth = (float)VideoResManager::GetSceneWidth();
	const float screenHeight = (float)VideoResManager::GetSceneHeight();
	const Vector4 screenSize(screenWidth, screenHeight, 1.0f / screenWidth, 1.0f / screenHeight);

	Vec4V projParams;
	Vec3V shearProj0, shearProj1, shearProj2;
	GetProjectionShaderParams(projParams, shearProj0, shearProj1, shearProj2);

	SetDeferredVar(deferredShader, DEFERRED_SHADER_SCREENSIZE, screenSize, aaMode);
	SetDeferredVar(deferredShader, DEFERRED_SHADER_PROJECTION_PARAMS, projParams, aaMode);
	SetDeferredVar(deferredShader, DEFERRED_SHADER_PERSPECTIVESHEAR_PARAMS0, shearProj0, aaMode);
	SetDeferredVar(deferredShader, DEFERRED_SHADER_PERSPECTIVESHEAR_PARAMS1, shearProj1, aaMode);
	SetDeferredVar(deferredShader, DEFERRED_SHADER_PERSPECTIVESHEAR_PARAMS2, shearProj2, aaMode);
}

// ----------------------------------------------------------------------------------------------- //

void DeferredLighting::SetLightShaderParams(
	eDeferredShaders deferredShader, 
	const CLightSource* light, 
	const grcTexture *texture, 
	float radiusScale, 
	const bool tangentAlignedToCamDir,
	const bool useHighResMesh,
 	const MultisampleMode aaMode)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));

	// Light Parameters
	Vector4 position;
	float radius = 0.0f;
	Vector4 direction;
	Vector4 tangent;
	Vector4 colourAndIntensity;
	eLightType type;

	// Defaults for all lights
	type = light->GetType();
	position = Vector4(light->GetPosition());
	direction = Vector4(light->GetDirection());
	tangent = Vector4(light->GetTangent());

#if __WIN32PC || RSG_DURANGO || RSG_ORBIS
	position.w = direction.w = tangent.w = 0.0f;	// to prevent NaN in shaders constants asserts
#endif

	if (tangentAlignedToCamDir)
	{
		const Vec3V camDir = -gVpMan.GetRenderGameGrcViewport()->GetCameraMtx().GetCol2();
		tangent = VEC3V_TO_VECTOR3(Normalize(Cross(RCC_VEC3V(direction), camDir)));
	}

	switch (type)
	{
		case LIGHT_TYPE_POINT:
		case LIGHT_TYPE_SPOT:
		case LIGHT_TYPE_CAPSULE:
		{
			position -= direction * light->GetCapsuleExtent() * 0.5f;
			radius = light->GetRadius()*radiusScale;
			colourAndIntensity = Vector4(light->GetColor().x, light->GetColor().y, light->GetColor().z, light->GetIntensity());
			BANK_ONLY(DebugLighting::OverrideDeferredLightParams(&colourAndIntensity, light));
			break;
		}
		case LIGHT_TYPE_DIRECTIONAL:
		{
			colourAndIntensity.SetVector3ClearW(light->GetColor() * light->GetIntensity());
			BANK_ONLY(DebugLighting::OverrideDirectional(&colourAndIntensity));
			break;
		}
		case LIGHT_TYPE_AO_VOLUME:
		{
			colourAndIntensity = Vector4(light->GetColor().x, light->GetColor().y, light->GetColor().z, light->GetIntensity());
			radius = light->GetRadius();
			break;
		}
		default:
		{
			Assertf(false, "Tried to set shaders parameters for light type %d\n", type);
			break;
		}
	}

	Vector4 deferredLightParams[LIGHT_PARAM_COUNT - 5];
	Lights::CalculateLightParams(light, deferredLightParams);

	Vector4 lightParams[LIGHT_PARAM_COUNT];

	lightParams[0]  = position;
	lightParams[1]  = direction;
	lightParams[2]  = tangent;
	lightParams[3]  = colourAndIntensity;
	lightParams[4]  = Lights::CalculateLightConsts(type, radius, useHighResMesh);
	lightParams[5]  = deferredLightParams[0];
	lightParams[6]  = deferredLightParams[1];
	lightParams[7]  = deferredLightParams[2];
	lightParams[8]  = deferredLightParams[3];
	lightParams[9]  = deferredLightParams[4];
	lightParams[10] = deferredLightParams[5];
	lightParams[11] = deferredLightParams[6];
	lightParams[12] = deferredLightParams[7];
	lightParams[13] = deferredLightParams[8];

	SetDeferredVarArray(deferredShader, DEFERRED_SHADER_LIGHT_PARAMS, lightParams, LIGHT_PARAM_COUNT, aaMode);
	SetDeferredVar(deferredShader, DEFERRED_SHADER_LIGHT_TEXTURE, (texture == NULL) ? grcTexture::None : texture, aaMode);

	// camera below water & light above water
	float waterLevel = Water::GetWaterLevel();
	if( light->UsesCaustic() && Water::IsCameraUnderwater() && (position.z-2.0f) > waterLevel )
	{
		SetDeferredVar(deferredShader, DEFERRED_SHADER_LIGHT_TEXTURE1, Water::GetCausticTexture(), aaMode);
	}
}

// ----------------------------------------------------------------------------------------------- //

void DeferredLighting::SetLightShaftShaderParamsGlobal()
{
	Mat44V compositeMtx;

#if __BANK
	if (!DebugLighting::m_LightShaftNearClipEnabled)
	{
		DebugLighting::m_LightShaftNearClipExponent = -logf(grcViewport::GetCurrent()->GetNearClip())/logf(2.0f);

		compositeMtx = grcViewport::GetCurrent()->GetCompositeMtx();
	}
	else
#endif // __BANK
	{
		grcViewport vp = *grcViewport::GetCurrent();
#if !__BANK || BANK_SWITCH_ASSERT
		static float nearClipDefault = powf(0.5f, LIGHTSHAFT_NEAR_CLIP_EXPONENT_DEFAULT);
#endif

		vp.SetNearClip(BANK_SWITCH(powf(0.5f, DebugLighting::m_LightShaftNearClipExponent), nearClipDefault));
		compositeMtx = vp.GetCompositeMtx();
	}

	m_deferredShaders[MM_DEFAULT][DEFERRED_SHADER_VOLUME]->SetVar(m_shaderVolumeParameterID_deferredVolumeShaftCompositeMtx, compositeMtx);
}

// ----------------------------------------------------------------------------------------------- //

static void AddLightShaftPlane(Vec4V shaftPlanes[3], int& numShaftPlanes, Vec3V_In p0, Vec3V_In p1, Vec3V_In p2, Vec3V_In camPos)
{
	const Vec4V plane = BuildPlane(p0, p1, p2);

	if (numShaftPlanes < 3 && IsLessThanAll(Dot(plane.GetXYZ(), camPos - p0), ScalarV(V_ZERO)))
	{
		shaftPlanes[numShaftPlanes++] = plane;
	}
}

// ----------------------------------------------------------------------------------------------- //

bool DeferredLighting::SetLightShaftShaderParams(const CLightShaft* shaft)
{
	if (shaft->m_intensity <= 0.01f)
	{
		return false;
	}

	Vec3V corners[4];
	corners[0] = shaft->m_corners[0];
	corners[1] = shaft->m_corners[1];
	corners[2] = shaft->m_corners[3]; // 2,3 are swapped
	corners[3] = shaft->m_corners[2]; // 2,3 are swapped

	const Vec3V dirx = corners[1] - corners[0];
	const Vec3V diry = corners[2] - corners[0]; // this was corners[3] - corners[0], but that meant diry needed to subtract dirx in the vertex shader
	const Vec3V dirz = shaft->m_direction;
	const Vec3V norm = Cross(dirx, diry);

	if (IsLessThanAll(Dot(norm, dirz), ScalarV(V_ZERO)))
	{
		return false;
	}

	const grcViewport& grcVP = *grcViewport::GetCurrent();
	const Vec3V camPos = grcVP.GetCameraPosition();

	if ((shaft->m_flags & LIGHTSHAFTFLAG_DRAW_IN_FRONT_AND_BEHIND) == 0)
	{
		if (IsLessThanAll(Dot(norm, camPos - corners[0]), ScalarV(V_ZERO)) BANK_ONLY(&& !DebugLighting::m_drawLightShaftsAlways))
		{
			return false;
		}
	}

	const ScalarV shaftInvRadius = InvSqrt(Max(MagSquared(dirx), MagSquared(diry))); // normalise intensity so that it is 1 looking perpendicularly through the shaft
	const ScalarV shaftIntensity = ScalarV(shaft->m_intensity)*shaftInvRadius;
	const ScalarV shaftLength    = ScalarV(shaft->m_length);

	// TODO -- this could be optimised if we stored light shaft corners as an oriented quad
	Vec4V shaftGradient;
	Vec4V shaftPlanes[3];
	int numShaftPlanes = 0;
	const Vec3V offset = dirz*shaftLength;
	const Vec3V p000 = corners[0];
	const Vec3V p100 = corners[1];
	const Vec3V p010 = corners[2];
	const Vec3V p110 = corners[3];
	const Vec3V p001 = corners[0] + offset;
	const Vec3V p101 = corners[1] + offset;
	const Vec3V p011 = corners[2] + offset;
	const Vec3V p111 = corners[3] + offset;

	AddLightShaftPlane(shaftPlanes, numShaftPlanes, p000, p001, p101, camPos);
	AddLightShaftPlane(shaftPlanes, numShaftPlanes, p100, p101, p111, camPos);
	AddLightShaftPlane(shaftPlanes, numShaftPlanes, p110, p111, p011, camPos);
	AddLightShaftPlane(shaftPlanes, numShaftPlanes, p010, p011, p001, camPos);
	AddLightShaftPlane(shaftPlanes, numShaftPlanes, p000, p100, p110, camPos);
	AddLightShaftPlane(shaftPlanes, numShaftPlanes, p001, p011, p111, camPos);

	for (; numShaftPlanes < 3; numShaftPlanes++)
	{
		shaftPlanes[numShaftPlanes] = BuildPlane(camPos, grcVP.GetCameraMtx().GetCol2());
	}

#if __BANK
	if (shaft->m_attrExt.m_gradientAlignToQuad) // TODO -- make this work! wtf ..
	{
		shaftGradient = Vec4V(norm, Dot(norm, corners[0] + offset))/Dot(norm, offset);
	}
	else
#endif // __BANK
	{
		const ScalarV zmin = Min(Dot(dirz, p000), Dot(dirz, p100), Dot(dirz, p010), Dot(dirz, p110));
		const ScalarV zmax = Max(Dot(dirz, p001), Dot(dirz, p101), Dot(dirz, p011), Dot(dirz, p111));
		const ScalarV fall = Invert(zmax - zmin);

		shaftGradient = Vec4V(-dirz*fall, ScalarV(V_ONE) + zmin*fall);
	}

	const Vec4V shaftGradientColourInv = Vec4V(BANK_SWITCH(Invert(Max(shaft->m_attrExt.m_gradientColour, Vec3V(V_FLT_SMALL_3))), Vec3V(V_ONE)), ScalarV(shaft->m_softness));
	const Vec4V colourAndIntensity = shaft->m_colour;
	const Vec4V colour = colourAndIntensity*colourAndIntensity.GetW()*shaftIntensity;
	const Vec3V midPos = (corners[0] + corners[1] + corners[2] + corners[3])*ScalarV(V_QUARTER);
	const ScalarV zero(V_ZERO);

	grmShader *const volume = m_deferredShaders[MM_DEFAULT][DEFERRED_SHADER_VOLUME];
	volume->SetVar(m_shaderVolumeParameterID_deferredVolumePosition, Vec4V(midPos, zero));
	volume->SetVar(m_shaderVolumeParameterID_deferredVolumeDirection, Vec4V(dirz, zero));
	volume->SetVar(m_shaderVolumeParameterID_deferredVolumeTangentXAndShaftRadius, Vec4V(dirx, Invert(shaftInvRadius)*ScalarV(V_HALF))); // TODO -- shaftInvRadius is actually 1/(2*shaftRadius)
	volume->SetVar(m_shaderVolumeParameterID_deferredVolumeTangentYAndShaftLength, Vec4V(diry, shaftLength));
	volume->SetVar(m_shaderVolumeParameterID_deferredVolumeColour, colour);
	volume->SetVar(m_shaderVolumeParameterID_deferredVolumeShaftPlanes, shaftPlanes, NELEM(shaftPlanes));
	volume->SetVar(m_shaderVolumeParameterID_deferredVolumeShaftGradient, shaftGradient);
	volume->SetVar(m_shaderVolumeParameterID_deferredVolumeShaftGradientColourInv, shaftGradientColourInv);

	SetProjectionShaderParams(DEFERRED_SHADER_VOLUME, MM_DEFAULT);

	return true;
}

// ----------------------------------------------------------------------------------------------- //

grcTexture* DeferredLighting::GetVolumeTexture()
{
	return m_volumeTexture;
}

// ----------------------------------------------------------------------------------------------- //
#if __XENON
// ----------------------------------------------------------------------------------------------- //

void DeferredLighting::SetExtraRefMapState(bool enable)
{
	if (enable)
		CHECK_HRESULT(GRCDEVICE.GetCurrent()->SetRenderState(D3DRS_HIGHPRECISIONBLENDENABLE1, TRUE));
	else
		CHECK_HRESULT(GRCDEVICE.GetCurrent()->SetRenderState(D3DRS_HIGHPRECISIONBLENDENABLE1, FALSE));
}

void DeferredLighting::SetExtraTAAState(float flag)
{
	static dev_s32 debugAltMask = 0xD2;
	static dev_s32 debugStdMask = 0x87;

	if (flag==0.0f)
		CHECK_HRESULT(GRCDEVICE.GetCurrent()->SetRenderState(D3DRS_ALPHATOMASKOFFSETS, debugAltMask));
	else
		CHECK_HRESULT(GRCDEVICE.GetCurrent()->SetRenderState(D3DRS_ALPHATOMASKOFFSETS, debugStdMask));
}

// ----------------------------------------------------------------------------------------------- //
#endif //__XENON
// ----------------------------------------------------------------------------------------------- //

grcDepthStencilStateHandle g_CullEqualStateBlock =grcStateBlock::DSS_Invalid;

void DeferredLighting::InitSetHiStencilCullState()
{
	grcDepthStencilStateDesc	dssDesc;
	dssDesc.DepthFunc = rage::FixupDepthDirection(grcRSV::CMP_LESS);
	dssDesc.StencilEnable=true;
	dssDesc.StencilWriteMask=0x0; 				// disable writes
	dssDesc.FrontFace.StencilFunc = grcRSV::CMP_EQUAL;
	dssDesc.BackFace = dssDesc.FrontFace;
	g_CullEqualStateBlock = grcStateBlock::CreateDepthStencilState(dssDesc);
}

void DeferredLighting::TerminateHiStencilCullState()
{
	grcStateBlock::DestroyDepthStencilState(g_CullEqualStateBlock);
}

// stencil culling works on the PS3
// - set render target
// - set stencil cull control
// - clear stencil only
// - render / update stencil
// - render with stencil test enabled and get early rejection
// Note: always switch off stencil write before early reject

void DeferredLighting::SetHiStencilCullState(HIStencilCullState mode)
{
	if (mode == CULL_OFF) 
	{		
		grcStateBlock::SetDepthStencilState( CShaderLib::DSS_Default_Invert);
		XENON_ONLY(	GRCDEVICE.GetCurrent()->SetRenderState( D3DRS_HISTENCILENABLE,	FALSE ) );		
#if DEPTH_BOUNDS_SUPPORT
		BANK_ONLY(if( DebugLighting::m_useLightsDepthBounds ))
		{
			grcDevice::SetDepthBoundsTestEnable(FALSE);
		}
#endif //DEPTH_BOUNDS_SUPPORT	
	}
	else if(mode == CULL_SUBSAMPLEAA_ONLY)
	{
		grcStateBlock::SetDepthStencilState( g_CullEqualStateBlock, DEFERRED_MATERIAL_SPECIALBIT );
	}
}

// ----------------------------------------------------------------------------------------------- //
// Define explicit template instantiations
// ----------------------------------------------------------------------------------------------- //

template void DeferredLighting::SetDeferredVar<const rage::grcTexture*>	(eDeferredShaders, eDeferredShaderVars, const rage::grcTexture* const&,	MultisampleMode);
template void DeferredLighting::SetDeferredVar<Vector4>					(eDeferredShaders, eDeferredShaderVars, const Vector4&,					MultisampleMode);
template void DeferredLighting::SetDeferredVar<Vec3V>					(eDeferredShaders, eDeferredShaderVars, const Vec3V&,					MultisampleMode);
template void DeferredLighting::SetDeferredVar<Vec4V>					(eDeferredShaders, eDeferredShaderVars, const Vec4V&,					MultisampleMode);
template void DeferredLighting::SetDeferredVar<Mat44V>					(eDeferredShaders, eDeferredShaderVars, const Mat44V&,					MultisampleMode);
template void DeferredLighting::SetDeferredVarArray<const Vec4V*>		(eDeferredShaders, eDeferredShaderVars, const Vec4V*, u32,				MultisampleMode);
