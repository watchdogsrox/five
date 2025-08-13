// Framework headers
#include "fwscript/scriptguid.h"

// Game headers
#include "PedGroup.h"

#include "camera/CamInterface.h"
#include "control\gamelogic.h"
#include "event\EventHandler.h"
#include "event\EventGroup.h"
#include "event\Events.h"
#include "event\EventDamage.h"
#include "frontend\MiniMap.h"
#include "game\ModelIndices.h"
#include "modelinfo\VehicleModelInfo.h"
#include "network\arrays\NetworkArrayHandlerTypes.h"
#include "network\arrays\NetworkArrayMgr.h"
#include "network/players/NetGamePlayer.h"
#include "network\NetworkInterface.h"
#include "objects\Object.h"
#include "pedgroup\PedGroup.h"
#include "peds\Ped.h"
#include "peds\PedGeometryAnalyser.h"
#include "peds\PedIntelligence.h"
#include "peds\PedPlacement.h"
#include "peds\pedpopulation.h"
#include "peds/Ped.h"
#include "peds\gangs.h"
#include "physics\GtaArchetype.h"
#include "physics/WorldProbe/worldprobe.h"
#include "scene\Entity.h"
#include "script/Handlers/GameScriptEntity.h"
#include "script\Handlers\GameScriptHandler.h"
#include "script\Handlers\GameScriptHandlerNetwork.h"
#include "script\Handlers\GameScriptResources.h"
#include "script\Script.h"
#include "Task/Scenario/TaskScenario.h"
#include "Task/Default/TaskWander.h"
#include "Task\General\TaskBasic.h"
#include "Task/Vehicle/TaskCar.h"
#include "Task/Vehicle/TaskCarAccessories.h"
#include "Task/Vehicle/TaskCarUtils.h"
#include "Task\Response\TaskFlee.h"
#include "Task\Response\TaskGangs.h"
#include "Task/Movement/TaskGoto.h"
#include "Task/Scenario/Info/ScenarioInfo.h"
#include "Task/Scenario/ScenarioManager.h"
#include "Task\System\TaskManager.h"
#include "Task/Vehicle/TaskEnterVehicle.h"
#include "Task\General\TaskSecondary.h"
#include "Task/Movement/TaskSeekEntity.h"
#include "task\Combat/TaskThreatResponse.h"
#include "Task\System\TaskTypes.h"
#include "Vfx\Misc\Fire.h"

AI_OPTIMISATIONS()

////////////////
//GROUP MEMBER
////////////////

void CPedGroupMembership::CPedGroupMember::ClearPedGroupMember()
{
	if (m_pPedGroupMember)
	{
		m_pPedGroupMember->SetPedGroupIndex(PEDGROUP_INDEX_NONE);
	}

	m_pPedGroupMember = NULL;
	m_pedNetworkId = NETWORK_INVALID_OBJECT_ID;
};

////////////////
//MEMBERSHIP
////////////////

#define __OPTIMIZED_GET_PEDS_GROUP	1

const float CPedGroupMembership::ms_fMaxSeparation=30.0f;
const float CPedGroupMembership::ms_fPlayerGroupMaxSeparation=30.0f;

CPedGroupMembership::CPedGroupMembership() 
: m_initialLeaderNetId(NETWORK_INVALID_OBJECT_ID)
{
	m_fMaxSeparation = ms_fMaxSeparation;

	s32 i;
	for(i=0;i<MAX_NUM_MEMBERS;i++)
	{
		m_members[i].ClearPedGroupMember();
	}
}

CPedGroupMembership::CPedGroupMembership(const CPedGroupMembership& src) 
: m_initialLeaderNetId(NETWORK_INVALID_OBJECT_ID)
{
	From(src);
}

CPedGroupMembership::~CPedGroupMembership()
{
	Flush(false);
}

CPedGroupMembership& CPedGroupMembership::operator=(const CPedGroupMembership& src)
{
	From(src);
	return *this;
}
void CPedGroupMembership::Flush(bool bDirty)
{
	Assertf(!bDirty || m_pPedGroup->IsLocallyControlled(), "Only the machine which controls this ped group can flush it");

	s32 i;
	for(i=0;i<MAX_NUM_MEMBERS;i++)
	{
		if(m_members[i].IsAssigned())
		{
			RemoveMember(i, bDirty);
		}
	}

	m_initialLeaderNetId = NETWORK_INVALID_OBJECT_ID;
}

void CPedGroupMembership::RemoveAllFollowers(bool bLeaveMissionPeds)
{
	Assertf(m_pPedGroup->IsLocallyControlled(), "Only the machine which controls this ped group can remove all followers");

	s32 i;
	for(i=0;i<MAX_NUM_MEMBERS;i++)
	{
		if(m_members[i].IsAssigned() && (i != LEADER))
		{
			// mission peds will always exist on all machines in a network game, so this function will still work properly
			if ((!bLeaveMissionPeds) || !m_members[i].GetPedGroupMember() || !m_members[i].GetPedGroupMember()->PopTypeIsMission())
			{
				RemoveMember(i);
			}
		}
	}
}

void CPedGroupMembership::RemoveNFollowers(s32 N)
{
	Assertf(m_pPedGroup->IsLocallyControlled(), "Only the machine which controls this ped group can remove N followers");

	s32 i;
	for(i=0;i<LEADER;i++)
	{
		if(m_members[i].IsAssigned() && N > 0)
		{
			// mission peds will always exist on all machines in a network game, so this function will still work properly
			if(!m_members[i].GetPedGroupMember() || !m_members[i].GetPedGroupMember()->PopTypeIsMission())
			{
				RemoveMember(i);
				N--;
			}
		}
	}
	Assert(N == 0);
}


void CPedGroupMembership::From(const CPedGroupMembership& src)
{
	if (m_members[LEADER].IsAssigned())
	{
		Assertf(m_pPedGroup->IsLocallyControlled(), "Only the machine which controls this ped group can alter the group");
	}

	s32 i;
	for(i=0;i<MAX_NUM_MEMBERS;i++)
	{
		if (src.m_members[i].GetPedGroupMember())
			AddMember(src.m_members[i].GetPedGroupMember(),i);
		else
			m_members[i].m_pedNetworkId = src.m_members[i].m_pedNetworkId;
	}
	m_pPedGroup=src.m_pPedGroup;
	m_fMaxSeparation = src.m_fMaxSeparation;
}

void CPedGroupMembership::AssignNetworkPedIds()
{
	// see if we can assign the ped pointers in a network game
	for(s32 i=0;i<MAX_NUM_MEMBERS;i++)
	{
		if (m_members[i].IsAssigned() && !m_members[i].GetPedGroupMember())
		{
			netObject* pObject = NetworkInterface::GetNetworkObject(m_members[i].m_pedNetworkId);

			if (pObject)
			{
				CPed *pPed = NetworkUtils::GetPedFromNetworkObject(pObject);

				if (!pPed)
				{
					Assertf(0, "%s is a member of a ped group!", pObject->GetLogName());
					RemoveMember(i, false);
				}

				// is ped a member of another group? If so remove him. 
				CPedGroup* pExistingGroup = pPed? pPed->GetPedsGroup() : NULL;

				if (pExistingGroup)
				{
					pPed->GetPedsGroup()->GetGroupMembership()->RemoveMember(pPed, false);
				}

				if (pPed)
				{
					m_members[i].ClearPedGroupMember();
					bool bAddDefaultTask = false;
					
					if (!pPed->IsNetworkClone())
					{
						bAddDefaultTask = !pPed->IsSwatPed() || !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_CreatedByDispatch) || pPed->PopTypeIsMission();
					}

					AddMember(pPed, i, false, bAddDefaultTask);
				}
			}
		}
	}
}

/*
s32 CPedGroupMembership::GetObjectForPedToHold()
{
	s32 randNum = fwRandom::GetRandomNumberInRange(0, 100);
	
	if (randNum<33)
	{
		return MI_GANG_SMOKE;
	}
	else if (randNum<66)
	{
		return MI_GANG_DRINK;
	}
	else
	{
		return -1;
	}
}
*/
void CPedGroupMembership::SetMaxSeparation(float fNewSeparationRange, bool bDirty) 
{ 
	m_fMaxSeparation = fNewSeparationRange; 

	if (bDirty)
	{
		m_pPedGroup->SetDirty();
	}
}

