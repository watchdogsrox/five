//
// commonly used functions and defines

#ifndef __CAMERA_FXH
#define __CAMERA_FXH

void ApplyCameraAlignment(inout float3 position, inout float3 normal, float3 relativeCentre)
{
#if CAMERA_FACING || CAMERA_ALIGNED
	// Z-Up conversion
	position.xyz = float3(position.x, -position.z, position.y);
	normal.xyz	 = float3(normal.x, -normal.z, normal.y);
	float3 maxRelativeCentre   = float3(relativeCentre.x, -relativeCentre.z, relativeCentre.y);
#endif // CAMERA_FACING || CAMERA_ALIGNED

#if CAMERA_ALIGNED
	float3x3 vInverseViewAligned;

	// Roll around ?? vector in ?-axis
	vInverseViewAligned[0] = normalize(cross(gViewInverse[2].xyz, float3(0,0,-1)));		// Right
	vInverseViewAligned[1] = float3(0,0,-1);											// Up
	vInverseViewAligned[2] = normalize(cross(gViewInverse[0].xyz, float3(0,0,-1)));		// Depth
#elif CAMERA_FACING
	float3x3 vInverseViewAligned;
	vInverseViewAligned[0] =  gViewInverse[0].xyz;
	vInverseViewAligned[1] = -gViewInverse[1].xyz;
	vInverseViewAligned[2] =  gViewInverse[2].xyz;
#endif

#if CAMERA_ALIGNED || CAMERA_FACING 
	position = mul(position - maxRelativeCentre, vInverseViewAligned) + relativeCentre;
	normal = mul(normal.xyz, (float3x3)vInverseViewAligned);
#endif // USE_CAMERA_ALIGNED
}

#endif		// __CAMERA_FXH
