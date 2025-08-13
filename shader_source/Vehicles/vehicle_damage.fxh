//
// vehicle shader damage common header:
//
//	31/10/2005 - Andrzej:	- initial;
//	10/09/2007 - Andrzej:	- VEHICLE_TYRE_DEFORM added;
//
//
//
//
#ifndef __GTA_VEHICLE_DAMAGE_FXH__
#define __GTA_VEHICLE_DAMAGE_FXH__

#include "vehicle_common_values.h"
#define RENDER_DEBUG_DAMAGECOLOR			(0 && !defined(SHADER_FINAL))

#if (__SHADERMODEL < 30)
	// texture samples in vertex shaders only allowed for shader >= 3.0
	#undef USE_VEHICLE_DAMAGE
#endif
#if USE_EDGE
	#if defined(USE_VEHICLE_DAMAGE)
		// PS3: all vehicle damage & deformation is handled by EDGE
		#undef  USE_VEHICLE_DAMAGE
		#define USE_VEHICLE_DAMAGE_EDGE
	#endif
#endif

CBSHARED BeginConstantBufferPagedDX10(vehicle_globals, b7)
#if RSG_ORBIS
	#define BOOLEAN int
#else
	#define BOOLEAN bool
#endif
	
shared BOOLEAN	switchOn : switchOn REGISTER(b8)	= false;
shared BOOLEAN	tyreDeformSwitchOn : tyreDeformSwitchOn REGISTER(b9)	= true;
EndConstantBufferDX10(vehicle_globals)

BeginConstantBufferPagedDX10(vehicle_tyredeformation, b8)
//
// tyreDeformParams (wheel local coords):
// x = tyreDeformScaleX:		"left-right" side deform scale <0;1> (bigger = tyre more flat)
// y = tyreDeformScaleY:		"up-down" deform scale towards alloy <0; 0.20f> (bigger = tyre more sticks to alloy)
// z = tyreDeformScaleZ:		"up-down" deform scale towards alloy <0; 0.20f> (bigger = tyre more sticks to alloy)
// w = tyreDeformContactPointX:	balances flat tyre point: 0=none, >0 - goes outside, <0 - goes inside
//
float4 tyreDeformParams : tyreDeformParams 
#if defined(USE_VEHICLE_INSTANCED_WHEEL_SHADER)
	REGISTER2(vs, c72)
#endif
	= float4(0.0f, 0.0f, 0.0f, 1.0f);

//
// tyreDeformParams2:
// x = tyreInnerRadius (local coords)
// y = groundPosZ (worldSpace)
// z = tyreDeformScale
// w = drawTyre:				1.0=yes, 0.0=no
//
float4 tyreDeformParams2 : tyreDeformParams2
#if defined(USE_VEHICLE_INSTANCED_WHEEL_SHADER)
	REGISTER2(vs, c73)
#endif
	= float4(0.262f, 1495.05f, 0.0f, 0.0f);
	
EndConstantBufferDX10(vehicle_tyredeformation)

#if defined(USE_VEHICLE_DAMAGE)

BeginSampler(sampler, damagetexture, DamageSampler, damagetexture)
ContinueSampler(sampler, damagetexture, DamageSampler, damagetexture)
	AddressU  = CLAMP;        
	AddressV  = CLAMP;
	AddressW  = CLAMP;
	MIPFILTER = POINT;
	MINFILTER = POINT;
	MAGFILTER = POINT;
EndSampler;

BEGIN_RAGE_CONSTANT_BUFFER(vehicle_damage_locals,b9)
float	BoundRadius;
float	DamageMultiplier;
float3	DamageTextureOffset;
float4  DamagedWheelOffsets[2];
#if __SHADERMODEL >= 40
	bool    bDebugDisplayDamageMap;
	bool    bDebugDisplayDamageScale;
#else
	#define bDebugDisplayDamageMap		(false)
	#define bDebugDisplayDamageScale	(false)
