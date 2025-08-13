#pragma dcl position

#if __SHADERMODEL>=50
#include "../commonMS.fxh"
#include "../../../rage/base/src/shaderlib/rage_common.fxh"
#include "../../../rage/base/src/grcore/AA_shared.h"

#define THREADS_PER_WAVEFRONT 64

// we've got the case of non-anchored sample pointing to the fragment
// that is not referenced by any of the anchor samples (B#1727655)
#define WORK_AROUND_LOST_SAMPLES	(RSG_DURANGO)

RWBuffer<uint>	FastClearTarget		REGISTER2( cs_5_0, u0 );
Buffer<uint>	FastClearSource;	//REGISTER2( cs_5_0, t0 );

Texture2DMS<float4>	AATexture;
Texture2D<uint2>	FmaskTexture;
Texture2DMS<float>	DepthTexture;

BeginConstantBufferDX10(clear_locals)
uint gValue;
uint4 gTargetAAParams;
#if AA_SAMPLE_DEBUG
float4 gDebugResolveParams;
#else
#define gDebugResolveParams		float4(0.225, 0.0, 1.0, 0.0)
#endif	//AA_SAMPLE_DEBUG
#define gResolveSideWeight		(gDebugResolveParams.x)
#define gResolveDepthThreshold	(gDebugResolveParams.y)
#define gResolveSkipEdges		(gDebugResolveParams.z)
EndConstantBufferDX10(clear_locals)

#define gAANumSamples	(gTargetAAParams.x)
#define gAANumFragments	(gTargetAAParams.y)
#define gAAFmaskShift	(gTargetAAParams.z)
#define gAADebugSample	(gTargetAAParams.w)
#define gAASampleOffset	(gTargetAAParams.w)

uint translateAASample(uint2 fmask, uint sampleId)
{
	uint shift = sampleId * gAAFmaskShift;
	uint mask = (1<<gAAFmaskShift) - 1;
	uint bits = shift>=32 ? (fmask.y>>(shift-32)) : (fmask.x>>shift);
	return bits & ((1<<gAAFmaskShift) - 1);
}

float4 loadTranslated(int2 tc, uint sampleId)
{
	uint2 fmask = FmaskTexture.Load(int3(tc,0));
	uint fragId = translateAASample(fmask, sampleId);
	return AATexture.Load(tc, fragId);
}

float4 loadBaseFragment(int2 tc, int2 offset)
{
	//return AATexture.Load(tc+offset, 0);
	// Ivan: Note that .Load takes an offset. It compiles into the same instruction and saves some ALU at some
	// little extra VMEM issue cost. The main reason to use it though it to reduce register pressure a little.
#if RSG_ORBIS
	return AATexture.Load(tc, offset, 0);
#else
	return AATexture.Load(tc, 0, offset);
#endif
}

float4 loadBaseFragment(int2 tc, int ofX, int ofY)
{
	return loadBaseFragment(tc, int2(ofX,ofY));
}

float4 loadOptionalFragment(int2 tc, uint flag, int2 offset, float4 fallback)
{
	const uint fmask = FmaskTexture.Load( int3(tc,0), offset );
	if (gResolveSkipEdges && fmask)
	{
		flag = 0;
	}
	return flag ? loadBaseFragment(tc,offset) : fallback;
}


[numthreads(THREADS_PER_WAVEFRONT,1,1)]
void CS_FastClear( uint groupID : SV_GroupID, uint threadID : SV_GroupThreadID )
{
	const uint offset = groupID * THREADS_PER_WAVEFRONT + threadID;
	FastClearTarget[offset] = gValue;
}

[numthreads(THREADS_PER_WAVEFRONT,1,1)]
void CS_FastCopy( uint groupID : SV_GroupID, uint threadID : SV_GroupThreadID )
{
	const uint offset = groupID * THREADS_PER_WAVEFRONT + threadID;
	FastClearTarget[offset] = FastClearSource[offset];
}


struct VSCLEAR_OUT
{
	float4 m_position : SV_Position;
};

