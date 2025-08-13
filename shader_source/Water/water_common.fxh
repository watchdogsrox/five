#include "../../renderer/waterdefines.h"

#define WATER_SHADER_DEBUG_ (0) // this adds some shader instructions .. enable it locally when needed

#if !defined(__GTA_COMMON_FXH)
#define WATER_REFLECTION_CLIP_IN_PIXEL_SHADER (1 && __PS3) // keep in sync with both common.fxh and water_common.fxh
#define WATER_SHADER_DEBUG	(WATER_SHADER_DEBUG_ && __BANK)
#else
#define WATER_SHADER_DEBUG	(WATER_SHADER_DEBUG_ && !defined(SHADER_FINAL))


//Tunable default params
#define DYN_WATER_FADEDIST 10
#define DEFAULT_WATERCOLOR			8,22,21,28
#define DEFAULT_RIPPLEBUMPINESS		(0.356)
#define DEFAULT_RIPPLESCALE			(0.04)
#define DEFAULT_RIPPLESPEED			(0.0)
#define DEFAULT_SPECULARINTENSITY	(1.0)
#define DEFAULT_SPECULARFALLOFF		(1118.0)
#define DEFAULT_REFLECTIONINTENSITY	(.202)
#define DEFAULT_FOGPIERCEINTENSITY	(2)
#define DEFAULT_PARALLAXINTENSITY	(0.3)

#define FOAMWEIGHT					(0.65)
#define FOAMSCALE					(1.00)

//============================== Samplers ==================================
#if __SHADERMODEL >= 40
shared Texture2D<float>	HeightTexture	REGISTER(t0);
#endif //__SHADERMODEL > 40

//cloud texture sampler in fog prepass
BeginSharedSampler		(sampler, StaticBumpTexture, StaticBumpSampler, StaticBumpTexture, s2)
ContinueSharedSampler	(sampler, StaticBumpTexture, StaticBumpSampler, StaticBumpTexture, s2)
	AddressU	= WRAP;
	AddressV	= WRAP;
	MIPFILTER	= MIPLINEAR;
	MINFILTER	= HQLINEAR;
	MAGFILTER	= MAGLINEAR;
EndSharedSampler;

BeginSharedSampler		(sampler, StaticFoamTexture, StaticFoamSampler, StaticFoamTexture, s4)
ContinueSharedSampler	(sampler, StaticFoamTexture, StaticFoamSampler, StaticFoamTexture, s4)
	AddressU		= WRAP;
	AddressV		= WRAP;
	MIPFILTER		= MIPLINEAR;
	MINFILTER		= MINANISOTROPIC;
	MAGFILTER		= LINEAR;
	ANISOTROPIC_LEVEL(8)
EndSharedSampler;

BeginSharedSampler		(sampler, BlendTexture, BlendSampler, BlendTexture, s6)
ContinueSharedSampler	(sampler, BlendTexture, BlendSampler, BlendTexture, s6)
	AddressU	= CLAMP;
	AddressV	= CLAMP;
	MINFILTER	= LINEAR;
	MAGFILTER	= LINEAR;
EndSharedSampler;

BeginSharedSampler		(sampler, ReflectionTex2, PlanarReflectionSampler, ReflectionTex2, s7)
ContinueSharedSampler	(sampler, ReflectionTex2, PlanarReflectionSampler, ReflectionTex2, s7)
	AddressU      = CLAMP;
	AddressV      = CLAMP;
	MIPFILTER     = MIPLINEAR;
	MINFILTER	  = MINANISOTROPIC;
	MAGFILTER     = MAGLINEAR;
	ANISOTROPIC_LEVEL(8)
EndSharedSampler;

//Wet map used for shoreline shader, no actual wet effect has been implemented for it
BeginSharedSampler		(sampler, WetTexture, WetSampler, WetTexture, s9)
ContinueSharedSampler	(sampler, WetTexture, WetSampler, WetTexture, s9)
	AddressU  = WRAP;
	AddressV  = WRAP;
	MINFILTER = HQLINEAR;
	MAGFILTER = LINEAR;
EndSharedSampler;

//256x256 water bump simulation
//#if (RSG_PC || RSG_DURANGO || RSG_ORBIS) && REFLECTION_CUBEMAP_SAMPLNIG
//BeginSharedSampler		(sampler, WaterBumpTexture, WaterBumpSampler, WaterBumpTexture, s5)
//ContinueSharedSampler	(sampler, WaterBumpTexture, WaterBumpSampler, WaterBumpTexture, s5)
//#else
BeginSharedSampler		(sampler, WaterBumpTexture, WaterBumpSampler, WaterBumpTexture, s10)
ContinueSharedSampler	(sampler, WaterBumpTexture, WaterBumpSampler, WaterBumpTexture, s10)
//#endif
	AddressU		= WRAP;
	AddressV		= WRAP;
	MIPFILTER		= MIPLINEAR;
	MINFILTER		= MINANISOTROPIC;
	MAGFILTER		= MAGLINEAR;
	ANISOTROPIC_LEVEL(4)
EndSharedSampler;

//low res depth buffer for refraction sampling
BeginSharedSampler		(sampler, RefractionDepthTexture, RefractionDepthSampler, RefractionDepthTexture, s11)
ContinueSharedSampler	(sampler, RefractionDepthTexture, RefractionDepthSampler, RefractionDepthTexture, s11)
	AddressU		= CLAMP;
	AddressV		= CLAMP;
	MINFILTER		= POINT;
	MAGFILTER		= POINT;
EndSharedSampler;

//low res color buffer for refraction sampling on PS3/PC, full res color buffer on 360, smoothstep texture for cloud shadows
BeginSharedSampler		(sampler, RefractionTexture, RefractionSampler, RefractionTexture, s12)
ContinueSharedSampler	(sampler, RefractionTexture, RefractionSampler, RefractionTexture, s12)
	AddressU		= CLAMP;
	AddressV		= CLAMP;
	MINFILTER		= LINEAR;
	MAGFILTER		= LINEAR;
