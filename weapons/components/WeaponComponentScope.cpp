//
// weapons/weaponcomponentscope.cpp
//
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved.
//

// File header
#include "Weapons/Components/WeaponComponentScope.h"

// Game headers
#include "camera/CamInterface.h"
#include "camera/system/CameraManager.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/gameplay/aim/FirstPersonAimCamera.h"
#include "camera/gameplay/aim/FirstPersonPedAimCamera.h"
#include "Peds/Ped.h"
#include "Renderer/Renderphases/RenderPhaseFX.h"
#include "Renderer/PostProcessFX.h"
#include "System/ControlMgr.h"
#include "audio/weaponaudioentity.h"

WEAPON_OPTIMISATIONS()

////////////////////////////////////////////////////////////////////////////////

CWeaponComponentScopeInfo::CWeaponComponentScopeInfo()
{
}

////////////////////////////////////////////////////////////////////////////////

CWeaponComponentScopeInfo::~CWeaponComponentScopeInfo()
{
}

////////////////////////////////////////////////////////////////////////////////

CWeaponComponentScope::CWeaponComponentScope()
: m_CameraHash(0)
, m_RecoilShakeAmplitude(1.0f)
, m_bActive(false)
, m_bActiveHistory(true)
, m_bFirstPersonScopeActive(false)
, m_bFirstPersonScopeActiveHistory(false)
, m_uScopeLastProcessedFrameCount(0)
, m_bNightVisionEnabledPreScope(false)
, m_bThermalVisionEnabledPreScope(false)
, m_bInitialisedSpecialScopeTint(false)
, m_pLocalPedOwner(NULL)
{
	CacheSeeThroughSettings();
	UpdateCamera();
}

////////////////////////////////////////////////////////////////////////////////

CWeaponComponentScope::CWeaponComponentScope(const CWeaponComponentScopeInfo* pInfo, CWeapon* pWeapon, CDynamicEntity* pDrawable)
: superclass(pInfo, pWeapon, pDrawable)
, m_CameraHash(0)
, m_RecoilShakeAmplitude(1.0f)
, m_bActive(false)
, m_bActiveHistory(true)
, m_bFirstPersonScopeActive(false)
, m_bFirstPersonScopeActiveHistory(false)
, m_uScopeLastProcessedFrameCount(0)
, m_bNightVisionEnabledPreScope(false)
, m_bThermalVisionEnabledPreScope(false)
, m_bInitialisedSpecialScopeTint(false)
, m_pLocalPedOwner(NULL)
{
	CacheSeeThroughSettings();
	UpdateCamera();
}

////////////////////////////////////////////////////////////////////////////////

