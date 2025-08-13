
#include "TaskSlopeScramble.h"

#include "Weapons/WeaponDamage.h"

#include "animation/Move.h"
#include "animation/MovePed.h"

#include "event/Events.h"
#include "math/angmath.h"
#include "Debug/debugscene.h"
#include "Peds/PedIntelligence.h"
#include "System\Control.h"
#include "Task/Default/TaskPlayer.h"
#include "Task/Physics/TaskNMHighFall.h"
#include "Task/System/MotionTaskData.h"
#include "physics/physics.h"
#include "physics\gtaInst.h"
#include "Task/Motion/Locomotion/TaskMotionAiming.h"
#include "Task/Combat/TaskCombatMelee.h"

AI_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////

const fwMvBooleanId	CTaskSlopeScramble::ms_OnChosenForwardId("OnChosenForward",0xC7E47D87);
const fwMvBooleanId	CTaskSlopeScramble::ms_OnChosenBackwardId("OnChosenBackward",0x7641C26E);
const fwMvRequestId	CTaskSlopeScramble::ms_GoToScrambleForwardId("ToScrambleForward",0x246BC6B1);
const fwMvRequestId	CTaskSlopeScramble::ms_GoToScrambleBackwardId("ToScrambleBackward",0xEF0CC0B2);
const fwMvRequestId	CTaskSlopeScramble::ms_GoToExitId("ToExit",0x5A47660E);
const fwMvBooleanId CTaskSlopeScramble::ms_OnOutroComplete("OnOutroComplete",0x4404F42A);
const fwMvBooleanId CTaskSlopeScramble::ms_InterruptibleId("Interrupt",0x6D6BC7B7);
const fwMvFlagId	CTaskSlopeScramble::ms_FirstPersonMode("FirstPersonMode",0x8BB6FFFA);
const fwMvFlagId	CTaskSlopeScramble::ms_HasGrip("HasGrip",0x1bcd14c6);
const fwMvFloatId	CTaskSlopeScramble::ms_PitchId("Pitch",0x3F4BB8CC);
const fwMvClipSetVarId	CTaskSlopeScramble::ms_UpperBodyClipSetId("UpperBodyClipSet",0x49bb9318);
const fwMvClipId CTaskSlopeScramble::ms_WeaponBlockedClipId(ATSTRINGHASH("wall_block", 0x0ea90630e));
const fwMvClipId CTaskSlopeScramble::ms_GripClipId(ATSTRINGHASH("grip_clip", 0x2920a474));

//////////////////////////////////////////////////////////////////////////

CTaskFSMClone* CClonedSlopeScrambleInfo::CreateCloneFSMTask()
{
	return rage_new CTaskSlopeScramble();
}

CTask* CClonedSlopeScrambleInfo::CreateTaskFromTaskInfo(fwEntity* /*pEntity*/)
{
	return rage_new CTaskSlopeScramble();
}

float	CTaskSlopeScramble::GetBlendInDuration()
{
	TUNE_GROUP_FLOAT (SLOPE_SCRAMBLE, fSlopeScrambleBlendInDuration, REALLY_SLOW_BLEND_DURATION, 0.0f, MIGRATE_SLOW_BLEND_DURATION, 0.01f);
	return fSlopeScrambleBlendInDuration;
}

float	CTaskSlopeScramble::GetBlendOutDuration()
{
	TUNE_GROUP_FLOAT (SLOPE_SCRAMBLE, fSlopeScrambleBlendOutDuration, REALLY_SLOW_BLEND_DURATION, 0.0f, MIGRATE_SLOW_BLEND_DURATION, 0.01f);
	return fSlopeScrambleBlendOutDuration;
}

float	CTaskSlopeScramble::GetActivationAngle()
{
	TUNE_GROUP_FLOAT (SLOPE_SCRAMBLE, fSlopeScrambleActiveAngle, 45.0f, 0.0f, 90.0f, 0.01f);
	return fSlopeScrambleActiveAngle;
}

float	CTaskSlopeScramble::GetDeActivationAngle()
{
	TUNE_GROUP_FLOAT (SLOPE_SCRAMBLE, fSlopeScrambleDeActiveAngle, 48.0f, 0.0f, 90.0f, 0.01f);
	return fSlopeScrambleDeActiveAngle;
}

float	CTaskSlopeScramble::GetProbeTestAngle()
{
	TUNE_GROUP_FLOAT (SLOPE_SCRAMBLE, fSlopeScrambleProbeTestAngle, 25.0f, 0.0f, 90.0f, 0.01f);
	return fSlopeScrambleProbeTestAngle;
}

bool	CTaskSlopeScramble::GetIsTooSteep(const Vector3 &normal)
{
	const float cosSlopeAngle = rage::Cosf( DtoR * GetActivationAngle() );
	return ( abs(normal.z) < cosSlopeAngle );
}

bool	CTaskSlopeScramble::GetIsTooShallow(const Vector3 &normal)
{
	const float cosSlopeAngle = rage::Cosf( DtoR * GetDeActivationAngle() );
	return ( abs(normal.z) >= cosSlopeAngle );
}

// Checks if the ped is stood on an entity
bool	CTaskSlopeScramble::IsStoodOnEntity(CPed *pPed)
{
	CEntity *pStoodOnEntity = pPed->GetGroundPhysical();
	if( pStoodOnEntity )
	{
		// TODO: check explicitly for a car or object? Or just assume anything?
		return true;
	}
	return false;
}

// Checks if the surface of the probe result is an entity
bool	CTaskSlopeScramble::IsEntitySurface(WorldProbe::CShapeTestHitPoint &result)
{
	CEntity *pEntity = CPhysics::GetEntityFromInst(result.GetHitInst());
	if(pEntity)
	{
		// BODGE!!!
		// WorldProbe returns valid entity pointers when it hasn't hit an entity.
		// Have extra checks here to try to figure out if what it actually is an entity or WorldProbe going wrong.
		if( !pEntity->IsArchetypeSet() )
		{
			return false;
		}
		// BODGE!!!

		// TODO: check explicitly for a car or object? Or just assume anything?
		return true;
	}
	return false;
}

