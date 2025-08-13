
///////////////////////////////////////////////////////////////////////////////
// PTFX_COMMON.FXH - particle shader header file
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//
// All ptfx shaders should include this header.
// 
// POSTPROCESS_VS and POSTPROCESS_PS should be called
// at the end of the vertex and pixel shaders, respectively.
//  
// USABLE ANNOTATIONS
//		UIName			= "variable_name"	Recognize the variable and create UI controls for it
//		UIAssetName 	= "file_name"		Initial sampler name
//		UIHint			= "Hidden"			Hide the UI controls 
//		UIHint			= "Keyframeable"	Make the variable keyframeable.  
//											When this is set, the variable will be passed in per vertex
//											in custom1, custom2, custom3, etc., in the vertex shader input structure (ptfxSpriteVSInfo).
//											They will be packed in starting from custom2.y.								
//
//		float UIMin		= 0.0;				UI - min value
//		float UIMax		= 1.0;				UI - maximum value
//		float UIStep	= 0.01;				UI - slider step amount
//
// See ptfx_sprite.fx or ptfx_special.fx for usage example.
//
// RECOGNIZED GLOBAL VARIABLES
// The program will feed in the values for the following global variables if they're defined in the shader.
//
//	float		CurrentTime:CurrentTime REGISTER(c16);						// Time elapsed in seconds
//	float2		NearFarPlane:NearFarPlane REGISTER(c17);					// (near distance, far distance)
//
///////////////////////////////////////////////////////////////////////////////
 
#ifndef __PTFX_COMMON_FXH__
#define __PTFX_COMMON_FXH__


///////////////////////////////////////////////////////////////////////////////
// INCLUDES & DEFINES
///////////////////////////////////////////////////////////////////////////////

#define NO_SKINNING

//Also need to set PTXDRAW_USE_INSTANCING in ptxdraw.h
#define PTFX_USE_INSTANCING	((__WIN32PC && __SHADERMODEL >= 40) || RSG_DURANGO || RSG_ORBIS)

//Can not be enabled on Orbis due to Sony internal issue 18054
//Has no visible/performance gain at this point, hence disabled
#define PTFX_TESSELATE_SPRITES (0 && (__WIN32PC && __SHADERMODEL >= 40))

#if PTFX_TESSELATE_SPRITES
	// enabling tessellation
	#define RAGE_USE_PN_TRIANGLES			1
	// gWorld is identity, so we don't bother using it
	#define PN_TRIANGLES_USE_WORLD_SPACE	1
#endif	//PTFX_TESSELATE_SPRITES

// including after PN_TRIANGLE_* definitions
#include "../common.fxh"
#include "../../../rage/base/src/shaderlib/rage_xplatformtexturefetchmacros.fxh"
#include "../../../rage/suite/src/rmptfx/ptxdrawcommon.h"

#define SHADOW_CASTING            (0)
#define SHADOW_CASTING_TECHNIQUES (0)
#define SHADOW_RECEIVING          (1)
#define SHADOW_RECEIVING_VS       (1)
#include "../Lighting/Shadows/localshadowglobals.fxh"
#include "../Lighting/Shadows/cascadeshadows.fxh"

#define USE_SOFT_PARTICLES									(!__LOW_QUALITY && 1)
#if USE_SOFT_PARTICLES || __LOW_QUALITY
	#define RAGE_USE_SOFT_SAMPLER								1
	#define RAGE_USE_SOFT_CONSTANTS								1
	#define RAGE_SOFT_PARTICLE_DEPTH_SHARED_SAMPLER_REGISTER	s12
	#include "../../../rage/base/src/shaderlib/rage_softparticles.fxh"
#elif 0 // __LOW_QUALITY // What we need from soft particles
	CBSHARED BeginConstantBufferPagedDX10(rage_SoftParticleBuffer, b7)
	SHARED float4 NearFarPlane : NearFarPlane REGISTER(c128);   
	SHARED float4 gInvScreenSize : InvScreenSize REGISTER2( ps,c129); 
	EndConstantBufferDX10(rage_SoftParticleBuffer)

	#define RAGE_SOFT_PARTICLE_DEPTH_SHARED_SAMPLER_REGISTER	s12

	// Depth map
	BeginSharedSampler(sampler,DepthMap,DepthMapTexSampler,DepthMap,RAGE_SOFT_PARTICLE_DEPTH_SHARED_SAMPLER_REGISTER)	// Depth map
	ContinueSharedSampler(sampler,DepthMap,DepthMapTexSampler,DepthMap,RAGE_SOFT_PARTICLE_DEPTH_SHARED_SAMPLER_REGISTER) 
	   MinFilter = POINT;
	   MagFilter = POINT;
	   MipFilter = NONE;
	   AddressU  = CLAMP;
	   AddressV  = CLAMP;
	EndSharedSampler;

#endif

#define IMPLEMENT_BLEND_MODES_AS_PASSES 1 // See counterpart in rage\suite\src\rmptfx\ptxconfig.h

#if IMPLEMENT_BLEND_MODES_AS_PASSES

#define IMPLEMENT_BLEND_MODES_AS_PASSES_SWITCH(x, y) x
#define IMPLEMENT_BLEND_MODES_AS_PASSES_PARAM(x) , x

#else

#define IMPLEMENT_BLEND_MODES_AS_PASSES_SWITCH(x, y) y
#define IMPLEMENT_BLEND_MODES_AS_PASSES_PARAM(x)

#endif

