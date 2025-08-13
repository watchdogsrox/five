//
// commonly used functions and defines
// Allows to share features on a per option basis, so as not to include extra samplers or constants 
// in your shaders.
//
// - ENABLE_TEX2DOFFSET for tex2DOffset and TexelSize Constant.
// - ENABLE_LAZYHALF for FLOAT<,2,4,...> define to half
//
//

#ifndef __SHADER_UTILS_FXH
#define __SHADER_UTILS_FXH

#ifndef ENABLE_TEX2DOFFSET
#define ENABLE_TEX2DOFFSET 0
#endif // ENABLE_TEX2DOFFSET

#ifndef ENABLE_LAZYHALF
#define ENABLE_LAZYHALF 0
#endif // ENABLE_TEX2DOFFSET

#if ENABLE_LAZYHALF
#define FLOAT half
#define FLOAT2 half2
#define FLOAT3 half3
#define FLOAT4 half4
#define FLOAT3x3 half3x3
#define FLOAT4x4 half4x4
#else
#define FLOAT float
#define FLOAT2 float2
#define FLOAT3 float3
#define FLOAT4 float4
#define FLOAT3x3 float3x3
#define FLOAT4x4 float4x4
#endif

#if ENABLE_TEX2DOFFSET

#if __PS3 || __SHADERMODEL <= 30
float4 TexelSize;
#endif // PS3 || __SHADERMODEL <= 30


float4 tex2DOffset(sampler2D ss, float2 uv, float2 offset)
{
    float4 result;
#if __XENON
    float offsetX = offset.x;
    float offsetY = offset.y;
    asm {
        tfetch2D result, uv, ss, OffsetX=offsetX, OffsetY=offsetY
    };
#else
	result = h4tex2D( ss, uv + TexelSize.xy * offset );
#endif 
    return result;
}
#endif // ENABLE_TEX2DOFFSET



#endif		// __SHADER_UTILS_FXH
