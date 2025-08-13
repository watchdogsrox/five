/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : UIReticule.cpp
// PURPOSE : manages the new Scaleform-based HUD code for the reticule (code taken from NewHud)
// AUTHOR  : James Chagaris
// STARTED : 1/23/2013
//
/////////////////////////////////////////////////////////////////////////////////

#include "Frontend/UIReticule.h"

#include "input/headtracking.h"

#include "Camera/Base/BaseCamera.h"
#include "Camera/CamInterface.h"
#include "Camera/Cinematic/CinematicDirector.h"
#include "Camera/replay/ReplayDirector.h"
#include "Camera/Gameplay/Aim/FirstPersonPedAimCamera.h"
#include "Camera/Gameplay/Aim/FirstPersonShooterCamera.h"
#include "Camera/Gameplay/GameplayDirector.h"
#include "camera/cinematic/camera/mounted/CinematicMountedCamera.h"
#include "Camera/Helpers/ControlHelper.h"
#include "Camera/Viewports/ViewportManager.h"
#include "control/replay/ReplayExtensions.h"
#include "Frontend/NewHud.h"
#include "Frontend/ui_channel.h"
#include "frontend/MobilePhone.h"
#include "modelinfo/VehicleModelInfoFlags.h"
#include "Network/Network.h"
#include "Peds/Ped.h"
#include "peds/PedHelmetComponent.h"
#include "Peds/PedIntelligence.h"
#include "peds/PlayerInfo.h"
#include "Peds/QueriableInterface.h"
#include "renderer/OcclusionQueries.h"
#include "scene/playerswitch/PlayerSwitchInterface.h"
#include "Scene/World/GameWorld.h"
#include "Script/Commands_graphics.h"
#include "Script/Script_hud.h"
#include "System/Control.h"
#include "System/ControlMgr.h"
#include "Task/General/Phone/TaskMobilePhone.h"
#include "Task/Vehicle/TaskVehicleWeapon.h"
#include "task/Weapons/Gun/TaskVehicleDriveBy.h"
#include "task/Vehicle/TaskMountAnimalWeapon.h"
#include "Weapons/Inventory/PedWeaponManager.h"
#include "Vehicles/VehicleGadgets.h"
#include "core/watermark.h"

//OPTIMISATIONS_OFF()
FRONTEND_OPTIMISATIONS()

#define FAKED_WEAPON_HASH_FOR_SCOPED_RETICLE_MAX ATSTRINGHASH("SNIPER_MAX",0xd3db07a8)
#define FAKED_WEAPON_HASH_FOR_SCOPED_RETICLE ATSTRINGHASH("SNIPER_LARGE",0xc399f251)
#define FAKED_WEAPON_HASH_FOR_SIMPLE_RETICLE ATSTRINGHASH("SIMPLE_RETICLE",0x5619404)
#define FAKED_WEAPON_HASH_FOR_SIMPLE_MP_RETICLE ATSTRINGHASH("SIMPLE_MP_RETICLE",0x89c1ec2)

#define INVALID_ZOOM_LEVEL (-1)
#define INVALID_RETICULE_QUERY (0)

bank_bool CUIReticule::sm_bHideReticleWithLaserSight = false;
bank_float CUIReticule::sm_fDistanceToNoTarget = 25.0f;
bank_float CUIReticule::sm_fAccuracyScaler = 100.0f;
bank_u16 CUIReticule::sm_uMaxInvAccuracy = 100;
bank_float CUIReticule::sm_fDrawSphereRadius = 1.7f;
bank_float CUIReticule::sm_fDrawSpherePixelLimit = 10.0f;
bank_float CUIReticule::sm_fVehicleReticuleDisplayTime = 3.0f;
bank_float CUIReticule::sm_fVehicleReticuleFadeOutTime = 1.0f;

/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////
CUIReticule::sPreviousFrameHudValues::sPreviousFrameHudValues():
m_eDisplayReticle(DISPLAY_INVALID)
{
	Reset();
}

void CUIReticule::sPreviousFrameHudValues::Reset()
{
	// Setting to an invalid location, to force the scaleform function to be called the next time it's checked.
	m_target.Reset(NULL);
	m_vReticlePosition = Vector2(-10.0f, -10.0f);
	m_ReticleStyle = RETICLE_STYLE_INVALID;
	m_ReticleLockOnStyle = LOCK_ON_NOT_LOCKED_ON;
	m_iReticleMode = HUD_RETICLE_INVALID;
	m_iWeaponHashForReticule = 0;
	m_accuracyScaler = -1.0f;
	m_iReticleZoom = INVALID_ZOOM_LEVEL;
	m_iHeading = -1;

	ResetDisplay();
}

void CUIReticule::sPreviousFrameHudValues::ResetDisplay()
{
	if(m_eDisplayReticle == DISPLAY_VISIBLE)
	{
		if (CHudTools::BeginHudScaleformMethod(NEW_HUD_RETICLE, "SET_RETICLE_VISIBLE"))
		{
			CScaleformMgr::AddParamBool(false);
			CScaleformMgr::EndMethod();
		}
	}

	m_eDisplayReticle = DISPLAY_INVALID;
}

/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////
void CUIReticule::sZoomState::Reset()
{
	m_iZoomLevel = INVALID_ZOOM_LEVEL;
	m_bIsFirstPersonAimingThroughScope = false;
	m_bIsZoomed = false;
}

/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////
CUIReticule::CUIReticule()
: m_queryId(INVALID_RETICULE_QUERY)
#if __BANK
, m_bForceHideReticle(false)
, m_debugOverrideHashIndex(0)
, m_fTimeDisplayed(0.0f)
, m_bOnFootReticuleLockedOn(false)
#endif
{
	m_vOnFootLockOnReticulePosition = Vector2(0.5f, 0.5f);
	Reset();
}

void CUIReticule::Reset()
{
	m_vReticuleBlendOutPos = Vector2(0.0f, 0.0f);
	m_bReticuleBlendOut = false;
	m_overrideHashThisFrame = 0;
	m_queryUsedThisFrame = false;
	m_killTargetThisFrame = false;
	m_incapacitatedTargetThisFrame = false;
	m_bHitVehicleThisFrame = false;

	if(m_queryId != INVALID_RETICULE_QUERY)
	{
		OcclusionQueries::OQFree(m_queryId);
		m_queryId = INVALID_RETICULE_QUERY;
	}

	PreviousHudValue.Reset();
	m_ValidVehicleHitHashes.Reset();
}

bool CUIReticule::ShouldHideReticule(const CPed* pPlayerPed)
{
	bool bDisplayReticle = true;

	if (CNewHud::IsFullHudHidden() && gVpMan.AreWidescreenBordersActive())   // don't show reticule if widescreen borders are active
	{
		bDisplayReticle = false;
	}
	else if(pPlayerPed)
	{
		const CQueriableInterface* pInterface = pPlayerPed->GetPedIntelligence() ? pPlayerPed->GetPedIntelligence()->GetQueriableInterface() : NULL;

		// dont display retitcle if player is entering or exiting the vehicle (only when he is actually sitting in it)
		if (pInterface &&
			(pInterface->IsTaskCurrentlyRunning(CTaskTypes::TASK_ENTER_VEHICLE) || 
			 pInterface->IsTaskCurrentlyRunning(CTaskTypes::TASK_EXIT_VEHICLE) ||
			 pInterface->IsTaskCurrentlyRunning(CTaskTypes::TASK_MELEE)))
		{
			bDisplayReticle = false;
		}

		const CWeaponInfo* pWeaponInfo = pPlayerPed->GetEquippedWeaponInfo();
		if(pWeaponInfo && pWeaponInfo->GetHideReticule())
		{
			bDisplayReticle = false;
		}
	}

	return !bDisplayReticle;
}

bool CUIReticule::IsInGameplay()
{
	const camBaseCamera* dominantRenderedCamera	= camInterface::GetDominantRenderedCamera();
	const camGameplayDirector& gameplayDirector	= camInterface::GetGameplayDirector();
	const camBaseCamera* renderedGameplayCamera	= gameplayDirector.GetRenderedCamera();
	const bool isActuallyRenderingGameplay		= dominantRenderedCamera && renderedGameplayCamera &&
													((dominantRenderedCamera == renderedGameplayCamera) ||
													renderedGameplayCamera->IsInterpolating(dominantRenderedCamera));
	bool firstPersonMountedCam = false;
	const camBaseCamera* pDominantRenderedCamera = camInterface::GetDominantRenderedCamera();
	if(pDominantRenderedCamera && camInterface::GetDominantRenderedCamera()->GetIsClassId(camCinematicMountedCamera::GetStaticClassId()))
	{
		// Return true for pov AND bonnet cameras. (train mounted cameras are blocked as gameplay camera is marked as invalid for reticle)
		firstPersonMountedCam = true;
	}

	const bool shouldRenderReticuleForGameplay	= (isActuallyRenderingGameplay || firstPersonMountedCam) && gameplayDirector.ShouldDisplayReticule();

	return shouldRenderReticuleForGameplay;
}

bool CUIReticule::IsInMovie()
{
	// don't display reticle if a cinematic camera is rendering and does not require one
	const camCinematicDirector& cinematicDirector		= camInterface::GetCinematicDirector();
	const camBaseCamera* renderedCinematicCamera		= cinematicDirector.GetRenderedCamera();
	const bool isActuallyRenderingCinematicCamera		= renderedCinematicCamera && camInterface::IsDominantRenderedCamera(*renderedCinematicCamera);
	const bool shouldBlockReticuleForCinematicCamera	= isActuallyRenderingCinematicCamera && !cinematicDirector.ShouldDisplayReticule();
	
	return shouldBlockReticuleForCinematicCamera;
}