#endif
EndConstantBufferDX10( vehicle_damage_locals )

#if RENDER_DEBUG_DAMAGECOLOR
	float4 DebugGetDamageColor(float length, float4 color)
	{
		if( length < 0.01f )
			return float4(lerp(color.rgb,float3(0.0,1.0,0.0),(length/0.01f)),color.a);
		else 
			return float4(lerp(float3(0.0,1.0,0.0), float3(1.0,0.0,0.0), (length-0.01f)/(0.5f -0.01f)),color.a);
	}
#endif // RENDER_DEBUG_DAMAGECOLOR


// get 2d tex coordinates from 3d vector
float2 GetTexCoordFromRadialPos(float3 R)
{
	float zOffset = 0.5f * (1.0f - R.z);

	// store 2d component of offset
	float2 texSampleCoords = R.xy;
	// and re-normalise just this (will get scaled by previously normalised z)
#if !(__WIN32PC || RSG_ORBIS)
	// Note:- normalising a zero vector on PS3 or XENON doesn`t seem to give rise to QNANs.
	texSampleCoords = normalize(texSampleCoords);
#else
	float LSqr = max(dot(texSampleCoords, texSampleCoords), 0.0001f*0.0001f);
	texSampleCoords /= sqrt(LSqr);
#endif

	texSampleCoords *= zOffset;
	texSampleCoords *= 0.5f;
	texSampleCoords += 0.5f;
	
	return texSampleCoords;
}

// get 3d vector from 2d tex coordinates
float3 GetRadialPosFromTexCoord(float2 texSampleCoords)
{
	texSampleCoords -= 0.5f;
	texSampleCoords *= 2.0f;
	
	float fLength = 0.0f;
	if(dot(texSampleCoords, texSampleCoords) > 0.0f)
		fLength = length(texSampleCoords);

	// don't cap fLength because it's required to properly normalise texSampleCoords later	
	//	if(fLength > 1.0f)
	//		fLength = 1.0f;
		
	float3 R = float3(0.0f, 0.0f, 0.0f);
	R.z = 1.0f - 2.0f*fLength;
	
	if(R.z > 1.0f)
		R.z = 1.0f;
	else if(R.z < -1.0f)
		R.z = -1.0f;
	
	float fXYMult = 0.0f;
	if(R.z < 1.0f && R.z > -1.0f && fLength > 0.0f)
	{
		fXYMult = sqrt(1.0f - R.z*R.z) / fLength;
	}
	
	R.x = texSampleCoords.x * fXYMult;
	R.y = texSampleCoords.y * fXYMult;
	
	return R;
}


