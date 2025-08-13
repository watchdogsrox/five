#define SCALEFORM_SHADER

#include "../common.fxh"
#include "../PostFX/SeeThroughFX.fxh"

#include "../../vfx/vfx_shared.h"

#define MULTIHEAD_FADE (__WIN32PC)

#define USE_HQ_ANTIALIASING (__D3D11 || RSG_ORBIS || (!defined(SHADER_FINAL) && (__PS3||__XENON))) // Also defined in sprite2d.h

#if __PS3 || RSG_ORBIS || RSG_PC || RSG_DURANGO || __MAX
#define X360_UNROLL
#else
#define X360_UNROLL [unroll]
#endif

#if USE_HQ_ANTIALIASING

#if __XENON
	#define FXAA_360 0
	#define FXAA_PC  1
	#define FXAA_HLSL_3 (FXAA_PC)
	#if FXAA_PC
		#define FXAA_QUALITY__PRESET 10
	#else
		#define FXAA_QUALITY__PRESET 13
	#endif 
#elif __PS3
	#define FXAA_PS3 1
	#define FXAA_QUALITY__PRESET 15
#elif RSG_ORBIS || RSG_DURANGO
	#define FXAA_PC 1
	#define FXAA_HLSL_5 1
	#define FXAA_QUALITY__PRESET 39
#elif __D3D11
	#define FXAA_PC 1
	#define FXAA_HLSL_4 1
	#define FXAA_QUALITY__PRESET 39
#endif

	#define FXAA_EARLY_EXIT 1
	#define FXAA_CUSTOM_TUNING					(1)
	
	#define Bluriness (0.50f)					// Max=0.5 Min = 0.33 Step=0.01 Default=0.5         : Soft/Sharp, see fxaaConsoleRcpFrameOpt below
	#define QualitySubpix (0.317f)				// Max=1.0, Min=0.0 Step=0.01 Default=0.75          : Sub-pixel quality (only used on FXAA Quality), see fxaaQualitySubpix below.
	#define EdgeThreshold (0.333f)				// Max=0.333, Min=0.063 Step=0.003 Default 0.166    : Edge threshold (only used on FXAA Quality), see fxaaQualityEdgeThreshold below.
	#define EdgeThresholdMin (0.0833f)			// Max=0.0833, Min=0.0312 Step=0.001 Default=0.0833 : Min edge threshold (only used on FXAA Quality), see fxaaQualityEdgeThresholdMin below.
	#define ConsoleEdgeSharpness (8)			// Max=8, Min=2 Step=*2, Default=8                  : Console Edge Sharpness (only used on non-PS3 FXAA Console), see fxaaConsoleEdgeSharpness below.
	#define ConsoleEdgeThreshold (0.125f)		// Max=0.25, Min=0.125 Step=0.125 Default=0.125     : Console Edge Threshold (only used on FXAA Console), see fxaaConsoleEdgeThreshold below.
	#define ConsoleEdgeThresholdMin (0.05f)		// Max=0.08, Min=0.02 Step=0.01 Default=0.05        : Console Min Edge Threshold (only used on FXAA Console), see ConsoleEdgeThresholdMin below.
	#define FXAA_CONSOLE__PS3_EDGE_THRESHOLD 0.25

	#include "../PostFX/FXAA3.h"
#endif // USE_HQ_ANTIALIASING

// =============================================================================================== //
// VARIABLES
// =============================================================================================== //

// NOTE:- Arays of float2's don`t work properly as shader locals. On Orbis these arrays a dealt with as shader locals (DX platforms embed them in them shader exe - so they are ok).
IMMEDIATECONSTANT float4 poisson24[12] =
{
	float4(0.7729676f, -0.2221565f, 0.5282406f, -0.8028475f),
	float4(0.2629631f, -0.07523455f, 0.8237055f, -0.5561877f),
	float4(0.5696832f, 0.121086f, 0.3988842f, -0.4114171f),
	float4(0.4400159f, 0.4879902f, -0.03464016f, 0.176342f),
	float4(-0.1203233f, -0.5860488f, 0.2004656f, -0.6874188f),
	float4(-0.258614f, -0.1630804f, -0.6883209f, -0.6355559f),
	float4(-0.3450861f, -0.8480957f, -0.3628126f, 0.6144199f),
	float4(0.06174967f, 0.5069199f, 0.9773164f, 0.2086624f),
	float4(0.7894329f, 0.4902646f, -0.7023129f, -0.07146062f),
	float4(-0.4953414f, 0.233821f, 0.1793105f, 0.9625847f),
	float4(-0.9309751f, -0.327397f, -0.9129849f, 0.2416974f),
	float4(-0.2172657f, 0.9727083f, -0.6971195f, 0.5296693f),
};

BeginConstantBufferPagedDX10(im_cbuffer, b5)
float4 TexelSize : TexelSize;
float4 refMipBlurParams : RefMipBlurParams;
float4 GeneralParams0 = float4(1.0, 1.0, 1.0, 0.0);
float4 GeneralParams1 = float4(0.0, 0.0, 0.0, 0.0);
#define ClearColor	(GeneralParams0)
float g_fBilateralCoefficient : BilateralCoefficient = 0.0016f;
#if __PS3
float g_fBilateralEdgeThreshold : BilateralEdgeThreshold = 0.75f;
#else
float g_fBilateralEdgeThreshold : BilateralEdgeThreshold = 0.96f;
#endif

#if ENABLE_DISTANT_CARS
float DistantCarAlpha : DistantCarAlpha = 1.0f;
#if __PS3
float DistantCarNearClip : DistantCarNearClip = 300.0f;
#endif
#endif

float4 tonemapColorFilterParams0; // xyz: B/W weights z: filter blend value
float4 tonemapColorFilterParams1; // xyz: Color weights

#define tonemapColorFilter_BWWeights	tonemapColorFilterParams0.xyz
#define tonemapColorFilter_TintColor	tonemapColorFilterParams1.xyz
#define tonemapColorFilter_Blend		(tonemapColorFilterParams0.w)

#if __SHADERMODEL >= 40
float4 RenderTexMSAAParam : RenderTexMSAAParam;
#endif

float4 RenderPointMapINTParam : RenderPointMapINTParam;

EndConstantBufferDX10(im_cbuffer)

#define refMipBlurParams_MipIndex      (refMipBlurParams.x) // never used in PS_refMipBlurBlend
#define refMipBlurParams_BlurSize      (refMipBlurParams.y) // never used in PS_refMipBlurBlend
#define refMipBlurParams_BlurSizeScale (refMipBlurParams.z) // never used in PS_refMipBlurBlend
#define refMipBlurParams_TexelSizeY    (refMipBlurParams.w) // never used in PS_refMipBlurBlend

#define refMipBlurParams_Alpha         (refMipBlurParams.x) // only used in PS_refMipBlurBlend

// =============================================================================================== //
// SAMPLERS
// =============================================================================================== //

#if __PS3
// we don't want rage_diffuse_sampler.fxh to be included, we have our own DiffuseSampler below
#define RAGE_DIFFUSE_SAMPLER_H

BeginSampler(sampler2D,diffuseTexture,DiffuseSampler,DiffuseTex)
	string UIName="Diffuse Texture";
	string TCPTemplate="Diffuse";
	string TextureType="Diffuse";
ContinueSampler(sampler2D,diffuseTexture,DiffuseSampler,DiffuseTex)
	AddressU  = WRAP;
	AddressV  = WRAP;
	AddressW  = WRAP;
	MIPFILTER = LINEAR;
	MINFILTER = LINEAR;
	MAGFILTER = LINEAR;
EndSampler;
#endif

#define RAGE_DEPENDENCY_MODE
#include "../../../rage/base/src/grcore/rage_im.fx"

#ifdef DRAWSKINNED_TECHNIQUES
#undef DRAWSKINNED_TECHNIQUES
#define DRAWSKINNED_TECHNIQUES (1)
#endif

#include "../Megashader/megashader.fxh"

#define ENABLE_LAZYHALF __PS3
#define ENABLE_TEX2DOFFSET 0
#include "shader_utils.fxh"

// ----------------------------------------------------------------------------------------------- //
#if __PS3
// ----------------------------------------------------------------------------------------------- //

// Quincunx filter for PS3
BeginSampler(sampler2D,RenderQuincunxMap,RenderMapQuincunxSampler,RenderQuincunxMap)
ContinueSampler(sampler2D,RenderQuincunxMap,RenderMapQuincunxSampler,RenderQuincunxMap)
   MinFilter = QUINCUNX;
   MagFilter = QUINCUNX;
   MipFilter = NONE;
   AddressU  = CLAMP;
   AddressV  = CLAMP;
EndSampler;

// ----------------------------------------------------------------------------------------------- //

// Quincunx Alt. filter for PS3
BeginSampler(sampler2D,RenderQuincunxAltMap,RenderMapQuincunxAltSampler,RenderQuincunxAltMap)
ContinueSampler(sampler2D,RenderQuincunxAltMap,RenderMapQuincunxAltSampler,RenderQuincunxAltMap)
   MinFilter = QUINCUNX_ALT;
   MagFilter = QUINCUNX_ALT;
   MipFilter = NONE;
   AddressU  = CLAMP;
   AddressV  = CLAMP;
EndSampler;

// ----------------------------------------------------------------------------------------------- //

// gauss filter for the PS3
// those sampler states are only available on the PS3
BeginSampler(sampler2D,RenderGaussMap,RenderMapGaussSampler,RenderGaussMap)
ContinueSampler(sampler2D,RenderGaussMap,RenderMapGaussSampler,RenderGaussMap)
   MinFilter = GAUSSIAN;
   MagFilter = GAUSSIAN;
   MipFilter = NONE;
   AddressU  = CLAMP;
   AddressV  = CLAMP;
EndSampler;

// ----------------------------------------------------------------------------------------------- //
#endif //__PS3...
// ----------------------------------------------------------------------------------------------- //

// ----------------------------------------------------------------------------------------------- //

BeginSampler(sampler2D,NoiseTexture,NoiseSampler,NoiseTexture)
ContinueSampler(sampler2D,NoiseTexture,NoiseSampler,NoiseTexture)
   MinFilter = LINEAR;
   MagFilter = LINEAR;
   MipFilter = NONE;
   AddressU  = WRAP;
   AddressV  = WRAP;
EndSampler;


// ----------------------------------------------------------------------------------------------- //

BeginDX10Sampler(sampler, Texture2D<float>, LowResDepthPointMap,LowResDepthMapPointSampler,LowResDepthPointMap)
ContinueSampler(sampler, LowResDepthPointMap,LowResDepthMapPointSampler,LowResDepthPointMap)
   MinFilter = POINT;
   MagFilter = POINT;
   MipFilter = NONE;
   AddressU  = CLAMP;
   AddressV  = CLAMP;
EndSampler;

// ----------------------------------------------------------------------------------------------- //

BeginSampler(sampler2D,HiResDepthPointMap,HiResDepthMapPointSampler,HiResDepthPointMap)
ContinueSampler(sampler2D,HiResDepthPointMap,HiResDepthMapPointSampler,HiResDepthPointMap)
MinFilter = POINT;
MagFilter = POINT;
MipFilter = NONE;
AddressU  = CLAMP;
AddressV  = CLAMP;
EndSampler;


// ----------------------------------------------------------------------------------------------- //

// We need a point filter on all targets here...
// Automatic assignment of samplers to registers has trouble detecting whether
// a sampler is also used in a vertex program, so we need to help it out.
#pragma sampler 0 // used in vertex shader
BeginSampler(sampler2D,CoronasDepthMap,CoronasDepthMapSampler,CoronasDepthMap)
ContinueSampler(sampler2D,CoronasDepthMap,CoronasDepthMapSampler,CoronasDepthMap)
   MinFilter = POINT;
   MagFilter = POINT;
   MipFilter = NONE;
   AddressU  = CLAMP;
   AddressV  = CLAMP;
EndSampler;

// ----------------------------------------------------------------------------------------------- //

#if !__PS3
BeginSampler(sampler, DepthTexture, DepthSampler, DepthTexture)
ContinueSampler(sampler, DepthTexture, DepthSampler, DepthTexture)
	AddressU  = CLAMP;
	AddressV  = CLAMP;
	MIPFILTER = NONE;
	MINFILTER = LINEAR;
	MAGFILTER = LINEAR;
EndSampler;
#endif // !__PS3

// ----------------------------------------------------------------------------------------------- //

BeginSampler(sampler, DepthPointTexture, DepthPointSampler, DepthPointTexture)
ContinueSampler(sampler, DepthPointTexture, DepthPointSampler, DepthPointTexture)
	AddressU  = CLAMP;
	AddressV  = CLAMP;
	MIPFILTER = NONE;
	MINFILTER = POINT;
	MAGFILTER = POINT;
EndSampler;

// ----------------------------------------------------------------------------------------------- //

BeginSampler(samplerCUBE,RenderCubeMap, RenderCubeMapSampler, RenderCubeMap)
ContinueSampler(samplerCUBE, RenderCubeMap, RenderCubeMapSampler, RenderCubeMap)
AddressU  = CLAMP;        
AddressV  = CLAMP;
AddressW  = CLAMP;
MinFilter = LINEAR;
MagFilter = LINEAR;
MipFilter = LINEAR;
EndSampler;

// ----------------------------------------------------------------------------------------------- //

BeginSampler(sampler2D,RefMipBlurMap,RefMipBlurSampler,RefMipBlurMap)
ContinueSampler(sampler2D,RefMipBlurMap,RefMipBlurSampler,RefMipBlurMap)
MinFilter = LINEAR;
MagFilter = LINEAR;
MipFilter = LINEAR;
AddressU  = CLAMP;
AddressV  = CLAMP;
EndSampler;

BeginSampler(sampler2D,PostFxTextureV0,CurLumSampler,PostFxTextureV0)
ContinueSampler(sampler2D,PostFxTextureV0,CurLumSampler,PostFxTextureV0)
   MinFilter = POINT;
   MagFilter = POINT;
   MipFilter = NONE;
   AddressU  = CLAMP;
   AddressV  = CLAMP;
EndSampler;

BeginDX10Sampler(sampler, Texture2D<uint4>, RenderPointMapINT, RenderMapPointSamplerINT, RenderPointMapINT)
ContinueSampler(sampler, RenderPointMapINT, RenderMapPointSamplerINT, RenderPointMapINT)
   MinFilter = POINT;
   MagFilter = POINT;
   MipFilter = POINT;
   AddressU  = CLAMP;
   AddressV  = CLAMP;
EndSampler;

#if __SHADERMODEL >= 40
#if __PSSL
#define Texture2DMS			MS_Texture2D
#endif

BeginDX10Sampler(sampler, Texture2DArray, RenderTexArray, RenderTexArraySampler, RenderTexArray)
ContinueSampler(sampler, RenderTexArray, RenderTexArraySampler, RenderTexArray)
AddressU  = CLAMP;        
AddressV  = CLAMP;
AddressW  = CLAMP;
MinFilter = LINEAR;
MagFilter = LINEAR;
MipFilter = LINEAR;
EndSampler;

Texture2DMS<float4>	RenderTexMSAA;
Texture2DMS<uint4>	RenderTexMSAAINT;
Texture2D<uint>		RenderTexFmask;
#endif // __SHADERMODEL >= 40

// ----------------------------------------------------------------------------------------------- //

struct vertexCoronaIN {
	float3 pos			: POSITION;
	float4 diffuseV		: COLOR0;
	float2 uHDRMult		: TEXCOORD0;
	float3 midPos		: NORMAL;
#if RAGE_INSTANCED_TECH
	uint InstID : SV_InstanceID;
#endif
};

// ----------------------------------------------------------------------------------------------- //

struct vertexCoronaOUT {
	DECLARE_POSITION(pos)
	float2 texCoord		: TEXCOORD0;
	float3 color0		: TEXCOORD1;
};

// ----------------------------------------------------------------------------------------------- //

struct vertexColCorUnlitIN {
	float3 pos			: POSITION;
	float3 diffuse		: COLOR0;
	float2 texCoord0	: TEXCOORD0;
};

// ----------------------------------------------------------------------------------------------- //

struct vertexColCorUnlitOUT {
	DECLARE_POSITION(pos)
	float2 texCoord0	: TEXCOORD0;
	float3 color0		: TEXCOORD1;
};

struct pixelColCorUnlitIN {
	DECLARE_POSITION_PSIN(pos)
	float2 texCoord0	: TEXCOORD0;
	float3 color0		: TEXCOORD1;
};

// ----------------------------------------------------------------------------------------------- //

struct vertexInputLPT
{
	float3	pos			: POSITION;
	float4	tex			: TEXCOORD0;
};

// ----------------------------------------------------------------------------------------------- //

struct vertexOutputLPT
{
	DECLARE_POSITION(pos)	// input expected in viewport space 0 to 1
	float4	tex			: TEXCOORD0;
};

// ----------------------------------------------------------------------------------------------- //

struct vertexOutputLPT2
{
	DECLARE_POSITION(pos)	// input expected in viewport space 0 to 1
	float2	tex			: TEXCOORD0;
};

// ----------------------------------------------------------------------------------------------- //

struct vertexInputLP
{
	float3	pos : POSITION;
};

// ----------------------------------------------------------------------------------------------- //

struct vertexOutputLP
{
	DECLARE_POSITION(pos)
};

struct pixelInputLP
{
	DECLARE_POSITION_PSIN(pos)
};

// ----------------------------------------------------------------------------------------------- //

struct vertexInputDistantLights
{
#if __XENON
	int index				: INDEX;
#else
	float3 worldPos : POSITION;
	float4 col : COLOR0;
	float2 tex : TEXCOORD0;
#endif
};

// ----------------------------------------------------------------------------------------------- //

struct vertexOutputDistantLights
{
	half4 texAndCentrePos : TEXCOORD0;
	half3 col : TEXCOORD1;
	DECLARE_POSITION_CLIPPLANES(pos)
};

// ----------------------------------------------------------------------------------------------- //

#if ENABLE_DISTANT_CARS
struct vertexInputDistantCars
{
#if __XENON
	int index			: INDEX;
#elif __PS3
	float3 pos			: POSITION;
	float3 normal		: NORMAL;
	float  index		: TEXCOORD1;
#else
	float3 pos			: POSITION;
	float3 normal		: NORMAL;

	#if USE_DISTANT_LIGHTS_2
		float4 colour : COLOR0;
		float2 tex : TEXCOORD0;
		float3 carPosition : TEXCOORD1;
		float3 carDirection : TEXCOORD2;
		float4 instanceColour : COLOR1;
	#else
		float3 carPosition : TEXCOORD1;
		float3 carDirection : TEXCOORD2;
	#endif //USE_DISTANT_LIGHTS_2
#endif //__XENON
	
};

// ----------------------------------------------------------------------------------------------- //

struct vertexOutputDistantCars
{
	DECLARE_POSITION(pos)
	float4 colour : COLOR0;
	float3 normal : TEXCOORD0;
#if USE_DISTANT_LIGHTS_2
	float2 tex : TEXCOORD1;
	float alphaVal : TEXCOORD2;
#endif //USE_DISTANT_LIGHTS_2
};

// ----------------------------------------------------------------------------------------------- //
#endif //ENABLE_DISTANT_CARS

struct vertexOutputGameGlow
{
	DECLARE_POSITION(pos)
	half2 tex        : TEXCOORD0;
	float3 centerPos : TEXCOORD1;
	half3 col        : TEXCOORD2;
};

struct pixelInputGameGlow
{
	DECLARE_POSITION_PSIN(pos)
	half2 tex        : TEXCOORD0;
	float3 centerPos : TEXCOORD1;
	half3 col        : TEXCOORD2;
};

// ----------------------------------------------------------------------------------------------- //

#if GLINTS_ENABLED
struct GlintVSOutput
{
	DECLARE_POSITION(Position)
	float2 Size : SIZE;
	float4 Color : COLOR0;
	float Depth : DEPTH0;
};

struct GlintGSOutput
{
	float4 PositionCS : SV_Position;
	float2 TexCoord : TEXCOORD;
	float3 Color : COLOR0;
	float Depth : DEPTH0;
};

struct GlintPSInput
{
	float4 PositionSS : SV_Position;
	float2 TexCoord : TEXCOORD;
	float3 Color : COLOR0;
	float Depth : DEPTH0;
};

struct GlintPoint
{
	float4 Position;
	float3 Color;
};


AppendStructuredBuffer<GlintPoint> GlintPointBuffer : register(u1);
StructuredBuffer<GlintPoint> GlintSpritePointBuffer : GlintSpritePointBuffer : register(t0);

#endif //GLINTS_ENABLED

#if MULTIHEAD_FADE
//---	Radial fade PARAMETERS		---//

BeginConstantBufferDX10(RadialFadeVariables)
// center.x, center.y, hsize.x, hsize.y
float4 RadialFadeArea	= float4(0,0,1,1);
// time.left, N/A, time.accel, direction
float4 RadialFadeTime	= float4(0,1,1,0);
// wave.size, subwave.num, subwave.scale
float4 RadialWaveParam	= float4(5, 1, 0, 0);
EndConstantBufferDX10(RadialFadeVariables)

