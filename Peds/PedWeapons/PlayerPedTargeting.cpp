// File header
#include "Peds/PedWeapons/PlayerPedTargeting.h"

//rage headers
#include "math/vecmath.h"

#include "fwmaths/geomutil.h"
#include "fwmaths/vectorutil.h"

// Game headers
#include "Camera/CamInterface.h"
#include "Camera/Gameplay/GameplayDirector.h"
#include "Camera/Gameplay/Aim/ThirdPersonAimCamera.h"
#include "camera/gameplay/aim/FirstPersonShooterCamera.h"
#include "Debug/DebugScene.h"
#include "Game/Localisation.h"
#include "Event/EventShocking.h"
#include "Event/EventWeapon.h"
#include "Event/ShockingEvents.h"
#include "Frontend/NewHud.h"
#include "Ik/solvers/TorsoSolver.h"
#include "Network/Network.h"
#include "PedGroup/PedGroup.h"
#include "Peds/Ped_Channel.h"
#include "Peds/PedCapsule.h"
#include "Peds/PedIntelligence.h"
#include "Peds/PedWeapons/PedTargetEvaluator.h"
#include "Peds/Ped.h"
#include "Physics/Physics.h"
#include "Task/Combat/Cover/TaskCover.h"
#include "Task/Combat/TaskThreatResponse.h"
#include "Task/Default/TaskPlayer.h"
#include "Task/Weapons/TaskProjectile.h"
#include "Task/Weapons/Gun/TaskAimGunOnFoot.h"
#include "Task/Weapons/Gun/TaskVehicleDriveBy.h"
#include "Task/Motion/TaskMotionBase.h"
#include "vehicleAi/VehicleIntelligence.h"
#include "vehicles/heli.h"
#include "Vehicles/VehicleGadgets.h"
#include "Vehicles/Metadata/VehicleLayoutInfo.h"
#include "Weapons/Weapon.h"
#include "Weapons/WeaponDebug.h"


// Parser headers
#include "Peds/PedWeapons/PedWeapons_parser.h"

AI_OPTIMISATIONS()

extern const float g_DefaultDeadPedLockOnDuration = 0.75f;

//////////////////////////////////////////////////////////////////////////
// CCurve
//////////////////////////////////////////////////////////////////////////

CCurve::CCurve()
: m_fInitialValue(0.0f)
, m_fInputMin(0.0f)
, m_fInputMax(0.0f)
, m_fResultMax(0.0f)
, m_fPow(0.0f)
, m_fOneOverInputMaxMinusInputMin(0.0f)
{
}

float CCurve::GetResult(float fInput) const
{
	if(fInput <= m_fInputMax)
	{
		float fResult = 0.0f;

		// If input min and max are equal, just effectively return m_fInitialValue
		if(m_fInputMax != m_fInputMin)
		{
			fResult = rage::Powf((fInput - m_fInputMin) * m_fOneOverInputMaxMinusInputMin, m_fPow);
		}

		fResult = m_fInitialValue + fResult * Max((m_fResultMax - m_fInitialValue), 0.0f);
		fResult = Clamp(fResult, 0.0f, 1.0f);
		return fResult;
	}

	// No result for input
	return -1.0f;
}

void CCurve::UpdateCurveOnChange()
{
	UpdateOneOverInputMaxMinusInputMinInternal();
	CPlayerPedTargeting::FinaliseCurveSets();
}

void CCurve::UpdateOneOverInputMaxMinusInputMinInternal()
{
	m_fOneOverInputMaxMinusInputMin = 1.0f / (m_fInputMax - m_fInputMin);
}

//////////////////////////////////////////////////////////////////////////
// CCurveSet
//////////////////////////////////////////////////////////////////////////

CCurveSet::CCurveSet()
#if __BANK
: m_colour(Color_white)
#endif // __BANK
{
}

void CCurveSet::SetCurve(s32 iCurveIndex, float fInputMax, float fResultMax, float fPow)
{
	pedFatalAssertf(iCurveIndex >= 0 && iCurveIndex < m_curves.GetCount(), "iCurveIndex is out of array bounds");
	m_curves[iCurveIndex].Set(fInputMax, fResultMax, fPow);
}

float CCurveSet::GetResult(float fInput) const
{
	for(s32 i = 0; i < m_curves.GetCount(); i++)
	{
		float fCurveResult = m_curves[i].GetResult(fInput);
		if(fCurveResult >= 0.0f)
		{
			return fCurveResult;
		}
	}

	return  0.0f;
}

void CCurveSet::Finalise()
{
	for(s32 i = 0; i < m_curves.GetCount(); i++)
	{
		if(i > 0)
		{
			if(m_curves[i].GetInputMax() < m_curves[i-1].GetInputMax())
			{
				m_curves[i].SetInputMax(m_curves[i-1].GetInputMax());
			}

			m_curves[i].SetInputMin(m_curves[i-1].GetInputMax());
		}

		m_curves[i].UpdateOneOverInputMaxMinusInputMinInternal();
	}

	for(s32 i = 1; i < m_curves.GetCount(); i++)
	{
		m_curves[i].SetInitialValue(m_curves[i-1].GetResult(m_curves[i-1].GetInputMax()));
	}
}

#if __BANK

void CCurveSet::PreAddWidgets(bkBank& bank)
{
	bank.PushGroup(m_Name.GetCStr(), false);
}

void CCurveSet::PostAddWidgets(bkBank& bank)
{
	bank.PopGroup();
}

void CCurveSet::Render(const Vector2& vOrigin, const Vector2& vEnd, float fInputScale) const
{
	Vector2 vLastResult(vOrigin);

	float fInput = 0.0f;
	while(fInput <= fInputScale)
	{
		float fResult = GetResult(fInput);

		Vector2 vNewPoint;
		vNewPoint.x = vOrigin.x + (vEnd.x - vOrigin.x) * (fInput/fInputScale);
		vNewPoint.y = vOrigin.y + (vEnd.y - vOrigin.y) * fResult;
		grcDebugDraw::Line(vLastResult, vNewPoint, m_colour);

		static bank_float INCREMENT = 0.01f;
		fInput += (INCREMENT * fInputScale);
		vLastResult = vNewPoint;
	}
}
#endif // __BANK

void CTargettingDifficultyInfo::PostLoad()
{
#if __BANK
	Color32 colours[CTargettingDifficultyInfo::CS_Max] = { Color_orange, Color_blue, Color_cyan, Color_magenta };
	Assert(m_CurveSets.GetCount() > 0);
	for(s32 i = 0; i < CTargettingDifficultyInfo::CS_Max; i++)
	{
		m_CurveSets[i].SetColour(colours[i]);
	}
#endif // __BANK
}

//////////////////////////////////////////////////////////////////////////
// CPlayerPedTargeting
//////////////////////////////////////////////////////////////////////////

// Static member initialisation
CPlayerPedTargeting::Tunables CPlayerPedTargeting::ms_Tunables;	
IMPLEMENT_PLAYER_TARGETTING_TUNABLES(CPlayerPedTargeting, 0xcb21697a);

const u8 CPlayerPedTargeting::MAX_FREE_AIM_ASYNC_RESULTS = 32;

CPlayerPedTargeting::ePlayerTargetLevel CPlayerPedTargeting::ms_ePlayerTargetLevel = CPlayerPedTargeting::PTL_TARGET_STRANGERS;

#if __BANK
bool CPlayerPedTargeting::ms_bEnableSnapShotBulletBend = false;
float CPlayerPedTargeting::ms_fSnapShotRadiusMin = 0.5f;
float CPlayerPedTargeting::ms_fSnapShotRadiusMax = 2.0f;
#endif

PARAM(enablecovervantagepointforlos, "[CPlayerPedTargeting] Use cover vantage point for LOS tests");

////////////////////////////////////////////////////////////////////////////////

const CTargettingDifficultyInfo& CPlayerPedTargeting::Tunables::GetTargettingInfo() const
{
	bool bFirstPerson = FPS_MODE_SUPPORTED_ONLY(CGameWorld::FindLocalPlayer() ? CGameWorld::FindLocalPlayer()->IsFirstPersonShooterModeEnabledForPlayer(false) || CGameWorld::FindLocalPlayer()->IsInFirstPersonVehicleCamera():) false;

	switch(CPedTargetEvaluator::GetTargetMode(CGameWorld::FindLocalPlayer()))
	{
	case(CPedTargetEvaluator::TARGETING_OPTION_GTA_TRADITIONAL):
		return bFirstPerson ? m_FirstPersonEasyTargettingDifficultyInfo : m_EasyTargettingDifficultyInfo;
	case(CPedTargetEvaluator::TARGETING_OPTION_ASSISTED_AIM):
		return bFirstPerson ? m_FirstPersonNormalTargettingDifficultyInfo : m_NormalTargettingDifficultyInfo;
	case(CPedTargetEvaluator::TARGETING_OPTION_ASSISTED_FREEAIM):
		return bFirstPerson ? m_FirstPersonHardTargettingDifficultyInfo : m_HardTargettingDifficultyInfo;
	case(CPedTargetEvaluator::TARGETING_OPTION_FREEAIM):
		return bFirstPerson ? m_FirstPersonExpertTargettingDifficultyInfo : m_ExpertTargettingDifficultyInfo;
	}

	Assert(0);
	return m_EasyTargettingDifficultyInfo;
}

////////////////////////////////////////////////////////////////////////////////

CPlayerPedTargeting::CPlayerPedTargeting()
: m_pPed(NULL)
, m_FreeAimProbeResults(MAX_FREE_AIM_ASYNC_RESULTS)
, m_FreeAimAssistProbeResults(MAX_FREE_AIM_ASYNC_RESULTS)
, m_fLostLockLOSTimer(0.0f)
, m_uLastLockLOSTestTime(0)
, m_pPotentialTarget(NULL)
, m_fPotentialTargetTimer(0.0f)
, m_uManualSwitchTargetTime(0)
{
	Reset();
}

bool CPlayerPedTargeting::CanPromoteToHardLock(TargetingMode UNUSED_PARAM(eTargetingMode), const CWeapon* UNUSED_PARAM(pWeapon), const CWeaponInfo& UNUSED_PARAM(weaponInfo))
{
	return false;
}

void CPlayerPedTargeting::SetOwner(CPed* pPed)
{ 
	m_pPed = pPed; 
	Reset();
}

void CPlayerPedTargeting::Reset()
{
	m_fTimeAiming = 0.0f;
	m_pLockOnTarget =NULL;
	m_pCachedLockOnTarget = NULL;
	m_vLockOnTargetPos = Vector3::ZeroType;
	m_vLastLockOnTargetPos = Vector3::ZeroType;
	m_vClosestFreeAimTargetPos = Vector3::ZeroType;
#if RSG_PC
	m_vClosestFreeAimTargetPosStereo = Vector3::ZeroType;
	m_vFreeAimTargetDir = Vector3::ZeroType;
	m_fClosestFreeAimTargetDist = 0.0f;
#endif
	m_vFineAimOffset = Vector3::ZeroType;
	m_fTimeFineAiming = 0.0f;
	m_fTimeLockedOn = 0.0f;
	m_bTargetSwitchedToLockOnTarget = false;
	m_fFineAimHorizontalSpeedWeight = 0.0f;
	m_bPlayerIsFineAiming = false;
	m_bPlayerReleasedUpDownControlForFineAim = false;
	m_bPlayerPressingUpDownControlForFineAim = false;
	m_bHasAttemptedNonSpringFineAim = false;
	m_bPlayerHasStartedToAim = false;
	m_uTimeSinceSoftFiringAndNotAiming = 0;
	m_bHasUpdatedLockOnPos = false;
	m_bPlayerHasBrokenLockOn = false; 
	m_bWasWithinFineAimLimits = false;
	m_bCheckForLeftTarget = false;
	m_bCheckForRightTarget = false;
	m_fTimeSoftLockedOn = 0.0f;
	m_fTimeTryingToBreakSoftLock = 0.0f;
	m_fAssistedAimDeadPedLockOnTimer = -1.0f;
	m_bVehicleHomingEnabled = true;
	m_bVehicleHomingForceDisabled = false;
	m_pFreeAimTarget = NULL;
	m_uStartTimeAimingAtFreeAimTarget = 0;
	m_pFreeAimTargetRagdoll = NULL;
	m_pFreeAimAssistTarget = NULL;
	m_vFreeAimAssistTargetPos = Vector3::ZeroType;
	m_vClosestFreeAimAssistTargetPos = Vector3::ZeroType;
	m_bFreeAimTargetingVehicleForCamera = false;
	m_fCurveSetBlend = 0.0f;
	m_fLastCameraInput = 0.0f;
	m_FreeAimProbeResults.Reset();
	m_FreeAimAssistProbeResults.Reset();
	m_fLockOnRangeOverride = -1.0f;
	m_fLostLockLOSTimer = 0.0f;
	m_uLastLockLOSTestTime = 0;
	m_bNoReticuleAimAssist = false;
	m_bBlockNoReticuleAimAssist = false;
	m_uTimeSinceLastAiming = 0;
	m_vLastValidStickInput.Zero();
	m_LastTimeDesiredStickInputSet = 0;
	m_eLastTargetingMode = None;
	m_uLastWeaponHash = 0;
	m_bNoTargetingMode = false;
	m_bTargetingStartedInWeaponWheelHUD = false;
	m_uTimeLockOnSwitchExtensionExpires = 0;
	m_pLockOnSwitchExtensionTarget = NULL;
	m_bCheckForLockOnSwitchExtension = false;
}

void CPlayerPedTargeting::Process()
{
	pedFatalAssertf(m_pPed, "m_pPed is NULL");

	m_pCachedLockOnTarget = NULL;

	// If we are assisted aiming, set the flag
	if (!m_pPed->GetIsInVehicle())
	{
		// If no reticule and trying to blind fire, give an aim assist.
		bool bRunAndGun = m_pPed->GetPlayerInfo()->IsRunAndGunning();

		if(CanUseNoReticuleAimAssist())
		{
			m_bNoReticuleAimAssist = true;
			m_bBlockNoReticuleAimAssist = true;
		}
		else if(!bRunAndGun)
		{
			m_bBlockNoReticuleAimAssist = false;
		}

		if(m_bNoReticuleAimAssist)
		{
			if(!m_pPed->GetPlayerInfo()->IsRunAndGunning() || m_fTimeAiming > ms_Tunables.GetMinNoReticuleAimTime())
			{
				m_bNoReticuleAimAssist = false;
			}

			if(m_fTimeAiming > ms_Tunables.GetMinNoReticuleAimTime())
			{
				m_bNoReticuleAimAssist = false;
			}
		}

		// Or when the weapon wheel is active as this changes the camera
		if(!CNewHud::IsShowingHUDMenu())
		{
			if (m_pPed->GetPlayerInfo()->IsAssistedAiming())
			{
				m_pPed->SetPlayerResetFlag(CPlayerResetFlags::PRF_ASSISTED_AIMING_ON);
			}
			else if(!m_pPed->GetIsInCover())
			{
				if(m_bNoReticuleAimAssist)
				{
					m_pPed->SetPlayerResetFlag(CPlayerResetFlags::PRF_ASSISTED_AIMING_ON);
					m_pPed->SetPlayerResetFlag(CPlayerResetFlags::PRF_NO_RETICULE_AIM_ASSIST_ON);
				}
				else if (bRunAndGun)
				{
					m_pPed->SetPlayerResetFlag(CPlayerResetFlags::PRF_RUN_AND_GUN);
				}
			}
		}
	}

#if __BANK
	if(CPedTargetEvaluator::DEBUG_LOCKON_TARGET_SCRORING_ENABLED)
	{
#if DEBUG_DRAW
		CTask::ms_debugDraw.ClearAll();
#endif
		CPedTargetEvaluator::EnableDebugScoring(true);
		FindLockOnTarget();
		CPedTargetEvaluator::EnableDebugScoring(false);
	}
	if(CPedTargetEvaluator::DEBUG_MELEE_TARGET_SCRORING_ENABLED)
	{
#if DEBUG_DRAW
		CTask::ms_debugDraw.ClearAll();
#endif
		CPedTargetEvaluator::EnableDebugScoring(true);
		static dev_bool s_bDoMeleeAction = true;
		FindMeleeTarget(m_pPed, GetWeaponInfo(), s_bDoMeleeAction, true);
		CPedTargetEvaluator::EnableDebugScoring(false);
	}
#endif

	m_bCheckForLeftTarget = false;
	m_bCheckForRightTarget = false;

	// If we aren't allowed to update targeting, clear the lock on target
	bool bClearLockOn = !ShouldUpdateTargeting();

	// Otherwise update the appropriate aiming mode
	if (!bClearLockOn)
	{
			pedFatalAssertf(GetWeaponInfo(), "Weapon Info is NULL");

			const CWeapon* pEquippedWeapon = m_pPed->GetWeaponManager()->GetEquippedWeapon();
			const CWeaponInfo& weaponInfo = *GetWeaponInfo();	

			//! If equipped weapon doesn't match weapon info, NULL it out as they don't match.
			if(pEquippedWeapon && (weaponInfo.GetHash() != pEquippedWeapon->GetWeaponInfo()->GetHash()))
			{
				pEquippedWeapon = NULL;
			}

			TargetingMode eTargetingMode = GetTargetingMode(pEquippedWeapon, weaponInfo);

			u32 uWeaponHash = pEquippedWeapon ? pEquippedWeapon->GetWeaponHash() : m_pPed->GetWeaponManager()->GetSelectedWeaponHash();

			//! Cache current weapon hash. It'll reset correctly when we release targeting controls.
			bool bCacheLastWeaponHash = m_eLastTargetingMode == None || m_bTargetingStartedInWeaponWheelHUD || m_pPed->GetMyVehicle();
			if(bCacheLastWeaponHash)
			{
				if(uWeaponHash != m_uLastWeaponHash)
				{
					m_uLastWeaponHash = uWeaponHash;
				}
			}

			if(CPlayerInfo::IsSoftAiming() || CPlayerInfo::IsHardAiming() || CPlayerInfo::IsDriverFiring() || (eTargetingMode != None) )
			{
				if( (uWeaponHash != m_uLastWeaponHash) || m_bNoTargetingMode)
				{
					if(weaponInfo.GetCanFreeAim())
					{
						m_pPed->GetPlayerInfo()->SetPlayerDataFreeAiming(true);
						if (!weaponInfo.GetIsOnFootHoming())
						{
							eTargetingMode = FreeAim;
						}
					}
					else
					{
						eTargetingMode = None;
					}

					m_bNoTargetingMode = true;
				}
			}
			else
			{
				m_bNoTargetingMode = false;
				m_bTargetingStartedInWeaponWheelHUD = false;
			}

			// DMKH. Check if we can promote to hard lock mode.
			if(CanPromoteToHardLock(eTargetingMode, pEquippedWeapon, weaponInfo))
			{
				eTargetingMode = HardLockOn;
			}

			// Update the time aiming variable
			if (m_pLockOnTarget || m_pPed->GetPlayerInfo()->GetPlayerDataFreeAiming())
			{
				m_fTimeAiming += fwTimer::GetTimeStep();

				//! If we have been aiming for min aim time (i.e. so we can move crosshair), flag last aim time.
				if( (m_fTimeAiming > ms_Tunables.GetMinFineAimTime()))
				{
					m_uTimeSinceLastAiming = fwTimer::GetTimeInMilliseconds();
				}
			}
			else
			{
				const camGameplayDirector& gameplayDirector	= camInterface::GetGameplayDirector();
				bool bShowReticule = gameplayDirector.ShouldDisplayReticule() || CNewHud::IsReticuleVisible();
				if(bShowReticule)
				{
					m_uTimeSinceLastAiming = fwTimer::GetTimeInMilliseconds();
				}

				m_fTimeAiming = 0.0f;
			}

			// Update the free aim status
			m_pPed->GetPlayerInfo()->SetPlayerDataFreeAiming(eTargetingMode == FreeAim ? true : false);

			switch (eTargetingMode)
			{
				case HardLockOn:
					{
						bClearLockOn = !UpdateHardLockOn(pEquippedWeapon, weaponInfo);

						UpdateFreeAim(weaponInfo);
					}
					break;
				case SoftLockOn:
					{
						if (m_fTimeSoftLockedOn == 0.0f)
						{
							m_bWasWithinFineAimLimits = false;
						}

						bClearLockOn = !UpdateSoftLockOn(pEquippedWeapon, weaponInfo);
						if (bClearLockOn && m_pLockOnTarget)
						{
							// When initially clearing lockon, make it the freeaim target
							// (Can't just update free aim as probes are asynchronous)
							m_pFreeAimTarget = m_pLockOnTarget;
							m_pFreeAimTargetRagdoll = m_pLockOnTarget;
						}
						//try to set a free aim target if aiming and not locking
						UpdateFreeAim(weaponInfo);
					}
					break;
				case FreeAim:
					{
						bClearLockOn = !UpdateFreeAim(weaponInfo);
					}
					break;
				case None: 
					{
						if(CNewHud::IsReticuleVisible())
						{
							bClearLockOn = !UpdateFreeAim(weaponInfo);
						}
						else
						{
							bClearLockOn = true;
							// Clear Free aim targets when not aiming
							ClearFreeAim();		
						}
					}
					break;
				case Manual:
					if (weaponInfo.GetIsOnFootHoming() && !weaponInfo.GetIsVehicleWeapon())
					{
						// Don't need to set bClearLockOn flag here as we handle clearing the lock on target within the ProcessOnFootHomingLockOn function.
						ProcessOnFootHomingLockOn(m_pPed);
						// B*2165686: Only update the free aim target if we're aiming (prevents us from triggering ped reaction events when we aren't aiming).
						if (CPlayerInfo::IsAiming())
						{
							// B*2154594: Try to set free aim target if possible to ensure we get ped reaction events
							UpdateFreeAim(weaponInfo);
						}
						else
						{
							// BlairT: Clear free aim target/pos so we don't get stuck with stale data.
							ClearFreeAim();
							m_vClosestFreeAimTargetPos = Vector3::ZeroType;
						}
					}
					else
					{
						// BlairT: Clear free aim target/pos so we don't get stuck with stale data.
						ClearFreeAim();
						m_vClosestFreeAimTargetPos = Vector3::ZeroType;
					}
					break;
				default: break;
			}

			//! Have we started to target when in weapon wheel?
			if(m_eLastTargetingMode == None && (eTargetingMode != None) && CNewHud::IsShowingHUDMenu())
			{
				m_bTargetingStartedInWeaponWheelHUD = true;
			}

			//! Cache targeting mode
			m_eLastTargetingMode = eTargetingMode;

			CControl* pControl = const_cast<CControl*>(m_pPed->GetControlFromPlayer());
			if( pControl )
			{
				// Get desired stick direction for melee lock on.
				const Vector2 vStickInput(pControl->GetPedWalkLeftRight().GetNorm(), -pControl->GetPedWalkUpDown().GetNorm());
				if( vStickInput.Mag2() > rage::square( 0.5f ) )
				{
					m_LastTimeDesiredStickInputSet = fwTimer::GetTimeInMilliseconds();
					m_vLastValidStickInput = vStickInput;
				}
			}
		}
	else
	{
		m_fTimeAiming = 0.0f;
	}

	if (bClearLockOn)
	{
		ClearLockOnTarget();
	}

	if(m_pPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_NO_RETICULE_AIM_ASSIST_ON) && !m_pLockOnTarget )
	{
		m_pPed->ClearPlayerResetFlag(CPlayerResetFlags::PRF_ASSISTED_AIMING_ON);
		m_pPed->ClearPlayerResetFlag(CPlayerResetFlags::PRF_NO_RETICULE_AIM_ASSIST_ON);
	};

	// Send gun aimed at events to targeted peds
	ProcessPedEvents();

	m_fLockOnRangeOverride = -1.0f;		// Clear the weapon range override - needs to be set every frame from script

#if __BANK
	TUNE_GROUP_BOOL(PLAYER_TARGETING, bDebugBulletBending, false);
	if (bDebugBulletBending)
	{
		const CPedWeaponManager* pWeaponManager = m_pPed->GetWeaponManager();
		if (pWeaponManager)
		{
			const CWeapon* pWeapon = pWeaponManager->GetEquippedWeapon();
			if (pWeapon && pWeapon->GetWeaponInfo() && pWeapon->GetWeaponInfo()->GetIsGunOrCanBeFiredLikeGun())
			{
				const CObject* pWeaponObject = pWeaponManager->GetEquippedWeaponObject();
				if (pWeaponObject)
				{
					const bool bCachedDebugTargettingValue = CPedTargetEvaluator::ms_Tunables.m_DebugTargetting;
					CPedTargetEvaluator::ms_Tunables.m_DebugTargetting = true;
					GetIsFreeAimAssistTargetPosWithinConstraints(pWeapon, RCC_MATRIX34(pWeaponObject->GetMatrixRef()), CPlayerPedTargeting::IsCameraInAccurateMode(), FIRING_VEC_CALC_FROM_AIM_CAM);
					CPedTargetEvaluator::ms_Tunables.m_DebugTargetting = bCachedDebugTargettingValue;
				}
			}
		}
	}
#endif	// __BANK
}

void CPlayerPedTargeting::ProcessPreCamera()
{
	if(m_pPed && m_pPed->IsLocalPlayer())
	{
		UpdateLockOnTargetPos();
	}
}

const CEntity* CPlayerPedTargeting::GetTarget() const
{
	if(m_pLockOnTarget)
	{
		// The lock-on target is definitive
		return m_pLockOnTarget;
	}
	else if(m_pFreeAimTarget)
	{
		return m_pFreeAimTarget;
	}
	else if(m_pFreeAimAssistTarget)
	{
		return m_pFreeAimAssistTarget;
	}

	return NULL;
}

CEntity* CPlayerPedTargeting::GetTarget()
{
	if(m_pLockOnTarget)
	{
		// The lock-on target is definitive
		return m_pLockOnTarget;
	}
	else if(m_pFreeAimTarget)
	{
		return m_pFreeAimTarget;
	}
	else if(m_pFreeAimAssistTarget)
	{
		return m_pFreeAimAssistTarget;
	}

	return NULL;
}

void CPlayerPedTargeting::SetLockOnTarget(CEntity* pTarget, bool bTargetSwitched)
{
	m_pLockOnTarget = pTarget;
	m_bTargetSwitchedToLockOnTarget = bTargetSwitched;

	if(m_pLockOnTarget)
	{
		m_pLockOnTarget->GetLockOnTargetAimAtPos(m_vLastLockOnTargetPos);

#if USE_TARGET_SEQUENCES
		TUNE_GROUP_BOOL(PLAYER_TARGETING, ASSISTED_AIMING_USE_TARGET_SEQUENCES, true);
		// Update the lock-on position
		atHashWithStringNotFinal targetSequenceGroup = GetTargetSequenceGroup();
		if ( ASSISTED_AIMING_USE_TARGET_SEQUENCES && m_pPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_ASSISTED_AIMING_ON) && targetSequenceGroup.GetHash() && m_pLockOnTarget->GetIsTypePed())
		{
			// TODO - pick a sequence group based on the weapon type (probably from weapon metadata)
			m_TargetSequenceHelper.InitSequence(targetSequenceGroup, m_pPed, static_cast<CPed*>(m_pLockOnTarget.Get()));
		}
		else if (m_pPed->GetIsDrivingVehicle() || !m_pPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_ASSISTED_AIMING_ON))
#else // USE_TARGET_SEQUENCES
		//if (m_pPed->GetIsDrivingVehicle() || !m_pPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_ASSISTED_AIMING_ON))
