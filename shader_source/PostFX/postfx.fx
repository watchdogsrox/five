
#pragma dcl position
 //// -------------------------------------------------------------
//// RAGE Post-Processing Pipeline
//// 
//// -------------------------------------------------------------
//
#define FOG_SKY
#define POSTFX

#include "../common.fxh"

#pragma sampler 0 // We need some breathing space, so lets start at 0, despite common.fxh claiming otherwise otherwise.

#include "SeeThroughFX.fxh"

#include "../../../rage/base/src/grcore/fastquad_switch.h"
#include "../../../rage/base/src/grcore/AA_shared.h"

#define SHADOW_CASTING            (0)
#define SHADOW_CASTING_TECHNIQUES (0)
#define SHADOW_RECEIVING          (1)
#define SHADOW_RECEIVING_VS       (0)
#include "../Lighting/Shadows/localshadowglobals.fxh"
#include "../Lighting/Shadows/cascadeshadows.fxh"

#include "../../renderer/PostFX_shared.h"
#include "../../Renderer/Lights/TiledLightingSettings.h"

#define USE_DUMMY_POSTFX_SHADER (__WIN32PC && __SHADERMODEL <= 30) // dummy shader for rage builder

#if (USE_DUMMY_POSTFX_SHADER)
#pragma constant 0
#pragma message("DUMMY POSTFX SHADER")
#else
#pragma constant 130
#endif

#define FXAA_WIN32  (__WIN32PC && __SHADERMODEL < 40)

#define USE_SUPERSAMPLE_COMPOSITE	((RSG_ORBIS || __D3D11) && MULTISAMPLE_TECHNIQUES)
#define USE_AA_HDR_BUFFER			(USE_SUPERSAMPLE_COMPOSITE)
#define USE_SEPARATE_AA_PASS					(POSTFX_SEPARATE_AA_PASS && 1)
#define USE_SEPARATE_LENS_DISTORTION_PASS		(POSTFX_SEPARATE_LENS_DISTORTION_PASS && 1) // Enabled for 4s1f EQAA on the consoles (which use the non-msaa postfx shaders)
#define USE_FXAA_3			(1 && !FXAA_WIN32 && !__MAX)
#define USE_FXAA_2			(0 && !FXAA_WIN32)
#define USE_FXAA			(0 && !FXAA_WIN32)	// This needs to be enabled in PostProcessFX.cpp as well...

#define FILM_EFFECT __WIN32PC

#ifndef MULTISAMPLE_TECHNIQUES
#define MULTISAMPLE_TECHNIQUES 0
#endif

// have to use a large triangle for the SSA pass
// because it reads and writes to the same surface
#define GENERATE_LARGE_TRIANGLE	(RSG_ORBIS)

#if SAMPLE_FREQUENCY
#define SAMPLE_INDEX	IN.sampleIndex
#else
#define SAMPLE_INDEX	0
#endif

// pc/orbis/durango uses 16.16.16 rt while xenon uses 10.10.10.
// this poses a issue with final color image since 16.16.16 has much higher range.
// color range of pc/orbis/durango is clamped when postprocess begins
// BLOOMRANGE_ADJUST is needed since xenon/ps3 does unpackcolor_3h which modifies color
// and uses it to calcuate bloom intensity, which makes blooming on pc/orbis/durango look much brighter
// also, two(double) vehicle lights (console uses twin light, which is single light source)
// makes blooming more brighter on pc/orbis/durango.  Use BLOOM_ADJUST to tone down blooming
#if RSG_ORBIS || RSG_PC || RSG_DURANGO
#define BLOOM_ADJUST	0.13f
#else
#define BLOOM_ADJUST	1.0f
#endif

#define USE_PACKED7e3 (__XENON)             // Use Packed 7e3Int buffer instead of 16f HDR back buffer sampling
#define USE_DISTBLUR_BRANCH (USE_PACKED7e3) // Use a dynamic branch in the DistanceBlur function, it's a win when using Packed 7e3Int sampling.

#define WATERMARK_ALPHA_SUPPORT ((!defined(SHADER_FINAL)) && RSG_PC)

// vertex input for light classification data
struct postfxVertexTileDataInput
{
#if __PS3
	half2	pos					: POSITION;
	half4   data				: TEXCOORD0;	
#elif __XENON
	int4	pos					: POSITION;
	half4   data				: TEXCOORD0;	
#else
	half4	pos					: POSITION;
#if TILED_LIGHTING_INSTANCED_TILES
	half4 instanceData			: TEXCOORD0;
#endif //TILED_LIGHTING_INSTANCED_TILES
#endif
};

struct postfxVertexOutputLum {
	// This is used for the simple vertex and pixel shader routines
    float4 pos	: POSITION;
};

struct postfxVertexOutputPassThrough {
    DECLARE_POSITION(pos)
    float2 tex	: TEXCOORD0;
};

struct postfxVertexOutputPassThroughCompression {
	DECLARE_POSITION(pos)
	float4 tex	: TEXCOORD0;
};

struct postfxSampleInputPassThrough	{
	DECLARE_POSITION_PSIN(pos)
    float2 tex	: TEXCOORD0;
#if SAMPLE_FREQUENCY
	uint sampleIndex : SV_SampleIndex;
#endif
};


struct postfxVertexOutputDof {
	DECLARE_POSITION(pos)
	float2 tex	: TEXCOORD0;
	float2 texD0 : TEXCOORD1;
	float2 texD1 : TEXCOORD2;
	float2 texD2 : TEXCOORD3;
	float2 texD3 : TEXCOORD4;
};

struct postfxVertexOutputGradientFilter {
    DECLARE_POSITION(pos)
    float2 tex	: TEXCOORD0;
};


struct postfxVertexOutputAA {
    DECLARE_POSITION(pos)
#if USE_FXAA_3
    float2 p    : TEXCOORD0;
	float4 pPos : TEXCOORD1;
#else
	float4 tex  : TEXCOORD0;
#endif
};

struct postfxVertexOutputPassThroughP {
    DECLARE_POSITION(pos)
    float4 tex	: TEXCOORD0;
    float4 eyeRay : TEXCOORD1;
};

struct postfxPixelInputPassThroughP {
    DECLARE_POSITION_PSIN(pos)
    float4 tex	: TEXCOORD0;
    float4 eyeRay : TEXCOORD1;
};

struct postfxSampleInputPassThroughP	{
	DECLARE_POSITION_PSIN(pos)
    float4 tex			: TEXCOORD0;
    float4 eyeRay		: TEXCOORD1;
#if SAMPLE_FREQUENCY
	uint sampleIndex	: SV_SampleIndex;
#endif
};

struct postfxVertexOutputPassThroughComposite {
    DECLARE_POSITION(pos)
    float4 tex	: TEXCOORD0;
	float  tex1 : TEXCOORD1;
};

struct postfxFragmentInputPassThroughComposite {
    DECLARE_POSITION_PSIN(pos)
    float4 tex	: TEXCOORD0;
	float  tex1 : TEXCOORD1;
#if SAMPLE_FREQUENCY
	uint sampleIndex	: SV_SampleIndex;
#endif
};

struct postfxFragmentInputPassThrough {
    DECLARE_POSITION_PSIN(pos)
    float2 tex	: TEXCOORD0;
};

struct postfxFragmentInputPassThroughS {
    DECLARE_POSITION_PSIN(pos)
    float4 tex	: TEXCOORD0;
};

#if __PS3
struct postfxVertexOutputPassThrough4Tex {
    float4 pos	: POSITION;
    float4 tex	: TEXCOORD0;
    float4 tex1	: TEXCOORD1;
    float4 tex2	: TEXCOORD2;
    float4 tex3	: TEXCOORD3;
};

struct postfxVertexOutputPassThrough4TexWithLum {
    float4 pos	: POSITION;
    float4 tex	: TEXCOORD0;
    float4 tex1	: TEXCOORD1;
    float4 tex2	: TEXCOORD2;
    float4 tex3	: TEXCOORD3;
	float  tex4 : TEXCOORD4;
};
#endif // __PS3

struct postfxDamageOverlayInput {
	float4 pos		: POSITION;
	float4 col		: COLOR0;
    float3 tex		: TEXCOORD0;
};

struct postfxDamageOverlayOutput {
	DECLARE_POSITION(pos)
	float4 col		: COLOR0;
    float3 tex		: TEXCOORD0;
};

#if FXAA_CUSTOM_TUNING
BeginConstantBufferPagedDX10(postfx_fxaa, b6)
	float Bluriness;
	float ConsoleEdgeSharpness;
	float ConsoleEdgeThreshold;
	float ConsoleEdgeThresholdMin;
	#define FXAA_CONSOLE__PS3_EDGE_SHARPNESS	ConsoleEdgeSharpness
	#define FXAA_CONSOLE__PS3_EDGE_THRESHOLD	ConsoleEdgeThreshold

	float QualitySubpix;
	float EdgeThreshold;
	float EdgeThresholdMin;
	/*
	#define QualitySubpix (0.75f)				// Max=1.0, Min=0.0 Step=0.01 Default=0.75          : Sub-pixel quality (only used on FXAA Quality), see fxaaQualitySubpix below.
	#define EdgeThreshold (0.166f)				// Max=0.333, Min=0.063 Step=0.003 Default 0.166    : Edge threshold (only used on FXAA Quality), see fxaaQualityEdgeThreshold below.
	#define EdgeThresholdMin (0.0833f)			// Max=0.0833, Min=0.0312 Step=0.001 Default=0.0833 : Min edge threshold (only used on FXAA Quality), see fxaaQualityEdgeThresholdMin below.
	*/
EndConstantBufferDX10(postfx_fxaa)
#endif

// Include here for sampler allocation easiness...
#if USE_FXAA
	#include "FXAA.fxh"
#elif USE_FXAA_2
	#include "FXAA2.fxh"
#elif USE_FXAA_3

	#define FXAA_PS3 __PS3
	#define FXAA_360 __XENON

#if (RSG_PC && __SHADERMODEL >= 40)
	#define FXAA_PC	1
	//This will want changing to HLSL 4 or 5 when dx11 texture objects are used on everything
	#define FXAA_HLSL_4	1
	//#define FXAA_HLSL_4 0 // fx2cg can't handle struct as parameters like Payne
	// #define FXAA_HLSL_5 1 // Handle 5.0 as well
	#define FXAA_QUALITY__PRESET 39
	#define FXAA_PRESET 5

#elif RSG_ORBIS || RSG_DURANGO
	#define FXAA_PC	1
	#define FXAA_HLSL_5 1
	//#define FXAA_HLSL_3	1

	#define FXAA_QUALITY__PRESET 39
	#define FXAA_PRESET 5
#else
	#define FXAA_HLSL_3	1
#endif

	#include "FXAA3.h"
#endif

//// Variables
// Miscs

BeginConstantBufferPagedDX10(postfx_cbuffer, b5)

// DOF
float4 dofProj : DOF_PROJ; //near plane, far plane, tanh, tanv
float4 dofShear: DOF_SHEAR;	//shear X, shear Y
float4 dofDist : DOF_PARAMS; // blur start distance, blur ramp length, blur strength multiplier, unused
float4 hiDofParams : HI_DOF_PARAMS; // start near, end near, start far, end far

float4 hiDofMiscParams;
#define hiDofUseSmallBlurOnly	hiDofMiscParams.x

#define hiDofWorldNearStart		hiDofParams.x
#define hiDofWorldNearRangeRcp	hiDofParams.y
#define hiDofWorldFarStart		hiDofParams.z
#define hiDofWorldFarRangeRcp	hiDofParams.w

#define hiDofWorldNearStart4	hiDofParams.xxxx
#define hiDofWorldNearRangeRcp4	hiDofParams.yyyy
#define hiDofWorldFarStart4		hiDofParams.zzzz
#define hiDofWorldFarRangeRcp4	hiDofParams.wwww

float4 PostFXAdaptiveDofEnvBlurParams;
#define	AdaptiveDofEnvBlurMaxFarInFocusDistance		PostFXAdaptiveDofEnvBlurParams.x
#define AdaptiveDofEnvBlurMaxFarOutOfFocusDistance	PostFXAdaptiveDofEnvBlurParams.y
#define AdaptiveDofEnvBlurMinBlurRadius				PostFXAdaptiveDofEnvBlurParams.z
#define AdaptiveDofScreenBlurFadeLevel				PostFXAdaptiveDofEnvBlurParams.w

float4 PostFXAdaptiveDofCustomPlanesParams;
#define	AdaptiveDofCustomPlanesBlendLevel			PostFXAdaptiveDofCustomPlanesParams.x
#define AdaptiveDofCustomPlanesBlurRadius			PostFXAdaptiveDofCustomPlanesParams.y
#define AdaptiveDofBlurDiskRadiusPowerFactorNear	PostFXAdaptiveDofCustomPlanesParams.z

// Bloom
float4 BloomParams;
#define BloomThreshold				(BloomParams.x)
#define BloomIntensity				(BloomParams.y)
#define BloomThresholdOOMaxMinusMin	(BloomParams.z)
#define BloomCompositeMultiplier	(BloomParams.w)

// Tonemapping
float4 Filmic0;
float4 Filmic1;

float4 BrightTonemapParams0;
float4 BrightTonemapParams1;

float4 DarkTonemapParams0;
float4 DarkTonemapParams1;

float2 TonemapParams;

#define ooWhitePoint		(Filmic0.w)


// Noise
float4 NoiseParams; 

// Motion Blur
float4 DirectionalMotionBlurParams : DirectionalMotionBlurParams; // x: directional blur length y: ghosting-fade factor, zw: velocity vector scale
float4 DirectionalMotionBlurIterParams : DirectionalMotionBlurIterParams;

#if RSG_PC
#define MB_NUM_SAMPLES		 (DirectionalMotionBlurIterParams.x)
#define MB_INV_NUM_SAMPLES	 (DirectionalMotionBlurIterParams.y)
#else
#define MB_NUM_SAMPLES		 (6)
#define MB_INV_NUM_SAMPLES	 (1.0f / (float)MB_NUM_SAMPLES)
#endif

float4 MBPrevViewProjMatrixX : MBPrevViewProjMatrixX;
float4 MBPrevViewProjMatrixY : MBPrevViewProjMatrixY;
float4 MBPrevViewProjMatrixW : MBPrevViewProjMatrixW;

float3 MBPerspectiveShearParams0 : MBPerspectiveShearParams0;
float3 MBPerspectiveShearParams1 : MBPerspectiveShearParams1;
float3 MBPerspectiveShearParams2 : MBPerspectiveShearParams2;

// Night Vission
float lowLum;
float highLum;
float topLum;

float scalerLum;

float offsetLum;
float offsetLowLum;
float offsetHighLum;

float noiseLum;
float noiseLowLum;
float noiseHighLum;

float bloomLum;

float4 colorLum;
float4 colorLowLum;
float4 colorHighLum;

// HeatHaze
float4 HeatHazeParams;
float4 HeatHazeTex1Params;
float4 HeatHazeTex2Params;
float4 HeatHazeOffsetParams;

#define HH_StartRange HeatHazeParams.x
#define HH_FarRange HeatHazeParams.y
#define HH_minIntensity HeatHazeParams.z
#define HH_maxIntensity HeatHazeParams.w

#define HH_Tex1UVTiling HeatHazeTex1Params.xy
#define HH_Tex1UVOffset HeatHazeTex1Params.zw
#define HH_Tex2UVTiling HeatHazeTex2Params.xy
#define HH_Tex2UVOffset HeatHazeTex2Params.zw

#define HH_OffsetScale	HeatHazeOffsetParams.xy
#define HH_AngleFade	HeatHazeOffsetParams.z

// Lens artefacts
float4 LensArtefactsParams0;
float4 LensArtefactsParams1;
float4 LensArtefactsParams2;
float4 LensArtefactsParams3;
float4 LensArtefactsParams4;
float4 LensArtefactsParams5;

float4 LightStreaksColorShift0;
float4 LightStreaksBlurColWeights;
float4 LightStreaksBlurDir;

#define LensArtefactsScale0					LensArtefactsParams0.xy
#define LensArtefactsOffset0				LensArtefactsParams0.zw
#define LensArtefactsOpacity0				LensArtefactsParams1.x
#define LensArtefactsFadeMaskType0			LensArtefactsParams1.y
#define LensArtefactsFadeMaskMin0			LensArtefactsParams1.z
#define LensArtefactsFadeMaskMax0			LensArtefactsParams1.w
#define LensArtefactsColorShift0			LensArtefactsParams2.xyz

#define LensArtefactsScale1					LensArtefactsParams3.xy
#define LensArtefactsOffset1				LensArtefactsParams3.zw
#define LensArtefactsOpacity1				LensArtefactsParams4.x
#define LensArtefactsFadeMaskType1			LensArtefactsParams4.y
#define LensArtefactsFadeMaskMin1			LensArtefactsParams4.z
#define LensArtefactsFadeMaskMax1			LensArtefactsParams4.w
#define LensArtefactsColorShift1			LensArtefactsParams5.xyz

// These only get used during the composite pass, so they can alias
// the constant above
#define LensArtefactsGlobalMultiplier		LensArtefactsParams2.w

#define LensArtefactsExposureMin			LensArtefactsParams0.x
#define LensArtefactsExposureOORange		LensArtefactsParams0.y
#define LensArtefactsRemappedMinMult		LensArtefactsParams0.z
#define LensArtefactsRemappedMaxMult		LensArtefactsParams0.w
#define LensArtefactsExposureDrivenFadeMult	LensArtefactsParams1.x

#if RSG_PC
float4 globalFreeAimDir;
#endif

//fog rays
float4 globalFogRayParam;
float4 globalFogRayFadeParam;
#define globalFogRayIntensity			globalFogRayParam.x
#define globalFogRayPow					globalFogRayParam.z

#define globalFogRayFadeStart			globalFogRayFadeParam.x
#define globalFogRayFadeDenominator		globalFogRayFadeParam.y
#define globalFogRayJitterScale			globalFogRayFadeParam.z
#define globalFogRayJitterBias			globalFogRayFadeParam.w

float4 lightrayParams; //rgb*intensity, effect distance;
float4 lightrayParams2; //water height, time, unused, unused
#define gLightRayColor		lightrayParams.rgb
#define gLightRayDistance	lightrayParams.w
#define gWaterHeight		lightrayParams2.x
#define gWaterShaftTime		lightrayParams2.y

// SeeThrough
float4 seeThroughParams = float4(0.25f,1.0f,0.5f,0.5f);

#define seeThroughMinNoiseAmount (seeThroughParams.x)
#define seeThroughMaxNoiseAmount (seeThroughParams.y)
#define seeThroughHiLightIntensity (seeThroughParams.z)
#define seeThroughHiLightNoise (seeThroughParams.w)

float4 seeThroughColorNear = float4(0.1,0,0.25,1.0);
float4 seeThroughColorFar = float4(0.25,0,0.75,1.0);
float4 seeThroughColorVisibleBase = float4(1,0,0,1.0);
float4 seeThroughColorVisibleWarm = float4(1,0,0,1.0);
float4 seeThroughColorVisibleHot = float4(0,1,1,0);

float4 debugParams0; // [TODO -- REMOVE FOR SHADER_FINAL]
float4 debugParams1; // [TODO -- REMOVE FOR SHADER_FINAL]

const float PLAYER_MASK = float(127.9f/255.0f);

// vignetting
float4 VignettingParams; // xyz: intensity, radius, contrast w: OO_OneMinusMidColPos
float4 VignettingColor;	// w: OO_MiddleColPos


// gradient filter
float4 GradientFilterColTop; // xyz: colour 1 w: midpoint
float4 GradientFilterColBottom; // xyz: colour 2 w: unused
float4 GradientFilterColMiddle; // xyz: colour 2 w: unused

float4 DamageOverlayMisc;

// scanline filter
float4 ScanlineFilterParams;

#define ScanlineFilterIntensity	ScanlineFilterParams.x

float ScreenBlurFade; // [0,1] range to lerp between sharp and blurred version of the screen

// color correction
float4 ColorCorrectHighLum;
float4 ColorShiftLowLum;
float Desaturate;
float Gamma;

#if FILM_EFFECT
float4 LensDistortionParams;
#endif

float4 DistortionParams;		// x: lens distortion coefficient y: lens distortion coefficient cube z: color aberration coefficient w: color aberration coefficient cube
float4 BlurVignettingParams;	// x: intensity y: radius

#if !__PS3
float4 BloomTexelSize;
#endif

#if __SHADERMODEL >= 40
float4 TexelSize;
#endif //__SHADERMODEL >= 40

float4 GBufferTexture0Param;

#if USE_FXAA_3

float2 rcpFrame;

#endif //USE_FXAA_3

float4	sslrParams:sslrParams;	 // additive reducer, ray Length, use intensity buffer, far clip plane
float3  sslrCenter:sslrCenter;

#define additiveReducer			sslrParams.x
#define rayLength				sslrParams.y
#define enableIntensityBuffer	sslrParams.z
#define farClipPlaneDistance	sslrParams.w

float4 ExposureParams0;
#define exposureCurveA			float(ExposureParams0.x)
#define exposureCurveB			float(ExposureParams0.y)
#define exposureCurveOffset		float(ExposureParams0.z)
#define exposureTweak			float(ExposureParams0.w)
float4 ExposureParams1;
#define exposureMin				float(ExposureParams1.x)
#define exposureMax				float(ExposureParams1.y)
#define exposurePush			float(ExposureParams1.z)
#define exposureTimestep		float(ExposureParams1.w)
float4 ExposureParams2;
#define exposureUnused2X		0.0f
#define averageExposure			float(ExposureParams2.y)
#define averageTimestep			float(ExposureParams2.z)
#define adaptationMult			float(ExposureParams2.w)
float4 ExposureParams3;
#define adaptationMaxStepSize	float(ExposureParams3.x)
#define adaptationThreshold		float(ExposureParams3.y)
#define adaptationMinStepSize	float(ExposureParams3.z)
#define overwritenExposure		float(ExposureParams3.w)

float4	LuminanceDownsampleOOSrcDstSize;

#if POSTFX_UNIT_QUAD
float4 QuadPosition;
float4 QuadTexCoords;
float4 QuadScale;
#if WATERMARK_ALPHA_SUPPORT
float QuadAlpha = 1.0f;
#endif
#define VIN	float2 pos :POSITION
#else
#define VIN	rageVertexInput IN
#endif	//POSTFX_UNIT_QUAD

#if BOKEH_SUPPORT
float4 BokehBrightnessParams; //x = exposureRangeMax, y = exposureRangeMin, z = brightnessThresholdMax, w = brightnessThresholdMin
float4 BokehParams1; //x = BokehBrightnessFadeThresholdMin, y = BokehBrightnessFadeThresholdMax, z = BokehBlurThreshold, w = MaxBokehSize;
float4 BokehParams2; //x = BokehShapeExposureRangeMax, y = BokehShapeExposureRangeMin, z = BokehShapeOverride (non final), w = bokehDebugOverlay (non final)
float2 DOFTargetSize;
float2 RenderTargetSize;
float BokehGlobalAlpha;
float BokehAlphaCutoff;
bool BokehEnableVar;
float BokehSortLevel;
float BokehSortLevelMask;
#if BOKEH_SORT_BITONIC_TRANSPOSE
float BokehSortTransposeMatWidth;
float BokehSortTransposeMatHeight;
#endif
#endif //__WIN32PC && __SHADERMODEL >= 50

#if DOF_TYPE_CHANGEABLE_IN_RAG
float currentDOFTechnique : currentDOFTechnique;
#endif

float4 fpvMotionBlurWeights;
float3 fpvMotionBlurVelocity;
float fpvMotionBlurSize;

EndConstantBufferDX10(postfx_cbuffer)

// Shader utils configuration (after declaration of TexelSize).
#define ENABLE_TEX2DOFFSET 1
#define ENABLE_LAZYHALF (__PS3 || (__SHADERMODEL >= 40))
#include "../Util/shader_utils.fxh"

//// Samplers

// Automatic assignment of samplers to registers has trouble detecting whether
// a sampler is also used in a vertex program, so we need to help it out.
#if __PS3
#pragma sampler 0
#endif
BeginSampler(sampler2D,PostFxTextureV0,CurLumSampler,PostFxTextureV0)
ContinueSampler(sampler2D,PostFxTextureV0,CurLumSampler,PostFxTextureV0)
   MinFilter = POINT;
   MagFilter = POINT;
   MipFilter = NONE;
   AddressU  = CLAMP;
   AddressV  = CLAMP;
EndSampler;

#if __PS3
#pragma sampler 1
#endif
BeginSampler(sampler2D,PostFxTextureV1,AdapLumSampler,PostFxTextureV1)
ContinueSampler(sampler2D,PostFxTextureV1,AdapLumSampler,PostFxTextureV1)
   MinFilter = POINT;
   MagFilter = POINT;
   MipFilter = NONE;
   AddressU  = CLAMP;
   AddressV  = CLAMP;
EndSampler;

// Miscs
BeginDX10Sampler(	sampler, TEXTURE2D_TYPE<float4>, gbufferTexture0, GBufferTextureSampler0, gbufferTexture0)
ContinueSampler(sampler, gbufferTexture0, GBufferTextureSampler0, gbufferTexture0)
	AddressU  = CLAMP;
	AddressV  = CLAMP;
	MinFilter = POINT;
	MagFilter = POINT;
	MIPFILTER = POINT;
EndSampler;

BeginDX10Sampler(	sampler, TEXTURE_DEPTH_TYPE, gbufferTextureDepth, GBufferTextureSamplerDepth, gbufferTextureDepth)
ContinueSampler(sampler, gbufferTextureDepth, GBufferTextureSamplerDepth, gbufferTextureDepth)
	AddressU  = CLAMP;
	AddressV  = CLAMP;
	MINFILTER = POINT;
	MAGFILTER = POINT;
	MIPFILTER = POINT;
EndSampler;

BeginDX10Sampler(sampler, Texture2D<float>, ResolvedDepthTexture, ResolvedDepthSampler, ResolvedDepthTexture)
ContinueSampler(sampler, ResolvedDepthTexture, ResolvedDepthSampler, ResolvedDepthTexture)
	AddressU	= CLAMP;
	AddressV	= CLAMP;
	MinFilter = POINT;
	MagFilter = POINT;
	MIPFILTER = POINT;
EndSampler;

#pragma sampler 4
BeginSampler(	sampler, gbufferTextureSSAODepth, GBufferTextureSamplerSSAODepth, gbufferTextureSSAODepth)
ContinueSampler(sampler, gbufferTextureSSAODepth, GBufferTextureSamplerSSAODepth, gbufferTextureSSAODepth)
	AddressU	= CLAMP;
	AddressV	= CLAMP;
	MAGFILTER	= POINT;
	MIPFILTER	= POINT;
EndSampler;

#pragma sampler 4
BeginSampler(sampler2D,PostFxTexture2,PostFxSampler2,PostFxTexture2)
ContinueSampler(sampler2D,PostFxTexture2,PostFxSampler2,PostFxTexture2)
	MINFILTER = LINEAR;
	MAGFILTER = LINEAR;
	MIPFILTER = NONE;
	AddressU  = CLAMP;
	AddressV  = CLAMP;
EndSampler;

#pragma sampler 5
BeginSampler(sampler2D,PostFxTexture0,HDRSampler,PostFxTexture0)
ContinueSampler(sampler2D,PostFxTexture0,HDRSampler,PostFxTexture0)
	MinFilter = LINEAR;
	MagFilter = LINEAR;
	MipFilter = LINEAR;
   AddressU  = CLAMP;
   AddressV  = CLAMP;
EndSampler;

#if MULTISAMPLE_TECHNIQUES
#pragma sampler 5
BeginDX10Sampler(	sampler, TEXTURE2D_TYPE<float4>, HDRTextureAA, HDRSamplerAA, HDRTextureAA)
ContinueSampler(sampler, HDRTextureAA, HDRSamplerAA, HDRTextureAA)
	AddressU  = CLAMP;
	AddressV  = CLAMP;
	MinFilter = POINT;
	MagFilter = POINT;
	MIPFILTER = POINT;
EndSampler;
#endif	//MULTISAMPLE_TECHNIQUES

BeginDX10Sampler(sampler2D, Texture2D<float4>, BackBufferTexture, BackBufferSampler, BackBufferTexture)
ContinueSampler(sampler2D,BackBufferTexture,BackBufferSampler,BackBufferTexture)
	MinFilter = LINEAR;
	MagFilter = LINEAR;
	MipFilter = NONE;
	AddressU  = CLAMP; // MIRROR
	AddressV  = CLAMP; // MIRROR
EndSampler;

#if 1 == __PS3

BeginSampler(sampler2D,PostFxTexture0a,HDRPointSampler,PostFxTexture0a)
ContinueSampler(sampler2D,PostFxTexture0a,HDRPointSampler,PostFxTexture0a)
	MinFilter = POINT;
	MagFilter = POINT;
	MipFilter = POINT;
   AddressU  = CLAMP;
   AddressV  = CLAMP;
EndSampler;
#endif

BeginDX10Sampler(sampler,Texture2D<float4>,PostFxTexture1,BlurSampler,PostFxTexture1)
ContinueSampler(sampler,PostFxTexture1,BlurSampler,PostFxTexture1)
   MinFilter = LINEAR;
   MagFilter = LINEAR;
   MipFilter = NONE;
   AddressU  = CLAMP;
   AddressV  = CLAMP;
EndSampler;

#if !__PS3

BeginDX10Sampler(sampler, TEXTURE_STENCIL_TYPE, StencilCopyTexture, StencilCopySampler, StencilCopyTexture)
ContinueSampler(sampler, StencilCopyTexture, StencilCopySampler, StencilCopyTexture)
	AddressU  = CLAMP;        
	AddressV  = CLAMP;
	MINFILTER = POINT;
	MAGFILTER = POINT;
	MIPFILTER = POINT;
EndSampler;

#endif // !__PS3


// Motion Blur
BeginSampler(sampler2D,PostFxMotionBlurTexture,MotionBlurSampler,PostFxMotionBlurTexture)
ContinueSampler(sampler2D,PostFxMotionBlurTexture,MotionBlurSampler,PostFxMotionBlurTexture)
	MinFilter = LINEAR;
	MagFilter = LINEAR;
	MipFilter = LINEAR;
   AddressU  = CLAMP;
   AddressV  = CLAMP;
EndSampler;



// HDR
BeginSampler(sampler2D,BloomTexture,BloomSampler,BloomTexture)
ContinueSampler(sampler2D,BloomTexture,BloomSampler,BloomTexture)
   MinFilter = LINEAR;
   MagFilter = LINEAR;
   MipFilter = NONE;
   AddressU  = CLAMP;
   AddressV  = CLAMP;
  EndSampler;

#if PTFX_APPLY_DOF_TO_PARTICLES
BeginDX10Sampler(sampler2D, Texture2D<float>, PtfxDepthMapTexture, PtfxDepthMapSampler, PtfxDepthMapTexture)
ContinueSampler(sampler2D,PtfxDepthMapTexture,PtfxDepthMapSampler,PtfxDepthMapTexture)
   MinFilter = LINEAR;
   MagFilter = LINEAR;
   MipFilter = NONE;
   AddressU  = CLAMP; // MIRROR
   AddressV  = CLAMP; // MIRROR
   EndSampler;
BeginDX10Sampler(sampler2D, Texture2D<float>, PtfxAlphaMapTexture, PtfxAlphaMapSampler, PtfxAlphaMapTexture)
ContinueSampler(sampler2D,PtfxAlphaMapTexture,PtfxAlphaMapSampler,PtfxAlphaMapTexture)
   MinFilter = LINEAR;
   MagFilter = LINEAR;
   MipFilter = NONE;
   AddressU  = CLAMP; // MIRROR
   AddressV  = CLAMP; // MIRROR
EndSampler;
#endif // PTFX_APPLY_DOF_TO_PARTICLES

#pragma sampler 11
BeginSampler(sampler2D,BloomTextureG,BloomSamplerG,BloomTextureG)
ContinueSampler(sampler2D,BloomTextureG,BloomSamplerG,BloomTextureG)
#if __PS3
	MINFILTER = GAUSSIAN;
	MAGFILTER = GAUSSIAN;
	MIPFILTER = NONE;
#else
	MINFILTER = LINEAR;
	MAGFILTER = LINEAR;
	MIPFILTER = NONE;
#endif
   AddressU  = CLAMP;
   AddressV  = CLAMP;
EndSampler;

