//
//
//
//
#ifndef __TREES_WINDFUNCS_FXH__
#define __TREES_WINDFUNCS_FXH__

#include "trees_shared.h"

// TODO:- Remove.
#define USE_ERIKS_TEMP_UMOVEMENT_COLOURS			(1)


#if (TREES_ENABLE_BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS || __MAX) && !__LOW_QUALITY

#ifdef TREE_DRAW
#define IS_TREE_DRAW (1)
#else
#define IS_TREE_DRAW (0)
#endif

#if __SHADERMODEL>=40 && (WIND_DISPLACEMENT || IS_TREE_DRAW)

DECLARE_SAMPLER(sampler3D, SfxWindTexture3D, SfxWindSampler3D,
AddressU  = CLAMP;
AddressV  = CLAMP;
AddressW  = CLAMP;
MIPFILTER = LINEAR;
MINFILTER = LINEAR;
MAGFILTER = LINEAR;
);

#endif //__SHADERMODEL>=40 && (WIND_DISPLACEMENT || IS_TREE_DRAW)


CBSHARED BeginConstantBufferPagedDX10(tri_uMovment_and_branch_bend_shared, b11)
	shared	float4 branchBendEtc_WindVector[4] : branchBendEtc_WindVector0					REGISTER(		c185) < int nostrip = 1; >;
	shared	float4 branchBendEtc_WindSpeedSoftClamp : branchBendEtc_WindSpeedSoftClamp0		REGISTER(		c189) < int nostrip = 1; >;
	shared	float4 branchBendEtc_UnderWaterTransition : branchBendEtc_UnderWaterTransition0	REGISTER(		c190) < int nostrip = 1; >;

	// Wind vector + um high low blend.
	#define triuMovement_umHighLowBlend		(branchBendEtc_WindVector[0].w)
	#define branchBend_Freq1				(branchBendEtc_WindVector[1].w)
	#define branchBend_Freq2				(branchBendEtc_WindVector[2].w)
	// We have a copy of the global phase shift for the tree (so it can be accessed by non-trees_common.fx based shaders).
	#define triuMovement_GlobalPhaseShift	(branchBendEtc_WindVector[3].w)	

	#define branchBend_WindSpeedUnrestricted		(branchBendEtc_WindSpeedSoftClamp.x)
	#define branchBend_WindSpeedSoftClampZone		(branchBendEtc_WindSpeedSoftClamp.y)
	#define branchBendEtc_GlobalAlphaCardStiffness	(branchBendEtc_WindSpeedSoftClamp.z)

	#define branchBendEtc_UnderWaterTransition_BlendA	(branchBendEtc_UnderWaterTransition.x)
	#define branchBendEtc_UnderWaterTransition_BlendB	(branchBendEtc_UnderWaterTransition.y)

#if TREES_INCLUDE_SFX_WIND && !__LOW_QUALITY
	#define branchBend_AABBMin				(branchBendEtc_AABBInfo[0].xyz)
	#define branchBend_HasSfxWind			(branchBendEtc_AABBInfo[0].w)
	#define branchBend_OneOverAABBDiagonal	(branchBendEtc_AABBInfo[1].xyz)

	shared	float4 branchBendEtc_AABBInfo[2]			 : branchBendEtc_AABBInfo0					REGISTER(		c191) < int nostrip = 1; >;
	shared	float4 branchBendEtc_SfxWindEvalModulation	 : branchBendEtc_SfxWindEvalModulation0		REGISTER(		c193) < int nostrip = 1; >;
	shared	float4 branchBendEtc_SfxWindValueModulation	 : branchBendEtc_SfxWindValueModulation0	REGISTER(		c194) < int nostrip = 1; >;

	#define branchBend_SfxEvalModulationFrequencies			(branchBendEtc_SfxWindEvalModulation.xyz)
	#define branchBend_SfxEvalModulationAmplitude			(branchBendEtc_SfxWindEvalModulation.w)

	#define branchBend_SfxValueModulationProportion			(branchBendEtc_SfxWindValueModulation.x)
#endif

