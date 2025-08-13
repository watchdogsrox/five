// Title	:	PedGroup.h
// Author	:	Neil F.
// Started	:	29/06/03

#ifndef PED_GROUP_H
#define PED_GROUP_H

//#include "General.h"
#include "network/General/NetworkUtil.h"
#include "pedgroup/formation.h"
#include "peds/pedtype.h"
#include "peds/peddefines.h"
#include "AI/AITarget.h"
#include "script/thread.h"
#include "scene/DynamicEntity.h"
#include "scene/RegdRefTypes.h"
#include "network/NetworkInterface.h"

// rage headers
#include "script/thread.h"
#include "vector/vector3.h"

// framework headers
#include "fwutil/Flags.h"

#include <vector>

class CEvent;
class CPed;
class CPedFormation;
class CVehicle;
class CTask;
class CGameScriptHandler;

//#define DEBUG_PLAYER_BUDDIES_NOT_GETTING_IN_CAR


///////////////////
//Membership
///////////////////

class CPedGroupMembership
{
public:

	class CPedGroupMember
	{
	public:

		CPedGroupMember() 
		{
			ClearPedGroupMember();
		};

		void ClearPedGroupMember();

		bool IsAssigned() const
		{
			return (m_pPedGroupMember || m_pedNetworkId != NETWORK_INVALID_OBJECT_ID);
		};

		void SetPedGroupMember(CPed *pPed) { m_pPedGroupMember = pPed; }
		CPed *GetPedGroupMember() const { return m_pPedGroupMember.Get(); }

		ObjectId m_pedNetworkId;  // m_pPed may be NULL in a network game as the ped does not exist yet

	private:
		RegdPed m_pPedGroupMember;
	};

public:

	static const float ms_fMaxSeparation;
	static const float ms_fPlayerGroupMaxSeparation;
	
	friend class CPedGroupIntelligence;
	
	enum
	{
		MAX_NUM_MEMBERS=CPedFormation::MAX_FORMATION_SIZE
	};
	
	enum
	{
		LEADER = MAX_NUM_MEMBERS-1
	};

	CPedGroupMembership();
	CPedGroupMembership(const CPedGroupMembership& src);
	~CPedGroupMembership();
	
	CPedGroupMembership& operator=(const CPedGroupMembership& src);
		
	void SetPedGroup(class CPedGroup* pPedGroup) {m_pPedGroup=pPedGroup;}

	void Flush(bool bDirty = true);
	void RemoveAllFollowers(bool bLeaveMissionPeds = false);
	void RemoveNFollowers(s32 N);
	
	float GetMaxSeparation() { return m_fMaxSeparation; }
	void SetMaxSeparation(float fNewSeparationRange, bool bDirty = true);

	ObjectId GetInitialLeaderNetId() const { return m_initialLeaderNetId; }

	void SetLeader(CPed* pLeader, bool bNetCall = false);
	void AddFollower(CPed* pFollower);
	void RemoveMember(CPed* pMember, bool bDirty = true);
	
	bool HasLeader() const;	
	bool LeaderHasChanged() const;	
	CPed* GetLeader() const;	
	ObjectId GetLeaderNetId() const;
	bool HasMember(const s32 i) const;
	CPed* GetMember(const s32 i) const;
	s32 GetMemberId(const CPed* pMember) const;
	CPed * GetNthMember(const s32 i) const;
	// The lower the number the more important the member
	s32 GetNonLeaderMemberPriority(const CPed* pMember) const;
	
	bool IsLeader(const CPed* pLeader) const;
	bool IsFollower(const CPed* pFollower) const;
	bool IsMember(const CPed* pMember) const;
	
	s32 CountMembers() const;
	s32 CountMembersExcludingLeader() const;
	CPed* GetFurthestMemberFromPosition(Vector3& vPos, bool bIncludeLeader) const;
	void SortByDistanceFromLeader();

	void Process();
	
	void AppointNewLeader();

	void RemoveMembers(const CPedGroupMembership& members);

//	static s32 GetObjectForPedToHold();
	
