
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse texcoord0 normal tangent0
	#define PRAGMA_DCL
#endif
// 
//	grass_fur.fx
//
//	2010/10/14	-	Andrzej:	- initial;
//
//
//
//
//
#define FURGRASS_GS					(0)

// Configure the megashder
#define USE_ANISOTROPIC
#define USE_SPECULAR
#define USE_NORMAL_MAP
#define	NO_SELF_SHADOW
#define USE_WETNESS_MULTIPLIER

#define CUTOUT_SHADER
#define GRASS_SHADER
#define GRASS_FUR_SHADER

#ifdef USE_GRASS_FUR_MASK
	#define GRASS_FUR_MASK			(1)
#else
	#define GRASS_FUR_MASK			(0)
#endif

#ifdef USE_GRASS_FUR_HF_DIFFUSE
	#define GRASS_FUR_HF_DIFFUSE			(GRASS_FUR_MASK)	// UV packing depends on mask functionality
	#define UINAME_SAMPLER_DIFFUSETEXTURE	"HF Diffuse"		// rename 1st diffuse to "HF Diffuse" and 2nd (HF) diffuse to "Area Diffuse" (as per artists' request)
#else
	#define	GRASS_FUR_HF_DIFFUSE			(0)
#endif

#include "../../Megashader/megashader.fxh"

#define UMSCALEG					(0.0)	// temp disable um

#define FUR_MIPMAPLOD_BIAS			1.15
#define	FUR_MIPMAPLOD_BIAS_MASK		-1.0
#define FUR_MIPMAPLOD_BIAS_HF		-1.0
#define FUR_MIPMAPLOD_BIAS_STIPPLE	-1.0

//
//
//
//
//
struct furVertexInputBump
{
	float3 pos			: POSITION;
	float4 diffuse		: COLOR0;
	float2 texCoord0	: TEXCOORD0;
#if GRASS_FUR_MASK
	float2 texCoord1	: TEXCOORD1;
#endif
#if GRASS_FUR_HF_DIFFUSE
	float2 texCoord2	: TEXCOORD2;
#endif
	float3 normal		: NORMAL;
	float4 tangent		: TANGENT0;
};

struct furVertexOutputD
{
    DECLARE_POSITION(pos)
	float4 color0					: COLOR0;	
#if PALETTE_TINT
	float4 color1					: COLOR1;
#endif
	float4 texCoord					: TEXCOORD0;	// uv0, uv1
	float4 furParamsAlphaFade		: TEXCOORD1;	// furParams, alpha fade params
#if REFLECT
	float3 worldEyePos				: TEXCOORD2;
#endif	
	float3 worldPos					: TEXCOORD3;
	float3 worldNormal				: TEXCOORD4;
#if NORMAL_MAP
	float4 worldTangent				: TEXCOORD5;	// .w = uv2.u
	float4 worldBinormal			: TEXCOORD6;	// .w = uv2.v
#endif
#if GRASS_FUR_MASK
	#if GRASS_FUR_HF_DIFFUSE
		float4 texCoord2			: TEXCOORD7;	// uv2, uv3
	#else
		float2 texCoord2			: TEXCOORD7;	// uv2
	#endif
#endif
};

struct furGeometryOutputD
{
    DECLARE_POSITION(pos)
	float4 color0					: COLOR0;	
#if PALETTE_TINT
	float4 color1					: COLOR1;
#endif
	float4 texCoord					: TEXCOORD0;	// uv0, uv1
	float4 furParamsAlphaFade		: TEXCOORD1;	// furParams, alpha fade params
#if REFLECT
	float3 worldEyePos				: TEXCOORD2;
#endif	
	float3 worldPos					: TEXCOORD3;
	float3 worldNormal				: TEXCOORD4;
#if NORMAL_MAP
	float4 worldTangent				: TEXCOORD5;	// .w = uv2.u
	float4 worldBinormal			: TEXCOORD6;	// .w = uv2.v
#endif
#if GRASS_FUR_MASK
	#if GRASS_FUR_HF_DIFFUSE
		float4 texCoord2			: TEXCOORD7;	// uv2, uv3
	#else
		float2 texCoord2			: TEXCOORD7;	// uv2
	#endif
#endif
};


struct furPixelInputD
{
    DECLARE_POSITION_PSIN(pos)
	float4 color0					: COLOR0;	
#if PALETTE_TINT
	float4 color1					: COLOR1;
#endif
	float4 texCoord					: TEXCOORD0;	// uv0, uv1
	float4 furParamsAlphaFade		: TEXCOORD1;	// furParams, alpha fade params
#if REFLECT
	float3 worldEyePos				: TEXCOORD2;
#endif	
	float3 worldPos					: TEXCOORD3;
	float3 worldNormal				: TEXCOORD4;
#if NORMAL_MAP
	float4 worldTangent				: TEXCOORD5;	// .w = uv2.u
	float4 worldBinormal			: TEXCOORD6;	// .w = uv2.v
#endif
#if GRASS_FUR_MASK
	#if GRASS_FUR_HF_DIFFUSE
		float4 texCoord2			: TEXCOORD7;	// uv2, uv3
	#else
		float2 texCoord2			: TEXCOORD7;	// uv2
	#endif
#endif
};


#if __LOW_QUALITY
	#define FUR_MAX_LAYERS				(1)	// lq shaders: 1 layer
#else
	#define FUR_MAX_LAYERS				(8)	// max: 8 layers
#endif

BEGIN_RAGE_CONSTANT_BUFFER(grass_fur_locals,b0)
float4 furLayerParams : furLayerParams
<
	int nostrip	= 1;	// dont strip
	string UIName = "Params: step, umSclX, umSclY, shadowFade";
	float UIMin	= 0;
	float UIMax	= 10;
	float UIStep= 0.0001f;
