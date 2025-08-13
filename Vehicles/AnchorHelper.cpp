// RAGE headers:
#include "physics/constrainthalfspace.h"

// Framework headers:

// Game headers:
#include "vehicles/AnchorHelper.h"

#include "vehicles/Submarine.h"
#include "vehicles/boat.h"
#include "vehicles/Heli.h"
#include "vehicles/planes.h"
#include "vehicles/AmphibiousAutomobile.h"
#include "Peds/ped.h"

AI_VEHICLE_OPTIMISATIONS()
ENTITY_OPTIMISATIONS()
VEHICLE_OPTIMISATIONS()

// Statics ////////////////////////////////////////////////////////////////
#if __BANK
bool CAnchorHelper::ms_bDebugModeEnabled = false;
#endif // __BANK
///////////////////////////////////////////////////////////////////////////


CAnchorHelper::CAnchorHelper(CVehicle* pParent)
 : m_pParent(pParent)
{
	// Boats are created unanchored for now until they're created in the right place
	// could make it so that only the car generators set this flag.
	m_nFlags.bLockedToXY = false;//true;
	m_nFlags.bLowLodAnchorMode = false;
	m_nFlags.bForcePlayerBoatAnchor = false;
	m_nFlags.bForceLowLodMode = false;

	m_vAnchorFwdVec.Set(V_ZERO);
}

bool CAnchorHelper::IsVehicleAnchorable(const CVehicle* pVehicle)
{
	return pVehicle->InheritsFromBoat() || pVehicle->InheritsFromSubmarine() || pVehicle->pHandling->GetSeaPlaneHandlingData() || pVehicle->InheritsFromAmphibiousAutomobile();
}

CAnchorHelper* CAnchorHelper::GetAnchorHelperPtr(CVehicle* pVehicle)
{
	Assert(IsVehicleAnchorable(pVehicle));

	if(pVehicle->InheritsFromBoat())
	{
		return &(static_cast<CBoat*>(pVehicle)->GetAnchorHelper());
	}
	else if(CSubmarineHandling* pSubHandling = pVehicle->GetSubHandling())
	{
		return &(pSubHandling->GetAnchorHelper());
	}
	else if(pVehicle->InheritsFromPlane())
	{
		if(CSeaPlaneExtension* pSeaPlaneExtension = static_cast<CPlane*>(pVehicle)->GetExtension<CSeaPlaneExtension>())
		{
			return &(pSeaPlaneExtension->GetAnchorHelper());
		}
	}
	else if( pVehicle->InheritsFromHeli() )
	{
		if( CSeaPlaneExtension* pSeaPlaneExtension = static_cast<CHeli*>( pVehicle )->GetExtension<CSeaPlaneExtension>() )
		{
			return &( pSeaPlaneExtension->GetAnchorHelper() );
		}
	}
	else if(pVehicle->InheritsFromAmphibiousAutomobile())
	{
		CAmphibiousAutomobile* pAmphibiousAutomobile = static_cast<CAmphibiousAutomobile*>(pVehicle);

		return &pAmphibiousAutomobile->GetAnchorHelper();
	}

	return NULL;
}

