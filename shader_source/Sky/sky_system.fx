///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//													PIOTR DUBLA														 //
//							         Sky shader (clouds, sun, stars, moon)											 //
//													 10/11/2009														 //
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma dcl position texcoord0 texcoord1

#if __WIN32PC
#pragma warning (disable: 3205)
#endif

// DEFINES
#define NO_SKINNING
#define FOG_SKY

// INCLUDES
#include "../common.fxh"
#include "../Util/macros.fxh"
#include "../Util/GSInst.fxh"

// EXTRAS
// We don't care for no skinning matrices here, so we can use a bigger constant register file
#pragma constant 130

#define OPTIMIZE CGC_FLAGS("-unroll all -fastmath")
//#define OPTIMIZE

#define FOG_OFF false
#define SUN_OFF false
#define MOON_OFF false
#define CLOUDS_OFF false
#define STARS_OFF false
#define DEBUG_OFF false

#define FOG_ON true
#define SUN_ON true
#define MOON_ON true
#define CLOUDS_ON true
#define STARS_ON true
#define DEBUG_ON true

#if __XENON
	#define kDitherScale 0.0002		// 360 only has 10 bits, so we need more dither
#else
	#define kDitherScale 0.0001
#endif

// ------------------------------------------------------------------------------------------------------------------//
//													VARIABLES														 //
// ------------------------------------------------------------------------------------------------------------------//

#define GEN_SKY_FUNCS(typeName, passName, sunEnabled, moonEnabled, cloudsEnabled, starsEnabled, debugEnabled) \
\
JOIN3(VS_,typeName,_OUTPUT) JOIN4(vs_,typeName,_normal_,passName)(JOIN3(VS_,typeName,_INPUT) IN) \
{ \
	float4 transPos = mul(IN.pos, gWorldViewProj); \
	return JOIN3(vs_,typeName,_main)(transPos, IN, FOG_ON, sunEnabled, moonEnabled, cloudsEnabled, starsEnabled, debugEnabled); \
} \
OutHalf4Color JOIN4(ps_,typeName,_normal_,passName)(JOIN3(PS_,typeName,_INPUT) IN) \
{ \
	half4 colour = JOIN3(ps_,typeName,_main)(IN, FOG_ON, sunEnabled, moonEnabled, cloudsEnabled, starsEnabled, debugEnabled); \
	return CastOutHalf4Color(PackHdr(colour)); \
} \
JOIN3(VS_,typeName,_OUTPUT) JOIN4(vs_,typeName,_water_reflection_,passName)(JOIN3(VS_,typeName,_INPUT) IN) \
{ \
	float4 transPos = mul(IN.pos, gWorldViewProj); \
	return JOIN3(vs_,typeName,_main)(transPos, IN, FOG_ON, sunEnabled, moonEnabled, cloudsEnabled, starsEnabled, debugEnabled); \
} \
OutHalf4Color JOIN4(ps_,typeName,_water_reflection_,passName)(JOIN3(PS_,typeName,_INPUT) IN) \
{ \
	half4 colour = JOIN3(ps_,typeName,_main)(IN, FOG_ON, sunEnabled, moonEnabled, cloudsEnabled, starsEnabled, debugEnabled); \
	return CastOutHalf4Color(PackReflection(colour)); \
} \
JOIN3(VS_,typeName,_OUTPUT) JOIN4(vs_,typeName,_mirror_reflection_,passName)(JOIN3(VS_,typeName,_INPUT) IN) \
{ \
	float4 transPos = mul(IN.pos, gWorldViewProj); \
	return JOIN3(vs_,typeName,_main)(transPos, IN, FOG_ON, sunEnabled, moonEnabled, cloudsEnabled, starsEnabled, debugEnabled); \
} \
OutHalf4Color JOIN4(ps_,typeName,_mirror_reflection_,passName)(JOIN3(PS_,typeName,_INPUT) IN) \
{ \
	half4 colour = JOIN3(ps_,typeName,_main)(IN, FOG_ON, sunEnabled, moonEnabled, cloudsEnabled, starsEnabled, debugEnabled); \
	return CastOutHalf4Color(PackHdr(colour)); \
} \

// ------------------------------------------------------------------------------------------------------------------//

#define GEN_PASS(typeName, techName, passName) \
pass JOIN(p_, passName) \
{ \
	VertexShader = compile VERTEXSHADER JOIN6(vs_,typeName,_,techName,_,passName)(); \
	PixelShader = compile PIXELSHADER JOIN6(ps_,typeName,_,techName,_,passName)()  OPTIMIZE; \
} \

// ------------------------------------------------------------------------------------------------------------------//
//													VARIABLES														 //
// ------------------------------------------------------------------------------------------------------------------//

BEGIN_RAGE_CONSTANT_BUFFER(sky_system_locals,b0)
// Sky variables
float3 azimuthEastColor;
float3 azimuthWestColor;
float3 azimuthTransitionColor;
half azimuthTransitionPosition;

float3 zenithColor;
float3 zenithTransitionColor;
const half4 zenithConstants;
#define zenithTransitionPosition		half(zenithConstants.x)
#define zenithTransitionEastBlend		half(zenithConstants.y)
#define zenithTransitionWestBlend		half(zenithConstants.z)
#define oneMinusZenithBlendStart		half(zenithConstants.w)

float4 skyPlaneColor;
float4 skyPlaneParams;
#define skyPlaneFogFadeStart			float(skyPlaneParams.x)
#define skyPlaneFogFadeEnd				float(skyPlaneParams.y)
#define skyPlaneFogFadeOneOverDiff		float(skyPlaneParams.z)

half hdrIntensity;

// Sun variables
half3 sunColor;
half3 sunColorHdr;
half3 sunDiscColorHdr;

// Mie constants need to be full float, otherwise sun has bands (B*789750)
const float4 sunConstants;
#define miePhaseTimesTwo				float(sunConstants.x)
#define miePhaseSqrPlusOne				float(sunConstants.y)
#define mieConstantTimesScatter			float(sunConstants.z)
#define mieIntensityMult				float(sunConstants.w)

const float3 sunDirection;
const float3 sunPosition;

// Cloud variables
half3 cloudBaseMinusMidColour;
half3 cloudMidColour;
half3 cloudShadowMinusBaseColourTimesShadowStrength;

const half4 cloudDetailConstants;
#define cloudEdgeDetailStrength			half(cloudDetailConstants.x)
#define cloudEdgeDetailScale			half(cloudDetailConstants.y)
#define cloudOverallDetailStrength		half(cloudDetailConstants.z)
#define cloudOverallDetailScale			half(cloudDetailConstants.w)

const half4 cloudConstants1;
#define cloudBaseStrength				half(cloudConstants1.x)
#define cloudDensityMultiplier			half(cloudConstants1.y)
#define cloudDensityBias				half(cloudConstants1.z)
#define cloudFadeOut					half(cloudConstants1.w)

const half4 cloudConstants2;
#define cloudShadowStrength				half(cloudConstants2.x)
#define cloudOffset						half(cloudConstants2.y)
#define cloudOverallColorStrength		half(cloudConstants2.z)
#define cloudHdrIntensity				half(cloudConstants2.w)

const half4 smallCloudConstants;
#define smallCloudDetailScale			half(smallCloudConstants.x)
#define	smallCloudDetailStrength		half(smallCloudConstants.y)
#define smallCloudDensityMultiplier		half(smallCloudConstants.z)
#define smallCloudDensityBias			half(smallCloudConstants.w)

half3 smallCloudColorHdr;

const half4 effectsConstants;
#define sunInfluenceRadius				half(effectsConstants.x)
#define sunScatterIntensity				half(effectsConstants.y)
#define moonInfluenceRadius				half(effectsConstants.z)
#define moonScatterIntensity			half(effectsConstants.w)

// Misc variables
const half horizonLevel;

const half3 speedConstants;
#define smallCloudOffset				half(speedConstants.x)
#define overallDetailOffset				half(speedConstants.y)
#define edgeDetailOffset				half(speedConstants.z)

// Night
const half starfieldIntensity;
const float3 moonDirection;
const float3 moonPosition;
const half moonIntensity;
const float3 lunarCycle;
const half3 moonColor;

// Perlin
const float noiseFrequency;
const float noiseScale;
const float noiseThreshold;
const float noiseSoftness;
const float noiseDensityOffset;
const float2 noisePhase;

#if !defined(SHADER_FINAL)
float4 debugCloudsParams[3];
#endif // !defined(SHADER_FINAL)

EndConstantBufferDX10(sky_system_locals)

// ------------------------------------------------------------------------------------------------------------------//
//													TEXTURES														 //
// ------------------------------------------------------------------------------------------------------------------//

BeginSampler(sampler2D, noiseTexture, NoiseSampler, noiseTexture)
    string UIName = "Noise Layer Texture";
ContinueSampler(sampler2D, noiseTexture, NoiseSampler, noiseTexture)
		AddressU = WRAP;        
		AddressV = WRAP;
		AddressW = WRAP;
		MINFILTER = LINEAR;
		MAGFILTER = LINEAR;
		MIPFILTER = LINEAR;
		MipMapLodBias = 0;
EndSampler;

