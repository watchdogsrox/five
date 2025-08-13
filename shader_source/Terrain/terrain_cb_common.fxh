//
// GTA terrain Combo shader (up to 8 layers using a lookup texture, multitexture):
//
//
//
#ifndef __GTA_TERRAIN_CB_COMMON_FXH__
#define __GTA_TERRAIN_CB_COMMON_FXH__

#define IS_TERRAIN_SHADER

#define USE_WETNESS_MULTIPLIER

#if __PS3
#define HALF_PRECISION_LIGHT_STRUCTS
#endif

#define NO_SKINNING
#undef  USE_DIFFUSE_SAMPLER

#include "../common.fxh"
#include "terrain_cb_tessellation_funcs.fxh"
#include "../Util/GSInst.fxh"

#ifndef SPECULAR
	#define SPECULAR 0
#endif // SPECULAR

#ifndef SPECULAR_MAP
	#define SPECULAR_MAP 0
#endif //  SPECULAR_MAP

#if SPECULAR_MAP
	// integrity check: SPECULAR_MAP=1 allowed only when SPECULAR=1!
	#if !SPECULAR
		#error "SPECULAR_MAP=1 only with SPECULAR=1!"
	#endif
#endif

#ifndef NORMAL_MAP
	#define NORMAL_MAP 0
#endif // NORMAL_MAP

#ifndef PARALLAX_MAP
	#define PARALLAX_MAP 0
#endif // PARALLAX_MAP

#ifndef USE_EDGE_WEIGHT
	#define USE_EDGE_WEIGHT  0
#endif

#if ((__SHADERMODEL < 40) && !__MAX && !__WIN32PC) || __LOW_QUALITY

#undef USE_EDGE_WEIGHT
#define USE_EDGE_WEIGHT 0

#undef PARALLAX_MAP
#define PARALLAX_MAP 0

#endif // (__SHADERMODEL < 40)

#if PARALLAX_MAP
	// integrity check: PARALLAX_MAP=1 allowed only when NORMAL_MAP=1!
	#if !NORMAL_MAP
		#error "PARALLAX_MAP=1 only with NORMAL_MAP=1!"
	#endif
#endif

#ifndef DOUBLEUV_SET
	#define DOUBLEUV_SET 0
#endif // DOUBLEUV_SET

#ifndef USE_TEXTURE_LOOKUP
	#define USE_TEXTURE_LOOKUP 0
#endif // USE_TEXTURE_LOOKUP

#ifndef LAYER_COUNT 
	#define LAYER_COUNT 4
#endif // LAYER_COUNT

#ifndef USE_DOUBLE_LOOKUP
	#define USE_DOUBLE_LOOKUP 0
#endif // USE_TEXTURE_LOOKUP

#ifndef USE_DIFFLUM_ON_SPECINT
	#define USE_DIFFLUM_ON_SPECINT 0
#endif // USE_DIFFLUM_ON_SPECINT

#if USE_DOUBLE_LOOKUP
	#undef USE_TEXTURE_LOOKUP
	#define USE_TEXTURE_LOOKUP 1
#endif // USE_DOUBLE_LOOKUP

#if USE_TEXTURE_LOOKUP
	#undef DOUBLEUV_SET
	#define DOUBLEUV_SET 1
#endif // USE_TEXTURE_LOOKUP

#ifdef USE_WETNESS_MULTIPLIER
	#define WETNESS_MULTIPLIER	1
#else
	#define WETNESS_MULTIPLIER	0
#endif

#ifdef USE_PER_MATERIAL_WETNESS_MULTIPLIER
	#define PER_MATERIAL_WETNESS_MULTIPLIER 1
	#undef WETNESS_MULTIPLIER
	#define WETNESS_MULTIPLIER 1
#else
	#define PER_MATERIAL_WETNESS_MULTIPLIER 0
#endif	

#ifdef USE_IN_INTERIOR
	#define DEFERRED_STENCIL_REF DEFERRED_MATERIAL_INTERIOR_TERRAIN
#else
	#define DEFERRED_STENCIL_REF DEFERRED_MATERIAL_TERRAIN
#endif

#define SELF_SHADOW		1

#if LAYER_COUNT == 4
#define LAYER0_TEXCOORD	texCoord0
#define LAYER1_TEXCOORD	texCoord1
#define LAYER2_TEXCOORD	texCoord1
#define LAYER3_TEXCOORD	texCoord1
#define LAYER4_TEXCOORD	texCoord1
#define LAYER5_TEXCOORD texCoord0
#define LAYER6_TEXCOORD texCoord0
#define LAYER7_TEXCOORD texCoord0

#elif LAYER_COUNT == 8

#define LAYER0_TEXCOORD	texCoord0
#define LAYER1_TEXCOORD	texCoord0
#define LAYER2_TEXCOORD	texCoord0
#define LAYER3_TEXCOORD	texCoord1
#define LAYER4_TEXCOORD	texCoord1
#define LAYER5_TEXCOORD texCoord1
#define LAYER6_TEXCOORD texCoord1
#define LAYER7_TEXCOORD texCoord1

#else

#define LAYER0_TEXCOORD	texCoord0
#define LAYER1_TEXCOORD	texCoord1
#define LAYER2_TEXCOORD	texCoord0
#define LAYER3_TEXCOORD	texCoord1
#define LAYER4_TEXCOORD	texCoord0
#define LAYER5_TEXCOORD texCoord1
#define LAYER6_TEXCOORD texCoord0
#define LAYER7_TEXCOORD texCoord1

#endif
#ifndef TINT_TEXTURE
	#define TINT_TEXTURE 0
#endif

#ifndef TINT_TEXTURE_NORMAL
	#define TINT_TEXTURE_NORMAL 0
#endif

#if TINT_TEXTURE_NORMAL
	// integrity check: TINT_TEXTURE_NORMAL=1 allowed only when TINT_TEXTURE=1 and NORMAL_MAP=1!
	#if !TINT_TEXTURE || !NORMAL_MAP
		#error "TINT_TEXTURE_NORMAL=1 only with TINT_TEXTURE=1 and NORMAL_MAP=1"
	#endif
#endif

#ifndef TINT_TEXTURE_DISTANCE_BLEND
	#define TINT_TEXTURE_DISTANCE_BLEND 0
#endif

#if TINT_TEXTURE_DISTANCE_BLEND
	// integrity check: TINT_TEXTURE_DISTANCE_BLEND=1 allowed only when TINT_TEXTURE=1!
	#if !TINT_TEXTURE 
		#error "TINT_TEXTURE_DISTANCE_BLEND=1 only with TINT_TEXTURE=1"
	#endif
#endif

// integrity check: PALETTE_TINT allowed only when USE_TEXTURE_LOOKUP=1
#if PALETTE_TINT
	#if !USE_TEXTURE_LOOKUP
		#error "USE_PALETTE_TINT works only with USE_TEXTURE_LOOKUP=1!"
	#endif
#endif

#ifndef USE_RDR_STYLE_BLENDING
	#define USE_RDR_STYLE_BLENDING 0
#endif

#if USE_RDR_STYLE_BLENDING
	#if USE_TEXTURE_LOOKUP || USE_DOUBLE_LOOKUP
		#error "USE_RDR_STYLE_BLENDING can't be use with USE_TEXTURE_LOOKUP or USE_DOUBLE_LOOKUP"
	#endif

	#define SEPERATE_TINT_TEXTURE_COORDS 0 //we don't want to use a seperate tint texture coord
#else
	#define SEPERATE_TINT_TEXTURE_COORDS TINT_TEXTURE
#endif

#ifndef PRAGMA_DCL

#if NORMAL_MAP
	#if USE_TEXTURE_LOOKUP
		#if PALETTE_TINT
			#pragma dcl position diffuse texcoord0 texcoord1 normal tangent0
		#else // PALETTE_TINT
			#if USE_DOUBLE_LOOKUP
				#pragma dcl position diffuse specular texcoord0 texcoord1 normal tangent0
			#else
				#pragma dcl position diffuse texcoord0 texcoord1 normal tangent0
			#endif
		#endif
	#else // USE_TEXTURE_LOOKUP
		#if DOUBLEUV_SET
			#pragma dcl position diffuse specular texcoord0 texcoord1 normal tangent0
		#else // DOUBLEUV_SET
			#pragma dcl position diffuse specular texcoord0 normal tangent0
		#endif // DOUBLEUV_SET
	#endif // USE_TEXTURE_LOOKUP
#else // NORMAL_MAP
	#if USE_TEXTURE_LOOKUP
		#if PALETTE_TINT
			#pragma dcl position diffuse texcoord0 texcoord1 normal
		#else // PALETTE_TINT
			#if USE_DOUBLE_LOOKUP
				#pragma dcl position diffuse specular texcoord0 texcoord1 normal
			#else
				#pragma dcl position diffuse texcoord0 texcoord1 normal
			#endif
		#endif
	#else // USE_TEXTURE_LOOKUP
		#if DOUBLEUV_SET
			#pragma dcl position diffuse specular texcoord0 texcoord1 normal
		#else // DOUBLEUV_SET
			#pragma dcl position diffuse specular texcoord0 normal
		#endif // DOUBLEUV_SET
	#endif // USE_TEXTURE_LOOKUP
#endif // NORMAL_MAP

#endif //PRAGMA_DCL

#ifdef _USE_RDR2_STYLE_TEXTURING
#include "Terrain.fxh"
#endif


BEGIN_RAGE_CONSTANT_BUFFER(terrain_cb_common_locals,b10)
#if SPECULAR

#define FRESNEL (0)

float specularFalloffMult : Specular
<
	string UIName = "Specular Falloff";
	float UIMin = 0.0;
	float UIMax = 512.0;
	float UIStep = 0.1;
> = 48.0;

#if SPECULAR_MAP
float specularFalloffMultSpecMap : specularFalloffMultSpecMap
<
string UIName = "Specular Falloff (Spec Map)";
float UIMin = 0.0;
float UIMax = 512.0;
float UIStep = 0.1;
> = 48.0;

float specFalloffAdjust : specFalloffAdjust
<
	string UIName = "Specular Falloff Adjust";
	float UIMin = 0.0;
	float UIMax = 1.0;
	float UIStep = 0.1;
> = 1.0;
#endif

float specularIntensityMult : SpecularColor
<
	string UIName = "Specular Intensity";
	float UIMin = 0.0;
	float UIMax = 1.0;
	float UIStep = 0.01;
> = 0.0;

#if SPECULAR_MAP
float specularIntensityMultSpecMap : specularIntensityMultSpecMap
<
string UIName = "Specular Intensity (Spec Map)";
float UIMin = 0.0;
float UIMax = 1.0;
float UIStep = 0.01;
> = 1.0;

float specIntensityAdjust : specIntensityAdjust
<
	string UIName = "Specular Intensity Adjust";
	float UIMin = 0.0;
	float UIMax = 1.0;
	float UIStep = 0.01;
> = 1.0;
#endif

#endif // SPECULAR

#if NORMAL_MAP
	float bumpiness : Bumpiness
	<
		string UIWidget = "slider";
		float UIMin = 0.0;
		float UIMax = 200.0;
		float UIStep = .01;
		string UIName = "Bumpiness";
	> = 1.0;

#if TINT_TEXTURE_NORMAL
	float tintBumpiness : tintBumpiness
	<
		string UIWidget = "slider";
		float UIMin = 0.0;
		float UIMax = 200.0;
		float UIStep = .01;
		string UIName = "Tint Bumpiness";
	> = 1.0;
#endif // TINT_TEXTURE_NORMAL

#if PARALLAX_MAP
	float parallaxSelfShadowAmount : ParallaxSelfShadowAmount
	<
		string UIWidget = "slider";
		float UIMin = 0.01;
		float UIMax = 1.0f;
		float UIStep = 0.001;
		string UIName = "Parallax Self Shadow";
		int nostrip = 1;
	> = 0.95;

	float heightScale0 : HeightScale0
	<
		string UIWidget = "slider";
		float UIMin = 0.01;
		float UIMax = 0.05f;
		float UIStep = 0.001;
		string UIName = "Height Scale 1";
		int nostrip = 1;
	> = 0.03f;

	float heightBias0 : HeightBias0
	<
		string UIWidget = "slider";
		float UIMin = 0.00;
		float UIMax = 0.05f;
		float UIStep = 0.001;
		string UIName = "Height Bias 1";
		int nostrip = 1;
	> = 0.015f;

#if LAYER_COUNT > 1
	float heightScale1 : HeightScale1
	<
		string UIWidget = "slider";
		float UIMin = 0.01;
		float UIMax = 0.05f;
		float UIStep = 0.001;
		string UIName = "Height Scale 2";
		int nostrip = 1;
	> = 0.03f;

	float heightBias1 : HeightBias1
	<
		string UIWidget = "slider";
		float UIMin = 0.00;
		float UIMax = 0.05f;
		float UIStep = 0.001;
		string UIName = "Height Bias 2";
		int nostrip = 1;
	> = 0.015f;
#endif // LAYER_COUNT > 1

#if LAYER_COUNT > 2
	float heightScale2 : HeightScale2
	<
		string UIWidget = "slider";
		float UIMin = 0.01;
		float UIMax = 0.05f;
		float UIStep = 0.001;
		string UIName = "Height Scale 3";
		int nostrip = 1;
	> = 0.03f;

	float heightBias2 : HeightBias2
	<
		string UIWidget = "slider";
		float UIMin = 0.00;
		float UIMax = 0.05f;
		float UIStep = 0.001;
		string UIName = "Height Bias 3";
		int nostrip = 1;
	> = 0.015f;
#endif // LAYER_COUNT > 2

#if LAYER_COUNT > 3
	float heightScale3 : HeightScale3
	<
		string UIWidget = "slider";
		float UIMin = 0.01;
		float UIMax = 0.05f;
		float UIStep = 0.001;
		string UIName = "Height Scale 4";
		int nostrip = 1;
	> = 0.03f;

	float heightBias3 : HeightBias3
	<
		string UIWidget = "slider";
		float UIMin = 0.00;
		float UIMax = 0.05f;
		float UIStep = 0.001;
		string UIName = "Height Bias 4";
		int nostrip = 1;
	> = 0.015f;
#endif // LAYER_COUNT > 3

#if LAYER_COUNT > 4
	float heightScale4 : HeightScale4
	<
		string UIWidget = "slider";
		float UIMin = 0.01;
		float UIMax = 0.05f;
		float UIStep = 0.001;
		string UIName = "Height Scale 5";
		int nostrip = 1;
	> = 0.03f;

	float heightBias4 : HeightBias4
	<
		string UIWidget = "slider";
		float UIMin = 0.00;
		float UIMax = 0.05f;
		float UIStep = 0.001;
		string UIName = "Height Bias 5";
		int nostrip = 1;
	> = 0.015f;
#endif // LAYER_COUNT > 4

#if LAYER_COUNT > 5
	float heightScale5 : HeightScale5
	<
		string UIWidget = "slider";
		float UIMin = 0.01;
		float UIMax = 0.05f;
		float UIStep = 0.001;
		string UIName = "Height Scale 6";
		int nostrip = 1;
	> = 0.03f;

	float heightBias5 : HeightBias5
	<
		string UIWidget = "slider";
		float UIMin = 0.00;
		float UIMax = 0.05f;
		float UIStep = 0.001;
		string UIName = "Height Bias 6";
		int nostrip = 1;
	> = 0.015f;
#endif // LAYER_COUNT > 5

#if LAYER_COUNT > 6
	float heightScale6 : HeightScale6
	<
		string UIWidget = "slider";
		float UIMin = 0.01;
		float UIMax = 0.05f;
		float UIStep = 0.001;
		string UIName = "Height Scale 7";
		int nostrip = 1;
	> = 0.03f;

	float heightBias6 : HeightBias6
	<
		string UIWidget = "slider";
		float UIMin = 0.00;
		float UIMax = 0.05f;
		float UIStep = 0.001;
		string UIName = "Height Bias 7";
		int nostrip = 1;
	> = 0.015f;
#endif // LAYER_COUNT > 6

#if LAYER_COUNT > 7
	float heightScale7 : HeightScale7
	<
		string UIWidget = "slider";
		float UIMin = 0.01;
		float UIMax = 0.05f;
		float UIStep = 0.001;
		string UIName = "Height Scale 8";
		int nostrip = 1;
	> = 0.03f;

	float heightBias7 : HeightBias7
	<
		string UIWidget = "slider";
		float UIMin = 0.00;
		float UIMax = 0.05f;
		float UIStep = 0.001;
		string UIName = "Height Bias 8";
		int nostrip = 1;
	> = 0.015f;
#endif // LAYER_COUNT > 7


#endif // PARALLAX_MAP

#endif // NORMAL_MAP...

#if SELF_SHADOW
	float bumpSelfShadowAmount : BumpSelfShadowAmount
	<
		string UIWidget = "slider";
		float UIMin = 0.0;
		float UIMax = 1.0;
		float UIStep = 0.005;
		string      UIName = "Bump Self Shadow";
	> = 0.3;
#endif // SELF_SHADOW

#if WETNESS_MULTIPLIER
#if PER_MATERIAL_WETNESS_MULTIPLIER
	float4 materialWetnessMultiplier : materialWetnessMultiplier
	<
		string UIWidget = "slider";
		float UIMin = 0.0;
		float UIMax = 1.0;
		float UIStep = 0.005;
		string      UIName = "Material wetness multiplier";
	> = 0.0;
#else
	float wetnessMultiplier : WetnessMultiplier
	<
		string UIWidget = "slider";
		float UIMin = 0.0;
		float UIMax = 1.0;
		float UIStep = 0.005;
		string      UIName = "Wetness multiplier";
	> = 0.0;
#endif	
#endif


// __WIN32PC is for the win32_30 shaders we build x86 resources against.
#if __MAX || TERRAIN_TESSELLATION_SHADER_DATA
	float useTessellation : UseTessellation
	<
		string UIName = "Use tessellation";
		float UIMin = 0.0;
		float UIMax = 1.0;
		float UIStep = 1.0;
		int nostrip	= 1;
	> = 0.0f;
#endif //__MAX || TERRAIN_TESSELLATION_SHADER_DATA

#if TINT_TEXTURE
#if USE_RDR_STYLE_BLENDING
	float TintIntensity : TintIntensity
	<
		string UIWidget = "slider";
		float UIMin = 0.0f;
		float UIMax = 10.0f;
		float UIStep = 0.01f;
		string UIName = "Tint Intensity";
		int nostrip = 1;
	> = 1.0f;

	float TintBlendAmount : TintBlendAmount
	<
		string UIWidget = "slider";
		float UIMin = 0.0f;
		float UIMax = 1.0f;
		float UIStep = 0.01f;
		string UIName = "Tint Blend Amount";
		int nostrip = 1;
	>  = 1.0f;
#else
	float desaturateTint : DesaturateTint
	<
		string UIWidget = "slider";
		float UIMin = 0.0f;
		float UIMax = 1.0f;
		float UIStep = 1.0f;
		string UIName = "Desaturate Tint";
		int nostrip = 1;
	> = 0.0f;

	float tintBlendLayer0 : TintBlendLayer0
	<
		string UIWidget = "slider";
		float UIMin = 0.0f;
		float UIMax = 1.0f;
		float UIStep = 0.001f;
		string UIName = "Tint Blend Layer 1";
		int nostrip = 1;
	> = 0.5f;

#if LAYER_COUNT > 1
	float tintBlendLayer1 : TintBlendLayer1
	<
		string UIWidget = "slider";
		float UIMin = 0.0f;
		float UIMax = 1.0f;
		float UIStep = 0.001f;
		string UIName = "Tint Blend Layer 2";
		int nostrip = 1;
	> = 0.5f;
#endif // LAYER_COUNT > 1

#if LAYER_COUNT > 2
	float tintBlendLayer2 : TintBlendLayer2
	<
		string UIWidget = "slider";
		float UIMin = 0.0f;
		float UIMax = 1.0f;
		float UIStep = 0.001f;
		string UIName = "Tint Blend Layer 3";
		int nostrip = 1;
	> = 0.5f;
#endif // LAYER_COUNT > 2

