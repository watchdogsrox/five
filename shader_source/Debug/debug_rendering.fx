
#pragma strip off
#pragma dcl position diffuse texcoord0 normal texcoord1

#define DEFERRED_UNPACK_LIGHT
#define DEFERRED_UNPACK_LIGHT_CSM_FORWARD_ONLY // ensure deferred shader vars are not used for cascade shadows
#define DEFERRED_NO_LIGHT_SAMPLERS
#define DEFINE_DEFERRED_LIGHT_TECHNIQUES_AND_FUNCS 1
#define	EXCLUDE_LIGHT_TECHNIQUES 1

#define SPECULAR 1
#define REFLECT 1

#include "../common.fxh"
#include "../Util/macros.fxh"

#pragma constant 130

#define SHADOW_CASTING            (0)
#define SHADOW_CASTING_TECHNIQUES (0)
#define SHADOW_RECEIVING          (1)
#define SHADOW_RECEIVING_VS       (0)
#include "../Lighting/Shadows/localshadowglobals.fxh"
#include "../Lighting/lighting.fxh"

// =============================================================================================== //
// SAMPLERS
// =============================================================================================== //

BeginSampler(sampler, standardTexture, standardSampler, standardTexture)
ContinueSampler(sampler, standardTexture, standardSampler, standardTexture)
	AddressU  = WRAP;        
	AddressV  = WRAP;
	MINFILTER = MIPLINEAR;
	MAGFILTER = LINEAR;
	MIPFILTER = LINEAR;
EndSampler;

// ----------------------------------------------------------------------------------------------- //

BeginSampler(sampler, pointTexture, pointSampler, pointTexture)
ContinueSampler(sampler, pointTexture, pointSampler, pointTexture)
	AddressU  = CLAMP;        
	AddressV  = CLAMP;
	MINFILTER = POINT;
	MAGFILTER = POINT;
	MIPFILTER = POINT;
EndSampler;

// ----------------------------------------------------------------------------------------------- //

BeginSampler(	sampler, deferredLightTexture, gDeferredLightSampler, deferredLightTexture)
ContinueSampler(sampler, deferredLightTexture, gDeferredLightSampler, deferredLightTexture)
	AddressU  = CLAMP;
	AddressV  = CLAMP;
	AddressW  = CLAMP; 
	MINFILTER = LINEAR;
	MAGFILTER = LINEAR;
	MIPFILTER = MIPLINEAR;
EndSampler;

// =============================================================================================== //
// INCLUDES
// =============================================================================================== //

#include "../Lighting/Lights/spot.fxh"
#include "../Lighting/Lights/point.fxh"
#include "../Lighting/Lights/capsule.fxh"

// =============================================================================================== //
// VARIABLES 
// =============================================================================================== //
BEGIN_RAGE_CONSTANT_BUFFER(debug_rendering_locals,b0)

float4 overrideColor0 = float4(0.0f, 0.0f, 0.0f, 0.0f);	
float4 overrideColor1 = float4(0.0f, 0.0f, 0.0f, 0.0f);	

float4 lightConstants;
float4 costConstants;

#define lightUseShading	(lightConstants.x)
#define lightShowCost	(lightConstants.y)

float4 windowParams = float4(0.0, 0.0, 0.0, 0.0f);
float3 mouseParams = float3(-1.0, -1.0, 0.0f);

float3 diffuseRangeLowerColour = float3(0.0, 0.0, 1.0);
float3 diffuseRangeMidColour = float3(0.0, 1.0, 0.0);
float3 diffuseRangeUpperColour = float3(1.0, 0.0, 0.0);

EndConstantBufferDX10(debug_rendering_locals)

// =============================================================================================== //
// DATA STRUCTURES
// =============================================================================================== //

struct pixelInputLP
{
	DECLARE_POSITION_PSIN(vPos)						// Fragment window position
};

struct vertexInputLP
{
	float3	pos					: POSITION;		// Local-space position
	float3  col					: COLOR0;
	float2	texcoord			: TEXCOORD0;
};

struct vertexOutputLP
{
	DECLARE_POSITION(pos)		// input expected in viewport space 0 to 1
	float3  colour				: COLOR0;
	float2	texcoord			: TEXCOORD0;
};

