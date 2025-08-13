///////////////////////////////////////////////////////////////////////////////
//
//
//
//
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  PRAGMAS
///////////////////////////////////////////////////////////////////////////////
#ifndef PRAGMA_DCL
	#pragma dcl position normal tangent0 texcoord0
	#define PRAGMA_DCL
#endif


///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////

#define DECAL_SHADER
#define PROJTEX_SHADER
#define DECAL_USE_NORMAL_MAP_ALPHA
#define VFX_DECAL

#define USE_PROJTEX_ZSHIFT_SUPPORT			// use z-shifting (to avoid z-fighting)

#define USE_SPECULAR
#if defined(GTA_PARALLAX_SPECMAP_SHADER)
#	undef IGNORE_SPECULAR_MAP
#else
#	define IGNORE_SPECULAR_MAP
#endif
#define USE_NORMAL_MAP
#define USE_PARALLAX_MAP

#if !__PS3
#	define USE_VEHICLE_DAMAGE
#	define VEHICLE_DAMAGE_SCALE    gVehicleDamageScale
#	if __MAX
#		define NO_SKINNING
#		include "../../../rage/base/src/shaderlib/rage_common.fxh"
#		include "../Vehicles/vehicle_damage.fxh"
#	endif
	static float gVehicleDamageScale;
#endif


///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

#include "../Megashader/megashader.fxh"


///////////////////////////////////////////////////////////////////////////////
//  GLOBAL VARIABLES
///////////////////////////////////////////////////////////////////////////////

// These constants change once per bucket (ie, relatively infrequently)
BeginConstantBufferDX10( decal_per_bucket )

	// The input vertex positions are in S16 format, so they require a scale to
	// be applied to get them back into the correct range.
	float3 gPositionScale;

#	if __PS3
		// Scale value applied to the per vertex vehDamageOffset before being
		// added to the position.
		float gVehDamageOffsetScale;
#	endif

#	if defined(DECAL_ROTATE_TOWARDS_CAMERA)
		// Bucket space point for decal to rotate around.
		float3 gRotationCenter;
#	endif

EndConstantBufferDX10( decal_per_bucket )


// These constants change once per instance constants (ie, relatively frequently)
#define VFX_DECAL_IS_COMPILING_SHADER   1
#include "../../../rage/framework/src/vfx/decal/decalperinstcbuf.h"

#if RSG_DURANGO || RSG_ORBIS || __WIN32PC
#	define CBUF_REGISTER(N)  CBUF_REGISTER_(N)
#	define CBUF_REGISTER_(N) register (b##N)
#	if RSG_ORBIS
#		define cbuffer ConstantBuffer
#	endif
	passthrough
	{
		cbuffer decal_per_instance : CBUF_REGISTER(VFX_DECAL_PER_INST_CBUF_SLOT)
		{
			decalPerInstCBuf gPerInstCBuf[256];
		}
	}
#	undef CBUF_REGISTER_
#	undef CBUF_REGISTER
#	define gConstSet0       (gPerInstCBuf[IN.cbufIdx].constSet0)
#	define gVertexColor     (gPerInstCBuf[IN.cbufIdx].vertexColor)
#	define gTexcoordConst   (gPerInstCBuf[IN.cbufIdx].texcoordConst)
#else
	float4 gConstSet0;
	float4 gVertexColor;
	float4 gTexcoordConst;
#endif

// Two constants used for calculating per vertex alpha from the fbratio.
// Combined into a float2 for platforms that can use only one constant register
// per instruction (ie, RSX).
#define gAlphaConst     (gConstSet0.xy)


#define DOF_DECAL_ALPHA_LIMIT					(0.15)

///////////////////////////////////////////////////////////////////////////////
//  DECALS - Vertex Declarations
///////////////////////////////////////////////////////////////////////////////

struct vertexInputDecalFx
{
#	if RSG_DURANGO || RSG_PC || __MAX
		float3 pos                      : POSITION;
		float4 fbRatioAndVehDamageScale : TEXCOORD0;
		float2 texcoords                : TEXCOORD1;
		float3 normal                   : NORMAL;
		float4 tangent                  : TANGENT0;
		uint   cbufIdx                  : TEXCOORD2;
#	elif RSG_ORBIS
		float3 pos                      : POSITION;
		float2 fbRatioAndVehDamageScale : TEXCOORD0;
		float2 texcoords                : TEXCOORD1;
		float3 normal                   : NORMAL;
		float4 tangent                  : TANGENT0;
		uint   cbufIdx                  : TEXCOORD2;
#	elif RSG_PS3
		float3 pos                      : ATTR0;
		float2 fbRatioAndVehDamageScale : ATTR1;
		float2 texcoords                : ATTR2;
		float3 normal                   : ATTR3;
		float3 tangent                  : ATTR4;
		float3 vehDamageOffset          : ATTR5;
#	elif RSG_XENON
		float3 pos                      : POSITION;
		float4 fbRatioAndVehDamageScale : TEXCOORD0;
		float2 texcoords                : TEXCOORD1;
		float3 normal                   : NORMAL;
		float4 tangent                  : TANGENT0;
#	endif

#	if RAGE_INSTANCED_TECH
		uint   InstID                   : SV_InstanceID;
#	endif
};


