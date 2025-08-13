#ifndef PRAGMA_DCL
	#pragma dcl position diffuse texcoord0 normal tangent0
	#define PRAGMA_DCL
#endif

// NOTE: this is actually a decal shader, but we cannot define DECAL_SHADER here because
// of a tools issue related to the fact that there is no diffuse sampler

#define SPECULAR 0
#define NORMAL_MAP 0
#define NO_SKINNING
#define USE_ALPHACLIP_FADE

#include "../common.fxh"

#include "../Lighting/lighting.fxh"
#include "../Debug/EntitySelect.fxh"

// ----------------------------------------------------------------------------------------------- //
// ----------------------------------------------------------------------------------------------- //
BEGIN_RAGE_CONSTANT_BUFFER(distance_map_locals,b0)
float3 fillColor : fillColor
<
	string UIName = "Fill Color";
	string UIWidget = "Numeric";
	float UIMin = 0.0;
	float UIMax = 1.0;
	float UIStep = 0.01;
> = {0.0, 0.0, 1.0};
EndConstantBufferDX10(distance_map_locals)

// ----------------------------------------------------------------------------------------------- //

BeginSampler(sampler, DistanceMap, distanceMapSampler, distanceMap)
	string UIName = "Distance Map";
	string TCPTemplateRelative = "maps_other/DistanceMap.tcp";
	int TCPAllowOverride = 0;
	string TextureType="Distance";
ContinueSampler(sampler, DistanceMap, distanceMapSampler, distanceMap)
	AddressU  = CLAMP;        
	AddressV  = CLAMP;
	AddressW  = CLAMP;
	MIPFILTER = MIPLINEAR;
	MINFILTER = LINEAR;
	MAGFILTER = LINEAR;
EndSampler;

// ----------------------------------------------------------------------------------------------- //
// ----------------------------------------------------------------------------------------------- //

struct vertexInput
{
	float3 pos			: POSITION;
	float4 diffuse		: COLOR0;
    float4 texCoord		: TEXCOORD0;
    float3 normal		: NORMAL;
	float4 tangent		: TANGENT0;
};

// ----------------------------------------------------------------------------------------------- //

struct vertexInputSimple
{
	float3 pos			: POSITION;
	float4 diffuse		: COLOR0;
    float4 texCoord		: TEXCOORD0;
    float3 normal		: NORMAL;
};

// ----------------------------------------------------------------------------------------------- //

struct vertexOutputDeferred
{
    DECLARE_POSITION(pos)
	float4 texCoord			: TEXCOORD0;
	float3 color			: TEXCOORD1;
	float3 worldNormal		: TEXCOORD2;
	float3 worldPos			: TEXCOORD3;
};

// ----------------------------------------------------------------------------------------------- //

struct vertexOutput
{
    DECLARE_POSITION(pos)
	float4 texCoord			: TEXCOORD0;	
	float4 color			: TEXCOORD1;
	float3 worldNormal		: TEXCOORD2;
	float3 worldPos			: TEXCOORD3;
};

// ----------------------------------------------------------------------------------------------- //

struct vertexOutputSimple
{
    DECLARE_POSITION(pos)
	float4 texCoord			: TEXCOORD0;
	float4 color			: TEXCOORD1;
	float3 worldNormal		: TEXCOORD2;
	float3 worldPos			: TEXCOORD3;
};

// ----------------------------------------------------------------------------------------------- //
// ----------------------------------------------------------------------------------------------- //

vertexOutput	VS_Transform(vertexInput IN)
{
	vertexOutput OUT;

    // Write out final position & texture coords
	float3	localPos	= IN.pos;
	float3	localNormal	= IN.normal;
	
    float4	projPos		= mul(float4(localPos,1), gWorldViewProj);
	float3  worldPos	= mul(float4(localPos,1), gWorld).xyz;

	// Transform normal by "transpose" matrix
	float3 worldNormal	= NORMALIZE_VS(mul(localNormal, (float3x3)gWorld));

	OUT.worldPos		= worldPos;
    OUT.pos				= projPos;
	OUT.worldNormal		= worldNormal;

	OUT.texCoord		= IN.texCoord;
	OUT.color			= IN.diffuse;
	
    return(OUT);
}

