// 
// rdr2WorldShaders/Terrain.fxh
// 
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved. 
//


#ifndef	BOBSHADERS_TERRAIN_FXH
#define	BOBWORLDSHADERS_TERRAIN_FXH



#ifndef	FOUR_TEXTURE_TERRAIN
#define	FOUR_TEXTURE_TERRAIN	0
#endif

#ifndef	THREE_TEXTURE_TERRAIN
#define	THREE_TEXTURE_TERRAIN	0
#endif

#if	__XENON
#define	DIFFUSE_MAX_ANISOTROPY	4	// for the terrain diffuse palette. NOTE:  8 adds just 0.7ms to a 1/2 screen terrain scene on 360
#define	NORMALS_MAX_ANISOTROPY	4	// for the terrain normal maps
#else
#define	DIFFUSE_MAX_ANISOTROPY	4	// for the terrain diffuse palette
#define	NORMALS_MAX_ANISOTROPY	4	// for the terrain normal maps
#endif

#define BLENDMAP_MAX_ANISOTROPY	2	// for the Channel blend maps

#define	DEBUG_CHANNELMAP	0

#define	COMPUTE_HEIGHT_FROM_DIFFUSE	1	// fake a height value based on Diffuse colors (so we can use a dxt1 texture...)

#define	PREDICATE_FETCHES	(1 && __XENON)	  // saves up to 2ms on 360, slows down ps3. sigh


#if __XENON && PREDICATE_FETCHES
#define PREDICATE [predicate]
#else
#define PREDICATE
#endif

#define	USE_PARTIAL_DERIVATIVE_NORMALS	1		// use partial derivative form of normals

#define ENABLE_RG_NORMALS				1		// if we are using a dxt1 for the normal map, the x is in red channel instead of w 
												// (NOTE: for now it works with either)

#if	__XENON
#define	ANISO_MIP_FILTER	Point
#else
#define	ANISO_MIP_FILTER	Linear
#endif


#if __WIN32PC || RSG_ORBIS
float2	BrightSandHack
<
	string	UIName = "Bright Sand Hack";
	float	UIMin = 0.0;
	float	UIMax = 1.0;
	float	UIStep = 0.001;
> = { 0.0, 0.0f };
#endif


float3	Bumpiness_Terrain123
<
	string	UIName = "Terrain Bumpiness123";
	string	UIWidget = "Numeric";
	float	UIMin = 0.0;
	float	UIMax = 200.0;
	float	UIStep = 0.01;
> = { 1.0, 1.0, 1.0 };



float3	Bumpiness_Terrain456
<
	string	UIName = "Terrain Bumpiness345";
	string	UIWidget = "Numeric";
	float	UIMin = 0.0;
	float	UIMax = 200.0;
	float	UIStep = 0.01;
> = { 1.0, 1.0, 1.0 };



// How many times, across the 0-1 of the UV channel map, do the tiles repeat

float2	MegaTileRepetitions
<
	string	UIName = "Mega Tile Repetitions";
	string	UIWidget = "Numeric";
	float	UIMin = 0.001;
	float	UIMax = 10000.0;
	float	UIStep = 1.0;
	int		nostrip = 1;
> = { 128.0, 128.0 };



// What is the offset of the UV for the tile when at (0,0) in the channel map

float2	MegaTileOffset
<
	string	UIName = "Mega Tile Offset";
	string	UIWidget = "Numeric";
	float	UIMin = -100.0;
	float	UIMax = 100.0;
	float	UIStep = 0.001;
	int		nostrip = 1;
> = { 0.0, 0.0 };



float2	BlendMapScale1
<
	string	UIName = "Blend Map Scale 1";
	string	UIWidget = "Numeric";
	float	UIMin = 0.0001;
	float	UIMax = 10000.0;
	float	UIStep = 0.001;
	int		nostrip = 1;
> = { 1.0, 1.0 };



// What is the offset of the UV for the tile when at (0,0) in the channel map

float2	BlendMapOffset1
<
	string	UIName = "Blend Map Offset 1";
	string	UIWidget = "Numeric";
	float	UIMin = -100.0;
	float	UIMax = 100.0;
	float	UIStep = 0.001;
	int		nostrip = 1;