void CAnchorHelper::CreateAnchorConstraints()
{
	Assert(m_pParent);

	const float fBoatHalfLength = m_pParent->GetBoundRadius();
	float fAnchorSeparation = rage::Clamp(0.1f*fBoatHalfLength, 0.3f, 1.0f);
	const Vector3 vThisPosition = VEC3V_TO_VECTOR3(m_pParent->GetTransform().GetPosition());
	Vector3 vecFwd = VEC3V_TO_VECTOR3(m_pParent->GetTransform().GetB());
	vecFwd.z = 0.0f;
	vecFwd.NormalizeFast();
	Vector3 vecSide = VEC3V_TO_VECTOR3(m_pParent->GetTransform().GetA());
	vecSide.z = 0.0f;
	vecSide.NormalizeFast();
	const Vector3 vProwPos = vThisPosition + fBoatHalfLength*vecFwd;
	const Vector3 vSternPos = vThisPosition - fBoatHalfLength*vecFwd;
#if __ASSERT
	const CEntity* pException = m_pParent;
	Matrix34 m = MAT34V_TO_MATRIX34(m_pParent->GetMatrix());
	
	if( !CanAnchorHere( true ) && m_pParent->TestVehicleBoundForCollision(
		&m, m_pParent, fwModelId::MI_INVALID, &pException, 1, NULL, ArchetypeFlags::GTA_ALL_MAP_TYPES))
	{
		vehicleWarningf("%s Anchoring a boat that's colliding with something (%f,%f,%f)", m_pParent->GetVehicleModelInfo()->GetModelName(),
																						vThisPosition.x, vThisPosition.y, vThisPosition.z);
	}
#endif

	Vector3 vOrigProwPos, vOrigSternPos;
	if(Unlikely(m_nFlags.bLockedToXY==0))
	{
		// Compute the positions to anchor the boat if we are creating the anchor instead of just switching from low LOD to full physics.
		vOrigProwPos = vProwPos;
		vOrigSternPos = vSternPos;
	}
	else
	{
		// If we get here we need to set up the constraints around the original anchor points or the boat will drift as it
		// switches from low LOD to high LOD anchor mode.
		vOrigProwPos = m_vAnchorPosProw;
		vOrigSternPos = m_vAnchorPosStern;
	}

	const Vector3 vecFwdOrig = m_vAnchorFwdVec;
	const Vector3 vecSideOrig = m_vAnchorSideVec;


	phConstraintHalfSpace::Params anchorConstraint;
	anchorConstraint.instanceA = m_pParent->GetCurrentPhysicsInst();

	// Half-space constraints for the prow.
	anchorConstraint.worldNormal = VECTOR3_TO_VEC3V(vecFwd);
	anchorConstraint.worldAnchorA = VECTOR3_TO_VEC3V(vProwPos);
	anchorConstraint.worldAnchorB = VECTOR3_TO_VEC3V(vOrigProwPos - fAnchorSeparation*vecFwd);
	PHCONSTRAINT->Insert(anchorConstraint);

	anchorConstraint.worldNormal = VECTOR3_TO_VEC3V(-vecFwd);
	anchorConstraint.worldAnchorA = VECTOR3_TO_VEC3V(vProwPos);
	anchorConstraint.worldAnchorB = VECTOR3_TO_VEC3V(vOrigProwPos + fAnchorSeparation*vecFwd);
	PHCONSTRAINT->Insert(anchorConstraint);

	anchorConstraint.worldNormal = VECTOR3_TO_VEC3V(vecSide);
	anchorConstraint.worldAnchorA = VECTOR3_TO_VEC3V(vProwPos);
	anchorConstraint.worldAnchorB = VECTOR3_TO_VEC3V(vOrigProwPos - fAnchorSeparation*vecSide);
	PHCONSTRAINT->Insert(anchorConstraint);

	anchorConstraint.worldNormal = VECTOR3_TO_VEC3V(-vecSide);
	anchorConstraint.worldAnchorA = VECTOR3_TO_VEC3V(vProwPos);
	anchorConstraint.worldAnchorB = VECTOR3_TO_VEC3V(vOrigProwPos + fAnchorSeparation*vecSide);
	PHCONSTRAINT->Insert(anchorConstraint);

	// Half-space constraints for the stern.
	anchorConstraint.worldNormal = VECTOR3_TO_VEC3V(vecFwd);
	anchorConstraint.worldAnchorA = VECTOR3_TO_VEC3V(vSternPos);
	anchorConstraint.worldAnchorB = VECTOR3_TO_VEC3V(vOrigSternPos - fAnchorSeparation*vecFwd);
	PHCONSTRAINT->Insert(anchorConstraint);

	anchorConstraint.worldNormal = VECTOR3_TO_VEC3V(-vecFwd);
	anchorConstraint.worldAnchorA = VECTOR3_TO_VEC3V(vSternPos);
	anchorConstraint.worldAnchorB = VECTOR3_TO_VEC3V(vOrigSternPos + fAnchorSeparation*vecFwd);
	PHCONSTRAINT->Insert(anchorConstraint);

	anchorConstraint.worldNormal = VECTOR3_TO_VEC3V(vecSide);
	anchorConstraint.worldAnchorA = VECTOR3_TO_VEC3V(vSternPos);
	anchorConstraint.worldAnchorB = VECTOR3_TO_VEC3V(vOrigSternPos - fAnchorSeparation*vecSide);
	PHCONSTRAINT->Insert(anchorConstraint);

	anchorConstraint.worldNormal = VECTOR3_TO_VEC3V(-vecSide);
	anchorConstraint.worldAnchorA = VECTOR3_TO_VEC3V(vSternPos);
	anchorConstraint.worldAnchorB = VECTOR3_TO_VEC3V(vOrigSternPos + fAnchorSeparation*vecSide);
	PHCONSTRAINT->Insert(anchorConstraint);
}

float CAnchorHelper::GetYawOffsetFromConstrainedOrientation() const
{
	// If this boat has never been anchored (we might be using low LOD mode as part of PLACE_ON_ROAD_PROPERLY) it
	// won't have a valid forward vector yet.
	if(m_vAnchorFwdVec.Mag2() > SMALL_FLOAT)
	{
		const Vector3 vFwdOrig = m_vAnchorFwdVec;
		Vector3 vFwdCurrent = VEC3V_TO_VECTOR3(m_pParent->GetTransform().GetB());

		return vFwdCurrent.AngleZ(vFwdOrig);
	}

	return 0.0f;
}