EndSharedSampler;

//offset texture for low lod ocean water to fix tiling issues, offset texture for shadow pass dither
BeginSharedSampler		(sampler, NoiseTexture, NoiseSampler, NoiseTexture, s5)
ContinueSharedSampler	(sampler, NoiseTexture, NoiseSampler, NoiseTexture, s5)
	AddressU		= WRAP;
	AddressV		= WRAP;
	MIPFILTER		= POINT;
	MINFILTER		= POINT;
	MAGFILTER		= POINT;
EndSharedSampler;

//special bump simp texture that has no rain ripples
BeginSharedSampler		(sampler, WaterBumpTexture2, WaterBumpSampler2, WaterBumpTexture2, s14)
ContinueSharedSampler	(sampler, WaterBumpTexture2, WaterBumpSampler2, WaterBumpTexture2, s14)
	AddressU		= WRAP;
	AddressV		= WRAP;
	MIPFILTER		= MIPLINEAR;
	MINFILTER		= MINANISOTROPIC;
	MAGFILTER		= MAGLINEAR;
	ANISOTROPIC_LEVEL(8)
EndSharedSampler;

//lighting buffer for water shadows/dynamic lights (to be implemented)
BeginSharedSampler		(sampler, LightingTexture, LightingSampler, LightingTexture, s15)
ContinueSharedSampler	(sampler, LightingTexture, LightingSampler, LightingTexture, s15)
	AddressU		= CLAMP;
	AddressV		= CLAMP;
	MINFILTER		= LINEAR;
	MAGFILTER		= LINEAR;
EndSharedSampler;

#define CloudSampler		StaticBumpSampler
#define SmoothStepSampler	RefractionSampler
//=======================================================================

#define WATER_USEHALF 1
#if WATER_USEHALF
#define wfloat		half
#define wfloat2		half2
#define wfloat3		half3
#define wfloat4		half4
#define wfloat3x3	half3x3
half	w1tex2D(sampler Sampler, half2 TexCoord){ return h1tex2D(Sampler, TexCoord); }
half2	w2tex2D(sampler Sampler, half2 TexCoord){ return h2tex2D(Sampler, TexCoord); }
half3	w3tex2D(sampler Sampler, half2 TexCoord){ return h3tex2D(Sampler, TexCoord); }
half4	w4tex2D(sampler Sampler, half2 TexCoord){ return h4tex2D(Sampler, TexCoord); }
half4	wCalcScreenSpacePosition(half4 Position)
{
	half4	ScreenPosition;

	ScreenPosition.x = Position.x * 0.5 + Position.w * 0.5;
	ScreenPosition.y = Position.w * 0.5 - Position.y * 0.5;
	ScreenPosition.z = Position.w;
	ScreenPosition.w = Position.w;

	return(ScreenPosition);
}
#else //WATER_USEHALF
#define wfloat		float
#define wfloat2		float2
#define wfloat3		float3
#define wfloat4		float4
#define wfloat3x3	float3x3
float	w1tex2D(sampler Sampler, float2 TexCoord){ return tex2D(Sampler, TexCoord).x; }
float2	w2tex2D(sampler Sampler, float2 TexCoord){ return tex2D(Sampler, TexCoord).xy; }
float3	w3tex2D(sampler Sampler, float2 TexCoord){ return tex2D(Sampler, TexCoord).xyz; }
float4	w4tex2D(sampler Sampler, float2 TexCoord){ return tex2D(Sampler, TexCoord).xyzw; }
#define	wCalcScreenSpacePosition rageCalcScreenSpacePosition
#endif

CBSHARED BeginConstantBufferPagedDX10(water_globals, b4 )
shared float2	gWorldBaseVS						: WorldBaseVS				REGISTER2(vs, c129);//worldBase, oldWorldBase
shared float4	gFlowParams							: RiverFlowParams			REGISTER2(ps, c176);//sets for default bump (timeA, timeB, opacityA, opacityB)
shared float4	gFlowParams2						: RiverFlowParams2			REGISTER2(ps, c177);//(timeC, RefractionIndex, opacityC, cloudShadowOffset)
shared float4	gWaterAmbientColor					: WaterAmbientColor			REGISTER(c178);//Ambient, DynamicFoamScale
shared float4	gWaterDirectionalColor				: WaterDirectionalColor		REGISTER2(ps, c179);//Directional, mask amount
shared float4	gScaledTime							: RiverScaledTime			REGISTER(c180);//gScaledTime, fogLightIntensity, -NearClip*FarClip, -FarClip/(FarClip-NearClip)
shared float4	gOceanParams0						: OceanParams0				REGISTER2(ps, c181);
shared float4	gOceanParams1						: OceanParams1				REGISTER2(ps, c182);
ROW_MAJOR shared float4x4 gReflectionWorldViewProj	: ReflectionMatrix			REGISTER2(ps, c183);
shared float4	gFogLight_Debugging					: RiverFogLight				REGISTER2(ps, c187);
ROW_MAJOR shared float4x4 gRefractionWorldViewProj	: RefractionMatrix			REGISTER2(ps, c188);
shared float4	gOceanParams2						: OceanParams2				REGISTER2(ps, c192); // Used by Replay to curb vertex bump-based ofsetting (see VSWaterCommon()), unused, unused, unused.
EndConstantBufferDX10(water_globals)