// Motion Blur (Godray shadow map downsample sampler for 360)
BeginSampler(sampler2D,JitterTexture,JitterSampler,JitterTexture)
ContinueSampler(sampler2D,JitterTexture,JitterSampler,JitterTexture)
   MinFilter = POINT;
   MagFilter = POINT;
   MipFilter = NONE;
   AddressU  = WRAP;
   AddressV  = WRAP;
EndSampler;

// Heat Haze (Godray shadow map downsample sampler for PS3)
BeginSampler(sampler2D,HeatHazeTexture,HeatHazeSampler,HeatHazeTexture)
ContinueSampler(sampler2D,HeatHazeTexture,HeatHazeSampler,HeatHazeTexture)
   MinFilter = LINEAR;
   MagFilter = LINEAR;
   MipFilter = NONE;
   AddressU  = WRAP;
   AddressV  = WRAP;
#if __PS3
   TEXTUREZFUNC = TEXTUREZFUNC_GREATER;
#endif //__PS3
EndSampler;

BeginSampler(sampler2D,PostFxTexture3,PostFxSampler3,PostFxTexture3)
ContinueSampler(sampler2D,PostFxTexture3,PostFxSampler3,PostFxTexture3)
   MinFilter = LINEAR;
   MagFilter = LINEAR;
   MipFilter = NONE;
   AddressU  = CLAMP;
   AddressV  = CLAMP;
EndSampler;

//Texture2D<float4> LensArtefactsTex;

#define POSTFX_CASCADE_INDEX 1 // <- this can be changed locally now

float4 GetDofEyeRay(float2 outPos)	{
#ifdef NVSTEREO
	float2 stereoParams = StereoParmsTexture.Load(int3(0,0,0)).xy;
	outPos -= float2(stereoParams.x,0.0f);
#endif

	const float2 projPos = (outPos + dofShear.xy) * dofProj.xy;
	return float4( mul(float4(projPos,-1,0), gViewInverse).xyz, 1);	
}

// Extract light classification data
float4 GetTileData(postfxVertexTileDataInput IN)
{
	// Get tile data
	float4 tileData;
	#if __XENON
		const int tileInfoIndex = int(IN.pos.z) + int(IN.pos.w) * 64;
		asm 
		{
			vfetch tileData, tileInfoIndex, texcoord0
		};
	#elif __PS3
		tileData = IN.data;
	#elif __SHADERMODEL >= 40
		tileData = tex2Dlod(CurLumSampler, float4(IN.pos.zw, 0, 0));
	#else
		tileData = float4(0.0, 0.0, 1.0, 1.0);
	#endif
	
	return tileData;
}

#if GENERATE_LARGE_TRIANGLE
struct postFxVertexInputPassThrough	{
	uint vertexId	: SV_VertexID;
};
postfxVertexOutputPassThrough VS_PassthroughSafe(postFxVertexInputPassThrough IN)
{
	postfxVertexOutputPassThrough OUT = (postfxVertexOutputPassThrough)0 ;
	
	float x = 2*(IN.vertexId &  1);
	float y = 2*(IN.vertexId >> 1);
	OUT.pos = float4( x*2-1, 1-y*2, 0, 1 );
	OUT.tex = float2( x, y );
  
    return OUT;
}
#else	//GENERATE_LARGE_TRIANGLE
#define VS_PassthroughSafe	VS_Passthrough
#endif	//GENERATE_LARGE_TRIANGLE

postfxVertexOutputPassThrough VS_Passthrough(VIN)
{
	postfxVertexOutputPassThrough OUT = (postfxVertexOutputPassThrough)0 ;
	
#if POSTFX_UNIT_QUAD
	OUT.pos = float4( QuadTransform(pos,QuadPosition), 0, 1 );
	OUT.tex = QuadTransform(pos,QuadTexCoords);
#else
	OUT.pos = float4(IN.pos, 1.0f);
    OUT.tex = IN.texCoord0;
#endif
    
    return OUT;
}

postfxVertexOutputPassThroughCompression VS_PassthroughCompression(VIN)
{
	postfxVertexOutputPassThroughCompression OUT = (postfxVertexOutputPassThroughCompression)0 ;

	#if POSTFX_UNIT_QUAD
		OUT.pos = float4( QuadTransform(pos,QuadPosition), 0, 1 );
		OUT.tex.xy = QuadTransform(pos,QuadTexCoords);
	#else
		OUT.pos = float4(IN.pos, 1.0f);
		OUT.tex.xy = IN.texCoord0;
	#endif

	float Pow2Exposure = tex2Dlod(AdapLumSampler, float4(0.5f, 0.5f, 0.0f, 0.0f)).x;
	OUT.tex.zw = float2(Pow2Exposure, 1.0f / Pow2Exposure);

	return OUT;
}


postfxVertexOutputDof VS_Dof(VIN)
{
	postfxVertexOutputDof OUT = (postfxVertexOutputDof)0 ;
	
#if POSTFX_UNIT_QUAD
	OUT.pos = float4( QuadTransform(pos,QuadPosition), 0, 1 );
	OUT.tex = QuadTransform(pos,QuadTexCoords);
#else
	OUT.pos = float4(IN.pos, 1.0f);
    OUT.tex = IN.texCoord0;
#endif

	OUT.texD0 = OUT.tex + float2( -1.5, -1.5 ) * gooScreenSize.xy;
	OUT.texD1 = OUT.tex + float2( -0.5, -1.5 ) * gooScreenSize.xy;
	OUT.texD2 = OUT.tex + float2( +0.5, -1.5 ) * gooScreenSize.xy;
	OUT.texD3 = OUT.tex + float2( +1.5, -1.5 ) * gooScreenSize.xy;

    return OUT;
}

postfxVertexOutputAA VS_AA(VIN)
{
	postfxVertexOutputAA OUT = (postfxVertexOutputAA)0 ;
	
#if POSTFX_UNIT_QUAD
	OUT.pos = float4( QuadTransform(pos,QuadPosition), 0, 1 );
	float2 tc = QuadTransform(pos,QuadTexCoords);
#else
	OUT.pos = float4(IN.pos, 1.0f);
	float2 tc = IN.texCoord0;
#endif

#if USE_FXAA_2
	OUT.tex = FxaaVertexShader(tc,gooScreenSize.xy);
#elif USE_FXAA_3
	OUT.p = (OUT.pos.xy * 0.5f) + 0.5f;
	OUT.p = float2(OUT.p.x, 1.0f - OUT.p.y);

#if (__PS3 || (__SHADERMODEL < 40))
	OUT.pPos.xy = OUT.p - gooScreenSize.xy * 0.5f;
	OUT.pPos.zw = OUT.p + gooScreenSize.xy * 0.5f;
#else
	OUT.pPos.xy = OUT.p;
	OUT.pPos.zw = OUT.p;
#endif

#else
    OUT.tex.xy = tc;
#endif    
    return OUT;
}

postfxVertexOutputPassThroughP VS_PassthroughP(VIN)
{
	postfxVertexOutputPassThroughP OUT = (postfxVertexOutputPassThroughP)0 ;
	
#if POSTFX_UNIT_QUAD
	OUT.pos = float4( QuadTransform(pos,QuadPosition), 0, 1 );
	OUT.tex.xy = QuadTransform(pos,QuadTexCoords).xy;
#else
	OUT.pos = float4(IN.pos, 1.0f);
    OUT.tex.xy = IN.texCoord0.xy;
#endif

	OUT.eyeRay = GetDofEyeRay(OUT.pos.xy);

	float Pow2Exposure = tex2Dlod(AdapLumSampler, float4(0.5f, 0.5f, 0.0f, 0.0f)).x;
	OUT.tex.zw = float2(Pow2Exposure, 1.0f / Pow2Exposure);

	return OUT;
}

postfxVertexOutputPassThroughP VS_PassthroughPFogRay(VIN)
{
	postfxVertexOutputPassThroughP OUT = (postfxVertexOutputPassThroughP)0 ;
	
#if POSTFX_UNIT_QUAD
	OUT.pos = float4( QuadTransform(pos,QuadPosition), 0, 1 );
	OUT.tex.xy = QuadTransform(pos,QuadTexCoords).xy;
#else
	OUT.pos = float4(IN.pos, 1.0f);
    OUT.tex.xy = IN.texCoord0.xy;
#endif

	OUT.eyeRay = GetDofEyeRay(OUT.pos.xy);

	return OUT;
}

postfxVertexOutputPassThroughP VS_DepthEffects(float2 pos : POSITION)
{
	postfxVertexOutputPassThroughP OUT = (postfxVertexOutputPassThroughP)0 ;

	OUT.eyeRay		= GetDofEyeRay(pos);
	OUT.tex.xy		= pos*.5 + .5;
	OUT.tex.y		= 1 - OUT.tex.y;
	//float linDepth	= max(min(globalFogNearDist, globalFogHazeStart)/length(OUT.eyeRay.xyz), 0); TODO: Start at 0 to be resolved by art
	float linDepth	= max(globalFogNearDist/length(OUT.eyeRay.xyz), 0);
	float projDepth = saturate(dofProj.z/linDepth - dofProj.w);

#if __XENON
	OUT.tex.xy		= OUT.tex.xy + 0.5*gooScreenSize;
	projDepth		= 1 - projDepth;
#endif

	float near		= dofProj.x;
#if USE_INVERTED_PROJECTION_ONLY
	OUT.pos			= float4(pos, fixupGBufferDepth(projDepth), 1);
#else
	OUT.pos			= float4(pos, projDepth, 1);
#endif
	return OUT;
}

#if TILED_LIGHTING_INSTANCED_TILES
float4 GetInstancedTilePosition(postfxVertexTileDataInput IN)
{
	float widthMult = IN.pos.z;
	float heightMult = IN.pos.w;
	float4 pos = IN.pos;
	pos.x += (IN.instanceData.x * widthMult);
	pos.y += 1.0f - (IN.instanceData.y * heightMult);
	pos.zw = IN.instanceData.zw;

	return pos;
}
#endif


postfxVertexOutputPassThroughP VS_DepthEffects_Tiled(postfxVertexTileDataInput IN)
{
#if TILED_LIGHTING_INSTANCED_TILES
	IN.pos = GetInstancedTilePosition(IN);
#endif //TILED_LIGHTING_INSTANCED_TILES

	float2 pos = IN.pos.xy;
	#if __XENON
		pos *= globalScreenSize.zw;
	#endif

	postfxVertexOutputPassThroughP OUT = (postfxVertexOutputPassThroughP)0 ;
	float4 tileData = GetTileData(IN);
	OUT.pos	= float4((pos - 0.5f) * 2.0f, 0, 1); 
	OUT.tex.xy = convertToVpos(OUT.pos, globalScreenSize).xy;


	OUT.pos = float4(OUT.pos.xy, globalFogNearDepth, 1.0f);
	OUT.eyeRay = GetDofEyeRay(OUT.pos.xy);

	// pass tiles with a maximum depth higher than globalFogNearDist*.80
	OUT.pos *= 	(tileData.y > globalFogNearDist*.80);

	return OUT;
}

half GetVignettingIntensity(half2 tex, half3 params)
{
#define __VignetteIntensity	params.x
#define __VignetteRadius	params.y
#define __VignetteContrast	params.z

	const half2 hTex = tex - 0.5f; 
	half intensity  = 1.0f - dot(hTex, hTex);
	intensity = saturate(pow(intensity, __VignetteRadius) + __VignetteIntensity);
	intensity = saturate(intensity*__VignetteContrast);
	return intensity;
}

postfxVertexOutputPassThrough VS_BlurVignettingTiled(postfxVertexTileDataInput IN)
{
#if TILED_LIGHTING_INSTANCED_TILES
	IN.pos = GetInstancedTilePosition(IN);
#endif //TILED_LIGHTING_INSTANCED_TILES

	float2 pos = IN.pos.xy;

	postfxVertexOutputPassThrough OUT = (postfxVertexOutputPassThrough)0 ;
	float4 tileData = GetTileData(IN);
	OUT.pos	= float4((pos - 0.5f) * 2.0f, 0, 1); 
	OUT.tex.xy = convertToVpos(OUT.pos, globalScreenSize).xy;


	OUT.pos = float4(OUT.pos.xy, 0.0f, 1.0f);

	float vignetteBlurIntensity = GetVignettingIntensity(OUT.tex.xy, float3(BlurVignettingParams.x, BlurVignettingParams.y, 1.0f));
	OUT.pos *= 	(vignetteBlurIntensity<1.0f);

	return OUT;
}

postfxVertexOutputPassThroughComposite VS_PassthroughComposite(VIN)
{
	postfxVertexOutputPassThroughComposite OUT = (postfxVertexOutputPassThroughComposite)0 ;
	
	//scale here
#if POSTFX_UNIT_QUAD
	OUT.pos = float4( QuadTransform(pos * QuadScale,QuadPosition), 0, 1 );
	OUT.tex.xy = QuadTransform(pos,QuadTexCoords);
#else
	OUT.pos = float4(IN.pos, 1.0f);
    OUT.tex.xy = IN.texCoord0;
#endif

	float2 Pow2Exposure = tex2Dlod(AdapLumSampler, float4(0.0f, 0.0f, 0.0f, 0.0f)).xy;
    OUT.tex.zw = float2(Pow2Exposure.x, 1 / Pow2Exposure.x);
	OUT.tex1 = Pow2Exposure.y;

    return OUT;
}

postfxVertexOutputPassThroughComposite VS_PassthroughCompositeTiled(postfxVertexTileDataInput IN)
{
#if TILED_LIGHTING_INSTANCED_TILES
	IN.pos = GetInstancedTilePosition(IN);
#endif //TILED_LIGHTING_INSTANCED_TILES

	postfxVertexOutputPassThroughComposite OUT = (postfxVertexOutputPassThroughComposite)0 ;
	

	float2 pos = IN.pos.xy;
	#if __XENON
		pos *= globalScreenSize.zw;
	#endif

	float4 tileData = GetTileData(IN);
	OUT.pos	= float4((pos - 0.5f) * 2.0f, 0, 1); 
	OUT.tex.xy = convertToVpos(OUT.pos, globalScreenSize).xy;


	OUT.pos = float4(OUT.pos.xy, 1.0f, 1.0f);


	const float distanceBlurTest = (tileData.x >= dofDist.x);
	const float noDistanceBlurTest = (tileData.x < dofDist.x);
	OUT.pos *= 	lerp(distanceBlurTest, noDistanceBlurTest, dofDist.w);

	float2 Pow2Exposure = tex2Dlod(AdapLumSampler, float4(0.0f, 0.0f, 0.0f, 0.0f)).xy;
	OUT.tex.zw = float2(Pow2Exposure.x, 1 / Pow2Exposure.x);
	OUT.tex1 = Pow2Exposure.y;

    return OUT;
}


postfxVertexOutputPassThroughComposite VS_PassthroughCompositeHighDofTiled(postfxVertexTileDataInput IN)
{
	postfxVertexOutputPassThroughComposite OUT = (postfxVertexOutputPassThroughComposite)0 ;
	

	float2 pos = IN.pos.xy;
	#if __XENON
		pos *= globalScreenSize.zw;
	#endif

	float4 tileData = GetTileData(IN);
	OUT.pos	= float4((pos - 0.5f) * 2.0f, 0, 1); 
	OUT.tex.xy = convertToVpos(OUT.pos, globalScreenSize).xy;


	OUT.pos = float4(OUT.pos.xy, 1.0f, 1.0f);

	const float minDepth = tileData.x;
	const float maxDepth = tileData.y;
	const float nearEnd = hiDofMiscParams.x;
	const float farStart = hiDofMiscParams.y;

	const float blur = (minDepth <= nearEnd) || (maxDepth >= farStart);
	const float noBlur = !blur;

	OUT.pos *= 	lerp(blur, noBlur, hiDofMiscParams.w);

	float Pow2Exposure = tex2Dlod(AdapLumSampler, float4(0.0f, 0.0f, 0.0f, 0.0f)).x;
	OUT.tex.zw = float2(Pow2Exposure, 1.0 / Pow2Exposure);

    return OUT;
}

#if __PS3
postfxVertexOutputPassThrough4Tex VS_Passthrough4TexXOffset(VIN)
{
	postfxVertexOutputPassThrough4Tex OUT;
	
#if POSTFX_UNIT_QUAD
	OUT.pos = float4( QuadTransform(pos,QuadPosition), 0, 1 );
	float2 tc = QuadTransform(pos,QuadTexCoords);
#else
	OUT.pos = float4(IN.pos, 1.0f);
	float2 tc = IN.texCoord0;
#endif
 	
    OUT.tex  = float4(tc + float2( 0.0f, 0.0f) * TexelSize.xy, tc + float2(-3.0f, 0.0f) * TexelSize.xy);
    OUT.tex1 = float4(tc + float2(-2.0f, 0.0f) * TexelSize.xy, tc + float2(-1.0f, 0.0f) * TexelSize.xy);
    OUT.tex2 = float4(tc + float2( 1.0f, 0.0f) * TexelSize.xy, tc + float2( 2.0f, 0.0f) * TexelSize.xy);
    OUT.tex3 = float4(tc + float2( 3.0f, 0.0f) * TexelSize.xy, tc + float2( 0.0f, 0.0f) * TexelSize.xy);
    
    return OUT;
}


postfxVertexOutputPassThrough4Tex VS_Passthrough4TexYOffset(VIN)
{
	postfxVertexOutputPassThrough4Tex OUT;
	
#if POSTFX_UNIT_QUAD
	OUT.pos = float4( QuadTransform(pos,QuadPosition), 0, 1 );
	float2 tc = QuadTransform(pos,QuadTexCoords);
#else
	OUT.pos = float4(IN.pos, 1.0f);
	float2 tc = IN.texCoord0;
#endif
 	
    OUT.tex  = float4(tc + float2( 0.0f, 0.0f) * TexelSize.xy, tc + float2( 0.0f, -3.0f) * TexelSize.xy);
    OUT.tex1 = float4(tc + float2( 0.0f,-2.0f) * TexelSize.xy, tc + float2( 0.0f, -1.0f) * TexelSize.xy);
    OUT.tex2 = float4(tc + float2( 0.0f, 1.0f) * TexelSize.xy, tc + float2( 0.0f,  2.0f) * TexelSize.xy);
    OUT.tex3 = float4(tc + float2( 0.0f, 3.0f) * TexelSize.xy, tc + float2( 0.0f,  0.0f) * TexelSize.xy);
    
    return OUT;
}

postfxVertexOutputPassThrough4TexWithLum VS_Passthrough4TexXOffsetWithLum(VIN)
{
	postfxVertexOutputPassThrough4TexWithLum OUT;
	
#if POSTFX_UNIT_QUAD
	OUT.pos = float4( QuadTransform(pos,QuadPosition), 0, 1 );
	float2 tc = QuadTransform(pos,QuadTexCoords);
#else
	OUT.pos = float4(IN.pos, 1.0f);
	float2 tc = IN.texCoord0;
#endif
 	
    OUT.tex  = float4(tc + float2( 0.0f, 0.0f) * TexelSize.xy, tc + float2(-3.0f, 0.0f) * TexelSize.xy);
    OUT.tex1 = float4(tc + float2(-2.0f, 0.0f) * TexelSize.xy, tc + float2(-1.0f, 0.0f) * TexelSize.xy);
    OUT.tex2 = float4(tc + float2( 1.0f, 0.0f) * TexelSize.xy, tc + float2( 2.0f, 0.0f) * TexelSize.xy);
    OUT.tex3 = float4(tc + float2( 3.0f, 0.0f) * TexelSize.xy, tc + float2( 0.0f, 0.0f) * TexelSize.xy);

#if __PS3 
	float LumScale = tex2D(AdapLumSampler, float2(0.0f,0.0f)).r;;	
    OUT.tex4 = LumScale;
#endif // __PS3
    
    return OUT;
}


postfxVertexOutputPassThrough4TexWithLum VS_Passthrough4TexYOffsetWithLum(VIN)
{
	postfxVertexOutputPassThrough4TexWithLum OUT;
	
#if POSTFX_UNIT_QUAD
	OUT.pos = float4( QuadTransform(pos,QuadPosition), 0, 1 );
	float2 tc = QuadTransform(pos,QuadTexCoords);
#else
	OUT.pos = float4(IN.pos, 1.0f);
	float2 tc = IN.texCoord0;
#endif
 	
    OUT.tex  = float4(tc + float2( 0.0f, 0.0f) * TexelSize.xy, tc + float2( 0.0f, -3.0f) * TexelSize.xy);
    OUT.tex1 = float4(tc + float2( 0.0f,-2.0f) * TexelSize.xy, tc + float2( 0.0f, -1.0f) * TexelSize.xy);
    OUT.tex2 = float4(tc + float2( 0.0f, 1.0f) * TexelSize.xy, tc + float2( 0.0f,  2.0f) * TexelSize.xy);
    OUT.tex3 = float4(tc + float2( 0.0f, 3.0f) * TexelSize.xy, tc + float2( 0.0f,  0.0f) * TexelSize.xy);

#if __PS3 
	float LumScale = tex2D(AdapLumSampler, float2(0.0f,0.0f)).r;;	
    OUT.tex4 = LumScale;
#endif // __PS3
    
    return OUT;
}

#endif // __PS3

postfxVertexOutputPassThrough VS_HeatHaze_Tiled(postfxVertexTileDataInput IN)
{
#if TILED_LIGHTING_INSTANCED_TILES
	IN.pos = GetInstancedTilePosition(IN);
#endif //TILED_LIGHTING_INSTANCED_TILES

	float2 pos = IN.pos.xy;
	#if __XENON
		pos *= globalScreenSize.zw;
	#endif

	postfxVertexOutputPassThrough OUT = (postfxVertexOutputPassThrough)0 ;
	float4 tileData = GetTileData(IN);
	OUT.pos	= float4((pos - 0.5f) * 2.0f, 0, 1); 
	OUT.tex.xy = convertToVpos(OUT.pos, globalScreenSize).xy;

	// pass tiles with a minimum depth greater/equal to HH_StartRange; also skip full-sky ones
	OUT.pos *= 	(tileData.x >= HH_StartRange && tileData.z >= 0);

	return OUT;
}

postfxVertexOutputPassThrough VS_SSLRMakeCutout_Tiled(postfxVertexTileDataInput IN)
{
#if TILED_LIGHTING_INSTANCED_TILES
	IN.pos = GetInstancedTilePosition(IN);
#endif //TILED_LIGHTING_INSTANCED_TILES

	float2 pos = IN.pos.xy;
	#if __XENON
		pos *= globalScreenSize.zw;
	#endif

	postfxVertexOutputPassThrough OUT = (postfxVertexOutputPassThrough)0;
	float4 tileData = GetTileData(IN);
	OUT.pos	= float4((pos - 0.5f) * 2.0f, 0, 1); 
	OUT.tex = convertToVpos(OUT.pos, globalScreenSize).xy;

	// pass any non sky tile
	half farClip = farClipPlaneDistance * 0.98;
	OUT.pos *= (tileData.y >= farClip);

	return (OUT);
}

postfxDamageOverlayOutput VS_DamageOverlay(postfxDamageOverlayInput IN)
{
	postfxDamageOverlayOutput OUT = (postfxDamageOverlayOutput)0;
	OUT.pos = IN.pos;
	OUT.tex = IN.tex;
	OUT.tex.y = OUT.tex.y*2.0f - 1.0f;
	OUT.col = IN.col;
	return OUT;
}

// SSAO Helper Function
float GetSSAODepth(sampler depthSampler, float2 tex)
{
#if __XENON || __SHADERMODEL < 40
	return tex2D(depthSampler, tex + 0.5/float2(gScreenSize.x, gScreenSize.y)).x;
#else
	return tex2D(depthSampler, tex).x;
#endif //__XENON || __WIN32PC
}

// DOF helper function
void HiDOFBlurSamples(sampler2D screen, float2 tex, out half3 smallBlur, out half3 medBlur)  
{  

	//	 _______ _______ _______ _______ _______ 
	//	|		|		|		|		|		|
	//	|		|		G		|		H		|
	//	|___F___|_______|_______|_______|_______|
	//	|		|		|		|		|		|
	//	|		|		|		B		|		|
	//	|_______|___A___|_______|_______|___I___|
	//	|		|		|		|		|		|
	//	|		|		|	E	|		|		|
	//	|___M___|_______|_______|___D___|_______|
	//	|		|		|		|		|		|
	//	|		|		C		|		|		|
	//	|_______|_______|_______|_______|___J___|
	//	|		|		|		|		|		|
	//	|		L		|		K		|		|
	//	|_______|_______|_______|_______|_______|

	half3 A = tex2DOffset( screen, tex.xy, float2(-1,-.5)).xyz;
	half3 B = tex2DOffset( screen, tex.xy, float2(.5,-1)).xyz;
	half3 C = tex2DOffset( screen, tex.xy, float2(-.5,1)).xyz;
	half3 D = tex2DOffset( screen, tex.xy, float2(1,.5)).xyz;

	half3 E = tex2DOffset( screen, tex.xy, float2(0,0)).xyz; 

	smallBlur = UnpackColor_3h(E);
	medBlur = UnpackColor_3h(half3(A+B+C+D+E/2)/4.5f).xyz;

} 

// common function for HiDof
half3 HiDOFInterpolateSamples(float3 unblurred, float3 small, float3 med, float3 large, float t, float3 d)  
{  
	// normalised distances
	// d.x + d.y + d.z = 1
	// x: unblurred to small blur
	// y: small to medium blur
	// z: medium to large blur
	const float4 dofLerpScale = float4(-1/d.x, -1/d.y, -1/d.z, 1/d.z);
	const float4 dofLerpBias = float4( 1, (1-d.z)/d.y, 1/d.z, (d.z-1)/d.z);

	float4 weights = saturate( t * dofLerpScale + dofLerpBias );
	weights.y = min( weights.y, 1 - weights.x );
	weights.z = min( weights.z, 1 - (weights.x + weights.y) );
	weights.w = 1 - (weights.x + weights.y + weights.z);

	half3 col = unblurred*weights.x + small*weights.y + med*weights.z + large*weights.w;
	return col;
}

half3 HiDOFSample(float3 sampleC, sampler2D screenSampler, sampler2D cocSampler, float2 tex, float depth)  
{  
	const float3 normDist = float3(0.499f, 0.499f, 0.001f);	// transition (0.2495, 0.7495, 0.9995)

	half3 small, med, large, screen2, coc1, coc2;
	HiDOFBlurSamples(cocSampler, tex, coc1, coc2);
	HiDOFBlurSamples(screenSampler, tex, small, screen2);
	med = small * 0.5f + coc1 * 0.5f;
	large = screen2 * 0.5f + coc2 * 0.5f;


	med   = (hiDofUseSmallBlurOnly != 0.0f) ? small : med;
	large = (hiDofUseSmallBlurOnly != 0.0f) ? small : large;

	const float nearOODist	= hiDofWorldNearRangeRcp;
	const float nearCoC	= (float)saturate(1.0f - ((depth - hiDofWorldNearStart)*nearOODist));

	const float ooDist	= hiDofWorldFarRangeRcp;
	const float farCoC	= saturate(((depth - hiDofWorldFarStart)*ooDist));

	float coc = max(nearCoC,farCoC);
	return HiDOFInterpolateSamples(sampleC.xyz, small.xyz, med.xyz, large.xyz, coc, normDist);
}  

// this should only be used for really shallow depth of field (e.g. upclose shots)
half3 ShallowHiDOFSample(half3 sampleC, sampler2D smallBlurSampler, sampler2D medBlurSampler, sampler2D largeBlurSampler, float2 tex, float depth)  
{  
	const float3 normDist = float3(0.002f, 1.974f, 0.01f);	// transition(0.001, 0.003, 0.995)

	half3 small  = UnpackColor(h4tex2D(smallBlurSampler, tex)).xyz;
	half3 med0	 = UnpackColor(h4tex2D(medBlurSampler, tex)).xyz;
	half3 large0 = UnpackColor(h4tex2D(largeBlurSampler, tex)).xyz;
	half3 med	 = small * 0.3f + med0 * 0.7f;
	half3 large	 = med0 * 0.5f + large0 * 0.5f;

	const float nearOODist	= hiDofWorldNearRangeRcp;
	const float nearCoC	= (float)saturate(1.0f - ((depth - hiDofWorldNearStart)*nearOODist));

	const float ooDist	= hiDofWorldFarRangeRcp;
	const float farCoC	= saturate(((depth - hiDofWorldFarStart)*ooDist));

	float coc = max(nearCoC,farCoC);
	return HiDOFInterpolateSamples(sampleC.xyz, small.xyz, med.xyz, large.xyz, coc, normDist);
}

#define SSA_LOAD_ALPHA(offset,si)	tex2DOffset(alphaSampler,tc,offset).a
#if __XENON
#define SSA_LOAD_COLOR(offset,si)	tex2DOffsetDecode7e3(colorSampler,tc,offset,true).rgb
#else
#define SSA_LOAD_COLOR(offset,si)	tex2DOffset(colorSampler,tc,offset).rgb
#endif	//__XENON

half3 MixSubSampleAlphaOnePass( sampler2D colorSampler, sampler2D alphaSampler, float2 tc, float2 vpos )
{
    bool rowBeginsOpaque = SSAIsOpaquePixel(float2(0.0f, vpos.y));
    bool isOpaquePixel = SSAIsOpaquePixel(vpos);
    const float2 subPixelOffset = float2(1.0f, 0.0f);

	half3 c0 = SSA_LOAD_COLOR(0.f.xx,sampleIndex);
	half  a0 = SSA_LOAD_ALPHA(isOpaquePixel ? subPixelOffset : (0.f).xx, sampleIndex);
   
    if(a0 == 0.0f)
         return c0;

    // c0 and a0 is the transparent pixel
    // c1 is the opaque pixel
    half3 c1 = SSA_LOAD_COLOR(subPixelOffset, sampleIndex);

    a0 = isOpaquePixel ? a0 : 1.0f - a0;  // opaque
    return lerp(c0, c1, a0);
}

#define SSA_USES_CLIP (1 && (RSG_ORBIS || RSG_DURANGO || RSG_PC)) 

half3 MixSubSampleAlpha( sampler2D colorSampler, sampler2D alphaSampler, float2 tc, float2 vpos, uniform bool onePass, uniform bool isUIPass )
{
	const float2 subPixelOffset = float2(1.0f, 0.0f);
	const int s1 = 0;

	half a0 = SSA_LOAD_ALPHA(0.f.xx,sampleIndex);
	half a1 = 1.0f;
	bool isOpaqueOnePass = true;

	// optimize one pass ssa combine (enabled only when SSA_USES_CLIP)
	if (onePass)
	{
		isOpaqueOnePass = SSAIsOpaquePixelFrac(vpos);

#if SSA_USES_CLIP
		if (!isOpaqueOnePass)
		{
#if RSG_PC
			if(a0 == 0.0f || a0 == 1.0f)
				return tex2D( colorSampler, tc.xy).rgb;
#else
			rageDiscard(a0 == 0.0f || a0 == 1.0f);
#endif
		}
		else
		{
			a1 = SSA_LOAD_ALPHA(subPixelOffset,s1);
#if RSG_PC
			if(a1 == 0.0f)
				return tex2D( colorSampler, tc.xy).rgb;
#else
			rageDiscard(a1 == 0.0f);
#endif
		}
#endif
	}

	// c0 and a0 is the transparent pixel
	// c1 and a1 is the opaque pixel
	half3 c0 = SSA_LOAD_COLOR(0.f.xx,sampleIndex);
	half3 c1 = SSA_LOAD_COLOR(subPixelOffset,s1);

	if ( onePass )
	{
		//half a1 = SSA_LOAD_ALPHA(subPixelOffset,s1);
		if( !isOpaqueOnePass )
		{
			a0 = a0 == 0.f ? 1.f : a0;
			return lerp(c1,c0,a0);
		}
		else
		{
			return lerp(c0,c1,a1);
		}
	}
	else
	{
#if SSA_USES_CLIP
		rageDiscard( a0 == 0.0f ); 
#else
		if(a0 == 0.0f)
			return c0;
#endif

		bool isOpaque = SSAIsOpaquePixelFrac(vpos);
		a0 = isOpaque ? a0 : 1.-a0;  // opaque	
		return lerp(c0,c1,a0);
	}
}


