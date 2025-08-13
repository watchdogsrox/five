#include "vehicles/trailer.h"
 
// rage
#include "Fragment/Instance.h"
#include "phArticulated\ArticulatedCollider.h"
#include "phBound/boundcomposite.h"
#include "physics/constraintmgr.h"
#include "physics/constraintdistance.h"
#include "physics/constraintspherical.h"
#include "physics/constraintrotation.h"

// framework
#include "fwmaths/vectorutil.h"

// game
#include "animation/AnimBones.h"
#include "network/events/NetworkEventTypes.h"
#include "physics/gtainst.h"
#include "physics/physics.h"
#include "vehicles/Bike.h"
#include "vehicles/vehicle.h"
#include "vehicles/VehicleFactory.h"
#include "vehicles/wheel.h"
#include "vehicleAi/VehicleIntelligence.h"
#include "control/record.h"
#include "game/modelIndices.h"

VEHICLE_OPTIMISATIONS();

const Vector3 CTrailer::sm_offsets[MAX_CARGO_VEHICLES] = 
{
	Vector3(0.0f, 0.0f, 1.5f),	// low middle (as viewed when trailer forward points left in screen space)
	Vector3(0.0f, 0.0f, 3.5f),	// top middle
	Vector3(0.0f,-4.8f, 1.5f),	// low left
	Vector3(0.0f,-4.9f, 3.5f),	// top left
	Vector3(0.0f, 4.8f, 1.5f),	// low right
	Vector3(0.0f, 4.8f, 3.5f)	// top right
};

//////////////////////////////////////////////////////////////////////////
// CTrailerLegs

CTrailerLegs::CTrailerLegs()
{
	m_iBoneIndex = -1;
	m_iFragChild = -1;

	m_fCurrentRatio = m_fTargetRatio = 0.999f;
    m_fDisplaceDistance = 0.0f;
}

CTrailerLegs::CTrailerLegs(int iBoneIndex, CVehicle *pParent)
{
	Init(iBoneIndex, pParent);
}

void CTrailerLegs::Init(int iBoneIndex, CVehicle* pParent)
{
	Assert(pParent);
	Assert(pParent->GetVehicleFragInst());
	Assert(iBoneIndex > -1);

	m_iFragChild = pParent->GetVehicleFragInst()->GetComponentFromBoneIndex(iBoneIndex);
	m_iBoneIndex = iBoneIndex;
}

static dev_float sfLegDeploySpeed = 3.0f;
void CTrailerLegs::ProcessPhysics(CVehicle* pParent, const float fTimestep, const int UNUSED_PARAM(nTimeslice))
{
	// first progress current ratio to desired
	float fDiff = m_fTargetRatio - m_fCurrentRatio;
	if(fDiff == 0.0f)
	{
		return;
	}

	m_fCurrentRatio += Clamp(fDiff, -sfLegDeploySpeed * fTimestep, sfLegDeploySpeed * fTimestep);
	m_fCurrentRatio = Clamp(m_fCurrentRatio, 0.0f, 1.0f);

	UpdateToCurrentRatio(*pParent);
}

void CTrailerLegs::UpdateToCurrentRatio(CVehicle & parentTrailer)
{
	if(m_iBoneIndex == -1 || m_iFragChild == -1)
	{
		// No collision
		return;
	}

	// Need to be awake to progress, because we need an articulated collider to adjust link attachment matrices
	if(parentTrailer.IsAsleep())
	{
		parentTrailer.ActivatePhysics();
	}

	// We move the legs up by their bounding box value
	// At 1.0f they are at authored position

	// Find the authored link attach position of the group
	const fragInst* pFragInst = parentTrailer.GetVehicleFragInst();
	Assert(pFragInst);

	// Move up by z extents of this component part
	Assert(pFragInst->GetCached());
	const phBoundComposite* pOriginalComposite = pFragInst->GetTypePhysics()->GetCompositeBounds();
	phBoundComposite* pInstanceComposite = pFragInst->GetCacheEntry()->GetBound();
	phBound* pChildBound = pInstanceComposite->GetBound(m_iFragChild);
	Matrix34 matLegBound = RCC_MATRIX34(pOriginalComposite->GetCurrentMatrix(m_iFragChild));


	float fDisplaceDist = pChildBound->GetBoundingBoxMax().GetZf() - pChildBound->GetBoundingBoxMin().GetZf();

	// Add extra bit on until art is fixed
	static dev_float sfAdditionalLegExtend = 0.2f;
	static dev_float sfAdditionalLegExtendForTrailerLarge = 0.4f;

	float additionalLegExtend = sfAdditionalLegExtend;

	if( MI_TRAILER_TRAILERLARGE.IsValid() && parentTrailer.GetModelIndex() == MI_TRAILER_TRAILERLARGE )
	{
		additionalLegExtend = sfAdditionalLegExtendForTrailerLarge;
	}

	float fOriginalMatLegBoundZ = matLegBound.d.z;

	matLegBound.d.z += m_fDisplaceDistance;

	pInstanceComposite->SetLastMatrix(m_iFragChild, MATRIX34_TO_MAT34V(matLegBound));

    m_fDisplaceDistance = fDisplaceDist * (1.0f - m_fCurrentRatio) - additionalLegExtend;

	matLegBound.d.z = fOriginalMatLegBoundZ;
	matLegBound.d.z += m_fDisplaceDistance;

	pInstanceComposite->SetCurrentMatrix(m_iFragChild, MATRIX34_TO_MAT34V(matLegBound));

	// Now setup the link attach matrices so the current matrix doesn't get overwritten by updatecurrentandlastmatrices
	if (const fragCacheEntry* pEntry = parentTrailer.GetVehicleFragInst()->GetCacheEntry())
	{
		if (Matrix34* linkAttachmentMatrices = pEntry->GetHierInst()->linkAttachment->GetElements())
		{
			phArticulatedBody* body = pEntry->GetHierInst()->body;
			Assert(body);

			const phArticulatedCollider* collider = pEntry->GetHierInst()->articulatedCollider;

			int bodyPartIndex = collider->GetLinkFromComponent(m_iFragChild);
			Assert(bodyPartIndex>=0);
			Matrix34 bodyPartMatrix(MAT34V_TO_MATRIX34(body->GetLink(bodyPartIndex).GetMatrix()));
			bodyPartMatrix.Transpose();

			Matrix34& linkAttachment = linkAttachmentMatrices[m_iFragChild];
			linkAttachment.Set(matLegBound);
			linkAttachment.Dot(RCC_MATRIX34(parentTrailer.GetVehicleFragInst()->GetMatrix()));
			linkAttachment.d.Subtract(RCC_VECTOR3(collider->GetPosition()));
			linkAttachment.DotTranspose(bodyPartMatrix);
		}
	}

	float oldRadius = pInstanceComposite->GetRadiusAroundCentroid();
	pInstanceComposite->CalculateCompositeExtents();
	CPhysics::GetLevel()->RebuildCompositeBvh(pFragInst->GetLevelIndex());

    // toggle the legs to not collide when fully up
    if(m_iFragChild > -1)
    {
        if(m_fCurrentRatio <= 0.0f)
        {
            pInstanceComposite->SetIncludeFlags(m_iFragChild, 0);
        }
        else
        {
            pInstanceComposite->SetIncludeFlags(m_iFragChild, ArchetypeFlags::GTA_VEHICLE_INCLUDE_TYPES);
        }
    }

	if (oldRadius != pInstanceComposite->GetRadiusAroundCentroid())
	{
		CPhysics::GetLevel()->UpdateObjectLocationAndRadius(pFragInst->GetLevelIndex(), (Mat34V_Ptr)NULL);
	}
}

void CTrailerLegs::PreRender(CVehicle *pParent)
{
    if(pParent->GetSkeleton() )
    {
        if(m_iBoneIndex > -1)
        {
            Matrix34& legMatrix = pParent->GetLocalMtxNonConst(m_iBoneIndex);

            legMatrix.Identity3x3();

            legMatrix.d = RCC_VECTOR3(pParent->GetSkeleton()->GetSkeletonData().GetBoneData(m_iBoneIndex)->GetDefaultTranslation());

            static dev_float sfLegDisplaceMax = 0.25f;
            if(m_fDisplaceDistance < sfLegDisplaceMax)
            {
                legMatrix.d.z += m_fDisplaceDistance;
            }
            else
            {
                legMatrix.d.z += sfLegDisplaceMax;
            }
        }
    }
}


//////////////////////////////////////////////////////////////////////////
// CTrailer

CTrailer::CTrailer(const eEntityOwnedBy ownedBy, u32 popType) : 
CAutomobile(ownedBy, popType, VEHICLE_TYPE_TRAILER),
m_vLastFramesCrossParentUpTrailerUp(VEC3_ZERO),
m_AttachedParent(nullptr)
{
	//m_pRotConstraintX = NULL;
	//m_pRotConstraintZ = NULL;
    m_isAttachedToParent = false;

	for (s32 i = 0; i < MAX_CARGO_VEHICLES; ++i)
	{
		m_cargoVehicles[i] = NULL;
	}
	m_cargoVehiclesAttached = false;
	m_bCollidedWithAttachParent = false;
	m_bHasBreakableExtras = false;
	m_bIsTrailerAttachmentEnabled = true;
	m_bCanLocalPlayerAttach = true;
	m_bDisableCollisionsWithObjectsBasicallyAttachedToParent = true;
	m_bDisablingImpacts = false;
	//m_bHasExtendingSides = false;
	//m_extendingSidesRatio = 0.0f;
	//m_extendingSidesTargetRatio = 0.0f;
	//m_scriptOverridingExtendableRatio = false;
	//m_stationaryDuration = 0.0f;
}

CTrailer::~CTrailer()
{
#if __ASSERT
	for(int i=0; i<MAX_CARGO_VEHICLES; i++)
	{
		Assertf(!m_cargoVehicles[i],"Trailer being destructed with cargo vehicles still attached.  Are cargo vehicles being added outside of CVehicleFactory?");
	}
#endif
}

