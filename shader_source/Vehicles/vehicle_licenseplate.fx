#ifndef PRAGMA_DCL
	#pragma dcl position diffuse texcoord0 texcoord1 normal tangent0
	#define PRAGMA_DCL
#endif
//
// vehicle_licenceplate.fx shader;
//
// 2010/10/23 - initial;
//
//
//
#define USE_SPECULAR
#define IGNORE_SPECULAR_MAP		// We can add support for a spec map later if needed, but currently this isn't necessary.
#define USE_NORMAL_MAP
#define ALLOW_NORMAL_MAP_STRIPPING
#define USE_FORWARD_CLOUD_SHADOWS

#define USE_DIRT_LAYER
#define USE_DIRT_UV_LAYER		// dirt 2nd channel
#define USE_VEHICLE_DAMAGE
#define USE_SUPERSAMPLING

#define VEHCONST_SPECULAR1_FALLOFF			(500.0f)
#define VEHCONST_SPECULAR1_INTENSITY		(0.2f)
#define VEHCONST_FRESNEL					(0.98f)

#include "vehicle_common.fxh"

#if __WIN32PC || RSG_ORBIS
	#pragma constant 130 //To fix out of constant registers compiler error on windows build. Might need to change if we make a PC version.
#endif

BeginSampler(sampler2D, PlateBgTex, PlateBgSampler, PlateBgTex)
	string UIName = "Plate Diffuse";
	WIN32PC_DONTSTRIP
	string TCPTemplate = "Diffuse";
	string	TextureType="Diffuse";
ContinueSampler(sampler2D, PlateBgTex, PlateBgSampler, PlateBgTex)
	AddressU = DIFFUSE_CLAMP;
	AddressV = DIFFUSE_CLAMP;
	AddressW = DIFFUSE_CLAMP;
	MIPFILTER = MIPLINEAR;
	#if defined(USE_ANISOTROPIC)
		MINFILTER = MINANISOTROPIC;
		MAGFILTER = MAGLINEAR; 
		ANISOTROPIC_LEVEL(4)
	#else
		MINFILTER = HQLINEAR;
		MAGFILTER = MAGLINEAR; 
	#endif
	#ifdef MIPMAP_LOD_BIAS
		LOD_BIAS = MIPMAP_LOD_BIAS;
	#endif
EndSampler;

BeginSampler(sampler2D, PlateBgBumpTex, PlateBgBumpSampler, PlateBgBumpTex)
	string UIName="Plate Bump Map";
	string UIHint="normalmap";
	WIN32PC_DONTSTRIP
	string TCPTemplate="NormalMap";
	string	TextureType="Bump";
ContinueSampler(sampler2D, PlateBgBumpTex, PlateBgBumpSampler, PlateBgBumpTex)
	AddressU  = WRAP;        
	AddressV  = WRAP;
	AddressW  = WRAP;
	MIPFILTER = MIPLINEAR;
	MINFILTER = HQLINEAR;
	MAGFILTER = MAGLINEAR;
EndSampler;


BeginSampler(sampler2D, FontTexture, FontSampler, FontTexture)
	string UIName = "Font Texture Array";
	WIN32PC_DONTSTRIP
	string TCPTemplateRelative = "maps_other/DistanceMap.tcp";
	int TCPAllowOverride = 0;
	string	TextureType="Distance";
ContinueSampler(sampler2D, FontTexture, FontSampler, FontTexture)
	#if defined(USE_ANISOTROPIC)
		MINFILTER = MINANISOTROPIC;
		MAGFILTER = MAGLINEAR; 
		ANISOTROPIC_LEVEL(4)
	#else
		MINFILTER = HQLINEAR;
		MAGFILTER = MAGLINEAR; 
	#endif
	MipFilter = MIPLINEAR;
	AddressU  = CLAMP;
	AddressV  = CLAMP;
EndSampler;

BeginSampler(sampler2D, FontNormalMap, FontNormalSampler, FontNormalMap)
	string UIName = "Font Bump Map Array";
	WIN32PC_DONTSTRIP
	string TCPTemplate = "NormalMap";
	string	TextureType="Bump";
ContinueSampler(sampler2D, FontNormalMap, FontNormalSampler, FontNormalMap)
	#if HACK_GTA4 && defined(USE_ANISOTROPIC)
		MINFILTER = MINANISOTROPIC;
		MAGFILTER = MAGLINEAR; 
		ANISOTROPIC_LEVEL(4)
	#else
		MINFILTER = HQLINEAR;
		MAGFILTER = MAGLINEAR; 
	#endif
	MipFilter = MIPLINEAR;
	AddressU  = CLAMP;
	AddressV  = CLAMP;
EndSampler;

BEGIN_RAGE_CONSTANT_BUFFER(vehicle_licenseplate_locals,b0)
float4 LetterIndex1 : LetterIndex1
<
	string UIName = "Letters Index 1 (1-4)";
	float UIMin = 0.0;
	float UIMax = 255.0;
	float UIStep = 1.0;
> = float4(10, 21, 10, 23);

float4 LetterIndex2 : LetterIndex2
<
	string UIName = "Letters Index 2 (5-8)";
	float UIMin = 0.0;
	float UIMax = 255.0;
	float UIStep = 1.0;
