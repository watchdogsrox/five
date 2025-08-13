//  
// Clouds.fxh
// 
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved. 
//

#define ALPHA_SHADER
#define	NO_SKINNING
#define FOG_SKY 
#define CLOUD_SHADER

#include "../../common.fxh"
#include "../../Lighting/lighting.fxh"

#define CLOUDS_IN_PARABOLOID_ENVMAP 0 // since they are not called from clouds.cpp, so we don't need the techniques

#if __WIN32PC
#	pragma constant 192 //To fix out of constant registers compiler error on windows build. Might need to change if we make a PC version.
#endif

struct FastLight4
{
	float4 OneOverRangeSq;
	float4 fallOff;
	float4 posX;
	float4 posY;
	float4 posZ;

	float4 colR;
	float4 colG;
	float4 colB;
};

////////////////////////////////////////////////////////////////////////////////////
// Cloud textures

BeginSampler(sampler2D, DensityTexture, DensitySampler, DensityTexture)
	string	UIName = "Density Map";
	string	UvSetName = "map1";
	int		UvSetIndex = 0;
	string	TCPTemplate = "Diffuse";
	string	TextureType="Density";
ContinueSampler(sampler2D, DensityTexture, DensitySampler, DensityTexture)
	AddressU = Wrap;
#if defined(USE_CAMERA_UV_SCROLL) 
	AddressV = Wrap;
#else
	AddressV = Clamp;
#endif
	MinFilter = HQLINEAR;
	MagFilter = MAGLINEAR;
	MipFilter = MIPLINEAR;
EndSampler;

BeginSampler(sampler2D, NormalTexture, NormalSampler, NormalTexture)
	string	UIName = "Normal Map";
	string	UvSetName = "map1";
	int		UvSetIndex = 0;
	string	TCPTemplate = "NormalMap";
	string	TextureType="Bump";
ContinueSampler(sampler2D, NormalTexture, NormalSampler, NormalTexture)
	AddressU = Wrap;
#if defined(USE_CAMERA_UV_SCROLL) 
	AddressV = Wrap;
#else
	AddressV = Clamp;
#endif
	MinFilter = HQLINEAR;
	MagFilter = MAGLINEAR;
	MipFilter = MIPLINEAR;
EndSampler;

//Animation textures
BeginSampler(sampler2D, DetailDensityTexture, DetailDensitySampler, DetailDensityTexture)
	string	UIName = "Detail Density Map";
	string	UvSetName = "map1";
	int		UvSetIndex = 0;
	string	TCPTemplate = "Diffuse";
	string	TextureType="Density";
ContinueSampler(sampler2D, DetailDensityTexture, DetailDensitySampler, DetailDensityTexture)
	AddressU = Wrap;
 	AddressV = Wrap;
	MinFilter = HQLINEAR;
	MagFilter = MAGLINEAR;
	MipFilter = MIPLINEAR;
EndSampler;

BeginSampler(sampler2D, DetailNormalTexture, DetailNormalSampler, DetailNormalTexture)
	string	UIName = "Detail Normal Map";
	string	UvSetName = "map1";
	int		UvSetIndex = 0;
	string	TCPTemplate = "NormalMap";
	string	TextureType="Bump";
ContinueSampler(sampler2D, DetailNormalTexture, DetailNormalSampler, DetailNormalTexture)
	AddressU = Wrap;
 	AddressV = Wrap;
	MinFilter = HQLINEAR;
	MagFilter = MAGLINEAR;
	MipFilter = MIPLINEAR;
EndSampler;

BeginSampler(sampler2D, DetailDensity2Texture, DetailDensity2Sampler, DetailDensity2Texture)
	string	UIName = "Detail Density 2 Map";
	string	UvSetName = "map1";
	int		UvSetIndex = 0;
	string	TCPTemplate = "Diffuse";
	string	TextureType="Density";
ContinueSampler(sampler2D, DetailDensity2Texture, DetailDensity2Sampler, DetailDensity2Texture)
	AddressU = Wrap;
 	AddressV = Wrap;
	MinFilter = HQLINEAR;
	MagFilter = MAGLINEAR;
	MipFilter = MIPLINEAR;
EndSampler;

BeginSampler(sampler2D, DetailNormal2Texture, DetailNormal2Sampler, DetailNormal2Texture)
	string	UIName = "Detail Normal 2 Map";
	string	UvSetName = "map1";
	int		UvSetIndex = 0;
	string	TCPTemplate = "NormalMap";
	string	TextureType="Bump";
ContinueSampler(sampler2D, DetailNormal2Texture, DetailNormal2Sampler, DetailNormal2Texture)
	AddressU = Wrap;
 	AddressV = Wrap;
	MinFilter = HQLINEAR;
	MagFilter = MAGLINEAR;
	MipFilter = MIPLINEAR;
EndSampler;


//LOW-RES texture for cheap envmap
BeginSampler(sampler2D, LowResDensityTexture, LowResDensitySampler, LowResDensityTexture)
	string	UIName = "LowRes Density Map";
	string	UvSetName = "map1";
	int		UvSetIndex = 0;
	string	TCPTemplate = "Diffuse";
	string	TextureType="Density";
ContinueSampler(sampler2D, LowResDensityTexture, LowResDensitySampler, LowResDensityTexture)
	AddressU = Wrap;
#if defined(USE_CAMERA_UV_SCROLL) 
	AddressV = Wrap;
#else
	AddressV = Clamp;
#endif
	MinFilter = HQLINEAR;
	MagFilter = MAGLINEAR;
	MipFilter = MIPLINEAR;
EndSampler;

BeginSampler(sampler2D, LowResNormalTexture, LowResNormalSampler, LowResNormalTexture)
	string	UIName = "LowRes Normal Map";
	string	UvSetName = "map1";
	int		UvSetIndex = 0;
	string	TCPTemplate = "NormalMap";
	string	TextureType="Bump";
ContinueSampler(sampler2D, LowResNormalTexture, LowResNormalSampler, LowResNormalTexture)
	AddressU = Wrap;
#if defined(USE_CAMERA_UV_SCROLL) 
	AddressV = Wrap;
#else
	AddressV = Clamp;
#endif
	MinFilter = HQLINEAR;
	MagFilter = MAGLINEAR;
	MipFilter = MIPLINEAR;
EndSampler;

//Depth Texture
BeginSampler(	sampler2D, DepthMapTexture, DepthMapTexSampler, DepthMapTexture)
ContinueSampler(sampler2D, DepthMapTexture, DepthMapTexSampler, DepthMapTexture)
	AddressU	= Clamp;
	AddressV	= Clamp;
	MinFilter	= POINT;
	MagFilter	= POINT;
EndSampler;

#if __XENON
BeginSampler(sampler2D, DitherTex, DitherSampler, DitherTex)
ContinueSampler(sampler2D, DitherTex, DitherSampler, DitherTex)
	AddressU  = WRAP;        
	AddressV  = WRAP;
	AddressW  = WRAP;
	MINFILTER = LINEAR;
	MAGFILTER = LINEAR;
EndSampler;
#endif

////////////////////////////////////////////////////////////////////////////////////
// Cloud code-controled keyframed variables
BEGIN_RAGE_CONSTANT_BUFFER(clouds_locals,b4)

float3 gSkyColor : SkyColorCloud = {0.0f, 0.0f, 0.0f};
float3 gEastMinusWestColor : EastColorCloud = {0.0f, 0.0f, 0.0f};
float3 gWestColor : WestColorCloud = {0.0f, 0.0f, 0.0f};

float3 gSunDirection : SunDirection;
float3 gSunColor : SunColor = {1.0f, 1.0f, 1.0f};
float3 gCloudColor : CloudColor = {1.0f, 1.0f, 1.0f};
float3 gAmbientColor : AmbientColor = {0.0f, 0.0f, 0.0f};
float3 gBounceColor : BounceColor = {0.0f, 0.0f, 0.0f};
float4 gDensityShiftScale : DensityShiftScale = {0.0f, 1.0f, 0.0f, 0.0f};   // z = Depth Buffer is valid , w unused
float4 gScatterG_GSquared_PhaseMult_Scale : ScatterG_GSquared_PhaseMult_Scale = {-0.75f, -0.75f * -0.75f, (3 * (1 - (-0.75f))) / (2 * (2 + (-0.75f))), 1.0f};
float4 gPiercingLightPower_Strength_NormalStrength_Thickness : PiercingLightPower_Strength_NormalStrength_Thickness = {1.0f, 1.0f, 1.0f, 1.0f};
float3 gScaleDiffuseFillAmbient : ScaleDiffuseFillAmbient = {1.0f, 1.0f, 1.0f};
float3 gWrapLighting_MSAARef : WrapLighting_MSAARef = {1.0f, 1.0f, 1.0f};