void CTrailer::DoProcessControl(bool fullUpdate, float fFullUpdateTimeStep)
{
	CAutomobile::DoProcessControl(fullUpdate, fFullUpdateTimeStep);

	// Todo check type
	if(GetIsAttached())
	{
		m_trailerLegs.SetTargetRatio(0.0f);

		CPhysical* pAttachParent = (CPhysical *) GetAttachParent();
		if(pAttachParent)
		{
			Assert(pAttachParent->GetIsTypeVehicle());
			CVehicle* pParentVehicle = static_cast<CVehicle*>(pAttachParent);

			//copy the vehicle controls into the trailer so it will update its reversing lights and brake lights.
			m_vehControls = pParentVehicle->m_vehControls;

			m_nVehicleFlags.bLightsOn = pParentVehicle->m_nVehicleFlags.bLightsOn;
		}
	}
	else
	{
		float trailerLegsRatio = 1.0f;
		if(GetStatus() == STATUS_WRECKED)
		{
			// Don't extend the legs if wrecked (This'll actually pull them in if they were extended too - that's probably ok)
			trailerLegsRatio = 0.0f;
		}

		m_nVehicleFlags.bLightsOn = false;
		m_trailerLegs.SetTargetRatio(trailerLegsRatio);
	}

	if (m_cargoVehiclesAttached)
	{
		bool doorOpen = false;
		for (s32 i = 0; i < GetNumDoors(); ++i)
		{
			CCarDoor* pDoor = GetDoor(i);
			if(pDoor && pDoor->GetIsIntact(this) && pDoor->GetHierarchyId()!=VEH_BOOT)
			{
				if (pDoor->GetTargetDoorRatio() > 0.f)
				{
					doorOpen = true;
					break;
				}
			}
		}

		// Trailers are using basic attachments so they can't resolve collisions with the ground. Just detach if we
		//   get into a bad state. 
		const ScalarV minimumUpAngleCosine(V_HALF); // 60 degrees 
		bool isTrailerUpsideDown = IsLessThanAll(GetTransform().GetUp().GetZ(),minimumUpAngleCosine) != 0;

		if (!IsSuperDummy() && (doorOpen || m_nVehicleFlags.bIsDrowning || isTrailerUpsideDown))
		{
			DetachCargoVehicles();	
		}

		Vector3 currentVelocity = GetVelocity();
		Vector3 currentAngVelocity = GetAngVelocity();
		for (int cargoVehicleIndex = 0; cargoVehicleIndex < MAX_CARGO_VEHICLES; ++cargoVehicleIndex)
		{
			if (CVehicle* pVehicle = m_cargoVehicles[cargoVehicleIndex])
			{
				if(	AssertVerify(pVehicle->m_nVehicleFlags.bIsCargoVehicle) && 
					AssertVerify(pVehicle->GetParentTrailer() == this) &&
					AssertVerify(!pVehicle->IsRunningCarRecording()) && 
					AssertVerify(pVehicle->GetOwnedBy() != ENTITY_OWNEDBY_CUTSCENE))
				{
					pVehicle->SetInactivePlaybackVelocity(currentVelocity);
					pVehicle->SetInactivePlaybackAngVelocity(currentAngVelocity);
					pVehicle->CacheAiData();
				}
			}
		}
	}

	//if( m_bHasExtendingSides &&
	//	GetStatus() != STATUS_WRECKED )
	//{
	//	if( !m_scriptOverridingExtendableRatio &&
	//		GetNumContactWheels() )
	//	{
	//		TUNE_GROUP_FLOAT( VEHICLE_TRAILER_EXTENDABLE_SIDES, StationaryDuration, 5.0f, 0.0f, 100.0f, 0.1f );
	//		TUNE_GROUP_FLOAT( VEHICLE_TRAILER_EXTENDABLE_SIDES, VelocityToRetractSidesSqr, 25.0f, 0.0f, 10000.0f, 0.1f );

	//		float velocityMag = GetVelocity().Mag2();
	//		if( velocityMag < 1.0f )
	//		{
	//			m_stationaryDuration = Min( m_stationaryDuration + fwTimer::GetTimeStep(), StationaryDuration + 1.0f );
	//		}
	//		else
	//		{
	//			m_stationaryDuration = 0.0f;
	//		}

	//		if( m_stationaryDuration > StationaryDuration )
	//		{
	//			m_extendingSidesTargetRatio = 1.0f;
	//		}
	//		else if( velocityMag > VelocityToRetractSidesSqr )
	//		{
	//			m_extendingSidesTargetRatio = 0.0f;
	//		}
	//	}
	//	UpdateExtendingSides( fwTimer::GetTimeStep() );
	//}

}

static dev_float sfTrailerZOpposeDetach = -0.35f;
static dev_float sfTrailerZOpposeLoseObjects = 0.1f;
float CTrailer::sm_fTrailerYOpposeDetach = 0.25f;
float CTrailer::sm_fTrailerYOpposeDetachSphericalConstraintInMP = 0.10f;

static dev_float sfTrailerLoseObjectsMinImpulseMagSum = 10.0f;
static dev_float sfTrailerLoseObjectsMaxImpulseMagSum = 60.0f;
static dev_float sfTrailerLoseObjectsSpeedSq = 36.0f;
static dev_float sfTrailerLoseObjectsMinAngSpeedSq = 1.8f;
static dev_float sfTrailerLoseObjectsMaxAngSpeedSq = 6.0f;

float CTrailer::sm_fAttachOffsetHauler = 0.2f;

void CTrailer::StartWheelIntegratorIfNecessary(ePhysicsResult automobilePhysicsResult, float fTimeStep)
{
	if(automobilePhysicsResult == PHYSICS_NEED_SECOND_PASS)
	{
		if(phCollider* pCollider = GetCollider())
		{
			StartWheelIntegratorTask(pCollider,fTimeStep);
		}
	}
}

ePhysicsResult CTrailer::ProcessPhysics(float fTimeStep, bool bCanPostpone, int nTimeSlice)
{
	const ePhysicsResult nResult = CAutomobile::ProcessPhysics(fTimeStep, bCanPostpone, nTimeSlice);	

	if( m_vehicleAiLod.IsLodFlagSet(CVehicleAILod::AL_LodDummy) ||
		m_vehicleAiLod.IsLodFlagSet(CVehicleAILod::AL_LodSuperDummy))
	{
		StartWheelIntegratorIfNecessary(nResult,fTimeStep);
		return nResult;
	}

	m_trailerLegs.ProcessPhysics(this, fTimeStep, nTimeSlice);

	phConstraintBase* pPosConstraint = PHCONSTRAINT->GetTemporaryPointer(m_posConstraintHandle);
	phConstraintRotation* pRotXConstraint = static_cast<phConstraintRotation*>( PHCONSTRAINT->GetTemporaryPointer(m_rotXConstraintHandle) );
	phConstraintRotation* pRotYConstraint = static_cast<phConstraintRotation*>( PHCONSTRAINT->GetTemporaryPointer(m_rotYConstraintHandle) );
	phConstraintRotation* pRotZConstraint = static_cast<phConstraintRotation*>( PHCONSTRAINT->GetTemporaryPointer(m_rotZConstraintHandle) );

	//if either constraint has failed just disconnect
	if( (pPosConstraint && pPosConstraint->IsBroken()) || (pRotXConstraint && pRotXConstraint->IsBroken()) || (pRotYConstraint && pRotYConstraint->IsBroken()) || (pRotZConstraint && pRotZConstraint->IsBroken()) )
	{
		DetachFromParent(0);
		StartWheelIntegratorIfNecessary(nResult,fTimeStep);
		return nResult;
	}
	else if(GetAttachParent())
	{
		CPhysical* pAttachParent = (CPhysical *) GetAttachParent();
		ScalarV sUpDotParent = Dot(GetTransform().GetC(), pAttachParent->GetTransform().GetC());
		ScalarV sForwardDotParent = Dot(GetTransform().GetB(), pAttachParent->GetTransform().GetB());
		bool bIsParentHeli = pAttachParent->GetIsTypeVehicle() && static_cast<CVehicle*>(pAttachParent)->InheritsFromHeli();

		float fTrailerYOpposeDetach = sm_fTrailerYOpposeDetach;

		if(NetworkInterface::IsGameInProgress())// Add some leeway for reconnecting with a spherical constraint but only for MP so we don't break any missions.
		{
			if(pPosConstraint && pPosConstraint->GetType() == phConstraintBase::SPHERICAL)
			{
				fTrailerYOpposeDetach = sm_fTrailerYOpposeDetachSphericalConstraintInMP;
			}
		}

		if(!IsNetworkClone() && !bIsParentHeli && (IsLessThanAll(sUpDotParent, ScalarVFromF32(sfTrailerZOpposeDetach)) ||
		  (IsLessThanAll(sForwardDotParent, ScalarVFromF32(fTrailerYOpposeDetach)) && GetCollidedWithAttachParent())))
		{
			DetachFromParent(0);
			StartWheelIntegratorIfNecessary(nResult,fTimeStep);
			return nResult;
		}
		else if(m_bHasBreakableExtras)
		{
			// Make the objects in the back of the trailer fall out
			const CCollisionHistory* pCollisionHistory = GetFrameCollisionHistory();
			const CCollisionHistory* pParentCollisionHistory = GetAttachParentVehicle() ? GetAttachParentVehicle()->GetFrameCollisionHistory() : NULL;
			
			float fMaxImpulseMag = pCollisionHistory ? pCollisionHistory->GetCollisionImpulseMagSum() : 0.0f;
			if(pCollisionHistory && pParentCollisionHistory)
				fMaxImpulseMag = Max(pCollisionHistory->GetCollisionImpulseMagSum(), pParentCollisionHistory->GetCollisionImpulseMagSum());
			
			// Trailer tipped over, everything should fall out
			float fBreakOffChance = IsLessThanAll(sUpDotParent, ScalarVFromF32(sfTrailerZOpposeLoseObjects)) ? 1.0f : 0.0f;

			// Easier to fall out at higher speeds
			if(GetAngVelocity().Mag2() > sfTrailerLoseObjectsMinAngSpeedSq)
				fBreakOffChance = Max(fBreakOffChance, Clamp(GetAngVelocity().Mag2()/sfTrailerLoseObjectsMaxAngSpeedSq, 0.0f, 0.5f));

			float fLinearSpeedMult = Clamp(GetVelocity().Mag2()/sfTrailerLoseObjectsSpeedSq, 0.0f, 2.0f);
			if(fMaxImpulseMag * fLinearSpeedMult > sfTrailerLoseObjectsMinImpulseMagSum * GetMass())
				fBreakOffChance = Max(fBreakOffChance, Clamp((fMaxImpulseMag * fLinearSpeedMult)/(sfTrailerLoseObjectsMaxImpulseMagSum * GetMass()), 0.0f, 1.0f));

			if(fBreakOffChance > 0.0f)
				GetVehicleDamage()->BreakOffMiscComponents(0.0f,fBreakOffChance, GetLocalMtx(0).d);
		}
	}

	// Keep the positional constraint up to date
	if(pPosConstraint)
	{
		CPhysical* pAttachParent = (CPhysical *) GetAttachParent();
		Assertf(pAttachParent,"Unexpected NULL attach parent");		

		if(pAttachParent)
		{
			Assert(pAttachParent->GetIsTypeVehicle());
			CVehicle* pParentVehicle = static_cast<CVehicle*>(pAttachParent);

            // Make sure the truck and trailer don't turn into a dummy whilst they are attached
			pParentVehicle->m_nVehicleFlags.bPreventBeingDummyThisFrame = !CVehicleAILodManager::ms_bAllowTrailersToConvertFromReal;
            m_nVehicleFlags.bPreventBeingDummyThisFrame = !CVehicleAILodManager::ms_bAllowTrailersToConvertFromReal;

			CTrailerHandlingData* pTrailerHandling = pHandling->GetTrailerHandlingData();

            if(CPhysics::GetLevel()->IsActive(GetVehicleFragInst()->GetLevelIndex()))
            {
                // Add an extra spring to try and keep the trailer upright if we have a rotational constraint
                Vector3 parentUp = VEC3V_TO_VECTOR3(pParentVehicle->GetTransform().GetC());
                Vector3 trailerUp = VEC3V_TO_VECTOR3(GetTransform().GetC());
                Vector3 crossParentUpTrailerUp;

				if(pTrailerHandling)
				{
					const float springConst = pTrailerHandling->m_fUprightSpringConstant;
					const float dampingConst = pTrailerHandling->m_fUprightDampingConstant;

					// Work out the difference in the two vehicles up vectors and use this to create a torque to keep the trailer upright in regards to the truck
					crossParentUpTrailerUp.Cross(parentUp, trailerUp);

					// Work out the torque to apply with some damping.
					Vector3 dampingToApply(VEC3_ZERO);
					if(fTimeStep > 0.0f)
					{
						dampingToApply = ((crossParentUpTrailerUp-m_vLastFramesCrossParentUpTrailerUp) / fTimeStep);
					}

					Vector3 torqueToApply = ((crossParentUpTrailerUp * springConst) - (dampingToApply * dampingConst)) * GetMass();// scale the torque by mass

					// Keep track of the previous frames difference so we can do some damping
					m_vLastFramesCrossParentUpTrailerUp = crossParentUpTrailerUp;
					ApplyTorque(torqueToApply);
				}
            }


			// Look up attachment point of parent
			int iParentAttachIndex = pParentVehicle->GetBoneIndex(VEH_ATTACH);
			int iMyAttachIndex = GetBoneIndex(TRAILER_ATTACH);

			Assertf(iParentAttachIndex > -1, "Cannot attach to a vehicle with no attach point");
			Assertf(iMyAttachIndex > -1, "This trailer has no attach point: %s", GetVehicleModelInfo()->GetModelName());

			if(iParentAttachIndex == -1 || iMyAttachIndex == -1)
			{
				StartWheelIntegratorIfNecessary(nResult,fTimeStep);
				return nResult;
			}

			// Assume A is trailer, B is truck
			Matrix34 matAttachA, matAttachB;
			GetGlobalMtx(iMyAttachIndex, matAttachA);

			pParentVehicle->GetGlobalMtx(iParentAttachIndex, matAttachB);

			if(pParentVehicle->GetModelIndex() == MI_CAR_HAULER && NetworkInterface::IsGameInProgress())
			{
				matAttachB.d += matAttachB.c * sm_fAttachOffsetHauler;
			}

			Assert(pPosConstraint->GetInstanceA() == GetVehicleFragInst());
			Assert(pPosConstraint->GetInstanceB() == pParentVehicle->GetVehicleFragInst());

			//Raise up the spherical constraint's Z position if we're colliding with our parent to prevent bouncing
			if(pTrailerHandling && pPosConstraint->GetType() == phConstraintBase::SPHERICAL)
			{
				if(GetCollidedWithAttachParent() && pTrailerHandling->m_fAttachRaiseZ > 0.0f)
				{
					matAttachA.d.z += pTrailerHandling->m_fAttachRaiseZ;
					matAttachB.d.z += pTrailerHandling->m_fAttachRaiseZ;
				}
			}

			pPosConstraint->SetWorldPosA(VECTOR3_TO_VEC3V(matAttachA.d));
			pPosConstraint->SetWorldPosB(VECTOR3_TO_VEC3V(matAttachB.d));

			SetCollidedWithAttachParent(false);

//Constraint length functionality is not currently used
#if 0 
			static dev_float sfDistReductionSpeed = 0.2f;
			static dev_float sfDistReductionVelThresholdSq = 5.0f * 5.0f;
			static dev_float sfLengthZeroThreshold = 0.01f;	// Joints act badly with small lengths, make sure they go to 0 eventually
			// Only do this at speed
			if(GetVelocity().Mag2() > sfDistReductionVelThresholdSq && m_pPosConstraint->GetLength() > sfLengthZeroThreshold)
			{
				// Pull constraint closer to desired position
				Vector3 vSep = matAttachA.d - matAttachB.d;

				static dev_float sfReleaseMagSq = 0.5f * 0.5f; // quick way to detect funk stuff happening. just detach for now. Will need to handle teleports properly soon though
				if(vSep.Mag2() > sfReleaseMagSq)
				{
					if (Verifyf(!IsNetworkClone(), "Trying to detach a network clone trailer"))
					{
						DetachFromParent(0);
					}
				}

				float fLength = vSep.Mag();

				fLength = Max(fLength - (sfDistReductionSpeed * fTimeStep), 0.0f);
				m_pPosConstraint->SetLength(fLength);

			}
#endif
		
		}
	}

	// The boot component is typically used on the trailers as a rear ramp object
	// We'd like to keep it physical, but it tends to bounce up to high when scraping along the ground
	// So, we're going to aggressively damp its velocity when rotating upwards
	const ScalarV angularDampingTerm(140.0f);
	const ScalarV angularDampingSquaredTerm(290.0f);
	int rampIndex = GetBoneIndex(VEH_BOOT);
	fragInstGta* pTrailerFragInst = GetVehicleFragInst();
	if(rampIndex != -1 && pTrailerFragInst != NULL)
	{
		phArticulatedCollider* pArtCollider = pTrailerFragInst->GetArticulatedCollider();
		if(pArtCollider != NULL && pArtCollider->IsArticulated())
		{
			ScalarV timeStep(fwTimer::GetTimeStep());
			const Vec3V trailerMatRight = GetTransform().GetA();

			const int fragChildIndex = pTrailerFragInst->GetComponentFromBoneIndex(rampIndex);
			const int rampLinkIndex = pArtCollider->GetLinkFromComponent(fragChildIndex);
			phArticulatedBody* artBody = pArtCollider->GetBody();

			// Have to do this before we make any changes
			artBody->EnsureVelocitiesFullyPropagated();

			const Vec3V rampAngVel = artBody->GetAngularVelocity(rampLinkIndex);
			const ScalarV angVelMagAlongAxis = Dot(rampAngVel, trailerMatRight);

			// Apply only when rotating up
			if( IsLessThanAll(angVelMagAlongAxis, ScalarV(V_ZERO)) != 0 )
			{
				// Linear
				const ScalarV dampedAngMagAlongAxis = Scale(angVelMagAlongAxis, angularDampingTerm);

				// Squared
				const ScalarV sqrdAngVelMagAlongAxis = Scale(angVelMagAlongAxis, Abs(angVelMagAlongAxis));
				const ScalarV sqrdDampedAngMagAlongAxis = Scale(sqrdAngVelMagAlongAxis, angularDampingSquaredTerm);

				// Total velocity change
				const ScalarV angMagDelta = Scale( rage::Add(dampedAngMagAlongAxis, sqrdDampedAngMagAlongAxis), timeStep );
				const ScalarV angMagDeltaClamped = SelectFT( IsLessThan(Abs(angMagDelta), Abs(angVelMagAlongAxis)), angVelMagAlongAxis, angMagDelta );
				const Vec3V dampedAngVelFinal = Subtract(rampAngVel, Scale(trailerMatRight, angMagDeltaClamped));

				// Do it
				artBody->SetAngularVelocity(rampLinkIndex, dampedAngVelFinal);

				// And this has to happen after our changes
				artBody->ResetPropagationVelocities();
			}
		}
	}

	StartWheelIntegratorIfNecessary(nResult,fTimeStep);
	return nResult;
}