// Particle shadows rely on the extra passes.
// disable orbis for now ...
#define USE_PARTICLE_SHADOWS	(1 && IMPLEMENT_BLEND_MODES_AS_PASSES && (RSG_PC || RSG_DURANGO || RSG_ORBIS))

#if USE_PARTICLE_SHADOWS
#define USE_PARTICLE_SHADOWS_ONLY(x) x
#else
#define USE_PARTICLE_SHADOWS_ONLY(x)
#endif


#define PTFX_MIRROR_NEAR_PLANE_FADE_DISTANCE (0.05f)
#define PTFX_MIRROR_NEAR_PLANE_FADE_INV_DISTANCE (1.0f / PTFX_MIRROR_NEAR_PLANE_FADE_DISTANCE)

///////////////////////////////////////////////////////////////////////////////
// STRUCTURES
///////////////////////////////////////////////////////////////////////////////

// need to maintain separate ptfxSpriteVSInfo and ptfxSpriteVSInfoProj due to limit on vs input parameters
struct ptfxSpriteVSInfo
{
	float3 centerPos		: POSITION;						// center position of the sprite
	float4 col				: COLOR0;					// sprite colour
	float3 normal			: NORMAL;						// normal of the sprite (xyw)
	float4 misc				: TEXCOORD0;					// texFrameRatio, emissiveIntensity, unused, unused (note that misc.w is used for depth in ptfxSpritePSInfo)
	float4 custom1			: TEXCOORD1;					// custom shader-defined vars
	float4 custom2			: TEXCOORD2;					// custom shader-defined vars

	//Need to put all the instance data together otherwise AMD cards go crazy.
#if PTFX_USE_INSTANCING
	float4 offsetUp			: TEXCOORD6;					// Up Offset	- w Left Offet x
	float4 offsetDown		: TEXCOORD7;					// Down offset	- w Left Offet y
	float4 offsetRight		: TEXCOORD8;					// right offset - w Left Offet z
	float4 clipRegion		: TEXCOORD9;					// clip region
	float4 misc1			: TEXCOORD10;					// non projected: uvInfoA
	float4 misc2			: TEXCOORD11;					// non projected: uvInfoB														// projected: x - projectionDepth, y - TexFrameA, z - TexFrameB, w - numTexColom
#if !PTFX_SHADOWS_INSTANCING
	uint VertexID			: SV_VertexID;
#endif
#endif
#if PTFX_SHADOWS_INSTANCING
	uint instID				: SV_InstanceID;
#endif

	float3 wldPos			: TEXCOORD3;					// world position of the vertex
															// for directional sprites (worldPos)
	float4 texUVs			: TEXCOORD4;					// uvs of source and destination diffuse textures (u0, v0, u1, v1)
	float4 normUV			: TEXCOORD5;					// normalU, normalV, distToCentre, unused
};

#if PTFX_SHADOWS_INSTANCING
struct ptfxSpriteVSInfoProj
{
	float3 centerPos		: POSITION;						// center position of the sprite
	float4 col				: COLOR0;						// sprite colour
	float3 normal			: NORMAL;						// normal of the sprite (xyw)
	float4 misc				: TEXCOORD0;					// texFrameRatio, emissiveIntensity, unused, unused (note that misc.w is used for depth in ptfxSpritePSInfo)
	float4 custom1			: TEXCOORD1;					// custom shader-defined vars
	float4 custom2			: TEXCOORD2;					// custom shader-defined vars

	//Need to put all the instance data together otherwise AMD cards go crazy.
#if PTFX_USE_INSTANCING
	float4 offsetUp			: TEXCOORD6;					// Up Offset	- w Left Offet x
	float4 offsetDown		: TEXCOORD7;					// Down offset	- w Left Offet y
	float4 offsetRight		: TEXCOORD8;					// right offset - w Left Offet z
	float4 clipRegion		: TEXCOORD9;					// clip region
	//float4 misc1			: TEXCOORD10;					// non projected: uvInfoA
	float4 misc2			: TEXCOORD11;					// non projected: uvInfoB
															// projected: x - projectionDepth, y - TexFrameA, z - TexFrameB, w - numTexColomns
	uint VertexID			: SV_VertexID; 
#endif
	uint instID				: SV_InstanceID;

	float3 wldPos			: TEXCOORD3;					// world position of the vertex
															// for directional sprites (worldPos)
	float4 texUVs			: TEXCOORD4;					// uvs of source and destination diffuse textures (u0, v0, u1, v1)
	float4 normUV			: TEXCOORD5;					// normalU, normalV, distToCentre, unused
};
#endif

#if PTFX_TESSELATE_SPRITES
struct ptfxSpriteVSInfo_CtrlPoint
{
	float3 centerPos		: CTRL_POSITION;						// center position of the sprite
	float4 col				: CTRL_COLOR0;						// sprite colour
	float3 normal			: CTRL_NORMAL;						// normal of the sprite (xyw)
	float4 misc				: CTRL_TEXCOORD0;					// texFrameRatio, emissiveIntensity, unused, unused (note that misc.w is used for depth in ptfxSpritePSInfo)
	float4 custom1			: CTRL_TEXCOORD1;					// custom shader-defined vars
	float4 custom2			: CTRL_TEXCOORD2;					// custom shader-defined vars
	float3 custom3			: CTRL_TEXCOORD12;					// variable custom data

#if PTFX_USE_INSTANCING
	float4 offsetUp			: CTRL_TEXCOORD6;					// Up Offset	- w Left Offet x
	float4 offsetDown		: CTRL_TEXCOORD7;					// Down offset	- w Left Offet y
	float4 offsetRight		: CTRL_TEXCOORD8;					// right offset - w Left Offet z
	float4 clipRegion		: CTRL_TEXCOORD9;					// clip region
	float4 misc1			: CTRL_TEXCOORD10;					// non projected: uvInfoA
	float4 misc2			: CTRL_TEXCOORD11;					// non projected: uvInfoB
															// projected: x - projectionDepth, y - TexFrameA, z - TexFrameB, w - numTexColomns 
#endif