#if WATER_SHADER_DEBUG
#define DebugAlphaBias							(floor(gFogLight_Debugging.x)/256.0)
#define DebugRefractionOverride					(frac(gFogLight_Debugging.x)*2)
#define DebugReflectionOverride					gFogLight_Debugging.y
#define DebugReflectionOverrideParaboloidAmount	gFogLight_Debugging.z
#define DebugHighlightAmount					gFogLight_Debugging.w
#define DebugWaterFogDepthBias					gFogLight_Debugging.x
#define DebugWaterFogRefractBias				gFogLight_Debugging.y
#define DebugWaterFogColourBias					gFogLight_Debugging.z
#define DebugWaterFogShadowBias					gFogLight_Debugging.w
#define DebugDistortionScale					gFlowParams2.x
#define DebugReflectionBrightnessScale			gFlowParams2.z
#endif // WATER_SHADER_DEBUG

#define RefractionIndex				gFlowParams2.y
#define FogLightIntensity			gScaledTime.y
#define OceanRippleBumpiness		gOceanParams0.x
#define OceanRippleScale			gOceanParams0.y
#define ReflectionIntensity			gOceanParams1.x
#define OceanBumpIntensity			gOceanParams1.w
#define CloudShadowParams			float4(gFlowParams2.w, 0, 0, 0)
#define	DynamicFoamScale			gWaterAmbientColor.a
#define WaterDirectionalColor		gWaterDirectionalColor.rgb
#define WaterAmbientColor			gWaterAmbientColor.rgb
#define DitherScale					gOceanParams0.xy

#ifdef IS_OCEAN
#define RippleSpeed			DEFAULT_RIPPLESPEED
#define RippleBumpiness		gOceanParams0.x
#define RippleScale			gOceanParams0.y
#define SpecularIntensity	gOceanParams0.z
#define SpecularFalloff		gOceanParams0.w
#define FogPierceIntensity	gOceanParams1.y
#define ParallaxIntensity	gOceanParams1.z
#else //IS_OCEAN

BeginConstantBufferDX10(water_common_locals)
float RippleBumpiness
<
	string UIName = "RippleBumpiness";
	string UIWidget = "Numeric";
	float UIMin = 0.00;
	float UIMax = 25.00;
	float UIStep = 0.5;
> = DEFAULT_RIPPLEBUMPINESS;
float RippleSpeed : RippleSpeed
<
	string UIName = "RippleSpeed";
	string UIWidget = "Numeric";
	float UIMin = 0.00;
	float UIMax = 10.00;
	float UIStep = 0.01;
> = DEFAULT_RIPPLESPEED;
float RippleScale
<
	string UIName = "RippleUVScale";
	string UIWidget = "Numeric";
	float UIMin = 0.00;
	float UIMax = 100.00;
	float UIStep = 0.1;
> = DEFAULT_RIPPLESCALE;
float SpecularIntensity
<
	string UIName = "SpecularIntensity";
	float UIMin = 0.0;
	float UIMax = 50.0;
	float UIStep = 0.25;
> = DEFAULT_SPECULARINTENSITY;
float SpecularFalloff
<
	string UIName = "SpecularFalloff";
	float UIMin = 4.0;
	float UIMax = 2000.0;
	float UIStep = 10.0;
> = DEFAULT_SPECULARFALLOFF;
float ParallaxIntensity
<
	string UIName = "ParallaxIntensity";
	float UIMin = 0.0;
	float UIMax = 5.0;
	float UIStep = 0.05;
> = DEFAULT_PARALLAXINTENSITY;

EndConstantBufferDX10(water_common_locals)

#define FogPierceIntensity (0.0)

#endif //IS_OCEAN

#ifdef IS_RIVEROCEAN
#define BlendRippleBumpiness		gOceanParams0.x
#define BlendRippleScale			gOceanParams0.y
#define BlendSpecularIntensity		gOceanParams0.z
#define BlendSpecularFalloff		gOceanParams0.w
#define BlendReflectionIntensity	gOceanParams1.x
#define BlendFogPierceIntensity		gOceanParams1.y
#define BlendParallaxIntensity		gOceanParams1.z
#else //IS_RIVEROCEAN
#define BlendRippleBumpiness		(0.0)
#define BlendRippleScale			(0.0)
#define BlendSpecularIntensity		(0.0)
#define BlendSpecularFalloff		(0.0)
#define BlendReflectionIntensity	(0.0)
#define BlendFogPierceIntensity		(0.0)
#define BlendParallaxIntensity		(0.0)
#endif //IS_RIVERSHALLOW

#if __PS3
#define BumpChannel2 ba
#else
#define BumpChannel2 rg
#endif

struct vertexOutputWaterHeight
{
	DECLARE_POSITION(pos)
};

vertexOutputWaterHeight VS_WaterHeight(float3 position : POSITION)
{
	vertexOutputWaterHeight OUT;
	OUT.pos = mul(float4(position, 1), gWorldViewProj);
	return OUT;
}

OutHalf4Color PS_WaterHeight() 
{
	return CastOutHalf4Color(0);
}

wfloat4 PackWaterColor(wfloat4 color)
{
#if __PS3
	return color;
#else
	return PackHdr(color);
#endif //__PS3
}

wfloat3 UnpackReflectionColor(wfloat3 color)
{
#if __XENON
	return color*gHdrScalars.y;
#elif __PS3
	return color*gToneMapScalers.y;
#else
	return color;
#endif //__PS3
}


wfloat GetSpecularIntensity(wfloat blendFactor)
{
	return lerp(SpecularIntensity, BlendSpecularIntensity, blendFactor);
}

float GetSpecularFalloff(float blendFactor)
{
	return lerp(SpecularFalloff, BlendSpecularFalloff, blendFactor);
}

wfloat GetFogPierceIntensity(wfloat blendFactor)
{
	return lerp(FogPierceIntensity, BlendFogPierceIntensity, blendFactor);
}

wfloat GetParallaxIntensity(wfloat blendFactor)
{
	return 0;
	//return lerp(ParallaxIntensity, BlendParallaxIntensity, blendFactor);
}