void CPedGroupMembership::SetLeader(CPed* pLeader, bool ASSERT_ONLY(bNetCall))
{
#if __ASSERT
	Assert(pLeader);

	// check if the leader is a player ped here, because otherwise it will assert when we setup a player's group for a joining player
	Assertf(bNetCall || m_pPedGroup->IsLocallyControlled() || pLeader->IsAPlayerPed(), "Only the machine which controls this ped group can set a leader");
#endif 

	//Check if the new leader is already in the group. 
	if(IsFollower(pLeader))
	{
		//Remove the follower from the group so that he 
		//can be added later as the leader.
		RemoveMember(pLeader);
	}
	
	//Check if the group already has a leader.
	if(m_members[LEADER].IsAssigned())
	{
		if (pLeader->GetPedType() == PEDTYPE_SWAT)
		{
			CPed* pOldLeader = GetLeader();

			//Remove the old leader from the group.
			RemoveMember(LEADER);
			AddFollower(pOldLeader);
		}
		else
		{
			//Remove the old leader from the group.
			RemoveMember(LEADER);
		}
	}

	Assertf(pLeader->GetPedGroupIndex() == PEDGROUP_INDEX_NONE,
			"About to set a ped as leader of group %d, but the ped appears to already be a member of a group (%d).",
			m_pPedGroup->GetGroupIndex(), pLeader->GetPedGroupIndex());

	//Add the new leader to the group.		
	AddMember(pLeader,LEADER);

// 	if (m_pPedGroup->PopTypeIsRandom())
// 	{
// 		CWeapon* currWeapon = pLeader->GetWeaponMgr()->GetWeaponUsable();
// 		if (currWeapon->GetWeaponType() == WEAPONTYPE_UNARMED)
// 		{
// 			s32 modelIndex = GetObjectForPedToHold();
// //			Assert(modelIndex!=-1);
// 			if (modelIndex!=-1)
// 			{
// 				pLeader->GiveObjectToPedToHold(modelIndex);
// 			}
// 		}
// 	}
}

void CPedGroupMembership::AddFollower(CPed* pFollower)
{
	Assertf(!IsMember(pFollower), "Added ped to group but he is already in the group");

	Assertf(!pFollower || pFollower->GetPedGroupIndex() == PEDGROUP_INDEX_NONE,
			"About to add a ped to a group (%d), but the ped appears to already be a member of a group (%d).",
			m_pPedGroup->GetGroupIndex(), pFollower->GetPedGroupIndex());

	Assertf(m_pPedGroup->IsLocallyControlled(), "Only the machine which controls this ped group can add a follower");

	//Check that the new follower isn't already a member of the group.
	if(!IsMember(pFollower))
	{
		//The new follower isn't already in the group.
		//Find an empty slot for the new follower.
		s32 iSlot=-1;
		s32 i;
		for(i=0;i<(MAX_NUM_MEMBERS-1);i++)
		{
			if(!m_members[i].IsAssigned())
			{
				//Found an empty slot.
				iSlot=i;
				break;
			}
		}
		
		Assertf(iSlot!=-1, "Ped was added to group that was already full");
		//Check if an empty slot has been found and add the new follower.
		if(iSlot!=-1)
		{
			AddMember(pFollower,iSlot);	
		
// 			if (m_pPedGroup->PopTypeIsRandom())
// 			{
// 				CWeapon* currWeapon = pFollower->GetWeaponMgr()->GetWeaponUsable();
// 				if (currWeapon->GetWeaponType() == WEAPONTYPE_UNARMED)
// 				{
// 					s32 modelIndex = GetObjectForPedToHold();
// //					Assert(modelIndex!=-1);
// 					if (modelIndex!=-1)
// 					{
// 						pFollower->GiveObjectToPedToHold(modelIndex);
// 					}
// 				}
// 			}
		}
	}
}

void CPedGroupMembership::RemoveMember(CPed* pMember, bool bDirty)
{
	Assertf(!bDirty || m_pPedGroup->IsLocallyControlled(), "Only the machine which controls this ped group can remove a member");

	Assert(pMember);
	s32 iSlot=-1;
	s32 i;
	for(i=0;i<MAX_NUM_MEMBERS;i++)
	{
		if (m_members[i].IsAssigned())
		{
			if(pMember==m_members[i].GetPedGroupMember())
			{
				iSlot=i;
				break;
			}
		}
	}
	
	if(iSlot!=-1)
	{
		RemoveMember(iSlot, bDirty);
	}
}

bool CPedGroupMembership::HasLeader() const
{
	return m_members[LEADER].IsAssigned();
}

CPed* CPedGroupMembership::GetLeader() const
{
	return m_members[LEADER].GetPedGroupMember();
}

ObjectId CPedGroupMembership::GetLeaderNetId() const
{
	return m_members[LEADER].m_pedNetworkId;
}

bool CPedGroupMembership::LeaderHasChanged() const
{
	aiAssertf(NetworkInterface::IsGameInProgress(), "CPedGroupMembership::LeaderHasChanged() can only be called in MP");
	return m_initialLeaderNetId != NETWORK_INVALID_OBJECT_ID && m_members[LEADER].m_pedNetworkId != m_initialLeaderNetId;
}

bool CPedGroupMembership::HasMember(const s32 i) const 
{
	Assert(i>=0);
	Assert(i<MAX_NUM_MEMBERS);
	return m_members[i].IsAssigned();
}

CPed* CPedGroupMembership::GetMember(const s32 i) const 
{
	Assert(i>=0);
	Assert(i<MAX_NUM_MEMBERS);
	return m_members[i].GetPedGroupMember();
}

s32 CPedGroupMembership::GetMemberId(const CPed* pMember) const
{
	s32 i;
	for(i=0;i<LEADER;i++)
	{
		if(m_members[i].GetPedGroupMember()==pMember)
		{
			return i;
		}
	}
	return -1;
}

CPed * CPedGroupMembership::GetNthMember(const s32 iIndex) const
{
	const s32 iNumMembersExcludingLeader = CountMembersExcludingLeader();
	Assert(iIndex >= 0 && iIndex < iNumMembersExcludingLeader);
	if(iIndex < 0 || iIndex >= iNumMembersExcludingLeader)
		return NULL;

	s32 iCount=0;
	for(s32 i=0;i<LEADER;i++)
	{
		if(m_members[i].GetPedGroupMember())
		{
			if(iIndex==iCount)
				return m_members[i].GetPedGroupMember();
			iCount++;
		}
	}
	Assertf(false, "CPedGroupMembership::GetNthMember iterated members but didn't return one.");
	return NULL;
}

s32 CPedGroupMembership::GetNonLeaderMemberPriority(const CPed* pMember) const
{
	s32 i;
	s32 iPriority = 0;
	for(i=0;i<LEADER;i++)
	{
		if(m_members[i].GetPedGroupMember()==pMember)
		{
			return iPriority;
		}
		else if( m_members[i].IsAssigned() )
		{
			++iPriority;
		}
	}
	Assertf(0, "Ped not member!" );
	return -1;
}

bool CPedGroupMembership::IsLeader(const CPed* pLeader) const
{
	return (pLeader && m_members[LEADER].GetPedGroupMember()==pLeader);
}

bool CPedGroupMembership::IsFollower(const CPed* pFollower) const
{
	if(0==pFollower)
	{
		return false;
	}
	
	bool b=false;
	s32 i;
	for(i=0;i<LEADER;i++)
	{
		if(m_members[i].GetPedGroupMember()==pFollower)
		{
			b=true;
			break;
		}
	}
	return b;
}

bool CPedGroupMembership::IsMember(const CPed* pMember) const
{
	return (IsFollower(pMember) || IsLeader(pMember));	
}

s32 CPedGroupMembership::CountMembers() const
{
	s32 iCountResult=0;
	for(s32 i=0;i<MAX_NUM_MEMBERS;i++)
	{
		if(m_members[i].GetPedGroupMember())
		{
			iCountResult++;
		}
	}
	
	return iCountResult;
}

