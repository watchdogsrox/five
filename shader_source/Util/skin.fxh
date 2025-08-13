// commonly used functions and defines related to skin rendering
// Include guards
#ifndef GTA_SKIN_FXH
#define GTA_SKIN_FXH

#include "../../../rage/base/src/shaderlib/rage_skin.fxh"

struct northSkinVertexInput {
	// This covers the default rage skinned vertex
    float3 pos			: POSITION0;
	float4 weight		: BLENDWEIGHT;
	index4 blendindices	: BLENDINDICES;
	float2 texCoord0	: TEXCOORD0;
#if PED_DAMAGE_TARGETS 
	float2 texCoord1	: TEXCOORD1;
#endif
    float3 normal		: NORMAL;
	float4 diffuse		: COLOR0;
#if PALETTE_TINT_EDGE
	float4 specular		: COLOR1;
#endif
};

struct northSkinVertexInputBump {
	// This covers the default rage skinned vertex
    float3 pos            : POSITION0;
	float4 weight		  : BLENDWEIGHT;
	index4 blendindices	  : BLENDINDICES;
	float2 texCoord0	  : TEXCOORD0;

#if PED_HAIR_DIFFUSE_MASK
	float2 texCoord1	  : TEXCOORD1;
#endif
#if PED_DAMAGE_TARGETS  || UMOVEMENTS_TEX
	float2 texCoord1	  : TEXCOORD1;
#endif
#if defined(USE_SPECULAR_UV1)  || EDGE_WEIGHT
    float2 texCoord1      : TEXCOORD1;
#endif
	float3 normal		  : NORMAL0;
#if defined(USE_NORMAL_MAP)
#if __MAX
	float4 tangent		: TANGENT;
	float3 binormal		: BINORMAL;
#else // __MAX
	float4 tangent		: TANGENT0;
#endif // __MAX
#endif
	float4 diffuse		  : COLOR0;
#if PED_CPV_WIND || UMOVEMENTS || PALETTE_TINT_EDGE || PEDSHADER_FAT || PEDSHADER_FUR
	float4 specular		  : COLOR1;
#endif
#if MTX_IN_VB
	float4 skinx : POSITION1;
	float4 skiny : POSITION2;
	float4 skinz : POSITION3;
#endif
#if RAGE_INSTANCED_TECH
	uint InstID : SV_InstanceID;
#endif
};


#endif // GTA_SKIN_FXH