// Sample the depth buffer and return an unscaled value.
float getZed(float2 tex, int sampleIndex)
{
	float zed;
#if MULTISAMPLE_TECHNIQUES
	int3 iTexCoords = getIntCoords(tex, globalScreenSize.xy);
	zed = fixupGBufferDepth(gbufferTextureDepth.Load(iTexCoords,sampleIndex).x);
#else
	zed = GBufferTexDepth2D(GBufferTextureSamplerDepth,tex).x;
#endif
	
	return zed;
}

// The two of the above, in one go. awesome feat of technology.
float getDepth(float2 tex)
{
	float zed;
#if MULTISAMPLE_TECHNIQUES
	int3 iTexCoords = getIntCoords(tex, globalScreenSize.xy);
	zed = gbufferTextureDepth.Load(iTexCoords,0).x;
#else
	zed = tex2D(GBufferTextureSamplerDepth,tex).x;
#endif
	return getLinearGBufferDepth(zed, dofProj.zw);
}

float getSeeThroughStencilMask(float stencilValue)
{
	stencilValue *= 255.0f;
	return	(abs(stencilValue - (DEFERRED_MATERIAL_CLEAR)) <= 0.1f) ||
			(abs(stencilValue - (DEFERRED_MATERIAL_SPECIALBIT)) <= 0.1f);
}

float2 getLinearAndRawDepth(float2 tex, uint sampleIndex, out half notSkyMask)
{
	float zed = getZed(tex.xy,sampleIndex);
	float depth = getLinearDepth(zed, dofProj.zw);
	notSkyMask=(zed!=0.0f);
	return float2(depth, zed);
}

float GetExposure(float4 tc)
{
	return tc.z;
}

float3 SampleBloom(float2 tex)
{
	float3 simpleBlurSample = UnpackColor_3h(tex2D(BloomSampler, tex.xy).rgb);
	return simpleBlurSample;
}

half4 SampleBloom4(float2 tex, float2 compression)
{
	half4 simpleBlurSample = h4tex2D(BloomSampler, tex.xy);
	#if __PS3
		return simpleBlurSample;
	#else
		return half4(simpleBlurSample.rgb * compression.y, simpleBlurSample.a);
	#endif
}

half3 CalculateBloom(sampler2D texSampler, float2 tc)
{
	half3 bloomSample = half3(0.0f.xxx);
	float accum = 0.0f;
	for (int j = -1; j <= 1; j++)
	{
		for (int i = -1; i <= 1; i++)
		{
			bloomSample += (tex2DOffset(texSampler, tc, float2(i, j)).rgb)*(1.0f/9.0f);
		}
	}

	float pow2Exposure = h1tex2D(AdapLumSampler, 0.0f.xx).r;

	half3 unpackedColor = bloomSample;
	// pre-expose, divide by white point (range upper bound) and saturate
	const half3 linearColour = unpackedColor * pow2Exposure * ooWhitePoint;
	// compute normalised intensity
	half intensity = max(1e-9, dot(linearColour, float3(0.3333333,0.3333333,0.3333333)));

	// intensity range that will make it into the bloom buffer
	half mult = saturate((intensity - BloomThreshold) * BloomThresholdOOMaxMinusMin);
	half bloom = intensity * mult;
	
	#if !defined(SHADER_FINAL)
		half3 infColour = half3(1.0f,0.0f,1.0f) * 1000.0f; //magenta for NaN debuging
	#else
		half3 infColour = half3(0.0f, 0.0f, 0.0f);
	#endif

	return (isnan(unpackedColor) || isinf(unpackedColor)) ? infColour : max(unpackedColor, 0.0f)*(bloom/intensity);
}

half3 ApplyColorCorrection(half3 color)
{
	half3 result;

	const half lum = dot(color, LumFactors);

	// Desaturate
	color = lerp(lum.xxx, color, Desaturate); 
		
	// Colour correction
	result = color.rgb * lerp(ColorShiftLowLum.rgb, ColorCorrectHighLum.rgb, saturate(lum / ColorShiftLowLum.a)); 	

	const float correctedHighLum = 1.0 - ColorCorrectHighLum.a;
	result = lerp(result, color, saturate((lum - correctedHighLum) / max(0.01, 1.0 - correctedHighLum)));

	return saturate(result);
}

#if FILM_EFFECT

float CalcDistortionFactor(float radius, float distortionCofficent, float distortionCubeCofficent)
{
	float f = 1.0f;

	//skip the sqrt if we don't have any cubic effect
	if(distortionCubeCofficent == 0.0f)
	{
		f = 1 + radius * distortionCofficent;
	}
	else
	{
		f = 1 + radius * (distortionCofficent * distortionCubeCofficent * sqrt(radius));
	}

	return f;
}

float3 ApplyLensDistortion(float2 texCoords)
{
	float aspectRatio = gScreenSize.x / gScreenSize.y;
	float2 texCoordsOffset = float2(texCoords.x - 0.5f, texCoords.y - 0.5f);
	float radius = aspectRatio * aspectRatio * (texCoordsOffset.x * texCoordsOffset.x + texCoordsOffset.y * texCoordsOffset.y);

	float rf = CalcDistortionFactor(radius, LensDistortionParams.x - LensDistortionParams.z, LensDistortionParams.y - LensDistortionParams.w);
	float bgf = CalcDistortionFactor(radius, LensDistortionParams.x, LensDistortionParams.y);

	float3 red = tex2D(HDRSampler, float2(texCoordsOffset.x * rf + 0.5f, texCoordsOffset.y * rf + 0.5f));
	float3 greenblue = tex2D(HDRSampler, float2(texCoordsOffset.x * bgf + 0.5f, texCoordsOffset.y * bgf + 0.5f));

	return float3(red.r, greenblue.g, greenblue.b);
}
#endif

half4 PS_Min(postfxVertexOutputPassThrough IN) : COLOR     
{
#if __XENON || __PS3 || RSG_ORBIS || RSG_PC || RSG_DURANGO
	half3 sample0=(tex2DOffset(HDRSampler, IN.tex, float2(-0.5f,-0.5f))).rgb;
	half3 sample1=(tex2DOffset(HDRSampler, IN.tex, float2(-0.5f, 0.5f))).rgb;
	half3 sample2=(tex2DOffset(HDRSampler, IN.tex, float2( 0.5f, 0.5f))).rgb;
	half3 sample3=(tex2DOffset(HDRSampler, IN.tex, float2( 0.5f,-0.5f))).rgb;
	return (half4(min(min(sample0, sample1),min(sample2,sample3)),1));
#else
	half4 sampleC=half4(tex2DOffset(HDRSampler, IN.tex, float2(0.0f,0.0f)).rgb,1);
	return sampleC;			
#endif	
}

half4 PS_Bloom_Min(postfxVertexOutputPassThrough IN) : COLOR     
{
#if __XENON || __PS3 || RSG_ORBIS || RSG_PC || RSG_DURANGO
	float st = 0.95f;	
	half3 sample0=(tex2DOffset(HDRSampler, IN.tex, float2(-st,-st))).rgb;
	half3 sample1=(tex2DOffset(HDRSampler, IN.tex, float2(-st, st))).rgb;
	half3 sample2=(tex2DOffset(HDRSampler, IN.tex, float2( st, st))).rgb;
	half3 sample3=(tex2DOffset(HDRSampler, IN.tex, float2( st,-st))).rgb;	
	return (half4(min(min( sample0, sample1),min(sample2,sample3)),1));	
#else
	half4 sampleC=half4(tex2DOffset(HDRSampler, IN.tex, float2(0.0f,0.0f)).rgb,1);
	return sampleC;			
#endif	
}


half4 PS_Max(postfxVertexOutputPassThrough IN) : COLOR     
{
#if __XENON || __PS3 || RSG_ORBIS || RSG_PC || RSG_DURANGO
	half3 sample0=(tex2DOffset(HDRSampler, IN.tex, float2(-0.5f,-0.5f))).rgb;
	half3 sample1=(tex2DOffset(HDRSampler, IN.tex, float2(-0.5f, 0.5f))).rgb;
	half3 sample2=(tex2DOffset(HDRSampler, IN.tex, float2( 0.5f, 0.5f))).rgb;
	half3 sample3=(tex2DOffset(HDRSampler, IN.tex, float2( 0.5f,-0.5f))).rgb;
	return (half4(max(max(sample0, sample1),max(sample2,sample3)),1));
#else
	half4 sampleC=half4(tex2DOffset(HDRSampler, IN.tex, float2(0.0f,0.0f)).rgb,1);
	return sampleC;			
#endif	
}

float2 getOffset(float2 uv, float2 offset)
{
	return (uv - 0.5 * LuminanceDownsampleOOSrcDstSize.zw) + ((offset + 0.5f) * LuminanceDownsampleOOSrcDstSize.xy);
}

OutFloatColor PS_Lum4x3Conversion(postfxVertexOutputPassThrough IN): COLOR     
{
	#define avg(offset) dot(tex2D(CurLumSampler, getOffset(IN.tex.xy, offset)).rgb, LumFactors);
	
	float samples[12];

	samples[0] = avg(float2(0.0f, 0.0f));
	samples[1] = avg(float2(1.0f, 0.0f));
	samples[2] = avg(float2(2.0f, 0.0f));
	samples[3] = avg(float2(3.0f, 0.0f));

	samples[4] = avg(float2(0.0f, 1.0f));
	samples[5] = avg(float2(1.0f, 1.0f));
	samples[6] = avg(float2(2.0f, 1.0f));
	samples[7] = avg(float2(3.0f, 1.0f));

	samples[8] = avg(float2(0.0f, 2.0f));
	samples[9] = avg(float2(1.0f, 2.0f));
	samples[10] = avg(float2(2.0f, 2.0f));
	samples[11] = avg(float2(3.0f, 2.0f));

	float fSum = 0.0f;
	for (int i = 0; i < 12; i++)
	{
		fSum += (isnan(samples[i]) || isinf(samples[i])) ? 0.0 : samples[i];
	}

	return CastOutFloatColor(fSum / 12.0f);
}


OutFloatColor PS_Lum4x3(postfxVertexOutputPassThrough IN)	: COLOR 
{
	float fSum = 0.0f;

	fSum += tex2D(CurLumSampler, getOffset(IN.tex.xy, float2(0.0f, 0.0f)));
	fSum += tex2D(CurLumSampler, getOffset(IN.tex.xy, float2(1.0f, 0.0f)));
	fSum += tex2D(CurLumSampler, getOffset(IN.tex.xy, float2(2.0f, 0.0f)));
	fSum += tex2D(CurLumSampler, getOffset(IN.tex.xy, float2(3.0f, 0.0f)));

	fSum += tex2D(CurLumSampler, getOffset(IN.tex.xy, float2(0.0f, 1.0f)));
	fSum += tex2D(CurLumSampler, getOffset(IN.tex.xy, float2(1.0f, 1.0f)));
	fSum += tex2D(CurLumSampler, getOffset(IN.tex.xy, float2(2.0f, 1.0f)));
	fSum += tex2D(CurLumSampler, getOffset(IN.tex.xy, float2(3.0f, 1.0f)));

	fSum += tex2D(CurLumSampler, getOffset(IN.tex.xy, float2(0.0f, 2.0f)));
	fSum += tex2D(CurLumSampler, getOffset(IN.tex.xy, float2(1.0f, 2.0f)));
	fSum += tex2D(CurLumSampler, getOffset(IN.tex.xy, float2(2.0f, 2.0f)));
	fSum += tex2D(CurLumSampler, getOffset(IN.tex.xy, float2(3.0f, 2.0f)));

	return CastOutFloatColor(fSum / 12.0f);
}


OutFloatColor PS_Lum4x5(postfxVertexOutputPassThrough IN) : COLOR     
{
	float fSum = 0.0f;

	fSum += tex2D(CurLumSampler, getOffset(IN.tex.xy, float2(0.0f, 0.0f)));
	fSum += tex2D(CurLumSampler, getOffset(IN.tex.xy, float2(1.0f, 0.0f)));
	fSum += tex2D(CurLumSampler, getOffset(IN.tex.xy, float2(2.0f, 0.0f)));
	fSum += tex2D(CurLumSampler, getOffset(IN.tex.xy, float2(3.0f, 0.0f)));

	fSum += tex2D(CurLumSampler, getOffset(IN.tex.xy, float2(0.0f, 1.0f)));
	fSum += tex2D(CurLumSampler, getOffset(IN.tex.xy, float2(1.0f, 1.0f)));
	fSum += tex2D(CurLumSampler, getOffset(IN.tex.xy, float2(2.0f, 1.0f)));
	fSum += tex2D(CurLumSampler, getOffset(IN.tex.xy, float2(3.0f, 1.0f)));

	fSum += tex2D(CurLumSampler, getOffset(IN.tex.xy, float2(0.0f, 2.0f)));
	fSum += tex2D(CurLumSampler, getOffset(IN.tex.xy, float2(1.0f, 2.0f)));
	fSum += tex2D(CurLumSampler, getOffset(IN.tex.xy, float2(2.0f, 2.0f)));
	fSum += tex2D(CurLumSampler, getOffset(IN.tex.xy, float2(3.0f, 2.0f)));

	fSum += tex2D(CurLumSampler, getOffset(IN.tex.xy, float2(0.0f, 3.0f)));
	fSum += tex2D(CurLumSampler, getOffset(IN.tex.xy, float2(1.0f, 3.0f)));
	fSum += tex2D(CurLumSampler, getOffset(IN.tex.xy, float2(2.0f, 3.0f)));
	fSum += tex2D(CurLumSampler, getOffset(IN.tex.xy, float2(3.0f, 3.0f)));

	fSum += tex2D(CurLumSampler, getOffset(IN.tex.xy, float2(0.0f, 4.0f)));
	fSum += tex2D(CurLumSampler, getOffset(IN.tex.xy, float2(1.0f, 4.0f)));
	fSum += tex2D(CurLumSampler, getOffset(IN.tex.xy, float2(2.0f, 4.0f)));
	fSum += tex2D(CurLumSampler, getOffset(IN.tex.xy, float2(3.0f, 4.0f)));

	return CastOutFloatColor(fSum / 20.0f);

}

OutFloatColor PS_Lum5x5(postfxVertexOutputPassThrough IN) : COLOR     
{
	float fSum = 0.0f;

	fSum += tex2D(CurLumSampler, getOffset(IN.tex.xy, float2(0.0f, 0.0f)));
	fSum += tex2D(CurLumSampler, getOffset(IN.tex.xy, float2(1.0f, 0.0f)));
	fSum += tex2D(CurLumSampler, getOffset(IN.tex.xy, float2(2.0f, 0.0f)));
	fSum += tex2D(CurLumSampler, getOffset(IN.tex.xy, float2(3.0f, 0.0f)));
	fSum += tex2D(CurLumSampler, getOffset(IN.tex.xy, float2(4.0f, 0.0f)));

	fSum += tex2D(CurLumSampler, getOffset(IN.tex.xy, float2(0.0f, 1.0f)));
	fSum += tex2D(CurLumSampler, getOffset(IN.tex.xy, float2(1.0f, 1.0f)));
	fSum += tex2D(CurLumSampler, getOffset(IN.tex.xy, float2(2.0f, 1.0f)));
	fSum += tex2D(CurLumSampler, getOffset(IN.tex.xy, float2(3.0f, 1.0f)));
	fSum += tex2D(CurLumSampler, getOffset(IN.tex.xy, float2(4.0f, 1.0f)));

	fSum += tex2D(CurLumSampler, getOffset(IN.tex.xy, float2(0.0f, 2.0f)));
	fSum += tex2D(CurLumSampler, getOffset(IN.tex.xy, float2(1.0f, 2.0f)));
	fSum += tex2D(CurLumSampler, getOffset(IN.tex.xy, float2(2.0f, 2.0f)));
	fSum += tex2D(CurLumSampler, getOffset(IN.tex.xy, float2(3.0f, 2.0f)));
	fSum += tex2D(CurLumSampler, getOffset(IN.tex.xy, float2(4.0f, 2.0f)));

	fSum += tex2D(CurLumSampler, getOffset(IN.tex.xy, float2(0.0f, 3.0f)));
	fSum += tex2D(CurLumSampler, getOffset(IN.tex.xy, float2(1.0f, 3.0f)));
	fSum += tex2D(CurLumSampler, getOffset(IN.tex.xy, float2(2.0f, 3.0f)));
	fSum += tex2D(CurLumSampler, getOffset(IN.tex.xy, float2(3.0f, 3.0f)));
	fSum += tex2D(CurLumSampler, getOffset(IN.tex.xy, float2(4.0f, 3.0f)));

	fSum += tex2D(CurLumSampler, getOffset(IN.tex.xy, float2(0.0f, 4.0f)));
	fSum += tex2D(CurLumSampler, getOffset(IN.tex.xy, float2(1.0f, 4.0f)));
	fSum += tex2D(CurLumSampler, getOffset(IN.tex.xy, float2(2.0f, 4.0f)));
	fSum += tex2D(CurLumSampler, getOffset(IN.tex.xy, float2(3.0f, 4.0f)));
	fSum += tex2D(CurLumSampler, getOffset(IN.tex.xy, float2(4.0f, 4.0f)));

	return CastOutFloatColor(fSum / 25.0f);
}

OutFloatColor PS_Lum2x2(postfxVertexOutputPassThrough IN) : COLOR     
{
	float fSum = 0.0f;

	fSum += tex2D(CurLumSampler, getOffset(IN.tex.xy, float2(0.0f, 0.0f)));
	fSum += tex2D(CurLumSampler, getOffset(IN.tex.xy, float2(1.0f, 0.0f)));

	fSum += tex2D(CurLumSampler, getOffset(IN.tex.xy, float2(0.0f, 1.0f)));
	fSum += tex2D(CurLumSampler, getOffset(IN.tex.xy, float2(1.0f, 1.0f)));

	return CastOutFloatColor(fSum / 4.0f);

}

OutFloatColor PS_Lum3x3(postfxVertexOutputPassThrough IN) : COLOR     
{
	float fSum = 0.0f;

	fSum += tex2D(CurLumSampler, getOffset(IN.tex.xy, float2(0.0f, 0.0f)));
	fSum += tex2D(CurLumSampler, getOffset(IN.tex.xy, float2(1.0f, 0.0f)));
	fSum += tex2D(CurLumSampler, getOffset(IN.tex.xy, float2(2.0f, 0.0f)));

	fSum += tex2D(CurLumSampler, getOffset(IN.tex.xy, float2(0.0f, 1.0f)));
	fSum += tex2D(CurLumSampler, getOffset(IN.tex.xy, float2(1.0f, 1.0f)));
	fSum += tex2D(CurLumSampler, getOffset(IN.tex.xy, float2(2.0f, 1.0f)));

	fSum += tex2D(CurLumSampler, getOffset(IN.tex.xy, float2(0.0f, 3.0f)));
	fSum += tex2D(CurLumSampler, getOffset(IN.tex.xy, float2(1.0f, 3.0f)));
	fSum += tex2D(CurLumSampler, getOffset(IN.tex.xy, float2(2.0f, 3.0f)));

	return CastOutFloatColor(fSum / 9.0f);
}

float CalculateTargetExposure(float currentLum)
{
	float targetExposure = exposureCurveA * pow(currentLum, exposureCurveB) + exposureCurveOffset;

	// Target exposure adjusts (add in tweak and clamp between min and max)
	targetExposure += exposureTweak;
	targetExposure = clamp(targetExposure, exposureMin, exposureMax);

	float pushAmount = abs(targetExposure) * exposurePush;
	targetExposure += pushAmount * sign(targetExposure);

	targetExposure = clamp(targetExposure, exposureMin, exposureMax);

	return targetExposure;
}

float4 PS_CalculateExposure() : COLOR
{
	float currentLum = tex2D(CurLumSampler, float2(0.5, 0.5));
	float4 prevFrameValues = tex2D(AdapLumSampler, float2(0.5, 0.5));

	float targetExposure = CalculateTargetExposure(currentLum);
	float prevExposure = prevFrameValues.g;

	float multiplier = saturate(adaptationMult * averageTimestep) / averageTimestep;

	float absExposureDiff = abs(targetExposure - prevExposure) * multiplier;
	float signExposureDiff = sign(targetExposure - prevExposure);

	float finalExposure = 0.0f;
	
	absExposureDiff = min(adaptationMaxStepSize, absExposureDiff);

	if (absExposureDiff <= adaptationMinStepSize || signExposureDiff == 0.0f)
	{
		finalExposure = targetExposure;
	}
	else
	{
		finalExposure = prevExposure + (absExposureDiff * signExposureDiff) * averageTimestep;

		finalExposure = (signExposureDiff < 0.0) ? 
			clamp(finalExposure, targetExposure, prevExposure) : 
			clamp(finalExposure, prevExposure, targetExposure);
	}

	finalExposure = clamp(finalExposure, exposureMin, exposureMax);

	return float4(pow(2.0f, finalExposure), finalExposure, targetExposure, currentLum);

}

float4 PS_ResetExposure() : COLOR
{
	float currentLum = tex2D(CurLumSampler, float2(0.5, 0.5));
	float targetExposure = CalculateTargetExposure(currentLum);

	return float4(pow(2.0f, targetExposure), targetExposure, targetExposure, currentLum);
}

#if !defined(SHADER_FINAL)
float4 PS_SetExposure() : COLOR
{
	float currentLum = tex2D(CurLumSampler, float2(0.5, 0.5));
	float targetExposure = CalculateTargetExposure(currentLum);

	float exposure = overwritenExposure;

	return float4(pow(2.0f, exposure), exposure, targetExposure, currentLum);
}
#endif

#if __PS3
half4 PS_BlurMax(postfxVertexOutputPassThrough4Tex IN): COLOR  
{
	half4 BBW=half4(0.0,0.7,0.8,0.9);
	float num=1.0f/(1.0f+dot(half4(2,2,2,2),BBW));

	// NOTE: on the ps3 we need also blur the alpha channel (it has light ray intensity in it)
	half4 samp0 = h4tex2D(BloomSampler, IN.tex.xy);

	// We separate the math in two slightly different approach : one applied on xy and the other one on z
	// That way the compiler will do a better job of dual issuing instruction on 0 and 1 unit.
	// (yes, it's real ugly, but I like it ;)
	
	// xy
	half2	sampxy =max(h2tex2D(BloomSampler, IN.tex.zw ).xy,samp0.xy)*BBW.y;
			sampxy+=max(h2tex2D(BloomSampler, IN.tex1.xy).xy,samp0.xy)*BBW.z;
			sampxy+=max(h2tex2D(BloomSampler, IN.tex1.zw).xy,samp0.xy)*BBW.w;

			sampxy+=max(h2tex2D(BloomSampler, IN.tex2.xy).xy,samp0.xy)*BBW.w;
			sampxy+=max(h2tex2D(BloomSampler, IN.tex2.zw).xy,samp0.xy)*BBW.z;
			sampxy+=max(h2tex2D(BloomSampler, IN.tex3.xy).xy,samp0.xy)*BBW.y;

	sampxy = sampxy -(dot(half4(2,2,2,2),BBW)*samp0.xy);

	//zw
	half2	sampzw =max(h4tex2D(BloomSampler, IN.tex.zw ).zw - samp0.zw,0)*BBW.y;
			sampzw+=max(h4tex2D(BloomSampler, IN.tex1.xy).zw - samp0.zw,0)*BBW.z;
			sampzw+=max(h4tex2D(BloomSampler, IN.tex1.zw).zw - samp0.zw,0)*BBW.w;

			sampzw+=max(h4tex2D(BloomSampler, IN.tex2.xy).zw - samp0.zw,0)*BBW.w;
			sampzw+=max(h4tex2D(BloomSampler, IN.tex2.zw).zw - samp0.zw,0)*BBW.z;
			sampzw+=max(h4tex2D(BloomSampler, IN.tex3.xy).zw - samp0.zw,0)*BBW.y;

	return half4(sampxy*num+samp0.xy,sampzw*num+samp0.zw);
}
#else // __PS3
half4 PS_BlurMaxX(postfxVertexOutputPassThrough IN): COLOR  
{
	half4 BBW=half4(0.0,0.7,0.8,0.9);
	float2 offset=float2(0.0f,0.0f);
	
	float3 samp0=tex2DOffset(BloomSampler, IN.tex, offset).rgb;
	float3 samp=0;

	samp+=max(tex2DOffset(BloomSampler, IN.tex, float2(-3.0f, 0.0f)+offset).rgb-samp0,0)*BBW.y;
	samp+=max(tex2DOffset(BloomSampler, IN.tex, float2(-2.0f, 0.0f)+offset).rgb-samp0,0)*BBW.z;
	samp+=max(tex2DOffset(BloomSampler, IN.tex, float2(-1.0f, 0.0f)+offset).rgb-samp0,0)*BBW.w;
	
	samp+=max(tex2DOffset(BloomSampler, IN.tex, float2( 1.0f, 0.0f)+offset).rgb-samp0,0)*BBW.w;
	samp+=max(tex2DOffset(BloomSampler, IN.tex, float2( 2.0f, 0.0f)+offset).rgb-samp0,0)*BBW.z;
	samp+=max(tex2DOffset(BloomSampler, IN.tex, float2( 3.0f, 0.0f)+offset).rgb-samp0,0)*BBW.y;

	float num=1.0f/(1.0f+dot(float4(2,2,2,2),BBW));

	return half4((samp*num)+samp0,1);
}


half4 PS_BlurMaxY(postfxVertexOutputPassThrough IN): COLOR  
{
	half4 BBW=half4(0.0,0.7,0.8,0.9);
	float2 offset=float2(0.0f,0.0f);
	
	float3 samp0=tex2DOffset(BloomSampler, IN.tex, offset).rgb;
	float3 samp=0;

	samp+=max(tex2DOffset(BloomSampler, IN.tex, float2(0.0f,-3.0f)+offset).rgb-samp0,0)*BBW.y;
	samp+=max(tex2DOffset(BloomSampler, IN.tex, float2(0.0f,-2.0f)+offset).rgb-samp0,0)*BBW.z;
	samp+=max(tex2DOffset(BloomSampler, IN.tex, float2(0.0f,-1.0f)+offset).rgb-samp0,0)*BBW.w;
	
	samp+=max(tex2DOffset(BloomSampler, IN.tex, float2(0.0f, 1.0f)+offset).rgb-samp0,0)*BBW.w;
	samp+=max(tex2DOffset(BloomSampler, IN.tex, float2(0.0f, 2.0f)+offset).rgb-samp0,0)*BBW.z;
	samp+=max(tex2DOffset(BloomSampler, IN.tex, float2(0.0f, 3.0f)+offset).rgb-samp0,0)*BBW.y;

	float num=1.0f/(1.0f+dot(float4(2,2,2,2),BBW));

	return half4((samp*num)+samp0,1);
	
}
#endif // __PS3

float distanceBlur(float depth)
{
	const float startDist = dofDist.x;
	const float ooEndDist = dofDist.y;
	float strength = saturate(ooEndDist*(depth-startDist))*dofDist.z;

	return strength;
}


half CalculatePlayerMask(half mask)
{
	return (mask > PLAYER_MASK ) ? 0.0f : 1.0f;
}


float ReadStencilMask( float2 uv 
			#if MULTISAMPLE_TECHNIQUES
					  , uint sampleIdx
			#endif
					  )
{
#if __PS3
	return h4tex2D( GBufferTextureSamplerDepth, uv ).a;
#elif MULTISAMPLE_TECHNIQUES
	return getStencilValueScreenMS( StencilCopyTexture, int3(uv * globalScreenSize.xy, 0), sampleIdx );
#elif __SHADERMODEL >= 40
	return getStencilValueScreen(StencilCopyTexture, uv * globalScreenSize.xy);
#else
	return tex2Dlod(StencilCopySampler, float4(uv,0,0) ).b;
#endif
}

//////////////////////////////////////////////////////////////////////////
// Motion blur

float2 GetMotionBlurVelocity(float2 tex, float2 depth, float lengthScale)
{
	const float2 MBVelScale = float2(0.5f, -0.5f);
	// based off 64/w, 64/h and reciprocals
	const float4 MBVelMax = float4( 0.0125f, 0.0222222222222222f, 1.0f / 0.0125f, 1.0f / 0.0222222222222222f);
	// Current NDC space position 
	// (depth.y is depth value as sampled from depth buffer)
	float4 curNDCPos = float4(tex.xy * float2(2, -2) + float2(-1, 1), depth.y, 1.0f);

	// Compute world space position
	// (depth.x is linear depth value)
    const float3 transformVec = float3( curNDCPos.xy, 1.0 );
	float4 eyeRay = float4( dot( transformVec, MBPerspectiveShearParams0), 
							dot( transformVec, MBPerspectiveShearParams1),
							dot( transformVec, MBPerspectiveShearParams2), 1);
	float4 worldPos = float4(gViewInverse[3].xyz + (eyeRay.xyz * depth.x), 1.0f);

	// Compute previous NDC space position
	float4 prevNDCPos;
	prevNDCPos.x = dot(worldPos, MBPrevViewProjMatrixX); 
	prevNDCPos.y = dot(worldPos, MBPrevViewProjMatrixY); 
	prevNDCPos.w = dot(worldPos, MBPrevViewProjMatrixW);
	prevNDCPos.w = prevNDCPos.w == 0 ? 0.00001f : prevNDCPos.w;
	prevNDCPos /= prevNDCPos.w;

	// Derive velocity
	float2 velocity = (curNDCPos-prevNDCPos).xy;
	float2 pixelVelocity = velocity*MBVelScale.xy;
	pixelVelocity *= MBVelMax.zw;
	float speed = dot(pixelVelocity, pixelVelocity);
	pixelVelocity = (speed > 1.0f) ? (pixelVelocity*rsqrt(speed)) : pixelVelocity;
	pixelVelocity *= MBVelMax.xy;

	return pixelVelocity*lengthScale;
}

float3 ApplyMotionBlur(float3 curSample, float2 tex, float2 depth, sampler2D screen, float lengthScale
						#if MULTISAMPLE_TECHNIQUES
							, uint sampleIdx
						#endif
							  )
{
	#if MULTISAMPLE_TECHNIQUES
		float sampleMask = ReadStencilMask(tex, sampleIdx);
	#else
		float sampleMask = ReadStencilMask(tex);
	#endif
	
	float playerMaskCenter = CalculatePlayerMask(sampleMask);
	float2 velocity = GetMotionBlurVelocity(tex, depth, lengthScale);

	float3 mbSamplesSum = curSample.xyz * playerMaskCenter;
	float mbWeights = playerMaskCenter;

	const float2 mbStep = velocity * MB_INV_NUM_SAMPLES;

	// Look up into our jitter texture using this 'magic' formula
	const float2 JitterOffset = { 58.164f, 47.13f };
	const float2 jitterLookup = tex * JitterOffset + (curSample.rg * 8.0f);
	float jitterScalar = h1tex2D(JitterSampler, jitterLookup) - 0.5f;
	tex += mbStep * jitterScalar*0.5f;

#if RSG_PC
	[loop]
#else
	[unroll]
#endif
	for (int i=1; i<(int)MB_NUM_SAMPLES; i++)
	{
		float2 curTC = tex.xy + mbStep*i;		
		float2 curTCr = (round((curTC * globalScreenSize.xy)) + 0.5) / globalScreenSize.xy;
		#if MULTISAMPLE_TECHNIQUES
			sampleMask = ReadStencilMask(curTCr, sampleIdx);
		#else
			sampleMask = ReadStencilMask(curTCr);
		#endif		
		float playerMask = CalculatePlayerMask(sampleMask);
#if MULTISAMPLE_TECHNIQUES
		int2 iTex = int2( round((curTC * globalScreenSize.xy)) + 0.5 );
		mbSamplesSum += HDRTextureAA.Load(iTex.xy,sampleIdx).rgb * playerMask;
#else
		mbSamplesSum += tex2Dlod(screen, float4(curTCr,0,0)).xyz * playerMask;
#endif
		mbWeights += playerMask;
	}

	float3 blurredSample = mbSamplesSum / max(mbWeights, 0.1);

	// discard motion blur result if length is near/equal to zero; need to do this because we sample from 
	// 1/4 res. buffer, otherwise we get blurry output when there's no motion
	const float discardSmallLength = saturate(dot(velocity,velocity)*100000.0f);
	// discard masked pixels
	const float discardSample = playerMaskCenter*discardSmallLength;
	return lerp(curSample, blurredSample, discardSample);
}

float SSAODecodeDepth ( float3 vCodedDepth )
{
    return dot( vCodedDepth, float3(255.0f, 1.0f, 1.0f/255.0f) );
}



