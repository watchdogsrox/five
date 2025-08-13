#include "vehicles/LandingGear.h"

// Rage includes 
#include "crskeleton/jointdata.h"
#include "pharticulated/articulatedcollider.h"
#include "phbound/boundcomposite.h"

// Game headers
#include "Vehicles/Heli.h"
#include "vehicles/Planes.h"
#include "vehicles/vehicle.h"
#include "vehicles/vehicle_channel.h"
#include "physics/gtaInst.h"
#include "debug/DebugScene.h"
#include "audio/planeaudioentity.h"
#include "audio/heliaudioentity.h"
#include "modelinfo/ModelSeatInfo.h"
#include "system/controlMgr.h"
#include "Peds/ped.h"

VEHICLE_OPTIMISATIONS()

// Enough room for 160 aeroplanes
#define LANDING_GEAR_PART_POOL (3 * 160)

#define MAX_LANDING_GEAR_PART_SIZE (sizeof(CLandingGearPartPhysicalRot))
CompileTimeAssert(sizeof(CLandingGearPartBase) <= MAX_LANDING_GEAR_PART_SIZE);
CompileTimeAssert(sizeof(CLandingGearPartPhysical) <= MAX_LANDING_GEAR_PART_SIZE);
CompileTimeAssert(sizeof(CLandingGearPartPhysicalRot) <= MAX_LANDING_GEAR_PART_SIZE);

FW_INSTANTIATE_BASECLASS_POOL_SPILLOVER(CLandingGearPartBase, LANDING_GEAR_PART_POOL, 0.74f, atHashString("Landing gear parts",0x45aaa13a), MAX_LANDING_GEAR_PART_SIZE);

// Deploy/retract rumble parameters.
u32 CLandingGear::ms_deployingRumbleDuration = 10;
float CLandingGear::ms_deployingRumbleIntensity = 0.2f;
u32 CLandingGear::ms_deployedRumbleDuration = 100;
float CLandingGear::ms_deployedRumbleIntensity = 0.9f;

u32 CLandingGear::ms_retractingRumbleDuration = 10;
float CLandingGear::ms_retractingRumbleIntensity = 0.2f;
u32 CLandingGear::ms_retractedRumbleDuration = 100;
float CLandingGear::ms_retractedRumbleIntensity = 0.9f;

//////////////////////////////////////////////////////////////////////////
// Class CLandingGearPartPhysical
//////////////////////////////////////////////////////////////////////////
CLandingGearPartPhysical::CLandingGearPartPhysical()
{
	m_iFragChild = -1;

	// Latch strength should be set to -1 so we start off latched
	m_bIsLatched = true;
	m_vLandingGearDeformation = VEC3_ZERO;
	m_vRetractedWheelDeformation = VEC3_ZERO;
	m_vRetractedWheelOffset = VEC3_ZERO;
	m_iWheelIndex = -1;
	m_eId = LANDING_GEAR_F;
}

void CLandingGearPartPhysical::InitPhys(CVehicle* pParent, eHierarchyId nId)
{
	m_eId = nId;
	int iBoneIndex = pParent->GetBoneIndex(nId);

	if(iBoneIndex > -1)
	{
		m_iFragChild = pParent->GetVehicleFragInst()->GetComponentFromBoneIndex(iBoneIndex);
	}
}

void CLandingGearPartPhysical::InitWheel(CVehicle *pParent)
{
	if( m_iFragChild == -1 )
	{
		return;
	}
	const fragTypeChild* child = pParent->GetFragInst()->GetTypePhysics()->GetChild(m_iFragChild);
	int nBoneIndex = pParent->GetFragInst()->GetType()->GetBoneIndexFromID(child->GetBoneID());

	if(nBoneIndex > -1)
	{
		for(int iWheelIndex = 0; iWheelIndex < pParent->GetNumWheels(); iWheelIndex++)
		{
			Assert(pParent->GetWheel(iWheelIndex));
			int iWheelBoneIndex = pParent->GetBoneIndex(pParent->GetWheel(iWheelIndex)->GetHierarchyId());
			if(iWheelBoneIndex > -1)
			{
				const crBoneData* pParentBoneData = pParent->GetVehicleFragInst()->GetType()->GetSkeletonData().GetBoneData(iWheelBoneIndex)->GetParent();
				if(pParentBoneData && pParentBoneData->GetIndex() == nBoneIndex)
				{
					m_iWheelIndex = iWheelIndex;
					break;
				}
			}
		}
	}

	if(GetWheelIndex() > -1)
	{
		Assert(pParent->GetWheel(GetWheelIndex()));
		int iWheelBoneIndex = pParent->GetBoneIndex(pParent->GetWheel(GetWheelIndex())->GetHierarchyId());
		if(iWheelBoneIndex > -1)
		{
			float fMin,fMax;
			Vector3 vAxis;
			CLandingGearPartPhysicalRot::FindDefaultJointLimits(pParent->GetVehicleFragInst(), m_iFragChild, fMin, fMax, &vAxis);
			float fClosedAngle = -fMin > fMax ? fMin : fMax;

			Vector3 vWheelPosLocal = RCC_VECTOR3(pParent->GetSkeletonData().GetBoneData(iWheelBoneIndex)->GetDefaultTranslation());

			const crBoneData* pParentBoneData = pParent->GetSkeletonData().GetBoneData(iWheelBoneIndex)->GetParent();
			while( pParentBoneData )
			{
				Matrix34 matParent;
				matParent.FromQuaternion(RCC_QUATERNION(pParentBoneData->GetDefaultRotation()));
				matParent.d = RCC_VECTOR3(pParentBoneData->GetDefaultTranslation());

				if(pParentBoneData && pParentBoneData->GetIndex() == nBoneIndex)
				{
					matParent.Rotate(vAxis, fClosedAngle);
				}

				matParent.Transform(vWheelPosLocal);

				pParentBoneData = pParentBoneData->GetParent();
			}

			m_vRetractedWheelOffset = vWheelPosLocal;
		}
	}
}

void CLandingGearPartPhysical::InitCompositeBound(CVehicle* pParent)
{
	phBoundComposite* pBoundComp = static_cast<phBoundComposite*>(pParent->GetVehicleFragInst()->GetArchetype()->GetBound());	
	Assert(pBoundComp);
	Assert(pParent->GetVehicleFragInst()->GetArchetype()->GetBound()->GetType()==phBound::COMPOSITE);

	if(m_iFragChild > -1)
	{
		pBoundComp->SetIncludeFlags(m_iFragChild, ArchetypeFlags::GTA_VEHICLE_INCLUDE_TYPES);
	}
}

void CLandingGearPartPhysical::LatchPart(CVehicle* pParent)
{
	fragInstGta* pFragInst = pParent->GetVehicleFragInst();
	Assert(pFragInst);
	Assert(pFragInst->GetType());

	vehicleAssertf(!m_bIsLatched || pParent->IsNetworkClone(),"Trying to latch twice");

	pFragInst->GetCacheEntry()->LazyArticulatedInit();

	if(m_iFragChild > -1 && !m_bIsLatched)
	{
		// We don't want to latch / unlatch landing gear if the gear's group has broken off
		int nGroup = pFragInst->GetTypePhysics()->GetAllChildren()[m_iFragChild]->GetOwnerGroupPointerIndex();
		if(pFragInst->GetCacheEntry()->GetHierInst()->groupBroken->IsClear(nGroup))
		{
			pFragInst->CloseLatchAbove(m_iFragChild);
		}
	}

	m_bIsLatched = true;

	// B*1816764 Restore the default matrix when latching the part. This can prevent the numerical error build up from deploying and retracting the landing gear over time
	int iBoneIndex = pParent->GetBoneIndex(m_eId);
	if(iBoneIndex > -1)
	{
		crSkeleton* pSkel = pParent->GetSkeleton();
		Mat34V& localMat = pSkel->GetLocalMtx(iBoneIndex);
		localMat = pSkel->GetSkeletonData().GetDefaultTransform(iBoneIndex);
		pSkel->PartialUpdate(iBoneIndex);
	}
}

void CLandingGearPartPhysical::UnlatchPart(CVehicle* pParent)
{
	fragInstGta* pFragInst = pParent->GetVehicleFragInst();
	Assert(pFragInst);

	vehicleAssertf(m_bIsLatched,"Trying to unlatch twice");

	pFragInst->GetCacheEntry()->LazyArticulatedInit();

	if(m_iFragChild > -1 && m_bIsLatched)
	{
		// We don't want to latch / unlatch landing gear if the gear's group has broken off
		int nGroup = pFragInst->GetTypePhysics()->GetAllChildren()[m_iFragChild]->GetOwnerGroupPointerIndex();
		if(pFragInst->GetCacheEntry()->GetHierInst()->groupBroken->IsClear(nGroup)
			&& pFragInst->GetCacheEntry()->IsGroupLatched(nGroup))
		{
			pFragInst->OpenLatchAbove(m_iFragChild);

			//B*2132587 Remove the allowed angluar penetration threshold for Helis' (swift) landing gears to avoid the visual pop when fully extracted.
			if(pParent->InheritsFromHeli() || pParent->GetModelIndex() == MI_PLANE_TITAN)
			{
				fragHierarchyInst* pHierInst = pFragInst->GetCacheEntry()->GetHierInst();

				phArticulatedCollider* pArticulatedCollider = pHierInst->articulatedCollider;
				if(pArticulatedCollider)
				{
					int linkIndex = pArticulatedCollider->GetLinkFromComponent(m_iFragChild);
					if(linkIndex > 0)
					{
						phJoint1Dof* p1DofJoint = NULL;
						phJoint* pJoint = &pHierInst->body->GetJoint(linkIndex - 1);
						if (pJoint && pJoint->GetJointType() == phJoint::JNT_1DOF)
							p1DofJoint = static_cast<phJoint1Dof*>(pJoint);

						if (physicsVerifyf(p1DofJoint,"Child is not connected by a 1 dof joint"))
						{
							float fMin, fMax;
							CLandingGearPartPhysicalRot::FindDefaultJointLimits(pFragInst, m_iFragChild, fMin, fMax);
							p1DofJoint->SetAngleLimits(fMin - phSimulator::GetAllowedAnglePenetration(),fMax + phSimulator::GetAllowedAnglePenetration());
						}
					}
				}
			}
		}
	}

	m_bIsLatched = false;
}



