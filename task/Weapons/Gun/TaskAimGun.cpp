// Class header
#include "Task/Weapons/Gun/TaskAimGun.h"

// Framework headers
#include "ai/task/taskchannel.h"
#include "grcore/debugdraw.h"

// Game headers
#include "Camera/CamInterface.h"
#include "Camera/Gameplay/GameplayDirector.h"
#include "Debug/DebugScene.h"
#include "ModelInfo/WeaponModelInfo.h"
#include "Network/Objects/Entities/NetObjPlayer.h"
#include "Peds/PedCapsule.h"
#include "Peds/PedIntelligence.h"
#include "Peds/Ped.h"
#include "Peds/PlayerSpecialAbility.h"
#include "Physics/physics.h"
#include "Peds/PedWeapons/PlayerPedTargeting.h"
#include "Task/Combat/Cover/TaskCover.h"
#include "Task/Default/TaskAmbient.h"
#include "Task/Default/TaskPlayer.h"
#include "Task/General/TaskBasic.h"
#include "Task/Motion/Locomotion/TaskLocomotion.h"
#include "Task/Motion/Locomotion/TaskMotionAiming.h"
#include "Task/Motion/Locomotion/TaskMotionPed.h"
#include "Task/Movement/TaskGotoPointAiming.h"
#include "Task/Weapons/Gun/GunFlags.h"
#include "Task/Weapons/TaskPlayerWeapon.h"
#include "Task/Weapons/TaskWeaponBlocked.h"
#include "Task/Weapons/Gun/TaskVehicleDriveBy.h"
#include "Task/Vehicle/TaskVehicleWeapon.h"
#include "Vfx/Systems/VfxWeapon.h"
#include "Weapons/inventory/PedInventoryLoadOut.h"
#include "Weapons/FiringPattern.h"
#include "weapons/projectiles/ProjectileManager.h"
#include "Weapons/Weapon.h"
#include "Vehicles/Metadata/VehicleSeatInfo.h"
#include "Vehicles/Metadata/VehicleMetadataManager.h"
#include "Vehicles/VehicleGadgets.h"
#include "camera/cinematic/camera/mounted/CinematicMountedCamera.h"

// Macro to disable optimisations if set
AI_OPTIMISATIONS()
AI_WEAPON_OPTIMISATIONS()
NETWORK_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// CTaskAimGun
//////////////////////////////////////////////////////////////////////////

bool CTaskAimGun::ms_bUseTorsoIk = true;
bool CTaskAimGun::ms_bUseLeftHandIk = true;
bool CTaskAimGun::ms_bUseIntersectionPos = false;

#if __BANK
bool CTaskAimGun::ms_bUseAltClipSetHash = false;
bool CTaskAimGun::ms_bUseOverridenYaw = false;
bool CTaskAimGun::ms_bUseOverridenPitch = false;
float CTaskAimGun::ms_fOverridenYaw = 0.0f;
float CTaskAimGun::ms_fOverridenPitch = 0.0f;

bool CTaskAimGun::ms_bRenderTestPositions = false;
bool CTaskAimGun::ms_bRenderYawDebug = false;
bool CTaskAimGun::ms_bRenderPitchDebug = false;
bool CTaskAimGun::ms_bRenderDebugText = true;
float CTaskAimGun::ms_fDebugArcRadius = 2.0f;

Vector3	CTaskAimGun::ms_vGoToPosition;
CEntity* CTaskAimGun::ms_pAimAtEntity = NULL;
CEntity* CTaskAimGun::ms_pTaskedPed = NULL;

void CTaskAimGun::SetGoToPosCB()
{
	Vector3 vMousePosition(Vector3::ZeroType);

	if (CPhysics::GetMeasuringToolPos(0, vMousePosition))
	{
		ms_vGoToPosition = vMousePosition;
		Displayf("Go to position set : (%.4f,%.4f,%.4f)", ms_vGoToPosition.x, ms_vGoToPosition.y, ms_vGoToPosition.z);
	}
}

void CTaskAimGun::SetAimAtEntityCB()
{
	CEntity* pEntity = CDebugScene::FocusEntities_Get(0);
	if (pEntity)
	{
		ms_pAimAtEntity = pEntity;
		Displayf("Aim at entity set : (%s)", ms_pAimAtEntity->GetModelName());
	}
}

void CTaskAimGun::SetTaskedEntityCB()
{
	CEntity* pEntity = CDebugScene::FocusEntities_Get(0);
	if (pEntity)
	{
		ms_pTaskedPed = pEntity;
        static const atHashWithStringNotFinal COP_HELI_LOADOUT("LOADOUT_COP_HELI",0x47E9948C);
        CPedInventoryLoadOutManager::SetLoadOut(static_cast<CPed*>(ms_pTaskedPed), COP_HELI_LOADOUT.GetHash());
		static_cast<CPed*>(ms_pTaskedPed)->GetWeaponManager()->EquipBestWeapon();
		Displayf("Tasked at entity set : (%s)", ms_pTaskedPed->GetModelName());
	}
}

void CTaskAimGun::GivePedTaskCB()
{
	CEntity* pEntity = CDebugScene::FocusEntities_Get(0);

	if (!pEntity)
	{
		pEntity = ms_pTaskedPed;
	}
	
	if (pEntity && pEntity->GetIsTypePed() && ms_pAimAtEntity)
	{
		Vector3 vMousePosition(Vector3::ZeroType);

		if (CPhysics::GetMeasuringToolPos(0, vMousePosition))
		{
			ms_vGoToPosition = vMousePosition;
			Displayf("Go to position set : (%.4f,%.4f,%.4f)", ms_vGoToPosition.x, ms_vGoToPosition.y, ms_vGoToPosition.z);
		}

		static bool bAddGoToPointTask = true;
		CTaskSequenceList* pSequence=rage_new CTaskSequenceList;
		if (bAddGoToPointTask)
		pSequence->AddTask(rage_new CTaskGoToPointAiming(CAITarget(ms_vGoToPosition), CAITarget(ms_pAimAtEntity), MOVEBLENDRATIO_RUN));
		pSequence->AddTask(rage_new CTaskGun(CWeaponController::WCT_Aim, CTaskTypes::TASK_AIM_GUN_ON_FOOT, CWeaponTarget(ms_pAimAtEntity), -1.0f));
		CTask* pTask = rage_new CTaskUseSequence(*pSequence);
		CEventGivePedTask event(PED_TASK_PRIORITY_PRIMARY,pTask,false,E_PRIORITY_MAX);
		static_cast<CPed*>(pEntity)->GetPedIntelligence()->AddEvent(event);
	}
}

void CTaskAimGun::RenderYawDebug(const CPed* pPed, Vec3V_In vAimFromPosition, const float fDesiredYawAngle, float fMinYawSweep, float fMaxYawSweep)
{
	// Draw a reference in front of the ped
	ScalarV debugArcRadius = ScalarVFromF32(ms_fDebugArcRadius);
	Vec3V vForward(pPed->GetTransform().GetB());
	Vec3V vScaledForward = vForward*debugArcRadius;
	grcDebugDraw::Sphere(vAimFromPosition + vScaledForward, 0.025f, Color_blue);
	grcDebugDraw::Line(vAimFromPosition, vAimFromPosition + vScaledForward, Color_green);

	Color32 iColor = Color_green1;
	iColor.SetAlpha(128);
	char szDebugText[128];

	// Draw min arc
	Vec3V vRight(pPed->GetTransform().GetA());
	grcDebugDraw::Arc(vAimFromPosition, ms_fDebugArcRadius, iColor, vForward, vRight, 0.0f, fwAngle::LimitRadianAngle(fMaxYawSweep));
	Vec3V vMaxYawArcEndPoint(V_Y_AXIS_WZERO);
	vMaxYawArcEndPoint = RotateAboutZAxis(vMaxYawArcEndPoint, ScalarVFromF32(fwAngle::LimitRadianAngle(pPed->GetTransform().GetHeading() - fMaxYawSweep)));
	vMaxYawArcEndPoint *= debugArcRadius;
	grcDebugDraw::Sphere(vAimFromPosition + vMaxYawArcEndPoint, 0.025f, Color_red);
	grcDebugDraw::Line(vAimFromPosition + vMaxYawArcEndPoint, vAimFromPosition, Color_green);

	// Draw max arc
	grcDebugDraw::Arc(vAimFromPosition, ms_fDebugArcRadius, iColor, vForward, vRight, fwAngle::LimitRadianAngle(fMinYawSweep), 0.0f);
	Vec3V vMinYawArcEndPoint(V_Y_AXIS_WZERO);
	vMinYawArcEndPoint = RotateAboutZAxis(vMinYawArcEndPoint, ScalarVFromF32(fwAngle::LimitRadianAngle(pPed->GetTransform().GetHeading() - fMinYawSweep)));
	vMinYawArcEndPoint *= debugArcRadius;
	grcDebugDraw::Sphere(vAimFromPosition + vMinYawArcEndPoint, 0.025f, Color_purple);
	grcDebugDraw::Line(vAimFromPosition + vMinYawArcEndPoint, vAimFromPosition, Color_green);

	// Draw desired yaw arc
	if (fDesiredYawAngle < 0.0f)
	{
		grcDebugDraw::Arc(vAimFromPosition, ms_fDebugArcRadius, iColor, vForward, vRight, fwAngle::LimitRadianAngle(fDesiredYawAngle), 0.0f, true);
	}
	else
	{
		grcDebugDraw::Arc(vAimFromPosition, ms_fDebugArcRadius, iColor, vForward, vRight, 0.0f, fwAngle::LimitRadianAngle(fDesiredYawAngle), true);
	}
	Vec3V vDesiredYawArcEndPoint(V_Y_AXIS_WZERO);
	vDesiredYawArcEndPoint = RotateAboutZAxis(vDesiredYawArcEndPoint, ScalarVFromF32(fwAngle::LimitRadianAngle(pPed->GetTransform().GetHeading() - fDesiredYawAngle)));
	vDesiredYawArcEndPoint *= debugArcRadius;
	grcDebugDraw::Sphere(vAimFromPosition + vDesiredYawArcEndPoint, 0.025f, Color_orange);
	grcDebugDraw::Line(vAimFromPosition + vDesiredYawArcEndPoint, vAimFromPosition, Color_orange);

	if (ms_bRenderDebugText)
	{
		formatf(szDebugText, "Yaw Max = %.2f", fMaxYawSweep * RtoD);
		grcDebugDraw::Text(vAimFromPosition + vMaxYawArcEndPoint, Color_red, szDebugText);
		formatf(szDebugText, "Yaw Phase Max = 1.0f");
		grcDebugDraw::Text(vAimFromPosition + vMaxYawArcEndPoint, Color_red, 0, 10, szDebugText);

		formatf(szDebugText, "Yaw Min = %.2f", fMinYawSweep * RtoD);
		grcDebugDraw::Text(vAimFromPosition + vMinYawArcEndPoint, Color_purple, szDebugText);
		formatf(szDebugText, "Yaw Phase Min = 0.0f");
		grcDebugDraw::Text(vAimFromPosition + vMinYawArcEndPoint, Color_purple, 0, 10, szDebugText);

		formatf(szDebugText, "Desired Yaw = %.2f", fDesiredYawAngle * RtoD);
		grcDebugDraw::Text(vAimFromPosition + vDesiredYawArcEndPoint, Color_orange, szDebugText);
		formatf(szDebugText, "Desired Yaw Param = %.2f", CTaskAimGun::ConvertRadianToSignal(fDesiredYawAngle, fMinYawSweep, fMaxYawSweep, false));
		grcDebugDraw::Text(vAimFromPosition + vDesiredYawArcEndPoint, Color_orange, 0, 10, szDebugText);
	}
}

