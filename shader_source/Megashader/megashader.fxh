// GTA_TOOLSET
//	This is designed to be a common header file to be used to handle some of the more
//	elaborate but commonly used shader effects.  Teams can feel free to use this.
//	Below, are a list of #defines that may or may not be set in order to control the
//	functionality and API of these functions.  IF YOU ADD MORE TO THIS FILE, MAKE SURE
//	YOU DESCRIBE THEM HERE
//
//		USE_NORMAL_MAP
//			Process appropriate tangent space vectors in vertex shader, and pull normal info from
//			a texture.
//		USE_SPECULAR
//			Factor in light specularity
//			USE_SPECULAR_UV1
//				Specular UV coordinates come from texcoord1
//			IGNORE_SPECULAR_MAP
//				Do not use a specular texture when computing specular strength
//		USE_REFLECT
//			Enable reflections
//			USE_SPHERICAL_REFLECT
//				Use a spherical reflection model instead of a cubic reflection model
//			USE_DYNAMIC_REFLECT
//				Use the dynamic reflection model (cube -> paraboloid)
//		USE_EMISSIVE
//			Enable CPV's to be treated as "light sources" instead of material colors
//		USE_DIFFUSE2
//			Enable a second texture stage channel
//			USE_DIFFUSE2_SPECULAR_MASK
//				Scale down specularity component in relation to inv. alpha channel
//
//		USE_DEFAULT_TECHNIQUES
//			Use the techniques in this shader versus rolling your own.
//
//		NO_SKINNING
//			Do not use skinning
//
//		NO_DIFFUSE_TEXTURE
//			disable the diffuse texture
//
//		SHADOW_ENABLED
//			Use warp shadow map
//
//      VEHICLE_DAMAGE_SCALE
//          Override where the vehicle damage scale comes from.  Defaults to inCpv.g.
//
//
// There are a lot of #if tests in this code, but it greatly reduces code duplication
//	for a number of shaders.
#ifndef __GTA_MEGASHADER_FXH__
#define __GTA_MEGASHADER_FXH__

#ifdef NO_DIFFUSE_TEXTURE
	#define DIFFUSE_TEXTURE	0
#else
	#define DIFFUSE_TEXTURE	1
#endif

#if DIFFUSE_TEXTURE
	#define  USE_DIFFUSE_SAMPLER
#endif

#ifndef VEHICLE_DAMAGE_SCALE
	#define VEHICLE_DAMAGE_SCALE inCpv.g
#endif

#if !defined MEGASHADER_PN_TRIANGLE_OVERRIDE

// Configure PN triangle code.
#define PN_TRIANGLE_TESSELLATION

#ifdef USE_DISPLACEMENT_MAP_NOTFOUND
	// we don't want a fixed tessellation factors on trees and other DPM objects
	#define PN_TRIANGLE_FIXED_TESSELLATION_FACTOR 1
#else
	#define PN_TRIANGLE_SCREEN_SPACE_ERROR_TESSELLATION_FACTOR 1
	#define PN_TRIANGLES_USE_EDGES_AND_MIDPOINT 1
#endif

#endif //!defined MEGASHADER_PN_TRIANGLE_OVERRIDE

#ifdef USE_EDGE_WEIGHT
	#define EDGE_WEIGHT					(1)
	//the way we have the data setup at the moment there's some extra data before the
	//the edge weights, so we have to have an extra set of texture coord defined or
	//it won't be picked up in the shader correctly, it's possible we can remove this in future
#ifdef SUPRESS_PADDING
	#define EDGE_WEIGHT_PAD				(0) 
#else
	#define EDGE_WEIGHT_PAD				(1)
#endif

#else
	#define EDGE_WEIGHT					(0)
	#define EDGE_WEIGHT_PAD				(0)
#endif

#if __MAX || __LOW_QUALITY
	#undef	USE_FOGRAY_FORWARDPASS
#else
	#define USE_FOGRAY_FORWARDPASS
#endif

#include "../common.fxh"
#include "../Util/skin.fxh"
#include "../PostFX/SeeThroughFX.fxh"
#include "../Util/GSInst.fxh"


#ifndef NO_FOG
#define USE_FOG							// always use fog by default
#endif

// All Max shaders that aren't reflective enable reflection so they look more like
// in-game versions of the shader
#if __MAX && defined(USE_SPECULAR) && !defined(USE_REFLECT)
	#define USE_REFLECT
	#define USE_DYNAMIC_REFLECT
#endif

// -----------------------------------------

#ifdef USE_PROJTEX_ZSHIFT_SUPPORT
	#define PROJTEX_ZSHIFT_SUPPORT		(1)	
#else
	#define PROJTEX_ZSHIFT_SUPPORT		(0)
#endif

#ifdef USE_ALPHA_CLIP
	#define ALPHA_CLIP					(1)
#else
	#define ALPHA_CLIP					(0)
#endif

#ifdef USE_WETNESS_MULTIPLIER
	#define WETNESS_MULTIPLIER						(1)
#else
	#define WETNESS_MULTIPLIER						(0)
#endif

#ifdef USE_CLOTH
	#define CLOTH						(1)
#else
	#define CLOTH						(0)
#endif

#if defined(USE_PARALLAX_MAP) && !__LOW_QUALITY
	// make sure normal map is defined when doing parallax mapping:
	#ifndef USE_NORMAL_MAP
		#error "NORMAL_MAP is required for PARALLAX!"
	#endif
	#define PARALLAX							(1)
	#define PARALLAX_BUMPSHIFT					(1)	// shift bump map (not only diffuse map)

	#ifdef USE_PARALLAX_CLAMP					// force procedural clamping on UVs for diffuse&bump map
		#define PARALLAX_CLAMP					(1)
	#else
		#define PARALLAX_CLAMP					(0)
	#endif

	#ifdef USE_PARALLAX_REVERTED				// use reverted parallax mode
		#define PARALLAX_REVERTED				(1)
	#else
		#define PARALLAX_REVERTED				(0)
	#endif

	#ifdef USE_PARALLAX_STEEP
		#define PARALLAX_STEEP					(1)
	#else
		#define PARALLAX_STEEP					(0)
	#endif
#else
	#define PARALLAX							(0)
#endif// USE_PARALLAX_MAP...

#if defined(USE_PARALLAX_MAP_V2) && !__LOW_QUALITY
	#define PARALLAX_MAP_V2						(1)
#else
	#define PARALLAX_MAP_V2						(0)
#endif

#ifdef USE_NORMAL_MAP
	#define NORMAL_MAP						(1)
#else
	#define NORMAL_MAP						(0)
#endif	// USE_NORMAL_MAP

#ifdef USE_SPECULAR
	#define SPECULAR						(1)
	
	#ifdef IGNORE_SPECULAR_MAP
		#define SPEC_MAP					(0)
	#else
		#define SPEC_MAP					(1)
	#endif	// IGNORE_SPECULAR_MAP
	#ifdef USE_INV_DIFFUSE_AS_SPECULAR_MAP
		#define INV_DIFFUSE_AS_SPECULAR_MAP	(1)
	#else
		#define INV_DIFFUSE_AS_SPECULAR_MAP	(0)
	#endif
	#ifdef USE_SPECULAR_UV1
		#define SPECULAR_PS_INPUT			IN.texCoord.zw
		#define SPECULAR_VS_INPUT			IN.texCoord1.xy
		#define SPECULAR_VS_OUTPUT			OUT.texCoord.zw
		#define SPECULAR_UV1				(1)
	#else
		#define SPECULAR_PS_INPUT			IN.texCoord.xy
		#define SPECULAR_VS_INPUT			IN.texCoord0.xy
		#define SPECULAR_VS_OUTPUT			OUT.texCoord.xy
		#define SPECULAR_UV1				(0)
	#endif	// USE_SPECULAR_UV1
	#ifdef USE_SPEC_MAP_INTFALLOFF_PACK
		#define SPEC_MAP_INTFALLOFF_PACK	(1)
	#else
		#define SPEC_MAP_INTFALLOFF_PACK	(0)
	#endif

	// #define USE_DIFFUSE_AS_SPEC
	#ifdef USE_DIFFUSE_AS_SPEC
		#define SPECULAR_DIFFUSE			(1)
	#else
		#define SPECULAR_DIFFUSE			(0)
	#endif // USE_DIFFUSE_AS_SPEC
#else
	#define SPECULAR						(0)
	#define SPEC_MAP						(0)
	#define SPEC_MAP_INTFALLOFF_PACK		(0)
#endif		// USE_SPECULAR

#ifdef USE_SECOND_SPECULAR_LAYER
	#define SECOND_SPECULAR_LAYER			(SPECULAR)			// second specular layer is not valid without specular
#else
	#define SECOND_SPECULAR_LAYER			(0)
#endif	//USE_SECOND_SPECULAR_LAYER...

#ifdef USE_REFLECT
	#define REFLECT							(1)
	#ifdef USE_SPHERICAL_REFLECT
		#define REFLECT_SAMPLER				sampler2D
		#define REFLECT_TEXOP				tex2D
		#define REFLECT_SPHERICAL			(1)
		#define REFLECT_MIRROR				(0)
		#define REFLECT_DYNAMIC				(0)
		#define REFLECT_TEX_SAMPLER			EnvironmentSampler
	#elif defined(USE_MIRROR_REFLECT)
		#define REFLECT_SAMPLER				sampler2D
		#define REFLECT_TEXOP				tex2D
		#define REFLECT_SPHERICAL			(0)
		#define REFLECT_MIRROR				(1)
		#define REFLECT_DYNAMIC				(0)
		#if __PS3
			#define REFLECT_TEX_SAMPLER		MirrorReflectionSampler
		#else
			#define REFLECT_TEX_SAMPLER		ReflectionSampler
		#endif
	#elif defined(USE_DYNAMIC_REFLECT)
		#define REFLECT_SAMPLER				sampler2D
		#define REFLECT_TEXOP				tex2D
		#define REFLECT_SPHERICAL			(0)
		#define REFLECT_MIRROR				(0)
		#define REFLECT_DYNAMIC				(1)
		#define REFLECT_TEX_SAMPLER			ReflectionSampler
	#else
		#define REFLECT_SAMPLER				samplerCUBE
		#define REFLECT_TEXOP				texCUBE
		#define REFLECT_SPHERICAL			(0)
		#define REFLECT_MIRROR				(0)
		#define REFLECT_DYNAMIC				(0)
		#define REFLECT_TEX_SAMPLER			EnvironmentSampler
	#endif
#else
	#define REFLECT							(0)
#endif	// USE_REFLECT

#ifdef USE_EMISSIVE_ADDITIVE
	#define USE_EMISSIVE
	#define	EMISSIVE_ADDITIVE				(1)
#else
	#define EMISSIVE_ADDITIVE				(0)
#endif

#ifdef USE_EMISSIVE
	#define EMISSIVE						(1)
#else
	#define EMISSIVE						(0)
#endif	// USE_EMISSIVE

#ifdef USE_EMISSIVE_NIGHTONLY
	#define EMISSIVE_NIGHTONLY				(1)
	#if !EMISSIVE
		#error "EMISSIVE_NIGHTONLY requested, but EMISSIVE variable not enabled!"
	#endif
#else
	#define EMISSIVE_NIGHTONLY				(0)
#endif

#ifdef USE_EMISSIVE_NIGHTONLY_FOLLOW_VERTEX_SCALE
	#define EMISSIVE_NIGHTONLY_FOLLOW_VERTEX_SCALE				(1)
	#if !EMISSIVE
		#error "EMISSIVE_NIGHTONLY_FOLLOW_VERTEX_SCALE requested, but EMISSIVE variable not enabled!"
	#endif
#else
	#define EMISSIVE_NIGHTONLY_FOLLOW_VERTEX_SCALE				(0)
#endif


#if EMISSIVE_NIGHTONLY_FOLLOW_VERTEX_SCALE && EMISSIVE_NIGHTONLY
	#error "Can`t have EMISSIVE_NIGHTONLY and EMISSIVE_NIGHTONLY_FOLLOW_VERTEX_SCALE
#endif

#ifdef USE_EMISSIVE_CLIP
	#define EMISSIVE_CLIP					(1)
#else
	#define EMISSIVE_CLIP					(0)
#endif

#ifdef USE_DIFFUSE2
	#define DIFFUSE2						(1)
	#ifdef USE_DIFFUSE2_SPECULAR_MASK
		#define DIFFUSE2_SPEC_MASK			(1)
	#else
		#define DIFFUSE2_SPEC_MASK			(0)
	#endif	// USE_DIFFUSE2_SPECULAR_MASK
#else
	#define DIFFUSE2						(0)
	#define DIFFUSE2_SPEC_MASK				(0)
#endif	// USE_DIFFUSE2

#ifdef USE_DIFFUSE_EXTRA
	#define DIFFUSE_EXTRA					(1)
#else 
	#define DIFFUSE_EXTRA					(0)
#endif

#ifdef DIFFUSE_TEXTURE
	#define DIFFUSE_PS_INPUT				IN.texCoord.xy
	#define DIFFUSE_VS_INPUT				IN.texCoord0.xy
	#define DIFFUSE_VS_OUTPUT				OUT.texCoord.xy
#endif

#ifdef NO_SKINNING
	#define SKINNING						(0)
#else
	#define SKINNING						(1)
#endif

#ifdef USE_DEFAULT_TECHNIQUES
	#define TECHNIQUES						(1)
#else
	#define TECHNIQUES						(0)
#endif	// USE_DEFAULT_TECHNIQUES

#ifdef USE_ALPHA_FRESNEL_ADJUST
	#define ALPHA_FRESNEL_ADJUST			(1)
#else
	#define ALPHA_FRESNEL_ADJUST			(0)
#endif

#ifdef USE_GLASS_LIGHTING
	#define GLASS_LIGHTING			        (1)
#else
	#define GLASS_LIGHTING			        (0)
#endif

#ifdef USE_TERRAIN_WETNESS
	#define TERRAIN_WETNESS					(1)
#else
	#define TERRAIN_WETNESS					(0)
#endif

#ifdef USE_COLOR_VARIATION_TEXTURE
	#define COLOR_VARIATION_TEXTURE			(1)
	#define DONT_TRUST_DIFFUSE_ALPHA		(1)
#else
	#define COLOR_VARIATION_TEXTURE			(0)
	#define DONT_TRUST_DIFFUSE_ALPHA		(0)
#endif

#ifdef USE_DONT_TRUST_DIFFUSE_ALPHA 
	#undef DONT_TRUST_DIFFUSE_ALPHA
	#define DONT_TRUST_DIFFUSE_ALPHA		(1)
#endif

#ifdef DISABLE_ALPHA_DOF
#undef APPLY_DOF_TO_ALPHA_DECALS
#define APPLY_DOF_TO_ALPHA_DECALS 0
#endif

#ifdef USE_ALPHA_AS_A_DOF_OUTPUT
	#define ALPHA_AS_A_DOF_OUTPUT 1
#else
	#define ALPHA_AS_A_DOF_OUTPUT 0
#endif

#ifdef USE_GRID_BASED_DISCARDS
	#define GRID_BASED_DISCARDS 1
#else
	#define GRID_BASED_DISCARDS 0
#endif	

#define _USE_FOGGING_PARABOLOID                        (1) // if 0, no fogging will be done whatsoever (for testing)
#define _USE_PERVERTEX_LIGHTING_AND_FOGGING_PARABOLOID (0) // if 0, lighting and fogging will be done in the pixel shader
#define _USE_PERVERTEX_SHADOWING_PARABOLOID            (0 && (__XENON || __PS3))
#define _USE_PERPIXEL_SHADOWING_PARABOLOID             (0 && (__XENON || __PS3))

// -----------------------------------------


#ifndef USE_SINGLE_LAYER_SSA
#define DEFERRED_SUBSAMPLEWRITEALPHA_RENDERSTATES_MEGASHADER DEFERRED_SUBSAMPLEWRITEALPHA_RENDERSTATES_TRANSPARENT
#else
#define DEFERRED_SUBSAMPLEWRITEALPHA_RENDERSTATES_MEGASHADER DEFERRED_SUBSAMPLEWRITEALPHA_RENDERSTATES
#endif

#if SPECULAR
#define FRESNEL (1)
#else
#define FRESNEL (0)
#endif

#ifdef USE_DISPLACEMENT_MAP
	#ifndef PN_TRIANGLE_TESSELLATION
		#error "PN_TRIANGLE_TESSELLATION is required for DISPLACEMENT_MAP!"
	#endif
	#define DISPLACEMENT_MAP			(1)
#else
	#define DISPLACEMENT_MAP			(0)
#endif	// USE_DISPLACEMENT_MAP

#define DISPLACEMENT_DOMINANT_UV	(1 && DISPLACEMENT_MAP)
#define DISPLACEMENT_LOD_ADAPTIVE	(0 && DISPLACEMENT_MAP)
#define DISPLACEMENT_LOD_COVERAGE	(1 && DISPLACEMENT_LOD_ADAPTIVE)
#define DISPLACEMENT_LOD_EDGE_AWARE	(0 && DISPLACEMENT_LOD_ADAPTIVE)
#define DISPLACEMENT_LOD_DEBUG		(0 && DISPLACEMENT_MAP)

#define ENABLE_BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS (TREES_ENABLE_BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS || __MAX) && !__LOW_QUALITY

#ifdef USE_WIND_DISPLACEMENT
	#define WIND_DISPLACEMENT	(1 && ENABLE_BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS)
#else
	#define WIND_DISPLACEMENT	(0)
#endif

#include "../Vegetation/Trees/trees_windfuncs.fxh"


#if BATCH_INSTANCING


#	if (__SHADERMODEL >= 40) && !GRASS_BATCH_CS_CULLING
#		error Megashader batch instancing only works if the batch compute shader is enabled!
#	endif
#	if UMOVEMENTS || WIND_DISPLACEMENT
#		error Megashader batch instancing needs color2, so it will not work when micromovements or wind displacement are enabled!
#	endif
#endif //BATCH_INSTANCING
#define COMPUTE_COMPOSITE_WORLD_MTX (1)


BEGIN_RAGE_CONSTANT_BUFFER(megashader_locals,b8)

#ifdef USE_PROJTEX_ZSHIFT_SUPPORT
float zShiftScale = 0.2f;
#endif //USE_PROJTEX_ZSHIFT_SUPPORT

#ifdef USE_DISPLACEMENT_MAP
	float tessellationMultiplier : TessellationMultiplier
	<
		int nostrip=1;	// dont strip
		string UIName = "Tessellation Mul";
		float UIMin = 0.01f;
		float UIMax = 100.0f;
		float UIStep = 0.1f;
	> = 1.0f;
#else
# define tessellationMultiplier	(1.0)
#endif	//USE_DISPLACEMENT_MAP

#ifdef USE_PARALLAX_MAP
	float parallaxScaleBias : ParallaxScaleBias 
	<
		string UIName = "Parallax ScaleBias";
		float UIMin = 0.01;
		float UIMax = 1.0f;
		float UIStep = 0.01;
	> = 
	#if PARALLAX_STEEP
		0.005f;
	#else
		0.03f;
	#endif
#endif //USE_PARALLAX_MAP

#ifdef DECAL_AMB_ONLY
	float3 ambientDecalMask : AmbientDecalMask
	<
		string UIWidget = "slider";
		float UIMin = 0.0;
		float UIMax = 1.0;
		float UIStep = .01;
		string UIName = "ambient map mask color";
	> = { 1.0, 0.0, 0.0};
#endif

#ifdef DECAL_DIRT
	float3 dirtDecalMask : DirtDecalMask
	<
		int nostrip=1;	// dont strip
		string UIWidget = "slider";
		float UIMin = 0.0;
		float UIMax = 1.0;
		float UIStep = .01;
		string UIName = "dirt map mask color";
	> = { 1.0, 0.0, 0.0};
#endif


#if EMISSIVE || EMISSIVE_ADDITIVE
	float emissiveMultiplier : EmissiveMultiplier
	<
		string UIName = "Emissive HDR Multiplier";
		float UIMin = 0.0;
		float UIMax = 16.0;
		float UIStep = 0.1;
	> = 1.0f;
#endif


#if FRESNEL || defined(USE_SPECULAR_FRESNEL)
	float specularFresnel : Fresnel
	<
		string UIName = "Specular Fresnel";
		float UIMin = 0.0;
		float UIMax = 1.0;
		float UIStep = 0.01;
	> = 0.97f;
#else
	#define specularFresnel (0.97f)
#endif

#if SPECULAR
	float specularFalloffMult : Specular 
	<
		string UIName = "Specular Falloff";
		float UIMin = 0.0;
		float UIMax = 512.0;
		float UIStep = 0.1;
	> = 100.0;

	float specularIntensityMult : SpecularColor
	<
		string UIName = "Specular Intensity";
		float UIMin = 0.0;
		float UIMax = 1.0;
		float UIStep = 0.01;
#ifdef DECAL_DEFAULT_SPEC_ZERO
	> = 0.0;
#else
	> = 0.125;
#endif

	#if SPEC_MAP
		#if SPEC_MAP_INTFALLOFF_PACK
			// no specMapIntMask here
		#else
			float3 specMapIntMask : SpecularMapIntensityMask
			<
				string UIWidget = "slider";
				float UIMin = 0.0;
				float UIMax = 1.0;
				float UIStep = .01;
				string UIName = "specular map intensity mask color";
			> = { 1.0, 0.0, 0.0};
		#endif

		#if SPECULAR_DIFFUSE
			float4 SpecDesaturateIntensity : SpecDesaturateIntensity
			<
				string UIWidget = "slider";
				float UIMin = 0.0;
				float UIMax = 1.0;
				float UIStep = .01;
				string UIName = "Spec Desat Intensity";
			> = { 0.3, 0.6, 0.1, 0.0625f};

			#define SpecBaseIntensity (SpecDesaturateIntensity.w)

			float4 SpecDesaturateExponent : SpecDesaturateExponent
			<
				string UIWidget = "slider";
				float UIMin = 0.0;
				float UIMax = 1.0;
				float UIStep = .01;
				string UIName = "Spec Desat Exponent";
			> = { 0.3, 0.6, 0.1, 0.0625f};

			#define SpecBaseExponent (SpecDesaturateExponent.w)
		#endif // SPECULAR_DIFFUSE
	#endif	// SPEC_MAP
#endif		// SPECULAR

#if SECOND_SPECULAR_LAYER
	float specular2Factor : Specular2Factor
	<
		string UIName = "Specular2 Falloff";
		float UIMin = 0.0;
		float UIMax = 10000.0;
		float UIStep = 0.1;
		WIN32PC_DONTSTRIP
	> = 40.0f;

	float specular2ColorIntensity : Specular2ColorIntensity
	<
		string UIName = "Specular2 Intensity";
		float UIMin = 0.0;
		float UIMax = 10000.0;
		float UIStep = 0.1;
		WIN32PC_DONTSTRIP
	> = 4.7f;

	float3 specular2Color : Specular2Color = float3(1.0f, 1.0f, 1.0f);
#endif //SECOND_SPECULAR_LAYER...

#if NORMAL_MAP
	float bumpiness : Bumpiness
	<
		string UIWidget = "slider";
		float UIMin = 0.0;
		float UIMax = 10.0;
		float UIStep = .01;
		string UIName = "Bumpiness";
	> = 1.0;
#endif // NORMAL_MAP...

#if DISPLACEMENT_MAP || PARALLAX_MAP_V2
	float heightScale : HeightScale
	<
		int nostrip = 1;
		string UIWidget = "slider";
#if PARALLAX_MAP_V2
		float UIMin = 0.01;
		float UIMax = 0.05f;
		float UIStep = 0.001;
#else
		float UIMin = -10.00;
		float UIMax = 10.0f;
		float UIStep = 0.01;
#endif // PARALLAX_MAP_V2
		string UIName = "Height Scale";
#if PARALLAX_MAP_V2
	> = 0.03f;
#else
	> = 0.4f;
#endif // PARALLAX_MAP_V2

	float heightBias : HeightBias
	<
		int nostrip = 1;
		string UIWidget = "slider";
#if PARALLAX_MAP_V2
		float UIMin = 0.00;
		float UIMax = 0.05f;
		float UIStep = 0.001;
#else	
		float UIMin = -10.00;
		float UIMax = 10.0f;
		float UIStep = 0.01;
#endif
		string UIName = "Height Bias";
#if PARALLAX_MAP_V2
	> = 0.015f;
#else
	> = -0.5f;
#endif // PARALLAX_MAP_V2
	
#endif

#if PARALLAX_MAP_V2
	float parallaxSelfShadowAmount : ParallaxSelfShadowAmount
	<
		string UIWidget = "slider";
		float UIMin = 0.01;
		float UIMax = 1.0f;
		float UIStep = 0.001;
		string UIName = "Parallax Self Shadow";
		int nostrip = 1;
	> = 0.95;
#endif //PARALLAX_MAP_V2

#if REFLECT
	#if !REFLECT_DYNAMIC
		float reflectivePower : Reflectivity
		<
			string UIName = "Reflectivity";
			float UIMin = -10.0;
			float UIMax = 100.0;
			float UIStep = 0.1;
		> = 0.45;
	#endif // !REFLECT_DYNAMIC

	#if REFLECT_MIRROR
		#if defined(USE_MIRROR_CRACK_MAP)
			float gMirrorCrackAmount : MirrorCrackAmount
			<
				string UIWidget = "slider";
				float  UIMin    =-32.00;
				float  UIMax    = 32.00;
				float  UIStep   =  0.01;
				string UIName   = "Crack Amount";
			> = 8.0;
		#endif // defined(USE_MIRROR_CRACK_MAP)
		#if defined(USE_MIRROR_DISTORTION)
			float gMirrorDistortionAmount : MirrorDistortionAmount
			<
				string UIWidget = "slider";
				float  UIMin    =-512.00;
				float  UIMax    = 512.00;
				float  UIStep   =   0.01;
				string UIName   = "Distortion Amount";
			> = 32.0;
		#endif // defined(USE_MIRROR_DISTORTION)
	#endif // REFLECT_MIRROR
#endif // REFLECT

#if SPECULAR
	#if DIFFUSE2
		#if DIFFUSE2_SPEC_MASK
			float diffuse2SpecClamp : Diffuse2MinSpec
			<
				string UIName="Texture2 Min Specular";
				string UIHelp="Minimum amount of specular allowed for secondary texture";
				float UIMin = 0.0;
				float UIMax = 10.0;
				float UIStep = 0.01;
			> = 0.2;
		#else
			float diffuse2SpecMod : Diffuse2ModSpec
			<
				string UIName="Texture2 Specular Modifier";
				string UIHelp="Amount of specular power added by alpha of secondary texture";
				float UIMin = 0.0;
				float UIMax = 1000.0;
				float UIStep = 0.1;
			> = 0.8;
		#endif // DIFFUSE_SPEC_MASK
	#endif	// DIFFUSE2
#endif // SPECULAR

#if WETNESS_MULTIPLIER
	float wetnessMultiplier : WetnessMultiplier
	<
		string UIWidget = "slider";
		float UIMin = 0.0;
		float UIMax = 1.0;
		float UIStep = 0.005;
		string      UIName = "Wetness multiplier";
		#if __WIN32PC
		int nostrip	= 1;
		#endif
	> = 1.0;
#endif

#if __XENON
	// Use this for forward 4 lights pixel shaders that fail to compile due to high register count
	#if defined ISOLATE_4
		#define MIN_LIGHT_ISOLATE (0)
	#elif defined ISOLATE_8
		#define MIN_LIGHT_ISOLATE (4)
	#else
		#define MIN_LIGHT_ISOLATE (8)
	#endif
#endif // __XENON
	
#if defined(DISPLACEMENT_SHADER)
//
//
//
//
float3 displParams : displacementParams
<
	string UIWidget = "slider";
	float UIMin = 0.0;
	float UIMax = 64.0;
	float UIStep = .01;
	string UIName = "Displacament: scaleU, scaleV, camDist";
> = { 16.0, 16.0, 15.0};

#endif //DISPLACEMENT_SHADER...

// __WIN32PC is for the win32_30 shaders we build x86 resources against.
#if __MAX || RAGE_SUPPORT_TESSELLATION_TECHNIQUES || __WIN32PC
	float useTessellation : UseTessellation
#if DISPLACEMENT_MAP
	<
		int nostrip	= 1;
	> = 1.0f; //we need this turned on so we can do displacement
#else
	<
		string UIName = "Use tessellation";
		float UIMin = 0.0;
		float UIMax = 1.0;
		float UIStep = 1.0;
		int nostrip	= 1;
	> = 0.0f;
#endif //DISPLACEMENT_MAP
#endif //__MAX || RAGE_SUPPORT_TESSELLATION_TECHNIQUES || __WIN32PC


float HardAlphaBlend : HardAlphaBlend 
<
	string UIName = "Hard Alpha";
	float UIMin = 0.0;
	float UIMax = 1.f;
	float UIStep = 0.01f;
> = 1.0f;

