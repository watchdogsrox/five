
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse texcoord0 normal tangent0
	#define PRAGMA_DCL
#endif
// 
//	grass_fur_lod.fx
//
//	2010/10/14	-	Andrzej:	- initial;
//
//
//
//
//
// Configure the megashder
#define USE_ANISOTROPIC
#define USE_SPECULAR
#define IGNORE_SPECULAR_MAP
#define USE_NORMAL_MAP
#define NO_SELF_SHADOW

#define CUTOUT_SHADER
#define GRASS_SHADER

#include "../../Megashader/megashader.fxh"

#define FUR_MIPMAPLOD_BIAS			-1.0

#define USE_ALPHA_STIPPLE			(1)

//
//
//
//
//
struct furgrassVertexInput
{
    float3 pos			: POSITION;
	float4 diffuse		: COLOR0;
	float2 texCoord0	: TEXCOORD0;
    float3 normal		: NORMAL;
};

struct furVertexOutputD
{
    DECLARE_POSITION(pos)
	float4 color0					: COLOR0;	
#if PALETTE_TINT
	float4 color1					: COLOR1;
#endif
	float4 texCoord					: TEXCOORD0;	// uv, furParams
	float2 texCoord2				: TEXCOORD1;	// uv2
	float3 worldPos					: TEXCOORD2;
#if REFLECT
	float3 worldEyePos				: TEXCOORD3;
#endif	
	float3 worldNormal				: TEXCOORD4;
#if NORMAL_MAP
	float4 worldTangent				: TEXCOORD5;		// .w = U scale N
	float4 worldBinormal			: TEXCOORD6;		// .w = V scale N
#endif
	float4 worldPlayerLFootPos		: TEXCOORD7;
	float4 worldPlayerRFootPos		: TEXCOORD8;
};

struct furPixelInputD
{
    DECLARE_POSITION_PSIN(pos)
	float4 color0					: COLOR0;	
#if PALETTE_TINT
	float4 color1					: COLOR1;
#endif
	float4 texCoord					: TEXCOORD0;	// uv, furParams
	float2 texCoord2				: TEXCOORD1;	// uv2
	float3 worldPos					: TEXCOORD2;
#if REFLECT
	float3 worldEyePos				: TEXCOORD3;
#endif	
	float3 worldNormal				: TEXCOORD4;
#if NORMAL_MAP
	float4 worldTangent				: TEXCOORD5;		// .w = U scale N
	float4 worldBinormal			: TEXCOORD6;		// .w = V scale N
#endif
	float4 worldPlayerLFootPos		: TEXCOORD7;
	float4 worldPlayerRFootPos		: TEXCOORD8;
};

#define FUR_MAX_LAYERS				(8)

BEGIN_RAGE_CONSTANT_BUFFER(grass_fur_lod_locals,b0)
float4 furLayerParams : furLayerParams
<
	int nostrip		= 1;	// dont strip
> = float4(1.0f, 0.0100f, 1.0f/255.0f, 0.0f);
#define furLayerShd				(furLayerParams.x)	// x=fur layer for "shadowing" (hacked for LOD, etc.) (0.0f)
#define furStep					(furLayerParams.y)	// y=fur step (0.0005f)
#define	furAlphaClip			(furLayerParams.z)	// z=fur alpha clip level (1.0f/255.0f)
#define furLayerParamUnusedW	(furLayerParams.w)	// free

float4 furLayerParams2 : furLayerParams2
<
	int nostrip		= 1;	// dont strip
> = float4(0.0f, 0.0f, 0.00250f, 0.00125f);
#define furOffsetUV				(furLayerParams2.xy)	// xy=UV offset
#define furUmScaleXY			(furLayerParams2.zw)	// zw=um scale XY 

float4 furLayerParams3 : furLayerParams3
<
	int nostrip		= 1;	// dont strip
> = float4(1.0f, 1.0f, 1.0f, 0.0f);
#define	furUVScale				(furLayerParams3.x)	// x=UV scale (diffuse)
#define	furUVScale2				(furLayerParams3.y)	// y=UV scale (layer mask)
#define	furUVScale3				(furLayerParams3.z)	// y=UV scale (normal)
#define furLayerParam3UnusedW	(furLayerParams3.w)	// free


