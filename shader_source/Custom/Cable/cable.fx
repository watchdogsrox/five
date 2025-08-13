// ========
// cable.fx
// ========
#ifndef PRAGMA_DCL
	#pragma dcl position normal diffuse texcoord0
	#define PRAGMA_DCL
#endif

#define ALPHA_SHADER // forward rendered
// apply lighting
#define CABLE_LIT		1
// soften cable edges
#define CABLE_AA		1
// apply fog
#define CABLE_FOG		1
// align to view vector, not the camera direction
#define CABLE_VIEW_VECTOR	1


// Configure PN triangle code.

// LDS DMC TEMP:-
#define RAGE_USE_PN_TRIANGLES ((1 && (RSG_PC && __SHADERMODEL >= 40)) || (1 && RSG_DURANGO) || (1 && RSG_ORBIS))
#define PN_TRIANGLE_SCREEN_SPACE_ERROR_TESSELLATION_FACTOR	1
#define PN_TRIANGLES_USE_EDGES_AND_MIDPOINT					1
#define PN_TRIANGLES_USE_WORLD_SPACE						1
#define PN_TRIANGLES_USE_DEPTH_LENGTH						1

#if RAGE_USE_PN_TRIANGLES
#define RAGE_USE_PN_TRIANGLES_ONLY(x) x
#else
#define RAGE_USE_PN_TRIANGLES_ONLY(x)
#endif

#include "../../common.fxh"

#if CABLE_LIT
	#define SHADOW_CASTING            (1 && !__MAX && !__LOW_QUALITY && RSG_PC)
	#define SHADOW_CASTING_TECHNIQUES (1 && !__MAX)
	#define SHADOW_RECEIVING          (1)
	#define SHADOW_RECEIVING_VS       (0)
	//#include "../Lighting/Shadows/localshadowglobals.fxh"
	#include "../../Lighting/Shadows/cascadeshadows.fxh"
	#include "../../Lighting/lighting.fxh"
	#include "../../Lighting/forward_lighting.fxh"
#endif // CABLE_LIT
#include "../../Debug/EntitySelect.fxh"

struct cableVertexInput
{
	float3 pos : POSITION;
	float3 tan : NORMAL; // tangent
	float4 col : COLOR0; // xy=phase, z=texcoord.x, w=diffuse2 amount
	float2 r_d : TEXCOORD0;
};

struct cableVertexIntermediate
{
	float4 col;
	float4 amb_tex;
	float3 n;
	float3 b;
	float4 worldPos;
#if CABLE_AA
	float2 offset;	//x = relative offset of the vertex from the core, in pixels along the normal, y = max offset
#endif
};

#if RAGE_USE_PN_TRIANGLES

struct cableVertexCtrlPoint
{
	float4 worldPos : CTRL_POS;
	float4 col      : CTRL_COLOR0;
	float4 amb_tex  : CTRL_TEXCOORD0;
	float3 n        : CTRL_TEXCOORD1; // normal
	float3 b        : CTRL_TEXCOORD2; // binormal
#if CABLE_AA
	float2 offset	: CTRL_PIXEL_OFFSET; // see cableVertexIntermediate.offset
#endif
};

#endif // RAGE_USE_PN_TRIANGLES

struct cableVertexOutput
{
	float4 col      : TEXCOORD0;
	float4 amb_tex  : TEXCOORD1; // xy=ambient, zw=texcoord
	float3 n        : TEXCOORD2; // normal
	float3 b        : TEXCOORD3; // binormal
	float4 worldPos : TEXCOORD4; // used for lit cables and debug overlay (w=emissive)
#if CABLE_AA
	float2 offset	: TEXCOORD5; // see cableVertexIntermediate.offset
#endif
	DECLARE_POSITION_CLIPPLANES(pos)
};

// ================================================================================================

#if __MAX
BeginSampler(sampler2D, textureCable, textureSamp, textureCable)
#else
BeginSampler(sampler2D, texture, textureSamp, texture)
#endif
	string UIName      = "Texture";
	string TCPTemplate = "CableMap";
	string TextureType = "Cable";
#if __MAX
ContinueSampler(sampler2D, textureCable, textureSamp, textureCable)
#else
ContinueSampler(sampler2D, texture, textureSamp, texture)
#endif
	AddressU  = CLAMP;
	AddressV  = BORDER; // border colour should be 0,0,0,0
	MIPFILTER = LINEAR;
	MINFILTER = LINEAR;
	MAGFILTER = LINEAR;