void CLandingGearPartPhysical::SetJointSwingFreely(fragInstGta* pFragInst, int iChildIndex)
{
	Assert(pFragInst);
	Assert(iChildIndex > -1);
	Assert(pFragInst->GetCacheEntry());

	fragHierarchyInst* pHierInst = pFragInst->GetCacheEntry()->GetHierInst();

	phArticulatedCollider* pArticulatedCollider = pHierInst->articulatedCollider;
	Assert(pArticulatedCollider);

	if(pArticulatedCollider == NULL)
	{
		return;
	}

	pFragInst->GetCacheEntry()->LazyArticulatedInit();

	int linkIndex = pArticulatedCollider->GetLinkFromComponent(iChildIndex);
	if(linkIndex > 0)
	{
		phJoint* pJoint = &pHierInst->body->GetJoint(linkIndex - 1);
		if (pJoint)
		{
			pJoint->SetDriveState(phJoint::DRIVE_STATE_FREE);
		}
	}
}

dev_float fRetractedWheelIntoWingsZBias = -0.05f;
void CLandingGearPartPhysical::ApplyDeformationToPart(CVehicle* pParentVehicle, const void *basePtr)
{
	if (pParentVehicle && pParentVehicle->GetFragInst() && pParentVehicle->GetFragInst()->GetType() && basePtr && m_iFragChild > -1)
	{
		const fragTypeChild* child = pParentVehicle->GetFragInst()->GetTypePhysics()->GetChild(m_iFragChild);
		int nBoneIndex = pParentVehicle->GetFragInst()->GetType()->GetBoneIndexFromID(child->GetBoneID());

		if(nBoneIndex > -1)
		{
			Vector3 basePos;
			pParentVehicle->GetDefaultBonePositionSimple(nBoneIndex, basePos);
			m_vLandingGearDeformation = VEC3V_TO_VECTOR3(pParentVehicle->GetVehicleDamage()->GetDeformation()->ReadFromVectorOffset(basePtr, VECTOR3_TO_VEC3V(basePos)));
		}

		m_vRetractedWheelDeformation = VEC3V_TO_VECTOR3(pParentVehicle->GetVehicleDamage()->GetDeformation()->ReadFromVectorOffset(basePtr, VECTOR3_TO_VEC3V(m_vRetractedWheelOffset)));
		if(pParentVehicle->InheritsFromPlane())
		{
			m_vRetractedWheelDeformation.z += fRetractedWheelIntoWingsZBias; // helps hiding wheels inside wings
		}
	}
}

void CLandingGearPartPhysical::ProcessPreRenderWheels(CVehicle *pParent)
{
	if(pParent && pParent->GetFragInst() && m_iFragChild > -1 && !pParent->GetFragInst()->GetChildBroken(m_iFragChild) && pParent->GetFragInst()->GetType() && pParent->GetSkeleton() && !m_vLandingGearDeformation.IsZero())
	{
		const fragTypeChild* child = pParent->GetFragInst()->GetTypePhysics()->GetChild(m_iFragChild);
		int nBoneIndex = pParent->GetFragInst()->GetType()->GetBoneIndexFromID(child->GetBoneID());

		if(nBoneIndex > -1)
		{
			crSkeleton* pSkel = pParent->GetSkeleton();
			Matrix34& landingGearMatrixLocalRef = pParent->GetLocalMtxNonConst(nBoneIndex);
			// Reset bone position
			const crBoneData* pBoneData = pSkel->GetSkeletonData().GetBoneData(nBoneIndex);
			landingGearMatrixLocalRef.d.Set(RCC_VECTOR3(pBoneData->GetDefaultTranslation()));
			// pre translate by deformation offset
			landingGearMatrixLocalRef.d.Add(m_vLandingGearDeformation);

			pSkel->PartialUpdate(nBoneIndex);
		}
	}
	else if( m_iFragChild > -1 &&
             pParent->GetFragInst()->GetChildBroken(m_iFragChild) )
	{
		pParent->SetWheelBroken( m_iWheelIndex );
	}
#if __DEV
	static bool bDrawWheelDeformation = false;
	if(bDrawWheelDeformation)
	{
 		Vec3V vRetractedPositionBeforeDeformation = pParent->GetTransform().Transform(VECTOR3_TO_VEC3V(m_vRetractedWheelOffset));
		grcDebugDraw::Sphere(vRetractedPositionBeforeDeformation, 0.25f, Color_green, false);

		Vec3V vRetractedPositionAfterDeformation = pParent->GetTransform().Transform(VECTOR3_TO_VEC3V(m_vRetractedWheelOffset + m_vRetractedWheelDeformation));
		grcDebugDraw::Sphere(vRetractedPositionAfterDeformation, 0.25f, Color_red, false);

		Vec3V vRetractedPositionWithLandingGearDeformation = pParent->GetTransform().Transform(VECTOR3_TO_VEC3V(m_vRetractedWheelOffset + m_vLandingGearDeformation));
		grcDebugDraw::Sphere(vRetractedPositionWithLandingGearDeformation, 0.25f, Color_blue, false);
	}
#endif //__DEV
}

Vector3 CLandingGearPartPhysical::GetWheelDeformation(const CVehicle *pParent) const
{
	// When wheel is deployed, the wheel deformation matches the landing gear deformation, as the landing gear mesh doesn't deform.
	// When wheel is retracted, the wheel deformation matches to the deformation at the retracted position, so wheel can follow with the wing mesh
	Assert(pParent->InheritsFromPlane() || (pParent->InheritsFromHeli() && static_cast<const CHeli* >( pParent )->HasLandingGear()));
	float fDeployRatio = 1.0f;
	
	if(pParent->InheritsFromPlane())
		fDeployRatio = static_cast<const CPlane *>(pParent)->GetLandingGear().GetGearDeployRatio();
	else if(pParent->InheritsFromHeli())
		fDeployRatio = static_cast<const CHeli *>(pParent)->GetLandingGear().GetGearDeployRatio();

	return m_vLandingGearDeformation * fDeployRatio + m_vRetractedWheelDeformation * (1.0f - fDeployRatio);
}


//////////////////////////////////////////////////////////////////////////
// Class CLandingGearPartRotPhysical
//////////////////////////////////////////////////////////////////////////

// Add a small overshoot amount to each limit; it won't really be allowed to overshoot because
// of the joint limits, but it gives the joint a chance to achieve the desired angle if it is
// an extremum.
dev_float fOvershootAmount = 0.1f;

bool CLandingGearPartPhysicalRot::Process(CVehicle* pParent, eLandingGearInstruction nInstruction, float& fCurrentOpenRatio)
{
	bool bWheelPositionChanged = false;
	fCurrentOpenRatio = 1.0f;

	if(m_iFragChild == -1 REPLAY_ONLY(|| CReplayMgr::IsReplayInControlOfWorld()))
	{
		return bWheelPositionChanged;
	}

	// We don't want to process landing gear if the gear's group has broken off
	fragInstGta* pFragInst = pParent->GetVehicleFragInst();
	Assert(pFragInst);
	int nGroup = pFragInst->GetTypePhysics()->GetAllChildren()[m_iFragChild]->GetOwnerGroupPointerIndex();
	if(pFragInst->GetCacheEntry()->GetHierInst()->groupBroken->IsSet(nGroup))
	{
		return bWheelPositionChanged;
	}

	switch(nInstruction)
	{
	case UNLOCK_DOWN:
		{
			fCurrentOpenRatio = 1.0f;
			UnlatchPart(pParent);
		}
		break;
	case LOCK_DOWN:
		{
			LatchPart(pParent);
		}
		break;
	case DRIVE_DOWN:
		{
			DriveToTargetOpenRatio(pParent,1.0f+fOvershootAmount,fCurrentOpenRatio);
			bWheelPositionChanged = true;
		}
		break;
	case DRIVE_UP:
		{
			DriveToTargetOpenRatio(pParent,0.0f-fOvershootAmount,fCurrentOpenRatio);
			bWheelPositionChanged = true;
		}
		break;
	case UNLOCK_UP:
		{
			fCurrentOpenRatio = 0.0f;
			UnlockPart(pParent);
		}
		break;
	case LOCK_UP:
		{
			LockPart(pParent);
			fCurrentOpenRatio = 0.0f;
		}
		break;
	case FORCE_UP:
		{
			Snap1DofJointToOpenRatio(pParent->GetVehicleFragInst(),m_iFragChild,0.0f);
			fCurrentOpenRatio = 0.0f;
			bWheelPositionChanged = true;
		}
		break;
	case FORCE_DOWN:
		{
			Snap1DofJointToOpenRatio(pParent->GetVehicleFragInst(),m_iFragChild,1.0f);
			fCurrentOpenRatio = 1.0f;
			bWheelPositionChanged = true;
		}
		break;
	case SWING_FREELY:
		{
			// Gear is free to move about
			// Joint modes are set in BreakGear() function
			// So no need to do any work here

			SetJointSwingFreely(pParent->GetVehicleFragInst(), m_iFragChild);
		}
		break;
	default:
		{
			vehicleAssertf(false,"Unhandled landing gear state");			
		}
		break;
	}

	return bWheelPositionChanged;
}