#if LAYER_COUNT > 3
	float tintBlendLayer3 : TintBlendLayer3
	<
		string UIWidget = "slider";
		float UIMin = 0.0f;
		float UIMax = 1.0f;
		float UIStep = 0.001f;
		string UIName = "Tint Blend Layer 4";
		int nostrip = 1;
	> = 0.5f;
#endif // LAYER_COUNT > 3

#if LAYER_COUNT > 4
	float tintBlendLayer4 : TintBlendLayer4
	<
		string UIWidget = "slider";
		float UIMin = 0.0f;
		float UIMax = 1.0f;
		float UIStep = 0.001f;
		string UIName = "Tint Blend Layer 5";
		int nostrip = 1;
	> = 0.5f;
#endif // LAYER_COUNT > 4

#if LAYER_COUNT > 5
	float tintBlendLayer5 : TintBlendLayer5
	<
		string UIWidget = "slider";
		float UIMin = 0.0f;
		float UIMax = 1.0f;
		float UIStep = 0.001f;
		string UIName = "Tint Blend Layer 6";
		int nostrip = 1;
	> = 0.5f;
#endif // LAYER_COUNT > 5

#if LAYER_COUNT > 6
	float tintBlendLayer6 : TintBlendLayer6
	<
		string UIWidget = "slider";
		float UIMin = 0.0f;
		float UIMax = 1.0f;
		float UIStep = 0.001f;
		string UIName = "Tint Blend Layer 7";
		int nostrip = 1;
	> = 0.5f;
#endif // LAYER_COUNT > 6

#if LAYER_COUNT > 7
	float tintBlendLayer7 : TintBlendLayer7
	<
		string UIWidget = "slider";
		float UIMin = 0.0f;
		float UIMax = 1.0f;
		float UIStep = 0.001f;
		string UIName = "Tint Blend Layer 8";
		int nostrip = 1;
	> = 0.5f;
#endif // LAYER_COUNT > 7
#endif // USE_RDR_STYLE_BLENDING
#endif // TINT_TEXTURE


EndConstantBufferDX10( terrain_cb_common_locals )


#define SHADOW_CASTING            (1)
#define SHADOW_CASTING_TECHNIQUES (1 && SHADOW_CASTING_TECHNIQUES_OK)
#define SHADOW_RECEIVING          (1) // need this for the water reflection
#define SHADOW_RECEIVING_VS       (0)
#include "../Lighting/Shadows/localshadowglobals.fxh"
#include "../Lighting/Shadows/cascadeshadows.fxh"
#include "../Lighting/lighting.fxh"
#include "../Lighting/forward_lighting.fxh"
#include "../Debug/EntitySelect.fxh"

// Aniso quality can be interleaved such that only one texture (either diffuse or normal) will get either the high or low on each layer.
// This help keep quality high and saves a bit of performance compared to putting them all at the highest aniso. 
#define TERRAIN_DIFFUSE_FILTER HQLINEAR 
#define TERRAIN_DIFFUSE_BIAS 0
#define TERRAIN_DIFFUSE_MAX_ANISOTROPY  4

#define TERRAIN_NORMAL_FILTER HQLINEAR 
#define TERRAIN_NORMAL_BIAS 0
#define TERRAIN_NORMAL_MAX_ANISOTROPY 4


#if PARALLAX_MAP || SPECULAR_MAP
	#define TERRAIN_HEIGHTSPEC_FILTER LINEAR
	#define TERRAIN_HEIGHTSPEC_MAX_ANISOTROPY 1
#endif

#ifndef _USE_RDR2_STYLE_TEXTURING

BeginSampler(	sampler, diffuseTexture_layer0, TextureSampler_layer0, DiffuseTexture_layer0)
	string UIName = "Diffuse 1";
	string TCPTemplate = "Diffuse";
	string TextureType="Diffuse";
ContinueSampler(sampler, diffuseTexture_layer0, TextureSampler_layer0, DiffuseTexture_layer0)
    AddressU  = WRAP;        
    AddressV  = WRAP;
    AddressW  = WRAP;
    MIPFILTER = MIPLINEAR;
    MINFILTER = TERRAIN_DIFFUSE_FILTER;
    MAGFILTER = TERRAIN_DIFFUSE_FILTER;
	MipMapLodBias = TERRAIN_DIFFUSE_BIAS;
	ANISOTROPIC_LEVEL(TERRAIN_DIFFUSE_MAX_ANISOTROPY)
EndSampler;

#if NORMAL_MAP
	BeginSampler(sampler2D,bumpTexture_layer0,BumpSampler_layer0,BumpTexture_layer0)
		string UIName="Bump 1";
		string UIHint="NORMAL_MAP";
		string TCPTemplate = "NormalMap";
		string TextureType="Bump";
	ContinueSampler(sampler2D,bumpTexture_layer0,BumpSampler_layer0,BumpTexture_layer0)
		AddressU  = WRAP;        
		AddressV  = WRAP;
		AddressW  = WRAP;
		MIPFILTER = MIPLINEAR;
		MINFILTER = TERRAIN_NORMAL_FILTER;
		MAGFILTER = TERRAIN_NORMAL_FILTER;
		MipMapLodBias = TERRAIN_NORMAL_BIAS;
		ANISOTROPIC_LEVEL(TERRAIN_NORMAL_MAX_ANISOTROPY)
	EndSampler;
#endif	// NORMAL_MAP

#if PARALLAX_MAP || SPECULAR_MAP
BeginSampler(sampler2D, heightMapTextureLayer0, heightMapSamplerLayer0, heightMapTextureLayer0)
	string UIName = "Height 1[r] + Specular 1[g]";
	string TCPTemplateRelative = "maps_other/DisplacementMap";
	int	TCPAllowOverride = 0;
	string TextureType = "Parallax";
	int nostrip = 1;
ContinueSampler(sampler, heightMapTextureLayer0, heightMapSamplerLayer0, heightMapTextureLayer0)
    AddressU  = WRAP;        
    AddressV  = WRAP;
    AddressW  = WRAP;
    MIPFILTER = MIPLINEAR;
	MINFILTER = TERRAIN_HEIGHTSPEC_FILTER;
	MAGFILTER = TERRAIN_HEIGHTSPEC_FILTER;
	MipMapLodBias = TERRAIN_DIFFUSE_BIAS;
	ANISOTROPIC_LEVEL(TERRAIN_HEIGHTSPEC_MAX_ANISOTROPY)
EndSampler;
#endif // PARALLAX_MAP

#if LAYER_COUNT > 1
BeginSampler(	sampler, diffuseTexture_layer1, TextureSampler_layer1, DiffuseTexture_layer1)
	string UIName = "Diffuse 2";
	string TCPTemplate = "Diffuse";
	string TextureType="Diffuse";
ContinueSampler(sampler, diffuseTexture_layer1, TextureSampler_layer1, DiffuseTexture_layer1)
    AddressU  = WRAP;        
    AddressV  = WRAP;
    AddressW  = WRAP;
    MIPFILTER = MIPLINEAR;
	MINFILTER = TERRAIN_DIFFUSE_FILTER;
	MAGFILTER = TERRAIN_DIFFUSE_FILTER;
	MipMapLodBias = TERRAIN_DIFFUSE_BIAS;
	ANISOTROPIC_LEVEL(TERRAIN_DIFFUSE_MAX_ANISOTROPY)
EndSampler;

#if NORMAL_MAP
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
		MINFILTER = TERRAIN_NORMAL_FILTER;
		MAGFILTER = TERRAIN_NORMAL_FILTER;
		MipMapLodBias = TERRAIN_NORMAL_BIAS;
		ANISOTROPIC_LEVEL(TERRAIN_NORMAL_MAX_ANISOTROPY)
	EndSampler;
#endif	// NORMAL_MAP

#if PARALLAX_MAP || SPECULAR_MAP
BeginSampler(sampler2D, heightMapTextureLayer1, heightMapSamplerLayer1, heightMapTextureLayer1)
	string UIName = "Height 2[r] + Specular 2[g]";
	string TCPTemplateRelative = "maps_other/DisplacementMap";
	int	TCPAllowOverride = 0;
	string TextureType = "Parallax";
	int nostrip = 1;
ContinueSampler(sampler, heightMapTextureLayer1, heightMapSamplerLayer1, heightMapTextureLayer1)
    AddressU  = WRAP;        
    AddressV  = WRAP;
    AddressW  = WRAP;
    MIPFILTER = MIPLINEAR;
	MINFILTER = TERRAIN_HEIGHTSPEC_FILTER;
	MAGFILTER = TERRAIN_HEIGHTSPEC_FILTER;
	MipMapLodBias = TERRAIN_DIFFUSE_BIAS;
	ANISOTROPIC_LEVEL(TERRAIN_HEIGHTSPEC_MAX_ANISOTROPY)
EndSampler;
#endif // PARALLAX_MAP

#endif

#if LAYER_COUNT > 2
BeginSampler(	sampler, diffuseTexture_layer2, TextureSampler_layer2, DiffuseTexture_layer2)
	string UIName = "Diffuse 3";
	string TCPTemplate = "Diffuse";
	string TextureType="Diffuse";
ContinueSampler(sampler, diffuseTexture_layer2, TextureSampler_layer2, DiffuseTexture_layer2)
    AddressU  = WRAP;        
    AddressV  = WRAP;
    AddressW  = WRAP;
    MIPFILTER = MIPLINEAR;
	MINFILTER = TERRAIN_DIFFUSE_FILTER;                                                                                     
	MAGFILTER = TERRAIN_DIFFUSE_FILTER;
	MipMapLodBias = TERRAIN_DIFFUSE_BIAS;
	ANISOTROPIC_LEVEL(TERRAIN_DIFFUSE_MAX_ANISOTROPY)
EndSampler;

#if NORMAL_MAP
	BeginSampler(sampler2D,bumpTexture_layer2,BumpSampler_layer2,BumpTexture_layer2)
		string UIName="Bump 3";
		string UIHint="NORMAL_MAP";
	string TCPTemplate="NormalMap";
		string TextureType="Bump";
	ContinueSampler(sampler2D,bumpTexture_layer2,BumpSampler_layer2,BumpTexture_layer2)
		AddressU  = WRAP;        
		AddressV  = WRAP;
		AddressW  = WRAP;
		MIPFILTER = MIPLINEAR;
		MINFILTER = TERRAIN_NORMAL_FILTER;
		MAGFILTER = TERRAIN_NORMAL_FILTER;
		MipMapLodBias = TERRAIN_NORMAL_BIAS;
		ANISOTROPIC_LEVEL(TERRAIN_NORMAL_MAX_ANISOTROPY)
	EndSampler;
#endif	// NORMAL_MAP

#if PARALLAX_MAP || SPECULAR_MAP
BeginSampler(sampler2D, heightMapTextureLayer2, heightMapSamplerLayer2, heightMapTextureLayer2)
	string UIName = "Height 3[r] + Specular 3[g]";
	string TCPTemplateRelative = "maps_other/DisplacementMap";
	int	TCPAllowOverride = 0;
	string TextureType = "Parallax";
	int nostrip = 1;
ContinueSampler(sampler, heightMapTextureLayer2, heightMapSamplerLayer2, heightMapTextureLayer2)
    AddressU  = WRAP;        
    AddressV  = WRAP;
    AddressW  = WRAP;
    MIPFILTER = MIPLINEAR;
	MINFILTER = TERRAIN_HEIGHTSPEC_FILTER;
	MAGFILTER = TERRAIN_HEIGHTSPEC_FILTER;
	MipMapLodBias = TERRAIN_DIFFUSE_BIAS;
	ANISOTROPIC_LEVEL(TERRAIN_HEIGHTSPEC_MAX_ANISOTROPY)
EndSampler;
#endif // PARALLAX_MAP

#endif

#if LAYER_COUNT > 3
BeginSampler(	sampler, diffuseTexture_layer3, TextureSampler_layer3, DiffuseTexture_layer3)
	string UIName = "Diffuse 4";
	string TCPTemplate = "Diffuse";
	string TextureType="Diffuse";
ContinueSampler(sampler, diffuseTexture_layer3, TextureSampler_layer3, DiffuseTexture_layer3)
    AddressU  = WRAP;        
    AddressV  = WRAP;
    AddressW  = WRAP;
    MIPFILTER = MIPLINEAR;
	MINFILTER = TERRAIN_DIFFUSE_FILTER;
	MAGFILTER = TERRAIN_DIFFUSE_FILTER;
	MipMapLodBias = TERRAIN_DIFFUSE_BIAS;
	ANISOTROPIC_LEVEL(TERRAIN_DIFFUSE_MAX_ANISOTROPY)
EndSampler;

#if NORMAL_MAP
	BeginSampler(sampler2D,bumpTexture_layer3,BumpSampler_layer3,BumpTexture_layer3)
		string UIName="Bump 4";
		string UIHint="NORMAL_MAP";
	string TCPTemplate="NormalMap";
		string TextureType="Bump";
	ContinueSampler(sampler2D,bumpTexture_layer3,BumpSampler_layer3,BumpTexture_layer3)
		AddressU  = WRAP;        
		AddressV  = WRAP;
		AddressW  = WRAP;
		MIPFILTER = MIPLINEAR;
		MINFILTER = TERRAIN_NORMAL_FILTER;
		MAGFILTER = TERRAIN_NORMAL_FILTER;
		MipMapLodBias = TERRAIN_NORMAL_BIAS;
		ANISOTROPIC_LEVEL(TERRAIN_NORMAL_MAX_ANISOTROPY)
	EndSampler;
#endif	// NORMAL_MAP

#if PARALLAX_MAP || SPECULAR_MAP
BeginSampler(sampler2D, heightMapTextureLayer3, heightMapSamplerLayer3, heightMapTextureLayer3)
	string UIName = "Height 4[r] + Specular 4[g]";
	string TCPTemplateRelative = "maps_other/DisplacementMap";
	int	TCPAllowOverride = 0;
	string TextureType = "Parallax";
	int nostrip = 1;
ContinueSampler(sampler, heightMapTextureLayer3, heightMapSamplerLayer3, heightMapTextureLayer3)
    AddressU  = WRAP;        
    AddressV  = WRAP;
    AddressW  = WRAP;
    MIPFILTER = MIPLINEAR;
	MINFILTER = TERRAIN_HEIGHTSPEC_FILTER;
	MAGFILTER = TERRAIN_HEIGHTSPEC_FILTER;
	MipMapLodBias = TERRAIN_DIFFUSE_BIAS;
	ANISOTROPIC_LEVEL(TERRAIN_HEIGHTSPEC_MAX_ANISOTROPY)
EndSampler;
#endif // PARALLAX_MAP

#endif

#if LAYER_COUNT > 4
BeginSampler(	sampler, diffuseTexture_layer4, TextureSampler_layer4, DiffuseTexture_layer4)
	string UIName = "Diffuse 5";
	string TCPTemplate = "Diffuse";
	string TextureType="Diffuse";
ContinueSampler(sampler, diffuseTexture_layer4, TextureSampler_layer4, DiffuseTexture_layer4)
    AddressU  = WRAP;        
    AddressV  = WRAP;
    AddressW  = WRAP;
    MIPFILTER = MIPLINEAR;
	MINFILTER = TERRAIN_DIFFUSE_FILTER;
	MAGFILTER = TERRAIN_DIFFUSE_FILTER;
	MipMapLodBias = TERRAIN_DIFFUSE_BIAS;
	ANISOTROPIC_LEVEL(TERRAIN_DIFFUSE_MAX_ANISOTROPY)
EndSampler;

#if NORMAL_MAP
	BeginSampler(sampler2D,bumpTexture_layer4,BumpSampler_layer4,BumpTexture_layer4)
		string UIName="Bump 5";
		string UIHint="NORMAL_MAP";
	string TCPTemplate="NormalMap";
		string TextureType="Bump";
	ContinueSampler(sampler2D,bumpTexture_layer4,BumpSampler_layer4,BumpTexture_layer4)
		AddressU  = WRAP;        
		AddressV  = WRAP;
		AddressW  = WRAP;
		MIPFILTER = MIPLINEAR;
		MINFILTER = TERRAIN_NORMAL_FILTER;
		MAGFILTER = TERRAIN_NORMAL_FILTER;
		MipMapLodBias = TERRAIN_NORMAL_BIAS;
		ANISOTROPIC_LEVEL(TERRAIN_NORMAL_MAX_ANISOTROPY)
	EndSampler;
#endif	// NORMAL_MAP

#if PARALLAX_MAP || SPECULAR_MAP
BeginSampler(sampler2D, heightMapTextureLayer4, heightMapSamplerLayer4, heightMapTextureLayer4)
	string UIName = "Height 5[r] + Specular 5[g]";
	string TCPTemplateRelative = "maps_other/DisplacementMap";
	int	TCPAllowOverride = 0;
	string TextureType = "Parallax";
	int nostrip = 1;
ContinueSampler(sampler, heightMapTextureLayer4, heightMapSamplerLayer4, heightMapTextureLayer4)
    AddressU  = WRAP;        
    AddressV  = WRAP;
    AddressW  = WRAP;
    MIPFILTER = MIPLINEAR;
	MINFILTER = TERRAIN_HEIGHTSPEC_FILTER;
	MAGFILTER = TERRAIN_HEIGHTSPEC_FILTER;
	MipMapLodBias = TERRAIN_DIFFUSE_BIAS;
	ANISOTROPIC_LEVEL(TERRAIN_HEIGHTSPEC_MAX_ANISOTROPY)
EndSampler;
#endif // PARALLAX_MAP

#endif

#if LAYER_COUNT > 5
BeginSampler(	sampler, diffuseTexture_layer5, TextureSampler_layer5, DiffuseTexture_layer5)
	string UIName = "Diffuse 6";
	string TCPTemplate = "Diffuse";
	string TextureType="Diffuse";
ContinueSampler(sampler, diffuseTexture_layer5, TextureSampler_layer5, DiffuseTexture_layer5)
    AddressU  = WRAP;        
    AddressV  = WRAP;
    AddressW  = WRAP;
    MIPFILTER = MIPLINEAR;
	MINFILTER = TERRAIN_DIFFUSE_FILTER;
	MAGFILTER = TERRAIN_DIFFUSE_FILTER;
	MipMapLodBias = TERRAIN_DIFFUSE_BIAS;
	ANISOTROPIC_LEVEL(TERRAIN_DIFFUSE_MAX_ANISOTROPY)
EndSampler;

#if NORMAL_MAP
	BeginSampler(sampler2D,bumpTexture_layer5,BumpSampler_layer5,BumpTexture_layer5)
		string UIName="Bump 6";
		string UIHint="NORMAL_MAP";
	string TCPTemplate="NormalMap";
		string TextureType="Bump";
	ContinueSampler(sampler2D,bumpTexture_layer5,BumpSampler_layer5,BumpTexture_layer5)
		AddressU  = WRAP;        
		AddressV  = WRAP;
		AddressW  = WRAP;
		MIPFILTER = MIPLINEAR;
		MINFILTER = TERRAIN_NORMAL_FILTER;
		MAGFILTER = TERRAIN_NORMAL_FILTER;
		MipMapLodBias = TERRAIN_NORMAL_BIAS;
		ANISOTROPIC_LEVEL(TERRAIN_NORMAL_MAX_ANISOTROPY)
	EndSampler;
#endif	// NORMAL_MAP

#if PARALLAX_MAP || SPECULAR_MAP
BeginSampler(sampler2D, heightMapTextureLayer5, heightMapSamplerLayer5, heightMapTextureLayer5)
	string UIName = "Height 6[r] + Specular 6[g]";
	string TCPTemplateRelative = "maps_other/DisplacementMap";
	int	TCPAllowOverride = 0;
	string TextureType = "Parallax";
	int nostrip = 1;
ContinueSampler(sampler, heightMapTextureLayer5, heightMapSamplerLayer5, heightMapTextureLayer5)
    AddressU  = WRAP;        
    AddressV  = WRAP;
    AddressW  = WRAP;
    MIPFILTER = MIPLINEAR;
	MINFILTER = TERRAIN_HEIGHTSPEC_FILTER;
	MAGFILTER = TERRAIN_HEIGHTSPEC_FILTER;
	MipMapLodBias = TERRAIN_DIFFUSE_BIAS;
	ANISOTROPIC_LEVEL(TERRAIN_HEIGHTSPEC_MAX_ANISOTROPY)