#if defined(USE_SEETHROUGH_TECHNIQUE)
float4 matMaterialColorScale : MaterialColorScale = float4(1,0,0,1); // diffuse, emissive, reflection, heat
#endif

#if REFLECT
#if REFLECT_MIRROR
	float4 gMirrorBounds : MirrorBounds;
	float4 gMirrorDebugParams : MirrorDebugParams;
#endif
#endif

#if WIND_DISPLACEMENT

float4 branchBendPivot : branchBendPivot0 
<
	string UIWidget	= "slider";
	string UIName	= "branch bend pivot";
	float UIMin		= -3.0;
	float UIMax		= 200.0;
	float UIStep	= 0.01;
> = float4(3.150f, 0.0f, 0.0f, 0.0f);

float4 branchBendStiffnessAdjust : branchBendStiffnessAdjust0 
<
	string UIWidget	= "slider";
	string UIName	= "stiffness adjust";
	float UIMin		= 0.0;
	float UIMax		= 50.0;
	float UIStep	= 0.01;
> = float4(0.0f, 1.0f, 0.0f, 1.0f);

#endif // WIND_DISPLACEMENT

#if COLOR_VARIATION_TEXTURE
	float paletteSelector : DiffuseTexPaletteSelector = 0.0f;
#endif

#if BATCH_INSTANCING
float3	vecBatchAabbMin : vecBatchAabbMin0 = float3(0,0,0); // xyz = batch aabb min. Offset pos is based of this.
float3	vecBatchAabbDelta : vecBatchAabbDelta0 = float3(1,1,1); // xyz = batch aabb max - min. Offset pos is based of this.
float gOrientToTerrain : gOrientToTerrain = 0.0f;
float3 gScaleRange : gScaleRange = float3(1.0f, 1.0f, 0.0f);

//CS params?
#if __SHADERMODEL >= 40 //Help with SM30
float3 gLodFadeInstRange : gLodFadeInstRange = float3(0.0f, 0.0f, 1.0f);

uint gUseComputeShaderOutputBuffer = 0;
float2 gInstCullParams = float2(0.0f, 1.0f); //Frustum culling uses these values.
#define gInstAabbRadius (gInstCullParams.x)
#define gClipPlaneNormalFlip (gInstCullParams.y)
uint gNumClipPlanes : gNumClipPlanes = 4;
float4 gClipPlanes[CASCADE_SHADOWS_SCAN_NUM_PLANES] : gClipPlanes;

//LOD Transitions
float3 gCameraPosition; 		//When batched, CS run before the viewport is setup!
uint gLodInstantTransition = 0;	//(LODFlag != LODDrawable::LODFLAG_NONE || isShadowDrawList)
float4 gLodThresholds = -1.0f;	//globalScale already pre-multiplied in.
float2 gCrossFadeDistance;		//x = g_crossFadeDistance[0], y = g_crossFadeDistance[distIdx]

uint gIsShadowPass = 0;
float3 gLodFadeControlRange = float3(0.0f, (float)0x1FFF, 1.0f);
#endif //__SHADERMODEL >= 40

//LOD Fade
float2 gLodFadeStartDist : gLodFadeStartDist
<
	string UIName = "LOD Fade Start Dist [Normal|Shadows]";
	float UIMin = -1.0;
	float UIMax = FLT_MAX / 1000.0;
	float UIStep = 0.1;
	int nostrip	= 1;
> = float2(-1.0f, -1.0f);

float2 gLodFadeRange : gLodFadeRange
<
	string UIName = "LOD Fade Range [Normal|Shadows]";
	float UIMin = 0.0;
	float UIMax = 1.0;
	float UIStep = 0.01;
	int nostrip	= 1;
> = float2(0.05f, 0.05f);

float2 gLodFadePower : gLodFadePower
<
	string UIName = "LOD Fade Power [Normal|Shadows]";
	float UIMin = 0.0;
	float UIMax = FLT_MAX / 1000.0;
	float UIStep = 0.1;
	int nostrip	= 1;
> = float2(1.0f, 1.0f);

float2 gLodFadeTileScale : gLodFadeTileScale
<
	string UIName = "LOD Fade Tile Scale";
	float UIMin = 0.0;
	float UIMax = FLT_MAX / 1000.0;
	float UIStep = 0.1;
	int nostrip	= 1;
> = float2(1.0f, 1.0f);

#define USE_SKIP_INSTANCE 0

#if RSG_DURANGO
uint4 gIndirectCountPerLod = uint4(0, 0, 0, 0);
#endif
#endif //BATCH_INSTANCING

EndConstantBufferDX10(megashader_locals)

#if BATCH_INSTANCING && (__SHADERMODEL >= 40)
BeginConstantBufferPagedDX10(grassbatch_instmtx,PASTE2(b,INSTANCE_CONSTANT_BUFFER_SLOT))
float4 gInstanceVars[GRASS_BATCH_NUM_CONSTS_PER_INSTANCE * GRASS_BATCH_SHARED_DATA_COUNT] REGISTER2(vs, PASTE2(c, INSTANCE_SHADER_CONSTANT_SLOT));
EndConstantBufferDX10(grassbatch_instmtx)

StructuredBuffer<BatchCSInstanceData> InstanceBuffer;// REGISTER2(vs, t0);
StructuredBuffer<BatchRawInstanceData> RawInstanceBuffer;// REGISTER2(cs_5_0, t0);
#endif //BATCH_INSTANCING && (__SHADERMODEL >= 40)

#if REFLECT
	#if REFLECT_MIRROR
		#if __PS3
			DECLARE_SAMPLER(sampler2D, MirrorReflection, MirrorReflectionSampler,
				AddressU  = CLAMP;
				AddressV  = CLAMP;
				MIPFILTER = NONE;
				MINFILTER = PS3_SWITCH(GAUSSIAN, LINEAR);
				MAGFILTER = PS3_SWITCH(GAUSSIAN, LINEAR);
			);
		#endif // __PS3
		#if defined(USE_MIRROR_CRACK_MAP)
			BeginSampler(sampler2D, gMirrorCrackTexture, gMirrorCrackSampler, MirrorCrackTexture)
				string UIName      = "Crack Map";
				string TCPTemplate = "NormalMap";
				string TextureType="Bump";
			ContinueSampler(sampler2D, gMirrorCrackTexture, gMirrorCrackSampler, MirrorCrackTexture)
				AddressU  = CLAMP;
				AddressV  = CLAMP;
				MIPFILTER = MIPLINEAR;
				MINFILTER = HQLINEAR;
				MAGFILTER = MAGLINEAR;
			EndSampler;
		#endif // defined(USE_MIRROR_CRACK_MAP)
	#endif // REFLECT_MIRROR
#endif // REFLECT

#if defined(DISPLACEMENT_SHADER)
#define gDisplScaleU		(displParams.x)		// max 16 pixels displacement
#define gDisplScaleV		(displParams.y)		// max 16 pixels displacement
#define gDisplMaxCamDist	(displParams.z)		// max dist from camera where displacement is visible
#endif //DISPLACEMENT_SHADER...

// DISABLE FEATURES BASED ON HARDWARE LIMITATIONS
#if RSG_PC || RSG_DURANGO || __MAX
	#if __SHADERMODEL < 20
		// Really old shaders lose lots of stuff
		#undef NORMAL_MAP
		#define NORMAL_MAP					(0)
		#undef	PARALLAX
		#define	PARALLAX					(0)
		#undef REFLECT
		#define REFLECT						(0)
		#undef SPECULAR
		#define SPECULAR					(0)
		#undef DIFFUSE2
		#define DIFFUSE2					(0)
		#undef 	DIFFUSE_TEX_COORDS
		#define DIFFUSE_TEX_COORDS			float2
		
		// Currently, all we can go down to is 1.4 - if we need lower, we can allow it
		#define PIXELSHADER					ps_1_4
		#define VERTEXSHADER				vs_1_1
	#endif	// __SHADERMODEL
#elif RSG_ORBIS
	#define PIXELSHADER						ps_5_0
	#define VERTEXSHADER					vs_5_0
#elif __XENON || __PS3
	#define PIXELSHADER						ps_3_0
	#define VERTEXSHADER					vs_3_0
#endif

// Determine whether to pack in two uv's into one register
#if DIFFUSE2 || DIFFUSE_UV1 || SPECULAR_UV1
	#define DIFFUSE_TEX_COORDS				float4
#else
	#define DIFFUSE_TEX_COORDS				float2
#endif	// DIFFUSE2 || DIFFUSE_UV1 || SPECULAR_UV1

#define SHADOW_CASTING            (1)
#define SHADOW_CASTING_TECHNIQUES (1 && SHADOW_CASTING_TECHNIQUES_OK)

#define SHADOW_RECEIVING          (1)
#define SHADOW_RECEIVING_VS       (_USE_PERVERTEX_SHADOWING_PARABOLOID || USE_SHADOW_RECEIVING_VS)
#if DIFFUSE_TEXTURE && CUTOUT_OR_SSA_TECHNIQUES
	#define SHADOW_USE_TEXTURE
#endif

#ifdef USE_DETAIL_MAP
	#include "detail_map.fxh"
	#define DETAIL_MAP 1
#else
	#define DETAIL_MAP 0
#endif


#if (__SHADERMODEL >= 40)
// DX11 TODO:- These are temporary here for the RAG sliders.	 We`ll need to put in proper depth based calculations.
#define gTessellationFactor gTessellationGlobal1.y
#define gPN_KFactor gTessellationGlobal1.z
#endif


// Bit like USE_DIFFUSE2 but compatible with detail mapping etc. Texture coords could come from multiple sources.
#ifdef USE_OVERLAY_DIFFUSE2
#define OVERLAY_DIFFUSE2	(1)
#else
#define OVERLAY_DIFFUSE2	(0)
#endif

#if DIFFUSE2 && OVERLAY_DIFFUSE2
#error "Can`t use DIFFUSE2 and OVERLAY_DIFFUSE2 together!"
#endif


#ifdef USE_OVERLAY_DIFFUSE2_SEPARATE_ALPHA_SOURCE
#define OVERLAY_DIFFUSE2_SEPARATE_ALPHA_SOURCE	(1)

#if !OVERLAY_DIFFUSE2
#error "Can`t have OVERLAY_DIFFUSE2_SEPARATE_ALPHA_SOURCE without OVERLAY_DIFFUSE2
#endif

#else
#define OVERLAY_DIFFUSE2_SEPARATE_ALPHA_SOURCE	(0)
#endif

#ifdef USE_NIGHTONLY_VERTEX_SCALE
#define USE_DAY_NIGHT_VERTEX_SCALE	(1)
#define DAY_NIGHT_VERTEX_SCALE				gDayNightEffectsVertexScale
#define DAY_NIGHT_VERTEX_SMOOTH_SCALE		gDayNightEffects

// Alex - Got any idea how I do this bit?
#define DAY_NIGHT_VERTEX_NAN				float4(0.0f, 0.0f, 0.0f, 0.0f) 

float CalcDayNightVertScale(float4x4 worldMat)
{
	if(DAY_NIGHT_VERTEX_SCALE == 0.0f)
		return 0.0f;
	else
	{
		if(DAY_NIGHT_VERTEX_SMOOTH_SCALE < 0.006f)
		{
			if(abs(sin(2.5f*gUmGlobalTimer + fmod(worldMat[3].x, 5.0f))) > 0.3f)
				return 1.0f;
			else
				return 0.0f;
		}
		else
			return 1.0f;
	}
}


float4 ApplyDayNightNan(float4 vertIn, float4x4 worldMat)
{
	if(CalcDayNightVertScale(worldMat) > 0.0f)
		return vertIn;
	else
		return DAY_NIGHT_VERTEX_NAN;
}

#else // USE_NIGHTONLY_VERTEX_SCALE

#define USE_DAY_NIGHT_VERTEX_SCALE	(0)
#define DAY_NIGHT_VERTEX_SCALE		1.0f

float4 ApplyDayNightNan(float4 vertIn, float4x4 worldMat)
{
	return vertIn;
}

float CalcDayNightVertScale(float4x4 worldMat)
{
	return 1.0f;
}

#endif // USE_NIGHTONLY_VERTEX_SCALE

float4 ApplyDayNightNan(float4 vertIn)
{
	return ApplyDayNightNan(vertIn, gWorld);

}

#include "../Lighting/Shadows/localshadowglobals.fxh"
#include "../Lighting/Shadows/cascadeshadows.fxh"
#include "../Lighting/lighting.fxh"
#include "../Lighting/forward_lighting.fxh"
#include "../Debug/EntitySelect.fxh"

#ifdef USE_VEHICLE_DAMAGE
	#include "../Vehicles/vehicle_damage.fxh"
#endif


#if DISPLACEMENT_MAP || PARALLAX_MAP_V2
#if __SHADERMODEL >= 40 && !PARALLAX_MAP_V2
	//we need an DX10 sampler for the hull shader
	BeginDX10Sampler(sampler, Texture2D<float>, heightTexture, heightSampler, heightTexture)
#else
	//we still nee to declare a sampler for the tools which used DX9 shaders
	BeginSampler(sampler2D, heightTexture, heightSampler, heightTexture)
#endif // __SHADERMODEL >= 40
		int nostrip=1;
		string UIName = "Height Texture";
		string      TCPTemplateRelative	= "maps_other/DisplacementMap";
		int         TCPAllowOverride	= 0;
#if PARALLAX_MAP_V2
		string TextureType				= "Parallax";
#else
		string  TextureType				= "Displacement";
#endif
	ContinueSampler(sampler, heightTexture, heightSampler, heightTexture)
		AddressU  = WRAP;        
		AddressV  = WRAP;
		MINFILTER = LINEAR;
		MAGFILTER = LINEAR;
		MIPFILTER = POINT;
	EndSampler;
#endif // DISPLACEMENT_MAP


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ====== COMMON OUTPUT STRUCTURES ===========
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// like rageVertexInput, but extended:
struct northVertexInput
{
	// This covers the default rage vertex format (non-skinned)
	float3 pos			: POSITION;
	float4 diffuse		: COLOR0;
#if PALETTE_TINT_EDGE || BATCH_INSTANCING
	float4 specular		: COLOR1;
#endif
	float2 texCoord0	: TEXCOORD0;
	float2 texCoord1	: TEXCOORD1;
	float3 normal		: NORMAL;
};

DeclareInstancedStuct(northInstancedVertexInput, northVertexInput);


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ====== INPUT STRUCTURES ===========
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef USE_SECONDARY_TEXCOORDS
#define SECONDARY_TEXCOORDS (1)
#else
#define SECONDARY_TEXCOORDS (0)
#endif

#if defined(USE_SPECULAR_UV1)
#define REQUIRE_CHANNEL1_FOR_SPECULAR_UV1 (1)
#else
#define REQUIRE_CHANNEL1_FOR_SPECULAR_UV1 (0)
#endif

// Always have stuff in channel 0.
#define ACCOMODATE_CHANNEL0			(1)
// Fvf channel 1
#define ACCOMODATE_CHANNEL1			(EDGE_WEIGHT_PAD || DIFFUSE2 || SECONDARY_TEXCOORDS || UMOVEMENTS_TEX)
// Fvf channel 2 (DIFFUSE2, USE_SPECULAR_UV1  expect texCoord1 to exist - corresponds to fvf channel 2 or 3 - not sure if this code path is used).
#define ACCOMODATE_CHANNEL2			(EDGE_WEIGHT || DIFFUSE2 || REQUIRE_CHANNEL1_FOR_SPECULAR_UV1)
// Fvf channel 3 - not used yet!
#define ACCOMODATE_CHANNEL3			(0)
// Fvf channels 4 & 5.
#define ACCOMODATE_CHANNEL4			(DISPLACEMENT_DOMINANT_UV || DISPLACEMENT_LOD_COVERAGE)
#define ACCOMODATE_CHANNEL5			(DISPLACEMENT_DOMINANT_UV || DISPLACEMENT_LOD_COVERAGE)
 

#define CHANNEL(_highlow, _name) ((_highlow && _name) || (!_highlow && !_name))
#define ALL_CHANNELS(_f, _e, _d, _c, _b, _a) CHANNEL(_f, ACCOMODATE_CHANNEL5) && CHANNEL(_e, ACCOMODATE_CHANNEL4) && CHANNEL(_d, ACCOMODATE_CHANNEL3) && CHANNEL(_c, ACCOMODATE_CHANNEL2) && CHANNEL(_b, ACCOMODATE_CHANNEL1) && CHANNEL(_a, ACCOMODATE_CHANNEL0)


#if		ALL_CHANNELS(0, 0, 0, 0, 0, 1)
	#define CHANNEL0_SRC		texCoord0.xy
	#define TEXCOORD0_IN_USE	(1)
#elif	ALL_CHANNELS(0, 0, 0, 0, 1, 1)
	#define CHANNEL0_SRC		texCoord0.xy
	#define CHANNEL1_SRC		texCoord1.xy
	#define TEXCOORD0_IN_USE	(1)
	#define TEXCOORD1_IN_USE	(1)
#elif	ALL_CHANNELS(0, 0, 0, 1, 1, 1)
	#define CHANNEL0_SRC		texCoord0.xy
	#define CHANNEL1_SRC		texCoord1.xy
	#define CHANNEL2_SRC		texCoord2.xy
	#define TEXCOORD0_IN_USE	(1)
	#define TEXCOORD1_IN_USE	(1)
	#define TEXCOORD2_IN_USE	(1)
#elif	ALL_CHANNELS(0, 0, 0, 1, 0, 1)
	#define CHANNEL0_SRC		texCoord0.xy
	#define CHANNEL2_SRC		texCoord1.xy
	#define TEXCOORD0_IN_USE	(1)
	#define TEXCOORD1_IN_USE	(1)
#elif	ALL_CHANNELS(1, 1, 0, 0, 0, 1)
	#define CHANNEL0_SRC		texCoord0.xy
	#define CHANNEL4_SRC		texCoord1.xy
	#define CHANNEL5_SRC		texCoord2.xy
	#define TEXCOORD0_IN_USE	(1)
	#define TEXCOORD1_IN_USE	(1)
	#define TEXCOORD2_IN_USE	(1)
#elif	ALL_CHANNELS(1, 1, 0, 0, 1, 1)
	#define CHANNEL0_SRC		texCoord0.xy
	#define CHANNEL1_SRC		texCoord1.xy
	#define CHANNEL4_SRC		texCoord2.xy
	#define CHANNEL5_SRC		texCoord3.xy
	#define TEXCOORD0_IN_USE	(1)
	#define TEXCOORD1_IN_USE	(1)
	#define TEXCOORD2_IN_USE	(1)
	#define TEXCOORD3_IN_USE	(1)
#else
	#error "Invalid expected channel!"
#endif


#if		ALL_CHANNELS(0, 0, 0, 0, 0, 1)
#define TEXCOORD1_DUMMY_IN_USE	(1)
#else
#define TEXCOORD1_DUMMY_IN_USE	(0)
#endif

#define	TEXCOORD0_ZERO		float2(0.0f, 0.0f)
#define	TEXCOORD1_ZERO		float2(0.0f, 0.0f)
#define	TEXCOORD2_ZERO		float2(0.0f, 0.0f)
#define	TEXCOORD3_ZERO		float2(0.0f, 0.0f)

#ifdef USE_SECONDARY_TEXCOORDS_AS_OVERLAY_DIFFUSE2_COORDS

#if !OVERLAY_DIFFUSE2 || !SECONDARY_TEXCOORDS
#error "Need OVERLAY_DIFFUSE2 and USE_SECONDARY_TEXCOORDS defined to use USE_SECONDARY_TEXCOORDS_AS_OVERLAY_DIFFUSE2_COORDS!"
#endif

#define SECONDARY_TEXCOORDS_AS_OVERLAY_DIFFUSE2_COORDS 1
#else // USE_SECONDARY_TEXCOORDS_AS_OVERLAY_DIFFUSE2_COORDS
#define SECONDARY_TEXCOORDS_AS_OVERLAY_DIFFUSE2_COORDS 0
#endif // USE_SECONDARY_TEXCOORDS_AS_OVERLAY_DIFFUSE2_COORDS

#ifdef USE_VERTEXOUTPUTD_COLOR0DOTA_AS_OVERLAY_DIFFUSE2_ALPHA_SOURCE
#define OVERLAY_DIFFUSE2_ALPHA_SOURCE color0.a
#endif

// =============================================================================================== //
// northVertexInputBump
// =============================================================================================== //

//
//
// like rageVertexInputBump, but extended:
//


struct northVertexInputBump
{
	float3 pos			: POSITION;
	float4 diffuse		: COLOR0;
#if UMOVEMENTS || WIND_DISPLACEMENT || BATCH_INSTANCING
	float4 specular		: COLOR1;
#endif
#if PALETTE_TINT_EDGE
	float4 specular		: COLOR1;
#endif

#if		ALL_CHANNELS(0, 0, 0, 0, 0, 1)
	float2 texCoord0	: TEXCOORD0;
#elif	ALL_CHANNELS(0, 0, 0, 0, 1, 1)
	float2 texCoord0	: TEXCOORD0;
	float2 texCoord1	: TEXCOORD1;
#elif	ALL_CHANNELS(0, 0, 0, 1, 1, 1)
	float2 texCoord0	: TEXCOORD0;
	float2 texCoord1	: TEXCOORD1;
	float2 texCoord2	: TEXCOORD2;
#elif	ALL_CHANNELS(0, 0, 0, 1, 0, 1)
	float2 texCoord0	: TEXCOORD0;
	float2 texCoord1	: TEXCOORD1;
#elif	ALL_CHANNELS(1, 1, 0, 0, 0, 1)
	float2 texCoord0	: TEXCOORD0;
	float2 texCoord1	: TEXCOORD1;
	float2 texCoord2	: TEXCOORD2;
#elif	ALL_CHANNELS(1, 1, 0, 0, 1, 1)
	float2 texCoord0	: TEXCOORD0;
	float2 texCoord1	: TEXCOORD1;
	float2 texCoord2	: TEXCOORD2;
	float2 texCoord3	: TEXCOORD3;
#else
	#error "Invalid expected channel!"
#endif

	float3 normal		: NORMAL;
	float4 tangent		: TANGENT0;

#if RAGE_INSTANCED_TECH
	uint InstID : SV_InstanceID;
#endif
};

DeclareInstancedStuct(northInstancedVertexInputBump, northVertexInputBump);

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ==== INSTANCING FUNCTIONS: ====
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//
//
//
#if BATCH_INSTANCING && __SHADERMODEL >= 40
BatchComputeData GetComputeData(uint nInstIndex)
{
	BatchComputeData OUT;
	if(gUseComputeShaderOutputBuffer != 0)
	{
		OUT = ExpandComputeData(InstanceBuffer[nInstIndex]);
	}
	else
	{
		OUT.InstId = nInstIndex;
		OUT.CrossFade = 1.0f;
		OUT.Scale = 1.0f;
	}

	return OUT;
}

BatchRawInstanceData GetRawInstanceData(uint nInstIndex)
{
	return RawInstanceBuffer[nInstIndex];
}

//Batch Instancing Code
BatchGlobalData FetchSharedInstanceVertexData(uint nInstIndex)
{
	BatchGlobalData OUT;
	uint instSharedIndex = nInstIndex % GRASS_BATCH_SHARED_DATA_COUNT;
	uint instSharedCBIndex = instSharedIndex * GRASS_BATCH_NUM_CONSTS_PER_INSTANCE;
	OUT.worldMatA = gInstanceVars[instSharedCBIndex + 0];
	OUT.worldMatB = gInstanceVars[instSharedCBIndex + 1];
	OUT.worldMatC = gInstanceVars[instSharedCBIndex + 2];

	return OUT;
}
#endif //BATCH_INSTANCING && __SHADERMODEL >= 40

float3x3 ComputeRotationMatrix(float3 vFrom, float3 vTo)
{
	float3 v = cross(vFrom, vTo);
	float cost = dot(vFrom, vTo);
	float sint = length(v);
	v /= sint; //normalize

	float omc = 1.0f - cost;
	float3x3 rotationMtx = float3x3(	float3(omc * v.x * v.x + cost, omc * v.x * v.y + sint * v.z, omc * v.x * v.z - sint * v.y),
		float3(omc * v.x * v.y - sint * v.z, omc * v.y * v.y + cost, omc * v.y * v.z + sint * v.x),
		float3(omc * v.x * v.z + sint * v.y, omc * v.y * v.z - sint * v.x, omc * v.z * v.z + cost)	);

	float3x3 identityMtx = float3x3(	float3(1, 0, 0),
		float3(0, 1, 0),
		float3(0, 0, 1)	);

	return (cost < 1.0f ? rotationMtx : identityMtx);
}

struct megaVertInstanceData
{
#if BATCH_INSTANCING
	BatchInstanceData inst;
	BatchGlobalData global;
	BatchComputeData csData;
#endif

	float4x4 worldMtx;
	float alpha;
	float naturalAmbientScale;
	float artificialAmbientScale;
};

struct megaPixelInstanceData
{
	float alpha;
	float naturalAmbientScale;
	float artificialAmbientScale;
};

void FetchInstanceVertexData(northInstancedVertexInput instIN, out northVertexInput vertData, out int nInstIndex)
{
	vertData = instIN.baseVertIn;
#if (INSTANCED || BATCH_INSTANCING) && (__SHADERMODEL >= 40)
	nInstIndex = instIN.nInstIndex;
#else
	nInstIndex = 0;
#endif
}

void FetchInstanceVertexData(northInstancedVertexInputBump instIN, out northVertexInputBump vertData, out int nInstIndex)
{
	vertData = instIN.baseVertIn;
#if (INSTANCED || BATCH_INSTANCING) && (__SHADERMODEL >= 40)
	nInstIndex = instIN.nInstIndex;
#else
#	if RAGE_INSTANCED_TECH
		nInstIndex = instIN.baseVertIn.InstID;
#	else
		nInstIndex = 0;
#	endif
#endif

#if RAGE_INSTANCED_TECH && !RSG_ORBIS  // orbis can't access this in profile 'sce_ds_vs_orbis'
	vertData.InstID = nInstIndex;
#endif
}

void GetInstanceData(inout int nInstIndex, out megaVertInstanceData inst)
{
#if BATCH_INSTANCING
	inst.csData = GRASS_BATCH_CS_CULLING_SWITCH(GetComputeData(nInstIndex), (BatchComputeData)0); //Make SM30 happy.
	nInstIndex = inst.csData.InstId;	//CS writes original instance ID.

	inst.inst = GRASS_BATCH_CS_CULLING_SWITCH(ExpandRawInstanceData(GetRawInstanceData(nInstIndex)), (BatchInstanceData)0); //Make SM30 happy.
	inst.global = GRASS_BATCH_CS_CULLING_SWITCH(FetchSharedInstanceVertexData(nInstIndex), (BatchGlobalData)0); //Make SM30 happy.

	float3 posOffset = float3(inst.inst.pos, inst.inst.posZ) * vecBatchAabbDelta;
	float3 translation = vecBatchAabbMin + posOffset;

#if __SHADERMODEL == 30
	float3 terrainNormal = float3(0,1,0);
#else	
	float2 dxtNrml = inst.inst.normal*2-1;
	float3 terrainNormal = float3(dxtNrml, sqrt(1-dot(dxtNrml, dxtNrml)));
#endif // __SHADERMODEL == 30

#if GRASS_BATCH_CS_CULLING
	float fade = inst.csData.Scale;
#else //GRASS_BATCH_CS_CULLING
	float fade = 1.0f;	//Make SM30 happy.
#endif //GRASS_BATCH_CS_CULLING

	//Compute scale.
	const float perInstScale = ((gScaleRange.y - gScaleRange.x) * inst.inst.color_scale.w) + gScaleRange.x;

	const float scaleRange = gScaleRange.y * gScaleRange.z;
	float randScale = inst.global.worldMatA.w * scaleRange * 2; //*2 b/c art wants scale range to be +/-
	randScale -= scaleRange;

	float finalScale = max(perInstScale + randScale, 0.0f) * fade;

	//vertpos = vertpos.xyz * finalScale.xxx;	//Must apply in shader... or maybe apply to composite rotation matrix?

	//Orient batch to supplied terrain normal
	float3x3 rotMtx = float3x3(	inst.global.worldMatA.xyz,
								inst.global.worldMatB.xyz,
								inst.global.worldMatC.xyz	);

	//Note:	This assumes that batches will only ever be placed on terrain with a normal that has a positive value in the up direction.
	//		Then again, so does the terrain normal decompression code above!
	float3 terrainNormSlerp = rageSlerp(float3(0, 0, 1), terrainNormal, gOrientToTerrain);
	float3x3 terrainMtx = ComputeRotationMatrix(float3(0, 0, 1), terrainNormSlerp);

#if COMPUTE_COMPOSITE_WORLD_MTX
	float3x3 worldMtx3x3 = mul(rotMtx, terrainMtx);

	float4x4 worldMtx;
	inst.worldMtx[0] = float4(worldMtx3x3[0] * finalScale.xxx, 0);
	inst.worldMtx[1] = float4(worldMtx3x3[1] * finalScale.xxx, 0);
	inst.worldMtx[2] = float4(worldMtx3x3[2] * finalScale.xxx, 0);
	inst.worldMtx[3] = float4(translation, 1);
#else
	//Need to implement per-matrix multiply
#	error Not implemented!
#endif //COMPUTE_COMPOSITE_WORLD_MTX

	inst.alpha =					globalAlpha;
	inst.naturalAmbientScale =		gNaturalAmbientScale;
	inst.artificialAmbientScale =	gArtificialAmbientScale;
#elif INSTANCED
	int nInstRegBase = nInstIndex * NUM_CONSTS_PER_INSTANCE;
	float3x4 worldMat;
	worldMat[0] = gInstanceVars[nInstRegBase];
	worldMat[1] = gInstanceVars[nInstRegBase+1];
	worldMat[2] = gInstanceVars[nInstRegBase+2];
	inst.worldMtx[0] = float4(worldMat[0].xyz, 0);
	inst.worldMtx[1] = float4(worldMat[1].xyz, 0);
	inst.worldMtx[2] = float4(worldMat[2].xyz, 0);
	inst.worldMtx[3] = float4(worldMat[0].w, worldMat[1].w, worldMat[2].w, 1);

	inst.alpha =					gInstanceVars[nInstRegBase+3].x;
	inst.naturalAmbientScale =		gInstanceVars[nInstRegBase+3].y;
	inst.artificialAmbientScale =	gInstanceVars[nInstRegBase+3].z;
#else
	inst.worldMtx =					gWorld;
	inst.alpha =					globalAlpha;
	inst.naturalAmbientScale =		gNaturalAmbientScale;
	inst.artificialAmbientScale =	gArtificialAmbientScale;
#endif
}