///////////////////////////////////////////////////////////////////////////////
//  DECALS - Vertex decoding
//
//  These functions just decompress the input vertices to the standard
//  northVertexInput/northVertexInputBump structures used by the vertex
//  shaders in megashader.fxh.
//
///////////////////////////////////////////////////////////////////////////////

// Debug variable used to pass values to the pixel shader when
// VFX_DECAL_DEBUG_OUTPUT is enabled.
static float4 gVtxDebugOut;

northVertexInputBump DecodeInputVtxBump_Internal(vertexInputDecalFx IN)
{
#	if RSG_ORBIS
		// Out of all the platforms Orbis is the simplest here.  No weird
		// hackery required, because it has a nice set of vertex buffer types.
		// Hopefully these will be exposed on Durango sometime soon and it can
		// use this code path too.
		float fbRatio = IN.fbRatioAndVehDamageScale.x;
		gVehicleDamageScale = IN.fbRatioAndVehDamageScale.y;
#	elif __PS3
#		if 1
			// The bitangent flip bit, and the front to back ratio are stored in
			// the same byte on PS3.  The 7 least significant bits store the
			// front to back ratio, the most significant bit is the bitangent
			// flip.  The reason for this is that the RSX does not support a
			// 10:10:10:2 format.  Instead the tangent x, y and z components are
			// stored in 11:11:10.
			//
			// The RSX converts a U8 to a float by dividing by 255 and rounding
			// towards 0.  This rounding towards zero causes inaccuracy here and
			// needs to be carefully handled.  Multiplying by 255.000015 (1 ulp
			// greater than 255.0), gives the correct result for all values,
			// except 0xff which gets converted to 255.000015.  This error for
			// 0xff doesn't adversely affect the code so is acceptable.  See
			// https://ps3.scedev.net/forums/thread/229397.
			//
			float fbRatioAndBitangentFlip = IN.fbRatioAndVehDamageScale.x;
			float tmp = fbRatioAndBitangentFlip * (255.000015/128.0);
			float bitangentFlip = floor(tmp) * 2.0 - 1.0;
			float fbRatio = frac(tmp) * (128.0/127.0);
#		else
			// Unfortunately with the decompression above, the shader code for
			// PS3 doesn't correctly handle swapping between compressed and
			// uncompressed vertices correctly.  This code path can be instead
			// for debugging purposes if necissary (though generally it isn't
			// required).  Notice that when uncompressed vertices are used, the
			// bitangent flip value is not passed through to the vertex shader
			// at all for PS3.
			float fbRatio = IN.fbRatioAndVehDamageScale.x;
			float bitangentFlip = 1.0;
#		endif
#	else
		// Due the whacky vertex declarations being used (to save memory), the
		// first two bytes of IN.fbRatioAndVehDamageScale are aliased over the
		// top of IN.pos.w.  D3DDECLTYPE_UBYTE4N and DXGI_FORMAT_R8G8B8A8_UNORM
		// treat the vertex attribute as a u32 in native endian, with X in the
		// least significant byte, Y the next, etc.  So the ordering is
		// different depending on the platform endian.
#		if __XENON
			float fbRatio = IN.fbRatioAndVehDamageScale.y;
			gVehicleDamageScale = IN.fbRatioAndVehDamageScale.x;
#		else
			float fbRatio = IN.fbRatioAndVehDamageScale.z;
			gVehicleDamageScale = IN.fbRatioAndVehDamageScale.w;
#		endif
#	endif

	northVertexInputBump decoded;

#	if !__PS3
		decoded.pos         = IN.pos * gPositionScale;
#	else
		decoded.pos         = IN.pos * gPositionScale + IN.vehDamageOffset * gVehDamageOffsetScale;
#	endif

	decoded.diffuse.rgb = gVertexColor.rgb;
	decoded.diffuse.a   = gAlphaConst.x * fbRatio + gAlphaConst.y;

#	if __WIN32PC || RSG_DURANGO || RSG_ORBIS

		// DX11 has the input normals encoded as DXGI_FORMAT_R10G10B10A2_UNORM
		// (there is no _SNORM version available).  So the encoding done in
		// Vec::V4PackNormFloats_10_10_10_2 applies a scale and bias that we
		// need to undo here.
		decoded.normal      = IN.normal * 2.0 - 1.0;
#	else
		decoded.normal      = IN.normal;
#	endif

#	if __PS3
		// On PS3, the tangent w component (the bitangent flip bit) is stored in
		// the sign bit of IN.texcoordsAndFBRatio.
		decoded.tangent.xyz = IN.tangent;
		decoded.tangent.w   = bitangentFlip;
#	elif (RSG_ORBIS && 0)
		// For Orbis, the X, Y and Z components of the tangent are SNorm, and
		// can therefore be used as is, but the W component is UNorm.
		// V4PackNormFloats_10_10_10_2 encodes -1 as 0, and +1 as +1.
		decoded.tangent.xyz = IN.tangent.xyz;
		decoded.tangent.w   = IN.tangent.w * 2.0 - 1.0;
#	elif __WIN32PC || RSG_DURANGO || RSG_ORBIS
		// For DX11 the entire tangent is UNorm and needs a bias and scale
		// applied.
		decoded.tangent     = IN.tangent * 2.0 - 1.0;
#	else
		decoded.tangent     = IN.tangent;
#	endif

	// The texcoords can be set to auto rotate towards the camera.  Note that
	// doing this in the vertex shader only works when the full texture is being
	// used (ie, not part of an atlas), and the sampler is set to clamp.  To
	// work in a pixel shader, we would need to manually clamp in there (the
	// unclamped texcoords are required for interpolation).
	float2 inputTexcoords = IN.texcoords;
	float2 rotatedTexcoords;
#	if defined(DECAL_ROTATE_TOWARDS_CAMERA)

		// Calculate the rotation amount so that the base of the texture is
		// towards the camera (view space origin).
		//
		// The model space rotation center is passed in as a vertex constant.
		// First we get the angle between the vector to the camera from the
		// rotation center, and the tangent.  Then rotate the texcoords so that
		// they align.
		//
		// Don't use the vertex normal as the rotation axis here, as it results
		// in weird tearing and stretching since different parts of the decal
		// end up rotating different amounts.  Instead hardcode to use model
		// space up.
		//
		// Note also that the inverse transpose of gWorldView is not required
		// here, since gWorldView is an affine transform.
		//
		float3 normalVS    = mul(float4(0,0,1,0), gWorldView).xyz;
		float3 rotCenterVS = mul(float4(gRotationCenter, 1), gWorldView).xyz;
		float3 rotCenterToCamVS = normalize(-rotCenterVS);
		float3 rotCenterToCamTangentPlaneVS = normalize(rotCenterToCamVS - normalVS * dot(rotCenterToCamVS, normalVS));

		// This is a bit of a hack here, should really be re-orthonormalizing
		// the viewspeace tangent since we didn't use the vertex's normal.  But
		// honestly looks fine without it.
		float3 tangentVS = mul(float4(decoded.tangent.xyz, 0), gWorldView).xyz;
		//tangentVS = normalize(tangentVS - normalVS * dot(tangentVS, normalVS));

		// Extract the sin and cos of the angle between the two vectors.
		float cosTheta = dot(tangentVS, rotCenterToCamTangentPlaneVS);
		float sinTheta = dot(cross(tangentVS, rotCenterToCamTangentPlaneVS), normalVS);

		// Rotate the texcoords so that the tangent is lined up towards the
		// camera.  inputTexcoords are in the range [0..1] so we need shift the
		// rotation center to (0.5,0.5).  Actually inputTexcoords can
		// occasionally be outside of this range, but the center is still the
		// same.
		rotatedTexcoords.x = (inputTexcoords.x - 0.5)*cosTheta - (inputTexcoords.y - 0.5)*sinTheta + 0.5;
		rotatedTexcoords.y = (inputTexcoords.x - 0.5)*sinTheta + (inputTexcoords.y - 0.5)*cosTheta + 0.5;

		// Rotate an addition pi/2 since the tangent goes left to right across
		// the image, not top to bottom.
		rotatedTexcoords = float2(1-rotatedTexcoords.y, rotatedTexcoords.x);

#	else
		rotatedTexcoords = inputTexcoords;
#	endif

	// Transform the rotated texcoords into the atlas space.
	decoded.texCoord0 = gTexcoordConst.xy * rotatedTexcoords + gTexcoordConst.zw;

#	if RAGE_INSTANCED_TECH
		decoded.InstID = IN.InstID;
#	endif

	return decoded;
}