> = float4(0.0080f, 0.0f, 0.0f, 1.0f);
#define furStep			(furLayerParams.x)	// x=fur step
#define umScaleU		(furLayerParams.y)	// y=um scale for U
#define umScaleV		(furLayerParams.z)	// z=um scale for V
#define furShadowFadeTo	(furLayerParams.w)	// w=shadow fade to value (with distance)

float4 furUvScales : furUvScales
<
	int nostrip	= 1;	// dont strip
	string UIName = "UV Scl: diff, hmask, normal, hf";
	float UIMin	= 0;
	float UIMax	= 1000;
	float UIStep= 0.0001f;
> = float4(1.0f, 1.0f, 1.0f, 1.0f);
#define ScaleUvDiffuse		(furUvScales.x)
#define ScaleUvHeightMap	(furUvScales.y)
#define ScaleUvNormal		(furUvScales.z)
#define ScaleUvHfDiffuse	(furUvScales.w)

float2 furAlphaDistance : furAlphaDistance
<
	int nostrip	= 1;	// dont strip
	string UIName = "Alpha fade dist: min, max";
	float UIMin	= 0;
	float UIMax	= 500;
	float UIStep= 0.0001f;
> = float4(10.0f, 25.0f, 0.0f, 0.0f);
#define furAlphaDistanceMin		(furAlphaDistance.x)
#define furAlphaDistanceMax		(furAlphaDistance.y)
#define furAlphaDistanceUnusedZ	(furAlphaDistance.z)
#define furAlphaDistanceUnusedW	(furAlphaDistance.w)

float4 furAlphaClip03 : furAlphaClip03
<
	int nostrip	= 1;	// dont strip
	string UIName = "Alpha Clip 0-3";
	float UIMin	= 0;
	float UIMax	= 1;
	float UIStep= 0.0001f;
> = float4(0.0f/255.0f, 10.0f/255.0f, 10.0f/255.0f, 15.0f/255.0f);

float4 furAlphaClip47 : furAlphaClip47
<
	int nostrip	= 1;	// dont strip
	string UIName = "Alpha Clip 4-7";
	float UIMin	= 0;
	float UIMax	= 1;
	float UIStep= 0.0001f;
> = float4(20.0f/255.0f, 25.0f/255.0f, 30.0f/255.0f, 34.0f/255.0f);

float4 furShadow03 : furShadow03
<
	int nostrip	= 1;	// dont strip
	string UIName = "Shadow 0-3";
	float UIMin	= 0;
	float UIMax	= 1;
	float UIStep= 0.0001f;
> = float4(90.0f/255.0f, 118.0f/255.0f, 148.0f/255.0f, 168.0f/255.0f);

float4 furShadow47 : furShadow47
<
	int nostrip	= 1;	// dont strip
	string UIName = "Shadow 4-7";
	float UIMin	= 0;
	float UIMax	= 1;
	float UIStep= 0.0001f;
> = float4(200.0f/255.0f, 220.0f/255.0f, 238.0f/255.0f, 255.0f/255.0f);
EndConstantBufferDX10( grass_fur_locals )


#define furLayerShd		(1.0f)			// y=fur layer for "shadowing" (hacked for LOD, etc.) (0.0f)
#define	furAlphaClip	(1.0f/255.0f)	// w=fur alpha clip level (1.0f/255.0f)
#define CameraPos		float3(gViewInverse[3].xyz)


//
//
//
//
float2 VS_CalculateAlphaFade(float alphaClose, float alphaFar)
{
float2 alphaFadeParams;
	const float invAlphaFade = 1.0f / (dot(alphaFar,alphaFar) - dot(alphaClose,alphaClose));
	alphaFadeParams.x = dot(alphaFar,alphaFar)	* invAlphaFade;		// f2  / (f2-c2)
	alphaFadeParams.y = -1.0f					* invAlphaFade;		//-1.0 / (f2-c2)
	return(alphaFadeParams);
}

//
//
//
//
float3 VS_ApplyFurTransform(float3 inPos, float3 inWorldNormal, float furLayer)
{
float3 outPos = inPos;

	outPos += inWorldNormal*furStep*furLayer;
	return(outPos);
}

//
//
//
//
float2 VS_CalculateFurPsParams(int furLayerI, float furLayer, bool shading)
{
float2 outFurParams;

	// furAlphaClipLevel:
	float alphaClip;
	float furShadow;

#if __LOW_QUALITY
	// Min Spec: One layer only:
	{
		alphaClip = furAlphaClip03.x;	// alphatest layer0
		furShadow = furShadow47.x;		// shadow from layer4
	}
#else
	if(furLayerI == 0)
	{
		alphaClip = furAlphaClip03.x;
		furShadow = furShadow03.x;
	}
	else if(furLayerI == 1)
	{
		alphaClip = furAlphaClip03.y;
		furShadow = furShadow03.y;
	}
	else if(furLayerI == 2)
	{
		alphaClip = furAlphaClip03.z;
		furShadow = furShadow03.z;
	}
	else if(furLayerI == 3)
	{
		alphaClip = furAlphaClip03.w;
		furShadow = furShadow03.w;
	}
	else if(furLayerI == 4)
	{
		alphaClip = furAlphaClip47.x;
		furShadow = furShadow47.x;
	}
	else if(furLayerI == 5)
	{
		alphaClip = furAlphaClip47.y;
		furShadow = furShadow47.y;
	}
	else if(furLayerI == 6)
	{
		alphaClip = furAlphaClip47.z;
		furShadow = furShadow47.z;
	}
	else if(furLayerI == 7)
	{
		alphaClip = furAlphaClip47.w;
		furShadow = furShadow47.w;
	}
#endif

	// store fur "shading":
	if(shading)
	{
		//outFurParams.x = furLayerShd * saturate( (furLayer+3)/FUR_MAX_LAYERS );
		outFurParams.x = furShadow;
	}
	else
	{
		outFurParams.x = 1.0f;
	}

	//outFurParams.y  = furAlphaClip * saturate(furLayer/FUR_MAX_LAYERS) * 255.0f;
	outFurParams.y = alphaClip;

	return outFurParams;
}