bool	CTaskSlopeScramble::CanSlideOnSurface(const Vector3 &normal, phMaterialMgr::Id matID)
{
	TUNE_GROUP_FLOAT (SLOPE_SCRAMBLE, fSlopeScrambleMinAngleFriction, 0.9f, 0.0f, 1.5f, 0.01f);
	TUNE_GROUP_FLOAT (SLOPE_SCRAMBLE, fSlopeScrambleMaxAngleFriction, 1.2f, 0.0f, 5.0f, 0.01f);
	TUNE_GROUP_FLOAT (SLOPE_SCRAMBLE, fSlopeScrambleMaxAngle, 65.0f, 0.0f, 90.0f, 0.01f);

	float fAngle = normal.Angle(ZAXIS) * RtoD;
	if(fAngle>fSlopeScrambleMaxAngle)
	{
		return true;
	}
	else
	{
		float fFrictionInterp = (fAngle - GetActivationAngle())/(fSlopeScrambleMaxAngle - GetActivationAngle());
		fFrictionInterp = fSlopeScrambleMinAngleFriction + ((fSlopeScrambleMaxAngleFriction - fSlopeScrambleMinAngleFriction) * fFrictionInterp);

		const phMaterial &mat = PGTAMATERIALMGR->GetMaterial(matID);
		float	friction = mat.GetFriction();
		if( friction <= fFrictionInterp )
		{
			return true;
		}
	}
	return false;			// true; (for testing purposes)
}

float CTaskSlopeScramble::GetCheckDistance(const Vector3& groundNormal)
{
	float fAngle = groundNormal.Angle(ZAXIS) * RtoD;
	//Handle extreme slopes > 65
	float fMaxAngle = rage::Max(fAngle,65.0f);

	float fInterp = (fAngle - GetActivationAngle())/(fMaxAngle - GetActivationAngle());

	TUNE_GROUP_FLOAT(SLOPE_SCRAMBLE, fSlopeScrambleCheckDistanceMin, 0.0f, 0.01f, 20.0f, 0.01f);
	TUNE_GROUP_FLOAT(SLOPE_SCRAMBLE, fSlopeScrambleCheckDistanceMax, 3.0f, 0.01f, 20.0f, 0.01f);
	
	
	float output = fInterp*fSlopeScrambleCheckDistanceMin+ (1-fInterp)*fSlopeScrambleCheckDistanceMax;
	return output;
}

bool CTaskSlopeScramble::NoGroundSteepSlopeCheck(CPed *pPed)
{
	TUNE_GROUP_BOOL(SLOPE_SCRAMBLE, bRenderSteepSlopeCheck, false);
	TUNE_GROUP_FLOAT(SLOPE_SCRAMBLE, fSlopeScrambleSteepSlopeExtendedGroundProbe, 1.7f, 0.01f, 20.0f, 0.01f);

	//The ground capsule hasn't detected any collsion but we could still be on a steep slope

	Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	Vector3 vPedExtendedGroundPos = vPedPos - Vector3(0.0f,0.0f,fSlopeScrambleSteepSlopeExtendedGroundProbe);

	WorldProbe::CShapeTestProbeDesc probeDesc;
	WorldProbe::CShapeTestFixedResults<> probeResults;
	probeDesc.SetResultsStructure(&probeResults);
	probeDesc.SetStartAndEnd(vPedPos, vPedExtendedGroundPos);
	probeDesc.SetExcludeEntity(pPed);
	probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_TYPES_MOVER);
	if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
	{
		//Check the normal
		if (Verifyf(probeResults[0].GetHitDetected(), "probe test reports a hit but results aren't valid????"))
		{
			Vector3	vHitNormal = probeResults[0].GetHitPolyNormal();
			if(GetIsTooSteep(vHitNormal))
			{
#if __BANK
				if( bRenderSteepSlopeCheck )
				{
					grcDebugDraw::Line(vPedPos,vPedExtendedGroundPos, Color32(255, 0, 0, 255), Color32(255, 0, 0, 255), 160 );
				}
#endif	//__BANK
				return true;
			}
		}
	}

#if __BANK
	if( bRenderSteepSlopeCheck )
	{
		grcDebugDraw::Line(vPedPos,vPedExtendedGroundPos, Color32(0, 255, 0, 255), Color32(0, 255, 0, 255), 160 );
	}
#endif	//__BANK

	return false;
}

bool	CTaskSlopeScramble::CanDoSlopeScramble(CPed *pPed, bool bActive)
{
	// If the task isn't enabled, we can't slide
	if(!CTaskPlayerOnFoot::ms_bEnableSlopeScramble)
	{
		return false;
	}

	if (pPed->IsNetworkClone())
	{
		return false;
	}

	if ( pPed->IsAPlayerPed() && pPed->GetPedType() == PEDTYPE_ANIMAL )
	{
		return false;
	}

	//Special case material that just forces the stumble behavior
	if (pPed->GetPedResetFlag(CPED_RESET_FLAG_TooSteepForPlayer))
	{
		return true;
	}

	if(!pPed->IsInMotion() && !bActive)
	{
		return false;
	}

	//! DMKH. TO DO. Need to replace standing check with a check to determine if we are touching ground.
	if(!pPed->IsOnGround() && !bActive)
	{
		if(NoGroundSteepSlopeCheck(pPed))
		{
			return true;
		}
		return false;
	}
	else if(!pPed->GetIsStanding() && bActive)
	{
		return true;
	}

#if ENABLE_DRUNK
	if(pPed->IsDrunk())
	{
		return false;
	}
#endif // ENABLE_DRUNK

	// If the ped is stood on an entity, we can't slide.
	if(IsStoodOnEntity(pPed))
	{
		// We're stood on an entity
		return false;
	}

	if (pPed->GetPedResetFlag(CPED_RESET_FLAG_IsVaulting))
	{
		return false;
	}

	CPhysical* pGroundPhysical = pPed->GetGroundPhysical();
	if (pGroundPhysical && (pGroundPhysical->GetIsTypePed() || pGroundPhysical->GetIsTypeVehicle() || pGroundPhysical->GetIsTypeObject()))
	{
		return false;
	}

	if( pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_OnStairs) )
	{
		return false;
	}

	// Get the ground poly normal
	const Vector3 groundNormal = pPed->GetGroundNormal();

	// If we're on a poly that's too shallow, we need to stop regardless of anything else
	if( GetIsTooShallow(groundNormal) )
	{
		return false;
	}
	
	// This should solve the problem with edges triggering slopescramble
	Vector3 groundNormalXY = Vector3(groundNormal.x, groundNormal.y, 0.f);
	groundNormalXY.Normalize();

	float groundDotDir = abs(groundNormalXY.Dot(VEC3V_TO_VECTOR3(pPed->GetTransform().GetForward())));
	const Vector3 SteepNormal = ((groundDotDir > 0.9f) ? pPed->GetSlopeNormal() : groundNormal);

	// If we're not currently sliding but we're on slope steep enough to start sliding, check ahead to see if we can slide the minimum distance
	if( !bActive && GetIsTooSteep(SteepNormal))
	{
		if(CheckSurfaceTypeAndDistance(pPed, SteepNormal, GetCheckDistance(groundNormal)))
		{
			if( !CheckAheadForObjects(pPed, GetCheckDistance(groundNormal)) )
			{
				return true;
			}
		}
	}
	else if( bActive )
	{
		// If we're already sliding, check for objects in the way
		if( !CheckAheadForObjects(pPed, GetCheckDistance(groundNormal)) )
		{
			return true;
		}
	}

	return false;
}