// DecodeInputVtxBump, but with the tangents discarded.
northVertexInput DecodeInputVtx_Internal(vertexInputDecalFx IN)
{
	northVertexInputBump decodedBump = DecodeInputVtxBump_Internal(IN);
	northVertexInput decoded;
	decoded.pos         = decodedBump.pos;
	decoded.diffuse     = decodedBump.diffuse;
	decoded.texCoord0   = decodedBump.texCoord0;
	decoded.normal      = decodedBump.normal;
	return decoded;
}

#if INSTANCED
#	error ERROR! Instancing is not supported yet for VFX_Decals!
#else
	northInstancedVertexInput DecodeInputVtx(vertexInputDecalFx IN)
	{
		northInstancedVertexInput OUT;
		OUT.baseVertIn = DecodeInputVtx_Internal(IN);
		return OUT;
	}

	northInstancedVertexInputBump DecodeInputVtxBump(vertexInputDecalFx IN)
	{
		northInstancedVertexInputBump OUT;
		OUT.baseVertIn = DecodeInputVtxBump_Internal(IN);
		return OUT;
	}
#endif


#if defined(USE_SEETHROUGH_TECHNIQUE)
vertexInputSeeThrough DecodeInputVtxSeeThrough(vertexInputDecalFx IN)
{
	northVertexInputBump decodedBump = DecodeInputVtxBump(IN);
	vertexInputSeeThrough decoded;
	decoded.pos = decodedBump.pos;
	return decoded;
}
#endif


///////////////////////////////////////////////////////////////////////////////
//  DECALS - Vertex Shader Output Structures
//
//  By default, these structures are simply wrappers around one of the vertex
//  output structures in the megashader.  But by enabling
//  VFX_DECAL_DEBUG_OUTPUT, additional debug fields are added.  See
//  VFX_DECAL_DEBUG_OUTPUT_VS_CODE() for the code that initializes these debug
//  fields.
//
///////////////////////////////////////////////////////////////////////////////

