#ifndef PRAGMA_DCL
	#pragma dcl position normal diffuse texcoord0 texcoord1 texcoord2 texcoord3 
	#define PRAGMA_DCL
#endif

///////////////////////////////////////////////////////////////////////////////
// PTFX_TRAIL.FX - trail particle shader
///////////////////////////////////////////////////////////////////////////////

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

// blend modes
#define BLEND_MODE_NORMAL_V                 	0
#define BLEND_MODE_ADD_V					   	1

#define BLEND_MODE_NORMAL                 		BM0
#define BLEND_MODE_ADD					   		BM1 

// misc
#define CLAMP_COLOR								(16.0f)
#define PS_DEBUG_OUTPUT							(0 && !defined(SHADER_FINAL))
#define REFRACTION_NO_BLUR						(0 || __LOW_QUALITY)


///////////////////////////////////////////////////////////////////////////////
// SHADER VARIABLES (not exposed to the interface)
///////////////////////////////////////////////////////////////////////////////

#include "../common.fxh"

BeginConstantBufferDX10( ptfx_trail_locals1 )
float gBlendMode : BlendMode;
float4 gChannelMask : ChannelMask;

///////////////////////////////////////////////////////////////////////////////
// GLOBAL SHADER VARIABLES (exposed to the interface)
///////////////////////////////////////////////////////////////////////////////

EndConstantBufferDX10( ptfx_trail_locals1 )

///////////////////////////////////////////////////////////////////////////////
// INCLUDES
///////////////////////////////////////////////////////////////////////////////

#include "ptfx_common.fxh"
#include "../Lighting/lighting.fxh"
#include "../Lighting/forward_lighting.fxh"
#include "../PostFX/SeeThroughFX.fxh"


///////////////////////////////////////////////////////////////////////////////
// SHADER VARIABLES (exposed to the interface)
///////////////////////////////////////////////////////////////////////////////

BeginConstantBufferDX10( ptfx_trail_locals2 )

/*
float gSuperAlpha : SuperAlpha
<
	string UIName	= "SuperAlpha"; 
	float UIMin		= 0.0; 
	float UIMax		= 20.0; 
	float UIStep	= 0.01; 
	string UIHint	= "Keyframeable";
	int nostrip=1;	// dont strip
> = 2.0;
*/
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

float gDirectionalMult : DirectionalMult
<
	string UIName = "DirectionalMult: Controls the amount of directional lighting on surface, color also controls this (0 - no light, 1- light)";
	float UIMin = 0.0;
	float UIMax = 2.0;
	float UIStep = 0.01;
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

#if 0
float gEmissiveIntensity : EmissiveIntensity
<
	string UIName = "EmissiveIntensity: Controls the emissive contribution (0 - no emissive)";
	float UIMin = 0.0;
	float UIMax = 1024.0;
	float UIStep = 0.01;
> = 0.0f;
#endif

EndConstantBufferDX10( ptfx_trail_locals2 )

///////////////////////////////////////////////////////////////////////////////
// SAMPLERS
///////////////////////////////////////////////////////////////////////////////
BeginSampler(sampler,DiffuseTex2,DiffuseTexSampler,DiffuseTex2)
	string UIName = "Texture Map";
	string TCPTemplate = "Diffuse";
	string	TextureType="Diffuse";
ContinueSampler(sampler,DiffuseTex2,DiffuseTexSampler,DiffuseTex2)
    MinFilter = LINEAR;
    MagFilter = LINEAR;
    MipFilter = MIPLINEAR;
    AddressU  = WRAP;
	AddressV  = CLAMP;
EndSampler;

/*
BeginSampler(sampler, FrameMap, FrameMapTexSampler, FrameMap)
ContinueSampler(sampler, FrameMap, FrameMapTexSampler, FrameMap) 
   MinFilter = HQLINEAR;
   MagFilter = LINEAR;
   MipFilter = NONE;
   AddressU  = CLAMP;
   AddressV  = CLAMP;
EndSampler;
*/

///////////////////////////////////////////////////////////////////////////////
// FUNCTIONS
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// ptfxTrailApplyFog
half4 ptfxTrailApplyFog(half4 color, ptfxTrailPSIn psInfo)
{
	half3 fogColor = psInfo.fog.xyz;
	half  fogBlend = psInfo.fog.w;

	color.xyz = lerp(color.xyz, fogColor, fogBlend);

	return color;
}

