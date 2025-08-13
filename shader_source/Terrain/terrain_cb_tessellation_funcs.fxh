
#include "terrain_cb_switches.fxh"


#if TERRAIN_TESSELLATION_SHADER_DATA

CBSHARED BeginConstantBufferPagedDX10(terrain_globals, b11)
shared float4 gTerrainGlobal0 : gTerrainGlobal0												REGISTER(c170) < int nostrip = 1; >;
shared float4 gTerrainNearAndFarRamp : gTerrainNearAndFarRamp								REGISTER(c171) < int nostrip = 1; >;
shared float4 gTerrainNearNoiseAmplitude : gTerrainNearNoiseAmplitude						REGISTER(c172) < int nostrip = 1; >;
shared float4 gTerrainFarNoiseAmplitude : gTerrainFarNoiseAmplitude							REGISTER(c173) < int nostrip = 1; >;
shared float4 gTerrainGlobalBlendAndProjectionInfo : gTerrainGlobalBlendAndProjectionInfo	REGISTER(c174)< int nostrip = 1; >;
shared float4 gTerrainCameraWEqualsZeroPlane : gTerrainCameraWEqualsZeroPlane				REGISTER(c175)< int nostrip = 1; >;
shared float4 gTerrainWModification : gTerrainWModification									REGISTER(c176)< int nostrip = 1; >;
shared float4 gTerrainNoiseTexParam : gTerrainNoiseTexParam									REGISTER(c177)< int nostrip = 1; >;
EndConstantBufferDX10(terrain_globals)

BeginDX10SamplerShared(sampler, Texture3D<float4>, TerrainNoiseTex, TerrainNoiseSampler, TerrainNoiseTex, s13)
ContinueSampler(sampler, TerrainNoiseTex, TerrainNoiseSampler, TerrainNoiseTex)
	AddressU  = WRAP;        
	AddressV  = WRAP;
	AddressW  = WRAP;
	MINFILTER = POINT;
	MAGFILTER = POINT;
	MIPFILTER = POINT;
EndSampler;

#endif // TERRAIN_TESSELLATION_SHADER_DATA


#if TERRAIN_TESSELLATION_SHADER_CODE

#define TERRAIN_TESSELLATION_PIXELS_PER_VERTEX	gTerrainGlobal0.x
#define TERRAIN_WORLD_NOISE_TEXEL_UNIT			gTerrainGlobal0.y
// Start & end of envelope where noise is non-zero.
#define TERRAIN_TESSELLATION_NEAR				gTerrainGlobal0.z
#define TERRAIN_TESSELLATION_FAR				gTerrainGlobal0.w

// Ramps of the "envelope" where noise is non-zero.
#define TERRAIN_TESSELLATION_NEAR_RAMP_A	gTerrainNearAndFarRamp.x
#define TERRAIN_TESSELLATION_NEAR_RAMP_B	gTerrainNearAndFarRamp.y
#define TERRAIN_TESSELLATION_FAR_RAMP_A		gTerrainNearAndFarRamp.z
#define TERRAIN_TESSELLATION_FAR_RAMP_B		gTerrainNearAndFarRamp.w

// Noise range [-1, 1] is mapped to [TERRAIN_NEAR_NOISE_MIN, TERRAIN_NEAR_NOISE_MIN]...
#define TERRAIN_NEAR_NOISE_MIN				gTerrainNearNoiseAmplitude.x	
#define TERRAIN_NEAR_NOISE_MAX				gTerrainNearNoiseAmplitude.y
#define TERRAIN_NEAR_NOISE_MAP_A			gTerrainNearNoiseAmplitude.z	
#define TERRAIN_NEAR_NOISE_MAP_B			gTerrainNearNoiseAmplitude.w

// And also, [-1, 1] is mapped to [TERRAIN_FAR_NOISE_MIN, TERRAIN_FAR_NOISE_MIN]
#define TERRAIN_FAR_NOISE_MIN				gTerrainFarNoiseAmplitude.x	
#define TERRAIN_FAR_NOISE_MAX				gTerrainFarNoiseAmplitude.y
#define TERRAIN_FAR_NOISE_MAP_A				gTerrainFarNoiseAmplitude.z	
#define TERRAIN_FAR_NOISE_MAP_B				gTerrainFarNoiseAmplitude.w