> = float4(63, 63, 62, 0);

//Default values updated to reflect dimentions of new font atlas (distance map)
float2 LetterSize : LetterSize
<
	string UIName = "Letter Size (glyphWidthHeight.xy / texDictWidthHeight.xy)";
	float UIMin = 0.0;
	float UIMax = 1.0;
	float UIStep = 0.1;
> = float2(1.0f / 16.0f, 1.0f / 4.0f); //float2(32.0f / 256.0f, 32.0f / 256.0f);

float2 NumLetters : NumLetters
<
	string UIName = "Num Letters (in font texture dictionary)";
	float UIMin = 0.0;
	float UIMax = 255.0;
	float UIStep = 1.0;
> = float2(16.0f, 4.0f);

float4 LicensePlateFontExtents : LicensePlateFontExtents
<
	string UIName = "License Plate Font UV Extents (XY = Top Left Corner, ZW = Bottom Right Corner)";
	float UIMin = 0.0;
	float UIMax = 1.0;
> = float4(0.043f, 0.38f, 0.945f, 0.841f);

#define LicensePlateMaxLetters float2(8.0f,1.0f)

float4 LicensePlateFontTint : LicensePlateFontTint
<
	string UIName = "License Plate Font Tint Color (alpha selects tint or font texture color)";
> = float4(1.0f, 1.0f, 1.0f, 0.0f);

float FontNormalScale : FontNormalScale
<
	string UIName = "Letter Bumpiness Scale";
	float UIMin = 0.0;
	float UIMax = 100.0;
	float UIStep = 0.1;
> = 1.0f;


//Distance Map Variables
#define USE_DISTANCE_MAP 1
#define USE_FONT_OUTLINE 0

float DistMapCenterVal : DistMapCenterVal
<
	string UIName = "Center/Alpha Test Value for Distance Map";
	float UIMin = 0.0;
	float UIMax = 1.0;
	float UIStep = 0.01;
> = 0.5f;

float4 DistEpsilonScaleMin = float4(10.0f, 10.0f, 0.0f, 0.0f);

float3 FontOutlineMinMaxDepthEnabled : FontOutlineMinMaxDepthEnabled
<
	string UIName = "Font Outline Start\Width, Font Outline - Enabled";
	float UIMin = 0.0;
	float UIMax = 1.0;
	float UIStep = 0.01;
> = float3(0.475f, 0.5, 0.0f);

float3 FontOutlineColor : FontOutlineColor
<
	string UIName = "Font Outline Color";
> = float3(0.0f, 0.0f, 0.0f);

EndConstantBufferDX10( vehicle_licenseplate_locals )

// ----------------------------------------------------------------------------------------------- //
// - Overloaded GetSurfaceProperties that take pre-sampled diffuse and bump values. This allows 
// - your shader to use custom sampling techniques before calling this function.
// - 
// - NOTE: Does not work correctly if PARALLAX or WRINKLE_MAP are defined.
// ----------------------------------------------------------------------------------------------- //

SurfaceProperties GetLicensePlateSurfaceProperties(	
	float4 IN_baseColor,
#if PALETTE_TINT
	float4 IN_baseColorTint,
#endif
#if DIFFUSE_TEXTURE
	float2 IN_diffuseTexCoord,
#endif
	float4 IN_diffuseColor,
#if DIFFUSE2
	float2 IN_diffuseTexCoord2,
	sampler2D IN_diffuseSampler2,
#endif
#if SPEC_MAP
	float2 IN_specularTexCoord,
	sampler2D IN_specularSampler,
#endif	// SPEC_MAP
#if REFLECT
	float3 IN_worldEyePos,
	REFLECT_SAMPLER	IN_environmentSampler,
#endif // REFLECT 
#if NORMAL_MAP
	float2 IN_bumpTexCoord,
	float3 IN_bumpColor,
	float3 IN_worldTangent,
	float3 IN_worldBinormal,
#endif	// NORMAL_MAP
#if DIRT_NORMALMAP
	float3 IN_dirtBumpUV,		// xy = UV, z = dirtLevel
	sampler2D IN_dirtBumpSampler,
#endif//DIRT_NORMALMAP...
	float3 IN_worldNormal)
{
	SurfaceProperties OUT = (SurfaceProperties)0;

	OUT.surface_baseColor		= IN_baseColor;	

#if PALETTE_TINT
	OUT.surface_baseColorTint	= IN_baseColorTint;
#endif

#if DIFFUSE2
	OUT.surface_overlayColor = tex2D(IN_diffuseSampler2, IN_diffuseTexCoord2);	
#endif	// DIFFUSE2

	OUT.surface_diffuseColor = calculateDiffuseColor(IN_diffuseColor);

#if NORMAL_MAP
	float3 packedNormalHeight = CalculatePackedNormal(
		IN_bumpColor,
		#if PARALLAX
			parallaxPackedNormalHeight,
		#endif
		#if DIRT_NORMALMAP
			IN_dirtBumpSampler,
			IN_dirtBumpUV.xyz,
		#endif
		IN_bumpTexCoord.xy);

		OUT.surface_worldNormal.xyz = CalculateWorldNormal(
			packedNormalHeight.xy, 
			bumpiness, 
			IN_worldTangent, 
			IN_worldBinormal, 
			IN_worldNormal);
#else
	//should be normalized as the normal is interpolated from vertex normals
	OUT.surface_worldNormal.xyz = normalize(IN_worldNormal.xyz); 
#endif

#ifdef DECAL_USE_NORMAL_MAP_ALPHA
	OUT.surface_worldNormal.a = packedNormalHeight.z;
#endif

#if REFLECT
	float3 surfaceToEyeDir = normalize(IN_worldEyePos);	
#endif

#if SPECULAR
	CalculateSpecularContribution(
		OUT,
		#if SPEC_MAP
			IN_specularSampler,
			IN_specularTexCoord.xy,
		#endif
		IN_baseColor,
		OUT.surface_worldNormal);
#endif // SPECULAR

#if REFLECT
	CalculateReflection(
		OUT,
		IN_environmentSampler,
		#if REFLECT_MIRROR
			float4(0,0,0,1), // not supported
			#if defined(USE_MIRROR_CRACK_MAP)
				float2(0,0),
			#endif
		#endif
		#if SPECULAR
			OUT.surface_specularExponent,
		#endif
		surfaceToEyeDir,
		OUT.surface_worldNormal);
#endif // REFLECT

#ifdef COLORIZE
	OUT.surface_diffuseColor.rgba*=colorize;
#endif // COLORIZE

#if SELF_SHADOW
	OUT.surface_selfShadow = 1.0f;
#endif // SELF_SHADOW

	return OUT;
}