s32 CPedGroupMembership::CountMembersExcludingLeader() const
{
	s32 iCountResult=0;
	for(s32 i=0;i<MAX_NUM_MEMBERS-1;i++)
	{
		if(m_members[i].GetPedGroupMember())
		{
			iCountResult++;
		}
	}
	
	return iCountResult;
}
CPed* CPedGroupMembership::GetFurthestMemberFromPosition(Vector3& vPos, bool bIncludeLeader) const
{
	float fFurthestPedSq = 0.0f;
	CPed* pFurthestPed = NULL;
	s32 iEnd = bIncludeLeader ? MAX_NUM_MEMBERS : MAX_NUM_MEMBERS-1;
	for(s32 i=0;i<iEnd-1;i++)
	{
		if(m_members[i].GetPedGroupMember())
		{
			float fDistSq = DistSquared(m_members[i].GetPedGroupMember()->GetTransform().GetPosition(), RCC_VEC3V(vPos)).Getf();
			if( pFurthestPed == NULL || fDistSq > fFurthestPedSq )
			{
				pFurthestPed = m_members[i].GetPedGroupMember();
				fFurthestPedSq = fDistSq;
			}
		}
	}
	return pFurthestPed;
}

// PURPOSE: Sort the membership by distance to the leader
void CPedGroupMembership::SortByDistanceFromLeader()
{
	// If we don't have a leader then we need to return
	if(!GetLeader())
	{
		return;
	}

	// We don't want to include the leader when going through the members
	static const s32 iEnd = MAX_NUM_MEMBERS - 1;
	float fDistancesSq[iEnd];

	// Get the leader position and set the distances for each index
	Vec3V vPos = GetLeader()->GetTransform().GetPosition();
	for(s32 i = 0; i < iEnd; i++)
	{
		fDistancesSq[i] = m_members[i].GetPedGroupMember() ? DistSquared(m_members[i].GetPedGroupMember()->GetTransform().GetPosition(), vPos).Getf() : -1.0f;
	}

	// Loop through valid peds in the membership
	for(s32 i = 0; i < iEnd - 1; i++)
	{
		if(m_members[i].GetPedGroupMember())
		{
			float fLowestDistanceSq = fDistancesSq[i];
			int iLowestIndex = i;

			// Loop through the valid peds coming after the current index and if the distance is lower than our index store it
			for(s32 j = (i + 1); j < iEnd; j++)
			{
				if(m_members[j].GetPedGroupMember() && fDistancesSq[j] < fLowestDistanceSq)
				{
					fLowestDistanceSq = fDistancesSq[j];
					iLowestIndex = j;
				}
			}

			// if we found a ped with a shorter distance to the leader then swap these indices
			if(iLowestIndex != i)
			{
				float fTemp = fDistancesSq[i];
				fDistancesSq[i] = fDistancesSq[iLowestIndex];
				fDistancesSq[iLowestIndex] = fTemp;

				CPedGroupMember tempMember = m_members[i];
				m_members[i] = m_members[iLowestIndex];
				m_members[iLowestIndex] = tempMember;
			}
		}
	}
}

void CPedGroupMembership::Process()
{
	s32 i;

	if (m_pPedGroup->IsLocallyControlled())
	{
		// If a member (follower or leader) is dead then remove him from the group
		for(i=0;i<MAX_NUM_MEMBERS;i++)
		{
			if (m_members[i].GetPedGroupMember())
			{
				if ( m_members[i].GetPedGroupMember()->IsInjured() && m_members[i].GetPedGroupMember()->GetIsDeadOrDying() ) 
				{
					if ( (LEADER==i) && m_members[i].GetPedGroupMember()->IsPlayer() )
					{	//	can't remove the player if he is the leader
					
					}
					else
					{
						RemoveMember(i);
					}
				}
			}
			else if (m_members[i].IsAssigned())
			{
				RemoveMember(i); 
			}
		}
		
		//If there is no leader (maybe dead and just removed) then appoint a new one.
		if(!HasLeader())
		{
			AppointNewLeader();
		}
		
		//Remove any of the followers that are too far away 
		//from the leader to remain in the group.
		if(GetLeader())
		{
			const Vector3 vLeaderPosition = VEC3V_TO_VECTOR3(GetLeader()->GetTransform().GetPosition());
			for(i=0;i<LEADER;i++)
			{
				CPed* pFollower=m_members[i].GetPedGroupMember();
				// Remove any peds automatically on death of a player in multiplayer
				if( pFollower && GetLeader()->IsAPlayerPed() && GetLeader()->IsInjured() && NetworkInterface::IsGameInProgress() )
				{
					RemoveMember(i);
					continue;
				}
				if(pFollower && !pFollower->GetPedConfigFlag( CPED_CONFIG_FLAG_NeverLeavesGroup ))
				{
					Vector3 vDiff = VEC3V_TO_VECTOR3(pFollower->GetTransform().GetPosition()) - vLeaderPosition;
					const float f2=vDiff.Mag2();
					if((f2> (m_fMaxSeparation*m_fMaxSeparation))/* && pFollower->GetAreaCode()==GetLeader()->GetAreaCode()*/)
					{				
						RemoveMember(i);
					}
				}
			}
		}
	}

	if (NetworkInterface::IsGameInProgress())
	{
		AssignNetworkPedIds();
	}
}

void CPedGroupMembership::AppointNewLeader()
{	
	Assertf(m_pPedGroup->IsLocallyControlled(), "Only the machine which controls this ped group can appoint a new leader");

	if(!m_members[LEADER].IsAssigned())
	{
		//Find a member that can be appointed the new leader.
		s32 i;
		s32 iSlot=-1;
		for(i=0;i<LEADER;i++)
		{
			if(m_members[i].IsAssigned())
			{
				// can only appoint this ped as leader if we are in control of him
				if (!m_members[i].GetPedGroupMember() || m_members[i].GetPedGroupMember()->IsNetworkClone())
				{
					continue;
				}
				iSlot=i;
				break;
			}	
		}
		
		//Test if a member has been found.
		if(iSlot!=-1)
		{
			//Remove the member and set the new leader.
			CPed* pNewLeader=m_members[iSlot].GetPedGroupMember();
			RemoveMember(iSlot);
			bool bAddDefaultTask = !pNewLeader->IsSwatPed() || !pNewLeader->GetPedConfigFlag(CPED_CONFIG_FLAG_CreatedByDispatch) || pNewLeader->PopTypeIsMission();
			AddMember(pNewLeader, LEADER, true, bAddDefaultTask);
		}	
	}
}

void CPedGroupMembership::RemoveMember(const s32 iSlot, bool bSetDirty)
{
	Assert(iSlot>=0);
	Assert(iSlot<MAX_NUM_MEMBERS);

	if (m_members[iSlot].GetPedGroupMember())
	{
		m_members[iSlot].GetPedGroupMember()->SetPedGroupIndex(PEDGROUP_INDEX_NONE);

		AssertEntityPointerValid_NotInWorld( m_members[iSlot].GetPedGroupMember() );
	}

	if (m_members[iSlot].GetPedGroupMember() && !m_members[iSlot].GetPedGroupMember()->IsNetworkClone())

		if(m_members[iSlot].GetPedGroupMember() && !m_members[iSlot].GetPedGroupMember()->IsPlayer())
		{
			// Remove the radar blip this ped may have been given for being in the player group
			if (m_members[iSlot].GetPedGroupMember()->GetPedConfigFlag( CPED_CONFIG_FLAG_ClearRadarBlipOnDeath ))
			{
				Assert(!NetworkInterface::IsGameInProgress());
				m_members[iSlot].GetPedGroupMember()->RemoveBlip(BLIP_TYPE_CHAR);
				m_members[iSlot].GetPedGroupMember()->SetPedConfigFlag( CPED_CONFIG_FLAG_ClearRadarBlipOnDeath, false );
			}

			m_members[iSlot].GetPedGroupMember()->GetPedIntelligence()->GetPrimaryDefensiveArea()->Reset();

			// #if __BANK
			// 		if (CGroupDebug::ms_bUseSquadAIStuff)
			// 		{
			// 			m_members[iSlot].Clear();
			// 		}
			// 		else
			// #endif // __BANK
			{
				CPed* pMember = m_members[iSlot].GetPedGroupMember();
				m_members[iSlot].ClearPedGroupMember();

				// recalculate the members default task
				if( !pMember->IsInjured())
				{
					CTask* pActiveTask = pMember->GetPedIntelligence()->GetTaskActive();
					bool bIsWritheTask = pActiveTask && pActiveTask->GetTaskType() == CTaskTypes::TASK_WRITHE;
					bool bIsFireTask = pActiveTask && pActiveTask->GetTaskType() == CTaskTypes::TASK_COMPLEX_ON_FIRE;
					if (!pMember->GetIsInVehicle() && !bIsWritheTask && !bIsFireTask)
					{
						pMember->GetPedIntelligence()->ClearTaskEventResponse();
					}
					if (pMember->GetPedType() != PEDTYPE_SWAT)	// Swat hack - don't want to redo the default task when removed from the group
					{
						pMember->GetPedIntelligence()->AddTaskDefault( pMember->ComputeDefaultTask( *pMember ) );
					}
				}
			}
		}

		if (bSetDirty)
			m_pPedGroup->SetDirty();

		m_members[iSlot].ClearPedGroupMember();
}

