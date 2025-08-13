//
//	gta_ped_common - all shared stuff for gta_ped_xxx shaders;
//
//	2005/08/04	-	Andrzej:	- initial;
//	2005/08/16	-	Andrzej:	- added SPEC_MAP support;
//	2006/02/20	-	Andrzej:	- added ped hair support;
//	2006/06/06	-	Andrzej:	- subsurface scattering support added;
//	2007/02/06	-	Andrzej:	- ped damage support added for deferred mode;
//	2007/02/12	-	Andrzej:	- ped reflection support with masking added;
//	----------	-	--------	- ------------------------------------------
//	2008/07/02	-	Andrzej:	- ped shader cleanup & refactor;
//	2009/01/22	-	Andrzej:	- fur shader;
//	2009/01/30	-	Andrzej:	- ped_decal shader;
//	2009/03/16	-	Andrzej:	- ped_hair_long_alpha_exp shader;
//	2009/03/30	-	Andrzej:	- ped_enveff (snow) shader;
//	2009/04/12	-	Andrzej:	- tool technique;
//	2010/06/30	-	Andrzej:	- ped_fat shader;
//
// 
//
//
#ifndef __GTA_PED_COMMON_FXH__
#define __GTA_PED_COMMON_FXH__

#define IS_PED_SHADER // Must be above common.fxh include

#if (__WIN32PC && __SHADERMODEL >= 40)
#ifndef USE_ALPHACLIP_FADE
#define USE_ALPHACLIP_FADE
#endif
#endif // (__WIN32PC && __SHADERMODEL >= 40)

#if defined(USE_PED_CLOTH)
	#define DX11_CLOTH	((__WIN32PC && __SHADERMODEL >= 40) || RSG_DURANGO || RSG_ORBIS)
#else
	#define DX11_CLOTH	(((__WIN32PC && __SHADERMODEL >= 40) || RSG_DURANGO || RSG_ORBIS) && !defined(PEDSHADER_HAIR_SPIKED) && !defined(PEDSHADER_HAIR_ORDERED) && !defined(PEDSHADER_HAIR_CUTOUT) && !defined(PEDSHADER_HAIR_LONGHAIR))
#endif


#if __PS3
#define HALF_PRECISION_LIGHT_STRUCTS
#endif

#include "ped_common_values.h"

#define PED_TRANSFORM_WORLDPOS_OUT (1) // make sure this doesn't add overhead

#define USE_DIFFUSE_SAMPLER
#include "../common.fxh"
#include "../Util/GSInst.fxh"

#if defined(USE_PED_CPV_WIND)
	#define PED_CPV_WIND					(1)
#else
	#define PED_CPV_WIND					(0)
#endif

#if defined(USE_PEDSHADER_FAT)
	#define PEDSHADER_FAT					(1)
#else
	#define PEDSHADER_FAT					(0)
#endif

#if defined(USE_NORMAL_MAP)
	#define NORMAL_MAP						(1)
#else
	#define NORMAL_MAP						(0)
#endif

#if defined(USE_SPECULAR)
	#define SPECULAR						(1)
	#if defined(IGNORE_SPECULAR_MAP)
		#define SPEC_MAP					(0)
	#else
		#define SPEC_MAP					(1)
	#endif
#else
	#define SPECULAR						(0)
	#define SPEC_MAP						(0)
#endif

#ifdef USE_EMISSIVE
	#define EMISSIVE						(1)
#else
	#define EMISSIVE						(0)
#endif	// USE_EMISSIVE

#if defined(USE_PED_CLOTH)
	#define PED_CLOTH						(1)
#else
	#define PED_CLOTH						(0)
#endif

#if defined(USE_PED_DAMAGE_TARGETS)			// enable support for Blood, damage and tattoo render targets.
	#define PED_DAMAGE_TARGETS				(1)
	#define PED_DAMAGE_INPUT				texCoord1.xy
#else
	#define PED_DAMAGE_TARGETS				(0)
#endif

#if defined(USE_PED_DAMAGE_MEDALS)			// enable use the decoration buffer for Medals decal, not normal decorations/tattoos/dirt/etc
	#define PED_DAMAGE_MEDALS				(1)
#else
	#define PED_DAMAGE_MEDALS				(0)
#endif

#if defined(USE_PED_DAMAGE_DECORATION_DECAL) && defined(USE_PED_DAMAGE_TARGETS)
	#define PED_DAMAGE_DECORATION_DECAL		(1)
#else
	#define PED_DAMAGE_DECORATION_DECAL		(0)
#endif 

#if defined(USE_PED_COLOR_VARIATION_TEXTURE)
	#define PED_COLOR_VARIATION_TEXTURE			(1)
#else
	#define PED_COLOR_VARIATION_TEXTURE			(0)
#endif

#if defined(USE_PEDSHADER_FUR)
	#define PEDSHADER_FUR						(1)
    #undef PED_CAN_BE_SKIN
#else
	#define PEDSHADER_FUR						(0)
#endif

#if defined(USE_PEDSHADER_SNOW)
	#define PEDSHADER_SNOW						(1)
	#define PEDSHADER_SNOW_MAX					(0 && __MAX)	// ped snow for Max DX viewport
#else
	#define PEDSHADER_SNOW						(0)
	#define PEDSHADER_SNOW_MAX					(0)
#endif

#if defined(PEDSHADER_HAIR_ORDERED)
	#define	PED_HAIR_ORDERED					(1)
	#undef	USE_TWIDDLE							// want fully blendable normalmap instead of twiddle
#else
	#define	PED_HAIR_ORDERED					(0)
#endif

#if defined(PEDSHADER_HAIR_DIFFUSE_MASK)
	#define	PED_HAIR_DIFFUSE_MASK				(1)
#else
	#define	PED_HAIR_DIFFUSE_MASK				(0)
#endif

#if defined(PEDSHADER_HAIR_CUTOUT)
	#define PED_HAIR_CUTOUT						(1)	
#else
	#define PED_HAIR_CUTOUT						(0)
#endif

#if defined(PEDSHADER_HAIR_ANISOTROPIC)
	#define PED_HAIR_ANISOTROPIC				(1)
#else
	#define PED_HAIR_ANISOTROPIC				(0)
#endif

#if defined(PEDSHADER_HAIR_LONGHAIR)
	#define PED_HAIR_LONG						(1)
#else
	#define PED_HAIR_LONG						(0)
#endif

#if defined(PEDSHADER_HAIR_SPIKED)
	#define	PED_HAIR_SPIKED						(1)
#else
	#define	PED_HAIR_SPIKED						(0)
#endif

#if defined(USE_HAIR_TINT_TECHNIQUES)
	#define HAIR_TINT_TECHNIQUES				(1)
#else
	#define HAIR_TINT_TECHNIQUES				(0)
#endif

#if defined(USE_PED_DECAL)
	#define PED_DECAL							(1)
	// full BlendStateBlock for ped_decal:
	#define PED_DECAL_BS()					\
			AlphaTestEnable = false;		\
			AlphaBlendEnable= true;			\
			BlendOp         = Add;			\
			SrcBlend        = SrcAlpha;		\
			DestBlend       = InvSrcAlpha;	\
			SET_COLOR_WRITE_ENABLE(RED+GREEN+BLUE, RED+GREEN+BLUE, RED+GREEN+BLUE, 0)
	#undef	USE_TWIDDLE							// want fully blendable normalmap instead of twiddle
#else
	#define PED_DECAL							(0)
#endif

#if defined(USE_PED_DECAL_DIFFUSE_ONLY)
	#define PED_DECAL_DIFFUSE_ONLY				(1)
	// full BlendStateBlock for ped_decal_diffuse_only:
	#define PED_DECAL_DIFFUSE_ONLY_BS()		\
			AlphaTestEnable = false;		\
			AlphaBlendEnable= true;			\
			BlendOp         = Add;			\
			SrcBlend        = SrcAlpha;		\
			DestBlend       = InvSrcAlpha;	\
			SET_COLOR_WRITE_ENABLE(RED+GREEN+BLUE, 0, 0, 0)
#else
	#define PED_DECAL_DIFFUSE_ONLY				(0)
#endif

#if defined(USE_PED_DECAL_NO_DIFFUSE)
	#define PED_DECAL_NO_DIFFUSE				(1)
	// full BlendStateBlock for ped_decal_nodiff:
	#define PED_DECAL_NO_DIFFUSE_BS()		\
			AlphaTestEnable = false;		\
			AlphaBlendEnable= true;			\
			BlendOp         = Add;			\
			SrcBlend        = SrcAlpha;		\
			DestBlend       = InvSrcAlpha;	\
			SET_COLOR_WRITE_ENABLE(0, RED+GREEN+BLUE, RED+GREEN+BLUE, 0)
#else
	#define PED_DECAL_NO_DIFFUSE				(0)
#endif

#if PED_DECAL
	#define PED_DECAL_RENDERSTATES()			\
		zFunc = FixedupLESS;					\
		ZwriteEnable = false;					\
		PED_DECAL_BS()
#elif PED_DECAL_DIFFUSE_ONLY
	#define PED_DECAL_RENDERSTATES()			\
		zFunc = FixedupLESS;					\
		ZwriteEnable = false;					\
		PED_DECAL_DIFFUSE_ONLY_BS()
#elif PED_DECAL_NO_DIFFUSE
	#define PED_DECAL_RENDERSTATES()			\
		zFunc = FixedupLESS;					\
		ZwriteEnable = false;					\
		PED_DECAL_NO_DIFFUSE_BS()
#else
	#define PED_DECAL_RENDERSTATES()
#endif


#if defined(USE_PED_WRINKLE) && defined(USE_NORMAL_MAP)
	#define	PED_WRINKLE							(1)
#else
	#define	PED_WRINKLE							(0)
#endif

#if defined(USE_PED_WRINKLE_CS) && PED_WRINKLE
	#define	PED_WRINKLE_CS						(1)
#else
	#define	PED_WRINKLE_CS						(0)
#endif

#if defined(USE_PED_STUBBLE)
	#define PED_STUBBLE							(1)
#else
	#define PED_STUBBLE							(0)
#endif

#if defined(USE_PED_BASIC_SHADER)
	#define PED_BASIC_SHADER					(1)		// cheap one, used for ped LODs
#else
	#define PED_BASIC_SHADER					(0)
#endif

#if defined(USE_UI_TECHNIQUES)
	#define UI_TECHNIQUES (1)
#else
	#define UI_TECHNIQUES (0)
#endif

#if defined(USE_DOUBLE_SIDED)
	#define DOUBLE_SIDED	(1)
#else
	#define DOUBLE_SIDED	(0)
#endif

#if __XENON
	#define IFANY_BRANCH /*[ifAny]*/
#elif __PS3
	#define IFANY_BRANCH [branch]
#else
	#define IFANY_BRANCH 
#endif

#if !(PED_HAIR_CUTOUT || PED_HAIR_ORDERED || PED_HAIR_LONG )
#if !defined(IS_NOT_SKIN)
#define PED_CAN_BE_SKIN
#endif
#define USE_TWIDDLE							// use normals twiddle
#endif

#define PED_VERTEX_DAMAGE_MASK (1 && __XENON)

#include "../Util/skin.fxh"

//
//
// Palettized ped variation texture stuff:
//
#if (__WIN32PC && __SHADERMODEL == 30)
	// hack for 3dsMax UI as only local samplers display correctly:
	BeginSampler(sampler2D,diffuseTexturePal,TextureSamplerDiffPal,DiffuseTexPal)
		#if PED_COLOR_VARIATION_TEXTURE
			string UIName="Diffuse Palette";
			string TCPTemplate="Diffuse";
			string TextureType="Palette";
		#endif
		int nostrip=1;	// dont strip
	ContinueSampler(sampler2D,diffuseTexturePal,TextureSamplerDiffPal,DiffuseTexPal)
		AddressU  = WRAP;        
		AddressV  = WRAP;
		MIPFILTER = POINT;
		MINFILTER = POINT;
		MAGFILTER = POINT;
	EndSampler;
#else
	BeginSharedSampler(sampler2D,diffuseTexturePal,TextureSamplerDiffPal,DiffuseTexPal, s13)
		#if PED_COLOR_VARIATION_TEXTURE
			string UIName="Diffuse Palette";
			string TCPTemplate="Diffuse";
			string TextureType="Palette";
		#endif
		int nostrip=1;	// dont strip
	ContinueSharedSampler(sampler2D,diffuseTexturePal,TextureSamplerDiffPal,DiffuseTexPal, s13)
		AddressU  = WRAP;        
		AddressV  = WRAP;
		MIPFILTER = POINT;
		MINFILTER = POINT;
		MAGFILTER = POINT;
	EndSharedSampler;
#endif

#if RSG_PC && __SHADERMODEL>=40// Only required because we want to minimize console changes
BeginSharedSampler(sampler2D, pedTattooTarget, pedTattooTargetSampler, pedTattooTarget, s6)
	int nostrip=1;	// dont strip
ContinueSharedSampler(sampler2D, pedTattooTarget, pedTattooTargetSampler, pedTattooTarget, s6)
#else
BeginSharedSampler(sampler2D, pedTattooTarget, pedTattooTargetSampler, pedTattooTarget, s14)
	int nostrip=1;	// dont strip
ContinueSharedSampler(sampler2D, pedTattooTarget, pedTattooTargetSampler, pedTattooTarget, s14)
#endif // RSG_PC
	AddressU  = CLAMP;
	AddressV  = CLAMP;
	MIPFILTER = POINT;
	MINFILTER = LINEAR;
	MAGFILTER = LINEAR;
	#if !__MAX
	#ifdef MIN_MIP_LEVEL
		MINMIPLEVEL = 1;
	#endif
	#endif		
EndSharedSampler; 

// NOTE: s15 cannot be used in the forward pass (it's used for the shadow)
BeginSharedSampler(sampler2D, pedBloodTarget, pedBloodTargetSampler, pedBloodTarget, s15)
	int nostrip=1;	// dont strip
ContinueSharedSampler(sampler2D, pedBloodTarget, pedBloodTargetSampler, pedBloodTarget, s15)
	AddressU  = CLAMP;
	AddressV  = CLAMP;
	MIPFILTER = POINT;
	MINFILTER = LINEAR;
	MAGFILTER = LINEAR;
EndSharedSampler;


CBSHARED BeginConstantBufferPagedDX10(ped_common_shared_locals, b13)

// wet ped effect data 
shared float4 matWetClothesData : WetClothesData //REGISTER2(vs, c253) // Set by code, so it needs to be exposed for all variations to make sure the entry is in the shader group for the drawable
<
	int nostrip=1;	// dont strip
> = float4(-2,-2,0,0);

// um override params:
shared float4 umPedGlobalOverrideParams : umPedGlobalOverrideParams0 //REGISTER2(vs, c254)
<
	int nostrip=1;
> = float4(1.0f, 1.0f, 1.0f, 1.0f);
#define umOverrideScaleH		(umPedGlobalOverrideParams.x)	// override scaleH
#define umOverrideScaleV		(umPedGlobalOverrideParams.y)	// override scaleV
#define umOverrideArgFreqH		(umPedGlobalOverrideParams.z)	// override ArgFreqH
#define umOverrideArgFreqV		(umPedGlobalOverrideParams.w)	// override ArgFreqV

// xy: snowScale: <0; 1> - controlled by runtime code
// z:  fatScale:  <0; 1> - controlled by runtime code
// w:  sweatScale:<0; 1> - controlled by runtime code 
shared float4 envEffFatSweatScale : envEffFatSweatScale0 //REGISTER2(vs, c255)
<
	int nostrip	= 1;	// dont strip
> = float4(1.0f, 0.001f, 0.001f, 0.0f);
#define snowScale	(envEffFatSweatScale.x)	// snow scale (=1.0f)
#define snowScaleV	(envEffFatSweatScale.y)	// snow scale =(1.0f/1000.0f) with compensation for snowStep
#define fatScaleV	(envEffFatSweatScale.z)	// fat scale =(1.0f/1000.0f) with compensation for snowStep
#define sweatScaleV	(envEffFatSweatScale.w)	// sweat scale <0; 1>

//
// Palettized ped variation texture stuff:
//
shared float paletteSelector : DiffuseTexPaletteSelector //REGISTER2(ps, c209)
<
	int nostrip=1;	// dont strip
> = 0.0f;	// 0-7


//---------- stubble mapping
#if PED_STUBBLE
	shared float2 StubbleGrowth : StubbleGrowth //REGISTER2(ps, c210)
	<
		string UIWidget = "Numeric";
		float UIMin = 0.0;
		float UIMax = 100.0;
		float UIStep = .001;
		string UIName = "Stubble Growth";
		int nostrip=1;
	> = float2(0.2f,1.f);
#else
	shared float2 StubbleGrowth : StubbleGrowth //REGISTER2(ps, c210)
	<
		int nostrip=1;
	> = float2(0.2f,1.f);
#endif

#define PedDetailScale  StubbleGrowth.y

// material color scale [0] - used for burnout peds
shared float4 _matMaterialColorScale[2] : MaterialColorScale /*REGISTER2(ps, c211)*/ = {	float4(1,1,1,1),	
																						float4(1,1,1,1)};
#define matMaterialColorScale0	_matMaterialColorScale[0]	// diffuse, emissive+reflection+rim, specular, heat
#define	matMaterialColorScale1	_matMaterialColorScale[1]	// detailMap, free, free, free


#define USE_VARIABLE_BLOOD_COLORS		(1)	// set to 0 once we find a good set of values

#if USE_VARIABLE_BLOOD_COLORS
	shared float4 PedDamageColors[3] : PedDamageColors //REGISTER2(ps, c213)
	<
		int nostrip=1;	// dont strip
	> = {	float4(0.4, 0.0,  0.0, 1.0),			// [0] rgb = Blood splat color, a = blood SpecExponent
			float4(0.5, 0.25, 0.0, 0.125),			// [1] rgb = Blood soak color,  a = blood SpecIntensity
			float4(.95, 1.0,  0.0, 0.0)};			// [2] r = blood fresnel, g = blood reflection adjust, b = unused, a = blood bump
	#define BloodSplatColor			(PedDamageColors[0].rgb)
	#define BloodSoakColor			(PedDamageColors[1].rgb)
	#define bloodSpecAdjust			float2(PedDamageColors[0].w,PedDamageColors[1].w) // x = spec exponent for blood splat, y =  spec intensity of blood splat
	#define BloodFresnel			(PedDamageColors[2].x)
	#define BloodReflection			(PedDamageColors[2].y)
#else
	#define BloodSplatColor			float3(.4,0,0)
	#define BloodSoakColor			float3(.5,.25,0)
	#define bloodSpecAdjust			float2(200.0,0.05)   // x = spec exponent for blood splat, y =  spec intensity of blood splat
	#define BloodFresnel			0.95
	#define BloodReflection			1.0
#endif

shared float4 envEffColorModCpvAdd : envEffColorModCpvAdd0 //REGISTER(c216)
<
	int nostrip	= 1;	// dont strip
> = float4(1,1,1,0);	
#define envEffColorMod		(envEffColorModCpvAdd.rgb)		// EnvEff color modulator
#define envEffCpvAdd		(envEffColorModCpvAdd.a)		// EnvEff cpv add


//
//
//
//
#if PED_WRINKLE 
	shared float4 wrinkleMaskStrengths0 : WrinkleMaskStrengths0 //REGISTER2(ps, c217)
	<
		string UIWidget = "slider";
		float UIMin = 0.0;
		float UIMax = 1.0;
		float UIStep = .01;
		string UIName = "wrinkle str0";
		int nostrip=1;
	> = { 0.0, 0.0, 0.0, 0.0};

	shared float4 wrinkleMaskStrengths1 : WrinkleMaskStrengths1 //REGISTER2(ps, c218)
	<
		string UIWidget = "slider";
		float UIMin = 0.0;
		float UIMax = 1.0;
		float UIStep = .01;
		string UIName = "wrinkle str1";
		int nostrip=1;
	> = { 0.0, 0.0, 0.0, 0.0};

	shared float4 wrinkleMaskStrengths2 : WrinkleMaskStrengths2 //REGISTER2(ps, c219)
	<
		string UIWidget = "slider";
		float UIMin = 0.0;
		float UIMax = 1.0;
		float UIStep = .01;
		string UIName = "wrinkle str2";
		int nostrip=1;
	> = { 0.0, 0.0, 0.0, 0.0};

	shared float4 wrinkleMaskStrengths3 : WrinkleMaskStrengths3 //REGISTER2(ps, c220)
	<
		string UIWidget = "slider";
		float UIMin = 0.0;
		float UIMax = 1.0;
		float UIStep = .01;
		string UIName = "wrinkle str3";
		int nostrip=1;
	> = { 0.0, 0.0, 0.0, 0.0};
#else  // PED_WRINKLE...
	shared float4 wrinkleMaskStrengths0 : WrinkleMaskStrengths0 //REGISTER2(ps, c217)
	<
		int nostrip=1;
	> = { 0.0, 0.0, 0.0, 0.0};

	shared float4 wrinkleMaskStrengths1 : WrinkleMaskStrengths1 //REGISTER2(ps, c218)
	<
		int nostrip=1;
	> = { 0.0, 0.0, 0.0, 0.0};

	shared float4 wrinkleMaskStrengths2 : WrinkleMaskStrengths2 //REGISTER2(ps, c219)
	<
		int nostrip=1;
	> = { 0.0, 0.0, 0.0, 0.0};

	shared float4 wrinkleMaskStrengths3 : WrinkleMaskStrengths3 //REGISTER2(ps, c220)
	<
		int nostrip=1;
	> = { 0.0, 0.0, 0.0, 0.0};
#endif //PED_WRINKLE...

#if PED_WRINKLE_CS
	shared float4 wrinkleMaskStrengths4 : WrinkleMaskStrengths4 //REGISTER2(ps, c221)
	<
		string UIWidget = "slider";
		float UIMin = 0.0;
		float UIMax = 2.0;
		float UIStep = .01;
		string UIName = "wrinkle str4";
		int nostrip=1;
	> = { 0.0, 0.0, 0.0, 0.0};

	shared float4 wrinkleMaskStrengths5 : WrinkleMaskStrengths5 //REGISTER2(ps, c222)
	<
		string UIWidget = "slider";
		float UIMin = 0.0;
		float UIMax = 2.0;
		float UIStep = .01;
		string UIName = "wrinkle str5";
		int nostrip=1;
	> = { 0.0, 0.0, 0.0, 0.0};
#else  // PED_WRINKLE_CS...
	shared float4 wrinkleMaskStrengths4 : WrinkleMaskStrengths4 //REGISTER2(ps, c221)
	<
		int nostrip=1;
	> = { 0.0, 0.0, 0.0, 0.0};

	shared float4 wrinkleMaskStrengths5 : WrinkleMaskStrengths5 //REGISTER2(ps, c222)
	<
		int nostrip=1;
	> = { 0.0, 0.0, 0.0, 0.0};
#endif //PED_WRINKLE_CS...


// TODO: add another float4. we are spending cycle unpacking the data
shared float4 PedDamageData : PedDamageData //REGISTER(c223)
<
	int nostrip=1;	// don't strip
> = float4(0,0,0,0); // x = blood enabled, y = normal maps enabled & scale, is rotate flags (frac part is 0.0=not rot, .5 = rotate, .75 = rot and dual target, whole part is 0 = tattoo no rotated, 1 = tat rotated), w = frac part is 1/blood target width, is tatto width
#define BloodEnabled			(PedDamageData.x)  // 0 mean turn off all blood/damage/decorations, + means full normal blood/damage/tattoos/decals/dirt, - means blood is enabled, but skip the decorations/dirt/tattoos part (it's being used for police medals), value is zone mask
#define BloodNormalMapsEnabled	(PedDamageData.y)
#define IsBloodRotated()		(frac(PedDamageData.z)>0.1)	 // note: if it's double wide it also has to be rotate currently  
#define IsBloodAndTattooInOne() (frac(PedDamageData.z)>0.6)
#define IsTattooRotated()		(PedDamageData.z>=1.0)       // note: with this encoding, tattoo can only be rotated if blood is, but that's okay, it only really used for the compressed targets being upright when the blood sometimes is not.

#define OneOverBloodWidth		(frac(PedDamageData.w))								// frac part is the 1/width for the blood (if upright)
#define OneOverBloodHeight		(OneOverBloodWidth*BloodTexelRatio)    
#define OneOverTattooWidth		(1.0/(PedDamageData.w-frac(PedDamageData.w)))		// whole portion hold tattoo width (if upright) (the ratio is not quite right if this is a compressed textures, but probably close enough
#define OneOverTattooHeight		(OneOverTattooWidth*BloodTexelRatio)   
#define BloodBumpScale 			(PedDamageData.y)  

shared float4 wetnessAdjust : WetnessAdjust	//REGISTER(c224)
<
	int nostrip=1;	// Set by code, so it needs to be exposed for all variations to make sure the entry is in the shader group for the drawable
>  = {0.5f, 16.0f, 1.0f, 0.1f};

shared float alphaToCoverageScale : AlphaToCoverageScale
<
	string UIName = "Alpha multiplier for MSAA coverage";
	float UIMin = 0.0;
	float UIMax = 5.0;
	float UIStep = 0.1;
	int nostrip	= 1;
> = 1.0f;

EndConstantBufferDX10(ped_common_shared_locals)


BeginConstantBufferDX10( ped_common_locals1 )

//---------- stubble mapping
#if PED_STUBBLE
	float2 StubbleControl : StubbleControl
	<
		string UIWidget = "Numeric";
		float UIMin = 0.0;
		float UIMax = 100.0;
		float UIStep = .001;
		string UIName = "Index,Tiling";
		int nostrip=1;
	> = float2(10.,1.);
#else
	float2 StubbleControl : StubbleControl
	<
		int nostrip=1;
	> = float2(2.,.6);
#endif

#define STUBBLE_DETAIL						(2)

//--------------- Detail Mapping ----------------------------------
#ifndef DISABLE_DETAIL_MAPS
	#define USE_DETAIL_ATLAS				(1)
#else
	#define USE_DETAIL_ATLAS				(0)
#endif

EndConstantBufferDX10( ped_common_locals1 )

#if PED_STUBBLE
BeginSampler(sampler2D,StubbleMap,StubbleSampler,StubbleTex)
	string UIName="Ped Stubble Control Map";
	string UIHint="detailmap";
	string TCPTemplate="Diffuse";
	string TextureType="detail";
ContinueSampler(sampler2D,StubbleMap,StubbleSampler,StubbleTex)
	AddressU  = WRAP;
	AddressV  = WRAP;
	MIPFILTER = MIPLINEAR;
	MinFilter = HQLINEAR;
	MagFilter = MAGLINEAR;
EndSampler;
#endif //#if PED_STUBBLE

#if USE_DETAIL_ATLAS

	#define DETAIL_USE_VOLUME_TEXTURE
	#define DETAIL_ATLAS_NAME  "__DetailAtlas__"
	#include "..\megaShader\detail_map.fxh"

#endif //USE_DETAIL_ATLAS...

// --- Alpha to Coverage helpers ---
bool IsAlphaToCoverage()
{
	return __SHADERMODEL>=40 && alphaToCoverageScale!=0.0f && !SSTAA && !ENABLE_TRANSPARENCYAA;
}
void ComputeCoverage(inout float alpha_out, float alpha_test, float threshold)
{
	if (!IsAlphaToCoverage())
		return;

#if PEDSHADER_FUR && RSG_ORBIS
    alpha_out = 1.0f;
#else
	alpha_out = alpha_test * alpha_out * alphaToCoverageScale;
#endif
}


#define SKIN_DETAIL_MAP_START 13.0

float PedIsSkin( float3 specularValue )
{
	// skin material indices are greater than SKIN_DETAIL_MAP_START
	return step(SKIN_DETAIL_MAP_START,specularValue.b*16.0);	
}

#define SKIN_DETAIL_MAP_SUBSURFACE_START  13.0
#define SKIN_DETAIL_MAP_SUBSURFACE_END    15.0
#define SKIN_DETAIL_MAP_SUBSURFACE_RANGE  (SKIN_DETAIL_MAP_SUBSURFACE_END-SKIN_DETAIL_MAP_SUBSURFACE_START)
#define SKIN_DETAIL_MAP_SUBSURFACE_MIDDLE ((SKIN_DETAIL_MAP_SUBSURFACE_END+SKIN_DETAIL_MAP_SUBSURFACE_START)/2)

float PedIsSubSurfaceSkin( float3 specularValue )
{
	// subsurface skin is between SKIN_DETAIL_MAP_SUBSURFACE_START and SKIN_DETAIL_MAP_SUBSURFACE_END
	return step(abs(SKIN_DETAIL_MAP_SUBSURFACE_MIDDLE - specularValue.b*16.0), SKIN_DETAIL_MAP_SUBSURFACE_RANGE/2);	
}

#define SKIN_DETAIL_MAP 14
half PedIsSkin2( half3 specularValue )
{
	//TODO:  need to revisit this. Should it be more like PedIsSkin() or PedIsSubSurfaceSkin
	return saturate(1.5-abs(specularValue.b-((SKIN_DETAIL_MAP)/16.))*8.);
}

BeginConstantBufferDX10( ped_common_locals2 )
#if PED_HAIR_ANISOTROPIC
	float2 anisotropicSpecularIntensity : AnisotropicSpecularIntensity
	<
		int nostrip=1;	// dont strip
	
		string UIWidget = "slider";
		string UIName = "Aniso Intensities";
		float UIMin = 0.0;
		float UIMax = 0.2;
		float UIStep = 0.001;
	> = { 0.1, 0.15 };
	
	float2 anisotropicSpecularExponent : AnisotropicSpecularExponent
	<
		int nostrip=1;	// dont strip
	
		string UIWidget = "slider";
		string UIName = "Aniso Exponents";
		float UIMin = 0.01;
		float UIMax = 1000000.0;
		float UIStep = 0.01;
	> = { 16.0, 32.0 };
	
	float4 anisotropicSpecularColour : AnisotropicSpecularColour
	<
		int nostrip=1;	// dont strip
	
		string UIWidget = "Color";		
		string UIName = "2nd Aniso Colour";
		float UIMin = 0.0;
		float UIMax = 100.0;
		float UIStep = 0.01;	
	> = { 0.1, 0.1, 0.1, 1.0 };
	
	float4 specularNoiseMapUVScaleFactor : SpecularNoiseMapUVScaleFactors
	<
		int nostrip=1;	// dont strip
	
		string UIWidget = "slider";
		string UIName = "Aniso UV scales";
		float UIMin = 0.01;
		float UIMax = 10.0;
		float UIStep = 0.01;		
	> = { 2.0, 1.0, 3.0, 1.0 };	