float	CTaskSlopeScramble::GetTimeBeforeScramble(const Vector3 &groundNormal, const Vector3 &moveVelocity, const Vector3 &/*upVec*/)
{
	//// Designed to be used when the ped is moving

	//// Project the velocity into the plane of the surface.
	//Vector3 VInPlane = moveVelocity - upVec * groundNormal.Dot(moveVelocity);
	//VInPlane.Normalize();

	//// Now this doesn't scale linearly , might want to convert the result from the dot to degree and scale that instead
	//// The effect now is that it kinda "kicks" in when running upward or downwards a slope, which is not necessary bad - Traffe
	//float const MaxTime = 1.0f;	// Seconds
	//float Time = (1.f - abs(VInPlane.Dot(upVec))) * MaxTime;


	// This method kicks in harder and does not care how steep the ground is the method above does
	Vector3 moveVelocityXY = Vector3(moveVelocity.x, moveVelocity.y, 0.f);
	moveVelocityXY.Normalize();

	Vector3 groundNormalXY = Vector3(groundNormal.x, groundNormal.y, 0.f);
	groundNormalXY.Normalize();

	float const MaxTime = 0.7f;	// Seconds
	float Time = (1.f - abs(moveVelocityXY.Dot(groundNormalXY))) * MaxTime;

	return Time;
}

// Checks the surface type and checks to see if it continues for at least "distToCheck" distance (down the face)
// Returns FALSE if we can't slide
bool	CTaskSlopeScramble::CheckSurfaceTypeAndDistance(CPed *pPed, const Vector3 &normal, float distToCheck )
{
#if __BANK
	TUNE_GROUP_BOOL(SLOPE_SCRAMBLE, bRenderPreCheckProbes, false);
#endif	//__BANK

	// Get the current surface type from the ped, if it's none "slidey", return false
	if( CanSlideOnSurface(normal, pPed->GetPackedGroundMaterialId()) )
	{
		// See if the surface continues in the direction of the slope for "distToCheck" distance.
		Assert( pPed->IsOnGround() );
		const Vector3 groundNormal = pPed->GetGroundNormal();
		const Vector3 groundPos = pPed->GetGroundPos();

#if 0
		// DEBUG
		{
			// Get the direction of the slope
			Vector3 temp = CrossProduct(groundNormal, ZAXIS);
			Vector3 forward = CrossProduct(groundNormal, temp);

			//	Draw the direction of the slope
			grcDebugDraw::Line(groundPos, groundPos + forward, Color32(255, 0, 0, 255), Color32(255, 0, 0, 255), 60 );

			//	Draw the normal at the ground pos.. and it's components
			grcDebugDraw::Line(groundPos, groundPos + groundNormal, Color32(255, 255, 255, 255), Color32(255, 255, 255, 255), 60 );
			temp = groundPos; temp.x += groundNormal.x;
			grcDebugDraw::Line(groundPos, temp, Color32(255, 0, 0, 255), Color32(255, 0, 0, 255), 60 );
			temp = groundPos; temp.y += groundNormal.y;
			grcDebugDraw::Line(groundPos, temp, Color32(0, 255, 0, 255), Color32(0, 255, 0, 255), 60 );
			temp = groundPos; temp.z += groundNormal.z;
			grcDebugDraw::Line(groundPos, temp, Color32(0, 0, 255, 255), Color32(0, 0, 255, 255), 60 );
		}
		// DEBUG
#endif

		const float PedOffsetToGround = pPed->GetCapsuleInfo()->GetGroundToRootOffset();
		Vector3 pedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		pedPos.z -= PedOffsetToGround * 0.5f;

		// Get the direction of the slope
		Vector3 forward = CrossProduct(groundNormal, CrossProduct(groundNormal, ZAXIS));

		// Make the descent in the direction of the slope be the minimum slope angle
		forward.z = 0;
		forward.Normalize();
		forward.z = -rage::Tanf( DtoR * GetActivationAngle() );
		forward.Normalize();

		// Move down the slope by the distance, from the ground position, to find our test point
		const Vector3 intermediatePos = groundPos + (forward * distToCheck);

#if __BANK
		if( bRenderPreCheckProbes )
		{
			grcDebugDraw::Line(groundPos, groundPos + forward, Color32(255, 255, 0, 255), Color32(255, 255, 0, 255), 60 );
			grcDebugDraw::Line(pedPos, intermediatePos, Color32(255, 0, 0, 255), Color32(255, 0, 0, 255), 60 );
		}
#endif	//__BANK

		// Now shoot a ray from the ped root to the test point.
		WorldProbe::CShapeTestProbeDesc probeDesc;
		WorldProbe::CShapeTestFixedResults<> probeResults;
		probeDesc.SetResultsStructure(&probeResults);
		probeDesc.SetStartAndEnd(pedPos, intermediatePos);
		probeDesc.SetExcludeEntity(pPed);
		probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_TYPES_MOVER);
		if(!WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
		{
			// We didn't hit anything, possible precipice -> Slide
			return true;
		}
		else
		{
			// We've hit something
			if (Verifyf(probeResults[0].GetHitDetected(), "probe test reports a hit but results aren't valid????"))
			{
				float	distSquared = (pedPos - intermediatePos).Mag2();
				Vector3	hitPos = probeResults[0].GetHitPosition();
				float	resDistSquared = (pedPos - hitPos).Mag2();
				if( distSquared <= resDistSquared )
				{
					// The ray didn't hit anything within our test distance
					return true;
				}
				else
				{

#if __BANK
					if( bRenderPreCheckProbes )
					{
						grcDebugDraw::Line(hitPos, hitPos + probeResults[0].GetHitNormal(), Color32(0, 255, 0, 255), Color32(0, 255, 0, 255), 60 );
					}
#endif	//__BANK

					if(!IsEntitySurface(probeResults[0]))
					{

						if( CanSlideOnSurface(normal, probeResults[0].GetHitMaterialId()) )
						{
							Vector3	hitDelta = probeResults[0].GetHitPosition() - groundPos;
							hitDelta.Normalize();

							const float sinSlopeAngle = rage::Sinf( DtoR * -GetProbeTestAngle() );
							if( hitDelta.z < sinSlopeAngle )
							{
								return true;
							}
						}
					}
				}
			}
		}
	}
	return false;
}


