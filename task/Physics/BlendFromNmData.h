//
// Physics/BlendFromNmData.h
//
// Copyright (C) 1999-2012 Rockstar Games. All Rights Reserved.
//

#ifndef PHYSICS_BLENDFROMNMDATA_H
#define PHYSICS_BLENDFROMNMDATA_H


#include "atl/array.h"
#include "atl/binmap.h"
#include "atl/hashstring.h"
#include "data/base.h"

#include "ai\task\taskregdref.h"
#include "fwanimation/posematcher.h"
#include "fwmaths/random.h"
#include "fwutil/flags.h"
#include "parser/macros.h"

#include "ai/AITarget.h"

#include "peds/pedDefines.h"
#include "peds/PedMotionState.h"
#include "Peds/PlayerArcadeInformation.h"

#if __WIN32
# pragma warning(push)
# pragma warning(disable: 4099) // First seen using struct, now using class
#endif

class CNmBlendOutPoseItem;

#if __WIN32
# pragma warning(pop)
#endif


typedef atHashWithStringNotFinal eNmBlendOutSet;

extern const eNmBlendOutSet NMBS_INVALID;

extern const eNmBlendOutSet NMBS_STANDING;
extern const eNmBlendOutSet NMBS_STANDING_AIMING_CENTRE;
extern const eNmBlendOutSet NMBS_STANDING_AIMING_LEFT;
extern const eNmBlendOutSet NMBS_STANDING_AIMING_RIGHT;

extern const eNmBlendOutSet NMBS_STANDING_AIMING_CENTRE_PISTOL_HURT;
extern const eNmBlendOutSet NMBS_STANDING_AIMING_LEFT_PISTOL_HURT;
extern const eNmBlendOutSet NMBS_STANDING_AIMING_RIGHT_PISTOL_HURT;

extern const eNmBlendOutSet NMBS_STANDING_AIMING_CENTRE_RIFLE_HURT;
extern const eNmBlendOutSet NMBS_STANDING_AIMING_LEFT_RIFLE_HURT;
extern const eNmBlendOutSet NMBS_STANDING_AIMING_RIGHT_RIFLE_HURT;

extern const eNmBlendOutSet NMBS_STANDING_AIMING_CENTRE_RIFLE;
extern const eNmBlendOutSet NMBS_STANDING_AIMING_LEFT_RIFLE;
extern const eNmBlendOutSet NMBS_STANDING_AIMING_RIGHT_RIFLE;

extern const eNmBlendOutSet NMBS_INJURED_STANDING;

extern const eNmBlendOutSet NMBS_STANDARD_GETUPS;
extern const eNmBlendOutSet NMBS_STANDARD_GETUPS_FEMALE;
extern const eNmBlendOutSet NMBS_STANDARD_CLONE_GETUPS;
extern const eNmBlendOutSet NMBS_STANDARD_CLONE_GETUPS_FEMALE;
extern const eNmBlendOutSet NMBS_PLAYER_GETUPS;
extern const eNmBlendOutSet NMBS_PLAYER_MP_GETUPS;
extern const eNmBlendOutSet NMBS_PLAYER_MP_CLONE_GETUPS;
extern const eNmBlendOutSet NMBS_SLOW_GETUPS;
extern const eNmBlendOutSet NMBS_SLOW_GETUPS_FEMALE;
extern const eNmBlendOutSet NMBS_FAST_GETUPS;
extern const eNmBlendOutSet NMBS_FAST_GETUPS_FEMALE;
extern const eNmBlendOutSet NMBS_FAST_CLONE_GETUPS;
extern const eNmBlendOutSet NMBS_FAST_CLONE_GETUPS_FEMALE;
extern const eNmBlendOutSet NMBS_INJURED_GETUPS;
extern const eNmBlendOutSet NMBS_INJURED_ARMED_GETUPS;
extern const eNmBlendOutSet NMBS_INJURED_PLAYER_GETUPS;
extern const eNmBlendOutSet NMBS_INJURED_PLAYER_MP_GETUPS;
extern const eNmBlendOutSet NMBS_INJURED_PLAYER_MP_CLONE_GETUPS;

