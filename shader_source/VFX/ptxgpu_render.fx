#ifndef PRAGMA_DCL
	#pragma dcl position
	#define PRAGMA_DCL
#endif

#define GLASS_LIGHTING (1)


///////////////////////////////////////////////////////////////////////////////
// GPUPTFX_RENDER.FX - gpu particle rendering shader
///////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// INCLUDES
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "../common.fxh"
#include "../Lighting/lighting_common.fxh"

#include "ptfx_common.fxh"
#include "ptxgpu_common.fxh"
#include "../../../rage/base/src/shaderlib/rage_diffuse_sampler.fxh"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// VARIABLES
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BeginDX10Sampler(sampler2D, Texture2D<float4>, BackBufferTexture, BackBufferSampler, BackBufferTexture)
ContinueSampler(sampler2D,BackBufferTexture,BackBufferSampler,BackBufferTexture)
	MinFilter = LINEAR;
	MagFilter = LINEAR;
	MipFilter = NONE;
	AddressU  = CLAMP; // MIRROR
	AddressV  = CLAMP; // MIRROR
EndSampler;

BeginDX10Sampler(sampler2D, Texture2D<float4>, DistortionTexture, DistortionSampler, DistortionTexture)
ContinueSampler(sampler2D,DistortionTexture,DistortionSampler,DistortionTexture)
	MinFilter = LINEAR;
	MagFilter = LINEAR;
	MipFilter = NONE;
	AddressU  = CLAMP; // MIRROR
	AddressV  = CLAMP; // MIRROR
EndSampler;

BeginConstantBufferDX10( ptxgpu_render_locals )

float gLightIntensityMult : LightIntensityMult
<	string UIName = "Light Intensity Multiplier : Controls the intensity of the directional and ambient contribution";
	float UIMin = 0.0;
	float UIMax = 2.0;
	float UIStep = 0.01;
> = 0.8f;

float4		gTextureRowsColsStartEnd	: TextureRowsColsStartEnd;
float4		gTextureAnimRateScaleOverLifeStart2End2	: TextureAnimRateScaleOverLifeStart2End2;
float4		gSizeMinRange				: SizeMinRange;
float4		gColour						: Colour;
float2		gFadeInOut					: FadeInOut;
float2		gRotSpeedMinRange			: RotSpeedMinRange;
float3		gDirectionalZOffsetMinRange	: DirectionalZOffsetMinRange;
float2		gFadeNearFar				: FadeNearFar;
float3      gFadeZBaseLoHi              : FadeZBaseLoHi;
float3		gDirectionalVelocityAdd		: DirectionalVelocityAdd;
float		gEdgeSoftness				: EdgeSoftness;
float		gMaxLife					: MaxLife;
float3		gRefParticlePos				: RefParticlePos;
float		gParticleColorPercentage	: ParticleColorPercentage;
float		gBackgroundDistortionVisibilityPercentage	: BackgroundDistortionVisibilityPercentage;
float		gBackgroundDistortionAlphaBooster			: BackgroundDistortionAlphaBooster;
float		gBackgroundDistortionAmount					: BackgroundDistortionAmount;
float		gDirectionalLightShadowAmount				: DirectionalLightShadowAmount < int nostrip = 1; > = 1.0f;
float		gLocalLightsMultiplier		: LocalLightsMultiplier;
float4		gCamAngleLimits				: CamAngleLimits = float4(PTXGPU_CAMANGLE_FORWARD_MIN, PTXGPU_CAMANGLE_FORWARD_INV_RANGE, PTXGPU_CAMANGLE_UP_MIN, PTXGPU_CAMANGLE_UP_INV_RANGE);

EndConstantBufferDX10( ptxgpu_render_locals )

#define gCamAngleForwardMin			(gCamAngleLimits.x)
#define gCamAngleForwardInvRange	(gCamAngleLimits.y)
#define gCamAngleUpMin				(gCamAngleLimits.z)
#define gCamAngleUpInvRange			(gCamAngleLimits.w)

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// STRUCTURES
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct ptxgpuDropInfo
{
	float4 pos;
	float4 worldPos;
	float2 texCoord;
	float3 dirUp;
	float3 dirSide;
};

struct ptxgpuVertexOutC
{
	float3 texCoordPrevA	: TEXCOORD0;
	float2 texCoordCurrA	: TEXCOORD1;
	float3 texCoordPrevB	: TEXCOORD2;
	float2 texCoordCurrB	: TEXCOORD3;
	NOINTERPOLATION float4 color			: TEXCOORD4;
	float4 lightColor_DepthW				: TEXCOORD5;
	NOINTERPOLATION float2 texABBlendValues : TEXCOORD6; 
	DECLARE_POSITION_CLIPPLANES( pos )
};

struct ptxgpuPixelInC
{
	float3 texCoordPrevA	: TEXCOORD0;
	float2 texCoordCurrA	: TEXCOORD1;
	float3 texCoordPrevB	: TEXCOORD2;
	float2 texCoordCurrB	: TEXCOORD3;
	NOINTERPOLATION float4 color			: TEXCOORD4;
	float4 lightColor_DepthW				: TEXCOORD5;
	NOINTERPOLATION float2 texABBlendValues : TEXCOORD6;
	DECLARE_POSITION_CLIPPLANES_PSIN( pos )
};

#define posDepthW         		lightColor_DepthW.w
#define lightColor				lightColor_DepthW.xyz

#if APPLY_DOF_TO_GPU_PARTICLES
#define gpuParticleOutput OutHalf4Color_DOF
#else
#define gpuParticleOutput half4
#endif //APPLY_DOF_TO_GPU_PARTICLES

#define DOF_GPU_PARTICLE_ALPHA_LIMIT			(0.15)

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HELPER FUNCTIONS
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


