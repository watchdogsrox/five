// 
//	trees_common.fxh
//
//	2009/11/30	-	Andrzej:	- trees_common.fxh added;
//	2011/03/10	-	Andrzej:	- USE_TREE_LOD added;
//
//
//
//
#ifndef __TREES_COMMON_FXH__
#define __TREES_COMMON_FXH__

#if 1
	#undef USE_INSTANCED		// Temp disable treelod instancing
#endif

#define TREE_DRAW

#define CAN_BE_CUTOUT_SHADER

#if defined(USE_ALPHACLIP_FADE) && !RSG_ORBIS && !RSG_DURANGO// && !MULTISAMPLE_TECHNIQUES && !RSG_DURANGO && !RSG_ORBIS
	#define ALPHA_CLIP 1
#else
	#define ALPHA_CLIP 0
#endif	

#define DISABLE_DISCARDS					(0)

#define RANDOMIZED_DITHER (0)

#if __PS3
	#define MIN_MIP_LEVEL	4
	#define MAX_MIP_LEVEL	0
#endif

#ifdef USE_TREE_LOD
	#define TREE_LOD						(1)	// cheaper crosspoly trees
#else
	#define TREE_LOD						(0)
#endif

#ifdef USE_TREE_LOD2
	#define TREE_LOD2						(1)	// tree billboards
#else
	#define TREE_LOD2						(0)
#endif

#ifdef USE_TREE_LOD2_FOR_DRAWABLES
	#define TREE_LOD2_DRAWABLE				(TREE_LOD2 && 1)	// tree billboards for drawable LODs
#else
	#define TREE_LOD2_DRAWABLE				(0)
#endif

#define SKINNED_TREES						(((RSG_PC && (__SHADERMODEL>=40)) || RSG_ORBIS || RSG_DURANGO) && !TREE_LOD && !TREE_LOD2)	// skinned techniques required for skinned frags on 360 and PC (and only for HD models)
#if !SKINNED_TREES
	#define NO_SKINNING
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
	#define SPECULAR_PS_INPUT				IN.texCoord.xy
	#define SPECULAR_VS_INPUT				IN.texCoord0.xy
	#define SPECULAR_VS_OUTPUT				OUT.texCoord.xy
	#define SPECULAR_UV1					(0)
#else
	#define SPECULAR						(0)
	#define SPEC_MAP						(0)
#endif		// USE_SPECULAR

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

#define USE_NORMAL_ALPHA_SCALE
#ifdef USE_NORMAL_ALPHA_SCALE
	#define NORMAL_ALPHA_SCALE (1)
#else
	#define NORMAL_ALPHA_SCALE (0)
#endif

#define ENABLE_BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS (TREES_ENABLE_BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS || __MAX) && !__LOW_QUALITY

#ifdef USE_WIND_DISPLACEMENT
	#define WIND_DISPLACEMENT (1 && ENABLE_BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS)
#else
	#define WIND_DISPLACEMENT (0)
#endif // USE_WIND_DISPLACEMENT

// SSTA is pretty expensive and doesn't provide much benefit on consoles now that we've got EQAA. We'll leave it enabled on PC for now.
#if !TREE_LOD && !TREE_LOD2 && ((__SHADERMODEL >=40) && !__LOW_QUALITY && RSG_PC)
#define USE_SSTAA
#endif // USE_SSTAA

#define USE_DIFFUSE_SAMPLER

#define NO_SELF_SHADOW
#define PRAGMA_CONSTANT_TREE
#include "../../common.fxh"
#include "trees_shared.h"
#include "trees_windfuncs.fxh"
#if SKINNED_TREES
	#include "..\..\Util\skin.fxh"
#endif

#ifdef USE_SHADOW_PROXY_VS_INPUT
#define IS_SHADOW_PROXY		(1)

#if SPECULAR || NORMAL_MAP|| CAMERA_FACING || CAMERA_ALIGNED || TREE_LOD || TREE_LOD2 || WIND_DISPLACEMENT
#error "Can`t use shadow proxy with any of the above"
#endif

#else
#define IS_SHADOW_PROXY		(0)
#endif // USE_SHADOW_PROXY_VS_INPUT

//#endif

#define DISABLE_SSA		(0 && TREE_LOD2)

#define FRESNEL								(SPECULAR)

#include "..\..\Util\camera.fxh"

#if INSTANCED
	//For tree instancing:
	//float3x4 gWorldInstanceMatrix;	=> 3
	//float4 gGlobals;					=> 1	//Globals = {globalAlpha, gArtificialAmbientScale, gNaturalAmbientScale, <UNUSED>}
	//float4 gPerInstance;				=> 1	//Vars = {1,1,1, invGlobalEntityScaleZ}
	//Total size of 5 registers
	#define NUM_CONSTS_PER_INSTANCE				(5)
	#define MAX_TREE_INSTANCES_PER_DRAW			(MAX_INSTANCES_PER_DRAW)
	BeginConstantBufferPagedDX10(rage_instmtx,PASTE2(b,INSTANCE_CONSTANT_BUFFER_SLOT))
		float4 gInstanceVars[NUM_CONSTS_PER_INSTANCE * MAX_TREE_INSTANCES_PER_DRAW] REGISTER2(vs, PASTE2(c, INSTANCE_SHADER_CONSTANT_SLOT));
	EndConstantBufferDX10(rage_instmtx)
#endif

#define PLAYER_TREE_COLL_RADIUS				(0.75f)												// radius of influence
#define gCollPedSphereRadiusSqr				(PLAYER_TREE_COLL_RADIUS*PLAYER_TREE_COLL_RADIUS)
#define gCollPedInvSphereRadiusSqr			(1.0f/gCollPedSphereRadiusSqr)

#if (__SHADERMODEL >= 40)
	#define BOOLEAN int
#else
	#define BOOLEAN bool
#endif

#define STENCIL_TREES_AGAINST_VEHICLE_INTERIOR 1

#if STENCIL_TREES_AGAINST_VEHICLE_INTERIOR
#define DEFERRED_MATERIAL_INTERIOR_VEH DEFERRED_MATERIAL_SPAREOR2
#define DEFERRED_MATERIAL_TREE_PLUS_INTERIOR_VEH 0x43 //DEFERRED_MATERIAL_TREE | DEFERRED_MATERIAL_INTERIOR_VEH
#define DEFERRED_MATERIAL_NOT_INTERIOR_VEH 0xBF //~DEFERRED_MATERIAL_INTERIOR_VEH

#define SetGlobalDeferredMaterialTree()			StencilRef=DEFERRED_MATERIAL_TREE_PLUS_INTERIOR_VEH; \
												StencilEnable=true; \
												StencilFunc=NOTEQUAL; \
												StencilPass=REPLACE; \
												StencilFail=KEEP; \
												StencilMask=DEFERRED_MATERIAL_INTERIOR_VEH; \
												StencilWriteMask=DEFERRED_MATERIAL_NOT_INTERIOR_VEH; \
												ZFunc=FixedupLESS
												
#else
#define SetGlobalDeferredMaterialTree()			StencilRef=DEFERRED_MATERIAL_TREE; \
												StencilEnable=true; \
												StencilPass=REPLACE; \
												ZFunc=FixedupLESS
#endif // STENCIL_TREES_AGAINST_VEHICLE_INTERIOR

CBSHARED BeginConstantBufferPagedDX10(playercolenabled_buffer, b8)
	shared BOOLEAN	bPlayerCollEnabled		: bPlayerCollEnabled0;		//REGISTER(b10) = false;
	shared BOOLEAN	bVehTreeColl0Enabled	: bVehTreeColl0Enabled0;		//REGISTER(b4) = false;
	shared BOOLEAN	bVehTreeColl1Enabled	: bVehTreeColl1Enabled0;		//REGISTER(b5) = false;
	shared BOOLEAN	bVehTreeColl2Enabled	: bVehTreeColl2Enabled0;		//REGISTER(b6) = false;
	shared BOOLEAN	bVehTreeColl3Enabled	: bVehTreeColl3Enabled0;		//REGISTER(b7) = false;
EndConstantBufferDX10(playercolenabled_buffer)

CBSHARED BeginConstantBufferPagedDX10(trees_common_shared_locals, b9)
	shared	float4 _worldPlayerPos_umGlobalPhaseShift : worldPlayerPos_umGlobalPhaseShift0 REGISTER(c218) = float4(0,0,0,0);
	#define gWorldPlayerPos				(_worldPlayerPos_umGlobalPhaseShift.xyz)
	#define umGlobalPhaseShift			(_worldPlayerPos_umGlobalPhaseShift.w)		// micro-movement phase shift

	// vehicle collision params:
	// vehicle#0:
	shared float4		_vecvehColl0[3]	: vecVehColl0 REGISTER(c219) = {float4(0,0,0,0), float4(0,0,0,0), float4(0,0,0,0)};
	#define _vecVehColl0B (_vecvehColl0[0]) //c219
	#define _vecVehColl0M (_vecvehColl0[1]) //c220
	#define _vecVehColl0R (_vecvehColl0[2]) //c221

	#define coll0VehVecB			(_vecVehColl0B.xyz)	// start segment point B
	#define coll0VehVecM			(_vecVehColl0M.xyz)	// segment vector M
	#define coll0VehInvDotMM		(_vecVehColl0M.w)	// 1/dot(M,M)
	#define coll0VehR				(_vecVehColl0R.x)	// collision radius
	#define coll0VehRR				(_vecVehColl0R.y)	// R*R
	#define coll0VehInvRR			(_vecVehColl0R.z)	// 1/(R*R)
	#define coll0VehGroundZ			(_vecVehColl0R.w)

	// vehicle#1:
	shared float4		_vecvehColl1[3]	: vecVehColl1 REGISTER(c222) = {float4(0,0,0,0), float4(0,0,0,0), float4(0,0,0,0)};
	#define _vecVehColl1B (_vecvehColl1[0]) //c222
	#define _vecVehColl1M (_vecvehColl1[1]) //c223
	#define _vecVehColl1R (_vecvehColl1[2]) //c224

	#define coll1VehVecB			(_vecVehColl1B.xyz)	// start segment point B
	#define coll1VehVecM			(_vecVehColl1M.xyz)	// segment vector M
	#define coll1VehInvDotMM		(_vecVehColl1M.w)	// 1/dot(M,M)
	#define coll1VehR				(_vecVehColl1R.x)	// collision radius
	#define coll1VehRR				(_vecVehColl1R.y)	// R*R
	#define coll1VehInvRR			(_vecVehColl1R.z)	// 1/(R*R)
	#define coll1VehGroundZ			(_vecVehColl1R.w)

	// vehicle#2:
	shared float4		_vecvehColl2[3]	: vecVehColl2 REGISTER(c225) = {float4(0,0,0,0), float4(0,0,0,0), float4(0,0,0,0)};
	#define _vecVehColl2B (_vecvehColl2[0]) //c225
	#define _vecVehColl2M (_vecvehColl2[1]) //c226
	#define _vecVehColl2R (_vecvehColl2[2]) //c227

	#define coll2VehVecB			(_vecVehColl2B.xyz)	// start segment point B
	#define coll2VehVecM			(_vecVehColl2M.xyz)	// segment vector M
	#define coll2VehInvDotMM		(_vecVehColl2M.w)	// 1/dot(M,M)
	#define coll2VehR				(_vecVehColl2R.x)	// collision radius
	#define coll2VehRR				(_vecVehColl2R.y)	// R*R
	#define coll2VehInvRR			(_vecVehColl2R.z)	// 1/(R*R)
	#define coll2VehGroundZ			(_vecVehColl2R.w)

	// vehicle#3:
	shared float4		_vecvehColl3[3]	: vecVehColl3 REGISTER(c228) = {float4(0,0,0,0), float4(0,0,0,0), float4(0,0,0,0)};
	#define _vecVehColl3B (_vecvehColl3[0]) //c228
	#define _vecVehColl3M (_vecvehColl3[1]) //c229
	#define _vecVehColl3R (_vecvehColl3[2]) //c230

	#define coll3VehVecB			(_vecVehColl3B.xyz)	// start segment point B
	#define coll3VehVecM			(_vecVehColl3M.xyz)	// segment vector M
	#define coll3VehInvDotMM		(_vecVehColl3M.w)	// 1/dot(M,M)
	#define coll3VehR				(_vecVehColl3R.x)	// collision radius
	#define coll3VehRR				(_vecVehColl3R.y)	// R*R
	#define coll3VehInvRR			(_vecVehColl3R.z)	// 1/(R*R)
	#define coll3VehGroundZ			(_vecVehColl3R.w)


	// um override params:
	shared	float4 umGlobalOverrideParams : umGlobalOverrideParams0 REGISTER(c231) < int nostrip = 1; > = float4(1.0f, 1.0f, 1.0f, 1.0f);
	#define umOverrideScaleH		(umGlobalOverrideParams.x)	// override scaleH
	#define umOverrideScaleV		(umGlobalOverrideParams.y)	// override scaleV
	#define umOverrideArgFreqH		(umGlobalOverrideParams.z)	// override ArgFreqH
	#define umOverrideArgFreqV		(umGlobalOverrideParams.w)	// override ArgFreqV

	shared	float4 _globalEntityScale : globalEntityScale0 REGISTER(c232) = float4(1.0f, 1.0f, 1.0f, 1.0f);
	#define GlobalEntityScaleXY		(_globalEntityScale.x) 
	#define GlobalEntityScaleZ		(_globalEntityScale.y) 
	#define invGlobalEntityScaleXY	(_globalEntityScale.z) 
	#define invGlobalEntityScaleZ	(_globalEntityScale.w) 

EndConstantBufferDX10(trees_common_shared_locals)


BEGIN_RAGE_CONSTANT_BUFFER(trees_common_locals,b10)
#if SPECULAR
	#if FRESNEL
		float specularFresnel : Fresnel
		<
			string UIName = "Specular Fresnel";
			float UIMin = 0.0;
			float UIMax = 1.0;
			float UIStep = 0.01;
			int nostrip=1;	// dont strip
		> = 0.97f;
	#endif

	float specularFalloffMult : Specular
	<
		string UIName = "Specular Falloff";
		float UIMin = 0.0;
		float UIMax = 2000.0;
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
	> = 1.0;
#endif

	#if SPEC_MAP
		float3 specMapIntMask : SpecularMapIntensityMask
		<
			string UIWidget = "slider";
			float UIMin = 0.0;
			float UIMax = 1.0;
			float UIStep = .01;
			string UIName = "specular map intensity mask color";
		> = { 1.0, 0.0, 0.0};
	#endif	// SPEC_MAP
#endif		// SPECULAR

#if NORMAL_MAP
	float bumpiness : Bumpiness
	<
		string UIWidget = "slider";
		float UIMin = 0.0;
		float UIMax = 200.0;
		float UIStep = .01;
		string UIName = "Bumpiness";
	> = 1.0;
#endif // NORMAL_MAP...

#define umGlobalTimer				(gUmGlobalTimer)			// micro-movement timer


#if WIND_DISPLACEMENT

float4 umTriWave1Params : umTriWave1Params0 
<
	string UIWidget	= "slider";
	string UIName	= "tri wave 1: freg low/amp low/freq high/amp high";
	float UIMin		= 0.0;
	float UIMax		= 5.0;
	float UIStep	= 0.001;
> = float4(0.053f, 0.020f, 0.043f, 0.110f);

float4 umTriWave2Params : umTriWave2Params0 
<
	string UIWidget	= "slider";
	string UIName	= "tri wave 1: freg low/amp low/freq high/amp high";
	float UIMin		= 0.0;
	float UIMax		= 5.0;
	float UIStep	= 0.001;
> = float4(0.046f, 0.019f, 0.028f, 0.048f);

float4 umTriWave3Params : umTriWave3Params0 
<
	string UIWidget	= "slider";
	string UIName	= "tri wave 3: freg low/amp low/freq high/amp high";
	float UIMin		= 0.0;
	float UIMax		= 5.0;
	float UIStep	= 0.001;
> = float4(0.079f, 0.012f, 0.010f, 0.029f);

float4 foliageBranchBendPivot : foliageBranchBendPivot0 
<
	string UIWidget	= "slider";
	string UIName	= "branch bend pivot + stiffness adjust";
	float UIMin		= -3.0;
	float UIMax		= 200.0;
	float UIStep	= 0.01;
> = float4(3.150f, 0.0f, 0.0f, 0.0f);

float4 foliageBranchBendStiffnessAdjust : foliageBranchBendStiffnessAdjust0 
<
	string UIWidget	= "slider";
	string UIName	= "stiffness adjust";
	float UIMin		= 0.0;
	float UIMax		= 50.0;
	float UIStep	= 0.01;
> = float4(0.0f, 1.0f, 0.0f, 1.0f);

#else // WIND_DISPLACEMENT

// micro-movement params: [globalScaleH | globalScaleV | globalArgFreqH | globalArgFreqV ]
float4 umGlobalParams : umGlobalParams0 
<
	string UIWidget	= "slider";
	string UIName	= "Wind: (x64/Xbox1/Ps4): ---/---/Stiffness Multiplier/--- (ps3/360):SclH/SclV/FrqH/FrqV";
	float UIMin		= 0.0;
	float UIMax		= 2.0;
	float UIStep	= 0.001;
> = float4(0.025f, 0.020f, 1.000f, 0.500f);

#if TREES_USE_ALPHA_CARD_ONLY_BENDING && !__LOW_QUALITY

#if TREES_USE_GLOBAL_STIFFNESS_RAW
	#define umStiffnessMultiplier	(1)
#else
	#define umStiffnessMultiplier	(umGlobalParams.z)
#endif

	// NOTE:- umStiffnessMultiplier and umGlobalArgFreqH will use same value.

	#define umGlobalScaleH			(umGlobalParams.x)	// (0.025f)  micro-movement: globalScaleHorizontal
	#define umGlobalScaleV			(umGlobalParams.y)	// (0.025f)  micro-movement: globalScaleVertical
	#define umGlobalArgFreqH		(umGlobalParams.z)	// (0.5250f) micro-movement: globalArgFreqHorizontal
	#define umGlobalArgFreqV		(umGlobalParams.w)	// (0.5250f) micro-movement: globalArgFreqVertical
