// Title	:	Targetting.cpp
// Author	:	Phil Hooker
// Started	:	10/08/05

// Framework headers
#include "grcore/debugdraw.h"
#include "fwmaths/Angle.h"

// Game headers
#include "Debug/DebugScene.h"
#include "Event/Events.h"
#include "Network/Objects/Entities/NetObjPlayer.h"
#include "PedGroup/PedGroup.h"
#include "Peds/Ped.h"
#include "Peds/PedEventScanner.h"
#include "Peds/PedGeometryAnalyser.h"
#include "Peds/PedIntelligence.h"
#include "Peds/Ped.h"
#include "Peds/PedWeapons/PedTargeting.h"
#include "Peds/HealthConfig.h"
#include "Physics/GtaInst.h"
#include "Physics/Physics.h"
#include "physics/WorldProbe/worldprobe.h"
#include "Scene/Entity.h"
#include "scene/world/GameWorld.h"
#include "task/Combat/Cover/Cover.h"
#include "Task/Vehicle/TaskCar.h"
#include "Task/Combat/TaskCombat.h"
#include "Task/Combat/TaskNewCombat.h"
#include "vehicles/vehicle.h"
#include "weapons/Weapon.h"

AI_OPTIMISATIONS()

FW_INSTANTIATE_BASECLASS_POOL_SPILLOVER(CPedTargetting, MAX_NO_TARGETTING_SYSTEMS, 0.23f, atHashString("Targetting",0x652a20af), sizeof(CPedTargetting));

const static int TARGET_FADE_TIME_MS				= 10000;
const static float TARGET_FORCED_REMOVAL_FADE_PERC	= 0.8f;

//-------------------------------------------------------------------------
// Constructor
//-------------------------------------------------------------------------
CSingleTargetInfo::CSingleTargetInfo()
{
	SetLastPositionSeen(Vector3(Vector3::ZeroType));
	SetDistance(0.0f);

	m_pEntity						= NULL;
	m_pBlockedByFriendlyPed			= NULL;
	m_fTotalScore					= 0.0f;
	m_iTimeExpiringMs				= 0;
	m_losStatus						= Los_unchecked;
	m_bLosBlockedNear				= false;
	m_bNeverScored					= true;
	m_bLastSeenInVehicle			= false;
	m_bLosWasDoneAsync				= false;
	m_bIsBeingTargeted				= false;
	m_bHasEverHadClearLos			= false;
	m_iCalcHeadingAndVehicleCountdown = 0;
	m_iLastSeenExpirationTimeMs		= 0;
	m_fLastDirectionSeenMovingIn	= 0.0f;
	m_fVisibleTimer					= 0.0f;
	m_fBlockedTimer					= 0.0f;
	m_iTimeLastHostileActionMs		= 0;
}

//-------------------------------------------------------------------------
// Destructor
//-------------------------------------------------------------------------
CSingleTargetInfo::~CSingleTargetInfo()
{
}

//-------------------------------------------------------------------------
// Reset the target information
//-------------------------------------------------------------------------
void CSingleTargetInfo::Revalidate( void )
{
	Assert( m_pEntity );
	m_iTimeExpiringMs = fwTimer::GetTimeInMilliseconds() + TARGET_FADE_TIME_MS;
}

#define MIN_MOVE_SPEED (0.0025f)

//-------------------------------------------------------------------------
// Calculate the current heading direction of the target
//-------------------------------------------------------------------------
void CSingleTargetInfo::CalculateHeading( void )
{
	// Set m_iCalcHeadingAndVehicleCountdown so we call this function again
	// after waiting for kCalcHeadingAndVehiclePeriodFrames - 1 frames.
	// Note: sharing the countdown between CalculateHeading() and CalculateInVehicle() basically
	// assumes that they are called together, which currently always seems to be the case.
	m_iCalcHeadingAndVehicleCountdown = kCalcHeadingAndVehiclePeriodFrames - 1;

	// IF the entity is physical work out its heading from the current velocity.
	if( m_pEntity->GetIsPhysical() )
	{
		ScalarV scMinMoveSpeedSq = ScalarVFromF32(MIN_MOVE_SPEED);

		const CPhysical* pPhysical = static_cast<const CPhysical*>(m_pEntity.Get());
		Vec3V vVelocity = RCC_VEC3V(pPhysical->GetVelocity());
		ScalarV scMagSq = MagSquared(vVelocity);
		if( IsGreaterThanAll(scMagSq, scMinMoveSpeedSq) )
		{
			StoreScalar32FromScalarV(m_fLastDirectionSeenMovingIn, Arctan2(-vVelocity.GetX(), vVelocity.GetY()));
			return;
		}
	}
	// If not physical, or if the velocity isn't high enough, work ou the direction
	// they were likely moving in from the heading
	m_fLastDirectionSeenMovingIn = m_pEntity->GetTransform().GetHeading();
}

void CSingleTargetInfo::CalculateInVehicle( void )
{
	//Check if the entity is a ped.
	if(m_pEntity->GetIsTypePed())
	{
		//Grab the ped.
		const CPed* pPed = static_cast<const CPed *>(m_pEntity.Get());
		
		//Set the flag.
		m_bLastSeenInVehicle = pPed->GetIsInVehicle();
	}
	else
	{
		//Clear the flag.
		m_bLastSeenInVehicle = false;
	}
}

bool CSingleTargetInfo::AreWhereaboutsKnown() const
{
	//Check if we have been seen recently.
	if(m_iLastSeenExpirationTimeMs > fwTimer::GetTimeInMilliseconds())
	{
#if !__NO_OUTPUT
		wantedDebugf3("CSingleTargetInfo::AreWhereaboutsKnown m_iLastSeenExpirationTimeMs[%u] > GetTimeInMilliseconds[%u] --> return true",m_iLastSeenExpirationTimeMs,fwTimer::GetTimeInMilliseconds());
#endif
		return true;
	}

	//Check if the entity is valid.
	const CEntity* pEntity = m_pEntity;
	if(pEntity)
	{
		//Check if the entity hasn't deviated much from the position where we last saw them.
		Vec3V vEntityPosition	= pEntity->GetTransform().GetPosition();
		Vec3V vLastPositionSeen	= VECTOR3_TO_VEC3V(GetLastPositionSeen());
		
#if !__NO_OUTPUT
		ScalarV scDist = Dist(vEntityPosition, vLastPositionSeen);
		float fDist = scDist.Getf();
#endif
	
		ScalarV scDistSq = DistSquared(vEntityPosition, vLastPositionSeen);
		ScalarV scMaxDistSq = ScalarVFromF32(square(10.0f));
		if(IsLessThanAll(scDistSq, scMaxDistSq))
		{
#if !__NO_OUTPUT
			wantedDebugf3("CSingleTargetInfo::AreWhereaboutsKnown fDist[%f] --> return true",fDist);
#endif
			return true;
		}
	}

	return false;
}

unsigned int CSingleTargetInfo::GetTimeUntilLastSeenTimerExpiresMs() const
{
	const u32 currentTimeMs = fwTimer::GetTimeInMilliseconds();
	if(m_iLastSeenExpirationTimeMs > currentTimeMs)
	{
		return m_iLastSeenExpirationTimeMs - currentTimeMs;
	}
	return 0;
}

//-------------------------------------------------------------------------
// Reset the target information
//-------------------------------------------------------------------------
void CSingleTargetInfo::Reset( void )
{
	SetLastPositionSeen(Vector3(Vector3::ZeroType));
	SetDistance(0.0f);
	
	m_pEntity						= NULL;
	m_pBlockedByFriendlyPed			= NULL;
	m_fTotalScore					= 0.0f;
	m_iTimeExpiringMs				= 0;
	m_iLastSeenExpirationTimeMs		= 0;
	m_losStatus						= Los_unchecked;
	m_bLosBlockedNear				= false;
	m_bNeverScored					= true;
	m_bLastSeenInVehicle			= false;
	m_bLosWasDoneAsync				= false;
	m_bIsBeingTargeted				= false;
	m_bHasEverHadClearLos			= false;
	m_iCalcHeadingAndVehicleCountdown = 0;
	m_fLastDirectionSeenMovingIn	= 0.0f;
	m_fVisibleTimer					= 0.0f;
	m_fBlockedTimer					= 0.0f;
	m_iTimeLastHostileActionMs		= 0;
}


//-------------------------------------------------------------------------
// Sets the target
//-------------------------------------------------------------------------
void CSingleTargetInfo::SetTarget( const CEntity* pEntity )
{
	wantedDebugf3("CSingleTargetInfo::SetTarget -- SetLastPositionSeen / set m_iLastSeenExpirationTimeMs");

	Assertf( !m_pEntity, "Overwriting an already valid target" );

	m_pEntity					= pEntity;
	m_iTimeExpiringMs			= fwTimer::GetTimeInMilliseconds() + TARGET_FADE_TIME_MS;
	m_iLastSeenExpirationTimeMs	= fwTimer::GetTimeInMilliseconds() + CPedTargetting::GetTunables().m_iTargetNotSeenIgnoreTimeMs;
	
	SetLastPositionSeen(VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition()));
	
	CalculateHeading();
	
	CalculateInVehicle();
}

//-------------------------------------------------------------------------
// Standalone targetting class, base class to be inherited from for
// specific uses
//-------------------------------------------------------------------------

#if !__FINAL
bool  CPedTargetting::DebugTargetting = false;
bool  CPedTargetting::DebugTargettingLos = false;
bool  CPedTargetting::DebugAcquaintanceScanner = false;
#endif

IMPLEMENT_COMBAT_TASK_TUNABLES(CPedTargetting, 0xea30d255);
CPedTargetting::Tunables CPedTargetting::ms_Tunables;

//-------------------------------------------------------------------------
// Gets the target at a given index
//-------------------------------------------------------------------------
const CEntity* CPedTargetting::GetTargetAtIndex( int iTargetIndex )
{
	const CSingleTargetInfo* pTargetInfo = GetTargetInfoAtIndex(iTargetIndex);
	if (!pTargetInfo)
	{
		return NULL;
	}

#if LAZY_RAGDOLL_BOUNDS_UPDATE
	// If targeting a human, force it's bounds to update
	if (pTargetInfo->m_pEntity && pTargetInfo->m_pEntity->GetIsTypePed())
	{
		CPed *pTargetPed = (CPed*)pTargetInfo->m_pEntity.Get();
		if (pTargetPed && pTargetPed->GetRagdollInst())
		{
			pTargetPed->RequestRagdollBoundsUpdate();
		}
	}
#endif

	return pTargetInfo->m_pEntity;
}


//-------------------------------------------------------------------------
// Gets the target info at a given index
//-------------------------------------------------------------------------
const CSingleTargetInfo* CPedTargetting::GetTargetInfoAtIndex( int iTargetIndex ) const
{
	// Has a target been set, return NULL if not
	if(iTargetIndex < 0)
	{
		return NULL;
	}

	// If this function gets called with an index outside of the current number of
	// active targets, there is probably a bug somewhere - that should be avoidable.
	Assert(iTargetIndex < m_iNumActiveTargets);
	Assert(iTargetIndex < MAX_NUM_TARGETS);

	// Is the selected target still valid
	if( !m_aTargets[iTargetIndex].IsValid() )
	{
		return NULL;
	}

	return &m_aTargets[iTargetIndex];
}

//-------------------------------------------------------------------------
// returns a pointer to the currently targeted entity
//-------------------------------------------------------------------------
const CEntity* CPedTargetting::GetCurrentTarget( void )
{
	return GetTargetAtIndex(m_iCurrentTargetIndex);
}


//-------------------------------------------------------------------------
// returns a pointer to the second best target entity
//-------------------------------------------------------------------------
const CEntity* CPedTargetting::GetSecondaryTarget( void )
{
	return GetTargetAtIndex(m_iSecondBestTargetIndex);
}


//-------------------------------------------------------------------------
// returns the LOS status of the given entity if it is in the list
//-------------------------------------------------------------------------
LosStatus CPedTargetting::GetLosStatus( const CEntity* pEntity, bool bForceImmediateUpdate )
{
	return GetLosStatusAtIndex(FindTargetIndex( pEntity ), bForceImmediateUpdate);
}

//-------------------------------------------------------------------------
// returns the LOS status of the given entity if it is in the list
//-------------------------------------------------------------------------
LosStatus CPedTargetting::GetLosStatusAtIndex( int iIndex, bool bForceImmediateUpdate )
{
	if( iIndex != -1 )
	{
		CSingleTargetInfo* pTargetInfo = &m_aTargets[iIndex];
		Assert( pTargetInfo->IsValid() );

		// If currently undefined, update the Los status
		if( bForceImmediateUpdate && (pTargetInfo->GetLosStatus() == Los_unchecked) )
		{
			UpdateSingleLos( iIndex, true );
			Assert(pTargetInfo->GetLosStatus() != Los_unchecked );
		}

		return pTargetInfo->GetLosStatus();
	}
	return Los_unchecked;
}


