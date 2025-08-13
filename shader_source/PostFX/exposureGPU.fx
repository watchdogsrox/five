#pragma dcl position

#include "../common.fxh"

#if __SHADERMODEL >= 40

#include "../../../rage/base/src/grcore/fastquad_switch.h"

struct exposureGPUVertexInput {
	float3 pos : POSITION;
};


BeginDX10Sampler(	sampler, Texture2D<float>, source, source_Sampler, source)
ContinueSampler(sampler, source, source_Sampler, source)
	AddressU  = CLAMP;        
	AddressV  = CLAMP;
	MINFILTER = POINT;
	MAGFILTER = POINT;
	MIPFILTER = POINT;
EndSampler;


BeginDX10Sampler(	sampler, Texture2D<float>, currentLum, currentLum_Sampler, currentLum)
ContinueSampler(sampler, currentLum, currentLum_Sampler, currentLum)
	AddressU  = CLAMP;        
	AddressV  = CLAMP;
	MINFILTER = POINT;
	MAGFILTER = POINT;
	MIPFILTER = POINT;
EndSampler;


BeginDX10Sampler(	sampler, Texture2D<float>, history, history_Sampler, history)
ContinueSampler(sampler, history, history_Sampler, history)
	AddressU  = CLAMP;        
	AddressV  = CLAMP;
	MINFILTER = POINT;
	MAGFILTER = POINT;
	MIPFILTER = POINT;
EndSampler;


BeginDX10Sampler(	sampler, Texture2D<float>, adaptive, adaptive_Sampler, adaptive)
ContinueSampler(sampler, adaptive, adaptive_Sampler, adaptive)
	AddressU  = CLAMP;        
	AddressV  = CLAMP;
	MINFILTER = POINT;
	MAGFILTER = POINT;
	MIPFILTER = POINT;
EndSampler;


BEGIN_RAGE_CONSTANT_BUFFER(exposure_gpu_locals,b4)
// Exposure curve A, B and Offset + Tweak.
float4 ExposureCurve : ExposureCurve0;
// Exposure min, max, history index, history size.
float4 ExposureClampAndHistory : ExposureClampAndHistory0;
// AdaptionStepPercentage, adaptionMinStepSize, adaptionMaxStepSize, adaptionThreshold
float4 AdaptionParams : AdaptionParams0;
// IsCameraCut, justUseCalculated, adaptionTimeStep
float4 ExposureSwitches : ExposureSwitches0;
#if POSTFX_UNIT_QUAD
float4 QuadPosition;
#endif	//POSTFX_UNIT_QUAD
EndConstantBufferDX10(exposure_gpu_locals)


#if POSTFX_UNIT_QUAD
#define VIN	float2 pos : POSITION
#else
#define VIN	exposureGPUVertexInput IN
#endif	//POSTFX_UNIT_QUAD



#define EXPOSURE_CURVE_A ExposureCurve.x	
#define EXPOSURE_CURVE_B ExposureCurve.y	
#define EXPOSURE_CURVE_OFFSET ExposureCurve.z	
#define EXPOSURE_TWEAK ExposureCurve.w	


#define EXPOSURE_MIN ExposureClampAndHistory.x	
#define EXPOSURE_MAX ExposureClampAndHistory.y	

#define HISTORY_INDEX ExposureClampAndHistory.z
#define HISTORY_SIZE ExposureClampAndHistory.w

#define ADAPTION_STEP_PERCENTAGE AdaptionParams.x
#define ADAPTION_MIN_STEP_SIZE AdaptionParams.y
#define ADAPTION_MAX_STEP_SIZE AdaptionParams.z
#define ADAPTION_THRESHOLD AdaptionParams.w

#define ADAPTION_TIME_STEP ExposureSwitches.z

#define IS_CAMERA_CUT ExposureSwitches.x
#define JUST_USE_CALCULATED ExposureSwitches.y
#define FILL_VALUE ExposureSwitches.w


struct exposureGPUVertexOutput {
    DECLARE_POSITION(pos)
};


exposureGPUVertexOutput VS_Passthrough(VIN)
{
	exposureGPUVertexOutput OUT = (exposureGPUVertexOutput)0;
	
#if POSTFX_UNIT_QUAD
	OUT.pos = float4( QuadTransform(pos,QuadPosition), 0, 1 );
#else
	OUT.pos = float4(IN.pos, 1.0f);
#endif
    return OUT;
}


float PS_CalcBaseExposure()
{
	float targetExposure;
	int3 zero = int3(0, 0, 0);
	float curLum = currentLum.Load(zero).r;

	targetExposure = EXPOSURE_CURVE_A*pow(curLum, EXPOSURE_CURVE_B) + EXPOSURE_CURVE_OFFSET;
	targetExposure += EXPOSURE_TWEAK;
	targetExposure = clamp(targetExposure, EXPOSURE_MIN, EXPOSURE_MAX);
	return targetExposure;
}