static dev_float sfGearSpeed = 1.0f;
static dev_float sfHeliGearSpeed = 0.2f;
static dev_float sfAvengerRearGearSpeed = 0.1f;
static dev_float sfStiffness = 0.5f;
static dev_float sfAngleStrength = 30.0f;
static dev_float sfSpeedStrength = 50.0f;
static dev_float sfMinAndMaxMuscleTorque = 100.0f;
static dev_float sfSpeedStrengthFadeOutAngle = PI / 45.0f;
void CLandingGearPartPhysicalRot::DriveToTargetOpenRatio(CVehicle* pParent, float fTargetOpenRatio, float& fCurrentOpenRatio)
{
	fragInst* pFragInst = pParent->GetVehicleFragInst();
	Assert(pFragInst->GetCacheEntry());	// All vehicles should be in the cache

	fragHierarchyInst* pHierInst = pFragInst->GetCacheEntry()->GetHierInst();
	Assert(pHierInst);
	Assert(m_iFragChild > -1);

	phArticulatedCollider* pArticulatedCollider = pHierInst->articulatedCollider;

	if(pArticulatedCollider == NULL)
	{
		return;
	}

	pFragInst->GetCacheEntry()->LazyArticulatedInit();

	int linkIndex = pArticulatedCollider->GetLinkFromComponent(m_iFragChild);
	if(linkIndex > 0)
	{
		phJoint1Dof* p1DofJoint = NULL;
		phJoint* pJoint = &pHierInst->body->GetJoint(linkIndex - 1);
		if (pJoint && pJoint->GetJointType() == phJoint::JNT_1DOF)
			p1DofJoint = static_cast<phJoint1Dof*>(pJoint);

		if(p1DofJoint)
		{

			// First figure out what our closed angle is
			// Assume open angle is 0
			float fMin,fMax;
			p1DofJoint->GetAngleLimits(fMin,fMax);

			float fClosedAngle = 0.0f;
			float fOpenAngle = 0.0f;
			if(-fMin > fMax)
			{
				fClosedAngle = fMin;
				fOpenAngle = fMax;
			}
			else
			{
				fClosedAngle = fMax;
				fOpenAngle = fMin;
			}

			float fTargetAngle = (1.0f - fTargetOpenRatio) * fClosedAngle;
			float fTargetSpeed = sfGearSpeed;

			if( pParent->GetVehicleType() == VEHICLE_TYPE_HELI )
			{
				fTargetSpeed = sfHeliGearSpeed;
			}
			if( m_eId != LANDING_GEAR_F &&
				pParent->GetModelIndex() == MI_PLANE_AVENGER )
			{
				fTargetSpeed = sfAvengerRearGearSpeed;
			}

			p1DofJoint->SetMuscleTargetAngle(fTargetAngle);

			// Set the strength and stiffness
			// Done here for tweaking
			// Todo: move to Init()

			// Need to scale some things by inertia
			const float fInvAngInertia = p1DofJoint->ResponseToUnitTorque(pHierInst->body);
			float fMinMaxTorque = sfMinAndMaxMuscleTorque / fInvAngInertia;
			// is it possible (or better) to get the inertia from the FragType?

			p1DofJoint->SetStiffness(sfStiffness);		// If i set this to >= 1.0f the game crashes with a NAN root link position
			p1DofJoint->SetMinAndMaxMuscleTorque(fMinMaxTorque);
			p1DofJoint->SetMuscleAngleStrength(sfAngleStrength);
			p1DofJoint->SetMuscleSpeedStrength(sfSpeedStrength);
			p1DofJoint->SetDriveState(phJoint::DRIVE_STATE_ANGLE);

			float fCurrentAngle = p1DofJoint->GetAngle(pHierInst->body);

			static dev_bool bSetTargetSpeed = true;
			if(bSetTargetSpeed)
			{
				// Smooth the desired speed out close to the target
				float fAngleDiff = fTargetAngle - fCurrentAngle;
				fAngleDiff = fwAngle::LimitRadianAngle(fAngleDiff);
				float fJointSpeedTarget = 0.0f;				
				fJointSpeedTarget = rage::Clamp(fTargetSpeed*fAngleDiff/sfSpeedStrengthFadeOutAngle,-fTargetSpeed,fTargetSpeed);

				// If this is set to 0 then we get damping
				// this gives us damping
				p1DofJoint->SetMuscleTargetSpeed(fJointSpeedTarget);
				p1DofJoint->SetDriveState(phJoint::DRIVE_STATE_ANGLE_AND_SPEED);
			}	

			if(fabs(fCurrentAngle) < fabs(fOpenAngle))// if the joint is past its lower limit clamp to 0 so we get a slightly more accurate ratio.
			{
				fCurrentAngle = 0.0f;
			}

			fCurrentOpenRatio = fCurrentAngle / fClosedAngle;
			fCurrentOpenRatio = 1.0f - fCurrentOpenRatio;

		}
	}
}

void CLandingGearPartPhysicalRot::LockPart(CVehicle* pParent)
{
	Freeze1DofJointInCurrentPosition(pParent->GetVehicleFragInst(),m_iFragChild);
}

void CLandingGearPartPhysicalRot::UnlockPart(CVehicle* pParent)
{
	RestoreDefaultJointLimits(pParent->GetVehicleFragInst(), m_iFragChild);
}

void CLandingGearPartPhysicalRot::FindDefaultJointLimits(fragInstGta* pFragInst, int iChildIndex, float &fMin, float &fMax, Vector3 *pAxis)
{
	// To restore the default joint limits we look at the rotational limits on the skeleton data
	crSkeletonData* pSkeletonData = pFragInst->GetType()->GetCommonDrawable()->GetSkeletonData();
	Assert(pSkeletonData);

	const crJointData* pJointData = pFragInst->GetType()->GetCommonDrawable()->GetJointData();
	Assert(pJointData);

	Assert(iChildIndex < pFragInst->GetTypePhysics()->GetNumChildren());

	int iBoneIndex = pFragInst->GetType()->GetBoneIndexFromID(pFragInst->GetTypePhysics()->GetAllChildren()[iChildIndex]->GetBoneID());

	Assert(iBoneIndex > -1);

	crBoneData* pBoneData = pSkeletonData->GetBoneData(iBoneIndex);
	Assert(pBoneData);

	Assert(pBoneData->GetDofs() & (crBoneData::HAS_ROTATE_LIMITS));

	bool hasXLimits, hasYLimits, hasZLimits, hasXTLimits, hasYTLimits, hasZTLimits;
	pFragInst->GetType()->HasMotionLimits(*pBoneData, *pJointData, hasXLimits, hasYLimits, hasZLimits, hasXTLimits, hasYTLimits, hasZTLimits);

	// This assumes that only one joint limit is set	
	Vector3 vLimitDir(hasXLimits ? 1.0f : 0.0f,
		hasYLimits ? 1.0f : 0.0f,
		hasZLimits ? 1.0f : 0.0f);

	Vec3V minimumRot, maximumRot;
	pJointData->ConvertToEulers(*pBoneData, minimumRot, maximumRot);
	fMin = RCC_VECTOR3(minimumRot).Dot(vLimitDir);
	fMax = RCC_VECTOR3(maximumRot).Dot(vLimitDir);
	if(pAxis)
	{
		*pAxis = vLimitDir;
	}
}

void CLandingGearPartPhysicalRot::RestoreDefaultJointLimits(fragInstGta* pFragInst, int iChildIndex)
{
	Assert(pFragInst);
	Assert(iChildIndex > -1);
	Assert(pFragInst->GetCacheEntry());

	fragHierarchyInst* pHierInst = pFragInst->GetCacheEntry()->GetHierInst();

	phArticulatedCollider* pArticulatedCollider = pHierInst->articulatedCollider;
	Assert(pArticulatedCollider);

	if(pArticulatedCollider == NULL)
	{
		return;
	}

	pFragInst->GetCacheEntry()->LazyArticulatedInit();

	int linkIndex = pArticulatedCollider->GetLinkFromComponent(iChildIndex);
	if(linkIndex > 0)
	{
		phJoint1Dof* p1DofJoint = NULL;
		phJoint* pJoint = &pHierInst->body->GetJoint(linkIndex - 1);
		if (pJoint && pJoint->GetJointType() == phJoint::JNT_1DOF)
			p1DofJoint = static_cast<phJoint1Dof*>(pJoint);

		if (physicsVerifyf(p1DofJoint,"Child is not connected by a 1 dof joint"))
		{
			float fMin, fMax;
			CLandingGearPartPhysicalRot::FindDefaultJointLimits(pFragInst, iChildIndex, fMin, fMax);
			CEntity * pParentEntity = CPhysics::GetEntityFromInst(pFragInst);

			//B*2132587 Remove the allowed angluar penetration threshold for Helis' (swift) landing gears to avoid the visual pop when fully extracted.
			if(pParentEntity && pParentEntity->GetIsTypeVehicle() && static_cast<CVehicle *>(pParentEntity)->InheritsFromHeli())
			{
				p1DofJoint->SetAngleLimits(fMin - phSimulator::GetAllowedAnglePenetration(),fMax + phSimulator::GetAllowedAnglePenetration());
			}
			else // planes
			{
				p1DofJoint->SetAngleLimits(fMin,fMax);
			}
		}
	}
}

void CLandingGearPartPhysicalRot::Freeze1DofJointInCurrentPosition(fragInstGta* pFragInst, int iChildIndex)
{
	Assert(pFragInst);
	Assert(iChildIndex > -1);
	Assert(pFragInst->GetCacheEntry());

	fragHierarchyInst* pHierInst = pFragInst->GetCacheEntry()->GetHierInst();

	phArticulatedCollider* pArticulatedCollider = pHierInst->articulatedCollider;
	Assert(pArticulatedCollider);

	if(pArticulatedCollider == NULL)
	{
		return;
	}

	pFragInst->GetCacheEntry()->LazyArticulatedInit();

	int linkIndex = pArticulatedCollider->GetLinkFromComponent(iChildIndex);
	if(linkIndex > 0)
	{
		phJoint1Dof* p1DofJoint = NULL;
		phJoint* pJoint = &pHierInst->body->GetJoint(linkIndex - 1);
		if (pJoint && pJoint->GetJointType() == phJoint::JNT_1DOF)
			p1DofJoint = static_cast<phJoint1Dof*>(pJoint);

		if (physicsVerifyf(p1DofJoint,"Child is not connected by a 1 dof joint"))
		{
			const float fCurrentAngle = p1DofJoint->GetAngle(pHierInst->body);
			p1DofJoint->SetAngleLimits(fCurrentAngle,fCurrentAngle);
            p1DofJoint->SetMuscleTargetSpeed(0.0f);
		}
	}
}

