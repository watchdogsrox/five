//
// filename:	ShaderLib.cpp
// description:	Class controlling loading of shaders
// written by:	Adam Fowler
//

#include "shaders/ShaderLib.h"

// Rage headers
#include "bank/bank.h"
#include "bank/bkmgr.h"
#include "grmodel/shader.h"
#include "grmodel/shaderfx.h"
#include "grcore/device.h"
#include "grcore/effect.h"
#include "grcore/im.h"
#include "streaming/streaming.h"
#include "system/xtl.h"

// Framework headers
#include "fwsys/gameskeleton.h"
#include "fwdrawlist/drawlistmgr.h"

// Game headers
#include "debug/debug.h"
#include "debug/Rendering/DebugDeferred.h"
#include "game/weather.h"
#include "peds/ped.h"
#include "Renderer/Renderer.h"
#include "renderer/DrawLists/drawListMgr.h"
#include "renderer/Deferred/DeferredLighting.h"
#include "renderer/PostProcessFX.h"
#include "renderer/RenderPhases/RenderPhaseCascadeShadows.h" // for IsEnabled
#include "renderer/RenderPhases/RenderPhaseEntitySelect.h" // for ENTITYSELECT_ENABLED_BUILD
#include "shader_source/vehicles/vehicle_common_values.h" // For GPU_DAMAGE_WRITE_ENABLED
#include "renderer/Deferred/GBuffer.h"
#include "renderer/rendertargets.h"
#include "renderer/SpuPm/SpuPmMgr.h"
#include "scene/world/GameWorld.h"
#include "timecycle/TimeCycle.h"
#include "vfx/misc/LinearPiecewiseFog.h"

#if RSG_ORBIS
#include "gnmx/helpers.h"
#endif

//OPTIMISATIONS_OFF()

// PSN: when enabled, AlphaRefs constants must be enabled as variables in common.fxh
#define USE_ALPHAREF		(__XENON || __PPU || __D3D11 || RSG_ORBIS)

// See also USE_SSTAA define for Durango in shader code, common.fxh, l. 331
#define USE_TRANSPARENCYAA	(1)

// Generated via: for (int i=0; i<256; i++) printf("%d.0f/255,%s",i,(i&7)!=7?"":"\n\t");
float CShaderLib::ms_divideBy255[256] = {
	0.0f/255,1.0f/255,2.0f/255,3.0f/255,4.0f/255,5.0f/255,6.0f/255,7.0f/255,
	8.0f/255,9.0f/255,10.0f/255,11.0f/255,12.0f/255,13.0f/255,14.0f/255,15.0f/255,
	16.0f/255,17.0f/255,18.0f/255,19.0f/255,20.0f/255,21.0f/255,22.0f/255,23.0f/255,
	24.0f/255,25.0f/255,26.0f/255,27.0f/255,28.0f/255,29.0f/255,30.0f/255,31.0f/255,
	32.0f/255,33.0f/255,34.0f/255,35.0f/255,36.0f/255,37.0f/255,38.0f/255,39.0f/255,
	40.0f/255,41.0f/255,42.0f/255,43.0f/255,44.0f/255,45.0f/255,46.0f/255,47.0f/255,
	48.0f/255,49.0f/255,50.0f/255,51.0f/255,52.0f/255,53.0f/255,54.0f/255,55.0f/255,
	56.0f/255,57.0f/255,58.0f/255,59.0f/255,60.0f/255,61.0f/255,62.0f/255,63.0f/255,
	64.0f/255,65.0f/255,66.0f/255,67.0f/255,68.0f/255,69.0f/255,70.0f/255,71.0f/255,
	72.0f/255,73.0f/255,74.0f/255,75.0f/255,76.0f/255,77.0f/255,78.0f/255,79.0f/255,
	80.0f/255,81.0f/255,82.0f/255,83.0f/255,84.0f/255,85.0f/255,86.0f/255,87.0f/255,
	88.0f/255,89.0f/255,90.0f/255,91.0f/255,92.0f/255,93.0f/255,94.0f/255,95.0f/255,
	96.0f/255,97.0f/255,98.0f/255,99.0f/255,100.0f/255,101.0f/255,102.0f/255,103.0f/255,
	104.0f/255,105.0f/255,106.0f/255,107.0f/255,108.0f/255,109.0f/255,110.0f/255,111.0f/255,
	112.0f/255,113.0f/255,114.0f/255,115.0f/255,116.0f/255,117.0f/255,118.0f/255,119.0f/255,
	120.0f/255,121.0f/255,122.0f/255,123.0f/255,124.0f/255,125.0f/255,126.0f/255,127.0f/255,
	128.0f/255,129.0f/255,130.0f/255,131.0f/255,132.0f/255,133.0f/255,134.0f/255,135.0f/255,
	136.0f/255,137.0f/255,138.0f/255,139.0f/255,140.0f/255,141.0f/255,142.0f/255,143.0f/255,
	144.0f/255,145.0f/255,146.0f/255,147.0f/255,148.0f/255,149.0f/255,150.0f/255,151.0f/255,
	152.0f/255,153.0f/255,154.0f/255,155.0f/255,156.0f/255,157.0f/255,158.0f/255,159.0f/255,
	160.0f/255,161.0f/255,162.0f/255,163.0f/255,164.0f/255,165.0f/255,166.0f/255,167.0f/255,
	168.0f/255,169.0f/255,170.0f/255,171.0f/255,172.0f/255,173.0f/255,174.0f/255,175.0f/255,
	176.0f/255,177.0f/255,178.0f/255,179.0f/255,180.0f/255,181.0f/255,182.0f/255,183.0f/255,
	184.0f/255,185.0f/255,186.0f/255,187.0f/255,188.0f/255,189.0f/255,190.0f/255,191.0f/255,
	192.0f/255,193.0f/255,194.0f/255,195.0f/255,196.0f/255,197.0f/255,198.0f/255,199.0f/255,
	200.0f/255,201.0f/255,202.0f/255,203.0f/255,204.0f/255,205.0f/255,206.0f/255,207.0f/255,
	208.0f/255,209.0f/255,210.0f/255,211.0f/255,212.0f/255,213.0f/255,214.0f/255,215.0f/255,
	216.0f/255,217.0f/255,218.0f/255,219.0f/255,220.0f/255,221.0f/255,222.0f/255,223.0f/255,
	224.0f/255,225.0f/255,226.0f/255,227.0f/255,228.0f/255,229.0f/255,230.0f/255,231.0f/255,
	232.0f/255,233.0f/255,234.0f/255,235.0f/255,236.0f/255,237.0f/255,238.0f/255,239.0f/255,
	240.0f/255,241.0f/255,242.0f/255,243.0f/255,244.0f/255,245.0f/255,246.0f/255,247.0f/255,
	248.0f/255,249.0f/255,250.0f/255,251.0f/255,252.0f/255,253.0f/255,254.0f/255,255.0f/255 };


//
//
//
//
grcEffectGlobalVar CShaderLib::ms_globalScalarsID; //x=globalAlpha y=HDRMultiplier z=AmbientScale
grcEffectGlobalVar CShaderLib::ms_globalScalars2ID; 
grcEffectGlobalVar CShaderLib::ms_globalScalars3ID; // scale, invscale, normalScaler, naturalAmbientPush
#if !RSG_ORBIS && !RSG_DURANGO
grcEffectGlobalVar	CShaderLib::ms_globalFadeID;
u16					CShaderLib::ms_FadeMask16;
#endif // !RSG_ORBIS && !RSG_DURANGO

grcDepthStencilStateHandle CShaderLib::ms_gDefaultState_DS = grcStateBlock::DSS_Invalid;
grcDepthStencilStateHandle CShaderLib::ms_gNoDepthWrite_DS = grcStateBlock::DSS_Invalid;
grcDepthStencilStateHandle CShaderLib::ms_gNoDepthRead_DS = grcStateBlock::DSS_Invalid;
grcRasterizerStateHandle CShaderLib::ms_frontFaceCulling_RS = grcStateBlock::RS_Invalid;
grcRasterizerStateHandle CShaderLib::ms_disableFaceCulling_RS = grcStateBlock::RS_Invalid;
grcDepthStencilStateHandle CShaderLib::DSS_Default_Invert;
grcDepthStencilStateHandle CShaderLib::DSS_LessEqual_Invert;
grcDepthStencilStateHandle CShaderLib::DSS_TestOnly_Invert;
grcDepthStencilStateHandle CShaderLib::DSS_TestOnly_LessEqual_Invert;
grcBlendStateHandle CShaderLib::BS_NoColorWrite;
static grcBlendStateHandle previousBS[NUMBER_OF_RENDER_THREADS];

#if __DEV
bool  CShaderLib::ms_gReflectTweakEnabled = false;
float CShaderLib::ms_gReflectTweak = 0.1f;
bool  CShaderLib::ms_gDisableShaderVarCaching = false;
#endif // __DEV

strLocalIndex CShaderLib::ms_txdId = strLocalIndex(-1);

#if SHADERLIB_USE_MSAA_PARAMS
static grcEffectGlobalVar globalTargetAAParamsID;
#endif // SHADERLIB_USE_MSAA_PARAMS

static grcEffectGlobalVar globalAOEffectScaleID;
static grcEffectGlobalVar globalScreenSizeID; //screensize
static grcEffectGlobalVar gFogParamsID;
static grcEffectGlobalVar gFogColorID;
static grcEffectGlobalVar gFogColorEID;
static grcEffectGlobalVar gFogColorNID;
#if LINEAR_PIECEWISE_FOG
static grcEffectGlobalVar gLinearPiecewiseFogParamsID;
#endif // LINEAR_PIECEWISE_FOG

static grcEffectGlobalVar gReflectionTweaksID;
static grcEffectGlobalVar gDynamicBakesAndWetnessID;

static grcEffectGlobalVar globalPlayerLFootPosID;
static grcEffectGlobalVar globalPlayerRFootPosID;

#if RSG_PC
static grcEffectGlobalVar gReticuleDistID;
static grcEffectGlobalVar globalTreesUseDiscardId;
static grcEffectGlobalVar globalShaderQualityID;
static grcEffectGlobalVar gStereoParamsID;
static grcEffectGlobalVar gStereoParams1ID;
#endif

static grcEffectGlobalVar globalUseFogRayId;

static grcEffectGlobalVar	gFogRayTextureID;
static grcEffectGlobalVar	gFogRayIntensityID;

#if DEVICE_MSAA
static grcEffectGlobalVar globalTransparencyAASamples;
#endif // DEVICE_MSAA

static grcEffectGlobalVar globalAlphaTestRef;

#if USE_ALPHAREF
	static grcEffectGlobalVar alphaRefVec0ID	= (grcEffectGlobalVar)0;
	static grcEffectGlobalVar alphaRefVec1ID	= (grcEffectGlobalVar)0;

	static s32 global_alphaRef				= 90;
#if HACK_RDR3
	static s32 ped_alphaRef					= 0;
#else
	static s32 ped_alphaRef					= 128;
