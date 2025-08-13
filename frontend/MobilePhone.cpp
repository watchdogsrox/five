/////////////////////////////////////////////////////////////////////////////////
// FILE    : MobilePhone.cpp

#include "Frontend/MobilePhone.h"

// Rage headers
#include "bank/bkmgr.h"
#include "rline/rlsystemui.h"

// Framework headers
#include "fwsys/gameskeleton.h"

// Game headers
#include "camera/viewports/ViewportManager.h"
#include "Cutscene/CutSceneManagerNew.h"
#include "peds/ped.h"
#include "Streaming/Streaming.h"
#include "audio/radioaudioentity.h"
#include "camera/CamInterface.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/gameplay/aim/ThirdPersonAimCamera.h"
#include "camera/gameplay/aim/ThirdPersonPedAimCamera.h"
#include "camera/helpers/Collision.h"
#include "camera/viewports/Viewport.h"
#include "debug/debugscene.h"
#include "frontend/WarningScreen.h"
#include "renderer/DrawLists/DrawListMgr.h"
#include "renderer/DrawLists/DrawListNY.h"
#include "renderer/RenderPhases/RenderPhaseStdNY.h"
#include "renderer/RenderPhases/renderphaseStd.h"
#include "renderer/lights/lights.h"
#include "renderer/rendertargetmgr.h"
#include "renderer/rendertargets.h"
#include "scene/world/GameWorld.h"
#include "script/script.h"
#include "shaders/CustomShaderEffectTint.h"
#include "Task/General/Phone/TaskMobilePhone.h"

// Defines
#define PHOTO_X_SIZE		256
#define PHOTO_Y_SIZE		256
#define SCREEN_RT_NAME	"PHONE_SCREEN"
#define	PHOTO_RT_NAME	"PHOTO"

#define MOBILE_DEFAULT_BRIGHTNESS			(0.25f)
#define MOBILE_DEFAULT_SCREEN_BRIGHTNESS	(1.0f)

// Game
bool CPhoneMgr::sm_display = false;
u32 CPhoneMgr::sm_removeFlags = 0;
bool CPhoneMgr::sm_goingOffScreen = false;
bool CPhoneMgr::sm_haveDisabledRadioControls = false;

// Render
CPhoneMgr::ePhoneType CPhoneMgr::sm_currentType =CPhoneMgr:: MOBILE_INVALID;
const CModelIndex CPhoneMgr::sm_ModelInfos[CPhoneMgr::MAX_MOBILE_TYPE] = { 
	strStreamingObjectName("Prop_Phone_ING",0x18A2A46E), 
	strStreamingObjectName("Prop_Phone_ING_02",0xD37A5F68),
	strStreamingObjectName("Prop_Phone_ING_03",0xE188FB85),
	strStreamingObjectName("Prop_Police_Phone",0xC5DC419E),
	strStreamingObjectName("Prop_Prologue_Phone",0xF0048B0),
};
#if __BANK
const char* CPhoneMgr::sm_ModelNames[CPhoneMgr::MAX_MOBILE_TYPE] = {
	"Prop_Phone_ING", 
	"Prop_Phone_ING_02",
	"Prop_Phone_ING_03",
	"Prop_Police_Phone",
	"Prop_Prologue_Phone"
};
int CPhoneMgr::sm_phoneType = CPhoneMgr::MOBILE_1;
bool CPhoneMgr::sm_bUseAA = true;

float CPhoneMgr::sm_debugBrightness = MOBILE_DEFAULT_BRIGHTNESS;
float CPhoneMgr::sm_debugScreenBrightness = MOBILE_DEFAULT_SCREEN_BRIGHTNESS;
#endif // __BANK

bool CPhoneMgr::sm_modelReferenced = false;

grcRenderTarget* CPhoneMgr::sm_RTScreen;
s32 CPhoneMgr::sm_RTScreenId = -1;

float CPhoneMgr::sm_brightness[CPhoneMgr::MAX_MOBILE_TYPE] = {
	0.25f,		// MOBILE_1
	0.25f,		// MOBILE_2
	0.25f,		// MOBILE_3
	0.25f,		// MOBILE_POLICE
	0.25f,		// MOBILE_PROLOGUE
};

float CPhoneMgr::sm_screenBrightness[CPhoneMgr::MAX_MOBILE_TYPE] = {
	1.0f,		// MOBILE_1
	1.0f,		// MOBILE_2
	1.0f,		// MOBILE_3
	1.0f,		// MOBILE_POLICE
	1.0f,		// MOBILE_PROLOGUE
};

float CPhoneMgr::sm_scale = 500.0f;
Vector3 CPhoneMgr::sm_angles(-HALF_PI, 0.0f, 0.0f);
EulerAngleOrder CPhoneMgr::sm_EulerAngleOrder = EULER_YXZ;
Vector3 CPhoneMgr::sm_pos(0.0f, 5.0f, -60.0f);
float CPhoneMgr::sm_radius = 0.0f;

// PhoneCam
extern const u32 g_SelfieAimCameraHash;

grcRenderTarget* CPhoneMgr::sm_RTPhoto;

float CPhoneMgr::sm_pedVisibilityMaxDist = 20.0f;

float CPhoneMgr::sm_zoom = 1.0f;
Vector2 CPhoneMgr::sm_centre(0.5f,0.5f);
Vector4 CPhoneMgr::sm_colorBrightness(1.0f,1.0f,1.0f,1.0f);
bool CPhoneMgr::sm_enablePhotoUpdate = false;
bool CPhoneMgr::sm_enableSelfieMode = false;
bool CPhoneMgr::sm_enableShallowDofMode = false;
float CPhoneMgr::sm_cameraSelfieModeSideOffsetScaling = 1.0f;
bool CPhoneMgr::sm_enableHorizontalMode = false;
int CPhoneMgr::sm_latestPhoneInput = CTaskMobilePhone::CELL_INPUT_NONE;
u32 CPhoneMgr::sm_selfieModeActivationTime = 0;
bool CPhoneMgr::sm_enableDOF = false;

