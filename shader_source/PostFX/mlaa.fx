//=============================================================================================
// 
// Copyright (C) 2010 Rockstar Games.  All Rights Reserved. 
// 
// Morphological Anti-Aliasing shaders. The original implementation comes from the
// "Practical Morphological Anti-aliasing" article in GPU Pro 2.
//
//=============================================================================================

#pragma strip off
#pragma dcl position
#define NO_SKINNING

#include "../common.fxh"

#pragma constant 130

//=============================================================================================
// Constants
//=============================================================================================

#define MLAA_MAX_SEARCH_STEPS  (32)

BeginConstantBufferDX10(MLAA_locals)
float4 g_vTexelSize		     : TexelSize = 1.f;	// to be set by the caller side
float  g_fLuminanceThreshold : LuminanceThreshold = 0.1f;
float4 g_vRelativeLuminanceThresholdParams : RelativeLuminanceThresholdParams = float4(0.056f,0.278f,0,0);//float4(0.035f,0.15f,0,0);
EndConstantBufferDX10(MLAA_locals)

//=============================================================================================
// Samplers
//=============================================================================================

BeginSampler(	sampler, MLAAPointTexture, MLAAPointSampler, MLAAPointTexture)
ContinueSampler(sampler, MLAAPointTexture, MLAAPointSampler, MLAAPointTexture)
		AddressU  = CLAMP;
		AddressV  = CLAMP;
		AddressW  = CLAMP; 
		MINFILTER = POINT;
		MAGFILTER = POINT;
		MIPFILTER = POINT;
EndSampler;

BeginSampler(	sampler, MLAALinearTexture, MLAALinearSampler, MLAALinearTexture)
ContinueSampler(sampler, MLAALinearTexture, MLAALinearSampler, MLAALinearTexture)
		AddressU  = CLAMP;
		AddressV  = CLAMP;
		AddressW  = CLAMP; 
		MINFILTER = LINEAR;
		MAGFILTER = LINEAR;
		MIPFILTER = POINT;
EndSampler;

BeginSampler(	sampler, MLAAAreaLUTTexture, MLAAAreaLUTSampler, MLAAAreaLUTTexture)
ContinueSampler(sampler, MLAAAreaLUTTexture, MLAAAreaLUTSampler, MLAAAreaLUTTexture)
		AddressU  = CLAMP;
		AddressV  = CLAMP;
		AddressW  = CLAMP; 
		MINFILTER = POINT;
		MAGFILTER = POINT;
		MIPFILTER = POINT;
EndSampler;

//=============================================================================================
// Shader input/output structs
//=============================================================================================

struct MLAAVertexInput 
{
    float3 vPosition	: POSITION;
    float2 vUV      	: TEXCOORD0;
};

struct MLAAVertexOutput 
{
    DECLARE_POSITION(vPosition)
    float2 vUV      	: TEXCOORD0;
};

//=============================================================================================
// Shader code
//=============================================================================================

//===============================================
// Draw a full screen quad
//===============================================
MLAAVertexOutput VS_MLAA_FullScreenQuad ( MLAAVertexInput IN )
{
	// All the hackery to ensure pixels align with texels is handled on the CPU side

    MLAAVertexOutput OUT;

	OUT.vPosition = float4(IN.vPosition,1);
	OUT.vUV       = IN.vUV
/*#if (__SHADERMODEL < 40 && !__PS3)		
	 + (0.5*g_vTexelSize.xy) // Half texel offset to get pixels and texels to align
#endif*/
	;

    return OUT;
}

float RelativeLuminanceThreshold (float3 vColor)
{
	const float fBias = g_vRelativeLuminanceThresholdParams.x;
	const float fScale = g_vRelativeLuminanceThresholdParams.y;
	return max( fBias, max( max( vColor.r, vColor.g ), vColor.b ) * fScale );
}