void CLandingGearPartPhysicalRot::Snap1DofJointToOpenRatio(fragInstGta* pFragInst, int iChildIndex, float fTargetOpenRatio)
{
	Assert(pFragInst->GetCacheEntry());	// All vehicles should be in the cache

	fragHierarchyInst* pHierInst = pFragInst->GetCacheEntry()->GetHierInst();
	Assert(pHierInst);

	phArticulatedCollider* pArticulatedCollider = pHierInst->articulatedCollider;
	Assert(pArticulatedCollider);

	if(pArticulatedCollider == NULL)
	{
		return;
	}

	pFragInst->GetCacheEntry()->LazyArticulatedInit();

	int linkIndex = pArticulatedCollider->GetLinkFromComponent(iChildIndex);
	if(linkIndex > 0)
	{
		phJoint1Dof* p1DofJoint = NULL;
		phJoint* pJoint = &pHierInst->body->GetJoint(linkIndex - 1);
		if (pJoint && pJoint->GetJointType() == phJoint::JNT_1DOF)
			p1DofJoint = static_cast<phJoint1Dof*>(pJoint);

		if(p1DofJoint)
		{
			// First figure out what our closed angle is
			// Assume open angle is 0
			float fMin,fMax;
			p1DofJoint->GetAngleLimits(fMin,fMax);

			float fClosedAngle = -fMin > fMax ? fMin : fMax;
			float fTargetAngle = (1.0f - fTargetOpenRatio) * fClosedAngle;

			p1DofJoint->MatchChildToParentPositionAndVelocity(pArticulatedCollider->GetBody(),ScalarVFromF32(fTargetAngle));
			pArticulatedCollider->GetBody()->CalculateInertias();
			pArticulatedCollider->UpdateCurrentMatrices();
		}
	}
}


CLandingGearPartBase* CLandingGearPartFactory::CreatePart(CVehicle* pParent, eHierarchyId nId)
{
	int iBoneIndex = pParent->GetBoneIndex(nId);
	if(iBoneIndex == -1)
	{
		// Invalid component
		return NULL;
	}

	// Just use skeleton data to see what type of gear part to make
	// Skeleton limits define what we should do
	vehicleAssert(pParent->GetVehicleFragInst());

	const crJointData* pJointData = pParent->GetVehicleFragInst()->GetType()->GetCommonDrawable()->GetJointData();

	if( pJointData )
	{
		const crBoneData* pBoneData = pParent->GetVehicleFragInst()->GetType()->GetSkeletonData().GetBoneData(iBoneIndex);

		Vec3V minimumRot, maximumRot;
		pJointData->ConvertToEulers(*pBoneData, minimumRot, maximumRot);

		if( pBoneData->GetDofs() & crBoneData::HAS_ROTATE_LIMITS &&
			( Mag( minimumRot ).Getf() > 0.0f ||
			  Mag( maximumRot ).Getf() > 0.0f ) )
		{
			return rage_new CLandingGearPartPhysicalRot();
		}
	}

	return NULL;
}


//////////////////////////////////////////////////////////////////////////
// Class CLandingGear
//////////////////////////////////////////////////////////////////////////

static dev_float sfGearLockedTolerance = 0.001f; // Ratio at which we lock
bank_float CLandingGear::ms_fFrontDoorSpeed = 3.0f;

CLandingGear::CLandingGear()
{
	for(int i = 0; i < MAX_NUM_PARTS; i++)
	{
		m_pLandingGearParts[i] = NULL;
	}

	m_nState = GEAR_INIT;
	m_nPreviousState = GEAR_INIT;
	m_fAuxDoorOpenRatio = 1.0f;
	m_fGearDeployRatio = 1.0f;
	m_iNumGearParts = 0;
	m_bLandingGearRetractAfterInit = false;
	m_iAuxDoorComponentFL = -1;
	m_iAuxDoorComponentFR = -1;

	vFrontLeftDoorDeformation = VEC3_ZERO;
	vFrontRightDoorDeformation = VEC3_ZERO;
}

CLandingGear::~CLandingGear()
{
	for(int i =0; i < MAX_NUM_PARTS; i++)
	{
		if(m_pLandingGearParts[i])
		{
			delete m_pLandingGearParts[i];
			m_pLandingGearParts[i] = NULL;
		}
	}
}

void CLandingGear::ControlLandingGear(CVehicle* pParent, eCommands nCommand, bool bNetCheck)
{
	Assert(pParent);

	if(bNetCheck && pParent->IsNetworkClone())
	{
		return;
	}

	bool bUpdateLocalMatrices = false;

	switch(nCommand)
	{
	case COMMAND_DEPLOY:
		{
			// B*2056549 - If the sea plane is on water and it will force the landing gear to be retracted 
			// after a second so don't allow it to be deployed.
			if( pParent->pHandling->GetSeaPlaneHandlingData() )
			{
				CPlane* pPlane = static_cast<CPlane*>( pParent );
				if( pPlane->GetIsInWater() &&
					!pPlane->HasContactWheels() )
				{
					return;
				}	
			}
			DeployGear(pParent);
		}
		break;
	case COMMAND_RETRACT:
		{
			RetractGear(pParent);
		}
		break;
	case COMMAND_DEPLOY_INSTANT:
		{
			DeployGearInstant(pParent);

			// GTAV NG B*2116182 - Force the physics update here to avoid the 1 frame delay
			if(pParent->GetCurrentPhysicsInst())
			{
				ProcessPhysics( pParent );
			}

			bUpdateLocalMatrices = true;
		}
		break;
	case COMMAND_RETRACT_INSTANT:
		{
			// GTAV B*1975169 - if the landing gear is still being intialised
			// force the physics update here to get rid of the 1 frame delay this would cause
			// this is probably a bad thing to do.
			bool physicsProcessed = false;
			if( m_nState == GEAR_INIT )
			{
				m_bLandingGearRetractAfterInit = true;
				physicsProcessed = true;
				ProcessPhysics( pParent );
			}
			RetractGearInstant(pParent);

			if( !physicsProcessed &&
				!m_bLandingGearRetractAfterInit &&
				pParent->GetCurrentPhysicsInst() && 
				!CPhysics::GetLevel()->IsActive(pParent->GetCurrentPhysicsInst()->GetLevelIndex()) )
			{
				m_bLandingGearRetractAfterInit = false;
				ProcessPhysics( pParent );
			}

			bUpdateLocalMatrices = true;
		}
		break;
	case COMMAND_BREAK:
		{
			BreakGear(pParent);
		}
		break;

	default:
		break;
	}


	if(bUpdateLocalMatrices)
	{
		// If art body has only just been initialised then wheel matrices can end up in weird place
		// Presumably the syncskeletontoartciulatedbody call is not working properly

		// TODO: Can we get physics sim to do this for us?
		// Otherwise we need to sync twice!
		// sync up articulated collider root matrix first if the parent vehicle is not active
		if(pParent->GetCurrentPhysicsInst() && !CPhysics::GetLevel()->IsActive(pParent->GetCurrentPhysicsInst()->GetLevelIndex()))
		{
			if(pParent->GetVehicleFragInst() && pParent->GetVehicleFragInst()->GetCacheEntry() && pParent->GetVehicleFragInst()->GetCacheEntry()->GetHierInst() && pParent->GetVehicleFragInst()->GetCacheEntry()->GetHierInst()->articulatedCollider)
			{
				pParent->GetVehicleFragInst()->GetCacheEntry()->GetHierInst()->articulatedCollider->SetColliderMatrixFromInstance();
			}
		}

		pParent->GetVehicleFragInst()->SyncSkeletonToArticulatedBody(true);

		if( !pParent->InheritsFromHeli() ||
			!static_cast< CHeli* >( pParent )->GetIsJetPack() )
		{
			for(int iWheelIndex = 0; iWheelIndex < pParent->GetNumWheels(); iWheelIndex++)
			{
				Assert(pParent->GetWheel(iWheelIndex));

				pParent->GetWheel(iWheelIndex)->SuspensionSetup(NULL);
			}
		}
	}

}

void CLandingGear::InitPhys(CVehicle* pParent, eHierarchyId nFrontId, eHierarchyId nMiddleRId, eHierarchyId nMiddleLId, eHierarchyId nRearRId, eHierarchyId nRearLId, eHierarchyId nRearMId)
{
	const eHierarchyId iGearIds[MAX_NUM_PARTS] = 
	{
		nFrontId,
		nMiddleRId,
		nMiddleLId,
		nRearRId,
		nRearLId,
        nRearMId,
	};

	vehicleAssertf(m_iNumGearParts == 0, "InitPhys called twice?");	

	for(int i = 0; i < MAX_NUM_PARTS && vehicleVerifyf(m_iNumGearParts < MAX_NUM_PARTS,"About to overrun array"); i++)
	{
		// Generate the appropriate landing gear part
		m_pLandingGearParts[m_iNumGearParts] = CLandingGearPartFactory::CreatePart(pParent, iGearIds[i]);

		if(m_pLandingGearParts[m_iNumGearParts])
		{
			m_pLandingGearParts[m_iNumGearParts]->InitPhys(pParent,iGearIds[i]);
			m_iNumGearParts++;
		}
	}

	int iLandingGearDoorBoneIndex = pParent->GetBoneIndex(LANDING_GEAR_DOOR_FL);
	if(iLandingGearDoorBoneIndex)
	{
		m_iAuxDoorComponentFL = pParent->GetVehicleFragInst()->GetComponentFromBoneIndex(iLandingGearDoorBoneIndex);
	}
	iLandingGearDoorBoneIndex = pParent->GetBoneIndex(LANDING_GEAR_DOOR_FR);
	if(iLandingGearDoorBoneIndex)
	{
		m_iAuxDoorComponentFR = pParent->GetVehicleFragInst()->GetComponentFromBoneIndex(iLandingGearDoorBoneIndex);
	}
}