#endif // TREES_USE_ALPHA_CARD_ONLY_BENDING && !__LOW_QUALITY

#endif // WIND_DISPLACEMENT

#if !TREE_LOD && !TREE_LOD2
	// wind global params: [scale]
	float4 WindGlobalParams : windGlobalParams0
	<
		string UIWidget	= "slider";
		string UIName	= "Global: WindScale CollR";
		int nostrip=1;
		float UIMin		= 0.0;
		float UIMax		= 50.0;
		float UIStep	= 0.01;
	> = float4(1.0f, 5.0f, 5.0f, 1.0f);
	#define windGlobalFreeX			(WindGlobalParams.x)	// free
	//#define GlobalPlayerCollR2	(WindGlobalParams.y)	// global player coll radius: recognized by CSETreeType setup code
	//#define GlobalVehicleCollR2	(WindGlobalParams.z)	// global vehicle coll radius: recognized by CSETreeType setup code
	#define windGlobalFreeW			(WindGlobalParams.w)	// free
#endif //!TREE_LOD && !TREE_LOD2


#if TREE_LOD2
	float4 treeLod2Params : treeLod2Params
	<
		string UIName = "BB Width/Height";
		string UIWidget = "slider";
		float UIMin = 0.0;
		float UIMax = 100.0;
		float UIStep = .1;
	> = { 1.0, 1.0, 0.0, 0.0};
	#define treeLod2Width	(treeLod2Params.x)
	#define treeLod2Height	(treeLod2Params.y)

	float3 treeLod2Normal : treeLod2Normal
	<
		string UIWidget = "slider";
		float UIMin = -1.0;
		float UIMax = 1.0;
		float UIStep = .01;
		string UIName = "TreeLOD2 World Normal";
	> = float3(0,0,1);
#endif

#define TREE_NORMAL_CONTROL (1)

#if TREE_NORMAL_CONTROL
float UseTreeNormals : UseTreeNormals
<
	string UIWidget = "slider";
	float UIMin = 0.0;
	float UIMax = 1.0;
	float UIStep = .01;
	string UIName = "UseTreeNormals";
> = {1.0};
#endif // TREE_NORMAL_CONTROL

float SelfShadowing : SelfShadowing
<
	string UIWidget = "slider";
	float UIMin = 0.0;
	float UIMax = 1.0;
	float UIStep = .01;
	string UIName = "Selfshadow factor";
> = {0.8};

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

#if NORMAL_ALPHA_SCALE
float AlphaScaleNormal : AlphaScaleNormal
<
	string UIWidget = "slider";
	float UIMin = -100.0;
	float UIMax = 100.0;
	float UIStep = .01;
	string UIName = "Normal Alpha Scale";
> = {1.0};

float AlphaClampNormal : AlphaClampNormal
<
	string UIWidget = "slider";
	float UIMin = 0.0;
	float UIMax = 10.0;
	float UIStep = .01;
	string UIName = "Normal Alpha Clamp";
> = {1.0};
#endif // NORMAL_ALPHA_SCALE

#if (__SHADERMODEL >= 40)
// DX11 TODO:- These are temporary here for the RAG sliders.	 We`ll need to put in proper depth based calculations.
#define gTessellationFactor gTessellationGlobal1.y
#define gPN_KFactor gTessellationGlobal1.z
#endif

#if CAMERA_FACING || CAMERA_ALIGNED
float ShadowFacingOffset : ShadowFacingOffset
<
	string UIWidget = "slider";
	float UIMin = -1000.0;
	float UIMax = 1000.0;
	float UIStep = 0.01;
	string UIName = "Shadow Facing Offset";
> = {0.0};
#endif // CAMERA_FACING || CAMERA_ALIGNED

EndConstantBufferDX10( trees_common_locals )

// Avoiding to use the global switch on consoles
#if 0 // RSG_ORBIS
#define ALLOW_TREE_DISCARD	(false)
#elif (__SHADERMODEL >= 40)
#define ALLOW_TREE_DISCARD	(gTreesUseDiscard)
#else
#define ALLOW_TREE_DISCARD	(true)
#endif

// Given the transparency value, produce a viable alpha
// to be consumed by the Alpha-To-Coverage transformation.
// The original alpha is not good because it produces too transparent results
// after MSAA resolve stage, hence we try to make it more opaque.

float convertAlphaToCoverage(float alpha)
{
#if (__SHADERMODEL >= 40)
	// The alpha for trees is not written, nor is it blended,
	// so we can convert it to coverage even if it's not used
	//return sin(alpha*(PI/2));
	//return pow(alpha,0.2);
	return alpha * 2.0;
#else
	return alpha;
#endif
}

float4 convertAlphaToCoverage(float4 alpha)
{
#if (__SHADERMODEL >= 40)
	// The alpha for trees is not written, nor is it blended,
	// so we can convert it to coverage even if it's not used
	//return sin(alpha*(PI/2));
	//return pow(alpha,0.2);
	return alpha * 2.0;
#else
	return alpha;
#endif
}


#define USE_SOFT_TREE_SSA	1

#if RANDOMIZED_DITHER

#define HARDCODE_ALPHA_RANGE 0

#if HARDCODE_ALPHA_RANGE
// foliage_alphaRef = 90/255.f
	float2 treeAlphaRange : treeAlphaRange
	<
	string UIWidget = "slider";
	float UIMin = 0.;
	float UIMax = 1.0;
	float UIStep = .01;
	string UIName = "Alpha Fade Range";
	> = float2(0.3f, 0.5f);   //90/255.f-0.15,90/255.f+.15);			  
#else
#define treeAlphaRange	float2(0.3,0.5)
#endif  // !HARDCODE_ALPHA_RANGE


#else //RANDOMIZED_DITHER
// used by SSA

#if USE_SOFT_TREE_SSA 
#define treeAlphaRange	float2(0.1,.75)
#else
#define treeAlphaRange	float2(0.3,0.5)
#endif

#endif

#define SHADOW_CASTING            (1)
#define SHADOW_CASTING_TECHNIQUES (1 && SHADOW_CASTING_TECHNIQUES_OK)
#define SHADOW_RECEIVING          (1)
#define SHADOW_RECEIVING_VS       (0)

#if !DISABLE_DISCARDS
#define SHADOW_USE_TEXTURE
#endif

#include "../../Lighting/Shadows/localshadowglobals.fxh"
#include "../../Lighting/Shadows/cascadeshadows.fxh"
#include "../../Lighting/forward_lighting.fxh"
#include "../../Lighting/lighting.fxh"
#include "../../Debug/EntitySelect.fxh"

#define _USE_PER_PIXEPL_TRANSLUCENCY 0


//
//
//
//
struct treeVertexInput
{
	float3 Position		:	POSITION;
	float2 TexCoord0	:	TEXCOORD0;
};

struct treeVertexInputD
{
	float3 Position		:	POSITION;
#if !IS_SHADOW_PROXY
	float3 Normal		:	NORMAL;
#endif
	float4 Diffuse		:	COLOR0;
#if !IS_SHADOW_PROXY
	float4 Specular		:	COLOR1;
#endif
	float2 TexCoord0	:	TEXCOORD0;
#if NORMAL_MAP && !IS_SHADOW_PROXY
	float4 Tangent		:	TANGENT0;
#endif
#if (CAMERA_FACING || CAMERA_ALIGNED) && !IS_SHADOW_PROXY
	float4 RelativeCentre	: TANGENT1;
#endif
};

DeclareInstancedStuct(treeInstancedVertexInputD, treeVertexInputD);

#if SKINNED_TREES
struct treeVertexSkinInputD
{
	float3 Position		:	POSITION;
	float4 weight		:	BLENDWEIGHT;
	index4 blendindices	:	BLENDINDICES;
	float3 Normal		:	NORMAL;
	float4 Diffuse		:	COLOR0;
	float4 Specular		:	COLOR1;
	float2 TexCoord0	:	TEXCOORD0;
#if NORMAL_MAP
	float4 Tangent		:	TANGENT0;
#endif
};
#endif //SKINNED_TREES...

#if USE_PN_TRIANGLES
struct treeVertexInputD_CtrlPoint
{
	float3 Position		:	CTRL_POSITION;
	float3 Normal		:	CTRL_NORMAL;
	float4 Diffuse		:	CTRL_COLOR0;
	float4 Specular		:	CTRL_COLOR1;
	float2 TexCoord0	:	CTRL_TEXCOORD0;
#if NORMAL_MAP
	float4 Tangent		:	CTRL_TANGENT0;
#endif
};

treeVertexInputD_CtrlPoint CtrlPointFromIN(treeVertexInputD IN)
{
	treeVertexInputD_CtrlPoint Ret;
	
	Ret.Position = IN.Position;
	Ret.Normal = IN.Normal;
	Ret.Diffuse = IN.Diffuse;
	Ret.Specular = IN.Specular;
	Ret.TexCoord0 = IN.TexCoord0;
#if NORMAL_MAP
	Ret.Tangent = IN.Tangent;
#endif
	
	return Ret;
}
#endif // USE_PN_TRIANGLES

struct treeVertexInputLod2D
{
	float3 Position		:	POSITION;
	float4 Diffuse		:	COLOR0;
	float3 Normal		:	NORMAL;
	float2 TexCoord0	:	TEXCOORD0;
	float2 TexCoord1	:	TEXCOORD1;
	float2 TexCoord2	:	TEXCOORD2;
	float2 TexCoord3	:	TEXCOORD3;
};

DeclareInstancedStuct(treeInstancedVertexInput2D, treeVertexInputLod2D);

struct treeVertexInputImp
{
	float3 Position		:	POSITION;
	float2 TexCoord0	:	TEXCOORD0;
};

struct treeVertexInputImpD
{
	float3 Position		:	POSITION;
	float3 Normal		:	NORMAL;
	float4 Diffuse		:	COLOR0;
	float2 TexCoord0	:	TEXCOORD0;
};

struct	treeVertexOutput
{
	DECLARE_POSITION(Position)
	float2	TexCoord0	:	TEXCOORD0;
};

struct treeVertexInputWaterReflection
{
	float3 Position		:	POSITION;
	float4 Diffuse		:	COLOR0;
	float3 Normal		:	NORMAL;
	float2 TexCoord		:	TEXCOORD0;
};

struct	treeVertexOutputWaterReflection
{
	float4	TexCoord	:	TEXCOORD0;	// z=depthness, w=shadowing
	float3	WorldPos	:	TEXCOORD1;
	float3	WorldNormal	:	TEXCOORD2;
	float4	FogData		:	TEXCOORD3;	// per-vertex fog data
	DECLARE_POSITION_CLIPPLANES(Position)
};

struct	treeVertexOutputD
{
	DECLARE_POSITION(Position)
#if PALETTE_TINT
	float4	Diffuse		:	COLOR0;
#endif
	float4	TexCoord0	:	TEXCOORD0;
	float3	Normal		:	TEXCOORD1;
//	float2	OccAlpha	:	TEXCOORD2;
#if NORMAL_MAP
	float3 worldTangent	:	TEXCOORD2;
	float3 worldBinormal:	TEXCOORD3;
#endif

#if INSTANCED
	float4 instParams	:	TEXCOORD4;
#endif

#if RANDOMIZED_DITHER
	float offset : TEXCOORD6;
#endif

#if ((WIND_DISPLACEMENT || TREES_USE_ALPHA_CARD_ONLY_BENDING) && !__LOW_QUALITY) && !SHADER_FINAL
	float4	windDebug	 :	COLOR2;
#endif
};

struct	treeVertexOutputLodD
{
	DECLARE_POSITION(Position)
#if PALETTE_TINT
	float4	Diffuse		:	COLOR0;
#endif
	float4	TexCoord0	:	TEXCOORD0;	// .zw = natural+artificial AO
	float4	worldNormal	:	TEXCOORD1;	// .w  = SelfShadowing
#if NORMAL_MAP
	float3 worldTangent	:	TEXCOORD2;
	float3 worldBinormal:	TEXCOORD3;
#endif

#if INSTANCED
	float4 instParams	:	TEXCOORD4;
#endif

#if RANDOMIZED_DITHER
	float offset : TEXCOORD6;
#endif
};

struct	treeVertexOutputLod2D
{
	DECLARE_POSITION(Position)
	float4	TexCoord0	:	TEXCOORD0;
	float3	worldNormal	:	TEXCOORD1;
#if INSTANCED
	float4 instParams	:	TEXCOORD2;
#endif
};

struct	treeVertexOutputImp
{
	DECLARE_POSITION(Position)
	float2	TexCoord0	:	TEXCOORD0;
};

struct	treeVertexOutputImpD
{
	DECLARE_POSITION(Position)
	float3	TexCoord0	:	TEXCOORD0;
	float3	Normal		:	TEXCOORD1;
//	float2	OccAlpha	:	TEXCOORD2;
};


#if SPEC_MAP
	BeginSampler(sampler2D,specTexture,SpecSampler,SpecularTex)
		string UIName="Specular Texture";
		#if SPECULAR_UV1
			string UIHint="specularmap,uv1";
		#else
			string UIHint="specularmap";
		#endif
		string TCPTemplate="Specular";
	string	TextureType="Specular";
	ContinueSampler(sampler2D,specTexture,SpecSampler,SpecularTex)
		AddressU  = WRAP;        
		AddressV  = WRAP;
		AddressW  = WRAP;
		MIPFILTER = MIPLINEAR;
		MINFILTER = HQLINEAR;
		MAGFILTER = MAGLINEAR;
		#ifdef MIN_MIP_LEVEL
			MINMIPLEVEL = MIN_MIP_LEVEL;
		#endif
		#ifdef MAX_MIP_LEVEL
			MAXMIPLEVEL = MAX_MIP_LEVEL;
		#endif
	EndSampler;
#endif	// SPEC_MAP


#if NORMAL_MAP
	BeginSampler(sampler2D,bumpTexture,BumpSampler,BumpTex)
		string UIName="Bump Texture";
		string UIHint="normalmap";
	string TCPTemplate="NormalMap";
	string	TextureType="Bump";
	ContinueSampler(sampler2D,bumpTexture,BumpSampler,BumpTex)
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
		#ifdef MIN_MIP_LEVEL
			MINMIPLEVEL = MIN_MIP_LEVEL;
		#endif
		#ifdef MAX_MIP_LEVEL
			MAXMIPLEVEL = MAX_MIP_LEVEL;
		#endif
	EndSampler;
#endif	// NORMAL_MAP

//
//
// Instancing
struct TreeVertInstanceData
{
	float4x4 worldMtx;
	float alpha;
	float naturalAmbientScale;
	float artificialAmbientScale;
	float invEntityScaleXY;
	float invEntityScaleZ;
};

struct TreePixelInstanceData
{
	float alpha;
	float naturalAmbientScale;
	float artificialAmbientScale;
};

void FetchTreeInstanceVertexData(treeInstancedVertexInputD instIN, out treeVertexInputD vertData, out int nInstIndex)
{
#if INSTANCED && __XENON
	int nIndex = instIN.nIndex;
	int nNumIndicesPerInstance = gInstancingParams.x;				//# of verts [indexed] in a single instance
	nInstIndex = ( nIndex + 0.5 ) / nNumIndicesPerInstance;			//index of the current instanced model								[0, <Num Models Instanced>]
	int nMeshIndex = nIndex - nInstIndex * nNumIndicesPerInstance;	//offset/index of the current vert in the current instanced model.	[0, <Max Num Verts Per Instance>]

	//Do custom vfetch of vertex components
	float3 inPos;
	float3 inNrm;
	float4 inCpv;
	float2 inTex;
	#if NORMAL_MAP
		float4 inTan;
	#endif
	float4 inSpec;
	asm
	{
		vfetch inPos.xyz,	nMeshIndex, position0;
		vfetch inNrm.xyz,	nMeshIndex, normal0;
		vfetch inCpv,		nMeshIndex, color0;
		vfetch inTex.xy,	nMeshIndex, texcoord0;
	#if NORMAL_MAP
		vfetch inTan,	nMeshIndex, tangent0;
	#endif
		vfetch inSpec,	nMeshIndex, color1;
	};

	vertData.Position.xyz = inPos;
	vertData.Normal.xyz = inNrm;
	vertData.Diffuse = inCpv;
	vertData.TexCoord0 = inTex;
	#if NORMAL_MAP
		vertData.Tangent = inTan;
	#endif
	vertData.Specular = inSpec;
#else //INSTANCED && __XENON
	vertData = instIN.baseVertIn;
	#if INSTANCED && (__SHADERMODEL >= 40)
		nInstIndex = instIN.nInstIndex;
	#else
		nInstIndex = 0;
	#endif
#endif //INSTANCED && __XENON
}

void FetchTreeInstanceVertexData(treeInstancedVertexInput2D instIN, out treeVertexInputLod2D vertData, out int nInstIndex)
{
#if INSTANCED && __XENON
	int nIndex = instIN.nIndex;
	int nNumIndicesPerInstance = gInstancingParams.x;				//# of verts [indexed] in a single instance
	nInstIndex = ( nIndex + 0.5 ) / nNumIndicesPerInstance;			//index of the current instanced model								[0, <Num Models Instanced>]
	int nMeshIndex = nIndex - nInstIndex * nNumIndicesPerInstance;	//offset/index of the current vert in the current instanced model.	[0, <Max Num Verts Per Instance>]

	//Do custom vfetch of vertex components
	float3 inPos;
	float3 inNrm;
	float4 inCpv;
	float4 inTex0;
	float4 inTex1;
	asm
	{
		vfetch inPos.xyz,	nMeshIndex, position0;
		vfetch inNrm.xyz,	nMeshIndex, normal0;
		vfetch inCpv,		nMeshIndex, color0;
		vfetch inTex0,		nMeshIndex, texcoord0;
		vfetch inTex1,		nMeshIndex, texcoord1;
	};

	vertData.Position.xyz = inPos;
	vertData.Normal.xyz = inNrm;
	vertData.Diffuse = inCpv;
	vertData.TexCoord0 = inTex0;
	vertData.TexCoord1 = inTex1;
#else //INSTANCED && __XENON
	vertData = instIN.baseVertIn;
	#if INSTANCED && (__SHADERMODEL >= 40)
		nInstIndex = instIN.nInstIndex;
	#else
		nInstIndex = 0;
	#endif
#endif //INSTANCED && __XENON
}

