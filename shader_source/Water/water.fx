//#pragma strip off
#define NO_SKINNING
#define IS_OCEAN 1

#include "../common.fxh"
#include "../../../rage/base/src/shaderlib/rage_xplatformtexturefetchmacros.fxh"

#define SPECULAR 1
static const float specularFalloffMult = 0.0f;
static const float specularIntensityMult = 0.0f;

#include "../lighting/lighting.fxh"

#pragma constant 130

#include "../../../rage/base/src/shaderlib/rage_xplatformtexturefetchmacros.fxh"
#include "../Lighting/Shadows/cascadeshadows_common.fxh"
#define SHADOW_CASTING            (0)
#define SHADOW_CASTING_TECHNIQUES (0)
#define SHADOW_RECEIVING          (1)
#define SHADOW_PROCESSING         (0)
#include "../Lighting/Shadows/cascadeshadows.fxh"
#include "../Lighting/lighting.fxh"

#include "water_common.fxh"

#pragma sampler 3
BeginSampler	(	sampler, FogTexture, FogSampler, FogTexture)
string	UIName = "Fog Texture";
string 	TCPTemplate = "Diffuse";
string	TextureType="Fog";
ContinueSampler(	sampler, FogTexture, FogSampler, FogTexture)
AddressU	= CLAMP;
AddressV	= CLAMP;
MINFILTER	= LINEAR;
MAGFILTER	= LINEAR;
EndSampler;

#include "../Util/GSInst.fxh"

//------------------------------------
struct VSINPUT {
	float3 pos			: POSITION;
	float2 tex			: TEXCOORD0;
};

struct VSOUTPUT {
	DECLARE_POSITION(pos)
	float4 tex			: TEXCOORD0;
	float4 ScreenPos	: TEXCOORD1;
};

struct WATER_VSINPUT {
	float3 pos			: POSITION;
	float2 tex			: TEXCOORD0;
	float4 color		: COLOR0;
};

struct WATERTESS_VSINPUT
{
	int2 tex			: TEXCOORD0;
};

struct WATERVSRT_INPUT	//Input format for tesselation vertex stream render target input
{
	int2	pos			: TEXCOORD0;
	float	height		: TEXCOORD1;//vertex height (comes from R32F 128x128 RT)
	float4	params		: TEXCOORD2;//bump xy, opacity, quad mask (comes from 8888 128x128 RT)
#define VSRTBump	params.wz
#define VSRTOpacity	params.y
#define VSRTFoam	params.x
};

struct WATERLOW_VSINPUT
{
	int2	posXY		: TEXCOORD0;
	float	posZ		: TEXCOORD1;
#if RAGE_INSTANCED_TECH
	uint InstID : SV_InstanceID;
#endif
};

struct WATER_VSOUTPUT {
	DECLARE_POSITION(Position)
	float4 ScreenPos	: TEXCOORD0;
	float4 Normal		: TEXCOORD1;//w channel contains CPV alpha
	float4 WorldPos		: TEXCOORD2;
	float4 TexCoord		: TEXCOORD3;
#ifdef NVSTEREO
	float4 ScreenPosStereo	:	TEXCOORD4;
#endif
#define PSScreenPos			ScreenPos.xy
#define PSScreenDepth		WorldPos.w
#define PSOpacity			Normal.w
#define PSWorldPos			WorldPos.xyz
#define PSBumpIntensity		1
#define PSTexCoord			TexCoord.xy
#define PSFoamTexCoord		TexCoord.zw
#define PSWetTexCoord		ScreenPos.zw
#define PSNormal			Normal.xyz
#define PSFlowVector		0
};

struct VS_OUTPUTWATERLIGHTING
{
	DECLARE_POSITION(Position)
	float4 ScreenPos	: TEXCOORD0;
	float4 WorldPos		: TEXCOORD1;
	float4 View			: TEXCOORD2;
};
//------------------------------------

BEGIN_RAGE_CONSTANT_BUFFER(water_locals,b0)

float4	OceanLocalParams0;	//gOceanFoamIntensity, gOceanFoamScale, gWorldBase
float4	FogParams;	//gFogScale, gFogOffset, 
float4	QuadAlpha;	//a1, a2, a3, a4

#define gOceanFoamIntensity	OceanLocalParams0.x
#define gWorldBase			OceanLocalParams0.zw
#define gFogOffset			FogParams.xy
#define gFogScale			FogParams.zw
#define a1					QuadAlpha.x
#define a2					QuadAlpha.y
#define a3					QuadAlpha.z
#define a4					QuadAlpha.w