// =============================================================================================== //


// Pack vertex data tightly between Vs-Hs-Ds stages
#define TESSELLATION_VERTEX_PACK		(1 && DISPLACEMENT_MAP)
#define TESSELLATION_PAD				(RSG_ORBIS && !TESSELLATION_VERTEX_PACK)

#if TESSELLATION_VERTEX_PACK

	#define TCTRL_POSITION	float3
	#define TCTRL_TEXCOORD	uint
	#define TCTRL_COLOUR	uint
	#define	TCTRL_NORMAL	uint
	#define	TCTRL_TANGENT	uint

	#define ToCTRL_POSITION(x)	x
	#define ToCTRL_TEXCOORD(x)	packTexCoords(x)
	#define ToCTRL_COLOUR(x)	packColorToUint(x)
	#define	ToCTRL_NORMAL(x)	packNormalToUint(x)	
	#define	ToCTRL_TANGENT(x)	packTangentToUint(x)

	#define FromCTRL_NORMAL(x)	unpackNormalFromUint(x)

#else // TESSELLATION_VERTEX_PACK

#if TESSELLATION_PAD
	// Working around Sony bug: https://ps4.scedev.net/support/issue/25862/_SDK_1.7_tessellation
	#define TCTRL_POSITION	float4
	#define TCTRL_TEXCOORD	float4
	#define TCTRL_COLOUR	float4
	#define	TCTRL_NORMAL	float4
	#define	TCTRL_TANGENT	float4

	#define ToCTRL_POSITION(x)	float4(x, 1)
	#define ToCTRL_TEXCOORD(x)	float4(x, 0, 0)
	#define ToCTRL_COLOUR(x)	float4(x)
	#define	ToCTRL_NORMAL(x)	float4(x, 1)	
	#define	ToCTRL_TANGENT(x)	float4(x)

	#define FromCTRL_NORMAL(x)	float3(x)
#else 
	#define TCTRL_POSITION	float3
	#define TCTRL_TEXCOORD	float2
	#define TCTRL_COLOUR	float4
	#define	TCTRL_NORMAL	float3
	#define	TCTRL_TANGENT	float4

	#define ToCTRL_POSITION(x)	x
	#define ToCTRL_TEXCOORD(x)	x
	#define ToCTRL_COLOUR(x)	x
	#define	ToCTRL_NORMAL(x)	x	
	#define	ToCTRL_TANGENT(x)	x

	#define FromCTRL_NORMAL(x)	x
#endif //TESSELLATION_PAD

#endif // TESSELLATION_VERTEX_PACK



#ifndef CHANNEL_PACK_RANGE
#define CHANNEL_PACK_RANGE_TEXCOORDS_MIN float2(-2,-8.5)
#define CHANNEL_PACK_RANGE_TEXCOORDS_MAX float2(2,1.5)
#else
#ifndef CHANNEL_PACK_RANGE_TEXCOORDS_MIN 
#error "Require CHANNEL_PACK_RANGE_TEXCOORDS_MAX to be defined!"
#endif
#ifndef CHANNEL_PACK_RANGE_TEXCOORDS_MAX 
#error "Require CHANNEL_PACK_RANGE_TEXCOORDS_MAX to be defined!"
#endif
#endif // CHANNEL_PACK_RANGE

#if TESSELLATION_VERTEX_PACK

#define RAGE_COMPUTE_BARYCENTRIC_UNPACKED(_coord, _patch, _component, _unpack) \
	rage_ComputeBarycentric(_coord, _unpack(_patch[0]._component), _unpack(_patch[1]._component), _unpack(_patch[2]._component))

#else // //TESSELLATION_VERTEX_PACK

#define RAGE_COMPUTE_BARYCENTRIC_UNPACKED(_coord, _patch, _component, _unpack) \
	rage_ComputeBarycentric(_coord, _patch[0]._component, _patch[1]._component, _patch[2]._component)

#endif //TESSELLATION_VERTEX_PACK

uint packColorToUint(float4 color)	// 8 bit per component
{
	uint4 bytes = uint4(saturate(color) * 255.0);
	return bytes[0] + (bytes[1]<<8) + (bytes[2]<<16) + (bytes[3]<<24);
}
float4 unpackColorFromUint(uint data)
{
	uint4 bytes = (data >> uint4(0,8,16,24)) & 0xFF;
	return float4(bytes) / 0xFF;
}

uint packTexCoords(float2 coords)	// 16 bits per component
{
	float2 normalized = (coords - CHANNEL_PACK_RANGE_TEXCOORDS_MIN)/(CHANNEL_PACK_RANGE_TEXCOORDS_MAX-CHANNEL_PACK_RANGE_TEXCOORDS_MIN);
	uint2 integer = uint2(saturate(normalized) * 0xFFFF);
	return integer[0] + (integer[1]<<16);
}
float2 unpackTexCoords(uint data)
{
	float2 normalized = float2((data >> uint2(0,16)) & 0xFFFF) / 0xFFFF;
	return normalized*(CHANNEL_PACK_RANGE_TEXCOORDS_MAX-CHANNEL_PACK_RANGE_TEXCOORDS_MIN) + CHANNEL_PACK_RANGE_TEXCOORDS_MIN;
}

uint packNormalToUint(float3 normal)	//10 bits per component
{
	uint3 digits = uint3(saturate(normal*0.5 + 0.5) * 0x3FF);
	return digits[0] + (digits[1]<<10) + (digits[2]<<20);
}
float3 unpackNormalFromUint(uint data)
{
	uint3 digits = (data >> uint3(0,10,20)) & 0x3FF;
	return (float3(digits) / 0x3FF) * 2.0 - 1.0;
}
uint packTangentToUint(float4 tangent)	//30 bits normal, 1 bit sign
{
	uint data = packNormalToUint(tangent.xyz);
	return data + ((tangent.w>0.0 ? 1 : 0) << 30);
}
float4 unpackTangentFromUint(uint data)
{
	float w = (data>>30) ? 1.0 : 0.0;
	return float4(unpackNormalFromUint(data), w);
}


#if USE_PN_TRIANGLES

struct northVertexInputBump_CtrlPoint
{
	TCTRL_POSITION	pos			: CTRL_POSITION;
	TCTRL_COLOUR	diffuse		: CTRL_COLOR0;
#if UMOVEMENTS || WIND_DISPLACEMENT || PALETTE_TINT_EDGE
	float4			specular	: CTRL_COLOR1;
#endif
#if ACCOMODATE_CHANNEL0
	TCTRL_TEXCOORD	texChan0	: CTRL_TEXCOORD0;
#endif
#if ACCOMODATE_CHANNEL1
	TCTRL_TEXCOORD	texChan1	: CTRL_TEXCOORD1;
#endif
#if ACCOMODATE_CHANNEL2
	TCTRL_TEXCOORD	texChan2	: CTRL_TEXCOORD2;
#endif
#if ACCOMODATE_CHANNEL4
	TCTRL_TEXCOORD	tcDominant	: CTRL_UV_DOMINANT;
#endif
#if ACCOMODATE_CHANNEL5
	half2	tcArea				: CTRL_UV_AREA;
#endif
	TCTRL_NORMAL	normal		: CTRL_NORMAL;
	TCTRL_TANGENT	tangent		: CTRL_TANGENT0;

#if INSTANCED || BATCH_INSTANCING
	uint InstID			: TEXCOORD15;
#endif
};


northVertexInputBump_CtrlPoint CtrlPointFromSkinnedIN(northSkinVertexInputBump IN)
{
	northVertexInputBump_CtrlPoint OUT = (northVertexInputBump_CtrlPoint)0;

	rageSkinMtx skinMtx = ComputeSkinMtx(IN.blendindices, IN.weight);
	float3 skinnedPos	= rageSkinTransform(IN.pos, skinMtx);
	float3 skinnedNormal= rageSkinRotate(IN.normal, skinMtx);
#if NORMAL_MAP
	float4 skinnedTangent	= float4(rageSkinRotate(IN.tangent.xyz, skinMtx), IN.tangent.w);
#else
	float4 skinnedTangent = float4(1.0, 0.0, 0.0, 1.0);
#endif

	OUT.pos = ToCTRL_POSITION(DAY_NIGHT_VERTEX_SCALE*skinnedPos);
	OUT.diffuse = ToCTRL_COLOUR(IN.diffuse); 
#if UMOVEMENTS || PALETTE_TINT_EDGE
	OUT.specular = IN.specular;
#endif

# if ACCOMODATE_CHANNEL0
	OUT.texChan0	= ToCTRL_TEXCOORD(IN.CHANNEL0_SRC);
# endif
# if ACCOMODATE_CHANNEL1
#  if PED_DAMAGE_TARGETS || UMOVEMENTS_TEX
	OUT.texChan1	= ToCTRL_TEXCOORD(IN.CHANNEL1_SRC);
#  else
	OUT.texChan1	= ToCTRL_TEXCOORD(float2(0, 0));
#  endif
# endif
# if ACCOMODATE_CHANNEL2
#  if DIFFUSE_UV1 || SPECULAR_UV1 || EDGE_WEIGHT
	OUT.texChan2	= ToCTRL_TEXCOORD(IN.texCoord0);
#  else
	OUT.texChan2	= ToCTRL_TEXCOORD(float2(0, 0));
#  endif
# endif
# if ACCOMODATE_CHANNEL4
	OUT.tcDominant	= ToCTRL_TEXCOORD(float2(0,0));
# endif
# if ACCOMODATE_CHANNEL5
	OUT.tcArea = half2(1,0);
# endif
	OUT.normal = ToCTRL_NORMAL(skinnedNormal);
	OUT.tangent = ToCTRL_TANGENT(skinnedTangent);

	return OUT;
}


northVertexInputBump_CtrlPoint CtrlPointFromIN(northVertexInputBump IN)
{
	northVertexInputBump_CtrlPoint OUT;
	
	OUT.pos = ToCTRL_POSITION(IN.pos);
	OUT.diffuse = ToCTRL_COLOUR(IN.diffuse);
#if UMOVEMENTS || WIND_DISPLACEMENT || PALETTE_TINT_EDGE
	OUT.specular = IN.specular;
#endif

# if ACCOMODATE_CHANNEL0
	OUT.texChan0	= ToCTRL_TEXCOORD(IN.CHANNEL0_SRC);
# endif
# if ACCOMODATE_CHANNEL1
	OUT.texChan1	= ToCTRL_TEXCOORD(IN.CHANNEL1_SRC);
# endif
# if ACCOMODATE_CHANNEL2
	OUT.texChan2	= ToCTRL_TEXCOORD(IN.CHANNEL2_SRC);
# endif
# if ACCOMODATE_CHANNEL4
	OUT.tcDominant	= ToCTRL_TEXCOORD(IN.CHANNEL4_SRC);
# endif
# if ACCOMODATE_CHANNEL5
	OUT.tcArea = IN.CHANNEL5_SRC;
# endif
	OUT.normal = ToCTRL_NORMAL(IN.normal);
	OUT.tangent = ToCTRL_TANGENT(IN.tangent);

	return OUT;
}


// Vertex shader which outputs a control point.
northVertexInputBump_CtrlPoint VS_northVertexInputBump_PNTri(northInstancedVertexInputBump instIN
#ifdef RAGE_ENABLE_OFFSET
	, rageVertexOffsetBump INOFFSET, uniform float bEnableOffsets
#endif
)
{
	int nInstIndex;
	northVertexInputBump IN;
	FetchInstanceVertexData(instIN, IN, nInstIndex);

	// Output a control point. 
	northVertexInputBump_CtrlPoint OUT = CtrlPointFromIN(IN);

#if INSTANCED || BATCH_INSTANCING
	OUT.InstID = nInstIndex;
#endif

	return OUT;
}


// Vertex shader which outputs a model space control point (applies skinning).
northVertexInputBump_CtrlPoint VS_northSkinVertexInputBump_PNTri(northSkinVertexInputBump IN
#ifdef RAGE_ENABLE_OFFSET
	, rageVertexOffsetBump INOFFSET, uniform float bEnableOffsets
#endif
)
{
	// Output a control point. 
	northVertexInputBump_CtrlPoint OUT = CtrlPointFromSkinnedIN(IN);
	return OUT;
}


// Patch Constant Function.
rage_PNTri_PatchInfo HS_northVertexInputBump_PNTri_PF(InputPatch<northVertexInputBump_CtrlPoint, 3> PatchPoints,  uint PatchID : SV_PrimitiveID)
{	
	rage_PNTri_PatchInfo Output = (rage_PNTri_PatchInfo)0;
	
	rage_PNTri_Vertex Points[3];

	Points[0].Position	= PatchPoints[0].pos;
	Points[0].Normal	= FromCTRL_NORMAL(PatchPoints[0].normal);
	Points[1].Position	= PatchPoints[1].pos;
	Points[1].Normal	= FromCTRL_NORMAL(PatchPoints[1].normal);
	Points[2].Position	= PatchPoints[2].pos;
	Points[2].Normal	= FromCTRL_NORMAL(PatchPoints[2].normal);

#if INSTANCED
	megaVertInstanceData inst;
	GetInstanceData(PatchPoints[0].InstID, inst);
	float4x4 worldMtx = inst.worldMtx;
#else
	float4x4 worldMtx = gWorld;
#endif

#if ENABLE_FRUSTUM_CULL || ENABLE_BACKFACE_CULL
	if (CullEnable)
	{
# if ENABLE_FRUSTUM_CULL
		float3 vPosition[3];
		vPosition[0] = mul(float4(Points[0].Position, 1.0f), worldMtx).xyz;
		vPosition[1] = mul(float4(Points[1].Position, 1.0f), worldMtx).xyz;
		vPosition[2] = mul(float4(Points[2].Position, 1.0f), worldMtx).xyz;

		if (!rage_IsTriangleInFrustum(vPosition[0], vPosition[1], vPosition[2], ViewFrustumEpsilon))
			return Output;
# endif // ENABLE_FRUSTUM_CULL

# if ENABLE_BACKFACE_CULL
		float3 vNormal[3];
		vNormal[0] = mul(Points[0].Normal, (float3x3)worldMtx);
		vNormal[1] = mul(Points[1].Normal, (float3x3)worldMtx);
		vNormal[2] = mul(Points[2].Normal, (float3x3)worldMtx);

		float3 vViewForward = gViewInverse[2].xyz;

		if (rage_BackFaceCulling(vNormal[0], vNormal[1], vNormal[2], vViewForward, BackFaceCullEpsilon))
			return Output;
# endif // ENABLE_BACKFACE_CULL
	}
#endif // ENABLE_FRUSTUM_CULL || ENABLE_BACKFACE_CULL

	rage_ComputePNTrianglePatchInfo(Points[0], Points[1], Points[2], Output, tessellationMultiplier);
	
	return Output;
}


// Hull shader.
[domain("tri")]
[partitioning("fractional_odd")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(3)]
[patchconstantfunc("HS_northVertexInputBump_PNTri_PF")]
[maxtessfactor(15.0)]
northVertexInputBump_CtrlPoint HS_northVertexInputBump_PNTri(InputPatch<northVertexInputBump_CtrlPoint, 3> PatchPoints, uint i : SV_OutputControlPointID, uint PatchID : SV_PrimitiveID)
{
	northVertexInputBump_CtrlPoint Output;

	// Our control points match the vertex shader output.
	Output = PatchPoints[i];

	return Output;
}

northVertexInputBump northVertexInputBump_PNTri_EvaluateAt(rage_PNTri_PatchInfo PatchInfo, float3 WUV, const OutputPatch<northVertexInputBump_CtrlPoint, 3> PatchPoints)
{
	northVertexInputBump Arg;

	Arg.pos = rage_EvaluatePatchAt(PatchInfo, WUV);
	Arg.normal = rage_EvaluatePatchNormalAt(PatchInfo, WUV);
	Arg.diffuse = RAGE_COMPUTE_BARYCENTRIC_UNPACKED(WUV, PatchPoints, diffuse, unpackColorFromUint);
	Arg.tangent = RAGE_COMPUTE_BARYCENTRIC_UNPACKED(WUV, PatchPoints, tangent, unpackTangentFromUint);

#if UMOVEMENTS || WIND_DISPLACEMENT || PALETTE_TINT_EDGE
	Arg.specular = RAGE_COMPUTE_BARYCENTRIC(WUV, PatchPoints, specular);
#endif

# if ACCOMODATE_CHANNEL0
	Arg.CHANNEL0_SRC = RAGE_COMPUTE_BARYCENTRIC_UNPACKED(WUV, PatchPoints, texChan0, unpackTexCoords);
# endif
# if ACCOMODATE_CHANNEL1
	Arg.CHANNEL1_SRC = RAGE_COMPUTE_BARYCENTRIC_UNPACKED(WUV, PatchPoints, texChan1, unpackTexCoords);
# endif
# if ACCOMODATE_CHANNEL2
	Arg.CHANNEL2_SRC = RAGE_COMPUTE_BARYCENTRIC_UNPACKED(WUV, PatchPoints, texChan2, unpackTexCoords);
# endif
# if ACCOMODATE_CHANNEL4
	Arg.CHANNEL4_SRC = 0;
	float2 tcDominant = RAGE_COMPUTE_BARYCENTRIC_UNPACKED(WUV, PatchPoints, tcDominant, unpackTexCoords);
# endif
# if ACCOMODATE_CHANNEL5
	Arg.CHANNEL5_SRC = 0;
	float2 tcArea = RAGE_COMPUTE_BARYCENTRIC(WUV, PatchPoints, tcArea);
# endif

#if DISPLACEMENT_MAP
	float3 normal = normalize(Arg.normal);
	float mipLevel = 0.0f;
# if DISPLACEMENT_LOD_ADAPTIVE
	// compute the edge-derived mip level, inspired by:
	// http://sebastiansylvan.com/2010/04/18/the-problem-with-tessellation-in-directx-11/
	float2 dim = 0;
	heightTexture.GetDimensions( dim.x, dim.y );
	float2 T[3] = {
		PatchPoints[0].texCoord0 * dim,
		PatchPoints[1].texCoord0 * dim,
		PatchPoints[2].texCoord0 * dim
	};
	float2 T12 = T[2]-T[1], T20 = T[0]-T[2], T01 = T[1]-T[0];
	const float uvMultiplier = 1.0;	// exp2 of LOD offset, originally: 2.0
#  if DISPLACEMENT_LOD_COVERAGE
	float uvDistance = uvMultiplier * tcArea.x / PatchInfo.Inside[0];
#  else // DISPLACEMENT_LOD_COVERAGE
	float3 factoredLengths = float3(length(T12), length(T20), length(T01)) /
		float3( PatchInfo.Edges[0], PatchInfo.Edges[1], PatchInfo.Edges[2] );
	float3 edgeWeights = 0.5 * (WUV.yzx+WUV.zxy);
	float uvDistance = uvMultiplier * dot( WUV, factoredLengths );
#  endif // DISPLACEMENT_LOD_COVERAGE
#  if DISPLACEMENT_LOD_EDGE_AWARE
	// find the distances in UV pixels to the edges of the original patch
	float2 X = Arg.texCoord0 * dim;
	float2 H[3] = {	//closest points on the edges
		(dot(T12,X)*T12 + dot(T12,T[2])*T[1] - dot(T12,T[1])*T[2]) / dot(T12,T12),
		(dot(T20,X)*T20 + dot(T20,T[0])*T[2] - dot(T20,T[2])*T[0]) / dot(T20,T20),
		(dot(T01,X)*T01 + dot(T01,T[1])*T[0] - dot(T01,T[0])*T[1]) / dot(T01,T01),
	};
	float3 HD = float3( length(H[0]-X), length(H[1]-X), length(H[2]-X) );
	uvDistance = min( min( uvDistance, HD.x ), min( HD.y, HD.z ) );
#  endif // DISPLACEMENT_LOD_EDGE_AWARE
	mipLevel = max( 0.0, log2(uvDistance) );
#  if DISPLACEMENT_LOD_DEBUG
	const float debugMaxLevel = 5.0;
#   if DISPLACEMENT_LOD_EDGE_AWARE
	Arg.diffuse = float4(log2(HD),mipLevel) / debugMaxLevel;
#   else
	Arg.diffuse = float4(mipLevel/debugMaxLevel, PatchInfo.Inside[0]/debugMaxLevel, tcArea);
#   endif // DISPLACEMENT_LOD_EDGE_AWARE
#  endif // DISPLACEMENT_LOD_DEBUG
# endif // DISPLACEMENT_LOD_ADAPTIVE
	
	float2 texCoord = Arg.texCoord0.xy;
	float4 debugColor = float4(0,0,0,1);

	float height = heightTexture.SampleLevel(heightSampler, texCoord, mipLevel).x;
	if (WUV.x*WUV.y*WUV.z==0)	// edge of the original primitive
	{
# if DISPLACEMENT_DOMINANT_UV && ACCOMODATE_CHANNEL5
		const float3 vertexSeams = float3(
			PatchPoints[0].tcArea.y,
			PatchPoints[1].tcArea.y,
			PatchPoints[2].tcArea.y);
		//assert(tcArea == dot(WUV,vertexSeams));

		if ((WUV.x!=0.0f && vertexSeams.x==1.0f) || (WUV.y!=0.0f && vertexSeams.y==1.0f) || (WUV.z!=0.0f && vertexSeams.z==1.0f))
		{
			//one of the ends is trivial - leave as is
			debugColor = float4(0,1,0,0);
		}else
		if (dot(WUV, abs(vertexSeams-2.0f)) == 0.0f)
		{
			//UV seam - average with the height on the other side
			debugColor = float4(0,0,1,0);
			float otherHeight = heightTexture.SampleLevel(heightSampler, tcDominant, mipLevel).x;
			height = 0.5 * (height + otherHeight);
		}else
		if (tcArea.y == 0.0f)
		{
			//mesh border - weld with other parts
			debugColor = float4(1,1,1,0);
			height = -heightBias;
		}else
# endif // DISPLACEMENT_DOMINANT_UV && ACCOMODATE_CHANNEL5
		{
			//case is too complex - fall back to 0.5
			debugColor = float4(1,0,0,0);
			height = 0.5;
		}
	}

# if DISPLACEMENT_LOD_DEBUG
	Arg.diffuse = debugColor;
# endif

	height += heightBias;
	Arg.pos += normal * (height * heightScale * globalHeightScale);
#endif // DISPLACEMENT_MAP

	return Arg;
}

uint PNTri_EvaluateInstanceID(float3 WUV, const OutputPatch<northVertexInputBump_CtrlPoint, 3> PatchPoints)
{
#if INSTANCED || BATCH_INSTANCING
	//Instance ID should be constant across a patch, so no reason to compute it.
	return PatchPoints[0].InstID;
#else
	return 0;
#endif
}

#endif // USE_PN_TRIANGLES

// =============================================================================================== //

//
//
//
//
struct vertexOutput
{
#if (1 && RSG_PC && !__MAX)
	inside_centroid
#endif
	float4 color0					: COLOR0;
#if PALETTE_TINT
#if (1 && RSG_PC && !__MAX)
	inside_centroid
#endif
	float4 color1					: COLOR1;
#endif
#if DIFFUSE_TEXTURE
	DIFFUSE_TEX_COORDS texCoord		: TEXCOORD0;
#endif
	float3 worldNormal				: TEXCOORD1;
#if REFLECT || PARALLAX_MAP_V2
	float3 worldEyePos				: TEXCOORD3;
#endif	
#if NORMAL_MAP
	float3 worldTangent				: TEXCOORD4;
	float3 worldBinormal			: TEXCOORD5;
#endif
	float4 worldPos					: TEXCOORD6;
#if PARALLAX
	float4 tanEyePos				: TEXCOORD7;
#endif
#if REFLECT_MIRROR
	float4 screenPos				: TEXCOORD7;
#endif
#if EDGE_WEIGHT
	float2 edgeWeight				: TEXCOORD8;
#endif
#if OVERLAY_DIFFUSE2		
	float2 overlayDiffuse2Coords	: TEXCOORD9;
#endif
	DECLARE_POSITION_CLIPPLANES(pos)

#if BATCH_INSTANCING
	float4	batchTint				: COLOR2;
	float	batchAo					: COLOR3;
#endif //BATCH_INSTANCING
};

#if APPLY_DOF_TO_ALPHA_DECALS
struct vertexOutputDOF
{
	vertexOutput vertOutput;
	float linearDepth				: TEXCOORD10;
};
#endif //APPLY_DOF_TO_ALPHA_DECALS

#if defined(DECAL_AMB_ONLY) || defined(DECAL_DIRT) || defined(DECAL_EMISSIVE_ONLY) || defined(DECAL_EMISSIVENIGHT_ONLY)
	#define VERTEXOUTPUTD_NORMAL			(0)
#else
	#define VERTEXOUTPUTD_NORMAL			(1)
#endif

#if defined(DECAL_NORMAL_ONLY)
	#define VERTEXOUTPUTD_COLOR0			(1)
#else
	#define VERTEXOUTPUTD_COLOR0			(1)
#endif

#if (REFLECT && !REFLECT_DYNAMIC) || SECOND_SPECULAR_LAYER || defined(DECAL_SHADOW_ONLY)|| PARALLAX_MAP_V2
	#define VERTEXOUTPUTD_WORLDEYEPOS			(1)
#else
	#define VERTEXOUTPUTD_WORLDEYEPOS			(0)
#endif
//
//
//
//
struct vertexOutputD
{
	DECLARE_POSITION(pos)
#if VERTEXOUTPUTD_COLOR0 || BATCH_INSTANCING
	float4 color0					: COLOR0;
#endif
#if PALETTE_TINT
	float4 color1					: COLOR1;
#endif
#if DIFFUSE_TEXTURE
	DIFFUSE_TEX_COORDS texCoord		: TEXCOORD0;
#endif
#if VERTEXOUTPUTD_NORMAL
	float3 worldNormal				: TEXCOORD1;
#endif //VERTEXOUTPUTD_NORMAL...
#if VERTEXOUTPUTD_WORLDEYEPOS
	float3 worldEyePos				: TEXCOORD3;
#endif	
#if NORMAL_MAP
	float3 worldTangent				: TEXCOORD4;
	float3 worldBinormal			: TEXCOORD5;
#endif
#if PARALLAX
	float4 tanEyePos				: TEXCOORD7;
#endif
#if EDGE_WEIGHT
	float2 edgeWeight				: TEXCOORD8;
#endif
#if OVERLAY_DIFFUSE2		
	float2 overlayDiffuse2Coords	: TEXCOORD9;
#endif
#if BATCH_INSTANCING
	float4	batchTint				: COLOR2;
	float	batchAo					: COLOR3;
#endif //BATCH_INSTANCING
#if WIND_DISPLACEMENT && !SHADER_FINAL
	float4	windDebug				:	COLOR4;
#endif
};

//
//
//
//
/*
// TODO --
struct vertexOutputAlphaClip2
{
	DECLARE_POSITION(pos)
#if DIFFUSE_TEXTURE
	DIFFUSE_TEX_COORDS texCoord		: TEXCOORD0;
#endif
};
*/

//
//
//
//
struct vertexOutputUnlit
{
	DECLARE_POSITION(pos)
	float4 color0					: COLOR0;
#if PALETTE_TINT
	float4 color1					: COLOR1;
#endif
#if DIFFUSE_TEXTURE
	DIFFUSE_TEX_COORDS texCoord		: TEXCOORD0;
#endif
#if BATCH_INSTANCING
	float4	batchTint				: COLOR2;
	float	batchAo					: COLOR3;
#endif //BATCH_INSTANCING
};

