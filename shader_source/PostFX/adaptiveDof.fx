
#pragma dcl position

#include "../../renderer/PostFX_shared.h"
#include "../common.fxh"
#include "../../../rage/base/src/grcore/fastquad_switch.h"

#if ADAPTIVE_DOF_GPU

// Constants
#define TGSize_ 16
static const uint TotalNumThreads = TGSize_ * TGSize_;

struct adaptiveDOFVertexOutputPassThrough {
    DECLARE_POSITION(pos)
    float2 tex	: TEXCOORD0;
};

//needs to match up with eDofPlane in camera/frame.h
#define NEAR_OUT_OF_FOCUS_DOF_PLANE 0
#define NEAR_IN_FOCUS_DOF_PLANE		1
#define FAR_IN_FOCUS_DOF_PLANE		2
#define FAR_OUT_OF_FOCUS_DOF_PLANE	3
#define NUM_DOF_PLANES				4

#define CIRCLE_OF_CONFUSION_FOR_35MM			(0.000029f)
// #define MIN_NEAR_IN_FOCUS_DOF_PLANE_DISTANCE	(0.01f)
#define MAX_BLUR_RADIUS_RATIO					(0.9f)
#define	MAX_ADAPTIVE_DOF_PLANE_DISTANCE			(10000.0f)
#define	SQRT_TWO								(1.4142f)

BeginConstantBufferPagedDX10(postfx_cbuffer_adaptiveDOF, b7)

float4 AdaptiveDofDepthDownSampleParams;
float4 AdaptiveDofParams0;
float4 AdaptiveDofParams1;
float4 AdaptiveDofParams2;
float4 AdaptiveDofParams3;
float4 AdaptiveDofFocusDistanceDampingParams1;
float4 AdaptiveDofFocusDistanceDampingParams2;

#if POSTFX_UNIT_QUAD
float4 QuadPosition;
float4 QuadTexCoords;
float4 QuadScale;
#define VIN	float2 pos :POSITION
#else
#define VIN	rageVertexInput IN
#endif	//POSTFX_UNIT_QUAD

float4 adapDOFProj;

EndConstantBufferDX10(postfx_cbuffer_adaptiveDOF)

#define AdaptiveDofMaxDepthToConsider					AdaptiveDofDepthDownSampleParams.x
#define AdaptiveDofDepthPowerFactor						AdaptiveDofDepthDownSampleParams.y
#define AdaptiveDofMaxDepthBlendLevel					AdaptiveDofDepthDownSampleParams.z

#define AdaptiveDofFNumberOfLens						AdaptiveDofParams0.x
#define AdaptiveDofFocalLengthOfLens					AdaptiveDofParams0.y
#define AdaptiveDofOverriddenFocusDistance				AdaptiveDofParams0.z
#define AdaptiveDofOverriddenFocusDistanceBlendLevel	AdaptiveDofParams0.w
#define AdaptiveDofFocusDistanceBias					AdaptiveDofParams1.x
#define AdaptiveDofMagnificationOfSubjectPowerNear		AdaptiveDofParams1.y
#define AdaptiveDofMagnificationOfSubjectPowerFar		AdaptiveDofParams1.z
//Unused												AdaptiveDofParams1.w
#define AdaptiveDofMaxNearInFocusDistance				AdaptiveDofParams2.x
//Unused												AdaptiveDofParams2.y
#define AdaptiveDofMaxBlurRadiusAtNearInFocusLimit		AdaptiveDofParams2.z
#define AdaptiveDofMaxNearInFocusDistanceBlendLevel		AdaptiveDofParams2.w
#define AdaptiveDofMinExposure							AdaptiveDofParams3.x
#define AdaptiveDofMaxExposure							AdaptiveDofParams3.y
#define AdaptiveDofFStopsAtMaxExposure					AdaptiveDofParams3.z

