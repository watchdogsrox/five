#ifndef PRAGMA_DCL
	#pragma dcl position color0 texcoord0 normal
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

#define SHADOW_CASTING				(0)
#define SHADOW_CASTING_TECHNIQUES	(0)
#define SHADOW_RECEIVING			(1)
#define SHADOW_RECEIVING_VS			(0)
#include "../Lighting/Shadows/cascadeshadows.fxh"
#include "../Debug/EntitySelect.fxh"

#include "water_river_common.fxh"
#define IS_RIVER 1
#include "water_common.fxh"
#include "water_river_wave.fxh"

struct	VS_INPUT
{ 
	float3	Position		: POSITION;
	float4	Color			: COLOR0;		//bumpiness, clarity
	float2	TexCoord		: TEXCOORD0;	//texcoord for flow map
	float3	Normal			: NORMAL;
#define VSBumpIntensity	Color.x
#define VSOpacity		Color.y
};

struct VS_OUTPUT
{
	DECLARE_POSITION(Position)
	float4	Params0			: TEXCOORD0;	//bumpiness in w channel
	float4	Params1			: TEXCOORD1;	//water fog clarity in z channel
	float4	Params2			: TEXCOORD2;
	float4	Params3			: TEXCOORD3;
	float4	Params4			: TEXCOORD4;
#ifdef NVSTEREO
	float4	ScreenPosStereo	: TEXCOORD5;
#endif
#define PSScreenPos			Params0.xy
#define PSWetTexCoord		Params0.zw
#define PSWorldPos			Params1.xyz
#define PSBumpIntensity		Params1.w
#define PSTexCoord			Params2.xy
#define PSFoamTexCoord		Params2.zw
#define PSNormal			Params3.xyz
#define PSScreenDepth		Params3.w
#define PSOpacity			Params4.x
#define PSFlowVector		Params4.zw
};

VS_OUTPUT	VS_RiverDepth(VS_INPUT IN)
{
	VS_OUTPUT	OUT;
	float3 worldPos		= IN.Position + gWorld[3].xyz;
	float depthBlend	= .01;
	float eFactor		= log(depthBlend);
	float depth			= eFactor/(-20)/(IN.VSOpacity*IN.VSOpacity)/ 2.71828183;

	float3 view			= worldPos - gViewInverse[3].xyz;
	float depthFactor	= dot(-gViewInverse[2].xyz, view);
	float dropHeight    = max(-depthFactor, 0); //this prevents verts behind cam from raising the depth height
	float3 offset		= view*depth/abs(depthFactor);
	float offsetLength	= length(offset);
	offset				= lerp(offset/offsetLength, offset, step(1,offsetLength));

	OUT.Position		= mul(float4(IN.Position + offset + float3(0, 0, -dropHeight), 1), gWorldViewProj);

	OUT.Params0	= 0;
	OUT.Params1	= 0;
	OUT.Params2	= 0;
	OUT.Params3 = 0;
	OUT.Params4 = 0;


	return OUT;
}