void CTaskAimGun::RenderPitchDebug(const CPed* pPed, Vec3V_In vAimFromPosition, const float fDesiredYawAngle, const float fDesiredPitchAngle, float fMinPitchSweep, float fMaxPitchSweep)
{
	Color32 iColor = Color_blue1;
	iColor.SetAlpha(128);
	char szDebugText[128];

	ScalarV debugArcRadius = ScalarVFromF32(ms_fDebugArcRadius);
	Vec3V vDesiredYawArcEndPoint(V_Y_AXIS_WZERO);
	vDesiredYawArcEndPoint = RotateAboutZAxis(vDesiredYawArcEndPoint, ScalarVFromF32(fwAngle::LimitRadianAngle(pPed->GetTransform().GetHeading() - fDesiredYawAngle)));
	vDesiredYawArcEndPoint *= debugArcRadius;

	// Draw desired pitch arc
	Vec3V vPitchArcDrawForwardAxis(V_Y_AXIS_WZERO);
	vPitchArcDrawForwardAxis = RotateAboutZAxis(vPitchArcDrawForwardAxis, ScalarVFromF32(fwAngle::LimitRadianAngle(pPed->GetTransform().GetHeading() - fDesiredYawAngle)));

	Vec3V vPedUp(pPed->GetTransform().GetC());
	if (fDesiredPitchAngle < 0.0f)
	{
		grcDebugDraw::Arc(vAimFromPosition, ms_fDebugArcRadius, iColor, vPitchArcDrawForwardAxis, vPedUp, fwAngle::LimitRadianAngle(fDesiredPitchAngle), 0.0f, true);
	}
	else
	{
		grcDebugDraw::Arc(vAimFromPosition, ms_fDebugArcRadius, iColor, vPitchArcDrawForwardAxis, vPedUp, 0.0f, fwAngle::LimitRadianAngle(fDesiredPitchAngle), true);
	}

	// Create a rotation matrix to rotate the pitch vector about an axis at right angles to the desired yaw vector
	Vec3V vAxisRotation = vDesiredYawArcEndPoint;
	vAxisRotation = RotateAboutZAxis(vAxisRotation, ScalarV(V_PI_OVER_TWO));

	Mat34V mRotMat;
	vAxisRotation = Normalize(vAxisRotation);
	Mat34VFromAxisAngle(mRotMat, vAxisRotation, ScalarVFromF32(-fDesiredPitchAngle)); 

	// Draw desired pitch end point
	Vec3V vDesiredPitchArcEndPoint = vPitchArcDrawForwardAxis;
	vDesiredPitchArcEndPoint = Transform3x3(mRotMat, vDesiredPitchArcEndPoint);
	vDesiredPitchArcEndPoint *= debugArcRadius;
	grcDebugDraw::Line(vAimFromPosition, vAimFromPosition + vDesiredPitchArcEndPoint, Color_blue);
	grcDebugDraw::Sphere(vAimFromPosition + vDesiredPitchArcEndPoint, 0.025f, Color_blue);

	// Draw min pitch arc
	grcDebugDraw::Arc(vAimFromPosition, ms_fDebugArcRadius, Color_blue, vPitchArcDrawForwardAxis, vPedUp, fwAngle::LimitRadianAngleForPitch(fMinPitchSweep), 0.0f);
	Vec3V vMinPitchArcEndPoint = vPitchArcDrawForwardAxis;
	Mat34VFromAxisAngle(mRotMat, vAxisRotation, ScalarVFromF32(-fwAngle::LimitRadianAngleForPitch(fMinPitchSweep))); 
	vMinPitchArcEndPoint = Transform3x3(mRotMat, vMinPitchArcEndPoint);
	vMinPitchArcEndPoint *= debugArcRadius;
	grcDebugDraw::Line(vAimFromPosition, vAimFromPosition + vMinPitchArcEndPoint, Color_blue);
	grcDebugDraw::Sphere(vAimFromPosition + vMinPitchArcEndPoint, 0.025f, Color_purple);

	// Draw max pitch arc
	grcDebugDraw::Arc(vAimFromPosition, ms_fDebugArcRadius, Color_blue, vPitchArcDrawForwardAxis, vPedUp, 0.0f, fwAngle::LimitRadianAngleForPitch(fMaxPitchSweep));
	Vec3V vMaxPitchArcEndPoint = vPitchArcDrawForwardAxis;
	Mat34VFromAxisAngle(mRotMat, vAxisRotation, ScalarVFromF32(-fwAngle::LimitRadianAngleForPitch(fMaxPitchSweep))); 
	vMaxPitchArcEndPoint = Transform3x3(mRotMat, vMaxPitchArcEndPoint);
	vMaxPitchArcEndPoint *= debugArcRadius;
	grcDebugDraw::Line(vAimFromPosition, vAimFromPosition + vMaxPitchArcEndPoint, Color_blue);
	grcDebugDraw::Sphere(vAimFromPosition + vMaxPitchArcEndPoint, 0.025f, Color_red);

	if (ms_bRenderDebugText)
	{
		formatf(szDebugText, "Desired Pitch = %.2f", fDesiredPitchAngle * RtoD);
		grcDebugDraw::Text(vAimFromPosition + vDesiredYawArcEndPoint, Color_orange, 0, 20, szDebugText);
		formatf(szDebugText, "Desired Pitch Param = %.2f", CTaskAimGun::ConvertRadianToSignal(fDesiredPitchAngle, fMinPitchSweep, fMaxPitchSweep, true));
		grcDebugDraw::Text(vAimFromPosition + vDesiredYawArcEndPoint, Color_orange, 0, 30, szDebugText);

		formatf(szDebugText, "Pitch Min = %.2f", fMinPitchSweep * RtoD);
		grcDebugDraw::Text(vAimFromPosition + vMinPitchArcEndPoint, Color_purple, szDebugText);
		formatf(szDebugText, "Pitch Max = %.2f", fMaxPitchSweep * RtoD);
		grcDebugDraw::Text(vAimFromPosition + vMaxPitchArcEndPoint, Color_red, szDebugText);
	}
}
#endif // __BANK

float CTaskAimGun::CalculateDesiredYaw(const CPed* pPed, const Vector3& vStart, const Vector3& vEnd)
{
#if __BANK
	if (!ms_bUseOverridenYaw)
#endif
	{
		// Calculate the direction to the target
		Vector3 vDir = vEnd - vStart; 
		vDir.Normalize();

		// G.S. 14/02/2011 - A suspected non-orthonormal entity matrix seems to be causing problems here.
		taskAssert(pPed->GetTransform().IsOrthonormal());

		// Transform direction into peds local space to make it relative to the ped
		vDir = VEC3V_TO_VECTOR3(pPed->GetTransform().UnTransform3x3(VECTOR3_TO_VEC3V(vDir)));

		vDir.z=0;
		vDir.Normalize(); // normalize again in case of a non-orthonormal ped matrix.

		if (!taskVerifyf(rage::FPIsFinite(vDir.x) && rage::FPIsFinite(vDir.y), "Invalid direction vector vStart : (%.4f, %.4f, %.4f) : vEnd : (%.4f, %.4f, %.4f)", vStart.x, vStart.y, vStart.z, vEnd.x, vEnd.y, vEnd.z))
		{
			return 0.0f;
		}

		// Get the angle of rotation about the z axis, i.e. the yaw
		float fYaw = rage::Atan2f(vDir.x, vDir.y);
		return fwAngle::LimitRadianAngle(fYaw);
	}
#if __BANK
	// Use a fixed yaw value for debugging purposes
	return ms_fOverridenYaw * DtoR;
#endif // __BANK
}

float CTaskAimGun::CalculateDesiredPitch(const CPed* pPed, const Vector3& vStart, const Vector3& vEnd, bool bUntranformRelativeToPed)
{
#if __BANK
	if (ms_bUseOverridenPitch)
	{
		// Use a fixed pitch value for debugging purposes
		return ms_fOverridenPitch * DtoR;
	}
	else
#endif
	{
		// Calculate the target direction
		Vector3 vDiff = vEnd - vStart;

		if (bUntranformRelativeToPed)
		{
			// G.S. 14/02/2011 - A suspected non-orthonormal entity matrix seems to be causing problems here.
			taskAssert(pPed->GetTransform().IsOrthonormal());

			// Transform direction into peds local space to make it relative to the ped
			vDiff = VEC3V_TO_VECTOR3(pPed->GetTransform().UnTransform3x3(VECTOR3_TO_VEC3V(vDiff)));
		}

		Vector2 vDir;
		vDiff.GetVector2XY(vDir);

		// Calculate the pitch
		const float fDiffMag = vDiff.Mag();
		if (fDiffMag != 0.0f)
		{
			float fPitch = Sign(vDiff.z) * rage::AcosfSafe(vDir.Mag() / fDiffMag);
			
			if (aiVerifyf(fPitch >= -THREE_PI && fPitch <= THREE_PI, "CTaskAimGun::CalculateDesiredPitch - Invalid pitch calculated"))
			{
				fPitch = fwAngle::LimitRadianAngleForPitch(fPitch);
			}
			else
			{
				fPitch = 0.0f;
			}
	
			return fPitch;
		}
	}

	return -1.0f;
}

