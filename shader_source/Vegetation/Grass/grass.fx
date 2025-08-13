
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse specular texcoord0 normal
	#define PRAGMA_DCL
#endif
//
// GTA grass shader:
//
//	30/06/2005	- Andrzej:	- initial;
//	26/04/2006	- Andrzej:	- support for instanced rendering added;
//	18/10/2006	- Andrzej:	- HDR multiply added;
//	02/02/2007	- Andrzej:	- Deferred mode support added;
//	26/08/2007	- Andrzej:	- GBuffer: alphablending added;
//	16/07/2008	- Andrzej:	- 2.0: micro-movements support added;
//	06/08/2008	- Andrzej:	- 2.0: collision for player's sphere added; IN.color0.a = collision scale (useful for flower tops, etc.);
//	22/10/2008	- Andrzej:	- 2.0: ground color modulation added;
//	28/10/2008	- Andrzej:	- 2.0: LOD0 and LOD1 support added;
//	30/10/2008	- Andrzej:	- 2.0: LOD2 support added;
//	31/10/2008	- Andrzej:	- 2.0: PSN: AlphaToMask() added;
//	06/11/2008	- Andrzej:	- 2.0: PSN: added two-pass rendering into GBuffer to allow to use hardware AlphaToMask() in GBuf0;
//	14/11/2008	- Andrzej:	- 2.0: PSN: revitalised old SPU renderloop;
//	26/11/2008	- Andrzej:	- 2.0: PSN: SpuRenderloop 2.0;
//	12/04/2009	- Andrzej:	- 2.0: tool technique;
//	22/11/2011	- Andrzej:	- 2.0: Improved CPLANT_STORE_LOCTRI_NORMAL
//
//
//
//
//
#define USE_SSA_STIPPLE_ALPHA				(0)
#define USE_SSA_STIPPLE_ALPHA_OFFSET		(0 && USE_SSA_STIPPLE_ALPHA)
#define WRITE_SELF_SHADOW_TERM				(0)
#define CPLANT_WRITE_GRASS_NORMAL			(0)
#define PS3_ALPHATOMASK_PASS				(1 && __PS3 && !CPLANT_WRITE_GRASS_NORMAL)
#define XENON_FILL_PASS						(1 && __XENON && !CPLANT_WRITE_GRASS_NORMAL)
#define CPLANT_STORE_LOCTRI_NORMAL			(0)				// 5:3 rgb+normal compression for single RT
#define PLANTS_CAST_SHADOWS					(1 && (__SHADERMODEL >= 40))
#define CPLANT_BLIT_MIN_GBUFFER             (__PS3) // Bind min number of GBuffer targets when doing the fakeBlit (mutually exclusive with CPLANT_STORE_LOCTRI_NORMAL)


#define PLAYER_COLL_RADIUS0			(0.75f)	// radius of influence
#define PLAYER_COLL_RADIUS_SQR0		(PLAYER_COLL_RADIUS0*PLAYER_COLL_RADIUS0)


#define USE_DIFFUSE_SAMPLER
#if !WRITE_SELF_SHADOW_TERM
	#define NO_SELF_SHADOW
#endif

#define GRASS_SHADER

#include "../../common.fxh"
#include "../../Util/skin.fxh"
#include "../../Util/camera.fxh"

#define SHADOW_CASTING            (0)
#define SHADOW_CASTING_TECHNIQUES (0)
#define SHADOW_RECEIVING          (1)
#define SHADOW_RECEIVING_VS       (0)
#include "../../Lighting/Shadows/localshadowglobals.fxh"
#include "../../Lighting/Shadows/cascadeshadows.fxh"
#include "../../Lighting/lighting.fxh"

#include "grass.fxh"
#include "grass_globals.fxh"

//#define USE_CAMERA_FACING
#ifdef USE_CAMERA_FACING
	#define CAMERA_FACING (1)
#else
	#define CAMERA_FACING (0)
#endif

//#define USE_CAMERA_ALIGNED
#ifdef USE_CAMERA_ALIGNED
	#define CAMERA_ALIGNED (1)
#else
	#define CAMERA_ALIGNED (0)
#endif

#define NEEDS_WORLD_POSITION (SHADOW_RECEIVING || USE_SSA_STIPPLE_ALPHA_OFFSET)


BEGIN_RAGE_CONSTANT_BUFFER(grasslocalsfrequent,b0)
ROW_MAJOR float4x4 matGrassTransform	: matGrassTransform0	REGISTER2(vs, GRASS_CREG_TRANSFORM);
float4	 plantColor			: plantColor0			REGISTER2(vs, GRASS_CREG_PLANTCOLOR);	// global plant color + alpha
float4	 groundColor		: groundColor0			REGISTER2(vs, GRASS_CREG_GROUNDCOLOR);	// global ground color [R | G | B | 1.0f]
EndConstantBufferDX10(grasslocalsfrequent)

#if GRASS_INSTANCING
	#define matGrassTransformD	matGrassTransform_local
	#define plantColorD			plantColor_local
	#define groundColorD		groundColor_local
#else // not GRASS_INSTANCING
	#define matGrassTransformD	matGrassTransform
	#define plantColorD			plantColor
	#define groundColorD		groundColor
#endif // not GRASS_INSTANCING

BEGIN_RAGE_CONSTANT_BUFFER(grasslocals,b2)


//
//
//
//
float3	vecCameraPos	: vecCameraPos0 REGISTER2(vs, GRASS_CREG_CAMERAPOS)	= float3(0,0,0);	// xyz = camera pos
float3	vecPlayerPos	: vecPlayerPos0 REGISTER2(vs, GRASS_CREG_PLAYERPOS)	= float3(0,0,0);	// xyz = current player pos


float4  _dimensionLOD2	: dimensionLOD20 REGISTER2(vs,GRASS_CREG_DIMENSIONLOD2)	= float4(0,0,0,0);	// xy = LOD.wh, zw=0
#if GRASS_INSTANCING
#define WidthLOD2		(_dimensionLOD2_local.x)	// LOD2: billboard width scale
#define HeightLOD2		(_dimensionLOD2_local.y)	// LOD2: billboard height scale
#define IsFarFadeLOD2	(_dimensionLOD2_local.z)	// LOD2: uses far fade: 0 or 1
#else // not GRASS_INSTANCING
#define WidthLOD2		(_dimensionLOD2.x)	// LOD2: billboard width scale
#define HeightLOD2		(_dimensionLOD2.y)	// LOD2: billboard height scale
#define IsFarFadeLOD2	(_dimensionLOD2.z)	// LOD2: uses far fade: 0 or 1
#endif // not GRASS_INSTANCING

// #define	GBuf0AlphaScale						(1.8f)

#if CPLANT_WRITE_GRASS_NORMAL
	float3 gView_Light_NormalScaler_AO	: gView_Light_NormalScaler_AO = float2(0.999f, 0.999f, 1.0f);
	#define gViewDirNormalScaler		(gView_Light_NormalScaler_AO.x)		// x = scaler value for view based offset for normal
	#define gLightDirNormalScaler		(gView_Light_NormalScaler_AO.y)		// y = scaler value for primary light direction based offset for normal
#endif


//
// ped collision params:
// x = coll sphere radius sqr
// y = inv of coll sphere radius sqr
//
float2	_vecCollParams	: vecCollParams0 REGISTER2(vs,GRASS_CREG_PLAYERCOLLPARAMS)	= float2(PLAYER_COLL_RADIUS_SQR0, 1.0f/PLAYER_COLL_RADIUS_SQR0);
#if GRASS_INSTANCING
#define collPedSphereRadiusSqr		(_vecCollParams_local.x)		// x = coll sphere radius sqr
#define collPedInvSphereRadiusSqr	(_vecCollParams_local.y)		// y = inv of coll sphere radius sqr
#else // not GRASS_INSTANCING
#define collPedSphereRadiusSqr		(_vecCollParams.x)		// x = coll sphere radius sqr
#define collPedInvSphereRadiusSqr	(_vecCollParams.y)		// y = inv of coll sphere radius sqr
#endif // not GRASS_INSTANCING

// vehicle collision params:
#define VEHCOL_DIST_LIMIT			(2.0f)	// skip vehicle deformation if plant is too far away from the vehicle/ground
// vehicle#0:
float4		_vecVehColl0B	: vecVehColl0B0 REGISTER2(vs,GRASS_CREG_VEHCOLL0B) = float4(0,0,0,0);	// [ segment start point B.xyz				| 0				]
float4		_vecVehColl0M	: vecVehColl0M0 REGISTER2(vs,GRASS_CREG_VEHCOLL0M) = float4(0,0,0,0);	// [ segment vector M.xyz					| 1.0f/dot(M,M) ]
float4		_vecVehColl0R	: vecVehColl0R0 REGISTER2(vs,GRASS_CREG_VEHCOLL0R) = float4(0,0,0,0);	// [ R	      | R*R          | 1.0f/(R*R)   | worldPosZ		]
#define coll0VehVecB			(_vecVehColl0B.xyz)	// start segment point B
#define coll0VehVecM			(_vecVehColl0M.xyz)	// segment vector M
#define coll0VehInvDotMM		(_vecVehColl0M.w)	// 1/dot(M,M)
#define coll0VehR				(_vecVehColl0R.x)	// collision radius
#define coll0VehRR				(_vecVehColl0R.y)	// R*R
#define coll0VehInvRR			(_vecVehColl0R.z)	// 1/(R*R)
#define coll0VehGroundZ			(_vecVehColl0R.w)
// vehicle#1:
float4		_vecVehColl1B	: vecVehColl1B0 REGISTER2(vs,GRASS_CREG_VEHCOLL1B) = float4(0,0,0,0);	// [ segment start point B.xyz				| 0				]
float4		_vecVehColl1M	: vecVehColl1M0 REGISTER2(vs,GRASS_CREG_VEHCOLL1M) = float4(0,0,0,0);	// [ segment vector M.xyz					| 1.0f/dot(M,M) ]
float4		_vecVehColl1R	: vecVehColl1R0 REGISTER2(vs,GRASS_CREG_VEHCOLL1R) = float4(0,0,0,0);	// [ R	      | R*R          | 1.0f/(R*R)   | worldPosZ		]
#define coll1VehVecB			(_vecVehColl1B.xyz)	// start segment point B
#define coll1VehVecM			(_vecVehColl1M.xyz)	// segment vector M
#define coll1VehInvDotMM		(_vecVehColl1M.w)	// 1/dot(M,M)
#define coll1VehR				(_vecVehColl1R.x)	// collision radius
#define coll1VehRR				(_vecVehColl1R.y)	// R*R
#define coll1VehInvRR			(_vecVehColl1R.z)	// 1/(R*R)
#define coll1VehGroundZ			(_vecVehColl1R.w)
// vehicle#2:
float4		_vecVehColl2B	: vecVehColl2B0 REGISTER2(vs,GRASS_CREG_VEHCOLL2B) = float4(0,0,0,0);	// [ segment start point B.xyz				| 0				]
float4		_vecVehColl2M	: vecVehColl2M0 REGISTER2(vs,GRASS_CREG_VEHCOLL2M) = float4(0,0,0,0);	// [ segment vector M.xyz					| 1.0f/dot(M,M) ]
float4		_vecVehColl2R	: vecVehColl2R0 REGISTER2(vs,GRASS_CREG_VEHCOLL2R) = float4(0,0,0,0);	// [ R	      | R*R          | 1.0f/(R*R)   | worldPosZ		]
#define coll2VehVecB			(_vecVehColl2B.xyz)	// start segment point B
#define coll2VehVecM			(_vecVehColl2M.xyz)	// segment vector M
#define coll2VehInvDotMM		(_vecVehColl2M.w)	// 1/dot(M,M)
#define coll2VehR				(_vecVehColl2R.x)	// collision radius
#define coll2VehRR				(_vecVehColl2R.y)	// R*R
#define coll2VehInvRR			(_vecVehColl2R.z)	// 1/(R*R)
#define coll2VehGroundZ			(_vecVehColl2R.w)
// vehicle#3:
float4		_vecVehColl3B	: vecVehColl3B0 REGISTER2(vs,GRASS_CREG_VEHCOLL3B) = float4(0,0,0,0);	// [ segment start point B.xyz				| 0				]
float4		_vecVehColl3M	: vecVehColl3M0 REGISTER2(vs,GRASS_CREG_VEHCOLL3M) = float4(0,0,0,0);	// [ segment vector M.xyz					| 1.0f/dot(M,M) ]
float4		_vecVehColl3R	: vecVehColl3R0 REGISTER2(vs,GRASS_CREG_VEHCOLL3R) = float4(0,0,0,0);	// [ R	      | R*R          | 1.0f/(R*R)   | worldPosZ		]
#define coll3VehVecB			(_vecVehColl3B.xyz)	// start segment point B
#define coll3VehVecM			(_vecVehColl3M.xyz)	// segment vector M
#define coll3VehInvDotMM		(_vecVehColl3M.w)	// 1/dot(M,M)
#define coll3VehR				(_vecVehColl3R.x)	// collision radius
#define coll3VehRR				(_vecVehColl3R.y)	// R*R
#define coll3VehInvRR			(_vecVehColl3R.z)	// 1/(R*R)
#define coll3VehGroundZ			(_vecVehColl3R.w)