#define VFX_DECAL_DEBUG_OUTPUT  0

#if __PS3 || __XENON
	// PS3 & 360 don't allow arbitrary VS output binding semantics, so just
	// define this as something that is not already taken.
#	define VFX_DEBUG_SEMANTIC   COLOR1
#endif

#if VFX_DECAL_DEBUG_OUTPUT
#	define VFX_DECAL_DEBUG_OUTPUT_STRUCT(BASE_NAME)                            \
		struct Vfx_Decal_##BASE_NAME                                           \
		{                                                                      \
			BASE_NAME BASE;                                                    \
			float4 DBG : VFX_DEBUG_SEMANTIC;                                   \
		}

#if APPLY_DOF_TO_ALPHA_DECALS
#	define VFX_DECAL_DEBUG_OUTPUT_STRUCT_DOF(BASE_NAME)                        \
		struct Vfx_Decal_DOF_##BASE_NAME                                         \
		{                                                                      \
			BASE_NAME BASE;                                                    \
			float4 DBG : VFX_DEBUG_SEMANTIC;                                   \
			float linearDepth;	: TEXCOORD10									\
		}
#endif //APPLY_DOF_TO_ALPHA_DECALS

#else
#	define VFX_DECAL_DEBUG_OUTPUT_STRUCT(BASE_NAME)                            \
		struct Vfx_Decal_##BASE_NAME                                           \
		{                                                                      \
			BASE_NAME BASE;                                                    \
		}

#if APPLY_DOF_TO_ALPHA_DECALS
#	define VFX_DECAL_DEBUG_OUTPUT_STRUCT_DOF(BASE_NAME)                        \
		struct Vfx_Decal_DOF_##BASE_NAME                                        \
		{                                                                      \
			BASE_NAME BASE;                                                    \
			float linearDepth		: TEXCOORD10;								\
		}
#endif //APPLY_DOF_TO_ALPHA_DECALS

#endif

VFX_DECAL_DEBUG_OUTPUT_STRUCT(vertexMegaOutputCube);
VFX_DECAL_DEBUG_OUTPUT_STRUCT(vertexOutput);
VFX_DECAL_DEBUG_OUTPUT_STRUCT(vertexOutputD);
VFX_DECAL_DEBUG_OUTPUT_STRUCT(vertexOutputMaxRageViewer);
VFX_DECAL_DEBUG_OUTPUT_STRUCT(vertexOutputSeeThrough);
VFX_DECAL_DEBUG_OUTPUT_STRUCT(vertexOutputUnlit);
VFX_DECAL_DEBUG_OUTPUT_STRUCT(vertexOutputWaterReflection);

#if APPLY_DOF_TO_ALPHA_DECALS
VFX_DECAL_DEBUG_OUTPUT_STRUCT_DOF(vertexOutput);
#endif //APPLY_DOF_TO_ALPHA_DECALS


#if defined(USE_SEETHROUGH_TECHNIQUE)
VFX_DECAL_DEBUG_OUTPUT_STRUCT(vertexOutputSeeThrough);
#endif

#undef VFX_DECAL_DEBUG_OUTPUT_STRUCT


///////////////////////////////////////////////////////////////////////////////
//  DECALS - Vertex Shader
//
//  These vertex shaders by default will decompress the input vertex attributes
//  into a format usable by the megashader code they are wrapping.  When
//  VFX_DECAL_DEBUG_OUTPUT is enabled, the vertex shaders also add the debug
//  code in VFX_DECAL_DEBUG_OUTPUT_VS_CODE() to pass values through to the pixel
//  shader.
//
//  The code in VFX_DECAL_DEBUG_OUTPUT_VS_CODE() can be changed to output
//  whatever you are trying to debug.
//
///////////////////////////////////////////////////////////////////////////////

#if VFX_DECAL_DEBUG_OUTPUT
#	define VFX_DECAL_DEBUG_OUTPUT_VS_CODE()                                    \
		OUT.DBG = gVtxDebugOut
#else
	// While defining VFX_DECAL_DEBUG_OUTPUT_VS_CODE() to something like
	// do{}while(0) would generally be considerred better form here (to eat the
	// semicolon), that causes a compiler on Durango.
#	define VFX_DECAL_DEBUG_OUTPUT_VS_CODE()
#endif

#ifdef RAGE_ENABLE_OFFSET
#	define OFFSET_ARGS_SIGNATURE_OFFSET     , rageVertexOffsetBump INOFFSET, uniform float bEnableOffsets
#	define OFFSET_ARGS_PASS_OFFSET          , INOFFSET, bEnableOffsets
#else
#	define OFFSET_ARGS_SIGNATURE_OFFSET
#	define OFFSET_ARGS_PASS_OFFSET
#endif
#define OFFSET_ARGS_SIGNATURE_NO_OFFSET
#define OFFSET_ARGS_PASS_NO_OFFSET

#define BUMP(SYMBOL)        SYMBOL##Bump
#define NO_BUMP(SYMBOL)     SYMBOL

