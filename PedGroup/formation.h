// Title	:	formation.h
// Author	:	Steven G.
// Started	:	8/12/03

#ifndef FORMATION_H
#define FORMATION_H

// needed for max num of members in group
#define MAX_FORMATION_DESTINATIONS 		CPointList::DISTANTLIGHTS_MAX_POINTS

// rage includes
#include "data/bitbuffer.h"
#include "fwnet/netserialisers.h"
#include "script/thread.h"
#include "vector/vector3.h"
#include "grcore\debugdraw.h"

class CPhysical;
class CPedTaskPair;
class CCoverScanner;
class CPedGroup;
class CPed;

// This class contains a temporary list of coordinates

class CPointList
{
public:
	enum { MAX_NUM_POINTS = 24 };

	s32			m_nPoints;
	Vector3		m_aPoints[MAX_NUM_POINTS];
	bool		m_aPointHasBeenClaimed[MAX_NUM_POINTS];

	CPointList() { Empty(); }
	~CPointList() {}

	void			Empty();
	void			AddPoint(const Vector3& Point);
	void			MergeListsRemovingDoubles(CPointList *pMainList, CPointList *pToBeMergedList);
#if !__FINAL
	void			PrintDebug();
#endif

};



//******************************************************************************
//	CPedFormationTypes - contains enumerated formation types
//******************************************************************************
class CPedFormationTypes
{
public:
	enum
	{
		FORMATION_LOOSE,
		FORMATION_SURROUND_FACING_INWARDS,
		FORMATION_SURROUND_FACING_AHEAD,
		FORMATION_LINE_ABREAST,
		FORMATION_ARROWHEAD,
		FORMATION_V,
		FORMATION_FOLLOW_IN_LINE,
		FORMATION_SINGLE,
		FORMATION_IN_PAIR,
		NUM_FORMATION_TYPES
	};
};

class CScriptablePedFormationTypes
{
public:
	enum
	{
		SCRIPT_FORMATION_LOOSE,
		SCRIPT_FORMATION_SURROUND_FACING_INWARDS,
		SCRIPT_FORMATION_SURROUND_FACING_AHEAD,
		SCRIPT_FORMATION_LINE_ABREAST,
		SCRIPT_FORMATION_FOLLOW_IN_LINE,

		SCRIPT_FORMATION_UNKNOWN
	};
};

int MapPedFormationToScriptableFormation(int iPedFormationType);
int MapScriptableFormationToPedFormation(int iScriptableFormationType);


//******************************************************************************



//******************************************************************************
//	CPedPositioning - a simple class to store a ped's position in formation.
//******************************************************************************

#define PEDPOSFLAG_POSITION_IS_VALID		0x01

class CPedPositioning
{
public:
	CPedPositioning() { m_iFlags = 0; }
	~CPedPositioning() { }

	inline u32 GetFlags() { return m_iFlags; }
	inline void SetFlags(u32 f) { m_iFlags = f; }

	inline Vector3 GetPosition(void) const { return Vector3(m_fPosX, m_fPosY, m_fPosZ); }
	inline float GetHeading() const { return m_fHeading; }
	inline float GetTargetRadius() const { return m_fTargetRadius; }

	inline void SetPosition(Vector3 & vPos) { m_fPosX = vPos.x; m_fPosY = vPos.y; m_fPosZ = vPos.z; }
	inline void SetHeading(float fHeading) { m_fHeading = fHeading; }
	inline void SetTargetRadius(float fTargetRadius) { m_fTargetRadius = fTargetRadius; }

private:
	float m_fPosX, m_fPosY, m_fPosZ;
	float m_fHeading;
	float m_fTargetRadius;
	u32 m_iFlags;
};

//******************************************************************************
//	CPedFormation - this is the base class from which all ped formations are
//	derived.  Each CPedGroup has a CFormation pointer, allocated from a pool.
//******************************************************************************
#define SIZE_LARGEST_PEDFORMATION_CLASS		128

class CPedFormation
{
public:
	enum
	{
		MAX_FORMATION_SIZE = 8
	};

public:
	CPedFormation() : m_pPedGroup(0), m_iScriptModified(THREAD_INVALID) { }
	virtual ~CPedFormation() { }

