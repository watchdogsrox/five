#ifndef PRAGMA_DCL
	#pragma dcl position diffuse specular texcoord0 normal
	#define PRAGMA_DCL
#endif
//
//
//
//
//
//
//
#if  !__MAX
property int __rage_drawbucket = 3;
#endif

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

#ifndef NO_DEFAULT_TECHNIQUES
	#define USE_DEFAULT_TECHNIQUES
#endif

#ifdef USE_DEFAULT_TECHNIQUES
	#define TECHNIQUES						(1)
#else
	#define TECHNIQUES						(0)
#endif	// USE_DEFAULT_TECHNIQUES

#define CAN_BE_CUTOUT_SHADER

//
// Grass Batch shader:
//

#define USE_BATCH_INSTANCING
#define USE_SSA_STIPPLE_ALPHA				(0)
#define USE_SSA_STIPPLE_ALPHA_OFFSET		(0 && USE_SSA_STIPPLE_ALPHA)
#define WRITE_SELF_SHADOW_TERM				(1)
#define CPLANT_WRITE_GRASS_NORMAL			(0)
#define PS3_ALPHATOMASK_PASS				(1 && __PS3 && !CPLANT_WRITE_GRASS_NORMAL)
#define CPLANT_STORE_LOCTRI_NORMAL			(0)				// 5:3 rgb+normal compression for single RT
#define PLANTS_CAST_SHADOWS					(1 && (__SHADERMODEL >= 40))
#define USE_GRASS_WIND_BENDING				(1)
#define SHADOW_USE_TEXTURE

//Ground color not set on collision mesh in rdr3, so we want to ignore it.
#define USE_GROUND_COLOR					(0)//(!HACK_RDR3)


#define PLAYER_COLL_RADIUS0			(0.75f)	// radius of influence
#define PLAYER_COLL_RADIUS_SQR0		(PLAYER_COLL_RADIUS0*PLAYER_COLL_RADIUS0)


#define USE_DIFFUSE_SAMPLER
#if !WRITE_SELF_SHADOW_TERM
	#define NO_SELF_SHADOW
#endif

#define GRASS_SHADER
#define GRASS_BATCH_SHADER

#include "../../common.fxh"
#include "../../Util/skin.fxh"

#define ENABLE_TRAIL_MAP					(!__PS3 && HACK_RDR3)
#define COLOR_FLAT_GRASS					(0 && ENABLE_TRAIL_MAP && USE_GROUND_COLOR)

#define SHADOW_CASTING            (1)
#define SHADOW_CASTING_TECHNIQUES (1 && SHADOW_CASTING_TECHNIQUES_OK)
#define SHADOW_RECEIVING          (1)
#define SHADOW_RECEIVING_VS       (0)
#include "../../Lighting/Shadows/localshadowglobals.fxh"
#include "../../Lighting/Shadows/cascadeshadows.fxh"
#include "../../Lighting/lighting.fxh"

#include "grass.fxh"
#include "grass_globals.fxh"
#include "..\..\Util\camera.fxh"

#define FOLIAGE_2GBUF_OPT 0

#define NEEDS_WORLD_POSITION (SHADOW_RECEIVING || USE_SSA_STIPPLE_ALPHA_OFFSET || defined(SHADOW_USE_TEXTURE))

#if USE_GROUND_COLOR
BEGIN_RAGE_CONSTANT_BUFFER(grassbatchlocalsfrequent,b0)
//float4x4 matGrassTransform	: matGrassTransform0	REGISTER2(vs, GRASS_CREG_TRANSFORM);
//float4	 plantColor			: plantColor0			REGISTER2(vs, GRASS_CREG_PLANTCOLOR);	// global plant color + alpha
float4	 groundColor		: groundColor0			REGISTER2(vs, GRASS_CREG_GROUNDCOLOR);	// global ground color [R | G | B | 1.0f]
EndConstantBufferDX10(grassbatchlocalsfrequent)

// #if GRASS_INSTANCING
// #define matGrassTransformD matGrassTransform_local
// #define plantColorD plantColor_local
// #define groundColorD groundColor_local
// #else // not GRASS_INSTANCING
//#define matGrassTransformD matGrassTransform
//#define plantColorD plantColor
#define groundColorD groundColor
//#endif // not GRASS_INSTANCING
#endif //USE_GROUND_COLOR

BEGIN_RAGE_CONSTANT_BUFFER(grassbatchlocals,b8)


//
//
//
//
float3	vecBatchAabbMin : vecBatchAabbMin0 = float3(0,0,0); // xyz = batch aabb min. Offset pos is based of this.
float3	vecBatchAabbDelta : vecBatchAabbDelta0 = float3(1,1,1); // xyz = batch aabb max - min. Offset pos is based of this.
//float3	vecCameraPos	: vecCameraPos0 REGISTER2(vs, GRASS_CREG_CAMERAPOS)	= float3(0,0,0);	// xyz = camera pos
#define vecCameraPos (gViewInverse[3].xyz)
float4	vecPlayerPos	: vecPlayerPos0 REGISTER2(vs, GRASS_CREG_PLAYERPOS)	= float4(0,0,0,0);	// xyz = current player pos

//Borrowing a float slot from vecPlayerPos
#define gOrientToTerrain (vecPlayerPos.w)

#if CPLANT_WRITE_GRASS_NORMAL
	float3 gView_Light_NormalScaler_AO	: gView_Light_NormalScaler_AO = float3(0.999f, 0.999f, GRASS_GLOBAL_AO);
	#define gViewDirNormalScaler		(gView_Light_NormalScaler_AO.x)		// x = scaler value for view based offset for normal
	#define gLightDirNormalScaler		(gView_Light_NormalScaler_AO.y)		// y = scaler value for primary light direction based offset for normal
#endif


//
// ped collision params:
// x = coll sphere radius sqr
// y = inv of coll sphere radius sqr
//
float2	_vecCollParams	: vecCollParams0 REGISTER2(vs,GRASS_CREG_PLAYERCOLLPARAMS)	= float2(PLAYER_COLL_RADIUS_SQR0, 1.0f/PLAYER_COLL_RADIUS_SQR0);
// #if GRASS_INSTANCING
// #define collPedSphereRadiusSqr		(_vecCollParams_local.x)		// x = coll sphere radius sqr
// #define collPedInvSphereRadiusSqr	(_vecCollParams_local.y)		// y = inv of coll sphere radius sqr
// #else // not GRASS_INSTANCING
#define collPedSphereRadiusSqr		(_vecCollParams.x)		// x = coll sphere radius sqr
#define collPedInvSphereRadiusSqr	(_vecCollParams.y)		// y = inv of coll sphere radius sqr
//#endif // not GRASS_INSTANCING

