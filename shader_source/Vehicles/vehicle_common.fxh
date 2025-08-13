//
// gta vehicle shader common header:
//
//	20/10/2005 - Andrzej:	- initial;
//	07/11/2005 - Andrzej:	- USE_DIRT_LAYER added;
//	08/11/2005 - Andrzej:	- USE_SECOND_UV_LAYER added (for decals, prints on sides, etc);
//	24/11/2005 - Andrzej:	- fog support added;
//	31/01/2006 - Andrzej:	- bodycolor modulation moved over to PS;
//	02/02/2006 - Andrzej:	- USE_DIRT_UV_LAYER added (independent set of UVs for dirt mapping);
//	26/01/2007 - Andrzej:	- USE_SECOND_SPECULAR_LAYER (independent specular);
//	02/07/2007 - Andrzej:	- DiffuseColor2 added (as fix for BS#38834);
//	06/09/2007 - Andrzej:	- VEHICLE_TYRE_DEFORM added;
//	07/09/2007 - Andrzej:	- VEHICLE_INSTANCED_WHEEL_SHADER added;
//	28/09/2007 - Andrzej:	- VEHCONSTs support added;
//	12/10/2007 - Andrzej:	- VEHICLE_TYRE_DEBUG_MEASUREMENT added (for debug only);
//	---------- - --------	- ------------------------------------------------------
//	30/06/2008 - Andrzej:	- cleanup and refactor of vehicle shaders;
//	04/12/2008 - Andrzej:	- USE_VEHICLE_ANIMATED_TRACK_UV added;
//	10/02/2009 - Andrzej:	- USE_SPECULAR_UV1 added;
//	06/04/2009 - Andrzej:	- USE_VEHICLESHADER_SNOW added;
//	12/04/2009 - Andrzej:	- tool technique;
//	30/10/2009 - Andrzej:	- USE_VEHICLESHADER_SNOW_USE_DIRTUV added;
//	10/03/2010 - Andrzej:	- USE_DIFFUSETEX_TILEUV added;
//	30/04/2010 - Andrzej:	- Max: Samplers' UINames with uv channel mapping;
//	22/07/2010 - Andrzej:	- USE_SPECTEX_TILEUV added;
//	04/08/2010 - Andrzej:	- IGNORE_SPECULAR_MAP_VALUES added;
//	10/08/2010 - Andrzej:	- USE_VEHICLESHADER_SNOW_BEFORE_DIFFUSE2 added;
//  16/08/2013 - Andrzej:	- USE_DETAIL_MAP_UV_LAYER added;
//	24/08/2014 - Andrzej:	- USE_SECOND_UV_LAYER_UV0 added;
//  22/07/2015 - Andrzej:	- USE_THIRD_UV_LAYER added;
//	02/08/2016 - Andrzej:	- USE_VEHICLE_ANIMATED_TRACK2_UV added;
//  02/03/2017 - Andrzej;	- USE_VEHICLE_ANIMATED_TRACKAMMO_UV added;
//
//
//
//
//
#ifndef __GTA_VEHICLE_COMMON_FXH__
#define __GTA_VEHICLE_COMMON_FXH__

//		USE_NORMAL_MAP
//			Process appropriate tangent space vectors in vertex shader, and pull normal info from
//			a texture.
//		USE_SPECULAR
//			Factor in light specularity
//			USE_SPECULAR_UV1
//				Specular UV coordinates come from texcoord1
//			IGNORE_SPECULAR_MAP
//				Do not use a specular texture when computing specular strength
//			TWEAK_SPECULAR_ON_ALPHA
//				Tweak Alpha of surface based on specular, to allow for bright spots on windows.
//		USE_REFLECT
//			Enable reflections
//			USE_SPHERICAL_REFLECT
//				Use a spherical reflection model instead of a cubic reflection model
//			USE_DYNAMIC_REFLECT
//				Use a dynamic reflection model based on paraboloid mapping (cube -> paraboloid)
//		USE_EMISSIVE
//			Enable CPV's to be treated as "light sources" instead of material colors
//
//		USE_EMISSIVE_DIMMER
//			Enable CPV's to be treated as "light sources" instead of material colors
//
//		USE_DEFAULT_TECHNIQUES
//			Use the techniques in this shader versus rolling your own.
//
//		DISABLE_DEPTH_WRITE
//			Disable ZWrite for all forward technique.
//
//		USE_SUPERSAMPLING
//			Force super-sampling for better MSAA/EQAA quality


#ifdef USE_DIFFUSETEX_USE_UV1
	#define DIFFUSE_UV1			(1)				// main diffuse texture to use UV1 channel
#else
	#define DIFFUSE_UV1			(0)
#endif

#ifdef USE_DIFFUSETEX_USE_UV2
	#define DIFFUSE_UV2			(!DIFFUSE_UV1)	// main diffuse texture to use UV2 channel
#else
	#define DIFFUSE_UV2			(0)
#endif

#ifdef USE_VEHICLESHADER_SNOW_USE_UV1
	#define VEH_SNOW_UV1		(1)
#else
	#define VEH_SNOW_UV1		(0)
#endif

#if DIFFUSE_UV1
	#define  UINAME_SAMPLER_DIFFUSETEXTURE	"2: Diffuse Texture"
#elif DIFFUSE_UV2
	#define  UINAME_SAMPLER_DIFFUSETEXTURE	"3: Diffuse Texture"
#else
	#define  UINAME_SAMPLER_DIFFUSETEXTURE	"1: Diffuse Texture"
#endif

#ifdef USE_SUPERSAMPLING
	#define VEHICLE_SUPERSAMPLE		(RSG_DURANGO || RSG_ORBIS)
#else
	#define VEHICLE_SUPERSAMPLE		0
#endif

#define IS_VEHICLE_SHADER

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

#ifndef USE_REFLECT
#define USE_REFLECT
#endif

//If vehicle shader is not an alpha shader, we enabled Forward Cloud Shadows
//This enables any opaque/deferred shader to have the same cloud shadows algorithm
//as in the cascade shadow reveal, and so when we force a vehicle to be completely forward/alpha
//the same codepath will be used and there will not be any pops
#ifndef ALPHA_SHADER
#define USE_FORWARD_CLOUD_SHADOWS
#define USE_FOGRAY_FORWARDPASS
#endif

#ifndef USE_DYNAMIC_REFLECT
#define USE_DYNAMIC_REFLECT
#endif

#if (__SHADERMODEL >= 40)
#define USE_ALPHACLIP_FADE
#endif // __SHADERMODEL >= 40

#if __PSSL
#pragma pack_matrix (row_major)
#endif

#include "../common.fxh"

#if __MAX
	// __MAX limitations:
	//#undef USE_SECOND_UV_LAYER
	//#undef USE_DIRT_LAYER
	#undef USE_VEHICLE_DAMAGE
	#undef USE_VEHICLE_DAMAGE_EDGE
	//#undef USE_SPECULAR_UV1
	//#undef USE_VEHICLESHADER_SNOW
	#undef USE_VEHICLE_INSTANCED_WHEEL_SHADER
	#undef VEHICLE_TYRE_DEFORM
	#undef USE_EMISSIVE_DIMMER
#endif //__MAX...
#include "../Util/macros.fxh"
#include "../Util/skin.fxh"
#include "../Vehicles/vehicle_damage.fxh"

#define USE_FOG						// use fog by default

//
//
//
//
#if defined(USE_VEHICLE_INSTANCED_WHEEL_SHADER)
	#define VEHICLE_INSTANCED_WHEEL_SHADER				(1)
	BeginConstantBufferPagedDX10(matWheelBuffer, b4)
	ROW_MAJOR float4x4 matWheelWorld : matWheelWorld0				REGISTER2(vs, c64);
	ROW_MAJOR float4x4 matWheelWorldViewProj : matWorldViewProj0	REGISTER2(vs, c68);
	EndConstantBufferDX10(matWheelBuffer)
#else
	#define VEHICLE_INSTANCED_WHEEL_SHADER				(0)
#endif //VEHICLE_INSTANCED_WHEEL_SHADER...


// -----------------------------------------
// Process externally set #define's for configuring functions, variables, structures, etc.
#if DIFFUSE_UV1
	#define DIFFUSE_VS_INPUT						IN.texCoord2.xy
	#define DIFFUSE_VS_OUTPUT						OUT.texCoord.zw
	#define DIFFUSE_PS_INPUT						IN.texCoord.zw
#elif DIFFUSE_UV2
	#define DIFFUSE_VS_INPUT						IN.texCoord3.xy
	#define DIFFUSE_VS_OUTPUT						OUT.texCoord3.xy
	#define DIFFUSE_PS_INPUT						IN.texCoord3.xy
#else
	#define DIFFUSE_VS_INPUT						IN.texCoord0.xy
	#define DIFFUSE_VS_OUTPUT						OUT.texCoord.xy
	#define DIFFUSE_PS_INPUT						IN.texCoord.xy
#endif


#ifdef USE_NORMAL_MAP
	#define NORMAL_MAP								(1)
	#if defined(USE_NORMAL_MAP_USE_UV0)
		#define BUMP_VS_INPUT						IN.texCoord0.xy
		#define BUMP_VS_OUTPUT						OUT.texCoord.xy
		#define BUMP_PS_INPUT						IN.texCoord.xy
		#define UINAME_SAMPLER_BUMPTEXTURE			"1: Bump Texture"
	#else
		#define BUMP_VS_INPUT						DIFFUSE_VS_INPUT
		#define BUMP_VS_OUTPUT						DIFFUSE_VS_OUTPUT
		#define BUMP_PS_INPUT						DIFFUSE_PS_INPUT
		#if DIFFUSE_UV1
			#define  UINAME_SAMPLER_BUMPTEXTURE		"2: Bump Texture"
		#elif DIFFUSE_UV2
			#define  UINAME_SAMPLER_BUMPTEXTURE		"3: Bump Texture"
		#else
			#define  UINAME_SAMPLER_BUMPTEXTURE		"1: Bump Texture"
		#endif
	#endif
#else
	#define NORMAL_MAP								(0)
#endif	// USE_NORMAL_MAP


// Determine whether to pack in two uv's into one register
#ifdef USE_SECOND_UV_LAYER
	#define DIFFUSE2								(1)
	#define	DIFFUSE2_IGNORE_SPEC_MODULATION			(1)

	#ifdef USE_SECOND_UV_LAYER_UV0
		#define DIFFUSE2_UV0						(1)
	#else
		#define DIFFUSE2_UV0						(0)
	#endif
	#define DIFFUSE2_UV1							(!DIFFUSE2_UV0)

	#if DIFFUSE2_UV0
		#define DIFFUSE_TEX_COORDS					float2
		#define DIFFUSE2_VS_INPUT					IN.texCoord0.xy
		#define DIFFUSE2_VS_OUTPUT					OUT.texCoord.xy
		#define DIFFUSE2_PS_INPUT					IN.texCoord.xy
		#define UINAME_SAMPLER_DIFFUSETEXTURE2		"1: Secondary Texture"
	#else
		#define DIFFUSE_TEX_COORDS					float4
		#define DIFFUSE2_VS_INPUT					IN.texCoord2.xy
		#define DIFFUSE2_VS_OUTPUT					OUT.texCoord.zw
		#define DIFFUSE2_PS_INPUT					IN.texCoord.zw
		#define UINAME_SAMPLER_DIFFUSETEXTURE2		"2: Secondary Texture"
	#endif

	#if defined(USE_SECOND_UV_LAYER_NORMAL_MAP_USE_UV0)
		#define BUMP2_VS_INPUT						IN.texCoord0.xy
		#define BUMP2_VS_OUTPUT						OUT.texCoord.xy
		#define BUMP2_PS_INPUT						IN.texCoord.xy
		#define UINAME_SAMPLER_BUMPTEXTURE2			"1: Secondary Bump Texture"
	#else
		// reuses DIFFUSE2 bindings:
		#define BUMP2_VS_INPUT						DIFFUSE2_VS_INPUT
		#define BUMP2_VS_OUTPUT						DIFFUSE2_VS_OUTPUT
		#define BUMP2_PS_INPUT						DIFFUSE2_PS_INPUT
		#if DIFFUSE2_UV0
			#define UINAME_SAMPLER_BUMPTEXTURE2		"1: Secondary Bump Texture"
		#else
			#define UINAME_SAMPLER_BUMPTEXTURE2		"2: Secondary Bump Texture"
		#endif
	#endif
	
	#ifdef USE_SECOND_UV_LAYER_ANISOTROPIC
		#define DIFFUSE2_ANISOTROPIC			(1)
	#else
		#define DIFFUSE2_ANISOTROPIC			(0)
	#endif
#else
	#define DIFFUSE2							(0)
	#define DIFFUSE_TEX_COORDS					float2
	#define DIFFUSE2_UV0						(0)
	#define DIFFUSE2_UV1						(0)
#endif //USE_SECOND_UV_LAYER...

#ifdef USE_SECOND_UV_LAYER_NORMAL_MAP
	#define NORMAL_MAP2							(DIFFUSE2)
#else
	#define NORMAL_MAP2							(0)
#endif

#ifdef USE_THIRD_UV_LAYER
	#if DIFFUSE2
		#error "Can't use USE_SECOND_UV_LAYER and USE_THIRD_UV_LAYER at the same time!"
	#endif

	#define DIFFUSE3								(1)
	#define	DIFFUSE3_IGNORE_SPEC_MODULATION			(1)

	#ifdef USE_THIRD_UV_LAYER_UV0
		#define DIFFUSE3_UV0						(1)
	#else
		#define DIFFUSE3_UV0						(0)
	#endif
	#define DIFFUSE3_UV1							(!DIFFUSE3_UV0)

	#undef DIFFUSE_TEX_COORDS
	#if DIFFUSE3_UV0
		#define DIFFUSE_TEX_COORDS					float2
		#define DIFFUSE3_VS_INPUT					IN.texCoord0.xy
		#define DIFFUSE3_VS_OUTPUT					OUT.texCoord.xy
		#define DIFFUSE3_PS_INPUT					IN.texCoord.xy
		#define UINAME_SAMPLER_DIFFUSETEXTURE3		"1: Secondary Texture"
		#define UINAME_SAMPLER_BUMPTEXTURE3			"1: Secondary Bump Texture"
	#else
		#define DIFFUSE_TEX_COORDS					float4
		#define DIFFUSE3_VS_INPUT					IN.texCoord2.xy
		#define DIFFUSE3_VS_OUTPUT					OUT.texCoord.zw
		#define DIFFUSE3_PS_INPUT					IN.texCoord.zw
		#define UINAME_SAMPLER_DIFFUSETEXTURE3		"2: Secondary Texture"
		#define UINAME_SAMPLER_BUMPTEXTURE3			"2: Secondary Bump Texture"
	#endif

	#ifdef USE_THIRD_UV_LAYER_ANISOTROPIC
		#define DIFFUSE3_ANISOTROPIC			(1)
	#else
		#define DIFFUSE3_ANISOTROPIC			(0)
	#endif
#else
	#define DIFFUSE3							(0)
	#define DIFFUSE3_UV0						(0)
	#define DIFFUSE3_UV1						(0)
#endif //USE_THIRD_UV_LAYER...

#ifdef USE_DIFFUSE_COLOR_TINT
	#define DIFFUSE_COLOR_TINT					(1)
#else
	#define DIFFUSE_COLOR_TINT					(0)
#endif

#ifdef USE_PAINT_RAMP
	#define PAINT_RAMP (VEHICLE_SUPPORT_PAINT_RAMP)
#else
	#define PAINT_RAMP (0)
#endif

//
//
//
//
#define DIRT_UV									(0)		// default value (no dirt UVs)

#if __SHADERMODEL < 30
	#undef	USE_DIRT_LAYER						// no dirt support for older shaders
#endif

#ifdef USE_DIRT_LAYER
	#define DIRT_LAYER							(1)

	#ifdef USE_DIRT_UV_LAYER
		#undef	DIRT_UV
		#define DIRT_UV							(1)

		#if DIFFUSE_UV1 || DIFFUSE2_UV1 || DIFFUSE3_UV1 || VEH_SNOW_UV1
			#define DIRT_VS_INPUT					IN.texCoord3.xy
			#define UINAME_SAMPLER_DIRTTEXTURE		"3: Dirt Texture"
			#define UINAME_SAMPLER_DIRTBUMPTEXTURE	"3: Dirt Bump Texture"
		#else
			#define DIRT_VS_INPUT					IN.texCoord2.xy
			#define UINAME_SAMPLER_DIRTTEXTURE		"2: Dirt Texture"
			#define UINAME_SAMPLER_DIRTBUMPTEXTURE	"2: Dirt Bump Texture"
		#endif
		#define DIRT_VS_OUTPUT						OUT.texCoord3.xy
		#define DIRT_PS_INPUT						IN.texCoord3.xy
	#else	
		#define UINAME_SAMPLER_DIRTTEXTURE			"1: Dirt Texture"
		#define UINAME_SAMPLER_DIRTBUMPTEXTURE		"1: Dirt Bump Texture"
	#endif

	#ifdef USE_NORMAL_MAP
		#ifdef USE_DIRT_NORMALMAP
			#define DIRT_NORMALMAP				(1)
		#else
			#define DIRT_NORMALMAP				(0)
		#endif
	#endif //USE_NORMAL_MAP...

	#ifdef USE_DIRT_ON_TOP
		#define DIRT_ON_TOP						(1)
	#else	
		#define DIRT_ON_TOP						(0)
	#endif
#else
	#define DIRT_LAYER							(0)
#endif //USE_DIRT_LAYER...

#ifdef USE_BURNOUT_IN_MATDIFFCOLOR
	#define	BURNOUT_IN_MATDIFFCOLOR				(1)
#else
	#define	BURNOUT_IN_MATDIFFCOLOR				(0)
#endif

#ifdef USE_SPECULAR
	#define SPECULAR							(1)
	
	#ifdef IGNORE_SPECULAR_MAP
		#define SPEC_MAP						(0)
	#else
		#define SPEC_MAP						(1)
	#endif	// IGNORE_SPECULAR_MAP
	
	#define SPEC_MAP_INTFALLOFF_PACK			(0)
	#ifdef USE_SPEC_MAP_INTFALLOFFFRESNEL_PACK
		#define SPEC_MAP_INTFALLOFFFRESNEL_PACK	(1)
	#else
		#define SPEC_MAP_INTFALLOFFFRESNEL_PACK	(0)
	#endif

	#ifdef IGNORE_SPECULAR_MAP_VALUES					// SpecMap sampler is still sampled (and used for Specular2, etc.),
		#define IGNORE_SPEC_MAP_VALUES			(1)		// but not really used for controlling basic Specular
	#else
		#define IGNORE_SPEC_MAP_VALUES			(0)
	#endif
	
	#if defined(USE_SPECULAR_UV1)
		#if DIFFUSE2_UV1
			#define SPECULAR_VS_INPUT		DIFFUSE2_VS_INPUT
			#define SPECULAR_VS_OUTPUT		DIFFUSE2_VS_OUTPUT
			#define SPECULAR_PS_INPUT		DIFFUSE2_PS_INPUT
			#define SPECULAR_UV1			(1)
		#elif DIFFUSE3_UV1
			#define SPECULAR_VS_INPUT		DIFFUSE3_VS_INPUT
			#define SPECULAR_VS_OUTPUT		DIFFUSE3_VS_OUTPUT
			#define SPECULAR_PS_INPUT		DIFFUSE3_PS_INPUT
			#define SPECULAR_UV1			(1)
		#elif DIRT_UV
			#define SPECULAR_VS_INPUT		DIRT_VS_INPUT
			#define SPECULAR_VS_OUTPUT		DIRT_VS_OUTPUT
			#define SPECULAR_PS_INPUT		DIRT_PS_INPUT
			#define SPECULAR_UV1			(1)
		#else
			#error "Can't use USE_SPECULAR_UV1 without DIFFUSE2, DIFFUSE3 or DIRT_UV!"
		#endif
		#define UINAME_SAMPLER_SPECTEXTURE	"2: Specular Texture"
	#else
		#define SPECULAR_VS_INPUT			DIFFUSE_VS_INPUT
		#define SPECULAR_VS_OUTPUT			DIFFUSE_VS_OUTPUT
		#define SPECULAR_PS_INPUT			DIFFUSE_PS_INPUT
		#define SPECULAR_UV1				(0)
		#if DIFFUSE_UV1
			#define UINAME_SAMPLER_SPECTEXTURE	"2: Specular Texture"
		#elif DIFFUSE_UV2
			#define UINAME_SAMPLER_SPECTEXTURE	"3: Specular Texture"
		#else
			#define UINAME_SAMPLER_SPECTEXTURE	"1: Specular Texture"
		#endif
	#endif	// USE_SPECULAR_UV1

	#ifdef TWEAK_SPECULAR_ON_ALPHA
		#define ALPHA_SPECULAR		(1)
	#else
		#define ALPHA_SPECULAR		(0)
	#endif
#else
	#define SPECULAR				(0)
	#define SPEC_MAP				(0)
	#define ALPHA_SPECULAR			(0)
#endif		// USE_SPECULAR

//
// second specular layer is not valid without specular
//
#ifdef USE_SECOND_SPECULAR_LAYER
	#define SECOND_SPECULAR_LAYER			(SPECULAR)
#else
	#define SECOND_SPECULAR_LAYER			(0)
#endif//USE_SECOND_SPECULAR_LAYER...

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


#ifdef USE_EMISSIVE_ADDITIVE
	#define USE_EMISSIVE
	#define	EMISSIVE_ADDITIVE				(1)
	#ifdef USE_EMISSIVE_DIMMER
		#define DIMMER						(1)
	#else
		#define DIMMER						(0)
	#endif
	#ifdef USE_EMISSIVE_DIMMER_19
		#define DIMMER_19					(DIMMER && 1)
	#else
		#define DIMMER_19					(0)
	#endif
#else
	#define EMISSIVE_ADDITIVE				(0)
#endif

#ifdef USE_SPECIAL_GLOW
	#define USE_GLOW						(1)
#else // USE_SPECIAL_GLOW
	#define USE_GLOW						(0)
#endif // USE_SPECIAL_GLOW

#ifdef USE_EMISSIVE
	#define EMISSIVE						(1)
	#ifdef USE_EMISSIVE_DIMMER
		#define DIMMER						(1)
	#else
		#define DIMMER						(0)
	#endif
	#ifdef USE_EMISSIVE_DIMMER_19
		#define DIMMER_19					(DIMMER && 1)
	#else
		#define DIMMER_19					(0)
	#endif
#else
	#define EMISSIVE						(0)
#endif		// USE_EMISSIVE

#ifdef USE_VEHICLESHADER_SNOW
	#define VEHICLESHADER_SNOW							(1)
	#if defined(USE_VEHICLESHADER_SNOW_USE_UV0)
		#define SNOW_VS_INPUT							IN.texCoord0.xy
		#define SNOW_VS_OUTPUT							OUT.texCoord.xy
		#define SNOW_PS_INPUT							IN.texCoord.xy
		#define UINAME_SAMPLER_ENVEFFTEX0 				"1: EnvEff Texture0"
		#define UINAME_SAMPLER_ENVEFFTEX1 				"1: EnvEff Texture1"
	#elif defined(USE_VEHICLESHADER_SNOW_USE_UV1)
		#undef  DIFFUSE_TEX_COORDS	
		#define DIFFUSE_TEX_COORDS						float4
		#define SNOW_VS_INPUT							IN.texCoord2.xy
		#define SNOW_VS_OUTPUT							OUT.texCoord.zw
		#define SNOW_PS_INPUT							IN.texCoord.zw
		#define UINAME_SAMPLER_ENVEFFTEX0 				"2: EnvEff Texture0"
		#define UINAME_SAMPLER_ENVEFFTEX1 				"2: EnvEff Texture1"
	#elif defined(USE_VEHICLESHADER_SNOW_USE_DIRTUV)
		#if !defined(USE_DIRT_UV_LAYER)
			#error "Snow layer wants to use 2nd UV channel. USE_DIRT_UV_LAYER must be enabled."
		#endif
		#define SNOW_VS_INPUT							DIRT_VS_INPUT
		#define SNOW_VS_OUTPUT							DIRT_VS_OUTPUT
		#define SNOW_PS_INPUT							(float2)DIRT_PS_INPUT
		#define UINAME_SAMPLER_ENVEFFTEX0 				"2: EnvEff Texture0"
		#define UINAME_SAMPLER_ENVEFFTEX1 				"2: EnvEff Texture1"
	#else
		// otherwise uses DIFFUSE input/output:
		#define SNOW_VS_INPUT							DIFFUSE_VS_INPUT
		#define SNOW_VS_OUTPUT							DIFFUSE_VS_OUTPUT
		#define SNOW_PS_INPUT							(float2)DIFFUSE_PS_INPUT
		#define UINAME_SAMPLER_ENVEFFTEX0 				"1: EnvEff Texture0"
		#define UINAME_SAMPLER_ENVEFFTEX1 				"1: EnvEff Texture1"
	#endif
	#if defined(USE_VEHICLESHADER_SNOW_BEFORE_DIFFUSE2)
		#define VEHICLESHADER_SNOW_BEFORE_DIFFUSE2		(DIFFUSE2)
	#else
		#define VEHICLESHADER_SNOW_BEFORE_DIFFUSE2		(0)
	#endif
#else
	#define VEHICLESHADER_SNOW							(0)
	#define VEHICLESHADER_SNOW_BEFORE_DIFFUSE2			(0)
#endif

#ifdef USE_DEFAULT_TECHNIQUES
	#define TECHNIQUES						(1)
#else
	#define TECHNIQUES						(0)
#endif	// USE_DEFAULT_TECHNIQUES

#ifdef DISABLE_DEPTH_WRITE
	#define NO_DEPTH_WRITE		(1)
#else
	#define NO_DEPTH_WRITE		(0)
#endif	

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

#ifdef USE_EMISSIVE_NIGHTONLY
	#define EMISSIVE_NIGHTONLY				(1)
	#if !EMISSIVE
		#error "EMISSIVE_NIGHTONLY requested, but EMISSIVE variable not enabled!"
	#endif
#else
	#define EMISSIVE_NIGHTONLY				(0)
#endif


#if defined(USE_ALPHA_DOF) && (__SHADERMODEL >= 40) && (!__MAX)
	#define APPLY_DOF_TO_ALPHA				(1)
#else
	#define APPLY_DOF_TO_ALPHA				(0)
#endif

#ifdef USE_CLOTH
	#define CLOTH						(1)
#else
	#define CLOTH						(0)
#endif

// -----------------------------------------
//
BEGIN_RAGE_CONSTANT_BUFFER( vehiclecommonlocals,b8 )