void CPedGroupMembership::AddMember(CPed* pPed, const s32 iSlot, bool bSetDirty, bool bAddDefaultTask)
{
	Assert(iSlot>=0);
	Assert(iSlot<MAX_NUM_MEMBERS);
	Assert(pPed);
	Assert(!m_members[iSlot].IsAssigned());

	m_members[iSlot].SetPedGroupMember(pPed);
	m_members[iSlot].GetPedGroupMember()->SetPedGroupIndex(m_pPedGroup->GetGroupIndex());

	if (pPed->GetNetworkObject())
	{
		m_members[iSlot].m_pedNetworkId = pPed->GetNetworkObject()->GetObjectID();

		if (iSlot == LEADER && m_initialLeaderNetId == NETWORK_INVALID_OBJECT_ID)
		{
			m_initialLeaderNetId = m_members[iSlot].m_pedNetworkId;
		}
	}

	if(m_members[iSlot].GetPedGroupMember())
	{
		AssertEntityPointerValid_NotInWorld( m_members[iSlot].GetPedGroupMember() );
	};
	
#if __DEV
	// Debug thing:
	// Test whether this ped gets added to a group other than the players
	// when he is actually already in the player group.
	CPed * pPlayerPed = FindPlayerPed();

	if (pPlayerPed)
	{
		s32 localPlayerGroup = CPedGroups::GetPlayersGroup(FindPlayerPed());

		if (AssertVerify(localPlayerGroup >=0) && CPedGroups::ms_groups[localPlayerGroup].GetGroupMembership() != this)
		{
			if (CPedGroups::ms_groups[localPlayerGroup].GetGroupMembership()->IsMember(pPed))
			{
				Assertf(0, "This ped joined a group while he was in the player group.");
			}
		}
	}
#endif //__DEV

	if (bSetDirty)
	{
		if (iSlot == LEADER)
		{
			if (m_pPedGroup->PopTypeGet() == POPTYPE_PERMANENT) // we know that the leaders of all the permanent groups are the players, so we don't need to send this info
			{		
				bSetDirty = false;
			}
			else if (pPed->GetNetworkObject() && pPed->GetNetworkObject()->GetObjectID() != m_initialLeaderNetId)
			{
				// don't dirty the group if the leader is changing, the group will be moved to another slot by the ped groups manager
				bSetDirty = false;
			}
		}
		
		if (bSetDirty)
		{
			m_pPedGroup->SetDirty();
		}
	}

	if(!m_members[iSlot].GetPedGroupMember()->IsPlayer())
	{
		// we can't alter peds we don't control
		if (m_members[iSlot].GetPedGroupMember()->IsNetworkClone())
			return;

		if(bAddDefaultTask)
		{
			m_members[iSlot].GetPedGroupMember()->GetPedIntelligence()->AddTaskDefault( m_members[iSlot].GetPedGroupMember()->ComputeDefaultTask( *m_members[iSlot].GetPedGroupMember() ) );
		}
	}
}

void CPedGroupMembership::RemoveMembers(const CPedGroupMembership& members)
{
	s32 i;
	for(i=0;i<CPedGroupMembership::MAX_NUM_MEMBERS;i++)
	{
		if(members.HasMember(i))
		{
			RemoveMember(i);
		}
	}
}

// these functions are called by the network array sync stuff when a ped group is being synced across the network

void CPedGroupMembership::CopyNetworkInfo(const CPedGroupMembership& membership, bool bApplyToPeds)
{
	for (u32 i=0; i<CPedGroupMembership::MAX_NUM_MEMBERS; i++)
	{
		// ignore leader for player groups (the players are always the leaders)
		if (i == LEADER && m_pPedGroup->IsPlayerGroup())
		{
			continue;
		}

		ObjectId pedGlobalId = membership.m_members[i].m_pedNetworkId;

		if (bApplyToPeds)
		{
			if (m_pPedGroup->IsActive())
			{
				// copy membership to this group, assigning ped pointers and tidying up 
				if (membership.HasMember(i))
				{
					if (pedGlobalId != m_members[i].m_pedNetworkId)
					{
						RemoveMember(i, false);
					}
				}
				else if (HasMember(i))
				{
					RemoveMember(i, false);
				}
			}
		}

		m_members[i].m_pedNetworkId = pedGlobalId;
	}

	if (bApplyToPeds)
	{
		AssignNetworkPedIds();
	}
}

void CPedGroupMembership::Serialise(CSyncDataBase& serialiser)
{
	SERIALISE_PACKED_FLOAT(serialiser, m_fMaxSeparation, 200.0f, SIZEOF_MAX_SEPARATION, "Max separation"); 

	for (s32 i=0; i<MAX_NUM_MEMBERS; i++)
	{
		bool bMember = m_members[i].IsAssigned() || serialiser.GetIsMaximumSizeSerialiser();

		// don't need to sync the leader for player groups
		if (i==LEADER && m_pPedGroup->IsPlayerGroup())
			bMember = false;

		SERIALISE_BOOL(serialiser, bMember);

		if (bMember)
		{
			if (i==LEADER)
			{
				SERIALISE_OBJECTID(serialiser, m_members[i].m_pedNetworkId, "Leader");
			}
			else
			{
				SERIALISE_OBJECTID(serialiser, m_members[i].m_pedNetworkId, "Member");
			}
		}
		else
		{
			m_members[i].ClearPedGroupMember();
		}
	}
}

//////////////
//PED GROUP
//////////////

CPedGroup::CPedGroup() :
	m_pFormation(NULL),
	m_pedGroupPopType(POPTYPE_UNKNOWN),
	m_bActive(false),
	m_pScriptHandler(NULL),
	m_bCanDirtyDuringFlush(true)
{
	m_membership.SetPedGroup(this);
	m_lastPedRespondedTo = NULL;

	m_pFormation = CreateDefaultFormation();

	if (m_pFormation)
		m_pFormation->Init(this);

	m_bNeedsGroupEventScan = true;
}

CPedGroup::~CPedGroup()
{
	if(m_pFormation)
	{
		delete m_pFormation;
		m_pFormation = NULL;
	}
}

void CPedGroup::SetActive() 
{ 
	m_bActive = true; 
}

void CPedGroup::SetInactive() 
{
	m_bActive = false; 
}

CPedFormation * CPedGroup::CreateDefaultFormation()
{
	// JB: Please don't change this from FORMATION_LOOSE, as it messes up the default AI buddy following behaviour.
	// The function CPedGroup::SetFormation(iType) should be used to set a custom formation on a pedgroup if required.

	return CPedFormation::NewFormation(CPedFormationTypes::FORMATION_LOOSE);
}

// Includes subgroup members
s32  CPedGroup::FindTotalNumberOfMembersInGroup(CPedGroup* pGroup)
{
	s32 iTotal = pGroup->GetGroupMembership()->CountMembers();
	return iTotal;
}