float4	fadeAlphaDistUmTimer	: fadeAlphaDistUmTimer0 REGISTER2(vs,GRASS_CREG_ALPHALOD0DISTUMTIMER);	// [ (f)/(f-c) | (-1.0)/(f-c) | umTimer | ? ];
#define fadeAlphaLOD0Dist0		(fadeAlphaDistUmTimer.x)	// (f)/(f-c)
#define fadeAlphaLOD0Dist1		(fadeAlphaDistUmTimer.y)	// (-1.0)/(f-c)
#define umGlobalTimer			(fadeAlphaDistUmTimer.z)	// micro-movement timer

float4	fadeAlphaLOD1Dist		: fadeAlphaLOD1Dist0	REGISTER2(vs,GRASS_CREG_ALPHALOD1DIST);	// [ (f)/(f-c) | (-1.0)/(f-c) | umTimer | ? ];
#define fadeAlpha0LOD1Dist0		(fadeAlphaLOD1Dist.x)	// (f)/(f-c)
#define fadeAlpha0LOD1Dist1		(fadeAlphaLOD1Dist.y)	// (-1.0)/(f-c)
#define fadeAlpha1LOD1Dist0		(fadeAlphaLOD1Dist.z)	// (f)/(f-c)
#define fadeAlpha1LOD1Dist1		(fadeAlphaLOD1Dist.w)	// (-1.0)/(f-c)

float4	fadeAlphaLOD2Dist		: fadeAlphaLOD2Dist0	REGISTER2(vs,GRASS_CREG_ALPHALOD2DIST);	// [ (f)/(f-c) | (-1.0)/(f-c) | umTimer | ? ];
#define fadeAlpha0LOD2Dist0		(fadeAlphaLOD2Dist.x)	// (f)/(f-c)
#define fadeAlpha0LOD2Dist1		(fadeAlphaLOD2Dist.y)	// (-1.0)/(f-c)
#define fadeAlpha1LOD2Dist0		(fadeAlphaLOD2Dist.z)	// (f)/(f-c)
#define fadeAlpha1LOD2Dist1		(fadeAlphaLOD2Dist.w)	// (-1.0)/(f-c)


float4	fadeAlphaLOD2DistFar0	: fadeAlphaLOD2DistFar0	REGISTER2(vs,GRASS_CREG_ALPHALOD2DIST_FAR);	// [ (f)/(f-c) | (-1.0)/(f-c) | umTimer | ? ];
#define fadeAlpha0LOD2DistFar0	(fadeAlphaLOD2DistFar0.x)	// (f)/(f-c)
#define fadeAlpha0LOD2DistFar1	(fadeAlphaLOD2DistFar0.y)	// (-1.0)/(f-c)

// micro-movement params: [globalScaleH | globalScaleV | globalArgFreqH | globalArgFreqV ]
float4	uMovementParams			: uMovementParams0		REGISTER2(vs,GRASS_CREG_UMPARAMS)	= float4(0.05f, 0.05f, 0.2125f, 0.2125f);

#if GRASS_INSTANCING
#define umGlobalScaleH			(uMovementParams_local.x)		// micro-movement: globalScaleHorizontal
#define umGlobalScaleV			(uMovementParams_local.y)		// micro-movement: globalScaleVertical
#define umGlobalArgFreqH		(uMovementParams_local.z)		// micro-movement: globalArgFreqHorizontal
#define umGlobalArgFreqV		(uMovementParams_local.w)		// micro-movement: globalArgFreqVertical
#else // not GRASS_INSTANCING
#define umGlobalScaleH			(uMovementParams.x)		// micro-movement: globalScaleHorizontal
#define umGlobalScaleV			(uMovementParams.y)		// micro-movement: globalScaleVertical
#define umGlobalArgFreqH		(uMovementParams.z)		// micro-movement: globalArgFreqHorizontal
#define umGlobalArgFreqV		(uMovementParams.w)		// micro-movement: globalArgFreqVertical
#endif // not GRASS_INSTANCING


#if __PS3
float4 _fakedGrassNormal		: fakedGrassNormal0		REGISTER2(vs,GRASS_CREG_FAKEDNORMAL)
#else
float4 _fakedGrassNormal		: fakedGrassNormal0		REGISTER(GRASS_CREG_FAKEDNORMAL)
#endif
<
	int nostrip=1;
> = float4(0,0,1,1);
#if CPLANT_WRITE_GRASS_NORMAL
	#define fakedGrassNormal		(-gDirectionalLight.xyz)
#else
	#define fakedGrassNormal		(_fakedGrassNormal.xyz)
#endif

#define globalAO					(_fakedGrassNormal.w)

#if CPLANT_WRITE_GRASS_NORMAL
	float4 _terrainNormal	: terrainNormal0		REGISTER2(vs, GRASS_CREG_TERRAINNORMAL);
	#define terrainNormal	(_terrainNormal.xyz)
#endif

#if USE_SSA_STIPPLE_ALPHA
	float2 gStippleThreshold	: gStippleThreshold = float2(0.5, 0.15);
#endif

float AlphaScale : AlphaScale
<
	string UIWidget = "slider";
	float UIMin = 0.0;
	float UIMax = 100.0;
	float UIStep = .01;
	string UIName = "Alpha Scale Factor";
> = {1.0};

float AlphaTest : AlphaTest
<
	string UIWidget = "slider";
	float UIMin = -100.0;
	float UIMax = 100.0;
	float UIStep = .01;
	string UIName = "Alpha Test Ref";
> = {0.0};

float ShadowFalloff : ShadowFalloff
<
	string UIWidget = "slider";
	float UIMin = 0.0;
	float UIMax = 1.0;
	float UIStep = .01;
	string UIName = "Shadow Fall off";
> = {1.0};

float gAlphaToCoverageScale	: gAlphaToCoverageScale
<
	string UIName = "Alpha multiplier for MSAA coverage";
	float UIMin = 0.0;
	float UIMax = 5.0;
	float UIStep = 0.1;
	int nostrip	= 1;
> = 1.3f;

EndConstantBufferDX10(grasslocals)


#if GRASS_INSTANCING_TRANSPORT_CONSTANT_BUFFERS

CBSHARED BeginConstantBufferPagedDX10(grassinstances, b5)
#pragma constant 0
shared float4 instanceData[GRASS_INSTANCING_INSTANCE_SIZE*GRASS_INSTANCING_BUCKET_SIZE] : instanceData0;
EndConstantBufferDX10(grassinstances)

#endif // GRASS_INSTANCING_TRANSPORT_CONSTANT_BUFFERS

//
//
//
//
BeginSampler(sampler,		grassTexture,	TextureGrassSampler,	grassTexture0)
//	string UIName = "Grass Texture";
ContinueSampler(sampler,	grassTexture,	TextureGrassSampler,	grassTexture0)
    AddressU  = WRAP;        
    AddressV  = WRAP;
    AddressW  = WRAP;
    MIPFILTER = HQLINEAR;
    MINFILTER = LINEAR;
    MAGFILTER = MAGLINEAR;
EndSampler;


#if CPLANT_STORE_LOCTRI_NORMAL
BeginSampler(	sampler, gbufferTexture0, GBufferTextureSampler0, gbufferTexture0)
ContinueSampler(sampler, gbufferTexture0, GBufferTextureSampler0, gbufferTexture0)
	AddressU  = CLAMP;        
	AddressV  = CLAMP;
	MINFILTER = POINT;
	MAGFILTER = POINT;
	MIPFILTER = POINT;
EndSampler;
#endif


#if GRASS_INSTANCING_TRANSPORT_TEXTURE
BeginDX10Sampler(	sampler, Texture2D<float4>, instanceTexture0, InstanceSampler0, instanceTexture0)
ContinueSampler(sampler, instanceTexture0, InstanceSampler0, instanceTexture0)
	AddressU  = CLAMP;        
	AddressV  = CLAMP;
	MINFILTER = POINT;
	MAGFILTER = POINT;
	MIPFILTER = POINT;
EndSampler;
#endif // GRASS_INSTANCING_TRANSPORT_TEXTURE

#if 0	//__PS3
BeginSampler(sampler2D,stippleTexture,StippleSampler,StippleTex)
//	int nostrip=1;	// dont strip
ContinueSampler(sampler2D,stippleTexture,StippleSampler,StippleTex)
	AddressU  = CLAMP;
	AddressV  = CLAMP;
	AddressW  = CLAMP;
	MIPFILTER = LINEAR;
	MINFILTER = HQLINEAR;
	MAGFILTER = MAGLINEAR; 
EndSampler;

