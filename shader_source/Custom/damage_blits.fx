
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse texcoord0 normal
	#define PRAGMA_DCL
#endif

#include "../common.fxh"

#if __PS3 == 1 && (!defined CGC_DEFAULTFLAGS)
	#define CGC_DEFAULTFLAGS	"-unroll all --O1 -fastmath"
#endif


BeginSampler(sampler, DiffuseTex, DiffuseTexSampler, DiffuseTex)
string UIWidget = "DiffuseTex";
string UIName = "DiffuseTex";
ContinueSampler(sampler, DiffuseTex, DiffuseTexSampler, DiffuseTex)
	AddressU = Border;
	AddressV = Border;
	MIPFILTER = MIPLINEAR;
	MINFILTER = MINANISOTROPIC;
	MAGFILTER = MAGLINEAR; 
	ANISOTROPIC_LEVEL(8)
	MipMapLodBias=-1.0;
 	BorderColor = 0x00000000; 
EndSampler;	 

BeginSampler(sampler, DiffuseSplatterTex, DiffuseSplatterTexSampler, DiffuseSplatterTex)
string UIWidget = "DiffuseSplatterTex";
string UIName = "DiffuseSplatterTex";
ContinueSampler(sampler, DiffuseSplatterTex, DiffuseSplatterTexSampler, DiffuseSplatterTex)
	AddressU = Border;
	AddressV = Border;
	MIPFILTER = MIPLINEAR;
	MINFILTER = MINANISOTROPIC;
	MAGFILTER = MAGLINEAR; 
	ANISOTROPIC_LEVEL(8)
	MipMapLodBias=-1.0;
	BorderColor = 0x00000000; 
EndSampler;

// TODO when we separate the wounds from the splatter, we could probably just reuse the samplers.
BeginSampler(sampler, DiffuseWoundTex, DiffuseWoundTexSampler, DiffuseWoundTex)
string UIWidget = "DiffuseWoundTex";
string UIName = "DiffuseWoundTex";
ContinueSampler(sampler, DiffuseWoundTex, DiffuseWoundTexSampler, DiffuseWoundTex)
	AddressU = Border;
	AddressV = Border;
	MIPFILTER = MIPLINEAR;
	MINFILTER = MINANISOTROPIC;
	MAGFILTER = MAGLINEAR; 
	ANISOTROPIC_LEVEL(8)
	MipMapLodBias=-1.0;
 	BorderColor = 0x00000000; 
EndSampler;

BeginSampler(sampler, DiffuseNoBorderTex, DiffuseNoBorderTexSampler, DiffuseNoBorderTex)
string UIWidget = "DiffuseTex";
string UIName = "DiffuseTex";
ContinueSampler(sampler, DiffuseNoBorderTex, DiffuseNoBorderTexSampler, DiffuseNoBorderTex)
	AddressU = clamp;
	AddressV = clamp;
	MIPFILTER = MIPLINEAR;
	MINFILTER = MINANISOTROPIC;
	MAGFILTER = MAGLINEAR; 
	ANISOTROPIC_LEVEL(8)
	MipMapLodBias=-1.0;
EndSampler;

BeginSampler(sampler2D,DecorationTintPaletteTex,DecorationTintPaletteTexSampler,DecorationTintPaletteTex)
ContinueSampler(sampler2D,DecorationTintPaletteTex,DecorationTintPaletteTexSampler,DecorationTintPaletteTex)
   MinFilter = POINT;
   MagFilter = POINT;
   MipFilter = POINT;
   AddressU  = CLAMP;
   AddressV  = CLAMP;
EndSampler;

BEGIN_RAGE_CONSTANT_BUFFER(damage_blits_locals,b0)
float4 SoakFrameInfo : SoakFrameInfo;  
float4 WoundFrameInfo : WoundFrameInfo;  
float4 SplatterFrameInfo : SplatterFrameInfo;  
float4 DecorationFrameInfo : DecorationFrameInfo;  // this should just alias the soakFrame Info so we have less to send down
float3 DecorationTintPaletteSel : DecorationTintPaletteSel;
float4 DecorationTattooAdjust:DecorationTattooAdjust;
EndConstantBufferDX10(damage_blits_locals)

#define GetFramesPerRow(data)	uint(data.y)
#define GetSubFrameScale(data)	float2(1.0f/data.y,1.0/data.x)
#define GetTexCenterUV(data)	(data.zw)

//
// some techniques for the character blood decals
//