float CalculateShadowAmount(float3 centerPos, float3 vertexPos, float3 dirUp, float3 dirSide, float effectShadowAmount)
{
#if (RSG_PC && __LOW_QUALITY)
	return 1.0f;
#else
	const int NUM_SAMPLES = 10;

	const float2 poisson10[NUM_SAMPLES] =
	{
		float2(0.02027519f, -0.7867655f),
		float2(-0.7015828f, -0.4747616f),
		float2(0.544736f, -0.06281875f),
		float2(-0.1233655f, -0.1290102f),
		float2(0.7202142f, -0.5998743f),
		float2(0.5887699f, 0.8062456f),
		float2(-0.5588413f, 0.3316797f),
		float2(-0.3311834f, 0.8879117f),
		float2(0.05035595f, 0.4865102f),
		float2(0.9244029f, 0.3605934f)
	};


	float3 midPt = (centerPos + vertexPos) * 0.5f;
	float radius = length(centerPos - vertexPos) * 0.5f;
	float shadowAmount = 0.0f;

	//Can't do an unroll here because PS4 compiler can't handle it and ends up
	//making GPU particles flicker a lot
	for(int i=0; i<NUM_SAMPLES; i++)
	{
		float3 samplePos = midPt + radius * ( dirSide * poisson10[i].x + dirUp * poisson10[i].y );
		shadowAmount += CalcCascadeShadowsFastNoFade(gViewInverse[3].xyz, samplePos, float3(0,0,1), float2(0,0));
	}
	shadowAmount *= (1.0f/NUM_SAMPLES);
	return lerp(1.0, shadowAmount, effectShadowAmount);
#endif
	
}
// ptxgpuDropCreate ///////////////////////////////

float3x3 rotationAboutAxis(float3 axis, float rads)
{
    // construct a quaternion 
	float s;
	float w;
	sincos(rads, s, w);
    float x = axis.x*s;
    float y = axis.y*s;
	float z = axis.z*s;
	
    // convert to a matrix
    float x2 = x + x;
    float y2 = y + y;
    float z2 = z + z;
    float xx = x * x2;
    float yy = y * y2;
    float zz = z * z2;
    float xy = x * y2;
    float yz = y * z2;
    float xz = z * x2;
    float wx = w * x2;
    float wy = w * y2;
    float wz = w * z2;
    
    float3x3 rotMat;
    rotMat[0][0] = 1.0f-(yy+zz);
	rotMat[1][0] = xy-wz;          
	rotMat[2][0] = xz+wy;          
    rotMat[0][1] = xy+wz;              
	rotMat[1][1] = 1.0f-(xx+zz);  
	rotMat[2][1] = yz-wx;          
    rotMat[0][2] = xz-wy;              
	rotMat[1][2] = yz+wx;          
	rotMat[2][2] = 1.0f-(xx+yy);  
    
    return rotMat;
}


ptxgpuDropInfo ptxgpuDropCreate(float2 uvs, float4 ptxPosition, float4 ptxVelocity, float drawDirectional, float curLife, float sizeX, float sizeY, float rotSpeed, float zOffset)
{
	ptxgpuDropInfo OUT = (ptxgpuDropInfo)0;

	OUT.texCoord = uvs;
	
	ptxPosition.z += sizeY*zOffset;

	uvs.xy = uvs.xy-0.5f;
	float3 dirSide, dirUp, offset;
	if (drawDirectional>0.0f)
	{
		float3 velocity = float3(ptxVelocity.xyz);
		velocity += gDirectionalVelocityAdd.xyz;
		float3 ptxToCam = gViewInverse[3].xyz-ptxPosition.xyz;
		dirUp = normalize(velocity);
		dirSide = normalize(cross(dirUp, ptxToCam));
		offset = (uvs.x*sizeX)*dirSide + ((0.7f-(uvs.y+0.5f))*sizeY)*-dirUp;
	}	
	else
	{
		// Find a rotation matrix around the cameras forward direction
		float currRot = rotSpeed * curLife;
		float3x3 rotMat = rotationAboutAxis(gViewInverse[2].xyz, currRot);

		//Let's orient the particle with the world Up axis
		float3 vViewAxis = normalize(gViewInverse[3] - ptxPosition);
		dirUp = float3(0,0,1);
		if(dot(vViewAxis, dirUp) < 0.999995f)
		{
			dirSide = cross(dirUp, vViewAxis);
			dirSide = normalize(dirSide);

			dirUp = cross(dirSide, vViewAxis);
			dirUp = normalize(dirUp);
		}
		else
		{
			dirSide = float3(1,0,0);
			dirUp = float3(0,1,0);
		}

		//applying rotations to the dirUp and dirSide
		dirUp = mul(dirUp, rotMat);
		dirSide = mul(dirSide, rotMat);
			
		// Calculate the final offset 
		offset = (uvs.x*sizeX)* dirSide + (uvs.y * sizeY) * dirUp;
	}

	// Transform position to the clipping space 
	float4 pos = float4(ptxPosition.xyz, 1.0f);
	pos.xyz += offset.xyz;

	OUT.pos = (float4)mul(pos, gWorldViewProj);
	OUT.worldPos = (float4)mul(pos, gWorld);

	OUT.dirUp = dirUp;
	OUT.dirSide = dirSide;

	return OUT;
}

float4 ptxgpuGetDiffuseColour(sampler2D texSampler, ptxgpuPixelInC IN, bool bAnimateTexture, bool bApplyABBlend, bool bPerform4ChannelBlend)
{
	float4 texCol = tex2D(texSampler, IN.texCoordCurrA);
	if(bAnimateTexture)
	{
		float4 texColPrevA = tex2D(texSampler, IN.texCoordPrevA);
		texCol = lerp(texColPrevA, texCol, IN.texCoordPrevA.z);
	}

	if(bApplyABBlend)
	{
		float4 texColB = tex2D(texSampler, IN.texCoordCurrB);
		if(bAnimateTexture)
		{
			float4 texColPrevB = tex2D(texSampler, IN.texCoordPrevB);
			texColB = lerp(texColPrevB, texColB, IN.texCoordPrevB.z);
		}
		if(bPerform4ChannelBlend)
		{
			texCol = texCol * IN.texABBlendValues.x + texColB * IN.texABBlendValues.y;

		}
		else
		{
			texCol = float4(texCol.rgb, texCol.a * IN.texABBlendValues.x) + float4(texColB.rgb, texColB.a * IN.texABBlendValues.y);

		}
	}

	return texCol;
}