float4 gNearFarQMult : NearFarQMult;

#define gValidDepthBuffer (gDensityShiftScale.z)

//Variables for Animations
float3 gAnimCombine : AnimCombine = {1.0f, 1.0f, 1.0f};
float3 gAnimSculpt : AnimSculpt = {0.0f, 0.0f, 0.0f};
float3 gAnimBlendWeights : AnimBlendWeights = {1.0f, 0.0f, 0.0f};
//On PC we need this to be float4's as an array of anything less than float4 gets expanded to float4's
//but the game side packs them as you would expect.
float4 gUVOffset[2] : UVOffset = {{0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.0f}};

// custom view proj, so we can work around edge clipping the cloud dome at the far clip plane
row_major float4x4 gCloudViewProj : CloudViewProjection;

float4 gCameraPos : CameraPos;						// xyz is camer potion, w is inverse depth projection flag
#define cloudUsesInverseZProjection (gCameraPos.w)

//Macro to easily access desired params
#define MAIN_LIGHT_DIRECTION	(gSunDirection)
#define MAIN_LIGHT_COLOR		(gSunColor)
//#define MAIN_LIGHT_DIRECTION	(gDirectionalLight.xzy)
//#define MAIN_LIGHT_DIRECTION	(-gDirectionalLight.xyz)
//#define MAIN_LIGHT_COLOR		(gDirectionalColour.rgb)


////////////////////////////////////////////////////////////////////////////////////
// Cloud art-controled variables
float2 gUVOffset1 : UVOffset1
<
	string	UIName = "UV Offset 1";
	float	UIMin = -1.0;
	float	UIMax = 1.0;
	float	UIStep = 0.01;
> = {0.0f, 0.0f};

float2 gUVOffset2 : UVOffset2
<
	string	UIName = "UV Offset 2";
	float	UIMin = -1.0;
	float	UIMax = 1.0;
	float	UIStep = 0.01;
> = {0.0f, 0.0f};

float2 gUVOffset3 : UVOffset3
<
	string	UIName = "UV Offset 3";
	float	UIMin = -1.0;
	float	UIMax = 1.0;
	float	UIStep = 0.01;
> = {0.0f, 0.0f};

float2 gRescaleUV1 : RescaleUV1
<
	string	UIName = "Rescale UV 1";
	float	UIMin = 0.0;
	float	UIMax = 50.0;
	float	UIStep = 1.0;
> = {1.0f, 1.0f};

float2 gRescaleUV2 : RescaleUV2
<
	string	UIName = "Rescale UV 2";
	float	UIMin = 0.0;
	float	UIMax = 50.0;
	float	UIStep = 1.0;
> = {1.0f, 1.0f};

float2 gRescaleUV3 : RescaleUV3
<
	string	UIName = "Rescale UV 3";
	float	UIMin = 0.0;
	float	UIMax = 50.0;
	float	UIStep = 1.0;
> = {1.0f, 1.0f};

float gSoftParticleRange : SoftParticleRange
<
	string	UIName = "Soft Partice Range";
	float	UIMin = 1.0;
	float	UIMax = 500.0;
	float	UIStep = 1.0;
> = 175.0f;

float gEnvMapAlphaScale : EnvMapAlphaScale
<
	string	UIName = "EnvMap Alpha Scale";
	float	UIMin = 0.0;
	float	UIMax = 1.0;
	float	UIStep = 0.01;
> = 0.75f;

float2 cloudLayerAnimScale1 : cloudLayerAnimScale1
<
	int nostrip=1;	// dont strip
	string UIWidget = "slider";
	string UIName = "Anim Scale 1";
	float UIMin = 0.0;
	float UIMax = 8.0;
	float UIStep = .1;
> = { 1.0, 1.0};

float2 cloudLayerAnimScale2 : cloudLayerAnimScale2
<
	int nostrip=1;	// dont strip
	string UIWidget = "slider";
	string UIName = "Anim Scale 2";
	float UIMin = 0.0;
	float UIMax = 8.0;
	float UIStep = .1;
> = { 1.0, 1.0};

float2 cloudLayerAnimScale3 : cloudLayerAnimScale3
<
	int nostrip=1;	// dont strip
	string UIWidget = "slider";
	string UIName = "Anim Scale 3";
	float UIMin = 0.0;
	float UIMax = 8.0;
	float UIStep = .1;
> = { 1.0, 1.0};


#ifdef USE_BILLBOARD

	float4 cloudBillboardParams : cloudBillboardParams
	<
		string UIName = "BB Width/Height";
		string UIWidget = "slider";
		float UIMin = 0.0;
		float UIMax = 100.0;
		float UIStep = .1;
	> = { 1.0, 1.0, 0.0, 0.0};

	#define cloudBillboardWidth		(cloudBillboardParams.x)
	#define cloudBillboardHeight	(cloudBillboardParams.y)

#endif // USE_BILLBOARD

EndConstantBufferDX10(clouds_locals)

//--------------------------------------------------------------------------------------------------------------------------------------------------//



////////////////////////////////////////////////////////////////////////////////////
// Cloud shader functions

float3 CalculateScatterDiffuseFastLight4ForClouds(float3 pos, float3 normal, float3 view, float thickness, float scatterScale, out float3 scatterCol, FastLight4 lgt)
{   
	float4 dX = lgt.posX - pos.xxxx;
	float4 dY = lgt.posY - pos.yyyy;
	float4 dZ = lgt.posZ - pos.zzzz;
	float4 dist = dX * dX;
	dist = dY * dY + dist;
	dist = dZ * dZ + dist;				// 6 instructions	
	float4 recip = rsqrt(dist);			// 4 instructions should be paired on 360

	float4 atten = saturate((dist * -lgt.OneOverRangeSq) + 1);	// 1 instruct
	float4 attenSq = atten * atten;	//Dampen attenuation a little more. Help remove circular edge from light.

	float4 light = dX * normal.xxxx;
	light = dY * normal.yyyy + light;
	light = dZ * normal.zzzz + light;
	light *= attenSq; // 4 instructions
	light = saturate(light * recip);

	//**** Cloud Specific...add scattering.
	float4 cosTheta = dX * view.xxxx;
	cosTheta = dY * normal.yyyy + cosTheta;
	cosTheta = dZ * normal.zzzz + cosTheta;
	cosTheta *= recip;
	float4 cosThetaSq = cosTheta * cosTheta;

	float4 phase = (1.0f + cosThetaSq) / ragePow((1.0f + gScatterG_GSquared_PhaseMult_Scale.y - (2.0f * gScatterG_GSquared_PhaseMult_Scale.x * cosTheta)), 1.5f);
	phase *= gScatterG_GSquared_PhaseMult_Scale.z;

	float4 scattering = phase * scatterScale * thickness * atten;
	scatterCol = float3(	dot(lgt.colR, scattering),
							dot(lgt.colG, scattering),
							dot(lgt.colB, scattering) );
	//**** End Scattering

	//Apply to color
	float3	col = float3(	dot(lgt.colR, light),
							dot(lgt.colG, light),
							dot(lgt.colB, light) );	// 3
	return col;
}

float3 CalculateScatterDiffuseFastLight4ConstantsForClouds(float3 pos, float3 normal, float3 view, float thickness, float scatterScale, out float3 scatterCol)
{
	FastLight4	lgt;
	#if __SHADERMODEL >= 40
		lgt.posX = float4(gLightPositionAndInvDistSqr[0].x, gLightPositionAndInvDistSqr[1].x, gLightPositionAndInvDistSqr[2].x, gLightPositionAndInvDistSqr[3].x);
		lgt.posY = float4(gLightPositionAndInvDistSqr[0].y, gLightPositionAndInvDistSqr[1].y, gLightPositionAndInvDistSqr[2].y, gLightPositionAndInvDistSqr[3].y);
		lgt.posZ = float4(gLightPositionAndInvDistSqr[0].z, gLightPositionAndInvDistSqr[1].z, gLightPositionAndInvDistSqr[2].z, gLightPositionAndInvDistSqr[3].z);
		lgt.OneOverRangeSq = float4(gLightPositionAndInvDistSqr[0].w, gLightPositionAndInvDistSqr[1].w, gLightPositionAndInvDistSqr[2].w, gLightPositionAndInvDistSqr[3].w);
		lgt.colR = float4(gLightColourAndCapsuleExtent[0].r, gLightColourAndCapsuleExtent[1].r, gLightColourAndCapsuleExtent[2].r, gLightColourAndCapsuleExtent[3].r);
		lgt.colG = float4(gLightColourAndCapsuleExtent[0].g, gLightColourAndCapsuleExtent[1].g, gLightColourAndCapsuleExtent[2].g, gLightColourAndCapsuleExtent[3].g);
		lgt.colB = float4(gLightColourAndCapsuleExtent[0].b, gLightColourAndCapsuleExtent[1].b, gLightColourAndCapsuleExtent[2].b, gLightColourAndCapsuleExtent[3].b);
		lgt.fallOff = float4(gLightDirectionAndFalloffExponent[0].w, gLightDirectionAndFalloffExponent[1].w, gLightDirectionAndFalloffExponent[2].w, gLightDirectionAndFalloffExponent[3].w);
	#else
		lgt.posX = gLight0PosX;
		lgt.posY = gLight0PosY;
		lgt.posZ = gLight0PosZ;
		lgt.colR = gLight0ColR;
		lgt.colG = gLight0ColG;
		lgt.colB = gLight0ColB;
		lgt.fallOff = gLight0FalloffExponent;
		lgt.OneOverRangeSq = gLight0InvDistSqr;
	#endif
	return CalculateScatterDiffuseFastLight4ForClouds(pos, normal, -view, thickness, scatterScale, scatterCol, lgt);
}