void CLandingGear::InitCompositeBound(CVehicle* pParent)
{
	for(int i = 0; i < MAX_NUM_PARTS; i++)
	{
		if(m_pLandingGearParts[i])
		{
			m_pLandingGearParts[i]->InitCompositeBound(pParent);
		}
	}

	phBoundComposite* pBoundComp = static_cast<phBoundComposite*>(pParent->GetVehicleFragInst()->GetArchetype()->GetBound());	
	Assert(pBoundComp);

	if(pBoundComp->GetTypeAndIncludeFlags())
	{
		int iLastLandingGearDoor = FIRST_LANDING_GEAR_DOOR + NUM_LANDING_GEAR_DOORS;
		bool bEnableCollision = GetPublicState() < STATE_LOCKED_UP; // STATE_LOCKED_DOWN, STATE_RETRACTING, STATE_DEPLOYING
		for(int nDoor=FIRST_LANDING_GEAR_DOOR; nDoor <= iLastLandingGearDoor; nDoor++)
		{
			if(pParent->GetBoneIndex((eHierarchyId)nDoor)!=-1)
			{
				int nChildIndex = pParent->GetVehicleFragInst()->GetComponentFromBoneIndex(pParent->GetBoneIndex((eHierarchyId)nDoor));
				if(nChildIndex != -1)
				{
					pBoundComp->SetIncludeFlags(nChildIndex, bEnableCollision ? ArchetypeFlags::GTA_VEHICLE_INCLUDE_TYPES : 0);
				}
			}
		}
	}
}

void CLandingGear::InitWheels(CVehicle *pParent)
{
	for(int i = 0; i < MAX_NUM_PARTS; i++)
	{
		if(m_pLandingGearParts[i])
		{
			m_pLandingGearParts[i]->InitWheel(pParent);
		}
	}
}

void CLandingGear::ProcessControl(CVehicle* UNUSED_PARAM(pParent))
{
	const float fTimestep = fwTimer::GetTimeStep();

	// Open / close front doors based on state
	eLandingGearState landingGearState = GetInternalState();
	float fTargetRatio = ( landingGearState == GEAR_LOCKED_UP || landingGearState == GEAR_RETRACT_INSTANT ) ? 0.0f : 1.0f;

	m_fAuxDoorOpenRatio += rage::Clamp(fTargetRatio - m_fAuxDoorOpenRatio, -fTimestep*ms_fFrontDoorSpeed, fTimestep*ms_fFrontDoorSpeed);
	physicsAssertf(m_fAuxDoorOpenRatio >= 0.0f && m_fAuxDoorOpenRatio <= 1.0f, "Invalid front landing gear open ratio: %f",m_fAuxDoorOpenRatio);
}

void CLandingGear::PreRenderWheels(CVehicle* pParent)
{
	for(int i = 0; i < m_iNumGearParts; i++)
	{
		if(m_pLandingGearParts[i])
		{
			m_pLandingGearParts[i]->ProcessPreRenderWheels(pParent);
		}
	}

	pParent->GetSkeleton()->Update();
}

void CLandingGear::PreRenderDoors(CVehicle* pParent)
{
	// better make sure we've got some handling data, otherwise we're screwed for flying
	CFlyingHandlingData* pFlyingHandling = pParent->pHandling->GetFlyingHandlingData();
	if(pFlyingHandling == NULL)
		return;

	// Set component ratio based on ratio
	// Doors are authored open
	const float fAngleFront = (1.0f - m_fAuxDoorOpenRatio) * pFlyingHandling->m_fGearDoorFrontOpen;

	pParent->SetComponentRotation(LANDING_GEAR_DOOR_FL,ROT_AXIS_LOCAL_Y,-fAngleFront, true, &vFrontLeftDoorDeformation);
	pParent->SetComponentRotation(LANDING_GEAR_DOOR_FR,ROT_AXIS_LOCAL_Y,fAngleFront, true, &vFrontRightDoorDeformation);

	const float fAngleRear = (1.0f - m_fAuxDoorOpenRatio) * pFlyingHandling->m_fGearDoorRearOpen;

	pParent->SetComponentRotation(LANDING_GEAR_DOOR_RL1,ROT_AXIS_LOCAL_Y,-fAngleRear);
	pParent->SetComponentRotation(LANDING_GEAR_DOOR_RR1,ROT_AXIS_LOCAL_Y,fAngleRear);

	const float fAngleRear2 = (1.0f - m_fAuxDoorOpenRatio) * pFlyingHandling->m_fGearDoorRearOpen2;
	pParent->SetComponentRotation(LANDING_GEAR_DOOR_RL2,ROT_AXIS_LOCAL_Y,fAngleRear2);
	pParent->SetComponentRotation(LANDING_GEAR_DOOR_RR2,ROT_AXIS_LOCAL_Y,-fAngleRear2);

	const float fAngleRearM = (1.0f - m_fAuxDoorOpenRatio) * pFlyingHandling->m_fGearDoorRearMOpen;
	pParent->SetComponentRotation(LANDING_GEAR_DOOR_RML,ROT_AXIS_LOCAL_Y,-fAngleRearM);
	pParent->SetComponentRotation(LANDING_GEAR_DOOR_RMR,ROT_AXIS_LOCAL_Y,fAngleRearM);

	pParent->GetSkeleton()->Update();

}