float CTaskAimGun::ConvertRadianToSignal(float fRadianValue, float fMinValue, float fMaxValue, bool bInvert)
{
	float fRange = Abs(fMaxValue - fMinValue);
	float fReturnValue = rage::Clamp(fRadianValue, fMinValue, fMaxValue);
	fReturnValue -= fMinValue;
	fReturnValue = fReturnValue / fRange;

	// Clip may be authored the opposite way
	if (bInvert)
	{
		fReturnValue = 1.0f - fReturnValue;
	}
	return fReturnValue;
}

float CTaskAimGun::SmoothInput(float fCurrentValue, float fDesiredValue, float fMaxChangeRate)
{
	// Calculate the maximum change in current value for this timestep
	const float fMaxChangeForTimeStep = fwTimer::GetTimeStep() * fMaxChangeRate;

	float fReturn = fDesiredValue;

	// If current value is negative, its not been initialised, so just return the desired
	if (fCurrentValue >= 0)
	{
		// Work out the requested change in current value for this timestep
		float fDelta = fDesiredValue - fCurrentValue;
		// Clamp the requested change so we don't go over the maximum change for this timestep
		fDelta = rage::Clamp(fDelta, -fMaxChangeForTimeStep, fMaxChangeForTimeStep);
		fReturn = fCurrentValue + fDelta;
	}

	return fReturn;
}

bool CTaskAimGun::ComputeAimMatrix(const CPed* pPed, Matrix34& mAimMat)
{
	const CObject* pWeaponObject = pPed->GetWeaponManager()->GetEquippedWeaponObject();
	const CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
	const CWeaponModelInfo* pModelInfo = pWeapon->GetWeaponModelInfo();

	if (pModelInfo && pWeaponObject)
	{
		int boneIdx = pModelInfo->GetBoneIndex(WEAPON_MUZZLE);
		if(boneIdx >= 0)
		{
			Matrix34 mWeaponMatrix;
			pWeaponObject->GetGlobalMtx(boneIdx, mWeaponMatrix);
			mWeaponMatrix.RotateLocalZ(-HALF_PI);
			mWeaponMatrix.a *= -1.0f;
			mWeaponMatrix.c *= -1.0f;
			mAimMat = mWeaponMatrix;
			return true;
		}
	}

	return false;
}

bool CTaskAimGun::ShouldUseAimingMotion(const CPed* pPed)
{
	const CWeaponInfo* pWeaponInfo = CTaskMotionPed::GetWeaponInfo(pPed);
	if (pWeaponInfo)
	{
		return ShouldUseAimingMotion(*pWeaponInfo, pWeaponInfo->GetAppropriateWeaponClipSetId(pPed), pPed);
	}
	return false;
}


bool CTaskAimGun::ShouldUseAimingMotion(const CWeaponInfo& weaponInfo, fwMvClipSetId appropriateWeaponClipSetId, const CPed* FPS_MODE_SUPPORTED_ONLY(pPed))
{
	//Throwing weapons now use aiming motion 
	if(weaponInfo.GetIsThrownWeapon())
	{
		return true;
	}
	if((weaponInfo.GetIsGunOrCanBeFiredLikeGun() || weaponInfo.GetCanBeAimedLikeGunWithoutFiring()) && appropriateWeaponClipSetId != CLIP_SET_ID_INVALID)
	{
		return true;
	}
#if FPS_MODE_SUPPORTED
	if(pPed->IsFirstPersonShooterModeEnabledForPlayer(false, true))
	{
		return true;
	}
#endif // FPS_MODE_SUPPORTED
	return false;
}

CTaskAimGun::CTaskAimGun(const CWeaponController::WeaponControllerType weaponControllerType, const fwFlags32& iFlags, const fwFlags32& iGFFlags, const CWeaponTarget& target)
: m_weaponControllerType(weaponControllerType)
, m_iFlags(iFlags)
, m_iGunFireFlags(iGFFlags)
, m_target(target)
, m_iBlanksFired(0)
, m_overrideAimClipSetId(CLIP_SET_ID_INVALID)
, m_overrideAimClipId(CLIP_ID_INVALID)
, m_overrideFireClipSetId(CLIP_SET_ID_INVALID)
, m_overrideFireClipId(CLIP_ID_INVALID)
, m_overrideClipSetId(CLIP_SET_ID_INVALID)
, m_uScriptedGunTaskInfoHash(0)
, m_fFireAnimRate(1.0f)
, m_bForceWeaponStateToFire(false)
, m_bCancelledFireRequest(false)
, m_fireCounter(0)
, m_uTimeInStandardAiming(0)
, m_uTimeInAlternativeAiming(0)
, m_iAmmoInClip(255)
, m_seed(0)
{
	if (NetworkInterface::IsGameInProgress())
	{
		const CPed* pPed = GetEntity() && (GetEntity()->GetType() == ENTITY_TYPE_PED) ? ((const CPed*)GetEntity()) : NULL;
		if (pPed)
		{
			const CWeapon* pWeapon = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeapon() : NULL;
			if (pWeapon)
				m_iAmmoInClip = (u8) pWeapon->GetAmmoInClip();
		}
	}
}

void CTaskAimGun::UpdatePedAccuracyModifier(CPed* pPed, const CEntity* pTargetEntity)
{
	// Don't alter the players accuracy
	if(!pPed->IsAPlayerPed())
	{
		if(pTargetEntity && pTargetEntity->GetIsTypePed())
		{
			taskAssert(pPed->GetWeaponManager());
			const CPed* pTargetPed = static_cast<const CPed*>(pTargetEntity);
			if(pTargetPed->IsAPlayerPed() && 
				pTargetPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_IN_COVER) && 
				((pTargetPed->GetPedIntelligence()->GetQueriableInterface()->GetStateForTaskType(CTaskTypes::TASK_IN_COVER) == CTaskInCover::State_AimIntro) || 
				pTargetPed->GetPedIntelligence()->GetQueriableInterface()->GetStateForTaskType(CTaskTypes::TASK_IN_COVER) == CTaskInCover::State_AimOutro))
			{
				pPed->GetWeaponManager()->GetAccuracy().GetResetVars().SetFlag(CPedAccuracyResetVariables::Flag_TargetComingOutOfCover);
			}
			else if(pTargetPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_IN_COVER))
			{
				pPed->GetWeaponManager()->GetAccuracy().GetResetVars().SetFlag(CPedAccuracyResetVariables::Flag_TargetInCover);
			}
			else if(pTargetPed->GetMotionData()->GetCombatRoll())
			{
				pPed->GetWeaponManager()->GetAccuracy().GetResetVars().SetFlag(CPedAccuracyResetVariables::Flag_TargetInCombatRoll);
			}
			else if(pTargetPed->GetMotionData()->GetIsWalking())
			{
				pPed->GetWeaponManager()->GetAccuracy().GetResetVars().SetFlag(CPedAccuracyResetVariables::Flag_TargetWalking);
			}
			else if(pTargetPed->GetMotionData()->GetIsRunning() || pTargetPed->GetMotionData()->GetIsSprinting())
			{
				pPed->GetWeaponManager()->GetAccuracy().GetResetVars().SetFlag(CPedAccuracyResetVariables::Flag_TargetRunning);
			}
			else if(pTargetPed->GetPedResetFlag( CPED_RESET_FLAG_IsClimbing ) || pTargetPed->GetPedResetFlag( CPED_RESET_FLAG_IsJumping ))
			{
				pPed->GetWeaponManager()->GetAccuracy().GetResetVars().SetFlag(CPedAccuracyResetVariables::Flag_TargetInAir);
			}
			else if(pTargetPed->GetIsInVehicle() && (pTargetPed->GetMyVehicle()->GetVelocity().Mag2() > rage::square(5.0f)))
			{
				pPed->GetWeaponManager()->GetAccuracy().GetResetVars().SetFlag(CPedAccuracyResetVariables::Flag_TargetDrivingAtSpeed);
			}
		}
	}
}