#if __SHADERMODEL>=40
float3 ptxgpuApplyBackgroundDistortion(ptxgpuPixelInC IN, bool bAnimateTexture, bool bApplyAABBBlend)
{
	float3 outColor;

#if __PS3 || __WIN32PC || RSG_ORBIS
	float2 screenPos = IN.pos.xy * gInvScreenSize.xy;
#else
	float2 screenPos = IN.pos.xy * gInvScreenSize.xy + gInvScreenSize.zw;
#endif // __PS3
	float2 offsetSamplePos = float2(0.0f, 0.0f);
	//Sample the distorion texture and offset screen space pos based on sample * distortion amount
	[branch]
	if(gBackgroundDistortionAmount > 0.0f)
	{
		float depthFactor = 1.0 - (IN.pos.z * IN.pos.z);
		float4 distortionNormal = ptxgpuGetDiffuseColour(DistortionSampler, IN, bAnimateTexture, bApplyAABBBlend, true);
		distortionNormal.xy = distortionNormal.xy * 2.0f - 1.0f;
		distortionNormal.xy = lerp(float2(0.0f,0.0f), distortionNormal.xy, distortionNormal.z);
		offsetSamplePos = (distortionNormal.xy * gInvScreenSize.xy) * (depthFactor * gBackgroundDistortionAmount);
	}

	const float3 backGroundColor = tex2D(BackBufferSampler, screenPos.xy + offsetSamplePos).rgb;
	outColor = (backGroundColor * gBackgroundDistortionVisibilityPercentage);
	return outColor;

}
#endif
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// VERTEX SHADER
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ptxgpuVertexOutC VS_ptxgpuRenderDrop_Common(ptxgpuVertexIn IN, bool bAnimateTexture, bool bApplyABBlend, bool bApplyBackgroundDistortion)
{
	// get the particle state
	float4 pos4;
	float4 vel4;
	float2 texCoords;
	ptxgpuGetParticleState(IN, texCoords, pos4, vel4);
	
	// Transform local position to world position
	pos4.xyz += gRefParticlePos;

	// Position W contains the collision state in the sign and the current particle life value
	float curLife = abs(pos4.w);

	// unpack the current and max life
	float seed;
	float initLife;
	UnPackVelW(vel4.w, gMaxLife, initLife, seed);

	// get a random value (use the texture id for the moment - this could need fiddled with a bit more)
	float randomTexId = seed;
	float randomSize = frac(seed*10.0f);
	float randomRot = frac(seed*100.0f);
	
	float rotSpeed = gRotSpeedMinRange.x + gRotSpeedMinRange.y * randomRot;

	// check if the particle has collided with the ground yet
	ptxgpuDropInfo ptxInfo;

	// particle has not collided - therefore it is visible
	float2 sizeXY = gSizeMinRange.xy + gSizeMinRange.zw * randomSize;
	
	// random height offset for particle
	float zOffset = gDirectionalZOffsetMinRange.y + gDirectionalZOffsetMinRange.z * frac(seed*1000.0f);
	ptxInfo = ptxgpuDropCreate(texCoords, pos4, vel4, gDirectionalZOffsetMinRange.x, curLife, sizeXY.x, sizeXY.y, rotSpeed, zOffset);

	ptxgpuVertexOutC OUT;
	OUT.texCoordCurrA = ptxInfo.texCoord;
	OUT.texCoordPrevA.xy = ptxInfo.texCoord;
	OUT.texCoordPrevA.z = 0.0f;
	OUT.texCoordCurrB = ptxInfo.texCoord;
	OUT.texCoordPrevB.xy = ptxInfo.texCoord;
	OUT.texCoordPrevB.z = 0.0f;
	OUT.pos = ptxInfo.pos;
	OUT.posDepthW = OUT.pos.w;
	OUT.texABBlendValues = float2(0.0f,0.0f);
	
	// calculate the animated texture id (depending on whether we're scaling over life ratio or curr life)
	float curLifeIncreasing = initLife - curLife;
	float animTexId = lerp(curLifeIncreasing*gTextureAnimRateScaleOverLifeStart2End2.x,
					      (curLifeIncreasing/initLife)*gTextureAnimRateScaleOverLifeStart2End2.x,
						   gTextureAnimRateScaleOverLifeStart2End2.y);

	// use the animated texture id if the anim rate is non-zero
	randomTexId = fselect_gt(gTextureAnimRateScaleOverLifeStart2End2.x, 0.0f, frac(animTexId), randomTexId);

	// set the colour of the falling drop
	OUT.color = gColour; // gColor converted to linear space on the CPU

	// turn the random texture id (0.0 -> 0.999) into a texture id into the texture page	
	float texIdA = gTextureRowsColsStartEnd.z + (randomTexId*0.999f*(1+gTextureRowsColsStartEnd.w-gTextureRowsColsStartEnd.z));
	float texIdCurrA = floor(texIdA);

	// calculate the texture page info 
	float4 TextureRowsColsCurrA = float4(fmod(texIdCurrA, gTextureRowsColsStartEnd.y),								// xIndex = id % cols
		floor(texIdCurrA / gTextureRowsColsStartEnd.y),								// yIndex = id / cols
		1.0f/gTextureRowsColsStartEnd.y,											// xStep  = 1 / cols
		1.0f/gTextureRowsColsStartEnd.x);											// yStep  = 1 / rows

	// convert the global uv space into that of the texture page at the required texture id
	OUT.texCoordCurrA.xy = (TextureRowsColsCurrA.xy+OUT.texCoordCurrA.xy) * TextureRowsColsCurrA.zw;

	if(bAnimateTexture)
	{
		// turn the random texture id (0.0 -> 0.999) into a texture id into the texture page
		float texIdPrevA = fselect_lt(texIdCurrA-1, gTextureRowsColsStartEnd.z, texIdCurrA, texIdCurrA-1);

		// calculate the texture page info 
		float4 TextureRowsColsPrevA = float4(fmod(texIdPrevA, gTextureRowsColsStartEnd.y),								// xIndex = id % cols
			floor(texIdPrevA / gTextureRowsColsStartEnd.y),								// yIndex = id / cols
			1.0f/gTextureRowsColsStartEnd.y,											// xStep  = 1 / cols
			1.0f/gTextureRowsColsStartEnd.x);											// yStep  = 1 / rows

		// convert the global uv space into that of the texture page at the required texture id
		OUT.texCoordPrevA.xy = (TextureRowsColsPrevA.xy+OUT.texCoordPrevA.xy) * TextureRowsColsPrevA.zw;
		OUT.texCoordPrevA.z = fselect_gt(gTextureAnimRateScaleOverLifeStart2End2.x, 0.0f, frac(texIdA), 1.0f);
	}

	if(bApplyABBlend)
	{
		// turn the random texture id (0.0 -> 0.999) into a texture id into the texture page
		float texIdB = gTextureAnimRateScaleOverLifeStart2End2.z + (randomTexId*0.999f*(1+gTextureAnimRateScaleOverLifeStart2End2.w-gTextureAnimRateScaleOverLifeStart2End2.z));
		float texIdCurrB = floor(texIdB);
		// calculate the texture page info 
		float4 TextureRowsColsCurrB = float4(fmod(texIdCurrB, gTextureRowsColsStartEnd.y),								// xIndex = id % cols
			floor(texIdCurrB / gTextureRowsColsStartEnd.y),								// yIndex = id / cols
			1.0f/gTextureRowsColsStartEnd.y,											// xStep  = 1 / cols
			1.0f/gTextureRowsColsStartEnd.x);											// yStep  = 1 / rows

		// convert the global uv space into that of the texture page at the required texture id
		OUT.texCoordCurrB.xy = (TextureRowsColsCurrB.xy+OUT.texCoordCurrB.xy) * TextureRowsColsCurrB.zw;

		if(bAnimateTexture)
		{
			// turn the random texture id (0.0 -> 0.999) into a texture id into the texture page
			float texIdPrevB = fselect_lt(texIdCurrB-1, gTextureAnimRateScaleOverLifeStart2End2.z, texIdCurrB, texIdCurrB-1);
			// calculate the texture page info 
			float4 TextureRowsColsPrevB = float4(fmod(texIdPrevB, gTextureRowsColsStartEnd.y),								// xIndex = id % cols
				floor(texIdPrevB / gTextureRowsColsStartEnd.y),								// yIndex = id / cols
				1.0f/gTextureRowsColsStartEnd.y,											// xStep  = 1 / cols
				1.0f/gTextureRowsColsStartEnd.x);											// yStep  = 1 / rows

			// convert the global uv space into that of the texture page at the required texture id
			OUT.texCoordPrevB.xy = (TextureRowsColsPrevB.xy+OUT.texCoordPrevB.xy) * TextureRowsColsPrevB.zw;
			OUT.texCoordPrevB.z = fselect_gt(gTextureAnimRateScaleOverLifeStart2End2.x, 0.0f, frac(texIdB), 1.0f);

		}

		//Lets calculate how much of A and B we require based on the view vector
		float3 eyeDir = normalize(pos4.xyz - gViewInverse[3].xyz);
		float cosCamAngle = abs(eyeDir.z);

		float forwardAngleFade = saturate((1.0f - cosCamAngle - gCamAngleForwardMin)*gCamAngleForwardInvRange);
		float upAngleFade = saturate((cosCamAngle - gCamAngleUpMin)*gCamAngleUpInvRange);
		OUT.texABBlendValues = float2(forwardAngleFade,upAngleFade);

	}	

	// fade all particles as they get far from the camera
	float fadeFar = 1.0f - (length(gViewInverse[3].xy - pos4.xy) * gFadeNearFar.y);
	float fadeFarMult = saturate(fadeFar * fadeFar * fadeFar);
	OUT.color.a *= fadeFarMult;

	// fade all particles as they get near the camera
	float fadeNear = length(gViewInverse[3].xy - pos4.xy) * gFadeNearFar.x;
	float fadeNearMult = saturate(fadeNear * fadeNear * fadeNear);
	OUT.color.a *= fadeNearMult;
	
	// fade all particles relative to the base z
	float baseZLoMult = saturate((pos4.z-gFadeZBaseLoHi.x)*abs(gFadeZBaseLoHi.y));
	OUT.color.a *= baseZLoMult;
	
	float baseZHiMult = saturate((pos4.z-gFadeZBaseLoHi.x)*abs(gFadeZBaseLoHi.z));
	OUT.color.a *= 1.0f-baseZHiMult;

	// fade the particle in and out over it's life
	float dropFadeIn = saturate((initLife - curLife) * gFadeInOut.x);
	float dropFadeOut = saturate(curLife * gFadeInOut.y);
	OUT.color.a *= dropFadeIn * dropFadeOut;

	// Take the exponent bias into account
	OUT.color.a *= gInvColorExpBias;

	LocalLightResult lightingRes = CalculateLightingForward(
			4, 
			true,							//diffuse
			float3(1.0, 1.0, 1.0),			// material.diffuseColor.rgb,
			pos4.xyz,						//surface.position 
			normalize(gViewInverse[3].xyz - pos4.xyz) // surface.normal
			#if GLASS_LIGHTING
				, OUT.color.w
			#endif // GLASS_LIGHTING
			);

	float3 dirIntensity =  gDirectionalColour.rgb;
	float3 ambIntensity = naturalAmbient(1.0f);
	[branch]
	if(gDirectionalLightShadowAmount>0.0f)
	{
		dirIntensity *= CalculateShadowAmount(pos4.xyz, ptxInfo.worldPos.xyz, ptxInfo.dirUp.xyz, ptxInfo.dirSide.xyz, gDirectionalLightShadowAmount);
	}
	OUT.lightColor.rgb = (dirIntensity + ambIntensity) * gLightIntensityMult  + (lightingRes.diffuseLightContribution * gLocalLightsMultiplier);

	//This is to boost alpha if using the background to contribute to the color (otherwise it just isn't noticeable on the rain)
	if(bApplyBackgroundDistortion)
	{
		OUT.color.a *= gBackgroundDistortionAlphaBooster;
	}
	
	// Zero out particles that have colided or that are fully transparent in order to cull them
#if __PS3
	// PS3 culls dead drop particles on the SPU (ground particles don't die)
	float isDead = OUT.color.a <= 0.01;
#else
	// dead if the alpha is too low or the particle is flagged as dead
	float isDead = OUT.color.a <= 0.001f || pos4.w<0.0f;	
#endif // !__PS3

#if __SHADERMODEL >= 40
	// isDead is set to one when we want to clip and zero otherwise
	OUT.pos_ClipDistance0 = 1.0f - (2.0f * isDead);
#else
	OUT.pos.xyz *= 1.0 - isDead;
	
#endif //__SHADERMODEL >= 40
	
	return OUT;
}