//-------------------------------------------------------------------------
// Gets the position of the entity.
// Returns true if successful
//-------------------------------------------------------------------------
bool CPedTargetting::GetTargetLastSeenPos( const CEntity* pEntity, Vector3& vOutPos )
{
	CSingleTargetInfo* pTargetInfo = FindTarget( pEntity );
	if( pTargetInfo )
	{
		vOutPos = pTargetInfo->GetLastPositionSeen();
		return true;
	}
	return false;
}

//-------------------------------------------------------------------------
// Gets the last known heading of the target
// Returns true if successful
//-------------------------------------------------------------------------
bool CPedTargetting::GetTargetDirectionLastSeenMovingIn( const CEntity* pEntity, float& fOutDirection )
{
	CSingleTargetInfo* pTargetInfo = FindTarget( pEntity );
	if( pTargetInfo )
	{
		fOutDirection = pTargetInfo->m_fLastDirectionSeenMovingIn;
		return true;
	}
	return false;
}

bool CPedTargetting::GetTargetLastSeenInVehicle( const CEntity* pEntity, bool& bOutInVehicle )
{
	CSingleTargetInfo* pTargetInfo = FindTarget( pEntity );
	if( pTargetInfo )
	{
		bOutInVehicle = pTargetInfo->GetLastSeenInVehicle();
		return true;
	}
	return false;
}

//-------------------------------------------------------------------------
// returns whether the target has been out of sight for enough time
// so that the ped no longer knows it's position
//-------------------------------------------------------------------------
bool CPedTargetting::AreTargetsWhereaboutsKnown( Vector3* pvBestPos, const CEntity* pTarget, float* pfLastSeenTimer, bool* pbHasTargetInfo )
{
	CSingleTargetInfo* pTargetInfo = FindTarget( pTarget ? pTarget : GetCurrentTarget() );
	if( pTargetInfo )
	{
		if( pbHasTargetInfo )
		{
			*pbHasTargetInfo = true;
		}

		if( pvBestPos )
		{
			*pvBestPos = pTargetInfo->GetLastPositionSeen();
		}		
		
		if ( pfLastSeenTimer )
		{
			*pfLastSeenTimer = pTargetInfo->GetLastSeenTimer();
		}
		
		return ( pTargetInfo->AreWhereaboutsKnown() );
	}
	else if( pbHasTargetInfo )
	{
		*pbHasTargetInfo = false;
	}

	return true;
}

dev_float TIME_TO_ZERO_ON_TARGET = (3.0f);

//-------------------------------------------------------------------------
// Gets the distance to the given entity if it is in the list,
// returns true if successful
//-------------------------------------------------------------------------
bool CPedTargetting::GetDistance( const CEntity* pEntity, float &fDistance )
{
	CSingleTargetInfo* pTargetInfo = FindTarget( pEntity );
	if( pTargetInfo )
	{
		fDistance = pTargetInfo->GetDistance();

		// Distance is 0 when uninitialised
		return fDistance != 0.0f;
	}
	return false;
}


//-------------------------------------------------------------------------
// Sets the targeting in use and wakes it up if needed
//-------------------------------------------------------------------------
void CPedTargetting::SetInUseAndWakeUp( void )
{
	m_fActivityTimer = ms_Tunables.m_fTargetingInactiveDisableTime;

	// If the targeting is currently inactive, wake it up
	if( !m_bActive )
	{
		m_bActive = true;

		// Carry out a single update
		UpdateTargetting( false );

		// By default the first update sets the los status of all targets to clear
		SetAllLosStatus( Los_unchecked );
	}
}

//-------------------------------------------------------------------------
// Sets the LOS status of all currently valid targets
//-------------------------------------------------------------------------
void CPedTargetting::SetAllLosStatus( LosStatus losStatus )
{
	const int numTargets = m_iNumActiveTargets;
	for(int i = 0; i < numTargets; i++)
	{
		if( m_aTargets[i].IsValid() )
		{
			m_aTargets[i].m_losStatus = losStatus;
			m_aTargets[i].m_bLosBlockedNear = false;
			if( losStatus == Los_clear )
			{
				wantedDebugf3("CPedTargetting::SetAllLosStatus clear -- SetLastPositionSeen / set m_iLastSeenExpirationTimeMs");
				m_aTargets[i].m_iLastSeenExpirationTimeMs = fwTimer::GetTimeInMilliseconds() + ms_Tunables.m_iTargetNotSeenIgnoreTimeMs;
				m_aTargets[i].SetLastPositionSeen(VEC3V_TO_VECTOR3(m_aTargets[i].m_pEntity->GetTransform().GetPosition()));
				m_aTargets[i].CalculateHeading();
				m_aTargets[i].CalculateInVehicle();
				m_aTargets[i].m_fBlockedTimer = 0.0f;
				m_aTargets[i].m_bHasEverHadClearLos = true;
			}
			else
			{
				m_aTargets[i].m_fVisibleTimer = 0.0f;
			}
		}
	}
}


//-------------------------------------------------------------------------
// calculates the two best targets and stores their indices
//-------------------------------------------------------------------------
void CPedTargetting::CalculateBestTargets( void )
{
	s32 iBestTarget = -1, iSecondaryTarget = -1;
	float fBestScore = 0.0f, fSecondaryScore = 0.0f;

	const int numTgs = m_iNumActiveTargets;
	for(int i = 0; i < numTgs; i++)
	{
		if( m_aTargets[i].IsValid() )
		{
			if( ( iBestTarget == -1 ) || ( m_aTargets[i].m_fTotalScore > fBestScore ) )
			{
				// Set our secondary target variables to what the current best is
				iSecondaryTarget = iBestTarget;
				fSecondaryScore = fBestScore;

				// Set our best target info
				iBestTarget = i;
				fBestScore = m_aTargets[i].m_fTotalScore;
			}
			else if( iSecondaryTarget == -1 || m_aTargets[i].m_fTotalScore > fSecondaryScore )
			{
				// This index didn't best the best but still beat our secondary score
				iSecondaryTarget = i;
				fSecondaryScore = m_aTargets[i].m_fTotalScore;
			}
		}
	}
#if !__FINAL
	/*if( m_iCurrentTargetIndex != iBestTarget )
	{
		printf( "Target switching from %i to %i\n", m_iCurrentTargetIndex, iBestTarget );
	}*/
#endif

	// If currently targetting a target without a LOS and whose location is unknown,
	// prevent them from switching to another target without a LOS
	if( m_iSecondBestTargetIndex != iSecondaryTarget &&	m_iSecondBestTargetIndex != -1 && m_aTargets[m_iSecondBestTargetIndex].IsValid() &&
		m_aTargets[m_iSecondBestTargetIndex].HasLastSeenTimerExpired() && iSecondaryTarget != -1 && m_aTargets[iSecondaryTarget].m_losStatus == Los_blocked )
	{
		iSecondaryTarget = m_iSecondBestTargetIndex;
	}

	// If valid, Make sure the secondary target does not decay
	if( iSecondaryTarget >= 0 )
	{
		Assert(iSecondaryTarget < m_iNumActiveTargets);
		Assert(iSecondaryTarget < MAX_NUM_TARGETS);

		m_aTargets[iSecondaryTarget].m_iTimeExpiringMs = fwTimer::GetTimeInMilliseconds() + TARGET_FADE_TIME_MS;
	}

	m_iSecondBestTargetIndex = (s16)iSecondaryTarget;


	// If currently targetting a target without a LOS and whose location is unknown,
	// prevent them from switching to another target without a LOS
	if( m_iCurrentTargetIndex != iBestTarget &&	m_iCurrentTargetIndex != -1 && m_aTargets[m_iCurrentTargetIndex].IsValid() &&
		m_aTargets[m_iCurrentTargetIndex].HasLastSeenTimerExpired() && iBestTarget != -1 && m_aTargets[iBestTarget].m_losStatus == Los_blocked )
	{
		iBestTarget = m_iCurrentTargetIndex;
	}
	
	// If valid, Make sure the currently selected target does not decay
	if( iBestTarget >= 0 )
	{
		Assert(iBestTarget < m_iNumActiveTargets);
		Assert(iBestTarget < MAX_NUM_TARGETS);

		m_aTargets[iBestTarget].m_iTimeExpiringMs = fwTimer::GetTimeInMilliseconds() + TARGET_FADE_TIME_MS;
	}

	m_iCurrentTargetIndex = (s16)iBestTarget;
}

#include "system/xtl.h"

EXT_PF_TIMER(TargettingLos);

//-------------------------------------------------------------------------
// Main update function to add new targets and update scores
// Returns whether the targeting system is currently active
//-------------------------------------------------------------------------
bool CPedTargetting::UpdateTargettingInternal( const CEntityScanner& entityScanner, const Vector3& vSourcePos, const float fSourceHeading, bool bUpdateActivityTimer, bool bDoFullUpdate )
{
#if __XENON && !__FINAL
	PIXBeginC(0, Color_orange.GetColor(), "CPedTargetting::UpdateTargettingInternal");
#endif

	PF_FUNC(TargettingLos);

	// Should the targeting be active?
	// Targeting is deactivated if not used for an amount of time
	if( m_fActivityTimer <= 0.0f )
	{
		// If the system is currently active, reset
		if( m_bActive )
		{
			ResetTargetting();
		}
		return false;
	}

	if(bUpdateActivityTimer)
	{
		m_fActivityTimer -= fwTimer::GetTimeStep();
	}

	// First update any active timers
	UpdateTargets();

	// Take a note of each peds previous LOS state
	LosStatus aPreviousLosStatus[MAX_NUM_TARGETS];

	const int numActiveTargets = m_iNumActiveTargets;
	Assert(numActiveTargets <= MAX_NUM_TARGETS);
	for( int i = 0; i < numActiveTargets; i++ )
	{
		aPreviousLosStatus[i] = m_aTargets[i].m_losStatus;

		// Reset the being targeted value back to false
		if(bDoFullUpdate)
		{
			m_aTargets[i].m_bIsBeingTargeted = false;
		}
	}

	// If probes have been set up asynchronously, then the next frame when this is called they will be collated
	ProcessAsyncResults();

	Assert(numActiveTargets == m_iNumActiveTargets);	// Make sure nothing changed in the array while we keep aPreviousLosStatus[] in parallel.

	const bool bProcessThisFrame = !IsRegistered() || ShouldBeProcessedThisFrame();

	// Update the LOS status to the targets
	if( bProcessThisFrame )
	{
		StartProcess();
		UpdateLosStatus();
		StopProcess();
	}

	Assert(numActiveTargets == m_iNumActiveTargets);	// Make sure nothing changed in the array while we keep aPreviousLosStatus[] in parallel.

	UpdatePostLos( aPreviousLosStatus );

	// Score each of the chosen targets
	ScoreTargets( entityScanner, vSourcePos, fSourceHeading, bProcessThisFrame );

	// Calculates and stores the best current and secondary targets
	CalculateBestTargets();

#if __XENON && !__FINAL
	PIXEnd();
#endif
	return true;
}


//-------------------------------------------------------------------------
// Updates the validity timers for each target
//-------------------------------------------------------------------------
void CPedTargetting::UpdateTargets( void )
{
	const int numTargets = m_iNumActiveTargets;
	if(numTargets == 0)
	{
		return;
	}

	int index = m_iNextTargetForUpdateTargetsToCheck;
	if(index >= numTargets)
	{
		index = 0;
	}

	Assert(index >= 0 && index < m_iNumActiveTargets);
	if( m_aTargets[index].IsValid() )
	{
		// Make sure this target is still considered valid
		if( TargetShouldBeRemoved( &m_aTargets[index] ) )
		{
			m_aTargets[index].m_iTimeExpiringMs = 0;
		}

		// If target locking is enabled, make sure all other entities are removed and
		// the locked target always remains
		if( m_pLockedTarget )
		{
			// all other entities are removed
			if( m_aTargets[index].m_pEntity != m_pLockedTarget )
			{
				m_aTargets[index].m_iTimeExpiringMs = 0;
			}

			// the locked target always remains
			if( m_aTargets[index].m_pEntity == m_pLockedTarget )
			{
				m_aTargets[index].m_iTimeExpiringMs = fwTimer::GetTimeInMilliseconds() + TARGET_FADE_TIME_MS;
			}
		}

		if(m_aTargets[index].m_iTimeExpiringMs <= fwTimer::GetTimeInMilliseconds())
		{
			ResetTarget(index);
			DeleteTargetFromArrays(index);
			index--;
		}
	}
	else
	{
		// This ResetTarget() may not be strictly necessary, but may help
		// to clear out some registered pointers and otherwise ensure that
		// all elements past the first m_iNumActiveTargets are kept in a consistently
		// cleared out state.
		ResetTarget(index);
		DeleteTargetFromArrays(index);
		index--;
	}

	CompileTimeAssert(MAX_NUM_TARGETS < 0xff);
	m_iNextTargetForUpdateTargetsToCheck = (u8)(index + 1);
}

