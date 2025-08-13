
#pragma dcl position
#include "../../renderer/PostFX_shared.h"
#include "../common.fxh"

//see efficient compute shader programming AMD powerpoint for more optimisations.

#if DOF_COMPUTE_GAUSSIAN || DOF_DIFFUSION

// Inputs
BeginDX10Sampler(sampler, Texture2D<float4>, ColorTexture, ColorSampler, ColorTexture)
ContinueSampler(sampler, ColorTexture, ColorSampler, ColorTexture)
	AddressU	= CLAMP;
	AddressV	= CLAMP;
	MINFILTER	= POINT;
	MAGFILTER	= POINT;
EndSampler;


BeginDX10Sampler(sampler, Texture2D<float2>, DepthBlurTexture, DepthBlurSampler, DepthBlurTexture)
ContinueSampler(sampler, DepthBlurTexture, DepthBlurSampler, DepthBlurTexture)
	AddressU	= CLAMP;
	AddressV	= CLAMP;
	MINFILTER	= POINT;
	MAGFILTER	= POINT;
EndSampler;

BeginDX10Sampler(sampler, Texture2D<float2>, DepthBlurHalfResTexture, DepthBlurHalfResSampler, DepthBlurHalfResTexture)
ContinueSampler(sampler, DepthBlurHalfResTexture, DepthBlurHalfResSampler, DepthBlurHalfResTexture)
	AddressU	= CLAMP;
	AddressV	= CLAMP;
	MINFILTER	= POINT;
	MAGFILTER	= POINT;
EndSampler;



// Outputs
#define GridSize			DOF_ComputeGridSize
#define OverlapSize			DOF_MaxSampleRadius
#define TGSize				GridSize + OverlapSize * 2

#define GridSizeCOC			DOF_ComputeGridSizeCOC
#define OverlapSizeCOC		DOF_MaxSampleRadiusCOC
#define TGSizeCOC			GridSizeCOC + OverlapSizeCOC * 2

//DOF Outputs
RWTexture2D<float4> DOFOutputTexture	REGISTER2( cs_5_0, u0 );

#if ADAPTIVE_DOF_OUTPUT_UAV
StructuredBuffer<AdaptiveDOFStruct> AdaptiveDOFParamsBuffer: register(t1);
#endif //ADAPTIVE_DOF_OUTPUT_UAV

struct DOFSample
{
	float3 Color;
	float2 DepthBlur;
	float isSkyWeight;
	float padding;
};	// structure 7 floats to prevent bank conflicts

// Shared memory
groupshared DOFSample DOFSamples[TGSize];

//COC Outputs
struct CoCSample
{
	float blur;
};

RWTexture2D<float2> COCOutputTexture	REGISTER2( cs_5_0, u1 );


StructuredBuffer<float> GaussianWeights: register(t0);

// Shared memory
groupshared CoCSample COCSamples[TGSize];


BeginConstantBufferDX10( dofCompute_locals )
float	kernelRadius			: kernelRadius;
#if COC_SPREAD
float	cocSpreadKernelRadius	: cocSpreadKernelRadius;
#endif
#if !defined(SHADER_FINAL)
float cocOverlayAlpha;
#endif //SHADER_FINAL
float4 dofProjCompute;
float dofSkyWeightModifier;
float4 dofRenderTargetSize; //.zw - 1.0/size
float fourPlaneDof;
float2 LumFilterParams;
bool dofBlendParams;
EndConstantBufferDX10(dofCompute_locals)

#define dofNegativeBlurSupport	(dofBlendParams)

#define GAUSSIAN_BLUR		1

//Unrolling the coc spread loop save a large chunk of GPU time, can only do this with fixed kernel radius
//this can be done when a value has been determined.
#if 1//defined(SHADER_FINAL)
	#define COC_SPREAD_KERNEL_RADIUS	DOF_MaxSampleRadiusCOC
	#define COC_SPREAD_LOOP_UNROLL		1
#else
	#define COC_SPREAD_KERNEL_RADIUS	cocSpreadKernelRadius
	#define COC_SPREAD_LOOP_UNROLL		0
#endif