//
// Screen Space Light Rays
// based on , brought over from RDR2 code base
//

struct VS_SSLR_IN
{
	float4 pos			: POSITION0;
	float2 texCoord		: TEXCOORD0;
};

struct PS_SSLR_CUTOUT_IN
{
	float4 pos						: POSITION0;
	float4 texCoordAndScreenPos		: TEXCOORD0;					// uv wz = screen pos
};

struct VS_SSLR_EXTRUDE_OUT
{
	DECLARE_POSITION(pos)
	float2 texCoord					: TEXCOORD0;		
	float4 screenPosAndRayToCenter	: TEXCOORD1;					// xy = screen pos, wz = ray to the center of the light ray card
};

struct PS_SSLR_EXTRUDE_IN
{
	DECLARE_POSITION_PSIN(pos)
	float2 texCoord					: TEXCOORD0;		
	float4 screenPosAndRayToCenter	: TEXCOORD1;					// xy = screen pos, wz = ray to the center of the light ray card
};

BeginSampler(sampler,SSLRTex, SSLRSampler, SSLRTex)
ContinueSampler(sampler, SSLRTex, SSLRSampler, SSLRTex)
	MinFilter = LINEAR;
	MagFilter = LINEAR;
	MipFilter = NONE;
    AddressU  = clamp;        
    AddressV  = clamp;
    AddressW  = clamp;
EndSampler;

#define SQUARE_IB		1   // this helps with banding if using 8 bit intensity buffers

#define IB_IN_BLOOM_ALPHA		__PS3

#define shadowShaftTranslucency 0.1


//
// helper function for light ray techniques
//
half4 CompositeLightRayAndBloom(float rayIntensity, postfxPixelInputPassThroughP IN)
{
#if SQUARE_IB
	// this should be gamma4, but light rays were tuned with a sqrt missing in sllr, so we need to use gamma 8 here to keep tuning the same
	rayIntensity = (enableIntensityBuffer>0.0) ? pow(rayIntensity,1.0/8.0) : rayIntensity;
#endif
	
	half3 outcol = rayIntensity.xxx;
	
	// calc bloom for non-intensity buffer technique
	if (enableIntensityBuffer==0.0 || IB_IN_BLOOM_ALPHA) // NOTE: on ps3 we need bloom and ray if using intensity buffer 
	{
		// if we're NOT using the intensity buffer for lightrays, we need to multiply by the light ray color
		outcol *= (enableIntensityBuffer==0.0) ? gLightRayColor : 0;  // we may just want bloom in color channel
	
		// now add in bloom color
		outcol += CalculateBloom(BloomSampler, IN.tex.xy);
	}

	return half4(outcol,rayIntensity);
}


#define SSLR_SEGMENTS 15		 // number of segments, samples are this + 1



half4	PS_SSLRMakeCutout(postfxVertexOutputPassThrough IN) : COLOR
{
	half4 result; 
	
	// NOTE: (1,1,1) in the SSAO depth sampler is "infinity", so we're looking for that.
	half farClip = farClipPlaneDistance*.98;
	result.x = tex2DOffset(GBufferTextureSamplerSSAODepth, IN.tex, float2( 0.0f,  0.0f)).x	> farClip;
	result.y = tex2DOffset(GBufferTextureSamplerSSAODepth, IN.tex, float2( 1.0f,  0.0f)).x	> farClip;
	result.z = tex2DOffset(GBufferTextureSamplerSSAODepth, IN.tex, float2( 1.0f, -1.0f)).x	> farClip;
	result.w = tex2DOffset(GBufferTextureSamplerSSAODepth, IN.tex, float2( 0.0f, -1.0f)).x	> farClip;

	half clouds = h1tex2D(BloomSampler, IN.tex).r;

	return half4(dot(result, (clouds/4.0).xxxx).xxx,1);      // optimized version of clouds*(result.x+result.y+result.z+result.w)/4;
}

// NOTE: this needs to be adjusted to render directly into the bloom buffer. not in to the 0..1 space the includes the viewport and the sun...
VS_SSLR_EXTRUDE_OUT VS_SSLRExtrudeRays(VS_SSLR_IN IN)
{
	VS_SSLR_EXTRUDE_OUT OUT = (VS_SSLR_EXTRUDE_OUT)0;
	
	OUT.texCoord = 2*(IN.texCoord.xy-float2(.5,.5));

	OUT.pos = MonoToStereoClipSpace(mul(float4(IN.pos.xyz, 1.0f), gWorldViewProj));

	// calculate screen coordinates of the card edge
	OUT.screenPosAndRayToCenter.x = OUT.pos.x * 0.5f + OUT.pos.w * 0.5f;
	OUT.screenPosAndRayToCenter.y = OUT.pos.w * 0.5f - OUT.pos.y * 0.5f;
	OUT.screenPosAndRayToCenter.xy = (OUT.screenPosAndRayToCenter.xy / OUT.pos.w);

	// calculate the ray from this vert to the center of the light card in screen space
	float4 centerPos = MonoToStereoClipSpace(mul(float4(sslrCenter, 1.0f), gWorldViewProj)); 
	float2 lightCenter;
	lightCenter.x = centerPos.x * 0.5f + centerPos.w * 0.5f;
	lightCenter.y = centerPos.w * 0.5f - centerPos.y * 0.5f;
	lightCenter.xy = (lightCenter.xy / centerPos.w);
	
	OUT.screenPosAndRayToCenter.zw = (lightCenter.xy - OUT.screenPosAndRayToCenter.xy) / SSLR_SEGMENTS;   
	OUT.screenPosAndRayToCenter.zw *= (rayLength * 0.1f);
	
	return OUT;
}

half SSLRExtrudeRaysCommon(PS_SSLR_EXTRUDE_IN IN)
{
	// Calculate the vector towards the light source
	float2 sampleLoc = IN.screenPosAndRayToCenter.xy;
	float2 vectorFromCenter = IN.screenPosAndRayToCenter.zw;   

     // compute a dithered sub offset (1/9 to 9/9)
	float offset = (1 + fmod(IN.pos.x,3) + fmod(IN.pos.y,3)*3)/9;
		
	#define weightAdjust ((1.5-.6)/(SSLR_SEGMENTS))  // the weight of each sample decrease from 1.5 to .6 by the last sample
 	
	// Sample in a line towards the light source
 	half luminance = 0;
 	
#if __XENON
	[unroll]
#endif // __XENON
	for (float i = 0; i<SSLR_SEGMENTS; i++)
		luminance += h1tex2D(SSLRSampler, sampleLoc + (vectorFromCenter * (i+offset))).x * (1.5-((i+1)*weightAdjust));	
	
	#define weightSum ((1.5*(SSLR_SEGMENTS+1)) - (weightAdjust*((SSLR_SEGMENTS)*(SSLR_SEGMENTS+1)/2.0))) // sum off all the weights
	
	half luminance0 = h1tex2D(SSLRSampler, sampleLoc).x;  // first sample is true, not dithered.
	
	half lightConeVisibility = max(luminance0 * shadowShaftTranslucency, (luminance+luminance0*1.5f)/weightSum);
	
 	// radial fade off of rays.
	half fade = (1-sqrt(dot(IN.texCoord.xy,IN.texCoord.xy))) * 0.5; // .5 is to match the general intensity with the old style light rays, so switch does not pop much
	return max((fade*lightConeVisibility),0);
}
 
half4	PS_SSLRExtrudeRays(PS_SSLR_EXTRUDE_IN IN) : COLOR
{
	half rayIntensity = SSLRExtrudeRaysCommon(IN);
	
#if SQUARE_IB
	rayIntensity =  pow(rayIntensity,1.0/4.0);	// Gamma 4 really helps with light ray banding
#endif

	#if IB_IN_BLOOM_ALPHA 
		return half4(1,1,1,rayIntensity);
	#else
		return half4(rayIntensity.xxx,1);
	#endif	
}

//
// End of Screen Space Light Rays
//
//#if __D3D11
#define PS_ShadowSampler SHADOWSAMPLER_TEXSAMP
//#else
//#define PS_ShadowSampler JitterSampler
//#endif

float rand_float(float2 uv)
{
	float2 noise = (frac(sin(dot(uv ,float2(12.9898,78.233)*2.0)) * 43758.5453));
	return abs(noise.x + noise.y) - 1.0;
}

float4 FogRaysCommon(postfxPixelInputPassThroughP IN, int numsamples, bool useGather)
{
	float depth;
#if __SHADERMODEL >= 40
	if (useGather)
	{
		float4 depths = fixupGBufferDepth(ResolvedDepthTexture.GatherRed(ResolvedDepthSampler, IN.tex.xy));
		depths.xy = max(depths.xy, depths.zw); 
		depths.w  = max(depths.x,  depths.y);
		depth = getLinearDepth( depths.w, dofProj.zw );
	}
	else	
#endif
	{
		float zed = GBufferTexDepth2D(ResolvedDepthSampler,IN.tex.xy).x;
		depth = getLinearDepth( zed, dofProj.zw );
	}	

	//depth = min(depth, 200*dot(gViewInverse[2].xyz, normalize(-IN.eyeRay.xyz)));
	half3 eyeRayToPoint = (IN.eyeRay.xyz*depth); 
	float eyeDist = length(eyeRayToPoint);
	float dist = -smoothstep(0,10, eyeDist);
	
	float rnd = rand_float(IN.tex.xy);	
	float rnd_v = lerp(0, 0.5 * rnd, saturate(eyeDist * globalFogRayJitterScale + globalFogRayJitterBias));

	int blurcell=3;
	float offset = fmod(IN.pos.x, blurcell) + fmod(IN.pos.y, blurcell)*blurcell;	
	float3 deltaPos = eyeRayToPoint / (numsamples+1);
	float3 tPos = gViewInverse[3].xyz+deltaPos*offset/(blurcell*blurcell)+rnd_v*deltaPos;

	half rayIntensity = 1.0f;
	rayIntensity = CalcCascadeShadowAccumAll(PS_ShadowSampler, true, tPos, deltaPos, numsamples);

	// clamp within a reasonable range so that e^eyeDistExp doesn't evaluate to a denormal/nan
	// e^-85 ~= 1.2e-37
	const float eyeDistExp = clamp(eyeDist*0.2f, 0.0f, 85.0f);

	// trying to give more contrast near distance
	//rayIntensity = pow(rayIntensity,1.0f + exp(-eyeDistExp)*globalFogRayPow);

	//rayIntensity = rayIntensity * (1.0f-saturate(exp(dist)));

	// Blend into 1 over fade range so that far distance fogging still takes place.
	float fade = saturate((eyeDist)*globalFogRayFadeDenominator);
	rayIntensity = (1.0f - fade)*rayIntensity + fade*1.0f;

	return float4(rayIntensity, depth, 0.0f, 0.0f);
}

float4 PS_FogRays(postfxPixelInputPassThroughP IN): COLOR
{
	return FogRaysCommon(IN, 8, true);
}

float4 PS_FogRaysNoGather(postfxPixelInputPassThroughP IN): COLOR
{
	return FogRaysCommon(IN, 8, false);
}

float4 PS_FogRaysHigh(postfxPixelInputPassThroughP IN): COLOR
{
	return FogRaysCommon(IN, 16, true);
}

float4 PS_FogRaysHighNoGather(postfxPixelInputPassThroughP IN): COLOR
{
	return FogRaysCommon(IN, 16, false);
}

OutFloatColor PS_BlitDepth(postfxVertexOutputPassThrough IN): COLOR
{
	float zed = fixupGBufferDepth(GBufferTexDepth2D(ResolvedDepthSampler,IN.tex.xy).x);
	
	return CastOutFloatColor(zed);
}
#undef PS_ShadowSampler

#if RSG_PC
float4 PS_CenterDist(postfxVertexOutputPassThrough IN) : COLOR
{
	float zed = GBufferTexDepth2D(ResolvedDepthSampler,float2(0.5f,0.5f)).x;
	float depth = getLinearDepth( zed, dofProj.zw );	

	half3 eyeRayToPoint = (globalFreeAimDir.xyz*depth); 
	float eyeDist = length(eyeRayToPoint);

	return eyeDist.xxxx;
}
#endif

#if __PS3
	#define PS_ShadowSampler HeatHazeSampler
#elif CASCADE_SHADOWS_USE_HW_PCF_DX10
	#define PS_ShadowSampler SHADOWSAMPLER_TEXSAMP
#else
	#define PS_ShadowSampler JitterSampler
#endif // platforms


half4 PS_LightRays1(postfxPixelInputPassThroughP IN): COLOR 
{
#if __PS3
	const int numsamples = 10; //PS3 needs less because of PCF filter
#else
	const int numsamples = 16;
#endif

	float depth = GetSSAODepth(GBufferTextureSamplerSSAODepth, IN.tex.xy).x;

	depth		= min(depth,gLightRayDistance);

	//dither filter
	const int blurcell=3;
	const float noisiness = 0.0; // TODO -- setting this to 0 can get rid of a tex2D with very little visual difference ..
	float offset = fmod(IN.pos.x, blurcell) + fmod(IN.pos.y, blurcell)*blurcell;
	//noise filter
	offset += (fmod(IN.pos.x+1, blurcell) + fmod(IN.pos.y+1, blurcell)*blurcell)/(blurcell*blurcell)*noisiness;
	//clamp to depth to water plane
	float height		= gViewInverse[3].z + depth*IN.eyeRay.z;
	depth				= max(depth - max(height - gWaterHeight, 0)/max(IN.eyeRay.z, 0.0001), 0);

	//compute world space startpos and offset
	float3 deltaPos=(IN.eyeRay.xyz*depth)/(numsamples+1);
	float3 tPos=gViewInverse[3].xyz+deltaPos*offset/(blurcell*blurcell+noisiness);

	//compute overall brightness
	half dScale=(dot(-gDirectionalLight.xyz, normalize(deltaPos.xyz))+1.0f)/2.0;
	dScale*=dScale;
	half lightRayDensity=(depth/gLightRayDistance) * dScale;

	//perform raymarch
	half rayIntensity = 0.0f;
	rayIntensity = CalcCascadeShadowAccum(PS_ShadowSampler, false, tPos, deltaPos, numsamples, POSTFX_CASCADE_INDEX);
	rayIntensity = pow(rayIntensity, 2);

	rayIntensity *= lightRayDensity;

	return CompositeLightRayAndBloom(rayIntensity, IN);
}

#undef PS_ShadowSampler

half4 PS_CalcBloom0(postfxVertexOutputPassThroughCompression  IN): COLOR 
{
	half3 outcol = CalculateBloom(BloomSampler, IN.tex.xy);
	return half4(outcol, 0);
}

half4 PS_CalcBloom_SeeThrough(postfxSampleInputPassThrough  IN): COLOR 
{
	half4 result;
	
	float depth = getDepth(IN.tex.xy);

	float stencil = ReadStencilMask(IN.tex.xy
									#if MULTISAMPLE_TECHNIQUES
										, SAMPLE_INDEX
									#endif
									);

	// Current AO technique has some view-dependent term that causes sky pixels to have a non-contant AO term. 
	// Also, far water writes to stencil with DEFERRED_MATERIAL_DEFAULT, so when sampling the SSAO texture
	// we still get the non-constant, view-dependent value.
	const float skipAODepthThreshold = 1000.0f;
	float skipAO = getSeeThroughStencilMask(stencil) || depth > skipAODepthThreshold;

	half4 seeThroughLayer = h4tex2D(BloomSampler, IN.tex.xy);
	half  seeThroughDepth = SeeThrough_UnPack(seeThroughLayer.xy)*gFadeEndDistance;
	half  visibility = saturate(seeThroughLayer.z);

	// Visible object 
	float depthDiff = (seeThroughDepth - depth)/gMaxThickness;
	
	half  covered = (depthDiff < 0.0f);
	half  thickness = 1.0f - saturate(depthDiff);
	half  seeThrough = 1.0f - (seeThroughLayer.w < 1.0f );

	result.r =  lerp(thickness*0.5f,1.0f,covered)* visibility * seeThrough;

	// increase contrast for non-cold objects that aren't covered
	// to avoid losing clothes detail without reducing visibility
	// for covered heat sources in the distance
	half  increaseContrast = (visibility > 0 && covered > 0);
	result.r = (increaseContrast ? result.r*result.r*result.r : result.r);
	

	// SSAO
	result.g = skipAO ? 1.0f : saturate((h1tex2D(GBufferTextureSamplerSSAODepth,IN.tex.xy).x + 0.05f));

	// Depth
	result.b = saturate(depth/gFadeEndDistance);
	
	result.a = 1.0f;

	return half4(result);
}

half4 PS_CalcBloom1(postfxVertexOutputPassThrough IN): COLOR 
{
	return (half4)h4tex2D(BloomSamplerG, IN.tex.xy); //gaussian
}

half4 DepthEffectsCommon(postfxSampleInputPassThroughP IN, bool useGather)
{
#if MULTISAMPLE_TECHNIQUES
	int3 iTexCoords = getIntCoords(IN.tex, globalScreenSize.xy);
	float z = gbufferTextureDepth.Load(iTexCoords, SAMPLE_INDEX).x;
#else	
	float z = tex2D(GBufferTextureSamplerDepth, IN.tex).x;
#endif
	float depth = getLinearGBufferDepth(z, dofProj.zw);

	float rayIntensity = 1.0f;
	float weightsSum = 0.0f;
#if !__LOW_QUALITY
	if (globalFogRayIntensity > 0.0f)
	{
#if __SHADERMODEL >= 40
		float4 lowResTexelSize = float4(gooScreenSize.xy * 2.0f, gScreenSize.xy * 0.5 );						
		float2 centerUV = (round( IN.tex * lowResTexelSize.zw )) * lowResTexelSize.xy;
	
		float4 depths = 0.0f;
		float4 rayIs = 0.0;
		
		if(useGather)
		{
			rayIs = PostFxTexture1.GatherRed(BlurSampler, centerUV);
			depths = PostFxTexture1.GatherGreen(BlurSampler, centerUV);
		}
		else
		{
			float4 coords[4];
			coords[0] = float4(centerUV + float2(-0.5f, -0.5f) * lowResTexelSize.xy, 0, 0);
			coords[1] = float4(centerUV + float2( 0.5f, -0.5f) * lowResTexelSize.xy, 0, 0);
			coords[2] = float4(centerUV + float2(-0.5f,  0.5f) * lowResTexelSize.xy, 0, 0);
			coords[3] = float4(centerUV + float2( 0.5f,  0.5f) * lowResTexelSize.xy, 0, 0);
			
			float2 samples[4];
			samples[0] = tex2Dlod(BlurSampler, coords[0]).xy;
			samples[1] = tex2Dlod(BlurSampler, coords[1]).xy;
			samples[2] = tex2Dlod(BlurSampler, coords[2]).xy;
			samples[3] = tex2Dlod(BlurSampler, coords[3]).xy;		

			depths = float4(samples[0].y, samples[1].y, samples[2].y, samples[3].y);			
			rayIs = float4(samples[0].x, samples[1].x, samples[2].x, samples[3].x);
		}
		
		// http://bglatzel.movingblocks.net/wp-content/uploads/2014/05/Volumetric-Lighting-for-Many-Lights-in-Lords-of-the-Fallen.pdf
		const float upsampleDepthThreshold = 0.01f;
		float4	depthDiff = abs((depths - depth)/depths);
	
		// ref	: 923.49us
		// new	: 986.38us	
		float2	result = float2(rayIs.x, depthDiff.x);

		result = depthDiff.y < result.y ? float2(rayIs.y, depthDiff.y) : result;
		result = depthDiff.z < result.y ? float2(rayIs.z, depthDiff.z) : result;
		result = depthDiff.w < result.y ? float2(rayIs.w, depthDiff.w) : result;
		
		rayIntensity = all(depthDiff < upsampleDepthThreshold) ? dot(rayIs, 0.25f) : result.x;
#else
		rayIntensity = tex2D(BlurSampler, IN.tex.xy).x;
#endif
	}
#endif
	
	half3 eyeRayToPoint = (IN.eyeRay.xyz*depth); 

	half4 color;
	color = calcDepthFxERay(eyeRayToPoint, saturate(lerp(1.0f,rayIntensity,globalFogRayIntensity)));

	XENON_ONLY(color.rgb *= color.a);			// premultiply by alpha to significantly reduce banding on 360 
	
	return PackHdr(color);
}

half4 PS_DepthEffects(postfxSampleInputPassThroughP IN): COLOR 
{
	return DepthEffectsCommon(IN, true);
}

half4 PS_DepthEffects_nogather(postfxSampleInputPassThroughP IN): COLOR 
{
	return DepthEffectsCommon(IN, false);
}

// Using GBufferTextureSamplerSSAODepth because it's one of the non-MSAA samplers available.
// We can't use GBufferTextureSampler0, since this shader is postfxMS, and the GBufferTexture0 is MSAA.

half4 PS_SubSampleAlpha(postfxFragmentInputPassThrough IN): COLOR 
{
#if !(__WIN32PC && __SHADERMODEL < 40 )
	half3 col= MixSubSampleAlpha( HDRSampler , GBufferTextureSamplerSSAODepth, IN.tex.xy, IN.pos, false, false ).rgb;
#else
	float3 col=tex2D( HDRSampler, IN.tex.xy).rgb;
#endif
	return half4(col.rgb, 0.0f);
}
half4 PS_SubSampleAlphaSinglePass(postfxFragmentInputPassThrough IN): COLOR 
{
#if !(__WIN32PC && __SHADERMODEL < 40 )
	half3 col= MixSubSampleAlpha( HDRSampler , GBufferTextureSamplerSSAODepth, IN.tex, IN.pos, true, false ).rgb;
#else
	float3 col=tex2D( HDRSampler, IN.tex.xy).rgb;
#endif
	return half4(col.rgb,0.0f);
}

half4 PS_SubSampleAlphaUI(postfxFragmentInputPassThrough IN): COLOR 
{
#if !(__WIN32PC && __SHADERMODEL < 40 )
	half3 col= MixSubSampleAlpha( HDRSampler , GBufferTextureSamplerSSAODepth, IN.tex.xy, IN.pos, false, true ).rgb;
#else
	float3 col=tex2D( HDRSampler, IN.tex.xy).rgb;
#endif
	return half4(col.rgb, dot(col.rgb,col.rgb) > 0.0f ? 1.0f : 0.0f );
}

half3 AA_Sample(float2 texXY)
{
	half3 color;

#if USE_SEPARATE_AA_PASS
#if USE_PACKED7e3
	color = PointSample7e3(HDRSampler, texXY, true).rgb;
#else // USE_PACKED7e3
	color = h3tex2D(HDRSampler, texXY).rgb;
#endif // USE_PACKED7e3
#else // USE_SEPARATE_AA_PASS

#if USE_FXAA
	color = FxaaPixelShader(texXY, BackBufferSampler, gooScreenSize.xy);
#else
	color = (half3)tex2D(HDRSampler, texXY).rgb;
#endif

#endif // USE_SEPARATE_AA_PASS

	return (half3)UnpackHdr(half4(color.rgb,0.0f)).rgb;
}

half3 AA_Sample_SS(float2 texXY, uint sampleIndex)
{
#if USE_AA_HDR_BUFFER
	half3 color;

	int3 iTexCoords = getIntCoords(texXY, globalScreenSize);
	color = HDRTextureAA.Load(iTexCoords.xy,sampleIndex).rgb;

	return (half3)UnpackHdr(half4(color.rgb,0.0f)).rgb;
#else	//USE_AA_HDR_BUFFER
	return AA_Sample(texXY);
#endif	//USE_AA_HDR_BUFFER
}


float GetHeatHazeIntensity(float depth)
{
	float width = (HH_FarRange - HH_StartRange) * 0.5f;
	float a = (depth - (HH_StartRange + width)) / width;

	return lerp(HH_minIntensity, HH_maxIntensity, 1.0 - saturate(abs(a)));
}

float GetHeatHazeIntensity(float2 tex, float depth)
{
	float depthInRange = (depth > HH_FarRange && depth < HH_FarRange);

	float intensity = tex2D(BackBufferSampler, tex).x*HH_AngleFade;
	
	return lerp(HH_minIntensity, HH_maxIntensity, intensity*depthInRange);
}


float3 GetHeatHazeTex2D(float2 tex)
{
	// future param
	float2 tex1 = (tex * HH_Tex1UVTiling) + HH_Tex1UVOffset;
	float2 tex2 = (tex * HH_Tex2UVTiling) + HH_Tex2UVOffset;
	
	return h3tex2D(HeatHazeSampler,tex1) + h3tex2D(HeatHazeSampler,tex2) - 1.0f.xxx;
}

half3 distanceBlurSample(half3 baseSample, sampler2D screen, float2 tex, half blurriness)
{
	// tbr: if anybody complains about the filtering being a bit rough, 
	// just revert back to a 5-tap filter; this version offers a similar
	// blur strength (sampling from quarter size buffer) but with a slightly 
	// rougher filtering, hopefully it's not noticeable (saves 0.33ms)
#if USE_DISTBLUR_BRANCH
	[branch] if ( blurriness )
#endif // USE_DISTBLUR_BRANCH
	{
		half3 blurSample = UnpackColor_3h(h3tex2D(screen, tex.xy).xyz); 
		return lerp(baseSample, blurSample, saturate(blurriness));
	}
#if USE_DISTBLUR_BRANCH
	return baseSample;
#endif // USE_DISTBLUR_BRANCH
}

#define TopMidGradMidPoint		GradientFilterColTop.w
#define MiddleColPos			GradientFilterColMiddle.w
#define MidBottomGradMidPoint	GradientFilterColBottom.w
#define	OO_OneMinusMidColPos	VignettingParams.w
#define OO_MiddleColPos			VignettingColor.w

#define TopColour				GradientFilterColTop.xyz
#define MiddleColour			GradientFilterColMiddle.xyz
#define BottomColour			GradientFilterColBottom.xyz

half3 GetGradientFilterColour(half2 tex)
{
	half3 col;
	const half t1 = saturate(saturate(tex.y*OO_MiddleColPos)+TopMidGradMidPoint);
	const half t2 = saturate(saturate(saturate(tex.y-MiddleColPos)*OO_OneMinusMidColPos)-MidBottomGradMidPoint);
	const half3 col1 = lerp(TopColour, MiddleColour, t1);
	const half3 col2 = lerp(MiddleColour, BottomColour, t2);
	//col = (tex.y < MiddleColPos) ? col1 : col2;
	col = lerp(col1, col2, tex.y);
	return col;
}

float2 WarpTexCoords(float2 texCoords, float distortionCoeff, float distortionCoeffCube)
{
	const float2 aspectCorrection = float2(TexelSize.x/TexelSize.y,1.0);
	const float2 dir = (texCoords-float2(0.5f,0.5f)) * aspectCorrection;
	const float distSqr = dot(dir, dir);
	const float distortionFactor = 1.0f + distSqr * (distortionCoeff + distortionCoeffCube*sqrt(distSqr));
	texCoords = distortionFactor.xx*(texCoords.xy-0.5.xx)+0.5f.xx;

	return texCoords;
}

half3 ColorAberration(float2 texCoords, half3 sampleC, uint sampleIndex, float scaleCoef)
{
	float2 tc = WarpTexCoords(texCoords, DistortionParams.z * scaleCoef, DistortionParams.w * scaleCoef);
#if USE_SEPARATE_LENS_DISTORTION_PASS
	sampleC.r = AA_Sample(tc);
#else
	sampleC.r = AA_Sample_SS(tc, sampleIndex);
#endif // USE_SEPARATE_LENS_DISTORTION_PASS
	return sampleC;
}

half3 LensDistortionFilter(inout float2 texCoords, uint sampleIndex)
{
#if RSG_PC
	const float scaleCoef = TexelSize.z;
#else
	const float scaleCoef = 1.0f;
#endif
	
	texCoords = WarpTexCoords(texCoords, DistortionParams.x * scaleCoef, DistortionParams.y * scaleCoef);
#if USE_SEPARATE_LENS_DISTORTION_PASS
	half3 sampleC = AA_Sample(texCoords);
#else
	half3 sampleC = AA_Sample_SS(texCoords, sampleIndex);
#endif // USE_SEPARATE_LENS_DISTORTION_PASS
	sampleC = ColorAberration(texCoords, sampleC, sampleIndex, scaleCoef);

	return sampleC;
}

half3 VignetteBlurFilter(float2 texCoords, half3 sampleC, sampler2D largeBlurSampler)
{
	float vignetteBlurIntensity = GetVignettingIntensity(texCoords, float3(BlurVignettingParams.x, BlurVignettingParams.y, 1.0f));
	float4 blurredSample = UnpackColor(tex2D(largeBlurSampler, texCoords));
	half3 outSample = lerp( blurredSample.rgb, sampleC.rgb,  vignetteBlurIntensity);
	return outSample;
}

half3 VignetteBlurFilterAlphaBlended(float2 texCoords, half3 sampleC, sampler2D largeBlurSampler, out float blendLevel)
{
	float vignetteBlurIntensity = GetVignettingIntensity(texCoords, float3(BlurVignettingParams.x, BlurVignettingParams.y, 1.0f));
	float4 blurredSample = tex2D(largeBlurSampler, texCoords);
	blendLevel = saturate(1.0f-vignetteBlurIntensity);
	return blurredSample;
}

half GetScanlineIntensity(half2 texcoords)
{
	half scanLine0 = (sin(ScanlineFilterParams.y*(texcoords.x + ScanlineFilterParams.w))*.5+.5)*ScanlineFilterIntensity;
	half scanLine1 = (sin(ScanlineFilterParams.z*(texcoords.y + ScanlineFilterParams.w*0.5f))*.5+.5)*ScanlineFilterIntensity;
	half scanLine = (scanLine0+scanLine1);
	return scanLine;
}

float GetLensArtefactsOpacityMultiplier(float curExposure)
{
	const float t = saturate((curExposure-LensArtefactsExposureMin)*LensArtefactsExposureOORange);
	const float exposureFadeMult = lerp(LensArtefactsRemappedMinMult, LensArtefactsRemappedMaxMult, t);
	const float finalFadeMult = lerp(1.0f, exposureFadeMult, LensArtefactsExposureDrivenFadeMult);
	return finalFadeMult*LensArtefactsGlobalMultiplier;
}

float fullFilmicTonemap(float inputValue, half4 srcFilmic0, half4 srcFilmic1)
{
	float A = srcFilmic0.x;
	float B = srcFilmic0.y;
	float C = srcFilmic0.z;
	float D = srcFilmic0.w;
	float E = srcFilmic1.x;
	float F = srcFilmic1.y;

	return (((inputValue*(A*inputValue+C*B)+D*E)/(inputValue*(A*inputValue+B)+D*F))-E/F);
}


void getFilmicParams(out float4 filmic0, out float4 filmic1, float exposure)
{	
	float t = saturate(exposure * TonemapParams.x + TonemapParams.y);	

	float4 srcFilmic0 = lerp(BrightTonemapParams0, DarkTonemapParams0, t);
	float4 srcFilmic1 = lerp(BrightTonemapParams1, DarkTonemapParams1, t);

	float4 filmicParams0 = srcFilmic0;
	filmicParams0.z = 1.0f / fullFilmicTonemap(srcFilmic1.z, srcFilmic0, srcFilmic1);
	filmicParams0.w = 1.0f / srcFilmic1.z;

	float4 filmicParams1;
	filmicParams1.x = srcFilmic0.z*srcFilmic0.y; // C_mul_B
	filmicParams1.y = srcFilmic0.w*srcFilmic1.x; // D_mul_E
	filmicParams1.z = srcFilmic0.w*srcFilmic1.y; // D_mul_F
	filmicParams1.w = srcFilmic1.x/srcFilmic1.y; // E_div_F

	filmic0 		= filmicParams0;
	filmic1 		= filmicParams1;
}

