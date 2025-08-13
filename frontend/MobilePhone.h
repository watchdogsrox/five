
#ifndef __MOBILE_PHONE_H
#define __MOBILE_PHONE_H

#include "fwutil/xmacro.h"
#include "vector/vector3.h"
#include "camera/helpers/DampedSpring.h"
#include "Game/ModelIndices.h"

namespace rage {
	class grcRenderTarget;
	class grcTexture;
	class grcViewport;
	class grcImage;
};

class CBaseModelInfo;
class CRenderPhaseScript2d;
class CViewport;

class CPhoneMgr
{
public:
	enum ePhoneType
	{
		MOBILE_INVALID = -1,
		MOBILE_1 = 0,
		MOBILE_2,
		MOBILE_3,
		MOBILE_POLICE,
		MOBILE_PROLOGUE,
		MAX_MOBILE_TYPE
	};

	enum 
	{
		WANTS_PHONE_REMOVED_FRONTEND = (1<<0), 
		WANTS_PHONE_REMOVED_PLAYER_DAMAGED = (1<<1), 
		WANTS_PHONE_REMOVED_PHONE_TASK_ABORTED_S1 = (1<<2),
		WANTS_PHONE_REMOVED_PHONE_TASK_ABORTED_S2 = (1<<3),
		WANTS_PHONE_REMOVED_CUTSCENE = (1<<4),
		WANTS_PHONE_REMOVED_STUNTJUMP = (1<<5),
		WANTS_PHONE_REMOVED_WEAPON_SWITCHING_S1 = (1<<6),
		WANTS_PHONE_REMOVED_WEAPON_SWITCHING_S2 = (1<<7),
		WANTS_PHONE_REMOVED_GARAGES = (1<<8)
	};

	enum eDrawPhoneMode 
	{
		DRAWPHONE_ALL = 0,
		DRAWPHONE_BODY_ONLY,
		DRAWPHONE_SCREEN_ONLY,
	};

	struct SelfieFramingParams
	{
		float m_fLateralPan;					// Scale
		float m_fVerticalPan;					// Scale
		float m_fDistance;						// 0.0f -> 1.0f
		float m_fRoll;							// -1.0f -> 1.0f
		float m_fHeadYaw;						// -1.0f -> 1.0f
		float m_fHeadRoll;						// -1.0f -> 1.0f
		float m_fHeadPitch;						// -1.0f -> 1.0f

		float m_fHeadWeight;
		float m_fHeadBlendOutRate;

		float m_fLateralPanDistanceModifier;	// 0.0f -> 1.0f
		float m_fVerticalPanDistanceModifier;	// 0.0f -> 1.0f

		float m_afLateralPanLimits[2];
		float m_afModifiedLateralPanLimits[2];
		float m_afVerticalPanLimits[2];
		float m_afModifiedVerticalPanLimits[2];
		float m_afDistanceLimits[2];
		float m_afRollLimits[2];
		float m_afHeadYawLimits[2];
		float m_afHeadRollLimits[2];
		float m_afHeadPitchLimits[2];
	};

#if RSG_PC
	static void DeviceLost();
	static void DeviceReset();
#endif

	static void CreateRenderTargets();
	static void DestroyRenderTargets();

	static void Init(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);
	static void Update();
	static void Render(eDrawPhoneMode drawMode);
#if __BANK
	static void InitWidgets();
#endif // __BANK

	static bool UseAATechnique() { return BANK_ONLY(sm_bUseAA &&) (__XENON || __PS3 MSAA_ONLY(|| GRCDEVICE.GetMSAA())); }
	// Game
	static void Create(ePhoneType mobileType);
	static void Delete();
	static bool IsPhoneInstantiated()	{ return sm_modelReferenced; }

	static bool IsDisplayed();
	static void SetRemoveFlags(u32 flag);
	static u32 GetRemoveFlags();
	static void SetGoingOffScreen(bool val);
	static bool GetGoingOffScreen();

	// Target
	static grcRenderTarget* GetRenderTarget();
	
	// Render
	static void SetScale(float fScale);
	static float GetScale();
	static void SetRotation(const Vector3 &vRotation, EulerAngleOrder RotationOrder);
	static void GetRotation(Vector3 &vRotation, EulerAngleOrder &RotationOrder);
	static void SetPosition(float fPosX, float fPosY, float fPosZ);
	static Vector3 *GetPosition();

	static void SetBrightness(ePhoneType mobileType, float brightness);
	static void SetScreenBrightness(ePhoneType mobileType, float brightness);

	static float GetBrightness(ePhoneType mobileType);
	static float GetScreenBrightness(ePhoneType mobileType);