//
//
//
//
struct vertexOutputMaxRageViewer
{
	DECLARE_POSITION(pos)
	float4 color0					: COLOR0;
#if PALETTE_TINT
	float4 color1					: COLOR1;
#endif
#if DIFFUSE_TEXTURE
	DIFFUSE_TEX_COORDS texCoord		: TEXCOORD0;
#endif
	float3 worldNormal				: TEXCOORD1;
	float3 worldPos					: TEXCOORD2;
};


//
//
//
//
struct vertexOutputWaterReflection
{
	float4 color0					: COLOR0;
#if PALETTE_TINT
	float4 color1					: COLOR1;
#endif
#if DIFFUSE_TEXTURE
	DIFFUSE_TEX_COORDS texCoord		: TEXCOORD0;
#endif
	float3 worldNormal				: TEXCOORD1;
	float3 worldPos					: TEXCOORD2;
	DECLARE_POSITION_CLIPPLANES(pos)

#if BATCH_INSTANCING
	float4	batchTint				: COLOR2;
	float	batchAo					: COLOR3;
#endif //BATCH_INSTANCING
};


//
//
//
//
struct vertexMegaOutputCube
{
	DECLARE_POSITION(pos)
	float4 color0					: COLOR0;
	#if PALETTE_TINT
		float4 color1				: COLOR1;
	#endif
	#if DIFFUSE_TEXTURE
		DIFFUSE_TEX_COORDS texCoord	: TEXCOORD0;
	#endif
	float3 worldNormal				: TEXCOORD1;
	float3 worldPos					: TEXCOORD2;
	// watch out for addition attribute. may need to fix vertexOutputCubeInst.

#if BATCH_INSTANCING
	float4	batchTint				: COLOR2;
	float	batchAo					: COLOR3;
#endif //BATCH_INSTANCING
};

#if PUDDLE_TECHNIQUES
#include "Puddle.fxh"
#endif // PUDDLE_TECHNIQUES

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ==== COMMON FUNCTIONS: ====
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//
//
//
void rtsProcessVertLightingUnskinned(
					float3 localPos,
					float3 localNormal,
	#if NORMAL_MAP
					float4 localTangent,
	#endif // NORMAL_MAP
					float4 vertColor,
	#if PALETTE_TINT_EDGE
					float4 vertColor2,
	#endif
					float4x3 worldMtx,
					out float3 worldPos,
					out float3 worldEyePos,
					out float3 worldNormal,
	#if NORMAL_MAP
					out float3 worldBinormal,
					out float3 worldTangent,
		#if PARALLAX
					out float4 tanEyePos,
		#endif
	#endif	// NORMAL_MAP
					out float4 color0
	#if PALETTE_TINT
					,out float4 color1
	#endif
					)
{
	float3 pos = mul(float4(localPos,1), worldMtx);
	worldPos = pos;

	// Store position in eyespace
	worldEyePos = gViewInverse[3].xyz - worldPos;


	// Some meshes do not provide normals correctly
	if (dot(localNormal,localNormal)<0.1)
		localNormal = float3(0,0,1);

	// Transform normal by "transpose" matrix
	worldNormal = NORMALIZE_VS(mul(localNormal, (float3x3)worldMtx));

#if USE_SLOPE	  // if NORMALIZE_VS does normalize this can be removed
	worldNormal = normalize(worldNormal);
#endif
	#if NORMAL_MAP
		// Do the tangent+binormal stuff
		worldTangent = NORMALIZE_VS(mul(localTangent.xyz, (float3x3)worldMtx));
	#endif		// NORMAL_MAP

    color0 = vertColor;
	#if PALETTE_TINT
		#if PALETTE_TINT_EDGE
			color1 = UnpackTintPalette(vertColor2);
		#else
			color1 = UnpackTintPalette(vertColor);
		#endif
	#endif

#if NORMAL_MAP
	worldBinormal	= rageComputeBinormal(worldNormal, worldTangent, localTangent.w);
	#if PARALLAX
		// transform EyePos into tangent space:
		tanEyePos.x = dot(worldTangent.xyz, worldEyePos.xyz);
		tanEyePos.y = dot(worldBinormal.xyz,worldEyePos.xyz);
		tanEyePos.z = dot(worldNormal.xyz,	worldEyePos.xyz);
		tanEyePos.w = 1.0f;
	#endif //PARALLAX...
#endif		// NORMAL_MAP
	
}

//
//
//
//
void rtsProcessVertLightingBasic(
					float3 localPos,
					float3 localNormal,
					float4 vertColor,
				#if PALETTE_TINT_EDGE
					float4 vertColor2,
				#endif
					float4x3 worldMtx,
					out float3 worldPos,
					out float3 worldEyePos,
					out float3 worldNormal,
					out float4 color0
				#if PALETTE_TINT
					,out float4 color1
				#endif
					)
{
	worldPos		= mul(float4(localPos,1), worldMtx);
	worldEyePos		= gViewInverse[3].xyz - worldPos;
	worldNormal		= NORMALIZE_VS(mul(localNormal, (float3x3)worldMtx));
	color0			= vertColor;
#if PALETTE_TINT
	#if PALETTE_TINT_EDGE
		color1		= UnpackTintPalette(vertColor2);
	#else
		color1		= UnpackTintPalette(vertColor);
	#endif
#endif
}


//
//
//
//
void rtsProcessVertLightingSkinned(
					float3		localPos,
					float3		localNormal,
	#if NORMAL_MAP
					float4		localTangent,
	#endif // NORMAL_MAP
					float4		vertColor,
	#if PALETTE_TINT_EDGE
					float4		vertColor2,
	#endif
					rageSkinMtx skinMtx, 
					float4x3	worldMtx,
					out float3	worldPos,
					out float3	worldEyePos,
					out float3	worldNormal,
	#if NORMAL_MAP
					out float3	worldBinormal,
					out float3	worldTangent,
		#if PARALLAX
					out float4	tanEyePos,
		#endif
	#endif	// NORMAL_MAP
					out float4	color0
	#if PALETTE_TINT
					,out float4	color1
	#endif
					)
{
	float3 skinnedPos	= rageSkinTransform(localPos, skinMtx);
	worldPos			= mul(float4(skinnedPos,1), worldMtx);

	// Transform normal by "transpose" matrix
	float3 skinnedNormal= rageSkinRotate(localNormal, skinMtx);
	worldNormal			= NORMALIZE_VS(mul(skinnedNormal, (float3x3)worldMtx));

	// Store position in eyespace
	worldEyePos = gViewInverse[3].xyz - worldPos;

	#if NORMAL_MAP
		// Do the tangent+binormal stuff
		float3 skinnedTangent	= rageSkinRotate(localTangent.xyz, skinMtx);
		worldTangent			= NORMALIZE_VS(mul(skinnedTangent, (float3x3)worldMtx));
	#endif // NORMAL_MAP

	#if PALETTE_TINT
		#if PALETTE_TINT_EDGE
			color1 = UnpackTintPalette(vertColor2);
		#else
			color1 = UnpackTintPalette(vertColor);
		#endif
	#endif

	color0 = vertColor;

#if NORMAL_MAP
	// Do the tangent+binormal stuff
	worldBinormal	= rageComputeBinormal(worldNormal, worldTangent, localTangent.w);
	#if PARALLAX
		// transform EyePos into tangent space:
		tanEyePos.x = dot(worldTangent.xyz, worldEyePos.xyz);
		tanEyePos.y = dot(worldBinormal.xyz,worldEyePos.xyz);
		tanEyePos.z = dot(worldNormal.xyz,	worldEyePos.xyz);
		tanEyePos.w = 1.0f;
	#endif //PARALLAX || PARALLAX_MAP_V2
#endif // NORMAL_MAP
}

//
//
//
//
float3 CalculateProjTexZShift(float3 inPos, megaVertInstanceData inst)
{
	float3 outPos = inPos;

#if PROJTEX_ZSHIFT_SUPPORT

	float3 localCamPos;

	float3 tmp = gViewInverse[3].xyz - (float3)inst.worldMtx[3];
	localCamPos.x = dot((float3)inst.worldMtx[0], tmp);
	localCamPos.y = dot((float3)inst.worldMtx[1], tmp);
	localCamPos.z = dot((float3)inst.worldMtx[2], tmp);

	float3 vecShift = (normalize(localCamPos - inPos)) * zShiftScale;
	outPos += vecShift;
#endif // PROJTEX_ZSHIFT_SUPPORT...

	return(outPos);
}


//
//
// all alpha have a z bias and sub pixel offset, set bInvertedProj to true if you are
// using an inverted-depth projection matrix (not needed if you are inverting depth
// via the viewport transform, or if you aren't inverting depth at all). currently
// we only use inverted-depth on __XENON and even then do the inversion with the proj
// matrix during GBuffer generation (all other passes invert with the viewport xform)
//
float4 AddEmissiveZBias(float4 inPos, bool bInvertedProj, float multiplier)
{
	float4 outPos = inPos;

	float direction = bInvertedProj ? -1.0f : 1.0f;

#if !__MAX
	#if (EMISSIVE || EMISSIVE_ADDITIVE)
		outPos.z	-= multiplier * direction * 0.00001f;
	#endif
#endif // !__MAX

	return(outPos);
}

#if defined(DISPLACEMENT_SHADER)
float4 PS_ApplyDisplacement(float4		inputOut,
							float2		IN_diffuseTexCoord,	sampler2D	IN_diffuseSampler,
							float2		IN_displTexCoord,	sampler2D	IN_displSampler,
							float3		IN_worldEyePos)
{
float4 OUT = inputOut;

	float2 vPos = IN_displTexCoord;
	float2 displDiff = tex2D(IN_diffuseSampler, IN_diffuseTexCoord);
	float2 displUV = float2(displDiff.x*gDisplScaleU,displDiff.y*gDisplScaleV);
	// scale displacement - up to 15m away from camera:
	float displScale = saturate( (gDisplMaxCamDist-length(IN_worldEyePos))/gDisplMaxCamDist );
	displUV *= displScale;

	#if __XENON
		vPos += float2(0.5, 0.5);
	#endif

	float2 texDisplUV = vPos.xy + displUV;
	texDisplUV.xy = texDisplUV.xy/float2(gScreenSize.x,gScreenSize.y);

	#if __PS3
		float3 displColour = UnpackColorLinearise(tex2Dlod(IN_displSampler, saturate(float4(texDisplUV,0,0))));
	#elif __XENON
		// Unpack the 7e3Int buffer. PointSample7e3 is *much* cheaper than LinearSample7e3, hopefully it's good enough!
		float3 displColour = UnpackHdr_3h(PointSample7e3(IN_displSampler, saturate(texDisplUV), true).rgb);   // <-- point sampling 7e3packed, fastest option
		//float3 displColour = UnpackHdr_3h(LinearSample7e3(IN_displSampler, texDisplUV, true).rgb);          // <-- better filtering but way more expensive
		//float3 displColour = UnpackHdr_3h(tex2Dlod(IN_displSampler, saturate(float4(texDisplUV,0,0))).rgb); // <-- the old way, sampling from 16f HDR
	#else
		float3 displColour = tex2Dlod(IN_displSampler, saturate(float4(texDisplUV,0,0)));
	#endif
	OUT.rgb = lerp(displColour, OUT.rgb, OUT.a);
	OUT.a	= 1.0f;
	return OUT;
}
#endif //DISPLACEMENT_SHADER...

#if SECOND_SPECULAR_LAYER
//
//
// calculates specular color factor for 1 single light;
// used for second specular layer;
//
float3 PS_GetSingleDirectionalLightSpecular2Color(
	float3	surfaceNormal,
	float3	surfaceToEyeDir,
	float	specularExponent,
	float3	lightDir,
	float4	lightColorIntensity
	)
{
	float3 surfaceToLightDir = -lightDir;
	float specScale = calculateBlinnPhong(surfaceNormal, normalize(surfaceToLightDir + surfaceToEyeDir), specularExponent);
	float3 OUT = (lightColorIntensity.rgb * lightColorIntensity.w * specScale);
	return(OUT);
}
#endif //SECOND_SPECULAR_LAYER...

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ====== VERTEX SHADERS ===========
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//	DRAW METHODS
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//
//
//

vertexOutput VS_Transform_Common(northInstancedVertexInputBump instIN
#ifdef RAGE_ENABLE_OFFSET
	, rageVertexOffsetBump INOFFSET, uniform float bEnableOffsets
#endif
#if RAGE_INSTANCED_TECH
	, uniform bool bUseInst
#endif
)
{
	int nInstIndex;
	northVertexInputBump IN;
	FetchInstanceVertexData(instIN, IN, nInstIndex);

	megaVertInstanceData inst;
	GetInstanceData(nInstIndex, inst);

	vertexOutput OUT;

	float3 inPos = DAY_NIGHT_VERTEX_SCALE*IN.pos;
	float3 inNrm = IN.normal;
	float4 inCpv = IN.diffuse;

#if NORMAL_MAP
	float4 inTan = IN.tangent;
#endif

#ifdef RAGE_ENABLE_OFFSET
	if( bEnableOffsets != 0.0f )
	{
		inPos += INOFFSET.pos;
		inNrm += INOFFSET.normal;
		inCpv += INOFFSET.diffuse;
	#if NORMAL_MAP
		inTan += INOFFSET.tangent;
	#endif
	}
#endif //RAGE_ENABLE_OFFSET...


#ifdef USE_VEHICLE_DAMAGE
	float3 inputPos		= inPos;
	float3 inputNormal	= inNrm;
	ApplyVehicleDamage(inputPos, inputNormal, VEHICLE_DAMAGE_SCALE, inPos, inNrm);
#endif //USE_VEHICLE_DAMAGE...

	float3 pvlWorldNormal, pvlWorldPosition, t_worldEyePos;

	rtsProcessVertLightingUnskinned(inPos, 
									inNrm,
#if NORMAL_MAP
									inTan,
#endif
									inCpv,
#if PALETTE_TINT_EDGE
									IN.specular,
#endif
									(float4x3) inst.worldMtx,
									pvlWorldPosition,
#if REFLECT
									OUT.worldEyePos,
#else
									t_worldEyePos,
#endif
									pvlWorldNormal,
#if NORMAL_MAP
									OUT.worldBinormal,
									OUT.worldTangent,
	#if PARALLAX
									OUT.tanEyePos,
	#endif
#endif // NORMAL_MAP
									OUT.color0 
#if PALETTE_TINT
									,OUT.color1
#endif
									);
	// Write out final position & texture coords
	inPos	= CalculateProjTexZShift(inPos, inst);

#if RAGE_INSTANCED_TECH
	if (bUseInst)
		OUT.pos = DAY_NIGHT_VERTEX_SCALE*mul(float4(inPos,1), mul(gInstWorld,gInstWorldViewProj[INSTOPT_INDEX(IN.InstID)]));
	else
#endif
		OUT.pos = DAY_NIGHT_VERTEX_SCALE*ApplyCompositeWorldTransform(float4(inPos,1), INSTANCING_ONLY_ARG(inst.worldMtx) gWorldViewProj);

	OUT.worldNormal		= pvlWorldNormal;

	// NOTE: These 3 statements may resolve to the exact same code -- rely on HLSL to strip worthless code
#if DIFFUSE_TEXTURE
	DIFFUSE_VS_OUTPUT	= AnimateUVs(DIFFUSE_VS_INPUT);
#endif
#if SPEC_MAP
	SPECULAR_VS_OUTPUT	= AnimateUVs(SPECULAR_VS_INPUT);
#endif	// SPEC_MAP
#if NORMAL_MAP
	OUT.texCoord.xy		= AnimateUVs(IN.texCoord0.xy);
#endif	// NORMAL_MAP

#if DIFFUSE2
	// Pack in 2nd set of texture coords if needed
	OUT.texCoord.zw = IN.texCoord1.xy;
#endif	// DIFFUSE2

	OUT.worldPos = float4(pvlWorldPosition,1);

#if REFLECT_MIRROR
	OUT.screenPos = OUT.pos;
#endif

	// Adjust ambient bakes
#if REFLECT
	OUT.color0.rg = CalculateBakeAdjust(OUT.worldEyePos, OUT.color0.rg);
#else
	OUT.color0.rg = CalculateBakeAdjust(t_worldEyePos, OUT.color0.rg);
#endif

#if EDGE_WEIGHT
	OUT.edgeWeight.xy = IN.CHANNEL2_SRC;
#endif

#if OVERLAY_DIFFUSE2	
#if SECONDARY_TEXCOORDS_AS_OVERLAY_DIFFUSE2_COORDS
	OUT.overlayDiffuse2Coords = IN.CHANNEL1_SRC;
#else
	#error "No overlayDiffuse2Coords set!"
#endif
#endif
	OUT.pos = DAY_NIGHT_VERTEX_SCALE*AddEmissiveZBias(OUT.pos, false, 1.0f); // INSTANCED SHADOWS

	RAGE_COMPUTE_CLIP_DISTANCES(OUT.pos);
	OUT.pos = ApplyDayNightNan(OUT.pos, inst.worldMtx);

#if BATCH_INSTANCING
	OUT.batchTint = float4(inst.inst.color_scale.rgb, IN.specular.r);
	OUT.batchAo = inst.inst.ao_pad3.x;
	OUT.color0.a *= inst.csData.CrossFade * inst.csData.Scale;
#endif //BATCH_INSTANCING
	
	return OUT;
}

vertexOutput VS_Transform_Base(northVertexInputBump IN, megaVertInstanceData inst, int nInstIndex
#ifdef RAGE_ENABLE_OFFSET
	, rageVertexOffsetBump INOFFSET, uniform float bEnableOffsets
#endif
)
{
	vertexOutput OUT;

	float3 inPos = IN.pos;
	float3 inNrm = IN.normal;
	float4 inCpv = IN.diffuse;

#if NORMAL_MAP
	float4 inTan = IN.tangent;
#endif

#ifdef RAGE_ENABLE_OFFSET
	if( bEnableOffsets != 0.0f )
	{
		inPos += INOFFSET.pos;
		inNrm += INOFFSET.normal;
		inCpv += INOFFSET.diffuse;
	#if NORMAL_MAP
		inTan += INOFFSET.tangent;
	#endif
	}
#endif //RAGE_ENABLE_OFFSET...


#ifdef USE_VEHICLE_DAMAGE
	float3 inputPos		= inPos;
	float3 inputNormal	= inNrm;
	ApplyVehicleDamage(inputPos, inputNormal, VEHICLE_DAMAGE_SCALE, inPos, inNrm);
#endif //USE_VEHICLE_DAMAGE...

	float3 pvlWorldNormal, pvlWorldPosition, t_worldEyePos;

	rtsProcessVertLightingUnskinned(inPos, 
									inNrm,
#if NORMAL_MAP
									inTan,
#endif
									inCpv,
#if PALETTE_TINT_EDGE
									IN.specular,
#endif
									(float4x3) inst.worldMtx,
									pvlWorldPosition,
#if REFLECT
									OUT.worldEyePos,
#else
									t_worldEyePos,
#endif
									pvlWorldNormal,
#if NORMAL_MAP
									OUT.worldBinormal,
									OUT.worldTangent,
	#if PARALLAX
									OUT.tanEyePos,
	#endif
#endif // NORMAL_MAP
									OUT.color0 
#if PALETTE_TINT
									,OUT.color1
#endif
									);
	// Write out final position & texture coords
	inPos	= CalculateProjTexZShift(inPos, inst);
#ifdef NVSTEREO
	float4 tPos = ApplyCompositeWorldTransform(float4(inPos,1), INSTANCING_ONLY_ARG(inst.worldMtx) gWorldViewProj);
	if (gStereoPhone)
		OUT.pos = DAY_NIGHT_VERTEX_SCALE*StereoToMonoClipSpace(tPos);
	else
		OUT.pos = DAY_NIGHT_VERTEX_SCALE*tPos;
#else
	OUT.pos = DAY_NIGHT_VERTEX_SCALE*ApplyCompositeWorldTransform(float4(inPos,1), INSTANCING_ONLY_ARG(inst.worldMtx) gWorldViewProj);
#endif
	OUT.worldNormal		= pvlWorldNormal;

	// NOTE: These 3 statements may resolve to the exact same code -- rely on HLSL to strip worthless code
#if DIFFUSE_TEXTURE
	DIFFUSE_VS_OUTPUT	= AnimateUVs(DIFFUSE_VS_INPUT);
#endif
#if SPEC_MAP
	SPECULAR_VS_OUTPUT	= AnimateUVs(SPECULAR_VS_INPUT);
#endif	// SPEC_MAP
#if NORMAL_MAP
	OUT.texCoord.xy		= AnimateUVs(IN.texCoord0.xy);
#endif	// NORMAL_MAP

#if DIFFUSE2
	// Pack in 2nd set of texture coords if needed
	OUT.texCoord.zw = IN.texCoord1.xy;
#endif	// DIFFUSE2

	OUT.worldPos = float4(pvlWorldPosition,1);

#if REFLECT_MIRROR
	OUT.screenPos = OUT.pos;
#endif

#if UI_TECHNIQUES
	OUT.pos = AddEmissiveZBias(OUT.pos, SUPPORT_INVERTED_PROJECTION ? true : false, 0.1f); // FORWARD
#else
	OUT.pos = AddEmissiveZBias(OUT.pos, SUPPORT_INVERTED_PROJECTION ? true : false, 1.0f); // FORWARD
#endif

	// Adjust ambient bakes
#if REFLECT
	OUT.color0.rg = CalculateBakeAdjust(OUT.worldEyePos.xyz, OUT.color0.rg);
#else
	OUT.color0.rg = CalculateBakeAdjust(t_worldEyePos.xyz, OUT.color0.rg);
#endif
#if DISPLACEMENT_LOD_DEBUG
	OUT.color0 = IN.diffuse;
#endif
#if EDGE_WEIGHT
	OUT.edgeWeight.xy = IN.CHANNEL2_SRC;
#endif

#if OVERLAY_DIFFUSE2	
#if SECONDARY_TEXCOORDS_AS_OVERLAY_DIFFUSE2_COORDS
	OUT.overlayDiffuse2Coords = IN.CHANNEL1_SRC;
#else
	#error "No overlayDiffuse2Coords set!"
#endif
#endif

	RAGE_COMPUTE_CLIP_DISTANCES(OUT.pos);
	OUT.pos = ApplyDayNightNan(OUT.pos, inst.worldMtx);

#if BATCH_INSTANCING
	OUT.batchTint = float4(inst.inst.color_scale.rgb, IN.specular.r);
	OUT.batchAo = inst.inst.ao_pad3.x;
	OUT.color0.a *= inst.csData.CrossFade * inst.csData.Scale;
#endif //BATCH_INSTANCING

	return OUT;
}// end of VS_Transform_Base()...

vertexOutput VS_Transform(northInstancedVertexInputBump instIN
#ifdef RAGE_ENABLE_OFFSET
						  , rageVertexOffsetBump INOFFSET, uniform float bEnableOffsets
#endif
						  )
{
	int nInstIndex;
	northVertexInputBump IN;
	FetchInstanceVertexData(instIN, IN, nInstIndex);

	megaVertInstanceData inst;
	GetInstanceData(nInstIndex, inst);

	return VS_Transform_Base(IN, inst, nInstIndex
#ifdef RAGE_ENABLE_OFFSET
							, INOFFSET, bEnableOffsets
#endif
		);
}

#if APPLY_DOF_TO_ALPHA_DECALS
vertexOutputDOF VS_Transform_DOF(northInstancedVertexInputBump instIN
#ifdef RAGE_ENABLE_OFFSET
						  , rageVertexOffsetBump INOFFSET, uniform float bEnableOffsets
#endif
						  )
{
	vertexOutput OutTransform = VS_Transform(instIN
	#ifdef RAGE_ENABLE_OFFSET
						, INOFFSET, gbOffsetEnable
	#endif
						);

	vertexOutputDOF OUT;
	OUT.vertOutput = OutTransform;
	OUT.linearDepth = OutTransform.pos.w;

	return OUT;
}
#endif //APPLY_DOF_TO_ALPHA_DECALS

#if USE_PN_TRIANGLES

// [domain("tri")]
// vertexOutput DS_Transform_PNTri(rage_PNTri_PatchInfo PatchInfo, float3 WUV : SV_DomainLocation, const OutputPatch<northVertexInputBump_CtrlPoint, 3> PatchPoints)
// {
// 	// Evaluate the patch at coords WUV.
// 	northVertexInputBump Arg = northVertexInputBump_PNTri_EvaluateAt(PatchInfo, WUV, PatchPoints);
// 
// 	int nInstIndex = PNTri_EvaluateInstanceID(WUV, PatchPoints);
// 	megaVertInstanceData inst;
// 	GetInstanceData(nInstIndex, inst);
// 
// 	// Transform as per the original vertex shader.
// #ifdef RAGE_ENABLE_OFFSET
// 	return VS_Transform_Base(Arg, inst, nInstIndex, INOFFSET, bEnableOffsets);
// #else
// 	return VS_Transform_Base(Arg, inst, nInstIndex);
// #endif
// }

#endif // USE_PN_TRIANGLES


vertexOutputSeeThrough VS_Transform_SeeThrough(vertexInputSeeThrough IN)
{
	vertexOutputSeeThrough OUT;

	OUT = SeeThrough_GenerateOutput(DAY_NIGHT_VERTEX_SCALE*IN.pos);
	OUT.pos = ApplyDayNightNan(OUT.pos);

	return OUT;
}


// =============================================================================================== //
//
//
//
//
vertexOutputD VS_TransformD_Base(northVertexInputBump IN, megaVertInstanceData inst, int nInstIndex
#ifdef RAGE_ENABLE_OFFSET
	, rageVertexOffsetBump INOFFSET, uniform float bEnableOffsets
#endif
	)
{
	vertexOutputD OUT;

	float3 inPos = DAY_NIGHT_VERTEX_SCALE*IN.pos;
	float3 inNrm = IN.normal;
	float4 inCpv = IN.diffuse;

#if WIND_DISPLACEMENT
	BRANCH_BEND_PROPERTIES bendProperties;

	bendProperties.PivotHeight = branchBendPivot.x;
	bendProperties.TrunkStiffnessAdjustLow	= branchBendStiffnessAdjust.x;
	bendProperties.TrunkStiffnessAdjustHigh = branchBendStiffnessAdjust.y;
	bendProperties.PhaseStiffnessAdjustLow	= branchBendStiffnessAdjust.z;
	bendProperties.PhaseStiffnessAdjustHigh = branchBendStiffnessAdjust.w;

#if USE_ERIKS_TEMP_UMOVEMENT_COLOURS
	float4 uMovementColour = IN.specular.bgbr;
#else
	float4 uMovementColour = IN.specular;
#endif

	inPos = ComputeBranchBend(IN.pos.xyz, inst.worldMtx, uMovementColour, gUmGlobalTimer, bendProperties);

#if !SHADER_FINAL
	OUT.windDebug = CalcWindDebugColour(uMovementColour, bendProperties);
#endif

#endif // WIND_DISPLACEMENT

#if NORMAL_MAP
	float4 inTan = IN.tangent;
#endif

#ifdef RAGE_ENABLE_OFFSET
	if( bEnableOffsets != 0.0f )
	{
		inPos += INOFFSET.pos;
		inNrm += INOFFSET.normal;
		inCpv += INOFFSET.diffuse;
	#if NORMAL_MAP
		inTan += INOFFSET.tangent;
	#endif
	}
#endif

#ifdef USE_VEHICLE_DAMAGE
	float3 inputPos		= inPos;
	float3 inputNormal	= inNrm;
	ApplyVehicleDamage(inputPos, inputNormal, VEHICLE_DAMAGE_SCALE, inPos, inNrm);
#endif //USE_VEHICLE_DAMAGE...

	float3 pvlWorldNormal, pvlWorldPosition, t_worldEyePos;
	float4 pvlColor0;

	rtsProcessVertLightingUnskinned(inPos, 
									inNrm,
#if NORMAL_MAP
									inTan,
#endif
									inCpv,
#if PALETTE_TINT_EDGE
									IN.specular,
#endif
									(float4x3) inst.worldMtx,
									pvlWorldPosition,
#if VERTEXOUTPUTD_WORLDEYEPOS
									OUT.worldEyePos,
#else
									t_worldEyePos,
#endif
									pvlWorldNormal,
#if NORMAL_MAP
									OUT.worldBinormal,
									OUT.worldTangent,
	#if PARALLAX
									OUT.tanEyePos,
	#endif
#endif // NORMAL_MAP
									pvlColor0
#if PALETTE_TINT
									,OUT.color1
#endif
									);

#if UMOVEMENTS
	inPos	= VS_ApplyMicromovements(inPos, IN.specular.rgb);
#endif
#if UMOVEMENTS_TEX
	inPos	= VS_ApplyMicromovements(inPos, IN.texCoord1.xxy);
#endif
	// Write out final position & texture coords
	inPos	= CalculateProjTexZShift(inPos, inst);
	OUT.pos = DAY_NIGHT_VERTEX_SCALE*ApplyCompositeWorldTransform(float4(inPos,1), INSTANCING_ONLY_ARG(inst.worldMtx) gWorldViewProj);

#if VERTEXOUTPUTD_COLOR0
	OUT.color0 = pvlColor0;
#else
	OUT.color0 = 1.0f.xxxx;
#endif
#if VERTEXOUTPUTD_NORMAL
	OUT.worldNormal		= pvlWorldNormal;
#endif //VERTEXOUTPUTD_NORMAL...

	// NOTE: These 3 statements may resolve to the exact same code -- rely on HLSL to strip worthless code
#if DIFFUSE_TEXTURE
	DIFFUSE_VS_OUTPUT	= AnimateUVs(DIFFUSE_VS_INPUT);
#endif
#if SPEC_MAP
	SPECULAR_VS_OUTPUT	= AnimateUVs(SPECULAR_VS_INPUT);
#endif	// SPEC_MAP
#if NORMAL_MAP
	OUT.texCoord.xy		= AnimateUVs(IN.texCoord0.xy);
#endif	// NORMAL_MAP

#if DIFFUSE2
	// Pack in 2nd set of texture coords if needed
	OUT.texCoord.zw = IN.texCoord1.xy;
#endif	// DIFFUSE2

	OUT.pos = DAY_NIGHT_VERTEX_SCALE*AddEmissiveZBias(OUT.pos, SUPPORT_INVERTED_PROJECTION ? true : false, 1.0f); // GBUFF

	// Adjust ambient bakes
#if VERTEXOUTPUTD_WORLDEYEPOS
	OUT.color0.rg = CalculateBakeAdjust(OUT.worldEyePos, OUT.color0.rg);
#else
	OUT.color0.rg = CalculateBakeAdjust(t_worldEyePos, OUT.color0.rg);
#endif

#if EDGE_WEIGHT
	OUT.edgeWeight.xy = IN.CHANNEL2_SRC;
#endif

#if OVERLAY_DIFFUSE2	
#if SECONDARY_TEXCOORDS_AS_OVERLAY_DIFFUSE2_COORDS
	OUT.overlayDiffuse2Coords = IN.CHANNEL1_SRC;
#else
	#error "No overlayDiffuse2Coords set!"
#endif
#endif

	OUT.pos = ApplyDayNightNan(OUT.pos, inst.worldMtx);

#if BATCH_INSTANCING
	OUT.batchTint = float4(inst.inst.color_scale.rgb, IN.specular.r);
	OUT.batchAo = inst.inst.ao_pad3.x;
	OUT.color0.a *= inst.csData.CrossFade * inst.csData.Scale;
#endif //BATCH_INSTANCING

	return OUT;
}// end of VS_TransformD_Base()...