void CTrailer::InitCompositeBound()
{
	CAutomobile::InitCompositeBound();

	// Turn on collision with the trailer legs
	int iLegsBoneIndex = GetBoneIndex(TRAILER_LEGS);

	if(iLegsBoneIndex > -1)
	{
		Assert(GetFragInst());
		Assert(GetFragInst()->GetType());
		int iCompositeBoundIndex = GetFragInst()->GetComponentFromBoneIndex(iLegsBoneIndex);

		if(iCompositeBoundIndex > -1)
		{
			Assert(GetFragInst()->GetArchetype()->GetBound()->GetType() == phBound::COMPOSITE);
			phBoundComposite* pCompBound = static_cast<phBoundComposite*>(GetFragInst()->GetArchetype()->GetBound());
			
			pCompBound->SetIncludeFlags(iCompositeBoundIndex, ArchetypeFlags::GTA_VEHICLE_INCLUDE_TYPES);
		}
	}
}

int CTrailer::InitPhys()
{
	CAutomobile::InitPhys();

	int iLegsIndex = GetBoneIndex(TRAILER_LEGS);
	//vehicleAssertf(iLegsIndex > -1,"Vehilce %s is missing trailer leg collision", GetVehicleModelInfo()->GetModelName()); //some trailer will have no legs, so no need for assert
	if(iLegsIndex > -1)
	{
		m_trailerLegs.Init(iLegsIndex, this);
	}

	for(int nExtra=VEH_BREAKABLE_EXTRA_1; nExtra <= VEH_LAST_BREAKABLE_EXTRA; nExtra++)
	{
		if(GetBoneIndex((eHierarchyId)nExtra) > -1)
		{
			m_bHasBreakableExtras = true;
			break;
		}
	}
	//for( int nExtra = VEH_EXTENDABLE_SIDE_L; nExtra <= VEH_EXTENDABLE_SIDE_R; nExtra++ )
	//{
	//	if( GetBoneIndex( (eHierarchyId)nExtra ) > -1 )
	//	{
	//		m_bHasExtendingSides = true;
	//		break;
	//	}
	//}
	if (MI_TRAILER_TRAILERLARGE.IsValid() && GetModelIndex() == MI_TRAILER_TRAILERLARGE)
	{
		m_nVehicleFlags.bDisableSuperDummy = true;
	}
	return INIT_OK;
}

void EnforceConstraintLimits(phConstraintRotation* pConstraint, float fMin, float fMax, bool bIsPitch)
{
	if(bIsPitch)
	{
		fMin = fwAngle::LimitRadianAngleForPitch(fMin);
		fMax = fwAngle::LimitRadianAngleForPitch(fMax);
	}
	else
	{
		fMin = fwAngle::LimitRadianAngle(fMin);
		fMax = fwAngle::LimitRadianAngle(fMax);
	}

	if(fMin > fMax)
	{
		SwapEm(fMin, fMax);
	}

	pConstraint->SetMaxLimit(fMax);
	pConstraint->SetMinLimit(fMin);
}

void EnforceConstraintLimits(phConstraintRotation* pConstraint, float fMin, float fMax, float fCurrent, bool bIsPitch)
{
	fMin -= fCurrent;
	fMax -= fCurrent;

	EnforceConstraintLimits(pConstraint, fMin, fMax, bIsPitch);
}

static dev_float sfConstraintStrengthPosition = 5500000.0f;
static dev_float sfConstraintStrengthRotation = 5000000.0f;