// Global blend (used to blend between "near" and "far" noise values) to realise greater amplitudes at depth.
#define TERRAIN_GlOBAL_BLEND_A					gTerrainGlobalBlendAndProjectionInfo.x
#define TERRAIN_GlOBAL_BLEND_B					gTerrainGlobalBlendAndProjectionInfo.y
#define TERRAIN_PROJECTION_TWICE_TAN_HALF_HFOV	gTerrainGlobalBlendAndProjectionInfo.z
#define TERRAIN_PROJECTION_DISPLAY_WIDTH		gTerrainGlobalBlendAndProjectionInfo.w

// Camera w equals 0 plane (we can`t use the reqular projection matrix in the shadow pass).
#define TERRAIN_CAMERA_W_EQUALS_ZERO_PLANE	gTerrainCameraWEqualsZeroPlane

// W used for tessellation factors:- 
// ModifiedW = TERRAIN_TESSELLATION_NEAR for  W < TerrainWUseNearEnd (see C code or rag) .
// ModifiedW = straight line from [TERRAIN_TESSELLATION_NEAR, TERRAIN_TESSELLATION_FAR]  for TerrainWUseNearEnd <= W <= TERRAIN_TESSELLATION_FAR.
// ModifiedW = W for W > TERRAIN_TESSELLATION_FAR.

// This way an area round the camera can have a "fixed" mesh, but allows for smooth tessellation LODing further away.
#define TERRAIN_W_MODIFICATION_A	gTerrainWModification.x
#define TERRAIN_W_MODIFICATION_B	gTerrainWModification.y


struct PatchInfo_TerrainTessellation
{
	float Edges[3] : SV_TessFactor;
	float Inside[1] : SV_InsideTessFactor;
	float doTessellation : DO_TESSELLATION;
};


float TerrainNoise3D(sampler S, Texture3D<float4> T, float3 T_dimensions, float3 P, float3 worldUnitsPerTexel)
{
	float3 unused;
	float3 units;
	float3 dimensions = T_dimensions;

	// Compute the position in "noise texel unis". Fractional + whole part. 
	float4 fractional;
	fractional.xyz = modf(P/worldUnitsPerTexel, units);
	fractional.w = 1.0f;

	// Compute the index into the repeating "noise cuboid".
	float3 texelCoord = fmod(units, dimensions);

	// Convert to sample coordinates.
	float3 sampleCoord = (texelCoord + float3(0.5f, 0.5f, 0.5f))/dimensions;
	float3 deltaSampleCoord = float3(1.0f, 1.0f, 1.0f)/dimensions;

	float4 A;
	float4 B;

	float3 PStar000 = fractional.xyz - float3(0.0f, 0.0f, 0.0f);
	float3 PStar010 = fractional.xyz - float3(0.0f, 1.0f, 0.0f);
	float3 PStar001 = fractional.xyz - float3(0.0f, 0.0f, 1.0f);
	float3 PStar011 = fractional.xyz - float3(0.0f, 1.0f, 1.0f);

	float3 PStar100 = fractional.xyz - float3(1.0f, 0.0f, 0.0f);
	float3 PStar110 = fractional.xyz - float3(1.0f, 1.0f, 0.0f);
	float3 PStar101 = fractional.xyz - float3(1.0f, 0.0f, 1.0f);
	float3 PStar111 = fractional.xyz - float3(1.0f, 1.0f, 1.0f);

	// Compute signed distances to the planes stored at each corner of the texel our point is in.
	A.x = dot(T.SampleLevel(S, sampleCoord + float3(0.0f, 0.0f, 0.0f)*deltaSampleCoord, 0).xyz, PStar000);
	A.y = dot(T.SampleLevel(S, sampleCoord + float3(0.0f, 1.0f, 0.0f)*deltaSampleCoord, 0).xyz, PStar010);
	A.z = dot(T.SampleLevel(S, sampleCoord + float3(0.0f, 0.0f, 1.0f)*deltaSampleCoord, 0).xyz, PStar001);
	A.w = dot(T.SampleLevel(S, sampleCoord + float3(0.0f, 1.0f, 1.0f)*deltaSampleCoord, 0).xyz, PStar011);

	B.x = dot(T.SampleLevel(S, sampleCoord + float3(1.0f, 0.0f, 0.0f)*deltaSampleCoord, 0).xyz, PStar100);
	B.y = dot(T.SampleLevel(S, sampleCoord + float3(1.0f, 1.0f, 0.0f)*deltaSampleCoord, 0).xyz, PStar110);
	B.z = dot(T.SampleLevel(S, sampleCoord + float3(1.0f, 0.0f, 1.0f)*deltaSampleCoord, 0).xyz, PStar101);
	B.w = dot(T.SampleLevel(S, sampleCoord + float3(1.0f, 1.0f, 1.0f)*deltaSampleCoord, 0).xyz, PStar111);

	float3 cubicFactor = 3.0*fractional*fractional*fractional - 2.0f*fractional*fractional;
	float3 OneMinusCubicFactor = float3(1.0f, 1.0f, 1.0f) - cubicFactor;

	// Interpolate in x.
	float4 inX = OneMinusCubicFactor.x*A + cubicFactor.x*B;
	// Interpolate in y.
	float inY1 = OneMinusCubicFactor.y*inX.x + cubicFactor.y*inX.y;
	float inY2 = OneMinusCubicFactor.y*inX.z + cubicFactor.y*inX.w;
	// Interpolate in z.
	return OneMinusCubicFactor.z*inY1 + cubicFactor.z*inY2;
}