#endif //PED_HAIR_ANISOTROPIC

#define PED_RIM_LIGHT						(1 && (!PED_BASIC_SHADER) && SPECULAR)
#define PRECOMPUTE_RIM						1
#define SPEC_MAP_INTFALLOFF_PACK			(PED_RIM_LIGHT)

#if REFLECT
	float reflectivePower : Reflectivity
	<
		string UIName = "Reflectivity";
		float UIMin = -10.0;
		float UIMax = 10.0;
		float UIStep = 0.1;
	> = 0.45;
#endif //REFLECT 

#if EMISSIVE || EMISSIVE_ADDITIVE
	float emissiveMultiplier : EmissiveMultiplier
	<
		string UIName = "Emissive HDR Multiplier";
		float UIMin = 0.0;
		float UIMax = 16.0;
		float UIStep = 0.1;
		int nostrip=1;	// dont strip
	> = 1.0f;
#endif

float bumpiness : Bumpiness
<
#if NORMAL_MAP
	string UIWidget = "slider";
	float UIMin = 0.0;
	float UIMax = 4.0;
	float UIStep = .01;
	string UIName = "Bumpiness";
#endif
	int nostrip=1;	// dont strip
> = 1.0;


#define FRESNEL (1 && SPECULAR)  // we don't use the fresnel term is there is no specular...

#if 1 //FRESNEL
	float specularFresnel : Fresnel
	<
	#if SPECULAR
		string UIName = "Specular Fresnel";
		float UIMin = 0.0;
		float UIMax = 1.0;
		float UIStep = 0.01;
	#endif
	int nostrip=1;	// dont strip
	> = 0.97f;
#else
	#define specularFresnel (0.97f)
#endif

float specularFalloffMult : Specular
<
#if SPECULAR
	string UIName = "Specular Falloff";
	float UIMin = 0.0;
	float UIMax = 512.0;
	float UIStep = 0.1;
#endif
	int nostrip=1;	// dont strip
> = 100.0;

float specularIntensityMult : SpecularColor
<
#if SPECULAR
	string UIName = "Specular Intensity";
	float UIMin = 0.0;
	float UIMax = 1.0;
	float UIStep = 0.01;
#endif
	int nostrip=1;	// dont strip
> = 0.125;


#if SPEC_MAP_INTFALLOFF_PACK
	// ped skin expects spec map in a specific channel order 
#else
	float3 specMapIntMask : SpecularMapIntensityMask
	<
	#if SPEC_MAP
		string UIWidget = "slider";
		float UIMin = 0.0;
		float UIMax = 1.0;
		float UIStep = .01;
		string UIName = "specular map intensity mask color";
	#endif
	int nostrip=1;	// dont strip
	> = { 1.0, 0.0, 0.0};
#endif

#if defined(USE_WET_EFFECT_DIFFUSE_ONLY)
	#define WET_PED_EFFECT_DIFFUSE_ONLY			(1)
#else
	#define WET_PED_EFFECT_DIFFUSE_ONLY			(0)
#endif

#define WET_PED_EFFECT							((SPECULAR || WET_PED_EFFECT_DIFFUSE_ONLY) && !defined(DISABLE_WET_EFFECT) && (!__MAX) && (!PED_BASIC_SHADER))
#define WETNESS_PARAM							texCoord.z	  

// Dynamic ped ambient bake
#define USE_DYNAMIC_AMBIENT_BAKE				(1)

#if PED_HAIR_ORDERED
	float orderNumber : OrderNumber
	<
		int nostrip	= 1;	// dont strip
		string UIName	= "Draw Order";
		int UIMin = 0;
		int UIMax = 9;
		int UIStep = 1;
	> = 0;
#endif //PED_HAIR_ORDERED...


#if PEDSHADER_FUR
// fur properties editable by artists:

float furMinLayers : furMinLayers0
<
	int nostrip	= 1;	// dont strip
    string UIName	= "Fur: Min layers";
    int UIMin = 1;
    int UIMax = 10;
    int UIStep = 1;
> = 1;

float furMaxLayers : furMaxLayers0
<
	int nostrip	= 1;	// dont strip
	string UIName	= "Fur: Max layers";
	int UIMin = 0;
	int UIMax = 50;
	int UIStep = 1;
> = 10;

float furLength : furLength0
<
	int nostrip	= 1;	// dont strip
	string UIName	= "Fur: length";
	float UIMin	= 0.01;
	float UIMax	= 5.0;
	float UIStep= 0.05f;
> = 0.5f;	

float furNoiseUVScale : furNoiseUVScale0
<
	int nostrip		= 1;	// dont strip
    string UIName	= "Fur: Noise UV Scale";
    float UIMin	= 0.5;
    float UIMax	= 50.0f;
    float UIStep= 0.25f;
> = 2.0f;

float furSelfShadowMin : furSelfShadowMin0
<
    int nostrip	= 1;	// dont strip
    string UIName	= "Fur: Self Shadow Min";
    float UIMin	= 0.0;
    float UIMax	= 1.0f;
    float UIStep= 0.05f;
> = 0.45f;

float furStiffness : furStiffness0
<
    int nostrip	= 1;	// dont strip
    string UIName	= "Fur: Stiffness";
    float UIMin	= 0.0;
    float UIMax	= 1.0f;
    float UIStep= 0.05f;
> = 0.75f;

float furAOBlend : furAOBlend0
<
    int nostrip	= 1;	// dont strip
    string UIName	= "Fur: AO Blend";
    float UIMin	= 0.0;
    float UIMax	= 1.0f;
    float UIStep= 0.05f;
> = 1.0f;

float3 furAttenCoef : furAttenCoef0
<
    int nostrip		= 1;	// dont strip
    string UIName	= "Fur: rounding,taper,bias";
    float UIMin	= -1.0f;
    float UIMax	= 4.0f;
    float UIStep= 0.01f;
> = float3(1.21f, -0.22f, 0.0f);


float3 furGlobalParams : furGlobalParams0
<
	int nostrip		= 1;	// dont strip
> = float3(0.0f, 0.0f, 1.0f/255.0f);

float4 furBendParams : furGlobalParams1
<
    int nostrip		= 1;	// dont strip
> = float4(0.0f, 0.0f, 0.0f, 0.0f);

#endif //PEDSHADER_FUR

EndConstantBufferDX10( ped_common_locals2 )

#if PEDSHADER_FUR
// This texture contains a hair noise texture. The diffuse map's alpha channel contains 
// a smooth length height field. Multiplied together we get the actual hair length.
BeginSampler(sampler2D, noiseTexture, NoiseSampler, noiseTex)
string	UIName = "Fur Noise Texture";
string TCPTemplate="FurNoise";
string TextureType="Height";
int nostrip=1;						
ContinueSampler(sampler2D, noiseTexture, NoiseSampler, noiseTex)
AddressU = Wrap;
AddressV = Wrap;
MinFilter = MINANISOTROPIC;
MagFilter = MAGLINEAR;
MipFilter = MIPLINEAR;
ANISOTROPIC_LEVEL(4)
EndSampler;
#endif //PEDSHADER_FUR


#if PED_HAIR_ANISOTROPIC
	
		BeginSampler(sampler2D,anisoNoiseSpecTexture,AnisoNoiseSpecSampler,AnisoNoiseSpecTex)
			string UIName="Aniso Noise Map";			
			int nostrip=1;						
			string TCPTemplate="Diffuse";
			string TextureType="AnisoNoise";
		ContinueSampler(sampler2D,anisoNoiseSpecTexture,AnisoNoiseSpecSampler,AnisoNoiseSpecTex)
			AddressU  = WRAP;        
			AddressV  = WRAP;
			MIPFILTER = MIPLINEAR;
			MINFILTER = HQLINEAR;
			MAGFILTER = MAGLINEAR;
		EndSampler;
#endif // PED_HAIR_ANISOTROPIC

#if PED_HAIR_DIFFUSE_MASK
	BeginSampler(sampler2D,diffuseMaskTexture,DiffuseMaskSampler,DiffuseMaskTex)
		string UIName="Diffuse Mask";			
		int nostrip=1;						
		string	TCPTemplate = "Diffuse";
		string	TextureType="Diffuse";
	ContinueSampler(sampler2D,diffuseMaskTexture,DiffuseMaskSampler,DiffuseMaskTex)
		AddressU  = WRAP;        
		AddressV  = WRAP;
		MIPFILTER = MIPLINEAR;
		MINFILTER = HQLINEAR;
		MAGFILTER = MAGLINEAR;
	EndSampler;
#endif // PED_HAIR_DIFFUSE_MASK

#include "../PostFX/SeeThroughFX.fxh"

#ifdef USE_DEFAULT_TECHNIQUES
	#define TECHNIQUES						(1)
#else
	#define TECHNIQUES						(0)
#endif	// USE_DEFAULT_TECHNIQUES

#ifdef USE_REFLECT
	#ifdef USE_SPHERICAL_REFLECT
		#define REFLECT_SAMPLER				sampler2D
		#define REFLECT_TEXOP				tex2D
		#define REFLECT_SPHERICAL			(1)
		#define REFLECT_DYNAMIC				(0)
	#else // REFLECT_SPHERICAL
		#ifdef USE_DYNAMIC_REFLECT
			#define REFLECT_SAMPLER			sampler2D
			#define REFLECT_TEXOP			tex2D
			#define REFLECT_SPHERICAL		(0)
			#define REFLECT_DYNAMIC			(1)
		#else // USE_DYNAMIC_REFLECT
			#define REFLECT_SAMPLER			samplerCUBE
			#define REFLECT_TEXOP			texCUBE
			#define REFLECT_SPHERICAL		(0)
			#define REFLECT_DYNAMIC			(0)
		#endif	// USE_DYNAMIC_REFLECT
	#endif // REFLECT_SPHERICAL
	#define REFLECT							(1)
#else
	#define REFLECT							(0)
#endif	// USE_REFLECT

#if __XENON
float4x4 clothParentMatrix			: clothParentMatrix REGISTER2(vs,	c208)
<
	int nostrip		= 1;	// dont strip
>;

#if PED_CLOTH
void ClothTransform( int idx0, int idx1, int idx2, float4 weight, out float3 pos, out float3 normal)
{
	float3 pos0,pos1,pos2;

	// Read indices from position1 stream (secondary vert stream).
	asm
	{
		vfetch pos0.xyz, idx0, position1;
		vfetch pos1.xyz, idx1, position1;
		vfetch pos2.xyz, idx2, position1;
	};

	float3 temp = pos0;
	pos0.x = dot(clothParentMatrix[0],temp);
	pos0.y = dot(clothParentMatrix[1],temp);
	pos0.z = dot(clothParentMatrix[2],temp);

	temp = pos1;
	pos1.x = dot(clothParentMatrix[0],temp);
	pos1.y = dot(clothParentMatrix[1],temp);
	pos1.z = dot(clothParentMatrix[2],temp);

	temp = pos2;
	pos2.x = dot(clothParentMatrix[0],temp);
	pos2.y = dot(clothParentMatrix[1],temp);
	pos2.z = dot(clothParentMatrix[2],temp);

	float3 v1 = pos0 - pos1;
	float3 v2 = pos2 - pos1;
	float3 n = normalize( cross( v1, v2) );

	pos =				(weight.z) * pos0 + 
						(weight.y) * pos1 + 
						(weight.x) * pos2 +
						((weight.w)-0.5f)*0.1f * n;						
	normal = -n;
}
#endif // PED_CLOTH
#endif // __XENON

#if REFLECT
	BeginSampler(REFLECT_SAMPLER,envTexture,EnvironmentSampler,EnvironmentTex)
		string UIName="Environment Texture";
		string ResourceType = "Cube";
		string TCPTemplate="Diffuse";
		string TextureType="EnvironmentCube";
	ContinueSampler(REFLECT_SAMPLER,envTexture,EnvironmentSampler,EnvironmentTex)
		MinFilter = HQLINEAR;
		MagFilter = MAGLINEAR;
		MipFilter = MIPLINEAR;
	EndSampler;
#endif	// REFLECT...


//
//
// extra cloth stream
//
struct pedClothStream
{
	float3 pos			: POSITION1;
};

//
//
// like rageVertexInputBump, but extended:
//
struct pedVertexInputBump
{
    float3 pos			: POSITION;
	float4 diffuse		: COLOR0;
#if PED_CPV_WIND || PEDSHADER_FAT || PEDSHADER_FUR
	float4 specular		: COLOR1;
#endif
	float2 texCoord0	: TEXCOORD0;

#if PED_HAIR_DIFFUSE_MASK
	float2 texCoord1	: TEXCOORD1;
#endif
#if PED_DAMAGE_TARGETS
	float2 texCoord1	: TEXCOORD1;
#endif

	float3 normal		: NORMAL;
	float4 tangent		: TANGENT0;
};

// same as above, but with no fat/wind support. (seems to be used on in the non deferred techniques)
struct pedVertexNoFatOrWindInputBump {
	// This covers the default rage vertex format (non-skinned)
    float3 pos			: POSITION;
	float4 diffuse		: COLOR0;
	float2 texCoord0	: TEXCOORD0;
#if PED_DAMAGE_TARGETS
	float2 texCoord1	: TEXCOORD1;
#endif
    float3 normal		: NORMAL;
	float4 tangent		: TANGENT0;
};

//
//
// custom seethrough struct
//
struct pedVertexInputSkinSeeThrough
{
	vertexInputSkinSeeThrough seeThrough;
#if SPEC_MAP	
	float2 texCoord0	: TEXCOORD0;
#endif	
};

struct pedVertexInputSeeThrough
{
	vertexInputSeeThrough seeThrough;
#if SPEC_MAP	
	float2 texCoord0	: TEXCOORD0;
#endif	
};

struct pedVertexOutputSeeThrough
{
	vertexOutputSeeThrough seeThrough;
#if SPEC_MAP	
	float2 texCoord		: TEXCOORD1;
#endif	
};


// VARIABLES THAT ARE SET BASED ON SWITCHES ENABLED

BeginSampler(sampler2D,bumpTexture,BumpSampler,BumpTex)
#if NORMAL_MAP
	string UIName="Bump Texture";
	string UIHint="normalmap";
	string TCPTemplate="NormalMap";
	string TextureType="Bump";
#endif
	int nostrip=1;	// dont strip
ContinueSampler(sampler2D,bumpTexture,BumpSampler,BumpTex)
	AddressU  = WRAP;        
	AddressV  = WRAP;
#if defined(USE_ANISOTROPIC)
		MINFILTER = MINANISOTROPIC;
		MAGFILTER = MAGLINEAR;
		ANISOTROPIC_LEVEL(4)
#else
		MINFILTER = HQLINEAR;
		MAGFILTER = MAGLINEAR; 
#endif

#if defined(USE_HIGH_QUALITY_TRILINEARTHRESHOLD) && !__MAX
	TRILINEARTHRESHOLD=0;
#endif

EndSampler;



#define SPECULAR_PS_INPUT	IN.texCoord.xy
#define SPECULAR_VS_INPUT	IN.texCoord0.xy
#define SPECULAR_VS_OUTPUT	OUT.texCoord.xy
#define SPECULAR_UV1		0

BeginSampler(sampler2D,specTexture,SpecSampler,SpecularTex)
#if SPEC_MAP
	string UIName="Specular Texture";
	#if SPECULAR_UV1
		string UIHint="specularmap,uv1";
	#else
		string UIHint="specularmap";
	#endif
	string TCPTemplate="Specular";
	string TextureType="Specular";
#endif
	int nostrip=1;	// dont strip
ContinueSampler(sampler2D,specTexture,SpecSampler,SpecularTex)
	AddressU  = WRAP;        
	AddressV  = WRAP;
	MIPFILTER = MIPLINEAR;
	MINFILTER = HQLINEAR;
	MAGFILTER = MAGLINEAR;
EndSampler;

#define DIFFUSE_PS_INPUT		IN.texCoord.xy
#define DIFFUSE_VS_INPUT		IN.texCoord0.xy
#define DIFFUSE_VS_OUTPUT		OUT.texCoord.xy

#if PED_HAIR_DIFFUSE_MASK
	#define DIFFUSE2_PS_INPUT		IN.texCoord2.xy
	#define DIFFUSE2_VS_INPUT		IN.texCoord1.xy
	#define DIFFUSE2_VS_OUTPUT		OUT.texCoord2.xy
#endif



#if PEDSHADER_FUR

#define furLayerOffset	(furGlobalParams.x)	// x=fur offset along the normal (0.0f)
#define furLayerShd		(furGlobalParams.y)	// y=fur "shadowing" AO (0.0f)
#define	furAlphaClip	(furGlobalParams.z)	// z=fur alpha clip level (1.0f/255.0f)
#define furBendVector   (furBendParams.xyz)
#define furBendFactor   (furBendParams.w)

//
//
//
//
float3 VS_ApplyFurTransform(float3 inPos, float3 inWorldNormal, float lenScale)
{
float3 outPos = inPos;
    float3 bentNormal = lerp(inWorldNormal, furBendVector, furBendFactor);
    normalize(bentNormal);
	outPos += bentNormal*furLayerOffset*lenScale;
	return outPos;
}

//
//
//
//
float2 VS_CalculateFurPsParams(float lenScale)
{
    float2 outFurParams;

	// store fur "shading":
	outFurParams.x = furLayerShd;

	// furAlphaClipLevel:
    // doing a steep linear function to mask out near-0 lengths, with a smooth fade
    float invscale = saturate(1.0-lenScale*FLT_MAX);
	outFurParams.y  = saturate(furAlphaClip + invscale);

	return outFurParams;
}

//
//
//
//
half4 PS_ApplyFur(half4 inSurfaceDiffuse, half2 diffuseTexCoord, half2 furParams)
{
	half4 outDiffuse = inSurfaceDiffuse;
    half4 noise = h4tex2D(NoiseSampler, diffuseTexCoord * furNoiseUVScale);
    // blend with diffuse texture's alpha channel
    outDiffuse.a *= noise.x;

	// fur "shading":
    outDiffuse.a -= furParams.y;
	half furShadow = furParams.x;
	outDiffuse.rgb *= furShadow;
#if RSG_PC || RSG_DURANGO || RSG_ORBIS
    outDiffuse.a *= 4.0;
#endif
	return outDiffuse;
}
#endif //PEDSHADER_FUR...


#if PED_HAIR_DIFFUSE_MASK
float PS_ApplyDiffuseAlphaMask(float inAlpha, float2 texCoord)
{
	float outAlpha = inAlpha;
	float4 mask = tex2D(DiffuseMaskSampler, texCoord);
	outAlpha *= mask.r;
	return outAlpha;
}
#endif //PED_HAIR_DIFFUSE_MASK...

#if PED_COLOR_VARIATION_TEXTURE
//
//
//
//
half4 PS_PalettizeColorVariationTexture(half4 IN_diffuseColor)
{
	half4 OUT = IN_diffuseColor;

	const half texAlpha = floor/*ceil*/(IN_diffuseColor.a * 255.01f);

	const half alphaMin	= 32;
	const half alphaMax	= 160;

#if __SHADERMODEL >= 40
	// shift range to <0; 1>
	half texAlpha0 = (texAlpha-alphaMin) / (alphaMax-alphaMin);
	//texAlpha0 = clamp(texAlpha0, 0.0f, 1.0f);

	// sample palette:
	const half paletteRow = paletteSelector;	// this must be converted in higher CustomShaderFX code
	half4 newDiffuse = h4tex2D(TextureSamplerDiffPal, float2(texAlpha0, paletteRow));
#endif


	if(	(texAlpha >= (alphaMin/*+2*/)) && (texAlpha <= (alphaMax/*-2*/)))
//		if(	GtaStep(texAlpha, alphaMin) && GtaStep(alphaMax, texAlpha))
	{
#if __SHADERMODEL < 40
		// shift range to <0; 1>
		half texAlpha0 = (texAlpha-alphaMin) / (alphaMax-alphaMin);
		//texAlpha0 = clamp(texAlpha0, 0.0f, 1.0f);

		// sample palette:
		const half paletteRow = paletteSelector;	// this must be converted in higher CustomShaderFX code
		half4 newDiffuse = h4tex2D(TextureSamplerDiffPal, float2(texAlpha0, paletteRow));
#endif

#if 0
		// old: palette's alpha decides about color mix ratio:
		const half alphablend = newDiffuse.a;
		OUT.rgb	= newDiffuse.rgb*alphablend + IN_diffuseColor.rgb*(1.0f-alphablend);
#else
		// new: palette's works as a tint:
		OUT.rgb = IN_diffuseColor.rgb * newDiffuse.rgb;
#endif
		OUT.a	= 1.0f;
	}

	return OUT;
}// end of PalettizeColorVariationTexture()...
#endif //PED_COLOR_VARIATION_TEXTURE...


#if HAIR_TINT_TECHNIQUES
half4 PS_ApplyHairRamp(half4 IN_DiffuseColor)
{
	half4 detail = IN_DiffuseColor;
	half paletteSelector2 = StubbleGrowth.x;
	half3 newDiffuse = h4tex2D(TextureSamplerDiffPal, float2(detail.g, paletteSelector)).rgb;

	if (paletteSelector2 != paletteSelector)
	{
		half3 secondDiffuse = h4tex2D(TextureSamplerDiffPal, float2(detail.g, paletteSelector2)).rgb;
		newDiffuse = secondDiffuse * detail.r + newDiffuse * (1.f - detail.r);
	}
	
	half4 OUT = float4(newDiffuse.rgb, IN_DiffuseColor.a);

	return OUT;
}
#endif // HAIR_TINT_TECHNIQUES


BeginConstantBufferDX10(pedmaterial)

// snow and fat Step:
#if PEDSHADER_SNOW_MAX	// max doesn't like float2 vars
	float envEffFatThickness0m : envEffFatThickness0m
	<
		int nostrip	= 1;	// dont strip
	#if PEDSHADER_SNOW
		string UIName	= "EnvEff: Max thickness";
	#endif
		int UIMin	= 0;
		int UIMax	= 100;
		int UIStep	= 1;
	> = float(1.0f);
	float envEffFatThickness1m : envEffFatThickness1m
	<
		int nostrip	= 1;	// dont strip
		string UIName	= "Fat: Max thickness";
		int UIMin	= 0;
		int UIMax	= 100;
		int UIStep	= 1;
	> = float(25.0f);
	#define snowStep	(envEffFatThickness0m)
	#define fatStep		(envEffFatThickness1m)
#else
	float2 envEffFatThickness : envEffFatThickness0
	<
		int nostrip	= 1;	// dont strip
	#if PEDSHADER_SNOW
		string UIName	= "EnvEfFatMaxThick";
	#endif
		int UIMin	= 0;
		int UIMax	= 100;
		int UIStep	= 1;
	> = float2(25.0f, 25.0f);	// must be divided by 1000 (=0.025f) - rescaling of snowStep done with snowScale
	#define snowStep	(envEffFatThickness.x)	// see note above
	#define fatStep		(envEffFatThickness.y)	// see note above
#endif




EndConstantBufferDX10(pedmaterial)


#if PEDSHADER_SNOW
//
//
// make it last sampler as Rick's Max scripts depend on shader ordering:
//
BeginSampler(sampler2D,snowTexture,SnowSampler,SnowTex)
	string UIName="EnvEff Texture";
	string UIHint="EnvironmentEffect map";
	string TCPTemplate="Diffuse";
	string TextureType="EnvironmentEffect";
#if __WIN32PC || RSG_DURANGO || RSG_ORBIS
	int nostrip=1;	// dont strip
#endif
ContinueSampler(sampler2D,snowTexture,SnowSampler,SnowTex)
	AddressU  = WRAP;        
	AddressV  = WRAP;
	MIPFILTER = MIPLINEAR;
	MINFILTER = HQLINEAR;
	MAGFILTER = MAGLINEAR;
EndSampler;

//
//
// calculates snowBlend for both ranges:
// snowAmount = <0;  127> : blend textures only
// snowAmount = <128;255> : blend textures + blend normals
//
float2 VS_CalculateSnowBlend(float snowAmount)
{
float2 snowBlend;

	// blending <0;1> for 0-127, 0 otherwise:
	snowBlend.x = (snowAmount + envEffCpvAdd * any1(snowAmount)) * 2.0f;	// shift CPV value only for non-zero CPV.b	// GtaStep(0.5f, snowAmount);

	// blending <0;1> for 128-255, 0 otherwise:
	snowBlend.y = saturate(snowAmount-0.5f)						* 2.0f;	

	return snowBlend;
}

//
//
//
//
float3 VS_ApplySnowTransform(float3 inPos, float3 inWorldNormal, float2 snowBlend)
{
	float3 outPos = inPos;

	//float snowStep0 = 0.025f;	//0.035f;
#if PEDSHADER_SNOW_MAX
	// test: Max DX parser doesn't like UI vars used directly in VS:
	outPos += inWorldNormal * /*snowStep* */ snowBlend.y /** snowScaleV*/ * 0.0001f;
#else
	outPos += inWorldNormal * snowStep * snowBlend.y * snowScaleV;
#endif
//	outPos.z += inWorldNormal.z	* snowStep0 * snowAmount;

	return outPos;
}

//
//
//
//
float VS_CalculateSnowPsParams(float2 snowBlend)
{
	// store snow "shading":
	float outSnowParams	= saturate(snowBlend.x + snowBlend.y) * snowScale;

	return outSnowParams;
}

//
//
//
//

half3 PS_ApplySnow(half3 inSurfaceDiffuse, half snowParams, float2 diffuseUV)
{
	half3 outDiffuse = inSurfaceDiffuse;

#define USE_SNOW_BRANCH (!__PS3 && !__PSSL) // Branch improves Xenon (and presumably WIN32PC) ped_wrinkle.fx performance by ~10-20%

#if USE_SNOW_BRANCH
	IFANY_BRANCH if ( snowParams )
#endif // USE_SNOW_BRANCH
	{

		half snowAmount = snowParams;

		half3 snowColor = h3tex2D(SnowSampler, diffuseUV.xy) * envEffColorMod.rgb;

		// snow "shading":
		outDiffuse = lerp(inSurfaceDiffuse, snowColor.rgb, snowAmount);
	}

	return outDiffuse;
}
#endif //PEDSHADER_SNOW...


#if PEDSHADER_FAT
//
//
//
//
float3 VS_ApplyFatTransform(float3 inPos, float3 inWorldNormal, float fatBlend)
{
	float3 outPos = inPos;

	outPos += inWorldNormal * fatStep * fatBlend * fatScaleV;

	return outPos;
}
#endif //PEDSHADER_FAT...



BeginConstantBufferDX10( pedmisclocals )

#if PED_HAIR_ANISOTROPIC 
	float AnisotropicAlphaBias : AnisotropicAlphaBias
	<
		int nostrip=1;	// dont strip	
		string UIWidget = "slider";
		string UIName = "Aniso Alpha Bias";
		float UIMin = 0.0;
		float UIMax = 1.;
		float UIStep = 0.001;
	> = 1.f;
#else
	#define AnisotropicAlphaBias 1.
#endif //PED_HAIR_ANISOTROPIC


// NOTE: this needs to change if CPedDamageSetBase::kTargetSizeW or CPedDamageSetBase::kTargetSizeH change in pedDamage.h
#define BloodTexelRatio				(256./640.) // #define instead of passed in because it's the only part of this array needed for the vertex shader (and it should not be changing) 

#if PED_DAMAGE_TARGETS
	#if !__MAX && !RSG_ORBIS
		passthrough { // optimisation: declare masks[] as read-only const table
	#endif
		//This needs to be an IMMEDIATECONSTANT so it get initialised correctly on DX11.
		IMMEDIATECONSTANT float4 BloodZoneAdjust[6] = {{4.0/10.0, 0.0,      1.0, 1.0}, {2.0/10.0, 4.0/10.0, 1.0, 1.0},	// Torso, Head
										   {1.0/10.0, 6.0/10.0,-1.0, 1.0}, {1.0/10.0, 7.0/10.0, 1.0,-1.0},	// Left Arm, Right Arm
										   {1.0/10.0, 8.0/10.0, 1.0, 1.0}, {1.0/10.0, 9.0/10.0, 1.0, 1.0}};	// Left Leg, Right Leg
	#if !__MAX && !RSG_ORBIS
		}
	#endif
#endif //PED_DAMAGE_TARGETS
#if PED_CPV_WIND
//
//
// micro-movement params: [globalScaleH | globalScaleV | globalArgFreqH | globalArgFreqV ]
//
float4 umGlobalParams : umGlobalParams0 
<
	string UIWidget	= "slider";
	string UIName	= "Wind:SclHV/FrqHV";
	float UIMin		= 0.0;
	float UIMax		= 10.0;
	float UIStep	= 0.001;
> = float4(0.0025f, 0.0025f, 7.000f, 7.000f);
#endif //PED_CPV_WIND

EndConstantBufferDX10( endmisclocals )

#if PED_CPV_WIND
// micro-movement params: [globalScaleH | globalScaleV | globalArgFreqH | globalArgFreqV ]
#define umGlobalScaleH			(umGlobalParams.x)	// (0.0025f)micro-movement: globalScaleHorizontal
#define umGlobalScaleV			(umGlobalParams.y)	// (0.0025f)micro-movement: globalScaleVertical
#define umGlobalArgFreqH		(umGlobalParams.z)	// (7.000f)	micro-movement: globalArgFreqHorizontal
#define umGlobalArgFreqV		(umGlobalParams.w)	// (7.000f)	micro-movement: globalArgFreqVertical