BeginSampler(sampler2D, perlinNoiseRT, perlinSampler, perlinNoiseRT)
    string UIName = "Perlin Noise Map";
ContinueSampler(sampler2D, perlinNoiseRT, perlinSampler, perlinNoiseRT)
	AddressU  = WRAP;        
	AddressV  = WRAP;
	AddressW  = WRAP;
	MIPFILTER = LINEAR;
	MINFILTER = LINEAR;
	MAGFILTER = LINEAR;
	MipMapLodBias = 0;
EndSampler;

BeginSampler(sampler2D, highDetailNoiseTex, highDetailSampler, highDetailNoiseTex)
    string UIName = "High Detail Noise Map";
ContinueSampler(sampler2D, highDetailNoiseTex, highDetailSampler, highDetailNoiseTex)
	AddressU  = WRAP;        
	AddressV  = WRAP;
	AddressW  = WRAP;
	MIPFILTER = LINEAR;
	MINFILTER = LINEAR;
	MAGFILTER = LINEAR;
	MipMapLodBias = 0;
EndSampler;

BeginSampler(sampler2D, starFieldTex, starFieldSampler, starFieldTex)
    string UIName = "Starfield Map";
ContinueSampler(sampler2D, starFieldTex, starFieldSampler, starFieldTex)
	AddressU  = WRAP;        
	AddressV  = WRAP;
	AddressW  = WRAP;
	MIPFILTER = LINEAR;
	MINFILTER = LINEAR;
	MAGFILTER = LINEAR;
	MipMapLodBias = 0;
EndSampler;

BeginSampler(sampler2D, ditherTex, ditherSampler, ditherTex)
    string UIName = "Dither Map";
ContinueSampler(sampler2D, ditherTex, ditherSampler, ditherTex)
	AddressU  = WRAP;        
	AddressV  = WRAP;
	AddressW  = WRAP;
	MIPFILTER = LINEAR;
	MINFILTER = LINEAR;
	MAGFILTER = LINEAR;
	MipMapLodBias = 0;
EndSampler;

BeginSampler(sampler2D, moonTex, moonSampler, moonTex)
    string UIName = "Moon Map";
ContinueSampler(sampler2D, moonTex, moonSampler, moonTex)
	AddressU  = CLAMP;        
	AddressV  = CLAMP;
	AddressW  = CLAMP;
	MIPFILTER = LINEAR;
	MINFILTER = LINEAR;
	MAGFILTER = LINEAR;
	MipMapLodBias = 0;
EndSampler;

// ------------------------------------------------------------------------------------------------------------------//
//													STRUCTURES														 //
// ------------------------------------------------------------------------------------------------------------------//

struct VS_sky_INPUT
{
	float4 pos : POSITION;
	half2 texCoord0 : TEXCOORD0;
	half2 texCoord1 : TEXCOORD1;

#if RAGE_INSTANCED_TECH
	uint InstID : SV_InstanceID;
#endif
};

// ------------------------------------------------------------------------------------------------------------------//

struct VS_sky_OUTPUT
{
	DECLARE_POSITION(pos)
	half4 texCoord:   TEXCOORD0;
	float4 worldPos : TEXCOORD1;
	float4 skyCol : TEXCOORD2;
	half4 detailTexCoord : TEXCOORD3;
	half4 ditherAndSmallCloudTexCoord : TEXCOORD4;	// ditherTexCoord and smallCloudTexCoords packed into one interpolator
	half2 starTexCoords : TEXCOORD5;
	float4 fogData : TEXCOORD6;						// do not change to half4, it results in banding/errors in dark fog conditions (especially under water)

	// watch out for addition attribute. may need to fix VS_sky_OUTPUTCubeInst.
};

// ------------------------------------------------------------------------------------------------------------------//

struct PS_sky_INPUT
{
	DECLARE_POSITION_PSIN(pos)
	half4 texCoord : TEXCOORD0;
	float4 worldPos : TEXCOORD1;
	float4 skyCol : TEXCOORD2;
	half4 detailTexCoord : TEXCOORD3;
	half4 ditherAndSmallCloudTexCoord : TEXCOORD4;	
	half2 starTexCoords : TEXCOORD5;
	float4 fogData : TEXCOORD6;						// do not change to half4
};

#define ditherTexCoord ditherAndSmallCloudTexCoord.xy	 
#define smallCloudTexCoords ditherAndSmallCloudTexCoord.zw	 

// ------------------------------------------------------------------------------------------------------------------//

struct VS_moon_INPUT
{
	float4 inPos : POSITION;
	half2 inTexCoords : TEXCOORD0;

#if RAGE_INSTANCED_TECH
	uint InstID : SV_InstanceID;
#endif
};
// ------------------------------------------------------------------------------------------------------------------//

struct VS_moon_OUTPUT
{
#if RSG_ORBIS
	DECLARE_POSITION(pos)
#else
	half4 pos : POSITION;
#endif
	half2 texCoord : TEXCOORD0;
	float4 worldPos : TEXCOORD1;
};

// ------------------------------------------------------------------------------------------------------------------//

struct PS_moon_INPUT
{
	half2 texCoord:   TEXCOORD0;
	float4 worldPos : TEXCOORD1;
};

// ------------------------------------------------------------------------------------------------------------------//

struct VS_sun_INPUT
{
	float4 inPos : POSITION;
	half2 inTexCoords: TEXCOORD0;

#if RAGE_INSTANCED_TECH
	uint InstID : SV_InstanceID;
#endif
};

// ------------------------------------------------------------------------------------------------------------------//

struct VS_sun_OUTPUT
{
#if RSG_ORBIS
	DECLARE_POSITION(pos)
#else
	half4 pos : POSITION;
#endif
	float4 fogData : TEXCOORD0;						// do not change to half4
};

// ------------------------------------------------------------------------------------------------------------------//

struct VS_sun_parab_OUTPUT
{
#if RSG_ORBIS
	DECLARE_POSITION(pos)
#else
	half4 pos : POSITION;
#endif
	float3 worldPos : TEXCOORD0;
	float4 fogData : TEXCOORD1;						// do not change to half4
};

// ------------------------------------------------------------------------------------------------------------------//

struct VS_sun_vis_OUTPUT
{
#if RSG_ORBIS
	DECLARE_POSITION(pos)
#else
	half4 pos : POSITION;
#endif
	float fogBlend : TEXCOORD0;
};

// ------------------------------------------------------------------------------------------------------------------//

struct PS_sun_vis_INPUT
{
#if !__XENON
	DECLARE_POSITION_PSIN(pos)
#endif
	float fogBlend : TEXCOORD0;
};

// ------------------------------------------------------------------------------------------------------------------//

struct PS_sun_INPUT
{
	float3 worldPos : TEXCOORD0;
	float4 fogData : TEXCOORD1;						// do not change to half4
};

// ------------------------------------------------------------------------------------------------------------------//

struct VS_PERLIN_OUTPUT
{
	DECLARE_POSITION(pos)
	float2 texCoord : TEXCOORD0;
};

// ------------------------------------------------------------------------------------------------------------------//

struct VS_SKY_PLANE_INPUT
{
	float4 pos : POSITION;

#if RAGE_INSTANCED_TECH
	uint InstID : SV_InstanceID;
#endif
};

// ------------------------------------------------------------------------------------------------------------------//

struct VS_SKY_PLANE_OUTPUT
{
	DECLARE_POSITION(pos)
	float3 color : TEXCOORD0;
	float4 eyeToWorld : TEXCOORD1;
};

// ------------------------------------------------------------------------------------------------------------------//

struct VS_SKY_PLANE_PARAB_OUTPUT
{
	DECLARE_POSITION(pos)
	float3 color : TEXCOORD0;
	float4 eyeToWorld : TEXCOORD1;
	float3 worldPos : TEXCOORD2;
};

// ------------------------------------------------------------------------------------------------------------------//
// ------------------------------------------------------------------------------------------------------------------//

struct TEX_DATA
{
	float2 perlin;
	float2 offsetPerlin;
	float detailOverall;
	float2 detailEdge;
	float dither;
	float3 stars;
};

// ------------------------------------------------------------------------------------------------------------------//

struct SUN_RESULT
{
	half3 color;
	half cosTheta;
	float invAlpha;
};

// ------------------------------------------------------------------------------------------------------------------//

struct CLOUD_RESULT
{
	half3 color;
	half dimming;
	half noise;
	half2 alpha;
};

// ------------------------------------------------------------------------------------------------------------------//

struct EFFECTS_RESULT
{
	half3 color;
};

// ------------------------------------------------------------------------------------------------------------------//
//												HELPER FUNCTIONS													 //
// ------------------------------------------------------------------------------------------------------------------//

half2 RageNormalize2(half2 d)
{
	const half mag = dot(d, d);
	const half recip = rsqrt(mag);
	return (mag == 0.0f) ?  0.0f : d * recip;
}

// ------------------------------------------------------------------------------------------------------------------//

half2 sphericalWarp(half2 tex)
{
	tex = tex - 0.5;
	const half mag = dot(tex, tex); 
	tex =  mag * RageNormalize2(tex) + 0.5;
	return tex;
}

// ------------------------------------------------------------------------------------------------------------------//

half2 sphericalWarpOffset(half2 tex , half2 offset)
{
	const half2 t = (2.0f * tex - 1.0f);
	return offset * (1.0f - dot(t, t));
}

