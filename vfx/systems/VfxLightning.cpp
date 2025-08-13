///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	VfxLightning.cpp
//	BY	: 	Anoop Ravi Thomas
//	FOR	:	Rockstar Games
//	ON	:	17 December 2012
//	WHAT:	Lightning Effects
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

#include "VfxLightning.h"


// Rage headers
#include "vectormath/classfreefuncsv.h"

// Framework headers
#include "fwmaths/Random.h"
#include "fwsys/gameskeleton.h"
#include "fwsys/timer.h"
#include "vfx/channel.h"
#include "vfx/vfxwidget.h"

// Game Headers
#include "Audio/WeatherAudioEntity.h"
#include "Camera/Viewports/ViewportManager.h"
#include "control/replay/replay.h"
#include "control/replay/Effects/LightningFxPacket.h"
#include "game/weather.h"
#include "renderer/lights/lights.h"
#include "renderer/PostProcessFX.h"
#include "renderer/Util/ShaderUtil.h"
#include "renderer/Water.h"
#include "scene/playerswitch/PlayerSwitchInterface.h"
#include "Scene/Portals/Portal.h"
#include "System/ControlMgr.h"
#include "Vfx/systems/VfxLightningSettings.h"
#include "Vfx/clouds/CloudHat.h"
#include "vfx/misc/Coronas.h"


///////////////////////////////////////////////////////////////////////////////
//  OPTIMISATIONS - TURN ON IN OPTIMISATIONS.H
///////////////////////////////////////////////////////////////////////////////

VFX_SYSTEM_OPTIMISATIONS()
///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////

#define VFXLIGHTNING_DEG2RAD(x) (1.74532925199e-2f*(x))

#define VFXLIGHTNINGSTRIKE_SHADERVAR_PARAMS_STR							"LightningStrikeParams"
#define VFXLIGHTNINGSTRIKE_SHADERVAR_NOISE_PARAMS_STR					"g_LightningStrikeNoiseParams"
#define VFXLIGHTNINGSTRIKE_SHADERVAR_NOISE_TEX_STR						"noiseTexture"
///////////////////////////////////////////////////////////////////////////////
//  GLOBAL VARIABLES
///////////////////////////////////////////////////////////////////////////////

#define VFX_LIGHTNING_VTX_CNT (6144U)

CVfxLightningManager		g_vfxLightningManager;

CVfxLightningStrike* CVfxLightningManager::m_lightningStrikeRender = NULL;

grmShader*			CVfxLightningStrike::m_vfxLightningStrikeShader = NULL;
grcEffectTechnique	CVfxLightningStrike::m_vfxLightningStrikeShaderTechnique;
grcEffectVar		CVfxLightningStrike::m_vfxLightningStrikeShaderVars[VFXLIGHTNINGSTIRKE_SHADERVAR_COUNT];
grcTexture*			CVfxLightningStrike::m_vfxLightningStrikeNoiseTex = NULL;

bank_float		LIGHTNING_SEGEMENT_DELTA_SIZE		= 1.6f;				//Used for increasing size of lightning segment
#if __BANK
bool			g_UseDebugLightningSettings		= false;					//Whether to use Debug Lightning settings
CVfxLightningStrike::SLightningSettings g_DebugLightningSettings;			//Debug Lightning settings
eLightningType	g_forceLightningType			= LIGHTNING_TYPE_NONE;	//Force lightning type for debug
int				g_DebugForceVariationIndex		= -1;						//Force Variation Index
bool			g_RenderDebugLightSphere		= false;
bool			g_LightningUseExposureReset		= true;
bool			g_UseTimeCycleMod				= true;
bool			g_FlickerDirectionalColor		= true;

static bkButton *s_pCreateLightningWidgetsButton = NULL;
static bkGroup  *s_pVfxLightningGroup = NULL;
#endif
					
bank_float		g_AngularWidthPowerMult = 1.65f;
bank_float		g_LightningDistance = 1000.0f;
bank_float		g_LightningSegmentWidth = 10.0f;
int				g_currentSplitLevel = 0;	//Used for maintaining the split level during lightning strike construction

//Global Keyframed Cloud Settings Map
static VfxLightningSettings g_LightningSettings;								//Lightning settings loaded from file
static const char g_LightningSettingsFile[]	= "common:/data/effects/vfxlightningsettings";	//File containing the settings

struct VtxPB_VfxLightning_trait 
{
	//float3 pos						: POSITION;
	//float4 diffuseV					: COLOR0;
	//float4 startPos_NoiseOffset		: TEXCOORD0; //startPos.xyz, startNoiseOffset
	//float4 endPos_NoiseOffset 		: TEXCOORD1; //endPos.xyz, endNoiseOffset
	//float3 segmentParams			: TEXCOORD2; // currentSegmentWidth, intensity, bottomVert

	float		_pos[3];		// grcFvf::grcdsFloat3;	// POSITION
	Color32		_cpv;			// grcFvf::grcdsColor;	// COLOR0
	float		_uv0[4];		// grcFvf::grcdsFloat3;	// TEXCOORD0
	float		_uv1[4];		// grcFvf::grcdsFloat3;	// TEXCOORD1
	float		_uv2[3];		// grcFvf::grcdsFloat2;	// TEXCOORD2

	static inline void fvfSetup(grcFvf &fvf)
	{
		fvf.SetPosChannel(true, grcFvf::grcdsFloat3);			// POSITION
		fvf.SetDiffuseChannel(true, grcFvf::grcdsColor);		// COLOR0
		fvf.SetTextureChannel(0, true, grcFvf::grcdsFloat4);	// TEXCOORD0
		fvf.SetTextureChannel(1, true, grcFvf::grcdsFloat4);	// TEXCOORD1
		fvf.SetTextureChannel(2, true, grcFvf::grcdsFloat3);	// TEXCOORD2
	}

	static inline int GetComponentCount() { return 5; }

	static inline void SetPosition(traitParam& param, const Vector3& vp)
	{
		Assert(param.m_VtxElement == 0);
#if 0 == VTXBUFFER_USES_NOOP
		float *ptr=(float*)param.m_lockPointer;
		*(ptr++)=vp.x;
		*(ptr++)=vp.y; 
		*(ptr++)=vp.z;
		param.m_lockPointer = ptr;
#endif // 0 == VTXBUFFER_USES_NOOP

		ASSERT_ONLY(param.m_VtxElement = 1);
	}

	static inline void SetCPV(traitParam& param, const Color32 c)
	{
		Assert(param.m_VtxElement == 1);
#if 0 == VTXBUFFER_USES_NOOP	
		u32 *ptr=(u32*)param.m_lockPointer;
		*ptr = c.GetDeviceColor();
		ptr++;
		param.m_lockPointer = ptr;
#endif // 0 == VTXBUFFER_USES_NOOP	
		ASSERT_ONLY(param.m_VtxElement = 2);
	}

	static inline void SetUV(traitParam& param, const Vector4& fv)
	{
		Assert(param.m_VtxElement == 2);
#if 0 == VTXBUFFER_USES_NOOP
		float *ptr=(float*)param.m_lockPointer;
		*(ptr++)=fv.x;
		*(ptr++)=fv.y;
		*(ptr++)=fv.z;
		*(ptr++)=fv.w;
		param.m_lockPointer = ptr;
#endif // 0 == VTXBUFFER_USES_NOOP	
		ASSERT_ONLY(param.m_VtxElement = 3);
	}

	static inline void SetUV1(traitParam& param, const Vector4& sv)
	{
		Assert(param.m_VtxElement == 3);
#if 0 == VTXBUFFER_USES_NOOP
		float *ptr=(float*)param.m_lockPointer;
		*(ptr++)=sv.x;
		*(ptr++)=sv.y;
		*(ptr++)=sv.z;
		*(ptr++)=sv.w;
		param.m_lockPointer = ptr;
#endif // 0 == VTXBUFFER_USES_NOOP	
		ASSERT_ONLY(param.m_VtxElement = 4);
	}

	static inline void SetUV2(traitParam& param, const Vector4& sv)
	{
		Assert(param.m_VtxElement == 4);
#if 0 == VTXBUFFER_USES_NOOP
		float *ptr=(float*)param.m_lockPointer;
		*(ptr++)=sv.x;
		*(ptr++)=sv.y;
		*(ptr++)=sv.z;
		param.m_lockPointer = ptr;
#endif // 0 == VTXBUFFER_USES_NOOP	
		ASSERT_ONLY(param.m_VtxElement = 5);
	}

	static inline void SetUV3(traitParam& UNUSED_PARAM(param), const Vector4& UNUSED_PARAM(sv))
	{
		// No op
	}

	static inline void SetUV4(traitParam& UNUSED_PARAM(param), const Vector4& UNUSED_PARAM(m))
	{
		// No op
	}

	static inline void SetUV(traitParam& UNUSED_PARAM(param), const Vector2& UNUSED_PARAM(t))
	{
		// No op
	}

	static inline void SetTangent(traitParam& UNUSED_PARAM(param), const Vector4& UNUSED_PARAM(t))
	{
		// No op
	}
};

static VtxPushBuffer<VtxPB_VfxLightning_trait> g_VfxLightningVtxBuffer;


// static variables
const char* g_vfxLightningStrikeShaderVarName[VFXLIGHTNINGSTIRKE_SHADERVAR_COUNT] = 
{ 	
	VFXLIGHTNINGSTRIKE_SHADERVAR_PARAMS_STR,
	VFXLIGHTNINGSTRIKE_SHADERVAR_NOISE_PARAMS_STR,
	VFXLIGHTNINGSTRIKE_SHADERVAR_NOISE_TEX_STR,
};

///////////////////////////////////////////////////////////////////////////////
//  CODE
///////////////////////////////////////////////////////////////////////////////



// =============================================================================================== //
// HELPER CLASSES
// =============================================================================================== //

class dlCmdSetupVfxLightningFrameInfo : public dlCmdBase {
public:
	enum {
		INSTRUCTION_ID = DC_SetupVfxLightningFrameInfo,
	};

	dlCmdSetupVfxLightningFrameInfo();
	s32 GetCommandSize()						{ return(sizeof(*this)); }
	static void ExecuteStatic(dlCmdBase &cmd) { ((dlCmdSetupVfxLightningFrameInfo &) cmd).Execute(); }
	void Execute();

private:			

	CVfxLightningStrike* newFrameInfo;
	eLightningType lightningType;
};

class dlCmdClearVfxLightningFrameInfo : public dlCmdBase {
public:
	enum {
		INSTRUCTION_ID = DC_ClearVfxLightningFrameInfo,
	};

	s32 GetCommandSize()						{ return(sizeof(*this)); }
	static void ExecuteStatic(dlCmdBase &cmd) { ((dlCmdClearVfxLightningFrameInfo &) cmd).Execute(); }
	void Execute();
};

dlCmdSetupVfxLightningFrameInfo::dlCmdSetupVfxLightningFrameInfo()
{
	vfxAssertf(CSystem::IsThisThreadId(SYS_THREAD_UPDATE), "dlCmdSetupVfxLightningFrameInfo::dlCmdSetupVfxLightningFrameInfo - not called from the update thread");
	lightningType = g_vfxLightningManager.GetLightningTypeUpdate();
	if(lightningType == LIGHTNING_TYPE_STRIKE || lightningType == LIGHTNING_TYPE_DIRECTIONAL_BURST_STRIKE)
	{
		const int sizeOfBuffer = sizeof(CVfxLightningStrike);
		newFrameInfo = gDCBuffer->AllocateObjectFromPagedMemory<CVfxLightningStrike>(DPT_LIFETIME_RENDER_THREAD, sizeOfBuffer);
		sysMemCpy(newFrameInfo, g_vfxLightningManager.GetCurrentLightningStrikePtr(), sizeOfBuffer);
	}
	else
	{
		newFrameInfo = NULL;
	}
}

void dlCmdSetupVfxLightningFrameInfo::Execute()
{
	vfxAssertf(g_vfxLightningManager.m_lightningStrikeRender == NULL, "m_lightningStrikeRender is not NULL, means that dlCmdClearVfxLightningFrameInfo was not executed before this");
	g_vfxLightningManager.m_lightningStrikeRender = newFrameInfo;
	g_vfxLightningManager.SetLightningTypeRender(lightningType);
}


void dlCmdClearVfxLightningFrameInfo::Execute()
{
	vfxAssertf(CSystem::IsThisThreadId(SYS_THREAD_RENDER), "dlCmdClearVfxLightningFrameInfo::dlCmdClearVfxLightningFrameInfo - not called from the render thread");
	vfxAssertf( (g_vfxLightningManager.GetLightningTypeRender() != LIGHTNING_TYPE_STRIKE && g_vfxLightningManager.GetLightningTypeRender() != LIGHTNING_TYPE_DIRECTIONAL_BURST_STRIKE && g_vfxLightningManager.m_lightningStrikeRender == NULL) || 
		((g_vfxLightningManager.GetLightningTypeRender() == LIGHTNING_TYPE_STRIKE || g_vfxLightningManager.GetLightningTypeRender() == LIGHTNING_TYPE_DIRECTIONAL_BURST_STRIKE) && g_vfxLightningManager.m_lightningStrikeRender != NULL), 
		"m_lightningStrikeRender was set incorrectly. Lightning Type = %d, m_lightningStrikeRender = %p", (int)g_vfxLightningManager.GetLightningTypeRender(), g_vfxLightningManager.m_lightningStrikeRender );
	g_vfxLightningManager.m_lightningStrikeRender = NULL;
	g_vfxLightningManager.SetLightningTypeRender(LIGHTNING_TYPE_NONE);
}