bool CTaskAimGun::CalculateFiringVector(CPed* pPed, Vector3& vStart, Vector3& vEnd, CEntity *pCameraEntity, float* fTargetDistOut, bool bAimFromCamera) const
{
	taskAssert(pPed->GetWeaponManager());

	bool fireVectorFound = false;
	
	//Check the target has been initialized
	if(!m_target.GetIsValid())
	{
		return false;
	}

	Vector3 vTargetPos( Vector3::ZeroType );
	const bool bGotTargetPos = m_target.GetPositionWithFiringOffsets( pPed, vTargetPos );

	//! Use actual weapon aim position when calculating weapon fire vec.
	static dev_bool s_UseActualWeaponAimPosition = true;
	if(s_UseActualWeaponAimPosition && pPed->IsPlayer() && pPed->IsNetworkClone())
	{	
		CNetObjPlayer* pNetObjPlayer = static_cast<CNetObjPlayer*>(pPed->GetNetworkObject());
		if(pNetObjPlayer)
		{
			//! Ensure it is up to date.
			pNetObjPlayer->UpdateTargettedEntityPosition();

			//! Override if we are aiming at an entity.
			if (pNetObjPlayer->HasValidTarget() && pNetObjPlayer->GetAimingTarget())
			{
				pNetObjPlayer->GetActualWeaponAimPosition(vTargetPos);
			}
		}
	}

	taskAssert(vTargetPos.FiniteElements());

	//if we have a valid target, return how far away it is by reference.
	//some aiming code will later use this to know how much to adjust the aiming 
	//vector so that the shots will always be a near-miss
	if (fTargetDistOut)
	{
		if (bGotTargetPos)
		{
			*fTargetDistOut = vTargetPos.Dist(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()));
		}
		else
		{
			*fTargetDistOut = -1.0f;
		}
	}
	
	CVehicleWeapon* pVehicleWeapon = pPed->GetWeaponManager()->GetEquippedVehicleWeapon();
	if( !pVehicleWeapon )
	{
		const CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
		if (!taskVerifyf(pWeapon, "NULL Weapon Pointer In  CTaskAimGun::CalculateFiringVector"))
		{
			return false;
		}

		const Matrix34* pmWeapon;
		const CObject* pWeaponObject = pPed->GetWeaponManager()->GetEquippedWeaponObject();
		if(pWeaponObject)
		{
			pmWeapon = &RCC_MATRIX34(pWeaponObject->GetMatrixRef());
		}
		else // Just use the peds matrix for taunts
		{
			pmWeapon = &RCC_MATRIX34(pPed->GetMatrixRef());
		}

		// Force bullets to be fired directly down the muzzle if GF_FireBulletsInGunDirection is set 
		if( m_iFlags.IsFlagSet(GF_FireBulletsInGunDirection) )
		{
			// Calculate the firing vector from the direction of the weapon
			fireVectorFound = pWeapon->CalcFireVecFromGunOrientation(pPed, *pmWeapon, vStart, vEnd);
		}
		else 
		{
			if( pPed->IsLocalPlayer() && ( !pPed->GetPlayerInfo() || !pPed->GetPlayerInfo()->AreControlsDisabled() ) && !m_iFlags.IsFlagSet(GF_NeverUseCameraForAiming) )
			{
				// if we haven't been specified an alternative entity to use for the camera test use the ped firing the weapon
				if(pCameraEntity == 0)
				{
					pCameraEntity = pPed;
				}

				bool bUseAimCamera = true;

				if(pPed->GetIsDrivingVehicle() && pPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_ASSISTED_AIMING_ON))
				{
					const CPlayerInfo* pPlayerInfo = pPed->GetPlayerInfo();
					if(pPlayerInfo)
					{
						CPlayerPedTargeting& targeting = const_cast<CPlayerPedTargeting&>(pPlayerInfo->GetTargeting());
						if(targeting.GetLockOnTarget())
						{
							// Calculate the firing vector from the direction of the weapon
							fireVectorFound = pWeapon->CalcFireVecFromGunOrientation(pPed, *pmWeapon, vStart, vEnd);
							vEnd = targeting.GetLockonTargetPos();
							bUseAimCamera = false;
						}
					}
				}

				if(bUseAimCamera)
				{
					// Calculate the firing vector from the weapon camera that goes through the target position
					pWeapon->CalcFireVecFromAimCamera(pCameraEntity, *pmWeapon, vStart, vEnd);
					fireVectorFound = true;

					// Check if anything is in the way.
					Vector3 vGunStart, vGunEnd;

					bool bFirstPerson = false;
					if(CTaskAimGunVehicleDriveBy::IsHidingGunInFirstPersonDriverCamera(pPed) || 
						(pPed->IsInFirstPersonVehicleCamera() && m_iGunFireFlags.IsFlagSet(GFF_ForceFireFromCamera)))
					{
						bFirstPerson=true;
					}

					if(bFirstPerson)
					{
						vGunStart = vStart;
						vGunEnd = vEnd;
					}
					else
					{
						pWeapon->CalcFireVecFromGunOrientation(pCameraEntity, *pmWeapon, vGunStart, vGunEnd);
					}

					// url:bugstar:5518472 - Don't do the fancy probe detection stuff on the RAYMINIGUN, which uses laser tracers. Means it'll shoot through stuff in more cases, but less bullet bending. 
					bool bFireFromMuzzle = m_iGunFireFlags.IsFlagSet(GFF_FireFromMuzzle) || pWeapon->GetWeaponHash() == ATSTRINGHASH("WEAPON_RAYMINIGUN", 0xB62D1F67);
					if (!bFireFromMuzzle)
					{
						if (!m_iGunFireFlags.IsFlagSet(GFF_ForceFireFromCamera) && !camInterface::GetGameplayDirector().IsFirstPersonAiming() && !bAimFromCamera)
						{				
							static dev_float PROBE_START_OFFSET = -0.3f;
							static dev_float PROBE_LENGTH = 1.6f;
							static dev_float DISTANCE_ALONG_CAMERA = 6.0f;
							static dev_float PED_PROBE_LENGTH = 1.0f; 
							
							// Calculate a new end from the gun start to 7m down the camera forward
							Vector3 camDir = vEnd-vStart;
							if(camDir.Mag2() > 0.001f)
								camDir.NormalizeFast();
							Vector3 vAimDir = (vStart +(camDir * DISTANCE_ALONG_CAMERA)) - vGunStart;
							if(vAimDir.Mag2() > 0.001f)
								vAimDir.NormalizeFast();
							Vector3 vTestStart, vTestEnd;
							vTestStart = vGunStart + (vAimDir * PROBE_START_OFFSET);
							vTestEnd = vTestStart + (vAimDir * PROBE_LENGTH);

							static const s32 iCollisionFlags = ArchetypeFlags::GTA_MAP_TYPE_WEAPON | ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_OBJECT_TYPE | ArchetypeFlags::GTA_RAGDOLL_TYPE;
							static const s32 iIgnoreFlags = WorldProbe::LOS_IGNORE_SHOOT_THRU | WorldProbe::LOS_IGNORE_SHOOT_THRU_FX;
							WorldProbe::CShapeTestProbeDesc probeDesc;
							WorldProbe::CShapeTestFixedResults<> gunProbeResult;
							probeDesc.SetResultsStructure(&gunProbeResult);
							probeDesc.SetStartAndEnd(vTestStart, vTestEnd);
							probeDesc.SetIncludeFlags(iCollisionFlags);
							probeDesc.SetExcludeEntity(pPed);
							probeDesc.SetOptions(iIgnoreFlags);
							probeDesc.SetContext(WorldProbe::LOS_GeneralAI);
							taskAssertf(FPIsFinite(vTestStart.x) && FPIsFinite(vTestStart.y) && FPIsFinite(vTestStart.z), "CTaskAimGun::CalculateFiringVector: World Probe 1");
							if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
							{
								//Reduce the probe length for peds, ignore any hits that are further than PED_PROBE_LENGTH
								CEntity* pEntity = gunProbeResult[0].GetHitEntity();
								if(!pEntity || !pEntity->GetIsTypePed() || (gunProbeResult[0].GetHitPosition() - vTestStart).Mag2() < rage::square(PED_PROBE_LENGTH) )
								{
									vAimDir = vEnd - vStart;
									if(vAimDir.Mag2() > 0.001f)
										vAimDir.NormalizeFast();
									vTestStart = vStart + (vAimDir * PROBE_START_OFFSET);
									vTestEnd = vStart + (vAimDir * DISTANCE_ALONG_CAMERA);
									WorldProbe::CShapeTestFixedResults<> camProbeResult;
									probeDesc.SetResultsStructure(&camProbeResult);
									probeDesc.SetStartAndEnd(vTestStart, vTestEnd);
									taskAssertf(FPIsFinite(vTestStart.x) && FPIsFinite(vTestStart.y) && FPIsFinite(vTestStart.z), "CTaskAimGun::CalculateFiringVector: World Probe 2");
									bool cameraHit = WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc);
									if(!cameraHit || (vTestStart - camProbeResult[0].GetHitPosition()).Mag2() > rage::square(PROBE_LENGTH))
									{
										if(cameraHit)
										{
											vTestEnd = camProbeResult[0].GetHitPosition();
										}
										else
										{
											//Default to DISTANCE_ALONG_CAMERA/2
											vTestEnd = vStart + (vAimDir * (DISTANCE_ALONG_CAMERA/2));
										}

										vAimDir = vTestEnd - vGunStart;
										if(vAimDir.Mag2() > 0.001f)
											vAimDir.NormalizeFast();
										vTestStart = vGunStart + (vAimDir * PROBE_START_OFFSET);
									
										probeDesc.SetStartAndEnd(vTestStart, vTestEnd);
										taskAssertf(FPIsFinite(vTestStart.x) && FPIsFinite(vTestStart.y) && FPIsFinite(vTestStart.z), "CTaskAimGun::CalculateFiringVector: World Probe 3");
										if(pWeapon->GetWeaponInfo() && pWeapon->GetWeaponInfo()->GetBatchSpread() == 0.0f && WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
										{
											//Offset used so set the vEnd
											vEnd = vGunStart + vAimDir * pWeapon->GetRange();
										}

										// We've hit something with the gun probe but didn't with the camera or the intersect finder so use the gun start.
										vStart = vGunStart;
									}
								}
							}
						}
					} 
					else
					{
						//Adjust the end position to ensure we hit the perceived reticule target
						WorldProbe::CShapeTestHitPoint probeIsect;
						WorldProbe::CShapeTestResults probeResults(probeIsect);
						WorldProbe::CShapeTestProbeDesc probeDesc;
						static const s32 iIgnoreFlags = WorldProbe::LOS_IGNORE_SHOOT_THRU | WorldProbe::LOS_IGNORE_SHOOT_THRU_FX;
						probeDesc.SetResultsStructure(&probeResults);
						probeDesc.SetStartAndEnd(vStart, vEnd);
						probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_WEAPON_TYPES);
						probeDesc.SetExcludeEntity(pPed);
						probeDesc.SetOptions(iIgnoreFlags);
						probeDesc.SetContext(WorldProbe::LOS_GeneralAI);
						taskAssertf(FPIsFinite(vStart.x) && FPIsFinite(vStart.y) && FPIsFinite(vStart.z), "CTaskAimGun::CalculateFiringVector: World Probe 4");
						if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
						{
							//float range = (vEnd - vStart).Mag();
							vEnd = probeResults[0].GetHitPosition();
// 							Vector3 newFireVector = vEnd - vGunStart;
// 							float newRange = newFireVector.Mag();
// 							if(newRange > 0.001f)
// 								newFireVector.Normalize();
// 							vEnd = vEnd + (newFireVector * (range - newRange));
						}
						vStart = vGunStart;
					}
				}

#if __BANK
				TUNE_BOOL(UseGunDirection, false);
#endif // __BANK
				if (m_iGunFireFlags.IsFlagSet(GFF_EnableDumbFire)
#if __BANK
					|| UseGunDirection
#endif // __BANK
					)
				{
					// Calculate the firing vector from the direction of the weapon
					fireVectorFound = pWeapon->CalcFireVecFromGunOrientation(pPed, *pmWeapon, vStart, vEnd);
				}
			}
			else
			{
				if(!bGotTargetPos )
				{
					// Our position has become invalid
					Warningf("Invalid target in CTaskAimGun::CalculateFiringVector");
					return false;
				}

				// Calculate the END of the firing vector using the specific position
				fireVectorFound = pWeapon->CalcFireVecFromPos(pPed, *pmWeapon, vStart, vEnd, vTargetPos);
			}
		}

		//
		// Adjust the end vector to be inside the IK limits (when in third person)
		//

		if((!pWeapon->GetHasFirstPersonScope() && m_iFlags.IsFlagSet(GF_FireAtIkLimitsWhenReached) && (pPed->GetIkManager().GetTorsoSolverStatus() & CIkManager::IK_SOLVER_UNREACHABLE_YAW)) || (pPed->GetIkManager().GetTorsoSolverStatus() & CIkManager::IK_SOLVER_UNREACHABLE_PITCH))
		{
			// Get the pitch and yaw limits from the Ik manager
			float fMinYaw    = pPed->GetIkManager().GetTorsoMinYaw();
			float fMaxYaw    = pPed->GetIkManager().GetTorsoMaxYaw();
			float fYawOffset = pPed->GetIkManager().GetTorsoOffsetYaw();
			float fMinPitch  = pPed->GetIkManager().GetTorsoMinPitch();
			float fMaxPitch  = pPed->GetIkManager().GetTorsoMaxPitch();

			static dev_float IK_LIMIT_TOLERANCE = EIGHTH_PI * 0.5f;
			fMinYaw   -= IK_LIMIT_TOLERANCE;
			fMaxYaw   += IK_LIMIT_TOLERANCE;
			fMinPitch -= IK_LIMIT_TOLERANCE;
			fMaxPitch += IK_LIMIT_TOLERANCE;

			// Work out the desired aiming direction
			Vector3 vFiringDirection = vEnd - vStart;
			vFiringDirection.RotateZ(-pPed->GetTransform().GetHeading() + fYawOffset);

			// Calculate the yaw
			float fYaw = rage::Atan2f(-vFiringDirection.x, vFiringDirection.y);
			fYaw = fwAngle::LimitRadianAngle(fYaw);

			// Calculate the pitch
			float fPitch = rage::Atan2f(vFiringDirection.z, vFiringDirection.XYMag());
			fPitch = fwAngle::LimitRadianAngle(fPitch);

			// If outside the maximum limits and the player, fire in the gun direction as the camera
			// fire calculation won't work at these acute angles
			static dev_float MAX_IK_LIMIT = 0.4f;
			if(pPed->IsLocalPlayer() && ((fYaw < (fMinYaw - MAX_IK_LIMIT) || fYaw > (fMaxYaw + MAX_IK_LIMIT)) || (fPitch < (fMinPitch - MAX_IK_LIMIT) || fPitch > (fMaxPitch + MAX_IK_LIMIT))))
			{
				// Calculate the firing vector from the direction of the weapon
				fireVectorFound = pWeapon->CalcFireVecFromGunOrientation(pPed, *pmWeapon, vStart, vEnd);
			}
			else
			{
				// Limit the angle
				if(fYaw < fMinYaw)
				{
					vFiringDirection.RotateZ(fMinYaw - fYaw);
				}
				else if(fYaw > fMaxYaw)
				{
					vFiringDirection.RotateZ(fMaxYaw - fYaw);
				}

				// Limit the pitch
				if(fPitch < fMinPitch)
				{
					vFiringDirection.z = rage::Tanf(fMinPitch) * vFiringDirection.XYMag();
				}
				else if(fPitch > fMaxPitch)
				{
					vFiringDirection.z = rage::Tanf(fMaxPitch) * vFiringDirection.XYMag();
				}

				// Update the firing vector
				vFiringDirection.RotateZ(pPed->GetTransform().GetHeading() - fYawOffset);
				vEnd = vStart + vFiringDirection;
			}
		}

