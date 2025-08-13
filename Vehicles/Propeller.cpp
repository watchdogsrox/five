#include "vehicles/Propeller.h"

#include "fwmaths/angle.h" 
#include "fwmaths/random.h"

#include "event/EventDamage.h"
#include "game/modelIndices.h"
#include "peds/ped.h"
#include "physics/physics.h"
#include "physics/gtaInst.h"
#include "vehicles/vehicle.h"
#include "vehicles/vehicle_channel.h"
#include "vehicles/vehicleDamage.h"
#include "vfx/systems/VfxMaterial.h"
#include "renderer/Water.h"
#include "pickups/Pickup.h"
#include "weapons/projectiles/Projectile.h"
#include "Weapons/Projectiles/ProjectileRocket.h"
#include "weapons/Weapon.h"
#include "weapons/WeaponDamage.h"
#include "vfx/Systems/VfxBlood.h"
#include "Task/Physics/TaskNMBalance.h"
#include "vehicles/Heli.h"
#include "vehicles/Planes.h"
#include "game/modelIndices.h"

#include "data/aes_init.h"
AES_INIT_E;

VEHICLE_OPTIMISATIONS();

//////////////////////////////////////////////////////////////////////////
// Class CPropeller
CPropeller::CPropeller() 
: m_uMainPropellerBone(PROPELLER_INVALID_BONE_INDEX), 
m_nRotationAxis(ROT_AXIS_LOCAL_Y),
m_fPropellerAngle(0.0f),
m_fPropellerSpeed(0.0f),
m_vDeformation(0.0f,0.0f,0.0f),
m_nReverse(0),
m_nVisible(true)
{		
}

void CPropeller::Init(eHierarchyId nComponent, eRotationAxis nRotateAxis, CVehicle* pParentVehicle)
{
	// Choose a random angle to start with
	m_fPropellerAngle = fwRandom::GetRandomNumberInRange(-PI,PI);

	m_nRotationAxis = nRotateAxis;

	// Configure the bone indices from the component
	if(nComponent > VEH_INVALID_ID)
	{
		int iMainBoneIndex = pParentVehicle->GetBoneIndex(nComponent);

		if(iMainBoneIndex > -1)
		{
			m_uMainPropellerBone = static_cast<u16>(iMainBoneIndex);

			if(pParentVehicle->InheritsFromPlane() && nComponent >= PLANE_PROP_1)
			{
				m_nReverse = ((nComponent - PLANE_PROP_1) % 2) == 1;
			}
		}
	}

	pParentVehicle->AssignBaseFlag(fwEntity::HAS_ALPHA_SHADOW, true);

}

void CPropeller::UpdatePropeller(float fPropSpeed, float fTimestep)
{
	m_fPropellerSpeed = fPropSpeed;
#if __BANK
	m_fPropellerSpeed += CVehicleDeformation::ms_fForcedPropellerSpeed;
#endif
	m_fPropellerAngle += (m_nReverse ? -m_fPropellerSpeed : m_fPropellerSpeed) * fTimestep;
	m_fPropellerAngle = fwAngle::LimitRadianAngleSafe(m_fPropellerAngle);
}

void CPropeller::SetPropellerSpeed(float fPropSpeed)
{
	m_fPropellerSpeed = fPropSpeed;
}

void CPropeller::PreRender(CVehicle* pParentVehicle)
{
	if(m_uMainPropellerBone != PROPELLER_INVALID_BONE_INDEX && !pParentVehicle->GetAnimatePropellers())
	{
		pParentVehicle->SetBoneRotation(m_uMainPropellerBone,static_cast<eRotationAxis>(m_nRotationAxis),m_fPropellerAngle,true,NULL,&m_vDeformation);
	}
}


dev_float fDefaultPropellerRadiusMult = 0.02f;	// Swap if we are travelling half a rotation per frame

// This is fraction of vehicle bound box Z
float CPropeller::GetSubmergeFraction(CVehicle* pParent)
{
	if(m_uMainPropellerBone == PROPELLER_INVALID_BONE_INDEX)
	{
		return 0.0f;
	}

	// Find the amount propeller is submerged
	// First find position of propeller
	Vector3 vPropellerPosition;
	pParent->GetDefaultBonePositionSimple((s32)m_uMainPropellerBone,vPropellerPosition);
	
	// Transform into world space
	vPropellerPosition = VEC3V_TO_VECTOR3(pParent->GetTransform().Transform(VECTOR3_TO_VEC3V(vPropellerPosition)));

	// Find water surface Z at XY coords
	float fWaterZ = 0.0f;
	float fSubmergeFraction = 0.0f;
	float fApproxPropellerRadius = pParent->GetBoundingBoxMax().z - pParent->GetBoundingBoxMin().z;
	fApproxPropellerRadius *= fDefaultPropellerRadiusMult;

	if(pParent->m_Buoyancy.GetWaterLevelIncludingRivers(vPropellerPosition,&fWaterZ,true, POOL_DEPTH, REJECTIONABOVEWATER, NULL))
	{
		if(fWaterZ > vPropellerPosition.z - fApproxPropellerRadius)
		{
			fSubmergeFraction = rage::Min(1.0f, (fWaterZ - vPropellerPosition.z + fApproxPropellerRadius) / fApproxPropellerRadius);
		}
	}

	return fSubmergeFraction;

}

#if __BANK
void CPropeller::InitWidgets(bkBank& bank)
{
	bank.PushGroup("Propeller");
		CPropellerBlurred::AddWidgetsToBank(bank);		
		CHeli::InitRotorWidgets(bank);
		CAutogyro::InitRotorWidgets(bank);
	bank.PopGroup();
}
#endif

void CPropeller::ApplyDeformation(CVehicle* pParentVehicle, const void* basePtr)
{
	s32 iMainBoneIndex = static_cast<s32>(m_uMainPropellerBone);

	Vector3 basePos;
	pParentVehicle->GetDefaultBonePositionSimple(iMainBoneIndex,basePos);
	
	m_vDeformation = VEC3V_TO_VECTOR3(pParentVehicle->GetVehicleDamage()->GetDeformation()->ReadFromVectorOffset(basePtr, VECTOR3_TO_VEC3V(basePos)));

	// Keep the rotor in center
	if(pParentVehicle->ShouldKeepRotorInCenter(this))
	{
		m_vDeformation.x = 0.0f;
		m_vDeformation.z = 0.0f;
	}
}