// Returns true if we hit something that would stop us sliding
bool	CTaskSlopeScramble::CheckAheadForObjects(CPed *pPed, float distToCheck )
{
#if __BANK
	TUNE_GROUP_BOOL(SLOPE_SCRAMBLE, bRenderActiveProbes, false);
#endif	//__BANK

	// See if the surface continues in the direction of the slope for "distToCheck" distance.
	Assert(pPed->IsOnGround());
	const Vector3 groundNormal = pPed->GetGroundNormal();
	const Vector3 groundPos = pPed->GetGroundPos();
	const Vector3 pedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

	// Get the direction of the slope
	Vector3 forward = CrossProduct(groundNormal, CrossProduct(groundNormal, ZAXIS));

	// Make the descent in the direction of the slope be the minimum slope angle
	forward.z = 0;
	forward.Normalize();
	forward.z = -rage::Tanf( DtoR * GetActivationAngle() );
	forward.Normalize();

	// Move down the slope by the distance, from the ground position, to find our test point
	const Vector3 intermediatePos = groundPos + (forward * distToCheck);

#if __BANK
	if( bRenderActiveProbes )
	{
		grcDebugDraw::Line(groundPos, groundPos + forward, Color32(255, 255, 0, 255), Color32(255, 255, 0, 255), 60 );
		grcDebugDraw::Line(pedPos, intermediatePos, Color32(255, 0, 0, 255), Color32(255, 0, 0, 255), 60 );
	}
#endif	//__BANK

	// Now shoot a ray from the ped root to the test point.
	WorldProbe::CShapeTestProbeDesc probeDesc;
	WorldProbe::CShapeTestFixedResults<> probeResults;
	probeDesc.SetResultsStructure(&probeResults);
	probeDesc.SetStartAndEnd(pedPos, intermediatePos);
	probeDesc.SetExcludeEntity(pPed);
	probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_TYPES_MOVER);
	if(!WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
	{
		// We didn't hit anything, possible precipice -> Slide
		return false;
	}
	else
	{
		// We've hit something
		if (Verifyf(probeResults[0].GetHitDetected(), "probe test reports a hit but results aren't valid????"))
		{
			float	distSquared = (pedPos - intermediatePos).Mag2();
			Vector3	hitPos = probeResults[0].GetHitPosition();
			float	resDistSquared = (pedPos - hitPos).Mag2();
			if( distSquared <= resDistSquared )
			{
				// The ray didn't hit anything within our test distance
				return false;
			}
			else
			{

#if __BANK
				if( bRenderActiveProbes )
				{
					grcDebugDraw::Line(hitPos, hitPos + probeResults[0].GetHitNormal(), Color32(0, 255, 0, 255), Color32(0, 255, 0, 255), 60 );
				}
#endif	//__BANK

				if(!IsEntitySurface(probeResults[0]))
				{
					return false;
				}

			}
		}
	}
	return true;			// Something is in the way
}

//////////////////////////////////////////////////////////////////////////