#endif // USE_TARGET_SEQUENCES
		{
			m_vLockOnTargetPos = m_vLastLockOnTargetPos;
		}
	}
	else
	{
		// No target, clear lock-on position
		m_vLockOnTargetPos.Zero();
		m_vLastLockOnTargetPos.Zero();

		TARGET_SEQUENCE_ONLY(m_TargetSequenceHelper.Reset());

		m_bHasUpdatedLockOnPos = false;
	}

	m_fTimeLockedOn = 0.0f;

	// Clear the fine aim variables
	ClearFineAim();
}

void CPlayerPedTargeting::ClearLockOnTarget()
{
	pedFatalAssertf(m_pPed, "m_pPed is NULL");

	if(m_pPed->IsControlledByLocalPlayer())
	{
		m_pLockOnTarget = NULL;
		m_vLockOnTargetPos.Zero();
		m_vLastLockOnTargetPos.Zero();
		m_bHasUpdatedLockOnPos = false;
		TARGET_SEQUENCE_ONLY(m_TargetSequenceHelper.Reset();)
		m_fLostLockLOSTimer = 0.0f;
		m_uLastLockLOSTestTime = 0;
		m_bNoReticuleAimAssist = false;
		m_fTimeLockedOn = 0.0f;
		m_bTargetSwitchedToLockOnTarget = false;
	}
}

bool CPlayerPedTargeting::GetFineAimingOffset(Vector3& vFineAimOffset) const
{
	vFineAimOffset = camInterface::GetPlayerControlCamAimFrame().GetRight() * m_vFineAimOffset.x;
	vFineAimOffset.z = m_vFineAimOffset.z;

	return m_bPlayerIsFineAiming;
}

#if !__FINAL
PARAM(enableCNCFreeAimImprovements, "[CPlayerPedTargeting] Enable CNC free aim improvements.");
PARAM(forceEnableCNCFreeAimImprovements, "[CPlayerPedTargeting] Force enable CNC free aim improvements (even if not in the mode).");
#endif	// !__FINAL

bool CPlayerPedTargeting::GetAreCNCFreeAimImprovementsEnabled(const CPed* CNC_MODE_ENABLED_ONLY(pPed), const bool CNC_MODE_ENABLED_ONLY(bControllerOnly))
{
#if CNC_MODE_ENABLED
	if (pPed)
	{
		const CPlayerInfo* pPlayerInfo = pPed->GetPlayerInfo();
		if (pPlayerInfo)
		{
			return pPlayerInfo->GetTargeting().GetAreCNCFreeAimImprovementsEnabled(bControllerOnly);
		}
	}	
#endif	// CNC_MODE_ENABLED

	return false;
}

bool CPlayerPedTargeting::GetAreCNCFreeAimImprovementsEnabled(const bool CNC_MODE_ENABLED_ONLY(bControllerOnly)) const
{
#if CNC_MODE_ENABLED

#if !__FINAL
	TUNE_GROUP_BOOL(PLAYER_TARGETING, bForceCnCFreeAimImprovements, false);
	if (bForceCnCFreeAimImprovements || PARAM_forceEnableCNCFreeAimImprovements.Get())
	{
		return true;
	}
#endif	// !__FINAL

	TUNE_GROUP_BOOL(PLAYER_TARGETING, bEnableCnCFreeAimImprovements, true);
	if ((bEnableCnCFreeAimImprovements || PARAM_enableCNCFreeAimImprovements.Get()) && NetworkInterface::IsInCopsAndCrooks())
	{
		if (bControllerOnly)
		{
#if RSG_PC
			const CControl* pControl = m_pPed->GetControlFromPlayer();
			if(pControl && pControl->UseRelativeLook())
			{
				return false;
			}
#endif	// RSG_PC
		}

		const u32 uTargetMode = CPedTargetEvaluator::GetTargetMode(m_pPed);
		if (uTargetMode == CPedTargetEvaluator::TARGETING_OPTION_FREEAIM || uTargetMode == CPedTargetEvaluator::TARGETING_OPTION_ASSISTED_FREEAIM)
		{
			return true;
		}
	}

#endif	// CNC_MODE_ENABLED

	return false;
}

bool CPlayerPedTargeting::GetIsFreeAimAssistTargetPosWithinConstraints(const CWeapon* pWeapon, const Matrix34& mWeapon, bool bZoomed, eFiringVecCalcType eFireVecType) const
{
	pedFatalAssertf(m_pPed, "m_pPed is NULL");

	if(m_pFreeAimAssistTarget)
	{
		if(pWeapon)
		{
			Vector3 vStart, vEnd;

			switch (eFireVecType)
			{
				// TODO: Fix up the parameters for unused types
				case FIRING_VEC_CALC_FROM_POS:
					pWeapon->CalcFireVecFromPos(m_pPed, mWeapon, vStart, vEnd, VEC3V_TO_VECTOR3(m_pFreeAimAssistTarget->GetTransform().GetPosition()));
					break;
				case FIRING_VEC_CALC_FROM_ENT:
					pWeapon->CalcFireVecFromEnt(m_pPed, mWeapon, vStart, vEnd, m_pFreeAimAssistTarget);
					break;
				case FIRING_VEC_CALC_FROM_AIM_CAM:
					pWeapon->CalcFireVecFromAimCamera(m_pPed->GetMyVehicle() != NULL ? static_cast<CEntity*>(m_pPed->GetMyVehicle()) : m_pPed, mWeapon, vStart, vEnd);
					break;
				case FIRING_VEC_CALC_FROM_GUN_ORIENTATION:
					pWeapon->CalcFireVecFromGunOrientation(m_pPed, mWeapon, vStart, vEnd);
					break;
				case FIRING_VEC_CALC_FROM_WEAPON_MATRIX:
					{
						// Hack to use bullet bending on jetpack lasers 
						pWeapon->CalcFireVecFromWeaponMatrix(mWeapon, vStart, vEnd);
					}
					break;
				default:
					taskAssertf(0,"Unhandled firing vector calc type");
			}
	
			return GetIsFreeAimAssistTargetPosWithinConstraints(pWeapon, vStart, vEnd, bZoomed);
		}
	}

	return false;
}

bool CPlayerPedTargeting::GetIsFreeAimAssistTargetPosWithinConstraints(const CWeapon* pWeapon, const Vector3& vStart, Vector3& vEnd, bool bZoomed) const
{
	pedFatalAssertf(m_pPed, "m_pPed is NULL");

	if(m_pFreeAimAssistTarget)
	{
		if(pWeapon)
		{
			float fNearRadius = 0.0f;
			float fFarRadius = 0.0f;
			GetBulletBendingData(m_pPed, pWeapon, bZoomed, fNearRadius, fFarRadius);

			fwGeom::fwLine line(vStart, vEnd);

			Vector3 vClosestPoint;
			float fT = line.FindClosestPointOnLine(m_vFreeAimAssistTargetPos, vClosestPoint);

			float fRadiusAtDistance = fNearRadius + fT * (fFarRadius - fNearRadius);
			// If distance from closest point on aiming vector to free aim assist target position is within the radius
			bool bWithinConstraints = m_vFreeAimAssistTargetPos.Dist2(vClosestPoint) < rage::square(fRadiusAtDistance);
#if __BANK
			RenderAimAssistConstraints(bWithinConstraints, vStart, vEnd, vClosestPoint, fRadiusAtDistance);
#endif // __BANK
			return bWithinConstraints;
		}
	}

	return false;
}

bool CPlayerPedTargeting::GetIsFreeAimAssistTargetPosWithinReticuleSlowDownConstraints(const CWeapon* pWeapon, const Vector3& vStart, Vector3& vEnd, bool bZoomed) const
{
	if (ms_Tunables.GetUseCapsuleTests())
	{
		return GetIsFreeAimAssistTargetPosWithinReticuleSlowDownConstraintsNew(pWeapon, vStart, vEnd, bZoomed);
	}
	if(m_pFreeAimAssistTarget)
	{
		if(pWeapon)
		{
#if __BANK
			float fNearRadius = 0.0f;
			float fFarRadius = 0.0f;
			GetBulletBendingData(m_pPed, pWeapon, bZoomed, fNearRadius, fFarRadius);

			fwGeom::fwLine line(vStart, vEnd);
			Vector3 vClosestPoint;
			float fT = line.FindClosestPointOnLine(m_vFreeAimAssistTargetPos, vClosestPoint);
			float fRadiusAtDistance = fNearRadius + fT * (fFarRadius - fNearRadius);
			// If distance from closest point on aiming vector to free aim assist target position is within the radius
			bool bWithinConstraints = m_vFreeAimAssistTargetPos.Dist2(vClosestPoint) < rage::square(fRadiusAtDistance);
			RenderAimAssistConstraints(bWithinConstraints, vStart, vEnd, vClosestPoint, fRadiusAtDistance);
#endif // __BANK

			Vector3 vOne = vStart;
			Vector3 vTwo = vEnd;

			const camFrame& aimFrame = camInterface::GetPlayerControlCamAimFrame();
			Matrix34 mFreeAimAssistMtx = aimFrame.GetWorldMatrix();
			mFreeAimAssistMtx.d = VEC3V_TO_VECTOR3(m_pFreeAimAssistTarget->GetTransform().GetPosition());
			mFreeAimAssistMtx.UnTransform(vOne);
			mFreeAimAssistMtx.UnTransform(vTwo);

			Vector3 vOneToTwo = vTwo - vOne;
			float fUnused = 0.0f;
			if (geomSegments::SegmentToDiskIntersection(vOne, vOneToTwo, ms_Tunables.GetReticuleSlowDownRadius(), &fUnused))
			{
				return true;
			}
		}
	}

	return false;
}

bool CPlayerPedTargeting::GetIsFreeAimAssistTargetPosWithinReticuleSlowDownConstraintsNew(const CWeapon* pWeapon, const Vector3& vStart, Vector3& vEnd, bool BANK_ONLY(bZoomed)) const
{
	if(m_pFreeAimAssistTarget)
	{
		if(pWeapon)
		{
#if __BANK
			float fNearRadius = 0.0f;
			float fFarRadius = 0.0f;
			GetBulletBendingData(m_pPed, pWeapon, bZoomed, fNearRadius, fFarRadius);

			fwGeom::fwLine line(vStart, vEnd);
			Vector3 vClosestPoint;
			float fT = line.FindClosestPointOnLine(m_vFreeAimAssistTargetPos, vClosestPoint);
			float fRadiusAtDistance = fNearRadius + fT * (fFarRadius - fNearRadius);
			// If distance from closest point on aiming vector to free aim assist target position is within the radius
			bool bWithinConstraints = m_vFreeAimAssistTargetPos.Dist2(vClosestPoint) < rage::square(fRadiusAtDistance);
			RenderAimAssistConstraints(bWithinConstraints, vStart, vEnd, vClosestPoint, fRadiusAtDistance);
#endif // __BANK

			Vector3 vOne = vStart;
			Vector3 vTwo = vEnd;

			const camFrame& aimFrame = camInterface::GetPlayerControlCamAimFrame();
			Matrix34 mFreeAimAssistMtx = aimFrame.GetWorldMatrix();
			mFreeAimAssistMtx.d = VEC3V_TO_VECTOR3(m_pFreeAimAssistTarget->GetTransform().GetPosition());
			mFreeAimAssistMtx.UnTransform(vOne);
			mFreeAimAssistMtx.UnTransform(vTwo);

			// geomSegments::SegmentCapsuleIntersectDirected assumes the capsule is lying on the y axis (length) so we need to rotate the aim vector accordingly
			static float ROTATE_X = HALF_PI;
			vOne.RotateX(ROTATE_X);
			vTwo.RotateX(ROTATE_X);

			float fLength = ms_Tunables.GetReticuleSlowDownCapsuleLength();
			float fRadius = ms_Tunables.GetReticuleSlowDownCapsuleRadius();

			/*if(m_fCurveSetBlend > 0.0f)
			{
				static dev_float s_Scale = 1.2f;
				fLength *= s_Scale;
				fRadius *= s_Scale;
			}*/

			Vector3 vOneToTwo = vTwo - vOne;
			Vec3V vPositionOnCapsule;
			Vec3V vNormalOnCapsule;
			ScalarV scFractionAlongSegment;
			bool bIntersectionFound = geomSegments::SegmentCapsuleIntersectDirected(RCC_VEC3V(vOne), RCC_VEC3V(vOneToTwo), ScalarVFromF32(fLength), ScalarVFromF32(fRadius), vPositionOnCapsule, vNormalOnCapsule, scFractionAlongSegment).Getb();
			if (bIntersectionFound)
			{
				return true;
			}
		}
	}

	return false;
}

#if __BANK
void CPlayerPedTargeting::RenderAimAssistConstraints(bool bWithinConstraints, const Vector3& vStart, const Vector3& vEnd, const Vector3& vClosestPoint, float fRadiusAtDistance) const
{
	if(CPedTargetEvaluator::ms_Tunables.m_DebugTargetting)
	{
		Color32 debugColour = bWithinConstraints ? Color_green : Color_red;
		static const u32 uDebugPartialHash = atPartialStringHash("AIM_ASSIST_CIRCLE_");

		CWeaponDebug::ms_debugStore.AddLine(RCC_VEC3V(vStart), RCC_VEC3V(vEnd), debugColour, 1000, atStringHash("TargetLine", uDebugPartialHash));

		Vec3V vShotDir(vEnd - vStart);
		vShotDir = NormalizeFast(vShotDir);
		Vec3V vRight, vUp;
		vRight = CrossZAxis(vShotDir);
		vRight = NormalizeFast(vRight);
		vUp = Cross(vRight, vShotDir);
		vUp = NormalizeFast(vUp);

		const s32 NUM_POINTS = 12;
		float fAngleIncrement = 2.0f * PI / NUM_POINTS;
		float fRotAngle = 0;
		ScalarV fRadiusAtDistanceV = ScalarVFromF32(fRadiusAtDistance);

		for(s32 i = 1; i <= NUM_POINTS; i++)
		{
			Vec3V a(vClosestPoint);
			a += fRadiusAtDistanceV * vRight * ScalarVFromF32(rage::Sinf(fRotAngle));
			a += fRadiusAtDistanceV * vUp * ScalarVFromF32(rage::Cosf(fRotAngle));

			fRotAngle = fAngleIncrement * i;

			Vec3V b(vClosestPoint);
			b += fRadiusAtDistanceV * vRight * ScalarVFromF32(rage::Sinf(fRotAngle));
			b += fRadiusAtDistanceV * vUp * ScalarVFromF32(rage::Cosf(fRotAngle));

			char buff[3];
			sprintf(buff, "%d", i);
			CWeaponDebug::ms_debugStore.AddLine(a, b, debugColour, 1000, atStringHash(buff, uDebugPartialHash));
		}
	}
}
#endif // __BANK

bool CPlayerPedTargeting::GetScaledLookAroundInputMagForCamera(const float fCurrentLookAroundInputMag, float& fScaledLookAroundInputMag)
{
	pedFatalAssertf(m_pPed, "m_pPed is NULL");

	if (!ms_Tunables.GetUseReticuleSlowDown())
	{
		return false;
	}
	
	if (m_pPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_RUN_AND_GUN) && !ms_Tunables.GetUseReticuleSlowDownForRunAndGun())
	{
		return false;
	}

	bool bInAccurateMode = IsCameraInAccurateMode();
	bool bValidCam = false;
	if(camInterface::GetGameplayDirector().GetThirdPersonAimCamera() FPS_MODE_SUPPORTED_ONLY(|| camInterface::GetGameplayDirector().GetFirstPersonShooterCamera()) )
	{
		bValidCam = true;
	}

	if(bValidCam)
	{
		weaponAssert(m_pPed->GetWeaponManager());
		const CWeapon* pWeapon = m_pPed->GetWeaponManager()->GetEquippedWeapon();
		if(pWeapon && pWeapon->GetWeaponInfo()->GetIsGunOrCanBeFiredLikeGun())
		{
			CObject* pObject = m_pPed->GetWeaponManager()->GetEquippedWeaponObject();
			if (pObject)
			{
				const Matrix34& mWeapon = RCC_MATRIX34(pObject->GetMatrixRef());

				if (ms_Tunables.GetUseNewSlowDownCode())
				{
					Vector3 vStart(Vector3::ZeroType);
					Vector3 vEnd(Vector3::ZeroType);
					if (m_pFreeAimAssistTarget && pWeapon->CalcFireVecFromAimCamera(m_pPed->GetMyVehicle() != NULL ? static_cast<CEntity*>(m_pPed->GetMyVehicle()) : m_pPed, mWeapon, vStart, vEnd) && GetIsFreeAimAssistTargetPosWithinReticuleSlowDownConstraints(pWeapon, vStart, vEnd, bInAccurateMode))
					{
						m_fCurveSetBlend += fwTimer::GetTimeStep() * (1.0f/ms_Tunables.GetAimAssistBlendInTime());
					}
					else
					{
						m_fCurveSetBlend -= fwTimer::GetTimeStep() * (1.0f/ms_Tunables.GetAimAssistBlendOutTime());
					}
				}
				else
				{
					if((m_pFreeAimAssistTarget && GetIsFreeAimAssistTargetPosWithinConstraints(pWeapon, mWeapon, bInAccurateMode, FIRING_VEC_CALC_FROM_AIM_CAM)) ||
						(ms_Tunables.m_UseRagdollTargetIfNoAssistTarget && m_pFreeAimTarget))
					{
						m_fCurveSetBlend += fwTimer::GetTimeStep() * (1.0f/ms_Tunables.GetAimAssistBlendInTime());
					}
					else
					{
						m_fCurveSetBlend -= fwTimer::GetTimeStep() * (1.0f/ms_Tunables.GetAimAssistBlendOutTime());
					}
				}

				m_fCurveSetBlend = Clamp(m_fCurveSetBlend, 0.0f, 1.0f);

				//! If we are strafing, need to clamp our scaled input so that it doesn't get to low as the reticule can "bounce"
				//! off our free aim assist target.
				Vector2 vMoveBlendRatio;
				m_pPed->GetMotionData()->GetCurrentMoveBlendRatio(vMoveBlendRatio);
				float fAimAssistDistanceResult = 1.0f;
				float fMinScaleInput = 0.0f;
				float fBlend = m_fCurveSetBlend;
				if(abs(vMoveBlendRatio.x) > 0.0f)
				{
					vMoveBlendRatio.NormalizeSafe();
					float fStrafeScale = abs(vMoveBlendRatio.x);
	
					if(m_pFreeAimAssistTarget)
					{
						Vector3 vLockOnPos = VEC3V_TO_VECTOR3(m_pFreeAimAssistTarget->GetTransform().GetPosition());
						Vector3 vPedPos = VEC3V_TO_VECTOR3(m_pPed->GetTransform().GetPosition());
						Vector3 vDistance = vLockOnPos - vPedPos;
						float fDistanceToFreeAimAssistTarget = vDistance.XYMag();
						fAimAssistDistanceResult = fStrafeScale * GetAimDistanceScaleCurve()->GetResult(fDistanceToFreeAimAssistTarget);
					
						if(ms_Tunables.m_UseReticuleSlowDownStrafeClamp)
						{
							fMinScaleInput = fAimAssistDistanceResult;
						}
						else
						{
							fBlend *= fAimAssistDistanceResult;
						}
					}
				}

				if(bInAccurateMode)
				{
					float fAimZoomValue = GetCurveSet(CTargettingDifficultyInfo::CS_AimZoom)->GetResult(fCurrentLookAroundInputMag);
					float fAimAssistZoomValue = GetCurveSet(CTargettingDifficultyInfo::CS_AimAssistZoom)->GetResult(fCurrentLookAroundInputMag);
					fScaledLookAroundInputMag = Lerp(fBlend, fAimZoomValue, fAimAssistZoomValue);
				}
				else
				{
					float fAimValue = GetCurveSet(CTargettingDifficultyInfo::CS_Aim)->GetResult(fCurrentLookAroundInputMag);
					float fAimAssistValue = GetCurveSet(CTargettingDifficultyInfo::CS_AimAssist)->GetResult(fCurrentLookAroundInputMag);
					fScaledLookAroundInputMag = Lerp(fBlend, fAimValue, fAimAssistValue);
				}

				if(ms_Tunables.m_UseReticuleSlowDownStrafeClamp && (fScaledLookAroundInputMag < fMinScaleInput))
				{
					fScaledLookAroundInputMag = Min(fMinScaleInput, fCurrentLookAroundInputMag);
				}

				//! Ensure we haven't increased look around input.
				taskAssert(fScaledLookAroundInputMag <= fCurrentLookAroundInputMag);

				m_fLastCameraInput = fCurrentLookAroundInputMag;
				return true;
			}
		}
	}

	return false;
}

#if __BANK
// void CPlayerPedTargeting::AddWidgets(bkBank& bank))
// {
// 	bank.PushGroup("SNAPSHOT", false);
// 		bank.AddToggle("Enable bullet bend",	&CPlayerPedTargeting::ms_bEnableSnapShotBulletBend);
// 		bank.AddSlider("Min radius bullet bend influence", & CPlayerPedTargeting::ms_fSnapShotRadiusMin, 0.0f, 20.0f, 0.1f);
// 		bank.AddSlider("Max radius bullet bend influence", & CPlayerPedTargeting::ms_fSnapShotRadiusMax, 0.0f, 20.0f, 0.1f);
// 	bank.PopGroup();
// }

void CPlayerPedTargeting::Tunables::SetEntityTargetableDistCB()
{
	CEntity* pFocusEntity = CDebugScene::FocusEntities_Get(0);
	if (pFocusEntity)
	{
		pFocusEntity->SetLockOnTargetableDistance(ms_Tunables.m_fTargetableDistance);
	}
}

void CPlayerPedTargeting::Tunables::SetEntityTargetThreatCB()
{
	CEntity* pFocusEntity = CDebugScene::FocusEntities_Get(0);
	if (pFocusEntity && pFocusEntity->GetIsTypePed())
	{
		((CPed*)pFocusEntity)->SetTargetThreatOverride(ms_Tunables.m_fTargetThreatOverride);
	}
}