	// Horizontal Mode
	static void HorizontalActivate(bool bActivate);
	static bool GetHorizontalModeActive();
	static void ResetHorizontalModeFlag() { sm_enableHorizontalMode = false; }

	// Input 
	static void SetLatestPhoneInput(int phoneInput);
	static int GetLatestPhoneInput();

	// PhoneCam
	static void CamActivate(bool bEnablePhotoUpdate, bool bGoFirstPerson);
	static void CamActivateSelfieMode(bool bShouldActivate);
	static void CamActivateShallowDofMode(bool bShouldActivate);
	static void CamSetSelfieModeSideOffsetScaling(float scaling);

	static bool CamIsCharVisible(int ped, bool bFaceCheck = true);
	static grcRenderTarget* CamGetRT();
	
	static void CamSetZoom(float zoom);
	static float CamGetZoom();

	static void CamSetCentre(Vector2 &v);
	static const Vector2 &CamGetCentre();

	static void CamSetColourAndBrightness(float r, float g, float b, float bright);
	static const Vector4 &CamGetColourAndBrightness();
	
	static bool CamGetState();
	static bool CamGetSelfieModeState();
	static bool CamGetShallowDofModeState();
	static float CamGetSelfieModeSideOffsetScaling();
	static float CamGetSelfieModeVertOffsetScaling();
	static u32 CamGetSelfieModeActivationTime();

	static void ClearRemoveFlags(u32 flag);

	static grcBlendStateHandle GetDefaultBS() { return sm_defaultBS; }
	static grcBlendStateHandle GetRGBBS() { return sm_rgbBS; }
	static grcBlendStateHandle GetRGBNoBlend() { return sm_rgbNoBlendBS; }

	static float GetRadius() { return sm_radius; }

	static const CModelIndex& GetPhoneModelIndex();

	static void CamSetSelfieModeHorzPanOffset(float scaling);
	static void CamSetSelfieModeVertPanOffset(float scaling);
	static void CamSetSelfieRoll(float roll);
	static void CamSetSelfieDistance(float distance);
	static void CamSetSelfieHeadYaw(float yaw);
	static void CamSetSelfieHeadRoll(float roll);
	static void CamSetSelfieHeadPitch(float pitch);

	static float CamGetSelfieRoll();
	static float CamGetSelfieDistance();
	static float CamGetSelfieHeadYaw();
	static float CamGetSelfieHeadRoll();
	static float CamGetSelfieHeadPitch();

	const SelfieFramingParams& GetSelfieFramingParams() { return sm_SelfieFramingParams; }

	static void SetDOFState(bool b) {sm_enableDOF = b;}
	static bool GetDOFState() { return sm_enableDOF; }

private:
	static void LoadModel();
	static void UpdateSelfieFramingParams();
	static void ResetSelfieFramingParams();
	static void UpdateSelfieHeadWeight();
	static void GetLateralPanLimits(float& fMinLimit, float& fMaxLimit);
	static void GetVerticalPanLimits(float& fMinLimit, float& fMaxLimit);

	// Game
	static bool sm_display;
	static u32 sm_removeFlags; // Is set by code so that script can put the phone away
	static bool sm_goingOffScreen;
	static bool sm_haveDisabledRadioControls;

	// Render
	static ePhoneType sm_currentType;
	static const CModelIndex sm_ModelInfos[MAX_MOBILE_TYPE];
	static bool sm_modelReferenced;

	static grcRenderTarget* sm_RTScreen;
	static s32 sm_RTScreenId;
	
	static float sm_scale;
	static Vector3 sm_angles;
	static EulerAngleOrder sm_EulerAngleOrder;
	static Vector3 sm_pos;
	static float sm_brightness[MAX_MOBILE_TYPE];
	static float sm_screenBrightness[MAX_MOBILE_TYPE];

	static float sm_radius;

	// PhoneCam
	static grcRenderTarget* sm_RTPhoto;

	static float sm_pedVisibilityMaxDist;

	static float sm_zoom;
	static Vector2 sm_centre;
	static Vector4 sm_colorBrightness;
	static bool sm_enablePhotoUpdate;
	static bool sm_enableSelfieMode;
	static bool sm_enableShallowDofMode;
	static float sm_cameraSelfieModeSideOffsetScaling;
	static bool sm_enableHorizontalMode;
	static int sm_latestPhoneInput;
	static u32 sm_selfieModeActivationTime;
	static bool sm_enableDOF;

#if RSG_PC
	static bool sm_Initialized;
#endif

