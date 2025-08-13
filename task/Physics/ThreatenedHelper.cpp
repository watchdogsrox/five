// Filename   :	GrabHelper.cpp
// Description:	Helper class for natural motion tasks which need grab behaviour.

// --- Include Files ------------------------------------------------------------

// C headers
// Rage headers
#include "crskeleton\Skeleton.h"
#include "fragment\Cache.h"
#include "fragment\Instance.h"
#include "fragment\Type.h"
#include "fragment\TypeChild.h"
#include "fragmentnm\messageparams.h"
#include "pharticulated/articulatedcollider.h"
#include "physics/shapetest.h"

// Framework headers
#include "fwanimation/animmanager.h"
#include "fwanimation/pointcloud.h"
#include "grcore/debugdraw.h"
#include "fwmaths\Angle.h"

// Game headers
#include "Animation\FacialData.h"
#include "camera/CamInterface.h"
#include "Event\EventDamage.h"
#include "Network\NetworkInterface.h"
#include "Peds\Ped.h"
#include "Peds\PedIntelligence.h"
#include "Peds\PedPlacement.h"
#include "PedGroup\PedGroup.h"
#include "Physics\GtaInst.h"
#include "Physics\Physics.h"
#include "physics/WorldProbe/worldprobe.h"
#include "scene/world/GameWorld.h"
#include "Task\General\TaskBasic.h"
#include "Task\Movement\Jumping\TaskInAir.h"
#include "Task/Physics/ThreatenedHelper.h"
#include "vehicles/vehicle.h"
#include "Vfx\Misc\Fire.h"
#include "Weapons/Weapon.h"

AI_OPTIMISATIONS()

CThreatenedHelper::Parameters::Parameters()
{
	static const int DEFAULT_MIN_TIME_BEFORE_GUN_THREATEN = 100;
	static const int DEFAULT_MAX_TIME_BEFORE_GUN_THREATEN = 800;
	static const int DEFAULT_MIN_TIME_BEFORE_FIRE_GUN = 200;
	static const int DEFAULT_MAX_TIME_BEFORE_FIRE_GUN = 700;
	static const int DEFAULT_MAX_ADDITIONAL_TIME_BETWEEN_FIRE_GUN = 1000;

	m_nMinTimeBeforeGunThreaten = DEFAULT_MIN_TIME_BEFORE_GUN_THREATEN;
	m_nMaxTimeBeforeGunThreaten = DEFAULT_MAX_TIME_BEFORE_GUN_THREATEN;
	m_nMinTimeBeforeFireGun = DEFAULT_MIN_TIME_BEFORE_FIRE_GUN;
	m_nMaxTimeBeforeFireGun = DEFAULT_MAX_TIME_BEFORE_FIRE_GUN;
	m_nMaxAdditionalTimeBetweenFireGun = DEFAULT_MAX_ADDITIONAL_TIME_BETWEEN_FIRE_GUN;
}

CThreatenedHelper::CThreatenedHelper()
:	m_nStartTimeForGunThreaten(0),
m_nTimeTillFireGunWhileThreaten(0),
m_nTimeTillCancelPointAndShoot(0),
m_eChosenWeaponTargetBoneTag(BONETAG_HEAD),
m_eUseOfGun(THREATENED_GUN_NO_USE),
m_bStartedGunThreaten(false),
m_bBalanceFailed(false)
{
}


void CThreatenedHelper::BehaviourFailure(ARTFeedbackInterfaceGta* pFeedbackInterface)
{
	if(CTaskNMBehaviour::QueryNmFeedbackMessage(pFeedbackInterface, NM_BALANCE_FB))
	{
		m_bBalanceFailed = true;

		// stop pointing when we fall over, but let the ped point/shoot briefly while falling
		if (m_bStartedGunThreaten)
		{
			m_nTimeTillCancelPointAndShoot = fwTimer::GetTimeInMilliseconds() + 
				fwRandom::GetRandomNumberInRange(500, 1500);
		}
	}
}