EndSampler;

BEGIN_RAGE_CONSTANT_BUFFER(cable_locals, b4)

float shader_radiusScale : RadiusScale
<
	string UIWidget = "slider";
	float  UIMin    =  0.000;
	float  UIMax    = 10.000;
	float  UIStep   =  0.001;
	string UIName   = "Radius Scale";
> = 1.0;

float shader_fadeExponent : FadeExponent
<
	string UIWidget = "slider";
	float  UIMin    =  0.000;
	float  UIMax    = 10.000;
	float  UIStep   =  0.001;
	string UIName   = "Fade Exponent";
> = 1.0;

float shader_windAmount : WindAmount
<
	string UIWidget = "slider";
	float  UIMin    =  0.000;
	float  UIMax    = 10.000;
	float  UIStep   =  0.001;
	string UIName   = "Wind Amount";
> = 1.0;

float3 shader_cableDiffuse : CableDiffuse
<
	string UIWidget = "slider";
	float  UIMin    = 0.000;
	float  UIMax    = 1.000;
	float  UIStep   = 0.010;
	string UIName   = "Diffuse";
> = {0,0,0};

float3 shader_cableDiffuse2 : CableDiffuse2
<
	string UIWidget = "slider";
	float  UIMin    = 0.000;
	float  UIMax    = 1.000;
	float  UIStep   = 0.010;
	string UIName   = "Diffuse2";
> = {0,0,0};

float2 shader_cableAmbient : CableAmbient // x=natural, y=artificial
<
	string UIWidget = "slider";
	float  UIMin    = 0.000;
	float  UIMax    = 1.000;
	float  UIStep   = 0.010;
	string UIName   = "Ambient";
> = {0,0};

float shader_cableEmissive : CableEmissive
<
	string UIWidget = "slider";
	float  UIMin    = 0.000;
	float  UIMax    = 32.000;
	float  UIStep   = 0.010;
	string UIName   = "Emissive";
> = {0.0};

ROW_MAJOR float4x4 gViewProj : ViewProjection;
float4 gCableParams[5] : CableParams;

float alphaTestValue : AlphaTestValue;

EndConstantBufferDX10(cable_locals)

