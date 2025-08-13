
#ifndef PRAGMA_DCL
	#pragma dcl position texcoord0 texcoord1 diffuse 
	#define PRAGMA_DCL
#endif
//
// GTA simple radar shader;
//
//	26/01/2006	- Andrzej:	- initial;
//
//
//
//
#define ALPHA_SHADER

#define  USE_DIFFUSE_SAMPLER
#include "../common.fxh"

#define SUPPORT_GEOMETRY_AA		(1 && __SHADERMODEL >= 40)
#define MANUAL_DEPTH_TEST		(__SHADERMODEL >= 40)		// should be synchnonized with MARKERS_MANUAL_DEPTH_TEST on the game side
#define ANTI_ALIAS_LOW_BORDER	(1)
#define WARP_TEXTURE_COORDS		(1)

BEGIN_RAGE_CONSTANT_BUFFER(radar_locals,b0)

float4 diffuseCol : DiffuseColor 
<
	string UIName	= "Radar Diffuse Color";
> = float4(1,1,1,1);

float4 clippingPlane : ClippingPlane = float4(0, 0, 0, 0);

float4 targetSize : TargetSize = float4(1, 1, 0, 0);

#if MANUAL_DEPTH_TEST
float4 distortionParams : DistortionParams = float4(0, 0, 0, 0);
float manualDepthTest : ManualDepthTest;
#endif

EndConstantBufferDX10( radar_locals )

#if MANUAL_DEPTH_TEST
BeginDX10Sampler(sampler, Texture2D<float>, DepthTexture, DepthSampler, DepthTexture)
ContinueSampler(sampler, DepthTexture, DepthSampler, DepthTexture)
	AddressU  = CLAMP;
	AddressV  = CLAMP;
	MinFilter = POINT;
	MagFilter = POINT;
	MIPFILTER = POINT;
EndSampler;

float2 WarpTexCoords(float2 texCoords, float distortionCoeff, float distortionCoeffCube)
{
	const float2 aspectCorrection = float2(targetSize.y/targetSize.x,1.0);
	const float2 dir = (texCoords-float2(0.5f,0.5f)) * aspectCorrection;
	const float distSqr = dot(dir, dir);
	const float distortionFactor = 1.0f + distSqr * (distortionCoeff + distortionCoeffCube*sqrt(distSqr));
	texCoords = distortionFactor.xx*(texCoords.xy-0.5.xx)+0.5f.xx;

	return texCoords;
}

float TestDepth(float3 screenPos)
{
	if (!manualDepthTest)
		return 1.0;

# if WARP_TEXTURE_COORDS
	float2 tc = WarpTexCoords(screenPos.xy / targetSize.xy, distortionParams.x, distortionParams.y);
	float oldDepth = tex2D(DepthSampler, tc - float2(0, 0.5)/targetSize.xy).x;
	float lowDepth = tex2D(DepthSampler, tc + float2(0, 0.5)/targetSize.xy).x;
# else
	float oldDepth = DepthTexture.Load(int3(screenPos.xy, 0)).x;
	float lowDepth = DepthTexture.Load(int3(screenPos.xy, 0) + int3(0,1,0)).x
# endif

# if SUPPORT_INVERTED_PROJECTION
	rageDiscard(screenPos.z < oldDepth);
# else
	rageDiscard(screenPos.z > oldDepth);
# endif
	return
# if ANTI_ALIAS_LOW_BORDER
#  if SUPPORT_INVERTED_PROJECTION
		screenPos.z < lowDepth ?
#  else
		screenPos.z > lowDepth ?
#  endif
		1.0 - (screenPos.z - lowDepth) / (oldDepth - lowDepth) :
# endif
		1.0;
}
#endif //MANUAL_DEPTH_TEST


#include "../Lighting/lighting.fxh"

struct radarVertexInput
{
    float3 pos			: POSITION;
    float2 texCoord0	: TEXCOORD0;
	float4 diffuse		: COLOR0;
    float3 normal		: NORMAL;
    float4 tangent		: TANGENT0;
#if SUPPORT_GEOMETRY_AA
    uint vertexId		: SV_VertexID;
#endif
};