#endif
	static s32 ped_longhair_alphaRef		= 128;
	static s32 foliage_alphaRef				= 90;	// valve distance-map alpha will use middle value of 0.35 to compensate
	static s32 foliageImposter_alphaRef		= 88;
	static s32 billboard_alphaRef			= 148;
	static s32 shadowBillboard_alphaRef		= 88;
	static s32 grass_alphaRef				= 32;
#endif // USE_ALPHAREF...
	static bool useTransparencyAA			= USE_TRANSPARENCYAA;
	static u32	maxStipplingAA				= 2;


ThreadVector4	CShaderLib::ms_gCurrentScalarGlobals[NUMBER_OF_RENDER_THREADS]		= {{1.0f, 1.0f, 1.0f, 1.0f}};
u32				CShaderLib::ms_giCurrentScalarGlobals[NUMBER_OF_RENDER_THREADS][4]	= {{255, 255, 255, 255}}; //setting to same values as ms_gCurrentScalarGlobals
ThreadVector4	CShaderLib::ms_gCurrentScalarGlobals2[NUMBER_OF_RENDER_THREADS]		= {{1.0f, 1.0f, 1.0f, 1.0f}};
ThreadVector4	CShaderLib::ms_gCurrentScalarGlobals3[NUMBER_OF_RENDER_THREADS]		= {{16.0f,1.0f/16.0f,0.0f,0.0f}};
ThreadVector4	CShaderLib::ms_gCurrentDirectionalAmbient[NUMBER_OF_RENDER_THREADS] = {{ 0.0f, 0.0f, 0.0f, 0.0f }};


bool			CShaderLib::ms_facingBackwards[NUMBER_OF_RENDER_THREADS]			= {false};
bool			CShaderLib::ms_fogDepthFxEnabled[NUMBER_OF_RENDER_THREADS]			= {true};

float CShaderLib::ms_overridesEmissiveScale = 0.0f;
float CShaderLib::ms_overridesEmissiveMult = 0.0f;
float CShaderLib::ms_overridesArtIntAmbDown = 0.0f;
float CShaderLib::ms_overridesArtIntAmbBase = 0.0f;
float CShaderLib::ms_overridesArtExtAmbDown = 0.0f;
float CShaderLib::ms_overridesArtExtAmbBase = 0.0f;
float CShaderLib::ms_overridesArtAmbScale = 0.0f;

// Other things are packed into the "unused" W component of these vectors
CShaderLib::CFogParams	CShaderLib::ms_globalFogParams[NUMBER_OF_RENDER_THREADS];
#if LINEAR_PIECEWISE_FOG
ThreadVector4	CShaderLib::ms_linearPiecewiseFog[NUMBER_OF_RENDER_THREADS][LINEAR_PIECEWISE_FOG_NUM_SHADER_VARIABLES];
#endif // LINEAR_PIECEWISE_FOG

u32				CShaderLib::ms_gDeferredMaterial[NUMBER_OF_RENDER_THREADS]			= { DEFERRED_MATERIAL_DEFAULT }; 
u32				CShaderLib::ms_gDeferredMaterialORMask[NUMBER_OF_RENDER_THREADS]	= { 0x0 };

ThreadVector4	CShaderLib::ms_gCurrentScreenSize[NUMBER_OF_RENDER_THREADS]			= { {1.0f, 1.0f, 1.0f, 1.0f} };
ThreadVector4	CShaderLib::ms_gCurrentAOEffectScale[NUMBER_OF_RENDER_THREADS]		= {{1.0f, 1.0f, 1.0f, 1.0f}};

bool			CShaderLib::ms_gDeferredWetBlend[NUMBER_OF_RENDER_THREADS]			= { false };

ThreadVector4	CShaderLib::ms_gReflectionTweaks									= {1.0f, 1.0f, 1.0f, 0.0f};
bool			CShaderLib::ms_gMotionBlurMask										= true;

float			CShaderLib::ms_alphaTestRef[NUMBER_OF_RENDER_THREADS]               = {NUMBER_OF_RENDER_THREADS_INITIALIZER_LIST(0.f)};

Vector4			CShaderLib::ms_PlayerLFootPos[2];
Vector4			CShaderLib::ms_PlayerRFootPos[2];
ThreadVector4	CShaderLib::ms_gCurrentPlayerLFootPos[NUMBER_OF_RENDER_THREADS]		= { {0,0,0,0} };
ThreadVector4	CShaderLib::ms_gCurrentPlayerRFootPos[NUMBER_OF_RENDER_THREADS]		= { {0,0,0,0} };;
#if RSG_PC
	float		CShaderLib::ms_ShaderQuality[2]										= {2.0f,2.0f};
#endif

static grcMaterialLibrary *s_MtlLib;

#if __PS3
namespace rage
{
	extern u32 TotalUcode, TotalUcodeWithoutCache, TotalUcodeInDebugHeap;
}
#endif
namespace rage
{
	extern u32 TotalReferenceInstanceData;
}

// We do this to minimize the amount of time spent between starting the 
// game and the first draw call. loading the remaining shaders is left 
// until the intro movie is active
void CShaderLib::LoadInitialShaders()
{
	grcEffect *effect = NULL;
	ASSET.PushFolder("update2:/common/shaders");	

	effect = grcEffect::Create("im");	
	Assert(effect);
	effect = grcEffect::Create("rage_bink");	
	Assert(effect);
	effect = grcEffect::Create("scaleform_shaders");	
	Assert(effect);
	effect = grcEffect::Create("rage_fastmipmap");	
	Assert(effect);
	if( GRCDEVICE.GetMSAA()>1 )
	{
		effect = grcEffect::Create("imMS");	
		Assert(effect);
	}
	ASSET.PopFolder();
}

void CShaderLib::LoadAllShaders()
{
	grmShaderFactory::GetInstance().PreloadShaders("common:/shaders");
	if( GRCDEVICE.GetMSAA()>1 )
	{
		grmShaderFactory::GetInstance().PreloadShaders("common:/shaders","preloadMS");
	}
#if __ASSERT
	grcEffect::SetAllowShaderLoading(false);
#endif
}


void CShaderLib::PreInit()
{
	ms_globalScalarsID = grcEffect::LookupGlobalVar("globalScalars");
	grcEffect::SetGlobalVar(ms_globalScalarsID, (Vec4V&)ms_gCurrentScalarGlobals[g_RenderThreadIndex]);

	sysMemUseMemoryBucket b(MEMBUCKET_RENDER);

	size_t used = sysMemGetMemoryUsed(MEMBUCKET_RENDER);

	// We need to register all techniques before any shaders get loaded:

#if __XENON || __D3D11 || RSG_ORBIS // special-case "ld" being used for local light shadows on 360 ..
	{
		grmShaderFx::RegisterTechniqueGroup("ld");
		grmShaderFx::RegisterTechniqueGroup("shadtexture_ld");
	}
#endif // __XENON || __D3D11 || RSG_ORBIS

	// CRenderPhaseCascadeShadows
	grmShaderFx::RegisterTechniqueGroup("wdcascade");
#if __PPU && USE_EDGE
	grmShaderFx::RegisterTechniqueGroup("wdcascadeedge");
	grmShaderFx::RegisterTechniqueGroup("shadtexture_wdcascadeedge");
#else // __PPU && USE_EDGE
	grmShaderFx::RegisterTechniqueGroup("shadtexture_wdcascade");
#endif

	// CParaboloidShadow
#if __PPU
	grmShaderFx::RegisterTechniqueGroup("ldpoint"); // xenon uses "ld"
	grmShaderFx::RegisterTechniqueGroup("shadtexture_ldpoint");

#if USE_EDGE
	grmShaderFx::RegisterTechniqueGroup("ldedgepoint");
	grmShaderFx::RegisterTechniqueGroup("shadtexture_ldedgepoint");
#endif // USE_EDGE

#endif // __PPU

	// CRenderPhaseWaterReflection
	grmShaderFx::RegisterTechniqueGroup("waterreflection");
	grmShaderFx::RegisterTechniqueGroup("waterreflectionalphaclip");
	grmShaderFx::RegisterTechniqueGroup("waterreflectionalphacliptint");

	// CRenderPhaseReflection
	grmShaderFx::RegisterTechniqueGroup("cube");
	#if GS_INSTANCED_CUBEMAP
		grmShaderFx::RegisterTechniqueGroup("cubeinst");
	#endif

	//Generic
	grmShaderFx::RegisterTechniqueGroup("alt0"); //UnderwaterLowTechniqueGroup
	grmShaderFx::RegisterTechniqueGroup("alt1"); //UnderwaterHighTechniqueGroup
	grmShaderFx::RegisterTechniqueGroup("alt2"); //UnderwaterEnvLowTechniqueGroup
	grmShaderFx::RegisterTechniqueGroup("alt3"); //UnderwaterEnvHighTechniqueGroup
	grmShaderFx::RegisterTechniqueGroup("alt4"); //EnvTechniqueGroup
	grmShaderFx::RegisterTechniqueGroup("alt5"); //SinglePassTechniqueGroup
	grmShaderFx::RegisterTechniqueGroup("alt6"); //SinglePassEnvTechniqueGroup
	grmShaderFx::RegisterTechniqueGroup("alt7"); //FoamTechniqueGrou
	grmShaderFx::RegisterTechniqueGroup("alt8"); //WaterHeightTechniqueGroup

	// CLights
	grmShaderFx::RegisterTechniqueGroup("lightweight0");
	grmShaderFx::RegisterTechniqueGroup("lightweight4");
	grmShaderFx::RegisterTechniqueGroup("lightweight8");
	grmShaderFx::RegisterTechniqueGroup("lightweight0CutOut");
	grmShaderFx::RegisterTechniqueGroup("lightweight4CutOut");
	grmShaderFx::RegisterTechniqueGroup("lightweight8CutOut");

	grmShaderFx::RegisterTechniqueGroup("lightweight0CutOutTint");
	grmShaderFx::RegisterTechniqueGroup("lightweight4CutOutTint");
	grmShaderFx::RegisterTechniqueGroup("lightweight8CutOutTint");

	grmShaderFx::RegisterTechniqueGroup("lightweightNoCapsule4");
	grmShaderFx::RegisterTechniqueGroup("lightweightNoCapsule8");

	grmShaderFx::RegisterTechniqueGroup("lightweightHighQuality0");
	grmShaderFx::RegisterTechniqueGroup("lightweightHighQuality4");
	grmShaderFx::RegisterTechniqueGroup("lightweightHighQuality8");
	grmShaderFx::RegisterTechniqueGroup("lightweightHighQuality0CutOut");
	grmShaderFx::RegisterTechniqueGroup("lightweightHighQuality4CutOut");
	grmShaderFx::RegisterTechniqueGroup("lightweightHighQuality8CutOut");

	// CTreeImposters
	grmShaderFx::RegisterTechniqueGroup("imposter");
	grmShaderFx::RegisterTechniqueGroup("imposterdeferred");

 	// CMarkers
#if __PPU
	grmShaderFx::RegisterTechniqueGroup("manualdepth");
#endif

	// CPedVariationMgr
	grmShaderFx::RegisterTechniqueGroup("deferredbs");

	// DeferredLighting
	grmShaderFx::RegisterTechniqueGroup("deferred");
	grmShaderFx::RegisterTechniqueGroup("deferredalphaclip");
	grmShaderFx::RegisterTechniqueGroup("deferredsubsamplealphaclip");
	grmShaderFx::RegisterTechniqueGroup("deferredalphacliptint");
	grmShaderFx::RegisterTechniqueGroup("deferredsubsamplealpha");
	grmShaderFx::RegisterTechniqueGroup("deferredsubsamplewritealpha");
	grmShaderFx::RegisterTechniqueGroup("deferredcutscene");
	grmShaderFx::RegisterTechniqueGroup("deferredsubsamplealphatint");

	//Vehicle Fade Override Techniques
	grmShaderFx::RegisterTechniqueGroup("lightweight0WaterRefractionAlpha");
	grmShaderFx::RegisterTechniqueGroup("lightweight4WaterRefractionAlpha");
	grmShaderFx::RegisterTechniqueGroup("lightweight8WaterRefractionAlpha");
	grmShaderFx::RegisterTechniqueGroup("lightweightHighQuality0WaterRefractionAlpha");
	grmShaderFx::RegisterTechniqueGroup("lightweightHighQuality4WaterRefractionAlpha");
	grmShaderFx::RegisterTechniqueGroup("lightweightHighQuality8WaterRefractionAlpha");

	// RenderPhaseSeeThrough
	grmShaderFx::RegisterTechniqueGroup("seethrough");

	// UI 
	grmShaderFx::RegisterTechniqueGroup("ui");

#if ENTITYSELECT_ENABLED_BUILD
	// Entity Select debug technique
	grmShaderFx::RegisterTechniqueGroup("entityselect");
#endif // ENTITYSELECT_ENABLED_BUILD

#if GPU_DAMAGE_WRITE_ENABLED
	// Adding new damage circles to the vehicles' damage texture when collisions occur
	grmShaderFx::RegisterTechniqueGroup("applydamage");
#endif

	// for adding clouds to the light rays
	grmShaderFx::RegisterTechniqueGroup("clouddepth");

#if __BANK
	// CRenderPhaseDebugOverlay
	grmShaderFx::RegisterTechniqueGroup("debugoverlay");
#endif // __BANK
#if __XENON
	MEMORYSTATUS before, after;
	GlobalMemoryStatus(&before);
#endif

#if __ASSERT
	// Assert on late loaded shaders.
	grcEffect::SetAllowShaderLoading(true);
#endif // __ASSERT

	LoadInitialShaders();
	
	used = sysMemGetMemoryUsed(MEMBUCKET_RENDER) - used;

	Displayf("CShaderLib::PreInit - %" SIZETFMT "u bytes consumed by inital shader load",used); 
}