extern const eNmBlendOutSet NMBS_ACTION_MODE_GETUPS;
extern const eNmBlendOutSet NMBS_ACTION_MODE_CLONE_GETUPS;
extern const eNmBlendOutSet NMBS_PLAYER_ACTION_MODE_GETUPS;
extern const eNmBlendOutSet NMBS_PLAYER_MP_ACTION_MODE_GETUPS;
extern const eNmBlendOutSet NMBS_PLAYER_MP_ACTION_MODE_CLONE_GETUPS;

extern const eNmBlendOutSet NMBS_MELEE_STANDING;
extern const eNmBlendOutSet NMBS_MELEE_GETUPS;

extern const eNmBlendOutSet NMBS_ARMED_1HANDED_GETUPS;
extern const eNmBlendOutSet NMBS_ARMED_1HANDED_SHOOT_FROM_GROUND;
extern const eNmBlendOutSet NMBS_ARMED_2HANDED_GETUPS;
extern const eNmBlendOutSet NMBS_ARMED_2HANDED_SHOOT_FROM_GROUND;

extern const eNmBlendOutSet NMBS_CUFFED_GETUPS;
extern const eNmBlendOutSet NMBS_WRITHE;
extern const eNmBlendOutSet NMBS_DRUNK_GETUPS;

extern const eNmBlendOutSet NMBS_PANIC_FLEE;
extern const eNmBlendOutSet NMBS_PANIC_FLEE_INJURED;
extern const eNmBlendOutSet NMBS_ARREST_GETUPS;

extern const eNmBlendOutSet NMBS_FULL_ARREST_GETUPS;
#if CNC_MODE_ENABLED
extern const eNmBlendOutSet NMBS_INCAPACITATED_GETUPS;
extern const eNmBlendOutSet NMBS_INCAPACITATED_GETUPS_02;
#endif

extern const eNmBlendOutSet NMBS_UNDERWATER_GETUPS;
extern const eNmBlendOutSet NMBS_SWIMMING_GETUPS;
extern const eNmBlendOutSet NMBS_DIVING_GETUPS;

extern const eNmBlendOutSet NMBS_SKYDIVE_GETUPS;

extern const eNmBlendOutSet NMBS_DEATH_HUMAN;
extern const eNmBlendOutSet NMBS_DEATH_HUMAN_OFFSCREEN;

extern const eNmBlendOutSet NMBS_DEAD_FALL;

extern const eNmBlendOutSet NMBS_CRAWL_GETUPS;
#if FPS_MODE_SUPPORTED
extern const eNmBlendOutSet NMBS_FIRST_PERSON_GETUPS;
extern const eNmBlendOutSet NMBS_FIRST_PERSON_ACTION_MODE_GETUPS;
extern const eNmBlendOutSet NMBS_INJURED_FIRST_PERSON_GETUPS;
extern const eNmBlendOutSet NMBS_FIRST_PERSON_MP_GETUPS;
extern const eNmBlendOutSet NMBS_FIRST_PERSON_MP_ACTION_MODE_GETUPS;
extern const eNmBlendOutSet NMBS_INJURED_FIRST_PERSON_MP_GETUPS;
#endif

/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
/// enum eNmBlendOutPoseType
/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////

enum eNmBlendOutItemType {
	NMBT_NONE				= 0, 
	NMBT_SINGLECLIP			= 1, 
	NMBT_DIRECTIONALBLEND	= 2, 
	NMBT_AIMFROMGROUND		= 3,
	NMBT_FORCEMOTIONSTATE	= 4,
	NMBT_SETBLEND			= 5,
	NMBT_DIRECTBLENDOUT		= 6,
	NMBT_UPPERBODYREACTION  = 7,
	NMBT_DISALLOWBLENDOUT	= 8,
	NMBT_CRAWL				= 9,
	NMBT_FINISH				= 10,
};

///////// Special values
const int eNmBlendOutItemType_NUM_ENUMS = 9;

const int eNmBlendOutItemType_MAX_VALUE = 8;


/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
/// class CNmBlendOutItem
/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////


class CNmBlendOutItem : public ::rage::datBase
{
public:
	CNmBlendOutItem();                 // Default constructor
	virtual ~CNmBlendOutItem();        // Virtual destructor

	virtual bool IsPoseItem() { return false; }
	virtual bool ShouldAddToPointCloud() { return false; }
	virtual bool HasClipSet() { return false; }
	virtual fwMvClipSetId GetClipSet() { return fwMvClipSetId((u32)0); }
	virtual fwMvClipId GetClip() { return fwMvClipId((u32)0); }