> = { 0.0, 0.0 };



float2	BlendMapScale2
<
	string	UIName = "Blend Map Scale 2";
	string	UIWidget = "Numeric";
	float	UIMin = 0.0001;
	float	UIMax = 10000.0;
	float	UIStep = 0.001;
	int		nostrip = 1;
> = { 1.0, 1.0 };



// What is the offset of the UV for the tile when at (0,0) in the channel map

float2	BlendMapOffset2
<
	string	UIName = "Blend Map Offset 2";
	string	UIWidget = "Numeric";
	float	UIMin = -100.0;
	float	UIMax = 100.0;
	float	UIStep = 0.001;
	int		nostrip = 1;
> = { 0.0, 0.0 };



float	MegaTexSpecularIntensityScale1
<
	string	UIName = "Mega Spec Intensity Scale 1";
	string	UIWidget = "Numeric";
	float	UIMin = 0.0;
	float	UIMax = 10.00;
	float	UIStep = 0.001;
	int		nostrip = 1;
> = 1.0;



float	MegaTexSpecularIntensityScale2
<
	string	UIName = "Mega Spec Intensity Scale 2";
	string	UIWidget = "Numeric";
	float	UIMin = 0.0;
	float	UIMax = 10.00;
	float	UIStep = 0.001;
	int		nostrip = 1;
> = 0.0;



float3	MegaTexSpecularIntensity123
<
	string	UIName = "Mega Spec Inten 123";
	string	UIWidget = "Numeric";
	float	UIMin = 0.0;
	float	UIMax = 1.00;
	float	UIStep = 0.001;
	int		nostrip = 1;
> = { 0.1, 0.1, 0.1 };



float3	MegaTexSpecularIntensity456
<
	string	UIName = "Mega Spec Inten 456";
	string	UIWidget = "Numeric";
	float	UIMin = 0.0;
	float	UIMax = 1.00;
	float	UIStep = 0.001;
	int		nostrip = 1;
> = { 0.1, 0.1, 0.1 };



float	MegaTexSpecularExponent123
<
	string	UIName = "Mega Spec Exp 123";
	string	UIWidget = "Numeric";
	float	UIMin = 0.0;
	float	UIMax = 100.00;
	float	UIStep = 0.001;
	int		nostrip = 1;
> = 30.0;



float	MegaTexSpecularExponent456
<
	string	UIName = "Mega Spec Exp 456";
	string	UIWidget = "Numeric";
	float	UIMin = 0.0;
	float	UIMax = 128.00;
	float	UIStep = 0.001;
	int		nostrip = 1;
> =  30.0;



float	MegaTexBump123
<
	string	UIName = "Mega Bump Self Shadow 123";
	string	UIWidget = "Numeric";
	float	UIMin = -1.0;
	float	UIMax = 1.00;
	float	UIStep = 0.001;
	int		nostrip = 1;
> =  0.125;



float	MegaTexBump456
<
	string	UIName = "Mega Bump Self Shadow 456";
	string	UIWidget = "Numeric";
	float	UIMin = -1.0;
	float	UIMax = 1.00;
	float	UIStep = 0.001;
	int		nostrip = 1;
> =  0.29;



float	MinBumpSelfShadow
<
	string UIName = "Min Bump Self Shadow";
	string UIWidget = "Numeric";
	float  UIMin = 0.0;
	float  UIMax = 5.00;
	float  UIStep = 0.01;
> = 0.0;



float	AmbOccStrength
<
	string UIName = "Amb Occ Offset";
	string UIWidget = "Numeric";
	float  UIMin = -5.0;
	float  UIMax = 5.00;
	float  UIStep = 0.01;
> = 0.0;



float	TerrainDirtIntensity
<
	string UIName = "Terrain Dirt Intensity";
	string UIWidget = "Numeric";
	float  UIMin = 0.0;
	float  UIMax = 5.00;
	float  UIStep = 0.001;
> = 1.0;



float	TerrainAmbOccSunFactor
<
	string UIName = "Terrain AmbOcc Sun Factor";
	float  UIMin = 0.0;
	float  UIMax = 1.0;
	float  UIStep = 0.01;
> = 0.5;



