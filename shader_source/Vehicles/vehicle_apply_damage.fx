//
// vehicle_apply_damage.fx shader;
//
// 
//
// 10/10/2013 - Bela Kampis: - initial;
//
//

#define USE_SPECULAR
#define USE_VEHICLE_DAMAGE

#include "vehicle_common.fxh"
#include "vehicle_common_values.h"

#if GPU_DAMAGE_WRITE_ENABLED

BeginConstantBufferDX10(apply_damage_shadervars)
float4 GeneralParams0 = float4(1.0, 1.0, 1.0, 0.0);
float4 GeneralParams1 = float4(0.0, 0.0, 0.0, 0.0);
EndConstantBufferDX10(apply_damage_shadervars)

BeginSampler(sampler2D, ApplyDamageTexture, ApplyDamageSampler, ApplyDamageTexture)
string UIName      = "ApplyDamageTexture";
string TCPTemplate = "Diffuse";
string TextureType = "Diffuse";
	
ContinueSampler(sampler2D, ApplyDamageTexture, ApplyDamageSampler, ApplyDamageTexture)
   MinFilter = LINEAR;
   MagFilter = LINEAR;
   MipFilter = NONE;
   AddressU  = WRAP;
   AddressV  = WRAP;
EndSampler;

struct rageVertexApplyDamageIn 
{
	float3 pos			: POSITION;
	float2 texCoord0	: TEXCOORD0;
};

struct rageVertexApplyDamage 
{
	DECLARE_POSITION(pos)
	float2 texCoord0	: TEXCOORD0;
};

rageVertexApplyDamage VS_ApplyDamage(rageVertexApplyDamageIn IN)
{
	rageVertexApplyDamage OUT;
	OUT.pos = float4(IN.pos.xyz, 1.0f);
	OUT.texCoord0 = IN.texCoord0;
	return(OUT);
}

half4 PS_ApplyDamage(rageVertexApplyDamage IN) : COLOR
{
	float damageRadius = GeneralParams0.w;
	float3 damageColor = GeneralParams0.xyz;

	float2 uv = IN.texCoord0;
	float2 center = GeneralParams1.xy;
	float2 noiseUV = fmod( (uv * 4.0f) + center, 1.0f); // Center the noise map on the circle
	
	float noiseMultiplier = tex2D(ApplyDamageSampler, noiseUV).r;
	float distanceFromCenter = length(uv - center);
	float intensity = 1.0 - clamp(distanceFromCenter / damageRadius, 0.0, 1.0);
	intensity *= noiseMultiplier;
	
#if VEHICLE_DEFORMATION_INVERSE_SQUARE_FIELD
	intensity *= intensity;
#endif

	float3 newColor = damageColor * intensity;
	return half4(newColor.x, newColor.y, newColor.z, 1.0f);
}

technique ApplyDamage
{
	pass pApplyDamage
	{
		VertexShader = compile VERTEXSHADER	VS_ApplyDamage();
		PixelShader  = compile PIXELSHADER	PS_ApplyDamage()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}

half4 PS_DamageCopy(rageVertexApplyDamage IN) : COLOR
{
	float2 uv = IN.texCoord0;
	
	float3 damageColor = tex2D(ApplyDamageSampler, uv).rgb;
	return half4(damageColor.x, damageColor.y, damageColor.z, 1.0f);
}


technique DamageCopy
{
	pass pDamageCopy
	{
		VertexShader = compile VERTEXSHADER	VS_ApplyDamage();
		PixelShader  = compile PIXELSHADER	PS_DamageCopy()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}

#else

technique ApplyDamage
{
	pass DummyPass
	{
	}
}

#endif //GPU_DAMAGE_WRITE_ENABLED

