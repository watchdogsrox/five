
#ifndef PRAGMA_DCL
	#pragma dcl position texcoord0 texcoord1 normal 
	#define PRAGMA_DCL
#endif
//-----------------------------------------------------
// Rage Curved Model
//

//
//
// Configure the megashder
//
#define USE_SPECULAR
#define IGNORE_SPECULAR_MAP
#define USE_NORMAL_MAP

#define PRAGMA_CONSTANT_ROPE
#define MEGASHADER_PN_TRIANGLE_OVERRIDE

#include "../../Megashader/megashader.fxh"

#ifdef SHADOW_USE_TEXTURE
// No alpha test for ropes, ever.
#undef SHADOW_USE_TEXTURE
#endif

struct CurveGPUVertexInput
{
    float3 pos			: POSITION;
	float3 normal		: NORMAL0;
	float3 tangent		: TEXCOORD0;
	float2 tex			: TEXCOORD1;
	
};

struct ropeVertexInputLD
{
    float3 pos			: POSITION;
};

#if __WIN32PC && __SHADERMODEL < 40
#define MAX_CURVE_POINTS		19	// curvedModel/curvedModel.h, l 27 
#else
#define MAX_CURVE_POINTS		39	// curvedModel/curvedModel.h, l 27 
#endif

BEGIN_RAGE_CONSTANT_BUFFER(rope,b2)
float		SegmentSize : SegmentSize ;
float		radius : Radius;
float3		uvScales : UVScales;  // model u scale, model v scale
float4		ropeColor : ropeColor = float4(1,0,0,1);
float4		curveCoeffs[ MAX_CURVE_POINTS * 3 ] : CurveCoefficients REGISTER2(vs,c64);  // tan.x, tan.y, tan.z, dist along rope
#if __WIN32PC
float4		approxTangent[ MAX_CURVE_POINTS + 1] : ApproximateTangent REGISTER2(vs,c170);  // tan.x, tan.y, tan.z, dist along rope
#else
float4		approxTangent[ MAX_CURVE_POINTS + 1] : ApproximateTangent REGISTER2(vs,c196);  // tan.x, tan.y, tan.z, dist along rope
#endif
EndConstantBufferDX10(rope)

float3 GetPosOnCurve( int curvePos, float t )
{
	float  t2 = t * t;
	float4	tv = float4( 1.0f, t, t2,  t2 * t );

	return  float3( dot( curveCoeffs[ curvePos].xyzw, tv.xyzw ),
				 dot( curveCoeffs[ curvePos + 1].xyzw, tv.xyzw ),
				 dot( curveCoeffs[ curvePos + 2].xyzw, tv.xyzw ) );
}

float3 GetCurvedBasePosition( int segmentIndex, float t,  out float3 tangent )
{
	int		curvePos = segmentIndex * 3;
	
	float3 pos = GetPosOnCurve( curvePos, t );
	float3 curvetangent = GetPosOnCurve( curvePos, t + 0.001f );
		
	tangent = normalize( curvetangent - pos );
	return pos;
}

void GenerateRopeVertex( float3 inPos, float3 inNormal, float3 inTangent, out float3 outPos, out float3 outNormal, out float3 outTangent)
{
    float curvePos = inPos.y;
    
	// calculate segment index
	int segmentIndex = curvePos * SegmentSize ; 
	float tValue = ( curvePos * SegmentSize )- (float)segmentIndex;

	float3 nt = float3( 0.0f, 1.0f, 0.0f );
	float3 basePos =  GetCurvedBasePosition( segmentIndex,  tValue,  nt );// + float3( 2.0f, 0.0f, 0.0f );
		
	// now need binormal
	float3 binormal;
	float3 tangent; 
	
	float3 approxTan1 = float3(approxTangent[segmentIndex].x, approxTangent[segmentIndex].y, approxTangent[segmentIndex].z);
	float3 approxTan2 = float3(approxTangent[segmentIndex+1].x, approxTangent[segmentIndex+1].y, approxTangent[segmentIndex+1].z);
	float3 approxTan = approxTan1 * ( 1 - tValue) +  approxTan2 *  tValue;

	binormal = normalize(cross( nt, approxTan ) );
	tangent = normalize( cross( binormal, nt ) );

	outPos = tangent * inPos.x * radius + binormal * inPos.z * radius +  basePos;
//	outPos = tangent * inPos.x * (radius * 0.2f) + binormal * inPos.z * radius +  basePos;
	outNormal = tangent * inNormal.x + binormal * inNormal.z + nt * inNormal.y;
	outTangent = tangent * inTangent.x + binormal * inTangent.z + nt * inTangent.y;
}