// vehicle collision params:
// vehicle#0:
float4		_vecVehColl0B	: vecVehColl0B0 REGISTER2(vs,GRASS_CREG_VEHCOLL0B) = float4(0,0,0,0);	// [ segment start point B.xyz				| VEHCOL_DIST_LIMIT	]
float4		_vecVehColl0M	: vecVehColl0M0 REGISTER2(vs,GRASS_CREG_VEHCOLL0M) = float4(0,0,0,0);	// [ segment vector M.xyz					| 1.0f/dot(M,M)		]
float4		_vecVehColl0R	: vecVehColl0R0 REGISTER2(vs,GRASS_CREG_VEHCOLL0R) = float4(0,0,0,0);	// [ R	      | R*R          | 1.0f/(R*R)   | worldPosZ			]
#define coll0VehVecB			(_vecVehColl0B.xyz)	// start segment point B
#define coll0VehVecM			(_vecVehColl0M.xyz)	// segment vector M
#define coll0VehInvDotMM		(_vecVehColl0M.w)	// 1/dot(M,M)
#define coll0VehR				(_vecVehColl0R.x)	// collision radius
#define coll0VehRR				(_vecVehColl0R.y)	// R*R
#define coll0VehInvRR			(_vecVehColl0R.z)	// 1/(R*R)
#define coll0VehGroundZ			(_vecVehColl0R.w)
#define coll0VehColDistLimit	(_vecVehColl0B.w)	// VEHCOL_DIST_LIMIT

// vehicle#1:
float4		_vecVehColl1B	: vecVehColl1B0 REGISTER2(vs,GRASS_CREG_VEHCOLL1B) = float4(0,0,0,0);	// [ segment start point B.xyz				| VEHCOL_DIST_LIMIT	]
float4		_vecVehColl1M	: vecVehColl1M0 REGISTER2(vs,GRASS_CREG_VEHCOLL1M) = float4(0,0,0,0);	// [ segment vector M.xyz					| 1.0f/dot(M,M)		]
float4		_vecVehColl1R	: vecVehColl1R0 REGISTER2(vs,GRASS_CREG_VEHCOLL1R) = float4(0,0,0,0);	// [ R	      | R*R          | 1.0f/(R*R)   | worldPosZ			]
#define coll1VehVecB			(_vecVehColl1B.xyz)	// start segment point B
#define coll1VehVecM			(_vecVehColl1M.xyz)	// segment vector M
#define coll1VehInvDotMM		(_vecVehColl1M.w)	// 1/dot(M,M)
#define coll1VehR				(_vecVehColl1R.x)	// collision radius
#define coll1VehRR				(_vecVehColl1R.y)	// R*R
#define coll1VehInvRR			(_vecVehColl1R.z)	// 1/(R*R)
#define coll1VehGroundZ			(_vecVehColl1R.w)
#define coll1VehColDistLimit	(_vecVehColl1B.w)	// VEHCOL_DIST_LIMIT

// vehicle#2:
float4		_vecVehColl2B	: vecVehColl2B0 REGISTER2(vs,GRASS_CREG_VEHCOLL2B) = float4(0,0,0,0);	// [ segment start point B.xyz				| VEHCOL_DIST_LIMIT	]
float4		_vecVehColl2M	: vecVehColl2M0 REGISTER2(vs,GRASS_CREG_VEHCOLL2M) = float4(0,0,0,0);	// [ segment vector M.xyz					| 1.0f/dot(M,M)		]
float4		_vecVehColl2R	: vecVehColl2R0 REGISTER2(vs,GRASS_CREG_VEHCOLL2R) = float4(0,0,0,0);	// [ R	      | R*R          | 1.0f/(R*R)   | worldPosZ			]
#define coll2VehVecB			(_vecVehColl2B.xyz)	// start segment point B
#define coll2VehVecM			(_vecVehColl2M.xyz)	// segment vector M
#define coll2VehInvDotMM		(_vecVehColl2M.w)	// 1/dot(M,M)
#define coll2VehR				(_vecVehColl2R.x)	// collision radius
#define coll2VehRR				(_vecVehColl2R.y)	// R*R
#define coll2VehInvRR			(_vecVehColl2R.z)	// 1/(R*R)
#define coll2VehGroundZ			(_vecVehColl2R.w)
#define coll2VehColDistLimit	(_vecVehColl2B.w)	// VEHCOL_DIST_LIMIT

// vehicle#3:
float4		_vecVehColl3B	: vecVehColl3B0 REGISTER2(vs,GRASS_CREG_VEHCOLL3B) = float4(0,0,0,0);	// [ segment start point B.xyz				| VEHCOL_DIST_LIMIT	]
float4		_vecVehColl3M	: vecVehColl3M0 REGISTER2(vs,GRASS_CREG_VEHCOLL3M) = float4(0,0,0,0);	// [ segment vector M.xyz					| 1.0f/dot(M,M)		]
float4		_vecVehColl3R	: vecVehColl3R0 REGISTER2(vs,GRASS_CREG_VEHCOLL3R) = float4(0,0,0,0);	// [ R	      | R*R          | 1.0f/(R*R)   | worldPosZ			]
#define coll3VehVecB			(_vecVehColl3B.xyz)	// start segment point B
#define coll3VehVecM			(_vecVehColl3M.xyz)	// segment vector M
#define coll3VehInvDotMM		(_vecVehColl3M.w)	// 1/dot(M,M)
#define coll3VehR				(_vecVehColl3R.x)	// collision radius
#define coll3VehRR				(_vecVehColl3R.y)	// R*R
#define coll3VehInvRR			(_vecVehColl3R.z)	// 1/(R*R)
#define coll3VehGroundZ			(_vecVehColl3R.w)
#define coll3VehColDistLimit	(_vecVehColl3B.w)	// VEHCOL_DIST_LIMIT


float4	fadeAlphaDistUmTimer	: fadeAlphaDistUmTimer0 REGISTER2(vs,GRASS_CREG_ALPHALOD0DISTUMTIMER);	// [ (f)/(f-c) | (-1.0)/(f-c) | umTimer | ? ];
#define fadeAlphaLOD0Dist0		(fadeAlphaDistUmTimer.x)	// (f)/(f-c)
#define fadeAlphaLOD0Dist1		(fadeAlphaDistUmTimer.y)	// (-1.0)/(f-c)
#define umGlobalTimer			(fadeAlphaDistUmTimer.z)	// micro-movement timer

// micro-movement params: [globalScaleH | globalScaleV | globalArgFreqH | globalArgFreqV ]
float4	uMovementParams			: uMovementParams0		REGISTER2(vs,GRASS_CREG_UMPARAMS)
<
	string	UIName = "umScaleFreqHV";