//////////////////////////////////////////////////////////////////////////
// Class: CPropellerBlurred
//////////////////////////////////////////////////////////////////////////
bank_float CPropellerBlurred::ms_fTransitionAngleChangeThreshold = (PI / 5.0f); // Displacement threshold at which to switch to blurred propeller
bank_float CPropellerBlurred::ms_fBlurredSpeedReduceFactor = 0.5f;

CPropellerBlurred::CPropellerBlurred() : m_uFastBoneIndex(PROPELLER_INVALID_BONE_INDEX),
m_uSlowBoneIndex(PROPELLER_INVALID_BONE_INDEX), m_fTimeInBlurredState(0.0f)
{

}

void CPropellerBlurred::Init(eHierarchyId nComponent, eRotationAxis nRotateAxis, CVehicle* pParentVehicle)
{
	CPropeller::Init(nComponent,nRotateAxis,pParentVehicle);

	if(m_uMainPropellerBone != PROPELLER_INVALID_BONE_INDEX)
	{
		// Search for child nodes of the right name
		const crSkeletonData& pSkelData = pParentVehicle->GetSkeletonData();

		s32 iMainBoneIndex = static_cast<s32>(m_uMainPropellerBone);

		const crBoneData* pBoneData = pSkelData.GetBoneData(iMainBoneIndex);

		vehicleAssert(pBoneData);

		// Search for fast and slow bone names
		char strSlowBoneName[64];
		char strFastBoneName[64];

		formatf(strSlowBoneName,64,"%s_slow",pBoneData->GetName());
		formatf(strFastBoneName,64,"%s_fast",pBoneData->GetName());

		pBoneData = pBoneData->GetChild();
		while(pBoneData)
		{
			const char* strBoneName = pBoneData->GetName();

			if(strcmp(strBoneName,strSlowBoneName) == 0)
			{
				m_uSlowBoneIndex = (u16)pBoneData->GetIndex();
			}
			else if(strcmp(strBoneName,strFastBoneName) == 0)
			{
				m_uFastBoneIndex = (u16)pBoneData->GetIndex();
			}
			pBoneData = pBoneData->GetNext();
		}
	}
}

static dev_float sfDefaultFrameRate = 33.0f;
void CPropellerBlurred::PreRender(CVehicle* pParentVehicle)
{
	if (!pParentVehicle->GetAnimatePropellers())
	{
		CPropeller::PreRender(pParentVehicle);

		float fBlurredSpeedThreshold = sfDefaultFrameRate * ms_fTransitionAngleChangeThreshold;

		// Choose between fast and slow prop
		bool bRenderFastProp = fabs(m_fPropellerSpeed)  > fBlurredSpeedThreshold;

		if(m_uFastBoneIndex != PROPELLER_INVALID_BONE_INDEX)
		{
			Matrix34& propMat = pParentVehicle->GetLocalMtxNonConst((s32)m_uFastBoneIndex);

			if( bRenderFastProp &&
				m_nVisible )
			{
				propMat.Identity3x3();
			}
			else
			{
				propMat.Zero3x3();
			}
		}

		if(m_uSlowBoneIndex != PROPELLER_INVALID_BONE_INDEX)
		{
			Matrix34& propMat = pParentVehicle->GetLocalMtxNonConst((s32)m_uSlowBoneIndex);

			if( !bRenderFastProp &&
				m_nVisible )
			{
				propMat.Identity3x3();
			}
			else
			{
				propMat.Zero3x3();
			}
		}
	}
	else
	{
		// B*1765835: Keep our propeller angle value in sync with whatever is happening while the propeller might be animated externally.
		// This prevents an obvious "pop" when the propeller goes from being animation driven to code driven.
		Matrix34& propMat = pParentVehicle->GetLocalMtxNonConst((s32)m_uMainPropellerBone);
		if(m_nRotationAxis == ROT_AXIS_LOCAL_X)
			m_fPropellerAngle = propMat.GetEulers().x;
		else if(m_nRotationAxis == ROT_AXIS_LOCAL_Y)
			m_fPropellerAngle = propMat.GetEulers().y;
		else if(m_nRotationAxis == ROT_AXIS_LOCAL_Z)
			m_fPropellerAngle = propMat.GetEulers().z;
	}
	
}

static dev_float sfBlurredTimeScaleRate = 0.5f;
void CPropellerBlurred::UpdatePropeller(float fPropSpeed, float fTimestep)
{
	// Scale the displacement threshold to be time step independent
	float fBlurredSpeedThreshold = fwTimer::GetInvTimeStep() * ms_fTransitionAngleChangeThreshold;
	bool bRenderFastProp = fabs(m_fPropellerSpeed) > fBlurredSpeedThreshold;
	
	// Gradually decrease speed as we're switching to blurred propeller
	float fEffectivePropSpeed = fPropSpeed;
	if(bRenderFastProp)
	{
		m_fTimeInBlurredState = rage::Clamp(m_fTimeInBlurredState + fTimestep * sfBlurredTimeScaleRate, 0.0f, 1.0f);
		float fSpeedMult = 1.0f - rage::Lerp(m_fTimeInBlurredState, 0.0f, ms_fBlurredSpeedReduceFactor);
		fEffectivePropSpeed *= fSpeedMult;
		fEffectivePropSpeed = Max(fEffectivePropSpeed, fBlurredSpeedThreshold);
	}
	else 
	{
		m_fTimeInBlurredState = 0.0f;
	}

	CPropeller::UpdatePropeller(fEffectivePropSpeed,fTimestep);
	m_fPropellerSpeed = fPropSpeed;
}

void CPropellerBlurred::SetPropellerSpeed(float fPropSpeed)
{
	bool bRenderFastProp = fabs(fPropSpeed) > sfDefaultFrameRate * ms_fTransitionAngleChangeThreshold;

	m_fTimeInBlurredState = bRenderFastProp ? 1.0f : 0.0f;

	CPropeller::SetPropellerSpeed(fPropSpeed);
}