#if !SHADER_FINAL

	// Debug/control.
	shared	float4 branchBendEtc_Control1			: branchBendEtc_Control10							REGISTER(		c195) < int nostrip = 1; >;
	shared	float4 branchBendEtc_Control2			: branchBendEtc_Control20							REGISTER(		c196) < int nostrip = 1; >;
	shared	float4 branchBendEtc_DebugRenderControl1: branchBendEtc_DebugRenderControl10	REGISTER(		c197) < int nostrip = 1; >;
	shared	float4 branchBendEtc_DebugRenderControl2: branchBendEtc_DebugRenderControl20	REGISTER(		c198) < int nostrip = 1; >;
	shared	float4 branchBendEtc_DebugRenderControl3: branchBendEtc_DebugRenderControl30	REGISTER(		c199) < int nostrip = 1; >;

	#define triuMovement_Enabled				(branchBendEtc_Control1.x)
	#define triuMovement_NoBranchBending		(branchBendEtc_Control1.y)
	#define triuMovement_NouMovement			(branchBendEtc_Control1.z)

	#define branchBend_PhaseVariationEnabled	(branchBendEtc_Control1.w) 
	#define branchBend_NoRegularWind			(branchBendEtc_Control2.x)
	#define branchBend_NoSfxWind				(branchBendEtc_Control2.y)
	#define branchBend_NoAlphaCardVariation		(branchBendEtc_Control2.z)
	#define branchBend_StiffnessRenderRange		(branchBendEtc_DebugRenderControl2.z)

#else // !SHADER_FINAL

	shared	float4 branchBendEtc_Control1Fake			: branchBendEtc_Control10							REGISTER(		c195) < int nostrip = 1; >;
	shared	float4 branchBendEtc_Control2Fake			: branchBendEtc_Control20							REGISTER(		c196) < int nostrip = 1; >;
	shared	float4 branchBendEtc_DebugRenderControl1Fake: branchBendEtc_DebugRenderControl10	REGISTER(		c197) < int nostrip = 1; >;
	shared	float4 branchBendEtc_DebugRenderControl2Fake: branchBendEtc_DebugRenderControl20	REGISTER(		c198) < int nostrip = 1; >;
	shared	float4 branchBendEtc_DebugRenderControl3Fake: branchBendEtc_DebugRenderControl30	REGISTER(		c199) < int nostrip = 1; >;

	// Debug/control.
	#define triuMovement_Enabled				(1.0f)
	#define triuMovement_NoBranchBending		(1.0f)
	#define triuMovement_NouMovement			(1.0f)
	#define branchBend_WindVariationEnabled		(1.0f)
	#define branchBendEtc_DebugRenderControl1	float4(0.0f, 0.0f, 0.0f, 0.0f)
	#define branchBendEtc_DebugRenderControl2	float4(0.0f, 0.0f, 0.0f, 0.0f)
	#define branchBendEtc_DebugRenderControl3	float4(0.0f, 0.0f, 0.0f, 0.0f)

	#define branchBend_PhaseVariationEnabled	1.0f
	#define branchBend_NoRegularWind			1.0f
	#define branchBend_NoSfxWind				1.0f
	#define branchBend_NoAlphaCardVariation		1.0f
	#define branchBend_StiffnessRenderRange		0.0f

#endif // !SHADER_FINAL

EndConstantBufferDX10(tri_uMovment_and_branch_bend_shared)


//------------------------------------------------------------------------------//
// uMovement bend funcs.														//
//------------------------------------------------------------------------------//


struct MICROMOVEMENT_PROPERTES
{
	float4 FreqAndAmp[TREES_NUMBER_OF_BASIS_WAVES];
};

float2 TriangleWave(float2 x)
{
	return 1.0f - 2.0f*abs(frac(x) - 0.5f);
}

float2 SmoothedTriangleWave(float2 x)
{
	float2 t = TriangleWave(x); // Basic triangel wave (T(0)= 0, T(0.5) = 1, T(1) = 0).
	t = t*t*(3.0f - 2.0f*t); // Smoomth at 0 and 1.
	return 2.0f*t - 1.0f; // Transform into [-1, 1] space.
}