#if RSG_PC
bool CPhoneMgr::sm_Initialized = false;
#endif

CPhoneMgr::SelfieFramingParams CPhoneMgr::sm_SelfieFramingParams =
{
	1.0f,				// m_fLateralPan
	1.0f,				// m_fVerticalPan
	1.0f,				// m_fDistance
	0.0f,				// m_fRoll
	0.0f,				// m_fHeadYaw
	0.0f,				// m_fHeadRoll
	0.0f,				// m_fHeadPitch

	1.0f,				// m_fHeadWeight
	8.0f,				// m_fHeadBlendOutRate

	1.0f,				// m_fLateralPanDistanceModifier
	1.0f,				// m_fVerticalPanDistanceModifier

	{ -0.9f, 2.0f },	// m_afLateralPanLimits[2]
	{  0.8f, 2.0f },	// m_afModifiedLateralPanLimits[2]
	{  0.5f, 1.5f },	// m_afVerticalPanLimits[2]
	{  0.5f, 1.35f },	// m_afModifiedVerticalPanLimits[2]
	{  0.74f, 1.0f },	// m_afDistanceLimit[2]
	{ -15.0f, 15.0f },	// m_afRollLimits[2]
	{ -40.0f, 40.0f },	// m_afHeadYawLimits[2]
	{ -20.0f, 20.0f },	// m_afHeadRollLimits[2]
	{ -25.0f, 20.0f }	// m_afHeadPitchLimits[2]
};
camDampedSpring CPhoneMgr::sm_HeadYawSpring;
camDampedSpring CPhoneMgr::sm_HeadRollSpring;
camDampedSpring CPhoneMgr::sm_HeadPitchSpring;

grcBlendStateHandle CPhoneMgr::sm_defaultBS;
grcBlendStateHandle CPhoneMgr::sm_rgbBS;
grcBlendStateHandle CPhoneMgr::sm_rgbNoBlendBS;

#if __BANK
bool CPhoneMgr::sm_isPlayerVisible = false;

bool CPhoneMgr::sm_goFirstPerson = false;

#endif 

////////////////////////////////////////////////////////////////////////////
#define PHONE_SCREEN_RES 256

#if RSG_PC
void CPhoneMgr::DeviceLost()
{
}

void CPhoneMgr::DeviceReset()
{
	if (sm_Initialized)
	{
		CreateRenderTargets();
	}
}
#endif

void CPhoneMgr::CreateRenderTargets()
{
#if RSG_PC
	sm_Initialized = true;
#endif

	USE_MEMBUCKET(MEMBUCKET_RENDER);

	// create render target and register it as a global texture
	const s32 resX = PHONE_SCREEN_RES;
	const s32 resY = PHONE_SCREEN_RES;
#if RSG_PC || RSG_DURANGO || RSG_ORBIS
	const s32 rtWidth = (s32)(((float)resX/1280.0) * SCREEN_WIDTH);
	const s32 rtHeight = (s32)(((float)resY/720.0) * SCREEN_HEIGHT);
#else
	const s32 rtWidth = resX;
	const s32 rtHeight = resY;
#endif

#if RSG_PC
	bool bRegisterTexture = sm_RTScreen == NULL;
#endif
	sm_RTScreen = gRenderTargetMgr.CreateRenderTarget(kRTPoolIDInvalid, SCREEN_RT_NAME, rtWidth, rtHeight, 0 WIN32PC_ONLY(, sm_RTScreen));

#if RSG_PC
	if (bRegisterTexture)
#endif
	{
	grcTextureFactory::GetInstance().RegisterTextureReference(SCREEN_RT_NAME, sm_RTScreen);
	}

	// Photo support
	u16 poolID = kRTPoolIDInvalid;
	u8 heapID = 0;
	PPU_ONLY(poolID = CRenderTargets::GetRenderTargetPoolID(PSN_RTMEMPOOL_P1024_TILED_LOCAL_COMPMODE_C32_2X1));	// mempools: only pitch is important
	XENON_ONLY(poolID = CRenderTargets::GetRenderTargetPoolID(XENON_RTMEMPOOL_SHADOWS));
	XENON_ONLY(heapID = kPhotoHeap);

	// set render target descriptions
	grcTextureFactory::CreateParams params;
	params.Multisample	= 0;
	params.HasParent	= true;
#if __XENON //requires parent to prevent this from spamming backbuffer in EDRAM
	params.Parent		= grcTextureFactory::GetInstance().GetBackBuffer(false); 
#else
	params.Parent		= NULL; 
#endif
	params.UseFloat		= true;
	params.Format		= grctfA8R8G8B8;
	params.MipLevels	= 1;
#if __PPU
	params.InTiledMemory= true;
#endif
	params.PoolID = poolID;
	params.PoolHeap = heapID;
	params.AllocateFromPoolOnCreate = true; // for now this is compatible with the old way, when render targets were always "locked" and ready for use.

#if RSG_PC
	bRegisterTexture = sm_RTPhoto == NULL;
#endif

	sm_RTPhoto = grcTextureFactory::GetInstance().CreateRenderTarget( PHOTO_RT_NAME, grcrtPermanent, PHOTO_X_SIZE, PHOTO_Y_SIZE, 32, &params WIN32PC_ONLY(, sm_RTPhoto));

#if RSG_PC
	if (bRegisterTexture)
#endif
	{
	grcTextureFactory::GetInstance().RegisterTextureReference(PHOTO_RT_NAME, sm_RTPhoto);
	}
}