VSCLEAR_OUT VS_Quad( uint id : SV_VertexID ) 
{
	// id | x  | y
	//----+----+--- 
	// 0  | 0  | 0
	//----+----+--- 
	// 1  | 1  | 0
	//----+----+--- 
	// 2  | 0  | 1
	//----+----+--- 
	// 3  | 1  | 1

	int x = id &  1;
	int y = id >> 1;

	VSCLEAR_OUT output = (VSCLEAR_OUT)0;
	output.m_position = float4(x*2-1,y*2-1,0,1);
	return output;
}


float4 PS_Dummy() : SV_Target
{
	return 0;
}

struct PS_IN
{
	DECLARE_POSITION_PSIN(screenPos)
};

struct PS_SS_IN
{
	DECLARE_POSITION_PSIN(screenPos)
	uint sampleIndex	: SV_SampleIndex;
};


float4 PS_CopyFmask(PS_IN IN) : SV_Target
{
	const int2 tc = int2( IN.screenPos.xy );
	//const uint globalMask = (1<<(gAAFmaskShift*gAANumSamples)) - 1;
	const uint2 fmask = FmaskTexture.Load( int3(tc,0) );
	const uint sampleMask = (1<<gAAFmaskShift) - 1;
	const uint4 shifts = gAAFmaskShift * (gAASampleOffset + uint4(0,1,2,3));
	const uint4 fmaskShifted = shifts.x>=32 ? (fmask.y>>(shifts-32)) : (fmask.x>>shifts);
	return (fmaskShifted & sampleMask) / (float)gAANumFragments;
}


float4 PS_Resolve_MSAA(PS_IN IN) : SV_Target
{
	const int2 tc = int2( IN.screenPos.xy );
	float4 sum = float4(0, 0, 0, 0);
	float numSamples = 0;

	for (uint i=0; i<gAANumSamples; ++i)
	{
		float4 val = AATexture.Load(tc, i);
		if (all(isfinite(val)))
		{
			sum +=  val;
			numSamples += 1.0f;
		}
	}
	return sum/numSamples;
}

float4 PS_DebugFragment_MSAA(PS_IN IN) : SV_Target
{
	const int2 tc = int2( IN.screenPos.xy );
	return AATexture.Load(tc, gAADebugSample);
}

float4 PS_DebugMask_MSAA(PS_IN IN) : SV_Target
{
	const int2 tc = int2( IN.screenPos.xy );
	float4 sum = float4(0, 0, 0, 1);
	float numFragments = 0;

	for (uint i=0; i<gAANumSamples; ++i)
	{
		if (gAADebugSample & (1<<i))	{
			numFragments += 1;
			sum += AATexture.Load(tc, i);
		}
	}
	return sum/numFragments;
}


float4 PS_Resolve_EQAA(PS_IN IN) : SV_Target
{
	const int2 tc = int2( IN.screenPos.xy );
	const uint2 fmask = FmaskTexture.Load( int3(tc,0) );
	float4 sum = float4(0, 0, 0, 0);
	float numValidSamples = 0;
	
	for (uint i=0; i<gAANumSamples; ++i)
	{
		uint fragId = translateAASample(fmask,i);
		if (fragId < gAANumFragments)
		{
			sum += AATexture.Load(tc, fragId);
			numValidSamples += 1.f;
		}
	}
	return sum/max(1.0f,numValidSamples);
}


float4 PS_Resolve_EQAA_NoNan(PS_IN IN) : SV_Target
{
	const int2 tc = int2( IN.screenPos.xy );
	const uint2 fmask = FmaskTexture.Load( int3(tc,0) );
	float4 sum = float4(0, 0, 0, 0);
	float numValidSamples = 0;

	for (uint i=0; i<gAANumSamples; ++i)
	{
		uint fragId = translateAASample(fmask,i);
		if (fragId < gAANumFragments)
		{
			float4 currentSample = AATexture.Load(tc, fragId);
			if (isfinite(dot(currentSample, 1.0.xxxx)))
			{
				sum += currentSample;
				numValidSamples += 1.f;
			}
		}
	}
	return (sum/max(1.0f,numValidSamples));
}

