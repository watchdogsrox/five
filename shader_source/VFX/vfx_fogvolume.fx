#ifndef PRAGMA_DCL
	#pragma dcl position
	#define PRAGMA_DCL
#endif

#define LTYPE pointCM

#if MULTISAMPLE_TECHNIQUES
#define SAMPLE_INDEX	IN.sampleIndex
#else
#define SAMPLE_INDEX	0
#endif
#define DEFERRED_UNPACK_LIGHT

#include "../common.fxh"
#include "../Lighting/lighting.fxh"
///////////////////////////////////////////////////////////////////////////////
// VFX_FOGBALL.FX - fog Volume shader
///////////////////////////////////////////////////////////////////////////////

// ----------------------------------------------------------------------------------------------- //
// SHADER STURCTURES
// ----------------------------------------------------------------------------------------------- //

struct vfxFogVolumeVertexInput
{
	float4 pos			: POSITION;
};

// ----------------------------------------------------------------------------------------------- //

struct vfxFogVolumeVertexOutput
{
	float3 eyeRay				: TEXCOORD0; 
	float3 screenPos			: TEXCOORD1; 
	float4 intersect			: TEXCOORD2; // w is lengthIntersections
	float4 gradient				: TEXCOORD3; // w us eyeRay.z
	float3 color				: TEXCOORD4;
	DECLARE_POSITION_CLIPPLANES(pos)
};

// ----------------------------------------------------------------------------------------------- //

struct vfxFogVolumePixelInput
{
	float3 eyeRay				: TEXCOORD0;
	float3 screenPos			: TEXCOORD1;
	float4 intersect			: TEXCOORD2;
#if (1 && RSG_PC && !__MAX)
	inside_centroid
#endif
	float4 gradient				: TEXCOORD3;
	float3 color				: TEXCOORD4;
	DECLARE_POSITION_CLIPPLANES_PSIN(pos)
#if MULTISAMPLE_TECHNIQUES
	uint sampleIndex	: SV_SampleIndex;
#endif
};

// ----------------------------------------------------------------------------------------------- //

struct vfxFogVolumeVolumeData
{
	float2 intersect;
	float gradient;
};
// ----------------------------------------------------------------------------------------------- //

BeginConstantBufferDX10( vfx_fogvolume_locals )

float3 fogVolumeColor;
float3 fogVolumePosition;
float4 fogVolumeParams;
float4x4 fogVolumeTransform;
float4x4 fogVolumeInvTransform;

EndConstantBufferDX10( vfx_fogvolume_locals )

#define fogVolumeDensity			(fogVolumeParams.x)
#define fogVolumeExponent			(fogVolumeParams.y)
#define fogVolumeRange				(fogVolumeParams.z)
#define fogVolumeFogMult			(fogVolumeParams.w)

// ----------------------------------------------------------------------------------------------- //


BeginSampler(	sampler, fogVolumeDepthBuffer, fogVolumeDepthSampler, fogVolumeDepthBuffer)
ContinueSampler(sampler, fogVolumeDepthBuffer, fogVolumeDepthSampler, fogVolumeDepthBuffer)
AddressU  = CLAMP;
AddressV  = CLAMP;
AddressW  = CLAMP; 
MINFILTER = POINT;
MAGFILTER = POINT;
MIPFILTER = POINT;
EndSampler;

// ----------------------------------------------------------------------------------------------- //
float3 pos_fogVolume(float3 pos, bool isEllipsoidal)
{
	float3 vpos = (any3(pos) ? normalize(pos) : pos);
	vpos *= fogVolumeRange;
	if(isEllipsoidal)
	{
		vpos = mul(float4(vpos, 1.0f), fogVolumeTransform).xyz;
	}
	
	vpos += fogVolumePosition;
	return vpos;
}

// ----------------------------------------------------------------------------------------------- //