/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////
void CUIReticule::Update(bool forceShow, bool isHudHidden)
{
	if(CPauseMenu::IsActive() REPLAY_ONLY(&& !CReplayMgr::IsEditModeActive()))
	{
		m_killTargetThisFrame = false;
		m_incapacitatedTargetThisFrame = false;
		m_bHitVehicleThisFrame = false;

		//
		// deal with hiding the reticle during the SP->MP or MP->SP transitions: (fixes 1618455 without script changes)
		//
		eReticleDisplay eDisplayReticle = g_PlayerSwitch.IsActive() ? DISPLAY_HIDDEN : PreviousHudValue.m_eDisplayReticle;

		if (eDisplayReticle != PreviousHudValue.m_eDisplayReticle)
		{
			if (CHudTools::BeginHudScaleformMethod(NEW_HUD_RETICLE, "SET_RETICLE_VISIBLE"))
			{
				CScaleformMgr::AddParamBool(eDisplayReticle == DISPLAY_VISIBLE);
				CScaleformMgr::EndMethod();
			}

			PreviousHudValue.m_eDisplayReticle = eDisplayReticle;
		}
		return;
	}

#define LAZER_COCKPIT_ROCKETS (u32)-199376390
#define LAZER_COCKPIT_MACHINE 1931187857
#define ROCKET_BARRAGE_HASH 144449218
#define LAZER_ROCKET_HASH 3487949222
#define LAZER_MACHINE_GUN_HASH 4261553085

	CPed *pPlayerPed = CGameWorld::FindLocalPlayer();

	if (!pPlayerPed)
		return;

	const CPedWeaponManager* pPlayerWeaponManager = pPlayerPed->GetWeaponManager();
	if (!pPlayerWeaponManager)
		return;

#if GTA_REPLAY
	// if this is a replay, we want to skip out and do a fake update that just triggers stuff on demand
	if(CReplayMgr::IsEditModeActive() && ReplayReticuleExtension::HasExtension(pPlayerPed))
	{
		bool isSniperScopeVisible = false;
		bool justMadeVisible = false;

		u32 iWeaponHash = ReplayReticuleExtension::GetWeaponHash(pPlayerPed);
		if (iWeaponHash == FAKED_WEAPON_HASH_FOR_SCOPED_RETICLE || iWeaponHash == FAKED_WEAPON_HASH_FOR_SCOPED_RETICLE_MAX || iWeaponHash == FAKED_WEAPON_HASH_FOR_SIMPLE_RETICLE || iWeaponHash == FAKED_WEAPON_HASH_FOR_SIMPLE_MP_RETICLE )
		{
			bool bFirstPerson = ReplayReticuleExtension::GetIsFirstPerson(pPlayerPed);
			if (bFirstPerson)
			{
				CNewHud::SetHudComponentToBeShown(NEW_HUD_RETICLE);

				isSniperScopeVisible = true;
			}
		}

		eReticleDisplay eDisplayReticle = (isSniperScopeVisible && CReplayMgr::IsUsingRecordedCamera()) ? DISPLAY_VISIBLE : DISPLAY_HIDDEN;
		if(PreviousHudValue.m_eDisplayReticle != eDisplayReticle)
		{
            //Adding a check to delay the sniper scope ui activation/deactivation until the replay has finished streaming all the entities.
            //Also avoid to do this if we are fine scrubbing because the IsReplayPlayBackSetupFinished is always return false in that case.
            if( (camInterface::GetReplayDirector().IsReplayPlaybackSetupFinished() || CReplayMgr::IsFineScrubbing()) )
            {
                if (CHudTools::BeginHudScaleformMethod(NEW_HUD_RETICLE, "SET_RETICLE_VISIBLE"))
                {
                    CScaleformMgr::AddParamBool(isSniperScopeVisible);
                    CScaleformMgr::EndMethod();
                }

                if (isSniperScopeVisible)
                    justMadeVisible = true;
            }
            else
            {
                eDisplayReticle = PreviousHudValue.m_eDisplayReticle;
            }
		}

		if (PreviousHudValue.m_iWeaponHashForReticule != iWeaponHash)
		{
			if (CHudTools::BeginHudScaleformMethod(NEW_HUD_RETICLE, "SET_USING_REMOTE_PLAY"))
			{
				CScaleformMgr::AddParamBool(CControlMgr::GetPlayerMappingControl().IsUsingRemotePlay());  // for 1847201
				CScaleformMgr::EndMethod();
			}

			// Always force the reticule type upon making reticule visible, otherwise we get visibility problems.
			if (CHudTools::BeginHudScaleformMethod(NEW_HUD_RETICLE, "SET_RETICLE_TYPE"))
			{
				CScaleformMgr::AddParamInt(iWeaponHash);
				CScaleformMgr::EndMethod();
			}
		}

		s32 zoomLevel = static_cast<s32>(ReplayReticuleExtension::GetZoomLevel(pPlayerPed));
		if (zoomLevel != PreviousHudValue.m_iReticleZoom || justMadeVisible)
		{
			if(isSniperScopeVisible)
			{
				if (CHudTools::BeginHudScaleformMethod(NEW_HUD_RETICLE, "SET_RETICLE_ZOOM"))
				{
					CScaleformMgr::AddParamInt(zoomLevel);
					CScaleformMgr::EndMethod();
				}
			}

			PreviousHudValue.m_iReticleZoom = zoomLevel;
		}

#if SUPPORT_MULTI_MONITOR
		DrawBlinders(isSniperScopeVisible && eDisplayReticle == DISPLAY_VISIBLE);
#endif // SUPPORT_MULTI_MONITOR


		PreviousHudValue.m_iReticleMode = HUD_RETICLE_INVALID;
	//	PreviousHudValue.m_ReticleLockOnStyle = ReticleLockOnStyle;
		PreviousHudValue.m_iWeaponHashForReticule = iWeaponHash;
		PreviousHudValue.m_eDisplayReticle = eDisplayReticle;

		// for now, just disable the reticule component when not in sniper
		// it's all we need it for ...for now.
		if (!isSniperScopeVisible)
		{
			CNewHud::SetHudComponentToBeHidden(NEW_HUD_RETICLE);
		}

		return;
	}
#endif // GTA_REPLAY

	const CWeapon* pPlayerEquippedWeapon = pPlayerWeaponManager->GetEquippedWeapon();
	const camGameplayDirector& gameplayDirector	= camInterface::GetGameplayDirector();

	// find out if we need to display the reticle:
	// NOTE: We support forcing/simulating aiming (ie via a script command)
	bool bDisplayReticle = forceShow || IsInGameplay();
	eReticleLockOnStyle ReticleLockOnStyle = LOCK_ON_INVALID;

	//
	// get the current weapon:
	//
	u32 iVehicleWeaponHash = 0;
	u32 iCurrentWeaponHash = 0;
	

	CVehicle *pVehicle = pPlayerPed->GetVehiclePedInside();
	const CPed* pMount = pPlayerPed->GetMyMount();
	const CVehicleWeapon* pEquippedVehicleWeapon = pPlayerWeaponManager->GetEquippedVehicleWeapon();
	const CWeaponInfo *pWeaponInfo  = NULL;

	m_queryUsedThisFrame = false;
	
	// B*2077359: Enable reticule in aircraft if in FPS mode and have the pilot helmet equipped and using the mobile phone
	bool bUsingPhoneInAircraftInFPSMode = CTaskMobilePhone::IsRunningMobilePhoneTask(*pPlayerPed) && pPlayerPed->GetHelmetComponent() && pPlayerPed->GetHelmetComponent()->HasPilotHelmetEquippedInAircraftInFPS(true);

	if (pEquippedVehicleWeapon)
	{
		pWeaponInfo = pEquippedVehicleWeapon->GetWeaponInfo();
		iVehicleWeaponHash = pEquippedVehicleWeapon->GetHash();
		bool bHasVehiclePOVIronSight = MI_PLANE_STARLING.IsValid() && pVehicle && pVehicle->GetModelIndex() == MI_PLANE_STARLING;

		if(pWeaponInfo && pWeaponInfo->GetDamageType() != DAMAGE_TYPE_NONE) // Don't put up the reticule if it can't hurt anything.
		{
			bDisplayReticle = !IsInMovie() || (camInterface::GetCinematicDirector().IsRenderingCinematicMountedCamera() && !bHasVehiclePOVIronSight);
		}
	}
#if FPS_MODE_SUPPORTED
	// B*2077359: Enable reticule in aircraft if in FPS mode and have the pilot helmet equipped and using the mobile phone
	else if(bUsingPhoneInAircraftInFPSMode && !pEquippedVehicleWeapon && pVehicle->GetVehicleWeaponMgr())
	{
		// Use the reticule style of the previously equipped vehicle weapon (as the phone is the currently equipped weapon)
		// This is cached in the phone task just before we equip the phone object.
		CTaskMobilePhone *pTaskMobilePhone = static_cast<CTaskMobilePhone*>(pPlayerPed->GetPedIntelligence()->FindTaskSecondaryByType(CTaskTypes::TASK_MOBILE_PHONE));
		if (pTaskMobilePhone && pTaskMobilePhone->GetPreviousVehicleWeaponIndex() != -1)
		{
			pEquippedVehicleWeapon = pVehicle->GetVehicleWeaponMgr()->GetVehicleWeapon(pTaskMobilePhone->GetPreviousVehicleWeaponIndex());
			if (pEquippedVehicleWeapon)
			{
				pWeaponInfo = pEquippedVehicleWeapon->GetWeaponInfo();
				iVehicleWeaponHash = pEquippedVehicleWeapon->GetHash();
				if(pWeaponInfo && pWeaponInfo->GetDamageType() != DAMAGE_TYPE_NONE) // Don't put up the reticule if it can't hurt anything.
				{
					bDisplayReticle = !IsInMovie() || camInterface::GetCinematicDirector().IsRenderingCinematicPointOfViewCamera();
				}
			}
		}
	}
#endif	//FPS_MODE_SUPPORTED

	//B*1833484: Fade out crosshair after timer
	// only doing this for spycar machinegun for now
	bool bFadeOutVehicleWeaponReticuleAfterTimer = pVehicle && pVehicle->InheritsFromSubmarineCar() && pEquippedVehicleWeapon && iVehicleWeaponHash == ATSTRINGHASH("VEHICLE_WEAPON_SPYCARGUN", 0xa9ee94f7);
	bool bFadeOutVehReticule = false;
	int	iDimValue = 100;
#if FPS_MODE_SUPPORTED
	TUNE_GROUP_BOOL(AIMING_TUNE, FORCE_FPS_SCOPE_RETICLE, false);
#endif // FPS_MODE_SUPPORTED

	bool isFirstPersonAiming = false;
	bool bHideReticuleInFPSMode = false;
	if ((!pEquippedVehicleWeapon) || iVehicleWeaponHash == 0)
	{
		// Use the weapon info provided by the camera system, as this should keep the reticle synchronized with any camera transitions
		pWeaponInfo = camGameplayDirector::ComputeWeaponInfoForPed(*pPlayerPed);
		if(pWeaponInfo)
		{
			iCurrentWeaponHash = pWeaponInfo->GetHash();

			if(pPlayerPed->GetWeaponManager() && pPlayerPed->GetWeaponManager()->GetEquippedWeaponHasFirstPersonScope())
			{
				//If the gameplay director isn't rendering a first-person aim camera, fall-back to using a third-person reticule.
				if(gameplayDirector.IsFirstPersonAiming())
				{
					isFirstPersonAiming = true;
				}
				else
				{
					iCurrentWeaponHash = WEAPONTYPE_ASSAULTRIFLE;
				}
			}
			if (rage::ioHeadTracking::UseFPSCamera())
			{
				isFirstPersonAiming = true;
				bDisplayReticle = true;
			}
		#if FPS_MODE_SUPPORTED
			const camFirstPersonShooterCamera* fpsCamera = camInterface::GetGameplayDirector().GetFirstPersonShooterCamera();
			if( fpsCamera &&
				camInterface::GetDominantRenderedCamera() == (camBaseCamera*)fpsCamera &&
				((pWeaponInfo->GetIsArmed() && !pWeaponInfo->GetIsMeleeFist()) || pPlayerPed->GetPedResetFlag(CPED_RESET_FLAG_ThrowingProjectile)) )
			{
				if ( fpsCamera->ShowReticle() || FORCE_FPS_SCOPE_RETICLE )
				{
					bDisplayReticle = true;
				}
				else
				{
					bDisplayReticle = false;
					bHideReticuleInFPSMode = true;
				}
			}
		#endif // FPS_MODE_SUPPORTED

			if(pWeaponInfo->GetReticuleStyleHash())
			{
				iCurrentWeaponHash = pWeaponInfo->GetReticuleStyleHash();
			}
		}
		else
		{
			iCurrentWeaponHash = pPlayerPed->GetDefaultUnarmedWeaponHash();
		}

		// The unarmed weapon shouldn't have a reticule when in a vehicle, B* 988479
		if((pVehicle || pMount) && iCurrentWeaponHash == pPlayerPed->GetDefaultUnarmedWeaponHash())
		{
			bDisplayReticle = false;
		}

		if(sm_bHideReticleWithLaserSight)
		{
			const CPedWeaponManager *pWeaponManager = pPlayerPed->GetWeaponManager();
			if(pWeaponManager)
			{
				const CWeapon *pPlayersWeapon = pWeaponManager->GetEquippedWeapon();
				if(pPlayersWeapon)
				{
					if (pPlayersWeapon->GetLaserSightComponent())
					{
						bDisplayReticle = false;
					}
				}
			}
		}
	}
	else if(pEquippedVehicleWeapon && pEquippedVehicleWeapon->GetWeaponInfo() && pEquippedVehicleWeapon->GetWeaponInfo()->GetReticuleStyleHash())
	{
		iCurrentWeaponHash = pEquippedVehicleWeapon->GetWeaponInfo()->GetReticuleStyleHash();

	#if FPS_MODE_SUPPORTED
		if(!FORCE_FPS_SCOPE_RETICLE && camInterface::GetDominantRenderedCamera())
		{
			if( camInterface::GetDominantRenderedCamera()->GetIsClassId(camCinematicMountedCamera::GetStaticClassId()) )
			{
				const camCinematicMountedCamera* pMountedCamera = (const camCinematicMountedCamera*)camInterface::GetDominantRenderedCamera();
				if(pMountedCamera->IsVehicleTurretCamera())
				{
					bDisplayReticle = pMountedCamera->ShouldDisplayReticule();
				}
			}
			else if( camInterface::GetDominantRenderedCamera()->GetIsClassId(camFirstPersonShooterCamera::GetStaticClassId()) )
			{
				const camFirstPersonShooterCamera* pFpsCamera = (const camFirstPersonShooterCamera*)camInterface::GetDominantRenderedCamera();
				if(pFpsCamera->IsEnteringTurretSeat())
				{
					bDisplayReticle = pFpsCamera->ShowReticle();
				}
			}
		}
	#endif // FPS_MODE_SUPPORTED
	}
	else
	{
		iCurrentWeaponHash = iVehicleWeaponHash;
	}

	Vector2 vReticlePosition(0.5f, 0.5f);
	s32 iHeading = 0;
	atHashWithStringNotFinal humanNameHash;
	atHashWithStringNotFinal humanNameSuffixHash;

	// B*2148150: On-foot homing weapons: process lock-on reticule style and lerp to/from lock-on position.
	if(pWeaponInfo && pWeaponInfo->GetIsOnFootHoming() && !pPlayerPed->GetIsInVehicle())
	{	
		CPlayerPedTargeting& rTargeting = pPlayerPed->GetPlayerInfo()->GetTargeting();
		Vector3 vReticlePositionWorld;
		bool bVisible = false;
		bool bShiftReticulePosition = false;

		// B*2153703: Only show reticule when aiming and locked on in FPS mode
		bool bInFPSMode = pPlayerPed->IsFirstPersonShooterModeEnabledForPlayer(false);
		
		if (rTargeting.GetLockOnTarget())
		{
			CPlayerPedTargeting::OnFootHomingLockOnState lockOnState = rTargeting.GetOnFootHomingLockOnState(pPlayerPed);

			switch(lockOnState)
			{
			case(CPlayerPedTargeting::OFH_NOT_LOCKED):
				ReticleLockOnStyle = LOCK_ON_NOT_LOCKED_ON;
				break;
			case(CPlayerPedTargeting::OFH_ACQUIRING_LOCK_ON):
				ReticleLockOnStyle = LOCK_ON_ACQUIRING_LOCK_ON;
				break;
			case(CPlayerPedTargeting::OFH_LOCKED_ON):
				ReticleLockOnStyle = LOCK_ON_LOCKED_ON;
				break;
			default:
				ReticleLockOnStyle = LOCK_ON_NOT_LOCKED_ON;
				break;
			}

			// Set the reticle position to the position of the current target entity
			vReticlePositionWorld =	rTargeting.GetLockonTargetPos();
			bVisible = true;
			bShiftReticulePosition = true;

			// We usually hide the reticule in iron sights camera, but in this case we still want to show it so we know what we're locked on to.
			if (bHideReticuleInFPSMode && !bDisplayReticle)
			{
				bDisplayReticle = true;
			}
		}
		else
		{
			// Lerp back to the normal reticule position if aren't back already.
			if (!bInFPSMode && (m_vOnFootLockOnReticulePosition - vReticlePosition).Mag() > 0.01f)
			{
				static dev_float s_fReticuleApproachRateNotLocked = 0.33f;
				float fLerpSpeed = s_fReticuleApproachRateNotLocked;
				fLerpSpeed *= fwTimer::GetTimeStep() / (1.0f / 30.0f);

				m_vOnFootLockOnReticulePosition = Lerp(fLerpSpeed, m_vOnFootLockOnReticulePosition, vReticlePosition);
				vReticlePosition = m_vOnFootLockOnReticulePosition;
			}
			else
			{
				m_vOnFootLockOnReticulePosition = vReticlePosition;
			}
			m_bOnFootReticuleLockedOn = false;
			ReticleLockOnStyle = LOCK_ON_NO_TARGET;
			bVisible = true;
		}

		if (bVisible)
		{
			if( pWeaponInfo && pWeaponInfo->GetIsStaticReticulePosition())
			{
				bDisplayReticle = true;
				vReticlePosition = pWeaponInfo->GetReticuleHudPosition();
			}
			else if (bShiftReticulePosition)
			{
				TransformReticulePosition(vReticlePositionWorld, vReticlePosition, bDisplayReticle);

				static dev_float fReticuleAccuracyThreshold = 0.01f;
				if((m_vOnFootLockOnReticulePosition - vReticlePosition).Mag() < fReticuleAccuracyThreshold)
				{
					m_bOnFootReticuleLockedOn = true;
				}

				// If we aren't fully locked on, lerp the reticule towards the target position
				if (!bInFPSMode && ReticleLockOnStyle != LOCK_ON_LOCKED_ON && !m_bOnFootReticuleLockedOn)
				{
					static dev_float s_fReticuleApproachRateLocked = 0.33f;
					float fLerpSpeed = s_fReticuleApproachRateLocked;
					fLerpSpeed *= fwTimer::GetTimeStep() / (1.0f / 30.0f);

					m_vOnFootLockOnReticulePosition = Lerp(fLerpSpeed, m_vOnFootLockOnReticulePosition, vReticlePosition);
					vReticlePosition = m_vOnFootLockOnReticulePosition;
				}
				else
				{
					m_vOnFootLockOnReticulePosition = vReticlePosition;
				}
			}
		}
		else
		{
			bDisplayReticle = false;
		}
	}


	// B*1997859 - For vehicles with mounted turrets and static reticules we don't really want to check distances
	const bool bAvoidDistanceChecks = (pWeaponInfo && pWeaponInfo->GetIsTurret() && pWeaponInfo->GetIsStaticReticulePosition()) || (pVehicle && MI_CAR_APC.IsValid() && pVehicle->GetModelIndex() == MI_CAR_APC);
	if (!bAvoidDistanceChecks && pEquippedVehicleWeapon && iVehicleWeaponHash != 0 && bDisplayReticle && pVehicle && (!pVehicle->GetIsLandVehicle() || pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_USE_AIRCRAFT_STYLE_WEAPON_TARGETING)))
	{
		bool bVisible = false;
		Vector3 vReticlePositionWorld;

		bool bIsVehicleGadget = false;

		bool bHoverMode = pVehicle->GetHoverMode();
		for(int i = 0; i < pVehicle->GetNumberOfVehicleGadgets(); i++)
		{
			CVehicleGadget *pVehicleGadget = pVehicle->GetVehicleGadget(i);

			if(pVehicleGadget && pVehicleGadget->GetType() == VGT_VEHICLE_WEAPON_BATTERY)
			{
				CVehicleWeaponMgr* pVehWeaponMgr = pVehicle->GetVehicleWeaponMgr();

				if (pVehWeaponMgr)
				{
					const CVehicleWeapon* pVehicleWeapon  = pVehWeaponMgr->GetVehicleWeapon(iVehicleWeaponHash);
					if(pVehicleWeapon)
					{
						pVehicleWeapon->GetReticlePosition(vReticlePositionWorld);
						bVisible = IsSphereVisible(vReticlePositionWorld);
						bIsVehicleGadget = true;
					}
				}
			}
		}

		if (!bIsVehicleGadget)
		{
			if(pEquippedVehicleWeapon->GetType() == VGT_VEHICLE_WEAPON_BATTERY)
			{
				const CVehicleWeaponBattery* pWeaponBattery = static_cast<const CVehicleWeaponBattery*>(pEquippedVehicleWeapon);

				Vector3 vPositionTotal( Vector3::ZeroType );
				int nWeaponCount = 0;
				for(int i = 0; i < pWeaponBattery->GetNumWeaponsInBattery(); i++)
				{
					const CVehicleWeapon* pWeapon = pWeaponBattery->GetVehicleWeapon(i);

					if(uiVerify(pWeapon) && pWeapon->GetHash() == iVehicleWeaponHash)
					{
						if( pWeapon->GetReticlePosition( vReticlePositionWorld ) )
						{
							vPositionTotal += vReticlePositionWorld;
							nWeaponCount++;
						}
					}
				}

				if( nWeaponCount )
					vReticlePositionWorld = vPositionTotal / (f32)nWeaponCount;
			}
			else
			{
				pEquippedVehicleWeapon->GetReticlePosition(vReticlePositionWorld);
			}

			bVisible = IsSphereVisible(vReticlePositionWorld);
		}

		if(pWeaponInfo && pWeaponInfo->GetIsHoming())
		{	// The vehicle weapon is a homing weapon
			CPlayerPedTargeting& rTargeting = pPlayerPed->GetPlayerInfo()->GetTargeting();

			if (rTargeting.GetLockOnTarget() && rTargeting.GetVehicleHomingEnabled())
			{
				// Time needed before the target is considered locked on
				float fTimeBeforeHoming = 0.0f;
				const CAmmoInfo* pAmmoInfo = pWeaponInfo->GetAmmoInfo();
				if(pAmmoInfo && (pAmmoInfo->GetClassId() == CAmmoRocketInfo::GetStaticClassId()) )
				{
					fTimeBeforeHoming = static_cast<const CAmmoRocketInfo*>(pAmmoInfo)->GetTimeBeforeHoming();
				}

				float fTotalLockOnTime = rTargeting.GetTimeAimingAtTarget();
				if (fTotalLockOnTime < (fTimeBeforeHoming/2))
				{	//	Display the green reticle
					ReticleLockOnStyle = LOCK_ON_NOT_LOCKED_ON;
				}
				else if (fTotalLockOnTime < fTimeBeforeHoming)
				{	//	Display the flashing orange reticle
					ReticleLockOnStyle = LOCK_ON_ACQUIRING_LOCK_ON;
				}
				else
				{	//	Display the red reticle
					ReticleLockOnStyle = LOCK_ON_LOCKED_ON;
				}
				// Set the reticle position to the position of the current target entity
				vReticlePositionWorld =	rTargeting.GetLockonTargetPos();
				bVisible = IsSphereVisible(vReticlePositionWorld, 1.0f);

				CEntity *pTargetEntity = rTargeting.GetLockOnTarget();

				// B*1817732: If sphere isn't visible, do a probe test from the vehicle to the lock on position.
				// Fixes issues on certain entities where the sphere is too small to be seen (ie inside of the Mule or the Titan).
				if (!bVisible && pTargetEntity && pVehicle)
				{
					u32 includeFlags = 0;
					if (pTargetEntity->GetIsTypeVehicle())
					{
						includeFlags = ArchetypeFlags::GTA_VEHICLE_TYPE;
					}
					else if (pTargetEntity->GetIsTypeObject())
					{
						includeFlags = ArchetypeFlags::GTA_OBJECT_TYPE;
					}

					WorldProbe::CShapeTestHitPoint probeIsect;
					WorldProbe::CShapeTestResults probeResult(probeIsect);
					WorldProbe::CShapeTestProbeDesc probeDesc;
					Vector3 vVehiclePosition = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition());
					probeDesc.SetStartAndEnd(vVehiclePosition, vReticlePositionWorld);
					probeDesc.SetResultsStructure(&probeResult);
					probeDesc.SetExcludeEntity(pVehicle);
					probeDesc.SetIncludeFlags(includeFlags);
					probeDesc.SetContext(WorldProbe::LOS_GeneralAI);

					if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_SYNCHRONOUS_TEST))
					{
						for (int i = 0; i < probeResult.GetNumHits(); i++)
						{
							if (pTargetEntity == probeResult[i].GetHitEntity())
							{
								bVisible = true;
							}
						}
					}
				}

				// Set target lock-on state
				if( bVisible && pVehicle && pTargetEntity && pTargetEntity->GetIsPhysical())
				{
					CEntity::eHomingLockOnState lockOnState = CEntity::HLOnS_NONE;
					if( ReticleLockOnStyle == LOCK_ON_ACQUIRING_LOCK_ON )
						lockOnState = CEntity::HLOnS_ACQUIRING;
					else if( ReticleLockOnStyle == LOCK_ON_LOCKED_ON )
						lockOnState = CEntity::HLOnS_ACQUIRED;

					pVehicle->SetHomingLockOnState( lockOnState );
                    pVehicle->SetLockOnTarget(static_cast<CPhysical *>(pTargetEntity));
				}
			}
			else
			{
				if(!rTargeting.GetVehicleHomingEnabled())
				{
					humanNameSuffixHash = "WT_HOMING_DISABLED";
				}

				ReticleLockOnStyle = LOCK_ON_NO_TARGET;
				bVisible = true;
			}
		}
		else if (pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_USE_AIRCRAFT_STYLE_WEAPON_TARGETING))
		{
			// We're a non-homing vehicle weapon (not just 'homing but disabled') that's not a static reticule turret.. let's just render the reticule?
			// We don't care about the distance check, but skipping the 'IsHoming' chunk above only renders a static reticule, because we never hit TransformReticulePosition below.
			bVisible = true;
		}

		if(pWeaponInfo && camInterface::GetCinematicDirector().IsRenderingCinematicMountedCamera())
		{
			// If we're locked on, the text should read "Lock"
			if(ReticleLockOnStyle == LOCK_ON_LOCKED_ON)
			{
				humanNameHash = "WT_LOCK";
			}
			else
			{
				humanNameHash = pWeaponInfo->GetHumanNameHash();
			}

			isFirstPersonAiming = true;
			iCurrentWeaponHash = pWeaponInfo->GetFirstPersonReticuleStyleHash();
			iHeading =  (s32)(camInterface::GetHeading() * RtoD);
			while(iHeading < 0) {iHeading += 360;}
			while(360 <= iHeading) {iHeading -= 360;}
		}


		if (bFadeOutVehicleWeaponReticuleAfterTimer)
		{
			m_fTimeDisplayed += fwTimer::GetTimeStep();
			if (m_fTimeDisplayed > sm_fVehicleReticuleDisplayTime) 
			{
				bFadeOutVehReticule = true;

				// Calculate reticule dimness value
				float fTimeLeft = (sm_fVehicleReticuleDisplayTime + sm_fVehicleReticuleFadeOutTime) - m_fTimeDisplayed;
				iDimValue = (int)((fTimeLeft / sm_fVehicleReticuleFadeOutTime) * 100.0f);
				if (iDimValue < 0)
				{
					bFadeOutVehReticule = false;
					bDisplayReticle = false;
				}
			}
		}

		if (bVisible)
		{
			if( pWeaponInfo && ( bHoverMode || pWeaponInfo->GetIsStaticReticulePosition() ) )
			{
				bDisplayReticle = true;
				vReticlePosition = pWeaponInfo->GetReticuleHudPosition();
			}
			else
			{
				TransformReticulePosition(vReticlePositionWorld, vReticlePosition, bDisplayReticle);
			}
		}
		else
		{
			bDisplayReticle = false;
		}
	}

	bool isPassenger = false;
	bool isTank = false;

	if (pVehicle)
	{
		bool bReticuleLockedOn = false;

		isPassenger = !pPlayerPed->GetIsDrivingVehicle();
		isTank = pVehicle->IsTank();

		//! Don't test this flag. It's better to have the reticule lock on, even when smashing window etc (as you get a pop to centre and back if not).
		//if(pPlayerPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_ASSISTED_AIMING_ON))
		//{
			CPlayerPedTargeting& rTargeting = pPlayerPed->GetPlayerInfo()->GetTargeting();
			CEntity* pLockOnEntity = rTargeting.GetLockOnTarget();
			if (pLockOnEntity && pPlayerPed->GetIsDrivingVehicle())
			{
				//! DMKH. Get lock on position directly. The targeting one contains some lag, which isn't appropriate for reticule here.
				Vector3 vLockOnPos;
				pLockOnEntity->GetLockOnTargetAimAtPos(vLockOnPos);
				TransformReticulePosition(vLockOnPos, vReticlePosition, bDisplayReticle);
				
				//! DMKH. Start a blend out back to idle reticule pos (0.5, 0.5). Bug* 1007203.
				m_bReticuleBlendOut = true;
				m_vReticuleBlendOutPos = vReticlePosition;

				bReticuleLockedOn = true;
			}
		//}
		
		//! Blend reticule back to centre if we have indicated we would like to do so.
		if(!bReticuleLockedOn && m_bReticuleBlendOut)
		{
			static dev_float s_fReticuleApproachRate = 0.25f;
			float fLerpSpeed = s_fReticuleApproachRate;
			fLerpSpeed *= fwTimer::GetTimeStep() / (1.0f / 30.0f);

			Vector2 vReticuleBlendPos = Lerp(fLerpSpeed, m_vReticuleBlendOutPos, vReticlePosition);
			m_vReticuleBlendOutPos = vReticuleBlendPos;

			static dev_float s_fEpsilon = 0.00001f;
			if(vReticuleBlendPos.IsClose(vReticlePosition, s_fEpsilon))
			{
				m_bReticuleBlendOut = false;
			}

			vReticlePosition = vReticuleBlendPos;
		}
		
		if(bDisplayReticle)
		{
			if(pVehicle->GetIsAircraft())
			{
				if(CControlMgr::GetMainPlayerControl().IsInputDisabled(INPUT_VEH_FLY_ATTACK) && !bUsingPhoneInAircraftInFPSMode)
				{
					bDisplayReticle = false;
				}
			}
			else if(pVehicle->GetIsLandVehicle())
			{
				if(CControlMgr::GetMainPlayerControl().IsInputDisabled(INPUT_VEH_ATTACK) && CControlMgr::GetMainPlayerControl().IsInputDisabled(INPUT_VEH_ATTACK2))
				{
					bDisplayReticle = false;
				}
			}

			if (pVehicle->GetIsAircraft() || pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_USE_AIRCRAFT_STYLE_WEAPON_TARGETING))
			{
				if(pEquippedVehicleWeapon && iVehicleWeaponHash != 0)
				{
					bool bShouldDoAimDirectionTest = pWeaponInfo ? !pWeaponInfo->GetDisableAimAngleChecksForReticule() : true;

					if (bShouldDoAimDirectionTest)
					{
						// Do not render the reticule if we're not facing in the correct direction
						// Use the ped's forward vector, rather than the vehicles as they maybe facing sideways
						Vec3V aimFrontV = pPlayerPed->GetTransform().GetB();
						Vector3 aimFront(aimFrontV.GetXf(), aimFrontV.GetYf(), aimFrontV.GetZf());
						Vector3 camFront = camInterface::GetFront();

						aimFront /= aimFront.Mag();
						camFront /= camFront.Mag();

						if(aimFront.Dot(camFront) < 0.0f)
						{
							bDisplayReticle = false;
						}
					}
				}
			}
		}
	}
	else
	{
		m_bReticuleBlendOut = false;
	}

	if(pMount &&
		(CControlMgr::GetMainPlayerControl().IsInputDisabled(INPUT_VEH_ATTACK) && CControlMgr::GetMainPlayerControl().IsInputDisabled(INPUT_VEH_ATTACK2)))
	{
		bDisplayReticle = false;
	}

	if (isHudHidden BANK_ONLY(|| m_bForceHideReticle))
	{
		bDisplayReticle = false;
		m_bReticuleBlendOut = false;
	}

	// If the weapon is using a purchased scope, then we need to fake the reticle
	if (pPlayerEquippedWeapon && gameplayDirector.IsFirstPersonAiming())
	{
		// NEVER TRUE?
		//if(pPlayerEquippedWeapon->GetHasFirstPersonScope() &&
		//	!pPlayerEquippedWeapon->GetWeaponInfo()->GetHasFirstPersonScope())
		//{
		//	isFirstPersonAiming = true;
		//	iCurrentWeaponHash = FAKED_WEAPON_HASH_FOR_SCOPED_RETICLE;
		//}

		const CWeaponComponentScope* pScope = pPlayerEquippedWeapon->GetScopeComponent();
		if(pScope && pScope->GetInfo() && pPlayerEquippedWeapon->GetHasFirstPersonScope())
		{
			u32 iScopeReticule = pScope->GetInfo()->GetReticuleHash();
			if(iScopeReticule != 0)
			{
				iCurrentWeaponHash = iScopeReticule;
			}
		}
	}
	else if (gameplayDirector.IsFirstPersonAiming())
	{
		const CWeapon* pWeapon = pPlayerPed->GetWeaponManager() ? pPlayerPed->GetWeaponManager()->GetEquippedWeapon() : NULL;
		if (pWeapon && pWeapon->GetHasFirstPersonScope())
		{
			const CPedInventory* inventory	= pPlayerPed->GetInventory();
			const CWeaponItem* weaponItem = inventory ? inventory->GetWeapon(pWeapon->GetWeaponInfo()->GetHash()) : NULL;
			if (weaponItem && weaponItem->GetComponents())
			{
				const CWeaponItem::Components& components = *weaponItem->GetComponents();
				for(s32 i=0; i<components.GetCount(); i++)
				{
					const CWeaponComponentInfo* componentInfo = components[i].pComponentInfo;
					if(componentInfo && (componentInfo->GetClassId() == WCT_Scope))
					{
						u32 iScopeReticule = static_cast<const CWeaponComponentScopeInfo*>(componentInfo)->GetReticuleHash();
						if (iScopeReticule != 0)
							iCurrentWeaponHash = iScopeReticule;
						break;
					}
				}
			}
		}
	}

	if(!isFirstPersonAiming && !isTank && (ReticleLockOnStyle == LOCK_ON_INVALID) &&
		CPauseMenu::GetMenuPreference(PREF_RETICULE) == 0 && iCurrentWeaponHash != LAZER_ROCKET_HASH)
	{
		iCurrentWeaponHash = FAKED_WEAPON_HASH_FOR_SIMPLE_RETICLE;
	}

	if(CNetwork::IsNetworkOpen() && iCurrentWeaponHash == FAKED_WEAPON_HASH_FOR_SIMPLE_RETICLE)
	{
		iCurrentWeaponHash = FAKED_WEAPON_HASH_FOR_SIMPLE_MP_RETICLE;

#if RSG_PC
		if(CHudTools::IsSuperWideScreen() || GRCDEVICE.GetMonitorConfig().isMultihead())
			iCurrentWeaponHash = FAKED_WEAPON_HASH_FOR_SIMPLE_RETICLE;
#endif // RSG_PC
	}