void CThreatenedHelper::ControlBehaviour(CPed* pPed)
{
	// stop pointing after we fall over or PedCanThreaten() fails (weapon drops?)
	if (m_bStartedGunThreaten)
	{
		if (!PedCanThreaten(pPed) || 
			(m_bBalanceFailed && fwTimer::GetTimeInMilliseconds() > m_nTimeTillCancelPointAndShoot))
		{
			ART::MessageParams msg;
			msg.addBool(NMSTR_PARAM(NM_START), false);
			msg.addBool(NMSTR_PARAM(NM_POINT_ARM_USE_RIGHT_ARM), true);
			pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_POINT_ARM_MSG), &msg);

			// stop firing too
			m_eUseOfGun = THREATENED_GUN_NO_USE;

			m_bStartedGunThreaten = false;
		}
	}
}

bool CThreatenedHelper::PedCanThreaten(CPed* pPed)
{
	FastAssert(pPed != 0);

	CObject* weaponObj = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeaponObject() : NULL;
	if (weaponObj && weaponObj->GetWeapon()->GetWeaponInfo()->GetIsGun())
	{
		if (weaponObj->GetWeapon()->GetWeaponInfo()->GetIsGun1Handed())
			return true;

		if (weaponObj->GetWeapon()->GetWeaponInfo()->GetIsGun2Handed() &&
			pPed->GetNMTwoHandedWeaponBothHandsConstrained())
			return true;
	}

	return false;
}

void CThreatenedHelper::Start(eUseOfGun eUseOfGun, const CThreatenedHelper::Parameters& timingParams)
{
	FastAssert(eUseOfGun >= THREATENED_GUN_NO_USE && eUseOfGun <= THREATENED_GUN_THREATEN_AND_FIRE);

	m_Parameters = timingParams;

	// choose an approximate body location to shoot at if we get the choice;
	// the brace, for example, just fires at the driver's head
	eAnimBoneTag boneTagChoices[2] = {
		BONETAG_SPINE1,	
		BONETAG_HEAD,
	};
	m_eChosenWeaponTargetBoneTag = boneTagChoices[fwRandom::GetRandomNumberInRange(0, 2)];

	m_eUseOfGun = eUseOfGun;
	m_bStartedGunThreaten = false;

	m_nStartTimeForGunThreaten = fwTimer::GetTimeInMilliseconds() + 
		fwRandom::GetRandomNumberInRange(m_Parameters.m_nMinTimeBeforeGunThreaten, m_Parameters.m_nMaxTimeBeforeGunThreaten);

	m_nTimeTillFireGunWhileThreaten = fwTimer::GetTimeInMilliseconds()
		+ fwRandom::GetRandomNumberInRange(m_Parameters.m_nMinTimeBeforeFireGun, m_Parameters.m_nMaxTimeBeforeFireGun);

#if __DEV
	//Displayf("NM| [%08i] CThreatenedHelper::Start()\n", fwTimer::GetTimeInMilliseconds());
#endif // __DEV
}

