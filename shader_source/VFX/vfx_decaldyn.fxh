///////////////////////////////////////////////////////////////////////////////
//  
//  
//  
//  
//  
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  PRAGMAS
///////////////////////////////////////////////////////////////////////////////
#ifndef PRAGMA_DCL
	#pragma dcl position
	#define PRAGMA_DCL
#endif

///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////

#define USING_PREDICATED_TILING __XENON

#define DECAL_SHADER
#define DECAL_USE_NORMAL_MAP_ALPHA
#define SPECULAR 1
#define VFX_DECAL

#define DEFERRED_UNPACK_LIGHT
#define DEFERRED_UNPACK_LIGHT_CSM_FORWARD_ONLY // ensure deferred shader vars are not used for cascade shadows
#define DEFERRED_NO_LIGHT_SAMPLERS //turn off light samplers and use custom ones

#ifdef USE_EMISSIVE
	#define EMISSIVE (1)
#endif

#if MULTISAMPLE_TECHNIQUES
	#define SAMPLE_INDEX IN.sampleIndex
#else
	#define SAMPLE_INDEX 0
#endif

///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////
#include "../common.fxh"

//Will want merging in with other constants once up and running.
BeginConstantBufferDX10( vfx_decaldyn_locals )

	float polyRejectThreshold : PolyRejectThreshold;

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

	float specularFresnel : Fresnel
	<
		string UIName = "Specular Fresnel";
		float UIMin = 0.0;
		float UIMax = 1.0;
		float UIStep = 0.01;
	> = 0.75;
	#endif // SPECULAR

	#if EMISSIVE
		float emissiveMultiplier : EmissiveMultiplier
		<
			string UIName = "Emissive HDR Multiplier";
			float UIMin = 0.0;
			float UIMax = 16.0;
			float UIStep = 0.1;
		> = 0.0f;
	#endif //EMISSIVE

	#ifdef USE_PARALLAX_MAP
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
	#else
		#define PARALLAX							(0)
	#endif // USE_PARALLAX_MAP...
EndConstantBufferDX10( vfx_decaldyn_locals )

///////////////////////////////////////////////////////////////////////////////
//  SAMPLERS
///////////////////////////////////////////////////////////////////////////////
BeginSampler(sampler, decalTexture, gDecalSampler, decalTexture)
ContinueSampler(sampler, decalTexture, gDecalSampler, decalTexture)
	MINFILTER = HQLINEAR;
	MAGFILTER = MAGLINEAR;
	MIPFILTER = MIPLINEAR;
EndSampler;

BeginSampler(sampler, decalNormal, gDecalNormalSampler, decalNormal)
ContinueSampler(sampler, decalNormal, gDecalNormalSampler, decalNormal)
	MINFILTER = HQLINEAR;
	MAGFILTER = MAGLINEAR;
	MIPFILTER = MIPLINEAR;
EndSampler;


///////////////////////////////////////////////////////////////////////////////
//  INCLUDES (MUST BE AFTER THE SAMPLER OR ONE OF THE CLAMPS TURNS TO A WRAP!)
///////////////////////////////////////////////////////////////////////////////

#define SHADOW_CASTING            (0)
#define SHADOW_CASTING_TECHNIQUES (0)
#define SHADOW_RECEIVING          (0)
#define SHADOW_RECEIVING_VS       (1)
#include "../Lighting/Shadows/localshadowglobals.fxh"
#include "../Lighting/Shadows/cascadeshadows.fxh"
#include "../Lighting/lighting.fxh"
#include "../Lighting/forward_lighting.fxh"


///////////////////////////////////////////////////////////////////////////////
//  GLOBAL VARIABLES
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  RIPPLES - Vertex Declarations
///////////////////////////////////////////////////////////////////////////////

struct vertexInputWaterFx
{
	float3 vtxPos				: POSITION;
	float3 midPos				: NORMAL;
	float2 dir					: TEXCOORD0;
	float4 flip					: COLOR0;
};

// With DECLARE_POSITION, VS output and PS input could be shared.
struct vertexOutputWaterFx
{
	DECLARE_POSITION(pos)
	float4 col					: TEXCOORD3;
	float4 pPos					: TEXCOORD0;
	float4 dir					: TEXCOORD1;
	float4 midPos				: TEXCOORD2;
};

struct pixelInputWaterFx
{
	DECLARE_POSITION(vPos)
	float4 col					: TEXCOORD3;
	float4 pPos					: TEXCOORD0;
	float4 dir					: TEXCOORD1;
	float4 midPos				: TEXCOORD2;
#if MULTISAMPLE_TECHNIQUES
	uint sampleIndex			: SV_SampleIndex;
#endif
};

///////////////////////////////////////////////////////////////////////////////
//  RIPPLES - Vertex Shader
///////////////////////////////////////////////////////////////////////////////

vertexOutputWaterFx VS_rippleFx(vertexInputWaterFx IN)
{
	vertexOutputWaterFx OUT;

	// calculate screen position of vertex
	OUT.pos	= mul(float4(IN.vtxPos, 1), gWorldViewProj);

	// pass through mid position of this ripple texture
	float size = length(IN.dir.xy);
	OUT.midPos.xyz = IN.midPos.xyz;
	OUT.midPos.w = 1.0f/size;

	// calculate the shadow at this vertex
	float shadow = CalcCascadeShadowSampleVS(OUT.midPos.xyz, true, true);

	// calculate the colour at this vertex
	OUT.col.rgb = calcAmbient(1.0f, 1.0f, 0.0f, gInInterior);	
	OUT.col.rgb	+= gDirectionalColour.rgb * (abs(gDirectionalLight.z) * (1-shadow));
	OUT.col.a = IN.flip.a * gInvColorExpBias;	

	// calculate the direction of this vertex
	float flip = (IN.flip.x-0.5f) * 2.0f;
	OUT.dir.xy = IN.dir.xy * OUT.midPos.w;
	OUT.dir.zw = IN.dir.yx * float2(-flip, flip) * OUT.midPos.w;

	//
	OUT.pPos = float4((IN.vtxPos-gViewInverse[3].xyz), OUT.pos.w);

	//
	return(OUT);
}