// ------------------------------------------------------------------------------------------------------------------//
//													FUNCTIONS														 //
// ------------------------------------------------------------------------------------------------------------------//

// Calculate sun colour
SUN_RESULT calcSunData(float3 viewDir)
{
	half cosTheta = dot(viewDir, -sunDirection);
	const half cosThetaSq = cosTheta * cosTheta;
	
	const half phase = (1.0f + cosThetaSq) / ragePow((miePhaseSqrPlusOne - (miePhaseTimesTwo * cosTheta)), 1.5f) * mieConstantTimesScatter;

	SUN_RESULT OUT;

	OUT.color =  (sunColorHdr * saturate(phase)) * mieIntensityMult;
	OUT.cosTheta = saturate(-cosTheta);
	OUT.invAlpha = saturate(1.0 - phase);

	return OUT;
}

// ------------------------------------------------------------------------------------------------------------------//

// Calculate sky colour
half3 calcAtmosphereColor(half3 viewDir)
{
	// Azimuth blend = cos(angle) [-1..1] -> [0..1] and used as a lerp factor
	// Zenith blend = cos(angle) [0..1] (as its only from the horizon upwards)
	const half azimuthBlend = sqrt(-viewDir.x * 0.5 + 0.5);
	const half zenithBlend = abs(viewDir.z);
	
	// Defines where along the arc the transition color is used
	const half3 azimuthColor = (azimuthBlend < azimuthTransitionPosition) ? 
								 lerp(azimuthEastColor, azimuthTransitionColor, azimuthBlend / azimuthTransitionPosition) : 
								 lerp(azimuthTransitionColor, azimuthWestColor, (azimuthBlend - azimuthTransitionPosition) / (1.0 - azimuthTransitionPosition));

	// Blend the zenith transition color with the already calculated azimuthBlend
	const half zenithTransitionBlend = lerp(zenithTransitionEastBlend, zenithTransitionWestBlend, azimuthBlend);
	const half3 newZenithTransitionColor = lerp(azimuthColor, zenithTransitionColor, zenithTransitionBlend);
	
	// Define where along the hemisphere a blend between the azimuth and zenith color occurs
	half zenithTransitionToTop = saturate((zenithBlend - zenithTransitionPosition) / (1.0f - zenithTransitionPosition));
	half bottomToZenithTransition = (zenithBlend / zenithTransitionPosition);

	zenithTransitionToTop = saturate(zenithTransitionToTop / oneMinusZenithBlendStart);

	const half3 skyColor = (zenithBlend < zenithTransitionPosition) ? 
							lerp(azimuthColor, newZenithTransitionColor,  bottomToZenithTransition) : 
							lerp(newZenithTransitionColor, zenithColor, zenithTransitionToTop);

	return skyColor;
}

// ------------------------------------------------------------------------------------------------------------------//

CLOUD_RESULT calcCloudData(PS_sky_INPUT IN, half3 viewDir, TEX_DATA texData)
{
	// ------------------------- //
	// Calculate base for clouds //
	// ------------------------- //
	half cloudBase = texData.offsetPerlin.x;
	cloudBase += texData.detailOverall * cloudOverallColorStrength * cloudOverallDetailStrength;
	cloudBase = cloudBase * cloudBaseStrength * texData.perlin.x;

	// ----------------------- //
	// Calculate cloud density //
	// ----------------------- //
	// Work done in two channels, x = large clouds, y = small clouds
	half2 detailAmt = half2(texData.detailOverall, 1.0-texData.perlin.y) * half2(cloudOverallDetailStrength, smallCloudDetailStrength);
	detailAmt += texData.detailEdge * half2(cloudEdgeDetailStrength, smallCloudDetailStrength) * saturate(1.0f - cloudBase * 2.0f) * texData.perlin.y;
	
	const half2 cloud = half2(texData.perlin.x, 0.0) + detailAmt;
	const half2 cloudDensity = saturate(half2(cloudDensityMultiplier, smallCloudDensityMultiplier) * 
										cloud - 
										half2(cloudDensityBias, smallCloudDensityBias));

	half2 amount = cloudDensity * cloudDensity;

	// Fade
	const half fade = saturate(viewDir.z * 5.0f - cloudFadeOut);
	amount *= fade;
	half invAmount = saturate(1.0 - amount.x);

	// -------- //
	// The rest //
	// -------- //

	// Compile output structure
	CLOUD_RESULT OUT;

	// So shadow isn't additive
//  half3 finalBaseColor = lerp(cloudBaseColour.xyz, cloudShadowColour, (texData.offsetPerlin.y + detailAmt.x) * cloudShadowStrength); 
//	OUT.color = lerp(cloudMidColour.xyz, finalBaseColor, cloudBase);
	half shadowAmount = (texData.offsetPerlin.y + detailAmt.x);
	half3 finalBaseColorMinusMid = shadowAmount * cloudShadowMinusBaseColourTimesShadowStrength + cloudBaseMinusMidColour;
	OUT.color = cloudBase * (finalBaseColorMinusMid) + cloudMidColour;
	OUT.dimming = amount.x + (amount.y * 0.3 * invAmount); // Reduce light on filler clouds
	OUT.noise = cloud.x;
	OUT.alpha = amount;

	return OUT;
}

// ------------------------------------------------------------------------------------------------------------------//

EFFECTS_RESULT calcEffectsDataSun(float3 viewDir, CLOUD_RESULT cloudResults, SUN_RESULT sunResults)
{
	const float influenceRange = 1.0 - saturate((1.0 - sunResults.cosTheta) / effectsConstants.x * 100.0);
	float dimming = influenceRange * cloudResults.dimming * saturate(1.0 - cloudResults.noise);
	dimming *= dimming;
	dimming *= dimming;
	dimming *= sunScatterIntensity;
		
	EFFECTS_RESULT OUT;
	OUT.color = sunColor * dimming; 
	return OUT;
}

// ------------------------------------------------------------------------------------------------------------------//

EFFECTS_RESULT calcEffectsDataMoon(float3 viewDir, CLOUD_RESULT cloudResults, float moonAngle)
{
	const float influenceRange = 1.0 - saturate((1.0 - moonAngle) / effectsConstants.z * 100.0);
	float dimming = influenceRange * cloudResults.dimming * saturate(1.0 - cloudResults.noise);
	dimming *= dimming;
	dimming *= dimming;
	dimming *= (moonScatterIntensity * (lunarCycle.y * 0.5 + 0.5));
	
	EFFECTS_RESULT OUT;
	OUT.color = moonColor * dimming; 
	return OUT;
}

// ------------------------------------------------------------------------------------------------------------------//

EFFECTS_RESULT calcEffectsDataSunMoon(float3 viewDir, CLOUD_RESULT cloudResults, SUN_RESULT sunResults, float moonAngle)
{
	// A fade from 1 to sunInfluence radius, 0 beyond that
	const half2 cosTheta = half2(sunResults.cosTheta, moonAngle);

	// effectsConstants.xz = sun/moon influence radius
	const half2 influenceRange = 1.0 - saturate((1.0 - cosTheta) / effectsConstants.xz * 100.0);
	half2 dimming = influenceRange * cloudResults.dimming * saturate(1.0 - cloudResults.noise);
	dimming *= dimming;
	dimming *= dimming;
	dimming *= half2(sunScatterIntensity, moonScatterIntensity * (lunarCycle.y * 0.5 + 0.5));
	
	EFFECTS_RESULT OUT;
	OUT.color = (sunColor * dimming.xxx) + (moonColor * dimming.yyy); 

	return OUT;
}

// ------------------------------------------------------------------------------------------------------------------//

// Get all the texture data upfront
TEX_DATA getTexData(PS_sky_INPUT IN)
{
	TEX_DATA OUT;

	OUT.perlin = h3tex2D(perlinSampler, IN.texCoord.xy).xz;
	OUT.offsetPerlin = h2tex2D(perlinSampler, IN.texCoord.zw).xy;

	OUT.detailOverall = h1tex2D(highDetailSampler, IN.detailTexCoord.zw).x - 0.5f;

	OUT.detailEdge.x = h1tex2D(highDetailSampler, IN.detailTexCoord.xy).x - 0.5f;
	OUT.detailEdge.y = h1tex2D(highDetailSampler, IN.smallCloudTexCoords.xy).x - 0.5f;

	OUT.dither = h1tex2D(ditherSampler, IN.ditherTexCoord.xy).x - 0.5;
	OUT.stars = h3tex2D(starFieldSampler, IN.starTexCoords.xy).rgb;

	return OUT;
}

// ------------------------------------------------------------------------------------------------------------------//

// Calculate Noise
half calculateCloudNoise(VS_PERLIN_OUTPUT IN)
{
	// amp = amplitude
	// point = position of point
	// freq = frequency
	// Persistance = gain

	half2 p = ((IN.texCoord.xy - 0.5) * noiseScale + 0.5) / 128.0 + noisePhase;
	
	float finalnoise = 0.0f;
	float amp = 1.0f;

	for(int i = 0 ; i < 3 ;i++)
	{
		half3 noiseVal = (h3tex2D(NoiseSampler, p.xy).xyz - 0.5) * 2.0;
		finalnoise += noiseVal[i] * amp;
		p *= noiseFrequency;
		amp *= 0.5;
	}

	return finalnoise / 1.75;
}