float3 CalculateDiffuseFastLight4ForClouds(float3 pos, float3 normal, FastLight4 lgt)
{   
	float4 dX = lgt.posX - pos.xxxx;
	float4 dY = lgt.posY - pos.yyyy;
	float4 dZ = lgt.posZ - pos.zzzz;
	float4 dist = dX * dX;
	dist = dY * dY + dist;
	dist = dZ * dZ + dist;				// 6 instructions	
	float4 recip = rsqrt(dist);			// 4 instructions should be paired on 360

	float4 atten = saturate(__powapprox((dist * -lgt.OneOverRangeSq) + 1, lgt.fallOff));	// 1 instruct - NOT ANYMORE !
	//float4 attenSq = atten * atten;	//Dampen attenuation a little more. Help remove circular edge from light.

	float4 light = dX * normal.xxxx;
	light = dY * normal.yyyy + light;
	light = dZ * normal.zzzz + light;
	light *= atten; // 4 instructions
	light = saturate(light * recip);

	// apply to colour
	float3	col = float3(	dot(lgt.colR, light),
							dot(lgt.colG, light),
							dot(lgt.colB, light) );	// 3
	return col;
}

float3 CalculateSpecDiffuseFastLight4ConstantsForClouds(float3 pos, float3 normal)
{
	FastLight4	lgt;
	#if __SHADERMODEL >= 40
		lgt.posX = float4(gLightPositionAndInvDistSqr[0].x, gLightPositionAndInvDistSqr[1].x, gLightPositionAndInvDistSqr[2].x, gLightPositionAndInvDistSqr[3].x);
		lgt.posY = float4(gLightPositionAndInvDistSqr[0].y, gLightPositionAndInvDistSqr[1].y, gLightPositionAndInvDistSqr[2].y, gLightPositionAndInvDistSqr[3].y);
		lgt.posZ = float4(gLightPositionAndInvDistSqr[0].z, gLightPositionAndInvDistSqr[1].z, gLightPositionAndInvDistSqr[2].z, gLightPositionAndInvDistSqr[3].z);
		lgt.OneOverRangeSq = float4(gLightPositionAndInvDistSqr[0].w, gLightPositionAndInvDistSqr[1].w, gLightPositionAndInvDistSqr[2].w, gLightPositionAndInvDistSqr[3].w);
		lgt.colR = float4(gLightColourAndCapsuleExtent[0].r, gLightColourAndCapsuleExtent[1].r, gLightColourAndCapsuleExtent[2].r, gLightColourAndCapsuleExtent[3].r);
		lgt.colG = float4(gLightColourAndCapsuleExtent[0].g, gLightColourAndCapsuleExtent[1].g, gLightColourAndCapsuleExtent[2].g, gLightColourAndCapsuleExtent[3].g);
		lgt.colB = float4(gLightColourAndCapsuleExtent[0].b, gLightColourAndCapsuleExtent[1].b, gLightColourAndCapsuleExtent[2].b, gLightColourAndCapsuleExtent[3].b);
		lgt.fallOff = float4(gLightDirectionAndFalloffExponent[0].w, gLightDirectionAndFalloffExponent[1].w, gLightDirectionAndFalloffExponent[2].w, gLightDirectionAndFalloffExponent[3].w);
	#else
		lgt.posX = gLight0PosX;
		lgt.posY = gLight0PosY;
		lgt.posZ = gLight0PosZ;
		lgt.colR = gLight0ColR;
		lgt.colG = gLight0ColG;
		lgt.colB = gLight0ColB;
		lgt.fallOff = gLight0FalloffExponent;
		lgt.OneOverRangeSq = gLight0InvDistSqr;
	#endif
	return CalculateDiffuseFastLight4ForClouds(pos, normal, lgt);
}

//----------------------------------------------------------------------------------------------------
//Global function brought over from RDR. See if I can eventually use GTA tech for similar effect.
//
float3 HemisphericLightingParams(float3 Normal, float HemiIntensity, float3 SkyColor, float3 GroundColor, float3 EastMinusWestColor, float3 WestColor)
{
	// Hemispheric lighting
#if 0
	Normal.xy = saturate(Normal.xy * 0.5 + 0.5);
	float3 Light = GroundColor + Normal.y * (SkyColor - GroundColor) + WestColor + Normal.x * (EastColor - WestColor);
#else

	float3 Light =  WestColor + saturate(Normal.x * 0.5 + 0.5) * (EastMinusWestColor);
	
	// don't want full wrap, it washes everything out too much and we end up with way too much bounce on the tops of the clouds
	const float GroundWrap = 0.25;
	const float SkyWrap	   = 0.75; 

// 	Light += GroundColor * saturate((GroundWrap-Normal.y)/(1+GroundWrap));
// 	Light += SkyColor    * saturate((SkyWrap+Normal.y)/(1+SkyWrap));
// same thing as above, but saves a register on the ps3
	Light += GroundColor * saturate(-Normal.y/(1+GroundWrap) + GroundWrap/(1+GroundWrap));
	Light += SkyColor    * saturate(Normal.y/(1+SkyWrap) + SkyWrap/(1+SkyWrap));

#endif
	return (HemiIntensity * Light); 
}

#define tex2DGamma(sampler, texCoords) tex2D(sampler,texCoords)
// float4 tex2DGamma(sampler2D sampler0, float2 texCoords)
// {
// 	float4 color = tex2D(texCoords, sampler0);
// 	return color;
// }
//
//End Global RDR Functions
//----------------------------------------------------------------------------------------------------

float3 CalculateAllDirectionalWithShadowsForClouds(float3 Normal, float HemisphericIntensity)
{
	float3 Fill = HemisphericLightingParams(Normal.xzy, HemisphericIntensity, gSkyColor, gBounceColor, gEastMinusWestColor, gWestColor);

	//Compute wrap lighting
	float wrapLight = saturate(dot(Normal, MAIN_LIGHT_DIRECTION) * gWrapLighting_MSAARef[0] + gWrapLighting_MSAARef[1]);

	float3 DiffuseLighting = MAIN_LIGHT_COLOR * wrapLight;
 	DiffuseLighting *= gScaleDiffuseFillAmbient.x;

	return(Fill + DiffuseLighting);
}


//------------------------------------------------------------------------------
//	Henyey-Greenstein\Mie Scattering
//		g = scattering constant. Can not be -1 or 1.
//		phaseK = (3 * (1 - g^2)) / (2 * (2 + g^2))
//		phase = phaseK * (1.0f + cosTheta^2) / (1 + g^2 - (2 * g * cosTheta))^(3/2)
//		Note: For Mie aerosol scattering, Use g = [-.75, -.999)
//
//	Classic rayleigh uses g = 0.0f. Simplifies down to:
//		result = (0.75f * (1.0f + ragePow(phase, 2.0f)));
//

//Mie\Henyey-Greenstein Scattering
float CalculateMieScattering(float3 view, float3 lightDir)
{
	// Fake light scattering inside the cloud = (L dot N)^2 [signed]
	float cosTheta = dot(view, lightDir);
	float cosThetaSq = cosTheta * cosTheta;

	float phase = (1.0f + cosThetaSq) / ragePow((1.0f + gScatterG_GSquared_PhaseMult_Scale.y - (2.0f * gScatterG_GSquared_PhaseMult_Scale.x * cosTheta)), 1.5f);
	phase *= gScatterG_GSquared_PhaseMult_Scale.z;

	return phase;
}

////////////////////////
//Soft Particle Computations

float4 ComputePartialPiercingValues(float3 view)
{
	float4 pierceComponents;

	//pierceA:
	pierceComponents.w = ragePow(saturate(dot(view, MAIN_LIGHT_DIRECTION)), gPiercingLightPower_Strength_NormalStrength_Thickness.x);

	//pierceB:
	float3 p = -view;
	float3 x = dot(p, MAIN_LIGHT_DIRECTION) * MAIN_LIGHT_DIRECTION;
	pierceComponents.xyz = normalize(p - x);

	return pierceComponents;
}