#define AdaptiveDofFocusDistanceIncreaseSpringConstant	AdaptiveDofFocusDistanceDampingParams1.x
#define AdaptiveDofFocusDistanceIncreaseDampingRatio	AdaptiveDofFocusDistanceDampingParams1.y
#define AdaptiveDofFocusDistanceDecreaseSpringConstant	AdaptiveDofFocusDistanceDampingParams1.z
#define AdaptiveDofFocusDistanceDecreaseDampingRatio	AdaptiveDofFocusDistanceDampingParams1.w
#define AdaptiveDofFocusDistanceDampingTimeStep			AdaptiveDofFocusDistanceDampingParams2.x

#if ADAPTIVE_DOF_OUTPUT_UAV
// Structure used for outputing bokeh points to an AppendStructuredBuffer
RWStructuredBuffer<AdaptiveDOFStruct> AdaptiveDOFOutputBuffer: register(u2);
#endif //ADAPTIVE_DOF_OUTPUT_UAV

//=================================================================================================
// Resources
//================================================================================================
BeginDX10Sampler(sampler, Texture2D<float>, reductionDepthTexture, reductionDepthSampler, reductionDepthTexture)
ContinueSampler(sampler, reductionDepthTexture, reductionDepthSampler, reductionDepthTexture)
	AddressU	= CLAMP;
	AddressV	= CLAMP;
	MINFILTER	= LINEAR;
	MAGFILTER	= LINEAR;
EndSampler;

BeginDX10Sampler(	sampler, Texture2D<float>, adaptiveDOFTextureDepth, adaptiveDOFSamplerDepth, adaptiveDOFTextureDepth)
ContinueSampler(sampler, adaptiveDOFTextureDepth, adaptiveDOFSamplerDepth, adaptiveDOFTextureDepth)
	AddressU  = CLAMP;
	AddressV  = CLAMP;
	MINFILTER = POINT;
	MAGFILTER = POINT;
	MIPFILTER = POINT;
EndSampler;

BeginSampler(sampler2D,adaptiveDOFTexture,adaptiveDOFSampler,adaptiveDOFTexture)
ContinueSampler(sampler2D,adaptiveDOFTexture,adaptiveDOFSampler,adaptiveDOFTexture)
	MinFilter = LINEAR;
	MagFilter = LINEAR;
	MipFilter = LINEAR;
   AddressU  = CLAMP;
   AddressV  = CLAMP;
EndSampler;

BeginSampler(sampler2D,adaptiveDOFExposureTexture,adaptiveDOFExposureSampler,adaptiveDOFExposureTexture)
ContinueSampler(sampler2D,adaptiveDOFExposureTexture,adaptiveDOFExposureSampler,adaptiveDOFExposureTexture)
	MinFilter = LINEAR;
	MagFilter = LINEAR;
	MipFilter = LINEAR;
   AddressU  = CLAMP;
   AddressV  = CLAMP;
EndSampler;


RWTexture2D<float> ReductionOutputTexture : register(u1);

// Shared memory
groupshared float4 ReductionSharedMem[TotalNumThreads];

//=================================================================================================
// Reduces by TGSize_ x TGSize_
//=================================================================================================
[numthreads(TGSize_, TGSize_, 1)]
void ReductionCS(uint3 GroupID : SV_GroupID, uint3 GroupThreadID : SV_GroupThreadID)
{
	const uint ThreadIdx = GroupThreadID.y * TGSize_ + GroupThreadID.x;

	const uint2 SampleIdx = (GroupID.xy * TGSize_ + GroupThreadID.xy) * 2;
	float4 sampleD = 0.0f;
	sampleD.x = reductionDepthTexture[SampleIdx + uint2(0, 0)];
	sampleD.y = reductionDepthTexture[SampleIdx + uint2(1, 0)];
	sampleD.z = reductionDepthTexture[SampleIdx + uint2(0, 1)];
	sampleD.w = reductionDepthTexture[SampleIdx + uint2(1, 1)];

	// Store in shared memory
	ReductionSharedMem[ThreadIdx] = sampleD;
	GroupMemoryBarrierWithGroupSync();

	// Parallel reduction
#if RSG_ORBIS
	[unroll]
#else
	[unroll(TotalNumThreads)]
#endif
	for(uint s = TotalNumThreads / 2; s > 0; s >>= 1)
	{
		if(ThreadIdx < s)
			ReductionSharedMem[ThreadIdx] += ReductionSharedMem[ThreadIdx + s];

		GroupMemoryBarrierWithGroupSync();
	}

	// Have the first thread write out to the output texture
	if(ThreadIdx == 0)
		ReductionOutputTexture[GroupID.xy] =  dot(ReductionSharedMem[0], 0.25f) / TotalNumThreads;
}