//
//
//
//
float3 VS_PedApplyMicromovements(float3 vertpos, float3 IN_Color0)
{
	float3 newVertpos;

	// local micro-movements of plants:
	float umScaleH	= IN_Color0.r		* umGlobalScaleH * umOverrideScaleH;	// horizontal movement scale (red0:   255=max, 0=min)
	float umScaleV	= IN_Color0.b		* umGlobalScaleV * umOverrideScaleV;	// vertical movement scale	 (blue0:  0=max, 255=min)
	float umArgPhase= IN_Color0.g		* 2.0f*PI;		// phase shift               (green0: phase 0-1     )

	float3 uMovementArg			= float3(gUmGlobalTimer, gUmGlobalTimer, gUmGlobalTimer);
	float3 uMovementArgPhase	= float3(umArgPhase, umArgPhase, umArgPhase);
	float3 uMovementScaleXYZ	= float3(umScaleH, umScaleH, umScaleV);

	uMovementArg			*= float3(umGlobalArgFreqH, umGlobalArgFreqH, umGlobalArgFreqV) * float3(umOverrideArgFreqH, umOverrideArgFreqH, umOverrideArgFreqV);
	uMovementArg			+= uMovementArgPhase;
	float3 uMovementAdd		=  sin(uMovementArg);
	uMovementAdd			*= uMovementScaleXYZ;

	// add final micro-movement:
	newVertpos = vertpos.xyz + uMovementAdd;

	return newVertpos.xyz;
}
#endif //PED_CPV_WIND...


#define SHADOW_CASTING            (1)
#define SHADOW_CASTING_TECHNIQUES (1 && SHADOW_CASTING_TECHNIQUES_OK)
#define SHADOW_RECEIVING          (__SHADERMODEL >= 40)
#define SHADOW_RECEIVING_VS       (0)
#include "../Lighting/Shadows/localshadowglobals.fxh"
#include "../Lighting/Shadows/cascadeshadows.fxh"
#include "../Lighting/lighting.fxh"
#include "../Lighting/forward_lighting.fxh"
#include "../Debug/EntitySelect.fxh"
#include "ped_common_values.h"


#if DX11_CLOTH

#pragma constant 0

CBSHARED BeginConstantBufferPagedDX10(pedcloth, b11)

shared ROW_MAJOR float4x4 clothParentMatrix			: clothParentMatrix;


shared float3 clothVertices[DX11_CLOTH_MAX_VERTICES]: clothVertices;
EndConstantBufferDX10( pedcloth )

#endif // DX11_CLOTH

#if PED_CLOTH && DX11_CLOTH
void ClothTransform( int idx0, int idx1, int idx2, float4 weight, out float3 pos, out float3 normal)
{
	float3 pos0,pos1,pos2;

	pos0 = clothVertices[idx0];
	pos1 = clothVertices[idx1];
	pos2 = clothVertices[idx2];

	float3 temp = pos0;
	pos0.x = dot(clothParentMatrix[0].xyz,temp);
	pos0.y = dot(clothParentMatrix[1].xyz,temp);
	pos0.z = dot(clothParentMatrix[2].xyz,temp);

	temp = pos1;
	pos1.x = dot(clothParentMatrix[0].xyz,temp);
	pos1.y = dot(clothParentMatrix[1].xyz,temp);
	pos1.z = dot(clothParentMatrix[2].xyz,temp);

	temp = pos2;
	pos2.x = dot(clothParentMatrix[0].xyz,temp);
	pos2.y = dot(clothParentMatrix[1].xyz,temp);
	pos2.z = dot(clothParentMatrix[2].xyz,temp);

	float3 v1 = pos0 - pos1;
	float3 v2 = pos2 - pos1;
	float3 n = -normalize( cross( v1, v2) );

	pos =				(weight.z) * pos0 + 
						(weight.y) * pos1 + 
						(weight.x) * pos2 +
						((weight.w)-0.5f)*0.1f * n;
	normal = -n;
}
#endif // PED_CLOTH && DX11_CLOTH

#if PED_WRINKLE 
//
//
//
//
#if RSG_PC && __SHADERMODEL>=40
#pragma sampler 7
#endif // RSG_PC
BeginSampler(sampler2D,wrinkleMaskTexture_0,WrinkleMaskSampler_0,WrinkleMaskTexture_0)
	string UIName="Wrinkle Mask 0";
	string TCPTemplate="Diffuse";
	string TextureType="WrinkleMask";
ContinueSampler(sampler2D,wrinkleMaskTexture_0,WrinkleMaskSampler_0,WrinkleMaskTexture_0)
	AddressU  = WRAP;        
	AddressV  = WRAP;
	MIPFILTER = MIPLINEAR;
	MINFILTER = HQLINEAR;
	MAGFILTER = MAGLINEAR;
EndSampler;

BeginSampler(sampler2D,wrinkleMaskTexture_1,WrinkleMaskSampler_1,WrinkleMaskTexture_1)
	string UIName="Wrinkle Mask 1";
	string TCPTemplate="Diffuse";
	string TextureType="WrinkleMask";
ContinueSampler(sampler2D,wrinkleMaskTexture_1,WrinkleMaskSampler_1,WrinkleMaskTexture_1)
	AddressU  = WRAP;        
	AddressV  = WRAP;
	MIPFILTER = MIPLINEAR;
	MINFILTER = HQLINEAR;
	MAGFILTER = MAGLINEAR;
EndSampler;

BeginSampler(sampler2D,wrinkleMaskTexture_2,WrinkleMaskSampler_2,WrinkleMaskTexture_2)
	string UIName="Wrinkle Mask 2";
	string TCPTemplate="Diffuse";
	string TextureType="WrinkleMask";
ContinueSampler(sampler2D,wrinkleMaskTexture_2,WrinkleMaskSampler_2,WrinkleMaskTexture_2)
	AddressU  = WRAP;        
	AddressV  = WRAP;
	MIPFILTER = MIPLINEAR;
	MINFILTER = HQLINEAR;
	MAGFILTER = MAGLINEAR;
EndSampler;

BeginSampler(sampler2D,wrinkleMaskTexture_3,WrinkleMaskSampler_3,WrinkleMaskTexture_3)
	string UIName="Wrinkle Mask 3";
	string TCPTemplate="Diffuse";
	string TextureType="WrinkleMask";
ContinueSampler(sampler2D,wrinkleMaskTexture_3,WrinkleMaskSampler_3,WrinkleMaskTexture_3)
	AddressU  = WRAP;        
	AddressV  = WRAP;
	MIPFILTER = MIPLINEAR;
	MINFILTER = HQLINEAR;
	MAGFILTER = MAGLINEAR;
EndSampler;

#if PED_WRINKLE_CS
BeginSampler(sampler2D,wrinkleMaskTexture_4,WrinkleMaskSampler_4,WrinkleMaskTexture_4)
	string UIName="Wrinkle Mask 4";
	string TCPTemplate="Diffuse";
	string TextureType="WrinkleMask";
ContinueSampler(sampler2D,wrinkleMaskTexture_4,WrinkleMaskSampler_4,WrinkleMaskTexture_4)
	AddressU  = WRAP;        
	AddressV  = WRAP;
	MIPFILTER = MIPLINEAR;
	MINFILTER = HQLINEAR;
	MAGFILTER = MAGLINEAR;
EndSampler;

BeginSampler(sampler2D,wrinkleMaskTexture_5,WrinkleMaskSampler_5,WrinkleMaskTexture_5)
	string UIName="Wrinkle Mask 5";
	string TCPTemplate="Diffuse";
	string TextureType="WrinkleMask";
ContinueSampler(sampler2D,wrinkleMaskTexture_5,WrinkleMaskSampler_5,WrinkleMaskTexture_5)
	AddressU  = WRAP;        
	AddressV  = WRAP;
	MIPFILTER = MIPLINEAR;
	MINFILTER = HQLINEAR;
	MAGFILTER = MAGLINEAR;
EndSampler;
#endif //PED_WRINKLE_CS...

BeginSampler(sampler2D,wrinkleTexture_A,WrinkleSampler_A,WrinkleTexture_A)
	string UIName="Wrinkle A";
	string UIHint="NORMAL_MAP";
	string TCPTemplate="NormalMap";
	string TextureType="WrinkleNormal";
ContinueSampler(sampler2D,wrinkleTexture_A,WrinkleSampler_A,WrinkleTexture_A)
	AddressU  = WRAP;        
	AddressV  = WRAP;
	MIPFILTER = MIPLINEAR;
	MINFILTER = HQLINEAR;
	MAGFILTER = MAGLINEAR;
EndSampler;

BeginSampler(sampler2D,wrinkleTexture_B,WrinkleSampler_B,WrinkleTexture_B)
	string UIName="Wrinkle B";
	string UIHint="NORMAL_MAP";
	string TCPTemplate="NormalMap";
	string TextureType="WrinkleNormal";
ContinueSampler(sampler2D,wrinkleTexture_B,WrinkleSampler_B,WrinkleTexture_B)
	AddressU  = WRAP;        
	AddressV  = WRAP;
	MIPFILTER = MIPLINEAR;
	MINFILTER = HQLINEAR;
	MAGFILTER = MAGLINEAR;
EndSampler;

//
//
//
//
half3 PS_PedGetWrinkledNormal(float2 texCoord, half3 worldNormal, half3 worldTangent, half3 worldBinormal)
{
#if PED_WRINKLE_CS
	return PS_GetWrinkledNormalCS(BumpSampler,
								WrinkleSampler_A,
								WrinkleSampler_B,
								WrinkleMaskSampler_0,
								WrinkleMaskSampler_1,
								WrinkleMaskSampler_2,
								WrinkleMaskSampler_3,
								WrinkleMaskSampler_4,
								WrinkleMaskSampler_5,
								wrinkleMaskStrengths0,
								wrinkleMaskStrengths1,
								wrinkleMaskStrengths2,
								wrinkleMaskStrengths3,
								wrinkleMaskStrengths4,
								wrinkleMaskStrengths5,
								texCoord,
								bumpiness,
								worldNormal,
								worldTangent,
								worldBinormal);
#else
	return PS_GetWrinkledNormal(BumpSampler,
								WrinkleSampler_A,
								WrinkleSampler_B,
								WrinkleMaskSampler_0,
								WrinkleMaskSampler_1,
								WrinkleMaskSampler_2,
								WrinkleMaskSampler_3,
								wrinkleMaskStrengths0,
								wrinkleMaskStrengths1,
								wrinkleMaskStrengths2,
								wrinkleMaskStrengths3,
								texCoord,
								bumpiness,
								worldNormal,
								worldTangent,
								worldBinormal);
#endif
}
#endif //PED_WRINKLE...





///////////////////////////////////////////////////////////////////////////////////////////////////////
//
//
//
//
struct pedVertexOutput
{
#if PED_DAMAGE_TARGETS  // is doing ped damae, we piggy back one other interpolators.
	#if WET_PED_EFFECT && 0		// currently we do not do wetness on non deferred passes 
		float4 texCoord					: TEXCOORD0;    // z if wetness, w holds peddamage extraData1
	#else
		float4 texCoord					: TEXCOORD0;    // z is free, w holds peddamage extraData1
	#endif
		float4 worldNormal				: TEXCOORD1;	// [ nx | ny | nz ], w holds peddamage extraData2
#else
	#if WET_PED_EFFECT && 0		// currently we do not do wetness on non deferred passes 
		float3 texCoord					: TEXCOORD0;    // z if wetness, w is free
	#else
		float2 texCoord					: TEXCOORD0;    // z is free, w is free
	#endif
		float3 worldNormal				: TEXCOORD1;	// [ nx | ny | nz ], w is free
#endif

	float4 worldPos					: TEXCOORD2;

	float4 color0					: TEXCOORD3;

#if REFLECT
	float3 surfaceToEye				: TEXCOORD4;	
#endif

#if NORMAL_MAP
	float3 worldTangent				: TEXCOORD5;
	float3 worldBinormal			: TEXCOORD6;
#endif // NORMAL_MAP

#if PED_DAMAGE_TARGETS
	float4 damageData				: TEXCOORD7;		// could we pack this in some other interpolators
#endif

#if 0 && PEDSHADER_SNOW_MAX
	float  snowParams				: TEXCOORD8;
#endif

    DECLARE_POSITION_CLIPPLANES(pos)
};


//
//
//
//
struct pedVertexOutputWaterReflection
{
	float2 texCoord					: TEXCOORD0;    // w is free to carry something else
	float3 worldNormal				: TEXCOORD1;	// [ nx | ny | nz ]
	float3 worldPos					: TEXCOORD2;
	float4 color0					: TEXCOORD3;

	DECLARE_POSITION_CLIPPLANES(pos)
};


//
//
//
//
struct pedVertexOutputD
{
    DECLARE_POSITION(pos)
    
#if PED_DAMAGE_TARGETS									// if doing ped damage, we piggy back one other interpolators.
	#if WET_PED_EFFECT
		float4 texCoord					: TEXCOORD0;    // z if wetness, w holds peddamage extraData1
	#else
		float4 texCoord					: TEXCOORD0;    // z is free, w holds peddamage extraData1
	#endif
		float4 worldNormal				: TEXCOORD1;	// [ nx | ny | nz ], w holds peddamage extraData2
#else
	#if WET_PED_EFFECT		
		float3 texCoord					: TEXCOORD0;    // z if wetness, w is free
	#else
		float2 texCoord					: TEXCOORD0;    // z is free, w is free
	#endif
		float3 worldNormal				: TEXCOORD1;	// [ nx | ny | nz ], w is free
#endif

	float4 color0					: TEXCOORD3;

#if REFLECT || PED_RIM_LIGHT
	float3 surfaceToEye				: TEXCOORD4;
#endif

#if NORMAL_MAP
	float3 worldTangent				: TEXCOORD5;
	float3 worldBinormal			: TEXCOORD6;
#endif // NORMAL_MAP

#if PED_DAMAGE_TARGETS 
	float4 damageData				: TEXCOORD7;
#endif

#if PEDSHADER_SNOW
	float snowParams				: TEXCOORD8;
#endif

#if PEDSHADER_FUR
	float2 furParams				: TEXCOORD9;	// x=furShadowing y=furAlphaClipLevel
#elif PED_HAIR_DIFFUSE_MASK
	float2 texCoord2				: TEXCOORD9;	// x,y=UV1
#endif

#if PED_RIM_LIGHT && PRECOMPUTE_RIM
	float3 rimValues				: TEXCOORD2;
#endif		
};

//
//
// strength of ambient occlusion messed with 
//
float4 pedAdjustAmbientScales(float4 inColor)
{
	float4 outColor = inColor;

	return outColor;
}


//
//
//
//
void pedProcessVertLightingBasic(float3 localPos,
								 float3 localNormal,
								 float4 vertColor,
								 float4x3 worldMtx,
								 out float3 worldPos,
								 out float3 worldEyePos,
								 out float3 worldNormal,
								 out float4 color0
								 )
{
	worldPos		= mul(float4(localPos,1), worldMtx);
	worldNormal		= NORMALIZE_VS(mul(localNormal, (float3x3)worldMtx));
#if NORMAL_SCALE
	worldNormal		= FlipNormalTwoSided(worldNormal, worldEyePos);
#endif // NORMAL_SCALE
	color0			= vertColor;
	worldEyePos		= gViewInverse[3].xyz - worldPos;
}


//
//
//
//
void pedProcessVertLightingUnskinned(	float3		localPos, 
										float3		localNormal, 
						#if NORMAL_MAP
										float4		localTangent, 
						#endif
										float4		vertColor,
										float4x3	worldMtx, 
										out float3	worldPos,
										out float3	surfaceToEye,
										out float3	worldNormal,
						#if NORMAL_MAP
										out float3	worldBinormal,
										out float3	worldTangent,
						#endif
										out float4	color0 )
{
	float3 pos		= mul(float4(localPos,1), worldMtx);
	worldPos		= pos;

	// Store position in eyespace
	surfaceToEye	= gViewInverse[3].xyz - pos;

	// Transform normal by "transpose" matrix
	worldNormal		= NORMALIZE_VS(mul(localNormal, (float3x3)worldMtx));

#if NORMAL_MAP
	// Do the tangent+binormal stuff
	worldTangent	= NORMALIZE_VS(mul(localTangent.xyz, (float3x3)worldMtx));
	worldBinormal	= rageComputeBinormal(worldNormal, worldTangent, localTangent.w);
#endif

	color0 = vertColor;
}

//
//
//
//
void pedProcessVertLightingSkinned( float3		localPos, 
									float3		localNormal, 
					#if NORMAL_MAP
									float4		localTangent, 
					#endif
									float4		vertColor,
									rageSkinMtx	skinMtx, 
									float4x3	worldMtx, 
									out float3	worldPos,
									out float3	surfaceToEye,
									out float3	skinnedNormal,
									out float3	worldNormal,
					#if NORMAL_MAP
									out float3	worldBinormal,
									out float3	worldTangent,
					#endif	
									out float4	color0 )
{
	float3 skinnedPos		= rageSkinTransform(localPos, skinMtx);
	float3 pos				= mul(float4(skinnedPos,1), worldMtx);
	worldPos				= pos;

	// Store position in eyespace
	surfaceToEye			= gViewInverse[3].xyz - pos;

	// Transform normal by "transpose" matrix
	skinnedNormal			= rageSkinRotate(localNormal, skinMtx);
	worldNormal				= NORMALIZE_VS(mul(skinnedNormal, (float3x3)worldMtx));

#if NORMAL_MAP
	// Do the tangent+binormal stuff
	float3 skinnedTangent	= rageSkinRotate(localTangent.xyz, skinMtx);
	worldTangent			= NORMALIZE_VS(mul(skinnedTangent, (float3x3)worldMtx));
	worldBinormal			= rageComputeBinormal(worldNormal, worldTangent, localTangent.w);
#endif // NORMAL_MAP

    color0 = vertColor;
}


#if WET_PED_EFFECT
//
//
// vertex shader helper function to compute the ped wetness params
//
float VS_CalcWetnessParam(float height)
{
//	float2 fadeRange = saturate((height-matWetClothesData.xy)/wetnessAdjust.w);
//	return lerp(matWetClothesData.z, matWetClothesData.w, fadeRange.x) * (1-fadeRange.y);

	// for ped props, we don't have height data in the vertex cpv, so it's set in matWetClothesData.xy directly and matWetClothesData.zw is set to negative
	float2 fadeRange = saturate(((matWetClothesData.w < 0 ? 0 : height) - matWetClothesData.xy)/wetnessAdjust.w);
	return abs(lerp(matWetClothesData.z, matWetClothesData.w, fadeRange.x) * (1-fadeRange.y));
}

//
//
// vertex shader helper function to compute the ped wetness params
//
float VS_CalcWetnessSweatParam(float height, float sweatScale)
{
	// wetness:
	float wetness = VS_CalcWetnessParam(height);

	// sweat:
	float sweat = sweatScaleV * sweatScale;

	return max(wetness, sweat);
}
#endif // WET_PED_EFFECT...

#if PED_DAMAGE_TARGETS


//
//
// vertex shader helper function to compute the ped damage data params
//
float4 CalcPedDamageData(float2 uv, out float extraData1, out float extraData2, uniform bool debug, uniform bool bForwardLighting)
{
	int zone = int((.5-uv.y)/2.0f);										// calculate the damage zone 

#if PED_VERTEX_DAMAGE_MASK
	float bloodEnabled = round(frac(abs(BloodEnabled)/exp2(zone+1)));   // see if blood is on in that section
#else
	float bloodEnabled = BloodEnabled;
#endif

	float4 data;
	data.xy = (!debug || bloodEnabled != 0.0) ? data.xy : 0;			// zero out uvs if damage is disabled

	if (uv.x!=0)
	{
		uv.y = 1-uv.y; // adjust for 1-y mapping
		data.xy = (zone>=2) ? uv.yx  : uv.xy;// need to swap the uv for the legs and arms. 

		// get the scale and offset 
		bool isDoubleWide = IsBloodAndTattooInOne() && PS3_SWITCH(1,bForwardLighting);  // currently on non ps3, we only support side by side for forward lighting.
		float2 scaleOffetDataBlood  = (BloodZoneAdjust[zone].xy + float2(-2,1.5) * OneOverBloodHeight)*(isDoubleWide?.5:1) + float2(0,isDoubleWide?.5:0);
		float2 scaleOffetDataTattoo = (BloodZoneAdjust[zone].xy + float2(-2,1.5) * OneOverTattooHeight)*((isDoubleWide&&IsTattooRotated())?.5:1);     // if double wide is set, but tattoo is vertical, it's a compressed texture and should not use the double wide pixel scaling

		data.zw = scaleOffetDataBlood * BloodZoneAdjust[zone].zw; // add in right/left arm flags
		extraData1 = scaleOffetDataTattoo.x;
		extraData2 = scaleOffetDataTattoo.y;
	}
	else
	{
		data = 0;
		extraData1 = 0;
		extraData2 = 0;
	}

	return data;
}

half4 SamplePedDamage(sampler2D Sampler, float2 TexCoord)
{
#if RSG_PC || RSG_DURANGO || RSG_ORBIS
//On Dx11 platforms we need to sample using tex2dlod to get around:
//error: warning X4121: gradient based operation must be moved outside of flow control to prevent divergence. Performance may be improved by using a non-gradient operation
//with tex2dlod we can force a specific mip avoiding the need for the gradient calculation, as the texture only have 1 mip it shouldn't cause issues.
	return tex2Dlod(Sampler, float4(TexCoord, 0.0f, 0.0f));
#else
	return h4tex2D(Sampler, TexCoord);
#endif
}

// get the medals texel from the decoration RT, using the diffuse texture coords
half4 LookupPedDamageMedals(float2 uv)
{
	float2 remappedUV = float2(uv.x,1-uv.y);

	if (!IsBloodRotated())									// medals are flipped from the normal sideways RT flag. 
		remappedUV.xy = float2(remappedUV.y, 1-remappedUV.x);  

#if __PS3
	if (IsBloodAndTattooInOne())
		remappedUV.x = remappedUV.x*0.5f;					// adjust if dual (side by side) target (NOTE when we get a seperate value for dual target (not combined with rotation) we could just mult and add that value directly, eliminating the if
#endif

	return SamplePedDamage(pedTattooTargetSampler,remappedUV);
}