// ----------------------------------------------------------------------------------------------- //

vertexOutputDeferred	VS_Transform_deferred(vertexInput IN)
{
	vertexOutputDeferred OUT;

    // Write out final position & texture coords
	float3	localPos	= IN.pos;
	float3	localNormal	= IN.normal;

    float4	projPos		= mul(float4(localPos,1), gWorldViewProj);
	float3  worldPos	= mul(float4(localPos,1), gWorld).xyz;

	// Transform normal by "transpose" matrix
	float3 worldNormal	= NORMALIZE_VS(mul(localNormal, (float3x3)gWorld));

    OUT.pos				= projPos;
	OUT.texCoord		= IN.texCoord;
	OUT.worldNormal		= worldNormal;
	OUT.color			= IN.diffuse.rgb;	
	OUT.worldPos		= worldPos;

    return(OUT);
}

// ----------------------------------------------------------------------------------------------- //

vertexOutputSimple VS_TransformParaboloid(vertexInputSimple IN)
{
	vertexOutputSimple OUT;

    // Write out final position & texture coords
	float3	localPos	= IN.pos;
	float3	localNormal	= IN.normal;
	
	float3  worldPos	= mul(float4(localPos,1), gWorld).xyz;
	
	// Transform normal by "transpose" matrix
	float3 worldNormal	= NORMALIZE_VS(mul(localNormal, (float3x3)gWorld));

	OUT.worldPos		= worldPos;
    OUT.pos				= DualParaboloidPosTransform(localPos);
	OUT.worldNormal		= worldNormal;
	OUT.pos.w			= 1.0f;
	
	OUT.texCoord		= IN.texCoord;
	OUT.color			= IN.diffuse.rgba;

    return(OUT);
}

// ----------------------------------------------------------------------------------------------- //

half4 PS_Textured(vertexOutput IN) : COLOR
{
	float alpha = tex2D(distanceMapSampler, IN.texCoord.xy).x;
	float4 color = float4(fillColor, (alpha >= 0.5));

#if __XENON
	[isolate]  // the compile thinks the shader is "too complex" without t
#endif
	float4 pixelColor = calcDepthFxBasic(color, IN.worldPos);
	return(PackHdr(pixelColor));
}

// ----------------------------------------------------------------------------------------------- //

half4 PS_TexturedParaboloid(vertexOutputSimple IN) : COLOR
{
	float4 OUT = float4(1.0, 0.0, 0.0, 1.0);
	return PackHdr(OUT);
}
// ----------------------------------------------------------------------------------------------- //

DeferredGBuffer	PS_Textured_deferred(vertexOutputDeferred IN)
{
	// Compute some surface information.								
	StandardLightingProperties surfaceStandardLightingProperties = (StandardLightingProperties)0;

	// Distance map
	float alpha = tex2Dlod(distanceMapSampler, float4(IN.texCoord.xy, 0.0f, 0.0f)).x;

	surfaceStandardLightingProperties.diffuseColor = float4(fillColor, 1.0);
	surfaceStandardLightingProperties.naturalAmbientScale =  IN.color.r;
	surfaceStandardLightingProperties.artificialAmbientScale = IN.color.g;

	float2 gradX = ddx(IN.texCoord.xy);
	float2 gradY = ddy(IN.texCoord.xy);

	float4 DistEpsilonScaleMin = float4(10.0, 10.0, 0.0, 0.0);

	float2 ddxyMax = max(abs(gradX), abs(gradY));
	float2 DistEpsilon = max(ddxyMax.x, ddxyMax.y).xx * DistEpsilonScaleMin.xy;
	DistEpsilon = max(DistEpsilon, DistEpsilonScaleMin.zw); //Clamp the min epsilon for softer edges?
	float2 DistMinMax = float2(0.5 - DistEpsilon.x, 0.5 + DistEpsilon.y);

	alpha = smoothstep(DistMinMax.x, DistMinMax.y, alpha);

	// Pack the information into the GBuffer(s).
	DeferredGBuffer OUT = PackGBuffer(normalize(IN.worldNormal), surfaceStandardLightingProperties);
	OUT.col0.a = alpha;
	OUT.col3.a = alpha;

	return(OUT);
}