float3 TriangleWave3(float3 x)
{
	return 1.0f - 2.0f*abs(frac(x) - 0.5f);
}


float3 SmoothedTriangleWave3(float3 x)
{
	float3 t = TriangleWave3(x); // Basic triangel wave (T(0)= 0, T(0.5) = 1, T(1) = 0).
	t = t*t*(3.0f - 2.0f*t); // Smoomth at 0 and 1.
	return 2.0f*t - 1.0f; // Transform into [-1, 1] space.
}


float3 ComputeMicromovement(float3 modelspacenormal, float3 color0, float globalTimer, MICROMOVEMENT_PROPERTES umProperties)
{
	float umScaleH	= color0.r;								// * triuMovement_AmplitudeH;	// horizontal movement scale (red0:   255=max, 0=min)
	float umScaleV	= color0.b;								// * triuMovement_AmplitudeV;	// vertical movement scale	 (blue0:  255=max, 0=min)
	float umArgPhase= abs(color0.g+triuMovement_GlobalPhaseShift);	// phase shift               (green0: phase 0-1     )

	// We need this to chose a "consistent" normal when back face culling is not set properly (i.e. when both sides are rendered).
	float3 normalToUse = sign(modelspacenormal.z)*modelspacenormal;
	// LDS DMC TEMP:-
	//float3 normalToUse = modelspacenormal;

	float2 waveSum = float2(0.0f, 0.0f);

	[loop]
	for(int i=0; i<TREES_NUMBER_OF_BASIS_WAVES; i++)
	{
		float2 triWaveArg = float2(globalTimer, globalTimer)*float2(umProperties.FreqAndAmp[i].x,umProperties.FreqAndAmp[i].z) + umArgPhase; 
		waveSum += SmoothedTriangleWave(triWaveArg)*float2(umProperties.FreqAndAmp[i].y, umProperties.FreqAndAmp[i].w);
	}

	// Blend um between low and high wind speeds.
	float sumOfWaves = (1.0f - triuMovement_umHighLowBlend)*waveSum.x + triuMovement_umHighLowBlend*waveSum.y;
	// Calculate final micro-movement.
	float3 micromovement = sumOfWaves*umScaleH*float3(normalToUse.xy, 0.0f) + sumOfWaves*umScaleV*float3(0.0f, 0.0f, normalToUse.z);

	return micromovement;
}


// TODO:- optimise this normalToUse (it`s done in the above also).
float ComputeUmPhase(float3 modelspacenormal)
{
	// We need this to chose a "consistent" normal when back face culling is not set properly (i.e. when both sides are rendered).
	float3 normalToUse = sign(modelspacenormal.z)*modelspacenormal;
	return normalToUse.x + normalToUse.y + normalToUse.z; 
}

//------------------------------------------------------------------------------//
// Helper functions.															//
//------------------------------------------------------------------------------//

struct BRANCH_BEND_PROPERTIES
{
	float PivotHeight;
	float TrunkStiffnessAdjustLow;
	float TrunkStiffnessAdjustHigh;
	float PhaseStiffnessAdjustLow;
	float PhaseStiffnessAdjustHigh;
};


float ComputePhaseStiffness(float s, BRANCH_BEND_PROPERTIES bendProperties)
{
	float stiffness = s*(bendProperties.PhaseStiffnessAdjustHigh - bendProperties.PhaseStiffnessAdjustLow) + bendProperties.PhaseStiffnessAdjustLow;
	stiffness = 1.0f - exp(-stiffness);
	//stiffness = min(stiffness, 1.0f);
	//stiffness = max(stiffness, 0.0f);
	//return pow(stiffness, StiffnessPower);
	return stiffness;
}


float ComputeTrunkStiffness(float s, BRANCH_BEND_PROPERTIES bendProperties)
{
	float stiffness = s*(bendProperties.TrunkStiffnessAdjustHigh - bendProperties.TrunkStiffnessAdjustLow) + bendProperties.TrunkStiffnessAdjustLow;
	stiffness = 1.0f - exp(-stiffness);
	//stiffness = min(stiffness, 1.0f);
	//stiffness = max(stiffness, 0.0f);
	return stiffness;
}


