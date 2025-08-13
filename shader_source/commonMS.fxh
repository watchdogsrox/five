// A common header for multisampled shaders, requires sm4_1
// but builds on base sm4_0. This is achieved by overriding the shader model
// for MSAA shaders, passes and techniques.
#define MULTISAMPLE_TECHNIQUES				(1 && (__SHADERMODEL >= 40) && (RSG_PC || RSG_ORBIS || RSG_DURANGO))
#define ENABLE_EQAA							(1 && MULTISAMPLE_TECHNIQUES && (RSG_ORBIS || RSG_DURANGO))
// Note: GetSamplePoint has a bug on Orbis 0.930.050, acknowledged as B#6352 by Sony
#define MULTISAMPLE_SUBPIXEL_OFFSET			((0 &&RSG_DURANGO) || (0 && RSG_ORBIS))

#ifndef SAMPLE_FREQUENCY
	#define SAMPLE_FREQUENCY MULTISAMPLE_TECHNIQUES
#endif
// Emulate per-sample interpolation for cases where we only need to process sample[0]
#define MULTISAMPLE_EMULATE_INTERPOLATOR	(MULTISAMPLE_TECHNIQUES && !SAMPLE_FREQUENCY)

#if __PSSL
	#define Texture2DMS			MS_Texture2D
	#define GetSamplePosition	GetSamplePoint
#endif

#if MULTISAMPLE_EMULATE_INTERPOLATOR
void adjustPixelInputForSample(Texture2DMS<float> depthTexture, uint sampleId, inout float3 input)
{
	const float2 sampleOffset = depthTexture.GetSamplePosition(sampleId);
	input += sampleOffset.x * ddx(input) + sampleOffset.y * ddy(input);
}
#endif //MULTISAMPLE_EMULATE_INTERPOLATOR

#if MULTISAMPLE_TECHNIQUES
#include "Util/macros.fxh"

int3 getIntCoords(float2 coords, float2 dim)
{
	return int3(coords * dim.xy, 0);
}

#if MULTISAMPLE_SUBPIXEL_OFFSET
int3 getIntCoordsWithEyeRay(TEXTURE_DEPTH_TYPE depthTexture, inout float2 coords, int iSample, float4x4 ViewInverse, inout float3 eyeRay, in float2 dim)
{
	// get sample offset
	float2 offPixelCenter = depthTexture.GetSamplePosition(iSample);
	float2 offScreen = offPixelCenter / dim.xy;
	float3 rayOffset = mul( float4(2*offScreen,0,0), ViewInverse ).xyz;
	coords += offScreen;
	eyeRay += rayOffset;
	// exit
	return int3(coords * dim.xy, 0);
}
#else	//MULTISAMPLE_SUBPIXEL_OFFSET
#define getIntCoordsWithEyeRay(tex,coords,_sample,_mx,_eye, dim)	getIntCoords(coords, dim)
#endif	//MULTISAMPLE_SUBPIXEL_OFFSET

float fetchTexDepth2DMS(TEXTURE_DEPTH_TYPE depthTexture, float2 texCoords, uint sampleIndex, float2 dim)
{
	int3 iPos = getIntCoords(texCoords, dim);
	return depthTexture.Load(iPos.xy, sampleIndex).x;
}
#endif	//MULTISAMPLE_TECHNIQUES