float3		CameraPosition;
EndConstantBufferDX10(water_locals)

VSOUTPUT VS_blit(VSINPUT IN)
{
	VSOUTPUT OUT;
	OUT.pos = float4(float3(IN.pos.xy, 0),1);
	OUT.tex = float4(IN.tex.xy, IN.pos.zz);
	OUT.ScreenPos = rageCalcScreenSpacePosition(OUT.pos);
	return OUT;
}

VSOUTPUT VS_UpdateDisturb(WATER_VSINPUT IN)
{
	VSOUTPUT OUT;
	OUT.pos = mul(float4(float3(IN.pos.xy, 0.0),1), gWorldViewProj);
	OUT.tex = float4(IN.tex.xy, IN.color.xy);
	OUT.ScreenPos = rageCalcScreenSpacePosition(OUT.pos);
	return OUT;
}

void VSWaterCommon(uniform bool UseHeightMap, float3 position, float3 normal, out WATER_VSOUTPUT OUTPUT, uniform bool UseStereo)
{
	OUTPUT.Position		= 0;
	OUTPUT.ScreenPos	= 0;
	OUTPUT.Normal		= 0;
	OUTPUT.WorldPos		= 0;
	OUTPUT.TexCoord		= 0;
#ifdef NVSTEREO
	OUTPUT.ScreenPosStereo = 0;
#endif

	float3 worldPos		= position + gWorld[3].xyz;

	float3 tPos		= position;
	float3 tNorm	= normal;

	float adder = 0;
	if(UseHeightMap)
	{
#if __SHADERMODEL >= 40
		float2 heightClamp	= DYNAMICGRIDELEMENTS*DYNAMICGRIDSIZE/2.0f - abs(gWorldBaseVS.xy+DYNAMICGRIDELEMENTS*DYNAMICGRIDSIZE/2.0f -worldPos.xy);
		heightClamp			= saturate(heightClamp/DYN_WATER_FADEDIST);
		float heightScale   = min(heightClamp.x, heightClamp.y);

		float2 heightTex	= (worldPos.xy - gWorldBaseVS.xy)/(DYNAMICGRIDELEMENTS*DYNAMICGRIDSIZE);
		int2 heightTexInt   = (int2)(heightTex*DYNAMICGRIDELEMENTS);

		float height		= HeightTexture.Load(int3(heightTexInt, 0)).r*heightScale;
		worldPos.z          += height;
		tPos.z              += height;

		float4 heights;
		heights.x			=  HeightTexture.Load(int3(heightTexInt + int2(-1,  0), 0)).r;
		heights.y			=  HeightTexture.Load(int3(heightTexInt + int2( 1,  0), 0)).r;
		heights.z			=  HeightTexture.Load(int3(heightTexInt + int2( 0, -1), 0)).r;
		heights.w			=  HeightTexture.Load(int3(heightTexInt + int2( 0,  1), 0)).r;

		heights				*= heightScale;
		heights				= height + heights;

		float2 bump=heights.xz - heights.yw;

		tNorm				= normalize(float3(bump, 1));
		//tNorm				= normalize(cross(float3(1, 0, -heights.x + heights.y), float3(0, 1, - heights.z + heights.w)));

		float2 tDir			= -2.0*tNorm.xy;
		tDir				= -normalize(tNorm.xy + 0.00001)*(sqrt(4*length(tNorm.xy+0.00001) + 1) - 1);

		// This turns on/off vertex perturbation. Water Height is quantized in Replay giving rise to jerky vertex offsets.
		tPos.xy				+= tDir*gOceanParams2.x;
		worldPos.xy			+= tDir*gOceanParams2.x;

		float f				= saturate(distance(worldPos.xyz, gViewInverse[3].xyz)/(DYNAMICGRIDELEMENTS*DYNAMICGRIDSIZE/2.0f));
		tNorm				= normalize(lerp(tNorm, float3(0,0,1), f*f));
#endif //__SHADERMODEL >= 40...
	}

	OUTPUT.TexCoord.xy		= (worldPos.xy - gFogOffset)*gFogScale;
	OUTPUT.TexCoord.y		= 1 - OUTPUT.TexCoord.y;

	float2 warpedPos		= position.xy + gWorld[3].xy;
	OUTPUT.PSFoamTexCoord	= warpedPos*DynamicFoamScale;
	OUTPUT.PSWetTexCoord	= (warpedPos - gWorldBaseVS.xy)/(DYNAMICGRIDELEMENTS*DYNAMICGRIDSIZE) + 0.5/DYNAMICGRIDELEMENTS;
	OUTPUT.PSWorldPos		= worldPos;
	OUTPUT.Normal.xyz		= tNorm;

#ifdef NVSTEREO
	if (UseStereo)
		OUTPUT.Position	= MonoToStereoClipSpace(mul(float4(tPos,1), gWorldViewProj));
	else
		OUTPUT.Position	= mul(float4(tPos,1), gWorldViewProj);
	
	if (UseStereo)
		OUTPUT.ScreenPosStereo = rageCalcScreenSpacePosition(OUTPUT.Position);
	else
		OUTPUT.ScreenPosStereo = rageCalcScreenSpacePosition(MonoToStereoClipSpace(OUTPUT.Position));
#else
	OUTPUT.Position		= mul(float4(tPos,1), gWorldViewProj);
#endif

	float4 screenPos	= rageCalcScreenSpacePosition(OUTPUT.Position);
	OUTPUT.PSScreenPos	= screenPos.xy;
	OUTPUT.PSScreenDepth= screenPos.w;
}