void
CPedGroup::SetFormation(s32 iType)
{
	if(m_pFormation) delete m_pFormation;
	CPedFormation * pFormation = CPedFormation::NewFormation(iType);
	Assert(pFormation);
	m_pFormation = pFormation;
	m_pFormation->Init(this);
	m_pFormation->Process();
	SetDirty();
}

bool CPedGroup::IsLocallyControlled() const
{
	bool bLocallyControlled = false;

	if (NetworkInterface::IsGameInProgress())
	{
		if (IsPlayerGroup())
		{
			CNetGamePlayer* pLocalPlayer = NetworkInterface::GetLocalPlayer();

			if (pLocalPlayer && m_iGroupIndex == pLocalPlayer->GetPhysicalPlayerIndex())
			{
				bLocallyControlled = true;
			}
			else
			{
				// player bots control their own groups
				CPed* leader = GetGroupMembership() ? GetGroupMembership()->GetLeader() : NULL;
			
				if (leader && leader->IsNetworkBot() && !leader->IsNetworkClone())
				{
					bLocallyControlled = true;
				}
			}
		}
		else if (NetworkInterface::GetArrayManager().GetPedGroupsArrayHandler())
		{
			bLocallyControlled = !NetworkInterface::GetArrayManager().GetPedGroupsArrayHandler()->IsElementRemotelyArbitrated(GetNetArrayHandlerIndex());
		}
	}
	else
	{
		bLocallyControlled = true;
	}

	return bLocallyControlled;
}

bool CPedGroup::IsPlayerGroup() const
{
	return CPedGroups::IsPlayerGroup(m_iGroupIndex);
}

void
CPedGroup::SetFormation(CPedFormation * pFormation)
{
	Assert(pFormation);
	if(m_pFormation) delete m_pFormation;
	m_pFormation = pFormation;
	m_pFormation->Init(this);
	m_pFormation->Process();
	SetDirty();
}

void CPedGroup::Process()
{
	// Process group membership (appointing leader, removing distant peds, etc)
	m_membership.Process();

	// Process the group's formation.  Calculate new target positions.
	Assert(m_pFormation);

	if (m_membership.GetLeader())
		m_pFormation->Process();

	// Remove empty random groups
	if(PopTypeIsRandomNonPermanent() &&
		IsLocallyControlled() &&
		m_membership.CountMembers() == 0 )
	{

		Flush();
	}
}

void CPedGroup::Flush(bool bDirty, bool ASSERT_ONLY(bNetAssert))
{
	m_bCanDirtyDuringFlush = bDirty;
	Assertf((IsLocallyControlled() || !bNetAssert), "Trying to flush group %d which is controlled by another machine", m_iGroupIndex);

	m_membership.Flush(bDirty);
	
	SetInactive();

	if(GetFormation())
	{
		GetFormation()->SetDefaultSpacing();
		GetFormation()->ResetScriptModified();
	}

	if (bDirty)
		SetDirty();

	m_pScriptHandler = NULL;

	// SetPedGroupPopType(POPTYPE_RANDOM_AMBIENT); can't do this as CPlayerPed::SetupPlayerGroup() needs to flush the players group but keep it permanent
	m_bCanDirtyDuringFlush = true;
}

void CPedGroup::RemoveAllFollowers()
{
	m_membership.RemoveAllFollowers();
}

