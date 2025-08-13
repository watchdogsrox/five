// =============================================================================================== //
// INCLUDES
// =============================================================================================== //

#include "renderer/lights/lights.h"

// STL
#include <set>

// Rage headers
#include "grcore/allocscope.h"
#include "grcore/debugdraw.h"
#include "grcore/device.h"
#include "grprofile/pix.h"
#include "grmodel/modelfactory.h"
#include "spatialdata/bkdtree.h"
#include "system/criticalsection.h"
#include "system/param.h"

#if __XENON
#include "grcore/wrapper_d3d.h"
#endif

// Framework headers
#include "fwdrawlist/drawlist.h"
#include "fwdrawlist/drawlistmgr.h"
#include "fwscene/scan/scan.h"

// Game headers
#include "camera/CamInterface.h"
#include "camera/cutscene/CutsceneDirector.h"
#include "camera/viewports/ViewportManager.h"
#include "cutscene/CutSceneManagerNew.h"
#include "cutscene/CutSceneCameraEntity.h"
#include "debug/budgetdisplay.h"
#include "debug/Rendering/DebugLights.h"
#include "debug/Rendering/DebugLighting.h"
#include "debug/Rendering/DebugDeferred.h"
#include "Game/Weather.h"
#include "Peds/Ped.h"
#include "Pickups/Pickup.h"
#include "Pickups/Data/PickupData.h"
#include "Renderer/Renderer.h"
#include "renderer/Occlusion.h"
#include "renderer/PostProcessFX.h"
#include "renderer/SSAO.h"
#include "renderer/Deferred/DeferredLighting.h"
#include "renderer/Deferred/DeferredConfig.h"
#include "renderer/Deferred/GBuffer.h"
#include "renderer/Lights/AmbientLights.h"
#include "renderer/Lights/LightCulling.h"
#include "renderer/Lights/LightEntity.h"
#include "renderer/Lights/TiledLighting.h"
#include "renderer/Lights/LODLights.h"
#include "renderer/Shadows/Shadows.h"
#include "renderer/Shadows/paraboloidshadows.h"
#include "renderer/render_channel.h"
#include "renderer/RenderPhases/RenderPhaseCascadeShadows.h"
#include "renderer/RenderPhases/RenderPhaseHeightMap.h"
#include "renderer/Util/ShaderUtil.h"
#include "renderer/Water.h"
#include "Scene/playerswitch/PlayerSwitchInterface.h"
#include "Scene/Portals/Portal.h"
#include "Scene/2deffect.h"
#include "Scene/world/GameWorldHeightMap.h"
#include "streaming/streaming.h"
#include "TimeCycle/TimeCycle.h"
#include "vfx/VfxHelper.h"
#include "timecycle/TimeCycleConfig.h"
#include "fwscene/scan/ScanNodes.h"
#include "fwscene/scan/ScreenQuad.h"
#if __BANK
#include "debug/BudgetDisplay.h"
#include "Cutscene/CutSceneDebugManager.h"
#endif
#include "weapons/projectiles/Projectile.h"

#if GTA_REPLAY
#include "control/replay/Misc/LightPacket.h"
#endif

// =============================================================================================== //
// DEFINES
// =============================================================================================== //

RENDER_OPTIMISATIONS()
PARAM(nolwlights, "Specify to switch off lightweight shader swapping");
PARAM(sceneShadows, "Enables shadows from scene lights. 0=none, 1=both, 2=spots only, 3=points only");

#define FAST_CULL_LIGHTS_TREE_THRESHOLD 20
PF_PAGE(LightsCost, "Lights");
PF_GROUP(LightsRenderingCost);
PF_LINK(LightsCost, LightsRenderingCost);
PF_TIMER(GetLightsInArea, LightsRenderingCost);

#if (RSG_ORBIS || RSG_DURANGO) && DEPTH_BOUNDS_SUPPORT
	#define DEPTHBOUNDS_STATEBLOCK_ONLY(...)	__VA_ARGS__
#else
	#define DEPTHBOUNDS_STATEBLOCK_ONLY(...)
#endif

// ----------------------------------------------------------------------------------------------- //

// active lights
CLightGroup Lights::m_lightingGroup;
CLightSource Lights::m_blackLight;

// lights in scene
static CLightSource m_sceneLightsBuffer[2][MAX_STORED_LIGHTS] ; 
static CLightSource m_prevSceneShadowLightsBuffer[MAX_STORED_LIGHTS] ;

LightingData Lights::lightingData;
LightingData *Lights::m_currFrameInfo = NULL;

CLightSource* Lights::m_prevSceneShadowLights = m_prevSceneShadowLightsBuffer;
CLightSource* Lights::m_sceneLights = m_sceneLightsBuffer[0];
CLightSource* Lights::m_renderLights = m_sceneLightsBuffer[1];

s32 Lights::m_numPrevSceneShadowLights;
s32 Lights::m_numSceneLights;
s32 Lights::m_numRenderLights;
s32 Lights::m_numRenderLightShafts;

static s32 m_currentLightBuffer = 0;
static s32 m_numSceneLightsBuf[3];

static CLightShaft m_lightShaftsBuffer[2][MAX_STORED_LIGHT_SHAFTS];
CLightShaft* Lights::m_lightShaft = m_lightShaftsBuffer[0];
s32 Lights::m_numLightShafts = 0;

CLightShaft* Lights::m_renderLightShaft = m_lightShaftsBuffer[0];
static s32 m_numLightShaftsBuf[2];
static s32 m_currentLightShaftBuffer=0;

static int s_LightweightTechniqueGroupId[Lights::kLightPassMax][Lights::kLightsActiveMax];
static int s_LightweightHighQualityTechniqueGroupId[Lights::kLightPassMax][Lights::kLightsActiveMax];

static DECLARE_MTR_THREAD int s_PreviousTechniqueGroupId = -1;
static DECLARE_MTR_THREAD int s_LightWeightTechniquesDepth = 0;
static DECLARE_MTR_THREAD int s_ResetLightingOverrideConstant = false;

static float s_exposureAvgArray[4];
static int   s_exposureAvgIndex = 0;
static int   s_exposureAvgSize = 0;
static float s_pow2NegExposure = 0.0f;
static int	 s_pedLightReset = 0;

static bool s_LightsAddedToDrawList[3] = { false, false, false };

#if __BANK
static u32 s_DefaultSceneLightFlags = 0;
static u32 s_DefaultSceneShadowTypes = 0;
#endif

grcBlendStateHandle Lights::m_SceneLights_B;
grcDepthStencilStateHandle Lights::m_Volume_DS[3][2][2];		// [kLightOnePassStencil=0, kLightTwoPassStencil=1, kLightNoStencil=2] [kLightCameraInside=0,kLightCameraOutside=1] [kLightDepthBoundsDisabled=0, kLightDepthBoundsEnabled=1]
grcRasterizerStateHandle Lights::m_Volume_R[3][2][2];			// [kLightOnePassStencil=0, kLightTwoPassStencil=1, kLightNoStencil=2] [kLightCameraInside=0,kLightCameraOutside=1] [kLightNoDepthBias=0,kLightDepthBiased=1] 
grcDepthStencilStateHandle Lights::m_StencilFrontSetup_DS[2];	// [kLightCameraInside=0,kLightCameraOutside=1]
grcDepthStencilStateHandle Lights::m_StencilPEDOnlyGreater_DS;
grcDepthStencilStateHandle Lights::m_StencilPEDOnlyLessEqual_DS;

#if USE_STENCIL_FOR_INTERIOR_EXTERIOR_CHECK
grcDepthStencilStateHandle Lights::m_StencilBackSetup_DS[3];	// [kLightStencilAllSurfaces=0, kLightStencilInteriorOnly=1, kLightStencilExteriorOnly=2]
#else
grcDepthStencilStateHandle Lights::m_StencilBackSetup_DS[1];	// [kLightStencilAllSurfaces=0]
#endif
grcBlendStateHandle Lights::m_StencilSetup_B;

grcDepthStencilStateHandle Lights::m_StencilDirectSetup_DS;	
grcDepthStencilStateHandle Lights::m_StencilBackDirectSetup_DS[2];		// [kLightCameraInside=0,kLightCameraOutside=1]
grcDepthStencilStateHandle Lights::m_StencilFrontDirectSetup_DS[2];		// [kLightCameraInside=0,kLightCameraOutside=1]
grcDepthStencilStateHandle Lights::m_StencilCullPlaneSetup_DS[2][2];	// [kLightOnePassStencil=0, kLightTwoPassStencil=1, kLightNoStencil=2] [kLightCullPlaneCamInFront=0, kLightCullPlaneCamInBack=1]
grcDepthStencilStateHandle Lights::m_StencilSinglePassSetup_DS[2];		// [kLightCameraInside=0,kLightCameraOutside=1]

grcBlendStateHandle Lights::m_AmbientScaleVolumes_B;
grcDepthStencilStateHandle Lights::m_AmbientScaleVolumes_DS;
grcRasterizerStateHandle Lights::m_AmbientScaleVolumes_R;

grcBlendStateHandle Lights::m_VolumeFX_B;
grcDepthStencilStateHandle Lights::m_VolumeFX_DS;
grcRasterizerStateHandle Lights::m_VolumeFX_R;
grcRasterizerStateHandle Lights::m_VolumeFXInside_R;

grcDepthStencilStateHandle Lights::m_pedLight_DS;
Vector3 Lights::m_pedLightDirection = Vector3(0.0f, 0.0f, 0.0f);
float Lights::m_pedLightDayFadeStart = 6.0f;
float Lights::m_pedLightDayFadeEnd = 7.0f;
float Lights::m_pedLightNightFadeStart = 20.0f;
float Lights::m_pedLightNightFadeEnd = 21.0f;
bool Lights::m_fadeUpPedLight = false;
u32  Lights::m_fadeUpStartTime = 0;
float Lights::m_fadeUpDuration = 0.0f;


float Lights::m_lightFadeDistance = 0.0f;
float Lights::m_shadowFadeDistance = 0.0f;
float Lights::m_specularFadeDistance = 0.0f;
float Lights::m_volumetricFadeDistance = 0.0f;
Vec4V Lights::m_defaultFadeDistances = Vec4V(V_ZERO);

float Lights::m_lightCullDistanceForNonGBufLights = 0.0f;
float Lights::m_mapLightFadeDistance = 1024.0f;

float Lights::m_startDynamicBakeMin = 0.7f;
float Lights::m_startDynamicBakeMax = 1.7f;

float Lights::m_endDynamicBakeMin = 0.3f;
float Lights::m_endDynamicBakeMax = 1.3f;

Vector4 Lights::m_SSSParams = Vector4(0.073f,100.0f,0.49f,0.14f);
Vector4 Lights::m_SSSParams2 = Vector4(0.0f, 0.0f, 1.0f/30.0f, 30.0f);
bool Lights::m_bUseSSS = true;

float Lights::m_lightOverrideMaxIntensityScale = 1.0f;

bool Lights::m_bRenderShadowedLightsWithNoShadow = false;

// LOW RES VOLUME FX

grcDepthStencilStateHandle	ms_dsVolumeLightRender		 = grcStateBlock::DSS_Invalid;
grcDepthStencilStateHandle	ms_dsVolumeInsideLightRender = grcStateBlock::DSS_Invalid;
grcDepthStencilStateHandle	ms_dsVolumeLightReconstruct	 = grcStateBlock::DSS_Invalid;
grcBlendStateHandle			ms_bsRefreshUpsample		 = grcStateBlock::BS_Invalid;
grcBlendStateHandle			ms_bsVolumeUpsampleAlphaOnly = grcStateBlock::BS_Invalid;
grcDepthStencilStateHandle	ms_ds_DisableDepth			 = grcStateBlock::DSS_Invalid;

static const u8 g_VolumeRenderStencilMask  = DEFERRED_MATERIAL_SPECIALBIT;
static const u8 g_VolumeRenderRefMaterial  = DEFERRED_MATERIAL_SPECIALBIT;

void		 	InitVolumeStates()
{

	if(ms_dsVolumeLightRender == grcStateBlock::DSS_Invalid)
	{
		//Create States if not created
		grcDepthStencilStateDesc ds;

		ds.DepthEnable					= true;
		ds.DepthFunc					= rage::FixupDepthDirection(grcRSV::CMP_LESSEQUAL);
		ds.DepthWriteMask				= false;
		ds.StencilEnable                = true;
		ds.StencilReadMask              = 0x0;
		ds.StencilWriteMask             = g_VolumeRenderStencilMask; // If you change this, make sure to update the clean up call to cellGcmSetStencilMask
		ds.FrontFace.StencilFailOp      = grcRSV::STENCILOP_KEEP;
		ds.FrontFace.StencilDepthFailOp = grcRSV::STENCILOP_KEEP;
		ds.FrontFace.StencilPassOp      = grcRSV::STENCILOP_REPLACE;
		ds.FrontFace.StencilFunc        = grcRSV::CMP_ALWAYS;
		ds.BackFace                     = ds.FrontFace;

		ms_dsVolumeLightRender = grcStateBlock::CreateDepthStencilState(ds);

		ds.DepthEnable					= false;
		ms_dsVolumeInsideLightRender = grcStateBlock::CreateDepthStencilState(ds);

		ds.DepthEnable					= false;
		ds.StencilReadMask              = g_VolumeRenderStencilMask;
		ds.StencilWriteMask             = 0x0;
		ds.FrontFace.StencilFailOp      = grcRSV::STENCILOP_KEEP;
		ds.FrontFace.StencilDepthFailOp = grcRSV::STENCILOP_KEEP;
		ds.FrontFace.StencilPassOp      = grcRSV::STENCILOP_KEEP;
		ds.FrontFace.StencilFunc        = grcRSV::CMP_NOTEQUAL;
		ds.BackFace                     = ds.FrontFace;

		ms_dsVolumeLightReconstruct = grcStateBlock::CreateDepthStencilState(ds);

		// Stencil/Zfar cull stateblocks
		grcBlendStateDesc BSDesc;
		BSDesc.BlendRTDesc[0].BlendEnable = false;
		BSDesc.BlendRTDesc[0].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_NONE;

		ms_bsRefreshUpsample = grcStateBlock::CreateBlendState(BSDesc);

		BSDesc.BlendRTDesc[0].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_ALPHA;
		ms_bsVolumeUpsampleAlphaOnly = grcStateBlock::CreateBlendState(BSDesc);
	}
}
u32 Lights::m_sceneShadows			= 0;
u32 Lights::m_forceSceneLightFlags	= (__D3D11 || RSG_ORBIS) ? LIGHT_TYPE_POINT | LIGHT_TYPE_SPOT : 0;
u32 Lights::m_forceSceneShadowTypes = LIGHTFLAG_CAST_STATIC_GEOM_SHADOWS | LIGHTFLAG_CAST_DYNAMIC_GEOM_SHADOWS | LIGHTFLAG_CAST_SHADOWS;

DECLARE_MTR_THREAD bool Lights::m_useHighQualityTechniques = false;
DECLARE_MTR_THREAD bool Lights::s_UseLightweightTechniques = false;
DECLARE_MTR_THREAD Lights::LightweightTechniquePassType Lights::s_LightweightTechniquePassType = Lights::kLightPassNormal;

//Used to detect if we can avoid recreating the depth downsample
XENON_ONLY(static bool s_IsDownsampleDepthReady = false;)
#if __ASSERT
static DECLARE_MTR_THREAD bool s_CheckLightWeightTechniquesDepth = false;
#endif

#if __BANK
static bool m_debugOverrideDirectionalLightDir = false;	
static Vector3 m_debugDirectionalLightDir = Vector3(0,0,1);
#endif

static Vector3 LumFactors = Vector3(0.2125f, 0.7154f, 0.0721f);

LightsVisibilitySortData g_LightsVisibilitySortData  ;
LightsVisibilityOutput g_LightsVisibilitySortOutput[2][MAX_STORED_LIGHTS] ;
u32 g_LightsVisibilitySortCount[2];
atFixedBitSet<MAX_STORED_LIGHTS> g_LightsVisibilityCamInsideOutput[2] ;
atFixedBitSet<MAX_STORED_LIGHTS> g_LightsVisibilityVisibleLastFrameOutput[2] ;

sysDependency			g_LightsVisibilitySortDependency;
u32						g_LightsVisibilitySortDependencyRunning;

#if __PPU
DECLARE_FRAG_INTERFACE(LightsVisibilitySort);
#endif

extern bool gLastGenMode;

#if __ASSERT
static void AssertIsValid( const CLightSource& lgt)
{
	Assert( (lgt.GetType() != LIGHT_TYPE_SPOT) || (lgt.GetConeCosOuterAngle() >= -1.0f && lgt.GetConeCosOuterAngle() <= 1.0f) );
}
#endif // __ASSERT

// =============================================================================================== //
// HELPER CLASSES
// =============================================================================================== //

static DECLARE_MTR_THREAD ThreadVector4 s_prevGlobalToneMapScalers;

bank_bool sm_forceMPGlow = false;
bank_float sm_settings_lifetimeFade = 1.0f;
bank_float sm_settings_minMult = 00.5f;
bank_float sm_settings_maxMult = 16.0f;
bank_float sm_settings_frequency = 0.0045f;
bank_bool sm_settings_useOtherGlow = true;
bank_float sm_settings_power = 1.0f;
bank_float sm_settings_grenadeFrequencyMultiplier = 0.15f;

class dlCmdLightOverride : public dlCmdBase {
public:
	
	enum {
		INSTRUCTION_ID = DC_dlCmdLightOverride,
	};
	
	dlCmdLightOverride(const CEntity * entity, bool isReset)
	{ 
		m_isReset = isReset;
		m_intensity = 0.0f;
		m_frequency = sm_settings_frequency;
		m_maxIntensityScale = Lights::m_lightOverrideMaxIntensityScale;

		if(!isReset)
		{
			phInst* finst = entity->GetCurrentPhysicsInst();

			if(entity->GetIsTypeObject())
			{
				CPickup *pickup = CPickup::GetParentPickUp((CObject *)entity);

				if(pickup)
				{
					if (!pickup->HasGlow())
					{
						m_intensity = 0.0f;
					}
					else if (pickup->HasLifetime())
					{
						// Pickup has a lifetime, fade intensity.
						float lifetime = (float)pickup->GetLifeTime() / 1000.0f;
						m_intensity = Clamp(lifetime/sm_settings_lifetimeFade,0.0f,1.0f);
						// Scale by intensity.

						if( sm_forceMPGlow || CNetwork::IsGameInProgress() )
							m_intensity *= Lerp(PostFX::GetLinearExposure(), pickup->GetPickupData()->GetMPGlowIntensity(), pickup->GetPickupData()->GetMPDarkGlowIntensity());
						else
							m_intensity *= Lerp(PostFX::GetLinearExposure(), pickup->GetPickupData()->GetGlowIntensity(), pickup->GetPickupData()->GetDarkGlowIntensity());
					}
					else
					{
						// Use intensity as is.
						if( sm_forceMPGlow || CNetwork::IsGameInProgress() )
							m_intensity = Lerp(PostFX::GetLinearExposure(), pickup->GetPickupData()->GetMPGlowIntensity(), pickup->GetPickupData()->GetMPDarkGlowIntensity());
						else
							m_intensity = Lerp(PostFX::GetLinearExposure(), pickup->GetPickupData()->GetGlowIntensity(), pickup->GetPickupData()->GetDarkGlowIntensity());
					}
				}
				else 
				{
					if(finst != NULL)
					{
						CProjectile *projectile = ((CObject *)entity)->GetAsProjectile();

						if( projectile )
						{
							m_frequency *= 1.0f + Saturate(powf((projectile->GetAge() / projectile->GetInfo()->GetLifeTime()),3.0f)) * sm_settings_grenadeFrequencyMultiplier;
						}
					}
				}
			}
		}
	}
	
	s32 GetCommandSize()						{ return(sizeof(*this)); }
	static void ExecuteStatic(dlCmdBase &cmd) { ((dlCmdLightOverride &) cmd).Execute(); }
	void Execute()
	{
		if(!m_isReset)
		{
			float min = sm_settings_minMult;
			float max = sm_settings_maxMult * m_maxIntensityScale;


			float time = (float)fwTimer::GetTimeInMilliseconds();
			float multiplier = 0.0f;
			if( sm_settings_useOtherGlow )
			{
				const float offs = powf((Sinf(time * m_frequency) + 1.0f) / 2.0f,sm_settings_power);
				multiplier = Lerp(offs,min,max);
			}
			else
			{
				const float alpha = Abs(powf(Sinf(time *	 m_frequency),sm_settings_power));
				multiplier = Lerp(alpha,min,max);
			}

			const float finalMultiplier = Lerp(m_intensity,1.0f,multiplier);

			s_prevGlobalToneMapScalers = CShaderLib::GetGlobalToneMapScalers();
			const Vector4 newTMScalers = Vector4(s_prevGlobalToneMapScalers.x, s_prevGlobalToneMapScalers.y, s_prevGlobalToneMapScalers.z, s_prevGlobalToneMapScalers.w) * Vector4(1.0f,1.0f,finalMultiplier,1.0f);
			CShaderLib::SetGlobalToneMapScalers(newTMScalers);

			s_ResetLightingOverrideConstant = true;
		}
		else
		{
			CShaderLib::SetGlobalToneMapScalers(Vector4(s_prevGlobalToneMapScalers.x, s_prevGlobalToneMapScalers.y, s_prevGlobalToneMapScalers.z, s_prevGlobalToneMapScalers.w));
		}
	}

private:			
	float m_intensity;
	float m_maxIntensityScale;
	float m_frequency;
	bool m_isReset;
};


class dlCmdSetupLightsFrameInfo : public dlCmdBase {
public:
	enum {
		INSTRUCTION_ID = DC_SetupLightsFrameInfo,
	};
	
	dlCmdSetupLightsFrameInfo();
	s32 GetCommandSize()						{ return(sizeof(*this)); }
	static void ExecuteStatic(dlCmdBase &cmd) { ((dlCmdSetupLightsFrameInfo &) cmd).Execute(); }
	void Execute();

private:			
	LightingData* newFrameInfo;
};

class dlCmdClearLightsFrameInfo : public dlCmdBase {
public:
	enum {
		INSTRUCTION_ID = DC_ClearLightsFrameInfo,
	};
	
	s32 GetCommandSize()						{ return(sizeof(*this)); }
	static void ExecuteStatic(dlCmdBase &cmd) { ((dlCmdClearLightsFrameInfo &) cmd).Execute(); }
	void Execute();
};

dlCmdSetupLightsFrameInfo::dlCmdSetupLightsFrameInfo()
{
	newFrameInfo = gDCBuffer->AllocateObjectFromPagedMemory<LightingData>(DPT_LIFETIME_RENDER_THREAD, sizeof(LightingData));
	sysMemCpy(newFrameInfo, &Lights::lightingData, sizeof(LightingData));
}

void dlCmdSetupLightsFrameInfo::Execute()
{
	Lights::m_currFrameInfo = newFrameInfo;
}


void dlCmdClearLightsFrameInfo::Execute()
{
	Lights::m_currFrameInfo = NULL;
}


class FastCullLightList
{
	BoundKdTree<spd::AABBIndexed> m_tree;
	atArray<spd::AABBIndexed> m_list;
	bool m_useTree;

	static const int NumLeafNodeChildren = 4;

	CLightSource* m_lightList;

public:
	void Init( int size = MAX_STORED_LIGHTS )  
	{
		m_list.Reserve( size ); 
		m_tree.Init( size );
		m_useTree = false;
	}
	int GetLightCount() const { return m_list.GetCount(); }
	CLightSource* GetLightList() const { return m_lightList; }

	void AddLights(CLightSource* lights, int amt , int useTreeThreshold = 32)
	{	
		m_list.Resize(0);

		for (int i = 0; i < amt ; i++)
		{
			if ((lights[i].GetType() != LIGHT_TYPE_SPOT && 
                 lights[i].GetType() != LIGHT_TYPE_POINT &&
			     lights[i].GetType() != LIGHT_TYPE_CAPSULE) || 
				lights[i].IsNoSpecular() ||
				lights[i].GetFlag(LIGHTFLAG_DONT_LIGHT_ALPHA) ||
				lights[i].GetFlag(LIGHTFLAG_DELAY_RENDER)) 
			{
				continue;
			}

#if __BANK
			bool bSkipLight = DebugLights::m_debug && DebugLights::m_isolateLight && (lights[i].GetLightEntity() != DebugLights::m_currentLightEntity);
			bSkipLight = bSkipLight || SBudgetDisplay::GetInstance().IsLightDisabled(lights[i]) || CutSceneManager::GetInstance()->GetDebugManager().IsLightDisabled(lights[i]);
			if(bSkipLight)
			{
				continue;
			}
#endif // __BANK

			spdAABB b;
			Lights::GetBound(lights[i], b);
			m_list.Push(spd::AABBIndexed(b.GetMinVector3(), b.GetMaxVector3(), i));
		}

#if __BANK
		DebugLights::m_numFastCullLightsCount = m_list.GetCount();
#endif
		m_useTree = m_list.GetCount() > useTreeThreshold;

		if (m_useTree )
		{
			m_tree.Create( m_list.begin(), m_list.end(), NumLeafNodeChildren ); 
		}

		m_lightList = lights;
	}

	int GetIndices( int* indices , const spdAABB& b )
	{
		spd::AABBIndexed::TraversalIntersector intersector( indices, b.GetMinVector3(), b.GetMaxVector3(), m_list.GetCount() );

		if ( m_useTree )
		{
			m_tree.Traverse( b.GetMinVector3(), b.GetMaxVector3(), intersector);
			return intersector.GetCount();
		}
		else
		{
			spd::AABBIndexed::TraversalIntersector res = std::for_each( m_list.begin(),  m_list.end(), intersector );
			return res.GetCount();
		}
	}
};

// ----------------------------------------------------------------------------------------------- //

class CCone
{
	Vector3 m_vertex;
	Vector3 m_axias;
	float   m_cosSqr;
	float	  m_sinRecip;
	float	  m_sinSqr;


	void CreateAngles( float angle )
	{
		rage::cos_and_sin( m_cosSqr,m_sinSqr,angle );

		m_cosSqr = m_cosSqr*m_cosSqr;
		m_sinRecip = 1.0f/m_sinSqr;
		m_sinSqr = m_sinSqr*m_sinSqr;
	}
public:
	CCone( const Vector3& vertex, const Vector3& to, float radius, bool /*Noop*/ )
		: m_vertex( vertex )
	{
		m_axias = to - vertex;
		float length = m_axias.Mag();
		m_axias.Normalize();

		float angle = 0.0f;
		if(length != 0.0f)
			angle = rage::Atan2f( radius/ length,1.0  );
		CreateAngles( angle );
	}

	CCone( const Vector3& vertex, const Vector3& axias, float angle ) :
	m_vertex( vertex ),
		m_axias( axias )
	{
		CreateAngles( angle );
	}


	CCone( float cosangle, const Vector3& vertex, const Vector3& axias ) :
	m_vertex( vertex ),
		m_axias( axias )
	{

		m_cosSqr = cosangle*cosangle;
		m_sinSqr = 1-m_cosSqr;

		m_sinRecip = rage::invsqrtf(m_sinSqr);

	}

	bool Intersects( const Vector3& pos, float radius ) const
	{
		// Optimised
		Vector3 U = m_vertex - ( radius * m_sinRecip ) * m_axias;
		Vector3 D = pos - U;
		float e = m_axias.Dot(D);

		if( e > 0.0f )
		{
			float dsqr = D.Dot(D);
			if( ( e * e ) >= ( dsqr * m_cosSqr ) )
			{
				D = pos - m_vertex;
				e = -m_axias.Dot(D);

				if ( e > 0.0f )
				{
					dsqr = D.Dot(D);
					if( ( e * e ) >= ( dsqr * m_sinSqr ) )
					{
						return dsqr <= ( radius * radius );
					}
					else
						return true;
				}
				else
					return true;
			}
		}
		return false;
	}
};

// ----------------------------------------------------------------------------------------------- //

static FastCullLightList	g_FastLights;

// ----------------------------------------------------------------------------------------------- //

class lightOrderIndex
{
public:

	u8 index;
	float sortVal;

	bool operator < (const lightOrderIndex& refParam) const
	{
		return (this->sortVal < refParam.sortVal);
	}
};

// ----------------------------------------------------------------------------------------------- //

atFixedArray<lightOrderIndex, MAX_STORED_LIGHTS> g_lightOrderSet;
bool g_lightsNeedSorting = false;

// ----------------------------------------------------------------------------------------------- //

static bool					g_bVolumeLightQueryInitialized = false;
static grcConditionalQuery	g_VolumeLightQuery;

// ----------------------------------------------------------------------------------------------- //
struct CLightData
{
#if __BE
	u32   m_intLoc;
	float m_size;			 // should be an estimate of screen size. for now it's just 1/distance
#else
	float m_size;			 // should be an estimate of screen size. for now it's just 1/distance
	u32   m_intLoc;
#endif
};

// ----------------------------------------------------------------------------------------------- //
class SortedLightInfo 
{
public:
	void Set(int index, float size, int shadowIndex, u32 interiorLoc) 
	{
		m_data.m_intLoc = interiorLoc;
		m_data.m_size = size; 
		m_index = index; 
		m_ShadowIndex = shadowIndex; 
	}
public:
	union 
	{
		s64 m_intLocAndSize;
		CLightData m_data;
	};
	int	  m_index;
	int   m_ShadowIndex;	 // so we don't has to look it up multiple times
};
// ----------------------------------------------------------------------------------------------- //

class CSortedVisibleLights
{
public:

	typedef atFixedArray<SortedLightInfo, MAX_STORED_LIGHTS> SortedLightArray;

	// Get the outside sorted light list
	SortedLightArray& GetOutsideSortedLights() { FastAssert(sysThreadType::IsRenderThread()); return m_outsideSortedLights; }

	// Get the inside sorted light list
	SortedLightArray& GetInsideSortedLights() { FastAssert(sysThreadType::IsRenderThread()); return m_insideSortedLights; }

	// Are volume lights present?
	bool AnyVolumeLightsPresent() const { FastAssert(sysThreadType::IsRenderThread()); return m_bAnyVolumeLightsPresent; }

	//Are any force low res volume lights present?
	bool AnyForcedLowResVolumeLightsPresent() const { FastAssert(sysThreadType::IsRenderThread()); return m_bIsAnyLowResVolumeLightsPresent; }

	// Is the camera inside any lights?
	bool IsCamInsideAnyLights() const { FastAssert(sysThreadType::IsRenderThread()); return m_bIsCamInsideAnyLights; }

