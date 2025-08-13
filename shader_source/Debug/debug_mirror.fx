// ===============
// debug_mirror.fx
// ===============

#pragma strip off

#ifndef PRAGMA_DCL
	#pragma dcl position texcoord0
	#define PRAGMA_DCL
#endif

#include "../common.fxh"
#include "../Util/macros.fxh"
#include "../Lighting/lighting.fxh"

#define MIRROR_USE_STENCIL_MASK  (1) // if we had a shared header file we could share this with the define in Mirrors.h
#define MIRROR_USE_CRACK_TEXTURE (1)

//------------------------------------

struct vertexInput
{
	float3 pos       : POSITION;
	float2 texcoords : TEXCOORD0;
};

struct vertexOutput
{
	DECLARE_POSITION(pos)
	float4 screenPos : TEXCOORD0;
	float4 bounds    : TEXCOORD1; // TODO -- mirrorParams should be accessible in pixel shader, but somehow it's getting corrupted
	float4 colour    : TEXCOORD2; // TODO -- mirrorParams should be accessible in pixel shader, but somehow it's getting corrupted
	float2 texcoords : TEXCOORD3;
};

BEGIN_RAGE_CONSTANT_BUFFER(debug_mirror_locals,b0)
float4 mirrorParams[2];

#if MIRROR_USE_CRACK_TEXTURE
float4 mirrorCrackParams[1];
#endif

#if MIRROR_USE_STENCIL_MASK
float4 mirrorProjectionParams;
#endif

EndConstantBufferDX10(debug_mirror_locals)

DECLARE_SAMPLER(sampler2D, mirrorReflectionTexture, mirrorReflectionSampler,
	AddressU  = CLAMP;
	AddressV  = CLAMP;
	MIPFILTER = NONE;
	MINFILTER = PS3_SWITCH(GAUSSIAN, LINEAR);
	MAGFILTER = PS3_SWITCH(GAUSSIAN, LINEAR);
);

vertexOutput VS_mirror_BANK_ONLY(vertexInput IN)
{
	vertexOutput OUT;

	OUT.pos       = mul(float4(IN.pos,1), gWorldViewProj);
	OUT.screenPos = OUT.pos;
	OUT.bounds    = mirrorParams[0];
	OUT.colour    = mirrorParams[1];
	OUT.texcoords = float2(0, 0);
	
	return OUT;
}

vertexOutput VS_mirror_direct_texcoords_BANK_ONLY(vertexInput IN)
{
	vertexOutput OUT;

	OUT.pos       = mul(float4(IN.pos,1), gWorldViewProj);
	OUT.screenPos = OUT.pos;
	OUT.bounds    = mirrorParams[0];
	OUT.colour    = mirrorParams[1];
	OUT.texcoords = IN.texcoords;
	
	return OUT;
}

half4 PS_mirror_BANK_ONLY(vertexOutput IN) : COLOR
{
	const float2 refUV = IN.bounds.xy + IN.bounds.zw*(IN.screenPos.xy/IN.screenPos.ww);

#if __XENON
	float3 refColour0;
	float3 refColour1;
	float3 refColour2;
	float3 refColour3;
	asm
	{
		tfetch2D refColour0.xyz, refUV, mirrorReflectionSampler, OffsetX = 0.0, OffsetY = 0.0
		tfetch2D refColour1.xyz, refUV, mirrorReflectionSampler, OffsetX = 0.0, OffsetY = 1.0
		tfetch2D refColour2.xyz, refUV, mirrorReflectionSampler, OffsetX = 1.0, OffsetY = 0.0
		tfetch2D refColour3.xyz, refUV, mirrorReflectionSampler, OffsetX = 1.0, OffsetY = 1.0
	};
	float3 refColour = (refColour0 + refColour1 + refColour2 + refColour3)/4.0;
#else
	float3 refColour = UnpackColor_3h(tex2D(mirrorReflectionSampler, refUV).xyz); // now uses gaussian
#endif

	return PackColor(IN.colour*float4(refColour*gEmissiveScale, gInvColorExpBias));
}