#define VFX_DECAL_DEBUG_OUTPUT_VS_WRAPPER(OUT_STRUCT, NAME, HAS_BUMP, HAS_OFFSET) \
	Vfx_Decal_##OUT_STRUCT VS_Vfx_Decal_##NAME(vertexInputDecalFx IN OFFSET_ARGS_SIGNATURE_##HAS_OFFSET) \
	{																					\
		Vfx_Decal_##OUT_STRUCT OUT;														\
		HAS_BUMP(northInstancedVertexInput) decoded = HAS_BUMP(DecodeInputVtx)(IN);		\
		OUT.BASE = VS_##NAME(decoded OFFSET_ARGS_PASS_##HAS_OFFSET);					\
		VFX_DECAL_DEBUG_OUTPUT_VS_CODE();												\
		return OUT;																		\
	}

#if APPLY_DOF_TO_ALPHA_DECALS
#define VFX_DECAL_DEBUG_OUTPUT_VS_WRAPPER_DOF(OUT_STRUCT, NAME, HAS_BUMP, HAS_OFFSET) \
	Vfx_Decal_DOF_##OUT_STRUCT VS_Vfx_Decal_DOF_##NAME(vertexInputDecalFx IN OFFSET_ARGS_SIGNATURE_##HAS_OFFSET) \
	{																					\
		Vfx_Decal_DOF_##OUT_STRUCT OUT;														\
		HAS_BUMP(northInstancedVertexInput) decoded = HAS_BUMP(DecodeInputVtx)(IN);		\
		OUT.BASE = VS_##NAME(decoded OFFSET_ARGS_PASS_##HAS_OFFSET);					\
		VFX_DECAL_DEBUG_OUTPUT_VS_CODE();												\
		OUT.linearDepth = OUT.BASE.pos.w;												\
		return OUT;																		\
	}
#endif //APPLY_DOF_TO_ALPHA_DECALS

VFX_DECAL_DEBUG_OUTPUT_VS_WRAPPER(vertexOutput,                 Transform,                  BUMP,       OFFSET)
VFX_DECAL_DEBUG_OUTPUT_VS_WRAPPER(vertexOutputD,                TransformD,                 BUMP,       OFFSET)
VFX_DECAL_DEBUG_OUTPUT_VS_WRAPPER(vertexOutputUnlit,            TransformUnlit,             NO_BUMP,    OFFSET)
VFX_DECAL_DEBUG_OUTPUT_VS_WRAPPER(vertexOutputWaterReflection,  TransformWaterReflection,   BUMP,       NO_OFFSET)

#if CUBEMAP_REFLECTION_TECHNIQUES
VFX_DECAL_DEBUG_OUTPUT_VS_WRAPPER(vertexMegaOutputCube,         TransformCube,              BUMP,       NO_OFFSET)
#endif

#if APPLY_DOF_TO_ALPHA_DECALS
VFX_DECAL_DEBUG_OUTPUT_VS_WRAPPER_DOF(vertexOutput,                 Transform,                  BUMP,       OFFSET)
#endif //APPLY_DOF_TO_ALPHA_DECALS

#undef NO_BUMP
#undef BUMP
#undef OFFSET_ARGS_PASS_NO_OFFSET
#undef OFFSET_ARGS_SIGNATURE_NO_OFFSET
#undef OFFSET_ARGS_PASS_OFFSET
#undef OFFSET_ARGS_SIGNATURE_OFFSET

#undef VFX_DECAL_DEBUG_OUTPUT_VS_WRAPPER

#if defined(USE_SEETHROUGH_TECHNIQUE)
Vfx_Decal_vertexOutputSeeThrough VS_Vfx_Decal_Transform_SeeThrough(vertexInputDecalFx IN)
{
	Vfx_Decal_vertexOutputSeeThrough OUT;
	vertexInputSeeThrough decoded = DecodeInputVtxSeeThrough(IN);
	OUT.BASE = VS_Transform_SeeThrough(decoded);
	VFX_DECAL_DEBUG_OUTPUT_VS_CODE();
	return OUT;
}
#endif


///////////////////////////////////////////////////////////////////////////////
//  DECALS - Pixel Shader
//
//  These pixel shader wrapper functions will generally not do anything
//  different than the megashader function they are wrapping.  But when
//  VFX_DECAL_DEBUG_OUTPUT is enabled, the output color is overriden by the
//  value passed through by the vertex shader code
//  VFX_DECAL_DEBUG_OUTPUT_VS_CODE().
//
///////////////////////////////////////////////////////////////////////////////

#if VFX_DECAL_DEBUG_OUTPUT
#	if 1
		// Pass through the debug output from the vertex shader
#		define VFX_DECAL_DEBUG_OUTPUT_PS_CODE()                                \
			OUT_COLOR = IN.DBG
#	elif 0
		// Blend the debug output from the vertex shader with the standard
		// output from the pixel shader
#		define VFX_DECAL_DEBUG_OUTPUT_PS_CODE()                                \
			OUT_COLOR.rgb *= 0.25f;                                            \
			OUT_COLOR.rgb += IN.DBG.rgb * 0.75;                                \
			OUT_COLOR.a = 1.f
#	else
		// Just output solid red
#		define VFX_DECAL_DEBUG_OUTPUT_PS_CODE()                                \
			OUT_COLOR = float4(1.f, 0.f, 0.f, 1.f)