float CalcGaussianWeight(int sampleDist, int kernel, int weightOffset)
{
	return GaussianWeights[weightOffset + sampleDist];
}

#if COC_SPREAD

float DOF_COCSpread_ProcessSample(int groupIdx, float gaussianWeight, float depth, float blur, bool centerTap, out float tapWeight)
{
	// Grab the sample from shared memory
	CoCSample tap = COCSamples[groupIdx];

	// Only accept samples if they're from the foreground, and have a higher blur amount
	float depthWeight = tap.blur > 0;
	float blurWeight = saturate(tap.blur - blur);
	tapWeight = depthWeight * blurWeight;

	if( centerTap )
	{
		// If it's the center tap, set the weight to 1 so that we don't reject it
		float centerWeight = centerTap ? 1.0 : 0.0f;
		tapWeight = saturate(tapWeight + centerWeight);
	}

	#if GAUSSIAN_BLUR	
	tapWeight *= gaussianWeight;
	#endif

	return (tap.blur * tapWeight);
}

void DOF_COCSpread_compute_impl(int2 curSample, uint GroupThreadID, int gridVal, int sampleDir, int renderTargetSizeDir)
{
	const int2 outputPos = curSample;
	const float2 samplePos = float2(curSample) + float2(0.5,0.5);

		// Sample the textures
	float2 depthBlur = DepthBlurHalfResTexture.SampleLevel( DepthBlurHalfResSampler, samplePos.xy * dofRenderTargetSize.zw, 0 );
	float depth = depthBlur.x;
	float blur = depthBlur.y;

		// Store in shared memory
	COCSamples[GroupThreadID].blur = blur;

	int weightOffset = GaussianWeights[COC_SPREAD_KERNEL_RADIUS-1];
	
	GroupMemoryBarrierWithGroupSync();

	// Don't continue for threads in the overlap, and threads outside the render target size
	if(gridVal >= 0 && gridVal < GridSizeCOC && sampleDir < renderTargetSizeDir)
	{
		float outputBlur = 0.0f;
		float totalContribution = 0.0f;

		//centre sample
		int groupTapID = GroupThreadID;
#if GAUSSIAN_BLUR	
		float gaussianWeight = CalcGaussianWeight( 0, COC_SPREAD_KERNEL_RADIUS, weightOffset);
#endif
		float tapWeight = 0.0;
		outputBlur += DOF_COCSpread_ProcessSample(groupTapID, gaussianWeight, depth, blur, true, tapWeight);
		totalContribution += tapWeight;

		// Gather sample taps inside the radius
#if COC_SPREAD_LOOP_UNROLL
		[unroll]
#endif
		for(int i = 1; i <= COC_SPREAD_KERNEL_RADIUS; ++i)
		{
			// Grab the sample from shared memory
			groupTapID = GroupThreadID + i;
#if GAUSSIAN_BLUR	
			gaussianWeight = CalcGaussianWeight( abs(i), COC_SPREAD_KERNEL_RADIUS, weightOffset);
#endif
			tapWeight = 0.0;
			outputBlur += DOF_COCSpread_ProcessSample(groupTapID, gaussianWeight, depth, blur, false, tapWeight);
			totalContribution += tapWeight;

			groupTapID = GroupThreadID - i;
			outputBlur += DOF_COCSpread_ProcessSample(groupTapID, gaussianWeight, depth, blur, false, tapWeight);;
			totalContribution += tapWeight;
		}

		// Write out the result
		COCOutputTexture[outputPos] = float2(depth, outputBlur / totalContribution);
	}
}

//=================================================================================================
// Performs the horizontal CoC spread
//=================================================================================================
[numthreads(TGSizeCOC, 1, 1)]
void DOF_COCSpread_H_compute(uint3 GroupID : SV_GroupID, uint3 DispatchThreadID : SV_DispatchThreadID,
					uint3 GroupThreadID : SV_GroupThreadID, uint GroupIndex : SV_GroupIndex)
{
	// These positions are relative to the "grid", AKA the horizontal group of pixels
	// that this thread group is writing to
	const int gridStartX = GroupID.x * GridSizeCOC;
	const int gridX = GroupThreadID.x - OverlapSizeCOC;

	// These positions are relative to the pixel coordinates
	int2 curSample = int2(gridStartX + gridX, GroupID.y);

	DOF_COCSpread_compute_impl(curSample, GroupThreadID.x, gridX, curSample.x, dofRenderTargetSize.x);
}