float TerrainSumNoise3D_2Terms(sampler S, Texture3D<float4> T, float3 T_dimensions, float3 P, float3 worldUnitsPerTexel)
{
	return TerrainNoise3D(S, T, T_dimensions, P, worldUnitsPerTexel) + 0.5f*TerrainNoise3D(S, T, T_dimensions, 2.0f*P, worldUnitsPerTexel);
}

float TerrainSumNoise3D_3Terms(sampler S, Texture3D<float4> T, float3 T_dimensions, float3 P, float3 worldUnitsPerTexel)
{
	return TerrainNoise3D(S, T, T_dimensions, P, worldUnitsPerTexel) + (1.0f/2.0f)*TerrainNoise3D(S, T, T_dimensions, 2.0f*P, worldUnitsPerTexel) + (1.0f/4.0f)*TerrainNoise3D(S, T, T_dimensions, 4.0f*P, worldUnitsPerTexel);
}


// Calculates an edge tessellation factor.
float CalculateEdgeTessellationFactor(float3 A, float3 B, float wA, float wB)
{
	float L = length(A - B);
	// Determine w the at the mid-point of the edge.
	float W = 0.5f*(wA + wB);

	float modifiedW = TERRAIN_W_MODIFICATION_A*W + TERRAIN_W_MODIFICATION_B;
	modifiedW = max(modifiedW, TERRAIN_TESSELLATION_NEAR); 
	W = max(W, TERRAIN_TESSELLATION_NEAR);
	modifiedW = min(W, modifiedW);

	// Calculate width of frustum at this depth.
	float frustumWidth = modifiedW*TERRAIN_PROJECTION_TWICE_TAN_HALF_HFOV;
	// And from that an approximation of the number of pixels the edge occupies.
	float approxNoOfPixels = (L/frustumWidth)*TERRAIN_PROJECTION_DISPLAY_WIDTH;

	// Determine the number of "vertices" (the number of segments the edge should be split into).
	float ret = approxNoOfPixels/TERRAIN_TESSELLATION_PIXELS_PER_VERTEX;

	return max(ret, 1.0f);
}


// Returns a scene depth value.
float ComputeSceneCameraDepth(float3 modelSpacePosition)
{
	// Calculate this using the stored camera info, rather than projected w, so this will work in the shadow pass.
	float3 worldPos = mul(float4(modelSpacePosition, 1), gWorld);
	return dot(float4(worldPos, 1), TERRAIN_CAMERA_W_EQUALS_ZERO_PLANE); 
}