struct vertexOutputRadial
{
	DECLARE_POSITION(pos)
	float4	tex		: TEXCOORD0;
};

struct pixelInputRadial
{
	DECLARE_POSITION(pos)
	float4	tex		: TEXCOORD0;
};
#endif	//MULTIHEAD_FADE

// =============================================================================================== //
// UTILITY / HELPER FUNCTIONS
// =============================================================================================== //

float3 refMipBlurExtendReflection(vertexOutputLPT IN)
{
	// This can be further optimised by removing the normalise, will look "wrong" but will be faster by +-2 passes
	float4 centre = (IN.tex.x < 0.5) ? float4(0.5, 0.5, 1.5, 0.5) : float4(1.5, 0.5, 0.5, 0.5);

	float2 diff = (IN.tex.xy * float2(2.0, 1.0)) - centre.xy;
	
	const float dist = length(diff);
	diff /= dist;

	float4 centreAndOffset = float4(1.0, 1.0, diff);
	centreAndOffset *= (dist <= 0.5) ? float4(centre.xy, dist.xx) : float4(centre.zw, (0.5 - (dist - 0.5)).xx);

	return tex2Dlod(RenderMapPointSampler, float4((centreAndOffset.xy + centreAndOffset.zw) * float2(0.5, 1.0), 0.0, 0)).rgb;
}

// ----------------------------------------------------------------------------------------------- //

float3 parab2world(float2 p) // useful utility function .. might want to move to common.fxh
{
	const float2 v = float2(frac(p.x*2), p.y)*2 - 1; // [-1..1]
	const float s = (p.x < 0.5) ? 1 : -1;
	const float d = dot(v, v);
	return float3(-2*v, s*(d - 1))/(d + 1);
}

// Cheaper version of the above. The main difference is that here p.xy is already in the correct [-1,1] space and the sign (world z) is passed in as p.z. This does NOT return a unit vector.
float3 parab2worldFast(float3 p) 
{
	const float2 v = p.xy;
	const float s = p.z;
	const float d = dot(v, v);
	return float3(-2*v, s*(d - 1)); // No need to divide by (d + 1), we don't care about the magnitude of this vector
}

// Fixed size mip blur for the dual paraboloid reflection map
float4 refMipBlur_fixedsize(vertexOutputLPT IN, int mipLevel, int blurSize)
{
	// mipLevel=1 -> blurSize=1x1
	// mipLevel=2 -> blurSize=3x3
	// mipLevel=3 -> blurSize=5x5
	// mipLevel=4 -> blurSize=7x7

	float3 sampleC = 0;

	const float2 texelSize = float2(refMipBlurParams_TexelSizeY*0.5, refMipBlurParams_TexelSizeY);
	const float4 centre    = (IN.tex.x >= 0.5) ? float4(0.75, 0.5, 0.25, 0.5) : float4(0.25, 0.5, 0.75, 0.5);
	float2 cv = IN.tex.xy;

	const int numSamples = blurSize*blurSize;
	const float h = ((float)blurSize - 1)/2.0;

	for (int y = 0; y < blurSize; y++)
	{
		for (int x = 0; x < blurSize; x++)
		{
			const float2 tex0 = cv + float2(x - h, y - h)*texelSize;
			
			float2 v = tex0 - centre.xy;
			v *= float2(4, 2);
			float2 vNewCentre = centre.xy;
			
			// sample is outside circle, flip to other hemisphere
			if (dot(v, v) >= 1.0f) 
			{
				v /= dot (v, v);
				v.x = -v.x;
				vNewCentre = centre.zw;
			}
			v /= float2(4, 2);
			sampleC += tex2Dlod(RefMipBlurSampler, float4(vNewCentre + v, 0, (float)mipLevel - 1)).xyz;
		}
	}

	return float4(sampleC/(float)numSamples, 1);
}

//====================================================================
float4 refMipBlurFunc(vertexOutputLPT IN, int mipLevel, int blurSize)
{
	return refMipBlur_fixedsize( IN, mipLevel, blurSize );
}


float4 refMipBlurPoisson(vertexOutputLPT IN, int mipLevel, int blurSize)
{
	float3 sampleC = 0;

	const float2 texelSize = float2(refMipBlurParams_TexelSizeY*0.5, refMipBlurParams_TexelSizeY);
	const float2 centre    = float2((IN.tex.x >= 0.5) ? 0.75 : 0.25, 0.5);

	float2 cv;
	{
		float2 v = IN.tex.xy - centre;

		// if |v| > 1, clamp v to perimeter of circle
		v *= float2(4, 2);
		v /= max(1, sqrt(dot(v, v)));
		v /= float2(4, 2);

		cv = v + centre;
	}

	const int numSamples = 12;

	for (int i = 0; i < numSamples; i++)
	{
		const float2 tex0 = cv + poisson24[i].zw*texelSize*blurSize*refMipBlurParams_BlurSizeScale;

		float2 v = tex0 - centre;

		v *= float2(4, 2);

		if (dot(v, v) > 1) // sample is outside circle, flip to other hemisphere
		{
			v /= dot(v, v);
			v.x = -2-v.x;
		}

		v /= float2(4, 2);

		sampleC += tex2Dlod(DiffuseSampler, float4(frac(v + centre), 0, (float)mipLevel - 1)).xyz;
	}

	return float4(sampleC/(float)numSamples, 1);
}

OutHalf4Color PS_refMipBlur_1(vertexOutputLPT IN) { return CastOutHalf4Color(refMipBlurFunc(IN, (int)refMipBlurParams_MipIndex, 1)); }
OutHalf4Color PS_refMipBlur_2(vertexOutputLPT IN) { return CastOutHalf4Color(refMipBlurFunc(IN, (int)refMipBlurParams_MipIndex, 2)); }
OutHalf4Color PS_refMipBlur_3(vertexOutputLPT IN) { return CastOutHalf4Color(refMipBlurFunc(IN, (int)refMipBlurParams_MipIndex, 3)); }
OutHalf4Color PS_refMipBlur_4(vertexOutputLPT IN) { return CastOutHalf4Color(refMipBlurFunc(IN, (int)refMipBlurParams_MipIndex, 4)); }
OutHalf4Color PS_refMipBlur_5(vertexOutputLPT IN) { return CastOutHalf4Color(refMipBlurFunc(IN, (int)refMipBlurParams_MipIndex, 5)); }
OutHalf4Color PS_refMipBlur_6(vertexOutputLPT IN) { return CastOutHalf4Color(refMipBlurFunc(IN, (int)refMipBlurParams_MipIndex, 6)); }
OutHalf4Color PS_refMipBlur_7(vertexOutputLPT IN) { return CastOutHalf4Color(refMipBlurFunc(IN, (int)refMipBlurParams_MipIndex, 7)); }
OutHalf4Color PS_refMipBlur_N(vertexOutputLPT IN) { return CastOutHalf4Color(refMipBlurFunc(IN, (int)refMipBlurParams_MipIndex, (int)refMipBlurParams_BlurSize)); }
OutHalf4Color PS_refMipBlur_P(vertexOutputLPT IN) { return CastOutHalf4Color(refMipBlurPoisson(IN, (int)refMipBlurParams_MipIndex, (int)refMipBlurParams_BlurSize)); }

// -------------------------------------------------------------
// Helper to help keep platform dependant depth sampling out of the shader code
// -------------------------------------------------------------
float SampleDepth ( sampler2D smpDepth, float2 vTexCoord )
{
	float fDepth;
	
	#if __PS3
		fDepth = texDepth2D(smpDepth, vTexCoord).x;
	#elif __XENON
		fDepth = f1tex2D(smpDepth, vTexCoord ).x;
	#else
		fDepth = fixupGBufferDepth(f1tex2D(smpDepth, vTexCoord).x);
	#endif
	
	return fDepth;
}

// =============================================================================================== //
// TECHNIQUE FUNCTIONS
// =============================================================================================== //

rageVertexBlit VS_GTABlit(rageVertexBlitIn IN)
{
	rageVertexBlit OUT;

	OUT.pos			= mul(float4(IN.pos.xyz, 1.0f), gWorldViewProj);
#ifdef NVSTEREO
	if (gStereorizeHud)
	{
		float2 stereoParms = StereoParmsTexture.Load(int3(0,0,0)).xy;
		OUT.pos.x += stereoParms.x * OUT.pos.w;
	}
#endif
	OUT.diffuse		= IN.diffuse;
	OUT.texCoord0	= IN.texCoord0;

	return(OUT);
}

// ----------------------------------------------------------------------------------------------- //

#if RSG_ORBIS
OutHalf4Color PS_BlitXZSwizzle(rageVertexBlit IN) {
	float4 texel = tex2Dlod(DiffuseSampler, float4(IN.texCoord0, 0.0, GeneralParams1.x));
	return CastOutHalf4Color(half4(texel * IN.diffuse * GeneralParams0).zyxw);
}
#endif

// ----------------------------------------------------------------------------------------------- //

OutHalf4Color PS_BlitGamma(rageVertexBlit IN) {
	float4 texel = UnpackHdr(tex2Dlod(DiffuseSampler, float4(IN.texCoord0, 0.0, GeneralParams1.x)));
	texel = PackColor(texel * IN.diffuse * GeneralParams0);
	return CastOutHalf4Color(half4(sqrt(texel.rgb), texel.a));
}

OutHalf4Color PS_BlitPow(rageVertexBlit IN)
{
	float3 texel = tex2D(DiffuseSampler, IN.texCoord0).rgb * GeneralParams0.xxx;
	return CastOutHalf4Color(half4(pow(texel, GeneralParams1.x), 1.0f));
}

// ----------------------------------------------------------------------------------------------- //

OutHalf4Color PS_BlitCalibration(rageVertexBlit IN)
{
	float4 texel = tex2D(DiffuseSampler, float2(IN.texCoord0.x, 1.0 - IN.texCoord0.y));
	float3 linearColour = pow(texel.rgb, 2.2f);
	float3 gammaColour = pow(linearColour, (1.0f / GeneralParams0.x));

	return CastOutHalf4Color(half4(gammaColour, texel.a > 0.0f));
}

// ----------------------------------------------------------------------------------------------- //


OutHalf4Color PS_BlitShowAlpha(rageVertexBlit IN)
{
	float4 texel = tex2D(RenderMapPointSampler, IN.texCoord0);

	float aspect = 1280.0f / 720.0f;

	float tileX = frac(IN.texCoord0.x * 32.0 * aspect);
	float tileY = frac(IN.texCoord0.y * 32.0);

	float check = ((tileX < 0.5 && tileY > 0.5) || 
			       (tileX > 0.5 && tileY < 0.5)) ? 0.1 : 0.2;

	float3 col = (texel.rgb * IN.diffuse.rgb);
	col = texel.a; 

	return CastOutHalf4Color(half4(col * GeneralParams0.xyz, IN.diffuse.a));
}

// ----------------------------------------------------------------------------------------------- //

OutHalf4Color PS_BlitNoBilinear(rageVertexBlit IN)
{
	float4 texel = tex2Dlod(RenderMapPointSampler, float4(IN.texCoord0.xy, 0.0, GeneralParams1.x));
	return CastOutHalf4Color(half4(texel * IN.diffuse * GeneralParams0));
}

// ----------------------------------------------------------------------------------------------- //

OutHalf4Color PS_BlitCube(rageVertexBlit IN)
{
	float r0 = IN.texCoord0.x - 0.5f;
	float r1 = 1.0 - IN.texCoord0.y;

	float theta = 2.0f * PI * r0;
	float a = sqrt(r1 * (1.0f - r1));

	float x = 2.0 * cos(theta) * a;
	float y = 2.0 * sin(theta) * a;
	float z = (1.0 - 2.0 * r1);

	float3 dir = float3(x, y, z);

	float4 texel = texCUBElod(RenderCubeMapSampler, float4(dir, GeneralParams1.x));
	
	return CastOutHalf4Color(half4(texel * IN.diffuse * GeneralParams0));
}

#if __SHADERMODEL >= 40
OutHalf4Color PS_BlitTexArray(rageVertexBlit IN)
{
	float3 texel = RenderTexArray.SampleLevel(RenderTexArraySampler,float3(IN.texCoord0.xy,GeneralParams1.w),0).xyz;
	return CastOutHalf4Color(half4((texel.xyz) * IN.diffuse * GeneralParams0, 1.0f));
}

OutHalf4Color PS_BlitMsaa(rageVertexBlit IN)
{
	int2 texCoord = int2(IN.texCoord0.xy * RenderTexMSAAParam.xy);
	float3 texel = RenderTexMSAA.Load(texCoord, GeneralParams1.x).xyz;
	return CastOutHalf4Color(half4((texel.xyz) * IN.diffuse * GeneralParams0, 1.0f));
}

OutHalf4Color PS_BlitMsaaDepth(rageVertexBlit IN)
{
	float depth = RenderTexMSAA.Load(int2(IN.pos.xy),0).x;

	if (any(GeneralParams1.zw != 0))
	{
		const float depthMin = GeneralParams1.x;
		const float depthMax = GeneralParams1.y;

		depth = (getLinearDepth(depth, GeneralParams1.zw) - depthMin)/(depthMax - depthMin);
	}

	return CastOutHalf4Color(GeneralParams0*depth);
}

OutFloat4Color PS_BlitMsaaStencil(rageVertexBlit IN)
{
#if RSG_DURANGO || RSG_ORBIS
	float stencil = RenderTexMSAAINT.Load(int2(IN.pos.xy),0).r;
#else
	float stencil = RenderTexMSAAINT.Load(int2(IN.pos.xy),0).g;
#endif

	return CastOutFloat4Color(GeneralParams0*stencil);
}

OutHalf4Color PS_BlitFmask(rageVertexBlit IN)
{
	int2 tc = int2(IN.texCoord0.xy * RenderTexMSAAParam.xy);
	//int2 tc = int2( IN.pos.xy );
	
	uint fmask = RenderTexFmask.Load( int3(tc,0) );
	uint4 params = uint4( GeneralParams1 );
	uint numSamples		= params.x;
	uint numFragments	= params.y;
	uint shift			= params.z;
	uint debugId = params.w & 0xFF;
	uint mode = params.w >> 8;
	const uint sampleMask = (1<<shift) - 1;
	float4 col = float4(0,0,0,0);
	OutHalf4Color invalidColor = CastOutHalf4Color(float4(1,0,1,0));

	if (mode == 1)	// show sample Id
	{
		if (debugId >= numFragments)
			return invalidColor;
		uint fragId = (fmask >> (shift*debugId)) & sampleMask;
		if (fragId >= numFragments)
			return invalidColor;
		col = RenderTexMSAA.Load(tc, fragId);
	}else
	if (mode == 2)	// show fragment Id
	{
		if (debugId >= numFragments)
			return invalidColor;
		col = RenderTexMSAA.Load(tc, debugId);
	}
	else
	if (mode == 3)	// show FMASK
	{
		col = ((fmask >> (shift*uint4(0,1,2,3))) & sampleMask) / (float)numFragments;
	}
	else
	if (mode == 4)	// resolve EQAA
	{
		float numValidSamples = 0.0f;
		for (uint i=0; i<numSamples; ++i)
		{
			uint fragId = (fmask >> (shift*i)) & sampleMask;
			if (fragId < numFragments)	{
				numValidSamples += 1.0f;
				col += RenderTexMSAA.Load(tc, fragId);
			}
		}
		col /= numValidSamples;
	}

	return CastOutHalf4Color(GeneralParams0*col);
}
#endif // __SHADERMODEL >= 40

half4 tex2DOffsetXenonOnly(sampler2D Sampler, float2 tex, float2 offset)
{
	float x = offset.x;
	float y = offset.y;
#if __XENON
	float4 color;
	asm{tfetch2D color, tex, Sampler, OffsetX = x, OffsetY = y};
	return color;
#else
	return h4tex2D(Sampler, tex);
#endif
}

struct VS_OUTPUTAA
{
	DECLARE_POSITION(pos)
	float2 tex	: TEXCOORD0;
	float4 nenw	: TEXCOORD1;
	float4 swse	: TEXCOORD2;
};

VS_OUTPUTAA VS_BlitAA(rageVertexBlitIn IN)
{
	VS_OUTPUTAA OUT;
	OUT.pos			= float4( IN.pos.xyz, 1.0f);
	float2 tex		= IN.texCoord0.xy;

	float2 texelSize = TexelSize.xy;

#if __XENON
	float2 offset	= texelSize*0.5;
	OUT.tex			= tex + texelSize*0.5;
#else
	float2 offset	= 0;
	OUT.tex			= tex;
#endif

	float scale	= .5;

	OUT.nenw	= float4(tex.xyxy + float4( texelSize.x,  texelSize.y, -texelSize.x,  texelSize.y)*scale + offset.xyxy);
	OUT.swse	= float4(tex.xyxy + float4(-texelSize.x, -texelSize.y,  texelSize.x, -texelSize.y)*scale + offset.xyxy);

	return(OUT);
}

OutFloat4Color PS_BlitAA(VS_OUTPUTAA IN)
{
	float2 texelSize	= TexelSize.xy;
	float2 tex			= IN.tex;

	float ne	= tex2D(DiffuseSampler, IN.nenw.xy).g;
	float nw	= tex2D(DiffuseSampler, IN.nenw.zw).g;
	float sw	= tex2D(DiffuseSampler, IN.swse.xy).g;
	float se	= tex2D(DiffuseSampler, IN.swse.zw).g;

	float str	= 3;
	float2 bump = float2(ne - sw, nw - se);
	float2 dir	= float2(-1, 1)*bump.x + float2(-1, -1)*bump.y;
	float len	= pow(saturate(length(dir)*str), 4);
	float2 dxdy = normalize(dir)*len*texelSize;

	float4 color = 0;
	//color += tex2D(DiffuseSampler, tex - 3.5*dxdy);
	color += tex2D(DiffuseSampler, tex - 1.5*dxdy);
	color += tex2D(DiffuseSampler, tex)*0.5;
	color += tex2D(DiffuseSampler, tex + 1.5*dxdy);
	//color += tex2D(DiffuseSampler, tex + 3.5*dxdy);
	return CastOutFloat4Color(color*0.4*GeneralParams0);
}

OutFloat4Color PS_BlitAAAlpha(VS_OUTPUTAA IN)
{
	float2 texelSize	= TexelSize.xy;
	float2 tex			= IN.tex;

	float2 ne	= tex2D(DiffuseSampler, IN.nenw.xy).ga;
	float2 nw	= tex2D(DiffuseSampler, IN.nenw.zw).ga;
	float2 sw	= tex2D(DiffuseSampler, IN.swse.xy).ga;
	float2 se	= tex2D(DiffuseSampler, IN.swse.zw).ga;
	ne.x		= dot(ne, float2(.75, .25));
	nw.x		= dot(nw, float2(.75, .25));
	sw.x		= dot(sw, float2(.75, .25));
	se.x		= dot(se, float2(.75, .25));

	float str	= 3;
	float2 bump = float2(ne.x - sw.x, nw.x - se.x);
	float2 dir	= float2(-1, 1)*bump.x + float2(-1, -1)*bump.y;
	float len	= pow(saturate(length(dir)*str), 4);
	float2 dxdy = normalize(dir)*len*texelSize;

	float4 color = 0;
	//color		+= tex2D(DiffuseSampler, tex - 3.5*dxdy);
	color		+= tex2D(DiffuseSampler, tex - 1.5*dxdy)*0.5;
	color		+= tex2D(DiffuseSampler, tex)*0.5;
	color		+= tex2D(DiffuseSampler, tex + 1.5*dxdy)*0.5;
	//color		+= tex2D(DiffuseSampler, tex + 3.5*dxdy);
	return CastOutFloat4Color(color*0.4*GeneralParams0);
}

OutHalf4Color PS_BlitColour(rageVertexBlit IN)
{
	return CastOutHalf4Color(GeneralParams0);
}

OutHalf4Color PS_BlitToAlpha(rageVertexBlit IN) 
{
	half4 texel = h4tex2D(DiffuseSampler, IN.texCoord0.xy).rgba;;
	half alpha = dot(texel,GeneralParams0);
	half result = lerp(GeneralParams1.x,GeneralParams1.y,alpha);
	return CastOutHalf4Color(half4(result.xxxx * IN.diffuse));
}

OutHalf4Color PS_BlitToAlphaBlur(rageVertexBlit IN) 
{
	half4 A = h4tex2D( DiffuseSampler, IN.texCoord0.xy + (float2(-0.5f,-0.5f)	) * TexelSize.xy ).rgba;
	half4 B = h4tex2D( DiffuseSampler, IN.texCoord0.xy + (float2(0.5f,-0.5f)	) * TexelSize.xy ).rgba;
	half4 C = h4tex2D( DiffuseSampler, IN.texCoord0.xy + (float2(-0.5f,0.5f)	) * TexelSize.xy ).rgba;
	half4 D = h4tex2D( DiffuseSampler, IN.texCoord0.xy + (float2(0.5f,0.5f)		) * TexelSize.xy ).rgba;
	half4 E = h4tex2D( DiffuseSampler, IN.texCoord0.xy + (float2(0.0f,0.0f)		) * TexelSize.xy ).rgba;
	half4 texel = half4((A+B+C+D+E)/5.0f);

	half alpha = dot(texel,GeneralParams0);
	half result = lerp(GeneralParams1.x,GeneralParams1.y,alpha);
	return CastOutHalf4Color(half4(result.xxxx * IN.diffuse));
}