float4 DamageSampleValue(float3 _inPos)
{
		float3 R = _inPos;		
		R += DamageTextureOffset;

		// normalise the offset vector (gonna use the z component)
		float magR = length(R);
		R /= magR;

		// get 2d tex coordinates from vector
		float2 texSampleCoords = GetTexCoordFromRadialPos(R);

		float s = 1.01f/(float)GTA_VEHICLE_DAMAGE_TEXTURE_SIZE;
		float2 texSampleOffset = fmod(texSampleCoords, s.xx);

		float4 sampleC  = tex2Dlod(DamageSampler, float4(texSampleCoords					+ float2(0, 0),0,0));
		float4 sampleR  = tex2Dlod(DamageSampler, float4(texSampleCoords - texSampleOffset	+ float2(s, 0),0,0));
		float4 sampleU  = tex2Dlod(DamageSampler, float4(texSampleCoords - texSampleOffset	+ float2(0, s),0,0));
		float4 sampleRU = tex2Dlod(DamageSampler, float4(texSampleCoords - texSampleOffset	+ float2(s, s),0,0));

#if !GPU_DAMAGE_WRITE_ENABLED
		/////////////////
		// unpack original sample
		// unpack z component
		modf(sampleC.x / exp2(16), sampleC.z);
		sampleC.x -= sampleC.z*exp2(16);
		// unpack y component
		modf(sampleC.x / exp2(8), sampleC.y);
		sampleC.x -= sampleC.y*exp2(8);

		/////////////////
		// unpack right offset sample
		// unpack z component
		modf(sampleR.x / exp2(16), sampleR.z);
		sampleR.x -= sampleR.z*exp2(16);
		// unpack y component
		modf(sampleR.x / exp2(8), sampleR.y);
		sampleR.x -= sampleR.y*exp2(8);

		/////////////////
		// unpack upward offset sample
		// unpack z component
		modf(sampleU.x / exp2(16), sampleU.z);
		sampleU.x -= sampleU.z*exp2(16);
		// unpack y component
		modf(sampleU.x / exp2(8), sampleU.y);
		sampleU.x -= sampleU.y*exp2(8);

		/////////////////
		// unpack right and up offset sample
		// unpack z component
		modf(sampleRU.x / exp2(16), sampleRU.z);
		sampleRU.x -= sampleRU.z*exp2(16);
		// unpack y component
		modf(sampleRU.x / exp2(8), sampleRU.y);
		sampleRU.x -= sampleRU.y*exp2(8);
#endif

		float2 normalisedSampleOffset = texSampleOffset / s;
		// do a bilinear interpolation of the 4 samples
		float4 smoothedSample = sampleC*(1.0f - normalisedSampleOffset.x)*(1.0f - normalisedSampleOffset.y);
		smoothedSample += sampleR*(normalisedSampleOffset.x)*(1.0f - normalisedSampleOffset.y);
		smoothedSample += sampleU*(1.0f - normalisedSampleOffset.x)*(normalisedSampleOffset.y);
		smoothedSample += sampleRU*(normalisedSampleOffset.x)*(normalisedSampleOffset.y);

#if !GPU_DAMAGE_WRITE_ENABLED
		// 3 components will be 0->255, scale to -1.0f->1.0f
		smoothedSample.x = (smoothedSample.x - GTA_VEHICLE_MAX_DAMAGE_RESOLUTION) / GTA_VEHICLE_MAX_DAMAGE_RESOLUTION;
		smoothedSample.y = (smoothedSample.y - GTA_VEHICLE_MAX_DAMAGE_RESOLUTION) / GTA_VEHICLE_MAX_DAMAGE_RESOLUTION;
		smoothedSample.z = (smoothedSample.z - GTA_VEHICLE_MAX_DAMAGE_RESOLUTION) / GTA_VEHICLE_MAX_DAMAGE_RESOLUTION;
#endif

		smoothedSample *= min(1.0f, magR / BoundRadius);

		return smoothedSample * 0.5 + 0.5;
}


