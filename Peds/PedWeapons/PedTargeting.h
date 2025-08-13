// Title	:	Targetting.h
// Author	:	Phil Hooker
// Started	:	10/08/05

#ifndef TARGETTING_H
#define TARGETTING_H

// Rage headers
#include "Vector/Vector3.h"

// Framework headers
#include "fwtl/pool.h"

// Game headers
#include "ai/ExpensiveProcess.h"
#include "scene/RegdRefTypes.h"
#include "Task/System/Tuning.h"

class CPedTargetting; // Forward definition to allow CPedTargetting to be declared as a friend class to CSingleTargetInfo
class CEntity;
class CPed;
class CEntityScanner;
class CExpensiveProcess;
class CVehicle;

//-------------------------------------------------------------------------
// LOS status enum
//-------------------------------------------------------------------------
typedef enum
{
	Los_unchecked = 0,
	Los_clear,
	Los_blocked,
	Los_blockedByFriendly
} LosStatus;

//-------------------------------------------------------------------------
// Information stored about a single examined target
//-------------------------------------------------------------------------
class CSingleTargetInfo
{
public:
	friend class CPedTargetting;
	
	CSingleTargetInfo();
	~CSingleTargetInfo();

	// Resets the target information
	void		Reset			( void );
	bool		IsValid			( void ) const;
	void		SetTarget		( const CEntity* pEntity );
	void		Revalidate		( void );
	void		CalculateHeading( void );
	void		CalculateInVehicle( void );
	inline void	PeriodicallyCalcHeadingAndVehicle();
	LosStatus	GetLosStatus	( void ) const {return m_losStatus;}
	float&		GetDistance		( void ) {return m_vLastPositionSeenAndDistance.w;}
	void		SetDistance(const float fDistance) { m_vLastPositionSeenAndDistance.w = fDistance; }
	void		ResetLastSeenTimer ( void ) { m_iLastSeenExpirationTimeMs = 0; }
	bool		GetLosBlockedNear() const { return m_bLosBlockedNear; }
	void		SetLosBlockedNear(bool val) { m_bLosBlockedNear = val; }
	bool		GetNeverScored() const { return m_bNeverScored; }
	void		SetNeverScored(bool val) { m_bNeverScored = val; }
	bool		GetIsBeingTargeted() const { return m_bIsBeingTargeted; }
	void		SetIsBeingTargeted(bool val) { m_bIsBeingTargeted = val; }
	bool		GetHasEverHadClearLos() const { return m_bHasEverHadClearLos; }
	Vector3		GetLastPositionSeen() const { return m_vLastPositionSeenAndDistance.GetVector3(); }
	void		SetLastPositionSeen(const Vector3& vPos) { m_vLastPositionSeenAndDistance.SetVector3(vPos); }
	CPed*		GetBlockedByFriendlyPed() const { return m_pBlockedByFriendlyPed; }
	bool		GetLastSeenInVehicle() const { return m_bLastSeenInVehicle; }
	bool		AreWhereaboutsKnown() const;
	// float	GetTimeSinceLastHostileAction() const { return m_fTimeSinceLastHostileAction; }
	float		GetTotalScore() const { return m_fTotalScore; }
	float		GetLastSeenTimer() const { return 0.001f*(float)GetTimeUntilLastSeenTimerExpiresMs(); }	// Note: this is somewhat slow due to int->float conversion, consider using GetTimeUntilLastSeenTimerExpiresMs() instead.
	unsigned int GetTimeUntilLastSeenTimerExpiresMs() const;
	bool		HasLastSeenTimerExpired() const { return GetTimeUntilLastSeenTimerExpiresMs() == 0; }
	float		GetTimeLosBlocked() const { return m_fBlockedTimer; }
	u32			GetTimeOfLastHostileAction() const { return m_iTimeLastHostileActionMs; }

private:

	Vector4			m_vLastPositionSeenAndDistance;// The last position this target was seen
	float			m_fLastDirectionSeenMovingIn; // The last direction in which the target was seen moving
	RegdConstEnt	m_pEntity;
	RegdPed			m_pBlockedByFriendlyPed;
	float			m_fTotalScore;