void CPhoneMgr::DestroyRenderTargets()
{
	if(sm_RTScreen)
	{
		sm_RTScreen->Release();
		sm_RTScreen = NULL;
	}

	grcTextureFactory::GetInstance().DeleteTextureReference(SCREEN_RT_NAME);

	if (sm_RTPhoto)
	{
		sm_RTPhoto->Release();
		sm_RTPhoto = NULL;
	}

	grcTextureFactory::GetInstance().DeleteTextureReference(PHOTO_RT_NAME);
}

void CPhoneMgr::Init(unsigned initMode)
{
    if(initMode == INIT_CORE)
    {
		sm_RTScreen = NULL;
		sm_RTPhoto = NULL;

		CreateRenderTargets();
		
		grcBlendStateDesc blendDesc;
		//@@: location CPHONEMGR_INIT_CORE
		blendDesc.BlendRTDesc[0].BlendEnable = true;
		blendDesc.BlendRTDesc[0].DestBlend = grcRSV::BLEND_INVSRCALPHA;
		blendDesc.BlendRTDesc[0].SrcBlend = grcRSV::BLEND_SRCALPHA;
		blendDesc.BlendRTDesc[0].BlendOp = grcRSV::BLENDOP_ADD;
		sm_defaultBS = grcStateBlock::CreateBlendState(blendDesc);
		
		blendDesc.BlendRTDesc[0].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_RGB;
		
		sm_rgbBS = grcStateBlock::CreateBlendState(blendDesc);

		blendDesc.BlendRTDesc[0].BlendEnable = false;
		
		sm_rgbNoBlendBS = grcStateBlock::CreateBlendState(blendDesc);
    }
    else if(initMode == INIT_SESSION)
    {
	    sm_display = false;
	    sm_currentType = MOBILE_1;
		
	    sm_removeFlags = false;
	    sm_modelReferenced = false;

		sm_selfieModeActivationTime = 0;

		sm_enableDOF = false;
	}
}


void CPhoneMgr::Shutdown(unsigned shutdownMode)
{
    if(shutdownMode == SHUTDOWN_CORE)
    {
		DestroyRenderTargets();

		grcStateBlock::DestroyBlendState(sm_defaultBS);
		grcStateBlock::DestroyBlendState(sm_rgbBS);
		grcStateBlock::DestroyBlendState(sm_rgbNoBlendBS);		
    }
    else if(shutdownMode == SHUTDOWN_SESSION)
    {
	    CamActivate(false,false);
	    Delete();

		sm_selfieModeActivationTime = 0;
	}
}


void CPhoneMgr::Update()
{
	// shuts down the phone if a cutscene is started
	if ((CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->IsRunning()) && (!CutSceneManager::GetInstance()->IsStreaming()))
	{
		if (!GetRemoveFlags())
		{
			SetRemoveFlags(CPhoneMgr::WANTS_PHONE_REMOVED_FRONTEND);
		}
	}
	else
	{
		ClearRemoveFlags(CPhoneMgr::WANTS_PHONE_REMOVED_FRONTEND);
	}

	// Put the phone away if the player has been damaged in the last second
	CPed * pPlayer = CGameWorld::FindLocalPlayer();
	if( pPlayer && (pPlayer->GetPlayerInfo()->LastTimeEnergyLost + 1000) > fwTimer::GetTimeInMilliseconds() )
	{
		SetRemoveFlags(CPhoneMgr::WANTS_PHONE_REMOVED_PLAYER_DAMAGED);
	}
	else
	{
		ClearRemoveFlags(CPhoneMgr::WANTS_PHONE_REMOVED_PLAYER_DAMAGED);
	}

	// Leave the code wants phone removed flag for a frame (hence S1 and S2)
	if( GetRemoveFlags() & CPhoneMgr::WANTS_PHONE_REMOVED_PHONE_TASK_ABORTED_S2 )
	{
		ClearRemoveFlags(CPhoneMgr::WANTS_PHONE_REMOVED_PHONE_TASK_ABORTED_S2);
	}
	if( GetRemoveFlags() & CPhoneMgr::WANTS_PHONE_REMOVED_PHONE_TASK_ABORTED_S1 )
	{
		ClearRemoveFlags(CPhoneMgr::WANTS_PHONE_REMOVED_PHONE_TASK_ABORTED_S1);
		SetRemoveFlags(CPhoneMgr::WANTS_PHONE_REMOVED_PHONE_TASK_ABORTED_S2);
	}

	// Leave the code wants phone removed flag for a frame (hence S1 and S2)
	if( GetRemoveFlags() & CPhoneMgr::WANTS_PHONE_REMOVED_WEAPON_SWITCHING_S2 )
	{
		ClearRemoveFlags(CPhoneMgr::WANTS_PHONE_REMOVED_WEAPON_SWITCHING_S2);
	}
	if( GetRemoveFlags() & CPhoneMgr::WANTS_PHONE_REMOVED_WEAPON_SWITCHING_S1)
	{
		ClearRemoveFlags(CPhoneMgr::WANTS_PHONE_REMOVED_WEAPON_SWITCHING_S1);
		SetRemoveFlags(CPhoneMgr::WANTS_PHONE_REMOVED_WEAPON_SWITCHING_S2);
	}

	UpdateSelfieFramingParams();
}

const CModelIndex& CPhoneMgr::GetPhoneModelIndex()
{
	return sm_ModelInfos[sm_currentType];
}

