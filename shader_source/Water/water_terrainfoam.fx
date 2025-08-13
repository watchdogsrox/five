#ifndef PRAGMA_DCL
	#pragma dcl position texcoord0 normal tangent color0
	#define PRAGMA_DCL
	#define USE_WETNESS_MULTIPLIER
#endif

//#define DEFERRED_UNPACK_LIGHT
#define SPECULAR 0
#define REFLECT 0
#define USE_SPECULAR
#define DEFERRED_NO_LIGHT_SAMPLERS

//#define ALPHA_SHADER
#define DECAL_SHADER
#define NO_SKINNING
#include "../common.fxh"

#define WATER_DECAL

#if SPECULAR
const float specularFalloffMult		= 8.0;
const float specularIntensityMult	= 0.75;
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
#include "water_common.fxh"
#include "../../../rage/base/src/shaderlib/rage_calc_noise.fxh"

BeginSampler	(sampler2D, FoamTexture, FoamSampler, FoamTexture)
    string  UIName = "Foam Opacity Map";
	string	UvSetName = "FoamOpacityMap";
	int		UvSetIndex = 1;
	string 	TCPTemplateRelative = "maps_other/FoamOpacityMap";
	int		TCPAllowOverride = 0;
	string	TextureType="Foam";
ContinueSampler	(sampler2D, FoamTexture, FoamSampler, FoamTexture)
	AddressU  = WRAP;
	AddressV  = CLAMP;
	MIPFILTER = MIPLINEAR;
	MINFILTER = HQLINEAR;
	MAGFILTER = MAGLINEAR;
EndSampler;

BEGIN_RAGE_CONSTANT_BUFFER(water_terrainfoam_locals,b0)
float WaveOffset : WaveOffset
<
	string UIName	= "Wave Offset";
	string UIWidget	= "Numeric";
	float UIMin		= -1.00;
	float UIMax		= 1.00;
	float UIStep	= 0.1;
> = 0.25;

float WaterHeight : WaterHeight
<
	string UIName	= "Water Height";
	string UIWidget	= "Numeric";
	float UIMin		= -5.00;
	float UIMax		= 300.00;
	float UIStep	= 0.1;
> = 0.0;

float WaveMovement : WaveMovement
<
	string UIName	= "Wave Movement";
	string UIWidget	= "Numeric";
	float UIMin		= 0.00;
	float UIMax		= 3.00;
	float UIStep	= 0.10;
> = 1.0;

float HeightOpacity : HeightOpacity
<
	string UIName	= "Height Opacity";
	string UIWidget	= "Numeric";
	float UIMin		= 0.00;
	float UIMax		= 50.00;
	float UIStep	= 1.00;
> = 10.0;

EndConstantBufferDX10(water_terrainfoam_locals)

struct	VS_INPUT
{ 
	float3	Position	:	POSITION;
	float3	Normal		:	NORMAL;
	float3	Tangent		:	TANGENT;
	float2	TexCoord	:	TEXCOORD0;
	float4	Color0		:	COLOR0;
};
struct VS_OUTPUT
{
	DECLARE_POSITION(Position)
	float4	WorldPos		:	TEXCOORD0;
	float3	Normal			:	TEXCOORD1;
	float3	Tangent			:	TEXCOORD2;
	float3	Binormal		:	TEXCOORD3;
	float4	TexCoord		:	TEXCOORD4;
#define PSWorldPos	WorldPos.xyz
#define PSTexCoord	TexCoord.xy
};

VS_OUTPUT	VSFoam	(VS_INPUT IN) 
{ 
	VS_OUTPUT	OUT;

	OUT.WorldPos.xyz	= mul(float4(IN.Position, 1.0f), gWorld).xyz;
	OUT.WorldPos.w	= IN.Color0.a;

	OUT.Position		= mul(float4(IN.Position, 1.0), gWorldViewProj);
	OUT.Normal			= IN.Normal;
	OUT.Tangent			= IN.Tangent;
	OUT.Binormal		= normalize(cross(IN.Normal, IN.Tangent));
	OUT.TexCoord.xy		= IN.TexCoord;
	OUT.TexCoord.zw		= (OUT.WorldPos.xy-gWorldBaseVS.xy)/(DYNAMICGRIDELEMENTS*DYNAMICGRIDSIZE);

	return(OUT);
}