void CPlayerPedTargeting::Debug()
{
	if ( ms_Tunables.m_DisplayFreeAimTargetDebug )
	{
		grcDebugDraw::Sphere(m_vClosestFreeAimTargetPos, 0.1f, Color_blue);

		Vector3 vTargeterPosition = VEC3V_TO_VECTOR3(m_pPed->GetTransform().GetPosition());
		grcDebugDraw::Line(vTargeterPosition, m_vClosestFreeAimTargetPos, Color_green);
	}

	if( ms_Tunables.m_DisplaySoftLockDebug && (ms_Tunables.GetLockType() == LT_Soft) )
	{
		grcDebugDraw::AddDebugOutput(Color_green, "Time Soft Locked : %.2f", m_fTimeSoftLockedOn);

		if (m_bPlayerHasStartedToAim)
			grcDebugDraw::AddDebugOutput(Color_green, "Started Soft Aiming : true");
		else
			grcDebugDraw::AddDebugOutput(Color_green, "Started Soft Aiming : false");

		if (m_bPlayerHasBrokenLockOn)
			grcDebugDraw::AddDebugOutput(Color_green, "Broken Lock On : true");
		else
			grcDebugDraw::AddDebugOutput(Color_green, "Broken Lock On : false");

		const CControl* pControl = m_pPed->GetControlFromPlayer();
		if (pControl)
		{
			Vector2 vecStickInput;
			// * 128.0f to convert to old range. We could have just printed text but it is in a different range from other relevent text.
			vecStickInput.x = pControl->GetPedAimWeaponLeftRight().GetNorm() * 128.0f;
			vecStickInput.y = pControl->GetPedAimWeaponUpDown().GetNorm() * 128.0f;
			grcDebugDraw::AddDebugOutput(Color_green, "X: %.2f, Y: %2f.", vecStickInput.x, vecStickInput.y);
		}	

		Vector3 vFineAim;
		GetFineAimingOffset(vFineAim);
		grcDebugDraw::AddDebugOutput(Color_green, "Fine Aim Offset: (%.2f,%.2f,%.2f)", vFineAim.x, vFineAim.y, vFineAim.z);	
	}

	if(ms_Tunables.m_DebugLockOnTargets)
	{
		const CWeaponInfo* pWeaponInfo = GetWeaponInfo();
		if(pWeaponInfo && pWeaponInfo->GetIsGunOrCanBeFiredLikeGun())
		{
			Vector3 vTargeterPosition = VEC3V_TO_VECTOR3(m_pPed->GetTransform().GetPosition());
			const float fWeaponRange = GetWeaponLockOnRange(pWeaponInfo, m_pLockOnTarget);
			const float fLockOnRange = fWeaponRange * ms_Tunables.GetLockOnRangeModifier();
			const CPedScanner& pedScanner = *(m_pPed->GetPedIntelligence()->GetPedScanner());
			const CEntityScannerIterator entityList = pedScanner.GetIterator();
			for (const CEntity* pEnt = static_cast<const CPed*>(entityList.GetFirst()); pEnt; pEnt = entityList.GetNext())
			{
				const CPed* pNearbyPed = static_cast<const CPed*>(pEnt);
				if (pNearbyPed)
				{
					Vector3 vTargetPosition = VEC3V_TO_VECTOR3(pNearbyPed->GetTransform().GetPosition());
					pNearbyPed->GetLockOnTargetAimAtPos(vTargetPosition);
					Vector3 vDelta = vTargetPosition - vTargeterPosition;
					float fDistanceSq = vDelta.Mag2();
					grcDebugDraw::Line(vTargetPosition, vTargeterPosition, fDistanceSq > rage::square(fLockOnRange) ? Color_red : Color_green);
					vTargetPosition.z += 1.0f;
					grcDebugDraw::Sphere(vTargetPosition, 0.05f, fDistanceSq > rage::square(fLockOnRange) ? Color_red : Color_green);
					CPedTargetEvaluator::tHeadingLimitData headingData;
					headingData.fHeadingAngularLimit = ms_Tunables.GetDefaultTargetAngularLimit();
					headingData.fHeadingAngularLimitClose = ms_Tunables.GetDefaultTargetAngularLimitClose();
					headingData.fHeadingAngularLimitCloseDistMin = ms_Tunables.GetDefaultTargetAngularLimitCloseDistMin();
					headingData.fHeadingAngularLimitCloseDistMax = ms_Tunables.GetDefaultTargetAngularLimitCloseDistMax();
					const float fMaxHeadingDifference = (DtoR * headingData.GetAngularLimitForDistance(rage::Sqrtf(fDistanceSq)));
					Vector3 vHeading = camInterface::GetPlayerControlCamAimFrame().GetFront();
					vDelta.Normalize();
					const float fOrientation = rage::Atan2f(-vDelta.x, vDelta.y);
					const float fOrientationDiff = fOrientation - rage::Atan2f( -vHeading.x, vHeading.y );
					vTargetPosition.z += 0.1f;
					grcDebugDraw::Sphere(vTargetPosition, 0.05f, Abs(fOrientationDiff) > fMaxHeadingDifference ? Color_red : Color_green);

					Vector3 vLockonTargetPos = GetLockonTargetPos();
					vLockonTargetPos.z += 1.5f;
					grcDebugDraw::Sphere(vLockonTargetPos, 0.05f, Color_blue);
				}
			}
		}
	}

	if(CPedTargetEvaluator::ms_Tunables.m_DebugTargetting)
	{
		// Call the lock-on search code in order to display the debug
		//GetLockOnTarget();
		// If GetLockOnTarget returns a CEntity then no need to clear it here
		//ClearLockOnTarget();

		if(!ms_Tunables.m_DisplayAimAssistIntersections)
		{
			if(m_pFreeAimTarget)
			{
				grcDebugDraw::Sphere(VEC3V_TO_VECTOR3(m_pFreeAimTarget->GetTransform().GetPosition()), 1.0f, Color_red, false, -1);
			}

			if(m_pFreeAimTargetRagdoll)
			{
				grcDebugDraw::Sphere(VEC3V_TO_VECTOR3(m_pFreeAimTargetRagdoll->GetTransform().GetPosition()), 1.0f, Color_blue, false, -1);
			}
		
			if(m_pFreeAimAssistTarget)
			{
				grcDebugDraw::Sphere(m_vFreeAimAssistTargetPos, 0.1f, Color_green, true, -1);
			}
		}

		if (ms_Tunables.GetUseNewSlowDownCode() && (ms_Tunables.GetLockType() != LT_None) )
		{
			if (m_pFreeAimAssistTarget)
			{
				weaponAssert(m_pPed->GetWeaponManager());
				const CWeapon* pWeapon = m_pPed->GetWeaponManager()->GetEquippedWeapon();
				if(pWeapon && pWeapon->GetWeaponInfo()->GetIsGunOrCanBeFiredLikeGun())
				{
					CObject* pObject = m_pPed->GetWeaponManager()->GetEquippedWeaponObject();
					if (pObject)
					{
						const Matrix34& mWeapon = RCC_MATRIX34(pObject->GetMatrixRef());
						Vector3 vStart(Vector3::ZeroType);
						Vector3 vEnd(Vector3::ZeroType);
						if (pWeapon->CalcFireVecFromAimCamera(m_pPed->GetMyVehicle() != NULL ? static_cast<CEntity*>(m_pPed->GetMyVehicle()) : m_pPed, mWeapon, vStart, vEnd))
						{
							const camFrame& aimFrame = camInterface::GetPlayerControlCamAimFrame();
							Vector3 vUp = aimFrame.GetUp();
							Vector3 vSide = aimFrame.GetRight();
							Vec3V vTargetPos = m_pFreeAimAssistTarget->GetTransform().GetPosition();
							Vec3V vLengthOffset = VECTOR3_TO_VEC3V(vUp * ms_Tunables.GetReticuleSlowDownCapsuleLength() * 0.5f);
							Vec3V vCapsuleStart = vTargetPos - vLengthOffset;
							Vec3V vCapsuleEnd = vTargetPos + vLengthOffset;

							if (GetIsFreeAimAssistTargetPosWithinReticuleSlowDownConstraints(pWeapon, vStart, vEnd, false))
							{
								grcDebugDraw::Line(RCC_VEC3V(vStart), RCC_VEC3V(vEnd), Color_green, -1);
								if (ms_Tunables.GetUseCapsuleTests())
									grcDebugDraw::Capsule(vCapsuleStart, vCapsuleEnd, ms_Tunables.GetReticuleSlowDownCapsuleRadius(), Color_green, false, -1);
								else
									grcDebugDraw::Circle(m_pFreeAimAssistTarget->GetTransform().GetPosition(), ms_Tunables.GetReticuleSlowDownRadius(), Color_green, RCC_VEC3V(vSide), RCC_VEC3V(vUp), false, false, -1);
							}
							else
							{
								grcDebugDraw::Line(RCC_VEC3V(vStart), RCC_VEC3V(vEnd), Color_red, -1);
								if (ms_Tunables.GetUseCapsuleTests())
									grcDebugDraw::Capsule(vCapsuleStart, vCapsuleEnd, ms_Tunables.GetReticuleSlowDownCapsuleRadius(), Color_red, false, -1);
								else
									grcDebugDraw::Circle(m_pFreeAimAssistTarget->GetTransform().GetPosition(), ms_Tunables.GetReticuleSlowDownRadius(), Color_red, RCC_VEC3V(vSide), RCC_VEC3V(vUp), false, false, -1);
							}
						}
					}
				}
			}
		}
	}

	if(ms_Tunables.m_DisplayAimAssistCurves)
	{
		// Draw some axes
		static Vector2 AXIS_ORIGIN(0.8f, 0.3f);
		static Vector2 AXIS_END(0.95f, 0.0733f);
		grcDebugDraw::Line(AXIS_ORIGIN, Vector2(AXIS_END.x, AXIS_ORIGIN.y), Color_red);
		grcDebugDraw::Line(AXIS_ORIGIN, Vector2(AXIS_ORIGIN.x, AXIS_END.y), Color_green);

		// Draw the camera input line
		grcDebugDraw::Line(Vector2(AXIS_ORIGIN.x + (AXIS_END.x - AXIS_ORIGIN.x) * m_fLastCameraInput, AXIS_ORIGIN.y), Vector2(AXIS_ORIGIN.x + (AXIS_END.x - AXIS_ORIGIN.x) * m_fLastCameraInput, AXIS_END.y), Color_HotPink);

		//! If we are strafing, need to clamp our scaled input so that it doesn't get to low as the reticule can "bounce"
		//! off our free aim assist target.
		Vector2 vMoveBlendRatio;
		m_pPed->GetMotionData()->GetCurrentMoveBlendRatio(vMoveBlendRatio);
		float fAimAssistDistanceResult = 1.0f;
		float fMinScaleInput = 0.0f;
		float fBlend = m_fCurveSetBlend;
		float fDistanceToFreeAimAssistTarget = 0.0f;
		if(abs(vMoveBlendRatio.x) > 0.0f)
		{
			vMoveBlendRatio.NormalizeSafe();
			float fStrafeScale = abs(vMoveBlendRatio.x);

			if(m_pFreeAimAssistTarget)
			{
				Vector3 vLockOnPos = VEC3V_TO_VECTOR3(m_pFreeAimAssistTarget->GetTransform().GetPosition());
				Vector3 vPedPos = VEC3V_TO_VECTOR3(m_pPed->GetTransform().GetPosition());
				Vector3 vDistance = vLockOnPos - vPedPos;
				fDistanceToFreeAimAssistTarget = vDistance.XYMag();
				fAimAssistDistanceResult = fStrafeScale * GetAimDistanceScaleCurve()->GetResult(fDistanceToFreeAimAssistTarget);
			
				if(ms_Tunables.m_UseReticuleSlowDownStrafeClamp)
				{
					fMinScaleInput = fAimAssistDistanceResult;
				}	
				else
				{
					fBlend *= fAimAssistDistanceResult;
				}
			}
		}

		s32 iStart = 0, iEnd = CTargettingDifficultyInfo::CS_Max;
	
		bool bAccurateMode = IsCameraInAccurateMode();
		float fCurrentResult = 0.0f;
		if(bAccurateMode)
		{
			iStart = CTargettingDifficultyInfo::CS_AimZoom;

			float fAimZoomValue = GetCurveSet(CTargettingDifficultyInfo::CS_AimZoom)->GetResult(m_fLastCameraInput);
			float fAimAssistZoomValue = GetCurveSet(CTargettingDifficultyInfo::CS_AimAssistZoom)->GetResult(m_fLastCameraInput);
			fCurrentResult = Lerp(fBlend, fAimZoomValue, fAimAssistZoomValue);
		}
		else
		{
			iEnd = CTargettingDifficultyInfo::CS_AimZoom;

			float fAimValue = GetCurveSet(CTargettingDifficultyInfo::CS_Aim)->GetResult(m_fLastCameraInput);
			float fAimAssistValue = GetCurveSet(CTargettingDifficultyInfo::CS_AimAssist)->GetResult(m_fLastCameraInput);
			fCurrentResult = Lerp(fBlend, fAimValue, fAimAssistValue);
		}

		if(ms_Tunables.m_UseReticuleSlowDownStrafeClamp && fCurrentResult < fMinScaleInput)
		{
			fCurrentResult = Min(fMinScaleInput, m_fLastCameraInput);
		}

		// Render our current result on the graph
		grcDebugDraw::Circle(Vector2(AXIS_ORIGIN.x + (AXIS_END.x - AXIS_ORIGIN.x) * m_fLastCameraInput, AXIS_ORIGIN.y + (AXIS_END.y - AXIS_ORIGIN.y) * fCurrentResult), 0.005f, Color_purple4);

		for(s32 i = iStart; i < iEnd; i++)
		{
			GetCurveSet(i)->Render(AXIS_ORIGIN, AXIS_END);
		}

		static Vector2 AXIS_ORIGIN_DISTANCE(0.8f, 0.6f);
		static Vector2 AXIS_END_DISTANCE(0.95f, 0.3733f);
		static dev_float AXIS_SCALE_DISTANCE = 250.0f;

		grcDebugDraw::Line(AXIS_ORIGIN_DISTANCE, Vector2(AXIS_END_DISTANCE.x, AXIS_ORIGIN_DISTANCE.y), Color_red);
		grcDebugDraw::Line(AXIS_ORIGIN_DISTANCE, Vector2(AXIS_ORIGIN_DISTANCE.x, AXIS_END_DISTANCE.y), Color_green);

		float fDistanceLine = Min(fDistanceToFreeAimAssistTarget/AXIS_SCALE_DISTANCE, 1.0f);

		// Draw the distance input line
		grcDebugDraw::Line(Vector2(AXIS_ORIGIN_DISTANCE.x + (AXIS_END_DISTANCE.x - AXIS_ORIGIN_DISTANCE.x) * fDistanceLine, AXIS_ORIGIN_DISTANCE.y), 
			Vector2(AXIS_ORIGIN_DISTANCE.x + (AXIS_END_DISTANCE.x - AXIS_ORIGIN_DISTANCE.x) * fDistanceLine, AXIS_END_DISTANCE.y), 
			Color_HotPink);

		GetAimDistanceScaleCurve()->Render(AXIS_ORIGIN_DISTANCE, AXIS_END_DISTANCE, AXIS_SCALE_DISTANCE);
	}
}
#endif // __BANK

bool CPlayerPedTargeting::ShouldUpdateTargeting() const
{
	//! If we haven't locked on to anything, then don't process whilst doing certain tasks. This allows us to process lock on immediately
	//! afterwards without using up our soft lock time.
	bool bDisableTargetingForTask = false;
	if(m_pLockOnTarget == NULL)
	{
		bDisableTargetingForTask = IsTaskDisablingTargeting();
	}

	// Do not update if we don't have a weapon
	if (!GetWeaponInfo() || bDisableTargetingForTask)
	{
		return false;
	}

	return true;
}

bool CPlayerPedTargeting::IsTaskDisablingTargeting() const
{
	if(m_pPed->GetPedResetFlag(CPED_RESET_FLAG_IsVaulting) || 
		m_pPed->GetPedResetFlag(CPED_RESET_FLAG_IsJumping) || 
		m_pPed->GetPedResetFlag(CPED_RESET_FLAG_IsFalling) || 
		m_pPed->GetPedResetFlag(CPED_RESET_FLAG_IsLanding) || 
		m_pPed->GetPedResetFlag(CPED_RESET_FLAG_IsExitingVehicle))
	{
		return true;
	}

	return false;
}

bool CPlayerPedTargeting::CanIncrementSoftLockTimer() const
{
	if(ms_Tunables.CanPauseSoftLockTimer())
	{
		return !m_pPed->GetPedResetFlag(CPED_RESET_FLAG_IsReloading) && !IsTaskDisablingTargeting()  FPS_MODE_SUPPORTED_ONLY( && (!m_pPed->IsFirstPersonShooterModeEnabledForPlayer(false) || !m_pPed->GetPedResetFlag(CPED_RESET_FLAG_DoingCombatRoll)) );
	}

	return true;
}

bool CPlayerPedTargeting::ShouldUpdateFineAim(const CWeaponInfo& weaponInfo)
{
	// Can't fine aim when using melee weapons
	if (weaponInfo.GetIsMelee())
	{
		return false;
	}
	return true;
}

bool CPlayerPedTargeting::UpdateHardLockOn(const CWeapon* pWeapon, const CWeaponInfo& weaponInfo)
{
	bool bClearFineAim = true;

	if (CannotLockOn(pWeapon, weaponInfo, HardLockOn) || GetShouldBreakLockOnWithTarget())
	{
		return false;
	}

	if (m_pLockOnTarget)
	{
		if (m_pPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_ASSISTED_AIMING_ON))
		{
			bool bFindNewTarget = true;
			bool bNULLTarget = false;

			if (m_pLockOnTarget->GetIsTypePed())
			{
				if(static_cast<CPed*>(m_pLockOnTarget.Get())->GetIsDeadOrDying())
				{
					bFindNewTarget = false;

					if (m_fAssistedAimDeadPedLockOnTimer < 0.0f)
					{
						TUNE_GROUP_FLOAT(ASSISTED_AIM_TUNE, DEAD_PED_LOCKON_DURATION, g_DefaultDeadPedLockOnDuration, 0.0f, 5.0f, 0.1f);
						m_fAssistedAimDeadPedLockOnTimer = DEAD_PED_LOCKON_DURATION;
					}
					else
					{
						m_fAssistedAimDeadPedLockOnTimer -= fwTimer::GetTimeStep();
						if (m_fAssistedAimDeadPedLockOnTimer <= 0.0f)
						{
							bFindNewTarget = true;
							bNULLTarget = true;
						}
					}
				}
				else
				{
					//! If we down our ped, don't count as downed for targeting check, otherwise we switch targets, which we don't want (unless they 
					//! are ligitimately blocked etc).
					CPedTargetEvaluator::SetPedToIgnoreForDownedCheck(static_cast<CPed*>(m_pLockOnTarget.Get()));
			
					//! Allow searching for better targets after a min time. 
					TUNE_GROUP_FLOAT(ASSISTED_AIM_TUNE, ASSISTED_AIM_MIN_LOCKON, 1.0f, 0.0f, 5.0f, 0.1f);
					if(m_fTimeLockedOn > ASSISTED_AIM_MIN_LOCKON)
					{
						bNULLTarget = true;
					}
				}
			}
			
			if (bFindNewTarget)
			{
				if(bNULLTarget)
				{
					m_pLockOnTarget = NULL;
				}
				
				FindAndSetLockOnTarget();
			}

			CPedTargetEvaluator::SetPedToIgnoreForDownedCheck(NULL);
		}

		if (m_pLockOnTarget)
		{
			//! Don't allow target switching if below fine aim time.
			if(m_fTimeLockedOn < ms_Tunables.GetMinFineAimTime())
			{
				m_bCheckForLeftTarget = false;
				m_bCheckForRightTarget = false;
			}

			UpdateLockOn(HardLockOn, m_pLockOnTarget);

			if (ShouldUpdateFineAim(weaponInfo))
			{
				UpdateFineAim();
				bClearFineAim = false;
			}
		}
	}
	else
	{
		m_fAssistedAimDeadPedLockOnTimer = -1.0f;

		// No lockon target currently, but want to only check in a certain direction relative to camera
		if (m_bCheckForLeftTarget || m_bCheckForRightTarget)
		{
			aiAssertf(!(m_bCheckForLeftTarget && m_bCheckForRightTarget), "Can't select a left and right target at the same time");
			FindNextLockOnTarget(m_pLockOnTarget, m_bCheckForLeftTarget ? HD_LEFT : HD_RIGHT);
		}
		else
		{
			FindAndSetLockOnTarget();
		}
	}

	if (bClearFineAim)
	{
		ClearFineAim();
	}

	// Don't clear lock on
	return true;
}

bool CPlayerPedTargeting::UpdateSoftLockOn(const CWeapon* pWeapon, const CWeaponInfo& weaponInfo)
{
	// NOTE: We don't allow an extended lock-on window in cover, as it can result in delayed lock-on when transitioning from idle to aiming/firing,
	// which has a negative impact on the perceived sync of the player and camera movement.
	float fSoftLockTimeToAcquireTarget = m_pPed->GetIsInCover() FPS_MODE_SUPPORTED_ONLY(&& !m_pPed->IsFirstPersonShooterModeEnabledForPlayer(false) )? ms_Tunables.GetSoftLockTimeToAcquireTargetInCover() : ms_Tunables.GetSoftLockTimeToAcquireTarget();

	bool bCanLockOn = m_fTimeSoftLockedOn <= fSoftLockTimeToAcquireTarget || m_bCheckForLockOnSwitchExtension;

	bool bClearFineAim = true;

	//! If we kill target ped, trigger target switch extension time.
	if(m_pLockOnTarget && 
		(m_pLockOnTarget != m_pLockOnSwitchExtensionTarget) && 
		m_pLockOnTarget->GetIsTypePed() && 
		ms_Tunables.GetLockOnSwitchTimeExtensionKillTarget() > 0)
	{
		CPed* pTargetPed = static_cast<CPed*>(m_pLockOnTarget.Get());
		if(pTargetPed && pTargetPed->IsDead() && pTargetPed->GetSourceOfDeath()==m_pPed )
		{
			m_uTimeLockOnSwitchExtensionExpires = Max(m_uTimeLockOnSwitchExtensionExpires, fwTimer::GetTimeInMilliseconds() + ms_Tunables.GetLockOnSwitchTimeExtensionKillTarget());
			m_pLockOnSwitchExtensionTarget = m_pLockOnTarget;
		}
	}

	if (CannotLockOn(pWeapon, weaponInfo, SoftLockOn) || GetShouldBreakSoftLockTarget())
	{
		m_bPlayerHasBrokenLockOn = true;
		return false;
	}

	if (m_pLockOnTarget)
	{
		// Allow switching targets?
		CEntity *pPreUpdateLockOnTarget = m_pLockOnTarget;
		
		//! Don't allow target switching until we have started fine aim.
		if(!m_bPlayerIsFineAiming)
		{
			m_bCheckForLeftTarget = false;
			m_bCheckForRightTarget = false;
		}

		UpdateLockOn(SoftLockOn, m_pLockOnTarget);

		// Reset lock on time if we switch targets?
		if(pPreUpdateLockOnTarget != m_pLockOnTarget)
		{
			ClearFineAim();
			m_fTimeSoftLockedOn = 0.0f;
			m_bPlayerHasBrokenLockOn = false;
			m_bWasWithinFineAimLimits = false;
		}

		if (ms_Tunables.GetAllowSoftLockFineAim() && ShouldUpdateFineAim(weaponInfo))
		{
			UpdateFineAim(ms_Tunables.GetUseFineAimSpring());
			bClearFineAim = false;
		}
	}
	else if(m_pLockOnSwitchExtensionTarget && m_bCheckForLockOnSwitchExtension)
	{
		UpdateLockOn(SoftLockOn, m_pLockOnSwitchExtensionTarget);
		
		if(m_pLockOnTarget)
		{
			ClearFineAim();
			m_fTimeSoftLockedOn = 0.0f;
			m_bPlayerHasBrokenLockOn = false;
			m_bWasWithinFineAimLimits = false;
		}

		m_bCheckForLockOnSwitchExtension = false;
	}
	else
	{
		if(bCanLockOn)
		{
			FindAndSetLockOnTarget();
		}
		else if(!m_pLockOnTarget)
		{
			m_bPlayerHasBrokenLockOn = true;
			return false;
		}
	}

	if (bClearFineAim)
	{
		ClearFineAim();
	}

	// Don't clear lock on
	return true;
}

void CPlayerPedTargeting::ProcessPedEvents()
{
	if(m_pLockOnTarget)
	{
		ProcessPedEvents(m_pLockOnTarget);
	}

	if(m_pFreeAimTarget && (m_pFreeAimTarget != m_pLockOnTarget))
	{
		ProcessPedEvents(m_pFreeAimTarget);
	}

	if(m_pFreeAimAssistTarget && (m_pFreeAimAssistTarget != m_pFreeAimTarget) && (m_pFreeAimAssistTarget != m_pLockOnTarget))
	{
		ProcessPedEvents(m_pFreeAimAssistTarget);
	}
}

void CPlayerPedTargeting::ProcessPedEvents(CEntity* pEntity)
{
	if(!pEntity || !pEntity->GetIsTypePed())
	{
		return;
	}

	CPed* pTargetPed = static_cast<CPed *>(pEntity);

#if LAZY_RAGDOLL_BOUNDS_UPDATE
	// Targeted Peds should have their bounds updated
	if (pTargetPed->GetRagdollInst())
	{
		pTargetPed->RequestRagdollBoundsUpdate();
	}
#endif

	// If member ped is valid
	if( m_pPed )
	{
		//Ensure the equipped weapon is valid.
		Assert( m_pPed->GetWeaponManager() );
		const CWeaponInfo* pTurretWeaponInfo = m_pPed->GetWeaponManager()->GetEquippedTurretWeaponInfo();
		const CWeapon* pEquippedWeapon = m_pPed->GetWeaponManager()->GetEquippedWeapon();
		if(!pEquippedWeapon && !pTurretWeaponInfo)
		{
			return;
		}

		//Ensure the weapon info is valid.
		const CWeaponInfo* pWeaponInfo = pTurretWeaponInfo ? pTurretWeaponInfo : pEquippedWeapon->GetWeaponInfo();
		if(!pWeaponInfo)
		{
			return;
		}

		// Don't fire any events if you are entering or exiting a vehicle.
		const bool bEnterTaskRunning = m_pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_ENTER_VEHICLE, true);
		const bool bExitTaskRunning = m_pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_EXIT_VEHICLE, true);
		if(bEnterTaskRunning || bExitTaskRunning)
		{
			return;
		}

		// Don't fire events when peeking from cover
		if( m_pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_IN_COVER, true) &&
			m_pPed->GetPedIntelligence()->GetQueriableInterface()->GetStateForTaskType(CTaskTypes::TASK_IN_COVER) == CTaskInCover::State_Peeking )
		{
			return;
		}

		//Ensure the time aiming is valid.
		// Turret weapon infos are apparently counted as unarmed, so many of the checks below don't make sense for them.
		if (!pWeaponInfo->GetIsTurret())
		{
			if(pWeaponInfo->GetIsUnarmed() || pWeaponInfo->GetIsMelee())
			{
				static dev_float s_fMinTimeAiming = 0.6f;
				if(m_fTimeAiming < s_fMinTimeAiming)
				{
					return;
				}
			}
			else if(m_pPed->GetIsInVehicle())
			{
				static dev_float s_fMinTimeAiming = 0.5f;
				if(m_fTimeAiming < s_fMinTimeAiming)
				{
					return;
				}
			}
		}

		static float s_fProjectileVisionRange = 2.0f;
		static float s_fDangerousMeleeWeaponVisionRange = 2.0f;
		// Compute distance to target squared
		float fDistSquared = VEC3V_TO_VECTOR3(pTargetPed->GetTransform().GetPosition()).Dist2(VEC3V_TO_VECTOR3(m_pPed->GetTransform().GetPosition()));

		// If within visual reaction range of the target
		float fRange = CEventShockingWeaponThreat::GetVisualReactionRange();

		//Use the weapons range + a little if it's less than default.
		fRange = rage::Min(fRange, pWeaponInfo->GetRange() + 2.0f);

		if (pWeaponInfo->GetIsProjectile() && pWeaponInfo->GetIsThrownWeapon())
		{
			// Projectiles have a much smaller visual reaction range than normal.
			fRange = s_fProjectileVisionRange;
		}

		// Don't add in weapon threat events for unarmed.
		if((!pWeaponInfo->GetIsUnarmed() || pWeaponInfo->GetIsTurret()) && !pWeaponInfo->GetIsNonViolent() && !pWeaponInfo->GetIsMelee() && !pTargetPed->PopTypeIsMission() && fDistSquared <= rage::square(fRange))
		{
			// Emit a shocking event for the weapon threat, in case anyone witnesses it
			CEventShockingWeaponThreat shockEvent(*m_pPed, pTargetPed);
			CShockingEventsManager::Add(shockEvent);
		}

		// Check for gun aimed at event at target ped
		if (CEventGunAimedAt::IsAimingInTargetSeeingRange(*pTargetPed, *m_pPed, *pWeaponInfo))
		{
			// Melee events have CEventMeleeAction as their targeting event.
			if ((pWeaponInfo->GetIsMelee() || pWeaponInfo->GetIsUnarmed()) && !pWeaponInfo->GetIsDangerousLookingMeleeWeapon() && !pWeaponInfo->GetIsTurret())
			{
				const bool bIsOffensive = true;
				CEventMeleeAction eventMeleeAction(m_pPed, pTargetPed, bIsOffensive);
				pTargetPed->GetPedIntelligence()->AddEvent(eventMeleeAction);
			}
			// Otherwise use gun aimed at.
			else if (pWeaponInfo->GetIsNonViolent() && !pWeaponInfo->GetIsTurret())
			{
				CPed* pFiringPed = static_cast<CPed*>(m_pPed);
				if (pFiringPed)
				{
					CEventShockingNonViolentWeaponAimedAt event(*pFiringPed);
					pTargetPed->GetPedIntelligence()->AddEvent(event);
				}
			}
			else if ((!pWeaponInfo->GetIsProjectile() || !pWeaponInfo->GetIsThrownWeapon() || fDistSquared <= square(s_fProjectileVisionRange)) 
				&& (!pWeaponInfo->GetIsDangerousLookingMeleeWeapon() || fDistSquared <= square(s_fDangerousMeleeWeaponVisionRange)))
			{
				CPed* pFiringPed = static_cast<CPed*>(m_pPed);
				CEventGunAimedAt event(pFiringPed);
				pTargetPed->GetPedIntelligence()->AddEvent(event);

				CEventFriendlyAimedAt eventFriendlyAimedAt(pFiringPed);
				pTargetPed->GetPedIntelligence()->AddEvent(eventFriendlyAimedAt);
			}
		}
	}
}

void CPlayerPedTargeting::ProcessPedEventsForFreeAimAssist()
{
	if(m_FreeAimAssistProbeResults.GetResultsReady())
	{
		for(WorldProbe::ResultIterator it = m_FreeAimAssistProbeResults.begin(); it < m_FreeAimAssistProbeResults.last_result(); ++it)
		{
			CEntity* pEntity = it->GetHitEntity();
			if(pEntity && pEntity->GetIsTypePed() && (pEntity != m_pLockOnTarget) && (pEntity != m_pFreeAimTarget) && (pEntity != m_pFreeAimAssistTarget))
			{
				ProcessPedEvents(pEntity);
			}
			else
			{
				// We are blocked by an object or map geometry -OR- We hit a non-entity.
				break;
			}
		}
	}
}

bool CPlayerPedTargeting::UpdateFreeAim(const CWeaponInfo& weaponInfo)
{
	// Cache the weapon objects
	weaponAssert(m_pPed->GetWeaponManager());
	const CWeapon* pWeapon       = m_pPed->GetWeaponManager()->GetEquippedWeapon();
	const CObject* pWeaponObject = m_pPed->GetWeaponManager()->GetEquippedWeaponObject();

	if(pWeapon)
	{
		const Matrix34* mWeapon;
		if (pWeaponObject)
			mWeapon = &RCC_MATRIX34(pWeaponObject->GetMatrixRef());
		else // Just use the peds matrix for taunts
			mWeapon = &RCC_MATRIX34(m_pPed->GetMatrixRef());
		
		Vector3 vStart, vEnd;
		pWeapon->CalcFireVecFromAimCamera(m_pPed->GetMyVehicle() != NULL ? static_cast<CEntity*>(m_pPed->GetMyVehicle()) : m_pPed, *mWeapon, vStart, vEnd);

		//! Override vEnd if it is a vehicle weapon.
		if(m_pPed->GetWeaponManager()->GetEquippedVehicleWeapon() && m_pPed->GetWeaponManager()->GetEquippedVehicleWeapon()->GetWeaponInfo())
		{			
			const camFrame& aimFrame = camInterface::GetPlayerControlCamAimFrame();

			Vector3 vShot  = aimFrame.GetFront();
			vEnd = vStart + (m_pPed->GetWeaponManager()->GetEquippedVehicleWeapon()->GetWeaponInfo()->GetRange() * vShot);
		}

		// Search for the ped capsule under the reticule
		Vector3 vHitPos;
		FindFreeAimTarget(vStart, vEnd, weaponInfo, vHitPos);

		if(m_pFreeAimTargetRagdoll == NULL)
		{
			float fTestLength = m_pPed->GetIsInVehicle() ? ms_Tunables.GetAimAssistCapsuleMaxLengthInVehicle() : ms_Tunables.GetAimAssistCapsuleMaxLength();

			if (GetAreCNCFreeAimImprovementsEnabled() && GetCurrentTargetingMode() == TargetingMode::FreeAim)
			{
				// Don't exceed the weapon range.
				const CWeaponInfo* pWeaponInfo = pWeapon->GetWeaponInfo();
				const bool bAimingSniper = pWeaponInfo && pWeaponInfo->GetGroup() == WEAPONGROUP_SNIPER && camInterface::GetGameplayDirector().IsFirstPersonAiming();
				float fMaxCapsuleLength = bAimingSniper ? ms_Tunables.GetAimAssistCapsuleMaxLengthFreeAimSniper() : ms_Tunables.GetAimAssistCapsuleMaxLengthFreeAim();
				fTestLength = Min(fMaxCapsuleLength, weaponInfo.GetRange());
			}

			// Search for the aim assist target
			// 5/10/13 - cthomas - Primarily for performance reasons, clamp the range of aim assist.
			Vector3 vClampedAimAssistFiringVec = vEnd - vStart;
			const float clampedAimAssistRange = Min(vClampedAimAssistFiringVec.Mag(), fTestLength);
			vClampedAimAssistFiringVec.Normalize();
			vClampedAimAssistFiringVec.Scale(clampedAimAssistRange);
			Vector3 vEndPositionForFreeAimAssist = vStart + vClampedAimAssistFiringVec; 
			FindFreeAimAssistTarget(vStart, vEndPositionForFreeAimAssist);
		}
		else
		{
			// Clear free aim assist target in the case of getting a valid ragdoll target
			m_pFreeAimAssistTarget = m_pFreeAimTargetRagdoll;
			m_vFreeAimAssistTargetPos = vHitPos;
			m_vClosestFreeAimAssistTargetPos = vHitPos;
		}
	}

	// Always clear lock on
	return false;
}