	u16 GetNumShadowedVolumetricLights() const { FastAssert(sysThreadType::IsRenderThread()); return m_numShadowedVolumetricLights; }
	
	// Update the light lists
	void Update(bool cameraCut) 
	{
		CLightSource* lights = Lights::m_renderLights;
		Assert(grcViewport::GetCurrent() != NULL);
		grcViewport& grcVP = *grcViewport::GetCurrent();
		const Vector3 camPos = VEC3V_TO_VECTOR3(grcVP.GetCameraPosition());

		m_bAnyVolumeLightsPresent = false;
		m_bIsCamInsideAnyLights = false;
		m_bIsAnyLowResVolumeLightsPresent = false;
		m_outsideSortedLights.Reset(); 
		m_insideSortedLights.Reset();  

		const int nBufferID = gRenderThreadInterface.GetRenderBuffer();
		const u32 nVisibleLightsCount = g_LightsVisibilitySortCount[nBufferID];
		m_numShadowedVolumetricLights = 0;
		LightsVisibilityOutput* pLightsVisibleSorted = &(g_LightsVisibilitySortOutput[nBufferID][0]);
		
		for(u32 i = 0; i < nVisibleLightsCount; i++)
		{
			const int lightIndex = pLightsVisibleSorted[i].index;
			CLightSource &currentVisibleLight = lights[lightIndex];
			
			// Get the lights params.
			const float radius = currentVisibleLight.GetBoundingSphereRadius();
			// calc relative screen size estimate		
			const float dist = ( camPos - currentVisibleLight.GetPosition()).Mag();
			float size = dist>0 ? radius / dist : 100000.0f; // avoid divide by zero case

			int shadowIndex = -1;

			if (
#if RSG_PC
				(CSettingsManager::GetInstance().GetSettings().m_graphics.m_ShadowQuality != (CSettings::eSettingsLevel) (0)) &&
#endif
				currentVisibleLight.GetFlag(LIGHTFLAG_CAST_SHADOWS|LIGHTFLAG_CAST_STATIC_GEOM_SHADOWS|LIGHTFLAG_CAST_DYNAMIC_GEOM_SHADOWS))
			{
				fwUniqueObjId shadowTrackingId = currentVisibleLight.GetShadowTrackingId();
				if(shadowTrackingId!=0)
				{
					bool visibleLastFrame = g_LightsVisibilityVisibleLastFrameOutput[nBufferID].IsSet(lightIndex);

					shadowIndex = CParaboloidShadow::GetLightShadowIndex(currentVisibleLight, visibleLastFrame);

					// was the light not visible last frame? if so, just go ahead and render it normally
					if (!visibleLastFrame && !Lights::m_bRenderShadowedLightsWithNoShadow)
					{
						// we were not visible last frame.
						if (shadowIndex == -1 )
						{
							// we don't have one. there are a couple of cases to deal with to reduce popping:
							// 1) we don't have any static objects, go ahead and draw without the shadow (hopefully dynamic shadows are small enough to not notice)
							// 2) we do have static objects, skip the light (this helps prevent light shining through walls, etc when first visible or turned on

							if (currentVisibleLight.GetFlag(LIGHTFLAG_CAST_STATIC_GEOM_SHADOWS))
							{
								continue;
							}
						}
						else if ( CParaboloidShadow::IsShadowIndexStaticOnly(shadowIndex) && currentVisibleLight.GetFlag(LIGHTFLAG_CAST_DYNAMIC_GEOM_SHADOWS))
						{
							// we have a shadow, but it's static only and we also need dynamic
							// 1) if we are not in a camera cut situation, it may be better to wait a frame and hope for a dynamic shadow too 
							//     it is a very common case when opening or even just standing next to a door.
							//	   the first frame the door starts to open a light on the other side will pop on. if the door was open before, it will have  a static shadow and render, but the door will be missing
							// 2) is we're in a camera cut situation, it may be better to use the static buffer than nothing (prevents shadows through walls, etc)
							if (!cameraCut)
							{
								continue;
							}
						}
					}				
					//	Displayf("Light %d ((%4.0f, %4.0f, %4.0f), ShadowTrackingId = 0x%08x, shadowIndex = %d, shadow type = %s, visible last frame = %s",lightIndex, currentVisibleLight.GetPosition().x, currentVisibleLight.GetPosition().y, currentVisibleLight.GetPosition().z, shadowTrackingId, shadowIndex,
					//				shadowType==DEFERRED_SHADOWTYPE_POINT?"POINT":(shadowType==DEFERRED_SHADOWTYPE_HEMISPHERE?"HEMISPHERE":"SPOT"), visibleLastFrame?"true":"false");
				}
			}

			//Keep a check to see if there are any lights with volumes in it
			if(currentVisibleLight.GetFlags() & LIGHTFLAG_DRAW_VOLUME)
			{
				m_bAnyVolumeLightsPresent = true;
				if(shadowIndex > -1)
				{
					m_numShadowedVolumetricLights++;
				}
				if(currentVisibleLight.GetExtraFlag(EXTRA_LIGHTFLAG_FORCE_LOWRES_VOLUME))
				{
					m_bIsAnyLowResVolumeLightsPresent = true;
				}
			}

			if (g_LightsVisibilityCamInsideOutput[nBufferID].IsSet(lightIndex))  // is the camera inside any parts of the light?
			{
				size += 100000.0f; // move all the inside ones together at the end (this make debugging easier, but since we make two passes over the list, it in not strictly necessary)
				m_bIsCamInsideAnyLights = true;
				m_insideSortedLights.Append().Set(lightIndex, size, shadowIndex, currentVisibleLight.GetInteriorLocation().GetAsUint32());
			}
			else  // if camera is outside outsideLightList
			{
				m_outsideSortedLights.Append().Set(lightIndex, size, shadowIndex, currentVisibleLight.GetInteriorLocation().GetAsUint32());
			}
		}

		Assertf((m_numShadowedVolumetricLights==0 || m_bAnyVolumeLightsPresent), "Invalid Shadowed volumetric light count = %d when volume light present flag = %s", m_numShadowedVolumetricLights, (m_bAnyVolumeLightsPresent?"true":"false") ); 
	}

private:

	u16 m_numShadowedVolumetricLights;
	bool m_bAnyVolumeLightsPresent;
	bool m_bIsCamInsideAnyLights;
	bool m_bIsAnyLowResVolumeLightsPresent;

	SortedLightArray m_outsideSortedLights;  
	SortedLightArray m_insideSortedLights;    
};

static CSortedVisibleLights g_SortedVisibleLights;
	

// ----------------------------------------------------------------------------------------------- //
static sysCriticalSectionToken g_SetRenderLightsCS;
// =============================================================================================== //
// FUNCTIONS
// =============================================================================================== //

static void SetRenderLights(const u32 lightBufferIdx, const u32 lightShaftBufferIdx, bool cameraCut)
{
	//Using a critical section in here just in case a couple of threads reaches here at the same time
	sysCriticalSection lock(g_SetRenderLightsCS);

	if (s_LightsAddedToDrawList[lightBufferIdx])
		return;

	PF_START(GetLightsInArea);
	PF_PUSH_MARKER("Lights::UseLightsInArea");
	// Copy Lights to the local array for working with the light updating

	Lights::m_renderLights = m_sceneLightsBuffer[lightBufferIdx];
	Lights::m_numRenderLights = m_numSceneLightsBuf[lightBufferIdx];
	Lights::m_renderLightShaft = m_lightShaftsBuffer[lightShaftBufferIdx];
	Lights::m_numRenderLightShafts = m_numLightShaftsBuf[lightShaftBufferIdx];

	// Setup fast lighting tree
	g_FastLights.AddLights( Lights::m_renderLights, Lights::m_numRenderLights, FAST_CULL_LIGHTS_TREE_THRESHOLD );
	
	//Setup visibile lights on the render thread
	Lights::SetupRenderThreadVisibleLightLists(cameraCut);
	
	PF_POP_MARKER();
	PF_STOP(GetLightsInArea);
	
	s_LightsAddedToDrawList[lightBufferIdx] = true;
}

// ----------------------------------------------------------------------------------------------- //

void Lights::GetBound(const CLightSource& lgt, spdAABB& bound, float nearPlaneDist, eDeferredShadowType shadowType)
{
	ASSERT_ONLY(AssertIsValid( lgt ));

	// Do the same adjust done in shaders
	const float extraRadius =  lgt.GetRadius() * EXTEND_LIGHT(lgt.GetType()) + nearPlaneDist;
	const float radius = lgt.GetRadius() + extraRadius;

	if (lgt.GetType() == LIGHT_TYPE_SPOT && lgt.GetConeCosOuterAngle() >= 0.01f && shadowType!=DEFERRED_SHADOWTYPE_HEMISPHERE) // hemispheres lights (which can be less than 90 degrees) currently render with more of the sphere, so we need to use that bounds or we end up inside the light when this says we're outside
	{
		// We make the cone bigger at both ends
		// NOTE: to make this correctly extra wide by extraRadius, we need to back up more than extraRadius, base on angle, etc.
		//		 hopefully this is close enough.
		const Vector3 pos = lgt.GetPosition() - lgt.GetDirection() * extraRadius;
		bound = fwBoxFromCone(pos, radius + extraRadius, lgt.GetDirection(), lgt.GetConeOuterAngle() * DtoR);
	}
	else if (lgt.GetType() == LIGHT_TYPE_CAPSULE && lgt.GetCapsuleExtent() > 0.0)
	{
		const Vector3 pointA = lgt.GetPosition() + (lgt.GetDirection() * (lgt.GetCapsuleExtent() * 0.5f));
		const Vector3 pointB = lgt.GetPosition() - (lgt.GetDirection() * (lgt.GetCapsuleExtent() * 0.5f));

		ScalarV vRadius = ScalarV(radius);
		const spdSphere capSphereA = spdSphere(RCC_VEC3V(pointA), vRadius);
		const spdSphere capSphereB = spdSphere(RCC_VEC3V(pointB), vRadius);

		bound.Invalidate();
		bound.GrowSphere(capSphereA);
		bound.GrowSphere(capSphereB);
	}
	else
	{
		bound = spdAABB(spdSphere(VECTOR3_TO_VEC3V(lgt.GetPosition()), ScalarV(radius)));
	}
}
// ----------------------------------------------------------------------------------------------- //

namespace rage { extern u32 g_AllowVertexBufferVramLocks; };

// ----------------------------------------------------------------------------------------------- //

static __THREAD bool g_AllowCachedVolumeVertexBuffer = false;
static __THREAD CVolumeVertexBuffer* g_CachedVolumeVertexBuffer = NULL;

void CVolumeVertexBuffer::GenerateShaftVolumeVB(int steps, int count)
{
	const bool bReadWrite = true;
	const bool bDynamic = false;
	m_vtxNum = count;
	grcFvf fvf;
	fvf.SetPosChannel(TRUE, grcFvf::grcdsFloat3);

	m_vtxDcl = grmModelFactory::BuildDeclarator(&fvf, NULL, NULL, NULL);
	m_vtxBuf = grcVertexBuffer::Create(m_vtxNum, fvf, bReadWrite, bDynamic, NULL);
	m_vtxBuf->LockRW();

	float* ptr=(float*)m_vtxBuf->GetLockPtr();

	for (int yy=0;yy<steps;yy++)
	{
		for (int xx=0;xx<steps;xx++)
		{
			Vector3 p0=Vector3((xx+0.0f)/steps, (yy+0.0f)/steps, 0.0f);
			Vector3 p1=Vector3((xx+0.0f)/steps, (yy+1.0f)/steps, 0.0f);

			Vector3 p2=Vector3((xx+1.0f)/steps, (yy+0.0f)/steps, 0.0f);
			Vector3 p3=Vector3((xx+1.0f)/steps, (yy+1.0f)/steps, 0.0f);

			if (xx==0)
			{
				*(ptr++)=p0.x;*(ptr++)=p0.y;*(ptr++)=0.0f;
				*(ptr++)=p0.x;*(ptr++)=p0.y;*(ptr++)=0.0f;
				*(ptr++)=p1.x;*(ptr++)=p1.y;*(ptr++)=0.0f;

				*(ptr++)=p0.x;*(ptr++)=p0.y;*(ptr++)=1.0f;
				*(ptr++)=p1.x;*(ptr++)=p1.y;*(ptr++)=1.0f;
			}

			*(ptr++)=p2.x;*(ptr++)=p2.y;*(ptr++)=1.0f;
			*(ptr++)=p3.x;*(ptr++)=p3.y;*(ptr++)=1.0f;

			if (xx==(steps-1))
			{
				*(ptr++)=p2.x;*(ptr++)=p2.y;*(ptr++)=0.0f;
				*(ptr++)=p3.x;*(ptr++)=p3.y;*(ptr++)=0.0f;
				*(ptr++)=p3.x;*(ptr++)=p3.y;*(ptr++)=0.0f;
			}
		}
	}

	for (int xx=0;xx<steps;xx++)
	{
		for (int yy=0;yy<steps;yy++)
		{
			Vector3 p0=Vector3((xx+0.0f)/steps, (yy+0.0f)/steps, 0.0f);
			Vector3 p1=Vector3((xx+1.0f)/steps, (yy+0.0f)/steps, 0.0f);

			Vector3 p2=Vector3((xx+0.0f)/steps, (yy+1.0f)/steps, 0.0f);
			Vector3 p3=Vector3((xx+1.0f)/steps, (yy+1.0f)/steps, 0.0f);

			if (yy==0)
			{
				*(ptr++)=p0.x;*(ptr++)=p0.y;*(ptr++)=1.0f;
				*(ptr++)=p0.x;*(ptr++)=p0.y;*(ptr++)=1.0f;
				*(ptr++)=p1.x;*(ptr++)=p1.y;*(ptr++)=1.0f;

				*(ptr++)=p0.x;*(ptr++)=p0.y;*(ptr++)=0.0f;
				*(ptr++)=p1.x;*(ptr++)=p1.y;*(ptr++)=0.0f;
			}

			*(ptr++)=p2.x;*(ptr++)=p2.y;*(ptr++)=0.0f;
			*(ptr++)=p3.x;*(ptr++)=p3.y;*(ptr++)=0.0f;

			if (yy==(steps-1))
			{
				*(ptr++)=p2.x;*(ptr++)=p2.y;*(ptr++)=1.0f;
				*(ptr++)=p3.x;*(ptr++)=p3.y;*(ptr++)=1.0f;
				*(ptr++)=p3.x;*(ptr++)=p3.y;*(ptr++)=1.0f;
			}
		}
	}

#if __ASSERT
	const int numVerts = ptrdiff_t_to_int(ptr - (float*)m_vtxBuf->GetLockPtr())/3;
	Assertf(numVerts == count, "CreateShaftVolumeVB(steps=%d): numVerts=%d, expected %d", steps, numVerts, m_vtxNum);
#endif // __ASSERT

	m_vtxBuf->UnlockRW();
	m_vtxBuf->MakeReadOnly();
}

// ----------------------------------------------------------------------------------------------- //

void CVolumeVertexBuffer::GeneratePointSpotVolumeVB(int steps, int stacks)
{
	grcFvf fvf;
	fvf.SetPosChannel(TRUE, grcFvf::grcdsHalf4);

	const bool bReadWrite = true;
	const bool bDynamic = false;

	m_vtxNum = PointSpotVolumeNumVerts(steps, stacks);
	m_vtxDcl = grmModelFactory::BuildDeclarator(&fvf, NULL, NULL, NULL);
	m_vtxBuf = grcVertexBuffer::Create(m_vtxNum, fvf, bReadWrite, bDynamic, NULL);

	grcVertexBufferEditor vbEditor(m_vtxBuf);
	s32 vertIndex = 0;

	for (int face=0;face<8;face++)
	{
		const bool bConeSide=(face>=4&&stacks>0);

		Vector3 v;
		v.x=(face&0x1)?1.0f:-1.0f;
		v.y=(face&0x2)?1.0f:-1.0f;
		v.z=(face&0x4)?1.0f:-1.0f;

		int flag=1;
		if (v.x!=v.y) flag=1-flag;
		if (v.z<0) flag=1-flag;

		Vector3 A=(flag==1)?Vector3(v.x,0,0):Vector3(0,v.y,0);
		Vector3 B=(flag==1)?Vector3(0,v.y,0):Vector3(v.x,0,0);
		Vector3 C=Vector3(0,0,v.z);

		int ysteps=steps;

		if (bConeSide)
		{
			ysteps=stacks;
		}

		for (int yy=0;yy<ysteps;yy++)
		{
			float g0=(yy+0.0f)/(ysteps*1.0f);
			float g1=(yy+1.0f)/(ysteps*1.0f);

			Vector3 A0=A+((C-A)*g0);
			Vector3 B0=B+((C-B)*g0);

			Vector3 A1=A+((C-A)*g1);
			Vector3 B1=B+((C-B)*g1);

			int xsteps=steps-yy;

			if (bConeSide)
			{
				xsteps=steps;
			}

			for (int xx=0;xx<xsteps;xx++)
			{
				float f0=(xx+0.0f)/(xsteps*1.0f);
				float f1=f0;

				if (xsteps>1&&!bConeSide)
				{
					f1=(xx+0.0f)/((xsteps-1)*1.0f);
				}

				Vector3 t0; SetQuantized(t0, A0+((B0-A0)*f0));
				Vector3 t1; SetQuantized(t1, A1+((B1-A1)*f1));

				if (xx==0)
				{
					vbEditor.SetPosition(vertIndex++, t0);
				}
				vbEditor.SetPosition(vertIndex++, t0);
				vbEditor.SetPosition(vertIndex++, t1);

				if (xx==(xsteps-1))
				{
					Vector3 tt; SetQuantized(tt, B0);

					vbEditor.SetPosition(vertIndex++, tt);

					if (bConeSide)
					{
						SetQuantized(tt, B1);
					}
					vbEditor.SetPosition(vertIndex++, tt);
					vbEditor.SetPosition(vertIndex++, tt);
				}
			}
		}
	}

	#if !RSG_DURANGO
		m_vtxBuf->MakeReadOnly();
	#endif

#if __ASSERT
	Assertf(vertIndex == m_vtxNum, "CreatePointSpotVolumeVB(steps=%d,stacks=%d): numVerts=%d, expected %d", steps, stacks, vertIndex, m_vtxNum);
#endif // __ASSERT

}

void CVolumeVertexBuffer::ResetLightMeshCache()
{
	g_CachedVolumeVertexBuffer = NULL;

}

void CVolumeVertexBuffer::BeginVolumeRender()
{
	g_AllowCachedVolumeVertexBuffer = true;
	ResetLightMeshCache();
}

void CVolumeVertexBuffer::EndVolumeRender()
{
	g_AllowCachedVolumeVertexBuffer = false;
	GRCDEVICE.ClearStreamSource(0);
	ResetLightMeshCache();
}

void CVolumeVertexBuffer::Draw()
{
	if(!g_AllowCachedVolumeVertexBuffer || this != g_CachedVolumeVertexBuffer)
	{
		GRCDEVICE.SetVertexDeclaration(m_vtxDcl);
		GRCDEVICE.SetStreamSource(0, *m_vtxBuf, 0, m_vtxBuf->GetVertexStride());
		if(g_AllowCachedVolumeVertexBuffer)
		{
			g_CachedVolumeVertexBuffer = this;
		}
	}
	
	GRCDEVICE.DrawPrimitive(drawTriStrip, 0, m_vtxNum);
}
// ----------------------------------------------------------------------------------------------- //

enum eVolumeVertexBufferType
{
	POINT_VB_LOWDETAIL = 0,
	POINT_VB_MIDDETAIL,
	POINT_VB_HIDETAIL,

	SPOT_VB_LOWDETAIL,
	SPOT_VB_MIDDETAIL,
	SPOT_VB_HIDETAIL,

	VOLUME_VB_TOTAL
};

CVolumeVertexBuffer g_VolumeVertexBuffers[VOLUME_VB_TOTAL];
CVolumeVertexBuffer g_shaftVolumeVertexBuffer;

// ----------------------------------------------------------------------------------------------- //

static void CreatePointAndSpotVolumeVertexBuffers()
{
	// DX11 TODO: Should have a CreateWithData function instead of our lame lock sharing
	// only pos

	// point
	{
		CVolumeVertexBuffer* vb = &g_VolumeVertexBuffers[POINT_VB_LOWDETAIL];
		const int steps = 4;
		const int stacks = 0;

		vb->GeneratePointSpotVolumeVB(steps, stacks);
	}

	// point mid res
	{
		CVolumeVertexBuffer* vb = &g_VolumeVertexBuffers[POINT_VB_MIDDETAIL];
		const int steps = 15;
		const int stacks = 0;

		vb->GeneratePointSpotVolumeVB(steps, stacks);
	}

	// point high res
	{
		CVolumeVertexBuffer* vb = &g_VolumeVertexBuffers[POINT_VB_HIDETAIL];
		const int steps = 30;
		const int stacks = 0;

		vb->GeneratePointSpotVolumeVB(steps, stacks);
	}

	// spot
	{
		CVolumeVertexBuffer* vb = &g_VolumeVertexBuffers[SPOT_VB_LOWDETAIL];
		const int steps = 4;
		const int stacks = 1;

		vb->GeneratePointSpotVolumeVB(steps, stacks);
	}

	// spot mid res
	{
		CVolumeVertexBuffer* vb = &g_VolumeVertexBuffers[SPOT_VB_MIDDETAIL];
		const int steps = 6;
		const int stacks = 1;

		vb->GeneratePointSpotVolumeVB(steps, stacks);
	}

	// spot high res
	{
		CVolumeVertexBuffer* vb = &g_VolumeVertexBuffers[SPOT_VB_HIDETAIL];
		const int steps = 15;
		const int stacks = 6;

		vb->GeneratePointSpotVolumeVB(steps, stacks);
	}
}

// ----------------------------------------------------------------------------------------------- //

static void CreateShaftVolumeVertexBuffers()
{
	// DX11 TODO: Should have a CreateWithData function instead of our lame lock sharing
	// only pos
	grcFvf fvf;
	fvf.SetPosChannel(TRUE, grcFvf::grcdsFloat3);

	// shaft
	{
		CVolumeVertexBuffer* vb = &g_shaftVolumeVertexBuffer;
		const int steps = 1;
		const int numVerts = 20;

		vb->GenerateShaftVolumeVB(steps, numVerts);
	}

	// shaft high res
	//{
	//	CVolumeVertexBuffer* vb = &g_shaftVolumeVertexBufferHighRes;
	//	const int steps = 4;
	//
	//	vb->m_vtxNum = 128;
	//	vb->m_vtxDcl = grmModelFactory::BuildDeclarator(&fvf, NULL, NULL, NULL);
	//	const bool bReadWrite = true;
	//	const bool bDynamic = false;
	//	vb->m_vtxBuf = grcVertexBuffer::Create(vb->m_vtxNum, fvf, bReadWrite, bDynamic, NULL);
	//	vb->m_vtxBuf->Lock();
	//	CreateShaftVolumeVB(vb->m_vtxBuf, steps, vb->m_vtxNum);
	//	vb->m_vtxBuf->UnlockRW();
	//	vb->m_vtxBuf->MakeReadOnly();
	//}
};

// ----------------------------------------------------------------------------------------------- //