#if USE_TARGET_SEQUENCES
		//always use the exact target position when the ped is firing in a target sequence
		CTargetSequenceHelper* pTargetSequence = pPed->FindActiveTargetSequence();
		if (pTargetSequence)
		{
			pTargetSequence->GetTargetPosition(vEnd);
		}
#endif // USE_TARGET_SEQUENCES
	}
	// Otherwise just use the original target position
	else
	{
		taskAssert( bGotTargetPos );
		vEnd = vTargetPos;
		fireVectorFound = true;
	}

#if __DEV
	static dev_bool RENDER_FIRING_LINE = false;
	if(RENDER_FIRING_LINE)
	{
		grcDebugDraw::Line(vStart, vEnd, Color_red);
	}
#endif

	return fireVectorFound;
}

CTaskAimGun::FSM_Return CTaskAimGun::ProcessPreFSM()
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	// If the task is aborted then continues
	if(GetIsFlagSet(aiTaskFlags::IsAborted))
	{
		return FSM_Quit;
	}

	bool bThrowingProjectileWhileAiming = pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_ThrowingGrenadeWhileAiming);
	bool bFirstPerson = false;

#if FPS_MODE_SUPPORTED
	bFirstPerson = pPed->IsFirstPersonShooterModeEnabledForPlayer(true, true);
#endif

	// Check we still have a valid target
	if(!m_target.GetIsValid() && !bThrowingProjectileWhileAiming && !bFirstPerson)
	{
		return FSM_Quit;
	}

	taskAssert(pPed->GetWeaponManager());
	const CObject* pWeaponObject = pPed->GetWeaponManager()->GetEquippedWeaponObject();
	const CVehicleWeapon* pVehicleWeapon = pPed->GetWeaponManager()->GetEquippedVehicleWeapon();
	if(!pVehicleWeapon && !pWeaponObject && !bFirstPerson && !pPed->GetWeaponManager()->GetIsArmedMelee() && !bThrowingProjectileWhileAiming)
	{
		// No weapon to aim with, quit task
		return FSM_Quit;
	}

	// We are aiming while this task is running
	pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_IsAimingGun, true );

	// Updates the ped weapon accuracy based on things like target status
	UpdatePedAccuracyModifier(pPed, m_target.GetEntity());

	// Ik
	CTaskGun::ProcessTorsoIk(*pPed, m_iFlags, m_target, GetClipHelper());

	if (pPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_ASSISTED_AIMING_ON) && pPed->IsLocalPlayer() && pPed->GetPlayerInfo()->GetTargeting().GetLockOnTarget())
	{
		m_iFlags.SetFlag(GF_NeverUseCameraForAiming);
	}
	else
	{
		m_iFlags.ClearFlag(GF_NeverUseCameraForAiming);
	}

	return FSM_Continue;
}

void CTaskAimGun::CleanUp()
{
	// No longer aiming
	GetPed()->SetPedConfigFlag( CPED_CONFIG_FLAG_IsAimingGun, false );

	if(GetClipHelper())
	{
		GetClipHelper()->SetFlag(APF_SKIP_NEXT_UPDATE_BLEND);
	}

	// Base class
	CTask::CleanUp();
}

float CTaskAimGun::GetFiringAnimPlaybackRate() const
{
	const CPed* pPed = GetPed();

	//
	// The clip rate determines the rate of fire - we scale the clip rate to produce
	// faster or slower firing rates based on config data
	//

	const CWeaponInfo* pWeaponInfo = CTaskMotionPed::GetWeaponInfo(pPed);
	taskAssert(pWeaponInfo);

	float fRate = m_iFlags.IsFlagSet(GF_BlindFiring) ? pWeaponInfo->GetBlindFireRateModifier() : pWeaponInfo->GetFireRateModifier();
	if(!pPed->IsAPlayerPed())
	{
		// Re-randomise the fire rate after every shot to break up the repetitive sounds
		static dev_float MIN_RATE_MOD = 0.9f;
		static dev_float MAX_RATE_MOD = 1.1f;
		fRate = fwRandom::GetRandomNumberInRange(fRate * MIN_RATE_MOD, fRate * MAX_RATE_MOD);
	}

	return fRate;
}