void GetTreeInstanceData(int nInstIndex, out TreeVertInstanceData inst)
{
#if INSTANCED
	int nInstRegBase = nInstIndex * NUM_CONSTS_PER_INSTANCE;
	float3x4 worldMat;
	worldMat[0] = gInstanceVars[nInstRegBase+0];
	worldMat[1] = gInstanceVars[nInstRegBase+1];
	worldMat[2] = gInstanceVars[nInstRegBase+2];
	inst.worldMtx[0] = float4(worldMat[0].xyz, 0);
	inst.worldMtx[1] = float4(worldMat[1].xyz, 0);
	inst.worldMtx[2] = float4(worldMat[2].xyz, 0);
	inst.worldMtx[3] = float4(worldMat[0].w, worldMat[1].w, worldMat[2].w, 1);

	inst.alpha =					gInstanceVars[nInstRegBase+3].x;
	inst.naturalAmbientScale =		gInstanceVars[nInstRegBase+3].y;
	inst.artificialAmbientScale =	gInstanceVars[nInstRegBase+3].z;
	inst.invEntityScaleXY =			gInstanceVars[nInstRegBase+3].w;
	inst.invEntityScaleZ =			gInstanceVars[nInstRegBase+4].w;
#else
	inst.worldMtx =					gWorld;
	inst.alpha =					globalAlpha;
	inst.naturalAmbientScale =		gNaturalAmbientScale;
	inst.artificialAmbientScale =	gArtificialAmbientScale;
	inst.invEntityScaleXY =			invGlobalEntityScaleXY;
	inst.invEntityScaleZ =			invGlobalEntityScaleZ;
#endif
}

#if INSTANCED
	#define PackTreeInstanceData(p, inst)		\
		(p).instParams = float4((inst).alpha, (inst).naturalAmbientScale, (inst).artificialAmbientScale, 1.0f);

	#define UnPackTreeInstanceData(p, inst)		\
		(inst).alpha = (p).instParams.x; (inst).naturalAmbientScale = (p).instParams.y; (inst).artificialAmbientScale = (p).instParams.z;
#else
	#define PackTreeInstanceData(p, inst)

	#define UnPackTreeInstanceData(p, inst)		\
		(inst).alpha = globalAlpha; (inst).naturalAmbientScale = gNaturalAmbientScale; (inst).artificialAmbientScale = gArtificialAmbientScale;
#endif


//
//
//
//
float GetMipLevel(sampler2D ss, float2 uv)
{
float	Result;
#if	__XENON
	asm
	{
		getCompTexLOD2D	Result.x, uv, ss
	};
#else
	#if 0
		float2 dx = ddx(uv * textureSize.x);
		float2 dy = ddy(uv * textureSize.y);
		float d = max(sqrt(dot(dx.x, dx.x) + dot(dx.y, dx.y)), sqrt(dot(dy.x, dy.x) + dot(dy.y, dy.y)));
		float mipLevel = log2(d); 
	#else
		Result = 0;
	#endif // 0
#endif // __XENON
	return(Result);
}// end of GetMipLevel()...


//
//
// useful when using alpha to mask is causing objects to mip to alpha (see foliage)
//
float ComputeScaledAlpha(float alpha, sampler2D ss, float2 uv)
{
#if __XENON
	float	MipLevel = GetMipLevel(ss, uv);
	return(saturate(alpha * lerp(1.0, 2.25f, saturate((MipLevel - 1) / 6))));
#else
	// NOTE: GetMipLevel() always returns 0 for non Xenon compiles
	return alpha;
#endif
}


//
//
//
//
#if WIND_DISPLACEMENT

float4 GetWindMovementColour(treeVertexInputD IN)
{
#if USE_ERIKS_TEMP_UMOVEMENT_COLOURS
	return IN.Diffuse.bgbr;	// Red has stiffness, we use blue for both horizontal and vertical um movement.
#else
	return float4(IN.Diffuse.rgb, IN.Specular.r); // Stiffness channel in CPV1.
#endif
}

float3 TreesApplyBranchBendAndTriMicromovements_StiffnessInAlpha(float3 modelspacevertpos, float3 modelspacenormal, float4 IN_Color0, float4x4 worldMtx, bool bShadowOffset)
{
	MICROMOVEMENT_PROPERTES umProperties;

	umProperties.FreqAndAmp[0] = umTriWave1Params;
	umProperties.FreqAndAmp[1] = umTriWave2Params;
	umProperties.FreqAndAmp[2] = umTriWave3Params;

	BRANCH_BEND_PROPERTIES bendProperties;

	bendProperties.PivotHeight = foliageBranchBendPivot.x;
	bendProperties.TrunkStiffnessAdjustLow	= foliageBranchBendStiffnessAdjust.x;
	bendProperties.TrunkStiffnessAdjustHigh = foliageBranchBendStiffnessAdjust.y;
	bendProperties.PhaseStiffnessAdjustLow	= foliageBranchBendStiffnessAdjust.z;
	bendProperties.PhaseStiffnessAdjustHigh = foliageBranchBendStiffnessAdjust.w;

	float3 newVertpos = ComputeBranchBendPlusFoliageMicromovements(modelspacevertpos, modelspacenormal, IN_Color0, worldMtx, umGlobalTimer, bendProperties, umProperties);

#if CAMERA_FACING || CAMERA_ALIGNED
	if (bShadowOffset)
	{
		float3 vWorldTranslation = gViewInverse[2].xyz * ShadowFacingOffset;

		newVertpos += vWorldTranslation;
	}
#endif // CAMERA_ALIGNED || CAMERA_FACING
	return newVertpos.xyz;
}

#else // WIND_DISPLACEMENT

float4 GetWindMovementColour(treeVertexInputD IN)
{
	return IN.Diffuse;
}

#if TREES_USE_ALPHA_CARD_ONLY_BENDING && !__LOW_QUALITY

float3 TreesApplyBendingAlphaCardOnly(float3 modelspacevertpos, float4 IN_Color0, float4x4 worldMtx, bool bShadowOffset)
{
	float3 newVertpos = ComputeBranchBend_AlphaCardOnly(modelspacevertpos, IN_Color0, worldMtx, umGlobalTimer, umStiffnessMultiplier);

#if CAMERA_FACING || CAMERA_ALIGNED
	if (bShadowOffset)
	{
		float3 vWorldTranslation = gViewInverse[2].xyz * ShadowFacingOffset;

		newVertpos += vWorldTranslation;
	}
#endif // CAMERA_ALIGNED || CAMERA_FACING
	return newVertpos.xyz;
}


float3 TreesApplyMicromovementsOriginal(float3 vertpos, float3 IN_Color0, bool bShadowOffset)
{
	float3 newVertpos;

	// local micro-movements of plants:
	float umScaleH	= IN_Color0.r							* umGlobalScaleH * umOverrideScaleH;	// horizontal movement scale (red0:   255=max, 0=min)
	float umScaleV	= IN_Color0.b							* umGlobalScaleV * umOverrideScaleV;	// vertical movement scale	 (blue0:  255=max, 0=min)
	float umArgPhase= abs(IN_Color0.g+umGlobalPhaseShift)	* TWO_PI;								// phase shift               (green0: phase 0-1     )

	float3 uMovementArg			= float3(umGlobalTimer, umGlobalTimer, umGlobalTimer);
	float3 uMovementArgPhase	= float3(umArgPhase, umArgPhase, umArgPhase);
	float3 uMovementScaleXYZ	= float3(umScaleH, umScaleH, umScaleV);

	uMovementArg			*= float3(umGlobalArgFreqH, umGlobalArgFreqH, umGlobalArgFreqV) * float3(umOverrideArgFreqH, umOverrideArgFreqH, umOverrideArgFreqV);
	uMovementArg			+= uMovementArgPhase;
	float3 uMovementAdd		=  sin(uMovementArg);
	uMovementAdd			*= uMovementScaleXYZ;

	// add final micro-movement:
	newVertpos = vertpos.xyz + uMovementAdd;

#if CAMERA_FACING || CAMERA_ALIGNED
	if (bShadowOffset)
	{
		float3 vWorldTranslation = gViewInverse[2].xyz * ShadowFacingOffset;

		newVertpos += vWorldTranslation;
	}
#endif // CAMERA_ALIGNED || CAMERA_FACING
	return newVertpos.xyz;
}

#endif // TREES_USE_ALPHA_CARD_ONLY_BENDING && !__LOW_QUALITY

#endif // WIND_DISPLACEMENT

float3 TreesApplyMicromovementsInternal(treeVertexInputD IN, float4x4 worldMtx, bool bShadowOffset)
{
	float4 windMovementColour = GetWindMovementColour(IN);

#if WIND_DISPLACEMENT
		return TreesApplyBranchBendAndTriMicromovements_StiffnessInAlpha(IN.Position.xyz, IN.Normal.xyz, windMovementColour, worldMtx, bShadowOffset);
#else // WIND_DISPLACEMENT
		
#if (!__MAX || TREES_ENABLE_BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS) && !__LOW_QUALITY
	float underWaterBlend = ComputeWindUnderWaterBlend(IN.Position.xyz);

	float3 fromNewWind = IN.Position.xyz;
	float3 fromOldWind = IN.Position.xyz;

	if(underWaterBlend > 0.0f)
	{
		fromNewWind = TreesApplyBendingAlphaCardOnly(IN.Position.xyz, windMovementColour, worldMtx, bShadowOffset);
	}
	if(underWaterBlend < 1.0f)
	{
		fromOldWind = TreesApplyMicromovementsOriginal(IN.Position.xyz, windMovementColour, bShadowOffset);
	}
	return (1.0f-underWaterBlend)*fromOldWind + underWaterBlend*fromNewWind;
#else // !__MAX
	return IN.Position.xyz;
#endif // !__MAX

#endif // WIND_DISPLACEMENT
}