vertexOutputD VS_TransformD(northInstancedVertexInputBump instIN
#ifdef RAGE_ENABLE_OFFSET
							, rageVertexOffsetBump INOFFSET, uniform float bEnableOffsets
#endif
							)
{
	int nInstIndex;
	northVertexInputBump IN;
	FetchInstanceVertexData(instIN, IN, nInstIndex);

	megaVertInstanceData inst;
	GetInstanceData(nInstIndex, inst);

	return VS_TransformD_Base(IN, inst, nInstIndex
#ifdef RAGE_ENABLE_OFFSET
		, INOFFSET, bEnableOffsets
#endif
		);
}

#if USE_PN_TRIANGLES

[domain("tri")]
vertexOutputD DS_TransformD_PNTri(rage_PNTri_PatchInfo PatchInfo, float3 WUV : SV_DomainLocation, const OutputPatch<northVertexInputBump_CtrlPoint, 3> PatchPoints)
{
	// Evaluate the patch at coords WUV.
	northVertexInputBump Arg = northVertexInputBump_PNTri_EvaluateAt(PatchInfo, WUV, PatchPoints);

	int nInstIndex = PNTri_EvaluateInstanceID(WUV, PatchPoints);
	megaVertInstanceData inst;
	GetInstanceData(nInstIndex, inst);

	// Transform as per the original vertex shader.
#ifdef RAGE_ENABLE_OFFSET
	return VS_TransformD_Base(Arg, inst, nInstIndex, INOFFSET, bEnableOffsets);
#else
	return VS_TransformD_Base(Arg, inst, nInstIndex);
#endif
}

#endif // USE_PN_TRIANGLES

// =============================================================================================== //

//
//
//
//
vertexOutputUnlit VS_TransformUnlit(northInstancedVertexInput instIN
#ifdef RAGE_ENABLE_OFFSET
	, rageVertexOffsetBump INOFFSET, uniform float bEnableOffsets
#endif
	)
{
	int nInstIndex;
	northVertexInput IN;
	FetchInstanceVertexData(instIN, IN, nInstIndex);

	megaVertInstanceData inst;
	GetInstanceData(nInstIndex, inst);

	vertexOutputUnlit OUT;

	float3 inPos = IN.pos;
	float4 inCpv = IN.diffuse;

#ifdef RAGE_ENABLE_OFFSET
	if( bEnableOffsets != 0.0f )
	{
		inPos += INOFFSET.pos;
		inCpv += INOFFSET.diffuse;
	}
#endif

#ifdef USE_VEHICLE_DAMAGE
	float3 inputPos		= inPos;
	ApplyVehicleDamage(inputPos, VEHICLE_DAMAGE_SCALE, inPos);
#endif //USE_VEHICLE_DAMAGE...


	// Write out final position & texture coords
	OUT.pos =  DAY_NIGHT_VERTEX_SCALE*ApplyCompositeWorldTransform(float4(inPos,1), INSTANCING_ONLY_ARG(inst.worldMtx) gWorldViewProj);
#if (defined(NVSTEREO) && defined(USE_MINIMAP_STEREO))
	if (OUT.pos.w != 1.0f)
	{
		OUT.pos = StereoToMonoClipSpace(OUT.pos);
	}
#endif

	// NOTE: These 3 statements may resolve to the exact same code -- rely on HLSL to strip worthless code
#if DIFFUSE_TEXTURE
	DIFFUSE_VS_OUTPUT	= AnimateUVs(DIFFUSE_VS_INPUT);
#endif
#if SPEC_MAP
	SPECULAR_VS_OUTPUT	= AnimateUVs(SPECULAR_VS_INPUT);
#endif	// SPEC_MAP
#if NORMAL_MAP
	OUT.texCoord.xy		= AnimateUVs(IN.texCoord0.xy);
#endif	// NORMAL_MAP

#if DIFFUSE2
	// Pack in 2nd set of texture coords if needed
	OUT.texCoord.zw = IN.texCoord1.xy;
#endif	// DIFFUSE2

	OUT.color0 = saturate(inCpv);
#if PALETTE_TINT
	#if PALETTE_TINT_EDGE
		OUT.color1 = UnpackTintPalette(IN.specular);
	#else
		OUT.color1 = UnpackTintPalette(inCpv);
	#endif
#endif

	OUT.pos = DAY_NIGHT_VERTEX_SCALE*AddEmissiveZBias(OUT.pos,false, 1.0f); // UNLIT
	OUT.pos = ApplyDayNightNan(OUT.pos, inst.worldMtx);

#if BATCH_INSTANCING
	OUT.batchTint = float4(inst.inst.color_scale.rgb, IN.specular.r);
	OUT.batchAo = inst.inst.ao_pad3.x;
	OUT.color0.a *= inst.csData.CrossFade * inst.csData.Scale;
#endif //BATCH_INSTANCING

	return OUT;
}// end of VS_TransformUnlit()...


//
//
//
//
vertexOutputWaterReflection VS_TransformWaterReflection(northInstancedVertexInputBump instIN)
{
	int nInstIndex;
	northVertexInputBump IN;
	FetchInstanceVertexData(instIN, IN, nInstIndex);

	megaVertInstanceData inst;
	GetInstanceData(nInstIndex, inst);

	vertexOutputWaterReflection OUT;

	float3 inPos = IN.pos;
	float3 inNrm = IN.normal;
	float4 inCpv = IN.diffuse;

	float3 pvlWorldNormal, pvlWorldPosition, pvlWorldEyePos;

	rtsProcessVertLightingBasic(
		inPos,
		inNrm,
		inCpv,
#if PALETTE_TINT_WATER_REFLECTION_EDGE
		IN.specular,
#elif PALETTE_TINT_EDGE
		1,
#endif
		(float4x3) inst.worldMtx,
		pvlWorldPosition,
		pvlWorldEyePos,
		pvlWorldNormal,
		OUT.color0
#if PALETTE_TINT
		,OUT.color1
#endif
		);

	// Write out final position & texture coords
	OUT.pos = DAY_NIGHT_VERTEX_SCALE*ApplyCompositeWorldTransform(float4(inPos,1), INSTANCING_ONLY_ARG(inst.worldMtx) gWorldViewProj);
	OUT.worldNormal	= pvlWorldNormal;
	OUT.worldPos = pvlWorldPosition;

#if DIFFUSE_TEXTURE
	DIFFUSE_VS_OUTPUT = AnimateUVs(DIFFUSE_VS_INPUT);
#endif
#if DIFFUSE2
	// Pack in 2nd set of texture coords if needed
	OUT.texCoord.zw = IN.texCoord1.xy;
#endif // DIFFUSE2

	OUT.color0 = saturate(OUT.color0);

	RAGE_COMPUTE_CLIP_DISTANCES(OUT.pos);
	OUT.pos = ApplyDayNightNan(OUT.pos, inst.worldMtx);

#if BATCH_INSTANCING
	OUT.batchTint = float4(inst.inst.color_scale.rgb, IN.specular.r);
	OUT.batchAo = inst.inst.ao_pad3.x;
	OUT.color0.a *= inst.csData.CrossFade * inst.csData.Scale;
#endif //BATCH_INSTANCING

	return OUT;
}// end of VS_TransformWaterReflection()...


#if WATER_REFLECTION_TECHNIQUES && DRAWSKINNED_TECHNIQUES
//
//
//
//
vertexOutputWaterReflection VS_TransformSkinWaterReflection(northSkinVertexInputBump IN)
{
	vertexOutputWaterReflection OUT;

	const rageSkinMtx skinMtx = ComputeSkinMtx(IN.blendindices, IN.weight);

	float3 inPos = rageSkinTransform(IN.pos, skinMtx);
	float3 inNrm = rageSkinRotate(IN.normal, skinMtx);
	float4 inCpv = IN.diffuse;

	float3 pvlWorldNormal, pvlWorldPosition, pvlWorldEyePos;

	rtsProcessVertLightingBasic(
		inPos,
		inNrm,
		inCpv,
#if PALETTE_TINT_WATER_REFLECTION_EDGE
		IN.specular,
#elif PALETTE_TINT_EDGE
		1,
#endif
		(float4x3) gWorld,
		pvlWorldPosition,
		pvlWorldEyePos,
		pvlWorldNormal,
		OUT.color0
#if PALETTE_TINT
		,OUT.color1
#endif
		);

	// Write out final position & texture coords
	OUT.pos =  DAY_NIGHT_VERTEX_SCALE*mul(float4(inPos,1), gWorldViewProj);
	OUT.worldNormal	= pvlWorldNormal;
	OUT.worldPos = pvlWorldPosition;

#if DIFFUSE_TEXTURE
	DIFFUSE_VS_OUTPUT = AnimateUVs(DIFFUSE_VS_INPUT);
#endif
#if DIFFUSE2
	// Pack in 2nd set of texture coords if needed
	OUT.texCoord.zw = IN.texCoord1.xy;
#endif // DIFFUSE2

	OUT.color0 = saturate(OUT.color0);

	RAGE_COMPUTE_CLIP_DISTANCES(OUT.pos);
	OUT.pos = ApplyDayNightNan(OUT.pos);

	return OUT;
}// end of VS_TransformSkinWaterReflection()...

#endif	// WATER_REFLECTION_TECHNIQUES && DRAWSKINNED_TECHNIQUES

#if CUBEMAP_REFLECTION_TECHNIQUES
vertexMegaOutputCube VS_TransformCube_Common(northInstancedVertexInputBump instIN
#if GS_INSTANCED_CUBEMAP
								  , uniform bool bUseInst
#endif
								  )
{
	int nInstIndex;
	northVertexInputBump IN;
	FetchInstanceVertexData(instIN, IN, nInstIndex);

	megaVertInstanceData inst;
	GetInstanceData(nInstIndex, inst);

	vertexMegaOutputCube OUT;

	float3 inPos = IN.pos;
	float3 inNrm = IN.normal;
	float4 inCpv = IN.diffuse;

	float3 pvlWorldNormal, pvlWorldPosition, pvlWorldEyePos;

	float4x3 worldMat;
#if GS_INSTANCED_CUBEMAP
	if (bUseInst)
		worldMat = (float4x3) gInstWorld;
	else
#endif
		worldMat = (float4x3) inst.worldMtx;

	rtsProcessVertLightingBasic(
		inPos, 
		inNrm,
		inCpv,
		#if PALETTE_TINT_EDGE
			IN.specular,
		#endif
		worldMat,
		pvlWorldPosition,
		pvlWorldEyePos,
		pvlWorldNormal,
		OUT.color0
		#if PALETTE_TINT
			,OUT.color1
		#endif
		);

	// Write out final position & texture coords
#if GS_INSTANCED_CUBEMAP
	if (bUseInst)
		OUT.pos = DAY_NIGHT_VERTEX_SCALE*mul(float4(inPos,1), mul(gInstWorld,gInstWorldViewProj[INSTOPT_INDEX(nInstIndex)]));
	else
#endif
		OUT.pos = DAY_NIGHT_VERTEX_SCALE*ApplyCompositeWorldTransform(float4(inPos,1), INSTANCING_ONLY_ARG(inst.worldMtx) gWorldViewProj);

	OUT.worldNormal	= pvlWorldNormal;
	OUT.worldPos = pvlWorldPosition;

	// NOTE: These 3 statements may resolve to the exact same code -- rely on HLSL to strip worthless code
	#if DIFFUSE_TEXTURE
		DIFFUSE_VS_OUTPUT	= AnimateUVs(DIFFUSE_VS_INPUT);
	#endif
	#if SPEC_MAP
		SPECULAR_VS_OUTPUT	= AnimateUVs(SPECULAR_VS_INPUT);
	#endif	// SPEC_MAP
	#if NORMAL_MAP
		OUT.texCoord.xy		= AnimateUVs(IN.texCoord0.xy);
	#endif	// NORMAL_MAP

	#if DIFFUSE2
		// Pack in 2nd set of texture coords if needed
		OUT.texCoord.zw = IN.texCoord1.xy;
	#endif	// DIFFUSE2

	OUT.color0.rg = saturate(CalculateBakeAdjust(pvlWorldEyePos.xyz, OUT.color0.rg));

	OUT.pos = DAY_NIGHT_VERTEX_SCALE*AddEmissiveZBias(OUT.pos, false, 2.0f); // CUBEMAP
	OUT.pos = ApplyDayNightNan(OUT.pos, inst.worldMtx);

#if BATCH_INSTANCING
	OUT.batchTint = float4(inst.inst.color_scale.rgb, IN.specular.r);
	OUT.batchAo = inst.inst.ao_pad3.x;
	OUT.color0.a *= inst.csData.CrossFade * inst.csData.Scale;
#endif //BATCH_INSTANCING

	return OUT;
} // end of VS_TransformCube()...

vertexMegaOutputCube VS_TransformCube(northInstancedVertexInputBump IN)
{
	return VS_TransformCube_Common(IN
#if GS_INSTANCED_CUBEMAP
		,false
#endif
		);
}
#endif // CUBEMAP_REFLECTION_TECHNIQUES

#if __MAX
//
//
//
//
vertexOutputUnlit VS_MaxTransformUnlit(maxVertexInput IN
					#ifdef RAGE_ENABLE_OFFSET
						, rageVertexOffsetBump INOFFSET, uniform float bEnableOffsets
					#endif
					)
{
	northInstancedVertexInput rageIN;

	rageIN.baseVertIn.pos		= IN.pos;
	rageIN.baseVertIn.diffuse	= IN.diffuse;
	rageIN.baseVertIn.diffuse.a	= IN.diffuseAlpha;
	rageIN.baseVertIn.texCoord0	= Max2RageTexCoord2(IN.texCoord0.xy);
	rageIN.baseVertIn.texCoord1	= Max2RageTexCoord2(IN.texCoord1.xy);
	rageIN.baseVertIn.normal	= IN.normal;

#if INSTANCED || BATCH_INSTANCING
	rageIN.nInstIndex = 0;
#endif

	return VS_TransformUnlit(rageIN
						#ifdef RAGE_ENABLE_OFFSET
							, INOFFSET, bEnableOffsets
						#endif
			);
}// end of VS_MaxTransformUnlit()...


//
//
//
//
vertexOutput	VS_MaxTransform(maxVertexInput IN
					#ifdef RAGE_ENABLE_OFFSET
						, rageVertexOffsetBump INOFFSET, uniform float bEnableOffsets
					#endif
					)
{
northVertexInputBump	rageIN;
vertexOutput			rageOUT;

	rageIN.pos		= IN.pos;
	rageIN.diffuse	= IN.diffuse;
	rageIN.diffuse.a= IN.diffuseAlpha;

#if		ALL_CHANNELS(0, 0, 0, 0, 0, 1)
	rageIN.texCoord0 = Max2RageTexCoord2(IN.texCoord0.xy);
#elif	ALL_CHANNELS(0, 0, 0, 0, 1, 1)
	rageIN.texCoord0 = float4(Max2RageTexCoord2(IN.texCoord0.xy), Max2RageTexCoord2(IN.texCoord1.xy));
#elif	ALL_CHANNELS(0, 0, 0, 1, 1, 1)
	rageIN.texCoord0 = Max2RageTexCoord2(IN.texCoord0.xy);
	rageIN.texCoord1 = Max2RageTexCoord2(IN.texCoord1.xy);
	rageIN.texCoord2 = Max2RageTexCoord2(IN.texCoord2.xy);
#elif	ALL_CHANNELS(0, 0, 0, 1, 0, 1)
	rageIN.texCoord0 = Max2RageTexCoord2(IN.texCoord0.xy);
	rageIN.texCoord1 = Max2RageTexCoord2(IN.texCoord2.xy);
#elif	ALL_CHANNELS(1, 1, 0, 0, 0, 1)
	rageIN.texCoord0 = Max2RageTexCoord2(IN.texCoord0.xy);
	rageIN.texCoord1 = float4(Max2RageTexCoord2(IN.texCoord4.xy), float2(0.0f, 0.0f));
#elif	ALL_CHANNELS(1, 1, 0, 0, 1, 1)
	rageIN.texCoord0 = float4(Max2RageTexCoord2(IN.texCoord0.xy), Max2RageTexCoord2(IN.texCoord1.xy));
	rageIN.texCoord1 = float4(Max2RageTexCoord2(IN.texCoord4.xy), float2(0.0f, 0.0f));
#else
	#error "Invalid expected channel!"
#endif

	rageIN.normal	= IN.normal;
	rageIN.tangent	= float4(IN.tangent.xyz,-1.0f);

	return VS_Transform(rageIN
				#ifdef RAGE_ENABLE_OFFSET
					, INOFFSET, bEnableOffsets
				#endif
			);
}// end of VS_MaxTransform()...
#endif //__MAX...

#if DRAWSKINNED_TECHNIQUES
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//	DRAWSKINNED METHODS
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//
//
//

vertexOutput VS_TransformSkin(northSkinVertexInputBump IN
#ifdef RAGE_ENABLE_OFFSET
	, rageSkinVertexOffsetBump INOFFSET, uniform float bEnableOffsets
#endif
	)
{
	vertexOutput OUT;

	float3 inPos = IN.pos;
	float3 inNrm = IN.normal;

#if NORMAL_MAP
	float4 inTan = IN.tangent;
#endif

#ifdef RAGE_ENABLE_OFFSET
	if( bEnableOffsets != 0.0f )
	{
		inPos += INOFFSET.pos;
		inNrm += INOFFSET.normal;
	#if NORMAL_MAP
		inTan += INOFFSET.tangent;
	#endif
	}
#endif

#ifdef USE_VEHICLE_DAMAGE
	float3 inputPos		= inPos;
	float3 inputNormal	= inNrm;
	float4 inCpv        = IN.diffuse;
	ApplyVehicleDamage(inputPos, inputNormal, VEHICLE_DAMAGE_SCALE, inPos, inNrm);
#endif //USE_VEHICLE_DAMAGE...


	float3 pvlWorldNormal, pvlWorldPosition, t_worldEyePos;

	rageSkinMtx skinMtx = ComputeSkinMtx(IN.blendindices, IN.weight);
	rtsProcessVertLightingSkinned(	inPos,
									inNrm,
	#if NORMAL_MAP
									inTan,
	#endif	// NORMAL_MAP
									IN.diffuse,
	#if PALETTE_TINT_EDGE
									IN.specular,
	#endif
									skinMtx, 
									(float4x3)gWorld,
									pvlWorldPosition,
	#if REFLECT
									OUT.worldEyePos,
	#else
									t_worldEyePos,
	#endif
									pvlWorldNormal,
	#if NORMAL_MAP
									OUT.worldBinormal,
									OUT.worldTangent,
	#if PARALLAX
									OUT.tanEyePos,
	#endif //PARALLAX || PARALLAX_MAP_V2
	#endif	// NORMAL_MAP
									OUT.color0
	#if PALETTE_TINT
									,OUT.color1
	#endif
									);

	float3 pos = rageSkinTransform(inPos, skinMtx);

	OUT.pos =  DAY_NIGHT_VERTEX_SCALE*mul(float4(pos,1), gWorldViewProj);

	OUT.worldNormal		= pvlWorldNormal;

	// NOTE: These 3 statements may resolve to the exact same code -- rely on HLSL to strip worthless code
#if DIFFUSE_TEXTURE
	DIFFUSE_VS_OUTPUT	= AnimateUVs(DIFFUSE_VS_INPUT);
#endif
#if SPEC_MAP
	SPECULAR_VS_OUTPUT	= AnimateUVs(SPECULAR_VS_INPUT);
#endif	// SPEC_MAP
#if NORMAL_MAP
	OUT.texCoord.xy		= AnimateUVs(IN.texCoord0.xy);
#endif	// NORMAL_MAP

#if DIFFUSE2
	// Pack in 2nd set of texture coords if needed
	OUT.texCoord.zw = IN.texCoord1.xy;
#endif	// DIFFUSE2

	OUT.worldPos=float4(pvlWorldPosition,1);

#if REFLECT_MIRROR
	OUT.screenPos = OUT.pos;
#endif

	// Adjust ambient bakes
#if REFLECT
	OUT.color0.rg = CalculateBakeAdjust(OUT.worldEyePos, OUT.color0.rg);
#else
	OUT.color0.rg = CalculateBakeAdjust(t_worldEyePos, OUT.color0.rg);
#endif

#if EDGE_WEIGHT
	OUT.edgeWeight.x = 0.0f;
	OUT.edgeWeight.y = 0.1f;
#endif
#if OVERLAY_DIFFUSE2
	OUT.overlayDiffuse2Coords = float2(0.0f, 0.0f); // northSkinVertexInputBump doesn`t support overlayDiffuse2Coords yet.
#endif

	OUT.pos = DAY_NIGHT_VERTEX_SCALE*AddEmissiveZBias(OUT.pos, SUPPORT_INVERTED_PROJECTION ? true : false, 1.0f); // FORWARD SKINNED

	RAGE_COMPUTE_CLIP_DISTANCES(OUT.pos);
	OUT.pos = ApplyDayNightNan(OUT.pos);

	return OUT;
}// end of VS_TransformSkin()...


vertexOutputSeeThrough VS_TransformSkin_SeeThrough(vertexInputSkinSeeThrough IN) 
{
	vertexOutputSeeThrough OUT;

	rageSkinMtx skinMtx = ComputeSkinMtx(IN.blendindices, IN.weight);

	float3 pos = rageSkinTransform(IN.pos, skinMtx);

	OUT = SeeThrough_GenerateOutput(pos);

	return OUT;
}


//
//
//
//
vertexOutputD VS_TransformSkinD(northSkinVertexInputBump IN
#ifdef RAGE_ENABLE_OFFSET
	, rageSkinVertexOffsetBump INOFFSET, uniform float bEnableOffsets
#endif
	) 
{
vertexOutputD OUT;

	float3 inPos = IN.pos;
	float3 inNrm = IN.normal;

#if NORMAL_MAP
	float4 inTan = IN.tangent;
#endif

#ifdef RAGE_ENABLE_OFFSET
	if( bEnableOffsets != 0.0f )
	{
		inPos += INOFFSET.pos;
		inNrm += INOFFSET.normal;
	#if NORMAL_MAP
		inTan += INOFFSET.tangent;
	#endif
	}
#endif


	float3 pvlWorldNormal, pvlWorldPosition, t_worldEyePos;
	float4 pvlColor0;

	rageSkinMtx skinMtx = ComputeSkinMtx(IN.blendindices, IN.weight);

	rtsProcessVertLightingSkinned(	inPos,
									inNrm,
	#if NORMAL_MAP
									inTan,
	#endif	// NORMAL_MAP
									IN.diffuse,
	#if PALETTE_TINT_EDGE
									IN.specular,
	#endif
									skinMtx, 
									(float4x3)gWorld,
									pvlWorldPosition,
	#if VERTEXOUTPUTD_WORLDEYEPOS
									OUT.worldEyePos,
	#else
									t_worldEyePos,
	#endif //REFLECT
									pvlWorldNormal,
	#if NORMAL_MAP
									OUT.worldBinormal,
									OUT.worldTangent,
		#if PARALLAX
									OUT.tanEyePos,
		#endif
	#endif	// NORMAL_MAP
									pvlColor0
	#if PALETTE_TINT
									,OUT.color1
	#endif
									);

	float3 pos = rageSkinTransform(inPos, skinMtx);

#if UMOVEMENTS
	pos		= VS_ApplyMicromovements(pos, IN.specular.rgb);
#endif
#if UMOVEMENTS_TEX
	pos		= VS_ApplyMicromovements(pos, IN.texCoord1.xxy);
#endif
	OUT.pos = DAY_NIGHT_VERTEX_SCALE*mul(float4(pos,1), gWorldViewProj);

#if VERTEXOUTPUTD_COLOR0
	OUT.color0 = pvlColor0;
#endif
#if VERTEXOUTPUTD_NORMAL
	OUT.worldNormal		= pvlWorldNormal;
#endif //VERTEXOUTPUTD_NORMAL...

	// NOTE: These 3 statements may resolve to the exact same code -- rely on HLSL to strip worthless code
#if DIFFUSE_TEXTURE
	DIFFUSE_VS_OUTPUT	= AnimateUVs(DIFFUSE_VS_INPUT);
#endif
#if SPEC_MAP
	SPECULAR_VS_OUTPUT	= AnimateUVs(SPECULAR_VS_INPUT);
#endif	// SPEC_MAP
#if NORMAL_MAP
	OUT.texCoord.xy		= AnimateUVs(IN.texCoord0.xy);
#endif	// NORMAL_MAP

#if DIFFUSE2
	// Pack in 2nd set of texture coords if needed
	OUT.texCoord.zw = IN.texCoord1.xy;
#endif	// DIFFUSE2

#if VERTEXOUTPUTD_WORLDEYEPOS
	OUT.color0.rg = CalculateBakeAdjust(OUT.worldEyePos, OUT.color0.rg);
#else
	OUT.color0.rg = CalculateBakeAdjust(t_worldEyePos, OUT.color0.rg);
#endif

#if EDGE_WEIGHT
	OUT.edgeWeight.x = 0.0f;
	OUT.edgeWeight.y = 0.1f;
#endif
#if OVERLAY_DIFFUSE2
	OUT.overlayDiffuse2Coords = float2(0.0f, 0.0f); // northSkinVertexInputBump doesn`t support overlayDiffuse2Coords yet.
#endif

	OUT.pos = DAY_NIGHT_VERTEX_SCALE*AddEmissiveZBias(OUT.pos, SUPPORT_INVERTED_PROJECTION ? true : false, 1.0f); // DEFERRED
	OUT.pos = ApplyDayNightNan(OUT.pos);

#if WIND_DISPLACEMENT && !SHADER_FINAL
	OUT.windDebug = float4(0.0f, 0.0f, 0.0f, 0.0f);
#endif

	return OUT;
}// end of VS_TransformSkinD()...