//------------------------------------------------------------------------------//
// Debug colour funcs.															//
//------------------------------------------------------------------------------//

#if !SHADER_FINAL

float ComputeStiffnessRenderValue(float stiffness)
{
	return min(max(0.0f, (stiffness - branchBend_StiffnessRenderRange)/(1.0f - branchBend_StiffnessRenderRange)), 1.0f);
}

float4 CalcWindDebugColour_Foliage(float4 uMovementColour, float3 normal, BRANCH_BEND_PROPERTIES bendProperties)
{
	float phaseStiffness = ComputePhaseStiffness(uMovementColour.a, bendProperties);
	float trunkStiffness = ComputeTrunkStiffness(uMovementColour.a, bendProperties);

	float3 baseColour = uMovementColour*branchBendEtc_DebugRenderControl1.rgb + ComputeUmPhase(normal)*float3(0.0f, branchBendEtc_DebugRenderControl2.g, 0.0f);
	float stiffnessAlpha = phaseStiffness*branchBendEtc_DebugRenderControl2.a + trunkStiffness*branchBendEtc_DebugRenderControl2.r;
	stiffnessAlpha = ComputeStiffnessRenderValue(stiffnessAlpha);

	float3 final1 = (1.0f - stiffnessAlpha)*baseColour + float3(stiffnessAlpha, stiffnessAlpha, stiffnessAlpha);
	float3 final2 = (1.0f - branchBendEtc_DebugRenderControl3.w)*final1 + branchBendEtc_DebugRenderControl3.xyz*branchBendEtc_DebugRenderControl3.w;
	return float4(final2, sign(branchBendEtc_DebugRenderControl1.a));
}


float4 CalcWindDebugColour(float4 uMovementColour, BRANCH_BEND_PROPERTIES bendProperties)
{
	float phaseStiffness = ComputePhaseStiffness(uMovementColour.a, bendProperties);
	float trunkStiffness = ComputeTrunkStiffness(uMovementColour.a, bendProperties);

	float3 baseColour = uMovementColour*branchBendEtc_DebugRenderControl1.rgb;
	float stiffnessAlpha = phaseStiffness*branchBendEtc_DebugRenderControl2.a + trunkStiffness*branchBendEtc_DebugRenderControl2.r;
	stiffnessAlpha = ComputeStiffnessRenderValue(stiffnessAlpha);

	float3 final1 = (1.0f - stiffnessAlpha)*baseColour + float3(stiffnessAlpha, stiffnessAlpha, stiffnessAlpha);
	float3 final2 = (1.0f - branchBendEtc_DebugRenderControl3.w)*final1 + branchBendEtc_DebugRenderControl3.xyz*branchBendEtc_DebugRenderControl3.w;
	return float4(final2, sign(branchBendEtc_DebugRenderControl1.a));
}


float4 CalcWindDebugColour_AlphaCardOnly(float4 uMovementColour, float umStiffnessMultiplier)
{
	float3 baseColour = uMovementColour*branchBendEtc_DebugRenderControl1.rgb;
	float stiffnessAlpha = (1.0f - (max(uMovementColour.r, uMovementColour.b)*(1.0f - branchBendEtc_GlobalAlphaCardStiffness*umStiffnessMultiplier)))*branchBendEtc_DebugRenderControl2.r;
	stiffnessAlpha = ComputeStiffnessRenderValue(stiffnessAlpha);

	float3 final1 = (1.0f - stiffnessAlpha)*baseColour + float3(stiffnessAlpha, stiffnessAlpha, stiffnessAlpha);
	float3 final2 = (1.0f - branchBendEtc_DebugRenderControl3.w)*final1 + branchBendEtc_DebugRenderControl3.xyz*branchBendEtc_DebugRenderControl3.w;
	return float4(final2, sign(branchBendEtc_DebugRenderControl1.a));
}