///////////////////////////////////////////////////////////////////////////////
//  RIPPLES - Pixel Shader
///////////////////////////////////////////////////////////////////////////////

half4 PS_rippleFx(pixelInputWaterFx IN): COLOR
{
//	return float4(0, 0, 1, 1);

	DeferredSurfaceInfo surfaceInfo = UnPackGBuffer_P(IN.vPos, IN.pPos, false, SAMPLE_INDEX);

	float3 diff = surfaceInfo.positionWorld - IN.midPos.xyz;

	float ooSize = IN.midPos.w;

	float2 texUV = (float2(dot(IN.dir.xy, diff.xy), dot(IN.dir.zw, diff.xy))*(0.5f*ooSize)) + 0.5f;

//	texUV -= 0.5f;
//	texUV.y *= 1.0f+(texUV.y<0.0f)*3;
//	texUV += 0.5f;
// 	texUV -= 0.5f;
// 	texUV.y=(texUV.y*4.0f)-1.0f;
// 	texUV += 0.5f;

	// alpha out any area outside 0...1 texture coords (avoids wrapping within the decal when viewed at an angle)
	if (texUV.x<0.0f) IN.col.a = 0.0f;
	if (texUV.x>1.0f) IN.col.a = 0.0f;
	if (texUV.y<0.0f) IN.col.a = 0.0f;
	if (texUV.y>1.0f) IN.col.a = 0.0f;

	float4 tcol = tex2D(gDecalSampler, texUV);

	return PackHdr(tcol*IN.col);
}













///////////////////////////////////////////////////////////////////////////////
//  DECALS - Vertex Declarations
///////////////////////////////////////////////////////////////////////////////

struct vertexInputDecalFx
{
	float4 vtxPos					: POSITION;										// world position of the decal projection box vertex
	float4 decalPos					: NORMAL;										// the world centre position of the decal projection box, w: packed texWrapLen and curWrapLen
	float4 forwardVec				: TEXCOORD0;									// the world forward vector of the projection (up the texture)
	float4 sideVec					: TEXCOORD1;									// the world side vector of the projection (along the texture)
	float4 tex						: TEXCOORD2;									// texRows, texCols, texId, flipUV	
	float4 alpha					: TEXCOORD3;									// alpha, alphaFront, alphaBack, poly reject threshold
};

#define VS_VERTEX_POS_WLD			vtxPos
#define VS_DECAL_COL_R				vtxPos.w
#define VS_DECAL_COL_G				alpha.w
#define VS_DECAL_COL_B				sideVec.w
#define VS_DECAL_POS_WLD			decalPos
#define VS_DECAL_FORWARD_WLD		forwardVec
#define VS_DECAL_SIDE_WLD			sideVec
#define	VS_TEX_ROWS					tex.x
#define	VS_TEX_COLUMNS				tex.y
#define	VS_TEX_ID					tex.z
#define	VS_TEX_FLIP_UV				tex.w
#define VS_ALPHA					alpha.x
#define VS_ALPHA_FRONT				alpha.y
#define VS_ALPHA_BACK				alpha.z

struct decalInfo
{
	float4 decalPos					: TEXCOORD0;									// the world centre position of the decal projection box
	float4 camToVtx					: TEXCOORD1;									// the vector from the camera to the vertex
	float4 forwardVec				: TEXCOORD2;									// the world forward vector of the projection (up the texture) - w now stores the length
	float4 sideVec					: TEXCOORD3;									// the world side vector of the projection (along the texture) - w now stores the length
	float4 dirVec					: TEXCOORD4;									// the world side vector of the projection (down the projection)
	float4 alphaInfo				: TEXCOORD5;									// alpha, alphaFront, alphaBack, ambient scale
	float4 uvInfo					: TEXCOORD6;									// xIndex, yIndex, xStep, yStep	
	float4 worldEyePos				: TEXCOORD7;									// 	
	float4 texInfo					: TEXCOORD8;									// flipu, flipv, total wrap length, curr wrap length
	float4 miscInfo					: TEXCOORD9;									// poly reject threshold, specular intensity, specular exponent, emissive scale
#if USING_PREDICATED_TILING
	float4 vpos						: TEXCOORD10;
#endif
};

// TODO: VS output and PS input could be shared now
struct vertexOutputDecalFx
{
	DECLARE_POSITION(screenPos)										// the screen position of the vertex
	decalInfo info;
};

struct pixelInputDecalFx
{
	DECLARE_POSITION(vPos)											// the interpolated pixel position
	decalInfo info;
#if MULTISAMPLE_TECHNIQUES
	uint sampleIndex			: SV_SampleIndex;
#endif
};