DeferredGBuffer PSFoamDeferred(VS_OUTPUT IN)
{
	DeferredGBuffer OUT;
	float wetSample = tex2D(WetSampler, IN.TexCoord.zw).r;
	float wet		= wetSample*2;
	float wetHeight = wet + WaterHeight;
	wet				*= WaveMovement;

	float foamIntensity = 1;
		
	float height = IN.WorldPos.z;

	float2 foamTex = IN.TexCoord.xy - float2(0, wet) + WaveOffset;

	foamIntensity = max(height - WaterHeight, 0)*HeightOpacity;
	
	float3 staticFoamSample	= tex2D(FoamSampler, foamTex).rgb;
	float staticFoamColor	= staticFoamSample.g;
	float4 bumpSample = tex2D(WaterBumpSampler, IN.TexCoord.xy + foamTex);

	float animOpacity =  bumpSample.r;
	float alpha = saturate(staticFoamColor * animOpacity * foamIntensity * IN.WorldPos.w * wetSample);


	float4 outColor = float4(1.0f.xxx, gInvColorExpBias*alpha);

	SurfaceProperties surfaceInfo = (SurfaceProperties)0;
	surfaceInfo.surface_worldNormal = IN.Normal;
	surfaceInfo.surface_baseColor = 1;
	surfaceInfo.surface_diffuseColor = outColor;
#if SPECULAR
	surfaceInfo.surface_specularIntensity = specularIntensityMult;
	surfaceInfo.surface_specularExponent = specularFalloffMult;
	surfaceInfo.surface_fresnel = 1;
#endif	// SPECULAR
#if REFLECT
	surfaceInfo.surface_reflectionColor = 1;
#endif // REFLECT
	surfaceInfo.surface_emissiveIntensity = 0;
#if SELF_SHADOW
	surfaceInfo.surface_selfShadow = 1;
#endif // SELF_SHADOW

	StandardLightingProperties surfaceStandardLightingProperties = DeriveLightingPropertiesForCommonSurface( surfaceInfo );

	//return PackColor(outColor);

	OUT = PackGBuffer( surfaceInfo.surface_worldNormal, surfaceStandardLightingProperties );

	return OUT;
}

half4 PSFoam(VS_OUTPUT IN) : COLOR
{
	DeferredGBuffer OUT = PSFoamDeferred(IN);
	return PackHdr(OUT.col0.rgba);
}

#if __SHADERMODEL >= 40 && DEFERRED_TECHNIQUES && NON_CUTOUT_DEFERRED_TECHNIQUES
DeferredGBuffer PSFoamDeferredAlphaClip(VS_OUTPUT IN) : COLOR
{
	FadeDiscard( IN.Position, globalFade );

	return PSFoamDeferred(IN);
}
#endif//__SHADERMODEL >= 40 && DEFERRED_TECHNIQUES && NON_CUTOUT_DEFERRED_TECHNIQUES


#if FORWARD_TECHNIQUES
technique draw
{
	pass p0
	{
		VertexShader	= compile VERTEXSHADER	VSFoam();
		PixelShader		= compile PIXELSHADER	PSFoam()	CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
#endif // FORWARD_TECHNIQUES

#if DEFERRED_TECHNIQUES && NON_CUTOUT_DEFERRED_TECHNIQUES
technique deferred_draw
{
	pass p0
	{
		VertexShader	= compile VERTEXSHADER	VSFoam();
		PixelShader		= compile PIXELSHADER	PSFoamDeferred()	CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}

#if __SHADERMODEL >= 40
technique deferredalphaclip_draw
{
	pass p0
	{
		VertexShader	= compile VERTEXSHADER	VSFoam();
		PixelShader		= compile PIXELSHADER	PSFoamDeferredAlphaClip()	CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
#endif //__SHADERMODEL >= 40

#endif // DEFERRED_TECHNIQUES && NON_CUTOUT_DEFERRED_TECHNIQUES

#if __MAX
technique tool_draw
{
	pass p0
	{
		alphatestenable	= false;
		VertexShader	= compile VERTEXSHADER	VSFoam();
		PixelShader		= compile PIXELSHADER	PSFoam() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
#else
	#define VS VSFoam
	SHADERTECHNIQUE_ENTITY_SELECT(VS(), VS())
	#include "../Debug/debug_overlay_water.fxh"
	#undef VS
#endif