#endif // !SHADER_FINAL

//------------------------------------------------------------------------------//
// Under water blend functions.													//
//------------------------------------------------------------------------------//

float ComputeWindUnderWaterBlend(float3 modelspacepos)
{
	// Transforms into worldspace, then applied (x-h)/blendrange.
	return saturate(dot(branchBendEtc_UnderWaterTransition.xyz, modelspacepos) + branchBendEtc_UnderWaterTransition.w);
}

//------------------------------------------------------------------------------//
// Wind vector funcs.															//
//------------------------------------------------------------------------------//

float4 CalculateWindVector(float phase, float globalTimer, float onOffSwitch)
{
	float2 arg = float2(branchBend_Freq1, branchBend_Freq2)*float2(globalTimer, globalTimer) + float2(phase, phase) + float2(triuMovement_GlobalPhaseShift, triuMovement_GlobalPhaseShift);
	float2 blend = SmoothedTriangleWave(arg);
	float2 invBlend = float2(1.0f, 1.0f) - blend;

	float4 factors = float4(invBlend.x*invBlend.y, blend.x*invBlend.y, invBlend.x*blend.y, blend.x*blend.y);
	float3 newWind = factors.x*branchBendEtc_WindVector[0] + factors.y*branchBendEtc_WindVector[1] + factors.z*branchBendEtc_WindVector[2] + factors.w*branchBendEtc_WindVector[3]; 
	//float k = 1.0f; //branchBend_WindVariationEnabled;
	float k = onOffSwitch;
	float3 finalWind = (1.0f - k)*branchBendEtc_WindVector[0] + k*newWind;
	return float4(finalWind, 0.0f);
}


float3 CalculateSfxWindVector(float3 pos)
{
#if (TREES_INCLUDE_SFX_WIND && !__LOW_QUALITY) && __SHADERMODEL>=40 && (WIND_DISPLACEMENT || IS_TREE_DRAW)
	if(branchBend_HasSfxWind > 0.0f)
	{
		float4 coord = float4((pos - branchBend_AABBMin)*branchBend_OneOverAABBDiagonal, 0.0f);
		return tex3Dlod(SfxWindSampler3D, coord);
	}
	else
#endif // (TREES_INCLUDE_SFX_WIND && !__LOW_QUALITY) && __SHADERMODEL>=40 && (WIND_DISPLACEMENT || IS_TREE_DRAW)
	{
		return float3(0.0f, 0.0f, 0.0f);
	}
}


float3 CalculatePhaseBasedSfxWind(float3 pos, float phase, float globalTimer)
{
#if TREES_INCLUDE_SFX_WIND && !__LOW_QUALITY
	float3 arg = branchBend_SfxEvalModulationFrequencies*globalTimer + float3(phase, phase, phase) + float3(triuMovement_GlobalPhaseShift, triuMovement_GlobalPhaseShift, triuMovement_GlobalPhaseShift);
	float3 triWaves = SmoothedTriangleWave3(arg);
	float3 displacement = triWaves*branchBend_SfxEvalModulationAmplitude;
	float3 windValue = CalculateSfxWindVector(pos + displacement);

	float scale = (1.0f - branchBend_SfxValueModulationProportion) + 0.5f*(triWaves.x + 1.0f)*branchBend_SfxValueModulationProportion;
	return scale * windValue;
#else // TREES_INCLUDE_SFX_WIND && !__LOW_QUALITY
	return float3(0.0f, 0.0f, 0.0f);
#endif // TREES_INCLUDE_SFX_WIND && !__LOW_QUALITY
}


//------------------------------------------------------------------------------//
// Branch bend funcs.															//
//------------------------------------------------------------------------------//


float3 ApplySoftClamp(float3 windByStiffness, float stiffness)
{
	// Prevent a zero length vector.
	float l = length(abs(windByStiffness) + float3(0.001f, 0.0f, 0.0f));

	// Do calculations in "multiplied by stiffness space" (can`t divide by stiffness in case it`s zero).
	float unrestricted = branchBend_WindSpeedUnrestricted*stiffness;
	float softClampZone = branchBend_WindSpeedSoftClampZone*stiffness;
	
	float k = max(0.0f, l - unrestricted)/softClampZone;
	float softClamped = softClampZone*(1.0f - exp(-k)) + unrestricted;

	float scale = fselect_gt(l, unrestricted, softClamped/l, 1.0f);
	return windByStiffness*scale;
}