//
// name:		FullInit
// description:	Initialise shader library. 
void CShaderLib::Init()
{
	size_t used = sysMemGetMemoryUsed(MEMBUCKET_RENDER);

	CShaderLib::LoadAllShaders();

#if __ASSERT
	// Assert on late loaded shaders.
	grcEffect::SetAllowShaderLoading(false);
#endif // __ASSERT

	used = sysMemGetMemoryUsed(MEMBUCKET_RENDER) - used;

	Displayf("CShaderLib::Init - %" SIZETFMT "u bytes consumed by full shader load",used); 
#if __PS3
	Displayf("CShaderLib::Init - %u bytes in cached shaders, %u if uncached, %u saved (%u in debug heap)",TotalUcode,TotalUcodeWithoutCache,TotalUcodeWithoutCache - TotalUcode,TotalUcodeInDebugHeap);
#endif
#if __XENON
	GlobalMemoryStatus(&after);
	Displayf("CShaderLib::Init - %u bytes of physical memory consumed by shaders",before.dwAvailPhys - after.dwAvailPhys);
#endif
	Displayf("CShaderLib::Init - %u bytes consumed by reference instance data",TotalReferenceInstanceData);

	s_MtlLib = grcMaterialLibrary::Preload("common:/shaders/db/");
	if (Verifyf(s_MtlLib,"Material library didn't load, you're probably in a world of suck."))
		grcMaterialLibrary::SetCurrent(s_MtlLib);

#if NUMBER_OF_RENDER_THREADS > 1
	Assert(g_RenderThreadIndex==0);		// make sure we're running on mainRT
	Assert(g_IsSubRenderThread==false);

	// initialise global vars for render subthreads:
	for(int i=1; i<NUMBER_OF_RENDER_THREADS; i++)
	{
		ms_gCurrentScalarGlobals[i]		= ms_gCurrentScalarGlobals[0];
		ms_giCurrentScalarGlobals[i][0]	= ms_giCurrentScalarGlobals[0][0];
		ms_giCurrentScalarGlobals[i][1]	= ms_giCurrentScalarGlobals[0][1];
		ms_giCurrentScalarGlobals[i][2]	= ms_giCurrentScalarGlobals[0][2];
		ms_giCurrentScalarGlobals[i][3]	= ms_giCurrentScalarGlobals[0][3];
		
		ms_gCurrentScalarGlobals2[i]	= ms_gCurrentScalarGlobals2[0];
		ms_gCurrentScalarGlobals3[i]	= ms_gCurrentScalarGlobals3[0];

		ms_gCurrentDirectionalAmbient[i]= ms_gCurrentDirectionalAmbient[0];
		ms_globalFogParams[i]			= ms_globalFogParams[0];
		ms_facingBackwards[i]			= ms_facingBackwards[0];
		ms_fogDepthFxEnabled[i]			= ms_fogDepthFxEnabled[0];
		
		ms_gCurrentScreenSize[i]		= ms_gCurrentScreenSize[0];
		ms_gCurrentAOEffectScale[i]		= ms_gCurrentAOEffectScale[0];
		ms_gDeferredWetBlend[i]			= ms_gDeferredWetBlend[0];

		ms_gDeferredMaterial[i]			= ms_gDeferredMaterial[0];
		ms_gDeferredMaterialORMask[i]	= ms_gDeferredMaterialORMask[0];

		ms_gCurrentPlayerLFootPos[i]	= ms_gCurrentPlayerLFootPos[0];
		ms_gCurrentPlayerRFootPos[i]	= ms_gCurrentPlayerRFootPos[0];
	}
#endif
	ms_globalScalarsID = grcEffect::LookupGlobalVar("globalScalars");
	grcEffect::SetGlobalVar(ms_globalScalarsID, (Vec4V&)(ms_gCurrentScalarGlobals[g_RenderThreadIndex]));

	ms_globalScalars2ID= grcEffect::LookupGlobalVar("globalScalars2");
	grcEffect::SetGlobalVar(ms_globalScalars2ID, (Vec4V&)(ms_gCurrentScalarGlobals2[g_RenderThreadIndex]));

	ms_globalScalars3ID = grcEffect::LookupGlobalVar("globalScalars3");
	grcEffect::SetGlobalVar(ms_globalScalars3ID, (Vec4V&)(ms_gCurrentScalarGlobals3[g_RenderThreadIndex]));

	globalAOEffectScaleID= grcEffect::LookupGlobalVar("globalAOEffectScale");
	grcEffect::SetGlobalVar(globalAOEffectScaleID, (Vec4V&)(ms_gCurrentAOEffectScale[g_RenderThreadIndex]));

	globalScreenSizeID= grcEffect::LookupGlobalVar("globalScreenSize");
	grcEffect::SetGlobalVar(globalScreenSizeID, (Vec4V&)(ms_gCurrentScreenSize[g_RenderThreadIndex]));

#if SHADERLIB_USE_MSAA_PARAMS
	globalTargetAAParamsID = grcEffect::LookupGlobalVar("gTargetAAParams");
	const IntVector AAParams = {0};
	grcEffect::SetGlobalVar(globalTargetAAParamsID, AAParams);
#endif // SHADERLIB_USE_MSAA_PARAMS

#if DEVICE_MSAA
#if RSG_PC
	globalTreesUseDiscardId = grcEffect::LookupGlobalVar("gTreesUseDiscard");
	grcEffect::SetGlobalVar(globalTreesUseDiscardId, UsesStippleFades());
#endif // RSG_PC
#if USE_TRANSPARENCYAA
	CSettings::eSettingsLevel shaderQuality = CSettingsManager::GetInstance().GetSettings().m_graphics.m_ShaderQuality;
	useTransparencyAA = (shaderQuality >= CSettings::High);

	globalTransparencyAASamples = grcEffect::LookupGlobalVar("gTransparencyAASamples", shaderQuality != CSettings::Low);
	grcEffect::SetGlobalVar(globalTransparencyAASamples, useTransparencyAA ? (float)GRCDEVICE.GetMSAA() : 0.0f);
#endif // USE_TRANSPARENCYAA
#endif // DEVICE_MSAA

	globalUseFogRayId = grcEffect::LookupGlobalVar("gUseFogRay", RSG_PC ? false : true);

	gFogParamsID = grcEffect::LookupGlobalVar("globalFogParams");
	grcEffect::SetGlobalVar(gFogParamsID, (Vec4V*)&ms_globalFogParams[g_RenderThreadIndex], sizeof(CFogParams)/sizeof(Vec4V));

	gFogColorID = grcEffect::LookupGlobalVar("globalFogColor");
	grcEffect::SetGlobalVar(gFogColorID, Vec4V(V_ONE));
	
	gFogColorEID = grcEffect::LookupGlobalVar("globalFogColorE");
	grcEffect::SetGlobalVar(gFogColorEID, Vec4V(V_ONE));

	gFogColorNID = grcEffect::LookupGlobalVar("globalFogColorN");
	grcEffect::SetGlobalVar(gFogColorNID, Vec4V(V_ONE));

#if LINEAR_PIECEWISE_FOG
	gLinearPiecewiseFogParamsID = grcEffect::LookupGlobalVar("linearPiecewiseFogParams");
	LinearPiecewiseFog_ShaderParams linearParams; // Default to off.
	SetLinearPiecewiseFogParams(linearParams);
	grcEffect::SetGlobalVar(gLinearPiecewiseFogParamsID, (Vec4V*)ms_linearPiecewiseFog[g_RenderThreadIndex], LINEAR_PIECEWISE_FOG_NUM_SHADER_VARIABLES);
#endif // LINEAR_PIECEWISE_FOG

	gReflectionTweaksID = grcEffect::LookupGlobalVar("gReflectionTweaks");
	grcEffect::SetGlobalVar(gReflectionTweaksID, (Vec4V&)(ms_gReflectionTweaks));

	gDynamicBakesAndWetnessID = grcEffect::LookupGlobalVar("gDynamicBakesAndWetness");
	grcEffect::SetGlobalVar(gDynamicBakesAndWetnessID, Vec4V(V_ONE));

#if __XENON
	GRCDEVICE.SetColorExpBiasNow(0);
#endif

#if RSG_PC
	gReticuleDistID			= grcEffect::LookupGlobalVar("StereoReticuleDistTexture", (GRCDEVICE.CanUseStereo() && GRCDEVICE.IsStereoEnabled()) ? true : false);

	globalShaderQualityID	= grcEffect::LookupGlobalVar("GlobalShaderQuality", true);
	grcEffect::SetGlobalVar(globalShaderQualityID, ms_ShaderQuality[0]);

	gStereoParamsID = grcEffect::LookupGlobalVar("gStereoParams", false);
	gStereoParams1ID = grcEffect::LookupGlobalVar("gStereoParams1", false);
#endif

	gFogRayTextureID = grcEffect::LookupGlobalVar("FogRayTex", RSG_PC ? false : true);
	gFogRayIntensityID = grcEffect::LookupGlobalVar("gGlobalFogIntensity");

	globalAlphaTestRef  = grcEffect::LookupGlobalVar("alphaTestRef");

#if USE_ALPHAREF
	alphaRefVec0ID		= grcEffect::LookupGlobalVar("alphaRefVec0");
	alphaRefVec1ID		= grcEffect::LookupGlobalVar("alphaRefVec1",false); // This one only exists when certain grass code path are enabled.
	CShaderLib::SetAlphaRefConsts();
#endif // USE_ALPHAREF...

#if !RSG_ORBIS && !RSG_DURANGO
	ms_globalFadeID = grcEffect::LookupGlobalVar("globalFade");
#endif // !RSG_ORBIS

	globalPlayerLFootPosID	= grcEffect::LookupGlobalVar("gPlayerLFootPos");
	grcEffect::SetGlobalVar(globalPlayerLFootPosID, (Vec4V&)(ms_gCurrentPlayerLFootPos[g_RenderThreadIndex]));

	globalPlayerRFootPosID	= grcEffect::LookupGlobalVar("gPlayerRFootPos");
	grcEffect::SetGlobalVar(globalPlayerRFootPosID, (Vec4V&)(ms_gCurrentPlayerRFootPos[g_RenderThreadIndex]));

	// initialise state blocks
	
	// default depth/stencil block
	grcDepthStencilStateDesc defaultStateDS;
	defaultStateDS.DepthFunc = rage::FixupDepthDirection(grcRSV::CMP_LESSEQUAL);
	defaultStateDS.StencilEnable = 1;
	defaultStateDS.FrontFace.StencilFailOp = grcRSV::STENCILOP_KEEP;
	defaultStateDS.FrontFace.StencilDepthFailOp = grcRSV::STENCILOP_KEEP;
	defaultStateDS.FrontFace.StencilPassOp = grcRSV::STENCILOP_REPLACE;
	defaultStateDS.FrontFace.StencilFunc = grcRSV::CMP_ALWAYS;
	defaultStateDS.BackFace.StencilFailOp = grcRSV::STENCILOP_KEEP;
	defaultStateDS.BackFace.StencilDepthFailOp = grcRSV::STENCILOP_KEEP;
	defaultStateDS.BackFace.StencilPassOp = grcRSV::STENCILOP_REPLACE;
	defaultStateDS.BackFace.StencilFunc = grcRSV::CMP_ALWAYS;

	ms_gDefaultState_DS = grcStateBlock::CreateDepthStencilState(defaultStateDS);

	// write depth disabled block
	grcDepthStencilStateDesc noDepthWriteStateDS;
	noDepthWriteStateDS.DepthFunc = rage::FixupDepthDirection(grcRSV::CMP_LESSEQUAL);
	noDepthWriteStateDS.DepthWriteMask = 0;
	noDepthWriteStateDS.StencilEnable = 1;
	noDepthWriteStateDS.FrontFace.StencilFailOp = grcRSV::STENCILOP_KEEP;
	noDepthWriteStateDS.FrontFace.StencilDepthFailOp = grcRSV::STENCILOP_KEEP;
	noDepthWriteStateDS.FrontFace.StencilPassOp = grcRSV::STENCILOP_REPLACE;
	noDepthWriteStateDS.FrontFace.StencilFunc = grcRSV::CMP_ALWAYS;
	noDepthWriteStateDS.BackFace.StencilFailOp = grcRSV::STENCILOP_KEEP;
	noDepthWriteStateDS.BackFace.StencilDepthFailOp = grcRSV::STENCILOP_KEEP;
	noDepthWriteStateDS.BackFace.StencilPassOp = grcRSV::STENCILOP_REPLACE;
	noDepthWriteStateDS.BackFace.StencilFunc = grcRSV::CMP_ALWAYS;

	ms_gNoDepthWrite_DS = grcStateBlock::CreateDepthStencilState(noDepthWriteStateDS);

	// write read disabled block
	grcDepthStencilStateDesc noDepthReadStateDS;
	noDepthReadStateDS.DepthFunc = rage::FixupDepthDirection(grcRSV::CMP_ALWAYS);
	noDepthReadStateDS.StencilEnable = 1;
	noDepthReadStateDS.FrontFace.StencilFailOp = grcRSV::STENCILOP_KEEP;
	noDepthReadStateDS.FrontFace.StencilDepthFailOp = grcRSV::STENCILOP_KEEP;
	noDepthReadStateDS.FrontFace.StencilPassOp = grcRSV::STENCILOP_REPLACE;
	noDepthReadStateDS.FrontFace.StencilFunc = grcRSV::CMP_ALWAYS;
	noDepthReadStateDS.BackFace.StencilFailOp = grcRSV::STENCILOP_KEEP;
	noDepthReadStateDS.BackFace.StencilDepthFailOp = grcRSV::STENCILOP_KEEP;
	noDepthReadStateDS.BackFace.StencilPassOp = grcRSV::STENCILOP_REPLACE;
	noDepthReadStateDS.BackFace.StencilFunc = grcRSV::CMP_ALWAYS;

	ms_gNoDepthRead_DS = grcStateBlock::CreateDepthStencilState(noDepthReadStateDS);

	grcRasterizerStateDesc faceCullingRS;
	faceCullingRS.CullMode=grcRSV::CULL_FRONT;
	ms_frontFaceCulling_RS = grcStateBlock::CreateRasterizerState(faceCullingRS);
	faceCullingRS.CullMode=grcRSV::CULL_NONE;
	ms_disableFaceCulling_RS = grcStateBlock::CreateRasterizerState(faceCullingRS);
	
	grcDepthStencilStateDesc DefaultDepthStencilInvertStateBlockDesc;
	DefaultDepthStencilInvertStateBlockDesc.DepthFunc = rage::FixupDepthDirection(grcRSV::CMP_LESS);
	DSS_Default_Invert = grcStateBlock::CreateDepthStencilState(DefaultDepthStencilInvertStateBlockDesc);
	DefaultDepthStencilInvertStateBlockDesc.DepthFunc = rage::FixupDepthDirection(grcRSV::CMP_LESSEQUAL);
	DSS_LessEqual_Invert = grcStateBlock::CreateDepthStencilState(DefaultDepthStencilInvertStateBlockDesc);
	DefaultDepthStencilInvertStateBlockDesc.DepthWriteMask = false;
	DSS_TestOnly_LessEqual_Invert = grcStateBlock::CreateDepthStencilState(DefaultDepthStencilInvertStateBlockDesc);
	DefaultDepthStencilInvertStateBlockDesc.DepthFunc = rage::FixupDepthDirection(grcRSV::CMP_LESS);
	DSS_TestOnly_Invert = grcStateBlock::CreateDepthStencilState(DefaultDepthStencilInvertStateBlockDesc);

	grcBlendStateDesc noColorWrite;
	noColorWrite.IndependentBlendEnable = true;
	noColorWrite.BlendRTDesc[0].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_NONE;
	noColorWrite.BlendRTDesc[1].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_NONE;
	noColorWrite.BlendRTDesc[2].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_NONE;
	noColorWrite.BlendRTDesc[3].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_NONE;
	BS_NoColorWrite = grcStateBlock::CreateBlendState(noColorWrite);
}// end of CShaderLib::Init()...
 

