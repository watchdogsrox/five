//
//
// 2 layers + height map
//
#define SPECULAR 1
#define NORMAL_MAP 1
//#define USE_PALETTE_TINT
#define NO_SKINNING
#include "../common.fxh"

#ifndef PRAGMA_DCL
	#pragma dcl position diffuse texcoord0 texcoord1 normal tangent0
#endif

BEGIN_RAGE_CONSTANT_BUFFER(blend_2lyr_locals,b0)
float specularFalloffMult : Specular
<
	string UIName = "Specular Falloff";
	float UIMin = 0.0;
	float UIMax = 2000.0;
	float UIStep = 0.1;
> = 32.0;

float specularIntensityMult : SpecularColor
<
	string UIName = "Specular Intensity";
	float UIMin = 0.0;
	float UIMax = 1.0;
	float UIStep = 0.01;
> = 0.0;

float bumpiness : Bumpiness
<
	string UIWidget = "slider";
	float UIMin = 0.0;
	float UIMax = 200.0;
	float UIStep = .01;
	string UIName = "Bumpiness";
> = 1.0;

EndConstantBufferDX10( blend_2lyr_locals )



#define SHADOW_CASTING            (1)
#define SHADOW_CASTING_TECHNIQUES (1 && SHADOW_CASTING_TECHNIQUES_OK)
#define SHADOW_RECEIVING          (0)
#define SHADOW_RECEIVING_VS       (0)
#include "../Lighting/Shadows/localshadowglobals.fxh"
#include "../Lighting/Shadows/cascadeshadows.fxh"
#include "../Lighting/forward_lighting.fxh"
#include "../Lighting/lighting.fxh"
#include "../Debug/EntitySelect.fxh"

BeginSampler(	sampler, diffuseTexture_layer0, TextureSampler_layer0, DiffuseTexture_layer0)
	string UIName = "Diffuse 1";
	string TCPTemplate = "Diffuse";
	string TextureType="Diffuse";
ContinueSampler(sampler, diffuseTexture_layer0, TextureSampler_layer0, DiffuseTexture_layer0)
	AddressU  = WRAP;
	AddressV  = WRAP;
	AddressW  = WRAP;
	MIPFILTER = MIPLINEAR;
	MINFILTER = HQLINEAR;
	MAGFILTER = MAGLINEAR;
EndSampler;

BeginSampler(sampler2D,bumpTexture_layer0,BumpSampler_layer0,BumpTexture_layer0)
	string UIName="Bump 1";
	string UIHint="NORMAL_MAP";
	string TCPTemplate="NormalMap";
	string TextureType="Bump";
	
ContinueSampler(sampler2D,bumpTexture_layer0,BumpSampler_layer0,BumpTexture_layer0)
	AddressU  = WRAP;
	AddressV  = WRAP;
	AddressW  = WRAP;
	MIPFILTER = MIPLINEAR;
	MINFILTER = HQLINEAR;
	MAGFILTER = MAGLINEAR;
EndSampler;

BeginSampler(	sampler, diffuseTexture_layer1, TextureSampler_layer1, DiffuseTexture_layer1)
	string UIName = "Diffuse 2";
	string TCPTemplate = "Diffuse";
	string TextureType="Diffuse";
	
ContinueSampler(sampler, diffuseTexture_layer1, TextureSampler_layer1, DiffuseTexture_layer1)
	AddressU  = WRAP;
	AddressV  = WRAP;
	AddressW  = WRAP;
	MIPFILTER = MIPLINEAR;
	MINFILTER = HQLINEAR;
	MAGFILTER = MAGLINEAR;
EndSampler;

BeginSampler(sampler2D,bumpTexture_layer1,BumpSampler_layer1,BumpTexture_layer1)
	string UIName="Bump 2";
	string UIHint="NORMAL_MAP";
	string TCPTemplate="NormalMap";
	string TextureType="Bump";
	
ContinueSampler(sampler2D,bumpTexture_layer1,BumpSampler_layer1,BumpTexture_layer1)
	AddressU  = WRAP;
	AddressV  = WRAP;
	AddressW  = WRAP;
	MIPFILTER = MIPLINEAR;
	MINFILTER = HQLINEAR;
	MAGFILTER = MAGLINEAR;
EndSampler;


BeginSampler(	sampler, lookupTexture, lookupSampler, lookupTexture)
	string UIName = "Lookup texture";
	string TCPTemplate = "Diffuse";
	string TextureType="Lookup";