// Computes a vertex`s perturbed position.
float3 ComputePerturbedVertexPosition(float3 modelSpacePosition, float sceneCameraViewSpaceW, float openEdgeFactor)
{
	float3 ret = modelSpacePosition;

	if((sceneCameraViewSpaceW > TERRAIN_TESSELLATION_NEAR) && (sceneCameraViewSpaceW < TERRAIN_TESSELLATION_FAR))
	{
		// Calculate noise at the world position.
		float3 worldPos = mul(float4(modelSpacePosition, 1), gWorld).xyz;
		float noise = TerrainSumNoise3D_3Terms(TerrainNoiseSampler, TerrainNoiseTex, gTerrainNoiseTexParam, worldPos, float3(TERRAIN_WORLD_NOISE_TEXEL_UNIT, TERRAIN_WORLD_NOISE_TEXEL_UNIT, TERRAIN_WORLD_NOISE_TEXEL_UNIT));

		// Transform noise into near ranges.
		float nearNoise = TERRAIN_NEAR_NOISE_MAP_A*noise + TERRAIN_NEAR_NOISE_MAP_B;
		nearNoise = clamp(nearNoise, TERRAIN_NEAR_NOISE_MIN, TERRAIN_NEAR_NOISE_MAX);
		// ...And far.
		float farNoise = TERRAIN_FAR_NOISE_MAP_A*noise + TERRAIN_FAR_NOISE_MAP_B;
		farNoise = clamp(farNoise, TERRAIN_FAR_NOISE_MIN, TERRAIN_FAR_NOISE_MAX);

		// Calculate near/far blend.
		float globalBlend = sceneCameraViewSpaceW*TERRAIN_GlOBAL_BLEND_A + TERRAIN_GlOBAL_BLEND_B;
		globalBlend = clamp(globalBlend, 0.0f, 1.0f);

		// Calculate final noise amplitude.
		noise = (1.0f - globalBlend)*nearNoise + globalBlend*farNoise;

		// Scale against the near and far ramps.
		float nearRamp = sceneCameraViewSpaceW*TERRAIN_TESSELLATION_NEAR_RAMP_A + TERRAIN_TESSELLATION_NEAR_RAMP_B;
		float farRamp = sceneCameraViewSpaceW*TERRAIN_TESSELLATION_FAR_RAMP_A + TERRAIN_TESSELLATION_FAR_RAMP_B;
		float noiseDepthScale = min(nearRamp, farRamp);
		noiseDepthScale = clamp(noiseDepthScale, 0.0f, 1.0f);

		// Perturb the vertex along "up" in worldspace.
		float3 worldUpInModelSpace = float3(gWorld[0].z, gWorld[1].z, gWorld[2].z);
		ret += (noise*noiseDepthScale*openEdgeFactor)*worldUpInModelSpace;
	}
	return ret;
}


// Compute tessellation factors.
PatchInfo_TerrainTessellation ComputeTessellationFactors(float4 A, float4 B, float4 C)
{
	float4 PatchPoints[3];
	PatchInfo_TerrainTessellation Output;

	PatchPoints[0] = A;
	PatchPoints[1] = B;
	PatchPoints[2] = C;

	if(TERRAIN_TESSELLATION_PIXELS_PER_VERTEX < 0.0f)
	{
		Output.Edges[0] = 1.0f;
		Output.Edges[1] = 1.0f;
		Output.Edges[2] = 1.0f;
		Output.Inside[0] = 1.0f;
		Output.doTessellation = 0.0f;
	}
	else
	{
		float wMin = min(min(PatchPoints[0].w, PatchPoints[1].w), PatchPoints[2].w);

		if(wMin > TERRAIN_TESSELLATION_FAR)
		{
			Output.Edges[0] = 1.0f;
			Output.Edges[1] = 1.0f;
			Output.Edges[2] = 1.0f;
			Output.Inside[0] = 1.0f;
			Output.doTessellation = 0.0f;
		}
		else
		{
			float wMax = max(max(PatchPoints[0].w, PatchPoints[1].w), PatchPoints[2].w);

			if(wMax < TERRAIN_TESSELLATION_NEAR)
			{
				Output.Edges[0] = 1.0f;
				Output.Edges[1] = 1.0f;
				Output.Edges[2] = 1.0f;
				Output.Inside[0] = 1.0f;
				Output.doTessellation = 0.0f;
			}
			else
			{
				Output.Edges[0] = CalculateEdgeTessellationFactor(PatchPoints[1].xyz, PatchPoints[2].xyz, PatchPoints[1].w, PatchPoints[2].w);
				Output.Edges[1] = CalculateEdgeTessellationFactor(PatchPoints[2].xyz, PatchPoints[0].xyz, PatchPoints[2].w, PatchPoints[0].w);
				Output.Edges[2] = CalculateEdgeTessellationFactor(PatchPoints[0].xyz, PatchPoints[1].xyz, PatchPoints[0].w, PatchPoints[1].w);
				//Output.Inside[0] = max(max(Output.Edges[0], Output.Edges[1]), Output.Edges[2]);
				Output.Inside[0] = (Output.Edges[0] + Output.Edges[1] + Output.Edges[2])/3.0f;
				Output.doTessellation = 1.0f;
			}
		}
	}
	return Output;
}


#endif // TERRAIN_TESSELLATION_SHADER_CODE

