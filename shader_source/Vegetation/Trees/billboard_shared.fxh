
// Configure the shared billboard shader

#include "../Megashader/megashader.fxh"


BeginSampler(sampler,dtb,TextureSamplerBillboard,DiffuseTexBillboard)
ContinueSampler(sampler,dtb,TextureSamplerBillboard,DiffuseTexBillboard)
    AddressU  = WRAP;        
    AddressV  = WRAP;
    AddressW  = WRAP;
    MIPFILTER = MIPLINEAR;
    MINFILTER = HQLINEAR;
    MAGFILTER = MAGLINEAR;
EndSampler;

#if NORMAL_MAP
	BeginSampler(sampler2D,bumpTex,BumpSamplerBillboard,BumpTexBillboard)
	ContinueSampler(sampler2D,bumpTex,BumpSamplerBillboard,BumpTexBillboard)
		AddressU  = WRAP;        
		AddressV  = WRAP;
		AddressW  = WRAP;
		MIPFILTER = MIPLINEAR;
		MINFILTER = HQLINEAR;
		MAGFILTER = MAGLINEAR;
	EndSampler;
#endif	// NORMAL_MAP

ROW_MAJOR float4x4 gViewProj : ViewProjection;
ROW_MAJOR float4x4 gWorldMatrix : WorldMatrix
#if !__PS3
	: register(vs, c240)
#endif
	;

vertexOutput VS_Transform_Temp(rageVertexInputBump IN)
{
    vertexOutput OUT;
    
    float3 inPos = IN.pos;
    float3 inNrm = IN.normal;
    float4 inCpv = IN.diffuse;

#if NORMAL_MAP
    float4 inTan = IN.tangent;
#endif

	rtsProcessVertLightingUnskinned( inPos, 
							inNrm, 
#if NORMAL_MAP
							inTan,
#endif
							inCpv, 
							(float4x3) gWorldMatrix, 
							OUT.worldEyePos,
							OUT.worldNormal,
#if NORMAL_MAP
							OUT.worldBinormal,
							OUT.worldTangent,
#endif // NORMAL_MAP
							OUT.color0 
						);

    // Write out final position & texture coords
    float4 worldPos = mul(float4(inPos, 1), gWorldMatrix);
    OUT.pos =  mul(worldPos, gViewProj);
    
    // NOTE: These 3 statements may resolve to the exact same code -- rely on HLSL to strip worthless code
    DIFFUSE_VS_OUTPUT = DIFFUSE_VS_INPUT;
#if SPEC_MAP
    SPECULAR_VS_OUTPUT = SPECULAR_VS_INPUT;
#endif	// SPEC_MAP
#if NORMAL_MAP
	OUT.texCoord.xy = IN.texCoord0.xy;
#endif	// NORMAL_MAP

#if DIFFUSE2
	// Pack in 2nd set of texture coords if needed
	OUT.texCoord.zw = IN.texCoord1.xy;
#endif	// DIFFUSE2

#if SHADOW_MAP    
	OUT.shadowmap_pixelPos = OUT.pos;
#endif

#if SHADOW_CASTING

	float4 tPos=mul(float4(inPos,1), gWorldView);
	float4 wPos=mul(tPos, gViewInverse);
	float3 tNorm=mul(float4(inNrm,0), gWorldView).xyz;
	float3 wNorm=mul(float4(tNorm,0), gViewInverse).xyz;
		
	if (enableFlag0!=0.0f)
	{	
		OUT.warpPos0=mul(wPos, lightMatrix0);
		OUT.warpPos0.w=dot(wNorm, float3(lightMatrix0._m02, lightMatrix0._m12, lightMatrix0._m22));
	}	
	else
	{
		OUT.warpPos0=float4(0,0,0,1);
	}	

	if (enableFlag1!=0.0f)
	{	
		OUT.warpPos1=mul(wPos, lightMatrix1);
		OUT.warpPos1.w=dot(wNorm, float3(lightMatrix1._m02, lightMatrix1._m12, lightMatrix1._m22));
	}	
	else
	{
		OUT.warpPos1=float4(0,0,0,1);
	}

#endif // SHADOW_CASTING

	return OUT;
}


// ******************************
vertexOutput VS_Transform_Billboard(rageVertexInputBump IN)
{
    vertexOutput OUT;

	// normal and tangent are just the camera vectors (z and x):
	IN.normal=gViewInverse[2].xyz;
	IN.tangent=gViewInverse[0].xyzw;
	
	return VS_Transform_Temp(IN);
}

float4 PS_Textured_Billboard( vertexOutput IN): COLOR
{
	float4 diffuse = tex2D(TextureSamplerBillboard, IN.texCoord);
	if( diffuse.w < 0.392156862f  )
		diffuse.w = 0.0f;
	return diffuse;
/*	return rtsComputePixelColor( IN.color0, 
								IN.worldNormal,
								IN.texCoord.xy,
								TextureSamplerBillboard, 
#if DIFFUSE2
								IN.texCoord.zw,
								TextureSampler2,
#endif	// DIFFUSE2
#if SPEC_MAP
								IN.texCoord,
								SpecSampler,
#endif	// SPEC_MAP
#if REFLECT
								EnvironmentSampler,
#endif	// REFLECT
#if NORMAL_MAP
								IN.texCoord,
								BumpSamplerBillboard, 
								IN.worldTangent,
								IN.worldBinormal,
#endif	// NORMAL_MAP
								IN.worldEyePos
#if SHADOW_MAP
								,IN.shadowmap_pixelPos
#endif								
#if ATMOSPHERIC_SCATTER
								,IN.vExtinction,
								float4(IN.vInscatter.xyz,0)
#endif	// ATMOSPHERIC_SCATTER
								);
*/
}

#if FORWARD_TECHNIQUES
technique draw 
{
	pass p0 
	{
		AlphaTestEnable = true;
		CullMode = None;
		VertexShader = compile VERTEXSHADER VS_Transform_Billboard();
		PixelShader  = compile PIXELSHADER  PS_Textured_Billboard()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
#endif // FORWARD_TECHNIQUES