float _ApplyVehicleDamageInt(float3 _inPos, float3 _inNormal, float gDmgScale,
							out float3 outPos, out float3 outNormal, bool bProcessNormal)
{
#if __PS3
	#define exp2(x)		exp2(float(x)) // force to use exp2(float) instead of exp2(half)
#endif

	if(switchOn)
	{
#if defined(USE_VEHICLE_DAMAGE_NO_SCALE)
		gDmgScale = 1;
#endif
		float3 R = _inPos;		
		R += DamageTextureOffset;

		// normalise the offset vector (gonna use the z component)
		float magR = length(R);
		R /= magR;

		// get 2d tex coordinates from vector
		float2 texSampleCoords = GetTexCoordFromRadialPos(R);

		float s = 1.01f/(float)GTA_VEHICLE_DAMAGE_TEXTURE_SIZE;
		float2 texSampleOffset = fmod(texSampleCoords, s.xx);

		float4 sampleC  = tex2Dlod(DamageSampler, float4(texSampleCoords					+ float2(0, 0),0,0));
		float4 sampleR  = tex2Dlod(DamageSampler, float4(texSampleCoords - texSampleOffset	+ float2(s, 0),0,0));
		float4 sampleU  = tex2Dlod(DamageSampler, float4(texSampleCoords - texSampleOffset	+ float2(0, s),0,0));
		float4 sampleRU = tex2Dlod(DamageSampler, float4(texSampleCoords - texSampleOffset	+ float2(s, s),0,0));

#if !GPU_DAMAGE_WRITE_ENABLED
		/////////////////
		// unpack original sample
		// unpack z component
		modf(sampleC.x / exp2(16), sampleC.z);
		sampleC.x -= sampleC.z*exp2(16);
		// unpack y component
		modf(sampleC.x / exp2(8), sampleC.y);
		sampleC.x -= sampleC.y*exp2(8);

		/////////////////
		// unpack right offset sample
		// unpack z component
		modf(sampleR.x / exp2(16), sampleR.z);
		sampleR.x -= sampleR.z*exp2(16);
		// unpack y component
		modf(sampleR.x / exp2(8), sampleR.y);
		sampleR.x -= sampleR.y*exp2(8);

		/////////////////
		// unpack upward offset sample
		// unpack z component
		modf(sampleU.x / exp2(16), sampleU.z);
		sampleU.x -= sampleU.z*exp2(16);
		// unpack y component
		modf(sampleU.x / exp2(8), sampleU.y);
		sampleU.x -= sampleU.y*exp2(8);

		/////////////////
		// unpack right and up offset sample
		// unpack z component
		modf(sampleRU.x / exp2(16), sampleRU.z);
		sampleRU.x -= sampleRU.z*exp2(16);
		// unpack y component
		modf(sampleRU.x / exp2(8), sampleRU.y);
		sampleRU.x -= sampleRU.y*exp2(8);
#endif

		float2 normalisedSampleOffset = texSampleOffset / s;
		// do a bilinear interpolation of the 4 samples
		float4 smoothedSample = sampleC*(1.0f - normalisedSampleOffset.x)*(1.0f - normalisedSampleOffset.y);
		smoothedSample += sampleR*(normalisedSampleOffset.x)*(1.0f - normalisedSampleOffset.y);
		smoothedSample += sampleU*(1.0f - normalisedSampleOffset.x)*(normalisedSampleOffset.y);
		smoothedSample += sampleRU*(normalisedSampleOffset.x)*(normalisedSampleOffset.y);

#if !GPU_DAMAGE_WRITE_ENABLED
		// 3 components will be 0->255, scale to -1.0f->1.0f
		smoothedSample.x = (smoothedSample.x - GTA_VEHICLE_MAX_DAMAGE_RESOLUTION) / GTA_VEHICLE_MAX_DAMAGE_RESOLUTION;
		smoothedSample.y = (smoothedSample.y - GTA_VEHICLE_MAX_DAMAGE_RESOLUTION) / GTA_VEHICLE_MAX_DAMAGE_RESOLUTION;
		smoothedSample.z = (smoothedSample.z - GTA_VEHICLE_MAX_DAMAGE_RESOLUTION) / GTA_VEHICLE_MAX_DAMAGE_RESOLUTION;
#endif

		smoothedSample *= min(1.0f, magR / BoundRadius);
		smoothedSample *= DamageMultiplier;

		float retVal = saturate(1.0f-saturate(length(smoothedSample.xyz))*4.0f);
//		float retVal = saturate(1.0f-saturate(saturate(length(smoothedSample))-0.25f)*4.0f);

		outPos = _inPos + smoothedSample.xyz*gDmgScale;

		if (DamagedWheelOffsets[0].w > 0.0)
		{
			float3 wheelPosition = DamagedWheelOffsets[0].xyz - DamageTextureOffset;
 			float3 directionFromWheel = outPos - wheelPosition;
 			float distanceToWheel = length(directionFromWheel);
			float tValue = clamp (distanceToWheel / (DamagedWheelOffsets[0].w * 1.1), 0.0, 1.0);

			if (tValue < 1.0)
			{
				float3 newPos = wheelPosition + ((directionFromWheel / distanceToWheel) * (0.9 + (tValue * 0.1)) * (DamagedWheelOffsets[0].w * 1.1));
				outPos.y = newPos.y;
			}
		}

		if (DamagedWheelOffsets[1].w > 0.0)
		{
			float3 wheelPosition = DamagedWheelOffsets[1].xyz - DamageTextureOffset;
 			float3 directionFromWheel = outPos - wheelPosition;
 			float distanceToWheel = length(directionFromWheel);
			float tValue = clamp (distanceToWheel / (DamagedWheelOffsets[1].w * 1.1), 0.0, 1.0);

			if (tValue < 1.0)
			{
				float3 newPos = wheelPosition + ((directionFromWheel / distanceToWheel) * (0.9 + (tValue * 0.1)) * (DamagedWheelOffsets[1].w * 1.1));
				outPos.y = newPos.y;
			}
		}

		/////////////////
		// ok now to calculate deformation's affect on the normals
		outNormal = _inNormal;
		if(bProcessNormal)
		{
			float3 normalDmgMask = float3(0.1f, 1.0f, 0.3333f);

			// work out the radial position of the base sample
			float2 Tsample = texSampleCoords - texSampleOffset;
			float3 Rsample = GetRadialPosFromTexCoord(Tsample);

#if !GPU_DAMAGE_WRITE_ENABLED
			sampleC.x = (sampleC.x - GTA_VEHICLE_MAX_DAMAGE_RESOLUTION) / GTA_VEHICLE_MAX_DAMAGE_RESOLUTION;
			sampleC.y = (sampleC.y - GTA_VEHICLE_MAX_DAMAGE_RESOLUTION) / GTA_VEHICLE_MAX_DAMAGE_RESOLUTION;
			sampleC.z = (sampleC.z - GTA_VEHICLE_MAX_DAMAGE_RESOLUTION) / GTA_VEHICLE_MAX_DAMAGE_RESOLUTION;
#endif

			sampleC *= min(1.0f, magR / BoundRadius);
			sampleC *= DamageMultiplier;

			// work out the radial position of the right sample
			float2 TsampleR = texSampleCoords - texSampleOffset + float2(s, 0);
			float3 RsampleR = GetRadialPosFromTexCoord(TsampleR); // + float3(0.00000001f, 0.00000001f, 0.00000001f);//Rsample + float3(0.03f, 0.03f, 0.03f);

#if !GPU_DAMAGE_WRITE_ENABLED
			sampleR.x = (sampleR.x - GTA_VEHICLE_MAX_DAMAGE_RESOLUTION) / GTA_VEHICLE_MAX_DAMAGE_RESOLUTION;
			sampleR.y = (sampleR.y - GTA_VEHICLE_MAX_DAMAGE_RESOLUTION) / GTA_VEHICLE_MAX_DAMAGE_RESOLUTION;
			sampleR.z = (sampleR.z - GTA_VEHICLE_MAX_DAMAGE_RESOLUTION) / GTA_VEHICLE_MAX_DAMAGE_RESOLUTION;
#endif

			sampleR *= min(1.0f, magR / BoundRadius);
			sampleR *= DamageMultiplier;

			float3 Rdiff = RsampleR - Rsample;
			float3 Sdiff = (float3)(sampleR - sampleC);
			float fMult = dot(Sdiff, _inNormal);
			//fMult /= length(Rdiff);
			if(dot(Rdiff, Rdiff) > 0.0f)
			{
				fMult /= dot(Rdiff, Rdiff);
				outNormal += normalDmgMask*Rdiff*fMult*gDmgScale;
			}

			// work out the radial position of the upper sample
			float2 TsampleU = texSampleCoords - texSampleOffset + float2(0, s);
			float3 RsampleU = GetRadialPosFromTexCoord(TsampleU);

#if !GPU_DAMAGE_WRITE_ENABLED
			sampleU.x = (sampleU.x - GTA_VEHICLE_MAX_DAMAGE_RESOLUTION) / GTA_VEHICLE_MAX_DAMAGE_RESOLUTION;
			sampleU.y = (sampleU.y - GTA_VEHICLE_MAX_DAMAGE_RESOLUTION) / GTA_VEHICLE_MAX_DAMAGE_RESOLUTION;
			sampleU.z = (sampleU.z - GTA_VEHICLE_MAX_DAMAGE_RESOLUTION) / GTA_VEHICLE_MAX_DAMAGE_RESOLUTION;
#endif

			sampleU *= min(1.0f, magR / BoundRadius);
			sampleU *= DamageMultiplier;

			Rdiff = RsampleU - Rsample;
			Sdiff = (float3)(sampleU - sampleC);
			fMult = dot(Sdiff, _inNormal);
			//fMult /= length(Rdiff);
			if(dot(Rdiff, Rdiff) > 0.0f)
			{
				fMult /= dot(Rdiff, Rdiff);
				outNormal += normalDmgMask*Rdiff*fMult*gDmgScale;
			}

#if __MAX
		outNormal = normalize(outNormal);
#else
		outNormal = normalize(outNormal+EPSILON);
#endif

		}// if(bProcessNormals)...

		return(retVal);
	}
	else
	{
		outPos		= _inPos;
		outNormal	= _inNormal;
		return(1.0f);
	}

#if __PS3
	#undef exp2
#endif //__PS3...
}// end of _ApplyVehicleDamageInt()...