float GetTesselationAlpha(float2 tex)
{
	float2 range = DYNAMICGRIDELEMENTS*DYNAMICGRIDSIZE;
	float2 lerpVal = (tex + DYNAMICGRIDELEMENTS*DYNAMICGRIDSIZE/2)/range;
	//3 4
	//1 2
	float2 yAlpha = lerp(float2(a1, a3), float2(a2, a4), lerpVal.x);
	float alpha = lerp(yAlpha.x, yAlpha.y, lerpVal.y);
	return alpha;
}

WATER_VSOUTPUT VS_WaterTess(WATERTESS_VSINPUT IN)
{
	WATER_VSOUTPUT OUT;
	VSWaterCommon(true, float3(IN.tex, 0), float3(0, 0, 1), OUT,false);
	OUT.Normal.w = GetTesselationAlpha(IN.tex);
	return OUT;
}

#ifdef NVSTEREO
WATER_VSOUTPUT VS_WaterTessStereo(WATERTESS_VSINPUT IN)
{
	WATER_VSOUTPUT OUT;
	VSWaterCommon(true, float3(IN.tex, 0), float3(0, 0, 1), OUT,true);
	OUT.Normal.w = GetTesselationAlpha(IN.tex);
	return OUT;
}
#endif

WATER_VSOUTPUT VS_Water(WATER_VSINPUT IN)
{
	WATER_VSOUTPUT OUT;
	VSWaterCommon(false, IN.pos, float3(0, 0, 1), OUT,false);
	OUT.Normal.w = IN.color.a;
	return OUT;
}

WATER_VSOUTPUT VS_WaterFlat(WATER_VSINPUT IN)
{
	WATER_VSOUTPUT OUT;
	OUT = VS_Water(IN);
#if __PS3 //PS3 needs a slight depth bias for skydive view, use water fog to hide z-fighting
	OUT.Position.z = OUT.Position.z*0.999999;
#endif
	return OUT;
}

WATER_VSOUTPUT VS_WaterFLOD(WATER_VSINPUT IN)
{
	WATER_VSOUTPUT OUT;
	VSWaterCommon(false, IN.pos, float3(0, 0, 1), OUT,false);
#if __PS3
	OUT.Position.z = 0;//PS3 has terrible hardware clipping, thankfully it has depth clamp
#endif //__PS3
	return OUT;
}

struct WATERFLOD_VSOUTPUT
{
	DECLARE_POSITION(Position)
	float4 WorldPos : TEXCOORD0;
};

WATERFLOD_VSOUTPUT VS_WaterFLODBlack(WATER_VSINPUT IN)
{
	WATERFLOD_VSOUTPUT OUT;
	OUT.WorldPos	= float4(IN.pos, 0);
	OUT.Position	= mul(float4(IN.pos, 1), gWorldViewProj);
	OUT.Position.z = __XENON? 1 : 0;//PS3 has terrible hardware clipping, thankfully it has depth clamp
	return OUT;
}