bool CPlayerPedTargeting::ProcessOnFootHomingLockOn(CPed* pPed)
{
	if(pPed->IsLocalPlayer())
	{
		weaponAssert(pPed->GetWeaponManager());
		CWeapon* pEquippedWeapon = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeapon() : NULL;

		int iTargetingFlags = CPedTargetEvaluator::AIRCRAFT_WEAPON_TARGET_FLAGS|CPedTargetEvaluator::TS_HOMING_MISSLE_CHECK;
		iTargetingFlags &= ~CPedTargetEvaluator::TS_CHECK_PEDS;
		iTargetingFlags &= ~CPedTargetEvaluator::TS_IGNORE_VEHICLES_IN_LOS;

		// Find range of current weapon
		float fHeadingLimit = CPedTargetEvaluator::ms_Tunables.m_DefaultTargetAngularLimitVehicleWeapon;
		float fPitchLimitMin = 0.0f;
		float fPitchLimitMax = 0.0f;
		float fWeaponRange = CVehicleWaterCannon::FIRETRUCK_CANNON_RANGE;
		float fRejectionWeaponRange = fWeaponRange;

		CEntity* pCurrentTarget = GetLockOnTarget();

		const CWeaponInfo* pWeaponInfo = pEquippedWeapon ? pEquippedWeapon->GetWeaponInfo() : NULL;
		if( pWeaponInfo )
		{
			const CAimingInfo* pAimingInfo = pWeaponInfo->GetAimingInfo();
			if( pAimingInfo )
			{
				fHeadingLimit  = pAimingInfo->GetHeadingLimit();
				fPitchLimitMin = pAimingInfo->GetSweepPitchMin();
				fPitchLimitMax = pAimingInfo->GetSweepPitchMax();
			}

			fWeaponRange = pWeaponInfo->GetRange();
			fRejectionWeaponRange = fWeaponRange;
		}

		CPedTargetEvaluator::tHeadingLimitData headingLimitData(fHeadingLimit);
		static dev_float fExpandedHeadingLimitsTune = 30.0f;

		if( pCurrentTarget )
		{
			bool bSelectingLeft = IsSelectingLeftOnFootHomingTarget(pPed);
			bool bSelectingRight = IsSelectingRightOnFootHomingTarget(pPed);
			if (bSelectingLeft || bSelectingRight)
			{
				iTargetingFlags |= bSelectingLeft ? CPedTargetEvaluator::TS_CYCLE_LEFT : iTargetingFlags |= CPedTargetEvaluator::TS_CYCLE_RIGHT;
				iTargetingFlags &= ~CPedTargetEvaluator::TS_PED_HEADING;			// Ignore ped heading checks when cycling targets.
				iTargetingFlags &= ~CPedTargetEvaluator::TS_USE_DISTANCE_SCORING;	// Ignore distance scoring to ensure we can cycle through all vehicles

				headingLimitData.fHeadingFalloffPower = 0.0f;
				// Use expanded heading limits when manually scanning for targets (target must be on screen).
				headingLimitData.bCamHeadingNeedsTargetOnScreen = true;
				headingLimitData.fHeadingAngularLimit = fExpandedHeadingLimitsTune;
				headingLimitData.fWideHeadingAngulerLimit = fExpandedHeadingLimitsTune;
				fHeadingLimit = fExpandedHeadingLimitsTune;
			}
		}

		bool bManualTargetSwitch = false;
		CEntity *pCurrentTargetPreSwitch = NULL;
		if(iTargetingFlags & (CPedTargetEvaluator::TS_CYCLE_LEFT | CPedTargetEvaluator::TS_CYCLE_RIGHT ))
		{
			bManualTargetSwitch = true;
			pCurrentTargetPreSwitch = pCurrentTarget;
		}

		CEntity* pFoundTarget = NULL;
		
		if(CPlayerInfo::IsAiming(false) && m_pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsAimingGun) && !pPed->GetIsInVehicle() && !m_pPed->GetPedResetFlag(CPED_RESET_FLAG_InAirDefenceSphere))
		{
			pFoundTarget = CPedTargetEvaluator::FindTarget( pPed, iTargetingFlags, fWeaponRange, fRejectionWeaponRange, NULL, NULL, headingLimitData, fPitchLimitMin, fPitchLimitMax );
		}

		//! If we tried to find a left/right target and failed, keep current target.
		if(pCurrentTargetPreSwitch && !pFoundTarget)
		{
			pFoundTarget =  pCurrentTargetPreSwitch;
		}

		// Reset manual target timer if we've just tried to switch but landed on the same target.
		if (bManualTargetSwitch && pFoundTarget && pCurrentTarget && pFoundTarget == pCurrentTarget)
		{
			m_uManualSwitchTargetTime = fwTimer::GetTimeInMilliseconds();
		}

		if(pFoundTarget)
		{
			// While we have a target, increment the potential target timer
			if( m_pPotentialTarget )
			{
				m_fPotentialTargetTimer += fwTimer::GetTimeStep();
			}

			// If we had no previous target, just set it
			if( !pCurrentTarget )
			{
				ResetTimeAimingAtTarget();
				SetLockOnTarget( pFoundTarget );
			}
			// Does our potential target differ from our current?
			else if( pCurrentTarget != pFoundTarget )
			{
				// Check to see the current target is valid and alive
				Vector3 vHeading = VEC3V_TO_VECTOR3( pPed->GetTransform().GetB() );
				float fHeadingDifference = 0.0f;
				bool bOutsideMinHeading = false;
				bool bInInclusionRangeFailedHeading = false;
				int iIsValidFlags = iTargetingFlags;
				iIsValidFlags |= CPedTargetEvaluator::TS_CYCLE_LEFT;
				iIsValidFlags |= CPedTargetEvaluator::TS_CYCLE_RIGHT;
				bool bTargetInDeadzone = false;

				// Use expanded heading limits when cycling targets manually (ignore deadzone box checks).
				static dev_u32 s_uManualSwitchCooldown = 2500;
				float fExpandedHeadingLimit = headingLimitData.GetAngularLimit();
				bool bManuallySwitchingTargets = bManualTargetSwitch || ((fwTimer::GetTimeInMilliseconds() - s_uManualSwitchCooldown) < m_uManualSwitchTargetTime);
				bool bDoDeadzoneBoxCheck = !bManuallySwitchingTargets;
				if (bManuallySwitchingTargets)
				{
					fExpandedHeadingLimit = fExpandedHeadingLimitsTune;

					// Check target validity using non-expanded heading limits. Don't allow lock on when outside of main box (by continually resetting aim timer).
					bool bCanLockOnToTarget = CPedTargetEvaluator::IsTargetValid( pPed->GetTransform().GetPosition(), pCurrentTarget->GetTransform().GetPosition(), iIsValidFlags, vHeading, fWeaponRange, fHeadingDifference, bOutsideMinHeading, bInInclusionRangeFailedHeading, bTargetInDeadzone, fHeadingLimit, fPitchLimitMin, fPitchLimitMax, true, false, false, bDoDeadzoneBoxCheck );
					if (!bCanLockOnToTarget)
					{
						ResetTimeAimingAtTarget();
					}			
				}

				bool bIsValid = CPedTargetEvaluator::IsTargetValid( pPed->GetTransform().GetPosition(), pCurrentTarget->GetTransform().GetPosition(), iIsValidFlags, vHeading, fWeaponRange, fHeadingDifference, bOutsideMinHeading, bInInclusionRangeFailedHeading, bTargetInDeadzone, fExpandedHeadingLimit, fPitchLimitMin, fPitchLimitMax, true, false, false, bDoDeadzoneBoxCheck );
				bool bIsAlive = pCurrentTarget->GetIsPhysical() && ((CPhysical*)pCurrentTarget)->GetHealth() > 0;

				// Homing specific logic 
				if( bIsValid && bIsAlive && pWeaponInfo && pWeaponInfo->GetIsHoming())
				{
					// Selected this manually.
					if( bManualTargetSwitch )
					{
						ResetTimeAimingAtTarget();
						SetLockOnTarget( pFoundTarget );

						//! Selected target manually. Allow a longer cooldown between auto switching targets.
						m_uManualSwitchTargetTime = fwTimer::GetTimeInMilliseconds();
					}

					// Do we have a new priority target to be?
					if( pFoundTarget != m_pPotentialTarget )
					{
						// Cache off the new potential target and reset the change target timer
						m_pPotentialTarget = pFoundTarget;
						m_fPotentialTargetTimer = 0.0f;
					}

					// prevent switching targets every frame
					const CAmmoInfo* pAmmoInfo = pWeaponInfo->GetAmmoInfo();
					if( pAmmoInfo && pAmmoInfo->GetClassId() == CAmmoRocketInfo::GetStaticClassId() )
					{
						const CAmmoRocketInfo* pRocketInfo = static_cast<const CAmmoRocketInfo*>(pAmmoInfo);
						Assert( pRocketInfo );

						// Calculate the speed ratio based on the default speed
						float fMaxSpeed = rage::square( DEFAULT_MAX_SPEED );
						float fSpeedRatio = rage::Min( pPed->GetVelocity().Mag2(), fMaxSpeed ) / fMaxSpeed;

						// Determine the time to switch targets based on the vehicle speed
						float fTimeToSwitchBetweenTargets = Lerp( fSpeedRatio, pRocketInfo->GetTimeBeforeSwitchTargetMax(), pRocketInfo->GetTimeBeforeSwitchTargetMin() );

						// B*2171844: If our current target is outside of the box (ie in the deadzone) and we have a target within the actual box, swap immediately.
						if( (fTimeToSwitchBetweenTargets < m_fPotentialTargetTimer && ((m_uManualSwitchTargetTime == 0) || ((fwTimer::GetTimeInMilliseconds() - s_uManualSwitchCooldown) > m_uManualSwitchTargetTime))) || bTargetInDeadzone)
						{
							ResetTimeAimingAtTarget();
							SetLockOnTarget( pFoundTarget );
							m_uManualSwitchTargetTime = 0;
							return true;
						}
					}
				}
				// Otherwise just set the new target
				else
				{
					ResetTimeAimingAtTarget();
					SetLockOnTarget( pFoundTarget );
					return true;
				}
			}

			if(GetLockOnTarget() && GetLockOnTarget()->GetIsTypeVehicle())
			{
				static_cast<CVehicle*>(GetLockOnTarget())->SetLastTimeHomedAt(fwTimer::GetTimeInMilliseconds());
			}
		}
		// Clear the target if we don't have one
		else
		{
			m_pPotentialTarget = NULL;
			m_fPotentialTargetTimer = 0.0f;
			ResetTimeAimingAtTarget();
			ClearLockOnTarget();
		}
#if __DEV
		// Debug draw the target for now
		static dev_bool bDebugDrawLockon = false;
		if(bDebugDrawLockon)
		{
			if(pCurrentTarget)
			{
				grcDebugDraw::BoxOriented(RCC_VEC3V(pCurrentTarget->GetBoundingBoxMin()),VECTOR3_TO_VEC3V(pCurrentTarget->GetBoundingBoxMax()), pCurrentTarget->GetMatrix(),Color_white);
			}
		}					
#endif

	}
	return false;
}

bool CPlayerPedTargeting::IsSelectingLeftOnFootHomingTarget(const CPed *pPed)
{
	const CControl* pControl = pPed->GetControlFromPlayer();
	if (pControl && (pControl->GetVehicleFlySelectLeftTarget().IsPressed()))
	{
		return true;
	}

	return false;
}

bool CPlayerPedTargeting::IsSelectingRightOnFootHomingTarget(const CPed *pPed)
{
	const CControl* pControl = pPed->GetControlFromPlayer();
	if (pControl && (pControl->GetVehicleFlySelectRightTarget().IsPressed()))
	{
		return true;
	}

	return false;
}

CPlayerPedTargeting::OnFootHomingLockOnState CPlayerPedTargeting::GetOnFootHomingLockOnState(const CPed *pPed)
{
	if (pPed)
	{
		const CWeaponInfo *pWeaponInfo = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeaponInfo() : NULL;

		// Time needed before the target is considered locked on.
		// Used to determine UI reticule style and whether to enable homing or not.
		float fTimeBeforeHoming = 0.0f;
		const CAmmoInfo* pAmmoInfo = pWeaponInfo ? pWeaponInfo->GetAmmoInfo() : NULL;
		if(pAmmoInfo && (pAmmoInfo->GetClassId() == CAmmoRocketInfo::GetStaticClassId()) )
		{
			fTimeBeforeHoming = static_cast<const CAmmoRocketInfo*>(pAmmoInfo)->GetTimeBeforeHoming();
		}

		if (m_pLockOnTarget)
		{
			float fTotalLockOnTime = GetTimeAimingAtTarget();
			if (fTotalLockOnTime < (fTimeBeforeHoming/2))
			{	//	Display the green reticle.
				return OFH_NOT_LOCKED;
			}
			else if (fTotalLockOnTime < fTimeBeforeHoming)
			{	//	Display the flashing orange reticle.
				return OFH_ACQUIRING_LOCK_ON;
			}
			else
			{	//	Display the red reticle.
				//	Enable homing.
				return OFH_LOCKED_ON;
			}
		}
	}

	return OFH_NO_TARGET;
}

void CPlayerPedTargeting::AddTargetableEntity(CEntity* pEntity)
{
	const int iEntityIndex = m_TargetableEntities.Find((RegdEnt)pEntity);
	if (iEntityIndex == -1)
	{
		// Verify we haven't got any null entities in the array if it's full (hopefully shouldn't happen, just being safe!)
		if (m_TargetableEntities.IsFull())
		{
			for (int i = 0; i < m_TargetableEntities.GetCount(); ++i)
			{
				CEntity* pEntity = m_TargetableEntities[i];
				if (!pEntity)
				{
					m_TargetableEntities.DeleteFast(i);
				}
			}
		}

		if (pedVerifyf(m_TargetableEntities.GetCount() < m_TargetableEntities.GetMaxCount(), "CPlayerPedTargeting::s_iMaxTargetableEntities is full (MAX: %d)!", s_iMaxTargetableEntities))
		{
			m_TargetableEntities.Append() = pEntity;
		}
	}
}

void CPlayerPedTargeting::RemoveTargetableEntity(CEntity* pEntity)
{
	const int iIndex = m_TargetableEntities.Find((RegdEnt)pEntity);
	if (iIndex != -1)
	{
		m_TargetableEntities.DeleteFast(iIndex);
	}
}	

void CPlayerPedTargeting::ClearTargetableEntities()
{
	m_TargetableEntities.clear();
}

CEntity* CPlayerPedTargeting::GetTargetableEntity(int iIndex) const
{
	return m_TargetableEntities[iIndex].Get();
}

bool CPlayerPedTargeting::GetIsEntityAlreadyRegisteredInTargetableArray(CEntity* pEntity) const
{
	const int iIndex = m_TargetableEntities.Find((RegdEnt)pEntity);
	if (iIndex != -1)
	{
		return true;
	}

	return false;
}

#if __BANK
void CPlayerPedTargeting::DebugAddTargetableEntity()
{
	CEntity* pFocusEntity = CDebugScene::FocusEntities_Get(0);
	if (pFocusEntity)
	{
		CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
		if (pPlayerPed && pPlayerPed != pFocusEntity)
		{
			CPlayerInfo* pPlayerInfo = pPlayerPed->GetPlayerInfo();
			if (pPlayerInfo)
			{
				CPlayerPedTargeting& rTargeting = pPlayerInfo->GetTargeting();
				rTargeting.AddTargetableEntity(pFocusEntity);
			}
		}
	}
}

void CPlayerPedTargeting::DebugClearTargetableEntities()
{
	CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
	if (pPlayerPed)
	{
		CPlayerInfo* pPlayerInfo = pPlayerPed->GetPlayerInfo();
		if (pPlayerInfo)
		{
			CPlayerPedTargeting& rTargeting = pPlayerInfo->GetTargeting();
			rTargeting.ClearTargetableEntities();
		}
	}
}
#endif	// __BANK

void CPlayerPedTargeting::UpdateLockOn(const TargetingMode eTargetMode, CEntity *pCurrentLockOnEntity)
{
	if (eTargetMode == HardLockOn || eTargetMode == SoftLockOn)
	{
		if (m_bCheckForLeftTarget)
		{
			FindNextLockOnTarget(pCurrentLockOnEntity, HD_LEFT);
		}
		else if (m_bCheckForRightTarget)
		{
			FindNextLockOnTarget(pCurrentLockOnEntity, HD_RIGHT);
		}

#if __BANK
		if(CPedTargetEvaluator::DEBUG_TARGET_SWITCHING_SCORING_ENABLED)
		{
#if DEBUG_DRAW
			CTask::ms_debugDraw.ClearAll();
#endif
			CPedTargetEvaluator::EnableDebugScoring(true);
			switch(CPedTargetEvaluator::DEBUG_TARGETSCORING_DIRECTION)
			{
			case(CPedTargetEvaluator::eDSD_Left):
				FindNextLockOnTarget(pCurrentLockOnEntity, HD_LEFT, false);
				break;
			case(CPedTargetEvaluator::eDSD_Right):
				FindNextLockOnTarget(pCurrentLockOnEntity, HD_RIGHT, false);
				break;
			default:
				break;
			}
			CPedTargetEvaluator::EnableDebugScoring(false);
		}
#endif

		m_fTimeLockedOn += fwTimer::GetTimeStep_NonScaledClipped();
	}
}

bool CPlayerPedTargeting::CanPedLockOnWithWeapon(const CWeapon* pWeapon, const CWeaponInfo& weaponInfo)
{
	bool bCanLockOn = false;
	if(m_pPed->GetIsDrivingVehicle())
	{
		if(weaponInfo.GetForcingDriveByAssistedAim())
		{
			bCanLockOn = true;
		}
		else if(ms_Tunables.GetUseDriveByAssistedAim())
		{
			CVehicle *pVehicle = m_pPed->GetVehiclePedInside();
			if(pVehicle->GetVelocity().Mag2() >= (ms_Tunables.GetMinVelocityForDriveByAssistedAim() * ms_Tunables.GetMinVelocityForDriveByAssistedAim()))
			{
				bCanLockOn = true;
			}
		}
	}
	else if(m_pPed->GetIsInVehicle())
	{
		if(!m_pPed->IsInFirstPersonVehicleCamera())
		{
			bCanLockOn = pWeapon ? pWeapon->GetCanLockonInVehicle() : weaponInfo.GetCanLockOnInVehicle();
		}
	}
	else
	{
		bCanLockOn = pWeapon ? pWeapon->GetCanLockonOnFoot() : weaponInfo.GetCanLockOnOnFoot();
	}

	//! Disable lock on inside planes as camera is too far out for target selection to work well.
	if(m_pPed->GetVehiclePedInside() && m_pPed->GetVehiclePedInside()->InheritsFromPlane())
	{
		bCanLockOn = false;
	}

	return bCanLockOn;
}

bool CPlayerPedTargeting::CannotLockOn(const CWeapon* pWeapon, const CWeaponInfo& weaponInfo, const TargetingMode eTargetMode)
{
	pedFatalAssertf(m_pPed, "m_pPed is NULL");

	bool bCanLockOn = CanPedLockOnWithWeapon(pWeapon, weaponInfo);
	if (!bCanLockOn)
	{
		return true;
	}

	//! Allow lock on for passengers in MP now.
	/*if (NetworkInterface::IsGameInProgress() && bIsInVehicle && !m_pPed->GetIsDrivingVehicle())
	{
		return true;
	}*/

	if (m_pPed->GetPedResetFlag(CPED_RESET_FLAG_ClearLockonTarget))
	{
		return true;
	}

	//! Remove lock if target becomes invisible.
	if(m_pLockOnTarget && !m_pLockOnTarget->GetIsVisible())
	{
		return true;
	}

	// Do not allow lock on for peds in driver seat unless we're assisted aiming
	if (!CTaskAimGunVehicleDriveBy::ms_UseAssistedAiming && !m_pPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_ASSISTED_AIMING_ON) && m_pPed->GetMyVehicle() && !CNetwork::IsGameInProgress())
	{
		const bool bVehicleHasDriver = m_pPed->GetMyVehicle()->GetLayoutInfo()->GetHasDriver();

		if (bVehicleHasDriver && m_pPed->GetMyVehicle()->GetSeatManager()->GetDriver() == m_pPed)
		{
			return true;
		}
	}

	// Break lock on if target is dead and player is not really pushing the aim stick
	TUNE_GROUP_FLOAT(PLAYER_TARGETING, DEAD_PED_LOCK_ON_BREAK_VALUE, 50.0f, 0.0f, 100.0f, 1.0f);

	if (eTargetMode == SoftLockOn)
	{
		const float fTimeStep = fwTimer::GetTimeStep();

		if(CanIncrementSoftLockTimer())
		{
			m_fTimeSoftLockedOn += fTimeStep;
		}

		if(!m_bCheckForLockOnSwitchExtension)
		{
			if ((m_fTimeSoftLockedOn > ms_Tunables.GetSoftLockTime()) TARGET_SEQUENCE_ONLY(&& !IsUsingTargetSequence()))
			{
				return true;
			}
			else
			{
				// If allowing fine aim during soft lock, only consider soft lock as broken if we're outside the fine aim limits
				bool bWantsToBreak = false;
				float fBreakTime = m_fFineAimHorizontalSpeedWeight >= 1.0f ? ms_Tunables.GetMinSoftLockBreakAtMaxXStickTime() : ms_Tunables.GetMinSoftLockBreakTime();

				if(ms_Tunables.GetAllowSoftLockFineAim())
				{
					const bool bWithinFineAimLimits = IsFineAimWithinBreakLimits();
					if (m_bWasWithinFineAimLimits)
					{
						bWantsToBreak = !bWithinFineAimLimits;
					}
					else
					{
						m_bWasWithinFineAimLimits = bWithinFineAimLimits;
					}

					if(m_pLockOnTarget)
					{
						Vector3 vLockOnPos = VEC3V_TO_VECTOR3(m_pLockOnTarget->GetTransform().GetPosition());
						Vector3 vPedPos = VEC3V_TO_VECTOR3(m_pPed->GetTransform().GetPosition());
						Vector3 vDistance = vLockOnPos - vPedPos;
						float fDist = Clamp(vDistance.Mag(), ms_Tunables.GetSoftLockBreakDistanceMin(), ms_Tunables.GetSoftLockBreakDistanceMax());

						float fWeight = (fDist - ms_Tunables.GetSoftLockBreakDistanceMin()) / (ms_Tunables.GetSoftLockBreakDistanceMax() - ms_Tunables.GetSoftLockBreakDistanceMin());
						fWeight = Clamp(fWeight, 0.0f, 1.0f);

						float fAbsoluteLimits = ms_Tunables.GetSoftLockFineAimXYAbsoluteValueClose() + ((ms_Tunables.GetSoftLockFineAimXYAbsoluteValue() - ms_Tunables.GetSoftLockFineAimXYAbsoluteValueClose()) * fWeight);

						//! Break immediately if we get outside max XY limits and we are pushing stick to break.
						if(!IsFineAimWithinAbsoluteXYLimits(fAbsoluteLimits) && m_fFineAimHorizontalSpeedWeight > 0.25f)
						{
							fBreakTime = 0.0f;
						}
						else if(!IsFineAimWithinBreakXYLimits() && m_pLockOnTarget)
						{
							//! If outside XY limits, reduce break out time if close to target to stop camera from spinning. Note: We ignore Z, as this
							//! is ok.
							fBreakTime = ms_Tunables.GetMinSoftLockBreakTimeCloseRange() + ((ms_Tunables.GetMinSoftLockBreakTime() - ms_Tunables.GetMinSoftLockBreakTimeCloseRange()) * fWeight);
						}
					}
				}
				else
				{
					bWantsToBreak = IsPushingStick(ms_Tunables.GetSoftLockBreakValue());
				}

				if (bWantsToBreak)
				{
					m_fTimeTryingToBreakSoftLock += fTimeStep;
					if(m_fTimeTryingToBreakSoftLock >= fBreakTime)
					{
						if(ms_Tunables.GetLockOnSwitchTimeExtensionBreakLock() > 0)
						{
							m_pLockOnSwitchExtensionTarget = m_pLockOnTarget;
							m_uTimeLockOnSwitchExtensionExpires = 
								Max(m_uTimeLockOnSwitchExtensionExpires, fwTimer::GetTimeInMilliseconds() + ms_Tunables.GetLockOnSwitchTimeExtensionBreakLock());
						}
						return true;
					};
				}
				else
				{
					m_fTimeTryingToBreakSoftLock = 0.0f;
				}
			}
		}
	}

	// If we have a dead lockon target, disable lockon if not pushing the aim stick
// 	if (HasDeadLockonTarget())
// 	{
// 		return !IsPushingStick(DEAD_PED_LOCK_ON_BREAK_VALUE);
// 	}

	return false;
}

#if USE_TARGET_SEQUENCES
bool CPlayerPedTargeting::IsTargetSequenceReadyToFire() const
{
	if (m_pLockOnTarget)
	{
		return m_TargetSequenceHelper.IsReadyToFire();
	}
	else
	{
		return true;
	}
}

bool CPlayerPedTargeting::IsUsingTargetSequence() const
{
	return m_pPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_ASSISTED_AIMING_ON) && m_pLockOnTarget && m_TargetSequenceHelper.HasSequence();
}

#endif // USE_TARGET_SEQUENCES

bool CPlayerPedTargeting::IsPrecisionAiming()const
{
	Vector3 vUnused;
	return GetFineAimingOffset(vUnused) TARGET_SEQUENCE_ONLY(|| IsUsingTargetSequence() || m_TargetSequenceHelper.IsSequenceFinished());
}