ContinueSampler(sampler, lookupTexture, lookupSampler, lookupTexture)
	AddressU  = WRAP;
	AddressV  = WRAP;
	AddressW  = WRAP;
	MIPFILTER = MIPLINEAR;
	MINFILTER = LINEAR;
	MAGFILTER = LINEAR;
EndSampler;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//
//
//
struct vertexInput
{
	float3 pos			: POSITION;
	float4 diffuse		: COLOR0;
	float2 texCoord0	: TEXCOORD0;
	float2 texCoord1	: TEXCOORD1;
	float3 normal		: NORMAL;
	float4 tangent		: TANGENT0;
};

struct vertexInputSimple
{
	float3 pos			: POSITION;
	float4 diffuse		: COLOR0;
	float2 texCoord0	: TEXCOORD0;
	float2 texCoord1	: TEXCOORD1;
	float3 normal		: NORMAL;
};

struct vertexInputUnlit
{
	float3 pos			: POSITION;
	float4 diffuse		: COLOR0;
	float2 texCoord0	: TEXCOORD0;
	float2 texCoord1	: TEXCOORD1;
	float3 normal		: NORMAL;
};

struct vertexOutputDeferred
{
	DECLARE_POSITION(pos)
	float4 texCoord			: TEXCOORD0;
#if PALETTE_TINT
	float4 tintColor		: COLOR1;
#endif
	float3 worldNormal		: TEXCOORD1;
	float2 color			: TEXCOORD2;
	float3 worldTangent		: TEXCOORD3;
	float3 worldBinormal	: TEXCOORD4;
};

struct vertexOutput
{
	DECLARE_POSITION(pos)
	float4 texCoord			: TEXCOORD0;
#if PALETTE_TINT
	float4 tintColor		: COLOR1;
#endif
	float3 worldPos			: TEXCOORD1;
	float3 worldNormal		: TEXCOORD2;
	float4 color			: TEXCOORD3;
	float3 worldTangent		: TEXCOORD4;
	float3 worldBinormal	: TEXCOORD5;
};

struct vertexOutputUnlit
{
	DECLARE_POSITION(pos)
	float4 texCoord			: TEXCOORD0;
#if PALETTE_TINT
	float4 tintColor		: COLOR1;
#endif
	float3 worldNormal		: TEXCOORD2;
	float3 worldPos			: TEXCOORD3;
};

struct vertexOutputSimple
{
	DECLARE_POSITION(pos)
	float4 texCoord			: TEXCOORD0;
#if PALETTE_TINT
	float4 tintColor		: COLOR1;
#endif
	float4 color			: TEXCOORD1;
	float3 worldNormal		: TEXCOORD2;
	float3 worldPos			: TEXCOORD3;
};

float3 ProcessSimpleLighting(float3 worldPos, float3 worldNormal, float3 vertColor)
{
	SurfaceProperties surfaceInfo = (SurfaceProperties)0;

	// Setup some default properties so we can use the lighting functions
	surfaceInfo.surface_worldNormal = worldNormal;
	surfaceInfo.surface_baseColor = float4(1.0, 1.0, 1.0, 1.0);
	surfaceInfo.surface_diffuseColor = float4(vertColor, 1.0);
	#if SPECULAR
		surfaceInfo.surface_specularIntensity = specularIntensityMult;
		surfaceInfo.surface_specularExponent = specularFalloffMult;
		surfaceInfo.surface_fresnel = 0.98;
	#endif	// SPECULAR
	#if REFLECT
		surfaceInfo.surface_reflectionColor = float3(0.0, 0.0, 0.0);
	#endif // REFLECT
	surfaceInfo.surface_emissiveIntensity = 0.0f;
	#if SELF_SHADOW
		surfaceInfo.surface_selfShadow = 1.0f;
	#endif

	StandardLightingProperties surfaceStandardLightingProperties =
		DeriveLightingPropertiesForCommonSurface(surfaceInfo);

	surfaceStandardLightingProperties.diffuseColor = ProcessDiffuseColor(surfaceStandardLightingProperties.diffuseColor);

	surfaceProperties surface;
	directionalLightProperties light;
	materialProperties material;

	populateForwardLightingStructs(
		surface,
		material,
		light,
		worldPos.xyz,
		surfaceInfo.surface_worldNormal.xyz,
		surfaceStandardLightingProperties);

	#if __WIN32PC_MAX_RAGEVIEWER
		int numLights = 0;
	#else
		int numLights = 8;
	#endif

	return calculateForwardLighting(
		numLights,
		true,
		surface,
		material,
		light,
		true, // directional
			false, // directionalShadow
			false, // directionalShadowHighQuality
		float2(0.0, 0.0)).rgb;
}