//=================================================================================================
// Performs the vertical CoC spread
//=================================================================================================
[numthreads(1, TGSizeCOC, 1)]
void DOF_COCSpread_V_compute(uint3 GroupID : SV_GroupID, uint3 DispatchThreadID : SV_DispatchThreadID,
					uint3 GroupThreadID : SV_GroupThreadID, uint GroupIndex : SV_GroupIndex)
{
	// These positions are relative to the "grid", AKA the vertical group of pixels
	// that this thread group is writing to
	const int gridStartY = GroupID.y * GridSizeCOC;
	const int gridY = GroupThreadID.y - OverlapSizeCOC;

	// These positions are relative to the pixel coordinates
	int2 curSample = int2(GroupID.x, gridStartY + gridY);

	DOF_COCSpread_compute_impl(curSample, GroupThreadID.y, gridY, curSample.y, dofRenderTargetSize.y);
}

#endif //COC_SPREAD

float3 DOF_computeProcessSample(int groupIdx, float gaussianWeight, float depth, float blur, out float tapWeight)
{
	DOFSample tap = DOFSamples[groupIdx];

	// Reject foreground samples, unless they're blurred as well
	float depthWeight = tap.DepthBlur.x >= depth;
	float blurWeight = tap.DepthBlur.y;
	tapWeight = saturate(depthWeight + blurWeight);

	//weight the sample by the difference between this coc and the center sample
	//this reduces light bleeding at night between high and low coc's
	/*float cocDiff = abs(blur - tap.DepthBlur.y);
	float cocDiffWeight = 1.0 - (cocDiff * cocDiff * cocDiff);*/

#if GAUSSIAN_BLUR	
	tapWeight *= gaussianWeight;
#endif

	tapWeight *= tap.isSkyWeight;

	float3 colour = tap.Color * tapWeight /** cocDiffWeight*/;

	return colour;
}