void Lights::Init()
{
	USE_MEMBUCKET(MEMBUCKET_RENDER);

	m_currFrameInfo = &lightingData;
	
	lightingData.m_directionalLight.SetCommon(LIGHT_TYPE_DIRECTIONAL, 0, Vector3(0.0, 0.0, 0.0), Vector3(0.0, 0.0, 0.0), 0, 0);
	lightingData.m_directionalLight.SetDirection(Vector3(0,0,-1));

	// black light is a light which has no effect which means
	// we don't have to worry about looping and such in the shader
	memset( &m_blackLight, 0, sizeof( CLightSource ));
	m_blackLight.SetType(LIGHT_TYPE_POINT);

	// Init broken light ring buffer
	lightingData.m_BrokenLightHashesHeadIDX = 0;
	lightingData.m_BrokenLightHashesCount = 0;
	lightingData.m_brokenLightHashes.Resize(MAX_STORED_BROKEN_LIGHTS);

	//@@: location LIGHTS_INIT
	g_FastLights.Init();

	SetUnitUp( Vector3( 0.0f, 0.0f, 1.0f));

	const char *techniquePassNames[kLightPassMax] = {"", "CutOut", "WaterRefractionAlpha"};
	const char *techniqueActiveDigits[kLightsActiveMax] = { "0", "4", "8" };

	for(int pass=0; pass<kLightPassMax; pass++)
	{
		for(int active=0; active<kLightsActiveMax; active++)
		{
			char techniqueName[256];
			char techniqueNameHighQuality[256];
			sprintf(techniqueName, "lightweight%s%s", techniqueActiveDigits[active], techniquePassNames[pass]);
			sprintf(techniqueNameHighQuality, "lightweightHighQuality%s%s", techniqueActiveDigits[active], techniquePassNames[pass]);

			s_LightweightTechniqueGroupId[pass][active] = grmShaderFx::FindTechniqueGroupId(techniqueName);
			s_LightweightHighQualityTechniqueGroupId[pass][active] = grmShaderFx::FindTechniqueGroupId(techniqueNameHighQuality);
			Assert(-1 != s_LightweightTechniqueGroupId[pass][active]);
			Assert(-1 != s_LightweightHighQualityTechniqueGroupId[pass][active]);
		}
	}

	// Yes, we know : we're editing vram vtx buffers...
	g_AllowVertexBufferVramLocks++;
	
	CreatePointAndSpotVolumeVertexBuffers();
	CreateShaftVolumeVertexBuffers();

	// And now, we're done with it!
	g_AllowVertexBufferVramLocks--;
	
	CLightGroup::InitClass();

	// State blocks for scene lights
	grcBlendStateDesc sceneLightsBlendState;
	sceneLightsBlendState.BlendRTDesc[0].BlendEnable = TRUE;
	sceneLightsBlendState.BlendRTDesc[0].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_RGB;
	sceneLightsBlendState.BlendRTDesc[0].DestBlend = grcRSV::BLEND_ONE;
	sceneLightsBlendState.BlendRTDesc[0].SrcBlend = grcRSV::BLEND_ONE;
	sceneLightsBlendState.BlendRTDesc[0].BlendOp = grcRSV::BLENDOP_ADD;
	m_SceneLights_B = grcStateBlock::CreateBlendState(sceneLightsBlendState);
	
	// Directional volume stencil setup
	grcDepthStencilStateDesc stencilSetupDirectionalDepthStencilState;
	stencilSetupDirectionalDepthStencilState.DepthWriteMask = FALSE;
	stencilSetupDirectionalDepthStencilState.DepthEnable = TRUE;
	stencilSetupDirectionalDepthStencilState.DepthFunc = rage::FixupDepthDirection(grcRSV::CMP_LESSEQUAL);
	stencilSetupDirectionalDepthStencilState.StencilEnable = TRUE;	
	stencilSetupDirectionalDepthStencilState.StencilReadMask = LIGHT_STENCIL_READ_MASK_BITS;
	stencilSetupDirectionalDepthStencilState.StencilWriteMask = LIGHT_STENCIL_WRITE_MASK_BITS;
	stencilSetupDirectionalDepthStencilState.FrontFace.StencilFailOp = grcRSV::STENCILOP_KEEP;
	stencilSetupDirectionalDepthStencilState.FrontFace.StencilDepthFailOp = grcRSV::STENCILOP_KEEP;
	stencilSetupDirectionalDepthStencilState.FrontFace.StencilPassOp = grcRSV::STENCILOP_REPLACE;
	stencilSetupDirectionalDepthStencilState.FrontFace.StencilFunc = grcRSV::CMP_LESS; 
	stencilSetupDirectionalDepthStencilState.BackFace = stencilSetupDirectionalDepthStencilState.FrontFace; 
	m_StencilFrontDirectSetup_DS[kLightCameraOutside]= grcStateBlock::CreateDepthStencilState(stencilSetupDirectionalDepthStencilState); 
	
	stencilSetupDirectionalDepthStencilState.FrontFace.StencilFailOp = grcRSV::STENCILOP_ZERO;
	stencilSetupDirectionalDepthStencilState.FrontFace.StencilDepthFailOp = grcRSV::STENCILOP_ZERO;
	stencilSetupDirectionalDepthStencilState.FrontFace.StencilFunc = grcRSV::CMP_EQUAL; 
	stencilSetupDirectionalDepthStencilState.FrontFace.StencilFailOp = grcRSV::STENCILOP_KEEP;
	stencilSetupDirectionalDepthStencilState.FrontFace.StencilDepthFailOp = grcRSV::STENCILOP_KEEP;
	stencilSetupDirectionalDepthStencilState.BackFace = stencilSetupDirectionalDepthStencilState.FrontFace; 
	m_StencilFrontDirectSetup_DS[kLightCameraInside]= grcStateBlock::CreateDepthStencilState(stencilSetupDirectionalDepthStencilState); 

	stencilSetupDirectionalDepthStencilState.DepthFunc = rage::FixupDepthDirection(grcRSV::CMP_GREATER);				// reverse depth test for the back side
	stencilSetupDirectionalDepthStencilState.StencilReadMask = LIGHT_FINAL_PASS_STENCIL_BIT;
	stencilSetupDirectionalDepthStencilState.StencilWriteMask = LIGHT_FINAL_PASS_STENCIL_BIT;
	stencilSetupDirectionalDepthStencilState.FrontFace.StencilFunc = grcRSV::CMP_NOTEQUAL; 
	stencilSetupDirectionalDepthStencilState.FrontFace.StencilFailOp  = grcRSV::STENCILOP_KEEP;
	stencilSetupDirectionalDepthStencilState.FrontFace.StencilDepthFailOp = grcRSV::STENCILOP_KEEP;
	stencilSetupDirectionalDepthStencilState.BackFace = stencilSetupDirectionalDepthStencilState.FrontFace; 
	m_StencilBackDirectSetup_DS[kLightCameraInside] = grcStateBlock::CreateDepthStencilState(stencilSetupDirectionalDepthStencilState);

	stencilSetupDirectionalDepthStencilState.StencilReadMask = LIGHT_FINAL_PASS_STENCIL_BIT;
	stencilSetupDirectionalDepthStencilState.StencilWriteMask = LIGHT_CAMOUTSIDE_BACK_PASS_STENCIL_BIT;
	m_StencilBackDirectSetup_DS[kLightCameraOutside] = grcStateBlock::CreateDepthStencilState(stencilSetupDirectionalDepthStencilState);

	grcDepthStencilStateDesc stencilDirectSetupDSS;
	stencilDirectSetupDSS.DepthWriteMask = FALSE;
	stencilDirectSetupDSS.DepthFunc = rage::FixupDepthDirection(grcRSV::CMP_LESSEQUAL);
	stencilDirectSetupDSS.StencilEnable = TRUE;	
	stencilDirectSetupDSS.StencilWriteMask = LIGHT_STENCIL_WRITE_MASK_BITS;
	stencilDirectSetupDSS.FrontFace.StencilPassOp = grcRSV::STENCILOP_REPLACE;
	stencilDirectSetupDSS.BackFace = stencilDirectSetupDSS.FrontFace;
	m_StencilDirectSetup_DS = grcStateBlock::CreateDepthStencilState(stencilDirectSetupDSS );

	// =======================================================================================================================================
	// Cull plane: DepthStencil state
	// =======================================================================================================================================
	{
		// kLightTwoPassStencil, kLightCullPlaneCamInBack
		grcDepthStencilStateDesc cullPlaneDepthStencilState;
		cullPlaneDepthStencilState.DepthEnable = TRUE;
		cullPlaneDepthStencilState.DepthWriteMask = FALSE;
		cullPlaneDepthStencilState.DepthFunc = rage::FixupDepthDirection(grcRSV::CMP_LESSEQUAL);
		cullPlaneDepthStencilState.StencilReadMask = LIGHT_STENCIL_WRITE_MASK_BITS;
		cullPlaneDepthStencilState.StencilWriteMask = LIGHT_STENCIL_WRITE_MASK_BITS;
		cullPlaneDepthStencilState.FrontFace.StencilFunc = grcRSV::CMP_EQUAL;
		cullPlaneDepthStencilState.FrontFace.StencilFailOp  = grcRSV::STENCILOP_KEEP;
		cullPlaneDepthStencilState.FrontFace.StencilPassOp  = grcRSV::STENCILOP_KEEP;
		cullPlaneDepthStencilState.FrontFace.StencilDepthFailOp = grcRSV::STENCILOP_ZERO;
		cullPlaneDepthStencilState.BackFace = cullPlaneDepthStencilState.FrontFace;
		m_StencilCullPlaneSetup_DS[kLightTwoPassStencil][kLightCullPlaneCamInBack] = grcStateBlock::CreateDepthStencilState(cullPlaneDepthStencilState);

		// kLightOnePassStencil, kLightCullPlaneCamInBack
		cullPlaneDepthStencilState.StencilReadMask = LIGHT_ONE_PASS_STENCIL_BIT;
		cullPlaneDepthStencilState.StencilWriteMask = LIGHT_ONE_PASS_STENCIL_BIT;
		m_StencilCullPlaneSetup_DS[kLightOnePassStencil][kLightCullPlaneCamInBack] = grcStateBlock::CreateDepthStencilState(cullPlaneDepthStencilState);

		// kLightTwoPassStencil, kLightCullPlaneCamInFront
		cullPlaneDepthStencilState.DepthFunc = rage::FixupDepthDirection(grcRSV::CMP_GREATER);
		cullPlaneDepthStencilState.StencilReadMask = LIGHT_STENCIL_WRITE_MASK_BITS;
		cullPlaneDepthStencilState.StencilWriteMask = LIGHT_STENCIL_WRITE_MASK_BITS;
		cullPlaneDepthStencilState.FrontFace.StencilFailOp  = grcRSV::STENCILOP_KEEP;
		cullPlaneDepthStencilState.FrontFace.StencilPassOp  = grcRSV::STENCILOP_KEEP;
		cullPlaneDepthStencilState.FrontFace.StencilDepthFailOp = grcRSV::STENCILOP_ZERO;
		cullPlaneDepthStencilState.BackFace = cullPlaneDepthStencilState.FrontFace;
		m_StencilCullPlaneSetup_DS[kLightTwoPassStencil][kLightCullPlaneCamInFront] = grcStateBlock::CreateDepthStencilState(cullPlaneDepthStencilState);

		// kLightOnePassStencil, kLightCullPlaneCamInFront
		cullPlaneDepthStencilState.StencilReadMask = LIGHT_ONE_PASS_STENCIL_BIT;
		cullPlaneDepthStencilState.StencilWriteMask = LIGHT_ONE_PASS_STENCIL_BIT;
		m_StencilCullPlaneSetup_DS[kLightOnePassStencil][kLightCullPlaneCamInFront] = grcStateBlock::CreateDepthStencilState(cullPlaneDepthStencilState);
	}


	// =======================================================================================================================================
	// Light volume: Blend state
	// =======================================================================================================================================
	{
		grcBlendStateDesc twoSidedStencilBlendState;
		twoSidedStencilBlendState.BlendRTDesc[0].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_NONE;
		m_StencilSetup_B = grcStateBlock::CreateBlendState(twoSidedStencilBlendState);
	}


	// =======================================================================================================================================
	// Light volume Setup: DepthStencil state (used to setup stencil prior to lighting pass)
	// =======================================================================================================================================
	{
		// 2-Pass, Front face, kLightCameraOutside
		grcDepthStencilStateDesc stencilSetupDepthStencilState;
#if (RSG_ORBIS || RSG_DURANGO) && DEPTH_BOUNDS_SUPPORT
		stencilSetupDepthStencilState.DepthBoundsEnable = true;
#endif

		stencilSetupDepthStencilState.DepthWriteMask = FALSE;
		stencilSetupDepthStencilState.DepthEnable = TRUE;
		stencilSetupDepthStencilState.DepthFunc = rage::FixupDepthDirection(grcRSV::CMP_LESSEQUAL);
		stencilSetupDepthStencilState.StencilEnable = TRUE;	
		stencilSetupDepthStencilState.StencilReadMask = LIGHT_STENCIL_READ_MASK_BITS;
		stencilSetupDepthStencilState.StencilWriteMask = LIGHT_STENCIL_WRITE_MASK_BITS;
		stencilSetupDepthStencilState.FrontFace.StencilFailOp = grcRSV::STENCILOP_ZERO;
		stencilSetupDepthStencilState.FrontFace.StencilDepthFailOp = grcRSV::STENCILOP_ZERO;
		stencilSetupDepthStencilState.FrontFace.StencilPassOp = grcRSV::STENCILOP_REPLACE;
		stencilSetupDepthStencilState.FrontFace.StencilFunc = grcRSV::CMP_LESS; 
		stencilSetupDepthStencilState.BackFace = stencilSetupDepthStencilState.FrontFace; 
		m_StencilFrontSetup_DS[kLightCameraOutside] = grcStateBlock::CreateDepthStencilState(stencilSetupDepthStencilState); 

		// 2-Pass, Front face, kLightCameraInside
		stencilSetupDepthStencilState.FrontFace.StencilFailOp = grcRSV::STENCILOP_ZERO;
		stencilSetupDepthStencilState.FrontFace.StencilDepthFailOp = grcRSV::STENCILOP_ZERO;
		stencilSetupDepthStencilState.FrontFace.StencilFunc = grcRSV::CMP_EQUAL; 
		stencilSetupDepthStencilState.BackFace = stencilSetupDepthStencilState.FrontFace; 
		m_StencilFrontSetup_DS[kLightCameraInside] = grcStateBlock::CreateDepthStencilState(stencilSetupDepthStencilState); 
		
		// 2-Pass, Back face, kLightStencilAllSurfaces
		stencilSetupDepthStencilState.FrontFace.StencilFailOp = grcRSV::STENCILOP_ZERO;
		stencilSetupDepthStencilState.FrontFace.StencilDepthFailOp = grcRSV::STENCILOP_ZERO;
		stencilSetupDepthStencilState.DepthFunc = rage::FixupDepthDirection(grcRSV::CMP_GREATER);				// reverse depth test for the back side
		stencilSetupDepthStencilState.StencilReadMask = LIGHT_INTERIOR_STENCIL_BIT; // we are sometimes interested in the interior bit for the back side test 
		stencilSetupDepthStencilState.FrontFace.StencilFunc = grcRSV::CMP_ALWAYS; 
		stencilSetupDepthStencilState.BackFace = stencilSetupDepthStencilState.FrontFace; 
		m_StencilBackSetup_DS[kLightStencilAllSurfaces] = grcStateBlock::CreateDepthStencilState(stencilSetupDepthStencilState);	

		#if USE_STENCIL_FOR_INTERIOR_EXTERIOR_CHECK
			stencilSetupDepthStencilState.FrontFace.StencilFunc = grcRSV::CMP_EQUAL; 
			stencilSetupDepthStencilState.BackFace = stencilSetupDepthStencilState.FrontFace; 
			m_StencilBackSetup_DS[kLightStencilInteriorOnly] = grcStateBlock::CreateDepthStencilState(stencilSetupDepthStencilState);	

			stencilSetupDepthStencilState.FrontFace.StencilFunc = grcRSV::CMP_NOTEQUAL; 
			stencilSetupDepthStencilState.BackFace = stencilSetupDepthStencilState.FrontFace; 
			m_StencilBackSetup_DS[kLightStencilExteriorOnly] = grcStateBlock::CreateDepthStencilState(stencilSetupDepthStencilState);	
		#endif

		// 1-Pass stencil setup: kLightCameraOutside
		grcDepthStencilStateDesc stencilSetupSinglePassDepthStencilState;
#if (RSG_ORBIS || RSG_DURANGO) && DEPTH_BOUNDS_SUPPORT
		stencilSetupSinglePassDepthStencilState.DepthBoundsEnable = true;		
#endif
		stencilSetupSinglePassDepthStencilState.DepthEnable = TRUE;
		stencilSetupSinglePassDepthStencilState.DepthWriteMask = FALSE;
		stencilSetupSinglePassDepthStencilState.DepthFunc = rage::FixupDepthDirection(grcRSV::CMP_LESSEQUAL);
		stencilSetupSinglePassDepthStencilState.StencilEnable = TRUE;	
		stencilSetupSinglePassDepthStencilState.StencilReadMask = 0x0;//LIGHT_ONE_PASS_STENCIL_BIT;
		stencilSetupSinglePassDepthStencilState.StencilWriteMask = LIGHT_ONE_PASS_STENCIL_BIT;
		stencilSetupSinglePassDepthStencilState.FrontFace.StencilFailOp = grcRSV::STENCILOP_KEEP; // Font/Back face culling are actually swapped
		stencilSetupSinglePassDepthStencilState.FrontFace.StencilDepthFailOp = grcRSV::STENCILOP_REPLACE;
		stencilSetupSinglePassDepthStencilState.FrontFace.StencilPassOp = grcRSV::STENCILOP_KEEP;
		stencilSetupSinglePassDepthStencilState.FrontFace.StencilFunc = grcRSV::CMP_ALWAYS; 
		stencilSetupSinglePassDepthStencilState.BackFace = stencilSetupSinglePassDepthStencilState.FrontFace;
		m_StencilSinglePassSetup_DS[kLightCameraOutside] = grcStateBlock::CreateDepthStencilState(stencilSetupSinglePassDepthStencilState);	

		// Single-pass stencil setup: kLightCameraInside
		stencilSetupSinglePassDepthStencilState.DepthEnable = TRUE;
		stencilSetupSinglePassDepthStencilState.DepthWriteMask = FALSE;
		stencilSetupSinglePassDepthStencilState.DepthFunc = rage::FixupDepthDirection(grcRSV::CMP_LESSEQUAL);
		stencilSetupSinglePassDepthStencilState.StencilEnable = TRUE;	
		stencilSetupSinglePassDepthStencilState.StencilReadMask = 0x0;//LIGHT_ONE_PASS_STENCIL_BIT;
		stencilSetupSinglePassDepthStencilState.StencilWriteMask = LIGHT_ONE_PASS_STENCIL_BIT;
		stencilSetupSinglePassDepthStencilState.FrontFace.StencilFailOp = grcRSV::STENCILOP_KEEP; // Font/Back face culling are actually swapped
		stencilSetupSinglePassDepthStencilState.FrontFace.StencilDepthFailOp = grcRSV::STENCILOP_REPLACE;
		stencilSetupSinglePassDepthStencilState.FrontFace.StencilPassOp = grcRSV::STENCILOP_ZERO;
		stencilSetupSinglePassDepthStencilState.FrontFace.StencilFunc = grcRSV::CMP_ALWAYS; 
		stencilSetupSinglePassDepthStencilState.BackFace = stencilSetupSinglePassDepthStencilState.FrontFace;
		m_StencilSinglePassSetup_DS[kLightCameraInside] = grcStateBlock::CreateDepthStencilState(stencilSetupSinglePassDepthStencilState);	
	}


	// =======================================================================================================================================
	// Light volume Rendering: Rasterizer state (used when we do the actual light pass)
	// =======================================================================================================================================
	{
		// kLightCameraInside, kLightNoDepthBias
		grcRasterizerStateDesc insideVolumeRasterizerState;
		insideVolumeRasterizerState.CullMode = grcRSV::CULL_FRONT;
		m_Volume_R[kLightOnePassStencil][kLightCameraInside][kLightNoDepthBias] = grcStateBlock::CreateRasterizerState(insideVolumeRasterizerState); 
		m_Volume_R[kLightTwoPassStencil][kLightCameraInside][kLightNoDepthBias] = grcStateBlock::CreateRasterizerState(insideVolumeRasterizerState);
		m_Volume_R[kLightNoStencil][kLightCameraInside][kLightNoDepthBias] = grcStateBlock::CreateRasterizerState(insideVolumeRasterizerState);

		// kLightCameraInside, kLightDepthBiased
		insideVolumeRasterizerState.DepthBiasDX9 = __PS3 || __WIN32PC || RSG_DURANGO || RSG_ORBIS ? 0.0005f : -0.0005f;
		m_Volume_R[kLightOnePassStencil][kLightCameraInside][kLightDepthBiased] = grcStateBlock::CreateRasterizerState(insideVolumeRasterizerState); 
		m_Volume_R[kLightTwoPassStencil][kLightCameraInside][kLightDepthBiased] = grcStateBlock::CreateRasterizerState(insideVolumeRasterizerState);
		m_Volume_R[kLightNoStencil][kLightCameraInside][kLightDepthBiased] = grcStateBlock::CreateRasterizerState(insideVolumeRasterizerState);

		// kLightCameraOutside, kLightNoDepthBias
		grcRasterizerStateDesc outsideVolumeRasterizerState;
		outsideVolumeRasterizerState.CullMode = grcRSV::CULL_BACK;
		m_Volume_R[kLightTwoPassStencil][kLightCameraOutside][kLightNoDepthBias] = grcStateBlock::CreateRasterizerState(outsideVolumeRasterizerState);
		m_Volume_R[kLightOnePassStencil][kLightCameraOutside][kLightNoDepthBias] = grcStateBlock::CreateRasterizerState(outsideVolumeRasterizerState);
		m_Volume_R[kLightNoStencil][kLightCameraOutside][kLightNoDepthBias] = grcStateBlock::CreateRasterizerState(outsideVolumeRasterizerState);

		// kLightCameraOutside, kLightDepthBiased
		outsideVolumeRasterizerState.DepthBiasDX9 = __PS3 || __WIN32PC || RSG_DURANGO || RSG_ORBIS? -0.0005f : 0.0005f;
		m_Volume_R[kLightTwoPassStencil][kLightCameraOutside][kLightDepthBiased] = grcStateBlock::CreateRasterizerState(outsideVolumeRasterizerState);
		m_Volume_R[kLightOnePassStencil][kLightCameraOutside][kLightDepthBiased] = grcStateBlock::CreateRasterizerState(outsideVolumeRasterizerState);
		m_Volume_R[kLightNoStencil][kLightCameraOutside][kLightDepthBiased] = grcStateBlock::CreateRasterizerState(outsideVolumeRasterizerState);
	}
	

	// =======================================================================================================================================
	// Light volume Rendering: DepthStencil state (used when we do the actual light pass)
	// =======================================================================================================================================
	{
		// kLightCameraInside, kLightNoStencil
		grcDepthStencilStateDesc insideVolumeDepthStencilState; 
		insideVolumeDepthStencilState.DepthWriteMask = FALSE;
		insideVolumeDepthStencilState.DepthEnable = TRUE;
		insideVolumeDepthStencilState.DepthFunc = rage::FixupDepthDirection(grcRSV::CMP_GREATER);
		m_Volume_DS[kLightNoStencil][kLightCameraInside][kLightDepthBoundsDisabled] = grcStateBlock::CreateDepthStencilState(insideVolumeDepthStencilState);
		DEPTHBOUNDS_STATEBLOCK_ONLY( insideVolumeDepthStencilState.DepthBoundsEnable = true );		
		m_Volume_DS[kLightNoStencil][kLightCameraInside][kLightDepthBoundsEnabled] = grcStateBlock::CreateDepthStencilState(insideVolumeDepthStencilState);
		DEPTHBOUNDS_STATEBLOCK_ONLY( insideVolumeDepthStencilState.DepthBoundsEnable = false );

		// kLightCameraInside, kLightTwoPassStencil
		insideVolumeDepthStencilState.StencilEnable = TRUE;	
		insideVolumeDepthStencilState.StencilReadMask = LIGHT_STENCIL_READ_MASK_BITS;
		insideVolumeDepthStencilState.StencilWriteMask = 0x00;
		insideVolumeDepthStencilState.FrontFace.StencilPassOp = grcRSV::STENCILOP_KEEP;
		insideVolumeDepthStencilState.FrontFace.StencilFunc = grcRSV::CMP_EQUAL;
		insideVolumeDepthStencilState.FrontFace.StencilFailOp = grcRSV::STENCILOP_KEEP;
		insideVolumeDepthStencilState.FrontFace.StencilDepthFailOp = grcRSV::STENCILOP_KEEP;
		insideVolumeDepthStencilState.BackFace = insideVolumeDepthStencilState.FrontFace; 
		m_Volume_DS[kLightTwoPassStencil][kLightCameraInside][kLightDepthBoundsDisabled] = grcStateBlock::CreateDepthStencilState(insideVolumeDepthStencilState);
		DEPTHBOUNDS_STATEBLOCK_ONLY( insideVolumeDepthStencilState.DepthBoundsEnable = true );		
		m_Volume_DS[kLightTwoPassStencil][kLightCameraInside][kLightDepthBoundsEnabled] = grcStateBlock::CreateDepthStencilState(insideVolumeDepthStencilState);
		DEPTHBOUNDS_STATEBLOCK_ONLY( insideVolumeDepthStencilState.DepthBoundsEnable = false );

		// kLightCameraInside, kLightOnePassStencil
		grcDepthStencilStateDesc singlePassInsideVolumeDepthStencilState;
		singlePassInsideVolumeDepthStencilState.DepthWriteMask = FALSE;
		singlePassInsideVolumeDepthStencilState.DepthEnable = TRUE;
		singlePassInsideVolumeDepthStencilState.DepthFunc = rage::FixupDepthDirection(grcRSV::CMP_GREATER);
		singlePassInsideVolumeDepthStencilState.StencilEnable = TRUE;	
		singlePassInsideVolumeDepthStencilState.StencilReadMask = LIGHT_ONE_PASS_STENCIL_BIT;
		singlePassInsideVolumeDepthStencilState.StencilWriteMask = 0x0;
		singlePassInsideVolumeDepthStencilState.FrontFace.StencilFunc = grcRSV::CMP_EQUAL;
		singlePassInsideVolumeDepthStencilState.FrontFace.StencilPassOp = grcRSV::STENCILOP_KEEP;
		singlePassInsideVolumeDepthStencilState.FrontFace.StencilFailOp = grcRSV::STENCILOP_KEEP;
		singlePassInsideVolumeDepthStencilState.FrontFace.StencilDepthFailOp = grcRSV::STENCILOP_KEEP;
		singlePassInsideVolumeDepthStencilState.BackFace = singlePassInsideVolumeDepthStencilState.FrontFace; // Front/Back face culling is swapped, careful!
		m_Volume_DS[kLightOnePassStencil][kLightCameraInside][kLightDepthBoundsDisabled] = grcStateBlock::CreateDepthStencilState(singlePassInsideVolumeDepthStencilState);
		DEPTHBOUNDS_STATEBLOCK_ONLY( singlePassInsideVolumeDepthStencilState.DepthBoundsEnable = true );		
		m_Volume_DS[kLightOnePassStencil][kLightCameraInside][kLightDepthBoundsEnabled] = grcStateBlock::CreateDepthStencilState(singlePassInsideVolumeDepthStencilState);
		DEPTHBOUNDS_STATEBLOCK_ONLY( singlePassInsideVolumeDepthStencilState.DepthBoundsEnable = false );
		

		// kLightCameraOutside, kLightNoStencil
		grcDepthStencilStateDesc outsideVolumeDepthStencilState;
		outsideVolumeDepthStencilState.DepthWriteMask = FALSE;
		outsideVolumeDepthStencilState.DepthEnable = TRUE;
		outsideVolumeDepthStencilState.DepthFunc = rage::FixupDepthDirection(grcRSV::CMP_LESSEQUAL);
		m_Volume_DS[kLightNoStencil][kLightCameraOutside][kLightDepthBoundsDisabled] = grcStateBlock::CreateDepthStencilState(outsideVolumeDepthStencilState);
		DEPTHBOUNDS_STATEBLOCK_ONLY( outsideVolumeDepthStencilState.DepthBoundsEnable = true );		
		m_Volume_DS[kLightNoStencil][kLightCameraOutside][kLightDepthBoundsEnabled] = grcStateBlock::CreateDepthStencilState(outsideVolumeDepthStencilState);
		DEPTHBOUNDS_STATEBLOCK_ONLY( outsideVolumeDepthStencilState.DepthBoundsEnable = false );		

	
		// kLightCameraOutside, kLightTwoPassStencil
		outsideVolumeDepthStencilState.StencilEnable = insideVolumeDepthStencilState.StencilEnable;
		outsideVolumeDepthStencilState.StencilReadMask = insideVolumeDepthStencilState.StencilReadMask;
		outsideVolumeDepthStencilState.StencilWriteMask = insideVolumeDepthStencilState.StencilWriteMask;
		outsideVolumeDepthStencilState.FrontFace = insideVolumeDepthStencilState.FrontFace;
		outsideVolumeDepthStencilState.BackFace = insideVolumeDepthStencilState.BackFace;
		m_Volume_DS[kLightTwoPassStencil][kLightCameraOutside][kLightDepthBoundsDisabled] = grcStateBlock::CreateDepthStencilState(outsideVolumeDepthStencilState);  
		DEPTHBOUNDS_STATEBLOCK_ONLY( outsideVolumeDepthStencilState.DepthBoundsEnable = true );
		m_Volume_DS[kLightTwoPassStencil][kLightCameraOutside][kLightDepthBoundsEnabled] = grcStateBlock::CreateDepthStencilState(outsideVolumeDepthStencilState);  
		DEPTHBOUNDS_STATEBLOCK_ONLY( outsideVolumeDepthStencilState.DepthBoundsEnable = false );

		// kLightCameraOutside, kLightOnePassStencil
		grcDepthStencilStateDesc singlePassOutsideVolumeDepthStencilState;
		singlePassOutsideVolumeDepthStencilState.DepthWriteMask = FALSE;
		singlePassOutsideVolumeDepthStencilState.DepthEnable = TRUE;
		singlePassOutsideVolumeDepthStencilState.DepthFunc = rage::FixupDepthDirection(grcRSV::CMP_LESSEQUAL);
		singlePassOutsideVolumeDepthStencilState.StencilEnable = TRUE;
		singlePassOutsideVolumeDepthStencilState.StencilReadMask = singlePassInsideVolumeDepthStencilState.StencilReadMask;
		singlePassOutsideVolumeDepthStencilState.StencilWriteMask = singlePassInsideVolumeDepthStencilState.StencilWriteMask;
		singlePassOutsideVolumeDepthStencilState.FrontFace.StencilFunc = grcRSV::CMP_EQUAL;
		singlePassOutsideVolumeDepthStencilState.FrontFace.StencilPassOp = grcRSV::STENCILOP_KEEP;
		singlePassOutsideVolumeDepthStencilState.FrontFace.StencilFailOp = grcRSV::STENCILOP_KEEP;
		singlePassOutsideVolumeDepthStencilState.FrontFace.StencilDepthFailOp = grcRSV::STENCILOP_KEEP;
		singlePassOutsideVolumeDepthStencilState.BackFace = singlePassOutsideVolumeDepthStencilState.FrontFace; // Front/Back face culling is swapped, careful!
		m_Volume_DS[kLightOnePassStencil][kLightCameraOutside][kLightDepthBoundsDisabled] = grcStateBlock::CreateDepthStencilState(singlePassOutsideVolumeDepthStencilState);  
		DEPTHBOUNDS_STATEBLOCK_ONLY( singlePassOutsideVolumeDepthStencilState.DepthBoundsEnable = true );
		m_Volume_DS[kLightOnePassStencil][kLightCameraOutside][kLightDepthBoundsEnabled] = grcStateBlock::CreateDepthStencilState(singlePassOutsideVolumeDepthStencilState);  
		DEPTHBOUNDS_STATEBLOCK_ONLY( singlePassOutsideVolumeDepthStencilState.DepthBoundsEnable = false );


		// ped only lights:
		grcDepthStencilStateDesc pedOnlyDepthStencilState;
		pedOnlyDepthStencilState.DepthEnable					= TRUE;
		pedOnlyDepthStencilState.DepthWriteMask					= FALSE;
		pedOnlyDepthStencilState.DepthFunc						= rage::FixupDepthDirection(grcRSV::CMP_GREATER);
		pedOnlyDepthStencilState.StencilEnable					= TRUE;
		pedOnlyDepthStencilState.StencilReadMask				= 0x07;
		pedOnlyDepthStencilState.StencilWriteMask				= 0x00;
		pedOnlyDepthStencilState.FrontFace.StencilFunc			= grcRSV::CMP_EQUAL;
		pedOnlyDepthStencilState.FrontFace.StencilPassOp		= grcRSV::STENCILOP_KEEP;
		pedOnlyDepthStencilState.FrontFace.StencilFailOp		= grcRSV::STENCILOP_KEEP;
		pedOnlyDepthStencilState.FrontFace.StencilDepthFailOp	= grcRSV::STENCILOP_KEEP;
		pedOnlyDepthStencilState.BackFace						= pedOnlyDepthStencilState.FrontFace;
		m_StencilPEDOnlyGreater_DS = grcStateBlock::CreateDepthStencilState(pedOnlyDepthStencilState);

		pedOnlyDepthStencilState.DepthFunc						= rage::FixupDepthDirection(grcRSV::CMP_LESSEQUAL);
		m_StencilPEDOnlyLessEqual_DS = grcStateBlock::CreateDepthStencilState(pedOnlyDepthStencilState);
	}
	// =======================================================================================================================================

	// State blocks for ambient scale volumes
	grcBlendStateDesc ambientScaleVolumesBlendState;
	ambientScaleVolumesBlendState.IndependentBlendEnable = TRUE;
	ambientScaleVolumesBlendState.BlendRTDesc[GBUFFER_RT_0].BlendEnable = TRUE;
	ambientScaleVolumesBlendState.BlendRTDesc[GBUFFER_RT_0].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_RGB;
	ambientScaleVolumesBlendState.BlendRTDesc[GBUFFER_RT_0].DestBlend = grcRSV::BLEND_INVSRCALPHA;
	ambientScaleVolumesBlendState.BlendRTDesc[GBUFFER_RT_0].SrcBlend = grcRSV::BLEND_SRCALPHA;
	ambientScaleVolumesBlendState.BlendRTDesc[GBUFFER_RT_0].BlendOp = grcRSV::BLENDOP_ADD;
	ambientScaleVolumesBlendState.BlendRTDesc[GBUFFER_RT_1].BlendEnable = FALSE;
	ambientScaleVolumesBlendState.BlendRTDesc[GBUFFER_RT_1].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_NONE;
	ambientScaleVolumesBlendState.BlendRTDesc[GBUFFER_RT_2].BlendEnable = FALSE;
	ambientScaleVolumesBlendState.BlendRTDesc[GBUFFER_RT_2].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_NONE;
	ambientScaleVolumesBlendState.BlendRTDesc[GBUFFER_RT_3].BlendEnable = TRUE;
	ambientScaleVolumesBlendState.BlendRTDesc[GBUFFER_RT_3].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_RED + grcRSV::COLORWRITEENABLE_GREEN;
	ambientScaleVolumesBlendState.BlendRTDesc[GBUFFER_RT_3].DestBlend = grcRSV::BLEND_INVSRCALPHA;
	ambientScaleVolumesBlendState.BlendRTDesc[GBUFFER_RT_3].SrcBlend = grcRSV::BLEND_SRCALPHA;
	ambientScaleVolumesBlendState.BlendRTDesc[GBUFFER_RT_3].BlendOp = grcRSV::BLENDOP_ADD;

	m_AmbientScaleVolumes_B = grcStateBlock::CreateBlendState(ambientScaleVolumesBlendState);

	grcDepthStencilStateDesc ambientScaleVolumesDepthStencilState;
	ambientScaleVolumesDepthStencilState.DepthWriteMask = FALSE;
	ambientScaleVolumesDepthStencilState.DepthEnable = TRUE;
	ambientScaleVolumesDepthStencilState.DepthFunc = rage::FixupDepthDirection(grcRSV::CMP_GREATER);
	ambientScaleVolumesDepthStencilState.StencilEnable = true;
	ambientScaleVolumesDepthStencilState.StencilReadMask = DEFERRED_MATERIAL_TYPE_MASK;
	ambientScaleVolumesDepthStencilState.StencilWriteMask = 0x0;
	ambientScaleVolumesDepthStencilState.FrontFace.StencilFunc = grcRSV::CMP_NOTEQUAL;
	ambientScaleVolumesDepthStencilState.BackFace.StencilFunc = grcRSV::CMP_NOTEQUAL;
	m_AmbientScaleVolumes_DS = grcStateBlock::CreateDepthStencilState(ambientScaleVolumesDepthStencilState);

	grcRasterizerStateDesc ambientScaleVolumesRasterizerState;
	ambientScaleVolumesRasterizerState.CullMode = grcRSV::CULL_CCW;
	m_AmbientScaleVolumes_R = grcStateBlock::CreateRasterizerState(ambientScaleVolumesRasterizerState);

	// State blocks for ped light
	grcDepthStencilStateDesc dirDepthStenciLState;
	dirDepthStenciLState.DepthEnable = FALSE;
	dirDepthStenciLState.DepthWriteMask = FALSE;
	dirDepthStenciLState.DepthFunc = grcRSV::CMP_LESS;
	dirDepthStenciLState.StencilEnable = TRUE;	
	dirDepthStenciLState.StencilWriteMask = 0x00;
	dirDepthStenciLState.StencilReadMask = 0x07;
	dirDepthStenciLState.FrontFace.StencilFunc = grcRSV::CMP_EQUAL;
	dirDepthStenciLState.BackFace.StencilFunc = grcRSV::CMP_EQUAL; 
	m_pedLight_DS = grcStateBlock::CreateDepthStencilState(dirDepthStenciLState); 

	// State blocks for volume FX
	grcBlendStateDesc volumeFxBlendState;
	volumeFxBlendState.BlendRTDesc[0].BlendEnable = TRUE;
	volumeFxBlendState.BlendRTDesc[0].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_RGB;
	volumeFxBlendState.BlendRTDesc[0].DestBlend = grcRSV::BLEND_ONE;
	volumeFxBlendState.BlendRTDesc[0].SrcBlend = grcRSV::BLEND_ONE;
	volumeFxBlendState.BlendRTDesc[0].BlendOp = grcRSV::BLENDOP_ADD;
	m_VolumeFX_B = grcStateBlock::CreateBlendState(volumeFxBlendState);

	grcDepthStencilStateDesc volumeFxDepthStencilState;
	volumeFxDepthStencilState.DepthEnable = FALSE;
	volumeFxDepthStencilState.DepthWriteMask = FALSE;
	volumeFxDepthStencilState.DepthFunc = grcRSV::CMP_LESS;
	m_VolumeFX_DS = grcStateBlock::CreateDepthStencilState(volumeFxDepthStencilState);

	grcRasterizerStateDesc volumeFxRasterizerState;
	volumeFxRasterizerState.CullMode = grcRSV::CULL_BACK;
	m_VolumeFX_R = grcStateBlock::CreateRasterizerState(volumeFxRasterizerState);

	volumeFxRasterizerState.CullMode = grcRSV::CULL_FRONT;
	m_VolumeFXInside_R = grcStateBlock::CreateRasterizerState(volumeFxRasterizerState);

	#if __BANK
		SetupForceSceneLightFlags(m_sceneShadows);
	#endif // __BANK

	InitVolumeStates();

	#if __PPU
		g_LightsVisibilitySortDependency.Init( FRAG_SPU_CODE(LightsVisibilitySort), 0, 0 );
	#else
		g_LightsVisibilitySortDependency.Init( LightsVisibilitySort::RunFromDependency, 0, 0 );
	#endif

}