struct DamageVertex
{
    float3 pos						: POSITION;		// z has frame info
	float4 color0					: COLOR0;		// r = intensity, g = alpha, b = UV index, a = unused
};


struct DamageVertex_Out  
{
	DECLARE_POSITION(pos)
	float4 color0					: TEXCOORD0;	// r = intensity, g = alpha, b = free, a = frame blend alpha
	float4 texCoords				: TEXCOORD1;	
};

struct DamageSimpleVertex_Out  
{
	DECLARE_POSITION(pos)
};


float4 CalcFrameSubFrameOffsets2(float4 frameData, float floatFrame, out float alpha )
{
	alpha = frac(floatFrame);
	uint frame = floatFrame;
	return int4((frame%GetFramesPerRow(frameData)), frame/GetFramesPerRow(frameData),((frame+1)%GetFramesPerRow(frameData)), (frame+1)/GetFramesPerRow(frameData)) * GetSubFrameScale(frameData).xyxy;	
}


// float4 CalcFrameSubFrameOffsets2(float4 frameData, int frame, float alpha)
// {
// 	return int4((frame%GetFramesPerRow(frameData)), frame/GetFramesPerRow(frameData),((frame+1)%GetFramesPerRow(frameData)), (frame+1)/GetFramesPerRow(frameData)) * GetSubFrameScale(frameData).xyxy;	
// }

// This has to stay at global scope or it won't compile for the PS3
// NOTE:- Arays of float2's don`t work properly as shader locals. On Orbis these arrays a dealt with as shader locals (DX platforms embed them in them shader exe - so they are ok).
IMMEDIATECONSTANT float4 g_UVOffset[4*2] = {
	float4(0.0,0.0, 0.0, 0.0), float4(0.0,1.0, 0.0, 0.0), float4(1.0,1.0, 0.0, 0.0), float4(1.0,0.0, 0.0, 0.0), // normal
	float4(0.0,1.0, 0.0, 0.0), float4(0.0,0.0, 0.0, 0.0), float4(1.0,0.0, 0.0, 0.0), float4(1.0,1.0, 0.0, 0.0), // Flip V
}; 

DamageVertex_Out TransformBloodCommon(DamageVertex IN, uniform float4 frameInfo)
{
	DamageVertex_Out OUT;
	float2 pos = (IN.color0.g==0) ? float2(-1000,-1000) : IN.pos.xy;

	OUT.pos =  mul(float4(pos,0,1), gWorldViewProj);
	OUT.color0.rgb = IN.color0.rgb;  // frame alpha will be in w
	OUT.texCoords = (g_UVOffset[int(IN.color0.b*255+.5)] * GetSubFrameScale(frameInfo)).xyxy + CalcFrameSubFrameOffsets2(frameInfo, IN.pos.z, OUT.color0.a);
	OUT.color0.b = float(int(IN.pos.z));

	return OUT;
}

DamageVertex_Out VS_TransformBloodSplatter(DamageVertex IN) 
{ 
	return TransformBloodCommon(IN,SplatterFrameInfo);  // we could use an index to pick the info, then these would be the same
}

DamageVertex_Out VS_TransformBloodWound(DamageVertex IN)
{
	return TransformBloodCommon(IN,WoundFrameInfo);
}

DamageVertex_Out VS_TransformBloodSoak(DamageVertex IN)
{
	return TransformBloodCommon(IN,SoakFrameInfo);
}

DamageVertex_Out VS_TransformDecoration(DamageVertex IN)
{
	return TransformBloodCommon(IN,DecorationFrameInfo);
}

DamageSimpleVertex_Out VS_TransformDecorationNoTex(DamageVertex IN)
{
	DamageSimpleVertex_Out OUT;
	float2 pos = (IN.color0.g==0) ? float2(-1000,-1000) : IN.pos.xy;
	OUT.pos =  mul(float4(pos,0,1), gWorldViewProj);
	return OUT;
}


float4 GetDamageTexture( DamageVertex_Out IN, sampler2D srcTex, float4 frameInfo)
{
	float4 color  = tex2D(srcTex,IN.texCoords.xy);

// #if __XENON
// 	[branch]
// #endif
	if(IN.color0.a != 0.0)
	{
		color = lerp( color, tex2D(srcTex, IN.texCoords.zw), IN.color0.a);
	}

	return color; 
}

