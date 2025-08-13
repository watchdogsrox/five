
#ifndef PRAGMA_DCL
	#pragma dcl position normal diffuse texcoord0 texcoord1 
	#define PRAGMA_DCL
#endif
///////////////////////////////////////////////////////////////////////////////
// GPUPTFX_UPDATE_DROP.FX - gpu drop particle (raindrop, snowdrop etc)
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// DEFINES
///////////////////////////////////////////////////////////////////////////////

#define DISABLE_RAGE_LIGHTING
#define NO_SKINNING

#define PTXGPU_USE_WIND						(1)
#define PTXGPU_USE_WIND_DIST_FIELD			(0)	&& (PTXGPU_USE_WIND)
#define PTXGPU_WINDFIELD_NUM_ELEMENTS_X		(32)
#define PTXGPU_WINDFIELD_NUM_ELEMENTS_Y		(32)
#define PTXGPU_WINDFIELD_NUM_ELEMENTS_Z		(8)

///////////////////////////////////////////////////////////////////////////////
// INCLUDES
///////////////////////////////////////////////////////////////////////////////

#include "../../../rage/base/src/shaderlib/rage_common.fxh"
#include "../common.fxh"														// obey global register allocation rules
#include "ptxgpu_common.fxh"

///////////////////////////////////////////////////////////////////////////////
// SHADER VARIABLES 
///////////////////////////////////////////////////////////////////////////////

#if __WIN32PC
	#pragma constant 202	// WIN32PC: it doesn't matter at runtime, but helps dx9's fxc not to fail
#endif

BeginConstantBufferDX10( ptxgpu_update_locals )

ROW_MAJOR float4x4	gDepthViewProjMtx		: DepthViewProjMtx;

float3 gBoxForward : BoxForward;
float3 gBoxRight : BoxRight;
float3 gBoxCentrePos : BoxCentrePos;

// Transforming particles from local to worls space
float3 gRefParticlePos : RefParticlePos;
float3 gOffsetParticlePos : OffsetParticlePos;

float4		gGravityWindTimeStepCut	: GravityWindTimeStepCut;	// x: Gravity, y: Wind Influence, z: TimeStep, w: 0 after camera cut, 1 otherwise

float		gResetParticles			: ResetParticles;

// emitter settings
float3  	gBoxSize				: BoxSize;   

float3  	gLifeMinMaxClampToGround: LifeMinMaxClampToGround;
float3		gVelocityMin			: VelocityMin;	
float3		gVelocityMax			: VelocityMax;


// wind related
float4		gWindBaseVel			: WindBaseVel;			// xyz: WindBaseVel, w: GameWindSpeed
float4		gWindGustVel			: WindGustVel;			// xyz: WindGustVel, w: unused
float4		gWindVarianceVel		: WindVarianceVel;		// xyz: WindVarianceVel, w: GameWindPositionalVarMult
float4		gWindMultParams			: WindMultParams;		// x: GameWindGlobalVarSinePosMult, y: GameWindGlobalVarSineTimeMult
															// z: GameWindGlobalVarCosinePosMult, w: GameWindGlobalVarCosineTimeMult
															
EndConstantBufferDX10( ptxgpu_update_locals )


///////////////////////////////////////////////////////////////////////////////
// SAMPLERS
///////////////////////////////////////////////////////////////////////////////

#if !(__SHADERMODEL >= 40)
// particle position
BeginSampler(sampler, ParticlePos, ParticlePosTexSampler, ParticlePos)
ContinueSampler(sampler, ParticlePos, ParticlePosTexSampler, ParticlePos)
	MinFilter = POINT;
	MagFilter = POINT;
	MipFilter = NONE;
    AddressU  = CLAMP;        
    AddressV  = CLAMP;
EndSampler;

// particle velocity
BeginSampler(sampler, ParticleVel, ParticleVelTexSampler, ParticleVel)
ContinueSampler(sampler, ParticleVel, ParticleVelTexSampler, ParticleVel)
	MinFilter = POINT;
	MagFilter = POINT;
	MipFilter = NONE;
    AddressU  = CLAMP;        
    AddressV  = CLAMP;
EndSampler;
#endif //!__WIN32PC || !(__SHADERMODEL >= 40)