static float m_fPowerGearDeployRatio = 0.5f; //Once the deploy rate passes this ratio, the landing gear will power through the deployment regardless of the collision
void CLandingGear::ProcessPhysics(CVehicle* pParent)
{
#if __BANK
	if(ms_bDisableLandingGear)
	{
		return;
	}
#endif

	// For wheel code to get updated properly local matrices must be correct before wheel pre sim update
	// Update them here if required
	bool bUpdateLocalMatrices = true;
	bool bUpdateWheelMatrices = false;

	eLandingGearState nOrigState = m_nState;
	bool bColliderSynced = false;
	bool bShouldStopSound = false;

	do
	{
		// If we had an instant retract call while in GEAR_INIT we're safe to call it here now that the
		// articulated body is all ready to go
		if(m_nState != GEAR_INIT && m_bLandingGearRetractAfterInit)
		{
			ControlLandingGear(pParent, COMMAND_RETRACT_INSTANT, false);
			m_bLandingGearRetractAfterInit = false;
		}

		nOrigState = m_nState;
		switch(m_nState)
		{
			// TODO: Move this state stuff to an init function
			// Could maybe be moved to model info
		case GEAR_INIT:
			{
				SetState(GEAR_LOCKED_DOWN);
			}
			break;
		case GEAR_LOCKED_DOWN:
			m_fGearDeployRatio = 1.0f;

			// B*3844822 - We should fix this properly
			if( CPhysics::GetIsLastTimeSlice( CPhysics::GetCurrentTimeSlice() ) &&
				pParent->IsNetworkClone() )
			{
				bUpdateWheelMatrices = true;
			}

			// All parts should be 'latched' in this state so no need to do anything
			bUpdateLocalMatrices = false;	// Wheel position shouldn't change
			break;

		case GEAR_LOCKED_UP:
			m_fGearDeployRatio = 0.0f;
			// All parts should be 'latched' in this state so no need to do anything


			// B*3721373 - We should fix this properly
			if( CPhysics::GetIsLastTimeSlice( CPhysics::GetCurrentTimeSlice() ) &&
				pParent->IsNetworkClone() )
			{
				bUpdateWheelMatrices = true;
			}

			bUpdateLocalMatrices = false;	// Wheel position shouldn't change
			
			break;

		case GEAR_LOCKED_DOWN_RETRACT:
			{
				for(int i = 0; i < m_iNumGearParts; i++)
				{
					if(m_pLandingGearParts[i])
					{
						m_pLandingGearParts[i]->Process(pParent,CLandingGearPartBase::UNLOCK_DOWN, m_fGearDeployRatio);
					}
				}
				SetState(GEAR_RETRACTING);
			}
			break;
		case GEAR_LOCKED_UP_DEPLOY:
			{
				for(int i = 0; i < m_iNumGearParts; i++)
				{
					if(m_pLandingGearParts[i])
					{
						m_pLandingGearParts[i]->Process(pParent,CLandingGearPartBase::UNLOCK_UP, m_fGearDeployRatio);
					}
				}
				SetState(GEAR_DEPLOYING);
			}
			break;
		case GEAR_DEPLOYING:
			{
				bool playGearSound = false;

				m_fGearDeployRatio = 0.0f;

				// Don't deploy landing gear till aux door fully opened
				if(m_fAuxDoorOpenRatio >= 0.95f)
				{
					for(int i = 0; i < m_iNumGearParts; i++)
					{
						if(m_pLandingGearParts[i])
						{
							float fGearDeployRatio = 0.0f;
							bool gearMoved = m_pLandingGearParts[i]->Process(pParent,CLandingGearPartBase::DRIVE_DOWN, fGearDeployRatio);
							m_fGearDeployRatio += Min(fGearDeployRatio, 1.0f);
							if(gearMoved)
							{
								playGearSound = true;
								// B*1816767: Do a bunch of short, low intensity rumbles while the gear is deploying. The same is done while it's retracting.
								if(pParent && pParent->GetDriver() && pParent->GetDriver()->IsLocalPlayer())
									CControlMgr::StartPlayerPadShakeByIntensity(ms_deployingRumbleDuration, ms_deployingRumbleIntensity);
							}
						}
					}
				}

				if(m_iNumGearParts > 0)
				{
					m_fGearDeployRatio /= (float)m_iNumGearParts;
				}

				if(playGearSound && (m_nPreviousState == GEAR_RETRACTING || m_nPreviousState == GEAR_LOCKED_UP_DEPLOY))
				{
					if(pParent->m_VehicleAudioEntity->GetAudioVehicleType() == AUD_VEHICLE_PLANE)
					{
						static_cast<audPlaneAudioEntity*>(pParent->m_VehicleAudioEntity)->DeployLandingGear();
					}
					else if(pParent->m_VehicleAudioEntity->GetAudioVehicleType() == AUD_VEHICLE_HELI)
					{
						static_cast<audHeliAudioEntity*>(pParent->m_VehicleAudioEntity)->DeployLandingGear();
					}
					
					m_nPreviousState = GEAR_DEPLOYING; // this clears previous state from being GEAR_RETRACTING or GEAR_LOCKED_UP_DEPLOY so we don't play the sound again
				}

				const bool bFullyDeployed = (1.0f - m_fGearDeployRatio) < sfGearLockedTolerance;
				if(bFullyDeployed)
				{
					SetState(GEAR_LOCKED_DOWN_INIT);
				}

				if(	m_fGearDeployRatio < m_fPowerGearDeployRatio && pParent->HasContactWheels() && 
					( !pParent->InheritsFromHeli() &&
					  !static_cast<CPlane*>(pParent)->GetVerticalFlightModeAvaliable() ) )
				{
					RetractGear(pParent);
				}
			}
			break;
		case GEAR_RETRACTING:
			{
				bool playGearSound = false;

				m_fGearDeployRatio = 0.0f;

				for(int i = 0; i < m_iNumGearParts; i++)
				{
					if(m_pLandingGearParts[i])
					{
						float fGearDeployRatio = 0.0f;
						bool gearMoved = m_pLandingGearParts[i]->Process(pParent,CLandingGearPartBase::DRIVE_UP, fGearDeployRatio);

						if(gearMoved)
						{
							m_fGearDeployRatio += Max(fGearDeployRatio, 0.0f);

							playGearSound = true;
							if(pParent && pParent->GetDriver() && pParent->GetDriver()->IsLocalPlayer())
							{
								CControlMgr::StartPlayerPadShakeByIntensity(ms_retractingRumbleDuration, ms_retractingRumbleIntensity);
							}
						}
					}
				}
				if(m_iNumGearParts > 0)
				{
					m_fGearDeployRatio /= (float)m_iNumGearParts;
				}

				if(playGearSound && (m_nPreviousState == GEAR_DEPLOYING || m_nPreviousState == GEAR_LOCKED_DOWN_RETRACT))
				{
					if(pParent->m_VehicleAudioEntity->GetAudioVehicleType() == AUD_VEHICLE_PLANE)
					{
						static_cast<audPlaneAudioEntity*>(pParent->m_VehicleAudioEntity)->RetractLandingGear();
					}
					else if(pParent->m_VehicleAudioEntity->GetAudioVehicleType() == AUD_VEHICLE_HELI)
					{
						static_cast<audHeliAudioEntity*>(pParent->m_VehicleAudioEntity)->RetractLandingGear();
					}

					m_nPreviousState = GEAR_RETRACTING; // this clears previous state from being GEAR_DEPLOYING or GEAR_LOCKED_DOWN_RETRACT so we don't play the sound again
				}

				const bool bFullyRetracted = (m_fGearDeployRatio) < sfGearLockedTolerance;
				if(bFullyRetracted)
				{
					SetState(GEAR_LOCKED_UP_INIT);
				}
			}
			break;
		case GEAR_RETRACT_INSTANT:
			{
				Assert(pParent->GetCurrentPhysicsInst());
				bUpdateLocalMatrices = false;
				if(pParent->GetCurrentPhysicsInst())// && CPhysics::GetLevel()->IsActive(pParent->GetCurrentPhysicsInst()->GetLevelIndex()))
				{
					float fRatio;
					for(int i = 0; i < m_iNumGearParts; i++)
					{
						if(m_pLandingGearParts[i])
						{
							m_pLandingGearParts[i]->Process(pParent,CLandingGearPartBase::FORCE_UP, fRatio);
						}				
					}

					//Do the update here rather than waiting a frame in 

					// sync up articulated collider root matrix first if the parent vehicle is not active
					if(!bColliderSynced && !CPhysics::GetLevel()->IsActive(pParent->GetCurrentPhysicsInst()->GetLevelIndex()))
					{
						if(pParent->GetVehicleFragInst() && pParent->GetVehicleFragInst()->GetCacheEntry() && pParent->GetVehicleFragInst()->GetCacheEntry()->GetHierInst() && pParent->GetVehicleFragInst()->GetCacheEntry()->GetHierInst()->articulatedCollider)
						{
							pParent->GetVehicleFragInst()->GetCacheEntry()->GetHierInst()->articulatedCollider->SetColliderMatrixFromInstance();
						}
						bColliderSynced = true;
					}
					pParent->GetVehicleFragInst()->SyncSkeletonToArticulatedBody(true);

					//GEAR_LOCKED_UP_INIT as that could get the wheels in a bad state

					// if we're retracting instantly and this vehicle isn't in the physics level leave the state here as the joints won't get updated correctly
					if( CPhysics::GetLevel()->IsActive( pParent->GetCurrentPhysicsInst()->GetLevelIndex() ) )
					{
						SetState(GEAR_LOCKED_UP_INIT);
					}
				}
			}
			break;
		case GEAR_DEPLOY_INSTANT:
			{
				Assert(pParent->GetCurrentPhysicsInst());
				bUpdateLocalMatrices = false;
				if(pParent->GetCurrentPhysicsInst())// && CPhysics::GetLevel()->IsActive(pParent->GetCurrentPhysicsInst()->GetLevelIndex()))
				{
					float fRatio;
					for(int i = 0; i < m_iNumGearParts; i++)
					{
						if(m_pLandingGearParts[i])
						{
							m_pLandingGearParts[i]->Process(pParent,CLandingGearPartBase::FORCE_DOWN, fRatio);
						}				
					}

					//Do the update here rather than waiting a frame in 

					// sync up articulated collider root matrix first if the parent vehicle is not active
					if(!bColliderSynced && !CPhysics::GetLevel()->IsActive(pParent->GetCurrentPhysicsInst()->GetLevelIndex()))
					{
						if(pParent->GetVehicleFragInst() && pParent->GetVehicleFragInst()->GetCacheEntry() && pParent->GetVehicleFragInst()->GetCacheEntry()->GetHierInst() && pParent->GetVehicleFragInst()->GetCacheEntry()->GetHierInst()->articulatedCollider)
						{
							pParent->GetVehicleFragInst()->GetCacheEntry()->GetHierInst()->articulatedCollider->SetColliderMatrixFromInstance();
						}
						bColliderSynced = true;
					}

					if( CPhysics::GetLevel()->IsActive( pParent->GetCurrentPhysicsInst()->GetLevelIndex() ) )
					{
						pParent->GetVehicleFragInst()->SyncSkeletonToArticulatedBody(true);
					}

					//GEAR_LOCKED_DOWN_INIT as that could get the wheels in a bad state
					SetState(GEAR_LOCKED_DOWN_INIT);
				}
			}
			break;
		case GEAR_LOCKED_DOWN_INIT:
			{
				for(int i = 0; i < m_iNumGearParts; i++)
				{
					if(m_pLandingGearParts[i])
					{
						m_pLandingGearParts[i]->Process(pParent,CLandingGearPartBase::LOCK_DOWN, m_fGearDeployRatio);
					}
				}

				// B*1816767: Do a single, longer, higher intensity rumble to signify the gear locking into position. Same is down for when it has been fully retracted.
				if(m_iNumGearParts > 0 && pParent && pParent->GetDriver() && pParent->GetDriver()->IsLocalPlayer())
					CControlMgr::StartPlayerPadShakeByIntensity(ms_deployedRumbleDuration, ms_deployedRumbleIntensity);

				InitCompositeBound(pParent); // enable landing gear door bounds

				SetState(GEAR_LOCKED_DOWN_POST_INIT);
				nOrigState = GEAR_LOCKED_DOWN_POST_INIT;// we needed an extra physics update to get the wheels in the correct position
			}
			break;
		case GEAR_LOCKED_DOWN_POST_INIT:
			{
				SetState(GEAR_LOCKED_DOWN);// we needed an extra physics update to get the wheels in the correct position
				nOrigState = GEAR_LOCKED_DOWN;// we needed an extra physics update to get the wheels in the correct position
				bShouldStopSound = true;
			}
			break;
		case GEAR_LOCKED_UP_INIT:
			{
				for(int i = 0; i < m_iNumGearParts; i++)
				{
					if(m_pLandingGearParts[i])
					{
						m_pLandingGearParts[i]->Process(pParent,CLandingGearPartBase::LOCK_UP, m_fGearDeployRatio);
					}
				}

				if(m_iNumGearParts > 0 && pParent && pParent->GetDriver() && pParent->GetDriver()->IsLocalPlayer())
					CControlMgr::StartPlayerPadShakeByIntensity(ms_retractedRumbleDuration, ms_retractedRumbleIntensity);

				InitCompositeBound(pParent); // disable landing gear door bounds

				SetState(GEAR_LOCKED_UP_POST_INIT);
				nOrigState = GEAR_LOCKED_UP_POST_INIT;// we needed an extra physics update to get the wheels in the correct position
			}
			break;
        case GEAR_LOCKED_UP_POST_INIT:
            {
				SetState(GEAR_LOCKED_UP);// we needed an extra physics update to get the wheels in the correct position
				nOrigState = GEAR_LOCKED_UP;// we needed an extra physics update to get the wheels in the correct position			
				bShouldStopSound = true;
            }
            break;
		case GEAR_BROKEN:
			{
				// Gear is free to move about
				// Joint modes are set in BreakGear() function
				// So no need to do any work here
				m_fGearDeployRatio = 1.0f;
			}
			break;
		default:
			physicsAssertf(0,"Unhandled landing gear state");
			break;
		}
	}
	while(nOrigState != m_nState);

	if(bUpdateLocalMatrices)
	{
		// TODO: Can we get physics sim to do this for us?
		// Otherwise we need to sync twice!
		// sync up articulated collider root matrix first if the parent vehicle is not active
		if(!bColliderSynced && pParent->GetCurrentPhysicsInst() && !CPhysics::GetLevel()->IsActive(pParent->GetCurrentPhysicsInst()->GetLevelIndex()))
		{
			if(pParent->GetVehicleFragInst() && pParent->GetVehicleFragInst()->GetCacheEntry() && pParent->GetVehicleFragInst()->GetCacheEntry()->GetHierInst() && pParent->GetVehicleFragInst()->GetCacheEntry()->GetHierInst()->articulatedCollider)
			{
				pParent->GetVehicleFragInst()->GetCacheEntry()->GetHierInst()->articulatedCollider->SetColliderMatrixFromInstance();
			}
		}
		pParent->GetVehicleFragInst()->SyncSkeletonToArticulatedBody(true);
	}

	for(int iWheelIndex = 0; iWheelIndex < pParent->GetNumWheels(); iWheelIndex++)
	{
		Assert(pParent->GetWheel(iWheelIndex));

		if( bUpdateLocalMatrices ||
			bUpdateWheelMatrices )
		{
			pParent->GetWheel(iWheelIndex)->GetConfigFlags().SetFlag(WCF_UPDATE_SUSPENSION);
		}
		else
		{
			pParent->GetWheel(iWheelIndex)->GetConfigFlags().ClearFlag(WCF_UPDATE_SUSPENSION);
		}
	}

	if(bShouldStopSound && pParent->m_VehicleAudioEntity)
	{
		pParent->m_VehicleAudioEntity->StopLandingGearSound();
	}
}