#if __BANK
void CPropellerBlurred::AddWidgetsToBank(bkBank& bank)
{
	bank.AddSlider("Blur transition", &CPropellerBlurred::ms_fTransitionAngleChangeThreshold,0.01f,100.0f,0.01f);
	bank.AddSlider("ms_fBlurredSpeedReduceFactor",&CPropellerBlurred::ms_fBlurredSpeedReduceFactor,0.0f,1.0f,0.01f);
}
#endif


//////////////////////////////////////////////////////////////////////////
// Class: CPropellerCollisionProcesser::PropellerImpactData
//////////////////////////////////////////////////////////////////////////
CPropellerCollisionProcessor::CPropellerImpactData::CPropellerImpactData()
{
	m_pPropOwner = NULL;
	m_pOtherEntity = NULL;
	m_vecHitNorm = VEC3_ZERO;
	m_vecHitPos = VEC3_ZERO;
	m_vecOtherPosOffset = VEC3_ZERO;
	m_pPropellerCollision = NULL;
	m_nOtherComponent = -1;
	m_nMaterialId = 0;
	m_fPropSpeedMult = 0.0f;
}

void CPropellerCollisionProcessor::CPropellerImpactData::Reset()
{
	// For speed only null regd pointers... don't bother resetting everything else
	m_pPropOwner = NULL;
	m_pOtherEntity = NULL;
	m_pPropellerCollision = NULL;
}

//////////////////////////////////////////////////////////////////////////
// Class: CPropellerCollisionProcesser
//////////////////////////////////////////////////////////////////////////

// Initialise singleton
CPropellerCollisionProcessor CPropellerCollisionProcessor::sm_Instance;

CPropellerCollisionProcessor::CPropellerCollisionProcessor()
{
	m_nNumRotorImpacts = 0;
}

void CPropellerCollisionProcessor::Reset()
{
	for(int i = 0; i < NUM_STORED_PROPELLER_IMPACTS; i++)
	{
		m_aStoredImpacts[i].Reset();
	}
}

void CPropellerCollisionProcessor::ProcessImpacts(float fTimeStep)
{
	for(int i=0; i<m_nNumRotorImpacts; i++)
	{
		if(m_aStoredImpacts[i].m_pPropOwner && m_aStoredImpacts[i].m_pOtherEntity)
		{
			int nOtherComponent = m_aStoredImpacts[i].m_nOtherComponent;
			// put other entity pos offset back into world space
			Vector3 vecOtherEntityPos;
			CPed* pHitPed = m_aStoredImpacts[i].m_pOtherEntity->GetIsTypePed() ? static_cast<CPed*>( m_aStoredImpacts[i].m_pOtherEntity.Get() ) : NULL;
			if(pHitPed)
			{
				Vector3 vecPedPos;
				Matrix34 ragdollRootMatrix;
				if(pHitPed->GetRagdollComponentMatrix(ragdollRootMatrix, 0))
					vecPedPos = ragdollRootMatrix.d;
				else
					vecPedPos = VEC3V_TO_VECTOR3(m_aStoredImpacts[i].m_pOtherEntity->GetCurrentPhysicsInst()->GetMatrix().GetCol3());

				vecOtherEntityPos.Add(m_aStoredImpacts[i].m_vecOtherPosOffset, vecPedPos);


				// GTAV - Make sure we use the correct component from the current lod.
				fragInst* pFragInst = pHitPed->GetFragInst();
				if(pFragInst && pFragInst->GetCached() && pHitPed->GetRagdollInst())
				{
					nOtherComponent = pHitPed->GetRagdollInst()->MapRagdollLODComponentHighToCurrent( nOtherComponent );
				}

			}
			else
			{
				RCC_MATRIX34(m_aStoredImpacts[i].m_pOtherEntity->GetCurrentPhysicsInst()->GetMatrix()).Transform(m_aStoredImpacts[i].m_vecOtherPosOffset, vecOtherEntityPos);
			}

			m_aStoredImpacts[i].m_pPropellerCollision->ApplyImpact(
				m_aStoredImpacts[i].m_pPropOwner,
				m_aStoredImpacts[i].m_pOtherEntity,
				m_aStoredImpacts[i].m_vecHitNorm,
				m_aStoredImpacts[i].m_vecHitPos,
				m_aStoredImpacts[i].m_vecOtherPosOffset,
				nOtherComponent,
				m_aStoredImpacts[i].m_nPartId,
				m_aStoredImpacts[i].m_nMaterialId,
				m_aStoredImpacts[i].m_fPropSpeedMult,
				fTimeStep,
				m_aStoredImpacts[i].m_bIsPositiveDepth,
				m_aStoredImpacts[i].m_bIsNewContact
				);

		}

		m_aStoredImpacts[i].Reset();
	}
	m_nNumRotorImpacts = 0;
}