	// Allocator.  Use this instead of 'new' - eventually this class will have a POOL & allocator
	static CPedFormation * NewFormation(s32 iType);

	inline s32 GetFormationType() const { return m_eFormationType; }

	virtual bool Init(CPedGroup * pPedGroup = NULL) { m_pPedGroup = pPedGroup; return true; }

	bool Process();

	inline const CPedPositioning & GetPedPositioning(s32 iPedIndex) const
	{
#if !__PPU	// Dont'cha just love the ps3?
		Assertf(iPedIndex >= 0 && iPedIndex < MAX_FORMATION_SIZE, "iPedIndex:%d, index is out of range for CPedGroupMembership", iPedIndex);
#endif
		const CPedPositioning & pedPositioning = m_PedPositions[iPedIndex];
		return pedPositioning;
	}

	virtual void SetDefaultSpacing() = 0;

	inline scrThreadId GetScriptModified() const { return m_iScriptModified; }
	inline void ResetScriptModified() { m_iScriptModified = THREAD_INVALID; }

	inline float GetSpacing() const { return m_fSpacing; }
	inline bool GetPositionsRotateWithLeader() const { return m_bPositionsRotateWithLeader; }
	inline bool GetFaceTowardsLeader() const { return m_bFaceTowardsLeader; }
	inline bool GetFaceAwayFromLeader() const { return m_bFaceAwayFromLeader; }
	inline bool GetFaceSameWayAsLeader() const { return m_bFaceSameWayAsLeader; }
	inline bool GetCrowdRoundLeaderWhenStationary() const { return m_bCrowdRoundLeaderWhenStationary; }
	inline bool GetAddLeadersVelocityOntoGotoTarget() const { return m_bAddLeadersVelocityOntoGotoTarget; }
	inline bool GetWalkAlongsideLeaderWhenClose() const { return m_bWalkAlongsideLeaderWhenClose; }

	// Override the formation spacings
	void SetSpacing(const float fSpacing, const float fMinDist=-1.0f, const float fMaxDist=-1.0f, const scrThreadId iScriptId=THREAD_INVALID);

	inline void SetPositionsRotateWithLeader(const bool b) { m_bPositionsRotateWithLeader = b; }

	// Setting any of these 3 facing states below, resets the other two
	inline void SetFaceTowardsLeader(const bool b) { m_bFaceTowardsLeader = b; m_bFaceAwayFromLeader = !b; m_bFaceSameWayAsLeader = !b; }
	inline void SetFaceAwayFromLeader(const bool b) { m_bFaceAwayFromLeader = b; m_bFaceTowardsLeader = !b;  m_bFaceSameWayAsLeader = !b; }
	inline void SetFaceSameWayAsLeader(const bool b) { m_bFaceSameWayAsLeader = b; m_bFaceTowardsLeader = !b; m_bFaceAwayFromLeader = !b; }

	inline void SetCrowdRoundLeaderWhenStationary(const bool b) { m_bCrowdRoundLeaderWhenStationary = b; }
	inline void SetAddLeadersVelocityOntoGotoTarget(const bool b) { m_bAddLeadersVelocityOntoGotoTarget = b; }
	inline void SetWalkAlongsideLeaderWhenClose(const bool b) { m_bWalkAlongsideLeaderWhenClose = b; }

	inline void SetAdjustSpeedMinDist(const float f) { m_fAdjustSpeedMinDist = f; }
	inline void SetAdjustSpeedMaxDist(const float f) { m_fAdjustSpeedMaxDist = f; }
	inline float GetAdjustSpeedMinDist() const { return m_fAdjustSpeedMinDist; }
	inline float GetAdjustSpeedMaxDist() const { return m_fAdjustSpeedMaxDist; }

	virtual bool IsInFormation(const CPed* UNUSED_PARAM(pPed)) const { return true; }

	virtual float GetMBRAccelForDistance(float UNUSED_PARAM(fDistance), bool UNUSED_PARAM(fDistance)) const { return 1.0f; }

