// =======================
// Debug/debug_shadows.fx
// (c) 2011 RockstarNorth
// =======================
#if __MAX
	technique tool_draw { pass empty {} }
#else //__MAX..

#pragma strip off
#pragma dcl position diffuse texcoord0

#include "../common.fxh"

#pragma constant 0

#define DEFERRED_UNPACK_LIGHT
#define DEBUG_SHADOWS_FX

#define SHADOW_CASTING            (0)
#define SHADOW_CASTING_TECHNIQUES (0)
#define SHADOW_RECEIVING          (1)
#define SHADOW_RECEIVING_VS       (0)
#include "../Lighting/Shadows/cascadeshadows.fxh"

// ================================================================================================
BeginDX10Sampler(sampler,  Texture2DArray, baseTexture, baseTextureSamp, baseTexture)
ContinueSampler( sampler,                  baseTexture, baseTextureSamp, baseTexture)
	AddressU  = WRAP;
	AddressV  = WRAP;
	MIPFILTER = POINT;
	MINFILTER = POINT;
	MAGFILTER = POINT;
EndSampler;

DECLARE_SAMPLER(sampler2D, depthBuffer, depthBufferSamp,
	AddressU  = CLAMP;
	AddressV  = CLAMP;
	MIPFILTER = NONE;
	MINFILTER = POINT;
	MAGFILTER = POINT;
);

DECLARE_SAMPLER(sampler2D, normalBuffer, normalBufferSamp,
	AddressU  = CLAMP;
	AddressV  = CLAMP;
	MIPFILTER = NONE;
	MINFILTER = POINT;
	MAGFILTER = POINT;
);

struct debugVertexIn
{
	float4 pos		: POSITION;
	float4 colour   : COLOR0;
	float2 texcoord : TEXCOORD0;
};

struct debugVertexOut
{
	DECLARE_POSITION(pos)
	float4 colour   : COLOR0;
	float3 texcoord : TEXCOORD0;
};

debugVertexOut VS_main_BANK_ONLY(debugVertexIn IN)
{
	debugVertexOut OUT;
	OUT.pos			= float4(IN.pos.xy, 0, 1);
	OUT.colour		= IN.colour;
	OUT.texcoord	= float3(IN.texcoord, IN.pos.z);
	return OUT;
}

float4 PS_copyColour_BANK_ONLY(debugVertexOut IN) : COLOR
{
	return IN.colour;
}

#if !__PS3
#define shadowOff 0.5.xx/shadowRes
#else
#define shadowOff 0
#endif

float4 PS_copyTexture_BANK_ONLY(debugVertexOut IN) : COLOR
{
	const float2 cascadeRes = float2(CASCADE_SHADOWS_RES_X, CASCADE_SHADOWS_RES_Y);
	const float2 shadowRes  = cascadeRes*float2(1, CASCADE_SHADOWS_COUNT);

#if __SHADERMODEL >= 40
	const float3 c			= baseTexture.Sample(baseTextureSamp, float3(IN.texcoord.xy + shadowOff, IN.texcoord.z)).xyz;
#else
	const float3 c			= tex2D(baseTextureSamp, float3(IN.texcoord.xy + shadowOff, IN.texcoord.z)).xyz;
#endif

	return float4(IN.colour.xyz*c, IN.colour.w);
}

float4 PS_copyTextureEdgeDetect_BANK_ONLY(debugVertexOut IN) : COLOR
{
	const float2 cascadeRes = float2(CASCADE_SHADOWS_RES_X, CASCADE_SHADOWS_RES_Y);
	const float2 shadowRes  = cascadeRes*float2(1, CASCADE_SHADOWS_COUNT);
	
	float3 tex				= float3(IN.texcoord.xy + shadowOff, IN.texcoord.z);

#if __SHADERMODEL >= 40
	const float3 c0 =          baseTexture.Sample(baseTextureSamp, tex.xyz).xyz;
	const float3 cx = abs(c0 - baseTexture.Sample(baseTextureSamp, float3(tex.xy + IN.colour.z*255*float2(1, 0)/shadowRes, tex.z)).xyz);
	const float3 cy = abs(c0 - baseTexture.Sample(baseTextureSamp, float3(tex.xy + IN.colour.z*255*float2(0, 1)/shadowRes, tex.z)).xyz);
#else
	const float3 c0 =          tex2D(baseTextureSamp, tex.xyz).xyz;
	const float3 cx = abs(c0 - tex2D(baseTextureSamp, float3(tex.xy + IN.colour.z*255*float2(1, 0)/shadowRes, tex.z)).xyz);
	const float3 cy = abs(c0 - tex2D(baseTextureSamp, float3(tex.xy + IN.colour.z*255*float2(0, 1)/shadowRes, tex.z)).xyz);
#endif

	const float3 ca = max(cx, cy);

	const float filled  = 0.11;
	const float opacity = 0.33;

	const float3 c = 1 - lerp(pow(ca, 0.25), c0, filled)*opacity;
	return float4(IN.colour.xyz*c, IN.colour.w);
}

float4 PS_copyDepth_BANK_ONLY(debugVertexOut IN) : COLOR
{
	const float2 cascadeRes = float2(CASCADE_SHADOWS_RES_X, CASCADE_SHADOWS_RES_Y);
	const float2 shadowRes  = cascadeRes*float2(1, CASCADE_SHADOWS_COUNT);

#if __SHADERMODEL >= 40
	float depth = baseTexture.Sample(baseTextureSamp, float3(IN.texcoord.xy + shadowOff, IN.texcoord.z));
#else
	float depth = tex2D(baseTextureSamp, float3(IN.texcoord.xy + shadowOff, IN.texcoord.z));
#endif

	if (IN.colour.x > 0.5) { depth = frac(0.5 + depth*32); }

	return float4(depth.xxx, IN.colour.w);
}