	// PURPOSE:	Set each time this target is identified as valid, this is a time stamp in ms
	//			for when the target should be removed.
	u32				m_iTimeExpiringMs;

	// PURPOSE:	Set each time a LOS is detected between the owner of the targetting and
	//			the target entity, the target is ignored if LOS is not established before
	//			the time reaches this time stamp.
	u32				m_iLastSeenExpirationTimeMs;

	LosStatus		m_losStatus : 3;
	bool			m_bLosBlockedNear : 1;
	bool			m_bNeverScored : 1;
	bool			m_bLastSeenInVehicle : 1;
	bool			m_bLosWasDoneAsync : 1;
	bool			m_bIsBeingTargeted : 1;
	bool			m_bHasEverHadClearLos : 1;

	// PURPOSE:	Keeps track of how many more calls to PeriodicallyCalcHeadingAndVehicle() will
	//			be required until it calls CalculateHeading() and CalculateInVehicle().
	u8				m_iCalcHeadingAndVehicleCountdown;

    float			m_fVisibleTimer;	// How long the target has been visible, used to affect the accuracy (trailing bullets behind the target)
	float			m_fBlockedTimer;

	// PURPOSE:	This is a time stamp in ms for when a hostile action happened from this target,
	//			or 0 if no such event has occurred
	u32				m_iTimeLastHostileActionMs;

	// PURPOSE:	Specifies how often PeriodicallyCalcHeadingAndVehicle() will call CalculateHeading()
	//			and CalculateInVehicle(), in frames.
	static const int kCalcHeadingAndVehiclePeriodFrames = 8;
};

/*
PURPOSE
	Call CalculateHeading() and CalculateInVehicle() if enough frames have
	passed since this was last done.
*/
void CSingleTargetInfo::PeriodicallyCalcHeadingAndVehicle()
{
	unsigned int cnt = m_iCalcHeadingAndVehicleCountdown;
	if(cnt == 0)
	{
		CalculateHeading();
		CalculateInVehicle();

		// Note: CalculateHeading() will set up m_iCalcHeadingAndVehicleCountdown
		// for the next waiting period.
	}
	else
	{
		cnt--;
		m_iCalcHeadingAndVehicleCountdown = (u8)cnt;
	}
}

// Match the number of peds for now, although significant optimisations are
// available to reduce teh number of systems required for a standard game
// e.g. Not required for retreating from specific ped
//		Or if decision maker states that only 1 target should be attacked.
//		Cheap targetting systems for guys 
#define MAX_NO_TARGETTING_SYSTEMS  (128)

//-------------------------------------------------------------------------
// Ped targetting class, to be used by peds targetting other peds.
//-------------------------------------------------------------------------
class CPedTargetting : public CExpensiveProcess
{
	// For debugging using the widget
	friend class CPedDebugVisualiserMenu;
	friend class CSingleTargetInfo;

protected:

	enum {MAX_NUM_TARGETS = 8};

public:

	struct Tunables : public CTuning
	{
		Tunables();

		// Const static variables used to define scoring alterations in different circumstances
		float m_fExistingTargetScoreWeight;
		// The time after no activity the targetting will become inactive
		float m_fTargetingInactiveDisableTime;
		// Floating point alteration to scoring due to blocked LOS
		float m_fBlockedLosWeighting;
		// Time we are allowed to ignore blocked los weighting
		float m_fTimeToIgnoreBlockedLosWeighting;
		// Multiplier for a close player threat or a relatively close player that is aiming at us
		float m_fPlayerHighThreatWeighting;
		// Give a penalty for targets in aircraft if our ped has been told to prefer other targets
		float m_fTargetInAircraftWeighting;
		// The time after which if the target has not been seen it is from then on ignored, in ms
		int m_iTargetNotSeenIgnoreTimeMs;

		// The distance which a non directly hostile player target needs to be within to get the high threat boost
		float m_fPlayerThreatDistance;
		// The distance which a directly hostile player target needs to be within to get the high threat boost
		float m_fPlayerDirectThreatDistance;
		// Need some fuzzy logic so the player can move around a bit and continue being the target if already the target
		float m_fPlayerBeingTargetedExtraDistance;
		// The time that a hostile event needs to be within to have a target be considered as directly hostile, in ms
		int m_iPlayerDirectThreatTimeMs;