#define MB_VISUALISE_MOTION_VECTOR (0)
#if MB_VISUALISE_MOTION_VECTOR
//
// direction-color mapping (pink when 
// motion vector length is 0):
//
//      R
//      |
//	B - . - G
//		|
//	   RGB
//
float4 VisualiseMotionVector(float2 tex, float2 depth, float moVecLen)
{
	float4 result = float4(1,0,1,1);

	float playerMask = CalculatePlayerMask(ReadStencilMask( tex.xy 
														#if MULTISAMPLE_TECHNIQUES
															, SAMPLE_INDEX
														#endif
														));
	float2 moVec = motionBlurGetVelocity(tex.xy, depth, moVecLen)*2.0f;

	const float3 red	= float3(1,0,0);
	const float3 green	= float3(0,1,0);
	const float3 blue	= float3(0,0,1);
	const float3 black	= float3(1,1,1);
	const float2 upVec	= float2(0,-1);

	float3 t;

	if ( abs(moVec.x+moVec.y) > 0.00001f)
	{
		t = dot(normalize(moVec), upVec);

		if (moVec.x >= 0.0f) {
			
			if (moVec.y < 0) {
				result.rgb = lerp(green, red, t);
			} 
			else {
				result.rgb = lerp(green, black, abs(t));
			}
		}
		else {
			if (moVec.y >= 0) {
				result.rgb = lerp(blue, black, abs(t));
			}
			else {
				result.rgb = lerp(blue, red, t);
			}
		}
	}


	result.rgb *= playerMask;

	return result;
}

#endif

#if __XENON
[maxtempreg(14)]
#endif // __XENON
half4 Composite(postfxFragmentInputPassThroughComposite IN, bool highDof, bool shallowDof, bool noise, bool motionBlur, bool nightVision, bool heatHaze, bool extraEffects, uniform bool lightRayComposite, bool useDistBlur) 
{
	half notSkyMask;

	const float2 depth2 = getLinearAndRawDepth(IN.tex.xy,
#if USE_SUPERSAMPLE_COMPOSITE && SAMPLE_FREQUENCY
		IN.sampleIndex,
#else
		0,
#endif	//USE_SUPERSAMPLE_COMPOSITE
		notSkyMask);
	const float depth = depth2.x;

	float dofblur = 0.0f;
	
	if (useDistBlur)
	{
		dofblur = distanceBlur(depth)*notSkyMask;
	}

	float2 texCoordIN = IN.tex.xy;

	float2 heatHazeTexCoords = texCoordIN;	
	half intensity = 0;
	if( heatHaze )
	{		
		intensity = GetHeatHazeIntensity(heatHazeTexCoords,depth);
		float3 heatHazeOffset = GetHeatHazeTex2D(heatHazeTexCoords);
		heatHazeTexCoords += heatHazeOffset.xy * HH_OffsetScale * intensity;
	}

	//When using compute of diffusion dof the dof is calculated before this pass and is in the back buffer passed in.
	//if heat haze is on we need to use the offset coords to sample the texture here.
#if DOF_COMPUTE_GAUSSIAN || DOF_DIFFUSION
	texCoordIN = heatHazeTexCoords;
#endif //DOF_COMPUTE_GAUSSIAN || DOF_DIFFUSION

		// grab sample and compute warped texture coordinates
	half3 sampleC;
#if !USE_SEPARATE_LENS_DISTORTION_PASS
	if (extraEffects)
	{
		sampleC = LensDistortionFilter( texCoordIN, SAMPLE_INDEX );
	}
	else
#endif // !USE_SEPARATE_LENS_DISTORTION_PASS
	{
		sampleC = AA_Sample_SS( texCoordIN, SAMPLE_INDEX );
	}

	if( heatHaze )
	{
		if (highDof)
		{
#if DOF_COMPUTE_GAUSSIAN || DOF_DIFFUSION
#if DOF_TYPE_CHANGEABLE_IN_RAG
			if( currentDOFTechnique > 0 )
#else
			if( 1 )
#endif
			{
				//Dont need to do anything here as when using compute or diffusion depth of field the backbuffer passed into composite already has the dof applied.
				
			}
			else
#endif
			{
				if (shallowDof)
				{
					sampleC.rgb = ShallowHiDOFSample(sampleC.rgb, MotionBlurSampler, PostFxSampler2, PostFxSampler3, texCoordIN, depth);
				}
				else
				{
					sampleC.rgb = HiDOFSample(sampleC.rgb, MotionBlurSampler, PostFxSampler2, texCoordIN, depth);
				}
			}
		}
		else
		{
			sampleC.rgb = distanceBlurSample(sampleC.rgb, MotionBlurSampler, texCoordIN, max(dofblur,intensity));
		}
	}
	else
	{
		if ( highDof )
		{
#if DOF_COMPUTE_GAUSSIAN || DOF_DIFFUSION
#if DOF_TYPE_CHANGEABLE_IN_RAG
			if( currentDOFTechnique > 0 )
#else
			if( 1 )
#endif
			{
				//Dont need to do anything here as when using compute or diffusion depth of field the backbuffer passed into composite already has the dof applied.
			}
			else
#endif
			{
				if (shallowDof)
				{
					sampleC.rgb = ShallowHiDOFSample(sampleC.rgb, MotionBlurSampler, PostFxSampler2, PostFxSampler3, texCoordIN, depth);
				}
				else
				{
					sampleC.rgb = HiDOFSample(sampleC.rgb, MotionBlurSampler, PostFxSampler2, texCoordIN, depth);					
				}
			}
		}
		else
		{
			sampleC.rgb = distanceBlurSample(sampleC.rgb, MotionBlurSampler, texCoordIN, dofblur);
		}
	}

#if BOKEH_SUPPORT
	float4 bokehSample = float4(0.0, 0.0, 0.0, 0.0);
	if( highDof && BokehEnableVar )
	{
		bokehSample = tex2D( BloomSamplerG, texCoordIN );
		sampleC.rgb = lerp(sampleC.rgb, bokehSample.rgb, bokehSample.a);
	}
#endif //BOKEH_SUPPORT

	if( motionBlur )
	{
		sampleC = ApplyMotionBlur(sampleC, texCoordIN, depth2, MotionBlurSampler, DirectionalMotionBlurParams.x
									#if MULTISAMPLE_TECHNIQUES
										, SAMPLE_INDEX
									#endif
										);
	}

	if (extraEffects)
	{
		float3 extraEffectsSample = UnpackColor(h4tex2D(PostFxSampler3, IN.tex.xy));
		#if BOKEH_SUPPORT
		if( highDof && BokehEnableVar )
		{
			extraEffectsSample = lerp(extraEffectsSample, bokehSample.rgb, bokehSample.a);					
		}
		#endif //BOKEH_SUPPORT

		//sampleC.rgb = VignetteBlurFilter(IN.tex.xy, sampleC.rgb, PostFxSampler3);
		sampleC.rgb = lerp(sampleC.rgb, extraEffectsSample, ScreenBlurFade);
	}
#if __LOW_QUALITY
	half4 bloomBlurSample = 0.0f;
#else
	half4 bloomBlurSample = SampleBloom4(texCoordIN, IN.tex.zw);
#endif

	if (lightRayComposite && enableIntensityBuffer>0.0f)
	{
#if IB_IN_BLOOM_ALPHA
		half rayPixel = bloomBlurSample.a;   // intensity in in the alpha channel of the bloom on ps3
#else
		half rayPixel = h1tex2D(SSLRSampler,texCoordIN).r;
#endif
		
#if SQUARE_IB
		rayPixel = pow(rayPixel,8.0); // This could switch back to gamma4 if we return the light rays.
#endif

		sampleC =  sampleC + (gLightRayColor) * rayPixel.rrr;
	}

	float exposure = GetExposure(IN.tex);

	if( !nightVision )
	{
		// temporary workaround until we support reusing a sampler for different textures, at the moment
		// they're maxed out in the composite pass
		#if !__LOW_QUALITY
		if (!heatHaze)
		{
			float3 lensArtefactsCol = tex2D(HeatHazeSampler, IN.tex.xy).rgb;
			float3 lensArtefacts = lensArtefactsCol*GetLensArtefactsOpacityMultiplier(exposure);
			sampleC.rgb += lensArtefacts;
		}

		sampleC.rgb +=(bloomBlurSample * BloomIntensity * 0.25f).rgb;
		#endif
		float3 vignetteTint = lerp(VignettingColor.rgb, float3(1,1,1), GetVignettingIntensity(IN.tex.xy, VignettingParams.xyz));
		sampleC.rgb *= vignetteTint;
	}
	sampleC.rgb = min(sampleC.rgb, 65504.0);
	
	float4 filmicParams0, filmicParams1;
	getFilmicParams(filmicParams0, filmicParams1, IN.tex1);
	sampleC = filmicToneMap(sampleC, filmicParams0, filmicParams1, exposure);
	sampleC = ApplyColorCorrection(sampleC);

	half4 result;
	
	if( nightVision )
	{
		half dotLum = dot(sampleC.rgb,LumFactors.rgb) * scalerLum;

		half alphaLowLum = 1.0f-saturate(dotLum/lowLum);
		half alphaHighLum = saturate((dotLum-highLum)/(topLum - highLum));
		half alphaHL = 1.0f - (alphaLowLum + alphaHighLum);
		
		half lumOffset = alphaLowLum * offsetLowLum + alphaHighLum * offsetHighLum + alphaHL * offsetLum;
		dotLum += lumOffset;

		half numNoise = h3tex2D(BlurSampler, frac(IN.tex.xy*float2(1.6,0.9)*1.5+NoiseParams.xy)).g;
	
		half noisiness = alphaLowLum * noiseLowLum  + alphaHL * noiseLum;
		dotLum += (numNoise-0.5f)*(noisiness);

		half highLumNoisiness = max((numNoise-0.5f)*(alphaHighLum * noiseHighLum),0.0f);
		dotLum += highLumNoisiness;

		half bloomThres=alphaHighLum;
		
		bloomBlurSample.rgb = filmicToneMap(bloomBlurSample.rgb, filmicParams0, filmicParams1, GetExposure(IN.tex));

		half bloomDotLum = dot(bloomBlurSample.rgb,LumFactors.rgb) * scalerLum;
		bloomDotLum += lumOffset;

		half intensityBloom = bloomLum;
		half bloom =(max((bloomDotLum-bloomThres), 0.0f)*intensityBloom);

		dotLum += bloom;

		float3 finalColor = alphaLowLum * colorLowLum.xyz + alphaHighLum * colorHighLum.xyz + alphaHL * colorLum.xyz;
		
		result.rgb = finalColor * dotLum;
	}	
	else
	{
		result.rgb = ragePow(sampleC, Gamma);
	}	


	if (extraEffects)
	{
		half scanLine = 1.0f - GetScanlineIntensity(IN.tex.yy);
		result.rgb *= scanLine;
	}

	if( noise )
	{
		half num = h4tex2D(BlurSampler, frac(IN.tex.xy*float2(1.6,0.9)*NoiseParams.w+NoiseParams.xy)).a;
		result.rgb = max(0,result.rgb+(num-0.5f)*NoiseParams.z);
	}

	result = saturate(result);
	result.a = dot(result.rgb, float3(0.299, 0.587, 0.114));

#if MB_VISUALISE_MOTION_VECTOR
	const float moVecLength =  1.0f;
	float2 weight;
	weight.x = saturate(1.0f-debugParams0.x);
	weight.y = (1.0f-weight.x);
	result.rgb = result.rgb*weight.x + VisualiseMotionVector(IN.tex.xy, depth2, moVecLength).rgb*weight.y;
#endif

	return half4(result);
}

#if FILM_EFFECT
half4 PS_FilmEffect(postfxSampleInputPassThrough IN): COLOR 
{
	float4 result;
	result.rgb = tex2D(HDRSampler, IN.tex.xy);
	result.a = 1.0f;

	result.rgb = ApplyLensDistortion(IN.tex.xy);
	result.rgb *= lerp(VignettingColor, float3(1,1,1), GetVignettingIntensity(IN.tex.xy, VignettingParams.xyz));

	if(NoiseParams.z > 0.0f) 
	{
		float num=tex2D(BlurSampler, frac(IN.tex.xy*float2(1.6,0.9)*8.0f+NoiseParams.xy)).a;
		result.r=max(0,result.r+(num-0.5f)*NoiseParams.z * NoiseParams.x);
		result.g=max(0,result.g+(num-0.5f)*NoiseParams.z);
		result.b=max(0,result.b+(num-0.5f)*NoiseParams.z * NoiseParams.y);	
	}

	return half4(result);
}
#endif

// Can I haz support for uniform bools ? kthxbye.
half4 PS_Composite(postfxFragmentInputPassThroughComposite IN): COLOR 
{
	return Composite(IN,false,false,false,false,false,false,false,true,true);
}


half4  PS_CompositeHighDOF(postfxFragmentInputPassThroughComposite IN): COLOR 
{
	return Composite(IN,true,false,false,false,false,false,false,true,true);
}

half4  PS_CompositeNoise(postfxFragmentInputPassThroughComposite IN): COLOR 
{
	return Composite(IN,false,false,true,false,false,false,false,true,true);
}

half4  PS_CompositeHighDOFNoise(postfxFragmentInputPassThroughComposite IN): COLOR 
{
	return Composite(IN,true,false,true,false,false,false,false,true,true);
}

half4  PS_CompositeMB(postfxFragmentInputPassThroughComposite IN): COLOR 
{
	return Composite(IN,false,false,false,true,false,false,false,true,true);
}


half4  PS_CompositeMBHighDOF(postfxFragmentInputPassThroughComposite IN): COLOR 
{
	return Composite(IN,true,false,false,true,false,false,false,true,true);
}

half4  PS_CompositeMBNoise(postfxFragmentInputPassThroughComposite IN): COLOR 
{
	return Composite(IN,false,false,true,true,false,false,false,true,true);
}

half4  PS_CompositeMBHighDOFNoise(postfxFragmentInputPassThroughComposite IN): COLOR 
{
	return Composite(IN,true,false,true,true,false,false,false,true,true);
}

half4  PS_CompositeNV(postfxFragmentInputPassThroughComposite IN): COLOR  
{
	return Composite(IN,false, false, false, false, true, false,false,true,true);
}

half4  PS_CompositeMBNV(postfxFragmentInputPassThroughComposite IN): COLOR  
{
	return Composite(IN,false, false, false, true, true, false,false,true,true);
}

half4  PS_CompositeHighDOFNV(postfxFragmentInputPassThroughComposite IN): COLOR  
{
	return Composite(IN,true, false, false, false, true, false,false,true,true);
}

half4  PS_CompositeMBHighDOFNV(postfxFragmentInputPassThroughComposite IN): COLOR  
{
	return Composite(IN,true, false, false, true, true, false,false,true,true);
}

half4  PS_CompositeHH(postfxFragmentInputPassThroughComposite IN): COLOR 
{
	return Composite(IN,false,false,false,false,false,true,false,true,true);
}


half4  PS_CompositeHighDOFHH(postfxFragmentInputPassThroughComposite IN): COLOR 
{
	return Composite(IN,true,false,false,false,false,true,false,true,true);
}


half4  PS_CompositeNoiseHH(postfxFragmentInputPassThroughComposite IN): COLOR 
{
	return Composite(IN,false,false,true,false,false,true,false,true,true);
}

half4  PS_CompositeHighDOFNoiseHH(postfxFragmentInputPassThroughComposite IN): COLOR 
{
	return Composite(IN,true,false,true,false,false,true,false,true,true);
}

half4  PS_CompositeMBHH(postfxFragmentInputPassThroughComposite IN): COLOR 
{
	return Composite(IN,false,false,false,true,false,true,false,true,true);
}

half4  PS_CompositeMBHighDOFHH(postfxFragmentInputPassThroughComposite IN): COLOR 
{
	return Composite(IN,true,false,false,true,false,true,false,true,true);
}

half4  PS_CompositeMBNoiseHH(postfxFragmentInputPassThroughComposite IN): COLOR 
{
	return Composite(IN,false,false,true,true,false,true,false,true,true);
}

half4  PS_CompositeMBHighDOFNoiseHH(postfxFragmentInputPassThroughComposite IN): COLOR 
{
	return Composite(IN,true,false,true,true,false,true,false,true,true);
}

half4  PS_CompositeNVHH(postfxFragmentInputPassThroughComposite IN): COLOR  
{
	return Composite(IN,false, false, false, false, true, true,false,true,true);
}

half4  PS_CompositeMBNVHH(postfxFragmentInputPassThroughComposite IN): COLOR  
{
	return Composite(IN,false, false, false, true, true, true,false,true,true);
}

half4  PS_CompositeHighDOFNVHH(postfxFragmentInputPassThroughComposite IN): COLOR  
{
	return Composite(IN,true, false, false, false, true, true,false,true,true);
}

half4  PS_CompositeMBHighDOFNVHH(postfxFragmentInputPassThroughComposite IN): COLOR  
{
	return Composite(IN,true, false, false, true, true, true,false,true,true);
}


half4  PS_CompositeShallowHighDOF(postfxFragmentInputPassThroughComposite IN): COLOR 
{
	return Composite(IN,true,true,false,false,false,false,false,true,true);
}

half4  PS_CompositeShallowHighDOFNoise(postfxFragmentInputPassThroughComposite IN): COLOR 
{
	return Composite(IN,true,true,true,false,false,false,false,true,true);
}

half4  PS_CompositeMBShallowHighDOF(postfxFragmentInputPassThroughComposite IN): COLOR 
{
	return Composite(IN,true,true,false,true,false,false,false,true,true);
}

half4  PS_CompositeMBShallowHighDOFNoise(postfxFragmentInputPassThroughComposite IN): COLOR 
{
	return Composite(IN,true,true,true,true,false,false,false,true,true);
}

half4  PS_CompositeShallowHighDOFNV(postfxFragmentInputPassThroughComposite IN): COLOR  
{
	return Composite(IN,true, true, false, false, true, false,false,true,true);
}

half4  PS_CompositeMBShallowHighDOFNV(postfxFragmentInputPassThroughComposite IN): COLOR  
{
	return Composite(IN,true, true, false, true, true, false,false,true,true);
}

half4  PS_CompositeShallowHighDOFHH(postfxFragmentInputPassThroughComposite IN): COLOR 
{
	return Composite(IN,true,true,false,false,false,true,false,true,true);
}

half4  PS_CompositeShallowHighDOFNoiseHH(postfxFragmentInputPassThroughComposite IN): COLOR 
{
	return Composite(IN,true,true,true,false,false,true,false,true,true);
}

half4  PS_CompositeMBShallowHighDOFHH(postfxFragmentInputPassThroughComposite IN): COLOR 
{
	return Composite(IN,true,true,false,true,false,true,false,true,true);
}

half4  PS_CompositeMBShallowHighDOFNoiseHH(postfxFragmentInputPassThroughComposite IN): COLOR 
{
	return Composite(IN,true,true,true,true,false,true,false,true,true);
}

half4  PS_CompositeShallowHighDOFNVHH(postfxFragmentInputPassThroughComposite IN): COLOR  
{
	return Composite(IN,true, true, false, false, true, true,false,true,true);
}

half4  PS_CompositeMBShallowHighDOFNVHH(postfxFragmentInputPassThroughComposite IN): COLOR  
{
	return Composite(IN,true, true, false, true, true, true,false,true,true);
}

half4 PS_CompositeExtraFX(postfxFragmentInputPassThroughComposite IN): COLOR 
{
	return Composite(IN,false,false,true,false,false,false,true,true,true);
}

half4  PS_CompositeMBExtraFX(postfxFragmentInputPassThroughComposite IN): COLOR 
{
	return Composite(IN,false,false,true,true,false,false,true,true,true);
}

half4  PS_CompositeHighDOFExtraFX(postfxFragmentInputPassThroughComposite IN): COLOR 
{
	return Composite(IN,true,false,true,false,false,false,true,true,true);
}

half4  PS_CompositeMBHighDOFExtraFX(postfxFragmentInputPassThroughComposite IN): COLOR 
{
	return Composite(IN,true,false,true,true,false,false,true,true,true);
}

half4  PS_CompositeHHExtraFX(postfxFragmentInputPassThroughComposite IN): COLOR 
{
	return Composite(IN,false,false,true,false,false,true,true,true,true);
}

half4  PS_CompositeMBHHExtraFX(postfxFragmentInputPassThroughComposite IN): COLOR 
{
	return Composite(IN,false,false,true,true,false,true,true,true,true);
}

half4  PS_CompositeHighDOFHHExtraFX(postfxFragmentInputPassThroughComposite IN): COLOR 
{
	return Composite(IN,true,false,true,false,false,true,true,true,true);
}

half4  PS_CompositeMBHighDOFHHExtraFX(postfxFragmentInputPassThroughComposite IN): COLOR 
{
	return Composite(IN,true,false,true,true,false,true,true,true,true);
}

half4  PS_CompositeShallowHighDOFExtraFX(postfxFragmentInputPassThroughComposite IN): COLOR 
{
	return Composite(IN,true,true,true,false,false,false,true,true,true);
}

half4  PS_CompositeMBShallowHighDOFExtraFX(postfxFragmentInputPassThroughComposite IN): COLOR 
{
	return Composite(IN,true,true,true,true,false,false,true,true,true);
}

half4  PS_CompositeShallowHighDOFHHExtraFX(postfxFragmentInputPassThroughComposite IN): COLOR 
{
	return Composite(IN,true,true,true,false,false,true,true,true,true);
}

half4  PS_CompositeMBShallowHighDOFHHExtraFX(postfxFragmentInputPassThroughComposite IN): COLOR 
{
	return Composite(IN,true,true,true,true,false,true,true,true,true);
}

half4 PS_Composite_NoBlur(postfxFragmentInputPassThroughComposite IN): COLOR 
{
	return Composite(IN,false,false,false,false,false,false,false,true,false);
}

half4 Common5TapBlur(sampler2D texSampler, float2 tex)
{
	//  5 tap 3x3 filter suggested by Piotr: 
	//				A B B
	//				A E D
	//				C C D	
	half offset = XENON_SWITCH(0.5f, 0.0f);
	half4 A = tex2DOffset( texSampler, tex.xy, float2(-1,-.5)	+ offset).rgba;
	half4 B = tex2DOffset( texSampler, tex.xy, float2(.5,-1)	+ offset).rgba;
	half4 C = tex2DOffset( texSampler, tex.xy, float2(-.5,1)	+ offset).rgba;
	half4 D = tex2DOffset( texSampler, tex.xy, float2(1,.5)		+ offset).rgba;
	half4 E = tex2DOffset( texSampler, tex.xy, float2(0,0)		+ offset).rgba; 

	return half4((A+B+C+D+E/2)/4.5);
}

half4 FilterSeeThroughData(sampler2D texSampler, float2 tex)
{
	return Common5TapBlur(texSampler, tex);
}

half4  PS_CompositeSeeThrough(postfxFragmentInputPassThroughComposite IN): COLOR  
{
	IN.tex.xy = WarpTexCoords(IN.tex.xy, DistortionParams.x, DistortionParams.y);
	half baseNoise = h3tex2D(BlurSampler, frac(IN.tex.xy*float2(1.6,0.9)*7.0f+NoiseParams.xy)).g - 0.5f;
	half4 seeThroughData = FilterSeeThroughData(BloomSampler, IN.tex.xy).rgba;


	// SSAO is filtered separately to avoid losing all the detail
	half4 seeThroughDataRaw = FilterSeeThroughData(PostFxSampler2, IN.tex.xy);
	half  SSAOFiltered = seeThroughDataRaw.g;
	half  visibility = saturate(seeThroughDataRaw.r);

	half visible = seeThroughData.r;
	half SSAOblured = SSAOFiltered;
	half depthiness = seeThroughData.b;

	half noiseAmount = seeThroughMinNoiseAmount + (seeThroughMaxNoiseAmount - seeThroughMinNoiseAmount) * (depthiness);
	half noise = noiseAmount * baseNoise;

	half3 baseColor = lerp(seeThroughColorNear.rgb,seeThroughColorFar.rgb,depthiness + noise);
	half3 hiLight = lerp(1.0f - SSAOblured,0.0f,baseNoise*seeThroughHiLightNoise).xxx * seeThroughHiLightIntensity;
	half3 visibleColor = lerp(seeThroughColorVisibleBase.rgb,seeThroughColorVisibleWarm.rgb,visible);

	const half visibilityMult = 1.0f;
	float3 finalColor = saturate(baseColor - hiLight + visibleColor.rgb * visible * visibilityMult);

	return half4(finalColor,1);
}

half4  PS_Simple(postfxFragmentInputPassThroughComposite IN) : COLOR  
{
	half3 sampleC = AA_Sample(IN.tex.xy);
	float avgLum = tex2D(AdapLumSampler, 0.0f.xx).r;   

#if HACK_RDR3 	// no tone mapping 
	float3 toneMappedColour = sampleC;
#else
	float4 filmicParams0, filmicParams1;
	getFilmicParams(filmicParams0, filmicParams1, IN.tex1);
	float3 toneMappedColour = filmicToneMap(sampleC, filmicParams0, filmicParams1, GetExposure(IN.tex));
	toneMappedColour = ApplyColorCorrection(toneMappedColour);
	
#endif	
	float4 result = float4(pow(toneMappedColour, 0.4545), 1.0);
	result = saturate(result);
	result.a = dot(result.rgb, float3(0.299, 0.587, 0.114));

	return half4(result);
}

half4  PS_JustExposure(postfxFragmentInputPassThroughS IN) : COLOR  
{
	half3 sampleC = AA_Sample(IN.tex.xy);
	half3 exposedColour = max(0.0f, sampleC * GetExposure(IN.tex));
	half4 result = half4(pow(exposedColour, 0.4545), 1.0);
	result = saturate(result);
	result.a = dot(result.rgb, half3(0.299, 0.587, 0.114));
	return half4(result);
}

half4  PS_CopyDepth(postfxSampleInputPassThrough IN, out float depth : DEPTH) : COLOR  
{
#if __PS3
	depth = texDepth2D(GBufferTextureSamplerDepth, IN.tex);
#else
	depth = getZed(IN.tex, SAMPLE_INDEX);
#endif
	return half4(1,0,0,1); // Whatever you want here
}

half4  PS_PassThrough(postfxFragmentInputPassThrough IN) : COLOR  
{
#if USE_PACKED7e3
	half4 sampleC = BilinearSample7e3(HDRSampler, IN.tex.xy, true);
#else // USE_PACKED7e3
	half4 sampleC = h4tex2D(HDRSampler, IN.tex.xy);
#endif // USE_PACKED7e3

#if POSTFX_UNIT_QUAD && WATERMARK_ALPHA_SUPPORT
	half4 result = half4(PackColor(UnpackHdr(sampleC)));
	result.a *= QuadAlpha;
	return result;
#else
	return half4(PackColor(UnpackHdr(sampleC)));
#endif
}

half4  PS_Copy(postfxFragmentInputPassThrough IN) : COLOR  
{
#if USE_PACKED7e3
	half4 sampleC = BilinearSample7e3(HDRSampler, IN.tex.xy, true);
#else // USE_PACKED7e3
	half4 sampleC = h4tex2D(HDRSampler, IN.tex.xy);
#endif // USE_PACKED7e3
	return half4(sampleC);
}

half4 PS_GradientFilter(postfxVertexOutputGradientFilter IN) : COLOR  
{
	float3 gradientCol = GetGradientFilterColour(IN.tex.xy);
	return half4(gradientCol, 1);
}


half4 PS_DamageOverlay(postfxDamageOverlayOutput IN) : COLOR  
{
	half4 col = IN.col;
	float fade = (h4tex2D(BlurSampler, IN.tex.xy/IN.tex.z));
	//fade*=fade;
	return half4(col*fade);
}

half4 PS_Scanline(postfxVertexOutputGradientFilter IN) : COLOR  
{
	half scanLine = GetScanlineIntensity(IN.tex.yy);
	return half4(0, 0, 0, scanLine);
}

half4  PS_DownSampleBloom(postfxVertexOutputPassThrough IN): COLOR     
{
	return tex2D(HDRSampler, IN.tex);
}

half4  PS_DownSampleStencil(postfxSampleInputPassThrough IN): COLOR     
{ 
#if USE_PACKED7e3
	half3 sampleHDR = BilinearSample7e3(HDRSampler, IN.tex,true);
#else
	half3 sampleHDR = h3tex2D(HDRSampler, IN.tex);
#endif

#if MULTISAMPLE_TECHNIQUES
	return half4(PackColor_3h(UnpackHdr_3h(sampleHDR)), ( ReadStencilMask(IN.tex, SAMPLE_INDEX) > PLAYER_MASK ) ? 0.0f : 1.0f);
#else
	return half4(PackColor_3h(UnpackHdr_3h(sampleHDR)), ( ReadStencilMask(IN.tex) > PLAYER_MASK ) ? 0.0f : 1.0f);
#endif
}

half ComputeNearCoC(postfxVertexOutputDof IN)
{
	
	const half4 ooDist = (half4)(hiDofWorldNearRangeRcp);
	half4 coc;
	half4 curCoc;
	half4 depth;

	// 1st
	depth.x = (half)getDepth(IN.texD0);
	depth.y = (half)getDepth(IN.texD1);
	depth.z = (half)getDepth(IN.texD2);
	depth.w = (half)getDepth(IN.texD3);
	curCoc = (half4)saturate(1.0f.xxxx - ((depth.xyzw - hiDofWorldNearStart4)*ooDist.xxxx));
	coc = curCoc;

	// 2nd
	const half2 dofRowDelta1 = half2(0, 0.25 * gooScreenSize.y);
	const half2 dofRowDelta2 = half2(0, 0.25 * gooScreenSize.y)*2.0;
	const half2 dofRowDelta3 = half2(0, 0.25 * gooScreenSize.y)*3.0;

	depth.x = (half)getDepth(IN.texD0 + dofRowDelta1);
	depth.y = (half)getDepth(IN.texD1 + dofRowDelta1);
	depth.z = (half)getDepth(IN.texD2 + dofRowDelta1);
	depth.w = (half)getDepth(IN.texD3 + dofRowDelta1);
	curCoc = (half4)saturate(1.0f.xxxx - ((depth.xyzw - hiDofWorldNearStart4)*ooDist.xxxx));
	coc = max(coc, curCoc);

	// 3rd
	depth.x = (half)getDepth(IN.texD0 + dofRowDelta2);
	depth.y = (half)getDepth(IN.texD1 + dofRowDelta2);
	depth.z = (half)getDepth(IN.texD2 + dofRowDelta2);
	depth.w = (half)getDepth(IN.texD3 + dofRowDelta2);
	curCoc = (half4)saturate(1.0f.xxxx - ((depth.xyzw - hiDofWorldNearStart4)*ooDist.xxxx));
	coc = max(coc, curCoc);

	// 4rd
	depth.x = (half)getDepth(IN.texD0 + dofRowDelta3);
	depth.y = (half)getDepth(IN.texD1 + dofRowDelta3);
	depth.z = (half)getDepth(IN.texD2 + dofRowDelta3);
	depth.w = (half)getDepth(IN.texD3 + dofRowDelta3);
	curCoc = (half4)saturate(1.0f.xxxx - ((depth.xyzw - hiDofWorldNearStart4)*ooDist.xxxx));
	coc = max(coc, curCoc);

	half maxCoC =  max( max( coc.x, coc.y ), max( coc.z, coc.w ) ); 

	return maxCoC;
}

half4  PS_DownSampleCoC(postfxVertexOutputDof IN): COLOR    
{
	return half4(h3tex2D(HDRSampler, IN.tex), XENON_SWITCH(0.0f, ComputeNearCoC(IN)));
}

half4  PS_DOFCoC(postfxVertexOutputPassThrough IN): COLOR  
{
	half4 downsampled = (half4)h4tex2D(HDRSampler, IN.tex.xy);
	half4 blurred = (half4)h4tex2D(BlurSampler, IN.tex.xy);

	half3 col = downsampled.rgb;
	half coc = 2.0f*max(blurred.a, downsampled.a)-downsampled.a;  
	return half4(col.xyz, coc);
}

half4  PS_Blur(postfxVertexOutputPassThrough IN): COLOR     
{ 
	return Common5TapBlur(HDRSampler, IN.tex.xy);
}

#define HH_VEGETATION_MASK	(3.0f/255.0f)
#define HH_TERRAIN_MASK		(4.0f/255.0f)
#define HH_WATER_MASK		(23.0f/255.0f)
#define HH_WATER_END_MASK	(24.0f/255.0f)

half4  PS_HeatHazeWater(postfxVertexOutputPassThrough IN): COLOR 
{

	const half depth			= (half)getDepth(IN.tex.xy);
	const half startDistance	= (half)HeatHazeParams.x;
	const half endDistance   	= startDistance+(half)HeatHazeParams.y;


	float width = (endDistance - startDistance) * 0.5f;
	float a = (depth - (startDistance + width)) / width;


	return half4(lerp(HH_minIntensity, HH_maxIntensity, 1.0 - saturate(abs(a))).xxxx);
}