half4 PS_mirror_direct_texcoords_BANK_ONLY(vertexOutput IN) : COLOR
{
	const float2 refUV = IN.texcoords;

#if __XENON
	float3 refColour0;
	float3 refColour1;
	float3 refColour2;
	float3 refColour3;
	asm
	{
		tfetch2D refColour0.xyz, refUV, mirrorReflectionSampler, OffsetX = 0.0, OffsetY = 0.0
		tfetch2D refColour1.xyz, refUV, mirrorReflectionSampler, OffsetX = 0.0, OffsetY = 1.0
		tfetch2D refColour2.xyz, refUV, mirrorReflectionSampler, OffsetX = 1.0, OffsetY = 0.0
		tfetch2D refColour3.xyz, refUV, mirrorReflectionSampler, OffsetX = 1.0, OffsetY = 1.0
	};
	float3 refColour = (refColour0 + refColour1 + refColour2 + refColour3)/4.0;
#else
	float3 refColour = UnpackColor_3h(tex2D(mirrorReflectionSampler, refUV).xyz); // now uses gaussian
#endif

	return PackColor(IN.colour*float4(refColour*gEmissiveScale, gInvColorExpBias));
}

// ================================================================================================

#if MIRROR_USE_CRACK_TEXTURE

struct vertexInputCrack
{
	float3 pos       : POSITION;
	float2 texcoord  : TEXCOORD0;
};

struct vertexOutputCrack
{
	DECLARE_POSITION(pos)
	float2 texcoord  : TEXCOORD0;
	float4 screenPos : TEXCOORD1;
	float4 bounds    : TEXCOORD2; // TODO -- mirrorParams should be accessible in pixel shader, but somehow it's getting corrupted
	float4 colour    : TEXCOORD3; // TODO -- mirrorParams should be accessible in pixel shader, but somehow it's getting corrupted
};

DECLARE_SAMPLER(sampler2D, mirrorCrackTexture, mirrorCrackSampler,
	AddressU  = CLAMP;
	AddressV  = CLAMP;
	MIPFILTER = POINT;
	MINFILTER = POINT;
	MAGFILTER = POINT;
);

vertexOutputCrack VS_mirrorCrack_BANK_ONLY(vertexInputCrack IN)
{
	vertexOutputCrack OUT;

	OUT.pos       = mul(float4(IN.pos,1), gWorldViewProj);
	OUT.texcoord  = IN.texcoord;
	OUT.screenPos = OUT.pos;
	OUT.bounds    = mirrorParams[0];
	OUT.colour    = mirrorParams[1];

	return OUT;
}

half4 PS_mirrorCrack_BANK_ONLY(vertexOutputCrack IN) : COLOR
{
	const float4 vm = tex2D(mirrorCrackSampler, IN.texcoord);
	//float2 sc;
	//sincos(vm.w*2.0*PI, sc.x, sc.y);
	const float2 sc = vm.xy*2 - 1;
	const float2 offUV = sc*mirrorCrackParams[0].xx/float2(512.0, 256.0);//*(vm.w != 0);
	const float2 refUV = offUV + IN.bounds.xy + IN.bounds.zw*(IN.screenPos.xy/IN.screenPos.ww);

#if __XENON
	float3 refColour0;
	float3 refColour1;
	float3 refColour2;
	float3 refColour3;
	asm
	{
		tfetch2D refColour0.xyz, refUV, mirrorReflectionSampler, OffsetX = 0.0, OffsetY = 0.0
		tfetch2D refColour1.xyz, refUV, mirrorReflectionSampler, OffsetX = 0.0, OffsetY = 1.0
		tfetch2D refColour2.xyz, refUV, mirrorReflectionSampler, OffsetX = 1.0, OffsetY = 0.0
		tfetch2D refColour3.xyz, refUV, mirrorReflectionSampler, OffsetX = 1.0, OffsetY = 1.0
	};
	float3 refColour = (refColour0 + refColour1 + refColour2 + refColour3)/4.0;
#else
	float3 refColour = UnpackColor_3h(tex2D(mirrorReflectionSampler, refUV).xyz); // now uses gaussian
#endif

	refColour += vm.zzz*mirrorCrackParams[0].yyy;

	return PackColor(IN.colour*float4(refColour*gEmissiveScale, gInvColorExpBias));
}

