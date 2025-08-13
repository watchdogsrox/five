#ifndef PRAGMA_DCL
	#pragma dcl position diffuse texcoord0
	#define PRAGMA_DCL
#endif
//
// DistortionWarp - FOr Oculus Rift
//
//
//

#include "../common.fxh"

//-------------------------------------------------------------------------------------
// ***** Distortion Post-process Shaders

BeginConstantBufferDX10(LensDistort)
	//float4x4 View : register(c4);
	row_major SHARED float4x4 Texm; // : register(c8);
	float2 LensCenter;
	float2 ScreenCenter;
	float2 ScreenSizeHalfScale;
	float2 Scale;
	float2 ScaleIn;
	float4 HmdWarpParam;
	float4 ChromAbParam;
EndConstantBufferDX10(LensDistort)

struct DistortionWarpVertexInput 
{
    float3 vPosition	: POSITION;
	float4 Diffuse		: COLOR0;
    float2 vUV      	: TEXCOORD0;
};

struct DistortionWarpVertexOutput 
{
    DECLARE_POSITION(vPosition)
	float4 Diffuse		: COLOR0;
    float2 vUV      	: TEXCOORD0;
};

DistortionWarpVertexOutput VS_DistortionWarp(DistortionWarpVertexInput IN)
/*	
	in float4 Position : POSITION, in float4 Color : COLOR0, in float2 TexCoord : TEXCOORD0, 
    out float4 oPosition : SV_Position, out float4 oColor : COLOR, out float2 oTexCoord : TEXCOORD0)
*/
{
	DistortionWarpVertexOutput OUT;
	OUT.vPosition = float4(IN.vPosition, 1.0f);
	OUT.vUV = mul(Texm, float4(IN.vUV,0,1));
	OUT.Diffuse = IN.Diffuse;
	/*
	oPosition = mul(View, Position);
	oTexCoord = mul(Texm, float4(TexCoord,0,1));
	oColor = Color;
	*/
	return OUT;
};

BeginDX10Sampler(sampler, Texture2D<float4>, SourceTexture, LinearSampler, SourceTexture)
ContinueSampler(sampler, SourceTexture, LinearSampler, SourceTexture)
	AddressU	= CLAMP;
	AddressV	= CLAMP;
	MINFILTER	= LINEAR;
	MAGFILTER	= LINEAR;
EndSampler;

// Shader with just lens distortion correction.
//Texture2D Texture : register(t0);
//SamplerState Linear : register(s0);

/*
float2 LensCenter;
float2 ScreenCenter;
float2 Scale;
float2 ScaleIn;
float4 HmdWarpParam;
*/

// Scales input texture coordinates for distortion.
// ScaleIn maps texture coordinates to Scales to ([-1, 1]), although top/bottom will be
// larger due to aspect ratio.
float2 HmdWarp(float2 in01)
{
   float2 theta = (in01 - LensCenter) * ScaleIn; // Scales to [-1, 1]
   float  rSq = theta.x * theta.x + theta.y * theta.y;
   float2 theta1 = theta * (HmdWarpParam.x + HmdWarpParam.y * rSq + 
                   HmdWarpParam.z * rSq * rSq + HmdWarpParam.w * rSq * rSq * rSq);
   return LensCenter + Scale * theta1;
}

/*
float4 main(in float4 oPosition : SV_Position, in float4 oColor : COLOR,
			in float2 oTexCoord : TEXCOORD0) : SV_Target
*/
float4 PS_LensDistortion(DistortionWarpVertexOutput IN) : COLOR  
{
   float2 tc = HmdWarp(IN.vUV); // oTexCoord);
   //if (any(clamp(tc, ScreenCenter-float2(0.25,0.5), ScreenCenter+float2(0.25, 0.5)) - tc))
   if (any(clamp(tc, ScreenCenter-ScreenSizeHalfScale, ScreenCenter+ScreenSizeHalfScale) - tc))
   
       return 0;
   return tex2D(LinearSampler, tc); // Texture.Sample(Linear, tc);
};

/*
// Shader with lens distortion and chromatic aberration correction.
Texture2D Texture : register(t0);
SamplerState Linear : register(s0);
float2 LensCenter;
float2 ScreenCenter;
float2 Scale;
float2 ScaleIn;
float4 HmdWarpParam;
float4 ChromAbParam;
*/

// Scales input texture coordinates for distortion.
// ScaleIn maps texture coordinates to Scales to ([-1, 1]), although top/bottom will be
// larger due to aspect ratio.
/*
float4 main(in float4 oPosition : SV_Position, in float4 oColor : COLOR,
            in float2 oTexCoord : TEXCOORD0) : SV_Target
*/
float4 PS_LensDistortionChromaticAberration(DistortionWarpVertexOutput IN) : COLOR  
{
   float2 theta = (IN.vUV /*oTexCoord*/ - LensCenter) * ScaleIn; // Scales to [-1, 1]
   float  rSq= theta.x * theta.x + theta.y * theta.y;
   float2 theta1 = theta * (HmdWarpParam.x + HmdWarpParam.y * rSq + 
                   HmdWarpParam.z * rSq * rSq + HmdWarpParam.w * rSq * rSq * rSq);
   
   // Detect whether blue texture coordinates are out of range since these will scaled out the furthest.
   float2 thetaBlue = theta1 * (ChromAbParam.z + ChromAbParam.w * rSq);
   float2 tcBlue = LensCenter + Scale * thetaBlue;
   //if (any(clamp(tcBlue, ScreenCenter-float2(0.25,0.5), ScreenCenter+float2(0.25, 0.5)) - tcBlue))
   if (any(clamp(tcBlue, ScreenCenter-ScreenSizeHalfScale, ScreenCenter+ScreenSizeHalfScale) - tcBlue))
       return 0;
   
   // Now do blue texture lookup.
   float  blue = tex2D(LinearSampler, tcBlue).b; // Texture.Sample(Linear, tcBlue).b;
   
   // Do green lookup (no scaling).
   float2 tcGreen = LensCenter + Scale * theta1;
   float  green = tex2D(LinearSampler, tcGreen).g; // Texture.Sample(Linear, tcGreen).g;
   
   // Do red scale and lookup.
   float2 thetaRed = theta1 * (ChromAbParam.x + ChromAbParam.y * rSq);
   float2 tcRed = LensCenter + Scale * thetaRed;
   float  red = tex2D(LinearSampler, tcRed).r; // Texture.Sample(Linear, tcRed).r;
   
   return float4(red, green, blue, 1);
};

technique DistortionWarp
{
	pass dw_LensDistortion
	{
		AlphaBlendEnable = false;
		AlphaTestEnable = false;

		VertexShader = compile VERTEXSHADER VS_DistortionWarp();
		PixelShader  = compile PIXELSHADER PS_LensDistortion()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
	pass dw_LensDistortionChromaticAberration
	{
		AlphaBlendEnable = false;
		AlphaTestEnable = false;

		VertexShader = compile VERTEXSHADER VS_DistortionWarp();
		PixelShader  = compile PIXELSHADER PS_LensDistortionChromaticAberration()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}