//-----------------------------------------------------------------------
// Pixel Shaders:

// Helper function to sample normal maps in dxt5 format:
float3 tex2D_NormalMap_Grad(sampler2D sampler0, float2 texcoord0, float2 ddx, float2 ddy)
{
	float4 nrm_dxt5 = tex2Dgrad(sampler0, texcoord0.xy, ddx, ddy);
	return( DecompressNormalDXT5(nrm_dxt5) );	
}

void ComputeCombinedDiffuseAndNormal(out float4 diffuse, out float3 normal, float2 uv)
{
	float4 diffuseSample = tex2D(PlateBgSampler, uv);
	float3 normalSample = tex2D_NormalMap(PlateBgBumpSampler, uv);

	float2 i;
	float2 fontAreaSize = LicensePlateFontExtents.zw - LicensePlateFontExtents.xy;
	float2 maxLetters = LicensePlateMaxLetters / fontAreaSize;

	float2 baseUv = modf(saturate(uv - LicensePlateFontExtents.xy) * maxLetters, i);
	baseUv *= LetterSize;

	int letterIdx = (int)i.x;
	float4 indexSel1 = letterIdx.xxxx == int4(0, 1, 2, 3);
	float4 indexSel2 = letterIdx.xxxx == int4(4, 5, 6, 7);
	int currLetter = dot(LetterIndex1, indexSel1) + dot(LetterIndex2, indexSel2);

	float fFrac;
	float f = modf(currLetter / NumLetters.x, fFrac);
	float2 letterCenter = float2(f, fFrac / NumLetters.y);
	baseUv += letterCenter;

	//float2 aMod = saturate( (abs(ddx(baseUv)) * 10.0f + abs(ddy(baseUv)) * 10.0f) - 0.1f);
	float2 gradX = ddx(uv) * 0.5f;
	float2 gradY = ddy(uv) * 0.5f;
	float4 fontColor = tex2Dgrad(FontSampler, baseUv, gradX, gradY);
	float3 fontNormal = tex2D_NormalMap_Grad(FontNormalSampler, baseUv, gradX, gradY);
	
	fontColor.a = fontColor.r; 
	fontColor.rgb = 0.0f;

	//Force font to have no effect if we are out of the text bounds.
	float4 outOfBoundCheck = float4(uv < LicensePlateFontExtents.xy, uv > LicensePlateFontExtents.zw);
	float oobLerp = saturate(dot(outOfBoundCheck, outOfBoundCheck));
	fontColor = lerp(fontColor, 0.0f.xxxx, oobLerp);
	fontNormal = lerp(fontNormal, 0.0f.xxx, oobLerp);

	fontColor.rgb = lerp(fontColor.rgb, LicensePlateFontTint.rgb, LicensePlateFontTint.a);

#if USE_DISTANCE_MAP
	float DistCenter = DistMapCenterVal;

	//This would work the same as a straight forward alpha test.
	//fontColor.a = lerp(1.0f, 0.0f, float(fontColor.a < DistCenter));

	float2 ddxyMax = max(abs(gradX), abs(gradY));
	float2 DistEpsilon = max(ddxyMax.x, ddxyMax.y).xx * DistEpsilonScaleMin.xy;
	DistEpsilon = max(DistEpsilon, DistEpsilonScaleMin.zw); //Clamp the min epsilon for softer edges?

	#if USE_FONT_OUTLINE
	{
		if(FontOutlineMinMaxDepthEnabled.z >= 0.5f)
		{
			float3 FontOutlineColor = float3(0.0f, 0.0f, 0.0f);
			float2 OutlineMaxSoftEdge = float2(FontOutlineMinMaxDepthEnabled.y - DistEpsilon.x, FontOutlineMinMaxDepthEnabled.y + DistEpsilon.y);
			float outlineFactor = smoothstep(OutlineMaxSoftEdge.x, OutlineMaxSoftEdge.y, fontColor.a);
			
			//Interpolate color
			fontColor.rgb = lerp(FontOutlineColor, fontColor.rgb, outlineFactor);

			//Update DistCenter so outline isn't alpha tested out.
			DistCenter = FontOutlineMinMaxDepthEnabled.x;
		}
	}
	#endif //USE_FONT_OUTLINE

	float2 DistMinMax = float2(DistCenter - DistEpsilon.x, DistCenter + DistEpsilon.y);
	fontColor.a = smoothstep(DistMinMax.x, DistMinMax.y, fontColor.a);
#endif //USE_DISTANCE_MAP

	//Combine diffuse and normal samples
	diffuse = float4(lerp(diffuseSample.rgb, fontColor.rgb, fontColor.a), diffuseSample.a);

	normal = normalSample + ((fontNormal - 0.5f) * FontNormalScale)* fontColor.a;
}