void CPlayerPedTargeting::FinaliseCurveSets()
{
	for(s32 i = 0; i < CTargettingDifficultyInfo::CS_Max; i++)
	{
		ms_Tunables.m_FirstPersonEasyTargettingDifficultyInfo.m_CurveSets[i].Finalise();
		ms_Tunables.m_EasyTargettingDifficultyInfo.m_CurveSets[i].Finalise();

		ms_Tunables.m_FirstPersonNormalTargettingDifficultyInfo.m_CurveSets[i].Finalise();
		ms_Tunables.m_NormalTargettingDifficultyInfo.m_CurveSets[i].Finalise();

		ms_Tunables.m_FirstPersonHardTargettingDifficultyInfo.m_CurveSets[i].Finalise();
		ms_Tunables.m_HardTargettingDifficultyInfo.m_CurveSets[i].Finalise();

		ms_Tunables.m_FirstPersonExpertTargettingDifficultyInfo.m_CurveSets[i].Finalise();
		ms_Tunables.m_ExpertTargettingDifficultyInfo.m_CurveSets[i].Finalise();
	}
}

bool CPlayerPedTargeting::IsDisabledDueToWeaponWheel() const
{
	bool bDisableDueToWeaponWheel = CNewHud::IsShowingHUDMenu() && !m_pLockOnTarget;
	return bDisableDueToWeaponWheel;
}	

bool CPlayerPedTargeting::IsLockOnDisabled() const
{
	if (m_pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DisablePlayerLockon) || IsDisabledDueToWeaponWheel()  || (m_pPed->GetPedIntelligence() && m_pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_CLIMB_LADDER)))
	{
		return true;
	}

	return false;
}

CPlayerPedTargeting::TargetingMode CPlayerPedTargeting::GetTargetingMode(const CWeapon* pWeapon, const CWeaponInfo& weaponInfo)
{
	pedFatalAssertf(m_pPed, "m_pPed is NULL");

	eLockType eTargetLockType = ms_Tunables.GetLockType();

	if (!IsLockOnDisabled())
	{
		bool bSoftFiringNotAiming = fwTimer::GetTimeInMilliseconds() < (m_uTimeSinceSoftFiringAndNotAiming + 100);

		bool bCanLockOn = CanPedLockOnWithWeapon(pWeapon, weaponInfo);

		// B*6465133: Free aim assisted (ie snap lock) shouldn't allow lock-on if already aiming over a target ped (ie we have a free aim target ped).
		if (CPedTargetEvaluator::GetTargetMode(m_pPed) == CPedTargetEvaluator::TARGETING_OPTION_ASSISTED_FREEAIM)
		{
			if (!m_pLockOnTarget && m_pFreeAimTarget && m_pFreeAimTarget->GetIsTypePed())
			{
				bCanLockOn = false;
			}
		}

		if( CPlayerInfo::IsFlyingAircraft() || weaponInfo.GetUseManualTargetingMode() )
		{
			return Manual;
		}
		else if( (CPlayerInfo::IsHardAiming() || CPlayerInfo::IsDriverFiring() || m_pPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_ASSISTED_AIMING_ON)) && bCanLockOn )
		{
			if(weaponInfo.GetForcingDriveByAssistedAim()) //force hard lock on for cars that force assisted aim?
			{	
				return HardLockOn;
			}
			else if( m_pPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_ASSISTED_AIMING_ON) && !m_pPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_NO_RETICULE_AIM_ASSIST_ON))
			{
				if (!m_pPed->GetIsDrivingVehicle() || eTargetLockType != LT_None)
				{
					return HardLockOn;
				}
			}
			if( eTargetLockType == LT_Hard || (weaponInfo.GetIsMelee() && !IsMeleeTargetingUsingNormalLock()) )
			{
				bool bSelectingLeftTarget = false;
				bool bSelectingRightTarget = false;
				if(GetWeaponInfo())
				{
					bSelectingLeftTarget = CPlayerInfo::IsSelectingLeftTarget();
					bSelectingRightTarget = CPlayerInfo::IsSelectingRightTarget();
				}

				//! Don't oscillate between free aim and hard lock during melee.
				bool bForceMeleeHardLock = (weaponInfo.GetIsMelee() && !IsMeleeTargetingUsingNormalLock());

				if ( bForceMeleeHardLock || !m_bPlayerHasStartedToAim || bSoftFiringNotAiming || m_bPlayerHasBrokenLockOn || m_pLockOnTarget || bSelectingLeftTarget || bSelectingRightTarget)
				{
					if (!m_pPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_ASSISTED_AIMING_ON))
					{
						m_bCheckForLeftTarget = bSelectingLeftTarget;
						m_bCheckForRightTarget = bSelectingRightTarget;
						m_bPlayerHasStartedToAim = true;
						m_bPlayerHasBrokenLockOn = false;
					}

					return HardLockOn;
				}
			}
			else if ( eTargetLockType == LT_Soft )
			{
				if (CPlayerPedTargeting::ms_Tunables.GetUseLockOnTargetSwitching())
				{
					if(CPlayerInfo::IsSelectingLeftTarget())
					{
						m_bCheckForLeftTarget = true;
					}
					if(CPlayerInfo::IsSelectingRightTarget())
					{
						m_bCheckForRightTarget = true;
					}
				}

				m_bCheckForLockOnSwitchExtension = false;

				if (!m_bPlayerHasStartedToAim || bSoftFiringNotAiming)
				{
					m_bPlayerHasStartedToAim = true;
					m_bPlayerHasBrokenLockOn = false;  
					return SoftLockOn;
				}
				else // Player has started to soft aim
				{
					// If player hasn't broken lockon and has been locked on less than the max lock on time then continue soft lock
					if (!m_bPlayerHasBrokenLockOn && ((m_fTimeSoftLockedOn < ms_Tunables.GetSoftLockTime()) TARGET_SEQUENCE_ONLY(|| (IsUsingTargetSequence()))))
					{
						return SoftLockOn;
					}

					u32 nCurrentTime = fwTimer::GetTimeInMilliseconds();

					if( (m_bCheckForLeftTarget || m_bCheckForRightTarget) &&
						m_pLockOnSwitchExtensionTarget && 
						(nCurrentTime < m_uTimeLockOnSwitchExtensionExpires) )
					{
						m_bCheckForLockOnSwitchExtension = true;
						return SoftLockOn;
					}

					m_bPlayerHasBrokenLockOn = true;
				}
				// If player has broken a lock on or didn't have a target when beginning to aim
				// fall through to free aim if possible
			}
		}
	}

	bool bPlayerIsSoftFiringNotAiming = CPlayerInfo::IsSoftFiring() && !CPlayerInfo::IsAiming(false) && !CPlayerInfo::IsDriverFiring();
	if(bPlayerIsSoftFiringNotAiming)
	{
		m_uTimeSinceSoftFiringAndNotAiming = fwTimer::GetTimeInMilliseconds();
	}

	// Reset soft aim variables if not aiming so we can reacquire a lock on target
	if( eTargetLockType == LT_Soft )
	{
		if ( (bPlayerIsSoftFiringNotAiming || !CPlayerInfo::IsAiming()) && !CPlayerInfo::IsDriverFiring())
		{
			ClearSoftAim();
		}
	}

	if (weaponInfo.GetCanFreeAim() && (CPlayerInfo::IsAiming() || CPlayerInfo::IsDriverFiring()) && !IsLockOnDisabled())
	{
		// If we've been free aiming for a bit, do not allow player to then lock on
		static bool ENABLE_FREE_AIM_CHECK = true;
		if( eTargetLockType == LT_Soft && ENABLE_FREE_AIM_CHECK )
		{
			dev_float MAX_TIME_TO_ALLOW_INITIAL_SOFT_LOCK = 0.25f;
			if (m_fTimeAiming > MAX_TIME_TO_ALLOW_INITIAL_SOFT_LOCK)
			{
				m_bPlayerHasStartedToAim = true;
				m_bPlayerHasBrokenLockOn = true;
			}
		}
		else if( eTargetLockType == LT_Hard && !CPlayerInfo::IsHardAiming() )
		{
			m_bPlayerHasBrokenLockOn = true;
		}

		m_pPed->GetPlayerInfo()->SetPlayerDataFreeAiming(true);
		return FreeAim;
	}

	m_bPlayerHasStartedToAim = false;
	m_uTimeSinceSoftFiringAndNotAiming = 0;
	m_pPed->GetPlayerInfo()->SetPlayerDataFreeAiming(false);

	return None;
}

void CPlayerPedTargeting::UpdateLockOnTarget()
{
	pedFatalAssertf(m_pPed, "m_pPed is NULL");

	const CWeaponInfo* pWeaponInfo = GetWeaponInfo();

	if(!pWeaponInfo)
	{
		// No weapon
		return;
	}

	CWeapon* pWeapon = m_pPed->GetWeaponManager()->GetEquippedWeapon();
	bool bCanLockon = CanPedLockOnWithWeapon(pWeapon, *pWeaponInfo);
	
	if(!bCanLockon)
	{
		// Selected weapon can not lock-on, clear target
		ClearLockOnTarget();
	}

	bool bAssistedAiming = m_pPed->GetPlayerInfo()->GetPlayerResetFlags().IsFlagSet(CPlayerResetFlags::PRF_ASSISTED_AIMING_ON);

	// Do not allow lock on for peds in driver seat
	if (m_pPed->GetMyVehicle())
	{
		const bool bVehicleHasDriver = m_pPed->GetMyVehicle()->GetLayoutInfo()->GetHasDriver();

		if (!bVehicleHasDriver || m_pPed->GetMyVehicle()->GetSeatManager()->GetDriver() == m_pPed)
		{
			ClearLockOnTarget();
			return;
		}
	}

	if((bCanLockon && (bAssistedAiming || !m_pPed->GetPlayerInfo()->GetPlayerDataFreeAiming() || CPlayerInfo::IsJustHardAiming())))
	{
		// If not locked-on, find a target
		if(!m_pLockOnTarget)
		{
			if(bAssistedAiming || CPlayerInfo::IsJustHardAiming() || (CPlayerInfo::IsHardAiming() && !CTaskPlayerOnFoot::ms_bAnalogueLockonAimControl))
			{
				FindAndSetLockOnTarget();

				if (bAssistedAiming && m_pLockOnTarget)
				{
					Vector3	vecAim = camInterface::GetPlayerControlCamAimFrame().GetFront();
					vecAim.z = 0.0f;
					vecAim.Normalize();
					Vector3 vToTarget = VEC3V_TO_VECTOR3(m_pLockOnTarget->GetTransform().GetPosition() - m_pPed->GetTransform().GetPosition());
					vToTarget.z = 0.0f;
					vToTarget.Normalize();
					const float fDot = vecAim.Dot(vToTarget);
					TUNE_GROUP_FLOAT(ONE_H_AIM, dotTolerance2, 0.85f, 0.0f, 1.0f, 0.01f);
					if (fDot <= dotTolerance2)
					{
						ClearLockOnTarget();
					}

					if(ShouldUsePullGunSpeech(pWeaponInfo))
						m_pPed->NewSay("PULL_GUN");
				}
			}
			else
			{
				ClearLockOnTarget();
			}
		}
		// Already locked on, update lock on camera mode
		else
		{
			if(GetShouldBreakLockOnWithTarget())
			{
				ClearLockOnTarget();
			}
			// If we already have a target we can find neighbouring targets if the player presses the L R buttons
			else if(m_pLockOnTarget)
			{
				if(CPlayerInfo::IsSelectingLeftTarget())
				{
					FindNextLockOnTarget(m_pLockOnTarget, HD_LEFT);
				}
				else if(CPlayerInfo::IsSelectingRightTarget())
				{
					FindNextLockOnTarget(m_pLockOnTarget, HD_RIGHT);
				}

				if(!m_pPed->GetCoverPoint())
				{
					m_pPed->SetDesiredHeading(VEC3V_TO_VECTOR3(m_pLockOnTarget->GetTransform().GetPosition()));
				}

				CControl* pControl = m_pPed->GetControlFromPlayer();
				Vector2 vecStickInput;
				vecStickInput.x = pControl->GetPedWalkLeftRight().GetNorm();
				vecStickInput.y = pControl->GetPedWalkUpDown().GetNorm();

				static dev_float s_fStickThreshold = 0.0f;
				
				// Prevent ped from continually moving forward when locked on and making small movements if no input is received
				if (Abs(vecStickInput.x) > s_fStickThreshold || Abs(vecStickInput.y) > s_fStickThreshold)
				{
					m_pPed->SetIsStrafing(true);
				}
				else
				{
					m_pPed->SetIsStrafing(false);
				}
			}
		}
	}
	else if(m_pPed->GetPlayerInfo()->GetPlayerDataFreeAiming() && 
		bCanLockon &&
		(m_pPed->GetPlayerInfo()->IsHardAiming() || m_pPed->GetPlayerInfo()->IsFiring() || m_pPed->GetPlayerInfo()->IsSelectingLeftTarget() || m_pPed->GetPlayerInfo()->IsSelectingRightTarget()))
	{
		if(m_pPed->GetPlayerInfo()->IsSelectingLeftTarget())
		{
			FindNextLockOnTarget(m_pLockOnTarget, HD_LEFT);
		}
		else if(m_pPed->GetPlayerInfo()->IsSelectingRightTarget())
		{
			FindNextLockOnTarget(m_pLockOnTarget, HD_RIGHT);
		}
	}

	if(!bAssistedAiming && !m_pLockOnTarget && pWeaponInfo->GetCanFreeAim() && (m_pPed->GetPlayerInfo()->IsAiming() || m_pPed->GetPlayerInfo()->IsFiring()))
	{
		m_pPed->GetPlayerInfo()->SetPlayerDataFreeAiming( true );
	}
	else
	{
		m_pPed->GetPlayerInfo()->SetPlayerDataFreeAiming( false );
	}

	m_fLockOnRangeOverride = -1.0f;		// Clear the weapon range override - needs to be set every frame from script
}

void CPlayerPedTargeting::UpdateLockOnTargetPos()
{
	pedFatalAssertf(m_pPed, "m_pPed is NULL");

	const bool bIsAssistedAiming = m_pPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_ASSISTED_AIMING_ON);

	Vector3 vCurrentAimAtPos(Vector3::ZeroType);

	const CWeapon* pWeapon = m_pPed->GetWeaponManager() ? m_pPed->GetWeaponManager()->GetEquippedWeapon() : NULL;
	const CObject* pWeaponObject = m_pPed->GetWeaponManager() ? m_pPed->GetWeaponManager()->GetEquippedWeaponObject() : NULL;

	Vector3 vStart(Vector3::ZeroType);

	if (pWeapon && pWeaponObject)
	{
		const Matrix34& mWeapon = RCC_MATRIX34(pWeaponObject->GetMatrixRef());
		pWeapon->CalcFireVecFromAimCamera(m_pPed, mWeapon, vStart, vCurrentAimAtPos);
	}

	// Update the sequence helper
	TARGET_SEQUENCE_ONLY(m_TargetSequenceHelper.UpdateTime();)

	if (m_pLockOnTarget || bIsAssistedAiming)
	{
		static dev_float NORMAL_LERP_SPEED = 0.6f;
		TUNE_GROUP_FLOAT(PLAYER_TARGETING, ASSISTED_AIMING_APPROACH_RATE, 5.0f, 0.0f, 50.0f, 0.1f);
		TUNE_GROUP_FLOAT(PLAYER_TARGETING, ASSISTED_AIMING_TARGET_SEQUENCE_APPROACH_RATE, 50.0f, 0.0f, 1000.0f, 0.1f);

		if (m_pLockOnTarget)
		{
#if USE_TARGET_SEQUENCES
			if (IsUsingTargetSequence() && bIsAssistedAiming)
			{
				if(m_pLockOnTarget->GetIsTypePed())
				{
					CPed* pTargetPed = static_cast<CPed*>(m_pLockOnTarget.Get());
					if (pTargetPed->GetIsDeadOrDying())
					{
						m_TargetSequenceHelper.Reset();
					}
					else
					{
						pedFatalAssertf(m_pPed->GetPlayerInfo(), "m_pPed GetPlayerInfo() is NULL");

						// If the player is pressing fire repeatedly, override the timing in the target sequence
						if (m_pPed->GetPlayerInfo()->IsJustFiring())
						{
							m_TargetSequenceHelper.ForceShotReady();
						}

						m_TargetSequenceHelper.UpdateTargetPosition(static_cast<CPed*>(m_pLockOnTarget.Get()), vCurrentAimAtPos);
					}
				}
				else
				{
					m_pLockOnTarget->GetLockOnTargetAimAtPos(vCurrentAimAtPos);
				}
				
				if (m_pPed->GetIsDrivingVehicle() || !m_bHasUpdatedLockOnPos)
				{
					m_vLockOnTargetPos = vCurrentAimAtPos;
					m_bHasUpdatedLockOnPos = true;
				}
			}
			else
#endif // USE_TARGET_SEQUENCES
			{
				if (m_pPed->GetIsDrivingVehicle() || (bIsAssistedAiming || !m_bHasUpdatedLockOnPos))
				{
					// We haven't updated the initial lock on position so it will currently be zero, just set it
					// to be the target position
					m_pLockOnTarget->GetLockOnTargetAimAtPos(m_vLockOnTargetPos);
					m_bHasUpdatedLockOnPos = true;
				}

				m_pLockOnTarget->GetLockOnTargetAimAtPos(vCurrentAimAtPos);
			}
		}

		if (bIsAssistedAiming)
		{
			//! DMKH. Removing this. It doesn't make sense to alter the target position here. If this causes a problem for external code, they 
			//! should fix it themselves.
			/*if (!IsUsingTargetSequence())
			{
				Vector3 vToAimPos = vCurrentAimAtPos - vStart;
				vToAimPos.Normalize();
				vCurrentAimAtPos = vStart + vToAimPos * CTaskAimGunOnFoot::ms_fTargetDistanceFromCameraForAimIk;  
#if DEBUG_DRAW
				CPedAccuracy::ms_debugDraw.AddSphere(RCC_VEC3V(vCurrentAimAtPos), 0.025f, Color_purple, 100);
#endif
			}*/

			if (!m_pLockOnTarget)
			{
				m_vLockOnTargetPos = vCurrentAimAtPos;
			}
		}

		TUNE_GROUP_BOOL(PLAYER_TARGETING, USE_LERP, false);
		if (bIsAssistedAiming && !USE_LERP)
		{
			m_vLockOnTargetPos.ApproachStraight(vCurrentAimAtPos, TARGET_SEQUENCE_ONLY(IsUsingTargetSequence() ? ASSISTED_AIMING_TARGET_SEQUENCE_APPROACH_RATE :) ASSISTED_AIMING_APPROACH_RATE, fwTimer::GetTimeStep());
		}
		else
		{
			TUNE_GROUP_FLOAT(PLAYER_TARGETING, ASSISTED_AIMING_LERP_RATE, 0.1f, 0.0f, 1.0f, 0.01f);
			float fLerpSpeed = bIsAssistedAiming ? ASSISTED_AIMING_LERP_RATE :NORMAL_LERP_SPEED;
			fLerpSpeed *= fwTimer::GetTimeStep() / (1.0f / 30.0f);
			m_vLockOnTargetPos.Lerp(fLerpSpeed, vCurrentAimAtPos);
		}

		Vector3 vDiff = m_vLockOnTargetPos - vCurrentAimAtPos;

		static dev_float MAX_OFFSET = 0.75f;
		if(!bIsAssistedAiming && vDiff.Mag2() > rage::square(MAX_OFFSET))
		{
			vDiff.Normalize();
			vDiff.Scale(MAX_OFFSET);
			m_vLockOnTargetPos = vCurrentAimAtPos + vDiff;
		}

#if DEBUG_DRAW
		static u32 LOCK_ON_TARGET_POS_DEBUG_1 = ATSTRINGHASH("LOCK_ON_TARGET_POS_DEBUG_1", 0x50CE1003);
		static u32 LOCK_ON_TARGET_POS_DEBUG_2 = ATSTRINGHASH("LOCK_ON_TARGET_POS_DEBUG_2", 0x48BFFFE7);
		CPedAccuracy::ms_debugDraw.AddLine(RCC_VEC3V(vCurrentAimAtPos), RCC_VEC3V(m_vLockOnTargetPos), Color_red, 100, LOCK_ON_TARGET_POS_DEBUG_1);
		CPedAccuracy::ms_debugDraw.AddSphere(RCC_VEC3V(m_vLockOnTargetPos), 0.025f, Color_red, 100, LOCK_ON_TARGET_POS_DEBUG_2);
#endif // DEBUG_DRAW

	}
	else if (!bIsAssistedAiming)
	{
		// Clear the position
		m_vLockOnTargetPos.Zero();
	}

	if(NetworkInterface::IsGameInProgress() && m_pLockOnTarget)
	{
		//! Test warp
		/*static dev_bool s_bTestWarp = false;
		if(m_fTimeAiming > 1.0f && s_bTestWarp)
		{
			Vector3 vTestWarpPos = VEC3V_TO_VECTOR3(m_pPed->GetTransform().GetPosition());

			static dev_float s_fTestWarpA = 20.0f;
			static dev_float s_fTestWarpB = -20.0f;

			vTestWarpPos += VEC3V_TO_VECTOR3(m_pPed->GetTransform().GetA()) * s_fTestWarpA;
			vTestWarpPos += VEC3V_TO_VECTOR3(m_pPed->GetTransform().GetB()) * s_fTestWarpB;
			m_pLockOnTarget->SetPosition(vTestWarpPos);
		}*/

		//! Do a test to see if target has warped.
		static dev_float s_fWarpTolerance = 10.0f;
		if(!m_vLockOnTargetPos.IsClose(m_vLastLockOnTargetPos, s_fWarpTolerance) )
		{
			ClearLockOnTarget();
		}
	}

	m_vLastLockOnTargetPos = m_vLockOnTargetPos;
}

s32 CPlayerPedTargeting::GetFindTargetFlags() const
{
	// Construct targeting flags
	s32 iFlags = CPedTargetEvaluator::DEFAULT_TARGET_FLAGS;

	// Allow auto targeting of players in vehicles if we're on foot
	if (NetworkInterface::IsGameInProgress() && !m_pPed->GetIsInVehicle())
	{
		iFlags |= CPedTargetEvaluator::TS_LOCKON_TO_PLAYERS_IN_VEHICLES;
	}

	// If we have been aiming for a while, disable the distance scoring
	// This will allow us to more accurately lock-on to peds under the reticule
	static dev_float AIM_TIME_TO_IGNORE_DISTANCE = 0.8f;
	if(m_fTimeAiming > AIM_TIME_TO_IGNORE_DISTANCE)
	{
		iFlags &= ~CPedTargetEvaluator::TS_USE_DISTANCE_SCORING;
	}

	// If the weapon can lock-on to vehicles, or we are in a vehicle, enable all vehicles as targets
	const CWeaponInfo* pWeaponInfo = GetWeaponInfo();
	pedFatalAssertf(pWeaponInfo, "Weapon Info is NULL");
	if(pWeaponInfo->GetIsHoming() || m_pPed->GetVehiclePedInside() != NULL)
	{
		iFlags |= CPedTargetEvaluator::TS_CHECK_VEHICLES;
	}

	//! Do Aim vector bonus.
	static dev_bool s_bDoAimBonus = false;
	if(s_bDoAimBonus)
	{
		iFlags |= CPedTargetEvaluator::TS_DO_AIMVECTOR_BONUS_AND_TASK_MODIFIER;
	}

	bool bUseCoverVantagePoint = false;

#if __BANK
	if(PARAM_enablecovervantagepointforlos.Get())
	{	
		bUseCoverVantagePoint = true;
	}
#endif // __BANK

	//! Don't use vantage point if assisted aiming as it will slow things down as you won't actually hit the target 
	//! if they are in cover.
	if(m_pPed->GetPlayerInfo()->IsAssistedAiming())
	{
		bUseCoverVantagePoint = false;
	}

	if(bUseCoverVantagePoint)
	{
		iFlags |= CPedTargetEvaluator::TS_USE_COVER_VANTAGE_POINT_FOR_LOS;
	}

	if(CPedTargetEvaluator::ms_Tunables.m_UseNonNormalisedScoringForPlayer)
	{
		iFlags |= CPedTargetEvaluator::TS_USE_NON_NORMALIZED_SCORE;
	}

	u32 nTimeSinceAiming = fwTimer::GetTimeInMilliseconds() - m_uTimeSinceLastAiming; 
	if(nTimeSinceAiming < CPedTargetEvaluator::ms_Tunables.m_TargetDistanceMaxWeightingAimTime)
	{
		iFlags |= CPedTargetEvaluator::TS_DO_COVER_LOCKON_REJECTION;
	}

	return iFlags;
} 

bool CPlayerPedTargeting::FindAndSetLockOnTarget()
{
	pedFatalAssertf(m_pPed, "m_pPed is NULL");

	// Get our held weapon info
	const CWeaponInfo* pWeaponInfo = GetWeaponInfo();
	if(!pWeaponInfo)
	{
		// No weapon
		return false;
	}

	// Do not allow lock-on targeting with static reticule
	if(pWeaponInfo->GetIsStaticReticulePosition())
	{
		return false;
	}

	s32 iFlags = GetFindTargetFlags();

	// Check if we're already locked on
	if(m_pLockOnTarget && !CPedTargetEvaluator::GetIsTargetingDisabled(m_pPed, m_pLockOnTarget, iFlags))
	{
		// Cache the weapon range (also check to see if overriding from script)
		const float fWeaponRange = GetWeaponLockOnRange(pWeaponInfo, m_pLockOnTarget);

		// Find out if the target is still valid
		// Check if ped is in this weapon's range
		const Vector3 vLockOnTargetPos = VEC3V_TO_VECTOR3(m_pLockOnTarget->GetTransform().GetPosition());
		const Vector3 vPedPosition = VEC3V_TO_VECTOR3(m_pPed->GetTransform().GetPosition());

		Vector2 v2DPedDistance;
		v2DPedDistance.x = vLockOnTargetPos.x - vPedPosition.x;
		v2DPedDistance.y = vLockOnTargetPos.y - vPedPosition.y;
		
		float fPedDistance = v2DPedDistance.Mag();
		if(fPedDistance > fWeaponRange)
		{
			ClearLockOnTarget();
		}
	}
	else
	{
		// Prepare to find a new target
		CEntity* pTargetEntity = m_pCachedLockOnTarget ? m_pCachedLockOnTarget : FindLockOnTarget();

		// Check if we found a valid target
		if(pTargetEntity)
		{
			// We found a target within weapon range - lock-on to it
			SetLockOnTarget(pTargetEntity);
			if(ShouldUsePullGunSpeech(pWeaponInfo))
				m_pPed->NewSay("PULL_GUN");
		}
	}

	return m_pLockOnTarget != NULL;
}