#define PS_PIXEL_POS				vPos
#define	PS_SCREEN_POS				screenPos
#define PS_DECAL_POS_WLD			info.decalPos
#define PS_CAM_TO_VTX				info.camToVtx
#define PS_FORWARD_WLD				info.forwardVec.xyz
#define PS_FORWARD_WLD_LENGTH		info.forwardVec.w
#define PS_SIDE_WLD					info.sideVec.xyz
#define PS_SIDE_WLD_LENGTH			info.sideVec.w
#define PS_DIR_WLD					info.dirVec.xyz
#define PS_ALPHA					info.alphaInfo.x
#define PS_ALPHA_FRONT				info.alphaInfo.y
#define PS_ALPHA_BACK				info.alphaInfo.z
#define PS_TEX_U_INDEX				info.uvInfo.x
#define PS_TEX_V_INDEX				info.uvInfo.y
#define PS_TEX_UV_INDEX				info.uvInfo.xy
#define PS_TEX_U_STEP				info.uvInfo.z
#define PS_TEX_V_STEP				info.uvInfo.w
#define PS_TEX_UV_STEP				info.uvInfo.zw
#define PS_WORLD_EYE_POS			info.worldEyePos.xyz
#define PS_UV_FLIP					info.texInfo
#define PS_UV_FLIP_U				PS_UV_FLIP.x
#define PS_UV_FLIP_V				PS_UV_FLIP.y
#define PS_TEX_TOTAL_WRAP_LENGTH	info.texInfo.z
#define PS_TEX_CURR_WRAP_LENGTH		info.texInfo.w
#define PS_AMBIENT_SCALE			info.alphaInfo.w
#define PS_EMISSIVE_SCALE			info.miscInfo.w
#define PS_COL_R					info.worldEyePos.w
#define PS_COL_G					info.decalPos.w
#define PS_COL_B					info.dirVec.w

#if USING_PREDICATED_TILING
#define PS_VPOS						info.vpos
#endif

///////////////////////////////////////////////////////////////////////////////
//  DECALS - Vertex Shader
///////////////////////////////////////////////////////////////////////////////

vertexOutputDecalFx VS_decalFx(vertexInputDecalFx IN)
{
	//
	vertexOutputDecalFx OUT = (vertexOutputDecalFx)0;

	// calculate screen position of vertex
	OUT.PS_SCREEN_POS = mul(float4(IN.VS_VERTEX_POS_WLD.xyz, 1.0f), gWorldViewProj);	

	// pass through mid position of this decal texture
	OUT.PS_DECAL_POS_WLD.xyz = IN.VS_DECAL_POS_WLD.xyz;

	// calculate ambient occlusion strength 
	// TODO: function reuse instead
	OUT.PS_AMBIENT_SCALE = gNaturalAmbientScale;
	OUT.PS_EMISSIVE_SCALE = gArtificialAmbientScale;

	// calculate the direction vector of the projection
	OUT.info.dirVec = float4(cross(normalize(IN.VS_DECAL_FORWARD_WLD.xyz), normalize(IN.VS_DECAL_SIDE_WLD.xyz)), 0.0f);

	// calculate the length of the forward vector (and store in w component)
	float size = length(IN.VS_DECAL_FORWARD_WLD.xyz);
	OUT.PS_FORWARD_WLD_LENGTH = 1.0f/size;

	// pass through the forward vector
    OUT.PS_FORWARD_WLD = IN.VS_DECAL_FORWARD_WLD.xyz * OUT.PS_FORWARD_WLD_LENGTH;

	// calculate the length of the side vector (and store in w component)
	size = length(IN.VS_DECAL_SIDE_WLD.xyz);
	OUT.PS_SIDE_WLD_LENGTH = 1.0f/size;
	
	// pass through the side vector
	OUT.PS_SIDE_WLD = IN.VS_DECAL_SIDE_WLD.xyz * OUT.PS_SIDE_WLD_LENGTH;

	// calculate world eye position (for specular)
	OUT.PS_WORLD_EYE_POS = gViewInverse[3].xyz - IN.VS_VERTEX_POS_WLD.xyz;

	// calculate the vector from the camera to the vertex
#ifdef NVSTEREO
	float3 StereorizedCamPos = gViewInverse[3].xyz + (StereoWorldCamOffSet());
	OUT.PS_CAM_TO_VTX = float4((IN.VS_VERTEX_POS_WLD.xyz - StereorizedCamPos), OUT.PS_SCREEN_POS.w);
#else
	OUT.PS_CAM_TO_VTX = float4((IN.VS_VERTEX_POS_WLD.xyz - gViewInverse[3].xyz), OUT.PS_SCREEN_POS.w);
#endif

	// pass down the transformed position to fragment shader so we can derive screen-relative coordinates
	// this is necessary on 360 since we currently do not have support to get an offset we can add to
	// the VPOS inputs when running with predicated tiling - i.e. we'd need to predicate a shader constant for this.
#if USING_PREDICATED_TILING
	OUT.PS_VPOS = convertToVpos(OUT.PS_SCREEN_POS, deferredLightScreenSize);
#endif

	// copy over the alpha info
	OUT.PS_ALPHA = IN.VS_ALPHA;
	OUT.PS_ALPHA_FRONT = IN.VS_ALPHA_FRONT;
	OUT.PS_ALPHA_BACK = IN.VS_ALPHA_BACK;

	// calculate the uvinfo (for correcting the uv coordinates in the texture page)
	OUT.PS_TEX_U_INDEX = fmod(IN.VS_TEX_ID, IN.VS_TEX_COLUMNS);						// xIndex = id % cols
	OUT.PS_TEX_V_INDEX = floor(IN.VS_TEX_ID / IN.VS_TEX_COLUMNS);					// yIndex = id / cols
	OUT.PS_TEX_U_STEP = 1.0f/IN.VS_TEX_COLUMNS;										// xStep  = 1 / cols
	OUT.PS_TEX_V_STEP = 1.0f/IN.VS_TEX_ROWS;										// yStep  = 1 / rows

#if RSG_DURANGO || RSG_ORBIS || RSG_PC
	// Both Durango and Orbis seems to be quite inaccurate.  fmod(6,3)~3, not 0.
	// Unfortunately both PIX and Razor GPU is usless at the moment so cannot
	// really debug the shader to see where the inaccuracy is coming from.
	// Guessing the compiler is emitting a V_RCP_F32 with no Newton-Raphson
	// refinement.
	//
	// For safety, apply this same hack on PC.
	//
	if (IN.VS_TEX_COLUMNS - OUT.PS_TEX_U_INDEX < 0.001)
	{
		OUT.PS_TEX_U_INDEX = 0;
		OUT.PS_TEX_V_INDEX += 1;
	}
#endif

	// copy over the tex info
	OUT.PS_UV_FLIP_U = floor(IN.VS_TEX_FLIP_UV/2.0f);
	OUT.PS_UV_FLIP_V = fmod(IN.VS_TEX_FLIP_UV, 2.0f);

	OUT.PS_TEX_TOTAL_WRAP_LENGTH = IN.VS_DECAL_FORWARD_WLD.w;
	OUT.PS_TEX_CURR_WRAP_LENGTH = IN.VS_DECAL_POS_WLD.w;

	// copy color over to PS structure
	OUT.PS_COL_R = IN.VS_DECAL_COL_R;
	OUT.PS_COL_G = IN.VS_DECAL_COL_G;
	OUT.PS_COL_B = IN.VS_DECAL_COL_B;

	//
    return OUT;
}