#	endif
#else
	// While defining VFX_DECAL_DEBUG_OUTPUT_PS_CODE() to something like
	// do{}while(0) would generally be considerred better form here (to eat the
	// semicolon), that causes a compiler on Durango.
#	define VFX_DECAL_DEBUG_OUTPUT_PS_CODE()
#endif


// Pixel shaders that output a struct (eg, gbuffer shaders), should use this
// macro to create the appropriate wrapper function.
#define VFX_DECAL_DEBUG_OUTPUT_PS_WRAPPER_STRUCT(OUT_STRUCT, NAME, IN_STRUCT)  \
	OUT_STRUCT PS_Vfx_Decal_##NAME(Vfx_Decal_##IN_STRUCT IN)                   \
	{                                                                          \
		OUT_STRUCT OUT = PS_##NAME(IN.BASE);                                   \
		float4 OUT_COLOR = OUT.col0;                                           \
		VFX_DECAL_DEBUG_OUTPUT_PS_CODE();                                      \
		OUT.col0 = OUT_COLOR;                                                  \
		return OUT;                                                            \
	}


#if APPLY_DOF_TO_ALPHA_DECALS
// Pixel shaders that output a struct (eg, gbuffer shaders), should use this
// macro to create the appropriate wrapper function.
#define VFX_DECAL_DEBUG_OUTPUT_PS_WRAPPER_STRUCT_DOF(OUT_STRUCT, NAME, IN_STRUCT)  \
	OUT_STRUCT PS_Vfx_Decal_DOF_##NAME(Vfx_Decal_DOF_##IN_STRUCT IN)                   \
	{                                                                          \
		OUT_STRUCT OUT;												\
		OUT.col0 = PS_##NAME(IN.BASE).col0;											\
		float4 OUT_COLOR = OUT.col0;                                           \
		VFX_DECAL_DEBUG_OUTPUT_PS_CODE();                                      \
		OUT.col0 = OUT_COLOR;													\
		float dofOutput = OUT.col0.a > DOF_DECAL_ALPHA_LIMIT ? 1.0 : 0.0;		\
		OUT.dof.depth = IN.linearDepth * dofOutput;								\
		OUT.dof.alpha = dofOutput;												\
		return OUT;                                                            \
	}
#endif //APPLY_DOF_TO_ALPHA_DECALS

VFX_DECAL_DEBUG_OUTPUT_PS_WRAPPER_STRUCT(DeferredGBuffer, DeferredTextured,                 vertexOutputD)
VFX_DECAL_DEBUG_OUTPUT_PS_WRAPPER_STRUCT(DeferredGBuffer, DeferredTextured_AlphaClip,       vertexOutputD)
VFX_DECAL_DEBUG_OUTPUT_PS_WRAPPER_STRUCT(DeferredGBuffer, DeferredTextured_SubSampleAlpha,  vertexOutputD)

VFX_DECAL_DEBUG_OUTPUT_PS_WRAPPER_STRUCT(OutHalf4Color,   TexturedUnlit,                    vertexOutputUnlit)
VFX_DECAL_DEBUG_OUTPUT_PS_WRAPPER_STRUCT(OutHalf4Color,   TexturedWaterReflection,          vertexOutputWaterReflection)
VFX_DECAL_DEBUG_OUTPUT_PS_WRAPPER_STRUCT(OutHalf4Color,   Textured_Eight,                   vertexOutput)
VFX_DECAL_DEBUG_OUTPUT_PS_WRAPPER_STRUCT(OutHalf4Color,   Textured_EightHighQuality,        vertexOutput)
VFX_DECAL_DEBUG_OUTPUT_PS_WRAPPER_STRUCT(OutHalf4Color,   Textured_Four,                    vertexOutput)
VFX_DECAL_DEBUG_OUTPUT_PS_WRAPPER_STRUCT(OutHalf4Color,   Textured_FourHighQuality,         vertexOutput)
VFX_DECAL_DEBUG_OUTPUT_PS_WRAPPER_STRUCT(OutHalf4Color,   Textured_Zero,			         vertexOutput)
VFX_DECAL_DEBUG_OUTPUT_PS_WRAPPER_STRUCT(OutHalf4Color,   Textured_ZeroHighQuality,         vertexOutput)
VFX_DECAL_DEBUG_OUTPUT_PS_WRAPPER_STRUCT(OutHalf4Target0, DeferredTextured_AlphaClip2,      vertexOutputD)

#if APPLY_DOF_TO_ALPHA_DECALS
VFX_DECAL_DEBUG_OUTPUT_PS_WRAPPER_STRUCT_DOF(OutHalf4Color_DOF, Textured_Zero,            vertexOutput)
VFX_DECAL_DEBUG_OUTPUT_PS_WRAPPER_STRUCT_DOF(OutHalf4Color_DOF, Textured_Four,            vertexOutput)
VFX_DECAL_DEBUG_OUTPUT_PS_WRAPPER_STRUCT_DOF(OutHalf4Color_DOF, Textured_Eight,            vertexOutput)
#endif //APPLY_DOF_TO_ALPHA_DECALS