struct uvSet
{
	float2 uv1;
	float2 uv2;
};
float2 CalcFlatNorm2Tap(
	uvSet UV,
	sampler2D texMap
	)
{
	float2 norm0 = tex2D(texMap, UV.uv1).ag;
	float2 norm1 = tex2D(texMap, UV.uv2).ag;
	return 2*(norm0 + norm1)-2;
	//return 2*norm0-1;
}

void GenUVs2(
	float2 uvBase,
	float uvScale,
	float uvOffset,
	float2 temporalOffset,
	float2 flow,
	out uvSet UV
	)
{	
	// First set of UVs
	UV.uv1 = uvBase - (flow*uvOffset);
	UV.uv1 *= uvScale;

	// Second set of UVs
	UV.uv2 = uvBase - (flow*uvOffset)*1.1;
	UV.uv2 *= uvScale * 4.0f ;
}

#if __PS3
float4 FindGridFromWorld(float4 WorldXYXY){ return fmod(((WorldXYXY/DYNAMICGRIDSIZE) + DYNAMICGRIDELEMENTS*1000),DYNAMICGRIDELEMENTS); }
#endif //__PS3


struct PS_OUTPUT
{
	half4 color0 :	SV_Target0;
	half4 color1 :	SV_Target1;
	half4 color2 :	SV_Target2;
	half4 color3 :	SV_Target3;
};

PS_OUTPUT	PS_WaterDepth()
{
	PS_OUTPUT OUT;
	OUT.color0 = 0;
	OUT.color1 = 0;
	OUT.color2 = 0;
	OUT.color3 = 0;
	return OUT;
}

struct PS_OUTPUTWATERVSRT
{	
	float4 Color0 : SV_Target0;//Bump, alpha, fog alpha
	float4 Color1 : SV_Target1;//Fog
	float4 Color2 : SV_Target2;//Height
};

float SSAODecodeDepth( float3 vCodedDepth )
{
	float fDepth;
	fDepth = dot( vCodedDepth, float3(255.0f, 1.0f, 1.0f/255.0f) );
	return fDepth;
}

float GetSSAODepthHalf(sampler depthSampler, float2 tex)
{
#if __XENON || __SHADERMODEL < 40
	return _Tex2DOffset(depthSampler, tex, 0.5).x;
#else
	return tex2D(depthSampler, tex).x;
#endif //__XENON || __WIN32PC
}

wfloat CalcCascadeShadowsWater(float3 eyePos, float3 worldPos, float2 screenPos, float depth, uniform bool clouds)
{
#if __PS3 || __WIN32PC
#define WATER_SHADOWQUALITY CSM_ST_LINEAR
#else
#define WATER_SHADOWQUALITY CSM_ST_POINT
#endif

	CascadeShadowsParams params = CascadeShadowsParams_setup(WATER_SHADOWQUALITY);
	float shadow = CalcCascadeShadows_internal(params, depth, eyePos, worldPos, 0, screenPos, true, true, false, true, true);

	if(clouds)
	{
		wfloat cloudShadow	= CalcCloudShadowsCustom(CloudSampler, SmoothStepSampler, worldPos, CloudShadowParams);
		shadow				= shadow*cloudShadow;
	}

	return shadow;
}

wfloat3 GetSpecularColor(float3 L, float3 V, float3 N, wfloat3 lightCol, wfloat blendFactor, wfloat shadow)
{
	float3 halfAngle = normalize(L + V);
	wfloat specularIntensity = GetSpecularIntensity(blendFactor);
	specularIntensity = specularIntensity * wfloat(pow(saturate(dot(-halfAngle, N)), GetSpecularFalloff(blendFactor))) * shadow;
	return lightCol * specularIntensity;
}

wfloat2 SampleBump(float2 tex)
{
	return w2tex2D(WaterBumpSampler, tex);
}

float2 GetBump(float2 tex)
{
	return 2*(SampleBump(tex).rg + SampleBump(tex*4).rg)-2;
}

float3 GetFountainBump(float2 WorldPos)
{
	return float3((2*SampleBump(WorldPos*RippleScale)-1), RippleBumpiness);
}

float3 GetShallowBump(float2 TexCoord, float2 FlowVector)
{
	uvSet UV1;
	float4 Norm1, Norm2;

	float flowSpeed	= length(FlowVector);
	float4 texCoord	= (TexCoord.xyxy + RippleSpeed*flowSpeed*float4(1, 0, 1, 0)*gFlowParams.xxyy);
	texCoord.zw		+= .5;

	float4 bump;
	bump			= float4(SampleBump(texCoord.xy*4), SampleBump(texCoord.zw*4));
	bump			= bump + float4(SampleBump(texCoord.xy), SampleBump(texCoord.zw));
	bump			= 2*bump - 2;

	bump			*= gFlowParams.zzww;

	float2 flatNormal = bump.xy + bump.zw;
	return float3(flatNormal, RippleBumpiness);
}

float4 GetBump2X(float4 tex)
{
	float4 bump;
	bump.xy		= SampleBump(tex.xy).rg;
	bump.zw		= SampleBump(tex.zw).rg;
	tex			*=4;
	float4 bump2;
	bump2.xy	= SampleBump(tex.xy).rg;
	bump2.zw	= SampleBump(tex.zw).rg;
	bump		+= bump2;
	return 2*bump - 2;
}