#ifdef USE_DIFFUSETEX_TILEUV
	// diffuseTex tileUV:
	#if defined(VEHCONST_DIFFUSETEXTILEUV)
		#define DiffuseTexTileUV		(VEHCONST_DIFFUSETEXTILEUV)
	#else
		float DiffuseTexTileUV : DiffuseTexTileUV
		<
			int nostrip	= 1;	// dont strip
			string UIName = "DiffuseTex TileUV";
			float UIMin = 1.0;
			float UIMax = 32.0;
			float UIStep = 0.1;
		> = 8.0f;
	#endif //VEHCONST_DIFFUSETEXTILEUV...
#else
	#define DiffuseTexTileUV			(1.0f)
#endif

#ifdef USE_NO_MATERIAL_DIFFUSE_COLOR
	#define	MATERIAL_DIFFUSE_COLOR			(0)
#else
	#define	MATERIAL_DIFFUSE_COLOR			(1)
#endif

#if MATERIAL_DIFFUSE_COLOR
// global material diffuse color:
float3 matDiffuseColor : DiffuseColor
<
	string UIName	= "Vehicle Diffuse Color";
	float UIMin		= 0.0;
	float UIMax		= 10.0;
	float UIStep	= 0.01;
> = float3(1, 1, 1);
#endif // MATERIAL_DIFFUSE_COLOR...

#if PAINT_RAMP
float matDiffuseSpecularRampEnabled = -1;
#define matIsDiffuseRampEnabled (matDiffuseSpecularRampEnabled > 0.0)
#define matIsSpecularRampEnabled (abs(matDiffuseSpecularRampEnabled) > 1.5)
#endif

float4 matDiffuseColor2 : DiffuseColor2 = float4(1,1,1,1);	// second material color to scale overlays for burntout cars

#if DIFFUSE_COLOR_TINT
	float4 matDiffuseColorTint : DiffuseColorTint = float4(1,1,1,0);	// RGB_tint/Alpha_add (mainly for glass)
#endif

// VARIABLES THAT ARE SET BASED ON SWITCHES ENABLED
#if EMISSIVE || EMISSIVE_ADDITIVE
	#if defined(VEHCONST_EMISSIVE_HDR_MULTIPLIER)
		#define emissiveMultiplier		(VEHCONST_EMISSIVE_HDR_MULTIPLIER)
	#else
		float emissiveMultiplier : EmissiveMultiplier
		<
			string UIName = "Emissive HDR Multiplier";
			float UIMin = 0.0;
			float UIMax = 16.0;
			float UIStep = 0.1;
			WIN32PC_DONTSTRIP
		> = 0.0;
	#endif //VEHCONST_EMISSIVE_HDR_MULTIPLIER...

	#if DIMMER
		// 00 LW_DEFAULT
		// 01 LW_HEADLIGHT_L
		// 02 LW_HEADLIGHT_R
		// 03 LW_TAILLIGHT_L
		
		// 04 LW_TAILLIGHT_R
		// 05 LW_INDICATOR_FL
		// 06 LW_INDICATOR_FR
		// 07 LW_INDICATOR_RL
		
		// 08 LW_INDICATOR_RR
		// 09 LW_BRAKELIGHT_L
		// 10 LW_BRAKELIGHT_R
		// 11 LW_BRAKELIGHT_M
		
		// 12 LW_REVERSINGLIGHT_L
		// 13 LW_REVERSINGLIGHT_R
		// 14 LW_EXTRALIGHT_1
		// 15 LW_EXTRALIGHT_2
		
		// 16 LW_EXTRALIGHT_3
		// 17 LW_EXTRALIGHT_4
		// 18 LW_EXTRALIGHT

		// 19 LW_SIREN (special hardcoded case only)
		float4 dimmerSetPacked[5];
	#endif // DIMMER
#endif //EMISSIVE || EMISSIVE_ADDITIVE

#ifdef USE_VEHICLE_ANIMATED_TRACK_UV
	#define VEHICLE_ANIMATED_TRACK_UV				(1)
#else
	#define VEHICLE_ANIMATED_TRACK_UV				(0)

	#ifdef USE_VEHICLE_ANIMATED_TRACK2_UV
		#define VEHICLE_ANIMATED_TRACK2_UV			(1)
	#else
		#define VEHICLE_ANIMATED_TRACK2_UV			(0)

		#ifdef USE_VEHICLE_ANIMATED_TRACKAMMO_UV
			#define VEHICLE_ANIMATED_TRACKAMMO_UV	(1)
		#else
			#define VEHICLE_ANIMATED_TRACKAMMO_UV	(0)
		#endif
	#endif
#endif


#if VEHICLE_ANIMATED_TRACK_UV
//
//
// uvAnimation variables (changed by runtime code):
//
float4	TrackAnimUV : TrackAnimUV0 = float4(0, 0, 0, 0);	// x=right track movement, y=left track movement
#endif //VEHICLE_ANIMATED_TRACK_UV

#if VEHICLE_ANIMATED_TRACK2_UV
float4	Track2AnimUV : Track2AnimUV0 = float4(0, 0, 0, 0);	// x=right track movement, y=left track movement
#endif //VEHICLE_ANIMATED_TRACK2_UV

#if VEHICLE_ANIMATED_TRACKAMMO_UV
float4	TrackAmmoAnimUV : TrackAmmoAnimUV0 = float4(0, 0, 0, 0);	// x=right track movement, y=left track movement
#endif //VEHICLE_ANIMATED_TRACKAMMO_UV

#if USE_GLOW
	float DiskBrakeGlow = 0.0f;
	#define glowColor float3(0.98f,0.25f,0.0f)
	#define intensity 100.0f
#endif // USE_GLOW...


//
// dirt level:
//
float4	dirtLevelMod : DirtLevelMod = float4(1.0f,1.0f,1.0f,1.0f);	// x=dirtLevel <0; 1>, y=dirtModulator <0; 1>, z=specIntScale; w=2ndSpecIntScale
#if defined(DIRT_LAYER_LEVEL_LIMIT)
	#define dirtLevel			(min(dirtLevelMod.x, DIRT_LAYER_LEVEL_LIMIT))
#else
	#define dirtLevel			(dirtLevelMod.x)
#endif
#define dirtModulator			(dirtLevelMod.y)
#define nonDirtSpecIntScale		(dirtLevelMod.z)		// non-dirt shaders: specInt scale: 1.0 (dirt) or 0.0 (burnout);
#define dirtOrBurnout			(dirtLevelMod.z)		// dirt=1.0, burnout=0.0
#define burnout2ndLayerScale	(1.0-dirtLevelMod.z)	// dirt shaders: 2nd layer scale: 0.0 (dirt) or 1.0 (burnout);
#define secondSpecIntScale		(dirtLevelMod.w)		// 2nd spec specInt scale

#if DIRT_LAYER
	float3	dirtColor	 : DirtColor
	<
		string UIName	= "Vehicle Dirt Color";
		float UIMin		= 0.0;
		float UIMax		= 1.0;
		float UIStep	= 0.001;
	> = float3(0.231372f, 0.223529f, 0.203921f);	// dirtColor RGB=(59,57,52)
#endif //DIRT_LAYER...




// VARIABLES THAT ARE SET BASED ON SWITCHES ENABLED
#if SPECULAR

#define FRESNEL (1)

#if FRESNEL
	#if defined(VEHCONST_FRESNEL)
		#define specularFresnel		(VEHCONST_FRESNEL)
	#else
		float specularFresnel : Fresnel
		<
			string UIName = "Specular Fresnel";
			float UIMin = 0.0;
			float UIMax = 1.0;
			float UIStep = 0.01;
			WIN32PC_DONTSTRIP
		> = 0.97f;
	#endif
#else
	#define specularFresnel (0.97f)
#endif

	#if defined(VEHCONST_SPECULAR1_FALLOFF)
		#define specularFalloffMult		(VEHCONST_SPECULAR1_FALLOFF)
	#else
		float specularFalloffMult : Specular
		<
			string UIName = "Specular Falloff";
			float UIMin = 0.0;
			float UIMax = 10000.0;
			float UIStep = 0.1;
			WIN32PC_DONTSTRIP
		> = 180.0f;
	#endif //VEHCONST_SPECULAR1_FALLOFF...

	#if defined(VEHCONST_SPECULAR1_INTENSITY)
		#define specularIntensityMult (VEHCONST_SPECULAR1_INTENSITY)
	#else
		float specularIntensityMult : SpecularColor
		<
			string UIName = "Specular Intensity";
			float UIMin = 0.0;
			float UIMax = 10000.0;
			float UIStep = 0.1;
			WIN32PC_DONTSTRIP
		> = 0.15f;
	#endif //VEHCONST_SPECULAR1_INTENSITY...

	#if SPEC_MAP
		#if SPEC_MAP_INTFALLOFF_PACK || SPEC_MAP_INTFALLOFFFRESNEL_PACK
			// no specMapIntMask here
		#else
			float3 specMapIntMask : SpecularMapIntensityMask
			<
				string UIWidget = "slider";
				float UIMin = 0.0;
				float UIMax = 1.0;
				float UIStep = .01;
				string UIName = "specular map intensity mask color";
				WIN32PC_DONTSTRIP
			> = { 1.0, 0.0, 0.0};
		#endif

	#ifdef USE_SPECTEX_TILEUV
		// specmap tileUV:
		#if defined(VEHCONST_SPECTEXTILEUV)
			#define specTexTileUV			(VEHCONST_SPECTEXTILEUV)
		#else
			float specTexTileUV : SpecTexTileUV
			<
				int nostrip	= 1;	// dont strip
				string UIName = "SpecMapTileUV";
				float UIMin = 1.0;
				float UIMax = 32.0;
				float UIStep = 0.1;
			> = 1.0f;
		#endif //VEHCONST_SPECTEXTILEUV...
	#else
		#define specTexTileUV				(1.0f)
	#endif // USE_SPECTEX_TILEUV...
	
	#endif	// SPEC_MAP
#endif		// SPECULAR

#if SECOND_SPECULAR_LAYER
	#if defined(VEHCONST_SPECULAR2_FALLOFF)
		#define specular2Factor (VEHCONST_SPECULAR2_FALLOFF)
	#else
		float specular2Factor : Specular2Factor
		<
			string UIName = "Specular2 Falloff";
			float UIMin = 0.0;
			float UIMax = 10000.0;
			float UIStep = 0.1;
			WIN32PC_DONTSTRIP
		> = 40.0f;
	#endif //VEHCONST_SPECULAR2_FALLOFF...

	#if defined(VEHCONST_SPECULAR2_INTENSITY)
		#define specular2ColorIntensity (VEHCONST_SPECULAR2_INTENSITY)
	#else
		float specular2ColorIntensity : specular2ColorIntensity
		<
			string UIName = "Specular2 Intensity";
			float UIMin = 0.0;
			float UIMax = 10000.0;
			float UIStep = 0.1;
			WIN32PC_DONTSTRIP
		> = 1.7f;	//off by default 1.0;
	#endif //VEHCONST_SPECULAR2_INTENSITY...

	#ifdef USE_SECOND_SPECULAR_LAYER_LOCKCOLOR
		// color of second specular is locked to white (1,1,1)
		#define SECOND_SPECULAR_LAYER_LOCKCOLOR (1)
	#else
		#define SECOND_SPECULAR_LAYER_LOCKCOLOR (0)
		#define specular2Color		(specular2Color_DirLerp.rgb)
	#endif
	float4 specular2Color_DirLerp : Specular2Color = float4(0.0f, 0.5f, 0.0f, 0.0f);
	#define secondSpecDirLerp		(specular2Color_DirLerp.a)

	#if 1 //#ifdef USE_SECOND_SPECULAR_LAYER_NOFRESNEL
		// 2nd spec layer not scaled by fresnel
		#define SECOND_SPECULAR_LAYER_NOFRESNEL	(1)
	#else
		#define SECOND_SPECULAR_LAYER_NOFRESNEL	(0)
	#endif
#endif //SECOND_SPECULAR_LAYER...

#if NORMAL_MAP || NORMAL_MAP2
	#if defined(VEHCONST_BUMPINESS)
		#define bumpiness (VEHCONST_BUMPINESS)
	#else
		float bumpiness : Bumpiness
		<
			string UIWidget = "slider";
			float UIMin = 0.0;
			float UIMax = 200.0;
			float UIStep = .01;
			string UIName = "Bumpiness";
			WIN32PC_DONTSTRIP
		> = 1.0;
	#endif //VEHCONST_BUMPINESS...
#endif		// NORMAL

#if REFLECT
	#if defined(VEHCONST_REFLECTIVITY)
		#define reflectivePower	(VEHCONST_REFLECTIVITY)
	#else	
		float reflectivePower : Reflectivity
		<
			string UIName = "Reflectivity";
			float UIMin = -10.0;
			float UIMax = 10.0;
			float UIStep = 0.1;
			WIN32PC_DONTSTRIP
		> = 0.45;
	#endif //VEHCONST_REFLECTIVITY...
#endif	// REFLECT


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
				WIN32PC_DONTSTRIP
			> = 0.2;
		#else
			float diffuse2SpecMod : Diffuse2ModSpec
			<
				string UIName="Texture2 Specular Modifier";
				string UIHelp="Amount of specular power added by alpha of secondary texture";
				float UIMin = 0.0;
				float UIMax = 1000.0;
				float UIStep = 0.1;
				WIN32PC_DONTSTRIP
			> = 0.8;
		#endif // DIFFUSE_SPEC_MASK
	#endif	// DIFFUSE2

	#if DIFFUSE3
		#if DIFFUSE3_SPEC_MASK
			float diffuse3SpecClamp : Diffuse3MinSpec
			<
				string UIName="Texture3 Min Specular";
				string UIHelp="Minimum amount of specular allowed for secondary texture";
				float UIMin = 0.0;
				float UIMax = 10.0;
				float UIStep = 0.01;
				WIN32PC_DONTSTRIP
			> = 0.2;
		#else
			float diffuse3SpecMod : Diffuse3ModSpec
			<
				string UIName="Texture3 Specular Modifier";
				string UIHelp="Amount of specular power added by alpha of secondary texture";
				float UIMin = 0.0;
				float UIMax = 1000.0;
				float UIStep = 0.1;
				WIN32PC_DONTSTRIP
			> = 0.8;
		#endif // DIFFUSE_SPEC_MASK
	#endif	// DIFFUSE3
#endif // SPECULAR

// snowStep:
float envEffThickness : envEffThickness0
<
	int nostrip	= 1;	// dont strip
	string UIName	= "EnvEff: Max thickness";
	int UIMin	= 0;
	int UIMax	= 100;
	int UIStep	= 1;
> = 25.0f;	// must be divided by 1000 (=0.025f) - rescaling of snowStep done with snowScale

#define snowStep	(envEffThickness)	// see note above


// snowScale: <0; 1> - controlled by runtime code
float2 envEffScale : envEffScale0
<
	int nostrip	= 1;	// dont strip
> = float2(1.0f, 0.001f);

#define snowScale	(envEffScale.x)	// snow scale (=1.0f)
#define snowScaleV	(envEffScale.y)	// snow scale =(1.0f/1000.0f) with compensation for snowStep

// snowtex tileUV:
#if defined(VEHCONST_ENVEFFTEXTILEUV)
	#define envEffTexTileUV		(VEHCONST_ENVEFFTEXTILEUV)
#else
	float envEffTexTileUV : envEffTexTileUV
	<
		int nostrip	= 1;	// dont strip
		string UIName = "EnvEff texTileUV";
		float UIMin = 1.0;
		float UIMax = 32.0;
		float UIStep = 0.1;
	> = 8.0f;
#endif //VEHCONST_ENVEFFTEXTILEUV...

#if defined(VEHICLE_GLASS_CRACK_TEXTURE)

float4 vehglassCrackTextureParams[2] = { float4(0,0,0,0), float4(1,1,1,1) };

#define vehglassCrackTextureParams_amount     vehglassCrackTextureParams[0].x
#define vehglassCrackTextureParams_scale      vehglassCrackTextureParams[0].y
#define vehglassCrackTextureParams_bumpamount vehglassCrackTextureParams[0].z
#define vehglassCrackTextureParams_bumpiness  vehglassCrackTextureParams[0].w
#define vehglassCrackTextureParams_tint       vehglassCrackTextureParams[1]

#endif // defined(VEHICLE_GLASS_CRACK_TEXTURE)

EndConstantBufferDX10( vehiclecommonlocals )



#if DIMMER && __PS3
	#if !__MAX
	passthrough { // optimisation: declare masks[] as read-only const table
	#endif
		const float4 masks[19] = {
				{1,0,0,0}, {0,1,0,0}, {0,0,1,0}, {0,0,0,1},
				{1,0,0,0}, {0,1,0,0}, {0,0,1,0}, {0,0,0,1},
				{1,0,0,0}, {0,1,0,0}, {0,0,1,0}, {0,0,0,1},
				{1,0,0,0}, {0,1,0,0}, {0,0,1,0}, {0,0,0,1},
				{1,0,0,0}, {0,1,0,0}, {0,0,1,0}, 
		};
	#if !__MAX
	}
	#endif
#endif

//
//
//
//
float4 CalculateDimmer(float4 inCpv)
{
#if DIMMER_19
	const uint idx = 19; // sirens: special case to hardcode last available index in dimmerSetPacked[]
	const float dimmerStrength = dot(dimmerSetPacked[idx/4],float4(0,0,0,1));
	return(float4(dimmerStrength * inCpv.rrr, dimmerStrength));
#elif DIMMER
	#if __XENON || RSG_PC || RSG_DURANGO || RSG_ORBIS
		// Xenon declares it locally, to save on temp registers.
		const float4 masks[19] = {
				{1,0,0,0}, {0,1,0,0}, {0,0,1,0}, {0,0,0,1},
				{1,0,0,0}, {0,1,0,0}, {0,0,1,0}, {0,0,0,1},
				{1,0,0,0}, {0,1,0,0}, {0,0,1,0}, {0,0,0,1},
				{1,0,0,0}, {0,1,0,0}, {0,0,1,0}, {0,0,0,1},
				{1,0,0,0}, {0,1,0,0}, {0,0,1,0}, };
	#endif // platforms
	uint idx = (uint)(inCpv.a * 255.0f + 0.5f);
	const float dimmerStrength = dot(dimmerSetPacked[idx/4],masks[idx]);
	return(float4(dimmerStrength * inCpv.rrr, dimmerStrength));
#else
	return(inCpv.rgba);
#endif //DIMMER...
}


#if VEHICLE_ANIMATED_TRACK_UV
//
// helper function for animated UVs:
//
float2 VehicleAnimateUVs(float2 uv, float3 inPos)
{
float2 OUT;

	// which track is it: left or right?
	float side = (inPos.x>0)? 1.0f : 0.0f;	// 1=right, 0=left

//  full animation:
//	float3 rightTrackAnim0	= dot(globalAnimUV0, float3(uv.x, uv.y, 1.0f));
//	float3 rightTrackAnim1	= dot(globalAnimUV1, float3(uv.x, uv.y, 1.0f));
//	float3 leftTrackAnim0	= dot(globalAnimUV0, float3(uv.x, uv.y, 1.0f));
//	float3 leftTrackAnim1	= dot(globalAnimUV1, float3(uv.x, uv.y, 1.0f));
//	OUT.x = lerp(leftTrackAnim0, rightTrackAnim0, side);
//	OUT.y = lerp(leftTrackAnim1, rightTrackAnim1, side);

	// U shift only:
	float rightTrackAnim	= uv.x + TrackAnimUV.x;
	float leftTrackAnim		= uv.x + TrackAnimUV.y;
	OUT.x = lerp(leftTrackAnim, rightTrackAnim, side);
	OUT.y = uv.y;

	return(OUT);
}
#elif VEHICLE_ANIMATED_TRACK2_UV
float2 VehicleAnimateUVs(float2 uv, float3 inPos)
{
float2 OUT;
	// which track is it: left or right?
	float side = (inPos.x>0)? 1.0f : 0.0f;	// 1=right, 0=left
	// U shift only:
	float rightTrackAnim	= uv.x + Track2AnimUV.x;
	float leftTrackAnim		= uv.x + Track2AnimUV.y;
	OUT.x = lerp(leftTrackAnim, rightTrackAnim, side);
	OUT.y = uv.y;
	return(OUT);
}
#elif VEHICLE_ANIMATED_TRACKAMMO_UV
float2 VehicleAnimateUVs(float2 uv, float3 inPos)
{
	float2 OUT;
	// which track is it: left or right?
	float side = (inPos.x>0)? 1.0f : 0.0f;	// 1=right, 0=left
	// U shift only:
	float rightTrackAnim	= uv.x + TrackAmmoAnimUV.x;
	float leftTrackAnim		= uv.x + TrackAmmoAnimUV.y;
	OUT.x = lerp(leftTrackAnim, rightTrackAnim, side);
	OUT.y = uv.y;
	return(OUT);
}
#else
float2 VehicleAnimateUVs(float2 uv, float3 inPos)
{
	return(uv);
}
#endif //VEHICLE_ANIMATED_TRACK_UV...

#if __SHADERMODEL >= 40
shared Texture2D<float>	WaterDepthTexture	REGISTER(t39);
shared Texture2D<float>	WaterParamsTexture	REGISTER(t40);

CBSHARED BeginConstantBufferPagedDX10(vehiclewater_globals, b7)
shared float2 gVehicleWaterProjParams;
EndConstantBufferDX10(vehiclewater_globals)
#endif //__SHADERMODEL > 40

#if VEHICLESHADER_SNOW

//
//
// make it last sampler as Rick's Max scripts depend on shader ordering:
//
#ifndef UINAME_SAMPLER_ENVEFFTEX0
	#define UINAME_SAMPLER_ENVEFFTEX0 "EnvEff Texture0"
#endif
BeginSampler(sampler2D,snowTexture0,SnowSampler0,SnowTex0)
	string UIName=UINAME_SAMPLER_ENVEFFTEX0;
	string UIHint="EnvironmentEffect map0";
	int nostrip=1;	// dont strip
	string TCPTemplate="Diffuse";
	string	TextureType="Dirt";
ContinueSampler(sampler2D,snowTexture0,SnowSampler0,SnowTex0)
	AddressU  = WRAP;
	AddressV  = WRAP;
	AddressW  = WRAP;
	MIPFILTER = MIPLINEAR;
	MINFILTER = HQLINEAR;
	MAGFILTER = MAGLINEAR;
EndSampler;

#ifndef UINAME_SAMPLER_ENVEFFTEX1
	#define UINAME_SAMPLER_ENVEFFTEX1 "EnvEff Texture1"
#endif
BeginSampler(sampler2D,snowTexture1,SnowSampler1,SnowTex1)
	string UIName=UINAME_SAMPLER_ENVEFFTEX1;
	string UIHint="EnvironmentEffect map1";
	int nostrip=1;	// dont strip
	string TCPTemplate="Diffuse";
	string	TextureType="Dirt";
ContinueSampler(sampler2D,snowTexture1,SnowSampler1,SnowTex1)
	AddressU  = WRAP;
	AddressV  = WRAP;
	AddressW  = WRAP;
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
	//snowBlend.x = snowAmount				* 2.0f;	// * GtaStep(0.5f, snowAmount);
	snowBlend.x = snowAmount;

	// blending <0;1> for 128-255, 0 otherwise:
	//snowBlend.y = saturate(snowAmount-0.5f)	* 2.0f;	
	snowBlend.y = snowAmount;

	return snowBlend;
}

//
//
//
//
float3 VS_ApplySnowTransform(float3 inPos, float3 inWorldNormal, float2 snowBlend)
{
float3 outPos = inPos;
	// no enveff growing vertex for vehicles:
	// outPos += inWorldNormal * snowStep * snowBlend.y * snowScaleV;
	return(outPos);
}

//
//
//
//
float VS_CalculateSnowPsParams(float2 snowBlend)
{
	// store snow "shading":
	//float outSnowParams	= saturate(snowBlend.x + snowBlend.y) * snowScale;
	float outSnowParams		= snowBlend.x * snowScale;

	return outSnowParams;
}

//
//
//
//
float4 PS_ApplySnow(float4 inSurfaceDiffuse, float snowParams, float2 diffuseUV)
{
float4 outDiffuse = inSurfaceDiffuse;

	float snowAmount = snowParams;

	float2 snowUV = diffuseUV.xy*envEffTexTileUV;					// 8x tiling for snow texture
	float4 snowTex0 = tex2D(SnowSampler0, snowUV);
	float4 snowTex1 = tex2D(SnowSampler1, snowUV);

	//float snowAmount0 = saturate( snowAmount*4.0f );			// blend <0.0 ,0.25> for tex0
	//float snowAmount1 = saturate((snowAmount-0.25f)*4.0f);	// blend <0.25,0.50> for tex1
	float snowAmount0 = saturate( snowAmount*2.0f );			// blend <0.0 ,0.5> for tex0
	float snowAmount1 = saturate((snowAmount-0.5f)*2.0f);		// blend <0.5 ,1.0> for tex1

	// base -> snowTex0:
	float4 snowColor0 = lerp(inSurfaceDiffuse,	snowTex0,		snowAmount0*snowTex0.a);
	// snowTex0 -> snowTex1:
	float4 snowColor1 = lerp(snowColor0,		snowTex1,		snowAmount1);

	// snow "shading":
	outDiffuse.rgb = snowColor1.rgb;

	return outDiffuse;
}
#endif //VEHICLESHADER_SNOW...

//
//
// decodes input CPV into base color used by vehicles:
//
float4 PS_GetBaseColorFromCPV(float4 IN_color0)
{
#if DIMMER
	return float4(IN_color0.rrr, 1.0f);
#else
	return float4(IN_color0.rrra);
#endif
}


// DISABLE FEATURES BASED ON HARDWARE LIMITATIONS
#if __WIN32PC 
	#if __SHADERMODEL < 20
		// Really old shaders lose lots of stuff
		#undef	NORMAL_MAP
		#define	NORMAL_MAP			0
		#undef	NORMAL_MAP2
		#define	NORMAL_MAP2			0
		#undef	REFLECT
		#define	REFLECT				0
		#undef	SPECULAR
		#define	SPECULAR			0
		#undef	DIFFUSE2
		#define	DIFFUSE2			0
		#undef  DIFFUSE2_UV0
		#define DIFFUSE2_UV0		0
		#undef  DIFFUSE2_UV1
		#define DIFFUSE2_UV1		0
		#undef	DIFFUSE3
		#define	DIFFUSE3			0
		#undef  DIFFUSE3_UV0
		#define DIFFUSE3_UV0		0
		#undef  DIFFUSE3_UV1
		#define DIFFUSE3_UV1		0
		#undef	DIRT_UV
		#define DIRT_UV				0
		#undef	DIFFUSE_TEX_COORDS
		#define	DIFFUSE_TEX_COORDS	float2
	#endif	// __SHADERMODEL < 20

	#if __SHADERMODEL < 30
		// limit stuff for PS/VS 2.0 (only 64 arithmetic instructions allowed):
		#undef	REFLECT
		#define	REFLECT 0
		#undef	DIRT_UV
		#define	DIRT_UV	0
	#endif	// __SHADERMODEL < 30