//
//
//
//
float4 PS_ApplyFur(float4 inSurfaceDiffuse, float2 furParams, float cpvScale, float fadeAlpha, bool furTest)
{
float4 outDiffuse = inSurfaceDiffuse;

	// restore fur "shading":
	float furShadow = furParams.x;
	float furHeight = outDiffuse.a * cpvScale; // fur "height"

	// dilute furShadow to white with distance:
	furShadow = lerp(furShadowFadeTo, furShadow, fadeAlpha);

	furShadow = lerp(furShadowFadeTo, furShadow, cpvScale);

	if(furTest)
	{
	#if __PS3
		// PSN: there's no AlphaTest in MRT mode, so we simulate AlphaTest functionality here:
		float furAlphaClipLevel = furParams.y;
		bool killPix = bool(furHeight < furAlphaClipLevel);
		if(killPix)	discard;
	#endif
	#if __XENON
		// 360: similiar (but more rough) clipping could be achieved with AlphaTest, but clipping here looks nicer:
		float furAlphaClipLevel = furParams.y;
		bool killPix = bool(furHeight < furAlphaClipLevel);
		clip(0-killPix);
	#endif
	#if __SHADERMODEL >= 40
		float furAlphaClipLevel = furParams.y;
		
		// increase furHeight to 1.0 with distance:
//		furAlphaClipLevel = lerp(1.0f, furAlphaClipLevel, sqrt(fadeAlpha));
	
		furAlphaClipLevel = lerp(0.01f, furAlphaClipLevel, saturate(cpvScale*2.0f));

		bool killPix = bool(furHeight < furAlphaClipLevel);
		clip(0-killPix);
	#endif
	}

	// fur "shading":
	outDiffuse.rgb *= furShadow;
	outDiffuse.a	= 1.0f;

	return outDiffuse;
}


//
//
//
//
furVertexOutputD VS_FurTransformD(furVertexInputBump IN, int furLayerI, float furLayer, float2 uvOffset, bool shading)
{
furVertexOutputD OUT;
    
    float3 inPos = IN.pos;
    float3 inNrm = IN.normal;
    float4 inCpv = IN.diffuse;
#if NORMAL_MAP
    float4 inTan = IN.tangent;
#endif


float3 pvlWorldNormal, t_worldEyePos;
float4 pvlColor0;
#if NORMAL_MAP
	float3 outWorldBinormal, outWorldTangent;
#endif
	rtsProcessVertLightingUnskinned( inPos, 
							inNrm, 
#if NORMAL_MAP
							inTan,
#endif
							inCpv, 
#if PALETTE_TINT_EDGE
							IN.specular,
#endif
							(float4x3) gWorld,
							OUT.worldPos,
#if REFLECT					
							OUT.worldEyePos,
#else							
							t_worldEyePos,
#endif							
							pvlWorldNormal,
#if NORMAL_MAP
							outWorldBinormal,
							outWorldTangent,
#endif
							pvlColor0
#if PALETTE_TINT
							,OUT.color1
#endif
							);

#if FURGRASS_GS
    OUT.pos				= float4(inPos,0);
	OUT.worldNormal		= inNrm;
#else
	inPos				= VS_ApplyFurTransform(inPos, inNrm, furLayer);
    OUT.pos				= mul(float4(inPos,1), gWorldViewProj);
	OUT.worldNormal		= pvlWorldNormal;
#endif

	OUT.color0			= pvlColor0;

	// Adjust ambient bakes
#if REFLECT
	OUT.color0.rg = CalculateBakeAdjust(OUT.worldEyePos, OUT.color0.rg);
#else
	OUT.color0.rg = CalculateBakeAdjust(t_worldEyePos, OUT.color0.rg);
#endif

	// umovement:
	float2 umOffset;
	umOffset.x = UMSCALEG * sin(gUmGlobalTimer				) * umScaleU * furLayer/FUR_MAX_LAYERS;
	umOffset.y = UMSCALEG * sin(gUmGlobalTimer+PI*0.3333f	) * umScaleV * furLayer/FUR_MAX_LAYERS;

	OUT.texCoord.xy = (IN.texCoord0.xy-uvOffset+umOffset) * ScaleUvDiffuse;
	OUT.texCoord.zw = (IN.texCoord0.xy-uvOffset+umOffset) * ScaleUvHeightMap;
#if GRASS_FUR_MASK
	OUT.texCoord2.xy= IN.texCoord1.xy;
#endif
#if GRASS_FUR_HF_DIFFUSE
	OUT.texCoord2.zw= IN.texCoord2.xy * ScaleUvHfDiffuse;
#endif

#if NORMAL_MAP
	OUT.worldTangent.xyz	= outWorldTangent;
	OUT.worldBinormal.xyz	= outWorldBinormal;

	float2 texCoordN		= (IN.texCoord0.xy-uvOffset+umOffset) * ScaleUvNormal;
	OUT.worldTangent.w		= texCoordN.x;
	OUT.worldBinormal.w		= texCoordN.y;
#endif

#if FURGRASS_GS
	OUT.furParamsAlphaFade.xy = float2(0,0);	// GS will recalculate this
#else
	OUT.furParamsAlphaFade.xy = VS_CalculateFurPsParams(furLayerI, furLayer, shading);
#endif
	OUT.furParamsAlphaFade.zw = VS_CalculateAlphaFade(furAlphaDistanceMin, furAlphaDistanceMax);

#if 0 && __XENON
	if (globalDeferredAlphaEnable==false)
	{	
		OUT.color0.a = abs(mul(float4(IN.normal,0), gWorldView).z);
	}
#endif


#if RSG_PC && (__SHADERMODEL >= 40)
	// shader quality settings:
	/*
	if(globalShaderQuality >= 2)
	{	// >2: all 8 layers
	}
	else
	*/
	if(globalShaderQuality == 1)
	{	// 1: only 4 layers:
		if(furLayerI > 3)
		{
		    OUT.pos	= float4(0,0,0,0);
		}
	}
	else
	{
	/*
		// 0: only 1 layer - done with LQ shaders
		if(furLayerI > 0)
		{
		    OUT.pos	= float4(0,0,0,0);
		}
	*/
	}
#endif // RSG_PC && (__SHADERMODEL >= 40)...

	return OUT;
}// end of VS_FurTransformD()...