void CPropellerCollisionProcessor::AddPropellerImpact(CVehicle* pPropOwner, CEntity* pOtherEntity, const Vector3& vecNorm, const Vector3& vecPos,
													  CPropellerCollision* pPropellerCollision, int nOtherComponent, int nPartId,
													  phMaterialMgr::Id nMaterialId, float fPropSpeedMult, bool bIsPositiveDepth, bool bIsNewContact)
{
	Assert(pPropOwner);

	if(m_nNumRotorImpacts < NUM_STORED_PROPELLER_IMPACTS && pOtherEntity && pOtherEntity->GetCurrentPhysicsInst())
	{

		for(int i=0; i<m_nNumRotorImpacts; i++)
		{
			// if we've already got an impact with this inst, then ignore any subsequent ones
			if(m_aStoredImpacts[i].m_pPropOwner == pPropOwner && m_aStoredImpacts[i].m_pOtherEntity == pOtherEntity)
				return;
		}

		CPed* pHitPed = pOtherEntity->GetIsTypePed() ? static_cast<CPed*>( pOtherEntity ) : NULL;

		// GTAV - Make sure we use the correct component from the current lod.
		if( pHitPed )
		{
			fragInst* pFragInst = pHitPed->GetFragInst();
			if(pFragInst && pFragInst->GetCached() && pHitPed->GetRagdollInst())
			{
				nOtherComponent = pHitPed->GetRagdollInst()->MapRagdollLODComponentCurrentToHigh( nOtherComponent );
			}
		}


		m_aStoredImpacts[m_nNumRotorImpacts].m_pPropOwner = pPropOwner;
		m_aStoredImpacts[m_nNumRotorImpacts].m_pOtherEntity = pOtherEntity;
		m_aStoredImpacts[m_nNumRotorImpacts].m_vecHitNorm.Set(vecNorm);
		m_aStoredImpacts[m_nNumRotorImpacts].m_vecHitPos.Set(vecPos);
		m_aStoredImpacts[m_nNumRotorImpacts].m_pPropellerCollision = pPropellerCollision;
		m_aStoredImpacts[m_nNumRotorImpacts].m_nOtherComponent = nOtherComponent;
		m_aStoredImpacts[m_nNumRotorImpacts].m_nPartId = nPartId;
		m_aStoredImpacts[m_nNumRotorImpacts].m_nMaterialId = nMaterialId;
		m_aStoredImpacts[m_nNumRotorImpacts].m_fPropSpeedMult = fPropSpeedMult;
		m_aStoredImpacts[m_nNumRotorImpacts].m_bIsPositiveDepth = bIsPositiveDepth;
		m_aStoredImpacts[m_nNumRotorImpacts].m_bIsNewContact = bIsNewContact;

		// also store hit position as an offset from other entity's root
		// because it will probably move before we apply the impact
		Vector3 vecOtherEntityPos;
		if(pHitPed)
		{
			Vector3 vecPedPos;
			Matrix34 ragdollRootMatrix;
			if(pHitPed->GetRagdollComponentMatrix(ragdollRootMatrix, 0))
				vecPedPos = ragdollRootMatrix.d;
			else
				vecPedPos = VEC3V_TO_VECTOR3(pOtherEntity->GetCurrentPhysicsInst()->GetMatrix().GetCol3());

			vecOtherEntityPos.Subtract(vecPos, vecPedPos);
		}
		else
		{
			RCC_MATRIX34(pOtherEntity->GetCurrentPhysicsInst()->GetMatrix()).UnTransform(vecPos, vecOtherEntityPos);
		}
		m_aStoredImpacts[m_nNumRotorImpacts].m_vecOtherPosOffset.Set(vecOtherEntityPos);

		m_nNumRotorImpacts++;
	}
}

//////////////////////////////////////////////////////////////////////////
// Class: CPropellerCollision
//////////////////////////////////////////////////////////////////////////
CPropellerCollision::CPropellerCollision()
{
	m_iFragGroup = -1;
	m_iFragChild = -1;
	m_iFragDisc = -1;
}

void CPropellerCollision::Init(eHierarchyId nId, CVehicle* pOwner)
{
	int iBoneIndex = pOwner->GetBoneIndex(nId);

	Assert(pOwner->GetVehicleFragInst()->GetTypePhysics()->GetNumChildGroups() < 128);
	Assert(pOwner->GetVehicleFragInst()->GetTypePhysics()->GetNumChildren() < 128);

	if(iBoneIndex > -1)
	{
		m_iFragGroup = (s8)pOwner->GetVehicleFragInst()->GetGroupFromBoneIndex(iBoneIndex);
		if(m_iFragGroup > -1)
		{
			fragTypeGroup* pGroup = pOwner->GetVehicleFragInst()->GetTypePhysics()->GetAllGroups()[m_iFragGroup];
			m_iFragChild = (s8)pGroup->GetChildFragmentIndex();
			m_iFragDisc = m_iFragChild;

			// Find the disc fragment. The disc bound is the one that has maximum volume
			if(pOwner->GetCurrentPhysicsInst() && pOwner->GetCurrentPhysicsInst()->GetArchetype() && pOwner->GetCurrentPhysicsInst()->GetArchetype()->GetBound()
				&& pOwner->GetCurrentPhysicsInst()->GetArchetype()->GetBound()->GetType() == phBound::COMPOSITE)
			{
				phBoundComposite *pBound = (phBoundComposite *)pOwner->GetCurrentPhysicsInst()->GetArchetype()->GetBound();
				float fMaxVolume = 0.0f;
				for(int iChild = 0; iChild < pGroup->GetNumChildren(); iChild++)
				{
					if(pBound->GetBound(m_iFragChild + iChild) && pBound->GetBound(m_iFragChild + iChild)->GetVolume() > fMaxVolume)
					{
						m_iFragDisc = m_iFragChild + (s8)iChild;
						fMaxVolume = pBound->GetBound(m_iFragChild + iChild)->GetVolume();
					}
				}
			}
		}
		else
		{
			m_iFragChild = (s8)pOwner->GetVehicleFragInst()->GetComponentFromBoneIndex(iBoneIndex);
			m_iFragDisc = m_iFragChild;
		}
	}
}
dev_float dfPropellerPushSpeed = 2.0f;
void CPropellerCollision::ProcessPreComputeImpacts(CVehicle* pPropellerOwner, phContactIterator impact, float fPropSpeedMult)
{
	if(IsPropellerComponent(pPropellerOwner, impact.GetMyComponent()))
	{
		CEntity* pHitEntity = CPhysics::GetEntityFromInst(impact.GetOtherInstance());

		if(pHitEntity && (pHitEntity->GetIsClass<CProjectile>() || pHitEntity->GetIsClass<CProjectileRocket>()))
		{
			// just return. The projectile code should handle this correctly
			return;
		}
		if( pHitEntity && (pHitEntity->GetIsClass<CPickup>() ) )
		{
			return;
		}

		if(ShouldDisableAndIgnoreImpact(pPropellerOwner, pHitEntity, impact.GetDepth()))
		{
			impact.DisableImpact();
			return;
		}

		if( pHitEntity && pHitEntity->GetIsTypeObject() )
		{
			phMaterialMgr::Id otherMaterialId = PGTAMATERIALMGR->UnpackMtlId( impact.GetOtherMaterialId() );

			if( otherMaterialId == PGTAMATERIALMGR->g_idPhysVehicleRefill ||
				otherMaterialId == PGTAMATERIALMGR->g_idPhysVehicleSpeedUp ||
				otherMaterialId == PGTAMATERIALMGR->g_idPhysVehicleSlowDown ||
				otherMaterialId == PGTAMATERIALMGR->g_idPhysVehicleBoostCancel ||
				otherMaterialId == PGTAMATERIALMGR->g_idPhysPropPlacement 
				)
			{
				return;
			}
		}
		
		const Vector3& vecOtherPosition = VEC3V_TO_VECTOR3(impact.GetOtherPosition());
		Vector3 vecNormal;
		impact.GetMyNormal(vecNormal);
		phMaterialMgr::Id nMaterialId = PGTAMATERIALMGR->UnpackMtlId(impact.GetOtherMaterialId());

		// save this impact to be applied later (after the physics sim update
		CPropellerCollisionProcessor::GetInstance().AddPropellerImpact(pPropellerOwner, pHitEntity, vecNormal, vecOtherPosition, this,
			impact.GetOtherComponent(), impact.GetOtherElement(), nMaterialId, fPropSpeedMult, impact.GetContact().IsPositiveDepth(), impact.GetContact().GetLifetime() == 1);

		if(ShouldDisableImpact(pPropellerOwner, pHitEntity, impact.GetMyComponent(), fPropSpeedMult))
		{
			impact.DisableImpact();
		}
		else
		{
			if( fPropSpeedMult == 0.0f &&
				pHitEntity &&
				pHitEntity->GetIsTypePed() && 
				static_cast< CPed* >( pHitEntity )->GetGroundPhysical() != pHitEntity &&
				pPropellerOwner->GetModelIndex() == MI_PLANE_MOGUL )
			{
				static dev_float minYToRemoveContact = 0.2f;
				if( vecNormal.y > minYToRemoveContact )
				{
					impact.DisableImpact();
					return;
				}
			}
			
			impact.SetDepth(Min(dfPropellerPushSpeed * fwTimer::GetTimeStep(), impact.GetDepth()));
			impact.SetMyPositionLocal(Vec3V(0.0f, 0.0f, 0.0f));
		}
	}
}