float	TerrainAmbOccFillFactor
<
	string UIName = "Terrain AmbOcc Fill Factor";
	float  UIMin = 0.0;
	float  UIMax = 1.0;
	float  UIStep = 0.01;
> = 0.5;



float	TerrainAmbOccPointLightFactor
<
	string UIName = "Terrain AmbOcc Point Light Factor";
	float  UIMin = 0.0;
	float  UIMax = 1.0;
	float  UIStep = 0.01;
> = 1.0;



float	DirectStrength	:	DirectStrength = 1.0;



//#pragma sampler 2

BeginSampler(sampler2D, TerrainDiffuseTexture1, TerrainDiffuseSampler1, TerrainDiffuseTex1)
	string	UIName = "Color Texture 1";
	string	UvSetName = "map1";
	int		UvSetIndex = 0;
	string	TCPTemplate = "Diffuse";
	string	TextureType="Diffuse";
ContinueSampler(sampler2D,TerrainDiffuseTexture1, TerrainDiffuseSampler1, TerrainDiffuseTex1)
	AddressU = Wrap;
	AddressV = Wrap;
	MinFilter = MINANISOTROPIC;
	MagFilter = MAGLINEAR;
	MipFilter = MIPLINEAR;
	ANISOTROPIC_LEVEL(DIFFUSE_MAX_ANISOTROPY)
EndSampler;



BeginSampler(sampler2D, TerrainDiffuseTexture2, TerrainDiffuseSampler2, TerrainDiffuseTex2)
	string	UIName = "Color Texture 2";
	string	UvSetName = "map1";
	int		UvSetIndex = 0;
	string	TCPTemplate = "Diffuse";
	string	TextureType="Diffuse";
ContinueSampler(sampler2D, TerrainDiffuseTexture2, TerrainDiffuseSampler2, TerrainDiffuseTex2)
	AddressU = Wrap;
	AddressV = Wrap;
	MinFilter = MINANISOTROPIC;
	MagFilter = MAGLINEAR;
	MipFilter = MIPLINEAR;
	ANISOTROPIC_LEVEL(DIFFUSE_MAX_ANISOTROPY)
EndSampler;



BeginSampler(sampler2D, TerrainDiffuseTexture3, TerrainDiffuseSampler3, TerrainDiffuseTex3)
	string	UIName = "Color Texture 3";
	string	UvSetName = "map1";
	int		UvSetIndex = 0;
	string	TCPTemplate = "Diffuse";
	string	TextureType="Diffuse";
ContinueSampler(sampler2D, TerrainDiffuseTexture3, TerrainDiffuseSampler3, TerrainDiffuseTex3)
	AddressU = Wrap;
	AddressV = Wrap;
	MinFilter = MINANISOTROPIC;
	MagFilter = MAGLINEAR;
	MipFilter = MIPLINEAR;
	ANISOTROPIC_LEVEL(DIFFUSE_MAX_ANISOTROPY)
EndSampler;



#if	!THREE_TEXTURE_TERRAIN

BeginSampler(sampler2D, TerrainDiffuseTexture4, TerrainDiffuseSampler4, TerrainDiffuseTex4)
	string	UIName = "Color Texture 4";
	string	UvSetName = "map1";
	int		UvSetIndex = 0;
	string	TCPTemplate = "Diffuse";
	string	TextureType="Diffuse";
ContinueSampler(sampler2D, TerrainDiffuseTexture4, TerrainDiffuseSampler4, TerrainDiffuseTex4)
	AddressU = Wrap;
	AddressV = Wrap;
	MinFilter = MINANISOTROPIC;
	MagFilter = MAGLINEAR;
	MipFilter = MIPLINEAR;
	ANISOTROPIC_LEVEL(DIFFUSE_MAX_ANISOTROPY)
EndSampler;



#if	!FOUR_TEXTURE_TERRAIN

BeginSampler(sampler2D, TerrainDiffuseTexture5, TerrainDiffuseSampler5, TerrainDiffuseTex5)
	string	UIName = "Color Texture 5";
	string	UvSetName = "map1";
	int		UvSetIndex = 0;
	string	TCPTemplate = "Diffuse";
	string	TextureType="Diffuse";