//============================
// Edge detection pixel shader
//============================
float4 PS_MLAA_EdgeDetection ( MLAAVertexOutput IN ) : COLOR
{
	float2 vUV = IN.vUV;

	// Point sample color values of neighborhood
	float4 vSample_Center, vSample_Left, vSample_Right, vSample_Top, vSample_Bottom;

#if __XENON
	asm {
		// We only need Point sampling here but we use the Linear sampler anyway since it results in better texture cache behavior (saving 0.1ms)
        tfetch2D vSample_Center, vUV, MLAAPointSampler, OffsetX= 0, OffsetY= 0
		tfetch2D vSample_Left,   vUV, MLAAPointSampler, OffsetX=-1, OffsetY= 0
		tfetch2D vSample_Right,  vUV, MLAAPointSampler, OffsetX= 1, OffsetY= 0
		tfetch2D vSample_Top,    vUV, MLAAPointSampler, OffsetX= 0, OffsetY=-1
		tfetch2D vSample_Bottom, vUV, MLAAPointSampler, OffsetX= 0, OffsetY= 1
    };
#else
	vSample_Center = tex2D( MLAAPointSampler, vUV );
	vSample_Left   = tex2D( MLAAPointSampler, vUV + float2(-1, 0) * g_vTexelSize.xy );
	vSample_Right  = tex2D( MLAAPointSampler, vUV + float2( 1, 0) * g_vTexelSize.xy );
	vSample_Top    = tex2D( MLAAPointSampler, vUV + float2( 0,-1) * g_vTexelSize.xy );
	vSample_Bottom = tex2D( MLAAPointSampler, vUV + float2( 0, 1) * g_vTexelSize.xy );
#endif

	// Compute luminance values
	
	const bool bUseRelativeThreshold = true;

	if ( !bUseRelativeThreshold )
	{
		// Absolute threshold

		const float3 vLuminanceCoefs = float3(0.2126f, 0.7152f, 0.0722f);
		
		// Luminance is already computed in the postfx composite pass and stored in alpha
		float fLum_Center = vSample_Center.a; //dot( vSample_Center, vLuminanceCoefs );
		float fLum_Left   = vSample_Left.a;   //dot( vSample_Left,   vLuminanceCoefs );
		float fLum_Right  = vSample_Right.a;  //dot( vSample_Right,  vLuminanceCoefs );
		float fLum_Top    = vSample_Top.a;    //dot( vSample_Top,    vLuminanceCoefs );
		float fLum_Bottom = vSample_Bottom.a; //dot( vSample_Bottom, vLuminanceCoefs );

		// Use luminance deltas to find edges
		float4 vLumDeltas = abs( fLum_Center.xxxx - float4( fLum_Left, fLum_Top, fLum_Right, fLum_Bottom ) );

		float4 vEdges = step( g_fLuminanceThreshold.xxxx, vLumDeltas );

		// Clip if there are no edges here, this is required to set stencil for edge pixels
		float fClipFlag = any4(vEdges) ? 1.f : -1.f;
		clip( fClipFlag );

		return vEdges;
	}
	else
	{
		// Relative threshold 

		const float3 vLuminanceCoefs = float3(0.2126f, 0.7152f, 0.0722f);
		
		// Luminance is already computed in the postfx composite pass and stored in alpha
		float fLum_Center = vSample_Center.a; //dot( vSample_Center, vLuminanceCoefs );
		float fLum_Left   = vSample_Left.a;   //dot( vSample_Left,   vLuminanceCoefs );
		float fLum_Right  = vSample_Right.a;  //dot( vSample_Right,  vLuminanceCoefs );
		float fLum_Top    = vSample_Top.a;    //dot( vSample_Top,    vLuminanceCoefs );
		float fLum_Bottom = vSample_Bottom.a; //dot( vSample_Bottom, vLuminanceCoefs );

		float4 vLumDeltas = abs( fLum_Center.xxxx - float4( fLum_Left, fLum_Top, fLum_Right, fLum_Bottom ) );

		float  fCenterThreshold = RelativeLuminanceThreshold(vSample_Center);
		float4 vThresholds;
		vThresholds.x = min( fCenterThreshold, RelativeLuminanceThreshold(vSample_Left) );
		vThresholds.y = min( fCenterThreshold, RelativeLuminanceThreshold(vSample_Top) );
		vThresholds.z = min( fCenterThreshold, RelativeLuminanceThreshold(vSample_Right) );
		vThresholds.w = min( fCenterThreshold, RelativeLuminanceThreshold(vSample_Bottom) );

		float4 vEdges = step( vThresholds, vLumDeltas );

		// Clip if there are no edges here, this is required to set stencil for edge pixels
		float fClipFlag = any4(vEdges) ? 1.f : -1.f;
		clip( fClipFlag );

		return vEdges;
	}
}