//Here's how the quad points are

// 0             3
//	-------------
//	|		   /|
//	| 		  /	|
//	|		 /	|
//	|   	/	|
//	|	   /	|
//	|     /		|
//	|    /      |
//	|   /		|
//	|  /		|
//	| /         |
//	|/          |
//	-------------
// 1             2

void PushQuad(const Vector3 *points, const Color32& color , const Vector3 &startPos, const Vector3 &endPos, const Vector3 &tangent, float currentSegmentWidth, float intensity, float startNoiseOffset, float endNoiseOffset )
{
	const Vector4 vStartPos(startPos.x, startPos.y, startPos.z, startNoiseOffset);
	const Vector4 vEndPos(endPos.x, endPos.y, endPos.z, endNoiseOffset);
	const Vector4 vSegmentParams(currentSegmentWidth, intensity, tangent.z, 0.0f);

#define VfxLightningSetVertex(i) \
	g_VfxLightningVtxBuffer.BeginVertex(); \
	g_VfxLightningVtxBuffer.SetPosition(points[i]); \
	g_VfxLightningVtxBuffer.SetCPV(color); \
	g_VfxLightningVtxBuffer.SetUV(vStartPos); \
	g_VfxLightningVtxBuffer.SetUV1(vEndPos); \
	g_VfxLightningVtxBuffer.SetUV2(Vector4(currentSegmentWidth, intensity, ((i==0||i==3)?0.0f:1.0f),0.0f)); \
	g_VfxLightningVtxBuffer.EndVertex();

	
	VfxLightningSetVertex(3);
	VfxLightningSetVertex(2);
	VfxLightningSetVertex(1);

	VfxLightningSetVertex(3);
	VfxLightningSetVertex(1);
	VfxLightningSetVertex(0);


}

///////////////////////////////////////////////////////////////////////////////;
//  CLASS CVfxLightningStrike
//	Definition: This class is used for creating and maintaining the lightning
//				strike. It creates a bunch of segments using the Random "L"
//				system based on a set of rules defined by g_LightningSettings
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  SLightningSettings - Structure to store information about each 
//							lightning segment
///////////////////////////////////////////////////////////////////////////////

CVfxLightningStrike::SLightningSettings::SLightningSettings()
{
	Color = Color32(1.0f,1.0f,1.0f);
}

///////////////////////////////////////////////////////////////////////////////
//  Constructor
///////////////////////////////////////////////////////////////////////////////
CVfxLightningStrike::CVfxLightningStrike()
	: m_duration (0.0f)
	, m_age(0.0f)
	, m_keyframeLerpMainIntensity(0.0f)
	, m_keyframeLerpBranchIntensity(0.0f)
	, m_keyframeLerpBurst(0.0f)
	, m_intensityFlickerMult(1.0f)
	, m_intensityFlickerMultCurrent(0.0f)
	, m_intensityFlickerMultNext(0.0f)	
	, m_intensityFlickerCurrentTime(0.0f)
	, m_color(0.0f, 0.0f, 0.0f)
	, m_horizDir(0.0f, 0.0f)
	, m_Height(0.0f)
	, m_distance(0.0f)
	, m_AnimationTime(0.0f)
	, m_variationIndex(0)
	, m_isBurstLightning(false)
{	

}

///////////////////////////////////////////////////////////////////////////////
//  Destructor
///////////////////////////////////////////////////////////////////////////////
CVfxLightningStrike::~CVfxLightningStrike()
{

}

///////////////////////////////////////////////////////////////////////////////
//  InitShader
///////////////////////////////////////////////////////////////////////////////
void CVfxLightningStrike::InitShader()
{

	//Pushing the folder for loading MSAA shader
	ASSET.PushFolder("common:/shaders");
	// setup shader
	m_vfxLightningStrikeShader = grmShaderFactory::GetInstance().Create();
	const char* pName = "vfx_lightningStrike";
	m_vfxLightningStrikeShader->Load(pName);
	ASSET.PopFolder();

	m_vfxLightningStrikeShaderTechnique = m_vfxLightningStrikeShader->LookupTechnique("lightning");

	for(int i=0; i<VFXLIGHTNINGSTIRKE_SHADERVAR_COUNT; i++)
	{
		m_vfxLightningStrikeShaderVars[i] = m_vfxLightningStrikeShader->LookupVar(g_vfxLightningStrikeShaderVarName[i]);
	}


	m_vfxLightningStrikeNoiseTex= CShaderLib::LookupTexture("cloudnoise");
	Assert(m_vfxLightningStrikeNoiseTex);
	if(m_vfxLightningStrikeNoiseTex)
		m_vfxLightningStrikeNoiseTex->AddRef();
}

///////////////////////////////////////////////////////////////////////////////
//  InitShader
///////////////////////////////////////////////////////////////////////////////
void CVfxLightningStrike::ShutdownShader()
{
	delete m_vfxLightningStrikeShader; 
	m_vfxLightningStrikeShader = NULL;
}

///////////////////////////////////////////////////////////////////////////////
//  Init - Initializes the lightning strike and generates the segments
//	Params:
//		horizDir - horizontal Direction for direction of lightning
///////////////////////////////////////////////////////////////////////////////
void CVfxLightningStrike::Init()
{
	SLightningSettings settings;
	Init(settings);
}

///////////////////////////////////////////////////////////////////////////////
//  Init - Initializes the lightning strike and generates the segments
//	Params:
//		horizDir - horizontal Direction for direction of lightning
//		settings - Overrides the settings for the lightning strike
///////////////////////////////////////////////////////////////////////////////
void CVfxLightningStrike::Init(SLightningSettings &settings)
{
#if __BANK
	if(g_UseDebugLightningSettings)
	{
		settings = g_DebugLightningSettings;
	}
	if(g_DebugForceVariationIndex > -1)
	{
		m_variationIndex = g_DebugForceVariationIndex;
	}
	else
#endif
	{
		m_variationIndex = fwRandom::GetRandomNumberInRange(0, LIGHTNING_SETTINGS_NUM_VARIATIONS);
	}
	const VfxLightningStrikeVariationSettings& strikeVariationSettings = g_LightningSettings.GetStrikeSettings().GetVariation(m_variationIndex);

	//keyframe lerp values determined here to choose a curve between min and max curve
	m_keyframeLerpMainIntensity = fwRandom::GetRandomNumberInRange(0.0f, 1.0f);
	m_keyframeLerpBranchIntensity = fwRandom::GetRandomNumberInRange(0.0f, 1.0f);
	m_keyframeLerpBurst = fwRandom::GetRandomNumberInRange(0.0f, 1.0f);

	m_intensityFlickerMult = 1.0f;
	m_intensityFlickerMultCurrent = g_LightningSettings.GetStrikeSettings().GetBaseIntensityFlickerRandom();
	m_intensityFlickerMultNext = g_LightningSettings.GetStrikeSettings().GetBaseIntensityFlickerRandom();
	m_intensityFlickerCurrentTime = 0.0f;
	m_segments.clear();
	m_age = 0.0f;
	m_color = settings.Color;
	m_horizDir.Set(0.0f, 0.0f);
	m_Height = 0.0f;
	m_isBurstLightning = false;
	m_duration = strikeVariationSettings.GetDurationRandom();
	m_AnimationTime = strikeVariationSettings.GetAnimationTimeRandom();
	m_distance = 0.0f;

	//Figure out starting position based on height
	const int cloudHatIndex = CLOUDHATMGR? CLOUDHATMGR->GetActiveCloudHatIndex():-1;
	bool bValidPositionFound = false;

	const grcViewport& grcVp = gVpMan.GetCurrentViewport()->GetGrcViewport();
	// const Vector3 camPos = VEC3V_TO_VECTOR3(grcVp.GetCameraPosition());

	ScalarV xv = grcVp.GetCameraMtx().GetCol2().GetX();
	ScalarV yv = grcVp.GetCameraMtx().GetCol2().GetY();

	ScalarV ov = Arctan2(xv, -yv);
	m_rootYaw = ov.Getf()*RtoD;

	Vector2 rootOrbitPos;
	//Get += 90 deg from center
	m_rootYaw += g_LightningSettings.GetStrikeSettings().GetOrbitDirXRandom();
	//Root position of the lightning 
	m_rootPitch = g_LightningSettings.GetStrikeSettings().GetOrbitDirYRandom();
	rootOrbitPos.Set(m_rootYaw, m_rootPitch);

	if(CLOUDHATMGR && CLOUDHATMGR->IsCloudHatIndexValid(cloudHatIndex))
	{
		const CloudHatFragContainer &currentCloudHat = CLOUDHATMGR->GetCloudHat(cloudHatIndex);
		if(currentCloudHat.IsAnyLayerStreamedIn())
		{
			Vector3 positionOnCloudHat;
			bValidPositionFound = currentCloudHat.CalculatePositionOnCloudHat(positionOnCloudHat, rootOrbitPos);
			
			m_Height = positionOnCloudHat.z;
			m_horizDir.Set(positionOnCloudHat.x, positionOnCloudHat.y) ;
			m_distance = m_horizDir.Mag();
			m_horizDir.Normalize();
#if __BANK
			if(g_forceLightningType == LIGHTNING_TYPE_STRIKE)
			{
				m_isBurstLightning = false;
			}
			else if(g_forceLightningType == LIGHTNING_TYPE_DIRECTIONAL_BURST_STRIKE)
			{
				m_isBurstLightning = true;
			}
			else
#endif
			{
				m_isBurstLightning = (m_distance < g_LightningSettings.GetStrikeSettings().GetMaxDistanceForBurst());
			}

		}
	}

	if(!bValidPositionFound)
	{
		Vec3V defaultLightningPosition;
		Vec3V dir(0.0f, 1.0f, 0.0f);
		dir = RotateAboutXAxis(dir, ScalarVFromF32(rootOrbitPos.y*DtoR));
		dir = RotateAboutZAxis(dir, ScalarVFromF32(rootOrbitPos.x*DtoR));

		defaultLightningPosition = grcVp.GetCameraPosition() + dir * ScalarVFromF32(g_LightningDistance);
		m_Height = defaultLightningPosition.GetZf();
		m_horizDir.Set(defaultLightningPosition.GetXf(), defaultLightningPosition.GetYf()) ;
		m_distance = m_horizDir.Mag();
		m_horizDir.Normalize();

	}

	vfxAssertf(m_distance > 0.0f, "Distance of lightning is set to 0.0f.");
	GenerateLightning();

}

///////////////////////////////////////////////////////////////////////////////
//  Shutdown - called when the lightning strike dies
///////////////////////////////////////////////////////////////////////////////
void CVfxLightningStrike::Shutdown()
{
	m_isBurstLightning = false;
	m_duration = 0.0f;
	m_age = 0.0f;
	m_segments[0].Intensity = 0.0f;
}
///////////////////////////////////////////////////////////////////////////////
//  Decay Functions used for reducing the settings on each iteration level
///////////////////////////////////////////////////////////////////////////////
// decay based on global subdivision level
float CVfxLightningStrike::Decay()
{
	return Decay(2.0f);
}
// decay based on global subdivision level
float CVfxLightningStrike::Decay(float amount)
{ 
	return Decay(amount, g_currentSplitLevel);
}
// decay based on provided subdivision level 
float CVfxLightningStrike::Decay(float amount, u32 level)
{ 
	return 1.0f / pow(amount, level + 1.0f); 
	//return exp(-amount * g_currentSplitLevel);
}

///////////////////////////////////////////////////////////////////////////////
//  GenerateLightning - Used for creating the lightning segments
//						It uses the Random "L" system (or fractal) algorithm 
//						for generating the segments. We first start out with 
//						one segments and then we repeatedly split that segment
//						in a number of iterations. On each iteration, we split
//						the segments based on a SplitSegment function and 
//						store the output segments. On the next iteration level
//						we use the newly generated segments as input. This continues
//						till we complete all iterations
//						On each iteration we use the Pattern mask to determine if 
//						we will create branches (or forks) on that current iteration
//						
///////////////////////////////////////////////////////////////////////////////
void CVfxLightningStrike::GenerateLightning()
{	

	SLightningSegment &firstSegment = m_segments.Append();

	//Initialize the start and end positions in the scale 0-1
	firstSegment.startPos.Set(0.0f, 1.0f);
	firstSegment.endPos.Set(g_LightningSettings.GetStrikeSettings().GetVariation(m_variationIndex).GetEndPointOffsetRandom(), 0.0f);
	firstSegment.startNoiseOffset = 0.0f;
	firstSegment.endNoiseOffset = 1.0f;

	//Set rest of the base lightning strike settings
	static const float minDistanceForLerp = 2000.0f;
	static const float maxDistanceForLerp = 10000.0f;
	const float widthFactor = 1.0f - Saturate((m_distance - minDistanceForLerp) / (maxDistanceForLerp - minDistanceForLerp));

	firstSegment.width = g_LightningSettings.GetStrikeSettings().GetBaseWidthRandom() * widthFactor;
	firstSegment.Intensity = g_LightningSettings.GetStrikeSettings().GetBaseIntensityRandom();

	//this keeps track of whether the segment is part of the main bolt. The first segment
	firstSegment.IsMainBoltSegment = true;
	firstSegment.distanceFromSource = 0;
	//Create Fractal Lightning
	const u8 subdivisionCount = g_LightningSettings.GetStrikeSettings().GetSubdivisionCount();
	const u8 subdivisionPatternMask = g_LightningSettings.GetStrikeSettings().GetVariation(m_variationIndex).GetSubdivisionPatternMask();

	//We first start out with one segment and use each iteration level to subdivide the current
	//list of segments into smaller segments
	for(u8 i=0; i<subdivisionCount; i++)
	{
		g_currentSplitLevel = i;
		//pattern mask used for deciding if we need to create branches on this iteration
		int fork = subdivisionPatternMask & (1<<i);
		SplitSegments(m_segments, fork);
	}

}


