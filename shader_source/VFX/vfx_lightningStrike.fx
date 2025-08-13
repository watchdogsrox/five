#ifndef PRAGMA_DCL
#pragma dcl position color0 texcoord0 texcoord1 texcoord2
#define PRAGMA_DCL
#endif

#include "../common.fxh"


BeginConstantBufferPagedDX10(lightningstrike_constants, b5)
float2 g_LightningStrikeParams : LightningStrikeParams = float2(0.55f, 8.0f);
float4 g_LightningStrikeNoiseParams : LightningStrikeNoiseParams;
EndConstantBufferDX10(lightning_constants)

#define lighning_Width_Fade				(g_LightningStrikeParams.x)
#define lighning_Width_Fade_Falloff_Exp	(g_LightningStrikeParams.y)

#define lightning_Noise_TexScale		(g_LightningStrikeNoiseParams.x)
#define lightning_Noise_Amplitude		(g_LightningStrikeNoiseParams.y)

// ----------------------------------------------------------------------------------------------- //
BeginDX10Sampler(sampler, Texture2D<float>, noiseTexture, noiseSampler, noiseTexture)
ContinueSampler(sampler, noiseTexture, noiseSampler, noiseTexture)
AddressU	= WRAP;
AddressV	= WRAP;
MINFILTER	= LINEAR;
MAGFILTER	= LINEAR;
MIPFILTER	= MIPLINEAR;
EndSampler;

// ----------------------------------------------------------------------------------------------- //

struct vertexLightningIN {
	float3 pos						: POSITION;
	float4 diffuseV					: COLOR0;
	float4 startPos_NoiseOffset		: TEXCOORD0; //startPos.xyz, startNoiseOffset
	float4 endPos_NoiseOffset 		: TEXCOORD1; //endPos.xyz, endNoiseOffset
	float3 segmentParams			: TEXCOORD2; // currentSegmentWidth, intensity, bottomVert

};

// ----------------------------------------------------------------------------------------------- //

struct vertexLightningOUT {
	DECLARE_POSITION(pos)
	float4 worldPos_midNoiseOffset				: TEXCOORD0;
	NOINTERPOLATION float4 color0				: TEXCOORD1;
	NOINTERPOLATION float4 startPosAndWidth		: TEXCOORD2;	// startPos, SegmentWidth
	NOINTERPOLATION float4 lineVec_tangent		: TEXCOORD3;
};
// ----------------------------------------------------------------------------------------------- //

struct pixelLightningIN {
#if __MAX
	DECLARE_POSITION_PSIN(pos)
#else
	inside_centroid DECLARE_POSITION_PSIN(pos)
#endif
	float4 worldPos_midNoiseOffset				: TEXCOORD0;
	NOINTERPOLATION float4 color0				: TEXCOORD1;
	NOINTERPOLATION float4 startPosAndWidth		: TEXCOORD2;	// startPos, SegmentWidth
	NOINTERPOLATION float4 lineVec_tangent		: TEXCOORD3;

};

#if __SHADERMODEL>=40
struct pixelLightningIN_MSAA {
	pixelLightningIN base;
	uint sampleIndex			: SV_SampleIndex;
};
#endif

#define startPos			startPos_NoiseOffset.xyz
#define endPos				endPos_NoiseOffset.xyz
#define startNoiseOffset	startPos_NoiseOffset.w
#define endNoiseOffset		endPos_NoiseOffset.w
#define worldPos			worldPos_midNoiseOffset.xyz
#define midNoiseOffset		worldPos_midNoiseOffset.w
#define intensity			segmentParams.y
#define bottomVert			segmentParams.z
#define currentSegmentWidth segmentParams.x
#define intensity			segmentParams.y
#define bottomVert			segmentParams.z
#define lineVec				lineVec_tangent.xy
#define tangent				lineVec_tangent.zw
// ----------------------------------------------------------------------------------------------- //
//Here's how the quad points are