void CPedGroup::Teleport(const Vector3& vNewPos, bool bKeepTasks, bool bKeepIK, bool bResetPlants)
{
	Assertf(IsLocallyControlled(), "Only the machine which controls this ped group can teleport the group");

	//Teleport the leader first.
	CPed*  pLeader=m_membership.GetLeader();
	if(pLeader)
	{
		if(!pLeader->IsDead())
		{
			pLeader->Teleport(vNewPos, pLeader->GetCurrentHeading(), bKeepTasks, true, bKeepIK, bResetPlants);
		}
	}
	
	//Work out if the leader is in a vehicle.  
	//If the leader isn't in a vehicle and and any member is in a vehicle then 
	//set all that member out the car.
	if(pLeader && !pLeader->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
	{
		s32 i;
		for(i=0;i<CPedGroupMembership::LEADER;i++)
		{
			CPed* pFollower=m_membership.GetMember(i);
			if(pFollower && pFollower->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && !pFollower->PopTypeIsMission())
			{
				if(!pFollower->IsDead())
				{
					pFollower->SetPedOutOfVehicle(CPed::PVF_Warp);
				}
			}
		}
	}
		
	//Teleport all the members.
	//Resolve all collisions in the new coord.
	//Abort all event response tasks because the peds have been 
	//moved to a new coord where the event might no longer be valid. 
	Assert(m_pFormation);

	// Update the formation to be relative to the leader's new coords
	m_pFormation->Process();

	s32 i;
	for(i=0;i<CPedGroupMembership::LEADER;i++)
	{
		CPed* pFollower=m_membership.GetMember(i);
		if(pFollower)
		{
			if(!pFollower->IsInjured())
			{
				Assertf(!pFollower->IsNetworkClone(), "Cannot teleport a group member as it is not controlled by this machine");
//				Vector2 vOffset2D=CTaskComplexFollowLeaderInFormation::ms_offsets.GetOffSet(i);
//				Vector3 vNewFollowerPos=vNewPos;
//				vNewFollowerPos+=Vector3(vOffset2D.x,vOffset2D.y,0);

				const CPedPositioning & pedPositioning = m_pFormation->GetPedPositioning(i);
				Vector3 vNewFollowerPos = pedPositioning.GetPosition();

				pFollower->Teleport(vNewFollowerPos, pFollower->GetCurrentHeading(), bKeepTasks, true, bKeepIK, bResetPlants);
				pFollower->PositionAnyPedOutOfCollision();
			}
		}
	}
}

void CPedGroup::SetPedGroupPopType(const ePopType iGroupCreatedBy, bool bDirty) 
{
	m_pedGroupPopType = static_cast<ePopType>(iGroupCreatedBy); 
	
	if (bDirty)
	{
		SetDirty(); 
	}
}


CPed* CPedGroup::GetClosestGroupPed(CPed* pPed, float* retDistSqr)
{
	CPed* pClosestPed = NULL;
	float closestDistSqr = 99999999.0f;

	Vector3 vPedPosition(Vector3::ZeroType);
	if (pPed)
	{
		vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	}
	for (s32 i=0; i<CPedGroupMembership::MAX_NUM_MEMBERS; i++)
	{	
		CPed* pPedThis = GetGroupMembership()->GetMember(i);
		
		if (pPedThis && pPedThis!=pPed)
		{
			Vector3 vec = vPedPosition - VEC3V_TO_VECTOR3(pPedThis->GetTransform().GetPosition());
			float distSqr = vec.Mag2();
			
			if (distSqr < closestDistSqr)
			{
				pClosestPed = pPedThis;
				closestDistSqr = distSqr;
			}
		}
	}
	
	if (retDistSqr)
	{
		*retDistSqr = closestDistSqr;
	}
	
	return pClosestPed;
}


void CPedGroup::SetDirty()
{
	if(!m_bCanDirtyDuringFlush)
	{
		return;
	}

	if (NetworkInterface::IsGameInProgress() && Verifyf(IsLocallyControlled(), "Only the machine which controls this group can alter it"))
	{
		if (IsPlayerGroup())
		{
			CPed* leader = GetGroupMembership() ? GetGroupMembership()->GetLeader() : NULL;
			bool isNetworkBot = leader && leader->IsNetworkBot();

			CPed * pLocalPlayer = isNetworkBot ? GetGroupMembership()->GetLeader() : CGameWorld::FindLocalPlayer();

			netObject* pPlayerNetObj = AssertVerify(pLocalPlayer) ? pLocalPlayer->GetNetworkObject() : NULL;

			if (pPlayerNetObj && pPlayerNetObj->GetSyncData())
			{
				// manually dirty the ped group sync data
				CPlayerSyncTree* pSyncTree = static_cast<CPlayerSyncTree*>(pPlayerNetObj->GetSyncTree());
				pSyncTree->DirtyNode(pPlayerNetObj, *pSyncTree->GetPedGroupNode());
			}
		}
		else if (NetworkInterface::GetArrayManager().GetPedGroupsArrayHandler())
		{
			if (GetGroupMembership()->GetLeader() || NetworkInterface::GetArrayManager().GetPedGroupsArrayHandler()->GetElementArbitration(GetNetArrayHandlerIndex()) != NULL)
			{
				// flag ped groups array handler as dirty so that this ped group gets sent to the other machines
				NetworkInterface::GetArrayManager().GetPedGroupsArrayHandler()->SetElementDirty(GetNetArrayHandlerIndex());
			}
		}
	}
}


bool CPedGroup::IsAnyoneUsingCar(const CVehicle& vehicle) const
{
	s32 i;
	for(i=0;i<CPedGroupMembership::MAX_NUM_MEMBERS;i++)
	{
		CPed* pMember=m_membership.GetMember(i);
		if(pMember)
		{
			//If pMember is in car or is exiting car then the pMember will have a record of the car.
			if(pMember->GetMyVehicle() && pMember->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && pMember->GetMyVehicle()==&vehicle)
			{
				return true;
			}
			
			CTaskEnterVehicle* pEnterTask=(CTaskEnterVehicle*)pMember->GetPedIntelligence()->FindTaskByType(CTaskTypes::TASK_ENTER_VEHICLE);
			if( pEnterTask && pEnterTask->GetVehicle() == &vehicle)
			{
				return true;
			}
		}
	}
	
	return false;
}

////////////////////////////////////////////////////////
// FUNCTION: FindDistanceToFurthestMember
// Calculates the distance from the leader to the nearest member

float CPedGroup::FindDistanceToFurthestMember()
{
	float	GreatestDistance = 0;
	CPed	*pLeader = GetGroupMembership()->GetLeader();
	
	if (pLeader)
	{
		for (s32 M = 0; M < CPedGroupMembership::LEADER; M++)
		{
			CPed *pPed = GetGroupMembership()->GetMember(M);
			if (pPed)
			{
				float fDistanceToLeader = Dist(pPed->GetTransform().GetPosition(), pLeader->GetTransform().GetPosition()).Getf();
				GreatestDistance = rage::Max(GreatestDistance, fDistanceToLeader);
			}
		}
	}
	return GreatestDistance;
}

////////////////////////////////////////////////////////
// FUNCTION: FindDistanceToNearestMember
// Calculates the distance from the leader to the nearest member

float CPedGroup::FindDistanceToNearestMember(CPed **ppNearestPed) const
{
	float	SmallestDistance = 9999999999.9f;
	CPed	*pLeader = GetGroupMembership()->GetLeader();
	
	if (pLeader)
	{
		for (s32 M = 0; M < CPedGroupMembership::LEADER; M++)
		{
			CPed *pPed = GetGroupMembership()->GetMember(M);
			if (pPed)
			{
				float fDistanceToLeader = Dist(pPed->GetTransform().GetPosition(), pLeader->GetTransform().GetPosition()).Getf();
				SmallestDistance = rage::Min(SmallestDistance, fDistanceToLeader);
				if (ppNearestPed) *ppNearestPed = pPed;
			}
		}
	}
	return SmallestDistance;
}

void
CPedGroup::GiveEventToAllMembers(CEvent & event, CPed * pExcludeThisMember)
{
	CPedGroupMembership * pMembers = GetGroupMembership();
	for(s32 m=0; m<CPedGroupMembership::MAX_NUM_MEMBERS; m++)
	{
		CPed * pMember = pMembers->GetMember(m);
		if(!pMember || pMember == pExcludeThisMember)
			continue;

		if(pMember->IsPlayer())	// Don't give events to the player
			continue;

		if(pMember->IsNetworkClone())
			continue;

		pMember->GetPedIntelligence()->AddEvent(event);
	}
}

bool CPedGroup::IsSyncedWithPlayer(const CNetGamePlayer& player) const
{
	bool bSynced = true;

	if (NetworkInterface::IsGameInProgress())
	{
		if (IsPlayerGroup())
		{
			CNetGamePlayer* pLocalPlayer = NetworkInterface::GetLocalPlayer();

			if (pLocalPlayer && m_iGroupIndex == pLocalPlayer->GetPhysicalPlayerIndex() && pLocalPlayer->GetPlayerPed())
			{
				netObject* pPlayerNetObj = pLocalPlayer->GetPlayerPed()->GetNetworkObject();

				if (AssertVerify(pPlayerNetObj) && pPlayerNetObj->GetSyncData())
				{
					CPlayerSyncTree* pSyncTree = static_cast<CPlayerSyncTree*>(pPlayerNetObj->GetSyncTree());
					bSynced = pSyncTree->IsNodeSyncedWithPlayer(pPlayerNetObj, *pSyncTree->GetPedGroupNode(), player.GetPhysicalPlayerIndex());
				}
			}
		}
		else
		{
			CPedGroupsArrayHandler*	pPedGroupsHandler = NetworkInterface::GetArrayManager().GetPedGroupsArrayHandler();

			if (pPedGroupsHandler && pPedGroupsHandler->IsElementLocallyArbitrated(GetNetArrayHandlerIndex()))
			{
				bSynced = pPedGroupsHandler->IsElementSyncedWithPlayer(GetNetArrayHandlerIndex(), player.GetPhysicalPlayerIndex());
			}
		}
	}

	return bSynced;

}


bool CPedGroup::IsSyncedWithAllPlayers() const
{
	bool bSynced = true;

	if (NetworkInterface::IsGameInProgress())
	{
		if (IsPlayerGroup())
		{
			CNetGamePlayer* pLocalPlayer = NetworkInterface::GetLocalPlayer();

			if (pLocalPlayer && m_iGroupIndex == pLocalPlayer->GetPhysicalPlayerIndex() && pLocalPlayer->GetPlayerPed())
			{
				netObject* pPlayerNetObj = pLocalPlayer->GetPlayerPed()->GetNetworkObject();

				if (AssertVerify(pPlayerNetObj) && pPlayerNetObj->GetSyncData())
				{
					CPlayerSyncTree* pSyncTree = static_cast<CPlayerSyncTree*>(pPlayerNetObj->GetSyncTree());
					bSynced = pSyncTree->IsNodeSyncedWithAllPlayers(pPlayerNetObj, *pSyncTree->GetPedGroupNode());
				}
			}
		}
		else
		{
			CPedGroupsArrayHandler*	pPedGroupsHandler = NetworkInterface::GetArrayManager().GetPedGroupsArrayHandler();

			if (pPedGroupsHandler && pPedGroupsHandler->IsElementLocallyArbitrated(GetNetArrayHandlerIndex()))
			{
				bSynced = pPedGroupsHandler->IsElementSyncedWithAllPlayers(GetNetArrayHandlerIndex());
			}
		}
	}

	return bSynced;

}

// the array handler ignores the player ped groups (they are synced via the player ped updates
u32 CPedGroup::GetNetArrayHandlerIndex() const
{
	Assert(m_iGroupIndex >= MAX_NUM_PHYSICAL_PLAYERS);
	return m_iGroupIndex - MAX_NUM_PHYSICAL_PLAYERS;
}

void CPedGroup::Serialise(CSyncDataBase& serialiser)
{
	bool bPlayerGroup = IsPlayerGroup();

	SERIALISE_BOOL(serialiser, bPlayerGroup, "Player group");

	m_membership.Serialise(serialiser);

	if (!bPlayerGroup || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, reinterpret_cast<u32&>(m_pedGroupPopType), SIZEOF_POPTYPE, "Pop type");
	}

	if (AssertVerify(m_pFormation))
	{
		m_pFormation->Serialise(serialiser);
	}

	SERIALISE_BOOL(serialiser, m_bNeedsGroupEventScan, "Needs group event scan");
}

netObject* CPedGroup::GetNetworkObject() const
{
	CPed* pLeader = GetGroupMembership()->GetLeader();

	netObject* pNetObj = NULL;

	if (pLeader)
	{
		pNetObj = pLeader->GetNetworkObject();
	}
	else if (GetGroupMembership()->GetLeaderNetId() != NETWORK_INVALID_OBJECT_ID)
	{
		pNetObj = NetworkInterface::GetNetworkObject(GetGroupMembership()->GetLeaderNetId());
	}

	return pNetObj;
}

