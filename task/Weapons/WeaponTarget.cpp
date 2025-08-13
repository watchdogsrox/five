// File header
#include "Task/Weapons/WeaponTarget.h"

// Game headers
#include "Camera/gameplay/aim/FirstPersonShooterCamera.h"
#include "camera/CamInterface.h"
#include "game/Wanted.h"
#include "Network/Cloud/Tunables.h"
#include "network/Objects/Entities/NetObjPlayer.h"
#include "Peds/PedIntelligence.h"
#include "Peds/Ped.h"
#include "Task/Default/TaskPlayer.h"
#include "Weapons/Weapon.h"

// Macro to disable optimisations if set
AI_OPTIMISATIONS()
AI_WEAPON_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// CWeaponTarget
//////////////////////////////////////////////////////////////////////////
bool CWeaponTarget::ms_bAimAtHeadForSlowMovingVehicleToggle = true;

void CWeaponTarget::Update(CPed* pPed)
{
	if(weaponVerifyf(pPed, "NULL Ped pointer passed to CWeaponTarget::Update") &&
       weaponVerifyf(pPed->IsLocalPlayer(), "CWeaponTarget::Update() can only be called on the local player!"))
	{

		if (pPed->GetPlayerInfo()->AreControlsDisabled() && GetIsValid()
#if FPS_MODE_SUPPORTED
			// If FirstPersonShooter camera is active, always set the target when controls are disabled
			&& !(camInterface::GetDominantRenderedCamera() && camInterface::GetDominantRenderedCamera()->GetIsClass<camFirstPersonShooterCamera>())
#endif // FPS_MODE_SUPPORTED
			)
		{
			return; //don't touch it, use whatever the last target set was.
		}

		if( pPed->GetPlayerInfo()->GetTargeting().GetLockOnTarget() )
		{
			// Get the lock on target...
			CEntity* pTargetEntity = pPed->GetPlayerInfo()->GetTargeting().GetLockOnTarget();

			// Update the target entity
			SetEntity( pTargetEntity );

			// pass on the current target to the player network object for syncing
			if (GetEntity() && pPed->GetNetworkObject())
			{
				// possible we should be using GetPositionWithFiringOffsets() 
				// and passing that position into SetAimTarget
				// rather than just the entity 
				static_cast<CNetObjPlayer*>(pPed->GetNetworkObject())->SetAimTarget(*GetEntity());
			}
		}
		else
		{
			//Only perform one type of set below... (lavalley)
			bool bSetAimTarget = false;

			if( pPed->GetPlayerInfo()->GetTargeting().GetFreeAimTarget() && pPed->GetPlayerInfo()->GetTargeting().GetFreeAimTarget()->GetIsPhysical())
			{
				// Get the free aim target... - please retain - necessary for accessing the pTargetEntity - don't comment out (lavalley)
				CEntity* pTargetEntity = pPed->GetPlayerInfo()->GetTargeting().GetFreeAimTarget();

				// pass on the current target to the player network object for syncing
				if (pTargetEntity && pPed->GetNetworkObject())
				{
					// possible we should be using GetPositionWithFiringOffsets() 
					// and passing that position into SetAimTarget
					// rather than just the entity 
					static_cast<CNetObjPlayer*>(pPed->GetNetworkObject())->SetAimTarget(*pTargetEntity);
					bSetAimTarget = true;
				}
			}

			weaponAssert(pPed->GetWeaponManager());
			const CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
			const CObject* pWeaponObject = pPed->GetWeaponManager()->GetEquippedWeaponObject();
			if(pWeapon && (pWeaponObject || (pWeapon->GetWeaponInfo() && pWeapon->GetWeaponInfo()->GetIsMelee())))
			{
                const Matrix34* pmWeapon;
                if(pWeapon->GetWeaponInfo()->GetIsMelee() || pWeapon->GetWeaponInfo()->GetIsNotAWeapon())
                {
					pmWeapon = &RCC_MATRIX34(pPed->GetMatrixRef());
                }
                else
                {
					pmWeapon = &RCC_MATRIX34(pWeaponObject->GetMatrixRef());
                }

				Vector3 vStart(VEC3_ZERO);
				Vector3 vEnd(VEC3_ZERO);

				// Calculate the firing vector from the weapon camera (be that 1st or 3rd person)
				// If successful vEnd will be the firing vector from the weapon camera (be that 1st or 3rd person)
//				if(pWeapon->CalcFireVecForFreeAim(pPed, mWeapon, vStart, vEnd))
				pWeapon->CalcFireVecFromAimCamera(pPed, *pmWeapon, vStart, vEnd);
				{
					// Get rid of side portion of the aim vector as we look vertically so we don't swing round to the side
					// this fix up is only valid if the player ped is facing the same way as the camera!
					// if the IK can aim to the same up/down angles as the camera then we probably won't need/want this
					Vector3 vOffset = vEnd - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
					Vector3 vecPedRight(VEC3V_TO_VECTOR3(pPed->GetTransform().GetA()));
					float fSideOffset = vOffset.Dot(vecPedRight);
					float fSideRatio = vOffset.XYMag() / vOffset.Mag();
					vEnd -= fSideOffset * (1.0f - fSideRatio) * vecPedRight;

					// Update the target position
					SetPosition(vEnd);

					// pass on the current target to the player network object for syncing
					if (!bSetAimTarget && pPed->GetNetworkObject())
					{
						static_cast<CNetObjPlayer*>(pPed->GetNetworkObject())->SetAimTarget(vEnd);
					}
				}
			}
			else
			{
				// No target - clear it
				Clear();
			}
		}
	}
}

