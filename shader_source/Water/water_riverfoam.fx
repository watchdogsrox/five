
#ifndef PRAGMA_DCL
	#pragma dcl position color0 texcoord0 normal tangent
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

#define SHADOW_CASTING            (0)
#define SHADOW_CASTING_TECHNIQUES (0)
#define SHADOW_RECEIVING          (1)
#define SHADOW_PROCESSING         (0)
#include "../Lighting/Shadows/cascadeshadows.fxh"
#include "../Lighting/lighting.fxh"
#include "../Debug/EntitySelect.fxh"

#pragma constant 130

#include "../../../rage/base/src/shaderlib/rage_xplatformtexturefetchmacros.fxh"
#include "water_river_common.fxh"
#define IS_RIVER
#include "water_common.fxh"
#include "../../../rage/base/src/shaderlib/rage_calc_noise.fxh"
#include "water_river_wave.fxh"

#pragma sampler 2
BeginSampler	(sampler2D, FoamTexture, FoamSampler, FoamTexture)
string	UIName				= "Foam Texture";
string 	TCPTemplateRelative	= "maps_other/WaterFoamMap";
int		TCPAllowOverride	= 0;
string	TextureType			= "Foam";
ContinueSampler	(sampler2D, FoamTexture, FoamSampler, FoamTexture)
	AddressU      = WRAP;
	AddressV      = WRAP;
	MIPFILTER     = MIPLINEAR;
	MINFILTER     = MINANISOTROPIC;
	MAGFILTER     = MAGLINEAR;
	ANISOTROPIC_LEVEL(8)
EndSampler;

struct	VS_INPUT
{ 
	float3 Position	: POSITION;
	float4 Color	: COLOR0;
	float2 TexCoord	: TEXCOORD0;
	float3 Normal	: NORMAL;
	float3 Tangent	: TANGENT;
};

struct VS_OUTPUT
{
	DECLARE_POSITION(Position)
	#if __MAX
		float4 WorldPos		: TEXCOORD0;
	#else
		inside_centroid float4 WorldPos		: TEXCOORD0;
	#endif
	float4 ScreenPos	: TEXCOORD1;
	float2 TexCoord		: TEXCOORD2;
	float3 Ambient		: TEXCOORD3;
	float3 Directional	: TEXCOORD4;
#ifdef NVSTEREO
	float4 ScreenPosStereo : TEXCOORD5;
#endif
#define PSWorldPos	WorldPos.xyz
#define FoamOpacity	WorldPos.w
#define PSTexCoord	TexCoord.xy
};

VS_OUTPUT	VS (VS_INPUT IN)
{ 
	VS_OUTPUT	OUT;

	OUT.Position		= mul(float4(IN.Position, 1), gWorldViewProj);
	OUT.Position.z		*= 0.999995;

	OUT.WorldPos.xyz	= IN.Position + gWorld[3];
	OUT.FoamOpacity		= IN.Color.g;
	OUT.ScreenPos		= rageCalcScreenSpacePosition(OUT.Position);
#ifdef NVSTEREO
	OUT.ScreenPosStereo = rageCalcScreenSpacePosition(MonoToStereoClipSpace(OUT.Position));
#endif
	OUT.TexCoord		= IN.TexCoord + float2(gScaledTime.x*RippleSpeed, 0);

	surfaceProperties surface;
	surface.normal					= IN.Normal;
	surface.position				= OUT.WorldPos.xyz;

	materialProperties material;
	material.ID						= 0;
	material.diffuseColor			= 1;
	material.naturalAmbientScale	= 1;
	material.artificialAmbientScale = gInInterior;
	material.inInterior				= gInInterior;
	material.emissiveIntensity		= 0;

	OUT.Ambient	= calculateAmbient(		true,
										false,
										true,
										false,
										surface,
										material,
										1);

	float3 sunColor		= gDirectionalColour.rgb;
	float3 sunDirection	= gDirectionalLight.xyz;
	OUT.Directional		= saturate(dot(IN.Normal, -sunDirection))*sunColor;

	return OUT;
}

half4 PSFoam (VS_OUTPUT IN) : COLOR
{
#ifdef NVSTEREO
	float2 screenPos	= IN.ScreenPosStereo.xy/IN.ScreenPosStereo.w;
#else
	float2 screenPos	= IN.ScreenPos.xy/IN.ScreenPos.w;
#endif

	// get the foam samples
	float4 foamSample	= tex2D(FoamSampler, IN.TexCoord);
	float alpha			= foamSample.a;
	float shadow		= tex2D(LightingSampler, screenPos).a;
	float3 foamColor	= IN.Ambient + IN.Directional*shadow;

	float foamAlpha		= pow(IN.FoamOpacity*alpha, 2);

	return PackHdr(float4(foamColor, foamAlpha));
}

#if FORWARD_TECHNIQUES
technique alt7_draw
{
	pass p0
	{
		ZWriteEnable			= false;
		zFunc					= FixedupLESS;

#if RSG_ORBIS

#if SUPPORT_INVERTED_PROJECTION
		SlopeScaleDepthBias		=  12.0;
#else
		SlopeScaleDepthBias		= -12.0;
#endif

#else

#if SUPPORT_INVERTED_PROJECTION
		SlopeScaleDepthBias		=  0.2;
#else
		SlopeScaleDepthBias		= -0.2;
#endif

#endif
		CullMode				= none;
		AlphaBlendEnable		= true;
		SrcBlend				= SrcAlpha;
		DestBlend				= InvSrcAlpha;
		VertexShader = compile VERTEXSHADER VS();
		PixelShader  = compile PIXELSHADER PSFoam() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
#endif // FORWARD_TECHNIQUES

#if !__MAX
SHADERTECHNIQUE_ENTITY_SELECT(VS(), VS())
#include "../Debug/debug_overlay_water.fxh"
#endif // !__MAX