void Lights::SetupForceSceneLightFlags(u32 sceneShadows)
{
#if __BANK
	if (PARAM_sceneShadows.Get())
	{
		PARAM_sceneShadows.Get(sceneShadows);
	}
#endif

	m_forceSceneLightFlags = 0;
	switch(sceneShadows)
	{
	case 1:
		m_forceSceneLightFlags = LIGHT_TYPE_POINT | LIGHT_TYPE_SPOT;
		break;
	case 2:
		m_forceSceneLightFlags = LIGHT_TYPE_SPOT;
		break;
	case 3:
		m_forceSceneLightFlags = LIGHT_TYPE_POINT;
		break;
	default:
		break;
	}

	m_forceSceneShadowTypes = 0;
	if(sceneShadows)
		m_forceSceneShadowTypes = LIGHTFLAG_CAST_STATIC_GEOM_SHADOWS | LIGHTFLAG_CAST_DYNAMIC_GEOM_SHADOWS | LIGHTFLAG_CAST_SHADOWS;

#if __BANK
	s_DefaultSceneShadowTypes = m_forceSceneShadowTypes;
	s_DefaultSceneLightFlags = m_forceSceneLightFlags;
#endif
}

void Lights::InitDLCCommands()
{
	DLC_REGISTER_EXTERNAL(dlCmdSetupLightsFrameInfo);
	DLC_REGISTER_EXTERNAL(dlCmdClearLightsFrameInfo);
	DLC_REGISTER_EXTERNAL(dlCmdLightOverride);
}

// ----------------------------------------------------------------------------------------------- //

void Lights::Update()
{
	if(s_pedLightReset > 0)
	{
		s_pedLightReset--;
	}
	else
	{
		// Adjust by inverse of average exposure
		s_exposureAvgArray[s_exposureAvgIndex] = PostFX::GetUpdateExposure();
		s_exposureAvgIndex++;
		s_exposureAvgSize = rage::Max(s_exposureAvgSize, s_exposureAvgIndex);
		s_exposureAvgIndex = s_exposureAvgIndex % 4;

		float average = 0.0f;
		for (u32 i = 0; i < s_exposureAvgSize; i++)
		{
			average += s_exposureAvgArray[i];
		}
		average /= (float)s_exposureAvgSize;
		s_pow2NegExposure = pow(2.0f, -average);
	}	

	Vector3 camPos = camInterface::GetPos();
	const float groundHeight =  CGameWorldHeightMap::GetMinInterpolatedHeightFromWorldHeightMap(camPos.x, camPos.y, true);
	const float distToGround = rage::Max<float>(0.0f, camPos.z - groundHeight);
	const float mult = CLODLights::CalculateSphereExpansion(distToGround);
	SetUpdateSphereExpansionMult(mult);	
}

// ----------------------------------------------------------------------------------------------- //

void Lights::PreSceneUpdate()
{

#if __BANK
	BUDGET_UPDATE_LIGHTS_INFO(m_sceneLights, m_numSceneLights, CLODLights::GetRenderedCount(), AmbientLights::GetRenderedCount() );
#endif
	Lights::ResetSceneLights();

#if (__D3D11 || RSG_ORBIS) && __BANK
	if (DebugLights::m_overrideSceneLightShadows)
	{
		m_forceSceneLightFlags = 0;
		m_forceSceneShadowTypes = 0;

		if (DebugLights::m_scenePointShadows) 
			m_forceSceneLightFlags |= LIGHT_TYPE_POINT;
		if (DebugLights::m_sceneSpotShadows)
			m_forceSceneLightFlags |= LIGHT_TYPE_SPOT;

		if (DebugLights::m_sceneStaticShadows)
			m_forceSceneShadowTypes |= LIGHTFLAG_CAST_STATIC_GEOM_SHADOWS;
		if (DebugLights::m_sceneDynamicShadows)
			m_forceSceneShadowTypes |= LIGHTFLAG_CAST_DYNAMIC_GEOM_SHADOWS;
		if (!m_forceSceneShadowTypes)
			m_forceSceneShadowTypes |= LIGHTFLAG_CAST_SHADOWS;
	}
	else
	{
		m_forceSceneLightFlags = s_DefaultSceneLightFlags;
		m_forceSceneShadowTypes = s_DefaultSceneShadowTypes;
	}
#endif // __BANK
}

// ----------------------------------------------------------------------------------------------- //

// remove (disable) cutscene light from the previous light list, so new lights for this camera cut can be added without dups
void Lights::ClearCutsceneLightsFromPrevious()
{
	for (int i=0;i<m_numPrevSceneShadowLights;i++)
	{
		if (m_prevSceneShadowLights[i].GetFlag(LIGHTFLAG_CUTSCENE))
		{
			m_prevSceneShadowLights[i].SetType(LIGHT_TYPE_NONE); 
			m_prevSceneShadowLights[i].SetShadowTrackingId(0);
		}
	}
}

// ----------------------------------------------------------------------------------------------- //

// Done after the time-cycle update
void Lights::UpdateBaseLights() 
{
	const tcKeyframeUncompressed& currKeyframe = g_timeCycle.GetCurrUpdateKeyframe();

	float naturalAmbientBaseMult = 
		currKeyframe.GetVar(TCVAR_LIGHT_NATURAL_AMB_BASE_INTENSITY) * 
		currKeyframe.GetVar(TCVAR_LIGHT_NATURAL_AMB_BASE_INTENSITY_MULT);

	float naturalAmbientDownMult = currKeyframe.GetVar(TCVAR_LIGHT_NATURAL_AMB_DOWN_INTENSITY);

	float artificialInteriorAmbientBaseMult = currKeyframe.GetVar(TCVAR_LIGHT_ARTIFICIAL_INT_AMB_BASE_INTENSITY);
	float artificialInteriorAmbientDownMult = currKeyframe.GetVar(TCVAR_LIGHT_ARTIFICIAL_INT_AMB_DOWN_INTENSITY);

	float artificialExteriorAmbientBaseMult = currKeyframe.GetVar(TCVAR_LIGHT_ARTIFICIAL_EXT_AMB_BASE_INTENSITY);
	float artificialExteriorAmbientDownMult = currKeyframe.GetVar(TCVAR_LIGHT_ARTIFICIAL_EXT_AMB_DOWN_INTENSITY);

	float directionalAmbientMult = 
		currKeyframe.GetVar(TCVAR_LIGHT_DIRECTIONAL_AMB_INTENSITY) * 
		currKeyframe.GetVar(TCVAR_LIGHT_DIRECTIONAL_AMB_INTENSITY_MULT);

	if( PostFX::GetUseNightVision())
	{
		naturalAmbientBaseMult *= currKeyframe.GetVar(TCVAR_NV_LIGHT_AMB_BASE_MULT);
		naturalAmbientDownMult *= currKeyframe.GetVar(TCVAR_NV_LIGHT_AMB_DOWN_MULT);
		
		artificialInteriorAmbientBaseMult *= currKeyframe.GetVar(TCVAR_NV_LIGHT_AMB_BASE_MULT);
		artificialInteriorAmbientDownMult *= currKeyframe.GetVar(TCVAR_NV_LIGHT_AMB_DOWN_MULT);

		artificialExteriorAmbientBaseMult *= currKeyframe.GetVar(TCVAR_NV_LIGHT_AMB_BASE_MULT);
		artificialExteriorAmbientDownMult *= currKeyframe.GetVar(TCVAR_NV_LIGHT_AMB_DOWN_MULT);
	}

	// The main directional light.
	const float dirLightIntensity = g_timeCycle.GetDirectionalLightIntensity();
	Vec3V vSunDirTemp = -g_timeCycle.GetDirectionalLightDirection();
	Vector3 dirLightDirection = RCC_VECTOR3(vSunDirTemp);

	// Directional ambient direction
	Vec3V bounceDir = Lerp(
		ScalarV(currKeyframe.GetVar(TCVAR_LIGHT_DIRECTIONAL_AMB_BOUNCE_ENABLED)),
		Negate(vSunDirTemp), 
		Negate(Reflect(vSunDirTemp, Vec3V(V_Z_AXIS_WONE))));

	// Natural Ambient 
	Vector4 naturalAmbientDown(
		currKeyframe.GetVar(TCVAR_LIGHT_NATURAL_AMB_DOWN_COL_R), 
		currKeyframe.GetVar(TCVAR_LIGHT_NATURAL_AMB_DOWN_COL_G), 
		currKeyframe.GetVar(TCVAR_LIGHT_NATURAL_AMB_DOWN_COL_B),
		0.0);
	naturalAmbientDown *= naturalAmbientDownMult;
	naturalAmbientDown.SetW(currKeyframe.GetVar(TCVAR_LIGHT_AMB_DOWN_WRAP));

	Vector4 naturalAmbientBase(
		currKeyframe.GetVar(TCVAR_LIGHT_NATURAL_AMB_BASE_COL_R), 
		currKeyframe.GetVar(TCVAR_LIGHT_NATURAL_AMB_BASE_COL_G), 
		currKeyframe.GetVar(TCVAR_LIGHT_NATURAL_AMB_BASE_COL_B),
		0.0);
	naturalAmbientBase *= naturalAmbientBaseMult;
	naturalAmbientBase.SetW(1.0f / (1.0f + currKeyframe.GetVar(TCVAR_LIGHT_AMB_DOWN_WRAP)));

	m_lightingGroup.SetNaturalAmbientDown(naturalAmbientDown);
	m_lightingGroup.SetNaturalAmbientBase(naturalAmbientBase);

	// Artificial Interior Ambient
	Vector4 artificialIntAmbientDown(
		currKeyframe.GetVar(TCVAR_LIGHT_ARTIFICIAL_INT_AMB_DOWN_COL_R), 
		currKeyframe.GetVar(TCVAR_LIGHT_ARTIFICIAL_INT_AMB_DOWN_COL_G), 
		currKeyframe.GetVar(TCVAR_LIGHT_ARTIFICIAL_INT_AMB_DOWN_COL_B),
		0.0);
	artificialIntAmbientDown *= artificialInteriorAmbientDownMult;
	artificialIntAmbientDown.SetW(currKeyframe.GetVar(TCVAR_LIGHT_NATURAL_PUSH));

	Vector4 artificialInteriorAmbientBase(
		currKeyframe.GetVar(TCVAR_LIGHT_ARTIFICIAL_INT_AMB_BASE_COL_R), 
		currKeyframe.GetVar(TCVAR_LIGHT_ARTIFICIAL_INT_AMB_BASE_COL_G), 
		currKeyframe.GetVar(TCVAR_LIGHT_ARTIFICIAL_INT_AMB_BASE_COL_B),
		0.0);
	artificialInteriorAmbientBase *= artificialInteriorAmbientBaseMult;
	artificialInteriorAmbientBase.SetW(bounceDir.GetXf());

	m_lightingGroup.SetArtificialIntAmbientDown(artificialIntAmbientDown);
	m_lightingGroup.SetArtificialIntAmbientBase(artificialInteriorAmbientBase);

	// Artificial Exterior Ambient
	Vector4 artificialExtAmbientDown(
		currKeyframe.GetVar(TCVAR_LIGHT_ARTIFICIAL_EXT_AMB_DOWN_COL_R), 
		currKeyframe.GetVar(TCVAR_LIGHT_ARTIFICIAL_EXT_AMB_DOWN_COL_G), 
		currKeyframe.GetVar(TCVAR_LIGHT_ARTIFICIAL_EXT_AMB_DOWN_COL_B),
		0.0);
	artificialExtAmbientDown *= artificialExteriorAmbientDownMult;
	artificialExtAmbientDown.SetW(bounceDir.GetYf());

	Vector4 artificialExteriorAmbientBase(
		currKeyframe.GetVar(TCVAR_LIGHT_ARTIFICIAL_EXT_AMB_BASE_COL_R), 
		currKeyframe.GetVar(TCVAR_LIGHT_ARTIFICIAL_EXT_AMB_BASE_COL_G), 
		currKeyframe.GetVar(TCVAR_LIGHT_ARTIFICIAL_EXT_AMB_BASE_COL_B),
		0.0);
	artificialExteriorAmbientBase *= artificialExteriorAmbientBaseMult;
	artificialExteriorAmbientBase.SetW(bounceDir.GetZf());

	m_lightingGroup.SetArtificialExtAmbientDown(artificialExtAmbientDown);
	m_lightingGroup.SetArtificialExtAmbientBase(artificialExteriorAmbientBase);
	
#if __BANK
	if (m_debugOverrideDirectionalLightDir == true && m_debugDirectionalLightDir.Mag2() > 0.001f)
	{
		m_debugDirectionalLightDir.Normalize();
		dirLightDirection = m_debugDirectionalLightDir;
	}
	else
	{
		m_debugDirectionalLightDir = dirLightDirection;
		m_debugOverrideDirectionalLightDir = false;
	}
#endif

	const Vector3 dirLightColour(currKeyframe.GetVar(TCVAR_LIGHT_DIR_COL_R), 
								 currKeyframe.GetVar(TCVAR_LIGHT_DIR_COL_G), 
								 currKeyframe.GetVar(TCVAR_LIGHT_DIR_COL_B));

	// Directional ambient
	Vector4 directionalAmbient(
		currKeyframe.GetVar(TCVAR_LIGHT_DIRECTIONAL_AMB_COL_R), 
		currKeyframe.GetVar(TCVAR_LIGHT_DIRECTIONAL_AMB_COL_G), 
		currKeyframe.GetVar(TCVAR_LIGHT_DIRECTIONAL_AMB_COL_B), 
		0.0f);

	// Final directional ambient intensity
	const float directionalAmbientIntensity = 
		directionalAmbientMult * 
		dirLightIntensity;

	m_lightingGroup.SetDirectionalAmbient(directionalAmbient * directionalAmbientIntensity);

	CutSceneManager* csMgr = CutSceneManager::GetInstance();
	// Setup the actual light
	CLightSource &dirLight = lightingData.m_directionalLight;
	int flags = LIGHTFLAG_CAST_SHADOWS;

	float dirLightMult = currKeyframe.GetVar(TCVAR_LIGHT_DIR_MULT) * 
		dirLightIntensity; 
	if(csMgr && csMgr->IsCutscenePlayingBackAndNotCutToGame())
	{
#if GTA_REPLAY
		if(CReplayMgr::IsEditModeActive())
		{
			const cutfCameraCutEventArgs* cutsceneFlags =  csMgr->GetReplayCutsceneCameraArgs();
			dirLightMult *= (cutsceneFlags && cutsceneFlags->IsDirectionalLightDisabled()) ? 0.0f : 1.0f;
		}
		else
#endif
		{
			const CCutSceneCameraEntity* cutsceneCamera = csMgr->GetCamEntity();

			dirLightMult *= (cutsceneCamera && cutsceneCamera->IsDirectionalLightDisabled()) ? 0.0f : 1.0f;
		}
	}
	dirLightMult *=  (PostFX::GetUseNightVision()) ? currKeyframe.GetVar(TCVAR_NV_LIGHT_DIR_MULT) : 1.0f;
	if (dirLightMult < 1e-6f || (dirLightColour.IsLessThanAll(Vector3(1e-6f, 1e-6f, 1e-6f)))) dirLightMult = 0.0f;

	dirLight.SetCommon(
		LIGHT_TYPE_DIRECTIONAL, 
		flags, 
		Vector3(0, 0, 0), 
		dirLightColour, 
		dirLightMult, 
		LIGHT_ALWAYS_ON);
	dirLight.SetDirection(dirLightDirection);

	// Setup ped light
	LightingData &lightData = Lights::GetUpdateLightingData();

	lightData.m_pedLightColour = Vector3(
		currKeyframe.GetVar(TCVAR_PED_LIGHT_COL_R), 
		currKeyframe.GetVar(TCVAR_PED_LIGHT_COL_G), 
		currKeyframe.GetVar(TCVAR_PED_LIGHT_COL_B));
	lightData.m_pedLightIntensity = currKeyframe.GetVar(TCVAR_PED_LIGHT_MULT);
	lightData.m_pedLightDirection = Vector3(
		currKeyframe.GetVar(TCVAR_PED_LIGHT_DIRECTION_X), 
		currKeyframe.GetVar(TCVAR_PED_LIGHT_DIRECTION_Y), 
		currKeyframe.GetVar(TCVAR_PED_LIGHT_DIRECTION_Z));

	BANK_ONLY(DebugDeferred::OverridePedLight(lightData.m_pedLightColour, lightData.m_pedLightIntensity, lightData.m_pedLightDirection));

	// Cut-scene specific overrides
	bool adjustByExposure = true;
	if(csMgr && csMgr->IsCutscenePlayingBackAndNotCutToGame())
	{
		const CCutSceneCameraEntity* cutsceneCamera = csMgr->GetCamEntity();

		if(cutsceneCamera REPLAY_ONLY(|| CReplayMgr::IsEditModeActive()))
		{
			const cutfCameraCutCharacterLightParams* params = NULL;
			
#if GTA_REPLAY
			if( CReplayMgr::IsEditModeActive() )
			{
				params = csMgr->GetReplayCutsceneCharacterLightParams();
			}
			else
#endif
			{
				params = cutsceneCamera->GetCharacterLightParams();
			}
			
			if (params && !params->m_bUseTimeCycleValues)
			{
				lightData.m_pedLightDirection= params->m_vDirection;
				float intensity = params->m_fIntensity; 

				if (params->m_fNightIntensity > 0.0f)
				{
					const float fadeVal = CalculateTimeFade(
						m_pedLightDayFadeStart, m_pedLightDayFadeEnd,
						m_pedLightNightFadeStart, m_pedLightNightFadeEnd);

					intensity = Lerp(fadeVal, params->m_fNightIntensity, params->m_fIntensity);
				}
				
				if (params->m_bUseAsIntensityModifier)
				{
					lightData.m_pedLightIntensity *= intensity;
				}
				else
				{
					lightData.m_pedLightColour = params->m_vColour;
					lightData.m_pedLightIntensity = intensity;
				}

				const cutfCameraCutEventArgs* cameraCutEventArgs;
#if GTA_REPLAY
				if( CReplayMgr::IsEditModeActive() )
				{
					cameraCutEventArgs = csMgr->GetReplayCutsceneCameraArgs();
				}
				else
#endif
				{
					cameraCutEventArgs = cutsceneCamera->GetCameraCutEventArgs();
				}
			
				if (cameraCutEventArgs)
				{
					adjustByExposure = !cameraCutEventArgs->IsAbsoluteIntensityEnabled();
				}
				
			}
		}

		if (m_fadeUpPedLight)
		{
			m_fadeUpPedLight = false;
			m_fadeUpStartTime = 0;
			m_fadeUpDuration = 0.0f;
		}
	}
	else
	{
		if (g_PlayerSwitch.GetLongRangeMgr().PreppingDescent() || g_PlayerSwitch.GetLongRangeMgr().PerformingDescent())
		{
			lightData.m_pedLightIntensity = 0.0f;
		}

		// Ped light fade up (doesn't apply in cutscenes)
		if (m_fadeUpPedLight)
		{
			const u32 currentTime = fwTimer::GetSystemTimeInMilliseconds();
			const float diff = (float)(currentTime - m_fadeUpStartTime);
			lightData.m_pedLightIntensity *= Saturate(diff / m_fadeUpDuration);

			if (currentTime > m_fadeUpStartTime + (u32)m_fadeUpDuration)
			{
				m_fadeUpPedLight = false;
				m_fadeUpStartTime = 0;
				m_fadeUpDuration = 0.0f;
			}
		}
	}

	lightData.m_pedLightDirection.Normalize();

	// Ped light is relative to the camera direction
	const Matrix34& camMtx = camInterface::GetMat();
	camMtx.Transform3x3(lightData.m_pedLightDirection);

	lightData.m_pedLightIntensity *= (adjustByExposure) ? s_pow2NegExposure : 1.0f;  

	// Setup light fade values
	lightData.m_lightFadeMultipliers = Vec4V(
		1.0f,
		g_timeCycle.GetCurrUpdateKeyframe().GetVar(TCVAR_SHADOW_DISTANCE_MULT),
		1.0f,
		1.0f);

	if(csMgr && csMgr->IsCutscenePlayingBackAndNotCutToGame())
	{
		const CCutSceneCameraEntity* cam = csMgr->GetCamEntity();

		if (cam)
		{
			const cutfCameraCutEventArgs* cameraCutEventArgs = cam->GetCameraCutEventArgs();
			if(cameraCutEventArgs)
			{
				lightData.m_lightFadeMultipliers = Vec4V(
					cameraCutEventArgs->GetLightFadeDistanceMult(),
					cameraCutEventArgs->GetLightShadowFadeDistanceMult(),
					cameraCutEventArgs->GetLightSpecularFadeDistMult(),
					cameraCutEventArgs->GetLightVolumetricFadeDistanceMult());
			}
		}
	}
}

// ----------------------------------------------------------------------------------------------- //

void Lights::ProcessVisibilitySortAsync()
{
	int bufferId = gRenderThreadInterface.GetUpdateBuffer();
	LightsVisibilityOutput* pVisibilityArray = &(g_LightsVisibilitySortOutput[bufferId][0]);
	atFixedBitSet<MAX_STORED_LIGHTS>* pVisibilityCamInsideOutput = &(g_LightsVisibilityCamInsideOutput[bufferId]);
	atFixedBitSet<MAX_STORED_LIGHTS>* pVisibleLastFrameOutput = &(g_LightsVisibilityVisibleLastFrameOutput[bufferId]);

	u32* pVisibilityCount = &(g_LightsVisibilitySortCount[bufferId]);
	//Reset Count to 0
	*pVisibilityCount = 0;
	
	const grcViewport* grcVP = gVpMan.GetCurrentGameGrcViewport();

	spdTransposedPlaneSet8 cullPlanes;
#if NV_SUPPORT
	cullPlanes.SetStereo(*grcVP, true, true);
#else
	cullPlanes.Set(*grcVP, true, true);
#endif
	Vector4 farclip = VEC4V_TO_VECTOR4(grcVP->GetFrustumClipPlane(grcViewport::CLIP_PLANE_FAR));

	// This is the distance from the camera position to the near frustum corner. It defines a radius that fully encloses the camera and it's near plane.
	// Note that it is bigger that the "near clip" distance since it has to enclose the corners of the near part of the camera frustum.
	float nearPlaneBoundIncrease = grcVP->GetNearClip() * sqrtf(1.0f + grcVP->GetTanHFOV() * grcVP->GetTanHFOV() + grcVP->GetTanVFOV() * grcVP->GetTanVFOV());

	Vector3 vCamPos = VEC3V_TO_VECTOR3(gVpMan.GetCurrentGameGrcViewport()->GetCameraPosition());
	

	// Go through ALL of the scene lights and work out which ones are on screen.
	u32 t_startLightIndex = 0;
	u32 t_endLightIndex = m_numSceneLights;

#if __BANK
	DebugLights::GetLightIndexRangeUpdateThread(t_startLightIndex, t_endLightIndex);
#endif

	g_LightsVisibilitySortData.m_pVisibilitySortedArray = pVisibilityArray;
	g_LightsVisibilitySortData.m_pVisibilitySortedCount = pVisibilityCount;
	g_LightsVisibilitySortData.m_pCamInsideOutput = pVisibilityCamInsideOutput;
	g_LightsVisibilitySortData.m_pVisibleLastFrameOutput = pVisibleLastFrameOutput;	
	g_LightsVisibilitySortData.m_prevLightCount = Lights::m_numPrevSceneShadowLights;
	g_LightsVisibilitySortData.m_cullPlanes = cullPlanes;
	g_LightsVisibilitySortData.m_CameraPos = vCamPos;
	g_LightsVisibilitySortData.m_farClip = farclip;
	g_LightsVisibilitySortData.m_nearPlaneBoundsIncrease = nearPlaneBoundIncrease;
	g_LightsVisibilitySortData.m_startLightIndex = t_startLightIndex;
	g_LightsVisibilitySortData.m_endLightIndex = t_endLightIndex;

	g_LightsVisibilitySortDependency.m_Priority = sysDependency::kPriorityMed;
	g_LightsVisibilitySortDependency.m_Flags = 
		sysDepFlag::ALLOC0 |
		sysDepFlag::INPUT1 |
		sysDepFlag::INPUT2 | 
		sysDepFlag::INPUT3;

	g_LightsVisibilitySortDependency.m_Params[1].m_AsPtr = (void*)(Lights::m_sceneLights);
	g_LightsVisibilitySortDependency.m_Params[2].m_AsPtr = (void*)(Lights::m_prevSceneShadowLights);
	g_LightsVisibilitySortDependency.m_Params[3].m_AsPtr = &g_LightsVisibilitySortData;
	g_LightsVisibilitySortDependency.m_Params[4].m_AsPtr = &g_LightsVisibilitySortDependencyRunning;


	g_LightsVisibilitySortDependency.m_DataSizes[0] = LIGHTSVISIBILITYSORT_SCRATCH_SIZE;
	g_LightsVisibilitySortDependency.m_DataSizes[1] = MAX_STORED_LIGHTS * sizeof(CLightSource);
	g_LightsVisibilitySortDependency.m_DataSizes[2] = MAX_STORED_LIGHTS * sizeof(CLightSource);
	g_LightsVisibilitySortDependency.m_DataSizes[3] = sizeof(LightsVisibilitySortData);


	g_LightsVisibilitySortDependencyRunning = 1;

	sysDependencyScheduler::Insert( &g_LightsVisibilitySortDependency );


}

// ----------------------------------------------------------------------------------------------- //

void Lights::WaitForProcessVisibilityDependency()
{
	PF_PUSH_TIMEBAR("Lights:: Wait for Visibility Sort Dependency");
	while(g_LightsVisibilitySortDependencyRunning)
	{
		sysIpcYield(PRIO_NORMAL);
	}
	PF_POP_TIMEBAR();
}
// ----------------------------------------------------------------------------------------------- //

void Lights::AddToDrawList()
{
	SetupRenderThreadLights();
}

// ----------------------------------------------------------------------------------------------- //

void Lights::BeginLightRender()
{
	CVolumeVertexBuffer::BeginVolumeRender();
}

// ----------------------------------------------------------------------------------------------- //

void Lights::EndLightRender()
{
	CVolumeVertexBuffer::EndVolumeRender();
}

// ----------------------------------------------------------------------------------------------- //

void Lights::ResetLightMeshCache()
{
	CVolumeVertexBuffer::ResetLightMeshCache();
}

// ----------------------------------------------------------------------------------------------- //
void Lights::SetupRenderThreadVisibleLightLists(bool cameraCut)
{
	FastAssert(sysThreadType::IsRenderThread());
	g_SortedVisibleLights.Update(cameraCut);
}

// ----------------------------------------------------------------------------------------------- //

void Lights::RenderVolumeLight(int type, bool useHighResMesh, bool isVolumeFXLight, bool isShadowCaster )
{
	CVolumeVertexBuffer* vb = NULL;
	
	int index;

	const int detailIndex = ((useHighResMesh || isShadowCaster)? 2 : isVolumeFXLight? 1: 0);
	if(type == LIGHT_TYPE_SPOT)
		index = SPOT_VB_LOWDETAIL;
	else
		index = POINT_VB_LOWDETAIL;

	index = index + detailIndex;

	vb = &(g_VolumeVertexBuffers[index]);
		
	// TODO -- try indexed verts; call GRCDEVICE.SetIndices(const grcIndexBuffer& pBuffer), and use DrawIndexedPrimitive

	vb->Draw();

	//commenting this out as we are using a caching mechanism for the light mesh
	//At the end of rendering lights we call Lights::EndLightRender() which
	//clears the stream surce
	//GRCDEVICE.ClearStreamSource(0);
}

// ----------------------------------------------------------------------------------------------- //

void Lights::RenderVolumeLightShaft(bool /*useHighResMesh*/)
{
	CVolumeVertexBuffer* vb = NULL;

	/*if (useHighResMesh)
	{
		vb = &g_shaftVolumeVertexBufferHighRes;
	}
	else*/
	{
		vb = &g_shaftVolumeVertexBuffer;
	}

	CVolumeVertexBuffer::ResetLightMeshCache();
	vb->Draw();
	GRCDEVICE.ClearStreamSource(0);
}

// ----------------------------------------------------------------------------------------------- //

grcVertexBuffer* Lights::GetLightMesh(u32 index)
{
	return g_VolumeVertexBuffers[index].GetVtxBuffer();
}

// ----------------------------------------------------------------------------------------------- //

