/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : UIReticule.h
// PURPOSE : manages the Scaleform-based Reticule code (code taken from NewHud)
// AUTHOR  : James Chagaris
// STARTED : 1/23/2013
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef INC_UI_RETICULE_H_
#define INC_UI_RETICULE_H_

// rage
#include "atl/array.h"
#include "vector/vector2.h"
#include "grcore/monitor.h"

// game
#include "scene/RegdRefTypes.h"
#include "weapons/WeaponEnums.h"


// FORWARD DECLARATIONS
class CPed;
class CEntity;
class CWeapon;
class CWeaponInfo;
class camGameplayDirector;

namespace rage{

#if __BANK
class bkBank;
class bkGroup;
#endif
};

class CUIReticule
{
	enum eHudReticleModes
	{
		HUD_RETICLE_INVALID = -1,
		HUD_RETICLE_DEFAULT = 0,
		HUD_RETICLE_CAN_BE_STUNNED, // start of script
		MAX_SCRIPT_RETICLE_MODES,  // end of script
		HUD_RETICLE_CANNOT_FIRE,  // start of code
		MAX_HUD_RETICLE_MODES  // end of code
	};

	// Needs to match the values passed into SET_RETICLE_STYLE
	enum eRETICLE_STYLE
	{
		// White = 0, Red = 1, White + 50% alpha = 2, Grey + 50% alpha = 3
		RETICLE_STYLE_INVALID = -1,
		RETICLE_STYLE_DEFAULT = 0,
		RETICLE_STYLE_ENEMY = 1,
		RETICLE_STYLE_FRIENDLY = 2,
		RETICLE_STYLE_DEAD = 3,
		RETICLE_STYLE_BUDDY = 4,
		RETICLE_STYLE_NON_LETHAL = 5
	};

	enum eReticleLockOnStyle
	{
		LOCK_ON_INVALID = -1,
		LOCK_ON_NOT_LOCKED_ON,
		LOCK_ON_ACQUIRING_LOCK_ON,
		LOCK_ON_LOCKED_ON,
		LOCK_ON_NO_TARGET,
	};

	enum eReticleDisplay
	{
		DISPLAY_INVALID = -1,
		DISPLAY_HIDDEN,
		DISPLAY_VISIBLE,
	};

	enum eReticleHitMarkerStyle
	{
		HITMARKER_HIT = 0,
		HITMARKER_DEAD,
		HITMARKER_KILLED_BY_LOCAL_PLAYER,
	};

	enum eReticleHitMarkerNonLethalStyle
	{
		HITMARKER_HIT_NON_LETHAL = 0,
		HITMARKER_INCAPACITATED,
		HITMARKER_INCAPACITATED_BY_LOCAL_PLAYER,
	};

	struct sPreviousFrameHudValues
	{
		RegdConstEnt m_target;
		Vector2 m_vReticlePosition;
		eRETICLE_STYLE m_ReticleStyle;
		eReticleLockOnStyle m_ReticleLockOnStyle;
		eHudReticleModes m_iReticleMode;
		float m_accuracyScaler;
		atHashWithStringNotFinal m_uPrevHumanNameHash;
		atHashWithStringNotFinal m_uPrevHumanNameSuffixHash;
		u32 m_iWeaponHashForReticule;
		s32 m_iReticleZoom;
		s32 m_iHeading;

		sPreviousFrameHudValues();
		void Reset();
		void ResetDisplay();

		bool IsDisplaying() const {return m_eDisplayReticle == DISPLAY_VISIBLE;}


		friend CUIReticule;
		private:
			eReticleDisplay m_eDisplayReticle;

	};

	struct sZoomState
	{
		s32 m_iZoomLevel;
		bool m_bIsFirstPersonAimingThroughScope;
		bool m_bIsZoomed;

		sZoomState() {Reset();}
		void Reset();
	};

public:
	CUIReticule();

	void Update(bool forceShow, bool isHudHidden);
	void Reset();

	sPreviousFrameHudValues& GetPreviousValues() {return PreviousHudValue;}
	bool ShouldHideReticule(const CPed* pPlayerPed);
	bool IsInGameplay();
	bool IsInMovie();

	void SetOverrideThisFrame(u32 overrideHash) {m_overrideHashThisFrame = overrideHash;}
	void RegisterPedKill(const CPed& inflictor, const CPed& victim, u32 weaponHash);
	void RegisterPedIncapacitated(const CPed& inflictor, const CPed& victim, u32 weaponHash);
	void RegisterVehicleHit(const CPed& inflictor, u32 uVehicleHash, float iHealthLost);
	void SetWeaponTimer(float fTimerPercent);

	void AddValidVehicleHitHash(u32 uHash) { m_ValidVehicleHitHashes.PushAndGrow(uHash); }
	void ClearValidVehicleHitHashes() { m_ValidVehicleHitHashes.Reset(); }

	static void TransformReticulePosition(const Vector3& vReticlePositionWorld, Vector2& vReticlePosition, bool& bDisplayReticle);

	BANK_ONLY(void CreateBankWidgets(bkBank *bank);)

private:

	eRETICLE_STYLE GetReticuleStyle(CPed* pPlayerPed, const CWeaponInfo *pWeaponInfo);
	eRETICLE_STYLE GetReticuleStyleForCNCDisplayMode(CPed* pPlayerPed, const CWeaponInfo *pWeaponInfo);
	float GetInvAccuracy(const CPed* pPlayerPed, const CWeapon* pPlayerEquippedWeapon);
	sZoomState GetZoomState(const CPed *pPlayerPed, const CWeapon* pPlayerEquippedWeapon, const camGameplayDirector& gameplayDirector);

	const CEntity* GetTargetEntity(const CPed* pPlayerPed, bool bCheckVehicles = false);
	bool IsSphereVisible(const Vector3& vReticlePositionWorld, float fPixelThresholdOverride = -1.0f);

	void ShowHitMarker(eReticleHitMarkerStyle markerStyle);
	void ShowHitMarkerNonLethal(eReticleHitMarkerNonLethalStyle markerStyle);

#if SUPPORT_MULTI_MONITOR
	void DrawBlinders(bool bSniperActive);
#endif // SUPPORT_MULTI_MONITOR

	sPreviousFrameHudValues PreviousHudValue;
	Vector2 m_vReticuleBlendOutPos;
	u32 m_overrideHashThisFrame;
	int m_queryId;// For point occlusion tests
	bool m_bReticuleBlendOut;
	bool m_queryUsedThisFrame;
	bool m_killTargetThisFrame;
	bool m_incapacitatedTargetThisFrame;
	
	bool m_bHitVehicleThisFrame;
	atArray<u32> m_ValidVehicleHitHashes;

	static bank_bool sm_bHideReticleWithLaserSight;
	static bank_u16 sm_uMaxInvAccuracy;
	static bank_float sm_fDistanceToNoTarget;
	static bank_float sm_fAccuracyScaler;
	static bank_float sm_fDrawSphereRadius;
	static bank_float sm_fDrawSpherePixelLimit;

	static bank_float sm_fVehicleReticuleDisplayTime;
	static bank_float sm_fVehicleReticuleFadeOutTime;
	float m_fTimeDisplayed;

	Vector2 m_vOnFootLockOnReticulePosition;
	bool m_bOnFootReticuleLockedOn;

#if __BANK
	bool m_bForceHideReticle;
	s32 m_debugOverrideHashIndex;
	atArray<u32> m_weaponHashes;
	atArray<const char*> m_weaponNames;
#endif
};

#endif  // INC_UI_RETICULE_H_

// eof