EndSampler;
#endif // PARALLAX_MAP

#endif

#if LAYER_COUNT > 6
BeginSampler(	sampler, diffuseTexture_layer6, TextureSampler_layer6, DiffuseTexture_layer6)
	string UIName = "Diffuse 7";
	string TCPTemplate = "Diffuse";
	string TextureType="Diffuse";
ContinueSampler(sampler, diffuseTexture_layer6, TextureSampler_layer6, DiffuseTexture_layer6)
    AddressU  = WRAP;        
    AddressV  = WRAP;
    AddressW  = WRAP;
    MIPFILTER = MIPLINEAR;
	MINFILTER = TERRAIN_DIFFUSE_FILTER;
	MAGFILTER = TERRAIN_DIFFUSE_FILTER;
	MipMapLodBias = TERRAIN_DIFFUSE_BIAS;
	ANISOTROPIC_LEVEL(TERRAIN_DIFFUSE_MAX_ANISOTROPY)
EndSampler;

#if NORMAL_MAP
	BeginSampler(sampler2D,bumpTexture_layer6,BumpSampler_layer6,BumpTexture_layer6)
		string UIName="Bump 7";
		string UIHint="NORMAL_MAP";
	string TCPTemplate="NormalMap";
		string TextureType="Bump";
	ContinueSampler(sampler2D,bumpTexture_layer6,BumpSampler_layer6,BumpTexture_layer6)
		AddressU  = WRAP;        
		AddressV  = WRAP;
		AddressW  = WRAP;
		MIPFILTER = MIPLINEAR;
		MINFILTER = TERRAIN_NORMAL_FILTER;
		MAGFILTER = TERRAIN_NORMAL_FILTER;
		MipMapLodBias = TERRAIN_NORMAL_BIAS;
		ANISOTROPIC_LEVEL(TERRAIN_NORMAL_MAX_ANISOTROPY)
	EndSampler;
#endif	// NORMAL_MAP

#if PARALLAX_MAP || SPECULAR_MAP
BeginSampler(sampler2D, heightMapTextureLayer6, heightMapSamplerLayer6, heightMapTextureLayer6)
	string UIName = "Height 7[r] + Specular 7[g]";
	string TCPTemplateRelative = "maps_other/DisplacementMap";
	int	TCPAllowOverride = 0;
	string TextureType = "Parallax";
	int nostrip = 1;
ContinueSampler(sampler, heightMapTextureLayer6, heightMapSamplerLayer6, heightMapTextureLayer6)
    AddressU  = WRAP;        
    AddressV  = WRAP;
    AddressW  = WRAP;
    MIPFILTER = MIPLINEAR;
	MINFILTER = TERRAIN_HEIGHTSPEC_FILTER;
	MAGFILTER = TERRAIN_HEIGHTSPEC_FILTER;
	MipMapLodBias = TERRAIN_DIFFUSE_BIAS;
	ANISOTROPIC_LEVEL(TERRAIN_HEIGHTSPEC_MAX_ANISOTROPY)
EndSampler;
#endif // PARALLAX_MAP

#endif

#if LAYER_COUNT > 7
BeginSampler(	sampler, diffuseTexture_layer7, TextureSampler_layer7, DiffuseTexture_layer7)
	string UIName = "Diffuse 8";
	string TCPTemplate = "Diffuse";
	string TextureType="Diffuse";
ContinueSampler(sampler, diffuseTexture_layer7, TextureSampler_layer7, DiffuseTexture_layer7)
    AddressU  = WRAP;        
    AddressV  = WRAP;
    AddressW  = WRAP;
    MIPFILTER = MIPLINEAR;
	MINFILTER = TERRAIN_DIFFUSE_FILTER;
	MAGFILTER = TERRAIN_DIFFUSE_FILTER;
	MipMapLodBias = TERRAIN_DIFFUSE_BIAS;
	ANISOTROPIC_LEVEL(TERRAIN_DIFFUSE_MAX_ANISOTROPY)
EndSampler;

#if NORMAL_MAP
	BeginSampler(sampler2D,bumpTexture_layer7,BumpSampler_layer7,BumpTexture_layer7)
		string UIName="Bump 8";
		string UIHint="NORMAL_MAP";
	string TCPTemplate="NormalMap";
		string TextureType="Bump";
	ContinueSampler(sampler2D,bumpTexture_layer7,BumpSampler_layer7,BumpTexture_layer7)
		AddressU  = WRAP;        
		AddressV  = WRAP;
		AddressW  = WRAP;
		MIPFILTER = MIPLINEAR;
		MINFILTER = TERRAIN_NORMAL_FILTER;
		MAGFILTER = TERRAIN_NORMAL_FILTER;
		MipMapLodBias = TERRAIN_NORMAL_BIAS;
		ANISOTROPIC_LEVEL(TERRAIN_NORMAL_MAX_ANISOTROPY)
	EndSampler;
#endif	// NORMAL_MAP

#if PARALLAX_MAP || SPECULAR_MAP
BeginSampler(sampler2D, heightMapTextureLayer7, heightMapSamplerLayer7, heightMapTextureLayer7)
	string UIName = "Height 8[r] + Specular 8[g]";
	string TCPTemplateRelative = "maps_other/DisplacementMap";
	int	TCPAllowOverride = 0;
	string TextureType = "Parallax";
	int nostrip = 1;
ContinueSampler(sampler, heightMapTextureLayer7, heightMapSamplerLayer7, heightMapTextureLayer7)
    AddressU  = WRAP;        
    AddressV  = WRAP;
    AddressW  = WRAP;
    MIPFILTER = MIPLINEAR;
	MINFILTER = TERRAIN_HEIGHTSPEC_FILTER;
	MAGFILTER = TERRAIN_HEIGHTSPEC_FILTER;
	MipMapLodBias = TERRAIN_DIFFUSE_BIAS;
	ANISOTROPIC_LEVEL(TERRAIN_HEIGHTSPEC_MAX_ANISOTROPY)
EndSampler;
#endif // PARALLAX_MAP

#endif

#if USE_TEXTURE_LOOKUP || USE_RDR_STYLE_BLENDING //we need this sampler and not the other texture lookup stuff

//this is horrible, need more texture slots :(
#if PALETTE_TINT && PARALLAX_MAP && !TINT_TEXTURE
#ifndef REUSE_SAMPLER_0
	#pragma sampler 0 //reuse the diffuse sampler slot, seen as we don't use it in this shader
	#define REUSE_SAMPLER_0
#else
	#error "Can't use this trick twice in one shader, we need more shader slots!"
#endif // REUSE_SAMPLER_0
#endif // PALETTE_TINT && PARALLAX_MAP && !TINT_TEXTURE

BeginSampler(	sampler, lookupTexture, lookupSampler, lookupTexture)
	string UIName = "Lookup texture";
	string TCPTemplate = "Diffuse";
	string TextureType="Lookup";
ContinueSampler(sampler, lookupTexture, lookupSampler, lookupTexture)
    AddressU  = WRAP;        
    AddressV  = WRAP;
    AddressW  = WRAP;
    MIPFILTER = LINEAR;
    MINFILTER = LINEAR;
    MAGFILTER = LINEAR;
EndSampler;
#endif // USE_TEXTURE_LOOKUP

#if TINT_TEXTURE
BeginSampler(sampler, tintTexture, tintSampler, tintTexture)
	string UIName = "Tint[rgb] + Tint blend[a]";
	string TCPTemplate = "Diffuse";
	string TextureType="Diffuse";
ContinueSampler(sampler, tintTexture, tintSampler, tintTexture)
    AddressU  = WRAP;
    AddressV  = WRAP;
    AddressW  = WRAP;
    MIPFILTER = MIPLINEAR;
    MINFILTER = TERRAIN_DIFFUSE_FILTER;
    MAGFILTER = TERRAIN_DIFFUSE_FILTER;
	MipMapLodBias = TERRAIN_DIFFUSE_BIAS;
	ANISOTROPIC_LEVEL(TERRAIN_DIFFUSE_MAX_ANISOTROPY)
EndSampler;

#if NORMAL_MAP && TINT_TEXTURE_NORMAL
//this can hopefully be removed if we can get some more texture slots
#ifndef REUSE_SAMPLER_0
	#pragma sampler 0 //reuse the diffuse sampler slot, seen as we don't use it in this shader
	#define REUSE_SAMPLER_0
#else
	#error "Can't use this trick twice in one shader, we need more shader slots!"
#endif // REUSE_SAMPLER_0

BeginSampler(sampler2D,tintBumpTexture,tintBumpSampler,tintBumpTexture)
	string UIName="Tint bump";
	string UIHint="NORMAL_MAP";
	string TCPTemplate="NormalMap";
	string TextureType="Bump";
ContinueSampler(sampler2D,tintBumpTexture,tintBumpSampler,tintBumpTexture)
	AddressU  = WRAP;        
	AddressV  = WRAP;
	AddressW  = WRAP;
	MIPFILTER = MIPLINEAR;
	MINFILTER = TERRAIN_NORMAL_FILTER;
	MAGFILTER = TERRAIN_NORMAL_FILTER;
	MipMapLodBias = TERRAIN_NORMAL_BIAS;
	ANISOTROPIC_LEVEL(TERRAIN_NORMAL_MAX_ANISOTROPY)
EndSampler;
#endif // NORMAL_MAP && TINT_TEXTURE_NORMAL

#endif

#endif


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//
//
//
struct vertexInput
{
	float3 pos			: POSITION;
	float4 diffuse		: COLOR0;
#if !USE_TEXTURE_LOOKUP	|| USE_DOUBLE_LOOKUP || USE_RDR_STYLE_BLENDING
	float4 specular		: COLOR1;
#endif // !USE_TEXTURE_LOOKUP || USE_DOUBLE_LOOKUP || USE_RDR_STYLE_BLENDING
#if USE_RDR_STYLE_BLENDING
	float2 blendWeight0	: TEXCOORD6;
	float2 blendWeight1	: TEXCOORD7;
#else
#endif // USE_RDR_STYLE_BLENDING
    float2 texCoord0	: TEXCOORD0;
#if DOUBLEUV_SET
    float2 texCoord1	: TEXCOORD1;
#endif // DOUBLEUV_SET
#if SEPERATE_TINT_TEXTURE_COORDS
	float2 tintTexCoord : TEXCOORD2;
#endif
#if USE_EDGE_WEIGHT 
#if SEPERATE_TINT_TEXTURE_COORDS
	float2 edgeWeight	: TEXCOORD3;
#elif DOUBLEUV_SET
	float2 edgeWeight	: TEXCOORD2;
#else
	float2 edgeWeight	: TEXCOORD1;
#endif // TINT_TEXTURE
#endif // USE_EDGE_WEIGHT
    float3 normal		: NORMAL;
#if NORMAL_MAP
	float4 tangent		: TANGENT0;
#endif // NORMAL_MAP
};


// copy of above for the puddles
struct northVertexInputBump
{
	float3 pos			: POSITION;
	float4 diffuse		: COLOR0;
#if !USE_TEXTURE_LOOKUP	|| USE_DOUBLE_LOOKUP
	float4 specular		: COLOR1;
#endif // !USE_TEXTURE_LOOKUP || USE_DOUBLE_LOOKUP
    float2 texCoord0	: TEXCOORD0;
#if DOUBLEUV_SET
    float2 texCoord1	: TEXCOORD1;
#endif // DOUBLEUV_SET
    float3 normal		: NORMAL;
#if NORMAL_MAP
	float4 tangent		: TANGENT0;
#endif // NORMAL_MAP
};


struct vertexInputCube
{
	float3 pos			: POSITION;
	float4 diffuse		: COLOR0;
#if !USE_TEXTURE_LOOKUP	|| USE_DOUBLE_LOOKUP
	float4 specular		: COLOR1;
#endif // !USE_TEXTURE_LOOKUP || USE_DOUBLE_LOOKUP
	float2 texCoord0	: TEXCOORD0;
#if DOUBLEUV_SET
	float2 texCoord1	: TEXCOORD1;
#endif // DOUBLEUV_SET
#if SEPERATE_TINT_TEXTURE_COORDS
	float2 tintTexCoord : TEXCOORD2;
#endif
	float3 normal		: NORMAL;

#if RAGE_INSTANCED_TECH
	uint InstID : SV_InstanceID;
#endif
};

struct vertexInput_lightweight0
{
	float3 pos			: POSITION;
	float4 diffuse		: COLOR0;
#if !USE_TEXTURE_LOOKUP	|| USE_DOUBLE_LOOKUP
	float4 specular		: COLOR1;
#endif // !USE_TEXTURE_LOOKUP || USE_DOUBLE_LOOKUP
	float2 texCoord0	: TEXCOORD0;
#if DOUBLEUV_SET
	float2 texCoord1	: TEXCOORD1;
#endif // DOUBLEUV_SET
	float3 normal		: NORMAL;
};

struct vertexOutput
{
#if DOUBLEUV_SET    
	float4 texCoord			: TEXCOORD0;	
#else // DOUBLEUV_SET
	float2 texCoord			: TEXCOORD0;	
#endif // DOUBLEUV_SET
#if !USE_TEXTURE_LOOKUP	|| USE_DOUBLE_LOOKUP
	float4 lookup			: COLOR0;
#endif // !USE_TEXTURE_LOOKUP || USE_DOUBLE_LOOKUP
#if PALETTE_TINT
	float4 tintColor		: COLOR1;
#endif
#if USE_RDR_STYLE_BLENDING
	float4 cpvLookup		: COLOR1;
	float4 cpvTint			: COLOR2;
#endif
	float3 worldPos			: TEXCOORD1;
	float3 worldNormal		: TEXCOORD2;
	float4 color0			: TEXCOORD3;
#if (SPECULAR && TERRAIN_SHADER_WORLD_EYE_POS) || PARALLAX_MAP
	float3 worldEyePos		: TEXCOORD4;
#endif //  (SPECULAR && TERRAIN_SHADER_WORLD_EYE_POS) || PARALLAX_MAP
#if NORMAL_MAP
	float3 worldTangent		: TEXCOORD5;
	float3 worldBinormal	: TEXCOORD6;
#endif // NORMAL_MAP
#if SEPERATE_TINT_TEXTURE_COORDS
	float2 tintTexCoord		: TEXCOORD8;
#endif

    DECLARE_POSITION_CLIPPLANES(pos)
// NOTE: if new entries are added that are needed for both the lit and unlit, make sure to copy them in VS_TransformUnlit()
};

struct vertexTerrainOutputCube
{
	DECLARE_POSITION(pos)
#if DOUBLEUV_SET    
	float4 texCoord			: TEXCOORD0;	
#else // DOUBLEUV_SET
	float2 texCoord			: TEXCOORD0;	
#endif // DOUBLEUV_SET
#if !USE_TEXTURE_LOOKUP	|| USE_DOUBLE_LOOKUP
	float4 lookup			: COLOR0;
#endif // !USE_TEXTURE_LOOKUP || USE_DOUBLE_LOOKUP
#if PALETTE_TINT
	float4 tintColor		: COLOR1;
#endif
	float3 worldPos			: TEXCOORD1;
	float3 worldNormal		: TEXCOORD2;
	float4 color0			: TEXCOORD3;
#if SEPERATE_TINT_TEXTURE_COORDS
	float2 tintTexCoord		: TEXCOORD4;
#endif
};

struct vertexOutput_lightweight0
{
#if DOUBLEUV_SET    
	float4 texCoord			: TEXCOORD0;	
#else // DOUBLEUV_SET
	float2 texCoord			: TEXCOORD0;	
#endif // DOUBLEUV_SET
#if !USE_TEXTURE_LOOKUP	|| USE_DOUBLE_LOOKUP
	float4 lookup			: COLOR0;
#endif // !USE_TEXTURE_LOOKUP || USE_DOUBLE_LOOKUP
#if PALETTE_TINT
	float4 tintColor		: COLOR1;
#endif
	float3 worldPos			: TEXCOORD1;
	float3 worldNormal		: TEXCOORD2;
	float4 color0			: TEXCOORD3;
#if SEPERATE_TINT_TEXTURE_COORDS
	float2 tintTexCoord		: TEXCOORD4;
#endif
#if NORMAL_MAP
	float3 worldTangent		: TEXCOORD5;
	float3 worldBinormal	: TEXCOORD6;
#endif // NORMAL_MAP	
	DECLARE_POSITION_CLIPPLANES(pos)
};

struct vertexOutputWaterReflection
{
#if DOUBLEUV_SET    
	float4 texCoord			: TEXCOORD0;	
#else // DOUBLEUV_SET
	float2 texCoord			: TEXCOORD0;	
#endif // DOUBLEUV_SET
#if !USE_TEXTURE_LOOKUP	|| USE_DOUBLE_LOOKUP
	float4 lookup			: COLOR0;
#endif // !USE_TEXTURE_LOOKUP || USE_DOUBLE_LOOKUP
#if PALETTE_TINT
	float4 tintColor		: COLOR1;
#endif
	float3 worldPos			: TEXCOORD1;
	float3 worldNormal		: TEXCOORD2;
	float4 color0			: TEXCOORD3;
#if SEPERATE_TINT_TEXTURE_COORDS
	float2 tintTexCoord		: TEXCOORD4;
#endif
	DECLARE_POSITION_CLIPPLANES(pos)
};

struct vertexOutputUnlit
{
    DECLARE_POSITION(pos)
#if DOUBLEUV_SET
	float4 texCoord			: TEXCOORD0;
#else // DOUBLEUV_SET
	float2 texCoord			: TEXCOORD0;
#endif // DOUBLEUV_SET
#if !USE_TEXTURE_LOOKUP	|| USE_DOUBLE_LOOKUP
	float4 lookup			: COLOR0;
#endif // !USE_TEXTURE_LOOKUP || USE_DOUBLE_LOOKUP
#if PALETTE_TINT || PALETTE_TINT_MAX
	float4 tintColor		: COLOR1;
#endif
#if USE_RDR_STYLE_BLENDING
	float4 cpvLookup		: COLOR1;
	float4 cpvTint			: COLOR2;
#endif
	float3 worldPos			: TEXCOORD1;
#if SEPERATE_TINT_TEXTURE_COORDS
	float2 tintTexCoord		: TEXCOORD2;
#endif
};

struct vertexOutputDeferred
{
    DECLARE_POSITION(pos)
#if DOUBLEUV_SET
	float4 texCoord			: TEXCOORD0;
#else // DOUBLEUV_SET
	float2 texCoord			: TEXCOORD0;
#endif // DOUBLEUV_SET
#if USE_RDR_STYLE_BLENDING
	float4 cpvLookup		: COLOR1;
	float4 cpvTint			: COLOR2;
#else
#if !USE_TEXTURE_LOOKUP	|| USE_DOUBLE_LOOKUP
	float4 lookup			: COLOR0;
#endif // !USE_TEXTURE_LOOKUP || USE_DOUBLE_LOOKUP
#endif // USE_RDR_STYLE_BLENDING
#if PALETTE_TINT
	float4 tintColor		: COLOR1;
#endif
	float4 worldNormal		: TEXCOORD2;		 // w has the self shadow setup value
	float4 color0			: TEXCOORD3;
#if NORMAL_MAP
	float3 worldTangent		: TEXCOORD4;
	float3 worldBinormal	: TEXCOORD5;
#if PARALLAX_MAP
	float3 worldEyePos		: TEXCOORD6;
#endif // PARALLAX_MAP
#endif // NORMAL_MAP
#if SEPERATE_TINT_TEXTURE_COORDS
	float2 tintTexCoord		: TEXCOORD7;
#endif
#if PARALLAX_MAP || (TINT_TEXTURE && TINT_TEXTURE_DISTANCE_BLEND)
	float viewDistance		: TEXCOORD8;
#endif // PARALLAX_MAP || (TINT_TEXTURE && TINT_TEXTURE_DISTANCE_BLEND)
#if USE_EDGE_WEIGHT
	float2 edgeWeight		: BLENDWEIGHT0;
#endif // USE_EDGE_WEIGHT
};