// Assumes directions are already scaled!
static void RenderCuboidVolume(int type, Vector3& pos, Vector3& dirz, Vector3& diry, Vector3& dirx)
{
	Color32 white(255,255,255,255);
	Vector3 vert[8];

	for(int x=0;x<=1;x++)
	{
		for(int y=0;y<=1;y++)
		{
			for(int z=0;z<=1;z++)
			{
				int i=x*4+y*2+z;

				vert[i]=pos;
				
				vert[i]+=dirx*(x?1.0f:-1.0f);
				vert[i]+=diry*(y?1.0f:-1.0f);
				
				if (type == LIGHT_TYPE_AO_VOLUME)
					vert[i]+=dirz*(z?1.0f:0.0f);
				else
					vert[i]+=dirz*(z?1.0f:-1.0f);
			}
		}
	}
	
	grcBegin(drawTriStrip, 18);
		grcVertex(vert[1].x, vert[1].y, vert[1].z, 0.0f, 0.0f, 0.0f, white, 0.0f, 0.0f);
		grcVertex(vert[0].x, vert[0].y, vert[0].z, 0.0f, 0.0f, 0.0f, white, 0.0f, 0.0f);
		grcVertex(vert[3].x, vert[3].y, vert[3].z, 0.0f, 0.0f, 0.0f, white, 0.0f, 0.0f);
		grcVertex(vert[2].x, vert[2].y, vert[2].z, 0.0f, 0.0f, 0.0f, white, 0.0f, 0.0f);
		grcVertex(vert[7].x, vert[7].y, vert[7].z, 0.0f, 0.0f, 0.0f, white, 0.0f, 0.0f);
		grcVertex(vert[6].x, vert[6].y, vert[6].z, 0.0f, 0.0f, 0.0f, white, 0.0f, 0.0f);
		grcVertex(vert[5].x, vert[5].y, vert[5].z, 0.0f, 0.0f, 0.0f, white, 0.0f, 0.0f);
		grcVertex(vert[4].x, vert[4].y, vert[4].z, 0.0f, 0.0f, 0.0f, white, 0.0f, 0.0f);

		grcVertex(vert[4].x, vert[4].y, vert[4].z, 0.0f, 0.0f, 0.0f, white, 0.0f, 0.0f);
		grcVertex(vert[3].x, vert[3].y, vert[3].z, 0.0f, 0.0f, 0.0f, white, 0.0f, 0.0f);

		grcVertex(vert[3].x, vert[3].y, vert[3].z, 0.0f, 0.0f, 0.0f, white, 0.0f, 0.0f);
		grcVertex(vert[7].x, vert[7].y, vert[7].z, 0.0f, 0.0f, 0.0f, white, 0.0f, 0.0f);
		grcVertex(vert[1].x, vert[1].y, vert[1].z, 0.0f, 0.0f, 0.0f, white, 0.0f, 0.0f);
		grcVertex(vert[5].x, vert[5].y, vert[5].z, 0.0f, 0.0f, 0.0f, white, 0.0f, 0.0f);
		grcVertex(vert[0].x, vert[0].y, vert[0].z, 0.0f, 0.0f, 0.0f, white, 0.0f, 0.0f);
		grcVertex(vert[4].x, vert[4].y, vert[4].z, 0.0f, 0.0f, 0.0f, white, 0.0f, 0.0f);
		grcVertex(vert[2].x, vert[2].y, vert[2].z, 0.0f, 0.0f, 0.0f, white, 0.0f, 0.0f);
		grcVertex(vert[6].x, vert[6].y, vert[6].z, 0.0f, 0.0f, 0.0f, white, 0.0f, 0.0f);
	grcEnd();
}

// ----------------------------------------------------------------------------------------------- //

Vector4 Lights::CalculateLightConsts(eLightType type, float radius, const bool useHighResMesh)
{
	return Vector4 ( type * 1.0f,
					radius,
					(radius == 0.0f) ? 0.0f : 1.0f / (radius * radius),
					radius * (useHighResMesh ? 0.02f : EXTEND_LIGHT(type)) );
}

// ----------------------------------------------------------------------------------------------- //
void Lights::CalculateLightParams(const CLightSource *light, Vector4* lightParams)
{
	const grcViewport *const vp = grcViewport::GetCurrent();
	Vec4V fadeValues;
	Lights::CalculateFadeValues(light, vp->GetCameraPosition(), fadeValues);

	switch (light->GetType())
	{
		case LIGHT_TYPE_POINT:
		case LIGHT_TYPE_DIRECTIONAL:
		{
			lightParams[0].Set(0.0f, 0.0f, 1.0f, 0.0f);
			break;
		}

		case LIGHT_TYPE_SPOT:
		{
			const float cosOuterAngle = light->GetConeCosOuterAngle();
			const float sinOuterAngle = sqrtf(1.0f - cosOuterAngle*cosOuterAngle);
			const float scale         = 1.0f / rage::Max((light->GetConeCosInnerAngle() - cosOuterAngle), 0.000001f);
			const float offset        = -cosOuterAngle * scale;

			lightParams[0].Set(cosOuterAngle, sinOuterAngle, offset, scale);
			break;
		}

		case LIGHT_TYPE_CAPSULE:
		{
			lightParams[0].Set(light->GetCapsuleExtent(), 0.0f, 1.0f, 0.0f);
			break;
		}

		case LIGHT_TYPE_AO_VOLUME:
		{
			const float zVal = ((light->HasValidTextureKey() == false) BANK_ONLY(|| DebugLighting::m_testSphereAmbVol == true)) ? 
								light->GetVolumeSizeZ() / 0.66f : 
								light->GetVolumeSizeZ();			
			float xVal = light->GetVolumeSizeX();
			float yVal = light->GetVolumeSizeY();
			// If AO Blob is a cuboid, then pre multiply them with adjusted constants (which are taken out from the shader)
			if(!((light->HasValidTextureKey() == false) BANK_ONLY(|| DebugLighting::m_testSphereAmbVol == true)))
			{
				//Moving values from shader to code to make shader cheaper for AO Blobs
				xVal *= 2.2f;
				yVal *= 1.8f;
			}
			lightParams[0].Set(xVal, yVal, zVal, light->GetVolumeBaseIntensity());
			break;
		}

		default:
		{
			lightParams[0].Zero();
			break;
		}
	}

	light->GetPlane(lightParams[1]);
			
	static dev_float timeScale = 0.025f;
	lightParams[2].Set(light->GetFalloffExponent(), 0.0, Water::GetWaterLevel(), TIME.GetElapsedTime() * timeScale);

	lightParams[3] = RCC_VECTOR4(fadeValues);

	// Setup headlight positions for vehicle lights .. don't use w components as SSS decided to hijack them
	if(light->GetFlags() & LIGHTFLAG_USE_VEHICLE_TWIN)
	{
		Vector3 lightPos1 = light->GetPosition() - light->GetVehicleHeadlightOffset() * light->GetTangent();
		Vector3 lightPos2 = light->GetPosition() + light->GetVehicleHeadlightOffset() * light->GetTangent();

		lightParams[4].SetVector3(lightPos1);
		lightParams[5].SetVector3(lightPos2); 

		Vector3 lightDir1 = light->GetDirection() - light->GetVehicleHeadlightSplitOffset() * light->GetTangent();
		Vector3 lightDir2 = light->GetDirection() + light->GetVehicleHeadlightSplitOffset() * light->GetTangent();

		lightDir1.Normalize();
		lightDir2.Normalize();

		lightParams[7].SetVector3(lightDir1); 
		lightParams[8].SetVector3(lightDir2);
	}
	else
	{
		lightParams[4] = Lights::m_SSSParams;
		lightParams[5] = Lights::m_SSSParams2;
		lightParams[7].Zero();
		lightParams[8].Zero();
	}

	lightParams[6].Set(light->GetFlipTexture() ? 1.0f : 0.0f, light->GetParentAlpha(), 0.0f, 0.0f);
}

// ----------------------------------------------------------------------------------------------- //

void Lights::DrawDefLight(
	const CLightSource *light, 
	const eDeferredShaders deferredShader,
	const grcEffectTechnique technique, 
	const s32 pass, 
	bool useHighResMesh, 
	bool isShadowCaster,
	MultisampleMode aaMode)
{
	const grmShader* shader = DeferredLighting::GetShader(deferredShader, aaMode);

	if (ShaderUtil::StartShader("light", shader, technique, pass, false))
	{
		Lights::RenderVolumeLight(light->GetType(), useHighResMesh, false, isShadowCaster);
		ShaderUtil::EndShader(shader, false);
	}
}
// ----------------------------------------------------------------------------------------------- //

void Lights::RenderAmbientScaleVolumes()
{
#if RSG_PC
	CShaderLib::SetStereoTexture();
#endif

	// Get the viewport for camera info.
	grcViewport* grcVP = grcViewport::GetCurrent();
	grcVP->SetWorldIdentity();
	Vector4 farclip = VEC4V_TO_VECTOR4(grcVP->GetFrustumClipPlane(grcViewport::CLIP_PLANE_FAR));

	DeferredLighting::SetProjectionShaderParams(DEFERRED_SHADER_LIGHTING, MM_DEFAULT);
	DeferredLighting::SetShaderGBufferTargets();
	grcTexture* pTexture = NULL;

	const tcKeyframeUncompressed& currKeyframe = g_timeCycle.GetCurrRenderKeyframe();
	const float applyVolumeToDiffuse = currKeyframe.GetVar(TCVAR_LIGHT_AMB_VOLUMES_IN_DIFFUSE);

	Vec4V vAmbOccEffectScale;	
	vAmbOccEffectScale.SetXf(currKeyframe.GetVar(TCVAR_LIGHT_AMB_OCC_MULT));
	vAmbOccEffectScale.SetYf(currKeyframe.GetVar(TCVAR_LIGHT_AMB_OCC_MULT) * currKeyframe.GetVar(TCVAR_LIGHT_AMB_OCC_MULT_PED));
	vAmbOccEffectScale.SetZf(currKeyframe.GetVar(TCVAR_LIGHT_AMB_OCC_MULT) * currKeyframe.GetVar(TCVAR_LIGHT_AMB_OCC_MULT_VEH));
	vAmbOccEffectScale.SetWf(currKeyframe.GetVar(TCVAR_LIGHT_AMB_OCC_MULT_PROP));
	CShaderLib::SetGlobalAmbientOcclusionEffect(RCC_VECTOR4(vAmbOccEffectScale));
	
	grcStateBlock::SetBlendState(m_AmbientScaleVolumes_B);
	grcStateBlock::SetDepthStencilState(m_AmbientScaleVolumes_DS, DEFERRED_MATERIAL_PED);
	grcStateBlock::SetRasterizerState(m_AmbientScaleVolumes_R);

#if __D3D11
	const grcRenderTarget* rendertargets[grcmrtColorCount] = {
		GBuffer::GetTarget(GBUFFER_RT_0),
		NULL,
		NULL,
		GBuffer::GetTarget(GBUFFER_RT_3)
	};

	grcRenderTarget* depth = GRCDEVICE.IsReadOnlyDepthAllowed() ? CRenderTargets::GetDepthBuffer_ReadOnly() : CRenderTargets::GetDepthBufferCopy();
	grcTextureFactory::GetInstance().LockMRT(rendertargets, depth);
#endif

#if NV_SUPPORT
	const bool useStereoFrustum = GRCDEVICE.CanUseStereo() && GRCDEVICE.IsStereoEnabled();
#endif

	Lights::BeginLightRender();
	for (s32 i = 0; i < m_numRenderLights; i++)
	{
		CLightSource* light = &m_renderLights[i];

		if (light->GetType() != LIGHT_TYPE_AO_VOLUME)
			continue;

		light->SetColor(Vector3(applyVolumeToDiffuse, applyVolumeToDiffuse, applyVolumeToDiffuse));

		pTexture = NULL;
		if (light->HasValidTextureKey())
			pTexture = CShaderLib::LookupTexture(light->GetTextureKey());

		// Get the lights params.
		Vector3	pos	= light->GetPosition();
		Vector3	dir	= light->GetDirection();
		Vector3	tangent	= light->GetTangent();

		float scalex = light->GetVolumeSizeX(); 
		float scaley = light->GetVolumeSizeY(); 
		float scalez = light->GetVolumeSizeZ();

		float radius = light->GetRadius();
		float dist = (-farclip.Dot3(pos)) - farclip.w;

		//visibility test
		if ( (dist<(-radius)) && (grcVP->IsSphereVisible(pos.x, pos.y, pos.z, radius NV_SUPPORT_ONLY(,NULL, useStereoFrustum)) != cullOutside) )
		{
			DeferredLighting::SetLightShaderParams(DEFERRED_SHADER_LIGHTING, light, pTexture, 1.0f, false, false, MM_DEFAULT);

			// Set the light params into the shader.
			if ((light->HasValidTextureKey() == false) BANK_ONLY(|| DebugLighting::m_testSphereAmbVol == true))
			{
				if (ShaderUtil::StartShader(
					"Ambient Volume", 
					DeferredLighting::m_deferredShaders[MM_DEFAULT][DEFERRED_SHADER_LIGHTING], 
					DeferredLighting::m_techniques[MM_DEFAULT][DEFERRED_TECHNIQUE_AMBIENT_VOLUME], 
					1))
				{
					RenderVolumeLight(light->GetType(), false, false, light->IsCastShadows());
					ShaderUtil::EndShader(DeferredLighting::m_deferredShaders[MM_DEFAULT][DEFERRED_SHADER_LIGHTING]);
				}
			}
			else
			{
				Vector3 dirz = dir * (scalez * 3.0f);
				Vector3 diry = tangent * (scaley * 1.5f + 0.5f);
				Vector3 dirx;
				dirx.Cross(dir, tangent);
				dirx *= (scalex * 1.0f + 0.5f);

				if (ShaderUtil::StartShader(
					"Ambient Volume", 
					DeferredLighting::m_deferredShaders[MM_DEFAULT][DEFERRED_SHADER_LIGHTING], 
					DeferredLighting::m_techniques[MM_DEFAULT][DEFERRED_TECHNIQUE_AMBIENT_VOLUME],
					0))
				{
					//Reset light mesh cache as RenderCuboidVolume uses dynamic vertex buffer
					//which clears stream source. So we need to invalidate the cache for the light
					//mesh
					ResetLightMeshCache();
					RenderCuboidVolume(LIGHT_TYPE_AO_VOLUME, pos, dirz, diry, dirx);
					ShaderUtil::EndShader(DeferredLighting::m_deferredShaders[MM_DEFAULT][DEFERRED_SHADER_LIGHTING]);
				}
			}
		}
	}

	Lights::EndLightRender();
	D3D11_ONLY( grcTextureFactory::GetInstance().UnlockMRT() );

}

// ----------------------------------------------------------------------------------------------- //
#if DEPTH_BOUNDS_SUPPORT
// ----------------------------------------------------------------------------------------------- //

inline bool SetDepthBoundsFromLightVolume (const CLightSource* pLightSource )
{

	// Get light's position in world space and view space
	const Matrix44 mViewMatrix = MAT44V_TO_MATRIX44( grcViewport::GetCurrent()->GetViewMtx() );
	const Vector4 vLightPositionWS = Vector4( pLightSource->GetPosition().GetX(), pLightSource->GetPosition().GetY(), pLightSource->GetPosition().GetZ(), 1.f );
	Vector4 vLightPositionVS;
	mViewMatrix.Transform( vLightPositionWS, vLightPositionVS );

	const float fCameraNearClip = grcViewport::GetCurrent()->GetNearClip();
	const float fCameraFarClip  = grcViewport::GetCurrent()->GetFarClip();

	const float fRadius = pLightSource->GetBoundingSphereRadius();

	// Min/Max distances in view space
	float fLightBoundsMin, fLightBoundsMax;

	if (pLightSource->GetType() == LIGHT_TYPE_SPOT && pLightSource->GetConeCosOuterAngle() >= 0.0001f)
	{
		// Compute the cone's AABB in view space:

		const Vector3 vPositionWS = pLightSource->GetPosition();
		const Vector3 vDirectionWS = pLightSource->GetDirection();
		
		Vector4 vPositionVS;
		mViewMatrix.Transform( Vector4(vPositionWS.x,vPositionWS.y,vPositionWS.z,1.f), vPositionVS );

		Vector3 vDirectionVS;
		mViewMatrix.Transform3x3( vDirectionWS, vDirectionVS );

		const spdAABB bound = fwBoxFromCone(vPositionVS.GetVector3(), fRadius, vDirectionVS, pLightSource->GetConeOuterAngle() * DtoR);
		fLightBoundsMin = Clamp( -bound.GetMax().GetZf(), fCameraNearClip, fCameraFarClip );
		fLightBoundsMax = Clamp( -bound.GetMin().GetZf(), fCameraNearClip, fCameraFarClip );
	}
	else
	{	
		// Just use a bounding sphere in view space: 

		const float fLightCenter = -vLightPositionVS.GetZ();
		fLightBoundsMin = Clamp( fLightCenter - fRadius, fCameraNearClip, fCameraFarClip );
		fLightBoundsMax = Clamp( fLightCenter + fRadius, fCameraNearClip, fCameraFarClip );
	}

	// Convert to z buffer value
	const float fZScale = fCameraFarClip / ( fCameraFarClip - fCameraNearClip );
	const float fZBoundsMin = Clamp( fZScale*( fLightBoundsMin - fCameraNearClip )/fLightBoundsMin, 0.0f, 1.0f );
	const float fZBoundsMax = Clamp( fZScale*( fLightBoundsMax - fCameraNearClip )/fLightBoundsMax, 0.0f, 1.0f );

	// Very rarely we get a light that is entirely behind the near plane (B*1850520). This is probably a sign that 
	// our light culling isn't quite as aggressive as it could be. This only happens in 2 known parts of the game 
	// though so I don't want to refactor the light culling code at this point just to fix 2 benign asserts.
	if (fZBoundsMin == fZBoundsMax)
	{
		return false;
	}

	// Set the depth bounds
#if USE_INVERTED_PROJECTION_ONLY
	grcDevice::SetDepthBounds( 1.0f - fZBoundsMax, 1.0f - fZBoundsMin );
#else
	grcDevice::SetDepthBounds( fZBoundsMin, fZBoundsMax );
#endif
	return true;
}

// ----------------------------------------------------------------------------------------------- //
#endif // DEPTH_BOUNDS_SUPPORT
// ----------------------------------------------------------------------------------------------- //

// ----------------------------------------------------------------------------------------------- //



// ----------------------------------------------------------------------------------------------- //
bool Lights::HasSceneLights()
{
	// Results will only be valid if Lights::SetupRenderThreadVisibleLightLists() was called on the render thread beforehand
	FastAssert(sysThreadType::IsRenderThread());
	return g_SortedVisibleLights.GetOutsideSortedLights().GetCount() || g_SortedVisibleLights.GetInsideSortedLights().GetCount();
}

// ----------------------------------------------------------------------------------------------- //
bool Lights::AnyVolumeLightsPresent()
{
	// Results will only be valid if Lights::SetupRenderThreadVisibleLightLists() was called on the render thread beforehand
	FastAssert(sysThreadType::IsRenderThread());
	return g_SortedVisibleLights.AnyVolumeLightsPresent();
}

// ----------------------------------------------------------------------------------------------- //

void Lights::RenderSceneLights(bool bShadows, const bool BANK_ONLY(drawDebugCost))
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	grcViewport& grcVP = *grcViewport::GetCurrent();
	// Vector4 farclip = VEC4V_TO_VECTOR4(grcVP.GetFrustumClipPlane(grcViewport::CLIP_PLANE_FAR));
	CLightSource* lights = m_renderLights;
	float tanHFov = grcVP.GetTanHFOV();

	grcTexture* pTexture = NULL;

	grcViewport::SetCurrentWorldIdentity();

	if ( !Lights::HasSceneLights() ) // nothing made it through the visibility tests...
	{
		return;
	}
	
	// Get sorted visible lights list
	CSortedVisibleLights::SortedLightArray& outsideSortedLights = g_SortedVisibleLights.GetOutsideSortedLights();
	CSortedVisibleLights::SortedLightArray& insideSortsLights = g_SortedVisibleLights.GetInsideSortedLights();
	const bool isCamInsideAnyLights = g_SortedVisibleLights.IsCamInsideAnyLights();
	bool useStencil = (DeferredLighting__m_useLightsHiStencil);
#if MSAA_EDGE_PASS
	CompileTimeAssert(LIGHT_ONE_PASS_STENCIL_BIT == DEFERRED_MATERIAL_SPAREOR1);
	const bool bUseEdgePass = DeferredLighting::IsEdgePassActiveForSceneLights()
		BANK_ONLY(&& (DebugDeferred::m_OptimizeSceneLights & (1<<EM_IGNORE)));
	Assert(!useStencil || !bUseEdgePass);
#endif //MSAA_EDGE_PASS

	// if we're using it, do some basic hi stencil setup outside the loop
	if ((useStencil && !CLODLights::Enabled()) BANK_ONLY(|| drawDebugCost)) 
	{
#if __XENON
		CHECK_HRESULT(GRCDEVICE.GetCurrent()->SetRenderState( D3DRS_HIZENABLE, D3DHIZ_AUTOMATIC));
		CHECK_HRESULT(GRCDEVICE.GetCurrent()->SetRenderState( D3DRS_HISTENCILFUNC, D3DHSCMP_NOTEQUAL ));
#elif __PS3	// we want to set the scull function and restore the depth compare direction a t the same time to avoid invalidation
		grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_Default,DEFERRED_MATERIAL_ALL);
		GCM_DEBUG(GCM::cellGcmSetScullControl(GCM_CONTEXT, CELL_GCM_SCULL_SFUNC_EQUAL, LIGHT_FINAL_PASS_STENCIL_BIT, LIGHT_STENCIL_WRITE_MASK_BITS));
		GCM_DEBUG(GCM::cellGcmSetClearZcullSurface(GCM_CONTEXT, false, true)); 
#endif
	}

	int prevScissorX      = -1;
	int prevScissorY	  = -1;
	int prevScissorWidth  = -1;
	int prevScissorHeight = -1;

	DeferredLighting::SetShaderGBufferTargets();
	Lights::BeginLightRender();
	atFixedArray<SortedLightInfo, MAX_STORED_LIGHTS> *pSortedList = NULL; 

	// now loop over the sorted list twice (if needed) to process the non camera inside and the camera inside lights separately.
	for (int outsideInsidePass = 0; outsideInsidePass<((isCamInsideAnyLights)?2:1); outsideInsidePass++)
	{
		bool cameraInside = (outsideInsidePass!=0);

		// quick test to see if the light needs to be drawn in this pass.
		if (cameraInside)
		{
			pSortedList = &insideSortsLights;
		}
		else
		{
			pSortedList = &outsideSortedLights;
		}

		atFixedArray<SortedLightInfo, MAX_STORED_LIGHTS> &sortedLights = *pSortedList;

		for (s32 i = 0; i < sortedLights.GetCount(); i++)
		{
			CLightSource & light = lights[sortedLights[i].m_index];
			float size = sortedLights[i].m_data.m_size;
			int shadowIndex = sortedLights[i].m_ShadowIndex;

			if (light.GetType() != LIGHT_TYPE_POINT &&
				light.GetType() != LIGHT_TYPE_SPOT &&
				light.GetType() != LIGHT_TYPE_CAPSULE)
			{
				continue;
			}

#if __BANK
			bool bSkipLight = SBudgetDisplay::GetInstance().IsLightDisabled(light) || CutSceneManager::GetInstance()->GetDebugManager().IsLightDisabled(light);
			if (bSkipLight)
			{
				continue;
			}
#endif
			if((light.GetFlags() & LIGHTFLAG_DELAY_RENDER) != 0)
			{
				continue;
			}
			pTexture = NULL;		

			if (BANK_ONLY(DebugDeferred::m_useTexturedLights==true &&) light.HasValidTextureKey() && light.GetTextureDict() != -1 && (light.GetFlags() & LIGHTFLAG_TEXTURE_PROJECTION) != 0)
			{
				const strLocalIndex sli(light.GetTextureDict());
				fwTxd* texDict = g_TxdStore.Get(sli);
				Assertf(texDict, "texture dictionary '%s' not present for textured light", g_TxdStore.GetName(sli));
				if (texDict)
				{
					#if RSG_PC && __DEV
						if (g_TxdStore.GetSlot(sli)->GetSubRefCount(REF_RENDER) == 0)
						{
							Warningf("texture dictionary '%s' being used without a render ref", g_TxdStore.GetName(sli));
						}
					#else
						Assertf(g_TxdStore.GetSlot(sli)->GetSubRefCount(REF_RENDER),
							"texture dictionary '%s' being used without a render ref", g_TxdStore.GetName(sli));
					#endif

					pTexture = texDict->Lookup(light.GetTextureKey());

					Assertf(pTexture, "Texture not present in dictionary '%s' for textured light %s", 
						g_TxdStore.GetName(sli),
						light.GetLightEntity()? (light.GetLightEntity()->GetParent()? light.GetLightEntity()->GetParent()->GetModelName() : "") : "");
				}
			}

			// figure out if this light will use a hi-res mesh or not
			bool useHighResMesh = false;

			eDeferredShadowType shadowType = DEFERRED_SHADOWTYPE_NONE;
			if (shadowIndex != -1)
				shadowType = CParaboloidShadow::GetDeferredShadowType(shadowIndex);

			if (shadowType == DEFERRED_SHADOWTYPE_POINT || shadowType == DEFERRED_SHADOWTYPE_HEMISPHERE)
			{
				static dev_float debugFovSize = 0.3f;
				useHighResMesh = (size >= (tanHFov * debugFovSize));
			}

			eDeferredShaders currentShader = DeferredLighting::GetShaderType(light.GetType());

			DeferredLighting::SetLightShaderParams(currentShader, &light, pTexture, 1.0f, false, useHighResMesh, MM_DEFAULT);
			DeferredLighting::SetProjectionShaderParams(currentShader, MM_DEFAULT);

#if MSAA_EDGE_PASS
			if (bUseEdgePass)
			{
				DeferredLighting::SetLightShaderParams(currentShader, &light, pTexture, 1.0f, false, useHighResMesh, MM_TEXTURE_READS_ONLY);
				DeferredLighting::SetProjectionShaderParams(currentShader, MM_TEXTURE_READS_ONLY);
			}
#endif //MSAA_EDGE_PASS

			const grcEffectTechnique technique = DeferredLighting::GetTechnique(light.GetType());
			const u32 pass = DeferredLighting::CalculatePass(&light, (pTexture != NULL), (shadowIndex>=0) && bShadows, false);

			int interiorExteriorCheck=kLightStencilAllSurfaces;

#if USE_STENCIL_FOR_INTERIOR_EXTERIOR_CHECK	
			if (light.GetFlag(LIGHTFLAG_INTERIOR_ONLY)) interiorExteriorCheck = kLightStencilInteriorOnly; 
			else if (light.GetFlag(LIGHTFLAG_EXTERIOR_ONLY)) interiorExteriorCheck = kLightStencilExteriorOnly;
#endif

			// Setup shadows
			if (shadowIndex>=0)
				CParaboloidShadow::SetDeferredShadow(shadowIndex, light.GetShadowBlur());

#if ENABLE_PIX_TAGGING
			// insert light type marker here, so it includes the stencils and facets
			const char *lightName;

			switch(light.GetType())
			{
				case LIGHT_TYPE_POINT:
					lightName = (shadowType == DEFERRED_SHADOWTYPE_NONE)?"Point":"Point (Shadowed)";
					break;
				case LIGHT_TYPE_SPOT:
					lightName = (shadowType == DEFERRED_SHADOWTYPE_NONE)? "Spot" : (shadowType==DEFERRED_SHADOWTYPE_SPOT?"Spot (Shadowed)":"HemiSphere (Shadowed)");
					break;
				case LIGHT_TYPE_CAPSULE:
					lightName = "Capsule";
					break;
				default:
					lightName = "UnknownType";
					break;
			}
			PIX_AUTO_TAG(1, lightName);
#endif // ENABLE_PIX_TAGGING


#if DEPTH_BOUNDS_SUPPORT
			if ( !SetupDepthBounds(&light) )
			{
				// The light bounds are entirely off screen, skip this light
				continue;
			}
#endif //DEPTH_BOUNDS_SUPPORT

			// we should also turn the bias off when the light greater then 0.9995 in screen space, since the .0005 depth bias will cause clipping
			// in practice we don't have shadowed point lights that large, but we sometimes do for helicopters spot lights. 
			// spot light don't really need the bias anyway, so let's just exclude them
			bool needsDepthBias = (shadowType == DEFERRED_SHADOWTYPE_POINT || shadowType == DEFERRED_SHADOWTYPE_HEMISPHERE);

#if __BANK
			if (DebugLights::m_EnableScissoringOfLights)
#endif
			{
				int scissorX;
				int scissorY;
				int scissorWidth;
				int scissorHeight;
				light.GetScissorRect(scissorX, scissorY, scissorWidth, scissorHeight);

				if (!(scissorX == prevScissorX && scissorY == prevScissorY && scissorWidth == prevScissorWidth && scissorHeight == prevScissorHeight))
				{
					if ((scissorX & scissorY & scissorWidth & scissorHeight) == -1)
					{
						GRCDEVICE.DisableScissor();
					}
					else
					{
						GRCDEVICE.SetScissor(scissorX, scissorY, scissorWidth, scissorHeight); 
					}
					prevScissorX      = scissorX;
					prevScissorY	  = scissorY;
					prevScissorWidth  = scissorWidth;
					prevScissorHeight = scissorHeight;
				}
			}

			// mark out the stencil
			if (useStencil)
			{
				PF_PUSH_MARKER(cameraInside?"Stencil (camera inside)":"Stencil (camera outside)");
				const int nNumStencilPasses = GetLightStencilPasses();
				for (int stencilPass = 0; stencilPass<nNumStencilPasses; stencilPass++)
					DrawLightStencil(&light, currentShader, technique, ((shadowIndex>=0) && bShadows) ? LIGHTPASS_STENCIL_SHADOW : LIGHTPASS_STENCIL_STANDARD, useHighResMesh, cameraInside, stencilPass, interiorExteriorCheck, needsDepthBias);

				// now for lights that have a clipping plane, lets render that clipping plane into the stencil
				if( light.GetFlag(LIGHTFLAG_USE_CULL_PLANE) BANK_ONLY(&& DebugLights::m_renderCullPlaneToStencil))
				{
					DrawCullPlaneStencil(&light, currentShader, technique);
				}

				PF_POP_MARKER();
			}

			const bool bIsPedLight = light.UseAsPedOnly();

			// Now setup state for drawing the light and then draw the light
			UseLightStencil(useStencil, cameraInside, needsDepthBias, true, bIsPedLight, EM_IGNORE);

#if __BANK
			if (drawDebugCost)
			{
				DebugLights::SetupDefLight(&light, NULL, true);

				DeferredLighting::SetLightShaderParams(DEFERRED_SHADER_DEBUG, &light, pTexture, 1.0f, false, false, MM_DEFAULT);
				DeferredLighting::SetProjectionShaderParams(DEFERRED_SHADER_DEBUG, MM_DEFAULT);

				u32 debugPass = 0;
				grcEffectTechnique debugTechnique = grcetNONE;
				DebugLights::CalcTechAndPass(&light, debugPass, debugTechnique, (shadowIndex>=0) && bShadows);

				DrawDefLight(
					&light, 
					DEFERRED_SHADER_DEBUG,
					debugTechnique, 
					debugPass, 
					useHighResMesh,  
					shadowIndex>=0,
					MM_DEFAULT);
			}
			else
#endif //__BANK
#if MSAA_EDGE_PASS
			if (bUseEdgePass)
			{
				// draw edges
				BANK_ONLY(if (DebugDeferred::m_OptimizeSceneLights & (1<<EM_EDGE0)))
				{
					UseLightStencil(useStencil, cameraInside, needsDepthBias, true, bIsPedLight, EM_EDGE0);
					DrawDefLight(
						&light, 
						currentShader,
						DeferredLighting::GetTechnique(light.GetType(), MM_SUPER_SAMPLE), 
						pass, 
						useHighResMesh, 
						shadowIndex>=0,
						MM_SUPER_SAMPLE);
				}
				
				// draw faces
				BANK_ONLY(if (DebugDeferred::m_OptimizeSceneLights & (1<<EM_FACE0)))
				{
					UseLightStencil(useStencil, cameraInside, needsDepthBias, true, bIsPedLight, EM_FACE0);
					DrawDefLight(
						&light, 
						currentShader,
						DeferredLighting::GetTechnique(light.GetType(), MM_TEXTURE_READS_ONLY), 
						pass, 
						useHighResMesh, 
						shadowIndex>=0,
						MM_TEXTURE_READS_ONLY);
				}
			}
			else
#endif // MSAA_EDGE_PASS
			{
				DrawDefLight(
					&light, 
					currentShader,
					technique, 
					pass, 
					useHighResMesh, 
					shadowIndex>=0,
					MM_DEFAULT);
			}
		}// for (int32 i = t_startLightIndex; i < t_endLightIndex; i++) ...
	}// inside/outside pass loop

	Lights::EndLightRender();

	if ((prevScissorX & prevScissorY & prevScissorWidth & prevScissorHeight) != -1)
	{
		GRCDEVICE.DisableScissor();
	}