// 0             3
//	-------------
//	|		   /|
//	| 		  /	|
//	|		 /	|
//	|   	/	|
//	|	   /	|
//	|     /		|
//	|    /      |
//	|   /		|
//	|  /		|
//	| /         |
//	|/          |
//	-------------
// 1             2
vertexLightningOUT VS_Lightning(vertexLightningIN IN)
{
	vertexLightningOUT OUT;
	OUT.worldPos = (mul(float4(IN.pos,1), gWorld)).xyz;
	OUT.pos = MonoToStereoClipSpace(mul(float4(IN.pos,1), gWorldViewProj));
	//Calculate line start and end positions in screen space
	float4 projectedStartPoint = (MonoToStereoClipSpace(mul(float4(IN.startPos,1), gWorldViewProj)));
	float4 projectedEndPoint = (MonoToStereoClipSpace(mul(float4(IN.endPos,1), gWorldViewProj)));
	projectedStartPoint /= projectedStartPoint.w;
	projectedEndPoint /= projectedEndPoint.w;
	projectedStartPoint.xy = (projectedStartPoint.xy *  float2(0.5f, -0.5f) + 0.5f ); //0-1 space
	projectedEndPoint.xy = (projectedEndPoint.xy *  float2(0.5f, -0.5f) + 0.5f);
	projectedStartPoint.xy *= gScreenSize.xy;
	projectedEndPoint.xy *= gScreenSize.xy;

	const float2 lineVector = projectedEndPoint.xy - projectedStartPoint.xy;

	OUT.lineVec.xy = normalize(lineVector);
	OUT.tangent = cross( float3(OUT.lineVec.xy, 0.0f), float3( 0,0,1)).xy;
	OUT.startPosAndWidth = float4(projectedStartPoint.xyz, IN.currentSegmentWidth);
	OUT.color0 = IN.diffuseV * IN.intensity;
	OUT.midNoiseOffset = ((IN.bottomVert>0.0f)? IN.startNoiseOffset:IN.endNoiseOffset);
	return OUT;
}

// ----------------------------------------------------------------------------------------------- //

OutHalf4Color PS_Lightning(pixelLightningIN IN)
{
	//This shader uses an antialiasing technique where we draw the
	//quads with a thick width, and we calculate the line width in 
	//the shader. We calculate the intensity of the pixel based on
	//the falloff between the actual width of the line and the fade
	//fade width of the line to reduce the jagginess 

	//Using pixel distance from line to calculate pixel intensity
	const float lineWidth = IN.startPosAndWidth.w;
	const float halfWidth = lineWidth * 0.5f;
	//Fade width is greater than actual width of the line
	const float lineWidthFade= lineWidth * lighning_Width_Fade;
	const float halflineWidthFade= lineWidthFade * 0.5f;

	float2 noiseTex = float2(IN.midNoiseOffset * lightning_Noise_TexScale, 0.5f);
#if __SHADERMODEL>=40
	float noiseOffset = (noiseTexture.SampleLevel(noiseSampler, noiseTex, 0).x * 2.0f - 1.0f) * lightning_Noise_Amplitude;
#else
	float noiseOffset = 0.0f;
#endif
	
	//Calculate distance of point P from input line AB
	//C is projected Point P on line AB
	//				A
	//			   /|
	//	 		  /	|
	//			 /	|
	//		   	/	|
	//		   /	|
	//		  /		|
	//	     /      |
	//	    /		|
	//	   /		|
	//	  /         |
	// P /__________| C
	//			    |
	//		        |
	//	   			|
	//				|
	//			    |
	//				|
	//	            |
	//	            | B
	//	
	//	
	// 
	const float2 ABDir = IN.lineVec.xy;
	const float2 A = IN.startPosAndWidth.xy;
	const float2 P = IN.pos.xy;
	const float2 AP = P - A;
	const float2 C = IN.startPosAndWidth + ABDir * dot(AP, IN.lineVec);
	const float2 CP = P - C + (IN.tangent * noiseOffset);
	const float distanceFromLine = length(CP);

	//Find what point we are between fade width and actual width in 0-1 space
	float lerpFactor = (distanceFromLine - halfWidth)/(halflineWidthFade - halfWidth);
	const float distFactor = pow(1.0f - saturate(lerpFactor), lighning_Width_Fade_Falloff_Exp);
	
	float4 ret = IN.color0  * distFactor;
	//Adding fog. Will disable if required
	const float3 camPos = +gViewInverse[3].xyz;
	ret.a = saturate(ret.a) * (1 - CalcFogData(IN.worldPos - camPos).w);
	return CastOutHalf4Color(PackHdr(ret));
}

#if __SHADERMODEL>=40
OutHalf4Color PS_Lightning_MSAA(pixelLightningIN_MSAA IN)
{
	OutHalf4Color result = PS_Lightning(IN.base);
	result.col0 += IN.sampleIndex * 0.00001;	//force super-sampling
	return result;
}
#endif


#	if RSG_PC
#		define LIGHTNING_MSAA_PIXEL_SHADER	ps_4_1
#	else
#		define LIGHTNING_MSAA_PIXEL_SHADER	PIXELSHADER
#	endif

technique lightning
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER VS_Lightning();
		PixelShader  = compile PIXELSHADER PS_Lightning()  CGC_FLAGS(CGC_DEFAULTFLAGS); 
	}

#if __SHADERMODEL>=40
	pass p1_sm41
	{
		VertexShader = compile VERTEXSHADER VS_Lightning();
		PixelShader  = compile LIGHTNING_MSAA_PIXEL_SHADER PS_Lightning_MSAA()  CGC_FLAGS(CGC_DEFAULTFLAGS); 
	}
#endif // __SHADERMODEL>=40
}