furVertexOutputD VS_FurTransformD_Zero(furVertexInputBump IN)
{
	return VS_FurTransformD(IN, 0, 0.0f, float2(0,0), true);
}

furVertexOutputD VS_FurTransformD_One(furVertexInputBump IN)
{
	return VS_FurTransformD(IN, 1, 1.0f, float2(0,0), true);
}

furVertexOutputD VS_FurTransformD_Two(furVertexInputBump IN)
{
	return VS_FurTransformD(IN, 2, 2.0f, float2(0,0), true);
}

furVertexOutputD VS_FurTransformD_Three(furVertexInputBump IN)
{
	return VS_FurTransformD(IN, 3, 3.0f, float2(0,0), true);
}

furVertexOutputD VS_FurTransformD_Four(furVertexInputBump IN)
{
	return VS_FurTransformD(IN, 4, 4.0f, float2(0,0), true);
}

furVertexOutputD VS_FurTransformD_Five(furVertexInputBump IN)
{
	return VS_FurTransformD(IN, 5, 5.0f, float2(0,0), true);
}

furVertexOutputD VS_FurTransformD_Six(furVertexInputBump IN)
{
	return VS_FurTransformD(IN, 6, 6.0f, float2(0,0), true);
}

furVertexOutputD VS_FurTransformD_Seven(furVertexInputBump IN)
{
	return VS_FurTransformD(IN, 7, 7.0f, float2(0,0), true);
}


#if FURGRASS_GS
// FUR_MAX_LAYERS*3 = 24
[maxvertexcount(24)]
void GS_FurTransformD( triangle furVertexOutputD In[3], inout TriangleStream<furGeometryOutputD> furStream )
{
	
	for( int furLayerI=0; furLayerI<FUR_MAX_LAYERS; furLayerI++ )
    {
        furGeometryOutputD Out;

        for(int v=0; v<3; v++)
        {
			float3 inPos		= In[v].pos.xyz;
			float3 inNrm		= In[v].worldNormal.xyz;

			inPos				= VS_ApplyFurTransform(inPos, inNrm, (float)furLayerI);
		    Out.pos				= mul(float4(inPos,1), gWorldViewProj);

			Out.worldNormal		= NORMALIZE_VS(mul(inNrm, (float3x3)gWorld));

			Out.furParamsAlphaFade.xy	= VS_CalculateFurPsParams(furLayerI, (float)furLayerI, true);
			Out.furParamsAlphaFade.zw	= In[v].furParamsAlphaFade.zw;


			Out.color0				= In[v].color0;
		#if PALETTE_TINT
			Out.color1				= In[v].color1;
		#endif
			Out.texCoord			= In[v].texCoord;
		#if REFLECT
			Out.worldEyePos			= In[v].worldEyePos;
		#endif	
			Out.worldPos			= In[v].worldPos;
		#if NORMAL_MAP
			Out.worldTangent		= In[v].worldTangent;
			Out.worldBinormal		= In[v].worldBinormal;
		#endif
		#if GRASS_FUR_MASK
			#if GRASS_FUR_HF_DIFFUSE
				Out.texCoord2		= In[v].texCoord2;
			#else
				Out.texCoord2		= In[v].texCoord2;
			#endif
		#endif
			
			furStream.Append( Out );
        }

		furStream.RestartStrip();
    }

}//GS_FurTransformD()...

#endif //FURGRASS_GS...

//
//
//
//
BeginSampler(sampler2D,ComboHeightTextureFur01,ComboHeightSamplerFur01,ComboHeightTexFur01)
		int nostrip=1;
		string UIName="FurHeight01";
		string UIHint="FurHeight01";
		string TCPTemplate="Diffuse";
		string TextureType="Height";
ContinueSampler(sampler2D,ComboHeightTextureFur01,ComboHeightSamplerFur01,ComboHeightTexFur01)
		AddressU = WRAP;
		AddressV = WRAP;
		AddressW = WRAP;
		MIPFILTER = LINEAR;
		MINFILTER = LINEAR;
		MAGFILTER = LINEAR; 
		MIPMAPLODBIAS = FUR_MIPMAPLOD_BIAS;
EndSampler;

BeginSampler(sampler2D,ComboHeightTextureFur23,ComboHeightSamplerFur23,ComboHeightTexFur23)
		int nostrip=1;
		string UIName="FurHeight23";
		string UIHint="FurHeight23";
		string TCPTemplate="Diffuse";
		string TextureType="Height";
ContinueSampler(sampler2D,ComboHeightTextureFur23,ComboHeightSamplerFur23,ComboHeightTexFur23)
		AddressU = WRAP;
		AddressV = WRAP;
		AddressW = WRAP;
		MIPFILTER = LINEAR;
		MINFILTER = LINEAR;
		MAGFILTER = LINEAR; 
		MIPMAPLODBIAS = FUR_MIPMAPLOD_BIAS;