struct vertexDbgInput
{
	float3	pos					: POSITION;		// Local-space position
	float3	col					: COLOR0;
	float2	texcoord			: TEXCOORD0;
};

struct vertexDbgOutput
{
	DECLARE_POSITION(pos)		// input expected in viewport space 0 to 1
	float3 colour				: COLOR0;
	float2 texcoord				: TEXCOORD0;
	float4 eyeRay				: TEXCOORD1;	
	float4 worldPos				: TEXCOORD2;
	float4 screenPos			: TEXCOORD3;
};

struct pixelDbgInput
{
	DECLARE_POSITION_PSIN(vPos)
	float3 colour				: COLOR0;
	float2 texcoord				: TEXCOORD0;
	float4 eyeRay				: TEXCOORD1;	
	float4 worldPos				: TEXCOORD2;
	float4 screenPos			: TEXCOORD3;
#if MULTISAMPLE_TECHNIQUES
	uint sampleIndex			: SV_SampleIndex;
#endif
};

struct vertexChartOutput
{
	DECLARE_POSITION(pos)		// input expected in viewport space 0 to 1
	float3 colour				: COLOR0;
	float2 texcoord				: TEXCOORD0;
	float3 normal				: TEXCOORD1;
};

struct pixelChartInput
{
	DECLARE_POSITION_PSIN(vPos)
	float3 colour				: COLOR0;
	float2 texcoord				: TEXCOORD0;
	float3 normal				: TEXCOORD1;
};


// =============================================================================================== //
// UTILITY FUNCTIONS 
// =============================================================================================== //

float4 intersectRayCone(float3 o, float3 d, float angle, float height, float radius, float maxT)
{
	return float4(0.0, 0.0, 0.0, 0.0);
}

float GetStencil(sampler2D samp, float4 vPos)
{
#if __XENON
	float2 StencilTexCoord = vPos;
	float4 StencilData;
	asm
	{
		tfetch2D StencilData, StencilTexCoord, samp, UnnormalizedTextureCoords=true, MagFilter=point, MinFilter=point
	};
	return(StencilData.z);
#elif __PS3
	float2 StencilTexCoord;
	StencilTexCoord.xy = vPos.xy * deferredLightScreenSize.zw;
	
	float4 StencilData = tex2D(samp, StencilTexCoord.xy);
	return StencilData.w;
#else
	return -1.0f;
#endif
}

// ----------------------------------------------------------------------------------------------- //

float4 GetTexture(sampler2D samp, float2 vPos)
{
#if __XENON
	float2 TexCoord = vPos;
	float4 GBufferData;
	asm
	{
		tfetch2D GBufferData, TexCoord, samp, UnnormalizedTextureCoords=true, MagFilter=point, MinFilter=point
	};
#elif MULTISAMPLE_SUBPIXEL_OFFSET
	const int3 iPos = int3((vPos.xy) * (deferredLightScreenSize.zw),0);
	GBufferData.rgb = pointSampler__map.Load(iPos, 0).xxx;
#else
	float2 TexCoord = (vPos.xy) * (deferredLightScreenSize.zw);
	float4 GBufferData = tex2D(samp, TexCoord.xy);
#endif

	return GBufferData;
}

// ----------------------------------------------------------------------------------------------- //

vertexDbgOutput VS_fullScreenQuad_BANK_ONLY(vertexDbgInput IN)
{
	vertexDbgOutput OUT;

	float3 vpos = float3((IN.pos.xy - 0.5f) * 2.0f, 0); 

	OUT.pos	= float4(vpos, 1.0f);
	OUT.screenPos = convertToVpos(OUT.pos, deferredLightScreenSize);
	OUT.eyeRay = GetEyeRay(OUT.pos.xy);
	OUT.texcoord = IN.texcoord;
	OUT.worldPos = mul(float4(vpos, 1), gWorld);	
	OUT.worldPos.w = OUT.pos.z / OUT.pos.w;
	OUT.colour = float4(0.0f, 0.0f, 0.0f, 0.0f);

	return(OUT);
}

// =============================================================================================== //
// DEFERRED
// =============================================================================================== //