//-------------------------------------------------------------------------
// Find the target in the array, if present, return a pointer to it
//-------------------------------------------------------------------------
const CSingleTargetInfo* CPedTargetting::FindTarget( const CEntity* pEntity ) const
{
	if(pEntity)
	{
		const int numActiveTargets = m_iNumActiveTargets;
		for(int i = 0; i < numActiveTargets; i++)
		{
			if( m_aTargets[i].m_pEntity == pEntity )
			{
				return &m_aTargets[i];
			}
		}
	}
	return NULL;
}

//-------------------------------------------------------------------------
// Find the target in the array, if present, return a pointer to it
//-------------------------------------------------------------------------
CSingleTargetInfo* CPedTargetting::FindTarget( const CEntity* pEntity )
{
	if(pEntity)
	{
		const int numActiveTargets = m_iNumActiveTargets;
		for(int i = 0; i < numActiveTargets; i++)
		{
			if( m_aTargets[i].m_pEntity == pEntity )
			{
				return &m_aTargets[i];
			}
		}
	}
	return NULL;
}

//-------------------------------------------------------------------------
// Find the target in the array, if present, return a pointer to it
//-------------------------------------------------------------------------
s32 CPedTargetting::FindTargetIndex( const CEntity* pEntity )
{
	if(pEntity)
	{
		const int numActiveTargets = m_iNumActiveTargets;
		for(int i = 0; i < numActiveTargets; i++)
		{
			if( m_aTargets[i].m_pEntity == pEntity )
			{
				return i;
			}
		}
	}
	return -1;
}


//-------------------------------------------------------------------------
// Resets the target in the given slot
//-------------------------------------------------------------------------
void CPedTargetting::ResetTarget( const s32 iTargetIndex )
{
	OnTargetReset( iTargetIndex );
	m_aTargets[iTargetIndex].Reset();
}

//-------------------------------------------------------------------------
// Removes a target (if we find it)
//-------------------------------------------------------------------------
void CPedTargetting::RemoveThreat( const CEntity* pTarget )
{
	s32 iTargetIndex = FindTargetIndex(pTarget);
	if(iTargetIndex != -1)
	{
		ResetTarget(iTargetIndex);
	}
}

dev_float DISTANCE_TO_TARGET_TO_IGNORE_FRIENDLIES = 3.0f;
dev_float DISTANCE_TO_TARGET_TO_REDUCE_FRIENDLYS_INFLUENCE = 15.0f;

//-------------------------------------------------------------------------
// Returns the reduction calculated based on the no. of friendlies also
// targeting this entity
//-------------------------------------------------------------------------
float CPedTargetting::CalculateFriendlyTargettingReduction( const CEntityScanner& entityScanner, const Vector3& vPos, const CEntity* pEntity )
{
	float fTargettingReduction = 1.0f;
	float fToTarget = vPos.Dist(VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition()));
	// Don't adjust for friendlies when we're really close to the target
	if( fToTarget < DISTANCE_TO_TARGET_TO_IGNORE_FRIENDLIES )
	{
		return 1.0f;
	}

	// Get the list of peds from the ped scanner passed

	const CEntityScannerIterator entityList = entityScanner.GetIterator();
	for( const CEntity* pEnt = entityList.GetFirst(); pEnt; pEnt = entityList.GetNext() )
	{
		// Reduce the targeting score if already targeted by this entity
		if( IsEntityTargettingEntity( pEnt, pEntity ) )
		{
			fTargettingReduction *= GetFriendlyTargettingScoreReduction( pEnt, pEntity );
		}
	}

	if( fToTarget < DISTANCE_TO_TARGET_TO_REDUCE_FRIENDLYS_INFLUENCE )
	{
		float fOneMinusReduction = 1.0f - fTargettingReduction;
		fTargettingReduction += fOneMinusReduction * (1.0f - (fToTarget/DISTANCE_TO_TARGET_TO_REDUCE_FRIENDLYS_INFLUENCE));
	}

	// reduce the amount of targeting reduction based on the distance to the target.
	return fTargettingReduction;
}


//-------------------------------------------------------------------------
// Calculates a component of the targetting score based on the location
// and visibility of the entity
//-------------------------------------------------------------------------
float CPedTargetting::CalculateSpatialTargettingScore( float& fOutDistance, const Vector3& vPos, const float, CSingleTargetInfo* pTargetInfo )
{
	float fScore = 0.0f;
	const CEntity* pTarget = pTargetInfo->m_pEntity;
	const CPed* pTargetPed = pTarget && pTarget->GetIsTypePed() ? static_cast<const CPed*>(pTarget) : NULL;

	// The score is based on the distance to the target
	fOutDistance = rage::Clamp( vPos.Dist( VEC3V_TO_VECTOR3(pTarget->GetTransform().GetPosition()) ), 0.0f, m_fMaxTargetRange );
	fScore = 1.0f - ( fOutDistance / m_fMaxTargetRange );

	// and and angle to the target unless the limit is set to 1.0f
	if( m_fDotAngularLimit != 1.0f )
	{
		// calculate the angle to the target and use it in the equation
	}
	else
	{
		fScore += 1.0f;
	}

	// Reduce the score for target who are in aircrafts if we prefer targets who are on foot or in grounded vehicles
	if( m_pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_PreferNonAircraftTargets) &&
		pTargetPed && pTargetPed->GetVehiclePedInside() && pTargetPed->GetVehiclePedInside()->GetIsAircraft() )
	{
		fScore *= ms_Tunables.m_fTargetInAircraftWeighting;
	}

	// Finally this target is reduced in value if there is no Los
	if( pTargetInfo->m_losStatus != Los_clear )
	{
		if( !m_pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_CanIgnoreBlockedLosWeighting) ||
			pTargetInfo->GetTimeLosBlocked() > ms_Tunables.m_fTimeToIgnoreBlockedLosWeighting )
		{
			fScore *= ms_Tunables.m_fBlockedLosWeighting;
		}
	}
	else if( !NetworkInterface::IsGameInProgress() && pTargetPed )
	{
		// Extra weight for close players in SP games
		if( pTargetPed->IsLocalPlayer() || pTargetPed->GetPedConfigFlag(CPED_CONFIG_FLAG_TreatAsPlayerDuringTargeting) )
		{
			float fExtaFuzzyDistance = pTargetInfo->GetIsBeingTargeted() ? ms_Tunables.m_fPlayerBeingTargetedExtraDistance : 0.0f;
			ScalarV scDistanceToTarget = ScalarVFromF32(fOutDistance);
			if( IsLessThanAll(scDistanceToTarget, ScalarVFromF32(ms_Tunables.m_fPlayerThreatDistance + fExtaFuzzyDistance)) ||
				((pTargetInfo->m_iTimeLastHostileActionMs && (fwTimer::GetTimeInMilliseconds() < pTargetInfo->m_iTimeLastHostileActionMs + ms_Tunables.m_iPlayerDirectThreatTimeMs))
				&& (IsLessThanAll(scDistanceToTarget, ScalarVFromF32(ms_Tunables.m_fPlayerDirectThreatDistance + fExtaFuzzyDistance)))))
			{
				fScore *= ms_Tunables.m_fPlayerHighThreatWeighting;
			}
		}
	}
	
	return fScore;    
}


//-------------------------------------------------------------------------
// Updates the validity timers for each target
//-------------------------------------------------------------------------
void CPedTargetting::ScoreTargets( const CEntityScanner& entityScanner, const Vector3& vPos, const float fHeading, const bool bProcessAll )
{
	const int numTargets = m_iNumActiveTargets;
	for(int i = 0; i < numTargets; i++)
	{
		if( m_aTargets[i].IsValid() && ( m_aTargets[i].GetNeverScored() || bProcessAll ) )
		{
			m_aTargets[i].SetNeverScored(false);
			// If the target has not been seen recently, it scores hardly anything
			if( m_aTargets[i].HasLastSeenTimerExpired() )
			{
				// Make the score more than 0, this is so the t
				m_aTargets[i].m_fTotalScore = 0.1f;
				// If this is the existing target, weight it more heavily so the ped is more likely to stick with it
				if( i == m_iCurrentTargetIndex )
				{
					m_aTargets[i].m_fTotalScore *= ms_Tunables.m_fExistingTargetScoreWeight;
				}
#if DEBUG_DRAW
				if( CPedTargetting::DebugTargettingLos )
					grcDebugDraw::Line( vPos + Vector3(0.0f, 0.0f, 0.3f), m_aTargets[i].GetLastPositionSeen(), Color32( 0xff, 0xff, 0xff, 0xff ) );
#endif

				continue;
			}

			// Calculate the specific score associated with this target
			m_aTargets[i].m_fTotalScore = GetTargettingScore( m_aTargets[i].m_pEntity );
			if(m_aTargets[i].m_fTotalScore > 0.0f)
			{
				// Calculate the score adjustment based on how many friendly entities are targeting it
				m_aTargets[i].m_fTotalScore *= CalculateFriendlyTargettingReduction( entityScanner, vPos, m_aTargets[i].m_pEntity ); 

				// If this is the existing target, weight it more heavily so the ped is more likely to stick with it
				if( i == m_iCurrentTargetIndex )
				{
					m_aTargets[i].m_fTotalScore *= GetExistingTargetReduction();
				}

				// Calculate the spatial score associated with this target
				// e.g. how close it is, is it in the correct direction, can you see it?
				m_aTargets[i].m_fTotalScore += CalculateSpatialTargettingScore( m_aTargets[i].GetDistance(), vPos, fHeading, &m_aTargets[i] );

				CPed * pTargetPed = (CPed*)m_aTargets[i].m_pEntity.Get();
				if (pTargetPed && pTargetPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsDecoyPed))
				{
					m_aTargets[i].m_fTotalScore *= DECOY_PED_SCORING_MULTIPLIER;
				}
			}
		}
	}
}


//-------------------------------------------------------------------------
// Adds a single target to the target list
//-------------------------------------------------------------------------
bool CPedTargetting::AddTarget( const CEntity* pEntity, bool bForceInsert )
{
	// Loop through the existing targets trying to find an invalid one 
	const int numActiveTargets = m_iNumActiveTargets;
	for( int i = 0; i < numActiveTargets; i++ )
	{
		if( !m_aTargets[i].IsValid() )
		{
			ResetTarget(i);
			m_aTargets[i].SetTarget( pEntity );
			return true;
		}
	}

	// If we didn't find an unused existing target, check to see if we have space
	// to add one to the array.
	if(numActiveTargets < MAX_NUM_TARGETS)
	{
		const int newTargetIndex = m_iNumActiveTargets++;
		ResetTarget(newTargetIndex);
		m_aTargets[newTargetIndex].SetTarget( pEntity );
		return true;
	}

	if(bForceInsert)
	{
		s32 iWorstScoredIndex = -1;
		float fWorstScore = FLT_MAX;

		// Check for the worst scores valid target
		for( int i = 0; i < numActiveTargets; i++ )
		{
			if(m_aTargets[i].m_fTotalScore < fWorstScore)
			{
				iWorstScoredIndex = i;
				fWorstScore = m_aTargets[i].m_fTotalScore;
			}
		}

		// If we found a valid target we should replace then go ahead and do so
		if(iWorstScoredIndex >= 0)
		{
			ResetTarget(iWorstScoredIndex);
			m_aTargets[iWorstScoredIndex].SetTarget( pEntity );
			return true;
		}
	}

	return false;
}

/*
PURPOSE
	Delete an element from the target array
NOTES
	The target element is assumed to have already been cleared out using
	ResetTarget().
*/
void CPedTargetting::DeleteTargetFromArrays(int targetIndex)
{
#if __ASSERT
	const CEntity* tgt1Before = GetCurrentTarget();
	const CEntity* tgt2Before = GetSecondaryTarget();
#endif

	int numTargets = m_iNumActiveTargets;

	Assert(targetIndex >= 0 && targetIndex < numTargets);
	Assert(!m_aTargets[targetIndex].m_pEntity);	// We expect ResetTarget() to have been called already.

	const int targetIndexToCopy = numTargets - 1;

	OnTargetDeleteFromArray(targetIndex);

	// If the element we are about to delete was considered the
	// current or secondary target, set the relevant target index back to -1.
	if(m_iCurrentTargetIndex == targetIndex)
	{
		m_iCurrentTargetIndex = -1;

		// Note: we could probably copy m_iSecondBestTargetIndex into m_iCurrentTargetIndex here.
		// However, the way it worked before, when we just reset the targets that became invalidated,
		// we didn't do anything like that, so presumably the next target selection call will be able
		// to sort it out OK.
	}
	if(m_iSecondBestTargetIndex == targetIndex)
	{
		m_iSecondBestTargetIndex = -1;
	}

	if(targetIndexToCopy != targetIndex)
	{
		// If the current or secondary target was the one we are now moving to the element
		// that got removed, keep the target index up to date.
		if(m_iCurrentTargetIndex == targetIndexToCopy)
		{
			m_iCurrentTargetIndex = (s16)targetIndex;
		}
		if(m_iSecondBestTargetIndex == targetIndexToCopy)
		{
			m_iSecondBestTargetIndex = (s16)targetIndex;
		}

		m_aTargets[targetIndex] = m_aTargets[targetIndexToCopy];
		ResetTarget(targetIndexToCopy);
	}

	numTargets--;
	m_iNumActiveTargets = (s16)numTargets;

	// Maintain m_iNextLosIndex: it probably doesn't matter too much how we
	// do it, since we have reordered the array we can't exactly preserve
	// the LOS ordering. m_iNextLosIndex is in fact the last index that we checked,
	// and the thing that matters is that it doesn't go any higher than the number
	// of active targets.
	if(m_iNextLosIndex >= numTargets)
	{
		m_iNextLosIndex = 0;
	}

	// Note: for m_iNextTargetForUpdateTargetsToCheck, there isn't really anything to maintain.
	// It's already set up to be allowed to go beyond the number of targets and will just reset
	// itself on the next call to UpdateTargets() if that's the case. Plus, this function is currently
	// only called from UpdateTargets() who will overwrite it anyway.

	Assert(tgt1Before == GetCurrentTarget());
	Assert(tgt2Before == GetSecondaryTarget());
}