// ------------------------------------------------------------------------------------------------------------------//
//												  SKY TECHNIQUE														 //
// ------------------------------------------------------------------------------------------------------------------//

VS_sky_OUTPUT vs_sky_main(
	float4 transPos, VS_sky_INPUT IN, 
	uniform bool fogEnabled, 
	uniform bool sunEnabled, 
	uniform bool moonEnabled, 
	uniform bool cloudsEnabled, 
	uniform bool starsEnabled,
	uniform bool debugEnabled,
	uniform bool useInst
	)
{
	VS_sky_OUTPUT OUT;
	OUT.pos = transPos;
	OUT.texCoord.xy = IN.texCoord0.xy;

	float3 camPos = float3(0,0,0);
#if GS_INSTANCED_CUBEMAP
	if (useInst)
	{
		OUT.worldPos.xyz = (float3)mul(IN.pos, gInstWorld);
		camPos = gInstViewInverse[IN.InstID][3].xyz;
	}
	else
#endif
	{
		// Set world pos (subtract any translation in world matrix)
		OUT.worldPos.xyz = (float3)mul(IN.pos, gWorld);
		camPos = gViewInverse[3].xyz;
	}

	OUT.fogData = CalcFogDataNoHaze( OUT.worldPos - camPos.xyz );

	// Set camera to always be at the horizon (this way sun doesn't move 
	// up and down the skydome
	const half3 cameraPos = half3(camPos.xy, horizonLevel);

	// Calculate sky color and pass to fragment shader
	const half3 viewDir = normalize(OUT.worldPos.xyz - cameraPos);

	float3 skyCol = calcAtmosphereColor(viewDir);

	OUT.skyCol.rgb = skyCol * hdrIntensity;
	OUT.skyCol.a = 0.0f; 

	// Azimuth blend
	float zenithBlend = saturate(dot(normalize(OUT.worldPos.xyz), half3(0.0f, 0.0f, 1.0f)));
	zenithBlend = saturate(zenithBlend / 0.2);
	OUT.worldPos.w = zenithBlend;

	// Calculate offset for bottom of cloud
	const half2 ofs = (cloudOffset / 10.0f) * (sunDirection.xyz - viewDir.xyz).xy;

	// This is used to offset clouds away from the sun (the base layer)
	OUT.texCoord.zw = IN.texCoord0.xy + sphericalWarpOffset(IN.texCoord0.xy, -ofs);

	OUT.detailTexCoord.xy = ((IN.texCoord0.xy - 0.5) + edgeDetailOffset) * cloudEdgeDetailScale ; // for edge of clouds
	OUT.detailTexCoord.zw = ((IN.texCoord0.yx - 0.5) + overallDetailOffset) * cloudOverallDetailScale; // for sky in general
	OUT.ditherTexCoord.xy = IN.texCoord1.xy * 64.0; // for dither texture (2nd UV set)
	OUT.smallCloudTexCoords.xy = ((IN.texCoord0.xy - 0.5) + smallCloudOffset) * smallCloudDetailScale; // small cloud tex coords
	OUT.starTexCoords.xy = IN.texCoord1.xy * 12.0; // star uv coords (2nd UV set)

	return OUT;
}

// ------------------------------------------------------------------------------------------------------------------//

VS_sky_OUTPUT vs_sky_main(
						  float4 transPos, VS_sky_INPUT IN, 
						  uniform bool fogEnabled, 
						  uniform bool sunEnabled, 
						  uniform bool moonEnabled, 
						  uniform bool cloudsEnabled, 
						  uniform bool starsEnabled,
						  uniform bool debugEnabled
						  )
{
	return vs_sky_main(transPos, IN, fogEnabled, sunEnabled, moonEnabled, cloudsEnabled, starsEnabled, debugEnabled, false);
}

// ------------------------------------------------------------------------------------------------------------------//

float4 ps_sky_main(
	PS_sky_INPUT IN, 
	uniform bool fogEnabled,
	uniform bool sunEnabled, 
	uniform bool moonEnabled, 
	uniform bool cloudsEnabled, 
	uniform bool starsEnabled,
	uniform bool debugEnabled)
{

#if !defined(SHADER_FINAL)
	if (debugEnabled)
	{
		IN.texCoord                    *= debugCloudsParams[2].x;
		IN.detailTexCoord              *= debugCloudsParams[2].x;
		IN.ditherAndSmallCloudTexCoord *= debugCloudsParams[2].x;

		// note: we've modified the texcoords in 'IN' above ..
		TEX_DATA texData = getTexData(IN);

		const float4 texData_perlin_texcoord_x = float4(IN.texCoord.xz, 0, 0);
		const float4 texData_perlin_texcoord_y = float4(IN.texCoord.yw, 0, 0);
		const float4 texData_detail_texcoord_x = float4(IN.detailTexCoord.x, IN.smallCloudTexCoords.x, IN.detailTexCoord.z, IN.ditherTexCoord.x);
		const float4 texData_detail_texcoord_y = float4(IN.detailTexCoord.y, IN.smallCloudTexCoords.y, IN.detailTexCoord.w, IN.ditherTexCoord.y);

		const float4 texData_perlin = float4(texData.perlin, texData.offsetPerlin);
		const float4 texData_detail = float4(texData.detailEdge, texData.detailOverall, texData.dither);

		float3 OUT;
		OUT.x = dot(texData_perlin_texcoord_x, debugCloudsParams[0]) + dot(texData_detail_texcoord_x, debugCloudsParams[1]);
		OUT.y = dot(texData_perlin_texcoord_y, debugCloudsParams[0]) + dot(texData_detail_texcoord_y, debugCloudsParams[1]);
		OUT.z = dot(texData_perlin           , debugCloudsParams[0]) + dot(texData_detail           , debugCloudsParams[1]);

		if (debugCloudsParams[2].y == 4) // wireframe
		{
			return float4(0.5,0,1,1);
		}
		else if (debugCloudsParams[2].y == 2) // density (large=red, small=green)
		{
			return float4(calcCloudData(IN, normalize(IN.worldPos), texData).alpha.xy, 0, 1);
		}
		else
		{
			return float4(lerp(OUT.zzz, float3(frac(OUT.xy), 0), debugCloudsParams[2].y), 1);
		}
	}
#endif // !defined(SHADER_FINAL)

	TEX_DATA texData = getTexData(IN);

	// view and adjusted moon direction
	half3 viewDir = normalize(IN.worldPos); 
	half3 skyColor = half3(0.0, 0.0, 0.0);
	SUN_RESULT sunResults = (SUN_RESULT)0;
	CLOUD_RESULT cloudResults = (CLOUD_RESULT)0;
	const float moonAngle = dot(viewDir, moonDirection);

	// Add in sun to sky color, make sure its not additive
	sunResults = calcSunData(viewDir);
	skyColor += IN.skyCol.rgb * sunResults.invAlpha;
	skyColor += sunResults.color;

	if (cloudsEnabled)
	{
		// Blend in small clouds and then big clouds
		cloudResults = calcCloudData(IN, viewDir, texData);
		skyColor = lerp(skyColor, smallCloudColorHdr, cloudResults.alpha.y);
		skyColor = lerp(skyColor, cloudResults.color * cloudHdrIntensity, cloudResults.alpha.x);
	}
	else
	{
		// These are needed for effects
		cloudResults.dimming = 0.0f;
		cloudResults.noise = texData.perlin.x;
	}
	
	if (sunEnabled || moonEnabled)
	{
		// Add in effects
		EFFECTS_RESULT effectResults;
		
		if (sunEnabled && !moonEnabled)
		{
			effectResults = calcEffectsDataSun(viewDir, cloudResults, sunResults);
		}

		if (moonEnabled && !sunEnabled)
		{
			effectResults = calcEffectsDataMoon(viewDir, cloudResults, moonAngle);
		}

		if (sunEnabled && moonEnabled)
		{
			effectResults = calcEffectsDataSunMoon(viewDir, cloudResults, sunResults, moonAngle);
		}

		skyColor += effectResults.color;
	}

	if (starsEnabled)
	{
		// Stars (don't add where there are clouds or the moon)
		const half moonDisk = step(0.00054475, 1.0 - moonAngle);
		half3 stars = saturate(texData.stars * 2.0 - 0.1f);
		skyColor += stars * starfieldIntensity * moonDisk * (1.0 - cloudResults.dimming);
	}

	// need to blend in fog here
	if (fogEnabled)
	{
		#if defined(USE_FOGRAY_FORWARDPASS) && !__LOW_QUALITY
			IN.fogData.a *= GetFogRayIntensity(IN.pos.xy * gooScreenSize.xy);
		#endif
	}	

	skyColor = lerp(skyColor, IN.fogData.rgb, IN.fogData.a);
	
	// Dithering
	skyColor += texData.dither*kDitherScale;

	// Write final colour with alpha
	half4 finalColor =  half4(skyColor, saturate(IN.worldPos.w - (cloudResults.alpha.x + cloudResults.alpha.y)));
	return finalColor;
}

// ------------------------------------------------------------------------------------------------------------------//