float ApplyVehicleDamage(float3 inPos, float3 inNormal, float scale,
						out float3 outPos, out float3 outNormal)
{
	return _ApplyVehicleDamageInt(inPos, inNormal, scale, outPos, outNormal, true);
}

float ApplyVehicleDamage(float3 inPos, float scale, out float3 outPos)
{
float3 _outNormal;
	return _ApplyVehicleDamageInt(inPos, float3(0,0,0), scale, outPos, _outNormal, false);
}

#else // USE_VEHICLE_DAMAGE

float ApplyVehicleDamage(float3 inPos, float3 inNormal, float scale,
						out float3 outPos, out float3 outNormal)
{	// pass-through:
	outPos		= inPos;
	outNormal	= inNormal;
	return(1.0f);
}

float ApplyVehicleDamage(float3 inPos, float scale, out float3 outPos)
{	// pass-through:
	outPos		= inPos;
	return(1.0f);
}
#endif //USE_VEHICLE_DAMAGE...




/////////////////////////////////////////////////////////////////////////////////////////////////////////////


//
//
//
//
#if !defined(VEHICLE_TYRE_DEFORM)
void ApplyTyreDeform(float3 inPos, float3 inWorldPos, float4 inColor, out float3 outPos, out float4 outColor, bool bFlatTyre)
{
	outPos = inPos;
	outColor = inColor;
}
#else // VEHICLE_TYRE_DEFORM...