//-------------------------------------------------------------------------
// Adds targets from the ped scanner depending on the hated 
// Returns false if it failed to add all valid targets due to space restrictions
//-------------------------------------------------------------------------
/*bool CPedTargetting::AddTargets( CEntityScanner& entityScanner )
{
	// If the target is locked, there's no need to add new targets
	if( m_pLockedTarget )
	{
		return true;
	}

	// Get the list of peds from the ped scanner passed
	CEntityScannerIterator entityList = entityScanner.GetIterator();
	for( CEntity* pEnt = entityList.GetFirst(); pEnt; pEnt = entityList.GetNext() )
	{
		// Is the target valid?
		if( IsValidTarget( pEnt ) )
		{
			CSingleTargetInfo* pTargetInfo = FindTarget( pEnt );
			// Is the target already present?
			if( pTargetInfo )
			{
				// Revalidate the existing target
				pTargetInfo->Revalidate();
			}
			// The target is not already present
			// add it to the list
			else if( !AddTarget( pEnt ) )
			{
				// Adding failed, try and clear space for the new target
				if( RemoveLeastValidTarget() )
				{
					AddTarget( pEnt );
				}
				else
				{
					// No point trying to add any more targets, 
					// the list is already full
					return false;
				}
			}
		}
	}
	return true;
}*/

//-------------------------------------------------------------------------
// Resets all targetting information
//-------------------------------------------------------------------------
void CPedTargetting::ResetTargetting( void )
{
	const int numActiveTargets = m_iNumActiveTargets;
	for(int i = 0; i < numActiveTargets; i++)
	{
		ResetTarget(i);
	}
	m_iNumActiveTargets = 0;

	m_fActivityTimer		= 0.0f;
	m_iCurrentTargetIndex	= -1;
	m_iSecondBestTargetIndex= -1;
	m_bActive				= false;
	m_iNextLosIndex			= 0;
	m_iNextTargetForUpdateTargetsToCheck = 0;
}

//-------------------------------------------------------------------------
// Locks the targetting to only consider this entity
//-------------------------------------------------------------------------
void CPedTargetting::LockTarget( CEntity* pLockedEntity )
{
	// Make sure any previous entity is unlocked
	UnlockTarget();

	m_pLockedTarget = pLockedEntity;

	if( m_pLockedTarget )
	{
		AddTarget( m_pLockedTarget );
	}
}


//-------------------------------------------------------------------------
// Unlocks the targetting making it free to target any entity
//-------------------------------------------------------------------------
void CPedTargetting::UnlockTarget( void )
{
	if( m_pLockedTarget )
	{
		m_pLockedTarget = NULL;
	}
}

//-------------------------------------------------------------------------
// Sets the targetting as currently active, being used by some external system
//-------------------------------------------------------------------------
void CPedTargetting::SetInUse()
{
	m_fActivityTimer = ms_Tunables.m_fTargetingInactiveDisableTime;
}

//-------------------------------------------------------------------------
// Ped targetting class, to be used by peds targetting other peds.
//-------------------------------------------------------------------------

float CPedTargetting::BASIC_THREAT_WEIGHTING			= 0.5f;
float CPedTargetting::ACTIVE_THREAT_WEIGHTING			= 1.0f;
float CPedTargetting::FLEEING_THREAT_WEIGHTING			= 0.5f;
float CPedTargetting::WEAPON_PISTOL_WEIGHTING			= 1.5f;
float CPedTargetting::WEAPON_2HANDED_WEIGHTING			= 2.0f;
float CPedTargetting::DIRECT_THREAT_WEIGHTING			= 1.6f;
float CPedTargetting::IN_VEHICLE_WEIGHTING				= 0.75;
float CPedTargetting::UNARMED_DIRECT_THREAT_WEIGHTING	= 1.6f;
float CPedTargetting::PLAYER_THREAT_WEIGHTING			= 2.5f;
float CPedTargetting::DEFEND_PLAYER_WEIGHTING			= 1.5f;
float CPedTargetting::FRIENDLY_TARGETTING_REDUCTION		= 0.75f;
float CPedTargetting::SCRIPT_THREAT_WEIGHTING			= 5.0f;
float CPedTargetting::DECOY_PED_SCORING_MULTIPLIER		= 100.f;
//-------------------------------------------------------------------------
// Constructor
//-------------------------------------------------------------------------
CPedTargetting::CPedTargetting( CPed* pPed ):
CExpensiveProcess(PPT_TargettingLos),
m_pScoringFn(DefaultTargetScoring),
m_fScoringOverrideTimer(0.0f),
m_pScoringEntity(NULL),
m_bActive(false),
m_fActivityTimer(0.0f),
m_iCurrentTargetIndex(-1),
m_iSecondBestTargetIndex(-1),
m_fMaxTargetRange(0.0f),
m_fDotAngularLimit(0.0f),
m_iNextLosIndex(0),
m_pLockedTarget(NULL),
m_bRunningInCheapMode(false),
m_bUseAlternatePosition(false),
m_iNextTargetForUpdateTargetsToCheck(0),
m_iNumActiveTargets(0)
{
	RegisterSlot();

	m_pPed = pPed;
	Assert( m_pPed );

	for( int i = 0; i < MAX_NUM_TARGETS; i++ )
	{
		m_apVehiclesTargetsLastSeenIn[i] = NULL;
	}
}

//-------------------------------------------------------------------------
// Destructor
//-------------------------------------------------------------------------
CPedTargetting::~CPedTargetting()
{
	if( !m_bRunningInCheapMode )
		UnregisterSlot();

	if( m_pPed )
		m_pPed = NULL;
}


//-------------------------------------------------------------------------
// Carries out an entire targetting update pass
//-------------------------------------------------------------------------
bool CPedTargetting::UpdateTargetting( bool bDoFullUpdate )
{
	Assert(m_pPed);

	if( m_pPed->GetWeaponManager() && m_pPed->GetWeaponManager()->GetIsArmed() )
		SetSpatialTargettingLimits( CPedPerception::ms_fSenseRangeOfMissionPeds, 1.0f );
	else
		SetSpatialTargettingLimits( 5.0f, 1.0f );

	// Disable targetting update for dead peds by setting the timer to 0, 
	// this allows the targetting system to clear up correctly
	if( m_pPed->IsDead() )
	{
		m_fActivityTimer = 0.0f;
	}

	// If the target scoring function is overridden, and the timer is set, count
	// down until expired
	if( m_pScoringFn != m_pPed->GetPedIntelligence()->GetDefaultTargetScoringFunction() )
	{
		// If the timer is set, count down
		if( m_fScoringOverrideTimer > 0.0f )
		{
			m_fScoringOverrideTimer -= fwTimer::GetTimeStep();
			// Timer expired, reset the target scoring function
			if( m_fScoringOverrideTimer <= 0.0f )
			{
				ResetScoringFunction();
			}
		}
	}

	bool bShouldUpdateActivityTimer = !m_pPed->GetUsingRagdoll();
	const Vector3& vTestPosition = m_bUseAlternatePosition ? m_vAlternatePosition : VEC3V_TO_VECTOR3(m_pPed->GetTransform().GetPosition());
	bool bReturn = UpdateTargettingInternal(
		*m_pPed->GetPedIntelligence()->GetPedScanner(),
		vTestPosition,
		m_pPed->GetTransform().GetHeading(),
		bShouldUpdateActivityTimer,
		bDoFullUpdate
	);

	// Want to make sure to reset this at the very end of the update
	m_bUseAlternatePosition = false;
	return bReturn;
}

dev_float INTERSECTION_BLOCKED_DISTANCE = 1.25f;

//*******************************************************************************
void
CPedTargetting::ProcessAsyncResults()
{
	const int numTargets = m_iNumActiveTargets;
	for(int t = 0; t < numTargets; t++)
	{
		if(m_aTargets[t].m_bLosWasDoneAsync)
		{
			if(!m_aTargets[t].m_pEntity)
				continue;	// cancel request, or let time-out?

			Assert(m_aTargets[t].m_pEntity->GetIsTypePed());
			if(!m_aTargets[t].m_pEntity->GetIsTypePed())
				continue;

			CPed * pOtherPed = (CPed*)m_aTargets[t].m_pEntity.Get();
			Vector3 vLosStartPos = VEC3V_TO_VECTOR3(m_pPed->GetTransform().GetPosition());
			Vector3 vLosEndPos = VEC3V_TO_VECTOR3(pOtherPed->GetTransform().GetPosition());
			ECanTargetResult eRet;
			if (pOtherPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsDecoyPed))
			{
				eRet = ECanTarget;
			}
			else
			{
				eRet = CPedGeometryAnalyser::CanPedTargetPedAsync(*m_pPed, *pOtherPed, 0, true, CPedGeometryAnalyser::ms_fCanTargetCacheMaxPosChangeSqr, CAN_TARGET_CACHE_TIMEOUT, &vLosStartPos, &vLosEndPos);
			}

			if (eRet == ECanTarget)
			{
				// Set the status to clear, unless we were blocked by friendly before.
				// If we are blocked by friendly, we will reevaluate that the next time
				// UpdateSingleLos() gets called.
				if(m_aTargets[t].m_losStatus != Los_blockedByFriendly)
				{
					m_aTargets[t].m_losStatus = Los_clear;
				}
				m_aTargets[t].m_bLosWasDoneAsync = false;	// Set this, other we'll keep doing this forever
				m_aTargets[t].m_bLosBlockedNear = false;
			}
			else if(eRet == ECanNotTarget)
			{
				m_aTargets[t].m_losStatus = Los_blocked;
				m_aTargets[t].m_bLosWasDoneAsync = false;	// Set this, other we'll keep doing this forever
				m_aTargets[t].m_bLosBlockedNear = vLosStartPos.Dist2(vLosEndPos) < rage::square(INTERSECTION_BLOCKED_DISTANCE);
#if DEBUG_DRAW
				TUNE_GROUP_BOOL(COVER_DEBUG, RENDER_BLOCKED_NEAR_CHECK, false);
				if (RENDER_BLOCKED_NEAR_CHECK)
				{
					CTask::ms_debugDraw.AddLine(RCC_VEC3V(vLosStartPos), RCC_VEC3V(vLosEndPos), m_aTargets[t].m_bLosBlockedNear ? Color_red : Color_green, 1000);
				}
#endif // DEBUG_DRAW
			}
		}
	}
}


//-------------------------------------------------------------------------
// Allows a component using the targetting system to override the default target scoring function
// with the one passed. If a time is passed > 0, the default function will revert after that time
//-------------------------------------------------------------------------	
void CPedTargetting::SetScoringFunction( TargetScoringFunction* pScoringFunction, CEntity* pScoringEntity, float fTime) 
{
	// Cannot overwrite an already overridden target scoring function
	if( m_pScoringFn != m_pPed->GetPedIntelligence()->GetDefaultTargetScoringFunction() &&
		pScoringEntity != (CEntity*) CGameWorld::FindLocalPlayer() )
	{
		return;
	}

	m_pScoringFn = pScoringFunction; 
	m_fScoringOverrideTimer = fTime;
	m_pScoringEntity = pScoringEntity;
}

//-------------------------------------------------------------------------
// Resets the scoring function to default
//-------------------------------------------------------------------------	
void CPedTargetting::ResetScoringFunction( void ) 
{
	m_pScoringFn = m_pPed->GetPedIntelligence()->GetDefaultTargetScoringFunction();
	m_fScoringOverrideTimer = 0.0f;
	m_pScoringEntity = NULL;
}