float4 PS_DebugFragment_EQAA(PS_IN IN) : SV_Target
{
	const int2 tc = int2( IN.screenPos.xy );
	const uint2 fmask = FmaskTexture.Load( int3(tc,0) );

	uint maxFragId = 0;
	for (uint i=0; i<gAANumSamples; ++i)
	{
		uint curFragId = translateAASample(fmask,i);
		if (maxFragId < curFragId)
			maxFragId = curFragId;
	}
	
	if (gAADebugSample<=maxFragId)
	{
		return AATexture.Load(tc, gAADebugSample);
	}

	return float4(0,0,0,1);
}


float4 PS_DebugSample_EQAA(PS_IN IN) : SV_Target
{
	const int2 tc = int2( IN.screenPos.xy );
	const uint2 fmask = FmaskTexture.Load( int3(tc,0) );

	uint fragId = translateAASample(fmask,gAADebugSample);
	return AATexture.Load(tc, fragId);
}


float4 PS_DebugFmask(PS_IN IN) : SV_Target
{
	const int2 tc = int2( IN.screenPos.xy );
	const uint2 fmask = FmaskTexture.Load( int3(tc,0) );

	float4 color = 0;
	uint maxFragId = 0;
	for (uint i=0; i<gAANumSamples; ++i)
	{
		uint fragId = translateAASample(fmask,i);
		if (maxFragId < fragId)
			maxFragId = fragId;
		color += float4(fragId==2,fragId==0,fragId==1,fragId>=3);
	}
	return float4(maxFragId==2,maxFragId==0,maxFragId==1,1);
	//return color / gNumSamples;
}


float PS_CompressDepth(PS_SS_IN IN) : SV_Depth
{
	const int2 tc = int2( IN.screenPos.xy );
	const uint2 fmask = FmaskTexture.Load( int3(tc,0) );

	for (uint i=0; i<gAANumFragments; ++i)
	{
		uint fragId = translateAASample(fmask, i);
		if (fragId == IN.sampleIndex)
			return AATexture.Load(tc, i).x;
	}
	return 0.0f;
}

// resolve EQAA surface with anchored samples already in place
float4 PS_Resolve_EQAA_Extra(PS_IN IN) : SV_Target
{
	const int2 tc = int2( IN.screenPos.xy );
	const uint2 fmask = FmaskTexture.Load( int3(tc,0) );
	
#if WORK_AROUND_LOST_SAMPLES
	float4 sum = AATexture.Load(tc,0);
#else
	float4 sum = float4(0,0,0,0);
#endif // WORK_AROUND_LOST_SAMPLES
	float4 anchors[4] = {sum,sum,sum,sum};
	uint i = 0;
	
	for (i=WORK_AROUND_LOST_SAMPLES?1:0; i<gAANumFragments; ++i)
	{
		uint fragId = translateAASample(fmask,i);
		float4 color = AATexture.Load(tc, i);
		anchors[fragId] = color;
		sum += color;
	}

	float numValidSamples = gAANumFragments;
	for (; i<gAANumSamples; ++i)
	{
		uint fragId = translateAASample(fmask,i);
		if (fragId >= gAANumFragments)
			continue;
	
		sum += anchors[fragId];
		numValidSamples += 1.f;
	}

	return sum/numValidSamples;
}