//
//
//
//
float4 AlphaToMaskTex(float fStippleAlpha, float2 vPos)
{
	float fAlpha = saturate(fStippleAlpha) * 0.999f;
//	float fAlpha = saturate(fStippleAlpha-AlphaTest) * 0.999f;	// shift by alpharef to use full range of stippletex
	
	

// stipple texture is 32x32, contains 8x8 stipple patterns
//const float fSubTileSizeInTexels	= 8.0f;
//const float fTextureScaleToPixels	= 1.0f/32.0f;

// stipple texture is 64x64, contains 16x16 stipple patterns
const float fSubTileSizeInTexels	= 16.0f;
const float fTextureScaleToPixels	= 1.0f/64.0f;


	// get tile location based on alpha value:
float2 fStippleSample;
//	fStippleSample.x = fmod(fAlpha, 0.25f) * 4.0f;
//	fStippleSample.y = fAlpha;
	fStippleSample.x = floor(fmod(fAlpha, 0.25f) * 16.0f) / 4.0f;
	fStippleSample.y = floor(fAlpha * 4.0f) / 4.0f;

	// get subtile position based on screenpos:
	float2 fStippleSubSample = fmod(vPos, fSubTileSizeInTexels) * fTextureScaleToPixels;

	fStippleSample += fStippleSubSample;

	return tex2D(StippleSampler, fStippleSample.xy);
}

bool AlphaToMask(float fStippleAlpha, float2 vPos)
{
	float fStippleVal = AlphaToMaskTex(fStippleAlpha, vPos).r;
	// kill pixel?
	return (fStippleVal > 0.0f)? false : true;
}
#endif //__PS3...

//
//
//
//
void ConvertToCoverage(inout float alpha)
{
	// has to leave alpha unchanged if no MSAA enabled
#if __SHADERMODEL >= 40
	alpha *= gAlphaToCoverageScale;
#endif
}

//
//
//
//
struct vertexInputUnlit
{
	float4 pos		        : POSITION;
	float3 normal			: NORMAL;
	float4 color0			: COLOR0;
	float4 color1			: COLOR1;
	float2 texCoord0        : TEXCOORD0;

#if CAMERA_FACING || CAMERA_ALIGNED
	float4 RelativeCentre	: TANGENT1;
#endif
};

struct vertexInputUnlitLOD2
{
	float2 uvs					: TEXCOORD0;
#if GRASS_LOD2_BATCHING
	float4 t1GrassTransform0	: TEXCOORD1;
	float4 t2GrassTransform1	: TEXCOORD2;
	float4 t3GrassTransform2	: TEXCOORD3;
	float4 t4GrassTransform3	: TEXCOORD4;
	float4 t5PlantColor			: TEXCOORD5;
	float4 t6GroundColor		: TEXCOORD6;
#endif

#if CAMERA_FACING || CAMERA_ALIGNED
	float4 RelativeCentre	: TANGENT1;
#endif
};


struct vertexOutputUnlit
{
    DECLARE_POSITION(pos)
	float4	color0			: TEXCOORD0;
	float4	worldNormal		: TEXCOORD1;	// worldNormal.w = UV0
#if NEEDS_WORLD_POSITION
	float4	worldPos		: TEXCOORD2;	// worldPos.w = UV1
#endif
	float4	groundColor0	: TEXCOORD3;	// groundColor(RGB) + groundBlend
#if CPLANT_WRITE_GRASS_NORMAL || USE_SSA_STIPPLE_ALPHA
	float4	selfShadow_AO_SSAThresholds	: TEXCOORD4;	// Light Dir & global AO
#endif
	float4	ambient0		: TEXCOORD5;	// x=ambient, yzw= free
};


struct vertexOutputShadow
{
    DECLARE_POSITION(pos)
	float2	uv				: TEXCOORD0;
};


#if GRASS_INSTANCING
float4 UnpackColour(float c)
{
#if 1
	uint col = asuint(c);

	// Color32 = AARRGGBB
	float4 ret;
	ret.x = (col >>16)	& 0x000000ff;
	ret.y = (col >> 8)	& 0x000000ff;
	ret.z = (col     )	& 0x000000ff;
	ret.w =	(col >>24)	& 0x000000ff;

	return ret / 255.0f;
#else
	float4 ret;
	float fractional;

	fractional = modf(c/(256*256*256), ret.w);
	c -= ret.w*(256*256*256);

	fractional = modf(c/(256*256), ret.x);
	c -= ret.x*(256*256);

	fractional = modf(c/(256), ret.y);
	c -= ret.y*256;

	ret.z = c;

	return (ret/255.0f);
#endif
}

void GetGrassInstanceData(uint instanceID, out float4x4 matGrassTransform_out, out float4 plantColour_out, out float4 groundColour_out, out float4 umParams_out, out float2 collParams_out, out float ambientScale_out)
{
#if GRASS_INSTANCING_TRANSPORT_TEXTURE	
	uint x = instanceID & ((0x1 << GRASS_INSTANCING_TEXTURE_WIDTH_IN_INSTANCES_POWER_OF_2) - 1);
	uint y = instanceID >> GRASS_INSTANCING_TEXTURE_WIDTH_IN_INSTANCES_POWER_OF_2;
	int3 c = int3(x*5, y, 0);

	matGrassTransform_out[0] = instanceTexture0.Load(c + int3(0, 0, 0));
	matGrassTransform_out[1] = instanceTexture0.Load(c + int3(1, 0, 0));
	matGrassTransform_out[2] = instanceTexture0.Load(c + int3(2, 0, 0));
	matGrassTransform_out[3] = instanceTexture0.Load(c + int3(3, 0, 0));
	umParams_out			 = instanceTexture0.Load(c + int3(4, 0, 0));
#endif // GRASS_INSTANCING_TRANSPORT_TEXTURE

#if GRASS_INSTANCING_TRANSPORT_CONSTANT_BUFFERS
	matGrassTransform_out[0] = instanceData[instanceID*GRASS_INSTANCING_INSTANCE_SIZE + 0];
	matGrassTransform_out[1] = instanceData[instanceID*GRASS_INSTANCING_INSTANCE_SIZE + 1];
	matGrassTransform_out[2] = instanceData[instanceID*GRASS_INSTANCING_INSTANCE_SIZE + 2];
	matGrassTransform_out[3] = instanceData[instanceID*GRASS_INSTANCING_INSTANCE_SIZE + 3];
	umParams_out			 = instanceData[instanceID*GRASS_INSTANCING_INSTANCE_SIZE + 4];
#endif // GRASS_INSTANCING_TRANSPORT_CONSTANT_BUFFERS

	plantColour_out	 = UnpackColour(matGrassTransform_out[0].w);
	groundColour_out = UnpackColour(matGrassTransform_out[1].w);

	ambientScale_out = groundColour_out.w;
	groundColour_out.w = 1.0f;

	collParams_out	 = float2(matGrassTransform_out[2].w, matGrassTransform_out[3].w);
	matGrassTransform_out[0].w = 0.0f;
	matGrassTransform_out[1].w = 0.0f;
	matGrassTransform_out[2].w = 0.0f;
	matGrassTransform_out[3].w = 1.0f;
}

void GetGrassInstanceDataShadow(uint instanceID, out float4x4 matGrassTransform_out, out float4 umParams_out, out float2 collParams_out)
{
#if GRASS_INSTANCING_TRANSPORT_TEXTURE	
	uint x = instanceID & ((0x1 << GRASS_INSTANCING_TEXTURE_WIDTH_IN_INSTANCES_POWER_OF_2) - 1);
	uint y = instanceID >> GRASS_INSTANCING_TEXTURE_WIDTH_IN_INSTANCES_POWER_OF_2;
	int3 c = int3(x*5, y, 0);

	matGrassTransform_out[0] = instanceTexture0.Load(c + int3(0, 0, 0));
	matGrassTransform_out[1] = instanceTexture0.Load(c + int3(1, 0, 0));
	matGrassTransform_out[2] = instanceTexture0.Load(c + int3(2, 0, 0));
	matGrassTransform_out[3] = instanceTexture0.Load(c + int3(3, 0, 0));
	umParams_out			 = instanceTexture0.Load(c + int3(4, 0, 0));
#endif // GRASS_INSTANCING_TRANSPORT_TEXTURE

#if GRASS_INSTANCING_TRANSPORT_CONSTANT_BUFFERS
	matGrassTransform_out[0] = instanceData[instanceID*GRASS_INSTANCING_INSTANCE_SIZE + 0];
	matGrassTransform_out[1] = instanceData[instanceID*GRASS_INSTANCING_INSTANCE_SIZE + 1];
	matGrassTransform_out[2] = instanceData[instanceID*GRASS_INSTANCING_INSTANCE_SIZE + 2];
	matGrassTransform_out[3] = instanceData[instanceID*GRASS_INSTANCING_INSTANCE_SIZE + 3];
	umParams_out			 = instanceData[instanceID*GRASS_INSTANCING_INSTANCE_SIZE + 4];
#endif // GRASS_INSTANCING_TRANSPORT_CONSTANT_BUFFERS
	collParams_out	 = float2(matGrassTransform_out[2].w, matGrassTransform_out[3].w);
	matGrassTransform_out[0].w = 0.0f;
	matGrassTransform_out[1].w = 0.0f;
	matGrassTransform_out[2].w = 0.0f;
	matGrassTransform_out[3].w = 1.0f;
}

#endif // GRASS_INSTANCING

//
//
//
//
#if CPLANT_WRITE_GRASS_NORMAL
float3 ComputeGrassNormal(float3 worldNormal, float3 toLightDir, float3 worldPos, float isTop)
{
	float3 invViewVec = normalize(float3(gViewInverse[3].xy - worldPos.xy, 0.0f));

	//Compute normal
	float3 normal = normalize(worldNormal + (gViewDirNormalScaler * invViewVec));
	normal = normalize(lerp(normal, normal + (gLightDirNormalScaler * toLightDir), isTop)); //sqrt(isTop)));

	return normal;
}
#endif //CPLANT_WRITE_GRASS_NORMAL

float3 VS_ApplyVehicleCollision(float3 vertpos, /*vecB*/float3 B, /*vecM*/float3 M, /*1/dot(M,M)*/float invDotMM,
												/*collision radius*/float R, /*R*R*/float RR, /*1/(R*R)*/float invRR, float GroundZ, float collScale)
{
	if(abs(vertpos.z-GroundZ) > VEHCOL_DIST_LIMIT)
	{
		return(vertpos);
	}

	float3 P		= vertpos.xyz;

	float2 PB = P.xy-B.xy;
	float t0 = dot(M.xy, PB.xy);
	t0 *= invDotMM;

	//t0 = max(t0, 0.0f);
	//t0 = min(t0, 1.0f);
	t0 = saturate(t0);

	float2 P0 = (B.xy + t0*M.xy);	// P0 = image of P on the segment

	float2 vecDist2 = P.xy - P0.xy;
	float dist2 = dot(vecDist2, vecDist2);


#define INNER_RADIUS		(R*0.50f)	// where grass is put down
#define INNER_RADIUS2		(INNER_RADIUS*INNER_RADIUS)

// version 1: vertpos: xy + z,	OUTER_RADIUS= R*1.00f
// version 2: vertpos: z,		OUTER_RADIUS= R*1.25f

	//float coll = (OUTER_RADIUS2 - dist2) * invRR;	// / OUTER_RADIUS2;
	float coll = (RR - dist2) * invRR;
	//coll = max(coll, 0.0f);
	//coll = min(coll, 1.0f);
	coll = saturate(coll);

#if 1
	// v1: xy + z
	vertpos.xy += 1.0f * coll * R * normalize(vecDist2).xy * collScale;
	vertpos.z   = lerp(vertpos.z, GroundZ, coll);
#else
	// v2: z
	vertpos.z   = lerp(vertpos.z, GroundZ, coll);
#endif

	if(dist2 < INNER_RADIUS2)
	{
		// put grass down:
		vertpos.z = GroundZ-0.25f;
	}

	return(vertpos.xyz);
}