///////////////////////////////////////////////////////////////////////////////
//  CreateSegment - Helper function for creating the new segment
//	Params:
//			out_segment: Store output segment
//			startPos:	Start Pos of new segment
//			endPos:		End Pos of new segment
//			distanceFromSource: number of segments away from source of lightning
//			IsMainBoltSegment: bool to keep track if this segment should be part of 
//								main bolt or not
//			isBranch:			if its a branch then reduce intensity and width by
//								"Decay Factor"
//			currentWidth:		width of parent segment
//			currentIntensity:	Intensity of input segment
//
///////////////////////////////////////////////////////////////////////////////
void CVfxLightningStrike::CreateSegment
(
	SLightningSegment& out_segment,
	const Vector2 &startPos, 
	const Vector2 &endPos,
	const u8 distanceFromSource, 
	bool IsMainBoltSegment, 
	bool isBranch,
	float currentWidth,
	float currentIntensity,
	float startNoiseOffset,
	float endNoiseOffset
)
{
	out_segment.startPos = startPos;
	out_segment.endPos = endPos;
	out_segment.width = currentWidth;
	out_segment.Intensity = currentIntensity;
	out_segment.distanceFromSource = distanceFromSource;
	out_segment.startNoiseOffset = startNoiseOffset;
	out_segment.endNoiseOffset = endNoiseOffset;
	// if its a branch then reduce width and intensity
	if(isBranch)
	{
		out_segment.width *= g_LightningSettings.GetStrikeSettings().GetWidthLevelDecayRandom();
		out_segment.Intensity *= g_LightningSettings.GetStrikeSettings().GetIntensityLevelDecayRandom();
		out_segment.IsMainBoltSegment = false;
	}
	else
	{
		//if not then retain current main bolt setting
		out_segment.IsMainBoltSegment = IsMainBoltSegment;
	}
	out_segment.width = Max(out_segment.width, g_LightningSettings.GetStrikeSettings().GetWidthMinClamp());
	out_segment.Intensity = Max(out_segment.Intensity, g_LightningSettings.GetStrikeSettings().GetIntensityMinClamp());
}

///////////////////////////////////////////////////////////////////////////////
//  PatternZigZag : Splits an input segment into 2 and offsets the split point
//					Using the Zig Zag settings 
// Illustration:
//
//	:\
//	: \
//	:  \
//	:   \
//	:    \
//	:    /
//	:   /
//	:  /
//	: /
//	:/
//
///////////////////////////////////////////////////////////////////////////////
void CVfxLightningStrike::PatternZigZag(const SLightningSegment &segment, SLightningSingleSegmentSplit &output)
{
	//Offset split point in direction perpendicular to input segment
	Vector2 forward = (segment.endPos - segment.startPos);
	forward.Normalize();
	Vector2 right = forward;
	right.Rotate(PI * 0.5f);
	
	// subdivision by splitting segment into two and randomly moving split point. In this function, the "ZigZag"
	// settings are used

	//Apply decay based on iteration level
	float decay = Decay(g_LightningSettings.GetStrikeSettings().GetVariation(m_variationIndex).GetZigZagDeviationDecay());	

	//Get offset amount
	float delta =  decay * g_LightningSettings.GetStrikeSettings().GetVariation(m_variationIndex).GetZigZagDeviationRightRandom();

	float zigzagFractionRandom = g_LightningSettings.GetStrikeSettings().GetVariation(m_variationIndex).GetZigZagFractionRandom();
	// Calculate new offset position by first choosing where along the segment the split should occur
	// and then offset it by the delta in the direction perpendicular to input position
	Vector2 jittered = Lerp(zigzagFractionRandom, segment.startPos, segment.endPos) + right * delta;

	float midNoiseOffset = Lerp(zigzagFractionRandom, segment.startNoiseOffset, segment.endNoiseOffset);
	SLightningSegment &segment1 = output.Append();
	SLightningSegment &segment2 = output.Append();

	//Create first segment using segment start position and new offset position
	CreateSegment(segment1, segment.startPos, jittered, segment.distanceFromSource * 2, segment.IsMainBoltSegment, false, segment.width, segment.Intensity, segment.startNoiseOffset, midNoiseOffset);

	//Create second segment using new offset position and segment end position position
	CreateSegment(segment2, jittered, segment.endPos, segment.distanceFromSource * 2 + 1, segment.IsMainBoltSegment, false, segment.width, segment.Intensity, midNoiseOffset, segment.endNoiseOffset);

}

///////////////////////////////////////////////////////////////////////////////
//  PatternFork:	Splits an input segment into 2 and offsets the split point.
//					Also adds a new segment like a branch
//					Using the Fork Zig Zag settings for the split and the Fork 
//					Settings for the new branch
// Illustration:
//
//	:\
//	: \
//	:  \
//	:   \
//	:    \
//	:    /\
//	:   /  \
//	:  /    \
//	: /      \
//	:/        \
///////////////////////////////////////////////////////////////////////////////
void CVfxLightningStrike::PatternFork(const SLightningSegment &segment, SLightningSingleSegmentSplit &output)
{
	//Offset split point in direction perpendicular to input segment
	Vector2 forward = (segment.endPos - segment.startPos);
	forward.Normalize();
	Vector2 right = forward;
	right.Rotate(PI * 0.5f);

	// subdivision by splitting segment into two and randomly moving split point
	// and adding a branch segment between the split position and the random end point
	// Same as Zig Zag, but in this function, the "ForkZigZag" settings are used

	//First get the split point using the "ForkZigZag" Settings

	//Apply decay based on iteration level
	float decay = Decay(g_LightningSettings.GetStrikeSettings().GetVariation(m_variationIndex).GetForkZigZagDeviationDecay());

	//Get offset amount
	float delta =  decay * g_LightningSettings.GetStrikeSettings().GetVariation(m_variationIndex).GetForkZigZagDeviationRightRandom();

	float forkZigZagDeviationRightRandom = g_LightningSettings.GetStrikeSettings().GetVariation(m_variationIndex).GetForkFractionRandom();

	// Calculate new split position by first choosing where along the segment the split should occur
	// and then offset it by the delta in the direction perpendicular to input position
	Vector2 jittered = Lerp(forkZigZagDeviationRightRandom, segment.startPos, segment.endPos) + 
		right * delta ;

	float midNoiseOffset = Lerp(forkZigZagDeviationRightRandom, segment.startNoiseOffset, segment.endNoiseOffset);
	SLightningSegment &segment1 = output.Append();
	SLightningSegment &segment2 = output.Append();

	//Create first segment using segment start position and new offset position
	CreateSegment(segment1, segment.startPos, jittered, segment.distanceFromSource * 2, segment.IsMainBoltSegment, false, segment.width, segment.Intensity, segment.startNoiseOffset, midNoiseOffset);
	
	//Create second segment using new offset position and segment end position position
	CreateSegment(segment2, jittered, segment.endPos, segment.distanceFromSource * 2 + 1, segment.IsMainBoltSegment, false, segment.width, segment.Intensity, midNoiseOffset, segment.endNoiseOffset);

	// Calculate the new branch end position
	
	//For this first calculate the direction of the new point.
	Vector2 v_delta =  
		Vector2(
		g_LightningSettings.GetStrikeSettings().GetVariation(m_variationIndex).GetForkDeviationRightRandom(), 
		g_LightningSettings.GetStrikeSettings().GetVariation(m_variationIndex).GetForkDeviationForwardRandom()
		);

	Vector2 dir = right * v_delta.x + forward * v_delta.y;

	dir.Normalize();
	//Now calculate the length of the new branch. Applied Decay based on iteration level
	float  f_length = g_LightningSettings.GetStrikeSettings().GetVariation(m_variationIndex).GetForkLengthRandom()
		* Decay(g_LightningSettings.GetStrikeSettings().GetVariation(m_variationIndex).GetForkLengthDecay());

	//Calculate the forked position of the forked point with the length and direction
	Vector2 f_jittered = jittered + dir * f_length;

	SLightningSegment &segment3 = output.Append();
	//Create thrid segment using new split position and new forked position
	CreateSegment(segment3, jittered, f_jittered, segment.distanceFromSource * 2 + 1, segment.IsMainBoltSegment, true, segment.width, segment.Intensity, midNoiseOffset, 1.0f);
}


///////////////////////////////////////////////////////////////////////////////
//  SplitSegment:	Chooses which kind of splitting should occur on the input
//					segment based on fork value
///////////////////////////////////////////////////////////////////////////////
void CVfxLightningStrike::SplitSegment(const SLightningSegment &inSegment, SLightningSingleSegmentSplit &outSegments, int fork)
{
	outSegments.clear();
	
	if(fork)
	{
		PatternFork(inSegment, outSegments);
	}
	else
	{
		PatternZigZag(inSegment, outSegments);
	}

}

///////////////////////////////////////////////////////////////////////////////
//  SplitSegments:	Single Iteration over all input segments. It splits each 
//					segment into multiple segments based on whether fork should
//					be applied or not. And then copies the results back as
//					output
///////////////////////////////////////////////////////////////////////////////
void CVfxLightningStrike::SplitSegments(SLightningSegments &segments, int fork)
{
	SLightningSegments newLightningSegments;
	SLightningSingleSegmentSplit tempLightningSegments;

	newLightningSegments.clear();
	for(int i=0; i<segments.GetCount(); i++)
	{
		//split current set of segments and store them in another array
		tempLightningSegments.clear();
		SplitSegment(segments[i], tempLightningSegments, fork);
		for(int j=0; j< tempLightningSegments.GetCount(); j++)
		{
			SLightningSegment &newSegment = newLightningSegments.Append();
			newSegment = tempLightningSegments[j];
		}
	}
	//copy back results as new set of segments
	segments = newLightningSegments;
}

///////////////////////////////////////////////////////////////////////////////
//  Update: for the lightning strike
///////////////////////////////////////////////////////////////////////////////
void CVfxLightningStrike::Update(float deltaTime)
{
	const float flickerChangeTime = 1.0f / g_LightningSettings.GetStrikeSettings().GetBaseIntensiyFlickerFrequency();
	m_intensityFlickerCurrentTime += deltaTime;
	m_age += deltaTime;
	if(!IsDead())
	{
		g_LightningSettings.GetStrikeSettings().GetVariation(m_variationIndex).GetKeyFrameSettings(m_age / m_duration, m_currentFrame);
		if(m_intensityFlickerCurrentTime > flickerChangeTime)
		{
			m_intensityFlickerCurrentTime = 0.0f;
			m_intensityFlickerMultCurrent = m_intensityFlickerMultNext;
			m_intensityFlickerMultNext = g_LightningSettings.GetStrikeSettings().GetBaseIntensityFlickerRandom();
		}
		float lerpFactor = Saturate(m_intensityFlickerCurrentTime * g_LightningSettings.GetStrikeSettings().GetBaseIntensiyFlickerFrequency());
		m_intensityFlickerMult = Lerp(lerpFactor, m_intensityFlickerMultCurrent, m_intensityFlickerMultNext);
	}
}

///////////////////////////////////////////////////////////////////////////////
//  IsDead: checks if age is greater than max age. If so, then the lightning
//	shuts down in CVfxlightningManager::Update()
///////////////////////////////////////////////////////////////////////////////
bool CVfxLightningStrike::IsDead()
{
	return (m_duration == 0.0f || m_age > m_duration);

}

///////////////////////////////////////////////////////////////////////////////
//  GetBurstLightningIntensity: Used for burst only lightning
///////////////////////////////////////////////////////////////////////////////
float CVfxLightningStrike::GetBurstLightningIntensity()
{
	float burstIntensity = Lerp(m_keyframeLerpMainIntensity,m_currentFrame.m_MainIntensity.GetXf(), m_currentFrame.m_MainIntensity.GetYf());
	burstIntensity = (burstIntensity > g_LightningSettings.GetStrikeSettings().GetBurstThresholdIntensity())? burstIntensity : 0.0f;
	return burstIntensity;
}