WATER_VSOUTPUT VS_WaterVSRT(WATERVSRT_INPUT IN)
{
	WATER_VSOUTPUT OUT;
	float3 position	= float3(IN.pos, IN.height);
	float2 bump		= (IN.params.wz - .5/255)*2 - 1;
	float opacity	= IN.params.y;
	float2 cutoff	= IN.pos.xy < DYNAMICGRIDELEMENTS;
#if !__WIN32PC && !__MAX
	position		= IN.params.y*cutoff.x*cutoff.y > 0? position : sqrt(-1.0f);
#endif //!__WIN32PC && !__MAX
	position.xy		= position.xy - bump;
	float3 worldPos	= position + gWorld[3].xyz;

	OUT.Position		= mul(float4(position,1), gWorldViewProj);
	float4 screenPos	= rageCalcScreenSpacePosition(OUT.Position);
	OUT.PSScreenPos		= screenPos.xy;
	OUT.PSScreenDepth	= screenPos.w;
	OUT.PSTexCoord		= position.xy/(DYNAMICGRIDELEMENTS*DYNAMICGRIDSIZE) + .5;
	float2 warpedPos	= IN.pos + gWorld[3].xy;
	OUT.PSFoamTexCoord	= warpedPos*DynamicFoamScale;
	OUT.PSWetTexCoord	= (warpedPos - gWorldBaseVS.xy)/(DYNAMICGRIDELEMENTS*DYNAMICGRIDSIZE) + 0.5/DYNAMICGRIDELEMENTS;
	OUT.PSNormal		= normalize(float3(bump, 1.5));
	OUT.PSOpacity		= opacity;
	OUT.PSWorldPos		= worldPos;
	return OUT;
}

float4 VS_WaterDepthCommon(float3 position, float alpha)
{
	float3 worldPos		= position + gWorld[3].xyz;
	float2 distXY		= abs(worldPos.xy - gViewInverse[3].xy);
	distXY				= 1 - distXY/DYNAMICGRIDELEMENTS/1.25 > 0;
	float distFactor	= max(distXY.x, distXY.y);
	float depthBlend	= 0.015;
	float eFactor		= log(depthBlend);
	float depth			= eFactor/(-20)/(alpha*alpha)/ 2.71828183;

	float3 view			= worldPos - gViewInverse[3].xyz;
	float depthFactor	= abs(dot(-gViewInverse[2].xyz, view));

	float depthBias		= RSG_DURANGO? 0.15 : 0.01;//Increase the depth bias for now until MS can fix the EQAA issue
	float3 offset		= view*(depth/depthFactor + depthBias);

	float4 outPos		= mul(float4(position + offset - float3(0, 0, .2), 1), gWorldViewProj);

	return outPos;
}

float4 VS_WaterDepthTess(WATERTESS_VSINPUT IN) : SV_Position
{
	float2 worldPos		= IN.tex + gWorld[3].xy;
	float range			= DYNAMICGRIDELEMENTS*DYNAMICGRIDSIZE/2;
	float2 heightClamp	= saturate((range - abs(gWorldBaseVS.xy + range -worldPos.xy))/DYN_WATER_FADEDIST);
	float heightScale   = min(heightClamp.x, heightClamp.y);
	float2 heightTex	= (worldPos.xy - gWorldBaseVS.xy)/(DYNAMICGRIDELEMENTS*DYNAMICGRIDSIZE);

#if __SHADERMODEL >= 40
	float height		= HeightTexture.Load(int3(heightTex*DYNAMICGRIDELEMENTS, 0)).r*heightScale;
#else
	float height		= 0;
#endif
	height				= height > 0?  height*0.5 : height;
	height              = height - .25;

	return VS_WaterDepthCommon(float3(IN.tex, height), GetTesselationAlpha(IN.tex));
}

float4 VS_WaterDepth(WATER_VSINPUT IN) : SV_Position
{
	return VS_WaterDepthCommon(IN.pos, IN.color.a);
}

VSOUTPUT VS_WaterLOD_common(WATERLOW_VSINPUT IN
#if GS_INSTANCED_CUBEMAP
							,uniform bool bUseInst
#endif
							)
{
	float3 position		= float4(IN.posXY, IN.posZ, 1);

	VSOUTPUT OUT;
	OUT.ScreenPos		= 0;
	OUT.tex				= 0;

#if GS_INSTANCED_CUBEMAP
	if (bUseInst)
	{
		OUT.pos				= mul(float4(position, 1), gInstWorldViewProj[INSTOPT_INDEX(IN.InstID)]);
		OUT.ScreenPos.xyz	= position - gInstViewInverse[INSTOPT_INDEX(IN.InstID)][3].xyz;
	}
	else
#endif
	{
		OUT.pos				= mul(float4(position, 1), gWorldViewProj);
		OUT.ScreenPos.xyz	= position - gViewInverse[3].xyz;
	}

	OUT.tex.xy			= (position.xy - gFogOffset)*gFogScale;
	OUT.tex.y			= 1 - OUT.tex.y;

	return OUT;
}