	void CopyNetworkInfo(const CPedGroupMembership& membership, bool bApplyToPeds);

	void Serialise(CSyncDataBase& serialiser);

private:

	static const unsigned SIZEOF_MAX_SEPARATION = 10;

	class CPedGroup* m_pPedGroup;

	CPedGroupMember m_members[MAX_NUM_MEMBERS];
	bool m_bOrderAcknowledgements[MAX_NUM_MEMBERS];

	float m_fMaxSeparation;
	
	ObjectId m_initialLeaderNetId; // only used in MP

	void AddMember(CPed* pPed, const s32 iSlot, bool bSetDirty = true, bool bAddDefaultTask = true);
	void RemoveMember(const s32 iSlot, bool bSetDirty = true);
	
	void From(const CPedGroupMembership& src);

	void AssignNetworkPedIds();
};

//////////////
//Ped group
//////////////

#define PEDGROUP_INDEX_NONE		255

class CPedGroup
{

	friend class CPedGroups;
	
public:

	// networking consts
	static unsigned const SIZEOF_POPTYPE			= datBitsNeeded<NUM_POPTYPES-1>::COUNT;

public:

	enum { MAX_NUM_SUBGROUPS = 2 };
	
	CPedGroup();
	~CPedGroup();

	static CPedFormation * CreateDefaultFormation();
	inline CPedFormation * GetFormation() const { return m_pFormation; }
	// Includes subgroup members
	static s32	FindTotalNumberOfMembersInGroup(CPedGroup* pGroup);

	void SetFormation(s32 iType);
	void SetFormation(CPedFormation * pFormation);

	void Process();
	
	void Flush(bool bDirty = true, bool bNetAssert = true);
	void RemoveAllFollowers();
	
	bool IsLocallyControlled() const;
	bool IsPlayerGroup() const;

	void Teleport(const Vector3& vNewPos, bool bKeepTasks=false, bool bKeepIK=false, bool bResetPlants=true);
	
	inline CPedGroupMembership* GetGroupMembership() { return &m_membership; }
	inline const CPedGroupMembership* GetGroupMembership() const { return &m_membership; }

	inline u8 GetGroupIndex() const { return m_iGroupIndex; }
	inline void SetGroupIndex(u8 index) { m_iGroupIndex = index; }

	CGameScriptHandler*	GetScriptHandler() const { return m_pScriptHandler; }

	bool IsActive() const { return m_bActive; }

	inline ePopType	PopTypeGet() const { return m_pedGroupPopType; }
	inline bool		PopTypeIsRandom() const { return IsPopTypeRandom(m_pedGroupPopType); }
	inline bool		PopTypeIsRandomNonPermanent() const { return IsPopTypeRandomNonPermanent(m_pedGroupPopType); }
	inline bool		PopTypeIsMission() const { return IsPopTypeMission(m_pedGroupPopType); }

	CPed* GetClosestGroupPed(CPed* pPed, float* retDistSqr=NULL);
	
	void SetDirty(); // flag the group as dirty (i.e. as being altered). Used by network code.

	bool IsAnyoneUsingCar(const CVehicle& vehicle) const;

	float FindDistanceToFurthestMember();
	float FindDistanceToNearestMember(CPed **ppNearestPed = NULL) const;

	void GiveEventToAllMembers(CEvent & event, CPed * pExcludeThisMember);

	bool GetNeedsGroupEventScan() const { return m_bNeedsGroupEventScan; }
	void SetNeedsGroupEventScan(bool b) { m_bNeedsGroupEventScan = b; }

	// NETWORK METHODS:
	bool IsSyncedWithPlayer(const CNetGamePlayer& player) const;
	bool IsSyncedWithAllPlayers() const;

	u32 GetNetArrayHandlerIndex() const; // returns the index for this group in the network ped group array handler