#if __BANK
	if(m_debugOverrideHashIndex)
	{
		m_overrideHashThisFrame = m_weaponHashes[m_debugOverrideHashIndex];
	}
#endif

	if(m_overrideHashThisFrame)
	{
		iCurrentWeaponHash = m_overrideHashThisFrame;
		m_overrideHashThisFrame = 0;
	}

	if((!isFirstPersonAiming || pPlayerPed->GetIsDrivingVehicle()) && CNewHud::IsShowingHUDMenu())
	{
		if(pVehicle && pVehicle->GetIsAircraft())
		{
			if(CNewHud::IsRadioWheelActive() || CNewHud::IsShowingCharacterSwitch())
			{
				bDisplayReticle = false;
			}
		}
		else
		{
			bDisplayReticle = false;
		}
	}

	if(ShouldHideReticule(pPlayerPed))
	{
		bDisplayReticle = false;
	}

	if (pEquippedVehicleWeapon && !pEquippedVehicleWeapon->GetIsEnabled())
	{
		bDisplayReticle = false;
	}

	// only update if has changed
	bool bForceUpdatePosAndHeading = false;

	const bool c_IsUsingFirstPersonLazerWeapons = (iCurrentWeaponHash == LAZER_COCKPIT_ROCKETS || iCurrentWeaponHash == LAZER_COCKPIT_MACHINE) &&
				((pPlayerPed && pPlayerPed->IsInFirstPersonVehicleCamera()) || camInterface::GetCinematicDirector().IsRenderingCinematicMountedCamera());
	if( c_IsUsingFirstPersonLazerWeapons )
	{
		if(pPlayerPed->GetHelmetComponent() && pPlayerPed->GetHelmetComponent()->HasPilotHelmetEquippedInAircraftInFPS(false))
		{
		}
		else
		{
			switch (iCurrentWeaponHash)
			{
			case LAZER_COCKPIT_ROCKETS:
				iCurrentWeaponHash = LAZER_ROCKET_HASH; // VULCAN_ROCKET
				break;
			case LAZER_COCKPIT_MACHINE:
				iCurrentWeaponHash = LAZER_MACHINE_GUN_HASH; // WEAPON_VULKAN_GUNS
				break;
			default:
				iCurrentWeaponHash = 0;	
			}
		}
	}

	eReticleDisplay eDisplayReticle = bDisplayReticle ? DISPLAY_VISIBLE : DISPLAY_HIDDEN;
	if ( (eDisplayReticle != PreviousHudValue.m_eDisplayReticle)
		|| (iCurrentWeaponHash != PreviousHudValue.m_iWeaponHashForReticule)
		|| (ReticleLockOnStyle != PreviousHudValue.m_ReticleLockOnStyle)
		|| (humanNameHash != PreviousHudValue.m_uPrevHumanNameHash)
		|| (humanNameSuffixHash != PreviousHudValue.m_uPrevHumanNameSuffixHash))
	{
		// Only reset the display timer if the weapon has changed
		if (m_fTimeDisplayed != 0.0f && iCurrentWeaponHash != PreviousHudValue.m_iWeaponHashForReticule)
		{
			m_fTimeDisplayed = 0.0f;
		}

		//! Invalidate style - it needs updated again, otherwise it becomes stale.
		PreviousHudValue.m_ReticleStyle = RETICLE_STYLE_INVALID;

		if (bDisplayReticle)
		{
			bForceUpdatePosAndHeading = true;

			if (CHudTools::BeginHudScaleformMethod(NEW_HUD_RETICLE, "SET_USING_REMOTE_PLAY"))  // for 1847201
			{
				CScaleformMgr::AddParamBool(CControlMgr::GetPlayerMappingControl().IsUsingRemotePlay());
				CScaleformMgr::EndMethod();
			}

			// Always force the reticule type upon making reticule visible, otherwise we get visibility problems.
			if(c_IsUsingFirstPersonLazerWeapons && iCurrentWeaponHash == 0)
			{
				eDisplayReticle = DISPLAY_HIDDEN;
				ReticleLockOnStyle = LOCK_ON_INVALID;
				bDisplayReticle = false;
			}
			else
			{
				if (CHudTools::BeginHudScaleformMethod(NEW_HUD_RETICLE, "SET_RETICLE_TYPE"))
				{
					CScaleformMgr::AddParamInt(iCurrentWeaponHash);
					CScaleformMgr::EndMethod();
				}
			}

			if (CHudTools::BeginHudScaleformMethod(NEW_HUD_RETICLE, "SET_RETICLE_LOCKON_TYPE"))
			{
				CScaleformMgr::AddParamInt(ReticleLockOnStyle);
				CScaleformMgr::EndMethod();
			}

			if(humanNameHash != 0)
			{
				if (CHudTools::BeginHudScaleformMethod(NEW_HUD_RETICLE, "SET_RETICLE_TEXT"))
				{
					atString weaponName(TheText.Get(humanNameHash, "WeaponName"));

					if(humanNameSuffixHash != 0)
					{
						weaponName += " ";
						weaponName += TheText.Get(humanNameSuffixHash, "WeaponNameSuffix");
					}

					CScaleformMgr::AddParamString(weaponName.c_str());
					CScaleformMgr::EndMethod();
				}
			}
		}

		if(iCurrentWeaponHash == ATSTRINGHASH("WEAPON_HOMINGLAUNCHER", 0x63ab0442))
		{
			if(pPlayerPed && pPlayerPed->GetPlayerInfo())
			{
				TUNE_GROUP_FLOAT(ON_FOOT_HOMING_LAUNCHER, fReticuleHeightOffset, 1.4f, 0.0f, 10.0f, 0.01f);
				TUNE_GROUP_FLOAT(ON_FOOT_HOMING_LAUNCHER, fReticuleWidthOffset, 1.4f, 0.0f, 10.0f, 0.01f);
				if (CHudTools::BeginHudScaleformMethod(NEW_HUD_RETICLE, "OVERRIDE_HOMING_SCOPE"))
				{
					CScaleformMgr::AddParamFloat(fReticuleHeightOffset);	// height
					CScaleformMgr::AddParamFloat(fReticuleWidthOffset);		// width
					CScaleformMgr::EndMethod();
				}
			}
		}

		if(PreviousHudValue.m_eDisplayReticle != eDisplayReticle)
		{
			if (CHudTools::BeginHudScaleformMethod(NEW_HUD_RETICLE, "SET_RETICLE_VISIBLE"))
			{
				CScaleformMgr::AddParamBool(bDisplayReticle);
				CScaleformMgr::EndMethod();
			}
		}

		PreviousHudValue.m_iReticleMode = HUD_RETICLE_INVALID;
		PreviousHudValue.m_ReticleLockOnStyle = ReticleLockOnStyle;
		PreviousHudValue.m_iWeaponHashForReticule = iCurrentWeaponHash;
		PreviousHudValue.m_eDisplayReticle = eDisplayReticle;
		PreviousHudValue.m_uPrevHumanNameHash = humanNameHash;
		PreviousHudValue.m_uPrevHumanNameSuffixHash = humanNameSuffixHash;			
	}

	if( (iHeading != PreviousHudValue.m_iHeading) || bForceUpdatePosAndHeading)
	{
		if (CHudTools::BeginHudScaleformMethod(NEW_HUD_RETICLE, "SET_CAM_HEADING"))
		{
			CScaleformMgr::AddParamInt(iHeading);
			CScaleformMgr::EndMethod();
		}

		PreviousHudValue.m_iHeading = iHeading;
	}

	if ( (vReticlePosition != PreviousHudValue.m_vReticlePosition) || bForceUpdatePosAndHeading)
	{
		if (CHudTools::BeginHudScaleformMethod(NEW_HUD_RETICLE, "SET_RETICLE_POSITION"))
		{
			CScaleformMgr::AddParamFloat(vReticlePosition.x);
			CScaleformMgr::AddParamFloat(vReticlePosition.y);
			CScaleformMgr::AddParamFloat(CHudTools::GetAspectRatioMultiplier());
			CScaleformMgr::EndMethod();
		}

		PreviousHudValue.m_vReticlePosition = vReticlePosition;
	}

	sZoomState zoomState = CUIReticule::GetZoomState(pPlayerPed, pPlayerEquippedWeapon, gameplayDirector);
	if (zoomState.m_iZoomLevel != PreviousHudValue.m_iReticleZoom)
	{
		if(zoomState.m_iZoomLevel != INVALID_ZOOM_LEVEL)
		{
			if (CHudTools::BeginHudScaleformMethod(NEW_HUD_RETICLE, "SET_RETICLE_ZOOM"))
			{
				CScaleformMgr::AddParamInt(zoomState.m_iZoomLevel);
				CScaleformMgr::EndMethod();
			}
		}

		PreviousHudValue.m_iReticleZoom = zoomState.m_iZoomLevel;
	}

	//
	// do some reticle effects:
	//
	bool bReloading = false;
	bool bFiring = false;
	bool bAiming = false;
	bool bIsInCover = pPlayerPed->GetCoverPoint() != NULL;

	const CTaskGun* gunTask = static_cast<CTaskGun*>(pPlayerPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_GUN));
	if(gunTask)
	{
		bFiring = (gunTask->GetIsFiring());
		bAiming = (gunTask->GetIsAiming()) && (!bFiring) && (!camInterface::GetGameplayDirector().IsCameraInterpolating());
		bReloading = (gunTask->GetIsReloading()) && (!bAiming) && (!bFiring);
	}

	// Reset the visibility timer if we're firing the vehicle weapon
	if (bFadeOutVehicleWeaponReticuleAfterTimer)
	{
		bool bFiringVehicleWeapon = false;
		const CTaskVehicleMountedWeapon* pTask = static_cast<CTaskVehicleMountedWeapon*>(pPlayerPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_VEHICLE_MOUNTED_WEAPON));
		if(pTask)
		{
			bFiringVehicleWeapon = pTask->IsFiring();
		}

		if (bFiringVehicleWeapon && m_fTimeDisplayed != 0.0f)
		{
			m_fTimeDisplayed = 0.0f;
		}
	}

	//NOTE: We do not dim the overlay when reloading a scoped weapon.
	bool shouldDimReticule = gameplayDirector.ShouldDimReticule() || bReloading || bFadeOutVehReticule;
	if (pPlayerPed && pPlayerPed->GetPedIntelligence())
	{
		const CTaskVehicleMountedWeapon* pMountedWeaponTask = NULL;
		if(!shouldDimReticule)
		{
			pMountedWeaponTask = static_cast<CTaskVehicleMountedWeapon*>(pPlayerPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_VEHICLE_MOUNTED_WEAPON));
			if(pMountedWeaponTask)
			{
				shouldDimReticule = pMountedWeaponTask->ShouldDimReticule();
			}
		}

		if(!shouldDimReticule && isPassenger && !pMountedWeaponTask)
		{
			const CTaskAimGunVehicleDriveBy* pTask = static_cast<CTaskAimGunVehicleDriveBy*>(pPlayerPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AIM_GUN_VEHICLE_DRIVE_BY));
			if(pTask)
			{
				bool bIsThrown = pTask->GetPedWeaponInfo() && pTask->GetPedWeaponInfo()->GetIsThrownWeapon();
				shouldDimReticule = (!pTask->GetIsReadyToFire() && !bIsThrown);
			}
			else
			{
				const CTaskMountThrowProjectile* pMountThrowProjectileTask = static_cast<CTaskMountThrowProjectile*>(pPlayerPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_MOUNT_THROW_PROJECTILE));
				shouldDimReticule = (pMountThrowProjectileTask == NULL) ? true : false;
			}
		}
	}

	if (shouldDimReticule && !zoomState.m_bIsFirstPersonAimingThroughScope)  // dim the reticle if its impossible to fire at this point
	{
		// Fixes 337967, If we dim the reticle before it's visible, then it won't get dimmed.
		if (bDisplayReticle && (HUD_RETICLE_CANNOT_FIRE != PreviousHudValue.m_iReticleMode || bFadeOutVehReticule))
		{
			if (CHudTools::BeginHudScaleformMethod(NEW_HUD_RETICLE, "SET_RETICLE_STYLE"))
			{
				CScaleformMgr::AddParamInt(RETICLE_STYLE_DEFAULT);
				CScaleformMgr::EndMethod();
			}

			PreviousHudValue.m_ReticleStyle = RETICLE_STYLE_DEFAULT;

			if (CHudTools::BeginHudScaleformMethod(NEW_HUD_RETICLE, "START_DIM_RETICLE"))
			{
				if (bFadeOutVehReticule)
				{	
					CScaleformMgr::AddParamInt(iDimValue);
				}
				else
					CScaleformMgr::AddParamInt(15);
				CScaleformMgr::EndMethod();
			}


			if(m_killTargetThisFrame)
			{
				ShowHitMarker(HITMARKER_KILLED_BY_LOCAL_PLAYER);
			}

			if (m_incapacitatedTargetThisFrame)
			{
				ShowHitMarkerNonLethal(HITMARKER_INCAPACITATED_BY_LOCAL_PLAYER);
			}

			PreviousHudValue.m_iReticleMode = HUD_RETICLE_CANNOT_FIRE;
		}
	}
	else  // check for any scripted reticle states:
	if (CScriptHud::iScriptReticleMode != -1)
	{
		if (CScriptHud::iScriptReticleMode != PreviousHudValue.m_iReticleMode)
		{
			Color32 colour;

			switch ((eHudReticleModes)CScriptHud::iScriptReticleMode)
			{
				case HUD_RETICLE_CAN_BE_STUNNED:
				{
					colour = CHudColour::GetRGBA(HUD_COLOUR_RED);
					break;
				}

				default:
				{
					colour = CHudColour::GetRGBA(HUD_COLOUR_PURE_WHITE);
					Assertf(0, "CUIReticule:: Invalid hud reticle mode passed by script %d", CScriptHud::iScriptReticleMode);
					break;
				}
			}

			if (CHudTools::BeginHudScaleformMethod(NEW_HUD_RETICLE, "START_FLASHING_RETICLE"))
			{
				CScaleformMgr::AddParamInt(colour.GetRed());
				CScaleformMgr::AddParamInt(colour.GetGreen());
				CScaleformMgr::AddParamInt(colour.GetBlue());
				CScaleformMgr::AddParamFloat((float)colour.GetAlphaf() * 100.0f);
				CScaleformMgr::EndMethod();
			}

			PreviousHudValue.m_iReticleMode = static_cast<CUIReticule::eHudReticleModes>(CScriptHud::iScriptReticleMode);
		}
	}
	else  // reset the reticle to standard state:
	{
		bool wasDim = (PreviousHudValue.m_iReticleMode == HUD_RETICLE_CANNOT_FIRE);

		if (CScriptHud::iScriptReticleMode != PreviousHudValue.m_iReticleMode)
		{
			CHudTools::CallHudScaleformMethod(NEW_HUD_RETICLE, "RESET_RETICLE");

			PreviousHudValue.m_iReticleMode = static_cast<CUIReticule::eHudReticleModes>(CScriptHud::iScriptReticleMode);
		}

#if WATERMARKED_BUILD 
		Color32 overrideColour = GetWatermarkColour();

		if (CHudTools::BeginHudScaleformMethod(NEW_HUD_RETICLE, "START_FLASHING_RETICLE"))
		{
			CScaleformMgr::AddParamInt(overrideColour.GetRed());
			CScaleformMgr::AddParamInt(overrideColour.GetGreen());
			CScaleformMgr::AddParamInt(overrideColour.GetBlue());
			CScaleformMgr::AddParamFloat((float)1.0f * 100.0f);
			CScaleformMgr::EndMethod();
		}
#endif

#if SUPPORT_MULTI_MONITOR
		DrawBlinders(CNewHud::IsSniperSightActive());
#endif // SUPPORT_MULTI_MONITOR

		eRETICLE_STYLE retStyle;
		if (CNewHud::GetDisplayMode() == CNewHud::DM_ARCADE_CNC)
		{
			retStyle = GetReticuleStyleForCNCDisplayMode(pPlayerPed, pWeaponInfo);
		}
		else
		{
			retStyle = zoomState.m_bIsZoomed ? RETICLE_STYLE_DEFAULT : GetReticuleStyle(pPlayerPed, pWeaponInfo);
		}

		// if we are currently firing we need to check to see if we are hitting at any specific target and change the reticle if we are:
		if (pPlayerEquippedWeapon && (bAiming || bReloading || bFiring || bIsInCover))
		{
			bool hitPed = pPlayerPed->GetPedResetFlag(CPED_RESET_FLAG_HitPedWithWeapon) || m_bHitVehicleThisFrame;
			bool isEnemyOrFriend = (PreviousHudValue.m_ReticleStyle == RETICLE_STYLE_ENEMY || PreviousHudValue.m_ReticleStyle == RETICLE_STYLE_FRIENDLY);

			if(isFirstPersonAiming && hitPed)
			{
				if(CHudTools::BeginHudScaleformMethod(NEW_HUD_RETICLE, "SHOW_SNIPER_HIT"))
				{
					CScaleformMgr::AddParamBool(retStyle == RETICLE_STYLE_DEAD);
					CScaleformMgr::EndMethod();
				}
			}


			const CEntity* pTarget = GetTargetEntity(pPlayerPed, CScriptHud::bUseVehicleTargetingReticule);

			// Show the hit marker if you just shot your target ped, or if the target ped just died this frame.
			if(m_killTargetThisFrame || (retStyle == RETICLE_STYLE_DEAD && isEnemyOrFriend && PreviousHudValue.m_target == pTarget))
			{
				if(!isFirstPersonAiming)
				{
					if(m_killTargetThisFrame)
					{
						ShowHitMarker(HITMARKER_KILLED_BY_LOCAL_PLAYER);
					}
					else if(retStyle == RETICLE_STYLE_DEAD)
					{
						ShowHitMarker(HITMARKER_DEAD);
					}
					else
					{
						ShowHitMarker(HITMARKER_HIT);
					}
				}

				if(retStyle == RETICLE_STYLE_DEAD)
				{
					retStyle = RETICLE_STYLE_DEFAULT;
				}
			}
			
			//Show non-lethal hit marker if incapacitated this frame
			if (m_incapacitatedTargetThisFrame && !isFirstPersonAiming)
			{
				ShowHitMarkerNonLethal(HITMARKER_INCAPACITATED_BY_LOCAL_PLAYER);
			}

			PreviousHudValue.m_target.Reset(pTarget);
			float accuracyScaler = GetInvAccuracy(pPlayerPed, pPlayerEquippedWeapon);

			if (accuracyScaler != PreviousHudValue.m_accuracyScaler)
			{
				if(CHudTools::BeginHudScaleformMethod(NEW_HUD_RETICLE, "SET_RETICLE_ACCURACY"))
				{
					CScaleformMgr::AddParamFloat(accuracyScaler);
					CScaleformMgr::EndMethod();
				}

				PreviousHudValue.m_accuracyScaler = accuracyScaler;
			}
		}

		if(CScriptHud::bUseVehicleTargetingReticule)
		{
			if(m_bHitVehicleThisFrame)
			{
				ShowHitMarker(HITMARKER_DEAD);
			}
		}
		
		if(ReticleLockOnStyle == LOCK_ON_INVALID && iCurrentWeaponHash != LAZER_COCKPIT_MACHINE)
		{
			if(wasDim || retStyle != PreviousHudValue.m_ReticleStyle)
			{
				// Otherwise affect the color
				if (CHudTools::BeginHudScaleformMethod(NEW_HUD_RETICLE, "SET_RETICLE_STYLE"))
				{
					CScaleformMgr::AddParamInt(static_cast<int>(retStyle));
					CScaleformMgr::EndMethod();
				}

				PreviousHudValue.m_ReticleStyle = retStyle;
			}
		}
		else
		{
			PreviousHudValue.m_ReticleStyle = RETICLE_STYLE_INVALID;
		}
	}

	if(!m_queryUsedThisFrame && m_queryId != INVALID_RETICULE_QUERY)
	{
		OcclusionQueries::OQFree(m_queryId);
		m_queryId = INVALID_RETICULE_QUERY;
	}