#if __MAX
float4 MaxProcessSimpleLighting(float3 worldPos, float3 worldNormal, float4 vertColor, float4 diffuseColor, float surfaceSpecularExponent, float surfaceSpecularIntensity)
{
	SurfaceProperties surfaceInfo;
	surfaceInfo.surface_worldNormal			= worldNormal;
	surfaceInfo.surface_baseColor			= vertColor;
	surfaceInfo.surface_diffuseColor		= diffuseColor;
	surfaceInfo.surface_specularIntensity	= surfaceSpecularIntensity;
	surfaceInfo.surface_specularExponent	= surfaceSpecularExponent;
	surfaceInfo.surface_specularSkin		= 0.0f;
	surfaceInfo.surface_fresnel				= 0.0f;

	// Compute some surface information.
	StandardLightingProperties surfaceStandardLightingProperties =
		DeriveLightingPropertiesForCommonSurface( surfaceInfo );

	surfaceProperties surface;
	directionalLightProperties light;
	materialProperties material;

	populateForwardLightingStructs(
		surface,
		material,
		light,
		worldPos.xyz,
		surfaceInfo.surface_worldNormal.xyz,
		surfaceStandardLightingProperties);

	float4 OUT = calculateForwardLighting(
		0, // numLights
		false,
		surface,
		material,
		light,
		true, // directional
			false, // directionalShadow
			false, // directionalShadowHighQuality
		float2(0.0, 0.0));

	return float4(sqrt(OUT.rgb), OUT.a); //gamma correction
}
#endif //__MAX...


float ApplyHeightMapWeights(float alpha, float height1, float control)
{
	const float sharpeningOffset = 0.5f;
	control = control*1.5f;
	height1 = saturate((((height1)*(sharpeningOffset+control)*2.0f-(sharpeningOffset+control))+sharpeningOffset)/(sharpeningOffset*2.0f));

	return saturate( alpha + height1*alpha);	
}


float4 BlendColor_2Layer(float2 texCoord0, float alpha, float control)
{
	float4 tex0 = tex2D(TextureSampler_layer0,texCoord0.xy);
	float4 tex1 = tex2D(TextureSampler_layer1,texCoord0.xy);

	alpha = ApplyHeightMapWeights(alpha, tex0.a, control);
	
	float4 res = lerp(tex0,tex1,alpha);
	
	return res;
}


float3 BlendNormal_2Layer(float2 texCoord0, float alpha, float control)
{
	float4 tex0 = tex2D(TextureSampler_layer0,texCoord0.xy);
	float4 tex1 = tex2D(TextureSampler_layer1,texCoord0.xy);
	float3 norm0 = tex2D_NormalMap(BumpSampler_layer0,texCoord0.xy);
	float3 norm1 = tex2D_NormalMap(BumpSampler_layer1,texCoord0.xy);

	alpha = ApplyHeightMapWeights(alpha, tex0.a, tex1.a);
	
	float3 res = lerp(norm0,norm1,alpha);
	
	return res;
}


struct comboTexel 
{
	float3 color;
	float3 normal;
};


comboTexel GetTexel(float4 texCoord, float3 worldNormal, float3 worldBinormal, float3 worldTangent)
{
	comboTexel result;

	float4 lookup = h4tex2D(lookupSampler,texCoord.zw).rgba;
	
	result.color = BlendColor_2Layer(texCoord.xy,lookup.r,lookup.g).rgb;
	result.normal = CalculateWorldNormal(BlendNormal_2Layer(texCoord.xy,lookup.r,lookup.g).xy, bumpiness, worldTangent, worldBinormal, worldNormal);

	return result;
}


comboTexel GetTexel(float4 texCoord, float3 worldNormal)
{
	comboTexel result;

	float4 lookup = h4tex2D(lookupSampler,texCoord.zw).rgba;
	
	result.color = BlendColor_2Layer(texCoord.xy,lookup.r,lookup.g).rgb;
	result.normal = worldNormal; // normalize?	

	return result;
}


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

	OUT.texCoord		= float4(IN.texCoord0,IN.texCoord1);

#if PALETTE_TINT
	#if PALETTE_TINT_EDGE
		OUT.tintColor	= UnpackTintPalette(IN.specular);
	#else
		OUT.tintColor	= UnpackTintPalette(IN.diffuse);
	#endif