//
//
//
//
float3 VS_ApplyVehicleCollision(float3 vertpos, /*vecB*/float3 B, /*vecM*/float3 M, /*1/dot(M,M)*/float invDotMM,
												/*collision radius*/float R, /*R*R*/float RR, /*1/(R*R)*/float invRR, float GroundZ, float collScale)
{
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
//
// player's collision:
// ps3: 25 instr, 360: 22 instr
//
float3 TreesApplyCollisionInternal(float3 localPos, float3 IN_Color0, float4x4 worldTransform, float invEntityScaleXY, float invEntityScaleZ)
{
	float3 OUT = localPos;

	if(bPlayerCollEnabled)
	{
		row_major float4x4 matWorld = worldTransform;
		// orthonormalise: remove scaling from world mtx:
		matWorld[0].xyz *= invEntityScaleXY;
		matWorld[1].xyz *= invEntityScaleXY;
		matWorld[2].xyz *= invEntityScaleZ;

		// warp to global pos:
		float3 worldPos = OUT.xyz * float3(GlobalEntityScaleXY,GlobalEntityScaleXY,GlobalEntityScaleZ);	// scaling
		worldPos = mul(float4(worldPos,1), matWorld).xyz;												// rotation+translation
		
		float3 vecDist3 = worldPos.xyz - gWorldPlayerPos.xyz;
		float distc2	= dot(vecDist3, vecDist3);
		float coll		= (gCollPedSphereRadiusSqr - distc2) * (gCollPedInvSphereRadiusSqr);
		coll = saturate(coll);

		worldPos.xy += (float2)coll*normalize(vecDist3).xy * (IN_Color0.r);

		// warp back to local pos:
		worldPos.xyz -= matWorld[3].xyz;
		worldPos.xyz *= float3(invEntityScaleXY, invEntityScaleXY, invEntityScaleZ);
		OUT.x = dot(worldPos.xyz, matWorld[0].xyz);
		OUT.y = dot(worldPos.xyz, matWorld[1].xyz);
		OUT.z = dot(worldPos.xyz, matWorld[2].xyz);
	}

	// vehicle collision:
	if(bVehTreeColl0Enabled)
	{
		row_major float4x4 matWorld = worldTransform;
		// orthonormalise: remove scaling from world mtx:
		matWorld[0].xyz *= invEntityScaleXY;
		matWorld[1].xyz *= invEntityScaleXY;
		matWorld[2].xyz *= invEntityScaleZ;

		// warp to global pos:
		float3 worldPos = OUT.xyz * float3(GlobalEntityScaleXY,GlobalEntityScaleXY,GlobalEntityScaleZ);// scaling
		worldPos = mul(float4(worldPos,1), matWorld).xyz;													// rotation+translation

		worldPos.xyz = VS_ApplyVehicleCollision(worldPos, coll0VehVecB, coll0VehVecM, coll0VehInvDotMM, coll0VehR, coll0VehRR, coll0VehInvRR, coll0VehGroundZ, IN_Color0.r);
	
		if(bVehTreeColl1Enabled)
		{
			worldPos.xyz = VS_ApplyVehicleCollision(worldPos, coll1VehVecB, coll1VehVecM, coll1VehInvDotMM, coll1VehR, coll1VehRR, coll1VehInvRR, coll1VehGroundZ, IN_Color0.r);
		}

		if(bVehTreeColl2Enabled)
		{
			worldPos.xyz = VS_ApplyVehicleCollision(worldPos, coll2VehVecB, coll2VehVecM, coll2VehInvDotMM, coll2VehR, coll2VehRR, coll2VehInvRR, coll2VehGroundZ, IN_Color0.r);
		}

		if(bVehTreeColl3Enabled)
		{
			worldPos.xyz = VS_ApplyVehicleCollision(worldPos, coll3VehVecB, coll3VehVecM, coll3VehInvDotMM, coll3VehR, coll3VehRR, coll3VehInvRR, coll3VehGroundZ, IN_Color0.r);
		}

		// warp back to local pos:
		worldPos.xyz -= matWorld[3].xyz;
		worldPos.xyz *= float3(invEntityScaleXY, invEntityScaleXY, invEntityScaleZ);
		OUT.x = dot(worldPos.xyz, matWorld[0].xyz);
		OUT.y = dot(worldPos.xyz, matWorld[1].xyz);
		OUT.z = dot(worldPos.xyz, matWorld[2].xyz);
	}

	return(OUT);
}

// =============================================================================================== //

//
//
//
//
treeVertexOutputD VS_PropFoliageDeferredInternal(treeVertexInputD IN, bool bApplyMicromovements, int nInstIndex)
{
	TreeVertInstanceData inst;
	GetTreeInstanceData(nInstIndex, inst);

	treeVertexOutputD OUT;

#if CAMERA_FACING || CAMERA_ALIGNED
	ApplyCameraAlignment(IN.Position.xyz, IN.Normal.xyz, IN.RelativeCentre.xyz);
#endif

#if !IS_SHADOW_PROXY
	float3 worldNormal	= mul(IN.Normal, (float3x3)inst.worldMtx);
#else
	float3 worldNormal	= float3(0.0f, 0.0f, 0.0f);
#endif

#if NORMAL_MAP
	// Do the tangent+binormal stuff
	float4 localTangent = IN.Tangent;
	float3 worldTangent	= NORMALIZE_VS(mul(localTangent.xyz, (float3x3)inst.worldMtx));
	float3 worldBinormal= rageComputeBinormal(worldNormal, worldTangent, localTangent.w);
	OUT.worldTangent	= worldTangent;
	OUT.worldBinormal	= worldBinormal;
#else
#if	TREE_NORMAL_CONTROL
	worldNormal = lerp(float3(0,0,1), worldNormal, UseTreeNormals.x);
#endif // TREE_NORMAL_CONTROL
	worldNormal = worldNormal*.5+.5;
#endif
	OUT.Normal			= worldNormal;

	// uMovements code (PSN: 17 instructions, 360: 16 instructions):
	float4 IN_Color0	= IN.Diffuse.rgba;
	float3 vertPos		= IN.Position.xyz;
	if(bApplyMicromovements)
	{
		vertPos	= TreesApplyMicromovementsInternal(IN, inst.worldMtx, false);
		vertPos	= TreesApplyCollisionInternal(vertPos, IN_Color0.rgb, inst.worldMtx, inst.invEntityScaleXY, inst.invEntityScaleZ);
	}
	OUT.Position		= ApplyCompositeWorldTransform(float4(vertPos,1), INSTANCING_ONLY_ARG(inst.worldMtx) gWorldViewProj);

#if PALETTE_TINT
	float4 tintColor;
	#if PALETTE_TINT_EDGE
		tintColor = UnpackTintPalette(IN.Specular);	// EDGE: already unpacked tint color
	#else
		tintColor = UnpackTintPalette(IN.Specular);	// 360: COLOR1.b to be unpacked into tint
	#endif
	OUT.Diffuse = tintColor;

	#if 0 && NORMAL_ALPHA_SCALE // Scale Alpha Based on Normal angle 
								// disabled (talk to Andrzej if you want this back for any reason)
		float3x3 ViewMatrix = transpose(gViewInverse);
		float3 fNormalViewSpace = mul(worldNormal, ViewMatrix);
		float fAlphaScale = abs(fNormalViewSpace.z);
		fAlphaScale *= fAlphaScale * AlphaScaleNormal;
		fAlphaScale = max(fAlphaScale, AlphaClampNormal);
		OUT.Diffuse.a *= fAlphaScale;
	#endif // NORMAL_ALPHA_SCALE
#endif // PALETTE_TINT
	
	// Output texture coordinate
	OUT.TexCoord0.xy	= IN.TexCoord0;
	OUT.TexCoord0.z		= IN.Diffuse.a;	// amount of "darkness"/"depthness" in a tree"
	OUT.TexCoord0.w		= SelfShadowing.x;

	PackTreeInstanceData(OUT, inst);

#if RANDOMIZED_DITHER
	 OUT.offset = frac(IN.Position.x+IN.Position.z)*2.f; 
#endif

#if (WIND_DISPLACEMENT && !SHADER_FINAL)
 	BRANCH_BEND_PROPERTIES bendProperties;

	bendProperties.PivotHeight = foliageBranchBendPivot.x;
	bendProperties.TrunkStiffnessAdjustLow	= foliageBranchBendStiffnessAdjust.x;
	bendProperties.TrunkStiffnessAdjustHigh = foliageBranchBendStiffnessAdjust.y;
	bendProperties.PhaseStiffnessAdjustLow	= foliageBranchBendStiffnessAdjust.z;
	bendProperties.PhaseStiffnessAdjustHigh = foliageBranchBendStiffnessAdjust.w;

	OUT.windDebug = CalcWindDebugColour_Foliage(GetWindMovementColour(IN), IN.Normal, bendProperties);
#elif (TREES_USE_ALPHA_CARD_ONLY_BENDING && !__LOW_QUALITY && !SHADER_FINAL)
	OUT.windDebug = CalcWindDebugColour_AlphaCardOnly(GetWindMovementColour(IN), umStiffnessMultiplier);
#endif

	return(OUT);
}// end of VS_PropFoliageDeferredInternal()...


// =============================================================================================== //
// Non PN triangle version.
// =============================================================================================== //

treeVertexOutputD VS_PropFoliageDeferred(treeInstancedVertexInputD instIN)
{
	int nInstIndex;
	treeVertexInputD IN;
	FetchTreeInstanceVertexData(instIN, IN, nInstIndex);
	return VS_PropFoliageDeferredInternal(IN, true, nInstIndex);
}

#if SKINNED_TREES
treeVertexOutputD VS_PropFoliageSkinDeferred(treeVertexSkinInputD IN)
{
	rageSkinMtx skinMtx		= ComputeSkinMtx(IN.blendindices, IN.weight);

	float3 skinnedPos		= rageSkinTransform(IN.Position, skinMtx);
	float3 skinnedNormal	= rageSkinRotate(IN.Normal.xyz, skinMtx);
#if NORMAL_MAP
	float3 skinnedTangent	= rageSkinRotate(IN.Tangent.xyz, skinMtx);
#endif

treeVertexInputD treeIN = (treeVertexInputD)0;
	treeIN.Position	= skinnedPos;
#if !IS_SHADOW_PROXY
	treeIN.Normal	= skinnedNormal;
#endif
	treeIN.Diffuse	= IN.Diffuse;
#if !IS_SHADOW_PROXY
	treeIN.Specular	= IN.Specular;
#endif
	treeIN.TexCoord0= IN.TexCoord0;
#if NORMAL_MAP
	treeIN.Tangent	= float4(skinnedTangent.xyz,IN.Tangent.w);
#endif

	return VS_PropFoliageDeferredInternal(treeIN, true, 0);
}
#endif //SKINNED_TREES...

treeVertexOutputD VS_PropFoliageDeferredSimple(treeInstancedVertexInputD instIN)
{
	int nInstIndex;
	treeVertexInputD IN;
	FetchTreeInstanceVertexData(instIN, IN, nInstIndex);

	return VS_PropFoliageDeferredInternal(IN, (__SHADERMODEL >= 40) ? true : false, nInstIndex);	// disable um/collision et al.
}

// =============================================================================================== //
// PN triangle version.
// =============================================================================================== //

#if USE_PN_TRIANGLES
#define TREES_TESSELLATION_MULTIPLIER		(1.0)

#define gTessellationFactor gTessellationGlobal1.y
#define gPN_KFactor gTessellationGlobal1.z

treeVertexInputD_CtrlPoint VS_PropFoliageDeferred_PNTri(treeVertexInputD IN)
{
	treeVertexInputD_CtrlPoint Ret = CtrlPointFromIN(IN);
	return Ret;
}

// Patch Constant Function.
rage_PNTri_PatchInfo HS_PropFoliageDeferred_PNTri_PatchFunc(InputPatch<treeVertexInputD_CtrlPoint, 3> PatchPoints,  uint PatchID : SV_PrimitiveID)
{	
	rage_PNTri_PatchInfo Output = (rage_PNTri_PatchInfo)0;

	rage_PNTri_Vertex Points[3];

	Points[0].Position = PatchPoints[0].Position;
	Points[0].Normal = PatchPoints[0].Normal;
	Points[1].Position = PatchPoints[1].Position;
	Points[1].Normal = PatchPoints[1].Normal;
	Points[2].Position = PatchPoints[2].Position;
	Points[2].Normal = PatchPoints[2].Normal;

#if ENABLE_FRUSTUM_CULL || ENABLE_BACKFACE_CULL
	if (CullEnable)
	{
#if ENABLE_FRUSTUM_CULL
		float3 vPosition[3];
		vPosition[0] = mul(float4(Points[0].Position, 1.0f), gWorld).xyz;
		vPosition[1] = mul(float4(Points[1].Position, 1.0f), gWorld).xyz;
		vPosition[2] = mul(float4(Points[2].Position, 1.0f), gWorld).xyz;

		if (!rage_IsTriangleInFrustum(vPosition[0], vPosition[1], vPosition[2], ViewFrustumEpsilon))
			return Output;
#endif // ENABLE_FRUSTUM_CULL

#if ENABLE_BACKFACE_CULL
		float3 vNormal[3];
		vNormal[0] = mul(Points[0].Normal, (float3x3)gWorld);
		vNormal[1] = mul(Points[1].Normal, (float3x3)gWorld);
		vNormal[2] = mul(Points[2].Normal, (float3x3)gWorld);

		float3 vViewForward = gViewInverse[2].xyz;

		if (rage_BackFaceCulling(vNormal[0], vNormal[1], vNormal[2], vViewForward, BackFaceCullEpsilon))
			return Output;
#endif // ENABLE_BACKFACE_CULL
	}
#endif // ENABLE_FRUSTUM_CULL || ENABLE_BACKFACE_CULL

	rage_ComputePNTrianglePatchInfo(Points[0], Points[1], Points[2], Output, TREES_TESSELLATION_MULTIPLIER);

	return Output;
}

// Hull shader.
[domain("tri")]
[partitioning("fractional_odd")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(3)]
[patchconstantfunc("HS_PropFoliageDeferred_PNTri_PatchFunc")]
[maxtessfactor(15)]
treeVertexInputD_CtrlPoint HS_PropFoliageDeferred_PNTri(InputPatch<treeVertexInputD_CtrlPoint, 3> PatchPoints, uint i : SV_OutputControlPointID, uint PatchID : SV_PrimitiveID )
{
	treeVertexInputD_CtrlPoint Output;

	// Our control points match the vertex shader output.
	Output = PatchPoints[i];

	return Output;
}

treeVertexInputD PropFoliageDeferred_PNTri_EvaluateAt(rage_PNTri_PatchInfo PatchInfo, float3 WUV, const OutputPatch<treeVertexInputD_CtrlPoint, 3> PatchPoints)
{
	treeVertexInputD Arg;

	float3 P = rage_EvaluatePatchAt(PatchInfo, WUV);

	Arg.Position = P;

	Arg.Normal = RAGE_COMPUTE_BARYCENTRIC(WUV, PatchPoints, Normal);
	Arg.Diffuse = RAGE_COMPUTE_BARYCENTRIC(WUV, PatchPoints, Diffuse);
	Arg.Specular = RAGE_COMPUTE_BARYCENTRIC(WUV, PatchPoints, Specular);
	Arg.TexCoord0 = RAGE_COMPUTE_BARYCENTRIC(WUV, PatchPoints, TexCoord0);
#if NORMAL_MAP
	Arg.Tangent = RAGE_COMPUTE_BARYCENTRIC(WUV, PatchPoints, Tangent);
#endif

	return Arg;
}

[domain("tri")]
treeVertexOutputD DS_PropFoliageDeferred_PNTri(rage_PNTri_PatchInfo PatchInfo, float3 WUV : SV_DomainLocation, const OutputPatch<treeVertexInputD_CtrlPoint, 3> PatchPoints )
{
	// Evaluate the patch at coords WUV.
	treeVertexInputD Arg = PropFoliageDeferred_PNTri_EvaluateAt(PatchInfo, WUV, PatchPoints);
	// Push the vertex through the original vertex shader.
	return VS_PropFoliageDeferredInternal(Arg, true, 0);
}

#endif // USE_PN_TRIANGLES

// =============================================================================================== //

//
//
//
//
treeVertexOutputLodD VS_PropFoliageDeferredLod(treeInstancedVertexInputD instIN)
{
	int nInstIndex;
	treeVertexInputD IN;
	FetchTreeInstanceVertexData(instIN, IN, nInstIndex);

	TreeVertInstanceData inst;
	GetTreeInstanceData(nInstIndex, inst);

	treeVertexOutputLodD OUT;

	OUT.Position		= ApplyCompositeWorldTransform(float4(IN.Position.xyz,1), INSTANCING_ONLY_ARG(inst.worldMtx) gWorldViewProj);
	
#if !IS_SHADOW_PROXY
	float3 worldNormal	= mul(IN.Normal, (float3x3)inst.worldMtx);
#else
	float3 worldNormal	= float3(0.0f, 0.0f, 0.0f);
#endif

#if NORMAL_MAP
	// Do the tangent+binormal stuff
	float4 localTangent = IN.Tangent;
	float3 worldTangent	= NORMALIZE_VS(mul(localTangent.xyz, (float3x3)inst.worldMtx));
	float3 worldBinormal= rageComputeBinormal(worldNormal, worldTangent, localTangent.w);
	OUT.worldTangent	= worldTangent;
	OUT.worldBinormal	= worldBinormal;
#else
#if TREE_NORMAL_CONTROL
	worldNormal			= lerp(float3(0,0,1), worldNormal, UseTreeNormals.x);
#endif // TREE_NORMAL_CONTROL
	worldNormal			= worldNormal*0.5+0.5;
#endif
	OUT.worldNormal.xyz	= worldNormal.xyz;
	OUT.worldNormal.w	= SelfShadowing.x;

#if PALETTE_TINT
	float4 tintColor;
	#if PALETTE_TINT_EDGE
		tintColor = UnpackTintPalette(IN.Specular);	// EDGE: already unpacked tint color
	#else
		tintColor = UnpackTintPalette(IN.Specular);	// 360: COLOR1.b to be unpacked into tint
	#endif
	OUT.Diffuse = tintColor;
#endif

	// Output texture coordinate
	OUT.TexCoord0.xy	= IN.TexCoord0;
	OUT.TexCoord0.zw	= IN.Diffuse.rg;		// natural+artificial AO

	PackTreeInstanceData(OUT, inst);

#if RANDOMIZED_DITHER
	OUT.offset = frac(IN.Position.x+IN.Position.z)*2.f; 
#endif
	return(OUT);
}

//
//
//
//
#if TREE_LOD2
treeVertexOutputLod2D vsPropFoliageDeferredLod2(treeInstancedVertexInput2D instIN, bool mirrorFlip, out float3 worldPos)
{
	int nInstIndex;
	treeVertexInputLod2D IN;
	FetchTreeInstanceVertexData(instIN, IN, nInstIndex);

	TreeVertInstanceData inst;
	GetTreeInstanceData(nInstIndex, inst);

	treeVertexOutputLod2D OUT;
	float2 uvs			= IN.TexCoord0.xy - float2(0.5f,0.5f);
	float2 modelScale	= IN.TexCoord2.xy;

	uvs *= treeLod2Params.xy * modelScale.xy;

	// camera aligned or axis aligned:
	float3 across	= lerp( gViewInverse[0].xyz,	normalize(cross(gViewInverse[2].xyz, float3(0,0,-1))),		IN.TexCoord3.x);
	float3 up		= lerp(-gViewInverse[1].xyz,	float3(0,0,-1),												IN.TexCoord3.x);

	if(mirrorFlip)
	{
		float fUp = lerp(-1.0f, 1.0f, IN.TexCoord3.x);	// flip only camera aligned 
		across	*= fUp;
		up		*= fUp;
	}

	float3 offset	= uvs.x*across + uvs.y*up;

	float3 IN_pos;

#if TREE_LOD2_DRAWABLE
	float4x4 matWorld = inst.worldMtx;
	matWorld[0].x *= inst.invEntityScaleXY;	// remove (potential) scaling from matrix
	matWorld[1].y *= inst.invEntityScaleXY;
	matWorld[2].z *= inst.invEntityScaleZ;

	float3 Position	= mul(IN.Position.xyz, (float3x3)matWorld);	// drawable's local rotation
	offset.xyz += Position.xyz;									// offset from middle of "mesh of 4-in-1's";

	IN_pos.x = dot(offset, inst.worldMtx[0].xyz);	// inverse local rotation - it will be re-applied by wvp matrix
	IN_pos.y = dot(offset, inst.worldMtx[1].xyz);
	IN_pos.z = dot(offset, inst.worldMtx[2].xyz);
	IN_pos.xy *= inst.invEntityScaleXY;
	IN_pos.z  *= inst.invEntityScaleZ;
#else
	offset.xyz += IN.Position.xyz;								// offset from middle of "mesh of 4-in-1's";

	IN_pos.x = dot(offset, inst.worldMtx[0].xyz);						// inverse local rotation - it will be re-applied by wvp matrix
	IN_pos.y = dot(offset, inst.worldMtx[1].xyz);
	IN_pos.z = dot(offset, inst.worldMtx[2].xyz);
#endif
	OUT.Position		= ApplyCompositeWorldTransform(float4(IN_pos.xyz,1), INSTANCING_ONLY_ARG(inst.worldMtx) gWorldViewProj);
#if TREE_NORMAL_CONTROL
	OUT.worldNormal		= lerp(treeLod2Normal, normalize(IN.Normal), UseTreeNormals.x);	// 'x-wrapped' -1..1 normal
#else
	OUT.worldNormal		=  normalize(IN.Normal);
#endif // TREE_NORMAL_CONTROL

	worldPos = mul(float4(IN_pos.xyz,1), inst.worldMtx).xyz;
//	worldPos = mul(float4(IN_pos.xyz,1), gWorld).xyz;

	OUT.TexCoord0.xy	= IN.TexCoord1.xy;
	OUT.TexCoord0.zw	= IN.Diffuse.rg;		// natural + artificial AO

	PackTreeInstanceData(OUT, inst);

	return(OUT);
}

treeVertexOutputLod2D vsPropFoliageDeferredLod2(treeInstancedVertexInput2D instIN, bool mirrorFlip)
{
	float3 worldPos;
	return vsPropFoliageDeferredLod2(instIN, mirrorFlip, worldPos);
}


treeVertexOutputLod2D VS_PropFoliageDeferredLod2(treeInstancedVertexInput2D IN)
{
	return vsPropFoliageDeferredLod2(IN, false);
}
#endif //TREE_LOD2...

//
//
//
//
treeVertexOutputImp	VS_PropFoliageImposter(treeVertexInputImp IN)
{
treeVertexOutputImp	OUT;

	OUT.Position =  mul(float4(IN.Position,1), gWorldViewProj);
	OUT.TexCoord0.xy = IN.TexCoord0;

	return(OUT);
}// end of VS_PropFoliageImposter()...


//
//
//
//
treeVertexOutputImpD	VS_PropFoliageDeferredImposter(treeVertexInputImpD IN)
{
treeVertexOutputImpD	OUT;

	// Output position value
	OUT.Position =  mul(float4(IN.Position,1), gWorldViewProj);

	// Output Normal in worldspace
	float3 worldNormal	= mul(IN.Normal, (float3x3)gWorld);
	OUT.Normal			= worldNormal;
	
	// Output texture coordinate
	OUT.TexCoord0.xy = IN.TexCoord0;
	OUT.TexCoord0.z	 = 0.0f;

	return(OUT);
}// end of VS_PropFoliageDeferredImposter()...

//
//
//
//
treeVertexOutput	VS_PropFoliage(treeVertexInput IN)
{
	treeVertexOutput	OUT;

	OUT.Position =  mul(float4(IN.Position,1), gWorldViewProj);
	OUT.TexCoord0.xy = IN.TexCoord0;

	return(OUT);
}// end of VS_PropFoliage()...

#if SSTAA

uint GetDiffuseAlphaCoverage(treeVertexOutputD IN)
{
	uint Coverage = 0xffffffff;

#if (RSG_ORBIS || RSG_DURANGO || (RSG_PC && 0))	// Consider always using 4 samples for PC for loop unroll niceness.
	if (ENABLE_TRANSPARENCYAA)
	{
		// Optimised, unrolled 4x supersampling
		float2 texCoord_ddx = ddx(TexCoord);
		float2 texCoord_ddy = ddy(TexCoord);

		Coverage = 0;

		float2 texelOffset;
		float4 subSample;

		texelOffset = (aaOffsets[0].x * texCoord_ddx) + (aaOffsets[0].y * texCoord_ddy);
		subSample.x = tex2D(DiffuseSampler, TexCoord + texelOffset).a;

		texelOffset = (aaOffsets[1].x * texCoord_ddx) + (aaOffsets[1].y * texCoord_ddy);
		subSample.y = tex2D(DiffuseSampler, TexCoord + texelOffset).a;

		texelOffset = (aaOffsets[2].x * texCoord_ddx) + (aaOffsets[2].y * texCoord_ddy);
		subSample.z = tex2D(DiffuseSampler, TexCoord + texelOffset).a;

		texelOffset = (aaOffsets[3].x * texCoord_ddx) + (aaOffsets[3].y * texCoord_ddy);
		subSample.w = tex2D(DiffuseSampler, TexCoord + texelOffset).a;

#if PALETTE_TINT && NORMAL_ALPHA_SCALE
		subSample *= DiffuseModifier;	// apply tinting
#endif // PALETTE_TINT && IMMEDIATECONSTANT

#if NORMAL_MAP && _USE_PER_PIXEPL_TRANSLUCENCY
		float translucency=saturate(subSample-.5)*2.f;
		subSample=saturate(subSample*2.);
#endif 

		subSample = convertAlphaToCoverage(subSample * AlphaScale * alpha);

		float4 stepVal = step( float4(AlphaTest, AlphaTest, AlphaTest, AlphaTest), subSample);

#if !DISABLE_DISCARDS
		rageDiscard(dot(stepVal, stepVal) == 0);
#endif // !DISABLE_DISCARDS

		stepVal *= float4(1, 2, 4, 8);
		Coverage = (int)stepVal.x | (int)stepVal.y | (int)stepVal.z | (int)stepVal.w;

		DiffuseColour.a = 1;
	}
#else // RSG_ORBIS || RSG_DURANGO
	if (ENABLE_TRANSPARENCYAA)
	{
		Coverage = 0;

		TreePixelInstanceData inst;
		UnPackTreeInstanceData(IN, inst);

		float	alphaDiffuse = 1;
#if PALETTE_TINT
		alphaDiffuse		= IN.Diffuse.a;
#endif // PALETTE_TINT

		float	alpha = inst.alpha * AlphaScale;

		for (uint sampleIndex=0; sampleIndex<gMSAANumSamples; ++sampleIndex)
		{
			float2 texCoordAtSample = EvaluateAttributeAtSample(IN.TexCoord0, sampleIndex);
			float subSample = tex2D(DiffuseSampler, texCoordAtSample).a;

		#if PALETTE_TINT && NORMAL_ALPHA_SCALE
			subSample *= alphaDiffuse;	// apply tinting
		#endif // PALETTE_TINT && IMMEDIATECONSTANT

		#if NORMAL_MAP && _USE_PER_PIXEPL_TRANSLUCENCY
			float translucency=saturate(subSample-.5)*2.f;
			subSample=saturate(subSample*2.);
		#endif 

			subSample = convertAlphaToCoverage(subSample * alpha);
			if (subSample > AlphaTest)
			{
				Coverage |= (1 << sampleIndex);
			}
		}
		#if !DISABLE_DISCARDS
			rageDiscard(Coverage == 0);
		#endif // !DISABLE_DISCARDS
	}
#endif

	return Coverage;
}
#else
#define GetDiffuseAlphaCoverage(x)		0xffffffff
#endif // SSTAA

//
//
//
//
float4 GetDiffuseColour(float4 DiffuseColour, float alpha, float2 TexCoord, float DiffuseModifier )
{
#if PALETTE_TINT && NORMAL_ALPHA_SCALE
	DiffuseColour.a *= DiffuseModifier;	// apply tinting
#endif // PALETTE_TINT && IMMEDIATECONSTANT

#if NORMAL_MAP && _USE_PER_PIXEPL_TRANSLUCENCY
	float translucency=saturate(DiffuseColour.a-.5)*2.f;
	DiffuseColour.a=saturate(DiffuseColour.a*2.);	
#endif 

	DiffuseColour.a	= convertAlphaToCoverage( AlphaScale * alpha * DiffuseColour.a );
#if !DISABLE_DISCARDS
	if (!ENABLE_TRANSPARENCYAA)
	{
	#if __LOW_QUALITY
		rageDiscard(DiffuseColour.a <= AlphaTest + 0.07f);
	#else // __LOW_QUALITY
		rageDiscard(DiffuseColour.a <= AlphaTest);
	#endif // __LOW_QUALITY
	}
#endif // !DISABLE_DISCARDS

	return DiffuseColour;
}// end of GetDiffuseColour()

//
//
// Common point from which to sample the texture. If we ever switch to using
// point sampling for SSTAA we'll have to use the Texture2D.Sample() syntax,
// so do the prep work here to make it easier
//
float4 SampleDiffuse(float2 TexCoords)
{
// #if SSTAA
// 	return diffuseTexture.Sample(PointSampler, TexCoords);
// #else
	return tex2D(DiffuseSampler, TexCoords);
//#endif // SSTAA
}


DeferredGBuffer GetFoliageDeferred( float4 DiffuseCol, treeVertexOutputD IN, float fFacing  )
{
	DeferredGBuffer OUT;
	TreePixelInstanceData inst;
	UnPackTreeInstanceData(IN, inst);
	// Calculate Diffuse lighting

#if PALETTE_TINT
	#if NORMAL_ALPHA_SCALE
		DiffuseCol *= IN.Diffuse;		// apply tinting
	#else
		DiffuseCol.rgb *= IN.Diffuse.rgb;	// apply tinting
	#endif
#endif // PALETTE_TINT

#if NORMAL_MAP   && _USE_PER_PIXEPL_TRANSLUCENCY
	float translucency=saturate(DiffuseCol.a-.5)*2.f;
	DiffuseCol.a=saturate(DiffuseCol.a*2.);	
#else
	const float translucency=0.f;
#endif

// On PC the faces are the wrong way round so need to flip fFacing
#if __WIN32PC
	fFacing *= -1;
#endif

#if NORMAL_MAP
	IN.Normal *= globalDoubleSidedRendering ? sign(fFacing) : 1.0;
	float3 worldNormal = CalculateWorldNormal(tex2D_NormalMap(BumpSampler, IN.TexCoord0.xy).xy, bumpiness, IN.worldTangent, IN.worldBinormal, IN.Normal);
#if TREE_NORMAL_CONTROL
	worldNormal = lerp(float3(0,0,1), worldNormal, UseTreeNormals.x);
#endif // TREE_NORMAL_CONTROL
#else
	float3 worldNormal = /*normalize*/(IN.Normal.xyz);
#endif // NORMAL_MAP

#if SPECULAR
	float surface_specularExponent;
	float surface_specularIntensity;

	#if FRESNEL
		float fresnel = specularFresnel;
	#else
		float fresnel = 1.0f;
	#endif
	#if SPEC_MAP
		float2 IN_specularTexCoord = IN.TexCoord0.xy;
		float4 specSamp = tex2D(SpecSampler, IN_specularTexCoord.xy);
		surface_specularExponent = specSamp.w*specularFalloffMult;
		surface_specularIntensity=dot(specSamp.xyz, specMapIntMask)*specularIntensityMult;
	#else
		surface_specularExponent = specularFalloffMult;
		surface_specularIntensity = specularIntensityMult;
	#endif	// SPEC_MAP
	float spI = (surface_specularIntensity*4.0f);		// see PackGBuffer() for reference
	float spE = sqrt(surface_specularExponent/512.0f);
#else
	float spI = 0;
	float spE = 0;
	float fresnel = 0.0f;
#endif	// SPECULAR

	const float TreeDepthness	= IN.TexCoord0.z;
	const float TreeShadow		= IN.TexCoord0.w;

	float AlphaDiffuse = 1;
#if PALETTE_TINT
	AlphaDiffuse		= IN.Diffuse.a;
#endif // PALETTE_TINT

	OUT.col0			= GetDiffuseColour(DiffuseCol, inst.alpha, IN.TexCoord0.xy, AlphaDiffuse);

#if NORMAL_MAP
	OUT.col1.xyz		= float3(worldNormal * 0.5f + 0.5f); // [0..1]
#else
	OUT.col1.xyz		= float3(worldNormal); // [0..1]
#endif
	OUT.col1.a			= 0.0f;	// tree material bit for foliage lighting pass

#if NORMAL_MAP
	OUT.col2			= float4(spI, spE, fresnel, TreeShadow);
#else
	OUT.col2			= float4(spI, spE, fresnel, TreeShadow);
#endif
	OUT.col3			= float4(TreeDepthness*inst.naturalAmbientScale, TreeDepthness*inst.artificialAmbientScale*ArtificialBakeAdjustMult(), 0.0f, Pack2ZeroOneValuesToU8(0.0f, ShadowFalloff));
	OUT.col3.xy			= sqrt(OUT.col3.xy * 0.5);

#if ((WIND_DISPLACEMENT || TREES_USE_ALPHA_CARD_ONLY_BENDING) && !__LOW_QUALITY) && !SHADER_FINAL
 	OUT.col0.rgb		= OUT.col0.rgb*(1.0f - IN.windDebug.a) + IN.windDebug.rgb*IN.windDebug.a;
#endif

	return OUT;
}

DeferredGBuffer GetPropFoliageDeferred(treeVertexOutputD IN, float fFacing)
{
	float4	DiffuseCol = SampleDiffuse(IN.TexCoord0.xy);
#if !DISABLE_DISCARDS
	// rageDiscard(ALLOW_TREE_DISCARD && DiffuseCol.a <= AlphaTest); // foliage_alphaRef);
#endif // !DISABLE_DISCARDS

	return GetFoliageDeferred( DiffuseCol, IN, fFacing);
}


DeferredGBufferC PS_PropFoliageDeferredC(treeVertexOutputD IN, DECLARE_FACING_INPUT(fFacing))
{
	DECLARE_FACING_FLOAT(fFacing)
	uint Coverage = GetDiffuseAlphaCoverage(IN);
	DeferredGBuffer returnVal = GetPropFoliageDeferred(IN, fFacing);
#if SSTAA
	returnVal.coverage = Coverage;
#endif // SSTAA
	return PackDeferredGBufferC(returnVal);
}// end of PS_PropFoliageDeferredC()...

DeferredGBufferNC PS_PropFoliageDeferredNC(treeVertexOutputD IN, DECLARE_FACING_INPUT(fFacing))
{
	DECLARE_FACING_FLOAT(fFacing)
	DeferredGBuffer result = GetPropFoliageDeferred(IN, fFacing);
	return PackDeferredGBufferNC(result);
}// end of PS_PropFoliageDeferredNC()...

#if RANDOMIZED_DITHER
void RandomizedDither( float4 DiffuseCol, float2 ScreenPosition, float offset )
{
#if !DISABLE_DISCARDS
	rageDiscard( (DiffuseCol.a< treeAlphaRange.x) || ((DiffuseCol.a <treeAlphaRange.y)  && SSAIsOpaquePixelFrac(ScreenPosition+float2(offset,0.))));	
#endif // !DISABLE_DISCARDS
}
#endif
DeferredGBuffer	PS_PropFoliageDeferred_AlphaClip(treeVertexOutputD IN, DECLARE_FACING_INPUT(fFacing)) 
{
	DECLARE_FACING_FLOAT(fFacing)
#if ALPHA_CLIP && !DISABLE_DISCARDS
	FadeDiscardOptional( IN.Position, globalFade, ALLOW_TREE_DISCARD );
#endif // ALPHA_CLIP

#if RANDOMIZED_DITHER
	float4	DiffuseCol = SampleDiffuse(IN.TexCoord0.xy);
	RandomizedDither( DiffuseCol, IN.Position.xy, IN.offset.x);
	return GetFoliageDeferred(DiffuseCol,IN, fFacing);								   	
#else
	return GetPropFoliageDeferred(IN,fFacing);
#endif
}// end of PS_PropFoliageDeferred_AlphaClip()...

DeferredGBufferC PS_PropFoliageDeferred_AlphaClipC(treeVertexOutputD IN, DECLARE_FACING_INPUT(fFacing)) 
{
	DECLARE_FACING_FLOAT(fFacing)
	uint Coverage = GetDiffuseAlphaCoverage(IN);
	DeferredGBuffer result = PS_PropFoliageDeferred_AlphaClip(IN, fFacing);
#if SSTAA
	result.coverage = Coverage;
#endif // SSTAA
	return PackDeferredGBufferC(result);
}// end of PS_PropFoliageDeferred_AlphaClipC()...

DeferredGBufferNC PS_PropFoliageDeferred_AlphaClipNC(treeVertexOutputD IN, DECLARE_FACING_INPUT(fFacing)) 
{
	DECLARE_FACING_FLOAT(fFacing)
	DeferredGBuffer result = PS_PropFoliageDeferred_AlphaClip(IN, fFacing);
	return PackDeferredGBufferNC(result);
}// end of PS_PropFoliageDeferred_AlphaClipNC()...

//
//
//
//

DeferredGBuffer	GetPropFoliageDeferredLod(treeVertexOutputLodD IN, float4 DiffuseCol, float fFacing )
{

	DeferredGBuffer OUT;
	TreePixelInstanceData inst;
	UnPackTreeInstanceData(IN, inst);

	float AlphaTint = 1;
#if PALETTE_TINT
	AlphaTint = IN.Diffuse.a;
	DiffuseCol.rgb *= IN.Diffuse.rgb;	// apply tinting
#endif

#if NORMAL_MAP
	//On PC the faces are the wrong way round so need to flip fFacing
	#if __WIN32PC// || RSG_ORBIS
		fFacing *= -1;
	#endif
	IN.worldNormal.xyz	*= globalDoubleSidedRendering ? sign(fFacing) : 1.0;
	float3 worldNormal	= CalculateWorldNormal(tex2D_NormalMap(BumpSampler, IN.TexCoord0.xy).xy, bumpiness, IN.worldTangent, IN.worldBinormal, IN.worldNormal.xyz);
#if TREE_NORMAL_CONTROL
	worldNormal			= lerp(float3(0,0,1), worldNormal, UseTreeNormals.x);
#endif // TREE_NORMAL_CONTROL
#else
	float3 worldNormal = /*normalize*/(IN.worldNormal.xyz);
#endif // NORMAL_MAP

#if SPECULAR
	float spI = (specularIntensityMult*4.0f);
	float spE = sqrt(specularFalloffMult/512.0f);
#else
	float spI = 0.0f;			
	float spE = 0.0f;			
#endif	// SPECULAR

	float naturalAO		= IN.TexCoord0.z;
	float artificialAO	= IN.TexCoord0.w;
	float TreeShadow	= IN.worldNormal.w;

	OUT.col0			= GetDiffuseColour(DiffuseCol, inst.alpha, IN.TexCoord0, AlphaTint);

#if NORMAL_MAP
	OUT.col1			= float4(worldNormal * 0.5f + 0.5f, 0); // [0..1]
	OUT.col2			= float4(spI, spE, 0, TreeShadow);
#else
	OUT.col1			= float4(worldNormal, 0); // [0..1]
	OUT.col2			= float4(spI, spE, 0, TreeShadow);
#endif
	
	OUT.col3			= float4(naturalAO*inst.naturalAmbientScale, artificialAO*inst.artificialAmbientScale*ArtificialBakeAdjustMult(), 0.0f, Pack2ZeroOneValuesToU8(0.0f, ShadowFalloff));						
	OUT.col3.xy			= sqrt(OUT.col3.xy * 0.5);

	return OUT;

}// end of PS_PropFoliageDeferredLod()...

DeferredGBuffer	PS_PropFoliageDeferredLod(treeVertexOutputLodD IN, DECLARE_FACING_INPUT(fFacing))
{
	DECLARE_FACING_FLOAT(fFacing)
	float4	DiffuseCol = SampleDiffuse(IN.TexCoord0.xy) + float4(1.,0.,1.,0.);
#if !DISABLE_DISCARDS
	//rageDiscard(ALLOW_TREE_DISCARD && DiffuseCol.a <= AlphaTest); // foliage_alphaRef);
#endif // !DISABLE_DISCARDS
	return GetPropFoliageDeferredLod(IN,  DiffuseCol, fFacing);
}

DeferredGBuffer	PS_PropFoliageDeferredLod_AlphaClip(treeVertexOutputLodD IN, DECLARE_FACING_INPUT(fFacing))
{
	DECLARE_FACING_FLOAT(fFacing)
#if ALPHA_CLIP && !DISABLE_DISCARDS
	FadeDiscardOptional( IN.Position, globalFade, ALLOW_TREE_DISCARD );
#endif // ALPHA_CLIP

#if RANDOMIZED_DITHER
	float4	DiffuseCol = SampleDiffuse(IN.TexCoord0.xy);
	RandomizedDither( DiffuseCol, IN.Position.xy, IN.offset.x);
	return GetPropFoliageDeferredLod( IN,DiffuseCol, fFacing);								   	
#else
	float4	DiffuseCol = SampleDiffuse(IN.TexCoord0.xy);
#if !DISABLE_DISCARDS
	// rageDiscard(ALLOW_TREE_DISCARD && DiffuseCol.a <= AlphaTest); //foliage_alphaRef);
#endif // !DISABLE_DISCARDS
	return GetPropFoliageDeferredLod(IN,  DiffuseCol, fFacing);
#endif
}// end of PS_PropFoliageDeferredLod_AlphaClip()...



half4	GetFoliageDeferredAlphaOnly( float4 DiffuseCol)
{
#if !DISABLE_DISCARDS
	rageDiscard(DiffuseCol.a <= treeAlphaRange.x);
#endif // !DISABLE_DISCARDS
	DiffuseCol=saturate((DiffuseCol-treeAlphaRange.x)/(treeAlphaRange.y-treeAlphaRange.x)+1./255.);
	return DiffuseCol;
}


half4	PS_PropFoliageDeferredLod_AlphaOnly(treeVertexOutputLodD IN) : SV_Target0
{
	return GetFoliageDeferredAlphaOnly( SampleDiffuse(IN.TexCoord0.xy) );
}
DeferredGBuffer	PS_PropFoliageDeferredLod_SubSampleAlpha(treeVertexOutputLodD IN, DECLARE_FACING_INPUT(fFacing))
{
	DECLARE_FACING_FLOAT(fFacing)
	float4	DiffuseCol = SampleDiffuse(IN.TexCoord0.xy);
#if !DISABLE_DISCARDS
	rageDiscard( (DiffuseCol.a <treeAlphaRange.y)  &&( SSAIsOpaquePixelFrac(IN.Position.xy) || DiffuseCol.a <= treeAlphaRange.x));
#endif // !DISABLE_DISCARDS
//	DiffuseCol.a=0.f;//saturate((DiffuseCol.a-treeAlphaRange.x)/(treeAlphaRange.y-treeAlphaRange.x)+1./255.);
	DeferredGBuffer result = GetPropFoliageDeferredLod(  IN, DiffuseCol, fFacing);

#if RSG_PC
	float fGlobalFade = dot(globalFade, globalFade)/4;
	if (fGlobalFade < 1)
	{
		rageDiscard( SSAIsOpaquePixelFrac(IN.Position.xy));
		result.col0.a = fGlobalFade;
	}
#endif // RSG_PC

	return result;

}// end of PS_PropFoliageDeferredLod_SubSampleAlpha()...



//
//
//
//
#if TREE_LOD2
DeferredGBuffer	PS_PropFoliageDeferredLod2(treeVertexOutputLod2D IN )
{
	DeferredGBuffer OUT;
	TreePixelInstanceData inst;
	UnPackTreeInstanceData(IN, inst);

	float4	DiffuseCol = SampleDiffuse(IN.TexCoord0.xy);
	float3 worldNormal = IN.worldNormal.xyz;

	float spI = 0.0f;			
	float spE = 1.0f;
	float naturalAO		= IN.TexCoord0.z;
	float artificialAO	= IN.TexCoord0.w;

	OUT.col0			= DiffuseCol;

	OUT.col0			= GetDiffuseColour(DiffuseCol, inst.alpha, IN.TexCoord0, 1);
	
	OUT.col1			= float4(worldNormal * 0.5f + 0.5f, 0); // [0..1]
	OUT.col2			= float4(spI, spE,  0.4999f, 1.0f);
	OUT.col3			= float4(naturalAO*inst.naturalAmbientScale, artificialAO*inst.artificialAmbientScale*ArtificialBakeAdjustMult(), 0.0f, Pack2ZeroOneValuesToU8(0.0f, ShadowFalloff));						
	OUT.col3.xy			= sqrt(OUT.col3.xy * 0.5);

	return OUT;
}// end of PS_PropFoliageDeferredLod2()...

DeferredGBuffer	PS_PropFoliageDeferredLod2_AlphaClip(treeVertexOutputLod2D IN )
{
#if ALPHA_CLIP && !DISABLE_DISCARDS
	FadeDiscardOptional( IN.Position, globalFade, ALLOW_TREE_DISCARD );
#endif // ALPHA_CLIP

	return PS_PropFoliageDeferredLod2(IN);
}// end of PS_PropFoliageDeferredLod2_AlphaClip()...

half4 PS_PropFoliageDeferredLod2_AlphaOnly(treeVertexOutputLod2D IN) : SV_Target0
{
	return GetFoliageDeferredAlphaOnly( SampleDiffuse(IN.TexCoord0.xy) );
}

DeferredGBuffer PS_PropFoliageDeferredLod2_SubSampleAlpha(treeVertexOutputLod2D IN)
{
#if ALPHA_CLIP && !DISABLE_DISCARDS
	FadeDiscard( IN.Position, globalFade );
#endif // ALPHA_CLIP
	float4	DiffuseCol = SampleDiffuse(IN.TexCoord0.xy);

#if !DISABLE_DISCARDS
	rageDiscard( (DiffuseCol.a <treeAlphaRange.y)  &&( SSAIsOpaquePixelFrac(IN.Position.xy) || DiffuseCol.a <= treeAlphaRange.x ));
#endif // !DISABLE_DISCARDS
	DiffuseCol.a=0.f;//saturate((DiffuseCol.a-treeAlphaRange.x)/(treeAlphaRange.y-treeAlphaRange.x)+1./255.);

	return PS_PropFoliageDeferredLod2(IN);
}// end of PS_PropFoliageDeferredLod2_AlphaClip()...
#endif //TREE_LOD2...

//
//
//
//

half4 PS_PropFoliageDeferredAlphaOnly(treeVertexOutputD IN) : SV_Target0
{
	return GetFoliageDeferredAlphaOnly( SampleDiffuse(IN.TexCoord0.xy) );
}

half4 PS_PropFoliageDeferredAlphaOnly_SSTA(treeVertexOutputD IN) : SV_Target0
{
	return PS_PropFoliageDeferredAlphaOnly(IN);
}

DeferredGBufferNC PS_PropFoliageDeferredSubSampleAlpha(treeVertexOutputD IN, DECLARE_FACING_INPUT(fFacing))
{
	DECLARE_FACING_FLOAT(fFacing)
#if ALPHA_CLIP && !DISABLE_DISCARDS
	FadeDiscard( IN.Position, globalFade );
#endif // ALPHA_CLIP

	float4	DiffuseCol = SampleDiffuse(IN.TexCoord0.xy);
#if !DISABLE_DISCARDS
	rageDiscard( ((DiffuseCol.a <treeAlphaRange.y)  &&( SSAIsOpaquePixelFrac(IN.Position.xy)) || DiffuseCol.a <= treeAlphaRange.x ));
#endif // !DISABLE_DISCARDS							
	DiffuseCol.a=saturate((DiffuseCol.a-treeAlphaRange.x)/(treeAlphaRange.y-treeAlphaRange.x)+1./255.);
	DeferredGBuffer result = GetFoliageDeferred( DiffuseCol, IN, fFacing);
	return PackDeferredGBufferNC(result);
}// end of PS_PropFoliageDeferredSubSampleAlpha()...
//
//
//
//
float4 PS_PropFoliageImposter(treeVertexOutputImp IN) : SV_Target
{
	float4	DiffuseCol = tex2Dlod(DiffuseSampler, float4(IN.TexCoord0,0,0));
//	float4	DiffuseCol = SampleDiffuse(TexCoord0.xy);

//	float4	DiffuseCol = tex2D(DiffuseSampler, IN.TexCoord0);

//	DiffuseCol.rgb = max(DiffuseCol.rgb, SampleDiffuse(IN.TexCoord0+float2(1.0f/32.0f, 0)).rgb);
//	DiffuseCol.rgb = max(DiffuseCol.rgb, SampleDiffuse(DiffuseSampler, IN.TexCoord0+float2(0, 1.0f/32.0f)).rgb);
//	DiffuseCol.rgb = max(DiffuseCol.rgb, SampleDiffuse(DiffuseSampler, IN.TexCoord0+float2(-1.0f/32.0f, 0)).rgb);
//	DiffuseCol.rgb = max(DiffuseCol.rgb, SampleDiffuse(DiffuseSampler, IN.TexCoord0+float2(0, -1.0f/32.0f)).rgb);

//	DiffuseCol.a=saturate(DiffuseCol.a*4.0f);

	return DiffuseCol;
}// end of PS_PropFoliageImposter()...

#define R3 (0.57735026918962576450914878050196f)

float3 imposterDir[8]={	float3( 1, 0, 0),
						float3( R3, R3,-R3),
						float3( 0, 1, 0), 
						float3(-R3, R3,-R3), 
						float3(-1, 0, 0), 
						float3(-R3,-R3,-R3), 
						float3( 0,-1, 0), 
						float3( R3,-R3,-R3)};
#if 1 //__PS3 || __WIN32PC
	// hard coded!!!
#else //__PS3..
	float3 normTable[16]={	float3( 0, 0, 0),

							float3( 0, 0,-1),
							float3( 0, 0.5,-0.8660254),
							float3( 0.4330127,-0.25,-0.8660254), 
							float3(-0.4330127,-0.25,-0.8660254),

							float3(0,0.93969262,0.34202014), 
							float3(0.81379768,0.46984631, -0.34202014), 
							float3(0.81379768,-0.46984631, 0.34202014), 
							float3(0,-0.93969262,-0.34202014), 
							float3(-0.81379768,-0.46984631, 0.34202014),
							float3(-0.81379768, 0.46984631,-0.34202014),

							float3(-0.4330127, 0.25,0.8660254), 
							float3( 0.4330127, 0.25,0.8660254),
							float3( 0, -0.5,-0.8660254),
							float3( 0, 0, 1),

							float3( 0, 0, 0)}; 

	//float altRemap[16]={0, 2,3,4,1, 6,7,8,9,10,5, 14,11,12,13, 15 };
	const static float altRemap[16]={0, 2,6,8,10, 11,7,12,9,13,5, 14,14,14,13, 15 };
#endif //__PS3...

//
//
//
//
float4 PS_PropFoliageDeferredImposter(treeVertexOutputImpD IN) : SV_Target
{
	float texAlpha=SampleDiffuse(IN.TexCoord0.xy).a;

//	float	DiffuseCol=SampleDiffuse(IN.TexCoord0);

//	DiffuseCol.a*=IN.OccAlpha.y;

//	float3 tdir=normalize(imposterDir[colorize.x]);
//	float3 tright=normalize(cross(float3(0,0,1), tdir));
//	float3 tup=normalize(cross(tright, tdir));

	float3 t_normal=normalize(IN.Normal);

//	float ang=acos(dot(tup,t_normal));
//	ang=lerp(ang, TWO_PI-ang, dot(tright,t_normal)<0.0f );

//	t_normal.x=dot(normalize(IN.Normal);
//	t_normal.y=dot(normalize(IN.Normal),gViewInverse[1].xyz);

//	float ang=acos(-t_normal.y);
//	ang=lerp(ang, TWO_PI-ang, ((t_normal.x<0.0f)?1.0f:0.0f)  );

//	float tmp0=(t_normal.x>0.0);
//	float tmp1=(t_normal.y>0.0);
//	float tmp2=(t_normal.z>0.0);
//	float tmp=(tmp2*4.0f+tmp1*2.0f+tmp0*1.0f)+1.0f;

#if 1
	float d=-1.0f;
	float w=0.0f;
	float i=0.0f;
	float m=d;
	float3 n=0.0f;
	float alt=i;
	float alt0=i;

	#if 1	//__PS3 || __WIN32PC
		n=float3( 0, 0,-1);
		alt0=2;
		d=dot(t_normal, n);
		w=((d>m)?1.0f:0.0f);
		i=lerp(i,1,w);
		m=lerp(m,d,w);
		alt=lerp(alt,alt0,w);

		n=float3( 0, 0.5,-0.8660254);
		alt0=6;
		d=dot(t_normal, n);
		w=((d>m)?1.0f:0.0f);
		i=lerp(i,2,w);
		m=lerp(m,d,w);
		alt=lerp(alt,alt0,w);

		n=float3( 0.4330127,-0.25,-0.8660254);
		alt0=8;
		d=dot(t_normal, n);
		w=((d>m)?1.0f:0.0f);
		i=lerp(i,3,w);
		m=lerp(m,d,w);
		alt=lerp(alt,alt0,w);

		n=float3(-0.4330127,-0.25,-0.8660254);
		alt0=10;
		d=dot(t_normal, n);
		w=((d>m)?1.0f:0.0f);
		i=lerp(i,4,w);
		m=lerp(m,d,w);
		alt=lerp(alt,alt0,w);

		n=float3(0,0.93969262,0.34202014);
		alt0=11;
		d=dot(t_normal, n);
		w=((d>m)?1.0f:0.0f);
		i=lerp(i,5,w);
		m=lerp(m,d,w);
		alt=lerp(alt,alt0,w);

		n=float3(0.81379768,0.46984631, -0.34202014);
		alt0=7;
		d=dot(t_normal, n);
		w=((d>m)?1.0f:0.0f);
		i=lerp(i,6,w);
		m=lerp(m,d,w);
		alt=lerp(alt,alt0,w);

		n=float3(0.81379768,-0.46984631, 0.34202014);
		alt0=12;
		d=dot(t_normal, n);
		w=((d>m)?1.0f:0.0f);
		i=lerp(i,7,w);
		m=lerp(m,d,w);
		alt=lerp(alt,alt0,w);

		n=float3(0,-0.93969262,-0.34202014);
		alt0=9;
		d=dot(t_normal, n);
		w=((d>m)?1.0f:0.0f);
		i=lerp(i,8,w);
		m=lerp(m,d,w);
		alt=lerp(alt,alt0,w);

		n=float3(-0.81379768,-0.46984631, 0.34202014);
		alt0=13;
		d=dot(t_normal, n);
		w=((d>m)?1.0f:0.0f);
		i=lerp(i,9,w);
		m=lerp(m,d,w);
		alt=lerp(alt,alt0,w);

		n=float3(-0.81379768, 0.46984631,-0.34202014);
		alt0=5;
		d=dot(t_normal, n);
		w=((d>m)?1.0f:0.0f);
		i=lerp(i,10,w);
		m=lerp(m,d,w);
		alt=lerp(alt,alt0,w);

		n=float3(-0.4330127, 0.25,0.8660254);
		alt0=14;
		d=dot(t_normal, n);
		w=((d>m)?1.0f:0.0f);
		i=lerp(i,11,w);
		m=lerp(m,d,w);
		alt=lerp(alt,alt0,w);

		n=float3( 0.4330127, 0.25,0.8660254);
		alt0=14;
		d=dot(t_normal, n);
		w=((d>m)?1.0f:0.0f);
		i=lerp(i,12,w);
		m=lerp(m,d,w);
		alt=lerp(alt,alt0,w);

		n=float3( 0, -0.5,-0.8660254);
		alt0=14;
		d=dot(t_normal, n);
		w=((d>m)?1.0f:0.0f);
		i=lerp(i,13,w);
		m=lerp(m,d,w);
		alt=lerp(alt,alt0,w);

		n=float3( 0, 0, 1);
		alt0=13;
		d=dot(t_normal, n);
		w=((d>m)?1.0f:0.0f);
		i=lerp(i,14,w);
		m=lerp(m,d,w);
		alt=lerp(alt,alt0,w);

	#else // __PS3 || __WIN32PC...
		for (int t=1;t<16;t++)
		{
	//		n=normalize(normTable[t]);
			n=normTable[t].xyz;
			alt0=altRemap[t];
			d=dot(t_normal, n);
			w=((d>m)?1.0f:0.0f);
			i=lerp(i,t,w);
			m=lerp(m,d,w);
			alt=lerp(alt,alt0,w);
		}
	#endif //__PS3 || __WIN32PC...

	#if 1 // __PS3 || __WIN32PC
		i=lerp(i,alt,((texAlpha<0.5f)?1.0f:0.0f));
	#else
		i=lerp(i,altRemap[i],((texAlpha<0.5f)?1.0f:0.0f));
	#endif

	float imp0=((colorize.x==0.0f)?1.0f:0.0f)*i;
	float imp1=((colorize.x==1.0f)?1.0f:0.0f)*i;
	float imp2=((colorize.x==2.0f)?1.0f:0.0f)*i;
	float imp3=((colorize.x==3.0f)?1.0f:0.0f)*i;
	float imp4=((colorize.x==4.0f)?1.0f:0.0f)*i;
	float imp5=((colorize.x==5.0f)?1.0f:0.0f)*i;
	float imp6=((colorize.x==6.0f)?1.0f:0.0f)*i;
	float imp7=((colorize.x==7.0f)?1.0f:0.0f)*i;

	float r=(imp1*16.0f+imp0)/255.0f;
	float g=(imp3*16.0f+imp2)/255.0f;
	float b=(imp5*16.0f+imp4)/255.0f;
	float a=(imp7*16.0f+imp6)/255.0f;

	return float4(r,g,b,a);

#else //#if 1...

	if (1)//DiffuseCol.a>(48.0f/255.0f))
	{
		float3 tdir=normalize(imposterDir[colorize.x]);
		float3 tright=normalize(cross(float3(0,0,1), tdir));
		float3 tup=normalize(cross(tright, tdir));

		float ang=acos(dot(tup,t_normal));
		ang=lerp(ang, TWO_PI-ang, dot(tright,t_normal)<0.0f );

//		float iang=(dot(t_normal.xyz, gViewInverse[0].xyz )<0.0f);//floor(saturate(ang/TWO_PI)*14.99f)+1.0f;
		float iang=floor(saturate(ang/TWO_PI)*14.99f)+1.0f;
//		float iang=tmp;

		float imp0=((colorize.x==0.0f)?1.0f:0.0f)*iang;
		float imp1=((colorize.x==1.0f)?1.0f:0.0f)*iang;
		float imp2=((colorize.x==2.0f)?1.0f:0.0f)*iang;
		float imp3=((colorize.x==3.0f)?1.0f:0.0f)*iang;
		float imp4=((colorize.x==4.0f)?1.0f:0.0f)*iang;
		float imp5=((colorize.x==5.0f)?1.0f:0.0f)*iang;
		float imp6=((colorize.x==6.0f)?1.0f:0.0f)*iang;
		float imp7=((colorize.x==7.0f)?1.0f:0.0f)*iang;

		float r=(imp1*16.0f+imp0)/255.0f;
		float g=(imp3*16.0f+imp2)/255.0f;
		float b=(imp5*16.0f+imp4)/255.0f;
		float a=(imp7*16.0f+imp6)/255.0f;

		return float4(r,g,b,a);
	}
	else
	{
		return 0;
	}
#endif //#if 1...
}// end of PS_PropFoliageDeferredImposter()...

/*
#if SHADOW_CASTING
#if __XENON

#if MODIFY_DEPTH
float4 PS_LDepthFoliage(pixelInputLD IN, out float depth : DEPTH): SV_Target
{
#else
float4 PS_LDepthFoliage(pixelInputLD IN): SV_Target
{
	float depth;
#endif // MODIFY_DEPTH
	return LinearDepthCommon(IN, 0.5f,depth);
}

#if MODIFY_DEPTH
float4 PS_LDepthFoliageOpaque(pixelInputLD IN, out float depth : DEPTH): SV_Target
{
#else
float4 PS_LDepthFoliageOpaque(pixelInputLD IN): SV_Target
{
	float depth;
#endif // MODIFY_DEPTH
	return LinearDepthCommon(IN, 0.0f, depth);
}

#endif // __XENON
#endif //SHADOW_CASTING...
*/

//
//
//
//
float4	PS_PropFoliageImposterShadow(treeVertexOutput IN)	:	COLOR
{
	float  alpha = SampleDiffuse(IN.TexCoord0).w;
#if __PS3 && !DISABLE_DISCARDS
	bool killPix = bool(alpha < AlphaTest); // 0.5f);
	rageDiscard(killPix);
#endif //AlphaTest...

	return alpha * 2.0f;
}// end of PS_PropFoliageImposterShadow()...



//
//
//
//
treeVertexOutput VS_PropFoliage0(treeVertexInput IN)
{
	return (treeVertexOutput)0;
}

//
//
//
//
float4	PS_PropFoliage0(treeVertexOutput IN)	:	COLOR
{
	return SampleDiffuse(IN.TexCoord0).w;
}

//
//
//
//
#if FORWARD_TREE_TECHNIQUES || WATER_REFLECTION_TECHNIQUES
treeVertexOutputWaterReflection VS_PropFoliageWaterReflection(treeVertexInputWaterReflection IN)
{
	treeVertexOutputWaterReflection OUT;

	#if TREE_LOD || TREE_LOD2
		const float TreeDepthness = IN.Diffuse.r;	// natural AO
		const float TreeShadowing = IN.Diffuse.g;	// artificial AO
	#else
		const float TreeDepthness = IN.Diffuse.a;
		const float TreeShadowing = SelfShadowing.x;
	#endif

	OUT.Position	= mul(float4(IN.Position,1), gWorldViewProj);
	OUT.TexCoord	= float4(IN.TexCoord, TreeDepthness, TreeShadowing);
	OUT.WorldPos	= mul(float4(IN.Position,1), gWorld).xyz;
#if TREE_NORMAL_CONTROL
	OUT.WorldNormal	= lerp(float3(0,0,1), mul(IN.Normal, (float3x3)gWorld), UseTreeNormals.x);
#else
	OUT.WorldNormal	= mul(IN.Normal, (float3x3)gWorld);
#endif // TREE_NORMAL_CONTROL
	OUT.FogData		= __CalcFogData(OUT.WorldPos - gViewInverse[3].xyz, 1);

	RAGE_COMPUTE_CLIP_DISTANCES(OUT.Position);

	return(OUT);
}// end of VS_PropFoliageWaterReflection()...

#if TREE_LOD2
treeVertexOutputWaterReflection VS_PropFoliageWaterReflectionLod2(treeInstancedVertexInput2D instIN)
{
treeVertexOutputWaterReflection OUT;

	treeVertexOutputLod2D outLod2d = vsPropFoliageDeferredLod2(instIN, true, OUT.WorldPos);	// flip for water reflection

	int nInstIndex;
	treeVertexInputLod2D IN;
	FetchTreeInstanceVertexData(instIN, IN, nInstIndex);

	OUT.Position	= outLod2d.Position;
	OUT.TexCoord	= outLod2d.TexCoord0;
	OUT.WorldNormal	= outLod2d.worldNormal;
	OUT.FogData		= __CalcFogData(OUT.WorldPos - gViewInverse[3].xyz, 1);

	RAGE_COMPUTE_CLIP_DISTANCES(OUT.Position);

	return(OUT);
}
#endif //TREE_LOD2...
#endif // FORWARD_TREE_TECHNIQUES || WATER_REFLECTION_TECHNIQUES
//
//
//
//
#if FORWARD_TREE_TECHNIQUES || WATER_REFLECTION_TECHNIQUES
half4 PS_PropFoliageWaterReflection(treeVertexOutputWaterReflection IN) : SV_Target
{
#if WATER_REFLECTION_CLIP_IN_PIXEL_SHADER && !DISABLE_DISCARDS
	rageDiscard(dot(float4(IN.WorldPos.xyz, 1), gLight0PosX) < 0);
#endif // WATER_REFLECTION_CLIP_IN_PIXEL_SHADER
	const float4 diffuse = SampleDiffuse(IN.TexCoord.xy);
#if !DISABLE_DISCARDS
	rageDiscard(diffuse.a <= AlphaTest); // foliage_alphaRef);
#endif // !DISABLE_DISCARDS
	SurfaceProperties surfaceInfo = (SurfaceProperties)0;

	surfaceInfo.surface_baseColor		= 1;
#if TREE_LOD || TREE_LOD2
	surfaceInfo.surface_baseColor.rg	= IN.TexCoord.zw;	// natural + artificial AO
#endif

#if PALETTE_TINT
	surfaceInfo.surface_baseColorTint	= 1;
#endif // PALETTE_TINT
	surfaceInfo.surface_diffuseColor	= float4(diffuse.rgb, 1);
	surfaceInfo.surface_worldNormal.xyz	= IN.WorldNormal;

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
		IN.WorldPos.xyz,
		surfaceInfo.surface_worldNormal.xyz,
		surfaceStandardLightingProperties);

	float4 OUT = calculateForwardLighting_WaterReflection(
		surface,
		material,
		light,
		#if TREE_LOD || TREE_LOD2
			1.0f,
			1.0f,				// shadowing
		#else
			IN.TexCoord.z,
			1 - IN.TexCoord.w,	// shadowing
		#endif
		SHADOW_RECEIVING);

	OUT.rgb	= lerp(OUT.rgb, IN.FogData.rgb, IN.FogData.w);
	OUT.a	= 1 - IN.FogData.a;

	OUT.a	*= diffuse.a;

	return PackReflection(OUT);
}// end of PS_PropFoliageWaterReflection()...
#endif // FORWARD_TREE_TECHNIQUES || WATER_REFLECTION_TECHNIQUES