void CPhoneMgr::Render(eDrawPhoneMode drawMode)
{
#if RSG_ORBIS
	// avoid flicker on return from system UI
	static s32 s_delayCnt = 0;
	if (s_delayCnt <= 0)
	{
		if  (g_SystemUi.IsUiShowing())
		{
			s_delayCnt = 2;
			return;
		}
	}
	else
	{
		if (drawMode != DRAWPHONE_BODY_ONLY)
			s_delayCnt--;
		return;
	}
#endif

	if (sm_display && !CWarningScreen::IsActive())
	{
		CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfo(sm_ModelInfos[sm_currentType]);
		if (pModelInfo->GetHasLoaded())
		{

			sm_radius = pModelInfo->GetBoundingSphereRadius();

			int palletId = 0xff;

			// Set the light for mobile phone
			Matrix34 camMtx;
			float naturalAmbientScale = 1.0f;
			float artifcialAmbientScale = 1.0f;
			bool inInterior = false;
			camMtx.Identity();

			CPed* player = FindPlayerPed();

			if(player)
			{
				naturalAmbientScale = player->GetNaturalAmbientScale() / 255.0f;
				artifcialAmbientScale = player->GetArtificialAmbientScale() / 255.0f;
				inInterior = player->GetInteriorLocation().IsValid();
				camMtx = MAT34V_TO_MATRIX34(player->GetMatrix());
				palletId = player->GetPedPhonePaletteIdx();
			}
			else
			{
				RCC_MATRIX44(grcViewport::GetCurrent()->GetModelViewMtx()).ToMatrix34(camMtx);
			}
			camMtx.Inverse();


#if CSE_TINT_EDITABLEVALUES
			if(CCustomShaderEffectTint::ms_bEVEnableForceTintPalette)
			{
				palletId = CCustomShaderEffectTint::ms_nEVForcedTintPalette;
			}
#endif // CSE_TINT_EDITABLEVALUES

			DLC ( CDrawPhoneDC_NY, 
					(sm_ModelInfos[sm_currentType], 
					 sm_pos, sm_scale, 
					 sm_angles, 
					 sm_EulerAngleOrder, 
					 sm_brightness[(int)sm_currentType], 
					 sm_screenBrightness[(int)sm_currentType],
					 naturalAmbientScale,
					 artifcialAmbientScale,
					 palletId,
					 camMtx,
					 (CDrawPhoneDC_NY::eDrawPhoneMode)drawMode,
					 inInterior));
		}
		else
		{
			LoadModel();
		}

	}
}


#if __BANK
void CPhoneMgr::InitWidgets()
{
	bkBank& bank = BANKMGR.CreateBank("Mobile Phone");
	
	bank.PushGroup("Rendering");
	
	bank.AddToggle("Apply AA", &sm_bUseAA);
	bank.AddToggle("Show Scissor Rect", &CRenderPhasePhoneModel::ms_bDrawScissorRect);
	bank.AddCombo("Phone Model", &sm_phoneType,MAX_MOBILE_TYPE,sm_ModelNames);
	
	bank.AddButton("Create Phone",CreateCB);
	bank.AddButton("Destroy Phone",DestroyCB);
	
	bank.AddSlider("Brightness", &sm_debugBrightness, 0.0f, 16.0f, 0.1f, ChangeBrightnessCB);
	bank.AddSlider("Screen Brightness", &sm_debugScreenBrightness, 0.0f, 16.0f, 0.1f, ChangeScreenBrightnessCB);
	
	bank.AddSlider("Scale", &sm_scale, 0.0f, 1000.0f, 1.0f);

	bank.AddSlider("Angle Rotate in X-Axis", &sm_angles.x, -PI, PI, 0.01f);
	bank.AddSlider("Angle Rotate in Y-Axis", &sm_angles.y, -PI, PI, 0.01f);
	bank.AddSlider("Angle Rotate in Z-Axis", &sm_angles.z, -PI, PI, 0.01f);

	static const char *eulerOrderList[]= {"XYZ", "XZY", "YXZ", "YZX", "ZXY", "ZYX"};
	bank.AddCombo("Rotation Order", (s32*)&sm_EulerAngleOrder, 6, &eulerOrderList[0]);

	bank.AddSlider("Position X", &sm_pos.x, -1000.0f, 1000.0f, 1.0f);
	bank.AddSlider("Position Y", &sm_pos.y, -1000.0f, 1000.0f, 1.0f);
	bank.AddSlider("Position Z", &sm_pos.z, -1000.0f, 1000.0f, 1.0f);

	bank.PopGroup();

	bank.PushGroup("Camera");
	bank.AddSlider("Photo Centre X",&sm_centre.x,0.0f,1.0f,0.01f);
	bank.AddSlider("Photo Centre Y",&sm_centre.y,0.0f,1.0f,0.01f);
	bank.AddSlider("Photo Zoom",&sm_zoom,0.0f,1.0f,0.01f);
	bank.AddToggle("Enable Photo Update",&sm_enablePhotoUpdate,CamActivateCB);
	bank.AddToggle("Go first Person",&sm_goFirstPerson,CamActivateCB);
	bank.AddToggle("Enable Selfie Mode",&sm_enableSelfieMode);
	bank.AddToggle("Enable Shallow-DOF Mode",&sm_enableShallowDofMode);
	bank.AddSlider("Selfie Mode Side Offset Scaling",&sm_cameraSelfieModeSideOffsetScaling, -10.0f,10.0f,0.01f);
	bank.AddButton("Check Is Player Visible",CamIsCharVisibleWidget);
	bank.AddText("Check Is Player Visible Result",&sm_isPlayerVisible);
	bank.AddSlider("Max Visibility",&sm_pedVisibilityMaxDist,0.0f,1000.0f,1.0f);
	bank.AddSlider("Tint Red",&sm_colorBrightness.x,0.0f,1.0f,0.01f);
	bank.AddSlider("Tint Green",&sm_colorBrightness.y,0.0f,1.0f,0.01f);
	bank.AddSlider("Tint Blue",&sm_colorBrightness.z,0.0f,1.0f,0.01f);
	bank.AddSlider("Brightness",&sm_colorBrightness.w,0.0f,1.0f,0.01f);
		bank.PushGroup("Selfie Framing Params");
		bank.AddSlider("Lateral Pan", &sm_SelfieFramingParams.m_fLateralPan, -2.0f, 2.0f, 0.01f);
		bank.AddSlider("Vertical Pan", &sm_SelfieFramingParams.m_fVerticalPan, -2.0f, 2.0f, 0.01f);
		bank.AddSlider("Distance", &sm_SelfieFramingParams.m_fDistance, 0.0f, 1.0f, 0.01f);
		bank.AddText("Lateral Pan Distance Modifier", &sm_SelfieFramingParams.m_fLateralPanDistanceModifier, true);
		bank.AddText("Vertical Pan Distance Modifier", &sm_SelfieFramingParams.m_fVerticalPanDistanceModifier, true);
		bank.AddSlider("Roll", &sm_SelfieFramingParams.m_fRoll, -1.0f, 1.0f, 0.01f);
		bank.AddSlider("Head Yaw", &sm_SelfieFramingParams.m_fHeadYaw, -1.0f, 1.0f, 0.01f);
		bank.AddSlider("Head Roll", &sm_SelfieFramingParams.m_fHeadRoll, -1.0f, 1.0f, 0.01f);
		bank.AddSlider("Head Pitch", &sm_SelfieFramingParams.m_fHeadPitch, -1.0f, 1.0f, 0.01f);
		bank.AddText("Head Weight", &sm_SelfieFramingParams.m_fHeadWeight, true);
			bank.PushGroup("Selfie Framing Limits");
			bank.AddSlider("Min Lateral Pan Limit", &sm_SelfieFramingParams.m_afLateralPanLimits[0], -2.0f, 2.0f, 0.01f);
			bank.AddSlider("Max Lateral Pan Limit", &sm_SelfieFramingParams.m_afLateralPanLimits[1], -2.0f, 2.0f, 0.01f);
			bank.AddSlider("Min Vertical Pan Limit", &sm_SelfieFramingParams.m_afVerticalPanLimits[0], -2.0f, 2.0f, 0.01f);
			bank.AddSlider("Max Vertical Pan Limit", &sm_SelfieFramingParams.m_afVerticalPanLimits[1], -2.0f, 2.0f, 0.01f);
			bank.AddSlider("Min Distance Limit", &sm_SelfieFramingParams.m_afDistanceLimits[0], 0.0f, 1.0f, 0.01f);
			bank.AddSlider("Max Distance Limit", &sm_SelfieFramingParams.m_afDistanceLimits[1], 0.0f, 1.0f, 0.01f);
			bank.AddSlider("Min Roll Limit", &sm_SelfieFramingParams.m_afRollLimits[0], -180.0f, 0.0f, 0.1f);
			bank.AddSlider("Max Roll Limit", &sm_SelfieFramingParams.m_afRollLimits[1], 0.0f, 180.0f, 0.1f);
			bank.AddSlider("Min Head Yaw Limit", &sm_SelfieFramingParams.m_afHeadYawLimits[0], -180.0f, 0.0f, 0.1f);
			bank.AddSlider("Max Head Yaw Limit", &sm_SelfieFramingParams.m_afHeadYawLimits[1], 0.0f, 180.0f, 0.1f);
			bank.AddSlider("Min Head Roll Limit", &sm_SelfieFramingParams.m_afHeadRollLimits[0], -180.0f, 0.0f, 0.1f);
			bank.AddSlider("Max Head Roll Limit", &sm_SelfieFramingParams.m_afHeadRollLimits[1], 0.0f, 180.0f, 0.1f);
			bank.AddSlider("Min Head Pitch Limit", &sm_SelfieFramingParams.m_afHeadPitchLimits[0], -180.0f, 0.0f, 0.1f);
			bank.AddSlider("Max Head Pitch Limit", &sm_SelfieFramingParams.m_afHeadPitchLimits[1], 0.0f, 180.0f, 0.1f);
			bank.PopGroup();
		bank.PopGroup();
	bank.PopGroup();

	CTaskMobilePhone::InitWidgets();
}
#endif // __BANK