//========================================
// Depth-based edge detection pixel shader
//========================================
float4 PS_MLAA_DepthEdgeDetection ( MLAAVertexOutput IN ) : COLOR
{
	float2 vUV = IN.vUV;

	// Point sample depth values of neighborhood
	float fSample_Center, fSample_Left, fSample_Right, fSample_Top, fSample_Bottom;

#if __XENON
	asm {
        tfetch2D fSample_Center.x, vUV, MLAAPointSampler, OffsetX= 0, OffsetY= 0
		tfetch2D fSample_Left.x,   vUV, MLAAPointSampler, OffsetX=-1, OffsetY= 0
		tfetch2D fSample_Right.x,  vUV, MLAAPointSampler, OffsetX= 1, OffsetY= 0
		tfetch2D fSample_Top.x,    vUV, MLAAPointSampler, OffsetX= 0, OffsetY=-1
		tfetch2D fSample_Bottom.x, vUV, MLAAPointSampler, OffsetX= 0, OffsetY= 1
    };
#else
	fSample_Center = tex2D( MLAAPointSampler, vUV ).x;
	fSample_Left   = tex2D( MLAAPointSampler, vUV + float2(-1, 0) * g_vTexelSize.xy ).x;
	fSample_Right  = tex2D( MLAAPointSampler, vUV + float2( 1, 0) * g_vTexelSize.xy ).x;
	fSample_Top    = tex2D( MLAAPointSampler, vUV + float2( 0,-1) * g_vTexelSize.xy ).x;
	fSample_Bottom = tex2D( MLAAPointSampler, vUV + float2( 0, 1) * g_vTexelSize.xy ).x;
#endif

	// Use deltas to find edges
	float4 vDeltas = abs( fSample_Center.xxxx - float4( fSample_Left, fSample_Top, fSample_Right, fSample_Bottom ) );

	// Dividing by 10 give us results similar to the color-based detection
	const float fThreshold = 0.001;
	float4 vEdges = step( fThreshold, vDeltas );

	// Clip if there are no edges here, this is required to set stencil for edge pixels
	float fClipFlag = any4(vEdges) ? 1.f : -1.f;
	clip( fClipFlag );

	return vEdges;
}

float MLAA_SearchHorizontalLeft( float2 vUV )
{
	vUV = vUV - float2(1.5, 0.0) * g_vTexelSize.xy;
    float fEdge = 0.f;

    // We offset by 0.5 to sample between edgels, thus fetching two in a row
	int i = 0;
    for ( ; i < MLAA_MAX_SEARCH_STEPS; i++ )
	{
		fEdge = tex2Dlod( MLAALinearSampler, float4(vUV,0,0) ).g;
		
        // We compare with 0.9 to prevent bilinear access precision problems
        /*[flatten]*/ if ( fEdge < 0.9f ) break;
        vUV -= float2(2.0, 0.0) * g_vTexelSize.xy;
    }

    // When we exit the loop without finding the end, we want to return -2 * maxSearchSteps
    return max(-2.0 * i - 2.0 * fEdge, -2.0 * MLAA_MAX_SEARCH_STEPS);
}

float MLAA_SearchHorizontalRight( float2 vUV )
{
	vUV = vUV + float2(1.5, 0.0) * g_vTexelSize.xy;
    float fEdge = 0.f;

    // We offset by 0.5 to sample between edgels, thus fetching two in a row
	int i = 0;
    for (; i < MLAA_MAX_SEARCH_STEPS; i++ )
	{
		fEdge = tex2Dlod( MLAALinearSampler, float4(vUV,0,0) ).g;

        // We compare with 0.9 to prevent bilinear access precision problems
        /*[flatten]*/ if ( fEdge < 0.9f ) break;
        vUV += float2(2.0, 0.0) * g_vTexelSize.xy;
    }

    // When we exit the loop without finding the end, we want to return 2 * maxSearchSteps
    return min(2.0 * i + 2.0 * fEdge, 2.0 * MLAA_MAX_SEARCH_STEPS);
}

float MLAA_SearchVerticalUp( float2 vUV )
{
	vUV = vUV - float2(0.0, 1.5) * g_vTexelSize.xy;
    float fEdge = 0.f;

	// We offset by 0.5 to sample between edgels, thus fetching two in a row
	int i = 0;
	for (; i < MLAA_MAX_SEARCH_STEPS; i++ )
	{
		fEdge = tex2Dlod( MLAALinearSampler, float4(vUV, 0,0) ).r;

		// We compare with 0.9 to prevent bilinear access precision problems
		/*[flatten]*/ if ( fEdge < 0.9f ) break;
		vUV -= float2(0.0, 2.0) * g_vTexelSize.xy;
	}

	// When we exit the loop without finding the end, we want to return -2 * maxSearchSteps
	return max(-2.0 * i - 2.0 * fEdge, -2.0 * MLAA_MAX_SEARCH_STEPS);
}