float3 ComputeBranchWind(float4 uMovementColour, float globalTimer, BRANCH_BEND_PROPERTIES bendProperties, out float vertexOneMinusTrunkStiffness)
{
	// Calculate base stiffness (for whole trunk).
	float trunkStiffness = ComputeTrunkStiffness(uMovementColour.a, bendProperties);
	// Compute stiffness for phase variation.
	float phaseStiffness = ComputePhaseStiffness(uMovementColour.a, bendProperties);

	vertexOneMinusTrunkStiffness = 1.0f - trunkStiffness;
	float vertexOneMinusPhaseStiffness = 1.0f - phaseStiffness;

	// Just main wind vector.
	float3 trunkWind = float3(branchBendEtc_WindVector[0].x, branchBendEtc_WindVector[0].y, branchBendEtc_WindVector[0].z);
	// Derived from phase variation.
	float3 phaseVariationWind = CalculateWindVector(uMovementColour.g, globalTimer, 1.0f);

	return trunkWind*vertexOneMinusTrunkStiffness + (phaseVariationWind - trunkWind)*vertexOneMinusPhaseStiffness*branchBend_PhaseVariationEnabled;
}


float3 ComputeBranchBend(float3 modelspacevertpos, float4x4 worldMtx, float4 uMovementColour, float globalTimer, BRANCH_BEND_PROPERTIES bendProperties)
{
	float oneMinustrunkStiffness;
	float3 branchWind = ComputeBranchWind(uMovementColour, globalTimer, bendProperties, oneMinustrunkStiffness);
	// Arrive at total wind (branch wind + Sfx wind).
	float3 totalWind = branchBend_NoRegularWind*branchWind; // + branchBend_NoSfxWind*oneMinustrunkStiffness*CalculateSfxWindVector(modelspacevertpos); 

	// Transform wind into modelspace.
	float3 wind = mul(worldMtx, float4(totalWind, 0.0f))*triuMovement_NoBranchBending;

	// We bend round the "pivot" point.
	float3 originalOffset = modelspacevertpos - float3(0.0f, 0.0f, bendProperties.PivotHeight);
	float originalLen = length(originalOffset);

	wind = ApplySoftClamp(wind, oneMinustrunkStiffness);

	float3 bentOffset = originalOffset + wind;
	float bentLen = length(bentOffset);

	// Maintain original distance form pivot.
	float3 branchBendPos = bentOffset*(originalLen/bentLen) + float3(0.0f, 0.0f, bendProperties.PivotHeight);

	return branchBendPos;
}


float3 ComputeBranchBend_Foliage(float3 modelspacevertpos, float4x4 worldMtx, float4 uMovementColour, float umPhase, float globalTimer, BRANCH_BEND_PROPERTIES bendProperties)
{
	float oneMinustrunkStiffness;

	// Calculate a wind vector using the micro-movement phase.
	float3 alphaCardWind = branchBend_NoRegularWind*CalculateWindVector(umPhase, globalTimer, branchBend_NoAlphaCardVariation) + branchBend_NoSfxWind*CalculatePhaseBasedSfxWind(modelspacevertpos, umPhase, globalTimer);

	// And some wind for the branch.
	float3 branchWind = branchBend_NoRegularWind*ComputeBranchWind(uMovementColour, globalTimer, bendProperties, oneMinustrunkStiffness);

	// Blend between the two using um amplitude to teather it to the branch.
	float k = min(uMovementColour.r, uMovementColour.b);
	float3 blendedWind = (1.0f - k)*branchWind + k*alphaCardWind*oneMinustrunkStiffness; // Foliage uses same stiffness as main branch.

	// Arrive at total wind (blended wind + Sfx wind).
	float3 windWorldSpace = blendedWind; //+ branchBend_NoSfxWind*oneMinustrunkStiffness*CalculateSfxWindVector(modelspacevertpos);

	// Transform wind into modelspace.
	float3 wind = mul(worldMtx, float4(windWorldSpace, 0.0f))*triuMovement_NoBranchBending;

	// We bend round the "pivot" point.
	float3 originalOffset = modelspacevertpos - float3(0.0f, 0.0f, bendProperties.PivotHeight);
	float originalLen = length(originalOffset);

	wind = ApplySoftClamp(wind, oneMinustrunkStiffness);

	float3 bentOffset = originalOffset + wind;
	float bentLen = length(bentOffset);

	// Maintain original distance form pivot.
	float3 branchBendPos = bentOffset*(originalLen/bentLen) + float3(0.0f, 0.0f, bendProperties.PivotHeight);

	return branchBendPos;
}