EndSampler;

BeginSampler(sampler2D,ComboHeightTextureFur45,ComboHeightSamplerFur45,ComboHeightTexFur45)
		int nostrip=1;
		string UIName="FurHeight45";
		string UIHint="FurHeight45";
		string TCPTemplate="Diffuse";
		string TextureType="Height";
ContinueSampler(sampler2D,ComboHeightTextureFur45,ComboHeightSamplerFur45,ComboHeightTexFur45)
		AddressU = WRAP;
		AddressV = WRAP;
		AddressW = WRAP;
		MIPFILTER = LINEAR;
		MINFILTER = LINEAR;
		MAGFILTER = LINEAR; 
		MIPMAPLODBIAS = FUR_MIPMAPLOD_BIAS;
EndSampler;

BeginSampler(sampler2D,ComboHeightTextureFur67,ComboHeightSamplerFur67,ComboHeightTexFur67)
		int nostrip=1;
		string UIName="FurHeight67";
		string UIHint="FurHeight67";
		string TCPTemplate="Diffuse";
		string TextureType="Height";
ContinueSampler(sampler2D,ComboHeightTextureFur67,ComboHeightSamplerFur67,ComboHeightTexFur67)
		AddressU = WRAP;
		AddressV = WRAP;
		AddressW = WRAP;
		MIPFILTER = LINEAR;
		MINFILTER = LINEAR;
		MAGFILTER = LINEAR; 
		MIPMAPLODBIAS = FUR_MIPMAPLOD_BIAS;
EndSampler;

#if GRASS_FUR_MASK
BeginSampler(sampler2D,FurMaskTex,FurMaskSampler,FurMaskTex)
		int nostrip=1;
		string UIName="FurMask";
		string UIHint="FurMask";
		string TCPTemplate="Diffuse";
		string TextureType="Diffuse";
ContinueSampler(sampler2D,FurMaskTex,FurMaskSampler,FurMaskTex)
		AddressU = WRAP;
		AddressV = WRAP;
		AddressW = WRAP;
		MIPFILTER = LINEAR;
		MINFILTER = LINEAR;
		MAGFILTER = LINEAR; 
		MIPMAPLODBIAS = FUR_MIPMAPLOD_BIAS_MASK;
EndSampler;
#endif //GRASS_FUR_MASK...

#if GRASS_FUR_HF_DIFFUSE
BeginSampler(sampler2D,DiffuseHfTex,DiffuseHfSampler,DiffuseHfTex)
		int nostrip=1;
		string UIName="Area Diffuse";
		string UIHint="Area Diffuse";
		string TCPTemplate="Diffuse";
		string TextureType="Diffuse";
ContinueSampler(sampler2D,DiffuseHfTex,DiffuseHfSampler,DiffuseHfTex)
		AddressU = WRAP;
		AddressV = WRAP;
		AddressW = WRAP;
		MIPFILTER = LINEAR;
		MINFILTER = LINEAR;
		MAGFILTER = LINEAR; 
		MIPMAPLODBIAS = FUR_MIPMAPLOD_BIAS_HF;
EndSampler;
#endif //GRASS_FUR_HF_DIFFUSE...

BeginSampler(sampler2D,StippleTexture,StippleSampler,StippleTexture)
		int nostrip=1;
		string UIName="StippleTexture";
		string UIHint="StippleTexture";
		string TCPTemplate="Diffuse";
		string TextureType="Diffuse";
ContinueSampler(sampler2D,StippleTexture,StippleSampler,StippleTexture)
		AddressU = CLAMP;
		AddressV = CLAMP;
		AddressW = CLAMP;
		MIPFILTER = POINT;
		MINFILTER = POINT;
		MAGFILTER = POINT;
		MIPMAPLODBIAS = FUR_MIPMAPLOD_BIAS_STIPPLE;
EndSampler;

//
//
//
//
float4 AlphaToMaskTex(float fStippleAlpha, float2 vPos)
{
	float fAlpha = saturate(fStippleAlpha) * 0.999f;
//	float fAlpha = saturate(fStippleAlpha-AlphaTest) * 0.999f;	// shift by alpharef to use full range of stippletex
	
// stipple texture is 128x128, contains 32x32 stipple patterns
const float fSubTileSizeInTexels	= 32.0f;
const float fTextureScaleToPixels	= 1.0f/128.0f;


	// get tile location based on alpha value:
float2 fStippleSample;
	fStippleSample.x = floor(fmod(fAlpha, 0.25f) * 16.0f) / 4.0f;
	fStippleSample.y = floor(fAlpha * 4.0f) / 4.0f;

	// get subtile position based on screenpos:
	float2 fStippleSubSample = fmod(vPos, fSubTileSizeInTexels) * fTextureScaleToPixels;

	fStippleSample += fStippleSubSample;

	return tex2D(StippleSampler, fStippleSample.xy);
}

bool AlphaToMask(float fStippleAlpha, float2 vPos)
{
	float fStippleVal = AlphaToMaskTex(fStippleAlpha, vPos).r;
	// kill pixel?
	return (fStippleVal > 0.0f)? false : true;
}