ContinueSampler(sampler2D, TerrainDiffuseTexture5, TerrainDiffuseSampler5, TerrainDiffuseTex5)
	AddressU = Wrap;
	AddressV = Wrap;
	MinFilter = MINANISOTROPIC;
	MagFilter = MAGLINEAR;
	MipFilter = MIPLINEAR;
	ANISOTROPIC_LEVEL(DIFFUSE_MAX_ANISOTROPY)
EndSampler;



BeginSampler(sampler2D, TerrainDiffuseTexture6, TerrainDiffuseSampler6, TerrainDiffuseTex6)
	string	UIName = "Color Texture 6";
	string	UvSetName = "map1";
	int		UvSetIndex = 0;
	string	TCPTemplate = "Diffuse";
	string	TextureType="Diffuse";
ContinueSampler(sampler2D, TerrainDiffuseTexture6, TerrainDiffuseSampler6, TerrainDiffuseTex6)
	AddressU = Wrap;
	AddressV = Wrap;
	MinFilter = MINANISOTROPIC;
	MagFilter = MAGLINEAR;
	MipFilter = MIPLINEAR;
	ANISOTROPIC_LEVEL(DIFFUSE_MAX_ANISOTROPY)
EndSampler;

#endif
#endif



//#pragma sampler 2

BeginSampler(sampler2D, TerrainNormalTexture1, TerrainNormalSampler1, TerrainNormalTex1)
	string	UIName = "Normal Map 1";
	string	UvSetName = "map1";
	int		UvSetIndex = 0;
	string	TextureOutputFormats = "PC=COMP_NRM_DXT5";
	string	UIHint="NORMAL_MAP";
	string	TCPTemplate = "NormalMap";
	string	TextureType="Bump";
ContinueSampler(sampler2D, TerrainNormalTexture1,  TerrainNormalSampler1, TerrainNormalTex1)
	AddressU = Wrap;
	AddressV = Wrap;
	MinFilter = MINANISOTROPIC;
	MagFilter = MAGLINEAR;
	MipFilter = MIPLINEAR;
	ANISOTROPIC_LEVEL(DIFFUSE_MAX_ANISOTROPY)
EndSampler;



BeginSampler(sampler2D, TerrainNormalTexture2, TerrainNormalSampler2, TerrainNormalTex2)
	string	UIName = "Normal Map 2";
	string	UvSetName = "map1";
	int		UvSetIndex = 0;
	string	TextureOutputFormats = "PC=COMP_NRM_DXT5";
	string	UIHint="NORMAL_MAP";
	string	TCPTemplate = "NormalMap";
	string	TextureType="Bump";
ContinueSampler(sampler2D, TerrainNormalTexture2,  TerrainNormalSampler2, TerrainNormalTex2)
	AddressU = Wrap;
	AddressV = Wrap;
	MinFilter = MINANISOTROPIC;
	MagFilter = MAGLINEAR;
	MipFilter = MIPLINEAR;
	ANISOTROPIC_LEVEL(DIFFUSE_MAX_ANISOTROPY)
EndSampler;



BeginSampler(sampler2D, TerrainNormalTexture3, TerrainNormalSampler3, TerrainNormalTex3)
	string	UIName = "Normal Map 3";
	string	UvSetName = "map1";
	int		UvSetIndex = 0;
	string	TextureOutputFormats = "PC=COMP_NRM_DXT5";
	string	UIHint="NORMAL_MAP";
	string	TCPTemplate = "NormalMap";
	string	TextureType="Bump";
ContinueSampler(sampler2D, TerrainNormalTexture3,  TerrainNormalSampler3, TerrainNormalTex3)
	AddressU = Wrap;
	AddressV = Wrap;
	MinFilter = MINANISOTROPIC;
	MagFilter = MAGLINEAR;
	MipFilter = MIPLINEAR;
	ANISOTROPIC_LEVEL(DIFFUSE_MAX_ANISOTROPY)
EndSampler;



#if	!THREE_TEXTURE_TERRAIN

BeginSampler(sampler2D, TerrainNormalTexture4, TerrainNormalSampler4, TerrainNormalTex4)
	string	UIName = "Normal Map 4";
	string	UvSetName = "map1";
	int		UvSetIndex = 0;
	string	TextureOutputFormats = "PC=COMP_NRM_DXT5";
	string	UIHint="NORMAL_MAP";
	string	TCPTemplate = "NormalMap";
	string	TextureType="Bump";