//
// name:		Shutdown
// description:	Shutdown shader library. 
void CShaderLib::Shutdown()
{
	if (s_MtlLib)
	{
		grcMaterialLibrary::SetCurrent(NULL);
		s_MtlLib->Release();
		s_MtlLib = NULL;
	}
}


void CShaderLib::InitTxd(unsigned initmode)
{
	if(initmode == INIT_CORE)
	{
		ms_txdId = g_TxdStore.FindSlot(strStreamingObjectName("platform:/textures/graphics",0xA306598));
		Assert(ms_txdId != -1);
		CStreaming::LoadObject(ms_txdId, g_TxdStore.GetStreamingModuleId());
		g_TxdStore.AddRef(ms_txdId, REF_OTHER);
	}
}

void CShaderLib::ShutdownTxd(unsigned shutdownmode)
{
	if(shutdownmode == SHUTDOWN_CORE)
	{
		g_TxdStore.RemoveRef(ms_txdId, REF_OTHER);
		ms_txdId = -1;
	}
}

//
// name:		InitWidgets
// description:	Creates widgets for shader library
#if __BANK

static bkBank*	 ms_pShadersBank=NULL;
static bkButton* ms_pAddShadersWidgetsBtn=NULL;

static void AddShaderDbWidgetsOnDemandCB()
{
	if(ms_pShadersBank)
	{
		// remove old button:
		if(ms_pAddShadersWidgetsBtn)
		{
			ms_pShadersBank->Remove( *((bkWidget*)ms_pAddShadersWidgetsBtn) );
			ms_pAddShadersWidgetsBtn = NULL;
		}
	
		// add shaders db buttons:
		if (s_MtlLib)
			s_MtlLib->AddWidgets(*ms_pShadersBank);
	}
}

void CShaderLib::InitWidgets(Static0 BANK_ONLY(shaderReloadCB))
{
	// add debug widgets
	bkBank& bank = BANKMGR.CreateBank("Shader database");
	ms_pShadersBank = &bank;

#if __DEV
	bank.AddToggle("Disable Shader Var Caching", &ms_gDisableShaderVarCaching);
#endif // __DEV
	bank.PushGroup("Shader Reload", false);
		bank.AddButton("Reload All Shaders", (CFA(shaderReloadCB)));	
	bank.PopGroup();

#if USE_ALPHAREF
	bank.PushGroup("Alpha Refs", true);
		bank.AddSlider("global_alphaRef",			&global_alphaRef,			0, 255, 1);
		bank.AddSlider("ped_alphaRef",				&ped_alphaRef,				0, 255, 1);
		bank.AddSlider("ped_longhair_alphaRef",		&ped_longhair_alphaRef,		0, 255, 1);
		bank.AddSlider("foliage/trees_alphaRef",	&foliage_alphaRef,			0, 255, 1);
		bank.AddSlider("foliageImposter_alphaRef",	&foliageImposter_alphaRef,	0, 255, 1);
		bank.AddSlider("billboard_alphaRef",		&billboard_alphaRef,		0, 255, 1);
		bank.AddSlider("shadowBillboard_alphaRef",	&shadowBillboard_alphaRef,	0, 255, 1);
		bank.AddSlider("grass_alphaRef",			&grass_alphaRef,			0, 255, 1);
	bank.PopGroup();
#endif // USE_ALPHAREF....

	bank.AddToggle("Use Supersample transparency anti-aliasing (best for true cutout)", &useTransparencyAA);
	bank.AddSlider("Maximum MSAA level that still uses stippling", &maxStipplingAA, 1, 8, 1);

	ms_pAddShadersWidgetsBtn = bank.AddButton("Activate Shader DB bank(s).", (CFA(AddShaderDbWidgetsOnDemandCB)));	
}
#endif //__BANK...