cableVertexIntermediate ComputeVertex(cableVertexInput IN, out float3 worldTan, bool bShadowPass)
{
	cableVertexIntermediate OUT;

	const float  params_worldToPixels = gCableParams[0].x;
	const float  params_radiusScale   = gCableParams[0].y*shader_radiusScale;
	const float  params_fadeExponent  = gCableParams[0].z*shader_fadeExponent;
	const float  params_cableOpacity  = gCableParams[0].w;
#if !defined(SHADER_FINAL)
	const float4 params_cableDiffuse  = gCableParams[3];
	const float4 params_cableAmbient  = gCableParams[4];
#endif // !defined(SHADER_FINAL)
#if SHADOW_CASTING_TECHNIQUES
	const float params_shadowThickness	  = gCableParams[2].z;
	const float params_shadowWindModifier = gCableParams[2].w;
#endif // SHADOW_CASTING_TECHNIQUES

	float3 worldPos = mul(float4(IN.pos, 1), (float4x3)gWorld);
	worldTan = mul(IN.tan, (float3x3)gWorld);

	const float r = abs(IN.r_d.x)*params_radiusScale;
	const float s = (IN.r_d.x > 0) ? 1 : -1;
	const float d = IN.r_d.y;

	if (1)//bApplyMicromovements)
	{
		const float2 params_cableWindMotionFreqTime = gCableParams[1].xy;
		const float2 params_cableWindMotionAmp      = gCableParams[1].zw*shader_windAmount;

		const float2 windPhase = IN.col.xy;
		const float2 wind      = params_cableWindMotionFreqTime + windPhase;
		float2 windSkew  = sin(sin(wind*2.0*PI)*params_cableWindMotionAmp); // do we need both sin's here?

#if SHADOW_CASTING_TECHNIQUES
		if (bShadowPass)
		{
			windSkew *= params_shadowWindModifier;
		}
#endif // SHADOW_CASTING_TECHNIQUES


		worldPos.xy += d*windSkew;
		worldPos.z  += d*dot(windSkew, windSkew);
	}

	const float3 camPos = +gViewInverse[3].xyz;
	const float3 camDir = -gViewInverse[2].xyz;

	const float z = max(0.01, dot(worldPos - camPos, camDir));

	float worldPixels = params_worldToPixels/z;
	float3 diffuse = (float3)0;
	float3 ambient = (float3)0;
	float opacity = 0.0;

#if CABLE_VIEW_VECTOR
	const float3 n = normalize(cross(camPos - worldPos, worldTan));
#else
	const float3 n = normalize(cross(-camDir, worldTan));
#endif
	
	float pixelOffset = 0;

#if SHADOW_CASTING_TECHNIQUES
	if (bShadowPass)
	{
		pixelOffset = min(r, 0.02f) * worldPixels; // size in pixels
#if CABLE_AA
		pixelOffset += saturate(pixelOffset) * 0.5;
#endif

		float worldOffset = pixelOffset/max(worldPixels, 1e-8f);
		worldOffset *= 0.9f;	
		worldPos += worldOffset*s*n;
	}
	else
#endif // SHADOW_CASTING_TECHNIQUES
	{
		pixelOffset = r*worldPixels; // size in pixels
		float a = min(1, pixelOffset);
#if CABLE_AA
		pixelOffset += saturate(pixelOffset) * 0.5;
#endif

		float worldOffset = pixelOffset/max(worldPixels, 1e-8f);
		worldPos += worldOffset*s*n;

		float foggedAlpha = 1.0 - CalcFogData(worldPos - camPos).w;
		float thicknessAlpha = pow(a*a, max(1e-8f, params_fadeExponent));

#if __MAX
		const float fade = 1.0f;
#else
		const float fade = thicknessAlpha * foggedAlpha;
#endif
		opacity = params_cableOpacity * fade;

		diffuse = lerp(shader_cableDiffuse, shader_cableDiffuse2, IN.col.w);
		ambient = float3(shader_cableAmbient, shader_cableEmissive);

#if !defined(SHADER_FINAL)
		// apply debug params
		diffuse = lerp(diffuse, params_cableDiffuse.xyz, params_cableDiffuse.w);
		ambient = lerp(ambient, params_cableAmbient.xyz, params_cableAmbient.w);
#endif // !defined(SHADER_FINAL)
	}

	OUT.col.xyz      = diffuse;
	OUT.col.w        = opacity;
	OUT.amb_tex.xy   = ambient.xy;
	OUT.amb_tex.zw   = float2(IN.col.x, (s + 1)/2);
	OUT.n            = n;
	OUT.b            = cross(n, worldTan);
	OUT.worldPos.xyz = worldPos;
	OUT.worldPos.w   = ambient.z; // emissive
#if CABLE_AA
	OUT.offset = float2(pixelOffset*s, pixelOffset);
#endif // CABLE_AA

	return OUT;
}

cableVertexOutput VS_Cable(cableVertexInput IN)
{
	cableVertexOutput OUT;

	float3 worldTan;
	cableVertexIntermediate V = ComputeVertex(IN, worldTan, false);

	OUT.pos      = mul(float4(V.worldPos.xyz, 1), gViewProj);
	OUT.col      = V.col;
	OUT.amb_tex  = V.amb_tex;
	OUT.n        = V.n;
	OUT.b        = V.b;
	OUT.worldPos = V.worldPos;
#if CABLE_AA
	OUT.offset	 = V.offset;
#endif

	RAGE_COMPUTE_CLIP_DISTANCES(OUT.pos);

	return OUT;
}

#if SHADOW_CASTING_TECHNIQUES
cableVertexOutput VS_Cable_Cascade(cableVertexInput IN)
{
	cableVertexOutput OUT = (cableVertexOutput)0;

	float3 worldTan;
	cableVertexIntermediate V = ComputeVertex(IN, worldTan, true);

	OUT.pos      = mul(float4(V.worldPos.xyz, 1), gViewProj);
//	OUT.col      = V.col;
//	OUT.amb_tex  = V.amb_tex;
//	OUT.n        = V.n;
//	OUT.b        = V.b;
	OUT.worldPos = V.worldPos;

	RAGE_COMPUTE_CLIP_DISTANCES(OUT.pos);

	return OUT;
}
#endif // SHADOW_CASTING_TECHNIQUES

#if RAGE_USE_PN_TRIANGLES
#define CABLE_MAX_TESS_FACTOR			8.0
#define CABLE_TESSELLATION_MULTIPLIER	1.0

