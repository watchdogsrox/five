#ifndef PRAGMA_DCL
	#pragma dcl position texcoord0
	#define PRAGMA_DCL
#endif

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

#define SHADOW_CASTING				(0)
#define SHADOW_CASTING_TECHNIQUES	(0)
#define SHADOW_RECEIVING			(1)
#define SHADOW_RECEIVING_VS			(0)
#include "../Lighting/Shadows/cascadeshadows.fxh"
#include "../Debug/EntitySelect.fxh"

#define IS_FOUNTAIN (1)
#include "water_river_common.fxh"
#include "water_common.fxh"
#include "../../../rage/base/src/shaderlib/rage_calc_noise.fxh"
#include "water_river_wave.fxh"


struct	VS_INPUT
{ 
	float4	Position	:	POSITION;
	float4	TexCoord0	:	TEXCOORD0;
};
struct VS_OUTPUT
{
	float4	WorldPos		:	TEXCOORD0;//bumpiness in w channel
	float4	ScreenPos		:	TEXCOORD1;
#ifdef NVSTEREO
	float4	ScreenPosStereo	:	TEXCOORD2;
#endif
	DECLARE_POSITION_CLIPPLANES(Position)
};

#define PSScreenPos			ScreenPos.xyz
#define PSScreenDepth		ScreenPos.z
#define PSOpacity			1
#define PSWorldPos			WorldPos.xyz
#define PSBumpIntensity		1
#define PSTexCoord			0
#define PSNormal			float3(0,0,1)
#define PSFlowVector		0

// assigning specific slot due to nvidia 3D vision bug
BEGIN_RAGE_CONSTANT_BUFFER(water_fountain_locals,b5)
float4 FogColor
<
	string UIName = "FogColor";
	string UIWidget = "Numeric";
	float UIMin = 0.00;
	float UIMax = 1.00;
	float UIStep = 0.1;
> = float4(0.416, 0.6, 0.631, 0.055);
EndConstantBufferDX10( water_fountain_locals )

VS_OUTPUT	VS	(VS_INPUT IN) 
{ 
	VS_OUTPUT	OUT;

	OUT.WorldPos		= mul(IN.Position, gWorld);
	OUT.Position		= mul(IN.Position, gWorldViewProj);

	OUT.ScreenPos		= rageCalcScreenSpacePosition(OUT.Position);
#ifdef NVSTEREO
	OUT.ScreenPosStereo = rageCalcScreenSpacePosition(MonoToStereoClipSpace(OUT.Position));
#endif

	RAGE_COMPUTE_CLIP_DISTANCES(OUT.Position);

	return(OUT);
}

PS_OUTPUTWATERFOG PS_WaterFog(VS_OUTPUT IN)
{
	float4 fogColor			= FogColor*FogColor;
	PS_OUTPUTWATERFOG OUT	= WaterFogCommon(	IN.PSWorldPos.xy,
#ifdef NVSTEREO
												IN.ScreenPosStereo.xy/IN.ScreenPosStereo.w,
#else
												IN.PSScreenPos/IN.PSScreenDepth,
#endif
		IN.PSScreenDepth,
		IN.PSWorldPos,
		(IN.PSWorldPos - gViewInverse[3].xyz)/IN.PSScreenDepth,
		sqrt(fogColor.a));

	OUT.Color0.xyz	= fogColor.rgb;
	return OUT;
}

float4	PoolEnvUnderwaterCommon(VS_OUTPUT IN, uniform bool UseParaboloidMap)
{
	float4 waterColor = UnderwaterCommon(
			UseParaboloidMap,								//UseParaboloidMap
			0,												//ReflectionBlend
			float3(0,0,1),									//Normal
			float3(1,0,0),									//Tangent
			float3(0,1,0),									//Binormal
			IN.WorldPos.xyz,								//WorldPos
			IN.ScreenPos.xy /= IN.ScreenPos.w,				//ScreenPos
			IN.ScreenPos.w,									//ScreenDepth
			IN.WorldPos.xy,									//TexCoord
			gDirectionalLight.xyz,							//SunDirection
			gWaterDirectionalColor.rgb,						//SunColor
			0,												//FoamMask
			0,												//FlowVector
			1,												//BumpIntensity
			0);												//NormalLerp

	return PackHdr(waterColor);
}