	float3 wldPos			: CTRL_TEXCOORD3;					// world position of the vertex
															// for directional sprites (worldPos)
	float4 texUVs			: CTRL_TEXCOORD4;					// uvs of source and destination diffuse textures (u0, v0, u1, v1)
	float4 normUV			: CTRL_TEXCOORD5;					// normalU, normalV, distToCentre, unused
};
#endif //PTFX_TESSELATE_SPRITES

#if __SHADERMODEL >= 40

struct ptfxSpriteVSOut
{
	half4 col				: TEXCOORD0;					// sprite colour

	// variables below are set by ptfxSpritePostProcessVS
	float4 texUVs			: TEXCOORD1;					// uvs of source and destination diffuse textures (u0, v0, u1, v1)
	float4 normUV			: TEXCOORD2;					// normalU, normalV, desatScale, gammaScale
	NOINTERPOLATION float2 misc	: TEXCOORD3;				// texFrameRatio, emissiveIntensity 
	float4 fogParam			: TEXCOORD5;					// fogColor.r, fogColor.g, fogColor.b, fogBlend / store instanceID in .x for shadows

	NOINTERPOLATION float4 custom1 : TEXCOORD6;				// custom shader-defined vars
	NOINTERPOLATION float4 custom2 : TEXCOORD7;				// xy: custom shader-defined vars, for Proj Ptfx - xy:ptfxProj_shadowUV, zw:ptfxProj_uvStep
	float4 custom3			: TEXCOORD8;					// x: distance to center, y: squared distance to camera, z: shadowAmount, w:DepthW
	float4 wldPos			: TEXCOORD9;
	float3 normal			: TEXCOORD10;
	float4 tangent			: TEXCOORD11;
#if PTFX_SHADOWS_INSTANCING_GEOMSHADER
	uint InstID			: TEXCOORD12;
#endif
	DECLARE_POSITION_CLIPPLANES(pos)
};

struct ptfxSpritePSIn
{
	half4 col				: TEXCOORD0;					// sprite colour

	// variables below are set by ptfxSpritePostProcessVS
	float4 texUVs			: TEXCOORD1;					// uvs of source and destination diffuse textures (u0, v0, u1, v1)
	float4 normUV			: TEXCOORD2;					// normalU, normalV, desatScale, gammaScale
	NOINTERPOLATION float2 misc	: TEXCOORD3;				// texFrameRatio, emissiveIntensity 
	float4 fogParam			: TEXCOORD5;					// fogColor.r, fogColor.g, fogColor.b, fogBlend

	NOINTERPOLATION float4 custom1 : TEXCOORD6;				// custom shader-defined vars and  store instanceID in .x for shadows
	NOINTERPOLATION float4 custom2 : TEXCOORD7;				// xy: custom shader-defined vars, for Proj Ptfx - xy:ptfxProj_shadowUV, zw:ptfxProj_uvStep
	float4 custom3			: TEXCOORD8;					// x: distance to center, y: squared distance to camera, z: shadowAmount, w:DepthW
	float4 wldPos			: TEXCOORD9;
	float3 normal			: TEXCOORD10;
	float4 tangent			: TEXCOORD11;
	DECLARE_POSITION_CLIPPLANES_PSIN(pos)
};
#else 
//keeping this to get SM 3.0 to compile
struct ptfxSpriteVSOut
{
	DECLARE_POSITION(pos)
	half4 col				: TEXCOORD0;					// sprite colour

	// variables below are set by ptfxSpritePostProcessVS
	float4 texUVs			: TEXCOORD1;					// uvs of source and destination diffuse textures (u0, v0, u1, v1)
	float4 normUV			: TEXCOORD2;					// normalU, normalV, desatScale, gammaScale
	float4 misc				: TEXCOORD3;					// texFrameRatio, emissiveIntensity, DepthZ, DepthW
	float4 fogParam			: TEXCOORD4;					// fogColor.r, fogColor.g, fogColor.b, fogBlend / store instanceID in .x for shadows

	float4 custom1			: TEXCOORD5;					// custom shader-defined vars
	float4 custom2			: TEXCOORD6;					// xy: custom shader-defined vars, for Proj Ptfx - xy:ptfxProj_shadowUV, zw:ptfxProj_uvStep
	float4 wldPos			: TEXCOORD7;
	float3 normal			: TEXCOORD8;
	float4 tangent			: TEXCOORD9;
#if PTFX_SHADOWS_INSTANCING_GEOMSHADER
	uint InstID			: TEXCOORD10;
#endif
};

struct ptfxSpritePSIn
{
	DECLARE_POSITION_PSIN(pos)
	half4 col				: TEXCOORD0;					// sprite colour

	// variables below are set by ptfxSpritePostProcessVS
	float4 texUVs			: TEXCOORD1;					// uvs of source and destination diffuse textures (u0, v0, u1, v1)
	float4 normUV			: TEXCOORD2;					// normalU, normalV, desatScale, gammaScale
	float4 misc				: TEXCOORD3;					// texFrameRatio, emissiveIntensity 
	float4 fogParam			: TEXCOORD4;					// fogColor.r, fogColor.g, fogColor.b, fogBlend