// resolve EQAA surface with anchored samples already in place
// average neighboring samples for unknown ones instead of skipping them
// hard-coded for 4 samples over a single fragment
float4 PS_Resolve_EQAA_Quality_41(PS_IN IN) : SV_Target
{
	const int2 tc = int2( IN.screenPos.xy );
	const uint2 fmask = FmaskTexture.Load( int3(tc,0) );
	uint3 fragId = uint3(
		translateAASample(fmask,1),
		translateAASample(fmask,2),
		translateAASample(fmask,3));

	float4 anchor = AATexture.Load(tc,0);

#if RSG_ORBIS
	// Pattern: (-6,6), (6,-6), (-2,-2), (2,2)
	float4 sum = anchor;
	sum += fragId.x!=0
		? loadBaseFragment(tc, 1,-1)
		: anchor;

	sum += fragId.y!=0
		? 0.5*(anchor + loadBaseFragment(tc, +0,-1))
		: anchor;
	sum += fragId.z!=0
		? 0.5*(anchor + loadBaseFragment(tc, +1,+0))
		: anchor;
	return sum;
#elif RSG_DURANGO
	// Pattern: (0,0), (-4,-4), (4,-8), (-8,4)
	// Note: these are inefficient locations
# if AA_SAMPLE_DEBUG
	float sideWeight = gResolveSideWeight;
# else
	float sideWeight = 0.33;
# endif
	float4 sides = 0;
	sides += fragId.x!=0
		? 0.5*(anchor + loadBaseFragment(tc, -1,-1))
		: anchor;
	sides += fragId.y!=0
		? (2*loadOptionalFragment(tc, 1, int2(+0,-1), anchor) + loadBaseFragment(tc, +1,-1)) / 3.0
		//? (2*loadBaseFragment(tc, +0,-1) + loadBaseFragment(tc, +1,-1)) / 3.0
		: anchor;
	sides += fragId.z!=0
		? (2*loadOptionalFragment(tc, 1, int2(-1,+0), anchor) + loadBaseFragment(tc, -1,+1)) / 3.0
		//? (2*loadBaseFragment(tc, -1,+0) + loadBaseFragment(tc, -1,+1)) / 3.0
		: anchor;
	return (1-3*sideWeight) * anchor + sideWeight * sides;
#else	//platforms
	return 1;
#endif	//platforms
}