bool CLandingGear::WantsToBeAwake(CVehicle* pParent) 
{
	if ((GetInternalState() == GEAR_RETRACTING) || (GetInternalState() == GEAR_DEPLOYING)
		|| (GetInternalState() == GEAR_RETRACT_INSTANT) || (GetInternalState() == GEAR_DEPLOY_INSTANT))
	{
		return true;
	}

	// Need to activate parent vehicle in order to update the suspension
	for(int iWheelIndex = 0; iWheelIndex < pParent->GetNumWheels(); iWheelIndex++)
	{
		if(pParent->GetWheel(iWheelIndex)->GetConfigFlags().IsFlagSet(WCF_UPDATE_SUSPENSION))
		{
			return true;
		}
	}

	return false;
}

void CLandingGear::DeployGear(CVehicle* UNUSED_PARAM(pParent))
{
	switch(m_nState)
	{
	case GEAR_LOCKED_UP:
		SetState(GEAR_LOCKED_UP_DEPLOY);
		break;
	case GEAR_RETRACTING:
	case GEAR_RETRACT_INSTANT:
	case GEAR_DEPLOY_INSTANT:
		SetState(GEAR_DEPLOYING);
		break;
	case GEAR_LOCKED_DOWN_RETRACT:
		SetState(GEAR_LOCKED_DOWN);
		break;
	default:
		break;
	}
}

void CLandingGear::RetractGear(CVehicle* UNUSED_PARAM(pParent))
{
	switch(m_nState)
	{
	case GEAR_LOCKED_DOWN:
		SetState(GEAR_LOCKED_DOWN_RETRACT);
		break;
	case GEAR_DEPLOYING:
	case GEAR_RETRACT_INSTANT:
	case GEAR_DEPLOY_INSTANT:
		SetState(GEAR_RETRACTING);
		break;
	case GEAR_LOCKED_UP_DEPLOY:
		SetState(GEAR_LOCKED_UP);
		break;
	default:
		break;
	}
}

void CLandingGear::DeployGearInstant(CVehicle* pParent)
{
	switch(GetInternalState())
	{
	case GEAR_INIT:
	case GEAR_LOCKED_DOWN:
	case GEAR_LOCKED_DOWN_INIT:
	case GEAR_LOCKED_DOWN_POST_INIT:
		{
			// Do nothing, we are already deployed
		}
		break;
	case GEAR_LOCKED_DOWN_RETRACT:
		{
			// Cancel the retraction order
			SetState(GEAR_LOCKED_DOWN);
		}
		break;
	// All other states need to snap the parts position
	case GEAR_LOCKED_UP_DEPLOY:
	case GEAR_LOCKED_UP:
		{
			// Need to unlock the gear before we can snap it to deployed
			float fRatio;
			for(int i = 0; i < m_iNumGearParts; i++)
			{
				if(m_pLandingGearParts[i])
				{
					m_pLandingGearParts[i]->Process(pParent,CLandingGearPartBase::UNLOCK_UP, fRatio);
				}				
			}
		}
	// Intentional fall through
	case GEAR_LOCKED_UP_INIT:
    case GEAR_LOCKED_UP_POST_INIT:
	case GEAR_RETRACTING:
	case GEAR_DEPLOYING:
	case GEAR_RETRACT_INSTANT:
	case GEAR_DEPLOY_INSTANT:
	case GEAR_BROKEN:
		{
			SetState(GEAR_DEPLOY_INSTANT);
		}
		break;
	default:
		{
			vehicleAssertf(false, "Unhandled state in CLandingGear::DeployGearInstant, state: %i",GetInternalState());
		}
		break;
	}

	// force the landing gear door to open
	m_fAuxDoorOpenRatio = 1.0f;
}

void CLandingGear::RetractGearInstant(CVehicle* pParent)
{
	switch(GetInternalState())
	{	
	case GEAR_INIT:
		m_bLandingGearRetractAfterInit = true;
		break;
	case GEAR_LOCKED_UP:
	case GEAR_LOCKED_UP_INIT:	
    case GEAR_LOCKED_UP_POST_INIT:
		{
			// Do nothing, we are already retracted
		}
		break;
	case GEAR_LOCKED_UP_DEPLOY:
		{
			// Cancel the deployment order
			SetState(GEAR_LOCKED_DOWN);
		}
		break;
		// All other states need to snap the parts position
	case GEAR_LOCKED_DOWN_RETRACT:
	case GEAR_LOCKED_DOWN:
		{
			// Need to unlock the gear before we can snap it to retracted
			float fRatio;
			for(int i = 0; i < m_iNumGearParts; i++)
			{
				if(m_pLandingGearParts[i])
				{
					m_pLandingGearParts[i]->Process(pParent,CLandingGearPartBase::UNLOCK_DOWN, fRatio);
				}				
			}
		}
		// Intentional fall through
	case GEAR_LOCKED_DOWN_INIT:
	case GEAR_LOCKED_DOWN_POST_INIT:
	case GEAR_RETRACTING:
	case GEAR_DEPLOYING:
	case GEAR_RETRACT_INSTANT:
	case GEAR_DEPLOY_INSTANT:
	case GEAR_BROKEN:
		{
			SetState(GEAR_RETRACT_INSTANT);
		}
		break;
	default:
		{
			vehicleAssertf(false, "Unhandled state in CLandingGear::RetractGearInstant, state: %i",GetInternalState());
		}
		break;
	}

	// force the landing gear door to closed
	m_fAuxDoorOpenRatio = 0.0f;
}

void CLandingGear::BreakGear(CVehicle* pParent)
{
	switch(m_nState)
	{
	case GEAR_BROKEN:
		break;
	case GEAR_LOCKED_DOWN:		
		{
			float fRatio;
			for(int i = 0; i < m_iNumGearParts; i++)
			{
				if(m_pLandingGearParts[i])
				{
					m_pLandingGearParts[i]->Process(pParent,CLandingGearPartBase::UNLOCK_DOWN, fRatio);
					m_pLandingGearParts[i]->Process(pParent,CLandingGearPartBase::SWING_FREELY, fRatio);
				}				
			}
		}
		break;
	case GEAR_LOCKED_UP:
		{
			float fRatio;
			for(int i = 0; i < m_iNumGearParts; i++)
			{
				if(m_pLandingGearParts[i])
				{
					m_pLandingGearParts[i]->Process(pParent,CLandingGearPartBase::UNLOCK_UP, fRatio);
					m_pLandingGearParts[i]->Process(pParent,CLandingGearPartBase::SWING_FREELY, fRatio);
				}
			}			
		}
		break;
	default:
		{
			float fRatio;
			for(int i = 0; i < m_iNumGearParts; i++)
			{
				if(m_pLandingGearParts[i])
				{
					m_pLandingGearParts[i]->Process(pParent,CLandingGearPartBase::SWING_FREELY, fRatio);
				}
			}
		}
	}

	SetState(GEAR_BROKEN);
}

float CLandingGear::GetLiftMult(CVehicle* pParent) const
{
	Assert(pParent->pHandling->GetFlyingHandlingData());
	// Do not let plane take off when its landing gear had broken off
	if(!pParent->IsInAir() && pParent->InheritsFromPlane() && !((CPlane *)pParent)->GetIsLandingGearintact())
	{
		return 0.0f;
	}
	return (1.0f - m_fGearDeployRatio) + (m_fGearDeployRatio * pParent->pHandling->GetFlyingHandlingData()->m_fGearDownLiftMult);
}

float CLandingGear::GetDragMult(CVehicle* pParent) const
{
	Assert(pParent->pHandling->GetFlyingHandlingData());
	return m_fGearDeployRatio * pParent->pHandling->GetFlyingHandlingData()->m_fGearDownDragV;
}

#if !__FINAL
const char* strLandingGearStateNames[] = 
{
	"GEAR_INIT",
	"GEAR_LOCKED_DOWN",
	"GEAR_LOCKED_DOWN_RETRACT",
	"GEAR_LOCKED_DOWN_INIT",
	"GEAR_LOCKED_DOWN_POST_INIT",
	"GEAR_RETRACTING",
	"GEAR_DEPLOYING",		
	"GEAR_RETRACT_INSTANT",
	"GEAR_DEPLOY_INSTANT",
	"GEAR_LOCKED_UP_DEPLOY",	
	"GEAR_LOCKED_UP",
	"GEAR_LOCKED_UP_INIT",	
    "GEAR_LOCKED_UP_POST_INIT",
	"GEAR_BROKEN"
};

const char* CLandingGear::GetDebugStateName() const
{
	return strLandingGearStateNames[GetInternalState()];
}
#endif

