
#include "vehicles/DraftVehicle.h"
 
// Rage Headers
//#include "phCore/segment.h"
//#include "phCore/isect.h"
#include "physics/constrainthandle.h"
#include "physics/constraintmgr.h"
#include "physics/constraintspherical.h"
#include "physics/constraintrotation.h"
#include "physics\constraintcylindrical.h"
#include "physics\constraintdistance.h"
#include "physics/simulator.h"

// Game Headers
#include "camera/CamInterface.h"
#include "peds/pedfactory.h"
#include "peds\pedIntelligence.h"
//#include "renderer\hierarchyIds.h"
#include "scene\world\GameWorld.h"
#include "streaming\streaming.h"
#include "Task/Vehicle/TaskDraftHorse.h"
#include "Peds/Horse/horseComponent.h"
//#include "Task/Vehicle/TaskRideHorse.h"
#include "vehicles\handlingMgr.h"
//#include "vehicles\vehicle.h"

VEHICLE_OPTIMISATIONS();

//////////////////////////////////////////////////////////////////////////
#define USE_NEW_HORSE_TASK 1
#define USE_CAR_CONTROLS_FOR_HORSES 1

CDraftVeh::CDraftVeh(const eEntityOwnedBy ownedBy, u32 popType) : CAutomobile(ownedBy, popType, VEHICLE_TYPE_DRAFT)
{
		for(int i = 0; i < MAX_DRAFT_ANIMALS; i++)
		{
				m_pDraftAnimals[i] = NULL;
		}
		m_bDraftAnimalMatrixValid = false;
		m_fThrottleRunningAverage = 0.0f;
}

CDraftVeh::~CDraftVeh()
{
	DeleteDraftAnimals();
}

int CDraftVeh::InitPhys()
{
		CAutomobile::InitPhys();

	//m_nVehicleFlags.bAnimateWheels = true;
	m_nVehicleFlags.bUseDeformation = false;
	return INIT_OK;
}

void CDraftVeh::InitWheels()
{
	CAutomobile::InitWheels();

	// Tell (front)wheel bones to stay up to date with any articulated motion
	for(int i = 0; i < GetNumWheels(); i++)
	{
		Assert(GetWheel(i));
		fwFlags32& wheelFlags = GetWheel(i)->GetConfigFlags();
		if( !wheelFlags.IsFlagSet(WCF_REARWHEEL) )
		{
			wheelFlags.SetFlag(WCF_UPDATE_SUSPENSION);
			wheelFlags.SetFlag(WCF_IS_PHYSICAL);
		}
	}
}

void CDraftVeh::AddPhysics()
{
		CAutomobile::AddPhysics();

		// We do this pretty late in the vehicle setup to make sure the skeleton is initialized
		//  and specifically after AddPhysics to make sure the fragment and entity matrix are synced up
		fwModelId draftAnimalType = CModelInfo::GetModelIdFromName("A_C_HORSELOWLANDS_01"); // TODO - Pull animal type from some vehicle option (script or the veh factory?)
		SpawnDraftAnimals(draftAnimalType, true);
}

void CDraftVeh::SetPosition(const Vector3& vec, bool bUpdateGameWorld, bool bUpdatePhysics, bool bWarp)
{
	CAutomobile::SetPosition(vec, bUpdateGameWorld, bUpdatePhysics, bWarp);

	//Warp the drafts if needed
	if (bWarp)
	{
		if(GetSkeleton() != NULL)
		{
			WaitForAnyActiveAnimUpdateToComplete();

			// Try a skeleton reset
			GetSkeleton()->Reset();
		}

		SnapDraftsToPosition();
	}
}

void CDraftVeh::SetHeading(float fHeading)
{
	CAutomobile::SetHeading(fHeading);

	if(GetSkeleton() != NULL)
	{
		WaitForAnyActiveAnimUpdateToComplete();

		// Try a skeleton reset
		GetSkeleton()->Reset();
	}

	//And get the drafts in place
	SnapDraftsToPosition();
}