///////////////////////////////////////////////////////////////////////////////
//  GetBurstLightningIntensity: Used for burst only lightning
///////////////////////////////////////////////////////////////////////////////
float CVfxLightningStrike::GetMainLightningIntensity()
{
	return Lerp(m_keyframeLerpMainIntensity,m_currentFrame.m_MainIntensity.GetXf(), m_currentFrame.m_MainIntensity.GetYf());
}


///////////////////////////////////////////////////////////////////////////////
//  Render: Renders a set of quads where each quad generated from each segment
///////////////////////////////////////////////////////////////////////////////
void CVfxLightningStrike::Render()
{
	//Adding a check for 0.0 for m_distance and earlying out as there is a case when we reach here with 0 distance
	//Added assert to keep track of that case. So now there's no more crash (because of the divide by Zero)
	vfxAssertf(m_distance > 0.0f, "Distance of lightning is set to 0.0f.");
	if(IsDead() || m_distance == 0.0f)
	{
		return;
	}

	// store some viewport and camera info
	const grcViewport* pCurrViewport = grcViewport::GetCurrent();
	Mat34V_ConstRef camMtx = pCurrViewport->GetCameraMtx();

	Vec3V vCamPos 				= camMtx.GetCol3();
	const float renderHeight = g_LightningDistance * m_Height / m_distance;

	grcViewport::SetCurrentWorldIdentity();
	grcWorldIdentity();

	u32 numToRender = (u32)m_segments.GetCount();
	Vector3 points[4];

	// check if we have some to render
	if (numToRender>0)
	{
		//Calculate the intensities based on keyframe. Keyframe Lerp values as used to decide where between the min and max curves it should take the values from.
		float intensityFade = Lerp(m_keyframeLerpMainIntensity, m_currentFrame.m_MainIntensity.GetXf(), m_currentFrame.m_MainIntensity.GetYf());
		float intensityBranchFade = Lerp(m_keyframeLerpBranchIntensity, m_currentFrame.m_BranchIntensity.GetXf(), m_currentFrame.m_BranchIntensity.GetYf());

		//Angular width to determine the ratio between the lightning horizontal stretch
		const float angularWidthFactor = renderHeight * pow(g_LightningSettings.GetStrikeSettings().GetAngularWidthFactor(), g_AngularWidthPowerMult);
		const u8 maxDistanceFromSource = (u8)pow((double)2, LIGHTNING_NUM_LEVEL_SUBDIVISIONS);
		const float animationLerp = (!m_AnimationTime)? 1.0f : rage::Clamp(m_age/ m_AnimationTime, 0.0f, 1.0f); 
		const float distanceFromSourceDelta = (1.0f / ((float)maxDistanceFromSource ));

		int pass = 0;
#if DEVICE_MSAA
		if (GRCDEVICE.GetMSAA())
		{
			pass = 1;
		}
#endif // DEVICE_MSAA

		if(m_vfxLightningStrikeShaderVars[VFXLIGHTNINGSTRIKE_SHADERVAR_PARAMS] != grcevNONE)
		{
			m_vfxLightningStrikeShader->SetVar(
				m_vfxLightningStrikeShaderVars[VFXLIGHTNINGSTRIKE_SHADERVAR_PARAMS],
				Vec2V(g_LightningSettings.GetStrikeSettings().GetLightningFadeWidthFactor(), g_LightningSettings.GetStrikeSettings().GetLightningFadeWidthFalloffExp()));
		}

		const float noiseAmplitudeFactor = 1.0f - Saturate((m_distance - g_LightningSettings.GetStrikeSettings().GetNoiseAmpMinDistLerp()) / (g_LightningSettings.GetStrikeSettings().GetNoiseAmpMaxDistLerp() - g_LightningSettings.GetStrikeSettings().GetNoiseAmpMinDistLerp()));
		if(m_vfxLightningStrikeShaderVars[VFXLIGHTNINGSTRIKE_SHADERVAR_NOISE_PARAMS] != grcevNONE)
		{
			m_vfxLightningStrikeShader->SetVar(
				m_vfxLightningStrikeShaderVars[VFXLIGHTNINGSTRIKE_SHADERVAR_NOISE_PARAMS],
				Vec4V(g_LightningSettings.GetStrikeSettings().GetNoiseTexScale(), g_LightningSettings.GetStrikeSettings().GetNoiseAmplitude() * noiseAmplitudeFactor, 0.0f, 0.0f));
		}

		if(m_vfxLightningStrikeShaderVars[VFXLIGHTNINGSTRIKE_SHADERVAR_NOISE_TEX] != grcevNONE)
		{
			m_vfxLightningStrikeShader->SetVar(
				m_vfxLightningStrikeShaderVars[VFXLIGHTNINGSTRIKE_SHADERVAR_NOISE_TEX], m_vfxLightningStrikeNoiseTex);
				
		}


		if(ShaderUtil::StartShader("VfxLightningStrike", m_vfxLightningStrikeShader, m_vfxLightningStrikeShaderTechnique, pass))
		{
			// render the lightning Segments whilst we still have some to render
			u32 currentSegmentIndex = 0;
			g_VfxLightningVtxBuffer.SetDrawModeType(drawTris);
			g_VfxLightningVtxBuffer.Begin();
			const int numVertsPerPrimitive = 6;
			while (numToRender)
			{
				// calc the size of this render batch
				int batchSize = Min(g_VfxLightningVtxBuffer.GetVtxBatchSize()/numVertsPerPrimitive, numToRender);
				// init the render batch
				g_VfxLightningVtxBuffer.Lock(numVertsPerPrimitive*batchSize);

				// loop through the segments filling up the batch
				s32 batchIndex = 0;
				while (batchIndex<batchSize)
				{
					const SLightningSegment& currentSegment = m_segments[currentSegmentIndex++];
					const Vector2& startPoint = currentSegment.startPos;
					const Vector2& endPoint = currentSegment.endPos;
					Vector3 actualStartPos, actualEndPos;
					Vector3 horizDir3D(m_horizDir.x, m_horizDir.y, 0.0f);

					Vector2 actualStartPos2D = m_horizDir;
					//Add Camera's horizontal position here. use ending point as zero and starting point as height
					actualStartPos2D.Rotate(startPoint.x * angularWidthFactor);
					const float distance = g_LightningDistance;

					//Calculate the actual start and end position based on distance and height
					actualStartPos.x = vCamPos.GetXf() + actualStartPos2D.x * distance;
					actualStartPos.y = vCamPos.GetYf() + actualStartPos2D.y * distance;
					actualStartPos.z = startPoint.y * renderHeight;

					Vector2 actualEndPos2D = m_horizDir;
					actualEndPos2D.Rotate(endPoint.x * angularWidthFactor);				
					actualEndPos.x = vCamPos.GetXf() + actualEndPos2D.x * distance;
					actualEndPos.y = vCamPos.GetYf() + actualEndPos2D.y * distance;
					actualEndPos.z = endPoint.y * renderHeight;

					//Calculate the right and forward vectors for sprite vertex positions
					Vector3 forwardDir = (actualEndPos - actualStartPos);
					forwardDir.Normalize();
					Vector3 rightDir;
					rightDir.Cross(forwardDir, horizDir3D);

					//Calculate animation lerps for animating the segments
					float currentSegmentAnimationLerp = (currentSegment.distanceFromSource * 1.0f) / (maxDistanceFromSource * 1.0f);
					float currentSegmentAnimationIntensity = rage::Clamp((animationLerp - currentSegmentAnimationLerp) / distanceFromSourceDelta, 0.0f, 1.0f);

					//Calculate center Pos of segment
					Vector3		vPos = (actualStartPos + actualEndPos) * 0.5f;
					//Forward direction size of segment
					float		size       = (actualStartPos.Dist(actualEndPos) + LIGHTNING_SEGEMENT_DELTA_SIZE) * 0.5f;
					float		intensity    = currentSegment.Intensity * m_intensityFlickerMult * intensityFade * currentSegmentAnimationIntensity * g_timeCycle.GetCurrRenderKeyframe().GetVar(TCVAR_VFXLIGHTNING_INTENSITY_MULT);
					if(!currentSegment.IsMainBoltSegment)
					{
						intensity *= intensityBranchFade;
					}

					// calc the render vectors
					//Right is determined by segment width
					const Vector3 vRight = rightDir * g_LightningSegmentWidth;
					const Vector3 vForward    = forwardDir * size;
						
					//Determine Points
					points[0] = vPos - vRight - vForward;
					points[1] = vPos - vRight + vForward;
					points[2] = vPos + vRight + vForward;
					points[3] = vPos + vRight - vForward;


					PushQuad(points, m_color, actualStartPos, actualEndPos, rightDir, currentSegment.width, intensity, currentSegment.startNoiseOffset, currentSegment.endNoiseOffset );

					// move onto the next space in the batch
					batchIndex++;
				}


				// end this render batch
				g_VfxLightningVtxBuffer.UnLock();

				// update the number left to render
				numToRender -= batchSize;
			}

			g_VfxLightningVtxBuffer.End();
			ShaderUtil::EndShader(m_vfxLightningStrikeShader);
		}
		RenderCorona();

		// reset the render state
		CShaderLib::SetGlobalEmissiveScale(1.0f,true);
	}
}