float3 ComputePiercingLight(float4 pierceComponents, float3 Normal, float thickness)
{
	//Compute piercing light
	float pierceB = saturate(dot(Normal, pierceComponents.xyz)) * gPiercingLightPower_Strength_NormalStrength_Thickness.z;

	// Mix the two piercing lights
	float pierceC = lerp(pierceB * pierceComponents.w, pierceComponents.w, pierceComponents.w);
	float3 piercingLight = pierceC * thickness * MAIN_LIGHT_COLOR * gPiercingLightPower_Strength_NormalStrength_Thickness.y;

	return piercingLight;
}

//All pixel shader version. Slow.
float3 ComputePiercingLight(float3 view, float3 Normal, float thickness)
{
	return ComputePiercingLight(ComputePartialPiercingValues(view), Normal, thickness);
}

////////////////////////
//Soft Particle Computations

float GetZDepth(float2 screenPosition, sampler2D DepthMap)
{
	float depth = fixupGBufferDepth(tex2D(DepthMap, screenPosition).x);
	return depth;
}

////////////////////////
//Converts projected depth into view space
//
// From the projection matrix, you get the following equation for Zview from Zproj:
// Q = far / (far - near)
// Zview = -(near * Q) /  (Zproj -Q)
//
// If you substitute equation for Q into above eq., you get:
// Zview = -(near * far) /  (Zproj * (far * near) - far)
//
// I am using the 1st equation, and as an optimization, Q and -(near * Q) are stored
// in a constant.
float ConvertZDepthToViewDepth(float zDepth)
{
	float Q = gNearFarQMult.z;
	float NegNearQ = gNearFarQMult.w;

	float viewDepth = NegNearQ / (zDepth - Q);
	return viewDepth;
}

float ComputeSoftParticle(float2 screenSpaceUV, float viewDepth, out float zViewDepth)
{
	float zDepth = GetZDepth(screenSpaceUV, DepthMapTexSampler); //DepthMapSampler?
	zViewDepth = ConvertZDepthToViewDepth(zDepth);

	//That function actually has alpha computation commented out... so let's do it ourselves.
	//(skip fade if we're testing against the far clip plane or silhouettes on ps3)
	float depthDiff = (zDepth>=(__PS3 ? (16777212.0f/16777216.0f): 1.0f)) ? gSoftParticleRange : zViewDepth - viewDepth;
	depthDiff = depthDiff / gSoftParticleRange;

	
#if USE_MSAA
	const float Tolerance = 1.0f;
	const float refVal = 0.5f;

#	define USE_MSAA_DDXY 0
#	if USE_MSAA_DDXY
		//Get partial derivative of depthDiff..
		float2 ddxy = float2(ddx(depthDiff), ddy(depthDiff));

		if(depthDiff < -Tolerance)
		{
			float2 nextDepthDiff = ddxy + depthDiff.xx;
			float2 isNextDepthDiffValid = step(0.0f.xx, nextDepthDiff);
			float sumValid = dot(nextDepthDiff, isNextDepthDiffValid);
			float numValid = isNextDepthDiffValid.x + isNextDepthDiffValid.y;

			depthDiff = lerp(refVal, sumValid / numValid, saturate(numValid));
		}
#	else //USE_MSAA_DDXY
		if (depthDiff < -Tolerance)
		{
			depthDiff = gWrapLighting_MSAARef.z;
		}
#	endif //USE_MSAA_DDXY
#endif //USE_MSAA

	float softAlpha = saturate(depthDiff);
	return softAlpha;
}


// we need a full normalize normal for the density blending to work
// we really should be using dxt5 normalmaps for clouds.
float3 LookUpFullNormal(sampler2D sampler0, float2 texcoord0)
{
	float3 normal = tex2D_NormalMap(sampler0, texcoord0.xy)*2-1;
	return float3(normal.xy,(sqrt(saturate(1-dot(normal.xy,normal.xy)))));
}


void ComputeCloudDensityAndNormal(out float density, out float3 normal, float2 uv1, float2 uv2, float2 uv3, bool isAnimEnabled, float2 densityAdjust)
{
	if(isAnimEnabled)
	{
		float3 animDensity;
		animDensity.x = tex2D(DensitySampler, uv1).g;
		animDensity.y = tex2D(DetailDensitySampler, uv2).g;
		animDensity.z = tex2D(DetailDensity2Sampler, uv3).g;
		animDensity = 1.0f - animDensity*animDensity;

		float3 normalMap1 = LookUpFullNormal(NormalSampler, uv1);
		float3 normalMap2 = LookUpFullNormal(DetailNormalSampler, uv2);
		float3 normalMap3 = LookUpFullNormal(DetailNormal2Sampler, uv3);

		//Blend in anim weights
		animDensity *= gAnimBlendWeights;

		//Seperate density into anim modes
		float3 combineDensity = animDensity * gAnimCombine;
		float3 sculptDensity = animDensity * gAnimSculpt;

		//Scale normals by combine densities
		normalMap1 = normalMap1 * combineDensity.x;
		normalMap2 = normalMap2 * combineDensity.y;
		normalMap3 = normalMap3 * combineDensity.z;

		//Compute combined and sculpted density
		float combinedDensity = densityAdjust.x * max( combineDensity.x, max(combineDensity.y, combineDensity.z));
		float sculptedDensity =  densityAdjust.y + sculptDensity.x + sculptDensity.y + sculptDensity.z;

		//Final computations for animated density and normal
		density = combinedDensity - ((1.0f - combinedDensity) * sculptedDensity);
		normal = normalize(normalMap1 + normalMap2 + normalMap3);
	}
	else
	{
//		density = (1.0f - tex2DGamma(DensitySampler, uv1).g)*densityAdjust.x;
//		density -= ((1.0f - density) * densityAdjust.y);
		density = 1.0f - tex2DGamma(DensitySampler, uv1).g;
		normal = LookUpFullNormal(NormalSampler, uv1);
	}
}

void ComputeCloudDensity(out float density, float2 uv1, float2 uv2, float2 uv3, bool isAnimEnabled, float2 densityAdjust)
{
	if(isAnimEnabled)
	{
		float3 animDensity;
		animDensity.x = tex2D(DensitySampler, uv1).g;
		animDensity.y = tex2D(DetailDensitySampler, uv2).g;
		animDensity.z = tex2D(DetailDensity2Sampler, uv3).g;
		animDensity = 1.0f - animDensity*animDensity;

		//Blend in anim weights
		animDensity *= gAnimBlendWeights;

		//Seperate density into anim modes
		float3 combineDensity = animDensity * gAnimCombine;
		float3 sculptDensity = animDensity * gAnimSculpt;

		//Compute combined and sculpted density
		float combinedDensity = densityAdjust.x * max( combineDensity.x, max(combineDensity.y, combineDensity.z));
		float sculptedDensity =  densityAdjust.y + sculptDensity.x + sculptDensity.y + sculptDensity.z;

		//Final computations for animated density
		density = combinedDensity - ((1.0f - combinedDensity) * sculptedDensity);
	}
	else
	{
//		density = (1.0f - tex2DGamma(DensitySampler, uv1).g)*densityAdjust.x;
//		density -= ((1.0f - density) * densityAdjust.y);
		density = 1.0f - tex2DGamma(DensitySampler, uv1).g;
	}
}



//--------------------------------------------------------------------------------------------------------------------------------------------------//



////////////////////////////////////////////////////////////////////////////////////
// Cloud shader input/output structures.

struct VS_INPUT_COMMON
{
	float4 Position		:	POSITION;
	float4 Color		:	COLOR0;     // a is cloud alpha. r is amount of haze to blend with the cloud. g is the cloud density combine scale. b is the density sculpt adjust.
	float3 Normal		:	NORMAL;
	float2 TexCoord0	:	TEXCOORD0;
	float4 Tangent		:	TANGENT;
};

#ifdef USE_BILLBOARD    // modify the input for billboards, follow same UV coord conventions as treeLod2D  
	struct VS_INPUT
	{
		float3 Position		:	POSITION;
		float4 Color		:	COLOR0;		// a is cloud alpha. r is amount of haze to blend with the cloud.  g is the cloud density combine scale. b is the density sculpt adjust.
		float4 TexCoord0	:	TEXCOORD0;
		float2 TexCoord1	:	TEXCOORD1;
	};
#else
	#define VS_INPUT VS_INPUT_COMMON
#endif