void CShaderLib::ResetBlendState()
{
	grcStateBlock::SetBlendState(previousBS[g_RenderThreadIndex], grcStateBlock::ActiveBlendFactors, grcStateBlock::ActiveSampleMask);
}

void CShaderLib::SetGlobals(u8 natAmb, u8 artAmb, bool setAlpha, u32 alpha, bool setStipple, 
								 bool bInvertStipple, u32 stippleAlpha, 
	  							 bool bDontWriteDepth, bool bDontReadDepth, bool bDontWriteColor)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));

	if (bDontReadDepth)
	{
		Assertf(!bDontWriteDepth, "No Z read and no Z write not supported");
		grcStateBlock::SetDepthStencilState(ms_gNoDepthRead_DS, ms_gDeferredMaterial[g_RenderThreadIndex]); // need to preserve the stencil ref
	}
	else if (bDontWriteDepth)
	{
		grcStateBlock::SetDepthStencilState(ms_gNoDepthWrite_DS, ms_gDeferredMaterial[g_RenderThreadIndex]); // need to preserve the stencil ref
	}

	if( bDontWriteColor )
	{
		previousBS[g_RenderThreadIndex] = grcStateBlock::BS_Active;
		grcStateBlock::SetBlendState(BS_NoColorWrite, grcStateBlock::ActiveBlendFactors, grcStateBlock::ActiveSampleMask);
	}

	bool updateGlobals = false;
	
	if (setAlpha && !gDrawListMgr->IsExecutingDrawList(DL_RENDERPHASE_REFLECTION_MAP))
	{
		// Set alpha
		if (ms_giCurrentScalarGlobals[g_RenderThreadIndex][0] != alpha DEV_ONLY(||ms_gDisableShaderVarCaching))
		{
			SetScalarGlobalConstant(0, alpha, CShaderLib::DivideBy255(alpha));
			updateGlobals = true;
		}
	}
	
	if( setStipple )
	{
		Assert(!DRAWLISTMGR->IsExecutingWaterReflectionDrawList());

		SetStippleAlpha(stippleAlpha, bInvertStipple);
	}

	if (ms_giCurrentScalarGlobals[g_RenderThreadIndex][1] != artAmb || ms_giCurrentScalarGlobals[g_RenderThreadIndex][2] != natAmb DEV_ONLY(||ms_gDisableShaderVarCaching))
	{
		SetScalarGlobalConstant(1, artAmb, CShaderLib::DivideBy255(artAmb));
		SetScalarGlobalConstant(2, natAmb, CShaderLib::DivideBy255(natAmb));
		updateGlobals = true;
	}
	
	if (updateGlobals) { grcEffect::SetGlobalVar(ms_globalScalarsID, (Vec4V&)(ms_gCurrentScalarGlobals[g_RenderThreadIndex])); }
}

u16 CShaderLib::GetStippleMask(u32 stippleAlpha, bool 
#if !__D3D11
								bInvertStipple
#endif
								)
{
	u16 aaMaskLUT = 0xff;
	stippleAlpha = stippleAlpha & 0xff;	//Make sure we don't access out of bounds if alpha is invalid!

#if !__D3D11
	if( bInvertStipple )
	{
		aaMaskLUT = ~(ms_aaMaskLut[stippleAlpha >> 3]);
	}
	else
	{
		aaMaskLUT = ms_aaMaskLut[stippleAlpha >> 3];
	}
#else
	if (!CShaderLib::UsesStippleFades())
	{
		aaMaskLUT = ms_aaMaskLut[stippleAlpha >> 3];
	}
	else
	{
		aaMaskLUT = ms_aaMaskFadeLut[stippleAlpha >> 3];
	}
#endif // __D3D11

	return aaMaskLUT;
}

void CShaderLib::SetStippleAlpha(u32 stippleAlpha, bool bInvertStipple)
{
	// State block path, set the current BS block with a new reference.
#if RSG_ORBIS || RSG_DURANGO
		u64 sampleMask = ComputeSampleMask(stippleAlpha);
		if (bInvertStipple)
			sampleMask = ~sampleMask;

		grcStateBlock::SetBlendState(grcStateBlock::BS_Active, grcStateBlock::ActiveBlendFactors, sampleMask);
#else
	if (!CShaderLib::UsesStippleFades())
	{
		u64 sampleMask = ComputeSampleMask(stippleAlpha);
		if (bInvertStipple)
			sampleMask = ~sampleMask;

		grcStateBlock::SetBlendState(grcStateBlock::BS_Active, grcStateBlock::ActiveBlendFactors, sampleMask);
# if MSAA_EDGE_PROCESS_FADING
		const u64 activeMask = (1<<GRCDEVICE.GetMSAA()) - 1;
		if (GBuffer::IsRenderingTo() && (sampleMask & activeMask) != activeMask && DeferredLighting::IsEdgeFadeProcessingActive())
		{
			grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_Active, grcStateBlock::ActiveStencilRef | EDGE_FLAG);
		}
# endif //MSAA_EDGE_PROCESS_FADING
	}
	else
	{
		grcStateBlock::SetBlendState(grcStateBlock::BS_Active, grcStateBlock::ActiveBlendFactors, ~0ULL);

		u16 fadeMask = ms_aaMaskFadeLut[stippleAlpha >> 3];

		if (bInvertStipple)
			fadeMask = ~fadeMask;

		int u32FadeMask[4] =
		{	(fadeMask >> 0)  & 0xf,
			(fadeMask >> 4)  & 0xf,
			(fadeMask >> 8)  & 0xf,
			(fadeMask >> 12) & 0xf 
		};

		Vec4V vectorFadeMask( ((float)u32FadeMask[0])/15.0f, ((float)u32FadeMask[1])/15.0f, ((float)u32FadeMask[2])/15.0f, ((float)u32FadeMask[3])/15.0f );

		ms_FadeMask16 = fadeMask;
		grcEffect::SetGlobalVar(ms_globalFadeID,vectorFadeMask);
	}
#endif // RSG_ORBIS || RSG_DURANGO
}

bool CShaderLib::UsesStippleFades()
{
	return GRCDEVICE.GetMSAA() <= maxStipplingAA;
}

void CShaderLib::SetScreenDoorAlpha(bool setAlpha, u32 alpha, bool setStipple, bool bInvertStipple, u32 stippleAlpha, bool bDontWriteDepth)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));

	if (bDontWriteDepth)
	{
		grcStateBlock::SetDepthStencilState(ms_gNoDepthWrite_DS, ms_gDeferredMaterial[g_RenderThreadIndex]); // need to preserve the stencil ref
	}

	bool updateGlobals = false;
	
	if (setAlpha)
	{
		// Set alpha
		if (ms_giCurrentScalarGlobals[g_RenderThreadIndex][0] != alpha DEV_ONLY(||ms_gDisableShaderVarCaching))
		{
			SetScalarGlobalConstant(0, alpha, CShaderLib::DivideBy255(alpha));
			updateGlobals = true;
		}
	}
	
	if( setStipple )
	{
		Assert(!DRAWLISTMGR->IsExecutingWaterReflectionDrawList());
		
		SetStippleAlpha(stippleAlpha, bInvertStipple);
	}

	if (updateGlobals) { grcEffect::SetGlobalVar(ms_globalScalarsID, (Vec4V&)(ms_gCurrentScalarGlobals[g_RenderThreadIndex])); }
}


void CShaderLib::ResetAlpha(bool resetAlpha, bool resetStipple)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));

	bool updateGlobals = false;
	
	if( resetAlpha )
	{
		// Set alpha
		updateGlobals = ms_giCurrentScalarGlobals[g_RenderThreadIndex][0] != 255;
		SetScalarGlobalConstant(0, 255, 1.0f);
	}
	
	if( resetStipple )
	{	
		// State block path, set the current BS block with a new reference.
		grcStateBlock::SetBlendState(grcStateBlock::BS_Active, grcStateBlock::ActiveBlendFactors, ~0ULL);

#if RSG_PC
		if (CShaderLib::UsesStippleFades())
		{
			u16 fadeMask = 0xFFFF;
			int u32FadeMask[4] =
			{	(fadeMask >> 0)  & 0xf,
				(fadeMask >> 4)  & 0xf,
				(fadeMask >> 8)  & 0xf,
				(fadeMask >> 12) & 0xf 
			};

			Vec4V vectorFadeMask( ((float)u32FadeMask[0])/15.0f, ((float)u32FadeMask[1])/15.0f, ((float)u32FadeMask[2])/15.0f, ((float)u32FadeMask[3])/15.0f );

			ms_FadeMask16 = fadeMask;
			grcEffect::SetGlobalVar(ms_globalFadeID,vectorFadeMask);
		}
# if MSAA_EDGE_PROCESS_FADING
		else if (GBuffer::IsRenderingTo())
		{
			grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_Active, grcStateBlock::ActiveStencilRef & ~EDGE_FLAG);
		}
# endif // MSAA_EDGE_PROCESS_FADING
#endif // RSG_PC
	}

	if (updateGlobals) { grcEffect::SetGlobalVar(ms_globalScalarsID, (Vec4V&)(ms_gCurrentScalarGlobals[g_RenderThreadIndex])); }
}

u64 eqaa1NSampleMask[32] = 
{
	0x0000000000000000,	//	0x0000, 
	0x0000000000000000,	//	0x0000,
	0x0000000000000001,	//	0x000F, 
	0x0000000000000001,	//	0x000F, 
	0x0000000000000003,	//	0x000F,
	0x0000000000000003,	//	0x000F, 
	0x0000000000000005,	//	0x000F, 
	0x0000000000000005,	//	0x000F,
	0x0000000000000007,	//	0x000F, 
	0x0000000000000007,	//	0x000F, 
	0x000000000000000F,	//	0x000F,
	0x000100000000000F,	//	0xF00F, 
	0x000100000000000F,	//	0xF00F, 
	0x000300000000000F,	//	0xF00F,
	0x000300000000000F,	//	0xF00F, 
	0x000500000000000F,	//	0xF00F, 
	0x000500000000000F,	//	0xF00F,
	0x000700000000000F,	//	0xF00F, 
	0x000700000000000F,	//	0xF00F, 
	0x000F00000000000F,	//	0xF00F,
	0x000F00000001000F,	//	0xF0FF, 
	0x000F00000003000F,	//	0xF0FF, 
	0x000F00000005000F,	//	0xF0FF,
	0x000F00000005000F,	//	0xF0FF, 
	0x000F00000007000F,	//	0xF0FF, 
	0x000F0000000F000F,	//	0xF0FF,
	0xFFFFFFFFFFFFFFFF,	//	0xFFFF,  // Pushing full on FFFFFF's so that the hardware gets a chance to not disable any early Z optimisations.
	0xFFFFFFFFFFFFFFFF,	//	0xFFFF, 
	0xFFFFFFFFFFFFFFFF,	//	0xFFFF,
	0xFFFFFFFFFFFFFFFF,	//	0xFFFF, 
	0xFFFFFFFFFFFFFFFF,	//	0xFFFF, 
	0xFFFFFFFFFFFFFFFF,	//	0xFFFF,
};