//
// ******************************
//	DRAW METHODS
// ******************************
//
//
//
//
//
#if GRASS_INSTANCING
vertexOutputUnlit VS_Transform_REC(vertexInputUnlit IN, uint instanceID : SV_InstanceID)
#else // not GRASS_INSTANCING
vertexOutputUnlit VS_Transform_REC(vertexInputUnlit IN)
#endif // not GRASS_INSTANCING
{
vertexOutputUnlit OUT;

	float ambientScale_local = 1.0f;
#if GRASS_INSTANCING
	float4x4 matGrassTransform_local;
	float4 plantColor_local;
	float4 groundColor_local;
	float4 uMovementParams_local;
	float2 _vecCollParams_local;
	GetGrassInstanceData(instanceID, matGrassTransform_local, plantColor_local, groundColor_local, uMovementParams_local, _vecCollParams_local, ambientScale_local);
#endif // GRASS_INSTANCING

    float4 vertColor	= plantColorD;
    // Write out final position & texture coords
	float3	vertpos		= (float3)IN.pos;
	float3  localNormal	= IN.normal;
    float2	localUV0	= IN.texCoord0;

	float3	worldNormal = //fakedGrassNormal;		// faked normal
						normalize(mul(float4(localNormal,0),	(float4x3)matGrassTransformD)); 

#if CAMERA_FACING || CAMERA_ALIGNED
	const float3 vCentre = IN.RelativeCentre.xyz;
	ApplyCameraAlignment(vertpos, worldNormal, vCentre);
#endif

	float4 IN_Color0 = IN.color0;	// used fields: rgb 
	float4 IN_Color1 = IN.color1;	// used fields: rba

	// local micro-movements of plants:
	float umScaleH	= IN_Color0.r		* umGlobalScaleH;	// horizontal movement scale (red0:   255=max, 0=min)
	float umScaleV	= (1.0f-IN_Color0.b)* umGlobalScaleV;	// vertical movement scale	 (blue0:  0=max, 255=min)
	float umArgPhase= IN_Color0.g		* 2.0f*PI;		// phase shift               (green0: phase 0-1     )

	float3 uMovementArg			= float3(umGlobalTimer, umGlobalTimer, umGlobalTimer);
	float3 uMovementArgPhase	= float3(umArgPhase, umArgPhase, umArgPhase);
	float3 uMovementScaleXYZ	= float3(umScaleH, umScaleH, umScaleV);

	uMovementArg			*= float3(umGlobalArgFreqH, umGlobalArgFreqH, umGlobalArgFreqV);
	uMovementArg			+= uMovementArgPhase;
	float3 uMovementAdd		=  sin(uMovementArg);
	uMovementAdd			*= uMovementScaleXYZ;

	// add final micro-movement:
	vertpos.xyz				+= uMovementAdd;


	// do local->global transform for this plant:
	float4	worldPos	= mul(float4(vertpos,1.0f), matGrassTransformD);	
	vertpos				= (float3)worldPos;

	// player's collision:
	if(1)
	{
		float3 vecDist3 = vertpos.xyz - vecPlayerPos.xyz;

		float distc2	= dot(vecDist3, vecDist3);
		float coll		= (collPedSphereRadiusSqr - distc2) * (collPedInvSphereRadiusSqr);

		//coll = max(coll, 0.0f);
		//coll = min(coll, 1.0f);
		coll = saturate(coll);

//		vertpos.xyz += coll*normalize(vecDist3)  * (IN_Color1.b);
		vertpos.xy += (float2)0.5f*coll*/*normalize*/(vecDist3).xy * (IN_Color1.b);		// blue1: scale for collision (useful for flower tops, etc.)
//		vertpos.z -= abs((float)coll*normalize(vecDist3).z * (1.0f-IN.color0.b));
	}


	// vehicle collision:
	if(bVehColl0Enabled)
	{
		vertpos.xyz = VS_ApplyVehicleCollision(vertpos, coll0VehVecB, coll0VehVecM, coll0VehInvDotMM, coll0VehR, coll0VehRR, coll0VehInvRR, coll0VehGroundZ, IN_Color1.b);
	}
	
	if(bVehColl1Enabled)
	{
		vertpos.xyz = VS_ApplyVehicleCollision(vertpos, coll1VehVecB, coll1VehVecM, coll1VehInvDotMM, coll1VehR, coll1VehRR, coll1VehInvRR, coll1VehGroundZ, IN_Color1.b);
	}

	if(bVehColl2Enabled)
	{
		vertpos.xyz = VS_ApplyVehicleCollision(vertpos, coll2VehVecB, coll2VehVecM, coll2VehInvDotMM, coll2VehR, coll2VehRR, coll2VehInvRR, coll2VehGroundZ, IN_Color1.b);
	}

	if(bVehColl3Enabled)
	{
		vertpos.xyz = VS_ApplyVehicleCollision(vertpos, coll3VehVecB, coll3VehVecM, coll3VehInvDotMM, coll3VehR, coll3VehRR, coll3VehInvRR, coll3VehGroundZ, IN_Color1.b);
	}

    float3 vecDist3	= vertpos - vecCameraPos;

    // calculate alpha fading:
    float2 vecDist	= vecDist3.xy;
#if 1
	// square distance approach (fading stops too rapidly):
    float  dist2	= dot(vecDist, vecDist);
#else
	// linear distance approach:
    float  dist2	= sqrt(dot(vecDist, vecDist));
#endif
    
/*
   f2      dist2
------- - -------
 f2-c2     f2-c2
*/
	float alphaFade = saturate(dist2 * fadeAlphaLOD0Dist1 + fadeAlphaLOD0Dist0);
	
	// final alpha:    
    float theAlpha		= vertColor.w * alphaFade; // * AlphaScale; // GBuf0AlphaScale;
	ConvertToCoverage( theAlpha );

    OUT.pos				= mul(float4(vertpos,1.0f),	gWorldViewProj);
    OUT.worldNormal.xyz	= worldNormal.xyz;
    OUT.worldNormal.w	= localUV0.x;
	OUT.color0			= float4(vertColor.rgb, theAlpha);
#if NEEDS_WORLD_POSITION
	OUT.worldPos.xyz	= worldPos.xyz;
	OUT.worldPos.w		= localUV0.y;
#endif
	OUT.ambient0		= 0;
	OUT.ambient0.x		= ambientScale_local;

	//Top vs Bottom of plant;
	float isTop = IN_Color1.a;
#if CPLANT_WRITE_GRASS_NORMAL || USE_SSA_STIPPLE_ALPHA
	float selfShadow = saturate(dot(worldNormal.xyz, -fakedGrassNormal) * 8.);
	float plantAO = lerp(globalAO, 1.0f, isTop);
	OUT.selfShadow_AO_SSAThresholds = float4(selfShadow, plantAO, 0.0f.xx);
	#if USE_SSA_STIPPLE_ALPHA
		OUT.selfShadow_AO_SSAThresholds.zw = gStippleThreshold.xy;
	#endif
#endif

	float groundBlend	= IN_Color1.r;		// ground blend amount

	OUT.groundColor0	= float4(groundColorD.rgb, groundBlend);

#if CPLANT_WRITE_GRASS_NORMAL
	float3 grassNormal = ComputeGrassNormal(terrainNormal, fakedGrassNormal, worldPos.xyz, isTop);
	OUT.worldNormal.xyz = (grassNormal.xyz + 1.0f) * 0.5f;
#endif

	return(OUT);
}


//
//
// simplified VS for LOD1: no umovements, collision, alphafade, etc.
//
#if GRASS_INSTANCING
vertexOutputUnlit VS_TransformLOD1_REC(vertexInputUnlit IN, uint instanceID : SV_InstanceID)
#else /// not GRASS_INSTANCING
vertexOutputUnlit VS_TransformLOD1_REC(vertexInputUnlit IN)
#endif // not GRASS_INSTANCING
{
vertexOutputUnlit OUT;

	float ambientScale_local = 1.0f;
#if GRASS_INSTANCING
	float4x4 matGrassTransform_local;
	float4 plantColor_local;
	float4 groundColor_local;
	float4 uMovementParams_local;
	float2 _vecCollParams_local;
	GetGrassInstanceData(instanceID, matGrassTransform_local, plantColor_local, groundColor_local, uMovementParams_local, _vecCollParams_local, ambientScale_local);
#endif // GRASS_INSTANCING

    float4 vertColor	= plantColorD;
    // Write out final position & texture coords
	float3	vertpos		= (float3)IN.pos;
	float3  localNormal	= IN.normal;
    float2	localUV0	= IN.texCoord0;

	float4 IN_Color0 = IN.color0;	// used fields: rgb 
	float4 IN_Color1 = IN.color1;	// used fields: rba


	// do local->global transform for this plant:
	float4	worldPos	= mul(float4(vertpos,1.0f), matGrassTransformD);	
	vertpos				= (float3)worldPos;

	float3	worldNormal = //fakedGrassNormal;		// faked normal
							normalize(mul(float4(localNormal,0),	(float4x3)matGrassTransformD)); 


	float3 vecDist3	= vertpos - vecCameraPos;

    // calculate alpha fading:
    float2 vecDist	= vecDist3.xy;
#if 1
	// square distance approach (fading stops too rapidly):
    float  dist2	= dot(vecDist, vecDist);
#else
	// linear distance approach:
    float  dist2	= sqrt(dot(vecDist, vecDist));
#endif
	// near alpha:
	float alphaFade0 = saturate(dist2 * fadeAlpha0LOD1Dist1 + fadeAlpha0LOD1Dist0);
	// far alpha:
	float alphaFade1 = saturate(dist2 * fadeAlpha1LOD1Dist1 + fadeAlpha1LOD1Dist0);

	float alphaFade = (1.0f-alphaFade0) * alphaFade1;

	// final alpha:    
    float theAlpha		= vertColor.w * alphaFade; // * AlphaScale; // GBuf0AlphaScale;
	ConvertToCoverage( theAlpha );

    OUT.pos				= mul(float4(vertpos,1.0f),	gWorldViewProj);
    OUT.worldNormal.xyz	= worldNormal.xyz;
    OUT.worldNormal.w	= localUV0.x;
	OUT.color0			= float4(vertColor.rgb, theAlpha);
#if NEEDS_WORLD_POSITION
	OUT.worldPos.xyz	= worldPos.xyz;
	OUT.worldPos.w		= localUV0.y;
#endif
	OUT.ambient0		= 0;
	OUT.ambient0.x		= ambientScale_local;

	//Top vs Bottom of plant;
	float isTop = IN_Color1.a;
#if CPLANT_WRITE_GRASS_NORMAL || USE_SSA_STIPPLE_ALPHA
	float selfShadow = saturate(dot(worldNormal.xyz, -fakedGrassNormal) * 8.);
	float plantAO = lerp(globalAO, 1.0f, isTop);
	OUT.selfShadow_AO_SSAThresholds = float4(selfShadow, plantAO, 0.0f.xx);
	#if USE_SSA_STIPPLE_ALPHA
		OUT.selfShadow_AO_SSAThresholds.zw = gStippleThreshold.xy;
	#endif
#endif

	float groundBlend	= IN_Color1.r;		// ground blend amount

	OUT.groundColor0	= float4(groundColorD.rgb, groundBlend);

#if CPLANT_WRITE_GRASS_NORMAL
	float3 grassNormal = ComputeGrassNormal(terrainNormal, fakedGrassNormal, worldPos.xyz, isTop);
	OUT.worldNormal.xyz = (grassNormal.xyz + 1.0f) * 0.5f;
#endif

    return(OUT);
}


