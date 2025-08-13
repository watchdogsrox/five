
#ifndef PRAGMA_DCL
	#pragma dcl position normal diffuse texcoord0 texcoord1 texcoord2 texcoord3 
	#define PRAGMA_DCL
#endif

///////////////////////////////////////////////////////////////////////////////
// PTFX_SPRITE.FX - sprite particle shader
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// GLOBAL SHADER VARIABLES (exposed to the interface)
///////////////////////////////////////////////////////////////////////////////

#define USE_WRAP_LIGHTING
#define EMISSIVE			(1)

#define SPECULAR (0)

#include "../Util/macros.fxh"
#include "../../../rage/base/src/shaderlib/rage_constantbuffers.fxh"

#pragma constant 64

BeginConstantBufferDX10( ptfx_sprite_locals1 )

#if SPECULAR
float specularFalloffMult : Specular
<
	string UIName = "Specular Falloff";
	float UIMin = 0.0;
	float UIMax = 2000.0;
	float UIStep = 0.1;
> = 32.0;

float specularIntensityMult : SpecularColor
<
	string UIName = "Specular Intensity";
	float UIMin = 0.0;
	float UIMax = 1.0;
	float UIStep = 0.01;
> = 0.0;
#endif

float wrapLigthtingTerm : WrapLightingTerm
<
	string UIName = "Wrap Lighting Term";
	float UIMin = 0.0;
	float UIMax = 1.0;
	float UIStep = 0.01;
> = 0.0;


#if EMISSIVE
float emissiveMultiplier : EmissiveMultiplier = 1.0f;
#endif

EndConstantBufferDX10( ptfx_sprite_locals1 )

#include "ptfx_common.fxh"

//Need to start a new constant buffer as other constants are defined in ptfx_common.fxh
//If we didnt do this the console versions registers would be incorrect.
BeginConstantBufferDX10( ptfx_sprite_locals2 )

///////////////////////////////////////////////////////////////////////////////
// DEFINES
///////////////////////////////////////////////////////////////////////////////

#ifndef XENON_ONLY
#if __XENON
#define XENON_ONLY(x)							x
#else
#define XENON_ONLY(x)
#endif 
#endif 

// diffuse modes
#define DIFFUSE_MODE_RGBA						0
#define DIFFUSE_MODE_XXXX						1
#define DIFFUSE_MODE_RGB						2
#define DIFFUSE_MODE_RG_BLEND					3

// blend modes
#define BLEND_MODE_NORMAL_V                 	0
#define BLEND_MODE_ADD_V					   	1

#define BLEND_MODE_NORMAL                 		BM0
#define BLEND_MODE_ADD					   		BM1

// misc
#define PS_DEBUG_OUTPUT							(0 && !defined(SHADER_FINAL))
#define REFRACTION_NO_BLUR						(0 || __LOW_QUALITY)
#define REFRACTION_7e3INT_PACKED				(__XENON)
#define USE_DISCARD_FOR_TRANSPARENCY            (RSG_PC)

///////////////////////////////////////////////////////////////////////////////
// SHADER VARIABLES (not exposed to the interface)
///////////////////////////////////////////////////////////////////////////////

float gBlendMode : BlendMode;
float4 gChannelMask : ChannelMask;

#if USE_SHADED_PTFX_MAP
float4 gShadedPtFxParams : ShadedPtFxParams;
float4 gShadedPtFxParams2 : ShadedPtFxParams2;

#define shadedPtFxRange				(gShadedPtFxParams.x)
#define shadedPtFxMapLoHeight		(gShadedPtFxParams.y)
#define shadedPtFxMapHiHeight		(gShadedPtFxParams.z)
#define shadedPtFxRangeFalloff		(gShadedPtFxParams.w)
#define shadedPtFxCloudShadowOffset (gShadedPtFxParams2.x)
#define shadedPtFxCloudShadowAmount (gShadedPtFxParams2.y)
#endif //USE_SHADED_PTFX_MAP

///////////////////////////////////////////////////////////////////////////////
// SHADER VARIABLES (exposed to the interface)
///////////////////////////////////////////////////////////////////////////////

float gSuperAlpha : SuperAlpha
<
	string UIName	= "SuperAlpha"; 
	float UIMin		= 0.0; 
	float UIMax		= 20.0; 
	float UIStep	= 0.01; 
	string UIHint	= "Keyframeable";
	int nostrip=1;	// dont strip
> = 2.0;

float gDirectionalMult : DirectionalMult
<
	string UIName = "DirectionalMult: Controls the amount of directional lighting on surface, color also controls this (0 - no light, 1- light)";
	float UIMin = 0.0;
	float UIMax = 2.0;
	float UIStep = 0.01;
	int nostrip=1;	// dont strip
> = 1.0f;

float gAmbientMult : AmbientMult
<
	string UIName = "AmbientMult: Controls the amount of ambient lighting to apply to surface";
	float UIMin = 0.0;
	float UIMax = 2.0;
	float UIStep = 0.01;
	int nostrip=1;	// dont strip
> = 1.0f;

float gShadowAmount : ShadowAmount
<
	string UIName = "ShadowAmount: Amount of shadows to apply to directional light";
	float UIMin = 0.0;
	float UIMax = 1.0;
	float UIStep = 0.01;
	string UIHint	= "Keyframeable";
	int nostrip=1;	// dont strip
> = 1.0f;

float gExtraLightMult : ExtraLightMult
<
	string UIName = "ExtraLightMult: Controls the amount of lighting applied from the additional 4 lights in gta (0 - no light, 4- light)";
	float UIMin = 0.0;
	float UIMax = 10.0;
	float UIStep = 0.01;
	int nostrip=1;	// dont strip
> = 1.0f;

float gCameraBias : CameraBias
<
	string UIName = "CameraBias: Specifies the distance (in metres) to bring the particle closer to the camera";
	float UIMin = -100.0;
	float UIMax = 100.0;
	float UIStep = 0.001;
#if PARTICLE_SHADOWS_SUPPORT
	int nostrip=1;	// dont strip
#endif
> = 0.0f;

float gCameraShrink : CameraShrink
<
	string UIName = "CameraShrink: Amount to scale down particles as the camera gets near (the higher the value the further away from the camera the particle shrinks)";
	float UIMin = 0.0;
	float UIMax = 1000.0;
	float UIStep = 0.01;
	string UIHint	= "Keyframeable";
	int nostrip=1;	// dont strip
> = 2.0f;

float gNormalArc : NormalArc
<
	string UIName = "NormalArc: How much to bend the normals from the face normal to pointing directly out from the centre of the sprite";
	float UIMin = 0.0;
	float UIMax = 1.0;
	float UIStep = 0.01;
	string UIHint	= "Keyframeable";
	int nostrip=1;	// dont strip
> = 1.0f;

float gDirNormalBias : DirNormalBias
<
	string UIName = "DirNormalBias: How much to bias the normals towards the light direction lerp(N.L, 1.0f, t)";
	float UIMin = 0.0;
	float UIMax = 1.0;
	float UIStep = 0.01;
	string UIHint	= "Keyframeable";
	int nostrip=1;	// dont strip
> = 0.0f;

float gSoftnessCurve : SoftnessCurve
<
	string UIName = "SoftnessCurve: Determines how round the soft particle fade edge is ( 0 - flat, 1 - round )";
	float UIMin = 0.0;
	float UIMax = 1.0;
	float UIStep = 0.01;
	int nostrip=1;	// dont strip
> = 0.0f;

float gSoftnessShadowMult : SoftnessShadowMult
<
	string UIName = "SoftnessShadowMult: Darkens the soft particle fade edge";
	float UIMin = 0.0;
	float UIMax = 1.0;
	float UIStep = 0.01;
	int nostrip=1;	// dont strip
> = 0.0f;

float gSoftnessShadowOffset : SoftnessShadowOffset
<
	string UIName = "SoftnessShadowOffset: Offsets the darkening effect with respect to the actual soft particle fade edge (0 - matches actual fade ramp, 1 - maximum offset from actual fade ramp )";
	float UIMin = 0.0;
	float UIMax = 1.0;
	float UIStep = 0.01;
	int nostrip=1;	// dont strip
> = 0.0f;

float gNormalMapMult : NormalMapMult
<
	string UIName = "NormalMapMult: scales the 2D vector sampled from the normal map used in the refraction techniques";
	float UIMin = -5.0;
	float UIMax = 5.0;
	float UIStep = 0.01;
	int nostrip=1;	// dont strip
> = 1.0f;

float3 gAlphaCutoffMinMax : AlphaCutoffMinMax
<	string UIName = "AlphaCutoffMinMax: specifies min (x) and max (y) values for alpha cutoff in grey scale packing techniques;(z =1.0f) use alpha cutoff; (z = 0.0f) use original alpha";
	float UIMin = 0.0;
	float UIMax = 1.0;
	float UIStep = 0.001;
	int nostrip=1;	// dont strip
> = float3(0.0f, 1.0f, 1.0f); // min, max, bool

float gRG_BlendStartDistance : RG_BlendStartDistance
<	string UIName = "RG_BlendStartDistance: start distance from the camera at which the R channel starts blending towards the G channel";
	float UIMin = 0.0;
	float UIMax = 100.0;
	float UIStep = 0.001;
	int nostrip=1;	// dont strip
> = 1.0f; 

float gRG_BlendEndDistance : RG_BlendEndDistance
<	string UIName = "RG_BlendEndDistance: end distance at which the G channel is fully blended in";
	float UIMin = 0.0;
	float UIMax = 10.0;
	float UIStep = 0.001;
	int nostrip=1;	// dont strip
> = 1.0f; 

EndConstantBufferDX10( ptfx_sprite_locals2 )

#include "../Lighting/lighting.fxh"
#include "../Lighting/forward_lighting.fxh"
#include "../PostFX/SeeThroughFX.fxh"
#include "../common.fxh"

#if PTFX_TESSELATE_SPRITES
#define gTessellationFactor gTessellationGlobal1.x
#endif

///////////////////////////////////////////////////////////////////////////////
// SAMPLERS
///////////////////////////////////////////////////////////////////////////////
BeginSampler(sampler,DiffuseTex2,DiffuseTexSampler,DiffuseTex2)
	string UIName = "Texture Map";
	string TCPTemplate = "Diffuse";
	string TextureType= "Diffuse";
	int nostrip=1;	// dont strip
ContinueSampler(sampler,DiffuseTex2,DiffuseTexSampler,DiffuseTex2)
    MinFilter = LINEAR;
    MagFilter = LINEAR;
    MipFilter = MIPLINEAR;
    AddressU  = CLAMP;        
	AddressV  = CLAMP;
EndSampler;

BeginDX10Sampler(sampler, Texture2D<float4>, FrameMap, FrameMapTexSampler, FrameMap)
	int nostrip=1;	// dont strip
ContinueSampler(sampler, FrameMap, FrameMapTexSampler, FrameMap) 
   MinFilter = LINEAR;
   MagFilter = LINEAR;
   MipFilter = NONE;
   AddressU  = CLAMP;
   AddressV  = CLAMP;
EndSampler;

BeginSampler(sampler, NormalSpecMap, NormalSpecMapTexSampler, NormalSpecMap)
	string UIName = "NormalSpec Map";
	string TCPTemplate = "Diffuse";
	string TextureType= "BumpSpec";
	int nostrip=1;	// dont strip
ContinueSampler(sampler, NormalSpecMap, NormalSpecMapTexSampler, NormalSpecMap) 
   MinFilter = LINEAR;
   MagFilter = LINEAR;
   MipFilter = NONE;
   AddressU  = CLAMP;
   AddressV  = CLAMP;
EndSampler;

BeginSampler(sampler, RefractionMap, RefractionMapTexSampler, RefractionMap)
string UIName = "RefractionMap Map";
string TCPTemplate = "Diffuse";
string	TextureType="BumpSpec";
int nostrip=1;	// dont strip
ContinueSampler(sampler, RefractionMap, RefractionMapTexSampler, RefractionMap) 
MinFilter = LINEAR;
MagFilter = LINEAR;
MipFilter = NONE;
AddressU  = CLAMP;
AddressV  = CLAMP;
EndSampler;

DECLARE_SAMPLER(sampler, ShadedPtFxMap, ShadedPtFxSampler,
	AddressU	= CLAMP;
	AddressV	= CLAMP;
	MINFILTER	= LINEAR;
	MAGFILTER	= LINEAR;
);

DECLARE_SAMPLER(sampler, CloudShadowMap, CloudShadowSampler,
	AddressU	= WRAP;
	AddressV	= WRAP;
	MINFILTER	= LINEAR;
	MAGFILTER	= LINEAR;
	MIPFILTER	= NONE;//This causes seam issues along large depth discontinuities
);

DECLARE_SAMPLER(sampler, SmoothStepMap, SmoothStepMapSampler,
	AddressU	= CLAMP;
	AddressV	= CLAMP;
	MINFILTER	= LINEAR;
	MAGFILTER	= LINEAR;
);

///////////////////////////////////////////////////////////////////////////////
// FUNCTIONS
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// ptfxSpritePostProcessVS
void ptfxSpritePostProcessVS(inout ptfxSpriteVSOut psInfo, ptfxSpriteVSInfo IN, bool isWaterRefraction, bool isShadow)
{
	if(isShadow)
	{
		psInfo.fogParam = 0.0f.xxxx;
	}
	else
	{
		if(isWaterRefraction && ptfx_UseWaterPostProcess)
		{
			float2 depthBlends = ptfxCommonGlobalWaterRefractionPostProcess_VS(IN.wldPos);
			psInfo.fogParam = float4(depthBlends.xy,0.0f,0.0f);
		}
		else
		{
			psInfo.fogParam = CalcFogData( IN.wldPos - gViewInverse[3].xyz); // we may want a lighter weight version of this...
		}
	}

	psInfo.texUVs   = IN.texUVs;
	psInfo.normUV.xy   = IN.normUV.xy;
	psInfo.custom1  = IN.custom1;
	psInfo.custom2.xy  = IN.custom2.xy; 
}