OutHalf4Color PS_BlitDepthToAlpha(rageVertexBlit IN) 
{
	float4 texel = tex2Dlod(DiffuseSampler, float4(IN.texCoord0, 0.0, GeneralParams1.x));
	float depth = SampleDepth(DepthMapPointSampler, IN.texCoord0);
	
	return CastOutHalf4Color(half4( (texel * IN.diffuse * GeneralParams0).xyz, depth == 1.0 ? 0.0 : IN.diffuse.a ));
}

OutFloatColor PS_FOWCount(rageVertexBlit IN): COLOR
{
#if __XENON
	float2 offset	= TexelSize.zw*0.5f;
#else
	float2 offset	= 0.0f;
#endif
	float2 texCoord = IN.texCoord0.xy + offset;

	half A = h4tex2D( RenderMapPointSampler, texCoord + (float2(-0.5f,-0.5f)	) * TexelSize.xy ).r > 0.5f;
	half B = h4tex2D( RenderMapPointSampler, texCoord + (float2( 0.5f,-0.5f)	) * TexelSize.xy ).r > 0.5f;
	half C = h4tex2D( RenderMapPointSampler, texCoord + (float2(-0.5f, 0.5f)	) * TexelSize.xy ).r > 0.5f;
	half D = h4tex2D( RenderMapPointSampler, texCoord + (float2( 0.5f, 0.5f)	) * TexelSize.xy ).r > 0.5f;

	return CastOutFloatColor(A+B+C+D);
}

OutFloatColor PS_FOWAverage(rageVertexBlit IN): COLOR
{
#if __XENON
	float2 offset	= TexelSize.zw*0.5f;
#else
	float2 offset	= 0.0f;
#endif
	float2 texCoord = IN.texCoord0.xy + offset;

	half A =	h4tex2D( RenderMapPointSampler, texCoord + (float2(1.5f , -1.5f)	) * TexelSize.xy ).r;
	half B =	h4tex2D( RenderMapPointSampler, texCoord + (float2(1.5f , -0.5f)	) * TexelSize.xy ).r;
	half C =	h4tex2D( RenderMapPointSampler, texCoord + (float2(1.5f ,  0.5f)	) * TexelSize.xy ).r;
	half D =	h4tex2D( RenderMapPointSampler, texCoord + (float2(1.5f ,  1.5f)	) * TexelSize.xy ).r;
	A +=		h4tex2D( RenderMapPointSampler, texCoord + (float2(0.5f , -1.5f)	) * TexelSize.xy ).r;
	B +=		h4tex2D( RenderMapPointSampler, texCoord + (float2(0.5f , -0.5f)	) * TexelSize.xy ).r;
	C +=		h4tex2D( RenderMapPointSampler, texCoord + (float2(0.5f ,  0.5f)	) * TexelSize.xy ).r;
	D +=		h4tex2D( RenderMapPointSampler, texCoord + (float2(0.5f ,  1.5f)	) * TexelSize.xy ).r;
	A +=		h4tex2D( RenderMapPointSampler, texCoord + (float2(-0.5f ,-1.5f)	) * TexelSize.xy ).r;
	B +=		h4tex2D( RenderMapPointSampler, texCoord + (float2(-0.5f ,-0.5f)	) * TexelSize.xy ).r;
	C +=		h4tex2D( RenderMapPointSampler, texCoord + (float2(-0.5f , 0.5f)	) * TexelSize.xy ).r;
	D +=		h4tex2D( RenderMapPointSampler, texCoord + (float2(-0.5f , 1.5f)	) * TexelSize.xy ).r;
	A +=		h4tex2D( RenderMapPointSampler, texCoord + (float2(-1.5f ,-1.5f)	) * TexelSize.xy ).r;
	B +=		h4tex2D( RenderMapPointSampler, texCoord + (float2(-1.5f ,-0.5f)	) * TexelSize.xy ).r;
	C +=		h4tex2D( RenderMapPointSampler, texCoord + (float2(-1.5f , 0.5f)	) * TexelSize.xy ).r;
	D +=		h4tex2D( RenderMapPointSampler, texCoord + (float2(-1.5f , 1.5f)	) * TexelSize.xy ).r;

	return CastOutFloatColor(A+B+C+D);
}

OutFloatColor PS_FOWFill(rageVertexBlit IN): COLOR
{
	return CastOutFloatColor(IN.diffuse.x);
}

struct VS_OUTPUTPARABOLOID 
{
	DECLARE_POSITION(pos)
	float4 view	: TEXCOORD0;
};

VS_OUTPUTPARABOLOID  VS_BlitParaboloid(rageVertexBlitIn IN)
{
	VS_OUTPUTPARABOLOID  OUT;
	OUT.pos		= float4(IN.pos.xyz, 1.0f);
	OUT.view	= float4(mul(float4(OUT.pos.xy*GeneralParams1.xy, -1, 0), gViewInverse));
	return(OUT);
}

OutHalf4Color PS_BlitParaboloid(VS_OUTPUTPARABOLOID IN)
{
	return CastOutHalf4Color(h4tex2D(DiffuseSampler, DualParaboloidTexCoord_2h(normalize(IN.view.xyz), 1)));
}

// ----------------------------------------------------------------------------------------------- //

OutFloat4Color PS_BlitDepthToColor(rageVertexBlit IN)
{
	float depth = tex2D(RenderMapPointSampler,IN.texCoord0).x;

	if (any(GeneralParams1.zw != 0))
	{
		const float depthMin = GeneralParams1.x;
		const float depthMax = GeneralParams1.y;

		depth = (getLinearGBufferDepth(depth, GeneralParams1.zw) - depthMin)/(depthMax - depthMin);
	}

	return CastOutFloat4Color(GeneralParams0*depth);
}

OutFloat4Color PS_BlitStencilToColor(rageVertexBlit IN)
{
	float stencil = 0.0f;
#if __SHADERMODEL >= 40	
	int2 texCoord = int2(IN.texCoord0.xy * RenderPointMapINTParam.xy);

#if RSG_DURANGO || RSG_ORBIS
	stencil = RenderPointMapINT.Load(int3(texCoord,0)).r;
#else
	stencil = RenderPointMapINT.Load(int3(texCoord,0)).g;
#endif
#elif __PS3
	stencil = tex2D(DepthMapPointSampler, IN.texCoord0.xy).w;
#elif __XENON
	stencil = tex2D(DepthMapPointSampler, IN.texCoord0.xy).z;
#endif

	return CastOutFloat4Color(GeneralParams0*stencil);
}

// ----------------------------------------------------------------------------------------------- //

OutHalf4Color PS_AlphaStencil(rageVertexBlit IN)
{
	float4 depth = tex2D(DepthMapPointSampler, IN.texCoord0);
	float4 texel = tex2D(DiffuseSampler, IN.texCoord0);
	half4 color = half4(texel.rgb,(depth.w > 0.0f) ? 1.0f : texel.a);
	return CastOutHalf4Color(color);
}

// ----------------------------------------------------------------------------------------------- //

OutHalf4Color PS_BlitDiffuse(rageVertexBlit IN) {
	return CastOutHalf4Color(half4(IN.diffuse));
}


// ----------------------------------------------------------------------------------------------- //