	::rage::atHashString        m_id;
	eNmBlendOutItemType         m_type;
	::rage::atHashString        m_nextItemId;

	PAR_PARSABLE;
};

    
/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
/// class CNmBlendOutPoseItem
/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////

    
class CNmBlendOutPoseItem : public CNmBlendOutItem
{
public:
    CNmBlendOutPoseItem();                 // Default constructor
    virtual ~CNmBlendOutPoseItem();        // Virtual destructor
    
	virtual bool IsPoseItem() { return true; }
	virtual bool ShouldAddToPointCloud() { return m_addToPointCloud; }
	virtual bool HasClipSet() { return m_clipSet.GetHash()!=0; }
	virtual fwMvClipSetId GetClipSet() { return fwMvClipSetId(m_clipSet.GetHash()); }
	virtual fwMvClipId GetClip() { return fwMvClipId(m_clip.GetHash()); }

	bool HasNo180Blend() { return m_no180Blend; }
	bool IsLooping() { return m_looping; }
	float GetPlaybackRate() { return fwRandom::GetRandomNumberInRange(m_minPlaybackRate, m_maxPlaybackRate); }
	float GetDuration() { return m_duration; }

    ::rage::atHashString        m_clipSet;
    ::rage::atHashString        m_clip;
    bool                        m_addToPointCloud;
	bool                        m_no180Blend;
	bool						m_looping;
	bool						m_allowInstantBlendToAim;
    float                       m_minPlaybackRate;
    float                       m_maxPlaybackRate;
    float                       m_earlyOutPhase;
	float						m_armedAIEarlyOutPhase;
    float                       m_movementBreakOutPhase;
	float                       m_turnBreakOutPhase;
	float						m_playerAimOrFireBreakOutPhase;
	float                       m_ragdollFrameBlendDuration;
	float						m_duration;
	float						m_fullBlendHeadingInterpRate;
	float						m_zeroBlendHeadingInterpRate;
	float						m_dropDownPhase;
    PAR_PARSABLE;
};

/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
/// class CNmBlendOutMotionStateItem -	Forces the motion state on the ped
/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////


class CNmBlendOutMotionStateItem : public CNmBlendOutItem
{
public:
	CNmBlendOutMotionStateItem();                 // Default constructor
	virtual ~CNmBlendOutMotionStateItem();        // Virtual destructor

	CPedMotionStates::eMotionState   m_motionState;
	bool							 m_forceRestart;
	float							 m_motionStartPhase;

	PAR_PARSABLE;
};

/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
/// class CNmBlendOutBlendItem -	Sets blend time and syncing options between two items
///									or at the end.
/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////

class CNmBlendOutBlendItem : public CNmBlendOutItem
{
public:
	CNmBlendOutBlendItem();                 // Default constructor
	virtual ~CNmBlendOutBlendItem();        // Virtual destructor

	float                       m_blendDuration;
	bool                        m_tagSync;
	PAR_PARSABLE;
};

/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
/// class CNmBlendOutReactionItem -	Upper body reaction item
/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////

class CNmBlendOutReactionItem : public CNmBlendOutItem
{
public:
	CNmBlendOutReactionItem();                 // Default constructor
	virtual ~CNmBlendOutReactionItem();        // Virtual destructor

	virtual bool HasClipSet() { return m_clipSet.GetHash()!=0; }
	virtual fwMvClipSetId GetClipSet() { return fwMvClipSetId(m_clipSet.GetHash()); }

	atHashString	m_clipSet;
	float			m_doReactionChance;
	PAR_PARSABLE;
};

    
 
/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
/// class CNmBlendOutPoseSet
/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////

    
class CNmBlendOutSet : public ::rage::datBase
{
public:
    CNmBlendOutSet();                 // Default constructor
    virtual ~CNmBlendOutSet();        // Virtual destructor
    
	bool HasFallbackSet() { return m_fallbackSet.GetHash()!=0; }
	eNmBlendOutSet GetFallbackSet(){ return (eNmBlendOutSet)m_fallbackSet.GetHash(); }