struct vertexOutputUnlit
{
    DECLARE_POSITION(pos)
	float4 color0			: COLOR0;
    float2 texCoord0		: TEXCOORD0;
	float4 worldPos			: TEXCOORD1;
#if SUPPORT_GEOMETRY_AA
	uint vertexId			: TEXCOORD2;
#endif
};

vertexOutputUnlit VS_TransformUnlit(radarVertexInput IN)
{
	vertexOutputUnlit OUT;

	float3 localPos	=	  IN.pos;
	float2 texCoord0	= IN.texCoord0;
	float3 localNormal	= IN.normal;
	float4 vertColor	= IN.diffuse;
	float4 localTangent = IN.tangent;
	
    float4	projPos		= MonoToStereoClipSpace(mul(float4(localPos,1), gWorldViewProj));

    OUT.pos				= projPos;
    OUT.texCoord0		= texCoord0;
	OUT.color0			= vertColor;
	OUT.worldPos		= mul(float4(localPos,1), gWorld);
	
#if SUPPORT_GEOMETRY_AA
	OUT.vertexId		= IN.vertexId;
#endif

    return(OUT);
}

#if __MAX
//
//
//
//
vertexOutputUnlit VS_MaxTransformUnlit(maxVertexInput IN)
{
radarVertexInput radarIN;

	radarIN.pos			= IN.pos;
	radarIN.diffuse		= IN.diffuse;
	radarIN.texCoord0	= Max2RageTexCoord2(IN.texCoord0.xy);
	radarIN.normal		= IN.normal;
	radarIN.tangent		= float4(IN.tangent,1.0f);	// TODO: missing sign for binormal

	return VS_TransformUnlit(radarIN);

}// end of VS_MaxTransformUnlit()...
#endif //__MAX...


float4 PS_TexturedUnlit(vertexOutputUnlit IN ) : COLOR
{
	float alpha = 1.0;
#if MANUAL_DEPTH_TEST
	alpha *= TestDepth(IN.pos);
#endif

	float4 color0	= IN.color0;
	float4 texcolor	= tex2D(DiffuseSampler, IN.texCoord0);

	float4 pixelColor = color0 * texcolor * diffuseCol;

	float planeTest = (dot(float4(IN.worldPos.xyz, 1.0), clippingPlane) >= 0);

	return float4(pixelColor.rgb, pixelColor.a * planeTest * alpha);
}

#if SUPPORT_GEOMETRY_AA
vertexOutputUnlit VS_TransformUnlitAA(radarVertexInput IN)
{
	vertexOutputUnlit OUT = VS_TransformUnlit(IN);
	return OUT;
}

struct geometryOutputUnlit {
    vertexOutputUnlit base;
	float3 distToSides	:TEXCOORD6;	//in absolute screen space
};

float4 PS_TexturedUnlitAA(geometryOutputUnlit INAA) : COLOR
{
	float4 color = PS_TexturedUnlit(INAA.base);
	// emulate two-sided rendering (use with back-face culling)
	//color.a *= 2.0 - color.a;	DISABLED
	// enable AA by computing the distance to the closest edge
	float min_dist = min(min(INAA.distToSides.x, INAA.distToSides.y), INAA.distToSides.z);
	color.a *= clamp(min_dist, 0.0, 1.0);
	// done
	return color;
}

float2 DistanceTo(float2 pos, float2 e0, float2 e1, vertexOutputUnlit adj)
{
	const float2 adjScreen = (adj.pos.xy / adj.pos.w + 1.0) * 0.5 * targetSize.xy;
	float2 normal = normalize(float2(e1.y - e0.y, e0.x - e1.x));
	float dist = dot(normal, pos-e0);	// signed distance from the vertex to the opposite edge, in pixels
	float adjDist = dot(normal, adjScreen-e0);	// distance from the adjacent vertex to the same edge
	
	// Returned X = actual distance to the edge
	// Y = additional offset to this edge applied to all vertices
	float2 ret = float2(abs(dist), 0);
	
	if (adj.vertexId != 0 && dist * adjDist >= 0.0)
	{
		// silhuete edge: both the distance point and adjacent ones are on the same side
	}else
	{
		// inner edge, setting the offset based on the adjacent vertex
		ret.y = abs(adjDist);
	}
	
	return ret;
}

// refer here for help with the input order:
// http://msdn.microsoft.com/en-us/library/windows/desktop/bb509609%28v=vs.85%29.aspx