void CPhoneMgr::Create(ePhoneType mobileType)
{
	//@@: location AHOOK_0067_CHK_LOCATION_A
	sm_currentType = mobileType;
	sm_display = true;
	LoadModel();
}


void CPhoneMgr::Delete()
{
	CamActivate(false,false);
	CamActivateSelfieMode(false);
	// Unload the model
	if(sm_modelReferenced)
	{
		(CModelInfo::GetBaseModelInfo(sm_ModelInfos[sm_currentType]))->RemoveRef();
		sm_modelReferenced = false;
	}
	sm_display = false;
}


void CPhoneMgr::CamActivate(bool bEnablePhotoUpdate, bool UNUSED_PARAM(bGoFirstPerson))
{
	//the movement is now handled by the player mobile phone task; 
	sm_enablePhotoUpdate = bEnablePhotoUpdate;
}

void CPhoneMgr::CamActivateSelfieMode(bool bShouldActivate)
{
	sm_enableSelfieMode = bShouldActivate;
	sm_selfieModeActivationTime = fwTimer::GetTimeInMilliseconds();
}

void CPhoneMgr::CamActivateShallowDofMode(bool bShouldActivate)
{
	sm_enableShallowDofMode = bShouldActivate;
}

void CPhoneMgr::CamSetSelfieModeSideOffsetScaling(float scaling)
{
	sm_cameraSelfieModeSideOffsetScaling = scaling;
}