///////////////
//PED GROUPS
///////////////

CPedGroup CPedGroups::ms_groups[CPedGroups::MAX_NUM_GROUPS];
bool CPedGroups::ms_bIsPlayerOnAMission=false;
s32 CPedGroups::ms_iNoOfPlayerKills=0;

s32 CPedGroups::AddGroup(const ePopType pedGroupPopType, CGameScriptHandler* pScriptHandler)
{
	s32 iGroupID=-1;
	s32 i;
	s32 iStart = 0;
	s32 iEnd = MAX_NUM_GROUPS;

	Assertf(!(pedGroupPopType==POPTYPE_MISSION && !pScriptHandler), "Mission ped groups must have an associated script handler");
	Assertf(!(pedGroupPopType!=POPTYPE_MISSION && pScriptHandler), "Non-mission ped groups should not have an associated script handler");

	// The first MAX_NUM_PHYSICAL_PLAYERS groups are permanently allocated to each player
	// The next 16 are for script created groups
	// The rest are for random groups

	switch (pedGroupPopType)
	{
	case POPTYPE_UNKNOWN:
		Assertf(false, "A valid pop type should be passed in.");
		return 0;
	case POPTYPE_RANDOM_PERMANENT : // Purposeful fall through.
	case POPTYPE_RANDOM_PARKED : // Purposeful fall through.
	case POPTYPE_RANDOM_PATROL : // Purposeful fall through.
	case POPTYPE_RANDOM_SCENARIO : // Purposeful fall through.
	case POPTYPE_RANDOM_AMBIENT :
		iStart = MAX_NUM_PHYSICAL_PLAYERS + MAX_NUM_SCRIPT_GROUPS;
		iEnd = MAX_NUM_GROUPS;
		break;
	case POPTYPE_PERMANENT :
		iStart = 0;
		iEnd = MAX_NUM_PHYSICAL_PLAYERS;
		break;
	case POPTYPE_MISSION :// Purposeful fall through.
	case POPTYPE_REPLAY :// Purposeful fall through.
	case POPTYPE_TOOL :
	case POPTYPE_CACHE :
		iStart = MAX_NUM_PHYSICAL_PLAYERS;
		iEnd = MAX_NUM_PHYSICAL_PLAYERS + MAX_NUM_SCRIPT_GROUPS;
		break;
	case NUM_POPTYPES : 
		Assert(0);
	}

	CPedGroupsArrayHandler* pPedGroupsHandler = NetworkInterface::IsGameInProgress() ? NetworkInterface::GetArrayManager().GetPedGroupsArrayHandler() : NULL;

	for(i=iStart;i<iEnd;i++)
	{
		if(!ms_groups[i].IsActive())
		{
			// we can't use an empty slot that is still in the process of cleaning up (ie informing other players it is empty), because the net id 
			// of the slot will now differ from the net id of the ped group leader.
			if (i>=MAX_NUM_PHYSICAL_PLAYERS && pPedGroupsHandler && pPedGroupsHandler->GetElementArbitration(i-MAX_NUM_PHYSICAL_PLAYERS))
			{
				continue;
			}

			iGroupID=i;
			ms_groups[i].Flush(false);
			ms_groups[i].SetActive();
			ms_groups[i].m_pScriptHandler = pScriptHandler;
			ms_groups[i].SetPedGroupPopType(pedGroupPopType);
			ms_groups[i].SetNeedsGroupEventScan(true);
			break;
		}
	}
	return iGroupID;
}

void CPedGroups::AddGroupAtIndex(const ePopType pedGroupPopType, s32 index)
{
	ms_groups[index].Flush(false, false);
	ms_groups[index].SetActive();
	ms_groups[index].SetPedGroupPopType(pedGroupPopType, false);
	ms_groups[index].SetNeedsGroupEventScan(true);
}

void CPedGroups::RemoveGroup(const s32 iGroupID, bool bDirty)
{
	Assert(ms_groups[iGroupID].PopTypeGet() != POPTYPE_PERMANENT && ms_groups[iGroupID].PopTypeGet() != POPTYPE_MISSION);
	RemoveGroupInternal(iGroupID, bDirty);
}

void CPedGroups::RemoveGroupFromScript(const s32 iGroupID, bool bDirty)
{
	Assert(ms_groups[iGroupID].PopTypeGet() != POPTYPE_PERMANENT && ms_groups[iGroupID].PopTypeGet() == POPTYPE_MISSION);
	RemoveGroupInternal(iGroupID, bDirty);
}


void CPedGroups::RemoveGroupInternal(const s32 iGroupID, bool bDirty)
{
	Assert(iGroupID<MAX_NUM_GROUPS);

	if(!ms_groups[iGroupID].IsActive())
	{
		return;
	}

	ms_groups[iGroupID].Flush(bDirty, bDirty);
}

bool CPedGroups::MoveGroupToFreeSlot(const s32 iGroupID, s32* freeSlotID )
{
	CPedGroup* pOriginalGroup = &ms_groups[iGroupID];

	Assert(NetworkInterface::IsGameInProgress()); // this should only be used in the network game otherwise it wont work properly 
	Assert(pOriginalGroup->IsActive());
	Assert(pOriginalGroup->IsLocallyControlled());

	s32 newGroupID = AddGroup(pOriginalGroup->PopTypeGet());

	if (newGroupID != -1)
	{
		CPedGroup* pNewGroup = &ms_groups[newGroupID];

		pNewGroup->CopyNetworkInfo(*pOriginalGroup, true);

		// membership of original group should now be 0 (CopyNetworkInfo should have reassigned all the peds via AssignNetworkPedIds)
		Assert(pOriginalGroup->GetGroupMembership()->CountMembers() == 0);

		if (pOriginalGroup->m_pScriptHandler)
		{
			pNewGroup->m_pScriptHandler = pOriginalGroup->m_pScriptHandler;

			// fixup resource reference to moved group
			scriptResource* pResource = pOriginalGroup->m_pScriptHandler->GetScriptResource(CGameScriptResource::SCRIPT_RESOURCE_PEDGROUP, iGroupID);

			if (AssertVerify(pResource))
			{
				pResource->SetReference(newGroupID);
			}
		}

		pOriginalGroup->Flush(false, false);

		if (freeSlotID)
		{
			*freeSlotID = newGroupID;
		}

		pNewGroup->SetDirty();

		return true;
	}

	return false;
}

//	CPedGroups::RemoveAllFollowersFromGroup - Removes all followers but leaves the leader
void CPedGroups::RemoveAllFollowersFromGroup(const s32 iGroupID)
{
	Assert(iGroupID<MAX_NUM_GROUPS);
	
	if(!ms_groups[iGroupID].IsActive())
	{
		return;
	}
	
	ms_groups[iGroupID].RemoveAllFollowers();
}

void CPedGroups::RemoveAllRandomGroups()
{
	for(u32 index = 0; index < MAX_NUM_GROUPS; index++)
	{
		CPedGroup *pedGroup = &ms_groups[index];

		if(pedGroup->IsActive() && pedGroup->PopTypeIsRandomNonPermanent())
		{
			RemoveGroup(index);
		}
	}
}

void CPedGroups::RemoveAllNonPermanentGroups()
{
	for(u32 index = 0; index < MAX_NUM_GROUPS; index++)
	{
		CPedGroup *pedGroup = &ms_groups[index];

		if(pedGroup->IsActive() && pedGroup->PopTypeGet() != POPTYPE_PERMANENT)
		{
			RemoveGroup(index);
		}
	}
}

//-------------------------------------------------------------------------
// Creates any needed default groups
//-------------------------------------------------------------------------
void CPedGroups::CreateDefaultGroups()
{
	// create a group for each player in a network game
	for (u32 i=0; i<MAX_NUM_PHYSICAL_PLAYERS; i++)
	{
		AddGroup(POPTYPE_PERMANENT);

		// set up any existing players with their groups
		if (NetworkInterface::IsNetworkOpen())
		{
			CNetGamePlayer* pPlayer = static_cast<CNetGamePlayer*>(NetworkInterface::GetPlayerMgr().GetPhysicalPlayerFromIndex((PhysicalPlayerIndex) i));

			if (pPlayer && pPlayer->GetPlayerPed())
			{
				pPlayer->GetPlayerPed()->GetPlayerInfo()->SetupPlayerGroup();
			}
		}
	}

	if (!NetworkInterface::IsNetworkOpen() && FindPlayerPed())
		FindPlayerPed()->GetPlayerInfo()->SetupPlayerGroup();

}