#if GTA_REPLAY
	// store off reticule data for replays
	if(CReplayMgr::ShouldRecord() && pPlayerPed && (pPlayerPed->GetReplayID() != ReplayIDInvalid))
	{
		if(!ReplayReticuleExtension::HasExtension(pPlayerPed))
		{
			ReplayReticuleExtension::Add(pPlayerPed);
		}

		if(ReplayReticuleExtension::HasExtension(pPlayerPed))
		{
			ReplayReticuleExtension::SetWeaponHash(pPlayerPed, bDisplayReticle ? iCurrentWeaponHash : 0);
			if (bDisplayReticle)
			{
				ReplayReticuleExtension::SetReadyToFire(pPlayerPed, bReloading);
				ReplayReticuleExtension::SetHit(pPlayerPed, m_killTargetThisFrame);
				ReplayReticuleExtension::SetIsFirstPerson(pPlayerPed, isFirstPersonAiming);

				// zoom is 0 - 100 ...don't really need an s16 for that.
				// don't worry about invalid too
				u8 zoomLevel = zoomState.m_iZoomLevel > 0 ? static_cast<u8>(zoomState.m_iZoomLevel) : 0;
				ReplayReticuleExtension::SetZoomLevel(pPlayerPed, zoomLevel);
			}
			else
			{
				ReplayReticuleExtension::SetReadyToFire(pPlayerPed, false);
				ReplayReticuleExtension::SetIsFirstPerson(pPlayerPed, false);
				ReplayReticuleExtension::SetHit(pPlayerPed, false);
				ReplayReticuleExtension::SetZoomLevel(pPlayerPed, 0);
			}
		}
	}