> = float4(0.05f, 0.05f, 0.2125f, 0.2125f);
#define umGlobalScaleH			(uMovementParams.x)		// micro-movement: globalScaleHorizontal
#define umGlobalScaleV			(uMovementParams.y)		// micro-movement: globalScaleVertical
#define umGlobalArgFreqH		(uMovementParams.z)		// micro-movement: globalArgFreqHorizontal
#define umGlobalArgFreqV		(uMovementParams.w)		// micro-movement: globalArgFreqVertical

#if ENABLE_TRAIL_MAP
float4 mapSizeWorldSpace		: mapSizeWorldSpace		REGISTER2(vs,GRASS_CREG_MAP_SIZE_WORLD_SPACE) = float4( 0.0f, 0.0f, 0.0f, 0.0f );
#endif


#if __PS3
float4 _fakedGrassNormal		: fakedGrassNormal0		REGISTER2(vs,GRASS_CREG_FAKEDNORMAL)
#else
float4 _fakedGrassNormal		: fakedGrassNormal0		REGISTER(GRASS_CREG_FAKEDNORMAL)
#endif
<
	int nostrip=1;
> = float4(0,0,1,GRASS_GLOBAL_AO);
#if CPLANT_WRITE_GRASS_NORMAL
	#define fakedGrassNormal		(-gDirectionalLight.xyz)
#else
	#define fakedGrassNormal		(_fakedGrassNormal.xyz)
#endif

#define GLOBAL_AO_EDITABLE		(!defined(SHADER_FINAL) && !CPLANT_WRITE_GRASS_NORMAL)
#if GLOBAL_AO_EDITABLE
	#define globalAO			(_fakedGrassNormal.w)
#else
	#if CPLANT_WRITE_GRASS_NORMAL
		#define globalAO		(gView_Light_NormalScaler_AO.z)
	#else
		#define globalAO		(GRASS_GLOBAL_AO)
	#endif
#endif

// #if CPLANT_WRITE_GRASS_NORMAL
// 	float4 _terrainNormal	: terrainNormal0		REGISTER2(vs, GRASS_CREG_TERRAINNORMAL);
// 	#define terrainNormal	(_terrainNormal.xyz)
// #endif

#if USE_SSA_STIPPLE_ALPHA
	float2 gStippleThreshold	: gStippleThreshold = float2(0.5, 0.15);
#endif

float3 gScaleRange : gScaleRange = float3(1.0f, 1.0f, 0.0f);

float4 gWindBendingGlobals : gWindBendingGlobals = float4(0.0f, 0.0f, 0.0f, 0.0f);
float2 gWindBendScaleVar : gWindBendScaleVar
<
	string UIName = "Wind Bend Scale Variation";
	float UIMin = 0.0;
	float UIMax = FLT_MAX / 1000.0;
	float UIStep = 0.1;
> = float2(0.0f, 0.0f);
#define wind_bend_scale (gWindBendScaleVar.x)
#define wind_bend_var (gWindBendScaleVar.y)

float gAlphaTest : gAlphaTest
<
	string UIWidget = "slider";
	float UIMin = -100.0;
	float UIMax = 100.0;
	float UIStep = .01;
	string UIName = "Alpha Test Ref";
> = {0.25};

float gAlphaToCoverageScale	: gAlphaToCoverageScale
<
	string UIName = "Alpha multiplier for MSAA coverage";
	float UIMin = 0.0;
	float UIMax = 5.0;
	float UIStep = 0.1;
	int nostrip	= 1;
> = 1.3f;

#define ShadowFalloff (0.1f)

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
uint gIsShadowPass = 0;
float3 gLodFadeControlRange = float3(0.0f, (float)0x1FFF, 1.0f);

#define USE_SKIP_INSTANCE		(RSG_DURANGO || RSG_PC)

#if RSG_DURANGO
uint4 gIndirectCountPerLod = uint4(0, 0, 0, 0);
#endif

#if USE_SKIP_INSTANCE
uint gGrassSkipInstance = 0xffffffff;
#endif // USE_SKIP_INSTANCE

//
//
//
//
#if __SHADERMODEL >= 40
	float4 bDebugSwitches : bDebugSwitches0 = {0,0,0,0};
	#define bDebugAlphaOverdraw		(bDebugSwitches.x > 0)
#else
	#define bDebugAlphaOverdraw		(false)
#endif
EndConstantBufferDX10(grassbatchlocals)

#if (__SHADERMODEL >= 40) //&& INSTANCED
BeginConstantBufferPagedDX10(grassbatch_instmtx,PASTE2(b,INSTANCE_CONSTANT_BUFFER_SLOT))
float4 gInstanceVars[GRASS_BATCH_NUM_CONSTS_PER_INSTANCE * GRASS_BATCH_SHARED_DATA_COUNT] REGISTER2(vs, PASTE2(c, INSTANCE_SHADER_CONSTANT_SLOT));
EndConstantBufferDX10(grassbatch_instmtx)
#endif

#if GRASS_BATCH_CS_CULLING
StructuredBuffer<BatchCSInstanceData> InstanceBuffer;// REGISTER2(vs, t0);
StructuredBuffer<BatchRawInstanceData> RawInstanceBuffer;// REGISTER2(cs_5_0, t0);
#endif


// #if GRASS_INSTANCING_TRANSPORT_CONSTANT_BUFFERS_SHADER_VARIABLES
// 
// CBSHARED BeginConstantBufferPagedDX10(grassinstances, b5)
// #pragma constant 0
// shared float4 instanceData[GRASS_INSTANCING_INSTANCE_SIZE*GRASS_INSTANCING_BUCKET_SIZE] : instanceData0;
// EndConstantBufferDX10(grassinstances)
// 
// #endif // GRASS_INSTANCING_TRANSPORT_CONSTANT_BUFFERS_SHADER_VARIABLES

//
//
//
//

#if ENABLE_TRAIL_MAP
BeginSampler(sampler2D, trailMapTexture, trailMapSampler, trailMapTexture0)
	//string	UIName = "__TRAILMAP__";
ContinueSampler(sampler2D, trailMapTexture, trailMapSampler, trailMapTexture0)
	AddressU = CLAMP;
	AddressV = CLAMP;
	MIPFILTER = LINEAR;
	MINFILTER = LINEAR;
	MAGFILTER = LINEAR;
EndSampler;
#endif

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

// #if GRASS_INSTANCING_TRANSPORT_TEXTURE_SHADER_VARIABLES
// BeginDX10Sampler(	sampler, Texture2D<float4>, instanceTexture0, InstanceSampler0, instanceTexture0)
// ContinueSampler(sampler, instanceTexture0, InstanceSampler0, instanceTexture0)
// 	AddressU  = CLAMP;        
// 	AddressV  = CLAMP;
// 	MINFILTER = POINT;
// 	MAGFILTER = POINT;
// 	MIPFILTER = POINT;
// EndSampler;
// #endif // GRASS_INSTANCING_TRANSPORT_TEXTURE_SHADER_VARIABLES