#if AA_SAMPLE_DEBUG
// An old version of the centered-dithered sample pattern
float4 PS_Resolve_EQAA_Quality_41_CenteredOld(PS_IN IN) : SV_Target
{
	const int2 tc = int2( IN.screenPos.xy );
	const uint2 fmask = FmaskTexture.Load( int3(tc,0) );
	float4 anchor = AATexture.Load(tc,0);

	[branch]
	if( fmask.x == 0 )
	{
		return anchor;
	}

	uint3 fragId = uint3(
		translateAASample(fmask,1),
		translateAASample(fmask,2),
		translateAASample(fmask,3));

#if RSG_ORBIS
	// See grcDevice::SetAALocations for the reference
	// Pattern X0Y0: (0,0), (+3,-7), (+1,+6), (-6,-1)
	// Pattern X1Y0: (0,0), (-7,-3), (+6,-1), (-1,+6)
	// Pattern X0Y1: (0,0), (-7,+3), (+6,+1), (-1,-6)
	// Pattern X1Y1: (0,0), (+3,+7), (+1,-6), (-6,+1)
	const float sideWeight = gResolveSideWeight;
	float4 sum = anchor * (1-3*sideWeight);

	// Flatten because in a wave, in fact in a quad there will be all 4 combos,
	// so trying to branch will be a waste of time.
	[flatten]
	if ((tc.y & 1) == 0)
	{
		[flatten]	// Increases register pressure, makes the shade a tin bit faster
		if ((tc.x & 1) == 0)
		{	//top left
			sum += sideWeight*(fragId.x!=0
				? (3*loadBaseFragment(tc,0,-1) + 2*loadBaseFragment(tc,1,0))/5.0
				: anchor);

			sum += sideWeight*(fragId.y!=0
				? (2*loadBaseFragment(tc,0,1) + 1*loadBaseFragment(tc,1,1) + anchor)/4.0
				: anchor);
				
			sum += sideWeight*(fragId.z!=0
				? (2*loadBaseFragment(tc,-1,0) + 1*loadBaseFragment(tc,-1,-1) + anchor)/4.0
				: anchor);
		}else	//x==1
		{	//top right
			sum += sideWeight*(fragId.x!=0
				? (3*loadBaseFragment(tc,-1,0) + 2*loadBaseFragment(tc,0,-1))/5.0
				: anchor);

			sum += sideWeight*(fragId.y!=0
				? (2*loadBaseFragment(tc,1,0) + 1*loadBaseFragment(tc,1,-1) + anchor)/4.0
				: anchor);
				
			sum += sideWeight*(fragId.z!=0
				? (2*loadBaseFragment(tc,0,1) + 1*loadBaseFragment(tc,-1,1) + anchor)/4.0
				: anchor);
		}
	}else	//y==1
	{
		[flatten]	// Increases register pressure, makes the shade a tin bit faster
		if ((tc.x & 1) == 0)
		{	//bottom left
			sum += sideWeight*(fragId.x!=0
				? (3*loadBaseFragment(tc,-1,0) + 2*loadBaseFragment(tc,0,1))/5.0
				: anchor);

			sum += sideWeight*(fragId.y!=0
				? (2*loadBaseFragment(tc,1,0) + 1*loadBaseFragment(tc,1,1) + anchor)/4.0
				: anchor);
				
			sum += sideWeight*(fragId.z!=0
				? (2*loadBaseFragment(tc,0,-1) + 1*loadBaseFragment(tc,-1,-1) + anchor)/4.0
				: anchor);
		}else	//x==1
		{	//bottom right
			sum += sideWeight*(fragId.x!=0
				? (3*loadBaseFragment(tc,0,1) + 2*loadBaseFragment(tc,1,0))/5.0
				: anchor);

			sum += sideWeight*(fragId.y!=0
				? (2*loadBaseFragment(tc,0,-1) + 1*loadBaseFragment(tc,1,-1) + anchor)/4.0
				: anchor);
				
			sum += sideWeight*(fragId.z!=0
				? (2*loadBaseFragment(tc,-1,0) + 1*loadBaseFragment(tc,-1,1) + anchor)/4.0
				: anchor);
		}
	}
#else	//RSG_ORBIS
	// not implemented, since only Orbis has the magical ability to use dithered patterns
	float4 sum = 1;
#endif	//RSG_ORBIS

	return sum;
}
#endif //AA_SAMPLE_DEBUG


// a variation of PS_Resolve_EQAA_Quality_41 for the custom dithered sample pattern,
// which is a triangle around a center #0 sample, rotated and mirrored for each 2x2 square.
float4 PS_Resolve_EQAA_Quality_41_Dithered(PS_IN IN) : SV_Target
{
	// See grcDevice::SetAALocations for the reference
	// Pattern X0Y0: (0,0), (+7,+2), (-5,+5), (-2,-7)
	// Pattern X1Y0: (0,0), (-2,+7), (-5,-5), (+7,-2)
	// Pattern X0Y1: (0,0), (+2,-7), (+5,+5), (-7,+2)
	// Pattern X1Y1: (0,0), (-7,-2), (+5,-5), (+2,+7)

	const int2 tc = int2( IN.screenPos.xy );
	const uint2 fmask = FmaskTexture.Load( int3(tc,0) );
	float4 anchor = AATexture.Load(tc,0);

	[branch]
	if( fmask.x == 0 )
	{
		return anchor;
	}

	uint3 fragId = uint3(
		translateAASample(fmask,1),
		translateAASample(fmask,2),
		translateAASample(fmask,3));

	float sideWeight = gResolveSideWeight;
#if AA_SAMPLE_DEBUG
	if( gResolveDepthThreshold )
	{
		float depth = DepthTexture.Load(tc,0);
		float2 derivatives = float2(ddx(depth),ddy(depth));
		float edge = saturate(length(derivatives) / gResolveDepthThreshold);
		//return lerp(float4(0,1,0,0), float4(1,0,0,0), edge);
		[branch]
		if (edge < 0.1)
		{
			return anchor;
		}
		sideWeight *= edge;
	}
#endif //AA_SAMPLE_DEBUG

	float4 sides = 0;
	[flatten]
	if ((tc.y & 1) == 0)
	{
		[flatten]
		if ((tc.x & 1) == 0)
		{	//top left
			sides += loadOptionalFragment(tc, fragId.x, int2(+1,+0), anchor);
			sides += loadOptionalFragment(tc, fragId.y, int2(-1,+1), anchor);
			sides += loadOptionalFragment(tc, fragId.z, int2(+0,-1), anchor);
		}else	//x==1
		{	//top right
			sides += loadOptionalFragment(tc, fragId.x, int2(+0,+1), anchor);
			sides += loadOptionalFragment(tc, fragId.y, int2(-1,-1), anchor);
			sides += loadOptionalFragment(tc, fragId.z, int2(+1,+0), anchor);
		}
	}else	//y==1
	{
		[flatten]
		if ((tc.x & 1) == 0)
		{	//bottom left
			sides += loadOptionalFragment(tc, fragId.x, int2(+0,-1), anchor);
			sides += loadOptionalFragment(tc, fragId.y, int2(+1,+1), anchor);
			sides += loadOptionalFragment(tc, fragId.z, int2(-1,+0), anchor);
		}else	//x==1
		{	//bottom right
			sides += loadOptionalFragment(tc, fragId.x, int2(-1,+0), anchor);
			sides += loadOptionalFragment(tc, fragId.y, int2(+1,-1), anchor);
			sides += loadOptionalFragment(tc, fragId.z, int2(+0,+1), anchor);
		}
	}

	return (1-3*sideWeight)*anchor + sideWeight*sides;
}