#endif
	OUT.color			= IN.diffuse;

	OUT.worldTangent	= NORMALIZE_VS(mul(IN.tangent.xyz, (float3x3)gWorld));
	OUT.worldBinormal	= rageComputeBinormal(OUT.worldNormal.xyz, OUT.worldTangent, IN.tangent.w);
	
	return(OUT);
}


vertexOutputDeferred	VS_TransformDeferred(vertexInput IN)
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

	OUT.texCoord		= float4(IN.texCoord0,IN.texCoord1);
#if PALETTE_TINT
	#if PALETTE_TINT_EDGE
		OUT.tintColor	= UnpackTintPalette(IN.specular);
	#else
		OUT.tintColor	= UnpackTintPalette(IN.diffuse);
	#endif
#endif

	OUT.worldNormal		= worldNormal;

	OUT.worldTangent	= NORMALIZE_VS(mul(IN.tangent.xyz, (float3x3)gWorld));
	OUT.worldBinormal	= rageComputeBinormal(OUT.worldNormal.xyz, OUT.worldTangent, IN.tangent.w);

	OUT.color			= IN.diffuse.rgba;

	return(OUT);
}


vertexOutputUnlit	VS_TransformUnlit(vertexInputUnlit IN)
{
	vertexOutputUnlit OUT;

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

	OUT.texCoord		= float4(IN.texCoord0,IN.texCoord1);

#if PALETTE_TINT
	#if PALETTE_TINT_EDGE
		OUT.tintColor	= UnpackTintPalette(IN.specular);
	#else
		OUT.tintColor	= UnpackTintPalette(IN.diffuse);
	#endif
#endif

	return(OUT);
}


#if __MAX
vertexOutput VS_MaxTransform(maxVertexInput IN)
{
vertexInput input;

	input.pos		= IN.pos;
	input.diffuse	= IN.diffuse;

	input.texCoord0	= Max2RageTexCoord2(IN.texCoord0.xy);
	input.texCoord1 = Max2RageTexCoord2(IN.texCoord1.xy);

	input.normal	= IN.normal;
	input.tangent	= float4(IN.tangent,1.0f);
	
	return VS_Transform(input);
}
#endif //__MAX...

half4	PS_TexturedUnlit(vertexOutputUnlit IN) : COLOR
{
	comboTexel texel = GetTexel(float4(IN.texCoord.xyzw), float3(IN.worldNormal.xyz));


	float3 blendColor = texel.color;
#if PALETTE_TINT
	blendColor.rgb *= IN.tintColor.rgb;
#endif
	float4 pixelColor = float4(blendColor,globalAlpha);

	pixelColor = calcDepthFxBasic(pixelColor, IN.worldPos);
	
	return(pixelColor);
}


half4	PS_Textured(vertexOutput IN) : COLOR
{
	float4 color = IN.color;
	color.a *= globalAlpha;
	
	comboTexel texel = GetTexel(float4(IN.texCoord.xyzw), float3(IN.worldNormal.xyz), float3(IN.worldBinormal.xyz), float3(IN.worldTangent.xyz));
	
#if PALETTE_TINT
	texel.color.rgb *= IN.tintColor.rgb;
#endif


#if __MAX
	float4 pixelColor = MaxProcessSimpleLighting(IN.worldPos.xyz, texel.normal, color, float4(texel.color.rgb,1.0f), specularFalloffMult, specularIntensityMult 
	);
#else //__MAX
	color.rgb = ProcessSimpleLighting(IN.worldPos.xyz, texel.normal, color.rgb );
	float4 pixelColor = color * float4(texel.color,1.0f);
#endif //__MAX...
	
	pixelColor = calcDepthFxBasic(pixelColor, IN.worldPos);
	
	return(PackColor(pixelColor));
}


#if __MAX
float4 PS_MaxTextured(vertexOutput IN) : COLOR
{
	float4 OUT;

	if(maxLightingToggle)
	{
		OUT = PS_Textured(IN);
	}
	else
	{
		vertexOutputUnlit unlitIN;
			unlitIN.pos			= IN.pos;
			unlitIN.texCoord	= IN.texCoord;
			unlitIN.worldNormal	= IN.worldNormal;
			unlitIN.worldPos	= (float3)0;
		OUT = PS_TexturedUnlit(unlitIN);
	}

	OUT.a = 1.0f;
	return(saturate(OUT));
}
#endif //__MAX...