ContinueSampler(sampler2D, TerrainNormalTexture4,  TerrainNormalSampler4, TerrainNormalTex4)
	AddressU = Wrap;
	AddressV = Wrap;
	MinFilter = MINANISOTROPIC;
	MagFilter = MAGLINEAR;
	MipFilter = MIPLINEAR;
	ANISOTROPIC_LEVEL(DIFFUSE_MAX_ANISOTROPY)
EndSampler;



#if	!FOUR_TEXTURE_TERRAIN

BeginSampler(sampler2D, TerrainNormalTexture5, TerrainNormalSampler5, TerrainNormalTex5)
	string	UIName = "Normal Map 5";
	string	UvSetName = "map1";
	int		UvSetIndex = 0;
	string	TextureOutputFormats = "PC=COMP_NRM_DXT5";
	string	UIHint="NORMAL_MAP";
	string	TCPTemplate = "NormalMap";
	string	TextureType="Bump";
ContinueSampler(sampler2D, TerrainNormalTexture5,  TerrainNormalSampler5, TerrainNormalTex5)
	AddressU = Wrap;
	AddressV = Wrap;
	MinFilter = MINANISOTROPIC;
	MagFilter = MAGLINEAR;
	MipFilter = MIPLINEAR;
	ANISOTROPIC_LEVEL(DIFFUSE_MAX_ANISOTROPY)
EndSampler;



BeginSampler(sampler2D, TerrainNormalTexture6, TerrainNormalSampler6, TerrainNormalTex6)
	string	UIName = "Normal Map 6";
	string	UvSetName = "map1";
	int		UvSetIndex = 0;
	string	TextureOutputFormats = "PC=COMP_NRM_DXT5";
	string	UIHint="NORMAL_MAP";
	string	TCPTemplate = "NormalMap";
	string	TextureType="Bump";
ContinueSampler(sampler2D, TerrainNormalTexture6,  TerrainNormalSampler6, TerrainNormalTex6)
	AddressU = Wrap;
	AddressV = Wrap;
	MinFilter = MINANISOTROPIC;
	MagFilter = MAGLINEAR;
	MipFilter = MIPLINEAR;
	ANISOTROPIC_LEVEL(DIFFUSE_MAX_ANISOTROPY)
EndSampler;

#endif
#endif



BeginSampler(sampler2D, TerrainBlendTexture1, TerrainBlendMap1, TerrainBlendTex1)
    string	UIName = "Blend Texture 1";
	string	UvSetName = "map1";
	int		UvSetIndex = 0;
	string	TextureOutputFormats = "PC=MEGA_CHANNEL_012";
	string	TCPTemplate = "TerrainMap0";
	string	TextureType="Lookup";
ContinueSampler(sampler2D, TerrainBlendTexture1, TerrainBlendMap1, TerrainBlendTex1)
	AddressU = Clamp;
	AddressV = Clamp;
	MinFilter = HQLINEAR;
	MagFilter = MAGLINEAR;
	MipFilter = MIPLINEAR;
	MipMapLodBias = 0.0;
EndSampler;



#if	!FOUR_TEXTURE_TERRAIN

BeginSampler(sampler2D, TerrainBlendTexture2, TerrainBlendMap2,TerrainBlendTex2)
	string	UIName = "Blend Texture 2";
	string	UvSetName = "map1";
	int		UvSetIndex = 0;
	string	TextureOutputFormats = "PC=MEGA_CHANNEL_345";
	string	TCPTemplate = "TerrainMap1";
	string	TextureType="Lookup";
ContinueSampler(sampler2D,TerrainBlendTexture2, TerrainBlendMap2, TerrainBlendTex2)
	AddressU = Clamp;
	AddressV = Clamp;
	MinFilter = HQLINEAR;
	MagFilter = MAGLINEAR;
	MipFilter = MIPLINEAR;
	MipMapLodBias = 0.0;
EndSampler;

#endif



float4	FetchThreeWithWeights	(sampler2D ss1, sampler2D ss2, sampler2D ss3, float2 uv, float3 weights)
{
	float4 col1 = tex2D(ss1,uv);
	float4 col2 = tex2D(ss2,uv);
	float4 col3 = tex2D(ss3,uv);

	return col1*weights.x + col2*weights.y + col3*weights.z;
}