struct VS_OUTPUT
{
	float4 EyeRay		:	TEXCOORD0;		// eyeray.xyz is normalized, w is length
	float4 Normal		:	TEXCOORD1;		// w is cloud alpha value.
	float4 Tangent		:	TEXCOORD2;		// w is cloud density combine scale
	float4 Binormal		:	TEXCOORD3;		// w is clouds density sculpt adjust
	float2 TexCoord		:	TEXCOORD4;
	float4 TexCoord2	:	TEXCOORD5;
	float4 ScreenPos	:	TEXCOORD6;
	float4 Piercing		:	TEXCOORD7;
	float3 Scattering	:	TEXCOORD8;
	float4 FogData		:	TEXCOORD9;		// fog/haze color and alpha

	DECLARE_POSITION_CLIPPLANES(Position)
};

struct VS_OUTPUT_OCCLUDER
{
	DECLARE_POSITION(Position)
	float4 Color		:	COLOR0;
	float2 TexCoord		:	TEXCOORD1;
	float4 TexCoord2	:	TEXCOORD2;
};

struct VS_OUTPUT_FOG
{
	DECLARE_POSITION(Position)
	float4 EyeRay		:	TEXCOORD0;		// eyeray.xyz is normalized, w is length
	float4 Normal		:	TEXCOORD1;		// w is cloud alpha value.
	float4 Tangent		:	TEXCOORD2;		// w is cloud density combine scale
	float4 Binormal		:	TEXCOORD3;		// w is clouds density sculpt adjust
	float2 TexCoord		:	TEXCOORD4;
	float4 ScreenPos	:	TEXCOORD5;
	float4 FogData		:	TEXCOORD6;		// fog/haze color and alpha
};

struct VS_OUTPUT_ENV
{
	DECLARE_POSITION(Position)
	float4 Normal		:	TEXCOORD1;		// w is cloud alpha value.
	float4 Tangent		:	TEXCOORD2;		// w cloud density combine scale
	float4 Binormal		:	TEXCOORD3;		// w is clouds density sculpt adjust
	float2 TexCoord		:	TEXCOORD4;
	float4 Piercing		:	TEXCOORD7;
	float3 Scattering	:	TEXCOORD8;
	float4 FogData		:	TEXCOORD9;		// fog/haze color and alpha
};

#if !defined(PS_OUTPUT)
#	define PS_OUTPUT half4
#endif //PS_OUTPUT

#define SETUP_CLOUD_STENCIL		ZWriteEnable	 = false; \
								ZFunc			 = FixedupLESSEQUAL; \
								StencilEnable    = true; \
								StencilPass      = keep; \
								StencilFail      = keep; \
								StencilZFail     = keep; \
								StencilFunc      = equal; \
								StencilRef       = DEFERRED_MATERIAL_CLEAR; \
								StencilMask      = 0x7; \
								StencilWriteMask = 0x0; 


//--------------------------------------------------------------------------------------------------------------------------------------------------//
#define USE_ATMO_AFTER_HAZE 0

float4 CalcCloudFogData(float3 eyeRay, float hazeAlpha)
{	
	float4 fogData = CalcFogDataNoHaze(eyeRay);
	
	fogData.w = max(fogData.w,hazeAlpha);   // Owen wanted to be able to control some of the fogging with the cloudshat cpv

    //NOTE: globalFogHazeAtFarClip = saturate(1-exp(-globalFogHazeDensity*globalFogFarClip))*globalFogHazeAlpha;
	hazeAlpha *= globalFogHazeAtFarClip;// compute haze at far clip
	
	#if USE_ATMO_AFTER_HAZE
		// pre calc parts of  Lerp(Lerp(color,Haze,hazeAlpha),FogColor,FogVal);
		fogData = float4(globalFogColorHaze*(hazeAlpha)*(1-fogData.w)+fogData.rgb*(fogData.a), (1-hazeAlpha)*(1-fogData.w));				
	#else
		// pre calc parts of  Lerp(Lerp(color,FogColor,FogVal),Haze,hazeAlpha);
		fogData = float4(fogData.rgb*(fogData.a)*(1-hazeAlpha)+globalFogColorHaze*(hazeAlpha), (1-fogData.w)*(1-hazeAlpha));				
	#endif

	return fogData;
}

#ifdef USE_BILLBOARD
VS_INPUT_COMMON ConvertToCommonInputs(VS_INPUT IN)
{
	VS_INPUT_COMMON OUT;
	float2 uvs			= IN.TexCoord0.xy - float2(0.5f,0.5f);
	float2 modelScale	= IN.TexCoord1.xy;

	float radiusW = cloudBillboardWidth*modelScale.x;	
	float radiusH = cloudBillboardHeight*modelScale.y;	

	float3 up		=  gViewInverse[0].xyz * radiusW; 
	float3 across	= -gViewInverse[1].xyz * radiusH;
	float3 offset	= IN.Position.xyz + uvs.x*up + uvs.y*across;   // offset from middle of "mesh of 4-in-1's";
	
	float3 IN_pos;
	IN_pos.x = dot(offset, gWorld[0].xyz);	// inverse local rotation - it will be re-applied by wvp matrix
	IN_pos.y = dot(offset, gWorld[1].xyz);
	IN_pos.z = dot(offset, gWorld[2].xyz);

	OUT.Position		= float4(IN_pos,1);					
	OUT.Normal			= gViewInverse[2].xyz;   // don't bother converting normal and tangent back to object space, we can skip the transform later.	
	OUT.Tangent         = gViewInverse[0].xyzw;
	
	OUT.Color			= IN.Color;
	OUT.TexCoord0		= IN.TexCoord0.zw;

	return(OUT);
}
#else
	#define ConvertToCommonInputs(X) X   // no conversion necessary
#endif // USE_BILLBOARD...


////////////////////////////////////////////////////////////////////////////////////
// Cloud common shaders

//*************************************************
//Vertex Shaders
VS_OUTPUT VSCloudsCommon(VS_INPUT IN_COMMON, bool useVertScattering, bool useVertPiercing, bool pullInVertsFromFarClip, bool infiniteClouds)
{
	VS_OUTPUT	OUT;

	VS_INPUT_COMMON IN = ConvertToCommonInputs(IN_COMMON);

	OUT.EyeRay.xyz  = mul(IN.Position, gWorld).xyz;  // the world matrix is adjusted to camera relative.
	OUT.EyeRay.w	= length(OUT.EyeRay.xyz);

#if __SHADERMODEL >=40
	//On PC we need to use the actual worldviewprojection to calculate the clipping planes.
	//If we use the cloudViewProj the clip plane appears offset and gets worse the deeper in the water you go.
	OUT.Position     = mul(float4(IN.Position.xyz, 1.0f), gWorldViewProj);
	RAGE_COMPUTE_CLIP_DISTANCES(OUT.Position);
#endif

	OUT.Position     = mul(float4(OUT.EyeRay.xyz, 1.0f), gCloudViewProj); // the viewproj has the  camera position subtracted out

	// if infinity clouds, or normal clouds that are not supposed to cross the far clip plane,
	// adjust the z to it in one, but does not cross the far clip plane
#if SUPPORT_INVERTED_PROJECTION 
	if (infiniteClouds)
	{
		OUT.Position.z = cloudUsesInverseZProjection ? 0.0f : OUT.Position.w;	// for infinite clouds, set depth to the far clip plane
	}
	else if (pullInVertsFromFarClip && OUT.Position.w>0)
	{
		if (cloudUsesInverseZProjection)
		{
			if (OUT.Position.z<0)
				OUT.Position.z = 0.1f/OUT.Position.w;							// for non infinite clouds, pull depth in slightly from the far clip plane for soft clouds silhouette layer
		}
		else
		{
			if (OUT.Position.z>OUT.Position.w)
				OUT.Position.z = OUT.Position.w - 0.1f;							// for non infinite clouds, pull depth in slightly from the far clip plane for soft clouds silhouette layer
		}
	}
#else
	if (infiniteClouds || (pullInVertsFromFarClip && OUT.Position.w>0 && OUT.Position.z>OUT.Position.w))
		OUT.Position.z = OUT.Position.w - ((infiniteClouds)? 0.0f:0.1f);	// for non infinite clouds, pull depth in slightly from the far clip plane for soft clouds silhouette layer
#endif

	OUT.ScreenPos    = rageCalcScreenSpacePosition(MonoToStereoClipSpace(OUT.Position));

	OUT.FogData		 = CalcCloudFogData(OUT.EyeRay.xyz, (1-IN.Color.r));

	OUT.EyeRay.xyz   = OUT.EyeRay.xyz/OUT.EyeRay.w;		// normalize the ray so pixel shaders don't have to
	
	OUT.Normal.a	 = IN.Color.a;   // Normal.a has the cloud's alpha value
	OUT.Tangent.a	 = IN.Color.g;   // Tangent.a has the cloud's density value
	OUT.Binormal.a   = IN.Color.b;	 // Binormal.a has the cloud's density sculpt adjust

	OUT.Normal.xyz   = IN.Normal;
	OUT.Tangent.xyz	 = IN.Tangent.xyz;
	OUT.Binormal.xyz = rageComputeBinormal(IN.Normal,IN.Tangent.xyzw);

#ifndef USE_BILLBOARD
	OUT.Normal.xyz   = normalize(mul(OUT.Normal.xyz,  (float3x3)gWorld));
	OUT.Tangent.xyz	 = normalize(mul(OUT.Tangent.xyz, (float3x3)gWorld));
	OUT.Binormal.xyz = normalize(mul(OUT.Binormal,    (float3x3)gWorld));
#endif

#if defined(USE_CAMERA_UV_SCROLL) // we use the  gUVOffset[1].zw for a scoll that counters the camera movement
	OUT.TexCoord     = ((IN.TexCoord0 * gRescaleUV1) + gUVOffset1) + gUVOffset[1].zw + saturate(gUVOffset[0].xy-1000);  // access to [0] entry to keep variable in order.
	OUT.TexCoord2.xy = ((IN.TexCoord0 * gRescaleUV2) + gUVOffset2) + gUVOffset[1].zw;
	OUT.TexCoord2.zw = ((IN.TexCoord0 * gRescaleUV3) + gUVOffset3) + gUVOffset[1].zw;
#else
	OUT.TexCoord     = ((IN.TexCoord0 * gRescaleUV1) + gUVOffset1) + gUVOffset[0].xy*cloudLayerAnimScale1;
	OUT.TexCoord2.xy = ((IN.TexCoord0 * gRescaleUV2) + gUVOffset2) + gUVOffset[0].zw*cloudLayerAnimScale2;
	OUT.TexCoord2.zw = ((IN.TexCoord0 * gRescaleUV3) + gUVOffset3) + gUVOffset[1].xy*cloudLayerAnimScale3;
#endif

	OUT.Scattering = 0.0f.xxx;
	OUT.Piercing = 0.0f.xxxx;

	if(useVertScattering || useVertPiercing)
	{
		if(useVertScattering)
			OUT.Scattering = CalculateMieScattering(OUT.EyeRay.xyz, MAIN_LIGHT_DIRECTION) * MAIN_LIGHT_COLOR * gScatterG_GSquared_PhaseMult_Scale.w;

		if(useVertPiercing)
			OUT.Piercing = ComputePartialPiercingValues(OUT.EyeRay.xyz);
	}

	return(OUT);
}