// height map
BeginSampler(sampler, ParticleCollisionTexture, ParticleCollisionSampler, ParticleCollisionTexture)
ContinueSampler(sampler, ParticleCollisionTexture, ParticleCollisionSampler, ParticleCollisionTexture)
	MinFilter = POINT;
	MagFilter = POINT;
	MipFilter = NONE;
    AddressU  = CLAMP;        
    AddressV  = CLAMP;
EndSampler;

// noise
BeginSampler(sampler, NoiseTexture, NoiseTexSampler, NoiseTexture)
ContinueSampler(sampler, NoiseTexture, NoiseTexSampler, NoiseTexture)
	MinFilter = POINT;
	MagFilter = POINT;
	MipFilter = NONE;
    AddressU  = WRAP;        
    AddressV  = WRAP;
EndSampler;

#if PTXGPU_USE_WIND_DIST_FIELD
// wind disturbance field xy
BeginSampler(sampler, DisturbanceFieldXY, DisturbanceFieldXYTexSampler, DisturbanceFieldXY)
ContinueSampler(sampler, DisturbanceFieldXY, DisturbanceFieldXYTexSampler, DisturbanceFieldXY)
	MinFilter = POINT;
	MagFilter = POINT;
	MipFilter = NONE;
    AddressU  = CLAMP;        
    AddressV  = CLAMP;
EndSampler;

// wind disturbance field z
BeginSampler(sampler, DisturbanceFieldZ, DisturbanceFieldZTexSampler, DisturbanceFieldZ)
ContinueSampler(sampler, DisturbanceFieldZ, DisturbanceFieldZTexSampler, DisturbanceFieldZ)
	MinFilter = POINT;
	MagFilter = POINT;
	MipFilter = NONE;
    AddressU  = CLAMP;        
    AddressV  = CLAMP;
EndSampler;
#endif


///////////////////////////////////////////////////////////////////////////////
// STRUCTURES
///////////////////////////////////////////////////////////////////////////////

struct PS_UPDATE_OUT
{
    float4 col0    : SV_Target0;
    float4 col1    : SV_Target1;
};

///////////////////////////////////////////////////////////////////////////////
// GLOBAL FUNCTIONS
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// DepthCompareHeightMap
// Return: x = 1 - no collison, -1 - collision
//         y = collision position Z
float2 DepthCompareHeightMap(float3 ptxPosition)
{	
	float4 pos = mul(float4(ptxPosition.xyz, 1.0f), gDepthViewProjMtx);

	pos.y = 1.0f - pos.y;

	float2 depth;
#if __PS3 
	depth.xy = 1.0f - rageTexDepth2D(ParticleCollisionSampler, pos.xy).x;
#elif __XENON
	asm 
    { 
		tfetch2D depth.xy__, pos.xy, ParticleCollisionSampler 
	};
#else
	depth.xy = fixupDepth(tex2D(ParticleCollisionSampler, pos.xy).xx);
#endif

#if __PS3 || __XENON 
	depth.x = (pos.z>depth.x) ? 1.0 : -1.0f;
#else
	depth.x = (pos.z<depth.x) ? 1.0 : -1.0f;
#endif

	return depth;
}


///////////////////////////////////////////////////////////////////////////////
// ptxgpuPackState

PS_UPDATE_OUT ptxgpuPackState(float4 ptxPosition, float4 ptxVelocity)
{
	PS_UPDATE_OUT output;

	output.col0 = ptxPosition;		
	output.col1 = ptxVelocity;

	return output;
}


///////////////////////////////////////////////////////////////////////////////
// ptxgpuUnPackState

void ptxgpuUnPackState(float2 texCoord0, out float4 ptxPosition, out float4 ptxVelocity)
{
	// just fetch 2 textures
	ptxPosition = tex2D(ParticlePosTexSampler, texCoord0);
	ptxVelocity = tex2D(ParticleVelTexSampler, texCoord0);
}