float4	FetchFourWithWeights	(sampler2D ss1, sampler2D ss2, sampler2D ss3, sampler2D ss4, float2 uv, float3 weights)
{
#if !PREDICATE_FETCHES
	float4 col1 = tex2D(ss1,uv);
	float4 col2 = tex2D(ss2,uv);
	float4 col3 = tex2D(ss3,uv);
	float4 col4 = tex2D(ss4,uv);

	return col1*weights.x + col2*weights.y + col3*weights.z + col4*(1-dot(weights,float3(1,1,1)));

#else

	float4 col = 0;
	
	float ww = (1-dot(weights,float3(1,1,1)));
	PREDICATE
	if (weights.x)	col += weights.x * tex2D(ss1,uv);

	PREDICATE
	if (weights.y) col += weights.y * tex2D(ss2,uv);

	PREDICATE
	if (weights.z) col += weights.z * tex2D(ss3,uv);
	
	PREDICATE
	if (ww) col += ww* tex2D(ss4,uv);

	return col;
#endif
}



float4 FetchSixWithWeights(sampler2D ss1, sampler2D ss2, sampler2D ss3, sampler2D ss4, sampler2D ss5, sampler2D ss6, float2 uv, float3 weights1, float3 weights2)
{
#if !PREDICATE_FETCHES
		float4 col1 = tex2D(ss1,uv);
		float4 col2 = tex2D(ss2,uv);
		float4 col3 = tex2D(ss3,uv);
		float4 col4 = tex2D(ss4,uv);
		float4 col5 = tex2D(ss5,uv);
		float4 col6 = tex2D(ss6,uv);

#if __WIN32PC || RSG_ORBIS	
		col1 += float4(BrightSandHack.xxx, 0.0f);
		col2 += float4(BrightSandHack.xxx, 0.0f);
		col3 += float4(BrightSandHack.xxx, 0.0f);
		col4 += float4(BrightSandHack.xxx, 0.0f);
		col5 += float4(BrightSandHack.xxx, 0.0f);
		col6 += float4(BrightSandHack.xxx, 0.0f);
#endif

	#if DEBUG_CHANNELMAP
		float3 debugOut =   dot(float3(1,1,1),weights1) + dot(float3(1,1,1),weights2);
		return float4(debugOut,1) + 
				saturate(-2 + col1*weights1.x + col2*weights1.y + col3*weights1.z + 
							  col4*weights2.x + col5*weights2.y + col6*weights2.z) ;
	#else
		return col1*weights1.x + col2*weights1.y + col3*weights1.z + 
		 		col4*weights2.x + col5*weights2.y + col6*weights2.z;
	#endif

#else

	float4 col = 0;
	
	PREDICATE
	if (weights1.x)	col += weights1.x * tex2D(ss1,uv);

	PREDICATE
	if (weights1.y) col += weights1.y * tex2D(ss2,uv);

	PREDICATE
	if (weights1.z) col += weights1.z * tex2D(ss3,uv);
	
	PREDICATE
	if (weights2.x) col += weights2.x * tex2D(ss4,uv);
	
	PREDICATE
	if (weights2.y)	col += weights2.y * tex2D(ss5,uv);
		
	PREDICATE
	if (weights2.z) col += weights2.z * tex2D(ss6,uv);

	return col;
#endif
}