CEntity *CPlayerPedTargeting::FindLockOnTarget()
{
	CEntity* pTargetEntity = NULL;

	const CWeaponInfo* pWeaponInfo = GetWeaponInfo();
	if(pWeaponInfo)
	{
		//! DMKH. Don't do melee targeting in cover.
		if(pWeaponInfo->GetIsMelee() && !IsMeleeTargetingUsingNormalLock())
		{
			pTargetEntity = FindLockOnMeleeTarget(m_pPed, true);
		}
		else
		{
			float fWeaponRange = GetWeaponLockOnRange(pWeaponInfo);

			bool bAllowNoReticule = true;
			if(pWeaponInfo->GetIsMelee())
			{
				fWeaponRange = ms_Tunables.m_UnarmedInCoverTargetingDistance;
				bAllowNoReticule = false;
			}

			CPedTargetEvaluator::tHeadingLimitData headingData;

			float fPitchMin = 0.0f;
			float fPitchMax = 0.0f;
			float fDistanceWeighting = CPedTargetEvaluator::ms_Tunables.m_TargetDistanceWeightingMax;
			float fHeadingWeighting = CPedTargetEvaluator::ms_Tunables.m_TargetHeadingWeighting;
			float fDistanceFallOff = CPedTargetEvaluator::ms_Tunables.m_TargetDistanceFallOffMax;

			if( bAllowNoReticule && m_pPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_NO_RETICULE_AIM_ASSIST_ON) )
			{
				fWeaponRange *= ms_Tunables.GetNoReticuleLockOnRangeModifier();
				fWeaponRange = Min(fWeaponRange, ms_Tunables.GetNoReticuleMaxLockOnRange());
				headingData.fHeadingAngularLimit = ms_Tunables.GetNoReticuleTargetAngularLimit();
				headingData.fHeadingAngularLimitClose = ms_Tunables.GetNoReticuleTargetAngularLimitClose();
				headingData.fHeadingAngularLimitCloseDistMin = ms_Tunables.GetNoReticuleTargetAngularLimitCloseDistMin();
				headingData.fHeadingAngularLimitCloseDistMax = ms_Tunables.GetNoReticuleTargetAngularLimitCloseDistMax();
				fPitchMin = -ms_Tunables.GetNoReticuleTargetAngularLimitClose();
				fPitchMax =  ms_Tunables.GetNoReticuleTargetAngularLimitClose();
				fDistanceWeighting = 1.0f;
				fHeadingWeighting = 1.0f;
				headingData.fAimPitchLimitMin = ms_Tunables.GetNoReticuleTargetAimPitchLimit();
				headingData.fAimPitchLimitMax = ms_Tunables.GetNoReticuleTargetAimPitchLimit();
			}
			else
			{
				fWeaponRange *= ms_Tunables.GetLockOnRangeModifier();
				headingData.fHeadingAngularLimitClose = ms_Tunables.GetDefaultTargetAngularLimitClose();
				headingData.fHeadingAngularLimitCloseDistMin = ms_Tunables.GetDefaultTargetAngularLimitCloseDistMin();
				headingData.fHeadingAngularLimitCloseDistMax = ms_Tunables.GetDefaultTargetAngularLimitCloseDistMax();

				u32 nTimeSinceAiming = fwTimer::GetTimeInMilliseconds() - m_uTimeSinceLastAiming; 

				if(nTimeSinceAiming < CPedTargetEvaluator::ms_Tunables.m_TargetDistanceMaxWeightingAimTime)
				{
					float fWeight = (float)nTimeSinceAiming/(float)CPedTargetEvaluator::ms_Tunables.m_TargetDistanceMaxWeightingAimTime;

					float fMinWeighting = CPedTargetEvaluator::ms_Tunables.m_TargetDistanceWeightingMin;
					float fMaxWeighting = CPedTargetEvaluator::ms_Tunables.m_TargetDistanceWeightingMax;

					fDistanceWeighting = fMinWeighting + ((fMaxWeighting - fMinWeighting) * fWeight);
					
					float fMinFallOff = CPedTargetEvaluator::ms_Tunables.m_TargetDistanceFallOffMin;
					float fMaxFallOff = CPedTargetEvaluator::ms_Tunables.m_TargetDistanceFallOffMax;

					fDistanceFallOff = fMinFallOff + ((fMaxFallOff - fMinFallOff) * fWeight);
				}

				float fAngularRange = ms_Tunables.GetDefaultTargetAngularLimit();
				if (m_pPed->GetIsDrivingVehicle() && m_pPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_ASSISTED_AIMING_ON))
				{
					TUNE_GROUP_FLOAT(DRIVE_BY_TUNE, DRIVEBY_ANGULAR_RANGE, 90.0f, 0.0f, 180.0f, 1.0f);
					fAngularRange = DRIVEBY_ANGULAR_RANGE;
				}

				headingData.fHeadingAngularLimit = fAngularRange;
				headingData.fWideHeadingAngulerLimit = ms_Tunables.GetWideTargetAngularLimit();

				headingData.fAimPitchLimitMin = ms_Tunables.GetDefaultTargetAimPitchMin();
				headingData.fAimPitchLimitMax = ms_Tunables.GetDefaultTargetAimPitchMax();
			}

			pTargetEntity = CPedTargetEvaluator::FindTarget(m_pPed, 
				GetFindTargetFlags(), 
				fWeaponRange, 
				fWeaponRange * ms_Tunables.GetLockOnDistanceRejectionModifier(),
				NULL, 
				NULL, 
				headingData,
				fPitchMin,
				fPitchMax,
				fDistanceWeighting,
				fHeadingWeighting,
				fDistanceFallOff);
		}
	}

	return pTargetEntity;
}

CEntity *CPlayerPedTargeting::FindLockOnMeleeTarget(const CPed *pPed, bool bAllowLockOnIfTargetFriendly) 
{
	const CWeaponInfo* pWeaponInfo = GetWeaponInfo();
	if(!pWeaponInfo)
	{
		return NULL;
	}

	float fWeaponRange = GetWeaponLockOnRange(pWeaponInfo);

	s32 nDefaultFlags = CPedTargetEvaluator::MELEE_TARGET_FLAGS;

	CPedTargetEvaluator::tHeadingLimitData headingData;
	
	CControl* pControl = const_cast<CControl*>(pPed->GetControlFromPlayer());
	if( pControl )
	{
		// Check the desired direction input
		const Vector2 vecStickInput( pControl->GetPedWalkLeftRight().GetNorm(), -pControl->GetPedWalkUpDown().GetNorm() );
		const float fInputMag = vecStickInput.Mag2();
		if( fInputMag > rage::square( 0.5f ) )
		{
			nDefaultFlags |= CPedTargetEvaluator::TS_DESIRED_DIRECTION_HEADING;
			nDefaultFlags |= CPedTargetEvaluator::TS_CAMERA_HEADING;

			headingData.fHeadingAngularLimit = CPedTargetEvaluator::ms_Tunables.m_DefaultTargetAngularLimitMelee;
			
			headingData.fStickHeadingWeighting = CPedTargetEvaluator::ms_Tunables.m_MeleeLockOnStickWeighting;
			headingData.fCameraHeadingWeighting = CPedTargetEvaluator::ms_Tunables.m_MeleeLockOnCameraWeighting;
		}
		else
		{
			nDefaultFlags |= CPedTargetEvaluator::TS_CAMERA_HEADING;
			nDefaultFlags |= CPedTargetEvaluator::TS_PED_HEADING;
			
			headingData.fPedHeadingWeighting = CPedTargetEvaluator::ms_Tunables.m_MeleeLockOnCameraWeightingNoStick;
			headingData.fCameraHeadingWeighting = CPedTargetEvaluator::ms_Tunables.m_MeleeLockOnPedWeightingNoStick;

			headingData.bPedHeadingNeedsTargetOnScreen = true;

			headingData.fHeadingAngularLimit = CPedTargetEvaluator::ms_Tunables.m_DefaultTargetAngularLimitMeleeLockOnNoStick;
		}
	}

	//! Allows us to lock on to peds that are allowing friendly lock on (e.g. chop).
	if(bAllowLockOnIfTargetFriendly)
	{
		nDefaultFlags |= CPedTargetEvaluator::TS_ALLOW_LOCK_ON_IF_TARGET_IS_FRIENDLY;
	}

	headingData.fHeadingFalloffPower = 1.0f;

	CEntity *pTargetEntity = CPedTargetEvaluator::FindTarget( pPed, 
		nDefaultFlags, 
		fWeaponRange, 
		fWeaponRange, 
		NULL, 
		NULL, 
		headingData, 
		0.0f, 
		0.0f, 
		CPedTargetEvaluator::ms_Tunables.m_DefaultTargetDistanceWeightMelee, 
		CPedTargetEvaluator::ms_Tunables.m_DefaultTargetHeadingWeightMelee );
	
	return pTargetEntity;	
}

CEntity *CPlayerPedTargeting::FindMeleeTarget(const CPed *pPed, 
											  const CWeaponInfo* pWeaponInfo, 
											  bool bDoMeleeActionCheck, 
											  bool bTargetDeadPeds,
											  bool bFallBackToPedHeading,
											  bool bMustUseDesiredDirection,
											  const CEntity* pCurrentMeleeTarget)
{
	// Find range of current weapon
	CPedTargetEvaluator::tHeadingLimitData headingData; 
	headingData.fHeadingAngularLimit = CPedTargetEvaluator::ms_Tunables.m_DefaultTargetAngularLimitMelee;
	headingData.fHeadingFalloffPower = 1.0f;
	float fPitchLimitMin = 0.0f;
	float fPitchLimitMax = 0.0f;
	float fWeaponRange = CPedTargetEvaluator::ms_Tunables.m_DefaultMeleeRange;  // Default to the unarmed range.

	if( pWeaponInfo )
	{
		const CAimingInfo* pAimingInfo = pWeaponInfo->GetAimingInfo();
		if( pAimingInfo )
		{
			headingData.fHeadingAngularLimit = pAimingInfo->GetHeadingLimit();
			fPitchLimitMin = pAimingInfo->GetSweepPitchMin();
			fPitchLimitMax = pAimingInfo->GetSweepPitchMax();
		}

		fWeaponRange = pWeaponInfo->GetLockOnRange();
	}

	float fHeadingWeighting = CPedTargetEvaluator::ms_Tunables.m_DefaultTargetHeadingWeightMelee;
	float fDistanceWeighting = CPedTargetEvaluator::ms_Tunables.m_DefaultTargetDistanceWeightMelee;

#if FPS_MODE_SUPPORTED
	bool bIsFPSMode = pPed->IsFirstPersonShooterModeEnabledForPlayer(false);
	if( bIsFPSMode )
	{
		fHeadingWeighting = CPedTargetEvaluator::ms_Tunables.m_DefaultTargetHeadingWeightMeleeFPS;
		fDistanceWeighting = CPedTargetEvaluator::ms_Tunables.m_DefaultTargetDistanceWeightMeleeFPS;
	}
#endif

	if(CPedTargetEvaluator::ms_Tunables.m_UseMeleeHeadingOverride)
	{
		float fRunningScale = 0.0;
		if(pPed->GetMotionData()->GetCurrentMbrY() > MOVEBLENDRATIO_WALK)
		{
			fRunningScale = Min(pPed->GetMotionData()->GetCurrentMbrY() - MOVEBLENDRATIO_WALK / 1.0f, 1.0f);
		}
		
		headingData.fHeadingAngularLimit = 
			CPedTargetEvaluator::ms_Tunables.m_MeleeHeadingOverride + ( (CPedTargetEvaluator::ms_Tunables.m_MeleeHeadingOverrideRunning - CPedTargetEvaluator::ms_Tunables.m_MeleeHeadingOverride) * fRunningScale );

		headingData.fHeadingFalloffPower = 1.0f + ((CPedTargetEvaluator::ms_Tunables.m_MeleeHeadingFalloffPowerRunning-1.0f) * fRunningScale);

#if FPS_MODE_SUPPORTED
		if( bIsFPSMode )
		{
			fHeadingWeighting = CPedTargetEvaluator::ms_Tunables.m_DefaultTargetHeadingWeightMeleeFPS + ((CPedTargetEvaluator::ms_Tunables.m_DefaultTargetHeadingWeightMeleeRunningFPS-CPedTargetEvaluator::ms_Tunables.m_DefaultTargetHeadingWeightMeleeFPS) * fRunningScale);;
			fDistanceWeighting = CPedTargetEvaluator::ms_Tunables.m_DefaultTargetDistanceWeightMeleeFPS + ((CPedTargetEvaluator::ms_Tunables.m_DefaultTargetDistanceWeightMeleeRunningFPS-CPedTargetEvaluator::ms_Tunables.m_DefaultTargetDistanceWeightMeleeFPS) * fRunningScale);
		}
		else
#endif
		{
			fHeadingWeighting = CPedTargetEvaluator::ms_Tunables.m_DefaultTargetHeadingWeightMelee + ((CPedTargetEvaluator::ms_Tunables.m_DefaultTargetHeadingWeightMeleeRunning-CPedTargetEvaluator::ms_Tunables.m_DefaultTargetHeadingWeightMelee) * fRunningScale);
			fDistanceWeighting = CPedTargetEvaluator::ms_Tunables.m_DefaultTargetDistanceWeightMelee + ((CPedTargetEvaluator::ms_Tunables.m_DefaultTargetDistanceWeightMeleeRunning-CPedTargetEvaluator::ms_Tunables.m_DefaultTargetDistanceWeightMelee) * fRunningScale);
		}
	}

	if(CPedTargetEvaluator::ms_Tunables.m_LimitMeleeRangeToDefault && (fWeaponRange > CPedTargetEvaluator::ms_Tunables.m_DefaultMeleeRange) )
	{
		fWeaponRange = CPedTargetEvaluator::ms_Tunables.m_DefaultMeleeRange;
	}

	CEntity *pTargetEntity = NULL;

	s32 nDefaultFlags = CPedTargetEvaluator::MELEE_TARGET_FLAGS | CPedTargetEvaluator::TS_USE_MELEE_LOS_POSITION;
	if(bDoMeleeActionCheck)
	{
		nDefaultFlags |= CPedTargetEvaluator::TS_CALCULATE_MELEE_ACTION;
	}

	if(bTargetDeadPeds)
	{
		nDefaultFlags |= CPedTargetEvaluator::TS_ACCEPT_INJURED_AND_DEAD_PEDS;
	}

	CPedTargetEvaluator::tFindTargetParams testParams;

	CControl* pControl = const_cast<CControl*>(pPed->GetControlFromPlayer());
	if( pControl )
	{
		// Check the desired direction input
		const Vector2 vecStickInput( pControl->GetPedWalkLeftRight().GetNorm(), -pControl->GetPedWalkUpDown().GetNorm() );
		const float fInputMag = vecStickInput.Mag2();
		if( fInputMag > rage::square( 0.5f ) )
		{
			nDefaultFlags |= CPedTargetEvaluator::TS_DESIRED_DIRECTION_HEADING;
		}
		else
		{
			//! If no stick input, see if we can use cached stick input
			if(fwTimer::GetTimeInMilliseconds() < (m_LastTimeDesiredStickInputSet + ms_Tunables.m_TimeToAllowCachedStickInputForMelee))
			{
				const float fInputMag = m_vLastValidStickInput.Mag2();
				if( fInputMag > rage::square( 0.5f ) )
				{
					testParams.pvDesiredStickOverride = &m_vLastValidStickInput;
					nDefaultFlags |= CPedTargetEvaluator::TS_DESIRED_DIRECTION_HEADING;
				}
			}
		}
	}

	if(!(nDefaultFlags & CPedTargetEvaluator::TS_DESIRED_DIRECTION_HEADING))
	{
		//! If we have no stick input, and we are asking for it, then bail, we aren't allowed to find a target.
		if(bMustUseDesiredDirection)
		{
			return NULL;
		}

		//! If we want to fallback to ped heading (as opposed to 360), then do so only if we aren't choosing stick direction.
		if( bFallBackToPedHeading )
		{
			nDefaultFlags |= CPedTargetEvaluator::TS_PED_HEADING;
		}

#if FPS_MODE_SUPPORTED
		//! If we're in first-person mode then ensure we use the camera heading.
		if( bIsFPSMode )
		{
			nDefaultFlags |= CPedTargetEvaluator::TS_CAMERA_HEADING;
		}
#endif
	}

	//! Force camera heading underwater. Need to use some form of distance scoring.
	CTaskMotionBase *pMotionTask = pPed->GetCurrentMotionTask();
	bool bUnderWater = pMotionTask && pMotionTask->IsUnderWater();
	if(bUnderWater)
	{
		nDefaultFlags |= CPedTargetEvaluator::TS_CAMERA_HEADING;
	}

	testParams.pTargetingPed = pPed;
	testParams.iFlags = nDefaultFlags;
	testParams.fMaxDistance = fWeaponRange;
	testParams.fMaxRejectionDistance = fWeaponRange;
	testParams.headingLimitData = headingData;
	testParams.fPitchLimitMin = fPitchLimitMin;
	testParams.fPitchLimitMax = fPitchLimitMax;
	testParams.fDistanceWeight = fDistanceWeighting;
	testParams.fHeadingWeight = fHeadingWeighting;
	testParams.pCurrentMeleeTarget = pCurrentMeleeTarget;

	// First try to find a target that this player is attempting to attack based on controller stick direction
	pTargetEntity = CPedTargetEvaluator::FindTarget( testParams );

	return pTargetEntity;
}

bool CPlayerPedTargeting::FindNextLockOnTarget(CEntity* pOldTarget, HeadingDirection eDirection, bool bSetTarget)
{
	// Get our held weapon info
	const CWeaponInfo* pWeaponInfo = GetWeaponInfo();
	if(!pWeaponInfo)
	{
		// No weapon
		return false;
	}

	// Do not allow lock-on targeting with static reticule
	if(pWeaponInfo->GetIsStaticReticulePosition())
	{
		return false;
	}

	float fLockOnRange = GetWeaponLockOnRange(pWeaponInfo);

	s32 iFlags = 0;

	// Allow auto targeting of players in vehicles if we're on foot
	if (NetworkInterface::IsGameInProgress() && !m_pPed->GetIsInVehicle())
	{
		iFlags |= CPedTargetEvaluator::TS_LOCKON_TO_PLAYERS_IN_VEHICLES;
	}

	CPedTargetEvaluator::tHeadingLimitData headingData;

	float fDistanceWeight;
	float fHeadingWeight;

	fDistanceWeight = CPedTargetEvaluator::ms_Tunables.m_TargetDistanceWeightingMax;
	fHeadingWeight = CPedTargetEvaluator::ms_Tunables.m_TargetHeadingWeighting;

	if(pWeaponInfo->GetIsMelee() && !IsMeleeTargetingUsingNormalLock())
	{
		iFlags |= CPedTargetEvaluator::MELEE_TARGET_FLAGS;
		iFlags |= CPedTargetEvaluator::TS_CAMERA_HEADING;
		
		headingData.fHeadingAngularLimit = ms_Tunables.GetCycleTargetAngularLimitMelee();
		headingData.fHeadingFalloffPower = 1.0f;
#if FPS_MODE_SUPPORTED
		if(m_pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
		{
			fDistanceWeight = CPedTargetEvaluator::ms_Tunables.m_DefaultTargetDistanceWeightMeleeFPS;
			fHeadingWeight = CPedTargetEvaluator::ms_Tunables.m_DefaultTargetHeadingWeightMeleeFPS;
		}
		else
#endif
		{
			fDistanceWeight = CPedTargetEvaluator::ms_Tunables.m_DefaultTargetDistanceWeightMelee;
			fHeadingWeight = CPedTargetEvaluator::ms_Tunables.m_DefaultTargetHeadingWeightMelee;
		}

		headingData.bCamHeadingNeedsTargetOnScreen = true;

		iFlags |= ((eDirection == HD_LEFT) ? CPedTargetEvaluator::TS_CYCLE_LEFT : CPedTargetEvaluator::TS_CYCLE_RIGHT);
	}
	else
	{
		if(pWeaponInfo->GetIsMelee())
		{
			fLockOnRange = ms_Tunables.m_UnarmedInCoverTargetingDistance;
		}

		headingData.fHeadingAngularLimit = ms_Tunables.GetCycleTargetAngularLimit();
		headingData.fWideHeadingAngulerLimit = ms_Tunables.GetWideTargetAngularLimit();
		headingData.fAimPitchLimitMin = ms_Tunables.GetDefaultTargetAimPitchMin();
		headingData.fAimPitchLimitMax = ms_Tunables.GetDefaultTargetAimPitchMax();

		iFlags |= ((eDirection == HD_LEFT) ? CPedTargetEvaluator::CYCLE_LEFT_TARGET_FLAGS : CPedTargetEvaluator::CYCLE_RIGHT_TARGET_FLAGS);


#if FPS_MODE_SUPPORTED
		//! In 1st person, don't allow us to lock on to peds that are off screen.
		if(m_pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
		{
			headingData.bCamHeadingNeedsTargetOnScreen = true;
		}
#endif
	}

	float fRejectionRange = fLockOnRange * ms_Tunables.GetLockOnDistanceRejectionModifier();

	CEntity* pTargetEntity = pTargetEntity = CPedTargetEvaluator::FindTarget(m_pPed, 
		iFlags, 
		fLockOnRange, 
		fRejectionRange, 
		pOldTarget, 
		NULL, 
		headingData,
		0.0f,
		0.0f,
		fDistanceWeight,
		fHeadingWeight);

	if(bSetTarget && pTargetEntity && pTargetEntity != pOldTarget)
	{
		SetLockOnTarget(pTargetEntity, true);
	}

	return m_pLockOnTarget != NULL;
}

bool CPlayerPedTargeting::GetShouldBreakSoftLockTarget()
{
	if(m_pLockOnTarget && m_pLockOnTarget->GetIsTypePed())
	{
		const CPed* pTargetPed = static_cast<const CPed*>(m_pLockOnTarget.Get());
		const CWeaponInfo* pWeaponInfo = GetWeaponInfo();

		//! Break if ped does a combat roll.
		if(pTargetPed->GetPedResetFlag(CPED_RESET_FLAG_BreakTargetLock) && (!pWeaponInfo || !pWeaponInfo->GetIsMelee()))
		{
			return true;
		}
	}

	return false;
}

bool CPlayerPedTargeting::GetShouldBreakLockOnWithTarget()
{
	pedFatalAssertf(m_pPed, "m_pPed is NULL");

	if(m_pLockOnTarget)
	{
		bool bForceLockOnUpdate = false;
		if (m_pPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_ASSISTED_AIMING_ON))
		{
			TUNE_GROUP_BOOL(PLAYER_TARGETING, ENABLE_ASSISTED_ANGLE_CHECK, false);
			if (ENABLE_ASSISTED_ANGLE_CHECK)
			{
				Vector3	vecAim = camInterface::GetPlayerControlCamAimFrame().GetFront();
				vecAim.z = 0.0f;
				vecAim.Normalize();
				Vector3 vToTarget = VEC3V_TO_VECTOR3(m_pLockOnTarget->GetTransform().GetPosition() - m_pPed->GetTransform().GetPosition());
				vToTarget.z = 0.0f;
				vToTarget.Normalize();
				const float fDot = vecAim.Dot(vToTarget);
				TUNE_GROUP_FLOAT(PLAYER_TARGETING, dotTolerance, 0.85f, 0.0f, 1.0f, 0.01f);
				if (fDot > dotTolerance)
					bForceLockOnUpdate = true;
			}
			else
			{
				bForceLockOnUpdate = true;
			}
		}

		if(!CPlayerInfo::IsHardAiming() && !CPlayerInfo::IsDriverFiring() && !bForceLockOnUpdate)
		{
			// Release lock if the lockon isn't held down hard
			return true;
		}

		const CWeaponInfo* pWeaponInfo = GetWeaponInfo();
		if(!pWeaponInfo)
		{
			// No weapon
			return true;
		}

		CWeapon* pWeapon = m_pPed->GetWeaponManager()->GetEquippedWeapon();
		
		bool bCanLockon = CanPedLockOnWithWeapon(pWeapon, *pWeaponInfo);

		if(!bCanLockon)
		{
			// Weapon can't lock-on
			return true;
		}

		s32 iFlags = CPedTargetEvaluator::DEFAULT_TARGET_FLAGS;
		if(pWeaponInfo->GetIsMelee() && !IsMeleeTargetingUsingNormalLock())
		{
			iFlags = CPedTargetEvaluator::MELEE_TARGET_FLAGS;

			//! If we lose LOS, break lock.
			if(GetShouldBreakLockTargetIfNoLOS(iFlags, ms_Tunables.m_MeleeLostLOSBreakTime, 250))
			{
				return true;
			}
		}

		// This will test to see if the current target is still a valid lockon target
		if(CPedTargetEvaluator::GetIsTargetingDisabled(m_pPed, m_pLockOnTarget, iFlags))
		{
			// Targeting disabled
			return true;
		}

		if(m_pLockOnTarget->GetIsTypePed())
		{
			const CPed* pTargetPed = static_cast<const CPed*>(m_pLockOnTarget.Get());

			if( pWeaponInfo->GetIsMelee() )
			{
				// Are they dead or dying
				if( !pTargetPed->GetTargetWhenInjuredAllowed() && pTargetPed->GetIsDeadOrDying() )
				{
					return true;
				}
			}

			// Allow auto targeting of players in vehicles if we're on foot
			if (NetworkInterface::IsGameInProgress() && !m_pPed->GetIsInVehicle())
			{
				iFlags |= CPedTargetEvaluator::TS_LOCKON_TO_PLAYERS_IN_VEHICLES;
			}

			if(!CPedTargetEvaluator::GetCanPedBeTargetedInAVehicle(m_pPed, pTargetPed, iFlags))
			{
				// Can't target ped in a vehicle
				return true;
			}

			if(!CLocalisation::KickingWhenDown())
			{
				if(pTargetPed->GetIsDeadOrDying())
				{
					// Break lock-on when target is dead due to localisation reasons
					return true;
				}
			}

			//! Break if ped does a combat roll.
			if(pTargetPed->GetPedResetFlag(CPED_RESET_FLAG_BreakTargetLock) && !pWeaponInfo->GetIsMelee())
			{
				return true;
			}
		}

		// Check if we have gone outside of lock-on range
		const Vector3 vPedPosition = VEC3V_TO_VECTOR3(m_pPed->GetTransform().GetPosition());
		const Vector3 vLockOnTargetPosition = VEC3V_TO_VECTOR3(m_pLockOnTarget->GetTransform().GetPosition());
		Vector3	vDistance = vLockOnTargetPosition - vPedPosition;
		float fDistanceSq = vDistance.Mag2();

		const float fLockOnRange = GetWeaponLockOnRange(pWeaponInfo, m_pLockOnTarget);
		if(fDistanceSq > rage::square(fLockOnRange))
		{
			return true;
		}

		if(!pWeaponInfo->GetIsMelee() || IsMeleeTargetingUsingNormalLock())
		{
			// Test if the aiming IK can reach the target position
			// PH: Do the IK test against the root bone, primarily because NM 
			// can drop the target position and break lock-on even though the reticule position is valid
			Vector3 vTargetPos = vLockOnTargetPosition;
			if(m_pLockOnTarget->GetIsTypePed())
			{
				const CPed* pPedTarget = static_cast<const CPed*>(m_pLockOnTarget.Get());
				s32 iBoneIndex = pPedTarget->GetBoneIndexFromBoneTag(BONETAG_ROOT);
				Matrix34 globalBoneMat;
				pPedTarget->GetGlobalMtx(iBoneIndex, globalBoneMat);
				vTargetPos = globalBoneMat.d;
			}

			Vector3 vDelta = vTargetPos - vPedPosition;
			vDelta.Normalize();

			// Calculate and limit the pitch
			float fPitch = rage::Atan2f(vDelta.z, vDelta.XYMag());
			fPitch = fwAngle::LimitRadianAngle(fPitch);

			if(fPitch > CTorsoIkSolver::ms_torsoInfo.maxPitch || fPitch < CTorsoIkSolver::ms_torsoInfo.minPitch)
			{
				// IK can't reach target
				return true;
			}
		}
	}

	return false;
}

 bool CPlayerPedTargeting::GetShouldBreakLockTargetIfNoLOS(s32 iFlags, float fBreakTime, u32 nTestTimeMS)
 {
 	pedFatalAssertf(m_pPed, "m_pPed is NULL");
 
 	if(m_pLockOnTarget)
 	{
 		if(CPedTargetEvaluator::GetNeedsLineOfSightToTarget(m_pLockOnTarget))
 		{ 
			if( (m_fLostLockLOSTimer > 0.0f) || ((fwTimer::GetTimeInMilliseconds() - nTestTimeMS) > m_uLastLockLOSTestTime) )
			{
				m_uLastLockLOSTestTime = fwTimer::GetTimeInMilliseconds();

				bool bLostLOS = CPedTargetEvaluator::GetIsLineOfSightBlocked(m_pPed, 
					m_pLockOnTarget, 
					false,																		//bIgnoreTargetsCover
					(iFlags & CPedTargetEvaluator::TS_IGNORE_LOW_OBJECTS)!=0,					//bAlsoIgnoreLowObjects
					(iFlags & CPedTargetEvaluator::TS_USE_MODIFIED_LOS_DISTANCE_IF_SET) != 0,	//bUseTargetableDistance
					(iFlags & CPedTargetEvaluator::TS_IGNORE_VEHICLES_IN_LOS) != 0,				//bIgnoreVehiclesForLOS
					(iFlags & CPedTargetEvaluator::TS_USE_COVER_VANTAGE_POINT_FOR_LOS)!=0,		//bUseCoverVantagePointForLOS
					(iFlags & CPedTargetEvaluator::TS_USE_MELEE_LOS_POSITION)!=0,				//bUseMeleePosForLOS
					false,																		//bTestPotentialPedTargets
					iFlags);

				if(bLostLOS)
				{
					m_fLostLockLOSTimer += fwTimer::GetTimeStep();
					if(m_fLostLockLOSTimer > fBreakTime)
						return true;
				}
				else
				{
					m_fLostLockLOSTimer = 0.0f;
				}
			}
 		}
 	}
 
 	return false;
 }


void CPlayerPedTargeting::UpdateFineAim(bool bSpringback)
{
	pedFatalAssertf(m_pPed, "m_pPed is NULL");

	if(m_pLockOnTarget)
	{
		float fTimeStep = fwTimer::GetTimeStep_NonScaledClipped();

		float fVerticalAimMovement = ms_Tunables.GetFineAimVerticalMovement();
		float fDownwardsVerticalAimMovement = ms_Tunables.GetFineAimDownwardsVerticalMovement();
		float fSidewaysScale = ms_Tunables.GetFineAimSidewaysScale();

		weaponAssert(m_pPed->GetWeaponManager());
		if(m_pPed->GetWeaponManager()->GetIsArmedMelee())
		{
			// Can't fine aim when using melee weapons
			ClearFineAim();
		}
		else
		{
			const CControl* pControl = m_pPed->GetControlFromPlayer();

			if(pControl && !IsTaskDisablingTargeting())
			{
				Vector3 vFineAimOffset(Vector3::ZeroType);
				bool bAttemptingFineAim = false;

				static dev_float MIN_FINE_AIM_VEC = 0.05f;

				if(!m_bPlayerIsFineAiming)
				{
					//! Clear any offset till we allow fine aiming.
					m_vFineAimOffset.Zero();
				}

				if(bSpringback)
				{
					vFineAimOffset.x   = pControl->GetPedAimWeaponLeftRight().GetUnboundNorm() * m_pPed->GetCapsuleInfo()->GetHalfWidth() * fSidewaysScale;
					vFineAimOffset.z = -1.0f * pControl->GetPedAimWeaponUpDown().GetUnboundNorm();

					if(vFineAimOffset.z > 0.0f)
					{
						vFineAimOffset.z *= fVerticalAimMovement;
					}
					else
					{
						vFineAimOffset.z *= fDownwardsVerticalAimMovement;
					}

					bAttemptingFineAim = vFineAimOffset.Mag2() > (rage::square(MIN_FINE_AIM_VEC));
				}
				else
				{
					Vector3 vLockOnPos = VEC3V_TO_VECTOR3(m_pLockOnTarget->GetTransform().GetPosition());
					Vector3 vPedPos = VEC3V_TO_VECTOR3(m_pPed->GetTransform().GetPosition());
					Vector3 vDistance = vLockOnPos - vPedPos;
					float fDist = Clamp(vDistance.Mag(), ms_Tunables.GetFineAimSpeedMultiplierCloseDistMin(), ms_Tunables.GetFineAimSpeedMultiplierCloseDistMax());
					float fSpeedWeight = (fDist - ms_Tunables.GetFineAimSpeedMultiplierCloseDistMin()) / (ms_Tunables.GetFineAimSpeedMultiplierCloseDistMax() - ms_Tunables.GetFineAimSpeedMultiplierCloseDistMin());
					fSpeedWeight = Clamp(fSpeedWeight, 0.0f, 1.0f);
					float fSpeedMultiplier = ms_Tunables.GetFineAimSpeedMultiplierClose() + ((1.0f - ms_Tunables.GetFineAimSpeedMultiplierClose()) * fSpeedWeight);
					fSpeedMultiplier *= ms_Tunables.GetFineAimSpeedMultiplier();
					fSpeedMultiplier *= fTimeStep;

					vFineAimOffset = m_vFineAimOffset;

					if(abs(pControl->GetPedAimWeaponLeftRight().GetUnboundNorm()) > 0.99f)
					{
						m_fFineAimHorizontalSpeedWeight += ms_Tunables.GetFineAimHorWeightSpeedMultiplier() * fTimeStep;
						m_fFineAimHorizontalSpeedWeight = Min(m_fFineAimHorizontalSpeedWeight, 1.0f);
					}
					else
					{
						m_fFineAimHorizontalSpeedWeight -= ms_Tunables.GetFineAimHorWeightSpeedMultiplier() * fTimeStep;
						m_fFineAimHorizontalSpeedWeight = Max(m_fFineAimHorizontalSpeedWeight, 0.0f);
					}

					float fFineAimHorizontalSpeedWeight = rage::Powf(m_fFineAimHorizontalSpeedWeight, ms_Tunables.GetFineAimHorSpeedPower());

					float fHorSpeed = ms_Tunables.GetFineAimHorSpeedMin() + ((ms_Tunables.GetFineAimHorSpeedMax() - ms_Tunables.GetFineAimHorSpeedMin()) * fFineAimHorizontalSpeedWeight);

					vFineAimOffset.x += pControl->GetPedAimWeaponLeftRight().GetUnboundNorm() * fHorSpeed * fSpeedMultiplier;
					vFineAimOffset.z += -1.0f * pControl->GetPedAimWeaponUpDown().GetUnboundNorm() * ms_Tunables.GetFineAimVerSpeed() * fSpeedMultiplier;

					//! Start attempting to fine aim immediately.
					m_bHasAttemptedNonSpringFineAim = true;
					bAttemptingFineAim = m_bHasAttemptedNonSpringFineAim;
				}

				if(!m_bPlayerIsFineAiming)
				{
					if(bAttemptingFineAim)
					{
						m_fTimeFineAiming += fTimeStep;
					}
					else
					{
						m_fTimeFineAiming = 0.0f;
					}

					float fUpDown = pControl->GetPedAimWeaponUpDown().GetUnboundNorm();

					static dev_float s_fReleaseThreshold = 0.1f;
					static dev_float s_fUpDownThreshold = 0.75f;

					if( (abs(fUpDown) < s_fReleaseThreshold))
					{
						m_bPlayerReleasedUpDownControlForFineAim = true;
					}

					if(m_bPlayerReleasedUpDownControlForFineAim && (abs(fUpDown) > s_fUpDownThreshold))
					{
						m_bPlayerPressingUpDownControlForFineAim = true;
					}

					float fMinTime = m_bPlayerPressingUpDownControlForFineAim ? ms_Tunables.GetMinFineAimTime() : ms_Tunables.GetMinFineAimTimeHoldingStick(); 
				
					const camGameplayDirector& gameplayDirector	= camInterface::GetGameplayDirector();
					bool bShowingReticule = gameplayDirector.ShouldDisplayReticule() || CNewHud::IsReticuleVisible();

					bool bAllowReticuleToEnableFineAiming = false;
					if(m_fTimeFineAiming > ms_Tunables.GetMinFineAimTime())
					{
						bAllowReticuleToEnableFineAiming = bShowingReticule;
					}

					if( (m_fTimeFineAiming > fMinTime) || bAllowReticuleToEnableFineAiming)
					{
						m_bPlayerIsFineAiming = true;
					}
				}
				else
				{
					if(bSpringback)
					{
						static dev_float LERP_AMOUNT = 0.5f;
						m_vFineAimOffset.Lerp(LERP_AMOUNT, vFineAimOffset);
					}
					else
					{
						m_vFineAimOffset = vFineAimOffset;
					}
					
					m_fTimeFineAiming += fTimeStep;
				}
			}
		}
	}
	else
	{
		// No lock-on target
		ClearFineAim();
	}
}

void CPlayerPedTargeting::ClearFineAim()
{
	m_bPlayerIsFineAiming = false;
	m_fTimeFineAiming = 0.0f;
	m_fFineAimHorizontalSpeedWeight = 0.0f;
	m_bHasAttemptedNonSpringFineAim = false;
	m_bPlayerReleasedUpDownControlForFineAim = false;
	m_bPlayerPressingUpDownControlForFineAim = false;
}

void CPlayerPedTargeting::ClearSoftAim()
{
	m_bPlayerHasStartedToAim = false;
	m_bPlayerHasBrokenLockOn = false;
	m_fTimeSoftLockedOn = 0.0f;
}

void CPlayerPedTargeting::ClearFreeAim()
{
	// Clear the pointers
	m_pFreeAimTarget = NULL;
	m_pFreeAimTargetRagdoll = NULL;
	m_pFreeAimAssistTarget = NULL;
	m_uStartTimeAimingAtFreeAimTarget = 0;
	m_bFreeAimTargetingVehicleForCamera = false;
	m_FreeAimProbeResults.Reset();
	//m_FreeAimAssistProbeResults.Reset();	// Should only be cleared in CPlayerPedTargeting::FindFreeAimAssistTarget
}	

void CPlayerPedTargeting::FindFreeAimTarget(const Vector3& vStart, const Vector3& vEnd, const CWeaponInfo& weaponInfo, Vector3& vHitPos)
{
	pedFatalAssertf(m_pPed, "m_pPed is NULL");

	bool bRejectFreeAimResults = false;
#if FPS_MODE_SUPPORTED
	if( m_pPed->IsFirstPersonShooterModeEnabledForPlayer(false) )
	{
		if( weaponInfo.GetIsArmed() )
		{
			if( m_pPed->GetPlayerInfo() && !m_pPed->GetPlayerInfo()->IsAiming() )
			{
				bRejectFreeAimResults = true;
			}
		}
		else
		{
			// In first person, cannot aim if unarmed.
			bRejectFreeAimResults = true;
		}
	}
#endif // FPS_MODE_SUPPORTED

	CVehicle *pVehicleTargeterInside = m_pPed->GetVehiclePedInside();
	bool bInLargeVehicle = pVehicleTargeterInside && (pVehicleTargeterInside->GetSeatManager()->GetMaxSeats() > 8);

	//static dev_bool sbForceSychronous = false;
	bool bUseSynchronousResults = false;
	if (/*sbForceSychronous ||*/ m_pPed->GetIsAttached())
	{
		// Probe for a target
		m_FreeAimProbeResults.Reset();
		WorldProbe::CShapeTestProbeDesc probeDesc;
		probeDesc.SetResultsStructure(&m_FreeAimProbeResults);
		probeDesc.SetStartAndEnd(vStart, vEnd);
		probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_RAGDOLL_TYPE | ArchetypeFlags::GTA_PED_TYPE | ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_ALL_MAP_TYPES | ArchetypeFlags::GTA_OBJECT_TYPE | ArchetypeFlags::GTA_GLASS_TYPE);
		probeDesc.SetExcludeEntity(m_pPed, bInLargeVehicle || m_pPed->IsInFirstPersonVehicleCamera() ? WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS : EXCLUDE_ENTITY_OPTIONS_NONE);
		WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_SYNCHRONOUS_TEST);
		bUseSynchronousResults = true;
	}