#define LOS_DOT_LIMIT (0.3f)
#if __ASSERT
// static bool DEBUG_LOS_TESTS = false;
#endif
//-------------------------------------------------------------------------
// Updates the Los status of a single entity
//-------------------------------------------------------------------------
void CPedTargetting::UpdateSingleLos( int iIndex, bool bForceImmediateUpdate )
{
	Assert(iIndex >= 0 && iIndex < m_iNumActiveTargets);

	const Vector3 vPedPosition = VEC3V_TO_VECTOR3(m_pPed->GetTransform().GetPosition());
	Assert(m_aTargets[iIndex].IsValid());
	CPed* pTargetPed = (CPed*) m_aTargets[iIndex].m_pEntity.Get();
	const Vector3 vTargetPedPosition = VEC3V_TO_VECTOR3(pTargetPed->GetTransform().GetPosition());
	bool bCheckLos = true;
	bool bIgnoreThisPedsVehicle = false;

	// Force an immediate update if the los status has never been checked
	// Note: we only do this for the player now, to improve performance when new AI
	// targets get added. We may want to skip it for the player too, but we would
	// have to be careful to make sure it doesn't introduce noticeable delays.
	if( !bForceImmediateUpdate && m_aTargets[iIndex].m_losStatus == Los_unchecked )
	{
		bForceImmediateUpdate = pTargetPed->IsLocalPlayer();
	}

	bool bPedCanSeeInAnyDir = m_pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && m_pPed->GetMyVehicle() && (m_pPed->GetMyVehicle()->GetVehicleType() == VEHICLE_TYPE_HELI || m_pPed->PopTypeIsMission() || pTargetPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ));

	// Can hear the target if within a certain range and in a vehicle
	if( pTargetPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && vTargetPedPosition.Dist2(vPedPosition) < rage::square(10.0f) )
	{
		bPedCanSeeInAnyDir = true;
	}

	// Ignore hits against this vehicle if we're in a turret seat (might be better to exclude the turret only somehow??)
	if (m_pPed->GetMyVehicle())
	{
		const s32 iSeatindex = m_pPed->GetMyVehicle()->GetSeatManager()->GetPedsSeatIndex(m_pPed);
		if (m_pPed->GetMyVehicle()->IsTurretSeat(iSeatindex))
		{
			bIgnoreThisPedsVehicle = true;	
		}
	}

	// Can see in any dir if the target has been seen recently
	if( m_aTargets[iIndex].GetTimeUntilLastSeenTimerExpiresMs() > 2500 )
		bPedCanSeeInAnyDir = true;

	// If this target is currently blocked, only regain LOS if facing in the correct direction
	if( m_aTargets[iIndex].m_losStatus == Los_blocked && !bPedCanSeeInAnyDir )
	{
		Vector3 vPedHeading = Vector3( ANGLE_TO_VECTOR_X( ( RtoD *  m_pPed->GetTransform().GetHeading() ) ), ANGLE_TO_VECTOR_Y( ( RtoD *  m_pPed->GetTransform().GetHeading() ) ), 0.0f );
		Vector3 vPedToTarget =  vTargetPedPosition - vPedPosition;
		vPedHeading.Normalize();
		vPedToTarget.Normalize();

		if( vPedHeading.Dot( vPedToTarget ) < LOS_DOT_LIMIT )
		{
			bCheckLos = false;
		}
	}

	// Cheap targetting mode.
	m_bRunningInCheapMode = false;
	if( m_bRunningInCheapMode )
	{
		RegisterSlot();
		m_bRunningInCheapMode = false;
	}

#if !__FINAL
	const bool bInvisiblePlayer = pTargetPed->IsPlayer() && CPlayerInfo::ms_bDebugPlayerInvisible;
#else
		const bool bInvisiblePlayer = false;
#endif // !__FINAL

	//Generate the target flags.
	s32 iTargetFlags = 0;

	//Check if we know where the target is.
	if(m_aTargets[iIndex].AreWhereaboutsKnown())
	{
		iTargetFlags |= TargetOption_UseCoverVantagePoint;
	}
	else
	{
		iTargetFlags |= TargetOption_IgnoreTargetsCover;
		iTargetFlags |= TargetOption_UseFOVPerception;
	}

	if(m_pPed->GetPedResetFlag(CPED_RESET_FLAG_IgnoreTargetsCoverForLOS))
	{
		iTargetFlags |= TargetOption_IgnoreTargetsCover;
	}

	if (bIgnoreThisPedsVehicle)
	{
		iTargetFlags |= TargetOption_IgnoreVehicleThisPedIsIn;
	}
	
	if( bCheckLos && !bInvisiblePlayer )
	{
		bool bCanTarget;
		Vector3 vLosStartPos = vPedPosition;
		Vector3 vLosEndPos = vTargetPedPosition;
		bool bLosBlockedNear = false;
		if(CPedGeometryAnalyser::ms_bProcessCanPedTargetPedAsynchronously && !bForceImmediateUpdate)
		{
			ECanTargetResult eRet = CPedGeometryAnalyser::CanPedTargetPedAsync( *m_pPed, *pTargetPed, iTargetFlags, false, CPedGeometryAnalyser::ms_fCanTargetCacheMaxPosChangeSqr, CAN_TARGET_CACHE_TIMEOUT, &vLosStartPos, &vLosEndPos );
			if(eRet==ECanTarget)
			{
				m_aTargets[iIndex].m_bLosWasDoneAsync = false;
				bCanTarget = true;
			}
			else if(eRet==ECanNotTarget)
			{
				m_aTargets[iIndex].m_bLosWasDoneAsync = false;
				bLosBlockedNear = vLosStartPos.Dist2(vLosEndPos) < rage::square(INTERSECTION_BLOCKED_DISTANCE);
				bCanTarget = false;
			}
			else
			{
				// If we're not sure about the result yet, then don't change from the prev state
				m_aTargets[iIndex].m_bLosWasDoneAsync = true;

				// If the status from before was either clear or blocked by friendly, we still
				// need to go ahead and do the friendly fire check even if we are waiting for a probe
				// to finish. If we don't, we may never update the friendly fire status properly, if
				// we move fast enough to invalidate the cached results.
				const LosStatus status = m_aTargets[iIndex].GetLosStatus();
				if(status == Los_clear || status == Los_blockedByFriendly)
				{
					bCanTarget = true;
				}
				else
				{
					return;
				}
			}
		}
		else
		{
			// Note: even in this bForceImmediateUpdate case, it would probably be good to try to cache
			// the result, but we don't do that now.

			bCanTarget = CPedGeometryAnalyser::CanPedTargetPed( *m_pPed, *pTargetPed, iTargetFlags, NULL, &vLosStartPos, &vLosEndPos );
			bLosBlockedNear = vLosStartPos.Dist2(vLosEndPos) < rage::square(INTERSECTION_BLOCKED_DISTANCE);;
		}

		// By default not reset
		m_aTargets[iIndex].m_bLosBlockedNear = false;

		if(bCanTarget)
		{
			// The LOS is not blocked by any peds or any buildings
			// Check for friendly fire

			CPed* pBlockingPed = ProbeForFriendlyFireBlockingPed(pTargetPed, VECTOR3_TO_VEC3V(vPedPosition), VECTOR3_TO_VEC3V(vTargetPedPosition));

			// Set the blocking ped
			m_aTargets[iIndex].m_losStatus = pBlockingPed ? Los_blockedByFriendly : Los_clear;
			m_aTargets[iIndex].m_pBlockedByFriendlyPed = pBlockingPed;
		}
		else
		{
			// LOS blocked by buildings or map objects
			m_aTargets[iIndex].m_losStatus = Los_blocked;
			m_aTargets[iIndex].m_bLosBlockedNear = bLosBlockedNear;
		}
	}
	else
	{
		// LOS blocked by buildings or map objects
		m_aTargets[iIndex].m_losStatus = Los_blocked;
		m_aTargets[iIndex].m_bLosBlockedNear = false;
	}
}

//-------------------------------------------------------------------------
// Carries out an LOS status update
//-------------------------------------------------------------------------
void CPedTargetting::UpdateLosStatus( void )
{
	s32 iIndex = m_iNextLosIndex;
	const int numTargets = m_iNumActiveTargets;
	for( s32 i = 1; i < ( numTargets + 1 ); i++ )
	{
		s32 iThisIndex = i + iIndex;
		if( iThisIndex >= numTargets )
		{
			iThisIndex -= numTargets;

			Assert(iThisIndex >= 0 && iThisIndex < numTargets);
		}

		// If this is a valid target, check the los
		if( m_aTargets[iThisIndex].IsValid() )
		{
			UpdateSingleLos( iThisIndex, false );

			m_iNextLosIndex = (s16)iThisIndex;
			return;
		}
	}

	// No valid targets, reset the next los to 0
	m_iNextLosIndex = 0;
}


bool CPedTargetting::AllowedToTargetPed( const CPed* pTarget, bool bIsBeingTargeted ) const
{
	if(!pTarget)
	{
		return false;
	}

	// We can continue attacking some peds from combat even if dead/injured
	// so make sure a ped isn't being targeted before removing them due to being dead/injured
	if(!bIsBeingTargeted) 
	{
		if(pTarget->IsDead() )
		{
			return false;
		}

		if( pTarget->IsInjured() && !m_pPed->WillAttackInjuredPeds() )
		{
			return false;
		}
	}

	// Check if this is a respawning network player
	if(NetworkInterface::IsGameInProgress() && pTarget->IsAPlayerPed())
	{
		CPlayerInfo* pTargetPlayerInfo = pTarget->GetPlayerInfo();
		if( pTargetPlayerInfo->GetPlayerState() == CPlayerInfo::PLAYERSTATE_RESPAWN ||
			(pTarget->IsNetworkClone() && pTargetPlayerInfo->GetPlayerState() == CPlayerInfo::PLAYERSTATE_HASDIED) )
		{
			return false;
		}

		// Check if the player has been respawned recently
		if(pTarget->GetPlayerInfo()->GetHasRespawnedWithinTime(CTaskCombat::ms_Tunables.m_MaxTimeToRejectRespawnedTarget))
		{
			// Also need to check how long we've been in the combat task (if we have one)
			const CTaskCombat* pCombatTask = static_cast<const CTaskCombat*>(m_pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_COMBAT));
			if(pCombatTask && (pCombatTask->GetTimeRunning() * 1000.0f) > CTaskCombat::ms_Tunables.m_MaxTimeToRejectRespawnedTarget)
			{
				return false;
			}
		}

		// ignore players in a different tutorial space
		CNetObjPed* pNetObjPed = static_cast<CNetObjPed*>(pTarget->GetNetworkObject());
		if(pNetObjPed && pNetObjPed->GetPlayerOwner() && pNetObjPed->GetPlayerOwner()->IsInDifferentTutorialSession())
		{
			return false;
		}

		//Ensure that if the playerped is dead - they are not allowed to be targeted (lavalley)
		if (pTarget->IsDead())
		{
			return false;
		}

		//If the player has just respawned and has an invincibility timer active - they are not allowed to be targeted (lavalley)
		if (pTarget->GetNetworkObject())
		{
			CNetObjPlayer* netObjPlayer = static_cast<CNetObjPlayer*>(pTarget->GetNetworkObject());
			if(netObjPlayer && netObjPlayer->GetRespawnInvincibilityTimer() > 0)
			{
				return false;
			}		
		}
	}

	CPedIntelligence* pPedIntelligence = m_pPed->GetPedIntelligence();
	const CRelationshipGroup* pRelGroup = pPedIntelligence->GetRelationshipGroup();
	const int otherRelGroupIndex = pTarget->GetPedIntelligence()->GetRelationshipGroupIndex();

	// Check if we want to ignore the ped
	if( pRelGroup && pRelGroup->CheckRelationship(ACQUAINTANCE_TYPE_PED_IGNORE, otherRelGroupIndex) )
	{
		return false;
	}

	// Is the target a cop and we shouldn't attack cops unless the player is wanted?
	if( !pPedIntelligence->CanAttackPed(pTarget) )
	{
		return false;
	}

	return true;
}