		PAR_PARSABLE;
	};

	// Constructor/destructor
	CPedTargetting( CPed* pPed );
	~CPedTargetting();

	FW_REGISTER_CLASS_POOL(CPedTargetting);
	// Called from the event system, adds a threat entity to the system
	// This entity will remain until counted out.
	void RegisterThreat( const CEntity* pEntity, bool bTargetSeen = true, bool bForceInsert = false, bool bWasDirectlyHostile = false );

	// Function to remove a target from our list
	void RemoveThreat( const CEntity* pTarget );

	// Get the current number of targets in the target array (it's possible that some of these may actually be invalid)
	int GetNumActiveTargets() const { return m_iNumActiveTargets; }

	// Gets the target at a given index
	const CEntity* GetTargetAtIndex( int iTargetIndex );
	// Gets the target info at a given index
	const CSingleTargetInfo* GetTargetInfoAtIndex( int iTargetIndex ) const;
	// Gets the currently assigned target
	const CEntity* GetCurrentTarget( void );
	// Gets the second best target
	const CEntity* GetSecondaryTarget( void );

	// Gets the LOS status of the given entity
	LosStatus	GetLosStatus( const CEntity* pEntity, bool bForceImmediateUpdate = true );
	// Gets the LOS status of the given entity
	LosStatus	GetLosStatusAtIndex( int iTargetIndex, bool bForceImmediateUpdate = true );
	// Gets the distance to the given entity
	bool		GetDistance	( const CEntity* pEntity, float &fDistance );
	// Gets the position of the entity.
	bool		GetTargetLastSeenPos( const CEntity* pEntity, Vector3& vOutPos );
	// Gets the last known heading of the target
	bool		GetTargetDirectionLastSeenMovingIn( const CEntity* pEntity, float& fOutDirection );
	// Gets if the target was last seen in a vehicle
	bool		GetTargetLastSeenInVehicle( const CEntity* pEntity, bool& bOutInVehicle );
	// returns whether the target has been out of sight for enough time so that the ped no longer knows it's position
	bool		AreTargetsWhereaboutsKnown( Vector3* pvBestPos = NULL, const CEntity* pTarget = NULL, float* pfLastSeenTimer = NULL, bool* pbHasTargetInfo = NULL);

	// Set the targeting in use and wake up if needed
	void SetInUseAndWakeUp( void );

	// Calculates the best current target
	const CEntity* GetBestTarget( const CEntityScanner& entityScanner, const Vector3& vSourcePos, const float fSourceHeading );

	// Calculates the best current target
	void SetSpatialTargettingLimits( float fMaximumRange, float fAngularLimit ) { m_fMaxTargetRange = fMaximumRange; m_fDotAngularLimit = fAngularLimit; }

	// Resets the current target list, removing all targets
	void ResetTargetting( void );

	// Locks the targetting to only consider this entity
	void LockTarget( CEntity* pLockedEntity );
	// Unlocks the targetting making it free to target any entity
	void UnlockTarget( void );

	// Returns the target info for the ped passed if already present
	const CSingleTargetInfo* FindTarget		( const CEntity* pEntity ) const;
	CSingleTargetInfo* FindTarget		( const CEntity* pEntity );

	// Returns the target info for the ped passed if already present
	s32 FindTargetIndex	( const CEntity* pEntity );

	// Sets the targetting as currently active, being used by some external system
	void SetInUse();

	// Sets the position that we want to do distance checks against (for choosing targets based on a position that isn't our own)
	void SetAlternatePosition(Vector3& vPosition) { m_vAlternatePosition = vPosition; }

	// Sets whether or not we should use the alternate position
	void SetUseAlternatePosition(bool bShouldUseAltPosition) { m_bUseAlternatePosition = bShouldUseAltPosition; }

	// Gets the best target
	CPed* GetBestTarget( void );
	CPed* GetSecondBestTarget( void );
	bool UpdateTargetting( bool bDoFullUpdate );

	// Scoring fuction definition
	typedef float (TargetScoringFunction)( CPed* pTargettingPed, const CEntity* pTargetEntity );