#if defined(USE_SEETHROUGH_TECHNIQUE)
VFX_DECAL_DEBUG_OUTPUT_PS_WRAPPER_STRUCT(OutFloat4Color,  SeeThrough,                       vertexOutputSeeThrough)
#endif
#if CUBEMAP_REFLECTION_TECHNIQUES
VFX_DECAL_DEBUG_OUTPUT_PS_WRAPPER_STRUCT(OutHalf4Color,   TexturedCube,                     vertexMegaOutputCube)
#endif
#if __WIN32PC_MAX_RAGEVIEWER
VFX_DECAL_DEBUG_OUTPUT_PS_WRAPPER_STRUCT(OutHalf4Color,   TexturedMaxRageViewer,            vertexOutputMaxRageViewer)
#endif

#undef VFX_DECAL_DEBUG_OUTPUT_PS_WRAPPER_STRUCT


// Misc pixel shaders that don't fit into the standard pattern (extra inputs, etc).

#if __PS3 && defined(USE_MANUALDEPTH_TECHNIQUE)
half PS_Vfx_Decal_TexturedUnlit_MD(Vfx_Decal_vertexOutputUnlit IN, float4 vPos : VPOS) : COLOR
{
	half OUT_COLOR = PS_TexturedUnlit_MD(IN.BASE, vPos);
	VFX_DECAL_DEBUG_OUTPUT_PS_CODE();
	return OUT_COLOR;
}
#endif


///////////////////////////////////////////////////////////////////////////////
//  TECHNIQUES
///////////////////////////////////////////////////////////////////////////////

#if EMISSIVE_ADDITIVE
	#define FORWARD_RENDERSTATES() \
		AlphaBlendEnable	= true; \
		BlendOp				= ADD; \
		SrcBlend			= ONE; \
		DestBlend			= ONE; \
		ColorWriteEnable	= RED+GREEN+BLUE; \
		ZWriteEnable		= false; \
		ZFunc				= FixedupLESS;
#else
	#define FORWARD_RENDERSTATES()