ptxgpuVertexOutC VS_ptxgpuRenderDrop_NoAnimate_NoABBlend_NoBackgroundDistortion(ptxgpuVertexIn IN)
{
	return VS_ptxgpuRenderDrop_Common(IN, false, false, false);
}

ptxgpuVertexOutC VS_ptxgpuRenderDrop_Animate_NoABBlend_NoBackgroundDistortion(ptxgpuVertexIn IN)
{
	return VS_ptxgpuRenderDrop_Common(IN, true, false, false);
}

ptxgpuVertexOutC VS_ptxgpuRenderDrop_NoAnimate_ABBlend_NoBackgroundDistortion(ptxgpuVertexIn IN)
{
	return VS_ptxgpuRenderDrop_Common(IN, false, true, false);
}

ptxgpuVertexOutC VS_ptxgpuRenderDrop_Animate_ABBlend_NoBackgroundDistortion(ptxgpuVertexIn IN)
{
	return VS_ptxgpuRenderDrop_Common(IN, true, true, false);
}

ptxgpuVertexOutC VS_ptxgpuRenderDrop_NoAnimate_NoABBlend_BackgroundDistortion(ptxgpuVertexIn IN)
{
	return VS_ptxgpuRenderDrop_Common(IN, false, false, true);
}

ptxgpuVertexOutC VS_ptxgpuRenderDrop_Animate_NoABBlend_BackgroundDistortion(ptxgpuVertexIn IN)
{
	return VS_ptxgpuRenderDrop_Common(IN, true, false, true);
}