	// Allows a component using the targetting system to override the default target scoring function
	// with the one passed. If a time is passed > 0, the default function will revert after that time
	void SetScoringFunction( TargetScoringFunction* pScoringFunction, CEntity* pScoringEntity = NULL, float fTime = -1.0f );
	TargetScoringFunction* GetScoringFunction( void ) { return m_pScoringFn; }
	void ResetScoringFunction( void );

	// Returns the vehicle this target was last seen in
	CVehicle* GetVehicleTargetLastSeenIn( CEntity* pTarget );
	// Find the next stored target in the direction specified.
	const CEntity* FindNextTarget( CEntity* pCurrentTarget, const bool bWithLos, const bool bCheckLeft, const float fCurrentHeading, const float fHeadingLimit );

	// Friendly with any target
	bool IsFriendlyWithAnyTarget();

	u32 GetMaxNumTargets() { return MAX_NUM_TARGETS; }
	float GetActiveTimer() { return m_fActivityTimer; }

	// Scoring functions
	static float	DefaultTargetScoring				( CPed* pTargettingPed, const CEntity* pEntity );
	static float	EasyKillTargetScoring				( CPed* pTargettingPed, const CEntity* pEntity );
	static float	CoveringFireTargetScoring			( CPed* pTargettingPed, const CEntity* pEntity );
	static float	AirCombatTargetScoring				( CPed* pTargettingPed, const CEntity* pEntity );

	static const Tunables& GetTunables() { return ms_Tunables; }

#if !__FINAL
	static bool DebugTargetting;
	static bool DebugTargettingLos;
	static bool DebugAcquaintanceScanner;
#endif

protected:

	// Main update function to update the scores of all targets
	bool UpdateTargettingInternal( const CEntityScanner& entityScanner, const Vector3& pvSourcePos, const float fSourceHeading, bool bUpdateActivityTimer, bool bDoFullUpdate );

	// Adds a single target
	bool AddTarget( const CEntity* pEntity, bool bForceInsert = false );

	// Delete an element from the target array
	void DeleteTargetFromArrays(int targetIndex);

	// Updates each target, reducing their validity timers
	void UpdateTargets( void );

	// Returns a reduction to the targeting score based on other friendlies targeting this entity
	float CalculateFriendlyTargettingReduction( const CEntityScanner& entityScanner, const Vector3& vPos, const CEntity* pEntity );

	// Calculates a spatial score for each target
	float CalculateSpatialTargettingScore( float& fOutDistance, const Vector3& vPos, const float fHeading, CSingleTargetInfo* pTargetInfo );

	// Calculates a score for each target
	void ScoreTargets( const CEntityScanner& entityScanner, const Vector3& vPos, const float fHeading, const bool bProcessAll );

	// Calculates and stores the indices of the best and secondary targets
	void CalculateBestTargets( void );

	// Sets the LOS status of all currently valid targets
	void SetAllLosStatus( LosStatus losStatus );

	// Function called each time a target is reset
	void ResetTarget( const s32 iTargetIndex );

	// Function called when an element is about to be deleted from the array, being
	// overwritten by the last element in the array.
	void OnTargetDeleteFromArray(int iTargetIndex);

	// Function called each time a target is reset
	void OnTargetReset (const int iTargetIndex );

	// Custom post LOS update function
	void	UpdatePostLos( LosStatus aPreviousLosStatus[MAX_NUM_TARGETS] );
private:
	// Returns if the ped is allowed to be a target of ours
	bool			AllowedToTargetPed( const CPed* pTarget, bool bIsBeingTargeted ) const;

	// PURPOSE: Test to see if any friendly ped is blocking the line between us and the target, and if so, return it.
	CPed*			ProbeForFriendlyFireBlockingPed(CPed* pTargetPed, Vec3V_In pedPositionV, Vec3V_In targetPedPositionV) const;

	// Returns if the target should be removed, note this is different to the the opposite of hte above function
	// to allow temporarily registered threats to persist unlesss dead or completely invalid targets.
	bool	TargetShouldBeRemoved					( const CSingleTargetInfo* pTarget ) const;

	// returns the targetting score of the passed entity
	float	GetTargettingScore					( const CEntity* pEntity ) const;