#endif

	#if 0
		#define CGC_FORWARD_LIGHTING_FLAGS "-unroll all --O3 -fastmath"
	#else
		#define CGC_FORWARD_LIGHTING_FLAGS CGC_DEFAULTFLAGS
	#endif

	// ===============================
	// Lit (forward) techniques
	// ===============================
	#if FORWARD_TECHNIQUES
	technique draw
	{
		pass p0
		{
			FORWARD_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_Vfx_Decal_Transform(gbOffsetEnable);
		#if __WIN32PC_MAX_RAGEVIEWER
			PixelShader  = compile PIXELSHADER	PS_Vfx_Decal_TexturedMaxRageViewer()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		#else
			PixelShader  = compile PIXELSHADER	PS_Vfx_Decal_Textured_Eight()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
		#endif
		}
	}
	#endif // FORWARD_TECHNIQUES

	#if FORWARD_TECHNIQUES
	technique lightweight0_draw
	{
		pass p0
		{
			FORWARD_RENDERSTATES()
#if APPLY_DOF_TO_ALPHA_DECALS
			VertexShader = compile VERTEXSHADER	VS_Vfx_Decal_DOF_Transform(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_Vfx_Decal_DOF_Textured_Zero()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
#else
			VertexShader = compile VERTEXSHADER	VS_Vfx_Decal_Transform(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_Vfx_Decal_Textured_Zero()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
#endif //APPLY_DOF_TO_ALPHA_DECALS
		}
	}
	#endif // FORWARD_TECHNIQUES

	#if FORWARD_TECHNIQUES
	technique lightweight4_draw
	{
		pass p0
		{
			FORWARD_RENDERSTATES()
#if APPLY_DOF_TO_ALPHA_DECALS
			VertexShader = compile VERTEXSHADER	VS_Vfx_Decal_DOF_Transform(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_Vfx_Decal_DOF_Textured_Four()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
#else
			VertexShader = compile VERTEXSHADER	VS_Vfx_Decal_Transform(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_Vfx_Decal_Textured_Four()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
#endif
		}
	}
	#endif // FORWARD_TECHNIQUES

	#if FORWARD_TECHNIQUES
	technique lightweight8_draw
	{
		pass p0
		{
			FORWARD_RENDERSTATES()
#if APPLY_DOF_TO_ALPHA_DECALS
			VertexShader = compile VERTEXSHADER	VS_Vfx_Decal_DOF_Transform(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_Vfx_Decal_DOF_Textured_Eight()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
#else
			VertexShader = compile VERTEXSHADER	VS_Vfx_Decal_Transform(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_Vfx_Decal_Textured_Eight()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
#endif //APPLY_DOF_TO_ALPHA_DECALS

		}
	}
	#endif // FORWARD_TECHNIQUES

	#if HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED
	technique lightweightHighQuality0_draw
	{
		pass p0
		{
			FORWARD_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_Vfx_Decal_Transform(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_Vfx_Decal_Textured_ZeroHighQuality()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
		}
	}
	#endif // HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED

	#if HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED
	technique lightweightHighQuality4_draw
	{
		pass p0
		{
			FORWARD_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_Vfx_Decal_Transform(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_Vfx_Decal_Textured_FourHighQuality()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
		}
	}
	#endif // HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED

	#if HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED
	technique lightweightHighQuality8_draw
	{
		pass p0
		{
			FORWARD_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_Vfx_Decal_Transform(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_Vfx_Decal_Textured_EightHighQuality()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
		}
	}
	#endif // HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED

	// ===============================
	// Unlit techniques
	// ===============================
	#if UNLIT_TECHNIQUES
	technique unlit_draw
	{
		pass p0
		{
			FORWARD_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_Vfx_Decal_TransformUnlit(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_Vfx_Decal_TexturedUnlit()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif // UNLIT_TECHNIQUES

	// ===============================
	// Deferred techniques
	// ===============================
	#if DEFERRED_TECHNIQUES && NON_CUTOUT_DEFERRED_TECHNIQUES
	technique deferred_draw
	{
		pass p0
		{
			#include "../Megashader/megashader_deferredRS.fxh"
			VertexShader = compile VERTEXSHADER	VS_Vfx_Decal_TransformD(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_Vfx_Decal_DeferredTextured()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif // DEFERRED_TECHNIQUES && NON_CUTOUT_DEFERRED_TECHNIQUES

	#if DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES
	technique deferredalphaclip_draw
	{
		pass p0
		{
			#include "../Megashader/megashader_deferredRS.fxh"
			VertexShader = compile VERTEXSHADER	VS_Vfx_Decal_TransformD(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_Vfx_Decal_DeferredTextured_AlphaClip()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif // DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES

	#if DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES
	technique deferredsubsamplewritealpha_draw
	{
		// preliminary pass to write down alpha
		pass p0
		{
			DEFERRED_SUBSAMPLEWRITEALPHA_RENDERSTATES()
			VertexShader = compile VERTEXSHADER VS_Vfx_Decal_TransformD(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER  PS_Vfx_Decal_DeferredTextured_AlphaClip2()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif // DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES

	#if DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES
	technique deferredsubsamplealpha_draw
	{
		// preliminary pass to write down alpha
		pass p0
		{
			VertexShader = compile VERTEXSHADER VS_Vfx_Decal_TransformD(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER  PS_Vfx_Decal_DeferredTextured_SubSampleAlpha()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif // DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES

	// ===============================
	// Water reflection techniques
	// ===============================
	#if WATER_REFLECTION_TECHNIQUES
	technique waterreflection_draw
	{
		pass p0
		{
			FORWARD_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_Vfx_Decal_TransformWaterReflection();
			PixelShader  = compile PIXELSHADER	PS_Vfx_Decal_TexturedWaterReflection()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}

	technique waterreflectionalphaclip_draw
	{
		pass p0
		{
			FORWARD_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_Vfx_Decal_TransformWaterReflection();
			PixelShader  = compile PIXELSHADER	PS_Vfx_Decal_TexturedWaterReflection()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif // WATER_REFLECTION_TECHNIQUES

	// ================================
	// Cubemap reflection techniques
	// ================================
	#if CUBEMAP_REFLECTION_TECHNIQUES
	technique cube_draw
	{
		pass p0
		{
			CullMode=CCW;
			VertexShader = compile VERTEXSHADER	VS_Vfx_Decal_TransformCube();
			PixelShader  = compile PIXELSHADER	PS_Vfx_Decal_TexturedCube()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}

	#if GS_INSTANCED_CUBEMAP
	#define TYPE				Mega
	#define VS_OUT_TYPE			vertexMegaOutputCube
	#define VS_INPUT_DECODE		DecodeInputVtxBump
	#define GSINST_VSTYPE		vertexInputDecalFx
	#define PS_CUBE_FUNC		PS_Vfx_Decal_TexturedCube

	GEN_GSINST_TYPE(VfxDecal,vertexMegaOutputCube,SV_RenderTargetArrayIndex)
	GEN_GSINST_FUNC(VfxDecal,VfxDecal,vertexInputDecalFx,vertexMegaOutputCube,vertexMegaOutputCube,VS_TransformCube_Common(DecodeInputVtxBump(IN),true),PS_TexturedCube(IN))
	technique cubeinst_draw
	{
		GEN_GSINST_TECHPASS(VfxDecal,all)
	}
	#endif

	#endif // CUBEMAP_REFLECTION_TECHNIQUES

	// ===============================
	// Manualdepth techniques
	// ===============================
	#if __PS3 && defined(USE_MANUALDEPTH_TECHNIQUE)
	technique manualdepth_draw
	{
		pass p0
		{
			ZWriteEnable = false;
			ZEnable      = false;
			VertexShader = compile VERTEXSHADER	VS_Vfx_Decal_Transform(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_Vfx_Decal_TexturedUnlit_MD()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif // __PS3 && defined(USE_MANUALDEPTH_TECHNIQUE)

	// ===============================
	// Seethrough techniques
	// ===============================
	#if defined(USE_SEETHROUGH_TECHNIQUE)
	technique seethrough_draw
	{
		pass p0
		{
			ZwriteEnable = true;
			VertexShader = compile VERTEXSHADER	VS_Vfx_Decal_Transform_SeeThrough();
			PixelShader  = compile PIXELSHADER	PS_Vfx_Decal_SeeThrough()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif // defined(USE_SEETHROUGH_TECHNIQUE)