GEN_SKY_FUNCS(sky, none,              SUN_OFF, MOON_OFF, CLOUDS_OFF, STARS_OFF, DEBUG_OFF)
GEN_SKY_FUNCS(sky, stars,             SUN_OFF, MOON_OFF, CLOUDS_OFF, STARS_ON,  DEBUG_OFF)
GEN_SKY_FUNCS(sky, clouds,            SUN_OFF, MOON_OFF, CLOUDS_ON,  STARS_OFF, DEBUG_OFF)
GEN_SKY_FUNCS(sky, clouds_stars,      SUN_OFF, MOON_OFF, CLOUDS_ON,  STARS_ON,  DEBUG_OFF)
GEN_SKY_FUNCS(sky, moon,              SUN_OFF, MOON_ON,  CLOUDS_OFF, STARS_OFF, DEBUG_OFF)
GEN_SKY_FUNCS(sky, moon_stars,        SUN_OFF, MOON_ON,  CLOUDS_OFF, STARS_ON,  DEBUG_OFF)
GEN_SKY_FUNCS(sky, moon_clouds,       SUN_OFF, MOON_ON,  CLOUDS_ON,  STARS_OFF, DEBUG_OFF)
GEN_SKY_FUNCS(sky, moon_clouds_stars, SUN_OFF, MOON_ON,  CLOUDS_ON,  STARS_ON,  DEBUG_OFF)
GEN_SKY_FUNCS(sky, sun,               SUN_ON,  MOON_OFF, CLOUDS_OFF, STARS_OFF, DEBUG_OFF)
GEN_SKY_FUNCS(sky, sun_stars,         SUN_ON,  MOON_OFF, CLOUDS_OFF, STARS_ON,  DEBUG_OFF)
GEN_SKY_FUNCS(sky, sun_clouds,        SUN_ON,  MOON_OFF, CLOUDS_ON,  STARS_OFF, DEBUG_OFF)
GEN_SKY_FUNCS(sky, sun_clouds_stars,  SUN_ON,  MOON_OFF, CLOUDS_ON,  STARS_ON,  DEBUG_OFF)
GEN_SKY_FUNCS(sky, sun_moon,          SUN_ON,  MOON_ON,  CLOUDS_OFF, STARS_OFF, DEBUG_OFF)
GEN_SKY_FUNCS(sky, sun_moon_stars,    SUN_ON,  MOON_ON,  CLOUDS_OFF, STARS_ON,  DEBUG_OFF)
GEN_SKY_FUNCS(sky, sun_moon_clouds,   SUN_ON,  MOON_ON,  CLOUDS_ON,  STARS_OFF, DEBUG_OFF)
GEN_SKY_FUNCS(sky, all,               SUN_ON,  MOON_ON,  CLOUDS_ON,  STARS_ON,  DEBUG_OFF)
#if !defined(SHADER_FINAL)
GEN_SKY_FUNCS(sky, dbg_clouds,        SUN_OFF, MOON_OFF, CLOUDS_ON,  STARS_OFF, DEBUG_ON )
#endif // !defined(SHADER_FINAL)
// ------------------------------------------------------------------------------------------------------------------//

technique sky
{
	GEN_PASS(sky, normal, none)
	GEN_PASS(sky, normal, stars)
	GEN_PASS(sky, normal, clouds)
	GEN_PASS(sky, normal, clouds_stars)
	GEN_PASS(sky, normal, moon)
	GEN_PASS(sky, normal, moon_stars)
	GEN_PASS(sky, normal, moon_clouds)
	GEN_PASS(sky, normal, moon_clouds_stars)
	GEN_PASS(sky, normal, sun)
	GEN_PASS(sky, normal, sun_stars)
	GEN_PASS(sky, normal, sun_clouds)
	GEN_PASS(sky, normal, sun_clouds_stars)
	GEN_PASS(sky, normal, sun_moon)
	GEN_PASS(sky, normal, sun_moon_stars)
	GEN_PASS(sky, normal, sun_moon_clouds)
	GEN_PASS(sky, normal, all)
#if !defined(SHADER_FINAL)
	GEN_PASS(sky, normal, dbg_clouds)
#endif // !defined(SHADER_FINAL)
}

technique sky_water_reflection
{
	GEN_PASS(sky, water_reflection, none)
	GEN_PASS(sky, water_reflection, stars)
	GEN_PASS(sky, water_reflection, clouds)
	GEN_PASS(sky, water_reflection, clouds_stars)
	GEN_PASS(sky, water_reflection, moon)
	GEN_PASS(sky, water_reflection, moon_stars)
	GEN_PASS(sky, water_reflection, moon_clouds)
	GEN_PASS(sky, water_reflection, moon_clouds_stars)
	GEN_PASS(sky, water_reflection, sun)
	GEN_PASS(sky, water_reflection, sun_stars)
	GEN_PASS(sky, water_reflection, sun_clouds)
	GEN_PASS(sky, water_reflection, sun_clouds_stars)
	GEN_PASS(sky, water_reflection, sun_moon)
	GEN_PASS(sky, water_reflection, sun_moon_stars)
	GEN_PASS(sky, water_reflection, sun_moon_clouds)
	GEN_PASS(sky, water_reflection, all)
#if !defined(SHADER_FINAL)
//	GEN_PASS(sky, water_reflection, dbg_clouds) -- don't need this for sky_water_reflection
#endif // !defined(SHADER_FINAL)
}

technique sky_mirror_reflection
{
	GEN_PASS(sky, mirror_reflection, none)
	GEN_PASS(sky, mirror_reflection, stars)
	GEN_PASS(sky, mirror_reflection, clouds)
	GEN_PASS(sky, mirror_reflection, clouds_stars)
	GEN_PASS(sky, mirror_reflection, moon)
	GEN_PASS(sky, mirror_reflection, moon_stars)
	GEN_PASS(sky, mirror_reflection, moon_clouds)
	GEN_PASS(sky, mirror_reflection, moon_clouds_stars)
	GEN_PASS(sky, mirror_reflection, sun)
	GEN_PASS(sky, mirror_reflection, sun_stars)
	GEN_PASS(sky, mirror_reflection, sun_clouds)
	GEN_PASS(sky, mirror_reflection, sun_clouds_stars)
	GEN_PASS(sky, mirror_reflection, sun_moon)
	GEN_PASS(sky, mirror_reflection, sun_moon_stars)
	GEN_PASS(sky, mirror_reflection, sun_moon_clouds)
	GEN_PASS(sky, mirror_reflection, all)
#if !defined(SHADER_FINAL)
//	GEN_PASS(sky, mirror_reflection, dbg_clouds) -- don't need this for sky_mirror_reflection
#endif // !defined(SHADER_FINAL)
}

#if GS_INSTANCED_CUBEMAP
GEN_GSINST_TYPE(Sky,VS_sky_OUTPUT,SV_RenderTargetArrayIndex)

GEN_GSINST_FUNC(Sky,sky_none,VS_sky_INPUT,VS_sky_OUTPUT,PS_sky_INPUT
				,vs_sky_main(mul(IN.pos, mul(gInstWorld,gInstWorldViewProj[IN.InstID])),IN,FOG_ON,SUN_OFF, MOON_OFF, CLOUDS_OFF, STARS_OFF, DEBUG_OFF,true)
				,CastOutHalf4Color(PackHdr(ps_sky_main(IN,FOG_ON,SUN_OFF, MOON_OFF, CLOUDS_OFF, STARS_OFF, DEBUG_OFF))))
GEN_GSINST_FUNC(Sky,sky_stars,VS_sky_INPUT,VS_sky_OUTPUT,PS_sky_INPUT
				,vs_sky_main(mul(IN.pos, mul(gInstWorld,gInstWorldViewProj[IN.InstID])),IN,FOG_ON,SUN_OFF, MOON_OFF, CLOUDS_OFF, STARS_ON,  DEBUG_OFF,true)
				,CastOutHalf4Color(PackHdr(ps_sky_main(IN,FOG_ON,SUN_OFF, MOON_OFF, CLOUDS_OFF, STARS_ON,  DEBUG_OFF))))
GEN_GSINST_FUNC(Sky,sky_clouds,VS_sky_INPUT,VS_sky_OUTPUT,PS_sky_INPUT
				,vs_sky_main(mul(IN.pos, mul(gInstWorld,gInstWorldViewProj[IN.InstID])),IN,FOG_ON,SUN_OFF, MOON_OFF, CLOUDS_ON,  STARS_OFF, DEBUG_OFF,true)
				,CastOutHalf4Color(PackHdr(ps_sky_main(IN,FOG_ON,SUN_OFF, MOON_OFF, CLOUDS_ON,  STARS_OFF, DEBUG_OFF))))
GEN_GSINST_FUNC(Sky,sky_clouds_stars,VS_sky_INPUT,VS_sky_OUTPUT,PS_sky_INPUT
				,vs_sky_main(mul(IN.pos, mul(gInstWorld,gInstWorldViewProj[IN.InstID])),IN,FOG_ON,SUN_OFF, MOON_OFF, CLOUDS_ON,  STARS_ON,  DEBUG_OFF,true)
				,CastOutHalf4Color(PackHdr(ps_sky_main(IN,FOG_ON,SUN_OFF, MOON_OFF, CLOUDS_ON,  STARS_ON,  DEBUG_OFF))))
