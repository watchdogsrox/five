//
// weapons/weaponcomponentflashlight.cpp
//
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved.
//

// File header
#include "Weapons/Components/WeaponComponentFlashLight.h"

// Game headers
#include "camera/CamInterface.h"
#include "camera/gameplay/aim/FirstPersonPedAimCamera.h"
#include "control/replay/Effects/ParticleWeaponFxPacket.h"
#include "game/Clock.h"
#include "Network/Objects/Entities/NetObjPlayer.h"
#include "Peds/Ped.h"
#include "peds/PedIntelligence.h"
#include "Renderer/Lights/Lights.h"
#include "System/ControlMgr.h"
#include "Task/Weapons/Gun/TaskAimGun.h"
#include "Task/Weapons/Gun/TaskGun.h"
#include "timecycle/TimeCycle.h"
#include "Vfx/Misc/Coronas.h"
#include "Weapons/Components/WeaponComponent.h"
#include "Weapons/Components/WeaponComponentScope.h"
#include "Weapons/Helpers/WeaponHelpers.h"

WEAPON_OPTIMISATIONS()

////////////////////////////////////////////////////////////////////////////////

CWeaponComponentFlashLightInfo::CWeaponComponentFlashLightInfo()
{
	m_texId = ATSTRINGHASH("torch", 0x008b461b5);
}

////////////////////////////////////////////////////////////////////////////////

CWeaponComponentFlashLightInfo::~CWeaponComponentFlashLightInfo()
{
}

////////////////////////////////////////////////////////////////////////////////

CWeaponComponentFlashLight::CWeaponComponentFlashLight()
: m_iFlashLightBoneIdx(-1)
, m_iFlashLightBoneBulbOnIdx(-1)
, m_iFlashLightBoneBulbOffIdx(-1)
, m_fInterpolateLightDirection(0.0f)
, m_OffTimer(0.0f)
, m_FPSOnTimer(150.0f)
, m_lod(3)
, m_bActive(false)
, m_bSetBulbState(true)
, m_bActiveHistory(false)
, m_bForceFlashLightOn(false)
, m_bProcessed(false)
{
}

////////////////////////////////////////////////////////////////////////////////

CWeaponComponentFlashLight::CWeaponComponentFlashLight(const CWeaponComponentFlashLightInfo* pInfo, CWeapon* pWeapon, CDynamicEntity* pDrawable)
: superclass(pInfo, pWeapon, pDrawable)
, m_iFlashLightBoneIdx(-1)
, m_iFlashLightBoneBulbOnIdx(-1)
, m_iFlashLightBoneBulbOffIdx(-1)
, m_fInterpolateLightDirection(0.0f)
, m_OffTimer(0.0f)
, m_FPSOnTimer(150.0f)
, m_lod(3)
, m_bActive(false)
, m_bSetBulbState(true)
, m_bActiveHistory(false)
, m_bForceFlashLightOn(false)
, m_bProcessed(false)
{
	if(GetDrawable())
	{
		m_iFlashLightBoneIdx = GetInfo()->GetFlashLightBone().GetBoneIndex(GetDrawable()->GetModelIndex());
		if(!weaponVerifyf(m_iFlashLightBoneIdx == -1 || m_iFlashLightBoneIdx < (s32)GetDrawable()->GetSkeleton()->GetBoneCount(), "Invalid m_iFlashLightBoneIdx bone index %d. Should be between 0 and %d", m_iFlashLightBoneIdx, GetDrawable()->GetSkeleton()->GetBoneCount()-1))
			m_iFlashLightBoneIdx = -1;
		m_iFlashLightBoneBulbOnIdx = GetInfo()->GetFlashLightBoneBulbOn().GetBoneIndex(GetDrawable()->GetModelIndex());
		if(!weaponVerifyf(m_iFlashLightBoneBulbOnIdx == -1 || m_iFlashLightBoneBulbOnIdx < (s32)GetDrawable()->GetSkeleton()->GetBoneCount(), "Invalid m_iFlashLightBoneBulbOnIdx bone index %d. Should be between 0 and %d", m_iFlashLightBoneBulbOnIdx, GetDrawable()->GetSkeleton()->GetBoneCount()-1))
			m_iFlashLightBoneBulbOnIdx = -1;
		m_iFlashLightBoneBulbOffIdx = GetInfo()->GetFlashLightBoneBulbOff().GetBoneIndex(GetDrawable()->GetModelIndex());
		if(!weaponVerifyf(m_iFlashLightBoneBulbOffIdx == -1 || m_iFlashLightBoneBulbOffIdx < (s32)GetDrawable()->GetSkeleton()->GetBoneCount(), "Invalid m_iFlashLightBoneBulbOffIdx bone index %d. Should be between 0 and %d", m_iFlashLightBoneBulbOffIdx, GetDrawable()->GetSkeleton()->GetBoneCount()-1))
			m_iFlashLightBoneBulbOffIdx = -1;
	}
}