VS_OUTPUT_FOG VSCloudsFog(VS_INPUT IN_COMMON)
{
	VS_OUTPUT NON_FOG_OUT = VSCloudsCommon(IN_COMMON, false, false, false, false);
	
	VS_OUTPUT_FOG OUT;
	OUT.Position	= NON_FOG_OUT.Position;
	OUT.FogData		= NON_FOG_OUT.FogData;
	OUT.EyeRay		= NON_FOG_OUT.EyeRay;
	OUT.Normal		= NON_FOG_OUT.Normal;
	OUT.Tangent		= NON_FOG_OUT.Tangent;
	OUT.Binormal	= NON_FOG_OUT.Binormal;
	OUT.TexCoord	= NON_FOG_OUT.TexCoord;
	OUT.ScreenPos	= NON_FOG_OUT.ScreenPos;

	return OUT;
}



VS_OUTPUT_ENV VSCloudEnvCommon(VS_INPUT IN_COMMON, out float4 EyeRay)
{
	VS_OUTPUT NON_FOG_OUT = VSCloudsCommon(IN_COMMON, true, true, false, false);

	VS_OUTPUT_ENV OUT;
	OUT.Position	= NON_FOG_OUT.Position;
	OUT.FogData		= NON_FOG_OUT.FogData;
	OUT.Normal		= NON_FOG_OUT.Normal;
	OUT.Tangent		= NON_FOG_OUT.Tangent;
	OUT.Binormal	= NON_FOG_OUT.Binormal;
	OUT.TexCoord	= NON_FOG_OUT.TexCoord;
	OUT.Piercing	= NON_FOG_OUT.Piercing;
	OUT.Scattering	= NON_FOG_OUT.Scattering;
	EyeRay          = NON_FOG_OUT.EyeRay;

	return OUT;
}

VS_OUTPUT_ENV VSCloudEnv(VS_INPUT IN_COMMON)
{
	float4 EyeRay;
	return VSCloudEnvCommon(IN_COMMON, EyeRay);
}


VS_OUTPUT_OCCLUDER VSCloudsOccluder(VS_INPUT IN_COMMON)
{
	VS_OUTPUT COMMON_OUT = VSCloudsCommon(IN_COMMON, false,  false,  true, false);
	VS_OUTPUT_OCCLUDER OUT;
	
	OUT.Position	= COMMON_OUT.Position;
	OUT.Color		= IN_COMMON.Color;
	OUT.TexCoord	= COMMON_OUT.TexCoord;
	OUT.TexCoord2	= COMMON_OUT.TexCoord2;

	return(OUT);
}


#define DEBUG_NORMALMAPS(color) (color)
//#define DEBUG_NORMALMAPS(color) (saturate(color.rgba-10000)+ float4(Normal.xz*.5+.5,0,color.a))
//#define DEBUG_NORMALMAPS(color) (saturate(color.rgba-10000)+ float4(Normal.zz*.5+.5,0,color.a))
//#define DEBUG_NORMALMAPS(color) (saturate(color.rgba-10000)+ float4(Normal.xx*.5+.5,0,color.a))
//#define DEBUG_NORMALMAPS(color) (saturate(color.rgba-10000)+ float4(normalMap.xy*.5+.5,0,color.a))

//*************************************************
//Pixel shaders

// NOTE: PSCloudUnlitCommon() this is only used by the Max shaders...
PS_OUTPUT PSCloudUnlitCommon(VS_OUTPUT IN, bool isAnimEnabled, bool isSoftParticlEnabled)
{
	float3 screenPos = IN.ScreenPos.xyz / IN.ScreenPos.w;
	float viewDepth = IN.ScreenPos.w;
	float density = 0.0f;
	float3 normalMap = 0.0f.xxx;

	//Compute the final density and normal.
	ComputeCloudDensityAndNormal(density, normalMap, IN.TexCoord.xy, IN.TexCoord2.xy, IN.TexCoord2.zw, isAnimEnabled, float2(IN.Tangent.a,IN.Binormal.a));
	density = saturate((density - gDensityShiftScale.x) * gDensityShiftScale.y);
	float alpha = density * IN.Normal.a; // Cloud's density is our alpha.

#if __PS3
	float worldZ = IN.EyeRay.z*IN.EyeRay.w + gCameraPos.z;
	alpha = (worldZ < 0) ? 0 : alpha;  // sadly, ps3 can't use a user clip plane to cut off the clouds underwater, so we hack this here
#endif

	//Transform normal from tangent space
	float3x3 BasisMat = float3x3(IN.Tangent.xyz, IN.Binormal.xyz, IN.Normal.xyz);
	float3 Normal = mul(normalMap, BasisMat);

	if(isSoftParticlEnabled)
	{
		//Compute soft alpha
		float zViewDepth;
		float softAlpha = ComputeSoftParticle(screenPos.xy, viewDepth, zViewDepth);
		alpha *= (gValidDepthBuffer!=0.) ? softAlpha : 1;
	}

	//Compute final color.
	float4 Color = float4(gCloudColor, alpha);

#if	__XENON
	Color.a *= gInvColorExpBias;
#endif

	// blend in fog/haze
	Color.rgb  = Color.rgb*IN.FogData.a + IN.FogData.rgb;

	Color = DEBUG_NORMALMAPS(Color);

	return PackHdr(Color);
}