adaptiveDOFVertexOutputPassThrough VS_PassthroughComposite(VIN)
{
	adaptiveDOFVertexOutputPassThrough OUT = (adaptiveDOFVertexOutputPassThrough)0 ;
	
	//scale here
#if POSTFX_UNIT_QUAD
	OUT.pos = float4( QuadTransform(pos * QuadScale,QuadPosition), 0, 1 );
	OUT.tex.xy = QuadTransform(pos,QuadTexCoords);
#else
	OUT.pos = float4(IN.pos, 1.0f);
    OUT.tex.xy = IN.texCoord0;
#endif

    return OUT;
}

float PS_DepthDownsample(adaptiveDOFVertexOutputPassThrough IN) : COLOR
{
	float d = tex2D(adaptiveDOFSamplerDepth,IN.tex).x;;
	float depth = getLinearGBufferDepth(d, adapDOFProj.zw);

	float limitedDepth = min(depth, AdaptiveDofMaxDepthToConsider);
	depth = lerp(depth, limitedDepth, AdaptiveDofMaxDepthBlendLevel);
	depth = pow(depth, AdaptiveDofDepthPowerFactor);

	return depth;
}

float4 PS_adaptiveDOFcalc(adaptiveDOFVertexOutputPassThrough IN) : COLOR
{
	float distanceToSubject = tex2D( adaptiveDOFSampler, IN.tex).r;

#if ADAPTIVE_DOF_OUTPUT_UAV
	//Apply damping to the *measured* distance to subject.

	distanceToSubject = max(distanceToSubject, 1.0f);
	distanceToSubject = log10(distanceToSubject);

	const float intialDisplacement		= AdaptiveDOFOutputBuffer[0].springResult - distanceToSubject;
	const float springConstantToApply	= (intialDisplacement >= 0.0f) ? AdaptiveDofFocusDistanceDecreaseSpringConstant : AdaptiveDofFocusDistanceIncreaseSpringConstant;
	const float dampingRatioToApply		= (intialDisplacement >= 0.0f) ? AdaptiveDofFocusDistanceDecreaseDampingRatio : AdaptiveDofFocusDistanceIncreaseDampingRatio;

	//If spring constant is too small, snap to the target.
	if(springConstantToApply < ADAPTIVE_DOF_SMALL_FLOAT)
	{
		AdaptiveDOFOutputBuffer[0].springResult		= distanceToSubject;
		AdaptiveDOFOutputBuffer[0].springVelocity	= 0.0f;
	}
	else if(AdaptiveDofFocusDistanceDampingTimeStep >= ADAPTIVE_DOF_SMALL_FLOAT)
	{
		//NOTE: The spring equations have been normalised for unit mass.
		const float halfDampingCoeff	= dampingRatioToApply * sqrt(springConstantToApply);
		const float omegaSqr			= springConstantToApply - (halfDampingCoeff * halfDampingCoeff);

		float springDisplacementToApply	= 0.0f;
		if(omegaSqr >= ADAPTIVE_DOF_SMALL_FLOAT)
		{
			//Under-damped.
			const float omega							= sqrt(omegaSqr);
			const float c								= (AdaptiveDOFOutputBuffer[0].springVelocity + (halfDampingCoeff * intialDisplacement)) / omega;
			springDisplacementToApply					= exp(-halfDampingCoeff * AdaptiveDofFocusDistanceDampingTimeStep) * ((intialDisplacement * cos(omega * AdaptiveDofFocusDistanceDampingTimeStep)) +
															(c * sin(omega * AdaptiveDofFocusDistanceDampingTimeStep)));
			AdaptiveDOFOutputBuffer[0].springVelocity	= exp(-halfDampingCoeff * AdaptiveDofFocusDistanceDampingTimeStep) * ((-halfDampingCoeff *
															((intialDisplacement * cos(omega * AdaptiveDofFocusDistanceDampingTimeStep)) + (c * sin(omega * AdaptiveDofFocusDistanceDampingTimeStep)))) +
															(omega * ((c * cos(omega * AdaptiveDofFocusDistanceDampingTimeStep)) - (intialDisplacement * sin(omega * AdaptiveDofFocusDistanceDampingTimeStep)))));
		}
		else
		{
			//Critically- or over-damped.
			//NOTE: We must negate (and clamp) the square omega term and use an alternate equation to avoid computing the square root of a
			//negative number.
			const float negatedOmegaSqr					= max(-omegaSqr, 0.0f);
			const float omega							= sqrt(negatedOmegaSqr);
			springDisplacementToApply					= intialDisplacement * exp((-halfDampingCoeff + omega) * AdaptiveDofFocusDistanceDampingTimeStep);
			AdaptiveDOFOutputBuffer[0].springVelocity	= (-halfDampingCoeff + omega) * springDisplacementToApply;
		}

		AdaptiveDOFOutputBuffer[0].springResult = distanceToSubject + springDisplacementToApply;
	}

	distanceToSubject = pow(10.0f, AdaptiveDOFOutputBuffer[0].springResult);
#endif //ADAPTIVE_DOF_OUTPUT_UAV

	distanceToSubject = lerp(distanceToSubject, AdaptiveDofOverriddenFocusDistance, AdaptiveDofOverriddenFocusDistanceBlendLevel);

 	float exposureValueToConsider = tex2D(adaptiveDOFExposureSampler, IN.tex).y;

	float fStopsToApplyForExposure = 0.0f;
	float exposureRange = AdaptiveDofMaxExposure - AdaptiveDofMinExposure;
	if(exposureRange >= ADAPTIVE_DOF_SMALL_FLOAT)
	{
		float exposureTValue = clamp((exposureValueToConsider - AdaptiveDofMinExposure) / exposureRange, 0.0f, 1.0f);
		fStopsToApplyForExposure = exposureTValue * AdaptiveDofFStopsAtMaxExposure;
	}
	else if(exposureValueToConsider >= AdaptiveDofMaxExposure)
	{
		fStopsToApplyForExposure = AdaptiveDofFStopsAtMaxExposure;
	}

	float fStopsOfLens						= log(AdaptiveDofFNumberOfLens) / log(SQRT_TWO);
	float exposureCompensatedFStopsOfLens	= fStopsOfLens - fStopsToApplyForExposure;
	float exposureCompensatedFNumberOfLens	= pow(SQRT_TWO, exposureCompensatedFStopsOfLens);

	distanceToSubject						+= AdaptiveDofFocusDistanceBias;
	distanceToSubject						= max(distanceToSubject, AdaptiveDofFocalLengthOfLens * 2.0f);

	float magnificationOfSubject			= AdaptiveDofFocalLengthOfLens / (distanceToSubject - AdaptiveDofFocalLengthOfLens);

	float magnificationOfSubjectNear		= pow(magnificationOfSubject, AdaptiveDofMagnificationOfSubjectPowerNear);
	float effectiveFocalLengthOfLensNear	= magnificationOfSubjectNear * distanceToSubject / (magnificationOfSubjectNear + 1.0f);
	float magnificationOfSubjectFar			= pow(magnificationOfSubject, AdaptiveDofMagnificationOfSubjectPowerFar);
	float effectiveFocalLengthOfLensFar		= magnificationOfSubjectFar * distanceToSubject / (magnificationOfSubjectFar + 1.0f);

	float minDistanceToSubject				= 2.0f * max(effectiveFocalLengthOfLensNear, effectiveFocalLengthOfLensFar);
	distanceToSubject						= max(distanceToSubject, minDistanceToSubject);

#if ADAPTIVE_DOF_OUTPUT_UAV
	AdaptiveDOFOutputBuffer[0].distanceToSubject		= distanceToSubject;
	float denominator									= exposureCompensatedFNumberOfLens * CIRCLE_OF_CONFUSION_FOR_35MM;
	AdaptiveDOFOutputBuffer[0].maxBlurDiskRadiusNear	= effectiveFocalLengthOfLensNear * magnificationOfSubjectNear / denominator;
	AdaptiveDOFOutputBuffer[0].maxBlurDiskRadiusFar		= effectiveFocalLengthOfLensFar * magnificationOfSubjectFar / denominator;

	float maxNearInFocusDistanceBlendLevelToApply		= sqrt(AdaptiveDofMaxNearInFocusDistanceBlendLevel);

	//Take account of any restriction on the maximum near in-focus plane when computing the max near/foreground blur disk radius.
	float maxNearInFocusDistanceToConsider				= max(AdaptiveDofMaxNearInFocusDistance, ADAPTIVE_DOF_SMALL_FLOAT);
	float nearDenominator								= max((distanceToSubject / maxNearInFocusDistanceToConsider) - 1.0f, ADAPTIVE_DOF_SMALL_FLOAT);
	float limitedMaxBlurDiskRadiusNear					= AdaptiveDofMaxBlurRadiusAtNearInFocusLimit / nearDenominator;
	float desiredMaxBlurDiskRadiusNear					= min(AdaptiveDOFOutputBuffer[0].maxBlurDiskRadiusNear, limitedMaxBlurDiskRadiusNear);
	AdaptiveDOFOutputBuffer[0].maxBlurDiskRadiusNear	= lerp(AdaptiveDOFOutputBuffer[0].maxBlurDiskRadiusNear, desiredMaxBlurDiskRadiusNear, maxNearInFocusDistanceBlendLevelToApply);
#endif //ADAPTIVE_DOF_OUTPUT_UAV

	return float4(0.0f.xxxx);
}