///////////////////////////////////////////////////////////////////////////////
//	DECALS - Pixel Shader (Common)
///////////////////////////////////////////////////////////////////////////////

bool PS_decalFxCommon(pixelInputDecalFx IN, out float3 positionWorld, out SurfaceProperties surfProps, out StandardLightingProperties stdLightProps, bool isForwardRendering)
{	

#if USING_PREDICATED_TILING
	// derive screen coordinates from view-projection transformed position
	const float2 screenPos = IN.PS_VPOS.xy / IN.PS_VPOS.w;
	DeferredSurfaceInfo surfaceInfo = UnPackGBuffer_S(screenPos.xy, IN.PS_CAM_TO_VTX, false, SAMPLE_INDEX);
#else
	// get information about the surface that this pixel is on
	DeferredSurfaceInfo surfaceInfo = UnPackGBuffer_P(IN.PS_PIXEL_POS, IN.PS_CAM_TO_VTX, false, SAMPLE_INDEX);
#endif

	// World space normal of underlying geometry
	float3 vNormalWS; 

	// This should be a compile time branch and allows us to avoid sampling Gbuffer1 to get 
	// the normal and thus avoids an early resolve on the 360
	const bool bSampleNormal = !isForwardRendering;

	if ( bSampleNormal ) 
	{
		vNormalWS = -normalize(surfaceInfo.normalWorld.xyz); // Why is this negated?
	}
	else
	{
		vNormalWS = normalize( cross( ddx( surfaceInfo.positionWorld.xyz ),  ddy( surfaceInfo.positionWorld.xyz ) ) );
	}

#if __MAX
	surfProps	  = (SurfaceProperties)0;
	stdLightProps = (StandardLightingProperties)0;
	positionWorld = float3(0.0f, 0.0f, 0.0f);
#else
	// reject this pixel if it's world normal is at too much of an angle to the decal projection
	if (dot(IN.PS_DIR_WLD.xyz, vNormalWS) < polyRejectThreshold)
	{
		surfProps = (SurfaceProperties)0;
		stdLightProps = (StandardLightingProperties)0;
		positionWorld = float3(0.0f, 0.0f, 0.0f);
		return false;
	}
#endif

	// calculate the vector from the centre of the decal to the world position of the pixel
	float3 decalVec = surfaceInfo.positionWorld - IN.PS_DECAL_POS_WLD.xyz;

	// calculate the texture coords by doing a dot product with the forward and side vectors
	float2 texUV = (float2(dot(IN.PS_SIDE_WLD, decalVec.xyz), dot(-IN.PS_FORWARD_WLD, decalVec.xyz))* float2(0.5f*IN.PS_SIDE_WLD_LENGTH, 0.5f*IN.PS_FORWARD_WLD_LENGTH)) + 0.5f;

	// apply the front/back alpha
	float alphaFront = IN.PS_ALPHA_FRONT;
	float alphaBack = IN.PS_ALPHA_BACK;
	IN.PS_ALPHA *= (alphaFront + texUV.y*(alphaBack-alphaFront));

	// flip the texture coords if required
	texUV.xy = lerp( texUV.xy, 1.0f-texUV.xy, IN.PS_UV_FLIP.xy );

	// alpha out any area outside 0...1 texture coords (avoids wrapping within the decal when viewed at an angle)
	if (texUV.x<0.0f) IN.PS_ALPHA = 0.0f;
	if (texUV.x>1.0f) IN.PS_ALPHA = 0.0f;
	if (texUV.y<0.0f) IN.PS_ALPHA = 0.0f;
	if (texUV.y>1.0f) IN.PS_ALPHA = 0.0f;

	// calc the correct v coords when the texture needs wrapped over a certain length
	if (IN.PS_TEX_TOTAL_WRAP_LENGTH>0.0f)
	{
		float startV = 1.0f - ((IN.PS_TEX_CURR_WRAP_LENGTH/IN.PS_TEX_TOTAL_WRAP_LENGTH) - floor(IN.PS_TEX_CURR_WRAP_LENGTH/IN.PS_TEX_TOTAL_WRAP_LENGTH));
		float scaleV = ((1.0f/IN.PS_FORWARD_WLD_LENGTH)*2.0f)/IN.PS_TEX_TOTAL_WRAP_LENGTH;
		texUV.y = startV + (texUV.y*scaleV);
	}

	// convert from texture page space to texture space (to a single texture within the texture page)
	texUV.xy = ((IN.PS_TEX_UV_INDEX+texUV.xy) * IN.PS_TEX_UV_STEP);						// (xIndex+u) * xStep; (yIndex+v) * yStep

	// clamp the texture coords outside this texture
	//texUV = clamp(texUV, float2(IN.uvInfo.x*IN.uvInfo.z, IN.uvInfo.y*IN.uvInfo.w), float2((IN.uvInfo.x+1.0f)*IN.uvInfo.z, (IN.uvInfo.y+1.0f)*IN.uvInfo.w));


	// we choose the projection box normal instead of surfaceInfo.normalWorld
	// to make the shading of dynamic decals match static ones more closely
	float3 worldGeomNormal	= normalize(cross(IN.PS_SIDE_WLD, IN.PS_FORWARD_WLD));
	float3 worldTangent		= IN.PS_SIDE_WLD*lerp(1.0f, -1.0f, IN.PS_UV_FLIP_U );
	float3 worldBiTangent	= -IN.PS_FORWARD_WLD*lerp(1.0f, -1.0f, IN.PS_UV_FLIP_V );

	// lookup the pixel's normal from the normal map 
#if PARALLAX
	// transform PS_WORLD_EYE_POS into tangent space:
	float4 tanEyePos;
	tanEyePos.x = dot(worldTangent, IN.PS_WORLD_EYE_POS.xyz);
	tanEyePos.y = dot(worldBiTangent, IN.PS_WORLD_EYE_POS.xyz);
	tanEyePos.z = dot(worldGeomNormal, IN.PS_WORLD_EYE_POS.xyz);
	tanEyePos.w = 1.0f;
	float2 outTexCoord;
	float3 packedNormalHeight = CalculateParallax(tanEyePos, tex2D_NormalHeightMap(gDecalNormalSampler, texUV.xy), texUV, texUV, gDecalNormalSampler, outTexCoord).xyz;
	texUV = outTexCoord;
#else
	float3 packedNormalHeight = tex2D_NormalMap(gDecalNormalSampler, texUV).xyz;
#endif

#if !defined(SHADER_FINAL)
//	comment back in to debug tangent frame
//
//	float3 posDDX = ddx(surfaceInfo.positionWorld);
//	float3 posDDY = ddy(surfaceInfo.positionWorld);
//	float2 uvDDX = ddx(texUV);
//	float2 uvDDY = ddy(texUV);
//	
//	worldTangent = normalize(posDDX * uvDDX.x + posDDY * uvDDY.x);
//	worldBiTangent = normalize(posDDX * uvDDX.y + posDDY * uvDDY.y);
#endif // !defined(SHADER_FINAL)

	float3 worldNormal = CalculateWorldNormal(packedNormalHeight.xy, worldTangent, worldBiTangent, worldGeomNormal );

	// lookup the pixel's diffuse colour 
	float4 diffCol = tex2D(gDecalSampler, texUV)*float4(IN.PS_COL_R, IN.PS_COL_G, IN.PS_COL_B, 1);

	if (isForwardRendering)
	{	
		diffCol = ProcessDiffuseColor(diffCol);
	}

	// setup surface properties structure
#if SPECULAR
	float fresnel = specularFresnel;

	surfProps.surface_fresnel = fresnel;
	surfProps.surface_specularIntensity	= specularIntensityMult;
	surfProps.surface_specularExponent	= specularFalloffMult;
	surfProps.surface_specularSkin = 0.0f;
#endif
	surfProps.surface_worldNormal		= float4(worldNormal.xyz, 1.0f);
	surfProps.surface_baseColor			= float4(IN.PS_AMBIENT_SCALE, IN.PS_EMISSIVE_SCALE, 1.0f , IN.PS_ALPHA);
	surfProps.surface_diffuseColor		= diffCol;

#if PARALLAX
	// parallax only: if BumpMap.xy is (0,0), then we want no lighting:
	const float threshold = 0.002f; // 0.00001f
	float parallaxLightMask = (dot(packedNormalHeight.xy, packedNormalHeight.xy) >= threshold);

	surfProps.surface_diffuseColor.rgb	*= parallaxLightMask;
	surfProps.surface_baseColor.rgb		*= parallaxLightMask;
#if SPECULAR
	surfProps.surface_specularIntensity	*= parallaxLightMask;
#endif
#endif //PARALLAX...


#if EMISSIVE
	surfProps.surface_emissiveIntensity	= emissiveMultiplier;
#endif
#if USE_SLOPE
	surfProps.slope = 0.;
#endif
	// compute standard light properties
	stdLightProps = DeriveLightingPropertiesForCommonSurface(surfProps);

	// copy over the world position
	positionWorld = surfaceInfo.positionWorld;

	return true;
}


