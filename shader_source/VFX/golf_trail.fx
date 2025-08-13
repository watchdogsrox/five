// 
// golf_trail.fx
// 
// Copyright (C) 2011 Rockstar Games.  All Rights Reserved.
// 
#pragma dcl position normal diffuse texcoord0
#include "../common.fxh"
#include "../Util/macros.fxh"

struct vertexInput
{
	float3 pos : POSITION;
	float3 tan : NORMAL; // tangent
	float4 col : COLOR0;
	float2 r_d : TEXCOORD0; // x=radius, y=texcoord.x
};

struct vertexOutput
{
	DECLARE_POSITION(pos)
	float4 col       : COLOR0;
	float3 tex       : TEXCOORD0; // z=fill
	float3 worldPos  : TEXCOORD1;
	float4 screenPos : TEXCOORD2;
};

BEGIN_RAGE_CONSTANT_BUFFER(golf_trail_locals,b0)
float4 params[3];
EndConstantBufferDX10(golf_trail_locals)


DECLARE_SAMPLER(sampler2D, GolfTrailTexture, GolfTrailTextureSamp,
	AddressU  = CLAMP;
	AddressV  = CLAMP;
	MIPFILTER = MIPLINEAR;
	MINFILTER = HQLINEAR;
	MAGFILTER = MAGLINEAR;
);

DECLARE_SAMPLER(sampler2D, DepthBuffer, DepthBufferSamp,
	AddressU  = CLAMP;
	AddressV  = CLAMP;
	MIPFILTER = NONE;
	MINFILTER = POINT;
	MAGFILTER = POINT;
);

vertexOutput GolfTrailInternal(vertexInput IN, bool bFacing)
{
	vertexOutput OUT;

	const float params_worldToPixels  = params[0].x;
	const float params_pixelExpansion = params[0].y;
	const float params_radiusScale    = 1;
	const float params_fadeOpacity    = params[0].z;
	const float params_fadeExponent   = params[0].w;
	const float params_textureFill    = params[1].x;

	float3 worldPos = IN.pos;//mul(float4(IN.pos, 1), (float4x3)gWorld);
	float3 worldTan = IN.tan;//mul(IN.tan, (float3x3)gWorld);

	float r = abs(IN.r_d.x);
	float s = (IN.r_d.x > 0) ? 1 : -1;
	float x = IN.r_d.y;

	const float3 camPos = +gViewInverse[3].xyz;
	const float3 camDir = -gViewInverse[2].xyz;

	const float z = max(0.001, dot(worldPos - camPos, camDir));

	float h = r*params_radiusScale*(params_worldToPixels/z); // size in pixels
	float a;

	a = min(1, h); h += params_pixelExpansion;
	r = max(1, h)/(params_worldToPixels/z);

	const float3 axis = bFacing ? (worldPos - camPos) : float3(0,0,1);

	worldPos += r*s*normalize(cross(axis, worldTan));

	const float fade = params_fadeOpacity*pow(a*a, params_fadeExponent);
	const float4 screenSize = float4((float)SCR_BUFFER_WIDTH, (float)SCR_BUFFER_HEIGHT, 1.0/(float)SCR_BUFFER_WIDTH, 1.0/(float)SCR_BUFFER_HEIGHT);

	OUT.pos       = mul(float4(worldPos, 1), gWorldViewProj);
	OUT.col       = float4(IN.col.xyz, IN.col.w*fade);
	OUT.tex       = float3(x, (s + 1)/2, params_textureFill);
	OUT.worldPos  = worldPos;
	OUT.screenPos = float4(convertToVpos(OUT.pos, screenSize).xy, dot(worldPos - camPos, camDir), OUT.pos.w);

	return OUT;
}

vertexOutput VS_GolfTrail       (vertexInput IN) { return GolfTrailInternal(IN, false); }
vertexOutput VS_GolfTrail_facing(vertexInput IN) { return GolfTrailInternal(IN, true); }

float4 GolfTrailInternal(vertexOutput IN, bool bDepthCull)
{
#if __SHADERMODEL >= 40
	//On dx11 we use an rg texture and it doesnt propagate to all channels like the console appears to do.
	float4 t = tex2D(GolfTrailTextureSamp, IN.tex.xy).gggr;
#else
	float4 t = tex2D(GolfTrailTextureSamp, IN.tex.xy);
#endif
	t.w = lerp(t.w, t.x, IN.tex.z); // apply texture fill

	if (bDepthCull)
	{
		const float depthSample = tex2D(DepthBufferSamp, IN.screenPos.xy/IN.screenPos.w).x;
		const float depthLinear = getLinearGBufferDepth(depthSample, params[2].zw);

		t *= (depthLinear >= IN.screenPos.z);
	}

	return IN.col*t;
}

float4 PS_GolfTrail      (vertexOutput IN) : COLOR { return GolfTrailInternal(IN, false); }
float4 PS_GolfTrail_depth(vertexOutput IN) : COLOR { return GolfTrailInternal(IN, true); }

#define GOLF_TRAIL_RENDERSTATES(ZEnable_) \
	CullMode         = NONE; \
	ZEnable          = ZEnable_; \
	ZFunc			 = FixedupLESS; \
	ZWriteEnable     = false; \
	AlphaBlendEnable = true; \
	AlphaTestEnable  = false; \
	BlendOp          = ADD; \
	SrcBlend         = SRCALPHA; \
	DestBlend        = INVSRCALPHA

technique golf_trail
{
	pass p0
	{
		GOLF_TRAIL_RENDERSTATES(false);
		VertexShader = compile VERTEXSHADER VS_GolfTrail();
		PixelShader  = compile PIXELSHADER  PS_GolfTrail_depth() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p1 // facing
	{
		GOLF_TRAIL_RENDERSTATES(false);
		VertexShader = compile VERTEXSHADER VS_GolfTrail_facing();
		PixelShader  = compile PIXELSHADER  PS_GolfTrail_depth() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p2
	{
		GOLF_TRAIL_RENDERSTATES(true);
		VertexShader = compile VERTEXSHADER VS_GolfTrail();
		PixelShader  = compile PIXELSHADER  PS_GolfTrail() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p3 // facing
	{
		GOLF_TRAIL_RENDERSTATES(true);
		VertexShader = compile VERTEXSHADER VS_GolfTrail_facing();
		PixelShader  = compile PIXELSHADER  PS_GolfTrail() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