// This is just used for MSAA fade. The other is used for FadeDiscard() in shader on PC.
u16 CShaderLib::ms_aaMaskLut[32] = { 
	 0x00, 0x00, 0x00, 0x00,
	 0x01, 0x01, 0x01, 0x01 ,
	 0x01, 0x01, 0x01, 0x01 ,
	 0x05, 0x05, 0x05, 0x05 ,
	 0x05, 0x05, 0x05, 0x05 ,
	 0x0d, 0x0d, 0x0d, 0x0d ,
	 0x0d, 0x0d, 0x0d, 0x0d ,
	 0x0f, 0x0f, 0x0f, 0x0f 
	
};

u16 CShaderLib::ms_aaMaskFadeLut[32] = { 
	0x0000, 0x0000,
	0x000F, 0x000F, 0x000F,
	0x000F, 0x000F, 0x000F,
	0x000F, 0x000F, 0x000F,
	0xF00F, 0xF00F, 0xF00F,
	0xF00F, 0xF00F, 0xF00F,
	0xF00F, 0xF00F, 0xF00F,
	0xF0FF, 0xF0FF, 0xF0FF,
	0xF0FF, 0xF0FF, 0xF0FF,
	0xFFFF, 0xFFFF, 0xFFFF,
	0xFFFF, 0xFFFF, 0xFFFF,
};

u64 CShaderLib::ComputeSampleMask(u32 stippleAlpha)
{
	// numSamples is different form numFragments if EQAA is enabled
#if DEVICE_EQAA
	const u32 numSamples = GRCDEVICE.GetMSAA().m_uSamples;
	const u32 numFragments = GRCDEVICE.GetMSAA().m_uFragments;
#else
	const u32 numSamples = GRCDEVICE.GetMSAA();
	const u32 numFragments = numSamples;
#endif // DEVICE_EQAA

	u64 sampleMask = ~0x0UL;
	u32 baseMask = ~0x0U;
	if( (numFragments == 1 && numSamples != numFragments)
#if RSG_ORBIS || RSG_DURANGO
		|| numFragments == 0
#endif // RSG_ORBIS || RSG_DURANGO
		)
	{ // EQAA 1/N, we need to go special..
		return eqaa1NSampleMask[stippleAlpha >> 3];
	}
	else
	{
		// the mask is designed for MSAA-4, hence we modify it
		const u32 origMask = ms_aaMaskLut[stippleAlpha >> 3];
		Assert( origMask < 0x10 );
		switch (numSamples)
		{
		case 2:	baseMask	= origMask>>2; break;
		case 4:	baseMask	= origMask; break;
		case 8:	baseMask	= origMask * 0x11; break;
		case 16:baseMask	= origMask * 0x1111; break;
		default: baseMask	= ~0x0U; break;
		}

		sampleMask = (((u64)baseMask & 0xFFFF)<<48) | (((u64)baseMask & 0xFFFF)<<32) | (((u64)baseMask & 0xFFFF)<<16) | (((u64)baseMask & 0xFFFF));
	}

	return sampleMask;
}


void CShaderLib::SetGlobalAlpha(float alpha)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));

	if (ms_gCurrentScalarGlobals[g_RenderThreadIndex].x!=alpha DEV_ONLY(||ms_gDisableShaderVarCaching))
	{
		SetScalarGlobalConstant(0, (u32)(alpha * 255.0f), alpha);
		grcEffect::SetGlobalVar(ms_globalScalarsID, (Vec4V&)(ms_gCurrentScalarGlobals[g_RenderThreadIndex]));
	}
}

void CShaderLib::SetAlphaTestRef(float alphaTestRef)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));
	const unsigned rti = g_RenderThreadIndex;
	if (ms_alphaTestRef[rti]!=alphaTestRef DEV_ONLY(||ms_gDisableShaderVarCaching))
	{
		ms_alphaTestRef[rti] = alphaTestRef;
		grcEffect::SetGlobalVar(globalAlphaTestRef, alphaTestRef);
	}
}

float CShaderLib::GetAlphaTestRef()
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));
	return ms_alphaTestRef[g_RenderThreadIndex];
}

void CShaderLib::SetGlobalEmissiveScale(float emissiveScale, bool ignoreDisable)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));
	
	if( !ignoreDisable && CRenderer::GetDisableArtificialLights() )
	{
		emissiveScale = GetOverridesEmissiveScale();
	}
	
	if (ms_gCurrentScalarGlobals[g_RenderThreadIndex].w!=emissiveScale DEV_ONLY(||ms_gDisableShaderVarCaching))
	{
		SetScalarGlobalConstant(3, (u32)(emissiveScale * 255.0f), emissiveScale);
		grcEffect::SetGlobalVar(ms_globalScalarsID, (Vec4V&)(ms_gCurrentScalarGlobals[g_RenderThreadIndex]));
	}
}

float CShaderLib::GetGlobalEmissiveScale()
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));
	return ms_gCurrentScalarGlobals[g_RenderThreadIndex].w;
}

void CShaderLib::SetGlobalNaturalAmbientScale(float ambScale)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));
	if (ms_gCurrentScalarGlobals[g_RenderThreadIndex].z!=ambScale DEV_ONLY(||ms_gDisableShaderVarCaching))
	{
		SetScalarGlobalConstant(2, (u32)(ambScale * 255.0f), ambScale);
		grcEffect::SetGlobalVar(ms_globalScalarsID, (Vec4V&)(ms_gCurrentScalarGlobals[g_RenderThreadIndex]));
	}
}

float CShaderLib::GetGlobalNaturalAmbientScale()
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));
	return ms_gCurrentScalarGlobals[g_RenderThreadIndex].z;
}

void CShaderLib::SetGlobalArtificialAmbientScale(float ambScale)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));
	if (ms_gCurrentScalarGlobals[g_RenderThreadIndex].y!=ambScale DEV_ONLY(||ms_gDisableShaderVarCaching))
	{
		SetScalarGlobalConstant(1, (u32)(ambScale * 255.0f), ambScale);
		grcEffect::SetGlobalVar(ms_globalScalarsID, (Vec4V&)(ms_gCurrentScalarGlobals[g_RenderThreadIndex])); 
	}
}

float CShaderLib::GetGlobalArtificialAmbientScale()
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));
	return ms_gCurrentScalarGlobals[g_RenderThreadIndex].y;
}


void CShaderLib::SetGlobalTimer(float time)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));
	if (ms_gCurrentScalarGlobals2[g_RenderThreadIndex].x != time DEV_ONLY(||ms_gDisableShaderVarCaching))
	{
		SetScalarGlobalConstant2(0, time);
		grcEffect::SetGlobalVar(ms_globalScalars2ID, (Vec4V&)(ms_gCurrentScalarGlobals2[g_RenderThreadIndex]));
	}
}

void CShaderLib::SetGlobalInInterior(bool inInterior)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));
	float inInteriorf = (inInterior) ? 1.0f : 0.0f;
	SetScalarGlobalConstant2(2, inInteriorf);
	grcEffect::SetGlobalVar(ms_globalScalars2ID, (Vec4V&)(ms_gCurrentScalarGlobals2[g_RenderThreadIndex]));
}

bool CShaderLib::GetGlobalInInterior()
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));
	return (ms_gCurrentScalarGlobals2[g_RenderThreadIndex].z == 1.0f);
}

void CShaderLib::SetGlobalAmbientOcclusionEffect(const Vector4 &ambEffect)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));
	if (ms_gCurrentAOEffectScale[g_RenderThreadIndex].x!=ambEffect.x || ms_gCurrentAOEffectScale[g_RenderThreadIndex].y!=ambEffect.y || ms_gCurrentAOEffectScale[g_RenderThreadIndex].z!=ambEffect.z || ms_gCurrentAOEffectScale[g_RenderThreadIndex].w!=ambEffect.w DEV_ONLY(||ms_gDisableShaderVarCaching))
	{
		SetCurrentAOEffectScale(ambEffect);
		grcEffect::SetGlobalVar(globalAOEffectScaleID, (Vec4V&)(ms_gCurrentAOEffectScale[g_RenderThreadIndex]));
	}
}

void CShaderLib::SetGlobalPedRimLightScale(float rimScale)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));

	if (ms_gCurrentScalarGlobals2[g_RenderThreadIndex].w!=rimScale DEV_ONLY(||ms_gDisableShaderVarCaching))
	{
		SetScalarGlobalConstant2(3, rimScale);
		grcEffect::SetGlobalVar(ms_globalScalars2ID, (Vec4V&)(ms_gCurrentScalarGlobals2[g_RenderThreadIndex]));
	}
}

void CShaderLib::SetGlobalToneMapScalers(const Vector4& scalers)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));
	SetScalarGlobalConstant3(scalers);
	grcEffect::SetGlobalVar(ms_globalScalars3ID, (Vec4V&)(ms_gCurrentScalarGlobals3[g_RenderThreadIndex]));
}

Vector4 CShaderLib::GetGlobalToneMapScalers() 
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));
	return ms_gCurrentScalarGlobals3[g_RenderThreadIndex].asVector4();
}

void CShaderLib::SetGlobalScreenSize(const Vector2& screenSize)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));
	if (ms_gCurrentScreenSize[g_RenderThreadIndex].x!=screenSize.x || ms_gCurrentScreenSize[g_RenderThreadIndex].y!=screenSize.y DEV_ONLY(||ms_gDisableShaderVarCaching))
	{
		SetCurrentScreenSize(screenSize);
		grcEffect::SetGlobalVar(globalScreenSizeID, (Vec4V&)(ms_gCurrentScreenSize[g_RenderThreadIndex]));
	}
}

void CShaderLib::SetPlayerFeetPos(const Vector4& LFootPos, const Vector4& RFootPos)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));

	const u8 rtIdx = g_RenderThreadIndex;

	if (ms_gCurrentPlayerLFootPos[rtIdx].x!=LFootPos.x	||
		ms_gCurrentPlayerLFootPos[rtIdx].y!=LFootPos.y	||
		ms_gCurrentPlayerLFootPos[rtIdx].z!=LFootPos.z	DEV_ONLY(||ms_gDisableShaderVarCaching))
	{
		SetCurrentPlayerLFootPos(LFootPos);
		grcEffect::SetGlobalVar(globalPlayerLFootPosID, (Vec4V&)(ms_gCurrentPlayerLFootPos[rtIdx]));
	}

	if (ms_gCurrentPlayerRFootPos[rtIdx].x!=RFootPos.x	||
		ms_gCurrentPlayerRFootPos[rtIdx].y!=RFootPos.y	||
		ms_gCurrentPlayerRFootPos[rtIdx].z!=RFootPos.z	DEV_ONLY(||ms_gDisableShaderVarCaching))
	{
		SetCurrentPlayerRFootPos(RFootPos);
		grcEffect::SetGlobalVar(globalPlayerRFootPosID, (Vec4V&)(ms_gCurrentPlayerRFootPos[rtIdx]));
	}
}