//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//
//
//
vertexOutputUnlit VS_TransformSkinUnlit(northSkinVertexInput IN
#ifdef RAGE_ENABLE_OFFSET
	, rageSkinVertexOffset INOFFSET, uniform float bEnableOffsets
#endif
	) 
{
vertexOutputUnlit OUT;

	float3 inPos = IN.pos;

#ifdef RAGE_ENABLE_OFFSET
	if( bEnableOffsets != 0.0f )
	{
		inPos += INOFFSET.pos;
	}
#endif
	
#ifdef USE_VEHICLE_DAMAGE
	float3 inputPos		= inPos;
	float4 inCpv        = IN.diffuse;
	ApplyVehicleDamage(inputPos, VEHICLE_DAMAGE_SCALE, inPos);
#endif //USE_VEHICLE_DAMAGE...

	
	rageSkinMtx skinMtx = ComputeSkinMtx(IN.blendindices, IN.weight);
	float3 pos = rageSkinTransform(inPos, skinMtx);

	// Write out final position & texture coords
	OUT.pos =  DAY_NIGHT_VERTEX_SCALE*mul(float4(pos,1), gWorldViewProj);

	// NOTE: These 3 statements may resolve to the exact same code -- rely on HLSL to strip worthless code
#if DIFFUSE_TEXTURE
	DIFFUSE_VS_OUTPUT	= AnimateUVs(DIFFUSE_VS_INPUT);
#endif
#if SPEC_MAP
	SPECULAR_VS_OUTPUT	= AnimateUVs(SPECULAR_VS_INPUT);
#endif	// SPEC_MAP
#if NORMAL_MAP
	OUT.texCoord.xy		= AnimateUVs(IN.texCoord0.xy);
#endif	// NORMAL_MAP

#if DIFFUSE2
	// Pack in 2nd set of texture coords if needed
	OUT.texCoord.zw = IN.texCoord1.xy;
#endif	// DIFFUSE2

	OUT.color0		= saturate(IN.diffuse);
#if PALETTE_TINT
	#if PALETTE_TINT_EDGE
		OUT.color1	= UnpackTintPalette(IN.specular);
	#else
		OUT.color1	= UnpackTintPalette(IN.diffuse);
	#endif
#endif

	OUT.pos = DAY_NIGHT_VERTEX_SCALE*AddEmissiveZBias(OUT.pos,false, 1.0f);	// UNLIT SKINNED
	OUT.pos = ApplyDayNightNan(OUT.pos);

	return OUT;
}// end of VS_TransformSkinUnlit()...

#endif	//DRAWSKINNED_TECHNIQUES



//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ====== Texture Samplers ===========
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// NOTE: Do we want to configure the filter & wrap states via the texture itself??

#if DIFFUSE2 || OVERLAY_DIFFUSE2
	BeginSampler(sampler2D,diffuseTexture2,DiffuseSampler2,DiffuseTex2)
		string UIName="Secondary Texture";
		string UIHint="uv1";
		string TCPTemplate="Diffuse";
		string TextureType="Diffuse";
	ContinueSampler(sampler2D,diffuseTexture2,DiffuseSampler2,DiffuseTex2)
		AddressU  = WRAP;        
		AddressV  = WRAP;
		AddressW  = WRAP;
		MIPFILTER = MIPLINEAR;
		MINFILTER = HQLINEAR;
		MAGFILTER = MAGLINEAR;
	EndSampler;
#endif	// DIFFUSE2 || OVERLAY_DIFFUSE2

#if DIFFUSE_EXTRA
	// fake extra diffuse slot for icon textures
	BeginSampler(sampler2D,diffuseExtraTexture,DiffuseExtraSampler,DiffuseExtraTex)
		string	UIName="Diffuse Extra Slot";
		string	UIHint="icon.dds";
		int		nostrip=1;	// dont strip
		string TCPTemplate="Diffuse";
		string TextureType="Diffuse";

	ContinueSampler(sampler2D,diffuseExtraTexture,DiffuseExtraSampler,DiffuseExtraTex)
		AddressU  = WRAP;
		AddressV  = WRAP;
		AddressW  = WRAP;
		MIPFILTER = MIPLINEAR;
		MINFILTER = HQLINEAR;
		MAGFILTER = MAGLINEAR;
	EndSampler;
#endif	// DIFFUSE_EXTRA

#if NORMAL_MAP
	BeginSampler(sampler2D,bumpTexture,BumpSampler,BumpTex)
	#if PARALLAX
		string UIName="Bump [RGB] + Height [A] Texture";
		string UIHint="normalmap+heightmap";
		string TCPTemplate="NormalMap";
		string TextureType="BumpAndHeight";
	#else
		string UIName="Bump Texture";
		string UIHint="normalmap";
		string TCPTemplate="NormalMap";
		string TextureType="Bump";
	#endif
	ContinueSampler(sampler2D,bumpTexture,BumpSampler,BumpTex)
	#if PARALLAX
		AddressU  = WRAP;        
		AddressV  = WRAP;
		AddressW  = WRAP;
		MIPFILTER = MIPLINEAR;
		MINFILTER = HQLINEAR;
		MAGFILTER = MAGLINEAR;
	#else //PARALLAX...
		AddressU  = WRAP;        
		AddressV  = WRAP;
		AddressW  = WRAP;
		#ifdef USE_ANISOTROPIC
			MINFILTER		= MINANISOTROPIC;
			MAGFILTER		= MAGLINEAR; 
			ANISOTROPIC_LEVEL(4)
		#else 
			MINFILTER		= HQLINEAR;
			MAGFILTER		= MAGLINEAR; 
		#endif
		MIPFILTER = MIPLINEAR;

		#if defined(USE_HIGH_QUALITY_TRILINEARTHRESHOLD) && !__MAX
			TRILINEARTHRESHOLD=0;
		#endif
	#endif //PARALLAX...
	EndSampler;
#endif	// NORMAL_MAP

#if SPEC_MAP && !SPECULAR_DIFFUSE
	BeginSampler(sampler2D,specTexture,SpecSampler,SpecularTex)
		string UIName="Specular Texture";
		#if SPECULAR_UV1
			string UIHint="specularmap,uv1";
		#else
			string UIHint="specularmap";
		#endif
		string TCPTemplate="Specular";
		string TextureType="Specular";
	ContinueSampler(sampler2D,specTexture,SpecSampler,SpecularTex)
	#if PARALLAX
		AddressU  = WRAP;
		AddressV  = WRAP;
		AddressW  = WRAP;
	#else //PARALLAX...
		AddressU  = WRAP;
		AddressV  = WRAP;
		AddressW  = WRAP;
		MIPFILTER = MIPLINEAR;
		MINFILTER = HQLINEAR;
		MAGFILTER = MAGLINEAR;
	#endif //PARALLAX...
	EndSampler;
#endif	// SPEC_MAP

#if REFLECT && !REFLECT_DYNAMIC && !REFLECT_MIRROR
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
#endif // REFLECT


#if COLOR_VARIATION_TEXTURE
//
//
//
//
BeginSampler(sampler2D,diffuseTexturePal,TextureSamplerDiffPal,DiffuseTexPal)
	string UIName="Diffuse Palette";
	string TCPTemplate="Diffuse";
	string TextureType="Palette";
ContinueSampler(sampler2D,diffuseTexturePal,TextureSamplerDiffPal,DiffuseTexPal)
	AddressU  = CLAMP;
	AddressV  = CLAMP;
	MIPFILTER = POINT;
	MINFILTER = POINT;
	MAGFILTER = POINT;
EndSampler;

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

	// shift range to <0; 1>
	half texAlpha0 = (texAlpha-alphaMin) / (alphaMax-alphaMin);

	// sample palette:
	const float paletteRow = paletteSelector;	// this must be converted in higher CustomShaderFX code
	half4 newDiffuse = h4tex2D(TextureSamplerDiffPal, float2(texAlpha0, paletteRow));

	if(	(texAlpha >= (alphaMin/*+2*/)) && (texAlpha <= (alphaMax/*-2*/)))
	{
		// new: palette's works as a tint:
		OUT.rgb = IN_diffuseColor.rgb * newDiffuse.rgb;
		OUT.a	= 1.0f;
	}

	return OUT;
}// end of PalettizeColorVariationTexture()...
#endif //COLOR_VARIATION_TEXTURE...

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ====== PIXEL SHADERS ===========
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//
//
//
float4 PixelTexturedUnlit( vertexOutputUnlit IN )
{
#if __MAX
	#if DIFFUSE_TEXTURE
		float4 outColor =  tex2D(DiffuseSampler, DIFFUSE_PS_INPUT);
		if (!useAlphaFromDiffuse)
		{
			outColor.a	=  tex2D(DiffuseSamplerAlpha, DIFFUSE_PS_INPUT).r;
		}
	#else
		float4 outColor = float4(1,1,1,1);
	#endif
	
	#if PALETTE_TINT_MAX
		outColor.rgba	*= IN.color0.rgba;	// Max: unpacked tint color is directly fed into COLOR0 (from MapChannel=13)
	#else
		#if EMISSIVE
			outColor	*= IN.color0.rrra;
		#else
			outColor	*= IN.color0.rgba;
		#endif
	#endif			
#else
	float4 outColor = float4(1,1,1,1);
	#if DIFFUSE_TEXTURE
		outColor = tex2D(DiffuseSampler, DIFFUSE_PS_INPUT);
	#endif

#if COLOR_VARIATION_TEXTURE
	outColor = PS_PalettizeColorVariationTexture(outColor);
#endif

	outColor *= 
				#if EMISSIVE
					1.0f;
				#else
					IN.color0.rgba;
				#endif
#endif

#if PALETTE_TINT
	outColor.rgb		*= IN.color1.rgb;
#endif

#if DIFFUSE2
	float4 overlayColor = tex2D(DiffuseSampler2, IN.texCoord.zw);
	outColor.rgb = outColor.rgb * (1.0 - overlayColor.w) + overlayColor.rgb * overlayColor.w;
#endif	// DIFFUSE2

#if BATCH_INSTANCING
	//Apply tint
	outColor.rgb = lerp(outColor.rgb, outColor.rgb * IN.batchTint.rgb, IN.batchTint.a);
#endif //BATCH_INSTANCING

#if EMISSIVE
	float3 emissiveColor = outColor.rgb * 
						   emissiveMultiplier *
						   outColor.a *
						#if PALETTE_TINT_MAX
						   1.0f
						#else
						   IN.color0.rrr
						#endif
						   ;

	#if EMISSIVE_NIGHTONLY
		emissiveColor *= gDayNightEffects;
	#endif 
	#if EMISSIVE_NIGHTONLY_FOLLOW_VERTEX_SCALE
		emissiveColor *= CalcDayNightVertScale(gWorld);
	#endif

	outColor.rgb += emissiveColor;
#endif

outColor.a *= globalAlpha * gInvColorExpBias;

#ifdef COLORIZE
	outColor.rgba *= colorize;
#endif

	return outColor;
}// end of PixelTexturedUnlit()...

//
//
//
//
float4 PixelTextured( vertexOutput IN, int numLights, bool directionalLightAndShadow, bool bSupportCapsule, bool bUseHighQuality)
{
	float4 OUT;

#if defined(MIRROR_DEBUG_SUPER_REFLECTIVE) // useful for debugging mirrors
#if defined(SHADER_FINAL)
	"super-reflective mirror should be debug-only"
#endif // defined(SHADER_FINAL)

	OUT.xyz = reflectivePower*CalculateMirrorReflection(REFLECT_TEX_SAMPLER, IN.screenPos, 0, IN.texCoord.xy);
	OUT.w = 1;

#elif defined(MIRROR_DECAL_FX)

#if NORMAL_MAP
	float3 worldNormal = CalculateWorldNormal(
		tex2D_NormalMap(BumpSampler, IN.texCoord.xy).xy,
		bumpiness,
		IN.worldTangent,
		IN.worldBinormal,
		IN.worldNormal);
#if !defined(SHADER_FINAL)
	worldNormal = normalize(lerp(worldNormal, IN.worldNormal, gMirrorDebugParams.x));
#endif // !defined(SHADER_FINAL)
#else
	const float3 worldNormal = IN.worldNormal;
#endif

#if defined(USE_SPECULAR_FRESNEL)
	// note that we're intentionally using the geometry normal (IN.worldNormal) for fresnel because it's smoother
	const float3 surfaceToEyeDir = normalize(IN.worldEyePos);
	const float fresnel = fresnelSlick(specularFresnel, saturate(dot(surfaceToEyeDir, IN.worldNormal)));
#else
	const float fresnel = 1.0f;
#endif

	OUT.xyz = reflectivePower*CalculateMirrorReflection(REFLECT_TEX_SAMPLER, IN.screenPos, worldNormal, IN.texCoord.xy);
	OUT.w = fresnel*IN.color0.a*tex2D(DiffuseSampler, DIFFUSE_PS_INPUT).w*gInvColorExpBias;

#if !defined(SHADER_FINAL)
	OUT.xyz *= gMirrorDebugParams.y; // brightness scale
	OUT.w = lerp(OUT.w, 1, gMirrorDebugParams.x);
#endif // !defined(SHADER_FINAL)

#else // not defined(MIRROR_DECAL_FX)

	SurfaceProperties surfaceInfo = GetSurfaceProperties(
			(float4)IN.color0,
	#if PALETTE_TINT
			(float4)IN.color1,
	#endif
	#if DIFFUSE_TEXTURE
			(float2)DIFFUSE_PS_INPUT,
			DiffuseSampler, 
	#endif
	#if DIFFUSE2
			(float2)IN.texCoord.zw,
			DiffuseSampler2,
	#elif OVERLAY_DIFFUSE2
			(float2)IN.overlayDiffuse2Coords,
			DiffuseSampler2,
	#if OVERLAY_DIFFUSE2_SEPARATE_ALPHA_SOURCE
		#if defined(OVERLAY_DIFFUSE2_ALPHA_SOURCE)
			IN.OVERLAY_DIFFUSE2_ALPHA_SOURCE,
		#elif defined(USE_VERTEXOUTPUTD_INVCOLOR0DOTA_AS_OVERLAY_DIFFUSE2_ALPHA_SOURCE)
			1.0f - IN.color0.a,
		#else
			#error "Need to specify an alpha source for OVERLAY_DIFFUSE2_SEPARATE_ALPHA_SOURCE"
		#endif
	#endif
	#endif
	#if SPEC_MAP
			(float2)SPECULAR_PS_INPUT,
	#if SPECULAR_DIFFUSE
			DiffuseSampler,
	#else
			SpecSampler,
	#endif // SPECULAR_DIFFUSE
	#endif	// SPEC_MAP
	#if REFLECT
			IN.worldEyePos,
			REFLECT_TEX_SAMPLER,
		#if REFLECT_MIRROR
			IN.texCoord.xy,
			IN.screenPos,
		#endif // REFLECT_MIRROR
	#endif	// REFLECT
	#if NORMAL_MAP
			(float2)IN.texCoord.xy,
			BumpSampler, 
			(float3)IN.worldTangent,
			(float3)IN.worldBinormal,
		#if PARALLAX || PARALLAX_MAP_V2
		#if PARALLAX 
			(float3)IN.tanEyePos,
		#endif //PARALLAX
		#if PARALLAX_MAP_V2
			IN.worldEyePos,
			heightSampler,
		#if EDGE_WEIGHT
			IN.edgeWeight.xy,	
		#endif // EDGE_WEIGHT
		#endif // PARALLAX_MAP_V2
		#endif //PARALLAX || PARALLAX_MAP_V2
	#endif	// NORMAL_MAP
		(float3)IN.worldNormal
		);

	#if COLOR_VARIATION_TEXTURE
		surfaceInfo.surface_diffuseColor.rgba = PS_PalettizeColorVariationTexture(surfaceInfo.surface_diffuseColor.rgba);
	#endif

	// Compute some surface information.
	StandardLightingProperties surfaceStandardLightingProperties =
		DeriveLightingPropertiesForCommonSurface( surfaceInfo );

	surfaceStandardLightingProperties.diffuseColor = ProcessDiffuseColor(surfaceStandardLightingProperties.diffuseColor);

	surfaceProperties surface;
	directionalLightProperties light;
	materialProperties material;

	#if __XENON
	if(numLights > MIN_LIGHT_ISOLATE)
	{
		[isolate]
		populateForwardLightingStructs(
			surface,
			material,
			light,
			IN.worldPos.xyz,
			surfaceInfo.surface_worldNormal.xyz,
			surfaceStandardLightingProperties);
	}
	else
	#endif
	{
		populateForwardLightingStructs(
			surface,
			material,
			light,
			IN.worldPos.xyz,
			surfaceInfo.surface_worldNormal.xyz,
			surfaceStandardLightingProperties);
	}

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

#endif // not defined(MIRROR_DECAL_FX)

#if defined(DISPLACEMENT_SHADER)
	OUT = PS_ApplyDisplacement(OUT, (float2)DIFFUSE_PS_INPUT, DiffuseSampler, IN.pos.xy, ReflectionSampler, IN.worldEyePos);
#endif

#if defined(USE_FOG) && !EMISSIVE_ADDITIVE
#if defined(USE_FOGRAY_FORWARDPASS) && !__LOW_QUALITY
	if (gUseFogRay)
		OUT =  calcDepthFxBasic(OUT, IN.worldPos.xyz, IN.pos.xy * gooScreenSize.xy);
	else
		OUT = calcDepthFxBasic(OUT, IN.worldPos.xyz);
#else
	OUT = calcDepthFxBasic(OUT, IN.worldPos.xyz);
#endif
#endif

	return OUT;
}// end of PixelTextured()...


//
//
//
//
#if __WIN32PC_MAX_RAGEVIEWER
float4 PixelTexturedMaxRageViewer(vertexOutputMaxRageViewer IN)
{
	SurfaceProperties surfaceInfo = GetSurfacePropertiesBasic(
		(float4)IN.color0, 
	#if PALETTE_TINT
		(float4)IN.color1,
	#endif
	#if DIFFUSE_TEXTURE
		(float2)DIFFUSE_PS_INPUT,
		DiffuseSampler, 
	#endif
		(float3)IN.worldNormal
		);

	// Compute some surface information.
	StandardLightingProperties surfaceStandardLightingProperties =
		DeriveLightingPropertiesForCommonSurface( surfaceInfo );

	surfaceStandardLightingProperties.diffuseColor = ProcessDiffuseColor(surfaceStandardLightingProperties.diffuseColor);

	// Populate all structures
	surfaceProperties surface;
	materialProperties material;
	directionalLightProperties light;

	populateForwardLightingStructs(
		surface, 
		material, 
		light, 
		IN.worldPos, 
		surfaceInfo.surface_worldNormal.xyz, 
		surfaceStandardLightingProperties);

	float4 OUT = calculateForwardLighting_DynamicReflection(
		surface,
		material, 
		light);

	#ifdef USE_FOG && !EMISSIVE_ADDITIVE
		OUT = calcDepthFxBasic(OUT, IN.worldPos);
	#endif

	return OUT;
}// end of PixelTexturedBasic_MaxRageViewer()...
#endif // __WIN32PC_MAX_RAGEVIEWER


//
//
//
//
float4 PixelTexturedWaterReflection(vertexOutputWaterReflection IN, bool useAlphaClip)
{
#if WATER_REFLECTION_CLIP_IN_PIXEL_SHADER
	rageDiscard(dot(float4(IN.worldPos.xyz, 1), gLight0PosX) < 0);
#endif // WATER_REFLECTION_CLIP_IN_PIXEL_SHADER

	SurfaceProperties surfaceInfo;
	if (useAlphaClip)
	{
		surfaceInfo = GetSurfacePropertiesBasicAlphaClip(
			IN.color0, 
#if PALETTE_TINT
			IN.color1,
#endif
#if DIFFUSE_TEXTURE
			DIFFUSE_PS_INPUT,
			DiffuseSampler, 
#endif
			IN.worldNormal
#if DIFFUSE_TEXTURE
			, true
#endif // DIFFUSE_TEXTURE
			);
	}
	else
	{
		surfaceInfo = GetSurfacePropertiesBasic(
			IN.color0, 
#if PALETTE_TINT
			IN.color1,
#endif
#if DIFFUSE_TEXTURE
			DIFFUSE_PS_INPUT,
			DiffuseSampler, 
#endif
			IN.worldNormal
		);
	}

	#if COLOR_VARIATION_TEXTURE
		surfaceInfo.surface_diffuseColor.rgba = PS_PalettizeColorVariationTexture(surfaceInfo.surface_diffuseColor.rgba);
	#endif

	// Compute some surface information.
	StandardLightingProperties surfaceStandardLightingProperties =
		DeriveLightingPropertiesForCommonSurfaceInternal( surfaceInfo, PALETTE_TINT );

	surfaceStandardLightingProperties.diffuseColor = ProcessDiffuseColor(surfaceStandardLightingProperties.diffuseColor);

#if BATCH_INSTANCING
	//Apply tint
	surfaceStandardLightingProperties.diffuseColor.rgb = lerp(surfaceStandardLightingProperties.diffuseColor.rgb, surfaceStandardLightingProperties.diffuseColor.rgb * IN.batchTint.rgb, IN.batchTint.a);
	surfaceStandardLightingProperties.naturalAmbientScale = IN.batchAo;
#endif //BATCH_INSTANCING

	// Populate all structures
	surfaceProperties surface;
	materialProperties material;
	directionalLightProperties light;

	populateForwardLightingStructs(
		surface, 
		material, 
		light, 
		IN.worldPos, 
		surfaceInfo.surface_worldNormal.xyz, 
		surfaceStandardLightingProperties);

	float4 OUT = calculateForwardLighting_WaterReflection(
		surface,
		material,
		light,
		1, // ambient scale
		1, // directional scale
		SHADOW_RECEIVING // directional shadow
		);

#if defined(USE_FOG) && !EMISSIVE_ADDITIVE
#if defined(USE_FOGRAY_FORWARDPASS) && !__LOW_QUALITY
	if (gUseFogRay)
		OUT =  calcDepthFxBasic(OUT, IN.worldPos.xyz, IN.pos.xy * gooScreenSize.xy);
	else
		OUT = calcDepthFxBasic(OUT, IN.worldPos);
#else
		OUT = calcDepthFxBasic(OUT, IN.worldPos);
#endif
#endif

#if RSG_PC
	OUT.a = saturate(OUT.a);
#endif

	return OUT;

}// end of PixelTexturedWaterReflection()...

#if CUBEMAP_REFLECTION_TECHNIQUES
float4 PixelTexturedCube(vertexMegaOutputCube IN)
{
	SurfaceProperties surfaceInfo = GetSurfacePropertiesBasic(
		saturate(IN.color0), 
		#if PALETTE_TINT
			IN.color1,
		#endif
		#if DIFFUSE_TEXTURE
			DIFFUSE_PS_INPUT,
			DiffuseSampler, 
		#endif
		IN.worldNormal
		);

#if COLOR_VARIATION_TEXTURE
	surfaceInfo.surface_diffuseColor.rgba = PS_PalettizeColorVariationTexture(surfaceInfo.surface_diffuseColor.rgba);
#endif

	// Compute some surface information.
	StandardLightingProperties surfaceStandardLightingProperties =
		DeriveLightingPropertiesForCommonSurface( surfaceInfo );

#if EMISSIVE_CLIP
	float alphaValue = surfaceStandardLightingProperties.diffuseColor.a;
	rageDiscard(alphaValue < 0.5f);
#endif

	surfaceStandardLightingProperties.diffuseColor = ProcessDiffuseColor(surfaceStandardLightingProperties.diffuseColor);

#if BATCH_INSTANCING
	//Apply tint
	surfaceStandardLightingProperties.diffuseColor.rgb = lerp(surfaceStandardLightingProperties.diffuseColor.rgb, surfaceStandardLightingProperties.diffuseColor.rgb * IN.batchTint.rgb, IN.batchTint.a);
	surfaceStandardLightingProperties.naturalAmbientScale = IN.batchAo;
#endif //BATCH_INSTANCING

	// Populate all structures
	surfaceProperties surface;
	materialProperties material;
	directionalLightProperties light;

	populateForwardLightingStructs(
		surface, 
		material, 
		light, 
		IN.worldPos, 
		surfaceInfo.surface_worldNormal.xyz, 
		surfaceStandardLightingProperties);

	float4 OUT = calculateForwardLighting_DynamicReflection(
		surface,
		material,
		light);

	#if defined(USE_FOG) && !EMISSIVE_ADDITIVE
	OUT = calcDepthFxBasic(OUT, IN.worldPos);
	#endif

	return OUT;
}// end of PixelTexturedCube()...
#endif // CUBEMAP_REFLECTION_TECHNIQUES

#if __MAX
//
//
//
//
float4 MaxPixelTextured( vertexOutput IN )
{
float4 OUT;

	SurfaceProperties surfaceInfo = GetSurfaceProperties(
				(float4)IN.color0, 
		#if DIFFUSE_TEXTURE
				(float2)DIFFUSE_PS_INPUT,
				DiffuseSampler,
		#endif 
		#if DIFFUSE2
				(float2)IN.texCoord.zw,
				DiffuseSampler2,
		#elif OVERLAY_DIFFUSE2
				(float2)IN.overlayDiffuse2Coords,
				DiffuseSampler2,
		#if OVERLAY_DIFFUSE2_SEPARATE_ALPHA_SOURCE
			#if defined(OVERLAY_DIFFUSE2_ALPHA_SOURCE)
				IN.OVERLAY_DIFFUSE2_ALPHA_SOURCE,
			#elif defined(USE_VERTEXOUTPUTD_INVCOLOR0DOTA_AS_OVERLAY_DIFFUSE2_ALPHA_SOURCE)
				1.0f - IN.color0.a,
			#else
				#error "Need to specify an alpha source for OVERLAY_DIFFUSE2_SEPARATE_ALPHA_SOURCE"
			#endif
		#endif
		#endif // DIFFUSE2
		#if SPEC_MAP
				(float2)SPECULAR_PS_INPUT,
		#if SPECULAR_DIFFUSE
				DiffuseSampler,
		#else
				SpecSampler,
		#endif // SPECULAR_DIFFUSE
		#endif // SPEC_MAP
		#if REFLECT
				IN.worldEyePos,
				REFLECT_TEX_SAMPLER,
			#if REFLECT_MIRROR
				IN.texCoord.xy,
				IN.screenPos,
			#endif // REFLECT_MIRROR
		#endif	// REFLECT
		#if NORMAL_MAP
				(float2)IN.texCoord.xy,
				BumpSampler, 
				(float3)IN.worldTangent,
				(float3)IN.worldBinormal,
			#if PARALLAX || PARALLAX_MAP_V2
			#if PARALLAX
				(float3)IN.tanEyePos,
			#endif
			#if PARALLAX_MAP_V2
				IN.worldEyePos,
				heightSampler,
			#if EDGE_WEIGHT
				IN.edgeWeight.xy,	
			#endif // EDGE_WEIGHT
			#endif // PARALLAX_MAP_V2
			#endif //PARALLAX...
		#endif	// NORMAL_MAP
		(float3)IN.worldNormal);

#if DIFFUSE_TEXTURE
	if (!useAlphaFromDiffuse)
	{
		surfaceInfo.surface_diffuseColor.a = tex2D(DiffuseSamplerAlpha,	DIFFUSE_PS_INPUT).r;
	}
#endif	

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

}// end of MaxPixelTextured()...


//
//
//
//
float4 PS_MaxPixelTextured( vertexOutput IN ) : COLOR
{   
//#error "I say it's gonna be a hippopadamous"
float4 OUT = float4(0,0,0,1);

	if(maxLightingToggle)
	{
		OUT = MaxPixelTextured(IN);
	}
	else
	{
		vertexOutputUnlit unlitIN;
		unlitIN.color0		= IN.color0;	// IN.color0.rgb - contains AO scaling, etc.
#if DIFFUSE_TEXTURE
		unlitIN.texCoord	= IN.texCoord;
#endif		
		OUT = PixelTexturedUnlit(unlitIN);
	}
	rageDiscard(useAlphaFromDiffuse && OUT.a <= 90.0/255.0);
	return saturate(OUT);
}// end of PS_MaxPixelTextured()...
#endif //__MAX...


OutHalf4Color PS_TexturedUnlit(vertexOutputUnlit IN)
{
	return CastOutHalf4Color(PackColor(PixelTexturedUnlit(IN)));
}

void DoCutOut(vertexOutput IN)
{
#if DIFFUSE_TEXTURE && !DONT_TRUST_DIFFUSE_ALPHA
	float alphaBlend = GetSurfaceAlpha( DIFFUSE_PS_INPUT.xy, DiffuseSampler);
#elif DONT_TRUST_DIFFUSE_ALPHA
	float alphaBlend = globalAlpha;
#else
	float alphaBlend = 1.0;
#endif
	rageDiscard(alphaBlend <= global_alphaRef);
}

OutHalf4Color PS_Textured_Zero(vertexOutput IN)
{
	return CastOutHalf4Color(PackHdr(PixelTextured(IN, 0, USE_DIRECTIONAL_LIGHT_FWD_0_LIGHTS, false, false)));
}

#if APPLY_DOF_TO_ALPHA_DECALS
OutHalf4Color_DOF PS_Textured_Zero_DOF(vertexOutputDOF IN)
{
	OutHalf4Color_DOF OUT;
	OUT.col0 = PS_Textured_Zero(IN.vertOutput).col0;
#if ALPHA_AS_A_DOF_OUTPUT
	float dofOutput = OUT.col0.a;
#else
	float dofOutput = OUT.col0.a > DOF_ALPHA_LIMIT ? 1.0 : 0.0;
#endif	
	OUT.dof.depth = IN.linearDepth * dofOutput;
	OUT.dof.alpha = dofOutput;
	return OUT;
}
#endif //APPLY_DOF_TO_ALPHA_DECALS