	CNmBlendOutPoseItem* FindPoseItem(fwMvClipSetId clipSet, fwMvClipId clip);
	typedef enum 
	{
		BOSF_ForceDefaultBlendOutSet = BIT0,
		BOSF_ForceNoBlendOutSet = BIT1,
		BOSF_AllowRootSlopeFixup = BIT2,
		BOSF_AllowWhenDead = BIT3,
		BOSF_DontAbortOnFall = BIT4,
	} BlendOutSetFlags;

	bool IsFlagSet(BlendOutSetFlags flag) const
	{
		return m_ControlFlags.IsFlagSet(flag);
	}

	fwFlags32 m_ControlFlags;
    ::rage::atArray< CNmBlendOutItem*, 0, ::rage::u16>         m_items;
	atHashString m_fallbackSet;
    
    PAR_PARSABLE;
};

        
/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
/// class CNmBlendOutPoseManager
/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////

    
class CNmBlendOutSetManager : public ::rage::datBase
{

public:
    CNmBlendOutSetManager();                 // Default constructor
    virtual ~CNmBlendOutSetManager();        // Virtual destructor

	static CNmBlendOutSet* GetBlendOutSet(eNmBlendOutSet set);

	static void Load();
	static bool Append(const char *fname);
	static void Shutdown();

#if !__FINAL
	static void Save();

	static const char * GetBlendOutSetName(eNmBlendOutSet set)
	{
		atHashString setHash((u32)set);
		const char * str = setHash.TryGetCStr();
		return str ? str : "UNKNOWN HASH";
	}
#endif

#if __ASSERT
	static void ValidateBlendOutSets();
#endif // __ASSERT

	static bool RequestBlendOutSet( eNmBlendOutSet set );
	static void AddRefBlendOutSet( eNmBlendOutSet set );
	static void ReleaseBlendOutSet( eNmBlendOutSet set );
	static bool IsBlendOutSetStreamedIn( eNmBlendOutSet set );

	static CNmBlendOutItem* FindItem(eNmBlendOutSet poseSet, u32 key);

    atBinaryMap< CNmBlendOutSet *, atHashString > m_sets;

    PAR_PARSABLE;
};

class CTaskMove;


class CNmBlendOutSetList
{
public:
	CNmBlendOutSetList();
	~CNmBlendOutSetList();

	// Reset / wipe everything....
	void Reset();

	// Adds a pose set to the list
	void Add( eNmBlendOutSet set, float bias = 1.0f );

	// Adds a pose set to the list with a fallback (if the first set is not streamed in, the fallback will be used
	void AddWithFallback ( eNmBlendOutSet set, eNmBlendOutSet fallbackSet );

	CNmBlendOutPoseItem* FindPoseItem(fwMvClipSetId clipSet, fwMvClipId clip, eNmBlendOutSet& outSet);

	fwPoseMatcher::Filter& GetFilter() {return m_filter; }

	void SetTarget(const CAITarget& target) {m_Target = target; }
	CAITarget& GetTarget() {return m_Target; }

	void SetMoveTask(CTaskMove* pMoveTask) { m_pMoveTask = pMoveTask; }
	CTaskMove* RelinquishMoveTask() { 
		CTaskMove* pMoveTask = m_pMoveTask;
		m_pMoveTask = NULL;
		return pMoveTask; 
	}

	inline int GetNumBlendOutSets(void) const					{ return m_sets.GetCount(); }
	inline eNmBlendOutSet const& GetBlendOutSet(int i) const	{ Assertf(i < GetNumBlendOutSets(), "ERROR : CNmBlendOutSetList::GetBlendOutSet() : Invalid index!"); return m_sets[i]; };

#if __ASSERT
	atString DebugDump() const;
#endif // __ASSERT

private:
	atArray<eNmBlendOutSet> m_sets;
	fwPoseMatcher::Filter m_filter;
	CAITarget m_Target;
	taskRegdRef<CTaskMove> m_pMoveTask;
};

// --- Globals ------------------------------------------------------------------

extern CNmBlendOutSetManager g_NmBlendOutPoseManager;

inline CNmBlendOutSet* CNmBlendOutSetManager::GetBlendOutSet(eNmBlendOutSet set)
{
	atHashString setHash((u32)set);

	CNmBlendOutSet** ppSet = g_NmBlendOutPoseManager.m_sets.SafeGet(setHash);
	return ppSet ? *ppSet : NULL;
}

#endif