void CVfxLightningStrike::RenderCorona()
{

	// store some viewport and camera info
	const grcViewport* pCurrViewport = grcViewport::GetCurrent();
	Mat34V_ConstRef camMtx = pCurrViewport->GetCameraMtx();

	Vec3V vCamPos = camMtx.GetCol3();
	Vec3V vCamRight = camMtx.GetCol0();
	Vec3V vCamUp = -camMtx.GetCol1();
	//Vec3V vCamDir = -camMtx.GetCol2();
	CSprite2d lightningCoronaSprite;

	grcDrawMode drawMode = drawQuads;
	s32 vertsPerQuad = 4;

#if __D3D11
	drawMode = drawTriStrip;
	vertsPerQuad = 6;
#endif

	int pass = 0;


	const Vec4V coronaParams = Vec4V(
		g_coronas.GetOcclusionSizeScale(),
		g_coronas.GetOcclusionSizeMax(),
		0.0f,
		0.0f
		);

	const Vec4V coronaBaseValue = Vec4V(
		g_coronas.GetBaseValueR(),
		g_coronas.GetBaseValueG(),
		g_coronas.GetBaseValueB(),
		0.0f
		);

	grcStateBlock::SetStates(grcStateBlock::RS_NoBackfaceCull, CShaderLib::DSS_TestOnly_LessEqual_Invert, grcStateBlock::BS_Add);

	//Now render the base Corona to cover up the sharp lightning source

	lightningCoronaSprite.SetGeneralParams(RCC_VECTOR4(coronaParams), RCC_VECTOR4(coronaBaseValue));
	lightningCoronaSprite.BeginCustomList(CSprite2d::CS_CORONA_LIGHTNING, g_coronas.GetCoronaDiffuseTexture(), pass);

	const SLightningSegment& firstSegment = m_segments[0];

	const Vector2& startPoint = firstSegment.startPos;
	const Vector2& endPoint = firstSegment.endPos;
	Vector3 actualStartPos, actualEndPos;
	Vector3 horizDir3D(m_horizDir.x, m_horizDir.y, 0.0f);

	const float angularWidthFactor = m_Height * pow(g_LightningSettings.GetStrikeSettings().GetAngularWidthFactor(), g_AngularWidthPowerMult);
	//Calculate the actual start and end position based on distance and height
	Vector2 actualStartPos2D = m_horizDir;
	actualStartPos2D.Rotate(startPoint.x * angularWidthFactor);
	const float distance = g_LightningDistance;
	const float renderHeight = g_LightningDistance * m_Height / m_distance;
	actualStartPos.x = vCamPos.GetXf() + actualStartPos2D.x * distance;
	actualStartPos.y = vCamPos.GetYf() + actualStartPos2D.y * distance;
	actualStartPos.z = startPoint.y * renderHeight;


	Vector2 actualEndPos2D = m_horizDir;
	actualEndPos2D.Rotate(endPoint.x * angularWidthFactor);				
	actualEndPos.x = vCamPos.GetXf() + actualEndPos2D.x * distance;
	actualEndPos.y = vCamPos.GetYf() + actualEndPos2D.y * distance;
	actualEndPos.z = endPoint.y * renderHeight;
	
	Vec3V		vPos = RCC_VEC3V(actualStartPos);	
	//Vec3V vCamToCorona = vPos - vCamPos;				
	ScalarV		size       = ScalarVFromF32(g_LightningSettings.GetStrikeSettings().GetBaseCoronaSize());
	//Calculate the intensities based on keyframe. Keyframe Lerp values as used to decide where between the min and max curves it should take the values from.
	float intensityFade = Lerp(m_keyframeLerpMainIntensity, m_currentFrame.m_MainIntensity.GetXf(), m_currentFrame.m_MainIntensity.GetYf());
	float		intensity    = firstSegment.Intensity * m_intensityFlickerMult * intensityFade * g_LightningSettings.GetStrikeSettings().GetCoronaIntensityMult();

	// calc the render vectors
	Vec3V vRight = vCamRight * size;
	Vec3V vUp    = vCamUp * size;

	Vec3V vCorner00 = vPos - vRight - vUp;
	Vec3V vCorner01 = vPos - vRight + vUp;
	Vec3V vCorner10 = vPos + vRight - vUp;
	Vec3V vCorner11 = vPos + vRight + vUp;

	float centreX = vPos.GetXf(); // technically, these are only necessary for RM_DEFAULT
	float centreY = vPos.GetYf();
	float centreZ = vPos.GetZf();

	// init the render batch
	grcBegin(drawMode, vertsPerQuad);

#if __D3D11
	m_color.SetAlpha(255);
	grcVertex(VEC3V_ARGS(vCorner01), centreX, centreY, centreZ, m_color, 0.0f, intensity); //degenerate vert
	grcVertex(VEC3V_ARGS(vCorner01), centreX, centreY, centreZ, m_color, 0.0f, intensity);
	grcVertex(VEC3V_ARGS(vCorner11), centreX, centreY, centreZ, m_color, 1.0f, intensity);

	m_color.SetAlpha(0);
	grcVertex(VEC3V_ARGS(vCorner00), centreX, centreY, centreZ, m_color, 0.0f, intensity);
	grcVertex(VEC3V_ARGS(vCorner10), centreX, centreY, centreZ, m_color, 1.0f, intensity);
	grcVertex(VEC3V_ARGS(vCorner10), centreX, centreY, centreZ, m_color, 1.0f, intensity); //degenerate vert
#else
	m_color.SetAlpha(255);
	grcVertex(VEC3V_ARGS(vCorner01), centreX, centreY, centreZ, m_color, 0.0f, intensity);
	grcVertex(VEC3V_ARGS(vCorner11), centreX, centreY, centreZ, m_color, 1.0f, intensity);

	m_color.SetAlpha(0);
	grcVertex(VEC3V_ARGS(vCorner10), centreX, centreY, centreZ, m_color, 1.0f, intensity);
	grcVertex(VEC3V_ARGS(vCorner00), centreX, centreY, centreZ, m_color, 0.0f, intensity);
#endif

	grcEnd();
	// shut down the custom renderer
	lightningCoronaSprite.EndCustomList();

	//Now Render Flare

	// use pass with no near fade, otherwise flares won't
	// get occluded when off-screen
	pass = 1;

	grcStateBlock::SetStates(grcStateBlock::RS_NoBackfaceCull, CShaderLib::DSS_TestOnly_LessEqual_Invert, grcStateBlock::BS_Add);
	lightningCoronaSprite.SetGeneralParams(RCC_VECTOR4(coronaParams), VEC4V_TO_VECTOR4(Vec4V(V_ZERO)));
	lightningCoronaSprite.BeginCustomList(CSprite2d::CS_CORONA_LIGHTNING, g_coronas.GetAnimorphicFlareDiffuseTexture(), pass);

	Mat44V_ConstRef projMtx = pCurrViewport->GetCompositeMtx();

	float	  flareMinAngleToFadeIn	= VFXLIGHTNING_DEG2RAD(g_coronas.GetFlareMinAngleDegToFadeIn());

	// init the render batch
	grcBegin(drawMode, vertsPerQuad);
	//Vec3V	  vCoronaToCam	= Normalize(vCamPos - vPos);
	float	  camCoronaAngleMult	= 1.0f;
	Color32   col          	= m_color;

	col.Setf(col.GetRedf() * g_coronas.GetFlareColShiftR(), col.GetGreenf()*g_coronas.GetFlareColShiftG(), col.GetBluef()*g_coronas.GetFlareColShiftB());

	// Convert to NDC space
	Vec3V vProjPos = TransformProjective(projMtx, vPos);
	// Compute size scalar
	ScalarV vSizeScalar = Saturate(Abs(Abs(vProjPos.GetX())-Abs(vProjPos.GetY())));
	vSizeScalar*=vSizeScalar;

	ScalarV vHSizeScaler = vSizeScalar;
	ScalarV vVSizeScaler = ScalarV(V_ZERO);

	// size scalers [0, 1] -> [1, FLARES_SCREEN_CENTER_OFFSET_SIZE_MULT]
	vHSizeScaler = ScalarV(V_ONE) + vHSizeScaler*(ScalarVFromF32(g_coronas.GetFlareScreenCenterOffsetSizeMult())-ScalarV(V_ONE));
	vVSizeScaler = ScalarV(V_ONE) + vVSizeScaler*(ScalarVFromF32(g_coronas.GetFlareScreenCenterOffsetVerticalSizeMult())-ScalarV(V_ONE));

	// make flares fade based on FLARES_MIN_ANGLE_DEG_TO_FADE_IN
	const float angleBasedMult = Clamp<float>((camCoronaAngleMult-flareMinAngleToFadeIn), 0.0f, 1.0f);
	intensity *= angleBasedMult;

	// calc the render vectors
	vRight = vCamRight * size * ScalarVFromF32(g_coronas.GetFlareCoronaSizeRatio()) * vHSizeScaler;
	vUp    = vCamUp * size* ScalarVFromF32(g_coronas.GetFlareCoronaSizeRatio()) * vVSizeScaler;

	vCorner00 = vPos - vRight - vUp;
	vCorner01 = vPos - vRight + vUp;
	vCorner10 = vPos + vRight - vUp;
	vCorner11 = vPos + vRight + vUp;

	centreX = vPos.GetXf(); // technically, these are only necessary for RM_DEFAULT
	centreY = vPos.GetYf();
	centreZ = vPos.GetZf();

	// send the verts 
	// NOTE: we send the hdr multiplier in the v tex coord slot (so it can be greater than 1.0)
	//       we therefore have to send the v tex coord in the alpha slot and we presume that the actual alpha is always 255

#if __D3D11
	col.SetAlpha(255);
	grcVertex(VEC3V_ARGS(vCorner01), centreX, centreY, centreZ, col, 0.0f, intensity); //degenerate vert
	grcVertex(VEC3V_ARGS(vCorner01), centreX, centreY, centreZ, col, 0.0f, intensity);
	grcVertex(VEC3V_ARGS(vCorner11), centreX, centreY, centreZ, col, 1.0f, intensity);

	col.SetAlpha(0);
	grcVertex(VEC3V_ARGS(vCorner00), centreX, centreY, centreZ, col, 0.0f, intensity);
	grcVertex(VEC3V_ARGS(vCorner10), centreX, centreY, centreZ, col, 1.0f, intensity);
	grcVertex(VEC3V_ARGS(vCorner10), centreX, centreY, centreZ, col, 1.0f, intensity); //degenerate vert
#else
	col.SetAlpha(255);
	grcVertex(VEC3V_ARGS(vCorner01), centreX, centreY, centreZ, col, 0.0f, intensity);
	grcVertex(VEC3V_ARGS(vCorner11), centreX, centreY, centreZ, col, 1.0f, intensity);

	col.SetAlpha(0);
	grcVertex(VEC3V_ARGS(vCorner10), centreX, centreY, centreZ, col, 1.0f, intensity);
	grcVertex(VEC3V_ARGS(vCorner00), centreX, centreY, centreZ, col, 0.0f, intensity);
#endif


	grcEnd();

	// shut down the custom renderer
	lightningCoronaSprite.EndCustomList();

}
///////////////////////////////////////////////////////////////////////////////
//  GetLightSourcePositionForClouds: Calculates the light source position of
//									the lightning specifically for lightning up
//									the clouds
///////////////////////////////////////////////////////////////////////////////
Vec3V_Out CVfxLightningStrike::GetLightSourcePositionForClouds() const
{
	Vec3V position = Vec3V(V_ZERO);

	const Vector2& startPoint = m_segments[0].startPos;
	Vector3 actualStartPosDir;
	Vector3 horizDir3D(m_horizDir.x, m_horizDir.y, 0.0f);

	Vector2 actualStartPos2D = m_horizDir;
	//Figure out starting position based on height
	const int cloudHatIndex = CLOUDHATMGR? CLOUDHATMGR->GetActiveCloudHatIndex():-1;
	if(cloudHatIndex > -1)
	{
		actualStartPosDir.x =  actualStartPos2D.x * m_distance;
		actualStartPosDir.y = actualStartPos2D.y * m_distance;
		actualStartPosDir.z = startPoint.y * m_Height;
		position = RCC_VEC3V(actualStartPosDir);
		actualStartPosDir.Normalize();

		//Move the light back a bit so that it is place in front of the clouds for proper
		// lighting
		actualStartPosDir *= g_LightningSettings.GetStrikeSettings().GetCloudLightDeltaPos();
		Vec3V delta = RCC_VEC3V(actualStartPosDir);
		position += delta;
	}

	return position;
}

///////////////////////////////////////////////////////////////////////////////
//  CLASS CVfxLightningManager
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  Constructor
///////////////////////////////////////////////////////////////////////////////
CVfxLightningManager::CVfxLightningManager()
	: m_lightningChance(0.0f)
	, m_forceLightningFlash(false)
	, m_lightningFlashAmount(0.0f)
	, m_lightningStartTime(0)
	, m_CloudBurstRootOrbitPos(0.0f, 0.0f)
	, m_DirectionalBurstRootOrbitPos(0.0f, 0.0f)
	, m_IsCloudLightningActive(false)
#if __BANK
	, m_overrideLightning(-0.01f)
#endif
	, m_numCloudLights(0)
{
	m_lightningFlashDirection = Vec3V(0.577f, 0.577f, 0.577f);
	m_lightningFlashDirection = Normalize(m_lightningFlashDirection);
	m_currentLightningType[VFXLIGHTNING_UPDATE_BUFFER_ID] = LIGHTNING_TYPE_NONE;
	m_currentLightningType[VFXLIGHTNING_RENDER_BUFFER_ID] = LIGHTNING_TYPE_NONE;

}

///////////////////////////////////////////////////////////////////////////////
//  Destructor
///////////////////////////////////////////////////////////////////////////////
CVfxLightningManager::~CVfxLightningManager()
{

}

///////////////////////////////////////////////////////////////////////////////
//  InitDLCCommands
///////////////////////////////////////////////////////////////////////////////
void CVfxLightningManager::InitDLCCommands()
{
	DLC_REGISTER_EXTERNAL(dlCmdSetupVfxLightningFrameInfo);
	DLC_REGISTER_EXTERNAL(dlCmdClearVfxLightningFrameInfo);
}

///////////////////////////////////////////////////////////////////////////////
//  Init
///////////////////////////////////////////////////////////////////////////////
void CVfxLightningManager::Init(unsigned initMode)
{	
	if(initMode == INIT_CORE)
	{
		//Load Lightning settings
		g_LightningSettings.Load(g_LightningSettingsFile);
		InitShaders();
		g_VfxLightningVtxBuffer.Init(drawTris, VFX_LIGHTNING_VTX_CNT);
		
	}
	m_lightningStartTime = 0;
	m_lightningChance = 0.0;
	m_lightningFlashAmount = 0.0f;

	//Directional Bursts and Cloud Bursts
	m_DirectionalBurstSequence.m_BlastCount = 0;
	for(int i=0;i<MAX_NUM_CLOUD_LIGHTS;i++)
		m_CloudBurstSequence[i].m_BlastCount = 0;

	m_lightningFlashDirection = Vec3V(0.577f, 0.577f, 0.577f);
	m_lightningFlashDirection = Normalize(m_lightningFlashDirection);
		
}

///////////////////////////////////////////////////////////////////////////////
//  Init
///////////////////////////////////////////////////////////////////////////////
void CVfxLightningManager::InitShaders()
{
	CVfxLightningStrike::InitShader();

}

///////////////////////////////////////////////////////////////////////////////
//  ShutdownShaders
///////////////////////////////////////////////////////////////////////////////
void CVfxLightningManager::ShutdownShaders()
{
	CVfxLightningStrike::ShutdownShader();
}

///////////////////////////////////////////////////////////////////////////////
//  Shutdown
///////////////////////////////////////////////////////////////////////////////
void CVfxLightningManager::Shutdown(unsigned shutdownMode)
{
	if(shutdownMode == SHUTDOWN_CORE)
	{
		g_VfxLightningVtxBuffer.Shutdown();
		ShutdownShaders();
	}
}

///////////////////////////////////////////////////////////////////////////////
//  SetupRenderthreadFrameInfo
///////////////////////////////////////////////////////////////////////////////

void CVfxLightningManager::SetupRenderthreadFrameInfo()
{
	DLC(dlCmdSetupVfxLightningFrameInfo, ());
}

///////////////////////////////////////////////////////////////////////////////
//  ClearRenderthreadFrameInfo
///////////////////////////////////////////////////////////////////////////////

void CVfxLightningManager::ClearRenderthreadFrameInfo()
{
	DLC(dlCmdClearVfxLightningFrameInfo, ());
}