GEN_GSINST_FUNC(Sky,sky_moon,VS_sky_INPUT,VS_sky_OUTPUT,PS_sky_INPUT
				,vs_sky_main(mul(IN.pos, mul(gInstWorld,gInstWorldViewProj[IN.InstID])),IN,FOG_ON,SUN_OFF, MOON_ON,  CLOUDS_OFF, STARS_OFF, DEBUG_OFF,true)
				,CastOutHalf4Color(PackHdr(ps_sky_main(IN,FOG_ON,SUN_OFF, MOON_ON,  CLOUDS_OFF, STARS_OFF, DEBUG_OFF))))
GEN_GSINST_FUNC(Sky,sky_moon_stars,VS_sky_INPUT,VS_sky_OUTPUT,PS_sky_INPUT
				,vs_sky_main(mul(IN.pos, mul(gInstWorld,gInstWorldViewProj[IN.InstID])),IN,FOG_ON,SUN_OFF, MOON_ON,  CLOUDS_OFF, STARS_ON,  DEBUG_OFF,true)
				,CastOutHalf4Color(PackHdr(ps_sky_main(IN,FOG_ON,SUN_OFF, MOON_ON,  CLOUDS_OFF, STARS_ON,  DEBUG_OFF))))
GEN_GSINST_FUNC(Sky,sky_moon_clouds,VS_sky_INPUT,VS_sky_OUTPUT,PS_sky_INPUT
				,vs_sky_main(mul(IN.pos, mul(gInstWorld,gInstWorldViewProj[IN.InstID])),IN,FOG_ON,SUN_OFF, MOON_ON,  CLOUDS_ON,  STARS_OFF, DEBUG_OFF,true)
				,CastOutHalf4Color(PackHdr(ps_sky_main(IN,FOG_ON,SUN_OFF, MOON_ON,  CLOUDS_ON,  STARS_OFF, DEBUG_OFF))))
GEN_GSINST_FUNC(Sky,sky_moon_clouds_stars,VS_sky_INPUT,VS_sky_OUTPUT,PS_sky_INPUT
				,vs_sky_main(mul(IN.pos, mul(gInstWorld,gInstWorldViewProj[IN.InstID])),IN,FOG_ON,SUN_OFF, MOON_ON,  CLOUDS_ON,  STARS_ON,  DEBUG_OFF,true)
				,CastOutHalf4Color(PackHdr(ps_sky_main(IN,FOG_ON,SUN_OFF, MOON_ON,  CLOUDS_ON,  STARS_ON,  DEBUG_OFF))))
GEN_GSINST_FUNC(Sky,sky_sun,VS_sky_INPUT,VS_sky_OUTPUT,PS_sky_INPUT
				,vs_sky_main(mul(IN.pos, mul(gInstWorld,gInstWorldViewProj[IN.InstID])),IN,FOG_ON,SUN_ON,  MOON_OFF, CLOUDS_OFF, STARS_OFF, DEBUG_OFF,true)
				,CastOutHalf4Color(PackHdr(ps_sky_main(IN,FOG_ON,SUN_ON,  MOON_OFF, CLOUDS_OFF, STARS_OFF, DEBUG_OFF))))
GEN_GSINST_FUNC(Sky,sky_sun_stars,VS_sky_INPUT,VS_sky_OUTPUT,PS_sky_INPUT
				,vs_sky_main(mul(IN.pos, mul(gInstWorld,gInstWorldViewProj[IN.InstID])),IN,FOG_ON,SUN_ON,  MOON_OFF, CLOUDS_OFF, STARS_ON,  DEBUG_OFF,true)
				,CastOutHalf4Color(PackHdr(ps_sky_main(IN,FOG_ON,SUN_ON,  MOON_OFF, CLOUDS_OFF, STARS_ON,  DEBUG_OFF))))
GEN_GSINST_FUNC(Sky,sky_sun_clouds,VS_sky_INPUT,VS_sky_OUTPUT,PS_sky_INPUT
				,vs_sky_main(mul(IN.pos, mul(gInstWorld,gInstWorldViewProj[IN.InstID])),IN,FOG_ON,SUN_ON,  MOON_OFF, CLOUDS_ON,  STARS_OFF, DEBUG_OFF,true)
				,CastOutHalf4Color(PackHdr(ps_sky_main(IN,FOG_ON,SUN_ON,  MOON_OFF, CLOUDS_ON,  STARS_OFF, DEBUG_OFF))))
GEN_GSINST_FUNC(Sky,sky_sun_clouds_stars,VS_sky_INPUT,VS_sky_OUTPUT,PS_sky_INPUT
				,vs_sky_main(mul(IN.pos, mul(gInstWorld,gInstWorldViewProj[IN.InstID])),IN,FOG_ON,SUN_ON,  MOON_OFF, CLOUDS_ON,  STARS_ON,  DEBUG_OFF,true)
				,CastOutHalf4Color(PackHdr(ps_sky_main(IN,FOG_ON,SUN_ON,  MOON_OFF, CLOUDS_ON,  STARS_ON,  DEBUG_OFF))))
GEN_GSINST_FUNC(Sky,sky_sun_moon,VS_sky_INPUT,VS_sky_OUTPUT,PS_sky_INPUT
				,vs_sky_main(mul(IN.pos, mul(gInstWorld,gInstWorldViewProj[IN.InstID])),IN,FOG_ON,SUN_ON,  MOON_ON,  CLOUDS_OFF, STARS_OFF, DEBUG_OFF,true)
				,CastOutHalf4Color(PackHdr(ps_sky_main(IN,FOG_ON,SUN_ON,  MOON_ON,  CLOUDS_OFF, STARS_OFF, DEBUG_OFF))))
GEN_GSINST_FUNC(Sky,sky_sun_moon_stars,VS_sky_INPUT,VS_sky_OUTPUT,PS_sky_INPUT
				,vs_sky_main(mul(IN.pos, mul(gInstWorld,gInstWorldViewProj[IN.InstID])),IN,FOG_ON,SUN_ON,  MOON_ON,  CLOUDS_OFF, STARS_ON,  DEBUG_OFF,true)
				,CastOutHalf4Color(PackHdr(ps_sky_main(IN,FOG_ON,SUN_ON,  MOON_ON,  CLOUDS_OFF, STARS_ON,  DEBUG_OFF))))
GEN_GSINST_FUNC(Sky,sky_sun_moon_clouds,VS_sky_INPUT,VS_sky_OUTPUT,PS_sky_INPUT
				,vs_sky_main(mul(IN.pos, mul(gInstWorld,gInstWorldViewProj[IN.InstID])),IN,FOG_ON,SUN_ON,  MOON_ON,  CLOUDS_ON,  STARS_OFF, DEBUG_OFF,true)
				,CastOutHalf4Color(PackHdr(ps_sky_main(IN,FOG_ON,SUN_ON,  MOON_ON,  CLOUDS_ON,  STARS_OFF, DEBUG_OFF))))
GEN_GSINST_FUNC(Sky,sky_all,VS_sky_INPUT,VS_sky_OUTPUT,PS_sky_INPUT
				,vs_sky_main(mul(IN.pos, mul(gInstWorld,gInstWorldViewProj[IN.InstID])),IN,FOG_ON,SUN_ON,  MOON_ON,  CLOUDS_ON,  STARS_ON,  DEBUG_OFF,true)
				,CastOutHalf4Color(PackHdr(ps_sky_main(IN,FOG_ON,SUN_ON,  MOON_ON,  CLOUDS_ON,  STARS_ON,  DEBUG_OFF))))
#if !defined(SHADER_FINAL)
GEN_GSINST_FUNC(Sky, dbg_clouds,VS_sky_INPUT,VS_sky_OUTPUT,PS_sky_INPUT
				,vs_sky_main(mul(IN.pos, mul(gInstWorld,gInstWorldViewProj[IN.InstID])),IN,FOG_ON,SUN_OFF, MOON_OFF, CLOUDS_ON,  STARS_OFF, DEBUG_ON,true)
				,CastOutHalf4Color(PackHdr(ps_sky_main(IN,FOG_ON,SUN_OFF, MOON_OFF, CLOUDS_ON,  STARS_OFF, DEBUG_ON))))
#endif // !defined(SHADER_FINAL)