#endif

#define SHADOW_CASTING            (1)
#define SHADOW_CASTING_TECHNIQUES (1 && defined (SHADOW_CASTING_TECHNIQUES_OK))
#define SHADOW_RECEIVING          (1)
#define SHADOW_RECEIVING_VS       (0)

#ifdef USE_DETAIL_MAP_UV_LAYER
	#if !defined(USE_DETAIL_MAP)
		#error "Using USE_DETAIL_MAP_UV_LAYER without USE_DETAIL_MAP!"
	#endif

	#define DETAIL_UV							(1)
	// note: using the same input/output as DIFFUSE2
	#undef DIFFUSE_TEX_COORDS	
	#define DIFFUSE_TEX_COORDS					float4
	#define DETAIL_VS_INPUT						IN.texCoord2.xy
	#define DETAIL_VS_OUTPUT					OUT.texCoord.zw
	#define DETAIL_PS_INPUT						IN.texCoord.zw
	#define UINAME_SAMPLER_DETAILTEXTURE		"2: Detail Map"
#else
	#define DETAIL_UV							(0)
#endif

#ifdef USE_DETAIL_MAP
	#include "..\megashader\detail_map.fxh"
	#define DETAIL_MAP							(1)
#else
	#define DETAIL_MAP							(0)
#endif

#if (__SHADERMODEL >= 40)
// DX11 TODO:- These are temporary here for the RAG sliders.	 We`ll need to put in proper depth based calculations.
#define gTessellationFactor gTessellationGlobal1.y
#define gPN_KFactor gTessellationGlobal1.z
#endif

#include "../Lighting/Shadows/localshadowglobals.fxh"
#include "../Lighting/Shadows/cascadeshadows.fxh"
#include "../Lighting/lighting.fxh"
#include "../Lighting/forward_lighting.fxh"
#include "../Debug/EntitySelect.fxh"

#if defined(USE_VEHICLE_DAMAGE_GLASS) && (DIFFUSE2 || DIFFUSE3)
	#error "Not expected to have DIFFUSE2/3 set along with USE_VEHICLE_DAMAGE_GLASS
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
//
//
//
//
struct vehicleSkinVertexInput
{	// This covers the default rage skinned vertex
	float3 pos				: POSITION0;
	float4 weight			: BLENDWEIGHT;
	index4 blendindices		: BLENDINDICES;
	float2 texCoord0		: TEXCOORD0;
#if DIFFUSE_UV1 || DIFFUSE2_UV1 || DIFFUSE3_UV1 || VEH_SNOW_UV1
	float2 texCoord2		: TEXCOORD1;
	#if DIRT_UV || DIFFUSE_UV2
		float2	texCoord3	: TEXCOORD2;
	#endif // DIRT_UV
#else // DIFFUSE_UV1 || DIFFUSE2_UV1 || DIFFUSE3_UV1 || VEH_SNOW_UV1
	#if DIRT_UV || DETAIL_UV
		float2 texCoord2	: TEXCOORD1;
	#endif // DIRT_UV
	#ifdef USE_VEHICLE_DAMAGE_GLASS
		float2 damageValue	: TEXCOORD2;
	#endif
#endif // DIFFUSE_UV1 || DIFFUSE2_UV1 || DIFFUSE3_UV1 || VEH_SNOW_UV1
	float3 normal			: NORMAL;
	float4 diffuse			: COLOR0;
};

struct vehicleSkinVertexInputBump
{	// This covers the default rage skinned vertex
	float3 pos				: POSITION0;
	float4 weight			: BLENDWEIGHT;
	index4 blendindices		: BLENDINDICES;
	float2 texCoord0		: TEXCOORD0;
#if DIFFUSE_UV1 || DIFFUSE2_UV1 || DIFFUSE3_UV1 || VEH_SNOW_UV1
	float2 texCoord2		: TEXCOORD1;
	#if DIRT_UV || DIFFUSE_UV2
		float2	texCoord3	: TEXCOORD2;
	#endif // DIRT_UV
#else // DIFFUSE_UV1 || DIFFUSE2_UV1 || DIFFUSE3_UV1 || VEH_SNOW_UV1
	#if DIRT_UV || DETAIL_UV
		float2 texCoord2	: TEXCOORD1;
	#endif // DIRT_UV
	#ifdef USE_VEHICLE_DAMAGE_GLASS
		float2 damageValue	: TEXCOORD2;
	#endif
#endif // DIFFUSE_UV1 || DIFFUSE2_UV1 || DIFFUSE3_UV1 || VEH_SNOW_UV1
	float3 normal			: NORMAL0;
#if NORMAL_MAP || NORMAL_MAP2
	float4 tangent			: TANGENT0;
#endif
	float4 diffuse			: COLOR0;
#if UMOVEMENTS
	float4 specular			: COLOR1;
#endif
};



// common rage structs:
struct vehicleVertexInput
{	// This covers the default rage vertex format (non-skinned)
	float3 pos				: POSITION;
	float4 diffuse			: COLOR0;
	float2 texCoord0		: TEXCOORD0;
#if DIFFUSE_UV1 || DIFFUSE2_UV1 || DIFFUSE3_UV1 || VEH_SNOW_UV1
	float2 texCoord2		: TEXCOORD1;
	#if DIRT_UV || DIFFUSE_UV2
		float2	texCoord3	: TEXCOORD2;
	#endif // DIRT_UV
#else // DIFFUSE_UV1 || DIFFUSE2_UV1 || DIFFUSE3_UV1 || VEH_SNOW_UV1
	#if DIRT_UV || DETAIL_UV
		float2 texCoord2	: TEXCOORD1;
	#endif // DIRT_UV
	#ifdef USE_VEHICLE_DAMAGE_GLASS
		float2 damageValue	: TEXCOORD2;
	#endif
#endif // DIFFUSE_UV1 || DIFFUSE2_UV1 || DIFFUSE3_UV1 || VEH_SNOW_UV1
	float3 normal			: NORMAL;
};


// =============================================================================================== //
// vehicleVertexInputBump
// =============================================================================================== //

struct vehicleVertexInputBump
{	// This covers the default rage vertex format (non-skinned)
	float3 pos				: POSITION;
	float4 diffuse			: COLOR0;
#if UMOVEMENTS
	float4 specular			: COLOR1;
#endif
	float2 texCoord0		: TEXCOORD0;
#if DIFFUSE_UV1 || DIFFUSE2_UV1 || DIFFUSE3_UV1 || VEH_SNOW_UV1
	float2 texCoord2		: TEXCOORD1;
	#if DIRT_UV || DIFFUSE_UV2
		float2 texCoord3	: TEXCOORD2;
	#endif // DIRT_UV
#else // DIFFUSE_UV1 || DIFFUSE2_UV1 || DIFFUSE3_UV1 || VEH_SNOW_UV1
	#if DIRT_UV || DETAIL_UV
		float2 texCoord2	: TEXCOORD1;
	#endif // DIRT_UV
	#ifdef USE_VEHICLE_DAMAGE_GLASS
		float2 damageValue  : TEXCOORD2;
	#endif
#endif // DIFFUSE_UV1 || DIFFUSE2_UV1 || DIFFUSE3_UV1 || VEH_SNOW_UV1
	float3 normal			: NORMAL;
	float4 tangent			: TANGENT0;
};

#if __SHADERMODEL >= 40

#define AXEL_VECTOR float3(1.0f, 0.0f, 0.0f)

float SplitIntoRadialAndAxial(float3 In, out float3 Radial)
{
	float Ret = dot(In, AXEL_VECTOR);
	Radial = In - Ret*AXEL_VECTOR;
	return Ret;
}

struct vehicleVertexInputBump_CtrlPoint
{	// This covers the default rage vertex format (non-skinned)
	float3 pos				: CTRL_POSITION;
	float4 diffuse			: CTRL_COLOR0;
#if UMOVEMENTS
	float4 specular			: CTRL_COLOR1;
#endif
	float2 texCoord			: CTRL_TEXCOORD0;
#if DIFFUSE_UV1 || DIFFUSE2_UV1 || DIFFUSE3_UV1 || VEH_SNOW_UV1
	float2 texCoord2		: CTRL_TEXCOORD1;
	#if DIRT_UV || DIFFUSE_UV2
		float2	texCoord3	: CTRL_TEXCOORD2;
	#endif // DIRT_UV
#else // DIFFUSE_UV1 || DIFFUSE2_UV1 || DIFFUSE3_UV1 || VEH_SNOW_UV1
	#if DIRT_UV || DETAIL_UV
		float2 texCoord2	: CTRL_TEXCOORD1;
	#endif // DIRT_UV
	#ifdef USE_VEHICLE_DAMAGE_GLASS
		float2 damageValue  : CTRL_TEXCOORD2;
	#endif
#endif // DIFFUSE_UV1 || DIFFUSE2_UV1 || DIFFUSE3_UV1 || VEH_SNOW_UV1
	float3 normal			: CTRL_NORMAL;
	float4 tangent			: CTRL_TANGENT0;
#if defined(DO_WHEEL_ROUNDING_TESSELLATION)
	float Radius			: CTRL_RADIUS;
#endif //defined(DO_WHEEL_ROUNDING_TESSELLATION)
};


// Returns a control point from an IN.
vehicleVertexInputBump_CtrlPoint CtrlPointFromIN(vehicleVertexInputBump IN)
{
	vehicleVertexInputBump_CtrlPoint OUT;
	
	OUT.pos = IN.pos;
	OUT.diffuse = IN.diffuse;
#if UMOVEMENTS
	OUT.specular = IN.specular;
#endif
	OUT.texCoord = IN.texCoord0;
#if DIFFUSE_UV1 || DIFFUSE2_UV1 || DIFFUSE3_UV1 || VEH_SNOW_UV1
	OUT.texCoord2 = IN.texCoord2;
	#if DIRT_UV || DIFFUSE_UV2
		OUT.texCoord3 = IN.texCoord3;
	#endif // DIRT_UV
#else // DIFFUSE_UV1 || DIFFUSE2_UV1 || DIFFUSE3_UV1 || VEH_SNOW_UV1
	#if DIRT_UV || DETAIL_UV
		OUT.texCoord2 = IN.texCoord2;
	#endif // DIRT_UV
	#ifdef USE_VEHICLE_DAMAGE_GLASS
		OUT.damageValue = IN.damageValue;
	#endif
#endif // DIFFUSE_UV1 || DIFFUSE2_UV1 || DIFFUSE3_UV1 || VEH_SNOW_UV1
	OUT.normal = IN.normal;
	OUT.tangent = IN.tangent;
	
#if defined(DO_WHEEL_ROUNDING_TESSELLATION)
	float3 Radial;
	float Axial = SplitIntoRadialAndAxial(IN.pos, Radial);
	OUT.Radius = length(Radial);
#endif // defined(DO_WHEEL_ROUNDING_TESSELLATION)

	return OUT;
}

#endif //__SHADERMODEL >= 40

// =============================================================================================== //

struct vehicleVertexOutput
{
	DIFFUSE_TEX_COORDS texCoord		: TEXCOORD0;
#if defined(USE_VEHICLE_DAMAGE) || defined(USE_VEHICLE_DAMAGE_EDGE)
	float4 worldNormal			: TEXCOORD1;	// xyz=normal, w=specDmgScale
#else
	float3 worldNormal			: TEXCOORD1;
#endif
	float4 worldPos					: TEXCOORD2; // stores fog opacity in w if USE_VERTEX_FOG_ALPHA is defined
#if REFLECT
	float3 worldEyePos				: TEXCOORD3;
#endif
#if NORMAL_MAP || NORMAL_MAP2
	float3 worldTangent				: TEXCOORD4;
	float3 worldBinormal			: TEXCOORD5;
#endif // NORMAL_MAP
	float4 color0					: TEXCOORD6;
#if DIRT_UV
	#if VEHICLESHADER_SNOW
		#define SNOW_VS_OUTPUTD	OUT.texCoord3.z
		#define SNOW_PS_INPUTD	IN.texCoord3.z
		float3 texCoord3			: TEXCOORD7;	// texCoord3+snowParams
	#else
		float2 texCoord3			: TEXCOORD7;
	#endif
#else
	#if VEHICLESHADER_SNOW
		#define SNOW_VS_OUTPUTD	OUT.snowParams
		#define SNOW_PS_INPUTD	IN.snowParams
		float snowParams			: TEXCOORD7;
	#endif
#endif
#if defined(USE_VERTEX_FOG)
	float4 fogData					: TEXCOORD8;
#endif
	DECLARE_POSITION_CLIPPLANES(pos)
};

#if APPLY_DOF_TO_ALPHA
struct vehicleVertexOutputDOF
{
	vehicleVertexOutput vertOutput;
	float linearDepth				: TEXCOORD10;
};
#endif //APPLY_DOF_TO_ALPHA

struct vehicleVertexOutputWaterReflection
{
	DIFFUSE_TEX_COORDS texCoord		: TEXCOORD0;
	float3 worldNormal				: TEXCOORD1;
	float3 worldPos					: TEXCOORD2;
	float4 color0					: TEXCOORD3;
#if DIRT_UV
	float2 texCoord3				: TEXCOORD4;
#endif
#if VEHICLE_SUPPORT_PAINT_RAMP && MATERIAL_DIFFUSE_COLOR
	float3 worldEyePos				: TEXCOORD5;
#endif
	DECLARE_POSITION_CLIPPLANES(pos)
};

//don't forget "vehicleSampleInputD" if you change this structure
struct vehicleVertexOutputD
{
	DECLARE_POSITION(pos)
	DIFFUSE_TEX_COORDS texCoord		: TEXCOORD0;
#if defined(USE_VEHICLE_DAMAGE) || defined(USE_VEHICLE_DAMAGE_EDGE)
	float4 worldNormal				: TEXCOORD1;	// xyz=normal, w=specDmgScale
#else
	float3 worldNormal				: TEXCOORD1;
#endif
	float4 color0					: TEXCOORD2;
#if REFLECT || SECOND_SPECULAR_LAYER
	float3 worldEyePos				: TEXCOORD3;
#endif	

#if NORMAL_MAP || NORMAL_MAP2
	float3 worldTangent				: TEXCOORD4;
	float3 worldBinormal			: TEXCOORD5;
#endif // NORMAL_MAP

#if DIRT_UV
	#if VEHICLESHADER_SNOW
		#define SNOW_VS_OUTPUTD	OUT.texCoord3.z
		#define SNOW_PS_INPUTD	IN.texCoord3.z
		float3 texCoord3			: TEXCOORD6;	// texCoord3+snowParams
	#else
		float2 texCoord3			: TEXCOORD6;
	#endif
#else
	#if VEHICLESHADER_SNOW
		#define SNOW_VS_OUTPUTD	OUT.snowParams
		#define SNOW_PS_INPUTD	IN.snowParams
		float snowParams			: TEXCOORD6;
	#endif
#endif
#ifdef VEHICLE_TYRE_DEBUG_MEASUREMENT
	float3 tyreParams				: TEXCOORD7;
#endif
};

#if VEHICLE_SUPERSAMPLE
	#define VEH_INPUT	inside_sample
#else
	#define VEH_INPUT
#endif

struct vehicleSampleInputD
{
	VEH_INPUT DECLARE_POSITION_PSIN(pos)
	VEH_INPUT DIFFUSE_TEX_COORDS texCoord	: TEXCOORD0;
#if defined(USE_VEHICLE_DAMAGE) || defined(USE_VEHICLE_DAMAGE_EDGE)
	VEH_INPUT float4 worldNormal			: TEXCOORD1;	// xyz=normal, w=specDmgScale
#else
	VEH_INPUT float3 worldNormal			: TEXCOORD1;
#endif
	VEH_INPUT float4 color0					: TEXCOORD2;
#if REFLECT || SECOND_SPECULAR_LAYER
	VEH_INPUT float3 worldEyePos			: TEXCOORD3;
#endif	

#if NORMAL_MAP || NORMAL_MAP2
	VEH_INPUT float3 worldTangent			: TEXCOORD4;
	VEH_INPUT float3 worldBinormal			: TEXCOORD5;
#endif // NORMAL_MAP

#if DIRT_UV
	#if VEHICLESHADER_SNOW
		#define SNOW_VS_OUTPUTD	OUT.texCoord3.z
		#define SNOW_PS_INPUTD	IN.texCoord3.z
		VEH_INPUT float3 texCoord3			: TEXCOORD6;	// texCoord3+snowParams
	#else
		VEH_INPUT float2 texCoord3			: TEXCOORD6;
	#endif
#else
	#if VEHICLESHADER_SNOW
		#define SNOW_VS_OUTPUTD	OUT.snowParams
		#define SNOW_PS_INPUTD	IN.snowParams
		VEH_INPUT float snowParams			: TEXCOORD6;
	#endif
#endif
#ifdef VEHICLE_TYRE_DEBUG_MEASUREMENT
	VEH_INPUT float3 tyreParams				: TEXCOORD7;
#endif
#if VEHICLE_SUPERSAMPLE
	uint	sampleIndex						: SV_SampleIndex;
#endif
};



struct vehicleVertexOutputUnlit
{
	DECLARE_POSITION(pos)
	DIFFUSE_TEX_COORDS texCoord		: TEXCOORD0;
	float4 color0					: TEXCOORD1;
#if DIRT_UV
	float2 texCoord3				: TEXCOORD2;
#endif
};





/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//
//
//
//
//
//
void vehicleProcessVertLightingUnskinned(
					float3		localPos,
					float3		localNormal,
	#if NORMAL_MAP || NORMAL_MAP2
					float4		localTangent,
	#endif // NORMAL_MAP || NORMAL_MAP2
					float4		vertColor,
					float4x3	worldMtx,
					out float3	worldPosition,
					out float3	worldEyePos,
					out float3	worldNormal,
	#if NORMAL_MAP || NORMAL_MAP2
					out float3	worldBinormal,
					out float3	worldTangent,
	#endif	// NORMAL_MAP || NORMAL_MAP2
					out float4	color0 )
{
	float3 pos		= mul(float4(localPos,1), worldMtx);
	worldPosition	= pos;

	// We need to add the world offset to the world pos
	// To ensure proper world to eye position.
	worldEyePos		= gViewInverse[3].xyz - pos;

	// Transform normal by "transpose" matrix
	worldNormal		= NORMALIZE_VS(mul(localNormal, (float3x3)worldMtx));

#if NORMAL_MAP || NORMAL_MAP2
	// Do the tangent+binormal stuff
	worldTangent	= NORMALIZE_VS(mul(localTangent.xyz, (float3x3)worldMtx));
	worldBinormal	= rageComputeBinormal(worldNormal, worldTangent,localTangent.w);
#endif // NORMAL_MAP || NORMAL_MAP2

	color0 = vertColor;
}

void vehicleProcessVertLightingSkinned(
					float3		localPos,
					float3		localNormal,
	#if NORMAL_MAP || NORMAL_MAP2
					float4		localTangent,
	#endif // NORMAL_MAP || NORMAL_MAP2
					float4		vertColor,
					rageSkinMtx skinMtx,
					float4x3	worldMtx,
					out float3	worldPosition,
					out float3	worldEyePos,
					out float3	skinnedNormal,
					out float3	worldNormal,
	#if NORMAL_MAP  || NORMAL_MAP2
					out float3	worldBinormal,
					out float3	worldTangent,
	#endif	// NORMAL_MAP || NORMAL_MAP2
					out float4	color0 )
{
	float3 skinnedPos		= rageSkinTransform(localPos, skinMtx);
	float3 pos				= mul(float4(skinnedPos,1), worldMtx);
	worldPosition			= pos;

	// We need to add the world offset to the world pos
	// To ensure proper world to eye position.
	worldEyePos				= gViewInverse[3].xyz - pos;

	// Transform normal by "transpose" matrix
	skinnedNormal			= rageSkinRotate(localNormal, skinMtx); 
	worldNormal				= NORMALIZE_VS(mul(skinnedNormal, (float3x3)worldMtx)); 

#if NORMAL_MAP || NORMAL_MAP2
	// Do the tangent+binormal stuff
	float3 skinnedTangent	= rageSkinRotate(localTangent.xyz, skinMtx); 
	worldTangent			= NORMALIZE_VS(mul(skinnedTangent, (float3x3)worldMtx)); 
	worldBinormal			= rageComputeBinormal(worldNormal, worldTangent,localTangent.w);
#endif // NORMAL_MAP || NORMAL_MAP2

	color0 = vertColor;
}


#if RAGE_ENABLE_OFFSET
	#define RAGE_ENABLE_OFFSET_FUNC_ARGS(v) , v INOFFSET, uniform float bEnableOffsets
#else
	#define RAGE_ENABLE_OFFSET_FUNC_ARGS(v)
#endif

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//
//
//

// =============================================================================================== //
// vehicleVertexInputBump wheel rounding tessellation functions.
// =============================================================================================== //

#if defined(DO_WHEEL_ROUNDING_TESSELLATION)

#define gWheelTessellationFactor gTessellationGlobal1.x

struct patchConstantInfo
{
	float Edges[3] : SV_TessFactor;
	float Inside[1] : SV_InsideTessFactor;
};

// Vertex shader which outputs a control point.
vehicleVertexInputBump_CtrlPoint VS_WR_vehicleVertexInputBump(vehicleVertexInputBump IN RAGE_ENABLE_OFFSET_FUNC_ARGS(rageVertexOffsetBump))
{
	// Output a control point. 
	vehicleVertexInputBump_CtrlPoint OUT = CtrlPointFromIN(IN);
	return OUT;
}


// Patch Constant Function.
patchConstantInfo HS_WR_vehicleVertexInputBump_PatchFunc( InputPatch<vehicleVertexInputBump_CtrlPoint, 3> PatchPoints,  uint PatchID : SV_PrimitiveID )
{
	patchConstantInfo Output;

	if(tyreDeformSwitchOn)
	{
		float Deform = ShouldDeform(PatchPoints[0].pos) + ShouldDeform(PatchPoints[1].pos) + ShouldDeform(PatchPoints[2].pos);
		
		if(Deform > 0.0f)
		{
			Output.Edges[0] = 1.0f;
			Output.Edges[1] = 1.0f;
			Output.Edges[2] = 1.0f;
			Output.Inside[0] = 1.0f;
		}
		else
		{
			// TODO:- Calculate from depth in scene.
			Output.Edges[0] = gWheelTessellationFactor;
			Output.Edges[1] = gWheelTessellationFactor;
			Output.Edges[2] = gWheelTessellationFactor;
			Output.Inside[0] = gWheelTessellationFactor;
		}
	}
	else
	{
		// TODO:- Calculate from depth in scene.
		Output.Edges[0] = gWheelTessellationFactor;
		Output.Edges[1] = gWheelTessellationFactor;
		Output.Edges[2] = gWheelTessellationFactor;
		Output.Inside[0] = gWheelTessellationFactor;
	}
	return Output;
}

[domain("tri")]
[partitioning("integer")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(3)]
[patchconstantfunc("HS_WR_vehicleVertexInputBump_PatchFunc")]
[maxtessfactor(15)]
vehicleVertexInputBump_CtrlPoint HS_WR_vehicleVertexInputBump(InputPatch<vehicleVertexInputBump_CtrlPoint, 3> PatchPoints, uint i : SV_OutputControlPointID, uint PatchID : SV_PrimitiveID )
{
	vehicleVertexInputBump_CtrlPoint Output;

	// Our control points match the vertex shader output.
	Output = PatchPoints[i];
#if NO_HULL_PASSTHROUGH
	Output.diffuse += 0.0001f;
#endif

	return Output;
}


// Patch Constant Function.
patchConstantInfo HS_WRND_vehicleVertexInputBump_PatchFunc( InputPatch<vehicleVertexInputBump_CtrlPoint, 3> PatchPoints,  uint PatchID : SV_PrimitiveID )
{
	patchConstantInfo Output;

	// TODO:- Calculate from depth in scene.
	Output.Edges[0] = gWheelTessellationFactor;
	Output.Edges[1] = gWheelTessellationFactor;
	Output.Edges[2] = gWheelTessellationFactor;
	Output.Inside[0] = gWheelTessellationFactor;
	return Output;
}

[domain("tri")]
[partitioning("integer")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(3)]
[patchconstantfunc("HS_WRND_vehicleVertexInputBump_PatchFunc")]
[maxtessfactor(15)]
vehicleVertexInputBump_CtrlPoint HS_WRND_vehicleVertexInputBump(InputPatch<vehicleVertexInputBump_CtrlPoint, 3> PatchPoints, uint i : SV_OutputControlPointID, uint PatchID : SV_PrimitiveID )
{
	vehicleVertexInputBump_CtrlPoint Output;

	// Our control points match the vertex shader output.
	Output = PatchPoints[i];
#if NO_HULL_PASSTHROUGH
	Output.diffuse += 0.0001f;
#endif

	return Output;
}


// Domain function which performs the offsetting of verts radially from the "axel" vector.
vehicleVertexInputBump DS_WR_vehicleVertexInputBump(patchConstantInfo PatchInfo, float3 UVW, const OutputPatch<vehicleVertexInputBump_CtrlPoint, 3> PatchPoints)
{
	vehicleVertexInputBump Arg;
	
	float3 Radial;
	float R = RAGE_COMPUTE_BARYCENTRIC(UVW, PatchPoints, Radius);
	float3 P = RAGE_COMPUTE_BARYCENTRIC(UVW, PatchPoints, pos);
	float Axial = SplitIntoRadialAndAxial(P, Radial);
	float CurrentR = length(Radial);
	
	float k = 1.0f;
	
	if(CurrentR > 0.01f)
	{
		k = (R/CurrentR);
	}
	
	Arg.pos = k*Radial + Axial*AXEL_VECTOR;
	Arg.diffuse = RAGE_COMPUTE_BARYCENTRIC(UVW, PatchPoints, diffuse); 
	
#if UMOVEMENTS
	Arg.specular = RAGE_COMPUTE_BARYCENTRIC(UVW, PatchPoints, specular);
#endif
#if DIFFUSE_UV1 || DIFFUSE2_UV1 || DIFFUSE3_UV1 || VEH_SNOW_UV1
	Arg.texCoord = RAGE_COMPUTE_BARYCENTRIC(UVW, PatchPoints, texCoord);
	#if DIRT_UV || DIFFUSE_UV2
		Arg.texCoord3 = RAGE_COMPUTE_BARYCENTRIC(UVW, PatchPoints, texCoord3);
	#endif // DIRT_UV
#else // DIFFUSE_UV1 || DIFFUSE2_UV1 || DIFFUSE3_UV1 || VEH_SNOW_UV1
	#if DIRT_UV
		Arg.texCoord = RAGE_COMPUTE_BARYCENTRIC(UVW, PatchPoints, texCoord);
	#else	
		Arg.texCoord = RAGE_COMPUTE_BARYCENTRIC(UVW, PatchPoints, texCoord);
	#endif // DIRT_UV
#endif // DIFFUSE_UV1 || DIFFUSE2_UV1 || DIFFUSE3_UV1 || VEH_SNOW_UV1
	Arg.normal = RAGE_COMPUTE_BARYCENTRIC(UVW, PatchPoints, normal);
	Arg.normal = normalize(Arg.normal);
	Arg.tangent = RAGE_COMPUTE_BARYCENTRIC(UVW, PatchPoints, tangent);

	return Arg;
}