#endif // MIRROR_USE_CRACK_TEXTURE

// ================================================================================================

#if MIRROR_USE_STENCIL_MASK

DECLARE_SAMPLER(sampler2D, mirrorDepthBuffer, mirrorDepthBufferSamp,
	AddressU  = CLAMP;
	AddressV  = CLAMP;
	MIPFILTER = NONE;
	MINFILTER = POINT;
	MAGFILTER = POINT;
);

struct VS_mirrorStencilMaskOutput
{
	DECLARE_POSITION(pos)
	float4 screenPos : TEXCOORD0; // z=depth
};

VS_mirrorStencilMaskOutput VS_mirrorStencilMask_BANK_ONLY(float3 pos : POSITION)
{
	VS_mirrorStencilMaskOutput OUT;

	const float4 mirrorScreenSize = float4((float)SCR_BUFFER_WIDTH, (float)SCR_BUFFER_HEIGHT, 1.0/(float)SCR_BUFFER_WIDTH, 1.0/(float)SCR_BUFFER_HEIGHT);

	const float3 camPos = +gViewInverse[3].xyz;
	const float3 camDir = -gViewInverse[2].xyz;

	OUT.pos       = mul(float4(pos, 1), gWorldViewProj);
	OUT.screenPos = float4(convertToVpos(OUT.pos, mirrorScreenSize).xy, dot(pos - camPos, camDir), OUT.pos.w);

	return OUT;
}

void mirrorStencilMaskDiscard(float4 screenPos)
{
	const float depthSample = tex2D(mirrorDepthBufferSamp, screenPos.xy/screenPos.ww).x;
	const float depth       = getLinearGBufferDepth(depthSample, mirrorProjectionParams.zw);

	rageDiscard(depth < screenPos.z);
}

#if __PS3

float PS_mirrorStencilMask_BANK_ONLY(VS_mirrorStencilMaskOutput IN) : DEPTH
{
	mirrorStencilMaskDiscard(IN.screenPos);

	return 1;
}

#else // not __PS3

struct PS_mirrorStencilMaskOutput
{
	float4 colour : COLOR;
	float  depth  : DEPTH;
};

PS_mirrorStencilMaskOutput PS_mirrorStencilMask_BANK_ONLY(VS_mirrorStencilMaskOutput IN)
{
	mirrorStencilMaskDiscard(IN.screenPos);

	PS_mirrorStencilMaskOutput OUT;

	OUT.colour = 0;
	OUT.depth  = 1;

	return OUT;
}

#endif // not __PS3

#endif // MIRROR_USE_STENCIL_MASK

// ===============================
// technique
// ===============================

#if !defined(SHADER_FINAL)

technique dbg_mirror
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER VS_mirror_BANK_ONLY();
		PixelShader  = compile PIXELSHADER  PS_mirror_BANK_ONLY() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p1
	{
		VertexShader = compile VERTEXSHADER VS_mirror_direct_texcoords_BANK_ONLY();
		PixelShader  = compile PIXELSHADER  PS_mirror_direct_texcoords_BANK_ONLY() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
	
#if MIRROR_USE_CRACK_TEXTURE
	pass p2
	{
		VertexShader = compile VERTEXSHADER VS_mirrorCrack_BANK_ONLY();
		PixelShader  = compile PIXELSHADER  PS_mirrorCrack_BANK_ONLY() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
#endif // MIRROR_USE_CRACK_TEXTURE

#if MIRROR_USE_STENCIL_MASK
	pass p3
	{
		VertexShader = compile VERTEXSHADER VS_mirrorStencilMask_BANK_ONLY();
		PixelShader  = compile PIXELSHADER  PS_mirrorStencilMask_BANK_ONLY() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
#endif // MIRROR_USE_STENCIL_MASK
}

#endif // !defined(SHADER_FINAL)