	float4 custom1			: TEXCOORD5;					// custom shader-defined vars and  store instanceID in .x for shadows
	float4 custom2			: TEXCOORD6;					// xy: custom shader-defined vars, for Proj Ptfx - xy:ptfxProj_shadowUV, zw:ptfxProj_uvStep
	float4 wldPos			: TEXCOORD7;
	float3 normal			: TEXCOORD8;
	float4 tangent			: TEXCOORD9;
};
#endif

#if PTXDRAW_USE_SPU

struct ptfxTrailVSInfo
{
	// ptxTrailVtxInstance[0]
	float3 p0_pos0			: POSITION;
	float4 p0_color			: COLOR;
	float4 p0_pos1_tex		: NORMAL; // w = texcoord.x

	// ptxTrailVtxInstance[1]
	float3 p1_pos0			: TEXCOORD0;
	float4 p1_color			: TEXCOORD1;
	float4 p1_pos1_tex		: TEXCOORD2; // w = texcoord.x

	// ptxTrailVtxPattern
	float2 pattern			: TEXCOORD3; // x selects [p0,p1], y interpolates between pos0..pos1
};

float3 ptfxTrailVSInfo_pos     (ptfxTrailVSInfo IN) { return IN.pattern.x < 0.5 ? lerp(IN.p0_pos0, IN.p0_pos1_tex.xyz, IN.pattern.y) : lerp(IN.p1_pos0, IN.p1_pos1_tex.xyz, IN.pattern.y); }
float4 ptfxTrailVSInfo_color   (ptfxTrailVSInfo IN) { return IN.pattern.x < 0.5 ? IN.p0_color : IN.p1_color; }
float2 ptfxTrailVSInfo_texcoord(ptfxTrailVSInfo IN) { return float2(IN.pattern.x < 0.5 ? IN.p0_pos1_tex.w : IN.p1_pos1_tex.w, IN.pattern.y); }
float3 ptfxTrailVSInfo_radius  (ptfxTrailVSInfo IN) { return IN.pattern.x < 0.5 ? length(IN.p0_pos0 - IN.p0_pos1_tex.xyz) : length(IN.p1_pos0 - IN.p1_pos1_tex.xyz); } // experimental for soft trails
float3 ptfxTrailVSInfo_centre  (ptfxTrailVSInfo IN) { return IN.pattern.x < 0.5 ? (IN.p0_pos0 + IN.p0_pos1_tex.xyz)*0.5 : (IN.p1_pos0 + IN.p1_pos1_tex.xyz)*0.5; }

struct ptfxTrailVSInfo_lit
{
	// ptxTrailVtxInstance[0]
	float3 p0_pos0			: POSITION;
	float4 p0_color			: COLOR;
	float4 p0_pos1_tex		: NORMAL; // w = texcoord.x
	float3 p0_tangent		: TEXCOORD0;

	// ptxTrailVtxInstance[1]
	float3 p1_pos0			: TEXCOORD1;
	float4 p1_color			: TEXCOORD2;
	float4 p1_pos1_tex		: TEXCOORD3; // w = texcoord.x
	float3 p1_tangent		: TEXCOORD4;

	// ptxTrailVtxPattern
	float2 pattern			: TEXCOORD5; // x selects [p0,p1], y interpolates between pos0..pos1
};

float3 ptfxTrailVSInfo_lit_pos     (ptfxTrailVSInfo_lit IN) { return IN.pattern.x < 0.5 ? lerp(IN.p0_pos0, IN.p0_pos1_tex.xyz, IN.pattern.y) : lerp(IN.p1_pos0, IN.p1_pos1_tex.xyz, IN.pattern.y); }
float4 ptfxTrailVSInfo_lit_color   (ptfxTrailVSInfo_lit IN) { return IN.pattern.x < 0.5 ? IN.p0_color : IN.p1_color; }
float2 ptfxTrailVSInfo_lit_texcoord(ptfxTrailVSInfo_lit IN) { return float2(IN.pattern.x < 0.5 ? IN.p0_pos1_tex.w : IN.p1_pos1_tex.w, IN.pattern.y); }
float3 ptfxTrailVSInfo_lit_radius  (ptfxTrailVSInfo_lit IN) { return IN.pattern.x < 0.5 ? length(IN.p0_pos0 - IN.p0_pos1_tex.xyz) : length(IN.p1_pos0 - IN.p1_pos1_tex.xyz); } // experimental for soft trails
float3 ptfxTrailVSInfo_lit_centre  (ptfxTrailVSInfo_lit IN) { return IN.pattern.x < 0.5 ? (IN.p0_pos0 + IN.p0_pos1_tex.xyz)*0.5 : (IN.p1_pos0 + IN.p1_pos1_tex.xyz)*0.5; }
float3 ptfxTrailVSInfo_lit_tangent (ptfxTrailVSInfo_lit IN) { return normalize(IN.pattern.x < 0.5 ? IN.p0_tangent : IN.p1_tangent); }
float3 ptfxTrailVSInfo_lit_normal  (ptfxTrailVSInfo_lit IN, float3 v)
{
	const float3 tangent = ptfxTrailVSInfo_lit_tangent(IN);
	const float y = IN.pattern.y*2 - 1;
	return normalize(cross(v, tangent)*y + v*(1 - abs(y)));
}

#else // not PTXDRAW_USE_SPU