void CPedGroups::Init()
{
	s32 i;
	for(i=0;i<MAX_NUM_GROUPS;i++)
	{
		ms_groups[i].SetGroupIndex((u8)i);
		RemoveGroupInternal(i, false);
		ms_groups[i].m_scriptRefIndex = 1;
	}

	// create any default groups
	CreateDefaultGroups();
}

#define THRESHOLD_NUM_PLAYER_KILLS (8)

void CPedGroups::Process()
{
	u32 i;

	//********************************************************************************************************
	// DEBUG : Make sure that the "m_iPedGroupIndex" member in CPed is up to date for all peds in all groups.
	//********************************************************************************************************

#if __ASSERT
	for(i=0;i<MAX_NUM_GROUPS;i++)
	{
		if(ms_groups[i].IsActive())
		{
			CPedGroup & group=ms_groups[i];
			CPedGroupMembership * pMembership = group.GetGroupMembership();
			for(s32 m=0; m<CPedGroupMembership::MAX_NUM_MEMBERS; m++)
			{
				CPed * pMember = pMembership->GetMember(m);
				if(pMember)
				{
					Assertf(pMember->GetPedGroupIndex() == i, "Ped thinks he is part of group %i, but his m_iPedGroupIndex is %i", i, pMember->GetPedGroupIndex());
				}
			}
		}
	}
#endif // __ASSERT

	//Count the number of members of each active group
	//Appoint new leader for groups with no leader.
	//Remove groups with no members left. 

	for(i=0;i<MAX_NUM_GROUPS;i++)
	{
		if(ms_groups[i].IsActive())
		{
			CPedGroup& r=ms_groups[i];
			r.Process();
			
			//Count the number of members.  
			//If none are left then remove the group.
			//If the group remains then test if the leader 
			//or membership has changed.
			// Never destroy permanent groups
			if( ( r.PopTypeGet() != POPTYPE_PERMANENT ) && r.IsLocallyControlled())
			{
				if (r.GetGroupMembership()->CountMembers() == 0) 
				{
					RemoveGroupInternal(i, true);
				}

				// groups that change leadership need to be moved to a new slot in MP, this is because the group is identified by the leader's network id across the
				// network, so we need to create a new group for the new leader. The previous group is flushed.
				if (NetworkInterface::IsGameInProgress() && 
					r.IsActive() && 
					r.GetGroupMembership()->HasLeader() && 
					r.GetGroupMembership()->LeaderHasChanged())
				{
					if (!MoveGroupToFreeSlot(i))
					{
						RemoveGroupInternal(i, true);
					}
				}
			}
		}
	}
	
	ms_bIsPlayerOnAMission=CTheScripts::GetPlayerIsOnAMission();
	if(!ms_bIsPlayerOnAMission)
	{
		ms_iNoOfPlayerKills=0;	
	}
}

void CPedGroups::ResetAllFormationsModifiedByThisScript(const scrThreadId iThreadId)
{
	Assert(iThreadId);

	for(s32 i=0;i<MAX_NUM_GROUPS;i++)
	{
		CPedFormation * pFormation = ms_groups[i].GetFormation();
		Assert(pFormation);

		if(pFormation->GetScriptModified() == iThreadId)
		{
			pFormation->SetDefaultSpacing();
			pFormation->ResetScriptModified();

			ms_groups[i].SetDirty();
		}
	}
}

void CPedGroups::RegisterKillByPlayer()
{
	if(ms_bIsPlayerOnAMission)
	{
		ms_iNoOfPlayerKills++;
	}
}

// need to flush out the tasks which are held as static data before shutdown
void CPedGroups::Shutdown()
{
	u32	i;
	
	for(i=0; i<MAX_NUM_GROUPS; i++)
	{
		ms_groups[i].Flush(false, false);
	}
}

bool CPedGroups::IsPlayerGroup(s32 iGroupID)
{
	return iGroupID < static_cast<s32>(MAX_NUM_PHYSICAL_PLAYERS);
}

bool CPedGroups::IsGroupLeader(CPed* pPed)
{
	s32 i;
	for(i=0;i<MAX_NUM_GROUPS;i++)
	{
		if(ms_groups[i].IsActive())
		{
			CPedGroup& r=ms_groups[i];	
			if(r.GetGroupMembership()->IsLeader(pPed))
			{
				return true;
			}
		}
	}
	return false;
}

CPedGroup* CPedGroups::GetPedsGroup(const CPed* pPed)
{
	Assert(pPed);

	u32 group_index = pPed->GetPedGroupIndex();

	if(group_index==PEDGROUP_INDEX_NONE)
		return NULL;

	Assert(group_index < MAX_NUM_GROUPS);
	CPedGroup * pPedGroup = &ms_groups[group_index];

#if __ASSERT
	const CPedGroupMembership* const pGroupMembership = pPedGroup->GetGroupMembership();
	bool bIsMember = pGroupMembership->IsMember(pPed);
	s32 NumberOfMembers = pGroupMembership->CountMembers();
	if(pPed)
	{
		Assertf(bIsMember, "The ped isn't a member of the group he thinks he is part of. Group Index: %i, GroupSize: %i, Ped Model Name - %s", group_index, NumberOfMembers, pPed->GetDebugName());
	}
	else
	{
		Assertf(bIsMember, "The ped isn't a member of the group he thinks he is part of. Group Index: %i, GroupSize: %i", group_index, NumberOfMembers);
	}
#endif

	return pPedGroup;
}

s32 CPedGroups::GetGroupId(CPedGroup* pPedGroup)
{
	Assert(pPedGroup);
	
	for (s32 i=0; i<MAX_NUM_GROUPS; i++)
	{
		if (&ms_groups[i] == pPedGroup)
		{
			return i;
		}	
	}
	
	return -1;
}

bool CPedGroups::IsInPlayersGroup(const CPed* pPed)
{
	// don't say the player is in the player's group!
	if(pPed->GetPlayerInfo())
		return false;

	const CPedGroupMembership* const pGroupMembership = ms_groups[FindPlayerPed()->GetPlayerInfo()->GetPlayerDataPlayerGroup()].GetGroupMembership();
	if(pGroupMembership->IsMember(pPed))
		return true;

	return false;
}

s32 CPedGroups::GetPlayersGroup(const CPed* pPlayer)
{
	s32 playerGroup = 0;

	// the first bunch of groups correspond to each physical player in MP
	if (pPlayer->GetNetworkObject() && AssertVerify(pPlayer->GetNetworkObject()->GetPlayerOwner()))
	{
		if (AssertVerify(pPlayer->GetNetworkObject()->GetPlayerOwner()->GetPhysicalPlayerIndex() != INVALID_PLAYER_INDEX))
		{
			playerGroup = pPlayer->GetNetworkObject()->GetPlayerOwner()->GetPhysicalPlayerIndex();
		}
	}

	return playerGroup;
}

bool CPedGroups::AreInSameGroup(const CPed* pPed1, const CPed* pPed2)
{
	if( pPed1->GetPedsGroup() && ( pPed1->GetPedsGroup() == pPed2->GetPedsGroup()) )
	{
		return true;
	}
	return false;
// 	if((0==pPed1)||(0==pPed2))
// 	{
// 		return false;
// 	}
// 	
// 	s32 i;
// 	for(i=0;i<MAX_NUM_GROUPS;i++)
// 	{
// 		if(ms_groups[i].IsActive())
// 		{
// 			CPedGroup& r=ms_groups[i];	
// 			if(r.GetGroupMembership()->IsMember(pPed1) && r.GetGroupMembership()->IsMember(pPed2))
// 			{
// 				return true;
// 			}
// 		}
// 	}
// 	return false;
}