#if __BANK
bool CTrailer::DebugAttachToParentVehicle(const char *strCodeFile, const char* strCodeFunction, u32 nCodeLine, CVehicle* pParent, bool bWarp, float fInvMass, bool bForceReAttach)
#else
bool CTrailer::AttachToParentVehicle(CVehicle* pParent, bool bWarp, float fInvMass, bool bForceReAttach)
#endif
{
	Assert(pParent);
	Assert(!IsDummy());
	Assert(!pParent->IsDummy());

	TUNE_GROUP_BOOL(TRAILER_HACKS, DISABLE_TRAILER_ATTACHMENT, false);
	// Script are blocking this trailer from being attached to vehicles
	if( !m_bIsTrailerAttachmentEnabled || DISABLE_TRAILER_ATTACHMENT )
	{
		return false;
	}

	// use CPhysical attachment system
	// Attach at tail position
	int iThisBoneIndex = GetBoneIndex(TRAILER_ATTACH);
	int iOtherBoneIndex = pParent->GetBoneIndex(VEH_ATTACH);

	// Assert that it is possible to attach this vehicle to other vehicle
	Assertf(iThisBoneIndex > -1, "This trailer has no attach point: %s", GetVehicleModelInfo()->GetModelName());
	Assertf(iOtherBoneIndex > -1, "Cannot attach to a vehicle with no attach point, %s", pParent->GetVehicleModelInfo()->GetModelName());

	if(iThisBoneIndex == -1 || iOtherBoneIndex == -1)
	{
		return false;
	}

	if(!bForceReAttach)
	{
		//make sure we're not already attached to this trailer, could happen if someone used the script command to attach.
		CPhysical* pNextChild = (CPhysical *) pParent->GetChildAttachment();
		while(pNextChild)
		{
			if(pNextChild == this)
			{
				return true;//already attached to this trailer.
			}
			else
				pNextChild = (CPhysical *) pNextChild->GetSiblingAttachment();
		}
	}


	if(GetIsAttached())
	{
		DetachFromParent(0);
	}

	if( pParent )
	{
		for(int i = 0; i < GetNumberOfVehicleGadgets(); i++)
		{
			CVehicleGadget *pVehicleGadget = GetVehicleGadget(i);

			if( pVehicleGadget->GetType() == VGT_TRAILER_ATTACH_POINT )
			{
				CVehicleTrailerAttachPoint *pTrailerAttachPoint = static_cast<CVehicleTrailerAttachPoint*>(pVehicleGadget);

				CTrailer *pTrailer = pTrailerAttachPoint->GetAttachedTrailer( pParent );

				if(pTrailer)
				{
					pTrailerAttachPoint->DetachTrailer( pParent );
				}
			}
		}

		CPhysical* pNextChild = (CPhysical*) pParent->GetChildAttachment();
		while( pNextChild )
		{
			if( pNextChild->GetType() == ENTITY_TYPE_VEHICLE )
			{
				CVehicle* pChildVehicle = static_cast< CVehicle* >( pNextChild );
				if( pChildVehicle->InheritsFromTrailer() )
				{
					CTrailer* pChildTrailer = static_cast< CTrailer* >( pChildVehicle );
					pChildTrailer->DetachFromParent( 0 );
				}
			}
			pNextChild = (CPhysical *)pNextChild->GetSiblingAttachment();
		}
	}
		
	Matrix34 matMyBone;
	GetGlobalMtx(iThisBoneIndex, matMyBone);

	Matrix34 matOtherBone;
	pParent->GetGlobalMtx(iOtherBoneIndex, matOtherBone);

	if(pParent->GetModelIndex() == MI_CAR_HAULER && NetworkInterface::IsGameInProgress())
	{
		matOtherBone.d += matOtherBone.c * sm_fAttachOffsetHauler;
	}
    // Check if we should warp
	bool warpPositionOnly = false;
	if(!bWarp)
	{
		Vector3 vSep = matMyBone.d - matOtherBone.d;
		const float arbitraryAttachDistanceScale = 1.1f;
		const float warpRadius = CVehicleTrailerAttachPoint::ms_fAttachRadius * arbitraryAttachDistanceScale;
		if(vSep.Mag2() > warpRadius * warpRadius)
		{
			bWarp = true;
			warpPositionOnly = true;
		}
	}

	static dev_bool sbDoPairedColl = true;

	u32 uAttachFlags = ATTACH_STATE_PHYSICAL | ATTACH_FLAG_POS_CONSTRAINT | ATTACH_FLAG_ROT_CONSTRAINT | ATTACH_FLAG_DELETE_WITH_PARENT;

	if(sbDoPairedColl)
	{
		uAttachFlags |= ATTACH_FLAG_DO_PAIRED_COLL;
	}

	if(bWarp)
	{
		uAttachFlags |= ATTACH_FLAG_INITIAL_WARP;

		WarpToPosition( pParent, warpPositionOnly );
	}

	if(IsDummy() && !CVehicleAILodManager::ms_bAllowTrailersToConvertFromReal)
    {
        TryToMakeFromDummy();
        Assert(!IsDummy());
    }

    if(pParent->IsDummy() && !CVehicleAILodManager::ms_bAllowTrailersToConvertFromReal)
    {
        pParent->TryToMakeFromDummy();
        Assert(!pParent->IsDummy());
    }

	//get the component that corresponds to the bone, or just use 0
	int nEntComponent = pParent->GetVehicleFragInst()->GetControllingComponentFromBoneIndex(iOtherBoneIndex); 
	if(nEntComponent < 0)
		nEntComponent = 0;


	int nMyComponent = GetVehicleFragInst()->GetControllingComponentFromBoneIndex(iThisBoneIndex);
	if(nMyComponent < 0)
		nMyComponent = 0;

	// Make sure we are allowed to activate
	if(GetCurrentPhysicsInst())
		GetCurrentPhysicsInst()->SetInstFlag(phInst::FLAG_NEVER_ACTIVATE, false);

	ActivatePhysics();

	// Get matrix of trailer in cab space
	// This might need to be the other way round!
	//Matrix34 matTrailerLocal = MAT34V_TO_MATRIX34(GetMatrix());
	//float fCurrentPitch = GetMatrix().c.AngleX(pParent->GetMatrix().c);
	//float fCurrentHeading = GetMatrix().b.AngleZ(pParent->GetMatrix().b);
	
	// Set up 4x constraints
	// Assert that we aren't about to leak constraints
	Assert( !m_posConstraintHandle.IsValid() );
	Assert( !m_rotXConstraintHandle.IsValid() );
	Assert( !m_rotYConstraintHandle.IsValid() );
	Assert( !m_rotZConstraintHandle.IsValid() );

	phConstraintBase* pPosConstraint = NULL;

	// Now set up rotation constraints
	phConstraintRotation* pRotConstraintX = NULL;
	phConstraintRotation* pRotConstraintY = NULL;
	phConstraintRotation* pRotConstraintZ = NULL;
	CTrailerHandlingData* pTrailerHandling = pHandling->GetTrailerHandlingData();

	const float posConstraintMassRatio = pTrailerHandling ? pTrailerHandling->m_fPosConstraintMassRatio : 1.0f;
	const float rotConstraintMassRatio = posConstraintMassRatio;

	if( pTrailerHandling &&
		MI_TRAILER_TRAILERLARGE.IsValid() && 
		GetModelIndex() == MI_TRAILER_TRAILERLARGE )
	{
		if( pTrailerHandling->m_fAttachLimitPitch >= 0.0f )
		{
			const float sfPitchLimit = DtoR * ( pTrailerHandling->m_fAttachLimitPitch );
			float fCurrentPitch = GetTransform().GetPitch() - pParent->GetTransform().GetPitch();

			fCurrentPitch = fwAngle::LimitRadianAngleForPitch( fCurrentPitch );

			if( Abs( fCurrentPitch ) > sfPitchLimit )
			{
				return false;
			}
		}
		if( pTrailerHandling->m_fAttachLimitRoll >= 0.0f )
		{
			const float sfRollLimit = DtoR * ( pTrailerHandling->m_fAttachLimitRoll );
			float fCurrentRoll = pParent->GetTransform().GetRoll() - GetTransform().GetRoll();

			fCurrentRoll = fwAngle::LimitRadianAngle( fCurrentRoll );

			if( Abs( fCurrentRoll ) > sfRollLimit )
			{
				return false;
			}
		}
		if(pTrailerHandling->m_fAttachLimitYaw >= 0.0f)
		{
			const float sfYawLimit = DtoR * ( pTrailerHandling->m_fAttachLimitYaw );
			float fCurrentYaw = GetTransform().GetHeading() - pParent->GetTransform().GetHeading();

			fCurrentYaw = fwAngle::LimitRadianAngle(fCurrentYaw);

			if( Abs( fCurrentYaw ) > sfYawLimit )
			{
				return false;
			}
		}
	}

	//Don't add distance constraints if we're doing a playback
	if(!GetIntelligence() || GetIntelligence()->GetRecordingNumber() < 0 || CVehicleRecordingMgr::GetUseCarAI(GetIntelligence()->GetRecordingNumber()))
	{
		if(pTrailerHandling && pTrailerHandling->m_fAttachedMaxDistance > 0.0f)
		{
			// Distance constraint is available
			// When used with a small amount of allowed penetration the connection may be more tolerant 
			//   of bad configurations or insufficient solver iterations
			const float distConstraintMin = 0.0f;
			const float distConstraintMax = pTrailerHandling->m_fAttachedMaxDistance;
			const float distConstraintAllowedPen = pTrailerHandling->m_fAttachedMaxPenetration;
			//
			phConstraintDistance::Params posConstraintParams;
			posConstraintParams.worldAnchorA = VECTOR3_TO_VEC3V(matOtherBone.d);
			posConstraintParams.worldAnchorB = VECTOR3_TO_VEC3V(matOtherBone.d);
			posConstraintParams.instanceA = GetVehicleFragInst(); // If this order is changed than the massInvScale setting also needs to be swapped
			posConstraintParams.instanceB = pParent->GetVehicleFragInst();
			Assign(posConstraintParams.componentA, nMyComponent);
			Assign(posConstraintParams.componentB, nEntComponent);
			posConstraintParams.minDistance = distConstraintMin;
			posConstraintParams.maxDistance = distConstraintMax;
			posConstraintParams.allowedPenetration = distConstraintAllowedPen;
			posConstraintParams.massInvScaleA = posConstraintMassRatio;
			posConstraintParams.softAttach = true;

			m_posConstraintHandle = PHCONSTRAINT->Insert(posConstraintParams);
			Assertf(m_posConstraintHandle.IsValid(), "Failed to create trailer constraint");

			phConstraintDistance* pDistConstraint = static_cast<phConstraintDistance*>( PHCONSTRAINT->GetTemporaryPointer(m_posConstraintHandle) );
			Assertf(pDistConstraint, "Trailer constraint pointer NULL from phConstraintMgr");
			pPosConstraint = pDistConstraint;
		}
		else
		{
			// Alternatively the old method of a spherical constraint is still possible
			//  This seems to work better on the large trailers such as for a traditional 18-wheeler
			// Spherical constraints avoid the downside of the distance constraint which is that they can overshoot 
			//  and possibly oscillate around the target position
			phConstraintSpherical::Params posConstraintParams;
			posConstraintParams.worldPosition = VECTOR3_TO_VEC3V(matOtherBone.d);
			posConstraintParams.instanceA = GetVehicleFragInst(); // If this order is changed than massInvScaleB also needs to be swapped
			posConstraintParams.instanceB = pParent->GetVehicleFragInst();
			Assign(posConstraintParams.componentA, nMyComponent);
			Assign(posConstraintParams.componentB, nEntComponent);
			posConstraintParams.massInvScaleA = posConstraintMassRatio;
			posConstraintParams.softAttach = true;

			m_posConstraintHandle = PHCONSTRAINT->Insert(posConstraintParams);
			Assertf(m_posConstraintHandle.IsValid(), "Failed to create trailer constraint");

			phConstraintSpherical* pSphConstraint = static_cast<phConstraintSpherical*>( PHCONSTRAINT->GetTemporaryPointer(m_posConstraintHandle) );
			Assertf(pSphConstraint, "Trailer constraint pointer NULL from phConstraintMgr");
			pPosConstraint = pSphConstraint;
		}

		// Rotation constraints only if handling data specifies
		if(pTrailerHandling)
		{
			if(pTrailerHandling->m_fAttachLimitPitch >= 0.0f)
			{
				const float sfPitchLimit = DtoR*(pTrailerHandling->m_fAttachLimitPitch);
				float fCurrentPitch = GetTransform().GetPitch() - pParent->GetTransform().GetPitch();

				phConstraintRotation::Params rotXConstraintParams;
				rotXConstraintParams.instanceA = GetVehicleFragInst();
				rotXConstraintParams.instanceB = pParent->GetVehicleFragInst();
				Assign(rotXConstraintParams.componentA, nMyComponent);
				Assign(rotXConstraintParams.componentB, nEntComponent);
				rotXConstraintParams.worldAxis = pParent->GetTransform().GetA();
				rotXConstraintParams.massInvScaleA = rotConstraintMassRatio;

				m_rotXConstraintHandle = PHCONSTRAINT->Insert(rotXConstraintParams);

				pRotConstraintX = static_cast<phConstraintRotation*>( PHCONSTRAINT->GetTemporaryPointer( m_rotXConstraintHandle ) );

				if(vehicleVerifyf(pRotConstraintX, "Failed to create trailer constraint"))
				{
					EnforceConstraintLimits(pRotConstraintX, -sfPitchLimit, sfPitchLimit, fCurrentPitch, true);
				}
			}

			//
			if(pTrailerHandling->m_fAttachLimitRoll >= 0.0f)
			{
				const float sfRollLimit = DtoR*(pTrailerHandling->m_fAttachLimitRoll);
				float fCurrentRoll = pParent->GetTransform().GetRoll() - GetTransform().GetRoll();

				phConstraintRotation::Params rotYConstraintParams;
				rotYConstraintParams.instanceA = GetVehicleFragInst();
				rotYConstraintParams.instanceB = pParent->GetVehicleFragInst();
				Assign(rotYConstraintParams.componentA, nMyComponent);
				Assign(rotYConstraintParams.componentB, nEntComponent);
				rotYConstraintParams.worldAxis = pParent->GetTransform().GetB();
				rotYConstraintParams.massInvScaleA = rotConstraintMassRatio;

				m_rotYConstraintHandle = PHCONSTRAINT->Insert(rotYConstraintParams);

				pRotConstraintY = static_cast<phConstraintRotation*>( PHCONSTRAINT->GetTemporaryPointer( m_rotYConstraintHandle ) );

				if(vehicleVerifyf(pRotConstraintY, "Failed to create trailer constraint"))
				{
					EnforceConstraintLimits(pRotConstraintY, -sfRollLimit, sfRollLimit, fCurrentRoll, false);
				}
			}

			//I don't think this constraint is strictly necessary, as the trailer can collide with the cab
			if(pTrailerHandling->m_fAttachLimitYaw >= 0.0f)
			{
				const float sfYawLimit = DtoR*(pTrailerHandling->m_fAttachLimitYaw);
				float fCurrentYaw = GetTransform().GetHeading() - pParent->GetTransform().GetHeading();

				phConstraintRotation::Params rotZConstraintParams;
				rotZConstraintParams.instanceA = GetVehicleFragInst();
				rotZConstraintParams.instanceB = pParent->GetVehicleFragInst();
				Assign(rotZConstraintParams.componentA, nMyComponent);
				Assign(rotZConstraintParams.componentB, nEntComponent);
				rotZConstraintParams.worldAxis = pParent->GetTransform().GetC();
				rotZConstraintParams.massInvScaleA = rotConstraintMassRatio;

				m_rotZConstraintHandle = PHCONSTRAINT->Insert(rotZConstraintParams);

				pRotConstraintZ = static_cast<phConstraintRotation*>( PHCONSTRAINT->GetTemporaryPointer( m_rotZConstraintHandle ) );

				if(vehicleVerifyf(pRotConstraintZ, "Failed to create trailer constraint"))
				{
					EnforceConstraintLimits(pRotConstraintZ, -sfYawLimit, sfYawLimit, fCurrentYaw, false);
				}
			}
		}
	}

	// network clones cannot detach unless told to via an update. The attachment state must stay synced with the master trailer.
	if (!IsNetworkClone())
	{

#if __BANK
		static dev_float fForceInvMass = 0.0f;
		if(fForceInvMass > 0.0f)
		{
			fInvMass = fForceInvMass;
		}
#endif

		if( pPosConstraint )
		{
			pPosConstraint->SetBreakable(true,sfConstraintStrengthPosition);
			pPosConstraint->SetMassInvScaleB(fInvMass);
		}

		if( pRotConstraintX )
		{
			pRotConstraintX->SetBreakable(true,sfConstraintStrengthRotation);
			pRotConstraintX->SetMassInvScaleB(fInvMass);
		}

		if( pRotConstraintY )
		{
			pRotConstraintY->SetBreakable(true,sfConstraintStrengthRotation);
			pRotConstraintY->SetMassInvScaleB(fInvMass);
		}

		if( pRotConstraintZ )
		{
			pRotConstraintZ->SetBreakable(true,sfConstraintStrengthRotation);
			pRotConstraintZ->SetMassInvScaleB(fInvMass);
		}
	}

	fwAttachmentEntityExtension *extension = CreateAttachmentExtensionIfMissing();
	extension->SetParentAttachment(this, pParent);
	extension->SetAttachFlags(uAttachFlags);

#if __BANK
	extension->DebugSetInvocationData(strCodeFile, strCodeFunction, nCodeLine);
#endif

    GetCurrentPhysicsInst()->SetInstFlag(phInst::FLAG_OPTIONAL_ITERATIONS, true);
    pParent->GetCurrentPhysicsInst()->SetInstFlag(phInst::FLAG_OPTIONAL_ITERATIONS, true);

    //make sure the truck and trailer don't turn into a dummy
    pParent->m_nVehicleFlags.bPreventBeingDummyThisFrame = !CVehicleAILodManager::ms_bAllowTrailersToConvertFromReal;
    m_nVehicleFlags.bPreventBeingDummyThisFrame = !CVehicleAILodManager::ms_bAllowTrailersToConvertFromReal;

    m_isAttachedToParent = true;

    // Update the trucks vehicle gadget so it knows its attached to this trailer.
    // Could make it so people have to call the vehicle gadget to connect up but that might be confusing
    for(s32 i = 0; i < pParent->GetNumberOfVehicleGadgets(); i++)
    {
        CVehicleGadget *pVehicleGadget = pParent->GetVehicleGadget(i);

        if(pVehicleGadget->GetType() == VGT_TRAILER_ATTACH_POINT)
        {
            CVehicleTrailerAttachPoint *pTrailerAttachPoint = static_cast<CVehicleTrailerAttachPoint*>(pVehicleGadget);

            pTrailerAttachPoint->SetAttachedTrailer(this);
        }
    }

	m_trailerLegs.SetTargetRatio(0.0f);

	m_AttachedParent = pParent;

	SetVelocity( pParent->GetVelocity() );
	SetAngVelocity( pParent->GetAngVelocity() );

	return true;
}