///////////////////////////////////////////////////////////////////////////////
// WIND RELATED FUNCTIONS
// these functions have been ported over from the code in pheffects\wind.h
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// ptxgpuCalcWindDistFieldTexCoord
#define PTXGPU_DISTFIELD_TEX_PAGE	(32)
#define PTXGPU_DISTFIELD_TEX_WIDTH	(256)
void ptxgpuCalcWindDistFieldTexCoord(float3 worldPos, out float2 texCoord)
{
	const int3 WINDFIELD_NUM_ELEMENTS_XYZ = int3(PTXGPU_WINDFIELD_NUM_ELEMENTS_X, PTXGPU_WINDFIELD_NUM_ELEMENTS_Y, PTXGPU_WINDFIELD_NUM_ELEMENTS_Z);
	const int3 WINDFIELD_ELEMENT_SIZE_XYZ = int3(1, 1, 1);
	int3 gridPos = ((int3)worldPos/WINDFIELD_ELEMENT_SIZE_XYZ);
	gridPos %= WINDFIELD_NUM_ELEMENTS_XYZ;

	texCoord.xy = (float2)gridPos.xy;
	texCoord.x += (float)(PTXGPU_DISTFIELD_TEX_PAGE*gridPos.z);	
	texCoord.xy /= float2(PTXGPU_DISTFIELD_TEX_WIDTH,PTXGPU_DISTFIELD_TEX_PAGE);
}

#if PTXGPU_USE_WIND_DIST_FIELD

///////////////////////////////////////////////////////////////////////////////
// ptxgpuUnsignedRangeToS16
//
// temporary workaround to shift and compress the full s16 range to positive numbers
// as at the moment there is no support to set a texture sampler to return signed values
// it is assumed that the actual range for signed values is [-16383,16383] to be able 
// to comfortably shift numbers into the [0, 32767] without aliasing.
float3 ptxgpuUnsignedRangeToS16(float3 unsignedVal)
{
	const float2 MINMAX_USHORT = float2(0.0f,32767.0f);
	const float2 MINMAX_SSHORT = float2(-16383.0f,16383.0f);
	float3 result = MINMAX_SSHORT.xxx + (unsignedVal.xyz - MINMAX_USHORT.xxx) * ((MINMAX_SSHORT.yyy-MINMAX_SSHORT.xxx)/(MINMAX_USHORT.yyy-MINMAX_USHORT.xxx));
	return result+0.5f.xxx;
}

///////////////////////////////////////////////////////////////////////////////
// ptxgpuGetDisturbanceFieldVel

float3 ptxgpuGetDisturbanceFieldVel(float3 ptxPosition)
{
	float2 tc;
	ptxgpuCalcWindDistFieldTexCoord(ptxPosition, tc);

	float3 distFieldVel;
#if __XENON
	distFieldVel.xy = tex2D(DisturbanceFieldXYTexSampler, tc).yx;
	distFieldVel.z = tex2D(DisturbanceFieldZTexSampler, tc).y;
#else
	distFieldVel.xy = tex2D(DisturbanceFieldXYTexSampler, tc).xy;
	distFieldVel.z = tex2D(DisturbanceFieldZTexSampler, tc).x;
#endif
	
	// bring values back to full signed range
	distFieldVel.xyz *= 65535.0f.xxx;
	distFieldVel = ptxgpuUnsignedRangeToS16(distFieldVel);

	// convert from fixed point
	const float s = 1.0f/256.0f;
	distFieldVel.xyz *= s.xxx;

	return distFieldVel;
}
#endif

///////////////////////////////////////////////////////////////////////////////
// ptxgpuGetPositionalWindVariance

float3 ptxgpuGetPositionalWindVariance(float3 ptxPosition, float2 texCoord)
{
	float2 texLookup = ptxPosition.xy;
	float3 randDir = tex2D(NoiseTexSampler, (texLookup.xy/16.0f)+texCoord.xy).xyz;
	randDir = (randDir * 2.0f.xxx) - 1.0f.xxx;
	randDir = normalize(randDir);

	randDir.xyz *= gWindBaseVel.www;
	randDir.xyz *= gWindVarianceVel.www;
	
	return randDir;
}

///////////////////////////////////////////////////////////////////////////////
// ptxgpuGetLocalWindVel