#endif // defined(DO_WHEEL_ROUNDING_TESSELLATION)

// =============================================================================================== //
// VS_VehicleTransform
// =============================================================================================== //

vehicleVertexOutput VS_VehicleTransform(vehicleVertexInputBump IN RAGE_ENABLE_OFFSET_FUNC_ARGS(rageVertexOffsetBump))
{
vehicleVertexOutput OUT;
	float3 inPos = IN.pos;
	float3 inNrm = IN.normal;
	float4 inCpv = IN.diffuse;

#if VEHICLESHADER_SNOW
	const float2 snowBlend = VS_CalculateSnowBlend( saturate(IN.diffuse.a) );
#endif
#if NORMAL_MAP || NORMAL_MAP2
	float4 inTan = IN.tangent;
#endif

#ifdef RAGE_ENABLE_OFFSET
	if( bEnableOffsets != 0.0f )
	{
		inPos += INOFFSET.pos;
		inNrm += INOFFSET.normal;
		inCpv += INOFFSET.diffuse;
#if NORMAL_MAP || NORMAL_MAP2
		inTan += INOFFSET.tangent;
#endif
	}
#endif // RAGE_ENABLE_OFFSET

#ifdef USE_VEHICLE_DAMAGE
	float3 inDmgPos = inPos;
	float3 inDmgNrm = inNrm;
	float specDmgScale = 1.0;
	specDmgScale = ApplyVehicleDamage(inDmgPos, inDmgNrm, VEHICLE_DAMAGE_SCALE, inPos, inNrm);
	#if RENDER_DEBUG_DAMAGECOLOR
		float damageLength = length(inDmgPos-inPos);
	#endif // RENDER_DEBUG_DAMAGECOLOR	
#elif defined(USE_VEHICLE_DAMAGE_EDGE)
	float specDmgScale = inCpv.g;	// already calculated by EDGE
#endif //USE_VEHICLE_DAMAGE...

	inCpv = CalculateDimmer(inCpv);

	float3 pvlWorldNormal, pvlWorldPosition, t_worldEyePos;

	vehicleProcessVertLightingUnskinned(	inPos,
											inNrm,
#if NORMAL_MAP || NORMAL_MAP2
											inTan,
#endif // NORMAL_MAP || NORMAL_MAP2
											inCpv,
#if VEHICLE_INSTANCED_WHEEL_SHADER
											(float4x3) matWheelWorld,
#else
											(float4x3) gWorld,
#endif//VEHICLE_INSTANCED_WHEEL_SHADER..
											pvlWorldPosition,
#if REFLECT
											OUT.worldEyePos,
#else
											t_worldEyePos,
#endif
											pvlWorldNormal,
#if NORMAL_MAP || NORMAL_MAP2
											OUT.worldBinormal,
											OUT.worldTangent,
#endif // NORMAL_MAP || NORMAL_MAP2
											OUT.color0
											);

#ifdef VEHICLE_TYRE_DEFORM
	float3 inputPos		= inPos;
	float3 inputWorldPos= pvlWorldPosition;
	float4 inputColor	= OUT.color0;
	ApplyTyreDeform(inputPos, inputWorldPos, inputColor, inPos, OUT.color0, true);

	#ifdef VEHICLE_TYRE_DEBUG_MEASUREMENT
		OUT.tyreParams.x = tyreDeformParams2.x;	//=VEHICLE_TYRE_INNER_RAD
		OUT.tyreParams.y = inPos.y;
		OUT.tyreParams.z = inPos.z;
	#endif //VEHICLE_TYRE_DEBUG_MEASUREMENT...
#endif //VEHICLE_TYRE_DEFORM...

#if RENDER_DEBUG_DAMAGECOLOR
	OUT.color0 = DebugGetDamageColor(damageLength,OUT.color0);
#endif // RENDER_DEBUG_DAMAGECOLOR

#if	defined(USE_VEHICLE_DAMAGE)
	if (bDebugDisplayDamageMap)
	{
	 	OUT.color0 = DamageSampleValue(inDmgPos);
	}
	if (bDebugDisplayDamageScale)
	{
	 	OUT.color0 = float4(VEHICLE_DAMAGE_SCALE, VEHICLE_DAMAGE_SCALE, VEHICLE_DAMAGE_SCALE, 1.0);
	}
#endif

	// Write out final position & texture coords
#if VEHICLE_INSTANCED_WHEEL_SHADER
	OUT.pos = MonoToStereoClipSpace(mul(float4(inPos,1), matWheelWorldViewProj));
#else
	OUT.pos = mul(float4(inPos,1), gWorldViewProj);
#endif

#if defined(USE_VEHICLE_DAMAGE) || defined(USE_VEHICLE_DAMAGE_EDGE)
	OUT.worldNormal		= float4(pvlWorldNormal, specDmgScale);
#else
	OUT.worldNormal		= pvlWorldNormal;
#endif

	// NOTE: These 3 statements may resolve to the exact same code -- rely on HLSL to strip worthless code
	DIFFUSE_VS_OUTPUT = DIFFUSE_VS_INPUT;
#if NORMAL_MAP
	BUMP_VS_OUTPUT = BUMP_VS_INPUT;
#endif
#if SPEC_MAP
	SPECULAR_VS_OUTPUT = SPECULAR_VS_INPUT;
#endif	// SPEC_MAP

#if DIFFUSE2
	// Pack in 2nd set of texture coords if needed
	DIFFUSE2_VS_OUTPUT = DIFFUSE2_VS_INPUT;
#endif // DIFFUSE2
#if NORMAL_MAP2
	BUMP2_VS_OUTPUT = BUMP2_VS_INPUT;
#endif
#if DIFFUSE3
	DIFFUSE3_VS_OUTPUT = DIFFUSE3_VS_INPUT;
#endif // DIFFUSE3
#if DETAIL_UV
	DETAIL_VS_OUTPUT = DETAIL_VS_INPUT;
#endif // DETAIL_UV...
#if DIRT_UV
	DIRT_VS_OUTPUT	= DIRT_VS_INPUT;
#endif //DIRT_UV...
#if VEHICLESHADER_SNOW
	SNOW_VS_OUTPUT  = SNOW_VS_INPUT;
	SNOW_VS_OUTPUTD = VS_CalculateSnowPsParams(snowBlend);
#endif

	OUT.worldPos=float4(pvlWorldPosition,1);

#if defined(USE_VERTEX_FOG)
	OUT.fogData = __CalcFogData(pvlWorldPosition - gViewInverse[3].xyz, 1.0f);
#elif defined(USE_VERTEX_FOG_ALPHA)
	OUT.worldPos.w = 1 - __CalcFogData(pvlWorldPosition - gViewInverse[3].xyz, 1.0f).w;
#endif // defined(USE_VERTEX_FOG_ALPHA)

	RAGE_COMPUTE_CLIP_DISTANCES(OUT.pos);

	return OUT;
}// end of VS_VehicleTransform()...

#if defined(DO_WHEEL_ROUNDING_TESSELLATION)

[domain("tri")]
vehicleVertexOutput DS_WR_VehicleTransform( patchConstantInfo PatchInfo, float3 UVW : SV_DomainLocation, const OutputPatch<vehicleVertexInputBump_CtrlPoint, 3> PatchPoints )
{
	// Apply the deformation/rounding.
	vehicleVertexInputBump Arg = DS_WR_vehicleVertexInputBump(PatchInfo, UVW, PatchPoints);

	// Transform as per the original vertex shader.
#ifdef RAGE_ENABLE_OFFSET
	return VS_VehicleTransform(Arg, INOFFSET, bEnableOffsets);
#else
	return VS_VehicleTransform(Arg);
#endif
}

#endif // defined(DO_WHEEL_ROUNDING_TESSELLATION)

// =============================================================================================== //
// VS_VehicleTransformWaterReflection
// =============================================================================================== //

vehicleVertexOutputWaterReflection VS_VehicleTransformWaterReflection(vehicleVertexInputBump IN)
{
	vehicleVertexOutputWaterReflection OUT;

	float3 inPos = IN.pos;
	float3 inNrm = IN.normal;
	float4 inCpv = IN.diffuse;

	// Write out final position & texture coords
#if VEHICLE_INSTANCED_WHEEL_SHADER
	OUT.pos = mul(float4(inPos,1), matWheelWorldViewProj);
	OUT.worldPos = mul(float4(inPos,1), matWheelWorld);
	OUT.worldNormal = mul(inNrm, (float3x3)matWheelWorld); // not normalised, that's ok
#else
	OUT.pos = mul(float4(inPos,1), gWorldViewProj);
	OUT.worldPos = mul(float4(inPos,1), gWorld);
	OUT.worldNormal = mul(inNrm, (float3x3)gWorld); // not normalised, that's ok
#endif
	OUT.color0 = inCpv;

#if VEHICLE_SUPPORT_PAINT_RAMP && MATERIAL_DIFFUSE_COLOR
	OUT.worldEyePos = gViewInverse[3].xyz - OUT.worldPos;
#endif

	// NOTE: These 2 statements may resolve to the exact same code -- rely on HLSL to strip worthless code
	DIFFUSE_VS_OUTPUT = DIFFUSE_VS_INPUT;
#if NORMAL_MAP
	BUMP_VS_OUTPUT = BUMP_VS_INPUT;
#endif
#if DIFFUSE2
	// Pack in 2nd set of texture coords if needed
	DIFFUSE2_VS_OUTPUT = DIFFUSE2_VS_INPUT;
#endif	// DIFFUSE2
#if NORMAL_MAP2
	BUMP2_VS_OUTPUT = BUMP2_VS_INPUT;
#endif
#if DIFFUSE3
	DIFFUSE3_VS_OUTPUT = DIFFUSE3_VS_INPUT;
#endif	// DIFFUSE3
#if DETAIL_UV
	DETAIL_VS_OUTPUT = DETAIL_VS_INPUT;
#endif // DETAIL_UV...
#if DIRT_UV
	DIRT_VS_OUTPUT	= DIRT_VS_INPUT;
#endif //DIRT_UV...
#if VEHICLESHADER_SNOW
	SNOW_VS_OUTPUT = SNOW_VS_INPUT;
#endif

	RAGE_COMPUTE_CLIP_DISTANCES(OUT.pos);

	return OUT;
}// end of VS_VehicleTransformWaterReflection()...

// =============================================================================================== //
// VS_VehicleTransformD
// =============================================================================================== //

vehicleVertexOutputD VS_VehicleTransformD(vehicleVertexInputBump IN RAGE_ENABLE_OFFSET_FUNC_ARGS(rageVertexOffsetBump))
{
vehicleVertexOutputD OUT;

	float3 inPos = IN.pos;
	float3 inNrm = IN.normal;
	float4 inCpv = IN.diffuse;

#if VEHICLESHADER_SNOW
	const float2 snowBlend = VS_CalculateSnowBlend( saturate(IN.diffuse.a) );
#endif

#if NORMAL_MAP || NORMAL_MAP2
	float4 inTan = IN.tangent;
#endif

#ifdef RAGE_ENABLE_OFFSET
	if( bEnableOffsets != 0.0f )
	{
		inPos += INOFFSET.pos;
		inNrm += INOFFSET.normal;
		inCpv += INOFFSET.diffuse;
#if NORMAL_MAP || NORMAL_MAP2
		inTan += INOFFSET.tangent;
#endif
	}
#endif // RAGE_ENABLE_OFFSET

#if defined(USE_VEHICLE_DAMAGE)
	float3 inDmgPos = inPos;
	float3 inDmgNrm = inNrm;
	float specDmgScale = 1.0;
	specDmgScale = ApplyVehicleDamage(inDmgPos, inDmgNrm, VEHICLE_DAMAGE_SCALE, inPos, inNrm);
	#if RENDER_DEBUG_DAMAGECOLOR
		float damageLength = length(inDmgPos-inPos);
	#endif // RENDER_DEBUG_DAMAGECOLOR
#elif defined(USE_VEHICLE_DAMAGE_EDGE)
	float specDmgScale = inCpv.g;	// already calculated by EDGE
#endif //USE_VEHICLE_DAMAGE...

	inCpv = CalculateDimmer(inCpv);

	float3 pvlWorldNormal, pvlWorldPosition, t_worldEyePos;

	vehicleProcessVertLightingUnskinned(	inPos,
											inNrm,
#if NORMAL_MAP || NORMAL_MAP2
											inTan,
#endif // NORMAL_MAP || NORMAL_MAP2
											inCpv,
#if VEHICLE_INSTANCED_WHEEL_SHADER
											(float4x3) matWheelWorld,
#else
											(float4x3) gWorld,
#endif//VEHICLE_INSTANCED_WHEEL_SHADER..
											pvlWorldPosition,
#if REFLECT || SECOND_SPECULAR_LAYER
											OUT.worldEyePos,
#else
											t_worldEyePos,
#endif
											pvlWorldNormal,
#if NORMAL_MAP || NORMAL_MAP2
											OUT.worldBinormal,
											OUT.worldTangent,
#endif // NORMAL_MAP || NORMAL_MAP2
											OUT.color0
											);


#ifdef VEHICLE_TYRE_DEFORM
	float3 inputPos		= inPos;
	float3 inputWorldPos= pvlWorldPosition;
	float4 inputColor	= OUT.color0;
	ApplyTyreDeform(inputPos, inputWorldPos, inputColor, inPos, OUT.color0, true);

	#ifdef VEHICLE_TYRE_DEBUG_MEASUREMENT
		OUT.tyreParams.x = tyreDeformParams2.x;	//=VEHICLE_TYRE_INNER_RAD
		OUT.tyreParams.y = inPos.y;
		OUT.tyreParams.z = inPos.z;
	#endif //VEHICLE_TYRE_DEBUG_MEASUREMENT...
#endif //VEHICLE_TYRE_DEFORM...



#if RENDER_DEBUG_DAMAGECOLOR
	OUT.color0 = DebugGetDamageColor(damageLength,OUT.color0);
#endif // RENDER_DEBUG_DAMAGECOLOR

#if	defined(USE_VEHICLE_DAMAGE)
	if (bDebugDisplayDamageMap)
	{
	 	OUT.color0 = DamageSampleValue(inDmgPos);
	}
	if (bDebugDisplayDamageScale)
	{
	 	OUT.color0 = float4(VEHICLE_DAMAGE_SCALE, VEHICLE_DAMAGE_SCALE, VEHICLE_DAMAGE_SCALE, 1.0);
	}
#endif

	// Write out final position & texture coords
#if UMOVEMENTS
	inPos = VS_ApplyMicromovements(inPos, IN.specular.rgb);
#endif

#if VEHICLESHADER_SNOW
	inPos = VS_ApplySnowTransform(inPos, inNrm, snowBlend);
#endif

#if VEHICLE_INSTANCED_WHEEL_SHADER
	OUT.pos =  mul(float4(inPos,1), matWheelWorldViewProj);
#else
	OUT.pos =  mul(float4(inPos,1), gWorldViewProj);
#endif

#if defined(USE_VEHICLE_DAMAGE) || defined(USE_VEHICLE_DAMAGE_EDGE)
	OUT.worldNormal		= float4(pvlWorldNormal, specDmgScale);
#else
	OUT.worldNormal		= pvlWorldNormal;
#endif

	// NOTE: These 3 statements may resolve to the exact same code -- rely on HLSL to strip worthless code
	DIFFUSE_VS_OUTPUT	= DIFFUSE_VS_INPUT;
#if NORMAL_MAP
	BUMP_VS_OUTPUT		= BUMP_VS_INPUT;
#endif
#if SPEC_MAP
	SPECULAR_VS_OUTPUT	= SPECULAR_VS_INPUT;
#endif	// SPEC_MAP

	OUT.texCoord.xy		= VehicleAnimateUVs(OUT.texCoord.xy, inPos);

#if DIFFUSE2
	// Pack in 2nd set of texture coords if needed
	DIFFUSE2_VS_OUTPUT	= DIFFUSE2_VS_INPUT;
#endif	// DIFFUSE2
#if NORMAL_MAP2
	BUMP2_VS_OUTPUT = BUMP2_VS_INPUT;
#endif
#if DIFFUSE3
	DIFFUSE3_VS_OUTPUT	= DIFFUSE3_VS_INPUT;
#endif	// DIFFUSE3
#if DETAIL_UV
	DETAIL_VS_OUTPUT = DETAIL_VS_INPUT;
#endif // DETAIL_UV...
#if DIRT_UV
	DIRT_VS_OUTPUT	= DIRT_VS_INPUT;
#endif //DIRT_UV...
#if VEHICLESHADER_SNOW
	SNOW_VS_OUTPUT	= SNOW_VS_INPUT;
	SNOW_VS_OUTPUTD = VS_CalculateSnowPsParams(snowBlend);
#endif

	return OUT;
}// end of VS_VehicleTransformD()...


#if defined(DO_WHEEL_ROUNDING_TESSELLATION)

[domain("tri")]
vehicleVertexOutputD DS_WR_VehicleTransformD( patchConstantInfo PatchInfo, float3 UVW : SV_DomainLocation, const OutputPatch<vehicleVertexInputBump_CtrlPoint, 3> PatchPoints )
{
	// Apply the deformation/rounding.
	vehicleVertexInputBump Arg = DS_WR_vehicleVertexInputBump(PatchInfo, UVW, PatchPoints);

	// Transform as per the original vertex shader.
#ifdef RAGE_ENABLE_OFFSET
	return VS_VehicleTransformD(Arg, INOFFSET, bEnableOffsets);
#else
	return VS_VehicleTransformD(Arg);
#endif
}

#endif // defined(DO_WHEEL_ROUNDING_TESSELLATION)

#if SHADOW_CASTING && !__MAX

	vertexOutputLD VS_VehicleLinearDepth(vertexInputLD IN)
	{
		vertexOutputLD OUT;

	#if VEHICLE_INSTANCED_WHEEL_SHADER
		IN.pos = (float3)mul(float4(IN.pos,1.0f), matWheelWorld);	
	#endif

		OUT = VS_LinearDepth(IN);
		return(OUT);
	}

#endif // SHADOW_CASTING



//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//
//
//
vehicleVertexOutputUnlit VS_VehicleTransformUnlit(vehicleVertexInput IN RAGE_ENABLE_OFFSET_FUNC_ARGS(rageVertexOffsetBump))
{
vehicleVertexOutputUnlit OUT;
	float3 inPos = IN.pos;
	float4 inCpv = IN.diffuse;

#ifdef RAGE_ENABLE_OFFSET
	if( bEnableOffsets != 0.0f )
	{
		inPos += INOFFSET.pos;
		inCpv += INOFFSET.diffuse;
	}
#endif // RAGE_ENABLE_OFFSET
	
#ifdef USE_VEHICLE_DAMAGE
	float3 inDmgPos = inPos;
	ApplyVehicleDamage(inDmgPos, VEHICLE_DAMAGE_SCALE, inPos);
	#if RENDER_DEBUG_DAMAGECOLOR
		float damageLength = length(inDmgPos-inPos);
	#endif // RENDER_DEBUG_DAMAGECOLOR	
#endif // USE_VEHICLE_DAMAGE

	// Write out final position & texture coords
	OUT.pos =  mul(float4(inPos,1), gWorldViewProj);
	// NOTE: These 3 statements may resolve to the exact same code -- rely on HLSL to strip worthless code
	DIFFUSE_VS_OUTPUT = DIFFUSE_VS_INPUT;
#if NORMAL_MAP
	BUMP_VS_OUTPUT = BUMP_VS_INPUT;
#endif
#if SPEC_MAP
	SPECULAR_VS_OUTPUT = SPECULAR_VS_INPUT;
#endif	// SPEC_MAP
#if DIFFUSE2
	// Pack in 2nd set of texture coords if needed
	DIFFUSE2_VS_OUTPUT	= DIFFUSE2_VS_INPUT;
#endif	// DIFFUSE2
#if NORMAL_MAP2
	BUMP2_VS_OUTPUT = BUMP2_VS_INPUT;
#endif
#if DIFFUSE3
	DIFFUSE3_VS_OUTPUT	= DIFFUSE3_VS_INPUT;
#endif	// DIFFUSE3
#if DETAIL_UV
	DETAIL_VS_OUTPUT = DETAIL_VS_INPUT;
#endif // DETAIL_UV...
#if DIRT_UV
	DIRT_VS_OUTPUT	= DIRT_VS_INPUT;
#endif
#if VEHICLESHADER_SNOW
	SNOW_VS_OUTPUT = SNOW_VS_INPUT;
#endif

	OUT.color0 = inCpv;

#if RENDER_DEBUG_DAMAGECOLOR
	OUT.color0 = DebugGetDamageColor(damageLength,OUT.color0);
#endif // RENDER_DEBUG_DAMAGECOLOR

#if	defined(USE_VEHICLE_DAMAGE)
	if (bDebugDisplayDamageMap)
	{
	 	OUT.color0 = DamageSampleValue(inDmgPos);
	}
	if (bDebugDisplayDamageScale)
	{
	 	OUT.color0 = float4(VEHICLE_DAMAGE_SCALE, VEHICLE_DAMAGE_SCALE, VEHICLE_DAMAGE_SCALE, 1.0);
	}
#endif

	return OUT;
}// end of VS_VehicleTransformUnlit()...


#if __MAX
//
//
//
//
vehicleVertexOutputUnlit VS_MaxTransformUnlit(maxVertexInput IN)
{
vehicleVertexInput vehicleIN;

	vehicleIN.pos		= IN.pos;
	vehicleIN.diffuse	= IN.diffuse;

	vehicleIN.texCoord0	= Max2RageTexCoord2(IN.texCoord0.xy);
#if DIFFUSE_UV1 || DIFFUSE2_UV1 || DIFFUSE3_UV1 || VEH_SNOW_UV1
	vehicleIN.texCoord2 = Max2RageTexCoord2(IN.texCoord1.xy);
	#if DIRT_UV || DIFFUSE_UV2
		vehicleIN.texCoord3.xy	= Max2RageTexCoord2(IN.texCoord2.xy);
	#endif // DIRT_UV
#else // DIFFUSE_UV1 || DIFFUSE2_UV1 || DIFFUSE3_UV1 || VEH_SNOW_UV1
	#if DIRT_UV || DETAIL_UV
		vehicleIN.texCoord2	= Max2RageTexCoord2(IN.texCoord1.xy);
	#endif // DIRT_UV
#endif // DIFFUSE_UV1 || DIFFUSE2_UV1 || DIFFUSE3_UV1 || VEH_SNOW_UV1

	vehicleIN.normal	= IN.normal;

	return VS_VehicleTransformUnlit(vehicleIN);

}// end of VS_MaxTransformUnlit()...