void CTrailer::SetTrailerConstraintIndestructible(bool indestructibleTrailerConstraint)
{
	phConstraintBase* pPosConstraint = PHCONSTRAINT->GetTemporaryPointer(m_posConstraintHandle);
	phConstraintRotation* pRotConstraintX = static_cast<phConstraintRotation*>( PHCONSTRAINT->GetTemporaryPointer(m_rotXConstraintHandle) );
	phConstraintRotation* pRotConstraintY = static_cast<phConstraintRotation*>( PHCONSTRAINT->GetTemporaryPointer(m_rotYConstraintHandle) );
	phConstraintRotation* pRotConstraintZ = static_cast<phConstraintRotation*>( PHCONSTRAINT->GetTemporaryPointer(m_rotZConstraintHandle) );

    if(pPosConstraint && pRotConstraintY)
    {
        if(indestructibleTrailerConstraint)
        {
			pPosConstraint->SetBreakable(false);
			pRotConstraintX->SetBreakable(false);
			pRotConstraintY->SetBreakable(false);
            pRotConstraintZ->SetBreakable(false);
        }
        else if (Verifyf(!IsNetworkClone(), "Trying to set destructable trailer constraints on a network clone"))
        {
			pPosConstraint->SetBreakable(true, sfConstraintStrengthPosition);
            pRotConstraintX->SetBreakable(true, sfConstraintStrengthRotation);
			pRotConstraintY->SetBreakable(true, sfConstraintStrengthRotation);
			pRotConstraintZ->SetBreakable(true, sfConstraintStrengthRotation);
        }
    }
}

bool CTrailer::IsTrailerConstraintIndestructible() const
{
	phConstraintBase* pPosConstraint = PHCONSTRAINT->GetTemporaryPointer(m_posConstraintHandle);
	phConstraintRotation* pRotConstraintX = static_cast<phConstraintRotation*>( PHCONSTRAINT->GetTemporaryPointer(m_rotXConstraintHandle) );
	phConstraintRotation* pRotConstraintY = static_cast<phConstraintRotation*>( PHCONSTRAINT->GetTemporaryPointer(m_rotYConstraintHandle) );
	phConstraintRotation* pRotConstraintZ = static_cast<phConstraintRotation*>( PHCONSTRAINT->GetTemporaryPointer(m_rotZConstraintHandle) );

	bool breakable = (pPosConstraint && pPosConstraint->IsBreakable()) || 
					(pRotConstraintX && pRotConstraintX->IsBreakable()) || 
					(pRotConstraintY && pRotConstraintY->IsBreakable()) || 
					(pRotConstraintZ && pRotConstraintZ->IsBreakable());
	return( !breakable );
}