bool CTaskAimGun::IsWeaponBlocked(CPed* pPed, bool bIgnoreControllerState, float sfProbeLengthMultiplier, float fRadiusMultiplier, bool bCheckAlternativePos, bool bWantsToFire, bool bGunGrenadeThrow )
{
	bool bBlocked = false;

	if (pPed->GetPedResetFlag(CPED_RESET_FLAG_IsThrowingProjectileWhileAiming) && !bGunGrenadeThrow)
	{
		return false;
	}

	taskAssert(pPed->GetWeaponManager());
	CWeapon* pEquippedWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();

	bool bInFPSMode = false;
#if FPS_MODE_SUPPORTED
	bInFPSMode = pPed->IsFirstPersonShooterModeEnabledForPlayer(false);
#endif	//FPS_MODE_SUPPORTED

	bool bDisableBlocking = false;

	if(pPed->IsPlayer())
	{
		// disabled the blocked state when coming out of cover as the blocked probes won't be valid if the ped is rotating
		const CTaskPlayerOnFoot* pPlayerOnFootTask	= static_cast<const CTaskPlayerOnFoot*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_PLAYER_ON_FOOT));
		if(pPlayerOnFootTask)
		{
			static dev_float FROM_COVER_BLOCK_TIME = 500;
			static dev_float FROM_COVER_BLOCK_TIME_FPS = 250;	// Need weapon blocked check during exiting cover
			u32 uBlockTime = bInFPSMode ? (u32)FROM_COVER_BLOCK_TIME_FPS : (u32)FROM_COVER_BLOCK_TIME;
			if(	(pPlayerOnFootTask->GetLastTimeInCoverTask() + uBlockTime) > fwTimer::GetTimeInMilliseconds())
			{
				bDisableBlocking = true;
			}
		}
	}

	// Disable weapons with OnlyAllowFiring flag, e.g., petrol can.
	if(pEquippedWeapon && (pEquippedWeapon->GetWeaponInfo()->GetOnlyAllowFiring() || (pEquippedWeapon->GetWeaponInfo()->GetCanBeAimedLikeGunWithoutFiring() && !bInFPSMode)))
	{
		bDisableBlocking = true;
	}

	if(!m_iFlags.IsFlagSet(GF_DisableBlockingClip) && 
		pEquippedWeapon && !bDisableBlocking)
	{
		//Make sure we stay in the standard anim for a min of 500 millseconds
		if( fwTimer::GetTimeInMilliseconds() - m_uTimeInStandardAiming < 500)
		{
			return false;
		}

		//Make sure we stay in the alternative anim for a min of 500 millseconds
		if( fwTimer::GetTimeInMilliseconds() - m_uTimeInAlternativeAiming < 500)
		{
			pPed->SetPedResetFlag(CPED_RESET_FLAG_UseAlternativeWhenBlock, true);
			return false;
		}

		//If we are in alternative aiming then increase the radius of the probe to stop oscillation
		float fRadiusMultiplierInHighForLow = fRadiusMultiplier;
		if(!bCheckAlternativePos)
		{
			CTaskMotionAiming* pAimingTask = static_cast<CTaskMotionAiming*>(pPed->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_AIMING));
			if(pAimingTask)
			{
				fwMvClipSetId clipSetId = pAimingTask->GetAimClipSet();
				if( clipSetId == pEquippedWeapon->GetWeaponInfo()->GetAlternativeClipSetWhenBlockedId(*pPed))
				{
					fRadiusMultiplierInHighForLow *= 2.0f;
				}
			}
		}
		
		if(pPed->IsLocalPlayer())
		{
			bool bValidTargetPosition = false;
			Vector3 vTargetPos( Vector3::ZeroType );

			// First check for the more precise reading, the camera aim at position
			const CObject* pWeaponObject = pPed->GetWeaponManager()->GetEquippedWeaponObject();
			if( pWeaponObject && pEquippedWeapon )
			{
				const Matrix34& mWeapon = RCC_MATRIX34(pWeaponObject->GetMatrixRef());

				Vector3 vStart( Vector3::ZeroType );
				bValidTargetPosition = pEquippedWeapon->CalcFireVecFromAimCamera( pPed, mWeapon, vStart, vTargetPos );
			}
			
			if( !bValidTargetPosition && m_target.GetIsValid() )
				bValidTargetPosition = m_target.GetPositionWithFiringOffsets( pPed, vTargetPos );
			
			// If we successfully retrieved a target position, perform the weapon block checks
			if( bValidTargetPosition )
			{
				WeaponControllerState controllerState = GetWeaponControllerState(pPed);
				
#if FPS_MODE_SUPPORTED
				bool bReloadingInFPSMode = (controllerState == WCS_Reload) && pPed->IsFirstPersonShooterModeEnabledForPlayer(false);
#endif
				if( ( bIgnoreControllerState || ( controllerState == WCS_Aim || controllerState == WCS_Fire || controllerState == WCS_FireHeld FPS_MODE_SUPPORTED_ONLY(|| bReloadingInFPSMode)) ) && 
					CTaskWeaponBlocked::IsWeaponBlocked(pPed, vTargetPos, NULL, sfProbeLengthMultiplier, fRadiusMultiplierInHighForLow, bCheckAlternativePos, bWantsToFire, bGunGrenadeThrow ))
				{
					bBlocked = true;
				}		
			}
		}
		else
		{
			if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_EnableWeaponBlocking)) 
			{
				Vector3 vTargetPos;			
				if(m_target.GetIsValid() && m_target.GetPositionWithFiringOffsets(pPed, vTargetPos))
				{
					if (CTaskWeaponBlocked::IsWeaponBlocked(pPed, vTargetPos, NULL, sfProbeLengthMultiplier, fRadiusMultiplier, bCheckAlternativePos, bWantsToFire, bGunGrenadeThrow ))
						bBlocked = true;
				}				
			} 
			else
				bBlocked = m_iFlags.IsFlagSet(GF_ForceBlockedClips);
		}
	}

	if(bBlocked && !bCheckAlternativePos)
	{
		//We are blocked so check the alternative anim position
		if(pEquippedWeapon && 
			pEquippedWeapon->GetWeaponInfo() && pEquippedWeapon->GetWeaponInfo()->GetAlternativeClipSetWhenBlockedId(*pPed) != CLIP_SET_ID_INVALID &&
		   !IsWeaponBlocked(pPed,false,sfProbeLengthMultiplier,fRadiusMultiplier, true, bWantsToFire))
		{
			fwMvClipSetId clipSetId = CLIP_SET_ID_INVALID;
			CTaskMotionAiming* pAimingTask = static_cast<CTaskMotionAiming*>(pPed->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_AIMING));
			if(pAimingTask)
			{
				clipSetId = pAimingTask->GetAimClipSet();

				//If the current clipset isn't alternative then reset the timer to stay in the state.
				if( clipSetId != pEquippedWeapon->GetWeaponInfo()->GetAlternativeClipSetWhenBlockedId(*pPed))
				{
					m_uTimeInAlternativeAiming = fwTimer::GetTimeInMilliseconds();
				}
			}

			//Normal anim is blocked but can use the alternative aim. 
			pPed->SetPedResetFlag(CPED_RESET_FLAG_UseAlternativeWhenBlock, true);
			bBlocked = false;
		}
	}
	else if(!bCheckAlternativePos)
	{
		fwMvClipSetId clipSetId = CLIP_SET_ID_INVALID;
		CTaskMotionAiming* pAimingTask = static_cast<CTaskMotionAiming*>(pPed->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_AIMING));
		if(pAimingTask && pEquippedWeapon)
		{
			clipSetId = pAimingTask->GetAimClipSet();	
			fwMvClipSetId weaponClipSet = pEquippedWeapon->GetWeaponInfo()->GetWeaponClipSetId(*pPed);
#if FPS_MODE_SUPPORTED
			// Should we use appropriate weapon clip set for third person too?
			if(pPed->IsFirstPersonShooterModeEnabledForPlayer(false, true))
			{
				weaponClipSet = pEquippedWeapon->GetWeaponInfo()->GetAppropriateWeaponClipSetId(pPed);
			}
#endif // FPS_MODE_SUPPORTED

			//If the current clipset isn't default then reset the timer to stay in state.
			if(pEquippedWeapon && pEquippedWeapon->GetWeaponInfo() && clipSetId != weaponClipSet)
			{
				m_uTimeInStandardAiming = fwTimer::GetTimeInMilliseconds();
			}
		}
	}
	
	return bBlocked;
}

bool CTaskAimGun::CanFireAtTarget(CPed *pPed, CPed *pForceTarget) const
{
	CWeapon* pEquippedWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
	if(pEquippedWeapon && pPed->IsLocalPlayer())
	{
		WeaponControllerState controllerState = GetWeaponControllerState(pPed);
		CPlayerPedTargeting &targeting	= const_cast<CPlayerPedTargeting &>(pPed->GetPlayerInfo()->GetTargeting());					
		const CEntity* pEntity = targeting.GetFreeAimTargetRagdoll() ? targeting.GetFreeAimTargetRagdoll() : targeting.GetLockOnTarget();

		if(pForceTarget)
		{
			pEntity = pForceTarget;
		}

		if(pEntity)
		{
			if((controllerState == WCS_Fire || controllerState == WCS_FireHeld || (pForceTarget != NULL)) && pEquippedWeapon->GetWeaponInfo() && !pEquippedWeapon->GetWeaponInfo()->GetIsNonViolent() && !pPed->IsAllowedToDamageEntity(pEquippedWeapon->GetWeaponInfo(), pEntity))
			{
				// allow firing at ghosted players 
				if (!(NetworkInterface::IsGameInProgress() && NetworkInterface::IsDamageDisabledInMP(*pPed, *pEntity)))
				{
					// B*2741181 - Force the second shot when aiming at friendlies for DBShotgun (it won't damage him anyway, but can mess up the reload if we don't)
					bool bAllowSecondShotForDBShotgun = false;
					if (pEquippedWeapon->GetWeaponInfo() && pEquippedWeapon->GetWeaponInfo()->GetHash() == ATSTRINGHASH("WEAPON_DBSHOTGUN",0xEF951FBB))
					{
						if (pEquippedWeapon->GetAmmoInClip() == 1)
						{
							bAllowSecondShotForDBShotgun = true;
						}
					}

					if (!bAllowSecondShotForDBShotgun)
					{
						taskDebugf1("CTaskAimGun::CanFireAtTarget: FALSE: Ped 0x%p [%s], controllerState %d, pEntity 0x%p [%s]", pPed, pPed->GetModelName(), controllerState, pEntity, pEntity->GetModelName());
						return false;
					}
				}
			}
		}

		// Can't fire while throwing a projectile weapon
		if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_ThrowingGrenadeWhileAiming))
		{
			taskDebugf1("CTaskAimGun::CanFireAtTarget: FALSE: Ped 0x%p [%s], CPED_CONFIG_FLAG_ThrowingGrenadeWhileAiming", pPed, pPed->GetModelName());
			return false;
		}

		// Can't fire if the weapon is flagged as being able to be aimed like gun without firing
		if(pEquippedWeapon->GetWeaponInfo() && pEquippedWeapon->GetWeaponInfo()->GetCanBeAimedLikeGunWithoutFiring())
		{
			taskDebugf1("CTaskAimGun::CanFireAtTarget: FALSE: Ped 0x%p [%s], CWeaponInfoFlags::CanBeAimedLikeGun", pPed, pPed->GetModelName());
			return false;
		}

		// Can't fire if script are blocking firing via reset flag
		if(pPed->GetPedResetFlag(CPED_RESET_FLAG_BlockWeaponFire))
		{
			taskDebugf1("CTaskAimGun::CanFireAtTarget: FALSE: Ped 0x%p [%s], CPED_RESET_FLAG_BlockWeaponFire", pPed, pPed->GetModelName());
			return false;
		}
	}

	return true;
}