#endif // GTA_REPLAY

	m_killTargetThisFrame = false;
	m_incapacitatedTargetThisFrame = false;
	m_bHitVehicleThisFrame = false;
}

#if SUPPORT_MULTI_MONITOR
void CUIReticule::DrawBlinders(bool bSniperActive)
{
	bool bInCutscene = (CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->IsRunning());
	if(bSniperActive && !bInCutscene)
	{
		CNewHud::SetDrawBlinder(2);
	}

	if(!bSniperActive && CNewHud::ShouldDrawBlinders() && !CutSceneManager::GetManualBlinders())
	{
		CNewHud::SetDrawBlinder(0);
		CutSceneManager::StartMultiheadFade(false, true);
	}
}
#endif // SUPPORT_MULTI_MONITOR

CUIReticule::eRETICLE_STYLE CUIReticule::GetReticuleStyleForCNCDisplayMode(CPed * pPlayerPed, const CWeaponInfo *pWeaponInfo)
{
	if (!uiVerify(pPlayerPed))
	{
		return RETICLE_STYLE_INVALID;
	}

	eRETICLE_STYLE retStyle = RETICLE_STYLE_DEFAULT;
	const CEntity* pTargetEntity = GetTargetEntity(pPlayerPed, CScriptHud::bUseVehicleTargetingReticule);

	if (pTargetEntity)
	{
		const CPed* pTargetPed = pTargetEntity->GetIsTypeVehicle() ? static_cast<const CVehicle*>(pTargetEntity)->GetDriver() : pTargetEntity->GetIsTypePed() ? static_cast<const CPed*>(pTargetEntity) : nullptr;

		if (pTargetPed == nullptr || (!CScriptHud::bUseVehicleTargetingReticule && pTargetEntity->GetIsTypeVehicle()))
		{
			return retStyle;
		}

		CPedTargetting* pTargetPedTargetting = pTargetPed->GetPedIntelligence()->GetTargetting();

		if (pTargetPed->IsDead())
		{
			retStyle = RETICLE_STYLE_DEAD;
		}
		else
		{
			bool canBeTargeted = !pTargetPed->GetPedConfigFlag(CPED_CONFIG_FLAG_NeverEverTargetThisPed);

			if (NetworkInterface::AreBulletsImpactsDisabledInMP(*pPlayerPed, *pTargetPed))
			{
				canBeTargeted = false;
			}

			if (pTargetPed->IsVisible() && canBeTargeted)
			{
				bool isEnemy = false;
				bool isBuddy = false;


				if (pTargetPed->IsPlayer())
				{
					eArcadeTeam eLocalTeam = pPlayerPed->GetPlayerInfo()->GetArcadeInformation().GetTeam();
					eArcadeTeam eTargetTeam = pTargetPed->GetPlayerInfo()->GetArcadeInformation().GetTeam();

					if (eLocalTeam == eTargetTeam)
					{
						isBuddy = true;
					}
					// If a crook is wanted and we are a cop they are our enemy
					else if (eLocalTeam == eArcadeTeam::AT_CNC_COP && eTargetTeam == eArcadeTeam::AT_CNC_CROOK)
					{
						if (!pTargetPed->GetPlayerWanted()->m_WantedLevel == WANTED_CLEAN)
						{
							isEnemy = true;
						}
					}
					else
					{
						isEnemy = true;
					}
				}
				else
				{
					if (pTargetPed->GetPedConfigFlag(CPED_CONFIG_FLAG_PedIsEnemyToPlayer))
					{
						isEnemy = true;
					}
					else
					{
						if(pTargetPed->GetPedIntelligence()->GetQueriableInterface() && pTargetPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_COMBAT))
						{
							CEntity* pTargetEntity = pTargetPed->GetPedIntelligence()->GetQueriableInterface()->GetTargetForTaskType(CTaskTypes::TASK_COMBAT);
							if (pTargetEntity && pTargetEntity->GetIsTypePed() && static_cast<CPed*>(pTargetEntity)->IsLocalPlayer())
							{
								isEnemy = true;
							}
							else
							{
								if(pTargetPedTargetting && pTargetPedTargetting->FindTarget(pPlayerPed))
								{
									isEnemy = true;
								}
							}
						}

						if (!isEnemy)
						{
							if(pTargetPed->GetPedIntelligence()->IsThreatenedBy(*pPlayerPed) || pPlayerPed->GetPedIntelligence()->IsThreatenedBy(*pTargetPed))
							{
								isEnemy = true;
							}
						}
					}

				}
				if (pWeaponInfo && pWeaponInfo->GetFlag(CWeaponInfoFlags::NonLethal) && pTargetPed->GetPedConfigFlag(CPED_CONFIG_FLAG_CanBeIncapacitated))
				{
					if (pTargetPed->GetEndurance() > 0.0f)
					{
						retStyle = RETICLE_STYLE_NON_LETHAL;
					}
					else
					{
						retStyle = RETICLE_STYLE_DEAD;
					}
				}
				else if (isBuddy)
				{
					retStyle = RETICLE_STYLE_BUDDY;
				}
				else if (isEnemy)
				{
					retStyle = RETICLE_STYLE_ENEMY;
				}
				else
				{
					retStyle = RETICLE_STYLE_DEAD;
				}
			}
		}
	}

	return retStyle;
}