CPed* CPedTargetting::ProbeForFriendlyFireBlockingPed(CPed* pTargetPed, Vec3V_In pedPositionV, Vec3V_In targetPedPositionV) const
{
	// These are non-critical, increasing them to be large enough will only cost us some extra stack space.
	static const int kMaxPedsNearLine = 64;
	static const int kMaxAdditionalExcludePeds = 16;

	// This is also not very critical, as long as it's large enough so that a sphere with this
	// radius around a ped's transform position includes the whole body.
	static const float pedBoundingRadius = 2.5f;

	// Query the spatial array for all peds that are near the targeting segment.
	CSpatialArrayNode* foundArray[kMaxPedsNearLine];
	int numFound = CPed::GetSpatialArray().FindNearSegment(pedPositionV, targetPedPositionV, pedBoundingRadius, foundArray, kMaxPedsNearLine);

	CPed* pTargetingPed = m_pPed;
	CPed* pBlockingPed = NULL;

	// Next, we will build an array of peds that we don't want to test against.
	int numAdditionalExcludePeds = 0;
	CPed* pAdditionalExcludePeds[kMaxAdditionalExcludePeds];

	// If this ped is in a car, add any other peds in the car.
	// This was adapted from WorldProbe::CShapeTestTaskData::ConvertExcludeEntitiesToInstances(), which
	// is what would have been used when we did the test through the physics level.
	if(pTargetingPed->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle))
	{
		CVehicle* pVehicle = pTargetingPed->GetMyVehicle();
		if(pVehicle && pVehicle->GetFragInst())	// Note: this GetFragInst() thing may not really be needed.
		{
			// Add any passengers in the car.
			const int maxSeats = pVehicle->GetSeatManager()->GetMaxSeats();
			for(int nPassenger = 0; nPassenger < maxSeats; nPassenger++)
			{
				CPed* pPassenger = pVehicle->GetSeatManager()->GetPedInSeat(nPassenger);
				if(pPassenger && pPassenger != pTargetingPed)
				{
					if(aiVerifyf(numAdditionalExcludePeds < kMaxAdditionalExcludePeds, "Too many vehicle passengers, increase kMaxAdditionalExcludePeds."))
					{
						pAdditionalExcludePeds[numAdditionalExcludePeds++] = pPassenger;
					}
					else
					{
						break;
					}
				}
			}
		}
	}

	// Set up a shape test for testing just an individual object, which we will use further down.
	WorldProbe::CShapeTestFixedResults<1> probeResult;
	WorldProbe::CShapeTestProbeDesc probeDesc;
	probeDesc.SetResultsStructure(&probeResult);
	probeDesc.SetStartAndEnd(RCC_VECTOR3(pedPositionV), RCC_VECTOR3(targetPedPositionV));
	probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_PED_TYPE);
	probeDesc.SetDomainForTest(WorldProbe::TEST_AGAINST_INDIVIDUAL_OBJECTS);

	// Loop over the potentially blocking peds that we found.
	float closestTVal = LARGE_FLOAT;
	for(int i = 0; i < numFound; i++)
	{
		// Get the ped through the spatial array node pointer.
		CPed* pPotentialBlockingPed = CPed::GetPedFromSpatialArrayNode(foundArray[i]);
		Assert(pPotentialBlockingPed);

		// Never test against the target or against ourselves. These two peds would generally
		// have been found by the spatial array operation, since the segment goes from one to the other.
		if(pPotentialBlockingPed == pTargetPed || pPotentialBlockingPed == pTargetingPed)
		{
			continue;
		}

		// Next, test if this ped should otherwise be excluded (possibly a passenger of the same vehicle).
		bool shouldBeExcluded = false;
		for(int j = 0; j < numAdditionalExcludePeds; j++)
		{
			if(pAdditionalExcludePeds[j] == pPotentialBlockingPed)
			{
				shouldBeExcluded = true;
				break;
			}
		}
		if(shouldBeExcluded)
		{
			continue;
		}

		// If injured or not friendly, move on to the next target.
		// Note: the original friendly fire code would still test against these,
		// but I don't think that was a good thing - it probably prevented us from
		// finding true blocking allies further back.
		if(pPotentialBlockingPed->IsInjured() || !IsFriendlyEntity(pPotentialBlockingPed))
		{
			continue;
		}

		// Make sure that the ped has a physics instance in the level, otherwise
		// we wouldn't have found it with a probe through the physics level.
		phInst* pInst = pPotentialBlockingPed->GetCurrentPhysicsInst();
		if(!pInst || !pInst->IsInLevel())
		{
			continue;
		}

		// Perform the shape test against just this one ped's instance.
		probeDesc.SetIncludeInstance(pInst);
		if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
		{
			// Get the T value, and see if it's closer than any we've already found.
			const float hitT = probeResult[0].GetHitTValue();
			if(hitT < closestTVal)
			{
				// Update what we've found so far.
				closestTVal = hitT;
				pBlockingPed = pPotentialBlockingPed;

				// We could potentially break out here, if we don't care about
				// m_pBlockedByFriendlyPed being the closest. But, it might be useful
				// to make sure we find the closest, so let's keep that for now.
			}

			// Reset the probe result, since we are done with it. This may not be
			// strictly necessary but feels safest.
			probeResult.Reset();
		}
	}

	// Return what we found.
	return pBlockingPed;
}

//-------------------------------------------------------------------------
// Checks to see if this entity is considered a valid target
//-------------------------------------------------------------------------
/*bool CPedTargetting::IsValidTarget( const CEntity* pEntity ) const
{
	Assert( pEntity );
	if( !pEntity->GetIsTypePed() )
	{
		return false;
	}

	CPed* pPed = (CPed*)pEntity;

	// Don't target this ped
	if( pPed == m_pPed )
	{
		return false;
	}

	if( !AllowedToTargetPed(pPed) )
	{
		return false;
	}

	if( m_pPed->GetPedIntelligence()->IsFriendlyWith(*pPed ) )
	{
		return false;
	}

	// Ignore other group members
	CPedGroup* pPedGroup = m_pPed->GetPedsGroup();
	if( pPedGroup )
	{
		if( pPedGroup->GetGroupMembership()->IsMember(pPed) )
		{
			return false;
		}
	}

	// Valid target if the type is considered threatening
	if( m_pPed->GetPedIntelligence()->IsThreatenedBy(*pPed) )
	{
		Assertf( !m_pPed->GetPedIntelligence()->IsFriendlyWith(*pPed ),  "Ped in relgroup (%s) is trying to fight friendly ped in relgroup (%s)", m_pPed->GetPedIntelligence()->GetRelationshipGroup()->GetName().GetCStr(), pPed->GetPedIntelligence()->GetRelationshipGroup()->GetName().GetCStr()  );
		return true;
	}

	// Also a valid target if in combat and targeting us or a friend
	CEntity* pEntityTarget = pPed->GetPedIntelligence()->GetQueriableInterface()->GetHostileTarget();
	if( pEntityTarget && pEntityTarget->GetIsTypePed() )
	{
		CPed* pPedTarget = (CPed*) pEntityTarget;
		if( ( ( pPedTarget == m_pPed ) ||
			m_pPed->GetPedIntelligence()->IsFriendlyWith( *pPedTarget ) ) )
		{
			return true;
		}
	}

	return false;
}*/


//-------------------------------------------------------------------------
// Returns if the target should be removed, note this is different to the the opposite of hte above function
// to allow temporarily registered threats to persist unlesss dead or completely invalid targets.
//-------------------------------------------------------------------------
bool CPedTargetting::TargetShouldBeRemoved( const CSingleTargetInfo* pTarget ) const
{
	Assert( pTarget );

	const CEntity* pEntity = pTarget->m_pEntity;

	Assert( pEntity );
	
	// Never remove the locked target 
	if( pEntity == m_pLockedTarget )
	{
		return false;
	}

	if( pEntity->GetIsTypePed() )
	{
		CPed* pPed = (CPed*)pEntity;

		if( !AllowedToTargetPed(pPed, pTarget->m_bIsBeingTargeted) )
		{
			return true;
		}
	}

	return false;
}

#define INJURED_PED_WEIGHTING	(0.2f)
#define DYING_PED_WEIGHTING		(0.1f)
#define AIR_COMBAT_TARGET_WEIGHTING (0.05f)
dev_float REDUCE_PLAYER_PASSENGERS_SCALE = 0.1f;

//-------------------------------------------------------------------------
// Default score calculation function
//-------------------------------------------------------------------------
float CPedTargetting::DefaultTargetScoring( CPed* pTargettingPed, const CEntity* pEntity )
{
	Assert( pEntity );
	Assert( pEntity->GetIsTypePed() );
	CPed* pPed = (CPed*) pEntity;

	// Dead peds aren't a target at all
	if( pPed->IsDead() )
	{
		return 0.0f;
	}

	float fScore = BASIC_THREAT_WEIGHTING;

	CQueriableInterface* pPedQueriableInterface = pPed->GetPedIntelligence()->GetQueriableInterface();
	const CEntity* pHostileTarget = pPedQueriableInterface->GetHostileTarget();
	// Targeting score is reduced if the target is fleeing
	if( pPedQueriableInterface->IsTaskCurrentlyRunning( CTaskTypes::TASK_SMART_FLEE ) )
	{
		fScore *= FLEEING_THREAT_WEIGHTING;
	}
	// Targeting score is increased if the target is targeting us
	else if( pHostileTarget == (CEntity*)pTargettingPed )
	{
		// If unarmed, more likely to fight whoever is fighting us
		const CPedWeaponManager *pedWeaponMgr = pPed->GetWeaponManager();
		if( pedWeaponMgr && pedWeaponMgr->GetEquippedWeapon() && !pedWeaponMgr->GetEquippedWeapon()->GetWeaponInfo()->GetIsMelee() )
		{
			fScore *= DIRECT_THREAT_WEIGHTING;
		}
		else
		{
			fScore *= UNARMED_DIRECT_THREAT_WEIGHTING;
		}
	}
	// Or if the target is the player
	else if( pPed->IsPlayer() || pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_TreatAsPlayerDuringTargeting) )
	{
		if( pPed->GetWeaponManager() && pPed->GetWeaponManager()->GetIsArmed() )
			fScore *= PLAYER_THREAT_WEIGHTING;
		else
			fScore *= ACTIVE_THREAT_WEIGHTING;
	}
	else if( pHostileTarget != NULL )
	{
		fScore *= ACTIVE_THREAT_WEIGHTING;
	}

	// If the target is in the players vehicle, the player has a higher target rating.
	if( pTargettingPed->GetPedType() == PEDTYPE_COP && !pPed->IsAPlayerPed() && pPed->GetWeaponManager() && !pPed->GetWeaponManager()->GetIsArmed() && 
		CGameWorld::FindLocalPlayerWanted() && CGameWorld::FindLocalPlayerWanted()->GetWantedLevel() > WANTED_CLEAN && 
		 pPed->GetVehiclePedInside() && pPed->GetVehiclePedInside() == CGameWorld::FindLocalPlayerVehicle() )
	{
		fScore *= REDUCE_PLAYER_PASSENGERS_SCALE;
	}

	// Less likely to attack peds travelling in vehicles
	if( pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && pPed->GetMyVehicle() )
	{
		fScore *= IN_VEHICLE_WEIGHTING;
	}

	// Adjust the weighting if the target is injured or dying
	if( pPed->IsInjured() )
	{
		fScore *= INJURED_PED_WEIGHTING;
	}
	else if( pPed->IsFatallyInjured())
	{
		fScore *= DYING_PED_WEIGHTING;
	}

	// If you're friendly with the player and the ped is targeting the player, increase the score by a factor
	if( pHostileTarget &&
		pHostileTarget->GetIsTypePed() &&
		((CPed*) pHostileTarget )->IsPlayer() &&
		pTargettingPed->GetPedIntelligence()->IsFriendlyWith( * ((CPed*) pHostileTarget ) ) )
	{
		fScore *= DEFEND_PLAYER_WEIGHTING;
	}

	//adjust weighting if we're preferring air combat
	if(pTargettingPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_PreferAirCombatWhenInAircraft))
	{
		if(pTargettingPed->GetVehiclePedInside() && pTargettingPed->GetVehiclePedInside()->GetIsAircraft())
		{
			if(!pPed->GetVehiclePedInside() || !pPed->GetVehiclePedInside()->GetIsAircraft())
			{
				fScore *= AIR_COMBAT_TARGET_WEIGHTING;
			}
		}
	}

	if(!pTargettingPed->IsAPlayerPed() && pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_ThisPedIsATargetPriorityForAI))
	{
		fScore *= SCRIPT_THREAT_WEIGHTING;
	}

	// Maximum available score to lock the result between 0 and 1
	const static float sfMaxScore = BASIC_THREAT_WEIGHTING * PLAYER_THREAT_WEIGHTING * DEFEND_PLAYER_WEIGHTING * SCRIPT_THREAT_WEIGHTING;

	return fScore / sfMaxScore;
}


//-------------------------------------------------------------------------
// Default score calculation function
//-------------------------------------------------------------------------
float CPedTargetting::EasyKillTargetScoring( CPed* pTargettingPed, const CEntity* pEntity )
{
	Assert( pEntity );
	Assert( pEntity->GetIsTypePed() );
	CPed* pPed = (CPed*) pEntity;

	float fScore = BASIC_THREAT_WEIGHTING;

	// Dead peds aren't a target at all
	if( pPed->IsDead() )
	{
		return 0.0f;
	}

	fScore = DefaultTargetScoring( pTargettingPed, pEntity );

	// Peds are weighted inversly proportional to their health,
	// so that the weakest return the highest targetting score
	const CHealthConfigInfo* pHealthInfo = pPed->GetPedHealthInfo();
	Assert( pHealthInfo );
	Assert( pHealthInfo->GetDefaultHealth() );
	fScore *= ( rage::Max( pHealthInfo->GetDefaultHealth() - pPed->GetHealth(), 0.0f ) / pHealthInfo->GetDefaultHealth() );

	return fScore;
}