OutHalf4Color PS_Textured_Zero_CutOut(vertexOutput IN)
{
	DoCutOut(IN);
	return PS_Textured_Zero(IN);
}


OutHalf4Color PS_Textured_Four(vertexOutput IN)
{
	return CastOutHalf4Color(PackHdr(PixelTextured(IN, 4, USE_DIRECTIONAL_LIGHT_FWD_4_OR_8_LIGHTS, true, false)));
}

#if APPLY_DOF_TO_ALPHA_DECALS
OutHalf4Color_DOF PS_Textured_Four_DOF(vertexOutputDOF IN)
{
	OutHalf4Color_DOF OUT;
	OUT.col0 = PS_Textured_Four(IN.vertOutput).col0;
	float dofOutput = OUT.col0.a > DOF_ALPHA_LIMIT ? 1.0 : 0.0;
	OUT.dof.depth = IN.linearDepth * dofOutput; //using linear Depth
	OUT.dof.alpha = dofOutput;
	return OUT;
}
#endif //APPLY_DOF_TO_ALPHA_DECALS

OutHalf4Color PS_Textured_Four_CutOut(vertexOutput IN)
{
	DoCutOut(IN);
	return PS_Textured_Four(IN);
}


OutHalf4Color PS_Textured_Eight(vertexOutput IN)
{
	return CastOutHalf4Color(PackHdr(PixelTextured(IN, 8, USE_DIRECTIONAL_LIGHT_FWD_4_OR_8_LIGHTS, true, false)));
}

#if APPLY_DOF_TO_ALPHA_DECALS
OutHalf4Color_DOF PS_Textured_Eight_DOF(vertexOutputDOF IN)
{
	OutHalf4Color_DOF OUT;
	OUT.col0 = PS_Textured_Eight(IN.vertOutput).col0;
	float dofOutput = OUT.col0.a > DOF_ALPHA_LIMIT ? 1.0 : 0.0;
	OUT.dof.depth = IN.linearDepth * dofOutput; //using linear Depth
	OUT.dof.alpha = dofOutput;
	return OUT;
}
#endif //APPLY_DOF_TO_ALPHA_DECALS

OutHalf4Color PS_Textured_Eight_CutOut(vertexOutput IN)
{
	DoCutOut(IN);
	return PS_Textured_Eight(IN);
}


#if HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED
#if HIGHQUALITY_FORWARD_TECHNIQUES

OutHalf4Color PS_Textured_ZeroHighQuality(vertexOutput IN)
{
	return CastOutHalf4Color(PackHdr(PixelTextured(IN, 0, USE_DIRECTIONAL_LIGHT_FWD_0_LIGHTS, false, HIGHQUALITY_FORWARD_TECHNIQUES)));
}
OutHalf4Color PS_Textured_ZeroHighQuality_CutOut(vertexOutput IN)
{
	DoCutOut(IN);
	return PS_Textured_ZeroHighQuality(IN);
}


OutHalf4Color PS_Textured_FourHighQuality(vertexOutput IN)
{
	return CastOutHalf4Color(PackHdr(PixelTextured(IN, 4, USE_DIRECTIONAL_LIGHT_FWD_4_OR_8_LIGHTS, true, HIGHQUALITY_FORWARD_TECHNIQUES)));
}
OutHalf4Color PS_Textured_FourHighQuality_CutOut(vertexOutput IN)
{
	DoCutOut(IN);
	return PS_Textured_FourHighQuality(IN);
}


OutHalf4Color PS_Textured_EightHighQuality(vertexOutput IN)
{
	return CastOutHalf4Color(PackHdr(PixelTextured(IN, 8, USE_DIRECTIONAL_LIGHT_FWD_4_OR_8_LIGHTS, true, HIGHQUALITY_FORWARD_TECHNIQUES)));
}
OutHalf4Color PS_Textured_EightHighQuality_CutOut(vertexOutput IN)
{
	DoCutOut(IN);
	return PS_Textured_EightHighQuality(IN);
}

#else

#define PS_Textured_ZeroHighQuality			PS_Textured_Zero
#define PS_Textured_ZeroHighQuality_CutOut	PS_Textured_Zero_CutOut

#define PS_Textured_FourHighQuality			PS_Textured_Four
#define PS_Textured_FourHighQuality_CutOut	PS_Textured_Four_CutOut

#define PS_Textured_EightHighQuality		PS_Textured_Eight
#define PS_Textured_EightHighQuality_CutOut PS_Textured_Eight_CutOut

#endif
#endif // HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED

#if __WIN32PC_MAX_RAGEVIEWER
OutHalf4Color PS_TexturedMaxRageViewer(vertexOutputMaxRageViewer IN)
{
	return CastOutHalf4Color(PackColor(PixelTexturedMaxRageViewer(IN)));
}
#endif // __WIN32PC_MAX_RAGEVIEWER

OutHalf4Color PS_TexturedWaterReflection(vertexOutputWaterReflection IN)
{
	float4 result = PixelTexturedWaterReflection(IN, false);
	return CastOutHalf4Color(PackReflection(result));
}

OutHalf4Color PS_TexturedWaterReflectionAlphaTest(vertexOutputWaterReflection IN)
{
	float4 result = PixelTexturedWaterReflection(IN, true);
	return CastOutHalf4Color(PackReflection(result));
}

#if CUBEMAP_REFLECTION_TECHNIQUES
OutHalf4Color PS_TexturedCube(vertexMegaOutputCube IN)
{
	OutHalf4Color OUT;
	OUT.col0 = PixelTexturedCube(IN);
	return OUT;
}
#endif // CUBEMAP_REFLECTION_TECHNIQUES

#if UI_TECHNIQUES
OutHalf4Color PS_Textured_UI(vertexOutput IN)
{
	float4 lightingResults = PixelTextured(IN, 0, USE_DIRECTIONAL_LIGHT_FWD_0_LIGHTS, false, false);
	lightingResults.rgb = pow(lightingResults.rgb, 1.0f / 2.2f);
	return CastOutHalf4Color(lightingResults);
}
#endif

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//
//
//

DeferredGBuffer DeferredTexturedCommon(vertexOutputD IN, float fFacing)
{
#if CLOTH
	// TODO: This doesn't seem to be needed anymore. Something, somewhere changed so the faces are not the wrong way anymore on PC
	// Go figure
	// - Svetli
	#if 0//RSG_PC
		fFacing *= -1;  // On PC the faces are the wrong way round so need to flip fFacing
	#endif
	IN.worldNormal.xyz *= fFacing;
#endif

#if ALPHA_CLIP && !SSTAA
	#if DIFFUSE_TEXTURE && !DONT_TRUST_DIFFUSE_ALPHA
		float alphaBlend = GetSurfaceAlpha( DIFFUSE_PS_INPUT.xy, DiffuseSampler);
	#elif  DONT_TRUST_DIFFUSE_ALPHA
		float alphaBlend = globalAlpha;
	#else
		float alphaBlend = 1.0;
	#endif
	rageDiscard(alphaBlend <= global_alphaRef);
#endif

	DeferredGBuffer OUT;
	SurfaceProperties surfaceInfo = GetSurfaceProperties(
#if VERTEXOUTPUTD_COLOR0 || BATCH_INSTANCING
		IN.color0.rgba, 
#else
		float4(1,1,1,1),
#endif // VERTEXOUTPUTD_COLOR0
#if PALETTE_TINT
		IN.color1.rgba,
#endif
#if DIFFUSE_TEXTURE
		DIFFUSE_PS_INPUT.xy,
		DiffuseSampler,
#endif
#if DIFFUSE2
		IN.texCoord.zw,
		DiffuseSampler2,
#elif OVERLAY_DIFFUSE2
		(float2)IN.overlayDiffuse2Coords,
		DiffuseSampler2,
	#if OVERLAY_DIFFUSE2_SEPARATE_ALPHA_SOURCE	
		#if defined(OVERLAY_DIFFUSE2_ALPHA_SOURCE)
			IN.OVERLAY_DIFFUSE2_ALPHA_SOURCE,
		#elif defined(USE_VERTEXOUTPUTD_INVCOLOR0DOTA_AS_OVERLAY_DIFFUSE2_ALPHA_SOURCE)
			1.0f - IN.color0.a,
		#else
			#error "Need to specify an alpha source for OVERLAY_DIFFUSE2_SEPARATE_ALPHA_SOURCE"
		#endif
	#endif
#endif
#if SPEC_MAP
		SPECULAR_PS_INPUT.xy,
#if SPECULAR_DIFFUSE
		DiffuseSampler,
#else
		SpecSampler,
#endif // SPECULAR_DIFFUSE
#endif	// SPEC_MAP
#if REFLECT
	#if !REFLECT_DYNAMIC 
		IN.worldEyePos.xyz,
	#else
		half3(0,0,0), // The reflection results are dead-code-eliminated below if REFLECT_DYNAMIC==1 
	#endif
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
	#if PARALLAX || PARALLAX_MAP_V2
	#if PARALLAX
		IN.tanEyePos.xyz,
	#endif
	#if PARALLAX_MAP_V2
		IN.worldEyePos,
		heightSampler,
	#if EDGE_WEIGHT
		IN.edgeWeight.xy,	
	#endif // EDGE_WEIGHT
	#endif // PARALLAX_MAP_V2
	#endif //PARALLAX || PARALLAX_MAP_V2
#endif	// NORMAL_MAP
#if VERTEXOUTPUTD_NORMAL
		IN.worldNormal.xyz
#else
		float3(0,0,1)
#endif //VERTEXOUTPUTD_NORMAL...
		);		
		
#if COLOR_VARIATION_TEXTURE
	surfaceInfo.surface_diffuseColor.rgba = PS_PalettizeColorVariationTexture(surfaceInfo.surface_diffuseColor.rgba);
#endif

	// Compute some surface information.
	StandardLightingProperties surfaceStandardLightingProperties = DeriveLightingPropertiesForCommonSurface( surfaceInfo );

#if BATCH_INSTANCING
	//Apply tint
	surfaceStandardLightingProperties.diffuseColor.rgb = lerp(surfaceStandardLightingProperties.diffuseColor.rgb, surfaceStandardLightingProperties.diffuseColor.rgb * IN.batchTint.rgb, IN.batchTint.a);
	surfaceStandardLightingProperties.naturalAmbientScale = IN.batchAo;
#endif //BATCH_INSTANCING
	
#if WETNESS_MULTIPLIER
	surfaceStandardLightingProperties.wetnessMult = wetnessMultiplier;
#endif
	
	// If you change these #if conditions, you'll want to revisit the conditions controlling VERTEXOUTPUTD_WORLDEYEPOS. Also take a look at
	// the comment above about dead-code-elimination (we pass in a bogus worldEyePos if REFLECT_DYNAMIC==1).
	#if REFLECT && !REFLECT_DYNAMIC 
	surfaceStandardLightingProperties.diffuseColor.xyz += surfaceStandardLightingProperties.reflectionColor;
	#endif	// REFLECT

#if defined(DECAL_SHADOW_ONLY)
	// check if facing away
	
	float faceAway = dot(IN.worldNormal.xyz, IN.worldEyePos.xyz) > 0.;
	// fade out decals on distance 
	float depth =length(IN.worldEyePos.xyz);
	float fadeOutDistance=70.f;
	float fadeOutLength=20.f;
	float fadeOut = saturate((depth-fadeOutDistance)/fadeOutLength);
	fadeOut *=faceAway;
	surfaceStandardLightingProperties.diffuseColor.a *= fadeOut;
#endif
#if SECOND_SPECULAR_LAYER
	if(1)
	{
		float _spec2Factor		= specular2Factor;
		float _spec2Intensity	= specular2ColorIntensity;
		// note: surfaceInfo.surface_fresnel already contains correctly rescaled fresnel
		float _spec2Fresnel		= surfaceInfo.surface_fresnel;
		#if SPEC_MAP
			#if SPEC_MAP_INTFALLOFF_PACK
				float _specSampInt		= surfaceInfo.surface_specMapSample.x*_spec2Fresnel;
				float _specSampFalloff	= surfaceInfo.surface_specMapSample.y;
			#else
				float _specSampInt		= dot(surfaceInfo.surface_specMapSample.xyz,specMapIntMask)*_spec2Fresnel;
				float _specSampFalloff	= surfaceInfo.surface_specMapSample.w;
			#endif
			_spec2Intensity			*= _specSampInt;
			_spec2Factor			*= _specSampFalloff;
		#endif
		float3 eyeDirection = normalize(IN.worldEyePos);

		float3 secondSpecular = PS_GetSingleDirectionalLightSpecular2Color(
									surfaceInfo.surface_worldNormal,
									eyeDirection,								// eyeDirection
									_spec2Factor,								// float	surfaceSpecularExponent,
									gDirectionalLight.xyz,
									float4(specular2Color*_spec2Intensity,1)	//gLightColor[0]			// lightColorIntensity
									);
		surfaceStandardLightingProperties.diffuseColor.xyz += secondSpecular;
	}
#endif //SECOND_SPECULAR_LAYER...

#if DISPLACEMENT_LOD_DEBUG
	surfaceStandardLightingProperties.diffuseColor = IN.color0;
#endif

	// Pack the information into the GBuffer(s).
	OUT = PackGBuffer(
					#ifdef DECAL_USE_NORMAL_MAP_ALPHA
						surfaceInfo.surface_worldNormal.xyzw,
					#else
						surfaceInfo.surface_worldNormal.xyz,
					#endif
						surfaceStandardLightingProperties );

#if WIND_DISPLACEMENT && !SHADER_FINAL
 	OUT.col0.rgb = OUT.col0.rgb*(1.0f - IN.windDebug.a) + IN.windDebug.rgb*IN.windDebug.a;
#endif

	return OUT;
}// end of PS_DeferredTextured()...

#if CLOTH
DeferredGBuffer PS_DeferredTextured(vertexOutputD IN, DECLARE_FACING_INPUT(fFacing))
{
	DECLARE_FACING_FLOAT(fFacing)
	return DeferredTexturedCommon(IN, fFacing);
}
#else
DeferredGBuffer PS_DeferredTextured(vertexOutputD IN)
{
	return DeferredTexturedCommon(IN, 1.0f);
}
#endif

DeferredGBuffer PS_DeferredTextured_SubSampleAlpha(vertexOutputD IN)
{
	DeferredGBuffer OUT;
#if DIFFUSE_TEXTURE && !DONT_TRUST_DIFFUSE_ALPHA
	float alphaBlend = GetSurfaceAlpha( DIFFUSE_PS_INPUT.xy, DiffuseSampler);
#elif  DONT_TRUST_DIFFUSE_ALPHA
	float alphaBlend = globalAlpha;
#else
	float alphaBlend = 1.0;
#endif
	float hardalpha =(alphaBlend-global_alphaRef)/((1.-global_alphaRef)*0.1f);
	alphaBlend=lerp( alphaBlend, hardalpha, HardAlphaBlend );

	rageDiscard( (SSAIsOpaquePixelFrac(IN.pos.xy) && alphaBlend <1.f) || alphaBlend <= 1./255. );

#if !RSG_ORBIS && !RSG_DURANGO
	float fGlobalFade = dot(globalFade, globalFade)/4;
	if (fGlobalFade < 1)
	{
		rageDiscard( SSAIsOpaquePixelFrac(IN.pos.xy));
		alphaBlend *= fGlobalFade;
	}
#endif // !RSG_ORBIS && !RSG_DURANGO
	
	OUT = DeferredTexturedCommon(IN, 1.0f);	
	OUT.col0.a = (half)alphaBlend;

#if SSTAA
	OUT.coverage = 0xffffffff;
#endif // SSTAA

#if WIND_DISPLACEMENT && !SHADER_FINAL
 	OUT.col0.rgb = OUT.col0.rgb*(1.0f - IN.windDebug.a) + IN.windDebug.rgb*IN.windDebug.a;
#endif

	return OUT;
}// end of PS_DeferredTextured_SubSampleAlpha()...

// Same as PS_DeferredTextured_SubSampleAlpha, except global alpha is ignored in favour of globalFade.
// Only used for SSA drawable crossfade, not global fade.
DeferredGBuffer PS_DeferredTextured_SubSampleAlphaClip(vertexOutputD IN)
{
#if !RSG_ORBIS && !RSG_DURANGO
	FadeDiscard( IN.pos, globalFade );
#endif

	DeferredGBuffer OUT;
	uint Coverage = 0;
#if DIFFUSE_TEXTURE && !DONT_TRUST_DIFFUSE_ALPHA
	float alphaBlend = GetSurfaceAlphaWithCoverage(DIFFUSE_PS_INPUT.xy, DiffuseSampler, 1.0f, global_alphaRef, Coverage);
#elif DONT_TRUST_DIFFUSE_ALPHA
	float alphaBlend = 1.0f;
#else
	float alphaBlend = 1.0;
#endif
	float hardalpha =(alphaBlend-global_alphaRef)/((1.-global_alphaRef)*0.1f);
	alphaBlend=lerp( alphaBlend, hardalpha, HardAlphaBlend );

	rageDiscard( (SSAIsOpaquePixelFrac(IN.pos.xy) && alphaBlend <1.f) || alphaBlend <= 1./255. );

	OUT = DeferredTexturedCommon(IN, 1.0f);	
	OUT.col0.a = (half)alphaBlend;

#if SSTAA
	OUT.coverage = 0xffffffff;
#endif // SSTAA

#if WIND_DISPLACEMENT && !SHADER_FINAL
 	OUT.col0.rgb = OUT.col0.rgb*(1.0f - IN.windDebug.a) + IN.windDebug.rgb*IN.windDebug.a;
#endif

	return OUT;
}// end of PS_DeferredTextured_SubSampleAlpha()...

// TODO --

OutHalf4Target0 PS_DeferredTextured_AlphaClip2(vertexOutputD IN) //vertexOutputAlphaClip2 IN)
{
	DeferredGBuffer OUT =(DeferredGBuffer)0;
#if DIFFUSE_TEXTURE && !DONT_TRUST_DIFFUSE_ALPHA
	float alphaBlend = GetSurfaceAlpha( DIFFUSE_PS_INPUT.xy, DiffuseSampler);
	// PS3: there's no AlphaTest in MRT mode, so we simulate AlphaTest functionality here:
#elif DONT_TRUST_DIFFUSE_ALPHA
	float alphaBlend = globalAlpha;
#else
	float alphaBlend = 1.0;
#endif
#if !RSG_ORBIS && !RSG_DURANGO
	float fGlobalFade = dot(globalFade, globalFade)/4;
	if (fGlobalFade < 1)
	{
		rageDiscard( SSAIsOpaquePixelFrac(IN.pos.xy));
		//alphaBlend = min(alphaBlend,fGlobalFade);
		alphaBlend *= fGlobalFade;	
	}
#endif // !RSG_ORBIS && !RSG_DURANGO

	float hardalpha =(alphaBlend-global_alphaRef)/((1.-global_alphaRef)*0.1f);
	alphaBlend=lerp( alphaBlend, hardalpha, HardAlphaBlend );
	rageDiscard(alphaBlend <= 1./255.);
	OUT.col0.a = (half)alphaBlend;

#if WIND_DISPLACEMENT && !SHADER_FINAL
 	OUT.col0.rgb = OUT.col0.rgb*(1.0f - IN.windDebug.a) + IN.windDebug.rgb*IN.windDebug.a;
#endif

	return CastOutHalf4Target0(OUT.col0);
}// end of PS_DeferredTextured_AlphaClip2()...

//
// special version of PS_DeferredTextured() with extra alpha test applied
// to simulate missing AlphaTest/AlphaToMask while in MRT mode:
//
DeferredGBuffer PS_DeferredTextured_AlphaClip(vertexOutputD IN)
{
#if !RSG_ORBIS && !RSG_DURANGO
	FadeDiscard( IN.pos, globalFade );
#endif

	DeferredGBuffer OUT;
	uint Coverage = 0;
#if DIFFUSE_TEXTURE && !DONT_TRUST_DIFFUSE_ALPHA
	float alphaBlend = GetSurfaceAlphaWithCoverage(DIFFUSE_PS_INPUT.xy, DiffuseSampler, globalAlpha, global_alphaRef, Coverage);
#elif DONT_TRUST_DIFFUSE_ALPHA
	float alphaBlend = globalAlpha;
#else
	float alphaBlend = 1.0;
#endif

	if (!ENABLE_TRANSPARENCYAA)
		rageDiscard(alphaBlend <= global_alphaRef);	

	OUT = DeferredTexturedCommon(IN, 1.0f);
	// PS3: there's no AlphaTest in MRT mode, so we simulate AlphaTest functionality here:

#if SSTAA
	if (ENABLE_TRANSPARENCYAA)
	{
		OUT.col0.a = 1;
		OUT.coverage = Coverage;
	}
	else
	{
		OUT.coverage = 0xffffffff;
	}
#endif // SSTAA
	
#if WIND_DISPLACEMENT && !SHADER_FINAL
 	OUT.col0.rgb = OUT.col0.rgb*(1.0f - IN.windDebug.a) + IN.windDebug.rgb*IN.windDebug.a;
#endif

#if GRID_BASED_DISCARDS
	uint2 iPos = uint2(IN.pos.xy) % uint2(2,2);
	if(OUT.col0.a < 0.5f && ((iPos.x & 1) || (iPos.y &1)))
	{
		discard;
	}
#endif // GRID_BASED_DISCARDS
	
	return OUT;
}// end of PS_DeferredTextured_AlphaClip()...

DeferredGBufferC PS_DeferredTextured_AlphaClipC(vertexOutputD IN)
{
	DeferredGBuffer result = PS_DeferredTextured_AlphaClip(IN);
	DeferredGBufferC OUT = PackDeferredGBufferC(result);
	return OUT;
}

DeferredGBufferNC PS_DeferredTextured_AlphaClipNC(vertexOutputD IN)
{
	DeferredGBuffer result = PS_DeferredTextured_AlphaClip(IN);
	DeferredGBufferNC OUT = PackDeferredGBufferNC(result);
	return OUT;
}

DeferredGBufferC PS_DeferredTextured_SubSampleAlphaClipC(vertexOutputD IN)
{
	DeferredGBuffer result = PS_DeferredTextured_SubSampleAlphaClip(IN);
	DeferredGBufferC OUT = PackDeferredGBufferC(result);
	return OUT;
}

DeferredGBufferNC PS_DeferredTextured_SubSampleAlphaClipNC(vertexOutputD IN)
{
	DeferredGBuffer result = PS_DeferredTextured_SubSampleAlphaClip(IN);
	DeferredGBufferNC OUT = PackDeferredGBufferNC(result);
	return OUT;
}

#if defined(USE_SEETHROUGH_TECHNIQUE)

OutFloat4Color PS_SeeThrough(vertexOutputSeeThrough IN)
{
	float4 result = SeeThrough_Render(IN);
	
	// Only base color, we don't want them to show up hot/bright.
	result.z *= matMaterialColorScale.w;
	
	return CastOutFloat4Color(result);
}
#endif // defined(USE_SEETHROUGH_TECHNIQUE)

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if __PS3 && defined(USE_MANUALDEPTH_TECHNIQUE)
	BeginSampler(	sampler, depthTexture, depthSampler, depthTexture)
	ContinueSampler(sampler, depthTexture, depthSampler, depthTexture)
		AddressU  = CLAMP;
		AddressV  = CLAMP;
		MINFILTER = POINT;
		MAGFILTER = POINT;
	EndSampler;

	half PS_TexturedUnlit_MD( vertexOutputUnlit IN, float4 vPos : VPOS ) : COLOR
	{
		float4 pixelColor = PixelTexturedUnlit(IN);

		float2 texCoord		= vPos.xy * gooScreenSize.xy;
		float depthSample	= texDepth2D(depthSampler, texCoord);
		float depthTest		= vPos.z<=depthSample;
		
		return float4(pixelColor.rgb, pixelColor.a * depthTest);
	}
#endif // __PS3 && defined(USE_MANUALDEPTH_TECHNIQUE)

///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////

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
		pass P0
		{
			MAX_TOOL_TECHNIQUE_RS
			VertexShader= compile VERTEXSHADER	VS_MaxTransform();
			PixelShader	= compile PIXELSHADER	PS_MaxPixelTextured();
		}
	}

#else //__MAX...

#if EMISSIVE_ADDITIVE
	#define FORWARD_RENDERSTATES() \
		AlphaBlendEnable	= true; \
		BlendOp				= ADD; \
		SrcBlend			= SRCALPHA; \
		DestBlend			= ONE; \
		ColorWriteEnable	= RED+GREEN+BLUE; \
		ZWriteEnable		= false; \
		zFunc				= FixedupLESS;
#elif defined(ALPHA_SHADER_DISABLE_ZWRITE)
	#define FORWARD_RENDERSTATES() \
		ZWriteEnable		= false; \
		zFunc				= FixedupLESSEQUAL;
#else
	#define FORWARD_RENDERSTATES()