////////////////////////////////////////////////////////////////////////////////

CWeaponComponentFlashLight::~CWeaponComponentFlashLight()
{
}

////////////////////////////////////////////////////////////////////////////////

void CWeaponComponentFlashLight::Process(CEntity* pFiringEntity)
{
	if(pFiringEntity && pFiringEntity->GetIsTypePed())
	{
		CPed* pPed = static_cast<CPed*>(pFiringEntity);
		if(!pPed->IsNetworkClone())
		{
			if(pPed->IsPlayer())
			{
				bool bToggleWhenAiming = GetInfo()->GetToggleWhenAiming();
				bool bSetActive = true;

				bool bInFPSMode = false;
#if FPS_MODE_SUPPORTED
				bInFPSMode = pPed->IsFirstPersonShooterModeEnabledForPlayer(false);
				if (bInFPSMode)
				{
					// B*2031397: Don't set flashlight active if in FPS idle/unholster state
					if (pPed->GetMotionData() && (pPed->GetMotionData()->GetIsFPSIdle() || pPed->GetMotionData()->GetIsFPSUnholster()))
					{
						bSetActive = false;
					}
				}
#endif	//FPS_MODE_SUPPORTED

				const CTaskGun* pTaskGun = pPed->GetPedIntelligence()->GetTaskGun();
				const CTaskMelee* pTaskMelee = pPed->GetPedIntelligence()->GetTaskMelee();
				if(pTaskGun && !pPed->GetIsInVehicle() && bSetActive)
				{
					//The light was previous on the last time the player was aiming, turn it on again.
					//Don't turn the light back on if we are only reloading it 
					if(m_bActiveHistory && !pTaskGun->GetIsReloading())
					{
						m_bActiveHistory = false;
						m_bActive = true; 
					}

					if (bToggleWhenAiming)
					{
						if (m_FPSOnTimer > 0.0f && bInFPSMode)
						{
							m_FPSOnTimer -= fwTimer::GetTimeStepInMilliseconds();
						}
						else
						{
							// Automatic Mode - Started aiming, automatically toggle light on 
							if (!m_bActive)
							{
								ToggleActive(pPed);
								m_OffTimer = 250.0f;
							}
						}
					}
					else
					{
						// Manual Mode - Toggle light based on d-pad input when aiming
						CControl* pControl = pPed->GetControlFromPlayer();
						if(pControl)
						{
							if(pControl->GetPedWeaponSpecialTwo().IsPressed() && !bToggleWhenAiming)
							{
								ToggleActive(pPed);
							}
						}

						if(m_bActive)
						{
							m_OffTimer = 500.0f;
						}
					}
				}
				else if(pTaskMelee && bToggleWhenAiming && m_bActive)
				{
					//We're in TASK_MELEE, but light is on so we must have been in TASK_GUN last frame. Leave the light on until TASK_MELEE is finished and immediately turn off.
					m_OffTimer = 0.0f;
				}
				else
				{
					if(m_OffTimer > 0.0f && !bInFPSMode)
					{
						m_OffTimer -= fwTimer::GetTimeStepInMilliseconds();
					}
					else
					{
						if (bToggleWhenAiming)
						{
							// Automatic Mode - Stopped aiming, automatically toggle light off (fully)
							if (m_bActive)
							{
								ToggleActive(pPed);
								m_FPSOnTimer = 150.0f;
							}
						}
						else
						{
							//Manual Mode - Stopped aiming, mark inactive and store active history
							if(m_bActive)
							{
								m_bActiveHistory = true;
							}
							m_bActive = false;
						}
					}
				}
			}
			else
			{
				//Turn the light off by default.
				m_bActive = false;
				
				//Disable for ai in day time or interiors
				if(m_iFlashLightBoneIdx != -1 && GetDrawable())
				{	
					if((IsNight() && !pPed->GetIsInInterior()) || pPed->GetPedResetFlag(CPED_RESET_FLAG_ForceEnableFlashLightForAI))
					{
						//Allow ai to have the flash light on any time, even when not aiming
						m_bActive = !pPed->GetPedResetFlag(CPED_RESET_FLAG_DisableFlashLight);
					}
				}
			}
		}

		// if this one is on, add it to the priority list
		if( m_bActive )
		{
			if( !m_bProcessed )
			{	
				if(!flashlightList.IsFull() && CouldBeVisible(pPed))
				{
					flashlightPriorityItem &item = flashlightList.Append();
					item.owner = pPed;
					item.pFlashlight = this;
					item.priority = 0;
					item.pos = pPed->GetTransform().GetPosition(); // only using rough position for priority sorting

					flashlightLookupList.Push((u8)(flashlightList.GetCount() - 1));
				}
				else
				{
					//No need to force this on
					m_lod = 3;
				}
			}
		}
	}
	else if (m_bForceFlashLightOn)
	{
		// There must have been no firing entity, but we want this flash light on anyway.
		m_bActive = true;
		
		if( !m_bProcessed )
		{
			if(!flashlightList.IsFull() && CouldBeVisible())
			{
				flashlightPriorityItem &item = flashlightList.Append();
				item.owner = NULL;
				item.pFlashlight = this;
				item.priority = 0;
				item.pos = GetWeapon() && GetWeapon()->GetEntity() ? GetWeapon()->GetEntity()->GetTransform().GetPosition() : Vec3V(0.0f,0.0f,0.0f); // only using rough position for priority sorting

				flashlightLookupList.Push((u8)(flashlightList.GetCount() - 1));
			}
			else
			{
				//Force it on 
				m_lod = 1;
			}
		}
	}

	// This is a reset flag
	m_bForceFlashLightOn = false;
	m_bProcessed = true;
}