#if RSG_PC
	bool bStereoEnabled = GRCDEVICE.IsStereoEnabled() && GRCDEVICE.CanUseStereo();
	const camFrame& aimFrame = camInterface::GetPlayerControlCamAimFrame();
	Vector3 vStartCam = aimFrame.GetPosition();	
#endif
	if(m_FreeAimProbeResults.GetResultsReady() || bUseSynchronousResults)
	{
		CEntity* pFreeAimTarget = NULL;
		CEntity* pFreeAimTargetRagdoll = NULL;
		m_vClosestFreeAimTargetPos.Zero();
		Vector3 vClosestFreeAimTargetPos(Vector3::ZeroType);

		bool bFreeAimTargetingVehicleForCamera = false;
		if(m_FreeAimProbeResults.GetNumHits() > 0)
		{
			const CEntity* pHitEntity = m_FreeAimProbeResults[0].GetHitEntity();
			if(pHitEntity && pHitEntity->GetIsTypeVehicle())
			{
				bFreeAimTargetingVehicleForCamera = true;
			}
		}

#if RSG_PC
		if (bStereoEnabled)
		{
			m_vClosestFreeAimTargetPosStereo.Zero();
		}
#endif
		for(WorldProbe::ResultIterator i = m_FreeAimProbeResults.begin(); i < m_FreeAimProbeResults.last_result(); ++i)
		{					
			const bool bIsShootThrough = PGTAMATERIALMGR->GetMtlFlagShootThrough( i->GetHitMaterialId() ) || PGTAMATERIALMGR->GetIsShootThrough( i->GetHitMaterialId() );
			if (bIsShootThrough )
				continue;

			//Remember closest hit position
			Vector3 hitPos = i->GetHitPosition();
			if (vClosestFreeAimTargetPos.IsZero() || (hitPos - vStart).Mag2() < (vClosestFreeAimTargetPos-vStart).Mag2() )
				vClosestFreeAimTargetPos = hitPos;

#if RSG_PC
			if (bStereoEnabled)
			{
				if (m_vClosestFreeAimTargetPosStereo.IsZero() || (hitPos - vStartCam).Mag2() < (m_vClosestFreeAimTargetPosStereo-vStartCam).Mag2() )
				{
					m_vClosestFreeAimTargetPosStereo = hitPos;
					m_vFreeAimTargetDir = (m_vClosestFreeAimTargetPosStereo-vStartCam);
					m_fClosestFreeAimTargetDist = m_vFreeAimTargetDir.Mag();
					m_vFreeAimTargetDir.Normalize();
				}
			}
#endif

			CEntity* pEntityAimedAt = i->GetHitEntity();
			if(pEntityAimedAt)
			{
				if(pEntityAimedAt->GetIsTypePed())
				{
					CPed* pPedAimedAt = static_cast<CPed*>(pEntityAimedAt);

					//! Don't allow free aiming against anyone inside our own vehicle. Just skip.
					//! Note: in 1st person, we allow this so you can't (so obviously) shoot through friendly peds.
					if(!m_pPed->IsInFirstPersonVehicleCamera())
					{
						if(pVehicleTargeterInside && (pVehicleTargeterInside == pPedAimedAt->GetVehiclePedInside()))
						{
							continue;
						}
					}

					//! If we hit ragdoll, set both.
					if(pPedAimedAt->GetRagdollInst() == i->GetHitInst())
					{
						pFreeAimTarget = pPedAimedAt;
						pFreeAimTargetRagdoll = pPedAimedAt;
					}
					else
					{
						// Otherwise, go through list (from current point) and check if we hit ragdoll too. 
						WorldProbe::ResultIterator j = i;
						for(; j < m_FreeAimProbeResults.last_result(); ++j)
						{
							CEntity* pEntityAimedAtPostPed = j->GetHitEntity();
							if(pEntityAimedAtPostPed)
							{
								if(pEntityAimedAtPostPed->GetIsTypePed())
								{
									CPed *pPedAimedAtPostPed = static_cast<CPed*>(pEntityAimedAtPostPed);

									//! Note: if we have got this far it means that the capsule contact didn't actually hit
									//! ragdoll bounds. In that case, allow us to pick any ragdoll inst.
									if(pPedAimedAtPostPed->GetRagdollInst() == j->GetHitInst())
									{
										pFreeAimTargetRagdoll = pPedAimedAtPostPed;
										pPedAimedAt = pPedAimedAtPostPed;
										break;
									}
									else
									{
										//hit animated bounds, just keep going.
									}
								}
								else if(pEntityAimedAtPostPed->GetIsTypeVehicle())
								{
									if(pPedAimedAt->GetMyVehicle() != pEntityAimedAtPostPed)
									{
										break;
									}

									//If we hit vehicle aimed at ped is in, keep going to find ragdoll inst we are aiming at.
								}
								else
								{
									// Stop out if subsequent hit isn't the same entity
									break;
								}
							}
						}

						pFreeAimTarget = pPedAimedAt;
					}

					vHitPos = VEC3V_TO_VECTOR3(i->GetPosition());

					break;
				}
				else if(pEntityAimedAt->GetIsTypeVehicle())
				{
					CVehicle* pVehicleAimedAt = static_cast<CVehicle*>(pEntityAimedAt);

					//Note that the player aimed at the vehicle.
					if(m_pPed->GetVehiclePedInside() != pVehicleAimedAt)
					{
						pVehicleAimedAt->GetIntelligence()->PlayerAimedAtOrAround();
					}

					// Check if we subsequently hit a ped
					WorldProbe::ResultIterator j = i;
					j++;
					for(; j < m_FreeAimProbeResults.last_result(); ++j)
					{
						CEntity* pEntityAimedAtPostVehicle = j->GetHitEntity();
						if(pEntityAimedAtPostVehicle && pVehicleAimedAt != pEntityAimedAtPostVehicle)
						{
							if(pEntityAimedAtPostVehicle->GetIsTypePed() && static_cast<CPed*>(pEntityAimedAtPostVehicle)->GetVehiclePedInside() == pEntityAimedAt)
							{
								pFreeAimTarget = pEntityAimedAtPostVehicle;
								vHitPos = VEC3V_TO_VECTOR3(j->GetPosition());
								break;
							}
						}
					}

					if (!pFreeAimTarget)
					{
						//! By default, choose vehicle.
						pFreeAimTarget = pVehicleAimedAt;
						vHitPos = VEC3V_TO_VECTOR3(i->GetPosition());

						// See if we can override with a ped from inside vehicle.
						s32 iMaxSeats = pVehicleAimedAt->GetSeatManager()->GetMaxSeats();
						for(s32 iSeat = 0; iSeat < iMaxSeats; iSeat++)
						{
							CPed* pPedInSeat = pVehicleAimedAt->GetSeatManager()->GetPedInSeat(iSeat);
							if(pPedInSeat)
							{
								pFreeAimTarget = pPedInSeat;
								break;
							}
						}
					}	

					break;
				}
				else if (pEntityAimedAt->GetType() == ENTITY_TYPE_OBJECT || pEntityAimedAt->GetType() == ENTITY_TYPE_BUILDING)
				{
					pFreeAimTarget = pEntityAimedAt;
					break;
				}
				else
				{
					// Blocked
					break;
				}
			}
		}

		if(vClosestFreeAimTargetPos.Mag2() > 0.01f)
		{
			m_vClosestFreeAimTargetPos = vClosestFreeAimTargetPos;
		}
		else
		{
			m_vClosestFreeAimTargetPos = vEnd;
		}

		if(pFreeAimTarget && !bRejectFreeAimResults)
		{
			// We are aiming at something, but its not the thing we were aiming at last frame
			if(pFreeAimTarget != m_pFreeAimTarget)
			{
				m_pFreeAimTarget = pFreeAimTarget;
				m_uStartTimeAimingAtFreeAimTarget = fwTimer::GetTimeInMilliseconds();

				if(ShouldUsePullGunSpeech(&weaponInfo))
					m_pPed->NewSay("PULL_GUN");
			}

			// Is it a ragdoll under the reticle?
			m_pFreeAimTargetRagdoll = pFreeAimTargetRagdoll;
		}
		else
		{
			// Clear free aim target if none is found
			vHitPos.Zero();
			ClearFreeAim();
		}

		//NOTE: This is deliberately assigned after any potential call to ClearFreeAim() above.
		m_bFreeAimTargetingVehicleForCamera = bFreeAimTargetingVehicleForCamera;
	}

	if(!bUseSynchronousResults && !m_FreeAimProbeResults.GetWaitingOnResults())
	{
		m_FreeAimProbeResults.Reset();

		// Probe for a target
		WorldProbe::CShapeTestProbeDesc probeDesc;
		probeDesc.SetResultsStructure(&m_FreeAimProbeResults);
		probeDesc.SetStartAndEnd(vStart, vEnd);
		probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_RAGDOLL_TYPE | ArchetypeFlags::GTA_PED_TYPE | ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_ALL_MAP_TYPES | ArchetypeFlags::GTA_OBJECT_TYPE | ArchetypeFlags::GTA_GLASS_TYPE);
		probeDesc.SetExcludeEntity(m_pPed, bInLargeVehicle ? WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS : EXCLUDE_ENTITY_OPTIONS_NONE);
		WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_ASYNCHRONOUS_TEST);
	}
}

void CPlayerPedTargeting::FindFreeAimAssistTarget(const Vector3& vStartIn, const Vector3& vEndIn)
{
	if (!ms_Tunables.GetEnableBulletBending())
	{
		return;
	}

	pedFatalAssertf(m_pPed, "m_pPed is NULL");

	Vector3 vStart = vStartIn;
	Vector3 vEnd = vEndIn;

	if (!ms_Tunables.m_DoAynchronousProbesWhenFindingFreeAimAssistTarget)
	{
		ProcessFreeAimAssistTargetCapsuleTests(vStart, vEnd);
	}

	if(m_FreeAimAssistProbeResults.GetResultsReady())
	{
		CEntity* pClosestEntity = NULL;
		float fBestScore = FLT_MAX;
		Vector3 vIntersection;

#if DEBUG_DRAW
		u32 i = 0;
#endif // DEBUG_DRAW

		bool bNeedsTest = false;

		for(WorldProbe::ResultIterator it = m_FreeAimAssistProbeResults.begin(); it < m_FreeAimAssistProbeResults.last_result(); ++it)
		{
#if DEBUG_DRAW
			if(ms_Tunables.m_DisplayAimAssistTest)
			{
				char buff[64];
				formatf(buff, "DISPLAY_AIM_ASSIST_INTERSECTIONS_%d", i);
				CWeaponDebug::ms_debugStore.AddSphere(RCC_VEC3V(it->GetHitPosition()), 0.05f, Color_orange, 1000, atStringHash(buff));
			}
			++i;
#endif // DEBUG_DRAW

			bool bHitPed = false;

			CEntity* pEntity = it->GetHitEntity();
			if(pEntity)
			{
				if(pEntity->GetIsTypePed())
				{
					bHitPed = true;

					CPed* pPed = static_cast<CPed*>(pEntity);

					pedAssertf(m_pPed != pPed, "IsAHit = %s, hit inst = %p, ragdoll inst = %p, animated inst = %p", 
						it->IsAHit() ? "true" : "false", 
						it->GetHitInst(),
						pPed->GetRagdollInst(),
						pPed->GetAnimatedInst());

					if(GetIsPedValidForFreeAimAssist(pPed))
					{
						float fScore = GetScoreForFreeAimAssistIntersection(vStart, vEnd, it->GetHitPosition());
						if(fScore < fBestScore)
						{
							pClosestEntity = pEntity;
							fBestScore = fScore;
							vIntersection = it->GetHitPosition();
						}
					}
				}
				else
				{
					//! hit something that wasn't a ped 1st. Need to do a LOS check to determine if the
					if(!bHitPed)
					{
						bNeedsTest = true;
					}
				}
			}
		}

		if(bNeedsTest && pClosestEntity)
		{
			Vector3 vTestPos = vIntersection;
			if(pClosestEntity->GetIsTypePed())
			{
				CPed* pPed = static_cast<CPed*>(pClosestEntity);
				vTestPos.z = pPed->GetBonePositionCached(BONETAG_HEAD).z;
			}

			s32 iTargetOptions = 0;
			if(!CPedGeometryAnalyser::CanPedTargetPoint(*m_pPed, vTestPos, iTargetOptions))
			{
				pClosestEntity = NULL;
			}
		}

#if __BANK
		if(CPedTargetEvaluator::ms_Tunables.m_DebugTargetting && pClosestEntity)
		{
			CWeaponDebug::ms_debugStore.AddSphere(RCC_VEC3V(vIntersection), 0.05f, Color_purple, 1000, ATSTRINGHASH("FREE_AIM_ASSIST_TARGET_BEST_INTERSECTION", 0xB82FD8C0));
		}
#endif // __BANK

		m_vClosestFreeAimAssistTargetPos = vIntersection;

		// Attempt to get a more accurate target position (If using the new slow down code, still get the more accurate assist position)
		if(pClosestEntity && (GetMoreAccurateFreeAimAssistTargetPos(vStart, vEnd, vIntersection, pClosestEntity, m_vFreeAimAssistTargetPos) || ms_Tunables.GetUseNewSlowDownCode()))
		{
			m_pFreeAimAssistTarget = pClosestEntity;
		}
		else
		{
			// Clear the aim assist target
			m_pFreeAimAssistTarget = NULL;
		}
	}

	ProcessPedEventsForFreeAimAssist();

	if (ms_Tunables.m_DoAynchronousProbesWhenFindingFreeAimAssistTarget)
	{
		ProcessFreeAimAssistTargetCapsuleTests(vStart, vEnd);
	}

#if __BANK
	if(CPedTargetEvaluator::ms_Tunables.m_DebugTargetting && m_pFreeAimAssistTarget)
	{
		Vector3 vAssistTargetPosition = VEC3V_TO_VECTOR3(m_pFreeAimAssistTarget->GetTransform().GetPosition());
		vAssistTargetPosition.z += 1.0f;
		CWeaponDebug::ms_debugStore.AddSphere(RCC_VEC3V(vAssistTargetPosition), 0.05f, Color_cyan, 1000, ATSTRINGHASH("FREE_AIM_ASSIST_TARGET", 0xD1F6DCE1));
	}
#endif // __BANK
}

void CPlayerPedTargeting::ProcessFreeAimAssistTargetCapsuleTests(const Vector3& vStart, const Vector3& vEnd)
{
	if(!m_FreeAimAssistProbeResults.GetWaitingOnResults())
	{
		m_FreeAimAssistProbeResults.Reset();

		float fTestRadius = m_pPed->GetIsInVehicle() ? ms_Tunables.GetAimAssistCapsuleRadiusInVehicle() : ms_Tunables.GetAimAssistCapsuleRadius();

		WorldProbe::eBlockingMode blockingMode = ms_Tunables.m_DoAynchronousProbesWhenFindingFreeAimAssistTarget ? WorldProbe::PERFORM_ASYNCHRONOUS_TEST : WorldProbe::PERFORM_SYNCHRONOUS_TEST;
		// Perform a capsule check with ped's, this will give us the free aim target
		WorldProbe::CShapeTestCapsuleDesc pedCapsuleDesc;
		pedCapsuleDesc.SetResultsStructure(&m_FreeAimAssistProbeResults);
		pedCapsuleDesc.SetCapsule(vStart, vEnd, fTestRadius);

		s32 flags = ArchetypeFlags::GTA_RAGDOLL_TYPE | ArchetypeFlags::GTA_ALL_MAP_TYPES | ArchetypeFlags::GTA_OBJECT_TYPE;

		// 3/28/13 - cthomas - We want to skip vehicles, so don't include ArchetypeFlags::GTA_VEHICLE_TYPE!
		//! DMKH. but we do want to prevent sticky aiming to targets behind vehicles.
		if(ms_Tunables.GetUseFirstPersonStickyAim() || GetAreCNCFreeAimImprovementsEnabled(m_pPed))
		{
			flags |= ArchetypeFlags::GTA_VEHICLE_TYPE;
		}

		pedCapsuleDesc.SetIncludeFlags(flags);
		pedCapsuleDesc.SetExcludeEntity(m_pPed, WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS);
		pedCapsuleDesc.SetIsDirected(true);		
		WorldProbe::GetShapeTestManager()->SubmitTest(pedCapsuleDesc, blockingMode);

#if DEBUG_DRAW
		if(ms_Tunables.m_DisplayAimAssistTest)
		{
			float fTestRadius = m_pPed->GetIsInVehicle() ? ms_Tunables.GetAimAssistCapsuleRadiusInVehicle() : ms_Tunables.GetAimAssistCapsuleRadius();
			CWeaponDebug::ms_debugStore.AddCapsule(RCC_VEC3V(vStart), RCC_VEC3V(vEnd), fTestRadius, Color_orange, 1000,  ATSTRINGHASH("FreeAimAssistCapsule", 0xAAE041A3), false);
		}
#endif // DEBUG_DRAW
	}
}