bool CPropellerCollision::ShouldDisableImpact(CVehicle *pOwnerVehicle, CEntity *pOtherEntity, int nOwnerComponent, float fPropSpeedMult)
{
	if(pOtherEntity==NULL || pOtherEntity->GetCurrentPhysicsInst()==NULL)
		return true;

	if(pOwnerVehicle == NULL || pOwnerVehicle->GetVehicleFragInst() == NULL)
	{
		return true;
	}

	if(pOwnerVehicle->InheritsFromHeli())
	{
		if(GetFragDisc() == nOwnerComponent && fPropSpeedMult > 0.0f && pOtherEntity->GetIsTypeBuilding())
		{
			return false;
		}

		if(GetFragDisc() != nOwnerComponent && fPropSpeedMult == 0.0f)
		{
			return false;
		}
	}
	else if(pOwnerVehicle->InheritsFromPlane())
	{
		if(((CPlane *)pOwnerVehicle)->GetBreakOffPropellerOnContact() && pOtherEntity->GetIsTypeBuilding())
		{
			return false;
		}

		if(GetFragDisc() != nOwnerComponent && fPropSpeedMult == 0.0f)
		{
			return false;
		}
	}

	return true;
}

const float VELUM_PROPELLER_BOUND_SHRINKING_DEPTH = 0.25f;
bool CPropellerCollision::ShouldDisableAndIgnoreImpact(CVehicle *pOwnerVehicle, CEntity *pOtherEntity, float fCollisionDepth) const
{
	if(pOtherEntity && pOtherEntity->GetIsTypeObject() && static_cast<CObject *>(pOtherEntity)->GetIsParachute())
	{
		return true;
	}

	if(pOwnerVehicle->InheritsFromPlane())
	{
		if(pOtherEntity && pOwnerVehicle->GetModelIndex() == MI_PLANE_VELUM && (pOtherEntity->GetIsTypeBuilding() || pOtherEntity->GetIsTypeObject()))
		{
			if(fCollisionDepth < VELUM_PROPELLER_BOUND_SHRINKING_DEPTH)
			{
				return true;
			}
		}
	}

	// in MP collisions between certain vehicles can be disabled (eg ghost vehicles in a race)
	if (pOwnerVehicle && pOtherEntity && NetworkInterface::AreInteractionsDisabledInMP(*pOwnerVehicle, *pOtherEntity))
	{
		return true;
	}

	return false;
}

dev_float fImpulseSpeedThreshold = 50.0f;
dev_float fPropellerPedDamage = 1000.0f;
dev_float fScaleMassMax = 200.0f;
dev_float fImpulseLinearMult = 2.0f;
dev_float fImpulseTorqueMult = 1.3f;
dev_float fImpulseRotorSpin = 0.7f;
dev_float fImpulseApplyToOtherMult = 10.0f;
dev_float fForceApplyToOtherLimit = 75.0f;
dev_float fMinimumRotorSpeedForDamage = 0.05f;