PS_OUTPUT PSCloudCommon(VS_OUTPUT IN, bool isAnimEnabled, bool isSoftParticlEnabled, bool useVertScattering, bool useVertPiercing, bool useDynamicLights)
{
	float3 screenPos = IN.ScreenPos.xyz / IN.ScreenPos.w;
	float3 view = IN.EyeRay.xyz;
	float viewDepth = IN.ScreenPos.w;
	float density = 0.0f;
	float3 normalMap = 0.0f.xxx;
	float3 worldPos = IN.EyeRay.xyz*IN.EyeRay.w + gCameraPos.xyz;

	//Compute the final density and normal.
	ComputeCloudDensityAndNormal(density, normalMap, IN.TexCoord.xy, IN.TexCoord2.xy, IN.TexCoord2.zw, isAnimEnabled, float2(IN.Tangent.a,IN.Binormal.a));
	density = saturate((density - gDensityShiftScale.x) * gDensityShiftScale.y);
	float alpha = density * IN.Normal.a; // Cloud's density is our alpha.

#if __PS3
	alpha = (worldPos.z < 0) ? 0 : alpha;  // sadly, ps3 can't use a user clip plane to cut off the clouds underwater, so we hack this here
#endif

	//Transform normal from tangent space
	float3x3 BasisMat = float3x3(IN.Tangent.xyz, IN.Binormal.xyz, IN.Normal.xyz);
	float3 Normal =  mul(normalMap, BasisMat);

	if(isSoftParticlEnabled)
	{
		//Compute soft alpha
		float zViewDepth;
		float softAlpha = ComputeSoftParticle(screenPos.xy, viewDepth, zViewDepth);
		alpha *= (gValidDepthBuffer!=0.0 || !useDynamicLights) ? softAlpha : 1;    // we use a technique switch for the non dynamic light versions, so we can skip the check
	}

	//Calculate Cloud Diffuse Lighting
	float3 Diffuse = CalculateAllDirectionalWithShadowsForClouds(Normal, gScaleDiffuseFillAmbient.y);
	float3 dynLights = float3(0.0f.xxx);
	if(useDynamicLights)
	{
		dynLights = CalculateSpecDiffuseFastLight4ConstantsForClouds(worldPos, Normal);
	}

	//Compute thickness (For scattering and Piercing)
	float thickness = 1.0f - density;
	thickness = saturate((1.0f - gPiercingLightPower_Strength_NormalStrength_Thickness.w) + (thickness * gPiercingLightPower_Strength_NormalStrength_Thickness.w));

	float3 Scattering;
	if(useVertScattering)
		Scattering = IN.Scattering * thickness;
	else
		Scattering = CalculateMieScattering(view, MAIN_LIGHT_DIRECTION) * MAIN_LIGHT_COLOR * gScatterG_GSquared_PhaseMult_Scale.w * thickness;

	float3 piercingLight;
	if(useVertPiercing)
		piercingLight = ComputePiercingLight(IN.Piercing, Normal, thickness);
	else
		piercingLight = ComputePiercingLight(view, Normal, thickness);

	//Keep for now. This is an attempt to help dull the piercing effect and bring up the alpha behind it so animations are consistent.
	//float3 scatterPierce = Scattering + piercingLight;
	//float scatterPiecePow = (scatterPierce.x + scatterPierce.y + scatterPierce.z) / 1.0f;
	//alpha = ragePow(alpha, max(scatterPiecePow, ***SomeUserDefinedPower***));

	//Compute final color.
	float4 Color = float4(gCloudColor, alpha);
	Color.rgb *= Diffuse + (gAmbientColor * gScaleDiffuseFillAmbient.z) + (dynLights * alpha);
	Color.rgb += Scattering;
	Color.rgb += piercingLight;

	// blend in fog/haze
	Color.rgb  = Color.rgb*IN.FogData.a + IN.FogData.rgb;

#if	__XENON
	// dither for 360, 10 bits just bands too much
	const float ditherIntensity = 0.0002;
	float2 ditherCoords = float2((abs(view.x)-abs(view.y)),view.z)*16;
	Color.rgb += tex2D(DitherSampler,ditherCoords).g*ditherIntensity-0.5*ditherIntensity;

	Color.a *= gInvColorExpBias;
#endif

	Color = DEBUG_NORMALMAPS(Color);

	return Color;
}

PS_OUTPUT PSCloudFogCommon(VS_OUTPUT_FOG IN)
{
	float3 screenPos = IN.ScreenPos.xyz / IN.ScreenPos.w;
	float viewDepth = IN.ScreenPos.w;
	float density = 0.0f;
	float3 normalMap = 0.0f.xxx;

	//Compute the final density and normal.
	ComputeCloudDensityAndNormal(density, normalMap, IN.TexCoord.xy, IN.TexCoord.xy, IN.TexCoord.xy, false, float2(IN.Tangent.a,IN.Binormal.a));
	density = saturate((density - gDensityShiftScale.x) * gDensityShiftScale.y);
	float alpha = density * IN.Normal.a; // Cloud's density is our alpha.

#if __PS3
	float worldZ = IN.EyeRay.z*IN.EyeRay.w + gCameraPos.z;
	alpha = (worldZ < 0) ? 0 : alpha;  // sadly, ps3 can't use a user clip plane to cut off the clouds underwater, so we hack this here
#endif

	//Transform normal from tangent space
	float3x3 BasisMat = float3x3(IN.Tangent.xyz, IN.Binormal.xyz, IN.Normal.xyz);
	float3 Normal = mul(normalMap, BasisMat);

	//Compute soft alpha
	float zViewDepth;
	float softAlpha = ComputeSoftParticle(screenPos.xy, viewDepth, zViewDepth);
	alpha *= (gValidDepthBuffer!=0.0) ? softAlpha : 1;

	//Calculate Cloud Diffuse Lighting
	float3 Diffuse = CalculateAllDirectionalWithShadowsForClouds(Normal, gScaleDiffuseFillAmbient.y);

	//Compute final color.
	float4 Color = float4(gCloudColor, alpha);
	Color.rgb *= Diffuse + (gAmbientColor * gScaleDiffuseFillAmbient.z);
		
	// blend in fog/haze
	Color.rgb  = Color.rgb*IN.FogData.a + IN.FogData.rgb;

#if	__XENON
	Color.a *= gInvColorExpBias;
#endif
	
	Color = DEBUG_NORMALMAPS(Color);

	return Color;
}

PS_OUTPUT PSCloudFog(VS_OUTPUT_FOG IN) : COLOR
{
	return PackColor(PSCloudFogCommon(IN));
}

PS_OUTPUT PSCloudFogWater(VS_OUTPUT_FOG IN) : COLOR
{
	return PackReflection(PSCloudFogCommon(IN));
}

PS_OUTPUT PSCloudEnv(VS_OUTPUT_ENV IN, bool useLowResTextures)
{
	//Compute the final density and normal.
	float Density;
	float3 NormalMap;
	if(useLowResTextures)
	{
		Density = 1.0 - tex2DGamma(LowResDensitySampler, IN.TexCoord.xy).g;
		NormalMap = LookUpFullNormal(LowResNormalSampler, IN.TexCoord.xy);
	}
	else
	{
		Density = 1.0 - tex2DGamma(DensitySampler, IN.TexCoord.xy).g;
		NormalMap = LookUpFullNormal(NormalSampler, IN.TexCoord.xy);
	}
	Density = saturate((Density - gDensityShiftScale.x) * gDensityShiftScale.y * IN.Tangent.a);
	float Alpha = Density * IN.Normal.a;	// Cloud's density is our alpha.

	//Transform normal from tangent space
	float3x3 BasisMat = float3x3(IN.Tangent.xyz, IN.Binormal.xyz, IN.Normal.xyz);
	float3 Normal = mul(NormalMap, BasisMat);

	// Calculate Cloud Diffuse Lighting
	float3 Diffuse = CalculateAllDirectionalWithShadowsForClouds(Normal, gScaleDiffuseFillAmbient.y);
	float3 DynLights = float3(0.0f.xxx);

	// Compute thickness (For scattering and Piercing)
	float Thickness = 1.0 - Density;
	Thickness = saturate((1.0 - gPiercingLightPower_Strength_NormalStrength_Thickness.w) + (Thickness * gPiercingLightPower_Strength_NormalStrength_Thickness.w));
	float3 Scattering = IN.Scattering * Thickness;
	float3 PiercingLight = ComputePiercingLight(IN.Piercing, Normal, Thickness);

	// Compute final color.
	float4 Color = float4(gCloudColor, Alpha);
	Color.rgb *= Diffuse + (gAmbientColor * gScaleDiffuseFillAmbient.z) + (DynLights * Alpha);
	Color.rgb += Scattering;
	Color.rgb += PiercingLight;

	//See below where used... this was just being undone for some reason. So rather than computing this then undoing the computation, just don't do this in the 1st place.
// #if	__XENON
// 	Color.a *= gInvColorExpBias;
// #endif
	
	// blend in fog/haze
	Color.rgb  = Color.rgb*IN.FogData.a + IN.FogData.rgb;

	return PackHdr(Color);
}



PS_OUTPUT PSCloudsOccluderCommon(VS_OUTPUT_OCCLUDER IN, bool isAnimEnabled)
{
	float density = 0.0f;
	ComputeCloudDensity(density, IN.TexCoord.xy, IN.TexCoord2.xy, IN.TexCoord2.zw, isAnimEnabled,IN.Color.gb);
	density = saturate((density - gDensityShiftScale.x) * gDensityShiftScale.y);

	float thickness = 1.0f - density;
	thickness = saturate((1.0f - gPiercingLightPower_Strength_NormalStrength_Thickness.w) + (thickness * gPiercingLightPower_Strength_NormalStrength_Thickness.w));

	return PackHdr(float4(0,0,0, saturate(1.0-thickness*12) * IN.Color.a));
}



//--------------------------------------------------------------------------------------------------------------------------------------------------//