half4 PS_PlateUnlit(vehicleVertexOutputUnlit IN) : COLOR
{
	float4 color = PS_GetBaseColorFromCPV(IN.color0);
	float4 diffuseSample = 0.0f.xxxx;
	float3 normalSample = float3(0.5f, 0.5f, 1.0f);
	ComputeCombinedDiffuseAndNormal(diffuseSample, normalSample, IN.texCoord.xy);

    float4 outColor = diffuseSample;	
	//body color modulation (diffuse color):
	outColor	*= float4(matDiffuseColor, 1.0f);
	outColor	*= color;
	outColor.a	*= globalAlpha * gInvColorExpBias;

#if DIRT_LAYER
	#if DIRT_UV
		float2 dirtUV = DIRT_PS_INPUT;
	#else
		float2 dirtUV = DIFFUSE_PS_INPUT;
	#endif
	float dirtSpecularMult;
	outColor = CalculateDirtLayer(outColor, dirtUV, dirtLevel, IN.color0.b, IN.color0.a, dirtSpecularMult);
#endif

	return PackColor(outColor);
}// end of PS_VehicleTexturedUnlit()...

float4 LicensePlateForward(vehicleVertexOutput IN, int numLights, bool directionalLightAndShadow, bool bUseWaterRefractionFog)
{
#if __WIN32PC && (__SHADERMODEL < 40)
	// easy "fix" for "out-of-registers" dx9 compiler error
	// (happens when VEHCONST_* consts became variables again):
	vehicleVertexOutputUnlit unlitIN;
	unlitIN.pos				= IN.pos;
	unlitIN.texCoord		= IN.texCoord;
	unlitIN.color0			= IN.color0;
	#if DIRT_UV
		unlitIN.texCoord3.xy= IN.texCoord3.xy;
	#endif
	return PS_PlateUnlit(unlitIN);
#else //__WIN32PC && (__SHADERMODEL < 40)

	float4 diffuseSample = 0.0f.xxxx;
	float3 normalSample = float3(0.5f, 0.5f, 1.0f);
	ComputeCombinedDiffuseAndNormal(diffuseSample, normalSample, IN.texCoord.xy);

	//-----------------
	//GTA lighting:

#if DIRT_LAYER
	#if DIRT_UV
		float2 dirtUV = DIRT_PS_INPUT;
	#else
		float2 dirtUV = DIFFUSE_PS_INPUT;
	#endif
#endif

	SurfaceProperties surfaceInfo = GetLicensePlateSurfaceProperties(
		PS_GetBaseColorFromCPV(IN.color0),
#if DIFFUSE_TEXTURE
		IN.texCoord.xy, //(float2)DIFFUSE_PS_INPUT * DiffuseTexTileUV,
		diffuseSample,
#endif
#if DIFFUSE2
		(float2)DIFFUSE2_PS_INPUT,
		DiffuseSampler2,
#endif	// DIFFUSE2
#if SPEC_MAP
		(float2)SPECULAR_PS_INPUT*specTexTileUV,
		SpecSampler,
#endif	// SPEC_MAP
#if REFLECT
		(float3)IN.worldEyePos,
		ReflectionSampler,
#endif	// REFLECT
#if NORMAL_MAP
		IN.texCoord.xy, //(float2)DIFFUSE_PS_INPUT,
		normalSample,
		(float3)IN.worldTangent,
		(float3)IN.worldBinormal,
#endif	// NORMAL_MAP
#if DIRT_NORMALMAP
		float3(dirtUV.x, dirtUV.y, dirtLevel),
		DirtBumpSampler,
#endif//DIRT_NORMALMAP...
		(float3)IN.worldNormal
		);

	float3 matColD = vehicleComputeMaterialDiffuseColor(IN.color0, 0, 0);

	//Fill out the standard lighting properties structure.	
	StandardLightingProperties surfaceStandardLightingProperties =
		vehicleDeriveLightingPropertiesForCommonSurface( surfaceInfo,
#if VEHICLESHADER_SNOW
		true, SNOW_PS_INPUTD,
#if VEHICLESHADER_SNOW_DIRTUV
		(float2)DIRT_PS_INPUT
#else
		(float2)DIFFUSE_PS_INPUT
#endif
#else
		false, 0, float2(0,0)
#endif
		,matColD
		);

#if DIRT_LAYER
	float dirtSpecularMult;
	surfaceStandardLightingProperties.diffuseColor = CalculateDirtLayer(surfaceStandardLightingProperties.diffuseColor, dirtUV, dirtLevel, IN.color0.b, IN.color0.a, dirtSpecularMult);
#if SPECULAR
	surfaceStandardLightingProperties.specularIntensity *= dirtSpecularMult;
#endif
#endif
	surfaceStandardLightingProperties.diffuseColor = ProcessDiffuseColor(surfaceStandardLightingProperties.diffuseColor);

	float4 OUT = float4(0.0, 0.0, 0.0, 0.0);

	surfaceProperties surface;
	directionalLightProperties light;
	materialProperties material;

	populateForwardLightingStructs(
		surface,
		material,
		light,
		IN.worldPos.xyz,
		surfaceInfo.surface_worldNormal.xyz,
		surfaceStandardLightingProperties);

	OUT = calculateForwardLighting(
		numLights,
		true,
		surface,
		material,
		light,
		directionalLightAndShadow,
			SHADOW_RECEIVING,
			false, // directionalShadowHighQuality
		IN.pos.xy * gooScreenSize.xy);

	OUT.a		*= globalAlpha * gInvColorExpBias;

#if __SHADERMODEL >= 40
	 if(bUseWaterRefractionFog)
	{
		const float waterFogOpacity = pow(WaterParamsTexture.Load(int3(IN.pos.xy, 0)).x, 2);
		const float linearDepth = dot(IN.worldEyePos, gViewInverse[2].xyz);// getLinearGBufferDepth(IN.pos.z, gVehicleWaterProjParams);
		float waterSurfaceLinearDepth = getLinearGBufferDepth(WaterDepthTexture.Load(int3(IN.pos.xy, 0)).r, gVehicleWaterProjParams);
		const float waterDepthDiff = linearDepth - waterSurfaceLinearDepth;
		//Using rageDiscard here doesnt compile on Durango, so just returning all zeros
		if(waterDepthDiff<=0.0f)
		{
			return float4(0,0,0,0);
		}
		half2 depthBlend = saturate(GetWaterDepthBlend(waterDepthDiff, waterFogOpacity, 0));
		OUT.a *= saturate(depthBlend.x);
	}
#endif //__SHADERMODEL >= 40...

	 return OUT;
#endif //__WIN32PC && (__SHADERMODEL < 40)
}