cableVertexCtrlPoint VS_PNTri(cableVertexInput IN)
{
	cableVertexCtrlPoint OUT;

	float3 worldTan;
	cableVertexIntermediate V = ComputeVertex(IN, worldTan, false);

	OUT.col      = V.col;
	OUT.amb_tex  = V.amb_tex;
	OUT.n        = V.n;
	OUT.b        = V.b;
	OUT.worldPos = V.worldPos;
#if CABLE_AA
	OUT.offset	 = V.offset;
#endif

	return OUT;
}

#if SHADOW_CASTING_TECHNIQUES
cableVertexCtrlPoint VS_PNTri_Cascade(cableVertexInput IN)
{
	cableVertexCtrlPoint OUT;

	float3 worldTan;
	cableVertexIntermediate V = ComputeVertex(IN, worldTan, true);

	OUT.col      = V.col;
	OUT.amb_tex  = V.amb_tex;
	OUT.n        = V.n;
	OUT.b        = V.b;
	OUT.worldPos = V.worldPos;
#if CABLE_AA
	OUT.offset	 = V.offset;
#endif

	return OUT;
}
#endif // SHADOW_CASTING_TECHNIQUES

// Patch Constant Function.
rage_PNTri_PatchInfo HS_PNTri_PF(InputPatch<cableVertexCtrlPoint, 3> PatchPoints,  uint PatchID : SV_PrimitiveID)
{	
	rage_PNTri_PatchInfo Output;
	rage_PNTri_Vertex Points[3];

	Points[0].Position = PatchPoints[0].worldPos.xyz;
	Points[1].Position = PatchPoints[1].worldPos.xyz;
	Points[2].Position = PatchPoints[2].worldPos.xyz;
	
	Points[0].Normal = PatchPoints[0].n;
	Points[1].Normal = PatchPoints[1].n;
	Points[2].Normal = PatchPoints[2].n;

	rage_ComputePNTrianglePatchInfo( Points[0], Points[1], Points[2], Output, CABLE_TESSELLATION_MULTIPLIER );

	return Output;
}

// Hull shader.
[domain("tri")]
[partitioning("fractional_odd")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(3)]
[patchconstantfunc("HS_PNTri_PF")]
[maxtessfactor(CABLE_MAX_TESS_FACTOR)]
cableVertexCtrlPoint HS_PNTri(InputPatch<cableVertexCtrlPoint, 3> PatchPoints, uint i : SV_OutputControlPointID, uint PatchID : SV_PrimitiveID)
{
	cableVertexCtrlPoint Output;

	// Our control points match the vertex shader output.
	Output = PatchPoints[i];

	return Output;
}

cableVertexOutput PNTri_EvaluateAt(rage_PNTri_PatchInfo PatchInfo, float3 WUV, const OutputPatch<cableVertexCtrlPoint, 3> PatchPoints)
{
	cableVertexOutput OUT;

	float3 worldPos = rage_EvaluatePatchAt(PatchInfo, WUV);

	OUT.pos          = mul(float4(worldPos, 1), gViewProj);
	OUT.col          = RAGE_COMPUTE_BARYCENTRIC(WUV, PatchPoints, col);
	OUT.amb_tex      = RAGE_COMPUTE_BARYCENTRIC(WUV, PatchPoints, amb_tex);
	OUT.n            = normalize(RAGE_COMPUTE_BARYCENTRIC(WUV, PatchPoints, n));
	OUT.b            = normalize(RAGE_COMPUTE_BARYCENTRIC(WUV, PatchPoints, b));
	OUT.worldPos.xyz = worldPos.xyz;
	OUT.worldPos.w   = RAGE_COMPUTE_BARYCENTRIC(WUV, PatchPoints, worldPos.w);
#if CABLE_AA
	OUT.offset		 = RAGE_COMPUTE_BARYCENTRIC(WUV, PatchPoints, offset);
#endif

	return OUT;
}

// Domain shader.
[domain("tri")]
cableVertexOutput DS_PNTri(rage_PNTri_PatchInfo PatchInfo, float3 WUV : SV_DomainLocation, const OutputPatch<cableVertexCtrlPoint, 3> PatchPoints)
{
	cableVertexOutput OUT;
	
	OUT = PNTri_EvaluateAt(PatchInfo, WUV, PatchPoints);
	RAGE_COMPUTE_CLIP_DISTANCES(OUT.pos);

	return OUT;
}

#endif // RAGE_USE_PN_TRIANGLES