ptxgpuVertexOutC VS_ptxgpuRenderDrop_NoAnimate_ABBlend_BackgroundDistortion(ptxgpuVertexIn IN)
{
	return VS_ptxgpuRenderDrop_Common(IN, false, true, true);
}

ptxgpuVertexOutC VS_ptxgpuRenderDrop_Animate_ABBlend_BackgroundDistortion(ptxgpuVertexIn IN)
{
	return VS_ptxgpuRenderDrop_Common(IN, true, true, true);
}


#if __SHADERMODEL >= 40

ptxgpuVertexOutC VS_ptxgpuRenderDrop_NoAnimate_NoABBlend_NoBackgroundDistortion_LookUp(ptxgpuLookUpVertexIn IN, uint InstanceID : SV_InstanceID)
{
	ptxgpuVertexIn Vertex = Create_ptxgpuVertexIn(IN, InstanceID);
	return VS_ptxgpuRenderDrop_NoAnimate_NoABBlend_NoBackgroundDistortion(Vertex);
}

ptxgpuVertexOutC VS_ptxgpuRenderDrop_Animate_NoABBlend_NoBackgroundDistortion_LookUp(ptxgpuLookUpVertexIn IN, uint InstanceID : SV_InstanceID)
{
	ptxgpuVertexIn Vertex = Create_ptxgpuVertexIn(IN, InstanceID);
	return VS_ptxgpuRenderDrop_Animate_NoABBlend_NoBackgroundDistortion(Vertex);
}

ptxgpuVertexOutC VS_ptxgpuRenderDrop_NoAnimate_ABBlend_NoBackgroundDistortion_LookUp(ptxgpuLookUpVertexIn IN, uint InstanceID : SV_InstanceID)
{
	ptxgpuVertexIn Vertex = Create_ptxgpuVertexIn(IN, InstanceID);
	return VS_ptxgpuRenderDrop_NoAnimate_ABBlend_NoBackgroundDistortion(Vertex);
}

ptxgpuVertexOutC VS_ptxgpuRenderDrop_Animate_ABBlend_NoBackgroundDistortion_LookUp(ptxgpuLookUpVertexIn IN, uint InstanceID : SV_InstanceID)
{
	ptxgpuVertexIn Vertex = Create_ptxgpuVertexIn(IN, InstanceID);
	return VS_ptxgpuRenderDrop_Animate_ABBlend_NoBackgroundDistortion(Vertex);
}

ptxgpuVertexOutC VS_ptxgpuRenderDrop_NoAnimate_NoABBlend_BackgroundDistortion_LookUp(ptxgpuLookUpVertexIn IN, uint InstanceID : SV_InstanceID)
{
	ptxgpuVertexIn Vertex = Create_ptxgpuVertexIn(IN, InstanceID);
	return VS_ptxgpuRenderDrop_NoAnimate_NoABBlend_BackgroundDistortion(Vertex);
}

ptxgpuVertexOutC VS_ptxgpuRenderDrop_Animate_NoABBlend_BackgroundDistortion_LookUp(ptxgpuLookUpVertexIn IN, uint InstanceID : SV_InstanceID)
{
	ptxgpuVertexIn Vertex = Create_ptxgpuVertexIn(IN, InstanceID);
	return VS_ptxgpuRenderDrop_Animate_NoABBlend_BackgroundDistortion(Vertex);
}