#define COVERING_FIRE_TARGET_WEIGHTING (10.0f)

//-------------------------------------------------------------------------
// Default score calculation function
//-------------------------------------------------------------------------
float CPedTargetting::CoveringFireTargetScoring( CPed* pTargettingPed, const CEntity* pEntity )
{
	Assert( pEntity );
	Assert( pEntity->GetIsTypePed() );
	float fScore = DefaultTargetScoring( pTargettingPed, pEntity );

	CEntity* pScoringEntity = pTargettingPed->GetPedIntelligence()->GetTargetting()->m_pScoringEntity;

	CPed* pPed = (CPed*) pEntity;
	// If you're friendly with the player and the ped is targetting hte player,
	// increase the score by a factor
	if( pScoringEntity &&
		//		pPed->GetPedIntelligence()->GetCurrentTarget() == ( CEntity* ) pScoringEntity )
			pPed->GetPedIntelligence()->GetQueriableInterface()->GetHostileTarget() == ( CEntity* ) pScoringEntity )

	{
		fScore *= COVERING_FIRE_TARGET_WEIGHTING;
	}

	return fScore;
}


//-------------------------------------------------------------------------
// Calculates an appropriate targetting score for this entity
//-------------------------------------------------------------------------
float CPedTargetting::GetTargettingScore( const CEntity* pEntity ) const
{
	Assert( m_pScoringFn );
	return m_pScoringFn( m_pPed, pEntity );
}


//-------------------------------------------------------------------------
// Checks to see if this entity is considered friendly
//-------------------------------------------------------------------------
bool CPedTargetting::IsFriendlyEntity( const CEntity* pEntity ) const
{
	Assert( pEntity );

	// Ignore non-peds
	if( !pEntity->GetIsTypePed() )
	{
		return false;
	}

	// Ignore this ped
	if( pEntity == (CEntity*) m_pPed )
	{
		return false;
	}

	// Dead peds are not considered friendly
	if( ((CPed*)pEntity)->IsDead() )
	{
		return false;
	}

	return m_pPed->GetPedIntelligence()->IsFriendlyWith(*(CPed*)pEntity);
}


//-------------------------------------------------------------------------
// Returns true if this friendly entity is targetting the given entity
//-------------------------------------------------------------------------
bool CPedTargetting::IsEntityTargettingEntity( const CEntity* pFriendlyEntity, const CEntity* pEntity ) const
{
	Assert( pEntity->GetIsTypePed() );

//	return ((CPed*)pFriendlyEntity)->GetPedIntelligence()->GetCurrentTarget() == pEntity;
	return ((CPed*)pFriendlyEntity)->GetPedIntelligence()->GetQueriableInterface()->GetHostileTarget() == pEntity;
}


//-------------------------------------------------------------------------
// Returns the targetting reduction caused by a friendly entity already targetting
// this entity
//-------------------------------------------------------------------------
float CPedTargetting::GetFriendlyTargettingScoreReduction( const CEntity* ASSERT_ONLY(pFriendlyEntity), const CEntity* ASSERT_ONLY(pEntity ) ) const
{
	Assert( pEntity->GetIsTypePed() );
	//Assert( IsFriendlyEntity( pFriendlyEntity ) );
	Assert( IsEntityTargettingEntity( pFriendlyEntity, pEntity ) );

	return FRIENDLY_TARGETTING_REDUCTION;
}

//-------------------------------------------------------------------------
// returns a pointer to the currently targetted entity
//-------------------------------------------------------------------------
CPed* CPedTargetting::GetBestTarget( void )
{
	SetInUseAndWakeUp();

	return (CPed*)GetCurrentTarget();
}

//-------------------------------------------------------------------------
// returns a pointer to the second best target
//-------------------------------------------------------------------------
CPed* CPedTargetting::GetSecondBestTarget( void )
{
	SetInUseAndWakeUp();

	return (CPed*)GetSecondaryTarget();
}


void CPedTargetting::OnTargetDeleteFromArray(const int iTargetIndex)
{
	const int numTargets = m_iNumActiveTargets;
	const int targetIndexToCopy = numTargets - 1;

	Assert(targetIndexToCopy >= 0 && targetIndexToCopy < MAX_NUM_TARGETS);
	Assert(iTargetIndex >= 0 && iTargetIndex < MAX_NUM_TARGETS);

	if(targetIndexToCopy != iTargetIndex)
	{
		m_apVehiclesTargetsLastSeenIn[iTargetIndex] = m_apVehiclesTargetsLastSeenIn[targetIndexToCopy];
	}
	m_apVehiclesTargetsLastSeenIn[targetIndexToCopy] = NULL;
}

//-------------------------------------------------------------------------
// Called each time the given target is reset
//-------------------------------------------------------------------------
void CPedTargetting::OnTargetReset (const int iTargetIndex )
{
	// Remove the associated vehicle
	Assert(iTargetIndex >= 0 && iTargetIndex < MAX_NUM_TARGETS);
	Assert(iTargetIndex < m_iNumActiveTargets);
	if( m_apVehiclesTargetsLastSeenIn[iTargetIndex] )
	{
		m_apVehiclesTargetsLastSeenIn[iTargetIndex] = NULL;
	}			
}


//-------------------------------------------------------------------------
//Updates the targets after an LOS update
//-------------------------------------------------------------------------
void CPedTargetting::UpdatePostLos( LosStatus aPreviousLosStatus[MAX_NUM_TARGETS] )
{
	CPed* pTarget = NULL;
	const int numTargets = m_iNumActiveTargets;
	for(int i = 0; i < numTargets; i++)
	{
		if( m_aTargets[i].IsValid() )
		{
			//MP ONLY: If the los status is unchecked don't process this any further as it will give false positives, but continue to next m_aTarget evaluation
			if (NetworkInterface::IsGameInProgress() && m_aTargets[i].m_losStatus == Los_unchecked)
				continue;

			pTarget = (CPed*) m_aTargets[i].m_pEntity.Get();
			Vector3 vTargetPosition = VEC3V_TO_VECTOR3(pTarget->GetTransform().GetPosition());

#if DEBUG_DRAW
			if( CPedTargetting::DebugTargettingLos )
			{
				Color32 colour( 0x00, 0x00, 0x00, 0xff );
				switch( m_aTargets[i].m_losStatus )
				{
				case Los_blocked:
					colour = Color32( 0xff, 0x00, 0x00, 0xff );
					break;
				case Los_clear:
					colour = Color32( 0x00, 0xff, 0x00, 0xff );
					break;
				case Los_blockedByFriendly:
					colour = Color32( 0x00, 0x00, 0xff, 0xff );
					break;
				default:
					break;
				}
				grcDebugDraw::Line( VEC3V_TO_VECTOR3(m_pPed->GetTransform().GetPosition()) + Vector3(0.0f, 0.0f, 0.3f), vTargetPosition, colour );
			}
#endif

			bool bReportedPoliceSpottingPlayer = false;

			CPed* pPlayerToReportSpotted = NULL;
			Vector3 vPositionToReportSpotted(VEC3_ZERO);
			if(pTarget->IsPlayer())
			{
				pPlayerToReportSpotted = pTarget;
				vPositionToReportSpotted = vTargetPosition;
			}
			else if(!pTarget->IsPlayer() && pTarget->GetIsInVehicle() && pTarget->GetVehiclePedInside()->ContainsLocalPlayer())
			{
				pPlayerToReportSpotted = CGameWorld::FindLocalPlayer();
				if(pPlayerToReportSpotted)
				{
					vPositionToReportSpotted = VEC3V_TO_VECTOR3(pPlayerToReportSpotted->GetTransform().GetPosition());
				}
			}

			// If this ped will not lose it's targets, set hte timer to max
			// and set the last position as the current position
			if( m_pPed->GetPedIntelligence()->GetCombatBehaviour().GetTargetLossResponse() == CCombatData::TLR_NeverLoseTarget )
			{
				wantedDebugf3("CPedTargetting::UpdatePosLos TLR_NeverLoseTarget -- SetLastPositionSeen / set m_iLastSeenExpirationTimeMs");
				m_aTargets[i].m_iLastSeenExpirationTimeMs = fwTimer::GetTimeInMilliseconds() + ms_Tunables.m_iTargetNotSeenIgnoreTimeMs;
				m_aTargets[i].SetLastPositionSeen(vTargetPosition);
				m_aTargets[i].PeriodicallyCalcHeadingAndVehicle();
				
				// Only alert to local player, CNetObjPed::CreateCloneCommunication deals with each local player being spotted by cloned ped
				if( m_aTargets[i].GetLosStatus() != Los_blocked && pPlayerToReportSpotted && m_pPed->ShouldBehaveLikeLaw() )
				{
					// OBBE: The cop can see the player here, called during the targeting update every few frames:
					CWanted* pPlayerWanted = pPlayerToReportSpotted->GetPlayerWanted();
					wantedDebugf3("CPedTargetting::UpdatePostLos m_aTargets[i].GetLosStatus[%d] invoke ReportPoliceSpottingPlayer A",m_aTargets[i].GetLosStatus());
					pPlayerWanted->ReportPoliceSpottingPlayer(m_pPed, vPositionToReportSpotted, pPlayerWanted->m_WantedLevel, true, true);
					bReportedPoliceSpottingPlayer = true;
				}
			}
			else
			{
				if( m_aTargets[i].GetLosStatus() != Los_blocked && m_aTargets[i].GetLosStatus() != Los_unchecked )
				{
					// If not in a vehicle 
					// OR in the same vehicle
					// OR in an unknown vehicle but was visible last frame
					// OR in an unknown vehicle but close, allow the LOS
					// AND this is a cop
					if(	m_pPed->PopTypeIsMission() ||
						(( aPreviousLosStatus[i] != Los_blocked &&
						aPreviousLosStatus[i] != Los_unchecked) ||
						( ( !m_pPed->IsLawEnforcementPed() ||
						!CPedGeometryAnalyser::IsPedInUnknownCar(*pTarget) ||
						CPedGeometryAnalyser::CanPedSeePedInUnknownCar(*m_pPed, *pTarget) ) &&
						(!CPedGeometryAnalyser::IsPedInBush(*pTarget) ||
						CPedGeometryAnalyser::CanPedSeePedInBush(*m_pPed, *pTarget) ) ) ) )
					{
#if !__NO_OUTPUT
#if __BANK
						wantedDebugf3("CPedTargetting::UpdatePostLos m_pPed[%p]->GetDebugName[%s] GetLosStatus[%d] != Los_blocked -- SetLastPositionSeen / set m_iLastSeenExpirationTimeMs",m_pPed,m_pPed->GetDebugName(),m_aTargets[i].GetLosStatus());
#else
						wantedDebugf3("CPedTargetting::UpdatePostLos m_pPed[%p] GetLosStatus[%d] != Los_blocked -- SetLastPositionSeen / set m_iLastSeenExpirationTimeMs",m_pPed,m_aTargets[i].GetLosStatus());
#endif
#endif
						m_aTargets[i].m_iLastSeenExpirationTimeMs = fwTimer::GetTimeInMilliseconds() + ms_Tunables.m_iTargetNotSeenIgnoreTimeMs;
						m_aTargets[i].SetLastPositionSeen(vTargetPosition);
						m_aTargets[i].PeriodicallyCalcHeadingAndVehicle();

						// If the player was previously in an unknown vehicle and a cop can see him, he isn't any more!
						// Only alert to local player, CNetObjPed::CreateCloneCommunication deals with each local player being spotted by cloned ped
						if( pPlayerToReportSpotted && m_pPed->ShouldBehaveLikeLaw() )
						{
							// OBBE: The cop can see the player here, called during the targeting update every few frames:
							CWanted* pPlayerWanted = pPlayerToReportSpotted->GetPlayerWanted();
							wantedDebugf3("CPedTargetting::UpdatePostLos m_aTargets[i].GetLosStatus[%d] invoke ReportPoliceSpottingPlayer B",m_aTargets[i].GetLosStatus());
							pPlayerWanted->ReportPoliceSpottingPlayer(m_pPed, vPositionToReportSpotted, pPlayerWanted->m_WantedLevel, true, true);
							bReportedPoliceSpottingPlayer = true;
						}

						// When seen in a vehicle, take note of the last vehicle the target has been seen in
						if( m_apVehiclesTargetsLastSeenIn[i] != pTarget->GetMyVehicle() )
						{
							if( pTarget->GetMyVehicle() && pTarget->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) )
							{
								m_apVehiclesTargetsLastSeenIn[i] = pTarget->GetMyVehicle();
							}
							else
							{
								m_apVehiclesTargetsLastSeenIn[i] = NULL;
							}
						}
					}
					// otherwise the LOS is invalid, and the target is lost
					else
					{
						m_aTargets[i].m_iLastSeenExpirationTimeMs = 0;
						m_aTargets[i].m_losStatus = Los_blocked;
						m_aTargets[i].m_bLosBlockedNear = false;
					}
				}

				/// Update the visibility timer based on the los status
				if( m_aTargets[i].GetLosStatus() == Los_blocked || 	
					( m_aTargets[i].m_pEntity && m_aTargets[i].m_pEntity->GetIsPhysical() &&
					( ((CPhysical*)m_aTargets[i].m_pEntity.Get())->GetVelocity().Mag2() > (3.0f) ) ) )
				{
					m_aTargets[i].m_fVisibleTimer -= fwTimer::GetTimeStep();
					if( m_aTargets[i].m_fVisibleTimer < 0.0f )
					{
						m_aTargets[i].m_fVisibleTimer = 0.0f;
					}
				}
				else
				{
					m_aTargets[i].m_fVisibleTimer += fwTimer::GetTimeStep();
					if( m_aTargets[i].m_fVisibleTimer > TIME_TO_ZERO_ON_TARGET )
					{
						m_aTargets[i].m_fVisibleTimer = TIME_TO_ZERO_ON_TARGET;
					}
				}
			}

			if( m_aTargets[i].GetLosStatus() != Los_clear )
			{
				m_aTargets[i].m_fBlockedTimer += fwTimer::GetTimeStep();
			}
			else
			{
				m_aTargets[i].m_fBlockedTimer = 0.0f;
				m_aTargets[i].m_bHasEverHadClearLos = true;
			}

			//We may know where the target is, even if we can't see him.
			if( !bReportedPoliceSpottingPlayer && pTarget->IsPlayer() && m_pPed->ShouldBehaveLikeLaw() &&
				!m_pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_OnlyUpdateTargetWantedIfSeen) && m_aTargets[i].AreWhereaboutsKnown() && m_aTargets[i].m_bHasEverHadClearLos)
			{
				CWanted* pPlayerWanted = pTarget->GetPlayerWanted();
#if !__NO_OUTPUT
#if __BANK
				wantedDebugf3("CPedTargetting::UpdatePostLos m_pPed[%p]->GetDebugName[%s] IsNetworkClone[%d] m_aTargets[i].GetLosStatus[%d] invoke ReportPoliceSpottingPlayer C",m_pPed,m_pPed->GetDebugName(),m_pPed->IsNetworkClone(),m_aTargets[i].GetLosStatus());
#else
				wantedDebugf3("CPedTargetting::UpdatePostLos m_pPed[%p] IsNetworkClone[%d] m_aTargets[i].GetLosStatus[%d] invoke ReportPoliceSpottingPlayer C",m_pPed,m_pPed->IsNetworkClone(),m_aTargets[i].GetLosStatus());
#endif
#endif
				pPlayerWanted->ReportPoliceSpottingPlayer(m_pPed, m_aTargets[i].GetLastPositionSeen(), pPlayerWanted->m_WantedLevel, false, true);
			}
		}
		else
		{
			// Remove the associated vehicle
			if( m_apVehiclesTargetsLastSeenIn[i] )
			{
				m_apVehiclesTargetsLastSeenIn[i] = NULL;
			}		
		}
	}
}