void CDraftVeh::SnapDraftsToPosition()
{
	// Update draft animal positions to the new spot as if we were just spawned there
	for(int draftIndex = 0; draftIndex < MAX_DRAFT_ANIMALS; draftIndex++)
	{
		if(m_pDraftAnimals[draftIndex] == NULL)
		{
			continue;
		}

		Matrix34 matMyBone(Matrix34::IdentityType);
		GetGlobalMtx(m_iDraftAnimalBoneIndices[draftIndex], matMyBone);
		Vector3 offsetVec(0.0f, 0.0f, 0.85f); // HACK - from the spawn function
		matMyBone.Transform3x3(offsetVec);
		matMyBone.GetVector(3) = matMyBone.GetVector(3) + offsetVec;

		m_pDraftAnimals[draftIndex]->SetMatrix(matMyBone, true, true, true);
	}

	//Update position so that we can retask correctly
	UpdateDraftAnimalMatrix();
}

void CDraftVeh::SetMatrix(const Matrix34& mat, bool bUpdateGameWorld, bool bUpdatePhysics, bool bWarp)
{
	CAutomobile::SetMatrix(mat, bUpdateGameWorld, bUpdatePhysics, bWarp);
	
	static bool onlyMoveHorsesWithWarp = false;
	if(bWarp || !onlyMoveHorsesWithWarp)
	{
		SnapDraftsToPosition();
	}
}

bool CDraftVeh::ProcessShouldFindImpactsWithPhysical(const phInst* otherInst) const
{
	for(int draftIndex = 0; draftIndex < MAX_DRAFT_ANIMALS; draftIndex++)
	{
		if(m_pDraftAnimals[draftIndex] == NULL)
		{
			continue;
		}

		// Never collide with our attached animals
		if( otherInst == m_pDraftAnimals[draftIndex]->GetFragInst() || otherInst == m_pDraftAnimals[draftIndex]->GetAnimatedInst() )
		{
			return false;
		}
	}

	return CAutomobile::ProcessShouldFindImpactsWithPhysical(otherInst);
}

void CDraftVeh::DoProcessControl(bool fullUpdate, float fFullUpdateTimeStep)
{
	// Attempt secondary draft animal spawning if we failed earlier
	if(!m_DraftTypeRequest.HasLoaded())
	{
		SpawnDraftAnimals( fwModelId(m_DraftTypeRequest.GetRequestId()) );
	}

	CAutomobile::DoProcessControl(fullUpdate, fFullUpdateTimeStep);

	// The above processes the newest player/AI inputs which is what we want to work with
	DriveDraftAnimals();
	UpdateDraftAnimalMatrix();
}

// Might want our own -- This is from vehicles\trailer.cpp
extern void EnforceConstraintLimits(phConstraintRotation* pConstraint, float fMin, float fMax, bool bIsPitch);