half4 PS_Plate_Zero(vehicleVertexOutput IN): COLOR
{
	return PackHdr(LicensePlateForward(IN, 0, USE_DIRECTIONAL_LIGHT_FWD_0_LIGHTS, false));
}

half4 PS_Plate_Four(vehicleVertexOutput IN): COLOR
{
	return PackHdr(LicensePlateForward(IN, 4, USE_DIRECTIONAL_LIGHT_FWD_4_OR_8_LIGHTS, false));
}

half4 PS_Plate_Eight(vehicleVertexOutput IN): COLOR
{
	return PackHdr(LicensePlateForward(IN, 8, USE_DIRECTIONAL_LIGHT_FWD_4_OR_8_LIGHTS, false));
}

half4 PS_Plate_Zero_WaterRefraction(vehicleVertexOutput IN): COLOR
{
	return PackHdr(LicensePlateForward(IN, 0, USE_DIRECTIONAL_LIGHT_FWD_0_LIGHTS, true));
}

half4 PS_Plate_Four_WaterRefraction(vehicleVertexOutput IN): COLOR
{
	return PackHdr(LicensePlateForward(IN, 4, USE_DIRECTIONAL_LIGHT_FWD_4_OR_8_LIGHTS, true));
}

half4 PS_Plate_Eight_WaterRefraction(vehicleVertexOutput IN): COLOR
{
	return PackHdr(LicensePlateForward(IN, 8, USE_DIRECTIONAL_LIGHT_FWD_4_OR_8_LIGHTS, true));
}