///////////////////////////////////////////////////////////////////////////////
//  AddLightning
///////////////////////////////////////////////////////////////////////////////
void CVfxLightningManager::AddLightningStrike()
{
	REPLAY_ONLY(if(CReplayMgr::IsEditModeActive() == false))
		m_lightningStrike.Init();

	m_lightningFlashDirection = m_lightningStrike.GetLightSourcePositionForClouds();
	m_lightningFlashDirection = NormalizeSafe(m_lightningFlashDirection, Vec3V(V_Y_AXIS_WZERO));
	if(BANK_ONLY(g_LightningUseExposureReset &&) m_lightningStrike.GetDistance() < g_LightningSettings.GetStrikeSettings().GetMaxDistanceForExposureReset())
	{
		PostFX::ResetAdaptedLuminance();
	}

	if(m_lightningStrike.IsBurstLightning())
	{
		//specify that lightning uses directional Flash
		m_lightningFlashAmount = 1.0f;
		SetLightningTypeUpdate(LIGHTNING_TYPE_DIRECTIONAL_BURST_STRIKE);
	}
	else
	{
		SetLightningTypeUpdate(LIGHTNING_TYPE_STRIKE);
	}
}

void CVfxLightningManager::AddLightningStrike(CVfxLightningStrike::SLightningSettings &settings)
{
	REPLAY_ONLY(if(CReplayMgr::IsEditModeActive() == false))
		m_lightningStrike.Init(settings);
}

