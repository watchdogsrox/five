#ifndef PRAGMA_DCL
	#pragma dcl position diffuse texcoord0 normal
	#define PRAGMA_DCL
#endif

#include "../commonMS.fxh"
#include "../common.fxh"

#if MULTISAMPLE_TECHNIQUES

Texture2DMS<float>	DepthTextureMS;

// ----------------------------------------------------------------------------------------------- //

struct rageVertexBlitIn {
	float3 pos			: POSITION;
	float3 normal		: NORMAL;
	float4 diffuse		: COLOR0;
	float2 texCoord0	: TEXCOORD0;
};

struct rageVertexBlit {
	// This is used for the simple vertex and pixel shader routines
	DECLARE_POSITION(pos)
	float4 diffuse		: COLOR0;
	float2 texCoord0	: TEXCOORD0;
};

rageVertexBlit VS_Blit(rageVertexBlitIn IN)
{
	rageVertexBlit OUT;
	OUT.pos = float4( IN.pos.xyz, 1.0f);
	OUT.diffuse	= IN.diffuse;
	OUT.texCoord0 = IN.texCoord0 ;
	return(OUT);
}

// ----------------------------------------------------------------------------------------------- //

#if ENABLE_EQAA
# define NUM_FRAGMENTS	gMSAANumFragments
#else
# define NUM_FRAGMENTS	gMSAANumSamples
#endif // ENABLE_EQAA

float PS_Resolve_Depth_Sample0(rageVertexBlit IN) : SV_Depth
{
	float Result = DepthTextureMS.Load(int2(IN.pos.xy),0).x;
	return Result;
}

float PS_Resolve_Depth_Min(rageVertexBlit IN) : SV_Depth
{
	int2 iPos = int2(IN.pos.xy);
	float depth = FLT_MAX;
	
	if (NUM_FRAGMENTS == 1)
	{
		depth = DepthTextureMS.Load(iPos, 0).x;
	}else
	if(NUM_FRAGMENTS <= 4)
	{
		float4 v4;
		v4.x = DepthTextureMS.Load(iPos, 0).x;
		v4.y = DepthTextureMS.Load(iPos, 1).x;
		if (NUM_FRAGMENTS == 4)
		{
			v4.z = DepthTextureMS.Load(iPos, 2).x;
			v4.w = DepthTextureMS.Load(iPos, 3).x;
			v4.x = min(v4.x, v4.y);
			v4.y = min(v4.z, v4.w);
		}
		depth = min(v4.x, v4.y);
	}
	else
	{
		float4	v0 = float4(
			DepthTextureMS.Load(iPos, 0),
			DepthTextureMS.Load(iPos, 1),
			DepthTextureMS.Load(iPos, 2),
			DepthTextureMS.Load(iPos, 3));
		float4	v1 = float4(
			DepthTextureMS.Load(iPos, 4),
			DepthTextureMS.Load(iPos, 5),
			DepthTextureMS.Load(iPos, 6),
			DepthTextureMS.Load(iPos, 7));
			
		v0 = min(v0, v1);
		depth = min(v0.w, min(v0.z, min(v0.x, v0.y)));
	}

	return depth;
}

float PS_Resolve_Depth_Max(rageVertexBlit IN) : SV_Depth
{
	int2 iPos = int2(IN.pos.xy);
	
	float depth = 0.0;
	if (NUM_FRAGMENTS == 1)
	{
		depth = DepthTextureMS.Load(iPos, 0).x;
	}else
	if(NUM_FRAGMENTS <= 4)
	{
		float4 v4;
		v4.x = DepthTextureMS.Load(iPos, 0).x;
		v4.y = DepthTextureMS.Load(iPos, 1).x;
		if (NUM_FRAGMENTS == 4)
		{
			v4.z = DepthTextureMS.Load(iPos, 2).x;
			v4.w = DepthTextureMS.Load(iPos, 3).x;
			v4.x = max(v4.x, v4.y);
			v4.y = max(v4.z, v4.w);
		}
		depth = max(v4.x, v4.y);
	}
	else
	{
		float4	v0 = float4(
			DepthTextureMS.Load(iPos, 0),
			DepthTextureMS.Load(iPos, 1),
			DepthTextureMS.Load(iPos, 2),
			DepthTextureMS.Load(iPos, 3));
		float4	v1 = float4(
			DepthTextureMS.Load(iPos, 4),
			DepthTextureMS.Load(iPos, 5),
			DepthTextureMS.Load(iPos, 6),
			DepthTextureMS.Load(iPos, 7));
			
		v0 = max(v0, v1);
		depth = max(v0.w, max(v0.z, max(v0.x, v0.y)));
	}

	return depth;
}