//Deferred shader
DeferredGBuffer PS_PlateDeferred(vehicleVertexOutputD IN)
{
DeferredGBuffer OUT;

	float4 diffuseSample = 0.0f.xxxx;
	float3 normalSample = float3(0.5f, 0.5f, 1.0f);
	ComputeCombinedDiffuseAndNormal(diffuseSample, normalSample, IN.texCoord.xy);

	//-----------------
	//GTA deferred lighting:

#if DIRT_LAYER
	#if DIRT_UV
		float2 dirtUV = DIRT_PS_INPUT;
	#else
		float2 dirtUV = DIFFUSE_PS_INPUT;
	#endif
#endif

	SurfaceProperties surfaceInfo = GetLicensePlateSurfaceProperties(
		PS_GetBaseColorFromCPV(IN.color0),
#if DIFFUSE_TEXTURE
		IN.texCoord.xy, //(float2)DIFFUSE_PS_INPUT * DiffuseTexTileUV,
		diffuseSample,
#endif
#if DIFFUSE2
		(float2)DIFFUSE2_PS_INPUT,
		DiffuseSampler2,
#endif	// DIFFUSE2
#if SPEC_MAP
		(float2)SPECULAR_PS_INPUT*specTexTileUV,
		SpecSampler,
#endif	// SPEC_MAP
#if REFLECT
		(float3)IN.worldEyePos,
		ReflectionSampler,
#endif	// REFLECT
#if NORMAL_MAP
		IN.texCoord.xy, //(float2)DIFFUSE_PS_INPUT,
		normalSample,
		(float3)IN.worldTangent,
		(float3)IN.worldBinormal,
#endif	// NORMAL_MAP
#if DIRT_NORMALMAP
		float3(dirtUV.x, dirtUV.y, dirtLevel),
		DirtBumpSampler,
#endif//DIRT_NORMALMAP...
		(float3)IN.worldNormal
		);

	float3 matColD = vehicleComputeMaterialDiffuseColor(IN.color0, 0, 0);

	// Compute some surface information.								
	StandardLightingProperties surfaceStandardLightingProperties =
		vehicleDeriveLightingPropertiesForCommonSurface( surfaceInfo,
													#if VEHICLESHADER_SNOW
														true, SNOW_PS_INPUTD,
														#if VEHICLESHADER_SNOW_DIRTUV
															(float2)DIRT_PS_INPUT
														#else
															(float2)DIFFUSE_PS_INPUT
														#endif
													#else
														false, 0, float2(0,0)
													#endif
														,matColD
														);

#if DIRT_LAYER
	float dirtSpecularMult;
	surfaceStandardLightingProperties.diffuseColor = CalculateDirtLayer(surfaceStandardLightingProperties.diffuseColor, dirtUV, dirtLevel, IN.color0.b, IN.color0.a, dirtSpecularMult);
	#if SPECULAR
		surfaceStandardLightingProperties.specularIntensity *= dirtSpecularMult;
	#endif
#endif

	// Pack the information into the GBuffer(s).
	OUT = PackGBuffer(surfaceInfo.surface_worldNormal, surfaceStandardLightingProperties );
	return OUT;
}

//
// special version of PS_PlateDeferred() with extra alpha test applied
// to simulate missing AlphaTest/AlphaToMask while in MRT mode:
//
half4 PS_PlateDeferred_AlphaClip(vehicleVertexOutputD IN) : SV_Target0
{
DeferredGBuffer OUT;
	OUT = PS_PlateDeferred(IN);

	// PS3: there's no AlphaTest in MRT mode, so we simulate AlphaTest functionality here:
	rageDiscard(OUT.col0.a <= vehicle_alphaRef);
	return OUT.col0;
}// end of PS_PlateDeferred_AlphaClip()...

DeferredGBuffer PS_PlateDeferred_SubSampleAlpha(vehicleVertexOutputD IN)
{
DeferredGBuffer OUT;
	OUT = PS_PlateDeferred(IN);

	// PS3: there's no AlphaTest in MRT mode, so we simulate AlphaTest functionality here:
	rageDiscard(OUT.col0.a <= vehicle_alphaRef);

	return OUT;
}// end of PS_PlateDeferred_SubSampleAlpha()...

#if __MAX
//
//
//
//
float4 MaxPlateTextured(vehicleVertexOutput IN)
{
	float4 OUT;

	float4 diffuseSample = 0.0f.xxxx;
	float3 normalSample = float3(0.5f, 0.5f, 1.0f);
	ComputeCombinedDiffuseAndNormal(diffuseSample, normalSample, IN.texCoord.xy);

	//-----------------
	//GTA lighting:

	SurfaceProperties surfaceInfo = GetLicensePlateSurfaceProperties(
		PS_GetBaseColorFromCPV(IN.color0),
#if DIFFUSE_TEXTURE
		IN.texCoord.xy, //(float2)DIFFUSE_PS_INPUT * DiffuseTexTileUV,
		diffuseSample,
#endif
#if DIFFUSE2
		(float2)DIFFUSE2_PS_INPUT,
		DiffuseSampler2,
#endif	// DIFFUSE2
#if SPEC_MAP
		(float2)SPECULAR_PS_INPUT*specTexTileUV,
		SpecSampler,
#endif	// SPEC_MAP
#if REFLECT
		IN.worldEyePos,
		ReflectionSampler,
#endif	// REFLECT
#if NORMAL_MAP
		IN.texCoord.xy, //(float2)DIFFUSE_PS_INPUT,
		normalSample,
		(float3)IN.worldTangent,
		(float3)IN.worldBinormal,
	#if PARALLAX
		(float3)IN.tanEyePos,
	#endif //PARALLAX...
#endif	// NORMAL_MAP
		(float3)IN.worldNormal
		);

	//Fill out the standard lighting properties structure.
	StandardLightingProperties surfaceStandardLightingProperties = DeriveLightingPropertiesForCommonSurface(surfaceInfo);

	surfaceProperties surface;
	directionalLightProperties light;
	materialProperties material;

	populateForwardLightingStructs(
		surface,
		material,
		light,
		IN.worldPos.xyz,
		surfaceInfo.surface_worldNormal.xyz,
		surfaceStandardLightingProperties);

	OUT = calculateForwardLighting(
		0, // numLights
		false,
		surface,
		material,
		light,
		true, // directional
			false, // directionalShadow
			false, // directionalShadowHighQuality
		float2(0,0));

	return float4(sqrt(OUT.rgb), OUT.a); //gamma correction
}