bool CPlayerPedTargeting::GetIsAiming() const
{
	return GetLockOnTarget() || m_pPed->GetPlayerInfo()->GetPlayerDataFreeAiming() || m_pPed->GetPlayerInfo()->GetPlayerResetFlags().IsFlagSet(CPlayerResetFlags::PRF_ASSISTED_AIMING_ON);
}

bool CPlayerPedTargeting::IsPushingStick(const float fThreshHold) const
{
	const CControl* pControl = m_pPed->GetControlFromPlayer();
	if (pControl)
	{
		Vector2 vecStickInput;
		vecStickInput.x = pControl->GetPedAimWeaponLeftRight().GetUnboundNorm();
		vecStickInput.y = pControl->GetPedAimWeaponUpDown().GetUnboundNorm();

		if (Abs(vecStickInput.x) > fThreshHold|| Abs(vecStickInput.y) > fThreshHold)
		{
			return true;
		}
	}
	return false;
}

bool CPlayerPedTargeting::IsFineAimWithinBreakLimits() const
{
	Vector3 vFineAim;
	GetFineAimingOffset(vFineAim);

	if (Abs(vFineAim.x) < ms_Tunables.GetSoftLockFineAimBreakXYValue() &&
		Abs(vFineAim.y) < ms_Tunables.GetSoftLockFineAimBreakXYValue() &&
		Abs(vFineAim.z) < ms_Tunables.GetSoftLockFineAimBreakZValue())
	{
		return true;
	}
	return false;
}

bool CPlayerPedTargeting::IsFineAimWithinBreakXYLimits() const
{
	Vector3 vFineAim;
	GetFineAimingOffset(vFineAim);

	if (Abs(vFineAim.x) < ms_Tunables.GetSoftLockFineAimBreakXYValue() &&
		Abs(vFineAim.y) < ms_Tunables.GetSoftLockFineAimBreakXYValue())
	{
		return true;
	}
	return false;
}

bool CPlayerPedTargeting::IsFineAimWithinAbsoluteXYLimits(float fLimit) const
{
	Vector3 vFineAim;
	GetFineAimingOffset(vFineAim);

	if (Abs(vFineAim.x) < fLimit &&
		Abs(vFineAim.y) < fLimit)
	{
		return true;
	}
	return false;
}

bool  CPlayerPedTargeting::HasDeadLockonTarget() const
{
	return m_pLockOnTarget && m_pLockOnTarget->GetIsTypePed() && static_cast<CPed*>(m_pLockOnTarget.Get())->IsDead(); 
}


bool CPlayerPedTargeting::GetIsPedValidForFreeAimAssist(const CPed* pTargetPed) const
{
	pedFatalAssertf(m_pPed, "m_pPed is NULL");
	pedFatalAssertf(pTargetPed, "pTargetPed is NULL");

	static bank_bool IGNORE_DEAD_OR_DYING = false;
	if(pTargetPed->GetIsDeadOrDying() && !IGNORE_DEAD_OR_DYING)
	{
		// Ped is dead/dying = ignore
		return false;
	}

	static bank_bool IGNORE_THREATENED_BY = true;
	if(!pTargetPed->GetPedIntelligence()->IsThreatenedBy(*m_pPed) && !IGNORE_THREATENED_BY)
	{
		// Ped is not a threat
		return false;
	}

	return true;
}

float CPlayerPedTargeting::GetScoreForFreeAimAssistIntersection(const Vector3& vStart, const Vector3& vEnd, const Vector3& vHitPos) const
{
	// Construct a line representing the aiming vector
	fwGeom::fwLine line(vStart, vEnd);

	// Get the closest point to the intersection along the aiming vector
	Vector3 vClosestPoint;
	line.FindClosestPointOnLine(vHitPos, vClosestPoint);

	// Do a distance check to the aiming vector
	float fDistSq = vHitPos.Dist2(vClosestPoint);
	return fDistSq;
}

bool CPlayerPedTargeting::GetMoreAccurateFreeAimAssistTargetPos(const Vector3& vStart, const Vector3& vEnd, const Vector3& vHitPos, const CEntity* pHitEntity, Vector3& vNewHitPos) const
{
	pedFatalAssertf(m_pPed, "m_pPed is NULL");

	//
	// Attempt to find a more accurate target position
	// Construct a horizontal probe from the nearest point on the aiming vector in the direction of the hit position
	//

	fwGeom::fwLine line(vStart, vEnd);

	Vector3 vClosestPoint;

	// SNAPSHOT special designer test
#if __BANK
	if( ms_bEnableSnapShotBulletBend && pHitEntity && pHitEntity->GetIsTypePed() )
	{
		const CPlayerSpecialAbility* pSpecialAbility = m_pPed->GetSpecialAbility();
		if( pSpecialAbility && pSpecialAbility->GetType() == SAT_SNAPSHOT && pSpecialAbility->IsActive() )
		{
			// Determine if the target head position is within snapshot radius
			Vector3 vHeadPos = ((CPed*)pHitEntity)->GetBonePositionCached(BONETAG_HEAD);
			float fLineDist = line.FindClosestPointOnLine(vHeadPos, vClosestPoint);
			float fHeadDist = (vHeadPos - vClosestPoint).Mag();
			if( fHeadDist < Lerp( fLineDist, ms_fSnapShotRadiusMin, ms_fSnapShotRadiusMax ) )
			{
				vNewHitPos = vHeadPos;
				return true;
			}
		}
	}
#endif

	float fT = line.FindClosestPointOnLine(vHitPos, vClosestPoint);
	float fRadiusAtDistance = GetFreeAimRadiusAtADistance( fT );
	if( fRadiusAtDistance > 0.0f )
	{
		// Get the horizontal points
		Vector3 vAimDir(vEnd - vStart);
		vAimDir.NormalizeFast();
		Vector3 vRight;
		vRight.Cross(vAimDir, ZAXIS);
		vRight.NormalizeFast();
		Vector3 vToIntersectionPos(vHitPos - vClosestPoint);
		vToIntersectionPos.NormalizeFast();

		Vector3 vAimAtPos;
		TUNE_GROUP_BOOL(PLAYER_TARGETING, USE_TO_INTERSECTION_VECTOR, true);
		if (USE_TO_INTERSECTION_VECTOR)
		{
			vAimAtPos = vClosestPoint + (vToIntersectionPos * fRadiusAtDistance);
		}
		else
		{
			// Perform a dot product to work out the direction the probe should go
			float fDot = vRight.Dot(vToIntersectionPos);
			if(fDot > 0.0f)
			{
				vAimAtPos = vClosestPoint + (vRight * fRadiusAtDistance);
			}
			else
			{
				vAimAtPos = vClosestPoint - (vRight * fRadiusAtDistance);
			}
		}

		// Line probe to determine if we should change the intended hit position
		static dev_float CAPSULE_RADIUS = 0.05f;
		WorldProbe::CShapeTestCapsuleDesc pedCapsuleDesc;
		WorldProbe::CShapeTestHitPoint result;
		WorldProbe::CShapeTestResults probeResult(result);
		pedCapsuleDesc.SetResultsStructure( &probeResult );
		pedCapsuleDesc.SetCapsule( vClosestPoint, vAimAtPos, CAPSULE_RADIUS );
		pedCapsuleDesc.SetIsDirected( true );
		pedCapsuleDesc.SetIncludeFlags( ArchetypeFlags::GTA_RAGDOLL_TYPE );
		pedCapsuleDesc.SetIncludeEntity( pHitEntity );
		pedCapsuleDesc.SetDomainForTest( WorldProbe::TEST_AGAINST_INDIVIDUAL_OBJECTS );
		pedCapsuleDesc.SetDoInitialSphereCheck( true );		

		const bool bHitRagdoll = WorldProbe::GetShapeTestManager()->SubmitTest( pedCapsuleDesc );

#if __BANK
		if(CPedTargetEvaluator::ms_Tunables.m_DebugTargetting)
		{
			//CWeaponDebug::ms_debugStore.AddCapsule(RCC_VEC3V(vClosestPoint), RCC_VEC3V(vAimAtPos), CAPSULE_RADIUS, bHitRagdoll ? Color_cyan : Color_red, 1000, ATSTRINGHASH("PLAYER_PED_TARGETING_HORZ_LINE", 0x6A664A90), false);
		}
#endif // __BANK

		if(bHitRagdoll)
		{
			vNewHitPos = probeResult[0].GetHitPosition();
			return true;
		}
	}

	// Getting here means we are outside the free aim assist influence and we should fire where we are pointing
	vNewHitPos = vEnd;
	return false;
}

// Returns the bullet bending radius given a distance
float CPlayerPedTargeting::GetFreeAimRadiusAtADistance( float fDistToTarget ) const
{
	bool bZoomed = IsCameraInAccurateMode();

	weaponAssert(m_pPed->GetWeaponManager());
	const CWeapon* pWeapon = m_pPed->GetWeaponManager()->GetEquippedWeapon();
	if( pWeapon && pWeapon->GetWeaponInfo()->GetIsGunOrCanBeFiredLikeGun() )
	{
		float fNearRadius = 0.0f;
		float fFarRadius = 0.0f;
		GetBulletBendingData(m_pPed, pWeapon, bZoomed, fNearRadius, fFarRadius);

		return fNearRadius + fDistToTarget * (fFarRadius - fNearRadius);
	}

	return 0.0f;
}

const CWeaponInfo* CPlayerPedTargeting::GetWeaponInfo() const
{
	pedFatalAssertf(m_pPed, "m_pPed is NULL");
	return GetWeaponInfo(m_pPed);
}

const CWeaponInfo* CPlayerPedTargeting::GetWeaponInfo(CPed *pPed)
{
	pedFatalAssertf(pPed, "pPed is NULL");

	CPedWeaponManager *pWeapMgr = pPed->GetWeaponManager();
	if (pWeapMgr)
	{
		if(pWeapMgr->GetEquippedVehicleWeapon())
		{
			return pWeapMgr->GetEquippedVehicleWeapon()->GetWeaponInfo();
		}
		else
		{
			if(pWeapMgr->GetEquippedWeaponInfo() == pWeapMgr->GetSelectedWeaponInfo())
			{
				return pWeapMgr->GetEquippedWeaponInfo();
			}
		}
	}

	return NULL;
}

atHashWithStringNotFinal CPlayerPedTargeting::GetTargetSequenceGroup() const
{
	const CWeaponInfo* pInfo = GetWeaponInfo();
	if (pInfo)
	{
		return pInfo->GetTargetSequenceGroup();
	}
	return (u32)0;
}

CCurveSet* CPlayerPedTargeting::GetCurveSet(s32 iCurveType)
{
	bool bFirstPerson = FPS_MODE_SUPPORTED_ONLY(CGameWorld::FindLocalPlayer() ? CGameWorld::FindLocalPlayer()->IsFirstPersonShooterModeEnabledForPlayer(false) :) false;
	if (aiVerifyf(iCurveType >= 0 && iCurveType < CTargettingDifficultyInfo::CS_Max, "Invalid curve type %i", iCurveType))
	{
		switch(CPedTargetEvaluator::GetTargetMode(CGameWorld::FindLocalPlayer()))
		{
		case(CPedTargetEvaluator::TARGETING_OPTION_GTA_TRADITIONAL):
			return bFirstPerson ? &ms_Tunables.m_FirstPersonEasyTargettingDifficultyInfo.m_CurveSets[iCurveType] : &ms_Tunables.m_EasyTargettingDifficultyInfo.m_CurveSets[iCurveType];
		case(CPedTargetEvaluator::TARGETING_OPTION_ASSISTED_AIM):
			return bFirstPerson ? &ms_Tunables.m_FirstPersonNormalTargettingDifficultyInfo.m_CurveSets[iCurveType] : &ms_Tunables.m_NormalTargettingDifficultyInfo.m_CurveSets[iCurveType];
		case(CPedTargetEvaluator::TARGETING_OPTION_ASSISTED_FREEAIM):
			return bFirstPerson ? &ms_Tunables.m_FirstPersonHardTargettingDifficultyInfo.m_CurveSets[iCurveType] : &ms_Tunables.m_HardTargettingDifficultyInfo.m_CurveSets[iCurveType];
		case(CPedTargetEvaluator::TARGETING_OPTION_FREEAIM):
			return bFirstPerson ? &ms_Tunables.m_FirstPersonExpertTargettingDifficultyInfo.m_CurveSets[iCurveType] : &ms_Tunables.m_ExpertTargettingDifficultyInfo.m_CurveSets[iCurveType];
		}
	}

	Assert(0);
	return &ms_Tunables.m_EasyTargettingDifficultyInfo.m_CurveSets[iCurveType];
}

CCurveSet* CPlayerPedTargeting::GetAimDistanceScaleCurve()
{
	bool bFirstPerson = FPS_MODE_SUPPORTED_ONLY(CGameWorld::FindLocalPlayer() ? CGameWorld::FindLocalPlayer()->IsFirstPersonShooterModeEnabledForPlayer(false) :) false;
	
	switch(CPedTargetEvaluator::GetTargetMode(CGameWorld::FindLocalPlayer()))
	{
	case(CPedTargetEvaluator::TARGETING_OPTION_GTA_TRADITIONAL):
		return bFirstPerson ? &ms_Tunables.m_FirstPersonEasyTargettingDifficultyInfo.m_AimAssistDistanceCurve : &ms_Tunables.m_EasyTargettingDifficultyInfo.m_AimAssistDistanceCurve;
	case(CPedTargetEvaluator::TARGETING_OPTION_ASSISTED_AIM):
		return bFirstPerson ? &ms_Tunables.m_FirstPersonNormalTargettingDifficultyInfo.m_AimAssistDistanceCurve : &ms_Tunables.m_NormalTargettingDifficultyInfo.m_AimAssistDistanceCurve;
	case(CPedTargetEvaluator::TARGETING_OPTION_ASSISTED_FREEAIM):
		return bFirstPerson ? &ms_Tunables.m_FirstPersonHardTargettingDifficultyInfo.m_AimAssistDistanceCurve : &ms_Tunables.m_HardTargettingDifficultyInfo.m_AimAssistDistanceCurve;
	case(CPedTargetEvaluator::TARGETING_OPTION_FREEAIM):
		return bFirstPerson ? &ms_Tunables.m_FirstPersonExpertTargettingDifficultyInfo.m_AimAssistDistanceCurve : &ms_Tunables.m_ExpertTargettingDifficultyInfo.m_AimAssistDistanceCurve;
	}

	Assert(0);
	return &ms_Tunables.m_EasyTargettingDifficultyInfo.m_AimAssistDistanceCurve;
}

bool CPlayerPedTargeting::ShouldUsePullGunSpeech(const CWeaponInfo* pWeaponInfo)
{
	if(pWeaponInfo->GetDamageType() != DAMAGE_TYPE_BULLET && pWeaponInfo->GetDamageType() != DAMAGE_TYPE_BULLET_RUBBER)
		return false;

	CEntity* pTargetEntity = m_pLockOnTarget.Get();
	if(pTargetEntity == NULL && m_pFreeAimTarget != NULL)
	{
		pTargetEntity = m_pFreeAimTarget.Get();
	}

	if(!pTargetEntity || ! m_pPed || pTargetEntity->GetType() != ENTITY_TYPE_PED || static_cast<CPed*>(pTargetEntity)->HasAnimalAudioParams() ||
		!static_cast<CPed*>(pTargetEntity)->GetTaskData().GetIsFlagSet(CTaskFlags::CanBeTalkedTo) ||
		static_cast<CPed*>(pTargetEntity)->GetIsDeadOrDying())
	{
		return false;
	}

	//Take logic from determining lock-on target, and swap the target and the player, to make sure the target is looking at us
	Vector3 vTargetToPlayer = VEC3V_TO_VECTOR3(m_pPed->GetTransform().GetPosition() - pTargetEntity->GetTransform().GetPosition());
	if(vTargetToPlayer.Mag2() > 250.0f)
		return false;
	vTargetToPlayer.z = 0.0f;
	vTargetToPlayer.Normalize();
	Vector3	targetHeading = VEC3V_TO_VECTOR3(pTargetEntity->GetTransform().GetForward());
	targetHeading.z = 0.0f;
	targetHeading.Normalize();
	const float fDot = targetHeading.Dot(vTargetToPlayer);
	TUNE_GROUP_FLOAT(ONE_H_AIM, dotTolerance2, 0.85f, 0.0f, 1.0f, 0.01f);
	
	return fDot > dotTolerance2;
}

// Get weapon lock on range.
float CPlayerPedTargeting::GetWeaponLockOnRange(const CWeaponInfo* pWeaponInfo, CEntity *pEntity) const
{
	if(m_fLockOnRangeOverride >= 0.0f)
	{
		return m_fLockOnRangeOverride;
	}

	if(pWeaponInfo)
	{
		if(pWeaponInfo->GetIsUnarmed() && m_pPed->GetIsInCover())
		{
			return ms_Tunables.m_UnarmedInCoverTargetingDistance;
		}

		float fRejectionModifier = 1.0f;
		if(CPedTargetEvaluator::CanBeTargetedInRejectionDistance(m_pPed, pWeaponInfo, pEntity))
		{
			fRejectionModifier = ms_Tunables.GetLockOnDistanceRejectionModifier();
		}

		return pWeaponInfo->GetLockOnRange() * fRejectionModifier;
	}

	return 0.0f;
}

bool CPlayerPedTargeting::IsMeleeTargetingUsingNormalLock() const
{
	return m_pPed->GetIsInCover();
}

bool CPlayerPedTargeting::CanUseNoReticuleAimAssist() const
{
	bool bRunAndGun = m_pPed->GetPlayerInfo()->IsRunAndGunning();

#if FPS_MODE_SUPPORTED
	if( m_pPed->IsFirstPersonShooterModeEnabledForPlayer(false) )
	{
		return false;
	}
#endif

	if(!m_bNoReticuleAimAssist && m_pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsAimingGun))
	{
		return false;
	}

	const camGameplayDirector& gameplayDirector	= camInterface::GetGameplayDirector();
	if(!gameplayDirector.ShouldDisplayReticule() && 
		!m_bBlockNoReticuleAimAssist && 
		bRunAndGun && 
		(m_pLockOnTarget == NULL) && 
		(GetWeaponInfo() && GetWeaponInfo()->GetIsGun()) &&
		!NetworkInterface::IsGameInProgress())
	{
		return true;
	}

	return false;
}

bool CPlayerPedTargeting::IsCameraInAccurateMode()
{
	camThirdPersonAimCamera* pAimCam = camInterface::GetGameplayDirector().GetThirdPersonAimCamera();
	if(pAimCam)
	{
		return pAimCam->IsInAccurateMode();
	}
#if FPS_MODE_SUPPORTED
	else
	{
		camFirstPersonShooterCamera* pFirstPersonShooterCam = camInterface::GetGameplayDirector().GetFirstPersonShooterCamera();
		if(pFirstPersonShooterCam)
		{
			return pFirstPersonShooterCam->IsScopeAiming();
		}
	}
#endif

	return false;
}

float CPlayerPedTargeting::ComputeExtraZoomFactorForAccurateMode(const CWeaponInfo& weaponInfo)
{
	float extraZoomFactor = 1.0f;

	if(CGameWorld::FindLocalPlayer())
	{
		//Check if this weapon has a scope component in the attach ped's inventory.
		const CPed* attachPed			= CGameWorld::FindLocalPlayer();
		const CPedInventory* inventory	= attachPed->GetInventory();
		if(inventory)
		{
			const CWeaponItem* weaponItem = inventory->GetWeapon(weaponInfo.GetHash());
			if(weaponItem)
			{
				if(weaponItem->GetComponents())
				{
					const CWeaponItem::Components& components = *weaponItem->GetComponents();
					for(s32 i=0; i<components.GetCount(); i++)
					{
						const CWeaponComponentInfo* componentInfo = components[i].pComponentInfo;
						if(componentInfo && (componentInfo->GetClassId() == WCT_Scope))
						{
							extraZoomFactor	= static_cast<const CWeaponComponentScopeInfo*>(componentInfo)->
								GetExtraZoomFactorForAccurateMode();

							break;
						}
					}
				}
			}
		}
	}

	return extraZoomFactor;
}

void CPlayerPedTargeting::GetBulletBendingData(const CPed *pPed, const CWeapon* pWeapon, bool bZoomed, float &fNear, float &fFar)
{
	const bool bUsingCNCFreeAimImprovements = GetAreCNCFreeAimImprovementsEnabled(pPed, true);

	// CNC Free Aim: use vehicle velocity bullet bending modifiers.
	if(pPed->IsInFirstPersonVehicleCamera() || (bUsingCNCFreeAimImprovements && pPed->GetIsInVehicle()))
	{
		float fSpeed = 0.0f;
		CVehicle *pVehicle = pPed->GetVehiclePedInside();
		if(pVehicle)
		{
			fSpeed = pVehicle->GetVelocity().Mag();
		}

		float fSpeedScale = Min( fSpeed / ms_Tunables.GetInVehicleBulletBendingMaxVelocity(), 1.0f);

		fNear = Lerp(fSpeedScale, ms_Tunables.GetInVehicleBulletBendingNearMinVelocity(), ms_Tunables.GetInVehicleBulletBendingNearMaxVelocity());
		fFar = Lerp(fSpeedScale, ms_Tunables.GetInVehicleBulletBendingFarMinVelocity(), ms_Tunables.GetInVehicleBulletBendingFarMaxVelocity());
	}
	else if(pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
	{
		fNear = pWeapon->GetWeaponInfo()->GetFirstPersonBulletBendingNearRadius() * ms_Tunables.GetBulletBendingNearMultiplier();
		fFar = bZoomed ? pWeapon->GetWeaponInfo()->GetFirstPersonBulletBendingZoomedRadius() * ms_Tunables.GetBulletBendingZoomMultiplier() 
							: pWeapon->GetWeaponInfo()->GetFirstPersonBulletBendingFarRadius() * ms_Tunables.GetBulletBendingFarMultiplier();
	}
	else
	{
		fNear = pWeapon->GetWeaponInfo()->GetBulletBendingNearRadius() * ms_Tunables.GetBulletBendingNearMultiplier();
		fFar = bZoomed ? pWeapon->GetWeaponInfo()->GetBulletBendingZoomedRadius() * ms_Tunables.GetBulletBendingZoomMultiplier()  
						: pWeapon->GetWeaponInfo()->GetBulletBendingFarRadius() * ms_Tunables.GetBulletBendingFarMultiplier();
	}

	// CNC Free Aim: apply extra bullet bending modifier.
	// (Don't apply modifier if player/target are not moving fast enough)
	if (pPed && GetAreCNCFreeAimImprovementsEnabled(pPed))
	{
		const CPlayerInfo* pPlayerInfo = pPed->GetPlayerInfo();
		if (pPlayerInfo)
		{
			const CPlayerPedTargeting& rTargeting = pPlayerInfo->GetTargeting();
			const CEntity* pFreeAimAssistTargetEntity = rTargeting.GetFreeAimAssistTarget();
			if (pFreeAimAssistTargetEntity && pFreeAimAssistTargetEntity->GetIsTypePed())
			{
				TUNE_GROUP_FLOAT(PLAYER_TARGETING, fCNCFreeAimBulletBendingModifierMinVelocity, 0.075f, 0.0f, 50.0f, 0.01f);

				const CPed* pFreeAimAssistTargetPed = static_cast<const CPed*>(pFreeAimAssistTargetEntity);

				float fTargetVelocity = 0.0f;
				if (pFreeAimAssistTargetPed->GetIsInVehicle())
				{
					const CVehicle* pFreeAimAssistTargetVehicle = pFreeAimAssistTargetPed->GetVehiclePedInside();
					fTargetVelocity = pFreeAimAssistTargetVehicle->GetVelocity().Mag();
				}
				else
				{
					fTargetVelocity = Mag(Subtract(VECTOR3_TO_VEC3V(pFreeAimAssistTargetPed->GetVelocity()), pFreeAimAssistTargetPed->GetGroundVelocity())).Getf();
				}
					
				float fPlayerVelocity = 0.0f;
				if (pPed->GetIsInVehicle())
				{
					const CVehicle* pVehicle = pPed->GetVehiclePedInside();
					fPlayerVelocity = pVehicle->GetVelocity().Mag();
				}
				else
				{
					fPlayerVelocity = Mag(Subtract(VECTOR3_TO_VEC3V(pPed->GetVelocity()), pPed->GetGroundVelocity())).Getf();
				}

				const float fRelativeVelocity = abs(fTargetVelocity - fPlayerVelocity);

				if (fTargetVelocity > fCNCFreeAimBulletBendingModifierMinVelocity || 
					fPlayerVelocity > fCNCFreeAimBulletBendingModifierMinVelocity || 
					fRelativeVelocity > fCNCFreeAimBulletBendingModifierMinVelocity)
				{
					// Using a lower value for in vehicles as bullet bending radius is already larger and scaled by velocity in the above code.
					TUNE_GROUP_FLOAT(PLAYER_TARGETING, fCNCFreeAimExtraBulletBendingModifierOnFoot, 2.2f, 1.0f, 10.0f, 0.01f);
					TUNE_GROUP_FLOAT(PLAYER_TARGETING, fCNCFreeAimExtraBulletBendingModifierInVehicle, 1.65f, 1.0f, 10.0f, 0.01f);
					float fCNCFreeAimExtraBulletBendingModifier = pPed->GetIsInVehicle() ? fCNCFreeAimExtraBulletBendingModifierInVehicle : fCNCFreeAimExtraBulletBendingModifierOnFoot;

					// Cop/Crook-specific modifiers.
					if (pPlayerInfo->GetArcadeInformation().GetTeam() == eArcadeTeam::AT_CNC_COP)
					{
						TUNE_GROUP_FLOAT(PLAYER_TARGETING, fCNCFreeAimExtraBulletBendingModifierOnFoot_COP, 2.2f, 1.0f, 10.0f, 0.01f);
						TUNE_GROUP_FLOAT(PLAYER_TARGETING, fCNCFreeAimExtraBulletBendingModifierInVehicle_COP, 1.65f, 1.0f, 10.0f, 0.01f);
						fCNCFreeAimExtraBulletBendingModifier = pPed->GetIsInVehicle() ? fCNCFreeAimExtraBulletBendingModifierInVehicle_COP : fCNCFreeAimExtraBulletBendingModifierOnFoot_COP;
					}
					else if (pPlayerInfo->GetArcadeInformation().GetTeam() == eArcadeTeam::AT_CNC_CROOK)
					{
						TUNE_GROUP_FLOAT(PLAYER_TARGETING, fCNCFreeAimExtraBulletBendingModifierOnFoot_CROOK, 1.925f, 1.0f, 10.0f, 0.01f);
						TUNE_GROUP_FLOAT(PLAYER_TARGETING, fCNCFreeAimExtraBulletBendingModifierInVehicle_CROOK, 1.375f, 1.0f, 10.0f, 0.01f);
						fCNCFreeAimExtraBulletBendingModifier = pPed->GetIsInVehicle() ? fCNCFreeAimExtraBulletBendingModifierInVehicle_CROOK : fCNCFreeAimExtraBulletBendingModifierOnFoot_CROOK;
					}

					fNear *= fCNCFreeAimExtraBulletBendingModifier;
					fFar *= fCNCFreeAimExtraBulletBendingModifier;
				} 
			}
		}
	}
}