struct ptfxTrailVSInfo
{
	// ptxTrailVertex
	float3 pos				: POSITION;
	float4 color			: COLOR0;
	float2 texcoord			: TEXCOORD0;
#if RAGE_INSTANCED_TECH
	uint instID : SV_InstanceID;
#endif
};

float3 ptfxTrailVSInfo_pos     (ptfxTrailVSInfo IN) { return IN.pos; }
float4 ptfxTrailVSInfo_color   (ptfxTrailVSInfo IN) { return IN.color; }
float2 ptfxTrailVSInfo_texcoord(ptfxTrailVSInfo IN) { return IN.texcoord.xy; }
float3 ptfxTrailVSInfo_radius  (ptfxTrailVSInfo IN) { return 1; }
float3 ptfxTrailVSInfo_centre  (ptfxTrailVSInfo IN) { return IN.pos; }

struct ptfxTrailVSInfo_lit
{
	// ptxTrailVertex
	float3 pos				: POSITION;
	float4 color			: COLOR0;
	float2 texcoord			: TEXCOORD0;
	float3 tangent			: TEXCOORD1;
#if RAGE_INSTANCED_TECH
	uint instID : SV_InstanceID;
#endif
};

float3 ptfxTrailVSInfo_lit_pos     (ptfxTrailVSInfo_lit IN) { return IN.pos; }
float4 ptfxTrailVSInfo_lit_color   (ptfxTrailVSInfo_lit IN) { return IN.color; }
float2 ptfxTrailVSInfo_lit_texcoord(ptfxTrailVSInfo_lit IN) { return IN.texcoord.xy; }
float3 ptfxTrailVSInfo_lit_radius  (ptfxTrailVSInfo_lit IN) { return 1; }
float3 ptfxTrailVSInfo_lit_centre  (ptfxTrailVSInfo_lit IN) { return IN.pos; }
float3 ptfxTrailVSInfo_lit_tangent (ptfxTrailVSInfo_lit IN) { return normalize(IN.tangent); }
float3 ptfxTrailVSInfo_lit_normal  (ptfxTrailVSInfo_lit IN, float3 v)
{
	const float3 tangent = ptfxTrailVSInfo_lit_tangent(IN);
	const float y = IN.texcoord.y*2 - 1;
	return normalize(cross(-v, tangent)*y + v*(1 - abs(y)));
}

#endif // not PTXDRAW_USE_SPU

struct ptfxTrailVSOut
{
	float4 color			: TEXCOORD0;
	float2 texcoord			: TEXCOORD1;
	float4 fog				: TEXCOORD2;  //stores instanceID in .x for shadows
	float2 soft				: TEXCOORD3;
#if PTFX_SHADOWS_INSTANCING_GEOMSHADER
	uint InstID			: TEXCOORD4;
#endif
	DECLARE_POSITION_CLIPPLANES(pos)
};

struct ptfxTrailPSIn
{
	float4 color			: TEXCOORD0;
	float2 texcoord			: TEXCOORD1;
	float4 fog				: TEXCOORD2; //stores instanceID in .x for shadows
	float2 soft				: TEXCOORD3;
	DECLARE_POSITION_CLIPPLANES_PSIN(pos)
};

struct ptfxOutputStruct
{
	half4	col0	: SV_Target0;
#if PTFX_APPLY_DOF_TO_PARTICLES
	DofOutputStruct dof;
#endif //PTFX_APPLY_DOF_TO_PARTICLES
};

#define ptfxInfoTrail_depthW   			soft.x
#define ptfxInfoTrail_distToCentre		soft.y


#define ptfxInfo_texFrameRatio 		misc.x
#define ptfxInfo_emissiveIntensity 	misc.y

#if __SHADERMODEL>=40
#define ptfxInfo_depthW         	custom3.w
#else
#define ptfxInfo_depthW         	misc.w
#endif
#define ptfxInfo_projDepth     		custom1.x

#define ptfxInfo_softness      		custom1.x
#define ptfxInfo_superAlpha    		custom1.y		// Not used in the pixel shader.
#define ptfxInfo_shadowAmount  		custom1.z		// Not used in the pixel shader.
#define ptfxInfo_cameraShrink  		custom1.w		// Not used in the pixel shader.
#define ptfxInfo_colRGB     		custom1.yzw		// used only in the pixel shader.
#define ptfxInfo_directionalColRGB	normal.xyz
#define ptfxInfo_normalArc     		custom2.x
#define ptfxInfo_normalBias    		custom2.y

#if __SHADERMODEL>=40
#define ptfxInfo_distToCenterTimeSoftnessCurve	custom3.x
#define ptfxInfo_RG_BlendFactor 	custom3.y
#define ptfxInfo_PS_ShadowAmount	custom3.z		//Also used for distance in shadow pass
#define ptfxInfo_CBInstIndex		custom1.z		//only used in shadow (where RGB is not used at all)
#define ptfxProj_PS_shadowAmount	custom3.z
#else
#define ptfxInfo_distToCenterTimeSoftnessCurve	custom2.z
#define ptfxInfo_RG_BlendFactor 	custom2.w
#define ptfxInfo_PS_ShadowAmount	wldPos.w	
#define ptfxInfo_CBInstIndex		fogparam.x		//only used in shadow (where RGB is not used at all)
#define ptfxProj_PS_shadowAmount	wldPos.w
#endif