#if 0	//__PS3
BeginSampler(sampler2D,stippleTexture,StippleSampler,StippleTex)
//	int nostrip=1;	// dont strip
ContinueSampler(sampler2D,stippleTexture,StippleSampler,StippleTex)
	AddressU  = CLAMP;
	AddressV  = CLAMP;
	AddressW  = CLAMP;
	MIPFILTER = LINEAR;
	MINFILTER = LINEAR;
	MAGFILTER = LINEAR; 
EndSampler;

//
//
//
//
float4 AlphaToMaskTex(float fStippleAlpha, float2 vPos)
{
	float fAlpha = saturate(fStippleAlpha) * 0.999f;
//	float fAlpha = saturate(fStippleAlpha-grass_alphaRef) * 0.999f;	// shift by alpharef to use full range of stippletex
	
	

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
struct grassBatchBaseData
{
	float4 pos		        : POSITION0;
	float3 normal			: NORMAL;
	float4 color0			: COLOR0;
	float4 color1			: COLOR1;
	float2 texCoord0        : TEXCOORD0;

#if CAMERA_FACING || CAMERA_ALIGNED
	float4 RelativeCentre	: TANGENT1;
#endif
};

struct grassBatchVertexInput
{
	grassBatchBaseData base;
	BatchInstanceData inst;
	BatchGlobalData global;

#if GRASS_BATCH_CS_CULLING
	BatchComputeData csData;
#endif
};

#if (__SHADERMODEL >= 40)
struct grassBatchVertexInputSM4
{
	grassBatchBaseData base;
#if !GRASS_BATCH_CS_CULLING
	BatchInstanceData inst;
#endif
	//Globals stored in constant buffer.
};
#endif

//Special case... don't want to use common instancing code because grass batches are very specific.
struct grassInstancedBatchVertexInput
{
#if __XENON
	int nIndex : INDEX;
#elif (__SHADERMODEL >= 40) //&& INSTANCED
	grassBatchVertexInputSM4 baseVertIn;
	uint nInstIndex : SV_InstanceID;
#else
	grassBatchVertexInput baseVertIn;
#endif
};

//DeclareInstancedStuct(grassInstancedBatchVertexInput, grassBatchVertexInput);

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
};


struct vertexOutputUnlit
{
    DECLARE_POSITION(pos)
	float4	color0			: TEXCOORD0;
	float4	worldNormal		: TEXCOORD1;	// worldNormal.w = UV0
#if NEEDS_WORLD_POSITION
	float4	worldPos		: TEXCOORD2;	// worldPos.w = UV1
#endif
#if USE_GROUND_COLOR
	float4	groundColor0	: TEXCOORD3;	// groundColor(RGB) + groundBlend
#endif
#if CPLANT_WRITE_GRASS_NORMAL || USE_SSA_STIPPLE_ALPHA
	float4	selfShadow_AO_SSAThresholds	: TEXCOORD4;	// Light Dir & global AO
#endif
#if GRASS_BATCH_CS_CULLING
	float2 alpha_ao		: TEXCOORD5;
#endif
};



#if FOLIAGE_2GBUF_OPT
	struct DeferredGBufferGrass
	{
		half4	col0	: COLOR0;	// col0.rgb = Albedo (diffuse)
									// col0.a   = SSA
		half4	col1	: COLOR1;	// col1.rgb = Normal (xyz)
									// col1.a   = AO //Twiddle
	};
	#define GrassGBuffer DeferredGBufferGrass
#else
	#if PS3_ALPHATOMASK_PASS
		struct DeferredGBuffer0
		{
			float4	col0	: COLOR0;	
		};
		#define GrassGBuffer DeferredGBuffer0
	#else //PS3_ALPHATOMASK_PASS
		#define GrassGBuffer DeferredGBuffer
	#endif //PS3_ALPHATOMASK_PASS
#endif

GrassGBuffer PackGrassGBuffer(DeferredGBuffer IN)
{
#if FOLIAGE_2GBUF_OPT
	GrassGBuffer OUT;
	OUT.col0 = IN.col0;
	OUT.col1 = half4(IN.col1.rgb, IN.col3.r);
	ConvertToCoverage( OUT.col0.a );
	return OUT;
#else
	#if PS3_ALPHATOMASK_PASS
	DeferredGBuffer0 OUT;
		#if CPLANT_STORE_LOCTRI_NORMAL
			OUT.col0.x	=	Pack2ZeroToOneValuesToU8_5_3(IN.col0.r, IN.col1.x);		// 5:3 rgb+normal
			OUT.col0.y	=	Pack2ZeroToOneValuesToU8_5_3(IN.col0.g, IN.col1.y);
			OUT.col0.z	=	Pack2ZeroToOneValuesToU8_5_3(IN.col0.b, IN.col1.z);
			OUT.col0.a	=	IN.col0.a;
		#else
			OUT.col0 = IN.col0;
		#endif
		float alpha = OUT.col0.a;
		ConvertToCoverage( alpha );
		OUT.col0.a = alpha;
	return OUT;
	#else //PS3_ALPHATOMASK_PASS
		float alpha = IN.col0.a;
		ConvertToCoverage( alpha );
		IN.col0.a = alpha;

		#if RSG_ORBIS || RSG_PC || RSG_DURANGO
			bool killPix	= bool(alpha <= gAlphaTest);
			killPix = killPix && bool(gAlphaToCoverageScale==1.0f);	// MSAA case doesn't need discard
			rageDiscard(killPix);
		#endif

		return IN;
	#endif //PS3_ALPHATOMASK_PASS
#endif
}

#if __SHADERMODEL >= 40
#if GRASS_BATCH_CS_CULLING
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
// 	if(gUseComputeShaderOutputBuffer != 0)
// 	{
// 		return cs_data.raw;
// 	}

	return RawInstanceBuffer[nInstIndex];
}
#endif //GRASS_BATCH_CS_CULLING

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
#endif //__SHADERMODEL >= 40