#if __MAX
//
//
//
//
treeVertexOutput VS_MaxTransformUnlit(maxVertexInput IN)
{
treeVertexInput treeIN;
	treeIN.Position		= IN.pos;
#if TREE_LOD2
	treeIN.TexCoord0	= Max2RageTexCoord2(IN.texCoord1.xy);
#else
	treeIN.TexCoord0	= Max2RageTexCoord2(IN.texCoord0.xy);
#endif

	return VS_PropFoliage(treeIN);
}// end of VS_MaxTransformUnlit()...

//
//
//
//
float4	PS_MaxPropFoliage(treeVertexOutput IN)	: SV_Target
{
	float4 outColor = SampleDiffuse(IN.TexCoord0);
	if(!useAlphaFromDiffuse)
	{
		outColor.a	= tex2D(DiffuseSamplerAlpha,IN.TexCoord0).r;
	}
#if !DISABLE_DISCARDS	
	rageDiscard(outColor.a <= AlphaTest); // 90.0/255.0);
#endif // !DISABLE_DISCARDS
	return outColor;
} // end of PS_MaxPropFoliage()...
#endif //__MAX...
				   
// Uses no early kills as it allows for better scheduling ( 5 pass to 3 passes for tree.fx alphaclip )
#if 1
#define CGC_TREEFLAGS	"-unroll all --O1 -fastmath --fno-early-kills"
#else
#define CGC_TREEFLAGS	CGC_DEFAULTFLAGS
#endif