bool CPhoneMgr::CamIsCharVisible(int ped, bool bFaceCheck)
{
	// We know the window pos and the zoom, so we need to define a window in which the peds head is in.

	// then do 3 tests...
	const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(ped);
	if (!pPed)
		return false;

	// 1) The ped is facing the camera.
	Matrix34 mat;
	pPed->GetBoneMatrix(mat, BONETAG_HEAD);

	Vector3 headPos    = mat.d;
	Vector3 pedHeadDir = mat.b;

	const camFrame& cameraFrame = camInterface::GetGameplayDirector().GetFrame();
	const Vector3& camDir = cameraFrame.GetFront();
	const Vector3& camPos = cameraFrame.GetPosition();

	if (bFaceCheck)
	{
		float dot = pedHeadDir.Dot(camDir);
		if (dot > 0.0f)
			return false;
	}

	// 1.5) need to determine if the position is in front of our facing direction!
	// quick hacky way of doing this... I'm sure there is better dotproduct way, but a bit rushed now.
	float d1 = Vector3(camPos-headPos).Mag();
	float d2 = Vector3(camPos+(camDir*0.01f)-headPos).Mag();

	if (d2>d1)
		return false;

	// 1.75) What if ped is too far away?
	float d3 = d1 * sm_zoom;
	if (d3>sm_pedVisibilityMaxDist)
		return false;

	// 2) There is a clear line of sight from the camera to the peds head. ( ignoring see through stuff ) 
	WorldProbe::CShapeTestFixedResults<> probeResults;
	WorldProbe::CShapeTestProbeDesc probeDesc;
	probeDesc.SetStartAndEnd(camPos, headPos);
	probeDesc.SetResultsStructure(&probeResults);
	probeDesc.SetIncludeFlags(camCollision::GetCollisionIncludeFlags());
	probeDesc.SetOptions(WorldProbe::LOS_IGNORE_SEE_THRU|WorldProbe::LOS_GO_THRU_GLASS);

	WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc);
	if(probeResults[0].GetHitDetected())
		return false;

	// 3) The peds head position when projected to screen is inside the window used in the photo copy.
	Vector2 mini(-0.5f,-0.5f);
	Vector2 maxi(0.5f,0.5f);

	mini *= sm_zoom;
	mini += sm_centre;

	maxi *= sm_zoom;
	maxi += sm_centre;

	Vector2 projectedPos;
	CViewport* pVP = gVpMan.GetGameViewport();

	// get screen coordinates of the point:
	pVP->GetGrcViewport().Transform(VECTOR3_TO_VEC3V(headPos),projectedPos.x,projectedPos.y);
	
	float fLastWidth = pVP->GetLastWidth();
	float fLastHeight = pVP->GetLastHeight();

	projectedPos.x /= fLastWidth;
	projectedPos.y /= fLastHeight;

	if (projectedPos.x > mini.x && projectedPos.x < maxi.x && 
		projectedPos.y > mini.y && projectedPos.y < maxi.y)
	{
		return true;
	}

	return false;
}


void CPhoneMgr::LoadModel()
{
	if (sm_display)
	{
		// Display Phone model
		fwModelId modelId = sm_ModelInfos[sm_currentType];
		if (!CModelInfo::HaveAssetsLoaded(modelId))
		{
			CModelInfo::RequestAssets(modelId, STRFLAG_FORCE_LOAD|STRFLAG_DONTDELETE|STRFLAG_PRIORITY_LOAD);
			return;
		}

		// Changed from an assert to a condition to match CPhoneMgr::Delete() logic
		// Note: this was causing bug #35067 due to the fact that CPhoneMgr::Create can
		// be called multiple times without calling CPhoneMgr::Delete().
		if( Verifyf( !sm_modelReferenced, "Attempt to load the phone model without properly calling delete." ) )
		{
			CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfo(sm_ModelInfos[sm_currentType]);
			Assert(pModelInfo);
			pModelInfo->AddRef();
			sm_modelReferenced = true;

			sm_radius = pModelInfo->GetBoundingSphereRadius();
		}
	}
}

void CPhoneMgr::HorizontalActivate(bool bActivate)
{
	sm_enableHorizontalMode = bActivate;
}

bool CPhoneMgr::GetHorizontalModeActive()
{
	return sm_enableHorizontalMode;
}

void CPhoneMgr::SetLatestPhoneInput(int phoneInput)
{
	sm_latestPhoneInput = phoneInput;
}

int CPhoneMgr::GetLatestPhoneInput()
{
	return sm_latestPhoneInput;
}

float CPhoneMgr::CamGetSelfieModeSideOffsetScaling()
{
	float fMinLimit;
	float fMaxLimit;

	GetLateralPanLimits(fMinLimit, fMaxLimit);

	return sm_cameraSelfieModeSideOffsetScaling * Clamp(sm_SelfieFramingParams.m_fLateralPan, fMinLimit, fMaxLimit);
}

float CPhoneMgr::CamGetSelfieModeVertOffsetScaling()
{
	float fMinLimit;
	float fMaxLimit;

	GetVerticalPanLimits(fMinLimit, fMaxLimit);

	return Clamp(sm_SelfieFramingParams.m_fVerticalPan, fMinLimit, fMaxLimit);
}

void CPhoneMgr::CamSetSelfieModeHorzPanOffset(float scaling)
{
	sm_SelfieFramingParams.m_fLateralPan = scaling;
}

void CPhoneMgr::CamSetSelfieModeVertPanOffset(float scaling)
{
	sm_SelfieFramingParams.m_fVerticalPan = scaling;
}

void CPhoneMgr::CamSetSelfieRoll(float roll)
{
	sm_SelfieFramingParams.m_fRoll = Clamp(roll, -1.0f, 1.0f);
}

void CPhoneMgr::CamSetSelfieDistance(float distance)
{
	sm_SelfieFramingParams.m_fDistance = Clamp(distance, 0.0f, 1.0f);
}

void CPhoneMgr::CamSetSelfieHeadYaw(float yaw)
{
	sm_SelfieFramingParams.m_fHeadYaw = Clamp(yaw, -1.0f, 1.0f);
}

void CPhoneMgr::CamSetSelfieHeadRoll(float roll)
{
	sm_SelfieFramingParams.m_fHeadRoll = Clamp(roll, -1.0f, 1.0f);
}

void CPhoneMgr::CamSetSelfieHeadPitch(float pitch)
{
	sm_SelfieFramingParams.m_fHeadPitch = Clamp(pitch, -1.0f, 1.0f);
}