// -------------------------------------------------------------
// 2x2 Down sample depth by picking MIN of the four samples
// -------------------------------------------------------------
#if __SHADERMODEL >= 40
void PS_DownsampleMinDepth(
#else
OutFloat4Color PS_DownsampleMinDepth(
#endif
	rageVertexBlit IN,
	out float depth : DEPTH
)
{
	// The tex coords are coming in wihtout the proper 1/2 texel offset for sampling at texel centers.
#if __XENON
	IN.texCoord0 += gooScreenSize.xy;// * 0.5f;
#endif

	float4 offsetTexCoords = float4(-0.5f, -0.5f, 0.5f, -0.5f) * gooScreenSize.xyxy;
	float4 vTexCoords1 = IN.texCoord0.xyxy + offsetTexCoords;
	float4 vTexCoords2 = IN.texCoord0.xyxy - offsetTexCoords;
	float4 depths;
#if __PS3	
	depths = float4(texDepth2D(DepthMapPointSampler, vTexCoords1.xy),
					texDepth2D(DepthMapPointSampler, vTexCoords1.zw),
					texDepth2D(DepthMapPointSampler, vTexCoords2.xy),
					texDepth2D(DepthMapPointSampler, vTexCoords2.zw));	
#else
	depths = float4(f1tex2D(DepthMapPointSampler, vTexCoords1.xy),
					f1tex2D(DepthMapPointSampler, vTexCoords1.zw),
					f1tex2D(DepthMapPointSampler, vTexCoords2.xy),
					f1tex2D(DepthMapPointSampler, vTexCoords2.zw));	
#endif	

#if SUPPORT_INVERTED_PROJECTION
	depth = min(min(depths.x, depths.y), min(depths.z, depths.w));	
#else
	depth = max(max(depths.x, depths.y), max(depths.z, depths.w));	
#endif

#if __SHADERMODEL < 40
	return CastOutFloat4Color(float4(1,0,0,1)); // Whatever you want here
#endif
}

// ----------------------------------------------------------------------------------------------- //

// ---------------------------------------------------------------------
// Compute bilinear sample points and weights for doing up-sampling from
// a 1/2 x 1/2 resolution texture up to a full resolution texture.
// ---------------------------------------------------------------------
struct BilinearPoints
{
	float2 vCenterCoord; // Sample points will be 0.5 a texel in the 4 diagonal directions from this point
	float4 vWeights;     // <Top left, top right, bottom left, bottom right>
};

// vDimensions of low resolution texture: <width, height, 1/width, 1/height>
BilinearPoints ComputeBilinearUpsamplingPoints ( float4 vDimensions, float2 vUV )
{
	// email Anoop Thomas if you find this is broken.

	// Snap to grid corners
	float2 vSnappedUV = round( vUV * vDimensions.xy ) * vDimensions.zw;
	float2 vLerpWeights = ( (vUV - vSnappedUV) / (vDimensions.zw*0.5) ) * 0.5 + 0.5;

	float fTopLeftWeight  = (1 - vLerpWeights.x - vLerpWeights.y) + (vLerpWeights.x * vLerpWeights.y);
    float fTopRightWeight = vLerpWeights.x - (vLerpWeights.x * vLerpWeights.y);
    float fBottomLeft     = vLerpWeights.y - (vLerpWeights.x * vLerpWeights.y);
    float fBottomRight    = vLerpWeights.x * vLerpWeights.y;

	BilinearPoints output;
	output.vCenterCoord = vSnappedUV;// + (0.5 * vDimensions.zw);
	output.vWeights = float4( fTopLeftWeight, fTopRightWeight, fBottomLeft, fBottomRight );

	return output;
}

// -------------------------------------------------------------
// Bilateral up-sampling. Preserves edge discontinuities. 
// -------------------------------------------------------------
float BilateralWeight( float fOriginal, float fSample, float fBilateralCoef )
{
   
    // Just like a gaussian weight but based on difference between samples
    // rather than distance from kernel center

    const float fDiff = fSample-fOriginal;
    //const float fDiffSqrd = fDiff * fDiff;
    //const float f2CoefSqrd = 2.0f * fBilateralCoef*fBilateralCoef;
    //static const float fTwoPi = 6.283185f;
    
    //float fWeight = ( 1.0f / ( fTwoPi * f2CoefSqrd ) ) * exp( -(fDiffSqrd) / f2CoefSqrd );
    float fExp = ( abs(fDiff)/fBilateralCoef );
    float fWeight = exp( -0.5f * ( fExp*fExp ) );
    return fWeight;
}
// -------------------------------------------------------------
// Bilateral up-sampling. Preserves edge discontinuities. 
// -------------------------------------------------------------
float4 BilateralWeight4( float fOriginal, float4 fSample, float fBilateralCoef )
{
   
    // Just like a gaussian weight but based on difference between samples
    // rather than distance from kernel center
	const float4 fDiff = fSample - fOriginal.xxxx; 

    //float fWeight = ( 1.0f / ( fTwoPi * f2CoefSqrd ) ) * exp( -(fDiffSqrd) / f2CoefSqrd );
    float4 fExp = ( abs(fDiff)/(fBilateralCoef.xxxx) );
    float4 fWeight = exp( -0.5f * ( fExp*fExp ) );
    return fWeight;
}
OutHalf4Color PS_BlitTransparentBilinearBlur(rageVertexBlit IN)
{
#if __XENON
	IN.texCoord0.xy += gooScreenSize.xy;
#endif
	half4 color = h4tex2D(TransparentSrcMapSampler, IN.texCoord0.xy);
#if __XENON
	color.rgb = sRGBtoLinearCheap(color.rgb);
#endif
	return CastOutHalf4Color(PackHdr(UnpackColor(color)));
}

OutHalf4Color psBlitTransparentBilinearBilateral(rageVertexBlit IN, half4 color, bool bBilinearSwitch)
{
	float2 lowResTexCoord = IN.texCoord0;
	BilinearPoints points = ComputeBilinearUpsamplingPoints( TexelSize.zwxy, IN.texCoord0 );

	// Depth at high resolution texel. Because we down-sample depth by picking the min depth in a 2x2 block, this
	// depth will always be greater than or equal to the depth in the down sampled depth buffer (in the corresponding
	// texel) *unless* a particle has written a new depth to the down-sampled depth buffer. This is a way for us to 
	// detect if a particle has writen a new depth to the down sampled depth buffer. This doesn't hold for neighboring 
	// texels which can have a depth less than the high resolution depth.
	float fDestinationDepth = SampleDepth( DepthMapPointSampler, IN.texCoord0 );

	// 2x2 sample points from low res 
	float2 vTexCoord0 = points.vCenterCoord + half2( -0.5, -0.5 ) * TexelSize.xy;
	float2 vTexCoord1 = points.vCenterCoord + half2(  0.5, -0.5 ) * TexelSize.xy;
	float2 vTexCoord2 = points.vCenterCoord + half2( -0.5,  0.5 ) * TexelSize.xy;
	float2 vTexCoord3 = points.vCenterCoord + half2(  0.5,  0.5 ) * TexelSize.xy;


	#if __SHADERMODEL >= 50
		float4 vSourceDepths = LowResDepthPointMap.Gather(LowResDepthMapPointSampler, points.vCenterCoord).wzxy;
	#elif __SHADERMODEL >= 40
		float4 vSourceDepths = float4( LowResDepthPointMap.Sample(LowResDepthMapPointSampler, vTexCoord0),
									   LowResDepthPointMap.Sample(LowResDepthMapPointSampler, vTexCoord1),
									   LowResDepthPointMap.Sample(LowResDepthMapPointSampler, vTexCoord2),
									   LowResDepthPointMap.Sample(LowResDepthMapPointSampler, vTexCoord3) );
	#else
		float4 vSourceDepths = float4( tex2D(LowResDepthMapPointSampler, vTexCoord0).x,
									   tex2D(LowResDepthMapPointSampler, vTexCoord1).x,
									   tex2D(LowResDepthMapPointSampler, vTexCoord2).x,
									   tex2D(LowResDepthMapPointSampler, vTexCoord3).x );
	#endif
	
	//Make sure we apply the fixup to the sampled depth.
	vSourceDepths = fixupGBufferDepth(vSourceDepths);

	float4 vWeights = BilateralWeight4( fDestinationDepth, vSourceDepths, g_fBilateralCoefficient );
	//default all weights to 1 if all depths don't match source depth (happens on PS3 for pixels containing glass)
	vWeights = all( vWeights < 0.00001f) ? 1.0f: vWeights;
	if(bBilinearSwitch)
	{
		float4 killPix4 = vWeights < g_fBilateralEdgeThreshold.xxxx;
		bool bUseBilateral = (dot(killPix4, float4(1,1,1,1)) > 0);

		if ( !bUseBilateral )
		{
			return CastOutHalf4Color(PackHdr(color)); 
		}
	}	

	// Accumulate 4 weighted taps
	float4 vAccumColor = 0;
	float  fAccumWeight;

	// The "(points.vWeights.x >= 0.5)" below is used to find the low resolution texel that was generated from 
	// that same 2x2 neighborhood into which we are rendering this pixel. That way we can be sure that DestDepth
	// is closely associated with the SrcDepth of the low-res depth at this point. The magic 0.5 comes from the 
	// fact that the bilinear weight is only over 0.5 at the texel that is closest to the pixel being rendered.
	vSourceDepths = (fDestinationDepth.xxxx < vSourceDepths) && (points.vWeights > 0.5.xxxx) ? fDestinationDepth.xxxx : vSourceDepths;
	vWeights *= points.vWeights;
	fAccumWeight = dot(vWeights, float4(1,1,1,1));

	half4 vSampleColor = tex2Dlod(TransparentSrcMapSampler, float4(vTexCoord0,0,0)); //SampleLevel
	vSampleColor = UnpackColor(vSampleColor);
	vAccumColor = vSampleColor * vWeights.x;

	vSampleColor = tex2Dlod(TransparentSrcMapSampler, float4(vTexCoord1,0,0)); //SampleLevel
	vSampleColor = UnpackColor(vSampleColor);
	vAccumColor += vSampleColor * vWeights.y;

	vSampleColor = tex2Dlod(TransparentSrcMapSampler, float4(vTexCoord2,0,0)); //SampleLevel
	vSampleColor = UnpackColor(vSampleColor);
	vAccumColor += vSampleColor * vWeights.z;

	vSampleColor = tex2Dlod(TransparentSrcMapSampler, float4(vTexCoord3,0,0)); //SampleLevel
	vSampleColor = UnpackColor(vSampleColor);
	vAccumColor += vSampleColor * vWeights.w;

	// It is possible, that the accumulated weight will be zero. The mini-depth buffer is storing the minimum
	// depth valau from a 2x2 block in the high resolution buffer. If one pixel in a region has a big depth and
	// all the other pixels have a small depth, then the big depth will be lost and none of the neighboring
	// pixels in the downsampled particle color buffer will have contained useful information for this pixel. 
	// Typically this is easy to work around because this single pixel is in the foreground so we set the weight 
	// to 1 and leave all 0s in the color and alpha channels which will result in nothing being composited on top
	// of this single foreground pixel. Usually this what you want. There will be cases where this results in a
	// single wrong pixel that lets the background show through where a particle system is... but there is no
	// perfect solution here.

	//Using new condition for PS3.. lerp does not have good precision and rounds low values to zero..
	//#if __PS3
	fAccumWeight = fAccumWeight?fAccumWeight:1.0f;
	//#else
	//fAccumWeight = lerp( 1.0f, fAccumWeight, ceil(fAccumWeight) ); 
	//#endif
	//vAccumColor  = lerp( h4tex2D(TransparentSrcMapSampler, IN.texCoord0), vAccumColor, ceil(fAccumWeight) );

	// Normalize sample weighting so that we don't add/subtract energy
	half4 vOutput = vAccumColor / fAccumWeight.xxxx;
	return CastOutHalf4Color(PackHdr(vOutput));
}

OutHalf4Color PS_BlitTransparentBilinearBilateralWithAlphaDiscard(rageVertexBlit IN)
{
	float2 lowResTexCoord = IN.texCoord0;
	half4 color = tex2D( TransparentSrcMapSampler, lowResTexCoord ); //SampleLevel(MySampler, Location, LOD);
	color = UnpackColor(color);		

	// Discard if there's nothing to up-sample here
	rageDiscard( color.a == 0.0 );

	
	return psBlitTransparentBilinearBilateral(IN, color, true);
}

OutHalf4Color PS_BlitTransparentBilinearBilateral(rageVertexBlit IN)
{
	float2 lowResTexCoord = IN.texCoord0;
	half4 color = tex2D( TransparentSrcMapSampler, lowResTexCoord ); //SampleLevel(MySampler, Location, LOD);
	color = UnpackColor(color);		

	return psBlitTransparentBilinearBilateral(IN, color, true);
}


OutHalf4Color PS_BlitTransparentBilateralBlur(rageVertexBlit IN)
{	
	return psBlitTransparentBilinearBilateral(IN, 0.0f, false);
}

// -------------------------------------------------------------

vertexOutputUnlit VS_TransformUnlit_IM(rageVertexInputLit IN)
{
    vertexOutputUnlit OUT;
    
    // Write out final position & texture coords
    OUT.pos = mul(float4(IN.pos,1), gWorldViewProj);

#ifdef NVSTEREO
	if (gStereorizeHud)
	{
		float2 stereoParms = StereoParmsTexture.Load(int3(0,0,0)).xy;
		OUT.pos.x += stereoParms.x * OUT.pos.w;
	}
	else if (gMinimapRendering && (OUT.pos.w != 1.0f))
	{
		OUT.pos = StereoToMonoClipSpace(OUT.pos);
	}
#endif

    OUT.texCoord = IN.texCoord0;
    OUT.color0 = IN.diffuse;
    return OUT;
}
// ----------------------------------------------------------------------------------------------- //

OutHalf4Color PS( rageVertexBlit IN)
{
     return CastOutHalf4Color((half4)h4tex2D(DiffuseSampler, IN.texCoord0));
}

// ----------------------------------------------------------------------------------------------- //

OutHalf4Color PS_Textured_IM( vertexOutput IN)
{
	float4 ret=PixelTextured(IN, 8, true, true, false);

	return CastOutHalf4Color(half4(ret.rgb*gEmissiveScale, ret.a));
}

// ----------------------------------------------------------------------------------------------- //

OutHalf4Color PS_TexturedUnlit_IM( vertexOutputUnlit IN )
{
	float4 ret=PixelTexturedUnlit(IN);

	return CastOutHalf4Color(half4(ret.rgb*gEmissiveScale, ret.a));
}

// ----------------------------------------------------------------------------------------------- //

OutHalf4Color PS_Textured_IM_PACKED( vertexOutput IN)
{
	float4 ret=PixelTextured(IN, 8, true, true, false);

	return CastOutHalf4Color(half4(PackColor_3h(ret.rgb*gEmissiveScale), ret.a));
}

// ----------------------------------------------------------------------------------------------- //

OutHalf4Color PS_TexturedUnlit_IM_PACKED( vertexOutputUnlit IN )
{
	float4 ret=PixelTexturedUnlit(IN);

	return CastOutHalf4Color(half4(PackColor_3h(ret.rgb*gEmissiveScale), ret.a));
}
OutHalf4Color PS_Depth_Only_MRT( vertexOutputUnlit IN )
{
	return CastOutHalf4Color((half4)0);
}

// ----------------------------------------------------------------------------------------------- //

// ----------------------------------------------------------------------------------------------- //


// ----------------------------------------------------------------------------------------------- //

float VSGetDepth(float2 tex)
{
#if __XENON
	return tex2Dlod(CoronasDepthMapSampler, float4(tex, 0.0f.xx)).r;
#elif __PS3 || __SHADERMODEL >= 40
	// this uses a pre formated F32 target, so there's no need for a depth specific lookup...
	return tex2Dlod(CoronasDepthMapSampler, float4(tex, 0.0f.xx)).r;
#else
	return 0.0f;
#endif // __XENON
}

float GetCoronaScreenEdgeFade(float2 screenPos)
{
	const float screenEdgeMinOffset = refMipBlurParams.x;
	const float screenEdgeScale = (1.0f/(1.0001f-screenEdgeMinOffset));

	float2 screenPosAbs = saturate(abs(screenPos.xy)-screenEdgeMinOffset.xx);
	float invScaledChebyshevDist = 1.0f-saturate(max(screenPosAbs.x, screenPosAbs.y) * screenEdgeScale);
	float screenEdgeFade = invScaledChebyshevDist*invScaledChebyshevDist;

	return screenEdgeFade;
}

// ----------------------------------------------------------------------------------------------- //

vertexCoronaOUT VS_corona_internal(vertexCoronaIN IN, bool bApplyNearFade, bool bHideIfCentreOccluded, out float2 debugOcclusion)
{
	vertexCoronaOUT OUT;

	debugOcclusion = 0;

#if __XENON || __PS3 || __SHADERMODEL >= 40
	float3 tPos = IN.midPos;
	float colScale = 0.0f;

	const float vSize = length(IN.pos - IN.midPos)*0.707106781f;
	const float vDist = length(IN.midPos - gViewInverse[3].xyz);

	float3 oVal;

	float furtherColScale = 1.0f;

	if (bApplyNearFade && vDist < vSize)
	{
		const float3 vDir = float3(gWorldView[2].xyz);
		const float vDot = dot(vDir, IN.midPos - gViewInverse[3].xyz);

		if (vDot > -vSize)
		{
			furtherColScale = saturate((1.0f - vDot/vSize)*0.5f);
		}

	#if !defined(SHADER_FINAL)
		debugOcclusion.x = colScale;
	#endif // !defined(SHADER_FINAL)
	}

	const float4 mPos = MonoToStereoClipSpace(mul(float4(IN.midPos,1), gWorldViewProj));
	const float4 vPos = MonoToStereoClipSpace(mul(float4(IN.pos,1), gWorldViewProj));

	const float oow = 1.0f/mPos.w;

	const float3 mScreenPos = mPos.xyz*oow;
	const float3 vScreenPos = vPos.xyz*oow;

	float2 delta = float2(vScreenPos.xy) - float2(mScreenPos.xy);
	const float radius = min(length(delta)*GeneralParams0.x, GeneralParams0.y*2.0f*gooScreenSize);
	const float depth	= VSGetDepth(mScreenPos.xy*float2(0.5f, -0.5f) + 0.5f.xx);

	if (depth >= mPos.w || !bHideIfCentreOccluded)
	{
		// For each sample in the Poisson disc
		[loop]
		for (int i = 0; i < 12; i++)
		{
			const float4 tScreenPos = mScreenPos.xyxy + poisson24[i].xyzw*radius;
			colScale += fselect_ge(VSGetDepth(tScreenPos.xy*float2(0.5f, -0.5f) + 0.5f.xx), mPos.w, 1.0f, 0.0f);
			colScale += fselect_ge(VSGetDepth(tScreenPos.zw*float2(0.5f, -0.5f) + 0.5f.xx), mPos.w, 1.0f, 0.0f);
		}

	#if !defined(SHADER_FINAL)
		debugOcclusion.y = (depth >= mPos.w);
	#endif // !defined(SHADER_FINAL)
	}

	colScale *= 1.0f/24.0f;
	
	#if !defined(SHADER_FINAL)
		debugOcclusion.x = colScale;
	#endif // !defined(SHADER_FINAL)
	colScale *= colScale; // square occlusion amount so coronas don't fade out so abruptly

	// Fade coronas close to the edge of the screen
	float screenEdgeFade = GetCoronaScreenEdgeFade(mScreenPos.xy);
	colScale *= screenEdgeFade;

	if (colScale > 0) // corona is visible
	{
		tPos = IN.pos;
	}
#else // PC DX9 can't do depth read in vertex shader
	float furtherColScale = 1.0f;
	const float3 tPos = IN.pos;
	const float colScale = 1.0f;
#endif

	OUT.pos = MonoToStereoClipSpace(mul(float4(tPos,1), gWorldViewProj));

#if __PS3 //|| RSG_ORBIS // not sure about this .. why is this necessary, and why is it ps3-only?
	if (bApplyNearFade)
	{
		const float distClip = 0.5f;
		const float2 vClip = saturate((abs(vScreenPos) - 1.0f.xx)/distClip);
		colScale *= (1.0f - vClip.x*vClip.y);
	}
#endif // __PS3

	OUT.texCoord.x = IN.uHDRMult.x; // u
	OUT.texCoord.y = IN.diffuseV.a; // v

	OUT.color0.rgb = (IN.diffuseV.rgb*colScale*furtherColScale*IN.uHDRMult.y); // HDR multiplier

	// opacity fog coronas (they're additively blended)
	OUT.color0.rgb *= (1.0f - CalcFogData(IN.pos.xyz - gViewInverse[3].xyz).w);

	return OUT;
}

vertexCoronaOUT VS_corona(vertexCoronaIN IN)
{
	float2 debugOcclusion;
	return VS_corona_internal(IN, true, true, debugOcclusion);
}

vertexCoronaOUT VS_corona_nofade(vertexCoronaIN IN)
{
	float2 debugOcclusion;
	return VS_corona_internal(IN, false, true, debugOcclusion);
}

vertexCoronaOUT VS_corona_nofade_nodepth(vertexCoronaIN IN)
{
	vertexCoronaOUT OUT;
	OUT.pos = MonoToStereoClipSpace(mul(float4(IN.pos,1), gWorldViewProj));
	OUT.texCoord.x = IN.uHDRMult.x; // u
	OUT.texCoord.y = IN.diffuseV.a; // v

	OUT.color0.rgb = (IN.diffuseV.rgb*IN.uHDRMult.y); // HDR multiplier

	// opacity fog coronas (they're additively blended)
	//OUT.color0.rgb *= (1.0f - CalcFogData(IN.pos.xyz - gViewInverse[3].xyz).w);

	return OUT;
}

#if !defined(SHADER_FINAL)

struct vertexCoronaDbgOUT
{
	DECLARE_POSITION(pos)
	float2 texCoord		: TEXCOORD0;
	float3 color0		: TEXCOORD1;
	float2 occlusion	: TEXCOORD2; // x = occlusion scale, y = centre occluded
};

vertexCoronaDbgOUT VS_corona_dbg_BANK_ONLY(vertexCoronaIN IN)
{
	vertexCoronaDbgOUT OUT;
	vertexCoronaOUT temp = VS_corona_internal(IN, false, false, OUT.occlusion);

	OUT.pos = mul(float4(IN.pos,1), gWorldViewProj);
	OUT.texCoord = temp.texCoord;
	OUT.color0 = temp.color0;

	return OUT;
}

#endif // !defined(SHADER_FINAL)

// ----------------------------------------------------------------------------------------------- //

OutHalf4Color PS_corona(vertexCoronaOUT IN)
{
#if __SHADERMODEL >= 40
	// On current-gen consoles, this is set to broadcast via hw register.
	float3 outColor = tex2D(DiffuseSampler, IN.texCoord).r;
#else
	float3 outColor = tex2D(DiffuseSampler, IN.texCoord).rgb;
#endif	
	// by request, if the texture sample is 0, don't add a base value
	float baseValueMultiplier = step( 1.0 / 255.0, outColor.r );
	float3 baseValue = GeneralParams1.rgb * baseValueMultiplier;
	
	outColor = ProcessDiffuseColor(outColor);
	outColor *= outColor;
	float3 ret = outColor * IN.color0.rgb;
	ret += baseValue;
	
	return CastOutHalf4Color(PackHdr(float4(ret, 1)));
}

#if !defined(SHADER_FINAL)
OutHalf4Color PS_corona_dbg_BANK_ONLY(vertexCoronaDbgOUT IN)
{
	const float2 v = IN.texCoord*2 - 1; // [-1..1]
	const float dial = (fmod(atan2(v.x, v.y) + PI, 2*PI) < 2*PI*IN.occlusion.x);
	const float mask = (dot(v, v) < 1); // mask
	const float ring = pow(min(dot(v, v), 1), 16)*mask; // ring

	return CastOutHalf4Color(float4(lerp(float3(1,0,0), float3(0,0,1), dial)*mask + ring*IN.occlusion.y, 1)*(IN.occlusion.y > 0 ? 1 : 0.022));
}
#endif // !defined(SHADER_FINAL)

// ----------------------------------------------------------------------------------------------- //
vertexCoronaOUT VS_corona_simple_reflection_common(vertexCoronaIN IN
#if RAGE_INSTANCED_TECH
, uniform bool bUseInst
#endif
)
{
	vertexCoronaOUT OUT;

#if GS_INSTANCED_CUBEMAP
	if (bUseInst)
		OUT.pos = mul(float4(IN.pos,1), gInstWorldViewProj[IN.InstID]);
	else
#endif
		OUT.pos = mul(float4(IN.pos,1), gWorldViewProj);

	OUT.texCoord.x = IN.uHDRMult.x; // u
	OUT.texCoord.y = IN.diffuseV.a; // v

	OUT.color0 = IN.diffuseV.rgb*IN.uHDRMult.y; // HDR multiplier

	return OUT;
}

vertexCoronaOUT VS_corona_simple_reflection(vertexCoronaIN IN)
{
	return VS_corona_simple_reflection_common(IN
#if RAGE_INSTANCED_TECH
,false
#endif
);
}
// ----------------------------------------------------------------------------------------------- //

OutHalf4Color PS_corona_simple_reflection(vertexCoronaOUT IN)
{
#if __SHADERMODEL >= 40
	// On current-gen consoles, this is set to broadcast via hw register.
	float3 texColor = tex2D(DiffuseSampler, IN.texCoord).r;
#else
	float3 texColor = tex2D(DiffuseSampler, IN.texCoord).rgb;
#endif // __SHADERMODEL >= 40

	// by request, if the texture sample is 0, don't add a base value
	float baseValueMultiplier = step( 1.0 / 255.0, texColor.r );
	float3 baseValue = GeneralParams1.rgb * baseValueMultiplier;
	
	texColor = ProcessDiffuseColor(texColor);
	texColor *= texColor;
	float3 outColor=texColor*IN.color0.rgb;
	outColor += baseValue;

	return CastOutHalf4Color(float4(outColor * 0.125f, 1));
}

// ----------------------------------------------------------------------------------------------- //

vertexCoronaOUT VS_corona_paraboloid_reflection(vertexCoronaIN IN)
{
	vertexCoronaOUT OUT;

	float3 diff=IN.pos-IN.midPos;
	float size=length(diff)*0.707106781f;

	float3 vPos=gViewInverse[3].xyz;

	float3 dir=IN.midPos-vPos;

	float3 tPos=IN.pos;

	float3 vDir=float3(0,0,1);

	float3 right=normalize(cross(dir, vDir))*(dir.z>0);
	float3 up=normalize(cross(dir, right))*(dir.z>0);

	tPos=IN.midPos+(right*diff.x)+(up*diff.y);

	OUT.pos = DualParaboloidPosTransform(tPos);
	OUT.pos.w = 1.0f;

	OUT.texCoord.x = IN.uHDRMult.x; // u
	OUT.texCoord.y = IN.diffuseV.a; // v

	OUT.color0 = IN.diffuseV.rgb*IN.uHDRMult.y*saturate(1.0 / (size * 0.5f));
	return OUT;
}

// ----------------------------------------------------------------------------------------------- //

vertexOutputLP VS_screenTransform(vertexInputLP IN)
{
	vertexOutputLP OUT;
	OUT.pos	= float4( (IN.pos.xy-0.5f)*2.0f, 0, 1);
	return(OUT);
}

// ----------------------------------------------------------------------------------------------- //

vertexOutputLPT VS_screenTransformSS(vertexInputLPT IN)
{
	// Pass down screen coords, this avoids the VPOS interpolator penalty on the 360
	vertexOutputLPT OUT;
	OUT.pos	= float4( (IN.pos.xy-0.5f)*2.0f, 0, 1);
	OUT.tex.xy = (OUT.pos.xy * float2( 0.5f, -0.5f ) + 0.5f) * gScreenSize.xy;
	OUT.tex.zw = 0;

#if __XENON
	OUT.tex.xy += float2(0.5,0.5);
#endif

	return(OUT);
}

// ----------------------------------------------------------------------------------------------- //

#if __PS3
OutFloat4Color PS_depthCopyEx( float2 vPos : VPOS, out float depth : DEPTH )
{
	float2 texCoord;

	#if __PS3
	texCoord.xy = vPos.xy * TexelSize.zw;
	depth = texDepth2D(DepthPointSampler, texCoord);
	#else // __PS3
	texCoord.x = (vPos.x + 0.50f) * (TexelSize.z);
	texCoord.y = (vPos.y + 0.50f) * (TexelSize.w);
	depth = tex2D(DepthSampler, texCoord).r;
	#endif // __PS3

	return CastOutFloat4Color(float4(0.0f.xxx, 1.0f));
}
#endif // __PS3

// ----------------------------------------------------------------------------------------------- //

// ----------------------------------------------------------------------------------------------- //
#if __XENON
// ----------------------------------------------------------------------------------------------- //
OutFloat4Color PS_depthCopy( float2 ScreenPos : VPOS )
{
	float ColumnIndex = (ScreenPos.x) / 80;
	float HalfColumn = frac( ColumnIndex );
	float2 DepthTexCoord = ScreenPos;
	if( HalfColumn >= 0.5 )
	{
		DepthTexCoord.x -= 40;
	}
	else
	{
		DepthTexCoord.x += 40;
	}

	float4 DepthData;
	asm
	{
		tfetch2D DepthData, DepthTexCoord, DepthSampler, UnnormalizedTextureCoords=true, MagFilter=point, MinFilter=point
	};

	float4 DepthResult = DepthData.zyxw;
	
	// mask out top 5 bits
	// values range from 0->255,  so 255-1. so 8  -> float 8/255.f
	//DepthResult.r = fmod( DepthResult.r, 8.f/255.f); // (0.367 ms)
	DepthResult.r = frac(DepthResult.r / (8.f / 255.f)) * (8.f / 255.f); // (0.345 ms)

	return CastOutFloat4Color(DepthResult);
}

// Same as above, but copies full stencil (doesn't shave off top 5 bits)
OutFloat4Color PS_depthStencilCopy( vertexOutputLPT IN )
{
	// Screen coords from VS, this avoids the VPOS interpolator penalty on the 360
	float2 vScreenPos = IN.tex.xy;
	bool bLeftRight = frac( vScreenPos.x / 80.0f ) < 0.5f; 
	vScreenPos.x += bLeftRight ? 40.0f : -40.0f;

	float4 StencilData;
	asm
	{
		tfetch2D StencilData.zyxw, vScreenPos, DepthSampler, UnnormalizedTextureCoords=true, MagFilter=point, MinFilter=point
	};

	return CastOutFloat4Color(StencilData);
}

vertexOutputLPT2 VS_RestoreDepth(vertexInputLP IN)
{
	vertexOutputLPT2 OUT;
	OUT.pos	= float4(IN.pos.xy, 0, 1);
	
	// Avoid the VPOS interpolation penalty, and some pixel shader ALU.
    // Instead, hoist calculation of screenspace coords to vertex shader.
    OUT.tex = ( IN.pos.xy * float2( 0.5f, -0.5f ) + 0.5f ) * GeneralParams0.xy + GeneralParams0.zw + 0.5f;

	return(OUT);
}
float4 PS_RestoreDepthCommon(float2 tex, bool bRestore3BitStencil)
{
    float2 vTexCoord = tex;

    bool bLeftRight = frac( tex.x / 80.0f ) < 0.5f; 
    vTexCoord.x += bLeftRight ? 40.0f : -40.0f;

    float4 vDepthStencilAsColor;
    asm 
    {
        tfetch2D vDepthStencilAsColor.zyxw, vTexCoord, DepthSampler, UnnormalizedTextureCoords=true, MinFilter=point, MagFilter=point
    };

	if(bRestore3BitStencil)
	{
		// DF: This makes the shader ALU bound and kills performance, but we need this for HiStencil.
		// mask out top 5 bits
		// values range from 0->255,  so 255-1. so 8  -> float 8/255.f
		//vDepthStencilAsColor.r = fmod( vDepthStencilAsColor.r, 8.f/255.f); // 10 ALU Instructions (0.415 ms)
		vDepthStencilAsColor.r = frac(vDepthStencilAsColor.r / (8.f / 255.f)) * (8.f / 255.f); // 9 ALU Instructions (0.388 ms)

	}

    return vDepthStencilAsColor;
}

OutFloat4Color PS_RestoreDepth3BitStencil(float2 tex : TEXCOORD0)
{
	return CastOutFloat4Color(PS_RestoreDepthCommon(tex, true));
}

OutFloat4Color PS_RestoreDepth(float2 tex : TEXCOORD0)
{
	return CastOutFloat4Color(PS_RestoreDepthCommon(tex, false));
}

// ----------------------------------------------------------------------------------------------- //
#else // __XENON
// ----------------------------------------------------------------------------------------------- //
OutFloat4Color PS_depthCopy( float2 vPos : VPOS, out float depth : DEPTH )
{
	float2 texCoord;

#if __PS3
	texCoord.xy = vPos.xy / float2(SCR_BUFFER_WIDTH,SCR_BUFFER_HEIGHT);
	depth = texDepth2D(depthSourceSampler, texCoord);
#else // __PS3
	texCoord.xy = (vPos.xy + 0.50f) * (TexelSize.zw);
	depth = tex2D(DepthSampler, texCoord).r;
#endif // __PS3

	return CastOutFloat4Color(float4(0.0f.xxx, 1.0f));
}

OutHalf4Color PS_depthCopyRaw(vertexOutputLPT IN)
{
#if __PS3
	return CastOutHalf4Color(h4tex2D(DepthPointSampler, IN.tex.xy));
#else
	return CastOutHalf4Color(0.0.xxxx);
#endif
}
// ----------------------------------------------------------------------------------------------- //
#endif // __XENON
// ----------------------------------------------------------------------------------------------- //

// ----------------------------------------------------------------------------------------------- //

OutFloat4Color PS_nothing(vertexOutputLPT IN)
{
	return CastOutFloat4Color(float4(0,0,0,0));
}

// ----------------------------------------------------------------------------------------------- //

#if __PS3 || __XENON
OutHalf4Color PS_GBufferCopy(vertexOutputLPT IN)
#else
OutHalf4Color PS_GBufferCopy(pixelInputLP IN)
#endif
{
	float2 posXY;

	#if __PS3 || __XENON
		posXY = IN.tex.xy; // Texel offset done for 360 below
	#else
		posXY.x = (IN.pos.x + 0.5f) * (TexelSize.z);
		posXY.y = (IN.pos.y + 0.5f) * (TexelSize.w);
	#endif

	half4 texel;

#if __XENON
	asm { tfetch2D texel, posXY, DiffuseSampler, OffsetX=0.5, OffsetY=0.5 };
#else
	texel  = h4tex2D(DiffuseSampler, posXY );
#endif

	return CastOutHalf4Color(texel);

}

// ----------------------------------------------------------------------------------------------- //

vertexColCorUnlitOUT VS_ColorCorrectedUnlit(vertexColCorUnlitIN IN)
{
	vertexColCorUnlitOUT OUT;

#ifdef NVSTEREO
	float4 stereoClipPos = (mul(float4(IN.pos.xyz, 1.0), gWorldViewProj));
	float2 stereoParms = StereoParmsTexture.Load(int3(0,0,0)).xy;
	stereoClipPos.x += stereoParms.x;

	OUT.pos = stereoClipPos;
#else
	OUT.pos =  (mul(float4(IN.pos.xyz, 1.0), gWorldViewProj));
#endif

	OUT.color0 = (IN.diffuse.rgb * IN.diffuse.rgb) * gEmissiveScale; // HDR multiplier	
	OUT.texCoord0 = IN.texCoord0;

	return OUT;
}

// ----------------------------------------------------------------------------------------------- //

OutHalf4Color PS_ColorCorrectedUnlit(pixelColCorUnlitIN IN)
{
	float4 color = h4tex2D(DiffuseSampler, IN.texCoord0 );

	float3 colorGrayScale = ((color.r + color.g + color.b) / 3) * GeneralParams1.w;
	color.rgb = lerp(color.rgb, colorGrayScale, GeneralParams1.z);

	color = ProcessDiffuseColor(color);
	color.rgb *= IN.color0.rgb;
	
	return CastOutHalf4Color(PackHdr(color));
}

// ----------------------------------------------------------------------------------------------- //

OutHalf4Color PS_ColorCorrectedUnlitGradient(pixelColCorUnlitIN IN)
{
	float4 color = h4tex2D(DiffuseSampler, IN.texCoord0 );
	color = ProcessDiffuseColor(color);
	color.rgb *= IN.color0.rgb;

	// Calculate the gradient change
	// GeneralParams0.xy - normalize(Flare2DPos - Light2DPos);
	// GeneralParams0.zw - Flare2DPos - GeneralParams0.xy * HalfWidth * 0.5;
	// GeneralParams1.xy - HalfWidth

	float2 vPixelPos = IN.pos.xy * gooScreenSize.xy;

	float2 normalised = (vPixelPos - GeneralParams0.zw) / GeneralParams1.xy;
	float fDot = saturate(dot(normalised, GeneralParams0.xy));
	color.rgb = color.rgb * fDot * fDot;

	return CastOutHalf4Color(PackHdr(color));
}

// ----------------------------------------------------------------------------------------------- //

vertexOutputLPT VS_screenTransformT(vertexInputLPT IN)
{
	vertexOutputLPT OUT;

	OUT.pos		= float4( (IN.pos.xy-0.5f)*2.0f, 0, 1); //adjust from viewport space (0 to 1) to screen space (-1 to 1)
	OUT.tex		= IN.tex;

	return(OUT);
}


vertexOutputLPT VS_screenTransformZT(vertexInputLPT IN)
{
	vertexOutputLPT OUT;

	OUT.pos		= float4( (IN.pos.xy-0.5f)*2.0f, IN.pos.z, 1); //adjust from viewport space (0 to 1) to screen space (-1 to 1)
	OUT.tex		= IN.tex;

	return(OUT);
}

// ----------------------------------------------------------------------------------------------- //

vertexOutputLPT VS_refMipBlurCubeToParabT(vertexInputLPT IN)
{
	vertexOutputLPT OUT;

	// UVs come in [-1,1] and this quad only covers half the screen. IN.pos.z is 1 for the left half (lower hemisphere), and is -1 for the right half (upper hemisphere).
	OUT.pos		= float4( (IN.pos.xy-0.5f)*2.0f, 0, 1); //adjust from viewport space (0 to 1) to screen space (-1 to 1)
	OUT.tex.xy	= IN.tex.xy; 
	OUT.tex.zw	= IN.pos.zz;

	return(OUT);
}

// ----------------------------------------------------------------------------------------------- //

OutHalf4Color PS_refMipBlurBlend(vertexOutputLPT IN)
{
	return CastOutHalf4Color(half4(h3tex2D(DiffuseSampler, IN.tex.xy), refMipBlurParams_Alpha));
}

// ----------------------------------------------------------------------------------------------- //

OutHalf4Color PS_refMipBlurExtendReflection(vertexOutputLPT IN)
{
	return CastOutHalf4Color(half4(refMipBlurExtendReflection(IN), 1));
}

// ----------------------------------------------------------------------------------------------- //

OutHalf4Color PS_refMipBlurCubeToParab(vertexOutputLPT IN)
{
	// IN.tex.xy is already in [-1,1] space for a given hemisphere of the parab. The hemisphere we are working on
	// is specified by IN.tex.z (1 for lower hemisphere, -1 for upper hemisphere). See VS_refMipBlurCubeToParabT for
	// more comments. No need to normalize the resulting vector, cube map sampling doesn't require it for correctness.
	float3 p2w = parab2worldFast(IN.tex.xyz); 
	half3 col = texCUBElod(RenderCubeMapSampler, float4(p2w, 0));
	return CastOutHalf4Color(half4(col, 1.0));
}

// ----------------------------------------------------------------------------------------------- //

#if __PS3
OutHalf4Color PS_SDDownsample(rageVertexBlit IN)
{
	return CastOutHalf4Color(h4tex2D(DiffuseSampler, IN.texCoord0));
}
#endif // __PS3

// ----------------------------------------------------------------------------------------------- //

half4 PS_AlphaMaskToStencilMask_Common(vertexOutputLPT IN, bool useBlueChannel)
{
	const half4 textureSample = h4tex2D(RenderMapPointSampler, IN.tex);
	half a0 = useBlueChannel? textureSample.b: textureSample.a;
	return half4(0,0,0,a0);
}

// ----------------------------------------------------------------------------------------------- //

OutHalf4Color PS_AlphaMaskToStencilMask(vertexOutputLPT IN)
{
	return CastOutHalf4Color(PS_AlphaMaskToStencilMask_Common(IN, false));
}

// ----------------------------------------------------------------------------------------------- //
#if __XENON
OutHalf4Color PS_AlphaMaskToStencilMask_StencilAsColor(vertexOutputLPT IN)
{
	return CastOutHalf4Color(PS_AlphaMaskToStencilMask_Common(IN, true));
}
#endif

// ----------------------------------------------------------------------------------------------- //

#if __PS3
OutHalf4Color PS_SCullReload()
{
	return CastOutHalf4Color(0.0.xxxx);
}

OutHalf4Color PS_ZCullReload(out float depth : DEPTH)
{
	depth = 1.0f;
	return CastOutHalf4Color(0.0.xxxx);
}


vertexOutputLP VS_screenTransformGreaterThan(vertexInputLP IN)
{
	vertexOutputLP OUT;
	OUT.pos	= float4( (IN.pos.xy-0.5f)*2.0f,IN.pos.z, 1);

	return(OUT);
}

#if 1
OutHalf4Color PS_ZCullReloadGreaterThan(out float depth : DEPTH)
{ 
	depth = 0.0f;  // need to write 0 if we're restoring with greater than
	return CastOutHalf4Color(0.0.xxxx);
}
#else
OutHalf4Color PS_ZCullReloadGreaterThan()
{ 
	return CastOutHalf4Color(0.0.xxxx);
}
#endif

struct vertexZCullNear {
	float2 pos			: POSITION;
};


vertexOutputLP VS_screenTransformNearZ(vertexZCullNear IN)
{
	vertexOutputLP OUT;

	float depth =  tex2Dlod(DepthPointSampler, float4(IN.pos.xy, 0, 0));

	OUT.pos	= float4( (IN.pos.xy-0.5f)*2.0f, depth, 1);

	return(OUT);
}

#endif

struct vertexInputSeeThrough_IM
{
	vertexInputSeeThrough Vtx;
	float4 heat : COLOR0;
};

struct vertexOutputSeeThrough_IM
{
	vertexOutputSeeThrough Vtx;
	float4 heat : COLOR0;
};

vertexOutputSeeThrough_IM VS_Transform_IM_SeeThrough(vertexInputSeeThrough_IM IN)
{
	vertexOutputSeeThrough_IM OUT;
	
	OUT.Vtx = VS_Transform_SeeThrough(IN.Vtx);
	OUT.heat = IN.heat;
	
	return OUT;
}

OutFloat4Color PS_IM_SeeThrough( vertexOutputSeeThrough_IM IN)
{
	float4 result = SeeThrough_Render(IN.Vtx);
	
	// Only base color, we don't want them to show up hot/bright.
	result.z *= IN.heat.x;
	return CastOutFloat4Color(result);
}

// ----------------------------------------------------------------------------------------------- //

void getDistantLightInputVert(vertexInputDistantLights IN, out float3 worldPos, out float4 color, out float2 tex)
{
#if __XENON
	int index = IN.index;
	int posBufIdx = index / 4;
	int texBufIdx = index % 4;
	asm {
		vfetch worldPos.xyz, posBufIdx, position
		vfetch color, posBufIdx, color
		vfetch tex.xy, texBufIdx, texcoord		 // could calculate these from the texBufIdx instead
	};
#else
	worldPos = IN.worldPos;
	color = IN.col;
	tex = IN.tex;
#endif
}

// ----------------------------------------------------------------------------------------------- //

vertexOutputDistantLights VS_DistantLightsCommon(vertexInputDistantLights IN, bool dist2d)
{
	vertexOutputDistantLights OUT;

	const float params_lightDiameter = GeneralParams0.x;
	const float params_nearDistance = GeneralParams0.y;
	const float params_invDistDiff = GeneralParams0.z; // (1 / (far - near))
	const float params_hdrIntensity = GeneralParams0.w;

	const float params_sizeUpscale = GeneralParams1.x; // default is 1024.0f/6.0f

	float3 inWorldPos;
	float4 inColor;
	float2 inTex;

	getDistantLightInputVert(IN, inWorldPos, inColor, inTex);

	const float3 camLeft = gViewInverse[0].xyz;
	const float3 camUp = gViewInverse[1].xyz;
	const float3 camDir = -gViewInverse[2].xyz;
	const float3 camPos = +gViewInverse[3].xyz;

	// Map u,v 0->1 to pos offset 0.5,0.5, then scale by diameter
	float2 offset = (inTex - 0.5f) * params_lightDiameter.xx;

	float3 lightLeft = offset.xxx * camLeft;
	float3 lightUp = offset.yyy * camUp;

	// Fade out the light as it increases in projected screen space size
	// range for light size is roughly 0.004 for a light that's real far to about 0.080 for one that's about half the size of the minimap
	float lightDistance = dot(inWorldPos - camPos, camDir);
	float lightSizeInv = lightDistance*params_sizeUpscale; // Make the numbers more reasonable (normal range is -1 to 1, scale by 1024/6 to get approx. pixel size)

	// 1.0f if lightSizeInv < 1, otherwise upscaleFactor / lightSizeInv = 1
	float upscaleFactor = max(1.0f, lightSizeInv);

	float worldDist;
	if (dist2d)
	{
		worldDist = distance(inWorldPos.xy, camPos.xy);
	}
	else
	{
		worldDist = distance(inWorldPos, camPos);
	}
	float postDepthFxColorModifier = (worldDist - params_nearDistance) >= 0.0f;

	// squared because we expand the point along 2 dimensions, so light energy should be reduced by the square of the expansion
	postDepthFxColorModifier /= (upscaleFactor * upscaleFactor);

	float4 fogData = CalcFogData(inWorldPos - gViewInverse[3].xyz);
	half alpha = (1.0 - fogData.a) * postDepthFxColorModifier;

#if USE_DISTANT_LIGHTS_2
	float DistFadeStart = refMipBlurParams.y;
	float DistFadeRange = refMipBlurParams.z;
	float distanceFadeUp = saturate((worldDist - DistFadeStart) / DistFadeRange);

	if(refMipBlurParams.x)
		alpha *= (distanceFadeUp);
#endif //USE_DISTANT_LIGHTS_2

	float3 worldCornerPos = inWorldPos + (lightLeft + lightUp) * upscaleFactor * (alpha > 0);
	float4 screenCornerPos = mul(float4(worldCornerPos, 1), gWorldViewProj);

	float intensityMult = lerp(1.0, 16.0, saturate((worldDist - 500.0) / 2000.0));

	// Final settings
	OUT.texAndCentrePos = half4(inTex.xy, inWorldPos.xy);
	OUT.pos = screenCornerPos;
	OUT.col.rgb = inColor.rgb * (inColor.a * params_hdrIntensity) * alpha * intensityMult;

	RAGE_COMPUTE_CLIP_DISTANCES(OUT.pos);

	return OUT;
}

vertexOutputDistantLights VS_DistantLights2d(vertexInputDistantLights IN)
{
	return VS_DistantLightsCommon(IN, true);
}

vertexOutputDistantLights VS_DistantLights3d(vertexInputDistantLights IN)
{
	return VS_DistantLightsCommon(IN, false);
}

// ----------------------------------------------------------------------------------------------- //

half4 PS_DistantLightsCommon(vertexOutputDistantLights IN)
{
	const float params_flickerMin = GeneralParams1.y;
	const float params_elapsedTime = GeneralParams1.z;
	const float params_twinkleAmount = GeneralParams1.w;

	float4 hfNoiseSample = tex2D(NoiseSampler, IN.texAndCentrePos.zw + params_elapsedTime).g;
	float flickerAmt = lerp(params_flickerMin, 1.0f, hfNoiseSample.a);

	float lfSampleDensity = 1.0f / 512.0f;	// Bigger denominator -> bigger "patches" of light and dark across the map
	float4 lfNoiseSample = tex2D(NoiseSampler, IN.texAndCentrePos.zw * lfSampleDensity + params_elapsedTime).g;
	float twinkleAmt = lerp(params_twinkleAmount, 1.0f, lfNoiseSample.r);

#if __SHADERMODEL >= 40
	// On current-gen consoles, this is set to broadcast via hw register.
	float3 color = tex2D(DiffuseSampler, IN.texAndCentrePos.xy).r;
#else
	float4 color = h4tex2D(DiffuseSampler, IN.texAndCentrePos.xy);
#endif // __SHADERMODEL >= 40
	color = ProcessDiffuseColor(color);
	color *= color;

	float3 finalColor = color.rgb * IN.col.rgb * flickerAmt * twinkleAmt;

	return half4(finalColor, 1.0f);
}

// ----------------------------------------------------------------------------------------------- //

OutHalf4Color PS_DistantLights(vertexOutputDistantLights IN)
{
	half4 OUT = PS_DistantLightsCommon(IN);
	return CastOutHalf4Color(PackHdr(OUT));
}

// ----------------------------------------------------------------------------------------------- //

OutHalf4Color PS_DistantLightsReflection(vertexOutputDistantLights IN)
{
	half4 OUT = PS_DistantLightsCommon(IN);
	return CastOutHalf4Color(half4(OUT.rgb, OUT.a));
}

// ----------------------------------------------------------------------------------------------- //

#if ENABLE_DISTANT_CARS
#if USE_DISTANT_LIGHTS_2
vertexOutputDistantCars VS_DistantCars(vertexInputDistantCars IN)
{
	vertexOutputDistantCars OUT;

	float3 carPosition, carDirection;

#if __XENON
	int index = IN.index;
	//num vertices in model 24 - prob want to pass in as shader constant.
	int posBufIdx = index / 24;
	asm {
		vfetch carPosition.xyz, posBufIdx, texcoord1
		vfetch carDirection.xyz, posBufIdx, texcoord2
	};
#else
	carPosition = IN.carPosition;
	carDirection = -IN.carDirection;
#endif
	float3 carRight = normalize(cross( carDirection, float3(0,0,1)));
	float3 carUp = normalize(cross( carRight, carDirection));
	float3x3 worldMat;
	worldMat[0] = carRight;
	worldMat[1] = carDirection;
	worldMat[2] = carUp;

	float scale = 1.0;
	float3 pos = mul(IN.pos * scale, worldMat);
	pos += float3(0.0, 0.0, -1.0);	//move cars down vertically to sit on the road
	OUT.pos = mul(float4(pos + carPosition, 1.0), gWorldViewProj);


	OUT.normal = NORMALIZE_VS(mul(IN.normal, worldMat));

	OUT.colour = IN.instanceColour;
	OUT.tex = IN.tex;

	OUT.alphaVal = IN.instanceColour.a;
	return OUT;
}
#else
#if __PS3
#define DISTANT_CARS_BATCH_SIZE	16

// 2 float4's per car:
//
// 1) XYZ - Position
// 2) XYZ - Direction

float4		instancingData[DISTANT_CARS_BATCH_SIZE*2] REGISTER2(vs,c64);
#endif

vertexOutputDistantCars VS_DistantCars(vertexInputDistantCars IN)
{
	vertexOutputDistantCars OUT;

	float3 carPosition, carDirection;

#if __XENON
	int index = IN.index;
	//num index in model 36 - prob want to pass in as shader constant.
	int posBufIdx = index / 36;
	int nIndexOfIndex = index - (posBufIdx * 36);

	// Fetch mesh index
	int4 vMeshIndexValue;
	asm {
		vfetch vMeshIndexValue, nIndexOfIndex, texcoord1, UseTextureCache = true
	};

	float4 posv;
	float4 normalv;
	asm {
		vfetch posv, vMeshIndexValue.x, position0
		vfetch normalv, vMeshIndexValue.x, normal0
	};

	asm {
		vfetch carPosition.xyz, posBufIdx, texcoord2
		vfetch carDirection.xyz, posBufIdx, texcoord3
	};
#elif __PS3
	float4 posv = float4(IN.pos.xyz,1.0);
	float4 normalv = float4(IN.normal.xyz,0.0);

	carPosition = instancingData[(IN.index*2)].xyz;//IN.carPosition;
	carDirection = instancingData[(IN.index*2)+1].xyz;//IN.carDirection;
#else
	float4 posv = float4(IN.pos.xyz,1.0);
	float4 normalv = float4(IN.normal.xyz,0.0);
	carPosition = IN.carPosition;
	carDirection = IN.carDirection;
#endif
	float3 carRight = normalize(cross( float3(0,0,1), carDirection));
	float3 carUp = normalize(cross( carRight, carDirection));
	float3x3 worldMat;
	worldMat[0] = carRight;
	worldMat[1] = carDirection;
	worldMat[2] = carUp;

	float scale = 0.7;
	float3 pos = mul(posv * scale, worldMat);

	// on the PS3 we need to kill verts that are too close to the camera
#if __PS3
	float distFromCamera = length(carPosition - gViewInverse[3].xyz);
	float isFarEnough = step(DistantCarNearClip,distFromCamera);

	pos *= isFarEnough;
	carPosition *= isFarEnough;
#endif
	OUT.pos = mul(float4((pos + carPosition), 1.0), gWorldViewProj);
	OUT.normal = NORMALIZE_VS(mul(normalv, worldMat));
	OUT.colour = float4( 0.1, 0.1, 0.1, DistantCarAlpha);

	return OUT;
}
#endif
// ----------------------------------------------------------------------------------------------- //

DeferredGBuffer PS_DistantCars(vertexOutputDistantCars IN)
{
#if USE_DISTANT_LIGHTS_2
	const int ms_aaMaskLut[32] = { 
	0x0000, 0x0000,
	0x000E, 0x000E, 0x000E,
	0xE00E, 0xE00E, 0xE00E,
	0x000F, 0x000F, 0x000F,
	0xE00F, 0xE00F, 0xE00F,
	0xF00F, 0xF00F, 0xF00F,
	0xF0EF, 0xF0EF, 0xF0EF,
	0xFEEF, 0xFEEF, 0xFEEF,
	0xFF0F, 0xFF0F, 0xFF0F,
	0xFFEF, 0xFFEF, 0xFFEF,
	0xFFFF, 0xFFFF, 0xFFFF
	};

	int alpha = IN.alphaVal * 255;
	int fadeMask = ms_aaMaskLut[alpha >> 3];

	int u32FadeMask[4] =
	{	(fadeMask >> 0)  & 0xf,
		(fadeMask >> 4)  & 0xf,
		(fadeMask >> 8)  & 0xf,
		(fadeMask >> 12) & 0xf 
	};

	float4 vectorFadeMask = float4( ((float)u32FadeMask[0])/15.0f, ((float)u32FadeMask[1])/15.0f, ((float)u32FadeMask[2])/15.0f, ((float)u32FadeMask[3])/15.0f );

	FadeDiscard( IN.pos, vectorFadeMask );

	float4 diffuseColour = float4(IN.colour.rgb, 1.0);
	float4 colour = tex2D(NoiseSampler, IN.tex);
	
	float nightRendering = GeneralParams0.x;
	if(nightRendering < 1.0f)
	{
		colour *= diffuseColour;
	}

	DeferredGBuffer OUT;
	OUT.col0 = colour;
	OUT.col1 = float4((IN.normal * 0.5f) + 0.5f, 1.0f);
	OUT.col2 = float4(0.8f, 0.2f, 0.95f, 0.94f);
	OUT.col3 = float4(0.65f, 0.5f, colour.r * 0.5 * nightRendering, 1.0f);
#else
	DeferredGBuffer OUT;
	OUT.col0 = IN.colour;
	OUT.col1 = float4((IN.normal * 0.5f) + 0.5f, 0.0f);
	OUT.col2 = float4(0.0f, 0.0f, 1.0f, 0.0f);
	OUT.col3 = float4(0.7f, 0.0f, 0.0f, 0.0f);
#endif //USE_DISTANT_LIGHTS_2

	return OUT;
}
// ----------------------------------------------------------------------------------------------- //
#endif //ENABLE_DISTANT_CARS

vertexOutputGameGlow VS_GameGlow(rageVertexBlitIn IN)
{
	vertexOutputGameGlow OUT;

	OUT.pos = mul(float4(IN.pos.xyz, 1.0f), gWorldViewProj);
	OUT.centerPos = IN.normal.xyz;
	OUT.col = IN.diffuse.rgb;
	OUT.tex	= IN.texCoord0.xy;
	
	return OUT;
}

// ----------------------------------------------------------------------------------------------- //
OutHalf4Color GameGlow(pixelInputGameGlow IN, float linearDepth)
{
	float2 otherScreenPos;
	otherScreenPos.x	= IN.pos.x * gooScreenSize.x;
	otherScreenPos.y	= 1.0f - IN.pos.y * gooScreenSize.y;

	float4 eyeRay = float4(mul(float4(((otherScreenPos.xy * 2.0f) - 1.0f) * GeneralParams0.xy, -1,0), gViewInverse).xyz, 1.0f);	
	eyeRay.xyz /= eyeRay.w;

	float3 positionWorld = gViewInverse[3].xyz + (eyeRay.xyz * linearDepth);
	
	float posDiff = 1.0f - saturate(( length(positionWorld - IN.centerPos) - IN.tex.x * 0.5f) / (IN.tex.x * 0.5f) );
	float heightDiff = clamp(IN.centerPos.z - positionWorld.z,0.0f,0.1f) * 10.0f;
	
	half3 glowColor = pow(IN.col.rgb * IN.col.rgb, 1.0f/2.2f);
	float att = posDiff * heightDiff * IN.tex.y ;

	return CastOutHalf4Color(half4(glowColor.rgb, att * att));
}

OutHalf4Color PS_GameGlow(pixelInputGameGlow IN)
{
	float2 screenPos;
	screenPos.x			= IN.pos.x * gooScreenSize.x;
	screenPos.y			= IN.pos.y * gooScreenSize.y;

	float depthSample	= tex2D(DepthMapPointSampler, screenPos).x;
	float depth = getLinearGBufferDepth(depthSample, GeneralParams0.zw);
	
	return GameGlow(IN,depth);
}

#if __SHADERMODEL >= 40
OutHalf4Color PS_GameGlowAA(pixelInputGameGlow IN)
{
	float depthSample = RenderTexMSAA.Load(int2(IN.pos.xy),0).x;
	float depth = getLinearGBufferDepth(depthSample, GeneralParams0.zw);
	
	return GameGlow(IN,depth);
}
#endif // __SHADERMODEL >= 40

// ----------------------------------------------------------------------------------------------- //

struct vertexOutputFSGameGlow
{
	DECLARE_POSITION(pos)
	half2 tex        : TEXCOORD0;
	half3 col        : TEXCOORD2;
};

struct pixelInputFSGameGlow
{
	DECLARE_POSITION_PSIN(pos)
	half2 tex        : TEXCOORD0;
	half3 col        : TEXCOORD2;
};

vertexOutputFSGameGlow VS_FSGameGlow(rageVertexBlitIn IN)
{
	vertexOutputFSGameGlow OUT;

	OUT.pos = float4( IN.pos.xyz, 1.0f);
	OUT.col = IN.diffuse.rgb;
	OUT.tex	= IN.texCoord0.xy;
	
	return OUT;
}

// ----------------------------------------------------------------------------------------------- //
bool RaySphere(float3 ro, float3 rd, float3 sp, float sr, out float t)
{
    float3 os = ro - sp;
	float a = dot(rd, rd);
	float b = 2.0 * dot(rd, os);
    float c = dot(os, os) - (sr * sr);

    float disc = b * b - 4.0 * a * c;
    
    if (disc < 0.0)
	{
		t = 0.0;
        return false;
	}
	
    float distSqrt = sqrt(disc);
    float q;
    if (b < 0.0)
        q = (-b - distSqrt)/2.0;
    else
        q = (-b + distSqrt)/2.0;

    float t0 = q / a;
    float t1 = c / q;

    if (t0 > t1)
    {
        float temp = t0;
        t0 = t1;
        t1 = temp;
    }

    if (t1 < 0.0)
        return false;

    if (t0 < 0.0)
    {
        t = t1;
        return true;
    }
    else
    {
        t = t0;
        return true;
    }
}

OutHalf4Color FSGameGlow(pixelInputFSGameGlow IN, float linearDepth, bool drawSphere)
{
	float2 otherScreenPos;
	otherScreenPos.x	= IN.pos.x * gooScreenSize.x;
	otherScreenPos.y	= 1.0f - IN.pos.y * gooScreenSize.y;

#ifdef NVSTEREO
	float2 stereoParams = StereoParmsTexture.Load(int3(0,0,0)).xy;
	otherScreenPos = otherScreenPos - float2(stereoParams.x,0.0f);
#endif

	float4 eyeRay = float4(mul(float4(((otherScreenPos.xy * 2.0f) - 1.0f) * GeneralParams0.xy, -1,0), gViewInverse).xyz, 1.0f);	
	eyeRay.xyz /= eyeRay.w;
	
	float3 worldPosA = 	gViewInverse[3].xyz +StereoWorldCamOffSet() ;
	float3 worldPosB = worldPosA + (eyeRay.xyz * linearDepth);
	float3 worldPos = worldPosB;
	float3 spherePos = GeneralParams1.xyz;

	float opacity = 0.0f;
	
	if( drawSphere == false )
	{
		float dist = length(spherePos - worldPos);
		float passedDist = GeneralParams1.w < 0.0f ? dist > abs(GeneralParams1.w) : dist < abs(GeneralParams1.w);
		
		opacity = lerp(0.0f,IN.tex.x,passedDist);
	}
	else
	{
		float t;
		if( RaySphere(worldPosA, eyeRay.xyz, spherePos, abs(GeneralParams1.w), t) )
		{
			opacity = t < linearDepth ? IN.tex.x : 0.0f ;
		}
	}	
	
	return CastOutHalf4Color(float4(IN.col.rgb,opacity));

}

OutHalf4Color PS_FSGameGlow(pixelInputFSGameGlow IN)
{
	float2 screenPos;
	screenPos.x			= IN.pos.x * gooScreenSize.x;
	screenPos.y			= IN.pos.y * gooScreenSize.y;

	float depthSample	= tex2D(DepthMapPointSampler, screenPos).x;
	float depth = getLinearGBufferDepth(depthSample, GeneralParams0.zw);
	
	return FSGameGlow(IN,depth, false);
}

#if __SHADERMODEL >= 40
OutHalf4Color PS_FSGameGlowAA(pixelInputFSGameGlow IN)
{
	float depthSample = RenderTexMSAA.Load(int2(IN.pos.xy * TexelSize.xy),0).x;
	float depth = getLinearGBufferDepth(depthSample, GeneralParams0.zw);
	
	return FSGameGlow(IN,depth, false);
}
#endif // __SHADERMODEL >= 40

OutHalf4Color PS_FSGameGlowMarker(pixelInputFSGameGlow IN)
{
	float2 screenPos;
	screenPos.x			= IN.pos.x * gooScreenSize.x;
	screenPos.y			= IN.pos.y * gooScreenSize.y;

	float depthSample	= tex2D(DepthMapPointSampler, screenPos).x;
	float depth = getLinearGBufferDepth(depthSample, GeneralParams0.zw);
	
	return FSGameGlow(IN,depth, true);
}

#if __SHADERMODEL >= 40
OutHalf4Color PS_FSGameGlowAAMarker(pixelInputFSGameGlow IN)
{
	float depthSample = RenderTexMSAA.Load(int2(IN.pos.xy * TexelSize.xy),0).x;
	float depth = getLinearGBufferDepth(depthSample, GeneralParams0.zw);
	
	return FSGameGlow(IN,depth, true);
}
#endif // __SHADERMODEL >= 40

// ----------------------------------------------------------------------------------------------- //
float GetExposure()
{
#if !(__WIN32PC || RSG_ORBIS) // TODO - Orbis - Are we setting this properly
	return refMipBlurParams.z;
#else
	return 1.0f;
#endif // !__WIN32PC
}

// ----------------------------------------------------------------------------------------------- //

float GetGamma()
{
#if !(__WIN32PC || RSG_ORBIS) // TODO - Orbis - Are we setting this properly
	return refMipBlurParams.w;
#else
	return 1.0f / 2.2f;
#endif // !__WIN32PC
}

// ----------------------------------------------------------------------------------------------- //

float3 ApplyColorFilter(float3 tonemappedCol)
{
	float brightness = dot(tonemappedCol, tonemapColorFilter_BWWeights);
	float3 filterCol = brightness*tonemapColorFilter_TintColor;
	tonemappedCol = lerp(tonemappedCol, filterCol, saturate(tonemapColorFilter_Blend));
	return tonemappedCol;
}

// ----------------------------------------------------------------------------------------------- //

OutHalf4Color PS_ToneMap(rageVertexBlit IN)
{
	half4 sampleC = UnpackHdr(tex2DOffsetXenonOnly(RenderMapPointSampler, IN.texCoord0, .5));

	half4 toneMappedCol;
	toneMappedCol.rgb = filmicToneMap(sampleC.rgb, GeneralParams0, GeneralParams1, GetExposure());
	toneMappedCol.a = 1.0f;

	toneMappedCol.rgb = ApplyColorFilter(toneMappedCol.rgb);

	toneMappedCol.rgb = pow(toneMappedCol.rgb, GetGamma());

	return CastOutHalf4Color(half4(toneMappedCol));
}

// ----------------------------------------------------------------------------------------------- //

OutHalf4Color PS_ToneMapWithAlpha(rageVertexBlit IN)
{
	half4 sampleC = UnpackHdr(tex2DOffsetXenonOnly(RenderMapPointSampler, IN.texCoord0, .5));

	half4 toneMappedCol;
	toneMappedCol.rgb = filmicToneMap(sampleC.rgb, GeneralParams0, GeneralParams1, GetExposure());
	toneMappedCol.a = sampleC.a;

	toneMappedCol.rgb = pow(toneMappedCol.rgb, GetGamma());

	return CastOutHalf4Color(half4(toneMappedCol));
}

// ----------------------------------------------------------------------------------------------- //
OutHalf4Color PS_LuminanceToAlpha(rageVertexBlit IN)
{
	half4 sampleC = tex2DOffsetXenonOnly(RenderMapPointSampler, IN.texCoord0, .5f);
	sampleC.a = dot(sampleC.rgb, float3(0.299, 0.587, 0.114));

	return CastOutHalf4Color(half4(sampleC));
}

// ----------------------------------------------------------------------------------------------- //
struct SimpleShadowVertexOutput
{
	DECLARE_POSITION(pos)
};

struct SimpleShadowVertexInput
{
	float3 pos: POSITION;
};


SimpleShadowVertexOutput VS_TransformShadowModel( SimpleShadowVertexInput IN 
#ifdef RAGE_ENABLE_OFFSET
	, rageVertexOffsetBump INOFFSET, uniform float bEnableOffsets
#endif
	)
{
	SimpleShadowVertexOutput OUT;
	float3 inPos = IN.pos;
#ifdef RAGE_ENABLE_OFFSET
	if( bEnableOffsets != 0.0f )
	{
		inPos += INOFFSET.pos;		
	}
#endif //RAGE_ENABLE_OFFSET...
	OUT.pos      = mul(float4(inPos, 1.f), gWorldViewProj);
	return OUT;
}

#if __SHADERMODEL >= 40
PSoutMrt4 PS_DeferredSplat(SimpleShadowVertexOutput IN) 
{
	PSoutMrt4 OUT;

	OUT.color0 = half4(GeneralParams0);
	OUT.color1 = half4(GeneralParams1);
	OUT.color2 = half4(GeneralParams0);
	OUT.color3 = half4(GeneralParams1);

	return OUT;
}
#endif //__SHADERMODEL

// ----------------------------------------------------------------------------------------------- //

#if MULTIHEAD_FADE
//---	Radial fade CODE	---//

vertexOutputRadial VS_RadialFade(float2 pos: POSITION)
{
    vertexOutputRadial OUT;

	//adjust from viewport space (0 to 1) to screen space (-1 to 1)
    OUT.pos	= float4( 2.0f*pos-float2(1,1), 0,1 );

	OUT.tex = float4( pos, 0,1 );

	return OUT;
}

OutFloat4Color PS_RadialFade(pixelInputRadial IN)
{
	const float2 off = abs(IN.tex.xy - RadialFadeArea.xy);
	rageDiscard( all(off<RadialFadeArea.zw) );
	
	const float dir = RadialFadeTime.w;
	const float time = dir + (1-2*dir)*RadialFadeTime.x;

	const float2 border = lerp( RadialFadeArea.zw, float2(1,1), saturate(time) );
	const float aspect = border.x / border.y;
	const float alpha = RadialWaveParam.y +
		RadialWaveParam.x *	(max(off.x,off.y*aspect) - border.x);
	return CastOutFloat4Color(float4( 0,0,0, alpha ));
}
#endif	//MULTIHEAD_FADE


// ----------------------------------------------------------------------------------------------- //
#if USE_HQ_ANTIALIASING

struct HQAAVertexOutput 
{
	DECLARE_POSITION(pos)
	float2 p    : TEXCOORD0;
	float4 pPos : TEXCOORD1;
};

HQAAVertexOutput VS_HQAA(rageVertexInput IN)
{
	HQAAVertexOutput OUT;

	OUT.pos = float4(IN.pos, 1.0f);
	float2 tc = IN.texCoord0;

	OUT.p = (OUT.pos.xy * 0.5f) + 0.5f;
	OUT.p = float2(OUT.p.x, 1.0f - OUT.p.y);
	OUT.pPos.xy = OUT.p - gooScreenSize.xy * 0.5f;
	OUT.pPos.zw = OUT.p + gooScreenSize.xy * 0.5f;
	return OUT;
}

OutHalf4Color PS_HQAA ( HQAAVertexOutput IN )
{
#if __PS3
	float2 vPixelSize = float2(1.0/1280.0, 1.0/720.0);// gooScreenSize.xy; // Save 3 cycles on PS3 by using hard coded pixel size
#else
	float2 vPixelSize = gooScreenSize.xy;
#endif // __PS3
	// Using TransparentSrcMapSampler because we need UV clamping + bilinear filtering
#if FXAA_HLSL_4 || FXAA_HLSL_5
	struct FxaaTex AA_INPUT = { TransparentSrcMapSampler, TransparentSrcMap };
#else
	#define AA_INPUT	TransparentSrcMapSampler
#endif
#if FXAA_HLSL_5
	return CastOutHalf4Color(FxaaPixelShader(	IN.p, IN.pPos, AA_INPUT, vPixelSize,true ));
#else
	return CastOutHalf4Color(FxaaPixelShader(	IN.p, IN.pPos, AA_INPUT, vPixelSize,false ));
#endif
#undef AA_INPUT
}


struct VertexTileDataInput
{
#if __PS3
	half2	pos					: POSITION;
	half4   data				: TEXCOORD0;	
#elif __XENON
	int4	pos					: POSITION;
	half4   data				: TEXCOORD0;	
#else
	half4	pos					: POSITION;
#endif
};
// Extract light classification data
float4 GetTileData(VertexTileDataInput IN)
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
HQAAVertexOutput VS_HQAA_Tiled_SkyOnly(VertexTileDataInput IN, uniform bool skyOnly)
{
	HQAAVertexOutput OUT = (HQAAVertexOutput)0 ;
	

	float2 pos = IN.pos.xy;
	#if __XENON
		pos *= globalScreenSize.zw;
	#endif

	float4 tileData = GetTileData(IN);
	OUT.pos	= float4((pos - 0.5f) * 2.0f, 1., 1); 


	OUT.p = (OUT.pos.xy * 0.5f) + 0.5f;
	OUT.p = float2(OUT.p.x, 1.0f - OUT.p.y);
	OUT.pPos.xy = OUT.p - gooScreenSize.xy * 0.5f;
	OUT.pPos.zw = OUT.p + gooScreenSize.xy * 0.5f;

	OUT.pos *= (tileData.z <0 );
	
	return OUT;
}
HQAAVertexOutput VS_HQAA_Tiled(VertexTileDataInput IN, uniform bool skyOnly)
{
	HQAAVertexOutput OUT = (HQAAVertexOutput)0 ;
	

	float2 pos = IN.pos.xy;
	#if __XENON
		pos *= globalScreenSize.zw;
	#endif

	float4 tileData = GetTileData(IN);
	OUT.pos	= float4((pos - 0.5f) * 2.0f, 1., 1); 


	OUT.p = (OUT.pos.xy * 0.5f) + 0.5f;
	OUT.p = float2(OUT.p.x, 1.0f - OUT.p.y);
	OUT.pPos.xy = OUT.p - gooScreenSize.xy * 0.5f;
	OUT.pPos.zw = OUT.p + gooScreenSize.xy * 0.5f;

	const float distanceBlurTest = (tileData.x >= GeneralParams0.x);
	const float noDistanceBlurTest = (tileData.x < GeneralParams0.x);
	OUT.pos *= 	lerp(distanceBlurTest, noDistanceBlurTest, GeneralParams0.w);
	OUT.pos *=(tileData.z >=0 );

	return OUT;
}




OutFloat4Color PS_UIAA(HQAAVertexOutput IN)
{
	float2 texelSize = float2(1.0/1280, 1.0/720);
	float4 xxyy;
	float2 tex	= IN.p;
	float scale = 0.2;
	float3 ne	= tex2D(TransparentSrcMapSampler, tex + float2( texelSize.x*scale,  texelSize.y*scale)).a;
	float3 nw	= tex2D(TransparentSrcMapSampler, tex + float2(-texelSize.x*scale,  texelSize.y*scale)).a;
	float3 sw	= tex2D(TransparentSrcMapSampler, tex + float2(-texelSize.x*scale, -texelSize.y*scale)).a;
	float3 se	= tex2D(TransparentSrcMapSampler, tex + float2( texelSize.x*scale, -texelSize.y*scale)).a;
	float str	= 1;
	float2 bump = float2(dot(ne - sw, str), dot(nw - se, str));
	float dx	= dot(normalize(float2( 1,  1)), bump);
	float dy	= dot(normalize(float2(-1,  1)), bump);
	float len	= saturate(length(float2(dx, dy))*10 - .3);	
	float2 dxdy = normalize(float2(dx, dy))*len*texelSize;
	float4 color = 0;
	color += tex2D(TransparentSrcMapSampler, tex - 3.5*dxdy);
	color += tex2D(TransparentSrcMapSampler, tex - 1.5*dxdy);
	color += tex2D(TransparentSrcMapSampler, tex);
	color += tex2D(TransparentSrcMapSampler, tex + 1.5*dxdy);
	color += tex2D(TransparentSrcMapSampler, tex + 3.5*dxdy);
	return CastOutFloat4Color(color/5);
}

OutHalf4Color PS_HQAA_Blit( HQAAVertexOutput IN )
{
	return CastOutHalf4Color(tex2D(TransparentSrcMapSampler, IN.p));
}
#endif // USE_HQ_ANTIALIASING


// =============================================================================================== //
//	TECHNIQUES
// =============================================================================================== //

//#if FORWARD_TECHNIQUES
technique draw
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER VS_Transform(gbOffsetEnable);
		PixelShader  = compile PIXELSHADER PS_Textured_IM()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
//#endif // FORWARD_TECHNIQUES

// ----------------------------------------------------------------------------------------------- //

//#if FORWARD_TECHNIQUES && DRAWSKINNED_TECHNIQUES
technique drawskinned
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER VS_TransformSkin(gbOffsetEnable);
		PixelShader  = compile PIXELSHADER PS_Textured_IM()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
//#endif // FORWARD_TECHNIQUES && DRAWSKINNED_TECHNIQUES

// ----------------------------------------------------------------------------------------------- //

//#if UNLIT_TECHNIQUES
technique unlit_draw
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER VS_TransformUnlit_IM(gbOffsetEnable);
		PixelShader  = compile PIXELSHADER PS_TexturedUnlit_IM()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
//#endif // UNLIT_TECHNIQUES

// ----------------------------------------------------------------------------------------------- //

//#if UNLIT_TECHNIQUES && DRAWSKINNED_TECHNIQUES
technique unlit_drawskinned
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER VS_TransformSkinUnlit(gbOffsetEnable);
		PixelShader  = compile PIXELSHADER PS_TexturedUnlit_IM()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
//#endif // UNLIT_TECHNIQUES && DRAWSKINNED_TECHNIQUES

// ----------------------------------------------------------------------------------------------- //

technique blit_draw
{
	pass p0 // Default
	{
		VertexShader = compile VERTEXSHADER VS_Blit();
		PixelShader  = compile PIXELSHADER PS_Blit()  CGC_FLAGS(CGC_DEFAULTFLAGS_NPC(1));
	}
}

// ----------------------------------------------------------------------------------------------- //
#if __SHADERMODEL >= 40
technique gbuffersplat
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER VS_TransformShadowModel();
		PixelShader  = compile PIXELSHADER PS_DeferredSplat()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
#endif //__SHADERMODEL

// ----------------------------------------------------------------------------------------------- //
#if __PS3
// ----------------------------------------------------------------------------------------------- //

technique packed_draw
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER VS_Transform(gbOffsetEnable);
		PixelShader  = compile PIXELSHADER PS_Textured_IM_PACKED()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}