VSOUTPUT VS_WaterLOD(WATERLOW_VSINPUT IN)
{
	return VS_WaterLOD_common(IN
#if GS_INSTANCED_CUBEMAP
		,false
#endif
		);
}

/* ====== PIXEL SHADERS =========== */
#if !defined(SHADER_FINAL)
half4 PS_WaterCalming_BANK_ONLY(VSOUTPUT IN) : COLOR
{
	return float4(lerp(float3(1,0,0), float3(0,0,1), IN.tex.x), 1);
}
#endif // !defined(SHADER_FINAL)

PS_OUTPUTWATERFOG OceanWaterFogCommon(WATER_VSOUTPUT IN, uniform bool vsrt)
{
	wfloat3 V				= (IN.PSWorldPos - gViewInverse[3].xyz)/IN.PSScreenDepth;
	PS_OUTPUTWATERFOG OUT	= WaterFogCommon(	IN.PSWorldPos.xy,
#ifdef NVSTEREO
												IN.ScreenPosStereo.xy/IN.ScreenPosStereo.w,
#else
												IN.PSScreenPos/IN.PSScreenDepth,
#endif
												IN.PSScreenDepth,
												IN.PSWorldPos,
												V,
												IN.PSOpacity);

	wfloat2 tex				= (OUT.Color0.xy - gFogOffset)*gFogScale;
	if(vsrt)
		tex.y				+= 0.5/DYNAMICGRIDELEMENTS;
	else
		tex.y				= 1 - tex.y;

	OUT.Color0.xyz			= w3tex2D(FogSampler, tex).rgb;

	wfloat3 SunDirection	= -gDirectionalLight.xyz;
	wfloat3 depth			= OUT.Color1.w;
	wfloat pierceScale		= pow(saturate(dot(refract(V, IN.Normal, 1/1.65), SunDirection)), 2)*saturate(depth/10);
	OUT.Color1.z			= pierceScale;

	return OUT;
}

PS_OUTPUTWATERFOG PS_WaterFog(WATER_VSOUTPUT IN)
{
	return OceanWaterFogCommon(IN, false);
}

PS_OUTPUTWATERFOG PS_WaterFogVSRT(WATER_VSOUTPUT IN)
{
	return OceanWaterFogCommon(IN, true);
}

half4 PS_UnderwaterCommon(WATER_VSOUTPUT IN, uniform bool UseParaboloidMap)
{
	float4 waterColor = UnderwaterCommon(
			UseParaboloidMap,							//UseParaboloidMap
			0,											//ReflectionBlend
			IN.Normal.xyz,								//Normal
			float3(1,0,0),								//Tangent
			float3(0,1,0),								//Binormal
			IN.PSWorldPos,								//WorldPos
			IN.PSScreenPos/IN.PSScreenDepth,			//ScreenPos
			IN.PSScreenDepth,							//ScreenDepth
			IN.PSWorldPos.xy,							//TexCoord
			gDirectionalLight.xyz,						//SunDirection
			gWaterDirectionalColor.rgb,					//SunColor
			0,											//FoamMask
			0,											//FlowVector
			1,											//BumpIntensity
			0);											//NormalLerp
	return PackWaterColor(waterColor);
}

half4 PS_UnderwaterHigh(WATER_VSOUTPUT IN) : COLOR
{
	return PS_UnderwaterCommon(IN, false);
}
half4 PS_UnderwaterLow(WATER_VSOUTPUT IN) : COLOR
{
	return PS_UnderwaterCommon(IN, true);
}

half4 PS_WaterCommon(WATER_VSOUTPUT IN, uniform bool HighDetail, uniform bool UseFogPrepass, float reflectionBlend)
{
	wfloat4 fog = w4tex2D(FogSampler, IN.TexCoord.xy);
	
	float foamScale			= FOAMSCALE;
	float foamWeight		= FOAMWEIGHT;
	wfloat foamIntensity	= w2tex2D(WetSampler, IN.PSWetTexCoord).r*lerp(0, foamScale, foamWeight);
	foamIntensity			= foamIntensity*saturate((2*DYNAMICGRIDELEMENTS - IN.PSScreenDepth)/(2*DYNAMICGRIDELEMENTS));
	wfloat foamSample		= w2tex2D(StaticFoamSampler, IN.PSFoamTexCoord).g*lerp(0, foamScale, 1 - foamWeight);
	wfloat foamMask			= foamSample;

	wfloat4 waterColor = WaterCommon(
		HighDetail,									//HighDetail
		UseFogPrepass,								//UseFogPrepass
		true,										//UseDynamicFoam
		reflectionBlend,							//ReflectionBlend
		IN.Normal.xyz,								//Normal
		0,											//Tangent
		0,											//Binormal
		IN.WorldPos.xyz,							//WorldPos
		IN.PSScreenPos/IN.PSScreenDepth,			//ScreenPos
#ifdef NVSTEREO
		IN.ScreenPosStereo.xy/IN.ScreenPosStereo.w,	//ScreenPosRfa
#else
		IN.PSScreenPos/IN.PSScreenDepth,			//ScreenPos
#endif
		IN.PSScreenDepth,							//ScreenDepth
		IN.WorldPos.xy,								//TexCoord
		gDirectionalLight.xyz,						//SunDirection
		wfloat4(fog.rgb ,IN.Normal.w*IN.Normal.w),	//FogColor
		foamMask,									//FoamMask
		foamIntensity,								//FoamIntensity
		0,											//FlowVector
		1,											//BumpIntensity
		0);											//NormalLerp
	
	return PackWaterColor(waterColor);
}