float CPhoneMgr::CamGetSelfieRoll()
{
	return (sm_SelfieFramingParams.m_fRoll < 0.0f) ? (fabsf(sm_SelfieFramingParams.m_fRoll) * sm_SelfieFramingParams.m_afRollLimits[0]) : (sm_SelfieFramingParams.m_fRoll * sm_SelfieFramingParams.m_afRollLimits[1]);
}

float CPhoneMgr::CamGetSelfieDistance()
{
	const float fInterp = sm_SelfieFramingParams.m_fDistance * Min(sm_SelfieFramingParams.m_fLateralPanDistanceModifier, sm_SelfieFramingParams.m_fVerticalPanDistanceModifier);
	return Lerp(fInterp, sm_SelfieFramingParams.m_afDistanceLimits[0], sm_SelfieFramingParams.m_afDistanceLimits[1]);
}

float CPhoneMgr::CamGetSelfieHeadYaw()
{
	return sm_HeadYawSpring.GetResult() * sm_SelfieFramingParams.m_fHeadWeight;
}

float CPhoneMgr::CamGetSelfieHeadRoll()
{
	return sm_HeadRollSpring.GetResult() * sm_SelfieFramingParams.m_fHeadWeight;
}

float CPhoneMgr::CamGetSelfieHeadPitch()
{
	return sm_HeadPitchSpring.GetResult() * sm_SelfieFramingParams.m_fHeadWeight;
}

void CPhoneMgr::UpdateSelfieFramingParams()
{
	if (!CamGetSelfieModeState())
	{
		ResetSelfieFramingParams();
		return;
	}

	UpdateSelfieHeadWeight();

	// Update additive yaw/roll/pitch
	const float kfSpringConstant = 200.0f;
	const float kfDampingRatio = 1.0f;

	const camFrame& cameraFrame = camInterface::GetGameplayDirector().GetFrame();
	const bool bCut = cameraFrame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition) || cameraFrame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation);
	const float fSpringConstant = bCut ? 0.0f : kfSpringConstant;

	float fYaw = (sm_SelfieFramingParams.m_fHeadYaw < 0.0f) ? (fabsf(sm_SelfieFramingParams.m_fHeadYaw) * sm_SelfieFramingParams.m_afHeadYawLimits[0]) : (sm_SelfieFramingParams.m_fHeadYaw * sm_SelfieFramingParams.m_afHeadYawLimits[1]);
	float fRoll = (sm_SelfieFramingParams.m_fHeadRoll < 0.0f) ? (fabsf(sm_SelfieFramingParams.m_fHeadRoll) * sm_SelfieFramingParams.m_afHeadRollLimits[0]) : (sm_SelfieFramingParams.m_fHeadRoll * sm_SelfieFramingParams.m_afHeadRollLimits[1]);
	float fPitch = (sm_SelfieFramingParams.m_fHeadPitch < 0.0f) ? (fabsf(sm_SelfieFramingParams.m_fHeadPitch) * sm_SelfieFramingParams.m_afHeadPitchLimits[0]) : (sm_SelfieFramingParams.m_fHeadPitch * sm_SelfieFramingParams.m_afHeadPitchLimits[1]);

	sm_HeadYawSpring.Update(fYaw, fSpringConstant, kfDampingRatio);
	sm_HeadRollSpring.Update(fRoll, fSpringConstant, kfDampingRatio);
	sm_HeadPitchSpring.Update(fPitch, fSpringConstant, kfDampingRatio);

	// Update distance based on panning limits
	const float kfMinLateralPanDistanceModifierRt = 0.60f;
	const float kfMinLateralPanDistanceModifierLt = 0.40f;
	const float kfMinVerticalPanDistanceModifier = 0.50f;

	float fMinLateralPanDistanceModifier;

	if (sm_SelfieFramingParams.m_fLateralPan >= 1.0f)
	{
		sm_SelfieFramingParams.m_fLateralPanDistanceModifier = (sm_SelfieFramingParams.m_afLateralPanLimits[1] - 1.0f) != 0.0f ? Clamp((sm_SelfieFramingParams.m_fLateralPan - 1.0f) / (sm_SelfieFramingParams.m_afLateralPanLimits[1] - 1.0f), 0.0f, 1.0f) : 0.0f;
		fMinLateralPanDistanceModifier = kfMinLateralPanDistanceModifierRt;
	}
	else
	{
		sm_SelfieFramingParams.m_fLateralPanDistanceModifier = (1.0f - sm_SelfieFramingParams.m_afLateralPanLimits[0]) != 0.0f ? Clamp((1.0f - sm_SelfieFramingParams.m_fLateralPan) / (1.0f - sm_SelfieFramingParams.m_afLateralPanLimits[0]), 0.0f, 1.0f) : 0.0f;
		fMinLateralPanDistanceModifier = kfMinLateralPanDistanceModifierLt;
	}
	sm_SelfieFramingParams.m_fLateralPanDistanceModifier = Max(1.0f - (sm_SelfieFramingParams.m_fLateralPanDistanceModifier * sm_SelfieFramingParams.m_fLateralPanDistanceModifier), fMinLateralPanDistanceModifier);

	if (sm_SelfieFramingParams.m_fVerticalPan >= 1.0f)
	{
		sm_SelfieFramingParams.m_fVerticalPanDistanceModifier = (sm_SelfieFramingParams.m_afVerticalPanLimits[1] - 1.0f) != 0.0f ? Clamp((sm_SelfieFramingParams.m_fVerticalPan - 1.0f) / (sm_SelfieFramingParams.m_afVerticalPanLimits[1] - 1.0f), 0.0f, 1.0f) : 0.0f;
	}
	else
	{
		sm_SelfieFramingParams.m_fVerticalPanDistanceModifier = 0.0f;
	}
	sm_SelfieFramingParams.m_fVerticalPanDistanceModifier = Max(1.0f - (sm_SelfieFramingParams.m_fVerticalPanDistanceModifier * sm_SelfieFramingParams.m_fVerticalPanDistanceModifier), kfMinVerticalPanDistanceModifier);
}