#if !defined(SHADER_FINAL)
adaptiveDOFVertexOutputPassThrough VS_PassthroughFocalGrid(rageVertexInput IN)
{
	adaptiveDOFVertexOutputPassThrough OUT = (adaptiveDOFVertexOutputPassThrough)0 ;
	
	OUT.pos = float4(IN.pos, 1.0f);
    OUT.tex.xy = IN.texCoord0;

    return OUT;
}

float4 PS_FocusGrid(adaptiveDOFVertexOutputPassThrough IN) : COLOR
{
	return float4(1.0,0.0,0.0,0.2);

}
#endif //!defined(SHADER_FINAL)


technique AdaptiveDOF
{
	pass pp_parallelReduction_compute_sm50
	{
		SetComputeShader(	compileshader( cs_5_0, ReductionCS() ));
	}

	pass pp_depthdownsample
	{
		AlphaBlendEnable = false;
		AlphaTestEnable = false;

		VertexShader = compile VERTEXSHADER VS_PassthroughComposite();
		PixelShader  = compile PIXELSHADER PS_DepthDownsample()  CGC_FLAGS(CGC_DEFAULTFLAGS)	PS4_TARGET(FMT_32_R);
	}

	pass pp_adaptiveDOFcalc
	{
		AlphaBlendEnable = false;
		AlphaTestEnable = false;

		VertexShader = compile VERTEXSHADER VS_PassthroughComposite();
		PixelShader  = compile ps_5_0 PS_adaptiveDOFcalc()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
	
#if !defined(SHADER_FINAL)
	pass pp_debugFocusGrid
	{
		VertexShader = compile VERTEXSHADER VS_PassthroughFocalGrid();
		PixelShader  = compile PIXELSHADER PS_FocusGrid()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
#endif //!defined(SHADER_FINAL)
}


#else
technique dummy{ pass dummy	{} }
#endif //ADAPTIVE_DOF_GPU