//------------------------------------------------------------------------------//
// Bend + uMovement funcs.														//
//------------------------------------------------------------------------------//


float3 ComputeBranchBendPlusFoliageMicromovements(float3 modelspacevertpos, float3 modelspacenormal, float4 IN_Color0, float4x4 worldMtx, float globalTimer, BRANCH_BEND_PROPERTIES bendProperties, MICROMOVEMENT_PROPERTES umProperties)
{
	// Note:- We`re deriving phase from the normal because the test model doesn`t have decent phase painted.
	float umPhase = ComputeUmPhase(modelspacenormal);
	float3 umColour = float3(IN_Color0.r, umPhase, IN_Color0.b);
	float3 micromovement = ComputeMicromovement(modelspacenormal, umColour, globalTimer, umProperties);
	float3 branchBendPos = ComputeBranchBend_Foliage(modelspacevertpos, worldMtx, IN_Color0, umPhase, globalTimer, bendProperties);

	// Sum branch bend + and micro movement for final position.
	float3 newVertpos = branchBendPos + micromovement*triuMovement_NouMovement;

	return newVertpos;
}

//------------------------------------------------------------------------------//
// Cut down version (only aplha cards can move).								//
//------------------------------------------------------------------------------//


float3 ComputeBranchBend_AlphaCardOnly(float3 modelspacevertpos, float4 IN_Color0, float4x4 worldMtx, float globalTimer, float umStiffnessMultiplier)
{
	// Alpha card only uses global stiffness.
	float oneMinusStiffness = 1.0f - saturate(branchBendEtc_GlobalAlphaCardStiffness*umStiffnessMultiplier);

	// Calculate a wind vector using the micro-movement phase.
	float3 alphaCardWind = branchBend_NoRegularWind*CalculateWindVector(IN_Color0.g, globalTimer, branchBend_NoAlphaCardVariation) + branchBend_NoSfxWind*CalculatePhaseBasedSfxWind(modelspacevertpos, IN_Color0.g, globalTimer);

	// Teather it to the branch.
	float k = max(IN_Color0.r, IN_Color0.b);
	float3 blendedWind = k*alphaCardWind*oneMinusStiffness; 

	// Arrive at total wind.
	float3 windWorldSpace = blendedWind;

	// Transform wind into modelspace.
	float3 wind = mul(worldMtx, float4(windWorldSpace, 0.0f))*triuMovement_NoBranchBending;

	// We bend round the "pivot" point (alpha card only uses 0 as pivot).
	float3 originalOffset = modelspacevertpos - float3(0.0f, 0.0f, 0.0f);
	float originalLen = length(originalOffset);

	wind = ApplySoftClamp(wind, oneMinusStiffness);

	float3 bentOffset = originalOffset + wind;
	float bentLen = length(bentOffset);

	// Maintain original distance form pivot.
	float3 branchBendPos = bentOffset*(originalLen/bentLen) + float3(0.0f, 0.0f, 0.0f);

	return branchBendPos;
}

#endif // (TREES_ENABLE_BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS || __MAX) && !__LOW_QUALITY

#endif //__TREES_WINDFUNCS_FXH__...