//
//
//
//
DeferredGBuffer PS_FurDeferredTextured0(furPixelInputD IN, bool bFurTest, int furLayer)
{
DeferredGBuffer OUT;

#if 1
	// distance alpha fade:
	float2 furDitherAlphaFadeParams = IN.furParamsAlphaFade.zw;
	float3 dist = CameraPos - IN.worldPos;
	float dist2 = dot(dist,dist);
	float fadeAlpha = saturate(dist2*furDitherAlphaFadeParams.y + furDitherAlphaFadeParams.x);
	#if RSG_PC
		rageDiscard( AlphaToMask(fadeAlpha,IN.pos*33333.333f) );
	#else
		rageDiscard( AlphaToMask(fadeAlpha,IN.pos + fmod(IN.worldPos.xy,16.0f)) );
	#endif
#endif

float2 diffuseUV = IN.texCoord.xy;
float2 diffuseUV2= IN.texCoord.zw;

#if (__SHADERMODEL >= 40)
	// feet collision:
	if(1)
	{
		float3 leftFootV = IN.worldPos.xyz - gPlayerLFootPos.xyz;
		float leftFoot2 = dot(leftFootV, leftFootV);
		leftFoot2 = 1.0f - saturate(leftFoot2*28.0f);

		float3 rightFootV = IN.worldPos.xyz - gPlayerRFootPos.xyz;
		float rightFoot2 = dot(rightFootV, rightFootV);
		rightFoot2 = 1.0f - saturate(rightFoot2*28.0f);

		float offsetL = leftFootV  * leftFoot2  * 0.16f;
		float offsetR = rightFootV * rightFoot2 * 0.16f;
		
		diffuseUV.xy += offsetL;
		diffuseUV.xy += offsetR;
		diffuseUV2.xy += offsetL;
		diffuseUV2.xy += offsetR;
	}
#endif // __SHADERMODEL >= 40...

	SurfaceProperties surfaceInfo = GetSurfaceProperties(
		float4(IN.color0.rgb, 1.0f), 
#if PALETTE_TINT
		IN.color1.rgba,
#endif
#if DIFFUSE_TEXTURE
		diffuseUV,
		DiffuseSampler, 
#endif
#if DIFFUSE2
		IN.texCoord.zw,
		DiffuseSampler2,
#endif	// DIFFUSE2
#if SPEC_MAP
		SPECULAR_PS_INPUT.xy,
	#if SPECULAR_DIFFUSE
		DiffuseSampler,
	#else
		SpecSampler,
	#endif // SPECULAR_DIFFUSE
#endif	// SPEC_MAP
#if REFLECT
		IN.worldEyePos.xyz,
		REFLECT_TEX_SAMPLER,
	#if REFLECT_MIRROR
		float2(0,0),
		float4(0,0,0,1), // not supported
	#endif // REFLECT_MIRROR
#endif	// REFLECT
#if NORMAL_MAP
		float2(IN.worldTangent.w, IN.worldBinormal.w),
		BumpSampler, 
		IN.worldTangent.xyz,
		IN.worldBinormal.xyz,
#endif	// NORMAL_MAP
		IN.worldNormal.xyz
	);		

#if GRASS_FUR_HF_DIFFUSE
	float4 DiffuseHF = tex2D(DiffuseHfSampler, IN.texCoord2.zw);
	surfaceInfo.surface_diffuseColor.rgb *= DiffuseHF.rgb;
#endif

	float2 comboUV = diffuseUV2;
	float4 ComboHeightSample;
	float4 ComboHeightTexMask;
	if(furLayer == 0)
	{
		ComboHeightSample	= tex2D(ComboHeightSamplerFur01, comboUV);
		ComboHeightTexMask	= float4(0,1,0,0);
	}
	else if (furLayer == 1)
	{
		ComboHeightSample	= tex2D(ComboHeightSamplerFur01, comboUV);
		ComboHeightTexMask	= float4(0,0,0,1);
	}
	else if (furLayer == 2)
	{
		ComboHeightSample	= tex2D(ComboHeightSamplerFur23, comboUV);
		ComboHeightTexMask	= float4(0,1,0,0);
	}
	else if (furLayer == 3)
	{
		ComboHeightSample	= tex2D(ComboHeightSamplerFur23, comboUV);
		ComboHeightTexMask	= float4(0,0,0,1);
	}
	else if (furLayer == 4)
	{
		ComboHeightSample	= tex2D(ComboHeightSamplerFur45, comboUV);
		ComboHeightTexMask	= float4(0,1,0,0);
	}
	else if (furLayer == 5)
	{
		ComboHeightSample	= tex2D(ComboHeightSamplerFur45, comboUV);
		ComboHeightTexMask	= float4(0,0,0,1);
	}
	else if (furLayer == 6)
	{
		ComboHeightSample	= tex2D(ComboHeightSamplerFur67, comboUV);
		ComboHeightTexMask	= float4(0,1,0,0);
	}
	else if (furLayer == 7)
	{
		ComboHeightSample	= tex2D(ComboHeightSamplerFur67, comboUV);
		ComboHeightTexMask	= float4(0,0,0,1);
	}
	surfaceInfo.surface_diffuseColor.a = dot(ComboHeightSample.rgba, ComboHeightTexMask.rgba);

	float maskScale = 1.0f;
#if GRASS_FUR_MASK
	maskScale = tex2D(FurMaskSampler, IN.texCoord2.xy).x;
#endif

	surfaceInfo.surface_diffuseColor.rgba = PS_ApplyFur(surfaceInfo.surface_diffuseColor.rgba, IN.furParamsAlphaFade.xy, IN.color0.a*maskScale, fadeAlpha, bFurTest);

	// Compute some surface information.								
	StandardLightingProperties surfaceStandardLightingProperties = DeriveLightingPropertiesForCommonSurface( surfaceInfo );	

#if WETNESS_MULTIPLIER
	surfaceStandardLightingProperties.wetnessMult = wetnessMultiplier * 0.000001 + 0.6f;
#endif

#if REFLECT && !REFLECT_DYNAMIC
	surfaceStandardLightingProperties.diffuseColor.xyz += surfaceStandardLightingProperties.reflectionColor;
#endif	// REFLECT

	// Pack the information into the GBuffer(s).
	OUT = PackGBuffer(
					#ifdef DECAL_USE_NORMAL_MAP_ALPHA
						surfaceInfo.surface_worldNormal.xyzw,
					#else
						surfaceInfo.surface_worldNormal.xyz,
					#endif
						surfaceStandardLightingProperties );

	return OUT;
}// end of PS_FurDeferredTextured0()...


