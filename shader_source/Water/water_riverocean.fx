
#ifndef PRAGMA_DCL
	#pragma dcl position color0 texcoord0
	#define PRAGMA_DCL
#endif
//#pragma strip off

#define SPECULAR 0
#define REFLECT 0

#define ALPHA_SHADER
#define NO_SKINNING
#include "../common.fxh"

#if SPECULAR
const float specularFalloffMult=1.0;
const float specularIntensityMult=1.0;
#endif

#include "../Lighting/lighting.fxh"

#pragma constant 130

#include "../../../rage/base/src/shaderlib/rage_xplatformtexturefetchmacros.fxh"

#define SHADOW_CASTING            (0)
#define SHADOW_CASTING_TECHNIQUES (0)
#define SHADOW_RECEIVING          (1)
#define SHADOW_PROCESSING         (0)
#include "../Lighting/Shadows/cascadeshadows.fxh"
#include "../Lighting/lighting.fxh"
#include "../Debug/EntitySelect.fxh"

#include "water_river_common.fxh"
#define IS_RIVEROCEAN 1
#include "water_common.fxh"
#include "../../../rage/base/src/shaderlib/rage_calc_noise.fxh"
#include "water_river_wave.fxh"

struct	VS_INPUT
{ 
	float3	Position	:	POSITION;
	float4  Color		:	COLOR0;//bumpiness, clarity, blend factor
	float2	TexCoord	:	TEXCOORD0;
#define VSBumpIntensity	Color.x
#define VSOpacity		Color.y
#define VSBlendFactor	Color.z
};

struct VS_OUTPUT
{
	DECLARE_POSITION(Position)
	float4	WorldPos		: TEXCOORD0;//bumpiness in w channel
	float4	ScreenPos		: TEXCOORD1;//water fog clarity in z channel
	float4	TexCoord		: TEXCOORD2;//TexCoord in xy channels, Ocean Fog coords in zw channels
	float4	Params			: TEXCOORD3;//blend factor in x channel
	float4	Params2			: TEXCOORD4;//
#ifdef NVSTEREO
	float4 ScreenPosStereo	:	TEXCOORD5;
#endif
#define PSScreenPos			ScreenPos.xy
#define PSScreenDepth		ScreenPos.z
#define PSWorldPos			WorldPos.xyz
#define PSBumpIntensity		WorldPos.w
#define PSOpacity			ScreenPos.w
#define PSBlendFactor		Params.x
#define PSOceanTexCoord		TexCoord.zw
#define PSTexCoord			TexCoord.xy
#define PSFoamTexCoord		Params2.xy
#define PSWetTexCoord		Params2.zw
};

VS_OUTPUT	VS	(VS_INPUT IN) 
{ 
	VS_OUTPUT	OUT;
	
	float3	worldPos	= IN.Position + gWorld[3].xyz;
	OUT.PSWorldPos		= worldPos;
	OUT.Position		= mul(float4(IN.Position, 1), gWorldViewProj);
#ifdef NVSTEREO
	OUT.ScreenPosStereo = rageCalcScreenSpacePosition(MonoToStereoClipSpace(OUT.Position));
#endif
	OUT.ScreenPos		= rageCalcScreenSpacePosition(OUT.Position);
	OUT.Params			= 0;
	OUT.PSFoamTexCoord	= worldPos.xy*DynamicFoamScale;
	OUT.PSWetTexCoord	= (worldPos.xy - gWorldBaseVS.xy)/(DYNAMICGRIDELEMENTS*DYNAMICGRIDSIZE) + 0.5/DYNAMICGRIDELEMENTS;
	OUT.PSBlendFactor	= IN.VSBlendFactor;
	OUT.PSOpacity		= IN.VSOpacity;
	OUT.PSBumpIntensity	= lerp(IN.VSBumpIntensity, 1, IN.VSBlendFactor);
	
	OUT.PSTexCoord		= IN.TexCoord;
	OUT.PSOceanTexCoord	= 0;

	return(OUT);
}

PS_OUTPUTWATERFOG PS_WaterFog(VS_OUTPUT IN)
{
	PS_OUTPUTWATERFOG OUT = WaterFogCommon(	IN.PSWorldPos.xy,
#ifdef NVSTEREO
												IN.ScreenPosStereo.xy/IN.ScreenPosStereo.w,
#else
												IN.PSScreenPos/IN.PSScreenDepth,
#endif
		IN.PSScreenDepth,
		IN.PSWorldPos,
		(IN.PSWorldPos - gViewInverse[3].xyz)/IN.PSScreenDepth,
		IN.PSOpacity);

	float2 tex		= IN.PSTexCoord;
	OUT.Color0.xyz	= tex2D(FogSampler, tex).rgb;
	return OUT;
}

float4	RiverOceanUnderwaterCommon(VS_OUTPUT IN, uniform bool UseParaboloidMap)
{
	float4 waterColor = UnderwaterCommon(
			UseParaboloidMap,								//UseParaboloidMap
			0,												//ReflectionBlend
			float3(0,0,1),									//Normal
			float3(1,0,0),									//Tangent
			float3(0,1,0),									//Binormal
			IN.PSWorldPos.xyz,								//WorldPos
			IN.PSScreenPos.xy/IN.PSScreenDepth,				//ScreenPos
			IN.PSScreenDepth,								//ScreenDepth
			IN.PSWorldPos.xy,								//TexCoord
			gDirectionalLight.xyz,							//SunDirection
			gWaterDirectionalColor.rgb,					//SunColor
			0,												//FoamOpacity,
			0,												//FlowVector
			IN.PSBumpIntensity,								//BumpIntensity
			IN.PSBlendFactor);								//Blend Factor

	return PackHdr(waterColor);
}