DeferredGBuffer	PS_TexturedDeferred(vertexOutputDeferred IN)
{
	// Compute some surface information.
	StandardLightingProperties surfLightingProperties;

	comboTexel texel = GetTexel(float4(IN.texCoord.xyzw), float3(IN.worldNormal.xyz), float3(IN.worldBinormal.xyz), float3(IN.worldTangent.xyz));

	surfLightingProperties.diffuseColor	= float4(texel.color.rgb, 1.0f);
#if PALETTE_TINT
	surfLightingProperties.diffuseColor.rgb	*= IN.tintColor.rgb;
#endif

	surfLightingProperties.naturalAmbientScale	= IN.color.r;
	surfLightingProperties.artificialAmbientScale= IN.color.g;

#if SELF_SHADOW
	surfLightingProperties.selfShadow = 1.0f;
#endif
#if USE_SLOPE
	surfLightingProperties.slope = 0.0f;
#endif
 
#if SPECULAR
	surfLightingProperties.specularSkin = 0.0;
	surfLightingProperties.fresnel = 0.0f;
	surfLightingProperties.specularExponent = specularFalloffMult;
	surfLightingProperties.specularIntensity= specularIntensityMult;
	surfLightingProperties.reflectionIntensity = 1.0f;
#endif

	surfLightingProperties.inInterior = gInInterior;

 	// Pack the information into the GBuffer(s).
	DeferredGBuffer OUT = PackGBuffer( texel.normal,surfLightingProperties );
	
	return(OUT);
}



#if __MAX
technique tool_draw
{
	pass p0
	{
		MAX_TOOL_TECHNIQUE_RS
		AlphaBlendEnable= false;
		AlphaTestEnable	= false;

		VertexShader	= compile VERTEXSHADER	VS_MaxTransform();
		PixelShader		= compile PIXELSHADER	PS_MaxTextured();
	}
}
#else // __MAX

#if DEFERRED_TECHNIQUES && NON_CUTOUT_DEFERRED_TECHNIQUES
technique deferred_draw
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER	VS_TransformDeferred();
		PixelShader  = compile PIXELSHADER	PS_TexturedDeferred()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
#endif // DEFERRED_TECHNIQUES && NON_CUTOUT_DEFERRED_TECHNIQUES

#if DEFERRED_TECHNIQUES && NON_CUTOUT_DEFERRED_TECHNIQUES && DRAWSKINNED_TECHNIQUES
technique deferred_drawskinned
{
	pass p0
	{
		// reuse non-skinned VS here:
		VertexShader = compile VERTEXSHADER	VS_TransformDeferred();
		PixelShader  = compile PIXELSHADER	PS_TexturedDeferred()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
#endif // DEFERRED_TECHNIQUES && NON_CUTOUT_DEFERRED_TECHNIQUES && DRAWSKINNED_TECHNIQUES

#if FORWARD_BLEND2LYR_TECHNIQUES
technique draw
{
	pass p0 
	{
		VertexShader = compile VERTEXSHADER	VS_Transform();
		PixelShader  = compile PIXELSHADER	PS_Textured()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
#endif // FORWARD_BLEND2LYR_TECHNIQUES

#if FORWARD_BLEND2LYR_TECHNIQUES && DRAWSKINNED_TECHNIQUES
technique drawskinned
{
	pass p0
	{
		// reuse non-skinned VS here:
		VertexShader = compile VERTEXSHADER	VS_Transform();
		PixelShader  = compile PIXELSHADER	PS_Textured()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
#endif // FORWARD_BLEND2LYR_TECHNIQUES && DRAWSKINNED_TECHNIQUES

#if UNLIT_TECHNIQUES
technique unlit_draw
{
	pass p0
	{
		// reuse lit version of VS here:
		VertexShader = compile VERTEXSHADER	VS_TransformUnlit();
		PixelShader  = compile PIXELSHADER	PS_TexturedUnlit()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
#endif // UNLIT_TECHNIQUES

#if UNLIT_TECHNIQUES && DRAWSKINNED_TECHNIQUES
technique unlit_drawskinned
{
	pass p0
	{
		// reuse lit and non-skinned version of VS here:
		VertexShader = compile VERTEXSHADER	VS_TransformUnlit();
		PixelShader  = compile PIXELSHADER	PS_TexturedUnlit()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
#endif // UNLIT_TECHNIQUES && DRAWSKINNED_TECHNIQUES

SHADERTECHNIQUE_CASCADE_SHADOWS()
SHADERTECHNIQUE_LOCAL_SHADOWS(VS_LinearDepth, VS_LinearDepthSkin, PS_LinearDepth)
SHADERTECHNIQUE_ENTITY_SELECT(VS_Transform(), VS_Transform())
#include "../Debug/debug_overlay_blend_2lyr.fxh"
#endif // __MAX