vertexOutputLP VS_screenTransform_BANK_ONLY(vertexInputLP IN)
{
    vertexOutputLP OUT;
    OUT.pos		 = float4( (IN.pos.xy-0.5f)*2.0f, 0, 1); //adjust from viewport space (0 to 1) to screen space (-1 to 1) 	
	OUT.colour	 = IN.col;
	OUT.texcoord = IN.texcoord;
	return(OUT);
}

// ----------------------------------------------------------------------------------------------- //

vertexOutputLP VS_screenTransformFlipY_BANK_ONLY(vertexInputLP IN)
{
	vertexOutputLP OUT;
	OUT.pos		 = float4( (float2(IN.pos.x,1.0f-IN.pos.y)-0.5f)*2.0f, 0, 1); //adjust from viewport space (0 to 1) to screen space (-1 to 1) 	
	OUT.colour	 = IN.col;
	OUT.texcoord = IN.texcoord;
	return(OUT);
}

// ----------------------------------------------------------------------------------------------- //

half4 PS_overrideTexture_BANK_ONLY(pixelInputLP IN) : COLOR
{
	return overrideColor0;
}

// ------------------------------------------------------------------------------- //

float4 PS_overrideTextureWithTexure_BANK_ONLY(vertexOutputLP IN) : COLOR
{
	return float4(tex2D(standardSampler, IN.texcoord.xy).rgb, 1.0f);
}

// ----------------------------------------------------------------------------------------------- //

half4 PS_showMRT_BANK_ONLY(pixelInputLP IN) : COLOR
{
	float4 GBufferData = GetTexture(pointSampler, IN.vPos);

	if (overrideColor0.a == 0) // show rgb
	{
		GBufferData.rgb = GBufferData.rgb*overrideColor0.rgb;
	}
	else if (abs(overrideColor0.a) == 1) // show alpha (if a is -1, the result will be inverted)
	{
		GBufferData.rgb = GBufferData.aaa*overrideColor0.aaa + saturate(-overrideColor0.aaa);
	}
	else if (overrideColor0.a == 2) // show normal twiddle (see UnPackGBuffer_S)
	{
		float3 twiddle = frac(GBufferData.w*float3(0.998046875f,7.984375f,63.875f));
		twiddle.xy -= float2(twiddle.y,twiddle.z)*0.125f;
		GBufferData.rgb = (twiddle + 1)/2;
	}
	else if (overrideColor0.a == 3) // show depth
	{
#if __PS3
		GBufferData.rgb = rageDepthUnpack(GBufferData).xxx;
#else
		GBufferData.rgb = GBufferData.rgb; // make depth more visible
#endif
	}

	return half4(GBufferData.rgb, 1.0);
}

// ----------------------------------------------------------------------------------------------- //

half4 PS_showDiffuseRange_BANK_ONLY(pixelInputLP IN) : COLOR
{
	float4 albedoData = GetTexture(pointSampler, IN.vPos);
	float3 linearData = albedoData.rgb * albedoData.rgb;
	float maxChannel = max(albedoData.r, max(albedoData.g, albedoData.b));

	float mult = dot(albedoData.rgb / maxChannel, 0.33333);

	float colorAvg = dot(linearData, 0.33333.xxx);

	float lowerPoint = overrideColor0.r;
	float upperPoint = overrideColor0.g;

	if (colorAvg <= lowerPoint)
	{
		albedoData.rgb = diffuseRangeLowerColour;
	}
	else if (colorAvg >= upperPoint && colorAvg <= 1.0)
	{
		albedoData.rgb = diffuseRangeUpperColour;
	}
	else if (colorAvg > 1.0)
	{
		albedoData.rgb = float3(1.0, 1.0, 1.0f);
	}
	else
	{
		albedoData.rgb = diffuseRangeMidColour;
	}

	return float4(albedoData.rgb * mult, 1.0);
}

// ----------------------------------------------------------------------------------------------- //

float4 PS_showSwatch_BANK_ONLY(vertexOutputLP IN) : COLOR
{
	return float4(sqrt(sRGBtoLinear(IN.colour)), 1.0);
}

// ----------------------------------------------------------------------------------------------- //