half4 PS_Water				(WATER_VSOUTPUT IN): COLOR{ return PS_WaterCommon(IN, false,	true,	0); }
half4 PS_WaterTess			(WATER_VSOUTPUT IN): COLOR{ return PS_WaterCommon(IN, true,		true,	0); }
half4 PS_WaterSinglePass	(WATER_VSOUTPUT IN): COLOR{ return PS_WaterCommon(IN, false,	false,	1); }
half4 PS_nothing			(WATER_VSOUTPUT IN): COLOR{	return 1; }

OutHalf4Color PS_WaterLOD(VSOUTPUT IN)
{
	float3 SunColor		= gDirectionalColour.rgb;
	float3 waterColor	= pow(tex2D(FogSampler, IN.tex.xy).rgb, 2);
	float3 fogLight		= saturate(-gDirectionalLight.z)*SunColor + gLightNaturalAmbient0.rgb;

	waterColor			= waterColor*fogLight;

	float4 fogData		= CalcFogData(IN.ScreenPos.xyz);

	float3 color		= lerp(waterColor, fogData.rgb, fogData.a);

	return CastOutHalf4Color(PackReflection(float4(color, 1)));
}

OutHalf4Color PS_WaterUnderwaterLOD(WATER_VSOUTPUT IN)
{
	float3 viewVector = IN.PSWorldPos - gViewInverse[3].xyz;
	float3 V = normalize(IN.PSWorldPos - gViewInverse[3].xyz);
	float3 N = float3(0, 0, -1);
	float3 refractionDirection	= refract(V, N, RefractionIndex);
	wfloat refractionBlend		= 1 - saturate(refractionDirection.z);
	
	return CastOutHalf4Color(PackReflection(float4(CalcFogData(-viewVector).rgb, refractionBlend)));
}

OutHalf4Color PS_WaterFLOD(WATER_VSOUTPUT  IN)
{
//sometimes with really huge or far away geometry at grazing angles, it looks like the interpolator turns things to garbage
//This is a quick workaround, might need further investigation later
#if RSG_ORBIS
	IN.PSWorldPos.xyz = clamp(IN.PSWorldPos.xyz, -1000000, 1000000);
#endif

	float3	eyeRay		= IN.PSWorldPos.xyz - gViewInverse[3].xyz;
	clip(22000 - length(eyeRay));

	wfloat2 lowBump		=	-(w4tex2D(StaticBumpSampler, IN.PSWorldPos.yx/448 + gScaledTime.x/10).ga*2 - 1)
							-(w4tex2D(StaticBumpSampler, IN.PSWorldPos.xy/512 - gScaledTime.x/10).ag*2 - 1);
	lowBump				= lowBump*OceanBumpIntensity*OceanRippleBumpiness;

	float3 SunDirection = gDirectionalLight.xyz;
	float3 SunColor		= gDirectionalColour.rgb;
	float3 V			= normalize(eyeRay);
	float3 N			= normalize(float3(lowBump, 1));
	float3 reflectionN	= lerp(N, float3(0,0,1), 5/6.0);
	float3 R			= reflect(V, reflectionN);

	float fresnel		= lerp(dot(-V, N), 1, .3);
	fresnel				= tex2D(BlendSampler, fresnel).r;

	wfloat3 reflectionColor	= UnpackReflection_3h(w3tex2D(ReflectionSampler, DualParaboloidTexCoord_2h(R, 1.0f)).rgb);
	wfloat2 tex				= (IN.PSWorldPos.xy - gFogOffset)*gFogScale;
	tex.y					= 1 - tex.y;
	wfloat3 waterColor		= pow(w3tex2D(FogSampler, tex).rgb, 2);
	float3 refractionColor	= waterColor*(WaterAmbientColor); //contains (-WaterDirectionalColor*gDirectionalLight.z + WaterAmbientColor)*gFogLightIntensity
	float3 color			= lerp(refractionColor, reflectionColor, fresnel) + GetSpecularColor(SunDirection, V, N, SunColor, 0, 1);
	float4 fogData			= CalcFogData(eyeRay);

	return CastOutHalf4Color(PackWaterColor(float4(lerp(color, fogData.rgb, fogData.a), 0)));
}