///////////////////////////////////////////////////////////////////////////////
//	DECALS - Pixel Shader (Forward)
///////////////////////////////////////////////////////////////////////////////

half4 PS_decalFxForward(pixelInputDecalFx IN, int numLights, bool bUseCapsule, bool bUseHighQuality)
{

	float3 positionWorld;
	SurfaceProperties surfProps;
	StandardLightingProperties stdLightProps;
	if (PS_decalFxCommon(IN, positionWorld, surfProps, stdLightProps, true))
	{
		surfaceProperties surface;
		directionalLightProperties light;
		materialProperties material;

		populateForwardLightingStructs(
			surface,
			material,
			light,
			positionWorld,
			surfProps.surface_worldNormal.xyz,
			stdLightProps);

		float4 finalCol = calculateForwardLighting(
			numLights,
			bUseCapsule,
			surface,
			material,
			light,
			true, // directional
				SHADOW_RECEIVING,
				bUseHighQuality, // directionalShadowHighQuality
			IN.PS_PIXEL_POS.xy*gooScreenSize.xy);
	
		return finalCol;
	}
	else
	{
		return float4(0.0f, 0.0f, 0.0f, 0.0f);
	}
}

half4 PS_decalFxForward_Textured_Zero(pixelInputDecalFx IN): COLOR
{
	return PackHdr(PS_decalFxForward(IN, 0, false, false));
}