float4 PS_MaxPlateTextured(vehicleVertexOutput IN) : SV_Target
{
	float4 OUT;

	if(maxLightingToggle)
	{
		OUT = MaxPlateTextured(IN);
	}
	else
	{
		vehicleVertexOutputUnlit unlitIN;
		unlitIN.pos					= IN.pos;
		unlitIN.texCoord			= IN.texCoord;
		unlitIN.color0				= IN.color0;
		#if DIRT_UV
			unlitIN.texCoord3.xy	= IN.texCoord3.xy;
		#endif
		OUT = PS_PlateUnlit(unlitIN);
	}

	return saturate(OUT);
}
#endif //__MAX...


////////////////
// Techniques

#if __MAX
	// ===============================
	// Tool technique
	// ===============================
	technique tool_draw
	{
		pass p0
		{
			MAX_TOOL_TECHNIQUE_RS
		#if 0
			// unlit:
			VertexShader= compile VERTEXSHADER	VS_MaxTransformUnlit();
			PixelShader	= compile PIXELSHADER	PS_PlateUnlit();
		#else
			// lit:
			VertexShader= compile VERTEXSHADER	VS_MaxTransform();
			PixelShader	= compile PIXELSHADER	PS_MaxPlateTextured();
		#endif
		}
	}
#else //__MAX...

#define FORWARD_RENDERSTATES() \
	AlphaRef			= 100; \
	AlphaBlendEnable	= true; \
	BlendOp				= ADD; \
	SrcBlend			= SRCALPHA; \
	DestBlend			= INVSRCALPHA; \
	AlphaTestEnable		= true; \
	ColorWriteEnable	= RED+GREEN+BLUE;