//
//
// simplified VS for LOD2: no umovements, collision, alphafade, etc.
//
#if GRASS_INSTANCING
vertexOutputUnlit VS_TransformLOD2_REC(vertexInputUnlitLOD2 IN, uint instanceID : SV_InstanceID)
#else // GRASS_INSTANCING
vertexOutputUnlit VS_TransformLOD2_REC(vertexInputUnlitLOD2 IN)
#endif // GRASS_INSTANCING
{
vertexOutputUnlit OUT;

	float ambientScale_local = 1.0f;
#if GRASS_INSTANCING
	float4x4 matGrassTransform_local;
	float4 plantColor_local;
	float4 groundColor_local;
	float4 _dimensionLOD2_local;
	float2 _vecCollParams_local;
	GetGrassInstanceData(instanceID, matGrassTransform_local, plantColor_local, groundColor_local, _dimensionLOD2_local, _vecCollParams_local, ambientScale_local);
#endif // GRASS_INSTANCING

	// reconstruct billboard vertex position from texcoord:
float2 IN_texCoord0;
float3 IN_pos;
float4 IN_Color1;

	float2 uvs = IN.uvs.xy - float2(0.5f,0.0f);	// center around x-axis only

//float radius = 1.5f;
float radiusW = WidthLOD2;	//3.169f;
float radiusH = HeightLOD2;	//1.525f;


	float3 up		=  gViewInverse[0].xyz * radiusW; 
	float3 across	= -gViewInverse[1].xyz * radiusH;
	float3 offset	= uvs.x*up + uvs.y*across;
	offset.z += radiusH*1.0f;

//	OUT.normal = normalize( offset );

	// Transform position to the clipping space 
	IN_pos			= offset.xyz;
	IN_texCoord0	= IN.uvs;
	IN_texCoord0.y	*= 0.5f; // use lower half of split texture
	IN_texCoord0.y  += 0.5f;
	IN_texCoord0.y  = min(IN_texCoord0.y, 1.0f);

	//fake: regenerate ground blending: 50% at the bottom
	IN_Color1		= float4(0.5f,0.5f,0.5f, 1.0f);
	IN_Color1.rgb	*= IN.uvs.y;
////////////////////////////////

#if GRASS_LOD2_BATCHING
    float4 vertColor	= IN.t5PlantColor;
#else
    float4 vertColor	= plantColorD;
#endif
	// Write out final position & texture coords
	float3	vertpos		= (float3)IN_pos;
//	float3  localNormal	= IN.normal;
    float2	localUV0	= IN_texCoord0;

//	float4 IN_Color0 = IN.color0;	// used fields: rgb 
//	float4 IN_Color1 = IN.color1;	// used fields: rb


	// do local->global transform for this plant:
#if GRASS_LOD2_BATCHING
	float4x4 matGrassTransform4x4;
	matGrassTransform4x4[0] = IN.t1GrassTransform0;
	matGrassTransform4x4[1] = IN.t2GrassTransform1;
	matGrassTransform4x4[2] = IN.t3GrassTransform2;
	matGrassTransform4x4[3] = IN.t4GrassTransform3;
	float4	worldPos	= mul(float4(vertpos,1.0f), matGrassTransform4x4);	
#else
	float4	worldPos	= mul(float4(vertpos,1.0f), matGrassTransformD);	
#endif
	vertpos				= (float3)worldPos;

	float3	worldNormal = fakedGrassNormal;		// faked normal
							// normalize(mul(float4(localNormal,0),	(float4x3)gWorld)); 


	float3 vecDist3	= vertpos - vecCameraPos;

    // calculate alpha fading:
    float2 vecDist	= vecDist3.xy;
#if 1
	// square distance approach (fading stops too rapidly):
    float  dist2	= dot(vecDist, vecDist);
#else
	// linear distance approach:
    float  dist2	= sqrt(dot(vecDist, vecDist));
#endif
	// near alpha:
	float alphaFade0 = saturate(dist2 * fadeAlpha0LOD2Dist1 + fadeAlpha0LOD2Dist0);
	// far alpha:
	float alphaFade1 = saturate(dist2 * fadeAlpha1LOD2Dist1 + fadeAlpha1LOD2Dist0);
	if(IsFarFadeLOD2 > 0.0f)	// uses a far fade
	{
		alphaFade1 = saturate(dist2 * fadeAlpha0LOD2DistFar1 + fadeAlpha0LOD2DistFar0);
	}
	
	float alphaFade = (1.0f-alphaFade0) * alphaFade1;

	// final alpha:    
    float theAlpha		= vertColor.w * alphaFade; // * AlphaScale; // GBuf0AlphaScale;
	ConvertToCoverage( theAlpha );

    OUT.pos				= mul(float4(vertpos,1.0f),	gWorldViewProj);
    OUT.worldNormal.xyz	= worldNormal.xyz;
    OUT.worldNormal.w	= localUV0.x;
	OUT.color0			= float4(vertColor.rgb, theAlpha);
#if NEEDS_WORLD_POSITION
	OUT.worldPos.xyz	= worldPos.xyz;
	OUT.worldPos.w		= localUV0.y;
#endif
	OUT.ambient0		= 0;
	OUT.ambient0.x		= ambientScale_local;

	//Top vs Bottom of plant;
	float isTop = IN_Color1.a;
#if CPLANT_WRITE_GRASS_NORMAL || USE_SSA_STIPPLE_ALPHA
	float selfShadow = saturate(dot(worldNormal.xyz, -fakedGrassNormal) * 8.);
	float plantAO = lerp(globalAO, 1.0f, isTop);
	OUT.selfShadow_AO_SSAThresholds = float4(selfShadow, plantAO, 0.0f.xx);
	#if USE_SSA_STIPPLE_ALPHA
		OUT.selfShadow_AO_SSAThresholds.zw = gStippleThreshold.xy;
	#endif
#endif

	// ground blend amount:
//	float groundBlend	= min(IN_Color1.r + (1.0f-alphaFade1), 1.0f);
	float groundBlend	= IN_Color1.r;

#if GRASS_LOD2_BATCHING
	OUT.groundColor0	= float4(IN.t6GroundColor.rgb, groundBlend);
#else
	OUT.groundColor0	= float4(groundColorD.rgb, groundBlend);
#endif

#if CPLANT_WRITE_GRASS_NORMAL
	float3 grassNormal = ComputeGrassNormal(terrainNormal, fakedGrassNormal, worldPos.xyz, isTop);
	OUT.worldNormal.xyz = (grassNormal.xyz + 1.0f) * 0.5f;
#endif

    return(OUT);
}


//
// ******************************
//	Shadow vertex shaders.
// ******************************


#if PLANTS_CAST_SHADOWS

#if GRASS_INSTANCING
vertexOutputShadow VS_Transform_REC_Shadow(vertexInputUnlit IN, uint instanceID : SV_InstanceID)
#else // GRASS_INSTANCING
vertexOutputShadow VS_Transform_REC_Shadow(vertexInputUnlit IN)
#endif // GRASS_INSTANCING
{
	vertexOutputShadow OUT;

#if GRASS_INSTANCING
	float4x4 matGrassTransform_local;
	float4 uMovementParams_local;
	float2 _vecCollParams_local;
	GetGrassInstanceDataShadow(instanceID, matGrassTransform_local, uMovementParams_local, _vecCollParams_local);
#endif // GRASS_INSTANCING

	float3 vecDist3	= matGrassTransformD[3] - vecCameraPos;
	float dSqr = dot(vecDist3, vecDist3);
	float k = dSqr*depthValueBias.z + depthValueBias.w;
	k = clamp(k, 0.0f, 1.0f);

    // Write out final position & texture coords
	float3	vertpos		= (float3)IN.pos*float3(k, k, k);
    float2	localUV0	= IN.texCoord0;

	float4 IN_Color0 = IN.color0;	// used fields: rgb 
	float4 IN_Color1 = IN.color1;	// used fields: rb

	// local micro-movements of plants:
	float umScaleH	= IN_Color0.r		* umGlobalScaleH;	// horizontal movement scale (red0:   255=max, 0=min)
	float umScaleV	= (1.0f-IN_Color0.b)* umGlobalScaleV;	// vertical movement scale	 (blue0:  0=max, 255=min)
	float umArgPhase= IN_Color0.g		* 2.0f*PI;		// phase shift               (green0: phase 0-1     )

	float3 uMovementArg			= float3(umGlobalTimer, umGlobalTimer, umGlobalTimer);
	float3 uMovementArgPhase	= float3(umArgPhase, umArgPhase, umArgPhase);
	float3 uMovementScaleXYZ	= float3(umScaleH, umScaleH, umScaleV);

	uMovementArg			*= float3(umGlobalArgFreqH, umGlobalArgFreqH, umGlobalArgFreqV);
	uMovementArg			+= uMovementArgPhase;
	float3 uMovementAdd		=  sin(uMovementArg);
	uMovementAdd			*= uMovementScaleXYZ;

	// add final micro-movement:
	vertpos.xyz				+= uMovementAdd;

	// do local->global transform for this plant:
	float4	worldPos	= mul(float4(vertpos,1.0f), matGrassTransformD);	
	vertpos				= (float3)worldPos;

	// player's collision:
	if(false && 1)
	{
		float3 vecTempDist3 = vertpos.xyz - vecPlayerPos.xyz;

		float distc2	= dot(vecTempDist3, vecTempDist3);
		float coll		= (collPedSphereRadiusSqr - distc2) * (collPedInvSphereRadiusSqr);

		//coll = max(coll, 0.0f);
		//coll = min(coll, 1.0f);
		coll = saturate(coll);

//		vertpos.xyz += coll*normalize(vecDist3)  * (IN_Color1.b);
		vertpos.xy += (float2)0.5f*coll*normalize(vecTempDist3).xy * (IN_Color1.b);		// blue1: scale for collision (useful for flower tops, etc.)
//		vertpos.z -= abs((float)coll*normalize(vecDist3).z * (1.0f-IN.color0.b));
	}


	// vehicle collision:
	if(bVehColl0Enabled)
	{
		vertpos.xyz = VS_ApplyVehicleCollision(vertpos, coll0VehVecB, coll0VehVecM, coll0VehInvDotMM, coll0VehR, coll0VehRR, coll0VehInvRR, coll0VehGroundZ, IN_Color1.b);
	}
	
	if(bVehColl1Enabled)
	{
		vertpos.xyz = VS_ApplyVehicleCollision(vertpos, coll1VehVecB, coll1VehVecM, coll1VehInvDotMM, coll1VehR, coll1VehRR, coll1VehInvRR, coll1VehGroundZ, IN_Color1.b);
	}

	if(bVehColl2Enabled)
	{
		vertpos.xyz = VS_ApplyVehicleCollision(vertpos, coll2VehVecB, coll2VehVecM, coll2VehInvDotMM, coll2VehR, coll2VehRR, coll2VehInvRR, coll2VehGroundZ, IN_Color1.b);
	}

	if(bVehColl3Enabled)
	{
		vertpos.xyz = VS_ApplyVehicleCollision(vertpos, coll3VehVecB, coll3VehVecM, coll3VehInvDotMM, coll3VehR, coll3VehRR, coll3VehInvRR, coll3VehGroundZ, IN_Color1.b);
	}

    OUT.pos	= mul(float4(vertpos,1.0f),	gWorldViewProj);
	OUT.uv	= localUV0;

	float biasFactor = clamp(abs(IN.pos.z/depthValueBias.y), 0.0f, 1.0f);
	OUT.pos.z += (biasFactor*depthValueBias.x);
	return(OUT);
}