//
//
// pixel shader helper function to compute the ped damage lighting adjustments
//
half ComputePedDamageAdjustments(float4 damageData, float extraData1,  float extradata2, inout StandardLightingProperties surfaceStandardLightingProperties, inout SurfaceProperties surfaceInfo, half3 IN_Tangent, half3 IN_Binormal, half2 IN_TexCoords, bool bForwardLighting)
{
	half detailMapScale = 1.0f;

#if __PS3
	IFANY_BRANCH		  // disabled for the whole ped, just skip everything (this gets patched by the SPU at to be a static jmp.)
	if (BloodEnabled!=0.0)  // keep it seperate so shader patching works
	{
		if (damageData.z != 0.0)
#else
	if (BloodEnabled*damageData.z != 0.0) // combining is faster.
	{
#endif
		{
			//
			// compute zone wrapped UV coords
			//
			float2 uv = frac(damageData.xy);						 // do the wrapping in local UV space
			
			float2 scaleOffetDataBlood  = abs(damageData.zw);
			float2 scaleOffetDataTattoo = float2(extraData1,extradata2);
			
			float2 bloodUV = float2(uv.x, uv.y*scaleOffetDataBlood.x + scaleOffetDataBlood.y);
			float2 tattooUV = float2(uv.x, uv.y*scaleOffetDataTattoo.x + scaleOffetDataTattoo.y);
			bloodUV  = IsBloodRotated() ? float2(bloodUV.y, 1-bloodUV.x) : bloodUV.xy; 
			tattooUV = IsTattooRotated() ? float2(tattooUV.y, 1-tattooUV.x) : tattooUV.xy;  

			//
			// determine if it's skin so we can turn on/off some features.
			//		
#if !defined(USE_PED_DAMAGE_NO_SKIN) && !PED_DAMAGE_MEDALS && WET_PED_EFFECT && SPECULAR
			bool isSkin = surfaceInfo.surface_isSkin > 0.0;	
#else
			bool isSkin = false;	 // Some times there is no skin data, usually decals or metal, so assume not skin
#endif

			//
			// get tattoo and blood samples (or use null values if locally disabled)
			//

#if !defined(USE_PED_DAMAGE_BLOOD_ONLY)
			#if PED_DAMAGE_DECORATION_DECAL
				half4 fullColorSample = SamplePedDamage(pedTattooTargetSampler,IN_TexCoords.xy);
			#else
				half4 fullColorSample = SamplePedDamage(pedTattooTargetSampler, tattooUV); 
			#endif
#endif
			half4 bloodSample = float4(0,0,1,0);

#if !defined(USE_PED_DAMAGE_DECORATIONS_ONLY)
			if (bForwardLighting)
			{
				if (IsBloodAndTattooInOne())			// hack, there are no free samplers so we double up on this one (if forward and double wide is set)
					bloodSample = SamplePedDamage(pedTattooTargetSampler, bloodUV); 
			}
			else
			{
				bloodSample = SamplePedDamage(pedBloodTargetSampler, bloodUV);
			}
#endif		

#if PED_DAMAGE_DECORATION_DECAL
			half4 decalColor  = fullColorSample;
#elif !defined(USE_PED_DAMAGE_BLOOD_ONLY)
//			half4 decalColor  = (BloodEnabled>0 && !PED_DAMAGE_MEDALS) && ((isSkin && (fullColorSample.a>=128/255.0) || !isSkin && fullColorSample.a<=128/255.0)) ? fullColorSample : float4(0,0,0,.5); 
			bool needsFullColor = isSkin ? (fullColorSample.a>=128/255.0) : (fullColorSample.a<=128/255.0);
			half4 decalColor = !PED_DAMAGE_MEDALS && BloodEnabled>0 && needsFullColor ? fullColorSample : float4(0,0,0,.5);
			
			decalColor.a = 1-abs(decalColor.a*2-1);
#else
			half4 decalColor = float4(0,0,0,1);
#endif
			// 
			// adjust specular
			//
#if SPECULAR && !defined(USE_PED_DAMAGE_DECORATIONS_ONLY)
			half specAdjust = bloodSample.r*(1-bloodSample.b)*(1-bloodSample.b);
			surfaceStandardLightingProperties.specularExponent  = lerp(surfaceStandardLightingProperties.specularExponent,  bloodSpecAdjust.x, specAdjust); 
			surfaceStandardLightingProperties.specularIntensity = lerp(surfaceStandardLightingProperties.specularIntensity, bloodSpecAdjust.y, specAdjust);
#endif
			// get the original diffuse
			half3 diffuse = surfaceStandardLightingProperties.diffuseColor.rgb;
		
			// add decorations (tattoo, bruise, team logo, etc.)
			decalColor.rgb *= (isSkin)?diffuse.rgb:float3(1,1,1); // tattoos look better multiplied by skin color	
			diffuse = diffuse*decalColor.a + decalColor.rgb;									// add decal color (rgb is pre alpha'd)

			// add blood soak
			diffuse = lerp(diffuse, diffuse * BloodSoakColor, (!isSkin) ? bloodSample.g : 0);	// darken cloth only

			// add wound
			diffuse = diffuse*bloodSample.b + BloodSplatColor * bloodSample.r;					// add blood splat (r is premultiplied by alpha) (blue is the blood's "1-alpha")
			
			// save the diffuse back out into the surface properties
			surfaceStandardLightingProperties.diffuseColor.rgb = diffuse;

#if PED_DAMAGE_DECORATION_DECAL
			surfaceStandardLightingProperties.diffuseColor.a = decalColor.a;
#endif
			
			// flatten detail map where the wound is, so we don't see fabric lines in the oozing blood;
			detailMapScale = bloodSample.b;
			
			//
			// compute bump map and adjust the surface normal with it
			//
#if !PED_DAMAGE_MEDALS && NORMAL_MAP && !defined(NO_PED_DAMAGE_NORMALS) && !defined(USE_PED_DAMAGE_DECORATIONS_ONLY)
			if (!bForwardLighting) // save time in mirror pass (it's low res anyway)
			{
				IFANY_BRANCH		  // bumpmaps are enabled/disabled for the whole ped, so we can jump over everything below
				if (BloodNormalMapsEnabled!=0.0)
				{
					half2 height00		= bloodSample.ra;  // we already have this sample for the blood color, etc
		
					// need better way to calc this:
					bool isDoubleWide  = IsBloodAndTattooInOne() && PS3_SWITCH(1,bForwardLighting);  // currently on non ps3, we only support side by side for forward lighting.
					bool isRotated     = IsBloodRotated();  // the bumps are all in the blood target,so we don't need to worry about tattoo rotation
					bool isTorsoOrHead = (damageData.z > ((isDoubleWide) ? .5*1.5/10.0 : 1.5/10.0)) ;   // if double wide the dmaagedata z was scaled by .5

					float2 texelScale  = (isRotated) ? float2((isDoubleWide) ? 0.5*OneOverBloodHeight : OneOverBloodHeight, OneOverBloodWidth) : float2(OneOverBloodWidth,OneOverBloodHeight);

					float4 offsets = (isRotated == isTorsoOrHead) ? float4(1,0,0,1) : float4(0,-1,-1,0);

					half2 height01		= SamplePedDamage(pedBloodTargetSampler,bloodUV+offsets.xy*texelScale).ra; // below
					half2 height10		= SamplePedDamage(pedBloodTargetSampler,bloodUV+offsets.zw*texelScale).ra; // to the right

#if !defined(USE_PED_DAMAGE_BLOOD_ONLY)
					half3 scarHeights = half3(height00.g,height01.g,height10.g);
					half2 scarDelta = (isSkin) * half2(scarHeights.z - scarHeights.x, scarHeights.y - scarHeights.x); // scar height only if skin
#else
					half2 scarDelta = float2(0,0);
#endif

					half3 bloodHeights = half3(height00.r,height01.r,height10.r)*(1-bloodSample.b); // scale the blood height values by the alpha
					half2 bloodDelta = half2(bloodHeights.z - bloodHeights.x, bloodHeights.y - bloodHeights.x);

					half3 normal = float3((scarDelta + bloodDelta)*BloodBumpScale,0);
					normal.z = saturate(1 - dot(normal.xy,normal.xy)); // skipping the sqrt does not look too bad actually...
				
					// the tangent and binormals are strange on the arms. we swap them and or negate them for the arms
					// damageData.z<0 means it's a left arm, damageData.w<0 means it's a right arm
					normal.xy = (damageData.z<0) ? float2(normal.y,-normal.x): ((damageData.w<0) ? normal.yx : normal.xy);

					surfaceInfo.surface_worldNormal = normalize(half3(normal.x*IN_Tangent + normal.y*IN_Binormal + normal.z*surfaceInfo.surface_worldNormal));	
				}
			}
#endif	// !PED_DAMAGE_MEDALS && NORMAL_MAP && !defined(USE_PED_DAMAGE_DECORATIONS_ONLY) && !defined(USE_PED_DAMAGE_DECORATIONS_ONLY)
		}
	}
	return detailMapScale;
}

#endif // PED_DAMAGE_TARGETS



//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////




//
//
//
//
pedVertexOutput VS_PedTransform0( pedVertexNoFatOrWindInputBump IN,  
							#if NORMAL_MAP
								  float4 inTan,
							#endif
								  float3 inPos, 
								  float3 inNrm )
{
	pedVertexOutput OUT;

#if PEDSHADER_SNOW_MAX
	const float2 snowBlend = VS_CalculateSnowBlend( 1.0f/*IN.diffuse.b*/ );
#endif

	float3 pvlWorldNormal, pvlWorldPosition, t_surfaceToEye;

	pedProcessVertLightingUnskinned(inPos,
									inNrm,
						#if NORMAL_MAP
									inTan,
						#endif
									IN.diffuse,
									(float4x3)gWorld,
									pvlWorldPosition,
						#if REFLECT
									OUT.surfaceToEye,
						#else
									t_surfaceToEye,
						#endif
									pvlWorldNormal,
						#if NORMAL_MAP
									OUT.worldBinormal,
									OUT.worldTangent,
						#endif
									OUT.color0
								   );

#if PEDSHADER_SNOW_MAX
	inPos = VS_ApplySnowTransform(inPos, inNrm, snowBlend);
#endif

    // Write out final position & texture coords
    OUT.pos = mul(float4(inPos,1), gWorldViewProj);

	OUT.worldNormal.xyz = pvlWorldNormal;

    // NOTE: These next 2 statements may resolve to the exact same code -- rely on HLSL to strip worthless code
    DIFFUSE_VS_OUTPUT = DIFFUSE_VS_INPUT;
    
#if SPEC_MAP
    SPECULAR_VS_OUTPUT = SPECULAR_VS_INPUT;
#endif 

	OUT.worldPos=float4(pvlWorldPosition,1);

#if 0 && PEDSHADER_SNOW_MAX
	OUT.snowParams = VS_CalculateSnowPsParams(snowBlend);
#endif

#if PED_DAMAGE_TARGETS
	OUT.damageData = CalcPedDamageData(IN.PED_DAMAGE_INPUT, OUT.texCoord.w, OUT.worldNormal.w, false, true);
	#if !(WET_PED_EFFECT && 0) // currently we do not do wetness on non deferred passes 
		OUT.texCoord.z = 0;    // there is a gap, since ped damage extra data is in w
	#endif
#endif

	OUT.color0 = pedAdjustAmbientScales(OUT.color0);
#if WET_PED_EFFECT
	OUT.color0.a = 1.0;
#endif

	RAGE_COMPUTE_CLIP_DISTANCES(OUT.pos);
	
	return OUT;
}


pedVertexOutputWaterReflection VS_PedTransformWaterReflection(pedVertexNoFatOrWindInputBump IN)
{
	pedVertexOutputWaterReflection OUT;

	float3 inPos = IN.pos;
	float3 inNrm = IN.normal;
	float4 inCpv = IN.diffuse;

	float3 pvlWorldNormal, pvlWorldPosition, pvlWorldEyePos;

	pedProcessVertLightingBasic(
		inPos,
		inNrm,
		inCpv,
		(float4x3) gWorld,
		pvlWorldPosition,
		pvlWorldEyePos,
		pvlWorldNormal,
		OUT.color0
		);

	// Write out final position & texture coords
	OUT.pos = mul(float4(inPos,1), gWorldViewProj);
	OUT.worldNormal	= pvlWorldNormal;
	OUT.worldPos = pvlWorldPosition;
	//OUT.color0 = pedAdjustAmbientScales(OUT.color0);

	DIFFUSE_VS_OUTPUT = DIFFUSE_VS_INPUT;

	RAGE_COMPUTE_CLIP_DISTANCES(OUT.pos);

	return OUT;
}


//
//
//
//
pedVertexOutput VS_PedTransform(pedVertexNoFatOrWindInputBump IN
						#ifdef RAGE_ENABLE_OFFSET
							  , rageVertexOffsetBump INOFFSET //, uniform float bEnableOffsets
						#endif
) 
{    
    float3 inPos = IN.pos;
    float3 inNrm = IN.normal;

#if NORMAL_MAP
    float4 inTan = IN.tangent;
#endif

	return VS_PedTransform0(IN,  
						#if NORMAL_MAP
							inTan,
						#endif
							inPos,
							inNrm );
}


pedVertexOutput VS_PedTransformBS(pedVertexNoFatOrWindInputBump IN
							#ifdef RAGE_ENABLE_OFFSET
								, rageVertexOffsetBump INOFFSET //, uniform float bEnableOffsets
							#endif
) 
{
	float3 inPos = IN.pos;
    float3 inNrm = IN.normal;
#if NORMAL_MAP
    float4 inTan = IN.tangent;
#endif

#ifdef RAGE_ENABLE_OFFSET
    //if( bEnableOffsets != 0.0f )
	{
		inPos += INOFFSET.pos*1.0f;
		inNrm += INOFFSET.normal;
		#if NORMAL_MAP
			inTan += INOFFSET.tangent;
		#endif
	}
#endif //RAGE_ENABLE_OFFSET...

	return VS_PedTransform0(IN,  
						#if NORMAL_MAP
							inTan,
						#endif
							inPos,
							inNrm);
}


pedVertexOutputSeeThrough VS_PedTransform_SeeThrough(pedVertexInputSeeThrough IN) 
{    
	pedVertexOutputSeeThrough OUT;
	
	OUT.seeThrough = SeeThrough_GenerateOutput(IN.seeThrough.pos);
#if SPEC_MAP
	OUT.texCoord = IN.texCoord0;
#endif

	return OUT;
}


#if __MAX
//
//
//
//
pedVertexOutput VS_MaxTransform( maxVertexInput IN
						#if 0 // #ifdef RAGE_ENABLE_OFFSET
							   , rageVertexOffsetBump INOFFSET, uniform float bEnableOffsets
						#endif
							)
{
	pedVertexNoFatOrWindInputBump pedIN=(pedVertexNoFatOrWindInputBump)0;

	float3 inPos = IN.pos;
    float3 inNrm = IN.normal;
#if NORMAL_MAP
    float4 inTan = float4(IN.tangent,1.0f);	// TODO: Max tangent.w should contain sign of binormal
#endif

	pedIN.pos		= IN.pos;
	pedIN.diffuse	= IN.diffuse;

#if PED_DAMAGE_TARGETS
	pedIN.texCoord0 = Max2RageTexCoord2(IN.texCoord0.xy).xyxy;
#else
	pedIN.texCoord0 = Max2RageTexCoord2(IN.texCoord0.xy);
#endif

	pedIN.normal	= IN.normal;
	pedIN.tangent	= float4(IN.tangent,1.0f); 	// TODO: Max tangent.w should contain sign of binormal

	return VS_PedTransform0(pedIN,  
					#if NORMAL_MAP
							inTan,
					#endif
							inPos,
							inNrm );
							
}// end of VS_MaxTransform()...
#endif //__MAX...


//
//
//
//
pedVertexOutputD VS_PedTransformD0( pedVertexInputBump IN,
						#if PED_TRANSFORM_WORLDPOS_OUT
									out float3 OUT_worldPos,
						#endif
						#if NORMAL_MAP
									float4 inTan,
						#endif
									float3 inPos, 
									float3 inNrm )
{
	pedVertexOutputD OUT;



	float3 pvlWorldNormal, pvlWorldPosition;
	float3 t_surfaceToEye;

#if PEDSHADER_SNOW
	const float2 snowBlend = VS_CalculateSnowBlend( IN.diffuse.b );
#endif

	pedProcessVertLightingUnskinned(inPos,
									inNrm,
						#if NORMAL_MAP
									inTan,
						#endif
									IN.diffuse,
									(float4x3) gWorld, 
									pvlWorldPosition,
						#if REFLECT || PED_RIM_LIGHT
									OUT.surfaceToEye,
						#else
									t_surfaceToEye,
						#endif
									pvlWorldNormal,
						#if NORMAL_MAP
									OUT.worldBinormal,
									OUT.worldTangent,
						#endif
									OUT.color0);

#if PEDSHADER_FUR
	inPos	= VS_ApplyFurTransform(inPos, inNrm, IN.specular.a);
#endif

#if PEDSHADER_SNOW
	inPos	= VS_ApplySnowTransform(inPos, inNrm, snowBlend);
#endif

#if PEDSHADER_FAT
	inPos	= VS_ApplyFatTransform(inPos, inNrm, IN.specular.a);
#endif

#if PED_CPV_WIND
	inPos	= VS_PedApplyMicromovements(inPos, IN.specular.rgb);
#endif

	// Write out final position & texture coords
	OUT.pos =  mul(float4(inPos,1), gWorldViewProj);

	OUT.worldNormal.xyz = pvlWorldNormal;
#if PED_TRANSFORM_WORLDPOS_OUT
	OUT_worldPos		= mul(float4(inPos,1), (float4x3)gWorld);
#endif

	// NOTE: These next 2 statements may resolve to the exact same code -- rely on HLSL to strip worthless code
	DIFFUSE_VS_OUTPUT = DIFFUSE_VS_INPUT;
	
#if SPEC_MAP
	SPECULAR_VS_OUTPUT = SPECULAR_VS_INPUT;
#endif	// SPEC_MAP

#if PED_HAIR_DIFFUSE_MASK
    DIFFUSE2_VS_OUTPUT = DIFFUSE2_VS_INPUT;
#endif

	OUT.color0 = pedAdjustAmbientScales(OUT.color0);

#if PED_DAMAGE_TARGETS
	OUT.damageData = CalcPedDamageData(IN.PED_DAMAGE_INPUT, OUT.texCoord.w, OUT.worldNormal.w, false, false);
	#if !WET_PED_EFFECT
		OUT.texCoord.z = 0; // there is a gap, since ped damage extra data is in w
	#endif
#endif

#if WET_PED_EFFECT
	OUT.WETNESS_PARAM = 
		#if PED_CPV_WIND || PEDSHADER_FAT
			VS_CalcWetnessSweatParam(IN.pos.z, IN.specular.a);
		#else
			VS_CalcWetnessParam(IN.pos.z);
		#endif
#endif

#if PEDSHADER_FUR
	OUT.furParams = VS_CalculateFurPsParams(IN.specular.a);
#endif

#if PEDSHADER_SNOW
	OUT.snowParams = VS_CalculateSnowPsParams(snowBlend);
#endif

#if PED_RIM_LIGHT && PRECOMPUTE_RIM
	const half bentrange=1.f/25.5f;
	half rv=OUT.color0.g+(255.-120.)/255.; 
	OUT.rimValues = float3( (OUT.color0.g-bentrange)*(1.f-bentrange) *gPedRimLightingScale, 1.0f - OUT.color0.g /bentrange, rv*rv*rv);
#endif

	return OUT;
}

//
//
//
//
pedVertexOutputD VS_PedTransformD(pedVertexInputBump IN
							#ifdef RAGE_ENABLE_OFFSET
								, rageVertexOffsetBump INOFFSET //, uniform float bEnableOffsets
							#endif
) 
{    
    float3 inPos = IN.pos;
    float3 inNrm = IN.normal;

#if NORMAL_MAP
    float4 inTan = IN.tangent;
#endif
#if PED_TRANSFORM_WORLDPOS_OUT
	float3 worldPos_NOT_USED;
#endif

	return VS_PedTransformD0(IN,  
					#if PED_TRANSFORM_WORLDPOS_OUT
							 worldPos_NOT_USED,
					#endif
					#if NORMAL_MAP
							 inTan,
					#endif
							 inPos,
							 inNrm );
}

//
//
//
//
pedVertexOutputD VS_PedTransformDBS(pedVertexInputBump IN
							#ifdef RAGE_ENABLE_OFFSET
								  , rageVertexOffsetBump INOFFSET //, uniform float bEnableOffsets
							#endif
) 
{
	float3 inPos = IN.pos;
    float3 inNrm = IN.normal;

#if NORMAL_MAP
    float4 inTan = IN.tangent;
#endif
#if PED_TRANSFORM_WORLDPOS_OUT
	float3 worldPos_NOT_USED;
#endif

#ifdef RAGE_ENABLE_OFFSET
    //if( bEnableOffsets != 0.0f )
	{
		inPos += INOFFSET.pos*1.0f;
		inNrm += INOFFSET.normal;
	#if NORMAL_MAP
		inTan += INOFFSET.tangent;
	#endif
	}
#endif //RAGE_ENABLE_OFFSET...

	return VS_PedTransformD0(IN,  
					#if PED_TRANSFORM_WORLDPOS_OUT
							 worldPos_NOT_USED,
					#endif
					#if NORMAL_MAP
							 inTan,
					#endif
							 inPos,
							 inNrm);
}

//
//
//
//
pedVertexOutput VS_PedTransformSkin0( northSkinVertexInputBump IN,  
							#if NORMAL_MAP
									  float4 inTan,
							#endif
									  float3 inPos, 
									  float3 inNrm )
{
	pedVertexOutput OUT;
	
#if USE_UBYTE4
	int4 IndexVector	= D3DCOLORtoUBYTE4(IN.blendindices);
#else
	int4 IndexVector	= D3DCOLORtoUBYTE4(IN.blendindices);
#endif
	rageSkinMtx skinMtx	= ComputeSkinMtx(IN.blendindices, IN.weight);

#if PED_CLOTH && (__XENON || DX11_CLOTH)
	bool isCloth = rageSkinGetBone0(IndexVector) > 254;
	// Override skinMt with the bone 0, to ensure somewhat correct things are getting rendered.
	if( isCloth )
	{
		skinMtx = gBoneMtx[0];
	}
#endif	// PED_CLOTH && (__XENON || DX11_CLOTH)

	float3 pvlWorldNormal, pvlSkinnedNormal, pvlWorldPosition, t_surfaceToEye;

	pedProcessVertLightingSkinned(	inPos,
									inNrm,
						#if NORMAL_MAP
									inTan,
						#endif
									IN.diffuse,
									skinMtx, 
									(float4x3)gWorld,
									pvlWorldPosition,
						#if REFLECT
									OUT.surfaceToEye,
						#else
									t_surfaceToEye,
						#endif
									pvlSkinnedNormal,
									pvlWorldNormal,
						#if NORMAL_MAP
									OUT.worldBinormal,
									OUT.worldTangent,
						#endif
									OUT.color0);

    // Write out final position & texture coords
	// Transform position by bone matrix
	float3 pos = rageSkinTransform(inPos, skinMtx);
	
#if PED_CLOTH && (__XENON || DX11_CLOTH)
	if( isCloth )
	{
		int idx0 = rageSkinGetBone1(IndexVector);
		int idx1 = rageSkinGetBone2(IndexVector);
		int idx2 = rageSkinGetBone3(IndexVector);

		ClothTransform(idx0, idx1, idx2, IN.weight, pos, pvlSkinnedNormal);

		pvlWorldPosition = mul(float4(pos,1), gWorld);
		pvlWorldNormal = NORMALIZE_VS(mul(pvlSkinnedNormal, (float3x3)gWorld));

	#if REFLECT
			// Store position in eyespace
			OUT.surfaceToEye = gViewInverse[3].xyz - pvlWorldPosition;
	#endif

	#if NORMAL_MAP
			// Do the tangent+binormal stuff, keep it in bone0 space, because we can.
			float3 morphedTangent	= mul((float3x3)skinMtx,inTan.xyz);
			OUT.worldTangent		= NORMALIZE_VS(mul(morphedTangent, (float3x3)gWorld));
			OUT.worldBinormal		= rageComputeBinormal(pvlWorldNormal, OUT.worldTangent, inTan.w);
	#endif
	}

#endif	// PED_CLOTH && (__XENON || DX11_CLOTH)
	
    OUT.pos =  mul(float4(pos,1), gWorldViewProj);

	OUT.worldNormal.xyz = pvlWorldNormal;

    // NOTE: These next 2 statements may resolve to the exact same code -- rely on HLSL to strip worthless code
    DIFFUSE_VS_OUTPUT = DIFFUSE_VS_INPUT;
    
#if SPEC_MAP
    SPECULAR_VS_OUTPUT = SPECULAR_VS_INPUT;
#endif	// SPEC_MAP

	OUT.worldPos=float4(pvlWorldPosition,1);
	OUT.color0 = pedAdjustAmbientScales(OUT.color0);
#if WET_PED_EFFECT
	OUT.color0.a = 1.0;
#endif

#if PED_DAMAGE_TARGETS
	OUT.damageData = CalcPedDamageData(IN.PED_DAMAGE_INPUT, OUT.texCoord.w, OUT.worldNormal.w, false, true);
	#if !(WET_PED_EFFECT && 0) // currently we do not do wetness on non deferred passes 
		OUT.texCoord.z = 0; // there is a gap, since ped damage extra data is in w
	#endif
#endif

	RAGE_COMPUTE_CLIP_DISTANCES(OUT.pos);

	return OUT;
}

//
//
//
//
//
pedVertexOutput VS_PedTransformSkin(northSkinVertexInputBump IN)
{
    float3 inPos = IN.pos;
    float3 inNrm = IN.normal;

#if NORMAL_MAP
    float4 inTan = IN.tangent;
#endif

	return VS_PedTransformSkin0(IN,  
						#if NORMAL_MAP
								inTan,
						#endif
								inPos,
								inNrm );
}

//
//
//
//
pedVertexOutputWaterReflection VS_PedTransformSkinWaterReflection(northSkinVertexInputBump IN)
{
	pedVertexOutputWaterReflection OUT;
	pedVertexOutput temp;

    float3 inPos = IN.pos;
    float3 inNrm = IN.normal;

	temp = VS_PedTransformSkin0(IN,  
						#if NORMAL_MAP
								0, // don't need tangent, because we're not using normal mapping
						#endif
								inPos,
								inNrm );

	OUT.pos			= temp.pos;
	OUT.texCoord	= temp.texCoord.xy;
	OUT.worldNormal	= temp.worldNormal.xyz;
	OUT.worldPos	= temp.worldPos.xyz;
	OUT.color0		= temp.color0;

	RAGE_COMPUTE_CLIP_DISTANCES(OUT.pos);

	return OUT;
}

//
//
//
//
pedVertexOutput VS_PedTransformSkinBS(northSkinVertexInputBump IN
								#ifdef RAGE_ENABLE_OFFSET
									, rageSkinVertexOffsetBump INOFFSET //, uniform float bEnableOffsets
								#endif
)
{
pedVertexOutput OUT;
    
    float3 inPos = IN.pos;
    float3 inNrm = IN.normal;

#if NORMAL_MAP
    float4 inTan = IN.tangent;
#endif

#ifdef RAGE_ENABLE_OFFSET
    //if( bEnableOffsets != 0.0f )
	{
		inPos += INOFFSET.pos*1.0f;
		inNrm += INOFFSET.normal;
	#if NORMAL_MAP
		inTan += INOFFSET.tangent;
	#endif
	}
#endif //RAGE_ENABLE_OFFSET...

	return VS_PedTransformSkin0(IN,  
						#if NORMAL_MAP
								inTan,
						#endif
								inPos,
								inNrm);
}

//
//
//
//
pedVertexOutputD VS_PedTransformSkinD0( northSkinVertexInputBump IN,
								#if PED_TRANSFORM_WORLDPOS_OUT
										out float3 OUT_worldPos,
								#endif
								#if NORMAL_MAP
										float4 inTan,
								#endif
										float3 inPos, 
										float3 inNrm )
{
	pedVertexOutputD OUT;

#if USE_UBYTE4
	int4 IndexVector  = D3DCOLORtoUBYTE4(IN.blendindices);
#else
	int4 IndexVector = D3DCOLORtoUBYTE4(IN.blendindices);
#endif

	rageSkinMtx skinMtx = ComputeSkinMtx(IN.blendindices, IN.weight);

#if PED_CLOTH && (__XENON || DX11_CLOTH)
	// Override skinMt with bone0, to ensure somewhat correct things are getting rendered.
	bool isCloth = rageSkinGetBone0(IndexVector) > 254;
#else
	bool isCloth = false;
#endif	// PED_CLOTH && (__XENON || DX11_CLOTH)

	float3 pvlWorldNormal, pvlSkinnedNormal, pvlWorldPosition;
	float3 t_surfaceToEye;

#if PEDSHADER_SNOW
	const float2 snowBlend = VS_CalculateSnowBlend( IN.diffuse.b );
#endif

	pedProcessVertLightingSkinned(	inPos,
									inNrm,
					#if NORMAL_MAP
									inTan,
					#endif
									IN.diffuse,
									isCloth ? gBoneMtx[0] : skinMtx,
									(float4x3)gWorld,
									pvlWorldPosition,
					#if REFLECT || PED_RIM_LIGHT
									OUT.surfaceToEye,
					#else
									t_surfaceToEye,
					#endif
									pvlSkinnedNormal,
									pvlWorldNormal,
					#if NORMAL_MAP
									OUT.worldBinormal,
									OUT.worldTangent,
					#endif	// NORMAL_MAP
									OUT.color0);

	// Write out final position & texture coords
	// Transform position by bone matrix
	float3 pos = rageSkinTransform(inPos, isCloth ? gBoneMtx[0] : skinMtx);
	
#if PED_CLOTH && (__XENON || DX11_CLOTH)
	if( isCloth )
	{
		int idx0 = rageSkinGetBone1(IndexVector);
		int idx1 = rageSkinGetBone2(IndexVector);
		int idx2 = rageSkinGetBone3(IndexVector);

		ClothTransform(idx0, idx1, idx2, IN.weight, pos, pvlSkinnedNormal);

		pvlWorldPosition = mul(float4(pos,1), gWorld);
		pvlWorldNormal = NORMALIZE_VS(mul(pvlSkinnedNormal, (float3x3)gWorld));

	#if REFLECT || PED_RIM_LIGHT
		// Store position in eyespace
		OUT.surfaceToEye = gViewInverse[3].xyz - pvlWorldPosition;
	#endif

	#if NORMAL_MAP
		// Do the tangent+binormal stuff, keep it in bone0 space, because we can.
		float3 morphedTangent	= mul((float3x3)(isCloth ? gBoneMtx[0] : skinMtx),inTan.xyz);
		OUT.worldTangent		= NORMALIZE_VS(mul(morphedTangent, (float3x3)gWorld));
		OUT.worldBinormal		= rageComputeBinormal(pvlWorldNormal, OUT.worldTangent, inTan.w);
	#endif 

	}
#endif //PED_CLOTH && (__XENON || DX11_CLOTH)

#if PEDSHADER_FUR
	pos = VS_ApplyFurTransform(pos, pvlSkinnedNormal, IN.specular.a);
#endif

#if PEDSHADER_SNOW
	pos = VS_ApplySnowTransform(pos, pvlSkinnedNormal, snowBlend);
#endif

#if PEDSHADER_FAT
	pos = VS_ApplyFatTransform(pos, pvlSkinnedNormal, IN.specular.a);
#endif

#if PED_CPV_WIND
	pos	= VS_PedApplyMicromovements(pos.xyz, IN.specular.rgb);
#endif

#if PED_RIM_LIGHT && PRECOMPUTE_RIM
	const half bentrange=1.f/25.5f;
	half rv=OUT.color0.g+(255.-120.)/255.; 
	OUT.rimValues = float3( (OUT.color0.g-bentrange)*(1.f-bentrange) *gPedRimLightingScale, 1.0f - OUT.color0.g /bentrange, rv*rv*rv);
#endif

	OUT.pos =  mul(float4(pos,1), gWorldViewProj);

	OUT.worldNormal.xyz = pvlWorldNormal;
#if PED_TRANSFORM_WORLDPOS_OUT
	OUT_worldPos		= mul(float4(pos,1), (float4x3)gWorld);
#endif

	// NOTE: These next 2 statements may resolve to the exact same code -- rely on HLSL to strip worthless code
	DIFFUSE_VS_OUTPUT = DIFFUSE_VS_INPUT;

#if SPEC_MAP
	SPECULAR_VS_OUTPUT = SPECULAR_VS_INPUT;
#endif	// SPEC_MAP

#if PED_HAIR_DIFFUSE_MASK
	DIFFUSE2_VS_OUTPUT = DIFFUSE2_VS_INPUT;
#endif

	OUT.color0 = pedAdjustAmbientScales(OUT.color0);

#if PED_DAMAGE_TARGETS
	OUT.damageData = CalcPedDamageData(IN.PED_DAMAGE_INPUT, OUT.texCoord.w, OUT.worldNormal.w, false, false);
	#if !WET_PED_EFFECT
		OUT.texCoord.z = 0; // there is a gap, since ped damage extra data is in w
	#endif
#endif

#if WET_PED_EFFECT
	OUT.WETNESS_PARAM = 
		#if PED_CPV_WIND || PEDSHADER_FAT
			VS_CalcWetnessSweatParam(IN.pos.z, IN.specular.a);
		#else
			VS_CalcWetnessParam(IN.pos.z);
		#endif
	OUT.color0.a = 1.0;
#endif

#if PEDSHADER_FUR
	OUT.furParams = VS_CalculateFurPsParams(IN.specular.a);
#endif

#if PEDSHADER_SNOW
	OUT.snowParams = VS_CalculateSnowPsParams(snowBlend);
#endif

	return OUT;
}


//
//
//
//
//
pedVertexOutputD VS_PedTransformSkinD(northSkinVertexInputBump IN)
{

	float3 inPos = IN.pos;
	float3 inNrm = IN.normal;
#if NORMAL_MAP
	float4 inTan = IN.tangent;
#endif
#if PED_TRANSFORM_WORLDPOS_OUT
	float3 worldPos_NOT_USED;
#endif

	return VS_PedTransformSkinD0(IN,
						#if PED_TRANSFORM_WORLDPOS_OUT
							 	worldPos_NOT_USED,
						#endif
						#if NORMAL_MAP
								inTan,
						#endif
								inPos,
								inNrm );
}


VS_CascadeShadows_OUT VS_CascadeShadows_PedTransformSkinD_Common(northSkinVertexInputBump IN
#if RAGE_INSTANCED_TECH
	,uniform bool bUseInst
#endif
																 )
{
	float3 inPos = IN.pos;
	float3 inNrm = IN.normal;
#if NORMAL_MAP
	float4 inTan = IN.tangent;
#endif
#if PED_TRANSFORM_WORLDPOS_OUT
	float3 worldPos_NOT_USED;
#endif

	VS_CascadeShadows_OUT OUT = (VS_CascadeShadows_OUT)0;

#if USE_UBYTE4
	int4 IndexVector = D3DCOLORtoUBYTE4(IN.blendindices);
#else
	int4 IndexVector = D3DCOLORtoUBYTE4(IN.blendindices);
#endif

	rageSkinMtx skinMtx = ComputeSkinMtx(IN.blendindices, IN.weight);

#if PED_CLOTH && (__XENON || DX11_CLOTH)
	// Override skinMt with bone0, to ensure somewhat correct things are getting rendered.
	bool isCloth = rageSkinGetBone0(IndexVector) > 254;
#else
	bool isCloth = false;
#endif	// PED_CLOTH && (__XENON || DX11_CLOTH)

	float3 pvlWorldNormal, pvlSkinnedNormal;

#if PEDSHADER_SNOW
	const float2 snowBlend = VS_CalculateSnowBlend( IN.diffuse.b );
#endif

	// Transform normal by "transpose" matrix
	pvlSkinnedNormal			= rageSkinRotate(inNrm, isCloth ? gBoneMtx[0] : skinMtx);
#if RAGE_INSTANCED_TECH
	if (bUseInst)
		pvlWorldNormal				= NORMALIZE_VS(mul(pvlSkinnedNormal, (float3x3)gInstWorld));
	else
#endif
		pvlWorldNormal				= NORMALIZE_VS(mul(pvlSkinnedNormal, (float3x3)gWorld));

	// Write out final position & texture coords
	// Transform position by bone matrix
	float3 pos = rageSkinTransform(inPos, isCloth ? gBoneMtx[0] : skinMtx);
	
#if PED_CLOTH && (__XENON || DX11_CLOTH)
	if( isCloth )
	{
		int idx0 = rageSkinGetBone1(IndexVector);
		int idx1 = rageSkinGetBone2(IndexVector);
		int idx2 = rageSkinGetBone3(IndexVector);

		ClothTransform(idx0, idx1, idx2, IN.weight, pos, pvlSkinnedNormal);

#if RAGE_INSTANCED_TECH
	if (bUseInst)
		pvlWorldNormal = NORMALIZE_VS(mul(pvlSkinnedNormal, (float3x3)gInstWorld));
	else
#endif
		pvlWorldNormal = NORMALIZE_VS(mul(pvlSkinnedNormal, (float3x3)gWorld));
	}
#endif //PED_CLOTH && (__XENON || DX11_CLOTH)

#if PEDSHADER_FUR
	pos = VS_ApplyFurTransform(pos, pvlSkinnedNormal, IN.specular.a);
#endif

#if PEDSHADER_SNOW
	pos = VS_ApplySnowTransform(pos, pvlSkinnedNormal, snowBlend);
#endif

#if PEDSHADER_FAT
	pos = VS_ApplyFatTransform(pos, pvlSkinnedNormal, IN.specular.a);
#endif

#if PED_CPV_WIND
	pos	= VS_PedApplyMicromovements(pos.xyz, IN.specular.rgb);
#endif

#if RAGE_INSTANCED_TECH
	if (bUseInst)
		OUT.pos =  mul(float4(pos,1), mul(gInstWorld, gInstWorldViewProj[INSTOPT_INDEX(IN.InstID)]));
	else
#endif
		OUT.pos =  mul(float4(pos,1), gWorldViewProj);

	return OUT;
}

VS_CascadeShadows_OUT VS_CascadeShadows_PedTransformSkinD(northSkinVertexInputBump IN)
{
	return VS_CascadeShadows_PedTransformSkinD_Common(IN
#if RAGE_INSTANCED_TECH
	,false
#endif
		);
}

//
//
//
//
//
pedVertexOutputD VS_PedTransformSkinDBS(northSkinVertexInputBump IN
								#ifdef RAGE_ENABLE_OFFSET
									, rageSkinVertexOffsetBump INOFFSET //, uniform float bEnableOffsets
								#endif
)
{

	float3 inPos = IN.pos;
	float3 inNrm = IN.normal;
#if NORMAL_MAP
	float4 inTan = IN.tangent;
#endif
#if PED_TRANSFORM_WORLDPOS_OUT
	float3 worldPos_NOT_USED;
#endif

#ifdef RAGE_ENABLE_OFFSET
	//if( bEnableOffsets != 0.0f )
	{
		inPos += INOFFSET.pos*1.0f;
		inNrm += INOFFSET.normal;
		#if NORMAL_MAP
			inTan += INOFFSET.tangent;
		#endif
	}
#endif //RAGE_ENABLE_OFFSET...

	return VS_PedTransformSkinD0(IN,
						#if PED_TRANSFORM_WORLDPOS_OUT
							 	worldPos_NOT_USED,
						#endif
						#if NORMAL_MAP
								inTan,
						#endif
								inPos,
								inNrm);
}


//
//
//
//
pedVertexOutputSeeThrough VS_PedTransformSkin_SeeThrough(pedVertexInputSkinSeeThrough IN) 
{
	pedVertexOutputSeeThrough OUT;
	
#if USE_UBYTE4
	int4 IndexVector	= D3DCOLORtoUBYTE4(IN.seeThrough.blendindices);
#else
	int4 IndexVector	= D3DCOLORtoUBYTE4(IN.seeThrough.blendindices);
#endif

	rageSkinMtx skinMtx	= ComputeSkinMtx(IN.seeThrough.blendindices, IN.seeThrough.weight);
#if PED_CLOTH && (__XENON || DX11_CLOTH)
	bool isCloth = rageSkinGetBone0(IndexVector) > 254;
	// Override skinMt with the bone 0, to ensure somewhat correct things are getting rendered.
	if( isCloth )
	{
		skinMtx = gBoneMtx[0];
	}
#endif	// PED_CLOTH && (__XENON || DX11_CLOTH)

	float3 pos = rageSkinTransform(IN.seeThrough.pos, skinMtx);

#if PED_CLOTH && (__XENON || DX11_CLOTH)
	if( isCloth )
	{
		int idx0 = rageSkinGetBone1(IndexVector);
		int idx1 = rageSkinGetBone2(IndexVector);
		int idx2 = rageSkinGetBone3(IndexVector);
		float3 clothNormal;
		ClothTransform(idx0, idx1, idx2, IN.seeThrough.weight, pos, clothNormal);
	}
#endif	// PED_CLOTH && (__XENON || DX11_CLOTH)
	
	OUT.seeThrough = SeeThrough_GenerateOutput(pos);	
	
#if SPEC_MAP	
	OUT.texCoord = IN.texCoord0;
#endif
	
	return OUT;
}


#if PED_HAIR_LONG
	#define PED_ALPHA_REF  ped_longhair_alphaRef
#elif PEDSHADER_FUR
	#define PED_ALPHA_REF  0
#else
	#define PED_ALPHA_REF  ped_alphaRef
#endif

//
//
//
//
void PedDoCutOut(pedVertexOutput IN)
{
	#if PEDSHADER_FUR
		float alphaRef = 0.5f;
	#else
		float alphaRef = PED_ALPHA_REF;
	#endif

#if SSTAA && !RSG_ORBIS
	if(ENABLE_TRANSPARENCYAA)
	{
		uint Coverage=0;
		GetSurfaceAlphaWithCoverage(DIFFUSE_PS_INPUT, DiffuseSampler, globalAlpha, alphaRef, Coverage);
		rageDiscard(Coverage==0);
	}
	else
#endif
	{
	#if DIFFUSE_TEXTURE
		#if RSG_ORBIS	// avoid calculating coverage on Orbis
			float alphaBlend = GetSurfaceAlpha_(DIFFUSE_PS_INPUT, DiffuseSampler, globalAlpha);
		#else
			float alphaBlend = GetSurfaceAlpha(DIFFUSE_PS_INPUT, DiffuseSampler);
		#endif
	#else
		float alphaBlend = 1.0;
	#endif

	#if PEDSHADER_FUR
		rageDiscard(alphaBlend < alphaRef);
	#else
		rageDiscard(alphaBlend <= alphaRef);	
	#endif
	}
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//
//
//
//
float4 PedTextured( pedVertexOutput IN, int numLights, bool directionalLightAndShadow, bool bSupportCapsule, bool bUseHighQuality, bool bAllowPedDamage, bool bUseHairTinting)
{
	SurfaceProperties surfaceInfo =
		 GetSurfaceProperties(
							IN.color0, 
				#if DIFFUSE_TEXTURE
							DIFFUSE_PS_INPUT,
							DiffuseSampler, 
				#endif
				#if SPEC_MAP
							SPECULAR_PS_INPUT,
							SpecSampler,
				#endif
				#if REFLECT
							IN.surfaceToEye,
							EnvironmentSampler,
					#if REFLECT_MIRROR
							float2(0,0),
							float4(0,0,0,1), // not supported
					#endif // REFLECT_MIRROR
				#endif
				#if NORMAL_MAP
							IN.texCoord.xy,
							BumpSampler, 
							IN.worldTangent,
							IN.worldBinormal,
				#endif
							IN.worldNormal.xyz);

#if PED_WRINKLE && FORWARD_PED_WRINKLE_MAPS
	surfaceInfo.surface_worldNormal.xyz = PS_PedGetWrinkledNormal(IN.texCoord.xy, IN.worldNormal.xyz, IN.worldTangent.xyz, IN.worldBinormal.xyz);
#endif // PED_WRINKLE && FORWARD_PED_WRINKLE_MAPS
#if !FORWARD_PED_NORMAL_MAPS
	surfaceInfo.surface_worldNormal = IN.worldNormal.xyz; // let shader compiler strip out normal maps
#endif // !FORWARD_PED_NORMAL_MAPS
#if PED_COLOR_VARIATION_TEXTURE
	surfaceInfo.surface_diffuseColor.rgba = PS_PalettizeColorVariationTexture(surfaceInfo.surface_diffuseColor.rgba);
#endif //PED_COLOR_VARIATION_TEXTURE...

#if HAIR_TINT_TECHNIQUES
	if (bUseHairTinting)
	{
		surfaceInfo.surface_diffuseColor.rgb = PS_ApplyHairRamp(surfaceInfo.surface_diffuseColor.rgba);
	}
#endif // HAIR_TINT_TECHNIQUES

	// Compute some surface information.
	StandardLightingProperties surfaceStandardLightingProperties =
						DeriveLightingPropertiesForCommonSurface( surfaceInfo );

#if PED_DAMAGE_TARGETS
	if (bAllowPedDamage)
	{
		ComputePedDamageAdjustments(IN.damageData, IN.texCoord.w, IN.worldNormal.w, surfaceStandardLightingProperties, surfaceInfo, 
		#if PED_DAMAGE_MEDALS || !NORMAL_MAP
										half3(0,0,0), half3(0,0,0), (half2)IN.texCoord.xy, true); 
		#else
										(half3)IN.worldTangent,(half3)IN.worldBinormal, (half2)IN.texCoord.xy, true); 
		#endif
	}
#endif

	surfaceStandardLightingProperties.diffuseColor = ProcessDiffuseColor(surfaceStandardLightingProperties.diffuseColor);

#if EMISSIVE
	surfaceStandardLightingProperties.emissiveIntensity	*= matMaterialColorScale0.y;
#endif	// EMISSIVE	

	//-----------------------------------------------------------------------
	// Determine the light striking the surface and heading toward
	// the eye from this location.

	half4 OUT;

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

	// burnout peds lose their reflectivity props:
	material.diffuseColor.rgb *= matMaterialColorScale0.x;

#if PED_HAIR_SPIKED
	surface.normal = -gDirectionalLight.xyz;	// fake normal for spiked hair in forward
#endif

#if __XENON
	[isolate]
#endif
	OUT = calculateForwardLighting(
		numLights,
		bSupportCapsule,
		surface,
		material,
		light,
		directionalLightAndShadow,
			SHADOW_RECEIVING,
			bUseHighQuality, // directionalShadowHighQuality
		IN.pos.xy * gooScreenSize.xy);

#if defined(USE_FOGRAY_FORWARDPASS) && !__LOW_QUALITY
	if (gUseFogRay)
		OUT = calcDepthFxBasic(OUT, IN.worldPos.xyz, IN.pos.xy * gooScreenSize.xy);
	else
		OUT = calcDepthFxBasic(OUT, IN.worldPos.xyz);
#else
	OUT = calcDepthFxBasic(OUT, IN.worldPos.xyz);
#endif

#if !(defined(ALPHA_SHADER) || PED_HAIR_ORDERED || PED_HAIR_CUTOUT || PED_HAIR_LONG || PED_HAIR_SPIKED)
	// write out skin mask for non-alpha (eg: ped_alpha) and non-hair shaders
	#if SPECULAR 
		OUT.a = surfaceStandardLightingProperties.specularSkin;
	#endif
#endif

	return OUT;
}// end of PedTextured()...

//
//
//
//
float4 PedTexturedWaterReflection(pedVertexOutputWaterReflection IN, bool bUseHairTinting)
{
#if WATER_REFLECTION_CLIP_IN_PIXEL_SHADER
	rageDiscard(dot(float4(IN.worldPos.xyz, 1), gLight0PosX) < 0);
#endif // WATER_REFLECTION_CLIP_IN_PIXEL_SHADER
	SurfaceProperties surfaceInfo = GetSurfacePropertiesBasic(
		IN.color0, 
#if DIFFUSE_TEXTURE
		DIFFUSE_PS_INPUT,
		DiffuseSampler, 
#endif
		IN.worldNormal.xyz
		);

#if PED_COLOR_VARIATION_TEXTURE
	surfaceInfo.surface_diffuseColor.rgba = PS_PalettizeColorVariationTexture(surfaceInfo.surface_diffuseColor.rgba);
#endif //PED_COLOR_VARIATION_TEXTURE...
	surfaceInfo.surface_baseColor.w = 1; // force alpha to 1 - peds use this for wetness effect which we don't want here

#if HAIR_TINT_TECHNIQUES
	if (bUseHairTinting)
	{
		surfaceInfo.surface_diffuseColor.rgb = PS_ApplyHairRamp(surfaceInfo.surface_diffuseColor.rgba);
	}
#endif // HAIR_TINT_TECHNIQUES

	// Compute some surface information.
	StandardLightingProperties surfaceStandardLightingProperties =
		DeriveLightingPropertiesForCommonSurfaceInternal( surfaceInfo, PALETTE_TINT_WATER_REFLECTION );

	surfaceStandardLightingProperties.diffuseColor = ProcessDiffuseColor(surfaceStandardLightingProperties.diffuseColor);

	//-----------------------------------------------------------------------
	// Determine the light striking the surface and heading toward
	// the eye from this location.

	float4 OUT;

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

	OUT = calculateForwardLighting_WaterReflection(
		surface,
		material,
		light,
		1, // ambient scale
		1, // directional scale
		SHADOW_RECEIVING);

	// don't bother fogging vehicles in water reflection, they fade out after 25 metres
	//OUT = calcDepthFxBasic(OUT, IN.worldPos.xyz);

	// burnout peds lose their reflectivity props:
	OUT.rgb *= matMaterialColorScale0.x;

	return OUT;
}// end of PedTexturedWaterReflection()...

half4 PS_PedTextured_Zero(pedVertexOutput IN): COLOR
{
	return PackHdr(PedTextured(IN, 0, USE_DIRECTIONAL_LIGHT_FWD_0_LIGHTS, false, false, true, false));
}

half4 PS_PedTexturedCutOut_Zero(pedVertexOutput IN): COLOR
{
	PedDoCutOut(IN);
	return PackHdr(PedTextured(IN, 0, USE_DIRECTIONAL_LIGHT_FWD_0_LIGHTS, false, false, true, false));
}

half4 PS_PedTexturedCutOutTint_Zero(pedVertexOutput IN): COLOR
{
	PedDoCutOut(IN);
	return PackHdr(PedTextured(IN, 0, USE_DIRECTIONAL_LIGHT_FWD_0_LIGHTS, false, false, true, true));
}

half4 PS_PedTextured_Four(pedVertexOutput IN): COLOR
{
	return PackHdr(PedTextured(IN, 4, USE_DIRECTIONAL_LIGHT_FWD_4_OR_8_LIGHTS, true, false, true, false));
}

half4 PS_PedTexturedCutOut_Four(pedVertexOutput IN): COLOR
{
	PedDoCutOut(IN);
	return PackHdr(PedTextured(IN, 4, USE_DIRECTIONAL_LIGHT_FWD_4_OR_8_LIGHTS, true, false, true, false));
}

half4 PS_PedTexturedCutOutTint_Four(pedVertexOutput IN): COLOR
{
	PedDoCutOut(IN);
	return PackHdr(PedTextured(IN, 4, USE_DIRECTIONAL_LIGHT_FWD_4_OR_8_LIGHTS, true, false, true, true));
}

half4 PS_PedTextured_Eight(pedVertexOutput IN): COLOR
{
	return PackHdr(PedTextured(IN, 8, USE_DIRECTIONAL_LIGHT_FWD_4_OR_8_LIGHTS, true, false, true, false)); // don't use ped damage on 8 light version, it's not needed for the UI 
}

half4 PS_PedTexturedCutOut_Eight(pedVertexOutput IN): COLOR
{
	PedDoCutOut(IN);
	return PackHdr(PedTextured(IN, 8, USE_DIRECTIONAL_LIGHT_FWD_4_OR_8_LIGHTS, true, false, true, false)); // don't use ped damage on 8 light version, it's not needed for the UI 
}

half4 PS_PedTexturedCutOutTint_Eight(pedVertexOutput IN): COLOR
{
	PedDoCutOut(IN);
	return PackHdr(PedTextured(IN, 8, USE_DIRECTIONAL_LIGHT_FWD_4_OR_8_LIGHTS, true, false, true, true)); // don't use ped damage on 8 light version, it's not needed for the UI 
}


#if HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED
#if HIGHQUALITY_FORWARD_TECHNIQUES
	half4 PS_PedTextured_ZeroHighQuality(pedVertexOutput IN): COLOR
	{
		return PackHdr(PedTextured(IN, 0, USE_DIRECTIONAL_LIGHT_FWD_0_LIGHTS, false, HIGHQUALITY_FORWARD_TECHNIQUES, true, false)); // don't allow ped damage, since not used in UI
	}

	half4 PS_PedTexturedCutOut_ZeroHighQuality(pedVertexOutput IN): COLOR
	{
		PedDoCutOut(IN);
		return PackHdr(PedTextured(IN, 0, USE_DIRECTIONAL_LIGHT_FWD_0_LIGHTS, false, HIGHQUALITY_FORWARD_TECHNIQUES, true, false)); // don't allow ped damage, since not used in UI
	}

	half4 PS_PedTextured_FourHighQuality(pedVertexOutput IN): COLOR
	{
		return PackHdr(PedTextured(IN, 4, USE_DIRECTIONAL_LIGHT_FWD_4_OR_8_LIGHTS, true, HIGHQUALITY_FORWARD_TECHNIQUES, true, false));
	}

	half4 PS_PedTexturedCutOut_FourHighQuality(pedVertexOutput IN): COLOR
	{
		PedDoCutOut(IN);
		return PackHdr(PedTextured(IN, 4, USE_DIRECTIONAL_LIGHT_FWD_4_OR_8_LIGHTS, true, HIGHQUALITY_FORWARD_TECHNIQUES, true, false));
	}

	half4 PS_PedTextured_EightHighQuality(pedVertexOutput IN): COLOR
	{
		return PackHdr(PedTextured(IN, 8, USE_DIRECTIONAL_LIGHT_FWD_4_OR_8_LIGHTS, true, HIGHQUALITY_FORWARD_TECHNIQUES, true, false));
	}

	half4 PS_PedTexturedCutOut_EightHighQuality(pedVertexOutput IN): COLOR
	{
		PedDoCutOut(IN);
		return PackHdr(PedTextured(IN, 8, USE_DIRECTIONAL_LIGHT_FWD_4_OR_8_LIGHTS, true, HIGHQUALITY_FORWARD_TECHNIQUES, true, false));
	}
#else
	#define PS_PedTextured_ZeroHighQuality			PS_PedTextured_Zero
	#define PS_PedTexturedCutOut_ZeroHighQuality	PS_PedTexturedCutOut_Zero
	#define PS_PedTextured_FourHighQuality			PS_PedTextured_Four
	#define PS_PedTexturedCutOut_FourHighQuality	PS_PedTexturedCutOut_Four
	#define PS_PedTextured_EightHighQuality			PS_PedTextured_Eight
	#define PS_PedTexturedCutOut_EightHighQuality	PS_PedTexturedCutOut_Eight
#endif
#endif // HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED

half4 PS_PedTexturedWaterReflection(pedVertexOutputWaterReflection IN): COLOR
{
	return PackReflection(PedTexturedWaterReflection(IN, false));
}

half4 PS_PedTexturedWaterReflection_Tint(pedVertexOutputWaterReflection IN): COLOR
{
	return PackReflection(PedTexturedWaterReflection(IN, true));
}

half4 PS_PedTextured_SeeThrough(pedVertexOutputSeeThrough IN): COLOR
{
	half4 value = SeeThrough_Render(IN.seeThrough);
	
#if SPEC_MAP
	// Skin/Not skin value is in specSamp.b
	float4 specSamp = h4tex2D(SpecSampler, IN.texCoord.xy);
	half isSkin = PedIsSkin2(specSamp.b);
	value.z = value.z * (1.0f + isSkin*0.5f);
	value.z *= matMaterialColorScale0.w;
#else
	value.z *= matMaterialColorScale0.w;
#endif	// SPEC_MAP

	
	return value;
}

#if UI_TECHNIQUES
//
//
//
//
half4 PS_PedTextured_Four_UI(pedVertexOutput IN): COLOR
{
	half4 sampleC = saturate(PackHdr(PedTextured(IN, 4, true, true, false, true, false)));
#if __SHADERMODEL >= 40 && CUTOUT_OR_SSA_TECHNIQUES
	rageDiscard(sampleC.a <= ped_alphaRef);
#endif

	return half4(sqrt(sampleC.rgb), sampleC.a);
}
#endif //UI_TECHNIQUES...


//
//
//
//
void PS_AlwaysApplyPedDetail(  float2 IN_texCoord, float2 IN_texCoordSpec,
					#if NORMAL_MAP
						half3 IN_worldTangent, half3 IN_worldBinormal,
					#endif
					   inout SurfaceProperties surfaceInfo, 
					   inout StandardLightingProperties surfaceStandardLightingProperties,
					   half specBlueIndex,
					   half detailAlpha,
					   half specFresnel)
{
#if USE_DETAIL_ATLAS
	#if SPECULAR && WET_PED_EFFECT
		const bool bIsSkin = !(surfaceInfo.surface_isSkin==0.0f);
	#else
		const bool bIsSkin = true;
	#endif

		const float _fDetailScale = lerp(2.0f, detailScale, matMaterialColorScale1.x);

		half4 detv;
		if(matMaterialColorScale1.x==1)
		{
			detv = GetDetailValueScaled( IN_texCoord, specBlueIndex, detailScale);
		}
		else
		{
			half4 detvOrig = GetDetailValueScaled( IN_texCoord, specBlueIndex, detailScale);
			const float detailScaleBurnt = bIsSkin? 2.0f : 3.0f;
			half4 detvBurnt = GetDetailValueScaled( IN_texCoord, specBlueIndex, detailScaleBurnt);
			detv = lerp(detvBurnt, detvOrig, matMaterialColorScale1.x); 
		}

		const float detailIntensityBurnt = bIsSkin? 2.0f : 1.0f;
		const float PedDetailScaleBurnt  = bIsSkin? 2.0f : 1.0f;
		const float _fDetailIntensity = lerp(detailIntensityBurnt, detailIntensity, matMaterialColorScale1.x);
		const float _fPedDetailScale = lerp(PedDetailScaleBurnt, PedDetailScale, matMaterialColorScale1.x);
		detv.w=1.+(detv.w*2.-1.)*_fDetailIntensity*detailAlpha*_fPedDetailScale;
		half ds=detv.w;


	#if PED_STUBBLE && SPEC_MAP 
		half4 stubbledetv=GetDetailMapValue(IN_texCoord*_fDetailScale*StubbleControl.y, StubbleControl.x);
		float stubbleStrength =h1tex2D( StubbleSampler,IN_texCoordSpec).x;
		stubbledetv.w = max(stubbledetv.w-StubbleGrowth.x,0.) ;
		stubbledetv.xyz *= stubbledetv.w*1.;

		half stubFactor = saturate(stubbleStrength * stubbledetv.w*4.);
		stubFactor = saturate(1.-stubFactor);

		stubbledetv.w = -stubbledetv.w;

		half4 stubbleMod = stubbledetv * stubbleStrength*4.;
		detv += stubbleMod;
		detv.w = max(detv.w,0.3);

		surfaceStandardLightingProperties.specularExponent *= saturate(detv.w);
		surfaceStandardLightingProperties.specularIntensity *=(detv.w*.8+.2f);
		surfaceStandardLightingProperties.specularSkin *=saturate(detv.w*2.);// reduce Sub surface scattering effect where they is stubble
	#endif // defined(USE_STUBBLE) && SPEC_MAP

		half di = detv.w;
		surfaceStandardLightingProperties.diffuseColor.rgb*=di;

	#if SPECULAR 
		// should change roughness ???
		//	surfaceInfo.surface_specularIntensity=saturate(surfaceStandardLightingProperties.specularIntensity*ds);
	#endif

	#if NORMAL_MAP
		half3 bumpmap=detv.xyz;
		// Only add in tangental components straight up normal doesn't affect it.
		float3 wspaceBump = IN_worldTangent.xyz * bumpmap.x + IN_worldBinormal.xyz* bumpmap.y;
		const float detailBumpIntensityBurnt = bIsSkin? 2.0f : 1.5f;
		const float fDetailBumpIntensity = lerp(detailBumpIntensityBurnt, detailBumpIntensity, matMaterialColorScale1.x);
		surfaceInfo.surface_worldNormal.xyz += wspaceBump*fDetailBumpIntensity*detailAlpha*_fPedDetailScale;
		surfaceInfo.surface_worldNormal.xyz = normalize(surfaceInfo.surface_worldNormal.xyz );
		#if SPECULAR 
			surfaceInfo.surface_fresnel = specFresnel;
		#endif
	#endif //NORMAL_MAP...

#else // USE_DETAIL_ATLAS

	#if SPECULAR && PED_DAMAGE_TARGETS  // even if not detail atlas, need to adjust fresnel for ped damage
		surfaceInfo.surface_fresnel = specFresnel;
	#endif

#endif // USE_DETAIL_ATLAS
}// end of PS_AlwaysApplyPedDetail()...

void PS_ConditionalApplyPedDetail(	float2 IN_texCoord, float2 IN_texCoordSpec,
								#if NORMAL_MAP
									half3 IN_worldTangent, half3 IN_worldBinormal,
								#endif
									inout SurfaceProperties surfaceInfo, 
									inout StandardLightingProperties surfaceStandardLightingProperties,
									half specBlueIndex,
									half detailAlpha,
									half specFresnel)
{
	// bumpmaps are enabled/disabled for the whole ped, so we can jump over everything below
	IFANY_BRANCH if ( PedDetailScale != 0. ) // Do not adjust this branch! This is carefully setup to generate shader asm that can be runtime stripped of branch instructions on the PS3!
	{
		PS_AlwaysApplyPedDetail(IN_texCoord,IN_texCoordSpec,
							#if NORMAL_MAP
								IN_worldTangent, IN_worldBinormal,
							#endif
								surfaceInfo, 
								surfaceStandardLightingProperties,
								specBlueIndex,
								detailAlpha,
								specFresnel);
	}
}


void PS_ApplyPedDetail(		float2 IN_texCoord, float2 IN_texCoordSpec,
						#if NORMAL_MAP
							half3 IN_worldTangent, half3 IN_worldBinormal,
						#endif
							inout SurfaceProperties surfaceInfo, 
							inout StandardLightingProperties surfaceStandardLightingProperties,
							half specBlueIndex,
							half detailAlpha,
							half specFresnel, uniform bool alwaysApplyDetail)
{
	// This function is written in this strange way because we want to be very careful about the branch inside of PS_ConditionalApplyPedDetail. It has to
	// be formed in such a way as to generate the right kind of branch instructions so that the PS3 can use its SPU runtime branch stripping code.

	if ( alwaysApplyDetail )
	{
		PS_AlwaysApplyPedDetail(IN_texCoord,IN_texCoordSpec,
			#if NORMAL_MAP
				IN_worldTangent, IN_worldBinormal,
			#endif
				surfaceInfo, 
				surfaceStandardLightingProperties,
				specBlueIndex,
				detailAlpha,
				specFresnel);
	}
	else
	{
		PS_ConditionalApplyPedDetail(IN_texCoord,IN_texCoordSpec,
			#if NORMAL_MAP
				IN_worldTangent, IN_worldBinormal,
			#endif
				surfaceInfo, 
				surfaceStandardLightingProperties,
				specBlueIndex,
				detailAlpha,
				specFresnel);
	}
}

#if __MAX
//
//
//
//
float4 PS_MaxPedTexturedUnlit(pedVertexOutput IN) : COLOR
{
	float4 diffColor = tex2D(DiffuseSampler, DIFFUSE_PS_INPUT);
//	diffColor = diffColor*0.00001f+float4(tex2D(BumpSampler, DIFFUSE_PS_INPUT).xyz,1);

#if PED_COLOR_VARIATION_TEXTURE
	diffColor.rgba = PS_PalettizeColorVariationTexture(diffColor.rgba);
#endif

#if 0 && PEDSHADER_SNOW_MAX
	diffColor.rgb = PS_ApplySnow(diffColor.rgb, IN.snowParams, DIFFUSE_PS_INPUT);
#endif
	
#if PED_WRINKLE
	float3 tnorm = PS_PedGetWrinkledNormal(DIFFUSE_PS_INPUT, float3(0,0,1),float3(1,0,0),float3(0,1,0));
	diffColor.rgb *= (abs(tnorm.z)*0.75f+0.25f);
#endif

#if 0 && USE_DETAIL_ATLAS	
	float4 detv = GetDetailValue((float2)IN.texCoord, 1.0f);	
	float di = (detv.w*2.-1.)*detailIntensity;
	
	#if PED_STUBBLE && SPEC_MAP
		float4 stubbledetv = GetDetailMapValue( (float2)IN.texCoord.xy*detailScale*StubbleControl.y, StubbleControl.x);
		float stubbleStrength = tex2D( StubbleSampler,(float2)SPECULAR_PS_INPUT).x;
		stubbledetv.w = max(stubbledetv.w-StubbleGrowth.x,0.);
		stubbledetv.xyz *= stubbledetv.w;
		di -= stubbledetv.w;
	#endif	

	diffColor.rgb += di;
#endif

	float4 outColor	 =	diffColor*IN.color0;
	outColor.a		*=	globalAlpha*gInvColorExpBias;

#if 0 && PED_WRINKLE
	outColor = float4(wrinkleMaskStrengths0.rgb,1);
#endif
	return outColor;
}

//
//
//
//
float4 MaxPedTextured(pedVertexOutput IN)
{
float4 OUT;

	SurfaceProperties surfaceInfo =  
				GetSurfaceProperties(	IN.color0, 
							#if DIFFUSE_TEXTURE
										DIFFUSE_PS_INPUT,
										DiffuseSampler, 
							#endif
							#if SPEC_MAP
										SPECULAR_PS_INPUT,
										SpecSampler,
							#endif
							#if REFLECT
										IN.surfaceToEye,
										EnvironmentSampler,
								#if REFLECT_MIRROR
										float2(0,0),
										float4(0,0,0,1), // not supported
								#endif // REFLECT_MIRROR
							#endif
							#if NORMAL_MAP
										IN.texCoord.xy,
										BumpSampler, 
										IN.worldTangent,
										IN.worldBinormal,
								#if PARALLAX
										(IN.tanEyePos,
								#endif //PARALLAX...
							#endif	
										IN.worldNormal.xyz);

#if 0 && PEDSHADER_SNOW_MAX
	surfaceInfo.surface_diffuseColor.rgb = PS_ApplySnow(surfaceInfo.surface_diffuseColor.rgb, IN.snowParams, DIFFUSE_PS_INPUT);
#endif

#if PED_WRINKLE
	surfaceInfo.surface_worldNormal.xyz = PS_PedGetWrinkledNormal(IN.texCoord.xy, IN.worldNormal.xyz, IN.worldTangent.xyz, IN.worldBinormal.xyz);
#endif

	// Compute some surface information.
	StandardLightingProperties surfaceStandardLightingProperties =
			DeriveLightingPropertiesForCommonSurface( surfaceInfo );

#if 0
	PS_ApplyPedDetail(IN.texCoord.xy, SPECULAR_PS_INPUT,
					#if NORMAL_MAP
						IN.worldTangent.xyz, IN.worldBinormal.xyz,
					#endif
					surfaceInfo,  1.0f, 1.0f, 
					specularFresnel);
#endif

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

	OUT = calculateForwardLighting(
		0, // numLights
		false,
		surface,
		material,
		light,
		true, // directional
			false, // directionalShadow
			false, // directionalShadowHighQuality
		float2(0,0));

	return float4(sqrt(OUT.rgb), OUT.a); //gamma correction
}// end of MaxPedTextured()...

//
//
//
//
float4 PS_MaxPedTextured(pedVertexOutput IN) : COLOR
{
float4 OUT;

	if(maxLightingToggle)
	{
		OUT = MaxPedTextured(IN);
	}
	else
	{
		OUT = PS_MaxPedTexturedUnlit(IN);
	}

	return saturate(OUT);
}// end of PS_MaxPixelTextured()...
#endif //__MAX...


#define USE_ANISOTROPIC_TEXTURE  0

//
//
//
//

DeferredGBuffer PS_DeferredPedTexturedImp(pedVertexOutputD IN, uniform bool useCutsceneOpt, uniform bool useHairTinting, float fFacing)
{
#if DOUBLE_SIDED
	#if __WIN32PC
		fFacing *= -1;  // On PC the faces are the wrong way round so need to flip fFacing
	#endif
	IN.worldNormal.xyz *= fFacing;
#endif
	SurfaceProperties surfaceInfo =
		  GetSurfaceProperties(	IN.color0, 
						#if DIFFUSE_TEXTURE
								DIFFUSE_PS_INPUT,
								DiffuseSampler, 
						#endif
						#if SPEC_MAP
								SPECULAR_PS_INPUT,
								SpecSampler,
						#endif
						#if REFLECT
								IN.surfaceToEye,
								EnvironmentSampler,
							#if REFLECT_MIRROR
								float2(0,0),
								float4(0,0,0,1), // not supported
							#endif // REFLECT_MIRROR
						#endif
						#if NORMAL_MAP
								IN.texCoord.xy,
								BumpSampler, 
								IN.worldTangent,
								IN.worldBinormal,
						#endif	
								IN.worldNormal.xyz);

#if PED_WRINKLE 
	surfaceInfo.surface_worldNormal.xyz = PS_PedGetWrinkledNormal(IN.texCoord.xy, IN.worldNormal.xyz, IN.worldTangent.xyz, IN.worldBinormal.xyz);
#endif

#if PED_COLOR_VARIATION_TEXTURE
	surfaceInfo.surface_diffuseColor.rgba = PS_PalettizeColorVariationTexture(surfaceInfo.surface_diffuseColor.rgba);
#endif //PED_COLOR_VARIATION_TEXTURE...

#if PEDSHADER_FUR
	surfaceInfo.surface_diffuseColor.rgba = PS_ApplyFur(surfaceInfo.surface_diffuseColor.rgba, DIFFUSE_PS_INPUT, IN.furParams);
	rageDiscard(surfaceInfo.surface_diffuseColor.a < 0.5f);
#endif //PEDSHADER_FUR

#if PED_HAIR_DIFFUSE_MASK
	surfaceInfo.surface_diffuseColor.a = PS_ApplyDiffuseAlphaMask(surfaceInfo.surface_diffuseColor.a, DIFFUSE2_PS_INPUT);
#endif

#if PEDSHADER_SNOW
	surfaceInfo.surface_diffuseColor.rgb = PS_ApplySnow(surfaceInfo.surface_diffuseColor.rgb, IN.snowParams, (float2)DIFFUSE_PS_INPUT);
#endif //PEDSHADER_SNOW

#if PED_DAMAGE_MEDALS
	if (!useCutsceneOpt)
		surfaceInfo.surface_diffuseColor.rgba = LookupPedDamageMedals((half2)DIFFUSE_PS_INPUT);
#endif

#if HAIR_TINT_TECHNIQUES
	if (useHairTinting)
	{
		surfaceInfo.surface_diffuseColor.rgb = PS_ApplyHairRamp(surfaceInfo.surface_diffuseColor.rgba);
	}
#endif // HAIR_TINT_TECHNIQUES

	// Compute some surface information.
	StandardLightingProperties surfaceStandardLightingProperties = DeriveLightingPropertiesForCommonSurface(surfaceInfo);

#if PEDSHADER_FUR
    surfaceStandardLightingProperties.naturalAmbientScale *= furAOBlend;
#endif

	half detailMapAdjust = 1.0;
	#if PED_DAMAGE_TARGETS
		if (!useCutsceneOpt)
		{
			detailMapAdjust = ComputePedDamageAdjustments(IN.damageData, IN.texCoord.w, IN.worldNormal.w, surfaceStandardLightingProperties, surfaceInfo, 
			#if PED_DAMAGE_MEDALS || !NORMAL_MAP 
																half3(0,0,0), half3(0,0,0), (half2)IN.texCoord.xy, false); 
			#else
																(half3)IN.worldTangent,(half3)IN.worldBinormal, (half2)IN.texCoord.xy, false); 
			#endif
		}
	#endif
		half specBlue=0.0f;
		half detailAlpha=1.0f;

	#if SPECULAR && SPEC_MAP && USE_SPEC_BLUE_INDEX_MAP
		specBlue= surfaceInfo.surface_specMapSample.b;
		detailAlpha= surfaceInfo.surface_specMapSample.a;
	#endif


#if  !(PED_HAIR_CUTOUT || PED_HAIR_ORDERED || PED_HAIR_LONG)
	PS_ApplyPedDetail(
						IN.texCoord.xy, SPECULAR_PS_INPUT,
					#if NORMAL_MAP				
						IN.worldTangent.xyz, IN.worldBinormal.xyz,
					#endif
						surfaceInfo, surfaceStandardLightingProperties, specBlue, detailMapAdjust*detailAlpha,
						lerp(BloodFresnel, specularFresnel, detailMapAdjust), useCutsceneOpt);
#else
	surfaceInfo.surface_fresnel =specularFresnel;
#endif

#if WET_PED_EFFECT		// adjust this for wet peds.
	{
		float2 diffuseSpecularWetMult = saturate(float2(IN.WETNESS_PARAM, IN.WETNESS_PARAM - 0.75) * 4.0.xx);
		#if WET_PED_EFFECT_DIFFUSE_ONLY
			// adjust the diffuse only:
			surfaceStandardLightingProperties.diffuseColor.rgb -= half3(surfaceStandardLightingProperties.diffuseColor.rgb*wetnessAdjust.x*diffuseSpecularWetMult.x);	
		#else
			// adjust the diffuse (only on cloth)					
			surfaceStandardLightingProperties.diffuseColor.rgb -= half3((surfaceInfo.surface_isSkin == 0.0) ? surfaceStandardLightingProperties.diffuseColor.rgb*wetnessAdjust.x*diffuseSpecularWetMult.x : 0.0f );	
			
			half2 specAdjust = wetnessAdjust.yz * diffuseSpecularWetMult.yy;
			surfaceStandardLightingProperties.specularExponent  += specAdjust.x;
			surfaceStandardLightingProperties.specularIntensity += specAdjust.y;
		#endif
	}
#endif

	// burnout peds lose their reflectivity props:
	surfaceStandardLightingProperties.diffuseColor.rgb	*= matMaterialColorScale0.x;
	
#if EMISSIVE
	surfaceStandardLightingProperties.emissiveIntensity	*= matMaterialColorScale0.y;
#endif	// EMISSIVE	

#if REFLECT
	surfaceStandardLightingProperties.reflectionColor	*= matMaterialColorScale0.y;
#endif	// REFLECT

#if SPECULAR 
	surfaceStandardLightingProperties.specularIntensity = lerp( ((surfaceInfo.surface_isSkin==0.0)? 0 : 0.01f), surfaceStandardLightingProperties.specularIntensity, matMaterialColorScale0.z);
#endif

#if PED_RIM_LIGHT 
	half3 worldToEyeDir = normalize(half3(IN.surfaceToEye));
	half num = saturate(dot(worldToEyeDir, normalize(surfaceInfo.surface_worldNormal)));

#if (PED_HAIR_CUTOUT || PED_HAIR_ORDERED || PED_HAIR_LONG)
	half rim =lerp( .5,.9,1.-num);// saves 3 cycles on ps3.
#else
	half rim = smoothstep(0.5f, 0.9f, 1.0f - num); 
#endif
	
	half downMask = (surfaceInfo.surface_worldNormal.z + 1.0f) * 0.5f;
	downMask = saturate(downMask - 0.3) / (0.7);

	#if PRECOMPUTE_RIM
		float3 rimValues=saturate(IN.rimValues);
		rim *= rimValues.x;
	#else
		const half bentrange=1.f/25.5f;
		rim = (IN.color0.g-bentrange)*(1.f-bentrange) * rim * gPedRimLightingScale;
	#endif
	rim *= downMask;

	surfaceStandardLightingProperties.diffuseColor.rgb += rim * surfaceStandardLightingProperties.diffuseColor.rgb * matMaterialColorScale0.y;
 
#if  !(PED_HAIR_CUTOUT || PED_HAIR_ORDERED || PED_HAIR_LONG)
	//extra hack for skin - to make lower eyelids and lips less bright
	#if PRECOMPUTE_RIM
		half bend= rimValues.y;
	#else
		half bend=saturate(1.0f - IN.color0.g /bentrange); //only bend within 8bit range 0-10
	#endif
	half3 bendDir=worldToEyeDir;//normalize(float3(-dir.xy,0));
	surfaceInfo.surface_worldNormal.xyz = lerp(surfaceInfo.surface_worldNormal, bendDir, bend);

	#if PRECOMPUTE_RIM
		surfaceStandardLightingProperties.reflectionIntensity=lerp(rimValues.z, BloodReflection, (1-detailMapAdjust));
	#else
		half rv=IN.color0.g+(255.-120.)/255.; // changed to 120 due to rick  // base level is 60,60,60 so we scale that up to one.
		surfaceStandardLightingProperties.reflectionIntensity=lerp(saturate(rv*rv*rv), BloodReflection, (1-detailMapAdjust));
	#endif
#endif

#endif //PED_RIM_LIGHT 

#if PED_HAIR_CUTOUT || PED_HAIR_ORDERED || PED_HAIR_LONG //anisotropic specular
	
	#if PED_RIM_LIGHT
		half3 R = worldToEyeDir;
	#else
		half3 R = gViewInverse[2].xyz;
	#endif

	#if PED_HAIR_ANISOTROPIC 
		half3 worldBinormal=normalize(IN.worldBinormal);
		half2 specSample0 = h4tex2D(AnisoNoiseSpecSampler, (float2)SPECULAR_PS_INPUT * specularNoiseMapUVScaleFactor.xy*.5).ra;
		half2 specSample1 = h2tex2D(AnisoNoiseSpecSampler, (float2)SPECULAR_PS_INPUT * specularNoiseMapUVScaleFactor.zw*.5).rg;
		specSample1 = (2.*specSample1.rg-1.)*0.1;

		half3 L = R;	
		half3 V = gViewInverse[2].xyz;
		
		half3 binorm = normalize(worldBinormal + IN.worldNormal.xyz *  specSample1.r );
		half LT = dot(L, binorm);
		half VT = dot(V, binorm);
	
		half3 binorm2 = normalize(worldBinormal  + IN.worldNormal.xyz *  specSample1.g );	
		half LT2 = dot(L, binorm2);			
		half VT2 = dot(V, binorm2);	

#if USE_ANISOTROPIC_TEXTURE
		half v0 = h1tex2D( ReflectionSampler, float2( LT,VT)*.5+.5);
		half v1 = h1tex2D( ReflectionSampler, float2( LT2,VT2)*.5+.5);
#else
		half v0 = abs(sqrt(1.0 - LT * LT) * sqrt(1.0 - VT * VT) - LT * VT);
		half v1 = abs(sqrt(1.0 - LT2 * LT2) * sqrt(1.0 - VT2 * VT2) - LT2 * VT2);
#endif
		half specHighlight0 = pow( v0, (anisotropicSpecularExponent.x + 8.0 ) );
		half specHighlight1 = pow( v1, (anisotropicSpecularExponent.y + 16.0) );
		half specHighlight = (specHighlight0 *anisotropicSpecularIntensity.x+ specHighlight1*anisotropicSpecularIntensity.y);

		half applyAnsio= surfaceInfo.surface_specMapSample.b<1./32.;

		surfaceStandardLightingProperties.specularIntensity  *=  applyAnsio? specHighlight * specSample0.r:1;
		surfaceStandardLightingProperties.diffuseColor.rgb += anisotropicSpecularColour.rgb * specHighlight1 * 0.5*applyAnsio;

		surfaceStandardLightingProperties.diffuseColor.a= surfaceInfo.surface_diffuseColor.a*saturate(specSample0.g*2.f+AnisotropicAlphaBias);

#else	
	half3 tangent=normalize(IN.worldTangent);
	half3 binorm=normalize(IN.worldBinormal);

		half c = dot(normalize(R), tangent);
		half b = dot(normalize(R), binorm);

		half specU = 1.0f;
		half specV = surfaceStandardLightingProperties.specularExponent;
		
		surfaceStandardLightingProperties.specularExponent = max(dot(float2(c*c,b*b),float2(specU,specV)),specU);
	#endif // PED_HAIR_ANISOTROPIC...
#endif  // PED_HAIR_CUTOUT || PED_HAIR_ORDERED || PED_HAIR_LONG...


#if 1 && PED_HAIR_LONG && __XENON
	// HACKME: 360: longhair: make alpha up to accumulate for RSX differences with alphaToMask:
	half alphaVal = surfaceStandardLightingProperties.diffuseColor.a;
	alphaVal -= 0.25f;
	surfaceStandardLightingProperties.diffuseColor.a = saturate(alphaVal);
#endif

	// Don't do the adjust based on distance but just up close
	surfaceStandardLightingProperties.artificialAmbientScale *= ArtificialBakeAdjustMult();

	// Pack the information into the GBuffer(s).
	return PackGBuffer(surfaceInfo.surface_worldNormal, surfaceStandardLightingProperties);
}

DeferredGBufferC PS_DeferredPedTexturedC(pedVertexOutputD IN, DECLARE_FACING_INPUT(fFacing))
{
#if !RSG_ORBIS && !RSG_DURANGO
	FadeDiscard( IN.pos, globalFade );
#endif
	DECLARE_FACING_FLOAT(fFacing);
	DeferredGBuffer result = PS_DeferredPedTexturedImp(IN, false, false, fFacing);
	return PackDeferredGBufferC(result);
}
DeferredGBufferNC PS_DeferredPedTexturedNC(pedVertexOutputD IN, DECLARE_FACING_INPUT(fFacing))
{
	DECLARE_FACING_FLOAT(fFacing);
	DeferredGBuffer result = PS_DeferredPedTexturedImp(IN, false, false, fFacing);
	return PackDeferredGBufferNC(result);
}

DeferredGBufferC PS_DeferredPedTexturedC_Tint(pedVertexOutputD IN, DECLARE_FACING_INPUT(fFacing))
{
	DECLARE_FACING_FLOAT(fFacing);
	DeferredGBuffer result = PS_DeferredPedTexturedImp(IN, false, true, fFacing);
	return PackDeferredGBufferC(result);
}

DeferredGBufferNC PS_DeferredPedTexturedNC_Tint(pedVertexOutputD IN, DECLARE_FACING_INPUT(fFacing))
{
	DECLARE_FACING_FLOAT(fFacing);
	DeferredGBuffer result = PS_DeferredPedTexturedImp(IN, false, true, fFacing);
	return PackDeferredGBufferNC(result);
}

DeferredGBufferC PS_DeferredPedTexturedCutscene(pedVertexOutputD IN, DECLARE_FACING_INPUT(fFacing))
{
	DECLARE_FACING_FLOAT(fFacing);
	DeferredGBuffer result = PS_DeferredPedTexturedImp(IN, true, false, fFacing);
	return PackDeferredGBufferC(result);
}
DeferredGBufferNC PS_DeferredPedTexturedCutsceneNC(pedVertexOutputD IN, DECLARE_FACING_INPUT(fFacing))
{
	DECLARE_FACING_FLOAT(fFacing);
	DeferredGBuffer result = PS_DeferredPedTexturedImp(IN, true, false, fFacing);
	return PackDeferredGBufferNC( result );
}

#if PED_CLOTH && (__XENON || DX11_CLOTH)
#if __XENON || DX11_CLOTH // yes this seems redundant, but please keep the "|| __XENON" so i can search for it

vertexOutputLD VS_LinearDepthSkinCloth(vertexSkinInputLD IN)
{
	float3 pos;

#ifdef NO_SKINNING
	pos = IN.pos.xyz;
#else

#if USE_UBYTE4
	int4 IndexVector	= D3DCOLORtoUBYTE4(IN.blendindices);
#else
	int4 IndexVector	= D3DCOLORtoUBYTE4(IN.blendindices);
#endif

	bool isCloth = rageSkinGetBone0(IndexVector) > 254;
	if( isCloth )
	{
		int idx0 = rageSkinGetBone1(IndexVector);
		int idx1 = rageSkinGetBone2(IndexVector);
		int idx2 = rageSkinGetBone3(IndexVector);
		float3 normal;

		ClothTransform(idx0, idx1, idx2, IN.weight, pos, normal);
	}
	else
	{
		pos = rageSkinTransform(IN.pos, ComputeSkinMtx(IN.blendindices, IN.weight));
	}
#endif

	vertexOutputLD OUT;

	OUT.pos = TransformCMShadowVert(pos, SHADOW_NEEDS_DEPTHINFO_OUT(OUT));

	SHADOW_USE_TEXTURE_ONLY(OUT.tex = IN.tex);

	return OUT;
}

#endif // __XENON || DX11_CLOTH
#endif // PED_CLOTH && (__XENON || DX11_CLOTH)




float GetPedSurfaceAlpha( pedVertexOutputD IN, float aaAlpha, float aaAlphaRef, inout uint Coverage )
{
float alpha=0;

#if PED_HAIR_ANISOTROPIC
	float detailAlpha = tex2D(AnisoNoiseSpecSampler, (float2)SPECULAR_PS_INPUT * specularNoiseMapUVScaleFactor.xy*.5).a;
	alpha = GetSurfaceAlphaWithCoverage( DIFFUSE_PS_INPUT.xy, DiffuseSampler, aaAlpha, aaAlphaRef, Coverage ) *saturate(detailAlpha*2.f+AnisotropicAlphaBias);
#else
	alpha = GetSurfaceAlphaWithCoverage( DIFFUSE_PS_INPUT, DiffuseSampler, aaAlpha, aaAlphaRef, Coverage );
	#if PEDSHADER_FUR
		alpha = PS_ApplyFur(alpha.xxxx, DIFFUSE_PS_INPUT, IN.furParams).a;
	#endif //PEDSHADER_FUR
#endif

#if PED_HAIR_DIFFUSE_MASK
	alpha = PS_ApplyDiffuseAlphaMask(alpha, DIFFUSE2_PS_INPUT);
#endif

    return alpha;
}

float GetPedSurfaceAlpha( pedVertexOutputD IN )
{
	uint coverage=0;
	return GetPedSurfaceAlpha( IN, 1, 0, coverage );
}

DeferredGBuffer PS_DeferredPedTextured_AlphaClip( pedVertexOutputD IN, bool bHairTint, DECLARE_FACING_INPUT(fFacing))
{
#if !RSG_ORBIS && !RSG_DURANGO
	FadeDiscard( IN.pos, globalFade );
#endif

	DECLARE_FACING_FLOAT(fFacing);
	uint Coverage=0xffffffff;
	float alphaBlend;

#if PEDSHADER_FUR
	float alphaRef = 0.5;
#else
	float alphaRef = PED_ALPHA_REF;
#endif // PEDSHADER_FUR

	if (ENABLE_TRANSPARENCYAA)
	{
		alphaBlend = GetPedSurfaceAlpha(IN, globalAlpha, alphaRef, Coverage);
		rageDiscard(Coverage == 0);
	}
	else
	{
		alphaBlend = GetPedSurfaceAlpha(IN);

	#if PEDSHADER_FUR
		rageDiscard(alphaBlend < alphaRef);
	#elif (__WIN32PC && EMISSIVE)
		//B*5514421: PC: New clothing pieces do not have passive/ghosting transparencies and appear invisible
		//B*5796159: PC: When local player goes into Passive Mode, some clothing worn by remote player becomes invisible 
		rageDiscard(alphaBlend < 1.0f/255.0f);
	#elif (PED_HAIR_CUTOUT && __LOW_QUALITY)
		rageDiscard(alphaBlend <= alphaRef);	// special case for LQ shaders with MSAA enabled
	#else
		rageDiscard(!IsAlphaToCoverage() && alphaBlend <= alphaRef);	
	#endif
	}

	DeferredGBuffer OUT = PS_DeferredPedTexturedImp(IN,false,bHairTint,fFacing);

#if SSTAA
	if (ENABLE_TRANSPARENCYAA)
	{
		OUT.coverage = Coverage;
		OUT.col0.a = 1;
	}
	else
#endif // SSTAA
	{
		float alpha = OUT.col0.a;
		ComputeCoverage(alpha, alphaBlend, PED_ALPHA_REF);
		OUT.col0.a = alpha;
	}

	return OUT;
}

DeferredGBufferC PS_DeferredPedTextured_AlphaClipC( pedVertexOutputD IN, DECLARE_FACING_INPUT(fFacing))
{
	DECLARE_FACING_FLOAT(fFacing);
	DeferredGBuffer result = PS_DeferredPedTextured_AlphaClip(IN,false,fFacing);
	DeferredGBufferC OUT = PackDeferredGBufferC( result );

	return OUT;
}

DeferredGBufferC PS_DeferredPedTextured_AlphaClipC_Tint( pedVertexOutputD IN, DECLARE_FACING_INPUT(fFacing))
{
	DECLARE_FACING_FLOAT(fFacing);
	DeferredGBuffer result = PS_DeferredPedTextured_AlphaClip(IN,true,fFacing);
	DeferredGBufferC OUT = PackDeferredGBufferC( result );

	return OUT;
}

DeferredGBufferNC PS_DeferredPedTextured_AlphaClipNC( pedVertexOutputD IN, DECLARE_FACING_INPUT(fFacing))
{
	DECLARE_FACING_FLOAT(fFacing);
	DeferredGBuffer result = PS_DeferredPedTextured_AlphaClip(IN,false,fFacing);
	return PackDeferredGBufferNC( result );
}

DeferredGBufferNC PS_DeferredPedTextured_AlphaClipNC_Tint( pedVertexOutputD IN, DECLARE_FACING_INPUT(fFacing))
{
	DECLARE_FACING_FLOAT(fFacing);
	DeferredGBuffer result = PS_DeferredPedTextured_AlphaClip(IN,true,fFacing);
	return PackDeferredGBufferNC( result );
}

//
// special version of PS_DeferredTextured() with extra alpha test applied
// to simulate missing AlphaTest/AlphaToMask while in MRT mode:
//
DeferredGBufferNC PS_DeferredPedTextured_SubSampleAlpha( pedVertexOutputD IN, DECLARE_FACING_INPUT(fFacing))
{
#if !RSG_ORBIS && !RSG_DURANGO
	FadeDiscard( IN.pos, globalFade );
#endif

	DECLARE_FACING_FLOAT(fFacing);
	DeferredGBufferNC OUT = PS_DeferredPedTexturedNC(IN,  fFacing);

	rageDiscard((SSAIsOpaquePixel(IN.pos.xy) && OUT.col0.a <= 0.95) || OUT.col0.a <= PED_ALPHA_REF);

	// compensate alpha for singlepass SSA:
	OUT.col0.a *= 1.33f;
	OUT.col0.a = saturate( ((OUT.col0.a-PED_ALPHA_REF)/(0.95-PED_ALPHA_REF))  + 2./255.);

	OUT.col2.a = 1;
	return OUT;
}

DeferredGBufferNC PS_DeferredPedTextured_SubSampleAlpha_SSTA( pedVertexOutputD IN, DECLARE_FACING_INPUT(fFacing))
{
	return PS_DeferredPedTextured_SubSampleAlpha(IN, DECLARE_FACING_NAME(fFacing));
}

DeferredGBufferNC PS_DeferredPedTextured_SubSampleAlpha_Tint( pedVertexOutputD IN, DECLARE_FACING_INPUT(fFacing))
{
	DECLARE_FACING_FLOAT(fFacing);
	DeferredGBufferNC OUT = PS_DeferredPedTexturedNC_Tint(IN,  fFacing);

	rageDiscard((SSAIsOpaquePixel(IN.pos.xy) && OUT.col0.a <= 0.95) || OUT.col0.a <= PED_ALPHA_REF);

	// compensate alpha for singlepass SSA:
	OUT.col0.a *= 1.33f;
	OUT.col0.a = saturate( ((OUT.col0.a-PED_ALPHA_REF)/(0.95-PED_ALPHA_REF))  + 2./255.);

	OUT.col2.a = 1;
	return OUT;
}

half4 PS_DeferredPedTextured_AlphaClipAlphaOnly( pedVertexOutputD IN, DECLARE_FACING_INPUT(fFacing)) : SV_Target0
{
	DECLARE_FACING_FLOAT(fFacing);
	float alphaBlend = GetPedSurfaceAlpha(IN);	
	half col = saturate( ((alphaBlend-PED_ALPHA_REF)/(0.95-PED_ALPHA_REF))  + 2./255.);

	return col;
}

#ifndef RAGE_ENABLE_OFFSET
	#define gbOffsetEnable
#endif

#if TECHNIQUES

#if __MAX
	// ===============================
	// Tool technique
	// ===============================
	technique tool_draw
	{
		pass p0
		{
		#if (PED_HAIR_ORDERED || PED_HAIR_CUTOUT || PED_HAIR_LONG)
			MAX_TOOL_TECHNIQUE_RS_CULLNONE	// Max: force CullMode=None for all ped hair shaders
		#else
			MAX_TOOL_TECHNIQUE_RS
		#endif
#if 0
			// unlit:
			VertexShader= compile VERTEXSHADER	VS_MaxTransform();
			PixelShader	= compile PIXELSHADER	PS_MaxPedTexturedUnlit();
#else
			// lit:
			VertexShader= compile VERTEXSHADER	VS_MaxTransform();
			PixelShader	= compile PIXELSHADER	PS_MaxPedTextured();
#endif
		}  
	}
#else //__MAX...

	#if defined(DECAL_SHADER)
		#define PED_FORWARD_RENDERSTATES()		\
			AlphaBlendEnable	= true;			\
			BlendOp				= Add;			\
			SrcBlend			= SrcAlpha;		\
			DestBlend			= InvSrcAlpha;	\
			AlphaTestEnable		= true;			\
			AlphaFunc			= GREATER;		\
			AlphaRef			= 100;
	#elif PED_HAIR_CUTOUT
		#define PED_FORWARD_RENDERSTATES()     // n/a
	#elif PED_HAIR_LONG || PED_HAIR_SPIKED
		#define PED_FORWARD_RENDERSTATES()     // required renderstate set in ShaderHairSort
	#else
		#define PED_FORWARD_RENDERSTATES()     
	#endif

	// ===============================
	// Unlit techniques
	// ===============================
	#if UNLIT_TECHNIQUES && NONSKINNED_PED_TECHNIQUES
	technique unlit_draw
	{
		pass p0
		{
			PED_FORWARD_RENDERSTATES()
			VertexShader	= compile VERTEXSHADER	VS_PedTransform();
			PixelShader		= compile PIXELSHADER	PS_PedTextured_Zero()  CGC_FLAGS(CGC_DEFAULTFLAGS_HALF);
		}
	}
	#endif // UNLIT_TECHNIQUES && NONSKINNED_PED_TECHNIQUES

	#if UNLIT_TECHNIQUES && DRAWSKINNED_TECHNIQUES
	technique unlit_drawskinned
	{
		pass p0
		{
			PED_FORWARD_RENDERSTATES()
			VertexShader	= compile VERTEXSHADER	VS_PedTransformSkin();
			PixelShader		= compile PIXELSHADER	PS_PedTextured_Zero()  CGC_FLAGS(CGC_DEFAULTFLAGS_HALF);
		}
	}
	#endif // UNLIT_TECHNIQUES && DRAWSKINNED_TECHNIQUES

	// ===============================
	// Lit (default) techniques
	// ===============================
	#if FORWARD_TECHNIQUES && NONSKINNED_PED_TECHNIQUES
	technique lightweight0_draw
	{
		pass p0
		{
			PED_FORWARD_RENDERSTATES()
			VertexShader	= compile VERTEXSHADER	VS_PedTransform();
			PixelShader		= compile PIXELSHADER	PS_PedTextured_Zero()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}

	technique lightweight0CutOut_draw
	{
		pass p0
		{
			PED_FORWARD_RENDERSTATES()
			VertexShader	= compile VERTEXSHADER	VS_PedTransform();
			PixelShader		= compile PIXELSHADER	PS_PedTexturedCutOut_Zero()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}

	#if HAIR_TINT_TECHNIQUES
	technique lightweight0CutOutTint_draw
	{
		pass p0
		{
			PED_FORWARD_RENDERSTATES()
			VertexShader	= compile VERTEXSHADER	VS_PedTransform();
			PixelShader		= compile PIXELSHADER	PS_PedTexturedCutOutTint_Zero()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif // HAIR_TINT_TECHNIQUES
	#endif // FORWARD_TECHNIQUES && NONSKINNED_PED_TECHNIQUES

	#if FORWARD_TECHNIQUES && DRAWSKINNED_TECHNIQUES
	technique lightweight0_drawskinned
	{
		pass p0
		{
			PED_FORWARD_RENDERSTATES()
			VertexShader	= compile VERTEXSHADER	VS_PedTransformSkin();
			PixelShader		= compile PIXELSHADER	PS_PedTextured_Zero()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}

	technique lightweight0CutOut_drawskinned
	{
		pass p0
		{
			PED_FORWARD_RENDERSTATES()
			VertexShader	= compile VERTEXSHADER	VS_PedTransformSkin();
			PixelShader		= compile PIXELSHADER	PS_PedTexturedCutOut_Zero()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}

	#if HAIR_TINT_TECHNIQUES
	technique lightweight0CutOutTint_drawskinned
	{
		pass p0
		{
			PED_FORWARD_RENDERSTATES()
			VertexShader	= compile VERTEXSHADER	VS_PedTransformSkin();
			PixelShader		= compile PIXELSHADER	PS_PedTexturedCutOutTint_Zero()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif // HAIR_TINT_TECHNIQUES
	#endif // FORWARD_TECHNIQUES && DRAWSKINNED_TECHNIQUES

	#if FORWARD_TECHNIQUES && NONSKINNED_PED_TECHNIQUES
	technique lightweight4_draw
	{
		pass p0
		{
			PED_FORWARD_RENDERSTATES()
			VertexShader	= compile VERTEXSHADER	VS_PedTransform();
			PixelShader		= compile PIXELSHADER	PS_PedTextured_Four()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}

	technique lightweight4CutOut_draw
	{
		pass p0
		{
			PED_FORWARD_RENDERSTATES()
			VertexShader	= compile VERTEXSHADER	VS_PedTransform();
			PixelShader		= compile PIXELSHADER	PS_PedTexturedCutOut_Four()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}

	#if HAIR_TINT_TECHNIQUES
	technique lightweight4CutOutTint_draw
	{
		pass p0
		{
			PED_FORWARD_RENDERSTATES()
			VertexShader	= compile VERTEXSHADER	VS_PedTransform();
			PixelShader		= compile PIXELSHADER	PS_PedTexturedCutOutTint_Four()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif // HAIR_TINT_TECHNIQUES
	#endif // FORWARD_TECHNIQUES && NONSKINNED_PED_TECHNIQUES

	#if FORWARD_TECHNIQUES && DRAWSKINNED_TECHNIQUES
	technique lightweight4_drawskinned
	{
		pass p0
		{
			PED_FORWARD_RENDERSTATES()
			VertexShader	= compile VERTEXSHADER	VS_PedTransformSkin();
			PixelShader		= compile PIXELSHADER	PS_PedTextured_Four()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}

	technique lightweight4CutOut_drawskinned
	{
		pass p0
		{
			PED_FORWARD_RENDERSTATES()
			VertexShader	= compile VERTEXSHADER	VS_PedTransformSkin();
			PixelShader		= compile PIXELSHADER	PS_PedTexturedCutOut_Four()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}

	#if HAIR_TINT_TECHNIQUES
	technique lightweight4CutOutTint_drawskinned
	{
		pass p0
		{
			PED_FORWARD_RENDERSTATES()
			VertexShader	= compile VERTEXSHADER	VS_PedTransformSkin();
			PixelShader		= compile PIXELSHADER	PS_PedTexturedCutOutTint_Four()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif // HAIR_TINT_TECHNIQUES
	#endif // FORWARD_TECHNIQUES && DRAWSKINNED_TECHNIQUES

	#if FORWARD_TECHNIQUES && NONSKINNED_PED_TECHNIQUES
	technique lightweight8_draw
	{
		pass p0
		{
			PED_FORWARD_RENDERSTATES()
			VertexShader	= compile VERTEXSHADER	VS_PedTransform();
			PixelShader		= compile PIXELSHADER	PS_PedTextured_Eight()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}

	technique lightweight8CutOut_draw
	{
		pass p0
		{
			PED_FORWARD_RENDERSTATES()
			VertexShader	= compile VERTEXSHADER	VS_PedTransform();
			PixelShader		= compile PIXELSHADER	PS_PedTexturedCutOut_Eight()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}

	#if HAIR_TINT_TECHNIQUES
	technique lightweight8CutOutTint_draw
	{
		pass p0
		{
			PED_FORWARD_RENDERSTATES()
			VertexShader	= compile VERTEXSHADER	VS_PedTransform();
			PixelShader		= compile PIXELSHADER	PS_PedTexturedCutOutTint_Eight()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif // HAIR_TINT_TECHNIQUES
	#endif // FORWARD_TECHNIQUES && NONSKINNED_PED_TECHNIQUES

	#if FORWARD_TECHNIQUES && DRAWSKINNED_TECHNIQUES
	technique lightweight8_drawskinned
	{
		pass p0
		{
			PED_FORWARD_RENDERSTATES()
			VertexShader	= compile VERTEXSHADER	VS_PedTransformSkin();
			PixelShader		= compile PIXELSHADER	PS_PedTextured_Eight()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}

	technique lightweight8CutOut_drawskinned
	{
		pass p0
		{
			PED_FORWARD_RENDERSTATES()
			VertexShader	= compile VERTEXSHADER	VS_PedTransformSkin();
			PixelShader		= compile PIXELSHADER	PS_PedTexturedCutOut_Eight()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}

	#if HAIR_TINT_TECHNIQUES
	technique lightweight8CutOutTint_drawskinned
	{
		pass p0
		{
			PED_FORWARD_RENDERSTATES()
			VertexShader	= compile VERTEXSHADER	VS_PedTransformSkin();
			PixelShader		= compile PIXELSHADER	PS_PedTexturedCutOutTint_Eight()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif // HAIR_TINT_TECHNIQUES
	#endif // FORWARD_TECHNIQUES && DRAWSKINNED_TECHNIQUES

	#if FORWARD_TECHNIQUES && NONSKINNED_PED_TECHNIQUES
	technique draw
	{
		pass p0
		{
			PED_FORWARD_RENDERSTATES()
			VertexShader	= compile VERTEXSHADER	VS_PedTransform();
			PixelShader		= compile PIXELSHADER	PS_PedTextured_Eight()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif // FORWARD_TECHNIQUES && NONSKINNED_PED_TECHNIQUES

	#if FORWARD_TECHNIQUES && DRAWSKINNED_TECHNIQUES
	technique drawskinned
	{
		pass p0
		{
			PED_FORWARD_RENDERSTATES()
			VertexShader	= compile VERTEXSHADER	VS_PedTransformSkin();
			PixelShader		= compile PIXELSHADER	PS_PedTextured_Eight()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif // FORWARD_TECHNIQUES && DRAWSKINNED_TECHNIQUES

	#if HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED && NONSKINNED_PED_TECHNIQUES
	technique lightweightHighQuality0_draw
	{
		pass p0
		{
			PED_FORWARD_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_PedTransform();
			PixelShader  = compile PIXELSHADER	PS_PedTextured_ZeroHighQuality()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}

	technique lightweightHighQuality0CutOut_draw
	{
		pass p0
		{
			PED_FORWARD_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_PedTransform();
			PixelShader  = compile PIXELSHADER	PS_PedTexturedCutOut_ZeroHighQuality()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif // HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED && NONSKINNED_PED_TECHNIQUES

	#if HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED && DRAWSKINNED_TECHNIQUES
	technique lightweightHighQuality0_drawskinned
	{
		pass p0
		{
			PED_FORWARD_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_PedTransformSkin();
			PixelShader  = compile PIXELSHADER	PS_PedTextured_ZeroHighQuality()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}

	technique lightweightHighQuality0CutOut_drawskinned
	{
		pass p0
		{
			PED_FORWARD_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_PedTransformSkin();
			PixelShader  = compile PIXELSHADER	PS_PedTexturedCutOut_ZeroHighQuality()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif // HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED && DRAWSKINNED_TECHNIQUES

	#if HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED && NONSKINNED_PED_TECHNIQUES
	technique lightweightHighQuality4_draw
	{
		pass p0
		{
			PED_FORWARD_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_PedTransform();
			PixelShader  = compile PIXELSHADER	PS_PedTextured_FourHighQuality()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}

	technique lightweightHighQuality4CutOut_draw
	{
		pass p0
		{
			PED_FORWARD_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_PedTransform();
			PixelShader  = compile PIXELSHADER	PS_PedTexturedCutOut_FourHighQuality()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif // HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED && NONSKINNED_PED_TECHNIQUES

	#if HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED && DRAWSKINNED_TECHNIQUES
	technique lightweightHighQuality4_drawskinned
	{
		pass p0
		{
			PED_FORWARD_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_PedTransformSkin();
			PixelShader  = compile PIXELSHADER	PS_PedTextured_FourHighQuality()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}

	technique lightweightHighQuality4CutOut_drawskinned
	{
		pass p0
		{
			PED_FORWARD_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_PedTransformSkin();
			PixelShader  = compile PIXELSHADER	PS_PedTexturedCutOut_FourHighQuality()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif // HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED && DRAWSKINNED_TECHNIQUES

	#if HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED && NONSKINNED_PED_TECHNIQUES
	technique lightweightHighQuality8_draw
	{
		pass p0
		{
			PED_FORWARD_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_PedTransform();
			PixelShader  = compile PIXELSHADER	PS_PedTextured_EightHighQuality()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}

	technique lightweightHighQuality8CutOut_draw
	{
		pass p0
		{
			PED_FORWARD_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_PedTransform();
			PixelShader  = compile PIXELSHADER	PS_PedTexturedCutOut_EightHighQuality()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif // HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED && NONSKINNED_PED_TECHNIQUES

	#if HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED && DRAWSKINNED_TECHNIQUES
	technique lightweightHighQuality8_drawskinned
	{
		pass p0
		{
			PED_FORWARD_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_PedTransformSkin();
			PixelShader  = compile PIXELSHADER	PS_PedTextured_EightHighQuality()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}

	technique lightweightHighQuality8CutOut_drawskinned
	{
		pass p0
		{
			PED_FORWARD_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_PedTransformSkin();
			PixelShader  = compile PIXELSHADER	PS_PedTexturedCutOut_EightHighQuality()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif // HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED && DRAWSKINNED_TECHNIQUES
	
	#if WATER_REFLECTION_TECHNIQUES && NONSKINNED_PED_TECHNIQUES
	technique waterreflection_draw
	{
		pass p0
		{
			PED_FORWARD_RENDERSTATES()
			VertexShader	= compile VERTEXSHADER	VS_PedTransformWaterReflection();
			PixelShader		= compile PIXELSHADER	PS_PedTexturedWaterReflection()  CGC_FLAGS(CGC_DEFAULTFLAGS_HALF);
		}
	}

	technique waterreflectionalphaclip_draw
	{
		pass p0
		{
			PED_FORWARD_RENDERSTATES()
			VertexShader	= compile VERTEXSHADER	VS_PedTransformWaterReflection();
			PixelShader		= compile PIXELSHADER	PS_PedTexturedWaterReflection()  CGC_FLAGS(CGC_DEFAULTFLAGS_HALF);
		}
	}

	technique waterreflectionalphacliptint_draw
	{
		pass p0
		{
			PED_FORWARD_RENDERSTATES()
			VertexShader	= compile VERTEXSHADER	VS_PedTransformWaterReflection();
			PixelShader		= compile PIXELSHADER	PS_PedTexturedWaterReflection_Tint()  CGC_FLAGS(CGC_DEFAULTFLAGS_HALF);
		}
	}	
	#endif // WATER_REFLECTION_TECHNIQUES && NONSKINNED_PED_TECHNIQUES

	#if WATER_REFLECTION_TECHNIQUES && DRAWSKINNED_TECHNIQUES
	technique waterreflection_drawskinned
	{
		pass p0
		{
			PED_FORWARD_RENDERSTATES()
			VertexShader	= compile VERTEXSHADER	VS_PedTransformSkinWaterReflection();
			PixelShader		= compile PIXELSHADER	PS_PedTexturedWaterReflection()  CGC_FLAGS(CGC_DEFAULTFLAGS_HALF);
		}
	}

	technique waterreflectionalphaclip_drawskinned
	{
		pass p0
		{
			PED_FORWARD_RENDERSTATES()
			VertexShader	= compile VERTEXSHADER	VS_PedTransformSkinWaterReflection();
			PixelShader		= compile PIXELSHADER	PS_PedTexturedWaterReflection()  CGC_FLAGS(CGC_DEFAULTFLAGS_HALF);
		}
	}

	technique waterreflectionalphacliptint_drawskinned
	{
		pass p0
		{
			PED_FORWARD_RENDERSTATES()
			VertexShader	= compile VERTEXSHADER	VS_PedTransformSkinWaterReflection();
			PixelShader		= compile PIXELSHADER	PS_PedTexturedWaterReflection_Tint()  CGC_FLAGS(CGC_DEFAULTFLAGS_HALF);
		}
	}
	#endif // WATER_REFLECTION_TECHNIQUES && DRAWSKINNED_TECHNIQUES

	// ===============================
	// Seethrough techniques
	// ===============================
	#if NONSKINNED_PED_TECHNIQUES
	technique seethrough_draw
	{
		pass p0
		{
			ZwriteEnable    = true; // is this necessary?

			VertexShader	= compile VERTEXSHADER	VS_PedTransform_SeeThrough();
			PixelShader		= compile PIXELSHADER	PS_PedTextured_SeeThrough()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif // NONSKINNED_PED_TECHNIQUES

	#if DRAWSKINNED_TECHNIQUES
	technique seethrough_drawskinned
	{
		pass p0
		{
			ZwriteEnable    = true; // is this necessary?

			VertexShader	= compile VERTEXSHADER	VS_PedTransformSkin_SeeThrough();
			PixelShader		= compile PIXELSHADER	PS_PedTextured_SeeThrough()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif // DRAWSKINNED_TECHNIQUES

	// ===============================
	// UI techniques
	// ===============================
	// used for rendering peds as part of ui - manual gamma encoding/decoding
	// assuming output to rgba8
	#if UI_TECHNIQUES

	#if FORWARD_TECHNIQUES && NONSKINNED_PED_TECHNIQUES
	technique ui_draw
	{
		pass p0
		{
			PED_FORWARD_RENDERSTATES()
			VertexShader	= compile VERTEXSHADER	VS_PedTransform();
			PixelShader		= compile PIXELSHADER	PS_PedTextured_Four_UI()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif // FORWARD_TECHNIQUES && NONSKINNED_PED_TECHNIQUES

	#if FORWARD_TECHNIQUES && DRAWSKINNED_TECHNIQUES
	technique ui_drawskinned
	{
		pass p0
		{
			PED_FORWARD_RENDERSTATES()
			VertexShader	= compile VERTEXSHADER	VS_PedTransformSkin();
			PixelShader		= compile PIXELSHADER	PS_PedTextured_Four_UI()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif // FORWARD_TECHNIQUES && DRAWSKINNED_TECHNIQUES

	#endif // UI_TECHNIQUES

	// ===============================
	// Deferred techniques
	// ===============================
	#if DEFERRED_TECHNIQUES && NON_CUTOUT_DEFERRED_TECHNIQUES && NONSKINNED_PED_TECHNIQUES
#if RSG_PC && SSTAA
	technique deferred_draw
	{
		pass p0
		{
		#if PED_HAIR_CUTOUT
			#if __XENON
				AlphaBlendEnable = false;
				AlphaToMaskEnable = false;
			#endif	
			SET_COLOR_WRITE_ENABLE(RED+GREEN+BLUE, RED+GREEN+BLUE+ALPHA, RED+GREEN+BLUE, RED+GREEN+BLUE)
					
			VertexShader	= compile VERTEXSHADER			VS_PedTransformD();
			PixelShader		= compile PIXELSHADER			PS_DeferredPedTextured_AlphaClipNC()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		#endif	//PED_HAIR_CUTOUT...

		#if PED_HAIR_LONG
			#if __XENON
				AlphaToMaskEnable= true;
				SET_COLOR_WRITE_ENABLE(RED+GREEN+BLUE, RED+GREEN+BLUE, RED+GREEN+BLUE, RED+GREEN+BLUE)
			#endif	
		#endif	//PED_HAIR_LONG...

		PED_DECAL_RENDERSTATES()

		#if !PED_HAIR_CUTOUT
			VertexShader	= compile VERTEXSHADER			VS_PedTransformD();
			PixelShader		= compile PIXELSHADER			PS_DeferredPedTexturedNC()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		#endif
		}
	}
#endif // RSG_PC && SSTAA

	technique SSTA_TECHNIQUE(deferred_draw)
	{
		pass p0
		{
		#if PED_HAIR_CUTOUT
			SET_COLOR_WRITE_ENABLE(RED+GREEN+BLUE, RED+GREEN+BLUE+ALPHA, RED+GREEN+BLUE, RED+GREEN+BLUE)
					
			VertexShader	= compile VERTEXSHADER			VS_PedTransformD();
			PixelShader		= compile PS_GBUFFER_COVERAGE	PS_DeferredPedTextured_AlphaClipC()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		#endif	//PED_HAIR_CUTOUT...

		PED_DECAL_RENDERSTATES()

		#if !PED_HAIR_CUTOUT
			VertexShader	= compile VERTEXSHADER			VS_PedTransformD();
			PixelShader		= compile PS_GBUFFER_COVERAGE	PS_DeferredPedTexturedC()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		#endif
		}
	}
	#endif // DEFERRED_TECHNIQUES && NON_CUTOUT_DEFERRED_TECHNIQUES && NONSKINNED_PED_TECHNIQUES

	// ===============================
	#if DEFERRED_TECHNIQUES && NON_CUTOUT_DEFERRED_TECHNIQUES && NONSKINNED_PED_TECHNIQUES
#if SSTAA && RSG_PC
	technique deferredcutscene_draw
	{
		pass p0
		{
		#if PED_HAIR_CUTOUT
			#if __XENON
				AlphaBlendEnable = false;
				AlphaToMaskEnable = false;
			#endif	
			SET_COLOR_WRITE_ENABLE(RED+GREEN+BLUE, RED+GREEN+BLUE+ALPHA, RED+GREEN+BLUE, RED+GREEN+BLUE)
					
			VertexShader	= compile VERTEXSHADER	VS_PedTransformD();
			PixelShader		= compile PIXELSHADER	PS_DeferredPedTextured_AlphaClipNC()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		#endif	//PED_HAIR_CUTOUT...

		#if PED_HAIR_LONG
			#if __XENON
				AlphaToMaskEnable= true;
				SET_COLOR_WRITE_ENABLE(RED+GREEN+BLUE, RED+GREEN+BLUE, RED+GREEN+BLUE, RED+GREEN+BLUE)
			#endif	
		#endif	//PED_HAIR_LONG...

		PED_DECAL_RENDERSTATES()

		#if !PED_HAIR_CUTOUT
			VertexShader	= compile VERTEXSHADER	VS_PedTransformD();
			PixelShader		= compile PIXELSHADER	PS_DeferredPedTexturedCutsceneNC()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		#endif
		}
	}
#endif // SSTAA && RSG_PC

	technique SSTA_TECHNIQUE(deferredcutscene_draw)
	{
		pass p0
		{
		#if PED_HAIR_CUTOUT
			#if __XENON
				AlphaBlendEnable = false;
				AlphaToMaskEnable = false;
			#endif	
			SET_COLOR_WRITE_ENABLE(RED+GREEN+BLUE, RED+GREEN+BLUE+ALPHA, RED+GREEN+BLUE, RED+GREEN+BLUE)
					
			VertexShader	= compile VERTEXSHADER	VS_PedTransformD();
			PixelShader		= compile PS_GBUFFER_COVERAGE	PS_DeferredPedTextured_AlphaClipC()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		#endif	//PED_HAIR_CUTOUT...

		#if PED_HAIR_LONG
			#if __XENON
				AlphaToMaskEnable= true;
				SET_COLOR_WRITE_ENABLE(RED+GREEN+BLUE, RED+GREEN+BLUE, RED+GREEN+BLUE, RED+GREEN+BLUE)
			#endif	
		#endif	//PED_HAIR_LONG...

		PED_DECAL_RENDERSTATES()

		#if !PED_HAIR_CUTOUT
			VertexShader	= compile VERTEXSHADER			VS_PedTransformD();
			PixelShader		= compile PS_GBUFFER_COVERAGE	PS_DeferredPedTexturedCutscene()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		#endif
		}
	}
	#endif // DEFERRED_TECHNIQUES && NON_CUTOUT_DEFERRED_TECHNIQUES && NONSKINNED_PED_TECHNIQUES

	#if DEFERRED_TECHNIQUES && NON_CUTOUT_DEFERRED_TECHNIQUES && DRAWSKINNED_TECHNIQUES
#if SSTAA && RSG_PC
	technique deferred_drawskinned
	{
		pass p0
		{
		#if PED_HAIR_CUTOUT
			#if __XENON
				AlphaBlendEnable = false;
				AlphaToMaskEnable = false;
			#endif
			SET_COLOR_WRITE_ENABLE(RED+GREEN+BLUE, RED+GREEN+BLUE+ALPHA, RED+GREEN+BLUE, RED+GREEN+BLUE)

			VertexShader	= compile VERTEXSHADER	VS_PedTransformSkinD();
			PixelShader		= compile PIXELSHADER	PS_DeferredPedTextured_AlphaClipNC()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		#endif	//PED_HAIR_CUTOUT...
		
		#if PED_HAIR_LONG
			#if __XENON
				AlphaToMaskEnable= true;
				SET_COLOR_WRITE_ENABLE(RED+GREEN+BLUE, RED+GREEN+BLUE, RED+GREEN+BLUE, RED+GREEN+BLUE)
			#endif	
		#endif	//PED_HAIR_LONG...

		PED_DECAL_RENDERSTATES()

		#if !PED_HAIR_CUTOUT
			VertexShader	= compile VERTEXSHADER			VS_PedTransformSkinD();
			PixelShader		= compile PIXELSHADER			PS_DeferredPedTexturedNC()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		#endif
		}
	}
#endif // SSTAA && RSG_PC

	technique SSTA_TECHNIQUE(deferred_drawskinned)
	{
		pass p0
		{
		#if PED_HAIR_CUTOUT
			#if __XENON
				AlphaBlendEnable = false;
				AlphaToMaskEnable = false;
			#endif
			SET_COLOR_WRITE_ENABLE(RED+GREEN+BLUE, RED+GREEN+BLUE+ALPHA, RED+GREEN+BLUE, RED+GREEN+BLUE)

			VertexShader	= compile VERTEXSHADER			VS_PedTransformSkinD();
			PixelShader		= compile PS_GBUFFER_COVERAGE	PS_DeferredPedTextured_AlphaClipC()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		#endif	//PED_HAIR_CUTOUT...
		
		#if PED_HAIR_LONG
			#if __XENON
				AlphaToMaskEnable= true;
				SET_COLOR_WRITE_ENABLE(RED+GREEN+BLUE, RED+GREEN+BLUE, RED+GREEN+BLUE, RED+GREEN+BLUE)
			#endif	
		#endif	//PED_HAIR_LONG...

		PED_DECAL_RENDERSTATES()

		#if !PED_HAIR_CUTOUT
			VertexShader	= compile VERTEXSHADER			VS_PedTransformSkinD();
			PixelShader		= compile PS_GBUFFER_COVERAGE	PS_DeferredPedTexturedC()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		#endif
		}
	}
	#endif // DEFERRED_TECHNIQUES && NON_CUTOUT_DEFERRED_TECHNIQUES && DRAWSKINNED_TECHNIQUES


	#if DEFERRED_TECHNIQUES && NON_CUTOUT_DEFERRED_TECHNIQUES && DRAWSKINNED_TECHNIQUES
#if SSTAA && RSG_PC
	technique deferredcutscene_drawskinned
	{
		pass p0
		{
		#if PED_HAIR_CUTOUT
			#if __XENON
				AlphaBlendEnable = false;
				AlphaToMaskEnable = false;
			#endif
			//SET_COLOR_WRITE_ENABLE(RED+GREEN+BLUE, RED+GREEN+BLUE+ALPHA, RED+GREEN+BLUE, RED+GREEN+BLUE)

			VertexShader	= compile VERTEXSHADER			VS_PedTransformSkinD();
			PixelShader		= compile PIXELSHADER			PS_DeferredPedTextured_AlphaClipNC()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		#endif	//PED_HAIR_CUTOUT...
		
		#if PED_HAIR_LONG
			#if __XENON
				AlphaToMaskEnable= true;
			//	SET_COLOR_WRITE_ENABLE(RED+GREEN+BLUE, RED+GREEN+BLUE, RED+GREEN+BLUE, RED+GREEN+BLUE)
			#endif	
		#endif	//PED_HAIR_LONG...

		PED_DECAL_RENDERSTATES()

		#if !PED_HAIR_CUTOUT
			VertexShader	= compile VERTEXSHADER			VS_PedTransformSkinD();
			PixelShader		= compile PIXELSHADER			PS_DeferredPedTexturedCutsceneNC()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		#endif
		}
	}
#endif // SSTAA && RSG_PC

	technique SSTA_TECHNIQUE(deferredcutscene_drawskinned)
	{
		pass p0
		{
		#if PED_HAIR_CUTOUT
			//SET_COLOR_WRITE_ENABLE(RED+GREEN+BLUE, RED+GREEN+BLUE+ALPHA, RED+GREEN+BLUE, RED+GREEN+BLUE)

			VertexShader	= compile VERTEXSHADER			VS_PedTransformSkinD();
			PixelShader		= compile PS_GBUFFER_COVERAGE	PS_DeferredPedTextured_AlphaClipC()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		#endif	//PED_HAIR_CUTOUT...
		
		PED_DECAL_RENDERSTATES()

		#if !PED_HAIR_CUTOUT
			VertexShader	= compile VERTEXSHADER			VS_PedTransformSkinD();
			PixelShader		= compile PS_GBUFFER_COVERAGE	PS_DeferredPedTexturedCutscene()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		#endif
		}
	}
	#endif // DEFERRED_TECHNIQUES && NON_CUTOUT_DEFERRED_TECHNIQUES && DRAWSKINNED_TECHNIQUES


	#if DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES && NONSKINNED_PED_TECHNIQUES

#if SSTAA && RSG_PC
	technique deferredalphaclip_draw
	{
	#if PED_HAIR_LONG
		// long hair: pass0 w/no alphaClipping:
		pass p0
		{
		#if __XENON
			AlphaToMaskEnable= true;
			SET_COLOR_WRITE_ENABLE(RED+GREEN+BLUE, RED+GREEN+BLUE, RED+GREEN+BLUE, RED+GREEN+BLUE)
		#endif	
			VertexShader	= compile VERTEXSHADER			VS_PedTransformD();
			PixelShader		= compile PIXELSHADER			PS_DeferredPedTexturedNC()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
		// long hair: pass1 w/alphaClipping:
		pass p1
		{
		#if __XENON
			AlphaToMaskEnable= true;
			SET_COLOR_WRITE_ENABLE(RED+GREEN+BLUE, RED+GREEN+BLUE, RED+GREEN+BLUE, RED+GREEN+BLUE)
		#endif	
			VertexShader	= compile VERTEXSHADER			VS_PedTransformD();
			PixelShader		= compile PIXELSHADER			PS_DeferredPedTextured_AlphaClipNC()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	#else //PED_HAIR_LONG...
		pass p0
		{
		#if PED_HAIR_CUTOUT && __XENON
			AlphaBlendEnable = false;
			AlphaToMaskEnable = false;
			SET_COLOR_WRITE_ENABLE(RED+GREEN+BLUE, RED+GREEN+BLUE+ALPHA, RED+GREEN+BLUE, RED+GREEN+BLUE)
		#endif	//PED_HAIR_CUTOUT...

			PED_DECAL_RENDERSTATES()

			VertexShader	= compile VERTEXSHADER			VS_PedTransformD();
			PixelShader		= compile PIXELSHADER			PS_DeferredPedTextured_AlphaClipNC()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}

	#if PED_HAIR_SPIKED
		pass p1	// spiked 1st pass - normal cap with no alpha test
		{
			VertexShader	= compile VERTEXSHADER			VS_PedTransformD();
			PixelShader		= compile PIXELSHADER			PS_DeferredPedTexturedNC()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	#endif
	#endif //!PED_HAIR_LONG...
	}

	#if HAIR_TINT_TECHNIQUES
	technique deferredalphacliptint_draw
	{
	#if PED_HAIR_LONG
		// long hair: pass0 w/no alphaClipping:
		pass p0
		{
		#if __XENON
			AlphaToMaskEnable= true;
			SET_COLOR_WRITE_ENABLE(RED+GREEN+BLUE, RED+GREEN+BLUE, RED+GREEN+BLUE, RED+GREEN+BLUE)
		#endif	
			VertexShader	= compile VERTEXSHADER			VS_PedTransformD();
			PixelShader		= compile PIXELSHADER			PS_DeferredPedTexturedNC_Tint()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
		// long hair: pass1 w/alphaClipping:
		pass p1
		{
		#if __XENON
			AlphaToMaskEnable= true;
			SET_COLOR_WRITE_ENABLE(RED+GREEN+BLUE, RED+GREEN+BLUE, RED+GREEN+BLUE, RED+GREEN+BLUE)
		#endif	
			VertexShader	= compile VERTEXSHADER			VS_PedTransformD();
			PixelShader		= compile PIXELSHADER			PS_DeferredPedTextured_AlphaClipNC_Tint()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	#else //PED_HAIR_LONG...
		pass p0
		{
		#if PED_HAIR_CUTOUT && __XENON
			AlphaBlendEnable = false;
			AlphaToMaskEnable = false;
			SET_COLOR_WRITE_ENABLE(RED+GREEN+BLUE, RED+GREEN+BLUE+ALPHA, RED+GREEN+BLUE, RED+GREEN+BLUE)
		#endif	//PED_HAIR_CUTOUT...

			PED_DECAL_RENDERSTATES()

			VertexShader	= compile VERTEXSHADER			VS_PedTransformD();
			PixelShader		= compile PIXELSHADER			PS_DeferredPedTextured_AlphaClipNC_Tint()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}

	#if PED_HAIR_SPIKED
		pass p1	// spiked 1st pass - normal cap with no alpha test
		{
			VertexShader	= compile VERTEXSHADER			VS_PedTransformD();
			PixelShader		= compile PIXELSHADER			PS_DeferredPedTexturedNC_Tint()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	#endif
	#endif //!PED_HAIR_LONG...
	#endif //HAIR_TINT_TECHNIQUES...
#endif // SSTAA && RSG_PC

	technique SSTA_TECHNIQUE(deferredalphaclip_draw)
	{
	#if PED_HAIR_LONG
		// long hair: pass0 w/no alphaClipping:
		pass p0
		{
			VertexShader	= compile VERTEXSHADER			VS_PedTransformD();
			PixelShader		= compile PS_GBUFFER_COVERAGE	PS_DeferredPedTexturedC()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
		// long hair: pass1 w/alphaClipping:
		pass p1
		{
			VertexShader	= compile VERTEXSHADER			VS_PedTransformD();
			PixelShader		= compile PS_GBUFFER_COVERAGE	PS_DeferredPedTextured_AlphaClipC()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	#else //PED_HAIR_LONG...
		pass p0
		{
			PED_DECAL_RENDERSTATES()

			VertexShader	= compile VERTEXSHADER			VS_PedTransformD();
			PixelShader		= compile PS_GBUFFER_COVERAGE	PS_DeferredPedTextured_AlphaClipC()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}

	#if PED_HAIR_SPIKED
		pass p1	// spiked 1st pass - normal cap with no alpha test
		{
			VertexShader	= compile VERTEXSHADER	VS_PedTransformD();
			PixelShader		= compile PS_GBUFFER_COVERAGE	PS_DeferredPedTexturedC()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	#endif
	#endif //!PED_HAIR_LONG...
	}

	#if HAIR_TINT_TECHNIQUES
	technique SSTA_TECHNIQUE(deferredalphacliptint_draw)
	{
	#if PED_HAIR_LONG
		// long hair: pass0 w/no alphaClipping:
		pass p0
		{
			VertexShader	= compile VERTEXSHADER			VS_PedTransformD();
			PixelShader		= compile PS_GBUFFER_COVERAGE	PS_DeferredPedTexturedC_Tint()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
		// long hair: pass1 w/alphaClipping:
		pass p1
		{
			VertexShader	= compile VERTEXSHADER			VS_PedTransformD();
			PixelShader		= compile PS_GBUFFER_COVERAGE	PS_DeferredPedTextured_AlphaClipC_Tint()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	#else //PED_HAIR_LONG...
		pass p0
		{
			PED_DECAL_RENDERSTATES()

			VertexShader	= compile VERTEXSHADER			VS_PedTransformD();
			PixelShader		= compile PS_GBUFFER_COVERAGE	PS_DeferredPedTextured_AlphaClipC_Tint()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}

	#if PED_HAIR_SPIKED
		pass p1	// spiked 1st pass - normal cap with no alpha test
		{
			VertexShader	= compile VERTEXSHADER	VS_PedTransformD();
			PixelShader		= compile PS_GBUFFER_COVERAGE	PS_DeferredPedTexturedC_Tint()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	#endif
	#endif //!PED_HAIR_LONG...
	}
	#endif //HAIR_TINT_TECHNIQUES...
	#endif // DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES && NONSKINNED_PED_TECHNIQUES

	#if DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES && DRAWSKINNED_TECHNIQUES
#if SSTAA && RSG_PC
	technique deferredalphaclip_drawskinned
	{
	#if PED_HAIR_LONG
		// long hair: non-alphaClipping pass0:
		pass p0
		{
		#if __XENON
			AlphaToMaskEnable= true;
			SET_COLOR_WRITE_ENABLE(RED+GREEN+BLUE, RED+GREEN+BLUE, RED+GREEN+BLUE, RED+GREEN+BLUE)
		#endif	
			VertexShader	= compile VERTEXSHADER			VS_PedTransformSkinD();
			PixelShader		= compile PIXELSHADER			PS_DeferredPedTexturedNC()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
		// long hair: alphaClipping pass1:
		pass p1
		{
		#if __XENON
			AlphaToMaskEnable= true;
			SET_COLOR_WRITE_ENABLE(RED+GREEN+BLUE, RED+GREEN+BLUE, RED+GREEN+BLUE, RED+GREEN+BLUE)
		#endif	
			VertexShader	= compile VERTEXSHADER			VS_PedTransformSkinD();
			PixelShader		= compile PIXELSHADER			PS_DeferredPedTextured_AlphaClipNC()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	#else //PED_HAIR_LONG...
		pass p0
		{
		#if PED_HAIR_CUTOUT && __XENON
			AlphaBlendEnable = false;
			AlphaToMaskEnable = false;
			SET_COLOR_WRITE_ENABLE(RED+GREEN+BLUE, RED+GREEN+BLUE+ALPHA, RED+GREEN+BLUE, RED+GREEN+BLUE)
		#endif	//PED_HAIR_CUTOUT...

			PED_DECAL_RENDERSTATES()

			VertexShader	= compile VERTEXSHADER			VS_PedTransformSkinD();
			PixelShader		= compile PIXELSHADER			PS_DeferredPedTextured_AlphaClipNC()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}

	#if PED_HAIR_SPIKED
		pass p1	// spiked 1st pass - normal cap with no alpha test
		{
			VertexShader	= compile VERTEXSHADER			VS_PedTransformSkinD();
			PixelShader		= compile PIXELSHADER			PS_DeferredPedTexturedNC()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	#endif
	#endif // !PED_HAIR_LONG...
	}

	#if HAIR_TINT_TECHNIQUES
	technique deferredalphacliptint_drawskinned
	{
	#if PED_HAIR_LONG
		// long hair: non-alphaClipping pass0:
		pass p0
		{
		#if __XENON
			AlphaToMaskEnable= true;
			SET_COLOR_WRITE_ENABLE(RED+GREEN+BLUE, RED+GREEN+BLUE, RED+GREEN+BLUE, RED+GREEN+BLUE)
		#endif	
			VertexShader	= compile VERTEXSHADER			VS_PedTransformSkinD();
			PixelShader		= compile PIXELSHADER			PS_DeferredPedTexturedNC_Tint()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
		// long hair: alphaClipping pass1:
		pass p1
		{
		#if __XENON
			AlphaToMaskEnable= true;
			SET_COLOR_WRITE_ENABLE(RED+GREEN+BLUE, RED+GREEN+BLUE, RED+GREEN+BLUE, RED+GREEN+BLUE)
		#endif	
			VertexShader	= compile VERTEXSHADER			VS_PedTransformSkinD();
			PixelShader		= compile PIXELSHADER			PS_DeferredPedTextured_AlphaClipNC_Tint()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	#else //PED_HAIR_LONG...
		pass p0
		{
		#if PED_HAIR_CUTOUT && __XENON
			AlphaBlendEnable = false;
			AlphaToMaskEnable = false;
			SET_COLOR_WRITE_ENABLE(RED+GREEN+BLUE, RED+GREEN+BLUE+ALPHA, RED+GREEN+BLUE, RED+GREEN+BLUE)
		#endif	//PED_HAIR_CUTOUT...

			PED_DECAL_RENDERSTATES()

			VertexShader	= compile VERTEXSHADER			VS_PedTransformSkinD();
			PixelShader		= compile PIXELSHADER			PS_DeferredPedTextured_AlphaClipNC_Tint()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}

	#if PED_HAIR_SPIKED
		pass p1	// spiked 1st pass - normal cap with no alpha test
		{
			VertexShader	= compile VERTEXSHADER			VS_PedTransformSkinD();
			PixelShader		= compile PIXELSHADER			PS_DeferredPedTexturedNC_Tint()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	#endif
	#endif // !PED_HAIR_LONG...
	}
	#endif //HAIR_TINT_TECHNIQUES...
#endif // SSTAA && RSG_PC

	technique SSTA_TECHNIQUE(deferredalphaclip_drawskinned)
	{
	#if PED_HAIR_LONG
		// long hair: non-alphaClipping pass0:
		pass p0
		{
			VertexShader	= compile VERTEXSHADER			VS_PedTransformSkinD();
			PixelShader		= compile PS_GBUFFER_COVERAGE	PS_DeferredPedTexturedC()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
		// long hair: alphaClipping pass1:
		pass p1
		{
			VertexShader	= compile VERTEXSHADER			VS_PedTransformSkinD();
			PixelShader		= compile PS_GBUFFER_COVERAGE	PS_DeferredPedTextured_AlphaClipC()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	#else //PED_HAIR_LONG...
		pass p0
		{
			PED_DECAL_RENDERSTATES()

			VertexShader	= compile VERTEXSHADER			VS_PedTransformSkinD();
			PixelShader		= compile PS_GBUFFER_COVERAGE	PS_DeferredPedTextured_AlphaClipC()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}

	#if PED_HAIR_SPIKED
		pass p1	// spiked 1st pass - normal cap with no alpha test
		{
			VertexShader	= compile VERTEXSHADER			VS_PedTransformSkinD();
			PixelShader		= compile PS_GBUFFER_COVERAGE	PS_DeferredPedTexturedC()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	#endif
	#endif // !PED_HAIR_LONG...
	}	

	#if HAIR_TINT_TECHNIQUES
	technique SSTA_TECHNIQUE(deferredalphacliptint_drawskinned)
	{
	#if PED_HAIR_LONG
		// long hair: non-alphaClipping pass0:
		pass p0
		{
			VertexShader	= compile VERTEXSHADER			VS_PedTransformSkinD();
			PixelShader		= compile PS_GBUFFER_COVERAGE	PS_DeferredPedTexturedC_Tint()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
		// long hair: alphaClipping pass1:
		pass p1
		{
			VertexShader	= compile VERTEXSHADER			VS_PedTransformSkinD();
			PixelShader		= compile PS_GBUFFER_COVERAGE	PS_DeferredPedTextured_AlphaClipC_Tint()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	#else //PED_HAIR_LONG...
		pass p0
		{
			PED_DECAL_RENDERSTATES()

			VertexShader	= compile VERTEXSHADER			VS_PedTransformSkinD();
			PixelShader		= compile PS_GBUFFER_COVERAGE	PS_DeferredPedTextured_AlphaClipC_Tint()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}

	#if PED_HAIR_SPIKED
		pass p1	// spiked 1st pass - normal cap with no alpha test
		{
			VertexShader	= compile VERTEXSHADER			VS_PedTransformSkinD();
			PixelShader		= compile PS_GBUFFER_COVERAGE	PS_DeferredPedTexturedC_Tint()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	#endif
	#endif // !PED_HAIR_LONG...
	}	
	#endif //HAIR_TINT_TECHNIQUES...
	#endif // DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES && DRAWSKINNED_TECHNIQUES

	#if DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES && NONSKINNED_PED_TECHNIQUES
#if SSTAA && RSG_PC
	technique deferredsubsamplealpha_draw
	{
	#if PED_HAIR_LONG
		// long hair: pass0 w/no alphaClipping:
		pass p0
		{
			VertexShader	= compile VERTEXSHADER			VS_PedTransformD();
			PixelShader		= compile PIXELSHADER			PS_DeferredPedTexturedNC()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
		// long hair: pass1 w/alphaClipping:
		pass p1
		{
			VertexShader	= compile VERTEXSHADER			VS_PedTransformD();
			PixelShader		= compile PIXELSHADER			PS_DeferredPedTextured_SubSampleAlpha()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	#else //PED_HAIR_LONG...
		pass p0
		{
			PED_DECAL_RENDERSTATES()

			VertexShader	= compile VERTEXSHADER			VS_PedTransformD();
			PixelShader		= compile PIXELSHADER			PS_DeferredPedTextured_SubSampleAlpha()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	
	#if PED_HAIR_SPIKED
		pass p1	// spiked 1st pass - normal cap with no alpha test
		{
			VertexShader	= compile VERTEXSHADER			VS_PedTransformD();
			PixelShader		= compile PIXELSHADER			PS_DeferredPedTexturedNC()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	#endif
	#endif //!PED_HAIR_LONG...
	}
#endif // SSTAA && RSG_PC...
	technique SSTA_TECHNIQUE(deferredsubsamplealpha_draw)
	{
	#if PED_HAIR_LONG
		// long hair: pass0 w/no alphaClipping:
		pass p0
		{
			VertexShader	= compile VERTEXSHADER			VS_PedTransformD();
			PixelShader		= compile PS_GBUFFER_COVERAGE	PS_DeferredPedTexturedC()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
		// long hair: pass1 w/alphaClipping:
		pass p1
		{
			VertexShader	= compile VERTEXSHADER			VS_PedTransformD();
			PixelShader		= compile PS_GBUFFER_COVERAGE	PS_DeferredPedTextured_SubSampleAlpha_SSTA()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	#else //PED_HAIR_LONG...
		pass p0
		{
			PED_DECAL_RENDERSTATES()

			VertexShader	= compile VERTEXSHADER			VS_PedTransformD();
			PixelShader		= compile PS_GBUFFER_COVERAGE	PS_DeferredPedTextured_SubSampleAlpha_SSTA()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	
	#if PED_HAIR_SPIKED
		pass p1	// spiked 1st pass - normal cap with no alpha test
		{
			VertexShader	= compile VERTEXSHADER			VS_PedTransformD();
			PixelShader		= compile PS_GBUFFER_COVERAGE	PS_DeferredPedTexturedC()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	#endif
	#endif //!PED_HAIR_LONG...
	}
	#endif // DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES && NONSKINNED_PED_TECHNIQUES

	#if DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES && DRAWSKINNED_TECHNIQUES
#if SSTAA && RSG_PC
	technique deferredsubsamplealpha_drawskinned
	{
	#if PED_HAIR_LONG
		// long hair: pass0 w/no alphaClipping:
		pass p0
		{
			VertexShader	= compile VERTEXSHADER				VS_PedTransformD();
			PixelShader		= compile PIXELSHADER				PS_DeferredPedTexturedNC()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
		// long hair: pass1 w/alphaClipping:
		pass p1
		{
			VertexShader	= compile VERTEXSHADER				VS_PedTransformD();
			PixelShader		= compile PIXELSHADER				PS_DeferredPedTextured_SubSampleAlpha()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	#else //PED_HAIR_LONG...
		pass p0
		{
			PED_DECAL_RENDERSTATES()

			VertexShader	= compile VERTEXSHADER				VS_PedTransformSkinD();
			PixelShader		= compile PIXELSHADER				PS_DeferredPedTextured_SubSampleAlpha()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}

	#if PED_HAIR_SPIKED
		pass p1	// spiked 1st pass - normal cap with no alpha test
		{
			VertexShader	= compile VERTEXSHADER				VS_PedTransformSkinD();
			PixelShader		= compile PIXELSHADER				PS_DeferredPedTexturedNC()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	#endif
	#endif //!PED_HAIR_LONG...
	}
#endif //SSTAA && RSG_PC...
	technique SSTA_TECHNIQUE(deferredsubsamplealpha_drawskinned)
	{
	#if PED_HAIR_LONG
		// long hair: pass0 w/no alphaClipping:
		pass p0
		{
			VertexShader	= compile VERTEXSHADER				VS_PedTransformD();
			PixelShader		= compile PS_GBUFFER_COVERAGE		PS_DeferredPedTexturedC()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
		// long hair: pass1 w/alphaClipping:
		pass p1
		{
			VertexShader	= compile VERTEXSHADER				VS_PedTransformD();
			PixelShader		= compile PS_GBUFFER_COVERAGE		PS_DeferredPedTextured_SubSampleAlpha_SSTA()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	#else //PED_HAIR_LONG...
		pass p0
		{
			PED_DECAL_RENDERSTATES()

			VertexShader	= compile VERTEXSHADER				VS_PedTransformSkinD();
			PixelShader		= compile PS_GBUFFER_COVERAGE		PS_DeferredPedTextured_SubSampleAlpha_SSTA()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}

	#if PED_HAIR_SPIKED
		pass p1	// spiked 1st pass - normal cap with no alpha test
		{
			VertexShader	= compile VERTEXSHADER				VS_PedTransformSkinD();
			PixelShader		= compile PS_GBUFFER_COVERAGE		PS_DeferredPedTexturedC()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	#endif
	#endif //!PED_HAIR_LONG...
	}
	#endif // DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES && DRAWSKINNED_TECHNIQUES

	#if DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES && NONSKINNED_PED_TECHNIQUES && HAIR_TINT_TECHNIQUES
#if SSTAA && RSG_PC
	technique deferredsubsamplealphatint_draw
	{
#if PED_HAIR_LONG
		// long hair: pass0 w/no alphaClipping:
		pass p0
		{
			VertexShader	= compile VERTEXSHADER			VS_PedTransformD();
			PixelShader		= compile PIXELSHADER			PS_DeferredPedTexturedNC_Tint()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
		// long hair: pass1 w/alphaClipping:
		pass p1
		{
			VertexShader	= compile VERTEXSHADER			VS_PedTransformD();
			PixelShader		= compile PIXELSHADER			PS_DeferredPedTextured_SubSampleAlpha_Tint()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
#else //PED_HAIR_LONG...
		pass p0
		{
			PED_DECAL_RENDERSTATES()

			VertexShader	= compile VERTEXSHADER			VS_PedTransformD();
			PixelShader		= compile PIXELSHADER			PS_DeferredPedTextured_SubSampleAlpha_Tint()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	
	#if PED_HAIR_SPIKED
		pass p1	// spiked 1st pass - normal cap with no alpha test
		{
			VertexShader	= compile VERTEXSHADER			VS_PedTransformD();
			PixelShader		= compile PIXELSHADER			PS_DeferredPedTexturedNC_Tint()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	#endif
#endif //!PED_HAIR_LONG...
	}
#endif //SSTAA && RSG_PC...
	technique SSTA_TECHNIQUE(deferredsubsamplealphatint_draw)
	{
#if PED_HAIR_LONG
		// long hair: pass0 w/no alphaClipping:
		pass p0
		{
			VertexShader	= compile VERTEXSHADER			VS_PedTransformD();
			PixelShader		= compile PS_GBUFFER_COVERAGE	PS_DeferredPedTexturedC_Tint()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
		// long hair: pass1 w/alphaClipping:
		pass p1
		{
			VertexShader	= compile VERTEXSHADER			VS_PedTransformD();
			PixelShader		= compile PIXELSHADER			PS_DeferredPedTextured_SubSampleAlpha_Tint()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
#else //PED_HAIR_LONG...
		pass p0
		{
			PED_DECAL_RENDERSTATES()

			VertexShader	= compile VERTEXSHADER			VS_PedTransformD();
			PixelShader		= compile PIXELSHADER			PS_DeferredPedTextured_SubSampleAlpha_Tint()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	
	#if PED_HAIR_SPIKED
		pass p1	// spiked 1st pass - normal cap with no alpha test
		{
			VertexShader	= compile VERTEXSHADER			VS_PedTransformD();
			PixelShader		= compile PS_GBUFFER_COVERAGE	PS_DeferredPedTexturedC_Tint()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	#endif
#endif //!PED_HAIR_LONG...
	}
	#endif // DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES && NONSKINNED_PED_TECHNIQUES && HAIR_TINT_TECHNIQUES

	#if DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES && DRAWSKINNED_TECHNIQUES && HAIR_TINT_TECHNIQUES
#if SSTAA && RSG_PC
	technique deferredsubsamplealphatint_drawskinned
	{
#if PED_HAIR_LONG
		// long hair: pass0 w/no alphaClipping:
		pass p0
		{
			VertexShader	= compile VERTEXSHADER				VS_PedTransformD();
			PixelShader		= compile PIXELSHADER				PS_DeferredPedTexturedNC_Tint()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
		// long hair: pass1 w/alphaClipping:
		pass p1
		{
			VertexShader	= compile VERTEXSHADER				VS_PedTransformD();
			PixelShader		= compile PIXELSHADER				PS_DeferredPedTextured_SubSampleAlpha_Tint()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
#else //PED_HAIR_LONG...
		pass p0
		{
			PED_DECAL_RENDERSTATES()

			VertexShader	= compile VERTEXSHADER				VS_PedTransformSkinD();
			PixelShader		= compile PIXELSHADER				PS_DeferredPedTextured_SubSampleAlpha_Tint()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}

	#if PED_HAIR_SPIKED
		pass p1	// spiked 1st pass - normal cap with no alpha test
		{
			VertexShader	= compile VERTEXSHADER				VS_PedTransformSkinD();
			PixelShader		= compile PIXELSHADER				PS_DeferredPedTexturedNC_Tint()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	#endif
#endif //!PED_HAIR_LONG...
	}
#endif //SSTAA && RSG_PC...
	technique SSTA_TECHNIQUE(deferredsubsamplealphatint_drawskinned)
	{
#if PED_HAIR_LONG
		// long hair: pass0 w/no alphaClipping:
		pass p0
		{
			VertexShader	= compile VERTEXSHADER				VS_PedTransformD();
			PixelShader		= compile PS_GBUFFER_COVERAGE		PS_DeferredPedTexturedC_Tint()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
		// long hair: pass1 w/alphaClipping:
		pass p1
		{
			VertexShader	= compile VERTEXSHADER				VS_PedTransformD();
			PixelShader		= compile PIXELSHADER				PS_DeferredPedTextured_SubSampleAlpha_Tint()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
#else //PED_HAIR_LONG...
		pass p0
		{
			PED_DECAL_RENDERSTATES()

			VertexShader	= compile VERTEXSHADER				VS_PedTransformSkinD();
			PixelShader		= compile PIXELSHADER				PS_DeferredPedTextured_SubSampleAlpha_Tint()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}

	#if PED_HAIR_SPIKED
		pass p1	// spiked 1st pass - normal cap with no alpha test
		{
			VertexShader	= compile VERTEXSHADER				VS_PedTransformSkinD();
			PixelShader		= compile PS_GBUFFER_COVERAGE		PS_DeferredPedTexturedC_Tint()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	#endif
#endif //!PED_HAIR_LONG...
	}
	#endif // DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES && DRAWSKINNED_TECHNIQUES && HAIR_TINT_TECHNIQUES

	#if DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES && NONSKINNED_PED_TECHNIQUES
	technique deferredsubsamplewritealpha_draw
	{
		// preliminary pass to write down alpha
		pass p0
		{
			#if __XENON
				AlphaToMaskEnable		= false;
			#endif
			DEFERRED_SUBSAMPLEWRITEALPHA_RENDERSTATES_TRANSPARENT()
			VertexShader				= compile VERTEXSHADER	VS_PedTransformD();
			PixelShader					= compile PIXELSHADER	PS_DeferredPedTextured_AlphaClipAlphaOnly()  CGC_FLAGS(CGC_DEFAULTFLAGS_HALF);
		}
	}
	#endif // DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES && NONSKINNED_PED_TECHNIQUES

	#if DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES && DRAWSKINNED_TECHNIQUES
	technique deferredsubsamplewritealpha_drawskinned
	{
		// preliminary pass to write down alpha
		pass p0
		{
			#if __XENON
				AlphaToMaskEnable		= false;
			#endif	
			DEFERRED_SUBSAMPLEWRITEALPHA_RENDERSTATES_TRANSPARENT()
			VertexShader				= compile VERTEXSHADER	VS_PedTransformSkinD();
			PixelShader					= compile PIXELSHADER	PS_DeferredPedTextured_AlphaClipAlphaOnly()  CGC_FLAGS(CGC_DEFAULTFLAGS_HALF);
		}	
	}
	#endif // DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES && DRAWSKINNED_TECHNIQUES

#ifdef RAGE_ENABLE_OFFSET
	// ===============================
	// blendshape techniques
	// ===============================
	#if NONSKINNED_PED_TECHNIQUES
	technique bs_draw
	{
		pass p0
		{   
			AlphaTestEnable	= true;
			AlphaFunc		= GREATER;
			AlphaRef		= 100;
			AlphaBlendEnable= true;
			BlendOp         = Add;
			SrcBlend        = SrcAlpha;
			DestBlend       = InvSrcAlpha;
			
			VertexShader	= compile VERTEXSHADER	VS_PedTransformBS();
			PixelShader		= compile PIXELSHADER	PS_PedTextured_Eight()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif // NONSKINNED_PED_TECHNIQUES

	#if DRAWSKINNED_TECHNIQUES
	technique bs_drawskinned
	{
		pass p0
		{
			AlphaTestEnable = true;
			AlphaFunc		= GREATER;
			AlphaRef		= 100;
			AlphaBlendEnable= true;
			BlendOp         = Add;
			SrcBlend        = SrcAlpha;
			DestBlend       = InvSrcAlpha;
			
			VertexShader	= compile VERTEXSHADER	VS_PedTransformSkinBS();
			PixelShader		= compile PIXELSHADER	PS_PedTextured_Eight()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif // DRAWSKINNED_TECHNIQUES

	#if DEFERRED_TECHNIQUES && NON_CUTOUT_DEFERRED_TECHNIQUES && NONSKINNED_PED_TECHNIQUES
#if RSG_PC && SSTAA
	technique deferredbs_draw
	{
		pass p0
		{
			AlphaTestEnable		= true;
			AlphaFunc			= GREATER;
			AlphaRef			= 100;
			AlphaBlendEnable	= false;

			VertexShader	= compile VERTEXSHADER	VS_PedTransformDBS();
			PixelShader		= compile PIXELSHADER	PS_DeferredPedTexturedNC()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
#endif // RSG_PC && SSTAA

	technique SSTA_TECHNIQUE(deferredbs_draw)
	{
		pass p0
		{
			#if __PS3
				AlphaTestEnable	= false;	// PS3: AlphaTest is prohibited while in MRT mode
			#else
				AlphaTestEnable	= true;
			#endif
			AlphaFunc			= GREATER;
			AlphaRef			= 100;
			AlphaBlendEnable	= false;

			VertexShader	= compile VERTEXSHADER			VS_PedTransformDBS();
			PixelShader		= compile PS_GBUFFER_COVERAGE	PS_DeferredPedTexturedC()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif // DEFERRED_TECHNIQUES && NON_CUTOUT_DEFERRED_TECHNIQUES && NONSKINNED_PED_TECHNIQUES

	#if DEFERRED_TECHNIQUES && NON_CUTOUT_DEFERRED_TECHNIQUES && DRAWSKINNED_TECHNIQUES
#if RSG_PC && SSTAA
	technique deferredbs_drawskinned
	{
		pass p0
		{
			AlphaTestEnable		= true;
			AlphaFunc			= GREATER;
			AlphaRef			= 100;
			AlphaBlendEnable	= false;

			VertexShader	= compile VERTEXSHADER	VS_PedTransformSkinDBS();
			PixelShader		= compile PIXELSHADER	PS_DeferredPedTexturedNC()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
#endif // RSG_PC && SSTAA

	technique SSTA_TECHNIQUE(deferredbs_drawskinned)
	{
		pass p0
		{
			#if __PS3
				AlphaTestEnable	= false;	// PS3: AlphaTest is prohibited while in MRT mode
			#else
				AlphaTestEnable = true;
			#endif
			AlphaFunc			= GREATER;
			AlphaRef			= 100;
			AlphaBlendEnable	= false;

			VertexShader	= compile VERTEXSHADER			VS_PedTransformSkinDBS();
			PixelShader		= compile PS_GBUFFER_COVERAGE	PS_DeferredPedTexturedC()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif // DEFERRED_TECHNIQUES && NON_CUTOUT_DEFERRED_TECHNIQUES && DRAWSKINNED_TECHNIQUES
#endif //RAGE_ENABLE_OFFSET

#endif //__MAX...
#endif //TECHNIQUES...

#if !__MAX
#if PED_CLOTH && (__XENON || DX11_CLOTH)
	#define LOCAL_SHADOW_SKIN_VS VS_LinearDepthSkinCloth
#else
	#define LOCAL_SHADOW_SKIN_VS VS_LinearDepthSkin
#endif
#if NONSKINNED_PED_TECHNIQUES
	SHADERTECHNIQUE_CASCADE_SHADOWS()
	SHADERTECHNIQUE_LOCAL_SHADOWS(VS_LinearDepth, LOCAL_SHADOW_SKIN_VS, PS_LinearDepth)
	SHADERTECHNIQUE_ENTITY_SELECT(VS_PedTransform(), VS_PedTransformSkin())
#else
	#if DX11_CLOTH
	technique wdcascade_drawskinned
	{
		pass p0
		{
			VertexShader = compile VERTEXSHADER	VS_CascadeShadows_PedTransformSkinD();
			COMPILE_PIXELSHADER_NULL()
		}
	}
		#if GS_INSTANCED_SHADOWS
			GEN_GSINST_TYPE(WDSkinned,VS_CascadeShadows_OUT,SV_ViewportArrayIndex)
			GEN_GSINST_FUNC_NOPS(WDSkinned,WDSkinned,northSkinVertexInputBump,VS_CascadeShadows_OUT,VS_CascadeShadows_OUT,VS_CascadeShadows_PedTransformSkinD_Common(IN,true))

			technique wdcascade_drawskinnedinstanced
			{
				pass p0
				{
					VertexShader = compile VSGS_SHADER	JOIN3(VS_,WDSkinned,TransformGSInst)();
					SetGeometryShader(compileshader(gs_5_0, JOIN3(GS_,WDSkinned,GSInstPassThrough)()));
					COMPILE_PIXELSHADER_NULL()
				}
			}
		#endif
	#else //DX11_CLOTH
		SHADERTECHNIQUE_CASCADE_SHADOWS_SKINNED_ONLY()
	#endif //DX11_CLOTH
		SHADERTECHNIQUE_LOCAL_SHADOWS_SKINNED_ONLY(LOCAL_SHADOW_SKIN_VS, PS_LinearDepth)
		SHADERTECHNIQUE_ENTITY_SELECT_SKINNED_ONLY(VS_PedTransformSkin())
#endif //NONSKINNED_PED_TECHNIQUES...
#if PED_TRANSFORM_WORLDPOS_OUT
	#include "../Debug/debug_overlay_ped.fxh"
#endif
#endif	//!__MAX...

#endif//__GTA_PED_COMMON_FXH__...