float4	MegaTexPCFColorOnly	(float2 InTex, out float aoFromCM, out float3 blendWeights1, out float3 blendWeights2)
{
	float2	uv = InTex.xy * MegaTileRepetitions.xy + MegaTileOffset.xy;

	float4	bw = tex2D(TerrainBlendMap1, InTex.xy * BlendMapScale1.xy + BlendMapOffset1.xy).zyxw;

	aoFromCM = bw.w;	// for old dxt1 Blendmap1 textures this is 1 anyway...

	blendWeights1 = bw.xyz;

#if	THREE_TEXTURE_TERRAIN
	blendWeights1 = normalize(blendWeights1);
	float4 final = FetchThreeWithWeights(TerrainDiffuseSampler1, TerrainDiffuseSampler2, TerrainDiffuseSampler3, uv, blendWeights1);	
	blendWeights2 = float3(0.0, 0.0, 0.0);
#elif	FOUR_TEXTURE_TERRAIN
	blendWeights2 = float3(1-dot(blendWeights1,float3(1,1,1)),0,0);
	float4 final = FetchFourWithWeights(TerrainDiffuseSampler1, TerrainDiffuseSampler2, TerrainDiffuseSampler3, TerrainDiffuseSampler4, uv, blendWeights1);	
#else
	blendWeights2 = tex2D(TerrainBlendMap2, InTex.xy * BlendMapScale2.xy + BlendMapOffset2.xy).zyx;		
	float total = dot(float3(1,1,1),blendWeights1) + dot(float3(1,1,1),blendWeights2);
	blendWeights1 /= total;     // normalize the weight, it all has to add to one or we get dark spots...
	blendWeights2 /= total;
	float4 final = FetchSixWithWeights(TerrainDiffuseSampler1, TerrainDiffuseSampler2, TerrainDiffuseSampler3,
									       TerrainDiffuseSampler4, TerrainDiffuseSampler5, TerrainDiffuseSampler6, uv, blendWeights1, blendWeights2);	
#endif
																
#if  !DEBUG_CHANNELMAP
	final *= final; // do the 2.0 Gamma thing...	
#endif	

#if COMPUTE_HEIGHT_FROM_DIFFUSE
	final.w = .2+dot(final.rgb,1.1*float3(0.30,0.59,0.11)); //*weights1.x + col2.r*weights1.y + col3.r*weights1.z + col4.r*weights2.x + col5.r*weights2.y + col6.r*weights2.z;
#endif					
	
	return final;
}


float3 MegaTexPCFNormalOnly(float2 InTex, out float3 blendWeights1, out float3 blendWeights2)
{
	float2 uv = InTex.xy * MegaTileRepetitions.xy + MegaTileOffset.xy;
	blendWeights1 = tex2D(TerrainBlendMap1, InTex.xy * BlendMapScale1.xy + BlendMapOffset1.xy).zyx;	
	
	#if THREE_TEXTURE_TERRAIN
		blendWeights1 = normalize(blendWeights1);
		float4 final = FetchThreeWithWeights(TerrainNormalSampler1, TerrainNormalSampler2, TerrainNormalSampler3, uv, blendWeights1);	
	#elif FOUR_TEXTURE_TERRAIN
		blendWeights2 = float3(1-dot(blendWeights1,float3(1,1,1)),0,0);
		float4 final = FetchFourWithWeights(TerrainNormalSampler1, TerrainNormalSampler2, TerrainNormalSampler3, TerrainNormalSampler4, uv, blendWeights1);	
	#else
 		blendWeights2 = tex2D(TerrainBlendMap2, InTex.xy * BlendMapScale2.xy + BlendMapOffset2.xy).zyx;	
		float total = dot(float3(1,1,1),blendWeights1) + dot(float3(1,1,1),blendWeights2);
		blendWeights1 /= total;     // normalize the weight, it all has to add to one or we get dark spots...
		blendWeights2 /= total;

		float4 final = FetchSixWithWeights(TerrainNormalSampler1, TerrainNormalSampler2, TerrainNormalSampler3,
									       TerrainNormalSampler4, TerrainNormalSampler5, TerrainNormalSampler6, uv, blendWeights1, blendWeights2);	
	#endif
		
#if USE_PARTIAL_DERIVATIVE_NORMALS

	#if ENABLE_RG_NORMALS
		final.xy = -(final.xy*2.0-1.0);
	#else
		final.xy = -(final.wy*2.0-1.0);
	#endif
	
	final.z = 1;
	final.xyz = normalize(final.xyz);
#else
	#if ENABLE_RG_NORMALS
		final.x = final.r ? final.r : final.a; // temp hack so we can use old and new normal maps, in dxt5, r = 0; int dxt 1 it's the x value
		final.xy = final.xy * 2. - 1.;
	#else
		final.xy = final.wy * 2. - 1.;
	#endif
		final.z = sqrt(1.0f - dot( final.xy, final.xy ));
#endif
	
	return final.xyz;
}



#endif