void CLandingGear::Fix(CVehicle* pParent)
{
	eLandingGearState eOriginalState = GetInternalState();
    for(int i = 0; i < m_iNumGearParts; i++)
    {
        if(m_pLandingGearParts[i])
        {
            m_pLandingGearParts[i]->Fix();
        }
    }

	SetState(GEAR_INIT);

	for(int iWheelIndex = 0; iWheelIndex < pParent->GetNumWheels(); iWheelIndex++)
	{
		pParent->GetWheel(iWheelIndex)->SuspensionSetup(NULL);
	}

	// Restore the original state
	switch(eOriginalState)
	{
	case GEAR_INIT:
	case GEAR_LOCKED_DOWN:
	case GEAR_LOCKED_DOWN_RETRACT:
	case GEAR_LOCKED_DOWN_INIT:
	case GEAR_LOCKED_DOWN_POST_INIT:
	case GEAR_DEPLOYING:
	case GEAR_DEPLOY_INSTANT:
	case GEAR_BROKEN:
		{
			// no-op
			break;
		}

	case GEAR_RETRACTING:
	case GEAR_RETRACT_INSTANT:
	case GEAR_LOCKED_UP:
	case GEAR_LOCKED_UP_DEPLOY:
	case GEAR_LOCKED_UP_INIT:
	case GEAR_LOCKED_UP_POST_INIT:
		{
			ControlLandingGear(pParent, COMMAND_RETRACT_INSTANT);
			break;
		}

	default:
		vehicleAssertf(false,"Unhandling state in Fix()")	;
		break;
	}

	vFrontLeftDoorDeformation = VEC3_ZERO;
	vFrontRightDoorDeformation = VEC3_ZERO;
}

CLandingGear::eLandingGearPublicStates CLandingGear::GetPublicState() const 
{
	// GTAV - B*2331267 - if the landing gear has no parts return that the state is locked down.
	if( !m_iNumGearParts )
	{
		return STATE_LOCKED_DOWN;
	}

	switch(GetInternalState())
	{
	case GEAR_INIT:
	case GEAR_LOCKED_DOWN:
	case GEAR_LOCKED_DOWN_RETRACT:
	case GEAR_LOCKED_DOWN_INIT:
	case GEAR_LOCKED_DOWN_POST_INIT:
		return STATE_LOCKED_DOWN;

	case GEAR_RETRACTING:
		return STATE_RETRACTING;
	case GEAR_RETRACT_INSTANT:
		return STATE_RETRACTING_INSTANT;

	case GEAR_DEPLOYING:
	case GEAR_DEPLOY_INSTANT:
		return STATE_DEPLOYING;

	case GEAR_LOCKED_UP:
	case GEAR_LOCKED_UP_DEPLOY:
	case GEAR_LOCKED_UP_INIT:
    case GEAR_LOCKED_UP_POST_INIT:
		return STATE_LOCKED_UP;

	case GEAR_BROKEN:
		return STATE_BROKEN;

	default:
		vehicleAssertf(false,"Unhandling state in GetPublicState()")	;
		break;
	}

	return STATE_LOCKED_DOWN;
}

void CLandingGear::SetFromPublicState(CVehicle* pParent, const u32 landingGearPublicState) 
{

	if((u32)GetPublicState() == landingGearPublicState)
	{
		return;
	}

	Assert(pParent);

	if(pParent->GetStatus() == STATUS_WRECKED)
	{
		//Ensure the internal state is set if broken.
		if (landingGearPublicState == STATE_BROKEN)
			SetState(GEAR_BROKEN);

		return;
	}

	switch(landingGearPublicState)
	{
	case STATE_LOCKED_DOWN:
		if(GetPublicState()!=STATE_DEPLOYING)
		{
			DeployGearInstant(pParent);
		}
		break;
	case STATE_RETRACTING_INSTANT:
		RetractGearInstant(pParent);
		break;
	case STATE_RETRACTING:
		RetractGear(pParent);
		break;
	case STATE_DEPLOYING:
		DeployGear(pParent);
		break;
	case STATE_LOCKED_UP:
		if(GetPublicState()!=STATE_RETRACTING)
		{
			RetractGearInstant(pParent);
		}
		break;
	case STATE_BROKEN:
		SetState(GEAR_BROKEN);
		break;
	default:
		vehicleAssertf(false,"Unhandling state in SetFromPublicState()")	;
		break;
	}
}

void CLandingGear::ApplyDeformation(CVehicle* pParentVehicle, const void *basePtr)
{
	for(int i = 0; i < m_iNumGearParts; i++)
	{
		if(m_pLandingGearParts[i])
		{
			m_pLandingGearParts[i]->ApplyDeformationToPart(pParentVehicle, basePtr);
		}
	}

	int nBoneIndex = pParentVehicle->GetBoneIndex(LANDING_GEAR_DOOR_FL);

	if(nBoneIndex > -1)
	{
		Vector3 basePos;
		pParentVehicle->GetDefaultBonePositionSimple(nBoneIndex, basePos);
		vFrontLeftDoorDeformation = VEC3V_TO_VECTOR3(pParentVehicle->GetVehicleDamage()->GetDeformation()->ReadFromVectorOffset(basePtr, VECTOR3_TO_VEC3V(basePos)));
	}

	nBoneIndex = pParentVehicle->GetBoneIndex(LANDING_GEAR_DOOR_FR);

	if(nBoneIndex > -1)
	{
		Vector3 basePos;
		pParentVehicle->GetDefaultBonePositionSimple(nBoneIndex, basePos);
		vFrontRightDoorDeformation = VEC3V_TO_VECTOR3(pParentVehicle->GetVehicleDamage()->GetDeformation()->ReadFromVectorOffset(basePtr, VECTOR3_TO_VEC3V(basePos)));
	}
}

bool CLandingGear::GetWheelDeformation(const CVehicle *pParentVehicle, const CWheel *pWheel, Vector3 &vDeformation) const
{
	for(int i = 0; i < m_iNumGearParts; i++)
	{
		if(m_pLandingGearParts[i] && m_pLandingGearParts[i]->GetWheelIndex() > -1 && pParentVehicle->GetWheel(m_pLandingGearParts[i]->GetWheelIndex()) == pWheel)
		{
			vDeformation = m_pLandingGearParts[i]->GetWheelDeformation(pParentVehicle);
			return true;
		}
	}

	return false;
}

void CLandingGear::ProcessPreComputeImpacts(const CVehicle *UNUSED_PARAM(pParentVehicle), phContactIterator impacts)
{
	// Disable aux door collision vs ground if landing gear is retracted, the aux door bound can be mismatching with its visual component due to the plane body deformation
	if(GetPublicState() == CLandingGear::STATE_LOCKED_UP)
	{
		phInst* otherInstance = impacts.GetOtherInstance();
		CEntity* pOtherEntity = otherInstance ? (CEntity*)otherInstance->GetUserData() : NULL;
		
		if(pOtherEntity && pOtherEntity->GetIsTypeBuilding())
		{
			if(impacts.GetMyComponent() == m_iAuxDoorComponentFL || impacts.GetMyComponent() == m_iAuxDoorComponentFR)
			{
				impacts.DisableImpact();
			}
		}
	}
}


#if __BANK
bool CLandingGear::ms_bDisableLandingGear = false;
CLandingGear::eCommands CLandingGear::ms_nCommand = CLandingGear::COMMAND_DEPLOY;

void CLandingGear::SendCommandCB()
{
	if(CDebugScene::FocusEntities_Get(0) && CDebugScene::FocusEntities_Get(0)->GetIsTypeVehicle()
		&& static_cast<CVehicle*>(CDebugScene::FocusEntities_Get(0))->InheritsFromPlane())
	{
		CPlane* pPlane = static_cast<CPlane*>(CDebugScene::FocusEntities_Get(0));
		CLandingGear& landingGear = pPlane->GetLandingGear();

		landingGear.ControlLandingGear(pPlane,ms_nCommand);	
	}
	if(CDebugScene::FocusEntities_Get(0) && CDebugScene::FocusEntities_Get(0)->GetIsTypeVehicle()
		&& static_cast<CVehicle*>(CDebugScene::FocusEntities_Get(0))->InheritsFromHeli())
	{
		CHeli* pHeli = static_cast<CHeli*>(CDebugScene::FocusEntities_Get(0));

		if( pHeli->HasLandingGear() )
		{
			CLandingGear& landingGear = pHeli->GetLandingGear();

			landingGear.ControlLandingGear( pHeli, ms_nCommand );	
		}
	}
}

void CLandingGear::InitWidgets(bkBank& bank)
{
	const char* strCommandNames[] =
	{
		"COMMAND_DEPLOY",
		"COMMAND_RETRACT",
		"COMMAND_DEPLOY_INSTANT",
		"COMMAND_RETRACT_INSTANT",
		"COMMAND_BREAK"
	};
	CompileTimeAssert(CLandingGear::NUM_COMMANDS == 5);

	bank.PushGroup("Landing Gear");
	bank.AddToggle("Disable landing gear", &ms_bDisableLandingGear);
	bank.AddCombo("Send command",(int*)&ms_nCommand,(int)CLandingGear::NUM_COMMANDS,strCommandNames);
	bank.AddButton("Send command", SendCommandCB);
	bank.AddSlider("Front gear door speed", &ms_fFrontDoorSpeed,0.0f,100.0f,0.01f);		

	bank.AddSlider("Deploying Rumble Duration", &ms_deployingRumbleDuration, 0, 500, 1);
	bank.AddSlider("Deploying Rumble Intensity", &ms_deployingRumbleIntensity, 0, 1.0f, 0.01f);
	bank.AddSlider("Deployed Rumble Duration", &ms_deployedRumbleDuration, 0, 500, 1);
	bank.AddSlider("Deployed Rumble Intensity", &ms_deployedRumbleIntensity, 0, 1.0f, 0.01f);

	bank.AddSlider("Retracting Rumble Duration", &ms_retractingRumbleDuration, 0, 500, 1);
	bank.AddSlider("Retracting Rumble Intensity", &ms_retractingRumbleIntensity, 0, 1.0f, 0.01f);
	bank.AddSlider("Retracted Rumble Duration", &ms_retractedRumbleDuration, 0, 500, 1);
	bank.AddSlider("Retracted Rumble Intensity", &ms_retractedRumbleIntensity, 0, 1.0f, 0.01f);
	bank.PopGroup();
}
#endif