// ----------------------------------------------------------------------------------------------- //

DeferredGBuffer	PS_Textured_deferred_AlphaClip(vertexOutputDeferred IN)
{
#if !RSG_ORBIS && !RSG_DURANGO
	FadeDiscard( IN.pos, globalFade );
#endif

	// Compute some surface information.								
	StandardLightingProperties surfaceStandardLightingProperties = (StandardLightingProperties)0;

	// Distance map
	float alpha = tex2Dlod(distanceMapSampler, float4(IN.texCoord.xy, 0.0f, 0.0f)).x;

	surfaceStandardLightingProperties.diffuseColor = float4(fillColor, 1.0);
	surfaceStandardLightingProperties.naturalAmbientScale =  IN.color.r;
	surfaceStandardLightingProperties.artificialAmbientScale = IN.color.g;

	float2 gradX = ddx(IN.texCoord.xy);
	float2 gradY = ddy(IN.texCoord.xy);

	float4 DistEpsilonScaleMin = float4(10.0, 10.0, 0.0, 0.0);

	float2 ddxyMax = max(abs(gradX), abs(gradY));
	float2 DistEpsilon = max(ddxyMax.x, ddxyMax.y).xx * DistEpsilonScaleMin.xy;
	DistEpsilon = max(DistEpsilon, DistEpsilonScaleMin.zw); //Clamp the min epsilon for softer edges?
	float2 DistMinMax = float2(0.5 - DistEpsilon.x, 0.5 + DistEpsilon.y);

	alpha = smoothstep(DistMinMax.x, DistMinMax.y, alpha);

	// Pack the information into the GBuffer(s).
	DeferredGBuffer OUT = PackGBuffer(normalize(IN.worldNormal), surfaceStandardLightingProperties);
	OUT.col0.a = alpha;
	OUT.col3.a = alpha;

	return(OUT);
}

// ----------------------------------------------------------------------------------------------- //

half4 PS_NotImplemented() : COLOR
{
	return half4(1.0, 0.0, 1.0, 1.0);
}

// ----------------------------------------------------------------------------------------------- //
#if __MAX
// ----------------------------------------------------------------------------------------------- //

vertexOutput VS_MaxTransform(maxVertexInput IN)
{
	vertexInput input;

	input.pos		= IN.pos;
	input.diffuse	= IN.diffuse;

	input.texCoord	= Max2RageTexCoord4(float4(IN.texCoord0.xy,IN.texCoord1.xy));

	input.normal	= IN.normal;
	input.tangent	= float4(IN.tangent,1.0f);
	
	return VS_Transform(input);
}

// ----------------------------------------------------------------------------------------------- //

float4 PS_MaxTextured(vertexOutput IN) : COLOR
{
	float4 OUT;

	if(maxLightingToggle)
	{
	}
	else
	{
	}
	
	float alpha = tex2D(distanceMapSampler, IN.texCoord.xy).x;
	OUT = float4(fillColor, (alpha >= 0.5));

	return(saturate(OUT));
}

// ----------------------------------------------------------------------------------------------- //