//
//
//
//
vehicleVertexOutput VS_MaxTransform(maxVertexInput IN)
{
vehicleVertexInputBump vehicleIN;

	vehicleIN.pos			= IN.pos;
	vehicleIN.diffuse		= IN.diffuse;

	vehicleIN.texCoord0	= Max2RageTexCoord2(IN.texCoord0.xy);
#if DIFFUSE_UV1 || DIFFUSE2_UV1 || DIFFUSE3_UV1 || VEH_SNOW_UV1
	vehicleIN.texCoord2 = Max2RageTexCoord2(IN.texCoord1.xy);
	#if DIRT_UV || DIFFUSE_UV2
		vehicleIN.texCoord3.xy	= Max2RageTexCoord2(IN.texCoord2.xy);
	#endif // DIRT_UV
#else // DIFFUSE_UV1 || DIFFUSE2_UV1 || DIFFUSE3_UV1 || VEH_SNOW_UV1
	#if DIRT_UV || DETAIL_UV
		vehicleIN.texCoord2	= Max2RageTexCoord2(IN.texCoord1.xy);
	#endif // DIRT_UV
#endif // DIFFUSE_UV1 || DIFFUSE2_UV1 || DIFFUSE3_UV1 || VEH_SNOW_UV1
	vehicleIN.normal		= IN.normal;
	vehicleIN.tangent		= float4(IN.tangent, 1.0f);

	vehicleVertexOutput OUT = VS_VehicleTransform(vehicleIN RAGE_ENABLE_OFFSET_FUNC_ARGS(rageVertexOffsetBump))

	RAGE_COMPUTE_CLIP_DISTANCES(OUT.pos);

	return OUT;
}
#endif //__MAX...


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//
//
//
vehicleVertexOutput VS_VehicleTransformSkin(vehicleSkinVertexInputBump IN RAGE_ENABLE_OFFSET_FUNC_ARGS(rageSkinVertexOffsetBump))
{
vehicleVertexOutput OUT;
	float3 inPos = IN.pos;
	float3 inNrm = IN.normal;
	float4 inCpv = IN.diffuse;

#if VEHICLESHADER_SNOW
	const float2 snowBlend = VS_CalculateSnowBlend( saturate(IN.diffuse.a) );
#endif

#if NORMAL_MAP || NORMAL_MAP2
	float4 inTan = IN.tangent;
#endif

#ifdef RAGE_ENABLE_OFFSET
	if( bEnableOffsets != 0.0f )
	{
		inPos += INOFFSET.pos;
		inNrm += INOFFSET.normal;
		inCpv += INOFFSET.diffuse;
#if NORMAL_MAP || NORMAL_MAP2
		inTan += INOFFSET.tangent;
#endif
	}
#endif // RAGE_ENABLE_OFFSET

#ifdef USE_VEHICLE_DAMAGE
	float3 inDmgPos = inPos;
	float3 inDmgNrm = inNrm;
	float specDmgScale = 1.0;
	specDmgScale = ApplyVehicleDamage(inDmgPos, inDmgNrm, VEHICLE_DAMAGE_SCALE, inPos, inNrm);
#if RENDER_DEBUG_DAMAGECOLOR
	float damageLength = length(inDmgPos-inPos);
#endif // RENDER_DEBUG_DAMAGECOLOR	
#elif defined(USE_VEHICLE_DAMAGE_EDGE)
	float specDmgScale = inCpv.g;	// already calculated by EDGE
#endif //USE_VEHICLE_DAMAGE...

#if __PS3
	rageSkinMtx skinMtx = gBoneMtx[IN.blendindices.y]; // KS
#else // not __PS3
#if USE_UBYTE4
	int4 i	= D3DCOLORtoUBYTE4(IN.blendindices);
#else
	int4 i	= D3DCOLORtoUBYTE4(IN.blendindices);
#endif

#if __SHADERMODEL >= 40
	rageSkinMtx skinMtx = FetchBoneMatrix(i.x);
#else
	rageSkinMtx skinMtx = FetchBoneMatrix(i.z);
#endif
#endif // not __PS3

	inCpv = CalculateDimmer(inCpv);


	float3 pvlSkinnedNormal, pvlWorldNormal, pvlWorldPosition, t_worldEyePos;

	vehicleProcessVertLightingSkinned(	inPos,
										inNrm,
#if NORMAL_MAP || NORMAL_MAP2
										inTan,
#endif	// NORMAL_MAP
										inCpv,
										skinMtx,
										(float4x3)gWorld,
										pvlWorldPosition,
#if REFLECT							
										OUT.worldEyePos,
#else
										t_worldEyePos,
#endif			
										pvlSkinnedNormal,
										pvlWorldNormal,
#if NORMAL_MAP || NORMAL_MAP2
										OUT.worldBinormal,
										OUT.worldTangent,
#endif // NORMAL_MAP
										OUT.color0
										);

	// Write out final position & texture coords
	// Transform position by bone matrix
	float3 pos = rageSkinTransform(inPos, skinMtx);
#if UMOVEMENTS
	pos	= VS_ApplyMicromovements(pos, IN.specular.rgb);
#endif
#if VEHICLESHADER_SNOW
	pos = VS_ApplySnowTransform(pos, pvlSkinnedNormal, snowBlend);
#endif
	// Write out final position & texture coords
	OUT.pos =  mul(float4(pos,1), gWorldViewProj);

#if defined(USE_VEHICLE_DAMAGE) || defined(USE_VEHICLE_DAMAGE_EDGE)
	OUT.worldNormal		= float4(pvlWorldNormal, specDmgScale);
#else
	OUT.worldNormal		= pvlWorldNormal;
#endif

#if RENDER_DEBUG_DAMAGECOLOR
	OUT.color0 = DebugGetDamageColor(damageLength,OUT.color0);
#endif // RENDER_DEBUG_DAMAGECOLOR

#if	defined(USE_VEHICLE_DAMAGE)
	if (bDebugDisplayDamageMap)
	{
	 	OUT.color0 = DamageSampleValue(inDmgPos);
	}
	if (bDebugDisplayDamageScale)
	{
	 	OUT.color0 = float4(VEHICLE_DAMAGE_SCALE, VEHICLE_DAMAGE_SCALE, VEHICLE_DAMAGE_SCALE, 1.0);
	}
#endif

	// NOTE: These 3 statements may resolve to the exact same code -- rely on HLSL to strip worthless code
	DIFFUSE_VS_OUTPUT	= DIFFUSE_VS_INPUT;
#if NORMAL_MAP
	BUMP_VS_OUTPUT = BUMP_VS_INPUT;
#endif
#if SPEC_MAP
	SPECULAR_VS_OUTPUT	= SPECULAR_VS_INPUT;
#endif	// SPEC_MAP

	OUT.texCoord.xy		= VehicleAnimateUVs(OUT.texCoord.xy, inPos);

#if DIFFUSE2
	// Pack in 2nd set of texture coords if needed
	DIFFUSE2_VS_OUTPUT	= DIFFUSE2_VS_INPUT;
#endif	// DIFFUSE2
#if NORMAL_MAP2
	BUMP2_VS_OUTPUT = BUMP2_VS_INPUT;
#endif
#if DIFFUSE3
	DIFFUSE3_VS_OUTPUT	= DIFFUSE3_VS_INPUT;
#endif	// DIFFUSE3
#if DETAIL_UV
	DETAIL_VS_OUTPUT = DETAIL_VS_INPUT;
#endif // DETAIL_UV...
#if DIRT_UV
	DIRT_VS_OUTPUT	= DIRT_VS_INPUT;
#endif //DIRT_UV...

#if SHADOW_RECEIVING
	OUT.worldPos=float4(pvlWorldPosition,1.0f);
#endif

#if defined(USE_VERTEX_FOG)
	OUT.fogData = __CalcFogData(pvlWorldPosition - gViewInverse[3].xyz, 1.0f);
#elif defined(USE_VERTEX_FOG_ALPHA)
	OUT.worldPos.w = 1 - __CalcFogData(pvlWorldPosition - gViewInverse[3].xyz, 1.0f).w;
#endif // defined(USE_VERTEX_FOG_ALPHA)

#if VEHICLESHADER_SNOW
	SNOW_VS_OUTPUT	= SNOW_VS_INPUT;
	SNOW_VS_OUTPUTD = VS_CalculateSnowPsParams(snowBlend);
#endif

	RAGE_COMPUTE_CLIP_DISTANCES(OUT.pos);

	return OUT;
}// end of VS_VehicleTransformSkin()...


//
//
//
//
vehicleVertexOutputWaterReflection VS_VehicleTransformSkinWaterReflection(vehicleSkinVertexInputBump IN)
{
	vehicleVertexOutputWaterReflection OUT;

#if __PS3
	rageSkinMtx skinMtx = gBoneMtx[IN.blendindices.y]; // KS
#else // not __PS3
#if USE_UBYTE4
	int4 i	= D3DCOLORtoUBYTE4(IN.blendindices);
#else
	int4 i	= D3DCOLORtoUBYTE4(IN.blendindices);
#endif

#if __SHADERMODEL >= 40
	rageSkinMtx skinMtx = FetchBoneMatrix(i.x);
#else
	rageSkinMtx skinMtx = FetchBoneMatrix(i.z);
#endif
#endif // not __PS3

	float3 inPos = rageSkinTransform(IN.pos, skinMtx);
	float3 inNrm = rageSkinRotate(IN.normal, skinMtx);
	float4 inCpv = IN.diffuse;

	OUT.worldPos = mul(float4(inPos,1), gWorld);
	OUT.worldNormal = mul(inNrm, (float3x3)gWorld); // not normalised, that's ok
	OUT.color0 = inCpv;

	// Write out final position & texture coords
	OUT.pos = mul(float4(inPos,1), gWorldViewProj);

#if VEHICLE_SUPPORT_PAINT_RAMP && MATERIAL_DIFFUSE_COLOR
	OUT.worldEyePos = gViewInverse[3].xyz - OUT.worldPos;
#endif

	// NOTE: These 2 statements may resolve to the exact same code -- rely on HLSL to strip worthless code
	DIFFUSE_VS_OUTPUT = DIFFUSE_VS_INPUT;
#if NORMAL_MAP
	BUMP_VS_OUTPUT = BUMP_VS_INPUT;
#endif
#if DIFFUSE2
	// Pack in 2nd set of texture coords if needed
	DIFFUSE2_VS_OUTPUT = DIFFUSE2_VS_INPUT;
#endif	// DIFFUSE2
#if NORMAL_MAP2
	BUMP2_VS_OUTPUT = BUMP2_VS_INPUT;
#endif
#if DIFFUSE3
	DIFFUSE3_VS_OUTPUT = DIFFUSE3_VS_INPUT;
#endif	// DIFFUSE3
#if DETAIL_UV
	DETAIL_VS_OUTPUT = DETAIL_VS_INPUT;
#endif // DETAIL_UV...
#if DIRT_UV
	DIRT_VS_OUTPUT	= DIRT_VS_INPUT;
#endif //DIRT_UV...
#if VEHICLESHADER_SNOW
	SNOW_VS_OUTPUT = SNOW_VS_INPUT;
#endif

	RAGE_COMPUTE_CLIP_DISTANCES(OUT.pos);

	return OUT;
}// end of VS_VehicleTransformSkinWaterReflection()...

//
//
//
//
vehicleVertexOutputD VS_VehicleTransformSkinD(vehicleSkinVertexInputBump IN RAGE_ENABLE_OFFSET_FUNC_ARGS(rageSkinVertexOffsetBump))
{
vehicleVertexOutputD OUT;
	float3 inPos = IN.pos;
	float3 inNrm = IN.normal;
	float4 inCpv = IN.diffuse;

#if VEHICLESHADER_SNOW
	const float2 snowBlend = VS_CalculateSnowBlend( saturate(IN.diffuse.a) );
#endif

#if NORMAL_MAP || NORMAL_MAP2
	float4 inTan = IN.tangent;
#endif

#ifdef RAGE_ENABLE_OFFSET
	if( bEnableOffsets != 0.0f )
	{
		inPos += INOFFSET.pos;
		inNrm += INOFFSET.normal;
		inCpv += INOFFSET.diffuse;
#if NORMAL_MAP || NORMAL_MAP2
		inTan += INOFFSET.tangent;
#endif
	}
#endif // RAGE_ENABLE_OFFSET

#if defined(USE_VEHICLE_DAMAGE)
	float3 inDmgPos = inPos;
	float3 inDmgNrm = inNrm;
	float specDmgScale = 1.0;
	specDmgScale = ApplyVehicleDamage(inDmgPos, inDmgNrm, VEHICLE_DAMAGE_SCALE, inPos, inNrm);
	#if RENDER_DEBUG_DAMAGECOLOR
		float damageLength = length(inDmgPos-inPos);
	#endif // RENDER_DEBUG_DAMAGECOLOR	
#elif defined(USE_VEHICLE_DAMAGE_EDGE)
	float specDmgScale = inCpv.g;	// already calculated by EDGE
#endif // USE_VEHICLE_DAMAGE

#if __PS3
	rageSkinMtx skinMtx = gBoneMtx[IN.blendindices.y];
#else // not __PS3
#if USE_UBYTE4
	int4 i	= D3DCOLORtoUBYTE4(IN.blendindices);
#else
	int4 i	= D3DCOLORtoUBYTE4(IN.blendindices);
#endif

#if __SHADERMODEL >= 40
	rageSkinMtx skinMtx = FetchBoneMatrix(i.x);
#else
	rageSkinMtx skinMtx = FetchBoneMatrix(i.z);
#endif
#endif // not __PS3

	inCpv = CalculateDimmer(inCpv);

#if USE_GLOW
	// Not supported in deferred rendering. yet.
#endif // USE_GLOW

	float3 pvlSkinnedNormal, pvlWorldNormal, pvlWorldPosition, t_worldEyePos;

	vehicleProcessVertLightingSkinned(	inPos, 
										inNrm, 
#if NORMAL_MAP || NORMAL_MAP2
										inTan,
#endif	// NORMAL_MAP
										inCpv,
										skinMtx, 
										(float4x3)gWorld,
										pvlWorldPosition,
#if REFLECT || SECOND_SPECULAR_LAYER
										OUT.worldEyePos,
#else
										t_worldEyePos,
#endif								
										pvlSkinnedNormal,
										pvlWorldNormal,
#if NORMAL_MAP || NORMAL_MAP2
										OUT.worldBinormal,
										OUT.worldTangent,
#endif // NORMAL_MAP
										OUT.color0 
										);

	// Write out final position & texture coords
	// Transform position by bone matrix
	float3 pos = rageSkinTransform(inPos, skinMtx);
#if UMOVEMENTS
	pos	= VS_ApplyMicromovements(pos, IN.specular.rgb);
#endif
#if VEHICLESHADER_SNOW
	pos = VS_ApplySnowTransform(pos, pvlSkinnedNormal, snowBlend);
#endif
	// Write out final position & texture coords
	OUT.pos =  mul(float4(pos,1), gWorldViewProj);

#if defined(USE_VEHICLE_DAMAGE) || defined(USE_VEHICLE_DAMAGE_EDGE)
	OUT.worldNormal		= float4(pvlWorldNormal, specDmgScale);
#else
	OUT.worldNormal		= pvlWorldNormal;
#endif

#if RENDER_DEBUG_DAMAGECOLOR
	OUT.color0 = DebugGetDamageColor(damageLength,OUT.color0);
#endif // RENDER_DEBUG_DAMAGECOLOR

#if	defined(USE_VEHICLE_DAMAGE)
	if (bDebugDisplayDamageMap)
	{
	 	OUT.color0 = DamageSampleValue(inDmgPos);
	}
	if (bDebugDisplayDamageScale)
	{
	 	OUT.color0 = float4(VEHICLE_DAMAGE_SCALE, VEHICLE_DAMAGE_SCALE, VEHICLE_DAMAGE_SCALE, 1.0);
	}
#endif

	// NOTE: These 3 statements may resolve to the exact same code -- rely on HLSL to strip worthless code
	DIFFUSE_VS_OUTPUT	= DIFFUSE_VS_INPUT;
#if NORMAL_MAP
	BUMP_VS_OUTPUT = BUMP_VS_INPUT;
#endif
#if SPEC_MAP
	SPECULAR_VS_OUTPUT	= SPECULAR_VS_INPUT;
#endif	// SPEC_MAP

	OUT.texCoord.xy		= VehicleAnimateUVs(OUT.texCoord.xy, inPos);

#if DIFFUSE2
	// Pack in 2nd set of texture coords if needed
	DIFFUSE2_VS_OUTPUT	= DIFFUSE2_VS_INPUT;
#endif	// DIFFUSE2
#if NORMAL_MAP2
	BUMP2_VS_OUTPUT = BUMP2_VS_INPUT;
#endif
#if DIFFUSE3
	DIFFUSE3_VS_OUTPUT	= DIFFUSE3_VS_INPUT;
#endif	// DIFFUSE3
#if DETAIL_UV
	DETAIL_VS_OUTPUT = DETAIL_VS_INPUT;
#endif // DETAIL_UV...
#if DIRT_UV
	DIRT_VS_OUTPUT	= DIRT_VS_INPUT;
#endif //DIRT_UV...

#ifdef VEHICLE_TYRE_DEBUG_MEASUREMENT
	OUT.tyreParams = float3(0,0,0);
#endif //VEHICLE_TYRE_DEBUG_MEASUREMENT...

#if VEHICLESHADER_SNOW
	SNOW_VS_OUTPUT  = SNOW_VS_INPUT;
	SNOW_VS_OUTPUTD = VS_CalculateSnowPsParams(snowBlend);
#endif

	return OUT;
}// end of VS_VehicleTransformSkinD()...

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//
//
//
vehicleVertexOutputUnlit VS_VehicleTransformSkinUnlit(vehicleSkinVertexInput IN RAGE_ENABLE_OFFSET_FUNC_ARGS(rageSkinVertexOffset))
{
vehicleVertexOutputUnlit OUT;
	float3 inPos = IN.pos;
	float4 inCpv = IN.diffuse;

#ifdef RAGE_ENABLE_OFFSET
	if( bEnableOffsets != 0.0f )
	{
		inPos += INOFFSET.pos;
	}
#endif // RAGE_ENABLE_OFFSET
	
#ifdef USE_VEHICLE_DAMAGE
	float3 inDmgPos = inPos;
	ApplyVehicleDamage(inDmgPos, VEHICLE_DAMAGE_SCALE, inPos);
	#if RENDER_DEBUG_DAMAGECOLOR	
		float damageLength = length(inDmgPos-inPos);	
	#endif // RENDER_DEBUG_DAMAGECOLOR	
#endif // USE_VEHICLE_DAMAGE

#if __PS3
	rageSkinMtx boneMtx = gBoneMtx[IN.blendindices.y]; // KS
#else // not __PS3
#if USE_UBYTE4
	int4 i	= D3DCOLORtoUBYTE4(IN.blendindices);
#else
	int4 i	= D3DCOLORtoUBYTE4(IN.blendindices);
#endif

#if __SHADERMODEL >= 40
	rageSkinMtx boneMtx = FetchBoneMatrix(i.x);
#else
	rageSkinMtx boneMtx = FetchBoneMatrix(i.z);
#endif
#endif // not __PS3

	inCpv = CalculateDimmer(inCpv);


#if USE_GLOW
	// Not supported in unlit rendering. yet.
#endif // USE_GLOW

	// Transform position by bone matrix
	float3 pos = rageSkinTransform(inPos, boneMtx);

    // Write out final position & texture coords
    OUT.pos =  /*MonoToStereoClipSpace*/(mul(float4(pos,1), gWorldViewProj));
    // NOTE: These 3 statements may resolve to the exact same code -- rely on HLSL to strip worthless code
    DIFFUSE_VS_OUTPUT = DIFFUSE_VS_INPUT;
#if NORMAL_MAP
	BUMP_VS_OUTPUT = BUMP_VS_INPUT;
#endif
#if SPEC_MAP
	SPECULAR_VS_OUTPUT = SPECULAR_VS_INPUT;
#endif	// SPEC_MAP
#if DIFFUSE2
	// Pack in 2nd set of texture coords if needed
	DIFFUSE2_VS_OUTPUT	= DIFFUSE2_VS_INPUT;
#endif // DIFFUSE2
#if NORMAL_MAP2
	BUMP2_VS_OUTPUT = BUMP2_VS_INPUT;
#endif
#if DIFFUSE3
	DIFFUSE3_VS_OUTPUT	= DIFFUSE3_VS_INPUT;
#endif // DIFFUSE3
#if DETAIL_UV
	DETAIL_VS_OUTPUT = DETAIL_VS_INPUT;
#endif // DETAIL_UV...
#if DIRT_UV
	DIRT_VS_OUTPUT	= DIRT_VS_INPUT;
#endif
#if VEHICLESHADER_SNOW
	SNOW_VS_OUTPUT = SNOW_VS_INPUT;
#endif

	OUT.color0 = inCpv;	// IN.diffuse
#if RENDER_DEBUG_DAMAGECOLOR
	OUT.color0 = DebugGetDamageColor(damageLength,OUT.color0);
#endif // RENDER_DEBUG_DAMAGECOLOR

#if	defined(USE_VEHICLE_DAMAGE)
	if (bDebugDisplayDamageMap)
	{
	 	OUT.color0 = DamageSampleValue(inDmgPos);
	}
	if (bDebugDisplayDamageScale)
	{
	 	OUT.color0 = float4(VEHICLE_DAMAGE_SCALE, VEHICLE_DAMAGE_SCALE, VEHICLE_DAMAGE_SCALE, 1.0);
	}
#endif

	return OUT;
}// end of VS_VehicleTransformSkinUnlit()...



#if APPLY_DOF_TO_ALPHA
vehicleVertexOutputDOF VS_VehicleTransformDOF(vehicleVertexInputBump IN RAGE_ENABLE_OFFSET_FUNC_ARGS(rageVertexOffsetBump))
{
	vehicleVertexOutput OutTransform = VS_VehicleTransform(IN
	#ifdef RAGE_ENABLE_OFFSET
						, INOFFSET, gbOffsetEnable
	#endif
						);

	vehicleVertexOutputDOF OUT;
	OUT.vertOutput = OutTransform;
	OUT.linearDepth = OutTransform.pos.w;

	return OUT;
}

vehicleVertexOutputDOF VS_VehicleTransformSkinDOF(vehicleSkinVertexInputBump IN RAGE_ENABLE_OFFSET_FUNC_ARGS(rageVertexOffsetBump))
{
	vehicleVertexOutput OutTransform = VS_VehicleTransformSkin(IN
	#ifdef RAGE_ENABLE_OFFSET
						, INOFFSET, gbOffsetEnable
	#endif
						);

	vehicleVertexOutputDOF OUT;
	OUT.vertOutput = OutTransform;
	OUT.linearDepth = OutTransform.pos.w;

	return OUT;
}

#endif //APPLY_DOF_TO_ALPHA



//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//
// NOTE: Do we want to configure the filter & wrap states via the texture itself??
//
//
//
//
#if DIFFUSE2
	#ifndef UINAME_SAMPLER_DIFFUSETEXTURE2
		#define UINAME_SAMPLER_DIFFUSETEXTURE2	"Secondary Texture"
	#endif
	BeginSampler(sampler2D,diffuseTexture2,DiffuseSampler2,DiffuseTex2)
		string UIName=UINAME_SAMPLER_DIFFUSETEXTURE2;
		string UIHint="uv1";
		string TCPTemplate="Diffuse";
	string	TextureType="Diffuse";
	ContinueSampler(sampler2D,diffuseTexture2,DiffuseSampler2,DiffuseTex2)
		AddressU  = WRAP;
		AddressV  = WRAP;
		AddressW  = WRAP;
		MIPFILTER = MIPLINEAR;
	#if DIFFUSE2_ANISOTROPIC
		MINFILTER = MINANISOTROPIC;
		MAGFILTER = MAGLINEAR; 
		ANISOTROPIC_LEVEL(4)
	#else
		MINFILTER = HQLINEAR;
		MAGFILTER = MAGLINEAR;
	#endif
	EndSampler;
#endif	// DIFFUSE2

#if DIFFUSE3
	#ifndef UINAME_SAMPLER_DIFFUSETEXTURE3
		#define UINAME_SAMPLER_DIFFUSETEXTURE3	"Third Texture"
	#endif
	BeginSampler(sampler2D,diffuseTexture3,DiffuseSampler3,DiffuseTex3)
		string UIName=UINAME_SAMPLER_DIFFUSETEXTURE3;
		string UIHint="uv1";
		string TCPTemplate="Diffuse";
	string	TextureType="Diffuse";
	ContinueSampler(sampler2D,diffuseTexture3,DiffuseSampler3,DiffuseTex3)
		AddressU  = WRAP;
		AddressV  = WRAP;
		AddressW  = WRAP;
		MIPFILTER = MIPLINEAR;
	#if DIFFUSE3_ANISOTROPIC
		MINFILTER = MINANISOTROPIC;
		MAGFILTER = MAGLINEAR; 
		ANISOTROPIC_LEVEL(4)
	#else
		MINFILTER = HQLINEAR;
		MAGFILTER = MAGLINEAR;
	#endif
	EndSampler;
#endif	// DIFFUSE3

#if DIRT_LAYER
#ifndef UINAME_SAMPLER_DIRTTEXTURE
	#define UINAME_SAMPLER_DIRTTEXTURE		"Dirt Texture"
#endif
BeginSampler(sampler2D,dirtTexture,DirtSampler,DirtTex)
	string UIName=UINAME_SAMPLER_DIRTTEXTURE;
	string TCPTemplate="Diffuse";
	string	TextureType="Dirt";
ContinueSampler(sampler2D,dirtTexture,DirtSampler,DirtTex)
	AddressU  = WRAP;
	AddressV  = WRAP;
	AddressW  = WRAP;
	MIPFILTER = MIPLINEAR;
	MINFILTER = HQLINEAR;
	MAGFILTER = MAGLINEAR;
EndSampler;

	#if DIRT_NORMALMAP
	#ifndef UINAME_SAMPLER_DIRTBUMPTEXTURE
		#define UINAME_SAMPLER_DIRTBUMPTEXTURE		"Dirt Bump Texture"
	#endif
	BeginSampler(sampler2D,dirtBumpTexture,DirtBumpSampler,DirtBumpTex)
		string UIName=UINAME_SAMPLER_DIRTBUMPTEXTURE;
	string TCPTemplate="NormalMap";
	string	TextureType="DirtBump";
	ContinueSampler(sampler2D,dirtBumpTexture,DirtBumpSampler,DirtBumpTex)
		AddressU  = WRAP;
		AddressV  = WRAP;
		AddressW  = WRAP;
		MIPFILTER = MIPLINEAR;
		MINFILTER = HQLINEAR;
		MAGFILTER = MAGLINEAR;
	EndSampler;
	#endif//DIRT_NORMALMAP...
#endif //DIRT_LAYER...

#if NORMAL_MAP
	#ifndef UINAME_SAMPLER_BUMPTEXTURE
		#define UINAME_SAMPLER_BUMPTEXTURE		"Bump Texture"
	#endif
	BeginSampler(sampler2D,bumpTexture,BumpSampler,BumpTex)
		string UIName=UINAME_SAMPLER_BUMPTEXTURE;
		string UIHint="normalmap";
	string TCPTemplate="NormalMap";
	string	TextureType="Bump";
	#if !defined(ALLOW_NORMAL_MAP_STRIPPING)
		WIN32PC_DONTSTRIP
	#endif //!defined(ALLOW_NORMAL_MAP_STRIPPING)
	ContinueSampler(sampler2D,bumpTexture,BumpSampler,BumpTex)
		AddressU  = WRAP;
		AddressV  = WRAP;
		AddressW  = WRAP;
		MIPFILTER = MIPLINEAR;
		MINFILTER = HQLINEAR;
		MAGFILTER = MAGLINEAR;
	EndSampler;
#endif	// NORMAL_MAP

#if NORMAL_MAP2
	#ifndef UINAME_SAMPLER_BUMPTEXTURE2
		#define UINAME_SAMPLER_BUMPTEXTURE2		"Secondary Bump Texture"
	#endif
	BeginSampler(sampler2D,bumpTexture2,BumpSampler2,BumpTex2)
		string UIName=UINAME_SAMPLER_BUMPTEXTURE2;
		string UIHint="normalmap2";
	string TCPTemplate="NormalMap";
	string	TextureType="Bump";
	#if !defined(ALLOW_NORMAL_MAP_STRIPPING)
		WIN32PC_DONTSTRIP
	#endif //!defined(ALLOW_NORMAL_MAP_STRIPPING)
	ContinueSampler(sampler2D,bumpTexture2,BumpSampler2,BumpTex2)
		AddressU  = WRAP;
		AddressV  = WRAP;
		AddressW  = WRAP;
		MIPFILTER = MIPLINEAR;
		MINFILTER = HQLINEAR;
		MAGFILTER = MAGLINEAR;
	EndSampler;
#endif	// NORMAL_MAP2

#if SPEC_MAP
	#ifndef UINAME_SAMPLER_SPECTEXTURE
		#define UINAME_SAMPLER_SPECTEXTURE		"Specular Texture"
	#endif
	BeginSampler(sampler2D,specTexture,SpecSampler,SpecularTex)
		string UIName=UINAME_SAMPLER_SPECTEXTURE;
		#if SPECULAR_UV1
			string UIHint="specularmap,uv1";
		#else
			string UIHint="specularmap";
		#endif
		string TCPTemplate="Specular";
	string	TextureType="Specular";
		WIN32PC_DONTSTRIP
	ContinueSampler(sampler2D,specTexture,SpecSampler,SpecularTex)
		AddressU  = WRAP;
		AddressV  = WRAP;
		AddressW  = WRAP;
		MIPFILTER = MIPLINEAR;
		MINFILTER = HQLINEAR;
		MAGFILTER = MAGLINEAR;
	#ifdef METALLIC_SPECMAP_MIP_BIAS
		MIPMAPLODBIAS = -1.5;
	#endif
	EndSampler;