#if TERRAIN_TESSELLATION_SHADER_CODE
struct vertexOutputDeferred_CtrlPoint
{
	float4 projPosition		: CTRL_POS;
	float4 modelPosAndW		: CTRL_MODELPOSANDW;
	float3 modelNormal		: CTRL_MODELNORMAL;
#if DOUBLEUV_SET
	float4 texCoord			: CTRL_TEXCOORD0;
#else // DOUBLEUV_SET
	float2 texCoord			: CTRL_TEXCOORD0;
#endif // DOUBLEUV_SET
#if !USE_TEXTURE_LOOKUP	|| USE_DOUBLE_LOOKUP
	float4 lookup			: CTRL_COLOR0;
#endif // !USE_TEXTURE_LOOKUP || USE_DOUBLE_LOOKUP
#if PALETTE_TINT
	float4 tintColor		: CTRL_COLOR1;
#endif
	float4 worldNormal		: CTRL_TEXCOORD2;		 // w has the self shadow setup value
	float4 color0			: CTRL_TEXCOORD3;
#if NORMAL_MAP
	float3 worldTangent		: CTRL_TEXCOORD4;
	float3 worldBinormal	: CTRL_TEXCOORD5;
#endif // NORMAL_MAP
#if SEPERATE_TINT_TEXTURE_COORDS
	float2 tintTexCoord		: CTRL_TEXCOORD6;
#endif
	float openEdgeFactor	: CTRL_OPEN_EDGE_FACTOR;
#if PARALLAX_MAP || (TINT_TEXTURE && TINT_TEXTURE_DISTANCE_BLEND)
	float viewDistance		: CTRL_TEXCOORD8;
#endif // PARALLAX_MAP || (TINT_TEXTURE && TINT_TEXTURE_DISTANCE_BLEND)
#if USE_EDGE_WEIGHT
	float2 edgeWeight		: CTRL_BLENDWEIGHT0;
#endif // USE_EDGE_WEIGHT
};
#endif // TERRAIN_TESSELLATION_SHADER_CODE

#if PUDDLE_TECHNIQUES
#include "..\MegaShader\Puddle.fxh"
#endif // PUDDLE_TECHNIQUES