#define ptfxProj_dirLight			custom1.xyz
#define ptfxProj_shadowUV			custom2.xy
#define ptfxProj_uvStep				custom2.zw
#define ptfxProj_idxA				texUVs.xy
#define ptfxProj_idxB				texUVs.zw

/////////////////////////////////////////////////////////////////////////////
// ptfxGetWorldPosFromVpos

BeginConstantBufferDX10( ptfx_common_locals )
float4	deferredLightScreenSize;
float4	deferredProjectionParams;
float3	deferredPerspectiveShearParams0;									// combination of perspective shear, deferred projection params,and inverse view matrix. 
float3	deferredPerspectiveShearParams1;									// set up for quick combination with the screen position to get eye vectors. 
float3	deferredPerspectiveShearParams2;									
float4  postProcess_FlipDepth_NearPlaneFade_Params;
float4  waterfogPtfxParams;
float4  activeShadowCascades;
float4	numActiveShadowCascades;	// x - number of cascades, y - 1.0 / num cascades
EndConstantBufferDX10( ptfx_common_locals )

#define ptfx_UseWaterPostProcess		(postProcess_FlipDepth_NearPlaneFade_Params.x)
#define ptfx_UseWaterGammaCompression	(postProcess_FlipDepth_NearPlaneFade_Params.y)
#define ptfx_InvertDepth				(postProcess_FlipDepth_NearPlaneFade_Params.z)	
#define ptfx_ApplyNearPlaneFade			(postProcess_FlipDepth_NearPlaneFade_Params.w)	
#define ptfx_WaterFogOpacity			(waterfogPtfxParams.x)
#define ptfx_WaterZHeight				(waterfogPtfxParams.y)
#define ptfx_WaterFogLightIntensity		(waterfogPtfxParams.z)
#define ptfx_WaterFogPierceIntensity	(waterfogPtfxParams.w)

BeginSampler(	sampler, gbufferTextureDepth, GBufferTextureSamplerDepth, gbufferTextureDepth)
ContinueSampler(sampler, gbufferTextureDepth, GBufferTextureSamplerDepth, gbufferTextureDepth)
	AddressU  = CLAMP;        
	AddressV  = CLAMP;
	MINFILTER = POINT;
	MAGFILTER = POINT;
	MIPFILTER = POINT;
EndSampler;

BeginSampler( sampler, waterColorTexture, WaterColorSampler, waterColorTexture)
ContinueSampler(sampler, waterColorTexture, WaterColorSampler, waterColorTexture)
AddressU	= WRAP;
AddressV	= WRAP;
MIPFILTER	= NONE;
MINFILTER	= POINT;
MAGFILTER	= POINT;
EndSampler;

BeginSampler(sampler,FogRayMap, FogRayTexSampler,FogRayMap)	// Depth map for fog ray
ContinueSampler(sampler,FogRayMap, FogRayTexSampler,FogRayMap) 
   MinFilter = LINEAR;
   MagFilter = LINEAR;
   MipFilter = NONE;
   AddressU  = CLAMP;
   AddressV  = CLAMP;
EndSampler;

#if __XENON
BeginSampler(	sampler, gbufferStencilTexture, GBufferStencilTextureSampler, gbufferStencilTexture)
ContinueSampler(sampler, gbufferStencilTexture, GBufferStencilTextureSampler, gbufferStencilTexture)
	AddressU  = CLAMP;        
	AddressV  = CLAMP;
	MINFILTER = POINT;
	MAGFILTER = POINT;
	MIPFILTER = POINT;
EndSampler;
#endif//__XENON

// NOTE: ViewPos is in range [-1,1]
float4 ptfxGetEyeRay(float2 signedScreenPos)
{
	// Some constants are precombined, to avoid constant ops 
	// Originally was:
	//  	const float2 projPos = (signedScreenPos + deferredPerspectiveShearParams.xy) * deferredProjectionParams.xy;
	//      return float4(mul( float4(projPos,-1,0), gViewInverse ).xyz, 1);
	// 
	// After factoring, it is now only a 3x3 transform instead of add + scale + 4x4 transform
	const float3 transformVec = float3( signedScreenPos, 1.0 );
	return float4( dot( transformVec, deferredPerspectiveShearParams0), 
		dot( transformVec, deferredPerspectiveShearParams1),
		dot( transformVec, deferredPerspectiveShearParams2), 1);
}

void ptfxGetWorldPosFromVpos(float2 vPos, float4 eyeRay, out float3 worldPos)
{
	float2 screenPos = convertToNormalizedScreenPos(vPos, deferredLightScreenSize);

#if __XENON
	float2 posXY=screenPos.xy;
	float	stencilSample;
	asm 
    { 
		tfetch2D stencilSample.z___, posXY.xy, GBufferStencilTextureSampler
	};
	float depthSample;
	asm 
    { 
		tfetch2D depthSample.x___, posXY.xy, GBufferTextureSamplerDepth 
	};
#elif RSG_PC || RSG_DURANGO || RSG_ORBIS || __MAX
	float2 posXY = screenPos.xy;
	float4 depthStencilSample = tex2D(GBufferTextureSamplerDepth, posXY.xy);
	float depthSample = depthStencilSample.x;
#else	// platforms
	// "Don't make me a half float, I need the extra precision" said posXY...
	float2 posXY = screenPos.xy;
	float4 depthStencilSample = northTexDepthStencil2D(GBufferTextureSamplerDepth, posXY.xy); //this is preferable because it goes directly to the function rather than via the shadows header
	float depthSample = depthStencilSample.x;
#endif	// platforms

	// Unpack the world space position from depth
	float linearDepth = getLinearGBufferDepth(depthSample, deferredProjectionParams.zw);

	// Normalize eye ray if needed
	eyeRay.xyz /= eyeRay.w;
	worldPos = gViewInverse[3].xyz + (eyeRay.xyz * linearDepth);
}