float3 GenerateRopePos( float3 inPos )
{
    float curvePos = inPos.y;
    
	// calculate segment index
	int segmentIndex = curvePos * SegmentSize ; 
	float tValue = ( curvePos * SegmentSize )- (float)segmentIndex;

	float3 nt = float3( 0.0f, 1.0f, 0.0f );
	float3 basePos =  GetCurvedBasePosition( segmentIndex,  tValue,  nt );// + float3( 2.0f, 0.0f, 0.0f );
		
	// now need binormal
	float3 binormal;
	float3 tangent; 
	
	float3 approxTan1 = float3(approxTangent[segmentIndex].x, approxTangent[segmentIndex].y, approxTangent[segmentIndex].z);
	float3 approxTan2 = float3(approxTangent[segmentIndex+1].x, approxTangent[segmentIndex+1].y, approxTangent[segmentIndex+1].z);
	float3 approxTan = approxTan1 * ( 1 - tValue) +  approxTan2 *  tValue;

	binormal = normalize(cross( nt, approxTan ) );
	tangent = normalize( cross( binormal, nt ) );

	return (tangent * inPos.x * radius + binormal * inPos.z * radius +  basePos);
}



vertexOutput VS_Rope( CurveGPUVertexInput IN) 
{
    vertexOutput OUT;
 
	float3 pos;
	float3 normal;
	float3 tangent;
	
	GenerateRopeVertex( IN.pos, IN.normal, IN.tangent, pos, normal, tangent);
	
	// output position value
    OUT.pos = mul(float4( pos, 1.0), gWorldViewProj);
    
    // send world position to pixel shader
    OUT.worldPos.xyz = (float3) mul(float4( pos,1), gWorld);
    OUT.worldPos.w = 1.0f;
    
    // create tangent space system
	OUT.worldNormal = normalize( mul(normal, (float3x3)gWorld));

#if NORMAL_MAP	
    OUT.worldTangent =  normalize( mul(tangent, (float3x3)gWorld));  
    OUT.worldBinormal = normalize(cross(OUT.worldNormal.xyz, OUT.worldTangent.xyz));  
#endif // NORMAL_MAP
    
    // output texture coordinate
    OUT.texCoord.xy = float2(IN.tex.x * uvScales.x, IN.tex.y * uvScales.y );
	
    OUT.color0.rgb = ropeColor.rgb;
    OUT.color0.a = 1.0f;
    
  // Compute the eye vector
#if REFLECT
    OUT.worldEyePos = normalize(gViewInverse[3].xyz - OUT.worldPos.xyz); 
#endif // REFLECT

	RAGE_COMPUTE_CLIP_DISTANCES(OUT.pos);
    
    return OUT;
}

#if SHADOW_CASTING

	vertexOutputLD VS_RopeLDepth(ropeVertexInputLD IN)
	{
		vertexOutputLD OUT = (vertexOutputLD)0;

		OUT.pos = TransformCMShadowVert( GenerateRopePos(IN.pos), SHADOW_NEEDS_DEPTHINFO_OUT(OUT));

		return OUT;
	}

#endif // SHADOW_CASTING

DeferredGBuffer PS_Rope_Deferred(vertexOutput IN)
{
	DeferredGBuffer OUT;

	SurfaceProperties surfaceInfo = GetSurfaceProperties(
		IN.color0.rgba, 
#if PALETTE_TINT
		IN.color1.rgba,
#endif
		DIFFUSE_PS_INPUT.xy,
		DiffuseSampler, 
#if SPEC_MAP
		SPECULAR_PS_INPUT.xy,
		SpecSampler,
#endif	// SPEC_MAP
#if REFLECT
		IN.worldEyePos.xyz,
		REFLECT_TEX_SAMPLER,
	#if REFLECT_MIRROR
		float2(0,0),
		float4(0,0,0,1), // not supported
	#endif // REFLECT_MIRROR
#endif	// REFLECT
#if NORMAL_MAP
		IN.texCoord.xy,
		BumpSampler, 
		IN.worldTangent.xyz,
		IN.worldBinormal.xyz,
	#if PARALLAX
		IN.tanEyePos.xyz,
	#endif //PARALLAX...
#endif	// NORMAL_MAP
		IN.worldNormal.xyz
		);		
		
	// Compute some surface information.								
	StandardLightingProperties surfaceStandardLightingProperties = DeriveLightingPropertiesForCommonSurface( surfaceInfo );	

	// Pack the information into the GBuffer(s).
	OUT = PackGBuffer(
						surfaceInfo.surface_worldNormal.xyz,
						surfaceStandardLightingProperties );

	return OUT;
}