#if DEPTH_BOUNDS_SUPPORT
	DisableDepthBounds();
#endif //DEPTH_BOUNDS_SUPPORT

	// shut down the hi stencil stuff before we return.
	if (useStencil)
	{
#if __XENON
		CHECK_HRESULT(GRCDEVICE.GetCurrent()->SetRenderState( D3DRS_HISTENCILENABLE, FALSE ));
		BANK_ONLY(if(!drawDebugCost))
		{ 
			CRenderTargets::UnlockSceneRenderTargets();
			GBuffer::CopyDepth();  // need to restore the stencil bits, not really the depth, but this is the only way...
			CRenderTargets::LockSceneRenderTargets();
		}
#elif __PS3
		//  clear scull and reset it to always true so we don't leave any side effects for other stencil users
		grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_Default,0xff);
		GCM_DEBUG(GCM::cellGcmSetScullControl(GCM_CONTEXT, CELL_GCM_SCULL_SFUNC_ALWAYS, 0x00, 0xff));
		GCM_DEBUG(GCM::cellGcmSetClearZcullSurface(GCM_CONTEXT, false, true)); 
#endif
	}

}// end of Lights::RenderSceneLights()...



// ----------------------------------------------------------------------------------------------- //

void Lights::DrawLightStencil( const CLightSource *light,  const eDeferredShaders deferredShader, const grcEffectTechnique technique,
									 const s32 shaderPass, bool useHighResMesh, bool cameraInside, int stencilPass,
									 int interiorExteriorCheck, bool needsDepthBias)
{
	if ( !AssertVerify( GetLightStencilPasses() > stencilPass ) )
	{
		return;
	}

	SetupLightStencil( cameraInside, stencilPass, interiorExteriorCheck, needsDepthBias );
	const bool isShadowCaster = (shaderPass == LIGHTPASS_STENCIL_SHADOW);
	DrawDefLight(light, deferredShader, technique, shaderPass, useHighResMesh, isShadowCaster, MM_DEFAULT);
}

void Lights::SetupLightStencil( bool bCameraInside, int nStencilPass, int nInteriorExteriorCheck, bool bUseDepthBias )
{
	Assert( GetLightStencilPasses() > nStencilPass );
	Assert( USE_STENCIL_FOR_INTERIOR_EXTERIOR_CHECK ? true : nInteriorExteriorCheck == kLightStencilAllSurfaces );

	const int nStencilTech = GetLightStencilPasses() == 1 ? kLightOnePassStencil : kLightTwoPassStencil;
	const int nInsideOutside = bCameraInside ? kLightCameraInside : kLightCameraOutside;
	const int nBiasType = bUseDepthBias ? kLightDepthBiased : kLightNoDepthBias;
	
	const bool bBackSidePass = (nStencilPass==0);
	const bool bFinalPass = (nStencilPass==1);

	const grcBlendStateHandle hBlendState = m_StencilSetup_B;
	const grcRasterizerStateHandle hRasterizerState = m_Volume_R[nStencilTech][bBackSidePass ? kLightCameraInside : kLightCameraOutside][nBiasType];;
	grcDepthStencilStateHandle hDepthStencilState = grcStateBlock::DSS_Invalid;
	int nStecnilRef = 0x0; 

	if ( nStencilTech == kLightOnePassStencil )
	{
		hDepthStencilState = m_StencilSinglePassSetup_DS[nInsideOutside];
		nStecnilRef = LIGHT_ONE_PASS_STENCIL_BIT;
	}
	else
	{
		hDepthStencilState = bBackSidePass ? m_StencilBackSetup_DS[nInteriorExteriorCheck] : m_StencilFrontSetup_DS[nInsideOutside];
		nStecnilRef = ((bFinalPass || bCameraInside) ? LIGHT_FINAL_PASS_STENCIL_BIT : LIGHT_CAMOUTSIDE_BACK_PASS_STENCIL_BIT) | LIGHT_INTERIOR_STENCIL_BIT;
	}
	
	// Mark out the stenciled area
	grcStateBlock::SetBlendState( hBlendState );
	grcStateBlock::SetDepthStencilState( hDepthStencilState, nStecnilRef );
	grcStateBlock::SetRasterizerState( hRasterizerState );
}

void Lights::DrawCullPlaneStencil( const CLightSource *light, const eDeferredShaders deferredShader, const grcEffectTechnique technique )
{
	Vector4 lightPlane = light->GetPlane();
	Vector4 camPos(VEC3V_TO_VECTOR3(grcViewport::GetCurrentCameraPosition()));
	camPos.SetW(1.0f);

	bool onFrontSide = lightPlane.Dot(camPos) >= 0.0f;

	const int nStencilTech = GetLightStencilPasses() == 1 ? kLightOnePassStencil : kLightTwoPassStencil;
	const int nCamPlaneSidedness = onFrontSide ? kLightCullPlaneCamInFront : kLightCullPlaneCamInBack;
	const int nStencilRef = nStencilTech == kLightTwoPassStencil ? LIGHT_FINAL_PASS_STENCIL_BIT : LIGHT_ONE_PASS_STENCIL_BIT;

	// mark out the stenciled area
	grcStateBlock::SetBlendState(m_StencilSetup_B);
	grcStateBlock::SetDepthStencilState(m_StencilCullPlaneSetup_DS[nStencilTech][nCamPlaneSidedness], nStencilRef);
	grcStateBlock::SetRasterizerState(grcStateBlock::RS_NoBackfaceCull);

	// Draw the actual plane
	{
		Vector3 x;
		Vector3 y;
		Vector3 z = -lightPlane.GetVector3();

		//BuildOrthoAxes(x, y, z);
		x.Cross(z, FindMinAbsAxis(z));
		x.Normalize();
		y.Cross(x, z);

		x *= light->GetRadius();
		y *= light->GetRadius();
		z *= light->GetRadius();

		const Vec3V lightPos = VECTOR3_TO_VEC3V(light->GetPosition());
		const Vec3V planeOrigin = PlaneProject(VECTOR4_TO_VEC4V(light->GetPlane()), lightPos);
		const Vec3V ox = VECTOR3_TO_VEC3V(x);
		const Vec3V oy = VECTOR3_TO_VEC3V(y);
		const Vec3V oz = VECTOR3_TO_VEC3V(z);

		// this zBias is needed for when the cull planes are sitting right on the surface of
		// where it's culling against - for example, a glass pain in a doorway.
		const ScalarV zBias = ScalarV(0.01f);
		Vec3V planeDir = VECTOR3_TO_VEC3V(-lightPlane.GetVector3());

		const Vec3V p0 = planeOrigin + ox + oy + (planeDir * zBias);
		const Vec3V p1 = planeOrigin + ox - oy + (planeDir * zBias);
		const Vec3V p2 = planeOrigin - ox + oy + (planeDir * zBias);
		const Vec3V p3 = planeOrigin - ox - oy + (planeDir * zBias);

		const Vec3V p4 = planeOrigin + ox + oy + oz;
		const Vec3V p5 = planeOrigin + ox - oy + oz;
		const Vec3V p6 = planeOrigin - ox + oy + oz;
		const Vec3V p7 = planeOrigin - ox - oy + oz;

		const grmShader* shader = DeferredLighting::GetShader(deferredShader);
		if (ShaderUtil::StartShader("light", shader, technique, LIGHTPASS_STENCIL_CULLPLANE, false))
		{
			ResetLightMeshCache();

			Vec3V poly[36] = {
				p0,p1,p2,	// the actual plane surface
				p1,p2,p3,
				p4,p5,p6,	// opposite side from the plane
				p5,p6,p7,
				p2,p0,p6,	// top of box
				p0,p6,p4,
				p1,p3,p7,	// bottom of box
				p7,p1,p5,
				p0,p1,p4,	// front of box
				p1,p4,p5,
				p2,p3,p6,	// back of box
				p3,p6,p7
			};

			grcBegin(drawTris, 36);
			for( int i = 0; i < 12; i++ )
			{
				grcVertex3f(poly[(i*3)+0]);
				grcVertex3f(poly[(i*3)+1]);
				grcVertex3f(poly[(i*3)+2]);
			}
			grcEnd();

			//grcDrawPolygon((Vector3*)poly, 36, NULL, true, Color32(255, 255, 255, 255));
			ShaderUtil::EndShader(shader, false);
		}
	}
}

void Lights::UseLightStencil( bool bUseStencilTest, bool bCameraInside, bool bUseDepthBias, bool DEPTHBOUNDS_STATEBLOCK_ONLY(bUseDepthBounds), bool bIsPedLight, EdgeMode edgeMode )
{
	const int nDepthBounds = (0 DEPTHBOUNDS_STATEBLOCK_ONLY(|| bUseDepthBounds)) BANK_ONLY(&& DebugLighting::m_useLightsDepthBounds) ? kLightDepthBoundsEnabled : kLightDepthBoundsDisabled;
	const int nStencilTech = edgeMode != EM_IGNORE ? kLightOnePassStencil : (!bUseStencilTest ? kLightNoStencil : ((GetLightStencilPasses() == 1) ? kLightOnePassStencil : kLightTwoPassStencil));
	const int nInsideOutside = bCameraInside ? kLightCameraInside : kLightCameraOutside;
	const int nBiasType = bUseDepthBias ? kLightDepthBiased : kLightNoDepthBias;
	const int nStencilRef = nStencilTech == kLightOnePassStencil ? (edgeMode == EM_FACE0 ? 0 : LIGHT_ONE_PASS_STENCIL_BIT) : LIGHT_FINAL_PASS_STENCIL_BIT;

	grcStateBlock::SetBlendState(m_SceneLights_B);
	grcStateBlock::SetRasterizerState(m_Volume_R[nStencilTech][nInsideOutside][nBiasType]);

	if(bIsPedLight)
	{
		grcStateBlock::SetDepthStencilState(nInsideOutside ? m_StencilPEDOnlyLessEqual_DS : m_StencilPEDOnlyGreater_DS, DEFERRED_MATERIAL_PED);
	}
	else
	{
		grcStateBlock::SetDepthStencilState(m_Volume_DS[nStencilTech][nInsideOutside][nDepthBounds], nStencilRef);
	}
}

// ----------------------------------------------------------------------------------------------- //

void Lights::SetupVolumeStencil( bool cameraInside, int stencilPass )
{
	bool backSidePass = (stencilPass==0);
	bool finalPass = (stencilPass==1);
	int ref = ((finalPass || cameraInside) ? LIGHT_FINAL_PASS_STENCIL_BIT : LIGHT_CAMOUTSIDE_BACK_PASS_STENCIL_BIT) | LIGHT_INTERIOR_STENCIL_BIT;

	if( finalPass == cameraInside) {
		if ( !cameraInside)
			ref |=LIGHT_FINAL_PASS_STENCIL_BIT;
		else
			ref |=LIGHT_CAMOUTSIDE_BACK_PASS_STENCIL_BIT;
	}

	// mark out the stenciled area
	grcStateBlock::SetBlendState(m_StencilSetup_B);
	int state = cameraInside ? kLightCameraInside : kLightCameraOutside;
	grcStateBlock::SetDepthStencilState(backSidePass ? m_StencilBackDirectSetup_DS[state] : m_StencilFrontDirectSetup_DS[state], ref);
	grcStateBlock::SetRasterizerState(backSidePass ? m_Volume_R[kLightTwoPassStencil][kLightCameraInside][kLightNoDepthBias] : m_Volume_R[kLightTwoPassStencil][kLightCameraOutside][kLightNoDepthBias]);  // use kLightCameraOutside is same as default, but with depth bias	
}

void Lights::UseVolumeStencil()
{
	grcStateBlock::SetRasterizerState(m_Volume_R[kLightTwoPassStencil][kLightCameraOutside][kLightNoDepthBias]);
	grcStateBlock::SetDepthStencilState(m_Volume_DS[kLightTwoPassStencil][kLightCameraOutside][kLightDepthBoundsDisabled],LIGHT_FINAL_PASS_STENCIL_BIT);
}

void Lights::SetupDirectStencil()
{
	grcStateBlock::SetDepthStencilState(m_StencilDirectSetup_DS, LIGHT_FINAL_PASS_STENCIL_BIT);
	grcStateBlock::SetBlendState(m_StencilSetup_B);
	grcStateBlock::SetRasterizerState(grcStateBlock::RS_NoBackfaceCull);
}

// ----------------------------------------------------------------------------------------------- //

CLightShaft* Lights::AddLightShaft(BANK_ONLY(CLightEntity* entity))
{
	if (m_numLightShafts < MAX_STORED_LIGHT_SHAFTS)
	{
		CLightShaft* shaft = &m_lightShaft[m_numLightShafts++];
		m_numLightShaftsBuf[m_currentLightShaftBuffer] = m_numLightShafts;
#if __BANK
		shaft->m_entity = entity;
#if 0 && __DEV
		for (int i = 0; i < m_numLightShafts - 1; i++)
		{
			Assert(m_lightShaft[i].m_entity != entity);
		}
#endif // __DEV
#endif // __BANK
		return shaft;
	}

	return NULL;
}

// ----------------------------------------------------------------------------------------------- //

void Lights::SetupDepthForVolumetricLights()
{
	//if there are any shadowed volumetric lights or any forced low res volumetric lights, lets perform the downsample
	if (g_SortedVisibleLights.GetNumShadowedVolumetricLights() > 0 || g_SortedVisibleLights.AnyForcedLowResVolumeLightsPresent())
	{
		CRenderTargets::DownsampleDepth();
	}

}
// ----------------------------------------------------------------------------------------------- //
static bool UseConditionalVolumeRendering()
{
#if RSG_PC
	return GRCDEVICE.GetDxFeatureLevel() >= 1100 || GRCDEVICE.GetManufacturer() != ATI;
#else
	return true;
#endif
}

void Lights::InitOffscreenVolumeLightsRender()
{
	// This must be done on the render thread
	if ( !g_bVolumeLightQueryInitialized ) 
	{ 
		g_VolumeLightQuery = GRCDEVICE.CreateConditionalQuery();
		g_bVolumeLightQueryInitialized = true;
	}

	CRenderTargets::UnlockSceneRenderTargets();

	DeferredLighting::SetShaderGBufferTargets();
	
#if LIGHT_VOLUME_USE_LOW_RESOLUTION
	// No need to call downsample depth here. It is not thread safe to do so (if multiple alpha render threads are being used) and it should have already been
	// called by a DLC issued in RenderPhaseStd::BuildDrawList anyway (making this one redundant). I'm leaving it here commented out as a form of documentation.
	//CRenderTargets::DownsampleDepth();

	//Clear Stencil for Volume rendering
	PF_PUSH_MARKER("Clear Stencil For Volume Render");
	{
		const grcRenderTarget* pQuarterDepth = CRenderTargets::GetDepthBufferQuarter_Read_Target(true);
		grcTextureFactory::GetInstance().LockRenderTarget(0, CRenderTargets::GetVolumeOffscreenBuffer(), pQuarterDepth);
		GRCDEVICE.Clear(true, Color32(0.0f, 0.0f, 0.0f, 0.0f), false, 1.0f, true, 0x00 );
			
		CShaderLib::SetGlobalScreenSize(Vector2((float)CRenderTargets::GetVolumeOffscreenBuffer()->GetWidth(), (float)CRenderTargets::GetVolumeOffscreenBuffer()->GetHeight()));
		DeferredLighting::SetShaderGBufferTarget(GBUFFER_DEPTH, CRenderTargets::GetDepthBufferQuarter());
	}
	PF_POP_MARKER();

	grcStateBlock::SetDepthStencilState(ms_dsVolumeLightRender, g_VolumeRenderRefMaterial);
	grcStateBlock::SetRasterizerState(m_VolumeFX_R);
	grcStateBlock::SetBlendState(m_VolumeFX_B);
#else // LIGHT_VOLUME_USE_LOW_RESOLUTION
	#if DEVICE_MSAA
		#if __D3D11
			const grcRenderTarget* pDepthBuffer = CRenderTargets::GetDepthBufferResolved_ReadOnly();
		#else
			const grcRenderTarget* pDepthBuffer = CRenderTargets::GetDepthResolved();
		#endif
	#else
		const grcRenderTarget* pDepthBuffer = CRenderTargets::GetDepthBuffer();
	#endif
	grcTextureFactory::GetInstance().LockRenderTarget(0, CRenderTargets::GetVolumeOffscreenBuffer(), pDepthBuffer);
	GRCDEVICE.Clear(true, Color32(0.0f, 0.0f, 0.0f, 0.0f), false, 1.0f, 0);

	CShaderLib::SetGlobalScreenSize(Vector2((float)CRenderTargets::GetVolumeOffscreenBuffer()->GetWidth(), (float)CRenderTargets::GetVolumeOffscreenBuffer()->GetHeight()));
	DeferredLighting::SetShaderGBufferTarget(GBUFFER_DEPTH, pDepthBuffer);

	grcStateBlock::SetBlendState(grcStateBlock::BS_Add);
#endif // LIGHT_VOLUME_USE_LOW_RESOLUTION

	if (UseConditionalVolumeRendering())
	{
		// Begin query: we only do interleaved-reconstruction (and up-sampling in the case of quarter-res volumes) if something was actually rendered.
		GRCDEVICE.BeginConditionalQuery(g_VolumeLightQuery);
	}
}

void Lights::ShutDownOffscreenVolumeLightsRender(const grcRenderTarget* LOW_RES_VOLUME_LIGHT_ONLY(pFullResColorTarget), const grcRenderTarget* LOW_RES_VOLUME_LIGHT_ONLY(pFullResDepthTarget))
{
	LOW_RES_VOLUME_LIGHT_ONLY( Assert(NULL != pFullResColorTarget) );
	LOW_RES_VOLUME_LIGHT_ONLY( Assert(NULL != pFullResDepthTarget) );

	if (UseConditionalVolumeRendering())
	{
		// End query: we only do interleaved-reconstruction (and up-sampling in the case of quarter-res volumes) if something was actually rendered.
		GRCDEVICE.EndConditionalQuery(g_VolumeLightQuery);
	}

	// Unbind the off screen volumetric buffer
	grcTextureFactory::GetInstance().UnlockRenderTarget(0);
	
	// Adding a sync since we're getting incorrect conditional rendering results on occasion on PS4 (GTA5 B*2063181). I'm not sure if the counter's DMA/reset is not
	// completed in time during the BeginConditionalQuery or if the "ready bit" isn't cleared properly or if there's just caching issue prior to BeginConditionalRender.
	// Either way, this fixes the issue. In future titles/SDKs, we can remove the wait and see what happens.
	ORBIS_ONLY( GRCDEVICE.GpuWaitOnPreviousWrites() ); 

	if (UseConditionalVolumeRendering())
	{
		GRCDEVICE.BeginConditionalRender(g_VolumeLightQuery);
	}

	// Reconstruct Phase
	PF_PUSH_MARKER("Volumetric Lighting Reconstruction");
	{
		#if LIGHT_VOLUME_USE_LOW_RESOLUTION
			const grcRenderTarget* pQuarterDepth = CRenderTargets::GetDepthBufferQuarter_Read_Target(false);
			// Bind the reconstruction target
			grcTextureFactory::GetInstance().LockRenderTarget(0, CRenderTargets::GetVolumeReconstructionBuffer(), pQuarterDepth);
			GRCDEVICE.Clear(true, Color32(0.0f, 0.0f, 0.0f, 0.0f), false, 1.0f, 0);
			grcStateBlock::SetDepthStencilState(ms_dsVolumeLightReconstruct, 0x00);
			grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
		#else //LIGHT_VOLUME_USE_LOW_RESOLUTION, RSG_ORBIS
			CRenderTargets::LockSceneRenderTargets();
			D3D11_ONLY( CRenderTargets::LockReadOnlyDepth(false) );
			grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_IgnoreDepth);
			grcStateBlock::SetBlendState(m_VolumeFX_B);
		#endif //LIGHT_VOLUME_USE_LOW_RESOLUTION, RSG_ORBIS

		// Interleave reconstruction
		grcStateBlock::SetRasterizerState(grcStateBlock::RS_NoBackfaceCull);
		DeferredLighting::SetShaderTargetsForVolumeInterleaveReconstruction();
		const int reconstructionPass = LIGHT_VOLUME_USE_LOW_RESOLUTION ? LIGHTVOLUME_INTERLEAVE_RECONSTRUCTION_PASS_WEIGHTED : LIGHTVOLUME_INTERLEAVE_RECONSTRUCTION_PASS_UNWEIGHTED_ALPHA_CLIPPED;

		// Reconstruction technique is in deferred_volume.fx
		if (ShaderUtil::StartShader("Reconstruction", DeferredLighting::m_deferredShaders[MM_DEFAULT][DEFERRED_SHADER_VOLUME],
			DeferredLighting::GetVolumeInterleaveReconstructionTechnique(), reconstructionPass, false)) 
		{
			grcDrawSingleQuadf(-1.0f,1.0f,1.0f,-1.0f,0.0f,0.0f,0.0f,1.0f,1.0f,Color32(0.0f,0.0f,0.0f,0.0f));
			ShaderUtil::EndShader(DeferredLighting::m_deferredShaders[MM_DEFAULT][DEFERRED_SHADER_VOLUME], false);	
		}	
	}
	PF_POP_MARKER();

	#if LIGHT_VOLUME_USE_LOW_RESOLUTION
		grcTextureFactory::GetInstance().UnlockRenderTarget(0);

		PF_PUSH_MARKER("Upsample Volumetric Lighting");
		{
			// Now composite it back to main target using bilinear/bilateral up-sampling
			CShaderLib::UpdateGlobalGameScreenSize();

			UpsampleHelper::UpsampleParams upsampleParams;
			upsampleParams.highResUpsampleDestination = pFullResColorTarget;
			upsampleParams.highResDepthTarget = pFullResDepthTarget;
			upsampleParams.lowResUpsampleSource = CRenderTargets::GetVolumeReconstructionBuffer();
			upsampleParams.lowResDepthTarget = CRenderTargets::GetDepthBufferQuarter();
			upsampleParams.blendType = grcCompositeRenderTargetHelper::COMPOSITE_RT_BLEND_ADD;
			upsampleParams.maskUpsampleByAlpha = BANK_SWITCH(DebugLighting::m_VolumeLightingMaskedUpsample, true);
			upsampleParams.lockTarget	= false;
			upsampleParams.unlockTarget	= false;

			grcStateBlock::SetDepthStencilState( grcStateBlock::DSS_IgnoreDepth );

			grcTextureFactory::GetInstance().LockRenderTarget(0, upsampleParams.highResUpsampleDestination, NULL);
			UpsampleHelper::PerformUpsample(upsampleParams);
			grcTextureFactory::GetInstance().UnlockRenderTarget(0);

			#if DEVICE_MSAA
				const grcRenderTarget* pGBufferDepth = CRenderTargets::GetDepthResolved();
			#else
				const grcRenderTarget* pGBufferDepth = CRenderTargets::GetDepthBuffer();
			#endif

			DeferredLighting::SetShaderGBufferTarget(GBUFFER_DEPTH,pGBufferDepth);
			CRenderTargets::LockSceneRenderTargets(); 
		}
		PF_POP_MARKER();
	#endif //LIGHT_VOLUME_USE_LOW_RESOLUTION

	if (UseConditionalVolumeRendering())
	{
		GRCDEVICE.EndConditionalRender(g_VolumeLightQuery);
	}
}


bool IsRenderLowResVolumetricLight(const CLightSource* light, bool bIsShadowedLight)
{
	if(bIsShadowedLight || light->GetExtraFlag(EXTRA_LIGHTFLAG_FORCE_LOWRES_VOLUME))
	{
		return true;
	}

	return false;
}

void DrawVolumetricFxLights( bool bDrawLowRes, bool bDrawShadowedLightsAsUnshadowed, const grcTexture* pDepthBuffer )
{
	using namespace Lights;

	Assert( NULL != pDepthBuffer );
	MSAA_ONLY( Assert( pDepthBuffer->GetMSAA()<=1 ) ); // This should be a single fragment (non-msaa) depth buffer

	int nBufferID = gRenderThreadInterface.GetRenderBuffer();
	u32 nVisibleLightsCount = g_LightsVisibilitySortCount[nBufferID];
	LightsVisibilityOutput* pLightsVisibleSorted = &(g_LightsVisibilitySortOutput[nBufferID][0]);
	atFixedBitSet<MAX_STORED_LIGHTS>* pVisibilityCamInsideOutput = &(g_LightsVisibilityCamInsideOutput[nBufferID]);
	atFixedBitSet<MAX_STORED_LIGHTS>& rLightVisibleLastFrameList = g_LightsVisibilityVisibleLastFrameOutput[nBufferID]; // NOTE: LightVisibleLastFrameList is only valid for shadows lights with static geometry
	
	// Begin stream source caching
	Lights::BeginLightRender();

	for( u32 i=0; i < nVisibleLightsCount; i++ )
	{
		const u16 nLightIndex = pLightsVisibleSorted[i].index;
		const CLightSource* light = &m_renderLights[nLightIndex];

		eLightType lightType  = light->GetType();
		const u32  lightFlags = light->GetFlags();

		if ((lightFlags & LIGHTFLAG_DRAW_VOLUME) == 0)
		{
			continue;
		}

		if (light->GetVolumeIntensity()*light->GetVolumeSizeScale() == 0.0f)
		{
			continue;
		}

		eDeferredShaders currentShader = DeferredLighting::GetShaderType(lightType);

		
		// Even shadowed lights may be drawn unshadowed. If we don't find a shadow map index, or if this is a point light, then we'll just draw this as an unshadowed light
		bool bIsShadowedLight = false;
		int nShadowIndex = -1;

		// Setup shadows 
		if (((lightType == LIGHT_TYPE_SPOT) || (lightType == LIGHT_TYPE_POINT)) && light->GetFlag(LIGHTFLAG_CAST_DYNAMIC_GEOM_SHADOWS | LIGHTFLAG_CAST_STATIC_GEOM_SHADOWS) )
		{			
			// Using shadow index from parabaloid shadows as this is the way its done in RenderSceneLights()
			nShadowIndex = CParaboloidShadow::GetLightShadowIndex(*light, rLightVisibleLastFrameList.IsSet(nLightIndex)); 
			if ( nShadowIndex >= 0 )
			{	
				bIsShadowedLight = !bDrawShadowedLightsAsUnshadowed;

			}
		}
		
		//only render it when required
		if(bDrawLowRes != IsRenderLowResVolumetricLight(light, bIsShadowedLight))
		{
			continue;
		}

		// Only hook up the shadow map if we're actually planning on rendering with it 
		if(bIsShadowedLight)
		{
			Assertf(nShadowIndex>=0, "Shadowed light using invalid shadow index");
			CParaboloidShadow::SetDeferredShadow(nShadowIndex, light->GetShadowBlur());
		}

		const float radiusScale = light->GetVolumeSizeScale();
		const float radius      = light->GetRadius()*radiusScale;
		float intensity = light->GetVolumeIntensity();

		if (lightType == LIGHT_TYPE_POINT)
		{
			intensity /= radius; // normalise intensity by 1/radius, so larger lights don't get brighter
		}
		else if (lightType == LIGHT_TYPE_SPOT)
		{
			const float cosOuterAngle = light->GetConeCosOuterAngle();
			const float sinOuterAngle = sqrtf(1.0f - cosOuterAngle*cosOuterAngle);

			intensity /= radius*sinOuterAngle;
		}
		else if (lightType == LIGHT_TYPE_CAPSULE)
		{
			// TODO: Add in scaling for capsule light
		}

		// Decrease intensity, so the existing ranges look ok
		intensity /= 16.0f;

		const eLightType lightMeshType = LIGHT_TYPE_POINT; // always use pointlight mesh, even for spotlights (we need the tessellation)
		const bool bCamInsideLight = pVisibilityCamInsideOutput->IsSet(nLightIndex);
	
		grcEffectTechnique technique = DeferredLighting::GetVolumeTechnique(lightType, MM_SINGLE);
		const eLightVolumePass pass = DeferredLighting::CalculateVolumePass(light, bIsShadowedLight, bCamInsideLight);
		grmShader* volumeShader = DeferredLighting::GetShader(currentShader, MM_SINGLE);

		const ScalarV volumeNearPlaneFade = light->IsVolumeUseHighNearPlaneFade() ? 
			BANK_SWITCH(DebugLighting::m_VolumeHighNearPlaneFade, LIGHT_VOLUME_HIGH_NEAR_PLANE_FADE) :
			BANK_SWITCH(DebugLighting::m_VolumeLowNearPlaneFade, LIGHT_VOLUME_LOW_NEAR_PLANE_FADE);

		// volumeParams
		const Vec4V volumeParams[LIGHTVOLUME_PARAM_COUNT] =
		{
			Vec4V(ScalarV(intensity), volumeNearPlaneFade, Vec2V(V_ZERO)),
			light->GetVolOuterColour(),
		};

		// use the lower resolution light mesh for lights that are smaller than 25 degrees wide, there's just no point in higher res for these small lights.
		const bool bRenderWithHiResMesh = (light->GetType() != LIGHT_TYPE_SPOT || light->GetConeOuterAngle() > 25.0f);

		DeferredLighting::SetDeferredVarArray(currentShader, DEFERRED_SHADER_LIGHT_VOLUME_PARAMS, volumeParams, LIGHTVOLUME_PARAM_COUNT, MM_SINGLE);
		DeferredLighting::SetLightShaderParams(currentShader, light, NULL, radiusScale, false, bRenderWithHiResMesh, MM_SINGLE);
		DeferredLighting::SetProjectionShaderParams(currentShader, MM_SINGLE);
		DeferredLighting::SetDeferredVar(currentShader, DEFERRED_SHADER_LIGHT_TEXTURE, DeferredLighting::GetVolumeTexture(), MM_SINGLE);
		DeferredLighting::SetDeferredVar(currentShader, DEFERRED_SHADER_LIGHT_TEXTURE2, pDepthBuffer, MM_SINGLE);

		// set the depth state based on whether we are inside or outside the light volume
		CompileTimeAssert( SUPPORT_HQ_VOLUMETRIC_LIGHTS ? LIGHTVOLUMEPASS_NUM_PASSES == 8 : LIGHTVOLUMEPASS_NUM_PASSES == 4 ); // Compile time assert to force you to revisit below logic when adding a new pass
		if ( pass == LIGHTVOLUMEPASS_STANDARD || pass == LIGHTVOLUMEPASS_SHADOW HQ_VOLUMETRIC_LIGHTS_ONLY( || pass == LIGHTVOLUMEPASS_STANDARD_HQ || pass == LIGHTVOLUMEPASS_SHADOW_HQ ) )
		{
			grcStateBlock::SetRasterizerState(m_VolumeFXInside_R);
			grcStateBlock::SetDepthStencilState(ms_dsVolumeInsideLightRender, g_VolumeRenderRefMaterial);
		}
		else
		{
			grcStateBlock::SetRasterizerState(m_VolumeFX_R);
			grcStateBlock::SetDepthStencilState(ms_dsVolumeLightRender, g_VolumeRenderRefMaterial);
		}

		if (ShaderUtil::StartShader("Volume", volumeShader, technique, pass)) 
		{
			RenderVolumeLight(lightMeshType, bRenderWithHiResMesh, true, light->IsCastShadows());
			ShaderUtil::EndShader(volumeShader);
		}
	}
	
	// Clearing stream source after finishing with all the lights only for 360 as we use the stream source cache only for lights
	Lights::EndLightRender();
}