	static dev_float ms_fMinSpacing;

#if !__FINAL
	static bool ms_bDebugPedFormations;
	static const char * sz_PedFormationTypes[CPedFormationTypes::NUM_FORMATION_TYPES];
	inline const char * GetFormationTypeName() { return GetFormationTypeName(m_eFormationType); }
	inline static const char * GetFormationTypeName(s32 iType) { Assert(iType >= 0 && iType < CPedFormationTypes::NUM_FORMATION_TYPES); return sz_PedFormationTypes[iType]; }
#if DEBUG_DRAW
	virtual void Debug(CPedGroup * pPedGroup);
#endif // DEBUG_DRAW
#endif // FINAL

	void Serialise(CSyncDataBase& serialiser)
	{
		SERIALISE_UNSIGNED(serialiser, m_eFormationType, SIZEOF_FORMATION_TYPE, "Formation type");
		SERIALISE_PACKED_FLOAT(serialiser, m_fSpacing, 10.0f, SIZEOF_FORMATION_SPACING, "Formation spacing");
	}

protected:

	virtual bool CalculatePositions(CPedGroup * pPedGroup) = 0;

	// The ped group using this formation
	CPedGroup* m_pPedGroup;

	scrThreadId m_iScriptModified;

	// The type of the formation
	s32 m_eFormationType;

	// The spacing between peds
	float m_fSpacing;

	float m_fAdjustSpeedMinDist;
	float m_fAdjustSpeedMaxDist;

	// Whether the formation's points rotate along with the leader's heading
	u32 m_bPositionsRotateWithLeader			:1;
	u32 m_bFaceTowardsLeader					:1;
	u32 m_bFaceAwayFromLeader				:1;
	u32 m_bFaceSameWayAsLeader				:1;
	u32 m_bCrowdRoundLeaderWhenStationary	:1;
	u32 m_bAddLeadersVelocityOntoGotoTarget	:1;
	u32 m_bWalkAlongsideLeaderWhenClose		:1;

	// The calculated positions.  Using an array ought to be better than having CPedGroupMembership::MAX_NUM_MEMBERS for all.
	CPedPositioning m_PedPositions[MAX_FORMATION_SIZE];

	static const float ms_fDefaultTargetRadius;

	static unsigned const SIZEOF_FORMATION_TYPE		= datBitsNeeded<CPedFormationTypes::NUM_FORMATION_TYPES>::COUNT;
	static unsigned const SIZEOF_FORMATION_SPACING	= 9;
};

//******************************************************************************
//	CPedFormation_Loose - Members seek the leader with a target radius equal
//	to the m_fSpacing member.
//******************************************************************************
class CPedFormation_Loose : public CPedFormation
{
public:
	CPedFormation_Loose();
	virtual ~CPedFormation_Loose() { }

	virtual void SetDefaultSpacing();

	static float GetAdaptiveMBRAccel(float fDistance, float fMinAdjustSpeedDistance, float fMaxAdjustSpeedDistance);

	static dev_float ms_fPerMemberSpacing;

protected:


	virtual float GetMBRAccelForDistance(float fDistance, bool bUseAdaptiveMbr) const;

private:
	virtual bool CalculatePositions(CPedGroup * pPedGroup);
};

//******************************************************************************
//	CPedFormation_SurroundFacingInwards - Members are spaced evenly around leader,
//	facing inwards.  The formation doesn't rotate with the leader.
//******************************************************************************
class CPedFormation_SurroundFacingInwards : public CPedFormation
{
public:
	CPedFormation_SurroundFacingInwards();
	virtual ~CPedFormation_SurroundFacingInwards() { }

	virtual void SetDefaultSpacing();

private:

	virtual bool CalculatePositions(CPedGroup * pPedGroup);
};

//******************************************************************************
//	CPedFormation_SurroundFacingAhead - Members are spaced evenly around leader,
//	facing the same direction as the leader.  The formation doesn't rotate with
//	the leader.
//******************************************************************************
class CPedFormation_SurroundFacingAhead : public CPedFormation
{
public:
	CPedFormation_SurroundFacingAhead();
	virtual ~CPedFormation_SurroundFacingAhead() { }

	virtual void SetDefaultSpacing();

private:

	virtual bool CalculatePositions(CPedGroup * pPedGroup);
};

//******************************************************************************
//	CPedFormation_LineAbreast - a line extending either side of the leader,
//	this formation rotates with the leader & members face in his direction.
//******************************************************************************
class CPedFormation_LineAbreast : public CPedFormation
{
public:
	CPedFormation_LineAbreast();
	virtual ~CPedFormation_LineAbreast() { }
	