void volume_fogVolume(inout vfxFogVolumeVolumeData volumeData, float3 pos, float3 inpos_unused, bool outsideVolume, bool isEllipsoidal)
{
	float t0 = 0.0f;
	float t1 = 1.0f;

	float3 worldPos = pos; // world pos
	float3 eyePos   = gViewInverse[3].xyz;
	float3 eyeRay   = worldPos - eyePos;
	float3 centrePos = fogVolumePosition;

	if(isEllipsoidal)
	{
		//transform the stuff back into sphere coordinates but all in world space itself
		eyePos = mul(float4(eyePos, 1.0f), fogVolumeInvTransform).xyz;
		eyeRay = mul(float4(eyeRay, 1.0f), fogVolumeInvTransform).xyz;
		centrePos = mul(float4(centrePos, 1.0f), fogVolumeInvTransform).xyz;
	}	

	const float  r = fogVolumeRange; // sphere radius
	const float3 q = eyePos - centrePos;
	const float3 v = eyeRay;

	const float vv = dot(v, v);
	const float qv = dot(q, v);
	const float qq = dot(q, q); // constant
	const float rr = r*r;       // constant

	float a = vv;
	float b = qv;
	float c = qq - rr;
	float det = b*b - a*c;

	//Calculate both intersection points because we are unsure which one is further away
	t0 = (-b + sqrt(abs(det)))/a;
	t1 = (-b - sqrt(abs(det)))/a;
	if(outsideVolume)
	{

		//First intersection point has to be at the point itself which is 1.0f, and the second intersection point
		// is greater than 1.0f which is the max of the 2 calculated intersection points
		t1 = max(t0, t1);
		t0 = 1.0f;

	}
	else
	{
		//Back face Technique
		//First intersection point is the eye itself, and the second intersection point
		// is the point on the backface of the sphere.
		t0 = saturate(min(t0,t1));	
		t1 = 1.0f;
	}

	volumeData.intersect = float2(t0,t1);

	const float3 temp1 = q - v*min(0, qv)/vv;
	const float  temp2 = 1 - dot(temp1, temp1)/rr;

	volumeData.gradient = pow(saturate(temp2*temp2), fogVolumeExponent);
	
}

// ----------------------------------------------------------------------------------------------- //
//  based on Crytek SIGGraph 2006 paper but simplified. Removed Height based fog
float ComputeVolumetricFogVolumeValue( float3 point1ToPoint2) 
{
	const float threshold = 0.01;

	float dist = length( point1ToPoint2 );
	return 1.0f - saturate(exp( -fogVolumeDensity * dist)); 
}

///////////////////////////////////////////////////////////////////////////////
// VERTEX SHADERS
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// vfxFogVolumeVSCommon
///////////////////////////////////////////////////////////////////////////////

vfxFogVolumeVertexOutput vfxFogVolumeVSCommon(vfxFogVolumeVertexInput IN, bool outsideVolume, bool isEllipsoidal)
{
	vfxFogVolumeVertexOutput OUT;

	const float3 worldPos = pos_fogVolume(IN.pos.xyz, isEllipsoidal);
	const float3 eyePos   = gViewInverse[3].xyz;
	const float3 eyeRay   = worldPos - eyePos;

	vfxFogVolumeVolumeData volumeData;
	volume_fogVolume(volumeData, worldPos, IN.pos.xyz, outsideVolume, isEllipsoidal);

	OUT.pos       = mul(float4(worldPos, 1), gWorldViewProj);
	// Clamping z to w so that we dont exceed the far depth and get a hole in the mesh
	// using a max with 0.0f as we're using inverted depth. Far plane has z value of 0.0f
	//If renderphase does not support inverted depth then it might not work so well
	OUT.pos.z	  = max(OUT.pos.z, 0.0f);

	float2 vPos = convertToVpos(MonoToStereoClipSpace(OUT.pos), deferredLightScreenSize).xy;
	float2 screenPos = vPos / OUT.pos.w;
	//Get eye Ray from Screen Pos (used for unprojecting the depth correctly)
	float2 screenRay = screenPos*float2(2,-2) + float2(-1,1);

	//Pack Eye ray into output
	float3 eyeRayFromScreenSpace = GetEyeRay(screenRay).xyz;

	float3 eyeToIntersect0 = eyeRay * volumeData.intersect.x;
	float3 eyeToIntersect1 = eyeRay * volumeData.intersect.y;

	float3 col = fogVolumeColor;
	//lets fog the fog volume if intersect0 is not same as eyePos, which occurs outside the volume
	if(dot(eyeToIntersect0, eyeToIntersect0) > 1e-5)
	{
		float4 fogData = CalcFogData(eyeToIntersect0);
		col = lerp(col, fogData.rgb, saturate(fogData.a * fogVolumeFogMult));
	}
	OUT.color = col;
	OUT.eyeRay  = worldPos - eyePos;
	OUT.screenPos = float3(vPos.xy, OUT.pos.w);
	OUT.intersect = float4(volumeData.intersect.xy, dot(eyeToIntersect0, eyeToIntersect0), dot(eyeToIntersect1, eyeToIntersect1));
	OUT.gradient = float4(eyeRayFromScreenSpace, volumeData.gradient);

	RAGE_COMPUTE_CLIP_DISTANCES(OUT.pos);
	return OUT;
}


///////////////////////////////////////////////////////////////////////////////
// VS_vfxFogVolume_Outside
///////////////////////////////////////////////////////////////////////////////
vfxFogVolumeVertexOutput VS_vfxFogVolume_Outside(vfxFogVolumeVertexInput IN)
{
	return vfxFogVolumeVSCommon(IN, true, false);
}