half4 PS_decalFxForward_Textured_Four(pixelInputDecalFx IN): COLOR
{
	return PackHdr(PS_decalFxForward(IN, 4, true, false));
}

half4 PS_decalFxForward_Textured_Eight(pixelInputDecalFx IN): COLOR
{
	return PackHdr(PS_decalFxForward(IN, 8, true, false));
}

#if HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED
#if HIGHQUALITY_FORWARD_TECHNIQUES

half4 PS_decalFxForward_Textured_ZeroHighQuality(pixelInputDecalFx IN): COLOR
{
	return PackHdr(PS_decalFxForward(IN, 0, false, HIGHQUALITY_FORWARD_TECHNIQUES));
}

half4 PS_decalFxForward_Textured_FourHighQuality(pixelInputDecalFx IN): COLOR
{
	return PackHdr(PS_decalFxForward(IN, 4, true, HIGHQUALITY_FORWARD_TECHNIQUES));
}

half4 PS_decalFxForward_Textured_EightHighQuality(pixelInputDecalFx IN): COLOR
{
	return PackHdr(PS_decalFxForward(IN, 8, true, HIGHQUALITY_FORWARD_TECHNIQUES));
}

#else

#define PS_decalFxForward_Textured_ZeroHighQuality PS_decalFxForward_Textured_Zero
#define PS_decalFxForward_Textured_FourHighQuality PS_decalFxForward_Textured_Four
#define PS_decalFxForward_Textured_EightHighQuality PS_decalFxForward_Textured_Eight

#endif
#endif // HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED

///////////////////////////////////////////////////////////////////////////////
//	DECALS - Pixel Shader (Deferred)
///////////////////////////////////////////////////////////////////////////////

DeferredGBuffer PS_decalFxDeferred(pixelInputDecalFx IN)
{
	float3 positionWorld;
	SurfaceProperties surfProps;
	StandardLightingProperties stdLightProps;
	if (PS_decalFxCommon(IN, positionWorld, surfProps, stdLightProps, false))
	{
		// pack the info into the g-buffer
		DeferredGBuffer OUT = PackGBuffer(surfProps.surface_worldNormal, stdLightProps);

		return OUT;
	}
	else
	{
		DeferredGBuffer OUT = (DeferredGBuffer)0;
		return OUT;
	}
}



///////////////////////////////////////////////////////////////////////////////
//  TECHNIQUES
///////////////////////////////////////////////////////////////////////////////