#if SHADERLIB_USE_MSAA_PARAMS
void CShaderLib::SetMSAAParams(const grcDevice::MSAAMode &mode, bool colorDecoding)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));
#if DEVICE_EQAA
	if (mode.m_uFragments == 1)
	{
		colorDecoding = false;	// nothing to decode
	}
	// modePure is the one of Gbuffers[1,2,3],
	// required to compute a secondary fragment shift
	grcDevice::MSAAMode modePure = mode;
	if (GBUFFER_REDUCED_COVERAGE)
	{
		modePure.DisableCoverage();
	}
	const IntVector AAParams = {
		mode.m_uSamples, mode.m_uFragments,
		colorDecoding*mode.GetFmaskShift(),
		colorDecoding*modePure.GetFmaskShift()
	};
#else
	(void)colorDecoding;
	const IntVector AAParams = {mode,mode,0,0};
#endif	//DEVICE_EQAA
	grcEffect::SetGlobalVar(globalTargetAAParamsID, AAParams);

#if !RSG_ORBIS && !RSG_DURANGO
	const Vec4V vectorFadeMask(1.0f, 1.0f, 1.0f, 1.0f);
	ms_FadeMask16 = 0xFFFF;
	grcEffect::SetGlobalVar(ms_globalFadeID,vectorFadeMask);
#endif // !RSG_ORBIS

#if USE_TRANSPARENCYAA
	grcEffect::SetGlobalVar(globalTransparencyAASamples, useTransparencyAA ? (float)GRCDEVICE.GetMSAA() : 0.0f);
#endif
}
#endif // SHADERLIB_USE_MSAA_PARAMS

void CShaderLib::SetTreesParams()
{
#if DEVICE_MSAA && RSG_PC
	grcEffect::SetGlobalVar(globalTreesUseDiscardId, UsesStippleFades());
#endif
}

void CShaderLib::SetUseFogRay(bool bUse)
{
	grcEffect::SetGlobalVar(globalUseFogRayId, bUse);
}

void CShaderLib::UpdateGlobalDevScreenSize()
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));
	Vector2 screenSize=Vector2(GRCDEVICE.GetWidth()*1.0f, GRCDEVICE.GetHeight()*1.0f);
	CShaderLib::SetGlobalScreenSize(screenSize);
}

void CShaderLib::UpdateGlobalGameScreenSize()
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));
	Vector2 screenSize=Vector2(VideoResManager::GetSceneWidth()*1.0f, VideoResManager::GetSceneHeight()*1.0f);
	CShaderLib::SetGlobalScreenSize(screenSize);
}


void CShaderLib::DisableGlobalFogAndDepthFx()
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));
	Assert(ms_fogDepthFxEnabled[g_RenderThreadIndex]==true);

	CFogParams fogParams;
	fogParams.vFogParams[0].Setf(1000.0f, 2000.0f, 0.0f, ms_facingBackwards[g_RenderThreadIndex] ? 1.f : 0.f);
	fogParams.vFogParams[1].Setf(0,0,0,0);
	fogParams.vFogParams[2].Setf(0.f, 0.f, 0.f, 0.0f);
	// Send setting for no-fog, but maintain the original settings.
	grcEffect::SetGlobalVar(gFogParamsID, (Vec4V*)&fogParams, sizeof(CFogParams)/sizeof(Vec4V));

#if LINEAR_PIECEWISE_FOG
	LinearPiecewiseFog_ShaderParams linearParams; // Default is off.
	SetLinearPiecewiseFogParams(linearParams);
#endif // LINEAR_PIECEWISE_FOG

	if(!g_IsSubRenderThread)
		for(int i=0; i<NUMBER_OF_RENDER_THREADS; i++)
			ms_fogDepthFxEnabled[i] = false;
	else
		ms_fogDepthFxEnabled[g_RenderThreadIndex] = false;
}

void CShaderLib::EnableGlobalFogAndDepthFx()
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));
	Assert(ms_fogDepthFxEnabled[g_RenderThreadIndex]==false);

	if(!g_IsSubRenderThread)
		for(int i=0; i<NUMBER_OF_RENDER_THREADS; i++)
			ms_fogDepthFxEnabled[i] = true;
	else
		ms_fogDepthFxEnabled[g_RenderThreadIndex] = true;

	CShaderLib::ResendCurrentFogSettings();
}

void CShaderLib::SetGlobalFogParams(const CFogParams &fogParams)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));
	const float facing =  ms_facingBackwards[g_RenderThreadIndex] ? 1.f : 0.f ;

	ms_globalFogParams[g_RenderThreadIndex] = fogParams;
	ms_globalFogParams[g_RenderThreadIndex].vFogParams[0].SetW(facing);

	if(!g_IsSubRenderThread)
	{
		for(int i=0; i<NUMBER_OF_RENDER_THREADS; i++)
		{
			ms_globalFogParams[i] = fogParams;
			ms_globalFogParams[i].vFogParams[0].SetW(facing);
		}
	}

	if (ms_fogDepthFxEnabled[g_RenderThreadIndex]==false) 
		return;
	
	grcEffect::SetGlobalVar(gFogParamsID, (Vec4V*)&ms_globalFogParams[g_RenderThreadIndex], sizeof(CFogParams)/sizeof(Vec4V));
}

void CShaderLib::SetFacingBackwards( bool facingBackwards)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));

	ms_facingBackwards[g_RenderThreadIndex] = facingBackwards;
	const float facing = facingBackwards ? 1.f : 0.f;
	ms_globalFogParams[g_RenderThreadIndex].vFogParams[0].SetW(facing);

	if(!g_IsSubRenderThread)
	{
		for(int i=0; i<NUMBER_OF_RENDER_THREADS; i++)
		{
			ms_facingBackwards[i] = facingBackwards;
			ms_globalFogParams[i].vFogParams[0].SetW(facing);
		}
	}

	if (ms_fogDepthFxEnabled[g_RenderThreadIndex]==false) 
		return;

	grcEffect::SetGlobalVar(gFogParamsID, VECTOR4_TO_VEC4V(ms_globalFogParams[g_RenderThreadIndex].vFogParams[0].asVector4()));
}


void CShaderLib::ResendCurrentFogSettings()
{
	CFogParams fogParams = ms_globalFogParams[g_RenderThreadIndex];
	SetGlobalFogParams(fogParams);
}


void CShaderLib::SetGlobalFogColors(Vec3V_In vFogColorSun, Vec3V_In vFogColorAtmosphere, Vec3V_In vFogColorGround, Vec3V_In vFogColorHaze, Vec3V_In vFogColorMoon)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));

	Vec4V params[4];
	params[0] = Vec4V(vFogColorSun, vFogColorHaze.GetX());
	params[1] = Vec4V(vFogColorAtmosphere, vFogColorHaze.GetY());
	params[2] = Vec4V(vFogColorGround, vFogColorHaze.GetZ());
	params[3] = Vec4V(vFogColorMoon, ScalarV(0.0f));

	if (ms_fogDepthFxEnabled[g_RenderThreadIndex]==false) 
		return;

	grcEffect::SetGlobalVar(gFogColorID, params, 4);
}


#if LINEAR_PIECEWISE_FOG
void CShaderLib::SetLinearPiecewiseFogParams(const class LinearPiecewiseFog_ShaderParams &fogParams)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));

	for(int i=0; i<LINEAR_PIECEWISE_FOG_NUM_SHADER_VARIABLES; i++)
		(Vec4V&)(ms_linearPiecewiseFog[g_RenderThreadIndex][i]) = fogParams.shaderParams[i];

	if(!g_IsSubRenderThread)
	{
		for(int threadIdx=0; threadIdx<NUMBER_OF_RENDER_THREADS; threadIdx++)
		{
			for(int i=0; i<LINEAR_PIECEWISE_FOG_NUM_SHADER_VARIABLES; i++)
				(Vec4V&)(ms_linearPiecewiseFog[threadIdx][i]) = fogParams.shaderParams[i];
		}
	}

	if (ms_fogDepthFxEnabled[g_RenderThreadIndex]==false) 
		return;

	grcEffect::SetGlobalVar(gLinearPiecewiseFogParamsID, (Vec4V*)ms_linearPiecewiseFog[g_RenderThreadIndex], LINEAR_PIECEWISE_FOG_NUM_SHADER_VARIABLES);

}
#endif // LINEAR_PIECEWISE_FOG


void CShaderLib::SetGlobalReflectionCamDirAndEmissiveMult(const Vector3 &dir, const float emissiveMult)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));
	float actualEmissiveMult = CRenderer::GetDisableArtificialLights() ? GetOverridesEmissiveMult() : emissiveMult;
	ms_gReflectionTweaks.Setf(dir.x, dir.y, dir.z, actualEmissiveMult);
}

void CShaderLib::ApplyGlobalReflectionTweaks()
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));
	Vector4 reflectionTweaks = ms_gReflectionTweaks.asVector4();
	grcEffect::SetGlobalVar(gReflectionTweaksID, (Vec4V&)(reflectionTweaks));
}

void CShaderLib::SetGlobalDynamicBakeBoostAndWetness(Vec4V_In bakeParams)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));
	grcEffect::SetGlobalVar(gDynamicBakesAndWetnessID, bakeParams);
}

//
//
// Ingame Day/Night balance:
// 1.0 = full day
// 0.0 = full night
//
void CShaderLib::ApplyDayNightBalance()
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));

	const float dayNightBalance = g_timeCycle.GetCurrDayNightBalance();

	if (ms_gCurrentScalarGlobals2[g_RenderThreadIndex].y != dayNightBalance DEV_ONLY(||ms_gDisableShaderVarCaching))
	{
		SetScalarGlobalConstant2(1, dayNightBalance);
		grcEffect::SetGlobalVar(ms_globalScalars2ID, (Vec4V&)(ms_gCurrentScalarGlobals2[g_RenderThreadIndex]));
	}
}

float CShaderLib::GetDayNightBalance()
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));
	return(ms_gCurrentScalarGlobals2[g_RenderThreadIndex].y);
}

void CShaderLib::SetMotionBlurMask(bool motion)
{
	if(ms_gMotionBlurMask != motion)
	{
		ms_gMotionBlurMask = motion;
	}
}

bool CShaderLib::GetMotionBlurMask()
{
	return(ms_gMotionBlurMask);
}