//float4 furUmFadeParams : furUmFadeParams = float4(0,0,0,0);
//#define umFadeParams		float2(furUmFadeParams.zw)

float4 playerLFootPos : playerLFootPos
<
	int nostrip		= 1;	// dont strip
> = float4(0,0,0,1);

float4 playerRFootPos : playerRFootPos
<
	int nostrip		= 1;	// dont strip
> = float4(0,0,0,1);

// heightmap mask to extract proper field out of full texture:
float4 ComboHeightTexMask : ComboHeightTexMask = float4(1,0,0,0);

float4 furDitherAlphaFadeParams : furDitherAlphaFadeParams = float4(0,0,0,0);

EndConstantBufferDX10( grass_fur_lod_locals )

#define CameraPos			float3(gViewInverse[3].xyz)


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
float2 VS_CalculateFurPsParams(float furLayer)
{
float2 outFurParams;

	// store fur "shading":
	outFurParams.x = furLayerShd;

	// furAlphaClipLevel:
	//outFurParams.y  = furAlphaClip * saturate(furLayer/FUR_MAX_LAYERS) * 255.0f;
	outFurParams.y  = furAlphaClip;

	return outFurParams;
}

//
//
//
//
float4 PS_ApplyFur(float4 inSurfaceDiffuse, float2 furParams, bool furTest)
{
float4 outDiffuse = inSurfaceDiffuse;

	// restore fur "shading":
	float furShadow = furParams.x;

	float furHeight = outDiffuse.a;	// fur "height"

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
		float killPix = step(furHeight,furAlphaClipLevel);
		clip(0-killPix);
	#endif
	#if __SHADERMODEL >= 40
		float furAlphaClipLevel = furParams.y;
		bool killPix = bool(furAlphaClipLevel > furHeight);
		clip(0-killPix);
	#endif
	}

	// fur "shading":
	outDiffuse.rgb *= furShadow;

	return outDiffuse;
}


//
//
//
//
furVertexOutputD VS_FurTriTransformD(furgrassVertexInput IN, float furLayer, float3 uvTexScales, float2 uvOffset, float2 umScale, bool umCameraDistControl)
{
furVertexOutputD OUT;

    float3 inPos = IN.pos;
    float3 inNrm = IN.normal;
    float4 inCpv = float4(IN.diffuse.rgb,1.0f);
#if NORMAL_MAP
    float4 inTan = float4(IN.texCoord0.xyx,0);	// fake tangent - not available in IM
#endif
	float uvScale	= uvTexScales.x;
	float uvScale2	= uvTexScales.y;
	float uvScaleN	= uvTexScales.z;

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
#endif // NORMAL_MAP
							pvlColor0
#if PALETTE_TINT
							,OUT.color1
#endif
							);

	inPos = VS_ApplyFurTransform(inPos, inNrm, furLayer);

	// Write out final position & texture coords
    OUT.pos = mul(float4(inPos,1), gWorldViewProj);
	OUT.color0 = pvlColor0;

	OUT.worldNormal		= pvlWorldNormal;

	// cam dist fade for um:
float distScale = 1.0f;
/*
	if(umCameraDistControl)
	{
		float3 worldPos = mul(float4(inPos,1), gWorld); 
		float3 dist = CameraPos - worldPos;
		float dist2 = dot(dist,dist);
		distScale = saturate(dist2*umFadeParams.y + umFadeParams.x);
	}
*/
	// umovement:
	float2 umOffset;
	umOffset.x = sin(gUmGlobalTimer				) * umScale.x * furLayer/FUR_MAX_LAYERS * distScale;
	umOffset.y = sin(gUmGlobalTimer+PI*0.3333f	) * umScale.y * furLayer/FUR_MAX_LAYERS * distScale;

	OUT.texCoord.xy = (IN.texCoord0.xy-uvOffset+umOffset) * uvScale;
	OUT.texCoord.zw = VS_CalculateFurPsParams(furLayer);

	OUT.texCoord2.xy= (IN.texCoord0.xy-uvOffset+umOffset) * uvScale2;