void CTrailer::WarpToPosition( CVehicle* pParent, bool translateOnly )
{
	const int iThisBoneIndex = GetBoneIndex(TRAILER_ATTACH);
	const int iOtherBoneIndex = pParent->GetBoneIndex(VEH_ATTACH);

	// Assert that it is possible to attach this vehicle to other vehicle
	Assertf(iThisBoneIndex > -1, "This trailer has no attach point: %s", GetVehicleModelInfo()->GetModelName());
	Assertf(iOtherBoneIndex > -1, "Cannot attach to a vehicle with no attach point");

	if(iThisBoneIndex == -1 || iOtherBoneIndex == -1)
	{
		return;
	}

	const Mat34V trailerMtxCur = GetTransform().GetMatrix();
	Mat34V trailerMtxNew;
	
#if ENABLE_FRAG_OPTIMIZATION
	Mat34V trailerAttachBoneToInstMtx;
	ComputeObjectMtx(iThisBoneIndex, trailerAttachBoneToInstMtx);
#else
	Assert(GetSkeleton());
	Mat34V trailerAttachBoneToInstMtx = GetSkeleton()->GetObjectMtx(iThisBoneIndex);
#endif

	Mat34V cabAttachBoneToWorldMtx(V_IDENTITY);
	// What to do if global matrices aren't up to date?
	pParent->GetGlobalMtx(iOtherBoneIndex, RC_MATRIX34(cabAttachBoneToWorldMtx));

	if(pParent->GetModelIndex() == MI_CAR_HAULER && NetworkInterface::IsGameInProgress())
	{
		cabAttachBoneToWorldMtx.SetCol3( rage::Add(cabAttachBoneToWorldMtx.GetCol3(), rage::Scale(cabAttachBoneToWorldMtx.GetCol2(), ScalarV(sm_fAttachOffsetHauler)) ));
	}

	if(translateOnly)
	{
		Mat34V trailerAttachBoneToWorldMtx;
		Transform(trailerAttachBoneToWorldMtx, trailerMtxCur, trailerAttachBoneToInstMtx);
		const Vec3V offsetToParentBone = Subtract(cabAttachBoneToWorldMtx.GetCol3(), trailerAttachBoneToWorldMtx.GetCol3());
		trailerMtxNew = trailerMtxCur;
		trailerMtxNew.SetCol3( rage::Add(trailerMtxNew.GetCol3(), offsetToParentBone) );
	}
	else
	{
		// We want to set the trailer matrix such that the world space of the attach bone is equal to the cab's attach bone world matrix
		// - Could be written as matrices: [(trailer)(trailerBone)] = [(cab)(cabBone)]
		//  -- Then we right multiply both by (trailerBone)^-1 and notice that we already have the cabBone in world space
		//  -- Resulting in us setting (trailer) = (cabBoneInWorldSpace)(trailerBone)^-1
		Mat34V parentAttachBoneToInstMtx = pParent->GetSkeleton()->GetObjectMtx( iOtherBoneIndex );

		// some of the assets have been created with the attachment bone the wrong way around
		if( parentAttachBoneToInstMtx.a().GetXf() * trailerAttachBoneToInstMtx.a().GetXf() < 0.0f )
		{
			trailerAttachBoneToInstMtx.GetCol0Ref() = trailerAttachBoneToInstMtx.GetCol0Ref() * ScalarV( -1.0f );
			trailerAttachBoneToInstMtx.GetCol1Ref() = trailerAttachBoneToInstMtx.GetCol1Ref() * ScalarV( -1.0f );
		}

		Mat34V trailerAttachBoneToInstMtxInverse;
		InvertTransformOrtho(trailerAttachBoneToInstMtxInverse, trailerAttachBoneToInstMtx);
		Transform(trailerMtxNew, cabAttachBoneToWorldMtx, trailerAttachBoneToInstMtxInverse);
	}
	//
	SetMatrix(MAT34V_TO_MATRIX34(trailerMtxNew),true,true,true);
	const Mat34V trailerMtxPrev = trailerMtxCur;

	// Matrix that we can use to calculate the full motion of an attachment's center given the change we've just made to the trailer
	Mat34V positionOffsetMtx;
	Subtract(positionOffsetMtx, trailerMtxNew, trailerMtxPrev);

	Mat34V rotMat;
	UnTransform3x3Ortho(rotMat, trailerMtxPrev, trailerMtxNew);
	rotMat.SetCol3(Vec3V(V_ZERO));

	UpdateInertiasAfterTeleport();

	// We also need to move any physical attachments along with the trailer, we're going to
	//  detach and reattach them so we need to cache them in an array so we don't change the
	//  attachments tree while traversing it
	if(GetChildAttachment())
	{
		atFixedArray<CPhysical *, MAX_ATTACHMENTS> attachedObjects;

		GetPhysicallyAttachedObjects((CPhysical*)GetChildAttachment(), attachedObjects);

		const int numAttachedObjects = attachedObjects.GetCount();

		for(unsigned index = 0; index < numAttachedObjects; index++)
		{
			CPhysical *attachedObject = attachedObjects[index];

			if(attachedObject)
			{
				Mat34V newMat = attachedObject->GetTransform().GetMatrix();
				const Vec3V posOffset = Transform(positionOffsetMtx, Subtract(newMat.GetCol3(), trailerMtxPrev.GetCol3()));
				Transform3x3(newMat, rotMat, newMat);
				newMat.SetCol3( rage::Add(newMat.GetCol3(), posOffset) );
				attachedObject->SetMatrix(MAT34V_TO_MATRIX34(newMat), true, true, true);
			}
		}
	}
}

void CTrailer::WarpParentToPosition()
{
	if( m_AttachedParent )
	{
		const int iThisBoneIndex = GetBoneIndex(TRAILER_ATTACH);
		const int iOtherBoneIndex = m_AttachedParent->GetBoneIndex(VEH_ATTACH);

		Mat34V parentMtxNew( V_IDENTITY );
		Mat34V trailerAttachBoneToInstMtx = GetSkeleton()->GetObjectMtx( iThisBoneIndex );
		Mat34V trailerAttachBoneToWorldMtx( V_IDENTITY );

		// What to do if global matrices aren't up to date?
		GetGlobalMtx( iThisBoneIndex, RC_MATRIX34( trailerAttachBoneToWorldMtx ) );
		m_AttachedParent->GetGlobalMtx( iOtherBoneIndex, RC_MATRIX34( parentMtxNew ) );

		// We want to set the trailer matrix such that the world space of the attach bone is equal to the cab's attach bone world matrix
		// - Could be written as matrices: [(trailer)(trailerBone)] = [(cab)(cabBone)]
		//  -- Then we right multiply both by (trailerBone)^-1 and notice that we already have the cabBone in world space
		//  -- Resulting in us setting (trailer) = (cabBoneInWorldSpace)(trailerBone)^-1
		Mat34V parentAttachBoneToInstMtx = m_AttachedParent->GetSkeleton()->GetObjectMtx( iOtherBoneIndex );

		// some of the assets have been created with the attachment bone the wrong way around
		if( parentAttachBoneToInstMtx.a().GetXf() * trailerAttachBoneToInstMtx.a().GetXf() < 0.0f )
		{
			trailerAttachBoneToInstMtx.GetCol0Ref() = trailerAttachBoneToInstMtx.GetCol0Ref() * ScalarV( -1.0f );
			trailerAttachBoneToInstMtx.GetCol1Ref() = trailerAttachBoneToInstMtx.GetCol1Ref() * ScalarV( -1.0f );
		}

		Vector3 vSep = VEC3V_TO_VECTOR3( trailerAttachBoneToWorldMtx.d() - parentMtxNew.d() );
		const float arbitraryAttachDistanceScale = 8.0f;
		const float warpRadius = Max( 2.0f, CVehicleTrailerAttachPoint::ms_fAttachRadius * arbitraryAttachDistanceScale );
		if(vSep.Mag2() < warpRadius * warpRadius)
		{
			return;
		}

		//Mat34V parentAttachBoneToInstMtxInverse;
		//InvertTransformOrtho( parentAttachBoneToInstMtxInverse, parentAttachBoneToInstMtx );
		//Transform( parentMtxNew, trailerAttachBoneToWorldMtx, parentAttachBoneToInstMtxInverse );
		Transform( parentMtxNew, trailerAttachBoneToWorldMtx, parentAttachBoneToInstMtx );

		//
		m_AttachedParent->SetMatrix( MAT34V_TO_MATRIX34( parentMtxNew ), true, true, true );

		Vector3 vecSetCoords = VEC3V_TO_VECTOR3( parentMtxNew.d() );

		m_AttachedParent->TeleportWithoutUpdateGadgets( vecSetCoords, camFrame::ComputeHeadingFromMatrix( MAT34V_TO_MATRIX34( parentMtxNew ) ), true, true );

		m_AttachedParent->UpdateGadgetsAfterTeleport( MAT34V_TO_MATRIX34( parentMtxNew ), false, true, false );
		m_AttachedParent->UpdateInertiasAfterTeleport();
	}
}

void CTrailer::GetPhysicallyAttachedObjects(CPhysical *attachedObj, atFixedArray<CPhysical *, MAX_ATTACHMENTS> &attachmentList)
{
    if(attachedObj)
    {
        fwAttachmentEntityExtension *attachExt = attachedObj->GetAttachmentExtension();
        Assertf(attachExt, "Attached child doesn't have an attachment extension?");

        if(attachExt)
        {
            if(attachExt->GetAttachState() == ATTACH_STATE_PHYSICAL)
            {
                attachmentList.Push(attachedObj);
            }

            if(attachedObj->GetSiblingAttachment())
            {
                GetPhysicallyAttachedObjects((CPhysical*)attachedObj->GetSiblingAttachment(), attachmentList);
            }

            if(attachedObj->GetChildAttachment())
            {
                GetPhysicallyAttachedObjects((CPhysical*)attachedObj->GetChildAttachment(), attachmentList);
            }
        }
    }
}

void CTrailer::AddCargoVehicle(u32 index, CVehicle* veh)
{
	if (Verifyf(index < MAX_CARGO_VEHICLES, "Invalid cargo vehicle index"))
	{
		Assertf(m_cargoVehicles[index] == NULL, "Vehicle alredy attached as cargo at index %d", index);
		m_cargoVehicles[index] = veh;
		m_cargoVehiclesAttached = true;
		if (veh)
		{
			veh->SetIsCargoVehicle(true);
			veh->m_nVehicleFlags.bCanMakeIntoDummyVehicle = true;
		}
	}
}

int	CTrailer::FindCargoVehicle(CVehicle* veh) const
{
	if (Verifyf(veh, "Invalid cargo vehicle pointer"))
	{
		for (s32 i = 0; i < MAX_CARGO_VEHICLES; ++i)
		{
			if (m_cargoVehicles[i] == veh)
			{
				return i;
			}
		}
	}
	return -1;
}

u32 CTrailer::RemoveCargoVehicle(CVehicle* veh)
{
	if (Verifyf(veh, "Invalid cargo vehicle pointer"))
	{
		for (s32 i = 0; i < MAX_CARGO_VEHICLES; ++i)
		{
			if (m_cargoVehicles[i] == veh)
			{
				m_cargoVehicles[i] = NULL;
				if (veh)
				{
					veh->SetIsCargoVehicle(false);
					if(veh->InheritsFromBoat())
					{
						veh->m_nVehicleFlags.bCanMakeIntoDummyVehicle = false;
					}
				}
				return i;
			}
		}
	}
	return (u32)-1;
}

void CTrailer::DetachCargoVehicles()
{
	for (s32 i = 0; i < MAX_CARGO_VEHICLES; ++i)
	{
		if (m_cargoVehicles[i])
		{
			Assertf(IsNetworkClone() || m_cargoVehicles[i]->GetIsPhysicalAParentAttachment(this), "Cargo vehicle at index %d isn't attached to current trailer", i);
			m_cargoVehicles[i]->DetachFromParent(/*DETACH_FLAG_NO_COLLISION_UNTIL_CLEAR | */DETACH_FLAG_APPLY_VELOCITY | DETACH_FLAG_ACTIVATE_PHYSICS);
			m_cargoVehicles[i]->SetParentTrailer(NULL);
			m_cargoVehicles[i]->SetIsCargoVehicle(false);
			if(m_cargoVehicles[i]->InheritsFromBoat())
			{
				m_cargoVehicles[i]->m_nVehicleFlags.bCanMakeIntoDummyVehicle = false;
				CVehicleFactory::GetFactory()->DetachedBoatOnTrailer();
			}
			m_cargoVehicles[i] = NULL;
		}
	}
	m_cargoVehiclesAttached = false;
}