///////////////////////////////////////////////////////////////////////////////
// ptfxTrailGlobalPostProcess
void ptfxTrailGlobalPostProcess(inout half4 color, ptfxTrailPSIn psInfo, bool bFogColor, bool isWaterRefraction)
{
	if (bFogColor)
	{
#if !__LOW_QUALITY
		if(isWaterRefraction && ptfx_UseWaterPostProcess)
		{
			color = ptfxCommonGlobalWaterRefractionPostProcess_PS(color,psInfo.pos, psInfo.fog.xy);

		}
		else
#endif // !__LOW_QUALITY
		{
			color = ptfxTrailApplyFog(color, psInfo);
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
// ptfxTrailCalculateBasicLighting
float3 ptfxTrailCalculateBasicLighting(float3 worldPos, float3 normal, float4 col)
{
	// initialise structures
	surfaceProperties surface = (surfaceProperties)0;
	directionalLightProperties light = (directionalLightProperties)0;
	materialProperties material = (materialProperties)0;
	StandardLightingProperties stdLightProps = (StandardLightingProperties)0;

	// surface properties
	surface.position = worldPos;
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
	material.diffuseColor = ProcessDiffuseColor(col);
	material.naturalAmbientScale = gAmbientMult*gNaturalAmbientScale;
	material.artificialAmbientScale = gAmbientMult*gArtificialAmbientScale;
#if 0
	material.emissiveIntensity = gEmissiveIntensity;
#endif
	material.inInterior = gInInterior;

	float shadowAmount = 
#if (RSG_PC && __LOW_QUALITY)
		1.0f;
#else
		CalcCascadeShadowsFastNoFade(gViewInverse[3].xyz, worldPos, float3(0,0,1), float2(0,0));
#endif

	float3 directionalLight, restOfTheLight;
	calculateParticleForwardLighting(
		4,
		true,
		surface,
		material,
		light,
		lerp(1.0f, 0.0f, CalcFogShadowDensity(worldPos)) * gDirectionalMult,
		gExtraLightMult,
		directionalLight,
		restOfTheLight);

	return (directionalLight * shadowAmount) + restOfTheLight;
}

#if USE_SOFT_PARTICLES

///////////////////////////////////////////////////////////////////////////////
// ptfxTrailComputeSoftEdge
void ptfxTrailComputeSoftEdge( float currenDepth, float spriteDepth, float distToCenter, float softness, float nearPlaneDistance,  inout float4 col )
{
	float depthDiff = (currenDepth - spriteDepth);
	depthDiff*=depthDiff;

	// apply roundness and darkening to fade ramp 
	// TODO: re-arrange instructions a bit better - keeping it more readable for now
	// offset depth difference to get a round edge
	depthDiff -= (distToCenter)*gSoftnessCurve;
	float softFade = saturate( depthDiff * softness );
	col.w *= softFade;

	// apply faux-shadow offset 
	depthDiff -= gSoftnessShadowOffset;
	softFade = saturate( depthDiff * softness );

	float darkenDiff = 1.0f - softFade;
	softFade += (darkenDiff*(1.0f-gSoftnessShadowMult) );

	col.xyz *= softFade;

	//Apply mirror near Plane fade (gets activated only on mirror reflection phase)
	col.a *= ptfxApplyNearPlaneFade(nearPlaneDistance);
}

#endif // USE_SOFT_PARTICLES


///////////////////////////////////////////////////////////////////////////////
// Geometry SHADERS
///////////////////////////////////////////////////////////////////////////////
#if PTFX_SHADOWS_INSTANCING_GEOMSHADER
struct GS_InstPassThrough_OUT
{
	DECLARE_POSITION(pos)
	float4 color			: TEXCOORD0;
	float2 texcoord			: TEXCOORD1;
	float4 fog				: TEXCOORD2;
#if USE_SOFT_PARTICLES
	float2 soft				: TEXCOORD3;
#endif // USE_SOFT_PARTICLES

	uint InstID			: TEXCOORD4;

#if CASCADE_SHADOW_TEXARRAY
	uint rt_index : SV_RenderTargetArrayIndex;
#else
	uint viewport : SV_ViewportArrayIndex;
#endif
};

[maxvertexcount(3)]
void GS_InstPassThrough(triangle ptfxTrailVSOut input[3], inout TriangleStream<GS_InstPassThrough_OUT> OutputStream)
{
	GS_InstPassThrough_OUT output;

	for(int i = 0; i < 3; i++)
	{
		output.pos = input[i].pos;
		output.color = input[i].color;
		output.texcoord = input[i].texcoord;
		output.fog = input[i].fog;
#if USE_SOFT_PARTICLES
		output.soft = input[i].soft;
#endif
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
#endif

///////////////////////////////////////////////////////////////////////////////
// VERTEX SHADERS
///////////////////////////////////////////////////////////////////////////////

#define PTFX_TRAIL_WIRE_RENDER_HACK (0) // hack to use ptfx trail to render a wire

///////////////////////////////////////////////////////////////////////////////
// ptfxTrailCommonVS
ptfxTrailVSOut ptfxTrailCommonVS(ptfxTrailVSInfo IN, bool isSoft, bool isScreenSpace, bool isShadow,bool useInstancing, bool isWaterRefraction)
{
#if (__LOW_QUALITY)
	isSoft = false;
#endif // __LOW_QUALITY

	ptfxTrailVSOut OUT = (ptfxTrailVSOut)0;

#if PTFX_TRAIL_WIRE_RENDER_HACK

	const float tan_VFOV = 1.6; // not really correct, but used for tuning

	const float3 camPos = +gViewInverse[3].xyz;
	const float3 camDir = -gViewInverse[2].xyz;

	float3 pos    = ptfxTrailVSInfo_pos(IN);
	float3 centre = ptfxTrailVSInfo_centre(IN);
	float  radius = ptfxTrailVSInfo_radius(IN);

	float z = dot(centre - camPos, camDir);
	float w = radius*768/(z*tan_VFOV); // width in pixels
	float h = max(w, 1);
	float a = min(w, 1);

	const float3 wpos = centre + (pos - centre)*h/w;

#else

	const float3 wpos = ptfxTrailVSInfo_pos(IN);

#endif

#if PTFX_SHADOWS_INSTANCING
	int iCBInstIndex = 0;
	if (useInstancing)
	{
		iCBInstIndex = ptfxGetCascadeInstanceIndex(IN.instID);
		OUT.pos      = mul(float4(wpos, 1), gInstWorldViewProj[(int)iCBInstIndex]);
		#if PTFX_SHADOWS_INSTANCING_GEOMSHADER
		OUT.InstID = iCBInstIndex;
		#endif
	}
	else
#endif
		OUT.pos      = mul(float4(wpos, 1), gWorldViewProj);
	
	OUT.color    = ptfxTrailVSInfo_color(IN);
	OUT.texcoord = ptfxTrailVSInfo_texcoord(IN);
	if(isWaterRefraction && ptfx_UseWaterPostProcess)
	{
		float4 wldPos = mul(float4(wpos, 1), gWorld);
		float2 depthBlends = ptfxCommonGlobalWaterRefractionPostProcess_VS(wldPos.xyz);
		OUT.fog = float4(depthBlends.xy,0.0f,0.0f);
	}
	else
	{
		OUT.fog      = CalcFogData(wpos - gViewInverse[3].xyz); // we may want a lighter weight version of this
	}

#if PTFX_TRAIL_WIRE_RENDER_HACK
	OUT.color.xyz = 0;
	OUT.color.w *= a;
#endif // PTFX_TRAIL_WIRE_RENDER_HACK

	OUT.ptfxInfoTrail_depthW = OUT.pos.w; // depth	

#if USE_SOFT_PARTICLES
	if (isSoft)
	{	
		OUT.ptfxInfoTrail_distToCentre = ptfxTrailVSInfo_radius(IN);
	}
#endif // USE_SOFT_PARTICLES

	RAGE_COMPUTE_CLIP_DISTANCES(OUT.pos);
	return OUT;
}


///////////////////////////////////////////////////////////////////////////////
// ptfxTrailLitVS
ptfxTrailVSOut ptfxTrailLitVS(ptfxTrailVSInfo_lit IN, bool isSoft, bool isScreenSpace, bool isShadow,bool useInstancing, bool isWaterRefraction)
{
#if (__LOW_QUALITY)
	isSoft = false;
#endif // __LOW_QUALITY

	ptfxTrailVSOut OUT = (ptfxTrailVSOut)0;

#if PTFX_TRAIL_WIRE_RENDER_HACK

	const float tan_VFOV = 1.6; // not really correct, but used for tuning

	const float3 camPos = +gViewInverse[3].xyz;
	const float3 camDir = -gViewInverse[2].xyz;

	float3 pos    = ptfxTrailVSInfo_lit_pos(IN);
	float3 centre = ptfxTrailVSInfo_lit_centre(IN);
	float  radius = ptfxTrailVSInfo_lit_radius(IN);

	float z = dot(centre - camPos, camDir);
	float w = radius*768/(z*tan_VFOV); // width in pixels
	float h = max(w, 1);
	float a = min(w, 1);

	const float3 wpos = centre + (pos - centre)*h/w;

#else

	const float3 wpos = ptfxTrailVSInfo_lit_pos(IN);

#endif

#if PTFX_SHADOWS_INSTANCING
	int iCBInstIndex = 0;
	if (useInstancing)
	{
		iCBInstIndex = ptfxGetCascadeInstanceIndex(IN.instID);
		OUT.pos      = mul(float4(wpos, 1), gInstWorldViewProj[(int)iCBInstIndex]);
		#if PTFX_SHADOWS_INSTANCING_GEOMSHADER
		OUT.InstID = iCBInstIndex;
		#endif
	}
	else
#endif
		OUT.pos      = mul(float4(wpos, 1), gWorldViewProj);
	
	OUT.color    = ptfxTrailVSInfo_lit_color(IN);
	OUT.texcoord = ptfxTrailVSInfo_lit_texcoord(IN);
	if(!isShadow)
	{
		if(isWaterRefraction && ptfx_UseWaterPostProcess)
		{
			float4 wldPos = mul(float4(wpos, 1), gWorld);
			float2 depthBlends = ptfxCommonGlobalWaterRefractionPostProcess_VS(wldPos.xyz);
			OUT.fog = float4(depthBlends.xy,0.0f,0.0f);
		}
		else
		{
			OUT.fog      = CalcFogData(wpos - gViewInverse[3].xyz); // we may want a lighter weight version of this
		}
	}

#if PTFX_TRAIL_WIRE_RENDER_HACK
	OUT.color.xyz = 0;
	OUT.color.w *= a;
#endif // PTFX_TRAIL_WIRE_RENDER_HACK

	OUT.ptfxInfoTrail_depthW = OUT.pos.w; // depth	

#if USE_SOFT_PARTICLES
	if (isSoft)
	{	
		OUT.ptfxInfoTrail_distToCentre = ptfxTrailVSInfo_lit_radius(IN);
	}
#endif // USE_SOFT_PARTICLES

	const float3 normal = ptfxTrailVSInfo_lit_normal(IN, gViewInverse[2].xyz);
	OUT.color.xyz = ptfxTrailCalculateBasicLighting(wpos, normal, ptfxTrailVSInfo_lit_color(IN));

#define DEBUG_TRAIL_TANGENTS (0)

#if DEBUG_TRAIL_TANGENTS
	if (OUT.pos.x != 99999999)
	{
		OUT.color.xyz = ptfxTrailVSInfo_lit_normal(IN, gViewInverse[2].xyz)*0.5 + 0.5;
	//	OUT.color.xyz = abs(dot(ptfxTrailVSInfo_lit_normal(IN, gViewInverse[2].xyz), gViewInverse[2].xyz));
		OUT.color.w = 1;
	}
#endif // DEBUG_TRAIL_TANGENTS

#if PTFX_SHADOWS_INSTANCING && !PTFX_SHADOWS_INSTANCING_GEOMSHADER
	if (isShadow)
	{
		float posYClipSpace = OUT.pos.y / OUT.pos.w;
		float posYViewport;
		float cascadeHeight = 1.0 / 4.0;
		float currentCascade = 3.0 - iCBInstIndex;
		posYViewport = (posYClipSpace + 1.0) * 0.5;
		posYViewport *= cascadeHeight;
		posYViewport += (cascadeHeight * currentCascade);
		posYViewport = (posYViewport * 2.0) - 1.0f;
		OUT.pos.y = posYViewport * OUT.pos.w;
		OUT.fog.x = iCBInstIndex;
	}
#endif

	RAGE_COMPUTE_CLIP_DISTANCES(OUT.pos);

	return OUT;
}

///////////////////////////////////////////////////////////////////////////////
// PIXEL SHADERS
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// ptfxTrailCommonPS
ptfxOutputStruct ptfxTrailCommonPSImpl(ptfxTrailPSIn IN, bool isLit, bool isSoft, bool isWaterRefraction, int diffuseMode, bool isShadow IMPLEMENT_BLEND_MODES_AS_PASSES_PARAM(int blendmode), bool useInstancing)
{
#if (__LOW_QUALITY)
	isSoft = false;
#endif // __LOW_QUALITY

#if DEBUG_TRAIL_TANGENTS
	if (IN.pos.x != 99999999)
	{
		return PackHdr(IN.color)*2;
	}
#endif // DEBUG_TRAIL_TANGENTS

#if PS_DEBUG_OUTPUT
	float4 texColor = float4(frac(IN.texcoord.xyy*5), 1)*IN.color;
	texColor.xyz *= CLAMP_COLOR;
	//return (half4)IN.color;
#endif
	float2 vPos = IN.pos;

#if PTFX_SHADOWS_INSTANCING && !PTFX_SHADOWS_INSTANCING_GEOMSHADER
	if( useInstancing )
	{
		uint instID = IN.fog.x;
		float yStart = instID * 1024;
		float yEnd = yStart + 1024;
		rageDiscard((vPos.y < yStart || vPos.y > yEnd));
		IN.fog.x = 0.0f;
	}
#endif

	if (gGlobalFogIntensity > 0.0f)
		IN.fog.w *= ptfxGetFogRayIntensity(vPos.xy);

#if 1
	// convert from srgb to linear space
	half4 diffColor1 = ProcessDiffuseColor(h4tex2D(DiffuseTexSampler, IN.texcoord));

	// grey scale packing
	if (diffuseMode != DIFFUSE_MODE_RGBA)
	{
		if (diffuseMode == DIFFUSE_MODE_XXXX) 
		{
			diffColor1.xyzw = dot(diffColor1.xyz, gChannelMask.xyz).xxxx;
		}
		else if (diffuseMode == DIFFUSE_MODE_RGB)
		{
			diffColor1.w = dot(diffColor1.xyz, 1);
		}
	}

	diffColor1.xyz *= CLAMP_COLOR; // why?

	// compute the diffuse color
	half4 texColor = diffColor1;
#endif

	// compute the final color
	half4 color;

#if USE_SOFT_PARTICLES
	if((isSoft) && (!isShadow))
	{
		float sampledDepth = 0;
		ptfxGetDepthValue(vPos.xy, sampledDepth);
		ptfxTrailComputeSoftEdge(sampledDepth, IN.ptfxInfoTrail_depthW, IN.ptfxInfoTrail_distToCentre, 1/*IN.ptfxInfo_softness*/, RAGE_NEAR_CLIP_DISTANCE(IN.pos), IN.color);
	}
#endif // USE_SOFT_PARTICLES


	const half opacity = IN.color.w*(1 - IN.fog.w);
	const half texOpacity = 1;

	// TODO -- fix gBlendMode
	if (1 || IMPLEMENT_BLEND_MODES_AS_PASSES_SWITCH(blendmode, gBlendMode) == BLEND_MODE_NORMAL_V)								// transparent when texColor = {-,-,-,0}
	{
		color.xyz = texColor.xyz*IN.color.xyz;
//			color.w   = opacity*texColor.w;
		color.w   = IN.color.w*texColor.w;									// don't fog opacity .. doesn't fog at the same rate
	}
	else if (IMPLEMENT_BLEND_MODES_AS_PASSES_SWITCH(blendmode, gBlendMode) == BLEND_MODE_ADD_V)									// transparent when texColor = {0,0,0,-} (texture alpha not used)
	{
		color.xyz = opacity*texOpacity*texColor.xyz*IN.color.xyz;
		color.w   = 1;
	}
	else
	{
		color = 0;
	}

#if __XENON || __SHADERMODEL >= 40
	color.w = saturate(color.w) *gInvColorExpBias;
#endif

	if (1 || IMPLEMENT_BLEND_MODES_AS_PASSES_SWITCH(blendmode, gBlendMode) == BLEND_MODE_NORMAL_V) // TODO -- fix gBlendMode
	{
		ptfxTrailGlobalPostProcess(color, IN, true, isWaterRefraction);
	}
	else if (IMPLEMENT_BLEND_MODES_AS_PASSES_SWITCH(blendmode, gBlendMode) == BLEND_MODE_ADD_V)
	{
		ptfxTrailGlobalPostProcess(color, IN, false, isWaterRefraction);

		// if rendering to a downsampled buffer that will be later composited onto the fullscreen surface,
		// we need to ouput the luminance as the alpha to allow for transparent pixels when blending additively
		half luminance = dot(color.xyz, float3(0.299, 0.587, 0.114));
		color.w = luminance;
	}



	half4 result;
	// pack 
	if(isShadow)
	{
		result = color.aaaa;
	}
	else
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

	ptfxOutputStruct outStruct;
	outStruct.col0 = result;
#if PTFX_APPLY_DOF_TO_PARTICLES
	outStruct.dof.depth = IN.ptfxInfoTrail_depthW;
	outStruct.dof.alpha = result.a * gGlobalParticleDofAlphaScale;
#endif //PTFX_APPLY_DOF_TO_PARTICLES

	return outStruct;
}

ptfxOutputStruct ptfxTrailCommonPS(ptfxTrailPSIn IN, bool isLit, bool isSoft, bool isWaterRefraction, int diffuseMode, bool isShadow IMPLEMENT_BLEND_MODES_AS_PASSES_PARAM(int blendmode), bool useInstancing)
{
	return ptfxTrailCommonPSImpl(IN, isLit, isSoft, isWaterRefraction, diffuseMode, isShadow IMPLEMENT_BLEND_MODES_AS_PASSES_PARAM(blendmode), useInstancing);
}

half4 ptfxTrailCommonShadowPS(ptfxTrailPSIn IN, bool isLit, bool isSoft, bool isWaterRefraction, int diffuseMode, bool isShadow IMPLEMENT_BLEND_MODES_AS_PASSES_PARAM(int blendmode), bool useInstancing)
{
	ptfxOutputStruct OUT = ptfxTrailCommonPSImpl(IN, isLit, isSoft, isWaterRefraction, diffuseMode, true IMPLEMENT_BLEND_MODES_AS_PASSES_PARAM(blendmode), useInstancing);
	return OUT.col0;
}

///////////////////////////////////////////////////////////////////////////////
// ptfxTrailRefractPS
half4 ptfxTrailRefractPS(ptfxTrailPSIn IN, bool isLit, bool isSoft, bool isShadow IMPLEMENT_BLEND_MODES_AS_PASSES_PARAM(int blendmode))
{
#if 1 || PS_DEBUG_OUTPUT
	return (half4)IN.color;
#endif
	float vPos = IN.pos;

	/*
	// compute the diffuse color
	float3 xyNormal_zAlpha = ((tex2D(DiffuseTexSampler, IN.normUV.xy) * 2.0f) - 1.0f).xyz;	 
	xyNormal_zAlpha.xy *= gNormalMapMult.xx*100.0f.xx;

	float4 perturbedUvs = vPos.xyxy + xyNormal_zAlpha.xyxy;	
	perturbedUvs.xy *= gInvScreenSize.xy;

	float4 refractedCol = UnpackColor( ptfxTrailSmallBlur( perturbedUvs.xy ) );
	refractedCol.w = xyNormal_zAlpha.z;

	// depth sample will be reused in ptfxTrailComputeSoftEdge 
	float sampledDepth;
	ptfxTrailGetDepthValue( perturbedUvs.zw, sampledDepth );

	// kill alpha if an object is in front of the particle 
	float alphaMultiplier = saturate((sampledDepth - IN.ptfxInfo_depth)*1000.0f);
	refractedCol.w *= alphaMultiplier;

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// compute the final color
	float4 finalColor;

#if USE_SOFT_PARTICLES
	if (isSoft)
	{
		ptfxTrailComputeSoftEdge( sampledDepth, IN.ptfxInfo_depth, IN.distToCenter, IN.ptfxInfo_softness, refractedCol );
	}
#endif // USE_SOFT_PARTICLES

	refractedCol.xyzw *= IN.color.xyzw;
	finalColor = float4(refractedCol.xyz, saturate(refractedCol.w) XENON_ONLY(*gInvColorExpBias));

	// post process
	ptfxTrailPostProcessPSUnlit(finalColor, IN, false, true);

	// clamp
	finalColor = clamp(finalColor, 0.0f, CLAMP_COLOR);

	// pack 
	return PackColor(finalColor);
	*/
}


half4 PS_none(ptfxTrailPSIn IN) : COLOR
{
	return float4(1.0f, 0.0f, 1.0f, 1.0f); 
}


half4 PS_none_shadow(ptfxTrailPSIn IN) : COLOR
{
	return float4(1.0f, 1.0f, 1.0f, 1.0f); 
}


///////////////////////////////////////////////////////////////////////////////
// TECHNIQUES
///////////////////////////////////////////////////////////////////////////////

#if IMPLEMENT_BLEND_MODES_AS_PASSES

#if PTFX_SHADOWS_INSTANCING_GEOMSHADER
#define DEF_ptfxTrailCommonVS(name, isSoft, isScreenSpace, isWaterRefraction) \
ptfxTrailVSOut JOIN(VS_, name) (ptfxTrailVSInfo IN) { return ptfxTrailCommonVS(IN, isSoft, isScreenSpace, false,false, isWaterRefraction); } \
USE_PARTICLE_SHADOWS_ONLY(ptfxTrailVSOut JOIN3(VS_, name, _shadow) (ptfxTrailVSInfo IN) { return ptfxTrailCommonVS(IN, isSoft, isScreenSpace, true, false, isWaterRefraction); }) \
USE_PARTICLE_SHADOWS_ONLY(ptfxTrailVSOut JOIN3(VS_, name, _shadow_instanced) (ptfxTrailVSInfo IN) { return ptfxTrailCommonVS(IN, isSoft, isScreenSpace, true, true, isWaterRefraction); }) \
DEF_TERMINATOR
#else
#define DEF_ptfxTrailCommonVS(name, isSoft, isScreenSpace, isWaterRefraction) \
ptfxTrailVSOut JOIN(VS_, name) (ptfxTrailVSInfo IN) { return ptfxTrailCommonVS(IN, isSoft, isScreenSpace, false,false, isWaterRefraction); } \
USE_PARTICLE_SHADOWS_ONLY(ptfxTrailVSOut JOIN3(VS_, name, _shadow) (ptfxTrailVSInfo IN) { return ptfxTrailCommonVS(IN, isSoft, isScreenSpace, true, false, isWaterRefraction); }) \
DEF_TERMINATOR
#endif

#if PTFX_SHADOWS_INSTANCING_GEOMSHADER
#define DEF_ptfxTrailLitVS(name, isSoft, isScreenSpace, isWaterRefraction) \
ptfxTrailVSOut JOIN(VS_, name) (ptfxTrailVSInfo_lit IN) { return ptfxTrailLitVS(IN, isSoft, isScreenSpace, false,false, isWaterRefraction); } \
USE_PARTICLE_SHADOWS_ONLY(ptfxTrailVSOut JOIN3(VS_, name, _shadow) (ptfxTrailVSInfo_lit IN) { return ptfxTrailLitVS(IN, isSoft, isScreenSpace, true,false, isWaterRefraction); }) \
USE_PARTICLE_SHADOWS_ONLY(ptfxTrailVSOut JOIN3(VS_, name, _shadow_instanced) (ptfxTrailVSInfo_lit IN) { return ptfxTrailLitVS(IN, isSoft, isScreenSpace, true,true, isWaterRefraction); }) \
DEF_TERMINATOR
#else
#define DEF_ptfxTrailLitVS(name, isSoft, isScreenSpace, isWaterRefraction) \
ptfxTrailVSOut JOIN(VS_, name) (ptfxTrailVSInfo_lit IN) { return ptfxTrailLitVS(IN, isSoft, isScreenSpace, false,false, isWaterRefraction); } \
USE_PARTICLE_SHADOWS_ONLY(ptfxTrailVSOut JOIN3(VS_, name, _shadow) (ptfxTrailVSInfo_lit IN) { return ptfxTrailLitVS(IN, isSoft, isScreenSpace, true,true, isWaterRefraction); }) \
DEF_TERMINATOR
#endif

DEF_ptfxTrailCommonVS(none,					false, false, false);
DEF_ptfxTrailCommonVS(soft,					true , false, false);
DEF_ptfxTrailCommonVS(soft_waterrefract,	true , false, true);
DEF_ptfxTrailCommonVS(screen,				false, true , false);
DEF_ptfxTrailLitVS(lit,						false, false, false);
DEF_ptfxTrailLitVS(lit_soft,				true , false, false);
DEF_ptfxTrailCommonVS(lit_screen,			false, true, false );
DEF_ptfxTrailLitVS(lit_soft_waterrefract,	true , false, true);


#define DEF_ptfxTrailCommonPS_BlendMode(name, isLit, isSoft, isWaterRefraction, diffuseMode, blendmode, blendmodeasvalue) \
ptfxOutputStruct JOIN3(PS_, name, blendmode) (ptfxTrailPSIn IN) { return ptfxTrailCommonPS (IN, isLit, isSoft, isWaterRefraction, diffuseMode, false, blendmodeasvalue, false); } \
USE_PARTICLE_SHADOWS_ONLY(half4 JOIN4(PS_, name, blendmode, _shadow) (ptfxTrailPSIn IN) : COLOR { return ptfxTrailCommonShadowPS (IN, isLit, isSoft, isWaterRefraction, diffuseMode, true, blendmodeasvalue, true); }) \
DEF_TERMINATOR

#define DEF_ptfxTrailRefractPS_BlendMode(name, isLit, isSoft, blendmode, blendmodeasvalue) \
half4 JOIN3(PS_, name, blendmode) (ptfxTrailPSIn IN) : COLOR { return ptfxTrailRefractPS (IN, isLit, isSoft, false, blendmodeasvalue); } \
USE_PARTICLE_SHADOWS_ONLY(half4 JOIN4(PS_, name, blendmode, _shadow) (ptfxTrailPSIn IN) : COLOR { return ptfxTrailRefractPS (IN, isLit, isSoft, true, blendmodeasvalue); }) \
DEF_TERMINATOR

#define DEF_ptfxTrailCommonPS(name, isLit, isSoft, isWaterRefraction, diffuseMode) \
DEF_ptfxTrailCommonPS_BlendMode(name, isLit, isSoft, isWaterRefraction, diffuseMode, BLEND_MODE_NORMAL, BLEND_MODE_NORMAL_V) \
DEF_ptfxTrailCommonPS_BlendMode(name, isLit, isSoft, isWaterRefraction, diffuseMode, BLEND_MODE_ADD, BLEND_MODE_ADD_V) \
DEF_TERMINATOR													  

#define DEF_ptfxTrailRefractPS(name, isLit, isSoft) \
DEF_ptfxTrailRefractPS_BlendMode(name, isLit, isSoft, BLEND_MODE_NORMAL, BLEND_MODE_NORMAL_V) \
DEF_ptfxTrailRefractPS_BlendMode(name, isLit, isSoft, BLEND_MODE_ADD, BLEND_MODE_ADD_V) \
DEF_TERMINATOR													   

DEF_ptfxTrailCommonPS(RGBA,						false, false, false, DIFFUSE_MODE_RGBA)
DEF_ptfxTrailCommonPS(RGBA_soft,				false, true , false, DIFFUSE_MODE_RGBA)
DEF_ptfxTrailCommonPS(RGBA_soft_waterrefract,		false, true , true , DIFFUSE_MODE_RGBA)
DEF_ptfxTrailCommonPS(RGBA_lit,					true , false, false, DIFFUSE_MODE_RGBA)
DEF_ptfxTrailCommonPS(RGBA_lit_soft,			true , true , false, DIFFUSE_MODE_RGBA)
DEF_ptfxTrailCommonPS(RGBA_lit_soft_waterrefract,	true , true , true , DIFFUSE_MODE_RGBA)
DEF_ptfxTrailCommonPS(XXXX,						false, false, false, DIFFUSE_MODE_XXXX)
DEF_ptfxTrailCommonPS(XXXX_soft,				false, true , false, DIFFUSE_MODE_XXXX)
DEF_ptfxTrailCommonPS(XXXX_lit,					true , false, false, DIFFUSE_MODE_XXXX)
DEF_ptfxTrailCommonPS(XXXX_lit_soft,			true , true , false, DIFFUSE_MODE_XXXX)
DEF_ptfxTrailCommonPS(RGB,						false, false, false, DIFFUSE_MODE_RGB )
DEF_ptfxTrailCommonPS(RGB_soft,					false, true , false, DIFFUSE_MODE_RGB )
DEF_ptfxTrailCommonPS(RGB_soft_waterrefract,		false, true , true , DIFFUSE_MODE_RGB )
DEF_ptfxTrailCommonPS(RGB_lit,					true , false, false, DIFFUSE_MODE_RGB )
DEF_ptfxTrailCommonPS(RGB_lit_soft,				true , true , false, DIFFUSE_MODE_RGB )
DEF_ptfxTrailCommonPS(RGB_lit_soft_waterrefract,	true , true , true , DIFFUSE_MODE_RGB)
DEF_ptfxTrailRefractPS(RGB_refract,			false, false)
DEF_ptfxTrailRefractPS(RGB_soft_refract,	false, true)

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
	PixelShader  = compile PIXELSHADER  ps() CGC_FLAGS(CGC_DEFAULTFLAGS); \
} \
USE_PARTICLE_SHADOWS_ONLY(DEF_ShadowPass(blendmode, vs, ps)) \
USE_PARTICLE_SHADOWS_ONLY(DEF_InstShadowPass(blendmode, vs, ps)) \
DEF_TERMINATOR


#define DEF_BlendModePassOFF(blendmode, vs, ps) \
pass blendmode \
{ \
	VertexShader = compile VERTEXSHADER VS_none(); \
	PixelShader  = compile PIXELSHADER  PS_none() CGC_FLAGS(CGC_DEFAULTFLAGS); \
} \
USE_PARTICLE_SHADOWS_ONLY(DEF_ShadowPass(blendmode, VS_none, PS_none)) \
USE_PARTICLE_SHADOWS_ONLY(DEF_InstShadowPass(blendmode, VS_none, PS_none)) \
DEF_TERMINATOR


#define DEF_ptfxTrailTechnique(name, vs, ps, normal, add) \
technique name  \
{  \
	JOIN(DEF_BlendModePass, normal)(BLEND_MODE_NORMAL, vs, JOIN(ps, BLEND_MODE_NORMAL)) \
	JOIN(DEF_BlendModePass, add)(BLEND_MODE_ADD, vs, JOIN(ps, BLEND_MODE_ADD)) \
} \
DEF_TERMINATOR

#include "ptfx_trail.fxh"

#else //IMPLEMENT_BLEND_MODES_AS_PASSES

#endif //IMPLEMENT_BLEND_MODES_AS_PASSES