void PS_CalcExposureAndAdaptionActive(out float newExposure, out float newAdaptionActive, out float debugValue)
{
	int3 zero = int3(0, 0, 0);

	// Calculate base exposure.
	float targetExposure = PS_CalcBaseExposure();


	int i;
	float avgExposure = 0;
	int historySize = (int)HISTORY_SIZE;

	// Compute avg. target exposure
	for(i = 0; i<historySize; i++)
	{
		int3 v = float3(i, 0, 0);
		avgExposure += history.Load(v).r;
	}
	avgExposure /= HISTORY_SIZE;

	float timeStep = ADAPTION_TIME_STEP;
	float absExposureDiff =	abs(targetExposure - avgExposure)*ADAPTION_STEP_PERCENTAGE*timeStep;
	absExposureDiff = min(ADAPTION_MAX_STEP_SIZE*timeStep, absExposureDiff);

	float adaptionActive = adaptive.Load(zero).r;

	if((absExposureDiff > ADAPTION_THRESHOLD*timeStep) || (adaptionActive > 0.0f))
	{
		if (absExposureDiff <= ADAPTION_MIN_STEP_SIZE*timeStep)
		{
			newExposure = targetExposure;
			newAdaptionActive = 0.0f;
		}
		else
		{
			newExposure = avgExposure + absExposureDiff*sign(targetExposure - avgExposure);
			newAdaptionActive = 1.0f;
		}
	}
	else
	{
		// Use last frames values.
		int3 one = float3(1, 0, 0);
		newExposure = source.Load(one).r;
		newAdaptionActive = adaptionActive;
	}
	// Clamp the exposure to allowed range.
	newExposure = clamp(newExposure, EXPOSURE_MIN, EXPOSURE_MAX);

	debugValue = targetExposure - avgExposure;
}



float PS_Copy(exposureGPUVertexOutput IN) : COLOR
{
	int3 v = int3((float)IN.pos.x, (float)IN.pos.y, 0);
	return source.Load(v).r;
}


float PS_UpdateHistory(exposureGPUVertexOutput IN) : COLOR
{
	float ret = 0.0f;

	if(abs(IN.pos.x - (HISTORY_INDEX + 0.5f)) < 0.5f)
	{
		int3 zero = int3(1, 0, 0);
		ret = source.Load(zero).r;
	}
	else
	{
		int3 v = int3((int)IN.pos.x, (int)IN.pos.y, 0);
		ret = history.Load(v).r;
	}
	return ret;
}


float PS_CalculateExposure(exposureGPUVertexOutput IN) : COLOR
{
	float ret = 0.0f;
	float exposure = 0.0f;
	float adaptionActive = 0.0f;
	float debugValue = 0.0f;

	if(IS_CAMERA_CUT > 0.0f || JUST_USE_CALCULATED > 0.0f)
	{
		ret = PS_CalcBaseExposure();
	}
	else
	{
		// Return the adapted exposure.
		PS_CalcExposureAndAdaptionActive(exposure, adaptionActive, debugValue);
		ret = exposure;
	}

	// Store 2^exposure in (0, 0) to save calculating the power on a per pixel bases in the postfx shader.
	if(abs(IN.pos.x - 0.5f) < 0.5f)
	{
		ret = ragePow(2.0f, ret);
	}

	// Store a debug value in (2, 0).
	if(abs(IN.pos.x - 2.5f) < 0.5f)
	{
		ret = debugValue;
	}

	return ret;
}


float PS_CalculateAdaptive(exposureGPUVertexOutput IN) : COLOR
{
	float ret = 0.0f;
	float exposure = 0.0f;
	float adaptionActive = 0.0f;
	float debugValue = 0.0f;


	if(IS_CAMERA_CUT > 0.0f || JUST_USE_CALCULATED > 0.0f)
	{
		// Reset adaption.
		ret = 1.0f;
	}
	else
	{
		// Return the adapted exposure.
		PS_CalcExposureAndAdaptionActive(exposure, adaptionActive, debugValue);
		ret = adaptionActive;
	}
	return ret;
}


float PS_Fill(exposureGPUVertexOutput IN) : COLOR
{
	return FILL_VALUE;
}


technique ExposureGPU
{
	pass exposuregpu_copy
	{
		VertexShader = compile VERTEXSHADER VS_Passthrough();
		PixelShader  = compile PIXELSHADER  PS_Copy() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
	pass exposuregpu_updatehistory
	{
		VertexShader = compile VERTEXSHADER VS_Passthrough();
		PixelShader  = compile PIXELSHADER  PS_UpdateHistory() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
	pass exposuregpu_calculateexposure
	{
		VertexShader = compile VERTEXSHADER VS_Passthrough();
		PixelShader  = compile PIXELSHADER  PS_CalculateExposure() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
	pass exposuregpu_calculateadaptive
	{
		VertexShader = compile VERTEXSHADER VS_Passthrough();
		PixelShader  = compile PIXELSHADER  PS_CalculateAdaptive() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
	pass exposuregpu_fill
	{
		VertexShader = compile VERTEXSHADER VS_Passthrough();
		PixelShader  = compile PIXELSHADER  PS_Fill() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}

#else
technique dummy{ pass dummy	{} }
#endif	//__SHADERMODEL >= 40