void CPropellerCollision::ApplyImpact(CVehicle* pOwnerVehicle, CEntity* pOtherEntity, const Vector3& vecNormal, const Vector3& vecPos,
									  const Vector3& vecOtherPosOffset, int nOtherComp, int nPartIndex, phMaterialMgr::Id nMaterialId,
									  float fPropSpeedMult, float fTimeStep, bool bIsPositiveDepth, bool bIsNewContact)
{
	if(pOtherEntity==NULL || pOtherEntity->GetCurrentPhysicsInst()==NULL)
		return;

	if(pOwnerVehicle == NULL || pOwnerVehicle->GetVehicleFragInst() == NULL)
	{
		return;
	}

	if( Abs( fPropSpeedMult ) <= fMinimumRotorSpeedForDamage )
	{
		return;
	}

	Assert(m_iFragDisc > -1);

#if __DEV
	static dev_bool sbDisplayRotorImpacts = false;
	if(sbDisplayRotorImpacts)
	{
		grcDebugDraw::Line(vecPos, vecPos + 0.2f * vecNormal, Color_red);
		grcDebugDraw::Sphere(vecPos, 0.02f, Color_green, false);
	}
#endif

	// check for projectiles hitting heli blades, and force them to explode
	if(pOtherEntity->GetIsTypeObject())
	{
		CObject* pOtherObject = static_cast<CObject*>(pOtherEntity);
		CProjectile* pProjectile = pOtherObject->GetAsProjectile();
		if(pProjectile && pProjectile->GetOwner() != pOwnerVehicle)
		{
			pProjectile->TriggerExplosion();
			return;
		}
	}
	
	// if heli blades hit a ped, apply damage
	if(pOtherEntity->GetIsTypePed())
	{
		CPed* pPed = (CPed*)pOtherEntity;

		// Check that this ped allows collision damage.
		if(pPed->m_nPhysicalFlags.bNotDamagedByCollisions || pPed->m_nPhysicalFlags.bNotDamagedByAnything)
		{
			return;
		}

		// If they are entering the vehicle they hit, ignore the damage
		if( pPed->GetIsEnteringVehicle() && pPed->GetVehiclePedEntering() == pOwnerVehicle )
		{
			return;
		}

		// Otherwise go through with the damage
		Vector3 vecStart(vecPos);

		// the ped will be switched into ragdoll inside generate ragdoll task (if possible)
		CWeaponDamage::GeneratePedDamageEvent(pOwnerVehicle, pPed, WEAPONTYPE_ROTORS, fPropellerPedDamage, vecStart, NULL, CPedDamageCalculator::DF_None);
	}
	else if(pOtherEntity->GetIsTypeVehicle() && ((CVehicle *)pOtherEntity)->InheritsFromBike())
	{
		Vector3 vecStart(vecPos);
		CVehicle* pBike = (CVehicle*)pOtherEntity;
		for(s32 i=0; i<pBike->GetSeatManager()->GetMaxSeats(); i++)
		{
			CPed* pPassenger = pBike->GetSeatManager()->GetPedInSeat(i);
			// Check that this ped allows collision damage.
			if(pPassenger && !(pPassenger->m_nPhysicalFlags.bNotDamagedByCollisions || pPassenger->m_nPhysicalFlags.bNotDamagedByAnything))
			{
				CWeaponDamage::GeneratePedDamageEvent(pOwnerVehicle, pPassenger, WEAPONTYPE_ROTORS, fPropellerPedDamage, vecStart, NULL, CPedDamageCalculator::DF_None);
			}
		}
	}

	// clamp offset in case vehicle has moved a lot since impact was stored
	Vector3 vecOffset = vecPos - VEC3V_TO_VECTOR3(pOwnerVehicle->GetTransform().GetPosition());
	if(vecOffset.Mag2() > square(pOwnerVehicle->GetBoundRadius()))
	{
		vecOffset.Scale(pOwnerVehicle->GetBoundRadius() / vecOffset.Mag());
	}

	Vector3 vecImpactSpeed = pOwnerVehicle->GetLocalSpeed(vecOffset, false);
	Vector3 vecForce = VEC3_ZERO;
	float fImpactSpeed = vecNormal.Dot(vecImpactSpeed);
    fImpactSpeed = Clamp(fImpactSpeed, -fImpulseSpeedThreshold, fImpulseSpeedThreshold);
	// Don't think we need to check this. It stops the other entity reacting to the rotor impact sometimes. 
	//if(fImpactSpeed < 0.0f)
	{
		vecForce = vecNormal;
		vecForce.Scale(-fImpactSpeed * fPropSpeedMult);

		Vector3 vecLinearForce(vecForce);
		vecLinearForce.Scale(fImpulseLinearMult*pOwnerVehicle->GetMass());
		vecLinearForce.ClampMag(0.0f, 149.9f*rage::Max(1.0f, pOwnerVehicle->GetMass()));
		pOwnerVehicle->ApplyInternalForceCg(vecLinearForce);

		float fMassAlongVec = pOwnerVehicle->GetMassAlongVectorLocal( vecNormal, vecOffset );
		Vector3 vecTorque(vecForce);
		vecTorque.Scale(fImpulseTorqueMult*fMassAlongVec);
		vecTorque.Cross(vecOffset);
		pOwnerVehicle->ApplyInternalTorque(vecTorque);

		
		// Find the bone index of this frag child
		int iBoneIndex = pOwnerVehicle->GetVehicleFragInst()->GetType()->GetBoneIndexFromID(
			pOwnerVehicle->GetVehicleFragInst()->GetTypePhysics()->GetAllChildren()[m_iFragDisc]->GetBoneID());
		Assert(iBoneIndex > -1);
		Vector3 vecSpinTorque = ZAXIS;

		Matrix34 matBone;
		pOwnerVehicle->GetGlobalMtx(iBoneIndex, matBone);
		vecSpinTorque = matBone.c;

		vecSpinTorque.Scale(fImpulseRotorSpin * pOwnerVehicle->GetAngInertia().z);
		pOwnerVehicle->ApplyInternalTorque(vecSpinTorque);

		// add this contribution to linear impulse, so we can apply this to other entity
		vecTorque.Cross(vecSpinTorque, vecPos - matBone.d);
		vecLinearForce.Add(vecTorque);

		if(pOtherEntity->GetIsPhysical())
		{
			// check if this is a frag inst that has already had this component broken off
			if(!pOtherEntity->GetFragInst() || !pOtherEntity->GetFragInst()->GetChildBroken(nOtherComp))
			{
				CPhysical* pOtherPhysical = static_cast<CPhysical*>(pOtherEntity);
				float fHitMass = pOtherPhysical->GetMass();

				// clamp the mass used to scale impact forces, so we don't push heavy things so much
				fHitMass = rage::Min(fHitMass, fScaleMassMax);

				vecLinearForce.Scale(fImpulseApplyToOtherMult);
				if(vecLinearForce.Mag2() > square(fForceApplyToOtherLimit * fHitMass))
					vecLinearForce.Scale((fForceApplyToOtherLimit * fHitMass) / vecLinearForce.Mag());

				Vector3 vecHitPosition; 
				MAT34V_TO_MATRIX34(pOtherEntity->GetTransform().GetMatrix()).Transform(vecOtherPosOffset, vecHitPosition);
				Vector3 vecOffsetPosition = vecHitPosition - VEC3V_TO_VECTOR3(pOtherEntity->GetTransform().GetPosition());

				// fragable objects can mean we are trying to hit things too far away from the centroid, so double check we are ok.
				Vec3V vWorldCentroid;
				float fCentroidRadius = 0.0f;
				if(pOtherEntity->GetCurrentPhysicsInst() && pOtherEntity->GetCurrentPhysicsInst()->GetArchetype())
				{
					vWorldCentroid = pOtherEntity->GetCurrentPhysicsInst()->GetArchetype()->GetBound()->GetWorldCentroid(pOtherEntity->GetCurrentPhysicsInst()->GetMatrix());
					fCentroidRadius = pOtherEntity->GetCurrentPhysicsInst()->GetArchetype()->GetBound()->GetRadiusAroundCentroid();
				}
				else
					fCentroidRadius = pOtherEntity->GetBoundCentreAndRadius(RC_VECTOR3(vWorldCentroid));

				if(DistSquared(VECTOR3_TO_VEC3V(vecHitPosition), vWorldCentroid).Getf() <= fCentroidRadius*fCentroidRadius)
				{
					if(pOtherEntity->GetIsTypePed())
					{
						CPed* pPed = (CPed*)pOtherEntity;

						if(pPed->GetUsingRagdoll())
						{
							pPed->ApplyForce(vecLinearForce, vecOffsetPosition, nOtherComp);
						}
					}
					else
					{
						static_cast<CPhysical*>(pOtherEntity)->ApplyExternalForce(vecLinearForce, vecOffsetPosition, nOtherComp, nPartIndex, NULL, 500.0f);
					}
				}
				else if(pOtherEntity->GetIsTypePed() && ((CPed*)pOtherEntity)->GetUsingRagdoll()) // shorten the hit offset to avoid the assert
				{
					Vector3 vecNewHitPosition = vecHitPosition - VEC3V_TO_VECTOR3(vWorldCentroid);
					vecNewHitPosition.Normalize();
					vecNewHitPosition = VEC3V_TO_VECTOR3(vWorldCentroid) + vecNewHitPosition * fCentroidRadius;

					vecOffsetPosition = vecNewHitPosition - VEC3V_TO_VECTOR3(pOtherEntity->GetTransform().GetPosition());
					((CPed*)pOtherEntity)->ApplyForce(vecLinearForce, vecOffsetPosition, nOtherComp);
				}
			}
		}		
	}

	// Trigger effects
	if(pOwnerVehicle->GetIsVisible() && (!pOwnerVehicle->GetVehicleFragInst() || !pOwnerVehicle->GetVehicleFragInst()->GetChildBroken(m_iFragDisc)))
	{
		// override default materials with concrete as a lot of high buildings are set to default
		if (PGTAMATERIALMGR->UnpackMtlId(nMaterialId)==PGTAMATERIALMGR->g_idDefault)
		{
			nMaterialId = PGTAMATERIALMGR->g_idConcrete;
		}

		if (pOtherEntity)// && CPhysics::GetIsLastTimeSlice(CPhysics::GetCurrentTimeSlice()))//&& (fwTimer::GetSystemFrameCount()%4)==0)
		{
			// do collision effects
			Vector3 collPos = vecPos;
			Vector3 collNormal = vecNormal;
			//Vector3 collNormalNeg = -vecNormal;
			//		Vector3 collVel = vecImpulse;
			Vector3 collDir(0.0f, 0.0f, 0.0f);

			Vector3 collPtToVehCentre = VEC3V_TO_VECTOR3(pOwnerVehicle->GetTransform().GetPosition()) - collPos;
			collPtToVehCentre.Normalize();
			Vector3 collVel = CrossProduct(collPtToVehCentre, VEC3V_TO_VECTOR3(pOwnerVehicle->GetTransform().GetC()));
			collVel.Normalize();

			float scrapeMag = 100000.0f;
			float accumImpulse = 100000.0f;

			g_vfxMaterial.DoMtlScrapeFx(pOwnerVehicle, 0, pOtherEntity, 0, RCC_VEC3V(collPos), RCC_VEC3V(collNormal), RCC_VEC3V(collVel), PGTAMATERIALMGR->g_idCarMetal, nMaterialId, RCC_VEC3V(collDir), scrapeMag, accumImpulse, VFXMATERIAL_LOD_RANGE_SCALE_HELI, 1.0f, 1.0);
//			g_vfxMaterial.DoMtlScrapeFx(pOtherEntity, 0, pOwnerVehicle, 0, collPos, collNormal, collVel, nMaterialId, PGTAMATERIALMGR->g_idCarMetal, collDir, scrapeMag, accumImpulse, VFXMATERIAL_LOD_RANGE_SCALE_HELI, 1.0f, 1.0);

            if (pOtherEntity->GetIsTypePed())
            {
                g_vfxBlood.UpdatePtFxBloodMist(static_cast<CPed*>(pOtherEntity));
            }

			pOtherEntity->ProcessFxEntityCollision(collPos, collNormal, 0, accumImpulse);
		}

        if( pOtherEntity->GetCurrentPhysicsInst()->IsInLevel() )
        {
		    WorldProbe::CShapeTestFixedResults<> tempResults;
		    tempResults[0].SetHitComponent((u16)nOtherComp);
		    tempResults[0].SetHitInst(
			    pOtherEntity->GetCurrentPhysicsInst()->GetLevelIndex(), 
    #if LEVELNEW_GENERATION_IDS
			    PHLEVEL->GetGenerationID(pOtherEntity->GetCurrentPhysicsInst()->GetLevelIndex())
    #else
			    0
    #endif // LEVELNEW_GENERATION_IDS
			    );
		    tempResults[0].SetHitMaterialId(nMaterialId);
		    tempResults[0].SetHitPosition(vecPos);
		    g_CollisionAudioEntity.ReportHeliBladeCollision(&tempResults[0], pOwnerVehicle);
        }
	}

    if( pOtherEntity->GetCurrentPhysicsInst()->IsInLevel() )
    {
	    // Apply impacts back on to the vehilce through the process collision call
	    // This won't have been called for this impact because the impact was disabled

	    // impulse that gets passed down for damage is just mass * velocity
	    if(vecForce.IsNonZero())
	    {
		    vecForce.Scale(pOwnerVehicle->GetMass());

		    // Need to convert the force into an impulse for collision processing
		    vecForce.Scale(fTimeStep);
	    }
    	
	    //Assume that it's the current insts that are involved in the collision, which isn't necessarily true
	    pOwnerVehicle->ProcessCollision(pOwnerVehicle->GetCurrentPhysicsInst(), pOtherEntity, pOtherEntity->GetCurrentPhysicsInst(), vecPos,
			vecOtherPosOffset, Mag(RCC_VEC3V(vecForce)).Getf(), vecNormal, GetFragChild(), nOtherComp, nMaterialId, bIsPositiveDepth, bIsNewContact);
    }
}

