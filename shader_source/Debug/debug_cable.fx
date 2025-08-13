// 
// debug_cable.fx
// 
// Copyright (C) 2011 Rockstar Games.  All Rights Reserved.
// 

#pragma strip off

#pragma dcl position normal diffuse texcoord0
#include "../common.fxh"
#include "../Util/macros.fxh"

struct vertexInput
{
	float3 pos : POSITION;
	float3 tan : NORMAL; // tangent
	float4 col : COLOR0; // xy=phase, z=texcoord.x, w=unused
	float2 r_d : TEXCOORD0;
};

struct vertexOutput
{
	DECLARE_POSITION(pos)
	float4 col : COLOR0;
	float2 tex : TEXCOORD0;
	float3 n   : TEXCOORD1; // normal
	float3 b   : TEXCOORD2; // binormal
};

float4 params[4];

DECLARE_SAMPLER(sampler2D, textureMap, textureSamp,
	AddressU  = CLAMP;
	AddressV  = BORDER; // border colour should be 0,0,0,0
	MIPFILTER = MIPLINEAR;
	MINFILTER = LINEAR;
	MAGFILTER = LINEAR;
);

vertexOutput DebugCableInternal(vertexInput IN, bool bApplyMicromovements)
{
	vertexOutput OUT;

	const float  params_worldToPixels  = params[0].x;
	const float  params_radiusScale    = params[0].y;
	const float  params_pixelExpansion = params[0].z;
	const float  params_fadeExponent   = params[0].w;
	const float4 params_cableColour    = params[1].xyzw;

	float3 worldPos = IN.pos;//mul(float4(IN.pos, 1), (float4x3)gWorld);
	float3 worldTan = IN.tan;//mul(IN.tan, (float3x3)gWorld);

	float r = abs(IN.r_d.x);
	float s = (IN.r_d.x > 0) ? 1 : -1;
	float d = IN.r_d.y;

	if (bApplyMicromovements)
	{
		const float2 params_cableWindMotionFreqTime   = params[2].xy;
		const float2 params_cableWindMotionAmp        = params[2].zw;
		const float2 params_cableWindMotionOffset     = params[3].xy;
		const float  params_cableWindMotionPhaseScale = params[3].z;
		const float  params_cableWindMotionSkewAdj    = params[3].w;

		const float2 windPhase = IN.col.xy;
		const float2 wind      = params_cableWindMotionFreqTime.xy + params_cableWindMotionPhaseScale.xx*windPhase.xy;
		const float2 windSkew  = float2(sin(params_cableWindMotionOffset.xy + sin(wind*2.0*PI)*params_cableWindMotionAmp.xy));

		worldPos.xy += d*windSkew;
		worldPos.z  += d*dot(windSkew, windSkew)*params_cableWindMotionSkewAdj;
	}

	const float3 camPos = +gViewInverse[3].xyz;
	const float3 camDir = -gViewInverse[2].xyz;

	const float z = dot(worldPos - camPos, camDir);

	float h = r*params_radiusScale*(params_worldToPixels/z); // size in pixels
	float a;

	a = min(1, h); h += params_pixelExpansion;
	r = max(1, h)/(params_worldToPixels/z);

	const float3 n = normalize(cross(worldPos - camPos, worldTan));

	worldPos += r*s*n;

	const float fade = pow(a*a, params_fadeExponent);

	OUT.pos = mul(float4(worldPos, 1), gWorldViewProj);
	OUT.col = float4(params_cableColour.xyz, params_cableColour.w*fade);
	OUT.tex = float2(IN.col.x, (s + 1)/2);
	OUT.n   = n;
	OUT.b   = cross(n, worldTan);

	return OUT;
}

vertexOutput VS_DebugCable_BANK_ONLY   (vertexInput IN) { return DebugCableInternal(IN, false); }
vertexOutput VS_DebugCable_BANK_ONLY_mm(vertexInput IN) { return DebugCableInternal(IN, true); }

float4 PS_DebugCable_BANK_ONLY(vertexOutput IN) : COLOR
{
	return float4(IN.col.xyz, IN.col.w*tex2D(textureSamp, IN.tex).x);
}

float4 PS_DebugCable_BANK_ONLY_lit(vertexOutput IN) : COLOR
{
	const float  y = IN.tex.y*2 - 1;
	const float3 n = normalize(IN.n)*y + normalize(IN.b)*sqrt(1 - y*y); // world normal

	return float4((n + 1)/2, IN.col.w*tex2D(textureSamp, IN.tex).x);
}

#if !defined(SHADER_FINAL)

technique dbg_cable
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER VS_DebugCable_BANK_ONLY();
		PixelShader  = compile PIXELSHADER  PS_DebugCable_BANK_ONLY() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p1 // micromovements
	{
		VertexShader = compile VERTEXSHADER VS_DebugCable_BANK_ONLY_mm();
		PixelShader  = compile PIXELSHADER  PS_DebugCable_BANK_ONLY() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p2 // lit
	{
		VertexShader = compile VERTEXSHADER VS_DebugCable_BANK_ONLY();
		PixelShader  = compile PIXELSHADER  PS_DebugCable_BANK_ONLY_lit() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p3 // lit + micromovements
	{
		VertexShader = compile VERTEXSHADER VS_DebugCable_BANK_ONLY_mm();
		PixelShader  = compile PIXELSHADER  PS_DebugCable_BANK_ONLY_lit() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}

#endif // !defined(SHADER_FINAL)