DeferredGBuffer PS_FurDeferredTextured_Zero(furPixelInputD IN)
{
	return PS_FurDeferredTextured0(IN, true, 0);
}

DeferredGBuffer PS_FurDeferredTextured_One(furPixelInputD IN)
{
	return PS_FurDeferredTextured0(IN, true, 1);
}

DeferredGBuffer PS_FurDeferredTextured_Two(furPixelInputD IN)
{
	return PS_FurDeferredTextured0(IN, true, 2);
}

DeferredGBuffer PS_FurDeferredTextured_Three(furPixelInputD IN)
{
	return PS_FurDeferredTextured0(IN, true, 3);
}

DeferredGBuffer PS_FurDeferredTextured_Four(furPixelInputD IN)
{
	return PS_FurDeferredTextured0(IN, true, 4);
}

DeferredGBuffer PS_FurDeferredTextured_Five(furPixelInputD IN)
{
	return PS_FurDeferredTextured0(IN, true, 5);
}

DeferredGBuffer PS_FurDeferredTextured_Six(furPixelInputD IN)
{
	return PS_FurDeferredTextured0(IN, true, 6);
}

DeferredGBuffer PS_FurDeferredTextured_Seven(furPixelInputD IN)
{
	return PS_FurDeferredTextured0(IN, true, 7);
}


#if FURGRASS_GS
	#define FURGRASS_RENDERSTATES()				\
				ZEnable		= true;				\
				ZWriteEnable= true;				\
				ZFunc		= FixedupLESSEQUAL;	\
				SET_COLOR_WRITE_ENABLE(RED+GREEN+BLUE, RED+GREEN+BLUE+ALPHA, RED+GREEN+BLUE+ALPHA, RED+GREEN+BLUE+ALPHA)
#else
	#define FURGRASS_RENDERSTATES()				\
				ZEnable		= true;				\
				ZWriteEnable= false;			\
				ZFunc		= FixedupLESSEQUAL;	\
				SET_COLOR_WRITE_ENABLE(RED+GREEN+BLUE, RED+GREEN+BLUE+ALPHA, RED+GREEN+BLUE+ALPHA, RED+GREEN+BLUE+ALPHA)
#endif