CUIReticule::eRETICLE_STYLE CUIReticule::GetReticuleStyle(CPed* pPlayerPed, const CWeaponInfo *pWeaponInfo)
{
	eRETICLE_STYLE retStyle = RETICLE_STYLE_DEFAULT;
	const CEntity* pTargetEntity = GetTargetEntity(pPlayerPed, CScriptHud::bUseVehicleTargetingReticule);

	if(pTargetEntity)
	{
		const CPed* pTargetPed = pTargetEntity->GetIsTypeVehicle() ? static_cast<const CVehicle*>(pTargetEntity)->GetDriver() : pTargetEntity->GetIsTypePed() ? static_cast<const CPed*>(pTargetEntity) : nullptr;

		if(pTargetPed == nullptr || (!CScriptHud::bUseVehicleTargetingReticule && pTargetEntity->GetIsTypeVehicle()))
		{
			return retStyle;
		}

		CPedTargetting* pTargetPedTargetting = pTargetPed->GetPedIntelligence()->GetTargetting();

		if(pTargetPed->IsDead())
		{
			retStyle = RETICLE_STYLE_DEAD;
		}
		else
		{
			bool canBeTargeted = !pTargetPed->GetPedConfigFlag( CPED_CONFIG_FLAG_NeverEverTargetThisPed);

			if (NetworkInterface::AreBulletsImpactsDisabledInMP(*pPlayerPed, *pTargetPed))
			{
				canBeTargeted = false;
			}

			if(pTargetPed->IsVisible() && canBeTargeted)
			{
				bool isEnemy = false;
				bool isBuddy = false;

				if(pTargetPed->IsPlayer())
				{
					CNetObjPed* pTargetPedObj = SafeCast(CNetObjPed, pTargetPed->GetNetworkObject());
					if (pTargetPedObj)
					{
						isBuddy = !pTargetPedObj->GetIsTargettableByPlayer(pPlayerPed->GetPlayerInfo()->GetPhysicalPlayerIndex());
					}
				}

				if(isBuddy)
				{
					retStyle = RETICLE_STYLE_BUDDY;
				}
				else
				{
					if(pTargetPed->IsPlayer() && pWeaponInfo)
					{
						isEnemy = pPlayerPed->IsAllowedToDamageEntity(pWeaponInfo, pTargetPed);
					}
				
					if(pTargetPed->GetPedConfigFlag(CPED_CONFIG_FLAG_PedIsEnemyToPlayer))
					{
						isEnemy = true;
					}
					else
					{
						if(pTargetPed->GetPedIntelligence()->GetQueriableInterface() && pTargetPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_COMBAT))
						{
							CEntity* pTargetEntity = pTargetPed->GetPedIntelligence()->GetQueriableInterface()->GetTargetForTaskType(CTaskTypes::TASK_COMBAT);
							if(pTargetEntity && pTargetEntity->GetIsTypePed() && static_cast<CPed*>(pTargetEntity)->IsLocalPlayer())
							{
								isEnemy   = true;
							}
							else
							{
								if(pTargetPedTargetting && pTargetPedTargetting->FindTarget(pPlayerPed))
								{
									isEnemy   = true;
								}
							}
						}

						if(!isEnemy)
						{
							if(pTargetPed->GetPedIntelligence()->IsThreatenedBy(*pPlayerPed) || pPlayerPed->GetPedIntelligence()->IsThreatenedBy(*pTargetPed))
							{
								isEnemy = true;
							}
						}
					}

					if(isEnemy)
					{
						retStyle = RETICLE_STYLE_ENEMY;
					}
					else
					{
						retStyle = RETICLE_STYLE_FRIENDLY;
					}
				}
			}
		}
	}

	return retStyle;
}