#define TYRE_DEFORM_X_BULGE			(tyreDeformParams.x)
#define TYRE_DEFORM_Y_INWARDS		(tyreDeformParams.y)
#define TYRE_DEFORM_Z_INWARDS		(tyreDeformParams.z)
#define TYRE_DEFORM_X_SIDESLIP		(tyreDeformParams.w)

#define TYRE_INNER_RAD				(tyreDeformParams2.x)
#define TYRE_WORLD_GROUND_POS_Z		(tyreDeformParams2.y)
#define TYRE_DEFORM_SCALE			(tyreDeformParams2.z)
#define TYRE_DISSAPEAR_SCALE		(tyreDeformParams2.w)

void ApplyTyreDeform(float3 inPos, float3 inWorldPos, float4 inColor, out float3 outPos, out float4 outColor, bool bFlatTyre)
{
	outPos = inPos;
	outColor = inColor;

	if(tyreDeformSwitchOn)
	{
//		float distTireYZ = sqrt(inPos.y*inPos.y + inPos.z*inPos.z);	// use sqrt()
//		if(distTireYZ > VEHICLE_TYRE_INNER_RAD)
		float distTireYZ = (inPos.y*inPos.y + inPos.z*inPos.z);	// use sqr()
		if(distTireYZ > (TYRE_INNER_RAD*TYRE_INNER_RAD))
		{
			// mark tyre:
			//outColor = float4(1,0,0,1);

			// tyre disappears:
			outPos.xyz *= TYRE_DISSAPEAR_SCALE;	// is 0.0 if tyre has dissapeared
			distTireYZ *= TYRE_DISSAPEAR_SCALE;	// is 0.0 if tyre has dissapeared

			// make inner radius 10% bigger, so outer tire points will be left untouched
			// (and tire deformation looks nicer):
			if(bFlatTyre)
			if(distTireYZ > ((TYRE_INNER_RAD*1.1f)*(TYRE_INNER_RAD*1.1f)))
			{
				float3 worldTyrePos = inWorldPos;
				// Add on a bit to let us blend the deformation in
				if(worldTyrePos.z < TYRE_WORLD_GROUND_POS_Z + 0.03f)
				{
					float tyreMoveScale = TYRE_WORLD_GROUND_POS_Z - worldTyrePos.z;
					if(tyreMoveScale < 0.0f)
						tyreMoveScale = 0.0f;

					float tyreDeformScale = (TYRE_WORLD_GROUND_POS_Z + 0.03f - worldTyrePos.z) * TYRE_DEFORM_SCALE;
					if(tyreDeformScale > 1.0f)
						tyreDeformScale = 1.0f;
				
					// move tyre inwards towards alloy:
					outPos.y += TYRE_DEFORM_Y_INWARDS*tyreMoveScale;
					outPos.z += TYRE_DEFORM_Z_INWARDS*tyreMoveScale;

					// side flop scale:
					outPos.x -= TYRE_DEFORM_X_SIDESLIP*tyreDeformScale;
					// tyre bulges outwards
					outPos.x += inPos.x*TYRE_DEFORM_X_BULGE*tyreDeformScale;
				}

				#if 0
					// test: funny tyre deform:
					float4x3 tyreDeformMat;
					tyreDeformMat[0] = float3(1,0,0);
					tyreDeformMat[1] = float3(0,1,0);
					tyreDeformMat[2] = float3(0,0,1);
					tyreDeformMat[3] = float3(0,0,0);
					// tyreMatrix.a.x = 1.0f +  WHEEL_BURST_SHIFT_MULT * fBurstRatio * (m_fWheelRadius - m_fWheelRimRadius);
					// tyreMatrix.b.y = 1.0f - (rage::Abs(tyreMatrix.d.y) / m_fWheelRadius);
					// tyreMatrix.c.z = 1.0f - (rage::Abs(tyreMatrix.d.z) / m_fWheelRadius);
					tyreDeformMat[1].y = 1.0f - (abs(outPos.y));// / VEHICLE_TYRE_OUTER_RAD);
					tyreDeformMat[2].z = 1.0f - (abs(outPos.z));// / VEHICLE_TYRE_OUTER_RAD);
					outPos = mul(float4(outPos,1), tyreDeformMat);
				#endif
			}
		}

	}//if(tyreDeformSwitchOn)...

}// end of ApplyTyreDeform()....

#if __SHADERMODEL >= 40

float ShouldDeform(float3 inPos)
{
	float distTireYZ = (inPos.y*inPos.y + inPos.z*inPos.z);	
	
	if(distTireYZ > ((TYRE_INNER_RAD*1.1f)*(TYRE_INNER_RAD*1.1f)))
	{
		float3 WorldPos = mul(float4(inPos,1), gWorld);

		if(WorldPos.z < TYRE_WORLD_GROUND_POS_Z + 0.03f)
		{
			return 1.0f;
		}
	}
	return 0.0f;
}

#endif //__SHADERMODEL >= 40

#endif //VEHICLE_TYRE_DEFORM...

#endif //__GTA_VEHICLE_DAMAGE_FXH__...