void CTrailer::DetachFromParent(u16 nDetachFlags)
{
#if __ASSERT
	// Make sure we aren't going to leak constraints
	// If correct attach flags are set then CPhysical will clean us up
	Assert( !m_posConstraintHandle.IsValid() || GetAttachFlags() & ATTACH_FLAG_POS_CONSTRAINT);
	Assert( !m_rotXConstraintHandle.IsValid() || GetAttachFlags() & ATTACH_FLAG_ROT_CONSTRAINT);
	Assert( !m_rotYConstraintHandle.IsValid() || GetAttachFlags() & ATTACH_FLAG_ROT_CONSTRAINT);
	Assert( !m_rotZConstraintHandle.IsValid() || GetAttachFlags() & ATTACH_FLAG_ROT_CONSTRAINT);

#endif

	if(GetCurrentPhysicsInst())
	{
		GetCurrentPhysicsInst()->SetInstFlag(phInst::FLAG_OPTIONAL_ITERATIONS, false);
	}

	if(GetAttachParent() && GetAttachParent()->GetCurrentPhysicsInst())
	{
		GetAttachParent()->GetCurrentPhysicsInst()->SetInstFlag(phInst::FLAG_OPTIONAL_ITERATIONS, false);
	}

	if(m_posConstraintHandle.IsValid())
	{
		PHCONSTRAINT->Remove(m_posConstraintHandle);
		m_posConstraintHandle.Reset();
	}

	if(m_rotXConstraintHandle.IsValid())
	{
		PHCONSTRAINT->Remove(m_rotXConstraintHandle);
		m_rotXConstraintHandle.Reset();
	}

	if(m_rotYConstraintHandle.IsValid())
	{
		PHCONSTRAINT->Remove(m_rotYConstraintHandle);
		m_rotYConstraintHandle.Reset();
	}

	if(m_rotZConstraintHandle.IsValid())
	{
		PHCONSTRAINT->Remove(m_rotZConstraintHandle);
		m_rotZConstraintHandle.Reset();
	}

	// Update the trucks vehicle gadget so it knows its no longer attached to this trailer.
	CPhysical* pAttachParent = (CPhysical *) GetAttachParent();	
	if(pAttachParent)
	{
		Assert(pAttachParent->GetIsTypeVehicle());
		CVehicle* pParentVehicle = static_cast<CVehicle*>(pAttachParent);
        
		// Could make it so people have to call the vehicle gadget to disconnect but that might be confusing
		for(s32 i = 0; i < pParentVehicle->GetNumberOfVehicleGadgets(); i++)
		{
			CVehicleGadget *pVehicleGadget = pParentVehicle->GetVehicleGadget(i);

			if(pVehicleGadget->GetType() == VGT_TRAILER_ATTACH_POINT)
			{
				CVehicleTrailerAttachPoint *pTrailerAttachPoint = static_cast<CVehicleTrailerAttachPoint*>(pVehicleGadget);

				pTrailerAttachPoint->SetAttachedTrailer(NULL);
			}
			else if(pVehicleGadget->GetType() == VGT_PICK_UP_ROPE || pVehicleGadget->GetType() == VGT_PICK_UP_ROPE_MAGNET)
			{
				// if the trail is attached to the grappling hook, notify the detachment
				CVehicleGadgetPickUpRope *pPickUpRope = static_cast<CVehicleGadgetPickUpRope*>(pVehicleGadget);
				if(!pPickUpRope->IsDetaching())
				{
					pPickUpRope->DetachEntity();
				}
			}
		}
	}

	//DetachCargoVehicles();

	CPhysical::DetachFromParent(nDetachFlags);

	m_AttachedParent = nullptr;
	m_isAttachedToParent = false;
	SetCollidedWithAttachParent(false);
}

float CTrailer::GetTotalMass()
{
    float fTotalMass = GetMass();

    if(GetChildAttachment())
    {
        CPhysical* pNextChild = (CPhysical *) GetChildAttachment();
        while(pNextChild)
        {
            fwAttachmentEntityExtension *nextChildAttachExt = pNextChild->GetAttachmentExtension();
            Assertf(nextChildAttachExt, "Attached child doesn't have an attachment extension?");

            if(nextChildAttachExt->GetAttachState() == ATTACH_STATE_PHYSICAL)
            {
                
                if(nextChildAttachExt->GetNumConstraintHandles() > 0)
                {
                    float avgInvMassRatio = 0.0f;
                    for(int  i = 0; i < nextChildAttachExt->GetNumConstraintHandles(); i++)
                    {
                        if( phConstraintBase* pConstraint = PHCONSTRAINT->GetTemporaryPointer(nextChildAttachExt->GetConstraintHandle(i)) )
                        {
                            avgInvMassRatio += pConstraint->GetMassInvScaleB();
                        }
                    }
                    avgInvMassRatio = avgInvMassRatio/nextChildAttachExt->GetNumConstraintHandles();
                    fTotalMass += pNextChild->GetMass()*avgInvMassRatio;
                }
                else
                {
                    fTotalMass += pNextChild->GetMass();
                }

                pNextChild = (CPhysical *) nextChildAttachExt->GetSiblingAttachment();
            }
            else
            {
                break;
            }

        }
    }

    return fTotalMass;
}

void CTrailer::SetInverseMassScale(float fInvMass)
{
	phConstraintBase* pPosConstraint = PHCONSTRAINT->GetTemporaryPointer(m_posConstraintHandle);
	phConstraintRotation* pRotConstraintX = static_cast<phConstraintRotation*>( PHCONSTRAINT->GetTemporaryPointer(m_rotXConstraintHandle) );
	phConstraintRotation* pRotConstraintY = static_cast<phConstraintRotation*>( PHCONSTRAINT->GetTemporaryPointer(m_rotYConstraintHandle) );
	phConstraintRotation* pRotConstraintZ = static_cast<phConstraintRotation*>( PHCONSTRAINT->GetTemporaryPointer(m_rotZConstraintHandle) );

	if( pPosConstraint )
	{
		pPosConstraint->SetMassInvScaleB(fInvMass);
	}

	if( pRotConstraintX )
	{
		pRotConstraintX->SetMassInvScaleB(fInvMass);
	}

	if( pRotConstraintY )
	{
		pRotConstraintY->SetMassInvScaleB(fInvMass);
	}

	if( pRotConstraintZ )
	{
		pRotConstraintZ->SetMassInvScaleB(fInvMass);
	}
}

ePrerenderStatus CTrailer::PreRender(const bool bIsVisibleInMainViewport)
{
    m_trailerLegs.PreRender(this);

    return CAutomobile::PreRender(bIsVisibleInMainViewport);
}

void CTrailer::PreRender2(const bool bIsVisibleInMainViewport)
{
	CVehicle::PreRender2(bIsVisibleInMainViewport);

	s32 lightFlags = 0;

	CPhysical* pAttachParent = (CPhysical *) GetAttachParent();	

	if(pAttachParent)
	{
		Assert(pAttachParent->GetIsTypeVehicle());
		CVehicle* pParentVehicle = static_cast<CVehicle*>(pAttachParent);
	
		if(pParentVehicle->IsEngineOn())//if the engines on make sure the trailers lights are on
			lightFlags |= LIGHTS_ALWAYS_HAVE_POWER;

		if(pParentVehicle->GetDriver())
			lightFlags |=LIGHTS_IGNORE_DRIVER_CHECK;
	}

	DoVehicleLights(lightFlags);
}

bool CTrailer::UsesDummyPhysics(const eVehicleDummyMode vdm) const
{
	switch(vdm)
	{
		case VDM_REAL:
			return false;
		case VDM_DUMMY:
		{
			if(GetAttachParent())
				return false;
			return true;
		}
		case VDM_SUPERDUMMY:
			return true;
		default:
			Assert(false);
			return false;
	}
}

bool CTrailer::GetCanMakeIntoDummy(eVehicleDummyMode dummyMode)
{
	// JB: Trailers cannot switch to VDM_DUMMY currently.
	// This is because the handling physics goes crazy when running with the m_pDummyInst active.
	// I tried enabling dummy mode whilst keeping the fragInst active, but the bikes kept
	// getting stuck on the road collision (because they're running along the road nodes)
	if(dummyMode == VDM_DUMMY)
	{
		BANK_ONLY(SetNonConversionReason("Trailer cannot use VDM_DUMMY currently due to issues with attachments.");)
		return false;
	}
	return CVehicle::GetCanMakeIntoDummy(dummyMode);
}

void CTrailer::SwitchToFullFragmentPhysics(const eVehicleDummyMode prevMode)
{
	CVehicle::SwitchToFullFragmentPhysics(prevMode);

	// init constraints/attachments/etc
}

void CTrailer::SwitchToDummyPhysics(const eVehicleDummyMode prevMode)
{
	Assertf(false, "Trailers cannot convert into dummy currently.");

	// remove constraints/attachments/etc

	CVehicle::SwitchToDummyPhysics(prevMode);
}

void CTrailer::SwitchToSuperDummyPhysics(const eVehicleDummyMode prevMode)
{
	// remove constraints/attachments/etc

	CVehicle::SwitchToSuperDummyPhysics(prevMode);
}

bool CTrailer::TryToMakeFromDummy(const bool bSkipClearanceTest)
{
	if(GetDummyAttachmentParent())
	{
		Assertf(false, "You must not call TryToMakeFromDummy() on an attached CTrailer; instead call it on the parent vehicle, which will ensure that the attached trailer converts as well.");
		return false;
	}
	else
	{
		return CAutomobile::TryToMakeFromDummy(bSkipClearanceTest);
	}
}

dev_float dfTrailerContactDepthToDetach = 0.5f;