void CThreatenedHelper::Update(CPed* pPed, const Vector3& target, const Vector3& worldTarget, int levelIndex)
{
	if (m_eUseOfGun != THREATENED_GUN_NO_USE)
	{
		CObject* weaponObj = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeaponObject() : NULL;

		if (!pPed->GetNMTwoHandedWeaponBothHandsConstrained())
		{
			if (!m_bStartedGunThreaten && fwTimer::GetTimeInMilliseconds() >= m_nStartTimeForGunThreaten)
			{
#if __DEV
				//Displayf("NM| [%08i] CThreatenedHelper::Update() - start threatening\n", fwTimer::GetTimeInMilliseconds());
#endif // __DEV

				ART::MessageParams msg;
				msg.addBool(NMSTR_PARAM(NM_START), true);

				// HD - if this ped is a gang member he can afford to be a bit more creative with his elbow twist
				//		to give themselves a bit of "gangsta flair"
				// TBD - stick these into TaskParams?
				if (pPed->IsGangPed())
				{
					// TODO RA: The unspecified arm version has been deprecated and replaced by "left" and "right" versions.
					// Just use the right arm for now (the old default behaviour).
					msg.addFloat(NMSTR_PARAM(NM_POINT_ARM_TWIST_RIGHT), fwRandom::GetRandomNumberInRange(-0.4f, 0.2f));
				}
				else
				{
					// otherwise we are a cop or well-armed civilian
					// TODO RA: The unspecified arm version has been deprecated and replaced by "left" and "right" versions.
					// Just use the right arm for now (the old default behaviour).
					msg.addFloat(NMSTR_PARAM(NM_POINT_ARM_TWIST_RIGHT), fwRandom::GetRandomNumberInRange(-1.00f, 1.00f));
				}
				// TODO RA: The unspecified arm version has been deprecated and replaced by "left" and "right" versions.
				// Just use the right arm for now (the old default behaviour).
				msg.addFloat(NMSTR_PARAM(NM_POINT_ARM_ARM_STRAIGHTNESS_RIGHT), fwRandom::GetRandomNumberInRange(0.75f, 0.95f));
				msg.addVector3(NMSTR_PARAM(NM_POINT_ARM_TARGET_RIGHT_VEC3), target.x, target.y, target.z);
				msg.addInt(NMSTR_PARAM(NM_POINT_ARM_INSTANCE_INDEX_RIGHT), levelIndex);

				msg.addFloat(NMSTR_PARAM(NM_POINT_ARM_POINT_SWING_LIMIT_RIGHT), 1.0f);
				msg.addBool(NMSTR_PARAM(NM_POINT_ARM_USE_ZERO_POSE_WHEN_NOT_POINTING_RIGHT), true);

				msg.addBool(NMSTR_PARAM(NM_POINT_ARM_USE_RIGHT_ARM), true);
				pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_POINT_ARM_MSG), &msg);

				m_bStartedGunThreaten = true;
			}
			else 
			{
				ART::MessageParams msg;

				// TODO RA: The unspecified arm version has been deprecated and replaced by "left" and "right" versions.
				// Just use the right arm for now (the old default behaviour).
				msg.addVector3(NMSTR_PARAM(NM_POINT_ARM_TARGET_RIGHT_VEC3), target.x, target.y, target.z);
				msg.addBool(NMSTR_PARAM(NM_POINT_ARM_USE_RIGHT_ARM), true);
				pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_POINT_ARM_MSG), &msg);
			}
		}

		if (m_eUseOfGun == THREATENED_GUN_THREATEN_AND_FIRE
			&& fwTimer::GetTimeInMilliseconds() >= m_nTimeTillFireGunWhileThreaten
			&& weaponObj)
		{
			if (!pPed->IsInjured())
			{
				CWeapon* pedWeapon = weaponObj->GetWeapon();

				if (pedWeapon->GetState() == CWeapon::STATE_READY)
				{
					// nabbed from the fire-weapon-when-it-hits-the-floor code
					Vector3 vecStart, vecEnd;
					pedWeapon->CalcFireVecFromGunOrientation(pPed, MAT34V_TO_MATRIX34(weaponObj->GetMatrix()), vecStart, vecEnd);

					Vector3 vnAlongWeapon, vnToTarget;
					vnAlongWeapon.Normalize(vecEnd - vecStart);
					vnToTarget.Normalize(worldTarget - vecStart);

					// is the gun pointing roughly at our target? otherwise we might be shooting ourselves in the foot / groin / etc
					static const float BRACE_AIM_ANGLE_DP_THRESHOLD = 0.8f;
					if (vnAlongWeapon.Dot(vnToTarget) > BRACE_AIM_ANGLE_DP_THRESHOLD)
						pedWeapon->Fire(CWeapon::sFireParams(pPed, MAT34V_TO_MATRIX34(weaponObj->GetMatrix()), &vecStart, &vecEnd));

					m_nTimeTillFireGunWhileThreaten = fwTimer::GetTimeInMilliseconds()
						+ static_cast<u32>(1000.0f * pedWeapon->GetWeaponInfo()->GetTimeBetweenShots())
						+ fwRandom::GetRandomNumberInRange(0, m_Parameters.m_nMaxAdditionalTimeBetweenFireGun);
					
					CFacialDataComponent* pFacialData = pPed->GetFacialData();
					if(pFacialData)
					{
						pFacialData->PlayFacialAnim(pPed, FACIAL_CLIP_ANGRY);
					}
				}
			}
		}
	}
}