#if NORMAL_MAP
	OUT.worldTangent.xyz	= outWorldTangent;
	OUT.worldBinormal.xyz	= outWorldBinormal;

	float2 texCoordN		= (IN.texCoord0.xy-uvOffset+umOffset) * uvScaleN;
	OUT.worldTangent.w		= texCoordN.x;
	OUT.worldBinormal.w		= texCoordN.y;
#endif

	OUT.worldPlayerLFootPos = playerLFootPos;
	OUT.worldPlayerRFootPos = playerRFootPos;

	return OUT;
}// end of VS_FurTriTransformD()...


#define UV_SCALES				float3(furUVScale, furUVScale2, furUVScale3)
#define UV_OFFSET				(furOffsetUV)
#define UM_SCALE				(furUmScaleXY)
#define CAM_UM_DIST_CONTROL		false
furVertexOutputD VS_FurTriGrassD_One(furgrassVertexInput IN)
{
	return VS_FurTriTransformD(IN, 1.0f, UV_SCALES, UV_OFFSET, UM_SCALE, CAM_UM_DIST_CONTROL);
}

furVertexOutputD VS_FurTriGrassD_Two(furgrassVertexInput IN)
{
	return VS_FurTriTransformD(IN, 2.0f, UV_SCALES, UV_OFFSET, UM_SCALE, CAM_UM_DIST_CONTROL);
}

furVertexOutputD VS_FurTriGrassD_Three(furgrassVertexInput IN)
{
	return VS_FurTriTransformD(IN, 3.0f, UV_SCALES, UV_OFFSET, UM_SCALE, CAM_UM_DIST_CONTROL);
}

furVertexOutputD VS_FurTriGrassD_Four(furgrassVertexInput IN)
{
	return VS_FurTriTransformD(IN, 4.0f, UV_SCALES, UV_OFFSET, UM_SCALE, CAM_UM_DIST_CONTROL);
}

furVertexOutputD VS_FurTriGrassD_Five(furgrassVertexInput IN)
{
	return VS_FurTriTransformD(IN, 5.0f, UV_SCALES, UV_OFFSET, UM_SCALE, CAM_UM_DIST_CONTROL);
}

furVertexOutputD VS_FurTriGrassD_Six(furgrassVertexInput IN)
{
	return VS_FurTriTransformD(IN, 6.0f, UV_SCALES, UV_OFFSET, UM_SCALE, CAM_UM_DIST_CONTROL);
}

furVertexOutputD VS_FurTriGrassD_Seven(furgrassVertexInput IN)
{
	return VS_FurTriTransformD(IN, 7.0f, UV_SCALES, UV_OFFSET, UM_SCALE, CAM_UM_DIST_CONTROL);
}

furVertexOutputD VS_FurTriGrassD_Eight(furgrassVertexInput IN)
{
	return VS_FurTriTransformD(IN, 8.0f, UV_SCALES, UV_OFFSET, UM_SCALE, CAM_UM_DIST_CONTROL);
}


BeginSampler(sampler2D,diffuseTextureFur,DiffuseSamplerFur,DiffuseTexFur)
		int nostrip=1;
		string UIName="Fur Diffuse";
		string UIHint="Fur Diffuse";
		string TCPTemplate="Diffuse";
		string TextureType="Diffuse";
ContinueSampler(sampler2D,diffuseTextureFur,DiffuseSamplerFur,DiffuseTexFur)
		AddressU = WRAP;
		AddressV = WRAP;
		AddressW = WRAP;
		MIPFILTER = LINEAR;
		MINFILTER = LINEAR;
		MAGFILTER = LINEAR; 
		MIPMAPLODBIAS = FUR_MIPMAPLOD_BIAS;
EndSampler;

