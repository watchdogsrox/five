
#pragma dcl position
#include "../../renderer/PostFX_shared.h"
#include "../common.fxh"

//see efficient compute shader programming AMD powerpoint for more optimisations.

#if AVG_LUMINANCE_COMPUTE

// Inputs
BeginDX10Sampler(sampler, Texture2D<float>, AvgLuminanceTexture, AvgLuminanceSampler, AvgLuminanceTexture)
ContinueSampler(sampler, AvgLuminanceTexture, AvgLuminanceSampler, AvgLuminanceTexture)
	AddressU	= CLAMP;
	AddressV	= CLAMP;
	MINFILTER	= POINT;
	MAGFILTER	= POINT;
EndSampler;

BeginDX10Sampler(sampler, Texture2D<float4>, HDRColorTexture, HDRColorSampler, HDRColorTexture)
ContinueSampler(sampler, HDRColorTexture, HDRColorSampler, HDRColorTexture)
	AddressU	= CLAMP;
	AddressV	= CLAMP;
	MINFILTER	= POINT;
	MAGFILTER	= POINT;
EndSampler;

BeginConstantBufferDX10( avgLuminance_locals )
float4 DstTextureSize : DstTextureSize;
EndConstantBufferDX10( avgLuminance_locals )

#define THREAD_GROUP_SIZE 16

static const uint TotalNumThreads = THREAD_GROUP_SIZE * THREAD_GROUP_SIZE ;
groupshared float LuminanceSharedMem[TotalNumThreads];

RWTexture2D<float> AverageLuminanceOut : register(u1);

void LuminanceDownsample(uint3 GroupID, uint3 GroupThreadID, bool init)
{
	const uint ThreadIdx = GroupThreadID.y * THREAD_GROUP_SIZE + GroupThreadID.x;
	const uint2 SampleIdx = (GroupID.xy * THREAD_GROUP_SIZE + GroupThreadID.xy);

	float PixCount = 1.0f;
	if(init)
	{
		PixCount = 1.0f / (DstTextureSize.z * DstTextureSize.w);	
	}

	if (SampleIdx.x < (uint)DstTextureSize.z && SampleIdx.y < (uint)DstTextureSize.w)
	{
		float Luminance;

		if(init)
		{
			float3 HdrColor = HDRColorTexture[SampleIdx.xy];
			Luminance = dot(HdrColor, LumFactors);
			LuminanceSharedMem[ThreadIdx] = (isnan(Luminance) || isinf(Luminance)) ? 0.0f : (Luminance);		
		}
		else
		{
			Luminance = AvgLuminanceTexture[SampleIdx.xy];
			LuminanceSharedMem[ThreadIdx] = (Luminance);
		}
		
	}
	else
	{
		LuminanceSharedMem[ThreadIdx] = 0.0f;
	}

	GroupMemoryBarrierWithGroupSync();

	[unroll]
	for(uint s = TotalNumThreads / 2; s > 0; s >>= 1)
	{
		if(ThreadIdx < s)
		{
			
			LuminanceSharedMem[ThreadIdx] += LuminanceSharedMem[ThreadIdx + s];
		}

		GroupMemoryBarrierWithGroupSync();
	}
	
	if(ThreadIdx == 0)
	{		
		float avg_luminance = (LuminanceSharedMem[0]);
		AverageLuminanceOut[GroupID.xy] = avg_luminance * PixCount;
	}
}

[numthreads(THREAD_GROUP_SIZE, THREAD_GROUP_SIZE, 1)]
void LuminanceDownsampleInitCS(uint3 GroupID : SV_GroupID, uint3 GroupThreadID : SV_GroupThreadID)
{
	LuminanceDownsample(GroupID, GroupThreadID, true);
}

[numthreads(THREAD_GROUP_SIZE, THREAD_GROUP_SIZE, 1)]
void LuminanceDownsampleCS(uint3 GroupID : SV_GroupID, uint3 GroupThreadID : SV_GroupThreadID)
{	
	LuminanceDownsample(GroupID, GroupThreadID, false);	
}


technique11 AvgLuminanceTechnique
{
	pass pp_luminance_downsample_init_sm50
	{
		SetComputeShader(	compileshader( cs_5_0, LuminanceDownsampleInitCS() ));
	}
	pass pp_luminance_downsample_sm50
	{
		SetComputeShader(	compileshader( cs_5_0, LuminanceDownsampleCS() ));
	}
}

#else
technique dummy{ pass dummy	{} }
#endif //AVG_LUMINANCE_COMPUTE