float MLAA_SearchVerticalDown( float2 vUV )
{
	vUV = vUV + float2(0.0, 1.5) * g_vTexelSize.xy;
	float fEdge = 0.f;

	// We offset by 0.5 to sample between edgels, thus fetching two in a row
	int i = 0;
	for (; i < MLAA_MAX_SEARCH_STEPS; i++ )
	{
		fEdge = tex2Dlod( MLAALinearSampler, float4(vUV, 0,0) ).r;

		// We compare with 0.9 to prevent bilinear access precision problems
		/*[flatten]*/ if ( fEdge < 0.9f ) break;
		vUV += float2(0.0, 2.0) * g_vTexelSize.xy;
	}

	// When we exit the loop without finding the end, we want to return 2 * maxSearchSteps
	return min(2.0 * i + 2.0 * fEdge, 2.0 * MLAA_MAX_SEARCH_STEPS);
}

// If you change MLAA_NUM_DISTANCES you must also change AREA_LUT_SIZE in the mlaa.cpp file
#define MLAA_NUM_DISTANCES (32)
#define MLAA_AREA_SIZE (MLAA_NUM_DISTANCES*5)
#define MLAA_USE_A8L8_FOR_LUT (!RSG_ORBIS)

// Sample from the precomputed Area Look Up Table
float2 MLAA_Area ( float2 vDistance, float fEdge1, float fEdge2 )
{
	// By dividing by AREA_SIZE-1 below we are implicitly offsetting to always fall inside a pixel.
	// Rounding prevents bilinear precision problems.

	float2 vPixelCoord = MLAA_NUM_DISTANCES * round(4.f * float2(fEdge1,fEdge2)) + vDistance;
	float2 vTexelCoord = vPixelCoord / (MLAA_AREA_SIZE-1.f);

	#if MLAA_USE_A8L8_FOR_LUT
		#if RSG_PC
			return tex2Dlod( MLAAAreaLUTSampler, float4(vTexelCoord, 0,0) ).rg;
		#else
			return tex2Dlod( MLAAAreaLUTSampler, float4(vTexelCoord, 0,0) ).wx;
		#endif
	#else
		return tex2Dlod( MLAAAreaLUTSampler, float4(vTexelCoord,0,0) ).xy;
	#endif
}


//======================================
// Blend weight calculation pixel shader
//======================================
float4 PS_MLAA_BlendWeightCalculation ( MLAAVertexOutput IN ) : COLOR
{
	float4 vWeights = 0.f;

	float2 vEdges = tex2D( MLAAPointSampler, IN.vUV ).xy;

	// Edge at North
	[branch] 
	if ( vEdges.g ) 
	{
		float2 vDistance = float2( MLAA_SearchHorizontalLeft(IN.vUV), MLAA_SearchHorizontalRight(IN.vUV) );

		// Instead of sampling between edgels, we sample at -0.25 to be able to discern what value each edgel has
		float4 vCoords = float4(vDistance.x, -0.25f, vDistance.y + 1.f, -0.25) * g_vTexelSize.xyxy + IN.vUV.xyxy;

		float fEdge1 = tex2Dlod( MLAALinearSampler, float4(vCoords.xy,0,0) ).r;
		float fEdge2 = tex2Dlod( MLAALinearSampler, float4(vCoords.zw,0,0) ).r;

		vWeights.xy = MLAA_Area( abs(vDistance), fEdge1, fEdge2 );
	}

	// Edge at West
	[branch]
	if ( vEdges.r ) 
	{
		float2 vDistance = float2( MLAA_SearchVerticalUp(IN.vUV), MLAA_SearchVerticalDown(IN.vUV) );

		// Instead of sampling between edgels, we sample at -0.25 to be able to
		// discern what value each edgel has
		float4 vCoords = float4(-0.25f, vDistance.x, -0.25f, vDistance.y + 1.f) * g_vTexelSize.xyxy + IN.vUV.xyxy;

		float fEdge1 = tex2Dlod( MLAALinearSampler, float4(vCoords.xy,0,0) ).g;
		float fEdge2 = tex2Dlod( MLAALinearSampler, float4(vCoords.zw,0,0) ).g;

		vWeights.zw = MLAA_Area( abs(vDistance), fEdge1, fEdge2 );
	}
	return vWeights;
}