////////////////////////////////////////////////////////////////////////////////

void CWeaponComponentFlashLight::ToggleActive(CPed* pPed)
{
	m_bActive = !m_bActive;
	m_bSetBulbState = true;
	g_WeaponAudioEntity.TriggerFlashlight(GetWeapon(),m_bActive);

	// Update enabled history for flash light
	if(pPed && pPed->GetInventory() && pPed->GetWeaponManager() && pPed->GetWeaponManager()->GetEquippedWeaponInfo())
	{
		CWeaponItem* pWeaponItem = pPed->GetInventory()->GetWeapon( pPed->GetWeaponManager()->GetEquippedWeaponInfo()->GetHash() );
		if(pWeaponItem)
		{
			if(pWeaponItem->GetComponents())
			{
				const CWeaponItem::Components& components = *pWeaponItem->GetComponents();
				for(s32 i = 0; i < components.GetCount(); i++)
				{
					if(components[i].pComponentInfo->GetClassId() == WCT_FlashLight)
					{
						pWeaponItem->SetActiveComponent(i, m_bActive);
					}
				}
			}
		}
	}

	if (pPed->GetNetworkObject() && pPed->IsLocalPlayer())
	{
		// Force an immediate weapon update so that other players don't miss that we toggled our flashlight
		static_cast<CNetObjPlayer*>(pPed->GetNetworkObject())->RequestForceSendOfGameState();
	} 
}

////////////////////////////////////////////////////////////////////////////////

bool CWeaponComponentFlashLight::CouldBeVisible(CPed *pFiringEntity)
{
	// Light direction can altered under special conditions when aim task in active (see ProcessPostPreRender() below).
	if(pFiringEntity->IsLocalPlayer() && GetAimTask(pFiringEntity))
		return true;

	Matrix34 lightMatrix(Matrix34::IdentityType);
	if(GetDrawable())
	{
		if(m_iFlashLightBoneIdx != -1)
		{
			GetDrawable()->GetGlobalMtx(m_iFlashLightBoneIdx, lightMatrix);
		}
	}
	else
	{
		const CEntity* pWeaponEntity = GetWeapon()->GetEntity();
		if(pWeaponEntity)
		{
			Mat34V m(pWeaponEntity->GetMatrix());
			lightMatrix = RC_MATRIX34(m);
		}
	}
	// Compute possible light visibility.
	return CouldBeVisible(lightMatrix);
}


