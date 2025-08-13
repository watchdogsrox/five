#ifndef PRAGMA_DCL
	#pragma dcl position texcoord0
	#define PRAGMA_DCL
#endif

#define SPECULAR 0
#define REFLECT 0

#define ALPHA_SHADER
#define NO_SKINNING

#include "../common.fxh"

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
#define IS_FOAM 1
#include "water_common.fxh"

#pragma sampler 2
BeginSampler	(sampler2D, FoamTexture, FoamSampler, FoamTexture)
string	UIName				= "Foam Texture";
string 	TCPTemplateRelative	= "maps_other/WaterFoamMap";
int		TCPAllowOverride	= 0;
string	TextureType			= "Foam";
ContinueSampler	(sampler2D, FoamTexture, FoamSampler, FoamTexture)
	AddressU	= WRAP;
	AddressV	= WRAP;
	MIPFILTER	= MIPLINEAR;
	MINFILTER	= HQLINEAR;
	MAGFILTER	= MAGLINEAR;
EndSampler;

BeginSampler	(sampler2D, FoamOpacityTexture, FoamOpacitySampler, FoamOpacityTexture)
	string	UIName				= "Foam Opacity Map";
	string	UvSetName			= "FoamOpacityMap";
	int		UvSetIndex			= 1;
	string 	TCPTemplateRelative	= "maps_other/FoamOpacityMap";
	string	TextureType="Foam";
int		TCPAllowOverride	= 0;
ContinueSampler	(sampler2D, FoamOpacityTexture, FoamOpacitySampler, FoamOpacityTexture)
	AddressU	= CLAMP;
	AddressV	= CLAMP;
	MIPFILTER	= MIPLINEAR;
	MINFILTER	= HQLINEAR;
	MAGFILTER	= MAGLINEAR;
EndSampler;

BEGIN_RAGE_CONSTANT_BUFFER(water_foam_locals,b2)
float HeightThreshold : HeightThreshold
<
	string UIName	= "Height Threshold";
	string UIWidget	= "Numeric";
	float UIMin		= -2.00;
	float UIMax		= 2.00;
	float UIStep	= 0.1;
> = 0.25;

float AnimationOpacity : AnimationOpacity
<
	string UIName	= "AnimationOpacity";
	string UIWidget	= "Numeric";
	float UIMin		= 0.00;
	float UIMax		= 1.00;
	float UIStep	= 0.10;
> = 0.5;

float3 WaterFogFadeColor : WaterFogFadeColor
<
	string UIName	= "Fog Fade Color";
	string UIWidget	= "Vector";
	float UIMin		= 0.00;
	float UIMax		= 1.00;
	float UIStep	= 0.10;
> = float3(.3,.7, 1);

float FoamScale : FoamScale
<
	string UIName	= "Foam Scale";
	string UIWidget	= "Numeric";
	float UIMin		= 0.00;
	float UIMax		= 5.00;
	float UIStep	= 0.10;
> = 0.3;

EndConstantBufferDX10(water_foam_locals)

struct	VS_INPUT
{ 
	float4	Position	:	POSITION;
	float4	TexCoord0	:	TEXCOORD0;
};
struct VS_OUTPUT
{
	DECLARE_POSITION(Position)
	float4	WorldPos		:	TEXCOORD0;//bumpiness in w channel
	float4	ScreenPos		:	TEXCOORD1;//water fog clarity in w channel
	float4	WaterUVandFlow	:	TEXCOORD6;
#define PSWorldPos	WorldPos.xyz
#define PSTexCoord	WaterUVandFlow.xy
};