// resolve EQAA surface with anchored samples already in place
// average neighboring samples for unknown ones instead of skipping them
// hard-coded for 4 samples over 2 fragments
float4 PS_Resolve_EQAA_Quality_42(PS_IN IN) : SV_Target
{
	const int2 tc = int2( IN.screenPos.xy );
	const uint2 fmask = FmaskTexture.Load( int3(tc,0) );

#if WORK_AROUND_LOST_SAMPLES
	float4 color0 = AATexture.Load(tc,0), color1 = AATexture.Load(tc,1);
	float4 anchors[2] = {color0,color0};
	uint fragId = translateAASample(fmask,1);
	anchors[fragId] = color1;
	float4 sum = anchors[0] + anchors[1];
#else // WORK_AROUND_LOST_SAMPLES
	float4 sum = float4(0,0,0,0);
	float4 anchors[2] = {sum,sum};
	uint fragId = 0;
	
	[unroll]
	for (uint i=0; i<2; ++i)	//gAANumFragments
	{
		fragId = translateAASample(fmask,i);
		float4 color = AATexture.Load(tc, i);
		anchors[fragId] = color;
		sum += color;
	}
#endif // WORK_AROUND_LOST_SAMPLES

	float4 sumAnchors = sum;
#if RSG_ORBIS
	// Pattern: (-6,6), (6,-6), (-2,-2), (2,2)
	const int2 offsets[4] = {int2(0,-1), int2(-1,0), int2(1,0), int2(0,1)};
#else // platforms
	// Pattern: (-6,-6), (6,6), (-2,2), (2,-2)
	const int2 offsets[4] = {int2(0,1), int2(-1,0), int2(1,0), int2(0,-1)};
#endif //platforms
	
	fragId = translateAASample(fmask,2);
	sum += (fragId >= 2 //gAANumFragments
		? 0.25*(sumAnchors + loadTranslated(tc+offsets[0],0) + loadTranslated(tc+offsets[1],1))
		: anchors[fragId]);
		
	fragId = translateAASample(fmask,3);
	sum += (fragId >= 2 //gAANumFragments
		? 0.25*(sumAnchors + loadTranslated(tc+offsets[2],0) + loadTranslated(tc+offsets[3],1))
		: anchors[fragId]);

	return sum * 0.25;
}