technique tool_draw
{
    pass p0
    {
		MAX_TOOL_TECHNIQUE_RS
		AlphaBlendEnable = true;
		AlphaTestEnable	 = false;
		StencilEnable = false;
		ZEnable = true;
		ZWriteEnable = false;
		SrcBlend = SRCALPHA;
		DestBlend = INVSRCALPHA;

		VertexShader	= compile VERTEXSHADER	VS_MaxTransform();
		PixelShader		= compile PIXELSHADER	PS_MaxTextured();
    }  
}

// ----------------------------------------------------------------------------------------------- //
#else // __MAX
// ----------------------------------------------------------------------------------------------- //

#if DEFERRED_TECHNIQUES && NON_CUTOUT_DEFERRED_TECHNIQUES
technique deferred_draw
{
	pass p0 
    {
		zFunc = FixedupLESS;
		AlphaBlendEnable = true;
		AlphaTestEnable	 = false;
		StencilEnable = false;
		ZEnable = true;
		ZWriteEnable = false;
		SrcBlend = SRCALPHA;
		DestBlend = INVSRCALPHA;
		SET_COLOR_WRITE_ENABLE(RED+GREEN+BLUE, 0, 0, RED+GREEN)

		VertexShader = compile VERTEXSHADER	VS_Transform_deferred();
        PixelShader  = compile PIXELSHADER	PS_Textured_deferred()  CGC_FLAGS(CGC_DEFAULTFLAGS);
    }
}
#endif // DEFERRED_TECHNIQUES && NON_CUTOUT_DEFERRED_TECHNIQUES

// ----------------------------------------------------------------------------------------------- //

#if DEFERRED_TECHNIQUES && NON_CUTOUT_DEFERRED_TECHNIQUES && DRAWSKINNED_TECHNIQUES
technique deferred_drawskinned
{
	pass p0 
	{
		VertexShader = compile VERTEXSHADER	VS_Transform();
        PixelShader  = compile PIXELSHADER	PS_NotImplemented()  CGC_FLAGS(CGC_DEFAULTFLAGS);
    }
}
#endif // DEFERRED_TECHNIQUES && NON_CUTOUT_DEFERRED_TECHNIQUES && DRAWSKINNED_TECHNIQUES

// ----------------------------------------------------------------------------------------------- //

#if DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES
technique deferredalphaclip_draw
{
	pass p0 
	{       
		zFunc = FixedupLESS;
		AlphaBlendEnable = true;
		AlphaTestEnable	 = false;
		StencilEnable = false;
		ZEnable = true;
		ZWriteEnable = false;
		SrcBlend = SRCALPHA;
		DestBlend = INVSRCALPHA;
		SET_COLOR_WRITE_ENABLE(RED+GREEN+BLUE, 0, 0, RED+GREEN)
		
		VertexShader = compile VERTEXSHADER	VS_Transform_deferred();
		PixelShader  = compile PIXELSHADER	PS_Textured_deferred_AlphaClip()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
#endif // DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES

// ----------------------------------------------------------------------------------------------- //

#if FORWARD_TECHNIQUES
technique draw
{
	pass p0 
	{
		zFunc = FixedupLESS;
		AlphaBlendEnable = true;
		AlphaTestEnable	 = false;
		StencilEnable = false;
		ZEnable = true;
		ZWriteEnable = false;
		SrcBlend = SRCALPHA;
		DestBlend = INVSRCALPHA;
		SET_COLOR_WRITE_ENABLE(RED+GREEN+BLUE, 0, 0, RED+GREEN)

		VertexShader = compile VERTEXSHADER	VS_Transform();
        PixelShader  = compile PIXELSHADER	PS_Textured()  CGC_FLAGS(CGC_DEFAULTFLAGS);
    }
}
#endif // FORWARD_TECHNIQUES

// ----------------------------------------------------------------------------------------------- //

SHADERTECHNIQUE_ENTITY_SELECT(VS_Transform(), VS_Transform())
#include "../Debug/debug_overlay_distance_map.fxh"

// ----------------------------------------------------------------------------------------------- //
#endif // __MAX
// ----------------------------------------------------------------------------------------------- //