	static SelfieFramingParams sm_SelfieFramingParams;
	static camDampedSpring sm_HeadYawSpring;
	static camDampedSpring sm_HeadRollSpring;
	static camDampedSpring sm_HeadPitchSpring;

	static grcBlendStateHandle sm_defaultBS;
	static grcBlendStateHandle sm_rgbBS;
	static grcBlendStateHandle sm_rgbNoBlendBS;

#if __BANK
	static void CamIsCharVisibleWidget();

	static void CreateCB();
	static void DestroyCB();

	static void ChangeBrightnessCB();
	static void ChangeScreenBrightnessCB();

	static void CamActivateCB();
	
	static bool sm_isPlayerVisible;
	static bool sm_goFirstPerson;
	static int	sm_phoneType;
	static const char *sm_ModelNames[MAX_MOBILE_TYPE];

	static float sm_debugBrightness;
	static float sm_debugScreenBrightness;

	public:

	static bool sm_bUseAA;
#endif 
};

inline bool CPhoneMgr::IsDisplayed()
{ 
	return sm_display; 
}

inline void CPhoneMgr::SetRemoveFlags(u32 flag)
{
	sm_removeFlags |= flag;
}

inline u32 CPhoneMgr::GetRemoveFlags()
{
	return (sm_removeFlags);
}

inline void CPhoneMgr::SetGoingOffScreen(bool val)
{
	sm_goingOffScreen = val;
}

inline bool CPhoneMgr::GetGoingOffScreen()
{
	return sm_goingOffScreen;
}

inline grcRenderTarget * CPhoneMgr::GetRenderTarget()
{
	return sm_RTScreen;
}

inline void CPhoneMgr::SetScale(float fScale)
{
	sm_scale = fScale;
}

inline float CPhoneMgr::GetScale()
{
	return sm_scale;
}

inline void CPhoneMgr::SetRotation(const Vector3 &vRotation, EulerAngleOrder RotationOrder)
{
	sm_angles = vRotation;
	sm_EulerAngleOrder = RotationOrder;
}

inline void CPhoneMgr::GetRotation(Vector3 &vRotation, EulerAngleOrder &RotationOrder)
{
	vRotation = sm_angles;
	RotationOrder = sm_EulerAngleOrder;
}

inline void CPhoneMgr::SetPosition(float fPosX, float fPosY, float fPosZ)
{
	sm_pos.x = fPosX; sm_pos.y = fPosY; sm_pos.z = fPosZ;
}

inline Vector3 *CPhoneMgr::GetPosition()
{
	return &sm_pos;
}

inline void CPhoneMgr::SetBrightness(ePhoneType mobileType, float brightness) 
{
	sm_brightness[(int)mobileType] = brightness;
}

inline void CPhoneMgr::SetScreenBrightness(ePhoneType mobileType, float brightness) 
{
	sm_screenBrightness[(int)mobileType] = brightness;
}

inline float CPhoneMgr::GetBrightness(ePhoneType mobileType)
{
	return sm_brightness[(int)mobileType];
}

inline float CPhoneMgr::GetScreenBrightness(ePhoneType mobileType)
{
	return sm_screenBrightness[(int)mobileType];
}

inline grcRenderTarget* CPhoneMgr::CamGetRT()
{
	return sm_RTPhoto;
}


inline void CPhoneMgr::CamSetZoom(float zoom)
{
	sm_zoom = zoom;
}

inline float CPhoneMgr::CamGetZoom()
{
	return sm_zoom;
}


inline void CPhoneMgr::CamSetCentre(Vector2 &v)
{
	sm_centre = v;
}

inline const Vector2 &CPhoneMgr::CamGetCentre()
{
	return sm_centre;
}


inline void CPhoneMgr::CamSetColourAndBrightness(float r, float g, float b, float brightness)
{
	sm_colorBrightness = Vector4(r,g,b,brightness);
}

inline const Vector4 &CPhoneMgr::CamGetColourAndBrightness()
{
	return sm_colorBrightness;
}


inline void CPhoneMgr::ClearRemoveFlags(u32 flag)
{
	sm_removeFlags &= (~flag);
}


inline bool CPhoneMgr::CamGetState()
{
	return sm_enablePhotoUpdate;
}

inline bool CPhoneMgr::CamGetSelfieModeState()
{
	return sm_enableSelfieMode;
}

inline bool CPhoneMgr::CamGetShallowDofModeState()
{
	return sm_enableShallowDofMode;
}

inline u32 CPhoneMgr::CamGetSelfieModeActivationTime()
{
	return sm_selfieModeActivationTime;
}
#endif