#if __MAX
	// ===============================
	// Tool technique
	// ===============================
	technique tool_draw
	{
		pass P0
		{
			AlphaTestEnable		= true;
			AlphaRef			= 90;
			AlphaFunc			= GREATEREQUAL;

			MAX_TOOL_TECHNIQUE_RS
			VertexShader= compile VERTEXSHADER	VS_MaxTransformUnlit();
			PixelShader	= compile PIXELSHADER	PS_MaxPropFoliage();
		}  
	}
#else //__MAX...

//
//
// Tree techniques:
//

#if DEFERRED_TECHNIQUES && NON_CUTOUT_DEFERRED_TECHNIQUES
technique deferred_draw
{
	pass p1
	{
		AlphaTestEnable			= false;
		#if __XENON
			// 360: don't dither, make it look the same on both platforms
			AlphaToMaskEnable	= false;
		#endif
	#if TREE_LOD
		SetGlobalDeferredMaterialTree();
		VertexShader= compile VERTEXSHADER			VS_PropFoliageDeferredLod();
		PixelShader = compile PIXELSHADER			PS_PropFoliageDeferredLod()  CGC_FLAGS(CGC_TREEFLAGS);
	#elif TREE_LOD2
		SetGlobalDeferredMaterial(DEFERRED_MATERIAL_DEFAULT);	// force standard lighting
		CullMode	= None;
		VertexShader= compile VERTEXSHADER			VS_PropFoliageDeferredLod2();
		PixelShader = compile PIXELSHADER			PS_PropFoliageDeferredLod2()  CGC_FLAGS(CGC_TREEFLAGS);
	#else
		SetGlobalDeferredMaterialTree();
		VertexShader= compile VERTEXSHADER			VS_PropFoliageDeferred();
		PixelShader = compile PIXELSHADER			PS_PropFoliageDeferredNC()  CGC_FLAGS(CGC_TREEFLAGS);
	#endif
	}
}
#if SKINNED_TREES
technique deferred_drawskinned
{
	pass p1
	{
		AlphaTestEnable			= false;
		#if __XENON
			// 360: don't dither, make it look the same on both platforms
			AlphaToMaskEnable	= false;
		#endif
		SetGlobalDeferredMaterialTree();
		VertexShader= compile VERTEXSHADER			VS_PropFoliageSkinDeferred();
		PixelShader = compile PIXELSHADER			PS_PropFoliageDeferredNC()  CGC_FLAGS(CGC_TREEFLAGS);
	}
}
#endif //SKINNED_TREES...