void ptxgpuGetLocalWindVel(float3 ptxPosition, float2 texCoord, out float3 windVel)
{
	windVel.xyz = gWindBaseVel.xyz;

	// add on gusts
	windVel.xyz += gWindGustVel.xyz;

	// add on global variance (on a sine wave)
	float3 gustMult = float3(0.0f,0.0f,1.0f);

	gustMult.xy = (ptxPosition.xy*gWindMultParams.xz) + (gGravityWindTimeStepCut.zz*gWindMultParams.yw);
	gustMult.xy	= sin(gustMult.xy);
	gustMult.xy = (gustMult.xy+1.0f.xx)*(0.5f.xx);

	windVel.xyz += gWindVarianceVel.xyz*gustMult.xyz;

	// add on positional variance
	windVel.xyz += ptxgpuGetPositionalWindVariance(ptxPosition, texCoord);

#if PTXGPU_USE_WIND_DIST_FIELD
	// add disturbance field contribution
	windVel.xyz += ptxgpuGetDisturbanceFieldVel(ptxPosition);
#endif
}

///////////////////////////////////////////////////////////////////////////////
// ptxgpuApplyWindInfluence

float3 ptxgpuApplyWindInfluence(float3 ptxPosition, float3 ptxVelocity, float2 texCoord)
{
	float3 vAirVel;
	ptxgpuGetLocalWindVel(ptxPosition.xyz, texCoord.xy, vAirVel);

	// calculate the difference between the particle and air velocity
	float3 vDeltaVel = vAirVel.xyz - ptxVelocity.xyz;

	// determine how quickly the particle velocity should match the wind velocity
	// if running at 30fps when a wind influence of 30 would match it instantly i.e. (delta mult = 1.0f)
	// a wind influence of 1 would mean it would take 1 second to match the wind velocity (delta mult = 1/30)
	float3 vDeltaMult = gGravityWindTimeStepCut.yyy * gGravityWindTimeStepCut.zzz;
	vDeltaMult = saturate(vDeltaMult);

	// apply the scale
	vDeltaVel *= vDeltaMult;

	return vDeltaVel.xyz;
}

///////////////////////////////////////////////////////////////////////////////
// PIXEL SHADERS
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// PS_ptxgpuUpdateDrop

PS_UPDATE_OUT PS_ptxgpuUpdateDrop(rageVertexOutputPassThroughTexOnly Input)
{
	// get the particle position and velocity 
	float4 ptxPosition;
	float4 ptxVelocity;
	ptxgpuUnPackState(Input.texCoord0.xy, ptxPosition, ptxVelocity);

	// Transform the position to world space
	ptxPosition.xyz += gRefParticlePos + gOffsetParticlePos;

	// Position W contains the current particle life value
	float curLife = abs(ptxPosition.w);
	
	// unpack the current and max life
	float initLife;
	float seed;
	UnPackVelW(ptxVelocity.w, gLifeMinMaxClampToGround.y, initLife, seed);

	// Use a constant up direction
	float3 boxUp = normalize(cross(gBoxRight, gBoxForward));

    // update the particle life
    curLife -= gGravityWindTimeStepCut.z;

	// check if the particle is dead
    if (curLife<=0.0f || gResetParticles>0.0f)
    {
		// the particle is dead - we need to emit a new one

		// get some random data
        float2 texLookup = ptxPosition.xy;
        float4 fRandom1 = tex2D(NoiseTexSampler, (texLookup.xy/16.0f)+Input.texCoord0.xy);
        float4 fRandom2 = tex2D(NoiseTexSampler, Input.texCoord0.xy);

		// position
		float3 emitRange = fRandom1.wzy-0.5f;
		ptxPosition.xyz = gBoxCentrePos;
		ptxPosition.xyz += gBoxRight * gBoxSize.x * emitRange.x;
		ptxPosition.xyz += gBoxForward * gBoxSize.y * emitRange.y;
		ptxPosition.xyz += boxUp * gBoxSize.z * emitRange.z;

		// velocity 
		ptxVelocity.xyz = gVelocityMin + (gVelocityMax.xyz-gVelocityMin)*fRandom2.xyz;

        // life
	    curLife = gLifeMinMaxClampToGround.x + (gLifeMinMaxClampToGround.y-gLifeMinMaxClampToGround.x)*fRandom1.w;
		initLife = curLife;

		// collision state and texture id
		seed = saturate(fRandom1.x - 0.004); // Seed can't be one
    }

    // update the particle position
	ptxPosition.xyz += ptxVelocity.xyz * gGravityWindTimeStepCut.zzz;

	// keep the particle inside the box
	float3 boxToPtx = ptxPosition.xyz - gBoxCentrePos;

	float dotRight = dot(gBoxRight, boxToPtx);
	float dotForward = dot(gBoxForward, boxToPtx);
	float dotUp = dot(boxUp, boxToPtx);
	
	// TODO: This code causes pops as the particle can changes position while fully opaque
 	ptxPosition.xyz -= gBoxRight * gBoxSize.x * ceil((dotRight/gBoxSize.x)-0.5f);
 	ptxPosition.xyz -= gBoxForward * gBoxSize.y * ceil((dotForward/gBoxSize.y)-0.5f);
 	ptxPosition.xyz -= boxUp * gBoxSize.z * ceil((dotUp/gBoxSize.z)-0.5f);

	// get the depth info for the new particle position (after keeping the particle within the box)
	float2 depthInfo2 = DepthCompareHeightMap(ptxPosition.xyz);

    // update the particle position (v += g * dt)
	ptxVelocity.xyz += float3(0.0f, 0.0f, gGravityWindTimeStepCut.x) * gGravityWindTimeStepCut.zzz;

#if PTXGPU_USE_WIND
	// update the particle's velocity with wind influence
	ptxVelocity.xyz += ptxgpuApplyWindInfluence(ptxPosition.xyz, ptxVelocity.xyz, Input.texCoord0.xy);
#endif

	// Transform particle position back to local space
	ptxPosition.xyz -= gRefParticlePos;
	
	// Store the current particle life value along with the collision info in position W
	// We saturate curLife it died and became negative
	ptxPosition.w = curLife * depthInfo2.x;
	
	// pack the current and max life
	ptxVelocity.w = PackVelW(gLifeMinMaxClampToGround.y, initLife, seed);

	// pack the position and velocity
	return ptxgpuPackState(ptxPosition, ptxVelocity);
}