void CShaderLib::BeginMotionBlurMask()
{
	// only should do this when rendering to gbuffer:
	if(ms_gMotionBlurMask && GBuffer::IsRenderingTo())
	{
		SetGlobalDeferredMaterialBit(DEFERRED_MATERIAL_MOTIONBLUR);
	}
}

void CShaderLib::EndMotionBlurMask()
{
	// only should do this when rendering to gbuffer:
	if(ms_gMotionBlurMask && GBuffer::IsRenderingTo())
	{
		ClearGlobalDeferredMaterialBit(DEFERRED_MATERIAL_MOTIONBLUR);
	}
}

void CShaderLib::SetDeferredWetBlendEnable(bool b)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));
	if(ms_gDeferredWetBlend[g_RenderThreadIndex] != b)
	{
		ms_gDeferredWetBlend[g_RenderThreadIndex] = b;

		if(!g_IsSubRenderThread)
		{
			for(int i=0; i<NUMBER_OF_RENDER_THREADS; i++)
			{
				ms_gDeferredWetBlend[i] = b;
			}
		}
	}
}
bool CShaderLib::GetDeferredWetBlendEnable()
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));
	return(ms_gDeferredWetBlend[g_RenderThreadIndex]);
}


#if __XENON
	static unsigned long sm_OldHiStencilEnable=0;
	static unsigned long sm_OldHiStencilWriteEnable=0;
#endif // __XENON

//static u32	prevStencilRef		=0,
//			prevStencilEnable	=0,
//			prevStencilMask		=0,
//			prevStencilWriteMask=0,
//			prevStencilFunc		=0,
//			prevStencilPass		=0,
//			prevStencilFail		=0,
//			prevStencilZFail	=0;

u32 CShaderLib::GetGlobalDeferredMaterial()
{
	return(ms_gDeferredMaterial[g_RenderThreadIndex]);
}

void CShaderLib::SetGlobalDeferredMaterial(u32 material, bool forceSetMaterial MSAA_EDGE_PROCESS_FADING_ONLY(, bool reset))
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));

	if (GBuffer::IsRenderingTo() || forceSetMaterial)
	{
		material &= 0xFF;	// max size is 8 bits
		const u32 finalMaterial = (material | ms_gDeferredMaterialORMask[g_RenderThreadIndex]);
		ms_gDeferredMaterial[g_RenderThreadIndex] = finalMaterial;

		if(!g_IsSubRenderThread)
		{
			for(int i=0; i<NUMBER_OF_RENDER_THREADS; i++)
			{
				ms_gDeferredMaterial[i] = finalMaterial;
			}
		}

		// State block path, set the current DSS block with a new reference.
		u8 ref = static_cast<u8>(ms_gDeferredMaterial[g_RenderThreadIndex]);
#if MSAA_EDGE_PROCESS_FADING
		if(!reset)
		{
			ref |= grcStateBlock::ActiveStencilRef & EDGE_FLAG;
		}
#endif
		grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_Active, ref);
	}
};

void CShaderLib::SetGlobalDeferredMaterialBit(u32 bit, bool forceSetMaterial)
{
	bit &= 0xFF;	// max size is 8 bits
	ms_gDeferredMaterialORMask[g_RenderThreadIndex] |= (bit);

	if(!g_IsSubRenderThread)
	{
		for(int i=0; i<NUMBER_OF_RENDER_THREADS; i++)
		{
			ms_gDeferredMaterialORMask[i] |= (bit);
		}
	}

	SetGlobalDeferredMaterial(ms_gDeferredMaterial[g_RenderThreadIndex], forceSetMaterial);
}

void CShaderLib::ClearGlobalDeferredMaterialBit(u32 bit, bool forceSetMaterial)
{
	bit &= 0xFF;	// max size is 8 bits
	ms_gDeferredMaterialORMask[g_RenderThreadIndex] &= (~bit);

	if(!g_IsSubRenderThread)
	{
		for(int i=0; i<NUMBER_OF_RENDER_THREADS; i++)
		{
			ms_gDeferredMaterialORMask[i] &= (~bit);
		}
	}

	SetGlobalDeferredMaterial(ms_gDeferredMaterial[g_RenderThreadIndex],forceSetMaterial);
}

void CShaderLib::ForceScalarGlobals2(const Vector4& vec)
{
	grcEffect::SetGlobalVar(ms_globalScalars2ID, (Vec4V&)(vec));
}

void CShaderLib::ResetScalarGlobals2()
{
	grcEffect::SetGlobalVar(ms_globalScalars2ID, (Vec4V&)(ms_gCurrentScalarGlobals2[g_RenderThreadIndex]));
}
#if RSG_PC
void CShaderLib::SetReticuleDistTexture(bool bDist)
{
	if (bDist)
		grcEffect::SetGlobalVar(gReticuleDistID, PostFX::CenterReticuleDist);
	else
		grcEffect::SetGlobalVar(gReticuleDistID, grcTexture::NoneBlack);
}

void CShaderLib::SetStereoTexture()
{
	if (GRCDEVICE.IsStereoEnabled())
		GRCDEVICE.SetStereoTexture();
}

void CShaderLib::SetStereoParams(const Vector4& vec)
{
	if (GRCDEVICE.IsStereoEnabled())
		grcEffect::SetGlobalVar(gStereoParamsID, (Vec4V&)vec);
}

void CShaderLib::SetStereoParams1(const Vector4& vec)
{
	if (GRCDEVICE.IsStereoEnabled())
		grcEffect::SetGlobalVar(gStereoParams1ID, (Vec4V&)vec);
}
#endif // __WIN32PC

void CShaderLib::SetFogRayTexture()
{
	grcEffect::SetGlobalVar(gFogRayTextureID, PostFX::GetFogRayRT());
}

void CShaderLib::SetFogRayIntensity(float fIntensity)
{
	grcEffect::SetGlobalVar(gFogRayIntensityID, fIntensity);
}

#if MSAA_EDGE_PASS
void CShaderLib::SetForcedEdge(bool bEdge)
{
	// only should do this when rendering to gbuffer:
	if(GBuffer::IsRenderingTo())
	{
		if (bEdge)
			SetGlobalDeferredMaterialBit(EDGE_FLAG);
		else
			ClearGlobalDeferredMaterialBit(EDGE_FLAG);
	}
}
#endif // MSAA_EDGE_PASS

void CShaderLib::SetAlphaRefConsts()
{
#if USE_ALPHAREF
	const float global_ar			= float(global_alphaRef)/255.0f;
	const float ped_ar				= float(ped_alphaRef)/255.0f;
	const float ped_longhair_ar		= float(ped_longhair_alphaRef)/255.0f;
	const float foliage_ar			= float(foliage_alphaRef)/255.0f;
	const float foliageImposter_ar	= float(foliageImposter_alphaRef)/255.0f;
	const float billboard_ar		= float(billboard_alphaRef)/255.0f;
	const float shadowBillboard_ar	= float(shadowBillboard_alphaRef)/255.0f;
	const float grass_ar			= float(grass_alphaRef)/255.0f;

	Vector4 vecAlphaRef0, vecAlphaRef1;
	vecAlphaRef0.x = global_ar;
	vecAlphaRef0.y = ped_ar;
	vecAlphaRef0.z = ped_longhair_ar;
	vecAlphaRef0.w = foliage_ar;

	vecAlphaRef1.x = foliageImposter_ar;
	vecAlphaRef1.y = billboard_ar;
	vecAlphaRef1.z = shadowBillboard_ar;
	vecAlphaRef1.w = grass_ar;

	grcEffect::SetGlobalVar(alphaRefVec0ID,	(Vec4V&)(vecAlphaRef0));
	grcEffect::SetGlobalVar(alphaRefVec1ID,	(Vec4V&)(vecAlphaRef1));
#endif //USE_ALPHAREF...
}

void CShaderLib::ResetAllVar()
{
	CShaderLib::SetGlobalEmissiveScale(1.0f);
	grcEffect::SetGlobalVar(globalAOEffectScaleID, (Vec4V&)(ms_gCurrentAOEffectScale[g_RenderThreadIndex]));
	grcEffect::SetGlobalVar(ms_globalScalarsID, (Vec4V&)(ms_gCurrentScalarGlobals[g_RenderThreadIndex]));
	grcEffect::SetGlobalVar(ms_globalScalars2ID, (Vec4V&)(ms_gCurrentScalarGlobals2[g_RenderThreadIndex]));
	grcEffect::SetGlobalVar(ms_globalScalars3ID, (Vec4V&)(ms_gCurrentScalarGlobals3[g_RenderThreadIndex]));
	grcEffect::SetGlobalVar(globalScreenSizeID, (Vec4V&)(ms_gCurrentScreenSize[g_RenderThreadIndex]));
}



//
//
// Update any global vars once per frame:
//
void CShaderLib::Update()
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));

	const u32 bufIdx = gRenderThreadInterface.GetUpdateBuffer();

#if RSG_PC
	ms_ShaderQuality[bufIdx] = (float)CSettingsManager::GetInstance().GetSettings().m_graphics.m_ShaderQuality; 
#endif

	CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
	if(pPlayerPed)
	{
		const s32 leftFootBoneIdx  = pPlayerPed->GetBoneIndexFromBoneTag(BONETAG_L_FOOT);
		const s32 rightFootBoneIdx = pPlayerPed->GetBoneIndexFromBoneTag(BONETAG_R_FOOT);

		if (leftFootBoneIdx != -1 && rightFootBoneIdx != -1)
		{
			Matrix34 leftFootMat;
			Matrix34 rightFootMat;
			pPlayerPed->GetGlobalMtx(leftFootBoneIdx, leftFootMat);
			pPlayerPed->GetGlobalMtx(rightFootBoneIdx, rightFootMat);

			// move positions forward, so fur collision circle better encircles both feet:
			const Vector3 vecForward(1,0,0);
			Vector3 LForward, RForward;
			leftFootMat.Transform3x3(vecForward,  LForward);
			rightFootMat.Transform3x3(vecForward, RForward);
static dev_float scaleLFwdX = 0.1f;
static dev_float scaleLFwdY = 0.1f;
static dev_float scaleLFwdZ = 0.0f;
			const Vector3 vecScaleFwd(scaleLFwdX, scaleLFwdY, scaleLFwdZ);

			Vector4 lFootPos(leftFootMat.d	+ LForward*vecScaleFwd);
			Vector4 rFootPos(rightFootMat.d	+ RForward*vecScaleFwd);
			lFootPos.w = rFootPos.w = 0.0f;

			ms_PlayerLFootPos[bufIdx] = lFootPos;
			ms_PlayerRFootPos[bufIdx] = rFootPos;
		}
	}
}

//
//
//
//
void CShaderLib::SetGlobalShaderVariables()
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));

	const u32 bufIdx = gRenderThreadInterface.GetRenderBuffer();
	SetPlayerFeetPos(ms_PlayerLFootPos[bufIdx], ms_PlayerRFootPos[bufIdx]);

#if RSG_PC
	grcEffect::SetGlobalVar(globalShaderQualityID, ms_ShaderQuality[bufIdx]);
#endif
}