bool CWeaponComponentFlashLight::CouldBeVisible()
{
	if(GetWeapon() && GetWeapon()->GetEntity())
	{
		// Get the light matrix from the weapon.
		const CEntity* pWeaponEntity = GetWeapon()->GetEntity();
		Matrix34 lightMatrix(Matrix34::IdentityType);
		Mat34V m(pWeaponEntity->GetMatrix());
		lightMatrix = RC_MATRIX34(m);

		// Compute possible light visibility.
		return CouldBeVisible(lightMatrix);
	}
	else
	{
		// Cannot compute without weapon. Assume it could be visible.
		return true;
	}
}


bool CWeaponComponentFlashLight::CouldBeVisible(Matrix34 &lightMatrix)
{
	Vector3 lightDir = lightMatrix.a;
	Vector3 lightFromCamera = lightMatrix.d - camInterface::GetPos();
	Vector3 camBackwardsDir = -camInterface::GetFront();

	float coneAngle = GetInfo()->GetMainLightOuterAngle();
	float lightRange = GetInfo()->GetMainLightRange();
	float distAlongCamera = Dot(lightFromCamera, camBackwardsDir);

	// Is the light behind the camera?
	if(distAlongCamera > 0.0f)
	{
		if(distAlongCamera > lightRange)
		{
			// The light is too far behind the camera to contribute.
			return false;
		}

		float dotAngle = Dot(lightDir, camBackwardsDir);
		float angleInDegrees = 180.0f*AcosfSafe(dotAngle)/PI;
		angleInDegrees += coneAngle;
		angleInDegrees = Min(angleInDegrees, 180.0f);

		// Could no point in front of the camera be inside the lights cone?
		if(angleInDegrees < 90.0f)
		{
			// The light will not contribute to the final image.
			return false;
		}

		// Is the light too far behind the camera ?
		if(distAlongCamera > -lightRange*Cosf(PI*angleInDegrees/180.0f))
		{
			// The light will not contribute to the final image.
			return false;
		}
	}
	return true;
}


////////////////////////////////////////////////////////////////////////////////