float4 PS_Cable(cableVertexOutput IN) : COLOR
{
	float4 OUT;

	OUT.xyz = IN.col.xyz;
	// Texture just used for shadow pass
//	OUT.w = IN.col.w*tex2D(textureSamp, IN.amb_tex.zw).x*globalAlpha*gInvColorExpBias;
	OUT.w = IN.col.w*globalAlpha*gInvColorExpBias;

#if CABLE_AA
	// only apply AA on thick wires
	float contribution = saturate( IN.offset.y/1.5 );	//taking the added 0.5 thickness into account
	float aa = saturate(IN.offset.y - abs(IN.offset.x));
	OUT.w *= lerp(1.0, aa, contribution);
#endif //CABLE_AA

	rageDiscard(OUT.w < alphaTestValue);

#if CABLE_LIT

	const float y = saturate(IN.amb_tex.w*2 - 1); // used to construct world normal

	SurfaceProperties surfaceInfo = (SurfaceProperties)0;

	surfaceInfo.surface_baseColor       = float4(IN.amb_tex.xy, 0, 1);
	surfaceInfo.surface_diffuseColor    = float4(OUT.xyz, 1);
	surfaceInfo.surface_worldNormal.xyz = (dot(IN.n,IN.n)*dot(IN.b,IN.b)>1e-8 ?
		normalize(normalize(IN.n)*y + normalize(IN.b)*sqrt(1 - y*y)) : float3(0,0,1));

	StandardLightingProperties surfaceStandardLightingProperties =
		DeriveLightingPropertiesForCommonSurfaceNoTint( surfaceInfo );

	surfaceStandardLightingProperties.diffuseColor = ProcessDiffuseColor(surfaceStandardLightingProperties.diffuseColor);

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

	OUT.xyz = calculateForwardLighting_Cable(
		surface,
		material,
		light,
#if !defined(SHADER_FINAL)
		gCableParams[2].x > 0, // interior ambient
		gCableParams[2].y > 0, // exterior ambient
#else
		true,
		true,
#endif
		SHADOW_RECEIVING).xyz;

	OUT.xyz += IN.col.xyz*IN.worldPos.w; // emissive

#endif // CABLE_LIT

#if CABLE_FOG
	// cheaper to recompute the fog here than to pass all through the tessellation stages
	const float3 camPos = gViewInverse[3].xyz;
	float4 fogData = CalcFogData(IN.worldPos.xyz - camPos);
	OUT.xyz = lerp(OUT.xyz, fogData.xyz, fogData.w);
#endif // CABLE_FOG

	return PackHdr(OUT);
}

#if SHADOW_CASTING_TECHNIQUES
float4 PS_CableShadow(cableVertexOutput IN) : COLOR
{
	return tex2D(textureSamp, IN.amb_tex.zw);
}
#endif // SHADOW_CASTING_TECHNIQUES

#if DEFERRED_TECHNIQUES
DeferredGBuffer PS_Cable_Deferred(cableVertexOutput IN)
{
	const float y = saturate(IN.amb_tex.w*2 - 1); // used to construct world normal
	float alpha = IN.col.w*globalAlpha*gInvColorExpBias;

	SurfaceProperties surfaceInfo = (SurfaceProperties)0;

	surfaceInfo.surface_baseColor       = float4(IN.amb_tex.xy, 0, 1);
	surfaceInfo.surface_diffuseColor    = float4(IN.col.xyz, alpha);
	surfaceInfo.surface_worldNormal.xyz = (dot(IN.n,IN.n)*dot(IN.b,IN.b)>1e-8 ?
		normalize(normalize(IN.n)*y + normalize(IN.b)*sqrt(1 - y*y)) : float3(0,0,1));

	StandardLightingProperties surfaceStandardLightingProperties =
		DeriveLightingPropertiesForCommonSurfaceNoTint( surfaceInfo );

	surfaceStandardLightingProperties.diffuseColor = ProcessDiffuseColor(surfaceStandardLightingProperties.diffuseColor);

	return PackGBuffer(
		surfaceInfo.surface_worldNormal.xyz,
		surfaceStandardLightingProperties );
}
#endif //DEFERRED_TECHNIQUES

// ================================================================================================

#if __MAX