//=================================================================================================
// Performs the horizontal pass for the DOF blur
//=================================================================================================
void DOF_compute_impl(int2 curSample, uint GroupThreadID, float kernelRadiusVal, int gridVal, int sampleDir, int renderTargetSizeDir, bool lumFilter)
{	
	const int2 outputPos = curSample;
	const float2 samplePos = float2(curSample) + float2(0.5,0.5);

	// Sample the textures
	float2 halfResBlur = DepthBlurHalfResTexture.SampleLevel( DepthBlurHalfResSampler, samplePos.xy * dofRenderTargetSize.zw, 0 );	
	float4 color = max(ColorTexture.SampleLevel( ColorSampler, samplePos.xy * dofRenderTargetSize.zw, 0 ), 0.0);	
	float2 depthBlur = DepthBlurTexture.SampleLevel( DepthBlurSampler, samplePos.xy * dofRenderTargetSize .zw, 0 );	
	float depth = depthBlur.x;
	float blur = depthBlur.y;	
	
	float4 colorB = color;

	//The sky causes thin objects like trees to get even thinner and more fizzy so scale the blur weight when we're
	//sampling from the sky. Need to convert the depth back to non linear to check for the sky.
	float sampleDepth = getSampleDepth(depth, dofProjCompute.zw);
	float isSkyWeight = 1.0;
	if( sampleDepth >= 1.0)
	{
		isSkyWeight = dofSkyWeightModifier;
	}

	//If halfResBlur.y is none zero this ia  foreground pixel so use the blur value from that.
	[branch] //This doesnt work correctly if I dont add branch to this
	if( halfResBlur.y > 0.0 && halfResBlur.y > blur )
	{
		blur = halfResBlur.y;	
	}

	float kernelSize = (blur * kernelRadiusVal);	
	//If we're using four plane dof take the ceil of the value
	int cocSize = fourPlaneDof ? ceil(kernelSize) : kernelSize;


	// Store in shared memory
	DOFSamples[GroupThreadID].Color = color.rgb;
	DOFSamples[GroupThreadID].DepthBlur.x = depth;
	DOFSamples[GroupThreadID].DepthBlur.y = blur;
	DOFSamples[GroupThreadID].isSkyWeight = isSkyWeight;

	int weightOffset = GaussianWeights[cocSize-1];

	GroupMemoryBarrierWithGroupSync();

	if(lumFilter) 
	{
		if(cocSize > 1)
		{
			const float3 lumParams = float3(0.299,0.587,0.114);
			uint dofLeft = clamp(GroupThreadID-1U, 0U, (uint)(TGSize-1));
			uint dofRight = clamp(GroupThreadID+1U, 0U, (uint)(TGSize-1));
			float3 clrLeft = DOFSamples[dofLeft].Color;
			float3 clrRight = DOFSamples[dofRight].Color;

			float lumLeft = dot(clrLeft, lumParams);
			float lumRight = dot(clrRight, lumParams);
			float lumC = dot(color.rgb, lumParams);

			float maxLum = max(lumLeft, max(lumRight, lumC));	
			float minLum = min(lumLeft, min(lumRight, lumC));	
			
			float4 clrUp = ColorTexture.SampleLevel( ColorSampler, (samplePos.xy + int2(0,1)) * dofRenderTargetSize.zw, 0 );	
			float4 clrDown = ColorTexture.SampleLevel( ColorSampler, (samplePos.xy + int2(0,-1)) * dofRenderTargetSize.zw, 0 );	

			float lumUp = dot(clrUp.rgb, lumParams);
			float lumDown = dot(clrDown.rgb, lumParams);
			
			maxLum = max(maxLum, max(lumUp, lumDown));
			minLum = min(minLum, min(lumUp, lumDown));

			float lerpv = saturate(maxLum / max(minLum, 1e-9) - LumFilterParams.x);			
			lerpv *= lerpv;

			float clr_max = max(color.r, max(color.g, color.b));
			float3 clr_scale = color.rgb / max(clr_max, 1e-9);			
			DOFSamples[GroupThreadID].Color.rgb = lerp(color.rgb, min(color.rgb, LumFilterParams.y * clr_scale), lerpv);
		}
		GroupMemoryBarrierWithGroupSync();		
	}	

	// Don't continue for threads in the overlap, and threads outside the render target size
	if(gridVal >= 0 && gridVal < GridSize && sampleDir < renderTargetSizeDir)
	{
		[branch]
		if(cocSize > 0)
		{
			float3 outputColor = 0.0f;
			float totalContribution = 0.0f;

			//centre sample
			int groupTapID = GroupThreadID;					
	#if GAUSSIAN_BLUR	
			float gaussianWeight = CalcGaussianWeight( 0, cocSize, weightOffset);					
	#endif
			float tapWeight = 0.0f;
			outputColor += DOF_computeProcessSample(groupTapID, gaussianWeight, depth, blur, tapWeight);
			totalContribution += tapWeight;

			// Gather sample taps inside the radius
			// unroll a fixed size loop and rejecting samples outside the coc is quicker than looping the correct size.
			[unroll]
			for(int i = 1; i <= DOF_MaxSampleRadius; ++i)
			{
				if( i <= cocSize)
				{
					groupTapID = GroupThreadID + i;					
	#if GAUSSIAN_BLUR	
					gaussianWeight = CalcGaussianWeight( i, cocSize, weightOffset);					
	#endif
					tapWeight = 0.0f;
					outputColor += DOF_computeProcessSample(groupTapID, gaussianWeight, depth, blur, tapWeight);
					totalContribution += tapWeight;

					groupTapID = GroupThreadID - i;					
					outputColor += DOF_computeProcessSample(groupTapID, gaussianWeight, depth, blur, tapWeight);
					totalContribution += tapWeight;
				}
			}

			// Write out the result
			DOFOutputTexture[outputPos] = float4(outputColor / totalContribution, 1 );
		}
		else
			DOFOutputTexture[outputPos] = float4(colorB);
	}
}