void FetchBatchInstanceVertexData(grassInstancedBatchVertexInput instIN, out grassBatchVertexInput vertData, out uint nInstIndex)
{
#if (__SHADERMODEL >= 40) //&& INSTANCED
	vertData.base = instIN.baseVertIn.base;
#if GRASS_BATCH_CS_CULLING
	vertData.csData = GetComputeData(instIN.nInstIndex);
	instIN.nInstIndex = vertData.csData.InstId;	//CS writes original instance ID.
#endif
	vertData.inst = GRASS_BATCH_CS_CULLING_SWITCH(ExpandRawInstanceData(GetRawInstanceData(instIN.nInstIndex)), instIN.baseVertIn.inst);
	vertData.global = FetchSharedInstanceVertexData(instIN.nInstIndex);

	//Debug: Enable to highlight crossfading grass!
// 	if(vertData.csData.CrossFade < 1.0f)
// 		vertData.inst.color_scale.rgb = float3(1.0f, 0.0f.xx);

#if RSG_ORBIS && !GRASS_BATCH_CS_CULLING
	//We swizzle color vert channels on orbis. (Using color to get kBufferFormat8_8_8_8)
	vertData.inst.color_scale.xyzw = vertData.inst.color_scale.zyxw;
	vertData.inst.ao_pad3.xyzw = vertData.inst.ao_pad3.zyxw;
#endif

	nInstIndex = instIN.nInstIndex;
#else
	vertData = instIN.baseVertIn;
	nInstIndex = 0;
#endif
}

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
												/*collision radius*/float R, /*R*R*/float RR, /*1/(R*R)*/float invRR, float GroundZ, float collScale, float VehColDistLimit)
{
	if(abs(vertpos.z-GroundZ) > VehColDistLimit)
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

#if ENABLE_TRAIL_MAP
void GetTrailMapValues( float3 vertPos, float3 mapCenterWorld, out float2 grassDir2D, out float intensity )
{
	// Get map lookup
	float2 trailUV = clamp((vertPos.xy - mapCenterWorld.xy)/ mapSizeWorldSpace.xy, -1.0f, 1.0f );
	trailUV = trailUV * 0.5f + 0.5f;
	float4 trailUVlod = float4( trailUV.x, 1.0f - trailUV.y, 0.0f, 0.0f );
	float4 trailMapColor = tex2Dlod( trailMapSampler, trailUVlod );
	grassDir2D = float2( trailMapColor.xy * 2.0f - 1.0f );
	intensity = trailMapColor.a > 0.05f ? 1.0f : 0.0f;
}


float3 FlattenGrass(float3 vertPos, float3 terrainNormal, float2 dir, float intensity, float isTop)
{
	float3 dir3 = float3( dir, 0.0f );
	float3 projDir = dir3 - terrainNormal * dot( terrainNormal, dir3 ); // Dir projected onto the poly plane
	float3 fullFlattenedPos = vertPos + normalize( projDir );

	fullFlattenedPos.z += 0.05f; // keep from z fighting

#if __XENON
	// Just do a linear blend for now
	return lerp( vertPos, fullFlattenedPos, intensity*isTop );
#else
	return vertPos; // Disable for now...
#endif
}
#endif

float ComputeInstanceFade(BatchGlobalData IN, uint nInstIndex)
{
#if __PS3
	//Need to know what instance is currently rendering and how many instances in the batch.
	int numSharedVerts = gInstancingParams.y;
	const float invScaleStep = float(numSharedVerts - 1);
	const int nSharedIndex = (IN.worldMatA.w * invScaleStep) + 0.5f;
	nInstIndex = int(gInstancingParams.x) + nSharedIndex;
#endif

	//NG fades from the back rather than the front... so need to compute the scale differently...
#if (__SHADERMODEL >= 40)
		int instIndex = nInstIndex - 1;
#else
		int instIndex = nInstIndex + 1;
#endif
	return saturate((instIndex - gLodFadeInstRange.x) * gLodFadeInstRange.z);
}

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


#define COMPUTE_COMPOSITE_WORLD_MTX (ENABLE_TRAIL_MAP)
//
// ******************************
//	DRAW METHODS
// ******************************
//
//
//
//
//
vertexOutputUnlit VS_TransformInternal(grassInstancedBatchVertexInput instIN, bool bPlayerColl, bool bVehicleColl)
{
	uint nInstIndex;
	grassBatchVertexInput IN;
	FetchBatchInstanceVertexData(instIN, IN, nInstIndex);

	vertexOutputUnlit OUT;

	float3 vertpos = (float3)IN.base.pos;
	float3 worldNormal = IN.base.normal; // geometric normal

#if CAMERA_FACING || CAMERA_ALIGNED
	const float3 vCentre = IN.base.RelativeCentre.xyz;
	ApplyCameraAlignment(vertpos, worldNormal, vCentre);
#endif

    // Write out final position & texture coords
    float2 localUV0	= IN.base.texCoord0;
	float4 IN_Color0 = IN.base.color0;	// used fields: rgb 
	float4 IN_Color1 = IN.base.color1;	// used fields: rba

	float4 vertColor = float4(IN.inst.color_scale.rgb, IN_Color1.r);
	float3 posOffset = float3(IN.inst.pos, IN.inst.posZ) * vecBatchAabbDelta;
	float3 translation = vecBatchAabbMin + posOffset;

	float2 dxtNrml = IN.inst.normal*2-1;
	float3 terrainNormal = float3(dxtNrml, sqrt(1-dot(dxtNrml, dxtNrml)));

#if GRASS_BATCH_CS_CULLING
	float fade = IN.csData.Scale;
#else //GRASS_BATCH_CS_CULLING
	float fade = ComputeInstanceFade(IN.global, nInstIndex);
#endif //GRASS_BATCH_CS_CULLING

	//Compute scale.
	const float perInstScale = ((gScaleRange.y - gScaleRange.x) * IN.inst.color_scale.w) + gScaleRange.x;

	const float scaleRange = gScaleRange.y * gScaleRange.z;
	float randScale = IN.global.worldMatA.w * scaleRange * 2; //*2 b/c art wants scale range to be +/-
	randScale -= scaleRange;

	float finalScale = max(perInstScale + randScale, 0.0f) * fade;

	vertpos = vertpos.xyz * finalScale.xxx;

	//Orient grass to supplied terrain normal
	float3x3 rotMtx = float3x3(	IN.global.worldMatA.xyz,
								IN.global.worldMatB.xyz,
								IN.global.worldMatC.xyz	);

	//Note:	This assumes that grass will only ever be placed on terrain with a normal that has a positive value in the up direction.
	//		Then again, so does the terrain normal decompression code above!
	float3 terrainNormSlerp = rageSlerp(float3(0, 0, 1), terrainNormal, gOrientToTerrain);
	float3x3 terrainMtx = ComputeRotationMatrix(float3(0, 0, 1), terrainNormSlerp);

#if COMPUTE_COMPOSITE_WORLD_MTX
	float3x3 worldMtx3x3 = mul(rotMtx, terrainMtx);

	float4x4 worldMtx;
	worldMtx[0] = float4(worldMtx3x3[0], 0);
	worldMtx[1] = float4(worldMtx3x3[1], 0);
	worldMtx[2] = float4(worldMtx3x3[2], 0);
	worldMtx[3] = float4(translation, 1);
#endif //COMPUTE_COMPOSITE_WORLD_MTX

	float4	worldPos;

#if ENABLE_TRAIL_MAP
	#if COMPUTE_COMPOSITE_WORLD_MTX
		// Do a non wind-applied transform for texture lookup
		float2 prevWind = worldMtx[2].xy;
		worldMtx[2].xy = 0;
		worldPos = mul(float4(vertpos,1.0f), worldMtx);

		worldNormal = mul(worldNormal, worldMtx3x3);
	#else //COMPUTE_COMPOSITE_WORLD_MTX
		// NOTE:	Grass batches do not currently have the same style wind applied to them as the proc grass system. So no need to do this.
		//			(Removing this makes it simplier to support non-composite world matrix.
		worldPos.xyz = mul(vertpos, rotMtx);
		worldPos.xyz = mul(worldPos.xyz, terrainMtx);
		worldPos.xyz += translation;
		worldPos.w = 0.0f;

		worldNormal = mul(worldNormal, rotMtx);
		worldNormal = mul(worldNormal, terrainMtx);
		worldNormal = normalize(worldNormal);
	#endif //COMPUTE_COMPOSITE_WORLD_MTX

	float2 grassDir;
	float intensity;
	GetTrailMapValues(worldPos, vecPlayerPos, grassDir, intensity);

	#if COMPUTE_COMPOSITE_WORLD_MTX
		//Re-transform to get wind
		worldMtx[2].xy = prevWind*(intensity > 0.9 ? 0.0 : 1.0 );
	#endif //COMPUTE_COMPOSITE_WORLD_MTX
#endif //ENABLE_TRAIL_MAP

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
#if ENABLE_TRAIL_MAP
	vertpos.xyz				+= uMovementAdd * ( intensity > 0.9 ? 0.0f : intensity );
#else
	vertpos.xyz				+= uMovementAdd;
#endif

#if USE_GRASS_WIND_BENDING
	//Compute wind bending
	float seed = cos(dot(translation, translation)) * 0.5 + 0.5;
	float bending = (1.0f + (wind_bend_var * seed)) * wind_bend_scale;

#if COMPUTE_COMPOSITE_WORLD_MTX
	worldMtx[2].xy = 
#else //COMPUTE_COMPOSITE_WORLD_MTX
	terrainMtx[2].xy = 
#endif //COMPUTE_COMPOSITE_WORLD_MTX
		gWindBendingGlobals.xy * bending;
#endif //USE_GRASS_WIND_BENDING

// do local->global transform for this plant:
#if COMPUTE_COMPOSITE_WORLD_MTX
	worldPos = mul(float4(vertpos,1.0f), worldMtx);
	worldNormal = mul(worldNormal, wolrdMtx3x3);
	worldNormal = normalize(worldNormal);
#else
	worldPos.xyz = mul(vertpos, rotMtx);
	worldPos.xyz = mul(worldPos.xyz, terrainMtx);
	worldPos.xyz += translation;
	worldPos.w = 0.0f;

	worldNormal = mul(worldNormal, rotMtx);
	worldNormal = mul(worldNormal, terrainMtx);
	worldNormal = normalize(worldNormal);
#endif
	vertpos = (float3)worldPos;

	// player's collision:
	if(bPlayerColl && 1)
	{
		float3 vecDist3 = vertpos.xyz - vecPlayerPos.xyz;

		float distc2	= dot(vecDist3, vecDist3);
		float coll		= (collPedSphereRadiusSqr - distc2) * (collPedInvSphereRadiusSqr);

		//coll = max(coll, 0.0f);
		//coll = min(coll, 1.0f);
		coll = saturate(coll);

//		vertpos.xyz += coll*normalize(vecDist3)  * (IN_Color1.b);
		vertpos.xy += (float2)0.5f*coll*normalize(vecDist3).xy * (IN_Color1.b);		// blue1: scale for collision (useful for flower tops, etc.)
//		vertpos.z -= abs((float)coll*normalize(vecDist3).z * (1.0f-IN.color0.b));
	}


	// vehicle collision:
#if __SHADERMODEL >= 40
	if(bVehicleColl && bVehColl0Enabled)
#else
	if(bVehColl0Enabled)
#endif
	{
		vertpos.xyz = VS_ApplyVehicleCollision(vertpos, coll0VehVecB, coll0VehVecM, coll0VehInvDotMM, coll0VehR, coll0VehRR, coll0VehInvRR, coll0VehGroundZ, IN_Color1.b, coll0VehColDistLimit);
	}
	
#if __SHADERMODEL >= 40
	if(bVehicleColl && bVehColl1Enabled)
#else
	if(bVehColl1Enabled)
#endif
	{
		vertpos.xyz = VS_ApplyVehicleCollision(vertpos, coll1VehVecB, coll1VehVecM, coll1VehInvDotMM, coll1VehR, coll1VehRR, coll1VehInvRR, coll1VehGroundZ, IN_Color1.b, coll1VehColDistLimit);
	}

#if __SHADERMODEL >= 40
	if(bVehicleColl && bVehColl2Enabled)
#else
	if(bVehColl2Enabled)
#endif
	{
		vertpos.xyz = VS_ApplyVehicleCollision(vertpos, coll2VehVecB, coll2VehVecM, coll2VehInvDotMM, coll2VehR, coll2VehRR, coll2VehInvRR, coll2VehGroundZ, IN_Color1.b, coll2VehColDistLimit);
	}

#if __SHADERMODEL >= 40
	if(bVehicleColl && bVehColl3Enabled)
#else
	if(bVehColl3Enabled)
#endif
	{
		vertpos.xyz = VS_ApplyVehicleCollision(vertpos, coll3VehVecB, coll3VehVecM, coll3VehInvDotMM, coll3VehR, coll3VehRR, coll3VehInvRR, coll3VehGroundZ, IN_Color1.b, coll3VehColDistLimit);
	}

#if ENABLE_TRAIL_MAP
	vertpos = FlattenGrass(vertpos, terrainNormal, grassDir, intensity, 1.0 - IN_Color0.z);
#endif

    OUT.pos				= mul(float4(vertpos,1.0f),	gWorldViewProj);
    OUT.worldNormal.xyz	= worldNormal.xyz;
    OUT.worldNormal.w	= localUV0.x;
	OUT.color0			= vertColor;
#if NEEDS_WORLD_POSITION
	OUT.worldPos.xyz	= vertpos.xyz; //worldPos.xyz; //Shouldn't this be the same as the position we transform?
	OUT.worldPos.w		= localUV0.y;
#endif
#if GRASS_BATCH_CS_CULLING
	OUT.alpha_ao.x		= IN.csData.CrossFade * IN.csData.Scale;
	OUT.alpha_ao.y		= IN.inst.ao_pad3.x;
#endif

	//Top vs Bottom of plant;
	float isTop = IN_Color1.a;
#if CPLANT_WRITE_GRASS_NORMAL || USE_SSA_STIPPLE_ALPHA
	float selfShadow = 1.0f;	// we aren't using the fake normal anymore	//saturate(dot(worldNormal.xyz, -fakedGrassNormal) * 8.);
	float plantAO = lerp(globalAO, 1.0f, isTop);
	OUT.selfShadow_AO_SSAThresholds = float4(selfShadow, plantAO, 0.0f.xx);
	#if USE_SSA_STIPPLE_ALPHA
		OUT.selfShadow_AO_SSAThresholds.zw = gStippleThreshold.xy;
	#endif
#endif

	float groundBlend	= IN_Color1.r;		// ground blend amount

#if USE_GROUND_COLOR
	OUT.groundColor0	= float4(groundColorD.rgb, groundBlend);
#endif

#if CPLANT_WRITE_GRASS_NORMAL
	float3 grassNormal = ComputeGrassNormal(terrainNormal, fakedGrassNormal, worldPos.xyz, isTop);
	OUT.worldNormal.xyz = (grassNormal.xyz + 1.0f) * 0.5f;
#else
	OUT.worldNormal.xyz = worldNormal;
#endif

#if COLOR_FLAT_GRASS
  	OUT.groundColor0 = lerp( float4( 1, 0, 0, 1 ), OUT.groundColor0, 1.0-intensity );
#endif

	return(OUT);
}

vertexOutputUnlit VS_Transform(grassInstancedBatchVertexInput instIN)
{
	return VS_TransformInternal(instIN, true, true);
}

#if __MAX
//
//
//
//
vertexOutputUnlit VS_MaxTransformUnlit(maxVertexInput IN)
{
vertexOutputUnlit OUT;

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
#if USE_GROUND_COLOR
	OUT.groundColor0	= float4(1,1,1,0);
#endif
	
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


VS_CascadeShadows_OUT VS_Cascade_Transform(grassInstancedBatchVertexInput instIN)
{
	VS_CascadeShadows_OUT OUT;
	vertexOutputUnlit VSOut = VS_TransformInternal(instIN, false, true);

	OUT.pos = VSOut.pos;
#ifdef SHADOW_USE_TEXTURE
	OUT.tex = float2(VSOut.worldNormal.w, VSOut.worldPos.w);
#endif // SHADOW_USE_TEXTURE

	return OUT;
}


//
// pixel shaders:
//
//
float4 PS_TexturedUnlit(vertexOutputUnlit IN ) : COLOR
{
	float2 localUV0 = float2(IN.worldNormal.w, IN.worldPos.w);
	float4 colortex	= tex2D(DiffuseSampler, localUV0);
	float4 color = float4(lerp(colortex.rgb, colortex.rgb * IN.color0.rgb, IN.color0.a), colortex.a); //Apply tint

	// modulate with ground color:
#if USE_GROUND_COLOR
	color.rgb = lerp(color.rgb, IN.groundColor0.rgb, IN.groundColor0.w);
#endif
	color.rgb *= gEmissiveScale;

	return PackHdr(color);
}


//
//
// pixel shaders:
//
DeferredGBuffer PS_GrassDeferredTextured0(vertexOutputUnlit IN,
											inout StandardLightingProperties surfaceStandardLightingProperties)
{
	DeferredGBuffer OUT;

	float2 localUV0 = float2(IN.worldNormal.w, IN.worldPos.w);

	SurfaceProperties surfaceInfo = GetSurfaceProperties(
		GRASS_BATCH_CS_CULLING_SWITCH(float4(1.0f.xxx, IN.alpha_ao.x), 1.0f.xxxx), //float4(IN.color0.rgb, 1.0f), //Note: Color is tint, not vert color. So we pass in 1.0f.xxx
		localUV0,
		DiffuseSampler,
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

	//Apply tint
	surfaceStandardLightingProperties.diffuseColor.rgb = lerp(surfaceStandardLightingProperties.diffuseColor.rgb, surfaceStandardLightingProperties.diffuseColor.rgb * IN.color0.rgb, IN.color0.a);

	// modulate with ground color:
#if USE_GROUND_COLOR
	surfaceStandardLightingProperties.diffuseColor.rgb =
			lerp(surfaceStandardLightingProperties.diffuseColor.rgb, IN.groundColor0.rgb, IN.groundColor0.w);
#endif

	//Apply global alpha.
	surfaceStandardLightingProperties.diffuseColor.a *= globalAlpha;

	// we don't want ambientScale for grass:
	surfaceStandardLightingProperties.naturalAmbientScale = GRASS_BATCH_CS_CULLING_SWITCH(IN.alpha_ao.y, globalAO);
	surfaceStandardLightingProperties.artificialAmbientScale = 0.0f;
	
	// Pack the information into the GBuffer(s).
	OUT = PackGBuffer( surfaceInfo.surface_worldNormal, surfaceStandardLightingProperties );

	OUT.col1.a = 1.0f;	// grass material bit for foliage lighting pass

	float alpha = surfaceStandardLightingProperties.diffuseColor.a;
	OUT.col0.a = alpha;
#if !SHADER_FINAL
	OUT.col0.a = bDebugAlphaOverdraw?  1-OUT.col0.a : OUT.col0.a;
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
	OUT.col3.xy = sqrt(OUT.col3.xy * 0.5);
#endif

	OUT.col3.a = Pack2ZeroOneValuesToU8(0.0f, ShadowFalloff);

	return(OUT);
}

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


GrassGBuffer PS_DeferredTextured(vertexOutputUnlit IN)
{
	StandardLightingProperties surfaceStandardLightingProperties = (StandardLightingProperties)0;
	GrassGBuffer OUT = PackGrassGBuffer(PS_GrassDeferredTextured0(IN, surfaceStandardLightingProperties));

	ApplySSAStipple(IN, surfaceStandardLightingProperties);

#if ((__PS3 && (!PS3_ALPHATOMASK_PASS)) || __WIN32PC) && (!USE_SSA_STIPPLE_ALPHA)

		// PS3: there's no AlphaTest in MRT mode, so we simulate AlphaTest functionality here:
		// equal to:
		// AlphaTest = true;
		// AlphaRef = 48.0f;
		// AlphaFunc = Greater;
		const float4 diffColor = surfaceStandardLightingProperties.diffuseColor;
//		const float colCmp = dot( diffColor.rgb*diffColor.a, float3(1.0f/3.0f,1.0f/3.0f,1.0f/3.0f));
		const float alphaCmp = diffColor.a;

		#if __PS3
				bool killPix	= bool(alphaCmp < grass_alphaRef); // AlphaTest);
				bool killPix2	= AlphaToMask(alphaCmp, IN.pos);

				if(killPix || killPix2) 	discard;
				//OUT.col0.rgb = lerp(OUT.col0.rgb, float3(1,0,0), killPix2);
		#else
			clip( int(alphaCmp	>= /*AlphaTest*/ grass_alphaRef)-1 );	// clip pixel, if comparison result is <0
		#endif
#endif //AlphaTest...

	return(OUT);
}

GrassGBuffer PS_DeferredTexturedLOD(vertexOutputUnlit IN)
{
	StandardLightingProperties surfaceStandardLightingProperties = (StandardLightingProperties )0;
	GrassGBuffer OUT =  PackGrassGBuffer(PS_GrassDeferredTextured0(IN, surfaceStandardLightingProperties));

	ApplySSAStipple(IN, surfaceStandardLightingProperties);

#if PS3 && (!PS3_ALPHATOMASK_PASS) && (!USE_SSA_STIPPLE_ALPHA)
	const float4 diffColor = surfaceStandardLightingProperties.diffuseColor;
	const float alphaCmp = diffColor.a;
		bool killPix	= bool(alphaCmp < grass_alphaRef); // AlphaTest);
		bool killPix2	= AlphaToMask(alphaCmp, vPos);	// only alpha range stippling
		if(killPix || killPix2)	discard;
#endif //AlphaTest...
		
	return(OUT);
}

//
//
// PS3: compatibility replacement PS for map models using grass shader, but not rendered with PlantsMgr
//      (in MRT PS must output 3 regs)
GrassGBuffer	PS_DeferredTexturedStd(vertexOutputUnlit IN)
{
	StandardLightingProperties surfaceStandardLightingProperties = (StandardLightingProperties)0;
	GrassGBuffer OUT =  PackGrassGBuffer(PS_GrassDeferredTextured0(IN, surfaceStandardLightingProperties));

	ApplySSAStipple(IN, surfaceStandardLightingProperties);

	return(OUT);
}

// techniques:
//
//
//
//
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
			VertexShader= compile VERTEXSHADER	VS_MaxTransformUnlit();
			PixelShader	= compile PIXELSHADER	PS_TexturedUnlit();
		}  
	}
#else //__MAX...

	#if DEFERRED_TECHNIQUES && NON_CUTOUT_DEFERRED_TECHNIQUES && !FOLIAGE_2GBUF_OPT
	technique deferred_draw
	{
		pass p0
		{
			SetGlobalDeferredMaterial(DEFERRED_MATERIAL_TREE);
			VertexShader		= compile VERTEXSHADER	VS_Transform();
			PixelShader			= compile PIXELSHADER	PS_DeferredTexturedStd()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif // DEFERRED_TECHNIQUES && NON_CUTOUT_DEFERRED_TECHNIQUES && !FOLIAGE_2GBUF_OPT

	#if DEFERRED_TECHNIQUES && NON_CUTOUT_DEFERRED_TECHNIQUES && DRAWSKINNED_TECHNIQUES && !FOLIAGE_2GBUF_OPT
	technique deferred_drawskinned
	{
		pass p0
		{
			SetGlobalDeferredMaterial(DEFERRED_MATERIAL_TREE);
			VertexShader		= compile VERTEXSHADER	VS_Transform();
			PixelShader			= compile PIXELSHADER	PS_DeferredTextured()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}	
	#endif // DEFERRED_TECHNIQUES && NON_CUTOUT_DEFERRED_TECHNIQUES && DRAWSKINNED_TECHNIQUES && !FOLIAGE_2GBUF_OPT

	#if DEFERRED_TECHNIQUES && FOLIAGE_2GBUF_OPT
	technique deferred2gbuff_draw
	{
		pass p0
		{
			SetGlobalDeferredMaterial(DEFERRED_MATERIAL_TREE);
			VertexShader		= compile VERTEXSHADER	VS_Transform();
			PixelShader			= compile PIXELSHADER	PS_DeferredTexturedStd()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif

	#if DEFERRED_TECHNIQUES && FOLIAGE_2GBUF_OPT && DRAWSKINNED_TECHNIQUES
	technique deferred2gbuff_drawskinned
	{
		pass p0
		{
			SetGlobalDeferredMaterial(DEFERRED_MATERIAL_TREE);
			VertexShader		= compile VERTEXSHADER	VS_Transform();
			PixelShader			= compile PIXELSHADER	PS_DeferredTexturedStd()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif

	#if DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES && !FOLIAGE_2GBUF_OPT
	technique deferredalphaclip_draw
	{
		pass p0
		{
			SetGlobalDeferredMaterial(DEFERRED_MATERIAL_TREE);
			VertexShader		= compile VERTEXSHADER	VS_Transform();
			PixelShader			= compile PIXELSHADER	PS_DeferredTexturedStd()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif // DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES && !FOLIAGE_2GBUF_OPT

	#if DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES && DRAWSKINNED_TECHNIQUES && __PS3 && !FOLIAGE_2GBUF_OPT
	technique deferredalphaclip_drawskinned
	{
		pass p0
		{
			SetGlobalDeferredMaterial(DEFERRED_MATERIAL_TREE);
			VertexShader		= compile VERTEXSHADER	VS_Transform();
			PixelShader			= compile PIXELSHADER	PS_DeferredTexturedStd()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif // DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES && DRAWSKINNED_TECHNIQUES && __PS3 && !FOLIAGE_2GBUF_OPT


	//
	//
	//
	//
	#if FORWARD_TECHNIQUES
	technique draw
	{
		pass p0
		{
			VertexShader = compile VERTEXSHADER	VS_Transform();
			PixelShader  = compile PIXELSHADER	PS_TexturedUnlit()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif // FORWARD_TECHNIQUES

	#if UNLIT_TECHNIQUES
	technique unlit_draw
	{
		pass p0
		{
			VertexShader = compile VERTEXSHADER	VS_Transform();
			PixelShader  = compile PIXELSHADER	PS_TexturedUnlit()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
	#endif // UNLIT_TECHNIQUES

#if GRASS_BATCH_CS_CULLING
#	include "BatchCS.fxh"
#endif //GRASS_BATCH_CS_CULLING

#define ES_ALPHACLIP_FUNC_INPUT vertexOutputUnlit
#define ES_ALPHACLIP_FUNC(IN) PS_DeferredTexturedStd(IN)
#include "../../Debug/EntitySelect.fxh"
SHADERTECHNIQUE_ENTITY_SELECT(VS_Transform(), VS_Transform())
SHADERTECHNIQUE_CASCADE_SHADOWS_GRASS()

#endif //__MAX...

#endif //TECHNIQUES