#endif	// SPEC_MAP

//
//
//
//
#if DIRT_LAYER
float4 CalculateDirtLayer(float4 basicColor, float2 coordUV, float dirtLevelScaled, float secondLayerScale_, float dimmerStrength, out float specularMult)
{
float4 outColor = basicColor;

	specularMult = 1.0f;

	const float secondBurnoutLayerScale = secondLayerScale_*burnout2ndLayerScale;	// dirt: 0, burnout: burnout scale

	const float dirtTileUV = 1.0f + burnout2ndLayerScale;	// dirtTileUV: 1.0 for dirt, 2.0 for burnout

	// color modulated dirt:
	float4 dirtColorTex		= tex2D(DirtSampler, coordUV * dirtTileUV);
	float4 dirtColorTexWet	= dirtColorTex;
	dirtColorTexWet.r		= lerp(dirtColorTex.r, dirtColorTex.b, gWetnessBlend * nonDirtSpecIntScale);	// blend to wet dirt only when dirt is on, do nothing for burnout
	
	float2 dirtAlpha		= dirtColorTexWet.rg * dirtLevelScaled * lerp(1.0f, secondBurnoutLayerScale, burnout2ndLayerScale); // dirt: 1, burnout: burnout scale
	float3 secondBurnLayer	= dirtColorTexWet.bbb;

#if EMISSIVE_NIGHTONLY
	dirtAlpha.r = saturate(dirtAlpha.r * (1.0f-gDayNightEffects*100.0f));	// lightsemissive: disable dirt on lamps at night
#endif
#if DIMMER
	dirtAlpha.r = saturate(dirtAlpha.r * (1.0f-saturate(dimmerStrength-2.0f)*0.75f)); // lightemissive: disable dirt on lamps when dimmer is >2.0 during the day
#endif

	outColor.xyz		= lerp(outColor.xyz, (dirtColor.xyz*dirtModulator), dirtAlpha.r);
	outColor.xyz		= lerp(outColor.xyz, secondBurnLayer.xyz, secondBurnoutLayerScale * dirtLevelScaled);
	#ifdef USE_VEHICLE_ALPHA
		// Jimmy's tweak for non-transparent dirt on vehglass:
		outColor.a		+= dirtAlpha.r;	// vehglass: modulate glass alpha with dirt alpha
	#endif
	specularMult *= (1.0f - dirtAlpha.g);

	return(outColor);
}
#endif//DIRT_LAYER...


//
//
//
//
float3 ProcessMatDiffuseColor(float3 bodyColor, float4 cpv)
{
#if BURNOUT_IN_MATDIFFCOLOR
	return lerp(bodyColor, float3(1,1,1), dirtLevel * cpv.b * burnout2ndLayerScale);	// burnout: blend bodycolor to white
#else
	return bodyColor;
#endif
}

#if PAINT_RAMP
DECLARE_SAMPLER(sampler2D, DiffuseRampTexture, DiffuseRampTextureSampler,
	AddressU  = CLAMP;
	AddressV  = CLAMP;
	MINFILTER = LINEAR;
	MAGFILTER = LINEAR;
	MIPFILTER = LINEAR;
);
// Albert: Unforunate hack to avoid overlapping with FogRayTex at s11
#if __FXL
#define DECLARE_SAMPLER_AT_REGISTER(samplerType,texName,samplerName,samp,states) samplerType samplerName : texName REGISTER(samp) = sampler_state { states }
#else
#define DECLARE_SAMPLER_AT_REGISTER(samplerType,texName,samplerName,samp,states) texture texName : texName; samplerType samplerName REGISTER(samp) = sampler_state { texture = < texName >; states }
#endif
DECLARE_SAMPLER_AT_REGISTER(sampler2D, SpecularRampTexture, SpecularRampTextureSampler, s14,
	AddressU  = CLAMP;
	AddressV  = CLAMP;
	MINFILTER = LINEAR;
	MAGFILTER = LINEAR;
	MIPFILTER = LINEAR;
);
#endif

#if defined(VEHICLE_GLASS_CRACK_TEXTURE)
DECLARE_SAMPLER(sampler2D, vehglassCrackTexture, vehglassCrackTextureSampler,
	AddressU       = WRAP;
	AddressV       = WRAP;
	MIPFILTER      = MIPLINEAR;
	MINFILTER      = MINANISOTROPIC;
	MAGFILTER      = MAGLINEAR;
	ANISOTROPIC_LEVEL(2)
	MipMapLodBias  = -1.5;
);
#endif // defined(VEHICLE_GLASS_CRACK_TEXTURE)

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//
//
//
half4 PS_VehicleTexturedUnlit(vehicleVertexOutputUnlit IN) : COLOR
{
	float4 color = PS_GetBaseColorFromCPV(IN.color0);

	// Sum it all up
	float4 outColor =  tex2D(DiffuseSampler, DIFFUSE_PS_INPUT * DiffuseTexTileUV);
#if __MAX
	if(!useAlphaFromDiffuse)
	{
		outColor.a = 1.0f;	// fix for outsourcing
	}
#endif

#if MATERIAL_DIFFUSE_COLOR
	// body color modulation (diffuse color):
	outColor.rgb *= ProcessMatDiffuseColor(matDiffuseColor, IN.color0);
#endif

#if DIFFUSE2
	float4 overlayColor = tex2D(DiffuseSampler2, DIFFUSE2_PS_INPUT);
	outColor.rgb = outColor.rgb * (1.0 - overlayColor.w) + overlayColor.rgb * overlayColor.w;
#endif	// DIFFUSE2
#if DIFFUSE3
	float4 overlayColor = tex2D(DiffuseSampler3, DIFFUSE3_PS_INPUT);
	outColor.rgb = outColor.rgb * (1.0 - overlayColor.w) + overlayColor.rgb * overlayColor.w;
#endif	// DIFFUSE3

	outColor	*= color;
	outColor.a	*= globalAlpha*gInvColorExpBias;


#if DIRT_LAYER
	#if DIRT_UV
		float2 dirtUV = DIRT_PS_INPUT;
	#else
		float2 dirtUV = DIFFUSE_PS_INPUT;
	#endif
	float dirtSpecularMult;
	outColor = CalculateDirtLayer(outColor, dirtUV, dirtLevel, IN.color0.b, IN.color0.a, dirtSpecularMult);
#endif

	return PackHdr(outColor);
}// end of PS_VehicleTexturedUnlit()...
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if MATERIAL_DIFFUSE_COLOR
float3 vehicleComputeMaterialDiffuseColor(float4 cpv, float3 worldNormal, float3 worldEyeDir) {
	float3 result = matDiffuseColor;

#if PAINT_RAMP
	if(matIsDiffuseRampEnabled) {
		float dotNV = saturate(dot(worldNormal, worldEyeDir));
		result = tex2D(DiffuseRampTextureSampler, float2(dotNV, 0)).rgb;
	}
#endif

	result = ProcessMatDiffuseColor(result, cpv);
	return result;
}
#endif

//
//
//
//
StandardLightingProperties vehicleDeriveLightingPropertiesForCommonSurface(inout SurfaceProperties surfaceInfo,
										bool bApplySnow, float snowParams, float2 snowDiffuseUV
									#if MATERIAL_DIFFUSE_COLOR
										, float3 matColD
									#endif
										)
{
StandardLightingProperties OUT=(StandardLightingProperties)0;
	
	// Diffuse.
	OUT.diffuseColor = surfaceInfo.surface_diffuseColor;

#if MATERIAL_DIFFUSE_COLOR
	// Add in the material base colour (for vehicle to vehicle color variation).
	OUT.diffuseColor.rgb *= matColD;
#endif

#if VEHICLESHADER_SNOW && VEHICLESHADER_SNOW_BEFORE_DIFFUSE2
	if(bApplySnow)
	{
		OUT.diffuseColor.rgba = PS_ApplySnow(OUT.diffuseColor.rgba, snowParams, snowDiffuseUV);
	}
#endif	//VEHICLESHADER_SNOW...

#if DIFFUSE2 || DIFFUSE3
	// Overlay colour contribution.
	surfaceInfo.surface_overlayColor *= float4(matDiffuseColor2);
	float alphaDiff2 = surfaceInfo.surface_overlayColor.a;
	OUT.diffuseColor.rgb = lerp(OUT.diffuseColor.rgb, surfaceInfo.surface_overlayColor.rgb, alphaDiff2);
	#if NORMAL_MAP2
		surfaceInfo.surface_worldNormal = lerp(surfaceInfo.surface_worldNormal.xyz, surfaceInfo.surface_worldNormal2.xyz, alphaDiff2);
	#endif

	#if MATERIAL_DIFFUSE_COLOR && DIFFUSE3
		// vehicle_paint3_lvr: modulate livery2 texture layer with basic diffuse color only when alpha==1.0:
		OUT.diffuseColor.rgb = lerp(OUT.diffuseColor.rgb, OUT.diffuseColor.rgb*matColD, GtaStep(alphaDiff2, 1.0));
	#endif
#endif	// DIFFUSE2

#if EMISSIVE
	// Emissive- self illumination.
	#if DIFFUSE_COLOR_TINT
		// surface_baseColor.a is max opaqueness, not CPV alpha
	#else
		OUT.diffuseColor.a	*=surfaceInfo.surface_baseColor.a;
	#endif
#else	// !EMISSIVE
	// Factor in material color
	#if DIFFUSE_COLOR_TINT
		// surface_baseColor.a is max opaqueness, not CPV alpha
		OUT.diffuseColor.rgb*=surfaceInfo.surface_baseColor.rrr;
	#else
		OUT.diffuseColor	*=surfaceInfo.surface_baseColor.rrra;
	#endif
#endif	// EMISSIVE

#if EMISSIVE
	OUT.naturalAmbientScale	= gNaturalAmbientScale;
	OUT.artificialAmbientScale	= gArtificialAmbientScale;
#else
	OUT.naturalAmbientScale	= surfaceInfo.surface_baseColor.r * gNaturalAmbientScale;
	OUT.artificialAmbientScale	= surfaceInfo.surface_baseColor.r * gArtificialAmbientScale;
#endif

	OUT.artificialAmbientScale *= ArtificialBakeAdjustMult();

#if EMISSIVE
	OUT.emissiveIntensity = surfaceInfo.surface_baseColor.r * emissiveMultiplier * dirtOrBurnout;
#else
	OUT.emissiveIntensity = 0.0f;
#endif

#if VEHICLESHADER_SNOW && !VEHICLESHADER_SNOW_BEFORE_DIFFUSE2
	if(bApplySnow)
	{
		OUT.diffuseColor.rgba = PS_ApplySnow(OUT.diffuseColor.rgba, snowParams, snowDiffuseUV);
	}
#endif //VEHICLESHADER_SNOW...

	
#if SPECULAR
	OUT.specularSkin = 0.0f;
	OUT.fresnel = surfaceInfo.surface_fresnel;

	#if IGNORE_SPEC_MAP_VALUES
		#if VEHICLESHADER_SNOW
			// snow: blend towards values from specMap
			const float snowAmount = snowParams;
			OUT.specularIntensity = lerp(specularIntensityMult, surfaceInfo.surface_specularIntensity, snowAmount);
			OUT.specularExponent  = lerp(specularFalloffMult, surfaceInfo.surface_specularExponent, snowAmount);
		#else
			// SpecMap is present, but not really contributing to basic Specular (only Specular2 is modulated):
			OUT.specularIntensity = specularIntensityMult;
			OUT.specularExponent  = specularFalloffMult;
		#endif
	#else
		OUT.specularIntensity = surfaceInfo.surface_specularIntensity;
		OUT.specularExponent  = surfaceInfo.surface_specularExponent;	
	#endif

	#ifdef DIFFUSE2_IGNORE_SPEC_MODULATION
		// do nothing
	#else
		#if DIFFUSE2
			#if DIFFUSE2_SPEC_MASK
				OUT.specularIntensity *= max(invAlpha, diffuse2SpecClamp);
			#else
				OUT.specularIntensity *= (1.0-invAlpha) * diffuse2SpecMod;
			#endif
		#endif
	#endif

	#ifdef DIFFUSE3_IGNORE_SPEC_MODULATION
		// do nothing
	#else
		#if DIFFUSE3
			#if DIFFUSE3_SPEC_MASK
				OUT.specularIntensity *= max(invAlpha, diffuse3SpecClamp);
			#else
				OUT.specularIntensity *= (1.0-invAlpha) * diffuse3SpecMod;
			#endif
		#endif
	#endif

	#if DIMMER
		// do nothing
	#else
		OUT.specularIntensity *= surfaceInfo.surface_baseColor.r;
	#endif
#endif	// SPECULAR


#if REFLECT
	// Reflection - environment reflections.
	// Add in reflection contribution
	OUT.reflectionColor = surfaceInfo.surface_reflectionColor;
#endif	// REFLECT

#if SELF_SHADOW
	OUT.selfShadow = 1.0f;
#endif // SELF_SHADOW

	OUT.inInterior = gInInterior;

	return(OUT);
}