wfloat3 GetRiverBump(float2 TexCoord, wfloat2 FlowVector, uniform bool HighDetail)
{
/*
	float4 grossTime;
	float4 scaledTime = gScaledTime.x*.75;

	scaledTime += float4(0, 1, 2, 3);

	const float numSamples = 2.0f;
	grossTime = fmod(scaledTime, numSamples);

	float4 offset	= numSamples/2.0f;
	float4 t		= grossTime - offset;
	grossTime		= scaledTime - grossTime;

	float4 tAbs		= t;
	tAbs			= abs(t);
	float4 c		= offset-tAbs;

	float4 flowParams	= float4(t.x, t.y, c.x, c.y);
	float4 flowParams2	= float4(t.z, t.w, c.z, c.w);

	float4 gFlowParams	= flowParams;
	float4 gFlowParams2	= flowParams2;
	float4 gGrossTime	= grossTime;
	gFlowParams.zw = normalize(gFlowParams.zw);
	gFlowParams2.zw = normalize(gFlowParams2.zw);
*/
	uvSet UV1;
	wfloat4 Norm1, Norm2;

	float4 texCoord	= (TexCoord.xyxy - FlowVector.xyxy*gFlowParams.xxyy)*RippleScale;
	texCoord.zw += .5;

	wfloat4 bump;
	if(HighDetail)
	{
		bump				= wfloat4(SampleBump(texCoord.xy*2.3), SampleBump(texCoord.zw*2.3));
		wfloat bumpScale	= 0;
#if __MAX
		if (dot(FlowVector,FlowVector)>0.01)
#endif	//__MAX
			bumpScale	= saturate(length(FlowVector));
		bump				= lerp(	bump + .5, 
									bump + float4(tex2D(WaterBumpSampler2, texCoord.xy*2.3).BumpChannel2, tex2D(WaterBumpSampler2, texCoord.zw*2.3).BumpChannel2), 
									bumpScale);
		bump				= 2*bump - 2;
	}
	else
	{
		bump.xy				= SampleBump(texCoord.xy);
		bump.zw				= SampleBump(texCoord.zw);
		bump				= 2*bump - 1;
	}

	bump			*= gFlowParams.zzww;

	wfloat2 flatNormal = bump.xy + bump.zw;
	return float3(flatNormal, RippleBumpiness);
}

wfloat3 GetOceanBump(float2 WorldPos, float ScreenDepth, uniform bool HighDetail)
{
	float2 tex = WorldPos*OceanRippleScale;

	wfloat2 noiseOffset	= w4tex2D(NoiseSampler, floor(tex)/256).ag*100;
	wfloat2 cellVal		= 2*abs(.5 - frac(tex));
	wfloat cellMask		= max(cellVal.x, cellVal.y);
	wfloat depthScale	= saturate(5-ScreenDepth/50);
	cellMask			= pow(cellMask, 32);
	//cellMask			*= depthScale;

	wfloat2 bump;

	if(HighDetail)
	{
		wfloat4 bumpHigh;
		bumpHigh.xy	= w4tex2D(WaterBumpSampler2, tex).BumpChannel2;
		bumpHigh.zw	= w2tex2D(WaterBumpSampler, tex*3.7).rg;
		bumpHigh	= 2*bumpHigh - 1;
		bump		= bumpHigh.xy + bumpHigh.zw;
	}
	else
	{
		wfloat4 bumpLow;
		bumpLow.xy		= w4tex2D(WaterBumpSampler2, tex + noiseOffset).BumpChannel2;
		bumpLow.zw		= w4tex2D(WaterBumpSampler2, tex).BumpChannel2;
		bumpLow			= 2*bumpLow - 1;
		//cellMask		= 0;
		bump			= lerp(bumpLow.xy, bumpLow.zw, cellMask);
	}

	wfloat2 lowBump		=	-(w4tex2D(StaticBumpSampler, WorldPos.yx/448 + gScaledTime.x/10).ga*2 - 1)
							-(w4tex2D(StaticBumpSampler, WorldPos.xy/512 - gScaledTime.x/10).ag*2 - 1);
	bump				+=	lowBump*OceanBumpIntensity;

	return float3(bump, OceanRippleBumpiness);
}

float2 SingleParaboloidTexCoord(float3 reflection, float scale, uniform float dir)
{
	float3 R = float3(-reflection.x, reflection.y, reflection.z);

	const float4 invertChannels = { 1.0f, 1.0f, -1.0f, -1.0f };
	float4 fbCoord = R.xyxy / (2.0f * (1.0f + R.zzzz * invertChannels)) * scale + 0.5f;

	fbCoord.yw = 1.0f - fbCoord.yw;

	fbCoord.xz *= 0.5f;
	fbCoord.x += 0.5f;

	return (dir > 0.0) ? fbCoord.xy : fbCoord.zw;
}

wfloat3 GetBumpCommon(float2 TexCoord, wfloat ScreenDepth, wfloat2 FlowVector, wfloat BlendFactor, uniform bool HighDetail)
{
	wfloat3 flatNormal;

#ifdef IS_OCEAN 
	flatNormal	= GetOceanBump(TexCoord, ScreenDepth, HighDetail);
#endif
#ifdef IS_RIVER
	flatNormal	= GetRiverBump(TexCoord, FlowVector, HighDetail);
#endif
#ifdef IS_SHALLOW
	flatNormal = GetShallowBump(TexCoord, FlowVector);
#endif
#ifdef IS_FOUNTAIN
	flatNormal	= GetFountainBump(TexCoord);
#endif
#ifdef IS_RIVEROCEAN
	flatNormal	= lerp(GetRiverBump(TexCoord, FlowVector, true), GetOceanBump(TexCoord, ScreenDepth, true), BlendFactor);
#endif
#ifdef IS_RIVERSHALLOW
	flatNormal	= lerp(GetShallowBump(TexCoord, FlowVector), GetOceanBump(TexCoord, ScreenDepth, HighDetail), BlendFactor);
#endif

	return flatNormal;
}

struct PS_OUTPUTWATERFOG
{
	half4 Color0 : SV_Target0;//Water Fog Color RGB, Shadow
	half4 Color1 : SV_Target1;//Water Refraction Lighting Depth Blend, Water Fog Depth Blend
};