//
//
// simplified VS for LOD1: no umovements, collision, alphafade, etc.
//
#if GRASS_INSTANCING
vertexOutputShadow VS_TransformLOD1_REC_Shadow(vertexInputUnlit IN, uint instanceID : SV_InstanceID)
#else // GRASS_INSTANCING
vertexOutputShadow VS_TransformLOD1_REC_Shadow(vertexInputUnlit IN)
#endif // GRASS_INSTANCING
{
	vertexOutputShadow OUT;

#if GRASS_INSTANCING
	float4x4 matGrassTransform_local;
	float4 uMovementParams_local;
	float2 _vecCollParams_local;
	GetGrassInstanceDataShadow(instanceID, matGrassTransform_local, uMovementParams_local, _vecCollParams_local);
#endif // GRASS_INSTANCING

	float3 vecDist3	= matGrassTransformD[3] - vecCameraPos;
	float dSqr = dot(vecDist3, vecDist3);
	float k = dSqr*depthValueBias.z + depthValueBias.w;
	k = clamp(k, 0.0f, 1.0f);

    // Write out final position & texture coords
	float3	vertpos		= (float3)IN.pos*float3(k, k, k);
    float2	localUV0	= IN.texCoord0;

	// do local->global transform for this plant:
	float4	worldPos	= mul(float4(vertpos,1.0f), matGrassTransformD);	
	vertpos				= (float3)worldPos;

    OUT.pos	= mul(float4(vertpos,1.0f),	gWorldViewProj);
	OUT.uv	= localUV0;
	float biasFactor = clamp(abs(IN.pos.z/depthValueBias.y), 0.0f, 1.0f);
	OUT.pos.z += (biasFactor*depthValueBias.x);
    return(OUT);
}