//
//
// calculates ambient & directional lighting for given pixel:
//
float3 ProcessSimpleLighting(float3 worldPos, float3 worldNormal, float3 vertColor
#if SPECULAR && TERRAIN_SHADER_WORLD_EYE_POS
	,float3	surfaceToEyeDir
#endif // SPECULAR && TERRAIN_SHADER_WORLD_EYE_POS
)
{
	SurfaceProperties surfaceInfo = (SurfaceProperties)0;

	// Setup some default properties so we can use the lighting functions
	surfaceInfo.surface_worldNormal = worldNormal;
	surfaceInfo.surface_baseColor = float4(1.0, 1.0, 1.0, 1.0);
	surfaceInfo.surface_diffuseColor = float4(vertColor, 1.0);
	#if SPECULAR
#if USE_DIFFLUM_ON_SPECINT
		surfaceInfo.surface_specularIntensity = specularIntensityMult;
#else // USE_DIFFLUM_ON_SPECINT
		surfaceInfo.surface_specularIntensity = specularIntensityMult;
#endif // USE_DIFFLUM_ON_SPECINT

		surfaceInfo.surface_specularExponent = specularFalloffMult;
		surfaceInfo.surface_fresnel = 0.98;
	#endif	// SPECULAR
	#if REFLECT
		surfaceInfo.surface_reflectionColor = float3(0.0, 0.0, 0.0);
	#endif // REFLECT
	surfaceInfo.surface_emissiveIntensity = 0.0f;
	#if SELF_SHADOW
		surfaceInfo.surface_selfShadow = dot(worldNormal.xyz, -gDirectionalLight.xyz) < 0;
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
//
//
// calculates directional lighting for given pixel:
//
float4 MaxProcessSimpleLighting(float3 worldPos, float3 worldNormal, float4 vertColor, float4 diffuseColor
#if SPECULAR && TERRAIN_SHADER_WORLD_EYE_POS
	,float3	eyeDirection
	,float	surfaceSpecularExponent, float surfaceSpecularIntensity
#endif // SPECULAR && TERRAIN_SHADER_WORLD_EYE_POS
)
{
	SurfaceProperties surfaceInfo;
		surfaceInfo.surface_worldNormal			= worldNormal;
		surfaceInfo.surface_baseColor			= float4(vertColor.rgb,1.0f);
		surfaceInfo.surface_diffuseColor		= diffuseColor;
	#if SPECULAR
		surfaceInfo.surface_specularIntensity	= surfaceSpecularIntensity;
		surfaceInfo.surface_specularExponent	= surfaceSpecularExponent;
	#endif
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
} // end of MaxProcessSimpleLighting()...
#endif //__MAX...

struct comboTexel 
{
	half3 color;
	half3 normal;
	half wetnessMult;
};


#ifdef _USE_RDR2_STYLE_TEXTURING

#if USE_DOUBLE_LOOKUP
comboTexel GetTexel_(float4 texcoord, half3 lookupV, half blend, half3 worldNormal, half3 worldBinormal, half3 worldTangent)
#elif USE_TEXTURE_LOOKUP
comboTexel GetTexel_(float4 texcoord, half3 worldNormal, half3 worldBinormal, half3 worldTangent)
#else
comboTexel GetTexel_(float4 texcoord, half3 lookupV, half3 worldNormal, half3 worldBinormal, half3 worldTangent)
#endif
{
	comboTexel result;
	half3 blendWeight1, blendWeight2;
	half aoFromCM;	
	result.color =  MegaTexPCFColorOnly(texcoord.xy, aoFromCM, blendWeight1, blendWeight2);
#if NORMAL_MAP
	float3 Snorm = MegaTexPCFNormalOnly(texcoord,blendWeight1,blendWeight2);
	result.normal = CalculateWorldNormal(Snorm, bumpiness, worldTangent, worldBinormal, worldNormal);
#else // NORMAL_MAP
	result.normal = worldNormal;	
#endif // NORMAL_MAP
	return result;
}

comboTexel GetTexel_(float4 texcoord,  half3 lookupV, half3 worldNormal)
{
	comboTexel result;
	half3 blendWeight1,blendWeight2;
	half aoFromCM;
	result.color =  MegaTexPCFColorOnly(texcoord.xy, aoFromCM, blendWeight1, blendWeight2);
	result.normal = worldNormal;	
	return result;
}

#else // _USE_RDR2_STYLE_TEXTURING

half GetAlpha(half3 rgb, half3 mrgb, half3 mask)
{
	half alphaR = mask.r*rgb.r + (1-mask.r)*(mrgb.r);
	half alphaG = mask.g*rgb.g + (1-mask.g)*(mrgb.g);
	half alphaB = mask.b*rgb.b + (1-mask.b)*(mrgb.b);

#if __MAX
	return saturate(alphaR)*saturate(alphaG)*saturate(alphaB);
#else
	return alphaR*alphaG*alphaB;
#endif
}


//
//
//
//

#if USE_RDR_STYLE_BLENDING
float4 BlendTerrainColor(float4 layerBlends, float2 texCoord)
{
	float4 result = float4(0.0f, 0.0f, 0.0f, 0.0f);

	result += layerBlends.x * tex2D(TextureSampler_layer0, texCoord);
	result += layerBlends.y * tex2D(TextureSampler_layer1, texCoord);
	result += layerBlends.z * tex2D(TextureSampler_layer2, texCoord);
	result += layerBlends.w * tex2D(TextureSampler_layer3, texCoord);

	return result;
}
#endif // USE_RDR_STYLE_BLENDING
half3 BlendTerrainColor( half3 srcRGB, float2 texCoord0, float2 texCoord1 )
{
	srcRGB = saturate(srcRGB);

	half3 msrcRGB = 1 - srcRGB;
	
	half4 alpha0716;
	half4 alpha2534;
	
	alpha0716.x = GetAlpha(srcRGB,msrcRGB,half3(0,0,0));
	alpha0716.y = GetAlpha(srcRGB,msrcRGB,half3(1,1,1));
	
	alpha0716.z = GetAlpha(srcRGB,msrcRGB,half3(0,0,1));
	alpha0716.w = GetAlpha(srcRGB,msrcRGB,half3(1,1,0));
	
	alpha2534.x = GetAlpha(srcRGB,msrcRGB,half3(0,1,0));
	alpha2534.y = GetAlpha(srcRGB,msrcRGB,half3(1,0,1));

	alpha2534.z = GetAlpha(srcRGB,msrcRGB,half3(0,1,1));
	alpha2534.w = GetAlpha(srcRGB,msrcRGB,half3(1,0,0));

	half3 result = h3tex2D(TextureSampler_layer0,LAYER0_TEXCOORD.xy)*alpha0716.x;

#if LAYER_COUNT > 1
	result += h3tex2D(TextureSampler_layer1,LAYER1_TEXCOORD.xy)*alpha0716.z;
#endif
#if LAYER_COUNT > 2
	result += h3tex2D(TextureSampler_layer2,LAYER2_TEXCOORD.xy)*alpha2534.x;
#endif
#if LAYER_COUNT > 3
	result += h3tex2D(TextureSampler_layer3,LAYER3_TEXCOORD.xy)*alpha2534.z;
#endif
#if LAYER_COUNT > 4
	result += h3tex2D((TextureSampler_layer4,LAYER4_TEXCOORD.xy)*alpha2534.w;
#endif
#if LAYER_COUNT > 5
	result += h3tex2D((TextureSampler_layer5,LAYER5_TEXCOORD.xy)*alpha2534.y;
#endif
#if LAYER_COUNT > 6
	result += h3tex2D((TextureSampler_layer6,LAYER6_TEXCOORD.xy)*alpha0716.w;
#endif
#if LAYER_COUNT > 7
	result += h3tex2D((TextureSampler_layer7,LAYER7_TEXCOORD.xy)*alpha0716.y;
#endif

	return result;
}

#if SEPERATE_TINT_TEXTURE_COORDS
half3 BlendLayerColorTint(sampler2D sampler0, float2 texCoords, float2 tintTextCoord, float blendValue)
{
	half3 layerColor = h3tex2D(sampler0, texCoords);
	half3 tintColor = h3tex2D(tintSampler, tintTextCoord);

	half3 lerpDestination;

	if(desaturateTint)
	{
		half3 tintColorGrayscale = (tintColor.r + tintColor.g + tintColor.b) / 3;
		lerpDestination = layerColor * lerp(tintColorGrayscale, tintColor, blendValue);
	}
	else
	{
		lerpDestination = (layerColor.r + layerColor.g + layerColor.b) / 3;
		lerpDestination *= tintColor;
	}

	return lerp(layerColor, lerpDestination, blendValue);
}

half3 BlendTerrainColorTint(half3 srcRGB, float2 texCoord0, float2 texCoord1, float2 tintTexCoord)
{
	half3 msrcRGB = 1 - srcRGB;
	
	half4 alpha0716;
	half4 alpha2534;
	
	alpha0716.x = GetAlpha(srcRGB,msrcRGB,half3(0,0,0));
	alpha0716.y = GetAlpha(srcRGB,msrcRGB,half3(1,1,1));
	
	alpha0716.z = GetAlpha(srcRGB,msrcRGB,half3(0,0,1));
	alpha0716.w = GetAlpha(srcRGB,msrcRGB,half3(1,1,0));
	
	alpha2534.x = GetAlpha(srcRGB,msrcRGB,half3(0,1,0));
	alpha2534.y = GetAlpha(srcRGB,msrcRGB,half3(1,0,1));

	alpha2534.z = GetAlpha(srcRGB,msrcRGB,half3(0,1,1));
	alpha2534.w = GetAlpha(srcRGB,msrcRGB,half3(1,0,0));

	half3 result = BlendLayerColorTint(TextureSampler_layer0,LAYER0_TEXCOORD.xy, tintTexCoord.xy, tintBlendLayer0)*alpha0716.x;

#if LAYER_COUNT > 1
	result += BlendLayerColorTint(TextureSampler_layer1,LAYER1_TEXCOORD.xy, tintTexCoord.xy, tintBlendLayer1)*alpha0716.z;
#endif
#if LAYER_COUNT > 2
	result += BlendLayerColorTint(TextureSampler_layer2,LAYER2_TEXCOORD.xy, tintTexCoord.xy, tintBlendLayer2)*alpha2534.x;
#endif
#if LAYER_COUNT > 3
	result += BlendLayerColorTint(TextureSampler_layer3,LAYER3_TEXCOORD.xy, tintTexCoord.xy, tintBlendLayer3)*alpha2534.z;
#endif
#if LAYER_COUNT > 4
	result += BlendLayerColorTint(TextureSampler_layer4,LAYER4_TEXCOORD.xy, tintTexCoord.xy, tintBlendLayer4)**alpha2534.w;
#endif
#if LAYER_COUNT > 5
	result += BlendLayerColorTint(TextureSampler_layer5,LAYER5_TEXCOORD.xy, tintTexCoord.xy, tintBlendLayer5)**alpha2534.y;
#endif
#if LAYER_COUNT > 6
	result += BlendLayerColorTint(TextureSampler_layer6,LAYER6_TEXCOORD.xy, tintTexCoord.xy, tintBlendLayer6)**alpha0716.w;
#endif
#if LAYER_COUNT > 7
	result += BlendLayerColorTint(TextureSampler_layer7,LAYER7_TEXCOORD.xy, tintTexCoord.xy, tintBlendLayer7)**alpha0716.y;
#endif

	return result;
}

#endif

#if NORMAL_MAP
//
//
//
//
#if USE_RDR_STYLE_BLENDING	
half3 BlendTerrainNormal(float4 layerBlends, float2 texCoord)
{
	half3 result = half3(0.0f, 0.0f, 0.0f);

	result += layerBlends.x * tex2D_NormalMap(BumpSampler_layer0, texCoord);
	result += layerBlends.y * tex2D_NormalMap(BumpSampler_layer1, texCoord);
	result += layerBlends.z * tex2D_NormalMap(BumpSampler_layer2, texCoord);
	result += layerBlends.w * tex2D_NormalMap(BumpSampler_layer3, texCoord);

	return result;
}
#endif // USE_RDR_STYLE_BLENDING
half3 BlendTerrainNormal(half3 srcRGB, float2 texCoord0, half2 texCoord1)
{
	half3 msrcRGB = 1 - srcRGB;
	
	half4 alpha0716;
	half4 alpha2534;
	
	alpha0716.x = GetAlpha(srcRGB,msrcRGB,half3(0,0,0));
	alpha0716.y = GetAlpha(srcRGB,msrcRGB,half3(1,1,1));
	
	alpha0716.z = GetAlpha(srcRGB,msrcRGB,half3(0,0,1));
	alpha0716.w = GetAlpha(srcRGB,msrcRGB,half3(1,1,0));
	
	alpha2534.x = GetAlpha(srcRGB,msrcRGB,half3(0,1,0));
	alpha2534.y = GetAlpha(srcRGB,msrcRGB,half3(1,0,1));

	alpha2534.z = GetAlpha(srcRGB,msrcRGB,half3(0,1,1));
	alpha2534.w = GetAlpha(srcRGB,msrcRGB,half3(1,0,0));
	
	half3 result = tex2D_NormalMap(BumpSampler_layer0,LAYER0_TEXCOORD.xy) * alpha0716.x;

#if LAYER_COUNT > 1
	result += tex2D_NormalMap(BumpSampler_layer1,LAYER1_TEXCOORD.xy) * alpha0716.z;
#endif
#if LAYER_COUNT > 2
	result += tex2D_NormalMap(BumpSampler_layer2,LAYER2_TEXCOORD.xy) * alpha2534.x;
#endif
#if LAYER_COUNT > 3
	result += tex2D_NormalMap(BumpSampler_layer3,LAYER3_TEXCOORD.xy) * alpha2534.z;
#endif
#if LAYER_COUNT > 4
	result += tex2D_NormalMap(BumpSampler_layer4,LAYER4_TEXCOORD.xy) * alpha2534.w;
#endif
#if LAYER_COUNT > 5
	result += tex2D_NormalMap(BumpSampler_layer5,LAYER5_TEXCOORD.xy) * alpha2534.y;
#endif
#if LAYER_COUNT > 6
	result += tex2D_NormalMap(BumpSampler_layer6,LAYER6_TEXCOORD.xy) * alpha0716.w;
#endif
#if LAYER_COUNT > 7
	result += tex2D_NormalMap(BumpSampler_layer7,LAYER7_TEXCOORD.xy) * alpha0716.y;
#endif

	return result;
}

#if (PARALLAX_MAP && __SHADERMODEL >= 40) || SPECULAR_MAP
//this is combined due to the limited number of texture slots height in the red channel, green is the specular
#if USE_RDR_STYLE_BLENDING	
half2 BlendTerrainHeightAndSpec(float4 layerBlends,  float2 texCoord0, half2 texCoord1)
{
	float2 result = float2(0.0f, 0.0f);

	result += layerBlends.x * tex2D(heightMapSamplerLayer0, texCoord0);
	result += layerBlends.y * tex2D(heightMapSamplerLayer1, texCoord0);
	result += layerBlends.z * tex2D(heightMapSamplerLayer2, texCoord0);
	result += layerBlends.w * tex2D(heightMapSamplerLayer3, texCoord0);

	return result;
}
#endif // USE_RDR_STYLE_BLENDING
half2 BlendTerrainHeightAndSpec(half3 srcRGB, float2 texCoord0, half2 texCoord1)
{
	half3 msrcRGB = 1 - srcRGB;
	
	half4 alpha0716;
	half4 alpha2534;
	
	alpha0716.x = GetAlpha(srcRGB,msrcRGB,half3(0,0,0));
	alpha0716.y = GetAlpha(srcRGB,msrcRGB,half3(1,1,1));
	
	alpha0716.z = GetAlpha(srcRGB,msrcRGB,half3(0,0,1));
	alpha0716.w = GetAlpha(srcRGB,msrcRGB,half3(1,1,0));
	
	alpha2534.x = GetAlpha(srcRGB,msrcRGB,half3(0,1,0));
	alpha2534.y = GetAlpha(srcRGB,msrcRGB,half3(1,0,1));

	alpha2534.z = GetAlpha(srcRGB,msrcRGB,half3(0,1,1));
	alpha2534.w = GetAlpha(srcRGB,msrcRGB,half3(1,0,0));

	float2 result = tex2D(heightMapSamplerLayer0, LAYER0_TEXCOORD.xy).rg * alpha0716.x;

#if LAYER_COUNT > 1
	result += tex2D(heightMapSamplerLayer1, LAYER1_TEXCOORD.xy).rg * alpha0716.z;
#endif
#if LAYER_COUNT > 2
	result += tex2D(heightMapSamplerLayer2, LAYER2_TEXCOORD.xy).rg * alpha2534.x;
#endif
#if LAYER_COUNT > 3
	result += tex2D(heightMapSamplerLayer3, LAYER3_TEXCOORD.xy).rg * alpha2534.z;
#endif
#if LAYER_COUNT > 4
	result += tex2D(heightMapSamplerLayer4, LAYER4_TEXCOORD.xy).rg * alpha2534.w;
#endif
#if LAYER_COUNT > 5
	result += tex2D(heightMapSamplerLayer5, LAYER5_TEXCOORD.xy).rg * alpha2534.y;
#endif
#if LAYER_COUNT > 6
	result += tex2D(heightMapSamplerLayer6, LAYER6_TEXCOORD.xy).rg * alpha0716.w;
#endif
#if LAYER_COUNT > 7
	result += tex2D(heightMapSamplerLayer7, LAYER7_TEXCOORD.xy).rg * alpha0716.y;
#endif

	return result;
}
#endif // PARALLAX_MAP || SPECULAR_MAP

#if PARALLAX_MAP && __SHADERMODEL >= 40
#if USE_RDR_STYLE_BLENDING	
half BlendTerrainHeight(float4 layerBlends, float2 texCoord0, half2 texCoord1)
{
	float result = 0.0f;

	result += layerBlends.x * tex2Dlod(heightMapSamplerLayer0, float4(texCoord0, 0.0f, 0.0f));
	result += layerBlends.y * tex2Dlod(heightMapSamplerLayer1, float4(texCoord0, 0.0f, 0.0f));
	result += layerBlends.z * tex2Dlod(heightMapSamplerLayer2, float4(texCoord0, 0.0f, 0.0f));
	result += layerBlends.w * tex2Dlod(heightMapSamplerLayer3, float4(texCoord0, 0.0f, 0.0f));

	return result;
}
#endif // USE_RDR_STYLE_BLENDING
half BlendTerrainHeight(half3 srcRGB, float2 texCoord0, half2 texCoord1)
{
	half3 msrcRGB = 1 - srcRGB;
	
	half4 alpha0716;
	half4 alpha2534;
	
	alpha0716.x = GetAlpha(srcRGB,msrcRGB,half3(0,0,0));
	alpha0716.y = GetAlpha(srcRGB,msrcRGB,half3(1,1,1));
	
	alpha0716.z = GetAlpha(srcRGB,msrcRGB,half3(0,0,1));
	alpha0716.w = GetAlpha(srcRGB,msrcRGB,half3(1,1,0));
	
	alpha2534.x = GetAlpha(srcRGB,msrcRGB,half3(0,1,0));
	alpha2534.y = GetAlpha(srcRGB,msrcRGB,half3(1,0,1));

	alpha2534.z = GetAlpha(srcRGB,msrcRGB,half3(0,1,1));
	alpha2534.w = GetAlpha(srcRGB,msrcRGB,half3(1,0,0));

	float result = tex2Dlod(heightMapSamplerLayer0, float4(LAYER0_TEXCOORD.xy, 0.0f, 0.0f)).r * alpha0716.x;

#if LAYER_COUNT > 1
	result += tex2Dlod(heightMapSamplerLayer1, float4(LAYER1_TEXCOORD.xy, 0.0f, 0.0f)).r * alpha0716.z;
#endif
#if LAYER_COUNT > 2
	result += tex2Dlod(heightMapSamplerLayer2, float4(LAYER2_TEXCOORD.xy, 0.0f, 0.0f)).r * alpha2534.x;
#endif
#if LAYER_COUNT > 3
	result += tex2Dlod(heightMapSamplerLayer3, float4(LAYER3_TEXCOORD.xy, 0.0f, 0.0f)).r * alpha2534.z;
#endif
#if LAYER_COUNT > 4
	result += tex2Dlod(heightMapSamplerLayer4, float4(LAYER4_TEXCOORD.xy, 0.0f, 0.0f)).r * alpha2534.w;
#endif
#if LAYER_COUNT > 5
	result += tex2Dlod(heightMapSamplerLayer5, float4(LAYER5_TEXCOORD.xy, 0.0f, 0.0f)).r * alpha2534.y;
#endif
#if LAYER_COUNT > 6
	result += tex2Dlod(heightMapSamplerLayer6, float4(LAYER6_TEXCOORD.xy, 0.0f, 0.0f)).r * alpha0716.w;
#endif
#if LAYER_COUNT > 7
	result += tex2Dlod(heightMapSamplerLayer7, float4(LAYER7_TEXCOORD.xy, 0.0f, 0.0f)).r * alpha0716.y;
#endif

	return result;
}

#if USE_RDR_STYLE_BLENDING
float2 GetBlendedScaleBiasValue(float4 layerBlends)
{
	float2 result = float2(0.0f, 0.0f);

	result += layerBlends.x * float2(heightScale0, heightBias0);
	result += layerBlends.y * float2(heightScale1, heightBias1);
	result += layerBlends.z * float2(heightScale2, heightBias2);
	result += layerBlends.w * float2(heightScale3, heightBias3);
	
	return result;
}
#endif // USE_RDR_STYLE_BLENDING
float2 GetBlendedScaleBiasValue(half3 srcRGB)
{
	half3 msrcRGB = 1 - srcRGB;
	
	half4 alpha0716;
	half4 alpha2534;
	
	alpha0716.x = GetAlpha(srcRGB,msrcRGB,half3(0,0,0));
	alpha0716.y = GetAlpha(srcRGB,msrcRGB,half3(1,1,1));
	
	alpha0716.z = GetAlpha(srcRGB,msrcRGB,half3(0,0,1));
	alpha0716.w = GetAlpha(srcRGB,msrcRGB,half3(1,1,0));
	
	alpha2534.x = GetAlpha(srcRGB,msrcRGB,half3(0,1,0));
	alpha2534.y = GetAlpha(srcRGB,msrcRGB,half3(1,0,1));

	alpha2534.z = GetAlpha(srcRGB,msrcRGB,half3(0,1,1));
	alpha2534.w = GetAlpha(srcRGB,msrcRGB,half3(1,0,0));

	float2 result = float2(heightScale0, heightBias0) * alpha0716.x;

#if LAYER_COUNT > 1
	result += float2(heightScale1, heightBias1) * alpha0716.z;
#endif
#if LAYER_COUNT > 2
	result += float2(heightScale2, heightBias2) * alpha2534.x;
#endif
#if LAYER_COUNT > 3
	result += float2(heightScale3, heightBias3) * alpha2534.z;
#endif
#if LAYER_COUNT > 4
	result += float2(heightScale4, heightBias4) * alpha2534.w;
#endif
#if LAYER_COUNT > 5
	result += float2(heightScale5, heightBias5) * alpha2534.y;
#endif
#if LAYER_COUNT > 6
	result += float2(heightScale6, heightBias6) * alpha0716.w;
#endif
#if LAYER_COUNT > 7
	result += float2(heightScale7, heightBias7) * alpha0716.y;
#endif

	return result;
	
}

static const int numberOfSamples = 7;

#if USE_RDR_STYLE_BLENDING
float CalculateSelfShadowTraceHeight(float heightRef, float2 texCoord0, float2 texCoord1, float4 blends, float3 tanLightDirection, float scale)
#else
float CalculateSelfShadowTraceHeight(float heightRef, float2 texCoord0, float2 texCoord1, half3 blends, float3 tanLightDirection, float scale)
#endif // USE_RDR_STYLE_BLENDING
{
	float2 inXY = (tanLightDirection.xy / tanLightDirection.z) * scale;

	float maxValue = 0.0f;
	for(int i = 0; i < numberOfSamples; ++i)
	{
		float offset = 1.0f - float(i) / float(numberOfSamples);
		float currentValue = (BlendTerrainHeight(blends, texCoord0 + inXY * offset, texCoord1 + inXY * offset).r - heightRef - offset ) *  (i + 1);
		maxValue = max(maxValue, currentValue);
	}

	float shadowAmount = saturate( maxValue ) * parallaxSelfShadowAmount;
	return 1.0f - shadowAmount;
}

#if USE_RDR_STYLE_BLENDING
float3 CalculateParallaxOffset(int numberOfSteps, half3 tanEyePos, float4 blends, float2 texCoord0, half2 texCoord1, float edgeWeight, float zLimit, float3 lightDir, bool computeShadow)
#else
float3 CalculateParallaxOffset(int numberOfSteps, half3 tanEyePos, half3 blends, float2 texCoord0, half2 texCoord1, float edgeWeight, float zLimit, float3 lightDir, bool computeShadow)
#endif // USE_RDR_STYLE_BLENDING
{
	// It seems splitting the early out in 3 passes is a bit faster (0.8ms average on my test scene)
	if(numberOfSteps <= 0 || POMDisable)
	{
		return float3(0.0f, 0.0f, 1.0f);
	}

	float globalScale = globalHeightScale * edgeWeight;

	if(globalScale > 0.0f)
	{
		float2 scaleBias = GetBlendedScaleBiasValue(blends);
		float maxScale = scaleBias.x * globalScale;

		if(maxScale == 0.0f)
		{
			return float3(0.0f, 0.0f, 1.0f);
		}

		float heightStep = 1.0f / float(numberOfSteps);

		tanEyePos = normalize(tanEyePos);
		float clampedZ = max(zLimit, tanEyePos.z);

		float2 maxParallaxOffset = (-tanEyePos.xy / clampedZ) * maxScale;
		float2 heightBiasOffset =  (tanEyePos.xy / clampedZ) * scaleBias.y * globalScale;
		float2 offsetPerStep = maxParallaxOffset * heightStep;

		float currentHeight	= 1.0f;
		float previousHeight = currentHeight;

		float2 texCoordOffset = heightBiasOffset;
		
		float baseHeight = BlendTerrainHeight(blends, texCoord0, texCoord1).r + 1e-6f;
		float terrainHeight = baseHeight;
		float previousTerrainHeight = terrainHeight;
		
		for(int i = 0; i < numberOfSteps; ++i)
		{
			if( terrainHeight < currentHeight)
			{
				previousHeight = currentHeight;
				previousTerrainHeight = terrainHeight;

				currentHeight -= heightStep;
				texCoordOffset += offsetPerStep;
				terrainHeight = BlendTerrainHeight(blends, texCoord0 + texCoordOffset, texCoord1 + texCoordOffset);
			}
			else
			{
				i = numberOfSteps; // get out of here.
			}
		}

		// Interpolate between the two points to find a more precise height
		float currentDelta = currentHeight - terrainHeight;
		float previousDelta = previousHeight - previousTerrainHeight;
		float denominator = previousDelta - currentDelta;

		float finalTerrainHeight = 1.0f;

		if(denominator != 0.0f)
		{
			finalTerrainHeight =  (currentHeight * previousDelta - previousHeight * currentDelta) / denominator;
		}

		float3 result;
		result.xy = heightBiasOffset + (maxParallaxOffset * (1.0f - finalTerrainHeight));
		result.z = 1.0;
		
#if SELF_SHADOW
		if(computeShadow && parallaxSelfShadowAmount > 0.0f)
		{
			result.z = CalculateSelfShadowTraceHeight(baseHeight, texCoord0, texCoord1, blends, lightDir, maxScale);
		}
#endif

		return result;
	}

	return float3(0.0f, 0.0f, 1.0f);
}
#endif // PARALLAX_MAP
#endif // NORMAL_MAP

#if WETNESS_MULTIPLIER
half BlendWetnessMultiplier( half3 srcRGB )
{
	half result;
#if PER_MATERIAL_WETNESS_MULTIPLIER
	half3 msrcRGB = 1 - srcRGB;
	
	half4 alpha0716;
	half4 alpha2534;
	
	alpha0716.x = GetAlpha(srcRGB,msrcRGB,half3(0,0,0));
	alpha0716.y = GetAlpha(srcRGB,msrcRGB,half3(1,1,1));
	
	alpha0716.z = GetAlpha(srcRGB,msrcRGB,half3(0,0,1));
	alpha0716.w = GetAlpha(srcRGB,msrcRGB,half3(1,1,0));
	
	alpha2534.x = GetAlpha(srcRGB,msrcRGB,half3(0,1,0));
	alpha2534.y = GetAlpha(srcRGB,msrcRGB,half3(1,0,1));

	alpha2534.z = GetAlpha(srcRGB,msrcRGB,half3(0,1,1));
	alpha2534.w = GetAlpha(srcRGB,msrcRGB,half3(1,0,0));
	
	result = materialWetnessMultiplier.x *alpha0716.x;
#if LAYER_COUNT > 1
	result += materialWetnessMultiplier.y *alpha0716.z;
#endif
#if LAYER_COUNT > 2
	result += materialWetnessMultiplier.z *alpha2534.x;
#endif	
#if LAYER_COUNT > 3
	result += materialWetnessMultiplier.w *alpha2534.z;
#endif	

#else // PER_MATERIAL_WETNESS_MULTIPLIER

	result = wetnessMultiplier;

#endif // PER_MATERIAL_WETNESS_MULTIPLIER



	return result;
}
#endif

#if USE_RDR_STYLE_BLENDING
float4 GetBlendWeightsFromTexture(float2 texCoord)
{
	float4 texel = tex2D(lookupSampler, texCoord).xyzw;
	return float4( texel.xyz, saturate( 1 - (texel.x + texel.y + texel.z) ) );	
}

float4 GetBlendWeights(float2 texCoord, float4 cpvLookup)
{
	float4 mapBlendWeights = GetBlendWeightsFromTexture(texCoord.xy);
	float4 cpvBlendWeights = float4( cpvLookup.rgb, saturate( 1.0f - (cpvLookup.r + cpvLookup.g + cpvLookup.b) ) );
	return lerp( mapBlendWeights, cpvBlendWeights, cpvLookup.a);
}

comboTexel GetTexel_(float2 texcoord, float viewDistance, float4 cpvLookup, float4 cpvTint, half3 worldNormal, half3 worldBinormal, half3 worldTangent)
{
	comboTexel result;

	/********************* Calculate uvs and blend weights ********************/
	float2 unique_uv = texcoord.xy; // 0 -> 1 only (probably warped/relaxed)
	float2 tile_uv  =  texcoord.xy; //we have the same so just keep it

	float4 blendWeights = GetBlendWeights(unique_uv, cpvLookup);

	// Weight the blendWeights by heights for height based blending
	float4 normalTex1 = tex2D( BumpSampler_layer0, tile_uv );
	float4 normalTex2 = tex2D( BumpSampler_layer1, tile_uv );
	float4 normalTex3 = tex2D( BumpSampler_layer2, tile_uv );
	float4 normalTex4 = tex2D( BumpSampler_layer3, tile_uv );
	float4 bw = blendWeights * pow( blendWeights * float4( normalTex1.a, normalTex2.a, normalTex3.a, normalTex4.a ), /*IN.BlendStrength*/10);
	if( dot( bw, bw ) > 0 )
	{
		blendWeights = normalize( bw );
	}

	/******************* Diffuse *******************************************/
	float4 diffuse = BlendTerrainColor(blendWeights,tile_uv);

	// Tint diffuse
	float3 mapTintDiffuse = tex2D( tintSampler, unique_uv ).rgb;
	float3 tintColor = lerp( mapTintDiffuse, cpvTint.rgb, cpvTint.a );
	float3 tintedDiffuse = lerp( diffuse.rgb, rageGetLuminance(diffuse.rgb) * tintColor * TintIntensity, TintBlendAmount );
	float  tintDistanceBlendAmount = clamp((viewDistance - TerrainTintNear) / TerrainTintFar, 0.0f, 1.0f);
	result.color = lerp( tintedDiffuse, tintColor, tintDistanceBlendAmount );
	/****************** End Diffuse ******************************************/


	/*********************** Normal *******************************/
	float3 Snorm = 0;
	Snorm += normalTex1.xyz * blendWeights.x;
	Snorm += normalTex2.xyz * blendWeights.y;
	Snorm += normalTex3.xyz * blendWeights.z;
	Snorm += normalTex4.xyz * blendWeights.w;

	float3 normal = CalculateWorldNormal(Snorm.xy, bumpiness, worldTangent, worldBinormal, worldNormal.xyz);

	// "Tint" normal
	float3 mapTintNormal = CalculateWorldNormal( tex2D( tintBumpSampler, unique_uv ).xy, tintBumpiness, worldTangent, worldBinormal, worldNormal.xyz );	
	float3 tintedNormal = lerp( normal, mapTintNormal, 0.5f );	
	result.normal  = normalize( lerp( tintedNormal, mapTintNormal, tintDistanceBlendAmount ) );
//	/************************** End Normal ******************************/

	result.wetnessMult = dot( materialWetnessMultiplier, blendWeights );

	return result;
}
#endif // USE_RDR_STYLE_BLENDING

#if USE_TEXTURE_LOOKUP
comboTexel GetTexel_(float4 texcoord, half3 worldNormal, half3 worldBinormal, half3 worldTangent)
{
	comboTexel result;

	half3 lookup = h3tex2D(lookupSampler,texcoord.zw).rgb;
	
	result.color = BlendTerrainColor(lookup,texcoord.xy,texcoord.xy).rgb;
#if NORMAL_MAP
	result.normal = CalculateWorldNormal(BlendTerrainNormal(lookup,texcoord.xy,texcoord.xy).xy, bumpiness, worldTangent, worldBinormal, worldNormal);
#else // NORMAL_MAP
	result.normal = worldNormal;	
#endif // NORMAL_MAP

#if WETNESS_MULTIPLIER
	result.wetnessMult = BlendWetnessMultiplier(lookup);
#endif // WETNESS_MULTIPLIER

	return result;
}
#endif // USE_TEXTURE_LOOKUP

comboTexel GetTexel_(float2 texcoord, half3 lookup, half3 worldNormal, half3 worldBinormal, half3 worldTangent)
{
	comboTexel result;

	result.color = BlendTerrainColor(lookup,texcoord.xy,texcoord.xy).rgb;
#if NORMAL_MAP
	result.normal = CalculateWorldNormal(BlendTerrainNormal(lookup,texcoord.xy,texcoord.xy).xy, bumpiness, worldTangent, worldBinormal, worldNormal);
#else // NORMAL_MAP
	result.normal = worldNormal;	
#endif // NORMAL_MAP
#if WETNESS_MULTIPLIER
	result.wetnessMult = BlendWetnessMultiplier(lookup);
#endif // WETNESS_MULTIPLIER

	return result;
}

comboTexel GetTexel_(float4 texcoord, half3 lookup, half3 worldNormal, half3 worldBinormal, half3 worldTangent)
{
	comboTexel result;

	result.color = BlendTerrainColor(lookup,texcoord.xy,texcoord.zw).rgb;
#if NORMAL_MAP
	result.normal = CalculateWorldNormal(BlendTerrainNormal(lookup,texcoord.xy,texcoord.zw).xy, bumpiness, worldTangent, worldBinormal, worldNormal);
#else
	result.normal = worldNormal;	
#endif // NORMAL_MAP
#if WETNESS_MULTIPLIER
	result.wetnessMult = BlendWetnessMultiplier(lookup);
#endif // WETNESS_MULTIPLIER

	return result;
}

#if USE_TEXTURE_LOOKUP
comboTexel GetTexel_(float4 texcoord, half3 worldNormal)
{
	comboTexel result;

	half3 lookup = h3tex2D(lookupSampler,texcoord.zw).rgb;
	
	result.color = BlendTerrainColor(lookup,texcoord.xy,texcoord.xy).rgb;
	result.normal = worldNormal;	

#if WETNESS_MULTIPLIER
	result.wetnessMult = BlendWetnessMultiplier(lookup);
#endif // WETNESS_MULTIPLIER
	return result;
}
#endif // USE_TEXTURE_LOOKUP

#if USE_DOUBLE_LOOKUP
comboTexel GetTexel_(float4 texcoord, half3 lookupV, half blend,
					half3 worldNormal, half3 worldBinormal, half3 worldTangent)
{
	comboTexel result;

	half3 lookupP = h3tex2D(lookupSampler,texcoord.zw).rgb;
	half3 lookup = lerp(lookupP,lookupV.rgb,blend);
	
	result.color = BlendTerrainColor(lookup,texcoord.xy,texcoord.xy).rgb;
#if NORMAL_MAP
	result.normal = CalculateWorldNormal(BlendTerrainNormal(lookup,texcoord.xy,texcoord.xy).xy, bumpiness, worldTangent, worldBinormal, worldNormal);
#else // NORMAL_MAP
	result.normal = worldNormal;	
#endif // NORMAL_MAP
#if WETNESS_MULTIPLIER
	result.wetnessMult = BlendWetnessMultiplier(lookup);
#endif // WETNESS_MULTIPLIER

	return result;
}

#if TINT_TEXTURE
comboTexel GetTexel_(float4 texcoord, float2 tintTexCoord, float tintDistance, half3 lookupV, half blend,
					half3 worldNormal, half3 worldBinormal, half3 worldTangent)
{
	comboTexel result;

	half3 lookupP = h3tex2D(lookupSampler,texcoord.zw).rgb;
	half3 lookup = lerp(lookupP,lookupV.rgb,blend);

	result.color  = BlendTerrainColorTint(lookup,texcoord.xy,texcoord.xy,tintTexCoord.xy).rgb;
#if TINT_TEXTURE_DISTANCE_BLEND
	float3 tintColorOnly = tex2D(tintSampler, tintTexCoord.xy);
	float distanceBlend = clamp((tintDistance - TerrainTintNear) / TerrainTintFar, 0.0f, 1.0f);

	result.color = lerp(result.color, tintColorOnly, distanceBlend);
#endif // TINT_TEXTURE_DISTANCE_BLEND
#if NORMAL_MAP
#if TINT_TEXTURE_NORMAL
	float3 tanNormal = BlendTerrainNormal(lookup,texcoord.xy,texcoord.xy);
	float3 tanTintNormal = tex2D_NormalMap(tintBumpSampler, tintTexCoord.xy);

	float normalBlend = h4tex2D(tintSampler, tintTexCoord.xy).a;

#if TINT_TEXTURE_DISTANCE_BLEND
	normalBlend = lerp(normalBlend, 1.0f, distanceBlend);
#endif // TINT_TEXTURE_DISTANCE_BLEND
	
	float3 tanBlendedNormal;
	tanBlendedNormal = ReorientedNormalBlend(tanNormal, tanTintNormal, normalBlend);
	result.normal = CalculateWorldNormal(tanBlendedNormal.xy, lerp(bumpiness, tintBumpiness, normalBlend), worldTangent, worldBinormal, worldNormal);
#else
	result.normal = CalculateWorldNormal(BlendTerrainNormal(lookup,texcoord.xy,texcoord.xy).xy, bumpiness, worldTangent, worldBinormal, worldNormal);
#endif // TINT_TEXTURE_NORMAL
#else // NORMAL_MAP
	result.normal = worldNormal;
#endif // NORMAL_MAP
#if WETNESS_MULTIPLIER
	result.wetnessMult = BlendWetnessMultiplier(lookup);

#endif // WETNESS_MULTIPLIER

	return result;
}

#endif

comboTexel GetTexel_(float4 texcoord, half3 lookupV, half blend, half3 worldNormal)
{
	comboTexel result;

	half3 lookupP = h3tex2D(lookupSampler,texcoord.zw).rgb;
	half3 lookup = lerp(lookupP,lookupV.rgb,blend);
	
	result.color = BlendTerrainColor(lookup,texcoord.xy,texcoord.xy).rgb;
	result.normal = worldNormal;	
#if WETNESS_MULTIPLIER
	result.wetnessMult = BlendWetnessMultiplier(lookup);
#endif // WETNESS_MULTIPLIER

	return result;
}

#endif // USE_DOUBLE_LOOKUP


comboTexel GetTexel_(float4 texcoord, half3 lookup, half3 worldNormal)
{
	comboTexel result;

	result.color = BlendTerrainColor(lookup,texcoord.xy,texcoord.zw).rgb;
	result.normal = worldNormal;	
#if WETNESS_MULTIPLIER
	result.wetnessMult = BlendWetnessMultiplier(lookup);
#endif // WETNESS_MULTIPLIER

	return result;
}

comboTexel GetTexel_(float2 texcoord, half3 lookup, half3 worldNormal)
{
	comboTexel result;

	result.color = BlendTerrainColor(lookup,texcoord.xy,texcoord.xy).rgb;
	result.normal = worldNormal;	
#if WETNESS_MULTIPLIER
	result.wetnessMult = BlendWetnessMultiplier(lookup);
#endif // WETNESS_MULTIPLIER

	return result;
}
#endif //_USE_RDR2_STYLE_TEXTURING

comboTexel GetTexel_deferred(vertexOutputDeferred IN)
{
	return GetTexel_(	
						 IN.texCoord
#if TINT_TEXTURE
#if SEPERATE_TINT_TEXTURE_COORDS
						,IN.tintTexCoord
#endif // SEPERATE_TINT_TEXTURE_COORDS
#if TINT_TEXTURE_DISTANCE_BLEND
						,IN.viewDistance
#else
						,0.0f
#endif // TINT_TEXTURE_DISTANCE_BLEND
#endif // TINT_TEXTURE
#if USE_RDR_STYLE_BLENDING
						,IN.cpvLookup
						,IN.cpvTint
#else
#if !USE_TEXTURE_LOOKUP || USE_DOUBLE_LOOKUP
						,IN.lookup.rgb
#endif // !USE_TEXTURE_LOOKUP || USE_DOUBLE_LOOKUP
#if USE_DOUBLE_LOOKUP
						,IN.lookup.a
#endif // USE_DOUBLE_LOOKUP
#endif // USE_RDR_STYLE_BLENDING
						,IN.worldNormal.xyz
#if NORMAL_MAP
						,IN.worldBinormal
						,IN.worldTangent 
#endif // NORMAL_MAP						
					 );
}


comboTexel GetTexel_unlit(vertexOutputUnlit IN)
{
	return GetTexel_(	
						IN.texCoord
#if !USE_TEXTURE_LOOKUP || USE_DOUBLE_LOOKUP
						,IN.lookup.rgb
#endif // !USE_TEXTURE_LOOKUP || USE_DOUBLE_LOOKUP
#if USE_DOUBLE_LOOKUP
						,IN.lookup.a
#endif // USE_DOUBLE_LOOKUP
						,half3(0,0,0) // don't need normal
					);
}

comboTexel GetTexel(vertexOutput IN)
{
	return GetTexel_(	
						IN.texCoord
#if !USE_TEXTURE_LOOKUP || USE_DOUBLE_LOOKUP
						,IN.lookup.rgb
#endif // !USE_TEXTURE_LOOKUP || USE_DOUBLE_LOOKUP
#if USE_DOUBLE_LOOKUP
						,IN.lookup.a
#endif // USE_DOUBLE_LOOKUP
						,IN.worldNormal.xyz
#if NORMAL_MAP						
						,IN.worldBinormal
						,IN.worldTangent 
#endif // NORMAL_MAP						
					 );
}

comboTexel GetTexelCube(vertexTerrainOutputCube IN)
{
	return GetTexel_(
		IN.texCoord
#if !USE_TEXTURE_LOOKUP || USE_DOUBLE_LOOKUP
		,IN.lookup.rgb
#endif // !USE_TEXTURE_LOOKUP || USE_DOUBLE_LOOKUP
#if USE_DOUBLE_LOOKUP
		,IN.lookup.a
#endif // USE_DOUBLE_LOOKUP
		,IN.worldNormal.xyz
		);
}


comboTexel GetTexel_lightweight0(vertexOutput_lightweight0 IN)
{
	return GetTexel_(
						IN.texCoord
#if !USE_TEXTURE_LOOKUP || USE_DOUBLE_LOOKUP
						,IN.lookup.rgb
#endif // !USE_TEXTURE_LOOKUP || USE_DOUBLE_LOOKUP
#if USE_DOUBLE_LOOKUP
						,IN.lookup.a
#endif // USE_DOUBLE_LOOKUP
						,IN.worldNormal.xyz
					);
}

#if WATER_REFLECTION_TECHNIQUES
comboTexel GetTexelWaterReflection(vertexOutputWaterReflection IN)
{
	return GetTexel_(
						IN.texCoord
#if !USE_TEXTURE_LOOKUP || USE_DOUBLE_LOOKUP
						,IN.lookup.rgb
#endif // !USE_TEXTURE_LOOKUP || USE_DOUBLE_LOOKUP
#if USE_DOUBLE_LOOKUP
						,IN.lookup.a
#endif // USE_DOUBLE_LOOKUP
						,IN.worldNormal.xyz
					);
}
#endif // WATER_REFLECTION_TECHNIQUES

#if SELF_SHADOW
// fake bump self shadow helpers
half BumpShadowSetup(half3 worldNormal)
{
	half BumpShadow = saturate(dot(worldNormal, -gDirectionalLight.xyz))- bumpSelfShadowAmount;
	return(BumpShadow * 1.5 * 6.0);
}

half BumpShadowApply(half BumpShadow, half3 DiffuseColor)
{
	half H = (0.5 - dot(DiffuseColor.rgb, half3(0.2125, 0.7154, 0.0721)*6.0) );

#define DISABLE	 // +1000.0 // uncomment the +1000 to disable bump self shadow for shader reloading tests
	half SelfShadow = saturate(BumpShadow - H DISABLE);
	return(SelfShadow);
}
#endif // SELF_SHADOW

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//
//
//
vertexOutput	VS_Transform(vertexInput IN)
{
	vertexOutput OUT;

    // Write out final position & texture coords
	float3	localPos	= IN.pos;
	float3	localNormal	= IN.normal;
    float4	projPos		= mul(float4(localPos,1), gWorldViewProj);
	float3  worldPos	= mul(float4(localPos,1), gWorld).xyz;
#if (SPECULAR && TERRAIN_SHADER_WORLD_EYE_POS) || PARALLAX_MAP
	float3	worldEyePos = gViewInverse[3].xyz - worldPos;
#endif // (SPECULAR && TERRAIN_SHADER_WORLD_EYE_POS) || PARALLAX_MAP

	// Transform normal by "transpose" matrix
	float3 worldNormal	= NORMALIZE_VS(mul(localNormal, (float3x3)gWorld));

	OUT.worldPos		= worldPos;
    OUT.pos				= projPos;
	OUT.worldNormal		= worldNormal;

#if DOUBLEUV_SET
	OUT.texCoord		= float4(IN.texCoord0,IN.texCoord1);
#else
	OUT.texCoord		= IN.texCoord0;
#endif	

#if !USE_TEXTURE_LOOKUP || USE_DOUBLE_LOOKUP
	OUT.lookup			= IN.specular;
#endif // !USE_TEXTURE_LOOKUP || USE_DOUBLE_LOOKUP
#if USE_DOUBLE_LOOKUP
	OUT.lookup.a		= IN.diffuse.a;
#endif
#if PALETTE_TINT
	OUT.tintColor	= UnpackTintPalette(IN.diffuse);
#endif
#if USE_RDR_STYLE_BLENDING
	OUT.cpvTint = IN.specular;
	OUT.cpvLookup.xy = IN.blendWeight0.xy;
	OUT.cpvLookup.zw = IN.blendWeight1.xy;
#endif
#if SEPERATE_TINT_TEXTURE_COORDS
	OUT.tintTexCoord	= IN.tintTexCoord;
#endif
	OUT.color0			= saturate(IN.diffuse);

#if SPECULAR && TERRAIN_SHADER_WORLD_EYE_POS || PARALLAX_MAP
	OUT.worldEyePos		= worldEyePos;
#endif // SPECULAR && TERRAIN_SHADER_WORLD_EYE_POS
#if NORMAL_MAP || PARALLAX_MAP
	OUT.worldTangent	= NORMALIZE_VS(mul(IN.tangent.xyz, (float3x3)gWorld));
	OUT.worldBinormal	= rageComputeBinormal(OUT.worldNormal.xyz, OUT.worldTangent, IN.tangent.w);
#endif // NORMAL_MAP

	RAGE_COMPUTE_CLIP_DISTANCES(OUT.pos);
	
    return(OUT);
}


// the proper way to do this is to make the unlit part a sub section, but that changes too much code right now...	 
// the compiler eliminated all the unused variables for us.
vertexOutputUnlit	VS_TransformUnlit(vertexInput IN)
{
	vertexOutput LITOUT = VS_Transform( IN);
	vertexOutputUnlit OUT;

    OUT.pos	= LITOUT.pos;
	OUT.texCoord = LITOUT.texCoord;
#if !USE_TEXTURE_LOOKUP	|| USE_DOUBLE_LOOKUP
	OUT.lookup= LITOUT.lookup;
#endif
#if PALETTE_TINT
	OUT.tintColor = LITOUT.tintColor;
#endif
#if USE_RDR_STYLE_BLENDING
	OUT.cpvTint = IN.specular;
	OUT.cpvLookup.xy = IN.blendWeight0.xy;
	OUT.cpvLookup.zw = IN.blendWeight1.xy;
#endif
#if SEPERATE_TINT_TEXTURE_COORDS
	OUT.tintTexCoord = IN.tintTexCoord;
#endif
	OUT.worldPos=LITOUT.worldPos;

    return(OUT);
}


//
//
//
//
vertexOutputWaterReflection VS_TransformWaterReflection(vertexInput IN)
{
	vertexOutputWaterReflection OUT;

	// Write out final position & texture coords
	float3 localPos		= IN.pos;
	float3 localNormal	= IN.normal;
	float4 pos			= mul(float4(localPos,1), gWorldViewProj);
	float3 worldPos		= mul(float4(localPos,1), gWorld).xyz;
	float3 worldNormal	= NORMALIZE_VS(mul(localNormal, (float3x3)gWorld));

	OUT.worldPos		= worldPos;
	OUT.pos				= pos;
	OUT.worldNormal		= worldNormal;

#if DOUBLEUV_SET
	OUT.texCoord		= float4(IN.texCoord0,IN.texCoord1);
#else
	OUT.texCoord		= IN.texCoord0;
#endif	

#if !USE_TEXTURE_LOOKUP || USE_DOUBLE_LOOKUP
	OUT.lookup			= IN.specular;
#endif // !USE_TEXTURE_LOOKUP || USE_DOUBLE_LOOKUP
#if USE_DOUBLE_LOOKUP
	OUT.lookup.a		= IN.diffuse.a;
#endif
#if PALETTE_TINT
	OUT.tintColor	= UnpackTintPalette(IN.diffuse);
#endif
#if SEPERATE_TINT_TEXTURE_COORDS
	OUT.tintTexCoord	= IN.tintTexCoord;
#endif
	OUT.color0			= saturate(IN.diffuse);

	RAGE_COMPUTE_CLIP_DISTANCES(OUT.pos);

	return(OUT);
}


#if __MAX
//
//
// Pass through the info from Max down to our default vertex shader...
//
vertexOutput VS_MaxTransform(maxVertexInput IN)
{
vertexInput input;

	input.pos		= IN.pos;
	input.diffuse	= IN.diffuse;
	input.diffuse.a = IN.diffuseAlpha;

#if !USE_TEXTURE_LOOKUP	|| USE_DOUBLE_LOOKUP
	input.specular	= IN.specular;
#endif // !USE_TEXTURE_LOOKUP || USE_DOUBLE_LOOKUP

	input.normal	= IN.normal;
#if NORMAL_MAP
	input.tangent	= float4(IN.tangent,1.0f);
#endif // NORMAL_MAP
	
	input.texCoord0	= Max2RageTexCoord2(IN.texCoord0.xy);

#if USE_RDR_STYLE_BLENDING
	input.specular	= IN.specular;
	input.blendWeight0 = float2(0.5f, 0.5f);
	input.blendWeight1 = float2(0.5f, 0.5f);
#endif

#if DOUBLEUV_SET
	input.texCoord1	= Max2RageTexCoord2(IN.texCoord1.xy);
#endif // DOUBLEUV_SET

#if SEPERATE_TINT_TEXTURE_COORDS
	input.tintTexCoord	= Max2RageTexCoord2(IN.texCoord2.xy);
#endif
#if USE_EDGE_WEIGHT
	input.edgeWeight = Max2RageTexCoord2(IN.texCoord3.xy);
#endif
	
	return VS_Transform(input);
}
#endif //__MAX...


//
//
//
//
vertexOutputDeferred	VS_TransformDeferred(vertexInput IN)
{
	vertexOutputDeferred OUT;

    // Write out final position & texture coords
	float3	localPos	= IN.pos;
	float3	localNormal	= IN.normal;
//	float4	outColor	= float4(1,1,1,1);	// default vertex color
//	float4	aoColor		= IN.diffuse;

    float4	projPos		= mul(float4(localPos,1), gWorldViewProj);
	float3  worldPos	= mul(float4(localPos,1), gWorld).xyz;
#if PARALLAX_MAP
	float3	worldEyePos = gViewInverse[3].xyz - worldPos;
#endif // PARALLAX_MAP

	// Transform normal by "transpose" matrix
	float3 worldNormal	= NORMALIZE_VS(mul(localNormal, (float3x3)gWorld));


    OUT.pos				= projPos;

#if DOUBLEUV_SET
	OUT.texCoord		= float4(IN.texCoord0,IN.texCoord1);
#else
	OUT.texCoord		= IN.texCoord0;
#endif	

#if USE_RDR_STYLE_BLENDING
	OUT.cpvTint = IN.specular;
	OUT.cpvLookup.xy = IN.blendWeight0.xy;
	OUT.cpvLookup.zw = IN.blendWeight1.xy;
#else // USE_RDR_STYLE_BLENDING
#if !USE_TEXTURE_LOOKUP || USE_DOUBLE_LOOKUP
	OUT.lookup			= IN.specular;
#endif // !USE_TEXTURE_LOOKUP || USE_DOUBLE_LOOKUP
#if USE_DOUBLE_LOOKUP
	OUT.lookup.a		= IN.diffuse.a;
#endif
#endif // USE_RDR_STYLE_BLENDING
#if PALETTE_TINT
	OUT.tintColor	= UnpackTintPalette(IN.diffuse);
#endif
#if SEPERATE_TINT_TEXTURE_COORDS
	OUT.tintTexCoord	= IN.tintTexCoord;
#endif // TINT_TEXTURE
#if USE_EDGE_WEIGHT
	OUT.edgeWeight.xy = IN.edgeWeight.xy;
#endif
#if PARALLAX_MAP || (TINT_TEXTURE && TINT_TEXTURE_DISTANCE_BLEND)
	float3 dir = (gViewInverse[3].xyz - worldPos);
	float3 camForward = cross(gViewInverse[0],gViewInverse[1]);

	OUT.viewDistance  = length((gViewInverse[3].xyz - camForward * POMForwardFadeOffset) - worldPos);
#endif // PARALLAX_MAP || (TINT_TEXTURE && TINT_TEXTURE_DISTANCE_BLEND)

	OUT.worldNormal.xyz	= worldNormal;
	
#if SELF_SHADOW
	OUT.worldNormal.w = BumpShadowSetup(worldNormal);
#else
	OUT.worldNormal.w = 1;
#endif

#if NORMAL_MAP || PARALLAX_MAP
	OUT.worldTangent	= NORMALIZE_VS(mul(IN.tangent.xyz, (float3x3)gWorld));
	OUT.worldBinormal	= rageComputeBinormal(OUT.worldNormal.xyz, OUT.worldTangent, IN.tangent.w);
#endif // NORMAL_MAP

#if PARALLAX_MAP
	OUT.worldEyePos.xyz =  NORMALIZE_VS(worldEyePos.xyz);
#endif // PARALLAX_MAP

	OUT.color0	= float4(IN.diffuse.rgb, 0.0);	

	// Adjust ambient bakes
	OUT.color0.rg = CalculateBakeAdjust(worldPos - gViewInverse[3].xyz, OUT.color0.rg);

    return(OUT);
}


#if TERRAIN_TESSELLATION_SHADER_CODE

vertexOutputDeferred_CtrlPoint	VS_TransformDeferred_Tess(vertexInput IN)
{
#if TEMP_RUNTIME_CALCULATE_TERRAIN_OPEN_EDGE_INFO
	float openEdgeFactor = clamp(sign(IN.pos.z), 0.0f, 1.0f);
	IN.pos.z = abs(IN.pos.z) + TERRAIN_OPEN_EDGE_INFO_COMMON_FLOOR;
#else
	float openEdgeFactor = 1.0f;
#endif

	vertexOutputDeferred_CtrlPoint OUT;
	vertexOutputDeferred TEMP = VS_TransformDeferred(IN);

	float3 localPos = IN.pos;
	OUT.projPosition = TEMP.pos;
	OUT.modelPosAndW.xyz = localPos;
	OUT.openEdgeFactor = openEdgeFactor;
	OUT.modelPosAndW.w = ComputeSceneCameraDepth(localPos);
	OUT.modelNormal = IN.normal;
#if DOUBLEUV_SET
	OUT.texCoord = TEMP.texCoord;
#else // DOUBLEUV_SET
	OUT.texCoord = TEMP.texCoord;
#endif // DOUBLEUV_SET
#if !USE_TEXTURE_LOOKUP	|| USE_DOUBLE_LOOKUP
	OUT.lookup = TEMP.lookup;
#endif // !USE_TEXTURE_LOOKUP || USE_DOUBLE_LOOKUP
#if PALETTE_TINT
	OUT.tintColor = TEMP.tintColor;
#endif
#if TINT_TEXTURE
	OUT.tintTexCoord	= IN.tintTexCoord;
#endif // TINT_TEXTURE
	OUT.worldNormal = TEMP.worldNormal;
	OUT.color0 = TEMP.color0;
#if NORMAL_MAP
	OUT.worldTangent = TEMP.worldTangent;
	OUT.worldBinormal = TEMP.worldBinormal;
#endif // NORMAL_MAP
#if PARALLAX_MAP || (TINT_TEXTURE && TINT_TEXTURE_DISTANCE_BLEND)
	OUT.viewDistance = TEMP.viewDistance;
#endif // PARALLAX_MAP || (TINT_TEXTURE && TINT_TEXTURE_DISTANCE_BLEND)
#if USE_EDGE_WEIGHT
	OUT.edgeWeight = IN.edgeWeight;
#endif // USE_EDGE_WEIGHT

    return(OUT);
}


// Patch Constant function.
PatchInfo_TerrainTessellation HS_TransformDeferred_Tess_PF(InputPatch<vertexOutputDeferred_CtrlPoint, 3> PatchPoints,  uint PatchID : SV_PrimitiveID)
{	
	PatchInfo_TerrainTessellation Output = ComputeTessellationFactors(PatchPoints[0].modelPosAndW, PatchPoints[1].modelPosAndW, PatchPoints[2].modelPosAndW);
	return Output;
}


// Hull shader.
[domain("tri")]
[partitioning("fractional_odd")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(3)]
[patchconstantfunc("HS_TransformDeferred_Tess_PF")]
[maxtessfactor(15)]
vertexOutputDeferred_CtrlPoint HS_TransformDeferred_Tess(InputPatch<vertexOutputDeferred_CtrlPoint, 3> PatchPoints, uint i : SV_OutputControlPointID, uint PatchID : SV_PrimitiveID)
{
	vertexOutputDeferred_CtrlPoint Output;

	// Our control points match the vertex shader output.
	Output = PatchPoints[i];

	return Output;
}

vertexOutputDeferred DS_TransformDeferred_Tess_Func(PatchInfo_TerrainTessellation PatchInfo, const OutputPatch<vertexOutputDeferred_CtrlPoint, 3> PatchPoints, float3 WUV, out float3 modelPositionOut)
{
	vertexOutputDeferred OUT;

	if(PatchInfo.doTessellation == 0.0f)
	{
		int idx = (int)(1.0f*WUV.x + 2.0f*WUV.y + 3.0f*WUV.z) - 1;

		OUT.pos = PatchPoints[idx].projPosition;
	#if DOUBLEUV_SET
		OUT.texCoord = PatchPoints[idx].texCoord;
	#else // DOUBLEUV_SET
		OUT.texCoord = PatchPoints[idx].texCoord;
	#endif // DOUBLEUV_SET
	#if !USE_TEXTURE_LOOKUP	|| USE_DOUBLE_LOOKUP
		OUT.lookup = PatchPoints[idx].lookup;
	#endif // !USE_TEXTURE_LOOKUP || USE_DOUBLE_LOOKUP
	#if PALETTE_TINT
		OUT.tintColor = PatchPoints[idx].tintColor
	#endif
		OUT.worldNormal = PatchPoints[idx].worldNormal;
		OUT.color0 = PatchPoints[idx].color0;
	#if NORMAL_MAP
		OUT.worldTangent = PatchPoints[idx].worldTangent;
		OUT.worldBinormal = PatchPoints[idx].worldBinormal;
	#endif // NORMAL_MAP
	#if TINT_TEXTURE
		OUT.tintTexCoord = PatchPoints[idx].tintTexCoord;
	#endif // TINT_TEXTURE
	#if USE_EDGE_WEIGHT
		OUT.edgeWeight = PatchPoints[idx].edgeWeight;
	#endif // USE_EDGE_WEIGHT
	#if PARALLAX_MAP || (TINT_TEXTURE && TINT_TEXTURE_DISTANCE_BLEND)
		OUT.viewDistance = PatchPoints[idx].viewDistance;
	#endif // PARALLAX_MAP || (TINT_TEXTURE && TINT_TEXTURE_DISTANCE_BLEND)

		modelPositionOut = PatchPoints[idx].modelPosAndW.xyz;
	}
	else
	{
	#if DOUBLEUV_SET
		OUT.texCoord = RAGE_COMPUTE_BARYCENTRIC(WUV, PatchPoints, texCoord);
	#else // DOUBLEUV_SET
		OUT.texCoord = RAGE_COMPUTE_BARYCENTRIC(WUV, PatchPoints, texCoord);
	#endif // DOUBLEUV_SET
	#if !USE_TEXTURE_LOOKUP	|| USE_DOUBLE_LOOKUP
		OUT.lookup = RAGE_COMPUTE_BARYCENTRIC(WUV, PatchPoints, lookup);
	#endif // !USE_TEXTURE_LOOKUP || USE_DOUBLE_LOOKUP
	#if PALETTE_TINT
		OUT.tintColor = RAGE_COMPUTE_BARYCENTRIC(WUV, PatchPoints, tintColor);
	#endif
		OUT.worldNormal = RAGE_COMPUTE_BARYCENTRIC(WUV, PatchPoints, worldNormal);
		OUT.worldNormal.xyz = normalize(OUT.worldNormal.xyz);
		OUT.color0 = RAGE_COMPUTE_BARYCENTRIC(WUV, PatchPoints, color0);
	#if NORMAL_MAP
		OUT.worldTangent = RAGE_COMPUTE_BARYCENTRIC(WUV, PatchPoints, worldTangent);
		OUT.worldTangent = normalize(OUT.worldTangent);
		OUT.worldBinormal = RAGE_COMPUTE_BARYCENTRIC(WUV, PatchPoints, worldBinormal);
		OUT.worldBinormal = normalize(OUT.worldBinormal);
	#endif // NORMAL_MAP
	#if TINT_TEXTURE
		OUT.tintTexCoord = RAGE_COMPUTE_BARYCENTRIC(WUV, PatchPoints, tintTexCoord);
	#endif // TINT_TEXTURE
	#if USE_EDGE_WEIGHT
		OUT.edgeWeight = RAGE_COMPUTE_BARYCENTRIC(WUV, PatchPoints, edgeWeight);
	#endif // USE_EDGE_WEIGHT
	#if PARALLAX_MAP || (TINT_TEXTURE && TINT_TEXTURE_DISTANCE_BLEND)
		OUT.viewDistance = RAGE_COMPUTE_BARYCENTRIC(WUV, PatchPoints, viewDistance);
	#endif // PARALLAX_MAP || (TINT_TEXTURE && TINT_TEXTURE_DISTANCE_BLEND)

		// Calculate the modelspace and then world space position etc.
		float4 modelPosAndW = RAGE_COMPUTE_BARYCENTRIC(WUV, PatchPoints, modelPosAndW);
		float openEdgeFactor = RAGE_COMPUTE_BARYCENTRIC(WUV, PatchPoints, openEdgeFactor);
		float3 perturbedModelSpacePosition = ComputePerturbedVertexPosition(modelPosAndW.xyz, modelPosAndW.w, openEdgeFactor);
		modelPositionOut = perturbedModelSpacePosition;
		OUT.pos = mul(float4(perturbedModelSpacePosition, 1), gWorldViewProj);
	}

#if NORMAL_MAP && PARALLAX_MAP
	OUT.worldEyePos = NORMALIZE_VS(gViewInverse[3].xyz - mul(float4(modelPositionOut,1), gWorld).xyz);
#endif // NORMAL_MAP && PARALLAX_MAP

	return OUT;
}


[domain("tri")]
vertexOutputDeferred DS_TransformDeferred_Tess(PatchInfo_TerrainTessellation PatchInfo, float3 WUV : SV_DomainLocation, const OutputPatch<vertexOutputDeferred_CtrlPoint, 3> PatchPoints)
{
	float3 unusedModelPosition;
	vertexOutputDeferred OUT = DS_TransformDeferred_Tess_Func(PatchInfo, PatchPoints, WUV, unusedModelPosition);
	return (OUT);
}

#endif // TERRAIN_TESSELLATION_SHADER_CODE


//
//
//
//

#if CUBEMAP_REFLECTION_TECHNIQUES
vertexTerrainOutputCube VS_TransformCube_Common(vertexInputCube IN
#if GS_INSTANCED_CUBEMAP
										 ,uniform bool bUseInst
#endif
										 )
{
	vertexTerrainOutputCube OUT;

	// Write out final position & texture coords
	float3 localPos		= IN.pos;
	float3 localNormal	= IN.normal;

	float4 pos;
	float3 worldPos;
	float3 worldNormal;
#if GS_INSTANCED_CUBEMAP
	if (bUseInst)
	{
		pos			= mul(float4(localPos,1), mul(gInstWorld,gInstWorldViewProj[INSTOPT_INDEX(IN.InstID)]));
		worldPos		= mul(float4(localPos,1), gInstWorld).xyz;
		worldNormal	= NORMALIZE_VS(mul(localNormal, (float3x3)gInstWorld));
	}
	else
#endif
	{
		pos			= mul(float4(localPos,1), gWorldViewProj);
		worldPos		= mul(float4(localPos,1), gWorld).xyz;
		worldNormal	= NORMALIZE_VS(mul(localNormal, (float3x3)gWorld));
	}

	OUT.worldPos		= worldPos;
	OUT.pos				= pos;
	OUT.worldNormal		= worldNormal;

#if DOUBLEUV_SET
	OUT.texCoord		= float4(IN.texCoord0,IN.texCoord1);
#else
	OUT.texCoord		= IN.texCoord0;
#endif	

	#if !USE_TEXTURE_LOOKUP || USE_DOUBLE_LOOKUP
		OUT.lookup			= IN.specular;
	#endif // !USE_TEXTURE_LOOKUP || USE_DOUBLE_LOOKUP
	#if USE_DOUBLE_LOOKUP
		OUT.lookup.a		= IN.diffuse.a;
	#endif
	#if PALETTE_TINT
		OUT.tintColor	= UnpackTintPalette(IN.diffuse);
	#endif
#if SEPERATE_TINT_TEXTURE_COORDS
	OUT.tintTexCoord	= IN.tintTexCoord;
#endif
	OUT.color0			= saturate(IN.diffuse);

	return(OUT);
}

vertexTerrainOutputCube VS_TransformCube(vertexInputCube IN)
{
	return VS_TransformCube_Common(IN
#if GS_INSTANCED_CUBEMAP	
		,false
#endif
		);
}
#endif // CUBEMAP_REFLECTION_TECHNIQUES

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//
//
//

#if UNLIT_TECHNIQUES || __MAX
float4	PS_TexturedUnlit(vertexOutputUnlit IN) : COLOR
{
	comboTexel texel = GetTexel_unlit(IN);
		
	float3 blendColor = texel.color;
#if PALETTE_TINT || PALETTE_TINT_MAX
	blendColor.rgb *= IN.tintColor.rgb;
#endif
	float4 pixelColor = float4(blendColor,globalAlpha);
	
	pixelColor = calcDepthFxBasic(pixelColor, IN.worldPos);
	
	return(pixelColor);
}
#endif // UNLIT_TECHNIQUES || __MAX

//
//
//
//
#if __MAX
// old rotten code - can we get rid of this?
half4 	PS_TexturedMax(vertexOutput IN) : COLOR
{
	float4 color0 = IN.color0;
	color0.a *= globalAlpha;

	comboTexel texel = GetTexel(IN);
#if PALETTE_TINT
	texel.color.rgb *= IN.tintColor.rgb;
#endif

	float4 pixelColor = MaxProcessSimpleLighting(IN.worldPos.xyz, texel.normal, color0, float4(texel.color.rgb,1.0f)
#if SPECULAR && TERRAIN_SHADER_WORLD_EYE_POS
#if USE_DIFFLUM_ON_SPECINT
		,normalize(IN.worldEyePos), specularFalloffMult, specularIntensityMult
#else // USE_DIFFLUM_ON_SPECINT
		,normalize(IN.worldEyePos), specularFalloffMult, specularIntensityMult
#endif // USE_DIFFLUM_ON_SPECINT
#endif // SPECULAR && TERRAIN_SHADER_WORLD_EYE_POS
		);

	pixelColor = calcDepthFxBasic(pixelColor, IN.worldPos);

	return(PackHdr(pixelColor));
}
#endif // __MAX



#if FORWARD_TERRAIN_TECHNIQUES

vertexOutput_lightweight0 VS_Transform_lightweight0(vertexInput_lightweight0 IN)
{
	vertexInput IN_;
	
	// copy vertex input without tangent
	{
		IN_.pos = IN.pos;
		IN_.diffuse = IN.diffuse;
	#if !USE_TEXTURE_LOOKUP	|| USE_DOUBLE_LOOKUP
		IN_.specular = IN.specular;
	#endif // !USE_TEXTURE_LOOKUP || USE_DOUBLE_LOOKUP

		IN_.texCoord0		= IN.texCoord0;
#if DOUBLEUV_SET
		IN_.texCoord1		= IN.texCoord1;
#endif	
		IN_.normal = IN.normal;
	#if SEPERATE_TINT_TEXTURE_COORDS
		IN_.tintTexCoord = float2(0.0f, 0.0f);
	#endif
	}

	vertexOutput OUT_ = VS_Transform(IN_);
	vertexOutput_lightweight0 OUT;

	// copy vertex output without tangent and binormal
	{
		OUT.texCoord = OUT_.texCoord;	
	#if !USE_TEXTURE_LOOKUP	|| USE_DOUBLE_LOOKUP
		OUT.lookup = OUT_.lookup;
	#endif // !USE_TEXTURE_LOOKUP || USE_DOUBLE_LOOKUP
	#if SEPERATE_TINT_TEXTURE_COORDS
		OUT.tintTexCoord = OUT_.tintTexCoord;
	#endif
	#if NORMAL_MAP
		OUT.worldTangent = float3(0.0f, 0.0f, 0.0f);
		OUT.worldBinormal = float3(0.0f, 0.0f, 0.0f);
	#endif
		OUT.pos = OUT_.pos;
		OUT.worldPos = OUT_.worldPos;
		OUT.worldNormal = OUT_.worldNormal;
		OUT.color0 = saturate(OUT_.color0);
	#if PALETTE_TINT
		OUT.tintColor = OUT_.tintColor;
	#endif

		RAGE_COMPUTE_CLIP_DISTANCES(OUT.pos);
	}

	return OUT;
}

half4 PS_Textured_lightweight0(vertexOutput_lightweight0 IN) : COLOR
{
	float4 color0 = IN.color0;
	//color0.a *= globalAlpha;

	comboTexel texel = GetTexel_lightweight0(IN);

	SurfaceProperties surfaceInfo = (SurfaceProperties)0;

	surfaceInfo.surface_baseColor			= color0;
#if PALETTE_TINT
	surfaceInfo.surface_baseColorTint		= IN.tintColor;
#endif
	surfaceInfo.surface_diffuseColor		= half4(texel.color,1.0f);
	surfaceInfo.surface_worldNormal.xyz		= texel.normal;

	// Compute some surface information.
	StandardLightingProperties surfaceStandardLightingProperties =
		DeriveLightingPropertiesForCommonSurface( surfaceInfo );

	surfaceStandardLightingProperties.diffuseColor = ProcessDiffuseColor(surfaceStandardLightingProperties.diffuseColor);

	surfaceProperties surface;
	directionalLightProperties light;
	materialProperties material;

	populateForwardLightingStructs(
		surface,
		material,
		light,
		IN.worldPos.xyz,
		surfaceInfo.surface_worldNormal.xyz,
		surfaceStandardLightingProperties);

	half4 OUT = calculateForwardLighting_WaterReflection(
		surface,
		material,
		light,
		1, // ambientScale
		1, // directionalScale
		SHADOW_RECEIVING && USE_DIRECTIONAL_LIGHT_FWD_0_LIGHTS); // directionalShadow

	OUT = calcDepthFxBasic(OUT, IN.worldPos);

	//OUT.a *= saturate((64.0f-IN.worldNormal.w)*0.25f);
	return PackHdr(half4(OUT.rgb, 1.0));
}
#endif // FORWARD_TERRAIN_TECHNIQUES

#if WATER_REFLECTION_TECHNIQUES
half4 PS_TexturedWaterReflection(vertexOutputWaterReflection IN) : COLOR
{
#if WATER_REFLECTION_CLIP_IN_PIXEL_SHADER
	rageDiscard(dot(float4(IN.worldPos.xyz, 1), gLight0PosX) < 0);
#endif // WATER_REFLECTION_CLIP_IN_PIXEL_SHADER
	half4 color0 = IN.color0;
	//color0.a *= globalAlpha;

	comboTexel texel = GetTexelWaterReflection(IN);
	SurfaceProperties surfaceInfo = (SurfaceProperties)0;

	surfaceInfo.surface_baseColor			= color0;
#if PALETTE_TINT
	surfaceInfo.surface_baseColorTint		= IN.tintColor;
#endif
	surfaceInfo.surface_diffuseColor		= half4(texel.color,1.0f);
	surfaceInfo.surface_worldNormal.xyz		= texel.normal;

	// Compute some surface information.
	StandardLightingProperties surfaceStandardLightingProperties =
		DeriveLightingPropertiesForCommonSurfaceInternal( surfaceInfo, PALETTE_TINT_WATER_REFLECTION );

	surfaceStandardLightingProperties.diffuseColor = ProcessDiffuseColor(surfaceStandardLightingProperties.diffuseColor);

	surfaceProperties surface;
	directionalLightProperties light;
	materialProperties material;

	populateForwardLightingStructs(
		surface,
		material,
		light,
		IN.worldPos.xyz,
		surfaceInfo.surface_worldNormal.xyz,
		surfaceStandardLightingProperties);

	half4 OUT = calculateForwardLighting_WaterReflection(
		surface,
		material,
		light,
		1, // ambientScale
		1, // directionalScale
		SHADOW_RECEIVING); // directionalShadow

	OUT = calcDepthFxBasic(OUT, IN.worldPos);

	//OUT.a *= saturate((64.0f-IN.worldNormal.w)*0.25f);
	return PackReflection(half4(OUT.rgb, 1.0));
}
#endif // WATER_REFLECTION_TECHNIQUES

#if __MAX
//
//
//
//
float4 PS_MaxTextured(vertexOutput IN) : COLOR
{
float4 OUT;

	if(maxLightingToggle)
	{
		OUT = PS_TexturedMax(IN);
	}
	else
	{
		vertexOutputUnlit unlitIN;
			unlitIN.pos			= IN.pos;
			unlitIN.texCoord	= IN.texCoord;
			#if !USE_TEXTURE_LOOKUP	|| USE_DOUBLE_LOOKUP
				unlitIN.lookup	= IN.lookup;
			#endif
			#if PALETTE_TINT_MAX
				unlitIN.tintColor	= IN.color0;
			#endif
			#if USE_RDR_STYLE_BLENDING
				unlitIN.cpvLookup = IN.cpvLookup;
				unlitIN.cpvTint	= IN.cpvTint;
			#endif
			#if SEPERATE_TINT_TEXTURE_COORDS
				unlitIN.tintTexCoord = IN.tintTexCoord;
			#endif
			unlitIN.worldPos	= float4(IN.worldPos,1.0f);
		OUT = PS_TexturedUnlit(unlitIN);
	}
	
	OUT.a = 1.0f;
	return(saturate(OUT));
}
#endif //__MAX...

//
//
//
//
OutHalf4Color PS_TexturedCube(vertexTerrainOutputCube IN)
{
	float4 color0 = saturate(IN.color0);
	color0.a *= globalAlpha;

	comboTexel texel = GetTexelCube(IN);
	SurfaceProperties surfaceInfo = (SurfaceProperties)0;

	surfaceInfo.surface_baseColor			= color0;
	#if PALETTE_TINT
		surfaceInfo.surface_baseColorTint		= IN.tintColor;
	#endif
	surfaceInfo.surface_diffuseColor		= float4(texel.color,1.0f);
	surfaceInfo.surface_worldNormal.xyz		= texel.normal;

	#if SPECULAR
		surfaceInfo.surface_specularExponent	= specularFalloffMult;
	#if USE_DIFFLUM_ON_SPECINT
		surfaceInfo.surface_specularIntensity	= specularIntensityMult;
	#else // USE_DIFFLUM_ON_SPECINT
	surfaceInfo.surface_specularIntensity	= specularIntensityMult;
	#endif // USE_DIFFLUM_ON_SPECINT
	#endif // SPECULAR

	// Compute some surface information.
	StandardLightingProperties surfaceStandardLightingProperties =
		DeriveLightingPropertiesForCommonSurface( surfaceInfo );

	surfaceStandardLightingProperties.diffuseColor = ProcessDiffuseColor(surfaceStandardLightingProperties.diffuseColor);

	surfaceProperties surface;
	directionalLightProperties light;
	materialProperties material;

	populateForwardLightingStructs(
		surface,
		material,
		light,
		IN.worldPos.xyz,
		surfaceInfo.surface_worldNormal.xyz,
		surfaceStandardLightingProperties);

	float4 OUT = calculateForwardLighting_DynamicReflection(
		surface,
		material,
		light);

	OUT = calcDepthFxBasic(OUT, IN.worldPos);
	return CastOutHalf4Color(PackReflection(float4(OUT.rgb, 1.0)));
}

#define NUM_POM_CTRL_POINTS (5)
//doesn't seem to work correctly on orbis if these are floats or float2s,
//they usually get padded to float4s anyway so this is just setting it explicitly
IMMEDIATECONSTANT float4 pomWeights[NUM_POM_CTRL_POINTS] = 
{
	float4(1.0f, 1.0f, 1.0f, 1.0f),
	float4(0.8f, 0.8f, 0.8f, 0.8f),
	float4(0.5f, 0.5f, 0.5f, 0.5f),
	float4(0.1f, 0.1f, 0.1f, 0.1f),
	float4(0.0f, 0.0f, 0.0f, 0.0f),
};

float ComputeDistanceFade(float distanceBlend)
{
	if(distanceBlend == 1.0f)
		return 0.0f;

	//find the nearest weights
	int startPointWeight = clamp(int(distanceBlend * NUM_POM_CTRL_POINTS), 0, NUM_POM_CTRL_POINTS - 1);
	int endPointWeight = clamp(startPointWeight + 1, 0, NUM_POM_CTRL_POINTS - 1);
	
	//work out how afr between the weight we are so we can interpolate between them
	return lerp(pomWeights[startPointWeight].x, 
							pomWeights[endPointWeight].x,
							float((distanceBlend * NUM_POM_CTRL_POINTS) - int(distanceBlend * NUM_POM_CTRL_POINTS)));
}

//
//
//
//
DeferredGBuffer	PS_TexturedDeferred(vertexOutputDeferred IN)
{
	// Compute some surface information.
	StandardLightingProperties surfLightingProperties;

#if PARALLAX_MAP || SPECULAR_MAP
#if USE_RDR_STYLE_BLENDING
	float4 blends = GetBlendWeights(IN.texCoord.xy, IN.cpvLookup);
#else
#if USE_DOUBLE_LOOKUP
	half3 lookupP = h3tex2D(lookupSampler,IN.texCoord.zw).rgb;
	half3 blends = lerp(lookupP,IN.lookup.rgb,IN.lookup.a);
#elif USE_TEXTURE_LOOKUP
	half3 blends = h3tex2D(lookupSampler,IN.texCoord.zw).rgb;
#else
	half3 blends = IN.lookup.rgb;
#endif
#endif // USE_RDR_STYLE_BLENDING
#endif // PARALLAX_MAP || SPECULAR_MAP

#if PARALLAX_MAP && __SHADERMODEL >= 40
	float VdotN = abs(dot(normalize(IN.worldEyePos.xyz), normalize(IN.worldNormal.xyz)));

	float3 tanEyePos;

	tanEyePos.x = dot(IN.worldTangent.xyz	,	IN.worldEyePos.xyz);
	tanEyePos.y = dot(IN.worldBinormal.xyz	,	IN.worldEyePos.xyz);
	tanEyePos.z = dot(IN.worldNormal.xyz	,	IN.worldEyePos.xyz);
	
#if USE_EDGE_WEIGHT
	float edgeWeight = 1.0f - clamp(IN.edgeWeight.x, 0.0f, 1.0f);
	float zLimit = 1.0f - clamp(IN.edgeWeight.y, 0.1f, 1.0f);
#else
	float edgeWeight = 1.0f;
	float zLimit = 0.1f;
#endif

	float blendStartDistance = POMDistanceStart;
	float blendEndDistance = POMDistanceEnd;

	float distanceBlend = clamp((IN.viewDistance - blendStartDistance) / blendEndDistance, 0.0f, 1.0f);

	float scaleOutRange = (blendEndDistance - blendStartDistance) * 0.35f;
	float weightDistanceBlend = clamp(((IN.viewDistance - blendStartDistance) - blendEndDistance + scaleOutRange) / scaleOutRange, 0.0f, 1.0f);

	float numberOfSteps = lerp(POMMaxSteps, POMMinSteps, VdotN);

	//reduce the number of steps over a distance, the artifacts should be less noticeable
	//also blend out to zero displacement, otherwise you get visual artifacts when steps drop to zero
	if(POMDistanceBasedStep)
	{		
		//blend out the weight as we get close to zero point
		edgeWeight *= (1.0f - weightDistanceBlend);
		numberOfSteps *= ComputeDistanceFade(distanceBlend);

	}

	edgeWeight *= saturate(numberOfSteps - 1.0f) * saturate(VdotN / POMVDotNBlendFactor);

	float3 tanLightDirection = 0.0f;

	bool computeShadow = false;

#if SELF_SHADOW
	tanLightDirection.x = dot(IN.worldTangent.xyz,  gDirectionalLight.xyz);
	tanLightDirection.y = dot(IN.worldBinormal.xyz, gDirectionalLight.xyz);
	tanLightDirection.z = dot(IN.worldNormal.xyz, gDirectionalLight.xyz);
	
	tanLightDirection = normalize(tanLightDirection);

	if(dot(IN.worldNormal.xyz, tanLightDirection.xyz) > 0.0f)
	{
		computeShadow = true;
	}
#endif

#if DOUBLEUV_SET && !USE_TEXTURE_LOOKUP
	float3 textCoordOffset = CalculateParallaxOffset(numberOfSteps, tanEyePos, blends, IN.texCoord.xy, IN.texCoord.zw, edgeWeight, zLimit, tanLightDirection, computeShadow);
	IN.texCoord.xy += textCoordOffset.xy;
	IN.texCoord.zw += textCoordOffset.xy;
#else
	float3 textCoordOffset = CalculateParallaxOffset(numberOfSteps, tanEyePos, blends, IN.texCoord.xy, IN.texCoord.xy, edgeWeight, zLimit, tanLightDirection, computeShadow);
	IN.texCoord.xy += textCoordOffset.xy;
#endif  // DOUBLEUV_SET && !USE_TEXTURE_LOOKUP

#if SELF_SHADOW
	float POMSelfShadow = textCoordOffset.z;
#endif

#if SEPERATE_TINT_TEXTURE_COORDS
	IN.tintTexCoord.xy += textCoordOffset;
#endif // TINT_TEXTURE

#endif // PARALLAX_MAP && __SHADERMODEL >= 40
	comboTexel texel = GetTexel_deferred(IN);

	surfLightingProperties.diffuseColor	= half4(texel.color.rgb, 1.0f);
#if PALETTE_TINT
	surfLightingProperties.diffuseColor.rgb	*= IN.tintColor.rgb;
#endif
	surfLightingProperties.naturalAmbientScale	= IN.color0.r;
	surfLightingProperties.artificialAmbientScale= IN.color0.g;
#if WETNESS_MULTIPLIER
	surfLightingProperties.wetnessMult = texel.wetnessMult;
#endif 	

	// Calculate fresnel
	surfLightingProperties.fresnel = 0.98f;

	// Specular contribution
	surfLightingProperties.specularSkin = 0.0f;
#if SPECULAR_MAP
		float specMult = BlendTerrainHeightAndSpec(blends, IN.texCoord.xy, IN.texCoord.xy).g;
		surfLightingProperties.specularIntensity = specularIntensityMultSpecMap * lerp(1.0f, specMult*specMult, specIntensityAdjust);
		surfLightingProperties.specularExponent	= specularFalloffMultSpecMap * lerp(1.0f, specMult*specMult, specFalloffAdjust);
#elif USE_DIFFLUM_ON_SPECINT
		surfLightingProperties.specularIntensity	= (specularIntensityMult*specularIntensityMult);
		surfLightingProperties.specularExponent	= specularFalloffMult;
#else	
		surfLightingProperties.specularIntensity = specularIntensityMult*specularIntensityMult;
		surfLightingProperties.specularExponent = specularFalloffMult;	
#endif	// SPECULAR_MAP

#if PARALLAX_MAP && __SHADERMODEL >= 40
	if(POMDistanceDebug)
	{	
		if(distanceBlend < 1.0f)
		{
			surfLightingProperties.diffuseColor = 0.0f;
			surfLightingProperties.diffuseColor.g = numberOfSteps / float(POMMaxSteps);
			surfLightingProperties.diffuseColor.r = edgeWeight;
		}
	}
#endif // PARALLAX_MAP && __SHADERMODEL >= 40

#if USE_EDGE_WEIGHT
	if(POMEdgeVisualiser)
	{
		surfLightingProperties.diffuseColor.r = clamp(IN.edgeWeight.x, 0.0f, 1.0f);
		surfLightingProperties.diffuseColor.g = 1.0f - clamp(IN.edgeWeight.y, 0.0f, 1.0f);
		surfLightingProperties.diffuseColor.b = 0.0f;
	}
#endif

#if SELF_SHADOW
#if PARALLAX_MAP && __SHADERMODEL >= 40
	float blendOffStart = 0.8f;

	if(distanceBlend == 1.0f)
	{
		surfLightingProperties.selfShadow = BumpShadowApply(IN.worldNormal.w, texel.color.rgb);
	}
	else if(distanceBlend > blendOffStart)
	{
		float BumpSelfShadow = BumpShadowApply(IN.worldNormal.w, texel.color.rgb);
		float range = 1.0f - blendOffStart;

		//blend the normal bump self shadowing in to reduce discontinuity between HD and LOD shaders
		surfLightingProperties.selfShadow = lerp(POMSelfShadow, BumpSelfShadow, (distanceBlend - blendOffStart) / range);
	}
	else
	{
		surfLightingProperties.selfShadow = POMSelfShadow;
	}
#else
	surfLightingProperties.selfShadow = BumpShadowApply(IN.worldNormal.w, texel.color.rgb);
#endif // PARALLAX_MAP && __SHADERMODEL >= 40
#endif // SELF_SHADOW

#if USE_SLOPE
	float3 normNormal = normalize(IN.worldNormal.xyz);
	surfLightingProperties.slope = CalcSlope(normNormal.z);
#endif

	surfLightingProperties.inInterior = gInInterior;

	// Pack the information into the GBuffer(s).
	DeferredGBuffer OUT = PackGBuffer(texel.normal,surfLightingProperties );
	
	return(OUT);
}

#if DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES
DeferredGBuffer	PS_TexturedDeferred_AlphaClip(vertexOutputDeferred IN)
{
	// The function is written for the alpha fade on PC really. Can't see this being needed otherwise...
	// Also, the role of alphaclip would be undefined, so I've not bothered attempting to make this do anything.
#if defined(USE_ALPHACLIP_FADE) && !RSG_ORBIS && !RSG_DURANGO
	FadeDiscard( IN.pos, globalFade );
#endif // USE_ALPHACLIP_FADE

	return PS_TexturedDeferred(IN);
}
#endif // DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES

#if __MAX
technique tool_draw
{
	pass p0
	{
		MAX_TOOL_TECHNIQUE_RS
		AlphaBlendEnable= false;
		AlphaTestEnable	= false;
#if 0
		// unlit:
		VertexShader	= compile VERTEXSHADER	VS_MaxTransform();
		PixelShader		= compile PIXELSHADER	PS_TexturedUnlit();
#else
		// lit:
		VertexShader	= compile VERTEXSHADER	VS_MaxTransform();
		PixelShader		= compile PIXELSHADER	PS_MaxTextured();
#endif
	}
}
#else // __MAX
//
//
//

#if DEFERRED_TECHNIQUES && NON_CUTOUT_DEFERRED_TECHNIQUES
technique deferred_draw
{
	pass p0
	{
		SetGlobalDeferredMaterial(DEFERRED_STENCIL_REF);
	
		VertexShader = compile VERTEXSHADER	VS_TransformDeferred();
		PixelShader  = compile PIXELSHADER	PS_TexturedDeferred()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
TERRAIN_TESSELLATION_SHADER_CODE_ONLY(SHADER_MODEL_50_OVERRIDE_TECHNIQUES(technique deferred_drawtessellated
{
	pass p0
	{
		SetGlobalDeferredMaterial(DEFERRED_STENCIL_REF);

		VertexShader = compile VERTEXSHADER	VS_TransformDeferred();
		PixelShader  = compile PIXELSHADER	PS_TexturedDeferred()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
},
technique SHADER_MODEL_50_OVERRIDE(deferred_drawtessellated)
{
	pass p0
	{
		SetGlobalDeferredMaterial(DEFERRED_STENCIL_REF);
	
		VertexShader = compile VSDS_SHADER	VS_TransformDeferred_Tess();
		SetHullShader(compileshader(hs_5_0, HS_TransformDeferred_Tess()));
		SetDomainShader(compileshader(ds_5_0, DS_TransformDeferred_Tess()));
		PixelShader  = compile PIXELSHADER	PS_TexturedDeferred()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}))
#endif // DEFERRED_TECHNIQUES && NON_CUTOUT_DEFERRED_TECHNIQUES

#if DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES
technique deferredalphaclip_draw
{
	pass p0
	{
		SetGlobalDeferredMaterial(DEFERRED_STENCIL_REF);
	
		VertexShader = compile VERTEXSHADER	VS_TransformDeferred();
		PixelShader  = compile PIXELSHADER	PS_TexturedDeferred_AlphaClip()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
TERRAIN_TESSELLATION_SHADER_CODE_ONLY(SHADER_MODEL_50_OVERRIDE_TECHNIQUES(technique deferredalphaclip_drawtessellated
{
	pass p0
	{
		SetGlobalDeferredMaterial(DEFERRED_STENCIL_REF);

		VertexShader = compile VERTEXSHADER	VS_TransformDeferred();
		PixelShader  = compile PIXELSHADER	PS_TexturedDeferred_AlphaClip()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
},
technique SHADER_MODEL_50_OVERRIDE(deferredalphaclip_drawtessellated)
{
	pass p0
	{
		SetGlobalDeferredMaterial(DEFERRED_STENCIL_REF);
	
		VertexShader = compile VSDS_SHADER	VS_TransformDeferred_Tess();
		SetHullShader(compileshader(hs_5_0, HS_TransformDeferred_Tess()));
		SetDomainShader(compileshader(ds_5_0, DS_TransformDeferred_Tess()));
		PixelShader  = compile PIXELSHADER	PS_TexturedDeferred_AlphaClip()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}))
#endif	// DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES

#if DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES
technique deferredsubsamplealpha_draw
{
	pass p0
	{
		SetGlobalDeferredMaterial(DEFERRED_STENCIL_REF);

		VertexShader = compile VERTEXSHADER	VS_TransformDeferred();
		PixelShader  = compile PIXELSHADER	PS_TexturedDeferred()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
TERRAIN_TESSELLATION_SHADER_CODE_ONLY(SHADER_MODEL_50_OVERRIDE_TECHNIQUES(technique deferredsubsamplealpha_drawtessellated
{
	pass p0
	{
		SetGlobalDeferredMaterial(DEFERRED_STENCIL_REF);

		VertexShader = compile VERTEXSHADER	VS_TransformDeferred();
		PixelShader  = compile PIXELSHADER	PS_TexturedDeferred()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
},
technique SHADER_MODEL_50_OVERRIDE(deferredsubsamplealpha_drawtessellated)
{
	pass p0
	{
		SetGlobalDeferredMaterial(DEFERRED_STENCIL_REF);
	
		VertexShader = compile VSDS_SHADER	VS_TransformDeferred_Tess();
		SetHullShader(compileshader(hs_5_0, HS_TransformDeferred_Tess()));
		SetDomainShader(compileshader(ds_5_0, DS_TransformDeferred_Tess()));
		PixelShader  = compile PIXELSHADER	PS_TexturedDeferred()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}))
#endif // DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES

#if FORWARD_TERRAIN_TECHNIQUES
technique draw // use lightweight0 for draw (we just want to draw something)
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER	VS_Transform_lightweight0();
#if __MAX
		PixelShader  = compile PIXELSHADER	PS_TexturedMax()  CGC_FLAGS(CGC_DEFAULTFLAGS);
#else
		PixelShader  = compile PIXELSHADER	PS_Textured_lightweight0()  CGC_FLAGS(CGC_DEFAULTFLAGS);
#endif
	}
}
#endif // FORWARD_TERRAIN_TECHNIQUES

#if FORWARD_TERRAIN_TECHNIQUES
technique lightweight4_draw // use lightweight0 for lightweight4 (we just want to draw something)
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER	VS_Transform_lightweight0();
		PixelShader  = compile PIXELSHADER	PS_Textured_lightweight0()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
#endif // FORWARD_TERRAIN_TECHNIQUES

#if FORWARD_TERRAIN_TECHNIQUES
technique lightweight0_draw
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER	VS_Transform_lightweight0();
		PixelShader  = compile PIXELSHADER	PS_Textured_lightweight0()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
#endif // FORWARD_TERRAIN_TECHNIQUES

#if WATER_REFLECTION_TECHNIQUES
technique waterreflection_draw
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER	VS_TransformWaterReflection();
		PixelShader  = compile PIXELSHADER	PS_TexturedWaterReflection()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
#endif // WATER_REFLECTION_TECHNIQUES

//
//
//
//

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

#if CUBEMAP_REFLECTION_TECHNIQUES
technique cube_draw
{
	pass p0
	{
		CullMode=CCW;
		VertexShader = compile VERTEXSHADER	VS_TransformCube();
		PixelShader  = compile PIXELSHADER	PS_TexturedCube()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}

#if GS_INSTANCED_CUBEMAP
	GEN_GSINST_TYPE(Terrain,vertexTerrainOutputCube,SV_RenderTargetArrayIndex)
	GEN_GSINST_FUNC(Terrain,Terrain,vertexInputCube,vertexTerrainOutputCube,vertexTerrainOutputCube,VS_TransformCube_Common(IN,true),PS_TexturedCube(IN))
	technique cubeinst_draw
	{
		GEN_GSINST_TECHPASS(Terrain,all)
	}
#endif

#endif // CUBEMAP_REFLECTION_TECHNIQUES

SHADERTECHNIQUE_CASCADE_SHADOWS()
SHADERTECHNIQUE_LOCAL_SHADOWS(VS_LinearDepth, VS_LinearDepth, PS_LinearDepth)
SHADERTECHNIQUE_ENTITY_SELECT(VS_Transform(), VS_Transform())
#include "../Debug/debug_overlay_terrain.fxh"


#if TERRAIN_TESSELLATION_SHADER_CODE

VS_CascadeShadows_OUT VS_TransformShadowPass(VS_CascadeShadows_IN IN)
{
	VS_CascadeShadows_OUT OUT;

	float3	localPos = IN.pos;
#if TEMP_RUNTIME_CALCULATE_TERRAIN_OPEN_EDGE_INFO
	localPos.z = abs(localPos.z) + TERRAIN_OPEN_EDGE_INFO_COMMON_FLOOR;
#endif
	OUT.pos = mul(float4(localPos, 1), gWorldViewProj);

#ifdef SHADOW_USE_TEXTURE
	OUT.tex.xy = IN.tex.xy;
#endif // SHADOW_USE_TEXTURE
	return OUT;
}

// TODO:- instanced shadow versions.

// Shadow techniques.
SHADER_MODEL_50_OVERRIDE_TECHNIQUES(technique wdcascade_drawtessellated
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER	VS_TransformShadowPass();
		COMPILE_PIXELSHADER_NULL()
	}
},
technique SHADER_MODEL_50_OVERRIDE(wdcascade_drawtessellated)
{
	pass p0
	{
		VertexShader = compile VSDS_SHADER	VS_TransformDeferred_Tess();
		SetHullShader(compileshader(hs_5_0, HS_TransformDeferred_Tess()));
		SetDomainShader(compileshader(ds_5_0, DS_TransformDeferred_Tess()));
		COMPILE_PIXELSHADER_NULL()
	}
})
#endif // TERRAIN_TESSELLATION_SHADER_CODE

#endif	//!__MAX...

#endif // __GTA_TERRAIN_CB_COMMON_FXH__