float4 PS_copyDepthEdgeDetect_BANK_ONLY(debugVertexOut IN) : COLOR
{
	const float2 cascadeRes = float2(CASCADE_SHADOWS_RES_X, CASCADE_SHADOWS_RES_Y);
	const float2 shadowRes  = cascadeRes*float2(1, CASCADE_SHADOWS_COUNT);

	float3 tex				= float3(IN.texcoord.xy + shadowOff, IN.texcoord.z);

#if __SHADERMODEL >= 40
	const float c0 =      baseTexture.Sample(baseTextureSamp, tex.xyz).x;
	const float cx = c0 - baseTexture.Sample(baseTextureSamp, float3(tex.xy + IN.colour.z*255*float2(1, 0)/shadowRes, tex.z)).x;
	const float cy = c0 - baseTexture.Sample(baseTextureSamp, float3(tex.xy + IN.colour.z*255*float2(0, 1)/shadowRes, tex.z)).x;
#else
	const float c0 =      tex2D(baseTextureSamp, tex.xyz).x;
	const float cx = c0 - tex2D(baseTextureSamp, float3(tex.xy + IN.colour.z*255*float2(1, 0)/shadowRes, tex.z)).x;
	const float cy = c0 - tex2D(baseTextureSamp, float3(tex.xy + IN.colour.z*255*float2(0, 1)/shadowRes, tex.z)).x;
#endif

	const float ca = 1 - pow(max(abs(cx), abs(cy)), IN.colour.x);

	float3 colour = ca;

	if (1) // apply tint
	{
		const float3 tint = clamp(float3(0, float2(cx, cy)*5000 + 0.5), 0, 1);
		const float3 colour2 = lerp(float3(1,1,1), tint, 0.5)*ca*ca;

		colour = lerp(colour, colour2, IN.colour.y);
	}

	return float4(colour, IN.colour.w);
}

technique dbg_shadows
{
	pass dbg_shadows_pass_copyColour
	{
		VertexShader = compile VERTEXSHADER VS_main_BANK_ONLY();
		PixelShader  = compile PIXELSHADER  PS_copyColour_BANK_ONLY() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass dbg_shadows_pass_copyTexture
	{
		VertexShader = compile VERTEXSHADER VS_main_BANK_ONLY();
		PixelShader  = compile PIXELSHADER  PS_copyTexture_BANK_ONLY() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass dbg_shadows_pass_copyDepth
	{
		VertexShader = compile VERTEXSHADER VS_main_BANK_ONLY();
		PixelShader  = compile PIXELSHADER  PS_copyDepth_BANK_ONLY() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass dbg_shadows_pass_copyTextureEdgeDetect
	{
		VertexShader = compile VERTEXSHADER VS_main_BANK_ONLY();
		PixelShader  = compile PIXELSHADER  PS_copyTextureEdgeDetect_BANK_ONLY() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass dbg_shadows_pass_copyDepthEdgeDetect
	{
		VertexShader = compile VERTEXSHADER VS_main_BANK_ONLY();
		PixelShader  = compile PIXELSHADER  PS_copyDepthEdgeDetect_BANK_ONLY() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}

// ================================================================================================

BEGIN_RAGE_CONSTANT_BUFFER(debug_shadows_locals,b0)
ROW_MAJOR float4x4 viewToWorldProjectionParam; // w components store deferred projection params
EndConstantBufferDX10(debug_shadows_locals)

float4 shadowDebug_internal(debugVertexOut IN, int sampleType)
{
	const float4 deferredProjectionParam = float4(
		viewToWorldProjectionParam[0].w,
		viewToWorldProjectionParam[1].w,
		viewToWorldProjectionParam[2].w,
		viewToWorldProjectionParam[3].w
	);
	const float3 eyePos      = viewToWorldProjectionParam[3].xyz;
	const float3 eyeRay      = mul(float3((IN.texcoord*2 - 1)*float2(1,-1)*deferredProjectionParam.xy, -1), (float3x3)viewToWorldProjectionParam); // in worldspace
	const float  sampleDepth = tex2D(depthBufferSamp, IN.texcoord).x;
	const float  linearDepth = getLinearGBufferDepth(sampleDepth, deferredProjectionParam.zw);
	const float3 worldNormal = normalize(tex2D(normalBufferSamp, IN.texcoord).xyz*2 - 1); // untwiddled
	const float3 worldPos    = eyePos + eyeRay*linearDepth;

	const CascadeShadowsParams params = CascadeShadowsParams_setup(sampleType);

	const float4 colour = CalcCascadeShadows_debug(params, linearDepth, eyePos, worldPos, worldNormal, IN.texcoord);
	const bool bIsSky = fixupDepth(sampleDepth) > 0.99995;

	return colour*IN.colour.w*(bIsSky ? 0 : 1);
}

float4 PS_shadowDebug_BANK_ONLY(debugVertexOut IN) : COLOR
{
	return shadowDebug_internal(IN, CSM_ST_DEFAULT);
}

#if !defined(SHADER_FINAL)

technique dbg_shadows_overlay
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER VS_main_BANK_ONLY();
		PixelShader  = compile PIXELSHADER  PS_shadowDebug_BANK_ONLY() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}

#endif // !defined(SHADER_FINAL)
#endif // __MAX...