// ----------------------------------------------------------------------------------------------- //

technique packed_unlit_draw
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER VS_TransformUnlit(gbOffsetEnable);
		PixelShader  = compile PIXELSHADER PS_TexturedUnlit_IM_PACKED()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}

// ----------------------------------------------------------------------------------------------- //
#endif // __PS3

#if __PS3 || RSG_ORBIS || __D3D11



// ----------------------------------------------------------------------------------------------- //


#endif // __PS3 || RSG_ORBIS

technique gtadrawblit
{
	pass p0 // original, kept around as the VS is multiplying by the world view.
	{
		VertexShader = compile VERTEXSHADER VS_GTABlit();
		PixelShader  = compile PIXELSHADER PS_Blit()  CGC_FLAGS(CGC_DEFAULTFLAGS_NPC(1));
	}

	pass p1 // default
	{
		VertexShader = compile VERTEXSHADER VS_Blit();
		PixelShader  = compile PIXELSHADER PS_Blit()  CGC_FLAGS(CGC_DEFAULTFLAGS_NPC(1));
	}

	pass p2 // no bilinear (useful for FP16 target)
	{
		VertexShader = compile VERTEXSHADER VS_Blit();
		PixelShader  = compile PIXELSHADER PS_BlitNoBilinear()  CGC_FLAGS(CGC_DEFAULTFLAGS_NPC(1));
	}

	pass p3 // depth to color
	{
		VertexShader = compile VERTEXSHADER VS_Blit();
		PixelShader  = compile PIXELSHADER PS_BlitDepthToColor()  CGC_FLAGS(CGC_DEFAULTFLAGS_NPC(1));
	}

	pass p4 // blit stencil to color
	{
		VertexShader = compile VERTEXSHADER VS_Blit();
		PixelShader  = compile PIXELSHADER PS_BlitStencilToColor()  CGC_FLAGS(CGC_DEFAULTFLAGS_NPC(1));
	}

	pass p5
	{
		VertexShader = compile VERTEXSHADER VS_Blit();
		PixelShader  = compile PIXELSHADER PS_BlitShowAlpha()  CGC_FLAGS(CGC_DEFAULTFLAGS_NPC(1));
	}

	pass p6 // reads from sRGB target and outputs to non-sRGB target (but keep colour in gamma space) 
	{
		VertexShader = compile VERTEXSHADER VS_Blit();
		PixelShader  = compile PIXELSHADER PS_BlitGamma()  CGC_FLAGS(CGC_DEFAULTFLAGS_NPC(1));
	}

	pass p7
	{
		VertexShader = compile VERTEXSHADER VS_Blit();
		PixelShader  = compile PIXELSHADER PS_BlitPow()  CGC_FLAGS(CGC_DEFAULTFLAGS_NPC(1));
	}

	pass p8
	{
		VertexShader = compile VERTEXSHADER VS_Blit();
		PixelShader  = compile PIXELSHADER PS_BlitCalibration()  CGC_FLAGS(CGC_DEFAULTFLAGS_NPC(1));
	}

	pass p9
	{
		VertexShader = compile VERTEXSHADER VS_Blit();
		PixelShader  = compile PIXELSHADER PS_BlitCube()  CGC_FLAGS(CGC_DEFAULTFLAGS_NPC(1));
	}

	pass p10
	{
		VertexShader = compile VERTEXSHADER	VS_BlitParaboloid();
		PixelShader  = compile PIXELSHADER	PS_BlitParaboloid()  CGC_FLAGS(CGC_DEFAULTFLAGS_NPC(1));
	}

	pass p11
	{
		VertexShader = compile VERTEXSHADER	VS_BlitAA();
		PixelShader  = compile PIXELSHADER	PS_BlitAA()  CGC_FLAGS(CGC_DEFAULTFLAGS_NPC(1));
	}

	pass p12
	{
		VertexShader = compile VERTEXSHADER	VS_BlitAA();
		PixelShader  = compile PIXELSHADER	PS_BlitAAAlpha()  CGC_FLAGS(CGC_DEFAULTFLAGS_NPC(1));
	}

	pass p13
	{
		VertexShader = compile VERTEXSHADER	VS_Blit();
		PixelShader  = compile PIXELSHADER	PS_BlitColour()  CGC_FLAGS(CGC_DEFAULTFLAGS_NPC(1));
	}
	
	pass p14
	{
		VertexShader = compile VERTEXSHADER	VS_GTABlit();
		PixelShader  = compile PIXELSHADER	PS_BlitToAlpha()  CGC_FLAGS(CGC_DEFAULTFLAGS_NPC(1));
	}
	
	pass p15
	{
		VertexShader = compile VERTEXSHADER	VS_GTABlit();
		PixelShader  = compile PIXELSHADER	PS_BlitToAlphaBlur()  CGC_FLAGS(CGC_DEFAULTFLAGS_NPC(1));
	}
	
	pass p16
	{
		VertexShader = compile VERTEXSHADER	VS_Blit();
		PixelShader  = compile PIXELSHADER	PS_BlitDepthToAlpha()  CGC_FLAGS(CGC_DEFAULTFLAGS_NPC(1));
	}

	pass p17
	{
		VertexShader = compile VERTEXSHADER	VS_Blit();
		PixelShader  = compile PIXELSHADER	PS_FOWCount()  CGC_FLAGS(CGC_DEFAULTFLAGS_NPC(1))	PS4_TARGET(FMT_32_R);
	}

	pass p18
	{
		VertexShader = compile VERTEXSHADER	VS_Blit();
		PixelShader  = compile PIXELSHADER	PS_FOWAverage()  CGC_FLAGS(CGC_DEFAULTFLAGS_NPC(1))	PS4_TARGET(FMT_32_R);
	}

	pass p19
	{
		VertexShader = compile VERTEXSHADER	VS_GTABlit();
		PixelShader  = compile PIXELSHADER	PS_FOWFill()  CGC_FLAGS(CGC_DEFAULTFLAGS_NPC(1))	PS4_TARGET(FMT_32_R);
	}
	
	// if you are changing order of shader pass, make appropriate change in sprite2d.h/cpp
#if __SHADERMODEL >= 40
# if RSG_ORBIS
#  define	PS41	ps_5_0
# else
#  define	PS41	ps_4_1
# endif
	pass p20
	{
		VertexShader = compile VERTEXSHADER VS_Blit();
		PixelShader  = compile PIXELSHADER PS_BlitTexArray()  CGC_FLAGS(CGC_DEFAULTFLAGS_NPC(1));
	}
	pass p21_sm41
	{
		VertexShader = compile VERTEXSHADER VS_Blit();
		PixelShader  = compile PS41 PS_BlitMsaaDepth()  CGC_FLAGS(CGC_DEFAULTFLAGS_NPC(1));
	}
	pass p22_sm41
	{
		VertexShader = compile VERTEXSHADER VS_Blit();
		PixelShader  = compile PS41 PS_BlitMsaaStencil()  CGC_FLAGS(CGC_DEFAULTFLAGS_NPC(1));
	}
	pass p23_sm41
	{
		VertexShader = compile VERTEXSHADER VS_Blit();
		PixelShader  = compile PS41 PS_BlitMsaa()  CGC_FLAGS(CGC_DEFAULTFLAGS_NPC(1));
	}
	pass p24_sm41
	{
		VertexShader = compile VERTEXSHADER VS_Blit();
		PixelShader  = compile PS41 PS_BlitFmask()  CGC_FLAGS(CGC_DEFAULTFLAGS_NPC(1));
	}
#endif // __SHADERMODEL
#if RSG_ORBIS
	pass p25 // blit and swizzle x/z
	{
		VertexShader = compile VERTEXSHADER VS_Blit();
		PixelShader  = compile PIXELSHADER PS_BlitXZSwizzle()  CGC_FLAGS(CGC_DEFAULTFLAGS_NPC(1));
	}
#endif
}