bool CWeaponTarget::GetPositionWithFiringOffsets(const CPed* pPed, Vector3& vTargetPosition) const
{
	if(weaponVerifyf(pPed, "NULL Ped pointer passed to CWeaponTarget::GetPositionWithFiringOffsets"))
	{
		// Consider shooting at the head if the target is a player in low cover. We do this
		// early now to avoid the GetPosition() call if possible.
		bool isLocalPlayer = pPed->IsLocalPlayer();
		const CEntity* pEntity = GetEntity();
		if(!isLocalPlayer && GetIsValid())
		{
			//Check if we are targeting a ped.
			if(pEntity && pEntity->GetIsTypePed())
			{
				//Check if we are targeting a player.
				const CPed* pTargetPed = static_cast<const CPed *>(pEntity);
				if(pTargetPed->IsPlayer())
				{
					bool bAimAtHead = false;

					//Check if the player is in low cover.
					if(pTargetPed->GetIsInCover() && pTargetPed->GetCoverPoint() &&
						(pTargetPed->GetCoverPoint()->GetHeight() == CCoverPoint::COVHEIGHT_LOW))
					{
						//Check if the player is close.
						ScalarV scDistSq = DistSquared(pPed->GetTransform().GetPosition(), pTargetPed->GetTransform().GetPosition());
						static float s_fMaxDistance = 3.0f;
						ScalarV scMaxDistSq = ScalarVFromF32(square(s_fMaxDistance));
						if(IsLessThanAll(scDistSq, scMaxDistSq))
						{
							bAimAtHead = true;
						}
					}

					if(!bAimAtHead)
					{
						if(ms_bAimAtHeadForSlowMovingVehicleToggle && CWanted::ShouldCompensateForSlowMovingVehicle(pPed, pTargetPed))
						{
							bAimAtHead = true;
						}
					}

					if(bAimAtHead)
					{
						vTargetPosition = VEC3V_TO_VECTOR3(pTargetPed->GetBonePositionCachedV(BONETAG_HEAD));
						return true;
					}
				}
			}
		}

		if(GetPosition(vTargetPosition))
		{
			if(isLocalPlayer)
			{
				// If we have a target entity
				if(pEntity && pEntity->GetIsTypePed())
				{
					Vector3 vFineAimOffset;
					if(pPed->GetPlayerInfo()->GetTargeting().GetFineAimingOffset(vFineAimOffset))
					{
						// Add a lock on fine aiming offset
						vTargetPosition += vFineAimOffset;
					}

					Vector3 vVelocityOffset;
					static_cast<const CPed*>(pEntity)->GetPlayerLockOnTargetVelocityOffset(vVelocityOffset, VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()));

					// Add a velocity offset
					vTargetPosition += vVelocityOffset;
				}
			}

			// Success
			return true;
		}
	}

	// Failed
	return false;
}

void CWeaponTarget::InitTunables()
{
	ms_bAimAtHeadForSlowMovingVehicleToggle = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("AIM_AT_HEAD_FOR_SLOW_MOVING_VEHICLE", 0x33540261), ms_bAimAtHeadForSlowMovingVehicleToggle);
}