bool CTaskAimGun::FireGun(CPed* pPed, bool bDriveBy, bool bUseVehicleAsCamEntity)
{
	taskDebugf1("CTaskAimGun::FireGun: m_bCancelledFireRequest = FALSE, Ped 0x%p [%s], m_bCancelledFireRequest %d", pPed, pPed->GetModelName(), m_bCancelledFireRequest);
	m_bCancelledFireRequest = false;

	weaponDebugf3("CTaskAimGun::FireGun");

	taskAssert(pPed->GetWeaponManager());
	CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
	taskAssert(pWeapon);

	const CWeaponInfo* pWeaponInfo = pWeapon->GetWeaponInfo();

	// Check if we should early out on some simple conditions that would prevent the weapon from firing
	if( !m_bForceWeaponStateToFire &&
		(pWeaponInfo->GetFireType() == FIRE_TYPE_DELAYED_HIT || pWeaponInfo->GetFireType() == FIRE_TYPE_INSTANT_HIT) )
	{
		u32 uEndTime = fwTimer::GetTimeInMilliseconds() + fwTimer::GetTimeStepInMilliseconds();
		if (pWeapon->GetNextShotAllowedTime() >= uEndTime || (pWeapon->GetState() != CWeapon::STATE_READY && pWeapon->GetState() != CWeapon::STATE_WAITING_TO_FIRE) )
		{
			//taskDebugf1("CTaskAimGun::FireGun: m_bCancelledFireRequest = TRUE (1), Ped 0x%p [%s], m_bCancelledFireRequest %d", pPed, pPed->GetModelName(), m_bCancelledFireRequest);
			//m_bCancelledFireRequest = true;
			return false;
		}
	}

	const CObject* pWeaponObject = pPed->GetWeaponManager()->GetEquippedWeaponObject();
	CVehicleWeapon* pVehicleWeapon = pPed->GetWeaponManager()->GetEquippedVehicleWeapon();
	if (!pWeaponObject && !pVehicleWeapon)
	{
		taskDebugf1("CTaskAimGun::FireGun: m_bCancelledFireRequest = TRUE (2), Ped 0x%p [%s], m_bCancelledFireRequest %d", pPed, pPed->GetModelName(), m_bCancelledFireRequest);
		m_bCancelledFireRequest = true;
		return false;
	}

	Vector3 vStart, vEnd;

    CEntity *cameraEntity = 0;

    if (bUseVehicleAsCamEntity)
    {
	    CVehicle* pVehicle = pPed->GetMyVehicle();
	    taskAssert(pVehicle);
        cameraEntity = pVehicle;
    }

	float fDesiredTargetDistance = -1.0f;
	
	// Moved from CalculateFiringVector as is was causing issues in the scripted gun tasks
	if(pPed->IsLocalPlayer())
	{
		CPlayerPedTargeting &targeting	= const_cast<CPlayerPedTargeting &>(pPed->GetPlayerInfo()->GetTargeting());					
		const CEntity* pEntity = targeting.GetLockOnTarget() ? targeting.GetLockOnTarget() : targeting.GetFreeAimTargetRagdoll();	
		if ( !pPed->IsAllowedToDamageEntity( pWeaponInfo, pEntity ) )
		{
			// allow firing at ghost players
			if (!(NetworkInterface::IsGameInProgress() && NetworkInterface::AreBulletsImpactsDisabledInMP(*pPed, *pEntity)))
			{
				// B*2741181 - Aiming at friendlies can mess up reloads of the DBShotgun, as we force the second shot no matter what.
				bool bAllowSecondShotForDBShotgun = false;
				if (pWeapon->GetWeaponInfo() && pWeapon->GetWeaponInfo()->GetHash() == ATSTRINGHASH("WEAPON_DBSHOTGUN",0xEF951FBB))
				{
					if (pWeapon->GetAmmoInClip() == 1)
					{
						bAllowSecondShotForDBShotgun = true;
					}
				}

				if (!bAllowSecondShotForDBShotgun)
				{
					// Unset the fire at least one bullet flag
					m_iFlags.ClearFlag(GF_FireAtLeastOnce);
					taskDebugf1("CTaskAimGun::FireGun: m_bCancelledFireRequest = TRUE (3), Ped 0x%p [%s], m_bCancelledFireRequest %d", pPed, pPed->GetModelName(), m_bCancelledFireRequest);
					m_bCancelledFireRequest = true;
					return false; // do not shoot friendly. B* 390727
				}
			}
		}
	}

	if (!CalculateFiringVector(pPed, vStart, vEnd, cameraEntity, &fDesiredTargetDistance))
	{
		taskDebugf1("CTaskAimGun::FireGun: m_bCancelledFireRequest = TRUE (4), Ped 0x%p [%s], m_bCancelledFireRequest %d", pPed, pPed->GetModelName(), m_bCancelledFireRequest);
		m_bCancelledFireRequest = true;
		return false;
	}

	// Store whether the gun was fired successfully
	bool bFired = false;

	if( !pVehicleWeapon )
	{
		Matrix34 mWeapon;
		if(pWeaponInfo->GetIsProjectile())
		{
			if(pWeapon->GetWeaponModelInfo()->GetBoneIndex(WEAPON_PROJECTILE) != -1)
			{
				pWeaponObject->GetGlobalMtx(pWeapon->GetWeaponModelInfo()->GetBoneIndex(WEAPON_PROJECTILE), mWeapon);
			}
			else if(pWeapon->GetWeaponModelInfo()->GetBoneIndex(WEAPON_MUZZLE) != -1)
			{
				pWeaponObject->GetGlobalMtx(pWeapon->GetWeaponModelInfo()->GetBoneIndex(WEAPON_MUZZLE), mWeapon);
			}
			else
			{
				pWeaponObject->GetMatrixCopy(mWeapon);
			}
		}
		else
		{
			pWeaponObject->GetMatrixCopy(mWeapon);
		}

		CWeapon::sFireParams params(pPed, mWeapon, &vStart, &vEnd);
		params.fDesiredTargetDistance = fDesiredTargetDistance;

		const CEntity* pTargetEntity = m_target.GetEntity();
		if( pTargetEntity )
		{
			params.pTargetEntity = pTargetEntity;
		}

		//--------------------------

		if(pPed->IsNetworkClone())
		{
			if(pWeaponInfo->GetEffectGroup()==WEAPON_EFFECT_GROUP_ROCKET)
			{  //Clones wait for synced projectile info before firing rockets B*1972374
				CProjectileManager::FireOrPlacePendingProjectile(pPed, GetNetSequenceID()); 
			}

			if(pPed->IsPlayer())
			{
				CNetObjPlayer* netObjPlayer = (CNetObjPlayer*)NetworkUtils::GetNetworkObjectFromEntity(pPed);
				if(netObjPlayer)
				{
					if(netObjPlayer->IsFreeAimingLockedOnTarget() && netObjPlayer->GetAimingTarget())
					{
						params.pTargetEntity = netObjPlayer->GetAimingTarget();
					}
				}
			}
		}

		//--------------------------

		if(ShouldFireBlank(pPed)) 
		{
			params.iFireFlags.SetFlag(CWeapon::FF_FireBlank);
		}

		if (bDriveBy)
		{
			const CVehicleSeatAnimInfo* pSeatClipInfo = CVehicleMetadataMgr::GetSeatAnimInfoFromPed(pPed);
			if (pSeatClipInfo)
			{
				const CVehicleDriveByInfo* pDrivebyClipInfo = pSeatClipInfo->GetDriveByInfo();
				if (pDrivebyClipInfo)
				{
					if (pDrivebyClipInfo->GetAllowDamageToVehicle() || m_iGunFireFlags.IsFlagSet(GFF_EnableDamageToOwnVehicle)) 
					{
						params.iFireFlags.SetFlag(CWeapon::FF_AllowDamageToVehicle);
					}
					if (pDrivebyClipInfo->GetAllowDamageToVehicleOccupants() || pPed->IsInFirstPersonVehicleCamera())
					{
						params.iFireFlags.SetFlag(CWeapon::FF_AllowDamageToVehicleOccupants);
					}
					if(m_iGunFireFlags.IsFlagSet(GFF_PassThroughOwnVehicleBulletProofGlass))
					{
						params.iFireFlags.SetFlag(CWeapon::FF_PassThroughOwnVehicleBulletProofGlass);
					}
				}
			}
		}

#if USE_TARGET_SEQUENCES
		if (pPed->FindActiveTargetSequence())
		{
			params.iFireFlags.SetFlag(CWeapon::FF_SetPerfectAccuracy);
		}
#endif // USE_TARGET_SEQUENCES

		if (m_iFlags.IsFlagSet(GF_BlindFiring))
		{
			params.iFireFlags.SetFlag(CWeapon::FF_BlindFire);
		}

		if (m_bForceWeaponStateToFire)
		{
			// Force the weapon to be in a state to fire
			pWeapon->SetState(CWeapon::STATE_READY);
			pWeapon->Process(pPed, fwTimer::GetTimeInMilliseconds());
		}

		if(GetClipHelper())
		{
			m_fFireAnimRate = GetFiringAnimPlaybackRate();
			// Set the clip playback rate
			GetClipHelper()->SetRate(m_fFireAnimRate);
		}

		// Store the anim rate for the firing anim
		params.fFireAnimRate = m_fFireAnimRate;

		u32 prevseed = 0;
		if(NetworkInterface::IsGameInProgress())
		{
			if(!pPed->IsNetworkClone())
			{
				//LOCAL - gather information to TaskAimGunOnFoot and TaskAimGunBlindFire + others to replicate.

				//the replay random seed value is used to synchronize the shotgun blast pattern on the local to the remote
				if (pPed->GetEquippedWeaponInfo()->GetGroup() == WEAPONGROUP_SHOTGUN)
				{
					u32 tmpseed = g_ReplayRand.GetSeed();
					if (tmpseed == m_seed)
						m_seed = fwRandom::GetRandomNumber(); //need to reset random with random as seed is the same as last time
					else
						m_seed = tmpseed;

					g_ReplayRand.Reset(m_seed); //IMPORTANT: need to reset the seed here with the seed we will use otherwise the m_seed1 will not be set appropriately on all consoles A/B/C...
				}
				else
				{
					m_seed = 0;
				}
			}
			else
			{
				//REMOTE - place the information sent from the LOCAL before firing so that the firing on the REMOTE is very close to what happened on the LOCAL
				if (m_seed)
				{
					prevseed = g_ReplayRand.GetSeed();
					g_ReplayRand.Reset(m_seed);
				}
			}
		}

		bFired = pWeapon->Fire(params);

		if (pPed->IsNetworkClone() && prevseed)
		{
			g_ReplayRand.Reset(prevseed);
		}
	}
	else
	{
		pVehicleWeapon->Fire( pPed, pPed->GetMyVehicle(), vEnd, m_target.GetEntity() );
		bFired = true;
	}

	if(bFired)
	{
		// Register the event with the firing pattern
		pPed->GetPedIntelligence()->GetFiringPattern().ProcessFired();

		// Perform the VO
		if(!pWeapon->GetIsSilenced())
			DoFireVO(pPed);

		// Reset the time since last fired
		pPed->SetTimeSinceLastShotFired(0.0f);

		// Unset the fire at least one bullet flag
		m_iFlags.ClearFlag(GF_FireAtLeastOnce);

		// B*2174340: Unset the fire flag for the parent task too (only in cover, bit scared to change this for all cases for now).
		if (pPed->GetIsInCover() && GetParent() && GetParent()->GetTaskType() == CTaskTypes::TASK_GUN)
		{
			static_cast<CTaskGun*>(GetParent())->GetGunFlags().ClearFlag(GF_FireAtLeastOnce);
		}

#if USE_TARGET_SEQUENCES
		// inform the player ped targeting that we've fired a shot
		CTargetSequenceHelper* pTargetSequence = pPed->FindActiveTargetSequence();
		if (pTargetSequence)
		{
			pTargetSequence->IncrementShotCount();
		}
#endif // USE_TARGET_SEQUENCES

		//Update the ammo after firing - this value is then reset in the ReadQueriableState - but needs to be updated here with the proper ammo so that is will be correct on the remote for the next firing also.
		if (NetworkInterface::IsGameInProgress())
		{
			u8 actualAmmoInClip = (u8) pWeapon->GetAmmoInClip();

			if (!pPed->IsNetworkClone())
			{
				m_iAmmoInClip = actualAmmoInClip;

				if(GetParent() && GetParent()->GetTaskType() == CTaskTypes::TASK_GUN)
				{
					bool bEmpty = (m_iAmmoInClip == 0);
					static_cast<CTaskGun*>(GetParent())->SetReloadingOnEmpty(bEmpty);
				}
			}
			else if (m_iAmmoInClip < actualAmmoInClip)
			{
				pWeapon->SetAmmoInClip(m_iAmmoInClip);
			}
		}

		m_fireCounter++;
	}

	return bFired;
}