//
//
// calculates specular color factor for 1 single light;
// used for second specular layer;
//
#if SECOND_SPECULAR_LAYER
float3 GetSingleDirectionalLightSpecular2Color(
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

float3 GetSpecular2Color(
	float3	surfaceNormal,
	float3	lightDir
	)
{
#if SECOND_SPECULAR_LAYER_LOCKCOLOR
	float3 OUT = float3(1,1,1);
#else
	float3 OUT = specular2Color;
#if PAINT_RAMP
	if(matIsSpecularRampEnabled) {
		float dotNL = saturate(dot(surfaceNormal, -lightDir));
		OUT = tex2D(SpecularRampTextureSampler, float2(dotNL, 0)).rgb;
	}
#endif
#endif
	return(OUT);
}
#endif //SECOND_SPECULAR_LAYER...

//
//
//
//
#if __XENON
[maxtempreg(28)]
#endif
half4 VehicleTextured(vehicleVertexOutput IN, int numLights, bool directionalLightAndShadow, bool bUseCapsule, bool bUseHighQuality, bool bUseWaterRefractionFog)
{
#if defined(VEHICLE_GLASS_CRACK_TEXTURE)
	// debugging
	//return PackColor(float4((IN.worldNormal + 1)/2, 1));
	//return PackColor(float4(frac(IN.texCoord*16), 0, 1));
	//return PackColor(float4(frac(IN.texCoord3*16), 0, 1));
	
	// Keep the normal facing the camera
	// Using the camera direction doesn't work because of perspective
	//float flipNorm = sign(dot(IN.worldNormal, gViewInverse[2].xyz));
	float3 toCamera = gViewInverse[3].xyz - IN.worldPos.xyz;
	float flipNorm = sign(dot(IN.worldNormal.xyz, toCamera));
	IN.worldNormal *= flipNorm;
#endif // defined(VEHICLE_GLASS_CRACK_TEXTURE)

#if __WIN32PC && __SHADERMODEL < 40
	// easy "fix" for "out-of-registers" dx9 compiler error
	// (happens when VEHCONST_* consts became variables again):
	vehicleVertexOutputUnlit unlitIN;
	unlitIN.pos				= IN.pos;
	unlitIN.texCoord		= IN.texCoord;
	unlitIN.color0			= IN.color0;
	#if DIRT_UV
		unlitIN.texCoord3.xy= IN.texCoord3.xy;
	#endif
	return PS_VehicleTexturedUnlit(unlitIN);
#else //__WIN32PC

#if DIRT_LAYER
	#if DIRT_UV
		float2 dirtUV = DIRT_PS_INPUT;
	#else
		float2 dirtUV = DIFFUSE_PS_INPUT;
	#endif
#endif

#if FORWARD_VEHICLE_DIRT_LAYER || DIRT_LAYER
#ifdef VEHICLE_GLASS_CRACK_TEXTURE
	const float dirtLevel_ = flipNorm < 0.0f ? (min(dirtLevelMod.x, DIRT_LAYER_LEVEL_LIMIT)) : dirtLevelMod.x;
#else
	const float dirtLevel_ = dirtLevel;
#endif
#else
	const float dirtLevel_ = 0;
#endif

SurfaceProperties surfaceInfo = GetSurfaceProperties(
		PS_GetBaseColorFromCPV(IN.color0),
#if DIFFUSE_TEXTURE
		(float2)DIFFUSE_PS_INPUT * DiffuseTexTileUV,
		DiffuseSampler, 
#endif
#if DIFFUSE2
		(float2)DIFFUSE2_PS_INPUT,
		DiffuseSampler2,
#elif DIFFUSE3
		(float2)DIFFUSE3_PS_INPUT,
		DiffuseSampler3,
#endif
#if SPEC_MAP
		(float2)SPECULAR_PS_INPUT*specTexTileUV,
		SpecSampler,
#endif	// SPEC_MAP
#if REFLECT
		(float3)IN.worldEyePos,
		ReflectionSampler,
	#if REFLECT_MIRROR
		float2(0,0),
		float4(0,0,0,1), // not supported
	#endif // REFLECT_MIRROR
#endif	// REFLECT
#if NORMAL_MAP
		(float2)BUMP_PS_INPUT,
		BumpSampler, 
		(float3)IN.worldTangent,
		(float3)IN.worldBinormal,
#endif	// NORMAL_MAP
#if NORMAL_MAP2
		(float2)BUMP2_PS_INPUT,
		BumpSampler2, 
		(float3)IN.worldTangent,
		(float3)IN.worldBinormal,
#endif	// NORMAL_MAP2
#if DIRT_NORMALMAP
		float3(dirtUV.x, dirtUV.y, dirtLevel_),
		DirtBumpSampler,
#endif//DIRT_NORMALMAP...
#if DETAIL_UV
		(float2)DETAIL_PS_INPUT,
#endif
		(float3)IN.worldNormal
		);

#if 0
	// grid for debugging
	{
		float2 g = frac(IN.texCoord3*256);
		g = max(g, 1 - g);
		g.x = max(g.x, g.y);
		surfaceInfo.surface_diffuseColor.xyz += 2*pow(g.x, 64);
	}
#endif

	float crackTintScale = 1.0;
#if defined(VEHICLE_GLASS_CRACK_TEXTURE)

	float2 crackUV = IN.texCoord3*vehglassCrackTextureParams_scale;

	#if NORMAL_MAP || NORMAL_MAP2
		const float3 worldTangent  = IN.worldTangent * flipNorm;
		const float3 worldBinormal = IN.worldBinormal * flipNorm;
	#else
		float3 dp1 = ddx(IN.worldPos.xyz);
		float3 dp2 = ddy(IN.worldPos.xyz);
		float2 duv1 = ddx(crackUV.xy);
		float2 duv2 = ddy(crackUV.xy);
		
		float2x3 M = float2x3( dp1, dp2 );
		float3 worldT = mul( float2( duv1.x, duv2.x ), M );
		float3 worldB = mul( float2( duv1.y, duv2.y ), M );
		float3 worldTangent = worldT / max(length(worldT), 1e-6);
		float3 worldBinormal = worldB / max(length(worldB), 1e-6);
	#endif

	if (vehglassCrackTextureParams_amount > 0)
	{
		const float4 crackTexture = tex2D(vehglassCrackTextureSampler, crackUV);
		const float  crackTextureOpacity = crackTexture.w;
		
		surfaceInfo.surface_diffuseColor     = lerp(surfaceInfo.surface_diffuseColor, float4(1,1,1,1), vehglassCrackTextureParams_amount*crackTextureOpacity);
		surfaceInfo.surface_diffuseColor.xyz = lerp(surfaceInfo.surface_diffuseColor.xyz, vehglassCrackTextureParams_tint.xyz, vehglassCrackTextureParams_tint.w*crackTextureOpacity);

		const float3 crackTextureNormal = CalculateWorldNormal(crackTexture.xy, vehglassCrackTextureParams_bumpiness, worldTangent, worldBinormal, IN.worldNormal);
		const float  crackTextureNormalAmount = vehglassCrackTextureParams_bumpamount*crackTextureOpacity;

		surfaceInfo.surface_worldNormal = normalize(crackTextureNormal*crackTextureNormalAmount + surfaceInfo.surface_worldNormal);

		crackTintScale = 1.0 - crackTexture.w; // This will separate the glass tint from the crack tint
	}

#endif // defined(VEHICLE_GLASS_CRACK_TEXTURE)

#if DIFFUSE_COLOR_TINT
	surfaceInfo.surface_diffuseColor.rgb	*= lerp(float3(1,1,1), matDiffuseColorTint.rgb, crackTintScale * surfaceInfo.surface_baseColor.a);	// amount of RGB tint controlled by cpv.a
	surfaceInfo.surface_diffuseColor.a		+= matDiffuseColorTint.a*surfaceInfo.surface_baseColor.a;							// Alpha add controlled by cpv.a
	surfaceInfo.surface_diffuseColor.a		= saturate(surfaceInfo.surface_diffuseColor.a); // On PS3 if this value goes above 1 the destination value becomes negative (B*956891)
#endif

#if SECOND_SPECULAR_LAYER || MATERIAL_DIFFUSE_COLOR
	float3 eyeDirection = normalize(IN.worldEyePos);
#endif

#if MATERIAL_DIFFUSE_COLOR
	float3 matColD = vehicleComputeMaterialDiffuseColor(IN.color0, surfaceInfo.surface_worldNormal, eyeDirection);
#endif

	// Compute some surface information.
	StandardLightingProperties surfaceStandardLightingProperties =
		vehicleDeriveLightingPropertiesForCommonSurface( surfaceInfo,
													#if VEHICLESHADER_SNOW
														true, SNOW_PS_INPUTD,
														(float2)SNOW_PS_INPUT
													#else
														false, 0, float2(0,0)
													#endif
													#if MATERIAL_DIFFUSE_COLOR
														,matColD
													#endif
														);

#ifndef ALPHA_SHADER
	{
		//ignore alpha channel for non alpha shader using the forward path
		surfaceStandardLightingProperties.diffuseColor = float4(surfaceStandardLightingProperties.diffuseColor.rgb, 1.0f);
	}
#endif

#if defined(USE_VEHICLE_DAMAGE) || defined(USE_VEHICLE_DAMAGE_EDGE)
	surfaceStandardLightingProperties.specularIntensity *= IN.worldNormal.w; // scale down specInt/reflection by damage amount
#endif

#if DIRT_LAYER
	float dirtSpecularMult;
	#if DIRT_ON_TOP
		CalculateDirtLayer(surfaceStandardLightingProperties.diffuseColor, dirtUV, dirtLevel_, IN.color0.b, IN.color0.a, dirtSpecularMult);
	#else
		surfaceStandardLightingProperties.diffuseColor = CalculateDirtLayer(surfaceStandardLightingProperties.diffuseColor, dirtUV, dirtLevel_, IN.color0.b, IN.color0.a, dirtSpecularMult);
	#endif
	#if SPECULAR
		surfaceStandardLightingProperties.specularIntensity *= dirtSpecularMult;
	#endif
#else
	#if SPECULAR
		surfaceStandardLightingProperties.specularIntensity *= nonDirtSpecIntScale;
	#endif
#endif

#if SECOND_SPECULAR_LAYER
		if(specular2ColorIntensity > 0.0f)
		{
			float _spec2Factor		= specular2Factor;
			float _spec2Intensity	= specular2ColorIntensity * secondSpecIntScale;
#if DIMMER
			// do nothing
#else
			_spec2Intensity		*= surfaceInfo.surface_baseColor.r;
#endif
#if SECOND_SPECULAR_LAYER_NOFRESNEL
			float _spec2Fresnel	= 1.0f;
#else
			// note: surfaceInfo.surface_fresnel already contains correctly rescaled fresnel
			float _spec2Fresnel	= surfaceInfo.surface_fresnel;
#endif

#if SPEC_MAP
#if SPEC_MAP_INTFALLOFFFRESNEL_PACK
			float _specSampInt		= surfaceInfo.surface_specMapSample.x*_spec2Fresnel;
			float _specSampFalloff	= surfaceInfo.surface_specMapSample.y;
#else
			float _specSampInt		= dot(surfaceInfo.surface_specMapSample.xyz,specMapIntMask)*_spec2Fresnel;
			float _specSampFalloff	= surfaceInfo.surface_specMapSample.w;
#endif
			_spec2Intensity				*= _specSampInt;
			_spec2Factor				*= _specSampFalloff;
#endif //SPEC_MAP...

			float3 _spec2LightDir = lerp(gDirectionalLight.xyz, float3(0,0,-1), secondSpecDirLerp);
			float3 _spec2Color = GetSpecular2Color(surfaceInfo.surface_worldNormal, _spec2LightDir);

			float3 secondSpecular = GetSingleDirectionalLightSpecular2Color(
				surfaceInfo.surface_worldNormal,
#if SPECULAR
				eyeDirection,								// eyeDirection
				_spec2Factor,								// float	surfaceSpecularExponent,
#endif
				_spec2LightDir,
				float4(_spec2Color*_spec2Intensity,1)		//gLightColor[0]			// lightColorIntensity
				);
			surfaceStandardLightingProperties.diffuseColor.xyz += secondSpecular;
		}//if(specular2ColorIntensity > 0.0f)...
#endif //SECOND_SPECULAR_LAYER...

// BS#1740042: clamp the vehicle paint shader to not exceed 240 240 240 in the gbuffer
// so that white cars dont go pure white and bloom too much due to second spec
#if defined(VEHICLE_PAINT_SHADER)
		surfaceStandardLightingProperties.diffuseColor.rgb = min(surfaceStandardLightingProperties.diffuseColor.rgb, float3(240,240,240));
#endif

//-----------------------------------------------------------------------
// Determine the light striking the surface and heading toward
// the eye from this location.

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

	float4 OUT;
#if __XENON
	if(numLights > 4)
	{
		[isolate]
		OUT = calculateForwardLighting(
			numLights,
			bUseCapsule,
			surface,
			material,
			light,
			directionalLightAndShadow,
				SHADOW_RECEIVING,
				bUseHighQuality, // directionalShadowHighQuality
			IN.pos.xy * gooScreenSize.xy);
	}
	else
#endif
	{
		OUT = calculateForwardLighting(
			numLights,
			bUseCapsule,
			surface,
			material,
			light,
			directionalLightAndShadow,
				SHADOW_RECEIVING,
				bUseHighQuality, // directionalShadowHighQuality
			IN.pos.xy * gooScreenSize.xy);
	}
	
#if USE_GLOW
	OUT.rgb = OUT.rgb + (glowColor * intensity) * DiskBrakeGlow; // Add emissive glow 
#endif // USE_GLOW 

#if DIRT_LAYER && DIRT_ON_TOP
	// apply dirt layer as last layer on top of everything:
	OUT.rgb = CalculateDirtLayer( float4(OUT.rgb,1), dirtUV, dirtLevel_, IN.color0.b, IN.color0.a, dirtSpecularMult).rgb;
#endif

#if defined(USE_FOG) && !defined(USE_NO_FOG) && (__SHADERMODEL >= 40)
	if(bUseWaterRefractionFog)
	{
		const float waterFogOpacity = pow(WaterParamsTexture.Load(int3(IN.pos.xy, 0)).x, 2);
		const float linearDepth = dot(IN.worldEyePos, gViewInverse[2].xyz);// getLinearGBufferDepth(IN.pos.z, gVehicleWaterProjParams);
		float waterSurfaceLinearDepth = getLinearGBufferDepth(WaterDepthTexture.Load(int3(IN.pos.xy, 0)).r, gVehicleWaterProjParams);
		const float waterDepthDiff = linearDepth - waterSurfaceLinearDepth;
		//Using rageDiscard here doesnt compile on Durango, so just returning all zeros
		if(waterDepthDiff<=0.0f)
		{
			return float4(0,0,0,0);
		}
		half2 depthBlend = saturate(GetWaterDepthBlend(waterDepthDiff, waterFogOpacity, 0));
		OUT.a *= saturate(depthBlend.x);
	}
	else
	{
		float4 OUTwithoutFog = OUT;
#if defined(USE_VERTEX_FOG) 
#if defined(USE_FOGRAY_FORWARDPASS) && !__LOW_QUALITY
		IN.fogData.w *= GetFogRayIntensity(IN.pos.xy * gooScreenSize.xy);
#endif 
		OUT.xyz = lerp(OUT.xyz, IN.fogData.xyz, IN.fogData.w);
#elif defined(USE_VERTEX_FOG_ALPHA)
		OUT.w = OUT.w * IN.worldPos.w;
#else
		OUT = calcDepthFxBasic(OUT, IN.worldPos.xyz,IN.pos.xy * gooScreenSize.xy);
#endif
	}
#endif // defined(USE_FOG) && !defined(USE_NO_FOG)  && (__SHADERMODEL >= 40)...

#if 0 && defined(VEHICLE_GLASS_CRACK_TEXTURE)
	// debugging
	{
		const float4 dirtUV_debug = float4(frac(IN.texCoord3*32), 0, 1);
		OUT = lerp(OUT, dirtUV_debug, 0.0001);
		//OUT += pow(max(dirtUV_debug.x, dirtUV_debug.y), 128);
	}
#endif

#ifdef ALPHA_SHADER
	OUT.a = saturate(OUT.a);
#endif

	return OUT;
#endif //__WIN32PC && __SHADERMODEL < 40
}// end of VehicleTextured()...


//
//
//
//
half4 VehicleTexturedWaterReflection(vehicleVertexOutputWaterReflection IN)
{
#if WATER_REFLECTION_CLIP_IN_PIXEL_SHADER
	rageDiscard(dot(float4(IN.worldPos.xyz, 1), gLight0PosX) < 0);
#endif // WATER_REFLECTION_CLIP_IN_PIXEL_SHADER

#if DIRT_LAYER
	#if DIRT_UV
		float2 dirtUV = DIRT_PS_INPUT;
	#else
		float2 dirtUV = DIFFUSE_PS_INPUT;
	#endif
#endif

	SurfaceProperties surfaceInfo = GetSurfacePropertiesBasic(
		IN.color0, 
#if DIFFUSE_TEXTURE
		DIFFUSE_PS_INPUT,
		DiffuseSampler, 
#endif
		IN.worldNormal
		);

#if MATERIAL_DIFFUSE_COLOR
	float3 eyeDir = normalize(IN.worldEyePos);
	float3 matColD = vehicleComputeMaterialDiffuseColor(IN.color0, surfaceInfo.surface_worldNormal, eyeDir);
#endif

	// Compute some surface information.
	StandardLightingProperties surfaceStandardLightingProperties =
		vehicleDeriveLightingPropertiesForCommonSurface(surfaceInfo, false, 0, float2(0,0)
		                                                #if MATERIAL_DIFFUSE_COLOR
		                                                	,matColD
		                                                #endif
		                                                );

	surfaceStandardLightingProperties.diffuseColor = ProcessDiffuseColor(surfaceStandardLightingProperties.diffuseColor);

#if DIRT_LAYER
	float dirtSpecularMult;
	surfaceStandardLightingProperties.diffuseColor = CalculateDirtLayer(surfaceStandardLightingProperties.diffuseColor, dirtUV, dirtLevel, IN.color0.b, IN.color0.a, dirtSpecularMult);
	#if SPECULAR
		surfaceStandardLightingProperties.specularIntensity *= dirtSpecularMult;
	#endif
#else
	#if SPECULAR
		surfaceStandardLightingProperties.specularIntensity *= nonDirtSpecIntScale;
	#endif
#endif

	//-----------------------------------------------------------------------
	// Determine the light striking the surface and heading toward
	// the eye from this location.

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

	float4 OUT = calculateForwardLighting_WaterReflection(
		surface,
		material,
		light,
		1, // ambient scale
		1, // directional scale
		SHADOW_RECEIVING);

	// don't bother fogging vehicles in water reflection, they fade out after 50 metres
	//OUT = calcDepthFxBasic(OUT, IN.worldPos.xyz);

	return OUT;
}// end of VehicleTexturedWaterReflection()...


half4 PS_VehicleTextured_Zero(vehicleVertexOutput IN): COLOR
{
	return PackHdr(VehicleTextured(IN, 0, USE_DIRECTIONAL_LIGHT_FWD_0_LIGHTS, false, false, false));
}

half4 PS_VehicleTextured_Four(vehicleVertexOutput IN): COLOR
{
	return PackHdr(VehicleTextured(IN, 4, USE_DIRECTIONAL_LIGHT_FWD_4_OR_8_LIGHTS, true, false, false));
}

half4 PS_VehicleTextured_Eight(vehicleVertexOutput IN): COLOR
{
	return PackHdr(VehicleTextured(IN, 8, USE_DIRECTIONAL_LIGHT_FWD_4_OR_8_LIGHTS, true, false, false));
}

half4 PS_VehicleTextured_Zero_WaterRefraction(vehicleVertexOutput IN): COLOR
{
	return PackHdr(VehicleTextured(IN, 0, USE_DIRECTIONAL_LIGHT_FWD_0_LIGHTS, false, false, true));
}

half4 PS_VehicleTextured_Four_WaterRefraction(vehicleVertexOutput IN): COLOR
{
	return PackHdr(VehicleTextured(IN, 4, USE_DIRECTIONAL_LIGHT_FWD_4_OR_8_LIGHTS, true, false, true));
}

half4 PS_VehicleTextured_Eight_WaterRefraction(vehicleVertexOutput IN): COLOR
{
	return PackHdr(VehicleTextured(IN, 8, USE_DIRECTIONAL_LIGHT_FWD_4_OR_8_LIGHTS, true, false, true));
}

#if APPLY_DOF_TO_ALPHA
OutHalf4Color_DOF PS_VehicleTextured_ZeroDOF(vehicleVertexOutputDOF IN)
{
	OutHalf4Color_DOF OUT;
	OUT.col0 = PS_VehicleTextured_Zero(IN.vertOutput);
	float dofOutput = OUT.col0.a > DOF_ALPHA_LIMIT ? 1.0 : 0.0;
	OUT.dof.depth = IN.linearDepth * dofOutput;
	OUT.dof.alpha = dofOutput;
	return OUT;
}

OutHalf4Color_DOF PS_VehicleTextured_FourDOF(vehicleVertexOutputDOF IN)
{
	OutHalf4Color_DOF OUT;
	OUT.col0 = PS_VehicleTextured_Four(IN.vertOutput);
	float dofOutput = OUT.col0.a > DOF_ALPHA_LIMIT ? 1.0 : 0.0;
	OUT.dof.depth = IN.linearDepth * dofOutput;
	OUT.dof.alpha = dofOutput;
	return OUT;
}

OutHalf4Color_DOF PS_VehicleTextured_EightDOF(vehicleVertexOutputDOF IN)
{
	OutHalf4Color_DOF OUT;
	OUT.col0 = PS_VehicleTextured_Eight(IN.vertOutput);
	float dofOutput = OUT.col0.a > DOF_ALPHA_LIMIT ? 1.0 : 0.0;
	OUT.dof.depth = IN.linearDepth * dofOutput;
	OUT.dof.alpha = dofOutput;
	return OUT;
}

OutHalf4Color_DOF PS_VehicleTextured_ZeroDOF_WaterRefraction(vehicleVertexOutputDOF IN)
{
	OutHalf4Color_DOF OUT;
	OUT.col0 = PS_VehicleTextured_Zero_WaterRefraction(IN.vertOutput);
	float dofOutput = OUT.col0.a > DOF_ALPHA_LIMIT ? 1.0 : 0.0;
	OUT.dof.depth = IN.linearDepth * dofOutput;
	OUT.dof.alpha = dofOutput;
	return OUT;
}

OutHalf4Color_DOF PS_VehicleTextured_FourDOF_WaterRefraction(vehicleVertexOutputDOF IN)
{
	OutHalf4Color_DOF OUT;
	OUT.col0 = PS_VehicleTextured_Four_WaterRefraction(IN.vertOutput);
	float dofOutput = OUT.col0.a > DOF_ALPHA_LIMIT ? 1.0 : 0.0;
	OUT.dof.depth = IN.linearDepth * dofOutput;
	OUT.dof.alpha = dofOutput;
	return OUT;
}

OutHalf4Color_DOF PS_VehicleTextured_EightDOF_WaterRefraction(vehicleVertexOutputDOF IN)
{
	OutHalf4Color_DOF OUT;
	OUT.col0 = PS_VehicleTextured_Eight_WaterRefraction(IN.vertOutput);
	float dofOutput = OUT.col0.a > DOF_ALPHA_LIMIT ? 1.0 : 0.0;
	OUT.dof.depth = IN.linearDepth * dofOutput;
	OUT.dof.alpha = dofOutput;
	return OUT;
}
#endif // APPLY_DOF_TO_ALPHA



#if HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED
#if HIGHQUALITY_FORWARD_TECHNIQUES

half4 PS_VehicleTextured_ZeroHighQuality(vehicleVertexOutput IN): COLOR
{
	return PackHdr(VehicleTextured(IN, 0, USE_DIRECTIONAL_LIGHT_FWD_0_LIGHTS, false, HIGHQUALITY_FORWARD_TECHNIQUES, false));
}

half4 PS_VehicleTextured_FourHighQuality(vehicleVertexOutput IN): COLOR
{
	return PackHdr(VehicleTextured(IN, 4, USE_DIRECTIONAL_LIGHT_FWD_4_OR_8_LIGHTS, true, HIGHQUALITY_FORWARD_TECHNIQUES, false));
}

half4 PS_VehicleTextured_EightHighQuality(vehicleVertexOutput IN): COLOR
{
	return PackHdr(VehicleTextured(IN, 8, USE_DIRECTIONAL_LIGHT_FWD_4_OR_8_LIGHTS, true, HIGHQUALITY_FORWARD_TECHNIQUES, false));
}

half4 PS_VehicleTextured_ZeroHighQuality_WaterRefraction(vehicleVertexOutput IN): COLOR
{
	return PackHdr(VehicleTextured(IN, 0, USE_DIRECTIONAL_LIGHT_FWD_0_LIGHTS, false, HIGHQUALITY_FORWARD_TECHNIQUES, true));
}

half4 PS_VehicleTextured_FourHighQuality_WaterRefraction(vehicleVertexOutput IN): COLOR
{
	return PackHdr(VehicleTextured(IN, 4, USE_DIRECTIONAL_LIGHT_FWD_4_OR_8_LIGHTS, true, HIGHQUALITY_FORWARD_TECHNIQUES, true));
}

half4 PS_VehicleTextured_EightHighQuality_WaterRefraction(vehicleVertexOutput IN): COLOR
{
	return PackHdr(VehicleTextured(IN, 8, USE_DIRECTIONAL_LIGHT_FWD_4_OR_8_LIGHTS, true, HIGHQUALITY_FORWARD_TECHNIQUES, true));
}

#else

#define PS_VehicleTextured_ZeroHighQuality PS_VehicleTextured_Zero
#define PS_VehicleTextured_FourHighQuality PS_VehicleTextured_Four
#define PS_VehicleTextured_EightHighQuality PS_VehicleTextured_Eight

#define PS_VehicleTextured_ZeroHighQuality_WaterRefraction PS_VehicleTextured_Zero_WaterRefraction
#define PS_VehicleTextured_FourHighQuality_WaterRefraction PS_VehicleTextured_Four_WaterRefraction
#define PS_VehicleTextured_EightHighQuality_WaterRefraction PS_VehicleTextured_Eight_WaterRefraction

#endif
#endif // HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED

half4 PS_VehicleTexturedWaterReflection(vehicleVertexOutputWaterReflection IN): COLOR
{
	return PackReflection(VehicleTexturedWaterReflection(IN));
}

#if __MAX
//
//
//
//
float4 MaxVehicleTextured( vehicleVertexOutput IN )
{
float4 OUT;

#if DIRT_LAYER
	#if DIRT_UV
		float2 dirtUV = DIRT_PS_INPUT;
	#else
		float2 dirtUV = DIFFUSE_PS_INPUT;
	#endif
#endif

SurfaceProperties surfaceInfo = GetSurfaceProperties(
			PS_GetBaseColorFromCPV(IN.color0),
	#if DIFFUSE_TEXTURE
			(float2)DIFFUSE_PS_INPUT * DiffuseTexTileUV,
			DiffuseSampler, 
	#endif
	#if DIFFUSE2
			(float2)DIFFUSE2_PS_INPUT,
			DiffuseSampler2,
	#elif DIFFUSE3
			(float2)DIFFUSE3_PS_INPUT,
			DiffuseSampler3,
	#endif
	#if SPEC_MAP
			(float2)SPECULAR_PS_INPUT*specTexTileUV,
			SpecSampler,
	#endif	// SPEC_MAP
	#if REFLECT
			IN.worldEyePos,
			ReflectionSampler,
		#if REFLECT_MIRROR
			float2(0,0),
			float4(0,0,0,1), // not supported
		#endif // REFLECT_MIRROR
	#endif	// REFLECT
	#if NORMAL_MAP
			(float2)BUMP_PS_INPUT,
			BumpSampler, 
			(float3)IN.worldTangent,
			(float3)IN.worldBinormal,
		#if PARALLAX
			(float3)IN.tanEyePos,
		#endif
	#endif	// NORMAL_MAP
	#if NORMAL_MAP2
			(float2)BUMP2_PS_INPUT,
			BumpSampler2, 
			(float3)IN.worldTangent,
			(float3)IN.worldBinormal,
	#endif	// NORMAL_MAP2
	#if DIRT_NORMALMAP
			float3(dirtUV.x, dirtUV.y, dirtLevel),
			DirtBumpSampler,
	#endif//DIRT_NORMALMAP...
	#if DETAIL_UV
		(float2)DETAIL_PS_INPUT,
	#endif
		(float3)IN.worldNormal
		);

	if(!useAlphaFromDiffuse)
	{
		//surfaceInfo.surface_diffuseColor.a = tex2D(DiffuseSamplerAlpha,	IN.texCoord.xy).r;
		surfaceInfo.surface_diffuseColor.a = 1.0f;	// fix for outsourcing
	}
	
	surfaceInfo.surface_diffuseColor.a = tex2D(DiffuseSamplerAlpha,	DIFFUSE_PS_INPUT).r;

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

	// Set MAX colours
//	gLightNaturalAmbient0.rgb = maxLightAmbientUpColor * (maxAmbLightingToggle) ? 1.0f : 0.00001f;
//	gLightNaturalAmbient1.rgb = maxLightAmbientDownColor * (maxAmbLightingToggle) ? 1.0f : 0.00001f;

	OUT = calculateForwardLighting(
		0,
		false,
		surface,
		material,
		light,
		true, // directional
			false, // directionalShadow
			false, // directionalShadowHighQuality
		float2(0,0));

	return float4(sqrt(OUT.rgb), OUT.a); //gamma correction

}// end of MaxVehicleTextured()...


//
//
//
//
float4 PS_MaxVehicleTextured( vehicleVertexOutput IN ) : COLOR
{
float4 OUT;

	if(maxLightingToggle)
	{
		OUT = MaxVehicleTextured(IN);
	}
	else
	{
		vehicleVertexOutputUnlit unlitIN;
			unlitIN.pos					= IN.pos;
			unlitIN.texCoord			= IN.texCoord;
			unlitIN.color0				= IN.color0;
			#if DIRT_UV
				unlitIN.texCoord3.xy	= IN.texCoord3.xy;
			#endif
		OUT = PS_VehicleTexturedUnlit(unlitIN);
	}

	return saturate(OUT);
}// end of PS_MaxPixelTextured()...
#endif //__MAX...

//
//
//
//
DeferredGBuffer DeferredVehicleTexturedCommon(vehicleSampleInputD IN, float fFacing)
{
DeferredGBuffer OUT = (DeferredGBuffer)0;
#if CLOTH
	#if __WIN32PC
		fFacing *= -1;  // On PC the faces are the wrong way round so need to flip fFacing
	#endif
	IN.worldNormal.xyz *= fFacing;
#endif

#if DIRT_LAYER
	#if DIRT_UV
		float2 dirtUV = DIRT_PS_INPUT;
	#else
		float2 dirtUV = DIFFUSE_PS_INPUT;
	#endif
#endif

	SurfaceProperties surfaceInfo = GetSurfaceProperties(
		PS_GetBaseColorFromCPV(IN.color0),
#if DIFFUSE_TEXTURE
		(float2)DIFFUSE_PS_INPUT * DiffuseTexTileUV,
		DiffuseSampler, 
#endif
#if DIFFUSE2
		(float2)DIFFUSE2_PS_INPUT,
		DiffuseSampler2,
#elif DIFFUSE3
		(float2)DIFFUSE3_PS_INPUT,
		DiffuseSampler3,
#endif
#if SPEC_MAP
		(float2)SPECULAR_PS_INPUT*specTexTileUV,
		SpecSampler,
#endif	// SPEC_MAP
#if REFLECT
		(float3)IN.worldEyePos,
		ReflectionSampler,
	#if REFLECT_MIRROR
		float2(0,0),
		float4(0,0,0,1), // not supported
	#endif // REFLECT_MIRROR
#endif	// REFLECT
#if NORMAL_MAP
		(float2)BUMP_PS_INPUT,
		BumpSampler, 
		(float3)IN.worldTangent,
		(float3)IN.worldBinormal,
#endif	// NORMAL_MAP
#if NORMAL_MAP2
		(float2)BUMP2_PS_INPUT,
		BumpSampler2, 
		(float3)IN.worldTangent,
		(float3)IN.worldBinormal,
#endif	// NORMAL_MAP2
#if DIRT_NORMALMAP
		float3(dirtUV.x, dirtUV.y, dirtLevel),
		DirtBumpSampler,
#endif//DIRT_NORMALMAP...
#if DETAIL_UV
		(float2)DETAIL_PS_INPUT,
#endif
		(float3)IN.worldNormal
		);

#if SECOND_SPECULAR_LAYER || MATERIAL_DIFFUSE_COLOR
	float3 eyeDirection = normalize(IN.worldEyePos);
#endif

#if MATERIAL_DIFFUSE_COLOR
	float3 matColD = vehicleComputeMaterialDiffuseColor(IN.color0, surfaceInfo.surface_worldNormal, eyeDirection);
#endif
    
	// Compute some surface information.								
	StandardLightingProperties surfaceStandardLightingProperties =
		vehicleDeriveLightingPropertiesForCommonSurface( surfaceInfo,
													#if VEHICLESHADER_SNOW
														true, SNOW_PS_INPUTD,
														(float2)SNOW_PS_INPUT
													#else
														false, 0, float2(0,0)
													#endif
													#if MATERIAL_DIFFUSE_COLOR
														,matColD
													#endif
														);

#if defined(USE_VEHICLE_DAMAGE) || defined(USE_VEHICLE_DAMAGE_EDGE)
	surfaceStandardLightingProperties.specularIntensity *= IN.worldNormal.w; // scale down specInt/reflection by damage amount
#endif

	// note: reflection term is ignored in deferred PS
#if 0 //REFLECT
	#if 0 //scale reflection to compensate for alpha		
		float refScale=1.0f/max(surfaceStandardLightingProperties.diffuseColor.w,0.01f);
		surfaceStandardLightingProperties.diffuseColor.xyz += surfaceStandardLightingProperties.reflectionColor*refScale;
	#else
		surfaceStandardLightingProperties.diffuseColor.xyz += surfaceStandardLightingProperties.reflectionColor;
	#endif
#endif	// REFLECT


#if DIRT_LAYER
	float dirtSpecularMult;
	surfaceStandardLightingProperties.diffuseColor = CalculateDirtLayer(surfaceStandardLightingProperties.diffuseColor, dirtUV, dirtLevel, IN.color0.b, IN.color0.a, dirtSpecularMult);
	#if SPECULAR
		surfaceStandardLightingProperties.specularIntensity *= dirtSpecularMult;
	#endif
#else
	#if SPECULAR
		surfaceStandardLightingProperties.specularIntensity *= nonDirtSpecIntScale;
	#endif
#endif

#if SECOND_SPECULAR_LAYER
	if(specular2ColorIntensity > 0.0f)
	{
		float _spec2Factor		= specular2Factor;
		float _spec2Intensity	= specular2ColorIntensity * secondSpecIntScale;
		#if DIMMER
			// do nothing
		#else
			_spec2Intensity		*= surfaceInfo.surface_baseColor.r;
		#endif
		#if SECOND_SPECULAR_LAYER_NOFRESNEL
			float _spec2Fresnel	= 1.0f;
		#else
			// note: surfaceInfo.surface_fresnel already contains correctly rescaled fresnel
			float _spec2Fresnel	= surfaceInfo.surface_fresnel;
		#endif

		#if SPEC_MAP
			#if SPEC_MAP_INTFALLOFFFRESNEL_PACK
				float _specSampInt		= surfaceInfo.surface_specMapSample.x*_spec2Fresnel;
				float _specSampFalloff	= surfaceInfo.surface_specMapSample.y;
			#else
				float _specSampInt		= dot(surfaceInfo.surface_specMapSample.xyz,specMapIntMask)*_spec2Fresnel;
				float _specSampFalloff	= surfaceInfo.surface_specMapSample.w;
			#endif
			_spec2Intensity				*= _specSampInt;
			_spec2Factor				*= _specSampFalloff;
		#endif //SPEC_MAP...

		float3 _spec2LightDir = lerp(gDirectionalLight.xyz, float3(0,0,-1), secondSpecDirLerp);
		float3 _spec2Color = GetSpecular2Color(surfaceInfo.surface_worldNormal, _spec2LightDir);

		float3 secondSpecular = GetSingleDirectionalLightSpecular2Color(
									surfaceInfo.surface_worldNormal,
								#if SPECULAR
									eyeDirection,								// eyeDirection
									_spec2Factor,								// float	surfaceSpecularExponent,
								#endif
									_spec2LightDir,
									float4(_spec2Color*_spec2Intensity,1)	//gLightColor[0]			// lightColorIntensity
									);

		surfaceStandardLightingProperties.diffuseColor.xyz += secondSpecular;
	}//if(specular2ColorIntensity > 0.0f)...
#endif //SECOND_SPECULAR_LAYER...


	// burnout vehicles lose their reflectivity props:
//	surfaceStandardLightingProperties.specularIntensity *= matDiffuseColor2.w;
//	surfaceStandardLightingProperties.specularExponent	*= matDiffuseColor2.w;



#ifdef VEHICLE_TYRE_DEBUG_MEASUREMENT
		float innerRadius = IN.tyreParams.x;	//0.150; //0.264;
		float innerRadius2 = innerRadius*innerRadius;
		float distTireYZ2 = IN.tyreParams.y*IN.tyreParams.y + IN.tyreParams.z*IN.tyreParams.z;	// use sqr()
		if(distTireYZ2 > innerRadius2)
		{	// mark tyre:
			surfaceStandardLightingProperties.diffuseColor = float4(1,0,0,1);
		}
#endif //VEHICLE_TYRE_DEBUG_MEASUREMENT...

	// BS#1740042: clamp the vehicle paint shader to not exceed 240 240 240 in the gbuffer
	// so that white cars dont go pure white and bloom too much due to second spec
	#if defined(VEHICLE_PAINT_SHADER)
		surfaceStandardLightingProperties.diffuseColor.rgb = min(surfaceStandardLightingProperties.diffuseColor.rgb, float3(240,240,240));
	#endif

	// Pack the information into the GBuffer(s).
	OUT = PackGBuffer(surfaceInfo.surface_worldNormal, surfaceStandardLightingProperties);

#if VEHICLE_SUPERSAMPLE
	//forcing the use of sampleIndex to trigger supersampling
	OUT.col3.a += 0.00001 * IN.sampleIndex;
#endif

#ifdef USE_VEHICLE_DAMAGE
	if (bDebugDisplayDamageScale || bDebugDisplayDamageMap)
	{
		OUT.col0 = IN.color0;
	}
#endif

	return OUT;
}

#if CLOTH
DeferredGBuffer PS_DeferredVehicleTextured(vehicleSampleInputD IN, DECLARE_FACING_INPUT(fFacing))
{
	DECLARE_FACING_FLOAT(fFacing)
	return DeferredVehicleTexturedCommon(IN, fFacing);
}
#else
DeferredGBuffer PS_DeferredVehicleTextured(vehicleSampleInputD IN)
{
	return DeferredVehicleTexturedCommon(IN, 1.0f);
}
#endif

//
// special version of PS_DeferredVehicleTextured() with extra alpha test applied
// to simulate missing AlphaTest/AlphaToMask while in MRT mode:
//
half4 PS_DeferredVehicleTextured_AlphaClip(vehicleSampleInputD IN) : SV_Target0
{
	// The function is written for the alpha fade on PC really. Can't see this being needed otherwise...
	// Also, the role of alphaclip would be undefined, so I've not bothered attempting to make this do anything.
#if defined(USE_ALPHACLIP_FADE) && !RSG_ORBIS
	FadeDiscard( IN.pos, globalFade );
#endif // USE_ALPHACLIP_FADE

	DeferredGBuffer OUT;
	OUT = DeferredVehicleTexturedCommon(IN, 1.0f);

	// PS3: there's no AlphaTest in MRT mode, so we simulate AlphaTest functionality here:
	rageDiscard(OUT.col0.a <= vehicle_alphaRef);

	return OUT.col0;
}// end of PS_DeferredVehicleTextured_AlphaClip()...

DeferredGBuffer PS_DeferredVehicleTextured_SubSampleAlpha(vehicleSampleInputD IN)
{
DeferredGBuffer OUT;
	OUT = DeferredVehicleTexturedCommon(IN, 1.0f);

	// PS3: there's no AlphaTest in MRT mode, so we simulate AlphaTest functionality here:
	rageDiscard(OUT.col0.a <= vehicle_alphaRef);

	return OUT;
}// end of PS_DeferredVehicleTextured_AlphaClip()...

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
#if 0
			// unlit:
			VertexShader= compile VERTEXSHADER	VS_MaxTransformUnlit();
			PixelShader	= compile PIXELSHADER	PS_VehicleTexturedUnlit();
#else
			// lit:
			VertexShader= compile VERTEXSHADER	VS_MaxTransform();
			PixelShader	= compile PIXELSHADER	PS_MaxVehicleTextured();
#endif
		}  
	}