void CTrailer::ProcessPreComputeImpacts(phContactIterator impacts)
{
	CAutomobile::ProcessPreComputeImpacts(impacts);
	bool impactsDisabled = false;

	const int numTurretBones = 6;
	eHierarchyId trailerLargeTurretBones[ numTurretBones ] = { VEH_TURRET_2_MOD, VEH_TURRET_3_MOD, VEH_TURRET_2_BASE, VEH_TURRET_3_BASE, VEH_TURRET_2_BARREL, VEH_TURRET_3_BARREL };
	bool isTrailerLarge = MI_TRAILER_TRAILERLARGE.IsValid() && GetModelIndex() == MI_TRAILER_TRAILERLARGE;

	if(GetAttachParent() && GetAttachParent()->GetCurrentPhysicsInst())
	{
		impacts.Reset();
		while(!impacts.AtEnd())
		{
			if(!impacts.IsDisabled() && !impacts.IsConstraint())
			{
				phInst* pOtherInstance = impacts.GetOtherInstance();
				if(pOtherInstance == GetAttachParent()->GetCurrentPhysicsInst())
				{
					if(impacts.GetDepth() > dfTrailerContactDepthToDetach)
					{
                        CPhysical *pAttachParent = (CPhysical *)GetAttachParent();

						if(!NetworkUtils::IsNetworkCloneOrMigrating(pAttachParent))
						{
							DetachFromParent(0);
						}
                        else
                        {
                            // send an event to the owner of the attached entity requesting they detach
				            CRequestDetachmentEvent::Trigger(*this);
                        }
						break;
					}
				}
			}
			impacts++;
		}
	}

	// Disable the push collision when player vaulting through the trailer, B*1747991
	impacts.Reset();
	while(!impacts.AtEnd())
	{
		if(!impacts.IsDisabled() && !impacts.IsConstraint())
		{
			phInst* pOtherInstance = impacts.GetOtherInstance();
			CEntity* pOtherEntity = CPhysics::GetEntityFromInst(pOtherInstance);
			if(pOtherEntity && pOtherEntity->GetIsTypePed())
			{
				CPed *pOtherPed = static_cast<CPed*>(pOtherEntity);
				if(pOtherPed->GetPedResetFlag(CPED_RESET_FLAG_IsVaulting))
				{
					impacts.SetDepth(0.0f);
				}
			}

			if( GetAttachParent() && GetAttachParent()->GetCurrentPhysicsInst() &&
				( !pOtherEntity || pOtherEntity->GetIsAnyFixedFlagSet() ) &&
				  isTrailerLarge ) 
			{
				impacts.SetFriction( 0.01f );
			}

			if( pOtherEntity &&
				pOtherEntity->GetIsTypeVehicle() )
			{
				if( isTrailerLarge &&
					MI_BIKE_OPPRESSOR.IsValid() && 
					pOtherEntity->GetModelIndex() == MI_BIKE_OPPRESSOR )
				{
					Vec3V myNormal;
					impacts.GetMyNormal( myNormal );

					if( myNormal.GetZf() > 0.8f )
					{
						if( GetVelocity().Mag2() > 10.0f )
						{
							CBike* bike		= static_cast< CBike* >( pOtherEntity );
							CPed* driver	= bike->GetDriver();

							if( driver )
							{
								driver->KnockPedOffVehicle( true );
							}
						}
					}
				}
			}

			if( m_bDisableCollisionsWithObjectsBasicallyAttachedToParent &&
				pOtherEntity &&
				pOtherEntity->GetIsTypeObject() )
			{
				if( GetAttachParent() &&
					pOtherEntity->GetAttachParent() == GetAttachParent() )
				{
					impacts.DisableImpact();
					continue;
				}
			}

			if( isTrailerLarge )
			{
				static dev_float forwardNormalLimit = -0.5f;

				Vec3V myNormal;
				impacts.GetMyNormal( myNormal );
				float forwardNormal = Dot( GetTransform().GetForward(), myNormal ).Getf();

				if( forwardNormal < forwardNormalLimit )
				{
					for( int i = 0; i < numTurretBones; i++ )
					{
						fragInst* pFragInst = GetVehicleFragInst();

						if( pFragInst )
						{
							int turretComponent = pFragInst->GetComponentFromBoneIndex( GetBoneIndex( trailerLargeTurretBones[ i ] ) );

							if( turretComponent == impacts.GetMyComponent() )
							{
								impacts.DisableImpact();
								continue;
							}
						}
					}
				}

				if( m_isAttachedToParent &&
					( !pOtherEntity ||
					  pOtherEntity->GetIsAnyFixedFlagSet() ) )
				{
					Vec3V vMyPosition = impacts.GetMyPosition();
					vMyPosition = GetTransform().UnTransform( vMyPosition ); 
					static dev_float minZForNormalFlattening = 1.5f;
					static dev_float minZForUpright = 0.7f;
					static dev_float maxZToDisableNormal = -0.8f;

					if( vMyPosition.GetZf() > minZForNormalFlattening &&
						GetTransform().GetUp().GetZf() > minZForUpright )
					{
						Vec3V myNormal; 
						impacts.GetMyNormal( myNormal );
						
						if( myNormal.GetZf() < maxZToDisableNormal ||
							myNormal.GetZf() > 0.0f ) // we should never get any upwards facing normals on the top of the trailer.
						{
							impacts.DisableImpact();
							impactsDisabled = true;
						}
						else if( m_bDisablingImpacts || impactsDisabled )
						{
							impacts.DisableImpact();
							impactsDisabled = true;
						}
					}
				}
			}
		}
		impacts++;
	}

	m_bDisablingImpacts = impactsDisabled;
}


///////////////////////////////////////////////////////////////////////////////////
// CalcVirtualFrontWheelAtAttachmentPoint
// 
// Calculates the point in world space where the trailer attachment
// point projected down to the rear wheel contact height.  Used to 
// calculate the forward vector for super-dummy trailers.
//
// vParentAttachPoint = attachment point on parent, global
// vRearWheelContactPoint = average contact point of rear wheels
// vRearWheelDesired = desired position of a rear wheel (arbitrarily chosen), to define local height of trailer
//
Vec3V_Out CTrailer::CalcTrailerVirtualWheelAtAttachmentPoint(Vec3V_In vParentAttachPoint, Vec3V_In vRearWheelContactPoint, Vec3V_In vRearWheelDesired, float fWheelRadius) const
{
	Matrix34 trailerAttachMtx;
	GetGlobalMtx(GetBoneIndex(TRAILER_ATTACH),trailerAttachMtx);
	Vec3V vTrailerAttach = RCC_VEC3V(trailerAttachMtx.d);

	Vec3V vToRearWheelDesired = Subtract(vRearWheelDesired,vTrailerAttach);

	ScalarV fVirtualWheelContactOffset = Abs(Dot(vToRearWheelDesired,GetTransform().GetC()));
	fVirtualWheelContactOffset += ScalarV(fWheelRadius);

	// The calculation ...

	// r = height of vehicle attachment above rear wheel contact in trailer space (should only vary with load and a little with orientation)
	ScalarV r(fVirtualWheelContactOffset);

	// v = vRearWheelContactPoint - vParentAttachPoint
	Vec3V v = Subtract(vRearWheelContactPoint,vParentAttachPoint);
	ScalarV vMag = Mag(v);
	if((vMag < r).Getb())
	{
		// Degenerate case... just point at vA for now
		return vParentAttachPoint;
	}
	Vec3V vNorm = v / vMag;

	// We are trying to calculate vP, which is on the circle of radius r from vA and vW->vP is tangent to the circle
	// I.e. we are trying to calculate the direction the trailer should be oriented toward.

	// x = distance along v from vA to vP (this is the core unknown calculation)
	ScalarV x = square(r) / vMag;

	// w = v X world up (i.e. horizontal perp to v)
	Vec3V w = CrossZAxis(v);

	// u = v X w (i.e. vertical plane down perp to v)
	Vec3V u = Cross(v,w);
	if(u.GetZf()>0.0f)
	{
		u = Negate(u);
	}
	Vec3V uNorm = Normalize(u);

	// y = distance along u from vA to vP
	ScalarV y (sqrtf((square(r)-square(x)).Getf()));

	Vec3V vP = vNorm * x + uNorm * y;
	vP += vParentAttachPoint;

#if DEBUG_DRAW && __BANK
	static bool bDebugDraw = false;
	if(bDebugDraw)
	{
		grcDebugDraw::Cross(vParentAttachPoint,0.3f,Color_red,-1);
		grcDebugDraw::Cross(vTrailerAttach,0.3f,Color_purple,-1);
		grcDebugDraw::Line(vParentAttachPoint,vTrailerAttach,Color_white,Color_white,-1);
		grcDebugDraw::Line(vRearWheelDesired,vRearWheelContactPoint,Color_yellow,Color_blue,-1);
		grcDebugDraw::Line(vRearWheelContactPoint,vP,Color_pink,Color_pink,-1);
	}
#endif

	return vP;
}

///////////////////////////////////////////////////////////////////////////////////
// FUNCTION : BlowUpCar
// PURPOSE :  Does everything needed to destroy a car
///////////////////////////////////////////////////////////////////////////////////
void CTrailer::BlowUpCar( CEntity *pCulprit, bool bInACutscene, bool bAddExplosion, bool bNetCall, u32 weaponHash, bool bDelayedExplosion)
{
	if (GetStatus() == STATUS_WRECKED)
	{
		return;	// Don't blow up a 2nd time
	}

	// Need to try to swap the whole vehicle setup out of dummy mode for blowing up an attached trailer
	if(CVehicle* parentVeh = GetDummyAttachmentParent())
	{
		parentVeh->MakeRealOrFixForExplosion();
	}

	DetachFromParent(0);

	if (m_nPhysicalFlags.bNotDamagedByAnything)
	{
		return;	//	If the flag is set for don't damage this trailer (usually set by script)
	}

	//if the trailer has any petrol blow it up
	CAutomobile::BlowUpCar( pCulprit, bInACutscene, bAddExplosion && (GetVehicleDamage()->GetPetrolTankLevel() > 0.0f), bNetCall, weaponHash, bDelayedExplosion);
	
	// Modifying the CG of articulated stuff will cause lots of side effects. 
	if(!IsColliderArticulated() && AssertVerify(pHandling))
	{
		Vec3V vCentreOfMassOffset = pHandling->m_vecCentreOfMassOffset;
		phArchetype* pArch = GetVehicleFragInst()->GetArchetype();
		phBound* pBound = pArch->GetBound();
		Vec3V cgOff = pBound->GetCGOffset();
		cgOff = rage::Add(cgOff, vCentreOfMassOffset);
		pBound->SetCGOffset(cgOff);
		if(phCollider* pCollider = GetCollider())
		{
			// Moving the CG means the collider and instance are no longer aligned.
			pCollider->SetColliderMatrixFromInstance();
		}
	}
	// This is never actually going to work - Either an InitArticulated or BreakOffAboveGroup 
	// will reset COM to the fragTypeLOD or recalculate COM based on remaining child masses =/
	// - I think the only way around would be to break everything off right before setting the COM here and somehow prevent it from ever becoming articulated as well...

	// No longer attaches automatically - we're broken
	m_nVehicleFlags.bAutomaticallyAttaches = false;
	m_nVehicleFlags.bScansWithNonPlayerDriver = false;

	//m_extendingSidesTargetRatio = m_extendingSidesRatio;
}

//void CTrailer::UpdateExtendingSides( float fTimeStep )
//{
//	TUNE_GROUP_FLOAT( VEHICLE_TRAILER_EXTENDABLE_SIDES, extendingSpeed, 5.0f, 0.0f, 20.0f, 0.1f );
//	float speed = extendingSpeed * fTimeStep;
//
//	int leftBoneIndex	= GetBoneIndex( VEH_EXTENDABLE_SIDE_L );
//	int rightBoneIndex	= GetBoneIndex( VEH_EXTENDABLE_SIDE_R );
//	float previousRatio = m_extendingSidesRatio;
//
//	if( leftBoneIndex != -1 )
//	{
//		SlideMechanicalPart( leftBoneIndex, m_extendingSidesTargetRatio, m_extendingSidesRatio, Vector3( -1.0f, 0.0f, 0.0f ), speed );
//	}
//	if( rightBoneIndex != -1 )
//	{
//		SlideMechanicalPart( rightBoneIndex, m_extendingSidesTargetRatio, previousRatio, Vector3( 1.0f, 0.0f, 0.0f ), speed );
//		m_extendingSidesRatio = previousRatio;
//	}
//
//	if( m_extendingSidesTargetRatio != m_extendingSidesRatio )
//	{
//		ActivatePhysics();
//
//		if(fragInst* pInst = GetFragInst())
//		{
//			pInst->PoseBoundsFromSkeleton( true, true, false );
//		}
//	}
//}

//void CTrailer::SetExendableRatio( float targetRatio )
//{
//	m_extendingSidesTargetRatio = targetRatio;
//	UpdateExtendingSides( 100.0f );
//}

#if __BANK
bank_bool CTrailer::ms_bDebugDrawTrailers = false;
void CTrailer::InitWidgets(bkBank* pBank)
{
	if(pBank)
	{
		pBank->PushGroup("Trailers");
			pBank->AddToggle("Debug draw trailers",&ms_bDebugDrawTrailers);
		pBank->PopGroup();
	}
}
#endif // __BANK