VS_OUTPUT	VSRiverWater	(VS_INPUT IN) 
{ 
	VS_OUTPUT	OUT;

	float4	Position = mul(IN.Position, gWorld);

	float	DistanceToEye = distance(gViewInverse[3].xyz, Position.xyz);

	float4 pPos=Position;

	OUT.WorldPos.xyz= Position.xyz;
	OUT.WorldPos.w	= 0;

	float2 heightClamp	= saturate((128-abs(gWorldBaseVS.xy+DYNAMICGRIDELEMENTS*DYNAMICGRIDSIZE/2-OUT.WorldPos.xy))/DYN_WATER_FADEDIST);
	float heightScale   = min(heightClamp.x, heightClamp.y);

#if __PS3
	float2 heightTex	= FindGridFromWorld(OUT.WorldPos.yxyx).xy;
	float4 heightTexO	= fmod(heightTex.xxyy + DYNAMICGRIDELEMENTS + float4(-1, 1,-1, 1), DYNAMICGRIDELEMENTS);
	float2 gridBase		= FindGridFromWorld(gWorldBaseVS.xyxy).xy;
	heightTex			/= DYNAMICGRIDELEMENTS;
	heightTexO			/= DYNAMICGRIDELEMENTS;
#else
	float2 heightTex	= (OUT.WorldPos.xy-gWorldBaseVS.xy)/(DYNAMICGRIDELEMENTS*DYNAMICGRIDSIZE);
#endif

	float height		= 0;
	OUT.WorldPos.w		= height;
	OUT.WorldPos.z		+= height;
	IN.Position.z		+= height;
	const float2 hOffsets = float2(-1.0/DYNAMICGRIDELEMENTS, 1.0/DYNAMICGRIDELEMENTS);
	float4 heights = 0;	
	heights				*= heightScale;
	//heights=height+heights;
#if __PS3
	float2 bump=heights.zx - heights.wy;
#else
	float2 bump=heights.xz - heights.yw;
#endif

	float3 normal	= normalize(float3(bump, 1));

	float2 tDir		= -sqrt(length(normal.xy))*normal.xy/(length(normal.xy)+0.00001);//this causes div by zero on PS3 if I use normalize() :/
	IN.Position.xy	+= tDir;
	OUT.WorldPos.xy	+= tDir;
	OUT.Position	= mul(IN.Position, gWorldViewProj);

	OUT.ScreenPos = rageCalcScreenSpacePosition(OUT.Position);

	OUT.WaterUVandFlow.xyzw = IN.TexCoord0.xyxy;

	OUT.Position.z*=.9999999;

	return(OUT);
}

half4 PS_Foam				(VS_OUTPUT IN) : COLOR	{ 

	float2 ScreenPos = IN.ScreenPos.xy/IN.ScreenPos.w;

	float ScreenDepth = IN.ScreenPos.z;
	float depth	= 0;

	float heightOpacity			= saturate(IN.WorldPos.w+HeightThreshold);
	float heightOpacityBlend	= heightOpacity*.5+.5;
	float animatedFoamOpacity	= lerp(length(2*tex2D(WaterBumpSampler, IN.WorldPos.xy*RippleScale).ba-1), 1.0, AnimationOpacity*heightOpacityBlend);	
	float staticFoamOpacity		= tex2D(FoamSampler, IN.WorldPos.xy*FoamScale).r + 1;
	float foamOpacity			= pow(tex2D(FoamOpacitySampler, IN.WaterUVandFlow.xy).r, 2)*heightOpacity*depth;
	float alpha					= saturate(animatedFoamOpacity*staticFoamOpacity*foamOpacity);
	float3 color				= lerp(WaterFogFadeColor, float3(1,1,1), alpha);//*gFogLight.rgb;

	return PackColor(float4(color, alpha*gInvColorExpBias));
}

#if FORWARD_TECHNIQUES
technique draw
{
	pass p0
	{ 
		alphatestenable				= false;
		alphablendenable			= true;
#if __XENON
		highprecisionblendenable	= true;
		SlopeScaleDepthBias			= 0.1;
#else
		SlopeScaleDepthBias			= -0.1;
#endif

		cullmode					= none;
		zFunc						= FixedupLESS;
		zwriteenable				= false;
		SrcBlend					= SrcAlpha;
		DestBlend					= One;
		VertexShader = compile VERTEXSHADER	VSRiverWater();
		PixelShader  = compile PIXELSHADER	PS_Foam() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
#endif // FORWARD_TECHNIQUES

#if FORWARD_TECHNIQUES
technique lightweight0_draw
{
	pass p0
	{
		alphatestenable				= false;
		alphablendenable			= true;
#if __XENON
		highprecisionblendenable	= true;
		SlopeScaleDepthBias			= 0.1;
#else
		SlopeScaleDepthBias			= -0.1;
#endif

		cullmode					= none;
		zFunc						= FixedupLESS;
		zwriteenable				= false;
		SrcBlend					= SrcAlpha;
		DestBlend					= One;
		VertexShader = compile VERTEXSHADER	VSRiverWater();
		PixelShader  = compile PIXELSHADER	PS_Foam() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
#endif // FORWARD_TECHNIQUES

#if FORWARD_TECHNIQUES
technique lightweight4_draw
{
	pass p0
	{
		alphatestenable				= false;
		alphablendenable			= true;
#if __XENON
		highprecisionblendenable	= true;
		SlopeScaleDepthBias			= 0.1;
#else
		SlopeScaleDepthBias			= -0.1;
#endif

		cullmode					= none;
		zFunc						= FixedupLESS;
		zwriteenable				= false;
		SrcBlend					= SrcAlpha;
		DestBlend					= One;
		VertexShader = compile VERTEXSHADER	VSRiverWater();
		PixelShader  = compile PIXELSHADER	PS_Foam() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
#endif // FORWARD_TECHNIQUES

#if !__MAX
#define VS VSRiverWater
SHADERTECHNIQUE_ENTITY_SELECT(VS(), VS())
#include "../Debug/debug_overlay_water.fxh"
#undef VS
#endif // !__MAX