	// copies network info from the given ped group. If bToValidGroup is set then this group is an existing group in the ped groups array.
	void CopyNetworkInfo(CPedGroup& pedGroup, bool bToValidGroup)
	{
		m_membership.CopyNetworkInfo(*pedGroup.GetGroupMembership(), bToValidGroup);

		if (m_iGroupIndex >= MAX_NUM_PHYSICAL_PLAYERS)
		{
			m_pedGroupPopType = pedGroup.PopTypeGet();
		}

		// we need to copy this to determine whether the group is a player group when serialising
		if (!bToValidGroup) m_iGroupIndex = pedGroup.m_iGroupIndex;

		m_bNeedsGroupEventScan = pedGroup.m_bNeedsGroupEventScan;

		m_bActive = true;
	}

	void Serialise(CSyncDataBase& serialiser);

	netObject* GetNetworkObject() const;

	CPed* m_lastPedRespondedTo;
	u16	m_scriptRefIndex;

private:

	// made these private to make group activation more secure
	void SetPedGroupPopType(const ePopType iGroupCreatedBy, bool bDirty = true);
	void SetActive();
	void SetInactive();

	CPedGroupMembership m_membership;

	CPedFormation * m_pFormation;

	ePopType m_pedGroupPopType;
	u8 m_iGroupIndex;

	// PURPOSE:	If true, CGroupEventScanner::Scan() will check for things like the
	//			leader entering a vehicle or drawing a weapon, and send related events to
	//			the group members. If this is not needed or desired, groups can disable that
	//			part of the behavior by setting this to false.
	bool m_bNeedsGroupEventScan;

	bool m_bCanDirtyDuringFlush;

	u16 m_bActive						:1;

	CGameScriptHandler*	m_pScriptHandler;	// the handler of the script that created this group
};

////////////////////
//Ped groups
////////////////////

class CPedGroups
{
	friend class CPed;

public:

	static const unsigned MAX_NUM_PLAYER_GROUPS = MAX_NUM_PHYSICAL_PLAYERS;
	static const unsigned MAX_NUM_SCRIPT_GROUPS = 16;
	static const unsigned MAX_NUM_AMBIENT_GROUPS = 16;
	static const unsigned MAX_NUM_GROUPS = MAX_NUM_PLAYER_GROUPS+MAX_NUM_SCRIPT_GROUPS+MAX_NUM_AMBIENT_GROUPS; 

public:

	static s32 AddGroup(const ePopType pedGroupPopType, CGameScriptHandler* pScriptHandler = NULL);
	static void AddGroupAtIndex(const ePopType pedGroupPopType, s32 index);
	static void RemoveGroup(const s32 iGroupID, bool bDirty = true);

	static void RemoveGroupFromScript(const s32 iGroupID, bool bDirty = true);

	static bool MoveGroupToFreeSlot(const s32 iGroupID, s32* freeSlotID = NULL);
	static void RemoveAllFollowersFromGroup(const s32 iGroupID);
	static void RemoveAllRandomGroups();
	static void RemoveAllNonPermanentGroups();
	static void Process();
	static s32 CountEvents();
	static void Init();
	static void CreateDefaultGroups();
	
	static bool IsPlayerGroup(s32 iGroupID);
	static bool IsGroupLeader(CPed* pPed);
	static bool AreInSameGroup(const CPed* pPed1, const CPed* pPed2);

	static s32 GetGroupId(CPedGroup* pPedGroup);
	static bool IsInPlayersGroup(const CPed* pPed);
	static s32 GetPlayersGroup(const CPed* pPlayer);
	
	static void RegisterKillByPlayer();
	
	static void ResetAllFormationsModifiedByThisScript(const scrThreadId iThreadId);

	static void Shutdown();

	static CPedGroup ms_groups[MAX_NUM_GROUPS];

private:

	static CPedGroup* GetPedsGroup(const CPed* pPed);
    static bool       ShouldBlockRandomGroupCreation();

public:

	static void RemoveGroupInternal(const s32 iGroupID, bool bDirty);

	static bool ms_bIsPlayerOnAMission;
	static s32 ms_iNoOfPlayerKills;

};

#endif