#if !__LOW_QUALITY
///////////////////////////////////////////////////////////////////////////////
// ptfxSpriteGetDepthValue
void ptfxGetDepthValue( float2 scrPos, out float depth )
{
#if __PS3 || __WIN32PC || RSG_ORBIS
	float2 screenPos = scrPos.xy * gInvScreenSize.xy;
#else
	float2 screenPos = scrPos.xy * gInvScreenSize.xy + gInvScreenSize.zw;
#endif // __PS3

#if __PS3
	float zDepth = 1.0f - texDepth2D(DepthMapTexSampler, screenPos.xy);
#elif __WIN32PC || RSG_ORBIS
	float zDepth = 1.0f - tex2D(DepthMapTexSampler, screenPos.xy).x;
#else
	float zDepth = tex2D(DepthMapTexSampler, screenPos.xy).x;	// depth value at pixel position
#endif // __PS3

	//If it's mirror reflection we should invert the depth
	zDepth = (ptfx_InvertDepth?(1.0f - zDepth):zDepth);
	float currDepth = fixupGBufferDepth(zDepth) * NearFarPlane[2] + NearFarPlane[3]; 
	currDepth = 1.0f/currDepth;

	depth = currDepth;
}
#endif // !__LOW_QUALITY

float ptfxGetFogRayIntensity( float2 scrPos )
{
#if __PS3 || __WIN32PC || RSG_ORBIS
	float2 screenPos = scrPos.xy * gInvScreenSize.xy;
#else
	float2 screenPos = scrPos.xy * gInvScreenSize.xy + gInvScreenSize.zw;
#endif // __PS3

	float rayIntensity = tex2D(FogRayTexSampler, screenPos.xy).x;

	return saturate(lerp(1.0f,rayIntensity,gGlobalFogIntensity));
}


///////////////////////////////////////////////////////////////////////////////
// ptfxApplyNearPlaneFade
float ptfxApplyNearPlaneFade(float distanceFromNearPlane)
{
	return ptfx_ApplyNearPlaneFade?saturate(distanceFromNearPlane * PTFX_MIRROR_NEAR_PLANE_FADE_INV_DISTANCE):1.0f;

}

///////////////////////////////////////////////////////////////////////////////
// ptfxCommonGlobalWaterRefractionPostProcess_VS
float2 ptfxCommonGlobalWaterRefractionPostProcess_VS(float3 worldPos)
{

	float waterOpacity = pow(ptfx_WaterFogOpacity, 2);
	const float3 camPos = +gViewInverse[3].xyz;
	float3 camToPtfxVector = worldPos - camPos;
	const float particleDistFromCamera = length(camToPtfxVector);
	const float3 camToPtfxDir = camToPtfxVector/particleDistFromCamera.xxx;
	const float3 camDir = -gViewInverse[2].xyz;

	float cosAngleEyeWaterHeight = abs(dot(float3(0,0,-1), camToPtfxDir));
	float ooCosAngleEyeWaterHeight = 1.0f/cosAngleEyeWaterHeight;
	float distanceBetweenPtfxWaterSurface = abs(ptfx_WaterZHeight - worldPos.z) * ooCosAngleEyeWaterHeight;
	float distanceBetweenCamWaterSurface = abs(camPos.z - ptfx_WaterZHeight) * ooCosAngleEyeWaterHeight;
	float3 waterSurfaceIntersection = worldPos - camToPtfxDir * distanceBetweenPtfxWaterSurface.xxx;

	float cosAngleEyeDirPtfxDir = abs(dot(camDir, camToPtfxDir));

	float camPlaneWaterSurfaceDistance = cosAngleEyeDirPtfxDir * distanceBetweenCamWaterSurface;
	float camPlanePtfxDistance = cosAngleEyeDirPtfxDir * (distanceBetweenCamWaterSurface + distanceBetweenPtfxWaterSurface);

	float projectedWaterDepth = (camPlaneWaterSurfaceDistance * deferredProjectionParams.z + deferredProjectionParams.w) / (-camPlaneWaterSurfaceDistance);
	float projectedPtfxDepth = (camPlanePtfxDistance * deferredProjectionParams.z + deferredProjectionParams.w) / (-camPlanePtfxDistance);
		
	float projectedDepthDiff = abs(projectedPtfxDepth - projectedWaterDepth);

	float2 depthBlends	= exp(float2(-20, -60*abs(projectedWaterDepth)) * waterOpacity * projectedDepthDiff * 2.71828183);

	//Pierce Scale for oceans. For all other water types, there is no pierce scale.
	//float pierceScale		= pow(saturate(dot(refract(V, IN.Normal, 1/1.65), SunDirection)), 2)*saturate(depth/10);

	return depthBlends;
}