BeginSampler(sampler2D,ComboHeightTextureFur,ComboHeightSamplerFur,ComboHeightTexFur)
		int nostrip=1;
		string UIName="FurHeight01";
		string UIHint="FurHeight01";
		string TCPTemplate="Diffuse";
		string TextureType="Height";
ContinueSampler(sampler2D,ComboHeightTextureFur,ComboHeightSamplerFur,ComboHeightTexFur)
		AddressU = WRAP;
		AddressV = WRAP;
		AddressW = WRAP;
		MIPFILTER = LINEAR;
		MINFILTER = LINEAR;
		MAGFILTER = LINEAR; 
		MIPMAPLODBIAS = FUR_MIPMAPLOD_BIAS;
EndSampler;

// fake sampler to pick textures from underlying object
BeginSampler(sampler2D,ComboHeightTextureFur2,ComboHeightSamplerFur2,ComboHeightTexFur2)
		int nostrip=1;
		string UIName="FurHeight23";
		string UIHint="FurHeight23";
		string TCPTemplate="Diffuse";
	string	TextureType="Height";
ContinueSampler(sampler2D,ComboHeightTextureFur2,ComboHeightSamplerFur2,ComboHeightTexFur2)
		AddressU = WRAP;
		AddressV = WRAP;
		AddressW = WRAP;
		MIPFILTER = LINEAR;
		MINFILTER = LINEAR;
		MAGFILTER = LINEAR; 
		MIPMAPLODBIAS = FUR_MIPMAPLOD_BIAS;
EndSampler;

BeginSampler(sampler2D,ComboHeightTextureFur3,ComboHeightSamplerFur3,ComboHeightTexFur3)
		int nostrip=1;
		string UIName="FurHeight45";
		string UIHint="FurHeight45";
		string TCPTemplate="Diffuse";
	string	TextureType="Height";
ContinueSampler(sampler2D,ComboHeightTextureFur3,ComboHeightSamplerFur3,ComboHeightTexFur3)
		AddressU = WRAP;
		AddressV = WRAP;
		AddressW = WRAP;
		MIPFILTER = LINEAR;
		MINFILTER = LINEAR;
		MAGFILTER = LINEAR; 
		MIPMAPLODBIAS = FUR_MIPMAPLOD_BIAS;
EndSampler;

BeginSampler(sampler2D,ComboHeightTextureFur4,ComboHeightSamplerFur4,ComboHeightTexFur4)
		int nostrip=1;
		string UIName="FurHeight67";
		string UIHint="FurHeight67";
		string TCPTemplate="Diffuse";
	string	TextureType="Height";
ContinueSampler(sampler2D,ComboHeightTextureFur4,ComboHeightSamplerFur4,ComboHeightTexFur4)
		AddressU = WRAP;
		AddressV = WRAP;
		AddressW = WRAP;
		MIPFILTER = LINEAR;
		MINFILTER = LINEAR;
		MAGFILTER = LINEAR; 
		MIPMAPLODBIAS = FUR_MIPMAPLOD_BIAS;
EndSampler;

#if NORMAL_MAP
	BeginSampler(sampler2D,BumpTextureFur,BumpSamplerFur,BumpTexFur)
		string UIName="Fur Bump Texture";
		string UIHint="normalmap";
		string TCPTemplate="NormalMap";
	string	TextureType="Bump";
	ContinueSampler(sampler2D,BumpTextureFur,BumpSamplerFur,BumpTexFur)
		AddressU  = WRAP;        
		AddressV  = WRAP;
		AddressW  = WRAP;
		#ifdef USE_ANISOTROPIC
			MINFILTER		= MINANISOTROPIC;
			MAGFILTER		= MAGLINEAR; 
			ANISOTROPIC_LEVEL(4)
		#else 
			MINFILTER		= LINEAR;
			MAGFILTER		= LINEAR; 
		#endif
		MIPFILTER = MIPLINEAR;
		MIPMAPLODBIAS = FUR_MIPMAPLOD_BIAS;
	EndSampler;
#endif	// NORMAL_MAP

#if USE_ALPHA_STIPPLE
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
		MIPMAPLODBIAS = FUR_MIPMAPLOD_BIAS;
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
#endif