#else //__MAX...

	#if NO_DEPTH_WRITE
		#define FORWARD_RENDERSTATES() \
			ZFunc			= FixedupLESS; \
			ZEnable			= true; \
			ZWriteEnable	= false;
		#define WATERREFRACTIONALPHA_RENDERSTATES()	\
			ZFunc			= FixedupLESS; \
			ZEnable			= true; \
			ZWriteEnable	= false; \
			AlphaBlendEnable	= true; \
			BlendOp				= ADD; \
			SrcBlend			= SRCALPHA; \
			DestBlend			= INVSRCALPHA; \
			ColorWriteEnable	= RED+GREEN+BLUE;
			
	#else
		#define FORWARD_RENDERSTATES()

		#define WATERREFRACTIONALPHA_RENDERSTATES()	\
			AlphaBlendEnable	= true; \
			BlendOp				= ADD; \
			SrcBlend			= SRCALPHA; \
			DestBlend			= INVSRCALPHA; \
			ColorWriteEnable	= RED+GREEN+BLUE;
	#endif


	#if defined(USE_BACKLIGHTING_HACK)
		#define DEFERRED_RENDERSTATES() \
			SetGlobalDeferredMaterial(DEFERRED_MATERIAL_TREE);
	#elif defined(VEHICLE_DECAL_SHADER)
		#define DEFERRED_RENDERSTATES() \
			SET_COLOR_WRITE_ENABLE(RED+GREEN+BLUE+ALPHA, RED+GREEN+BLUE+ALPHA, RED+GREEN+BLUE+ALPHA, 0)	\
			AlphaBlendEnable = true;	\
			BlendOp   = ADD;			\
			SrcBlend  = SRCALPHA;		\
			DestBlend = INVSRCALPHA;
	#else
		#define DEFERRED_RENDERSTATES()
	#endif

	#if 1
		#define CGC_FORWARD_LIGHTING_FLAGS "-unroll all --O3 -fastmath"
	#else
		#define CGC_FORWARD_LIGHTING_FLAGS CGC_DEFAULTFLAGS
	#endif

	// ===============================
	// Lit (forward) techniques
	// ===============================
	#if FORWARD_TECHNIQUES && NONSKINNED_VEHICLE_TECHNIQUES
	technique draw
	{
		pass p0
		{
			FORWARD_RENDERSTATES()
#if APPLY_DOF_TO_ALPHA
			VertexShader = compile VERTEXSHADER	VS_VehicleTransformDOF(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_VehicleTextured_EightDOF()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
#else // APPLY_DOF_TO_ALPHA
			VertexShader = compile VERTEXSHADER	VS_VehicleTransform(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_VehicleTextured_Eight()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
#endif // APPLY_DOF_TO_ALPHA
		}
	}
	#endif // FORWARD_TECHNIQUES && NONSKINNED_VEHICLE_TECHNIQUES

	#if FORWARD_TECHNIQUES && DRAWSKINNED_TECHNIQUES
	technique drawskinned
	{
		pass p0
		{
			FORWARD_RENDERSTATES()
#if APPLY_DOF_TO_ALPHA
			VertexShader = compile VERTEXSHADER	VS_VehicleTransformSkinDOF(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_VehicleTextured_EightDOF()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
#else // APPLY_DOF_TO_ALPHA
			VertexShader = compile VERTEXSHADER	VS_VehicleTransformSkin(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_VehicleTextured_Eight()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
#endif // APPLY_DOF_TO_ALPHA
		}
	}
	#endif // FORWARD_TECHNIQUES && DRAWSKINNED_TECHNIQUES

	#if FORWARD_TECHNIQUES && NONSKINNED_VEHICLE_TECHNIQUES
	technique lightweight0_draw
	{
		pass p0
		{
			FORWARD_RENDERSTATES()
#if APPLY_DOF_TO_ALPHA
			VertexShader = compile VERTEXSHADER	VS_VehicleTransformDOF(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_VehicleTextured_ZeroDOF()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
#else // APPLY_DOF_TO_ALPHA
			VertexShader = compile VERTEXSHADER	VS_VehicleTransform(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_VehicleTextured_Zero()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
#endif // APPLY_DOF_TO_ALPHA
		}
	}
	#endif // FORWARD_TECHNIQUES && NONSKINNED_VEHICLE_TECHNIQUES

#if FORWARD_TECHNIQUES && NONSKINNED_VEHICLE_TECHNIQUES
	technique lightweight0WaterRefractionAlpha_draw
	{
		pass p0
		{
			WATERREFRACTIONALPHA_RENDERSTATES()
#if APPLY_DOF_TO_ALPHA
			VertexShader = compile VERTEXSHADER	VS_VehicleTransformDOF(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_VehicleTextured_ZeroDOF_WaterRefraction()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
#else // APPLY_DOF_TO_ALPHA
			VertexShader = compile VERTEXSHADER	VS_VehicleTransform(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_VehicleTextured_Zero_WaterRefraction()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
#endif // APPLY_DOF_TO_ALPHA
		}
	}
#endif // FORWARD_TECHNIQUES && NONSKINNED_VEHICLE_TECHNIQUES

	#if FORWARD_TECHNIQUES && DRAWSKINNED_TECHNIQUES
	technique lightweight0_drawskinned
	{
		pass p0
		{
			FORWARD_RENDERSTATES()
#if APPLY_DOF_TO_ALPHA
			VertexShader = compile VERTEXSHADER	VS_VehicleTransformSkinDOF(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_VehicleTextured_ZeroDOF()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
#else //  APPLY_DOF_TO_ALPHA
			VertexShader = compile VERTEXSHADER	VS_VehicleTransformSkin(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_VehicleTextured_Zero()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
#endif // APPLY_DOF_TO_ALPHA			
		}
	}
	#endif // FORWARD_TECHNIQUES && DRAWSKINNED_TECHNIQUES

#if FORWARD_TECHNIQUES && DRAWSKINNED_TECHNIQUES
	technique lightweight0WaterRefractionAlpha_drawskinned
	{
		pass p0
		{
			WATERREFRACTIONALPHA_RENDERSTATES()
#if APPLY_DOF_TO_ALPHA
				VertexShader = compile VERTEXSHADER	VS_VehicleTransformSkinDOF(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_VehicleTextured_ZeroDOF_WaterRefraction()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
#else //  APPLY_DOF_TO_ALPHA
				VertexShader = compile VERTEXSHADER	VS_VehicleTransformSkin(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_VehicleTextured_Zero_WaterRefraction()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
#endif // APPLY_DOF_TO_ALPHA			
		}
	}
#endif // FORWARD_TECHNIQUES && DRAWSKINNED_TECHNIQUES

	#if FORWARD_TECHNIQUES && NONSKINNED_VEHICLE_TECHNIQUES
	technique lightweight4_draw
	{
		pass p0
		{
			FORWARD_RENDERSTATES()
#if APPLY_DOF_TO_ALPHA
			VertexShader = compile VERTEXSHADER	VS_VehicleTransformDOF(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_VehicleTextured_FourDOF()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
#else // APPLY_DOF_TO_ALPHA
			VertexShader = compile VERTEXSHADER	VS_VehicleTransform(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_VehicleTextured_Four()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
#endif // APPLY_DOF_TO_ALPHA
		}
	}
#endif // FORWARD_TECHNIQUES && NONSKINNED_VEHICLE_TECHNIQUES

#if FORWARD_TECHNIQUES && NONSKINNED_VEHICLE_TECHNIQUES
	technique lightweight4WaterRefractionAlpha_draw
	{
		pass p0
		{
			WATERREFRACTIONALPHA_RENDERSTATES()
#if APPLY_DOF_TO_ALPHA
				VertexShader = compile VERTEXSHADER	VS_VehicleTransformDOF(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_VehicleTextured_FourDOF_WaterRefraction()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
#else // APPLY_DOF_TO_ALPHA
				VertexShader = compile VERTEXSHADER	VS_VehicleTransform(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_VehicleTextured_Four_WaterRefraction()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
#endif // APPLY_DOF_TO_ALPHA
		}
	}
#endif // FORWARD_TECHNIQUES && NONSKINNED_VEHICLE_TECHNIQUES

	#if FORWARD_TECHNIQUES && DRAWSKINNED_TECHNIQUES
	technique lightweight4_drawskinned
	{
		pass p0
		{
			FORWARD_RENDERSTATES()
#if APPLY_DOF_TO_ALPHA
			VertexShader = compile VERTEXSHADER	VS_VehicleTransformSkinDOF(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_VehicleTextured_FourDOF()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
#else // APPLY_DOF_TO_ALPHA
			VertexShader = compile VERTEXSHADER	VS_VehicleTransformSkin(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_VehicleTextured_Four()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
#endif // APPLY_DOF_TO_ALPHA
		}
	}
#endif // FORWARD_TECHNIQUES && DRAWSKINNED_TECHNIQUES

#if FORWARD_TECHNIQUES && DRAWSKINNED_TECHNIQUES
	technique lightweight4WaterRefractionAlpha_drawskinned
	{
		pass p0
		{
			WATERREFRACTIONALPHA_RENDERSTATES()
#if APPLY_DOF_TO_ALPHA
				VertexShader = compile VERTEXSHADER	VS_VehicleTransformSkinDOF(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_VehicleTextured_FourDOF_WaterRefraction()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
#else // APPLY_DOF_TO_ALPHA
				VertexShader = compile VERTEXSHADER	VS_VehicleTransformSkin(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_VehicleTextured_Four_WaterRefraction()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
#endif // APPLY_DOF_TO_ALPHA
		}
	}
#endif // FORWARD_TECHNIQUES && DRAWSKINNED_TECHNIQUES

	#if FORWARD_TECHNIQUES && NONSKINNED_VEHICLE_TECHNIQUES
	technique lightweight8_draw
	{
		pass p0
		{
			FORWARD_RENDERSTATES()
#if APPLY_DOF_TO_ALPHA
			VertexShader = compile VERTEXSHADER	VS_VehicleTransformDOF(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_VehicleTextured_EightDOF()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
#else // APPLY_DOF_TO_ALPHA
			VertexShader = compile VERTEXSHADER	VS_VehicleTransform(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_VehicleTextured_Eight()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
#endif // APPLY_DOF_TO_ALPHA
		}
	}
#endif // FORWARD_TECHNIQUES && NONSKINNED_VEHICLE_TECHNIQUES

#if FORWARD_TECHNIQUES && NONSKINNED_VEHICLE_TECHNIQUES
	technique lightweight8WaterRefractionAlpha_draw
	{
		pass p0
		{
			WATERREFRACTIONALPHA_RENDERSTATES()
#if APPLY_DOF_TO_ALPHA
				VertexShader = compile VERTEXSHADER	VS_VehicleTransformDOF(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_VehicleTextured_EightDOF_WaterRefraction()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
#else // APPLY_DOF_TO_ALPHA
				VertexShader = compile VERTEXSHADER	VS_VehicleTransform(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_VehicleTextured_Eight_WaterRefraction()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
#endif // APPLY_DOF_TO_ALPHA
		}
	}
#endif // FORWARD_TECHNIQUES && NONSKINNED_VEHICLE_TECHNIQUES

	#if FORWARD_TECHNIQUES && DRAWSKINNED_TECHNIQUES
	technique lightweight8_drawskinned
	{
		pass p0
		{
			FORWARD_RENDERSTATES()
#if APPLY_DOF_TO_ALPHA
			VertexShader = compile VERTEXSHADER	VS_VehicleTransformSkinDOF(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_VehicleTextured_EightDOF()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
#else // APPLY_DOF_TO_ALPHA
			VertexShader = compile VERTEXSHADER	VS_VehicleTransformSkin(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_VehicleTextured_Eight()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
#endif // APPLY_DOF_TO_ALPHA
		}
	}
#endif // FORWARD_TECHNIQUES && DRAWSKINNED_TECHNIQUES

#if FORWARD_TECHNIQUES && DRAWSKINNED_TECHNIQUES
	technique lightweight8WaterRefractionAlpha_drawskinned
	{
		pass p0
		{
			WATERREFRACTIONALPHA_RENDERSTATES()
#if APPLY_DOF_TO_ALPHA
				VertexShader = compile VERTEXSHADER	VS_VehicleTransformSkinDOF(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_VehicleTextured_EightDOF_WaterRefraction()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
#else // APPLY_DOF_TO_ALPHA
				VertexShader = compile VERTEXSHADER	VS_VehicleTransformSkin(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_VehicleTextured_Eight_WaterRefraction()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
#endif // APPLY_DOF_TO_ALPHA
		}
	}
#endif // FORWARD_TECHNIQUES && DRAWSKINNED_TECHNIQUES

	#if HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED && NONSKINNED_VEHICLE_TECHNIQUES
	technique lightweightHighQuality0_draw
	{
		pass p0
		{
			FORWARD_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_VehicleTransform(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_VehicleTextured_ZeroHighQuality()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
		}
	}
#endif // HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED && NONSKINNED_VEHICLE_TECHNIQUES

#if HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED && NONSKINNED_VEHICLE_TECHNIQUES
	technique lightweightHighQuality0WaterRefractionAlpha_draw
	{
		pass p0
		{
			WATERREFRACTIONALPHA_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_VehicleTransform(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_VehicleTextured_ZeroHighQuality_WaterRefraction()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
		}
	}
#endif // HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED && NONSKINNED_VEHICLE_TECHNIQUES

	#if HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED && DRAWSKINNED_TECHNIQUES
	technique lightweightHighQuality0_drawskinned
	{
		pass p0
		{
			FORWARD_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_VehicleTransformSkin(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_VehicleTextured_ZeroHighQuality()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
		}
	}
#endif // HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED && DRAWSKINNED_TECHNIQUES

#if HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED && DRAWSKINNED_TECHNIQUES
	technique lightweightHighQuality0WaterRefractionAlpha_drawskinned
	{
		pass p0
		{
			WATERREFRACTIONALPHA_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_VehicleTransformSkin(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_VehicleTextured_ZeroHighQuality_WaterRefraction()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
		}
	}
#endif // HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED && DRAWSKINNED_TECHNIQUES

	#if HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED && NONSKINNED_VEHICLE_TECHNIQUES
	technique lightweightHighQuality4_draw
	{
		pass p0
		{
			FORWARD_RENDERSTATES()
#if APPLY_DOF_TO_ALPHA
			VertexShader = compile VERTEXSHADER	VS_VehicleTransformDOF(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_VehicleTextured_EightDOF()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
#else //  APPLY_DOF_TO_ALPHA
			VertexShader = compile VERTEXSHADER	VS_VehicleTransform(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_VehicleTextured_FourHighQuality()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
#endif // APPLY_DOF_TO_ALPHA
		}
	}
#endif // HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED && NONSKINNED_VEHICLE_TECHNIQUES

#if HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED && NONSKINNED_VEHICLE_TECHNIQUES
	technique lightweightHighQuality4WaterRefractionAlpha_draw
	{
		pass p0
		{
			WATERREFRACTIONALPHA_RENDERSTATES()
#if APPLY_DOF_TO_ALPHA
			VertexShader = compile VERTEXSHADER	VS_VehicleTransformDOF(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_VehicleTextured_EightDOF_WaterRefraction()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
#else //  APPLY_DOF_TO_ALPHA
			VertexShader = compile VERTEXSHADER	VS_VehicleTransform(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_VehicleTextured_FourHighQuality_WaterRefraction()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
#endif // APPLY_DOF_TO_ALPHA
		}
	}
#endif // HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED && NONSKINNED_VEHICLE_TECHNIQUES

	#if HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED && DRAWSKINNED_TECHNIQUES
	technique lightweightHighQuality4_drawskinned
	{
		pass p0
		{
			FORWARD_RENDERSTATES()
#if APPLY_DOF_TO_ALPHA
			VertexShader = compile VERTEXSHADER	VS_VehicleTransformSkinDOF(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_VehicleTextured_EightDOF()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
#else //  APPLY_DOF_TO_ALPHA
			VertexShader = compile VERTEXSHADER	VS_VehicleTransformSkin(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_VehicleTextured_FourHighQuality()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
#endif // APPLY_DOF_TO_ALPHA
		}
	}
#endif // HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED && DRAWSKINNED_TECHNIQUES

#if HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED && DRAWSKINNED_TECHNIQUES
	technique lightweightHighQuality4WaterRefractionAlpha_drawskinned
	{
		pass p0
		{
			WATERREFRACTIONALPHA_RENDERSTATES()
#if APPLY_DOF_TO_ALPHA
			VertexShader = compile VERTEXSHADER	VS_VehicleTransformSkinDOF(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_VehicleTextured_EightDOF_WaterRefraction()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
#else //  APPLY_DOF_TO_ALPHA
			VertexShader = compile VERTEXSHADER	VS_VehicleTransformSkin(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_VehicleTextured_FourHighQuality_WaterRefraction()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
#endif // APPLY_DOF_TO_ALPHA
		}
	}
#endif // HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED && DRAWSKINNED_TECHNIQUES

	#if HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED && NONSKINNED_VEHICLE_TECHNIQUES
	technique lightweightHighQuality8_draw
	{
		pass p0
		{
			FORWARD_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_VehicleTransform(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_VehicleTextured_EightHighQuality()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
		}
	}
#endif // HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED && NONSKINNED_VEHICLE_TECHNIQUES

#if HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED && NONSKINNED_VEHICLE_TECHNIQUES
	technique lightweightHighQuality8WaterRefractionAlpha_draw
	{
		pass p0
		{
			WATERREFRACTIONALPHA_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_VehicleTransform(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_VehicleTextured_EightHighQuality_WaterRefraction()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
		}
	}
#endif // HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED && NONSKINNED_VEHICLE_TECHNIQUES

	#if HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED && DRAWSKINNED_TECHNIQUES
	technique lightweightHighQuality8_drawskinned
	{
		pass p0
		{
			FORWARD_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_VehicleTransformSkin(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_VehicleTextured_EightHighQuality()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
		}
	}
#endif // HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED && DRAWSKINNED_TECHNIQUES

#if HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED && DRAWSKINNED_TECHNIQUES
	technique lightweightHighQuality8WaterRefractionAlpha_drawskinned
	{
		pass p0
		{
			WATERREFRACTIONALPHA_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_VehicleTransformSkin(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_VehicleTextured_EightHighQuality_WaterRefraction()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
		}
	}
#endif // HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED && DRAWSKINNED_TECHNIQUES

	// ===============================
	// Unlit techniques
	// ===============================
	#if UNLIT_TECHNIQUES && NONSKINNED_VEHICLE_TECHNIQUES
	technique unlit_draw
	{
		pass p0
		{
			FORWARD_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_VehicleTransformUnlit(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_VehicleTexturedUnlit()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif // UNLIT_TECHNIQUES && NONSKINNED_VEHICLE_TECHNIQUES

	#if UNLIT_TECHNIQUES && DRAWSKINNED_TECHNIQUES
	technique unlit_drawskinned
	{
		pass p0
		{
			FORWARD_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_VehicleTransformSkinUnlit(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_VehicleTexturedUnlit()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif // UNLIT_TECHNIQUES && DRAWSKINNED_TECHNIQUES

	// ===============================
	// Deferred techniques
	// ===============================
	#if DEFERRED_TECHNIQUES && NON_CUTOUT_DEFERRED_TECHNIQUES && NONSKINNED_VEHICLE_TECHNIQUES
	technique deferred_draw
	{
		pass p0
		{
			DEFERRED_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_VehicleTransformD(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_DeferredVehicleTextured()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#if defined(DO_WHEEL_ROUNDING_TESSELLATION)
	technique deferred_draw_sm50
	{
		pass p0
		{
			VertexShader = compile VSDS_SHADER	VS_WR_vehicleVertexInputBump(gbOffsetEnable);
			SetHullShader(compileshader(hs_5_0, HS_WR_vehicleVertexInputBump()));
			SetDomainShader(compileshader(ds_5_0, DS_WR_VehicleTransformD()));
			PixelShader = compile PIXELSHADER PS_DeferredVehicleTextured()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif // defined(DO_WHEEL_ROUNDING_TESSELLATION)
	#endif // DEFERRED_TECHNIQUES && NON_CUTOUT_DEFERRED_TECHNIQUES && NONSKINNED_VEHICLE_TECHNIQUES

	//-------------------------------------------------------------------------------//
	
	#if DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES && NONSKINNED_VEHICLE_TECHNIQUES
	technique deferredalphaclip_draw
	{
		pass p0
		{
			DEFERRED_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_VehicleTransformD(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_DeferredVehicleTextured_AlphaClip()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#if defined(DO_WHEEL_ROUNDING_TESSELLATION)
	technique deferredalphaclip_draw_sm50
	{
		pass p0
		{
			VertexShader = compile VSDS_SHADER	VS_WR_vehicleVertexInputBump(gbOffsetEnable);
			SetHullShader(compileshader(hs_5_0, HS_WR_vehicleVertexInputBump()));
			SetDomainShader(compileshader(ds_5_0, DS_WR_VehicleTransformD()));
			PixelShader = compile PIXELSHADER PS_DeferredVehicleTextured_AlphaClip()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif // defined(DO_WHEEL_ROUNDING_TESSELLATION)
	#endif // DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES && NONSKINNED_VEHICLE_TECHNIQUES

	//-------------------------------------------------------------------------------//

	#if DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES && NONSKINNED_VEHICLE_TECHNIQUES
	technique deferredsubsamplealpha_draw
	{
		pass p0
		{
			DEFERRED_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_VehicleTransformD(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_DeferredVehicleTextured_SubSampleAlpha()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#if defined(DO_WHEEL_ROUNDING_TESSELLATION)
	technique deferredsubsamplealpha_draw_sm50
	{
		pass p0
		{
			VertexShader = compile VSDS_SHADER	VS_WR_vehicleVertexInputBump(gbOffsetEnable);
			SetHullShader(compileshader(hs_5_0, HS_WR_vehicleVertexInputBump()));
			SetDomainShader(compileshader(ds_5_0, DS_WR_VehicleTransformD()));
			PixelShader = compile PIXELSHADER PS_DeferredVehicleTextured_SubSampleAlpha()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif // defined(DO_WHEEL_ROUNDING_TESSELLATION)
	#endif // DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES && NONSKINNED_VEHICLE_TECHNIQUES

	//-------------------------------------------------------------------------------//

	#if DEFERRED_TECHNIQUES && NON_CUTOUT_DEFERRED_TECHNIQUES && DRAWSKINNED_TECHNIQUES
	technique deferred_drawskinned
	{
		pass p0
		{
			DEFERRED_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_VehicleTransformSkinD(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_DeferredVehicleTextured()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}	
	#endif // DEFERRED_TECHNIQUES && NON_CUTOUT_DEFERRED_TECHNIQUES && DRAWSKINNED_TECHNIQUES

	#if DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES && DRAWSKINNED_TECHNIQUES
	technique deferredalphaclip_drawskinned
	{
		pass p0
		{
			DEFERRED_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_VehicleTransformSkinD(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_DeferredVehicleTextured_AlphaClip()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}	
	#endif // DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES && DRAWSKINNED_TECHNIQUES

	#if DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES && DRAWSKINNED_TECHNIQUES
	technique deferredsubsamplealpha_drawskinned
	{
		pass p0
		{
			DEFERRED_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_VehicleTransformSkinD(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_DeferredVehicleTextured_SubSampleAlpha()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
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
			VertexShader = compile VERTEXSHADER	VS_VehicleTransformWaterReflection();
			PixelShader  = compile PIXELSHADER	PS_VehicleTexturedWaterReflection()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}

	technique waterreflectionalphaclip_draw
	{
		pass p0
		{
			FORWARD_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_VehicleTransformWaterReflection();
			PixelShader  = compile PIXELSHADER	PS_VehicleTexturedWaterReflection()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif // WATER_REFLECTION_TECHNIQUES

	#if WATER_REFLECTION_TECHNIQUES && DRAWSKINNED_TECHNIQUES
	technique waterreflection_drawskinned
	{
		pass p0
		{
			FORWARD_RENDERSTATES()
			VertexShader = compile VERTEXSHADER	VS_VehicleTransformSkinWaterReflection();
			PixelShader  = compile PIXELSHADER	PS_VehicleTexturedWaterReflection()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif // WATER_REFLECTION_TECHNIQUES && DRAWSKINNED_TECHNIQUES

#endif	//__MAX...

#endif	// TECHNIQUES

#if !__MAX
#if NONSKINNED_VEHICLE_TECHNIQUES
	SHADERTECHNIQUE_CASCADE_SHADOWS()
	SHADERTECHNIQUE_LOCAL_SHADOWS(VS_VehicleLinearDepth, VS_LinearDepthSkin, PS_LinearDepth)
	SHADERTECHNIQUE_ENTITY_SELECT(VS_VehicleTransformUnlit(gbOffsetEnable), VS_VehicleTransformSkinUnlit(gbOffsetEnable))
#else
	SHADERTECHNIQUE_CASCADE_SHADOWS_SKINNED_ONLY()
	SHADERTECHNIQUE_LOCAL_SHADOWS_SKINNED_ONLY(VS_LinearDepthSkin, PS_LinearDepth)
	SHADERTECHNIQUE_ENTITY_SELECT_SKINNED_ONLY(VS_VehicleTransformSkinUnlit(gbOffsetEnable))
#endif
#include "../Debug/debug_overlay_vehicle.fxh"
#endif	//!__MAX...


#endif //__GTA_VEHICLE_COMMON_H__...