///////////////////////////////////////////////////////////////////////////////
// PS_ptxgpuUpdateGround

PS_UPDATE_OUT PS_ptxgpuUpdateGround(rageVertexOutputPassThroughTexOnly Input)
{
	// get the particle position and velocity 
	float4 ptxPosition;
	float4 ptxVelocity;
	ptxgpuUnPackState(Input.texCoord0.xy, ptxPosition, ptxVelocity);

	// Transform the position to world space
	ptxPosition.xyz += gRefParticlePos + gOffsetParticlePos;

	float3 ptxStartPosition = ptxPosition.xyz;
	
	// Position W contains the current particle life value
	float curLife = ptxPosition.w;
	
	// unpack the current and max life
	float initLife;
	float seed;
	UnPackVelW(ptxVelocity.w, gLifeMinMaxClampToGround.y, initLife, seed);

	// Use a constant up direction
	float3 boxUp = float3(0.0f, 0.0f, 1.0f);

    // update the particle life
    curLife -= gGravityWindTimeStepCut.z;

	// check if the particle is dead
    if (curLife<=0.0f)
    {
		// the particle is dead - we need to emit a new one

		// get some random data
        float2 texLookup = ptxPosition.xy;
        float4 fRandom1 = tex2D(NoiseTexSampler, (texLookup.xy/16.0f)+Input.texCoord0.xy);
        float4 fRandom2 = tex2D(NoiseTexSampler, Input.texCoord0.xy);

		// position
		float3 emitRange = fRandom1.wzy-0.5f;
		ptxPosition.xyz = gBoxCentrePos;
		ptxPosition.xyz += gBoxRight * gBoxSize.x * emitRange.x;
		ptxPosition.xyz += gBoxForward * gBoxSize.y * emitRange.y;
		ptxPosition.xyz += boxUp * gBoxSize.z * emitRange.z;

		// fix to ground level
		float2 depthInfo1 = DepthCompareHeightMap(ptxPosition.xyz);
		float collisionZ = (depthInfo1.y - (gDepthViewProjMtx._43))/(gDepthViewProjMtx._33);
		ptxPosition.z = collisionZ;

		// velocity 
		ptxVelocity.xyz = gVelocityMin + (gVelocityMax.xyz-gVelocityMin)*fRandom2.xyz;
		
		// life
		curLife = gLifeMinMaxClampToGround.x + (gLifeMinMaxClampToGround.y-gLifeMinMaxClampToGround.x)*fRandom1.w;
		initLife = max(curLife, 0.0);

		// seed
		seed = saturate(fRandom1.x - 0.004); // Seed can't be one
    }

    // update the particle position
	ptxPosition.xyz += ptxVelocity.xyz * gGravityWindTimeStepCut.zzz;

	// keep the particle inside the box
	float3 ptxPosPreBox = ptxPosition.xyz;
	float3 boxToPtx = ptxPosition.xyz - gBoxCentrePos;

	float dotRight = dot(gBoxRight, boxToPtx);
	float dotForward = dot(gBoxForward, boxToPtx);
	
 	ptxPosition.xyz -= gBoxRight * gBoxSize.x * ceil((dotRight/gBoxSize.x)-0.5f);
 	ptxPosition.xyz -= gBoxForward * gBoxSize.y * ceil((dotForward/gBoxSize.y)-0.5f);

	float3 ptxPosMove = ptxPosition.xyz - ptxPosPreBox;

	bool ptxMoved = false;
	//if (any(ptxPosMove.xyz))
	if (dot(ptxPosMove.xyz, ptxPosMove.xyz) > 0.01)
	{
		// During normal play - restore life to initial value
		// This will force a fade in and help avoid a pop
		// For camera cuts we basically don't want to do anything.
		curLife += (initLife - curLife) * gGravityWindTimeStepCut.w;
		
		ptxMoved = true;
	}

	// get the depth info for the new particle position (after keeping the particle within the box)
	float2 depthInfo2 = DepthCompareHeightMap(ptxPosition.xyz);
	if (gLifeMinMaxClampToGround.z>0.0f)
	{
		float collisionZ = (depthInfo2.y - (gDepthViewProjMtx._43))/(gDepthViewProjMtx._33);
		ptxPosition.z = collisionZ;
	}
	else if (depthInfo2.x < 0.1f || ptxMoved)
	{
		float collisionZ = (depthInfo2.y - (gDepthViewProjMtx._43))/(gDepthViewProjMtx._33);

		if(gGravityWindTimeStepCut.w == 0.0)
		{
			ptxPosition.z = collisionZ;
		}
		else if (depthInfo2.y <= 0.0f)
		{
			// kill the particle if it's below the entire height map (below a z of zero)
			curLife = 0.0f;
		}
		else if (collisionZ - ptxPosition.z > 1.0f)
		{
			// reset the particle position if it's hit a barrier that's too high
			ptxPosition.xyz = ptxStartPosition;
		}
		else
		{
			// Let the particle raise gradually
			ptxPosition.z = min(ptxPosition.z + 0.05, collisionZ);
		}
	}

    // update the particle position (v += g * dt)
	ptxVelocity.xyz += float3(0.0f, 0.0f, gGravityWindTimeStepCut.x) * gGravityWindTimeStepCut.zzz;

#if PTXGPU_USE_WIND
	// update the particle's velocity with wind influence
	ptxVelocity.xyz += ptxgpuApplyWindInfluence(ptxPosition.xyz, ptxVelocity.xyz, Input.texCoord0.xy);
#endif

	// Transform particle position back to local space
	ptxPosition.xyz -= gRefParticlePos;
	
	// Position W contains the current particle life value
	ptxPosition.w = curLife;
	
	// pack the current and max life
	ptxVelocity.w = PackVelW(gLifeMinMaxClampToGround.y, initLife, seed);

	// pack the position and velocity
	return ptxgpuPackState(ptxPosition, ptxVelocity);
}


///////////////////////////////////////////////////////////////////////////////
// TECHNIQUES
///////////////////////////////////////////////////////////////////////////////

technique draw
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER VS_ragePassThroughNoXformTexOnly();
		PixelShader  = compile PIXELSHADER PS_ptxgpuUpdateDrop()	PS4_TARGET(FMT_32_ABGR);
	}
}

technique draw_drop
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER VS_ragePassThroughNoXformTexOnly();
		PixelShader  = compile PIXELSHADER PS_ptxgpuUpdateDrop()	PS4_TARGET(FMT_32_ABGR);
	}
}

technique draw_ground
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER VS_ragePassThroughNoXformTexOnly();
		PixelShader  = compile PIXELSHADER PS_ptxgpuUpdateGround()	PS4_TARGET(FMT_32_ABGR);
	}
}