float3 GetDamageTintColor(float3 inColor, DamageVertex_Out IN, sampler2D srcTex)
{
	float3 detail = tex2D(srcTex, IN.texCoords.xy).rgb;
	float paletteSelector = DecorationTintPaletteSel.x;
	float paletteSelector2 = DecorationTintPaletteSel.y;

	float3 newDiffuse = tex2D(DecorationTintPaletteTexSampler, float2(detail.g, paletteSelector)).rgb;

	if (paletteSelector2 != paletteSelector)
	{
		float3 secondDiffuse = tex2D(DecorationTintPaletteTexSampler, float2(detail.g, paletteSelector2)).rgb;
		newDiffuse = secondDiffuse * detail.r + newDiffuse * (1.f - detail.r);
	}

	float3 outCol = (DecorationTintPaletteSel.z > 0.0f) ? newDiffuse : inColor;
	return outCol;
}
// blood blit function, decodes blood textures, just picks one frame of splatter and wound
float4 PS_BloodSplatterBlit( DamageVertex_Out IN): COLOR
{
	float2 color = GetDamageTexture(IN, DiffuseSplatterTexSampler, SplatterFrameInfo).rb*IN.color0.rg; 
	return  float4(color.r, 0, 0, color.g);
}

float4 PS_BloodWoundBlit( DamageVertex_Out IN): COLOR
{
	float2 color = GetDamageTexture(IN, DiffuseWoundTexSampler, WoundFrameInfo).rb*IN.color0.rg; 
	return  float4(color.r, 0, 0, color.g);
}

float4 PS_BloodSoakBlit( DamageVertex_Out IN): COLOR
{
	float color = GetDamageTexture(IN, DiffuseTexSampler, SoakFrameInfo).g * IN.color0.r * IN.color0.g; 
	return  float4(0, color, 0, 1) ;   
}


// no sampler border and pre mulitply alpha
float4 PS_DecorationBlit( DamageVertex_Out IN): COLOR
{
	float4 color = GetDamageTexture(IN, DiffuseNoBorderTexSampler, DecorationFrameInfo) * IN.color0.g;
	
	float intensity    = dot(color.rgb,float3(0.299,0.587,0.114));
	float distFromGrey = saturate(dot(abs(color.rgb - intensity.xxx),float3(1,1,1)));   // sum of the delta of each color channel from "grey"
	float alphaAdjust  = (1-(1-distFromGrey)*DecorationTattooAdjust.a*intensity);		  // reduce the adjustment by the tuning value and the intensity (so dark lines are not effected as much as colors)
	
	color.rgba *= float4(DecorationTattooAdjust.rgb,alphaAdjust);

	color.rgb = GetDamageTintColor(color.rgb, IN, DiffuseNoBorderTexSampler);

	return float4(color.rgb, color.a);
}

float4 PS_DecorationNoTex( DamageSimpleVertex_Out IN): COLOR
{
	return float4(1,1,1,1);
}


float4 PS_SkinBumpBlit ( DamageVertex_Out IN): COLOR
{
	float bump =  GetDamageTexture(IN, DiffuseTexSampler, WoundFrameInfo).g * IN.color0.g;
	return bump.xxxx;
}


//------------------------------------------------------------------------------
// Techniques

technique DrawBloodSoak  // just a single frame of soak
{
	pass p0 
	{       
		VertexShader = compile VERTEXSHADER VS_TransformBloodSoak();
		PixelShader  = compile PIXELSHADER PS_BloodSoakBlit() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}

technique DrawBloodSplatter
{
	pass p0 
	{       
		VertexShader = compile VERTEXSHADER VS_TransformBloodSplatter();
		PixelShader  = compile PIXELSHADER PS_BloodSplatterBlit() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}

technique DrawBloodWound  // blend 2 animation frames wound
{
	pass p0 
	{       
		VertexShader = compile VERTEXSHADER VS_TransformBloodWound();
		PixelShader  = compile PIXELSHADER PS_BloodWoundBlit() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}


technique DrawDecoration
{
	pass p0 
	{        
		VertexShader = compile VERTEXSHADER VS_TransformDecoration();
		PixelShader  = compile PIXELSHADER PS_DecorationBlit() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}

technique DrawDecorationNoTex
{
	pass p0 
	{        
		VertexShader = compile VERTEXSHADER VS_TransformDecorationNoTex();
		PixelShader  = compile PIXELSHADER PS_DecorationNoTex() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}

technique DrawSkinBump
{
	pass p0 
	{  
		VertexShader = compile VERTEXSHADER VS_TransformDecoration();
		PixelShader  = compile PIXELSHADER PS_SkinBumpBlit() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}