DeferredGBuffer PS_Rope_Deferred_NoNormalMap(vertexOutput IN)
{
	DeferredGBuffer OUT;

	SurfaceProperties surfaceInfo = GetSurfaceProperties(
		IN.color0.rgba, 
#if PALETTE_TINT
		IN.color1.rgba,
#endif
		DIFFUSE_PS_INPUT.xy,
		DiffuseSampler, 
#if SPEC_MAP
		SPECULAR_PS_INPUT.xy,
		SpecSampler,
#endif	// SPEC_MAP
#if REFLECT
		IN.worldEyePos.xyz,
		REFLECT_TEX_SAMPLER,
	#if REFLECT_MIRROR
		float2(0,0),
		float4(0,0,0,1), // not supported
	#endif // REFLECT_MIRROR
#endif	// REFLECT
#if NORMAL_MAP
		IN.texCoord.xy,
		BumpSampler, 
		IN.worldTangent.xyz,
		IN.worldBinormal.xyz,
	#if PARALLAX
		IN.tanEyePos.xyz,
	#endif //PARALLAX...
#endif	// NORMAL_MAP
		IN.worldNormal.xyz
		);		

	// Compute some surface information.								
	StandardLightingProperties surfaceStandardLightingProperties = DeriveLightingPropertiesForCommonSurface( surfaceInfo );	

	// Pack the information into the GBuffer(s).
	OUT = PackGBuffer(
						normalize(IN.worldNormal.xyz),
						surfaceStandardLightingProperties );

	return OUT;
}

DeferredGBuffer PS_Rope_Deferred_NoDiffuseMap(vertexOutput IN)
{
	DeferredGBuffer OUT;

	SurfaceProperties surfaceInfo = GetSurfaceProperties(
		IN.color0.rgba, 
#if PALETTE_TINT
		IN.color1.rgba,
#endif
		DIFFUSE_PS_INPUT.xy,
		DiffuseSampler, 
#if SPEC_MAP
		SPECULAR_PS_INPUT.xy,
		SpecSampler,
#endif	// SPEC_MAP
#if REFLECT
		IN.worldEyePos.xyz,
		REFLECT_TEX_SAMPLER,
	#if REFLECT_MIRROR
		float2(0,0),
		float4(0,0,0,1), // not supported
	#endif // REFLECT_MIRROR
#endif	// REFLECT
#if NORMAL_MAP
		IN.texCoord.xy,
		BumpSampler, 
		IN.worldTangent.xyz,
		IN.worldBinormal.xyz,
	#if PARALLAX
		IN.tanEyePos.xyz,
	#endif //PARALLAX...
#endif	// NORMAL_MAP
		IN.worldNormal.xyz
		);		

	
	// Compute some surface information.								
	StandardLightingProperties surfaceStandardLightingProperties = DeriveLightingPropertiesForCommonSurface( surfaceInfo );	

	// No diffuse map: Override it, the compiler will take care of the rest.
	surfaceStandardLightingProperties.diffuseColor = IN.color0.rgba;

	// Pack the information into the GBuffer(s).
	OUT = PackGBuffer(normalize(IN.worldNormal.xyz), surfaceStandardLightingProperties );

	return OUT;
}

#if DEFERRED_TECHNIQUES && NON_CUTOUT_DEFERRED_TECHNIQUES
technique deferred_draw
{
	pass p0 
	{        
		#include "../../Megashader/megashader_deferredRS.fxh"
		VertexShader = compile VERTEXSHADER	VS_Rope();
		PixelShader  = compile PIXELSHADER	PS_Rope_Deferred()  CGC_FLAGS(CGC_DEFAULTFLAGS);			
	}

	pass p1
	{        
		#include "../../Megashader/megashader_deferredRS.fxh"
		VertexShader = compile VERTEXSHADER	VS_Rope();
		PixelShader  = compile PIXELSHADER	PS_Rope_Deferred_NoNormalMap()  CGC_FLAGS(CGC_DEFAULTFLAGS);			
	}

	pass p2
	{        
		#include "../../Megashader/megashader_deferredRS.fxh"
		VertexShader = compile VERTEXSHADER	VS_Rope();
		PixelShader  = compile PIXELSHADER	PS_Rope_Deferred_NoDiffuseMap()  CGC_FLAGS(CGC_DEFAULTFLAGS);			
	}
}
#endif // DEFERRED_TECHNIQUES && NON_CUTOUT_DEFERRED_TECHNIQUES

SHADERTECHNIQUE_CASCADE_SHADOWS_ROPE()
SHADERTECHNIQUE_LOCAL_SHADOWS(VS_RopeLDepth, VS_RopeLDepth, PS_LinearDepth);