float2 ChooseDominant(float4 v, float avg)
{
	float4 diff = abs(v - avg);
	if (diff.x < diff.w)
	{
		diff.xw = diff.wx;
		v.xw = v.wx;
	}
	if (diff.y < diff.w)
	{
		diff.yw = diff.wy;
		v.yw = v.wy;
	}
	if (diff.z < diff.w)
	{
		diff.zw = diff.wz;
		v.zw = v.wz;
	}
	return float2(v.w, diff.w);
}

float PS_Resolve_Depth_Dominant(rageVertexBlit IN) : SV_Depth
{
	int2 iPos = int2(IN.pos.xy);

	if (NUM_FRAGMENTS <= 2)
	{
		return DepthTextureMS.Load(iPos, 0).x;
	}else
	if(NUM_FRAGMENTS <= 4)
	{
		float4 v = float4(
			DepthTextureMS.Load(iPos, 0),
			DepthTextureMS.Load(iPos, 1),
			DepthTextureMS.Load(iPos, 2),
			DepthTextureMS.Load(iPos, 3));
		float avg = dot(v, 0.25);
		return ChooseDominant(v, avg).x;
	}
	else
	{
		float4	v0 = float4(
			DepthTextureMS.Load(iPos, 0),
			DepthTextureMS.Load(iPos, 1),
			DepthTextureMS.Load(iPos, 2),
			DepthTextureMS.Load(iPos, 3));
		float4	v1 = float4(
			DepthTextureMS.Load(iPos, 4),
			DepthTextureMS.Load(iPos, 5),
			DepthTextureMS.Load(iPos, 6),
			DepthTextureMS.Load(iPos, 7));
		float avg = dot(v0, 0.125) + dot(v1, 0.125);
		float2 r0 = ChooseDominant(v0, avg);
		float2 r1 = ChooseDominant(v1, avg);
		return r0.y < r1.y ? r0.x : r1.x;
	}
}

// ----------------------------------------------------------------------------------------------- //
#if __PSSL
	#define RESOLVEDEPTH_VS	vs_5_0
	#define RESOLVEDEPTH_PS	ps_5_0
#else
	#define RESOLVEDEPTH_VS	vs_4_1
	#define RESOLVEDEPTH_PS	ps_4_1
#endif	//__PSSL

technique ResolveDepthMS
{
	pass resolve_depth_sample0
	{
		SetVertexShader(	compileshader( RESOLVEDEPTH_VS, VS_Blit() )	);
		SetPixelShader(		compileshader( RESOLVEDEPTH_PS, PS_Resolve_Depth_Sample0() )	);
	}
	pass resolve_depth_min
	{
		SetVertexShader(	compileshader( RESOLVEDEPTH_VS, VS_Blit() )	);
		SetPixelShader(		compileshader( RESOLVEDEPTH_PS, PS_Resolve_Depth_Min() )	);
	}
	pass resolve_depth_max
	{
		SetVertexShader(	compileshader( RESOLVEDEPTH_VS, VS_Blit() )	);
		SetPixelShader(		compileshader( RESOLVEDEPTH_PS, PS_Resolve_Depth_Max() )	);
	}
	pass resolve_depth_dominant
	{
		SetVertexShader(	compileshader( RESOLVEDEPTH_VS, VS_Blit() )	);
		SetPixelShader(		compileshader( RESOLVEDEPTH_PS, PS_Resolve_Depth_Dominant() )	);
	}
}

#else

technique ResolveDepthMS
{
 	pass dummy
	{
	}
}

#endif //MULTISAMPLE_TECHNIQUES