technique sky_cubemap
{
	GEN_GSINST_TECHPASS(sky_none, none)
	GEN_GSINST_TECHPASS(sky_stars, stars)
	GEN_GSINST_TECHPASS(sky_clouds, clouds)
	GEN_GSINST_TECHPASS(sky_clouds_stars, clouds_stars)
	GEN_GSINST_TECHPASS(sky_moon, moon)
	GEN_GSINST_TECHPASS(sky_moon_stars, moon_stars)
	GEN_GSINST_TECHPASS(sky_moon_clouds, moon_clouds)
	GEN_GSINST_TECHPASS(sky_moon_clouds_stars, moon_clouds_stars)
	GEN_GSINST_TECHPASS(sky_sun, sun)
	GEN_GSINST_TECHPASS(sky_sun_stars, sun_stars)
	GEN_GSINST_TECHPASS(sky_sun_clouds, sun_clouds)
	GEN_GSINST_TECHPASS(sky_sun_clouds_stars, sun_clouds_stars)
	GEN_GSINST_TECHPASS(sky_sun_moon, sun_moon)
	GEN_GSINST_TECHPASS(sky_sun_moon_stars, sun_moon_stars)
	GEN_GSINST_TECHPASS(sky_sun_moon_clouds, sun_moon_clouds)
	GEN_GSINST_TECHPASS(sky_all, all)
}
#endif
// ------------------------------------------------------------------------------------------------------------------//
//												MOON TECHNIQUES														 //
// ------------------------------------------------------------------------------------------------------------------//
float4 vs_moon_main(float4 inPos
#if GS_INSTANCED_CUBEMAP
					,uniform bool bUseInst,uint InstID
#endif
					)
{
	// Create the billboard matrix, don't use the camera up vector as this 
	// will make the moon rotate around as that changes, so its fixed
	float3x3 bMat;
#if GS_INSTANCED_CUBEMAP
	if (bUseInst)
		bMat[1] = normalize(gInstViewInverse[InstID][3].xyz - moonPosition.xyz);
	else
#endif
		bMat[1] = normalize(gViewInverse[3].xyz - moonPosition.xyz);

	bMat[0] = normalize(cross(float3(0, 0, 1.0), bMat[1]));
	bMat[2] = cross(bMat[1], bMat[0]);

	return float4(mul(inPos.xyz, bMat) + moonPosition, inPos.w);
}

// ------------------------------------------------------------------------------------------------------------------//

VS_moon_OUTPUT vs_moon_common(VS_moon_INPUT IN
#if GS_INSTANCED_CUBEMAP
							  ,uniform bool bUseInst
#endif
							  )
{
	float4 pos = vs_moon_main(IN.inPos
#if GS_INSTANCED_CUBEMAP
		,bUseInst
		,IN.InstID
#endif
		);

	VS_moon_OUTPUT OUT;
	
#if GS_INSTANCED_CUBEMAP
	if (bUseInst)
	{
		OUT.pos = mul(pos, mul(gInstWorld,gInstWorldViewProj[IN.InstID]));
		OUT.worldPos = mul(pos, gInstWorld);
		OUT.worldPos.w = saturate(1.0 - CalcFogDataNoHaze(normalize(OUT.worldPos - gInstViewInverse[IN.InstID][3].xyz) * 25000.0f).a);
	}
	else
#endif
	{
		OUT.pos = mul(pos, gWorldViewProj);
		OUT.worldPos = mul(pos, gWorld);
		OUT.worldPos.w = saturate(1.0 - CalcFogDataNoHaze(normalize(OUT.worldPos - gViewInverse[3].xyz) * 25000.0f).a);
	}
	OUT.texCoord.xy = float2(IN.inTexCoords.x, 1.0f - IN.inTexCoords.y);

	float zenithBlend = saturate(dot(normalize(OUT.worldPos.xyz), half3(0.0f, 0.0f, 1.0f)));
	zenithBlend = saturate(zenithBlend / 0.2);
	OUT.worldPos.w *= zenithBlend;

	return OUT;
}

VS_moon_OUTPUT vs_moon(VS_moon_INPUT IN)
{
	return vs_moon_common(IN
#if GS_INSTANCED_CUBEMAP
		,false
#endif
		);
}

// ------------------------------------------------------------------------------------------------------------------//

half4 ps_moon_main(VS_moon_OUTPUT IN)
{
	half4 moonBrightness = h4tex2D(moonSampler, IN.texCoord.xy);

	// Tex-coords are stretched out so bring them back into [0..1]
	float2 normCoords = IN.texCoord.xy - float2(0.5, 0.5);
	normCoords *= 2.0;

	// Work out normal
	float3 normal = float3(normCoords.x, sqrt(1.0 - dot(normCoords.xy, normCoords.xy)), normCoords.y);
	normal = normalize(normal);

	// Do standard diffuse lighting on the fake sphere
	half3 L = normalize(lunarCycle);
	half light = max(0.0, dot(normal, L));

	const half3 moon = (moonBrightness.rgb * moonColor) * light * moonIntensity;
	half4 result = half4(moon, 1.0f);

	result.rgb = result.rgb * moonBrightness.a * IN.worldPos.w;

	return result;
}

// ------------------------------------------------------------------------------------------------------------------//

OutHalf4Color ps_moon(VS_moon_OUTPUT IN)
{
	return CastOutHalf4Color(PackHdr(ps_moon_main(IN)));
}

OutHalf4Color ps_moon_water_reflection(VS_moon_OUTPUT IN)
{
	half4 mainMoonColor = ps_moon_main(IN);
	mainMoonColor.rgb *= 0.03125;
	return CastOutHalf4Color(mainMoonColor);
}

OutHalf4Color ps_moon_mirror_reflection(VS_moon_OUTPUT IN)
{
	half4 mainMoonColor = ps_moon_main(IN);
	return CastOutHalf4Color(mainMoonColor);
}

// ------------------------------------------------------------------------------------------------------------------//

technique moon
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER vs_moon();
		PixelShader = compile PIXELSHADER ps_moon()  OPTIMIZE;
	}
}

technique moon_water_reflection
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER vs_moon();
		PixelShader = compile PIXELSHADER ps_moon_water_reflection() OPTIMIZE;
	}
}

technique moon_mirror_reflection
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER vs_moon();
		PixelShader = compile PIXELSHADER ps_moon_mirror_reflection() OPTIMIZE;
	}
}

#if GS_INSTANCED_CUBEMAP
GEN_GSINST_TYPE(Moon,VS_moon_OUTPUT,SV_RenderTargetArrayIndex)
GEN_GSINST_FUNC(Moon,Moon,VS_moon_INPUT,VS_moon_OUTPUT,VS_moon_OUTPUT,vs_moon_common(IN,true),ps_moon(IN))

technique moon_cubemap
{
	GEN_GSINST_TECHPASS(Moon, all)
}
#endif
// ------------------------------------------------------------------------------------------------------------------//
//												SUN TECHNIQUES														 //
// ------------------------------------------------------------------------------------------------------------------//

float4 vs_sun_main(float4 inPos
#if GS_INSTANCED_CUBEMAP
					,uniform bool bUseInst, uint InstID
#endif
				   )
{
	float3x3 bMat;

#if GS_INSTANCED_CUBEMAP
	if (bUseInst)
		bMat[1] = normalize(gInstViewInverse[InstID][3].xyz - sunPosition.xyz);
	else
#endif
		bMat[1] = normalize(gViewInverse[3].xyz - sunPosition.xyz);

	bMat[0] = normalize(cross(float3(0, 0, 1.0), bMat[1]));
	bMat[2] = cross(bMat[1], bMat[0]);

	return float4(mul(inPos.xyz, bMat) + sunPosition.xyz, inPos.w);
}

// ------------------------------------------------------------------------------------------------------------------//

VS_sun_OUTPUT vs_sun_common(VS_sun_INPUT IN
#if GS_INSTANCED_CUBEMAP
							,uniform bool bUseInst
#endif
							)
{
	float4 pos = vs_sun_main(IN.inPos
#if GS_INSTANCED_CUBEMAP
		,bUseInst, IN.InstID
#endif
		);

	VS_sun_OUTPUT OUT;

#if GS_INSTANCED_CUBEMAP
	if (bUseInst)
	{
		OUT.pos = mul(pos, mul(gInstWorld,gInstWorldViewProj[IN.InstID]));
		float3 worldPos = (float3)mul(pos, gInstWorld);
		OUT.fogData = CalcFogDataNoHaze(normalize(worldPos - gInstViewInverse[IN.InstID][3].xyz) * 25000.0f);
	}
	else
#endif
	{
		OUT.pos = mul(pos, gWorldViewProj);
		float3 worldPos = (float3)mul(pos, gWorld);
		OUT.fogData = CalcFogDataNoHaze(normalize(worldPos - gViewInverse[3].xyz) * 25000.0f);
	}

	return OUT;
}

VS_sun_OUTPUT vs_sun(VS_sun_INPUT IN)
{
	return vs_sun_common(IN
#if GS_INSTANCED_CUBEMAP
		,false
#endif
		);
}
// ------------------------------------------------------------------------------------------------------------------//

half4 ps_sun_main(half4 fogData)
{
	half4 result = half4(sunDiscColorHdr, 1.0);
	result.rgb = lerp(result.rgb, fogData.rgb, fogData.a);
	return result;
}

// ------------------------------------------------------------------------------------------------------------------//

OutHalf4Color ps_sun(VS_sun_OUTPUT IN)
{
	return CastOutHalf4Color(PackHdr(ps_sun_main(IN.fogData)));
}

// ------------------------------------------------------------------------------------------------------------------//

OutHalf4Color ps_sun_water_reflection(VS_sun_OUTPUT IN)
{
	return CastOutHalf4Color(PackReflection(ps_sun_main(IN.fogData)));
}

// ------------------------------------------------------------------------------------------------------------------//

OutHalf4Color ps_sun_mirror_reflection(VS_sun_OUTPUT IN)
{
	return CastOutHalf4Color(PackHdr(ps_sun_main(IN.fogData)));
}

// ------------------------------------------------------------------------------------------------------------------//