// hard-coded for 8 samples over 2 fragments
float4 PS_Resolve_EQAA_Quality_82(PS_IN IN) : SV_Target
{
	const int2 tc = int2( IN.screenPos.xy );
	const uint2 fmask = FmaskTexture.Load( int3(tc,0) );
	float4 sum = float4(0,0,0,0);
	float4 fragments[2] = {sum,sum};
	float4 anchors[2] = {sum,sum};
	uint fragId = 0;
	
	[unroll]
	for (uint i=0; i<2; ++i)	//gAANumFragments
	{
		fragId = translateAASample(fmask,i);
		anchors[i] = AATexture.Load(tc, i);
		fragments[fragId] = anchors[i];
		sum += anchors[i];
	}

	fragId = translateAASample(fmask,2);
	sum += (fragId >= 2 //gAANumFragments
		? (4*anchors[0] + 3*anchors[1] + 4*loadTranslated(tc+int2(1,0),0) + 3*loadTranslated(tc-int2(0,1),1)) / 14.0
		: fragments[fragId]);
		
	fragId = translateAASample(fmask,3);
	sum += (fragId >= 2 //gAANumFragments
		? (3*anchors[0] + 2*anchors[1] + 3*loadTranslated(tc+int2(0,1),0) + 5*loadTranslated(tc-int2(1,0),1)) / 13.0
		: fragments[fragId]);

	fragId = translateAASample(fmask,4);
	sum += (fragId >= 2 //gAANumFragments
		? anchors[0] : fragments[fragId]);

	fragId = translateAASample(fmask,5);
	sum += (fragId >= 2 //gAANumFragments
		? anchors[1] : fragments[fragId]);

	fragId = translateAASample(fmask,6);
	sum += (fragId >= 2 //gAANumFragments
		? (1*anchors[0] + 3*anchors[1] + 3*loadTranslated(tc+int2(1,0),0) + 1*loadTranslated(tc-int2(0,1),1)) / 8.0
		: fragments[fragId]);

	fragId = translateAASample(fmask,7);
	sum += (fragId >= 2 //gAANumFragments
		? 0.5*(anchors[0] + anchors[1])
		: fragments[fragId]);

	return sum * 0.125;
}

// hard-coded for 16 samples over 4 fragments
// focus on performance, not quality
float4 PS_Resolve_EQAA_Performance_104(PS_IN IN) : SV_Target
{
	const int2 tc = int2( IN.screenPos.xy );
	const uint2 fmask = FmaskTexture.Load( int3(tc,0) );
	float4 sum = float4(0,0,0,0);
	float4 anchors[4] = {sum,sum,sum,sum};
	
	uint i = 0;
	[unroll]
	for (; i<4; ++i)
	{
		uint fragId = translateAASample(fmask,i);
		float4 color = AATexture.Load(tc, i);
		anchors[fragId] = color;
		sum += color;
	}

	float numValidSamples = 4;
	[unroll]
	for (; i<0x10; ++i)
	{
		uint fragId = translateAASample(fmask,i);
		if (fragId >= 4)
			continue;
	
		sum += anchors[fragId];
		numValidSamples += 1.f;
	}

	return sum/numValidSamples;
}


technique clear
{
	pass fast_clear_cs
	{
		SetComputeShader(	compileshader( cs_5_0, CS_FastClear() )	);
	}
	pass fast_copy_cs
	{
		SetComputeShader(	compileshader( cs_5_0, CS_FastCopy() )	);
	}
	pass quad_clear
	{
		SetVertexShader(	compileshader( vs_5_0, VS_Quad() )	);
	}
	pass quad_dummy
	{
		SetVertexShader(	compileshader( vs_5_0, VS_Quad() )	);
		SetPixelShader(		compileshader( ps_5_0, PS_Dummy() )	);
	}
}