//
//
//
//
DeferredGBuffer PS_FurDeferredTextured(furPixelInputD IN, bool bFurTest, bool bComboHeight)
{
DeferredGBuffer OUT;

#if USE_ALPHA_STIPPLE
	// distance alpha fade:
	float3 dist = CameraPos - IN.worldPos;
	float dist2 = dot(dist,dist);
	float fadeAlpha = saturate(dist2*furDitherAlphaFadeParams.y + furDitherAlphaFadeParams.x);
	rageDiscard( AlphaToMask(fadeAlpha,IN.pos.xy) );
#endif

float2 diffuseUV = IN.texCoord.xy;
float2 diffuseUV2= IN.texCoord2.xy;

	// feet collision:
	if(1)
	{
		float3 leftFootV = IN.worldPos.xyz - IN.worldPlayerLFootPos.xyz;
		float leftFoot2 = dot(leftFootV, leftFootV);
		leftFoot2 = 1.0f - saturate(leftFoot2*28.0f);

		float3 rightFootV = IN.worldPos.xyz - IN.worldPlayerRFootPos.xyz;
		float rightFoot2 = dot(rightFootV, rightFootV);
		rightFoot2 = 1.0f - saturate(rightFoot2*28.0f);

		float offsetL = leftFootV  * leftFoot2  * 0.16f;
		float offsetR = rightFootV * rightFoot2 * 0.16f;
		
		diffuseUV.xy += offsetL;
		diffuseUV.xy += offsetR;
		diffuseUV2.xy += offsetL;
		diffuseUV2.xy += offsetR;
	}

	SurfaceProperties surfaceInfo = GetSurfaceProperties(
		IN.color0.rgba, 
#if PALETTE_TINT
		IN.color1.rgba,
#endif
#if DIFFUSE_TEXTURE
		diffuseUV.xy,
		DiffuseSamplerFur, 
#endif
#if DIFFUSE2
		IN.texCoord.zw,
		DiffuseSampler2,
#endif	// DIFFUSE2
#if SPEC_MAP
		SPECULAR_PS_INPUT.xy,
		SpecSampler,
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
		BumpSamplerFur, 
		IN.worldTangent.xyz,
		IN.worldBinormal.xyz,
#endif	// NORMAL_MAP
		IN.worldNormal.xyz
	);		

	if(bComboHeight)
	{
		surfaceInfo.surface_diffuseColor.a = dot(tex2D(ComboHeightSamplerFur, diffuseUV2.xy).rgba, ComboHeightTexMask.rgba);
	}

	surfaceInfo.surface_diffuseColor.rgba = PS_ApplyFur(surfaceInfo.surface_diffuseColor.rgba, IN.texCoord.zw, bFurTest);

	// Compute some surface information.
	StandardLightingProperties surfaceStandardLightingProperties = DeriveLightingPropertiesForCommonSurface( surfaceInfo );	

	surfaceStandardLightingProperties.naturalAmbientScale	= 1.0f;
	surfaceStandardLightingProperties.artificialAmbientScale= 0.0f;

	// ground tint:
	surfaceStandardLightingProperties.diffuseColor.rgb *= surfaceInfo.surface_baseColor.rgb;

#if REFLECT && !REFLECT_DYNAMIC
	surfaceStandardLightingProperties.diffuseColor.xyz += surfaceStandardLightingProperties.reflectionColor;
#endif	// REFLECT

#if !defined(SHADER_FINAL)
	// feet collision debug:
	if(0)
	{
		float3 leftFootV = IN.worldPos.xyz - IN.worldPlayerLFootPos.xyz;
		float leftFoot2 = dot(leftFootV, leftFootV);
		leftFoot2 = 1.0f - saturate(leftFoot2*20.0f);

		float3 rightFootV = IN.worldPos.xyz - IN.worldPlayerRFootPos.xyz;
		float rightFoot2 = dot(rightFootV, rightFootV);
		rightFoot2 = 1.0f - saturate(rightFoot2*20.0f);

		surfaceStandardLightingProperties.diffuseColor.rgb = leftFoot2+rightFoot2;
	}
#endif // !defined(SHADER_FINAL)

	// Pack the information into the GBuffer(s).
	OUT = PackGBuffer(
					#ifdef DECAL_USE_NORMAL_MAP_ALPHA
						surfaceInfo.surface_worldNormal.xyzw,
					#else
						surfaceInfo.surface_worldNormal.xyz,
					#endif
						surfaceStandardLightingProperties );

	return OUT;
}// end of PS_FurDeferredTextured()...