///////////////////////////////////////////////////////////////////////////////
// VS_vfxFogVolume_Ellipsoid_Outside
///////////////////////////////////////////////////////////////////////////////
vfxFogVolumeVertexOutput VS_vfxFogVolume_Ellipsoid_Outside(vfxFogVolumeVertexInput IN)
{
	return vfxFogVolumeVSCommon(IN, true, true);
}

///////////////////////////////////////////////////////////////////////////////
// VS_vfxFogVolume_Inside
///////////////////////////////////////////////////////////////////////////////
vfxFogVolumeVertexOutput VS_vfxFogVolume_Inside(vfxFogVolumeVertexInput IN)
{
	return vfxFogVolumeVSCommon(IN, false, false);
}

///////////////////////////////////////////////////////////////////////////////
// VS_vfxFogVolume_Inside
///////////////////////////////////////////////////////////////////////////////
vfxFogVolumeVertexOutput VS_vfxFogVolume_Ellipsoid_Inside(vfxFogVolumeVertexInput IN)
{
	return vfxFogVolumeVSCommon(IN, false, true);
}

///////////////////////////////////////////////////////////////////////////////
// PIXEL SHADERS
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// PS_vfxFogVolume
// ----------------------------------------------------------------------------------------------- //
//Using 3*3 grid for interleaving samples
float4 vfxFogVolumePSCommon(vfxFogVolumePixelInput IN, uniform bool outsideVolume)
{
	const float3 eyePos   = gViewInverse[3].xyz;
	const float3 eyeRay   = IN.eyeRay;
	const float2 screenPos = IN.screenPos.xy/IN.screenPos.z;
	const float intersect1LenSqr = IN.intersect.w;
	const float3 eyeRayFromScreenSpace = IN.gradient.xyz;
	const float gradient = IN.gradient.w;

	const float depth = getLinearGBufferDepth(tex2D(fogVolumeDepthSampler, screenPos).x, deferredProjectionParams.zw);//This has the resolved depth buffer.  Instead of MSAA buffer from UnPackGBufferDepth( screenPos, 0 );

	//Calculate depth from GBuffer
	float3 GbufVec = eyeRayFromScreenSpace * depth;
	float3 GbufPos = eyePos + GbufVec;
	float GbufLengthSqr = dot(GbufVec, GbufVec);

	float3 intersect0 =  eyePos + eyeRay * IN.intersect.x;
	float3 intersect1 = eyePos + eyeRay * IN.intersect.y;
	float isGbufDepthCloser = step(GbufLengthSqr, intersect1LenSqr);
	intersect1 = isGbufDepthCloser? GbufPos : intersect1;
	float density = ComputeVolumetricFogVolumeValue(intersect1 - intersect0) * IN.gradient.w;

	if(!outsideVolume)
	{
		const float intersect0LenSqr = IN.intersect.z;
		//Perform depth test here for backface technique as HW Depth test is disabled
		density *= step(intersect0LenSqr, GbufLengthSqr);
	}
	
	float4 finalColor = PackHdr(float4(IN.color.rgb, density));
	return finalColor;
}


float4 PS_vfxFogVolume_Outside(vfxFogVolumePixelInput IN) : COLOR
{
	return vfxFogVolumePSCommon(IN, true);
}
float4 PS_vfxFogVolume_Inside(vfxFogVolumePixelInput IN) : COLOR
{
	return vfxFogVolumePSCommon(IN, false);
}

///////////////////////////////////////////////////////////////////////////////
// TECHNIQUES
///////////////////////////////////////////////////////////////////////////////
technique MSAA_NAME(fogvolume_default)
{
	pass MSAA_NAME(fogvolume_outside)
	{
		VertexShader = compile VERTEXSHADER VS_vfxFogVolume_Outside(); 
		PixelShader  = compile MSAA_PIXEL_SHADER PS_vfxFogVolume_Outside()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
	pass MSAA_NAME(fogvolume_inside)
	{
		VertexShader = compile VERTEXSHADER VS_vfxFogVolume_Inside(); 
		PixelShader  = compile MSAA_PIXEL_SHADER PS_vfxFogVolume_Inside()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}

technique MSAA_NAME(fogvolume_ellipsoidal)
{
	pass MSAA_NAME(fogvolume_outside)
	{
		VertexShader = compile VERTEXSHADER VS_vfxFogVolume_Ellipsoid_Outside(); 
		PixelShader  = compile MSAA_PIXEL_SHADER PS_vfxFogVolume_Outside()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
	pass MSAA_NAME(fogvolume_inside)
	{
		VertexShader = compile VERTEXSHADER VS_vfxFogVolume_Ellipsoid_Inside(); 
		PixelShader  = compile MSAA_PIXEL_SHADER PS_vfxFogVolume_Inside()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

}