VS_OUTPUT	VS	(VS_INPUT IN) 
{ 
	VS_OUTPUT	OUT;
	OUT.Params4 = 0;

	OUT.Position			= mul(float4(IN.Position, 1), gWorldViewProj);

	float4 screenPos		= rageCalcScreenSpacePosition(OUT.Position);
	OUT.PSScreenPos			= screenPos.xy;
	OUT.PSScreenDepth		= screenPos.w;	

#ifdef NVSTEREO
	OUT.ScreenPosStereo		= rageCalcScreenSpacePosition(MonoToStereoClipSpace(OUT.Position));
#endif

	OUT.PSNormal			= normalize(IN.Normal);
	float3 worldPos			= IN.Position + gWorld[3].xyz;
	OUT.PSWorldPos			= worldPos;
	OUT.PSFoamTexCoord		= worldPos.xy*DynamicFoamScale;
	OUT.PSWetTexCoord		= (worldPos.xy - gWorldBaseVS.xy)/(DYNAMICGRIDELEMENTS*DYNAMICGRIDSIZE) + 0.5/DYNAMICGRIDELEMENTS;
	OUT.PSTexCoord			= IN.TexCoord;
	OUT.PSOpacity			= IN.VSOpacity;
	OUT.PSBumpIntensity		= IN.VSBumpIntensity;

#if	COMPUTE_FLOW_IN_VS
	OUT.PSFlowVector		= tex2Dlod(FlowSampler, float4(IN.TexCoord, 0, 0)).rg*2 - 1;
#endif //COMPUTE_FLOW_IN_VS

	return OUT;
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

float4	PSUnderwaterCommon(VS_OUTPUT IN, uniform bool UseParaboloidMap)
{
#if COMPUTE_FLOW_IN_VS
	float2 flowVector = IN.PSFlowVector;
#else
	float2 flowVector = tex2D(FlowSampler, IN.PSTexCoord).xy*2 - 1;	// flow vector
#endif //COMPUTE_FLOW_IN_VS

	float4 waterColor = UnderwaterCommon(
			UseParaboloidMap,								//UseParaboloidMap
			0,												//ReflectionBlend
			IN.PSNormal,									//Normal
			float3(1,0,0),									//Tangent
			float3(0,1,0),									//Binormal
			IN.PSWorldPos,									//WorldPos
			IN.PSScreenPos.xy/IN.PSScreenDepth,				//ScreenPos
			IN.PSScreenDepth,								//ScreenDepth
			IN.PSWorldPos.xy,								//TexCoord
			gDirectionalLight.xyz,							//SunDirection
			gWaterDirectionalColor.rgb,						//SunColor
			0,												//FoamMask
			flowVector,										//FlowVector
			IN.PSBumpIntensity,								//BumpIntensity
			0);												//NormalLerp

	return PackHdr(waterColor);
}

wfloat4	RiverCommon(VS_OUTPUT IN, uniform bool HighDetail, uniform bool UseFogPrepass, float reflectionBlend)
{
	wfloat3 fogColor = w3tex2D(FogSampler, IN.PSTexCoord);

	float foamScale			= FOAMSCALE;
	float foamWeight		= FOAMWEIGHT;
	wfloat foamIntensity	= w2tex2D(WetSampler, IN.PSWetTexCoord).r*lerp(0, foamScale, foamWeight);
	foamIntensity			= foamIntensity*saturate((2*DYNAMICGRIDELEMENTS - IN.PSScreenDepth)/(2*DYNAMICGRIDELEMENTS));
	wfloat foamMask			= w2tex2D(StaticFoamSampler, IN.PSFoamTexCoord).g*lerp(0, foamScale, 1 - foamWeight);

#if COMPUTE_FLOW_IN_VS
	wfloat2 flowVector = IN.PSFlowVector;
#else
	wfloat2 flowVector = w2tex2D(FlowSampler, IN.PSTexCoord).xy*2 - 1;	// flow vector
#endif

	float4 waterColor = WaterCommon(
			HighDetail,										//HighDetail
			UseFogPrepass,									//UseFogPrepass
			true,											//UseDynamicFoam
			reflectionBlend,								//ReflectionBlend
			IN.PSNormal,									//Normal
			wfloat3(1,0,0),									//Tangent
			wfloat3(0,1,0),									//Binormal
			IN.PSWorldPos,									//WorldPos
			IN.PSScreenPos.xy/IN.PSScreenDepth,				//ScreenPos
#ifdef NVSTEREO
			IN.ScreenPosStereo.xy/IN.ScreenPosStereo.w,		//ScreenPosRfa
#else
			IN.PSScreenPos.xy/IN.PSScreenDepth,				//ScreenPos
#endif
			IN.PSScreenDepth,								//ScreenDepth
			IN.PSWorldPos.xy,								//TexCoord
			gDirectionalLight.xyz,							//SunDirection
			wfloat4(fogColor, pow(IN.PSOpacity, 2)),		//FogColor
			foamMask,										//FoamMask
			foamIntensity,									//FoamIntensity
			flowVector,										//FlowVector
			IN.PSBumpIntensity,								//BumpIntensity
			0);												//NormalLerp

	return PackWaterColor(waterColor);
}

half4 PS					(VS_OUTPUT IN) : COLOR	{ return RiverCommon(IN, true,	true,	0); }
half4 PS_SinglePass			(VS_OUTPUT IN) : COLOR	{ return RiverCommon(IN, false,	false,	1); }
half4 PS_UnderwaterLow		(VS_OUTPUT IN) : COLOR  { return PSUnderwaterCommon(IN,	true); }
half4 PS_UnderwaterHigh		(VS_OUTPUT IN) : COLOR  { return PSUnderwaterCommon(IN,	false); }


#if FORWARD_TECHNIQUES
technique draw
{
	pass p0
	{
		alphablendenable	= false;
		VertexShader		= compile VERTEXSHADER	VS();
		PixelShader			= compile PIXELSHADER	PS()				CGC_FLAGS("-unroll all --O2 -fastmath");
	}
}

technique alt5_draw
{
	pass p0
	{
		//fillmode			= wireframe;
		alphablendenable	= false;
		VertexShader		= compile VERTEXSHADER	VS();
		PixelShader			= compile PIXELSHADER	PS_SinglePass()		CGC_FLAGS(CGC_DEFAULTFLAGS);
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
		PixelShader			= compile PIXELSHADER	PS_UnderwaterLow()	CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}

technique alt1_draw
{
	pass p0
	{
		alphablendenable	= false;
		cullmode			= ccw;
		VertexShader		= compile VERTEXSHADER	VS();
		PixelShader			= compile PIXELSHADER	PS_UnderwaterHigh()	CGC_FLAGS(CGC_DEFAULTFLAGS);
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

technique clouddepth_draw
{
	pass p0
	{
		zFunc				= FixedupLESS;
		StencilRef			= DEFERRED_MATERIAL_CLEAR;
		VertexShader		= compile VERTEXSHADER	VS_RiverDepth();
#if __XENON
		PixelShader			= NULL;
#else
		ColorWriteEnable	= 0;
		ColorWriteEnable1	= 0;
		ColorWriteEnable2	= 0;
		ColorWriteEnable3	= 0;
		PixelShader			= compile PIXELSHADER	PS_WaterDepth() CGC_FLAGS(CGC_DEFAULTFLAGS);
#endif //__XENON
	}
}

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