PS_OUTPUTWATERFOG WaterFogCommon(wfloat2 TexCoord, wfloat2 ScreenPos, wfloat ScreenDepth, float3 WorldPos, float3 V, wfloat WaterOpacity)
{
	PS_OUTPUTWATERFOG OUT;

	float depth			= tex2D(RefractionDepthSampler, ScreenPos).r - ScreenDepth;

#if WATER_SHADER_DEBUG
	depth				+= DebugWaterFogDepthBias;
#endif // WATER_SHADER_DEBUG

	float depthScale	= 5;

	depth				= max(depth, 0);

	wfloat sampleN		= w1tex2D(NoiseSampler, ScreenPos*DitherScale);
	float2 scatterDirection;
	sincos(sampleN*3.14, scatterDirection.x, scatterDirection.y);

	wfloat3 offset		= (V + float3(scatterDirection*0.1, 0))*sampleN*depthScale;
	WorldPos			+= offset;

	wfloat shadow		= CalcCascadeShadowsWater(gViewInverse[3].xyz, WorldPos, ScreenPos, ScreenDepth, true);

#if WATER_SHADER_DEBUG
	shadow				+= DebugWaterFogShadowBias;
#endif // WATER_SHADER_DEBUG

	OUT.Color0			= wfloat4(WorldPos, shadow.x);
	OUT.Color1			= wfloat4(WaterOpacity, depth, 0, 0);
	return OUT;
}

wfloat4 UnderwaterCommon(
	uniform bool	UseParaboloidMap,
	wfloat			ReflectionBlend,
	wfloat3			Normal,
	wfloat3			Tangent,
	wfloat3			Binormal,
	float3			WorldPos,
	wfloat2			ScreenPos,
	wfloat			ScreenDepth,
	wfloat2			TexCoord,
	wfloat3			SunDirection,
	wfloat3			SunColor,
	wfloat			FoamOpacity,
	wfloat2			FlowVector,
	wfloat			BumpIntensity,
	wfloat			BumpLerp)
{
	FlowVector *= RippleSpeed;

	//Get Normal ==========================================================================
	wfloat3 flatNormal = GetBumpCommon(TexCoord, ScreenDepth, FlowVector, BumpLerp, true);

	wfloat3 V = normalize(WorldPos - gViewInverse[3].xyz);
	wfloat3 N = -wfloat3(Normal.xy + flatNormal.xy*flatNormal.z*BumpIntensity, Normal.z);
#if WATER_SHADER_DEBUG
	N = lerp(float3(0,0,-1), N, DebugDistortionScale);
#endif // WATER_SHADER_DEBUG
	N = normalize(N);
	//========================================================================================

	//Get Water Color ===========================================================================
	//Get Refraction Color
	wfloat3 refractionColor		= 0;
	wfloat depth				= w1tex2D(RefractionDepthSampler, ScreenPos) - ScreenDepth;
	wfloat3 refractionDirection	= refract(V, N, RefractionIndex);

	//Planar
	wfloat refractionBlend		= saturate(refractionDirection.z);
	wfloat4 refractionTex		= mul((wfloat4(WorldPos + (wfloat3(N.xy, 0)+V)*min(depth*refractionBlend, 5), 1.0)), gRefractionWorldViewProj);
	refractionTex				= rageCalcScreenSpacePosition(refractionTex);
	wfloat2 refractionPos1		= refractionTex.xy/refractionTex.w;
	refractionPos1				= lerp(ScreenPos, refractionPos1, refractionBlend);

	float2 refractionPos2       = refractionPos1*.9 + ScreenPos*.1;

	wfloat refractionDepth1		= w1tex2D(RefractionDepthSampler, refractionPos1);
	refractionDepth1			= max(refractionDepth1, 0);
	wfloat refractionDepth2		= w1tex2D(RefractionDepthSampler, refractionPos2);
	refractionDepth2			= max(refractionDepth2, 0);

	refractionPos1				= (refractionDepth1 > ScreenDepth)? refractionPos1 : ScreenPos.xy;
	refractionPos2				= (refractionDepth2 > ScreenDepth)? refractionPos2 : ScreenPos.xy;

	wfloat3 screenRefractionColor = 0;
	screenRefractionColor		+= w3tex2D(RefractionSampler, refractionPos1)*wfloat3(1,.5,0);
	screenRefractionColor		+= w3tex2D(RefractionSampler, refractionPos2)*wfloat3(0,.5,1);
	screenRefractionColor		= screenRefractionColor;

	//Paraboloid
	wfloat3 paraboloidRefractionColor;
	paraboloidRefractionColor	= w3tex2D(ReflectionSampler, DualParaboloidTexCoord_2h(refractionDirection, 1.0f));

	//Select
	refractionColor				= UseParaboloidMap? paraboloidRefractionColor : screenRefractionColor;
	//===============================================================================================

	//Get Reflection Color ===========================================================================
	wfloat3 reflectionNormal	= lerp(N, wfloat3(0, 0,-1), 5.0/6);
	wfloat3 reflectionDirection	= reflect(V, reflectionNormal);
	wfloat3 reflectionColor		= 0;

	//Get Planar Reflection
	wfloat4 reflectionPos		= mul(wfloat4(reflectionDirection, 0.0), gReflectionWorldViewProj);
	reflectionPos				= rageCalcScreenSpacePosition(reflectionPos);
	reflectionPos				= reflectionPos/reflectionPos.w;
	reflectionColor				= w3tex2D(PlanarReflectionSampler, reflectionPos.xy);

	wfloat3 planarReflectionColor	= UnpackReflection_3h(w3tex2D(PlanarReflectionSampler, reflectionPos.xy));

	//Get Paraboloid Reflection
	wfloat3 paraboloidReflectionColor;
	paraboloidReflectionColor	= w3tex2D(ReflectionSampler, DualParaboloidTexCoord_2h(reflectionDirection, 1.0f));

	//Select
	wfloat reflectionBlend		= ReflectionBlend;
	if(UseParaboloidMap)
		reflectionBlend			= 1;
	else
		paraboloidReflectionColor	= UnpackReflection_3h(paraboloidReflectionColor);

	reflectionColor				= lerp(planarReflectionColor, paraboloidReflectionColor, reflectionBlend);
	//================================================================================================

	//Blend ======================================================================
	wfloat3 color				= lerp(reflectionColor, refractionColor, refractionBlend);
	if(UseParaboloidMap)
		color					= UnpackReflection_3h(color);
	//============================================================================

#if WATER_SHADER_DEBUG
	color = lerp(color, refractionColor, DebugRefractionOverride);
	color = lerp(color, lerp(reflectionColor, paraboloidReflectionColor, DebugReflectionOverrideParaboloidAmount), DebugReflectionOverride);	
#ifdef IS_OCEAN
	color = lerp(color, float3(0,0,16), DebugHighlightAmount);
#elif defined(IS_SHALLOW)
	color = lerp(color, float3(0,16,16), DebugHighlightAmount);
#else
	color = lerp(color, float3(0,16,0), DebugHighlightAmount);
#endif
#endif // WATER_SHADER_DEBUG

	//=================================================================================================
	return wfloat4(color, 0);
	//=================================================================================================
}