#if !__LOW_QUALITY
///////////////////////////////////////////////////////////////////////////////
// ptfxCommonGlobalWaterRefractionPostProcess_PS
half4 ptfxCommonGlobalWaterRefractionPostProcess_PS(half4 color, float2 pos, half2 depthBlends)
{
	//calculate screen space tex coords
	float2 screenSpaceTex		= pos.xy * gInvScreenSize.xy;
	float4 particleColor		= color;
#if __XENON
	half4 WaterColorSample	= _Tex2DOffset(WaterColorSampler, screenSpaceTex, 0.5f);
#else 
	half4 WaterColorSample	= h4tex2D(WaterColorSampler,	screenSpaceTex);
#endif
	half3 nWaterColor		= WaterColorSample.rgb;

	half3 waterColor		= pow(nWaterColor, 2);
	half3 sunColor			= gDirectionalColour.rgb;
	half3 sunDirection		= gDirectionalLight.xyz;

	half3 fogLight			= gLightNaturalAmbient0 - sunDirection.z*sunColor;//*lerp(.2, 1, shadow.x);

	waterColor				= waterColor * fogLight * ptfx_WaterFogLightIntensity;
	half3 refractionTintPtfxColor = 2 * nWaterColor.rgb * particleColor.rgb;
	//refractionTintPtfxColor	= lerp(refractionTintPtfxColor, particleColor.rgb, depthBlend.y);
	
	refractionTintPtfxColor	= lerp(refractionTintPtfxColor, particleColor.rgb, depthBlends.y);//return half4(depthBlend.yyy, 1.0f);
	refractionTintPtfxColor	= lerp(waterColor, refractionTintPtfxColor, depthBlends.x);

	//add pierce (we dont have pierce Scale yet)
	//refractionTintPtfxColor			= refractionTintPtfxColor + pierceScale*nWaterColor*sunColor*ptfx_WaterFogPierceIntensity;//*(shadow.x)
	//refractionTintPtfxColor = float4(refractionTintPtfxColor, saturatedDepth > 0);
	color					= nWaterColor.g? color : particleColor;
	
	return half4(refractionTintPtfxColor, color.a);
}
#endif // !__LOW_QUALITY

float ptfxGetShadowMapBiasedDepth(float2 vPos, float depth, float distance)
{
	float biasedDepth = depth;
	if( distance < gGlobalPtfxShadowDepthBiasRange + gGlobalPtfxShadowDepthBiasRangeFalloff )
	{
		float2 oneOverSize = gShadowRes.zw;

		//Use four shadow map samples to determine the gradient of the object we're depth testing against
		//then bias the particles depth based on that to prevent biasing issues.

		float2 depthXPlus = float2(vPos.x + 1, vPos.y) * oneOverSize;
		float zDepthXPlus = tex2Dlod(DepthMapTexSampler, float4(depthXPlus, 0.0, 0.0)).x;

		float2 depthXMinus = float2(vPos.x - 1, vPos.y) * oneOverSize;
		float zDepthXMinus = tex2Dlod(DepthMapTexSampler, float4(depthXMinus, 0.0, 0.0)).x;

		float2 depthYPlus = float2(vPos.x, vPos.y + 1) * oneOverSize;
		float zDepthYPlus = tex2Dlod(DepthMapTexSampler, float4(depthYPlus, 0.0, 0.0)).x;

		float2 depthYMinus = float2(vPos.x, vPos.y - 1) * oneOverSize;
		float zDepthYMinus = tex2Dlod(DepthMapTexSampler, float4(depthYMinus, 0.0, 0.0)).x;

		float shadowDx = (zDepthXPlus - zDepthXMinus);
		float shadowDy = (zDepthYPlus - zDepthYMinus);

		float depthSlope = fixupGBufferDepth(max(abs(shadowDx), abs(shadowDy)));

	#if (__SHADERMODEL >= 40)
		float depthSlopeBias  = gGlobalPtfxShadowSlopeBias;
		//This currently isnt used but left there in case theres a need for it.
		float depthBias = 0.0;
	#else
		float depthSlopeBias  = 0;
		float depthBias = 0;
	#endif

		biasedDepth = (depth + (depthSlope * depthSlopeBias + depthBias));

		//Fade off the biasing
		float fadeVal = saturate((distance - gGlobalPtfxShadowDepthBiasRange) * gGlobalPtfxShadowDepthBiasRangeDivision);
		biasedDepth = lerp(biasedDepth, depth, fadeVal);
	}

	return biasedDepth;
}

int ptfxGetCascadeInstanceIndex(uint instanceID)
{
	int cascadeCount = (int)numActiveShadowCascades.x;
	float oneOverCascadeCount = numActiveShadowCascades.y;
	int cascadeInstIndex = instanceID - (int)(instanceID  * oneOverCascadeCount) * cascadeCount;		
	cascadeInstIndex = activeShadowCascades[cascadeInstIndex];
	return cascadeInstIndex;
}

float CalculateVertexShadowAmount(float3 centerPos, float3 centreToVtx, float effectShadowAmount, bool isShadow, bool bHighQuality)
{
#if (__WIN32PC && (__SHADERMODEL == 30 ||__LOW_QUALITY))
	return 1.0;
#else
	if( isShadow )
	{
		return 1.0f;
	}
	else
	{
		//take x samplers along the diagonal of the particle, we could maybe improve this with a different sample pattern.
		float numSamples = bHighQuality?16.0f:4.0f;
		float3 offset = centreToVtx / numSamples;
		float3 pos = centerPos;				
		float shadowAmount = 0.0f;
		for(int i = 0 ; i < numSamples; i++)
		{
			shadowAmount += CalcCascadeShadowsFastNoFade(gViewInverse[3].xyz, pos, float3(0,0,1), float2(0,0));
			pos+= offset;
		}				
		shadowAmount/=numSamples;
		//store shadow amount in wldPos
		return lerp(1.0, shadowAmount, effectShadowAmount);
	}
#endif
}

#endif // __PTFX_COMMON_FXH__

