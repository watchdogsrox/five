//--------------------------------------------------------------------------------------
// File: DOF_include.hlsl
//
// Copyright (c) AMD Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// Defines
//--------------------------------------------------------------------------------------
#ifndef KERNEL_DIAMETER
#define KERNEL_DIAMETER         ( 29 )
#endif

#define KERNEL_RADIUS           ( KERNEL_DIAMETER / 2 )
#define PI                      ( 3.14159f )
#define GAUSSIAN_DEVIATION      ( 7.0f )
#define PIXELS_PER_THREAD       ( 4 )

//--------------------------------------------------------------------------------------
// Constant Buffers
//--------------------------------------------------------------------------------------
/*cbuffer cbDOF : register(b0)
{
    float4	g_vInputImageSize;
    float4	g_vOutputImageSize;
    float   g_fQ;
    float   g_fQTimesZNear;
    float   g_fFocalDepth;
    float   g_fDOFAmount;
    float   g_fNoBlurRange;
    float   g_fMaxBlurRange;
};*/


//--------------------------------------------------------------------------------------
// Textures
//--------------------------------------------------------------------------------------

/*BeginDX10Sampler(sampler, Texture2D, txTextureInput_Texture, txTextureInput_Sampler, txTextureInput_Texture)
ContinueSampler(sampler, txTextureInput_Texture, txTextureInput_Sampler, txTextureInput_Texture)
	AddressU	= CLAMP;
	AddressV	= CLAMP;
	MINFILTER	= POINT;
	MAGFILTER	= POINT;
EndSampler;*/

BeginDX10Sampler(sampler, Texture2D, txCOC_Texture, txCOC_Sampler, txCOC_Texture)
ContinueSampler(sampler, txCOC_Texture, txCOC_Sampler, txCOC_Texture)
	AddressU	= CLAMP;
	AddressV	= CLAMP;
	MINFILTER	= POINT;
	MAGFILTER	= POINT;
EndSampler;

/*BeginDX10Sampler(sampler, Texture2D, txOriginalTextureInput_Texture, txOriginalTextureInput_Sampler, txOriginalTextureInput_Texture)
ContinueSampler(sampler, txOriginalTextureInput_Texture, txOriginalTextureInput_Sampler, txOriginalTextureInput_Texture)
	AddressU	= CLAMP;
	AddressV	= CLAMP;
	MINFILTER	= POINT;
	MAGFILTER	= POINT;
EndSampler;*/

// Texture2D txTextureInput		: register(t0);
// Texture2D txDepthInput			: register(t1);
// Texture2D txOriginalTextureInput: register(t2);


//--------------------------------------------------------------------------------------
// Sampler States Variables
//--------------------------------------------------------------------------------------
// SamplerState samLinear : register (s0);
// SamplerState samPoint  : register (s1);


//--------------------------------------------------------------------------------------
// Functions
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// ConvertDepthToLinear()
// Convert depth buffer depth to linear depth
// fQ           = CameraFarClip / (CameraFarClip - CameraNearClip)
// fQTimesZNear = fQ * CameraNearClip;
//--------------------------------------------------------------------------------------
float ConvertDepthToLinear(float fDepth, float fQ, float fQTimesZNear)
{
	return ( -fQTimesZNear / ( fDepth - fQ ) );
}

//--------------------------------------------------------------------------------------
// CalculateCircleOfConfusion()
// Calculate CoC from supplied parameters
// fDOFAmount:    A scale factor to determine the amount of DOF to generate
// fDepth:        Pixel depth
// fFocalDepth:   Depth at which the image is in focus
// fNoBlurRange:  Depth range under which the image is in full focus (non-blurry)
// fMaxBlurRange: Depth range beyond which the image is fully blurred
//                fMaxBlurRange must be larger then fNoBlurRange
//--------------------------------------------------------------------------------------
float CalculateCircleOfConfusion(float fDOFAmount, float fDepth, float fFocalDepth, 
                                 float fNoBlurRange, float fMaxBlurRange)
{
    float fCOC = saturate( fDOFAmount * max(0, abs(fDepth - fFocalDepth) - fNoBlurRange) / (fMaxBlurRange - fNoBlurRange) );
    
    return fCOC;
}

//--------------------------------------------------------------------------------------
// GetGaussianDistribution
//--------------------------------------------------------------------------------------
float GetGaussianDistribution(float x, float rho)
{
   float g = 1.0 / sqrt(2.0 * 3.141592654f * rho * rho);
   return g * exp(-(x*x)/(2.0*rho*rho));
}


#if 0
float CalculateDOFFactor(float fDepth, float fFocalDepth, float fOneOverFocalRangeNear, float fOneOverFocalRangeFar)
{
	float fDOFFactor;

	/*	
	[flatten]if (fDepth < fFocalDepth)
	{
		// Scale depth value between near DOF plane and focal plane to [-1, 0] range
		fDOFFactor = (fDepth - fFocalDepth) * fOneOverFocalRangeNear;	// fDOFFactor = (fDepth - fFocalDepth) / (fFocalDepth - fDOFNearPlane);
		fDOFFactor = clamp(fDOFFactor, -1.0, 0.0);
	}
	else
	{
		// Scale depth value between focal depth and far DOF plane to [0, 1] range
		fDOFFactor = (fDepth - fFocalDepth) * fOneOverFocalRangeFar;	// fDOFFactor = (fDepth - fFocalDepth) / (fDOFFarPlane - fFocalDepth);
		
		// Clamp DOF factor to a maximum blurriness if required
		fDOFFactor = clamp(fDOFFactor, 0.0, 1.0);
	}
	*/
	
	// Faster version than the above
	fDOFFactor =	 saturate( (fDepth - fFocalDepth) * fOneOverFocalRangeFar ) - 
					 saturate( (fFocalDepth - fDepth) * fOneOverFocalRangeNear );

	return fDOFFactor;
}
#endif
	
//--------------------------------------------------------------------------------------
// Packing and unpacking functions
//--------------------------------------------------------------------------------------

uint PackFloat4IntoUint(float4 vValue)
{
	return ( ((uint)(vValue.w*255)) << 24 ) | ( ((uint)(vValue.z*255)) << 16 ) | ( ((uint)(vValue.y*255)) << 8) | (uint)(vValue.x * 255);
}

float4 UnpackUintIntoFloat4(uint uValue)
{
	return float4( (uValue & 0x000000FF) / 255.0, ( (uValue & 0x0000FF00)>>8 ) / 255.0, ( (uValue & 0x00FF0000)>>16 ) / 255.0, ( (uValue & 0xFF000000)>>24 ) / 255.0 );
}

uint PackFloat3IntoUint(float3 vValue)
{
	//return ( ( ((uint)(vValue.z*255)) << 16 ) | ( ((uint)(vValue.y*255)) << 8) | (uint)(vValue.x * 255) );
	
	// 11 11 10 packing version
	return ( ( ((uint)(vValue.z*2047)) << 21 ) | ( ((uint)(vValue.y*2047)) << 10) | (uint)(vValue.x * 1023) );
}

float3 UnpackUintIntoFloat3(uint uValue)
{
	//return float3( (uValue & 0x000000FF) / 255.0, ( (uValue & 0x0000FF00)>>8 ) / 255.0, ( (uValue & 0x00FF0000)>>16 ) / 255.0 );
	
	// 11 11 10 unpacking version
	return float3( (uValue & 0x000003FF) / 1023.0, ( (uValue & 0x001FFC00)>>10 ) / 2047.0, ( (uValue & 0xFFE00000)>>21 ) / 2047.0 );
}
