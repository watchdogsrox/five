//
// GTA terrain Combo shader (up to 8 layers using a lookup texture, multitexture):
//
//
//
#ifndef __GTA_SEETHROUGHFX_FXH__
#define __GTA_SEETHROUGHFX_FXH__

#include "../common.fxh"

struct vertexInputSkinSeeThrough {
    float3 pos          : POSITION0;
	float4 weight		: BLENDWEIGHT;
	index4 blendindices	: BLENDINDICES;
};

struct vertexInputSeeThrough {
    float3 pos          : POSITION0;
};

struct vertexOutputSeeThrough
{
    DECLARE_POSITION(pos)
    float2 wpos			: TEXCOORD0;
};


float2 SeeThrough_Pack(float value)
{
	float depth = value * 65536.0f;

	// avoid overflow
	depth = min(depth, 8191.0f);

	float depthHi = floor(depth/256.0f);
	float depthLo = floor(depth - depthHi*256.0f);

	float2 result;
	
	result.x = depthHi/256.0f;
	result.y = depthLo/256.0f;
	
	return result;
}

float SeeThrough_UnPack(float2 value)
{
	//return (value.x*65536.0f + value.y*256.0f)/65536.0f;
	return (value.x*256.0f + value.y)/256.0f;
}

vertexOutputSeeThrough SeeThrough_GenerateOutput(float3 inPos )
{
	vertexOutputSeeThrough OUT;
	
    // Write out final position & texture coords
    OUT.pos =  mul(float4(inPos,1), gWorldViewProj);
	OUT.wpos.x = saturate(OUT.pos.w/gFadeEndDistance);
	
	OUT.wpos.y = OUT.pos.w;
	
	return OUT;
}

float4 SeeThrough_Render(vertexOutputSeeThrough IN)
{
	float4 result;

	result.xy = SeeThrough_Pack(IN.wpos.x);
	
	float visibility = saturate( (gFadeEndDistance-IN.wpos.y)/(gFadeEndDistance-gFadeStartDistance) );
	
	result.z = visibility;
	result.w = 1.0f;
	
	return result;
}

#endif // __GTA_SEETHROUGHFX_FXH__