ptxgpuVertexOutC VS_ptxgpuRenderDrop_NoAnimate_ABBlend_BackgroundDistortion_LookUp(ptxgpuLookUpVertexIn IN, uint InstanceID : SV_InstanceID)
{
	ptxgpuVertexIn Vertex = Create_ptxgpuVertexIn(IN, InstanceID);
	return VS_ptxgpuRenderDrop_NoAnimate_ABBlend_BackgroundDistortion(Vertex);
}

ptxgpuVertexOutC VS_ptxgpuRenderDrop_Animate_ABBlend_BackgroundDistortion_LookUp(ptxgpuLookUpVertexIn IN, uint InstanceID : SV_InstanceID)
{
	ptxgpuVertexIn Vertex = Create_ptxgpuVertexIn(IN, InstanceID);
	return VS_ptxgpuRenderDrop_Animate_ABBlend_BackgroundDistortion(Vertex);
}

#endif //__WIN32PC && __SHADERMODEL >= 40

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// PIXEL SHADER
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

gpuParticleOutput PS_ptxgpuRender_Common(ptxgpuPixelInC IN, bool bAnimateTexture, bool bApplyABBlend, bool bApplyBackgroundDistortion)
{
	float4 color = IN.color;
	color.rgb *= IN.lightColor;

	// We only care about the alpha value
#if __SHADERMODEL >= 40

	float4 rainColor = color.rgba;

	float4 particleTexture = ptxgpuGetDiffuseColour(DiffuseSampler, IN, bAnimateTexture, bApplyABBlend, false);

	color.a = saturate(rainColor.a * particleTexture.a);
	color.rgb = (rainColor.rgb * gParticleColorPercentage);

	// If we need background distortion...
	if(bApplyBackgroundDistortion)
	{
		color.rgb += ptxgpuApplyBackgroundDistortion(IN, bAnimateTexture, bApplyABBlend);		
	}

#else // __SHADERMODEL >= 40

	float colAlphaMult = ptxgpuGetDiffuseColour(DiffuseSampler, IN, bAnimateTexture, bApplyABBlend, false).a;
	color.a *= colAlphaMult;

#endif // __SHADERMODEL >= 40

	half4 packedCol = PackHdr(color);

	gpuParticleOutput OUT;
#if APPLY_DOF_TO_GPU_PARTICLES	
	// calculate the final pixel colour
	OUT.col0 = packedCol;
	float dofOutput = color.a > DOF_GPU_PARTICLE_ALPHA_LIMIT ? 1.0 : 0.0;
	float dofDepth = IN.posDepthW;
#if SUPPORT_INVERTED_PROJECTION
	dofDepth = 1.0 - dofDepth;
#endif
	OUT.dof.depth = dofDepth * dofOutput;	//z/w
	OUT.dof.alpha = dofOutput;
#else
	OUT = packedCol;
#endif
	
	return OUT;
}

gpuParticleOutput PS_ptxgpuRender_NoAnimate_NoABBlend_NoBackgroundDistortion(ptxgpuPixelInC IN) : COLOR
{
	return PS_ptxgpuRender_Common(IN, false, false, false);
}

gpuParticleOutput PS_ptxgpuRender_Animate_NoABBlend_NoBackgroundDistortion(ptxgpuPixelInC IN) : COLOR
{
	return PS_ptxgpuRender_Common(IN, true, false, false);
}

gpuParticleOutput PS_ptxgpuRender_NoAnimate_ABBlend_NoBackgroundDistortion(ptxgpuPixelInC IN) : COLOR
{
	return PS_ptxgpuRender_Common(IN, false, true, false);
}

gpuParticleOutput PS_ptxgpuRender_Animate_ABBlend_NoBackgroundDistortion(ptxgpuPixelInC IN) : COLOR
{
	return PS_ptxgpuRender_Common(IN, true, true, false);
}

gpuParticleOutput PS_ptxgpuRender_NoAnimate_NoABBlend_BackgroundDistortion(ptxgpuPixelInC IN) : COLOR
{
	return PS_ptxgpuRender_Common(IN, false, false, true);
}

gpuParticleOutput PS_ptxgpuRender_Animate_NoABBlend_BackgroundDistortion(ptxgpuPixelInC IN) : COLOR
{
	return PS_ptxgpuRender_Common(IN, true, false, true);
}

gpuParticleOutput PS_ptxgpuRender_NoAnimate_ABBlend_BackgroundDistortion(ptxgpuPixelInC IN) : COLOR
{
	return PS_ptxgpuRender_Common(IN, false, true, true);
}

gpuParticleOutput PS_ptxgpuRender_Animate_ABBlend_BackgroundDistortion(ptxgpuPixelInC IN) : COLOR
{
	return PS_ptxgpuRender_Common(IN, true, true, true);
}

half4 PS_ptxgpuRenderSoftCommon(ptxgpuPixelInC IN, bool bAnimateTexture, bool bApplyABBlend, bool bApplyBackgroundDistortion)
{
	float4 color = IN.color;
	color.rgb *= IN.lightColor;

	// We only care about the alpha value
	float colAlphaMult = ptxgpuGetDiffuseColour(DiffuseSampler, IN, bAnimateTexture, bApplyABBlend, false).a;
	color.a *= colAlphaMult;

#if __SHADERMODEL>=40
	// If we need background distortion...
	if(bApplyBackgroundDistortion)
	{
		color.rgb += ptxgpuApplyBackgroundDistortion(IN, bAnimateTexture, bApplyABBlend);
	}
#endif
	// Get the pixels GBuffer depth
	
#if __LOW_QUALITY
	float sampledDepth = 1.0f;
#else
	float sampledDepth;
	ptfxGetDepthValue(IN.pos.xy, sampledDepth);
#endif // __LOW_QUALITY

	// Do the soft edge calculation
	float depthDiff = (sampledDepth - IN.posDepthW);
	depthDiff*=depthDiff;
	float softFade = saturate( depthDiff * gEdgeSoftness );
	color.a *= softFade;

	// calculate the final pixel colour
	return PackHdr(color);
}