///////////////////////////////////////////////////////////////////////////////
//  SetLightSourcesForClouds - called during UpdateSafe()
///////////////////////////////////////////////////////////////////////////////
void CVfxLightningManager::SetLightSourcesForClouds()
{
	if(GetLightningTypeUpdate() == LIGHTNING_TYPE_CLOUD_BURST || GetLightningTypeUpdate() == LIGHTNING_TYPE_DIRECTIONAL_BURST_STRIKE || GetLightningTypeUpdate() == LIGHTNING_TYPE_STRIKE)
	{
		if(CLOUDHATMGR)
		{
			CLOUDHATMGR->SetCloudLights(m_lightSources);
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  PerformTimeCycleOverride - overrides time cycle settings during a lightning
///////////////////////////////////////////////////////////////////////////////
void CVfxLightningManager::PerformTimeCycleOverride(CTimeCycle::frameInfo& currentFrameInfo, float alpha)
{	
#if __BANK
	if(!g_UseTimeCycleMod)
	{
		return;
	}
#endif

	if(GetLightningTypeUpdate() > LIGHTNING_TYPE_NONE && m_lightningFlashAmount > 0.0f)
	{
		bool bPerformOverride = false;
		bool bPerformSunSpriteOverride = false;
		const tcModifier *mod = g_timeCycle.FindModifier(g_LightningSettings.GetTimeCycleSettings().GetLightningTCMod(GetLightningTypeUpdate()), NULL);
		if(mod)
		{
			mod->Apply(currentFrameInfo.m_keyframe, alpha);
			switch(GetLightningTypeUpdate())
			{
			case LIGHTNING_TYPE_STRIKE:
			case LIGHTNING_TYPE_DIRECTIONAL_BURST_STRIKE:
				bPerformSunSpriteOverride = true;
			case LIGHTNING_TYPE_DIRECTIONAL_BURST:
				bPerformOverride = true;
				break;
			case LIGHTNING_TYPE_CLOUD_BURST:
				break;
			default:
				break;
			}

		}

		if(bPerformOverride)
		{
			currentFrameInfo.m_directionalLightIntensity *= m_lightningFlashAmount;
			currentFrameInfo.m_vDirectionalLightDir = m_lightningFlashDirection;
			if(bPerformSunSpriteOverride)
			{
				g_timeCycle.SetLightningDirection(m_lightningFlashDirection);
				g_timeCycle.SetLightningColor(g_LightningSettings.GetStrikeSettings().GetBaseColor());
				BANK_ONLY(if(g_FlickerDirectionalColor))
				{
					const float appliedFogColor = m_lightningStrike.GetMainFlickerIntensity();
					const Color32 flickerFogColor(appliedFogColor,appliedFogColor,appliedFogColor,1.0f);
					g_timeCycle.SetLightningFogColor(g_LightningSettings.GetStrikeSettings().GetFogColor()* flickerFogColor);
				}
#if __BANK
				else
				{
					g_timeCycle.SetLightningFogColor(g_LightningSettings.GetStrikeSettings().GetFogColor());
				}
#endif
				g_timeCycle.SetLightningOverride(true);
			}
		}
	}
	else
	{
		g_timeCycle.SetLightningOverride(false);
	}
}

///////////////////////////////////////////////////////////////////////////////
//  UpdateDirectionalBurstSequence - Used for updating the directional flashes
///////////////////////////////////////////////////////////////////////////////
void CVfxLightningManager::UpdateDirectionalBurstSequence(float deltaTime)
{
	//Get Current blast
	LightningBlast & curblast = m_DirectionalBurstSequence.m_LightningBlasts[m_DirectionalBurstSequence.m_BlastCount-1];

	//Use sin wave for ramp up and ramp down
	float rampScalar = sin(curblast.m_Timer*180.0f*DtoR);
	float lightIntensity = curblast.m_Intensity * rampScalar;
	g_WeatherAudioEntity.TriggerThunderBurst(lightIntensity);
	//Calculate direction for flash
	Vector4 orbit = Vector4(m_DirectionalBurstRootOrbitPos.x+curblast.m_OrbitPos.x, m_DirectionalBurstRootOrbitPos.y+curblast.m_OrbitPos.y, 0.000000000, 0.000000000);
	
	curblast.m_Timer+=deltaTime*curblast.m_Duration;
	//Reduce blast count if timer for current one is exceeded duration
	if(curblast.m_Timer>=1.0f)
		m_DirectionalBurstSequence.m_BlastCount-=1;

	//Get Direction of light flash
	Vector3 pos(0.0f, 1.0f, 0.0f);
	pos.RotateX(orbit.y*DtoR);
	pos.RotateZ(orbit.x*DtoR);
	pos.Normalize();

	// Vector4 lightColor = Vector4(1.0f, 1.0f, 1.0f, lightIntensity);
	m_lightningFlashDirection = RCC_VEC3V(pos);
	m_lightningFlashAmount = lightIntensity;

}
///////////////////////////////////////////////////////////////////////////////
//  UpdateCloudLightningSequence
///////////////////////////////////////////////////////////////////////////////
void CVfxLightningManager::UpdateCloudLightningSequence(float deltaTime, int index)
{

	m_CloudBurstSequence[index].m_Delay -= deltaTime;
	if(m_CloudBurstSequence[index].m_Delay > 0.0f)
		return;

	if(gVpMan.GetCurrentViewport())
	{
		const grcViewport& grcVp = gVpMan.GetCurrentViewport()->GetGrcViewport();
		Vector3 camPos = VEC3V_TO_VECTOR3(grcVp.GetCameraPosition());
		camPos.z = 0.0f;

		//Get Current blast
		LightningBlast & curblast = m_CloudBurstSequence[index].m_LightningBlasts[m_CloudBurstSequence[index].m_BlastCount-1];
		Vector2 localRoot(m_CloudBurstSequence[index].m_OrbitRoot);

		float rampScalar = sin(curblast.m_Timer*180.0f*DtoR);

		float lightIntensity = curblast.m_Intensity * rampScalar *  g_timeCycle.GetCurrUpdateKeyframe().GetVar(TCVAR_VFXLIGHTNING_INTENSITY_MULT);
		float size = curblast.m_Size * rampScalar;
		Vector4 orbit = Vector4(m_CloudBurstRootOrbitPos.x+curblast.m_OrbitPos.x+localRoot.x, m_CloudBurstRootOrbitPos.y+curblast.m_OrbitPos.y+localRoot.y, 0.000000000, 0.000000000);

		curblast.m_Timer += deltaTime*curblast.m_Duration;
		//Reduce blast count if timer for current one is exceeded duration
		if(curblast.m_Timer>=1.0f)
			m_CloudBurstSequence[index].m_BlastCount-=1;

		//Calculate correct position based on function

		Vector3 pos(0.0f, g_LightningSettings.GetCloudBurstSettings().GetLightDistance(), 0.0f);

		pos.RotateX(orbit.y*DtoR);
		pos.RotateZ(orbit.x*DtoR);
		Vector3 direction = pos;
		direction.Normalize();
		pos += direction * g_LightningSettings.GetCloudBurstSettings().GetLightDeltaPos();
		pos += camPos;

		f32 audioIntensity = ClampRange(lightIntensity,g_LightningSettings.GetCloudBurstSettings().GetBurstMinIntensity(),g_LightningSettings.GetCloudBurstSettings().GetBurstMaxIntensity());
		g_WeatherAudioEntity.AddCLoudBurstSpikeEvent(pos,audioIntensity);
		//Set Light details
		Color32 lightColor = g_LightningSettings.GetCloudBurstSettings().GetLightColor();		
		m_lightSources[index].SetPosition(pos);
		m_lightSources[index].SetRadius(size);
		m_lightSources[index].SetColor(Vector3(lightColor.GetRed(), lightColor.GetGreen(), lightColor.GetBlue()));
		m_lightSources[index].SetIntensity(lightIntensity);
		m_lightSources[index].SetFalloffExponent(g_LightningSettings.GetCloudBurstSettings().GetLightExponentFallOff());

	}
	
}

///////////////////////////////////////////////////////////////////////////////
//  CreateDirectionalBurstSequence - Initializes the directional burst lightning sequence
///////////////////////////////////////////////////////////////////////////////

void CVfxLightningManager::CreateDirectionalBurstSequence()
{
#if GTA_REPLAY
	if(CReplayMgr::IsEditModeActive())
		return;
#endif // GTA_REPLAY

	if(gVpMan.GetCurrentViewport())
	{
		// const grcViewport& grcVp = gVpMan.GetCurrentViewport()->GetGrcViewport();
		// const Vector3 camPos = VEC3V_TO_VECTOR3(grcVp.GetCameraPosition());
		//Root position of the lightning 
		m_DirectionalBurstRootOrbitPos.x = g_LightningSettings.GetDirectionalBurstSettings().GetRootOrbitXRandom();
		m_DirectionalBurstRootOrbitPos.y = g_LightningSettings.GetDirectionalBurstSettings().GetRootOrbitYRandom();

		//Generate the flashing sequence 
		Assertf(g_LightningSettings.GetDirectionalBurstSettings().GetBurstSequenceCount()>=1,"Uninitalized Lightning system");
		m_DirectionalBurstSequence.m_BlastCount = g_LightningSettings.GetDirectionalBurstSettings().GetBurstSequenceRandomCount();
		//float avgIntensity = 0;
		//Set properties for each blast
		for(int i=0;i<m_DirectionalBurstSequence.m_BlastCount;i++)
		{
			m_DirectionalBurstSequence.m_LightningBlasts[i].m_OrbitPos.x = g_LightningSettings.GetDirectionalBurstSettings().GetBurstSeqOrbitXRandom();
			m_DirectionalBurstSequence.m_LightningBlasts[i].m_OrbitPos.y = g_LightningSettings.GetDirectionalBurstSettings().GetBurstSeqOrbitYRandom();
			m_DirectionalBurstSequence.m_LightningBlasts[i].m_Intensity = g_LightningSettings.GetDirectionalBurstSettings().GetBurstIntensityRandom();
			m_DirectionalBurstSequence.m_LightningBlasts[i].m_Duration = 1.0f/g_LightningSettings.GetDirectionalBurstSettings().GetBurstDurationRandom();
			m_DirectionalBurstSequence.m_LightningBlasts[i].m_Timer = 0.0f;
			//avgIntensity += m_DirectionalBurstSequence.m_LightningBlasts[i].m_Intensity;
		}

		Vector3 direction(0.0f, 1.0f, 0.0f);;
		direction.RotateX(m_DirectionalBurstRootOrbitPos.y*DtoR);
		direction.RotateZ(m_DirectionalBurstRootOrbitPos.x*DtoR);
		m_lightningFlashDirection = RCC_VEC3V(direction);
		m_lightningFlashDirection = Normalize(m_lightningFlashDirection);
		m_lightningFlashAmount = 1.0f;
	}

}

///////////////////////////////////////////////////////////////////////////////
//  CreateCloudLightningSequence - Initializes the cloud burst lightning sequence
///////////////////////////////////////////////////////////////////////////////
void CVfxLightningManager::CreateCloudLightningSequence(float yaw, float pitch,  bool bUseStrikeSettings)
{
#if GTA_REPLAY
	if(CReplayMgr::IsEditModeActive())
		return;
#endif // GTA_REPLAY

	const VfxLightningCloudBurstCommonSettings& cloudBurstCommonSettings = bUseStrikeSettings? g_LightningSettings.GetStrikeSettings().GetCloudBurstCommonSettings()
		: g_LightningSettings.GetCloudBurstSettings().GetCloudBurstCommonSettings();

	m_CloudBurstRootOrbitPos.Set(yaw, pitch);

	//Determine the number of sequences (minimum is 1 and max is MAX_NUM_CLOUD_LIGHTS - 1)
	m_numCloudLights =  fwRandom::GetRandomNumber() % (MAX_NUM_CLOUD_LIGHTS - 1) + 1;	
	for(int s=0;s<m_numCloudLights;s++)
	{
		m_CloudBurstSequence[s].m_OrbitRoot.x = cloudBurstCommonSettings.GetLocalOrbitXRandom();
		m_CloudBurstSequence[s].m_OrbitRoot.y = cloudBurstCommonSettings.GetLocalOrbitYRandom();

		//Determine the delay 
		m_CloudBurstSequence[s].m_Delay = cloudBurstCommonSettings.GetDelayRandom();

		//Determine the number of flashes 
		Assertf(cloudBurstCommonSettings.GetBurstSequenceCount()>=1,"Uninitalized Lightning system");
		int cnt = cloudBurstCommonSettings.GetBurstSequenceRandomCount();

		//Generate the flashes values
		for(int i=0;i<cnt;i++)
		{
			m_CloudBurstSequence[s].m_LightningBlasts[i].m_Size = cloudBurstCommonSettings.GetSizeRandom();
			m_CloudBurstSequence[s].m_LightningBlasts[i].m_OrbitPos.x = cloudBurstCommonSettings.GetBurstSeqOrbitXRandom();
			m_CloudBurstSequence[s].m_LightningBlasts[i].m_OrbitPos.y = cloudBurstCommonSettings.GetBurstSeqOrbitYRandom();
			m_CloudBurstSequence[s].m_LightningBlasts[i].m_Intensity = g_LightningSettings.GetCloudBurstSettings().GetBurstIntensityRandom();
			m_CloudBurstSequence[s].m_LightningBlasts[i].m_Duration = 1.0f/cloudBurstCommonSettings.GetBurstDurationRandom();
			m_CloudBurstSequence[s].m_LightningBlasts[i].m_Timer = 0.0f;
		}
		m_CloudBurstSequence[s].m_BlastCount = cnt;

	}
	m_CloudBurstSequence[0].m_Delay = 0.0f;
}

///////////////////////////////////////////////////////////////////////////////
//  CreateCloudLightningSequence - Initializes the cloud burst lightning sequence
///////////////////////////////////////////////////////////////////////////////
void CVfxLightningManager::CreateCloudLightningSequence()
{
#if GTA_REPLAY
	if(CReplayMgr::IsEditModeActive())
		return;
#endif // GTA_REPLAY

	m_lightningFlashAmount = 0.0f;

	if(gVpMan.GetCurrentViewport())
	{
		const grcViewport& grcVp = gVpMan.GetCurrentViewport()->GetGrcViewport();

		ScalarV xv = grcVp.GetCameraMtx().GetCol2().GetX();
		ScalarV yv = grcVp.GetCameraMtx().GetCol2().GetY();

		ScalarV ov = Arctan2(xv, -yv);
		float orbitCenter = ov.Getf()*RtoD;
		//Get random angle change based on current view
		orbitCenter += g_LightningSettings.GetCloudBurstSettings().GetCloudBurstCommonSettings().GetRootOrbitXRandom();
		float pitch = g_LightningSettings.GetCloudBurstSettings().GetCloudBurstCommonSettings().GetRootOrbitYRandom();
		// const Vector3 camPos = VEC3V_TO_VECTOR3(grcVp.GetCameraPosition());
		CreateCloudLightningSequence(orbitCenter, pitch, false);

	}

}
///////////////////////////////////////////////////////////////////////////////
//  Update
///////////////////////////////////////////////////////////////////////////////
void CVfxLightningManager::Update(float deltaTime)
{

	//Add a new lightning based on probability
	float lightningChance = m_lightningChance;
#if __BANK
	if (m_overrideLightning>=0.0f)
	{
		lightningChance = m_overrideLightning;
	}
#endif

	if(g_timeCycle.GetCurrUpdateKeyframe().GetVar(TCVAR_VFXLIGHTNING_VISIBILITY) < 0.5f)
	{
		lightningChance = 0.0f;
	}

	//Reset current light sources to black light
	for(int i=0; i<MAX_NUM_CLOUD_LIGHTS; i++)
	{	
		m_lightSources[i] = Lights::m_blackLight;
	}
	m_IsCloudLightningActive = false;

	bool bIsPlayerSwitchActive = g_PlayerSwitch.IsActive() && (g_PlayerSwitch.GetSwitchType() != CPlayerSwitchInterface::SWITCH_TYPE_SHORT);
	if(GetLightningTypeUpdate() == LIGHTNING_TYPE_NONE REPLAY_ONLY(&& CReplayMgr::IsEditModeActive() == false)) //no lightning currently
	{
		// check if there's a chance of lightning
		if ((lightningChance>0.99f || m_forceLightningFlash) && !bIsPlayerSwitchActive)
		{
			// check if we should start one
			if (((fwRandom::GetRandomNumber()&65535)< g_LightningSettings.GetLightningOccurranceChance() || m_forceLightningFlash))
			{
				//yes we should

				const int cloudHatIndex = CLOUDHATMGR? CLOUDHATMGR->GetActiveCloudHatIndex():-1;
				bool bValidCloudHat = CLOUDHATMGR && CLOUDHATMGR->IsCloudHatIndexValid(cloudHatIndex) && CLOUDHATMGR->GetCloudHat(cloudHatIndex).IsAnyLayerStreamedIn();
				//Choose a lightning type
				//Use Directional Burst only if there is no cloud hat
#if __BANK
				if(g_forceLightningType > LIGHTNING_TYPE_NONE)
				{
					SetLightningTypeUpdate(g_forceLightningType);
					if(g_forceLightningType > LIGHTNING_TYPE_DIRECTIONAL_BURST)
					{
						if(!bValidCloudHat)
						{
							Assertf(bValidCloudHat, "Cannot use Cloud Bursts or strikes when there is no Cloud Hat loaded. Please load Cloud hat and try again");
							SetLightningTypeUpdate(LIGHTNING_TYPE_DIRECTIONAL_BURST);

						}
					}
				}
				else
#endif			
				{
					//If cloud hat is not available, then force type to directional burst only
					if(bValidCloudHat)// && !Water::IsCameraUnderwater())
					{
						eLightningType maxAvailableLightningType = LIGHTNING_TYPE_TOTAL;
						if(gVpMan.GetCurrentViewport())
						{
							const grcViewport& grcVp = gVpMan.GetCurrentViewport()->GetGrcViewport();
							const Vector3 camPos = VEC3V_TO_VECTOR3(grcVp.GetCameraPosition());
							//Skip strike lightnings over certain height because it looks bad in aerial views
							if(camPos.z > g_LightningSettings.GetStrikeSettings().GetMaxHeightForStrikes())
							{
								//have to add one because the randomization does not contain the last choice
								maxAvailableLightningType = LIGHTNING_TYPE_STRIKE;
							}
						}
						SetLightningTypeUpdate((eLightningType)fwRandom::GetRandomNumberInRange(LIGHTNING_TYPE_DIRECTIONAL_BURST, maxAvailableLightningType));
					}
					else
					{
						SetLightningTypeUpdate(LIGHTNING_TYPE_DIRECTIONAL_BURST);
					}
					
				}

				CVfxLightningStrike::SLightningSettings settings; 

				m_lightningStartTime = fwTimer::GetTimeInMilliseconds();
				//Create the lightning
				switch(GetLightningTypeUpdate())
				{
				case LIGHTNING_TYPE_CLOUD_BURST:
					CreateCloudLightningSequence();
					break;
				case LIGHTNING_TYPE_DIRECTIONAL_BURST:
					CreateDirectionalBurstSequence();
					PostFX::ResetAdaptedLuminance();
					break;
				case LIGHTNING_TYPE_STRIKE:
				case LIGHTNING_TYPE_DIRECTIONAL_BURST_STRIKE:
					AddLightningStrike();		
					CreateCloudLightningSequence(m_lightningStrike.GetYaw(), m_lightningStrike.GetPitch(), true);
					break;

				default:
					break;
				}

#if GTA_REPLAY
				RecordReplayLightning();
#endif // GTA_REPLAY

			}
			else
			{
				// don't start one yet - reset
				m_lightningFlashAmount = 0.0f;
			}
		}
	}
	else //currently in a lightning burst.. update the current system
	{
		bool bFinished = false;
		//Lightning strike updates
		if(GetLightningTypeUpdate() == LIGHTNING_TYPE_STRIKE || GetLightningTypeUpdate() == LIGHTNING_TYPE_DIRECTIONAL_BURST_STRIKE)
		{	
			bool bStrikeFinished = false;
			if(m_lightningStrike.IsDead() || bIsPlayerSwitchActive)
			{
				m_lightningStrike.Shutdown();
				bStrikeFinished = true;
				m_lightningFlashAmount = 0.0f;			
			}
			else 
			{
				m_lightningStrike.Update(deltaTime);
				if(GetLightningTypeUpdate() == LIGHTNING_TYPE_DIRECTIONAL_BURST_STRIKE)
				{
					m_lightningFlashAmount = m_lightningStrike.GetBurstLightningIntensity();
				}
				else
				{
					m_lightningFlashAmount = m_lightningStrike.GetMainLightningIntensity();
				}

				BANK_ONLY(if(g_FlickerDirectionalColor))
				{
					m_lightningFlashAmount *= m_lightningStrike.GetMainFlickerIntensity();
				}

			}
			//Set the light source
			if(gVpMan.GetCurrentViewport())
			{
				const grcViewport& currViewport = gVpMan.GetCurrentViewport()->GetGrcViewport();
				Mat34V_ConstRef camMtx = currViewport.GetCameraMtx();
				Vec3V vCamPos 				= camMtx.GetCol3();
				vCamPos.SetZ(ScalarV(V_ZERO));
				Vec3V lightPos = m_lightningStrike.GetLightSourcePositionForClouds();
				lightPos += vCamPos;

				Color32 lightningColor = m_lightningStrike.GetColor();
				Vec3V lightColorVec3V = lightningColor.GetRGB();
				Vector3 lightColorVec3 = RCC_VECTOR3(lightColorVec3V);

				Vector3 lightPosVec3 = RCC_VECTOR3(lightPos);

				if(GetLightningTypeUpdate() == LIGHTNING_TYPE_DIRECTIONAL_BURST_STRIKE)
				{
					g_WeatherAudioEntity.TriggerThunderStrikeBurst(lightPosVec3);
				}
				else
				{
					g_WeatherAudioEntity.TriggerThunderStrike(lightPosVec3);
				}
				
			}

			bool bCloudBurstFinished = true;
			if(!bIsPlayerSwitchActive)
			{
				for(int i=0;i<MAX_NUM_CLOUD_LIGHTS;i++)
				{
					if(m_CloudBurstSequence[i].m_BlastCount)
					{
						UpdateCloudLightningSequence(deltaTime, i);
						bCloudBurstFinished = false;
					}

					if(m_lightSources[i].GetIntensity() > 0.0f)
					{
						m_IsCloudLightningActive = true;
					}				
				}

			}
			bFinished = bStrikeFinished && bCloudBurstFinished;

		}
		else if(GetLightningTypeUpdate() == LIGHTNING_TYPE_CLOUD_BURST) //For Cloud Bursts
		{
			bFinished = true;
			if(!bIsPlayerSwitchActive)
			{
				for(int i=0;i<MAX_NUM_CLOUD_LIGHTS;i++)
				{
					if(m_CloudBurstSequence[i].m_BlastCount)
					{
						UpdateCloudLightningSequence(deltaTime, i);
						bFinished = false;
					}

					if(m_lightSources[i].GetIntensity() > 0.0f)
					{
						m_IsCloudLightningActive = true;
					}				
				}	

			}		
		}
		else if(GetLightningTypeUpdate() == LIGHTNING_TYPE_DIRECTIONAL_BURST) //For Directional Bursts
		{
			m_lightningFlashAmount = 0.0f;
			//If we have a lightning sequence then play it out
			if(m_DirectionalBurstSequence.m_BlastCount && !bIsPlayerSwitchActive)
			{
				UpdateDirectionalBurstSequence(deltaTime);
			}
			bFinished = (m_DirectionalBurstSequence.m_BlastCount == 0) || bIsPlayerSwitchActive;
		}

		if(bFinished)
		{
			m_forceLightningFlash = false;
			SetLightningTypeUpdate(LIGHTNING_TYPE_NONE);
		}
	}
	//// check if lightning audio has been set
	//if (m_lightningAudioStartTime)
	//{
	//	// check if it's time to play the audio 
	//	if (fwTimer::GetTimeInMilliseconds()>m_lightningAudioStartTime)
	//	{
			//// trigger the audio
			//g_WeatherAudioEntity.TriggerLightning(0.f);

			// create rumble on the pad
			//float fShakeIntensity = 0.1f + (m_lightningAudioDuration * g_LightningSettings.GetLightningShakeIntensity());
			//CControlMgr::StartPlayerPadShakeByIntensity(10 + m_lightningAudioDuration * 2, fShakeIntensity);

	//		// audio triggered - reset
	//		m_lightningAudioStartTime = 0;
	//	}
	//}
}

///////////////////////////////////////////////////////////////////////////////
//  Render
///////////////////////////////////////////////////////////////////////////////
void CVfxLightningManager::Render()
{
	//Skip function if there's no lightning or if burst is active
	if(GetLightningTypeRender() == LIGHTNING_TYPE_NONE || GetLightningTypeRender() == LIGHTNING_TYPE_DIRECTIONAL_BURST)
	{
		return ;
	}

	if((GetLightningTypeRender() == LIGHTNING_TYPE_STRIKE || GetLightningTypeRender() == LIGHTNING_TYPE_DIRECTIONAL_BURST_STRIKE) && !Water::IsCameraUnderwater())
	{

		if(m_lightningStrikeRender )
		{

			PF_PUSH_TIMEBAR_DETAIL("Lightning Strikes");

			grcStateBlock::SetStates(grcStateBlock::RS_NoBackfaceCull, CShaderLib::DSS_TestOnly_LessEqual_Invert, grcStateBlock::BS_Max);

			// store viewport settings and matrices as they are changed
			grcViewport* view = grcViewport::GetCurrent();
			grcViewport view_prev = *view; // save the viewport so we can restore it when we're done

			// Move far-clip plane
			view->SetNearFarClip(1.0f, 25000.0f);

			// Reset world matrix
			view->SetWorldIdentity();

			// HACK : Make it so lightning is only drawn on the far clip plane (same as sky)
			const int viewX = view->GetUnclippedWindow().GetX();
			const int viewY = view->GetUnclippedWindow().GetY();
			const int viewW = view->GetUnclippedWindow().GetWidth();
			const int viewH = view->GetUnclippedWindow().GetHeight();
			
			// Infinite projection matrix
			Mat44V proj = view->GetProjection();

			// Take projection matrix far-plane to infinity and 
			// adjust appropriately
#if USE_INVERTED_PROJECTION_ONLY
			proj.GetCol2Ref().SetZf(-(1e-6f - 1.0f)-1.0f);
			proj.GetCol2Ref().SetW(ScalarV(V_NEGONE));
			proj.GetCol3Ref().SetZ(ScalarV(V_ZERO));

			view->SetProjection(proj);

			// This ensures that before the perspective divide 
			// we that [x, y, z, z] -> [x/z, y/z, 0.0, 0.0]
			view->SetWindow(viewX, viewY, viewW, viewH, 0.0f, 0.0f);
#else
			proj.GetCol2Ref().SetZf(1e-6f - 1.0f);
			proj.GetCol2Ref().SetW(ScalarV(V_NEGONE));
			proj.GetCol3Ref().SetZ(ScalarV(V_ZERO));

			view->SetProjection(proj);

			// This ensures that before the perspective divide 
			// we that [x, y, z, z] -> [x/z, y/z, 1.0, 1.0]
			view->SetWindow(viewX, viewY, viewW, viewH, 1.0f, 1.0f);
#endif // USE_INVERTED_PROJECTION_ONLY


			//Render the lightning strike
			m_lightningStrikeRender->Render();

			// Revert settings
			*view = view_prev;
			grcViewport::SetCurrent(view); 

			grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
			grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_Default);
			grcStateBlock::SetRasterizerState(grcStateBlock::RS_Default);

			// reset the render state
			CShaderLib::SetGlobalEmissiveScale(1.0f,true);


			PF_POP_TIMEBAR_DETAIL();
		}
		else
		{
			vfxAssertf(m_lightningStrikeRender!= NULL, "Rendering lightning strike without the render thread side lightning Strike Set");
		}
	}
		
		

#if __BANK
	if(g_RenderDebugLightSphere && m_IsCloudLightningActive)
	{
		if(GetLightningTypeRender() == LIGHTNING_TYPE_CLOUD_BURST || GetLightningTypeUpdate() == LIGHTNING_TYPE_STRIKE || GetLightningTypeUpdate() == LIGHTNING_TYPE_DIRECTIONAL_BURST_STRIKE)
		{
			for(int index = 0; index < MAX_NUM_CLOUD_LIGHTS; index++)
			{
				if(m_CloudBurstSequence[index].m_BlastCount)
				{
					grcDebugDraw::Sphere(m_lightSources[index].GetPosition(), m_lightSources[index].GetRadius(), Color32(1.0f, 1.0f, 0.0f, 1.0f), false);
				}
			}
		}
	}
#endif

}

///////////////////////////////////////////////////////////////////////////////
//  UpdateAfterRender - called in VisualEffect::UpdateSafe()
///////////////////////////////////////////////////////////////////////////////
void CVfxLightningManager::UpdateAfterRender()
{
	//Set the correct light sources for cloud lightning
	SetLightSourcesForClouds();


}

#if __BANK

///////////////////////////////////////////////////////////////////////////////
//  Save and Reload settings 
///////////////////////////////////////////////////////////////////////////////
static const u32 sBANKONLY_Lightning_MaxPathLen = 256;
void sBANKONLY_SaveCurrentLightningSettings()
{
	g_LightningSettings.Save(g_LightningSettingsFile);
}
void sBANKONLY_ReloadLightningSettings()
{
	g_LightningSettings.Load(g_LightningSettingsFile);
}


///////////////////////////////////////////////////////////////////////////////
//  InitWidget
///////////////////////////////////////////////////////////////////////////////
void CVfxLightningManager::InitWidgets()
{
	bkBank* pBank = vfxWidget::GetBank();

	if (pBank==NULL)
	{
		return;
	}

	s_pVfxLightningGroup = pBank->PushGroup("Vfx Lightning", false);

	s_pCreateLightningWidgetsButton = pBank->AddButton("Create Vfx Lightning Widgets", CreateVfxLightningWidgets);

	pBank->PopGroup();

}

void CVfxLightningManager::CreateVfxLightningWidgets()
{
	bkBank* pBank = vfxWidget::GetBank();
	if (pBank==NULL || s_pCreateLightningWidgetsButton==NULL || s_pVfxLightningGroup == NULL)
	{
		return;
	}

	s_pCreateLightningWidgetsButton->Destroy();
	s_pCreateLightningWidgetsButton = NULL;

	pBank->SetCurrentGroup(*s_pVfxLightningGroup);
	{
		pBank->AddButton("Trigger Lightning", CWeather::ForceLightningFlash);
		pBank->AddSlider("Delta Segment Size", &LIGHTNING_SEGEMENT_DELTA_SIZE, 0.0f, 50.0f, 0.001f);
		pBank->AddToggle("Use Debug Lightning Settings", &g_UseDebugLightningSettings);

		pBank->AddToggle("Render Debug Light Sphere", &g_RenderDebugLightSphere);
		pBank->AddSlider("Angular Width Power Multiplier", &g_AngularWidthPowerMult, 0.0, 10.0f, 1.0f);
		pBank->AddSlider("Lightning Distance", &g_LightningDistance, 0.0f, 10000.0f, 0.01f);
		pBank->AddSlider("Lightning Segment Width", &g_LightningSegmentWidth, 0.0f, 50.0f, 0.001f);
		pBank->AddToggle("Enable Exposure Reset for lightning", &g_LightningUseExposureReset);
		pBank->AddToggle("Use Timecycle Mod", &g_UseTimeCycleMod);
		pBank->AddToggle("Flicker Directional Light with Lightning", &g_FlickerDirectionalColor);
		
		const char* forcelightningTypes[] =
		{
			"Disable",
			"Burst Only",
			"Cloud Burst",
			"Strike Only",
			"Burst + Strike"
		};
		CompileTimeAssert(NELEM(forcelightningTypes) == LIGHTNING_TYPE_TOTAL);
		pBank->AddCombo("Force Lightning Type", (int*)(&g_forceLightningType), NELEM(forcelightningTypes), forcelightningTypes);

		pBank->PushGroup("Debug Lightning");
		pBank->AddColor("Color", &g_DebugLightningSettings.Color);
		pBank->PopGroup();

		pBank->AddSlider("Force Strike Variation Index", &g_DebugForceVariationIndex, -1, LIGHTNING_SETTINGS_NUM_VARIATIONS - 1, 1);
		pBank->PushGroup("Lightning Settings");
		{
			pBank->AddButton("Reload", sBANKONLY_ReloadLightningSettings);
			pBank->AddButton("Save", sBANKONLY_SaveCurrentLightningSettings);
			pBank->AddSeparator();
			g_LightningSettings.AddWidgets(*pBank);
		}
		pBank->PopGroup();
	}
	pBank->UnSetCurrentGroup(*s_pVfxLightningGroup);
}


///////////////////////////////////////////////////////////////////////////////
//  AddLightningBurstInfoWidgets
///////////////////////////////////////////////////////////////////////////////
void CVfxLightningManager::AddLightningBurstInfoWidgets(bkBank* pBank)
{
	pBank->AddSlider("Lightning", &m_lightningChance, 0.0f, 1.0f, 0.0f);
}


///////////////////////////////////////////////////////////////////////////////
//  AddLightningBurstDebugWidgets
///////////////////////////////////////////////////////////////////////////////
void CVfxLightningManager::AddLightningBurstDebugWidgets(bkBank* pBank)
{
	m_overrideLightning = -0.01f;
	pBank->AddSlider("Override Lightning", &m_overrideLightning, -0.01f, 1.0f, 0.01f);

}

#endif

#if GTA_REPLAY

void CVfxLightningManager::RecordReplayLightning()
{
	switch(GetLightningTypeUpdate())
	{
	case LIGHTNING_TYPE_CLOUD_BURST:
		{
			//CreateCloudLightningSequence
			CReplayMgr::RecordFx(CPacketCloudLightningFxPacket(m_CloudBurstRootOrbitPos,
				(u8)0,
				m_numCloudLights,
				m_CloudBurstSequence));
		}
		break;
	case LIGHTNING_TYPE_DIRECTIONAL_BURST:
		{
			//CreateDirectionalBurstSequence
			CReplayMgr::RecordFx(CPacketDirectionalLightningFxPacket(m_DirectionalBurstRootOrbitPos,
				(u8)m_DirectionalBurstSequence.m_BlastCount,
				m_DirectionalBurstSequence.m_LightningBlasts,
				VEC3V_TO_VECTOR3(m_lightningFlashDirection),
				m_lightningFlashAmount));		
		}
		break;
	case LIGHTNING_TYPE_STRIKE:
	case LIGHTNING_TYPE_DIRECTIONAL_BURST_STRIKE:
		{
			//AddLightningStrike		
			//CreateCloudLightningSequence
			CReplayMgr::RecordFx(CPacketLightningStrikeFxPacket(m_lightningFlashDirection, m_lightningStrike));
		}		
		break;

	default:
		break;
	}
}


void CVfxLightningManager::CreateDirectionalBurstSequence(const Vector2& rootOrbitPos, u8 blastCount, const PacketLightningBlast* pBlasts, const Vector3& direction, float flashAmount)
{
	m_DirectionalBurstRootOrbitPos			= rootOrbitPos;
	m_DirectionalBurstSequence.m_BlastCount	= blastCount;
	for(u8 i = 0; i < blastCount; ++i)
	{
		pBlasts[i].m_OrbitPos.Load(m_DirectionalBurstSequence.m_LightningBlasts[i].m_OrbitPos);
		m_DirectionalBurstSequence.m_LightningBlasts[i].m_Intensity	= pBlasts[i].m_Intensity;
		m_DirectionalBurstSequence.m_LightningBlasts[i].m_Duration	= pBlasts[i].m_Duration;
		m_DirectionalBurstSequence.m_LightningBlasts[i].m_Timer		= 0.0f;
	}
	m_lightningFlashDirection = VECTOR3_TO_VEC3V(direction);
	m_lightningFlashAmount	= flashAmount;

	SetLightningTypeUpdate(LIGHTNING_TYPE_DIRECTIONAL_BURST);
	m_lightningStartTime = fwTimer::GetTimeInMilliseconds();

	PostFX::ResetAdaptedLuminance();
}


void CVfxLightningManager::CreateCloudLightningSequence(const Vector2& rootOrbitPos, u8 dominantIndex, u8 burstCount, const PacketCloudBurst* pBursts)
{
	(void)dominantIndex;
	m_CloudBurstRootOrbitPos = rootOrbitPos;

	for(u8 s = 0; s < burstCount; ++s)
	{
		pBursts[s].m_rootOrbitPos.Load(m_CloudBurstSequence[s].m_OrbitRoot);
		m_CloudBurstSequence[s].m_Delay = pBursts[s].m_delay;

		m_CloudBurstSequence[s].m_BlastCount = pBursts[s].m_blastCount;
		for(u8 i = 0; i < m_CloudBurstSequence[s].m_BlastCount; ++i)
		{
			pBursts[s].m_lightningBlasts[i].m_OrbitPos.Load(m_CloudBurstSequence[s].m_LightningBlasts[i].m_OrbitPos);
			m_CloudBurstSequence[s].m_LightningBlasts[i].m_Size			= pBursts[s].m_lightningBlasts[i].m_size;
			m_CloudBurstSequence[s].m_LightningBlasts[i].m_Intensity	= pBursts[s].m_lightningBlasts[i].m_Intensity;
			m_CloudBurstSequence[s].m_LightningBlasts[i].m_Duration		= pBursts[s].m_lightningBlasts[i].m_Duration;
			m_CloudBurstSequence[s].m_LightningBlasts[i].m_Timer		= 0.0f;
		}
	}
	m_CloudBurstSequence[0].m_Delay = 0.0f;

	SetLightningTypeUpdate(LIGHTNING_TYPE_CLOUD_BURST);
	m_lightningStartTime = fwTimer::GetTimeInMilliseconds();
}

#endif // GTA_REPLAY