//#if FORWARD_TECHNIQUES
technique MSAA_NAME(draw)
{
	// ripples
	pass ripple
	{
		zFunc = FixedupLESS;
		ZEnable = true;
		ZWriteEnable = false;

		VertexShader = compile VERTEXSHADER VS_rippleFx();
		PixelShader  = compile MSAA_PIXEL_SHADER PS_rippleFx()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	// decals
	pass decal
	{

		VertexShader = compile VERTEXSHADER VS_decalFx();
		PixelShader  = compile MSAA_PIXEL_SHADER PS_decalFxForward_Textured_Eight()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}

technique  MSAA_NAME(lightweight0_draw)
{
	// ripples
	pass ripple
	{
		zFunc = FixedupLESS;
		ZEnable = true;
		ZWriteEnable = false;

		VertexShader = compile VERTEXSHADER VS_rippleFx();
		PixelShader  = compile MSAA_PIXEL_SHADER PS_rippleFx()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	// decals
	pass decal
	{

		VertexShader = compile VERTEXSHADER VS_decalFx();
		PixelShader  = compile MSAA_PIXEL_SHADER PS_decalFxForward_Textured_Zero()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}

technique  MSAA_NAME(lightweight4_draw)
{
	// ripples
	pass ripple
	{
		zFunc = FixedupLESS;
		ZEnable = true;
		ZWriteEnable = false;

		VertexShader = compile VERTEXSHADER VS_rippleFx();
		PixelShader  = compile MSAA_PIXEL_SHADER PS_rippleFx()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	// decals
	pass decal
	{

		VertexShader = compile VERTEXSHADER VS_decalFx();
		PixelShader  = compile MSAA_PIXEL_SHADER PS_decalFxForward_Textured_Four()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}

technique  MSAA_NAME(lightweight8_draw)
{
	// ripples
	pass ripple
	{
		zFunc = FixedupLESS;
		ZEnable = true;
		ZWriteEnable = false;

		VertexShader = compile VERTEXSHADER VS_rippleFx();
		PixelShader  = compile MSAA_PIXEL_SHADER PS_rippleFx()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	// decals
	pass decal
	{

		VertexShader = compile VERTEXSHADER VS_decalFx();
		PixelShader  = compile MSAA_PIXEL_SHADER PS_decalFxForward_Textured_Eight()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}

#if HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED
technique  MSAA_NAME(lightweightHighQuality0_draw)
{
	// ripples
	pass ripple
	{
		zFunc = FixedupLESS;
		ZEnable = true;
		ZWriteEnable = false;

		VertexShader = compile VERTEXSHADER VS_rippleFx();
		PixelShader  = compile MSAA_PIXEL_SHADER PS_rippleFx()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	// decals
	pass decal
	{

		VertexShader = compile VERTEXSHADER VS_decalFx();
		PixelShader  = compile MSAA_PIXEL_SHADER PS_decalFxForward_Textured_ZeroHighQuality()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}

technique MSAA_NAME(lightweightHighQuality4_draw)
{
	// ripples
	pass ripple
	{
		zFunc = FixedupLESS;
		ZEnable = true;
		ZWriteEnable = false;

		VertexShader = compile VERTEXSHADER VS_rippleFx();
		PixelShader  = compile MSAA_PIXEL_SHADER PS_rippleFx()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	// decals
	pass decal
	{

		VertexShader = compile VERTEXSHADER VS_decalFx();
		PixelShader  = compile MSAA_PIXEL_SHADER PS_decalFxForward_Textured_FourHighQuality()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}

technique MSAA_NAME(lightweightHighQuality8_draw)
{
	// ripples
	pass ripple
	{
		zFunc = FixedupLESS;
		ZEnable = true;
		ZWriteEnable = false;

		VertexShader = compile VERTEXSHADER VS_rippleFx();
		PixelShader  = compile MSAA_PIXEL_SHADER PS_rippleFx()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	// decals
	pass decal
	{

		VertexShader = compile VERTEXSHADER VS_decalFx();
		PixelShader  = compile MSAA_PIXEL_SHADER PS_decalFxForward_Textured_EightHighQuality()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
#endif // HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED
//#endif // FORWARD_TECHNIQUES

//#if DEFERRED_TECHNIQUES && NON_CUTOUT_DEFERRED_TECHNIQUES
technique MSAA_NAME(deferred_draw)
{
	// ripples
	pass MSAA_NAME(ripple)
	{
		VertexShader = compile VERTEXSHADER	VS_rippleFx();
		PixelShader  = compile MSAA_PIXEL_SHADER PS_rippleFx()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	// decals
	pass MSAA_NAME(decal)
	{
		VertexShader = compile VERTEXSHADER	VS_decalFx();
		PixelShader  = compile MSAA_PIXEL_SHADER PS_decalFxDeferred()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
//#endif // DEFERRED_TECHNIQUES && NON_CUTOUT_DEFERRED_TECHNIQUES

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
struct VertexInputTerrainGrid
{
	float3 pos			: POSITION;
	float3 normal		: NORMAL;
	float4 col			: COLOR0;
	float2 texCoords	: TEXCOORD0;
};



struct VertexOutputTerrainGrid
{
	DECLARE_POSITION(pos)
	float4 camToVert			: TEXCOORD0;
};

struct PixelInputTerrainGrid
{
	DECLARE_POSITION_PSIN(pos)
	float4 camToVert			: TEXCOORD0;
#if MULTISAMPLE_TECHNIQUES
	uint sampleIndex	: SV_SampleIndex;
#endif	//MULTISAMPLE_TECHNIQUES
};

BeginConstantBufferPagedDX10(terraingrid_cbuffer, b11)

float4 TerrainGridForward;
float4 TerrainGridRight;
float3 TerrainGridProjDir;
float3 TerrainGridCentrePos;
float4 TerrainGridParams;
float4 TerrainGridParams2;
float4 TerrainGridLowHeightCol;
float4 TerrainGridMidHeightCol;
float4 TerrainGridHighHeightCol;

#define TerrainGridRes							TerrainGridParams.x
#define TerrainGridColorMult					TerrainGridParams.y
#define TerrainGridRefHeight					TerrainGridParams.z
#define TerrainGridHeightMin					TerrainGridParams2.x
#define TerrainGridHeightMax					TerrainGridParams2.y
#define TerrainGridOORefHeightMinusMinHeight	TerrainGridParams2.z
#define TerrainGridOOMaxHeightMinusRefHeight	TerrainGridParams2.w

EndConstantBufferDX10(terraingrid_cbuffer)

//////////////////////////////////////////////////////////////////////////
//
void TerrainGridGetWorldPosFromVpos(float2 vPos, float4 eyeRay, uint sampleIndex, out float3 worldPos, out float stencil)
{
	float2 screenPos = convertToNormalizedScreenPos(vPos, deferredLightScreenSize);

#if __XENON
	float2 posXY=screenPos.xy;
	float	stencilSample;
	asm 
    { 
		tfetch2D stencilSample.z___, posXY.xy, GBufferStencilTextureSamplerGlobal
	};
	float depthSample;
	asm 
    { 
		tfetch2D depthSample.x___, posXY.xy, GBufferTextureSamplerDepthGlobal 
	};
	stencil = stencilSample;
#elif RSG_PC || RSG_DURANGO || RSG_ORBIS || __MAX
	float2 posXY = screenPos.xy;
#if MULTISAMPLE_TECHNIQUES
	const int3 iPos = getIntCoordsWithEyeRay(gbufferTextureDepthGlobal, screenPos, sampleIndex, gViewInverse, eyeRay.xyz, globalScreenSize.xy);
	float4 depthStencilSample = gbufferTextureDepthGlobal.Load(iPos, sampleIndex);
	float depthSample = depthStencilSample.x;
	stencil = getStencilValueScreenMS(gbufferStencilTextureGlobal, iPos, sampleIndex);
#else
	float4 depthStencilSample = tex2D(GBufferTextureSamplerDepthGlobal, posXY.xy);
	float depthSample = depthStencilSample.x;
#if __SHADERMODEL >= 40
	stencil = getStencilValueScreen(gbufferStencilTextureGlobal, screenPos * globalScreenSize);
#else
	stencil  = depthStencilSample.w;
#endif // __SHADERMODEL >= 40
#endif // MULTISAMPLE_TECHNIQUES
#else	// platforms
	// "Don't make me a half float, I need the extra precision" said posXY...
	float2 posXY = screenPos.xy;
	float4 depthStencilSample = northTexDepthStencil2D(GBufferTextureSamplerDepthGlobal, posXY.xy); //this is preferable because it goes directly to the function rather than via the shadows header
	float depthSample = depthStencilSample.x;
	stencil = depthStencilSample.w;
#endif	// platforms

	// Unpack the world space position from depth
	float linearDepth = getLinearGBufferDepth(depthSample, deferredProjectionParams.zw);

	// Normalize eye ray if needed
	eyeRay.xyz /= eyeRay.w;
	worldPos = gViewInverse[3].xyz + (eyeRay.xyz * linearDepth);
}

//////////////////////////////////////////////////////////////////////////
//
VertexOutputTerrainGrid VS_TerrainGrid(VertexInputTerrainGrid IN)
{
	VertexOutputTerrainGrid OUT = (VertexOutputTerrainGrid)0;

	// calculate screen position of vertex
	OUT.pos = mul(float4(IN.pos, 1), gWorldViewProj);
	// calculate the direction of this vertex
#ifdef NVSTEREO
	float3 StereorizedCamPos = gViewInverse[3].xyz + (StereoWorldCamOffSet());
	OUT.camToVert = float4((IN.pos.xyz-StereorizedCamPos), OUT.pos.w);
#else
	OUT.camToVert = float4((IN.pos.xyz-gViewInverse[3].xyz), OUT.pos.w);
#endif

	return OUT;
}


//////////////////////////////////////////////////////////////
//
float GetStencilMask(float stencil)
{
#if __SHADERMODEL < 40
	stencil = fmod(stencil, 8.0f/255.0f);
#endif
	stencil *= 255.0f;

	return (abs(stencil - (DEFERRED_MATERIAL_DEFAULT)) <= 0.1f) || 
		   (abs(stencil - (DEFERRED_MATERIAL_TERRAIN)) <= 0.1f);
}

//////////////////////////////////////////////////////////////////////////
//
half4 PS_TerrainGrid(PixelInputTerrainGrid IN) : COLOR
{
	// derive projection
	float2 vPos = IN.pos.xy;
	float3 worldPos;
	float stencil;
	TerrainGridGetWorldPosFromVpos(vPos, IN.camToVert, SAMPLE_INDEX, worldPos, stencil);
	float3 centerToCurPos = worldPos - TerrainGridCentrePos;


	float2 texUV = (	float2(	dot(TerrainGridRight.xyz, centerToCurPos.xyz), 
								dot(-TerrainGridForward.xyz, centerToCurPos.xyz) )
					*	float2(0.5f*TerrainGridRight.w, 0.5f*TerrainGridForward.w)) + 0.5f;

	half4 result = (half4)0;
	result.a = GetStencilMask(stencil) ? 1.0f : 0.0f;

	// alpha out any area outside 0...1 texture coords (avoids wrapping within the decal when viewed at an angle)
	if (texUV.x<0.0f) result.a = 0.0f;
	if (texUV.x>1.0f) result.a = 0.0f;
	if (texUV.y<0.0f) result.a = 0.0f;
	if (texUV.y>1.0f) result.a = 0.0f;


	const float2 gridRes = float2(TerrainGridRes, TerrainGridRes*(TerrainGridRight.w/TerrainGridForward.w));
	const float2 texCoords = texUV*gridRes;
	float4 diffCol = tex2D(gDecalSampler, texCoords);

	float2 edgeFade;
	const float2 oohGridRes =  (2/gridRes.xy);
	edgeFade = lerp(	(1 - saturate( (texUV.xy - (1.0f-oohGridRes))/oohGridRes ) ),
					(texUV.xy/oohGridRes),
					step(texUV.xy,oohGridRes) );


	result.a *= edgeFade.x*edgeFade.y;


	const half currentHeight = worldPos.z;
	const half t0 = saturate((currentHeight-TerrainGridHeightMin)*TerrainGridOORefHeightMinusMinHeight);
	const half t1 = saturate((currentHeight-TerrainGridRefHeight)*TerrainGridOOMaxHeightMinusRefHeight);

	half4 finalCol = (currentHeight < TerrainGridRefHeight) ?	lerp(TerrainGridLowHeightCol, TerrainGridMidHeightCol, t0) :
																lerp(TerrainGridMidHeightCol, TerrainGridHighHeightCol, t1);

	result.rgb = finalCol.rgb*TerrainGridColorMult;

	result.a *= diffCol.x * finalCol.a;

	return PackHdr(result);
}

//////////////////////////////////////////////////////////////////////////
//

technique MSAA_NAME(terrain_grid)
{
	pass MSAA_NAME(p0)
	{
		VertexShader = compile VERTEXSHADER VS_TerrainGrid();
		PixelShader  = compile MSAA_PIXEL_SHADER PS_TerrainGrid()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

}