vertexChartOutput VS_chart_BANK_ONLY(vertexDbgInput IN)
{
	const float numX = 6;
	const float numY = 4;
	const float size = overrideColor0.x / 100.0f;
	const float spacing = overrideColor0.y / 100.0f;
	const float offset = overrideColor0.z;

	const float currentX = IN.texcoord.x;
	const float currentY = IN.texcoord.y;

	const float2 sizeChart = float2(numX, numY) * size + float2(numX - 1, numY - 1) * spacing;

	float3 normal = cross(gViewInverse[0].xyz, gViewInverse[1].xyz);

	// Position chart in front of us
	float3 newPos = gViewInverse[3].xyz;
	newPos += gViewInverse[0].xyz * IN.pos.x * (size * 0.5f);
	newPos -= gViewInverse[1].xyz * IN.pos.y * (size * 0.5f);
	
	// More than one
	newPos += gViewInverse[0].xyz * (currentX * (size + spacing));
	newPos -= gViewInverse[1].xyz * (currentY * (size + spacing));

	// Centre chart
	newPos -= gViewInverse[0] * sizeChart.x * 0.5;
	newPos += gViewInverse[1] * sizeChart.y * 0.5;

	// Offset away from camera
	float3 vpos = newPos - gViewInverse[2] * offset;

	vertexChartOutput OUT;
	OUT.pos	= mul(float4(vpos, 1), gWorldViewProj);
	OUT.colour = IN.col;
	OUT.texcoord = IN.texcoord;
	OUT.normal = normal;
	return OUT;
}

// ----------------------------------------------------------------------------------------------- //

DeferredGBuffer PS_chart_BANK_ONLY(pixelChartInput IN)
{
	DeferredGBuffer OUT;
	OUT.col0.rgba = float4(sqrt(sRGBtoLinear(IN.colour.rgb)), 0.0);
	OUT.col1.rgba = float4(IN.normal.xyz * 0.5f + 0.5f, 0.0);
	OUT.col2.rgba = float4(0.0, 0.0, 0.0, 1.0);
	OUT.col3.rgba = float4(sqrt(0.5.xx), 0.0, 0.0);
	return OUT;
}

// ----------------------------------------------------------------------------------------------- //

half4 PS_desaturateAlbedo_BANK_ONLY(pixelInputLP IN) : COLOR
{
	float4 albedoData = GetTexture(pointSampler, IN.vPos);
	const float B = dot(albedoData.rgb, 0.333333.xxx);
	
	albedoData.rgb = lerp(albedoData.rgb, float3(B,B,B), overrideColor0.r); // lerp towards desaturated
	albedoData.rgb = lerp(albedoData.rgb, float3(1,1,1), overrideColor0.g); // lerp towards white

	return albedoData;
}

// ----------------------------------------------------------------------------------------------- //
// DEFERRED TECHNIQUE
// ----------------------------------------------------------------------------------------------- //