CWeaponComponentScope::~CWeaponComponentScope()
{
	// B*3732776: Only clean up PostFX if this object belonged to a local player
	if (m_pLocalPedOwner)
	{
		// B*3918443: If the scope isn't active but some form of postfx is on screen, skip the cleanup (can accidently disable script postfx)
		bool bScriptPostFXActive = !m_bActive && (PostFX::GetUseNightVision() || RenderPhaseSeeThrough::GetState());
		if (!bScriptPostFXActive)
		{
			m_bActive = false;
			UpdatePostFX();
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CWeaponComponentScope::Process(CEntity* pFiringEntity)
{
	UpdateSpecialScope(pFiringEntity);
	UpdateCamera();

	// B*3648528: Only update m_bFirstPersonScopeActive flag once per frame. 
	// CWeaponComponentScope::Process can get called from multiple places at different points in the frame (ie from m_bForceWeaponStateToFire flag in CTaskAimGun::FireGun), 
	// meaning the scope can flick on/off (m_bFirstPersonScopeActive gets set from camera code).
	const u32 uCurrentFrameCount = fwTimer::GetFrameCount();
	if (uCurrentFrameCount != m_uScopeLastProcessedFrameCount)
	{
		// Reset every frame (set from camera)
		m_bFirstPersonScopeActive = false;
	}

	m_uScopeLastProcessedFrameCount = uCurrentFrameCount;
}

void CWeaponComponentScope::UpdateSpecialScope(CEntity* pFiringEntity)
{
	if (GetInfo()->GetIsSpecialScopeType())
	{
		bool bisActive = m_bActive;

		SetSpecialScopeTint();

		if(pFiringEntity && pFiringEntity->GetIsTypePed())
		{
			CPed* pPed = static_cast<CPed*>(pFiringEntity);
			if(pPed->IsLocalPlayer())
			{
				// Keep a record of the local player (for cleanup purposes)
				m_pLocalPedOwner = pPed;

				// Set from camera code earlier this frame
				if(m_bFirstPersonScopeActive)
				{
					if (!m_bFirstPersonScopeActiveHistory)
					{
						// We just entered first person scope this frame, so so cache off script postfx
						m_bNightVisionEnabledPreScope = PostFX::GetUseNightVision();
						m_bThermalVisionEnabledPreScope = RenderPhaseSeeThrough::GetState();
					}
					m_bFirstPersonScopeActiveHistory = true;

					// If we're not active but our history says we were active, then activate
					if (m_bActiveHistory && !bisActive)
					{
						bisActive = true;
					}

					// If a script effect matching the scope type was already active, then force activate and don't allow toggle
					if ( (GetInfo()->GetIsNightVisionScope() && m_bNightVisionEnabledPreScope) ||
						(GetInfo()->GetIsThermalVisionScope() && m_bThermalVisionEnabledPreScope) )
					{
						bisActive = true;
					}
					else
					{
						// Process control for active toggle
						CControl* pControl = pPed->GetControlFromPlayer();
						if(pControl)
						{
							if(pControl->GetPedWeaponSpecialTwo().IsPressed())
							{
								bisActive = !bisActive;
							}
						}
					}

					// If we're aiming, update our history
					m_bActiveHistory = bisActive;
				}
				else 
				{
					m_bFirstPersonScopeActiveHistory = false;

					// Scope active but no longer aiming in first person, so disable (but don't update history)
					if (bisActive)
					{
						bisActive = false;
					}
				}

				// If our active state changed this frame
				if (m_bActive != bisActive)
				{	
					m_bActive = bisActive;
					UpdateInventoryHistory(pPed);
					UpdatePostFX(); 
				}
			}
		}
	}
}

void CWeaponComponentScope::SetSpecialScopeTint()
{
	if (!m_bInitialisedSpecialScopeTint && GetInfo() && GetInfo()->GetIsThermalVisionScope())
	{
		m_bInitialisedSpecialScopeTint = true;
		static dev_u8 iThermalTintIdx = 1;
		UpdateShaderVariables(iThermalTintIdx);
	}
}

void CWeaponComponentScope::UpdateInventoryHistory(CPed* pPed)
{
	if(pPed && pPed->GetInventory() && GetWeapon())
	{
		CWeaponItem* pWeaponItem = pPed->GetInventory()->GetWeapon(GetWeapon()->GetWeaponHash());
		if(pWeaponItem && pWeaponItem->GetComponents())
		{
			const CWeaponItem::Components& components = *pWeaponItem->GetComponents();
			for(s32 i = 0; i < components.GetCount(); i++)
			{
				if(components[i].pComponentInfo->GetClassId() == WCT_Scope)
				{
					pWeaponItem->SetActiveComponent(i, m_bActiveHistory);
				}
			}
		}
	}
}

void CWeaponComponentScope::UpdatePostFX()
{
	bool bSkipSet = false;

	if (GetInfo()->GetIsNightVisionScope())
	{
		if (m_bActive) // TURNING ON
		{
			// If script thermal vision is on, turn off before we activate scope night vision
			if (m_bThermalVisionEnabledPreScope)
			{
				RenderPhaseSeeThrough::SetState(false);
			}
		}
		else // TURNING OFF
		{
			// If script thermal vision was on before scope night vision, restore it
			if (m_bThermalVisionEnabledPreScope)
			{
				RenderPhaseSeeThrough::SetState(true);
			}
			// If script night vision was on before scope night vision, leave active
			if (m_bNightVisionEnabledPreScope)
			{
				bSkipSet = true;
			}
		}

		if (!bSkipSet)
		{
			PostFX::SetUseNightVision(m_bActive);
		}

		g_WeaponAudioEntity.ToggleThermalVision(m_bActive);
	}
	else if (GetInfo()->GetIsThermalVisionScope())
	{
		if (m_bActive) // TURNING ON
		{
			// Before we enable scope thermal vision, cache off any script settings
			CacheSeeThroughSettings();

			// If script night vision is on, turn off before we activate scope thermal vision
			if (m_bNightVisionEnabledPreScope)
			{
				PostFX::SetUseNightVision(false);
			}
			
			// Reset to default
			RenderPhaseSeeThrough::UpdateVisualDataSettings();

			// Apply custom settings for scope
			RenderPhaseSeeThrough::SetMaxThickness(5.0f);
			
			
		}
		else // TURNING OFF
		{
			// Restore any script thermal vision settings we cached earlier
			RestoreSeeThroughSettings();

			// If script night vision was on before scope thermal vision, restore it
			if (m_bNightVisionEnabledPreScope)
			{
				PostFX::SetUseNightVision(true);
			}

			// If script thermal vision was on before scope thermal vision, leave active
			if (m_bThermalVisionEnabledPreScope)
			{
				bSkipSet = true;
			}
		}

		if (!bSkipSet)
		{
			RenderPhaseSeeThrough::SetState(m_bActive);
		}

		g_WeaponAudioEntity.ToggleThermalVision(m_bActive);
	}
}

void CWeaponComponentScope::CacheSeeThroughSettings()
{
	m_fCachedSeeThroughHeatScaleDead = RenderPhaseSeeThrough::GetHeatScale(TB_DEAD);
	m_fCachedSeeThroughHeatScaleCold = RenderPhaseSeeThrough::GetHeatScale(TB_COLD);
	m_fCachedSeeThroughHeatScaleWarm = RenderPhaseSeeThrough::GetHeatScale(TB_WARM);
	m_fCachedSeeThroughHeatScaleHot = RenderPhaseSeeThrough::GetHeatScale(TB_HOT);
	m_fCachedSeeThroughFadeStartDistance = RenderPhaseSeeThrough::GetFadeStartDistance();
	m_fCachedSeeThroughFadeEndDistance = RenderPhaseSeeThrough::GetFadeEndDistance();
	m_fCachedSeeThroughMaxThickness = RenderPhaseSeeThrough::GetMaxThickness();
	m_fCachedSeeThroughNoiseMin = RenderPhaseSeeThrough::GetMinNoiseAmount();
	m_fCachedSeeThroughNoiseMax = RenderPhaseSeeThrough::GetMaxNoiseAmount();
	m_fCachedSeeThroughHiLightIntensity = RenderPhaseSeeThrough::GetHiLightIntensity();
	m_fCachedSeeThroughHiLightNoise = RenderPhaseSeeThrough::GetHiLightNoise();
	m_fCachedSeeThroughColorNear = RenderPhaseSeeThrough::GetColorNear();
	m_fCachedSeeThroughColorFar = RenderPhaseSeeThrough::GetColorFar();
	m_fCachedSeeThroughColorVisibleBase = RenderPhaseSeeThrough::GetColorVisibleBase();
	m_fCachedSeeThroughColorVisibleWarm = RenderPhaseSeeThrough::GetColorVisibleWarm();
	m_fCachedSeeThroughColorVisibleHot = RenderPhaseSeeThrough::GetColorVisibleHot();
}

void CWeaponComponentScope::RestoreSeeThroughSettings()
{
	RenderPhaseSeeThrough::SetHeatScale(TB_DEAD, m_fCachedSeeThroughHeatScaleDead);
	RenderPhaseSeeThrough::SetHeatScale(TB_COLD, m_fCachedSeeThroughHeatScaleCold);
	RenderPhaseSeeThrough::SetHeatScale(TB_WARM, m_fCachedSeeThroughHeatScaleWarm);
	RenderPhaseSeeThrough::SetHeatScale(TB_HOT, m_fCachedSeeThroughHeatScaleHot);
	RenderPhaseSeeThrough::SetFadeStartDistance(m_fCachedSeeThroughFadeStartDistance);
	RenderPhaseSeeThrough::SetFadeEndDistance(m_fCachedSeeThroughFadeEndDistance);
	RenderPhaseSeeThrough::SetMaxThickness(m_fCachedSeeThroughMaxThickness);
	RenderPhaseSeeThrough::SetMinNoiseAmount(m_fCachedSeeThroughNoiseMin);
	RenderPhaseSeeThrough::SetMaxNoiseAmount(m_fCachedSeeThroughNoiseMax);
	RenderPhaseSeeThrough::SetHiLightIntensity(m_fCachedSeeThroughHiLightIntensity);
	RenderPhaseSeeThrough::SetHiLightNoise(m_fCachedSeeThroughHiLightNoise);
	RenderPhaseSeeThrough::SetColorNear(m_fCachedSeeThroughColorNear);
	RenderPhaseSeeThrough::SetColorFar(m_fCachedSeeThroughColorFar);
	RenderPhaseSeeThrough::SetColorVisibleBase(m_fCachedSeeThroughColorVisibleBase);
	RenderPhaseSeeThrough::SetColorVisibleWarm(m_fCachedSeeThroughColorVisibleWarm);
	RenderPhaseSeeThrough::SetColorVisibleHot(m_fCachedSeeThroughColorVisibleHot);
}

void CWeaponComponentScope::UpdateCamera()
{
	u32 componentCameraHash = GetInfo()->GetCameraHash();

	// If we always have a scope, always use the component camera, if valid
	if(componentCameraHash && GetWeapon()->GetHasFirstPersonScope())
	{
		m_CameraHash = componentCameraHash;
		m_RecoilShakeAmplitude = GetInfo()->GetRecoilShakeAmplitude();
	}
	else
	{
		m_CameraHash = GetWeapon()->GetWeaponInfo()->GetDefaultCameraHash();
		m_RecoilShakeAmplitude = GetWeapon()->GetWeaponInfo()->GetRecoilShakeAmplitude();
	}
}

////////////////////////////////////////////////////////////////////////////////