//-------------------------------------------------------------------------
// Registers a fresh target, updates the activity timer if it already exists
//-------------------------------------------------------------------------
void CPedTargetting::RegisterThreat( const CEntity* pEntity, bool bTargetSeen, bool bForceInsert, bool bWasDirectlyHostile )
{
#if !__NO_OUTPUT
#if __BANK
	wantedDebugf3("CPedTargetting::RegisterThreat m_pPed[%p] debugname[%s] bTargetSeen[%d] bForceInsert[%d] bWasDirectlyHostile[%d]",m_pPed,m_pPed->GetDebugName(),bTargetSeen,bForceInsert,bWasDirectlyHostile);
#else
	wantedDebugf3("CPedTargetting::RegisterThreat m_pPed[%p] bTargetSeen[%d] bForceInsert[%d] bWasDirectlyHostile[%d]",m_pPed,bTargetSeen,bForceInsert,bWasDirectlyHostile);
#endif
#endif

	CPed* pTargetPed = (CPed*) pEntity;

	Assertf( pTargetPed != m_pPed, "CPedTargetting::RegisterThreat Ped registering themselves as a target!");

	// Don't register friendly peds.
	if( m_pPed->GetPedIntelligence()->IsFriendlyWith(*pTargetPed) )
	{
		Assertf(!m_pPed->GetPedIntelligence()->IsFriendlyWith(*pTargetPed), "RegisterThreat called with friendly ped, ped in group (%s) is attacking ped in group (%s)", 
			m_pPed->GetPedIntelligence()->GetRelationshipGroup()->GetName().GetCStr(), 
			pTargetPed->GetPedIntelligence()->GetRelationshipGroup()->GetName().GetCStr() );

#if __DEV
		const CRelationshipGroup* pRelGroup = m_pPed->GetPedIntelligence()->GetRelationshipGroup();
		const int otherRelationshipGroupIndex = pTargetPed->GetPedIntelligence()->GetRelationshipGroupIndex();
		if(pRelGroup && pTargetPed->GetPedIntelligence()->GetRelationshipGroup())
		{
			Assertf(!pRelGroup->CheckRelationship(ACQUAINTANCE_TYPE_PED_RESPECT, otherRelationshipGroupIndex), "Ped %s called RegisterThreat, RESPECTS target %s",
					m_pPed->GetBaseModelInfo()->GetModelName(), pTargetPed->GetBaseModelInfo()->GetModelName());

			Assertf(!pRelGroup->CheckRelationship(ACQUAINTANCE_TYPE_PED_LIKE, otherRelationshipGroupIndex), "Ped %s called RegisterThreat LIKES target %s",
					m_pPed->GetBaseModelInfo()->GetModelName(), pTargetPed->GetBaseModelInfo()->GetModelName());
		}

		CPedGroup* pMyGroup = m_pPed->GetPedsGroup();
		Assertf(!pMyGroup || pMyGroup != pTargetPed->GetPedsGroup(), "Ped %s called RegisterThreat and is in the same ped group as target %s",
				m_pPed->GetBaseModelInfo()->GetModelName(), pTargetPed->GetBaseModelInfo()->GetModelName()); 

		if(m_pPed->GetPedType() == PEDTYPE_MEDIC || m_pPed->GetPedType() == PEDTYPE_FIRE || m_pPed->GetPedType() == PEDTYPE_COP)
		{
			Assertf(m_pPed->GetPedType() != pTargetPed->GetPedType(), "Ped %s called RegisterThreat, has some ped type (%d) as target %s",
					m_pPed->GetBaseModelInfo()->GetModelName(), m_pPed->GetPedType(), pTargetPed->GetBaseModelInfo()->GetModelName()); 
		}
#endif

		return;
	}

	CSingleTargetInfo* pTargetInfo = FindTarget( pEntity );

	// Is the target already present?
	if( pTargetInfo )
	{
		// Revalidate the existing target
		pTargetInfo->Revalidate();

		// If the target is seen, reset the timer
		if( bTargetSeen )
		{
			pTargetInfo->m_iLastSeenExpirationTimeMs = fwTimer::GetTimeInMilliseconds() + ms_Tunables.m_iTargetNotSeenIgnoreTimeMs;
			pTargetInfo->SetLastPositionSeen(VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition()));

			// I guess we can afford counting down the counter by one and possibly do it if it's about time:
			pTargetInfo->PeriodicallyCalcHeadingAndVehicle();
		}
	}
	else
	{
		// Add it is a fresh target
		AddTarget( pEntity, bForceInsert );
	}

	// If the target has actually been seen, remember the vehicle last seen in.
	if( bTargetSeen || bWasDirectlyHostile )
	{
		const int numTargets = m_iNumActiveTargets;
		for(int i = 0; i < numTargets; i++)
		{
			if( m_aTargets[i].m_pEntity == pEntity )
			{
				if(bTargetSeen)
				{
					// remember the current vehicle the ped has been seen in
					if( pTargetPed->GetMyVehicle() && pTargetPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) )
					{
						m_apVehiclesTargetsLastSeenIn[i] = pTargetPed->GetMyVehicle();
					}
					else
					{
						m_apVehiclesTargetsLastSeenIn[i] = NULL;
					}
				}

				if(bWasDirectlyHostile)
				{
					m_aTargets[i].m_iTimeLastHostileActionMs = fwTimer::GetTimeInMilliseconds();
				}
			}
		}
	}
}


//-------------------------------------------------------------------------
// Returns the vehicle the target was last seen in
//-------------------------------------------------------------------------
CVehicle* CPedTargetting::GetVehicleTargetLastSeenIn( CEntity* pTarget )
{
	const int numTargets = m_iNumActiveTargets;
	Assert(numTargets <= NELEM(m_apVehiclesTargetsLastSeenIn));
	for(int i = 0; i < numTargets; i++)
	{
		if( m_aTargets[i].m_pEntity == pTarget )
		{
			return m_apVehiclesTargetsLastSeenIn[i];
		}
	}
	return NULL;
}


//-------------------------------------------------------------------------
// Find the next stored target in the direction specified.
//-------------------------------------------------------------------------
const CEntity* CPedTargetting::FindNextTarget( CEntity* pCurrentTarget, const bool bWithLos, const bool bCheckLeft, const float fCurrentHeading, const float fHeadingLimit )
{
	float fOrientationToCurrentTarget = 0.0f;
	float fCurrentClosest = -999999.0f;
	const CEntity* pNewTarget = NULL;
	Assert(m_pPed);
	const Vector3 vPedPosition = VEC3V_TO_VECTOR3(m_pPed->GetTransform().GetPosition());
	if( pCurrentTarget )
	{
		const Vector3 vCurrentTargetPosition = VEC3V_TO_VECTOR3(pCurrentTarget->GetTransform().GetPosition());
		fOrientationToCurrentTarget = fwAngle::GetATanOfXY( (vCurrentTargetPosition.x - vPedPosition.x), (vCurrentTargetPosition.y - vPedPosition.y) );
	}
	else
	{
		Vector3 vecPedForward(VEC3V_TO_VECTOR3(m_pPed->GetTransform().GetB()));
		fOrientationToCurrentTarget = fwAngle::GetATanOfXY(vecPedForward.x, vecPedForward.y );
	}

	// Loop through all the targets and try to find one in hte direction given
	const int numTargets = m_iNumActiveTargets;
	for(int i = 0; i < numTargets; i++)
	{
		if( m_aTargets[i].m_pEntity && pCurrentTarget != m_aTargets[i].m_pEntity )
		{
			const CEntity* pTarget = m_aTargets[i].m_pEntity;

			// Check that there is a clear LOS if one is required.
			if( bWithLos && m_aTargets[i].GetLosStatus() == Los_blocked )
			{
				continue;
			}

			Vector3 vTargetPosition = VEC3V_TO_VECTOR3(pTarget->GetTransform().GetPosition());
			const float fOrientationToPossibleTarget = fwAngle::GetATanOfXY( (vTargetPosition.x - vPedPosition.x), (vTargetPosition.y - vPedPosition.y) );

			// Check the ped is in the range from the current heading
			float fOrientationDiff = fOrientationToPossibleTarget - fCurrentHeading;
			fOrientationDiff = fwAngle::LimitRadianAngle(fOrientationDiff);

			if( ABS(fOrientationDiff) > fHeadingLimit )
			{
				continue;
			}

			fOrientationDiff = fOrientationToPossibleTarget - fOrientationToCurrentTarget;
			fOrientationDiff = fwAngle::LimitRadianAngle(fOrientationDiff);

			float fScore = 0.0f;
			if (bCheckLeft)
			{
				if (fOrientationDiff <= 0.0f)
				{
					fScore = -ABS(fOrientationDiff);
				}
				else
				{
					fScore = -1000.0f + ABS(fOrientationDiff);
				}
			}
			else
			{
				if (fOrientationDiff >= 0.0f)
				{
					fScore = -ABS(fOrientationDiff);
				}
				else
				{
					fScore = -1000.0f + ABS(fOrientationDiff);
				}
			}
			if (fScore > fCurrentClosest)
			{
				pNewTarget = pTarget;
				fCurrentClosest = fScore;
			}	
		}
	}
	return pNewTarget;
}

float CPedTargetting::GetExistingTargetReduction() const
{
	return ms_Tunables.m_fExistingTargetScoreWeight;
}

bool CPedTargetting::IsFriendlyWithAnyTarget()
{
	// Loop through all the targets and try to find one in hte direction given
	const int numTargets = m_iNumActiveTargets;
	for(int i = 0; i < numTargets; i++)
	{
		if( m_aTargets[i].m_pEntity && m_aTargets[i].m_pEntity->GetIsTypePed() )
		{
			const CPed* pTarget = static_cast<const CPed*>(m_aTargets[i].m_pEntity.Get());
			if( m_pPed->GetPedIntelligence()->IsFriendlyWith(*pTarget) )
				return true;
		}
	}
	return false;
}