float CUIReticule::GetInvAccuracy(const CPed* pPlayerPed, const CWeapon* pPlayerEquippedWeapon)
{
	if(!uiVerify(pPlayerEquippedWeapon))
	{
		return 1;
	}

	const CEntity* pTarget = GetTargetEntity(pPlayerPed, CScriptHud::bUseVehicleTargetingReticule);
	const float distToTarget = (pTarget && pPlayerPed) ? (pPlayerPed->GetPreviousPosition() - pTarget->GetPreviousPosition()).Mag() : sm_fDistanceToNoTarget;

	sWeaponAccuracy weaponAccuracy;
	pPlayerEquippedWeapon->GetAccuracy(pPlayerPed, distToTarget, weaponAccuracy);
	float spread = pPlayerEquippedWeapon->GetWeaponInfo()->GetAccuracySpread();

	return (1.0f - weaponAccuracy.fAccuracyMin) * spread;
}

CUIReticule::sZoomState CUIReticule::GetZoomState(const CPed *pPlayerPed, const CWeapon* pPlayerEquippedWeapon, const camGameplayDirector& gameplayDirector)
{
	sZoomState zoomState;

	if (pPlayerEquippedWeapon && pPlayerEquippedWeapon->GetHasFirstPersonScope())
	{
		const camFirstPersonPedAimCamera* pAimCamera = gameplayDirector.GetFirstPersonPedAimCamera(pPlayerPed);
		if (pAimCamera)
		{
			zoomState.m_bIsFirstPersonAimingThroughScope = true;

			const camControlHelper* pControlHelper = pAimCamera->GetControlHelper();
			if (pControlHelper)
			{
				const Vector2& vZoomFactorLimits = pControlHelper->GetZoomFactorLimits();

				if(vZoomFactorLimits.y > (vZoomFactorLimits.x + SMALL_FLOAT))
				{
					zoomState.m_bIsZoomed = true;
					float fZoomFactor = pControlHelper->GetZoomFactor();

					zoomState.m_iZoomLevel = (s32)Floorf(RampValueSafe(fZoomFactor, vZoomFactorLimits.x, vZoomFactorLimits.y, 0.0f, 100.0f));
				}
			}
		}
	}

	return zoomState;
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CUIReticule::TransformReticulePosition()
// PURPOSE: transforms reticule postion from world space to screen space
/////////////////////////////////////////////////////////////////////////////////////
void CUIReticule::TransformReticulePosition(const Vector3& vReticlePositionWorld, Vector2& vReticlePosition, bool& bDisplayReticle)
{
	if(!uiVerify(gVpMan.GetCurrentViewport()))
	{
		return;
	}

	float fX = 1.0f;
	float fY = 1.0f;
	gVpMan.GetCurrentViewport()->GetGrcViewport().Transform((Vec3V&)vReticlePositionWorld, fX, fY);

	float fWidth = (float)GRCDEVICE.GetWidth();
	float fHeight = (float)GRCDEVICE.GetHeight();

#if SUPPORT_MULTI_MONITOR
	if(GRCDEVICE.GetMonitorConfig().isMultihead())
	{
		const GridMonitor &mon = GRCDEVICE.GetMonitorConfig().getLandscapeMonitor();
		float fOffset = fWidth * mon.getArea().GetWidth();
		fWidth = fOffset;
		fX -= fWidth;
	}
#endif // SUPPORT_MULTI_MONITOR
	vReticlePosition.x = fX / fWidth;
	vReticlePosition.y = fY / fHeight;

	if (vReticlePosition.x > 1.0f ||
		vReticlePosition.y > 1.0f ||
		vReticlePosition.x < 0.0f ||
		vReticlePosition.y < 0.0f)
	{
		bDisplayReticle = false;
	}
	else
	{
		// round off to 3dp so we dont get a very slightly different value every frame, even though the on-screen pos is the same in reality
		vReticlePosition.x = ceilf(vReticlePosition.x * 1000.0f) / 1000.0f;
		vReticlePosition.y = ceilf(vReticlePosition.y * 1000.0f) / 1000.0f;
	}
}

const CEntity* CUIReticule::GetTargetEntity(const CPed* pPlayerPed, bool bCheckVehicles)
{
	if(!uiVerify(pPlayerPed) || !uiVerify(pPlayerPed->GetPlayerInfo()))
	{
		return NULL;
	}

	return bCheckVehicles ? pPlayerPed->GetPlayerInfo()->GetTargeting().GetFreeAimTarget() : pPlayerPed->GetPlayerInfo()->GetTargeting().GetFreeAimTargetRagdoll();
}

bool CUIReticule::IsSphereVisible(const Vector3& vReticlePositionWorld, float fPixelThresholdOverride)
{
	bool isOnScreen = uiVerify(gVpMan.GetCurrentViewport()) ? (gVpMan.GetCurrentViewport()->GetGrcViewport().IsSphereVisible(vReticlePositionWorld.x, vReticlePositionWorld.y, vReticlePositionWorld.z, 0.01f) != cullOutside) : false;

	if(isOnScreen && !camInterface::GetCinematicDirector().IsRenderingCinematicMountedCamera())
	{
		m_queryUsedThisFrame = true;

		if(m_queryId == INVALID_RETICULE_QUERY)
		{
			m_queryId =OcclusionQueries::OQAllocate();
			Assertf(m_queryId != 0, "Out of Occlusion Query Ids %s",CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}

		if(m_queryId != INVALID_RETICULE_QUERY)
		{
			int pixelCount = OcclusionQueries::OQGetQueryResult(m_queryId);

			if(0 <= pixelCount)
			{
				float fThreshold = fPixelThresholdOverride > 0.0f ? fPixelThresholdOverride : sm_fDrawSpherePixelLimit;
				isOnScreen = fThreshold < pixelCount;
			}

			const Vec3V minV(ScalarV(-sm_fDrawSphereRadius * 0.5f));
			const Vec3V maxV(ScalarV(sm_fDrawSphereRadius * 0.5f));
			Mat34V matrix(V_IDENTITY);
			matrix.Setd(RCC_VEC3V(vReticlePositionWorld));
	
			OcclusionQueries::OQSetBoundingBox(m_queryId, minV, maxV, matrix);
		}
	}

	return isOnScreen;
}

void CUIReticule::ShowHitMarker(eReticleHitMarkerStyle markerStyle)
{
	if(CHudTools::BeginHudScaleformMethod(NEW_HUD_RETICLE, "SHOW_HITMARKER"))
	{
		CScaleformMgr::AddParamInt((int)markerStyle);
		CScaleformMgr::EndMethod();
	}

}

void CUIReticule::ShowHitMarkerNonLethal(eReticleHitMarkerNonLethalStyle markerStyle)
{
	if (CHudTools::BeginHudScaleformMethod(NEW_HUD_RETICLE, "SHOW_HITMARKER_NON_LETHAL"))
	{
		CScaleformMgr::AddParamInt((int)markerStyle);
		CScaleformMgr::EndMethod();
	}
}

void CUIReticule::RegisterPedKill(const CPed& inflictor, const CPed& victim, u32 weaponHash)
{
	if(inflictor.IsLocalPlayer())
	{
		const CEntity* pTarget = GetPreviousValues().m_target.Get();
		u32 equipedWeaponHash = inflictor.GetEquippedWeaponInfo() ? inflictor.GetEquippedWeaponInfo()->GetHash() : 0;

		// If you kill the ped your aiming at, or if you don't have a target but kill someone (i.e. using shotgun and their outside your aiming probe range, or kill the target before the aiming probe finishes)
		if(pTarget == &victim ||
			(pTarget == NULL && weaponHash == equipedWeaponHash))
		{
			m_killTargetThisFrame = true;
		}
	}
}

void CUIReticule::RegisterPedIncapacitated(const CPed& inflictor, const CPed& victim, u32 weaponHash)
{
	if (inflictor.IsLocalPlayer())
	{
		const CEntity* pTarget = GetPreviousValues().m_target.Get();
		u32 equipedWeaponHash = inflictor.GetEquippedWeaponInfo() ? inflictor.GetEquippedWeaponInfo()->GetHash() : 0;

		// If you incapacitate the ped your aiming at, or if you don't have a target but incapacitate someone (i.e. using shotgun and their outside your aiming probe range, or incapacitate the target before the aiming probe finishes)
		if (pTarget == &victim ||
			(pTarget == NULL && weaponHash == equipedWeaponHash))
		{
			m_incapacitatedTargetThisFrame = true;
		}
	}
}

void CUIReticule::RegisterVehicleHit(const CPed& inflictor, u32 uVehicleHash, float iHealthLost)
{
	if(inflictor.IsLocalPlayer())
	{
		// NOTE: As of now we're only going to allow this for the Bombushka DLC vehicle...
		// ... and some others because 'this is a one off' was a lie.
		#define BOMBUSHKA_HASH 4262088844
		#define MIN_HP_LOSS_REQUIRED_FOR_HIT 13

		bool bValidVehicleHit = false;
		for(int i = 0; i < m_ValidVehicleHitHashes.GetCount(); ++i)
		{
			if(uVehicleHash == m_ValidVehicleHitHashes[i])
			{
				bValidVehicleHit = true;
				break;
			}
		}

		if( uVehicleHash == BOMBUSHKA_HASH ||
			bValidVehicleHit)
		{
			if(iHealthLost > MIN_HP_LOSS_REQUIRED_FOR_HIT)
			{
				m_bHitVehicleThisFrame = true;
			}
		}
	}
}

void CUIReticule::SetWeaponTimer(float fTimePercent)
{
	Color32 colour;
	fTimePercent = 100 - fTimePercent;
	bool bWhite = (fTimePercent == 100) ||
		(fTimePercent >= 98 && fTimePercent <= 99) ||
		(fTimePercent >= 92 && fTimePercent <= 94) ||
		(fTimePercent >= 85 && fTimePercent <= 88) ||
		(fTimePercent >= 77 && fTimePercent <= 80) ||
		(fTimePercent >= 67 && fTimePercent <= 71) ||
		(fTimePercent >= 56 && fTimePercent <= 61) ||
		(fTimePercent >= 43 && fTimePercent <= 49) ||
		(fTimePercent >= 27 && fTimePercent <= 35) ||
		(fTimePercent >= 8 && fTimePercent <= 18);

	if(bWhite)
	{
		colour = CHudColour::GetRGBA(HUD_COLOUR_PURE_WHITE);
	}
	else
	{
		colour = CHudColour::GetRGBA(HUD_COLOUR_GREYDARK);
	}

	if (CHudTools::BeginHudScaleformMethod(NEW_HUD_RETICLE, "SET_RETICULE_COLOR"))
	{
		CScaleformMgr::AddParamInt(colour.GetRed());
		CScaleformMgr::AddParamInt(colour.GetGreen());
		CScaleformMgr::AddParamInt(colour.GetBlue());
		CScaleformMgr::AddParamFloat((float)colour.GetAlphaf() * 100.0f);
		CScaleformMgr::EndMethod();
	}
}

#if __BANK
/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////
void CUIReticule::CreateBankWidgets(bkBank *pBank)
{
	if(uiVerify(pBank))
	{
		pBank->AddToggle("Hide reticle", &m_bForceHideReticle);
		pBank->AddToggle("Hide reticle when using laser sight", &sm_bHideReticleWithLaserSight);
		pBank->AddSlider("Assumed non-target distance", &sm_fDistanceToNoTarget, 0.0f, 1000.0f, 0.1f);
		pBank->AddSlider("Reticule Accuracy Scaler", &sm_fAccuracyScaler, 0.0f, 1000.0f, 1.0f);
		pBank->AddSlider("Max Inv Reticule Accuracy", &sm_uMaxInvAccuracy, 0, 1000, 1);
		pBank->AddSlider("Draw Sphere Radius", &sm_fDrawSphereRadius, 0.0f, 100.0f, 0.1f);

		{
			const int pWeaponNameSize = 50;
			char* pWeaponName;
			u32 nameHash = 0;

			pWeaponName = rage_new char[pWeaponNameSize];
			formatf(pWeaponName, pWeaponNameSize, "%s (%d)", "NONE", nameHash);
			m_weaponNames.PushAndGrow(pWeaponName);
			m_weaponHashes.PushAndGrow(0);

			pWeaponName = rage_new char[pWeaponNameSize];
			formatf(pWeaponName, pWeaponNameSize, "%s (%d)", "FAKED_SCOPED_RETICLE", FAKED_WEAPON_HASH_FOR_SCOPED_RETICLE);
			m_weaponNames.PushAndGrow(pWeaponName);
			m_weaponHashes.PushAndGrow(FAKED_WEAPON_HASH_FOR_SCOPED_RETICLE);

			pWeaponName = rage_new char[pWeaponNameSize];
			formatf(pWeaponName, pWeaponNameSize, "%s (%d)", "FAKED_SIMPLE_RETICLE", FAKED_WEAPON_HASH_FOR_SIMPLE_RETICLE);
			m_weaponNames.PushAndGrow(pWeaponName);
			m_weaponHashes.PushAndGrow(FAKED_WEAPON_HASH_FOR_SIMPLE_RETICLE);

			CWeaponInfoManager::StringList& weaponNames = CWeaponInfoManager::GetNames(CWeaponInfoBlob::IT_Weapon);
			for(int i=0; i<weaponNames.GetCount(); ++i)
			{
				nameHash = atStringHash(weaponNames[i]);

				pWeaponName = rage_new char[pWeaponNameSize];
				formatf(pWeaponName, pWeaponNameSize, "%s (%d)", weaponNames[i], nameHash);
				m_weaponNames.PushAndGrow(pWeaponName);
				m_weaponHashes.PushAndGrow(nameHash);
			}

			pBank->AddCombo("Reticule Override", &m_debugOverrideHashIndex, m_weaponNames.GetCount(), &m_weaponNames[0], NullCallback);
		}
	}
}
#endif

// eof