void CWeaponComponentFlashLight::ProcessPostPreRender(CEntity* pFiringEntity)
{
	m_bProcessed = false;
	if(GetWeapon())
	{
		if( GetDrawable() && GetWeapon()->GetWeaponHash() != ATSTRINGHASH("OBJECT", 0x39958261) )
		{
			Matrix34 zeroMatrix(Matrix34::ZeroType);
			if( m_bActive )
			{
				if(m_iFlashLightBoneBulbOnIdx != -1)
				{
					GetDrawable()->GetObjectMtxNonConst(m_iFlashLightBoneBulbOnIdx).Identity3x3();
				}

				if(m_iFlashLightBoneBulbOffIdx != -1)
				{
					GetDrawable()->GetObjectMtxNonConst(m_iFlashLightBoneBulbOffIdx).Zero3x3();
				}
			}
			else
			{
				if(m_iFlashLightBoneBulbOnIdx != -1)
				{
					GetDrawable()->GetObjectMtxNonConst(m_iFlashLightBoneBulbOnIdx).Zero3x3();
				}

				if(m_iFlashLightBoneBulbOffIdx != -1)
				{
					GetDrawable()->GetObjectMtxNonConst(m_iFlashLightBoneBulbOffIdx).Identity3x3();
				}
			}
		}

		if(m_bActive)
		{
			bool bAimingInFirstPersonScope = false;
			const camBaseCamera* pCamera = camInterface::GetDominantRenderedCamera();
			if(pCamera && pCamera->GetIsClass<camFirstPersonPedAimCamera>() && GetWeapon()->GetHasFirstPersonScope())
			{
				bAimingInFirstPersonScope = true;
			}

			const CEntity* pWeaponEntity = GetWeapon()->GetEntity();
			if( pWeaponEntity && (pWeaponEntity->GetRenderPhaseVisibilityMask().IsFlagSet(VIS_PHASE_MASK_GBUF) == false) && !bAimingInFirstPersonScope )
			{
				return;
			}

			Matrix34 lightMatrix(Matrix34::IdentityType);
			if(GetDrawable())
			{
				if(m_iFlashLightBoneIdx != -1)
				{
					GetDrawable()->GetGlobalMtx(m_iFlashLightBoneIdx, lightMatrix);
				}
			}
			else
			{
				if(pWeaponEntity)
				{
					Mat34V m(pWeaponEntity->GetMatrix());
					lightMatrix = RC_MATRIX34(m);
				}
			}

			// Override the camera direction in sniper scope mode. Fixed sniper reloads and flashlights. B* 2118730.
			if(bAimingInFirstPersonScope)
			{
				CPed* pPed = static_cast<CPed*>(pFiringEntity);
				if(pPed->IsLocalPlayer())
				{
					lightMatrix.a = pCamera->GetFrame().GetFront();
					lightMatrix.b = pCamera->GetFrame().GetRight();
					lightMatrix.c = pCamera->GetFrame().GetUp();
				}
			}

			fwInteriorLocation intLoc;
			if(GetDrawable())
			{
				GetDrawable()->GetPortalTracker()->Update(lightMatrix.d);
				intLoc = GetDrawable()->GetInteriorLocation();
			}

			const CWeaponComponentFlashLightInfo* info = GetInfo();
			
			Vector3 vLightDirection = lightMatrix.a;
			Vector3 vLightTan = lightMatrix.b;

			//Move the light direction to be centered on the aim camera
			if(pFiringEntity && pFiringEntity->GetIsTypePed() && static_cast<CPed*>(pFiringEntity)->IsLocalPlayer())
			{
				const CTaskAimGun* pAimGunTask = GetAimTask(static_cast<CPed*>(pFiringEntity));
				
				//Handle the weapon being blocked
				bool bBlocked = false;
				if(pAimGunTask && pAimGunTask->GetIsWeaponBlocked())
				{
					bBlocked = true;
					//Set the interpolate value so we can blend out of the blocked position back to aim.
					m_fInterpolateLightDirection = -1.0f;
				}

				if(pAimGunTask && pAimGunTask->GetIsReadyToFire() && !bBlocked)
				{
					const camFrame& aimFrame = camInterface::GetPlayerControlCamAimFrame();
					Vector3 vAdjustedLightDirection = aimFrame.GetPosition() + (aimFrame.GetFront() * info->GetTargetDistAlongAimCamera()) - lightMatrix.d;
					vAdjustedLightDirection.Normalize();

					//Currently we only need to interpolate when blind firing or coming out of blocked state
					if(pAimGunTask->GetTaskType() == CTaskTypes::TASK_AIM_GUN_BLIND_FIRE || m_fInterpolateLightDirection < 0.0f)
					{
						if(m_fInterpolateLightDirection < 1.0f)
						{
							static dev_float fInterpolaterate = 0.1f;
							m_fInterpolateLightDirection += fInterpolaterate;
						}

						//If the value is less than 0 then we are coming out of blocked state and want to interpolate between 0-1 when the value is -1-0
						float t = m_fInterpolateLightDirection;
						if(t < 0.0f)
							t += 1.0f;

						//Calculate the lerp
						vLightDirection = Lerp(t,vLightDirection,vAdjustedLightDirection);
						//additional normalize to avoid asserts
						vLightDirection.NormalizeSafe(lightMatrix.a);
					}
					else
					{
						m_fInterpolateLightDirection = 0.0f;

						// I'm leaving this in here commented out... but execting this line of code artifically changes the
						// direction these lights face.  That cases the shadow maps that contain shadows from the gun 
						// (for example, a silencer) to be wrong.  During standing still aiming, removing this makes no difference.
						// Anyway, keeping it in here so no one has to chase an old bug down forever if this was needed for
						// some reason, but removing it makes the lights physically correct, and thus the shadow maps correct
						// -jkinz
						//vLightDirection = vAdjustedLightDirection;
					}

					vLightTan.Cross(vLightDirection, lightMatrix.c);
					vLightTan.Normalize();
				}
				else
				{
					//Don't update the interpolate value when blocked, we have set this up so we interpolate back of blocked.
					if(!bBlocked)
					{
						m_fInterpolateLightDirection = 0.0f;
					}
				}
			}


			//==========================FLASHLIGHT LOD SETTINGS==========================================
			// 			||CORONA| MAIN----------------------------->	|SECONDARY------------------>	|
			//	Setting	|| ON	|  ON	|SHADOW	|VOLUME	| SPEC	|TEXTURE|  ON	|VOLUME	| SPEC	|TEXTURE|
			//	--------++------+-------+-------+-------+-------+-------+-------+-------+-------+-------+
			//	FULL	||	x	|	x	|	x	|	x	|	x	|	x	|	x	|		|	x	|	x	|
			//  HIGH	||	x	|	x	|	x	|	x	|		|	x	|	x	|		|		|	x	|
			//  MED		||	x	|	x	|		|		|		|		|		|		|		|		|
			//  LOW		||	x	|		|		|		|		|		|		|		|		|		|
			//===========================================================================================

			// Main light - top 3 tiers
			if( m_lod <= 2 )
			{
				float intensity = info->GetMainLightIntensity() * ( (float)GetWeapon()->GetEntity()->GetAlpha() / 255.0f );
				Vector3 col = Vector3(info->GetMainLightColor().GetRedf(),info->GetMainLightColor().GetGreenf(),info->GetMainLightColor().GetBluef());
				float range = info->GetMainLightRange();
				float falloffExp = info->GetMainLightFalloffExponent();
				float innerAngle = info->GetMainLightInnerAngle();
				float outerAngle = info->GetMainLightOuterAngle();
				float vIntensity = info->GetMainLightVolumeIntensity();
				float vSize = info->GetMainLightVolumeSize();
				float vExp = info->GetMainLightVolumeExponent();
				Vec4V vColor = info->GetMainLightVolumeOuterColor().GetRGBA();
				float shadowFade = info->GetMainLightShadowFadeDistance();
				float specularFade = info->GetMainLightSpecularFadeDistance();
				// using specularFade*2 because it kinda feels right, and there's no official fade distance in the info
				u8 lightFade = sm_LightFadeDist == (u8)0 ? static_cast<u8>(specularFade*2.0f) : sm_LightFadeDist;
				
				float fadeVal = 0.0f;
#if GTA_REPLAY
				if(CReplayMgr::IsEditModeActive())
				{
					const float fadeDist = 5.0f;
					const Vector3 vThisPosition = lightMatrix.d;
					const Vector3& camPos = camInterface::GetPos();
					const float dist = camPos.Dist(vThisPosition);
					fadeVal = rage::Clamp((dist - ((float)lightFade - fadeDist))/fadeDist,0.0f,1.0f);
				}
#endif // GTA_REPLAY

				CLightSource light(
					LIGHT_TYPE_SPOT, 
					LIGHTFLAG_FX 
					| (m_lod <= 1 ? LIGHTFLAG_CAST_SHADOWS | LIGHTFLAG_CAST_STATIC_GEOM_SHADOWS | LIGHTFLAG_CAST_DYNAMIC_GEOM_SHADOWS : 0)
					| LIGHTFLAG_MOVING_LIGHT_SOURCE
					| (m_lod <= 1 ? LIGHTFLAG_DRAW_VOLUME | LIGHTFLAG_USE_VOLUME_OUTER_COLOUR : 0)
					| (m_lod == 2 ? 0 : LIGHTFLAG_TEXTURE_PROJECTION)
					| (m_lod == 0 ? 0 : LIGHTFLAG_NO_SPECULAR),
					lightMatrix.d, 
					col, 
					intensity * (1.0f - fadeVal), 
					LIGHT_ALWAYS_ON);
				light.SetDirTangent(vLightDirection, vLightTan);
				light.SetInInterior(intLoc);
				light.SetShadowTrackingId(fwIdKeyGenerator::Get(this));
				light.SetTexture(info->GetTextureId(),CShaderLib::GetTxdId());
				
				light.SetRadius(range);
				light.SetFalloffExponent(falloffExp);
				light.SetSpotlight(innerAngle, outerAngle);
				
				light.SetLightVolumeIntensity(vIntensity, vSize, vColor, vExp);
				
				light.SetShadowFadeDistance(m_lod <= 1 ? (u8)shadowFade : (u8)0);
				light.SetSpecularFadeDistance((u8)specularFade);
				light.SetVolumetricFadeDistance(m_lod <= 1 ? (u8)specularFade : (u8)0);
				if( lightFade != 255 )
				{
					light.SetLightFadeDistance(lightFade); 
				}
				light.SetExtraFlags(EXTRA_LIGHTFLAG_CAUSTIC);

				Lights::AddSceneLight(light);
			}

			// Secondary light - Top 2 tiers
			if( m_lod <= 1 )
			{
				float intensity = info->GetSecondaryLightIntensity() * ( (float)GetWeapon()->GetEntity()->GetAlpha() / 255.0f );
				Vector3 col = Vector3(info->GetSecondaryLightColor().GetRedf(),info->GetSecondaryLightColor().GetGreenf(),info->GetSecondaryLightColor().GetBluef());
				float range = info->GetSecondaryLightRange();
				float falloffExp = info->GetSecondaryLightFalloffExponent();
				float innerAngle = info->GetSecondaryLightInnerAngle();
				float outerAngle = info->GetSecondaryLightOuterAngle();
				float vIntensity = info->GetSecondaryLightVolumeIntensity();
				float vSize = info->GetSecondaryLightVolumeSize();
				float vExp = info->GetSecondaryLightVolumeExponent();
				Vec4V vColor = info->GetSecondaryLightVolumeOuterColor().GetRGBA();
				float lightFade = info->GetSecondaryLightFadeDistance();
				
				float fadeVal = 0.0f;
#if GTA_REPLAY
				if(CReplayMgr::IsEditModeActive())
				{
					const float fadeDist = 2.0f;
					const Vector3 vThisPosition = lightMatrix.d;
					const Vector3& camPos = camInterface::GetPos();
					const float dist = camPos.Dist(vThisPosition);
					fadeVal = rage::Clamp((dist - (lightFade - fadeDist))/fadeDist,0.0f,1.0f);
				}
#endif // GTA_REPLAY

				CLightSource light(
					LIGHT_TYPE_SPOT, 
					LIGHTFLAG_FX 
					| LIGHTFLAG_TEXTURE_PROJECTION
					| (m_lod == 0 ? 0 : LIGHTFLAG_NO_SPECULAR),
					lightMatrix.d, 
					col, 
					intensity * (1.0f - fadeVal), 
					LIGHT_ALWAYS_ON);
				light.SetDirTangent(vLightDirection, vLightTan); 
				light.SetInInterior(intLoc);
				light.SetTexture(info->GetTextureId(),CShaderLib::GetTxdId());
				
				light.SetRadius(range);
				light.SetFalloffExponent(falloffExp);
				light.SetSpotlight(innerAngle, outerAngle);
				
				light.SetLightVolumeIntensity(vIntensity, vSize, vColor, vExp);
				
				light.SetLightFadeDistance((u8)lightFade);
				
				light.SetVolumetricFadeDistance(m_lod == 0 ? (u8)lightFade : (u8)0);

				Lights::AddSceneLight(light);
			}
			
			// Corona - all tiers
			{
				float sizeMult = 2.5f;
				float intensityMult = 1.0f;
								
				if (!intLoc.IsValid() BANK_ONLY(|| g_coronas.GetForceExterior()))
				{
					sizeMult       *= g_timeCycle.GetCurrUpdateKeyframe().GetVar(TCVAR_SPRITE_SIZE);
					intensityMult  *= g_timeCycle.GetCurrUpdateKeyframe().GetVar(TCVAR_SPRITE_BRIGHTNESS);
				}


				float intensity = info->GetMainLightCoronaIntensity() * intensityMult;
				float size = info->GetMainLightCoronaSize() * sizeMult;
				Color32 color = info->GetMainLightColor();

				g_coronas.Register(
					RCC_VEC3V(lightMatrix.d), 
					size,
					color,
					intensity,
					size * 0.5f, 
					RCC_VEC3V(vLightDirection),
					1.0f,
					info->GetSecondaryLightInnerAngle(),
					info->GetSecondaryLightOuterAngle(),
					CORONA_DONT_REFLECT | CORONA_HAS_DIRECTION);
			}
		}

#if GTA_REPLAY
		//Record if the flash light is active for replay playback
		if(CReplayMgr::ShouldRecord())
		{
			CReplayMgr::RecordFx<CPacketWeaponFlashLight>(CPacketWeaponFlashLight(m_bActive, m_lod), GetWeapon()->GetEntity());
		}
#endif
	}
}