[numthreads(TGSize,1,1)]
void DOF_H_compute(uint3 GroupID : SV_GroupID, uint3 DispatchThreadID : SV_DispatchThreadID,
					uint3 GroupThreadID : SV_GroupThreadID, uint GroupIndex : SV_GroupIndex)
{
	// These positions are relative to the "grid", AKA the horizontal group of pixels
	// that this thread group is writing to
	const int gridStartX = GroupID.x * GridSize;
	const int gridX = GroupThreadID.x - OverlapSize;
	// These positions are relative to the pixel coordinates
	int2 curSample = int2(gridStartX + gridX, GroupID.y);

	DOF_compute_impl(curSample, GroupThreadID.x, kernelRadius, gridX, curSample.x, dofRenderTargetSize.x, false);
}

[numthreads(TGSize,1,1)]
void DOF_H_compute_LumFilter(uint3 GroupID : SV_GroupID, uint3 DispatchThreadID : SV_DispatchThreadID,
					uint3 GroupThreadID : SV_GroupThreadID, uint GroupIndex : SV_GroupIndex)
{
	// These positions are relative to the "grid", AKA the horizontal group of pixels
	// that this thread group is writing to
	const int gridStartX = GroupID.x * GridSize;
	const int gridX = GroupThreadID.x - OverlapSize;
	// These positions are relative to the pixel coordinates
	int2 curSample = int2(gridStartX + gridX, GroupID.y);

	DOF_compute_impl(curSample, GroupThreadID.x, kernelRadius, gridX, curSample.x, dofRenderTargetSize.x, true);
}

#if ADAPTIVE_DOF_OUTPUT_UAV
[numthreads(TGSize,1,1)]
void DOF_H_compute_Adaptive(uint3 GroupID : SV_GroupID, uint3 DispatchThreadID : SV_DispatchThreadID,
					uint3 GroupThreadID : SV_GroupThreadID, uint GroupIndex : SV_GroupIndex)
{
	// These positions are relative to the "grid", AKA the horizontal group of pixels
	// that this thread group is writing to
	const int gridStartX = GroupID.x * GridSize;
	const int gridX = GroupThreadID.x - OverlapSize;
	// These positions are relative to the pixel coordinates
	int2 curSample = int2(gridStartX + gridX, GroupID.y);

	DOF_compute_impl(curSample, GroupThreadID.x, (float)DOF_MaxSampleRadius, gridX, curSample.x, dofRenderTargetSize.x, false);
}

[numthreads(TGSize,1,1)]
void DOF_H_compute_Adaptive_LumFilter(uint3 GroupID : SV_GroupID, uint3 DispatchThreadID : SV_DispatchThreadID,
					uint3 GroupThreadID : SV_GroupThreadID, uint GroupIndex : SV_GroupIndex)
{
	// These positions are relative to the "grid", AKA the horizontal group of pixels
	// that this thread group is writing to
	const int gridStartX = GroupID.x * GridSize;
	const int gridX = GroupThreadID.x - OverlapSize;
	// These positions are relative to the pixel coordinates
	int2 curSample = int2(gridStartX + gridX, GroupID.y);

	DOF_compute_impl(curSample, GroupThreadID.x, (float)DOF_MaxSampleRadius, gridX, curSample.x, dofRenderTargetSize.x, true);
}
#endif //ADAPTIVE_DOF_OUTPUT_UAV


[numthreads(1, TGSize, 1)]
void DOF_V_compute(uint3 GroupID : SV_GroupID, uint3 DispatchThreadID : SV_DispatchThreadID,
					uint3 GroupThreadID : SV_GroupThreadID, uint GroupIndex : SV_GroupIndex)
{
	// These positions are relative to the "grid", AKA the horizontal group of pixels
	// that this thread group is writing to
	const int gridStartY = GroupID.y * GridSize;
	const int gridY = GroupThreadID.y - OverlapSize;
	// These positions are relative to the pixel coordinates
	int2 curSample = int2(GroupID.x, gridStartY + gridY);

	DOF_compute_impl(curSample, GroupThreadID.y, kernelRadius, gridY, curSample.y, dofRenderTargetSize.y, false);
}