//======================================
// Anti-aliasing pixel shader
//======================================
float4 PS_MLAA_AntiAliasing ( MLAAVertexOutput IN ) : COLOR
{
	float2 vUV = IN.vUV;

	// Get the blending weights
	float4 vTopLeftWeights;
	float fRightWeight, fBottomWeight;

#if __XENON
	asm {
        tfetch2D vTopLeftWeights,    vUV, MLAAPointSampler, OffsetX= 0, OffsetY= 0
		tfetch2D fRightWeight.y___,  vUV, MLAAPointSampler, OffsetX= 0, OffsetY= 1
		tfetch2D fBottomWeight.w___, vUV, MLAAPointSampler, OffsetX= 1, OffsetY= 0
    };
#else
	vTopLeftWeights = tex2D( MLAAPointSampler, vUV );
	fRightWeight    = tex2D( MLAAPointSampler, vUV + float2( 0, 1) * g_vTexelSize.xy ).g;
	fBottomWeight   = tex2D( MLAAPointSampler, vUV + float2( 1, 0) * g_vTexelSize.xy ).a;
#endif

	float4 vBlendWeights = float4( vTopLeftWeights.r, fRightWeight, vTopLeftWeights.b, fBottomWeight );
	float fWeightSum = dot( vBlendWeights, float4(1,1,1,1) );

	[branch]
	if ( fWeightSum > 0.f )
	{
		// Do MLAA blending
		float4 vO = vBlendWeights * g_vTexelSize.yyxx;

		float4 vColor  = tex2Dlod( MLAALinearSampler, float4(vUV + float2(0,-vO.r), 0,0) ) * vBlendWeights.r;
			   vColor += tex2Dlod( MLAALinearSampler, float4(vUV + float2(0,vO.g ), 0,0) ) * vBlendWeights.g;
			   vColor += tex2Dlod( MLAALinearSampler, float4(vUV + float2(-vO.b,0), 0,0) ) * vBlendWeights.b;
			   vColor += tex2Dlod( MLAALinearSampler, float4(vUV + float2(vO.a,0 ), 0,0) ) * vBlendWeights.a;

		return vColor / fWeightSum;
	}
	else
	{
		// Not an edge pixel, no blending necessary
		return tex2D(  MLAALinearSampler, vUV ); // MLAAPointSampler, vUV );
	}
}

float4 PS_MLAA_NonAliasedCopy ( MLAAVertexOutput IN ) : COLOR
{
	float2 vUV = IN.vUV;
	return tex2D( MLAAPointSampler, vUV );
}

//=============================================================================================
// Techniques
//=============================================================================================

technique EdgeDetection
{
	
	pass p0
	{
		/*
		CullMode = NONE;
  		ZEnable = false;
		AlphaBlendEnable = false;
        AlphaTestEnable = false;
		ZWriteEnable = false;

		StencilEnable = true;
		StencilPass   = replace;
		StencilFunc   = always;
		StencilRef    = 1;
		*/

		VertexShader = compile VERTEXSHADER VS_MLAA_FullScreenQuad();
		PixelShader  = compile PIXELSHADER PS_MLAA_EdgeDetection();
	}
}

technique DepthEdgeDetection
{
	
	pass p0
	{
		/*
		CullMode = NONE;
  		ZEnable = false;
		AlphaBlendEnable = false;
        AlphaTestEnable = false;
		ZWriteEnable = false;

		StencilEnable = true;
		StencilPass   = replace;
		StencilFunc   = always;
		StencilRef    = 1;
		*/

		VertexShader = compile VERTEXSHADER VS_MLAA_FullScreenQuad();
		PixelShader  = compile PIXELSHADER PS_MLAA_DepthEdgeDetection();
	}
}

technique BlendWeightCalculation
{
	
	pass p0
	{
		/*
		CullMode = NONE;
  		ZEnable = false;
		AlphaBlendEnable = false;
        AlphaTestEnable = false;
		ZWriteEnable = false;

        StencilEnable = true;
        StencilFunc   = equal;
		StencilRef		= 1;
		*/

		VertexShader = compile VERTEXSHADER VS_MLAA_FullScreenQuad();
		PixelShader  = compile PIXELSHADER PS_MLAA_BlendWeightCalculation();
	}
}

technique AntiAliasing
{
	
	pass p0
	{
		/*
		CullMode = NONE;
  		ZEnable = false;
		AlphaBlendEnable = false;
        AlphaTestEnable = false;
		ZWriteEnable = false;

		StencilEnable = true;
        StencilFunc   = equal;
		StencilRef		= 1;
		*/

		VertexShader = compile VERTEXSHADER VS_MLAA_FullScreenQuad();
		PixelShader  = compile PIXELSHADER PS_MLAA_AntiAliasing();
	}
}

technique NonAliasedCopy
{
	
	pass p0
	{
		/*
		CullMode = NONE;
  		ZEnable = false;
		AlphaBlendEnable = false;
        AlphaTestEnable = false;
		ZWriteEnable = false;

		StencilEnable = true;
        StencilFunc   = notequal;
		StencilRef		= 1;
		*/

		VertexShader = compile VERTEXSHADER VS_MLAA_FullScreenQuad();
		PixelShader  = compile PIXELSHADER PS_MLAA_NonAliasedCopy();
	}
}