DeferredGBuffer PS_FurTriGrassDeferredTexturedCombo(furPixelInputD IN)
{
	return PS_FurDeferredTextured(IN, true, true);
}

//
//
// fur grass LOD v4: LocTri plane rendering:
//
technique deferred_furgrasstriCombo
{
	pass p0 
	{
		VertexShader = compile VERTEXSHADER	VS_FurTriGrassD_One();
		PixelShader  = compile PIXELSHADER	PS_FurTriGrassDeferredTexturedCombo()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

#if FUR_MAX_LAYERS > 1
	pass p1
	{
		VertexShader = compile VERTEXSHADER	VS_FurTriGrassD_Two();
		PixelShader  = compile PIXELSHADER	PS_FurTriGrassDeferredTexturedCombo()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
#endif

#if FUR_MAX_LAYERS > 2
	pass p2
	{
		VertexShader = compile VERTEXSHADER	VS_FurTriGrassD_Three();
		PixelShader  = compile PIXELSHADER	PS_FurTriGrassDeferredTexturedCombo()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
#endif

#if FUR_MAX_LAYERS > 3
	pass p3
	{
		VertexShader = compile VERTEXSHADER	VS_FurTriGrassD_Four();
		PixelShader  = compile PIXELSHADER	PS_FurTriGrassDeferredTexturedCombo()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
#endif

#if FUR_MAX_LAYERS > 4
	pass p4
	{
		VertexShader = compile VERTEXSHADER	VS_FurTriGrassD_Five();
		PixelShader  = compile PIXELSHADER	PS_FurTriGrassDeferredTexturedCombo()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
#endif

#if FUR_MAX_LAYERS > 5
	pass p5
	{
		VertexShader = compile VERTEXSHADER	VS_FurTriGrassD_Six();
		PixelShader  = compile PIXELSHADER	PS_FurTriGrassDeferredTexturedCombo()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
#endif

#if FUR_MAX_LAYERS > 6
	pass p6
	{
		VertexShader = compile VERTEXSHADER	VS_FurTriGrassD_Seven();
		PixelShader  = compile PIXELSHADER	PS_FurTriGrassDeferredTexturedCombo()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
#endif

#if FUR_MAX_LAYERS > 7
	pass p7
	{
		VertexShader = compile VERTEXSHADER	VS_FurTriGrassD_Eight();
		PixelShader  = compile PIXELSHADER	PS_FurTriGrassDeferredTexturedCombo()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
#endif
}

struct furDitheredOutput
{
    DECLARE_POSITION(pos)
	float3 worldPos					: TEXCOORD0;
};


//
//
//
//
furDitheredOutput VS_FurDithered(northVertexInputBump IN)
{
furDitheredOutput OUT;
 
	float3 inPos	= IN.pos;
	OUT.pos			= mul(float4(inPos,1), gWorldViewProj);
	OUT.worldPos	= mul(float4(inPos,1), gWorld);

	return OUT;
}// end of VS_FurDithered()...

//
//
//
//
float4 PS_FurDithered(furDitheredOutput IN) : COLOR
{
float4 inColor = float4(1,1,1,1);

#if USE_ALPHA_STIPPLE
	// do nothing
#else
	// distance alpha fade:
	float3 dist = CameraPos - IN.worldPos;
	float dist2 = dot(dist,dist);
	inColor.a = saturate(dist2*furDitherAlphaFadeParams.y + furDitherAlphaFadeParams.x);
#endif
	return(inColor);
}


// fur grass: dithered alpha pre-pass:
technique furgrasstri_dithered
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER	VS_FurDithered();
		PixelShader  = compile PIXELSHADER	PS_FurDithered()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}



//
//
// fur grass ground: default rendering only (the one used for picking up fur params)
//
furVertexOutputD VS_FurGroundTransformD(northVertexInputBump IN)
{
furVertexOutputD OUT=(furVertexOutputD)0;

	float3 inPos = IN.pos;
	float3 inNrm = IN.normal;
	float4 inCpv = IN.diffuse;
#if NORMAL_MAP
	float4 inTan = IN.tangent;
#endif


float3 pvlWorldNormal, pvlWorldPosition, t_worldEyePos;
float4 pvlColor0;
#if NORMAL_MAP
float3 outWorldBinormal, outWorldTangent;
#endif

	rtsProcessVertLightingUnskinned(	inPos,
										inNrm,
#if NORMAL_MAP
										inTan,
#endif
										inCpv,
#if PALETTE_TINT_EDGE
										IN.specular,
#endif
										(float4x3) gWorld,
										pvlWorldPosition,
#if REFLECT
										OUT.worldEyePos,
#else
										t_worldEyePos,
#endif
										pvlWorldNormal,
#if NORMAL_MAP
										outWorldBinormal,
										outWorldTangent,
#endif // NORMAL_MAP
										pvlColor0
#if PALETTE_TINT
										,OUT.color1
#endif
										);

	// Write out final position & texture coords
	OUT.pos = mul(float4(inPos,1), gWorldViewProj);
	OUT.color0 = pvlColor0;

	OUT.worldNormal		= pvlWorldNormal;

	OUT.texCoord.xy = IN.texCoord0.xy;
	OUT.texCoord.zw = float2(0,0);

#if NORMAL_MAP
	OUT.worldTangent.xyz	= outWorldTangent.xyz;
	OUT.worldBinormal.xyz	= outWorldBinormal.xyz;
	OUT.worldTangent.w		= IN.texCoord0.x;
	OUT.worldBinormal.w		= IN.texCoord0.y;
#endif // NORMAL_MAP

	return OUT;
}// end of VS_FurGroundTransformD()...

//
//
//
//
DeferredGBuffer PS_FurGroundDeferredTextured(furPixelInputD IN)
{
DeferredGBuffer OUT;
	SurfaceProperties surfaceInfo = GetSurfaceProperties(
		IN.color0.rgba,
#if PALETTE_TINT
		IN.color1.rgba,
#endif
#if DIFFUSE_TEXTURE
		DIFFUSE_PS_INPUT.xy,
		DiffuseSampler,
#endif
#if DIFFUSE2
		IN.texCoord.zw,
		DiffuseSampler2,
#endif	// DIFFUSE2
#if SPEC_MAP
		SPECULAR_PS_INPUT.xy,
		SpecSampler,
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

	// Compute some surface information.
	StandardLightingProperties surfaceStandardLightingProperties = DeriveLightingPropertiesForCommonSurface( surfaceInfo );

	surfaceStandardLightingProperties.naturalAmbientScale	= 1.0f;
	surfaceStandardLightingProperties.artificialAmbientScale= 0.0f;


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
}// end of PS_FurGroundDeferredTextured()...

#if DEFERRED_TECHNIQUES && NON_CUTOUT_DEFERRED_TECHNIQUES
technique deferred_draw
{
	pass p0 
	{
		VertexShader = compile VERTEXSHADER	VS_FurGroundTransformD();
		PixelShader  = compile PIXELSHADER	PS_FurGroundDeferredTextured()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
#endif // DEFERRED_TECHNIQUES && NON_CUTOUT_DEFERRED_TECHNIQUES

#if DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES && __PS3
technique deferredalphaclip_draw
{
	pass p0 
	{
		VertexShader = compile VERTEXSHADER	VS_FurGroundTransformD();
		PixelShader  = compile PIXELSHADER	PS_FurGroundDeferredTextured()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
#endif // DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES && __PS3