#if ADAPTIVE_DOF_OUTPUT_UAV
[numthreads(1, TGSize, 1)]
void DOF_V_compute_Adaptive(uint3 GroupID : SV_GroupID, uint3 DispatchThreadID : SV_DispatchThreadID,
					uint3 GroupThreadID : SV_GroupThreadID, uint GroupIndex : SV_GroupIndex)
{
	// These positions are relative to the "grid", AKA the horizontal group of pixels
	// that this thread group is writing to
	const int gridStartY = GroupID.y * GridSize;
	const int gridY = GroupThreadID.y - OverlapSize;
	// These positions are relative to the pixel coordinates
	int2 curSample = int2(GroupID.x, gridStartY + gridY);

	DOF_compute_impl(curSample, GroupThreadID.y, (float)DOF_MaxSampleRadius, gridY, curSample.y, dofRenderTargetSize.y, false);
}
#endif //ADAPTIVE_DOF_OUTPUT_UAV



struct VertexOut
{
    DECLARE_POSITION(Pos)
	float2 texCoord	: TEXCOORD0;
};

struct PixelIn
{
    DECLARE_POSITION_PSIN(Pos)
	inside_sample float2 texCoord	: TEXCOORD0;
	uint sampleIndex				: SV_SampleIndex;
};

struct PixelInMS0
{
	DECLARE_POSITION_PSIN(Pos)
	float2 texCoord	: TEXCOORD0;
};

VertexOut DOF_Quad(float2 Position : POSITION)
{
	VertexOut OUT = {
		float4(Position * 2 - 1, 0, 1),
		float2(Position.x, 1 - Position.y),
	};
	return OUT;
}

float blur2alpha(float blur)
{
	//return step(1.0,blur);
	rageDiscard(blur<=0.5);
	return smoothstep(0.5,1.5,blur);
}

float4 DOF_Blend_impl(PixelIn input, float kernelRadiusVal)
{
	float4 tc = float4(input.texCoord, 0, 0);
	float2 depthBlur = tex2Dlod(DepthBlurSampler, tc).xy;
	if (dofNegativeBlurSupport)	//near DoF
		depthBlur.y = abs(depthBlur.y);
	float4 color = tex2Dlod(ColorSampler, tc);
	float alpha = blur2alpha( depthBlur.y * kernelRadiusVal);
#if RSG_ORBIS
	// force the supersampling, since Orbis doesn't recognize 'sample' interpolator yet
	alpha += input.sampleIndex * 0.00001;
#endif
	return float4(color.xyz, alpha);
}

float4 DOF_Blend(PixelIn input) : SV_Target
{
	return DOF_Blend_impl(input, kernelRadius);
}

float4 DOF_BlendMS0(PixelInMS0 inMS0) : SV_Target
{
	PixelIn input = { inMS0.Pos, inMS0.texCoord, 0 };
	return DOF_Blend_impl(input, kernelRadius);
}

#if ADAPTIVE_DOF_OUTPUT_UAV
float4 DOF_Blend_Adaptive(PixelIn input) : SV_Target
{
	return DOF_Blend_impl(input, (float)DOF_MaxSampleRadius);
}

float4 DOF_Blend_AdaptiveMS0(PixelInMS0 inMS0) : SV_Target
{
	PixelIn input = { inMS0.Pos, inMS0.texCoord, 0 };
	return DOF_Blend_impl(input, (float)DOF_MaxSampleRadius);
}
#endif //ADAPTIVE_DOF_OUTPUT_UAV

#if !defined(SHADER_FINAL)
float4 DOF_COC_Overlay_impl(PixelIn input, float kernelRadiusVal)
{	
	float4 tc = float4(input.texCoord, 0, 0);
	float2 depthBlur = tex2Dlod(DepthBlurSampler, tc).xy;
	float blur = abs(depthBlur.y);

	float kernelSize = (blur * kernelRadiusVal);
	//If we're using four plane dof take the ceil of the value
	int cocSize = fourPlaneDof ? ceil(kernelSize) : kernelSize;

	float cocFloat = (float)cocSize / (float)kernelRadiusVal;

	float3 color = lerp(float3(1.0f, 1.0f, 0.0f), float3(1.0f, 0.0f, 0.0f), cocFloat);
	float alpha = cocSize == 0 ? 0.0f : cocOverlayAlpha;
	return float4(color, alpha);
}