void CAnchorHelper::Anchor(bool bAnchor, bool bForceConstraints)
{
	Assert(m_pParent);

	if(bAnchor)
	{
		// Cache off the position at which the boat is being anchored so that it doesn't wander when the constraints
		// are reinserted on switching from low LOD to high LOD anchor mode.
		const Vector3 vThisPosition = VEC3V_TO_VECTOR3(m_pParent->GetTransform().GetPosition());
		Vector3 vecFwd = VEC3V_TO_VECTOR3(m_pParent->GetTransform().GetB());
		vecFwd.z = 0.0f;
		vecFwd.NormalizeFast();
		Vector3 vecSide = VEC3V_TO_VECTOR3(m_pParent->GetTransform().GetA());
		vecSide.z = 0.0f;
		vecSide.NormalizeFast();
		const float fBoatHalfLength = m_pParent->GetBoundRadius();
		m_vAnchorPosProw = vThisPosition + fBoatHalfLength*vecFwd;
		m_vAnchorPosStern = vThisPosition - fBoatHalfLength*vecFwd;
		m_vAnchorFwdVec = vecFwd;
		m_vAnchorSideVec = vecSide;

		if(m_pParent->GetCurrentPhysicsInst() && (!m_nFlags.bLockedToXY || bForceConstraints))
		{
			CreateAnchorConstraints();
		}
		m_nFlags.bLockedToXY = 1;
	}
	else
	{
		if(m_pParent->GetCurrentPhysicsInst() && (m_nFlags.bLockedToXY || bForceConstraints))
		{
			m_pParent->RemoveConstraints(m_pParent->GetCurrentPhysicsInst());

			// If we are in low LOD mode, make sure we wake up the physics instance here.
			if(m_nFlags.bLowLodAnchorMode)
			{
				// If we are currently in low LOD anchor mode, we need to toggle the low LOD flag off and make sure the boat physics is activated.
				phInst* pPhysInst = m_pParent->GetCurrentPhysicsInst();
				if(pPhysInst)
				{
					// Now this boat can activate again.
					pPhysInst->SetInstFlag(phInst::FLAG_NEVER_ACTIVATE, false);

					// ... and activate it since we no longer check for sleep overrides once a frame.
					if(pPhysInst->IsInLevel())
						CPhysics::GetSimulator()->ActivateObject(pPhysInst->GetLevelIndex());
				}

				// Ensure the "is in water" flag is up-to-date.
				m_pParent->SetIsInWater(true);

				m_nFlags.bLowLodAnchorMode = 0;
			}
		}
		m_nFlags.bLockedToXY = 0;

        if(!m_pParent->IsNetworkClone())
        {
		    // Reset the rudder angle. Useful when script are unanchoring boats once the occupants have been killed and
		    // stops the boats appearing to float off in different directions on rivers.
		    m_pParent->SetSteerAngle(0.0f);
        }

		// B*1916381: Reset the anchor forward vector to zero when unanchoring. 
		// This prevents old anchor data being used when processing low-LOD buoyancy for PLACE_ON_ROAD_PROPERLY and causing the vehicle to be facing the wrong direction.
		m_vAnchorFwdVec.Zero();
	}
}