technique resolve
{
	pass copy_fmask
	{
		SetVertexShader(	compileshader( vs_5_0, VS_Quad() )	);
		SetPixelShader(		compileshader( ps_5_0, PS_CopyFmask() )	);
	}
	pass resolve_msaa
	{
		SetVertexShader(	compileshader( vs_5_0, VS_Quad() )	);
		SetPixelShader(		compileshader( ps_5_0, PS_Resolve_MSAA() )	);
	}
	pass debug_frag_msaa
	{
		SetVertexShader(	compileshader( vs_5_0, VS_Quad() )	);
		SetPixelShader(		compileshader( ps_5_0, PS_DebugFragment_MSAA() )	);
	}
	pass debug_mask_msaa
	{
		SetVertexShader(	compileshader( vs_5_0, VS_Quad() )	);
		SetPixelShader(		compileshader( ps_5_0, PS_DebugMask_MSAA() )	);
	}
	pass resolve_eqaa
	{
		SetVertexShader(	compileshader( vs_5_0, VS_Quad() )	);
		SetPixelShader(		compileshader( ps_5_0, PS_Resolve_EQAA() )	);
	}
	pass debug_frag_eqaa
	{
		SetVertexShader(	compileshader( vs_5_0, VS_Quad() )	);
		SetPixelShader(		compileshader( ps_5_0, PS_DebugFragment_EQAA() )	);
	}
	pass debug_sample
	{
		SetVertexShader(	compileshader( vs_5_0, VS_Quad() )	);
		SetPixelShader(		compileshader( ps_5_0, PS_DebugSample_EQAA() )	);
	}
	pass debug_fmask
	{
		SetVertexShader(	compileshader( vs_5_0, VS_Quad() )	);
		SetPixelShader(		compileshader( ps_5_0, PS_DebugFmask() )	);
	}
	pass compress_depth
	{
		SetVertexShader(	compileshader( vs_5_0, VS_Quad() )	);
		SetPixelShader(		compileshader( ps_5_0, PS_CompressDepth() )	);
	}
	pass resolve_eqaa_extra
	{
		SetVertexShader(	compileshader( vs_5_0, VS_Quad() )	);
		SetPixelShader(		compileshader( ps_5_0, PS_Resolve_EQAA_Extra() )	);
	}
	pass resolve_eqaa_quality_41
	{
		SetVertexShader(	compileshader( vs_5_0, VS_Quad() )	);
		SetPixelShader(		compileshader( ps_5_0, PS_Resolve_EQAA_Quality_41() )	);
	}
#if AA_SAMPLE_DEBUG
	pass resolve_eqaa_quality_41_centered_old
	{
		SetVertexShader(	compileshader( vs_5_0, VS_Quad() )	);
		SetPixelShader(		compileshader( ps_5_0, PS_Resolve_EQAA_Quality_41_CenteredOld() )	);
	}
#endif //AA_SAMPLE_DEBUG
	pass resolve_eqaa_quality_41_dithered
	{
		SetVertexShader(	compileshader( vs_5_0, VS_Quad() )	);
		SetPixelShader(		compileshader( ps_5_0, PS_Resolve_EQAA_Quality_41_Dithered() )	);
	}
	pass resolve_eqaa_quality_42
	{
		SetVertexShader(	compileshader( vs_5_0, VS_Quad() )	);
		SetPixelShader(		compileshader( ps_5_0, PS_Resolve_EQAA_Quality_42() )	);
	}
	pass resolve_eqaa_quality_82
	{
		SetVertexShader(	compileshader( vs_5_0, VS_Quad() )	);
		SetPixelShader(		compileshader( ps_5_0, PS_Resolve_EQAA_Quality_82() )	);
	}
	pass resolve_eqaa_performance_104
	{
		SetVertexShader(	compileshader( vs_5_0, VS_Quad() )	);
		SetPixelShader(		compileshader( ps_5_0, PS_Resolve_EQAA_Performance_104() )	);
	}
	pass resolve_eqaa_nandetect
	{
		SetVertexShader(	compileshader( vs_5_0, VS_Quad() )	);
		SetPixelShader(		compileshader( ps_5_0, PS_Resolve_EQAA_NoNan() )	);
	}

}

#else	//__SHADERMODEL
technique clear
{
	pass clear
	{
	}
}
#endif	//__SHADERMODEL