// ----------------------------------------------------------------------------------------------- //

// like gtadrawblit, but no texturing:
technique gtadrawblit_do
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER VS_GTABlit();
		PixelShader  = compile PIXELSHADER PS_BlitDiffuse()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}

// ----------------------------------------------------------------------------------------------- //

technique DownSampleMinDepth
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER VS_Blit();
		PixelShader  = compile PIXELSHADER PS_DownsampleMinDepth()  CGC_FLAGS(CGC_DEFAULTFLAGS_NPC(3));
	}
}

// ----------------------------------------------------------------------------------------------- //

technique BlitTransparentBilinearBlur
{
	pass P0
	{
		VertexShader = compile VERTEXSHADER VS_Blit();
		PixelShader  = compile PIXELSHADER PS_BlitTransparentBilinearBlur()  CGC_FLAGS(CGC_DEFAULTFLAGS_NPC(1));
	}
}
// ----------------------------------------------------------------------------------------------- //

technique BlitTransparentBilinearBilateralWithAlphaDiscard
{
	pass P0
	{
		VertexShader = compile VERTEXSHADER VS_Blit();
		PixelShader  = compile PIXELSHADER PS_BlitTransparentBilinearBilateralWithAlphaDiscard()  CGC_FLAGS(CGC_DEFAULTFLAGS_NPC(3));
	}
}