//
//
// simplified VS for LOD2: no umovements, collision, alphafade, etc.
//
#if GRASS_INSTANCING
vertexOutputShadow VS_TransformLOD2_REC_Shadow(vertexInputUnlitLOD2 IN, uint instanceID : SV_InstanceID)
#else // GRASS_INSTANCING
vertexOutputShadow VS_TransformLOD2_REC_Shadow(vertexInputUnlitLOD2 IN)
#endif // GRASS_INSTANCING
{
	vertexOutputShadow OUT;

#if GRASS_INSTANCING
	float4x4 matGrassTransform_local;
	float4 _dimensionLOD2_local;
	float2 _vecCollParams_local;
	GetGrassInstanceDataShadow(instanceID, matGrassTransform_local, _dimensionLOD2_local, _vecCollParams_local);
#endif // GRASS_INSTANCING

	// reconstruct billboard vertex position from texcoord:
	float2 IN_texCoord0;
	float3 IN_pos;
	float3 IN_Color1;

	float2 uvs = IN.uvs.xy - float2(0.5f,0.0f);	// center around x-axis only

	float3 vecDist3	= matGrassTransformD[3] - vecCameraPos;
	float dSqr = dot(vecDist3, vecDist3);
	float k = dSqr*depthValueBias.z + depthValueBias.w;
	k = clamp(k, 0.0f, 1.0f);

	//float radius = 1.5f;
	float radiusW = WidthLOD2*k;	//3.169f;
	float radiusH = HeightLOD2*k;	//1.525f;

	float3 up		=  gViewInverse[0].xyz * radiusW; 
	float3 across	= -gViewInverse[1].xyz * radiusH;
	float3 offset	= uvs.x*up + uvs.y*across;
	offset.z += radiusH*1.0f;

	// Transform position to the clipping space 
	IN_pos			= offset.xyz;
	IN_texCoord0	= IN.uvs;
	IN_texCoord0.y	*= 0.5f; // use lower half of split texture
	IN_texCoord0.y  += 0.5f;
	IN_texCoord0.y  = min(IN_texCoord0.y, 1.0f);

	// Write out final position & texture coords
	float3	vertpos		= (float3)IN_pos;
    float2	localUV0	= IN_texCoord0;

	float4	worldPos	= mul(float4(vertpos,1.0f), matGrassTransformD);	
	vertpos				= (float3)worldPos;

    OUT.pos	= mul(float4(vertpos,1.0f),	gWorldViewProj);
	OUT.uv	= localUV0;
	float biasFactor = clamp(abs(IN_pos.z/depthValueBias.y), 0.0f, 1.0f);
	OUT.pos.z += (biasFactor*depthValueBias.x);
    return(OUT);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////


#if GRASS_INSTANCING || __XENON || RSG_ORBIS
vertexOutputUnlit VS_Dummy(vertexInputUnlit IN)
{
	vertexOutputUnlit OUT = (vertexOutputUnlit)0;
	OUT.pos = uMovementParams + float4(_vecCollParams, 0, 0) + _dimensionLOD2 + matGrassTransform[0] + plantColor + groundColor;
	return(OUT);
}
#endif

#endif // PLANTS_CAST_SHADOWS


#if __MAX
//
//
//
//
vertexOutputUnlit VS_MaxTransformUnlit(maxVertexInput IN)
{
vertexOutputUnlit OUT=(vertexOutputUnlit)0;

	float4  vertColor	= float4(1,1,1,1);	//IN.diffuse;
	float3	vertpos		= (float3)IN.pos;
    float2	localUV0	= Max2RageTexCoord2(IN.texCoord0.xy);

    OUT.pos				= mul(float4(vertpos,1.0f),	gWorldViewProj);
    OUT.worldNormal.xyz	= float3(0,0,1);
    OUT.worldNormal.w	= localUV0.x;
	OUT.color0			= vertColor.rgba;
#if NEEDS_WORLD_POSITION
	OUT.worldPos.xyz	= vertpos.xyz;
	OUT.worldPos.w		= localUV0.y;
#endif
	OUT.groundColor0	= float4(1,1,1,0);
	
#if CPLANT_WRITE_GRASS_NORMAL || USE_SSA_STIPPLE_ALPHA
	float selfShadow = 1.0f;
	float plantAO = globalAO;
	OUT.selfShadow_AO_SSAThresholds = float4(selfShadow, plantAO, 0.0f.xx);
	#if USE_SSA_STIPPLE_ALPHA
		OUT.selfShadow_AO_SSAThresholds.zw = gStippleThreshold.xy;
	#endif
#endif

    return(OUT);

}// end of VS_MaxTransformUnlit()...
#endif //__MAX...


//
// pixel shaders:
//
//
float4 PS_TexturedUnlit(vertexOutputUnlit IN ) : COLOR
{
	float2 localUV0 = float2(IN.worldNormal.w, IN.worldPos.w);

#if __MAX || __WIN32PC || RSG_ORBIS
	float4 colortex	= tex2D(DiffuseSampler, localUV0);
#else
	float4 colortex	= tex2D(TextureGrassSampler, localUV0);
#endif
	float4 color = IN.color0 * colortex;
//	color *= materialDiffuse;

	// modulate with ground color:
	color.rgb = lerp(color.rgb, color.rgb*IN.groundColor0.rgb, IN.groundColor0.w);
	color.rgb *= gEmissiveScale;

	return PackHdr(color);
}


//
//
// pixel shaders:
//
DeferredGBuffer PS_GrassDeferredTextured0(vertexOutputUnlit IN,
											inout StandardLightingProperties surfaceStandardLightingProperties, bool bDebugMode)
{
DeferredGBuffer OUT;

	float2 localUV0 = float2(IN.worldNormal.w, IN.worldPos.w);

	SurfaceProperties surfaceInfo = GetSurfaceProperties(
		float4(IN.color0.rgb, 1.0f),
		localUV0,
		TextureGrassSampler,
	#if CPLANT_STORE_LOCTRI_NORMAL
		clamp(IN.worldNormal.xyz,-0.6f,0.6f)	// clamp normal to minimise 3bit compression artifacts
	#else
		(float3)IN.worldNormal.xyz				// standard normal
	#endif
		);

	// Compute some surface information.								
	surfaceStandardLightingProperties = DeriveLightingPropertiesForCommonSurface(surfaceInfo);

	// color modulation:
	surfaceStandardLightingProperties.diffuseColor.rgb *= surfaceInfo.surface_baseColor.rgb;

	// modulate with ground color:
	surfaceStandardLightingProperties.diffuseColor.rgb =
			lerp(surfaceStandardLightingProperties.diffuseColor.rgb, surfaceStandardLightingProperties.diffuseColor.rgb*IN.groundColor0.rgb, IN.groundColor0.w);

	// distant alphafade:
	surfaceStandardLightingProperties.diffuseColor.w *= IN.color0.a;

	surfaceStandardLightingProperties.naturalAmbientScale = IN.ambient0.x;		// AO

	surfaceStandardLightingProperties.artificialAmbientScale = 0.0f;
	
	// Pack the information into the GBuffer(s).
	OUT = PackGBuffer( surfaceInfo.surface_worldNormal, surfaceStandardLightingProperties );

	OUT.col0.a	= surfaceStandardLightingProperties.diffuseColor.a;
	OUT.col1.a	= 1.0f;	// grass material bit for foliage lighting pass

#if !SHADER_FINAL
	OUT.col0.a = bDebugMode? 1-OUT.col0.a : OUT.col0.a;
#endif

#if CPLANT_WRITE_GRASS_NORMAL
	//Pack gbuffers with passed in info. Faster + doesn't pull in additional constants that make RecordBind unhappy.
	float3 grassNormal = IN.worldNormal.xyz; //(IN.worldNormal.xyz + 1.0f) * 0.5f
	OUT.col1.xyz = half3(grassNormal);
	OUT.col1.a = 0.0;

	float spI = 0.0;
	float spE = 0.25;
	float selfShadow = IN.selfShadow_AO_SSAThresholds.x; //surfaceStandardLightingProperties.selfShadow;
	OUT.col2.xyz = half3(spI, spE, 1.0f);
	OUT.col2.a = (half)selfShadow;

	float emissive = 0.0f;
	float naturalAmbientScale = IN.selfShadow_AO_SSAThresholds.y; //surfaceStandardLightingProperties.naturalAmbientScale;
	float artificialAmbientScale = 0.0f; //surfaceStandardLightingProperties.artificialAmbientScale;
	OUT.col3.x = (half)naturalAmbientScale;
	OUT.col3.y = (half)artificialAmbientScale;
	OUT.col3.z = (half)emissive;
	OUT.col3.a = 0.0;
#endif

	OUT.col3.a = Pack2ZeroOneValuesToU8(0.0f, ShadowFalloff);

	return(OUT);
}


struct DeferredGBuffer0
{
	float4	col0	: COLOR;	
};

void ApplySSAStipple(vertexOutputUnlit IN, StandardLightingProperties surfaceStandardLightingProperties)
{
#if USE_SSA_STIPPLE_ALPHA
# if __SHADERMODEL >= 40
	// MSAA case doesn't need discard
	if (gAlphaToCoverageScale != 1.0)
		return;
# endif

	float2 vPos = IN.pos;
	const float4 diffColor = surfaceStandardLightingProperties.diffuseColor;

	#if USE_SSA_STIPPLE_ALPHA_OFFSET
		const float3 worldPos = IN.worldPos.xyz;
		//Staggered SSA 1/2 alpha pattern.
		float2 worldPosIP = 0;
		modf(worldPos.xy, worldPosIP);
		float ssaOffsetPix = SSAIsOpaquePixel(worldPosIP);
		vPos.x += ssaOffsetPix;
	#endif

	//Faster but same as:
	//rageDiscard(diffColor.a >= IN.selfShadow_AO_SSAThresholds.z ? false : (diffColor.a >= IN.selfShadow_AO_SSAThresholds.w ? SSAIsOpaquePixelFrac(vPos) : true));
	float2 thresholds = step(IN.selfShadow_AO_SSAThresholds.zw, diffColor.aa);
	float killPix = 1.0f - thresholds.x - (thresholds.y * SSAIsOpaquePixelFrac(vPos));
	rageDiscard(killPix > 0.0f);
#endif
}


#if PS3_ALPHATOMASK_PASS || XENON_FILL_PASS
DeferredGBuffer0	PS_DeferredTextured(vertexOutputUnlit IN)
#else
DeferredGBuffer		PS_DeferredTextured(vertexOutputUnlit IN)
#endif
{
	StandardLightingProperties surfaceStandardLightingProperties = (StandardLightingProperties)0;
	DeferredGBuffer OUT0 =  PS_GrassDeferredTextured0(IN, surfaceStandardLightingProperties, false);

	ApplySSAStipple(IN, surfaceStandardLightingProperties);

#if PS3_ALPHATOMASK_PASS || XENON_FILL_PASS
	DeferredGBuffer0	OUT;
	#if CPLANT_STORE_LOCTRI_NORMAL
		OUT.col0.x	=	Pack2ZeroToOneValuesToU8_5_3(OUT0.col0.r, OUT0.col1.x);		// 5:3 rgb+normal
		OUT.col0.y	=	Pack2ZeroToOneValuesToU8_5_3(OUT0.col0.g, OUT0.col1.y);
		OUT.col0.z	=	Pack2ZeroToOneValuesToU8_5_3(OUT0.col0.b, OUT0.col1.z);
		OUT.col0.a	=	OUT0.col0.a;
	#else
		OUT.col0	=	OUT0.col0;
	#endif
#else
	DeferredGBuffer		OUT = OUT0;
#endif
	OUT.col0.a *= AlphaScale;
	ConvertToCoverage( OUT.col0.a );

#if (__PS3 && (!PS3_ALPHATOMASK_PASS) && (!USE_SSA_STIPPLE_ALPHA)) || __WIN32PC || RSG_ORBIS

		// PS3: there's no AlphaTest in MRT mode, so we simulate AlphaTest functionality here:
		// equal to:
		// AlphaTest = true;
		// AlphaRef = 48.0f;
		// AlphaFunc = Greater;
		const float4 diffColor = surfaceStandardLightingProperties.diffuseColor;
//		const float colCmp = dot( diffColor.rgb*OUT.col0.a, float3(1.0f/3.0f,1.0f/3.0f,1.0f/3.0f));
		const float alphaCmp = OUT.col0.a;

		#if __PS3
				bool killPix	= bool(alphaCmp <= AlphaTest);
				bool killPix2	= lerp(AlphaToMask(alphaCmp, IN.pos), 0, IN.color0.a);	// only alpha range stippling
				//bool killPix2	= AlphaToMask(alphaCmp, IN.pos);

				rageDiscard(killPix || killPix2);
				//OUT.col0.rgb = lerp(OUT.col0.rgb, float3(1,0,0), killPix2);
		#else
			rageDiscard(alphaCmp <= AlphaTest);	// clip pixel, if threshold or lower transparency
		#endif
#endif //AlphaTest...

	return(OUT);
}

#if !SHADER_FINAL
#if PS3_ALPHATOMASK_PASS || XENON_FILL_PASS
DeferredGBuffer0	PS_DeferredTextured_Dbg(vertexOutputUnlit IN)
#else
DeferredGBuffer		PS_DeferredTextured_Dbg(vertexOutputUnlit IN)
#endif
{
	StandardLightingProperties surfaceStandardLightingProperties = (StandardLightingProperties)0;
	DeferredGBuffer OUT0 =  PS_GrassDeferredTextured0(IN, surfaceStandardLightingProperties, true);

	ApplySSAStipple(IN, surfaceStandardLightingProperties);

#if PS3_ALPHATOMASK_PASS || XENON_FILL_PASS
	DeferredGBuffer0	OUT;
	#if CPLANT_STORE_LOCTRI_NORMAL
		OUT.col0.x	=	Pack2ZeroToOneValuesToU8_5_3(OUT0.col0.r, OUT0.col1.x);		// 5:3 rgb+normal
		OUT.col0.y	=	Pack2ZeroToOneValuesToU8_5_3(OUT0.col0.g, OUT0.col1.y);
		OUT.col0.z	=	Pack2ZeroToOneValuesToU8_5_3(OUT0.col0.b, OUT0.col1.z);
		OUT.col0.a	=	OUT0.col0.a;
	#else
		OUT.col0	=	OUT0.col0;
	#endif
#else
	DeferredGBuffer		OUT = OUT0;
#endif
	OUT.col0.a *= AlphaScale;
	ConvertToCoverage( OUT.col0.a );

#if (__PS3 && (!PS3_ALPHATOMASK_PASS) && (!USE_SSA_STIPPLE_ALPHA)) || __WIN32PC || RSG_ORBIS

		// PS3: there's no AlphaTest in MRT mode, so we simulate AlphaTest functionality here:
		// equal to:
		// AlphaTest = true;
		// AlphaRef = 48.0f;
		// AlphaFunc = Greater;
		const float4 diffColor = surfaceStandardLightingProperties.diffuseColor;
//		const float colCmp = dot( diffColor.rgb*OUT.col0.a, float3(1.0f/3.0f,1.0f/3.0f,1.0f/3.0f));
		const float alphaCmp = OUT.col0.a;

		#if __PS3
				bool killPix	= bool(alphaCmp <= AlphaTest);
				bool killPix2	= lerp(AlphaToMask(alphaCmp, IN.pos), 0, IN.color0.a);	// only alpha range stippling
				//bool killPix2	= AlphaToMask(alphaCmp, IN.pos);

				rageDiscard(killPix || killPix2);
				//OUT.col0.rgb = lerp(OUT.col0.rgb, float3(1,0,0), killPix2);
		#else
			rageDiscard(alphaCmp <= AlphaTest);	// clip pixel, if threshold or lower transparency
		#endif
#endif //AlphaTest...

	return(OUT);
}
#endif //!SHADER_FINAL...

#if PS3_ALPHATOMASK_PASS || XENON_FILL_PASS
DeferredGBuffer0	PS_DeferredTexturedLOD(vertexOutputUnlit IN)
#else
DeferredGBuffer		PS_DeferredTexturedLOD(vertexOutputUnlit IN)
#endif
{
	StandardLightingProperties surfaceStandardLightingProperties = (StandardLightingProperties )0;
	DeferredGBuffer OUT0 =  PS_GrassDeferredTextured0(IN, surfaceStandardLightingProperties, false);

	ApplySSAStipple(IN, surfaceStandardLightingProperties);

#if PS3_ALPHATOMASK_PASS || XENON_FILL_PASS
	DeferredGBuffer0	OUT;
	#if CPLANT_STORE_LOCTRI_NORMAL
		OUT.col0.x	=	Pack2ZeroToOneValuesToU8_5_3(OUT0.col0.r, OUT0.col1.x);		// 5:3 rgb+normal
		OUT.col0.y	=	Pack2ZeroToOneValuesToU8_5_3(OUT0.col0.g, OUT0.col1.y);
		OUT.col0.z	=	Pack2ZeroToOneValuesToU8_5_3(OUT0.col0.b, OUT0.col1.z);
		OUT.col0.a	=	OUT0.col0.a;
	#else
		OUT.col0	=	OUT0.col0;
	#endif
#else
	DeferredGBuffer		OUT = OUT0;
#endif
	OUT.col0.a *= AlphaScale;
	ConvertToCoverage( OUT.col0.a );

#if PS3 && (!PS3_ALPHATOMASK_PASS) && (!USE_SSA_STIPPLE_ALPHA) 
	const float4 diffColor = surfaceStandardLightingProperties.diffuseColor;
	const float alphaCmp = OUT.col0.a;
	bool killPix	= bool(alphaCmp <= AlphaTest);
	bool killPix2	= lerp(AlphaToMask(alphaCmp, vPos), 0, IN.color0.a);	// only alpha range stippling
	rageDiscard(killPix || killPix2);
#endif //AlphaTest...
#if RSG_ORBIS || RSG_PC || RSG_DURANGO
	const float alphaCmp = OUT.col0.a;
	bool killPix	= bool(alphaCmp <= AlphaTest);
	rageDiscard(killPix);
#endif // RSG_ORBIS

	return(OUT);
}

#if !SHADER_FINAL
#if PS3_ALPHATOMASK_PASS || XENON_FILL_PASS
DeferredGBuffer0	PS_DeferredTexturedLOD_Dbg(vertexOutputUnlit IN)
#else
DeferredGBuffer		PS_DeferredTexturedLOD_Dbg(vertexOutputUnlit IN)
#endif
{
	StandardLightingProperties surfaceStandardLightingProperties = (StandardLightingProperties )0;
	DeferredGBuffer OUT0 =  PS_GrassDeferredTextured0(IN, surfaceStandardLightingProperties, true);

	ApplySSAStipple(IN, surfaceStandardLightingProperties);

#if PS3_ALPHATOMASK_PASS || XENON_FILL_PASS
	DeferredGBuffer0	OUT;
	#if CPLANT_STORE_LOCTRI_NORMAL
		OUT.col0.x	=	Pack2ZeroToOneValuesToU8_5_3(OUT0.col0.r, OUT0.col1.x);		// 5:3 rgb+normal
		OUT.col0.y	=	Pack2ZeroToOneValuesToU8_5_3(OUT0.col0.g, OUT0.col1.y);
		OUT.col0.z	=	Pack2ZeroToOneValuesToU8_5_3(OUT0.col0.b, OUT0.col1.z);
	#else
		OUT.col0	=	OUT0.col0;
	#endif
#else
	DeferredGBuffer		OUT = OUT0;
#endif
	OUT.col0.a *= AlphaScale;
	ConvertToCoverage( OUT.col0.a );

#if PS3 && (!PS3_ALPHATOMASK_PASS) && (!USE_SSA_STIPPLE_ALPHA) 
	const float4 diffColor = surfaceStandardLightingProperties.diffuseColor;
	const float alphaCmp = OUT.col0.a; // diffColor.a;
	bool killPix	= bool(alphaCmp <= AlphaTest);
	bool killPix2	= lerp(AlphaToMask(alphaCmp, vPos), 0, IN.color0.a);	// only alpha range stippling
	rageDiscard(killPix || killPix2);
#endif //AlphaTest...
#if RSG_ORBIS || RSG_PC || RSG_DURANGO
	const float alphaCmp = OUT.col0.a; // surfaceStandardLightingProperties.diffuseColor.a;
	bool killPix	= bool(alphaCmp <= AlphaTest);
	rageDiscard(killPix);
#endif // RSG_ORBIS

	return(OUT);
}
#endif//!SHADER_FINAL...

#if PLANTS_CAST_SHADOWS
//
//
//
//
float4 PS_DeferredTextured_Shadow(vertexOutputShadow IN) : COLOR
{
	float4 OUT = tex2D(TextureGrassSampler, IN.uv);

	const float alphaRef = 150.0f/255.0f;	//grass_alphaRef;
	const float alphaCmp = OUT.a;
	rageDiscard(alphaCmp < alphaRef);	// clip pixel, if comparison result is <0
	return(OUT);
}
#endif //PLANTS_CAST_SHADOWS

//
//
// PS3: compatibility replacement PS for map models using grass shader, but not rendered with PlantsMgr
//      (in MRT PS must output 3 regs)
DeferredGBuffer	PS_DeferredTexturedStd(vertexOutputUnlit IN)
{
	StandardLightingProperties surfaceStandardLightingProperties = (StandardLightingProperties)0;
	DeferredGBuffer OUT =  PS_GrassDeferredTextured0(IN, surfaceStandardLightingProperties, false);

	ApplySSAStipple(IN, surfaceStandardLightingProperties);

	return(OUT);
}

//////////////////////////////////////////////////////////////////////////////////
//
//
//
//
struct vertexInputBlit {
    float3 pos			: POSITION;
#if CPLANT_STORE_LOCTRI_NORMAL
	float2 tex			: TEXCOORD0;    
#endif
};

struct vertexOutputBlit {
    DECLARE_POSITION(pos)
#if CPLANT_STORE_LOCTRI_NORMAL
	float2 tex			: TEXCOORD0;
	float4 worldNormal	: TEXCOORD1;
#else
	float4 worldNormal	: TEXCOORD0;
#endif
};

vertexOutputBlit VS_blit(vertexInputBlit IN)
{
vertexOutputBlit OUT;
	OUT.pos			= float4(IN.pos,1);
#if CPLANT_STORE_LOCTRI_NORMAL
	OUT.tex			= IN.tex.xy;
#endif
	OUT.worldNormal.xyz = fakedGrassNormal;		// faked normal
	OUT.worldNormal.w	= globalAO;
	return(OUT);
}

#if CPLANT_BLIT_MIN_GBUFFER
	struct FakedDeferredGBuffer
	{
		half4	col0	: SV_Target0;
		half4	col1	: SV_Target1;
		half4	col2	: SV_Target2;
	};
#else
	#define FakedDeferredGBuffer DeferredGBuffer
#endif

FakedDeferredGBuffer PackFakedDeferredGBuffer( DeferredGBuffer IN )
{
	FakedDeferredGBuffer OUT;
#if CPLANT_BLIT_MIN_GBUFFER
	OUT.col0 = IN.col1;
	OUT.col1 = IN.col2;
	OUT.col2 = IN.col3;
#else
	OUT = IN;
#endif
	return OUT;
}

//
// GBuffer:
// blits faked normals (to color1) and specular (into color2)
//
FakedDeferredGBuffer PS_BlitFakedGbuf(vertexOutputBlit IN)
{
	DeferredGBuffer OUT;

	StandardLightingProperties surfaceStandardLightingProperties=(StandardLightingProperties)0;
	
	surfaceStandardLightingProperties.diffuseColor.rgba		= float4(0,0,0,0);
	surfaceStandardLightingProperties.naturalAmbientScale	= IN.worldNormal.w;
	surfaceStandardLightingProperties.artificialAmbientScale= 0.0f;

	// Pack the information into the GBuffer(s).
#if CPLANT_STORE_LOCTRI_NORMAL
	OUT			= PackGBuffer(float3(0,0,0), surfaceStandardLightingProperties);
	float4 mrt0 = tex2D(GBufferTextureSampler0, IN.tex.xy);
	float3 rgb, normal;
	UnPack2ZeroToOneValuesToU8_5_3(mrt0.r, rgb.x, normal.x);
	UnPack2ZeroToOneValuesToU8_5_3(mrt0.g, rgb.y, normal.y);
	UnPack2ZeroToOneValuesToU8_5_3(mrt0.b, rgb.z, normal.z);
	OUT.col0.xyz	=	rgb.xyz;
	OUT.col1.xyz	=	normal.xyz;
	#if WRITE_SELF_SHADOW_TERM
		OUT.col2.a	=	1.0f;
	#endif
#else
//	float3 mrt0 = tex2D(GBufferTextureSampler0, IN.tex.xy);
//	surfaceStandardLightingProperties.artificialAmbientScale = mrt0.x * 0.0000001f; // hack to preserve sampler
//	surfaceStandardLightingProperties.diffuseColor.rgba	= float4(mrt0.rgb,1.0f);
	OUT = PackGBuffer(IN.worldNormal.xyz, surfaceStandardLightingProperties);
#endif

	OUT.col0.a = 1.0f;

	return PackFakedDeferredGBuffer(OUT);
}




// techniques:
//
//
//
//
#if __MAX
	// ===============================
	// Tool technique
	// ===============================
	technique tool_draw
	{
		pass P0
		{
			MAX_TOOL_TECHNIQUE_RS
			VertexShader= compile VERTEXSHADER	VS_MaxTransformUnlit();
			PixelShader	= compile PIXELSHADER	PS_TexturedUnlit();
		}  
	}
#else //__MAX...

	#if DEFERRED_TECHNIQUES && NON_CUTOUT_DEFERRED_TECHNIQUES
	technique deferred_draw
	{
		pass p0 
		{        
			VertexShader		= compile VERTEXSHADER	VS_Transform_REC();
			PixelShader			= compile PIXELSHADER	PS_DeferredTexturedStd()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif // DEFERRED_TECHNIQUES && NON_CUTOUT_DEFERRED_TECHNIQUES

	technique deferredLOD0_draw
	{
		pass p0 
		{        
			VertexShader		= compile VERTEXSHADER	VS_Transform_REC();
			PixelShader			= compile PIXELSHADER	PS_DeferredTextured()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
#if !SHADER_FINAL
	technique deferredLOD0_dbgdraw
	{
		pass p0 
		{        
			VertexShader		= compile VERTEXSHADER	VS_Transform_REC();
			PixelShader			= compile PIXELSHADER	PS_DeferredTextured_Dbg()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
#endif

	technique deferredLOD1_draw
	{
		pass p0 
		{        
			VertexShader		= compile VERTEXSHADER	VS_TransformLOD1_REC();
			PixelShader			= compile PIXELSHADER	PS_DeferredTexturedLOD()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
#if !SHADER_FINAL
	technique deferredLOD1_dbgdraw
	{
		pass p0 
		{        
			VertexShader		= compile VERTEXSHADER	VS_TransformLOD1_REC();
			PixelShader			= compile PIXELSHADER	PS_DeferredTexturedLOD_Dbg()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
#endif

	technique deferredLOD2_draw
	{
		pass p0 
		{        
			VertexShader		= compile VERTEXSHADER	VS_TransformLOD2_REC();
			PixelShader			= compile PIXELSHADER	PS_DeferredTexturedLOD()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
#if !SHADER_FINAL
	technique deferredLOD2_dbgdraw
	{
		pass p0 
		{        
			VertexShader		= compile VERTEXSHADER	VS_TransformLOD2_REC();
			PixelShader			= compile PIXELSHADER	PS_DeferredTexturedLOD_Dbg()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
#endif

#if PLANTS_CAST_SHADOWS

	technique deferredLOD0_drawshadow
	{
		pass p0 
		{        
			VertexShader		= compile VERTEXSHADER	VS_Transform_REC_Shadow();
			PixelShader			= compile PIXELSHADER	PS_DeferredTextured_Shadow()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}

	technique deferredLOD1_drawshadow
	{
		pass p0 
		{        
			VertexShader		= compile VERTEXSHADER	VS_TransformLOD1_REC_Shadow();
			PixelShader			= compile PIXELSHADER	PS_DeferredTextured_Shadow()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}

	technique deferredLOD2_drawshadow
	{
		pass p0 
		{        
			VertexShader		= compile VERTEXSHADER	VS_TransformLOD2_REC_Shadow();
			PixelShader			= compile PIXELSHADER	PS_DeferredTextured_Shadow()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
#endif

	#if DEFERRED_TECHNIQUES && NON_CUTOUT_DEFERRED_TECHNIQUES && DRAWSKINNED_TECHNIQUES
	technique deferred_drawskinned
	{
		pass p0 
		{        
			VertexShader		= compile VERTEXSHADER	VS_Transform_REC();
			PixelShader			= compile PIXELSHADER	PS_DeferredTextured()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}	
	#endif // DEFERRED_TECHNIQUES && NON_CUTOUT_DEFERRED_TECHNIQUES && DRAWSKINNED_TECHNIQUES

	#if DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES && __PS3
	technique deferredalphaclip_draw
	{
		pass p0 
		{        
			VertexShader		= compile VERTEXSHADER	VS_Transform_REC();
			PixelShader			= compile PIXELSHADER	PS_DeferredTexturedStd()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif // DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES && __PS3

	#if DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES && DRAWSKINNED_TECHNIQUES && __PS3
	technique deferredalphaclip_drawskinned
	{
		pass p0 
		{        
			VertexShader		= compile VERTEXSHADER	VS_Transform_REC();
			PixelShader			= compile PIXELSHADER	PS_DeferredTexturedStd()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif // DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES && DRAWSKINNED_TECHNIQUES && __PS3


	//
	//
	//
	//
	#if FORWARD_TECHNIQUES
	technique draw
	{
		pass p0 
		{        
			VertexShader = compile VERTEXSHADER	VS_Transform_REC();
			PixelShader  = compile PIXELSHADER	PS_TexturedUnlit()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif // FORWARD_TECHNIQUES

	#if UNLIT_TECHNIQUES
	technique unlit_draw
	{
		pass p0 
		{        
			VertexShader = compile VERTEXSHADER	VS_Transform_REC();
			PixelShader  = compile PIXELSHADER	PS_TexturedUnlit()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif // UNLIT_TECHNIQUES

	technique blitFakedGBuf
	{
		pass p0 
		{        
			VertexShader = compile VERTEXSHADER	VS_blit();
			PixelShader  = compile PIXELSHADER	PS_BlitFakedGbuf()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}

#if PLANTS_CAST_SHADOWS
	technique dummy
	{
		pass p0 
		{        
			VertexShader = compile VERTEXSHADER	VS_Dummy();
			COMPILE_PIXELSHADER_NULL()
		}
	}
#endif

#endif //__MAX...