half4 PS_ptxgpuRenderSoft_NoAnimate_NoABBlend_NoBackgroundDistortion(ptxgpuPixelInC IN) : COLOR
{
	return PS_ptxgpuRenderSoftCommon(IN, false, false, false);
}
half4 PS_ptxgpuRenderSoft_Animate_NoABBlend_NoBackgroundDistortion(ptxgpuPixelInC IN) : COLOR
{
	return PS_ptxgpuRenderSoftCommon(IN, true, false, false);
}
half4 PS_ptxgpuRenderSoft_NoAnimate_ABBlend_NoBackgroundDistortion(ptxgpuPixelInC IN) : COLOR
{
	return PS_ptxgpuRenderSoftCommon(IN, false, true, false);
}
half4 PS_ptxgpuRenderSoft_Animate_ABBlend_NoBackgroundDistortion(ptxgpuPixelInC IN) : COLOR
{
	return PS_ptxgpuRenderSoftCommon(IN, true, true, false);
}

half4 PS_ptxgpuRenderSoft_NoAnimate_NoABBlend_BackgroundDistortion(ptxgpuPixelInC IN) : COLOR
{
	return PS_ptxgpuRenderSoftCommon(IN, false, false, true);
}
half4 PS_ptxgpuRenderSoft_Animate_NoABBlend_BackgroundDistortion(ptxgpuPixelInC IN) : COLOR
{
	return PS_ptxgpuRenderSoftCommon(IN, true, false, true);
}
half4 PS_ptxgpuRenderSoft_NoAnimate_ABBlend_BackgroundDistortion(ptxgpuPixelInC IN) : COLOR
{
	return PS_ptxgpuRenderSoftCommon(IN, false, true, true);
}
half4 PS_ptxgpuRenderSoft_Animate_ABBlend_BackgroundDistortion(ptxgpuPixelInC IN) : COLOR
{
	return PS_ptxgpuRenderSoftCommon(IN, true, true, true);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// TECHNIQUES
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

technique draw
{
	pass p_NoAnim_NoABBlend_NoBackgroundDistortion 
	{        
		CullMode = None;
	#if __SHADERMODEL >= 40
		VertexShader = compile VERTEXSHADER VS_ptxgpuRenderDrop_NoAnimate_NoABBlend_NoBackgroundDistortion_LookUp(); 
	#else
		VertexShader = compile VERTEXSHADER VS_ptxgpuRenderDrop_NoAnimate_NoABBlend_NoBackgroundDistortion(); 
	#endif
		PixelShader  = compile PIXELSHADER PS_ptxgpuRender_NoAnimate_NoABBlend_NoBackgroundDistortion() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p_Anim_NoABBlend_NoBackgroundDistortion
	{        
		CullMode = None;
#if __SHADERMODEL >= 40
		VertexShader = compile VERTEXSHADER VS_ptxgpuRenderDrop_Animate_NoABBlend_NoBackgroundDistortion_LookUp(); 
#else
		VertexShader = compile VERTEXSHADER VS_ptxgpuRenderDrop_Animate_NoABBlend_NoBackgroundDistortion(); 
#endif
		PixelShader  = compile PIXELSHADER PS_ptxgpuRender_Animate_NoABBlend_NoBackgroundDistortion() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p_NoAnim_ABBlend_NoBackgroundDistortion
	{        
		CullMode = None;
#if __SHADERMODEL >= 40
		VertexShader = compile VERTEXSHADER VS_ptxgpuRenderDrop_NoAnimate_ABBlend_NoBackgroundDistortion_LookUp(); 
#else
		VertexShader = compile VERTEXSHADER VS_ptxgpuRenderDrop_NoAnimate_ABBlend_NoBackgroundDistortion(); 
#endif
		PixelShader  = compile PIXELSHADER PS_ptxgpuRender_NoAnimate_ABBlend_NoBackgroundDistortion() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p_Anim_ABBlend_NoBackgroundDistortion
	{        
		CullMode = None;
#if __SHADERMODEL >= 40
		VertexShader = compile VERTEXSHADER VS_ptxgpuRenderDrop_Animate_ABBlend_NoBackgroundDistortion_LookUp(); 
#else
		VertexShader = compile VERTEXSHADER VS_ptxgpuRenderDrop_Animate_ABBlend_NoBackgroundDistortion(); 
#endif
		PixelShader  = compile PIXELSHADER PS_ptxgpuRender_Animate_ABBlend_NoBackgroundDistortion() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}




	pass p_NoAnim_NoABBlend_BackgroundDistortion
	{        
		CullMode = None;
#if __SHADERMODEL >= 40
		VertexShader = compile VERTEXSHADER VS_ptxgpuRenderDrop_NoAnimate_NoABBlend_BackgroundDistortion_LookUp(); 
#else
		VertexShader = compile VERTEXSHADER VS_ptxgpuRenderDrop_NoAnimate_NoABBlend_BackgroundDistortion(); 
#endif
		PixelShader  = compile PIXELSHADER PS_ptxgpuRender_NoAnimate_NoABBlend_BackgroundDistortion() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p_Anim_NoABBlend_BackgroundDistortion
	{        
		CullMode = None;
#if __SHADERMODEL >= 40
		VertexShader = compile VERTEXSHADER VS_ptxgpuRenderDrop_Animate_NoABBlend_BackgroundDistortion_LookUp(); 
#else
		VertexShader = compile VERTEXSHADER VS_ptxgpuRenderDrop_Animate_NoABBlend_BackgroundDistortion(); 
#endif
		PixelShader  = compile PIXELSHADER PS_ptxgpuRender_Animate_NoABBlend_BackgroundDistortion() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p_NoAnim_ABBlend_BackgroundDistortion
	{        
		CullMode = None;
#if __SHADERMODEL >= 40
		VertexShader = compile VERTEXSHADER VS_ptxgpuRenderDrop_NoAnimate_ABBlend_BackgroundDistortion_LookUp(); 
#else
		VertexShader = compile VERTEXSHADER VS_ptxgpuRenderDrop_NoAnimate_ABBlend_BackgroundDistortion(); 
#endif
		PixelShader  = compile PIXELSHADER PS_ptxgpuRender_NoAnimate_ABBlend_BackgroundDistortion() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p_Anim_ABBlend_BackgroundDistortion
	{        
		CullMode = None;
#if __SHADERMODEL >= 40
		VertexShader = compile VERTEXSHADER VS_ptxgpuRenderDrop_Animate_ABBlend_BackgroundDistortion_LookUp(); 
#else
		VertexShader = compile VERTEXSHADER VS_ptxgpuRenderDrop_Animate_ABBlend_BackgroundDistortion(); 
#endif
		PixelShader  = compile PIXELSHADER PS_ptxgpuRender_Animate_ABBlend_BackgroundDistortion() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}

technique draw_soft
{
	pass p_NoAnim_NoBlend_NoBackgroundDistortion
	{        
		CullMode = None;
	#if __SHADERMODEL >= 40
		VertexShader = compile VERTEXSHADER VS_ptxgpuRenderDrop_NoAnimate_NoABBlend_NoBackgroundDistortion_LookUp(); 
	#else
		VertexShader = compile VERTEXSHADER VS_ptxgpuRenderDrop_NoAnimate_NoABBlend_NoBackgroundDistortion(); 
	#endif
		PixelShader  = compile PIXELSHADER PS_ptxgpuRenderSoft_NoAnimate_NoABBlend_NoBackgroundDistortion() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
	pass p_Anim_NoBlend_NoBackgroundDistortion
	{        
		CullMode = None;
#if __SHADERMODEL >= 40
		VertexShader = compile VERTEXSHADER VS_ptxgpuRenderDrop_Animate_NoABBlend_NoBackgroundDistortion_LookUp(); 
#else
		VertexShader = compile VERTEXSHADER VS_ptxgpuRenderDrop_Animate_NoABBlend_NoBackgroundDistortion(); 
#endif
		PixelShader  = compile PIXELSHADER PS_ptxgpuRenderSoft_Animate_NoABBlend_NoBackgroundDistortion() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
	pass p_NoAnim_Blend_NoBackgroundDistortion
	{        
		CullMode = None;
#if __SHADERMODEL >= 40
		VertexShader = compile VERTEXSHADER VS_ptxgpuRenderDrop_NoAnimate_ABBlend_NoBackgroundDistortion_LookUp(); 
#else
		VertexShader = compile VERTEXSHADER VS_ptxgpuRenderDrop_NoAnimate_ABBlend_NoBackgroundDistortion(); 
#endif
		PixelShader  = compile PIXELSHADER PS_ptxgpuRenderSoft_NoAnimate_ABBlend_NoBackgroundDistortion() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
	pass p_Anim_Blend_NoBackgroundDistortion
	{        
		CullMode = None;
#if __SHADERMODEL >= 40
		VertexShader = compile VERTEXSHADER VS_ptxgpuRenderDrop_Animate_ABBlend_NoBackgroundDistortion_LookUp(); 
#else
		VertexShader = compile VERTEXSHADER VS_ptxgpuRenderDrop_Animate_ABBlend_NoBackgroundDistortion(); 
#endif
		PixelShader  = compile PIXELSHADER PS_ptxgpuRenderSoft_Animate_ABBlend_NoBackgroundDistortion() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
	pass p_NoAnim_NoBlend_BackgroundDistortion
	{        
		CullMode = None;
#if __SHADERMODEL >= 40
		VertexShader = compile VERTEXSHADER VS_ptxgpuRenderDrop_NoAnimate_NoABBlend_BackgroundDistortion_LookUp(); 
#else
		VertexShader = compile VERTEXSHADER VS_ptxgpuRenderDrop_NoAnimate_NoABBlend_BackgroundDistortion(); 
#endif
		PixelShader  = compile PIXELSHADER PS_ptxgpuRenderSoft_NoAnimate_NoABBlend_BackgroundDistortion() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
	pass p_Anim_NoBlend_BackgroundDistortion
	{        
		CullMode = None;
#if __SHADERMODEL >= 40
		VertexShader = compile VERTEXSHADER VS_ptxgpuRenderDrop_Animate_NoABBlend_NoBackgroundDistortion_LookUp(); 
#else
		VertexShader = compile VERTEXSHADER VS_ptxgpuRenderDrop_Animate_NoABBlend_NoBackgroundDistortion(); 
#endif
		PixelShader  = compile PIXELSHADER PS_ptxgpuRenderSoft_Animate_NoABBlend_BackgroundDistortion() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
	pass p_NoAnim_Blend_BackgroundDistortion
	{        
		CullMode = None;
#if __SHADERMODEL >= 40
		VertexShader = compile VERTEXSHADER VS_ptxgpuRenderDrop_NoAnimate_ABBlend_NoBackgroundDistortion_LookUp(); 
#else
		VertexShader = compile VERTEXSHADER VS_ptxgpuRenderDrop_NoAnimate_ABBlend_NoBackgroundDistortion(); 
#endif
		PixelShader  = compile PIXELSHADER PS_ptxgpuRenderSoft_NoAnimate_ABBlend_BackgroundDistortion() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
	pass p_Anim_Blend_BackgroundDistortion
	{        
		CullMode = None;
#if __SHADERMODEL >= 40
		VertexShader = compile VERTEXSHADER VS_ptxgpuRenderDrop_Animate_ABBlend_NoBackgroundDistortion_LookUp(); 
#else
		VertexShader = compile VERTEXSHADER VS_ptxgpuRenderDrop_Animate_ABBlend_NoBackgroundDistortion(); 
#endif
		PixelShader  = compile PIXELSHADER PS_ptxgpuRenderSoft_Animate_ABBlend_BackgroundDistortion() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