technique BlitTransparentBilinearBilateral
{
	pass P0
	{
		VertexShader = compile VERTEXSHADER VS_Blit();
		PixelShader  = compile PIXELSHADER PS_BlitTransparentBilinearBilateral()  CGC_FLAGS(CGC_DEFAULTFLAGS_NPC(3));
	}
}

// ----------------------------------------------------------------------------------------------- //
technique BlitTransparentBilateralBlur
{
    pass P0
    {
        VertexShader = compile VERTEXSHADER VS_Blit();
        PixelShader  = compile PIXELSHADER PS_BlitTransparentBilateralBlur()  CGC_FLAGS(CGC_DEFAULTFLAGS_NPC(1));
    }
}
// ----------------------------------------------------------------------------------------------- //

technique corona
{
	pass p0 // CS_CORONA_DEFAULT
	{
		VertexShader = compile VERTEXSHADER VS_corona();
		PixelShader  = compile PIXELSHADER PS_corona()  CGC_FLAGS(CGC_DEFAULTFLAGS); 
	}

	pass p1 // CS_CORONA_DEFAULT (no near fade)
	{
		VertexShader = compile VERTEXSHADER VS_corona_nofade();
		PixelShader  = compile PIXELSHADER PS_corona()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p2 // CS_CORONA_DEFAULT (show occlusion)
	{
#if defined(SHADER_FINAL)
		VertexShader = compile VERTEXSHADER VS_corona();
		PixelShader  = compile PIXELSHADER PS_corona()  CGC_FLAGS(CGC_DEFAULTFLAGS); 
#else
		VertexShader = compile VERTEXSHADER VS_corona_dbg_BANK_ONLY();
		PixelShader  = compile PIXELSHADER PS_corona_dbg_BANK_ONLY()  CGC_FLAGS(CGC_DEFAULTFLAGS);
#endif
	}

	pass p3 // CS_CORONA_SIMPLE_REFLECTION (RM_WATER_REFLECTION, RM_MIRROR_REFLECTION)
	{
		VertexShader = compile VERTEXSHADER VS_corona_simple_reflection();
		PixelShader  = compile PIXELSHADER PS_corona_simple_reflection()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p4 // CS_CORONA_PARABOLOID_REFLECTION
	{
		VertexShader = compile VERTEXSHADER VS_corona_paraboloid_reflection();
		PixelShader  = compile PIXELSHADER PS_corona_simple_reflection()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p5 // CS_CORONA_LIGHTNING (no near fade, no depth test)
	{
		VertexShader = compile VERTEXSHADER VS_corona_nofade_nodepth();
		PixelShader = compile PIXELSHADER PS_corona()  CGC_FLAGS(CGC_DEFAULTFLAGS); 

	}
}

#if GS_INSTANCED_CUBEMAP
GEN_GSINST_TYPE(Corona,vertexCoronaOUT,SV_RenderTargetArrayIndex)
GEN_GSINST_FUNC(Corona,Corona,vertexCoronaIN,vertexCoronaOUT,vertexCoronaOUT,VS_corona_simple_reflection_common(IN,true),PS_corona_simple_reflection(IN))
technique corona_cubeinst_draw
{
	GEN_GSINST_TECHPASS(Corona,cubeinst)
}
#endif

// ----------------------------------------------------------------------------------------------- //

technique colorcorrectedunlit
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER VS_ColorCorrectedUnlit();
		PixelShader  = compile PIXELSHADER PS_ColorCorrectedUnlit()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}

// ----------------------------------------------------------------------------------------------- //

technique colorcorrectedunlitgradient
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER VS_ColorCorrectedUnlit();
		PixelShader  = compile PIXELSHADER PS_ColorCorrectedUnlitGradient()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}

// ----------------------------------------------------------------------------------------------- //

technique seethrough
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER VS_Transform_IM_SeeThrough(gbOffsetEnable);
		PixelShader  = compile PIXELSHADER PS_IM_SeeThrough()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}

// ----------------------------------------------------------------------------------------------- //

technique refMipBlur
{
	pass pass_refMipBlur_CubeToParab
	{
		VertexShader = compile VERTEXSHADER VS_refMipBlurCubeToParabT();
		PixelShader  = compile PIXELSHADER PS_refMipBlurCubeToParab() CGC_FLAGS(CGC_DEFAULTFLAGS_NPC_HALF(1));
	}

	pass pass_refMipBlur_1
	{
		VertexShader = compile VERTEXSHADER VS_screenTransformT();
		PixelShader  = compile PIXELSHADER PS_refMipBlur_1() CGC_FLAGS(CGC_DEFAULTFLAGS_NPC_HALF(1));
	}

	pass pass_refMipBlur_2
	{
		VertexShader = compile VERTEXSHADER VS_screenTransformT();
		PixelShader  = compile PIXELSHADER PS_refMipBlur_2() CGC_FLAGS(CGC_DEFAULTFLAGS_NPC_HALF(1));
	}

	pass pass_refMipBlur_3
	{
		VertexShader = compile VERTEXSHADER VS_screenTransformT();
		PixelShader  = compile PIXELSHADER PS_refMipBlur_3() CGC_FLAGS(CGC_DEFAULTFLAGS_NPC_HALF(3));
	}

	pass pass_refMipBlur_4
	{
		VertexShader = compile VERTEXSHADER VS_screenTransformT();
		PixelShader  = compile PIXELSHADER PS_refMipBlur_4() CGC_FLAGS(CGC_DEFAULTFLAGS_NPC_HALF(1));
	}

	pass pass_refMipBlur_5
	{
		VertexShader = compile VERTEXSHADER VS_screenTransformT();
		PixelShader  = compile PIXELSHADER PS_refMipBlur_5() CGC_FLAGS(CGC_DEFAULTFLAGS_NPC_HALF(1));
	}

	pass pass_refMipBlur_6
	{
		VertexShader = compile VERTEXSHADER VS_screenTransformT();
		PixelShader  = compile PIXELSHADER PS_refMipBlur_6() CGC_FLAGS(CGC_DEFAULTFLAGS_NPC_HALF(1));
	}

	pass pass_refMipBlur_N // pass 6 - independent blurSize
	{
		VertexShader = compile VERTEXSHADER VS_screenTransformT();
		PixelShader  = compile PIXELSHADER PS_refMipBlur_N() CGC_FLAGS(CGC_DEFAULTFLAGS_NPC_HALF(1));
	}

	pass pass_refMipBlur_P // pass 7 - use poisson filter
	{
		VertexShader = compile VERTEXSHADER VS_screenTransformT();
		PixelShader  = compile PIXELSHADER PS_refMipBlur_P() CGC_FLAGS(CGC_DEFAULTFLAGS_NPC_HALF(1));
	}
}

// ----------------------------------------------------------------------------------------------- //

technique refMipBlurBlend
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER VS_screenTransformT();
		PixelShader  = compile PIXELSHADER PS_refMipBlurBlend()  CGC_FLAGS(CGC_DEFAULTFLAGS_NPC(1));
	}
}

// ----------------------------------------------------------------------------------------------- //
#if __SHADERMODEL >= 40
// ----------------------------------------------------------------------------------------------- //

float4 mipMapFunc(vertexOutputLPT IN, float mipLevel)
{
	return RenderTexArray.SampleLevel(RenderTexArraySampler, float3(IN.tex.xy, refMipBlurParams.y), mipLevel);
}

OutHalf4Color PS_mipMap(vertexOutputLPT IN) { return CastOutHalf4Color(mipMapFunc(IN, refMipBlurParams_MipIndex)); }
OutHalf4Color PS_mipMap_Unorm(vertexOutputLPT IN) { return CastOutHalf4Color(mipMapFunc(IN, refMipBlurParams_MipIndex)); }

OutHalf4Color PS_mipMap_Cube(vertexOutputLPT IN) 
{ 
	// no need for normalize if sampling directly
	float3 dir = normalize(gViewInverse[2].xyz - IN.tex.x * gViewInverse[0].xyz + IN.tex.y * gViewInverse[1].xyz); 

	int mipLevel = int(refMipBlurParams_MipIndex);

	float3 cubeResult = texCUBElod(RenderCubeMapSampler, float4(dir, refMipBlurParams_MipIndex));
	float4 finalColour = float4(cubeResult, 1.0f);

	return CastOutHalf4Color(finalColour); 
}

technique mipMap
{
	pass pass_mipMap
	{
		VertexShader = compile VERTEXSHADER VS_screenTransformT();
		PixelShader  = compile PIXELSHADER PS_mipMap() CGC_FLAGS(CGC_DEFAULTFLAGS_NPC_HALF(1));
	}

	pass pass_mipMap_unorm
	{
		VertexShader = compile VERTEXSHADER VS_screenTransformT();
		PixelShader  = compile PIXELSHADER PS_mipMap_Unorm() CGC_FLAGS(CGC_DEFAULTFLAGS_NPC_HALF(1))	PS4_TARGET(FMT_UNORM16_ABGR);
	}

	pass pass_mipMap_cube
	{
		VertexShader = compile VERTEXSHADER VS_screenTransformT();
		PixelShader  = compile PIXELSHADER PS_mipMap_Cube() CGC_FLAGS(CGC_DEFAULTFLAGS_NPC_HALF(1));
	}
}

// ----------------------------------------------------------------------------------------------- //
#endif
// ----------------------------------------------------------------------------------------------- //

OutHalf4Color PS_VisualizeSignedTexture(rageVertexBlit IN)
{
	float4 color = h4tex2D(DiffuseSampler, IN.texCoord0 );
	color = color * 0.5 + 0.5;
	return CastOutHalf4Color(color);
}

technique visualizeSignedTexture
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER VS_Blit();
		PixelShader  = compile PIXELSHADER PS_VisualizeSignedTexture() CGC_FLAGS(CGC_DEFAULTFLAGS_NPC_HALF(1));
	}
}

float4 VS_reflectionSkyPortal(float2 xy : POSITION) : SV_Position
{
	return float4(xy, 0, 1);
}

technique refSkyPortal
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER VS_reflectionSkyPortal();
		COMPILE_PIXELSHADER_NULL()
	}
}

// ----------------------------------------------------------------------------------------------- //