float4	RiverOceanCommon(VS_OUTPUT IN, uniform bool HighDetail, uniform bool UseFogPrepass, float reflectionBlend)
{
	float3 fogColor		= tex2D(FogSampler,	IN.PSTexCoord).rgb;
	float blendFactor	= smoothstep(0, 1, IN.PSBlendFactor);

	float foamScale			= FOAMSCALE;
	float foamWeight		= FOAMWEIGHT;
	wfloat foamIntensity	= w2tex2D(WetSampler, IN.PSWetTexCoord).r*lerp(0, foamScale, foamWeight);
	foamIntensity			= foamIntensity*saturate((2*DYNAMICGRIDELEMENTS - IN.PSScreenDepth)/(2*DYNAMICGRIDELEMENTS));
	wfloat foamMask			= w2tex2D(StaticFoamSampler, IN.PSFoamTexCoord).g*lerp(0, foamScale, 1 - foamWeight);

	float4 waterColor = WaterCommon(
			HighDetail,													//HighDetail
			UseFogPrepass,												//UseFogPrepass
			true,														//UseDynamicFoam
			reflectionBlend,											//ReflectionBlend
			float3(0,0,1),												//Normal
			float3(1,0,0),												//Tangent
			float3(0,1,0),												//Binormal
			IN.PSWorldPos.xyz,											//WorldPos
			IN.PSScreenPos.xy/IN.PSScreenDepth,							//ScreenPos
#ifdef NVSTEREO
			IN.ScreenPosStereo.xy/IN.ScreenPosStereo.w,	//ScreenPosRfa
#else
			IN.PSScreenPos/IN.PSScreenDepth,			//ScreenPosRfa
#endif
			IN.PSScreenDepth,											//ScreenDepth
			IN.PSWorldPos.xy,											//TexCoord
			gDirectionalLight,											//SunDirection
			float4(fogColor, pow(IN.PSOpacity, 2)),						//FogColor
			foamMask,													//FoamMask
			foamIntensity,												//FoamIntensity
			0,															//FlowVector
			IN.PSBumpIntensity,											//BumpIntensity
			blendFactor);												//BlendFactor

	return PackWaterColor(waterColor);
}
half4 PS						(VS_OUTPUT IN) : COLOR	{ return RiverOceanCommon(IN, true,  true,  0); }
half4 PS_RiverOceanSinglePass	(VS_OUTPUT IN) : COLOR	{ return RiverOceanCommon(IN, false, false, 1); }
half4 PS_UnderwaterLow			(VS_OUTPUT IN) : COLOR  { return RiverOceanUnderwaterCommon(IN, true); }
half4 PS_UnderwaterHigh			(VS_OUTPUT IN) : COLOR  { return RiverOceanUnderwaterCommon(IN, false); }

#if FORWARD_TECHNIQUES
technique draw
{
	pass p0
	{ 
		alphablendenable	= false;
		VertexShader		= compile VERTEXSHADER	VS();
		PixelShader			= compile PIXELSHADER	PS()						CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}

technique alt5_draw
{
	pass p0
	{
		alphablendenable	= false;
		VertexShader		= compile VERTEXSHADER	VS();
		PixelShader			= compile PIXELSHADER	PS_RiverOceanSinglePass()	CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
#endif // FORWARD_TECHNIQUES

technique alt0_draw
{
	pass p0
	{
		alphablendenable	= false;
		cullmode			= ccw;
		VertexShader		= compile VERTEXSHADER	VS();
		PixelShader			= compile PIXELSHADER	PS_UnderwaterLow()				CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}

technique alt1_draw
{
	pass p0
	{
		alphablendenable	= false;
		cullmode			= ccw;
		VertexShader		= compile VERTEXSHADER	VS();
		PixelShader			= compile PIXELSHADER	PS_UnderwaterHigh()		CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}

technique alt8_draw
{
	pass p0
	{
		VertexShader		= compile VERTEXSHADER	VS_WaterHeight();
	}
}

//NOT AN IMPOSTER TECHNIQUE, used for river fog pass. I'm using the imposter_draw name so I don't have to make another shadergroup
technique imposter_draw
{
	pass p0
	{
		alphablendenable	= false;
		VertexShader		= compile VERTEXSHADER	VS();
		PixelShader			= compile PIXELSHADER	PS_WaterFog()	CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}

#if __PS3
//NOT AN IMPOSTER TECHNIQUE, used for river fog pass. I'm using the imposter_draw name so I don't have to make another shadergroup
PS_OUTPUTWATERVSRT PS_VertexStreamParams(VS_OUTPUT IN)
{
	float4 fogSample	= tex2D(FogSampler, IN.PSTexCoord);

	PS_OUTPUTWATERVSRT OUT;
	OUT.Color0	= 0;
	OUT.Color1	= fogSample;
	OUT.Color2	= 0;
	return OUT;
}

technique imposterdeferred_draw
{
	pass p0
	{
		alphablendenable	= false;
		ColorWriteEnable	= 0;
		ColorWriteEnable1	= RED | GREEN | BLUE;
		ColorWriteEnable2	= 0;
		VertexShader		= compile VERTEXSHADER	VS();
		PixelShader			= compile PIXELSHADER	PS_VertexStreamParams()	CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
#endif

#if __MAX
technique tool_draw
{
	pass p0
	{
		alphablendenable	= false;
		VertexShader		= compile VERTEXSHADER	VS();
		PixelShader			= compile PIXELSHADER	PS() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
#else
SHADERTECHNIQUE_ENTITY_SELECT(VS(), VS())
#include "../Debug/debug_overlay_water.fxh"
#endif