bool	CTaskSlopeScramble::ShouldContinueSlopeScramble( CPed *pPed )
{
	TUNE_GROUP_FLOAT (SLOPE_SCRAMBLE, fSlopeScrambleShallowTime, 0.1f, 0.01f, 10.0f, 0.01f);
	TUNE_GROUP_FLOAT (SLOPE_SCRAMBLE, fSlopeScrambleMinTime, 0.1f, 0.01f, 10.0f, 0.01f);
	TUNE_GROUP_FLOAT (SLOPE_SCRAMBLE, fSlopeTooShallowAngle, 15.0f, 1.00f, 90.0f, 0.01f);

	// Minimum time timer
	m_SlopeScrambleTimer += fwTimer::GetTimeStep();

	if( CanDoSlopeScramble(pPed, true ))
	{
		m_TooShallowTimer = 0.0f;
		return true;
	}
	else
	{
		// Says we can't continue, so continue until the time runs out....

		// Time on shallow slopes timer
		m_TooShallowTimer += fwTimer::GetTimeStep();

		// (3)a total minimum time for the task to prevent it switching in and out
		if( m_SlopeScrambleTimer <= fSlopeScrambleMinTime )
		{
			return true;
		}

		// (1)If the angle is shallower than the activation slope and has been for more than N seconds, transition back
		if(m_TooShallowTimer <= fSlopeScrambleShallowTime )
		{
			return true;		// Keep sliding until this timer is expired
		}

		Assert(pPed->IsOnGround());
		const Vector3 groundNormal = pPed->GetGroundNormal();

		// (2)If we encounter a very shallow slope it transitions out quicker (<15 degrees)
		const float cosShallowAngle = rage::Cosf( DtoR * fSlopeTooShallowAngle );
		const bool bSurfaceTooShallow = ( abs(groundNormal.z) > cosShallowAngle );
		if( bSurfaceTooShallow )
		{
			if(m_TooShallowTimer <= fSlopeScrambleShallowTime * 0.5f )		// For very low inclines, use half time
			{
				return true;		// Keep sliding until this timer is expired
			}
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////

CTaskSlopeScramble::CTaskSlopeScramble()
: m_TooShallowTimer(0.0f)
, m_SlopeScrambleTimer(0.0f)
, m_fTimeWithNoGround(0.0f)
, m_vGroundNormal(0.0f, 0.0f, 0.0f)
, m_vVelocity(0.f, 0.f, 0.f)
, m_bHasFPSUpperBodyAnim(false)
{
	SetInternalTaskType(CTaskTypes::TASK_ON_FOOT_SLOPE_SCRAMBLE);
}

//////////////////////////////////////////////////////////////////////////

CTaskSlopeScramble::~CTaskSlopeScramble()
{

}


void CTaskSlopeScramble::OnCloneTaskNoLongerRunningOnOwner()
{
	if (GetState() == State_DoSlopeScramble)
		SetStateFromNetwork(State_DoSlopeScrambleOutro);
	else
		SetStateFromNetwork(State_Quit);
}

CTaskInfo* CTaskSlopeScramble::CreateQueriableState() const
{
	Assert(GetState() != -1);

	CClonedSlopeScrambleInfo::InitParams initParams(
		GetState(),
		m_clipSetId
		);

	return rage_new CClonedSlopeScrambleInfo(initParams);
}

void CTaskSlopeScramble::ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo)
{
	Assert( pTaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_SLOPE_SCRAMBLE );
    CClonedSlopeScrambleInfo* info = static_cast<CClonedSlopeScrambleInfo*>(pTaskInfo);

	m_clipSetId = info->GetClipSetId();
	CTaskFSMClone::ReadQueriableState(pTaskInfo);
}

CTask::FSM_Return CTaskSlopeScramble::UpdateClonedFSM(const s32 iState, const FSM_Event iEvent)
{
	if(iEvent == OnUpdate)
	{
		if(GetStateFromNetwork() != -1)
		{
			if(( GetState() != State_Initial ) && ( GetStateFromNetwork() != State_Initial ))
			{
				if(GetStateFromNetwork() != GetState())
				{
					SetState(GetStateFromNetwork());
				}
			}
		}
	}

	FSM_Begin
		FSM_State(State_Initial)
			FSM_OnEnter
			return StateInitial_OnEnter();
		FSM_OnUpdate
			return StateInitial_OnUpdate();

		FSM_State(State_DoSlopeScramble)
			FSM_OnEnter
			return StateDoSlopeScramble_OnEnter(); 
		FSM_OnUpdate
			return StateDoSlopeScramble_OnUpdate();

		FSM_State(State_DoSlopeScrambleOutro)
			FSM_OnEnter
			return StateDoSlopeScrambleOutro_OnEnter(); 
		FSM_OnUpdate
			return StateDoSlopeScrambleOutro_OnUpdate();

		FSM_State(State_Quit)
			FSM_OnEnter
			return FSM_Quit;

	FSM_End
}

CTaskFSMClone* CTaskSlopeScramble::CreateTaskForClonePed(CPed* /*pPed*/)
{
	return rage_new CTaskSlopeScramble();
}

CTaskFSMClone* CTaskSlopeScramble::CreateTaskForLocalPed(CPed* pPed)
{
	return CreateTaskForClonePed(pPed);
}

bool CTaskSlopeScramble::ProcessMoveSignals()
{
#if FPS_MODE_SUPPORTED
	if(m_networkHelper.IsNetworkActive())
	{
		const CPed* pPed = GetPed();
		bool bFirstPersonEnabled = false;
		if(m_bHasFPSUpperBodyAnim && pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
		{
			bFirstPersonEnabled = true;

			float fPitchRatio = 0.5f;
			const CTaskMotionBase* pMotionTask = pPed->GetCurrentMotionTask(false);
			if (pMotionTask && pMotionTask->GetTaskType() == CTaskTypes::TASK_MOTION_AIMING)
			{
				fPitchRatio = static_cast<const CTaskMotionAiming*>(pMotionTask)->GetCurrentPitch();	
			}

			m_networkHelper.SetFloat(ms_PitchId, fPitchRatio);
		}

		m_networkHelper.SetFlag(bFirstPersonEnabled, ms_FirstPersonMode);

	}
#endif // FPS_MODE_SUPPORTED

	return true;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskSlopeScramble::ProcessPreFSM()
{
	CPed *pPed = GetPed();

#if FPS_MODE_SUPPORTED
	if(pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
	{
		pPed->SetPedResetFlag(CPED_RESET_FLAG_IsAiming, true);
		pPed->SetIsStrafing(true);
	}
#endif // FPS_MODE_SUPPORTED

	pPed->SetPedResetFlag(CPED_RESET_FLAG_DisableMotionBaseVelocityOverride, true);

	if(GetState() != State_DoSlopeScrambleOutro )
	{
		// Tell the task to call ProcessPhysics
		pPed->SetPedResetFlag( CPED_RESET_FLAG_ProcessPhysicsTasks, true );
	}

	if( pPed->IsOnGround() )
	{
		m_vGroundNormal = pPed->GetGroundNormal();
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskSlopeScramble::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin
		FSM_State(State_Initial)
			FSM_OnEnter
			return StateInitial_OnEnter();
		FSM_OnUpdate
			return StateInitial_OnUpdate();

		FSM_State(State_DoSlopeScramble)
			FSM_OnEnter
			return StateDoSlopeScramble_OnEnter(); 
		FSM_OnUpdate
			return StateDoSlopeScramble_OnUpdate();

		FSM_State(State_DoSlopeScrambleOutro)
			FSM_OnEnter
			return StateDoSlopeScrambleOutro_OnEnter(); 
		FSM_OnUpdate
			return StateDoSlopeScrambleOutro_OnUpdate();

		FSM_State(State_Quit)
			FSM_OnEnter
			return FSM_Quit;

	FSM_End
}


//////////////////////////////////////////////////////////////////////////

void	CTaskSlopeScramble::CleanUp()
{
	if (m_networkHelper.GetNetworkPlayer())
	{
		GetPed()->GetMovePed().ClearTaskNetwork(m_networkHelper, GetBlendOutDuration());
		m_networkHelper.ReleaseNetworkPlayer();
	}

#if FPS_MODE_SUPPORTED
	GetPed()->SetPedConfigFlag(CPED_CONFIG_FLAG_ForceFPSIKWithUpperBodyAnim, false);
#endif // FPS_MODE_SUPPORTED

	// Base class
	CTask::CleanUp();
}

//////////////////////////////////////////////////////////////////////////

bool	CTaskSlopeScramble::ProcessPhysics(float fTimeStep, int UNUSED_PARAM(nTimeSlice))
{
	CPed *pPed = GetPed();

	// Use the animated velocity to linearly interpolate the heading.
	float fDesiredHeadingChange =  SubtractAngleShorter(
		fwAngle::LimitRadianAngleSafe(pPed->GetMotionData()->GetDesiredHeading()), 
		fwAngle::LimitRadianAngleSafe(pPed->GetMotionData()->GetCurrentHeading())
		);

	if (!IsNearZero(fDesiredHeadingChange))
	{
		TUNE_GROUP_FLOAT (PED_NAVIGATION, fSlopeScrambleTurnRate, 1.0f, 0.0f, 100.0f, 0.001f);

		float fRotateSpeed;
		// try not to overshoot
		if ((fSlopeScrambleTurnRate*fTimeStep) > abs(fDesiredHeadingChange))
		{
			fRotateSpeed = abs(fDesiredHeadingChange) / fTimeStep;
		}
		else
		{
			fRotateSpeed = fSlopeScrambleTurnRate;
		}
		if (fDesiredHeadingChange<0.0f)
		{
			fRotateSpeed *= -1.0f;
		}
		pPed->SetDesiredAngularVelocity(Vector3(0.0f, 0.0f, fRotateSpeed));
	}

	if(GetState() == State_DoSlopeScramble)
	{
		// set the desired velocity
		pPed->SetDesiredVelocityClamped(m_vVelocity,CTaskMotionBase::ms_fAccelLimitHigher * fTimeStep);
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return	CTaskSlopeScramble::StateInitial_OnEnter()
{	
	// Get the motion task data set to get our clipset for this pedtype
	CPedModelInfo *pModelInfo = GetPed()->GetPedModelInfo();
	const CMotionTaskDataSet* pMotionTaskDataSet = CMotionTaskDataManager::GetDataSet(pModelInfo->GetMotionTaskDataSetHash().GetHash());
	m_clipSetId	= fwMvClipSetId(pMotionTaskDataSet->GetOnFootData()->m_SlopeScrambleClipSetHash.GetHash());	//CLIP_SET_SLOPE_SCAMBLE

	// request the move network
	m_networkHelper.RequestNetworkDef(CClipNetworkMoveInfo::ms_NetworkTaskOnFootSlopeScramble);
	return FSM_Continue;
}

CTask::FSM_Return	CTaskSlopeScramble::StateInitial_OnUpdate()
{
	fwClipSet *clipSet = fwClipSetManager::GetClipSet(m_clipSetId);
	if (!taskVerifyf(clipSet, "Failed to find clipset %s", m_clipSetId.TryGetCStr()))
	{
		return FSM_Quit;
	}

	if(GetPed()->GetIsSwimming())
		return FSM_Quit;

	if ( clipSet && clipSet->Request_DEPRECATED() && m_networkHelper.IsNetworkDefStreamedIn(CClipNetworkMoveInfo::ms_NetworkTaskOnFootSlopeScramble))
	{	
		// Create the network player
		m_networkHelper.CreateNetworkPlayer(GetPed(), CClipNetworkMoveInfo::ms_NetworkTaskOnFootSlopeScramble);
		m_networkHelper.SetClipSet(m_clipSetId);

		const CWeapon* pWeapon = GetPed()->GetWeaponManager()->GetEquippedWeapon();
		const CWeaponInfo* pWeaponInfo = pWeapon ? pWeapon->GetWeaponInfo() : NULL;

		if(pWeaponInfo)
		{
			const fwMvClipSetId appropriateWeaponClipSetId = pWeaponInfo->GetAppropriateWeaponClipSetId(GetPed());

#if FPS_MODE_SUPPORTED
			if(fwAnimManager::GetClipIfExistsBySetId(appropriateWeaponClipSetId, ms_WeaponBlockedClipId))
			{
				m_bHasFPSUpperBodyAnim = true;
				m_networkHelper.SetClipSet(appropriateWeaponClipSetId, ms_UpperBodyClipSetId);
				GetPed()->SetPedConfigFlag(CPED_CONFIG_FLAG_ForceFPSIKWithUpperBodyAnim, true);
				if(GetPed()->IsFirstPersonShooterModeEnabledForPlayer(false))
				{
					m_networkHelper.SetFlag(true, ms_FirstPersonMode);
				}
			}
#endif // FPS_MODE_SUPPORTED

			// B*2320671: Set weapon grip clip (if it exists) for melee fist weapons in 1st/3rd person, and other weapons in 3rd person (1st person animations don't need grips)..
			fwMvClipId gripClipId = CLIP_ID_INVALID;
			crFrameFilter *pFilter = NULL;
			if (pWeaponInfo->GetIsMeleeFist())
			{
				fwMvClipId gripIdleClipId(ATSTRINGHASH("GRIP_IDLE", 0x3ec63b58));
				gripClipId = gripIdleClipId;
				pFilter = CTaskMelee::GetMeleeGripFilter(appropriateWeaponClipSetId);
			}
#if FPS_MODE_SUPPORTED
			else if (!GetPed()->IsFirstPersonShooterModeEnabledForPlayer(false))
			{
				fwMvClipId gripIdleClipId(ATSTRINGHASH("IDLE", 0x71c21326));
				gripClipId = gripIdleClipId;
				fwMvFilterId filter(ATSTRINGHASH("Grip_R_Only_NO_IK_filter", 0xfdbff2aa));
				pFilter = g_FrameFilterDictionaryStore.FindFrameFilter(filter);
			}
#endif	// FPS_MODE_SUPPORTED

			if(fwAnimManager::GetClipIfExistsBySetId(appropriateWeaponClipSetId, gripClipId))
			{
				const crClip* pGripClip = fwClipSetManager::GetClip(appropriateWeaponClipSetId, gripClipId);
				if (pGripClip)
				{
					m_networkHelper.SetClipSet(appropriateWeaponClipSetId, ms_UpperBodyClipSetId);
					m_networkHelper.SetClip(pGripClip, ms_GripClipId);
					m_networkHelper.SetFlag(true, ms_HasGrip);

					if (pFilter)
					{
						m_networkHelper.SetFilter(pFilter, CTaskHumanLocomotion::ms_MeleeGripFilterId);
					}
				}
			}
		}

		// Attach it to an empty precedence slot in fwMove
		Assert(GetPed()->GetAnimDirector());
		CMovePed& move = GetPed()->GetMovePed();
		move.SetTaskNetwork(m_networkHelper.GetNetworkPlayer(), GetBlendInDuration());

		// Move into the enter duck state
		SetState(State_DoSlopeScramble);

		// Reset swapped direction flag
		m_SwappedDirection = false;
	}
	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return	CTaskSlopeScramble::StateDoSlopeScramble_OnEnter()
{
	CPed *pPed = GetPed();

	m_scrambleDirection = GetFacingDirClipType(pPed, true);

	if(m_scrambleDirection == SCRAMBLE_FORWARD)
	{
		// Must be forward
		m_networkHelper.SendRequest(ms_GoToScrambleForwardId);
		// Set to wait for Enter into forward
		m_networkHelper.WaitForTargetState(ms_OnChosenForwardId);
	}
	else
	{
		// Must be backward
		m_networkHelper.SendRequest(ms_GoToScrambleBackwardId);
		// Set to wait for Enter into backward
		m_networkHelper.WaitForTargetState(ms_OnChosenBackwardId);
	}

	m_vVelocity = pPed->GetVelocity();
	Assert(fabs(m_vVelocity.Mag2()) < 100000.0f);

	//! DMKH. Make it slightly easier to ragdoll if we lose ground immediately.
	TUNE_GROUP_FLOAT (SLOPE_SCRAMBLE, fTimeWithNoGroundQuitInit, 0.5f, 0.01f, 20.0f, 0.01f);
	m_fTimeWithNoGround = fTimeWithNoGroundQuitInit;

	return FSM_Continue;
}


CTask::FSM_Return	CTaskSlopeScramble::StateDoSlopeScramble_OnUpdate()
{
	TUNE_GROUP_FLOAT (SLOPE_SCRAMBLE, fSlopeScrambleFriction, 5.0f, 0.000f, 1000.0f, 0.0001f);
	TUNE_GROUP_FLOAT (SLOPE_SCRAMBLE, fSlopePedMass, 75.0f, 1.0f, 200.0f, 0.1f);
	TUNE_GROUP_FLOAT (SLOPE_SCRAMBLE, fSlopeScrambleRagdollTime, 1.25f, 0.01f, 20.0f, 0.01f);
	TUNE_GROUP_FLOAT (SLOPE_SCRAMBLE, fTimeWithNoGroundQuit, 1.0f, 0.01f, 20.0f, 0.01f);
	TUNE_GROUP_FLOAT (SLOPE_SCRAMBLE, fShallowTimerSwapDirection, 0.2f, 0.01f, 20.0f, 0.01f);

	// Await move into whichever direction we want
	if (!m_networkHelper.IsInTargetState())
		return FSM_Continue;

	CPed *pPed = GetPed();

	// If we lost the ped or the ped is in the air, quit
	if(pPed==NULL)
	{
		return FSM_Quit;
	}

#if LAZY_RAGDOLL_BOUNDS_UPDATE
	// Request to update the bounds in case a ragdoll activation occurs
	pPed->RequestRagdollBoundsUpdate();
#endif

	// Quit scramble if we hit the water
	if(pPed->GetIsSwimming())
	{
		return FSM_Quit;
	}

	// Deal with the ped no longer standing on ground
	if( pPed->IsOnGround() )
	{
		m_fTimeWithNoGround = 0.0f;
	}
	// Quit if the ground has been missing for a while
	else if( m_fTimeWithNoGround > fTimeWithNoGroundQuit )
	{
		if(NoGroundSteepSlopeCheck(pPed) && CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_FALL))
		{
			CTask* pTaskNM = rage_new CTaskNMHighFall(1000, NULL, CTaskNMHighFall::HIGHFALL_SLOPE_SLIDE);
			CEventSwitch2NM event(10000, pTaskNM);
			pTaskNM = NULL;
			pPed->GetPedIntelligence()->AddEvent(event);
		}
		return FSM_Quit;
	}
	else
	{
		m_fTimeWithNoGround += fwTimer::GetTimeStep();
	}

	//or we're no longer on a slope - ask for outro
	if(!pPed->IsNetworkClone() && ShouldContinueSlopeScramble(pPed) == false )
	{
		if(!m_SwappedDirection)
		{
			SetState(State_DoSlopeScrambleOutro);
			return FSM_Continue;
		}
		else if(m_TooShallowTimer > fShallowTimerSwapDirection)
		{
			return FSM_Quit;
		}
	}

	// We're still going
	// If we've been going too long, ragdoll
	if( m_SlopeScrambleTimer >= fSlopeScrambleRagdollTime )
	{
		if(CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_FALL))
		{
			CTask* pTaskNM = rage_new CTaskNMHighFall(1000, NULL, CTaskNMHighFall::HIGHFALL_SLOPE_SLIDE);
			CEventSwitch2NM event(10000, pTaskNM);
			pTaskNM = NULL;
			pPed->SwitchToRagdoll(event);
		}
		else
		{
			return FSM_Quit;
		}
	}

	// Re-orient to nearest clip type, don't allow oscillation.
	SCRAMBLE_DIRECTION thisDir = GetFacingDirClipType(pPed);
	if(thisDir != m_scrambleDirection && GetTimeInState() > 0.2f)
	{
		m_SwappedDirection = true;					// Don't allow the use of the outro if we have swapped direction, this could put it back up the slope.
		SetFlag(aiTaskFlags::RestartCurrentState);	// Kick it off again to change direction.
	}

	const Vector3 groundNormal = m_vGroundNormal;

	m_vVelocity = pPed->GetVelocity();
	Assert(fabs(m_vVelocity.Mag2()) < 100000.0f);

	m_vVelocity = DoSliding( groundNormal, m_vVelocity, fSlopePedMass, fSlopeScrambleFriction, -9.81f, ZAXIS, fwTimer::GetTimeStep() );

	// Calculate the direction we want to locomote in and set the ped direction accordingly (taking into account the direction we're scrambling in)
	// SetDesiredHeading takes a target position, not direction vector
	const Vector3 playerPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	Vector3 target;
	if(m_scrambleDirection == SCRAMBLE_FORWARD)		
	{
		target = playerPos + m_vVelocity;
	}
	else
	{
		target = playerPos - m_vVelocity;
	}
	pPed->SetDesiredHeading( target );

	TUNE_GROUP_FLOAT(SLOPE_SCRAMBLE, InitialTime, 0.25f, 0.0f, 1.0f, 0.1f);
	if (m_SlopeScrambleTimer < InitialTime)
	{
		// In the beginning of the scramble we want an extra push to prevent ending up on top of the slope
		// If we are moving against the slope that is

		//
		Vector3 GroundDir = groundNormal;
		GroundDir.z = 0.f;
		GroundDir.Normalize();
		GroundDir = GroundDir - ZAXIS * groundNormal.Dot(GroundDir);

		if (GroundDir.Dot(pPed->GetVelocity()) < 0.f )
		{
			TUNE_GROUP_FLOAT(SLOPE_SCRAMBLE, InitialStrength, 10.0f, 0.0f, 100.0f, 1.0f);
			m_vVelocity += GroundDir * InitialStrength * fwTimer::GetTimeStep();
		}
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return	CTaskSlopeScramble::StateDoSlopeScrambleOutro_OnEnter()
{
	// Request_DEPRECATED the outro
	m_networkHelper.SendRequest(ms_GoToExitId);

	return FSM_Continue;
}


CTask::FSM_Return	CTaskSlopeScramble::StateDoSlopeScrambleOutro_OnUpdate()
{
	// Await the end of the anim
	if( m_networkHelper.GetBoolean(ms_OnOutroComplete) == true || GetHasInterruptedClip(GetPed()) || GetPed()->GetIsSwimming() )
	{
		return FSM_Quit;
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTaskSlopeScramble::SCRAMBLE_DIRECTION	CTaskSlopeScramble::GetFacingDirClipType(CPed *pPed, const bool bExcludeDeadZone)
{
	// Choose which direction we want to scramble in
	const Vector3 groundNormal = m_vGroundNormal;
	Vector3	dir = VEC3V_TO_VECTOR3(pPed->GetTransform().GetForward());
	float	dot = DotProduct(dir, groundNormal);
	
	//If the slope is flat within a dead zone then return the existing direction
	TUNE_GROUP_FLOAT(SLOPE_SCRAMBLE, fDirectionDeadZone, 0.1f, -1.0f, 1.0f, 0.01f);
	if( dot > -fDirectionDeadZone && dot < fDirectionDeadZone && !bExcludeDeadZone)
	{
		return m_scrambleDirection;
	}
	else if( dot >= 0.0f )
	{
		return SCRAMBLE_FORWARD;
	}
	else
	{
		return SCRAMBLE_BACKWARD;
	}
}

//////////////////////////////////////////////////////////////////////////

Vector3 CTaskSlopeScramble::DoSliding( const Vector3 &a_UnitNormal, Vector3 &a_InV, float a_Mass, float a_FrictionK, float a_Gravity, const Vector3 &a_UnitUpVector, float a_tDelta )
{
#define  EPSILON 0.001f
	// We assume a_UnitUpVector to be all zeros except one component.

	// Check for a vertical surface.
	if( fabs( a_UnitUpVector.Dot( a_UnitNormal ) ) < EPSILON )
	{
		return a_InV;
	}

	// Project the velocity into the plane of the surface.
	Vector3 VInPlane = a_InV - a_UnitUpVector*a_UnitNormal.Dot(a_InV);

	// -v*k*|v|^2.
	Vector3 ForceFromFriction = VInPlane * (-1.0f*a_FrictionK*VInPlane.Dot( VInPlane ));

	// Formulate the force from gravity (project gravity onto the plane).
	Vector3 G = a_UnitUpVector*a_Gravity;
	Vector3 ForceFromGravity = (G - a_UnitNormal*a_UnitNormal.Dot( G )) * a_Mass;

	// Combine the forces and convert the rate of change in momentum into one of velocity.    
	Vector3 Ret = VInPlane + ( (ForceFromGravity + ForceFromFriction)*(1.0f/a_Mass) ) * a_tDelta;

	// Seems like you could have a strong velocity downward and getting additional force down from gravity could push
	// the ped over the assert limit I guess, so we just make sure that we don't return too high values from here
	Ret.ClampMag(0.f, 300.f);

	return Ret;
}

//////////////////////////////////////////////////////////////////////////

bool CTaskSlopeScramble::GetHasInterruptedClip(const CPed* pPed) const
{
	if(m_networkHelper.GetBoolean( ms_InterruptibleId ))
	{
		if (pPed->IsNetworkClone())
			return true;

		const CControl* pControl = pPed->GetControlFromPlayer();

		// Interrupt if we're pressing 'X' or any movement
		if(pControl && (pControl->GetPedWalkLeftRight().GetNorm() != 0.0f || 
						pControl->GetPedWalkUpDown().GetNorm() != 0.0f ||
						pControl->GetPedSprintIsPressed() ) )
		{
			return true;
		}

	}
	return false;
}




//////////////////////////////////////////////////////////////////////////

#if !__FINAL

const char * CTaskSlopeScramble::GetStaticStateName( s32 iState )
{
	switch(iState)
	{
	case State_Initial: return "Initial";
	case State_DoSlopeScramble: return "DoSlopeScramble";
	case State_DoSlopeScrambleOutro: return "DoSlopeScrambleOutro";
	case State_Quit: return "Quit";
	default: { taskAssert(false); return "Unknown"; }
	}
}

void CTaskSlopeScramble::Debug() const
{

}


#endif //!__FINAL

//////////////////////////////////////////////////////////////////////////