technique deferred
{
	pass override_gbuffer_texture
	{
		VertexShader = compile VERTEXSHADER VS_screenTransform_BANK_ONLY();
		PixelShader  = compile PIXELSHADER PS_overrideTexture_BANK_ONLY()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	// ------------------------------------------------------------------------------------------- //

	pass show_gbuffer
	{
		VertexShader = compile VERTEXSHADER VS_screenTransform_BANK_ONLY();
		PixelShader  = compile PIXELSHADER PS_showMRT_BANK_ONLY()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	// ------------------------------------------------------------------------------------------- //

	pass desat_albedo
	{
		VertexShader = compile VERTEXSHADER VS_screenTransform_BANK_ONLY();
		PixelShader  = compile PIXELSHADER PS_desaturateAlbedo_BANK_ONLY()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	// ------------------------------------------------------------------------------------------- //

	pass show_diffuse_range
	{
		VertexShader = compile VERTEXSHADER VS_screenTransform_BANK_ONLY();
		PixelShader  = compile PIXELSHADER PS_showDiffuseRange_BANK_ONLY()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	// ------------------------------------------------------------------------------------------- //

	pass show_swatch
	{
		VertexShader = compile VERTEXSHADER VS_screenTransformFlipY_BANK_ONLY();
		PixelShader  = compile PIXELSHADER PS_showSwatch_BANK_ONLY()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	// ------------------------------------------------------------------------------------------- //

	pass show_chart
	{
		StencilEnable = true;
		ZEnable = true;
		ZWriteEnable = true;
		StencilPass = replace;
		StencilFunc = always;
		StencilRef = DEFERRED_MATERIAL_DEFAULT;
		CullMode=NONE;

		VertexShader = compile VERTEXSHADER VS_chart_BANK_ONLY();
		PixelShader  = compile PIXELSHADER PS_chart_BANK_ONLY()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass show_focus_chart
	{
		VertexShader = compile VERTEXSHADER VS_screenTransform_BANK_ONLY();
		PixelShader  = compile PIXELSHADER PS_overrideTextureWithTexure_BANK_ONLY()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}

// =============================================================================================== //
// LIGHTS
// =============================================================================================== //

vertexDbgOutput VS_dbgLightCommon(vertexDbgInput IN, float3 vpos)
{
	vertexDbgOutput OUT;
	OUT.pos	= mul(float4(vpos, 1), gWorldViewProj);
	OUT.screenPos = convertToVpos(OUT.pos, deferredLightScreenSize);
	OUT.eyeRay = float4(vpos - gViewInverse[3].xyz, OUT.pos.w);
	OUT.worldPos = float4(mul(float4(vpos, 1), gWorld).xyz, OUT.pos.z / OUT.pos.w);	
	OUT.texcoord = IN.texcoord;
	OUT.colour = float3(0.0f, 0.0f, 0.0f);
	return OUT;
}

// ----------------------------------------------------------------------------------------------- //

vertexDbgOutput VS_dbgSpot_BANK_ONLY(vertexDbgInput IN)
{
	lightProperties light = PopulateLightPropertiesDeferred();
	float3 vpos = pos_spotCM(IN.pos.xyz, light, false, false, false);
	return VS_dbgLightCommon(IN, vpos);
}

// ----------------------------------------------------------------------------------------------- //

vertexDbgOutput VS_dbgPoint_BANK_ONLY(vertexDbgInput IN)
{
	lightProperties light = PopulateLightPropertiesDeferred();
	float3 vpos = pos_pointCM(IN.pos.xyz, light, false, false, false);
	return VS_dbgLightCommon(IN, vpos);
}

// ----------------------------------------------------------------------------------------------- //

vertexDbgOutput VS_dbgCapsule_BANK_ONLY(vertexDbgInput IN)
{
	lightProperties light = PopulateLightPropertiesDeferred();
	float3 vpos = pos_capsule(IN.pos.xyz, light, false, false, false);
	return VS_dbgLightCommon(IN, vpos);
}

// ----------------------------------------------------------------------------------------------- //

vertexDbgOutput VS_dbgSpotShadow_BANK_ONLY(vertexDbgInput IN)
{
	lightProperties light = PopulateLightPropertiesDeferred();
	float3 vpos = pos_spotCM(IN.pos.xyz, light, true, false, false);
	return VS_dbgLightCommon(IN, vpos);
}

// ----------------------------------------------------------------------------------------------- //

vertexDbgOutput VS_dbgPointShadow_BANK_ONLY(vertexDbgInput IN)
{
	lightProperties light = PopulateLightPropertiesDeferred();
	float3 vpos = pos_pointCM(IN.pos.xyz, light, true, false, false);
	return VS_dbgLightCommon(IN, vpos);
}

// ----------------------------------------------------------------------------------------------- //

vertexDbgOutput VS_dbgCapsuleShadow_BANK_ONLY(vertexDbgInput IN)
{
	lightProperties light = PopulateLightPropertiesDeferred();
	float3 vpos = pos_capsule(IN.pos.xyz, light, true, false, false);
	return VS_dbgLightCommon(IN, vpos);
}

// ----------------------------------------------------------------------------------------------- //

half4 PS_dbgShowGeometryCapsule_BANK_ONLY(pixelDbgInput IN) : COLOR
{
	DeferredSurfaceInfo surfaceInfo = UnPackGBuffer_S(IN.screenPos.xy / IN.screenPos.w, IN.eyeRay, true, SAMPLE_INDEX);

	float4 col = overrideColor0;

	float3 approxNormal = normalize(IN.worldPos.xyz - deferredLightPosition);
	float3 surfaceToEye = normalize(gViewInverse[3].xyz - IN.worldPos.xyz);

	col.rgb *= (lightUseShading) ? max(0.0, dot(approxNormal, surfaceToEye)) : 1.0f;
	col.rgb = pow(col.rgb, 1.0 / 2.2f);

	col = (lightShowCost) ? overrideColor0 : col;

	return col;
}

// ----------------------------------------------------------------------------------------------- //

half4 PS_dbgShowGeometryPoint_BANK_ONLY(pixelDbgInput IN) : COLOR
{
	DeferredSurfaceInfo surfaceInfo = UnPackGBuffer_S(IN.screenPos.xy / IN.screenPos.w, IN.eyeRay, true, SAMPLE_INDEX);

	float4 col = overrideColor0;

	float3 approxNormal = normalize(IN.worldPos.xyz - deferredLightPosition);
	float3 surfaceToEye = normalize(gViewInverse[3].xyz - IN.worldPos.xyz);

	col.rgb *= (lightUseShading) ? max(0.0, dot(approxNormal, surfaceToEye)) : 1.0f;
	col.rgb = pow(col.rgb, 1.0 / 2.2f);

	col = (lightShowCost) ? overrideColor0 : col;

	return col;
}

// ----------------------------------------------------------------------------------------------- //

half4 PS_dbgShowGeometrySpot_BANK_ONLY(pixelDbgInput IN) : COLOR
{
	DeferredSurfaceInfo surfaceInfo = UnPackGBuffer_S(IN.screenPos.xy / IN.screenPos.w, IN.eyeRay, true, SAMPLE_INDEX);

	float4 col = overrideColor0;

	float3 approxNormal = normalize(IN.worldPos.xyz - deferredLightPosition);
	float3 surfaceToEye = normalize(gViewInverse[3].xyz - IN.worldPos.xyz);

	col.rgb *= (lightUseShading) ? max(0.0, dot(approxNormal, surfaceToEye)) : 1.0f;
	col.rgb = pow(col.rgb, 1.0 / 2.2f);

	col = (lightShowCost) ? overrideColor0 : col;

	return col;
}

// =============================================================================================== //
// LIGHTS TECHNIQUE
// =============================================================================================== //

technique MSAA_NAME(light)
{
	pass MSAA_NAME(pass_point)
	{
		VertexShader = compile VERTEXSHADER VS_dbgPoint_BANK_ONLY();
		PixelShader  = compile MSAA_PIXEL_SHADER PS_dbgShowGeometryPoint_BANK_ONLY()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	// ------------------------------------------------------------------------------------------- //

	pass MSAA_NAME(pass_spot)
	{
		VertexShader = compile VERTEXSHADER VS_dbgSpot_BANK_ONLY();
		PixelShader  = compile MSAA_PIXEL_SHADER PS_dbgShowGeometrySpot_BANK_ONLY()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
	
	// ------------------------------------------------------------------------------------------- //

	pass MSAA_NAME(pass_capsule)
	{
		VertexShader = compile VERTEXSHADER VS_dbgCapsule_BANK_ONLY();
		PixelShader  = compile MSAA_PIXEL_SHADER PS_dbgShowGeometryCapsule_BANK_ONLY()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	// ------------------------------------------------------------------------------------------- //

	pass MSAA_NAME(pass_point_shadow)
	{
		VertexShader = compile VERTEXSHADER VS_dbgPointShadow_BANK_ONLY();
		PixelShader  = compile MSAA_PIXEL_SHADER PS_dbgShowGeometryPoint_BANK_ONLY()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	// ------------------------------------------------------------------------------------------- //

	pass MSAA_NAME(pass_spot_shadow)
	{
		VertexShader = compile VERTEXSHADER VS_dbgSpotShadow_BANK_ONLY();
		PixelShader  = compile MSAA_PIXEL_SHADER PS_dbgShowGeometrySpot_BANK_ONLY()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	// ------------------------------------------------------------------------------------------- //

	pass MSAA_NAME(pass_capsule_shadow)
	{
		VertexShader = compile VERTEXSHADER VS_dbgCapsuleShadow_BANK_ONLY();
		PixelShader  = compile MSAA_PIXEL_SHADER PS_dbgShowGeometryCapsule_BANK_ONLY()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}

// =============================================================================================== //
// COST
// =============================================================================================== //

half4 PS_show_cost_BANK_ONLY(pixelInputLP IN) : COLOR
{
	const float overdrawMin = costConstants.x;
	const float overdrawMax = costConstants.y;
	const float overdraw = GetTexture(pointSampler, IN.vPos).x*255.0f - overdrawMin;

	rageDiscard(overdraw < -0.001f); // compensate for floating-point inaccuracy

	const float3 colour0 = float3(0.0f, 0.0f, 1.0f); // blue
	const float3 colour1 = float3(0.0f, 1.0f, 0.0f); // green
	const float3 colour2 = float3(1.0f, 1.0f, 0.0f); // yellow
	const float3 colour3 = float3(1.0f, 0.0f, 0.0f); // red

	float3 result = 0;

	if (overdrawMax > overdrawMin)
	{
		const float4 boundary = float4(0.0f, 1.0f, 2.0f, 3.0f)*(overdrawMax - overdrawMin)/3.0f;
		const float3 dividers = boundary.yyy;

		if      (overdraw < boundary.y) { result = lerp(colour0, colour1, (overdraw - boundary.x)/dividers.x); }
		else if (overdraw < boundary.z) { result = lerp(colour1, colour2, (overdraw - boundary.y)/dividers.y); }
		else if (overdraw < boundary.w) { result = lerp(colour2, colour3, (overdraw - boundary.z)/dividers.z); }
		else	                        { result = colour3; }
	}
	else
	{
		result = colour3;
	}

	return float4(result, costConstants.w);
}

// ----------------------------------------------------------------------------------------------- //

technique cost
{
	pass overdraw
	{
		VertexShader = compile VERTEXSHADER VS_fullScreenQuad_BANK_ONLY();
		PixelShader  = compile PIXELSHADER PS_show_cost_BANK_ONLY() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}

// =============================================================================================== //
// DEBUG 
// =============================================================================================== //

half4 PS_graph_BANK_ONLY(pixelDbgInput IN) : COLOR
{
	const float2 mousePos = mouseParams.xy;
	const float2 mouseTrans = (mousePos - 0.5f) * 2.0f; 
	const float4 mouseRay = GetEyeRay(mouseTrans.xy);

	DeferredSurfaceInfo mouseSurfaceInfo = UnPackGBuffer_S(mousePos, mouseRay, true, SAMPLE_INDEX);

	// Light variables
	float3 surfaceToLight = deferredLightPosition - mouseSurfaceInfo.positionWorld;
	float surfaceToLightDistSqr = dot(surfaceToLight, surfaceToLight);
	const float3 surfaceToLightDir = normalize(surfaceToLight);
	const float3 surfaceToEyeDir = normalize(gViewInverse[3].xyz - mouseSurfaceInfo.positionWorld);
	const float3 lightToWorldDir = deferredLightDirection;	

	float distSqr = IN.texcoord.x;

	float distAtten = __powapprox(1.0 - IN.texcoord.x, deferredLightFalloffExponent);
	float final = distAtten;

	float2 position = float2(IN.vPos.x, 720 - IN.vPos.y) - windowParams.xy;

	// Flip graph
	position.y = windowParams.w - position.y;
	float transFinal = final * windowParams.w;

	float val = abs(transFinal - position.y);
	float intensity = (val < 2.0) ? 1.0 - saturate(val - 1.0) : 0.0;

	float3 col = (transFinal < 1.0) ? float3(0.0, 0.0, 1.0) : float3(1.0 - final, final, 0.0);

	if (abs((dot(surfaceToLight, surfaceToLight)) - distAtten) < 4.0)
		intensity = 1.0;

	return half4(col * intensity, 1.0);
}

// =============================================================================================== //
// DEBUG TECHNIQUE
// =============================================================================================== //

#if !defined(SHADER_FINAL)

technique MSAA_NAME(debug)
{
	pass MSAA_NAME(graph)
	{
		CullMode = NONE;

		ZEnable = false;
		ZWriteEnable = false;		
		ZFunc = always;		

		VertexShader = compile VERTEXSHADER VS_fullScreenQuad_BANK_ONLY();
		PixelShader  = compile MSAA_PIXEL_SHADER PS_graph_BANK_ONLY()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}

#endif // !defined(SHADER_FINAL)