float4 DOF_COC_Overlay(PixelIn input) : SV_Target
{
	return DOF_COC_Overlay_impl(input, kernelRadius);
}

#if ADAPTIVE_DOF_OUTPUT_UAV
float4 DOF_COC_Overlay_Adaptive(PixelIn input) : SV_Target
{
	return DOF_COC_Overlay_impl(input, (float)DOF_MaxSampleRadius);
}
#endif //ADAPTIVE_DOF_OUTPUT_UAV
#endif


technique11 DOF_Compute
{
#if COC_SPREAD
	pass DOF_COCSpread_H_compute_sm50
	{
		SetComputeShader(	compileshader( cs_5_0, DOF_COCSpread_H_compute() ));
	}
	pass DOF_COCSpread_V_compute_sm50
	{
		SetComputeShader(	compileshader( cs_5_0, DOF_COCSpread_V_compute() ));
	}
#endif

	pass DOF_H_compute_sm50
	{
		SetComputeShader(	compileshader( cs_5_0, DOF_H_compute() ));
	}
	pass DOF_V_compute_sm50
	{
		SetComputeShader(	compileshader( cs_5_0, DOF_V_compute() ));
	}

#if ADAPTIVE_DOF_OUTPUT_UAV
	pass DOF_H_compute_Adaptive_sm50
	{
		SetComputeShader(	compileshader( cs_5_0, DOF_H_compute_Adaptive() ));
	}
	pass DOF_V_compute_Adaptive_sm50
	{
		SetComputeShader(	compileshader( cs_5_0, DOF_V_compute_Adaptive() ));
	}
	pass DOF_H_compute_Adaptive_LumFilter_sm50
	{
		SetComputeShader(	compileshader( cs_5_0, DOF_H_compute_Adaptive_LumFilter() ));
	}
#endif //ADAPTIVE_DOF_OUTPUT_UAV
	pass DOF_H_compute_LumFilter_sm50
	{
		SetComputeShader(	compileshader( cs_5_0, DOF_H_compute_LumFilter() ));
	}
	pass DOF_MSAA_apply_sm50
	{
		VertexShader = compile VERTEXSHADER	DOF_Quad();
		PixelShader  = compile ps_5_0		DOF_Blend()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
	
	pass DOF_MSAA_applyMS0_sm50
	{
		VertexShader = compile VERTEXSHADER	DOF_Quad();
		PixelShader  = compile ps_5_0		DOF_BlendMS0()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

#if ADAPTIVE_DOF_OUTPUT_UAV
	pass DOF_MSAA_apply_Adaptive_sm50
	{
		VertexShader = compile VERTEXSHADER	DOF_Quad();
		PixelShader  = compile ps_5_0		DOF_Blend_Adaptive()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
	pass DOF_MSAA_apply_AdaptiveMS0_sm50
	{
		VertexShader = compile VERTEXSHADER	DOF_Quad();
		PixelShader  = compile ps_5_0		DOF_Blend_AdaptiveMS0()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
#endif //ADAPTIVE_DOF_OUTPUT_UAV

#if !defined(SHADER_FINAL)
	pass DOF_COC_Overlay_sm50
	{
		VertexShader = compile VERTEXSHADER	DOF_Quad();
		PixelShader  = compile ps_5_0		DOF_COC_Overlay()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

#if ADAPTIVE_DOF_OUTPUT_UAV
	pass DOF_COC_Overlay_Adaptive_sm50
	{
		VertexShader = compile VERTEXSHADER	DOF_Quad();
		PixelShader  = compile ps_5_0		DOF_COC_Overlay_Adaptive()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
#endif //ADAPTIVE_DOF_OUTPUT_UAV
#endif
}

#else
technique dummy{ pass dummy	{} }
#endif //DOF_COMPUTE_GAUSSIAN || DOF_DIFFUSION