//
//
//
//
#if DEFERRED_TECHNIQUES && NON_CUTOUT_DEFERRED_TECHNIQUES
technique deferred_draw
{
#if FURGRASS_GS
	pass p0 
	{
		FURGRASS_RENDERSTATES()
		VertexShader = compile VERTEXSHADER	VS_FurTransformD_Zero();
		SetGeometryShader(compileshader(gs_5_0, GS_FurTransformD()));
		PixelShader  = compile PIXELSHADER	PS_FurDeferredTextured_Zero()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
#else //FURGRASS_GS
	pass p0 
	{
		FURGRASS_RENDERSTATES()
		VertexShader = compile VERTEXSHADER	VS_FurTransformD_Zero();
		PixelShader  = compile PIXELSHADER	PS_FurDeferredTextured_Zero()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

#if FUR_MAX_LAYERS > 1
	pass p1
	{
		FURGRASS_RENDERSTATES()
		VertexShader = compile VERTEXSHADER	VS_FurTransformD_One();
		PixelShader  = compile PIXELSHADER	PS_FurDeferredTextured_One()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
#endif

#if FUR_MAX_LAYERS > 2
	pass p2
	{
		FURGRASS_RENDERSTATES()
		VertexShader = compile VERTEXSHADER	VS_FurTransformD_Two();
		PixelShader  = compile PIXELSHADER	PS_FurDeferredTextured_Two()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
#endif

#if FUR_MAX_LAYERS > 3
	pass p3
	{
		FURGRASS_RENDERSTATES()
		VertexShader = compile VERTEXSHADER	VS_FurTransformD_Three();
		PixelShader  = compile PIXELSHADER	PS_FurDeferredTextured_Three()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
#endif

#if FUR_MAX_LAYERS > 4
	pass p4
	{
		FURGRASS_RENDERSTATES()
		VertexShader = compile VERTEXSHADER	VS_FurTransformD_Four();
		PixelShader  = compile PIXELSHADER	PS_FurDeferredTextured_Four()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
#endif

#if FUR_MAX_LAYERS > 5
	pass p5
	{
		FURGRASS_RENDERSTATES()
		VertexShader = compile VERTEXSHADER	VS_FurTransformD_Five();
		PixelShader  = compile PIXELSHADER	PS_FurDeferredTextured_Five()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
#endif

#if FUR_MAX_LAYERS > 6
	pass p6
	{
		FURGRASS_RENDERSTATES()
		VertexShader = compile VERTEXSHADER	VS_FurTransformD_Six();
		PixelShader  = compile PIXELSHADER	PS_FurDeferredTextured_Six()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
#endif

#if FUR_MAX_LAYERS > 7
	pass p7
	{
		FURGRASS_RENDERSTATES()
		VertexShader = compile VERTEXSHADER	VS_FurTransformD_Seven();
		PixelShader  = compile PIXELSHADER	PS_FurDeferredTextured_Seven()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
#endif
#endif//FURGRASS_GS...
}
#endif // DEFERRED_TECHNIQUES && NON_CUTOUT_DEFERRED_TECHNIQUES

#if DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES
technique deferredalphaclip_draw
{
#if FURGRASS_GS
	pass p0 
	{
		FURGRASS_RENDERSTATES()
		VertexShader = compile VERTEXSHADER	VS_FurTransformD_Zero();
		SetGeometryShader(compileshader(gs_5_0, GS_FurTransformD()));
		PixelShader  = compile PIXELSHADER	PS_FurDeferredTextured_Zero()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
#else //FURGRASS_GS
	pass p0 
	{
		FURGRASS_RENDERSTATES()
		VertexShader = compile VERTEXSHADER	VS_FurTransformD_Zero();
		PixelShader  = compile PIXELSHADER	PS_FurDeferredTextured_Zero()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

#if FUR_MAX_LAYERS > 1
	pass p1
	{
		FURGRASS_RENDERSTATES()
		VertexShader = compile VERTEXSHADER	VS_FurTransformD_One();
		PixelShader  = compile PIXELSHADER	PS_FurDeferredTextured_One()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
#endif

#if FUR_MAX_LAYERS > 2
	pass p2
	{
		FURGRASS_RENDERSTATES()
		VertexShader = compile VERTEXSHADER	VS_FurTransformD_Two();
		PixelShader  = compile PIXELSHADER	PS_FurDeferredTextured_Two()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
#endif

#if FUR_MAX_LAYERS > 3
	pass p3
	{
		FURGRASS_RENDERSTATES()
		VertexShader = compile VERTEXSHADER	VS_FurTransformD_Three();
		PixelShader  = compile PIXELSHADER	PS_FurDeferredTextured_Three()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
#endif

#if FUR_MAX_LAYERS > 4
	pass p4
	{
		FURGRASS_RENDERSTATES()
		VertexShader = compile VERTEXSHADER	VS_FurTransformD_Four();
		PixelShader  = compile PIXELSHADER	PS_FurDeferredTextured_Four()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
#endif

#if FUR_MAX_LAYERS > 5
	pass p5
	{
		FURGRASS_RENDERSTATES()
		VertexShader = compile VERTEXSHADER	VS_FurTransformD_Five();
		PixelShader  = compile PIXELSHADER	PS_FurDeferredTextured_Five()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
#endif

#if FUR_MAX_LAYERS > 6
	pass p6
	{
		FURGRASS_RENDERSTATES()
		VertexShader = compile VERTEXSHADER	VS_FurTransformD_Six();
		PixelShader  = compile PIXELSHADER	PS_FurDeferredTextured_Six()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
#endif

#if FUR_MAX_LAYERS > 7
	pass p7
	{
		FURGRASS_RENDERSTATES()
		VertexShader = compile VERTEXSHADER	VS_FurTransformD_Seven();
		PixelShader  = compile PIXELSHADER	PS_FurDeferredTextured_Seven()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
#endif
#endif//FURGRASS_GS...
}
#endif // DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES

#if DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES
technique deferredsubsamplealpha_draw
{
#if FURGRASS_GS
	pass p0 
	{
		FURGRASS_RENDERSTATES()
		VertexShader = compile VERTEXSHADER	VS_FurTransformD_Zero();
		SetGeometryShader(compileshader(gs_5_0, GS_FurTransformD()));
		PixelShader  = compile PIXELSHADER	PS_FurDeferredTextured_Zero()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
#else //FURGRASS_GS
	pass p0 
	{
		FURGRASS_RENDERSTATES()
		VertexShader = compile VERTEXSHADER	VS_FurTransformD_Zero();
		PixelShader  = compile PIXELSHADER	PS_FurDeferredTextured_Zero()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

#if FUR_MAX_LAYERS > 1
	pass p1
	{
		FURGRASS_RENDERSTATES()
		VertexShader = compile VERTEXSHADER	VS_FurTransformD_One();
		PixelShader  = compile PIXELSHADER	PS_FurDeferredTextured_One()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
#endif

#if FUR_MAX_LAYERS > 2
	pass p2
	{
		FURGRASS_RENDERSTATES()
		VertexShader = compile VERTEXSHADER	VS_FurTransformD_Two();
		PixelShader  = compile PIXELSHADER	PS_FurDeferredTextured_Two()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
#endif

#if FUR_MAX_LAYERS > 3
	pass p3
	{
		FURGRASS_RENDERSTATES()
		VertexShader = compile VERTEXSHADER	VS_FurTransformD_Three();
		PixelShader  = compile PIXELSHADER	PS_FurDeferredTextured_Three()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
#endif

#if FUR_MAX_LAYERS > 4
	pass p4
	{
		FURGRASS_RENDERSTATES()
		VertexShader = compile VERTEXSHADER	VS_FurTransformD_Four();
		PixelShader  = compile PIXELSHADER	PS_FurDeferredTextured_Four()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
#endif

#if FUR_MAX_LAYERS > 5
	pass p5
	{
		FURGRASS_RENDERSTATES()
		VertexShader = compile VERTEXSHADER	VS_FurTransformD_Five();
		PixelShader  = compile PIXELSHADER	PS_FurDeferredTextured_Five()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
#endif

#if FUR_MAX_LAYERS > 6
	pass p6
	{
		FURGRASS_RENDERSTATES()
		VertexShader = compile VERTEXSHADER	VS_FurTransformD_Six();
		PixelShader  = compile PIXELSHADER	PS_FurDeferredTextured_Six()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
#endif

#if FUR_MAX_LAYERS > 7
	pass p7
	{
		FURGRASS_RENDERSTATES()
		VertexShader = compile VERTEXSHADER	VS_FurTransformD_Seven();
		PixelShader  = compile PIXELSHADER	PS_FurDeferredTextured_Seven()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
#endif
#endif//FURGRASS_GS...
}
#endif // DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES


SHADERTECHNIQUE_ENTITY_SELECT(VS_FurTransformD_Zero(), VS_FurTransformD_Zero())