cableVertexOutput VS_MaxTransform(maxVertexInput IN)
{
	cableVertexInput temp=(cableVertexInput)0;

	temp.pos      = IN.pos;
//	temp.diffuse  = IN.diffuse;
//	temp.texCoord = Max2RageTexCoord4(float4(IN.texCoord0.xy, IN.texCoord1.xy));
//	temp.normal   = IN.normal;
	temp.tan      = float4(IN.tangent, 1);
	temp.r_d      = float2(0, 0);

	return VS_Cable(temp);
}

float4 PS_MaxTextured(cableVertexOutput IN) : COLOR
{
	return float4(0,0,0,1);
}

technique tool_draw { pass p0
{
	MAX_TOOL_TECHNIQUE_RS
	AlphaBlendEnable = true;
	AlphaTestEnable  = false;
	VertexShader     = compile VERTEXSHADER VS_MaxTransform();
	PixelShader      = compile PIXELSHADER  PS_MaxTextured();
}}

#else // not __MAX

#define CABLEPIXELSHADER	PixelShader = compile PIXELSHADER PS_Cable() CGC_FLAGS(CGC_DEFAULTFLAGS);

#if __XENON
#define CABLEPIXELSHADER_SHADOWS	PixelShader = NULL;
#else // __XENON
#define CABLEPIXELSHADER_SHADOWS	PixelShader = compile PIXELSHADER PS_CableShadow() CGC_FLAGS(CGC_DEFAULTFLAGS);
#endif // __XENON

#if !RAGE_USE_PN_TRIANGLES

#define CABLE_TECHNIQUES_BASE(techname, VSName, VSNamePN, PSCompileName) \
	technique techname { pass p0 \
	{ \
		VertexShader = compile VERTEXSHADER VSName(); \
		PSCompileName \
	}}

#else // not !RAGE_USE_PN_TRIANGLES

#define CABLE_TECHNIQUES_BASE(techname, VSName, VSNamePN, PSCompileName) \
	technique techname { pass p0 \
	{ \
		VertexShader = compile VERTEXSHADER VSName(); \
		PSCompileName \
	}} \
	technique SHADER_MODEL_50_OVERRIDE(techname##tessellated) { pass p0 \
	{ \
		VertexShader = compile VSDS_SHADER VSNamePN(); \
		SetHullShader(compileshader(hs_5_0, HS_PNTri())); \
		SetDomainShader(compileshader(ds_5_0, DS_PNTri())); \
		PSCompileName \
	}}
#endif // not !RAGE_USE_PN_TRIANGLES


#define CABLE_TECHNIQUES(techname) CABLE_TECHNIQUES_BASE(techname, VS_Cable, VS_PNTri, CABLEPIXELSHADER)

#if FORWARD_TECHNIQUES
CABLE_TECHNIQUES(draw)
CABLE_TECHNIQUES(lightweight0_draw)
CABLE_TECHNIQUES(lightweight4_draw)
CABLE_TECHNIQUES(lightweight8_draw)
#if HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED
CABLE_TECHNIQUES(lightweightHighQuality0_draw)
CABLE_TECHNIQUES(lightweightHighQuality4_draw)
CABLE_TECHNIQUES(lightweightHighQuality8_draw)
#endif // HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED
#endif // FORWARD_TECHNIQUES

#if DEFERRED_TECHNIQUES
#define CABLE_PIXELSHADER_DEFERRED	PixelShader = compile PIXELSHADER PS_Cable_Deferred() CGC_FLAGS(CGC_DEFAULTFLAGS);
#define CABLE_TECHNIQUES_DEFERRED(techname) CABLE_TECHNIQUES_BASE(deferred_##techname, VS_Cable, VS_PNTri, CABLE_PIXELSHADER_DEFERRED)

CABLE_TECHNIQUES_DEFERRED(draw)
#endif	// DEFERRED_TECHNIQUES

#if SHADOW_CASTING_TECHNIQUES
CABLE_TECHNIQUES_BASE(wdcascade_draw, VS_Cable_Cascade, VS_PNTri_Cascade, CABLEPIXELSHADER_SHADOWS)
//CABLE_TECHNIQUES_BASE(shadtexture_wdcascade_draw, VS_Cable_Cascade, VS_PNTri_Cascade, CABLEPIXELSHADER_SHADOWS)
#endif // SHADOW_CASTING_TECHNIQUES

#undef CABLE_TECHNIQUES

SHADERTECHNIQUE_ENTITY_SELECT(VS_Cable(), VS_Cable())
#include "../../Debug/debug_overlay_cable.fxh"

#endif // not __MAX