////////////////////////////////////////////////////////////////////////////////////
// Cloud specifc shaders

// VS_OUTPUT VSCloudsPixScatterPiercing(VS_INPUT IN)
// {
// 	return VSCloudsCommon(IN, false, false, false, false);
// }

VS_OUTPUT VSCloudsVertPiercing(VS_INPUT IN)
{
	return VSCloudsCommon(IN, false, true, true, false);
}

// VS_OUTPUT VSCloudsVertScatter(VS_INPUT IN)
// {
// 	return VSCloudsCommon(IN, true, false, false, false);
// }

VS_OUTPUT VSCloudsVertScatterPiercing(VS_INPUT IN)
{
	return VSCloudsCommon(IN, true, true, true, false);
}

VS_OUTPUT VSCloudsVertScatterPiercing_Infinite(VS_INPUT IN)
{
	VS_OUTPUT OUT = VSCloudsCommon(IN, true, true, false, true);
	return OUT;
}


//EnvMap
VS_OUTPUT_ENV VSCloudsEnvMapParab(VS_INPUT IN)
{
	float4	eyeRay;
	VS_OUTPUT_ENV OUT = VSCloudEnvCommon(IN, eyeRay);

	OUT.Position = DualParaboloidPosTransform(eyeRay.xyz*eyeRay.w);

	return(OUT);
}


//Pixel shaders:
//No Animations
PS_OUTPUT PSCloudsVertScatterPiercingCommon(VS_OUTPUT IN, bool isAnimEnabled, bool isSoftParticlEnabled)
{
	return PSCloudCommon(IN, isAnimEnabled, isSoftParticlEnabled, true, true, false);
}

PS_OUTPUT PSCloudsVertScatterPiercing(VS_OUTPUT IN) : COLOR
{
	return PackHdr(PSCloudsVertScatterPiercingCommon(IN, false, false));
}

PS_OUTPUT PSCloudsVertScatterPiercingWater(VS_OUTPUT IN) : COLOR
{
	return PackReflection(PSCloudsVertScatterPiercingCommon(IN, false, false));
}


PS_OUTPUT PSCloudsVertScatterPiercing_Soft(VS_OUTPUT IN) : COLOR
{
	return PackHdr(PSCloudsVertScatterPiercingCommon(IN, false, true));
}

PS_OUTPUT PSCloudsVertScatterPiercing_Anim(VS_OUTPUT IN) : COLOR
{
	return PackHdr(PSCloudsVertScatterPiercingCommon(IN, true, false));
}

PS_OUTPUT PSCloudsVertScatterPiercing_AnimSoft(VS_OUTPUT IN) : COLOR
{
	return PackHdr(PSCloudsVertScatterPiercingCommon(IN, true, true));
}

PS_OUTPUT PSCloudsVertScatterPiercing_AnimWater(VS_OUTPUT IN) : COLOR
{
	return PackReflection(PSCloudsVertScatterPiercingCommon(IN, true, true));
}


//Cheap
PS_OUTPUT PSClouds_Cheap(VS_OUTPUT IN) : COLOR
{
	return PackHdr(PSCloudsVertScatterPiercingCommon(IN, false, false));
}

//With Lightning
PS_OUTPUT PSCloudsVertScatterPiercingLightningCommon(VS_OUTPUT IN, bool isAnimEnabled, bool isSoftParticlEnabled)
{
	return PSCloudCommon(IN, isAnimEnabled, isSoftParticlEnabled, true, true, true);
}

PS_OUTPUT PSCloudsVertScatterPiercingLightning(VS_OUTPUT IN) : COLOR
{
	return PackHdr(PSCloudsVertScatterPiercingLightningCommon(IN, false, false));
}

PS_OUTPUT PSCloudsVertScatterPiercingLightning_Soft(VS_OUTPUT IN) : COLOR
{
	return PackHdr(PSCloudsVertScatterPiercingLightningCommon(IN, false, true));
}

PS_OUTPUT PSCloudsVertScatterPiercingLightning_Anim(VS_OUTPUT IN) : COLOR
{
	return PackHdr(PSCloudsVertScatterPiercingLightningCommon(IN, true, false));
}

PS_OUTPUT PSCloudsVertScatterPiercingLightning_AnimSoft(VS_OUTPUT IN) : COLOR
{
	return PackHdr(PSCloudsVertScatterPiercingLightningCommon(IN, true, true));
}

//Cheap
PS_OUTPUT PSCloudsLightning_Cheap(VS_OUTPUT IN) : COLOR
{
	return PackHdr(PSCloudsVertScatterPiercingLightningCommon(IN, false, false));
}

//Occluder
PS_OUTPUT PSCloudsOccluder(VS_OUTPUT_OCCLUDER IN) : COLOR
{
	return PSCloudsOccluderCommon(IN, false);
}

PS_OUTPUT PSCloudsOccluder_Anim(VS_OUTPUT_OCCLUDER IN) : COLOR
{
	return PSCloudsOccluderCommon(IN, true);
}


//EnvMap
PS_OUTPUT PSCloudsEnvMapParab_Base(VS_OUTPUT_ENV IN, bool useLowResTextures)
{
	return PSCloudEnv(IN, useLowResTextures);
}

PS_OUTPUT PSCloudsEnvMapParab(VS_OUTPUT_ENV IN) : COLOR
{
	return PSCloudsEnvMapParab_Base(IN, false);
}

PS_OUTPUT PSCloudsEnvMapParab_LowRes(VS_OUTPUT_ENV IN) : COLOR
{
	return PSCloudsEnvMapParab_Base(IN, true);
}

//
// Max functions
//
#if __MAX
VS_OUTPUT VS_MaxTransform(maxVertexInput IN)
{
VS_INPUT maxIN=(VS_INPUT)0;
		 
	maxIN.Position = float4(IN.pos, 1.0f);
	maxIN.TexCoord0.xy = Max2RageTexCoord2(IN.texCoord0.xy);

#ifdef USE_BILLBOARD
	maxIN.TexCoord0.zw = Max2RageTexCoord2(IN.texCoord1.xy);
	maxIN.TexCoord1.xy = Max2RageTexCoord2(IN.texCoord2.xy);
#else
	maxIN.Normal  = IN.normal;
	maxIN.Tangent = float4(IN.tangent, 1.0f);
#endif

	return VSCloudsVertScatterPiercing(maxIN);
}

float4 PS_MaxClouds_common(VS_OUTPUT IN, bool isAnimEnabled)
{
	float4 OUT;

	if(maxLightingToggle)
	{
		OUT = PSCloudsVertScatterPiercingCommon(IN, isAnimEnabled, false); //PSCloudsVertScatterPiercing_Anim(IN);
	}
	else
	{
		OUT = PSCloudUnlitCommon(IN, isAnimEnabled, false);//PSCloudsUnlit_Anim(IN);
	}

	return saturate(OUT);
}

float4 PS_MaxClouds(VS_OUTPUT IN) : COLOR
{
	return PS_MaxClouds_common(IN, false);
}

float4 PS_MaxClouds_Anim(VS_OUTPUT IN) : COLOR
{
	return PS_MaxClouds_common(IN, false);
}
#endif //__MAX...


#if __MAX
#	ifndef CLOUDS_NO_MAX_TECHNIQUE
	// ===============================
	// Tool technique
	// ===============================
	technique tool_draw
	{
		pass P0
		{
			MAX_TOOL_TECHNIQUE_RS
			VertexShader= compile VERTEXSHADER	VS_MaxTransform();
#		ifdef MAX_USES_CLOUD_ANIM_TECH
			PixelShader	= compile PIXELSHADER	PS_MaxClouds_Anim();
#		else //MAX_USES_CLOUD_ANIM_TECH
			PixelShader	= compile PIXELSHADER	PS_MaxClouds();
#		endif //MAX_USES_CLOUD_ANIM_TECH
		}  
	}
#	endif //CLOUDS_NO_MAX_TECHNIQUE
#else //__MAX...

	//That's it! You define the technique.

#	ifndef CLOUDS_NO_REFLECTION_TECHNIQUE
		//Well, maybe 2 techniques...
		technique reflection_draw
		{
			pass p0
			{
				VertexShader = compile VERTEXSHADER VSCloudsVertScatterPiercing_Infinite();
				PixelShader = compile PIXELSHADER PSClouds_Cheap() CGC_FLAGS(CGC_DEFAULTFLAGS);
			}
		}

		technique reflectionlightning_draw
		{
			pass p0
			{
				VertexShader = compile VERTEXSHADER VSCloudsVertScatterPiercing_Infinite();
				PixelShader = compile PIXELSHADER PSCloudsLightning_Cheap() CGC_FLAGS("-texformat d RGBA8 -po OutColorPrec=fp16");
			}
		}
#	endif
#endif	//__MAX...