#endif

	#if 0
		#define CGC_FORWARD_LIGHTING_FLAGS "-unroll all --O3 -fastmath"
	#else
		#define CGC_FORWARD_LIGHTING_FLAGS CGC_DEFAULTFLAGS
	#endif

	// ===============================
	// Lit (forward) techniques
	// ===============================
	#if FORWARD_TECHNIQUES
	technique draw
	{
		pass p0
		{
			FORWARD_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_Transform(gbOffsetEnable);
		#if __WIN32PC_MAX_RAGEVIEWER
			PixelShader  = compile PIXELSHADER	PS_TexturedMaxRageViewer()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		#else
			PixelShader  = compile PIXELSHADER	PS_Textured_Eight()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
		#endif
		}
	}
	#endif // FORWARD_TECHNIQUES

	#if FORWARD_TECHNIQUES && DRAWSKINNED_TECHNIQUES
	technique drawskinned
	{
		pass p0
		{
			FORWARD_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_TransformSkin(gbOffsetEnable);
		#if __WIN32PC_MAX_RAGEVIEWER
			PixelShader  = compile PIXELSHADER	PS_TexturedMaxRageViewer()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		#else
			PixelShader  = compile PIXELSHADER	PS_Textured_Eight()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
		#endif
		}
	}
	#endif // FORWARD_TECHNIQUES && DRAWSKINNED_TECHNIQUES

	#if FORWARD_TECHNIQUES
	technique lightweight0_draw
	{
		pass p0
		{
			FORWARD_RENDERSTATES()
#if APPLY_DOF_TO_ALPHA_DECALS
			VertexShader = compile VERTEXSHADER	VS_Transform_DOF(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_Textured_Zero_DOF()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
#else
			VertexShader = compile VERTEXSHADER	VS_Transform(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_Textured_Zero()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
#endif //APPLY_DOF_TO_ALPHA_DECALS
		}
	}
	technique lightweight0CutOut_draw
	{
		pass p0
		{
			FORWARD_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_Transform(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_Textured_Zero_CutOut()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
		}
	}
	#if GS_INSTANCED_SHADOWS
	GEN_GSINST_TYPE(LWShadow,vertexOutput,SV_ViewportArrayIndex)
	GEN_GSINST_FUNC_INSTANCEDPARAM(LWShadow,LWShadow,northInstancedVertexInputBump,vertexOutput,vertexOutput,VS_Transform_Common(IN,true),PS_Textured_Zero(IN))
	technique lightweight0_drawinstanced
	{
		pass p0
		{
			FORWARD_RENDERSTATES()
			GEN_GSINST_NOTECHPASS(LWShadow)
		}
	}
	#endif // GS_INSTANCED_SHADOWS
	#endif // FORWARD_TECHNIQUES

	#if FORWARD_TECHNIQUES && DRAWSKINNED_TECHNIQUES
	technique lightweight0_drawskinned
	{
		pass p0
		{
			FORWARD_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_TransformSkin(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_Textured_Zero()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
		}
	}
	technique lightweight0CutOut_drawskinned
	{
		pass p0
		{
			FORWARD_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_TransformSkin(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_Textured_Zero_CutOut()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
		}
	}
	#endif // FORWARD_TECHNIQUES && DRAWSKINNED_TECHNIQUES

	#if FORWARD_TECHNIQUES
	technique lightweight4_draw
	{
		pass p0
		{
			FORWARD_RENDERSTATES()
#if APPLY_DOF_TO_ALPHA_DECALS
			VertexShader = compile VERTEXSHADER	VS_Transform_DOF(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_Textured_Four_DOF()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
#else
			VertexShader = compile VERTEXSHADER	VS_Transform(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_Textured_Four()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
#endif //APPLY_DOF_TO_ALPHA_DECALS
		}
	}
	technique lightweight4CutOut_draw
	{
		pass p0
		{
			FORWARD_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_Transform(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_Textured_Four_CutOut()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
		}
	}
	#if GS_INSTANCED_SHADOWS
	GEN_GSINST_TYPE(LW4Shadow,vertexOutput,SV_ViewportArrayIndex)
	GEN_GSINST_FUNC_INSTANCEDPARAM(LW4Shadow,LW4Shadow,northInstancedVertexInputBump,vertexOutput,vertexOutput,VS_Transform_Common(IN,true),PS_Textured_Four(IN))
	technique lightweight4_drawinstanced
	{
		pass p0
		{
			FORWARD_RENDERSTATES()
			GEN_GSINST_NOTECHPASS(LW4Shadow)
		}
	}
	#endif // GS_INSTANCED_SHADOWS
	#endif // FORWARD_TECHNIQUES

	#if FORWARD_TECHNIQUES && DRAWSKINNED_TECHNIQUES
	technique lightweight4_drawskinned
	{
		pass p0
		{
			FORWARD_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_TransformSkin(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_Textured_Four()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
		}
	}
	technique lightweight4CutOut_drawskinned
	{
		pass p0
		{
			FORWARD_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_TransformSkin(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_Textured_Four_CutOut()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
		}
	}
	#endif // FORWARD_TECHNIQUES && DRAWSKINNED_TECHNIQUES

	#if FORWARD_TECHNIQUES
	technique lightweight8_draw
	{
		pass p0
		{
			FORWARD_RENDERSTATES()
#if APPLY_DOF_TO_ALPHA_DECALS
			VertexShader = compile VERTEXSHADER	VS_Transform_DOF(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_Textured_Eight_DOF()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
#else
			VertexShader = compile VERTEXSHADER	VS_Transform(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_Textured_Eight()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
#endif //APPLY_DOF_TO_ALPHA_DECALS
		}
	}
	technique lightweight8CutOut_draw
	{
		pass p0
		{
			FORWARD_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_Transform(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_Textured_Eight_CutOut()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
		}
	}
	#if GS_INSTANCED_SHADOWS
	GEN_GSINST_TYPE(LW8Shadow,vertexOutput,SV_ViewportArrayIndex)
	GEN_GSINST_FUNC_INSTANCEDPARAM(LW8Shadow,LW8Shadow,northInstancedVertexInputBump,vertexOutput,vertexOutput,VS_Transform_Common(IN,true),PS_Textured_Eight(IN))
	technique lightweight8_drawinstanced
	{
		pass p0
		{
			FORWARD_RENDERSTATES()
			GEN_GSINST_NOTECHPASS(LW8Shadow)
		}
	}
	#endif // GS_INSTANCED_SHADOWS
	#endif // FORWARD_TECHNIQUES

	#if FORWARD_TECHNIQUES && DRAWSKINNED_TECHNIQUES
	technique lightweight8_drawskinned
	{
		pass p0
		{
			FORWARD_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_TransformSkin(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_Textured_Eight()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
		}
	}
	technique lightweight8CutOut_drawskinned
	{
		pass p0
		{
			FORWARD_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_TransformSkin(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_Textured_Eight_CutOut()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
		}
	}
	#endif // FORWARD_TECHNIQUES && DRAWSKINNED_TECHNIQUES

	#if HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED
	technique lightweightHighQuality0_draw
	{
		pass p0
		{
			FORWARD_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_Transform(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_Textured_ZeroHighQuality()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
		}
	}
	technique lightweightHighQuality0CutOut_draw
	{
		pass p0
		{
			FORWARD_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_Transform(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_Textured_ZeroHighQuality_CutOut()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
		}
	}
	#endif // HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED

	#if HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED && DRAWSKINNED_TECHNIQUES
	technique lightweightHighQuality0_drawskinned
	{
		pass p0
		{
			FORWARD_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_TransformSkin(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_Textured_ZeroHighQuality()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
		}
	}
	technique lightweightHighQuality0CutOut_drawskinned
	{
		pass p0
		{
			FORWARD_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_TransformSkin(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_Textured_ZeroHighQuality_CutOut()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
		}
	}
	#endif // HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED && DRAWSKINNED_TECHNIQUES

	#if HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED
	technique lightweightHighQuality4_draw
	{
		pass p0
		{
			FORWARD_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_Transform(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_Textured_FourHighQuality()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
		}
	}
	technique lightweightHighQuality4CutOut_draw
	{
		pass p0
		{
			FORWARD_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_Transform(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_Textured_FourHighQuality_CutOut()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
		}
	}
	#endif // HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED

	#if HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED && DRAWSKINNED_TECHNIQUES
	technique lightweightHighQuality4_drawskinned
	{
		pass p0
		{
			FORWARD_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_TransformSkin(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_Textured_FourHighQuality()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
		}
	}
	technique lightweightHighQuality4CutOut_drawskinned
	{
		pass p0
		{
			FORWARD_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_TransformSkin(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_Textured_FourHighQuality_CutOut()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
		}
	}
	#endif // HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED && DRAWSKINNED_TECHNIQUES

	#if HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED
	technique lightweightHighQuality8_draw
	{
		pass p0
		{
			FORWARD_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_Transform(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_Textured_EightHighQuality()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
		}
	}
	technique lightweightHighQuality8CutOut_draw
	{
		pass p0
		{
			FORWARD_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_Transform(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_Textured_EightHighQuality_CutOut()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
		}
	}
	#endif // HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED

	#if HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED && DRAWSKINNED_TECHNIQUES
	technique lightweightHighQuality8_drawskinned
	{
		pass p0
		{
			FORWARD_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_TransformSkin(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_Textured_EightHighQuality()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
		}
	}
	technique lightweightHighQuality8CutOut_drawskinned
	{
		pass p0
		{
			FORWARD_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_TransformSkin(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_Textured_EightHighQuality_CutOut()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
		}
	}
	#endif // HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED && DRAWSKINNED_TECHNIQUES

	// ===============================
	// Unlit techniques
	// ===============================
	#if UNLIT_TECHNIQUES
	technique unlit_draw
	{
		pass p0
		{
			FORWARD_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_TransformUnlit(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_TexturedUnlit()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif // UNLIT_TECHNIQUES

	#if UNLIT_TECHNIQUES && DRAWSKINNED_TECHNIQUES
	technique unlit_drawskinned
	{
		pass p0
		{
			FORWARD_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_TransformSkinUnlit(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_TexturedUnlit()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif // UNLIT_TECHNIQUES && DRAWSKINNED_TECHNIQUES

	// ===============================
	// Deferred techniques
	// ===============================
	#if DEFERRED_TECHNIQUES && NON_CUTOUT_DEFERRED_TECHNIQUES
	technique deferred_draw
	{
		pass p0
		{
			#include "megashader_deferredRS.fxh"
			VertexShader = compile VERTEXSHADER	VS_TransformD(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_DeferredTextured()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#if USE_PN_TRIANGLES
	#if __SHADERMODEL == 40
	technique deferred_drawtessellated
	{
		pass p0
		{
			#include "megashader_deferredRS.fxh"
			VertexShader = compile VERTEXSHADER	VS_TransformD(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_DeferredTextured()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif //__SHADERMODEL == 40
	technique SHADER_MODEL_50_OVERRIDE(deferred_drawtessellated)
	{
		pass p0
		{
			#include "megashader_deferredRS.fxh"
			VertexShader = compile VSDS_SHADER VS_northVertexInputBump_PNTri(gbOffsetEnable);
			SetHullShader(compileshader(hs_5_0, HS_northVertexInputBump_PNTri()));
			SetDomainShader(compileshader(ds_5_0, DS_TransformD_PNTri()));
			PixelShader = compile PIXELSHADER PS_DeferredTextured()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif // USE_PN_TRIANGLES
	#endif // DEFERRED_TECHNIQUES && NON_CUTOUT_DEFERRED_TECHNIQUES

	// --------------

	#if DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES
#if SSTAA && RSG_PC
	technique deferredalphaclip_draw
	{
		pass p0
		{
			#include "megashader_deferredRS.fxh"
			VertexShader = compile VERTEXSHADER			VS_TransformD(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER			PS_DeferredTextured_AlphaClipNC()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
#endif // SSTAA && RSG_PC
	technique SSTA_TECHNIQUE(deferredalphaclip_draw)
	{
		pass p0
		{
			#include "megashader_deferredRS.fxh"
			VertexShader = compile VERTEXSHADER			VS_TransformD(gbOffsetEnable);
			PixelShader  = compile PS_GBUFFER_COVERAGE	PS_DeferredTextured_AlphaClipC()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#if USE_PN_TRIANGLES
	#if __SHADERMODEL == 40
	technique deferredalphaclip_drawtessellated
	{
		pass p0
		{
			#include "megashader_deferredRS.fxh"
			VertexShader = compile VERTEXSHADER	VS_TransformD(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_DeferredTextured_AlphaClipNC()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif //__SHADERMODEL == 40
	technique SHADER_MODEL_50_OVERRIDE(deferredalphaclip_drawtessellated)
	{ 
		pass p0
		{
			#include "megashader_deferredRS.fxh"
			VertexShader = compile VSDS_SHADER VS_northVertexInputBump_PNTri(gbOffsetEnable);
			SetHullShader(compileshader(hs_5_0, HS_northVertexInputBump_PNTri()));
			SetDomainShader(compileshader(ds_5_0, DS_TransformD_PNTri()));
			PixelShader = compile PS_GBUFFER_COVERAGE PS_DeferredTextured_AlphaClipC()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif // USE_PN_TRIANGLES

#if SSTAA && RSG_PC
	technique deferredsubsamplealphaclip_draw
	{
		pass p0
		{
			#include "megashader_deferredRS.fxh"
			VertexShader = compile VERTEXSHADER			VS_TransformD(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER			PS_DeferredTextured_SubSampleAlphaClipNC()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
#endif // SSTAA && RSG_PC
	technique SSTA_TECHNIQUE(deferredsubsamplealphaclip_draw)
	{
		pass p0
		{
			#include "megashader_deferredRS.fxh"
			VertexShader = compile VERTEXSHADER			VS_TransformD(gbOffsetEnable);
			PixelShader  = compile PS_GBUFFER_COVERAGE	PS_DeferredTextured_SubSampleAlphaClipC()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#if USE_PN_TRIANGLES
	#if __SHADERMODEL == 40
	technique deferredsubsamplealphaclip_drawtessellated
	{
		pass p0
		{
			#include "megashader_deferredRS.fxh"
			VertexShader = compile VERTEXSHADER	VS_TransformD(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_DeferredTextured_SubSampleAlphaClipNC()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif //__SHADERMODEL == 40
	technique SHADER_MODEL_50_OVERRIDE(deferredsubsamplealphaclip_drawtessellated)
	{ 
		pass p0
		{
			#include "megashader_deferredRS.fxh"
			VertexShader = compile VSDS_SHADER VS_northVertexInputBump_PNTri(gbOffsetEnable);
			SetHullShader(compileshader(hs_5_0, HS_northVertexInputBump_PNTri()));
			SetDomainShader(compileshader(ds_5_0, DS_TransformD_PNTri()));
			PixelShader = compile PS_GBUFFER_COVERAGE PS_DeferredTextured_SubSampleAlphaClipC()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif // USE_PN_TRIANGLES
	#endif // DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES
	
	// --------------

	#if DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES
	technique deferredsubsamplewritealpha_draw
	{	
		// preliminary pass to write down alpha
		pass p0
		{
			DEFERRED_SUBSAMPLEWRITEALPHA_RENDERSTATES_MEGASHADER()
			VertexShader = compile VERTEXSHADER VS_TransformD(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER PS_DeferredTextured_AlphaClip2()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#if USE_PN_TRIANGLES
	#if __SHADERMODEL == 40
	technique deferredsubsamplewritealpha_drawtessellated
	{	
		// preliminary pass to write down alpha
		pass p0
		{
			DEFERRED_SUBSAMPLEWRITEALPHA_RENDERSTATES_MEGASHADER()
			VertexShader = compile VERTEXSHADER VS_TransformD(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER PS_DeferredTextured_AlphaClip2()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif //__SHADERMODEL == 40
	technique SHADER_MODEL_50_OVERRIDE(deferredsubsamplewritealpha_drawtessellated)
	{
		// preliminary pass to write down alpha
		pass p0
		{
			DEFERRED_SUBSAMPLEWRITEALPHA_RENDERSTATES_MEGASHADER()
			VertexShader = compile VSDS_SHADER VS_northVertexInputBump_PNTri(gbOffsetEnable);
			SetHullShader(compileshader(hs_5_0, HS_northVertexInputBump_PNTri()));
			SetDomainShader(compileshader(ds_5_0, DS_TransformD_PNTri()));
			PixelShader = compile PIXELSHADER PS_DeferredTextured_AlphaClip2()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif // USE_PN_TRIANGLES
	#endif // DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES

	// --------------

	#if DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES
	technique deferredsubsamplealpha_draw
	{
		// preliminary pass to write down alpha
		pass p0
		{
			VertexShader = compile VERTEXSHADER			VS_TransformD(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER			PS_DeferredTextured_SubSampleAlpha()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#if USE_PN_TRIANGLES
	#if __SHADERMODEL == 40
	technique deferredsubsamplealpha_drawtessellated
	{
		// preliminary pass to write down alpha
		pass p0
		{
			VertexShader = compile VERTEXSHADER	VS_TransformD(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_DeferredTextured_SubSampleAlpha()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif //__SHADERMODEL == 40
	technique SHADER_MODEL_50_OVERRIDE(deferredsubsamplealpha_drawtessellated)
	{
		// preliminary pass to write down alpha
		pass p0
		{
			VertexShader = compile VSDS_SHADER VS_northVertexInputBump_PNTri(gbOffsetEnable);
			SetHullShader(compileshader(hs_5_0, HS_northVertexInputBump_PNTri()));
			SetDomainShader(compileshader(ds_5_0, DS_TransformD_PNTri()));
			PixelShader = compile PS_GBUFFER_COVERAGE PS_DeferredTextured_SubSampleAlpha()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif // USE_PN_TRIANGLES
	#endif // DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES

	// ===============================

	#if DEFERRED_TECHNIQUES && NON_CUTOUT_DEFERRED_TECHNIQUES && DRAWSKINNED_TECHNIQUES
	technique deferred_drawskinned
	{
		pass p0
		{
			#include "megashader_deferredRS.fxh"
			VertexShader = compile VERTEXSHADER	VS_TransformSkinD(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_DeferredTextured()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#if USE_PN_TRIANGLES
	#if __SHADERMODEL == 40
	technique deferred_drawskinnedtessellated
	{
		pass p0
		{
			#include "megashader_deferredRS.fxh"
			VertexShader = compile VERTEXSHADER	VS_TransformSkinD(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_DeferredTextured()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif //__SHADERMODEL == 40
	technique SHADER_MODEL_50_OVERRIDE(deferred_drawskinnedtessellated)
	{
		pass p0
		{
			#include "megashader_deferredRS.fxh"
			VertexShader = compile VSDS_SHADER VS_northSkinVertexInputBump_PNTri(gbOffsetEnable);
			SetHullShader(compileshader(hs_5_0, HS_northVertexInputBump_PNTri()));
			SetDomainShader(compileshader(ds_5_0, DS_TransformD_PNTri()));
			PixelShader = compile PIXELSHADER PS_DeferredTextured()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif // USE_PN_TRIANGLES
	#endif // DEFERRED_TECHNIQUES && NON_CUTOUT_DEFERRED_TECHNIQUES && DRAWSKINNED_TECHNIQUES

	// --------------

	#if DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES && DRAWSKINNED_TECHNIQUES
	technique deferredalphaclip_drawskinned
	{
		pass p0
		{
			#include "megashader_deferredRS.fxh"
			VertexShader = compile VERTEXSHADER	VS_TransformSkinD(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_DeferredTextured_AlphaClipNC()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#if USE_PN_TRIANGLES 
	#if __SHADERMODEL == 40
	technique deferredalphaclip_drawskinnedtessellated
	{
		pass p0
		{
			#include "megashader_deferredRS.fxh"
			VertexShader = compile VERTEXSHADER	VS_TransformSkinD(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_DeferredTextured_AlphaClipNC()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	} 
	#endif //__SHADERMODEL == 40
	technique SHADER_MODEL_50_OVERRIDE(deferredalphaclip_drawskinnedtessellated)
	{
		pass p0
		{
			#include "megashader_deferredRS.fxh"
			VertexShader = compile VSDS_SHADER VS_northSkinVertexInputBump_PNTri(gbOffsetEnable);
			SetHullShader(compileshader(hs_5_0, HS_northVertexInputBump_PNTri()));
			SetDomainShader(compileshader(ds_5_0, DS_TransformD_PNTri()));
			PixelShader = compile PS_GBUFFER_COVERAGE PS_DeferredTextured_AlphaClipC()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif // USE_PN_TRIANGLES
	#endif // DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES && DRAWSKINNED_TECHNIQUES
	
	// --------------

	#if DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES && DRAWSKINNED_TECHNIQUES
	technique deferredsubsamplealpha_drawskinned
	{
		pass p0
		{
			#include "megashader_deferredRS.fxh"
			VertexShader = compile VERTEXSHADER	VS_TransformSkinD(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_DeferredTextured_SubSampleAlpha()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#if USE_PN_TRIANGLES
	#if __SHADERMODEL == 40
	technique deferredsubsamplealpha_drawskinnedtessellated
	{
		pass p0
		{
			#include "megashader_deferredRS.fxh"
			VertexShader = compile VERTEXSHADER	VS_TransformSkinD(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_DeferredTextured_SubSampleAlpha()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif //__SHADERMODEL == 40
	technique SHADER_MODEL_50_OVERRIDE(deferredsubsamplealpha_drawskinnedtessellated)
	{
		pass p0
		{
			#include "megashader_deferredRS.fxh"
			VertexShader = compile VSDS_SHADER VS_northSkinVertexInputBump_PNTri(gbOffsetEnable);
			SetHullShader(compileshader(hs_5_0, HS_northVertexInputBump_PNTri()));
			SetDomainShader(compileshader(ds_5_0, DS_TransformD_PNTri()));
			PixelShader = compile PIXELSHADER PS_DeferredTextured_SubSampleAlpha()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif // USE_PN_TRIANGLES
	#endif // DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES && DRAWSKINNED_TECHNIQUES

	#if DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES && DRAWSKINNED_TECHNIQUES
	technique deferredsubsamplewritealpha_drawskinned
	{
		pass p0
		{
			DEFERRED_SUBSAMPLEWRITEALPHA_RENDERSTATES_MEGASHADER()
			VertexShader = compile VERTEXSHADER	VS_TransformSkinD(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_DeferredTextured_AlphaClip2()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#if USE_PN_TRIANGLES
	#if __SHADERMODEL == 40
	technique deferredsubsamplewritealpha_drawskinnedtessellated
	{
		pass p0
		{
			DEFERRED_SUBSAMPLEWRITEALPHA_RENDERSTATES_MEGASHADER()
			VertexShader = compile VERTEXSHADER	VS_TransformSkinD(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_DeferredTextured_AlphaClip2()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif //__SHADERMODEL == 40
	technique SHADER_MODEL_50_OVERRIDE(deferredsubsamplewritealpha_drawskinnedtessellated)
	{
		pass p0
		{
			DEFERRED_SUBSAMPLEWRITEALPHA_RENDERSTATES_MEGASHADER()
			VertexShader = compile VSDS_SHADER VS_northSkinVertexInputBump_PNTri(gbOffsetEnable);
			SetHullShader(compileshader(hs_5_0, HS_northVertexInputBump_PNTri()));
			SetDomainShader(compileshader(ds_5_0, DS_TransformD_PNTri()));
			PixelShader = compile PIXELSHADER PS_DeferredTextured_AlphaClip2()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif // USE_PN_TRIANGLES
	#endif // DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES && DRAWSKINNED_TECHNIQUES

	// ===============================
	// Water reflection techniques
	// ===============================
	#if WATER_REFLECTION_TECHNIQUES
	technique waterreflection_draw
	{
		pass p0
		{
			FORWARD_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_TransformWaterReflection();
			PixelShader  = compile PIXELSHADER	PS_TexturedWaterReflection()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}

	technique waterreflectionalphaclip_draw
	{
		pass p0
		{
			FORWARD_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_TransformWaterReflection();
			PixelShader  = compile PIXELSHADER	PS_TexturedWaterReflectionAlphaTest()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}

	#endif // WATER_REFLECTION_TECHNIQUES

	#if WATER_REFLECTION_TECHNIQUES && DRAWSKINNED_TECHNIQUES
	technique waterreflection_drawskinned
	{
		pass p0
		{
			FORWARD_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_TransformSkinWaterReflection();
			PixelShader  = compile PIXELSHADER	PS_TexturedWaterReflection()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif // WATER_REFLECTION_TECHNIQUES && DRAWSKINNED_TECHNIQUES

	#if CUBEMAP_REFLECTION_TECHNIQUES
	technique cube_draw
	{
		pass p0
		{
			FORWARD_RENDERSTATES()
			CullMode=CCW;
			VertexShader = compile VERTEXSHADER	VS_TransformCube();
			PixelShader  = compile PIXELSHADER	PS_TexturedCube()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}

	#if GS_INSTANCED_CUBEMAP
	GEN_GSINST_TYPE(Mega,vertexMegaOutputCube,SV_RenderTargetArrayIndex)
	GEN_GSINST_FUNC_INSTANCEDPARAM(Mega,Mega,northInstancedVertexInputBump,vertexMegaOutputCube,vertexMegaOutputCube,VS_TransformCube_Common(IN,true),PS_TexturedCube(IN))
	technique cubeinst_draw
	{
		GEN_GSINST_TECHPASS(Mega,all)
	}
	#endif

	#endif // CUBEMAP_REFLECTION_TECHNIQUES

	// ===============================
	// Manualdepth techniques
	// ===============================
	#if __PS3 && defined(USE_MANUALDEPTH_TECHNIQUE)
	technique manualdepth_draw
	{
		pass p0
		{
			ZWriteEnable = false;
			ZEnable      = false;
			VertexShader = compile VERTEXSHADER	VS_Transform(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_TexturedUnlit_MD()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif // __PS3 && defined(USE_MANUALDEPTH_TECHNIQUE)

	#if __PS3 && defined(USE_MANUALDEPTH_TECHNIQUE) && DRAWSKINNED_TECHNIQUES
	technique manualdepth_drawskinned
	{
		pass p0
		{
			ZWriteEnable = false;
			ZEnable      = false;
			VertexShader = compile VERTEXSHADER	VS_TransformSkin(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_TexturedUnlit_MD()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif // __PS3 && defined(USE_MANUALDEPTH_TECHNIQUE) && DRAWSKINNED_TECHNIQUES

	// ===============================
	// Seethrough techniques
	// ===============================
	#if defined(USE_SEETHROUGH_TECHNIQUE)
	technique seethrough_draw
	{
		pass p0
		{
			ZwriteEnable = true;
			VertexShader = compile VERTEXSHADER	VS_Transform_SeeThrough();
			PixelShader  = compile PIXELSHADER	PS_SeeThrough()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif // defined(USE_SEETHROUGH_TECHNIQUE)

	#if defined(USE_SEETHROUGH_TECHNIQUE) && DRAWSKINNED_TECHNIQUES
	technique seethrough_drawskinned
	{
		pass p0
		{
			ZwriteEnable = true;
			VertexShader = compile VERTEXSHADER	VS_TransformSkin_SeeThrough();
			PixelShader  = compile PIXELSHADER	PS_SeeThrough()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif // defined(USE_SEETHROUGH_TECHNIQUE) && DRAWSKINNED_TECHNIQUES

	// ===============================
	// UI techniques (add in a gamma correct)
	// ===============================

	#if UI_TECHNIQUES
	technique ui_draw
	{
		pass p0
		{
			FORWARD_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_Transform(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_Textured_UI()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
		}
	}
	#endif

	#if UI_TECHNIQUES && DRAWSKINNED_TECHNIQUES
	technique ui_drawskinned
	{
		pass p0
		{
			FORWARD_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_TransformSkin(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_Textured_UI()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
		}
	}
	#endif // UI_TECHNIQUES && DRAWSKINNED_TECHNIQUES

#endif // __MAX

#if !__MAX
#if UMOVEMENTS_TEX

	VS_CascadeShadows_OUT VS_CascadeShadowsUMovements(northInstancedVertexInputBump instIN) 
	{
		int nInstIndex;
		northVertexInputBump IN;
		FetchInstanceVertexData(instIN, IN, nInstIndex);

		megaVertInstanceData inst;
		GetInstanceData(nInstIndex, inst);

		VS_CascadeShadows_OUT OUT;
		float3 inPos = IN.pos;
		inPos	= VS_ApplyMicromovements(inPos, IN.texCoord1.xxy);
		OUT.pos = ApplyCompositeWorldTransform(float4(inPos, 1), INSTANCING_ONLY_ARG(inst.worldMtx) gWorldViewProj); 
		SHADOW_USE_TEXTURE_ONLY(OUT.tex = IN.texCoord0.xy); 
		return OUT; 
	}

	SHADERTECHNIQUE_CASCADE_SHADOWS_UMOVEMENTS()

#elif USE_DAY_NIGHT_VERTEX_SCALE

	VS_CascadeShadows_OUT VS_CascadeShadowsUMovements(northInstancedVertexInputBump instIN) 
	{
		int nInstIndex;
		northVertexInputBump IN;
		FetchInstanceVertexData(instIN, IN, nInstIndex);

		megaVertInstanceData inst;
		GetInstanceData(nInstIndex, inst);

		VS_CascadeShadows_OUT OUT;
		float3 inPos = IN.pos;
		OUT.pos = DAY_NIGHT_VERTEX_SCALE*ApplyCompositeWorldTransform(float4(inPos, 1), INSTANCING_ONLY_ARG(inst.worldMtx) gWorldViewProj); 
		OUT.pos = ApplyDayNightNan(OUT.pos, inst.worldMtx);
		SHADOW_USE_TEXTURE_ONLY(OUT.tex = IN.texCoord0.xy); 
		return OUT; 
	}

	SHADERTECHNIQUE_CASCADE_SHADOWS_UMOVEMENTS()

#else
	SHADERTECHNIQUE_CASCADE_SHADOWS()

#endif
SHADERTECHNIQUE_LOCAL_SHADOWS(VS_LinearDepth, VS_LinearDepthSkin, PS_LinearDepth)
SHADERTECHNIQUE_ENTITY_SELECT(VS_Transform(gbOffsetEnable), VS_TransformSkin(gbOffsetEnable))

#if BATCH_INSTANCING && GRASS_BATCH_CS_CULLING
#	include "../Vegetation/Grass/BatchCS.fxh"
#endif //BATCH_INSTANCING && GRASS_BATCH_CS_CULLING

#if !defined(SHADER_FINAL)
#if USE_PN_TRIANGLES
#if __SHADERMODEL == 40
technique entityselect_drawtessellated
{
	pass p0
	{
		#include "megashader_deferredRS.fxh"
		VertexShader = compile VERTEXSHADER	VS_TransformD(gbOffsetEnable);
		PixelShader  = compile PIXELSHADER	PS_EntitySelectID_BANK_ONLY()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
#endif //__SHADERMODEL == 40
technique SHADER_MODEL_50_OVERRIDE(entityselect_drawtessellated)
{
	pass p0
	{
		#include "megashader_deferredRS.fxh"
		VertexShader = compile VSDS_SHADER VS_northVertexInputBump_PNTri(gbOffsetEnable);
		SetHullShader(compileshader(hs_5_0, HS_northVertexInputBump_PNTri()));
		SetDomainShader(compileshader(ds_5_0, DS_TransformD_PNTri()));
		PixelShader = compile PIXELSHADER PS_EntitySelectID_BANK_ONLY()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
#endif // USE_PN_TRIANGLES

#if USE_PN_TRIANGLES && DRAWSKINNED_TECHNIQUES

#if __SHADERMODEL == 40
technique entityselect_drawskinnedtessellated
{
	pass p0
	{
		#include "megashader_deferredRS.fxh"
		VertexShader = compile VERTEXSHADER	VS_TransformSkinD(gbOffsetEnable);
		PixelShader  = compile PIXELSHADER	PS_EntitySelectID_BANK_ONLY()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
#endif //__SHADERMODEL == 40
technique SHADER_MODEL_50_OVERRIDE(entityselect_drawskinnedtessellated)
{
	pass p0
	{
		#include "megashader_deferredRS.fxh"
		VertexShader = compile VSDS_SHADER VS_northSkinVertexInputBump_PNTri(gbOffsetEnable);
		SetHullShader(compileshader(hs_5_0, HS_northVertexInputBump_PNTri()));
		SetDomainShader(compileshader(ds_5_0, DS_TransformD_PNTri()));
		PixelShader = compile PIXELSHADER PS_EntitySelectID_BANK_ONLY()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
#endif // USE_PN_TRIANGLES

#endif // !__FINAL

#include "../Debug/debug_overlay_megashader.fxh"
#endif	//!__MAX...

#endif	// TECHNIQUES

#endif	// __GTA_MEGASHADER_FXH__