/*#if PTFX_SHADOWS_INSTANCING
void ptfxSpritePostProcessVSProj(inout ptfxSpriteVSOut psInfo, ptfxSpriteVSInfoProj IN, bool isWaterRefraction)
{
	if(isWaterRefraction && ptfx_UseWaterPostProcess)
	{
		float2 depthBlends = ptfxCommonGlobalWaterRefractionPostProcess_VS(IN.wldPos);
		psInfo.fogParam = float4(depthBlends.xy,0.0f,0.0f);
	}
	else
	{
		psInfo.fogParam = CalcFogData( IN.wldPos - gViewInverse[3].xyz); // we may want a lighter weight version of this...
	}
	psInfo.texUVs   = IN.texUVs;
	psInfo.normUV.xy   = IN.normUV.xy;
	psInfo.custom1  = IN.custom1;
	psInfo.custom2.xy  = IN.custom2.xy; // zw gets used for other params (see ptfx_common.fxh)
}
#endif*/

///////////////////////////////////////////////////////////////////////////////
// ptfxSpriteApplyFog
half4 ptfxSpriteApplyFog(half4 colour, ptfxSpritePSIn psInfo)
{
	// variables below are set by ptfxSpritePostProcessVS
	half3 fogColour = psInfo.fogParam.xyz;
	half fogBlend = psInfo.fogParam.w;

	colour.rgb = lerp(colour.rgb, fogColour, fogBlend);

	return colour;
}