half4  PS_HeatHaze(postfxSampleInputPassThrough IN): COLOR 
{
#if MULTISAMPLE_TECHNIQUES
	int3 iPos = getIntCoords( IN.tex.xy, globalScreenSize);
	half3 normalSample = gbufferTexture0.Load( iPos.xy, 0 );
#else
	half3 normalSample = h3tex2D(GBufferTextureSampler0, IN.tex.xy);
#endif

	const half nZ = (half)normalize((normalSample.xyz*2.0f)-1.0f).z;
	const half depth			= (half)getDepth(IN.tex.xy);
	const half startDistance	= (half)HeatHazeParams.x;
	const half radius			= (half)HeatHazeParams.y;
	const half angleThreshold	= (half)0.97f;

	// mask out vegetation and water
	const half stencilMask = (half)ReadStencilMask(IN.tex.xy
												#if MULTISAMPLE_TECHNIQUES
													, SAMPLE_INDEX
												#endif
													);
	const half4 refBounds = half4(HH_VEGETATION_MASK, HH_TERRAIN_MASK, HH_WATER_MASK, HH_WATER_END_MASK);
	const half vegWaterMask = !((stencilMask >= refBounds.x && stencilMask < refBounds.y) || (stencilMask >= refBounds.z && stencilMask < refBounds.w));

	half depthInRange = (depth > startDistance && depth < startDistance+radius);
	depthInRange = (half)((nZ < angleThreshold) ? 0.0f : depthInRange)*vegWaterMask;

	return half4(depthInRange.xxxx);
}

half DilationNorth(float2 tex)
{
	half sampleC = h1tex2D(CurLumSampler, tex.xy);

	const int numSamples = 4;
	for (int i = 0; i < numSamples; i++)
	{
		sampleC += tex2DOffset(CurLumSampler, tex.xy, float2(0,i));
	}

	sampleC /= (numSamples+1);

	return sampleC;
}

half4 PS_HeatHazeDilateBinary(postfxVertexOutputPassThrough IN): COLOR
{
	half sampleC = DilationNorth(IN.tex.xy);
	return half4((sampleC>0).xxxx);
}

half4 PS_HeatHazeDilate(postfxVertexOutputPassThrough IN): COLOR
{
	half sampleC = DilationNorth(IN.tex.xy);
	return half4(sampleC.xxxx);
}

half4 PS_GaussBlur9x9Bilateral_H(postfxVertexOutputPassThrough IN): COLOR
{
	const float offsets[3] = { 0.0f, 1.38462f, 3.23077f };
	const half  weights[3] = { 0.22703f, 0.31622f, 0.07027f };
	
	half4 col = h4tex2D(HDRSampler, IN.tex.xy);

	float d0 = col.y;
	float weightsSum = weights[0];
	col.x *= weights[0];
	
	const float depthScale = -10.0f;
	for (int i = 1; i < 3; i++)
	{
#if __PS3
		col += tex2DOffset(HDRSampler, IN.tex.xy, float2(offsets[i], 0.0f))* weights[i];
		col += tex2DOffset(HDRSampler, IN.tex.xy, float2(-offsets[i], 0.0f)) * weights[i];
#else
		float2 clrl = h4tex2D(HDRSampler, IN.tex.xy + float2(offsets[i], 0.0f)*BloomTexelSize.xy);
		float2 clrr = h4tex2D(HDRSampler, IN.tex.xy + float2(-offsets[i], 0.0f)*BloomTexelSize.xy);
		
		float dl = clrl.y;
		float dr = clrr.y;

		float wl = exp(depthScale*abs(dl - d0)/d0)* weights[i];
		float wr = exp(depthScale*abs(dr - d0)/d0) * weights[i];

		col += clrl.xxxx * wl;
		col += clrr.xxxx * wr;
		
		weightsSum += wl + wr;
#endif
	}
	return half4(col.x / weightsSum, d0, 0.0f, 0.0f);
}

half4 PS_GaussBlur9x9Bilateral_V(postfxVertexOutputPassThrough IN): COLOR
{
	const float offsets[3] = { 0.0f, 1.38462f, 3.23077f };
	const half  weights[3] = { 0.22703f, 0.31622f, 0.07027f };
	 
	half4 col = h4tex2D(HDRSampler, IN.tex.xy);

	float d0 = col.y;

	float weightsSum = weights[0];
	col.x *= weights[0];
	
	const float depthScale = -10.0f;

	for (int i = 1; i < 3; i++)
	{
#if __PS3
		col += tex2DOffset(HDRSampler, IN.tex.xy, float2(0.0, offsets[i])) * weights[i];
		col += tex2DOffset(HDRSampler, IN.tex.xy, float2(0.0, -offsets[i])) * weights[i];
#else 
		float2 clrt = h4tex2D(HDRSampler, IN.tex.xy + float2(0.0f, offsets[i])*BloomTexelSize.xy);
		float2 clrb = h4tex2D(HDRSampler, IN.tex.xy + float2(0.0, -offsets[i])*BloomTexelSize.xy);

		float dt = clrt.y;
		float db = clrb.y;

		float wt = exp(depthScale*abs(dt - d0)/d0) * weights[i];
		float wb = exp(depthScale*abs(db - d0)/d0) * weights[i];
		col += clrt.xxxx * wt ;
		col += clrb.xxxx * wb ;
			
		weightsSum += wt + wb;
#endif
	 }

	return half4(col.x / weightsSum, d0, 0.0f, 0.0f);
}

#define SAMPLES_ULTRA 8
half4 PS_GaussBlur9x9Bilateral_H_High(postfxVertexOutputPassThrough IN): COLOR
{
	half4 col = h4tex2D(HDRSampler, IN.tex.xy);
	const float oneSampleWeight = (SAMPLES_ULTRA -1) * 2.0 + 1.0;

	float d0 = col.y;
	float weightsSum = oneSampleWeight;
	col.x *= oneSampleWeight;

	const float depthScale = -10.0f;
	for (int i = 1; i < SAMPLES_ULTRA; i++)
	{
		float2 clrl = h4tex2D(HDRSampler, IN.tex.xy + float2(i, 0.0f)*BloomTexelSize.xy);
		float2 clrr = h4tex2D(HDRSampler, IN.tex.xy + float2(-i, 0.0f)*BloomTexelSize.xy);

		float dl = clrl.y;
		float dr = clrr.y;

		float wl = exp(depthScale*abs(dl - d0)/d0)* oneSampleWeight;
		float wr = exp(depthScale*abs(dr - d0)/d0) * oneSampleWeight;

		col += clrl.xxxx * wl;
		col += clrr.xxxx * wr;

		weightsSum += wl + wr;
	}
	return half4(col.x / weightsSum, d0, 0.0f, 0.0f);
}

half4 PS_GaussBlur9x9Bilateral_V_High(postfxVertexOutputPassThrough IN): COLOR
{
	half4 col = h4tex2D(HDRSampler, IN.tex.xy);

	float d0 = col.y;

	const float oneSampleWeight = (SAMPLES_ULTRA -1) * 2.0 + 1.0;
	float weightsSum = oneSampleWeight;
	col.x *= oneSampleWeight;

	const float depthScale = -10.0f;

	for (int i = 1; i < SAMPLES_ULTRA; i++)
	{

		float2 clrt = h4tex2D(HDRSampler, IN.tex.xy + float2(0.0f, i)*BloomTexelSize.xy);
		float2 clrb = h4tex2D(HDRSampler, IN.tex.xy + float2(0.0, -i)*BloomTexelSize.xy);

		float dt = clrt.y;
		float db = clrb.y;

		float wt = exp(depthScale*abs(dt - d0)/d0) * oneSampleWeight;
		float wb = exp(depthScale*abs(db - d0)/d0) * oneSampleWeight;
		col += clrt.xxxx * wt ;
		col += clrb.xxxx * wb ;

		weightsSum += wt + wb;
	}

	return half4(col.x / weightsSum, d0, 0.0f, 0.0f);
}

half4 PS_GaussBlur9x9_H(postfxVertexOutputPassThrough IN): COLOR
{
	const float offsets[3] = { 0.0f, 1.38462f, 3.23077f };
	const half  weights[3] = { 0.22703f, 0.31622f, 0.07027f };

	 half4 col = h4tex2D(HDRSampler, IN.tex.xy)*weights[0];

	 for (int i = 1; i < 3; i++)
	 {
			col += h4tex2D(HDRSampler, IN.tex.xy + float2(offsets[i], 0.0f)*BloomTexelSize.xy) * weights[i];
			col += h4tex2D(HDRSampler, IN.tex.xy + float2(-offsets[i], 0.0f)*BloomTexelSize.xy) * weights[i];
	 }

	 return half4(col);
}

half4 PS_GaussBlur9x9_V(postfxVertexOutputPassThrough IN): COLOR
{
	const float offsets[3] = { 0.0f, 1.38462f, 3.23077f };
	const half  weights[3] = { 0.22703f, 0.31622f, 0.07027f };

	 half4 col = h4tex2D(HDRSampler, IN.tex.xy)*weights[0];

	 for (int i = 1; i < 3; i++)
	 {
			col += h4tex2D(HDRSampler, IN.tex.xy + float2(0.0, offsets[i])*BloomTexelSize.xy) * weights[i];
			col += h4tex2D(HDRSampler, IN.tex.xy + float2(0.0, -offsets[i])*BloomTexelSize.xy) * weights[i];
	 }

	 return half4(col);
}

half4 PS_BloomComposite(postfxVertexOutputPassThrough IN): COLOR
{
	half3 col = h3tex2D(HDRSampler, IN.tex.xy).xyz*BloomCompositeMultiplier;
	
	// do not output alpha as it might be used as the intensity buffer for light rays
	return half4(col.xyz, 0);
}

half4 PS_BloomComposite_SeeThrough(postfxVertexOutputPassThrough IN): COLOR
{
	 return half4(h4tex2D(HDRSampler, IN.tex.xy).xyzw);
}

//
// fast 3x3 blur filter
//
half4  PS_SSLRBlur(postfxVertexOutputPassThrough IN): COLOR
{
	//  5 tap 3x3 filter suggested by Piotr: 
	//				A B B
	//				A E D
	//				C C D
#if __PS3  
	// NOTE: on the ps3, the intensity buffer is in the alpha channel of the bloom buffer, so just blur alpha.
	half	A = tex2DOffset( BloomSampler, IN.tex.xy, float2(-1,-.5)).a;
	half	B = tex2DOffset( BloomSampler, IN.tex.xy, float2(.5,-1)).a;
	half	C = tex2DOffset( BloomSampler, IN.tex.xy, float2(-.5,1)).a;
	half	D = tex2DOffset( BloomSampler, IN.tex.xy, float2(1,.5)).a;
	half4	E = tex2DOffset( BloomSampler, IN.tex.xy, float2(0,0)); 
	return half4(E.rgb, (A+B+C+D+E.a/2)/4.5);    
#else
	float offset	= 0;
#if __XENON || (__WIN32PC && __SHADERMODEL < 40)
	offset			= 0.5;
#endif
	//4 tap extension of the 5 tap 3x3 filter suggested by Piotr:
	//				A A B
	//				A A B
	//				C C D
	float4 A	= tex2DOffset( BloomSampler, IN.tex.xy, float2(-0.5,  0.5) + offset);
	float4 B	= tex2DOffset( BloomSampler, IN.tex.xy, float2( 1.0,  0.5) + offset);
	float4 C	= tex2DOffset( BloomSampler, IN.tex.xy, float2(-0.5, -1.0) + offset);
	float4 D	= tex2DOffset( BloomSampler, IN.tex.xy, float2( 1.0, -1.0) + offset);
	return (A*4 + B*2 + C*2 + D)/9;
#endif
}	


// Implements the platform specific anti-aliasing pass (called from PS_AA)
// vPixelSize = 1.0/resolution
half4 AntiAliasingPS (postfxVertexOutputAA IN, float2 vPixelSize, bool useGather )
{
#if USE_FXAA_3
	#if FXAA_HLSL_4 || FXAA_HLSL_5
		struct FxaaTex AA_INPUT = { BackBufferSampler, BackBufferTexture };
	#else
		#define AA_INPUT	BackBufferSampler
	#endif
	return FxaaPixelShader(	IN.p, IN.pPos, AA_INPUT, vPixelSize, useGather );
	#undef AA_INPUT
#else
	float3 color = float3(0,0,0);
	#if USE_FXAA
		color = FxaaPixelShader(IN.tex.xy, BackBufferSampler, vPixelSize);
	#elif USE_FXAA_2
		color = FxaaPixelShader(IN.tex.xyzw, BackBufferSampler, vPixelSize);
	#else
		color = tex2D(BackBufferSampler, IN.tex.xy);
	#endif
	return half4(color, 1.0);

#endif
}


// Main entry point for anti-aliasing pass. Worths for variable resolution.
half4 PS_AA(postfxVertexOutputAA IN ) : COLOR
{
#if USE_FXAA_3
	return AntiAliasingPS( IN, rcpFrame, false );
#else
	return AntiAliasingPS( IN, gooScreenSize.xy, false );
#endif
}

#if RSG_PC
// Main entry point for anti-aliasing pass. Worths for variable resolution.
half4 PS_AA_SM50(postfxVertexOutputAA IN ) : COLOR
{
#if USE_FXAA_3
	return AntiAliasingPS( IN, rcpFrame, true );
#else
	return AntiAliasingPS( IN, gooScreenSize.xy, true );
#endif
}
#endif

#if __PS3

// Same as PS_AA but assumes we're running at 720p (saves 3 cycles on the PS3)
half4 PS_AA_720p(postfxVertexOutputAA IN ) : COLOR
{
	static const half2 vPixelSize = float2(1.0/1280.0, 1.0/720.0);
	return AntiAliasingPS( IN, vPixelSize );
}

#endif // __PS3

float4 PS_UIAA(postfxVertexOutputPassThrough IN) : COLOR
{
	float2 texelSize = gooScreenSize.xy;
	float4 xxyy;
	float2 tex	= IN.tex;

	float scale = 0.5;
	float ne	= tex2D(BackBufferSampler, tex + float2( texelSize.x*scale,  texelSize.y*scale)).a;
	float nw	= tex2D(BackBufferSampler, tex + float2(-texelSize.x*scale,  texelSize.y*scale)).a;
	float sw	= tex2D(BackBufferSampler, tex + float2(-texelSize.x*scale, -texelSize.y*scale)).a;
	float se	= tex2D(BackBufferSampler, tex + float2( texelSize.x*scale, -texelSize.y*scale)).a;

	float str	= 3;
	float2 bump = float2(ne - sw, nw - se);
	float2 dir	= float2(-1, 1)*bump.x + float2(-1, -1)*bump.y;
	float len	= pow(saturate(length(dir)*str), 4);
	float2 dxdy = normalize(dir)*len*texelSize;

	float4 color = 0;
	//color += tex2D(BackBufferSampler, tex - 3.5*dxdy);
	color += tex2D(BackBufferSampler, tex - 1.5*dxdy);
	color += tex2D(BackBufferSampler, tex)*0.5;
	color += tex2D(BackBufferSampler, tex + 1.5*dxdy);
	//color += tex2D(BackBufferSampler, tex + 3.5*dxdy);
	return color*0.4;
}

float4 PS_LensDistortion(postfxVertexOutputPassThrough IN) : COLOR
{
	return float4( LensDistortionFilter( IN.tex, 0 ), 1 );
}

float4 PS_BlurVignetting(postfxVertexOutputPassThrough IN) : COLOR
{
	float3 col = tex2D(HDRSampler, IN.tex.xy).xyz;
	float blendLevel;
	col = VignetteBlurFilterAlphaBlended(IN.tex.xy, col, PostFxSampler3, blendLevel);
	return float4(col, blendLevel);
}


float LensArtefactsGetFadeMaskMultiplier(float2 tex, float LensArtefactsFadeMaskType, float LensArtefactsFadeMaskMin, float LensArtefactsFadeMaskMax)
{
	const float t = ((LensArtefactsFadeMaskType > 0.0f) ? abs(tex.y - 0.5f) : abs(tex.x - 0.5f))*2.0f; 
	float fadeMaskMult = LensArtefactsFadeMaskMin + (LensArtefactsFadeMaskMax-LensArtefactsFadeMaskMin)*t;
	return fadeMaskMult*fadeMaskMult;
}

half4 PS_LensArtefacts(postfxVertexOutputPassThrough IN): COLOR
{
	float2 offset = (IN.tex.xy-LensArtefactsOffset0)*LensArtefactsScale0+(1.0f.xx-LensArtefactsOffset0);

	// fade mask
	float fadeMaskMult = saturate(LensArtefactsGetFadeMaskMultiplier(IN.tex.xy, LensArtefactsFadeMaskType0, LensArtefactsFadeMaskMin0, LensArtefactsFadeMaskMax0));

	half3 col = min(h3tex2D(HDRSampler, offset).xyz, 65504.0)*LensArtefactsColorShift0;
	col *= LensArtefactsOpacity0*fadeMaskMult;

	// do not output alpha as it might be used as the intensity buffer for light rays
	return half4(col.xyz, 0);
}

half4 PS_LensArtefactsCombined(postfxVertexOutputPassThrough IN): COLOR
{
	// First layer
	const float2 offset0 = (IN.tex.xy-LensArtefactsOffset0)*LensArtefactsScale0+(1.0f.xx-LensArtefactsOffset0);
	const float fadeMaskMult0 = saturate(LensArtefactsGetFadeMaskMultiplier(IN.tex.xy, LensArtefactsFadeMaskType0, LensArtefactsFadeMaskMin0, LensArtefactsFadeMaskMax0));
	float3 col0 = tex2D(HDRSampler, offset0).xyz*LensArtefactsColorShift0;
	col0 *= LensArtefactsOpacity0*fadeMaskMult0;

	// Second layer
	const float2 offset1 = (IN.tex.xy-LensArtefactsOffset1)*LensArtefactsScale1+(1.0f.xx-LensArtefactsOffset1);
	const float fadeMaskMult1 = saturate(LensArtefactsGetFadeMaskMultiplier(IN.tex.xy, LensArtefactsFadeMaskType1, LensArtefactsFadeMaskMin1, LensArtefactsFadeMaskMax1));
	float col1 = h3tex2D(BlurSampler, offset1).xyz*LensArtefactsColorShift1;
	col1 *= LensArtefactsOpacity1*fadeMaskMult1;

	return half4(col0+col1, 0);
}

half4 LightStreaksBlur(postfxVertexOutputPassThrough IN, int sampleCount, float ooSampleCount)
{
	// WIP
	float3 col	= float3(0.0f, 0.0f, 0.0f);

	for (int i = 0; i < sampleCount; i++)
	{
		const float x = 1.0f-(float)(i)*ooSampleCount;
		const float weight = x*x;
		col	+= tex2DOffset( HDRSampler, IN.tex.xy, LightStreaksBlurDir.xy*(float)(i).xx )*weight;
	}

	col *= (1.0f/(float)(sampleCount/4)); 
	col *= LightStreaksColorShift0.xyz;

	// fade mask
	float fadeMaskMult = LensArtefactsGetFadeMaskMultiplier(IN.tex.xy, LensArtefactsFadeMaskType0, LensArtefactsFadeMaskMin0, LensArtefactsFadeMaskMax0);

	return half4(col.rgb*fadeMaskMult, 1.0f);
}

half4 PS_LightStreaksBlurLow(postfxVertexOutputPassThrough IN): COLOR 
{
	return LightStreaksBlur(IN, 4, 1.0f/4.0f);
}

half4 PS_LightStreaksBlurMed(postfxVertexOutputPassThrough IN): COLOR 
{
	return LightStreaksBlur(IN, 16, 1.0f/16.0f);
}

half4 PS_LightStreaksBlurHigh(postfxVertexOutputPassThrough IN): COLOR 
{
	return LightStreaksBlur(IN, 96, 1.0f/96.0f);
}

float GetFPVMBMask(int3 coords)
{
#if __SHADERMODEL >= 40
	#if RSG_PC
		#if MULTISAMPLE_TECHNIQUES
			uint stencilBuffer = StencilCopyTexture.Load(coords.xy, 0).g;
		#else
			uint stencilBuffer = StencilCopyTexture.Load(coords).g;
		#endif
	#else
		#if MULTISAMPLE_TECHNIQUES
			uint stencilBuffer = StencilCopyTexture.Load(coords.xy, 0).r;
		#else
			uint stencilBuffer = StencilCopyTexture.Load(coords).r;
		#endif
	#endif // RSG_PC

	return (((stencilBuffer & (1<<7)) != 0) && 
			((stencilBuffer & 0x7)    != 2));
#else
	uint stencilBuffer = 0;
	return 1.0f;
#endif // __SHADERMODEL >= 40
}

half4 PS_MotionBlurFPV(postfxVertexOutputPassThrough IN) : COLOR
{
#if MULTISAMPLE_TECHNIQUES || __SHADERMODEL < 40 || RSG_PC
	float2 coords[4];
	coords[0] = IN.tex.xy + float2(-0.5f, -0.5f) * TexelSize.xy;
	coords[1] = IN.tex.xy + float2( 0.5f, -0.5f) * TexelSize.xy;
	coords[2] = IN.tex.xy + float2(-0.5f,  0.5f) * TexelSize.xy;
	coords[3] = IN.tex.xy + float2( 0.5f,  0.5f) * TexelSize.xy;

	float mask = 0.0f;
	mask += GetFPVMBMask(int3(coords[0] * TexelSize.zw, 0)) * 0.25;
	mask += GetFPVMBMask(int3(coords[1] * TexelSize.zw, 0)) * 0.25;
	mask += GetFPVMBMask(int3(coords[2] * TexelSize.zw, 0)) * 0.25;
	mask += GetFPVMBMask(int3(coords[3] * TexelSize.zw, 0)) * 0.25;
#else
	uint4 stencil = StencilCopyTexture.GatherRed(StencilCopySampler, IN.tex.xy);
	stencil = (((stencil & DEFERRED_MATERIAL_MOTIONBLUR) != 0) && ((stencil & DEFERRED_MATERIAL_TYPE_MASK) != DEFERRED_MATERIAL_VEHICLE));
	float mask = dot(stencil, 0.25f);
#endif

	half3 col = tex2Dlod(HDRSampler, float4(IN.tex.xy, 0.0f, 0.0f));
	return float4(col, mask);
}

half4 PS_MotionBlurFPV_DS(postfxVertexOutputPassThrough IN) : COLOR
{
	const float2 aspectCorrection = float2(TexelSize.x / TexelSize.y, 1.0);

	float2 vStart = IN.tex.xy;
	float2 vPrev = IN.tex.xy + (fpvMotionBlurVelocity.xy * aspectCorrection * TexelSize.xy) * fpvMotionBlurSize;
	float2 vStep = vPrev - vStart;

	float2 tex = IN.tex.xy;

	const int numSamples = 16;

	float4 initialSample = tex2Dlod(HDRSampler, float4(tex, 0.0f, 0.0f));

	float4 mbSamples[numSamples];
	mbSamples[0] = initialSample;

	const float2 mbStep = vStep / float(numSamples - 1.0f);

	// Look up into our jitter texture using this 'magic' formula
	const float2 JitterOffset = { 58.164f, 47.13f };
	const float2 jitterLookup = tex * JitterOffset + (mbSamples[0].rb * 8.0f);
	float jitterScalar = h1tex2D(JitterSampler, jitterLookup) - 0.5f;
	tex += mbStep * jitterScalar * fpvMotionBlurWeights.w;

	[unroll]
	for (int i = 1; i < numSamples; i++)
	{
		float2 curTC = tex.xy + mbStep*i;		
		float2 curTCr = (round((curTC * TexelSize.zw)) + 0.5) / TexelSize.zw;
		mbSamples[i] = tex2Dlod(HDRSampler, float4(curTCr, 0.0f, 0.0f));
	}

	float4 blurredSample = 0.0f.xxxx;
	float weights = 0.0;
	float alpha = 0.0f;
	float totalAlpha = 0.0f;

	[unroll]
	for (int k = 0; k < numSamples; k++)
	{
		blurredSample += mbSamples[k] * mbSamples[k].a;
		weights += mbSamples[k].a;

		totalAlpha += 1.0 - (k/(numSamples-1.0f));
		alpha += mbSamples[k].a * (1.0 - (k/(numSamples-1.0f)));
	}

	blurredSample /= (weights == 0.0f) ? 1.0f : weights;

	float finalAlpha = saturate((alpha / (totalAlpha * fpvMotionBlurWeights.x)) + mbSamples[0].a);

	return half4(lerp(initialSample.rgb, blurredSample.rgb, finalAlpha), finalAlpha);
}

half4 PS_MotionBlurFPV_Composite(postfxVertexOutputPassThrough IN) : COLOR
{
	float4 res = 0.0f.xxxx;
	const float dt = 0.5f;

	/*
	res += tex2Dlod(HDRSampler, float4(IN.tex.xy + float2(-dt, -dt) * TexelSize.xy, 0.0f, 0.0f));
	res += tex2Dlod(HDRSampler, float4(IN.tex.xy + float2( dt, -dt) * TexelSize.xy, 0.0f, 0.0f));
	res += tex2Dlod(HDRSampler, float4(IN.tex.xy + float2(-dt,  dt) * TexelSize.xy, 0.0f, 0.0f));
	res += tex2Dlod(HDRSampler, float4(IN.tex.xy + float2( dt,  dt) * TexelSize.xy, 0.0f, 0.0f));
	res *= 0.25f;
	*/

	res = tex2Dlod(HDRSampler, float4(IN.tex.xy, 0.0f, 0.0f));

	res.a *= saturate(((fpvMotionBlurWeights.x * max(1.0f, fpvMotionBlurSize)) - 4.0f) / 2.0f);

	return half4(res.rgb, res.a);
}


#define __POSTFX_STRINGIFY(A) #A
#define CGC_POSTFXCOMPFLAGS(x)								__POSTFX_STRINGIFY(-unroll all --O##x -fastmath -disablepc all)
#define CGC_POSTFXCOMPFLAGS_WITH_REGCOUNT(x,y)				__POSTFX_STRINGIFY(-unroll all --O##x -fastmath -regcount y)
#define CGC_POSTFXCOMPFLAGS_NO_UNROLL(x)					__POSTFX_STRINGIFY(-fastmath --O##x)

#if FILM_EFFECT
technique PostFx_FilmEffect
{
	pass pp_filmeffect
	{
		AlphaBlendEnable = false;
		AlphaTestEnable = false;
        VertexShader = compile VERTEXSHADER VS_Passthrough();
        PixelShader  = compile MSAA_PIXEL_SHADER PS_FilmEffect();
	}
}
#endif