void Lights::RenderVolumetricLightingFx(bool bEnableShadows)
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	grcViewport::SetCurrentWorldIdentity();

	// Set the state blocks
	PostFX::SetLDR16bitHDR16bit();
	grcStateBlock::SetBlendState(m_VolumeFX_B, ~0U, ~0ULL);
	grcStateBlock::SetDepthStencilState(m_VolumeFX_DS);
	grcStateBlock::SetRasterizerState(m_VolumeFXInside_R);

	const grcRenderTarget* pDepthBuffer = CRenderTargets::GetDepthBuffer();
	bool useRoDepth = false;
#if DEVICE_MSAA
	if (GRCDEVICE.GetMSAA() > 1)
	{
		pDepthBuffer = CRenderTargets::GetDepthResolved();
		useRoDepth = false;
	}else {
		useRoDepth = true;
	}
#endif //DEVICE_MSAA

	D3D11_ONLY(if (useRoDepth) { pDepthBuffer = CRenderTargets::LockReadOnlyDepth(false, false); })

	PF_PUSH_MARKER("Light Shafts");
	Lights::RenderAllVolumeLightShafts(bEnableShadows, pDepthBuffer);
	PF_POP_MARKER();

	D3D11_ONLY(if (useRoDepth) { CRenderTargets::UnlockReadOnlyDepth(); });

	bool bDrawVolumeLights = g_SortedVisibleLights.AnyVolumeLightsPresent();
	#if RSG_BANK
		if ( !DebugLighting::m_drawSceneLightVolumes || SBudgetDisplay::GetInstance().GetDisableVolumetricLights() )
		{
			bDrawVolumeLights = false;
		}
		else if (DebugLighting::m_drawSceneLightVolumesAlways)
		{
			bDrawVolumeLights = true;
		}
	#endif // RSG_BANK

	PF_PUSH_MARKER("Volume Lights");
	{
		if( bDrawVolumeLights )
		{
			if (true BANK_ONLY(&& DebugLighting::m_usePreAlphaDepthForVolumetricLight))
			{
				pDepthBuffer = CRenderTargets::GetDepthBuffer_PreAlpha();
				useRoDepth = false;
			}else
#if DEVICE_MSAA
			if (GRCDEVICE.GetMSAA())
			{
				// leave pDepthBuffer untouched
				useRoDepth = false;
			}else
#endif //DEVICE_MSAA
			{
				useRoDepth = true;
			}

			D3D11_ONLY(if (useRoDepth) { pDepthBuffer = CRenderTargets::LockReadOnlyDepth(false, false); })

			// Draw mostly unshadowed volume lights (no forced low res volume lights) to the back buffer
			PF_PUSH_MARKER("Hi-Res Volume Lights");	
			DrawVolumetricFxLights( false, !bEnableShadows, pDepthBuffer );
			PF_POP_MARKER();

			D3D11_ONLY(if (useRoDepth) { CRenderTargets::UnlockReadOnlyDepth(); });

			// Now draw shadowed lights/ Forced unshadowed volume lights to an offscreen target
			PF_PUSH_MARKER("Low Res Volume Lights");	
			// This condition fixes a GPU crash on AMD 4870 (bugs 2243422, 2244754). Could be GRCDEVICE.IsReadOnlyDepthAllowed(), but it cuts off more cards.
			if ((g_SortedVisibleLights.GetNumShadowedVolumetricLights() > 0 && bEnableShadows) || g_SortedVisibleLights.AnyForcedLowResVolumeLightsPresent())
			{
				InitOffscreenVolumeLightsRender(); // Init off screen volume light rendering (this may switch us to quarter res target if that feature is enabled)
				PF_PUSH_MARKER("Shadowed Lights");	
				DrawVolumetricFxLights( true, !bEnableShadows, LOW_RES_VOLUME_LIGHT_SWITCH(CRenderTargets::GetDepthBufferQuarter(), pDepthBuffer) );
				PF_POP_MARKER();	
				ShutDownOffscreenVolumeLightsRender( CRenderTargets::GetBackBuffer(false),
					(MSAA_ONLY(GRCDEVICE.GetMSAA() ? CRenderTargets::GetDepthResolved() :) CRenderTargets::GetDepthBuffer(false))
					);
			}
			PF_POP_MARKER();
		}
	}
	PF_POP_MARKER();
}

// ----------------------------------------------------------------------------------------------- //

void Lights::RenderAllVolumeLightShafts(bool bEnableShadows, const grcTexture* pDepthBuffer)
{
	if ( m_numRenderLightShafts BANK_ONLY(&& DebugLighting::m_drawLightShafts) )
	{
		#if LIGHTSHAFT_USE_SHADOWS
			bool bShadowsInited = false;
		#endif // LIGHTSHAFT_USE_SHADOWS

		DeferredLighting::SetShaderDepthTargetsForDeferredVolume(pDepthBuffer);
		DeferredLighting::SetLightShaftShaderParamsGlobal();

		for (int p = 0; p < m_numRenderLightShafts; p++)
		{
			const CLightShaft* shaft = &m_renderLightShaft[p];

			#if RSG_BANK
				const CEntity* isolatedEntity = DebugLights::GetIsolatedLightShaftEntity();

				if ((isolatedEntity && isolatedEntity != shaft->m_entity) || SBudgetDisplay::GetInstance().GetDisableVolumetricLights())
				{
					continue;
				}
			#endif // __BANK

			if (DeferredLighting::SetLightShaftShaderParams(shaft))
			{
				int densityType = shaft->m_densityType;

				#if LIGHTSHAFT_USE_SHADOWS
					if (densityType == LIGHTSHAFT_DENSITYTYPE_SOFT_SHADOW ||
						densityType == LIGHTSHAFT_DENSITYTYPE_SOFT_SHADOW_HD)
					{
						if (bEnableShadows)
						{
							if (!bShadowsInited)
							{
								bShadowsInited = true;
								CRenderPhaseCascadeShadowsInterface::SetSharedShaderVars();
							}
						}
						else
						{
							densityType = LIGHTSHAFT_DENSITYTYPE_SOFT; // use non-shadow density type
						}
					}
				#endif // LIGHTSHAFT_USE_SHADOWS

				if (ShaderUtil::StartShader("Light Shaft", DeferredLighting::m_deferredShaders[MM_DEFAULT][DEFERRED_SHADER_VOLUME],
					DeferredLighting::m_shaderLightShaftVolumeTechniques[shaft->m_volumeType],densityType))
				{
					RenderVolumeLightShaft(false);
					ShaderUtil::EndShader(DeferredLighting::m_deferredShaders[MM_DEFAULT][DEFERRED_SHADER_VOLUME]);
				}
			}
		}
	}
}
// ----------------------------------------------------------------------------------------------- //

void Lights::Shutdown()
{
	if ( g_bVolumeLightQueryInitialized )
	{
		GRCDEVICE.DestroyConditionalQuery(g_VolumeLightQuery);
		g_bVolumeLightQueryInitialized = false;
	}
}

// ----------------------------------------------------------------------------------------------- //

static void ResetSceneLightOrder();

// ----------------------------------------------------------------------------------------------- //

// this need to be updated before the cutsManager::PreSceneUpdate() for shadowed lights to work
void Lights::ResetSceneLights()
{
	m_numLightShafts = 0;

#if __BANK
	//don't mess with lights if debugging them
	if (DebugLights::m_freeze)
	{
		return;
	}

	DebugLights::m_numOverflow = 0;
#endif

	if (++m_currentLightBuffer == 2)
		m_currentLightBuffer = 0;

	s_LightsAddedToDrawList[m_currentLightBuffer] = false;

	// copy just the shadowable lights to the previous shadow list (saves time later for visibility checks)
	int index = 0;
	for (int i=0;i<m_numSceneLights;i++)
	{
		if (m_sceneLights[i].GetShadowTrackingId()!=0)
			m_prevSceneShadowLights[index++] = m_sceneLights[i]; // Copy includes exclusion list Refs.
	}
	m_numPrevSceneShadowLights=index;		

	m_sceneLights = m_sceneLightsBuffer[m_currentLightBuffer];
	m_numSceneLightsBuf[m_currentLightBuffer] = 0;

	if (++m_currentLightShaftBuffer == 2)
		m_currentLightShaftBuffer = 0;
		
	m_lightShaft = m_lightShaftsBuffer[m_currentLightShaftBuffer];
	m_numLightShaftsBuf[m_currentLightShaftBuffer] = 0;

	m_numSceneLights = 0;

	lightingData.m_lightHashes.Reset();
	lightingData.m_coronaLightHashes.Reset();

	ResetSceneLightOrder();
}

// ----------------------------------------------------------------------------------------------- //

static void ResetSceneLightOrder()
{
	g_lightOrderSet.clear();
	g_lightsNeedSorting = false;
}

// ----------------------------------------------------------------------------------------------- //

static int GetSceneLightOrderIndex(float sortVal)
{
	lightOrderIndex end_node = g_lightOrderSet[g_lightOrderSet.size() - 1];

	if (sortVal < end_node.sortVal)
	{
		const u8 index = end_node.index;

		lightOrderIndex new_node;
		new_node.index = index;
		new_node.sortVal = sortVal;

		g_lightOrderSet[g_lightOrderSet.size() - 1] = new_node;
		std::sort(g_lightOrderSet.begin(), g_lightOrderSet.end());

		return index;
	}
	else
	{
		return -1;
	}
}

// ----------------------------------------------------------------------------------------------- //

void AddLightHash(const CLightEntity *lightEntity, const u32 lightIndex)
{
	if (!CLODLights::Enabled())
		return;

	LightingData& lightingData = Lights::GetUpdateLightingData();

	u32 hash = UINT_MAX;

	if (lightEntity != NULL)
	{
		hash = CLODLightManager::GenerateLODLightHash(lightEntity);
		Assertf(lightIndex < MAX_STORED_LIGHTS, "Trying to add a light hash out of range (%d / %d)", lightIndex, MAX_STORED_LIGHTS);
	}

	if (lightIndex == (u32)lightingData.m_lightHashes.GetCount())
	{
		lightingData.m_lightHashes.Push(hash);
	}
	else if (lightIndex < lightingData.m_lightHashes.GetCount())
	{
		lightingData.m_lightHashes[lightIndex] = hash;
	}
	else
	{
		Assertf(false, "Light hashses and scene lights have gone out of sync");
	}

	#if __BANK
		if (DebugLighting::m_showLODLightHashes)
		{
			char buffer[255];
			sprintf(buffer, "Light Entity hash: %u", hash);
			Vec3V vPosition = VECTOR3_TO_VEC3V(Lights::m_sceneLights[lightIndex].GetPosition());
			grcDebugDraw::Text(vPosition, Color32(255, 255, 0, 255), 0, 10, buffer, true);
		}
	#endif
}

// ----------------------------------------------------------------------------------------------- //

static s32 AddSceneLightOrdered(const CLightSource& sceneLight, const float sortVal, const CLightEntity *lightEntity)
{
	if (Lights::m_numSceneLights < MAX_STORED_LIGHTS)
	{
		// Add sorting node
		lightOrderIndex new_node;
		new_node.index = (u8)Lights::m_numSceneLights;
		new_node.sortVal = sortVal;
		g_lightOrderSet.Append() = new_node;

		Lights::m_sceneLights[Lights::m_numSceneLights] = sceneLight;
		AddLightHash(lightEntity, Lights::m_numSceneLights);

		Lights::m_numSceneLights++;

		return Lights::m_numSceneLights - 1;
	}
	else
	{
		if (g_lightsNeedSorting)
		{
			std::sort(g_lightOrderSet.begin(), g_lightOrderSet.begin() + Lights::m_numSceneLights);
			g_lightsNeedSorting = false;
		}

		const int lightIndex = GetSceneLightOrderIndex(sortVal);
		if (lightIndex != -1)
		{
			AddLightHash(lightEntity, lightIndex);
			Lights::m_sceneLights[lightIndex] = sceneLight;
			g_lightsNeedSorting = true;
			return lightIndex;
		}

		#if __BANK
			Assertf(false, "We've added more than %d HD lights, things will degenerate to a flickery mess", MAX_STORED_LIGHTS);
			DebugLights::m_numOverflow++;
		#endif	
	}

	return -1;
}

// ----------------------------------------------------------------------------------------------- //

void Lights::RegisterBrokenLight(const CEntity *pEntity, s32 lightID)
{
	// Record the hash of this light into the broken light array (ring buffer)
	u32	hash = 0;
	hash = CLODLightManager::GenerateLODLightHash(pEntity, lightID);
	RegisterBrokenLight(hash);
}

// ----------------------------------------------------------------------------------------------- //

void Lights::RegisterBrokenLight(u32 hash)
{
	int thisIDX = lightingData.m_brokenLightHashes.Find(hash);
	if(thisIDX == -1)
	{
		int newIDX = lightingData.m_BrokenLightHashesHeadIDX;
		lightingData.m_brokenLightHashes[newIDX] = hash;
		lightingData.m_BrokenLightHashesHeadIDX = (lightingData.m_BrokenLightHashesHeadIDX+1) % MAX_STORED_BROKEN_LIGHTS;
		lightingData.m_BrokenLightHashesCount = Min(++lightingData.m_BrokenLightHashesCount, (u16)MAX_STORED_BROKEN_LIGHTS);

#if GTA_REPLAY
		if(CReplayMgr::ShouldRecord())
		{
			CReplayMgr::RecordFx(CPacketLightBroken(hash));
		}
#endif
	}
}

// ----------------------------------------------------------------------------------------------- //

void Lights::RemoveRegisteredBrokenLight(u32 hash)
{
	int thisIDX = lightingData.m_brokenLightHashes.Find(hash);
	if(thisIDX != -1)
	{
		int headIDX = lightingData.m_BrokenLightHashesHeadIDX;
		while(thisIDX != headIDX)
		{
			int nextIDX = (thisIDX+1) % MAX_STORED_BROKEN_LIGHTS;	// AND here if can guarantee a power of 2
			lightingData.m_brokenLightHashes[thisIDX] = lightingData.m_brokenLightHashes[nextIDX];
			thisIDX = (thisIDX+1) % MAX_STORED_BROKEN_LIGHTS;
		}
		// We lost an element, set the head to be zero
		lightingData.m_brokenLightHashes[lightingData.m_BrokenLightHashesHeadIDX] = 0;
		lightingData.m_BrokenLightHashesHeadIDX--;	// And sub one off head
		if( lightingData.m_BrokenLightHashesHeadIDX < 0 )		
		{
			// And wrap if needed
			lightingData.m_BrokenLightHashesHeadIDX += MAX_STORED_BROKEN_LIGHTS;
		}
		lightingData.m_BrokenLightHashesCount--;
	}
}

#if GTA_REPLAY
bool Lights::IsLightBroken(u32 hash)
{
	return lightingData.m_brokenLightHashes.Find(hash) != -1;
}
#endif

// ----------------------------------------------------------------------------------------------- //

float Lights::CalculateAdjustedLightFade(
	const float lightFade, 
	const CLightEntity *lightEntity, 
	const CLightSource &lightSource, 
	const bool useParentAlpha)
{
	const bool isInInterior = lightSource.GetFlag(LIGHTFLAG_INTERIOR_ONLY);
	float fadeValue = 1.0f;

	if (lightEntity != NULL && lightEntity->GetParent() != NULL && 
		(isInInterior || useParentAlpha || lightSource.GetLightFadeDistance() > 0))
	{
		const float lightAlpha = CShaderLib::DivideBy255(lightEntity->GetParent()->GetAlpha());
		fadeValue = rage::Min<float>(lightFade, lightAlpha);

		CEntity* pLightParent = lightEntity->GetParent();
		
		if (pLightParent && pLightParent->GetIsInInterior())
		{
			const fwInteriorLocation interiorLocation = pLightParent->GetInteriorLocation();

			if (!interiorLocation.IsAttachedToPortal() && interiorLocation.GetRoomIndex() == 0)
			{
				s32 interiorLightFade = 0;
				CInteriorInst* pIntInst = CInteriorInst::GetInteriorForLocation(pLightParent->GetInteriorLocation());
				if (pIntInst)
				{
					if (pLightParent == pIntInst->GetInternalLOD())
					{
						interiorLightFade = 255 - pIntInst->GetAlpha();
					} else
					{
						interiorLightFade = pIntInst->GetAlpha();
					}
				}
				fadeValue = rage::Min<float>(lightFade, CShaderLib::DivideBy255(interiorLightFade));
			}
		}
	}

	return fadeValue;
}

// ----------------------------------------------------------------------------------------------- //

bool Lights::AddSceneLight(CLightSource& sceneLight, const CLightEntity *lightEntity, bool addToPreviousLightList)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));

	if( CRenderer::GetDisableArtificialLights() )
	{ 
		if(CRenderer::GetDisableArtificialVehLights()==false)
		{
			// EMP shut down, only let FX and VEHICLE lights through...
			if( !(sceneLight.GetFlag(LIGHTFLAG_FX) || sceneLight.GetFlag(LIGHTFLAG_VEHICLE)) )
				return false;

		}
		else
		{
			// EMP shut down, only let FX lights through...
			if( !sceneLight.GetFlag(LIGHTFLAG_FX) )
				return false;
		}
	}

	// don't add lights to the scene if they are very dim (very, very dim!)
	if (sceneLight.GetRadius() <= 0.001f ||
		(sceneLight.GetIntensity() < 0.001f && !(sceneLight.GetFlag(LIGHTFLAG_DRAW_VOLUME) || sceneLight.GetFlag(LIGHTFLAG_DELAY_RENDER))))
	{
		return false;
	}

	Assertf(sceneLight.GetType()!=LIGHT_TYPE_SPOT || sceneLight.GetConeOuterAngle()>0.0f, "Invalid Spotlight at (%f,%f,%f) ConeOuterAngle (%f)", sceneLight.GetPosition().x, sceneLight.GetPosition().y, sceneLight.GetPosition().z, sceneLight.GetConeOuterAngle());
	Assertf(sceneLight.GetType()!=LIGHT_TYPE_SPOT || sceneLight.GetConeBaseRadius()!=0.0f, "Spot light at (%f,%f,%f) GetConeBaseRadius() is 0.0", sceneLight.GetPosition().x, sceneLight.GetPosition().y, sceneLight.GetPosition().z);
	
	const bool bCutsceneRunning = (CutSceneManager::GetInstance()  && CutSceneManager::GetInstance()->IsCutscenePlayingBackAndNotCutToGame());
	if ((sceneLight.GetFlags() & LIGHTFLAG_DONT_USE_IN_CUTSCENE) && bCutsceneRunning)
	{
		return false;
	}

#if __BANK
	if (!DebugLighting::ShouldProcess(sceneLight))
	{
		return false;
	}
#endif

	const grcViewport* grcVP = gVpMan.GetCurrentGameGrcViewport();

	Vec4V fadeValues;
	CalculateFadeValues(&sceneLight, grcVP->GetCameraPosition(), fadeValues);
	const u32 shadowFlags = (LIGHTFLAG_CAST_DYNAMIC_GEOM_SHADOWS | LIGHTFLAG_CAST_STATIC_GEOM_SHADOWS | LIGHTFLAG_CAST_SHADOWS);

	if (fadeValues.GetXf() == 0.0f) { return false; }
	if (fadeValues.GetYf() == 0.0f) { sceneLight.ClearFlag(shadowFlags); sceneLight.SetShadowIndex(-1); }
	if (fadeValues.GetZf() == 0.0f && !sceneLight.GetFlag(LIGHTFLAG_USE_VEHICLE_TWIN)) { sceneLight.SetFlag(LIGHTFLAG_NO_SPECULAR); }
	if (fadeValues.GetWf() == 0.0f) { sceneLight.ClearFlag(LIGHTFLAG_DRAW_VOLUME); }
	
	// Apply volume and light fade
	const float finalFadeValue = CalculateAdjustedLightFade(fadeValues.GetXf(), lightEntity, sceneLight, false);
	sceneLight.SetIntensity(sceneLight.GetIntensity() * finalFadeValue);

	const float lightAlpha = (lightEntity && lightEntity->GetParent() && (sceneLight.GetFlag(LIGHTFLAG_INTERIOR_ONLY) || sceneLight.GetLightFadeDistance() > 0)) ? CShaderLib::DivideBy255(lightEntity->GetParent()->GetAlpha()) : 1.0f;
	const float finalVolumeFade = rage::Min<float>(fadeValues.GetWf(), lightAlpha);
	sceneLight.SetVolumeIntensity(sceneLight.GetVolumeIntensity() * finalVolumeFade);

	// Get relevant cull radius
	const float cullRadius = sceneLight.GetBoundingSphereRadius();
	Vector4 farclip=VEC4V_TO_VECTOR4(grcVP->GetFrustumClipPlane(grcViewport::CLIP_PLANE_FAR));
	float dist = (-farclip.Dot3(sceneLight.GetPosition())) - farclip.w;

#if __DEV
	if ((sceneLight.GetFlags() & LIGHTFLAG_CUTSCENE) && (dist>=(-cullRadius)))
	{
		Warningf("a cutscene light (radius:%f) crosses the far clip plane (at:%f) and has been culled", cullRadius, farclip.w);
	}
#endif

	bool isCulled = (dist >= (-cullRadius));

	// Do an extra AABB test for lights that don't didn't get added by an entity (the entity went through that test during the scan)
	if(!isCulled && ((lightEntity == NULL BANK_ONLY( && DebugLights::m_EnableLightAABBTest )) || addToPreviousLightList))
	{
		if(sceneLight.GetType() == LIGHT_TYPE_SPOT || sceneLight.GetType() == LIGHT_TYPE_CAPSULE )
		{
			spdAABB bound;
			Lights::GetBound(sceneLight, bound);
			isCulled = !grcVP->IsAABBVisible(bound.GetMin().GetIntrin128(), bound.GetMax().GetIntrin128(), grcVP->GetCullFrustumLRTB());
		}

		if (sceneLight.GetType() == LIGHT_TYPE_AO_VOLUME || sceneLight.GetType() == LIGHT_TYPE_POINT)
		{
			isCulled = (grcVP->IsSphereVisible(sceneLight.GetPosition().x, sceneLight.GetPosition().y, sceneLight.GetPosition().z, cullRadius) == cullOutside);
		}

		if (!isCulled &&
			(sceneLight.GetFlags() & LIGHTFLAG_ALREADY_TESTED_FOR_OCCLUSION) == 0 &&
			Likely(g_scanMutableDebugFlags & SCAN_MUTABLE_DEBUG_ENABLE_OCCLUSION) && 
			!sceneLight.InInterior() &&
			sceneLight.GetType() != LIGHT_TYPE_AO_VOLUME &&
			!addToPreviousLightList)  // can't test against occluders when adding light to previous frame list for shadow update
		{
			isCulled = !IsLightVisible(sceneLight);			
		}
	}

	if (sceneLight.GetFlags() & (LIGHTFLAG_CUTSCENE | LIGHTFLAG_FX))
	{
		isCulled = false;
	}

#if __BANK
	const bool bIsLightBudgetActive = SBudgetDisplay::GetInstance().IsActive();
	if (bIsLightBudgetActive)
	{
		SBudgetDisplay::GetInstance().ColouriseLight(&sceneLight);
	}
#endif

	if (isCulled == false)
	{
		/*if (!gVpMan.GetRenderSettings().QueryFlags(RENDER_SETTING_RENDER_SHADOWS))
		{
			sceneLight.ClearFlag((LIGHTFLAG_CAST_STATIC_GEOM_SHADOWS | LIGHTFLAG_CAST_DYNAMIC_GEOM_SHADOWS));
		}*/
		
		const float t_sortVal = rage::Max((sceneLight.GetPosition() - VEC3V_TO_VECTOR3(grcVP->GetCameraPosition())).Mag() - cullRadius, 0.0f);

		#if __BANK
		if (DebugLights::m_freeze)
		{
			for (int l = 0; l < m_numSceneLights; l++)
			{
				if (sceneLight.GetLightEntity() != NULL && m_sceneLights[l].GetLightEntity() == sceneLight.GetLightEntity())
				{
					m_sceneLights[l] = sceneLight;
				}
			}
		}
		else
		#endif //__BANK
		{
			s32 txdId = sceneLight.GetTextureDict();
			// detect & fix invalid txdId: tentative fix for B*3230574 - Crash - Local player Crashed after getting kicked from "Rancho Clubhouse":
			if((txdId <= 0) || (txdId >= g_TxdStore.GetSize()))
			{
				txdId = -1;
				sceneLight.SetTextureDict(-1);
			}

			if (txdId != -1)
			{
				gDrawListMgr->AddTxdReference(txdId);
			}

			if (addToPreviousLightList)  // special for the shadows, update the old list with new (after cutscene camera cut) lights
			{
				if (m_numPrevSceneShadowLights<MAX_STORED_LIGHTS)
					m_prevSceneShadowLights[m_numPrevSceneShadowLights++] = sceneLight;
			}
			else
			{
				AddSceneLightOrdered(sceneLight, t_sortVal, lightEntity);
			}
		}
		
		m_numSceneLightsBuf[m_currentLightBuffer] = m_numSceneLights;
		
		return true;
	}

	return false;
}

// ----------------------------------------------------------------------------------------------- //

void Lights::BeginLightweightTechniques()
{
	ASSERT_ONLY(s_CheckLightWeightTechniquesDepth = true;)

	if( s_LightWeightTechniquesDepth == 0 )
	{
		Assert(s_PreviousTechniqueGroupId == -1);
		s_PreviousTechniqueGroupId = grmShaderFx::GetForcedTechniqueGroupId();
		s_UseLightweightTechniques = !PARAM_nolwlights.Get();
	}
	s_LightWeightTechniquesDepth++;
}

// ----------------------------------------------------------------------------------------------- //

void Lights::SelectLightweightTechniques(s32 activeCount, bool bUseHighQuality)
{
	Assert(s_CheckLightWeightTechniquesDepth == false || s_LightWeightTechniquesDepth > 0);

	if (s_UseLightweightTechniques)
	{
		int passIdx = s_LightweightTechniquePassType;
		int activeIdx = (activeCount == 0) ? kLightsActive0 : ((activeCount <=4) ? kLightsActive4 : kLightsActive8);
		int techniqueGroupId = s_PreviousTechniqueGroupId;

		if (bUseHighQuality BANK_ONLY( || DebugLighting::m_bForceHighQualityForward))
		{
			techniqueGroupId = s_LightweightHighQualityTechniqueGroupId[passIdx][activeIdx];
		}
		else
		{
			techniqueGroupId = s_LightweightTechniqueGroupId[passIdx][activeIdx];

		}
		grmShaderFx::SetForcedTechniqueGroupId(techniqueGroupId);
	}
}


void Lights::SetLightweightTechniquePassType(LightweightTechniquePassType passType)
{
	Assert(passType < kLightPassMax);
	s_LightweightTechniquePassType = passType;
}

Lights::LightweightTechniquePassType Lights::GetLightweightTechniquePassType() 
{ 
	return s_LightweightTechniquePassType; 
}


// ----------------------------------------------------------------------------------------------- //

