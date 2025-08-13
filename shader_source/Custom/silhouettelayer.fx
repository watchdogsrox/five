#ifndef PRAGMA_DCL
	#pragma dcl position
	#define PRAGMA_DCL
#endif

#include "../common.fxh"

struct vertexInputUnlit
{
	float3 pos : POSITION;
};

struct vertexOutputUnlit
{
    DECLARE_POSITION(pos)

    float3 WorldPos			: TEXCOORD0;
    
#if __PS3
    float ClipSpaceDepth	: TEXCOORD1;
#endif // __PS3
};

#if __MAX
vertexOutputUnlit VS_TransformUnlit(maxVertexInput IN)
{
	vertexOutputUnlit OUT;

    OUT.pos = mul(float4(IN.pos.xyz, 1.0f),	gWorldViewProj);
    OUT.WorldPos = float3(0.0, 0.0, 0.0);
    
    return(OUT);
}

float4 PS_Unlit() : COLOR
{
	return float4(1.0, 0.0, 0.0, 1.0);
}

#else

vertexOutputUnlit VS_TransformSilhouette(vertexInputUnlit IN)
{
	vertexOutputUnlit OUT;

    OUT.pos = mul(float4(IN.pos.xyz, 1.0f),	gWorldViewProj);
    OUT.WorldPos = mul(float4(IN.pos.xyz, 1.0f), gWorld);
    
    return(OUT);
}

half4 FogDebugCommon(vertexOutputUnlit IN)
{
	// Kill pixels under water
#if !__MAX	
	clip(IN.WorldPos.z);
#endif // !__MAX
	
	// Just output a red color
	return half4(1.0, 0.0, 0.0, 1.0);
}

float4 FogColorCommon(vertexOutputUnlit IN)
{
	// Kill pixels under water
#if !__MAX	
	clip(IN.WorldPos.z);
#endif // !__MAX	

#if __PS3
	// Clip pixels in front of the near plane
	clip(IN.ClipSpaceDepth);
#endif // __PS3

	float4 finalColor = CalcFogData(IN.WorldPos - gViewInverse[3].xyz);
	finalColor.w = 1.0;

	return finalColor;
}

half4 PS_FogColor(vertexOutputUnlit IN) : COLOR
{
	return PackHdr(FogColorCommon(IN)); // Ignore fog blend value - we use only the fog color
}

half4 PS_FogDebug(vertexOutputUnlit IN) : COLOR
{
	return PackHdr(FogDebugCommon(IN)); // Ignore fog blend value - we use only the fog color
}

OutHalf4Color PS_FogColorWaterReflection(vertexOutputUnlit IN) : COLOR
{
	return CastOutHalf4Color(PackReflection(FogColorCommon(IN)));
}

OutHalf4Color PS_FogDebugWaterReflection(vertexOutputUnlit IN) : COLOR
{
	return CastOutHalf4Color(PackReflection(FogDebugCommon(IN)));
}
#endif // __MAX

//////////////////////////////////////////////////////////////////
// Techniques

#if __MAX
	// ===============================
	// Tool technique
	// ===============================
	technique tool_draw
	{
		pass P0
		{
			MAX_TOOL_TECHNIQUE_RS
			VertexShader = compile VERTEXSHADER	VS_TransformUnlit();
			PixelShader  = compile PIXELSHADER	PS_Unlit() CGC_FLAGS(CGC_DEFAULTFLAGS);
		}  
	}
#else //__MAX...

technique draw
{
	pass p0 // Normal rendering
	{
		VertexShader = compile VERTEXSHADER	VS_TransformSilhouette();
		PixelShader  = compile PIXELSHADER	PS_FogColor() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
	
#if !defined(SHADER_FINAL)
	pass p1 // Debug rendering
	{
		VertexShader = compile VERTEXSHADER	VS_TransformSilhouette();
		PixelShader  = compile PIXELSHADER	PS_FogDebug() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
#endif // !defined(SHADER_FINAL)
}

technique waterreflection_draw
{
	pass p0 // High quality
	{
		VertexShader = compile VERTEXSHADER	VS_TransformSilhouette();
		PixelShader  = compile PIXELSHADER	PS_FogColorWaterReflection()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
	
#if !defined(SHADER_FINAL)
	pass p1 // Debug rendering
	{
		VertexShader = compile VERTEXSHADER	VS_TransformSilhouette();
		PixelShader  = compile PIXELSHADER	PS_FogDebugWaterReflection() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
#endif // !defined(SHADER_FINAL)
}

#endif // __MAX...