bool CAnchorHelper::CanAnchorHere( bool excludeAllPedsStandingOnParent ) const
{
	// Do a box test with a tolerance of the maximum travel of the boat when constrained.
	// NB - This function takes into account the lateral travel allowed in the anchor constraints.

	Assert(m_pParent);

	const float fBoatHalfLength = m_pParent->GetBoundRadius();
	float fAnchorSeparation = rage::Clamp(0.1f*fBoatHalfLength, 0.3f, 1.0f);
	Vector3 vBoundBoxMax = m_pParent->GetBoundingBoxMax();
	Vector3 vBoundBoxMin = m_pParent->GetBoundingBoxMin();
	Vector3 vBoundBoxExtend(fAnchorSeparation, fAnchorSeparation, 1.0f);

	phBoundBox* pBoxBound = rage_new phBoundBox(vBoundBoxMax-vBoundBoxMin+vBoundBoxExtend);
	Matrix34 boatMat = MAT34V_TO_MATRIX34(m_pParent->GetMatrix());
	// Average the bounding box extents to find the centre of the bound.
	Vector3 vBoundCentre = vBoundBoxMax + vBoundBoxMin;
	vBoundCentre.Scale(0.5f);
	vBoundCentre += boatMat.d;
	boatMat.d = vBoundCentre;

	WorldProbe::CShapeTestBoundDesc boxDesc;
	boxDesc.SetBound(pBoxBound);
	boxDesc.SetTransform(&boatMat);
	boxDesc.SetIncludeFlags(ArchetypeFlags::GTA_VEHICLE_INCLUDE_TYPES);
	boxDesc.SetTypeFlags(ArchetypeFlags::GTA_SCRIPT_TEST);
	boxDesc.SetExcludeEntity(m_pParent);

	if(excludeAllPedsStandingOnParent)
	{
		for( int i = 0; i < MAX_NUM_PHYSICAL_PLAYERS; i++ )
		{
			CPed* ped = CGameWorld::FindPlayer( static_cast<PhysicalPlayerIndex>(i) );

			if( ped && ped->GetGroundPhysical() == m_pParent )
			{
				boxDesc.AddExcludeEntity(ped);
			}
		}

		CPed* pPed = nullptr;
		CPed::Pool* PedPool = CPed::GetPool();

		if (PedPool)
		{
			s32 i = PedPool->GetSize();

			while (i--)
			{
				pPed = PedPool->GetSlot(i);
				if (pPed && !pPed->IsLocalPlayer() && !pPed->IsPlayer() && pPed->GetGroundPhysical() == m_pParent)
				{
					boxDesc.AddExcludeEntity(pPed);
				}
			}
		}
	}

	boxDesc.SetContext(WorldProbe::EScript);

#if __BANK
	// For debug only, we are interested in the object(s) preventing anchoring.
	WorldProbe::CShapeTestResults debugResults;
	boxDesc.SetResultsStructure(&debugResults);
#endif // __BANK

	bool bHitSomething = WorldProbe::GetShapeTestManager()->SubmitTest(boxDesc);

#if __BANK
	// For debugging purposes, render the box tested for anchoring. Colour is red if CanAnchor returns false, green if true.
	// If CanAnchor detects a collision, we provide information on the object(s) hit.
	if(ms_bDebugModeEnabled)
	{
		const u32 knFramesToLive = 2000;
		
		Color32 colour = bHitSomething ? Color_red : Color_green;
		grcDebugDraw::BoxOriented(pBoxBound->GetBoundingBoxMin(), pBoxBound->GetBoundingBoxMax(), MATRIX34_TO_MAT34V(boatMat), colour, false, knFramesToLive);

		char zText[512];
		u32 nLevelIndex = phInst::INVALID_INDEX;
		if(m_pParent->GetCurrentPhysicsInst())
		{
			nLevelIndex = m_pParent->GetCurrentPhysicsInst()->GetLevelIndex();
		}
		sprintf(zText, "CAN ANCHOR TEST (%s:%d) - Frame: %d", m_pParent->GetModelName(), nLevelIndex, fwTimer::GetFrameCount());
		Vector3 vTextPos = VEC3V_TO_VECTOR3(pBoxBound->GetBoundingBoxMax());
		boatMat.Transform(vTextPos);
		grcDebugDraw::Text(vTextPos, Color_white, zText, true, knFramesToLive);

		if(bHitSomething)
		{
			for(WorldProbe::ResultIterator it = debugResults.begin(); it < debugResults.last_result(); ++it)
			{
				if(it->GetHitDetected())
				{
					grcDebugDraw::Cross(RCC_VEC3V(it->GetHitPosition()), 0.05f, Color_white, knFramesToLive, true);
					if(it->GetHitEntity())
					{
						sprintf(zText, "%s (lvl idx:%d)", it->GetHitEntity()->GetModelName(), it->GetLevelIndex());
						grcDebugDraw::Text(it->GetHitPosition(), Color_white, zText, true, knFramesToLive);
					}
				}
			}
		}
	}
#endif // __BANK

	pBoxBound->Release();

	bool bCanAmphibiousAnchor = true;
	if(m_pParent->InheritsFromAmphibiousAutomobile())
	{
		CAmphibiousAutomobile* pAmphibiousAutomobile = static_cast<CAmphibiousAutomobile*>(m_pParent);

		bCanAmphibiousAnchor = pAmphibiousAutomobile->GetNumContactWheels() == 0 && pAmphibiousAutomobile->IsPropellerSubmerged();
	}

	return !bHitSomething && bCanAmphibiousAnchor;
}

#if __BANK
void CAnchorHelper::RenderDebug() const
{
	if(!m_pParent)
	{
		return;
	}

	char zText[512];
	sprintf(zText, "Anchored: %s", IsAnchored()?"true":"false");
	grcDebugDraw::Text(m_pParent->GetVehiclePosition(), Color_white, 0, 0, zText);
}
#endif // __BANK