////////////////////////////////////////////////////////////////////////////////

void CWeaponComponentFlashLight::TransferActiveness(CWeaponComponentFlashLight* pPrevFlashlightComp)
{
	if (!pPrevFlashlightComp)
	{
		return;
	}

	// Default is off, so for a new component, we only care if the previous component was actually on.
	if (pPrevFlashlightComp->m_bActive && !m_bActive)
	{
		m_bActive = true;

		// Copy across LOD if "higher".
		if (pPrevFlashlightComp->m_lod < m_lod)
		{
			m_lod = pPrevFlashlightComp->m_lod;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CWeaponComponentFlashLight::GetIsVisible() const
{
	return (GetActive() && IsNight());
}

////////////////////////////////////////////////////////////////////////////////

bool CWeaponComponentFlashLight::IsNight() const
{
	const int iEveningStart = 20;
	const int iMorningStart = 6;
	const int iHours = CClock::GetHour();

	return (iHours > iEveningStart || iHours < iMorningStart);
}

////////////////////////////////////////////////////////////////////////////////

u8 CWeaponComponentFlashLight::sm_LightFadeDist = 0;

atFixedArray<CWeaponComponentFlashLight::flashlightPriorityItem, 16> CWeaponComponentFlashLight::flashlightList;
atFixedArray<u8, 16> CWeaponComponentFlashLight::flashlightLookupList;

static int FlashlightDistSorter(const u8 *a, const u8 *b)
{
	Vec3V aPos = CWeaponComponentFlashLight::flashlightList[*a].pos;
	Vec3V bPos = CWeaponComponentFlashLight::flashlightList[*b].pos;
	Vec3V camPos = VECTOR3_TO_VEC3V(camInterface::GetPos());

	Vec3V camPosA = camPos - aPos;
	Vec3V camPosB = camPos - bPos;

	ScalarV distA = MagSquared(camPosA);
	ScalarV distB = MagSquared(camPosB);

	return IsGreaterThan(distA, distB).Getb() ? -1 : 1;
}

static int FlashlightPrioSorter(const u8 *a, const u8 *b)
{
	return CWeaponComponentFlashLight::flashlightList[*b].priority - CWeaponComponentFlashLight::flashlightList[*a].priority;
}

void CWeaponComponentFlashLight::CalcFlashlightPriorities()
{
	if(flashlightList.IsEmpty())
		return;

	Assert(flashlightLookupList.GetCount() == flashlightList.GetCount());

	// First, sort the list based on distance to camera
	flashlightLookupList.QSort(0,-1,FlashlightDistSorter);

	const int isThePlayer = 100;
	const int isAPlayer = 50;
	const int isMission = 8;
	const int onScreen = 1;

	// Give priority to certain flashlights
	int flashlightCount = flashlightLookupList.GetCount();
	for(int i=0;i<flashlightCount;i++)
	{
		CWeaponComponentFlashLight::flashlightPriorityItem& item = flashlightList[flashlightLookupList[i]];
		if (!item.owner)
			continue;

		int prio = i;

		CPed *pPed = item.owner.Get();

		if( pPed )
		{
			// always give the player full featured flash light
			if( pPed->IsPlayer() )
				prio += isThePlayer;
			if( pPed->IsAPlayerPed() )
				prio += isAPlayer;
			if( pPed->PopTypeIsMission() )
				prio += isMission;
			if( pPed->GetIsOnScreen() )
				prio += onScreen;
		}

		item.priority = prio;
		item.owner = NULL;
	}

	// Then, sort by priority
	flashlightLookupList.QSort(0,-1,FlashlightPrioSorter);

	// now set the LOD tiers
	const int FLASHLIGHT_HIGH	= 1; // main player?
	const int FLASHLIGHT_MEDIUM	= 5; // couple of pretty good ones (teammates?)
	const int FLASHLIGHT_LOW	= 8; // plain old spotlights
	for(int i = 0;i< flashlightCount;i++)
	{
		flashlightPriorityItem &item = flashlightList[flashlightLookupList[i]];
		if( i < FLASHLIGHT_HIGH && item.priority >= 100) // player only
		{
			item.pFlashlight->m_lod = 0;
		}
		else if( i < FLASHLIGHT_MEDIUM )
		{
			item.pFlashlight->m_lod = 1;
		}
		else if( i < FLASHLIGHT_LOW )
		{
			item.pFlashlight->m_lod = 2;
		}
		else
		{
			item.pFlashlight->m_lod = 3;
		}
	}

	flashlightList.Reset();
	flashlightLookupList.Reset();
}

void CWeaponComponentFlashLight::SetFlashlightFadeDistance(float distance)
{ 
	// values <= 0 will use default distance, values >= 255 will disable fade because fade is u8
	sm_LightFadeDist = static_cast<u8>(Clamp(distance, 0.0f, 255.0f) + 0.1f);
}