void CPropellerCollision::UpdateBound(CVehicle *pOwnerVehicle, int iBoneIndex, float fAngle, int iAxis, bool fullUpdate)
{
	if(iBoneIndex > -1 && m_iFragGroup > -1 && m_iFragChild > -1)
	{
		fragTypeGroup* pGroup = pOwnerVehicle->GetVehicleFragInst()->GetTypePhysics()->GetAllGroups()[m_iFragGroup];
		if(pGroup->GetNumChildren() > 1)
		{
			if(	pOwnerVehicle->GetCurrentPhysicsInst() && 
				pOwnerVehicle->GetCurrentPhysicsInst()->GetArchetype() &&
				pOwnerVehicle->GetCurrentPhysicsInst()->GetArchetype()->GetBound() &&
				phBound::IsTypeComposite(pOwnerVehicle->GetCurrentPhysicsInst()->GetArchetype()->GetBound()->GetType()))
			{
				phBoundComposite *pBound = (phBoundComposite *)pOwnerVehicle->GetCurrentPhysicsInst()->GetArchetype()->GetBound();
				if(pBound->GetCurrentMatrices() && pBound->GetLastMatrices())
				{
					pOwnerVehicle->SetBoneRotation(iBoneIndex,static_cast<eRotationAxis>(iAxis),fAngle,true,NULL,NULL);
					// Need to make sure rotor bones have their object matrices updated; SetBoneRotation only changes the locals.
					pOwnerVehicle->GetSkeleton()->PartialUpdate(iBoneIndex);

					const Matrix34 &matBone = pOwnerVehicle->GetObjectMtx(iBoneIndex);
					
					Matrix34 matLink = matBone;
					phArticulatedCollider *pCollider = NULL;

					// B*1855069: Using the collider from the frag instance cache entry so that link attachment matrices can be kept up-to-date even once the vehicle has become inactive.
					if(pOwnerVehicle->GetFragInst() && pOwnerVehicle->GetFragInst()->GetCacheEntry() && pOwnerVehicle->GetFragInst()->GetCacheEntry()->GetHierInst() 
						&& pOwnerVehicle->GetFragInst()->GetCacheEntry()->GetHierInst()->articulatedCollider
						&& pOwnerVehicle->GetFragInst()->GetCacheEntry()->GetHierInst()->articulatedCollider->GetBody())
					{
						pCollider = pOwnerVehicle->GetFragInst()->GetCacheEntry()->GetHierInst()->articulatedCollider;

						//I think this is correct but we are so close to releasing this pack I'm special casing this for the TULA only
						if( MI_PLANE_TULA.IsValid() &&
							pOwnerVehicle->GetModelIndex() == MI_PLANE_TULA )
						{
							matLink.d -= VEC3V_TO_VECTOR3(pBound->GetCGOffset());
						}
						else
						{
							if(const Matrix34 *pMat = pCollider->GetLinkAttachmentMatrices())
							{							
								matLink.d = pMat[m_iFragChild].d;
							}
						}
					}

					// B*1855069: All bounds (including the disc bound) get rotated by bound update dependency thread anyway so we should make sure current and last matrices are kept in sync with the bone.	
					for(int iChild = 0; iChild < pGroup->GetNumChildren(); iChild++)
					{												
						pBound->SetCurrentMatrix(m_iFragChild + iChild, MATRIX34_TO_MAT34V(matBone));
						pBound->SetLastMatrix(m_iFragChild + iChild, MATRIX34_TO_MAT34V(matBone));

						// Keep articulated collider link attachments in sync.
						if(pCollider)
						{
							if(Matrix34 *pMat = (Matrix34 *)pCollider->GetLinkAttachmentMatrices())
							{
								pMat[m_iFragChild + iChild] = matLink;
							}
						}
					}
							
					// B*1855069: We don't need to update the composite extents if we're no longer interested in collision with the box propellers, which happens when the propeller is spinning fast enough.
					if(fullUpdate)
					{
						// Update the composite bounds and BVH
						pBound->CalculateCompositeExtents(true);
						if(pOwnerVehicle->GetCurrentPhysicsInst()->IsInLevel())
						{
							CPhysics::GetLevel()->UpdateCompositeBvh(pOwnerVehicle->GetCurrentPhysicsInst()->GetLevelIndex());
						}
						else
						{
							pBound->UpdateBvh(false);
						}
					}
				}
			}
		}
	}
}

bool CPropellerCollision::IsPropellerComponent(const CVehicle *pOwnerVehicle, int nComponent) const
{
	if(GetFragChild() == nComponent)
	{
		return true;
	}
	else if(GetFragGroup() > -1)
	{
		fragTypeGroup* pGroup = pOwnerVehicle->GetVehicleFragInst()->GetTypePhysics()->GetAllGroups()[GetFragGroup()];
		if(pGroup->GetNumChildren() > 1)
		{
			for(int iChild = 1; iChild < pGroup->GetNumChildren(); iChild++)
			{
				if(GetFragChild() + iChild == nComponent)
				{
					return true;
				}
			}
		}
	}

	return false;
}