#define WATERREFRACTIONALPHA_RENDERSTATES()	\
	AlphaRef			= 100; \
	AlphaBlendEnable	= true; \
	BlendOp				= ADD; \
	SrcBlend			= SRCALPHA; \
	DestBlend			= INVSRCALPHA; \
	AlphaTestEnable		= true; \
	ColorWriteEnable	= RED+GREEN+BLUE;

	#if FORWARD_TECHNIQUES
	technique draw
	{
		pass p0
		{
			FORWARD_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_VehicleTransform(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER PS_Plate_Eight() CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif // FORWARD_TECHNIQUES

	#if FORWARD_TECHNIQUES && DRAWSKINNED_TECHNIQUES
	technique drawskinned
	{
		pass p0
		{
			FORWARD_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_VehicleTransformSkin(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_Plate_Eight()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif // FORWARD_TECHNIQUES && DRAWSKINNED_TECHNIQUES

	#if FORWARD_TECHNIQUES
	technique lightweight0_draw
	{
		pass p0
		{
			FORWARD_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_VehicleTransform(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_Plate_Zero()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif // FORWARD_TECHNIQUES

#if FORWARD_TECHNIQUES
	technique lightweight0WaterRefractionAlpha_draw
	{
		pass p0
		{
			WATERREFRACTIONALPHA_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_VehicleTransform(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_Plate_Zero_WaterRefraction()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
#endif // FORWARD_TECHNIQUES

	#if FORWARD_TECHNIQUES && DRAWSKINNED_TECHNIQUES
	technique lightweight0_drawskinned
	{
		pass p0
		{
			FORWARD_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_VehicleTransformSkin(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_Plate_Zero()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
#endif // FORWARD_TECHNIQUES && DRAWSKINNED_TECHNIQUES

#if FORWARD_TECHNIQUES && DRAWSKINNED_TECHNIQUES
	technique lightweight0WaterRefractionAlpha_drawskinned
	{
		pass p0
		{
			WATERREFRACTIONALPHA_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_VehicleTransformSkin(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_Plate_Zero_WaterRefraction()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
#endif // FORWARD_TECHNIQUES && DRAWSKINNED_TECHNIQUES

#if FORWARD_TECHNIQUES
	technique lightweight4_draw
	{
		pass p0
		{
			FORWARD_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_VehicleTransform(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_Plate_Four()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
#endif // FORWARD_TECHNIQUES

#if FORWARD_TECHNIQUES
	technique lightweight4WaterRefractionAlpha_draw
	{
		pass p0
		{
			WATERREFRACTIONALPHA_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_VehicleTransform(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_Plate_Four_WaterRefraction()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
#endif // FORWARD_TECHNIQUES

#if FORWARD_TECHNIQUES && DRAWSKINNED_TECHNIQUES
	technique lightweight4_drawskinned
	{
		pass p0
		{
			FORWARD_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_VehicleTransformSkin(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_Plate_Four()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
#endif // FORWARD_TECHNIQUES && DRAWSKINNED_TECHNIQUES

#if FORWARD_TECHNIQUES && DRAWSKINNED_TECHNIQUES
	technique lightweight4WaterRefractionAlpha_drawskinned
	{
		pass p0
		{
			WATERREFRACTIONALPHA_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_VehicleTransformSkin(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_Plate_Four_WaterRefraction()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
#endif // FORWARD_TECHNIQUES && DRAWSKINNED_TECHNIQUES

#if FORWARD_TECHNIQUES
	technique lightweight8_draw
	{
		pass p0
		{
			FORWARD_RENDERSTATES()
				VertexShader = compile VERTEXSHADER	VS_VehicleTransform(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_Plate_Eight()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
#endif // FORWARD_TECHNIQUES

#if FORWARD_TECHNIQUES
	technique lightweight8WaterRefractionAlpha_draw
	{
		pass p0
		{
			WATERREFRACTIONALPHA_RENDERSTATES()
				VertexShader = compile VERTEXSHADER	VS_VehicleTransform(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_Plate_Eight_WaterRefraction()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
#endif // FORWARD_TECHNIQUES

#if FORWARD_TECHNIQUES && DRAWSKINNED_TECHNIQUES
	technique lightweight8_drawskinned
	{
		pass p0
		{
			FORWARD_RENDERSTATES()
				VertexShader = compile VERTEXSHADER	VS_VehicleTransformSkin(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_Plate_Eight()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
#endif // FORWARD_TECHNIQUES && DRAWSKINNED_TECHNIQUES

#if FORWARD_TECHNIQUES && DRAWSKINNED_TECHNIQUES
	technique lightweight8WaterRefractionAlpha_drawskinned
	{
		pass p0
		{
			WATERREFRACTIONALPHA_RENDERSTATES()
				VertexShader = compile VERTEXSHADER	VS_VehicleTransformSkin(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_Plate_Four_WaterRefraction()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
#endif // FORWARD_TECHNIQUES && DRAWSKINNED_TECHNIQUES

	// ===============================
	// Deferred techniques
	// ===============================
	#if DEFERRED_TECHNIQUES && NON_CUTOUT_DEFERRED_TECHNIQUES
	technique deferred_draw
	{
		pass p0
		{
			VertexShader = compile VERTEXSHADER	VS_VehicleTransformD(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_PlateDeferred()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif // DEFERRED_TECHNIQUES && NON_CUTOUT_DEFERRED_TECHNIQUES

	#if DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES
	technique deferredalphaclip_draw
	{
		pass p0
		{
			VertexShader = compile VERTEXSHADER	VS_VehicleTransformD(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_PlateDeferred_AlphaClip()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif // DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES

	#if DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES
	technique deferredsubsamplealpha_draw
	{
		pass p0
		{
			VertexShader = compile VERTEXSHADER	VS_VehicleTransformD(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_PlateDeferred_SubSampleAlpha()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif // DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES

	#if DEFERRED_TECHNIQUES && NON_CUTOUT_DEFERRED_TECHNIQUES && DRAWSKINNED_TECHNIQUES
	technique deferred_drawskinned
	{
		pass p0
		{
			VertexShader = compile VERTEXSHADER	VS_VehicleTransformSkinD(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_PlateDeferred()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif // DEFERRED_TECHNIQUES && NON_CUTOUT_DEFERRED_TECHNIQUES && DRAWSKINNED_TECHNIQUES

	#if DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES && DRAWSKINNED_TECHNIQUES
	technique deferredalphaclip_drawskinned
	{
		pass p0
		{
			AlphaRef = 100;
			AlphaBlendEnable = false;//true;
			AlphaTestEnable = false;//true;
			
			VertexShader = compile VERTEXSHADER	VS_VehicleTransformSkinD(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_PlateDeferred_AlphaClip()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}	
	#endif // DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES && DRAWSKINNED_TECHNIQUES

	#if DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES && DRAWSKINNED_TECHNIQUES
	technique deferredsubsamplealpha_drawskinned
	{
		pass p0
		{
			AlphaRef = 100;
			AlphaBlendEnable = false;//true;
			AlphaTestEnable = false;//true;
			
			VertexShader = compile VERTEXSHADER	VS_VehicleTransformSkinD(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_PlateDeferred_SubSampleAlpha()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif // DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES && DRAWSKINNED_TECHNIQUES

	// ===============================
	// Unlit techniques
	// ===============================
	#if UNLIT_TECHNIQUES
	technique unlit_draw
	{
		pass p0
		{
			VertexShader = compile VERTEXSHADER	VS_VehicleTransformUnlit(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_PlateUnlit()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif // UNLIT_TECHNIQUES

	#if UNLIT_TECHNIQUES && DRAWSKINNED_TECHNIQUES
	technique unlit_drawskinned
	{
		pass p0
		{
			VertexShader = compile VERTEXSHADER	VS_VehicleTransformSkinUnlit(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_PlateUnlit()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif // UNLIT_TECHNIQUES && DRAWSKINNED_TECHNIQUES

#endif	//__MAX...