OutHalf4Color PS_WaterFLODBlack(WATERFLOD_VSOUTPUT  IN)
{
	float3	eyeRay = IN.PSWorldPos.xyz - gViewInverse[3].xyz;
	clip(22000 - length(eyeRay));
	return CastOutHalf4Color(PackWaterColor(float4(0, 0, 0, 1)));
}

OutFloat4Color PS_OceanHeight(WATER_VSOUTPUT IN, out float depth : DEPTH)
{
	float3 worldPos		= IN.PSWorldPos;
	float2 heightTex	= (worldPos.xy - gWorldBase)/(DYNAMICGRIDELEMENTS*DYNAMICGRIDSIZE);

	float height		= tex2D(LightingSampler, heightTex + .5/DYNAMICGRIDELEMENTS).x;

	float4 projPos		= mul(float4(worldPos - gWorld[3].xyz + height, 1), gWorldViewProj);
	depth				= fixupDepth(projPos.z);

	return CastOutFloat4Color(0);
}

#if GS_INSTANCED_CUBEMAP
GEN_GSINST_TYPE(Water,VSOUTPUT,SV_RenderTargetArrayIndex)
GEN_GSINST_FUNC(Water,Water,WATERLOW_VSINPUT,VSOUTPUT,VSOUTPUT,VS_WaterLOD_common(IN,true),PS_WaterLOD(IN))
#endif