///////////////////////////////////////////////////////////////////////////////
// ptfxSpriteGlobalPostProcess
void ptfxSpriteGlobalPostProcess(inout half4 color, ptfxSpritePSIn psInfo, bool bFogColor, bool isWaterRefraction)
{
	if (bFogColor)
	{
#if !__LOW_QUALITY
		if(isWaterRefraction && ptfx_UseWaterPostProcess)
		{
			color = ptfxCommonGlobalWaterRefractionPostProcess_PS(color,psInfo.pos.xy, psInfo.fogParam.xy);
		}
		else
#endif // !__LOW_QUALITY
		{
			color = ptfxSpriteApplyFog(color, psInfo);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
// ptfxSpriteSmallBlur
half3 ptfxSpriteSampleDistortion(float2 texCoord, bool bApplyBlur)
{
#if REFRACTION_NO_BLUR || __SHADERMODEL < 40
	return h4tex2D( FrameMapTexSampler, texCoord.xy ).rgb;
#else // !REFRACTION_NO_BLUR
		const float invSampleCount = 1.0f/ 5.0f;
		const float4 offsets = { -0.5f, 0.5f, -1.5f, 1.5f };
		float4 samplePattern0 = texCoord.xyxy + (offsets.yzzx*gInvScreenSize.xyxy);
		float4 samplePattern1 = texCoord.xyxy + (offsets.xwwy*gInvScreenSize.xyxy);

		half3 frameCol = FrameMap.SampleLevel( FrameMapTexSampler, texCoord.xy, 0 ).rgb;

		if(bApplyBlur)
		{
			frameCol += FrameMap.SampleLevel( FrameMapTexSampler, samplePattern0.xy, 0 ).rgb;
			frameCol += FrameMap.SampleLevel( FrameMapTexSampler, samplePattern0.zw, 0 ).rgb;  
			frameCol += FrameMap.SampleLevel( FrameMapTexSampler, samplePattern1.xy, 0 ).rgb;  
			frameCol += FrameMap.SampleLevel( FrameMapTexSampler, samplePattern1.zw, 0 ).rgb;
			frameCol *= invSampleCount;
		}
	return frameCol;
#endif // !REFRACTION_NO_BLUR
}

///////////////////////////////////////////////////////////////////////////////
// ptfxSpriteComputeSoftEdge
void ptfxSpriteComputeSoftEdge( float currenDepth, float spriteDepth, float distToCenterTimeSoftnessCurve, float softness, float nearPlaneFadeDistance,  inout half4 col )
{
	float depthDiff = (currenDepth - spriteDepth);
	depthDiff*=depthDiff;

	// apply roundness and darkening to fade ramp 
	// TODO: re-arrange instructions a bit better - keeping it more readable for now
	// offset depth difference to get a round edge
	depthDiff -= distToCenterTimeSoftnessCurve;
	col.a *= saturate( depthDiff * softness ); //soft fade on alpha

	// apply faux-shadow offset 
	const float softFadeRGB = saturate( (depthDiff - gSoftnessShadowOffset) * softness );
	const float darkenDiff = 1.0f - softFadeRGB;

	col.rgb *= (softFadeRGB + (darkenDiff * (1.0f - gSoftnessShadowMult)));

	//Apply mirror near Plane fade (gets activated only on mirror reflection phase)
	col.a *= ptfxApplyNearPlaneFade(nearPlaneFadeDistance);

}

///////////////////////////////////////////////////////////////////////////////
// ptfxSpriteBiasToCamera
void ptfxSpriteBiasToCamera(inout float3 pos)
{
	float3 cameraBias = gViewInverse[3].xyz - pos.xyz;
	cameraBias = normalize(cameraBias);
	pos.xyz += cameraBias*gCameraBias;
}

///////////////////////////////////////////////////////////////////////////////
// ptfxSpriteCalculateBasicLighting
void ptfxSpriteCalculateBasicLighting(ptfxSpriteVSInfo vsInfo, float3 normal, out float3 directionalLight, out float3 restOfTheLight)
{
	// initialise structures
	surfaceProperties surface = (surfaceProperties)0;
	directionalLightProperties light = (directionalLightProperties)0;
	materialProperties material = (materialProperties)0;
	StandardLightingProperties stdLightProps = (StandardLightingProperties)0;

	// surface properties
	surface.position = vsInfo.wldPos;
	surface.normal = normal;
	surface.lightDir = -gDirectionalLight.xyz;
	surface.sqrDistToLight = 0.0f;
	#if SPECULAR
		surface.eyeDir = normalize(gViewInverse[3].xyz - vsInfo.wldPos.xyz);
	#endif

	// light properties
	light.direction	= gDirectionalLight.xyz;
	light.color		= gDirectionalColour.rgb;

	// material properties
	material.ID = -1.0;
	material.diffuseColor = ProcessDiffuseColor(vsInfo.col);
	material.naturalAmbientScale = gAmbientMult*gNaturalAmbientScale;
	material.artificialAmbientScale = gAmbientMult*gArtificialAmbientScale*ArtificialBakeAdjustMult();
	material.emissiveIntensity = vsInfo.ptfxInfo_emissiveIntensity;
	material.inInterior = gInInterior;

	calculateParticleForwardLighting(
		4,
		true,
		surface,
		material,
		light,
		lerp(1.0f, 0.0f, CalcFogShadowDensity(vsInfo.wldPos)) * gDirectionalMult, 
		gExtraLightMult,
		directionalLight,
		restOfTheLight);
}

#if PTFX_SHADOWS_INSTANCING
float3 ptfxSpriteCalculateBasicLightingProj(ptfxSpriteVSInfoProj vsInfo, float3 normal)
{
	// initialise structures
	surfaceProperties surface = (surfaceProperties)0;
	directionalLightProperties light = (directionalLightProperties)0;
	materialProperties material = (materialProperties)0;
	StandardLightingProperties stdLightProps = (StandardLightingProperties)0;

	// surface properties
	surface.position = vsInfo.wldPos;
	surface.normal = normal;

	// light properties
	light.direction	= gDirectionalLight.xyz;
	light.color		= gDirectionalColour.rgb;

	// material properties
	material.ID = -1.0;
	material.diffuseColor = ProcessDiffuseColor(vsInfo.col);
	material.naturalAmbientScale = gAmbientMult*gNaturalAmbientScale;
	material.artificialAmbientScale = gAmbientMult*gArtificialAmbientScale*ArtificialBakeAdjustMult();
	material.emissiveIntensity = vsInfo.ptfxInfo_emissiveIntensity;
	material.inInterior = gInInterior;

	float3 directionalLight, restOfTheLight;
	calculateParticleForwardLighting(
		4,
		true,
		surface,
		material,
		light,
		lerp(1.0f, 0.0f, CalcFogShadowDensity(vsInfo.wldPos)) * gDirectionalMult,
		gExtraLightMult,
		directionalLight,
		restOfTheLight);

	return directionalLight + restOfTheLight;
}
#endif

///////////////////////////////////////////////////////////////////////////////
// ptfxSpriteCalculatePerPixelLighting
half4 ptfxSpriteCalculatePerPixelLighting(ptfxSpritePSIn IN, sampler2D screen, float4 texColor, float shadowAmount, bool specInAlpha, bool isScreenSpace)
{
	// derive tangent basis
	half3 n = normalize(IN.normal);
	half3 t = normalize(IN.tangent);
	half3 b = normalize(cross(t, n));

	// compute world normal
	half4 normSpec = h4tex2D(screen, IN.normUV.xy);
	half3 worldNormal = CalculateWorldNormal(normSpec.xy, t, b, n);


	// initialise structures
	surfaceProperties surface = (surfaceProperties)0;
	directionalLightProperties light = (directionalLightProperties)0;
	materialProperties material = (materialProperties)0;
	StandardLightingProperties stdLightProps = (StandardLightingProperties)0;

	// surface properties
	surface.position = IN.wldPos.xyz;
	surface.normal = worldNormal;
	surface.lightDir = -gDirectionalLight.xyz;
	surface.sqrDistToLight = 0.0f;
#if SPECULAR
	//Eye direction is the same as the camera if it's a screenspace effect
	surface.eyeDir = (isScreenSpace?-gViewInverse[2].xyz:normalize(gViewInverse[3].xyz - IN.wldPos.xyz));
#endif

	// light properties
	light.direction	= gDirectionalLight.xyz;
	light.color		= gDirectionalColour.rgb;

	// material properties
	material.ID = -1.0;
	material.diffuseColor = IN.col*texColor;
	material.naturalAmbientScale = gAmbientMult*gNaturalAmbientScale;
	material.artificialAmbientScale = gAmbientMult*gArtificialAmbientScale*ArtificialBakeAdjustMult();
	material.emissiveIntensity = 0.0f;
	material.inInterior = gInInterior;

#if SPECULAR
	float specIntensity = normSpec.z;
	if (specInAlpha)
	{
		specIntensity = normSpec.w;
	}

	material.specularIntensity = specIntensity*specularIntensityMult;
	material.specularExponent = specularFalloffMult;
#endif

	float3 directionalLight, restOfTheLight;
	calculateParticleForwardLighting(
				4,
				true,
				surface,
				material,
				light,
				lerp(1.0f, 0.0f, CalcFogShadowDensity(IN.wldPos.xyz)) * gDirectionalMult,
				gExtraLightMult,
				directionalLight,
				restOfTheLight);

	// add in the directional lighting component, but shadowed.
	half3 color = restOfTheLight + (directionalLight * shadowAmount);

	return half4(color, material.diffuseColor.a);
}

///////////////////////////////////////////////////////////////////////////////
// ptfxSpriteBlendDiffusePixelColor
void ptfxSpriteBlendDiffusePixelColor(out half4 color, half4 texColor, half4 texColor2, ptfxSpritePSIn psInfo)
{
	color = lerp(texColor, texColor2, psInfo.ptfxInfo_texFrameRatio.xxxx);
}

///////////////////////////////////////////////////////////////////////////////
// ptfxSpriteGetShadowAmountPS
float ptfxSpriteGetShadowAmountPS(ptfxSpritePSIn psInfo, bool isShadow)
{
	float shadowAmount = 1.0f;
	//No need to calculate shadow amount if we're rendering particle shadows
	if(!isShadow)
	{
#if USE_SHADED_PTFX_MAP
		float2 shadowLookup = IN.wldPos.xy;
		float4 shadowBuffer = tex2D(ShadedPtFxSampler, shadowLookup.xy);
		float t = IN.wldPos.z;

		shadowAmount = lerp(shadowBuffer.r, shadowBuffer.g, t);
		float outOfRangeFadeToAmount = shadedPtFxCloudShadowAmount * gNaturalAmbientScale;
		shadowAmount = lerp(outOfRangeFadeToAmount, shadowAmount, psInfo.ptfxInfo_PS_ShadowAmount);
#else
		shadowAmount = psInfo.ptfxInfo_PS_ShadowAmount;
#endif
	}
	return shadowAmount;
}

///////////////////////////////////////////////////////////////////////////////
// ptfxSpriteGetDiffuseColor
half4 ptfxSpriteGetDiffuseColor(ptfxSpritePSIn psInfo, int diffuseMode, bool useDiscard)
{
	// convert from srgb to linear space
	half4 diffColor1 = ProcessDiffuseColor(h4tex2D(DiffuseTexSampler, psInfo.texUVs.xy));
	half4 diffColor2 = ProcessDiffuseColor(h4tex2D(DiffuseTexSampler, psInfo.texUVs.zw));

#if USE_DISCARD_FOR_TRANSPARENCY
	rageDiscard(useDiscard && ((diffColor1.a+diffColor2.a) < 0.005));
#endif // USE_DISCARD_FOR_TRANSPARENCY

	// grey scale packing
	if (diffuseMode!=DIFFUSE_MODE_RGBA)
	{
		if (diffuseMode == DIFFUSE_MODE_XXXX) 
		{
			diffColor1.xyzw = dot(diffColor1.xyz, gChannelMask.xyz).xxxx;
			diffColor2.xyzw = dot(diffColor2.xyz, gChannelMask.xyz).xxxx;
		}
		else if (diffuseMode == DIFFUSE_MODE_RGB)
		{
			diffColor1.w = dot(diffColor1.xyz, 1);
			diffColor2.w = dot(diffColor2.xyz, 1);
		}
		else if (diffuseMode == DIFFUSE_MODE_RG_BLEND)
		{	
			diffColor1.xyzw = lerp(diffColor1.yyyy, diffColor1.xxxx, psInfo.ptfxInfo_RG_BlendFactor);
			diffColor2.xyzw = lerp(diffColor2.yyyy, diffColor2.xxxx, psInfo.ptfxInfo_RG_BlendFactor);
		}

		// initial alpha
		half2 inputAlpha = half2(diffColor1.a, diffColor2.a);

		// alpha cutoff:
		//							  [			min		   ,		max			 ]
		// anything outside the range [gAlphaCutoffMinMax.x, gAlphaCutoffMinMax.y]
		// is completely transparent - and opaque otherwise.
		// a small epsilon is added to gAlphaCutoffMinMax.yy to make 'max' inclusive
		half2 outputAlpha = saturate((inputAlpha-gAlphaCutoffMinMax.xx)*10000.0f.xx);
		outputAlpha *= saturate(((gAlphaCutoffMinMax.y+0.001f)-inputAlpha.x)*10000.0f);

		// keep results from previous alpha cutoff or use original alpha
		// note: this could actually be separated into a different technique altogether
		outputAlpha.xy = lerp(inputAlpha.xy, outputAlpha.xy, gAlphaCutoffMinMax.zz);

		// final alpha
		diffColor1.a = outputAlpha.x;
		diffColor2.a = outputAlpha.y;
	}

	// compute the diffuse colour
	half4 texColor;
	ptfxSpriteBlendDiffusePixelColor(texColor, diffColor1, diffColor2, psInfo);
	return texColor;

}

///////////////////////////////////////////////////////////////////////////////
// ptfxSpriteRG_CalcBlendFactor
float ptfxSpriteRG_CalcBlendFactor(float3 wldPos)
{
	float distToCam = length(wldPos.xyz-gViewInverse[3].xyz);
	distToCam = saturate((distToCam-gRG_BlendEndDistance)/(gRG_BlendStartDistance));
	return distToCam;
}


///////////////////////////////////////////////////////////////////////////////
// ptfxComputeColorAndLighting
half4 ptfxComputeColorAndLighting(ptfxSpritePSIn IN, float2 vPos, bool isLit, bool isNormalSpec, bool isSoft, bool isShadow, bool isScreenSpace, bool isRefract, int diffuseMode, out float4 texColor)
{
	// compute the final colour
	half4 color = IN.col;
#if USE_DISCARD_FOR_TRANSPARENCY
	texColor = ptfxSpriteGetDiffuseColor(IN, diffuseMode, !isRefract);
#else // USE_DISCARD_FOR_TRANSPARENCY
	texColor = 0.0f;
#endif // USE_DISCARD_FOR_TRANSPARENCY

	//we will always fetch the shadow amount from the input
	float shadowAmount = ptfxSpriteGetShadowAmountPS(IN, isShadow);

	if (isNormalSpec)
	{
		//Normal spec diffuse color fetch does their sampling here. The non normal spec diffuse fetch
		//is done later to reduce VGPR count
#if !USE_DISCARD_FOR_TRANSPARENCY
		texColor = ptfxSpriteGetDiffuseColor(IN, diffuseMode, !isRefract);
#endif // !USE_DISCARD_FOR_TRANSPARENCY
		color = ptfxSpriteCalculatePerPixelLighting(IN, NormalSpecMapTexSampler, texColor, shadowAmount, false, isScreenSpace);
	}
	// handle shadowing on the directional light first
	else if (isLit && (!isShadow))
	{
		// add in the directional lighting component, but shadowed.
		// IN.normal.xyz is the directional light component
		// IN.color.xyz is the rest of the light
		color.rgb += (IN.ptfxInfo_directionalColRGB * shadowAmount);
	}

	const bool bPerformComputeSoftEdge = isSoft && !isShadow;
	if(isRefract || bPerformComputeSoftEdge)
	{
#if __LOW_QUALITY
		float sampledDepth = 1.0f;
#else
		// depth sample will be reused in ptfxSpriteComputeSoftEdge 
		float sampledDepth;
		ptfxGetDepthValue( vPos.xy, sampledDepth );
#endif // __LOW_QUALITY

		if(isRefract)
		{
			// kill alpha if an object is in front of the particle 
			half alphaMultiplier = saturate((sampledDepth - IN.ptfxInfo_depthW)*1000.0f);
			color.a *= alphaMultiplier;		
		}

#if USE_SOFT_PARTICLES
		if(bPerformComputeSoftEdge)
		{
			ptfxSpriteComputeSoftEdge( sampledDepth, IN.ptfxInfo_depthW, IN.ptfxInfo_distToCenterTimeSoftnessCurve, IN.ptfxInfo_softness, RAGE_NEAR_CLIP_DISTANCE(IN.pos), color );
		}
#endif
	}

#if !USE_DISCARD_FOR_TRANSPARENCY
   	texColor = 0.0f;
#endif // !USE_DISCARD_FOR_TRANSPARENCY
	if (isNormalSpec)
	{
		// nothing to do here for now
	}
	else
	{
		//Moved the diffuse texture fetch for non normal spec to here to reduce VPGR count
#if !USE_DISCARD_FOR_TRANSPARENCY
		texColor = ptfxSpriteGetDiffuseColor(IN, diffuseMode, !isRefract);
#endif // !USE_DISCARD_FOR_TRANSPARENCY
		color *= texColor;
	}

	if (isLit && !isNormalSpec)
	{
		half3 finalColor = IN.ptfxInfo_colRGB * texColor;

#if EMISSIVE
		const half materialLum = saturate(dot(finalColor, half3(0.2126f, 0.7152f, 0.0722f)) - 0.05) * IN.ptfxInfo_emissiveIntensity;
		color.rgb += finalColor.rgb * materialLum;
#endif
	}

	return color;
}

///////////////////////////////////////////////////////////////////////////////
// Instancing Setup functions
///////////////////////////////////////////////////////////////////////////////
#if PTFX_USE_INSTANCING
void SetupVertFromInstanceData(ptfxSpriteVSInfo IN, out float3 worldPos, out float3 centerPos, out float4 texUVs, out float4 normUVs)
{
	float3 vOffsetUp	= IN.offsetUp;
	float3 vOffsetDown	= IN.offsetDown;
	float3 vOffsetLeft	= float3( IN.offsetUp.w, IN.offsetDown.w, IN.offsetRight.w);
	float3 vOffsetRight	= IN.offsetRight;

	//When instancing is on we put the sprite position in the center pos slot and calculate the center in shader
	float3 vSpriteBLPosWld = IN.centerPos + vOffsetDown + vOffsetLeft;
	float3 vSpriteRight = vOffsetRight-vOffsetLeft;
	float3 vSpriteUp = vOffsetUp-vOffsetDown;

	centerPos = vSpriteBLPosWld + ((vSpriteRight + vSpriteUp) * 0.5f);

	float4 uvInfoA = IN.misc1;
	float4 uvInfoB = IN.misc2;
	float4 uvInfoN = uvInfoA;

	// calc the tex coord deltas
	float duA = (uvInfoA.y - uvInfoA.x);
	float dvA = (uvInfoA.z - uvInfoA.w);
	float duB = (uvInfoB.y - uvInfoB.x);
	float dvB = (uvInfoB.z - uvInfoB.w);
	float duN = (uvInfoN.y - uvInfoN.x);
	float dvN = (uvInfoN.z - uvInfoN.w);

	float4 vClipRegion = IN.clipRegion;
	float4 vClipRegionNormal = vClipRegion;

	float clipBoundsU[3] = {vClipRegion.x, 0.5, vClipRegion.y};
	float clipBoundsV[3] = {vClipRegion.z, 0.5, vClipRegion.w};

	float clipBoundsUN[3] = {vClipRegionNormal.x, 0.5, vClipRegionNormal.y};
	float clipBoundsVN[3] = {vClipRegionNormal.z, 0.5, vClipRegionNormal.w};

	//Calculate Position
	int xPosIndex = IN.wldPos.x;
	int yPosIndex = IN.wldPos.y;

	float tRight = clipBoundsU[xPosIndex];
	float tUp = clipBoundsV[yPosIndex];

	float3 temp = vSpriteBLPosWld + (vSpriteRight * tRight);
	float3 vVtxPosWld = temp + (vSpriteUp * tUp);

	worldPos = vVtxPosWld;

	//Tex coords
	float3 halfLength = centerPos - vSpriteBLPosWld;
	float vInvSqrSpriteHalfLength = 1.0f / sqrt(dot(halfLength, halfLength));

	float4 vUVInfoAB;
	vUVInfoAB.x = uvInfoA.x +  (clipBoundsU[xPosIndex] * duA);
	vUVInfoAB.y = uvInfoA.w +  (clipBoundsV[yPosIndex] * dvA);
	vUVInfoAB.z = uvInfoB.x +  (clipBoundsU[xPosIndex] * duB);
	vUVInfoAB.w = uvInfoB.w +  (clipBoundsV[yPosIndex] * dvB);

	float4 vUVInfoN_DistToCentre;
	vUVInfoN_DistToCentre.x = uvInfoN.x + (clipBoundsUN[xPosIndex] * duN);
	vUVInfoN_DistToCentre.y = uvInfoN.w + (clipBoundsVN[yPosIndex] * dvN);
	vUVInfoN_DistToCentre.z = length(centerPos-vVtxPosWld)*vInvSqrSpriteHalfLength;
	vUVInfoN_DistToCentre.w = 0.0f;
	
	texUVs = vUVInfoAB;
	normUVs = vUVInfoN_DistToCentre;
}

#if PTFX_SHADOWS_INSTANCING
void SetupVertFromInstanceDataProjected(ptfxSpriteVSInfoProj IN, uint VertexID, out float3 worldPos, out float3 centerPos, out float4 texUVs, out float4 normUVs)
#else
void SetupVertFromInstanceDataProjected(ptfxSpriteVSInfo IN, uint VertexID, out float3 worldPos, out float3 centerPos, out float4 texUVs, out float4 normUVs)
#endif
{
	float3 vOffsetUp	= IN.offsetUp;
	float3 vOffsetDown	= IN.offsetDown;
	float3 vOffsetLeft	= float3( IN.offsetUp.w, IN.offsetDown.w, IN.offsetRight.w);
	float3 vOffsetRight	= IN.offsetRight;

	//When instancing is on we put the sprite position in the center pos slot and calculate the center in shader
	float3 vSpriteBLPosWld = IN.centerPos + vOffsetDown + vOffsetLeft;
	float3 vSpriteRight = vOffsetRight-vOffsetLeft;
	float3 vSpriteUp = vOffsetUp-vOffsetDown;

	centerPos = vSpriteBLPosWld + ((vSpriteRight + vSpriteUp) * 0.5f);

	float projectionDepth = IN.misc2.x;
	float3 vSpritePos = IN.centerPos;

	float3 vVerts[8];
	float3 vHeightVec = float3(0.0f, 0.0f, projectionDepth*0.5f);
	float3 vForwardVec = float3(vSpriteUp.xy, 0.0f);
	float3 vSideVec = float3(vSpriteRight.xy, 0.0f);

	vVerts[0] = vSpritePos + vForwardVec - vSideVec + vHeightVec;
	vVerts[1] = vSpritePos + vForwardVec + vSideVec + vHeightVec;
	vVerts[2] = vSpritePos - vForwardVec + vSideVec + vHeightVec;
	vVerts[3] = vSpritePos - vForwardVec - vSideVec + vHeightVec;	

	vVerts[4] = vVerts[0] - (vHeightVec * 2.0f);		
	vVerts[5] = vVerts[1] - (vHeightVec * 2.0f);
	vVerts[6] = vVerts[2] - (vHeightVec * 2.0f);	
	vVerts[7] = vVerts[3] - (vHeightVec * 2.0f);	

	worldPos = vVerts[VertexID];

	float texFrameA = IN.misc2.y;
	float texFrameB = IN.misc2.z;
	float numTexColumns = IN.misc2.w;

	float xIndexA = texFrameA % numTexColumns;
	float yIndexA = texFrameA / numTexColumns;
	float xIndexB = texFrameB % numTexColumns;
	float yIndexB = texFrameB / numTexColumns;

	float4 vUVInfoAB = float4(xIndexA, yIndexA, xIndexB, yIndexB );
	float4 vUVInfoN_ProjDir = float4(1.0f, 1.0f, vForwardVec.xy );

	texUVs = vUVInfoAB;
	normUVs = vUVInfoN_ProjDir;
}
#endif //PTFX_USE_INSTANCING

///////////////////////////////////////////////////////////////////////////////
// Geometry SHADERS
///////////////////////////////////////////////////////////////////////////////
#if PTFX_SHADOWS_INSTANCING_GEOMSHADER
struct GS_InstPassThrough_OUT
{
	DECLARE_POSITION(pos)
	half4 col				: TEXCOORD0;					// sprite colour

	// variables below are set by ptfxSpritePostProcessVS
	float4 texUVs			: TEXCOORD1;					// uvs of source and destination diffuse textures (u0, v0, u1, v1)
	float4 normUV			: TEXCOORD2;					// normalU, normalV, desatScale, gammaScale
	float4 misc				: TEXCOORD3;					// texFrameRatio, emissiveIntensity, alphaFactor, depth (for softness calculation)
	float4 fogParam			: TEXCOORD4;					// fogColor.r, fogColor.g, fogColor.b, fogBlend

	float4 custom1			: TEXCOORD5;					// custom shader-defined vars
	float4 custom2			: TEXCOORD6;					// xy: custom shader-defined vars, z: distance to center, w: squared distance to camera
	float4 wldPos			: TEXCOORD7;
	float3 normal			: TEXCOORD8;
	float4 tangent			: TEXCOORD9;

	uint InstID			: TEXCOORD10;

#if CASCADE_SHADOW_TEXARRAY
	uint rt_index : SV_RenderTargetArrayIndex;
#else
	uint viewport : SV_ViewportArrayIndex;
#endif
};

[maxvertexcount(3)]
void GS_InstPassThrough(triangle ptfxSpriteVSOut input[3], inout TriangleStream<GS_InstPassThrough_OUT> OutputStream)
{
	GS_InstPassThrough_OUT output;

	for(int i = 0; i < 3; i++)
	{
		output.pos = input[i].pos;
		output.col = input[i].col;
		output.texUVs = input[i].texUVs;
		output.normUV = input[i].normUV;
		output.misc = input[i].misc;
		output.fogParam = input[i].fogParam;
		output.custom1 = input[i].custom1;
		output.custom2 = input[i].custom2;
		output.custom3 = input[i].custom3;
		output.wldPos = input[i].wldPos;
		output.normal = input[i].normal;
		output.tangent = input[i].tangent;
		output.InstID = input[i].InstID;

#if CASCADE_SHADOW_TEXARRAY
		output.rt_index = input[i].InstID;
#else
		output.viewport = input[i].InstID;
#endif

		OutputStream.Append(output);
	}
	OutputStream.RestartStrip();
}
#endif //PTFX_SHADOWS_INSTANCING_GEOMSHADER


///////////////////////////////////////////////////////////////////////////////
// VERTEX SHADERS
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// ptfxSpriteCommonVS
ptfxSpriteVSOut ptfxSpriteCommonVS(ptfxSpriteVSInfo IN, bool isLit, bool isSoft, bool isScreenSpace, bool isNormalSpec, bool isShadow, bool tessellation,bool useInstancing, bool isWaterRefraction)
{
#if (__LOW_QUALITY)
	isSoft = false;
#endif // __LOW_QUALITY

#if PTFX_USE_INSTANCING
	if( !tessellation )
	{
		float3 worldPos;
		float3 centerPos;
		float4 texUVs;
		float4 normUVs;
		SetupVertFromInstanceData(IN, worldPos, centerPos, texUVs, normUVs);
		IN.wldPos = worldPos;
		IN.centerPos = centerPos;
		IN.texUVs = texUVs;
		IN.normUV = normUVs;
	}
#endif

	ptfxSpriteVSOut OUT = (ptfxSpriteVSOut)0;
	OUT.col = IN.col;
	OUT.ptfxInfo_texFrameRatio = saturate(IN.ptfxInfo_texFrameRatio); // copy misc params
	OUT.ptfxInfo_emissiveIntensity = IN.ptfxInfo_emissiveIntensity; 

#if PTFX_SHADOWS_INSTANCING
	int iCBInstIndex = 0;
	if (useInstancing)
	{
		iCBInstIndex = ptfxGetCascadeInstanceIndex(IN.instID);
		#if PTFX_SHADOWS_INSTANCING_GEOMSHADER
		OUT.InstID = iCBInstIndex;
		#endif
	}
#endif

	float3 centreToVtx = float3(0.0, 0.0, 0.0);
	if (!isScreenSpace)
	{
		if(isShadow)
		{
			// Disable camera shrink for now on shadows. Probably need it to work correctly.
			IN.ptfxInfo_cameraShrink = 1.0f;
		}

		float3 centrePos;
#if PTFX_SHADOWS_INSTANCING
		if (useInstancing)
			centrePos = mul(float4(IN.centerPos, 1.0f), gInstWorldViewProj[iCBInstIndex]).xyz;
		else
#endif
			centrePos = mul(float4(IN.centerPos, 1.0f), gWorldViewProj).xyz;

		centreToVtx = (IN.wldPos - IN.centerPos)*IN.ptfxInfo_cameraShrink;
		IN.wldPos = IN.centerPos + centreToVtx;

		if(!isShadow)
		{
			ptfxSpriteBiasToCamera(IN.wldPos.xyz);
			ptfxSpriteBiasToCamera(IN.centerPos.xyz);
		}

		// calc the world position of the vertex
#if PTFX_SHADOWS_INSTANCING
		if (useInstancing)
			OUT.pos = mul(float4(IN.wldPos, 1), gInstWorldViewProj[iCBInstIndex]);
		else			
#endif
			OUT.pos = mul(float4(IN.wldPos, 1), gWorldViewProj);
			

		// strip out all alpha quads
		float closeFade = OUT.col.a<=0.000001f ? 0.0f : IN.ptfxInfo_cameraShrink;
		OUT.col.a *= closeFade;

		OUT.ptfxInfo_depthW = OUT.pos.w;

	#if USE_SOFT_PARTICLES
		// pass down normalised distance to sprite centre
		OUT.ptfxInfo_distToCenterTimeSoftnessCurve = IN.normUV.z * gSoftnessCurve;
	#endif

		// pass down blend factor for rg channels
		OUT.ptfxInfo_RG_BlendFactor = ptfxSpriteRG_CalcBlendFactor(IN.wldPos.xyz);
	}
	else
	{
		OUT.pos.xyz = IN.wldPos.xzy;
 		OUT.pos.z = 0.0f; 		
		OUT.pos.w = 1.0f;

		//world and center positions are in projected space. We should bring them back to
		//world space to get correct lighting calculations
		const float2 screenRayForWorldPos = OUT.pos.xy;
		float3 eyeRayFromScreenSpaceForWorldPos = ptfxGetEyeRay(screenRayForWorldPos).xyz;
		float3 eyeRayToPointForWorldPos = eyeRayFromScreenSpaceForWorldPos * NearFarPlane.x;
		IN.wldPos.xyz = gViewInverse[3].xyz + eyeRayToPointForWorldPos;

		const float2 screenRayForCenterPos = IN.centerPos.xz;
		float3 eyeRayFromScreenSpaceForCenterPos = ptfxGetEyeRay(screenRayForCenterPos).xyz;
		float3 eyeRayToPointForCenterPos = eyeRayFromScreenSpaceForCenterPos * NearFarPlane.x;
		IN.centerPos.xyz = gViewInverse[3].xyz + eyeRayToPointForCenterPos;

		OUT.wldPos.xyz = IN.wldPos.xyz;
		//base normal is the same as camera direction
		IN.normal.xyz = -gViewInverse[2].xyz;

		if (isNormalSpec)
		{
			float3 normalBent = IN.wldPos-IN.centerPos;
			normalBent = dot(normalBent, normalBent)<0.001f ? IN.normal.xyz : normalize(normalBent);
			float3 normalFinal = lerp(IN.normal.xyz, normalBent, IN.ptfxInfo_normalArc);
			normalFinal = normalize(normalFinal);

			// pass down world position, normal and tangent for per-pixel lighting
			OUT.normal = normalFinal;
			float3 up = float3(0,0,1);
			float3 tangent = normalize(cross(up,normalFinal));
			OUT.tangent.xyz = tangent.xyz;
		}
	}	

	OUT.ptfxInfo_PS_ShadowAmount = 1.0f;
	if (isLit && !isShadow)
	{
		// bend the normal based on the offset of the current vertex
		// from the particle center
		float3 normalBent = IN.wldPos-IN.centerPos;
		normalBent = dot(normalBent, normalBent)<0.001f ? IN.normal.xyz : normalize(normalBent);
		float3 normalFinal = lerp(IN.normal.xyz, normalBent, IN.ptfxInfo_normalArc);
		normalFinal = normalize(normalFinal);

		if (isNormalSpec == false)
		{
			// calculate per-vertex lighting, but keep the directional light seperate for shadowing purposes
			// OUT.normal.xyz = directional light
			// OUT.col.xyz = rest of the light
			float3 restOfTheLight;
			ptfxSpriteCalculateBasicLighting(IN, normalFinal, OUT.ptfxInfo_directionalColRGB, restOfTheLight);
			OUT.col.xyz = restOfTheLight.xyz;
		}
		else
		{
			// pass down world position, normal and tangent for per-pixel lighting
			OUT.normal = normalFinal;
			float3 up = float3(0,0,1);
			float3 tangent = normalize(cross(up,normalFinal));
			OUT.tangent.xyz = tangent.xyz;
			OUT.wldPos.xyz = IN.wldPos.xyz;
		}
		//we plan to get rid of USE_SHADED_PTFX_MAP. So disabling shadow calculations if that is 
		//set to 1
#if !USE_SHADED_PTFX_MAP
		float shadowAmount = CalculateVertexShadowAmount(IN.centerPos, centreToVtx, IN.ptfxInfo_shadowAmount, isShadow, false);
		OUT.ptfxInfo_PS_ShadowAmount = shadowAmount;
#endif
	}
	else
	{
	#if EMISSIVE
		// apply emissive contribution
		OUT.col.xyz += ProcessDiffuseColor(IN.col).xyz*IN.ptfxInfo_emissiveIntensity;
	#endif
	}

	//For the shadows get the distance to the vertex to determine if we need to bias the shadows
	if( isShadow )
	{
		float3 vecToCamera = IN.wldPos.xyz - gViewInverse[3].xyz;
		float distToCam = length(vecToCamera);
		OUT.ptfxInfo_PS_ShadowAmount = distToCam;
	}

	// apply the super alpha
	OUT.col.a *= IN.ptfxInfo_superAlpha;

	ptfxSpritePostProcessVS(OUT, IN, isWaterRefraction, isShadow); 

	if (isLit && !isNormalSpec)
	{
		#if EMISSIVE
			// Pack vert colour into variables we don't need.
			OUT.ptfxInfo_colRGB = IN.col.rgb;
		#endif
	}

#if PTFX_SHADOWS_INSTANCING && !PTFX_SHADOWS_INSTANCING_GEOMSHADER
	if (useInstancing)
	{
		float posYClipSpace = OUT.pos.y / OUT.pos.w;
		float posYViewport;
		float cascadeHeight = 1.0 / CASCADE_SHADOWS_COUNT;
		float currentCascade = CASCADE_SHADOWS_COUNT - 1.0 - iCBInstIndex;
		posYViewport = (posYClipSpace + 1.0) * 0.5;
		posYViewport *= cascadeHeight;
		posYViewport += (cascadeHeight * currentCascade);
		posYViewport = (posYViewport * 2.0) - 1.0f;
		OUT.pos.y = posYViewport * OUT.pos.w;
		OUT.ptfxInfo_CBInstIndex = iCBInstIndex;
	}
#endif

	RAGE_COMPUTE_CLIP_DISTANCES(OUT.pos);
	return OUT;
}

///////////////////////////////////////////////////////////////////////////////
// ptfxSpriteProjCommon
#if PTFX_SHADOWS_INSTANCING
ptfxSpriteVSOut ptfxSpriteProjCommon(ptfxSpriteVSInfoProj IN, bool isShadow, bool useInstancing, bool isWaterRefraction) 
#else
ptfxSpriteVSOut ptfxSpriteProjCommon(ptfxSpriteVSInfo IN, bool isShadow, bool useInstancing, bool isWaterRefraction) 
#endif
{
#if PTFX_USE_INSTANCING
	float3 worldPos;
	float3 centerPos;
	float4 texUVs;
	float4 normUVs;
	SetupVertFromInstanceDataProjected(IN, IN.VertexID, worldPos, centerPos, texUVs, normUVs);
	IN.wldPos = worldPos;
	IN.centerPos = centerPos;
	IN.texUVs = texUVs;
	IN.normUV = normUVs;
#endif //PTFX_USE_INSTANCING

	ptfxSpriteVSOut OUT = (ptfxSpriteVSOut)0;
	OUT.col = IN.col;
	OUT.ptfxInfo_texFrameRatio = saturate(IN.ptfxInfo_texFrameRatio); // copy misc params
	OUT.ptfxInfo_emissiveIntensity = IN.ptfxInfo_emissiveIntensity; // copy misc params

	// calculate screen position of vertex
#if PTFX_SHADOWS_INSTANCING
	int iCBInstIndex = 0;
	if (useInstancing)
	{
		iCBInstIndex = ptfxGetCascadeInstanceIndex(IN.instID);
		#if PTFX_SHADOWS_INSTANCING_GEOMSHADER
		OUT.InstID = iCBInstIndex;
		#endif 
		OUT.pos = mul(float4(IN.wldPos, 1), gInstWorldViewProj[iCBInstIndex]);
	}
	else
#endif
		OUT.pos = mul(float4(IN.wldPos, 1), gWorldViewProj);

	float3 projDir = IN.normal.xyz;

	// pass through mid position
	float size = length(projDir.xy);
    OUT.wldPos.xyz = IN.centerPos.xyz;
	OUT.wldPos.w = 1.0f/size;

	// calculate the direction of this vertex
	OUT.normUV.zw = projDir.xy * OUT.wldPos.w;
	OUT.normal.xy = projDir.yx * float2(1.0f, -1.0f) * OUT.wldPos.w;

	//
	OUT.tangent = float4((IN.wldPos-gViewInverse[3].xyz), OUT.pos.w);


	float3 normal =  normalize(IN.normal.xyz);

	// calculate per-vertex lighting
#if PTFX_SHADOWS_INSTANCING
 	OUT.col.xyz = ptfxSpriteCalculateBasicLightingProj(IN, normal);
#else
	// calculate per-vertex lighting, but keep the directional light seperate for shadowing purposes
	float3 restOfTheLight;
	ptfxSpriteCalculateBasicLighting(IN, normal, OUT.ptfxProj_dirLight, restOfTheLight);
	OUT.col.xyz = restOfTheLight;

	float3 vecToCamera = IN.wldPos.xyz - gViewInverse[3].xyz;
#if USE_SHADED_PTFX_MAP
	// texture coordinate lookups into the shaded ptfx buffer
	OUT.ptfxProj_shadowUV = vecToCamera.xy / (shadedPtFxRange * 2.0);
	OUT.ptfxProj_shadowUV += 0.5f;

	// shadow amount from the asset and the fade of this effect as it passes 80% of range
	float distToCam = length(vecToCamera);
	distToCam *= step( shadedPtFxRangeFalloff, distToCam );
	OUT.ptfxProj_PS_shadowAmount = IN.ptfxInfo_shadowAmount * saturate( 1.0 - (distToCam / shadedPtFxRange) );
#else
	float3 centreToVtx = (IN.wldPos - IN.centerPos);
	float shadowAmount = CalculateVertexShadowAmount(IN.centerPos, centreToVtx, IN.ptfxInfo_shadowAmount, isShadow, false);
	OUT.ptfxProj_PS_shadowAmount = shadowAmount;
#endif

#endif

	// from ptfxSpritePostProcessVS - pulled these out because these are the only 2 we need to set for proj particles
	// and we don't want to overwrite the values we set above
	OUT.fogParam = CalcFogData( IN.wldPos - gViewInverse[3].xyz); // we may want a lighter weight version of this...
	OUT.normUV.xy   = IN.normUV.xy;

	OUT.ptfxProj_uvStep = IN.ptfxProj_uvStep;
	OUT.ptfxProj_idxA = IN.ptfxProj_idxA;
	OUT.ptfxProj_idxB = IN.ptfxProj_idxB;

	RAGE_COMPUTE_CLIP_DISTANCES(OUT.pos);
	return OUT;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Thermal vertex shaders
ptfxSpriteVSOut VS_thermalFunc(ptfxSpriteVSInfo IN, bool isShadow, bool useInstancing)
{
#if PTFX_USE_INSTANCING
	float3 worldPos;
	float3 centerPos;
	float4 texUVs;
	float4 normUVs;
	SetupVertFromInstanceData(IN, worldPos, centerPos, texUVs, normUVs);
	IN.wldPos = worldPos;
	IN.centerPos = centerPos;
	IN.texUVs = texUVs;
	IN.normUV = normUVs;
#endif 

	ptfxSpriteVSOut OUT = (ptfxSpriteVSOut)0;
	OUT.col = IN.col;
	OUT.ptfxInfo_texFrameRatio = saturate(IN.ptfxInfo_texFrameRatio); // copy misc params
	OUT.ptfxInfo_emissiveIntensity = IN.ptfxInfo_emissiveIntensity; // copy misc params

	float3 centrePos;
#if PTFX_SHADOWS_INSTANCING
	int iCBInstIndex = 0;
	if (useInstancing)
	{
		iCBInstIndex = ptfxGetCascadeInstanceIndex(IN.instID);
		#if PTFX_SHADOWS_INSTANCING_GEOMSHADER
		OUT.InstID = iCBInstIndex;
		#endif
		centrePos = mul(float4(IN.centerPos, 1.0f), gInstWorldViewProj[iCBInstIndex]).xyz;
	}
	else
#endif
	centrePos = mul(float4(IN.centerPos, 1.0f), gWorldViewProj).xyz;

	if(isShadow)
	{
		// Disable camera shrink for now on shadows. Probably need it to work correctly.
		IN.ptfxInfo_cameraShrink = 1.0f;
	}

	float3 centreToVtx = (IN.wldPos - IN.centerPos)*IN.ptfxInfo_cameraShrink;
	IN.wldPos = IN.centerPos + centreToVtx;

	if(!isShadow)
	{
		ptfxSpriteBiasToCamera(IN.wldPos.xyz);
		ptfxSpriteBiasToCamera(IN.centerPos.xyz);
	}

	// calc the world position of the vertex
#if PTFX_SHADOWS_INSTANCING
	if (useInstancing)
	{
		OUT.pos = mul(float4(IN.wldPos, 1), gInstWorldViewProj[iCBInstIndex]);
	}
	else
#endif
	OUT.pos = mul(float4(IN.wldPos, 1), gWorldViewProj);

	// strip out all alpha quads
	float closeFade = OUT.col.a<=0.000001f ? 0.0f : IN.ptfxInfo_cameraShrink;
	OUT.col.a *= closeFade;

	OUT.ptfxInfo_depthW = OUT.pos.w;

	// pass down normalised distance to sprite centre
	OUT.ptfxInfo_distToCenterTimeSoftnessCurve = IN.normUV.z * gSoftnessCurve;

	// pass down blend factor for rg channels
	OUT.ptfxInfo_RG_BlendFactor = ptfxSpriteRG_CalcBlendFactor(IN.wldPos.xyz);

#if EMISSIVE
	// apply emissive contribution
	OUT.col.xyz += ProcessDiffuseColor(IN.col).xyz*IN.ptfxInfo_emissiveIntensity;

	// if the particle effect is not emissive we don't want it to show up in thermal vision
	OUT.col.xyzw = (IN.ptfxInfo_emissiveIntensity == 0.0f ? 0.0f.xxxx : OUT.col.xyzw);
#endif

	// apply the super alpha
	OUT.col.a *= IN.ptfxInfo_superAlpha; 

	ptfxSpritePostProcessVS(OUT, IN, false, isShadow);
	
	OUT.ptfxInfo_projDepth = saturate(OUT.pos.z/gFadeEndDistance);

	RAGE_COMPUTE_CLIP_DISTANCES(OUT.pos);
	return OUT;
	
}

#if PTFX_TESSELATE_SPRITES

ptfxSpriteVSInfo_CtrlPoint ptfxSpriteCommonVS_Tessellated(ptfxSpriteVSInfo IN)
{
#if PTFX_USE_INSTANCING
	float3 worldPos;
	float3 centerPos;
	float4 texUVs;
	float4 normUVs;
	SetupVertFromInstanceData(IN, worldPos, centerPos, texUVs, normUVs);
	IN.wldPos = worldPos;
	IN.centerPos = centerPos;
	IN.texUVs = texUVs;
	IN.normUV = normUVs;
#endif

	ptfxSpriteVSInfo_CtrlPoint OUT;
	OUT.centerPos = IN.centerPos;
	OUT.col = IN.col;
	OUT.normal = IN.normal;
	OUT.misc = IN.misc;
	OUT.custom1 = IN.custom1;
	OUT.custom2 = IN.custom2;
	OUT.wldPos = IN.wldPos;
	OUT.texUVs = IN.texUVs;
	OUT.normUV = IN.normUV;

#if PTFX_USE_INSTANCING
	OUT.offsetUp = IN.offsetUp;
	OUT.offsetDown = IN.offsetDown;
	OUT.offsetRight = IN.offsetRight;
	OUT.clipRegion = IN.clipRegion;
	OUT.misc1 = IN.misc1;
	OUT.misc2 = IN.misc2;
#endif

	return OUT;
}

struct patchConstantInfo
{
	float Edges[3] : SV_TessFactor;
	float Inside[1] : SV_InsideTessFactor;
};

// Patch Constant Function.
patchConstantInfo ptfxSpriteCommonHSPatchFunction(InputPatch<ptfxSpriteVSInfo_CtrlPoint, 3> PatchPoints,  uint PatchID : SV_PrimitiveID)
{
	float TessFactor = rage_ComputePNTriangleTessellationFactor_DepthBased(PatchPoints[0].centerPos); 

	patchConstantInfo Output;
	Output.Edges[0] = TessFactor;	
	Output.Edges[1] = TessFactor;	
	Output.Edges[2] = TessFactor;	
	Output.Inside[0] = TessFactor;

	return Output;
}

// Hull shader.
[domain("tri")]
[partitioning("fractional_odd")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(3)]
[patchconstantfunc("ptfxSpriteCommonHSPatchFunction")]
[maxtessfactor(15.0)]
ptfxSpriteVSInfo_CtrlPoint ptfxSpriteCommonHS(InputPatch<ptfxSpriteVSInfo_CtrlPoint, 3> PatchPoints, uint i : SV_OutputControlPointID, uint PatchID : SV_PrimitiveID)
{

	ptfxSpriteVSInfo_CtrlPoint Output;

	// Our control points match the vertex shader output.
	Output = PatchPoints[i];

	return Output;
}

ptfxSpriteVSInfo ptfxSpriteCommonDS_EvaluateAt(patchConstantInfo PatchInfo, float3 WUV, const OutputPatch<ptfxSpriteVSInfo_CtrlPoint, 3> PatchPoints)
{
	ptfxSpriteVSInfo Arg;

	Arg.centerPos = PatchPoints[0].centerPos;
	Arg.normal = PatchPoints[0].normal;
	Arg.col = PatchPoints[0].col;

	Arg.misc = PatchPoints[0].misc;
	Arg.custom1 = PatchPoints[0].custom1;
	Arg.custom2 = PatchPoints[0].custom2;
	Arg.custom3 = PatchPoints[0].custom3;

	Arg.wldPos = RAGE_COMPUTE_BARYCENTRIC(WUV, PatchPoints, wldPos);
	Arg.texUVs = RAGE_COMPUTE_BARYCENTRIC(WUV, PatchPoints, texUVs);
	Arg.normUV = RAGE_COMPUTE_BARYCENTRIC(WUV, PatchPoints, normUV);

#if PTFX_USE_INSTANCING
	Arg.offsetUp = PatchPoints[0].offsetUp;
	Arg.offsetDown = PatchPoints[0].offsetDown;
	Arg.offsetRight = PatchPoints[0].offsetRight;
	Arg.clipRegion = PatchPoints[0].clipRegion;

	Arg.misc1 = PatchPoints[0].misc;
	Arg.misc2 = PatchPoints[0].misc2;
#endif

	return Arg;
}

#endif //PTFX_TESSELATE_SPRITES

///////////////////////////////////////////////////////////////////////////////
// PIXEL SHADERS
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// ptfxSpriteCommonPS
ptfxOutputStruct ptfxSpriteCommonPSImpl(ptfxSpritePSIn IN, bool isLit, bool isSoft, bool packColor, bool isNormalSpec, bool isScreenSpace, bool isWaterRefraction, int diffuseMode, bool isShadow, bool isThermalEffect IMPLEMENT_BLEND_MODES_AS_PASSES_PARAM(int blendmode), bool useInstancing)
{
#if (__LOW_QUALITY)
	isSoft = false;
#endif // __LOW_QUALITY

#if PS_DEBUG_OUTPUT
	return IN.col;
#endif

	float2 vPos = IN.pos.xy;

#if PTFX_SHADOWS_INSTANCING && !PTFX_SHADOWS_INSTANCING_GEOMSHADER
	if( useInstancing )
	{
		uint instID = IN.ptfxInfo_CBInstIndex;
		float cascadeHeight = CASCADE_SHADOWS_RES_Y;
		float yStart = instID * cascadeHeight;
		float yEnd = yStart + cascadeHeight;
		rageDiscard((vPos.y < yStart || vPos.y > yEnd));
		IN.fogParam.x = 0.0f;
	}
#endif

	// compute the final colour
	half4 texColor = 0.0f;
	half4 color = ptfxComputeColorAndLighting(IN, vPos, isLit, isNormalSpec, isSoft, isShadow, isScreenSpace, false, diffuseMode, texColor);

	if (gGlobalFogIntensity > 0.0f)
	{
		IN.fogParam.w *= ptfxGetFogRayIntensity(vPos.xy);
	}

	const half opacity = IN.col.w*(1 - IN.fogParam.w);

	if (IMPLEMENT_BLEND_MODES_AS_PASSES_SWITCH(blendmode, gBlendMode) == BLEND_MODE_NORMAL_V)									// transparent when texColor = {-,-,-,0}
	{
																			// don't fog opacity .. doesn't fog at the same rate
	}
	else if (IMPLEMENT_BLEND_MODES_AS_PASSES_SWITCH(blendmode, gBlendMode) == BLEND_MODE_ADD_V)									// transparent when texColor = {0,0,0,-} (texture alpha not used)
	{
		color.xyz = opacity*color.xyz;
		color.w   = 1;
	}
	else
	{
		color = 0;
	}

#if __XENON || __SHADERMODEL >= 40
	color.a = saturate(color.a) *gInvColorExpBias;
#endif

	if (IMPLEMENT_BLEND_MODES_AS_PASSES_SWITCH(blendmode, gBlendMode) == BLEND_MODE_NORMAL_V)
	{
		ptfxSpriteGlobalPostProcess(color, IN, !isScreenSpace, isWaterRefraction);
	}
	else if (IMPLEMENT_BLEND_MODES_AS_PASSES_SWITCH(blendmode, gBlendMode) == BLEND_MODE_ADD_V)
	{
		ptfxSpriteGlobalPostProcess(color, IN, false, isWaterRefraction);

		// if rendering to a downsampled buffer that will be later composited onto the fullscreen surface,
		// we need to ouput the luminance as the alpha to allow for transparent pixels when blending additively		
		float luminance = dot(color.rgb, float3(0.299, 0.587, 0.114));
		color.a = luminance;
	}

	// need to do this when outputting to FP16
	color.a = saturate(color.a);

	half4 result;

	//IsShadow needs only alpha component
	//the else part changes only the color components
	if(isShadow)
	{
		result = color.aaaa;
	}
	else if( packColor )
	{
		if(isWaterRefraction && ptfx_UseWaterGammaCompression)
		{
			result = color;

		}
		else
		{
			result = PackHdr(color);
		}
	}
	else
	{
		result = color;
	}

	if (isThermalEffect)
	{
		result.rgba *= (IN.ptfxInfo_emissiveIntensity > 0.0f) ? dot(texColor,1.0f.xxxx) : 0.0f;
	}

	ptfxOutputStruct outStruct;
	outStruct.col0 = result;
#if PTFX_APPLY_DOF_TO_PARTICLES
	outStruct.dof.depth = IN.ptfxInfo_depthW; //storing Linear Depth
	outStruct.dof.alpha = result.a * gGlobalParticleDofAlphaScale;
#endif //PTFX_APPLY_DOF_TO_PARTICLES

	return outStruct;
}

struct PsShadowStruct
{
	half4 col	: COLOR;
	float depth : DEPTH;
};

ptfxOutputStruct ptfxSpriteCommonPS(ptfxSpritePSIn IN, bool isLit, bool isSoft, bool packColor, bool isNormalSpec, bool isScreenSpace, bool isWaterRefraction, int diffuseMode, bool isShadow IMPLEMENT_BLEND_MODES_AS_PASSES_PARAM(int blendmode))
{
	return ptfxSpriteCommonPSImpl(IN, isLit, isSoft, packColor, isNormalSpec, isScreenSpace, isWaterRefraction, diffuseMode, isShadow, false IMPLEMENT_BLEND_MODES_AS_PASSES_PARAM(blendmode), false);
}

PsShadowStruct ptfxSpriteCommonShadowPS(ptfxSpritePSIn IN, bool isLit, bool isSoft, bool packColor, bool isNormalSpec, bool isScreenSpace, bool isWaterRefraction, int diffuseMode, bool isShadow IMPLEMENT_BLEND_MODES_AS_PASSES_PARAM(int blendmode))
{

	ptfxOutputStruct commonOUT;
	commonOUT = ptfxSpriteCommonPSImpl(IN, isLit, isSoft, packColor, isNormalSpec, isScreenSpace, isWaterRefraction, diffuseMode, true, false IMPLEMENT_BLEND_MODES_AS_PASSES_PARAM(blendmode), true);
	PsShadowStruct OUT;
	OUT.col = commonOUT.col0;
#if __SHADERMODEL >= 40
	float depth = IN.pos.z;
#else
	float depth = 0;	// n/a in SM30
#endif
	OUT.depth = ptfxGetShadowMapBiasedDepth(IN.pos.xy, depth, IN.ptfxInfo_PS_ShadowAmount);
	return OUT;
}

///////////////////////////////////////////////////////////////////////////////
// ptfxSpriteRefractPS
ptfxOutputStruct ptfxSpriteRefractPS(ptfxSpritePSIn IN, bool isLit, bool isSoft, bool isNormalSpec, bool isScreenSpace, bool isShadow IMPLEMENT_BLEND_MODES_AS_PASSES_PARAM(int blendmode))
{
#if (__LOW_QUALITY)
	isSoft = false;
#endif // __LOW_QUALITY

#if PS_DEBUG_OUTPUT 
	return (half4)IN.col;
#endif
	float2 vPos = IN.pos.xy;

	if (gGlobalFogIntensity > 0.0f)
		IN.fogParam.w *= ptfxGetFogRayIntensity(vPos.xy);

	// compute the diffuse colour
	half4 xyNormal_zAlpha_wSpecInt = h4tex2D(RefractionMapTexSampler, IN.normUV.xy);
	xyNormal_zAlpha_wSpecInt.xy = xyNormal_zAlpha_wSpecInt.xy*2.0f - 1.0f;

	float distortionNormalBlend = xyNormal_zAlpha_wSpecInt.z * IN.col.a;
	xyNormal_zAlpha_wSpecInt.xy *= gNormalMapMult.xx*100.0f.xx * distortionNormalBlend;
	half4 perturbedUvs = vPos.xyxy + xyNormal_zAlpha_wSpecInt.xyxy;	
	perturbedUvs.xy *= gInvScreenSize.xy;

	// compute the final colour
	half4 texColor = 0.0f;
	half4 color = ptfxComputeColorAndLighting(IN, perturbedUvs.zw, isLit, isNormalSpec, isSoft, isShadow, isScreenSpace, true, DIFFUSE_MODE_RGBA, texColor);
	half3 refractedCol = UnpackHdr( half4(ptfxSpriteSampleDistortion( perturbedUvs.xy, distortionNormalBlend > 0.0001f ), 1.0f )).rgb;

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// compute the final colour
	half4 finalColor = float4(lerp(refractedCol.rgb, color.rgb, color.a), distortionNormalBlend);

	// post process
	ptfxSpriteGlobalPostProcess(finalColor, IN, false, false);

	ptfxOutputStruct outStruct;
	outStruct.col0 = PackHdr(finalColor);
#if PTFX_APPLY_DOF_TO_PARTICLES
	outStruct.dof.depth = IN.ptfxInfo_depthW; //storing Linear Depth
	outStruct.dof.alpha = color.a * gGlobalParticleDofAlphaScale;
#endif //PTFX_APPLY_DOF_TO_PARTICLES

	// pack 
	return outStruct;
}


ptfxOutputStruct PS_none(ptfxSpritePSIn IN) : COLOR
{
	ptfxOutputStruct outStruct;
	outStruct.col0 = float4(1.0f, 0.0f, 1.0f, 1.0f);
#if PTFX_APPLY_DOF_TO_PARTICLES
	outStruct.dof.depth = float4(0.0.xxxx);
#endif //PTFX_APPLY_DOF_TO_PARTICLES

	return outStruct; 
}


ptfxOutputStruct PS_none_shadow(ptfxSpritePSIn IN) : COLOR
{
	ptfxOutputStruct outStruct;
	outStruct.col0 = float4(1.0f, 1.0f, 1.0f, 1.0f);
#if PTFX_APPLY_DOF_TO_PARTICLES
	outStruct.dof.depth = 0.0f;
	outStruct.dof.alpha = 0.0f;
#endif //PTFX_APPLY_DOF_TO_PARTICLES

	return outStruct; 
}


ptfxOutputStruct ptfxSpriteProjCommonPS(ptfxSpritePSIn IN, int isShadow, bool isWaterRefraction IMPLEMENT_BLEND_MODES_AS_PASSES_PARAM(int blendmode))
{
	// derive projection
	float2 vPos = IN.pos.xy;
	float3 worldPos;
	ptfxGetWorldPosFromVpos(vPos, IN.tangent, worldPos);
	float3 diff = worldPos - IN.wldPos.xyz;

	float ooSize = IN.wldPos.w;
	float2 texUV = (float2(dot(IN.normUV.zw, diff.xy), dot(IN.normal.xy, diff.xy))*(0.5f*ooSize)) + 0.5f;

	// alpha out any area outside 0...1 texture coords (avoids wrapping within the decal when viewed at an angle)
	if (texUV.x<0.0f) IN.col.a = 0.0f;
	if (texUV.x>1.0f) IN.col.a = 0.0f;
	if (texUV.y<0.0f) IN.col.a = 0.0f;
	if (texUV.y>1.0f) IN.col.a = 0.0f;

	// compute UVs for frames
	float2 uvStep = IN.ptfxProj_uvStep;
	float4 uvIndices;
	uvIndices.xy = IN.ptfxProj_idxA;
	uvIndices.zw = IN.ptfxProj_idxB;
	float4 texUVs = ((uvIndices+texUV.xyxy) * uvStep.xyxy);	

	// convert from srgb to linear space
	half4 diffColor1 = ProcessDiffuseColor(tex2D(DiffuseTexSampler, texUVs.xy));
	half4 diffColor2 = ProcessDiffuseColor(tex2D(DiffuseTexSampler, texUVs.zw));

	// compute the diffuse colour
	half4 texColor;
	ptfxSpriteBlendDiffusePixelColor(texColor, diffColor1, diffColor2, IN);

	// compute the final colour
	half4 color;


	// first shadow the directional light
	{
#if USE_SHADED_PTFX_MAP
		float4 shadowBuffer = tex2D(ShadedPtFxSampler, IN.ptfxProj_shadowUV);

		// blend value between heights (this can be moved to the vertex shader if an extra interpolator is found)
		float blendValue = abs((IN.wldPos.z - shadedPtFxMapLoHeight) / (shadedPtFxMapHiHeight - shadedPtFxMapLoHeight));

		float shadowAmount = lerp(shadowBuffer.r, shadowBuffer.g, blendValue);
		float outOfRangeFadeToAmount = shadedPtFxCloudShadowAmount * gNaturalAmbientScale;
		shadowAmount = lerp(outOfRangeFadeToAmount, shadowAmount, IN.ptfxProj_PS_shadowAmount);
#else
		float shadowAmount = IN.ptfxProj_PS_shadowAmount;
#endif
		// add in the directional lighting component, but shadowed.
		// IN.normal.xyz is the directional light component
		// IN.color.xyz is the rest of the light
		IN.col.rgb += (IN.ptfxProj_dirLight * shadowAmount);
	}

	color = texColor*IN.col;

	// blend final colour
	bool isScreenSpace = false; 
	bool isLit = false;

	if (gGlobalFogIntensity > 0.0f)
		IN.fogParam.w *= ptfxGetFogRayIntensity(vPos.xy);
	const half opacity = IN.col.w*(1 - IN.fogParam.w);
	const half texOpacity = 1;

	if (IMPLEMENT_BLEND_MODES_AS_PASSES_SWITCH(blendmode, gBlendMode) == BLEND_MODE_NORMAL_V)									// transparent when texColor = {-,-,-,0}
	{
																			// don't fog opacity .. doesn't fog at the same rate
	}
	else if (IMPLEMENT_BLEND_MODES_AS_PASSES_SWITCH(blendmode, gBlendMode) == BLEND_MODE_ADD_V)									// transparent when texColor = {0,0,0,-} (texture alpha not used)
	{
		color.xyz = opacity*texOpacity*color.xyz;
		color.w   = 1;
	}
	else
	{
		color = 0;
	}

	XENON_ONLY(color.a = saturate(color.a)*gInvColorExpBias);				// not really sure if this is correct, need to test this on xenon

	if (IMPLEMENT_BLEND_MODES_AS_PASSES_SWITCH(blendmode, gBlendMode) == BLEND_MODE_NORMAL_V)
	{
		ptfxSpriteGlobalPostProcess(color, IN, !isScreenSpace, isWaterRefraction);
	}
	else if (IMPLEMENT_BLEND_MODES_AS_PASSES_SWITCH(blendmode, gBlendMode) == BLEND_MODE_ADD_V)
	{
		ptfxSpriteGlobalPostProcess(color, IN, false, isWaterRefraction);

		// if rendering to a downsampled buffer that will be later composited onto the fullscreen surface,
		// we need to ouput the luminance as the alpha to allow for transparent pixels when blending additively		
		half luminance = dot(color.rgb, half3(0.299, 0.587, 0.114));
		color.a = luminance;
	}

	half4 result = 0;
	if(isWaterRefraction && ptfx_UseWaterGammaCompression)
	{
		result = color;

	}
	else
	{
		result = PackHdr(color);
	}

	ptfxOutputStruct outStruct;
	outStruct.col0 = result;
#if PTFX_APPLY_DOF_TO_PARTICLES
	outStruct.dof.depth = 0.0;
	outStruct.dof.alpha = result.a * gGlobalParticleDofAlphaScale;
#endif //PTFX_APPLY_DOF_TO_PARTICLES

	return outStruct;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Thermal Pixel shaders
ptfxOutputStruct ptfxThermalPack(ptfxSpritePSIn IN, ptfxOutputStruct inStruct IMPLEMENT_BLEND_MODES_AS_PASSES_PARAM(int blendmode))
{
	const float ooMaxHDRRange = 1.0f/8.0f;
	const float lumThreshold = 0.5f;
	float3 normCol = saturate((inStruct.col0.rgb-lumThreshold.xxx)*ooMaxHDRRange);
	half luminance = dot(normCol.rgb, float3(0.299, 0.587, 0.114));
	
	half alpha = inStruct.col0.w*saturate(luminance);
	half isVisible = (alpha == 0.0f ? 0.0f : 1.0f);
	half4 result;
	
	result.x = 0.0f;
	result.y = SeeThrough_Pack(IN.ptfxInfo_projDepth).y * isVisible;
	
	const half hotProfileScale = 0.75f;
	result.z = saturate(hotProfileScale+alpha*0.24f)*isVisible;
	
	// only output the heat scale multiplier if the pixel is visible
	// also, instead of a fixed value, let it be the luminance of the pixel
	result.w = isVisible ? saturate(luminance) : 0.0f;

	ptfxOutputStruct outStruct;
	outStruct.col0 = result;
#if PTFX_APPLY_DOF_TO_PARTICLES
	outStruct.dof.depth = 0.0;
	outStruct.dof.alpha = result.a * gGlobalParticleDofAlphaScale;
#endif //PTFX_APPLY_DOF_TO_PARTICLES

	return outStruct;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Diffuse mode shaders.
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if IMPLEMENT_BLEND_MODES_AS_PASSES

// Vertex shaders.
#if PTFX_SHADOWS_INSTANCING_GEOMSHADER
#define DEF_ptfxSpriteCommonVS(name, isLit, isSoft, isScreenSpace, isNormalSpec, isWaterRefraction) \
ptfxSpriteVSOut JOIN(VS_, name) (ptfxSpriteVSInfo IN) { return ptfxSpriteCommonVS(IN, isLit, isSoft, isScreenSpace, isNormalSpec, false, false,false, isWaterRefraction);} \
USE_PARTICLE_SHADOWS_ONLY(ptfxSpriteVSOut JOIN3(VS_, name, _shadow) (ptfxSpriteVSInfo IN) { return ptfxSpriteCommonVS(IN, isLit, isSoft, isScreenSpace, isNormalSpec, true, false,false, isWaterRefraction);})\
USE_PARTICLE_SHADOWS_ONLY(ptfxSpriteVSOut JOIN3(VS_, name, _shadow_instanced) (ptfxSpriteVSInfo IN) { return ptfxSpriteCommonVS(IN, isLit, isSoft, isScreenSpace, isNormalSpec, true, false,true, isWaterRefraction);})\
DEF_TERMINATOR
#else
#define DEF_ptfxSpriteCommonVS(name, isLit, isSoft, isScreenSpace, isNormalSpec, isWaterRefraction) \
ptfxSpriteVSOut JOIN(VS_, name) (ptfxSpriteVSInfo IN) { return ptfxSpriteCommonVS(IN, isLit, isSoft, isScreenSpace, isNormalSpec, false, false,false, isWaterRefraction);} \
USE_PARTICLE_SHADOWS_ONLY(ptfxSpriteVSOut JOIN3(VS_, name, _shadow) (ptfxSpriteVSInfo IN) { return ptfxSpriteCommonVS(IN, isLit, isSoft, isScreenSpace, isNormalSpec, true, false,true, isWaterRefraction);})\
DEF_TERMINATOR
#endif

DEF_ptfxSpriteCommonVS(none,					false, false, false, false, false);
DEF_ptfxSpriteCommonVS(lit,						true, false, false, false, false);
DEF_ptfxSpriteCommonVS(soft,					false, true, false, false, false);
DEF_ptfxSpriteCommonVS(soft_waterrefract,		false, true, false, false, true);
DEF_ptfxSpriteCommonVS(lit_soft,				true, true, false, false, false);
DEF_ptfxSpriteCommonVS(lit_soft_waterrefract,	true, true, false, false, true);
DEF_ptfxSpriteCommonVS(screen,					false, false, true, false, false);
DEF_ptfxSpriteCommonVS(lit_screen,				true, false, true, false, false);
DEF_ptfxSpriteCommonVS(lit_normalspec,			true, false, false, true, false);
DEF_ptfxSpriteCommonVS(lit_soft_normalspec,		true, true, false, true, false);
DEF_ptfxSpriteCommonVS(lit_screen_normalspec,	true, false, true, true, false);

// Domain shaders.
#if PTFX_TESSELATE_SPRITES
#define DEF_ptfxSpriteCommonDS(name, isLit, isSoft, isScreenSpace, isNormalSpec, isWaterRefraction) \
[domain("tri")] ptfxSpriteVSOut JOIN(DS_, name) (patchConstantInfo PatchInfo, float3 WUV : SV_DomainLocation, const OutputPatch<ptfxSpriteVSInfo_CtrlPoint, 3> PatchPoints) \
{ ptfxSpriteVSInfo Arg = ptfxSpriteCommonDS_EvaluateAt(PatchInfo, WUV, PatchPoints); return ptfxSpriteCommonVS(Arg, isLit, isSoft, isScreenSpace, isNormalSpec, false, true,false, isWaterRefraction);} \
DEF_TERMINATOR

DEF_ptfxSpriteCommonDS(lit,						true, false, false, false, true);
DEF_ptfxSpriteCommonDS(lit_soft,				true, true, false, false, true);
DEF_ptfxSpriteCommonDS(lit_screen,				true, false, true, false, true);
DEF_ptfxSpriteCommonDS(lit_soft_waterrefract,	true, true, false, false, true);
#endif //PTFX_TESSELATE_SPRITES

// Pixel shaders.
#define DEF_ptfxSpriteCommonPS_BlendMode(name, isLit, isSoft, packColor, isNormalSpec, isScreenSpace, isWaterRefraction, diffuseMode, blendmode, blendmodeasvalue) \
ptfxOutputStruct JOIN3(PS_, name, blendmode) (ptfxSpritePSIn IN) { return ptfxSpriteCommonPS(IN, isLit, isSoft, packColor, isNormalSpec, isScreenSpace, isWaterRefraction, diffuseMode, false, blendmodeasvalue); } \
USE_PARTICLE_SHADOWS_ONLY(PsShadowStruct JOIN4(PS_, name, blendmode, _shadow) (ptfxSpritePSIn IN) { return ptfxSpriteCommonShadowPS(IN, isLit, isSoft, packColor, isNormalSpec, isScreenSpace, isWaterRefraction, diffuseMode, true, blendmodeasvalue); })\
DEF_TERMINATOR

#define DEF_ptfxSpriteCommonPS(name, isLit, isSoft, packColor, isNormalSpec, isScreenSpace, isWaterRefraction, diffuseMode) \
DEF_ptfxSpriteCommonPS_BlendMode(name, isLit, isSoft, packColor, isNormalSpec, isScreenSpace, isWaterRefraction, diffuseMode, BLEND_MODE_NORMAL, BLEND_MODE_NORMAL_V) \
DEF_ptfxSpriteCommonPS_BlendMode(name, isLit, isSoft, packColor, isNormalSpec, isScreenSpace, isWaterRefraction, diffuseMode, BLEND_MODE_ADD, BLEND_MODE_ADD_V) \
DEF_TERMINATOR


#define DEF_ptfxSpriteRefractPS_BlendMode(name, isLit, isSoft, isNormalSpec, isScreenSpace, blendmode, blendmodeasvalue) \
ptfxOutputStruct JOIN3(PS_, name, blendmode) (ptfxSpritePSIn IN) { return ptfxSpriteRefractPS(IN, isLit, isSoft, isNormalSpec, isScreenSpace, false, blendmodeasvalue); } \
USE_PARTICLE_SHADOWS_ONLY(ptfxOutputStruct JOIN4(PS_, name, blendmode, _shadow) (ptfxSpritePSIn IN) { return ptfxSpriteRefractPS(IN, isLit, isSoft, isNormalSpec, isScreenSpace, true, blendmodeasvalue); })\
DEF_TERMINATOR

#define DEF_ptfxSpriteRefractPS(name, isLit, isSoft, isNormalSpec, isScreenSpace) \
DEF_ptfxSpriteRefractPS_BlendMode(name, isLit, isSoft, isNormalSpec, isScreenSpace, BLEND_MODE_NORMAL, BLEND_MODE_NORMAL_V) \
DEF_ptfxSpriteRefractPS_BlendMode(name, isLit, isSoft, isNormalSpec, isScreenSpace, BLEND_MODE_ADD, BLEND_MODE_ADD_V) \
DEF_TERMINATOR


// RGBA pixel shaders
DEF_ptfxSpriteCommonPS(RGBA,						false, false, true, false, false, false, DIFFUSE_MODE_RGBA);
DEF_ptfxSpriteCommonPS(RGBA_screen,					false, false, true, false, true, false, DIFFUSE_MODE_RGBA);
DEF_ptfxSpriteCommonPS(RGBA_lit,					true,  false, true, false, false, false, DIFFUSE_MODE_RGBA);
DEF_ptfxSpriteCommonPS(RGBA_lit_screen,				true,  false, true, false, true, false, DIFFUSE_MODE_RGBA);
DEF_ptfxSpriteCommonPS(RGBA_soft,					false, true, true, false, false, false, DIFFUSE_MODE_RGBA);
DEF_ptfxSpriteCommonPS(RGBA_soft_waterrefract,		false, true, true, false, false, true, DIFFUSE_MODE_RGBA);
DEF_ptfxSpriteCommonPS(RGBA_lit_soft,				true,  true, true, false, false, false, DIFFUSE_MODE_RGBA);
DEF_ptfxSpriteCommonPS(RGBA_lit_normalspec,			true,  false, true, true, false, false, DIFFUSE_MODE_RGBA);
DEF_ptfxSpriteCommonPS(RGBA_lit_soft_normalspec,	true,  true, true, true, false, false, DIFFUSE_MODE_RGBA);
DEF_ptfxSpriteCommonPS(RGBA_lit_screen_normalspec,  true,  true, true, true, true, false, DIFFUSE_MODE_RGBA);
DEF_ptfxSpriteCommonPS(RGBA_lit_soft_waterrefract,	true,  true, true, false, false, true, DIFFUSE_MODE_RGBA);

// XXXX pixel shaders
DEF_ptfxSpriteCommonPS(XXXX,						false, false, true, false, false, false, DIFFUSE_MODE_XXXX);
DEF_ptfxSpriteCommonPS(XXXX_screen,					false, false, true, false, true, false, DIFFUSE_MODE_XXXX);
DEF_ptfxSpriteCommonPS(XXXX_lit,					true,  false, true, false, false, false, DIFFUSE_MODE_XXXX);
DEF_ptfxSpriteCommonPS(XXXX_lit_screen,				true,  false, true, false, true, false, DIFFUSE_MODE_XXXX);
DEF_ptfxSpriteCommonPS(XXXX_soft,					false, true, true, false, false, false, DIFFUSE_MODE_XXXX);
DEF_ptfxSpriteCommonPS(XXXX_lit_soft,				true,  true, true, false, false, false, DIFFUSE_MODE_XXXX);
DEF_ptfxSpriteCommonPS(XXXX_lit_normalspec,			true,  false, true, true, false, false, DIFFUSE_MODE_XXXX);
DEF_ptfxSpriteCommonPS(XXXX_lit_screen_normalspec,	true,  false, true, true, true, false, DIFFUSE_MODE_XXXX);
DEF_ptfxSpriteCommonPS(XXXX_lit_soft_normalspec,	true,  true, true, true, false, false, DIFFUSE_MODE_XXXX);

// RG_Blend pixel shaders
DEF_ptfxSpriteCommonPS(RG_Blend,			false, false, true, false, false, false, DIFFUSE_MODE_RG_BLEND);
DEF_ptfxSpriteCommonPS(RG_Blend_lit,		true,  false, true, false, false, false, DIFFUSE_MODE_RG_BLEND);
DEF_ptfxSpriteCommonPS(RG_Blend_soft,		false, true, true, false, false, false, DIFFUSE_MODE_RG_BLEND);
DEF_ptfxSpriteCommonPS(RG_Blend_lit_soft,	true,  true, true, false, false, false, DIFFUSE_MODE_RG_BLEND);

// RGB pixel shaders
DEF_ptfxSpriteCommonPS(RGB,						false, false, true, false, false, false, DIFFUSE_MODE_RGB);
DEF_ptfxSpriteCommonPS(RGB_screen,				false, false, true, false, true, false, DIFFUSE_MODE_RGB);
DEF_ptfxSpriteCommonPS(RGB_lit,					true,  false, true, false, false, false, DIFFUSE_MODE_RGB);
DEF_ptfxSpriteCommonPS(RGB_lit_screen,			true,  false, true, false, true, false, DIFFUSE_MODE_RGB);
DEF_ptfxSpriteCommonPS(RGB_soft,				false, true, true, false, false, false, DIFFUSE_MODE_RGB);
DEF_ptfxSpriteCommonPS(RGB_soft_waterrefract,	false, true, true, false, false, true, DIFFUSE_MODE_RGB);
DEF_ptfxSpriteCommonPS(RGB_lit_soft,			true,  true, true, false, false, false, DIFFUSE_MODE_RGB);
DEF_ptfxSpriteCommonPS(RGB_lit_normalspec,		true,  false, true, true, false, false, DIFFUSE_MODE_RGB);
DEF_ptfxSpriteCommonPS(RGB_lit_soft_normalspec,	true,  true, true, true, false, false, DIFFUSE_MODE_RGB);
DEF_ptfxSpriteCommonPS(RGB_lit_screen_normalspec,  true,  true, true, true, true, false, DIFFUSE_MODE_RGB);
DEF_ptfxSpriteCommonPS(RGB_lit_soft_waterrefract, true,  true, true, false, false, true, DIFFUSE_MODE_RGB);

// Refract shaders.
DEF_ptfxSpriteRefractPS(RGB_refract, false, false, false, false);
DEF_ptfxSpriteRefractPS(RGB_screen_refract, false, false, false, true);
DEF_ptfxSpriteRefractPS(RGB_soft_refract, false, true, false, false);
DEF_ptfxSpriteRefractPS(RGB_refract_normalspec, false, false, true, false);
DEF_ptfxSpriteRefractPS(RGB_screen_refract_normalspec, false, false, true, true);
DEF_ptfxSpriteRefractPS(RGB_soft_refract_normalspec, false, true, true, false);


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Water projection.
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Vertex shaders.
#if PTFX_SHADOWS_INSTANCING_GEOMSHADER
#define DEF_VS_Proj(name, isWaterRefraction) \
ptfxSpriteVSOut JOIN(VS_, name) (ptfxSpriteVSInfoProj IN) { return ptfxSpriteProjCommon(IN, false,false, isWaterRefraction);} \
USE_PARTICLE_SHADOWS_ONLY(ptfxSpriteVSOut JOIN3(VS_, name,_shadow) (ptfxSpriteVSInfoProj IN) { return ptfxSpriteProjCommon(IN, true,false, isWaterRefraction);})\
USE_PARTICLE_SHADOWS_ONLY(ptfxSpriteVSOut JOIN3(VS_, name,_shadow_instanced) (ptfxSpriteVSInfoProj IN) { return ptfxSpriteProjCommon(IN, true,true, isWaterRefraction);})\
DEF_TERMINATOR
#elif PTFX_SHADOWS_INSTANCING
#define DEF_VS_Proj(name, isWaterRefraction) \
ptfxSpriteVSOut JOIN(VS_, name) (ptfxSpriteVSInfoProj IN) { return ptfxSpriteProjCommon(IN, false,false, isWaterRefraction);} \
USE_PARTICLE_SHADOWS_ONLY(ptfxSpriteVSOut JOIN3(VS_, name,_shadow) (ptfxSpriteVSInfoProj IN) { return ptfxSpriteProjCommon(IN, true,false, isWaterRefraction);})\
DEF_TERMINATOR
#else
#define DEF_VS_Proj(name, isWaterRefraction) \
ptfxSpriteVSOut JOIN(VS_, name) (ptfxSpriteVSInfo IN) { return ptfxSpriteProjCommon(IN, false,false, isWaterRefraction);} \
USE_PARTICLE_SHADOWS_ONLY(ptfxSpriteVSOut JOIN3(VS_, name,_shadow) (ptfxSpriteVSInfo IN) { return ptfxSpriteProjCommon(IN, true,false, isWaterRefraction);})\
DEF_TERMINATOR
#endif

#define DEF_PS_proj_BlendMode(name, isWaterRefraction, blendmode, blendmodeasvalue) \
ptfxOutputStruct JOIN3(PS_, name, blendmode) (ptfxSpritePSIn IN) { return ptfxSpriteProjCommonPS(IN, false, isWaterRefraction, blendmodeasvalue); } \
USE_PARTICLE_SHADOWS_ONLY(ptfxOutputStruct JOIN4(PS_, name, blendmode, _shadow) (ptfxSpritePSIn IN) { return ptfxSpriteProjCommonPS(IN, true, isWaterRefraction, blendmodeasvalue); })\
DEF_TERMINATOR

#define DEF_PS_Proj(name, isWaterRefraction) \
DEF_PS_proj_BlendMode(name, isWaterRefraction, BLEND_MODE_NORMAL, BLEND_MODE_NORMAL_V) \
DEF_PS_proj_BlendMode(name, isWaterRefraction, BLEND_MODE_ADD, BLEND_MODE_ADD_V) \
DEF_TERMINATOR

DEF_VS_Proj(proj, false);
DEF_VS_Proj(proj_waterrefract, true);
DEF_PS_Proj(proj, false);
DEF_PS_Proj(proj_waterrefract, true);

#if !defined(SHADER_FINAL)
half4 PS_DebugOverride_Alpha_BANK_ONLY(ptfxSpritePSIn IN) : COLOR
{
	float4 col = float4(1,0,0,0.75f);
	half4 result = PackHdr(col);
	return result;
}
#endif // !defined(SHADER_FINAL)

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Thermal shaders.
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Vertex shaders.
#if PTFX_SHADOWS_INSTANCING_GEOMSHADER
#define DEF_ptfxSpriteThermalVS() \
ptfxSpriteVSOut VS_thermal (ptfxSpriteVSInfo IN) { return VS_thermalFunc(IN, false,false);} \
USE_PARTICLE_SHADOWS_ONLY(ptfxSpriteVSOut VS_thermal_shadow (ptfxSpriteVSInfo IN) { return VS_thermalFunc(IN, true,false);})\
USE_PARTICLE_SHADOWS_ONLY(ptfxSpriteVSOut VS_thermal_shadow_instanced (ptfxSpriteVSInfo IN) { return VS_thermalFunc(IN, true,true);})\
DEF_TERMINATOR
#else
#define DEF_ptfxSpriteThermalVS() \
ptfxSpriteVSOut VS_thermal (ptfxSpriteVSInfo IN) { return VS_thermalFunc(IN, false,false);} \
USE_PARTICLE_SHADOWS_ONLY(ptfxSpriteVSOut VS_thermal_shadow (ptfxSpriteVSInfo IN) { return VS_thermalFunc(IN, true,true);})\
DEF_TERMINATOR
#endif

DEF_ptfxSpriteThermalVS();

// Pixel shaders.
#define DEF_ptfxSpriteThermalPS_BlendMode(name, isLit, isSoft, packColor, isNormalSpec, isScreenSpace, diffuseMode, blendmode, blendmodeasvalue) \
ptfxOutputStruct JOIN3(PS_, name, blendmode) (ptfxSpritePSIn IN) { ptfxOutputStruct color = ptfxSpriteCommonPSImpl(IN, isLit, isSoft, packColor, isNormalSpec, isScreenSpace, false, diffuseMode, false, true, blendmodeasvalue, false); return ptfxThermalPack(IN,color, blendmodeasvalue); } \
USE_PARTICLE_SHADOWS_ONLY(ptfxOutputStruct JOIN4(PS_, name, blendmode, _shadow) (ptfxSpritePSIn IN) { ptfxOutputStruct color = ptfxSpriteCommonPSImpl(IN, isLit, isSoft, packColor, isNormalSpec, isScreenSpace, false, diffuseMode, true, true, blendmodeasvalue, true); return ptfxThermalPack(IN,color, blendmodeasvalue); })\
DEF_TERMINATOR

#define DEF_ptfxSpriteThermalPS(name, isLit, isSoft, packColor, isNormalSpec, isScreenSpace, diffuseMode) \
DEF_ptfxSpriteThermalPS_BlendMode(name, isLit, isSoft, packColor, isNormalSpec, isScreenSpace, diffuseMode, BLEND_MODE_NORMAL, BLEND_MODE_NORMAL_V) \
DEF_ptfxSpriteThermalPS_BlendMode(name, isLit, isSoft, packColor, isNormalSpec, isScreenSpace, diffuseMode, BLEND_MODE_ADD, BLEND_MODE_ADD_V) \
DEF_TERMINATOR


DEF_ptfxSpriteThermalPS(RGBA_Thermal, false, false, false, false, true, DIFFUSE_MODE_RGBA);
DEF_ptfxSpriteThermalPS(XXXX_Thermal, false, false, false, false, true, DIFFUSE_MODE_XXXX);
DEF_ptfxSpriteThermalPS(RG_Blend_Thermal, false, false, false, false, true, DIFFUSE_MODE_RG_BLEND);
DEF_ptfxSpriteThermalPS(RGB_Thermal, false, false, false, false, true, DIFFUSE_MODE_RGB);

///////////////////////////////////////////////////////////////////////////////
// TECHNIQUES
///////////////////////////////////////////////////////////////////////////////
#define DEF_ShadowPass(blendmode, _vs, _ps) pass JOIN(blendmode, _shadow) { VertexShader = compile VERTEXSHADER JOIN(_vs, _shadow)(); PixelShader  = compile PIXELSHADER JOIN(_ps, _shadow)()  CGC_FLAGS(CGC_DEFAULTFLAGS); }
#if PTFX_SHADOWS_INSTANCING_GEOMSHADER
#define DEF_InstShadowPass(blendmode, _vs, _ps) pass JOIN(blendmode, _shadow_instanced) { VertexShader = compile VSGS_SHADER JOIN(_vs, _shadow_instanced)(); SetGeometryShader(compileshader(gs_5_0, GS_InstPassThrough())); PixelShader  = compile PIXELSHADER JOIN(_ps, _shadow)()  CGC_FLAGS(CGC_DEFAULTFLAGS); }
#else
#define DEF_InstShadowPass(blendmode, _vs, _ps)
#endif

#define DEF_BlendModePassON(blendmode, vs, ps) \
pass blendmode \
{ \
	VertexShader = compile VERTEXSHADER vs(); \
	PixelShader  = compile PIXELSHADER JOIN(ps, blendmode)()  CGC_FLAGS(CGC_DEFAULTFLAGS); \
} \
USE_PARTICLE_SHADOWS_ONLY(DEF_ShadowPass(blendmode, vs, JOIN(ps, blendmode))) \
USE_PARTICLE_SHADOWS_ONLY(DEF_InstShadowPass(blendmode, vs, JOIN(ps, blendmode))) \
DEF_TERMINATOR


#define DEF_BlendModePassOFF(blendmode, vs, ps) \
pass blendmode \
{ \
	VertexShader = compile VERTEXSHADER VS_none(); \
	PixelShader  = compile PIXELSHADER PS_none()  CGC_FLAGS(CGC_DEFAULTFLAGS); \
} \
USE_PARTICLE_SHADOWS_ONLY(DEF_ShadowPass(blendmode, VS_none, PS_none)) \
USE_PARTICLE_SHADOWS_ONLY(DEF_InstShadowPass(blendmode, VS_none, PS_none)) \
DEF_TERMINATOR


#define DEF_TechniqueAllPasses(name, vs, ps, normal, add) \
technique name \
{ \
	JOIN(DEF_BlendModePass, normal)(BLEND_MODE_NORMAL, vs, ps) \
	JOIN(DEF_BlendModePass, add)(BLEND_MODE_ADD, vs, ps) \
} \
DEF_TERMINATOR

#if PTFX_TESSELATE_SPRITES
#define DEF_BlendModePassON_Tessellated(blendmode, vs, vsShadow, hs, ds, ps) \
pass blendmode \
{ \
	VertexShader = compile VSDS_SHADER vs(); \
	SetHullShader(compileshader(hs_5_0, hs)); \
	SetDomainShader(compileshader(ds_5_0, ds)); \
	PixelShader  = compile PIXELSHADER JOIN(ps, blendmode)()  CGC_FLAGS(CGC_DEFAULTFLAGS); \
} \
USE_PARTICLE_SHADOWS_ONLY(DEF_ShadowPass(blendmode, vsShadow, JOIN(ps, blendmode))) \
USE_PARTICLE_SHADOWS_ONLY(DEF_InstShadowPass(blendmode, vsShadow, JOIN(ps, blendmode))) \
DEF_TERMINATOR


#define DEF_BlendModePassOFF_Tessellated(blendmode, vs, vsShadow, hs, ds, ps) \
pass blendmode \
{ \
	VertexShader = compile VERTEXSHADER VS_none(); \
	PixelShader  = compile PIXELSHADER PS_none()  CGC_FLAGS(CGC_DEFAULTFLAGS); \
} \
USE_PARTICLE_SHADOWS_ONLY(DEF_ShadowPass(blendmode, VS_none, PS_none)) \
USE_PARTICLE_SHADOWS_ONLY(DEF_InstShadowPass(blendmode, VS_none, PS_none)) \
DEF_TERMINATOR

#define DEF_TechniqueAllPasses_Tessellated(name, vs, vsShadow, hs, ds, ps, normal, add) \
technique JOIN(name, _sm50) \
{ \
	JOIN(JOIN(DEF_BlendModePass, normal),_Tessellated)(BLEND_MODE_NORMAL, vs, vsShadow,hs, ds, ps) \
	JOIN(JOIN(DEF_BlendModePass, add),_Tessellated)(BLEND_MODE_ADD, vs, vsShadow,hs, ds, ps) \
} \
DEF_TERMINATOR
#endif //PTFX_TESSELATE_SPRITES

#define DEF_Technique(diffuseMode, normal, add) \
DEF_TechniqueAllPasses(diffuseMode, VS_none, JOIN(PS_, diffuseMode), normal, add) \
DEF_TERMINATOR

#define DEF_TechniqueVariant(diffuseMode, variant, normal, add) \
DEF_TechniqueAllPasses(JOIN3(diffuseMode, _, variant), JOIN(VS_, variant), JOIN4(PS_, diffuseMode, _, variant), normal, add) \
DEF_TERMINATOR

//creates tessellated and non-tessellated techniques
#if PTFX_TESSELATE_SPRITES
	#define DEF_TechniqueVariant_Tessellated(diffuseMode, variant, normal, add) \
	DEF_TechniqueAllPasses(JOIN3(diffuseMode, _, variant), JOIN(VS_, variant), JOIN4(PS_, diffuseMode, _, variant), normal, add) \
	DEF_TechniqueAllPasses_Tessellated(JOIN3(diffuseMode, _, variant), ptfxSpriteCommonVS_Tessellated, JOIN(VS_, variant), ptfxSpriteCommonHS(), JOIN(DS_, variant()), JOIN4(PS_, diffuseMode, _, variant), normal, add) \
	DEF_TERMINATOR

	#define DEF_TechniqueVariantVSDS_Tessellated(diffuseMode, vsds_variant, variant, normal, add) \
	DEF_TechniqueAllPasses(JOIN3(diffuseMode, _, variant), JOIN(VS_, vsds_variant), JOIN4(PS_, diffuseMode, _, variant), normal, add) \
	DEF_TechniqueAllPasses_Tessellated(JOIN3(diffuseMode, _, variant), ptfxSpriteCommonVS_Tessellated, JOIN(VS_, vsds_variant), ptfxSpriteCommonHS(), JOIN(DS_, vsds_variant()), JOIN4(PS_, diffuseMode, _, variant), normal, add) \
	DEF_TERMINATOR
#else
	#define DEF_TechniqueVariant_Tessellated(diffuseMode, variant, normal, add) \
	DEF_TechniqueAllPasses(JOIN3(diffuseMode, _, variant), JOIN(VS_, variant), JOIN4(PS_, diffuseMode, _, variant), normal, add) \
	DEF_TERMINATOR

#define DEF_TechniqueVariantVSDS_Tessellated(diffuseMode, vsds_variant, variant, normal, add) \
	DEF_TechniqueAllPasses(JOIN3(diffuseMode, _, variant), JOIN(VS_, vsds_variant), JOIN4(PS_, diffuseMode, _, variant), normal, add) \
	DEF_TERMINATOR
#endif //PTFX_TESSELATE_SPRITES

#define DEF_TechniqueVariantVS(diffuseMode, variant, vs_variant, normal, add) \
DEF_TechniqueAllPasses(JOIN3(diffuseMode, _, variant), JOIN(VS_, vs_variant), JOIN4(PS_, diffuseMode, _, variant), normal, add) \
DEF_TERMINATOR

#define DEF_TechniqueVariantVSPS(diffuseMode, variant, vs_variant, ps_variant, normal, add) \
DEF_TechniqueAllPasses(JOIN3(diffuseMode, _, variant), JOIN(VS_, vs_variant), JOIN4(PS_, diffuseMode, _, ps_variant), normal, add) \
DEF_TERMINATOR

#define DEF_TechniqueNameVSPS(name, vs, ps, normal, add) \
DEF_TechniqueAllPasses(name, vs, ps, normal, add) \
DEF_TERMINATOR

#include "ptfx_sprite.fxh"

#if !defined(SHADER_FINAL)

#define DEF_DebugOverride_Alpha_Pass() \
pass p0	{ VertexShader = compile VERTEXSHADER VS_none(); PixelShader  = compile PIXELSHADER PS_DebugOverride_Alpha_BANK_ONLY()  CGC_FLAGS(CGC_DEFAULTFLAGS);	} \
USE_PARTICLE_SHADOWS_ONLY(pass p0_shadow { VertexShader = compile VERTEXSHADER VS_none(); PixelShader  = compile PIXELSHADER PS_DebugOverride_Alpha_BANK_ONLY()  CGC_FLAGS(CGC_DEFAULTFLAGS);	}) \
DEF_TERMINATOR

technique DebugOverride_Alpha 
{
	DEF_DebugOverride_Alpha_Pass()
	DEF_DebugOverride_Alpha_Pass()
	DEF_DebugOverride_Alpha_Pass()
	DEF_DebugOverride_Alpha_Pass()
	DEF_DebugOverride_Alpha_Pass()
	DEF_DebugOverride_Alpha_Pass()
}
#endif // !defined(SHADER_FINAL)

#else //IMPLEMENT_BLEND_MODES_AS_PASSES

#endif //IMPLEMENT_BLEND_MODES_AS_PASSES

#if USE_SHADED_PTFX_MAP
struct shadedPtFxVSInfo
{
	float3 Position : POSITION;
	float2 UV	    : TEXCOORD0;
};

struct shadedPtFxPSInfo
{
	DECLARE_POSITION(pos)
	float3 Position : TEXCOORD0;
	half2  UV	    : TEXCOORD1;
};

shadedPtFxPSInfo VS_ShadedPtFxBufferGen(shadedPtFxVSInfo IN)
{
	shadedPtFxPSInfo OUT;

	OUT.pos = float4(IN.Position, 1);
	OUT.Position = IN.Position;
	OUT.UV = IN.UV;

	return OUT;
}

half4 PS_ShadedPtFxBufferGen(shadedPtFxPSInfo IN) : SV_Target0
{
	half4 retVal = half4(1,1,1,1);

	float3 camPos = gViewInverse[3].xyz;
	float3 normal = float3( 0.0, 0.0, -1.0 );
	
	float3 tapWorldPos = float3( camPos.x + IN.UV.x,
								 camPos.y + IN.UV.y,
								 shadedPtFxMapLoHeight );

	// cloud shadows at the lowest tap
	float cloudShadow = CalcCloudShadows(tapWorldPos);

	// first height tap (low to ground)
	retVal.r = CalcCascadeShadowsFastNoFade(camPos, tapWorldPos, normal, IN.UV) * cloudShadow;

	// second height tap (a little higher above ground)
	tapWorldPos.z = shadedPtFxMapHiHeight;
	retVal.g = CalcCascadeShadowsFastNoFade(camPos, tapWorldPos, normal, IN.UV) * cloudShadow;

	return retVal;
}

technique shadedparticles_buffergen
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER VS_ShadedPtFxBufferGen(); 
		PixelShader  = compile PIXELSHADER PS_ShadedPtFxBufferGen()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
#endif //USE_SHADED_PTFX_MAP