OutHalf4Color ps_sun_seethrough(VS_sun_OUTPUT IN)
{
	half4 result;
	result.xy = 0.0f.xx;	
	float visibility = 0.5f;
	result.z = visibility;
	result.w = 1.0f;
	
	return CastOutHalf4Color(result);
}

// ------------------------------------------------------------------------------------------------------------------//

technique sun
{
	pass p0
	{
		AlphaBlendEnable = true;
		SrcBlend = DESTALPHA;
		DestBlend = INVDESTALPHA;

		VertexShader = compile VERTEXSHADER vs_sun();
		PixelShader = compile PIXELSHADER ps_sun()  OPTIMIZE;
	}
}

technique sun_water_reflection
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER vs_sun();
		PixelShader = compile PIXELSHADER ps_sun_water_reflection() OPTIMIZE;
	}
}

technique sun_mirror_reflection
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER vs_sun();
		PixelShader = compile PIXELSHADER ps_sun_mirror_reflection() OPTIMIZE;
	}
}

technique sun_seethrough
{
	pass p0
	{
		AlphaBlendEnable = true;
		SrcBlend = DESTALPHA;
		DestBlend = INVDESTALPHA;

		VertexShader = compile VERTEXSHADER vs_sun();
		PixelShader = compile PIXELSHADER ps_sun_seethrough()  OPTIMIZE;
	}
}

#if GS_INSTANCED_CUBEMAP
GEN_GSINST_TYPE(Sun,VS_sun_OUTPUT,SV_RenderTargetArrayIndex)
GEN_GSINST_FUNC(Sun,Sun,VS_sun_INPUT,VS_sun_OUTPUT,VS_sun_OUTPUT,vs_sun_common(IN,true),ps_sun(IN))

technique sun_cubemap
{
	GEN_GSINST_TECHPASS(Sun,all)
}
#endif

// ------------------------------------------------------------------------------------------------------------------//
//												PERLIN TECHNIQUES													 //
// ------------------------------------------------------------------------------------------------------------------//

VS_PERLIN_OUTPUT vs_perlin(float4 inPos: POSITION0)
{
	VS_PERLIN_OUTPUT OUT;

	OUT.pos = float4(inPos.xy * 2.0 - 1.0, 0.0, 1.0);
	OUT.texCoord.x = inPos.x;
	OUT.texCoord.y = 1.0 - inPos.y;

	return OUT;
}

// ------------------------------------------------------------------------------------------------------------------//

OutHalf4Color ps_perlin_main(VS_PERLIN_OUTPUT IN)
{
	const half finalNoise = calculateCloudNoise(IN) * 0.5f + 0.5f;

	const half minValue = noiseThreshold - noiseSoftness, 
		maxValue = noiseThreshold + noiseSoftness;

	return CastOutHalf4Color(half4(smoothstep(minValue, maxValue, finalNoise),
		smoothstep(minValue + noiseDensityOffset, 
		maxValue + noiseDensityOffset, 
		finalNoise),
		finalNoise,
		0.0));
}

// ------------------------------------------------------------------------------------------------------------------//

VS_sun_vis_OUTPUT vs_sun_visibility(float4 inPos: POSITION)
{	
	float4 pos = vs_sun_main(inPos
#if GS_INSTANCED_CUBEMAP
			,false,0
#endif
		);

	VS_sun_vis_OUTPUT OUT;
	OUT.pos = mul(pos, gWorldViewProj);
	OUT.fogBlend = 1.0 - CalcFogDataNoHaze(normalize(mul(pos, gWorld).xyz - gViewInverse[3].xyz) * 25000.0f).a * noiseSoftness;
	
	return OUT;
}

// ------------------------------------------------------------------------------------------------------------------//
#if !__XENON
void StippleAlpha(float fStippleAlpha, float2 vPos)
{
#if __WIN32PC && __SHADERMODEL < 40
	half2 s = frac(vPos * 0.5f); // PC + DX9 VPOS is always a whole number
#else
	half2 s = frac((vPos - 0.5) * 0.5f); // PS3 or PC + DX10/11 VPOS always contains .5
#endif // __WIN32PC && __SHADERMODEL < 40

	// This is scientific
	// s:
	//   Even  Odd
	// /-----------\
	// |.0,.0|.5,.0|
	// |-----------|
	// |.0,.5|.5,.5|
	// \-----------/
	// fAlphaRef:
	// /-------\
	// |.2 |.7 |
	// |-------|
	// |.45|.95|
	// \-------/
	half fAlphaRef = 0.5 * dot( s.xy, float2(1.0, 2.0)) + 0.2;
	
	rageDiscard(fStippleAlpha <= fAlphaRef || fStippleAlpha < noiseThreshold);
}
#endif // !__XENON
// ------------------------------------------------------------------------------------------------------------------//

OutHalf4Color ps_sun_visibility(PS_sun_vis_INPUT IN)
{
#if !__XENON
	StippleAlpha(IN.fogBlend.x, IN.pos);
#endif // !__XENON

	return CastOutHalf4Color(1.0f);
}

// ------------------------------------------------------------------------------------------------------------------//

technique draw_perlin_noise
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER vs_perlin();
		PixelShader = compile PIXELSHADER ps_perlin_main() OPTIMIZE;
	}
}

// ------------------------------------------------------------------------------------------------------------------//
//											SKY PLANE TECHNIQUES													 //
// ------------------------------------------------------------------------------------------------------------------//
VS_SKY_PLANE_OUTPUT vs_sky_plane_common(VS_SKY_PLANE_INPUT IN
#if GS_INSTANCED_CUBEMAP
										,uniform bool bUseInst
#endif
										)
{
	VS_SKY_PLANE_OUTPUT OUT;

	float3 camPos = float3(0.0, 0.0, 0.0);
	float3 worldPos = float3(0.0, 0.0, 0.0);

#if GS_INSTANCED_CUBEMAP
	if (bUseInst)
	{
		worldPos = (float3)mul(IN.pos, gInstWorld);
		camPos = gInstViewInverse[IN.InstID][3].xyz;

		OUT.pos = mul(IN.pos, mul(gInstWorld,gInstWorldViewProj[IN.InstID]));
	}
	else
#endif
	{
		OUT.pos = mul(IN.pos, gWorldViewProj);
		worldPos = (float3)mul(IN.pos, gWorld);
		camPos = gViewInverse[3].xyz;
	}

	OUT.eyeToWorld.xyz = float3(worldPos.xy, horizonLevel) - camPos.xyz;
	OUT.eyeToWorld.w = saturate((camPos.z - skyPlaneFogFadeStart) * skyPlaneFogFadeOneOverDiff);

	OUT.color = (skyPlaneColor.rgb * skyPlaneColor.rgb) * skyPlaneColor.a;
	return OUT;
}

VS_SKY_PLANE_OUTPUT vs_sky_plane(VS_SKY_PLANE_INPUT IN )
{
	return vs_sky_plane_common(IN
#if GS_INSTANCED_CUBEMAP
		,false
#endif
		);
}
// ------------------------------------------------------------------------------------------------------------------//

OutHalf4Color ps_sky_plane(VS_SKY_PLANE_OUTPUT IN)
{
	float4 fogData = CalcFogData(IN.eyeToWorld.xyz);
	float3 finalColour = lerp(IN.color.rgb, fogData.rgb, fogData.a * IN.eyeToWorld.w);
	return CastOutHalf4Color(PackHdr(float4(finalColour, 1.0)));
}

// ------------------------------------------------------------------------------------------------------------------//

OutHalf4Color ps_sky_plane_cubemap(VS_SKY_PLANE_OUTPUT IN)
{
	float4 fogData = CalcFogData(IN.eyeToWorld.xyz);
	float3 finalColour = lerp(IN.color.rgb, fogData.rgb, fogData.a * IN.eyeToWorld.w);
	return CastOutHalf4Color(PackReflection(float4(finalColour, 1.0)));
}

// ------------------------------------------------------------------------------------------------------------------//

#if GS_INSTANCED_CUBEMAP
GEN_GSINST_TYPE(SkyPlane,VS_SKY_PLANE_OUTPUT,SV_RenderTargetArrayIndex)
GEN_GSINST_FUNC(SkyPlane,SkyPlane,VS_SKY_PLANE_INPUT,VS_SKY_PLANE_OUTPUT,VS_SKY_PLANE_OUTPUT,vs_sky_plane_common(IN,true),ps_sky_plane(IN))
#endif

technique sky_plane
{
	pass p0_normal
	{
		VertexShader = compile VERTEXSHADER vs_sky_plane();
		PixelShader = compile PIXELSHADER ps_sky_plane() OPTIMIZE;
	}

	#if GS_INSTANCED_CUBEMAP
		GEN_GSINST_TECHPASS(SkyPlane,cubeinst)
	#else
		pass p2_cubemap
		{
			VertexShader = compile VERTEXSHADER vs_sky_plane();
			PixelShader = compile PIXELSHADER ps_sky_plane_cubemap() OPTIMIZE;
		}
	#endif
}
// ------------------------------------------------------------------------------------------------------------------//

technique sun_visibility
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER vs_sun_visibility();
		PixelShader = compile PIXELSHADER ps_sun_visibility() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