	// calculate a reduction for the existing target
	float GetExistingTargetReduction() const;;

	// Friendly query functions, is the entity considered friendly, is it targetting the
	// same entity, if so what amount should the targetting score be reduced by?
	bool	IsFriendlyEntity					( const CEntity* pEntity ) const;
	bool	IsEntityTargettingEntity	( const CEntity* pFriendlyEntity, const CEntity* pEntity ) const;
	float	GetFriendlyTargettingScoreReduction	( const CEntity* pFriendlyEntity, const CEntity* pEntity ) const;

	// Ped Targetting specific implementation of the los status updates
	void	UpdateSingleLos( int iIndex, bool bForceImmediateUpdate );
	void	UpdateLosStatus( void );

	// The currently set scoring function
	TargetScoringFunction*	m_pScoringFn;
	float					m_fScoringOverrideTimer;
	// An entity used to weight the scoring depending on the scoring function
	CEntity*				m_pScoringEntity;
	CPed*					m_pPed;

	// If probes have been set up asynchronously, then the next frame when this is called they will be collated
	void ProcessAsyncResults();

	// An array of vehicles recording the vehicles the target
	// was last seen in
	RegdVeh			m_apVehiclesTargetsLastSeenIn[MAX_NUM_TARGETS];

	// Const static variables used to define scoring alterations in different circumstances
	static float BASIC_THREAT_WEIGHTING;
	static float ACTIVE_THREAT_WEIGHTING;
	static float FLEEING_THREAT_WEIGHTING;
	static float DIRECT_THREAT_WEIGHTING;
	static float IN_VEHICLE_WEIGHTING;
	static float UNARMED_DIRECT_THREAT_WEIGHTING;
	static float PLAYER_THREAT_WEIGHTING;
	static float DEFEND_PLAYER_WEIGHTING;
	static float WEAPON_PISTOL_WEIGHTING;
	static float WEAPON_2HANDED_WEIGHTING;
	static float FRIENDLY_TARGETTING_REDUCTION;
	static float SCRIPT_THREAT_WEIGHTING;
	static float DECOY_PED_SCORING_MULTIPLIER;

private:

	// Prevent accidental and unwanted copying.
	CPedTargetting( const CPedTargetting &rhs );

	// Monitors the activity of the targetting system.
	// If inactive for a period of time, the targetting system switches to idle
	
	float	m_fActivityTimer;
	s16	m_iCurrentTargetIndex;
	s16 m_iSecondBestTargetIndex;

	// The next Los target to have Los checked
	s16		m_iNextLosIndex;

	// PURPOSE:	The current number of elements of m_aTargets[] that are in use.
	// NOTES:	Some of these elements may actually have been invalidated.
	s16		m_iNumActiveTargets;

	// The alternate position that will be used for distance testing (if instructed to do so).
	Vector3 m_vAlternatePosition;

	// Information used in spatial scoring
	float	m_fMaxTargetRange;
	float	m_fDotAngularLimit;

	// If targetting is locked to a specific entity, that is stored here
	// NULL if no lock
	RegdConstEnt	m_pLockedTarget;

	// If the targetting is running in cheap mode, it does no LOS checks and isn't registered with the expensive process distributer
	bool	m_bRunningInCheapMode;

	// The flag that determines if we should use the alternate position when checking distances
	bool	m_bUseAlternatePosition;
	bool	m_bActive;

	// PURPOSE:	In UpdateTargets(), this is the index of the next target we will check, if possible.
	u8		m_iNextTargetForUpdateTargetsToCheck;

	// PURPOSE:	Array of targets, of which m_iNumActiveTargets are in use.
	// NOTES:	Located at the end of the object for cache reasons, since we often only have
	//			one target or so, at the beginning of the array, it's better for that to be
	//			closer to the other member variables in memory.
	CSingleTargetInfo m_aTargets[MAX_NUM_TARGETS];

	// Our tunables
	static Tunables		 ms_Tunables;
};

//-------------------------------------------------------------------------
// Reset the target information
//-------------------------------------------------------------------------
inline bool CSingleTargetInfo::IsValid( void ) const
{
	return m_pEntity != NULL;
}

#endif // TARGETTING_H