void Lights::EndLightweightTechniques()
{
	ASSERT_ONLY(s_CheckLightWeightTechniquesDepth = false;)

	s_LightWeightTechniquesDepth--;
	Assert(s_LightWeightTechniquesDepth >= 0);
	
	if( s_LightWeightTechniquesDepth == 0 )
	{
		grmShaderFx::SetForcedTechniqueGroupId(s_PreviousTechniqueGroupId);
		s_PreviousTechniqueGroupId = -1;
		s_UseLightweightTechniques = false;
	}
}

// ----------------------------------------------------------------------------------------------- //

static float CalcFallOffByDistance(float dist, float maxDist, float exponent)
{
	// Similar code to distanceFalloff in lighting_common.fxh
	const float bsse = Clamp(1.0f - (dist * dist) / (maxDist * maxDist), 0.0f, 1.0f);
	return PowApprox(bsse, exponent); // This is the same approximation we use in the shader (see __powapprox)
}

// ----------------------------------------------------------------------------------------------- //

static float CalcFallOff(const CLightSource &light, const Vector3& boxPos, const Vector3& boxRange)
{
	const Vector3 diff = (boxPos-light.GetPosition());
	float dist = diff.Mag();
	float boxRadius = boxRange.Mag();
	if (dist < (light.GetRadius() + boxRadius))
	{
		float brightness = LumFactors.Dot(light.GetColor()) * light.GetIntensity();
		float falloff = CalcFallOffByDistance(dist, light.GetRadius() + boxRadius, light.GetFalloffExponent());
		float angfalloff = 1.0f;

		if (dist > 0.0f && falloff > 0.0f && light.GetType() == LIGHT_TYPE_SPOT && light.GetConeCosOuterAngle() > 0.01f ) 
		{
			CCone K( rage::Clamp(light.GetConeCosOuterAngle(), 0.0f, 1.0f), light.GetPosition(), light.GetDirection() );
			angfalloff = (K.Intersects(boxPos, boxRadius))  ? 1.0f : 0.0f;
		}

		return falloff*angfalloff*brightness;
	}

	return 0.0f;
}

// ----------------------------------------------------------------------------------------------- //

static __forceinline void CalculatePartialModelBound(const grmShaderGroup &group, const grmModel& model, int bucketMask, spdAABB& aabb, int& amt) 
{
	if (!model.GetAABBs())
		return;

	for (int i = 0; i < model.GetGeometryCount(); i++) 
	{
		const int shaderBucketMask = group.GetShader(model.GetShaderIndex(i)).GetDrawBucketMask();

		if (BUCKETMASK_MATCH(shaderBucketMask,bucketMask))
		{
			aabb.GrowAABB(model.GetGeometryAABB(i));
			amt++;
		}
	}
}

// ----------------------------------------------------------------------------------------------- //

int Lights::CalculatePartialDrawableBound( spdAABB& box, const rmcDrawable* pDrawable, int bucketMask,int lod )
{
	Assert( lod <  8);

	int		amt = 0;
	spdAABB bbox;
	bbox.Invalidate();

	if ( pDrawable && pDrawable->GetLodGroup().ContainsLod( lod) )
	{
		const rmcLod& lodModels = pDrawable->GetLodGroup().GetLod( lod);
		const grmShaderGroup& shadGroup = pDrawable->GetShaderGroup();
		for ( int i = 0; i < lodModels.GetCount(); i++ )
		{
			const grmModel* model = lodModels.GetModel(i);
			CalculatePartialModelBound(shadGroup, *model, bucketMask, bbox, amt);
		}
	}
	if ( amt )
	{
		box =  bbox;
	}
	return amt;
}

// ----------------------------------------------------------------------------------------------- //

spdAABB Lights::CalculatePartialDrawableBound( const CEntity& entity, int bucketMask,int lod )
{
	spdAABB box;
	int amt = CalculatePartialDrawableBound(box, entity.GetDrawable(), bucketMask, lod);
	if ( amt )
	{
		box.Transform(entity.GetMatrix());
	}
	else
	{
		box = entity.GetBoundBox(box);
	}
	return box;
}

// ----------------------------------------------------------------------------------------------- //

void Lights::SetupRenderThreadLights()
{
	Assert(!gDrawListMgr->IsExecutingDrawList());
	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));		

	bool cameraCut = camInterface::GetFrame().GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition | camFrame::Flag_HasCutOrientation) ||
		(CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->IsRunning() && CutSceneManager::GetInstance()->GetHasCutThisFrame());

	DLC_Add(SetRenderLights, m_currentLightBuffer, m_currentLightShaftBuffer, cameraCut);
}

// ----------------------------------------------------------------------------------------------- //

void Lights::SetupDirectionalGlobals(float directionalScale)
{
	m_lightingGroup.SetDirectionalLightingGlobals(directionalScale); 
}

// ----------------------------------------------------------------------------------------------- //

void Lights::SetupDirectionalAndAmbientGlobals()
{
	m_lightingGroup.SetDirectionalLightingGlobals(); 
	m_lightingGroup.SetAmbientLightingGlobals(); 
}

// ----------------------------------------------------------------------------------------------- //

void Lights::SetupDirectionalAndAmbientGlobalsTweak(float directionalScale, float artificialIntAmbientScale, float artificialExtAmbientScale)
{
	m_lightingGroup.SetDirectionalLightingGlobals(directionalScale); 
	m_lightingGroup.SetAmbientLightingGlobals(1.0f, artificialIntAmbientScale, artificialExtAmbientScale); 
}

// ----------------------------------------------------------------------------------------------- //

void Lights::UseLightsInArea(const spdAABB& box, const bool inInterior, const bool forceSetAll8Lights, const bool reflectionPhase, const bool respectHighPriority)
{
	PF_START(GetLightsInArea);

	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));		

	m_lightingGroup.Reset(); // Reset previous light group
	
	bool bHasCapsule = false;
	BANK_ONLY(int totalPotentialLights = 0); // The total amount of lights that can potentially affect this entity
	if(g_FastLights.GetLightCount() > 0)
	{
		// choose best lights
		float maxfalloff[MAX_RENDERED_LIGHTS];
		int maxindex[MAX_RENDERED_LIGHTS];

		for (int n=0;n<(MAX_RENDERED_LIGHTS);n++)
		{
			maxfalloff[n] = 0.0f;
			maxindex[n] = -1;
		}
	
		int	lightIndices[MAX_STORED_LIGHTS];
		int amtLights = g_FastLights.GetIndices( lightIndices , box);

		CLightSource* lights = g_FastLights.GetLightList();

		// Cache these values so we don't have to calculate them for every light
		const Vector3 boxPos = VEC3V_TO_VECTOR3(box.GetCenter());
		const Vector3 boxRange = VEC3V_TO_VECTOR3(box.GetExtent());
		BANK_ONLY(totalPotentialLights = amtLights); // Start with the amount of lights found by the bound search
		for (int j = 0; j < amtLights; j++)
		{
			const int idx = lightIndices[j];

			const CLightSource &light = lights[idx];
		
			const bool invalidLight = (light.GetFlag(LIGHTFLAG_INTERIOR_ONLY) && (inInterior == false)) ||
									  (light.GetFlag(LIGHTFLAG_EXTERIOR_ONLY) && (inInterior == true)) ||
									  (light.GetFlag(LIGHTFLAG_NOT_IN_REFLECTION) && reflectionPhase) || 
									  (light.GetFlag(LIGHTFLAG_ONLY_IN_REFLECTION) && !reflectionPhase);

			if(!invalidLight)
			{
				float falloff = CalcFallOff(light, boxPos, boxRange);

				if (respectHighPriority && light.GetExtraFlag(EXTRA_LIGHTFLAG_HIGH_PRIORITY))
				{
					const float highPriorityBoost = 100000.0f;
					falloff += highPriorityBoost;
				}

				// perform portal culling
				if (falloff > 0.0f)
				{
					int index = idx;
					if (falloff > maxfalloff[MAX_RENDERED_LIGHTS - 1])
					{
						for (int n = 0; n < (MAX_RENDERED_LIGHTS); n++)
						{
							if (falloff > maxfalloff[n])
							{
								std::swap(falloff, maxfalloff[n]);
								std::swap(index, maxindex[n]);
							}
						}
					}
				}
				else
				{
					BANK_ONLY(totalPotentialLights--); // Skip lights that won't affect the entity
				}
			}
			else
			{
				BANK_ONLY(totalPotentialLights--); // Skip lights that are invalid for the search
			}
		}

		// Populate light group
		for (int n = 0; n < MAX_RENDERED_LIGHTS; n++)
		{
			if (maxindex[n] != -1)
			{
				m_lightingGroup.SetLight(n, &lights[maxindex[n]]);
				if (lights[maxindex[n]].GetType() == LIGHT_TYPE_CAPSULE)
				{
					bHasCapsule = true;
				}
			}
		}
#if __BANK
		if (reflectionPhase ? DebugLights::m_showForwardReflectionDebugging : DebugLights::m_showForwardDebugging)
		{
			DebugLights::ShowLightLinks(box, lights, maxindex, totalPotentialLights);
		}
#endif
	}

#if __BANK
	if (reflectionPhase ? DebugLights::m_showForwardReflectionDebugging : DebugLights::m_showForwardDebugging)
	{
		grcDebugDraw::BoxAxisAligned(box.GetMin(), box.GetMax(), totalPotentialLights > 8 ? Color32(255, 0, 0, 255) : Color32(255, 255, 255, 255), false);
	}
#endif

	SelectLightweightTechniques(m_lightingGroup.GetActiveCount(), m_useHighQualityTechniques);
	m_lightingGroup.SetLocalLightingGlobals(false, forceSetAll8Lights);

	if( s_ResetLightingOverrideConstant )
	{
		CShaderLib::SetGlobalToneMapScalers(Vector4(s_prevGlobalToneMapScalers.x, s_prevGlobalToneMapScalers.y, s_prevGlobalToneMapScalers.z, s_prevGlobalToneMapScalers.w));

		s_ResetLightingOverrideConstant = false;
	}

	PF_STOP(GetLightsInArea);
}

// ----------------------------------------------------------------------------------------------- //

void Lights::SetHighQualityTechniques(bool bUseHighQuality)
{
	m_useHighQualityTechniques = bUseHighQuality;
}

void Lights::SetUseSceneShadows(u32 sceneShadows)
{
	m_sceneShadows = sceneShadows;
}

// ----------------------------------------------------------------------------------------------- //

u32 Lights::GetNumSceneLights()
{
	return m_numSceneLightsBuf[m_currentLightBuffer];
}

// ----------------------------------------------------------------------------------------------- //

void Lights::SetupRenderthreadFrameInfo()
{
	DLC(dlCmdSetupLightsFrameInfo, ());
}

// ----------------------------------------------------------------------------------------------- //

void Lights::ClearRenderthreadFrameInfo()
{
	DLC(dlCmdClearLightsFrameInfo, ());
}

// ----------------------------------------------------------------------------------------------- //

float Lights::CalculateTimeFade(const u32 timeFlags)
{
	const u32 hour = CClock::GetHour();

	const u32 nextHour = (CClock::GetHour() + 1) % 24;

	// Light is off now but will turn on in the next hour
	if ((timeFlags & (1 << (nextHour))) != 0 &&
		(timeFlags & (1 << hour)) == 0)
	{
		const float fade = (float)CClock::GetMinute() + ((float)CClock::GetSecond() / 60.0f);
		return Saturate(fade - 59.0f);
	}

	// Light is on but will switch off in the next hour
	if ((timeFlags & (1 << hour)) != 0 &&
		(timeFlags & (1 << (nextHour))) == 0)
	{
		const float fade = (float)CClock::GetMinute() + ((float)CClock::GetSecond() / 60.0f);
		return(1.0f - Saturate(fade - 59.0f));
	}

	// If light is on, make sure its on
	if (timeFlags & (1 << hour))
	{
		return 1.0;
	}

	return 0.0f;
}

// ----------------------------------------------------------------------------------------------- //
#if DEPTH_BOUNDS_SUPPORT
// ----------------------------------------------------------------------------------------------- //

bool Lights::SetupDepthBounds(const CLightSource *lightSource)
{
	// set depth bounds test
	if(BANK_SWITCH(DebugLighting::m_useLightsDepthBounds, true))
	{
		grcDevice::SetDepthBoundsTestEnable(true);
		return SetDepthBoundsFromLightVolume( lightSource );
	}

	return true;
}

// ----------------------------------------------------------------------------------------------- //

void Lights::DisableDepthBounds()
{
	if(BANK_SWITCH(DebugLighting::m_useLightsDepthBounds, true))
	{
		grcDevice::SetDepthBoundsTestEnable(false);
	}
}
#endif //DEPTH_BOUNDS_SUPPORT

// ----------------------------------------------------------------------------------------------- //

float Lights::GetReflectorIntensity(const Vector3 &lightDir)
{
	Vector3 directionalLightDir = RCC_VECTOR3(g_timeCycle.GetDirectionalLightDirection());
	return Saturate(directionalLightDir.Dot(lightDir));
}

// ----------------------------------------------------------------------------------------------- //

float Lights::GetDiffuserIntensity(const Vector3 &lightDir)
{
	Vector3 directionalLightDir = RCC_VECTOR3(g_timeCycle.GetDirectionalLightDirection());
	return Saturate(directionalLightDir.Dot(lightDir) * -1.0f);
}


// ----------------------------------------------------------------------------------------------- //

void Lights::CalculateFromAmbient(CLightSource &light)
{
	float intensity = light.GetIntensity();
	Vector3 colour = light.GetColor();

	const tcKeyframeUncompressed& currKeyframe = g_timeCycle.GetCurrUpdateKeyframe();

	Vector3 ambientColour = Vector3(0.0f, 0.0f, 0.0f);
	float ambientIntensity = 1.0f;

	if (light.InInterior())
	{
		ambientColour = Vector3(
			currKeyframe.GetVar(TCVAR_LIGHT_ARTIFICIAL_INT_AMB_DOWN_COL_R),
			currKeyframe.GetVar(TCVAR_LIGHT_ARTIFICIAL_INT_AMB_DOWN_COL_G),
			currKeyframe.GetVar(TCVAR_LIGHT_ARTIFICIAL_INT_AMB_DOWN_COL_B));
		ambientIntensity = currKeyframe.GetVar(TCVAR_LIGHT_ARTIFICIAL_INT_AMB_DOWN_INTENSITY);
	}	
	else
	{
		ambientColour = Vector3(
			currKeyframe.GetVar(TCVAR_LIGHT_NATURAL_AMB_DOWN_COL_R),
			currKeyframe.GetVar(TCVAR_LIGHT_NATURAL_AMB_DOWN_COL_G),
			currKeyframe.GetVar(TCVAR_LIGHT_NATURAL_AMB_DOWN_COL_B));
		ambientIntensity = currKeyframe.GetVar(TCVAR_LIGHT_NATURAL_AMB_DOWN_INTENSITY);
	}

	light.SetColor(colour * ambientColour);
	light.SetIntensity(intensity * ambientIntensity);
}

// ----------------------------------------------------------------------------------------------- //

void Lights::SetMapLightFadeDistance(float mix)
{ 
	m_mapLightFadeDistance = Lerp(Saturate(mix), 50.0f, g_visualSettings.Get("lights.cutoff.map.distance", 1000.0f)); 
}

// ----------------------------------------------------------------------------------------------- //

void Lights::CalculateFadeValues(const CLightSource *light, Vec3V_In cameraPosition, Vec4V_InOut out_fadeValues)
{
	const Vec3V lightPosition = VECTOR3_TO_VEC3V(light->GetPosition());
	const ScalarV distToLight = Mag(Subtract(lightPosition, cameraPosition));

	//converting from u8 to float. Convert frmo 0-255 to 0.0f-1.0f using the table
	//and then multiplying by 255 to rescale it to 0.0f-255.0f to avoid LHS
	Vec4V lightCutoffDistances = Vec4V(
		CShaderLib::DivideBy255(light->GetLightFadeDistance()),
		CShaderLib::DivideBy255(light->GetShadowFadeDistance()),
		CShaderLib::DivideBy255(light->GetSpecularFadeDistance()),
		CShaderLib::DivideBy255(light->GetVolumetricFadeDistance()));

	if( light->GetExtraFlag(EXTRA_LIGHTFLAG_USE_FADEDIST_AS_FADEVAL) )
	{
		out_fadeValues = lightCutoffDistances;
	}
	else
	{
		lightCutoffDistances *= ScalarV(255.0f);
		const Vec4V distToLightV = Vec4V(distToLight);

		// Setup map light fade distance for min-spec PC
		Vec4V defaultDistances = m_defaultFadeDistances;
		if (light->GetLightEntity() && 
			!light->InInterior() && 
			!light->GetFlag(LIGHTFLAG_CUTSCENE | LIGHTFLAG_MOVING_LIGHT_SOURCE | LIGHTFLAG_VEHICLE | LIGHTFLAG_FX) && 
			light->GetLightFadeDistance() == 0)
		{
			defaultDistances.SetXf(m_mapLightFadeDistance);
		}

		const VecBoolV selectMask = IsGreaterThan(lightCutoffDistances, Vec4V(V_ZERO));
		Vec4V currentDistances = SelectFT(selectMask, defaultDistances, lightCutoffDistances);

		// Light fade multipliers
		const Vec4V lightFadeMultipliers = CSystem::IsThisThreadId(SYS_THREAD_UPDATE) ? 
			GetUpdateLightFadeMultipliers() : GetRenderLightFadeMultipliers();

		if (CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->IsCutscenePlayingBackAndNotCutToGame() &&
			!light->GetFlag(LIGHTFLAG_CUTSCENE))
		{
			currentDistances = Scale(currentDistances, lightFadeMultipliers);
		}
		else if (!light->InInterior())
		{
			currentDistances = Scale(currentDistances, lightFadeMultipliers);
		}

		out_fadeValues = Saturate(InvScale(Subtract(currentDistances, distToLightV), Vec4V(V_TEN)));
		
		if (!light->InInterior() && 
			!light->GetFlag(LIGHTFLAG_CUTSCENE | LIGHTFLAG_MOVING_LIGHT_SOURCE | LIGHTFLAG_VEHICLE | LIGHTFLAG_FX) && 
			light->GetShadowFadeDistance() == 0)
		{
			const float fadeValue = 1.0f - Saturate(((1.0f - light->GetParentAlpha()) - 0.25f) / 0.5f);
			out_fadeValues = Scale(out_fadeValues, Vec4V(1.0f, fadeValue, fadeValue, 1.0f));
		}
	}
}

// ----------------------------------------------------------------------------------------------- //
// All times are in hours [0..24]
// ----------------------------------------------------------------------------------------------- //

float Lights::CalculateTimeFade(float fadeInTimeStart, float fadeInTimeEnd, float fadeOutTimeStart, float fadeOutTimeEnd)
{
	const float t0 = fadeInTimeStart;
	const float t = fmodf((float)CClock::GetHour() + (float)CClock::GetMinute()/60.0f + (float)CClock::GetSecond()/(60.f*60.0f) - t0 + 24.0f, 24.0f);

	// shift time relative to fadeInTimeStart
	fadeInTimeStart  = 0.0f;
	fadeInTimeEnd    = fmodf(fadeInTimeEnd    - t0 + 24.0f, 24.0f);
	fadeOutTimeStart = fmodf(fadeOutTimeStart - t0 + 24.0f, 24.0f);
	fadeOutTimeEnd   = fmodf(fadeOutTimeEnd   - t0 + 24.0f, 24.0f);

	if (t <= fadeInTimeStart || t >= fadeOutTimeEnd)
	{
		return 0.0f;
	}
	else if (t >= fadeInTimeEnd && t <= fadeOutTimeStart)
	{
		return 1.0f;
	}
	else if (t <= fadeInTimeEnd)
	{
		return (t - fadeInTimeStart)/(fadeInTimeEnd - fadeInTimeStart); // fading in
	}
	else
	{
		return (t - fadeOutTimeEnd)/(fadeOutTimeStart - fadeOutTimeEnd); // fading out
	}
}

// ----------------------------------------------------------------------------------------------- //

void Lights::CalculateDynamicBakeBoostAndWetness()
{
	Vec4V dynamicBakeWetnessParams = Vec4V(0.3f, 1.0f, 0.0f, 0.0f);
	const tcKeyframeUncompressed& currKeyframe = g_timeCycle.GetCurrRenderKeyframe();

	const float dynamicBakeTweak = currKeyframe.GetVar(TCVAR_LIGHT_DYNAMIC_BAKE_TWEAK);
	float mixValue  = Saturate((dynamicBakeTweak - 2.0f) * -0.25f);

	const float dynamicBakeMin = Clamp(Lerp(mixValue, m_startDynamicBakeMin, m_endDynamicBakeMin), 0.0f, 2.0f);
	const float dynamicBakeMax = Clamp(Lerp(mixValue, m_startDynamicBakeMax, m_endDynamicBakeMax), 0.0f, 2.0f);
	const float wetness = rage::Clamp(g_weather.GetWetness(), 0.0f, 1.0f);

	dynamicBakeWetnessParams = Vec4V(dynamicBakeMin, dynamicBakeMax - dynamicBakeMin, wetness, 0.0f);

	BANK_ONLY(DebugDeferred::OverrideDynamicBakeValues(dynamicBakeWetnessParams);)
	CShaderLib::SetGlobalDynamicBakeBoostAndWetness(dynamicBakeWetnessParams);
}

// ----------------------------------------------------------------------------------------------- //

void Lights::UpdateVisualDataSettings()
{
	if (g_visualSettings.GetIsLoaded())
	{
		m_lightFadeDistance = g_visualSettings.Get("lights.cutoff.distance", 1000.0f);
		m_shadowFadeDistance = g_visualSettings.Get("lights.cutoff.shadow.distance", 1000.0f);
		m_specularFadeDistance = g_visualSettings.Get("lights.cutoff.specular.distance", 1000.0f);
		m_volumetricFadeDistance = g_visualSettings.Get("lights.cutoff.volumetric.distance", 1000.0f);

		m_defaultFadeDistances = Vec4V(
			m_lightFadeDistance,
			m_shadowFadeDistance,
			m_specularFadeDistance,
			m_volumetricFadeDistance
			);
		m_lightCullDistanceForNonGBufLights = g_visualSettings.Get("lights.cutoff.nonGbuf.distance", 150.0f);

		m_startDynamicBakeMin = g_visualSettings.Get("dynamic.bake.start.min", 0.7f);
		m_startDynamicBakeMax = g_visualSettings.Get("dynamic.bake.start.max", 1.7f);

		m_endDynamicBakeMin = g_visualSettings.Get("dynamic.bake.end.min", 0.3f);
		m_endDynamicBakeMax = g_visualSettings.Get("dynamic.bake.end.max", 1.3f);

		m_pedLightDirection = Vector3(
			g_visualSettings.Get("ped.light.direction.x", -0.825f),
			g_visualSettings.Get("ped.light.direction.y",  0.565f),
			g_visualSettings.Get("ped.light.direction.z",  0.000f));
		m_pedLightDirection.Normalize();

		m_pedLightDayFadeStart = g_visualSettings.Get("ped.light.fade.day.start", 6.0f);
		m_pedLightDayFadeEnd = g_visualSettings.Get("ped.light.fade.day.end", 7.0f);

		m_pedLightNightFadeStart = g_visualSettings.Get("ped.light.fade.night.start", 20.0f);
		m_pedLightNightFadeEnd = g_visualSettings.Get("ped.light.fade.night.end", 21.0f);
	}
}

// ----------------------------------------------------------------------------------------------- //

int	Lights::CalculateLightCategory(u8 lightType, float falloff, float intensity, float capsuleExtentX, u32 lightAttrFlags)
{
	// Large
	// LIGHTFLAG_FAR_LOD_LIGHT: This now marks the lights to go in the large category. Anything else goes into medium.
	if( lightAttrFlags & LIGHTFLAG_FAR_LOD_LIGHT )
	{
		return LIGHT_CATEGORY_LARGE;
	}

	// Calc length for categorization.
	float length = falloff;	// Spot/Point
	if( lightType == LIGHT_TYPE_CAPSULE )
	{
		// Capsule
		length = (2.0f*falloff + capsuleExtentX);
	}

	// Medium
	if( lightAttrFlags & LIGHTFLAG_FORCE_MEDIUM_LOD_LIGHT || (length >= 10.0f && intensity >= 1.0f) )
	{
		return LIGHT_CATEGORY_MEDIUM;
	}

	// Small
	return LIGHT_CATEGORY_SMALL;
}

// ----------------------------------------------------------------------------------------------- //

#if __BANK
void Lights::AddWidgets(bkBank &bank)
{
	bank.PushGroup("Pickup Ambient");
	bank.AddSlider("Light Override",	&Lights::m_lightOverrideMaxIntensityScale, 0.f, 10.f, 0.1f);
	bank.PopGroup();
	bank.PushGroup("SSS");
		bank.AddToggle("Enable",		&Lights::m_bUseSSS);
		bank.AddSlider("Normal shift",	&Lights::m_SSSParams.x, -10.f, 10.f, 0.01f);
		bank.AddSlider("Distance scale",&Lights::m_SSSParams.y, -1000.f, 1000.f, 0.1f);
		bank.AddSlider("Over-scatter",	&Lights::m_SSSParams.z, 0.f, 10.f, 0.01f);
		bank.AddSlider("Ambient",		&Lights::m_SSSParams.w, 0.f, 100.f, 0.01f);
		bank.AddSlider("Shadow Offset", &Lights::m_SSSParams2.x, -100.f, 100.f, 0.1f);
	bank.PopGroup();
}
#endif	//__BANK

// ----------------------------------------------------------------------------------------------- //

void Lights::StartPedLightFadeUp(const float seconds)
{
	if (m_fadeUpPedLight != true)
	{
		m_fadeUpPedLight = true;
		m_fadeUpStartTime = fwTimer::GetSystemTimeInMilliseconds();
		m_fadeUpDuration = seconds * 1000.0f;
	}
	else
	{
		renderWarningf("Tried to start a ped light fade up with one already active");
	}
}

void Lights::ResetPedLight()
{
	s_exposureAvgIndex = 0;		 
	s_exposureAvgArray[0] = s_exposureAvgArray[1] = s_exposureAvgArray[2] = s_exposureAvgArray[3] = 0.0f;
	s_exposureAvgSize = 0;		
	s_pow2NegExposure = 0.0f;
	s_pedLightReset = 4;
}

// ----------------------------------------------------------------------------------------------- //

void Lights::RenderPedLight()
{
	#if __BANK
		if (!DebugDeferred::m_enablePedLight)
		{
			return;
		}
	#endif

	LightingData &lightData = Lights::GetRenderLightingData();

	Vector3 pedLightColour = lightData.m_pedLightColour;
	float pedLightIntensity = lightData.m_pedLightIntensity;
	Vector3 pedLightDirection = lightData.m_pedLightDirection;

	if (pedLightIntensity <= 1e-6f || pedLightColour.Dot(Vector3(1.0f, 1.0f, 1.0f)) <= 1e-6f)
	{
		return;
	}

	PF_PUSH_MARKER("Ped light");

	CLightSource light;
	
	light.SetCommon(
		LIGHT_TYPE_DIRECTIONAL, 
		LIGHTFLAG_BOTH_INTERIOR_AND_EXTERIOR, 
		Vector3(0.0f, 0.0f, 0.0f), 
		pedLightColour,
		pedLightIntensity,
		LIGHT_ALWAYS_ON);

	Vector3 pedLightTangent = FindMinAbsAxis(pedLightDirection);
	light.SetDirTangent(pedLightDirection, pedLightTangent);

	grmShader* currentShader = DeferredLighting::GetShader(DEFERRED_SHADER_DIRECTIONAL);
	s32 pass = DIRECTIONALPASS_JUSTDIR;
	grcEffectTechnique tech = DeferredLighting::GetTechnique(light.GetType());

	DeferredLighting::SetShaderGBufferTargets();
	DeferredLighting::SetLightShaderParams(DEFERRED_SHADER_DIRECTIONAL, &light, NULL, 1.0f, false, false, MM_DEFAULT);
	DeferredLighting::SetProjectionShaderParams(DEFERRED_SHADER_DIRECTIONAL, MM_DEFAULT);
	CRenderPhaseCascadeShadowsInterface::SetSharedShaderVars();	

	grcStateBlock::SetBlendState(Lights::m_SceneLights_B);
	grcStateBlock::SetRasterizerState(grcStateBlock::RS_NoBackfaceCull);
	grcStateBlock::SetDepthStencilState(m_pedLight_DS, DEFERRED_MATERIAL_PED);

#if RSG_PC
	if(GRCDEVICE.GetMSAA()>1)
	{
		grcRenderTarget *pDepthTex = CRenderTargets::LockReadOnlyDepth(true, true, CRenderTargets::GetDepthBuffer(), CRenderTargets::GetDepthBufferCopy(), CRenderTargets::GetDepthBuffer_ReadOnly());

		if ((GRCDEVICE.GetDxFeatureLevel() < 1100) && (!GRCDEVICE.IsReadOnlyDepthAllowed()))
		{
			// if DX10/10.1
			DeferredLighting::SetShaderGBufferTarget(GBUFFER_DEPTH, pDepthTex);
		}
	}
	if(GRCDEVICE.GetDxFeatureLevel() <= 1000)
	{
		DeferredLighting::SetShaderGBufferTarget(GBUFFER_DEPTH, CRenderTargets::GetDepthBufferCopy());
	}
#endif

	if (ShaderUtil::StartShader("Directional Light", currentShader, tech, pass))
	{
		DeferredLighting::Render2DDeferredLightingRect(0.0f, 0.0f, 1.0f, 1.0f);
		ShaderUtil::EndShader(currentShader);	
	}
#if RSG_PC
		if(GRCDEVICE.GetMSAA()>1)
		{
			CRenderTargets::UnlockReadOnlyDepth();
		}
		if(GRCDEVICE.GetDxFeatureLevel() <= 1000)
		{
			DeferredLighting::SetShaderGBufferTarget(GBUFFER_DEPTH, CRenderTargets::GetDepthBuffer());
		}
#endif
	PF_POP_MARKER();
}

// ----------------------------------------------------------------------------------------------- //

void Lights::ApplyLightOveride(const CEntity *entity)
{
	DLC(dlCmdLightOverride, (entity, false));
}


void Lights::UnApplyLightOveride(const CEntity *entity)
{
	DLC(dlCmdLightOverride, (entity, true));
}

void Lights::SetLightOverrideMaxIntensityScale(float maxIntensityScale)
{
	m_lightOverrideMaxIntensityScale = maxIntensityScale;
}

float Lights::GetLightOverrideMaxIntensityScale()
{
	return m_lightOverrideMaxIntensityScale;
}