wfloat4 WaterCommon(
	uniform bool	HighDetail,
	uniform bool	UseFogPrepass,
	uniform bool	UseDynamicFoam,
	float			ReflectionBlend,
	wfloat3			Normal,
	wfloat3			Tangent,
	wfloat3			Binormal,
	float3			WorldPos,
	wfloat2			ScreenPos,
	wfloat2			ScreenPosRfa,
	wfloat			ScreenDepth,
	wfloat2			TexCoord,
	float3			SunDirection,
	wfloat4			WaterColor,
	wfloat			FoamMask,
	wfloat			FoamIntensity,
	wfloat2			FlowVector,
	wfloat			BumpIntensity,
	wfloat			BlendFactor)
{
	FlowVector *= RippleSpeed;

	//Get Normal ==========================================================================
	wfloat3 flatNormal = GetBumpCommon(TexCoord, ScreenDepth, FlowVector, BlendFactor, HighDetail);

	float3 V = normalize(WorldPos - gViewInverse[3].xyz);

	wfloat vertexFresnel = pow(1 + V.z, 4);

#ifdef IS_SHALLOW
	wfloat3 N = mul(float3(flatNormal.xy*flatNormal.z*BumpIntensity, 1), wfloat3x3(normalize(Tangent), normalize(Binormal), normalize(Normal)));
#else
	wfloat3 N = wfloat3(Normal.xy + flatNormal.xy*flatNormal.z*BumpIntensity, Normal.z);
#endif //IS_SHALLOW

	N = N - GetParallaxIntensity(BlendFactor) * V * vertexFresnel;
#if WATER_SHADER_DEBUG
	N = lerp(float3(0,0,1), N, DebugDistortionScale);
	WaterColor.w = saturate(WaterColor.w + DebugAlphaBias);
#endif // WATER_SHADER_DEBUG
	N = normalize(N);
	//========================================================================================

	wfloat3 color = 0;

	//Get Water Color ===========================================================================
	//Get Refraction Color
	float4 depths;		
	depths.x = tex2D(RefractionDepthSampler, ScreenPosRfa + float2(-1.0f, -1.0f) * gooScreenSize.xy).r;
	depths.y = tex2D(RefractionDepthSampler, ScreenPosRfa + float2( 1.0f, -1.0f) * gooScreenSize.xy).r;
	depths.z = tex2D(RefractionDepthSampler, ScreenPosRfa + float2(-1.0f,  1.0f) * gooScreenSize.xy).r;
	depths.w = tex2D(RefractionDepthSampler, ScreenPosRfa + float2( 1.0f,  1.0f) * gooScreenSize.xy).r;

	float depthMax = max(depths.x, max(depths.y, max(depths.z, depths.w)));
	float depth						= depthMax - ScreenDepth; //only used for single pass
	depth							= max(depth, 0);

	wfloat4 refractionColor;
	refractionColor.a               = tex2D(LightingSampler, ScreenPosRfa).g;

	wfloat shadow;

	if(UseFogPrepass)
	{
		if(HighDetail)
		{
			//work out refraction position
			wfloat2 refractionPos;
			wfloat4 refractionScreenPos = MonoToStereoClipSpace(mul(wfloat4(WorldPos + (wfloat3(-N.xy, 0)+V)*refractionColor.a, 1), gRefractionWorldViewProj));
			refractionScreenPos         = wCalcScreenSpacePosition(refractionScreenPos);
			refractionPos               = refractionScreenPos.xy/refractionScreenPos.w;

#if WATER_SHADER_DEBUG
			refractionPos               = lerp(ScreenPos, refractionPos, DebugDistortionScale);
#endif // WATER_SHADER_DEBUG

			float3 paramsSample         = tex2D(LightingSampler,   refractionPos).yzw;
			wfloat cannotRefract        = paramsSample.y;
			shadow                      = paramsSample.z;

			float3 refPosDepth          = cannotRefract? float3(ScreenPosRfa, refractionColor.a) : float3(refractionPos, paramsSample.x);
			refractionColor.rgb         = tex2D(RefractionSampler, refPosDepth.xy);
			refractionColor.a           = refPosDepth.z;
		}
		else
		{
			refractionColor.rgb         = tex2D(RefractionSampler, ScreenPosRfa);
			shadow                      = w4tex2D(LightingSampler, ScreenPosRfa).a;
		}
		
		refractionColor.rgb	= refractionColor.rgb;
	}
	else //Cannot be high detail anymore
	{
#if __XENON
		refractionColor.rgb = PointSample7e3(RefractionSampler, ScreenPos, true).rgb;
#else
		refractionColor.rgb = w3tex2D(RefractionSampler, ScreenPosRfa).rgb;
#endif //__XENON

		refractionColor.rgb = UnpackHdr_3h(refractionColor.rgb);
		
		shadow              = CalcCascadeShadowsWater(gViewInverse[3].xyz, WorldPos, ScreenPos, ScreenDepth, false);

		refractionColor.a   = saturate(depth);
	}

	wfloat3 fogLight	= WaterAmbientColor - SunDirection.z*WaterDirectionalColor*shadow;

	//Get Water Fog Color
	wfloat3 waterColor	= WaterColor.rgb * WaterColor.rgb * fogLight * FogLightIntensity;
	wfloat3 nWaterColor	= WaterColor.rgb;
	wfloat pierceScale	= pow(saturate(dot(refract(V, lerp(N, Normal, .5), 1/1.5), -SunDirection)), 2)*saturate(depth/10);
	pierceScale			*= GetFogPierceIntensity(BlendFactor) * pierceScale;

#if defined(IS_OCEAN) || defined(IS_RIVEROCEAN)
	waterColor			+=  nWaterColor*WaterDirectionalColor*pierceScale;
#endif

	wfloat2 depthBlend			= saturate(exp(float2(-20, -60*abs(V.z)) * WaterColor.a * depth * 2.71828183));
	wfloat3 litRefractionColor	= lerp(2*nWaterColor.rgb*refractionColor.rgb, refractionColor.rgb, depthBlend.y);
	waterColor = lerp(waterColor, litRefractionColor, depthBlend.x);
	
	//This overrides all the above calculations
	if(UseFogPrepass)
		waterColor				= refractionColor.rgb;

#if RSG_PC && (__SHADERMODEL >= 40)
	if(UseDynamicFoam)
#else
	if(UseDynamicFoam && HighDetail)
#endif
	{
		wfloat foam				= FoamMask*(length(flatNormal.xy)*0.27 + 0.44);
		foam					= h2tex1D(BlendSampler, foam + FoamIntensity).g*refractionColor.a;
		waterColor				= waterColor +
								 (WaterDirectionalColor*saturate(dot(N, -SunDirection)*.7 + .3)*shadow + WaterAmbientColor)*foam;
	}

	color += waterColor;
	//===============================================================================================

	//Get Reflection Color ===========================================================================
	wfloat3 reflectionColor;

	wfloat3 reflectionNormal	= lerp(N, wfloat3(0,0,1), 5.0/6);
	//wfloat fresnel				= dot(-V, N); This is hack to counter the fact that about 30% the normals face away from the camera
	wfloat fresnel				= lerp(dot(-V, N), 1, .3);


	wfloat3 reflectionDirection	= reflect(V, reflectionNormal);
	reflectionDirection.z		= lerp(-V.z + abs(-V.z - reflectionDirection.z), reflectionDirection.z, ReflectionBlend);
	//reflectionDirection.z		= lerp(max(-2*V.z - reflectionDirection.z, reflectionDirection.z), reflectionDirection.z, ReflectionBlend);

	wfloat3 planarReflectionColor;
	wfloat4 reflectionPos		= mul(wfloat4(reflectionDirection, 0.0), gReflectionWorldViewProj);
	reflectionPos				= rageCalcScreenSpacePosition(reflectionPos);
	reflectionPos				/= reflectionPos.w;

	planarReflectionColor		= w3tex2D(PlanarReflectionSampler, reflectionPos.xy);
	planarReflectionColor		= UnpackReflection_3h(planarReflectionColor);
	
	wfloat3 paraboloidReflectionColor;
	paraboloidReflectionColor	= h3tex2D(ReflectionSampler, DualParaboloidTexCoord_2h(reflectionDirection, 1.0f)).rgb;
	paraboloidReflectionColor	= UnpackReflection_3h(paraboloidReflectionColor);

#if WATER_SHADER_DEBUG
	paraboloidReflectionColor	*= DebugReflectionBrightnessScale;
	planarReflectionColor		*= DebugReflectionBrightnessScale;
#endif // WATER_SHADER_DEBUG

	reflectionColor				= lerp(planarReflectionColor, paraboloidReflectionColor, ReflectionBlend);

	wfloat reflectionFactor		= h1tex1D(BlendSampler, fresnel);

	color						= lerp(color, reflectionColor, reflectionFactor);

	color						+= GetSpecularColor(SunDirection, V, N, WaterDirectionalColor, BlendFactor, shadow);

	color						= lerp(waterColor, color, refractionColor.a);
	//================================================================================================

	//=================================================================================================

#if WATER_SHADER_DEBUG
	color = lerp(color, refractionColor, DebugRefractionOverride);
	color = lerp(color, UnpackReflectionColor(lerp(planarReflectionColor, paraboloidReflectionColor, DebugReflectionOverrideParaboloidAmount)), DebugReflectionOverride);
#ifdef IS_OCEAN
	color = lerp(color, float3(0,0,16), DebugHighlightAmount);
#elif defined(IS_SHALLOW)
	color = lerp(color, float3(0,16,16), DebugHighlightAmount);
#else
	color = lerp(color, float3(0,16,0), DebugHighlightAmount);
#endif
#endif // WATER_SHADER_DEBUG

#if __WIN32PC && !defined(SHADER_FINAL)
	color = lerp(color, float3(0,0,64), gWaterDirectionalColor.w);
#endif // __WIN32PC && !defined(SHADER_FINAL)

	return wfloat4(color, 0);
	//=================================================================================================
}

#endif //!defined(__GTA_COMMON_FXH)