// ===============================
// technique
// ===============================
technique water
{
	pass fogflat
	{
		VertexShader	= compile VERTEXSHADER	VS_WaterFlat();
		PixelShader		= compile PIXELSHADER	PS_WaterFog()			CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
	pass fogtess
	{
		VertexShader	= compile VERTEXSHADER	VS_WaterTess();
		PixelShader		= compile PIXELSHADER	PS_WaterFog()			CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
	pass fogtessvsrt
	{
		VertexShader	= compile VERTEXSHADER	VS_WaterVSRT();
		PixelShader		= compile PIXELSHADER	PS_WaterFogVSRT()		CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
	pass flat
	{
		VertexShader	= compile VERTEXSHADER	VS_WaterFlat();
		PixelShader		= compile PIXELSHADER	PS_Water()			CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
	pass tess
	{
		VertexShader	= compile VERTEXSHADER	VS_WaterTess();
		PixelShader		= compile PIXELSHADER	PS_WaterTess()		CGC_FLAGS("-unroll all --O1 -fastmath");
	}
	pass tessvsrt
	{
		VertexShader	= compile VERTEXSHADER	VS_WaterVSRT();
		PixelShader		= compile PIXELSHADER	PS_WaterTess()		CGC_FLAGS("-unroll all --O1 -fastmath");
	}
	pass singlepassflat
	{
		VertexShader	= compile VERTEXSHADER	VS_Water();
		PixelShader		= compile PIXELSHADER	PS_WaterSinglePass()	CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
	pass singlepasstess
	{
		VertexShader	= compile VERTEXSHADER	VS_WaterTess();
		PixelShader		= compile PIXELSHADER	PS_WaterSinglePass()	CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
	pass singlepasstessvsrt
	{
		VertexShader	= compile VERTEXSHADER	VS_WaterVSRT();
		PixelShader		= compile PIXELSHADER	PS_WaterSinglePass()	CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
	pass underwaterflat
	{
		VertexShader	= compile VERTEXSHADER	VS_Water();
		PixelShader		= compile PIXELSHADER	PS_UnderwaterHigh()		CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
	pass underwatertess
	{
#if __PS3
		VertexShader	= compile VERTEXSHADER	VS_WaterVSRT();
#else
		VertexShader	= compile VERTEXSHADER	VS_WaterTess();
#endif //VS_WaterTess
		PixelShader		= compile PIXELSHADER	PS_UnderwaterHigh()		CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
	pass underwaterflatlow
	{
		VertexShader	= compile VERTEXSHADER	VS_Water();
		PixelShader		= compile PIXELSHADER	PS_UnderwaterLow()		CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
	pass underwatertesslow
	{
#if __PS3
		VertexShader	= compile VERTEXSHADER	VS_WaterVSRT();
#else
		VertexShader	= compile VERTEXSHADER	VS_WaterTess();
#endif //VS_WaterTess
		PixelShader		= compile PIXELSHADER	PS_UnderwaterLow()		CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
	pass clear
	{
		VertexShader	= compile VERTEXSHADER	VS_WaterDepth();
		PixelShader		= compile PIXELSHADER	PS_WaterDepth()			CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
	pass cleartess
	{
		VertexShader	= compile VERTEXSHADER	VS_WaterDepthTess();
		PixelShader		= compile PIXELSHADER	PS_WaterDepth()			CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
	pass height
	{
		VertexShader	= compile VERTEXSHADER	VS_Water();
		PixelShader		= compile PIXELSHADER	PS_OceanHeight()		CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
	pass cube
	{
		VertexShader	= compile VERTEXSHADER	VS_WaterLOD();
		PixelShader		= compile PIXELSHADER	PS_WaterLOD()			CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
	pass cubeunderwater
	{
		VertexShader	= compile VERTEXSHADER	VS_WaterFlat();
		PixelShader		= compile PIXELSHADER	PS_WaterUnderwaterLOD()	CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
#if GS_INSTANCED_CUBEMAP
	pass cubeinst
	{
		VertexShader = compile VSGS_SHADER VS_WaterTransformGSInst();
		SetGeometryShader(compileshader(gs_5_0, GS_WaterGSInstPassThrough()));
		PixelShader  = compile PIXELSHADER PS_WaterTexturedGSInst() 	CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
#endif
	pass FLOD
	{
		VertexShader	= compile VERTEXSHADER	VS_WaterFLOD();
		PixelShader		= compile PIXELSHADER	PS_WaterFLOD()			CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
	pass FLODBlack
	{
		VertexShader	= compile VERTEXSHADER	VS_WaterFLODBlack();
		PixelShader		= compile PIXELSHADER	PS_WaterFLODBlack()		CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}

#if __PS3

PS_OUTPUTWATERVSRT PS_VertexStreamParams(WATER_VSOUTPUT IN)
{
	float2 heightClamp	= saturate(DYN_WATER_FADEDIST*(.99 - abs(IN.PSScreenPos.xy*2 - 1)));
	float heightScale   = min(heightClamp.x, heightClamp.y);
	float2 heightTex	= FindGridFromWorld(IN.PSWorldPos.yxyx).xy;
	float4 heightTexO	= fmod(heightTex.xxyy + DYNAMICGRIDELEMENTS + float4(-1, 1,-1, 1), DYNAMICGRIDELEMENTS);
	heightTex			= heightTex/DYNAMICGRIDELEMENTS;
	heightTexO			= heightTexO/DYNAMICGRIDELEMENTS;

	float height		= tex2D(HeightSampler, heightTex).r*heightScale;
	float4 heights;
	heights.x			= tex2D(HeightSampler, float2(heightTexO.x, heightTex.y)).r;
	heights.y			= tex2D(HeightSampler, float2(heightTexO.y, heightTex.y)).r;
	heights.z			= tex2D(HeightSampler, float2(heightTex.x, heightTexO.z)).r;
	heights.w			= tex2D(HeightSampler, float2(heightTex.x, heightTexO.w)).r;

	float bumpScale		= saturate(distance(IN.PSWorldPos.xyz, CameraPosition.xyz)/128);
	bumpScale			= 1 - bumpScale*bumpScale;
	float2 bump			= (heights.xz - heights.yw);
	bump				= normalize(bump + 0.00001)*(sqrt(4*length(bump + 0.00001) + 1) - 1);	
	bump				= bump*heightScale*bumpScale;

	float4 fogSample	= tex2D(FogSampler, IN.PSTexCoord);

	PS_OUTPUTWATERVSRT OUT;
	OUT.Color0	= float4(bump.yx*.5 + .5, IN.PSOpacity, fogSample.a);
	OUT.Color1	= fogSample;
	OUT.Color2	= unpack_4ubyte(height + IN.PSWorldPos.z);
	return OUT;
}

technique vertexstream
{
	pass vsrtparams
	{
		VertexShader	= compile VERTEXSHADER	VS_Water();
		PixelShader		= compile PIXELSHADER	PS_VertexStreamParams()	CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
};

#endif //__PS3

#if !__MAX
#include "../Debug/debug_overlay_water.fxh"
// no entity select because water quads are not entities
#endif // !__MAX