void CPhoneMgr::ResetSelfieFramingParams()
{
	sm_HeadYawSpring.Reset(0.0f);
	sm_HeadRollSpring.Reset(0.0f);
	sm_HeadPitchSpring.Reset(0.0f);

	sm_SelfieFramingParams.m_fHeadWeight = 1.0f;
	sm_SelfieFramingParams.m_fHeadBlendOutRate = 8.0f;
}

void CPhoneMgr::UpdateSelfieHeadWeight()
{
	static const float afBlendDuration[CClipEventTags::CSelfieHeadControlEventTag::BtCount] = 
	{
		1.000f,	// BtSlowest
		0.533f,	// BtSlow
		0.267f,	// BtNormal
		0.133f,	// BtFast
		0.067f	// BtFastest
	};

	const float fTimeStep = fwTimer::GetTimeStep();

	float fDesiredHeadWeight = 1.0f;
	float fBlendRate = sm_SelfieFramingParams.m_fHeadBlendOutRate;

	CPed* pPlayer = FindPlayerPed();
	if (pPlayer)
	{
		fwAnimDirector* pAnimDirector = pPlayer->GetAnimDirector();

		if (pAnimDirector)
		{
			const CClipEventTags::CSelfieHeadControlEventTag* pTag = static_cast<const CClipEventTags::CSelfieHeadControlEventTag*>(pAnimDirector->FindPropertyInMoveNetworks(CClipEventTags::SelfieHeadControl));

			if (pTag)
			{
				fDesiredHeadWeight = pTag->GetWeight();
				fBlendRate = 1.0f / afBlendDuration[pTag->GetBlendIn()];
				sm_SelfieFramingParams.m_fHeadBlendOutRate = 1.0f / afBlendDuration[pTag->GetBlendOut()];
			}
		}
	}

	sm_SelfieFramingParams.m_fHeadWeight = Lerp(Min(fBlendRate * fTimeStep, 1.0f), sm_SelfieFramingParams.m_fHeadWeight, fDesiredHeadWeight);
}

void CPhoneMgr::GetLateralPanLimits(float& fMinLimit, float& fMaxLimit)
{
	// Scale min lateral pan limit based on camera pitch
	const camThirdPersonAimCamera* pCamera = camInterface::GetGameplayDirector().GetThirdPersonAimCamera();
	float fMinLimitInterp = 0.0f;

	if (pCamera && (pCamera->GetNameHash() == g_SelfieAimCameraHash) && pCamera->GetIsClassId(camThirdPersonPedAimCamera::GetStaticClassId()))
	{
		const camThirdPersonPedAimCamera* pAimCamera = static_cast<const camThirdPersonPedAimCamera*>(pCamera);
		const float fLimitsRatio = pAimCamera->GetOrbitHeadingLimitsRatio();

		if (fLimitsRatio < 0.0f)
		{
			fMinLimitInterp = fabsf(fLimitsRatio);
		}
	}

	fMinLimit = Lerp(fMinLimitInterp, sm_SelfieFramingParams.m_afLateralPanLimits[0], sm_SelfieFramingParams.m_afModifiedLateralPanLimits[0]);
	fMaxLimit = sm_SelfieFramingParams.m_afLateralPanLimits[1];
}

void CPhoneMgr::GetVerticalPanLimits(float& fMinLimit, float& fMaxLimit)
{
	// Scale max vertical pan limit based on camera pitch
	const camThirdPersonAimCamera* pCamera = camInterface::GetGameplayDirector().GetThirdPersonAimCamera();
	float fMaxLimitInterp = 0.0f;

	if (pCamera && (pCamera->GetNameHash() == g_SelfieAimCameraHash))
	{
		const float fPitch = camInterface::GetPitch();

		if (fPitch < 0.0f)
		{
			const Vector2& vOrbitPitchLimits = pCamera->GetOrbitPitchLimits();
			fMaxLimitInterp = (vOrbitPitchLimits.x != 0.0f) ? Clamp((fabsf(fPitch) / fabsf(vOrbitPitchLimits.x)), 0.0f, 1.0f) : 0.0f;
		}
	}

	fMinLimit = sm_SelfieFramingParams.m_afVerticalPanLimits[0];
	fMaxLimit = Lerp(fMaxLimitInterp, sm_SelfieFramingParams.m_afVerticalPanLimits[1], sm_SelfieFramingParams.m_afModifiedVerticalPanLimits[1]);
}

#if __BANK
void CPhoneMgr::CamIsCharVisibleWidget()
{
	CPed *pPed = NULL;
	CEntity* pFocusEntity = CDebugScene::FocusEntities_Get(0);
	if(pFocusEntity && pFocusEntity->GetIsTypePed())
	{
		pPed = (CPed *)pFocusEntity;
	}
	else
	{
		pPed = FindPlayerPed();
	}

	if(pPed)
	{
		s32 PedIndex = CTheScripts::GetGUIDFromEntity(*pPed);
		sm_isPlayerVisible = CamIsCharVisible(PedIndex);
	}
}


void CPhoneMgr::CreateCB()
{
	CPhoneMgr::Create((ePhoneType)sm_phoneType);
}


void CPhoneMgr::DestroyCB()
{
	CPhoneMgr::Delete();
}


void CPhoneMgr::CamActivateCB()
{
	CamActivate(sm_enablePhotoUpdate,sm_goFirstPerson);
}

void CPhoneMgr::ChangeBrightnessCB()
{
	sm_brightness[sm_phoneType] = sm_debugBrightness;

}

void CPhoneMgr::ChangeScreenBrightnessCB() 
{
	sm_screenBrightness[sm_phoneType] = sm_debugScreenBrightness;
}

#endif //__BANK