void CTaskAimGun::DoFireVO(CPed* pPed)
{
	if(pPed->IsLocalPlayer() && pPed->GetSpeechAudioEntity())
	{
		if(!m_iFlags.IsFlagSet(GF_BlindFiring))
		{
			const CEntity* pTarget = m_target.GetEntity();
			if(!pTarget)
			{
				pTarget = pPed->GetPlayerInfo()->GetTargeting().GetFreeAimTarget();
			}

			if(pTarget && pTarget->GetIsTypePed())
			{
				static dev_float SPEECH_DIST_SQ = 400.0f;
				if(DistSquared(pPed->GetTransform().GetPosition(), pTarget->GetTransform().GetPosition()).Getf() < SPEECH_DIST_SQ)
				{
					const CPed* pTargetPed = static_cast<const CPed*>(pTarget);
					if(!pTargetPed->GetPedConfigFlag( CPED_CONFIG_FLAG_BlippedByScript ) && !pTargetPed->ShouldBeDead() && 
						pTargetPed->GetPedType() != PEDTYPE_ANIMAL)
					{
						pPed->GetSpeechAudioEntity()->SayWhenSafe("SHOOT");
					}
				}
			}
		}
	}
}

bool CTaskAimGun::ShouldFireBlank(CPed* pPed)
{
	taskAssert(pPed->GetWeaponManager());
	const CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
	taskAssert(pWeapon);

	bool bFireBlank = false;

	// Are we a non player with a non projectile weapon?
	if(!pPed->IsPlayer() && !pWeapon->GetWeaponInfo()->GetIsProjectile())
	{
		// Get the target position
		Vector3 vTargetPos;
		if(!m_target.GetPositionWithFiringOffsets(pPed, vTargetPos))
		{
			// Our position has become invalid
			taskAssertf(0,"Invalid target in CTaskAimGun::ShouldFireBlank");
			return false;
		}

		const float fDistance = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()).Dist(vTargetPos);

		float fChanceOfFiringBlanks = 0.0f;
		bool bReduceBlankChance = true;

		static dev_float SHOTGUN_BLANK_RANGE = 20.0f;
		if(pWeapon->GetWeaponInfo()->GetGroup() == WEAPONGROUP_SHOTGUN && fDistance < SHOTGUN_BLANK_RANGE)
		{
			// Shotguns don't fire blanks within a certain range
			fChanceOfFiringBlanks = 0.0f;
		}
		else if(pPed->GetPedResetFlag(CPED_RESET_FLAG_ShootFromGround))
		{
			TUNE_GROUP_FLOAT(CHANCE_TO_FIRE_BLANKS, fShootFromGroundBlankChance, 0.75f, 0.0f, 1.0f, 0.01f);
			fChanceOfFiringBlanks = fShootFromGroundBlankChance;
		}
		else
		{
			float fChanceToFireBlanksMax = pPed->GetWeaponManager()->GetChanceToFireBlanksMax();

			// Clones should fire more blanks
			if(NetworkInterface::IsGameInProgress() && pPed->IsNetworkClone())
			{
				TUNE_GROUP_FLOAT(CHANCE_TO_FIRE_BLANKS, fCloneChanceToFireBlanksOffScreen, 0.75f, 0.0f, 1.0f, 0.01f);
				fChanceToFireBlanksMax = Max(fCloneChanceToFireBlanksOffScreen, fChanceToFireBlanksMax);
			}
			// Increase the amount of blanks fired by off screen peds
			else if(!pPed->GetIsVisibleInSomeViewportThisFrame() || fDistance > 30.f)
			{
				TUNE_GROUP_FLOAT(CHANCE_TO_FIRE_BLANKS, fChanceToFireBlanksOffScreen, 0.75f, 0.0f, 1.0f, 0.01f);
				fChanceToFireBlanksMax = Max(fChanceToFireBlanksOffScreen, fChanceToFireBlanksMax);
				bReduceBlankChance = false;
			}

			// Reduce the chance of firing blanks when closer to the target
			TUNE_GROUP_FLOAT(CHANCE_TO_FIRE_BLANKS, MIN_DIST_TO_FIRE_BLANKS, 5.0f, 0.0f, 100.0f, 0.01f);
			TUNE_GROUP_FLOAT(CHANCE_TO_FIRE_BLANKS, MAX_DIST_TO_END_BLANK_SCALE, 15.0f, 0.0f, 100.0f, 0.01f);
			fChanceOfFiringBlanks = pPed->GetWeaponManager()->GetChanceToFireBlanksMin() + 
				(Clamp((fDistance - MIN_DIST_TO_FIRE_BLANKS) / (MAX_DIST_TO_END_BLANK_SCALE - MIN_DIST_TO_FIRE_BLANKS), 0.0f, 1.0f) * (fChanceToFireBlanksMax - pPed->GetWeaponManager()->GetChanceToFireBlanksMin()));
		}

		// Also reduce the chance of firing blanks after having fired at least one if it's been specified
		if(bReduceBlankChance && m_iBlanksFired > 0)
		{
			fChanceOfFiringBlanks *= 1.0f / float(m_iBlanksFired + 1);
		}

		bFireBlank = fwRandom::GetRandomNumberInRange(0.0f, 1.0f) < fChanceOfFiringBlanks;

		// Record whether the ped fired blanks so it affects later probabilities
		if(bFireBlank)
		{
			m_iBlanksFired++;
		}
		else
		{
			m_iBlanksFired = 0;
		}
	}

	return bFireBlank;
}

#if __BANK
void CTaskAimGun::InitWidgets()
{
	bkBank* pBank = BANKMGR.FindBank("A.I.");
	if (pBank)
	{
		pBank->PushGroup("Gun Tasks", false);

		pBank->AddSeparator();

		pBank->AddButton("Set go to position", SetGoToPosCB);
		pBank->AddButton("Set aim at entity", SetAimAtEntityCB);
		pBank->AddButton("Set tasked entity", SetTaskedEntityCB);
		pBank->AddButton("Give focus ped goto point aiming task", GivePedTaskCB);

		pBank->AddToggle("Use left hand Ik",	&ms_bUseLeftHandIk);
		pBank->AddToggle("Use torso IK", &ms_bUseTorsoIk);

		pBank->AddToggle("Use overriden yaw",	&ms_bUseOverridenYaw);
		pBank->AddToggle("Use overriden pitch", &ms_bUseOverridenPitch);
		pBank->AddSlider("Overriden Yaw",		&ms_fOverridenYaw, -270.0f, 270.0f, 1.0f);
		pBank->AddSlider("Overriden Pitch",		&ms_fOverridenPitch, -90.0f, 90.0f, 1.0f);
		pBank->AddToggle("Use alt grip clipset", &ms_bUseAltClipSetHash);
		pBank->AddToggle("Use intersection test", &ms_bUseIntersectionPos);

		pBank->AddToggle("Render test positions",	&ms_bRenderTestPositions);
		pBank->AddToggle("Render Yaw Debug",		&ms_bRenderYawDebug);
		pBank->AddToggle("Render Pitch Debug",		&ms_bRenderPitchDebug);
		pBank->AddToggle("Render Debug Text",		&ms_bRenderDebugText);

		pBank->AddSlider("Debug Arc Radius",		&ms_fDebugArcRadius, 0.0f, 4.0f, 0.1f);

		pBank->AddSeparator();

		//CTaskAimGunStanding::InitWidgets(*pBank);
		//CTaskAimGunStrafe::InitWidgets(*pBank);
		CTaskAimGunVehicleDriveBy::InitWidgets(*pBank);

		pBank->PopGroup();
	}
}
#endif // __BANK