technique MSAA_NAME(PostFx)
{    
	// pass 0
    pass pp_lum_4x3_conversion // average HDR luminance
    {
    	AlphaBlendEnable = false;
		AlphaTestEnable = false;    
        VertexShader = compile VERTEXSHADER VS_Passthrough();
        PixelShader  = compile PIXELSHADER PS_Lum4x3Conversion() CGC_FLAGS(CGC_POSTFXCOMPFLAGS(1))	PS4_TARGET(FMT_32_R);
    }
    
    pass pp_lum_4x3
    {
    	AlphaBlendEnable = false;
		AlphaTestEnable = false;    
        VertexShader = compile VERTEXSHADER VS_Passthrough();
        PixelShader  = compile PIXELSHADER PS_Lum4x3() CGC_FLAGS(CGC_POSTFXCOMPFLAGS(1))	PS4_TARGET(FMT_32_R);
    }
    
	// pass 1
    pass pp_lum_4x5 
    {
    	AlphaBlendEnable = false;
		AlphaTestEnable = false;    
        VertexShader = compile VERTEXSHADER VS_Passthrough();
        PixelShader  = compile PIXELSHADER PS_Lum4x5() CGC_FLAGS(CGC_DEFAULTFLAGS)	PS4_TARGET(FMT_32_R);
    }

	// pass 2
    pass pp_lum_5x5
    {
    	AlphaBlendEnable = false;
		AlphaTestEnable = false;    
        VertexShader = compile VERTEXSHADER VS_Passthrough();
        PixelShader  = compile PIXELSHADER PS_Lum5x5()  CGC_FLAGS(CGC_DEFAULTFLAGS)	PS4_TARGET(FMT_32_R);
    }

	// pass 2
    pass pp_lum_2x2
    {
    	AlphaBlendEnable = false;
		AlphaTestEnable = false;    
        VertexShader = compile VERTEXSHADER VS_Passthrough();
        PixelShader  = compile PIXELSHADER PS_Lum2x2()  CGC_FLAGS(CGC_DEFAULTFLAGS)	PS4_TARGET(FMT_32_R);
    }

	pass pp_lum_3x3
	{
		AlphaBlendEnable = false;
		AlphaTestEnable = false;    
		VertexShader = compile VERTEXSHADER VS_Passthrough();
		PixelShader  = compile PIXELSHADER PS_Lum3x3()  CGC_FLAGS(CGC_DEFAULTFLAGS)	PS4_TARGET(FMT_32_R);
	}

    pass pp_min
    {
    	AlphaBlendEnable = false;
		AlphaTestEnable = false;    
        VertexShader = compile VERTEXSHADER VS_Passthrough();
        PixelShader  = compile PIXELSHADER PS_Min()  CGC_FLAGS(CGC_POSTFXCOMPFLAGS(1));
    }

	// pass 3
    pass pp_max // max 
    {
    	AlphaBlendEnable = false;
		AlphaTestEnable = false;    
        VertexShader = compile VERTEXSHADER VS_Passthrough();
        PixelShader  = compile PIXELSHADER PS_Max()  CGC_FLAGS(CGC_POSTFXCOMPFLAGS(1));
    }

	// pass 4
    pass pp_maxx // bright pass blur x 
    {
    	AlphaBlendEnable = false;
		AlphaTestEnable = false;    
#if __PS3		   
        VertexShader = compile VERTEXSHADER VS_Passthrough4TexXOffset();
        PixelShader  = compile PIXELSHADER PS_BlurMax()  CGC_FLAGS(CGC_DEFAULTFLAGS);
#else // __PS3
        VertexShader = compile VERTEXSHADER VS_Passthrough();
        PixelShader  = compile PIXELSHADER PS_BlurMaxX()  CGC_FLAGS(CGC_DEFAULTFLAGS);
#endif // __PS3
    }

	// pass 5
    pass pp_maxy // bright pass blur y 
    {
    	AlphaBlendEnable = false;
		AlphaTestEnable = false; 
#if __PS3		   
        VertexShader = compile VERTEXSHADER VS_Passthrough4TexYOffset();
        PixelShader  = compile PIXELSHADER PS_BlurMax()  CGC_FLAGS(CGC_DEFAULTFLAGS);
#else // __PS3
        VertexShader = compile VERTEXSHADER VS_Passthrough();
        PixelShader  = compile PIXELSHADER PS_BlurMaxY()  CGC_FLAGS(CGC_DEFAULTFLAGS);
#endif // __PS3        
    }           

#define PASS(name,pixelshader,cgc_flags)	\
	pass MSAA_NAME(name)			\
	{								\
		AlphaBlendEnable = false;	\
		AlphaTestEnable = false;	\
        VertexShader = compile VERTEXSHADER			VS_PassthroughComposite();						\
        PixelShader  = compile MSAA_PIXEL_SHADER	pixelshader()  CGC_FLAGS(cgc_flags);	\
	}

	// pass 6: Composite
	PASS( pp_composite, PS_Composite, CGC_POSTFXCOMPFLAGS(1) )
	// pass 7: Composite - with directional motionblur
	PASS( pp_composite_mb, PS_CompositeMB, CGC_POSTFXCOMPFLAGS(1) )
	// pass 8: Composite - with pseudo high DOF
	PASS( pp_composite_highdof, PS_CompositeHighDOF, CGC_POSTFXCOMPFLAGS(1) )
	// pass 9: Composite - with directional motionblur - with pseudo high DOF
	PASS( pp_composite_mb_highdof, PS_CompositeMBHighDOF, CGC_POSTFXCOMPFLAGS(1) )
	// pass 10: Composite - with noise
	PASS( pp_composite_noise, PS_CompositeNoise, CGC_POSTFXCOMPFLAGS(1) )
	// pass 11: Composite - with noise - with directional motionblur
	PASS( pp_composite_noise_mb, PS_CompositeMBNoise, CGC_POSTFXCOMPFLAGS(1) )
	// pass 12: Composite - with noise - with pseudo high DOF
	PASS( pp_composite_noise_highdof, PS_CompositeHighDOFNoise, CGC_POSTFXCOMPFLAGS(1) )
	// pass 13: Composite - with noise - with directional motionblur - with pseudo high DOF
    PASS( pp_composite_noise_mb_highdof, PS_CompositeMBHighDOFNoise, CGC_POSTFXCOMPFLAGS(1) )
	// pass 14: Composite - with NightVision
	PASS( pp_composite_nv, PS_CompositeNV, CGC_POSTFXCOMPFLAGS(3) )
	// pass 15: Composite - with directional motionblur - with NightVision
	PASS( pp_composite_mb_nv, PS_CompositeMBNV, CGC_POSTFXCOMPFLAGS(1) )
	// pass 16: Composite - with pseudo high DOF - with NightVision
	PASS( pp_composite_highdof_nv, PS_CompositeHighDOFNV, CGC_POSTFXCOMPFLAGS(3) )
	// pass 17: Composite - with directional motionblur - with pseudo high DOF - with NightVision
	PASS( pp_composite_mb_highdof_nv, PS_CompositeMBHighDOFNV, CGC_POSTFXCOMPFLAGS(1) )
	// pass 18: Composite - with Heat Haze
	PASS( pp_composite_hh, PS_CompositeHH, CGC_POSTFXCOMPFLAGS(1) )
	// pass 19: Composite - with directional motionblur - with Heat Haze
	PASS( pp_composite_mb_hh, PS_CompositeMBHH, CGC_POSTFXCOMPFLAGS(3) )
	// pass 20: Composite - with pseudo high DOF - with Heat Haze
	PASS( pp_composite_highdof_hh, PS_CompositeHighDOFHH, CGC_POSTFXCOMPFLAGS(3) )
	// pass 21: Composite - with directional motionblur - with pseudo high DOF - with Heat Haze
	PASS( pp_composite_mb_highdof_hh, PS_CompositeMBHighDOFHH, CGC_POSTFXCOMPFLAGS(1) )
	// pass 22: Composite - with noise - with Heat Haze
	PASS( pp_composite_noise_hh, PS_CompositeNoiseHH, CGC_POSTFXCOMPFLAGS(1) )
	// pass 23: Composite - with noise - with directional motionblur - with Heat Haze
	PASS( pp_composite_noise_mb_hh, PS_CompositeMBNoiseHH, CGC_POSTFXCOMPFLAGS(3) )
	// pass 24: Composite - with noise - with pseudo high DOF - with Heat Haze
	PASS( pp_composite_noise_highdof_hh, PS_CompositeHighDOFNoiseHH, CGC_POSTFXCOMPFLAGS(1) )
	// pass 25: Composite - with noise - with directional motionblur - with pseudo high DOF - with Heat Haze
	PASS( pp_composite_noise_mb_highdof_hh, PS_CompositeMBHighDOFNoiseHH, CGC_POSTFXCOMPFLAGS(1) )
	// pass 26: Composite - with NightVision - with Heat Haze
	PASS( pp_composite_nv_hh, PS_CompositeNVHH, CGC_POSTFXCOMPFLAGS(3) )
	// pass 27: Composite - with directional motionblur - with NightVision - with Heat Haze
	PASS( pp_composite_mb_nv_hh, PS_CompositeMBNVHH, CGC_POSTFXCOMPFLAGS(1) )
	// pass 28: Composite - with pseudo high DOF - with NightVision - with Heat Haze
	PASS( pp_composite_highdof_nv_hh, PS_CompositeHighDOFNVHH, CGC_POSTFXCOMPFLAGS(3) )
	// pass 29: Composite - with directional motionblur - with pseudo high DOF - with NightVision - with Heat Haze
	PASS( pp_composite_mb_highdof_nv_hh, PS_CompositeMBHighDOFNVHH, CGC_POSTFXCOMPFLAGS(1) )
	// pass 30: Composite - with pseudo high DOF
	PASS( pp_composite_shallowhighdof, PS_CompositeShallowHighDOF, CGC_POSTFXCOMPFLAGS(1) )
	// pass 31: Composite - with directional motionblur - with pseudo high DOF
	PASS( pp_composite_mb_shallowhighdof, PS_CompositeMBShallowHighDOF, CGC_POSTFXCOMPFLAGS(3) )
	// pass 32: Composite - with noise - with pseudo high DOF
	PASS( pp_composite_noise_shallowhighdof, PS_CompositeShallowHighDOFNoise, CGC_POSTFXCOMPFLAGS(1) )
	// pass 33: Composite - with noise - with directional motionblur - with pseudo high DOF
	PASS( pp_composite_noise_mb_shallowhighdof, PS_CompositeMBShallowHighDOFNoise, CGC_POSTFXCOMPFLAGS(3) )
	// pass 34: Composite - with pseudo high DOF - with NightVision
	PASS( pp_composite_shallowhighdof_nv, PS_CompositeShallowHighDOFNV, CGC_POSTFXCOMPFLAGS(3) )
	// pass 35: Composite - with directional motionblur - with pseudo high DOF - with NightVision
	PASS( pp_composite_mb_shallowhighdof_nv, PS_CompositeMBShallowHighDOFNV, CGC_POSTFXCOMPFLAGS(1) )
	// pass 36: Composite - with pseudo high DOF - with Heat Haze
	PASS( pp_composite_shallowhighdof_hh, PS_CompositeShallowHighDOFHH, CGC_POSTFXCOMPFLAGS(1) )
	// pass 37: Composite - with directional motionblur - with pseudo high DOF - with Heat Haze
	PASS( pp_composite_mb_shallowhighdof_hh, PS_CompositeMBShallowHighDOFHH, CGC_POSTFXCOMPFLAGS(3) )
	// pass 38: Composite - with noise - with pseudo high DOF - with Heat Haze
	PASS( pp_composite_noise_shallowhighdof_hh, PS_CompositeShallowHighDOFNoiseHH, CGC_POSTFXCOMPFLAGS(3) )
	// pass 39: Composite - with noise - with directional motionblur - with pseudo high DOF - with Heat Haze
	PASS( pp_composite_noise_mb_shallowhighdof_hh, PS_CompositeMBShallowHighDOFNoiseHH, CGC_POSTFXCOMPFLAGS(3) )
	// pass 40: Composite - with pseudo high DOF - with NightVision - with Heat Haze
	PASS( pp_composite_shallowhighdof_nv_hh, PS_CompositeShallowHighDOFNVHH, CGC_POSTFXCOMPFLAGS(3) )
	// pass 41: Composite - with directional motionblur - with pseudo high DOF - with NightVision - with Heat Haze
	PASS( pp_composite_mb_shallowhighdof_nv_hh, PS_CompositeMBShallowHighDOFNVHH, CGC_POSTFXCOMPFLAGS(1) )
	// pass 42: Composite - with extra fxs
	PASS( pp_composite_ee, PS_CompositeExtraFX, CGC_POSTFXCOMPFLAGS(3) )					
	// pass 43: Composite - with extra fxs - with motion blur
	PASS( pp_composite_mb_ee, PS_CompositeMBExtraFX, CGC_POSTFXCOMPFLAGS(1) )		
	// pass 44: Composite - with extra fxs - with pseudo high DOF
	PASS( pp_composite_highdof_ee, PS_CompositeHighDOFExtraFX, CGC_POSTFXCOMPFLAGS(3) )	
	// pass 45: Composite - with extra fxs - with motion blur - with pseudo high DOF
	PASS( pp_composite_mb_highdof_ee, PS_CompositeMBHighDOFExtraFX, CGC_POSTFXCOMPFLAGS(3) )			
	// pass 46: Composite - with extra fxs - with heat haze
	PASS( pp_composite_hh_ee, PS_CompositeHHExtraFX, CGC_POSTFXCOMPFLAGS(1) )					
	// pass 47: Composite - with extra fxs - with motion blur - with heat haze
	PASS( pp_composite_mb_hh_ee, PS_CompositeMBHHExtraFX, CGC_POSTFXCOMPFLAGS(1) )			
	// pass 48: Composite - with extra fxs - with pseudo high DOF - with heat haze
	PASS( pp_composite_highdof_hh_ee, PS_CompositeHighDOFHHExtraFX, CGC_POSTFXCOMPFLAGS(3) )			
	// pass 49: Composite - with extra fxs - with motion blur - with pseudo high DOF
	PASS( pp_composite_mb_highdof_hh_ee, PS_CompositeMBHighDOFHHExtraFX, CGC_POSTFXCOMPFLAGS(3) )		
	// pass 50: Composite - with extra fxs - with shallow high DOF
	PASS( pp_composite_shallowhighdof_ee, PS_CompositeShallowHighDOFExtraFX, CGC_POSTFXCOMPFLAGS(3) )	
	// pass 51: Composite - with extra fxs - with motion blur - with shallow high DOF
	PASS( pp_composite_mb_shallowhighdof_ee, PS_CompositeMBShallowHighDOFExtraFX, CGC_POSTFXCOMPFLAGS(1) )
	// pass 52: Composite - with extra fxs - with shallow high DOF - with heat haze
	PASS( pp_composite_shallowhighdof_hh_ee, PS_CompositeShallowHighDOFHHExtraFX, CGC_POSTFXCOMPFLAGS(3) )
	// pass 53: Composite - with extra fxs - with motion blur - with shallow high dof - with heat haze
	PASS( pp_composite_mb_shallowhighdof_hh_ee, PS_CompositeMBShallowHighDOFHHExtraFX, CGC_POSTFXCOMPFLAGS(1) )

	// pass 54: Composite - with see through bloom
	PASS( pp_composite_seethrough, PS_CompositeSeeThrough, CGC_POSTFXCOMPFLAGS(3) )

#undef PASS
    
	// pass : 55
	pass MSAA_NAME(pp_composite_tiled)
    {
		// Don't put any render states here or you'll invalidate the "depthFXBlendState" state block!
        VertexShader = compile VERTEXSHADER			VS_PassthroughCompositeTiled();
        PixelShader  = compile MSAA_PIXEL_SHADER	PS_Composite()  CGC_FLAGS(CGC_POSTFXCOMPFLAGS(1));
    }

	// pass : 56
	pass MSAA_NAME(pp_composite_tiled_noblur)
    {
		// Don't put any render states here or you'll invalidate the "depthFXBlendState" state block!
        VertexShader = compile VERTEXSHADER			VS_PassthroughCompositeTiled();
        PixelShader  = compile MSAA_PIXEL_SHADER	PS_Composite_NoBlur()  CGC_FLAGS(CGC_POSTFXCOMPFLAGS(1));
    }

#if (__SHADERMODEL >= 40)
	// pass : 57
	pass MSAA_NAME(pp_depthfx) // Depth Based effects
    {
		// Don't put any render states here or you'll invalidate the "depthFXBlendState" state block!
        VertexShader = compile VERTEXSHADER			VS_DepthEffects();
        PixelShader  = compile ps_5_0				PS_DepthEffects()  CGC_FLAGS(CGC_DEFAULTFLAGS);
    }
   
	// pass : 58
	pass MSAA_NAME(pp_depthfx_tiled) // Depth Based effects
    {
		// Don't put any render states here or you'll invalidate the "depthFXBlendState" state block!
        VertexShader = compile VERTEXSHADER			VS_DepthEffects_Tiled();
        PixelShader  = compile ps_5_0				PS_DepthEffects()  CGC_FLAGS(CGC_DEFAULTFLAGS);
    }
#endif

#if RSG_PC
	// pass : 57
	pass MSAA_NAME(pp_depthfx_nogather) // Depth Based effects
    {
		// Don't put any render states here or you'll invalidate the "depthFXBlendState" state block!
        VertexShader = compile VERTEXSHADER			VS_DepthEffects();
        PixelShader  = compile MSAA_PIXEL_SHADER	PS_DepthEffects_nogather()  CGC_FLAGS(CGC_DEFAULTFLAGS);
    }
    
    pass MSAA_NAME(pp_depthfx_tiled_nogather) // Depth Based effects
    {
		// Don't put any render states here or you'll invalidate the "depthFXBlendState" state block!
        VertexShader = compile VERTEXSHADER			VS_DepthEffects_Tiled();
        PixelShader  = compile MSAA_PIXEL_SHADER	PS_DepthEffects_nogather()  CGC_FLAGS(CGC_DEFAULTFLAGS);
    }
#endif

	// pass : 59
	pass MSAA_NAME(pp_simple) // Simplified PostFx pass
	{
    	AlphaBlendEnable = false;
		AlphaTestEnable = false;
        VertexShader = compile VERTEXSHADER			VS_PassthroughComposite();
        PixelShader  = compile MSAA_PIXEL_SHADER	PS_Simple()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	// pass : 60
	pass MSAA_NAME(pp_passthrough) // Pass through (don't do any postfx)
	{
    	AlphaBlendEnable = false;
		AlphaTestEnable = false;
        VertexShader = compile VERTEXSHADER			VS_PassthroughComposite();
        PixelShader  = compile MSAA_PIXEL_SHADER	PS_PassThrough()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	// pass : 61
	pass pp_lightrays1
    {
    	AlphaBlendEnable = false;
		AlphaTestEnable = false;
        VertexShader = compile VERTEXSHADER VS_PassthroughP();
        PixelShader  = compile PIXELSHADER PS_LightRays1()  CGC_FLAGS(CGC_DEFAULTFLAGS);
    }
    
	// pass : 62
    pass pp_sslrcutout
	{
		AlphaBlendEnable=false;
		CullMode = NONE;
  		ZEnable = false;
		VertexShader = compile VERTEXSHADER VS_Passthrough();
		PixelShader = compile PIXELSHADER PS_SSLRMakeCutout() CGC_FLAGS(CGC_POSTFXCOMPFLAGS_NO_UNROLL(1));
	}

	// pass : 63
    pass pp_sslrcutout_tiled
	{
		AlphaBlendEnable=false;
		CullMode = NONE;
  		ZEnable = false;
		VertexShader = compile VERTEXSHADER VS_SSLRMakeCutout_Tiled();
		PixelShader = compile PIXELSHADER PS_SSLRMakeCutout() CGC_FLAGS(CGC_POSTFXCOMPFLAGS_NO_UNROLL(1));
	}

	// pass : 64
    pass pp_sslrextruderays
	{
		AlphaBlendEnable=false;
		AlphaTestEnable = false;
#if IB_IN_BLOOM_ALPHA				// just update the intensity buffer this time
		ColorWriteEnable = ALPHA;
#endif 
		VertexShader = compile VERTEXSHADER VS_SSLRExtrudeRays();
		PixelShader = compile PIXELSHADER PS_SSLRExtrudeRays() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

#if (__SHADERMODEL >= 40)
	pass MSAA_NAME(pp_fogray)
	{
		AlphaBlendEnable=false;
		AlphaTestEnable = false;

		VertexShader = compile VERTEXSHADER VS_PassthroughPFogRay();
		PixelShader = compile ps_5_0 PS_FogRays() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass MSAA_NAME(pp_fogray_high)
	{
		AlphaBlendEnable=false;
		AlphaTestEnable = false;

		VertexShader = compile VERTEXSHADER VS_PassthroughPFogRay();
		PixelShader = compile ps_5_0 PS_FogRaysHigh() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
#endif

#if RSG_PC	
	pass MSAA_NAME(pp_fogray_nogather)
	{
		AlphaBlendEnable=false;
		AlphaTestEnable = false;

		VertexShader = compile VERTEXSHADER VS_PassthroughPFogRay();
		PixelShader = compile MSAA_PIXEL_SHADER PS_FogRaysNoGather() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass MSAA_NAME(pp_fogray_high_nogather)
	{
		AlphaBlendEnable=false;
		AlphaTestEnable = false;

		VertexShader = compile VERTEXSHADER VS_PassthroughPFogRay();
		PixelShader = compile MSAA_PIXEL_SHADER PS_FogRaysHighNoGather() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
#endif

	pass MSAA_NAME(pp_shadowmapblit)
	{
		AlphaBlendEnable=false;
		AlphaTestEnable = false;

		VertexShader = compile VERTEXSHADER VS_Passthrough();
		PixelShader = compile MSAA_PIXEL_SHADER PS_BlitDepth() CGC_FLAGS(CGC_DEFAULTFLAGS)	PS4_TARGET(FMT_32_R);
	}


	// pass : 65
	pass pp_calcbloom0
    {
    	AlphaBlendEnable = false;
		AlphaTestEnable = false;
        VertexShader = compile VERTEXSHADER VS_PassthroughCompression();
        PixelShader  = compile PIXELSHADER PS_CalcBloom0()  CGC_FLAGS(CGC_DEFAULTFLAGS);
    }

	// pass : 66
	pass pp_calcbloom1
    {
    	AlphaBlendEnable = false;
		AlphaTestEnable = false;
        VertexShader = compile VERTEXSHADER VS_Passthrough();
        PixelShader  = compile PIXELSHADER PS_CalcBloom1()  CGC_FLAGS(CGC_DEFAULTFLAGS);
    }
    
	// pass : 67
	pass MSAA_NAME(pp_calcbloom_seethrough)
    {
    	AlphaBlendEnable = false;
		AlphaTestEnable = false;
        VertexShader = compile VERTEXSHADER			VS_Passthrough();
        PixelShader  = compile MSAA_PIXEL_SHADER	PS_CalcBloom_SeeThrough()  CGC_FLAGS(CGC_DEFAULTFLAGS);
    }
    
	// pass : 68
   pass pp_sslrblur
	{
        AlphaTestEnable = false;
        AlphaBlendEnable = false;
        CullMode = NONE;
        ZWriteEnable = false;
        ZEnable = false;
        VertexShader = compile VERTEXSHADER VS_Passthrough();
        PixelShader  = compile PIXELSHADER PS_SSLRBlur() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

   // pass : 69
	pass MSAA_NAME(pp_subsampleAlpha)
	{
		// See render state blocks in PostProcessFX.cpp : subSampleAlphaDSState & subSampleAlphaWithStencilCullDSState
		VertexShader = compile VERTEXSHADER			VS_PassthroughSafe();
        PixelShader  = compile MSAA_PIXEL_SHADER	PS_SubSampleAlpha()  CGC_FLAGS(CGC_DEFAULTFLAGS);
    }

	// pass : 70
	pass MSAA_NAME(pp_subsampleAlphaSinglePass)
	{
    	// See render state blocks in PostProcessFX.cpp : subSampleAlphaDSState & subSampleAlphaWithStencilCullDSState
		VertexShader = compile VERTEXSHADER			VS_PassthroughSafe();
        PixelShader  = compile MSAA_PIXEL_SHADER	PS_SubSampleAlphaSinglePass()  CGC_FLAGS(CGC_DEFAULTFLAGS);
    }
    
	// pass : 71
    pass pp_DownSampleBloom
    {
        AlphaBlendEnable = false;
        AlphaTestEnable = false;
  		ZEnable = false;
		ZWriteEnable = false;
        VertexShader = compile VERTEXSHADER VS_Passthrough();
        PixelShader  = compile PIXELSHADER PS_DownSampleBloom()  CGC_FLAGS(CGC_POSTFXCOMPFLAGS(1));
    }

	// pass : 72
    pass MSAA_NAME(pp_DownSampleStencil)
    {
        AlphaBlendEnable = false;
        AlphaTestEnable = false;
  		ZEnable = false;
		ZWriteEnable = false;
        VertexShader = compile VERTEXSHADER			VS_Passthrough();
        PixelShader  = compile MSAA_PIXEL_SHADER	PS_DownSampleStencil()  CGC_FLAGS(CGC_DEFAULTFLAGS);
    }

	// pass : 73
    pass MSAA_NAME(pp_DownSampleCoC)
    {
        AlphaBlendEnable = false;
        AlphaTestEnable = false;
  		ZEnable = false;
		ZWriteEnable = false;
        VertexShader = compile VERTEXSHADER			VS_Dof();
        PixelShader  = compile MSAA_PIXEL_SHADER	PS_DownSampleCoC()  CGC_FLAGS(CGC_POSTFXCOMPFLAGS(3));
    }

	// pass : 74
    pass pp_Blur
    {
        AlphaBlendEnable = false;
        AlphaTestEnable = false;
  		ZEnable = false;
		ZWriteEnable = false;
        VertexShader = compile VERTEXSHADER VS_Passthrough();
        PixelShader  = compile PIXELSHADER PS_Blur()  CGC_FLAGS(CGC_DEFAULTFLAGS);
    }

	// pass : 75
	pass pp_DOFCoC
    {
        AlphaBlendEnable = false;
        AlphaTestEnable = false;
  		ZEnable = false;
		ZWriteEnable = false;
        VertexShader = compile VERTEXSHADER VS_Passthrough();
        PixelShader  = compile PIXELSHADER PS_DOFCoC()  CGC_FLAGS(CGC_DEFAULTFLAGS);
    }

	// pass : 76
	pass MSAA_NAME(pp_HeatHaze)
    {
        AlphaBlendEnable = false;
        AlphaTestEnable = false;
  		ZEnable = false;
		ZWriteEnable = false;
        VertexShader = compile VERTEXSHADER			VS_Passthrough();
        PixelShader  = compile MSAA_PIXEL_SHADER	PS_HeatHaze()  CGC_FLAGS(CGC_DEFAULTFLAGS);
    }

	// pass : 77
	pass MSAA_NAME(pp_HeatHaze_Tiled)
    {
        AlphaBlendEnable = false;
        AlphaTestEnable = false;
  		ZEnable = false;
		ZWriteEnable = false;
        VertexShader = compile VERTEXSHADER			VS_HeatHaze_Tiled();
        PixelShader  = compile MSAA_PIXEL_SHADER	PS_HeatHaze()  CGC_FLAGS(CGC_DEFAULTFLAGS);
    }

	// pass : 78
	pass pp_HeatHazeDilateBinary
    {
        AlphaBlendEnable = false;
        AlphaTestEnable = false;
  		ZEnable = false;
		ZWriteEnable = false;
        VertexShader = compile VERTEXSHADER VS_Passthrough();
        PixelShader  = compile PIXELSHADER PS_HeatHazeDilateBinary()  CGC_FLAGS(CGC_DEFAULTFLAGS_NPC(1));
    }

	// pass : 79
	pass pp_HeatHazeDilate
    {
        AlphaBlendEnable = false;
        AlphaTestEnable = false;
  		ZEnable = false;
		ZWriteEnable = false;
        VertexShader = compile VERTEXSHADER VS_Passthrough();
        PixelShader  = compile PIXELSHADER PS_HeatHazeDilate()  CGC_FLAGS(CGC_DEFAULTFLAGS_NPC(1));
    }

	// pass : 80
	pass MSAA_NAME(pp_HeatHazeWater)
	{
        AlphaBlendEnable = false;
        AlphaTestEnable = false;
  		ZEnable = false;
		ZWriteEnable = false;
        VertexShader = compile VERTEXSHADER			VS_Passthrough();
        PixelShader  = compile MSAA_PIXEL_SHADER	PS_HeatHazeWater()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	// pass : 81
	pass pp_GaussBlur_Hor
	{
        AlphaBlendEnable = false;
        AlphaTestEnable = false;
  		ZEnable = false;
		ZWriteEnable = false;
        VertexShader = compile VERTEXSHADER VS_Passthrough();
        PixelShader  = compile PIXELSHADER PS_GaussBlur9x9_H()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	// pass : 82
	pass pp_GaussBlur_Ver
	{
        AlphaBlendEnable = false;
        AlphaTestEnable = false;
  		ZEnable = false;
		ZWriteEnable = false;
        VertexShader = compile VERTEXSHADER VS_Passthrough();
        PixelShader  = compile PIXELSHADER PS_GaussBlur9x9_V()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	// pass : 83
	pass pp_BloomComposite
	{
		#if !__PS3
			AlphaBlendEnable = true;
			SrcBlend         = ONE;
			DestBlend        = ONE;
			BlendOp          = ADD;
		#endif
		
		#if IB_IN_BLOOM_ALPHA
			ColorWriteEnable = RED+GREEN+BLUE;	// don't overwrite alpha if it has SSLR in it
		#endif

		AlphaTestEnable = false;
  		ZEnable = false;
		ZWriteEnable = false;
        VertexShader = compile VERTEXSHADER VS_Passthrough();
        PixelShader  = compile PIXELSHADER PS_BloomComposite()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	// pass : 84
	pass pp_BloomComposite_SeeThrough
	{
		AlphaBlendEnable = true;
		SrcBlend         = ONE;
		DestBlend        = ONE;
		BlendOp          = ADD;
        AlphaTestEnable = false;
  		ZEnable = false;
		ZWriteEnable = false;
        VertexShader = compile VERTEXSHADER VS_Passthrough();
        PixelShader  = compile PIXELSHADER PS_BloomComposite_SeeThrough()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	// pass : 85
	pass pp_gradientfilter 
	{
		AlphaBlendEnable 			= true;
		SrcBlend         			= DESTCOLOR;
		DestBlend        			= ZERO;
		BlendOp          			= ADD;
		//TEMP FIX FOR B*741897 PC - Can be removed when fixed at RAGE level.
		SEPARATEALPHABLENDENABLE	= true;
		SrcBlendAlpha				= ZERO;	
		DestBlendAlpha				= ONE;
		BlendOpAlpha				= ADD;		
		//END TEMP FIX

        AlphaTestEnable				= false;
        VertexShader = compile VERTEXSHADER VS_Passthrough();
        PixelShader  = compile PIXELSHADER PS_GradientFilter()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	// pass : 86
	pass pp_scanline 
	{
		AlphaBlendEnable 			= true;
		SrcBlend         			= SRCALPHA;
		DestBlend        			= INVSRCALPHA;
		BlendOp          			= ADD;

        VertexShader = compile VERTEXSHADER	VS_Passthrough();
        PixelShader  = compile PIXELSHADER	PS_Scanline()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
	
	// pass : 87
	pass pp_AA
	{
        AlphaBlendEnable = false;
        AlphaTestEnable = false;
		ZEnable = false;

        VertexShader = compile VERTEXSHADER VS_AA();

#if USE_FXAA_3
		// Compiler flags to match reference NVSHADERPERF output values from the docs (see the fxaa3.h for details)
	#if FXAA_EARLY_EXIT
        PixelShader  = compile PIXELSHADER PS_AA()  CGC_FLAGS("--fenable-bx2 --fastmath --fastprecision --nofloatbindings --disablepc all --O2 -texformat default RGBA8");
	#else
		PixelShader  = compile PIXELSHADER PS_AA()  CGC_FLAGS("--fenable-bx2 --fastmath --fastprecision --nofloatbindings --disablepc all --O3 -texformat default RGBA8");
	#endif
#endif
	}
	
#if RSG_PC
	pass pp_AA_sm50
	{
        AlphaBlendEnable = false;
        AlphaTestEnable = false;
		ZEnable = false;

        VertexShader = compile VERTEXSHADER VS_AA();

#if USE_FXAA_3
		// Compiler flags to match reference NVSHADERPERF output values from the docs (see the fxaa3.h for details)
	#if FXAA_EARLY_EXIT
        PixelShader  = compile ps_5_0 PS_AA_SM50()  CGC_FLAGS("--fenable-bx2 --fastmath --fastprecision --nofloatbindings --disablepc all --O2 -texformat default RGBA8");
	#else
		PixelShader  = compile ps_5_0 PS_AA_SM50()  CGC_FLAGS("--fenable-bx2 --fastmath --fastprecision --nofloatbindings --disablepc all --O3 -texformat default RGBA8");
	#endif
#endif
	}
#endif

#if __PS3
	// pass : 88
	pass pp_AA_720p
	{
		AlphaBlendEnable = false;
		AlphaTestEnable = false;
		ZEnable = false;

		VertexShader = compile VERTEXSHADER VS_AA();

#if USE_FXAA_3
		// Compiler flags to match reference NVSHADERPERF output values from the docs (see the fxaa3.h for details)
	#if FXAA_EARLY_EXIT
		PixelShader  = compile PIXELSHADER PS_AA_720p()  CGC_FLAGS("--fenable-bx2 --fastmath --fastprecision --nofloatbindings --disablepc all --O2  -texformat default RGBA8");
	#else
		PixelShader  = compile PIXELSHADER PS_AA_720p()  CGC_FLAGS("--fenable-bx2 --fastmath --fastprecision --nofloatbindings --disablepc all --O3  -texformat default RGBA8");
	#endif
#endif
	}
#endif //__PS3

	// pass : 89
	pass pp_UIAA
	{
		VertexShader = compile VERTEXSHADER	VS_Passthrough();
		PixelShader  = compile PIXELSHADER	PS_UIAA() CGC_FLAGS("--fenable-bx2 --fastmath --fastprecision --nofloatbindings --disablepc all --O3  -texformat default RGBA8");
	}

	// pass : 90
	pass pp_JustExposure
	{
		AlphaBlendEnable = false;
		AlphaTestEnable = false;

		VertexShader = compile VERTEXSHADER	VS_PassthroughComposite();
		PixelShader  = compile PIXELSHADER	PS_JustExposure()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	// pass : 91
	pass MSAA_NAME(pp_CopyDepth)
	{
		VertexShader = compile VERTEXSHADER			VS_Passthrough();
		PixelShader  = compile MSAA_PIXEL_SHADER	PS_CopyDepth()  CGC_FLAGS(CGC_POSTFXCOMPFLAGS(1));
	}

	// pass : 92
	pass pp_DamageOverlay
	{
		AlphaBlendEnable = true;
		BlendOp         = Add;
		SrcBlend        = SrcAlpha;
		DestBlend       = InvSrcAlpha;
        AlphaTestEnable	= true;
        AlphaFunc		= GREATER;
        AlphaRef		= 1;
        VertexShader = compile VERTEXSHADER VS_DamageOverlay();		
        PixelShader  = compile PIXELSHADER PS_DamageOverlay()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	// pass : 92
	pass pp_exposure
	{
		AlphaBlendEnable = false;
		AlphaTestEnable = false;

		VertexShader = compile VERTEXSHADER VS_PassthroughComposite();
		PixelShader  = compile PIXELSHADER PS_CalculateExposure()  CGC_FLAGS(CGC_DEFAULTFLAGS)	PS4_TARGET(FMT_32_ABGR);
	}

	// pass : 93
	pass pp_exposure_reset
	{
		AlphaBlendEnable = false;
		AlphaTestEnable = false;

		VertexShader = compile VERTEXSHADER VS_PassthroughComposite();
		PixelShader  = compile PIXELSHADER	PS_ResetExposure()  CGC_FLAGS(CGC_DEFAULTFLAGS)	PS4_TARGET(FMT_32_ABGR);
	}

	// pass : 94
	pass pp_copy // Pass through (but does colour compression / decompression)
	{
		AlphaBlendEnable = false;
		AlphaTestEnable = false;
		VertexShader = compile VERTEXSHADER	VS_PassthroughComposite();
		PixelShader  = compile PIXELSHADER	PS_Copy()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}


	#if !defined(SHADER_FINAL)
	// pass : 95
	pass pp_exposure_set
	{
		AlphaBlendEnable = false;
		AlphaTestEnable = false;

		VertexShader = compile VERTEXSHADER VS_PassthroughComposite();
		PixelShader  = compile PIXELSHADER PS_SetExposure()  CGC_FLAGS(CGC_DEFAULTFLAGS)	PS4_TARGET(FMT_32_ABGR);
	}
	#endif

	// pass : 96
	pass MSAA_NAME(pp_composite_highdof_blur_tiled)
    {
		// Don't put any render states here or you'll invalidate the "depthFXBlendState" state block!
        VertexShader = compile VERTEXSHADER VS_PassthroughCompositeHighDofTiled();
        PixelShader  = compile MSAA_PIXEL_SHADER PS_CompositeHighDOF()  CGC_FLAGS(CGC_POSTFXCOMPFLAGS(1));
    }

	// pass : 97
	pass MSAA_NAME(pp_composite_highdof_noblur_tiled)
    {
		// Don't put any render states here or you'll invalidate the "depthFXBlendState" state block!
        VertexShader = compile VERTEXSHADER VS_PassthroughCompositeHighDofTiled();
        PixelShader  = compile MSAA_PIXEL_SHADER PS_Composite_NoBlur()  CGC_FLAGS(CGC_POSTFXCOMPFLAGS(1));
    }

   // pass : 98
	pass MSAA_NAME(pp_subsampleAlphaUI)
	{
		// See render state blocks in PostProcessFX.cpp : subSampleAlphaDSState & subSampleAlphaWithStencilCullDSState
		VertexShader = compile VERTEXSHADER			VS_Passthrough();
        PixelShader  = compile MSAA_PIXEL_SHADER	PS_SubSampleAlphaUI()  CGC_FLAGS(CGC_DEFAULTFLAGS);
    }

	// pass : 99
	pass pp_lens_distortion
	{
		VertexShader = compile VERTEXSHADER			VS_Passthrough();
        PixelShader  = compile MSAA_PIXEL_SHADER	PS_LensDistortion()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass pp_lens_artefacts
	{
		AlphaBlendEnable = true;
		SrcBlend         = ONE;
		DestBlend        = ONE;
		BlendOp          = ADD;
        AlphaTestEnable = false;
  		ZEnable = false;
		ZWriteEnable = false;
        VertexShader = compile VERTEXSHADER VS_Passthrough();
        PixelShader  = compile PIXELSHADER PS_LensArtefacts()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass pp_lens_artefacts_combined
	{
		AlphaBlendEnable = true;
		SrcBlend         = ONE;
		DestBlend        = ONE;
		BlendOp          = ADD;
        AlphaTestEnable = false;
  		ZEnable = false;
		ZWriteEnable = false;
        VertexShader = compile VERTEXSHADER VS_Passthrough();
        PixelShader  = compile PIXELSHADER PS_LensArtefactsCombined()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass pp_light_streaks_blur_low
    {
        AlphaBlendEnable = false;
        AlphaTestEnable = false;
  		ZEnable = false;
		ZWriteEnable = false;
        VertexShader = compile VERTEXSHADER VS_Passthrough();
        PixelShader  = compile PIXELSHADER PS_LightStreaksBlurLow()  CGC_FLAGS(CGC_DEFAULTFLAGS);
    }

	pass pp_light_streaks_blur_med
    {
        AlphaBlendEnable = false;
        AlphaTestEnable = false;
  		ZEnable = false;
		ZWriteEnable = false;
        VertexShader = compile VERTEXSHADER VS_Passthrough();
        PixelShader  = compile PIXELSHADER PS_LightStreaksBlurMed()  CGC_FLAGS(CGC_DEFAULTFLAGS);
    }

	pass pp_light_streaks_blur_high
    {
        AlphaBlendEnable = false;
        AlphaTestEnable = false;
  		ZEnable = false;
		ZWriteEnable = false;
        VertexShader = compile VERTEXSHADER VS_Passthrough();
        PixelShader  = compile PIXELSHADER PS_LightStreaksBlurHigh()  CGC_FLAGS(CGC_DEFAULTFLAGS);
    }

	pass pp_blur_vignetting
	{
		AlphaBlendEnable = true;
		BlendOp         = Add;
		SrcBlend        = SrcAlpha;
		DestBlend       = InvSrcAlpha;
		VertexShader = compile VERTEXSHADER			VS_Passthrough();
        PixelShader  = compile PIXELSHADER			PS_BlurVignetting()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
	
	pass pp_blur_vignetting_tiled
	{
		AlphaBlendEnable = true;
		BlendOp         = Add;
		SrcBlend        = SrcAlpha;
		DestBlend       = InvSrcAlpha;
		VertexShader = compile VERTEXSHADER			VS_BlurVignettingTiled();
        PixelShader  = compile PIXELSHADER			PS_BlurVignetting()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass pp_blur_vignetting_blur_hor_tiled
	{
        AlphaBlendEnable = false;
        AlphaTestEnable = false;
  		ZEnable = false;
		ZWriteEnable = false;
        VertexShader = compile VERTEXSHADER VS_BlurVignettingTiled();
        PixelShader  = compile PIXELSHADER PS_GaussBlur9x9_H()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass pp_blur_vignetting_blur_ver_tiled
	{
        AlphaBlendEnable = false;
        AlphaTestEnable = false;
  		ZEnable = false;
		ZWriteEnable = false;
        VertexShader = compile VERTEXSHADER VS_BlurVignettingTiled();
        PixelShader  = compile PIXELSHADER PS_GaussBlur9x9_V()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
	
	pass pp_bloom_min
    {
    	AlphaBlendEnable = false;
		AlphaTestEnable = false;    
        VertexShader = compile VERTEXSHADER VS_Passthrough();
        PixelShader  = compile PIXELSHADER PS_Bloom_Min()  CGC_FLAGS(CGC_POSTFXCOMPFLAGS(1));
    }

	PASS pp_motion_blur_fpv
	{
		AlphaBlendEnable = false;
		AlphaTestEnable  = false;    

		/*
		StencilEnable = true;
		StencilFunc = EQUAL;
		ZEnable = false;
		ZWriteEnable = false;
		StencilPass = keep;
		StencilRef = DEFERRED_MATERIAL_MOTIONBLUR;
		*/

		VertexShader = compile VERTEXSHADER VS_Passthrough();
		PixelShader  = compile MSAA_PIXEL_SHADER PS_MotionBlurFPV()  CGC_FLAGS(CGC_POSTFXCOMPFLAGS(1));
	}

	PASS pp_motion_blur_fpv_ds
	{
		AlphaBlendEnable = false;
		AlphaTestEnable  = false;    
		
		VertexShader = compile VERTEXSHADER VS_Passthrough();
		PixelShader  = compile MSAA_PIXEL_SHADER PS_MotionBlurFPV_DS()  CGC_FLAGS(CGC_POSTFXCOMPFLAGS(1));
	}

	PASS pp_motion_blur_fpv_composite
	{
		AlphaBlendEnable = true;
		AlphaTestEnable  = false;    
		BlendOp         = Add;
		SrcBlend        = SrcAlpha;
		DestBlend       = InvSrcAlpha;

		VertexShader = compile VERTEXSHADER VS_Passthrough();
		PixelShader  = compile MSAA_PIXEL_SHADER PS_MotionBlurFPV_Composite()  CGC_FLAGS(CGC_POSTFXCOMPFLAGS(1));
	}
		
	pass pp_GaussBlurBilateral_Hor
	{
        AlphaBlendEnable = false;
        AlphaTestEnable = false;
  		ZEnable = false;
		ZWriteEnable = false;
        VertexShader = compile VERTEXSHADER VS_Passthrough();
        PixelShader  = compile PIXELSHADER PS_GaussBlur9x9Bilateral_H()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass pp_GaussBlurBilateral_Ver
	{
        AlphaBlendEnable = false;
        AlphaTestEnable = false;
  		ZEnable = false;
		ZWriteEnable = false;
        VertexShader = compile VERTEXSHADER VS_Passthrough();
        PixelShader  = compile PIXELSHADER PS_GaussBlur9x9Bilateral_V()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
	pass pp_GaussBlurBilateral_Hor_High
	{
		AlphaBlendEnable = false;
		AlphaTestEnable = false;
		ZEnable = false;
		ZWriteEnable = false;
		VertexShader = compile VERTEXSHADER VS_Passthrough();
		PixelShader  = compile PIXELSHADER PS_GaussBlur9x9Bilateral_H_High()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass pp_GaussBlurBilateral_Ver_High
	{
		AlphaBlendEnable = false;
		AlphaTestEnable = false;
		ZEnable = false;
		ZWriteEnable = false;
		VertexShader = compile VERTEXSHADER VS_Passthrough();
		PixelShader  = compile PIXELSHADER PS_GaussBlur9x9Bilateral_V_High()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
#if RSG_PC
	pass MSAA_NAME(pp_centerdist) // Pass through (don't do any postfx)
	{
    	AlphaBlendEnable = false;
		AlphaTestEnable = false;
		ZEnable = false;
		ZWriteEnable = false;
        VertexShader = compile VERTEXSHADER			VS_Passthrough();
        PixelShader  = compile MSAA_PIXEL_SHADER	PS_CenterDist()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
#endif
}

#if BOKEH_SUPPORT

struct BokehVSOutput
{
	DECLARE_POSITION(Position)
	float2 Size : SIZE;
	float4 Color : COLOR0;
	float Depth : DEPTH0;
};

struct BokehGSOutput
{
	float4 PositionCS : SV_Position;
	float2 TexCoord : TEXCOORD;
	float2 vPos : TEXCOORD2;
	float4 Color : COLOR0;
	float Depth : DEPTH0;
};

struct BokehPSInput
{
	float4 PositionSS : SV_Position;
	float2 TexCoord : TEXCOORD;
	float2 vPos : TEXCOORD2;
	float4 Color : COLOR0;
	float Depth : DEPTH0;
};

// Structure used for outputing bokeh points to an AppendStructuredBuffer
struct BokehPoint
{
#if RSG_PC
	float4 Position;
#else
	float3 Position;
#endif
	float Blur;
	float4 Color;
};

struct BokehSortedList
{
	uint index;
	float depth;
};

RWStructuredBuffer<BokehPoint> BokehPointBuffer									: register(u1);
RWStructuredBuffer<BokehSortedList> BokehSortedListBuffer						: register(u2);
RWStructuredBuffer<uint> BokehNumAddedToBuckets									: register(u3);

StructuredBuffer<BokehPoint> BokehSpritePointBuffer								: register(t0);
StructuredBuffer<BokehSortedList> BokehSortedIndexBuffer						: register(t2);

#if ADAPTIVE_DOF_OUTPUT_UAV
StructuredBuffer<AdaptiveDOFStruct> AdaptiveDOFParamsBuffer						: register(t1);
#endif //ADAPTIVE_DOF_OUTPUT_UAV

// Generates the buffer containing linear depth + blur factor
float2 PS_BokehDepthBlur(postfxFragmentInputPassThroughComposite IN) : COLOR
{
	float z = GBufferTexDepth2D(PostFxSampler2,IN.tex.xy).x;
	float depth = getLinearDepth( z, dofProj.zw );

#if PTFX_APPLY_DOF_TO_PARTICLES
	float particledepth = tex2D( PtfxDepthMapSampler, IN.tex.xy).r; //This is already linear depth
	float particleAlpha = saturate(tex2D( PtfxAlphaMapSampler, IN.tex.xy).r);

	float depthMod = lerp(depth, particledepth, particleAlpha);
	depth = depthMod;
#endif //PTFX_APPLY_DOF_TO_PARTICLES

	float f0 = 1.0f - saturate((depth - hiDofWorldNearStart) * hiDofWorldNearRangeRcp);
    float f1 = saturate((depth - hiDofWorldFarStart) * hiDofWorldFarRangeRcp);
	float blur = saturate(f0 + f1);

	if( f0 > 0.0 )
		blur = -f0;

	return float2(depth, blur);
}

#if ADAPTIVE_DOF_GPU

struct postfxVertexOutputDepthBlurAdaptive {
    DECLARE_POSITION(pos)
    float4 tex	: TEXCOORD0;
};

postfxVertexOutputDepthBlurAdaptive VS_BokehDepthBlurAdaptive(VIN)
{
	postfxVertexOutputDepthBlurAdaptive OUT = (postfxVertexOutputDepthBlurAdaptive)0 ;
	
	//scale here
#if POSTFX_UNIT_QUAD
	OUT.pos = float4( QuadTransform(pos * QuadScale,QuadPosition), 0, 1 );
	OUT.tex.xy = QuadTransform(pos,QuadTexCoords);
#else
	OUT.pos = float4(IN.pos, 1.0f);
    OUT.tex.xy = IN.texCoord0;
#endif

    return OUT;
}

// Generates the buffer containing linear depth + blur factor
float2 PS_BokehDepthBlurAdaptive(postfxVertexOutputDepthBlurAdaptive IN) : COLOR
{
	float z = GBufferTexDepth2D(PostFxSampler2,IN.tex.xy).x;
	float depth = getLinearDepth( z, dofProj.zw );

#if PTFX_APPLY_DOF_TO_PARTICLES
	float particledepth = tex2D( PtfxDepthMapSampler, IN.tex.xy).r; //This is already linear depth
	float particleAlpha = saturate(tex2D( PtfxAlphaMapSampler, IN.tex.xy).r);

	float depthMod = lerp(depth, particledepth, particleAlpha);
	depth = depthMod;
#endif //PTFX_APPLY_DOF_TO_PARTICLES

	float blur = 0.0f;
	if(depth < ADAPTIVE_DOF_SMALL_FLOAT)
	{
		blur = 1.0f;
	}
	else
	{
		float blurSampleRadius;

		float distanceRatio = (AdaptiveDOFParamsBuffer[0].distanceToSubject / depth);
		float isForeground = 1.0f;
		if(distanceRatio > 1.0f)
		{
			isForeground = -1.0f;
			blurSampleRadius = AdaptiveDOFParamsBuffer[0].maxBlurDiskRadiusNear * (distanceRatio - 1.0f);
			blurSampleRadius = pow(blurSampleRadius, AdaptiveDofBlurDiskRadiusPowerFactorNear);
		}
		else
		{
			blurSampleRadius = AdaptiveDOFParamsBuffer[0].maxBlurDiskRadiusFar * (1.0f - distanceRatio);
		}

//		//NOTE: We subtract one from the sample radius here to ensure that we don't apply visible blur until we are beyond the in-focus planes (rather than at or within them.) 
//		blurSampleRadius -= 1.0f;

		//Ensure that we respect the level of environmental blur required by the time-cycle at this depth.
		float minblurSampleRadiusForEnv = 0.0f;
		if(depth >= AdaptiveDofEnvBlurMaxFarOutOfFocusDistance)
		{
			minblurSampleRadiusForEnv = AdaptiveDofEnvBlurMinBlurRadius;
		}
		else if(depth >= AdaptiveDofEnvBlurMaxFarInFocusDistance)
		{
			float envBlurRange = AdaptiveDofEnvBlurMaxFarOutOfFocusDistance - AdaptiveDofEnvBlurMaxFarInFocusDistance;
			if(envBlurRange >= ADAPTIVE_DOF_SMALL_FLOAT)
			{
				minblurSampleRadiusForEnv = AdaptiveDofEnvBlurMinBlurRadius * (depth - AdaptiveDofEnvBlurMaxFarInFocusDistance) / envBlurRange;
			}
		}

		blurSampleRadius = max(blurSampleRadius, minblurSampleRadiusForEnv);

		//Blend in any custom (legacy) four-plane DOF settings.
		//NOTE: This was copied from PS_BokehDepthBlur above.
		float f0 = 1.0f - saturate((depth - hiDofWorldNearStart) * hiDofWorldNearRangeRcp);
		float f1 = saturate((depth - hiDofWorldFarStart) * hiDofWorldFarRangeRcp);
		bool isForegroundFourPlane = 1.0;
		if( f0 > 0.0 )
			isForegroundFourPlane = -1.0;

		float blurSampleRadiusForCustomPlanes = AdaptiveDofCustomPlanesBlurRadius * saturate(f0 + f1);

		blurSampleRadius = lerp(blurSampleRadius, blurSampleRadiusForCustomPlanes, AdaptiveDofCustomPlanesBlendLevel);
		isForeground = lerp(isForeground, isForegroundFourPlane, AdaptiveDofCustomPlanesBlendLevel);

		blur = blurSampleRadius / DOF_MaxSampleRadius;
		blur = min(blur, 1.0f);
		blur = lerp(blur, 1.0f, AdaptiveDofScreenBlurFadeLevel);
		if( isForeground < 0.0 && !(depth >= AdaptiveDofEnvBlurMaxFarInFocusDistance || AdaptiveDofScreenBlurFadeLevel > 0.0)  )
			blur *= -1;
	}

	return float2(depth, blur);
}
#endif // ADAPTIVE_DOF_GPU

float2 PS_BokehBlurDownsample(postfxVertexOutputDepthBlurAdaptive IN) : COLOR
{
	float2 downsampledDepthBlur = tex2D( BackBufferSampler, IN.tex.xy).rg;
	downsampledDepthBlur.y = downsampledDepthBlur.y < 0.0 ? -downsampledDepthBlur.y : 0.0;
	return downsampledDepthBlur;
}

// Extracts pixels for bokeh point generation
float4 PS_BokehGenerate(postfxFragmentInputPassThroughComposite IN) : COLOR
{
	float2 centerCoord = IN.tex;

	// Start with center sample color
	//JitterSampler containts backbuffer with point sampling - could maybe use texture objects to do this better.
	float3 centerColor = tex2D( JitterSampler, centerCoord.xy).rgb;
	float3 colorSum = centerColor;
	float totalContribution = 1.0f;

	// Sample the depth and blur at the center
	float2 centerDepthBlur = tex2D( GBufferTextureSamplerSSAODepth, centerCoord).xy;
	float centerDepth = centerDepthBlur.x;
	float centerBlur = abs(centerDepthBlur.y);

	float2 exposureRT = tex2D( CurLumSampler, centerCoord).rg;
	float exposure = exposureRT.g;

	const uint NumSamples = 4;
	const float2 SamplePoints[NumSamples] =
	{
		float2(-1.0f, -1.0f), float2(-1.0f, 1.0f),
		float2(1.0f,  -1.0f), float2(1.0f,  1.0f)
	};

	//Changed this so that it gets the minimum values rather than finding the single brightest sample
	//in the hope to reduce single pixel bright spots.
	float minBlur = centerBlur;
	float3 minColor = centerColor;
	[unroll]
	for(uint i = 0; i < NumSamples; ++i)
	{
		float2 sampleCoord = centerCoord + SamplePoints[i] / (gScreenSize * 0.5f);
		float2 sampleDepthBlur = tex2D( GBufferTextureSamplerSSAODepth, sampleCoord);
		float3 sampleColor = tex2D( JitterSampler, sampleCoord).rgb;

		minBlur = min(abs(sampleDepthBlur.y), minBlur);
	
		if( abs(sampleDepthBlur.x - centerDepth) < 0.1)
		{			
			minColor = min(sampleColor, minColor);
		}
	}

	float pow2Exposure = exposureRT.r;
	float3 unpackedColor = minColor;
	// pre-expose, divide by white point (range upper bound) and saturate
	const float3 linearColour = unpackedColor * pow2Exposure * ooWhitePoint;
	// compute normalised intensity
	float centerBrightness = max(1e-9, dot(linearColour, float3(0.3333333,0.3333333,0.3333333)));


	//calculate brightness threshold based on exposure
	exposure = (clamp(exposure, BokehBrightnessParams.y, BokehBrightnessParams.x) + abs(BokehBrightnessParams.y)) / (BokehBrightnessParams.x - BokehBrightnessParams.y);	

	float fadeRange = lerp(BokehParams1.x, BokehParams1.y, exposure);
	float brightnessThreshold = lerp(BokehBrightnessParams.w, BokehBrightnessParams.z, exposure);

	[branch]
	if(centerBrightness > fadeRange && minBlur > BokehParams1.z)
	{
		float alpha = saturate((centerBrightness - fadeRange) / (brightnessThreshold - fadeRange));
		alpha *= BokehGlobalAlpha;

		if( alpha > BokehAlphaCutoff)
		{
#if RSG_ORBIS
			uint counter = BokehPointBuffer.IncrementCount();
#else
			uint counter = BokehPointBuffer.IncrementCounter();
#endif
			BokehPoint bPoint;
			//Multiplied by 2 as going from half res to full res target
			
#ifdef NVSTEREO
			bPoint.Position.xyz = float3(IN.pos.xy * 2, centerDepth);
			// identify left/right eye and insert left/right id to each vertex
			float2 stereoParams = StereoParmsTexture.Load(int3(0,0,0)).xy;
			if (stereoParams.x < 0) // left
				bPoint.Position.w = -1.0f;
			else	// right
				bPoint.Position.w = 1.0f;
#else
#if RSG_PC
			bPoint.Position = float4(IN.pos.xy * 2, centerDepth, 1.0f);
#else
			bPoint.Position = float3(IN.pos.xy * 2, centerDepth);
#endif
#endif
			bPoint.Blur = minBlur;
			bPoint.Color.xyz = centerColor;
			bPoint.Color.w = alpha;
			BokehPointBuffer[counter] = bPoint;

#if BOKEH_SORT_BITONIC_TRANSPOSE
			uint sortListIndex = 0;
#else
			uint sortListIndex = counter / BOKEH_SORT_BITONIC_BLOCK_SIZE;
#endif
			uint currentTotalAdded;
			InterlockedAdd(BokehNumAddedToBuckets[sortListIndex], 1, currentTotalAdded);

			BokehSortedListBuffer[counter].index = counter;
			BokehSortedListBuffer[counter].depth = centerDepth;	
			}
	}

	return float4(0.0f.xxx, 1.0f);
}

BokehVSOutput VS_BokehSprites_impl(uint VertexID, float kernelRadius)
{
	BokehPoint bPoint = BokehSpritePointBuffer[BokehSortedIndexBuffer[VertexID].index];
	BokehVSOutput output;

	// Position the vertex in normalized device coordinate space [-1, 1]
	output.Position.xy = bPoint.Position.xy;
	output.Position.xy /= DOFTargetSize;
	output.Position.xy = output.Position.xy * 2.0f - 1.0f;
	output.Position.y *= -1.0f;
	output.Position.zw = 1.0.xx;

	// Scale the size based on the CoC size, and max bokeh sprite size
	output.Size = (bPoint.Blur * (1.0f / DOFTargetSize) * kernelRadius );
	output.Depth = bPoint.Position.z;
	
	float c = 0.0f;
#ifdef NVSTEREO
	float2 stereoParams = StereoParmsTexture.Load(int3(0,0,0)).xy;
	//output.Position.xy = output.Position.xy + float2(stereoParams.x,0.0f);
	if (((stereoParams.x < 0) && (bPoint.Position.w == -1.0f)) // left
		|| ((stereoParams.x > 0) && (bPoint.Position.w == 1.0f))) // right
	{
		c = bPoint.Color.w;
	}
#else
	c = bPoint.Color.w;
#endif
	output.Color = float4( bPoint.Color.xyz, c);
	
	return output;
}

BokehVSOutput VS_BokehSprites(uint VertexID	: SV_VertexID)
{
	return VS_BokehSprites_impl(VertexID, BokehParams1.w);
}

#if ADAPTIVE_DOF_OUTPUT_UAV
BokehVSOutput VS_BokehSprites_Adaptive(uint VertexID	: SV_VertexID)
{
	return VS_BokehSprites_impl(VertexID, (float)DOF_MaxSampleRadius * BokehParams1.w);
}
#endif //ADAPTIVE_DOF_OUTPUT_UAV

[maxvertexcount(4)]
void GS_BokehSprites(point BokehVSOutput input[1], inout TriangleStream<BokehGSOutput> SpriteStream)
{
	BokehGSOutput output;

	const float2 Offsets[4] =
	{
		float2(-1, 1),
		float2(1, 1),
		float2(-1, -1),
		float2(1, -1)
	};

	const float2 TexCoords[4] =
	{
		float2(0, 0),
		float2(1, 0),
		float2(0, 1),
		float2(1, 1)
	};

	// Emit 4 new verts, and 2 new triangles
	[unroll]
	for (int i = 0; i < 4; i++)
	{
		output.PositionCS = float4(input[0].Position.xy, 1.0f, 1.0f);
		output.PositionCS.xy += Offsets[i] * input[0].Size;
		output.vPos = (output.PositionCS.xy * float2( 0.5f, -0.5f ) + 0.5f) * RenderTargetSize; 
		output.TexCoord = TexCoords[i];
		output.Color = input[0].Color;
		output.Depth = input[0].Depth;

		SpriteStream.Append(output);
	}
	SpriteStream.RestartStrip();
}

//=================================================================================================
// Pixel Shader, applies the bokeh shape texture
//=================================================================================================
float4 PS_BokehSprites(BokehPSInput input) : SV_Target0
{
	// Sample the bokeh texture
	//float textureSample = BokehTexture.Sample(BokehSampler, input.TexCoord).x;
	
	float exposure = tex2D( CurLumSampler, input.TexCoord).g;
	//clamp exposure to min max range
	exposure = (clamp(exposure, BokehParams2.y, BokehParams2.x) + abs(BokehParams2.y)) / (BokehParams2.x - BokehParams2.y);	

	int numBokehSprites = 16;
	int textureID = lerp(numBokehSprites-1, 0, exposure);

#if !SHADER_FINAL
	if( BokehParams2.z != -1 )
		textureID = BokehParams2.z;
#endif

	float numSpritesX = 4;
	float numSpritesY = 4;
	float xSpriteSize = 1.0 / numSpritesX;
	float ySpriteSize = 1.0 / numSpritesY;
	float xVal = input.TexCoord.x * xSpriteSize;
	float yVal = input.TexCoord.y * ySpriteSize;

	int xOffs = textureID % numSpritesX;
	int yOffs = textureID / numSpritesY;

	float2 textureCoords = float2( xVal + (xSpriteSize * xOffs), yVal + (ySpriteSize * yOffs));
	float textureSample = tex2D( HeatHazeSampler, textureCoords).y;

//	float2 screenTexCoord = input.PositionSS.xy / RenderTargetSize;    // Don't Use SV_Position in Pixel Shaders, it's slower.
	float2 screenTexCoord = input.vPos.xy;
	//float2 depthBlurSample = DepthBlurTexture.Sample(DepthBlurSampler, screenTexCoord);
	float2 depthBlurSample = tex2D( HDRSampler, screenTexCoord );

	float alpha = input.Color.a;

#if !SHADER_FINAL
	if( BokehParams2.w == 1.0 )	//debug overlay
	{
		float3 debugColor = lerp(float3(10,10,0), float3(10,0,0), alpha);
		return float4(debugColor, 1.0 * textureSample);
	}
#endif

	alpha *= textureSample;

	return float4(input.Color.rgb * textureSample , alpha);
	
}

half4  PS_BokehDownsample(postfxFragmentInputPassThroughComposite IN) : COLOR  
{
	const uint NumSamples = 4;
	const float2 SamplePoints[NumSamples] =
	{
		float2(-1.0f, -1.0f), float2(1.0f,  -1.0f),
		float2(1.0f,  -1.0f), float2(1.0f,  1.0f)
	};

	float4 averageColor = 0.0f;
	float2 centerCoord = IN.tex;

	[unroll]
	for(uint i = 0; i < NumSamples; ++i)
	{
		float2 sampleCoord = centerCoord + SamplePoints[i] / gScreenSize;
		float4 sampleHDR = tex2D( HDRSampler, sampleCoord);
		averageColor += sampleHDR;
	}

	averageColor/= NumSamples;

	return averageColor;
}

groupshared BokehSortedList shared_data[BOKEH_SORT_BITONIC_BLOCK_SIZE];

[numthreads(BOKEH_SORT_BITONIC_BLOCK_SIZE, 1, 1)]
void CS_BokehBitonicSort(uint3 GroupID : SV_GroupID, uint3 DispatchThreadID : SV_DispatchThreadID,
					uint3 GroupThreadID : SV_GroupThreadID, uint GroupIndex : SV_GroupIndex)
{
#if BOKEH_SORT_BITONIC_TRANSPOSE
	uint bucketID = 0;
#else		
	uint bucketID = GroupID.x;
#endif
	uint bucketSize = BokehNumAddedToBuckets[bucketID];

	uint startLocation = 0;
	startLocation = bucketID * BOKEH_SORT_BITONIC_BLOCK_SIZE;

	//Save to shared memory
	uint currentIndex = BokehSortedListBuffer[startLocation + DispatchThreadID.x].index;
	float depthValue = BokehSortedListBuffer[startLocation + DispatchThreadID.x].depth;

	shared_data[GroupIndex].index = currentIndex;
	shared_data[GroupIndex].depth = depthValue;

	GroupMemoryBarrierWithGroupSync();
    
	uint sortLevel = (uint) BokehSortLevel;
	uint sortLevelMask = (uint) BokehSortLevelMask;

	// Sort the shared data
	for (uint j = sortLevel >> 1 ; j > 0 ; j >>= 1)
	{
		BokehSortedList result;
		if((shared_data[GroupIndex & ~j].depth >= shared_data[GroupIndex | j].depth) == (bool)(sortLevelMask & DispatchThreadID.x))
			result = shared_data[GroupIndex ^ j];
		else
			result = shared_data[GroupIndex];

		GroupMemoryBarrierWithGroupSync();
		shared_data[GroupIndex] = result;
		GroupMemoryBarrierWithGroupSync();
	}
    
	// Store shared data
#if !BOKEH_SORT_BITONIC_TRANSPOSE
	if( bucketSize != 0 )
#endif
	{
		BokehSortedListBuffer[startLocation + DispatchThreadID.x].index = shared_data[GroupIndex].index;
		BokehSortedListBuffer[startLocation + DispatchThreadID.x].depth = shared_data[GroupIndex].depth;
	}
}

#if BOKEH_SORT_BITONIC_TRANSPOSE
groupshared BokehSortedList transpose_shared_data[BOKEH_SORT_TRANSPOSE_BLOCK_SIZE * BOKEH_SORT_TRANSPOSE_BLOCK_SIZE];

[numthreads(BOKEH_SORT_TRANSPOSE_BLOCK_SIZE, BOKEH_SORT_TRANSPOSE_BLOCK_SIZE, 1)]
void CS_BokehBitonicSortMatrixTranspose( uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, 
										uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex )
{
	uint transposeMatWidth = (uint)BokehSortTransposeMatWidth;
	uint transposeMatHeight = (uint)BokehSortTransposeMatHeight;
    transpose_shared_data[GI] = BokehSortedIndexBuffer[DTid.y * transposeMatWidth + DTid.x];
    GroupMemoryBarrierWithGroupSync();
    uint2 XY = DTid.yx - GTid.yx + GTid.xy;
    BokehSortedListBuffer[XY.y * transposeMatHeight + XY.x] = transpose_shared_data[GTid.x * BOKEH_SORT_TRANSPOSE_BLOCK_SIZE + GTid.y];
}
#endif //BOKEH_SORT_BITONIC_TRANSPOSE

technique11 PostFx_Bokeh_sm50
{  
	pass pp_BokehDepthBlur_sm50
	{
		AlphaBlendEnable = false;
		AlphaTestEnable = false;

		VertexShader = compile VERTEXSHADER VS_PassthroughComposite();
		PixelShader  = compile ps_5_0 PS_BokehDepthBlur()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
#if ADAPTIVE_DOF_GPU
	pass pp_BokehDepthBlurAdaptive_sm50
	{
		AlphaBlendEnable = false;
		AlphaTestEnable = false;

		VertexShader = compile vs_5_0 VS_BokehDepthBlurAdaptive();
		PixelShader  = compile ps_5_0 PS_BokehDepthBlurAdaptive()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
#endif
	pass pp_BokehBlurDownsample_sm50
	{
		AlphaBlendEnable = false;
		AlphaTestEnable = false;

		VertexShader = compile VERTEXSHADER VS_PassthroughComposite();
		PixelShader  = compile ps_5_0 PS_BokehBlurDownsample()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass pp_BokehGenerate_sm50
	{
		VertexShader = compile VERTEXSHADER VS_PassthroughComposite();
		PixelShader  = compile ps_5_0 PS_BokehGenerate()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass pp_BokehSprites_sm50
	{
		VertexShader = compile VSGS_SHADER_50 VS_BokehSprites();
		SetGeometryShader(compileshader(gs_5_0, GS_BokehSprites()));
		PixelShader  = compile ps_5_0 PS_BokehSprites()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

#if ADAPTIVE_DOF_OUTPUT_UAV
	pass pp_BokehSprites_Adaptive_sm50
	{
		AlphaBlendEnable = false;
		AlphaTestEnable = false;

		VertexShader = compile VSGS_SHADER_50 VS_BokehSprites_Adaptive();
		SetGeometryShader(compileshader(gs_5_0, GS_BokehSprites()));
		PixelShader  = compile ps_5_0 PS_BokehSprites()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
#endif

	pass pp_BokehDownsample_sm50
	{
		AlphaBlendEnable = false;
		AlphaTestEnable = false;

		VertexShader = compile VERTEXSHADER VS_PassthroughComposite();
		PixelShader  = compile ps_5_0 PS_BokehDownsample()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass pp_BokehComputeBitonicSort_sm50
	{
		SetComputeShader(	compileshader( cs_5_0, CS_BokehBitonicSort() ));
	}

#if BOKEH_SORT_BITONIC_TRANSPOSE
	pass pp_BokehComputeBitonicSortTranspose_sm50
	{
		SetComputeShader(	compileshader( cs_5_0, CS_BokehBitonicSortMatrixTranspose() ));
	}
#endif //BOKEH_SORT_BITONIC_TRANSPOSE
	
}

#endif //BOKEH_SUPPORT