[maxvertexcount(3)]
void GS_TexturedUnlit(triangleadj vertexOutputUnlit input[6], inout TriangleStream<geometryOutputUnlit> outputStream)
{
	const float3x2 sp = float3x2(	//screen positions
		(input[0].pos.xy / input[0].pos.w + 1.0) * 0.5 * targetSize.xy,
		(input[2].pos.xy / input[2].pos.w + 1.0) * 0.5 * targetSize.xy,
		(input[4].pos.xy / input[4].pos.w + 1.0) * 0.5 * targetSize.xy
	);
	
	const float3x2 d = float3x2( //opposite edge distances and base offsets
		DistanceTo(sp[0], sp[1], sp[2], input[3]),
		DistanceTo(sp[1], sp[2], sp[0], input[5]),
		DistanceTo(sp[2], sp[0], sp[1], input[1])
	);
	float3 offset = float3(d[0].y, d[1].y, d[2].y);

	if(input[0].pos.w < 0.0 || input[2].pos.w < 0.0 || input[4].pos.w < 0.0) 
		offset = 1.0;

	geometryOutputUnlit v0 = { input[0], offset + float3(d[0].x, 0, 0) };
	outputStream.Append(v0);
	geometryOutputUnlit v1 = { input[2], offset + float3(0, d[1].x, 0) };
	outputStream.Append(v1);
	geometryOutputUnlit v2 = { input[4], offset + float3(0, 0, d[2].x) };
	outputStream.Append(v2);
    outputStream.RestartStrip();
}
#endif //SUPPORT_GEOMETRY_AA

#if __MAX
	// ===============================
	// Tool technique
	// ===============================
	technique tool_draw
	{
		pass P0
		{
			MAX_TOOL_TECHNIQUE_RS
			VertexShader= compile VERTEXSHADER	VS_MaxTransformUnlit();
			PixelShader	= compile PIXELSHADER	PS_TexturedUnlit()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}  
	}
#else //__MAX...

	//
	//
	//
	//
	//#if FORWARD_TECHNIQUES
	technique draw
	{
		pass p0 
		{        
			VertexShader = compile VERTEXSHADER	VS_TransformUnlit();
			PixelShader  = compile PIXELSHADER	PS_TexturedUnlit()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	//#endif // FORWARD_TECHNIQUES

	//#if UNLIT_TECHNIQUES
	technique unlit_draw
	{
		pass p0 
		{        
			VertexShader = compile VERTEXSHADER	VS_TransformUnlit();
			PixelShader  = compile PIXELSHADER	PS_TexturedUnlit()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	
	#if SUPPORT_GEOMETRY_AA
	technique11 unlit_draw_sm50
	{
		pass p0_sm50
		{        
			SetVertexShader(	compileshader( VSGS_SHADER,	VS_TransformUnlitAA() ));
			SetGeometryShader(	compileshader( gs_5_0,		GS_TexturedUnlit() ));
			SetPixelShader(		compileshader( PIXELSHADER,	PS_TexturedUnlitAA() ));
		}
	}
	#endif //SUPPORT_GEOMETRY_AA
	//#endif // UNLIT_TECHNIQUES

	//#if DEFERRED_TECHNIQUES && NON_CUTOUT_DEFERRED_TECHNIQUES
	technique deferred_draw
	{
		pass p0 
		{        
			VertexShader = compile VERTEXSHADER	VS_TransformUnlit();
			PixelShader  = compile PIXELSHADER	PS_TexturedUnlit()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	//#endif // DEFERRED_TECHNIQUES && NON_CUTOUT_DEFERRED_TECHNIQUES

	//#if FORWARD_TECHNIQUES
	technique lightweight0_draw
	{
		pass p0 
		{        
			VertexShader = compile VERTEXSHADER	VS_TransformUnlit();
			PixelShader  = compile PIXELSHADER	PS_TexturedUnlit()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	//#endif // FORWARD_TECHNIQUES

	//#if FORWARD_TECHNIQUES
	technique lightweight4_draw
	{
		pass p0 
		{        
			VertexShader = compile VERTEXSHADER	VS_TransformUnlit();
			PixelShader  = compile PIXELSHADER	PS_TexturedUnlit()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	//#endif // FORWARD_TECHNIQUES
#endif //__MAX...