void CDraftVeh::SpawnDraftAnimals(const fwModelId& ENABLE_HORSE_ONLY(modelId), bool ENABLE_HORSE_ONLY(firstAttempt))
{
#if ENABLE_HORSE
	if(modelId.GetModelIndex() == fwModelId::MI_INVALID)
	{
			return;
	}

	// Try to get the draft animal model loaded
	if(firstAttempt)
	{
		// Make one first blocking attempt to load the model
		if (!CModelInfo::HaveAssetsLoaded(modelId))
		{
				CModelInfo::RequestAssets(modelId, STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
				CStreaming::LoadAllRequestedObjects(true);
		}

		// If that failed we fall back to a polling asynchronous attempt
		if(!CModelInfo::HaveAssetsLoaded(modelId))
		{
			m_DraftTypeRequest.Request(modelId.ConvertToStreamingIndex(), CModelInfo::GetStreamingModuleId());
			return;
		}
	}
	else
	{
		// For as long as we haven't loaded the thing we'll hang out here looking for it
		if(!m_DraftTypeRequest.HasLoaded())
		{
			return;
		}

		// We're now spawning at some indeterminate time after the initial attempt so the skeleton could be offset
		// This would cause the draft animals to bias the steering to one side or the other
		//  We end up having to reset a whole bunch of stuff to prevent this
		//  -- Alternative could be to freeze the vehicle in initial spawning position and only release once horses are spawned, but that seems lame
		WaitForAnyActiveAnimUpdateToComplete();

		crSkeleton* pSkeleton = GetSkeleton();
		Assert(pSkeleton != NULL);
		pSkeleton->Reset();
		pSkeleton->Update();

		fragInst* pFrag = GetFragInst();
		Assert(pFrag != NULL);
		pFrag->PoseBoundsFromSkeleton(false, false);
		pFrag->PoseArticulatedBodyFromBounds();

		phArticulatedBody* pBody = GetFragInst()->GetArticulatedBody();
		Assert(pBody != NULL);
		pBody->Freeze();
		pBody->CalculateInertias();
	}

	// Inverse masses don't seem to behave well with the horse systems - no idea why yet
	// -- Presumably they don't like not getting their way
	static float yawConstraintLimit = 12.5f;
	static float yawConstraintInvMassScale_Veh = 1.0f;
	static float yawConstraintInvMassScale_Ped = 1.0f;
	static float rollConstraintLimit = 48.0f;
	static float rollConstraintInvMassScale_Veh = 1.0f;
	static float rollConstraintInvMassScale_Ped = 1.0f;
	//
	int boneIndex[4];
	boneIndex[0] = GetBoneIndex(DRAFTVEH_ANIMAL_ATTACH_LR);
	boneIndex[1] = GetBoneIndex(DRAFTVEH_ANIMAL_ATTACH_RR);
	boneIndex[2] = GetBoneIndex(DRAFTVEH_ANIMAL_ATTACH_LF);
	boneIndex[3] = GetBoneIndex(DRAFTVEH_ANIMAL_ATTACH_RF);

	static int constraintType = 1;
	static float distConstraintMin = 0.0f;
	static float distConstraintMax = 0.01f;
	static float distConstraintAllowedPen = 0.2f;

	static bool useNewBoneIndices = false;
	static int newBoneIndex[4] = { 24, 27, 25, 26 };
	static int indexStart = 0;
	static int indexEnd = 4;

	// Nasty hacks that shouldn't exist -- need to define a standard for bone placement
	// -- IMO most extensible for future different animals is placement on the ground modified by height and width params on the animal peds themselves
	static Vector3 attachOffset[4] = { Vector3(0.0f, 0.0f, 0.85f), Vector3(0.0f, 0.0f, 0.85f), Vector3(0.0f, 0.0f, 0.85f), Vector3(0.0f, 0.0f, 0.85f) };
	int draftIndex = 0;
	for(int i = indexStart; i < indexEnd; i++)
	{
		int iThisBoneIndex = useNewBoneIndices ? newBoneIndex[i] : boneIndex[i];
		if(iThisBoneIndex == -1)
		{
				continue;
		}

		// Figure out full bone matrix in world space
		Matrix34 matMyBone(Matrix34::IdentityType);
		GetGlobalMtx(iThisBoneIndex, matMyBone);

		Matrix34 TempMat = matMyBone;
		TempMat.Set(matMyBone.GetVector(0), matMyBone.GetVector(1), matMyBone.GetVector(2), matMyBone.GetVector(3));
		Vector3 offsetVec = attachOffset[i];
		TempMat.Transform3x3(offsetVec);
		TempMat.GetVector(3) = TempMat.GetVector(3) + offsetVec;

		const CControlledByInfo localAiControl(false, false);
		CPed* ped = CPedFactory::GetFactory()->CreatePed(localAiControl, modelId, &TempMat, true, true, false);
		if(ped)
		{
			CGameWorld::Add(ped, CGameWorld::OUTSIDE);
			//ped->SetPedConfigFlag(CPED_CONFIG_FLAG_CannotBeMounted, true);
			int nMyComponent = GetVehicleFragInst()->GetControllingComponentFromBoneIndex(iThisBoneIndex);
			if(nMyComponent < 0)
					nMyComponent = 0;

			static int nEntComponent = 0;

			if(constraintType == 0)
			{
				phConstraintSpherical::Params posConstraintParams;
				posConstraintParams.worldPosition = VECTOR3_TO_VEC3V(TempMat.d);
				posConstraintParams.instanceA = GetVehicleFragInst();
				posConstraintParams.instanceB = ped->GetAnimatedInst();
				posConstraintParams.componentA = (u16)nMyComponent;
				posConstraintParams.componentB = (u16)nEntComponent;

				/*m_posConstraintHandle = */PHCONSTRAINT->Insert(posConstraintParams);
				//Assertf(m_posConstraintHandle.IsValid(), "Failed to create draft constraint");
			}
			else if(constraintType == 1)
			{
				// This one seems nicer since it gives some apparent stretching and motion between draft animal and cart when starting/stopping
				phConstraintDistance::Params posConstraintParams;
				posConstraintParams.worldAnchorA = VECTOR3_TO_VEC3V(TempMat.d);
				posConstraintParams.worldAnchorB = VECTOR3_TO_VEC3V(TempMat.d);
				posConstraintParams.instanceA = GetVehicleFragInst();
				posConstraintParams.instanceB = ped->GetAnimatedInst();
				posConstraintParams.componentA = (u16)nMyComponent;
				posConstraintParams.componentB = (u16)nEntComponent;
				posConstraintParams.minDistance = distConstraintMin;
				posConstraintParams.maxDistance = distConstraintMax;
				posConstraintParams.allowedPenetration = distConstraintAllowedPen;
				// Should probably hang onto a handle so we can delete/control/query later
				PHCONSTRAINT->Insert(posConstraintParams);
			}

			// Yaw constraint
			if(yawConstraintLimit >= 0.0f)
			{
				static dev_float sfYawLimit = DtoR*(yawConstraintLimit);
				//Vec3V transformAxisB = ped->GetAnimatedInst()->GetMatrix().GetCol1(); // ped->GetAnimatedInst()->GetTransform().GetB()
				//float fCurrentYaw = VEC3V_TO_VECTOR3(transformAxisB).AngleZ(VEC3V_TO_VECTOR3(GetTransform().GetB()));

				phConstraintRotation::Params rotZConstraintParams;
				rotZConstraintParams.instanceA = ped->GetAnimatedInst();
				rotZConstraintParams.instanceB = GetVehicleFragInst();
				rotZConstraintParams.componentA = (u16)nEntComponent;
				rotZConstraintParams.componentB = (u16)nMyComponent;
				rotZConstraintParams.worldAxis = GetTransform().GetC();
				rotZConstraintParams.massInvScaleA = yawConstraintInvMassScale_Ped;
				rotZConstraintParams.massInvScaleB = yawConstraintInvMassScale_Veh;

				phConstraintHandle tempConstraintHandle = PHCONSTRAINT->Insert(rotZConstraintParams);
				phConstraintRotation* pRotConstraintZ = static_cast<phConstraintRotation*>( PHCONSTRAINT->GetTemporaryPointer( tempConstraintHandle ) );
				if(Verifyf(pRotConstraintZ, "Failed to create yaw constraint"))
				{
						EnforceConstraintLimits(pRotConstraintZ, -sfYawLimit, sfYawLimit, false);
				}
			}

			// Roll constraint
			if(rollConstraintLimit >= 0.0f)
			{
				static dev_float sfRollLimit = DtoR*(rollConstraintLimit);
				//Vec3V transformAxisA = ped->GetAnimatedInst()->GetMatrix().GetCol0(); // ped->GetAnimatedInst()->GetTransform().GetB()
				//float fCurrentRoll = VEC3V_TO_VECTOR3(transformAxisA).AngleY(VEC3V_TO_VECTOR3(GetTransform().GetA()));

				phConstraintRotation::Params rotYConstraintParams;
				rotYConstraintParams.instanceA = ped->GetAnimatedInst();
				rotYConstraintParams.instanceB = GetVehicleFragInst();
				rotYConstraintParams.componentA = (u16)nEntComponent;
				rotYConstraintParams.componentB = (u16)nMyComponent;
				rotYConstraintParams.worldAxis = GetTransform().GetB();
				rotYConstraintParams.massInvScaleA = rollConstraintInvMassScale_Ped;
				rotYConstraintParams.massInvScaleB = rollConstraintInvMassScale_Veh;

				phConstraintHandle tempConstraintHandle = PHCONSTRAINT->Insert(rotYConstraintParams);
				phConstraintRotation* pRotConstraintY = static_cast<phConstraintRotation*>( PHCONSTRAINT->GetTemporaryPointer( tempConstraintHandle ) );
				if(Verifyf(pRotConstraintY, "Failed to create roll constraint"))
				{
						EnforceConstraintLimits(pRotConstraintY, -sfRollLimit, sfRollLimit, false);
				}
			}

			// Task draft animal to follow orders
#if USE_NEW_HORSE_TASK
			ped->GetPedIntelligence()->GetTaskManager()->SetTask(PED_TASK_TREE_MOVEMENT, rage_new CTaskMoveDraftAnimal(), PED_TASK_MOVEMENT_DEFAULT);
#else
			ped->GetPedIntelligence()->GetTaskManager()->SetTask(PED_TASK_TREE_MOVEMENT, rage_new CTaskMoveMounted(), PED_TASK_MOVEMENT_DEFAULT);
#endif

			m_pDraftAnimals[draftIndex] = ped;
			m_iDraftAnimalBoneIndices[draftIndex] = iThisBoneIndex;
			m_fDraftAnimalCurSteer[draftIndex] = 0.0f;
			draftIndex++;
		}
	}
#else
	Assert(0);
#endif
}

void CDraftVeh::DeleteDraftAnimals()
{
	for(int draftIndex = 0; draftIndex < MAX_DRAFT_ANIMALS; draftIndex++)
	{
		if(m_pDraftAnimals[draftIndex] == NULL)
		{
			continue;
		}

		// Shouldn't have to worry about the constraints attached
		//  They will clean themselves up once they notice nothing on one side
		CPedFactory::GetFactory()->DestroyPed(m_pDraftAnimals[draftIndex]);
		m_pDraftAnimals[draftIndex] = NULL;
	}
}

void CDraftVeh::DriveDraftAnimals()
{
#if ENABLE_HORSE
	// Want to move any of these that matter to handling data (specific to draft vehicles)
	static bool useThrottleStaging = true;
	static float throttleStage1 = 0.2f;
	static float throttleStage2 = 0.7f;
	static float throttleStage3 = 1.0f;
	static float brakingThreshold = 0.21f;// We really can't allow any actual braking until we're already nearly stopped
	static float brakingMin = 0.0f;
	static float brakeHardThreshold = 0.8f;
	static float brakeScalar = 24.0f;
	static float throttleContributionPerSec = 0.9f;
	static float throttleDecayRatePerSec = 1.2f;
	static float throttleScalar = 0.8f;
	static float throttleScalarReverse = 0.0f;
	static float throttleDeadZone = 0.1f;
	static float throttleFullThreshold = 0.5f;
	static float throttleNotFullScalar = 0.4f;
	static float throttleSteerDamping = 1.8f;
	static float throttleSteerScalar = 3.4f;
	static float throttleSteerBoost = 1.0f;
	static float throttleSteerBoostDamping = 1.5f;
	static float throttleOverSteerBoost = 0.3f;
	static float throttleOverSteerBoostDamping = 0.375f;
	static float steerChangePerSecond = 12.0f;//2.8f;
	static float extraSteerChangePerSecondForAI = 6.0f;
	static float maxOverSteer = 0.35f;
	static float steeringDeadzone = 0.1f;
	static float steerScalar = 24.0f;//1.7f;
	static bool bUseCameraRelative = false;

	const float frameTimeInSeconds = TIME.GetSeconds();
	const bool driverIsPlayer = GetDriver() != NULL ? GetDriver()->IsLocalPlayer() : false;

	// Basic car input struct (AI should be capable of controlling via these as well as the player)
	float steerAngle = m_vehControls.GetSteerAngle();
	float throttleVal = m_vehControls.GetThrottle();
	float brakeVal = m_vehControls.GetBrake();
	//bool handBrakeOn = m_vehControls.GetHandBrake();
	brakeVal = brakeVal > brakingMin ? brakeVal - brakingMin : 0.0f;
	bool braking = brakeVal > 0.0f;
	bool brakingHard = brakeVal > brakeHardThreshold;

	// Discretize the analog input to specific steps to make targeting a gait easier for players
	// - Seems fine, but are there any negative effects for AI drivers with this running?
	if(useThrottleStaging)
	{
		if(throttleVal > 0.0f)
		{
			if(throttleVal <= throttleStage1)
			{
				throttleVal = throttleStage1;
			}
			else if(throttleVal <= throttleStage2)
			{
				throttleVal = throttleStage2;
			}
			else
			{
				throttleVal = throttleStage3;
			}
		}
	}

	// Throttle interpretation
	bool reversing = false;
	if(throttleVal < 0.0f)
	{
		reversing = true;
		throttleVal *= throttleScalarReverse;
	}
	else
	{
		throttleVal *= throttleScalar;
	}

	// Mess with throttle to add some apparent inertia
	float adjustedThrottleContributionPerSec = throttleContributionPerSec;
	// Scaling up goes linearly, but we would like to decay much slower (But only if we're coasting, braking should still fall off quickly)
	if(throttleVal < m_fThrottleRunningAverage && !brakingHard)
	{
		float adjustedThrottleDecayRatePerSec = throttleDecayRatePerSec;
		if(braking)
		{
			adjustedThrottleDecayRatePerSec += (brakeVal*brakeVal) * brakeScalar;
		}
		float newScale = Clamp(adjustedThrottleDecayRatePerSec * frameTimeInSeconds, 0.0f, 2.0f);
		float oldScale = 2.0f - newScale;
		throttleVal = ((throttleVal*newScale) + (m_fThrottleRunningAverage*oldScale)) / (2.0f);

		//adjustedThrottleContributionPerSec += brakeVal * brakeScalar;
	}
	float scaleT = Min(adjustedThrottleContributionPerSec * frameTimeInSeconds, 1.0f);
	m_fThrottleRunningAverage = m_fThrottleRunningAverage + (scaleT*(throttleVal - m_fThrottleRunningAverage));
	throttleVal = m_fThrottleRunningAverage;

	float absThrottle = Abs(throttleVal);
	bool inThrottleDeadZone = (absThrottle <= throttleDeadZone);
	if(absThrottle < throttleFullThreshold)
	{
		absThrottle = absThrottle * throttleNotFullScalar;
	}

	//
	if(braking)
	{
		// Don't allow actual braking to kick in until we've slowed enough
		if(throttleVal > brakingThreshold)
		{
			brakeVal = 0.0f;
			braking = false;
		}
		else
		{
			// But if we have, we should immediately zero out our throttle
			throttleVal = 0.0f;
			m_fThrottleRunningAverage = 0.0f;
		}
	}

	// Steering interpretation
	float adjustedSteerChangePerSecond = steerChangePerSecond + (throttleSteerScalar * Max(1.0f-(absThrottle*throttleSteerDamping), 0.0f) );
	if(!driverIsPlayer)
	{
		adjustedSteerChangePerSecond += extraSteerChangePerSecondForAI;
	}
	float maxSteerChange = adjustedSteerChangePerSecond * frameTimeInSeconds;

	bool inSteeringDeadzone = (Abs(steerAngle) <= steeringDeadzone);
	float additionalSteer = Max(throttleSteerBoost-(absThrottle*throttleSteerBoostDamping), 0.0f);
	steerAngle = steerAngle >= 0.0f ? steerAngle + additionalSteer : steerAngle - additionalSteer;
	steerAngle *= steerScalar;
	steerAngle = Clamp(steerAngle * frameTimeInSeconds, -maxSteerChange, maxSteerChange);

	float maxTotalSteer = maxOverSteer + Max(throttleOverSteerBoost-(absThrottle*throttleOverSteerBoostDamping), 0.0f);

	CPed* pPlayerPed = GetDriver();
	float fBaseOrient = 0.0f;
	if(bUseCameraRelative && pPlayerPed != NULL)
	{
		fBaseOrient = camInterface::GetPlayerControlCamHeading(*pPlayerPed);
	}

	for(int draftIndex = 0; draftIndex < MAX_DRAFT_ANIMALS; draftIndex++)
	{
		if(m_pDraftAnimals[draftIndex] == NULL)
		{
			continue;
		}

		CPed* myMount = m_pDraftAnimals[draftIndex];
		CTask* horseMoveTask = myMount->GetPedIntelligence()->GetDefaultMovementTask();
		if (horseMoveTask->GetTaskTypeInternal() == CTaskTypes::TASK_MOVE_DRAFT_ANIMAL)
		{
			CTaskMoveDraftAnimal* moveTask = static_cast<CTaskMoveDraftAnimal*>(horseMoveTask);

			// What is the stick input even for?
			Vector2 vStick(0.0f, 0.0f);
		//	float curDesiredHeading = moveTask->GetDesiredHeading();
			float curHeading = myMount->GetCurrentHeading();
			float curSteer = m_fDraftAnimalCurSteer[draftIndex];
			float newSteer;

			float desiredHeading;
			if(bUseCameraRelative)
			{
				desiredHeading = fBaseOrient;
			}
			else
			{
				desiredHeading = curHeading;
			}

			float newHeading;
			if(inSteeringDeadzone) // Would like to disable steering while stationary
			{
				newSteer = curSteer >= 0.0f ? Max(curSteer - maxSteerChange, 0.0f) : Min(curSteer + maxSteerChange, 0.0f);
				//desiredHeading += newSteer;
				newHeading = fwAngle::LimitRadianAngle( desiredHeading );
			}
			else
			{
				newSteer = Clamp(curSteer + steerAngle, -maxTotalSteer, maxTotalSteer);
				desiredHeading += steerAngle;//newSteer;
				newHeading = fwAngle::LimitRadianAngle( desiredHeading );
			}
			m_fDraftAnimalCurSteer[draftIndex] = newSteer;

			//////////////////////////////////////////////////////////////////////////
			// Steering changes to following if there is a draft animal in front of you
			const int draftIndexInFront = draftIndex + 2;
			if(draftIndexInFront < MAX_DRAFT_ANIMALS && m_pDraftAnimals[draftIndexInFront] != NULL)
			{
				CPed* myMount = m_pDraftAnimals[draftIndex];
				CPed* myLeader = m_pDraftAnimals[draftIndexInFront];

				const Vec3V curPos = myMount->GetMatrix().GetCol3();
				const Vec3V targetPos = myLeader->GetMatrix().GetCol3();

				const float targetHeading = fwAngle::GetRadianAngleBetweenPoints(targetPos.GetXf(), targetPos.GetYf(), curPos.GetXf(), curPos.GetYf());
				newHeading = fwAngle::LimitRadianAngle( targetHeading );
			}
			//////////////////////////////////////////////////////////////////////////

			// Want better control over reversing behavior/direction
			moveTask->ApplyInput(newHeading, false, false, brakeVal, !inThrottleDeadZone, false, vStick, absThrottle);
			if(reversing)
			{
				moveTask->RequestReverse();
			}
		}
	}
#else
	Assert(0);
#endif
}

void CDraftVeh::UpdateDraftAnimalMatrix()
{
	m_bDraftAnimalMatrixValid = false;

	Mat34V MatSum(V_ZERO);
	ScalarV numAdded(V_ZERO);
	for(int draftIndex = 0; draftIndex < MAX_DRAFT_ANIMALS; draftIndex++)
	{
		if(m_pDraftAnimals[draftIndex] == NULL)
		{
			continue;
		}

		rage::Add(MatSum, MatSum, m_pDraftAnimals[draftIndex]->GetMatrix());
		numAdded = rage::Add(numAdded, ScalarV(V_ONE));

		m_bDraftAnimalMatrixValid = true;
	}

	InvScale(m_AveragedDraftAnimalMatrix, MatSum, Vec3V(numAdded));
}

CPed* CDraftVeh::GetDraftAnimalAtHarnessIndex(eDraftHarnessIndex harnessIndex) const
{
	Assert(harnessIndex >= 0);
	Assert(harnessIndex < DRAFT_HARNESS_COUNT);

	return m_pDraftAnimals[harnessIndex];
}

bool CDraftVeh::IsPedDrafted(CPed* ped) const
{
	for(int i = 0; i < MAX_DRAFT_ANIMALS; i++)
	{
		if(m_pDraftAnimals[i].Get() == ped)
		{
			return true;
		}
	}

	return false;
}

// Use the average of attached draft animals if possible
// Since they much more strongly define where we're headed
// than the vehicle's own matrix
Vec3V_Out CDraftVeh::GetVehiclePositionForDriving() const
{
	if(m_bDraftAnimalMatrixValid)
	{
		return m_AveragedDraftAnimalMatrix.GetCol3();
	}
	else
	{
		return GetMatrix().GetCol3();
	}
}

Vec3V_Out CDraftVeh::GetVehicleForwardDirectionForDriving() const
{
	if(m_bDraftAnimalMatrixValid)
	{
		return m_AveragedDraftAnimalMatrix.GetCol1();
	}
	else
	{
		return GetMatrix().GetCol1();
	}
}

Vec3V_Out CDraftVeh::GetVehicleRightDirectionForDriving() const
{
	if(m_bDraftAnimalMatrixValid)
	{
		return m_AveragedDraftAnimalMatrix.GetCol0();
	}
	else
	{
		return GetMatrix().GetCol0();
	}
}

Vec3V_ConstRef CDraftVeh::GetVehicleForwardDirectionForDrivingRef() const
{
	if(m_bDraftAnimalMatrixValid)
	{
		return m_AveragedDraftAnimalMatrix.GetCol1ConstRef();
	}
	else
	{
		return GetMatrixRef().GetCol1ConstRef();
	}
}