	virtual void SetDefaultSpacing();

private:

	virtual bool CalculatePositions(CPedGroup * pPedGroup);
};


//******************************************************************************
//	CPedFormation_Arrowhead - an arrow head, with the leader at the tip.
//	This formation rotates with the leader & all members take his heading
//******************************************************************************
class CPedFormation_Arrowhead : public CPedFormation
{
public:
	CPedFormation_Arrowhead();
	virtual ~CPedFormation_Arrowhead() { }

	virtual void SetDefaultSpacing();

private:

	virtual bool CalculatePositions(CPedGroup * pPedGroup);
};

//******************************************************************************
//	CPedFormation_V - a v-shaped formation with the leader in the centre at
//	the front.
//******************************************************************************
class CPedFormation_V : public CPedFormation
{
public:
	CPedFormation_V();
	virtual ~CPedFormation_V() { }

	virtual void SetDefaultSpacing();

private:

	virtual bool CalculatePositions(CPedGroup * pPedGroup);
};

//******************************************************************************
//	CPedFormation_FollowInLine - stores a list of positions where the leader
//	has been, which are used to allocate positions to group members.
//******************************************************************************
class CPedFormation_FollowInLine : public CPedFormation
{
public:
	CPedFormation_FollowInLine();
	virtual ~CPedFormation_FollowInLine() { }

	virtual void SetDefaultSpacing();

#if __DEV
	virtual void Debug(CPedGroup * pPedGroup);
#endif

	bool HasEnoughPositionsForFollowers() const;

	void ResetPositionHistory() { m_bResetPositionHistory = true; }

private:

	virtual bool CalculatePositions(CPedGroup * pPedGroup);

	bool m_bResetPositionHistory;
	s32 m_iLastNumMembersExclLeader;
	Vector3 m_vLeaderLastPosition;

	//**************************************************************************************
	// This array is filled as the leader moves.  We can't actually use the m_PedPositions
	// array here because ped groups can have gaps in them (ie. members are not shuffled
	// along when one is removed).
	//**************************************************************************************
	Vector3 m_vPositionHistory[MAX_FORMATION_SIZE];
	float m_fHeadingHistory[MAX_FORMATION_SIZE];
	float m_fTargetRadiusHistory[MAX_FORMATION_SIZE];
	float m_fRandomSpacing[MAX_FORMATION_SIZE];
};	

//******************************************************************************
//	CPedFormation_Single - Test Formation
//******************************************************************************
class CPedFormation_Single : public CPedFormation
{
public:
	CPedFormation_Single();
	virtual ~CPedFormation_Single() { }

	virtual void SetDefaultSpacing();

#if __DEV
	virtual void Debug(CPedGroup * pPedGroup);
#endif

private:

	virtual bool CalculatePositions(CPedGroup * pPedGroup);
};	

//******************************************************************************
//	CPedFormation_Pair - Formation For Two Peds To Stay Side By Side
//******************************************************************************
class CPedFormation_Pair : public CPedFormation
{
public:
	CPedFormation_Pair();
	virtual ~CPedFormation_Pair() { }

	virtual void SetDefaultSpacing();

	// Leader state
	enum
	{
		LeaderStateInvalid = -1,
		LeaderMovingToCover = 0,
		LeaderWaitingForFollowerAtCover,
		LeaderMovingToPosition,
		LeaderIdle
	};

	s32	GetLeaderState() const { return m_iLeaderState; }
	void	SetLeaderState(s32 iLeaderState) { m_iLeaderState = iLeaderState; m_bLeaderStateAcknowledged = false; }

	bool	GetLeaderStateAcknowledged() const { return m_bLeaderStateAcknowledged; }
	void	SetLeaderStateAcknowledged(bool bLeaderStateAcknowledged) { m_bLeaderStateAcknowledged = bLeaderStateAcknowledged; }

#if __DEV
	virtual void Debug(CPedGroup * pPedGroup);
#endif

private:

	virtual bool CalculatePositions(CPedGroup * pPedGroup);

	s32 m_iLeaderState;
	bool  m_bLeaderStateAcknowledged;
};	


#endif // FORMATION_H