float4	PoolEnvCommon(VS_OUTPUT IN, uniform bool HighDetail, uniform bool UseFogPrepass, float reflectionBlend)
{
	float4 waterColor = WaterCommon(
			HighDetail,											//HighDetail
			UseFogPrepass,										//UseFogPrepass
			false,												//UseDynamicFoam
			reflectionBlend,									//ReflectionBlend
			float3(0,0,1),										//Normal
			float3(1,0,0),										//Tangent
			float3(0,1,0),										//Binormal
			IN.WorldPos.xyz,									//WorldPos
			IN.ScreenPos.xy/IN.ScreenPos.w,						//ScreenPos
#ifdef NVSTEREO
			IN.ScreenPosStereo.xy/IN.ScreenPosStereo.w,			//ScreenPosRfa
#else
			IN.ScreenPos.xy/IN.ScreenPos.w,						//ScreenPosRfa
#endif
			IN.ScreenPos.z,										//ScreenDepth
			IN.WorldPos.xy,										//TexCoord
			gDirectionalLight,									//SunDirection
			FogColor*FogColor,									//FogColor
			0,													//FoamMask
			0,													//FoamIntensity
			0,													//FlowVector
			1,													//BumpIntensity
			0);													//NormalLerp

	return PackWaterColor(waterColor);
}

half4 PS_PoolEnv			(VS_OUTPUT IN) : COLOR { return PoolEnvCommon(IN, true, true, 1);	}
half4 PS_PoolEnvSinglePass	(VS_OUTPUT IN) : COLOR { return PoolEnvCommon(IN, false, false, 1); }
half4 PS_UnderwaterLow		(VS_OUTPUT IN) : COLOR { return PoolEnvUnderwaterCommon(IN, true);	}
half4 PS_UnderwaterHigh		(VS_OUTPUT IN) : COLOR { return PoolEnvUnderwaterCommon(IN, true);	}

#if FORWARD_TECHNIQUES
technique alt4_draw //env_draw
{
	pass p0
	{ 
		alphablendenable	= false;
		VertexShader		= compile VERTEXSHADER	VS();
		PixelShader			= compile PIXELSHADER	PS_PoolEnv()				CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}

technique alt6_draw //singlepassenv_draw
{
	pass p0
	{
		alphablendenable	= false;
		VertexShader		= compile VERTEXSHADER	VS();
		PixelShader			= compile PIXELSHADER	PS_PoolEnvSinglePass()		CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
#endif // FORWARD_TECHNIQUES

technique alt2_draw //underwaterenvlow_draw
{
	pass p0
	{
		alphablendenable	= false;
		cullmode			= ccw;
		VertexShader		= compile VERTEXSHADER	VS();
		PixelShader			= compile PIXELSHADER	PS_UnderwaterLow()			CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}

technique alt3_draw //underwaterenvhigh_draw
{
	pass p0
	{
		alphablendenable	= false;
		cullmode			= ccw;
		VertexShader		= compile VERTEXSHADER	VS();
		PixelShader			= compile PIXELSHADER	PS_UnderwaterHigh()			CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}

technique alt8_draw
{
	pass p0
	{
		VertexShader		= compile VERTEXSHADER	VS_WaterHeight();
	}
}

//NOT AN IMPOSTER TECHNIQUE, fog prepass, using this so I don't have to add another technique group
technique imposter_draw
{
	pass p0
	{
		alphablendenable	= false;
		VertexShader		= compile VERTEXSHADER	VS();
		PixelShader			= compile PIXELSHADER	PS_WaterFog()	CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}

#if __MAX
technique tool_draw
{
	pass p0
	{
		alphablendenable	= false;
		VertexShader		= compile VERTEXSHADER	VS();
		PixelShader			= compile PIXELSHADER	PS_PoolEnv()	CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
#else
SHADERTECHNIQUE_ENTITY_SELECT(VS(), VS())
#include "../Debug/debug_overlay_water.fxh"
#endif
