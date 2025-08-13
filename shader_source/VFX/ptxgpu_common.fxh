#ifndef RAGE_PTXGPURENDER_H
#define RAGE_PTXGPURENDER_H

#if !(__SHADERMODEL >= 40)
struct ptxgpuVertexIn
{
#if __XENON
	int index				: INDEX;
#else
	float4 position			: TEXCOORD0;
	float4 velocity			: TEXCOORD1;
	float2 index			: TEXCOORD2;
#endif
};
#endif //!__WIN32PC || !(__SHADERMODEL >= 40)

struct ptxgpuParticleInfo
{
	float4 pos;
	float2 texCoord;
	float3 worldPos;
	float3 normal;
	float  life;
};

///////////////////////////////////////////////////////////////////////////////
// DX11 WIN32 specific code.
///////////////////////////////////////////////////////////////////////////////

#if __SHADERMODEL >= 40

BeginConstantBufferDX10( ptxgpu_common_locals )
float4 gParticleTextureSize : ParticleTextureSize;
EndConstantBufferDX10( ptxgpu_common_locals )

// On PC we read from the position textures using PositionUV to create a ptxgpuVertexIn.

// particle position xy
BeginSampler(sampler, ParticlePos, ParticlePosTexSampler, ParticlePos)
ContinueSampler(sampler, ParticlePos, ParticlePosTexSampler, ParticlePos)
	MinFilter = POINT;
	MagFilter = POINT;
	MipFilter = NONE;
    AddressU  = CLAMP;        
    AddressV  = CLAMP;
EndSampler;

// particle position zw
BeginSampler(sampler, ParticleVel, ParticleVelTexSampler, ParticleVel)
ContinueSampler(sampler, ParticleVel, ParticleVelTexSampler, ParticleVel)
	MinFilter = POINT;
	MagFilter = POINT;
	MipFilter = NONE;
    AddressU  = CLAMP;        
    AddressV  = CLAMP;
EndSampler;


struct ptxgpuLookUpVertexIn
{
	float2 Index			: TEXCOORD0;
};

struct ptxgpuVertexIn
{
	float4 position;
	float4 velocity;
	float2 index;
};

// Create a ptxgpuVertexIn from the passed ptxgpuLookUpVertexIn and position/velocity textures.
ptxgpuVertexIn Create_ptxgpuVertexIn(ptxgpuLookUpVertexIn Input, uint InstanceID)
{
	ptxgpuVertexIn Ret;
	
	float4 uv;
	// Create a look up texture coord using the instanceID and texture dimensions.
	uv.x = fmod((float)InstanceID, gParticleTextureSize.x);
	uv.y = floor((float)InstanceID/gParticleTextureSize.x);
	uv.z = 0.0f;
	uv.w = 0.0f;
	uv.xy *= gParticleTextureSize.zw;
	
	Ret.position = tex2Dlod(ParticlePosTexSampler, uv);
	Ret.velocity = tex2Dlod(ParticleVelTexSampler, uv);
	Ret.index = Input.Index;
	return Ret;
}

#endif // __SHADERMODEL >= 40

///////////////////////////////////////////////////////////////////////////////
// Common code.
///////////////////////////////////////////////////////////////////////////////

// PURPOSE : Calculate wrapping texture frame animation
float ptxgpuGetTextureFrameWrap( float life, float uCoord, float4 TexAnimation )
{
	return frac( ( int)(life * TexAnimation.x - TexAnimation.y )	 * TexAnimation.z )+ TexAnimation.z * uCoord;
}

// PURPOSE : Calculate clampping texture frame animation
float ptxgpuGetTextureFrameClamp( float life, float uCoord, float4 TexAnimation )
{
	return saturate( ( int)(life * TexAnimation.x - TexAnimation.y )  * TexAnimation.z ) + TexAnimation.z * uCoord;
}

#if __XENON
// PURPOSE : Get the State which is a 4 element position and velocity values
// REMARKS: This is the only cross platform function
void ptxgpuGetParticleState( ptxgpuVertexIn IN, out float2 uvs, out float4 Position4, out float4 Velocity4 )
{
	int index = IN.index;
	int idx = index / 4;
    asm {
        vfetch Position4.xyzw, idx, texcoord0
        vfetch Velocity4.xyzw, idx, texcoord1
    };

	uvs = float2( saturate( (( index + 1 )/2) %2 ), saturate( (index /2)% 2 ) ); 
}

#else

void ptxgpuGetParticleState( ptxgpuVertexIn IN, out float2 uvs,   out float4 Position4, out float4 Velocity4)
{
	Position4 =  IN.position;
	Velocity4 = IN.velocity;

	uvs = IN.index;
}
#endif

// PURPOSE : Uses the state and stanard setting creates the Particle information
// REMARKS: This is following from rmptfx
//
ptxgpuParticleInfo ptxgpuParticleCreate( float2 uvs , float4 Position4, float4 Velocity4, bool isDirectional, float radius, float motionBlur )
{
	ptxgpuParticleInfo OUT=(ptxgpuParticleInfo)0;

	OUT.life = Position4.w;

	OUT.texCoord = uvs;
	
	uvs.xy = uvs.xy - 0.5f;

	float3 velocity = float3( Velocity4.xyz );
	
	float3 up;
	float3 across;
	float3  offset;
	if ( isDirectional )
	{

		across = normalize(velocity );
		float3 viewDir = gViewInverse[3].xyz - Position4.xyz ;
	
		up = normalize( cross( across, viewDir ) );
		across = (radius * across) + (motionBlur.xxx * velocity );
		up *=radius;
		offset = uvs.x * up + (0.7 - ( uvs.y + 0.5f)) * -across;
	}	
	else
	{
		up = gViewInverse[0].xyz * radius ; 
		across = -gViewInverse[1].xyz * radius;
		offset = uvs.x * up + uvs.y * across;
	}
	
	OUT.normal = normalize( offset );
	
	// Transform position to the clipping space 
	float4 pos = float4( Position4.xyz, 1.0f );
	pos.xyz += offset.xyz;

	OUT.worldPos = pos.xyz;
	OUT.pos = (float4)mul(pos, gWorldViewProj);
	
	return OUT;
}

// Pack the seed into the range 0-255 and the normalize the initial life into the range <epsilon>-1/maxLife
float PackVelW(float maxLife, float initLife, float seed)
{
	float initLifeNorm = initLife / maxLife;
	return ceil(255.0 * initLifeNorm) + seed;
}

// Unpack the seed and initial life values
void UnPackVelW(float packedFloat, float maxLife, out float initLife, out float seed)
{
	seed = frac(packedFloat);
	float initLifeNorm = (packedFloat - seed) / 255.0;
	initLife = initLifeNorm * maxLife;
}

#endif