#if __PS3
technique psnResolves
{
	pass FlickerFilter // Anti flicker filter for SD interlaced resolutions
	{
		AlphaBlendEnable = false;
		AlphaTestEnable = false;
		CullMode = NONE;
		ZEnable = false;
		ZWriteEnable = false;
		VertexShader = compile VERTEXSHADER VS_screenTransformT();
		PixelShader  = compile PIXELSHADER PS_SDDownsample()  CGC_FLAGS(CGC_DEFAULTFLAGS_NPC(1));
	}

	pass DepthToVS // To resolve a depth buffer to a VS friendly 32bit float
	{
		AlphaBlendEnable = false;
		AlphaTestEnable = false;
		VertexShader = compile VERTEXSHADER VS_Blit();
		PixelShader  = compile PIXELSHADER PS_BlitDepthToColor()  CGC_FLAGS(CGC_DEFAULTFLAGS_NPC(1));
	}

	pass AlphaStencil // Compositing resolve. Uses stencil to potentially overide alpha
	{
		AlphaBlendEnable = true;
		AlphaTestEnable = false;
		SrcBlend = ONE;
		DestBlend = INVSRCALPHA;
		BlendOp = ADD;

		VertexShader = compile VERTEXSHADER VS_Blit();
		PixelShader  = compile PIXELSHADER PS_AlphaStencil()  CGC_FLAGS(CGC_DEFAULTFLAGS_NPC(1));
	}

}
#endif // __PS3

// ----------------------------------------------------------------------------------------------- //

#if __PS3
technique ZCullReload
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER VS_screenTransform();
		PixelShader  = compile PIXELSHADER PS_SCullReload()  CGC_FLAGS(CGC_DEFAULTFLAGS_NPC(1));
	}

	pass p1
	{
		VertexShader = compile VERTEXSHADER VS_screenTransform();
		PixelShader  = compile PIXELSHADER PS_ZCullReload()  CGC_FLAGS(CGC_DEFAULTFLAGS_NPC(1));
	}

	pass p2
	{
		VertexShader = compile VERTEXSHADER VS_screenTransformGreaterThan();
		PixelShader  = compile PIXELSHADER PS_ZCullReloadGreaterThan()  CGC_FLAGS(CGC_DEFAULTFLAGS_NPC(1));
	}
	
	pass p3
	{
		VertexShader = compile VERTEXSHADER VS_screenTransformNearZ();
		PixelShader  = compile PIXELSHADER PS_SCullReload()  CGC_FLAGS(CGC_DEFAULTFLAGS_NPC(1));
	}
}
#endif // __PS3

// ----------------------------------------------------------------------------------------------- //

technique AlphaMaskToStencilMask
{
	pass p0
	{	
		VertexShader = compile VERTEXSHADER VS_screenTransformZT();
		PixelShader  = compile PIXELSHADER PS_AlphaMaskToStencilMask()  CGC_FLAGS(CGC_DEFAULTFLAGS_NPC_HALF_REG(1,10));
	}
#if __XENON
	pass p1
	{
		VertexShader = compile VERTEXSHADER VS_screenTransformZT();
		PixelShader  = compile PIXELSHADER PS_AlphaMaskToStencilMask_StencilAsColor()  CGC_FLAGS(CGC_DEFAULTFLAGS_NPC_HALF_REG(1,10));
		
	}
#endif
}

// ----------------------------------------------------------------------------------------------- //
#if __XENON
technique CopyGBufferDepth
{
	pass p0
	{
		CullMode = NONE;

		AlphaBlendEnable = false;
		AlphaTestEnable = false;

		VertexShader = compile VERTEXSHADER VS_screenTransform();
		PixelShader  = compile PIXELSHADER PS_depthCopy()  CGC_FLAGS(CGC_DEFAULTFLAGS_NPC(1));
	}

	pass p1
	{
		VertexShader = compile VERTEXSHADER VS_screenTransformSS();
		PixelShader  = compile PIXELSHADER PS_depthStencilCopy()  CGC_FLAGS(CGC_DEFAULTFLAGS_NPC(1));
	}
}
#endif

// ----------------------------------------------------------------------------------------------- //

#if __XENON
technique RestoreHiZ
{
	pass p0 //z restore on 360 and ps3 depth copy
	{
	#if __XENON
		VertexShader = compile VERTEXSHADER VS_screenTransform();
		PixelShader  = NULL;
	#elif __PS3
		VertexShader = compile VERTEXSHADER VS_screenTransform();
		PixelShader  = compile PIXELSHADER PS_depthCopyEx()  CGC_FLAGS(CGC_DEFAULTFLAGS_NPC(1));
	#else
		VertexShader = compile VERTEXSHADER VS_screenTransform();
		PixelShader  = compile PIXELSHADER PS_nothing()  CGC_FLAGS(CGC_DEFAULTFLAGS_NPC(1));
	#endif
	}
}

technique RestoreDepth
{
	pass restoreDepthNormal
	{
		VertexShader = compile VERTEXSHADER VS_RestoreDepth();
		PixelShader  = compile PIXELSHADER PS_RestoreDepth();
	}

	pass restoreDepth3BitStencil
	{
		VertexShader = compile VERTEXSHADER VS_RestoreDepth();
		PixelShader  = compile PIXELSHADER PS_RestoreDepth3BitStencil();
	}	
	
	pass p1
	{
		VertexShader = compile VERTEXSHADER VS_RestoreDepth();
		PixelShader  = NULL;
	}
}
#endif

// ----------------------------------------------------------------------------------------------- //

technique CopyGBuffer
{
	pass p0
	{
		CullMode = NONE;

		ZEnable = false;
		ZWriteEnable = false;
		ZFunc = always;

		AlphaBlendEnable = false;
		AlphaTestEnable = false;

		#if __PS3 || __XENON
		VertexShader = compile VERTEXSHADER VS_screenTransformT();
		#else
		VertexShader = compile VERTEXSHADER VS_screenTransform();
		#endif

		PixelShader  = compile PIXELSHADER PS_GBufferCopy()  CGC_FLAGS(CGC_DEFAULTFLAGS_NPC(1));
	}
}


OutHalf4Color PS_ShadowModel_Depth_Only_MRT( SimpleShadowVertexOutput IN )
{
	return CastOutHalf4Color((half4)0);
}

technique WriteDepthOnlyMRT
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER VS_TransformShadowModel(gbOffsetEnable);
		#if __XENON
		PixelShader = NULL;
		#else
		PixelShader  = compile PIXELSHADER PS_ShadowModel_Depth_Only_MRT()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		#endif
	}
}

// ----------------------------------------------------------------------------------------------- //

technique DistantLights
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER VS_DistantLights3d();
		PixelShader  = compile PIXELSHADER  PS_DistantLights() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p1 
	{
		VertexShader = compile VERTEXSHADER VS_DistantLights2d();
		PixelShader  = compile PIXELSHADER  PS_DistantLights() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p2
	{
		VertexShader = compile VERTEXSHADER VS_DistantLights3d();
		PixelShader  = compile PIXELSHADER  PS_DistantLightsReflection() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p3 
	{
		VertexShader = compile VERTEXSHADER VS_DistantLights2d();
		PixelShader  = compile PIXELSHADER  PS_DistantLightsReflection() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}

// ----------------------------------------------------------------------------------------------- //

#if ENABLE_DISTANT_CARS
technique DistantCars
{
	pass p0
	{
		AlphaBlendEnable         = true;
		BLENDOP                  = ADD;
		SRCBLEND                 = SRCALPHA;
		DESTBLEND                = INVSRCALPHA;
		VertexShader = compile VERTEXSHADER VS_DistantCars();
		PixelShader  = compile PIXELSHADER  PS_DistantCars() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}

// ----------------------------------------------------------------------------------------------- //
#endif //ENABLE_DISTANT_CARS

technique GameGlows
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER VS_GameGlow();
		PixelShader  = compile PIXELSHADER PS_GameGlow()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

#if __SHADERMODEL >= 40 && RSG_PC
#  define	PS41	ps_4_1
	pass p1
	{
		VertexShader = compile VERTEXSHADER VS_GameGlow();
		PixelShader  = compile PS41 PS_GameGlowAA()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
#endif // __SHADERMODEL >= 40 && RSG_PC

	pass p2
	{
		VertexShader = compile VERTEXSHADER VS_FSGameGlow();
		PixelShader  = compile PIXELSHADER PS_FSGameGlow()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

#if __SHADERMODEL >= 40 && RSG_PC
#  define	PS41	ps_4_1
	pass p3
	{
		VertexShader = compile VERTEXSHADER VS_FSGameGlow();
		PixelShader  = compile PS41 PS_FSGameGlowAA()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
#endif // __SHADERMODEL >= 40 && RSG_PC

	pass p4
	{
		VertexShader = compile VERTEXSHADER VS_FSGameGlow();
		PixelShader  = compile PIXELSHADER PS_FSGameGlowMarker()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

#if __SHADERMODEL >= 40 && RSG_PC
#  define	PS41	ps_4_1
	pass p5
	{
		VertexShader = compile VERTEXSHADER VS_FSGameGlow();
		PixelShader  = compile PS41 PS_FSGameGlowAAMarker()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
#endif // __SHADERMODEL >= 40 && RSG_PC

}

// ----------------------------------------------------------------------------------------------- //

technique ToneMap 
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER	VS_Blit();
		PixelShader  = compile PIXELSHADER	PS_ToneMap()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p1
	{
		VertexShader = compile VERTEXSHADER	VS_Blit();
		PixelShader  = compile PIXELSHADER	PS_ToneMapWithAlpha()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}

// ----------------------------------------------------------------------------------------------- //

technique LuminanceToAlpha 
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER	VS_Blit();
		PixelShader  = compile PIXELSHADER	PS_LuminanceToAlpha()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}

// ----------------------------------------------------------------------------------------------- //

#if MULTIHEAD_FADE
technique RadialFade
{
	pass p1
	{
		CullMode = NONE;
		ZEnable = false;
		AlphaBlendEnable = true;
		AlphaTestEnable = false;
		SrcBlend	= SRCALPHA;
		DestBlend	= INVSRCALPHA;

		VertexShader = compile VERTEXSHADER VS_RadialFade();
		PixelShader  = compile PIXELSHADER PS_RadialFade() CGC_FLAGS(CGC_DEFAULTFLAGS2D);
	}
}
#endif	//MULTIHEAD_FADE

// ----------------------------------------------------------------------------------------------- //

#if USE_HQ_ANTIALIASING 
#if RSG_PC
# define HQAA_PIXEL_SHADER	ps_4_0
# define HQAA_NAME(name)	name##_sm40
#else
# define HQAA_PIXEL_SHADER	ps_5_0
# define HQAA_NAME(name)	name
#endif

technique HQAA_NAME(HighQualityAA)
{
	
	pass HQAA_NAME(p0)
	{
		VertexShader = compile VERTEXSHADER VS_HQAA();

#if FXAA_EARLY_EXIT
		PixelShader  = compile HQAA_PIXEL_SHADER PS_HQAA()  CGC_FLAGS("--fenable-bx2 --fastmath --fastprecision --nofloatbindings --regcount 7 --disablepc all --O2 -texformat default RGBA8");
#else
		PixelShader  = compile HQAA_PIXEL_SHADER PS_HQAA()  CGC_FLAGS("--fenable-bx2 --fastmath --fastprecision --nofloatbindings --regcount 7 --disablepc all --O3 -texformat default RGBA8");
#endif
	}
}
technique HQAA_NAME(HighQualityAA_Tiled)
{
	pass HQAA_NAME(p1)
	{
		VertexShader = compile VERTEXSHADER VS_HQAA_Tiled();

#if FXAA_EARLY_EXIT
		PixelShader  = compile HQAA_PIXEL_SHADER PS_HQAA()  CGC_FLAGS("--fenable-bx2 --fastmath --fastprecision --nofloatbindings --regcount 7 --disablepc all --O2 -texformat default RGBA8");
#else
		PixelShader  = compile HQAA_PIXEL_SHADER PS_HQAA()  CGC_FLAGS("--fenable-bx2 --fastmath --fastprecision --nofloatbindings --regcount 7 --disablepc all --O3 -texformat default RGBA8");
#endif
	}
	pass HQAA_NAME(p2)
	{
		VertexShader = compile VERTEXSHADER VS_HQAA_Tiled();
		PixelShader  = compile HQAA_PIXEL_SHADER PS_UIAA() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
	pass HQAA_NAME(p3)
	{
		VertexShader = compile VERTEXSHADER VS_HQAA_Tiled_SkyOnly();
		PixelShader  = compile HQAA_PIXEL_SHADER PS_HQAA_Blit() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}

#endif // USE_HQ_ANTIALIASING


#if GLINTS_ENABLED
OutFloat4Color PS_GenerateGlints(rageVertexBlit IN)
{
	sampler2D backBufferPoint = RenderMapPointSampler;
 	sampler2D backBufferLinear = RefMipBlurSampler;

	float2 centerCoord = IN.texCoord0;

	// Start with center sample color
	float3 centerColor = tex2D( backBufferPoint, centerCoord.xy).rgb;
	float3 colorSum = centerColor;
	float totalContribution = 1.0f;

	float depthSample	= GBufferTexDepth2D(DepthMapPointSampler, centerCoord).x;
	float centerDepth = getLinearDepth(depthSample, GeneralParams0.zw);
	// Sample the depth and blur at the center
// 	float2 centerDepthBlur = tex2D( PostFxSampler3, centerCoord).xy;
// 	float centerDepth = centerDepthBlur.x;
// 	float centerBlur = centerDepthBlur.y;

	// We're going to calculate the average of the current 5x5 block of texels
	// by taking 9 bilinear samples
	const uint NumSamples = 16;
	const float2 SamplePoints[NumSamples] =
	{
		float2(-1.5f, -1.5f), float2(-0.5f, -1.5f), float2(0.5f, -1.5f), float2(1.5f, -1.5f),
		float2(-1.5f, -0.5f), float2(-0.5f, -0.5f), float2(0.5f, -0.5f), float2(1.5f, -0.5f),
		float2(-1.5f, 0.5f), float2(-0.5f, 0.5f), float2(0.5f, 0.5f), float2(1.5f, 0.5f),
		float2(-1.5f, 1.5f), float2(-0.5f, -1.5f), float2(0.5f, 1.5f), float2(1.5f, 1.5f)
	};

	float3 averageColor = 0.0f;
	for(uint i = 0; i < NumSamples; ++i)
	{
		float2 sampleCoord = centerCoord + SamplePoints[i] / gScreenSize;
		float3 sampleC = tex2D( backBufferLinear, sampleCoord).rgb;

		averageColor += sampleC;
	}
	averageColor /= NumSamples;

	/*// Calculate the difference between the current texel and the average
	float averageBrightness = dot(averageColor, 1.0f);
	float centerBrightness = dot(centerColor, 1.0f);
	float brightnessDiff = max(centerBrightness - averageBrightness, 0.0f);*/

	float brightnessDiff = averageColor;

	// If the texel is brighter than its neighbors and the blur amount
	// is above our threshold, then push the pixel position/color/blur amount into
	// our AppendStructuredBuffer containing glint points

	float BrightnessThreshold = GeneralParams1.x;
#if !RSG_ORBIS	//TODO
	[branch]
#endif
	if(brightnessDiff >= BrightnessThreshold)
	{
		GlintPoint bPoint;
		bPoint.Position = float4(IN.pos.xy, centerDepth, depthSample);
		bPoint.Color = centerColor;

		GlintPointBuffer.Append(bPoint);

		centerColor = 0.0f;
	}

	return CastOutFloat4Color(float4(centerColor, 1.0f));
}

GlintVSOutput VS_GlintSprites(uint VertexID	: SV_VertexID)
{
	GlintPoint bPoint = GlintSpritePointBuffer[VertexID];
	GlintVSOutput output;

	// Position the vertex in normalized device coordinate space [-1, 1]
	output.Position.xy = bPoint.Position.xy;
	output.Position.xy /= gScreenSize.xy;
	output.Position.xy = output.Position.xy * 2.0f - 1.0f;
	output.Position.y *= -1.0f;
	output.Position.zw = 1.0.xx;
	output.Depth = bPoint.Position.z;

	float centerEmptyZoneScalar = 0.3;

	//Depending on their position on screen orientate them horizonatal or vertical and scale them based on
	//distance from the center of the screen.
	float2 intensity = abs(output.Position.xy);
	intensity = saturate(intensity - centerEmptyZoneScalar) / (1.0f-centerEmptyZoneScalar);

	float2 glintSize = float2(GeneralParams1.y, GeneralParams1.z);
	float2 sizeVal;
	sizeVal.x = glintSize.x * intensity.x * (1.0 - intensity.y);
	sizeVal.y = glintSize.y;
	float flipUV = 0.0f;
	if( intensity.x <= centerEmptyZoneScalar )
	{
		sizeVal.x = glintSize.y;
		sizeVal.y = glintSize.x * intensity.y * (1.0 - intensity.x);
		flipUV = 1.0f;
	}

	float f0 = 1.0f - saturate((bPoint.Position.w - GeneralParams0.x) * (1.0f / GeneralParams0.x));
    float f1 = saturate((bPoint.Position.w - GeneralParams0.x) * (1.0f / (1.0 - GeneralParams0.x)));
    float depth = 1.0f - saturate(f0 + f1);

	float depthScale = depth;//1.0 - bPoint.Position.w;
	//depthScale *= GeneralParams0.x;
	output.Size = sizeVal * (1.0f / gScreenSize.xy) * depthScale;

	float alphaScale = GeneralParams1.w;
	output.Color = float4( bPoint.Color * (1.0f / alphaScale), flipUV);

	return output;
}

[maxvertexcount(4)]
void GS_GlintSprites(point GlintVSOutput input[1], inout TriangleStream<GlintGSOutput> SpriteStream)
{
	GlintGSOutput output;

	const float2 Offsets[4] =
	{
		float2(-1, 1),
		float2(1, 1),
		float2(-1, -1),
		float2(1, -1)
	};

	float uvFlip = input[0].Color.a;
	const float2 TexCoords[4] =
	{
		float2(0, uvFlip),
		float2(1-uvFlip, 0),
		float2(uvFlip, 1),
		float2(1, 1-uvFlip)
	};

	// Emit 4 new verts, and 2 new triangles
	[unroll]
	for (int i = 0; i < 4; i++)
	{
		output.PositionCS = float4(input[0].Position.xy, 1.0f, 1.0f);
		output.PositionCS.xy += Offsets[i] * input[0].Size;
		output.TexCoord = TexCoords[i];
		output.Color = input[0].Color.rgb;
		output.Depth = input[0].Depth;

		SpriteStream.Append(output);
	}
	SpriteStream.RestartStrip();
}

//=================================================================================================
// Pixel Shader, applies the glint shape texture
//=================================================================================================
float4 PS_GlintSprites(GlintPSInput input) : SV_Target0
{
	sampler2D GlintSampler = RefMipBlurSampler;

	// Sample the Glint texture
	float textureSample = tex2D( GlintSampler, input.TexCoord).x;

	return float4(input.Color * textureSample, 1.0f);
}

technique11 Glints_sm50
{
	pass pGlintGenerate_sm50
	{
		AlphaBlendEnable = false;
		AlphaTestEnable = false;

		VertexShader = compile VERTEXSHADER VS_Blit();
		PixelShader  = compile ps_5_0 PS_GenerateGlints()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass pGlintDrawSprites_sm50
	{
		AlphaBlendEnable = true;

		VertexShader = compile VSGS_SHADER_50 VS_GlintSprites();
		SetGeometryShader(compileshader(gs_5_0, GS_GlintSprites()));
		PixelShader  = compile ps_5_0 PS_GlintSprites()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}


#endif //GLINTS_ENABLED

// ----------------------------------------------------------------------------------------------- //

//
//
//
//
vertexOutputUnlit VS_sprite3d_UnlitIM(rageVertexInputLit IN)
{
vertexOutputUnlit OUT;

	float2 uvs			= IN.texCoord0.xy - float2(0.5f,0.5f);

	float2 widthHeight	= GeneralParams0.xy;
	uvs *= widthHeight;

	// camera aligned:
	float3 across	= gViewInverse[0].xyz;
	float3 up		= -gViewInverse[1].xyz;

	float3 offset	= uvs.x*across + uvs.y*up;

	offset.xyz		+= IN.pos.xyz;				// offset from middle of "mesh of 4-in-1's";

float3 IN_pos;
	IN_pos.x = dot(offset, gWorld[0].xyz);	// inverse local rotation - it will be re-applied by wvp matrix
	IN_pos.y = dot(offset, gWorld[1].xyz);
	IN_pos.z = dot(offset, gWorld[2].xyz);

	OUT.pos			= ApplyCompositeWorldTransform(float4(IN_pos.xyz,1), gWorldViewProj);
	OUT.texCoord	= IN.normal.xy; // (U,V)
	OUT.color0		= IN.diffuse;

	return(OUT);
}


//
//
//
//
OutHalf4Color PS_sprite3d_UnlitIM( vertexOutputUnlit IN )
{
	float4 ret=PixelTexturedUnlit(IN);
	return CastOutHalf4Color(half4(ret.rgb*gEmissiveScale, ret.a));
}


technique draw_sprite3d
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER	VS_sprite3d_UnlitIM();
		PixelShader  = compile PIXELSHADER	PS_sprite3d_UnlitIM()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
	
// ----------------------------------------------------------------------------------------------- //
// ----------------------------------------------------------------------------------------------- //
// ----------------------------------------------------------------------------------------------- //