#if USE_PN_TRIANGLES
technique deferred_draw_sm50
{
	pass p1
	{
		AlphaTestEnable			= false;
		#if __XENON
			// 360: don't dither, make it look the same on both platforms
			AlphaToMaskEnable	= false;
		#endif
	#if TREE_LOD
		SetGlobalDeferredMaterialTree();
		VertexShader= compile VERTEXSHADER	VS_PropFoliageDeferredLod();
		PixelShader = compile PIXELSHADER	PS_PropFoliageDeferredLod()  CGC_FLAGS(CGC_TREEFLAGS);
	#elif TREE_LOD2
		SetGlobalDeferredMaterial(DEFERRED_MATERIAL_DEFAULT);	// force standard lighting
		CullMode	= None;
		VertexShader= compile VERTEXSHADER	VS_PropFoliageDeferredLod2();
		PixelShader = compile PIXELSHADER	PS_PropFoliageDeferredLod2()  CGC_FLAGS(CGC_TREEFLAGS);
	#else
		SetGlobalDeferredMaterialTree();
		VertexShader = compile VSDS_SHADER	VS_PropFoliageDeferred_PNTri();
		SetHullShader(compileshader(hs_5_0, HS_PropFoliageDeferred_PNTri()));
		SetDomainShader(compileshader(ds_5_0, DS_PropFoliageDeferred_PNTri()));
		PixelShader = compile PIXELSHADER PS_PropFoliageDeferredNC()  CGC_FLAGS(CGC_TREEFLAGS);
	#endif
	}
}
#endif // USE_PN_TRIANGLES
#endif // DEFERRED_TECHNIQUES && NON_CUTOUT_DEFERRED_TECHNIQUES

#if DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES
#if RSG_PC && SSTAA
technique deferredalphaclip_draw
{
	pass p0
	{
	#if TREE_LOD
		SetGlobalDeferredMaterialTree();
		VertexShader= compile VERTEXSHADER			VS_PropFoliageDeferredLod();
		PixelShader = compile PIXELSHADER			PS_PropFoliageDeferredLod_AlphaClip()  CGC_FLAGS(CGC_TREEFLAGS);
	#elif TREE_LOD2
		SetGlobalDeferredMaterial(DEFERRED_MATERIAL_DEFAULT);	// force standard lighting
		CullMode	= None;
		VertexShader= compile VERTEXSHADER			VS_PropFoliageDeferredLod2();
		PixelShader = compile PIXELSHADER			PS_PropFoliageDeferredLod2_AlphaClip()  CGC_FLAGS(CGC_TREEFLAGS);
	#else
		SetGlobalDeferredMaterialTree();
		VertexShader= compile VERTEXSHADER			VS_PropFoliageDeferred();
		PixelShader = compile PIXELSHADER			PS_PropFoliageDeferred_AlphaClipNC()  CGC_FLAGS(CGC_TREEFLAGS);
	#endif
	}
}
#endif // RSG_PC && SSTAA
technique SSTA_TECHNIQUE(deferredalphaclip_draw)
{
	pass p0
	{
	#if TREE_LOD
		SetGlobalDeferredMaterialTree();
		VertexShader= compile VERTEXSHADER			VS_PropFoliageDeferredLod();
		PixelShader = compile PIXELSHADER			PS_PropFoliageDeferredLod_AlphaClip()  CGC_FLAGS(CGC_TREEFLAGS);
	#elif TREE_LOD2
		SetGlobalDeferredMaterial(DEFERRED_MATERIAL_DEFAULT);	// force standard lighting
		CullMode	= None;
		VertexShader= compile VERTEXSHADER			VS_PropFoliageDeferredLod2();
		PixelShader = compile PIXELSHADER			PS_PropFoliageDeferredLod2_AlphaClip()  CGC_FLAGS(CGC_TREEFLAGS);
	#else
		SetGlobalDeferredMaterialTree();
		VertexShader= compile VERTEXSHADER			VS_PropFoliageDeferred();
		PixelShader = compile PS_GBUFFER_COVERAGE	PS_PropFoliageDeferred_AlphaClipC()  CGC_FLAGS(CGC_TREEFLAGS);
	#endif
	}
}
#if SKINNED_TREES
#if RSG_PC && SSTAA
technique deferredalphaclip_drawskinned
{
	pass p0
	{
		SetGlobalDeferredMaterialTree();
		VertexShader= compile VERTEXSHADER			VS_PropFoliageSkinDeferred();
		PixelShader = compile PIXELSHADER			PS_PropFoliageDeferred_AlphaClipNC()  CGC_FLAGS(CGC_TREEFLAGS);
	}
}
#endif // RSG_PC && SSTAA
technique SSTA_TECHNIQUE(deferredalphaclip_drawskinned)
{
	pass p0
	{
		SetGlobalDeferredMaterialTree();
		VertexShader= compile VERTEXSHADER			VS_PropFoliageSkinDeferred();
		PixelShader = compile PS_GBUFFER_COVERAGE	PS_PropFoliageDeferred_AlphaClipC()  CGC_FLAGS(CGC_TREEFLAGS);
	}
}
#endif //SKINNED_TREES...

#if USE_PN_TRIANGLES
technique deferredalphaclip_draw_sm50
{
	pass p0
	{
	#if TREE_LOD
		SetGlobalDeferredMaterialTree();
		VertexShader= compile VERTEXSHADER	VS_PropFoliageDeferredLod();
		PixelShader = compile PIXELSHADER	PS_PropFoliageDeferredLod_AlphaClip()  CGC_FLAGS(CGC_TREEFLAGS);
	#elif TREE_LOD2
		SetGlobalDeferredMaterial(DEFERRED_MATERIAL_DEFAULT);	// force standard lighting
		CullMode	= None;
		VertexShader= compile VERTEXSHADER	VS_PropFoliageDeferredLod2();
		PixelShader = compile PIXELSHADER	PS_PropFoliageDeferredLod2_AlphaClip()  CGC_FLAGS(CGC_TREEFLAGS);
	#else
		SetGlobalDeferredMaterialTree();
		VertexShader= compile VSDS_SHADER	VS_PropFoliageDeferred_PNTri();
		SetHullShader(compileshader(hs_5_0, HS_PropFoliageDeferred_PNTri()));
		SetDomainShader(compileshader(ds_5_0, DS_PropFoliageDeferred_PNTri()));
		PixelShader = compile PS_GBUFFER_COVERAGE	PS_PropFoliageDeferred_AlphaClipC()  CGC_FLAGS(CGC_TREEFLAGS);
	#endif
	}
}
#endif // USE_PN_TRIANGLES
#endif // DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES

#if DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES
technique deferredsubsamplealpha_draw
{
	pass p0
	{
		ZWriteEnable	= true;
	
	#if TREE_LOD
		SetGlobalDeferredMaterialTree();
		CullMode	= CW;
		VertexShader= compile VERTEXSHADER			VS_PropFoliageDeferredLod();
		PixelShader = compile PIXELSHADER			PS_PropFoliageDeferredLod_SubSampleAlpha()  CGC_FLAGS(CGC_TREEFLAGS);
	#elif TREE_LOD2

		SetGlobalDeferredMaterial(DEFERRED_MATERIAL_DEFAULT);	// force standard lighting
		CullMode	= None;
		VertexShader= compile VERTEXSHADER	VS_PropFoliageDeferredLod2();
		#if !DISABLE_SSA
			PixelShader = compile PIXELSHADER	PS_PropFoliageDeferredLod2_SubSampleAlpha() CGC_FLAGS(CGC_TREEFLAGS);
		#else
			PixelShader = compile PIXELSHADER	PS_PropFoliageDeferred_AlphaClipNC() CGC_FLAGS(CGC_TREEFLAGS);
		#endif
	#else
		SetGlobalDeferredMaterialTree();
		CullMode	= CW;
		VertexShader= compile VERTEXSHADER		VS_PropFoliageDeferred();
		PixelShader = compile PIXELSHADER		PS_PropFoliageDeferredSubSampleAlpha()  CGC_FLAGS(CGC_TREEFLAGS);
	#endif
	}
}

#if SKINNED_TREES
technique deferredsubsamplealpha_drawskinned
{
	pass p0
	{
		ZWriteEnable	= true;
		SetGlobalDeferredMaterialTree();
		CullMode	= CW;
		VertexShader= compile VERTEXSHADER			VS_PropFoliageSkinDeferred();
		PixelShader = compile PIXELSHADER			PS_PropFoliageDeferredSubSampleAlpha()  CGC_FLAGS(CGC_TREEFLAGS);
	}
}
#endif //SKINNED_TREES...

#if USE_PN_TRIANGLES
technique deferredsubsamplealpha_draw_sm50
{
	pass p0
	{
		zFunc = FixedupLESS;
		ZWriteEnable	= true;
	
	#if TREE_LOD
		SetGlobalDeferredMaterialTree();
		CullMode	= CW;
		VertexShader= compile VERTEXSHADER			VS_PropFoliageDeferredLod();
		PixelShader = compile PIXELSHADER			PS_PropFoliageDeferredLod_SubSampleAlpha()  CGC_FLAGS(CGC_TREEFLAGS);
	#elif TREE_LOD2
		SetGlobalDeferredMaterial(DEFERRED_MATERIAL_DEFAULT);	// force standard lighting
		CullMode	= None;
		VertexShader= compile VERTEXSHADER			VS_PropFoliageDeferredLod2();
		PixelShader = compile PIXELSHADER			PS_PropFoliageDeferredLod2_SubSampleAlpha() CGC_FLAGS(CGC_TREEFLAGS);
	#else
		SetGlobalDeferredMaterialTree();
		CullMode	= CW;	
		VertexShader= compile VSDS_SHADER			VS_PropFoliageDeferred_PNTri();
		SetHullShader(compileshader(hs_5_0,			HS_PropFoliageDeferred_PNTri()));
		SetDomainShader(compileshader(ds_5_0,		DS_PropFoliageDeferred_PNTri()));
		PixelShader = compile PIXELSHADER			PS_PropFoliageDeferredSubSampleAlpha()  CGC_FLAGS(CGC_TREEFLAGS);
	#endif
	}
}
#endif // USE_PN_TRIANGLES
#endif // DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES

#if DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES
technique deferredsubsamplealphaclip_draw
{
	pass p0
	{
		ZWriteEnable	= true;
	
	#if TREE_LOD
		SetGlobalDeferredMaterialTree();
		CullMode	= CW;
		VertexShader= compile VERTEXSHADER			VS_PropFoliageDeferredLod();
		PixelShader = compile PIXELSHADER			PS_PropFoliageDeferredLod_SubSampleAlpha()  CGC_FLAGS(CGC_TREEFLAGS);
	#elif TREE_LOD2

		SetGlobalDeferredMaterial(DEFERRED_MATERIAL_DEFAULT);	// force standard lighting
		CullMode	= None;
		VertexShader= compile VERTEXSHADER	VS_PropFoliageDeferredLod2();
		#if !DISABLE_SSA
			PixelShader = compile PIXELSHADER	PS_PropFoliageDeferredLod2_SubSampleAlpha() CGC_FLAGS(CGC_TREEFLAGS);
		#else
			PixelShader = compile PIXELSHADER	PS_PropFoliageDeferred_AlphaClipNC() CGC_FLAGS(CGC_TREEFLAGS);
		#endif
	#else
		SetGlobalDeferredMaterialTree();
		CullMode	= CW;
		VertexShader= compile VERTEXSHADER		VS_PropFoliageDeferred();
		PixelShader = compile PIXELSHADER		PS_PropFoliageDeferredSubSampleAlpha()  CGC_FLAGS(CGC_TREEFLAGS);
	#endif
	}
}

#if SKINNED_TREES
technique deferredsubsamplealphaclip_drawskinned
{
	pass p0
	{
		ZWriteEnable	= true;
		SetGlobalDeferredMaterialTree();
		CullMode	= CW;
		VertexShader= compile VERTEXSHADER			VS_PropFoliageSkinDeferred();
		PixelShader = compile PIXELSHADER			PS_PropFoliageDeferredSubSampleAlpha()  CGC_FLAGS(CGC_TREEFLAGS);
	}
}
#endif //SKINNED_TREES...

#if USE_PN_TRIANGLES
technique deferredsubsamplealphaclip_draw_sm50
{
	pass p0
	{
		zFunc = FixedupLESS;
		ZWriteEnable	= true;
	
	#if TREE_LOD
		SetGlobalDeferredMaterialTree();
		CullMode	= CW;
		VertexShader= compile VERTEXSHADER			VS_PropFoliageDeferredLod();
		PixelShader = compile PIXELSHADER			PS_PropFoliageDeferredLod_SubSampleAlpha()  CGC_FLAGS(CGC_TREEFLAGS);
	#elif TREE_LOD2
		SetGlobalDeferredMaterial(DEFERRED_MATERIAL_DEFAULT);	// force standard lighting
		CullMode	= None;
		VertexShader= compile VERTEXSHADER			VS_PropFoliageDeferredLod2();
		PixelShader = compile PIXELSHADER			PS_PropFoliageDeferredLod2_SubSampleAlpha() CGC_FLAGS(CGC_TREEFLAGS);
	#else
		SetGlobalDeferredMaterialTree();
		CullMode	= CW;	
		VertexShader= compile VSDS_SHADER			VS_PropFoliageDeferred_PNTri();
		SetHullShader(compileshader(hs_5_0,			HS_PropFoliageDeferred_PNTri()));
		SetDomainShader(compileshader(ds_5_0,		DS_PropFoliageDeferred_PNTri()));
		PixelShader = compile PIXELSHADER			PS_PropFoliageDeferredSubSampleAlpha()  CGC_FLAGS(CGC_TREEFLAGS);
	#endif
	}
}
#endif // USE_PN_TRIANGLES
#endif // DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES
#if DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES && !DISABLE_SSA

#if RSG_PC && SSTAA
technique deferredsubsamplewritealpha_draw
{
	// preliminary pass to write down alpha
	pass p0
	{
		DEFERRED_SUBSAMPLEWRITEALPHA_RENDERSTATES_TRANSPARENT()
		
	#if TREE_LOD
		CullMode	= CW;
		VertexShader= compile VERTEXSHADER			VS_PropFoliageDeferredLod();
		PixelShader = compile PIXELSHADER			PS_PropFoliageDeferredLod_AlphaOnly()  CGC_FLAGS(CGC_TREEFLAGS);
	#elif TREE_LOD2
		CullMode	= None;
		VertexShader= compile VERTEXSHADER			VS_PropFoliageDeferredLod2();
//		PixelShader = compile PIXELSHADER			PS_PropFoliageDeferredAlphaOnly()  CGC_FLAGS(CGC_TREEFLAGS);
		PixelShader = compile PIXELSHADER			PS_PropFoliageDeferredLod2_AlphaOnly() CGC_FLAGS(CGC_TREEFLAGS);
	#else
		CullMode	= CW;
		VertexShader= compile VERTEXSHADER			VS_PropFoliageDeferred();
		PixelShader = compile PIXELSHADER			PS_PropFoliageDeferredAlphaOnly()  CGC_FLAGS(CGC_TREEFLAGS);
	#endif
	}
}
#endif // RSG_PC && SSTAA

#if SKINNED_TREES
technique SSTA_TECHNIQUE(deferredsubsamplewritealpha_drawskinned)
{
	// preliminary pass to write down alpha
	pass p0
	{
		DEFERRED_SUBSAMPLEWRITEALPHA_RENDERSTATES_TRANSPARENT()
		
		CullMode	= CW;
		VertexShader= compile VERTEXSHADER			VS_PropFoliageSkinDeferred();
		PixelShader = compile PS_GBUFFER_COVERAGE	PS_PropFoliageDeferredAlphaOnly_SSTA()  CGC_FLAGS(CGC_TREEFLAGS);
	}
}
#endif //SKINNED_TREES...

#if USE_PN_TRIANGLES
technique deferredsubsamplewritealpha_draw_sm50
{
	// preliminary pass to write down alpha
	pass p0
	{
		DEFERRED_SUBSAMPLEWRITEALPHA_RENDERSTATES_TRANSPARENT()
		
	#if TREE_LOD
		CullMode	= CW;
		VertexShader= compile VERTEXSHADER	VS_PropFoliageDeferredLod();
		PixelShader = compile PIXELSHADER	PS_PropFoliageDeferredLod_AlphaOnly()  CGC_FLAGS(CGC_TREEFLAGS);
	#elif TREE_LOD2
		CullMode	= None;
		VertexShader= compile VERTEXSHADER	VS_PropFoliageDeferredLod2();
//		PixelShader = compile PIXELSHADER	PS_PropFoliageDeferredAlphaOnly()  CGC_FLAGS(CGC_TREEFLAGS);
		PixelShader = compile PIXELSHADER	PS_PropFoliageDeferredLod2_AlphaOnly() CGC_FLAGS(CGC_TREEFLAGS);
	#else
		CullMode	= CW;
		VertexShader= compile VSDS_SHADER			VS_PropFoliageDeferred_PNTri();
		SetHullShader(compileshader(hs_5_0,			HS_PropFoliageDeferred_PNTri()));
		SetDomainShader(compileshader(ds_5_0,		DS_PropFoliageDeferred_PNTri()));
		PixelShader = compile PIXELSHADER			PS_PropFoliageDeferredAlphaOnly()  CGC_FLAGS(CGC_TREEFLAGS);
	#endif
	}
}
#endif // USE_PN_TRIANGLES
#endif // DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES

#if FORWARD_TREE_TECHNIQUES
technique lightweight0_draw
{
	pass p0
	{
		VertexShader= compile VERTEXSHADER	VS_PropFoliageWaterReflection(); // reuse water reflection for mirror reflection
		PixelShader = compile PIXELSHADER	PS_PropFoliageWaterReflection()  CGC_FLAGS(CGC_TREEFLAGS);
	}
}
technique lightweight4_draw
{
	pass p0
	{
		VertexShader= compile VERTEXSHADER	VS_PropFoliageWaterReflection(); // reuse water reflection for mirror reflection
		PixelShader = compile PIXELSHADER	PS_PropFoliageWaterReflection()  CGC_FLAGS(CGC_TREEFLAGS);
	}
}
technique lightweight8_draw
{
	pass p0
	{
		VertexShader= compile VERTEXSHADER	VS_PropFoliageWaterReflection(); // reuse water reflection for mirror reflection
		PixelShader = compile PIXELSHADER	PS_PropFoliageWaterReflection()  CGC_FLAGS(CGC_TREEFLAGS);
	}
}
technique draw
{
	pass p0
	{
		VertexShader= compile VERTEXSHADER	VS_PropFoliageWaterReflection(); // reuse water reflection for mirror reflection
		PixelShader = compile PIXELSHADER	PS_PropFoliageWaterReflection()  CGC_FLAGS(CGC_TREEFLAGS);
	}
}
#if HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED
technique lightweightHighQuality0_draw
{
	pass p0
	{
		VertexShader= compile VERTEXSHADER	VS_PropFoliageWaterReflection(); // reuse water reflection for mirror reflection
		PixelShader = compile PIXELSHADER	PS_PropFoliageWaterReflection()  CGC_FLAGS(CGC_TREEFLAGS);
	}
}
technique lightweightHighQuality4_draw
{
	pass p0
	{
		VertexShader= compile VERTEXSHADER	VS_PropFoliageWaterReflection(); // reuse water reflection for mirror reflection
		PixelShader = compile PIXELSHADER	PS_PropFoliageWaterReflection()  CGC_FLAGS(CGC_TREEFLAGS);
	}
}
technique lightweightHighQuality8_draw
{
	pass p0
	{
		VertexShader= compile VERTEXSHADER	VS_PropFoliageWaterReflection(); // reuse water reflection for mirror reflection
		PixelShader = compile PIXELSHADER	PS_PropFoliageWaterReflection()  CGC_FLAGS(CGC_TREEFLAGS);
	}
}
#endif // HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED
#endif // FORWARD_TREE_TECHNIQUES

#if WATER_REFLECTION_TECHNIQUES
technique waterreflection_draw
{
	pass p0
	{
	#if TREE_LOD2
		VertexShader= compile VERTEXSHADER	VS_PropFoliageWaterReflectionLod2();
		PixelShader = compile PIXELSHADER	PS_PropFoliageWaterReflection()  CGC_FLAGS(CGC_TREEFLAGS);
	#else
		VertexShader= compile VERTEXSHADER	VS_PropFoliageWaterReflection();
		PixelShader = compile PIXELSHADER	PS_PropFoliageWaterReflection()  CGC_FLAGS(CGC_TREEFLAGS);
	#endif
	}
}

technique waterreflectionalphaclip_draw
{
	pass p0
	{
	#if TREE_LOD2
		VertexShader= compile VERTEXSHADER	VS_PropFoliageWaterReflectionLod2();
		PixelShader = compile PIXELSHADER	PS_PropFoliageWaterReflection()  CGC_FLAGS(CGC_TREEFLAGS);
	#else
		VertexShader= compile VERTEXSHADER	VS_PropFoliageWaterReflection();
		PixelShader = compile PIXELSHADER	PS_PropFoliageWaterReflection()  CGC_FLAGS(CGC_TREEFLAGS);
	#endif
	}
}

#endif // WATER_REFLECTION_TECHNIQUES

technique imposter_draw
{
	pass p0
	{
		CullMode		= None;
		AlphaTestEnable = false;
		AlphaBlendEnable= false;

		VertexShader= compile VERTEXSHADER	VS_PropFoliageImposter();
		PixelShader = compile PIXELSHADER	PS_PropFoliageImposter()  CGC_FLAGS(CGC_TREEFLAGS);
	}
}

#if DEFERRED_TECHNIQUES
technique imposterdeferred_draw
{
	pass p0
	{
		CullMode = None;

		VertexShader= compile VERTEXSHADER	VS_PropFoliageDeferredImposter();
		PixelShader = compile PIXELSHADER	PS_PropFoliageDeferredImposter()  CGC_FLAGS(CGC_TREEFLAGS);
	}
}
#endif // DEFERRED_TECHNIQUES

technique impostershadow_draw
{
	pass p0
	{
		VertexShader= compile VERTEXSHADER	VS_PropFoliage();
		PixelShader = compile PIXELSHADER	PS_PropFoliageImposterShadow()  CGC_FLAGS(CGC_TREEFLAGS);
	}
}

#if TREE_LOD2
VS_CascadeShadows_OUT VS_Cascade_PropFoliageLod2(treeVertexInputLod2D IN)
{
	VS_CascadeShadows_OUT OUT;
	float2 uvs			= IN.TexCoord0.xy - float2(0.5f,0.5f);
	float2 modelScale	= IN.TexCoord2.xy;

	float radiusW = treeLod2Width*modelScale.x;		//10.0f;
	float radiusH = treeLod2Height*modelScale.y;	//10.0f;
	
	// camera aligned or axis aligned:
	float3 up		= lerp( gViewInverse[0].xyz,	/*normalize*/(cross(gViewInverse[2].xyz, float3(0,0,-1))),	IN.TexCoord3.x);
	float3 across	= lerp(-gViewInverse[1].xyz,	float3(0,0,-1),												IN.TexCoord3.x);

	up		*= radiusW;
	across	*= radiusH;

	// camera aligned or axis aligned:
	float3 offset	= uvs.x*up + uvs.y*across;

	float ShadowBias = .3*radiusW;
	offset -= ShadowBias * gViewInverse[2].xyz;

	float3 IN_pos;

#if TREE_LOD2_DRAWABLE 
	float4x4 matWorld = gWorld;
	matWorld[0].x *= invGlobalEntityScaleXY;					// remove (potential) scaling from matrix
	matWorld[1].y *= invGlobalEntityScaleXY;
	matWorld[2].z *= invGlobalEntityScaleZ;

	float3 Position	= mul(IN.Position.xyz, (float3x3)matWorld);	// drawable's local rotation
	offset.xyz += Position.xyz;									// offset from middle of "mesh of 4-in-1's";

	IN_pos.x = dot(offset, matWorld[0].xyz);					// inverse local rotation - it will be re-applied by wvp matrix
	IN_pos.y = dot(offset, matWorld[1].xyz);
	IN_pos.z = dot(offset, matWorld[2].xyz);
	
	IN_pos.xy *= invGlobalEntityScaleXY;
	IN_pos.z  *= invGlobalEntityScaleZ;
#else
	offset.xyz += IN.Position.xyz;								// offset from middle of "mesh of 4-in-1's";

	IN_pos.x = dot(offset, gWorld[0].xyz);						// inverse local rotation - it will be re-applied by wvp matrix
	IN_pos.y = dot(offset, gWorld[1].xyz);
	IN_pos.z = dot(offset, gWorld[2].xyz);
#endif
	OUT.pos		= mul(float4(IN_pos.xyz,1), gWorldViewProj);	
	SHADOW_USE_TEXTURE_ONLY(OUT.tex = IN.TexCoord1.xy); 
	return(OUT);
}

SHADERTECHNIQUE_CASCADE_SHADOWS_TREE(VS_Cascade_PropFoliageLod2)

#else // TREE_LOD2

VS_CascadeShadows_OUT VS_Cascade_Micromovments( INSTANCED_ONLY(treeInstancedVertexInputD instanceIN) NOT_INSTANCED_ONLY(treeVertexInputD IN) )
{
	VS_CascadeShadows_OUT OUT; INSTANCED_ONLY(treeVertexInputD IN;)
	TreeVertInstanceData instIN;
	int nInstIndex = 0; INSTANCED_ONLY(FetchTreeInstanceVertexData(instanceIN, IN, nInstIndex);)
	GetTreeInstanceData(nInstIndex, instIN);
	float3 pos = IN.Position.xyz;
	if (1) pos = TreesApplyMicromovementsInternal(IN, instIN.worldMtx, true);
	if (0) pos = TreesApplyCollisionInternal(pos, IN.Diffuse.rgb, instIN.worldMtx, instIN.invEntityScaleXY, instIN.invEntityScaleZ);
	OUT.pos = ApplyCompositeWorldTransform(float4(pos,1), INSTANCING_ONLY_ARG(instIN.worldMtx) gWorldViewProj);
	SHADOW_USE_TEXTURE_ONLY(OUT.tex = IN.TexCoord0.xy);
	return OUT;
} 

SHADERTECHNIQUE_CASCADE_SHADOWS_TREE(VS_Cascade_Micromovments)

#endif // TREE_LOD2


#if __WIN32PC || RSG_ORBIS //|| __XENON// TODO -- implement for all platforms
SHADERTECHNIQUE_LOCAL_SHADOWS(VS_LinearDepth, VS_LinearDepth, PS_LinearDepth)
#endif // __WIN32PC || RSG_ORBIS

#if TREE_LOD
	SHADERTECHNIQUE_ENTITY_SELECT(VS_PropFoliageDeferredLod(), VS_PropFoliageDeferredLod())
#elif TREE_LOD2
	SHADERTECHNIQUE_ENTITY_SELECT(VS_PropFoliageDeferredLod2(), VS_PropFoliageDeferredLod2())
#else
	SHADERTECHNIQUE_ENTITY_SELECT(VS_PropFoliageDeferredSimple(), VS_PropFoliageDeferredSimple())
#endif

#if USE_PN_TRIANGLES && !TREE_LOD && !TREE_LOD2

technique entityselect_draw_sm50
{
	pass p1
	{
		SetGlobalDeferredMaterial(DEFERRED_MATERIAL_TREE);
		VertexShader = compile VSDS_SHADER	VS_PropFoliageDeferred_PNTri();
		SetHullShader(compileshader(hs_5_0, HS_PropFoliageDeferred_PNTri()));
		SetDomainShader(compileshader(ds_5_0, DS_PropFoliageDeferred_PNTri()));
		PixelShader = compile PIXELSHADER	PS_EntitySelectID_BANK_ONLY()  CGC_FLAGS(CGC_TREEFLAGS);
	}
}

#endif // USE_PN_TRIANGLES && !TREE_LOD && !TREE_LOD2

#include "../../Debug/debug_overlay_tree.fxh"

#endif //!__MAX... 

#endif //__TREES_COMMON_FXH__...

