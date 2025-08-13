// Title	:	Relationships.cpp
// Author	:	Phil Hooker
// Started	:	08/03/06

// Set of classes used to manage the relationships between all peds.
// Replacing the dependancy between pedtype and relationships.

// C headers
#include <stdio.h>

//Game headers
#include "debug/debug.h"
#include "peds/ped.h"
#include "peds/ped_channel.h"
#include "peds/pedIntelligence.h"
#include "peds/relationships.h"
#include "System\FileMgr.h"

#include "peds/Relationships_parser.h"

AI_OPTIMISATIONS();

//-------------------------------------------------------------------------
// RELATIONSHIP GROUPS
//-------------------------------------------------------------------------

FW_INSTANTIATE_CLASS_POOL_SPILLOVER(CRelationshipGroup, MAX_RELATIONSHIP_GROUPS, 0.54f, atHashString("CRelationshipGroup",0x8de0784b));

#define IMPLEMENT_RELATIONSHIP_TUNABLES(classname, hash)	IMPLEMENT_TUNABLES_PSO(classname, #classname, hash, "Relationship Tuning", "Ped Population")

CRelationshipGroup::CRelationshipGroup(atHashString name, eGroupOwnershipType type)
: m_name(name)
, m_eType(type)
, m_index(0xFFFF)
, m_bCanBeCleanedUp(false)
, m_bAffectsWantedLevel(true)
, m_bBlipPeds(false)
{
	for( u32 i = 0; i < MAX_RELATIONSHIP_GROUPS; i++ )
	{
		m_relations[i] = (u8)ACQUAINTANCE_TYPE_INVALID;
	}

	for ( u8 j = 0; j < MAX_NUM_ACQUAINTANCE_TYPES; j++ )
	{
		m_relationTypeRef[j] = 0;
	}
}

CRelationshipGroup::~CRelationshipGroup()
{
#if __ASSERT
	for ( u8 j = 0; j < MAX_NUM_ACQUAINTANCE_TYPES; j++ )
	{
		pedAssertf(m_relationTypeRef[j] == 0, "RelationshipGroup::ClearAllRelationships. Reference count isn't zero as expected\n");
	}
#endif
}

void CRelationshipGroup::SetRelationshipByIndex( eRelationType AcquaintanceType, unsigned int index )
{
	pedAssert(index <= MAX_RELATIONSHIP_GROUPS);
	if (m_relations[index] != ACQUAINTANCE_TYPE_INVALID)
	{
		pedAssertf(m_relationTypeRef[m_relations[index]] > 0, "RelationshipGroup. Decrementing reference count below zero\n");
		--m_relationTypeRef[m_relations[index]];
	}

	Assert(AcquaintanceType >= 0 && AcquaintanceType <= 0xff);
	m_relations[index] = (u8)AcquaintanceType;

	if (AcquaintanceType != ACQUAINTANCE_TYPE_INVALID)
	{
		++m_relationTypeRef[AcquaintanceType];
		pedAssertf(m_relationTypeRef[AcquaintanceType] < 255,  "RelationshipGroup. Incrementing reference count above max\n");
	}
}

void CRelationshipGroup::SetRelationship( eRelationType AcquaintanceType, CRelationshipGroup* pOtherRelationshipGroup )
{
	SetRelationshipByIndex(AcquaintanceType, pOtherRelationshipGroup->GetIndex());
}

void CRelationshipGroup::ClearRelationship( CRelationshipGroup* pOtherRelationshipGroup )
{
	pedAssert(pOtherRelationshipGroup);
	SetRelationship(ACQUAINTANCE_TYPE_INVALID,pOtherRelationshipGroup);
}

void CRelationshipGroup::ClearAllRelationships()
{
	for( u32 i = 0; i < MAX_RELATIONSHIP_GROUPS; i++ )
	{
		SetRelationshipByIndex(ACQUAINTANCE_TYPE_INVALID,i);
	}

#if __ASSERT 
	for( u32 j = 0; j < MAX_NUM_ACQUAINTANCE_TYPES; j++ )
	{
		pedAssertf( m_relationTypeRef[j] == 0, "RelationshipGroup::ClearAllRelationships. reference count isn't zero as expected\n");
	}
#endif 
}

void CRelationshipGroup::ClearAllRelationshipsOfType( eRelationType AcquaintanceType )
{
	pedAssert(AcquaintanceType >= 0);
	for( u32 i = 0; i < MAX_RELATIONSHIP_GROUPS; i++ )
	{
		if( m_relations[i] == (u8)AcquaintanceType )
		{
			SetRelationshipByIndex(ACQUAINTANCE_TYPE_INVALID,i);
		}
	}
	pedAssert( m_relationTypeRef[AcquaintanceType] == 0);
}

bool CRelationshipGroup::HasAnyRelationShipOfType( eRelationType AcquaintanceType ) const
{
	if ( m_relationTypeRef[AcquaintanceType] > 0)
	{
		return true;
	}

	return false;
}

RegdRelationshipGroup CRelationshipManager::s_pPlayerGroup;
RegdRelationshipGroup CRelationshipManager::s_pCopGroup;
RegdRelationshipGroup CRelationshipManager::s_pArmyGroup;
RegdRelationshipGroup CRelationshipManager::s_pSecurityGuardGroup;
RegdRelationshipGroup CRelationshipManager::s_pCivmaleGroup;
RegdRelationshipGroup CRelationshipManager::s_pCivfemaleGroup;
RegdRelationshipGroup CRelationshipManager::s_pNoRelationshipGroup;
RegdRelationshipGroup CRelationshipManager::s_pAggressiveInvestigateGroup;
unsigned int CRelationshipManager::ms_iNextUpdateTime = 0;

void	CRelationshipManager::Init( void )
{
	ms_iNextUpdateTime = 0;

	CRelationshipGroup::InitPool( MEMBUCKET_GAMEPLAY );
	LoadRelationshipData();
}

//-------------------------------------------------------------------------
// Shutdown the relationship manager
//-------------------------------------------------------------------------
void	CRelationshipManager::Shutdown( void )
{
	s_pPlayerGroup = NULL;
	s_pCopGroup = NULL;
	s_pArmyGroup = NULL;
	s_pSecurityGuardGroup = NULL;
	s_pCivmaleGroup = NULL;
	s_pCivfemaleGroup = NULL;
	s_pNoRelationshipGroup = NULL;
	s_pAggressiveInvestigateGroup = NULL;

	CRelationshipGroup::ShutdownPool();
}

CRelationshipManager::Tunables CRelationshipManager::sm_Tunables;
IMPLEMENT_RELATIONSHIP_TUNABLES(CRelationshipManager, 0x4481ab47);

CRelationshipGroup* CRelationshipManager::AddRelationshipGroup( atHashString relgroupName, eGroupOwnershipType type )
{
#if __BANK
	if( CRelationshipGroup::GetPool()->GetNoOfFreeSpaces() == 0 )
	{
		s32 iCount = CRelationshipGroup::GetPool()->GetNoOfUsedSpaces();
		for(s32 i = 0; i < iCount; i++)
		{
			CRelationshipGroup* pGroup = CRelationshipGroup::GetPool()->GetSlot(i);
			if( pGroup )
			{
				Displayf("%i: %s", i, pGroup->GetName().GetCStr());
			}
		}
		Assertf(0, "Out of relationship groups! See spew for current list, include spew in any bug!");
	}
#endif // __BANK

	CRelationshipGroup* pGroup = rage_new CRelationshipGroup(relgroupName, type);
	if( Verifyf(pGroup, "Failed to create relationship group!" ) )
	{
		if( atHashString("PLAYER",0x6F0783F5) == relgroupName )
		{
			s_pPlayerGroup = pGroup;
			s_pPlayerGroup->SetAffectsWantedLevel(false);
			s_pPlayerGroup->SetType(RT_mission);
		}
		else if( atHashString("COP",0xA49E591C) == relgroupName )
			s_pCopGroup = pGroup;
		else if( atHashString("CIVMALE",0x2B8FA80) == relgroupName )
			s_pCivmaleGroup = pGroup;
		else if( atHashString("CIVFEMALE",0x47033600) == relgroupName )
			s_pCivfemaleGroup = pGroup;
		else if( atHashString("NO_RELATIONSHIP",0xFADE4843) == relgroupName )
			s_pNoRelationshipGroup = pGroup;
		else if( atHashString("AGGRESSIVE_INVESTIGATE",0xEB47D4E0) == relgroupName )
		{
			s_pAggressiveInvestigateGroup = pGroup;
			pGroup->SetType(RT_mission);
		}
		else if( atHashString("ARMY",0xE3D976F3) == relgroupName )
		{
			s_pArmyGroup = pGroup;
			pGroup->SetType(RT_mission);
		}
		else if( atHashString("SECURITY_GUARD",0xF50B51B7) == relgroupName )
		{
			s_pSecurityGuardGroup = pGroup;
			pGroup->SetType(RT_mission);
		}
		else
			pGroup->SetType(RT_mission);

		s32 iIndex = CRelationshipGroup::GetPool()->GetJustIndex(pGroup);
		pGroup->SetIndex((u16)iIndex);
		if( type == RT_mission )
			pGroup->SetRelationship(ACQUAINTANCE_TYPE_PED_LIKE, pGroup);
// #if __DEV
// 		s32 iSignedVal = (s32) relgroupName.GetHash();
// 		iSignedVal = iSignedVal;
// 		printf("GROUP: %s=%d\n", relgroupName.GetCStr(), iSignedVal);
// #endif

	}

	return pGroup;
}


// Name			:	LoadRelationshipData
// Purpose		:	Loads relationship data info from PED.DAT file
// Parameters	:	None
// Returns		:	Nothing
void CRelationshipManager::LoadRelationshipData(void)
{
	FileHandle fid;
	char* pLine;
	char* pToken;
	char aString[32];
	const char* seps = " ,\t";

	CRelationshipGroup* pCurrentGroup = NULL;

	fid = CFileMgr::OpenFile("common:/DATA/Relationships.DAT");
	if (Verifyf(CFileMgr::IsValidFileHandle(fid), "Problem loading Relationships.dat"))
	{
		// keep going till we reach the end of the file
		while ((pLine = CFileMgr::ReadLine(fid)) != NULL)
		{
			// ignore lines starting with # as they are comments
			if(pLine[0] == '#' || pLine[0] == '\0')
				continue;

			sscanf(pLine, "%s", aString);

			// Get all acquaintances.
			if(!strcmp(aString, "Hate"))
			{
				Assert(pCurrentGroup);

				strtok(pLine, seps);
				while((pToken = strtok(NULL, seps)) != NULL)
				{
					atHashString groupHash(pToken);
					CRelationshipGroup* pGroup = FindRelationshipGroup(groupHash);
					if( pGroup == NULL )
						pGroup = AddRelationshipGroup(groupHash, RT_random);

					if( pGroup != NULL )
					{
						pCurrentGroup->SetRelationship(ACQUAINTANCE_TYPE_PED_HATE, pGroup);
					}
				}
			}
			else if(!strcmp(aString, "Dislike"))
			{
				Assert(pCurrentGroup != NULL);

				strtok(pLine, seps);
				while((pToken = strtok(NULL, seps)) != NULL)
				{
					atHashString groupHash(pToken);
					CRelationshipGroup* pGroup = FindRelationshipGroup(groupHash);
					if( pGroup == NULL )
						pGroup = AddRelationshipGroup(groupHash, RT_random);

					if( pGroup != NULL )
					{
						pCurrentGroup->SetRelationship(ACQUAINTANCE_TYPE_PED_DISLIKE, pGroup);
					}
				}
			}
			else if(!strcmp(aString, "Like"))
			{
				Assert(pCurrentGroup != NULL);

				strtok(pLine, seps);
				while((pToken = strtok(NULL, seps)) != NULL)
				{
					atHashString groupHash(pToken);
					CRelationshipGroup* pGroup = FindRelationshipGroup(groupHash);
					if( pGroup == NULL )
						pGroup = AddRelationshipGroup(groupHash, RT_random);

					if( pGroup != NULL )
					{
						pCurrentGroup->SetRelationship(ACQUAINTANCE_TYPE_PED_LIKE, pGroup);
					}
				}
			}
			else if(!strcmp(aString, "Respect"))
			{
				Assert(pCurrentGroup != NULL);

				strtok(pLine, seps);
				while((pToken = strtok(NULL, seps)) != NULL)
				{
					atHashString groupHash(pToken);

					CRelationshipGroup* pGroup = FindRelationshipGroup(groupHash);
					if( pGroup == NULL )
						pGroup = AddRelationshipGroup(groupHash, RT_random);

					if( pGroup != NULL )
					{
						pCurrentGroup->SetRelationship(ACQUAINTANCE_TYPE_PED_RESPECT, pGroup);
					}

				}
			}
			else
			{
				atHashString groupHash(aString);
				pCurrentGroup = FindRelationshipGroup(groupHash);
				if( pCurrentGroup == NULL )
				{
					pCurrentGroup = AddRelationshipGroup(groupHash, RT_random);
				}
			}
		}

		CFileMgr::CloseFile(fid);
	}

} // end - CPedType::LoadPedData

//-------------------------------------------------------------------------
// Will go through an remove any relationship groups under certain conditions
//-------------------------------------------------------------------------

void CRelationshipManager::UpdateRelationshipGroups( void )
{
	if(fwTimer::GetTimeInMilliseconds() >= ms_iNextUpdateTime)
	{
		s32 iPoolSize = CRelationshipGroup::GetPool()->GetSize();
		for( s32 i = 0; i < iPoolSize; i++ )
		{
			CRelationshipGroup* pGroup = CRelationshipGroup::GetPool()->GetSlot(i);
			if( pGroup && pGroup->GetCanBeCleanedUp() && !pGroup->IsReferenced())
			{
				RemoveRelationshipGroup(pGroup);
			}
		}

		ms_iNextUpdateTime = fwTimer::GetTimeInMilliseconds() + 2000;
	}
}

//-------------------------------------------------------------------------
// Takes a string and returns the corresponding relationship group
//-------------------------------------------------------------------------


CRelationshipGroup* CRelationshipManager::FindRelationshipGroup( atHashString relgroupName )
{
	return FindRelationshipGroup(relgroupName.GetHash());
}

CRelationshipGroup* CRelationshipManager::FindRelationshipGroup( u32 relGroupHash )
{
	s32 iPoolSize = CRelationshipGroup::GetPool()->GetSize();
	for( s32 i = 0; i < iPoolSize; i++ )
	{
		CRelationshipGroup* pGroup = CRelationshipGroup::GetPool()->GetSlot(i);
		if( pGroup && pGroup->GetName().GetHash() == relGroupHash )
		{
			return pGroup;
		}
	}
	return NULL;
}

CRelationshipGroup* CRelationshipManager::FindRelationshipGroupByIndex( s32 iIndex )
{
	CRelationshipGroup* pGroup = CRelationshipGroup::GetPool()->GetSlot(iIndex);
	if( pGroup )
	{
		return pGroup;
	}
	return pGroup;
}

void CRelationshipManager::RemoveRelationshipGroup( atHashString relgroupName )
{
	s32 iPoolSize = CRelationshipGroup::GetPool()->GetSize();
	for( s32 i = 0; i < iPoolSize; i++ )
	{
		CRelationshipGroup* pGroup = CRelationshipGroup::GetPool()->GetSlot(i);
		if( pGroup && pGroup->GetName() == relgroupName )
		{
			RemoveRelationshipGroup(pGroup);
			return;
		}
	}
}

void CRelationshipManager::RemoveRelationshipGroup( CRelationshipGroup* pGroupToDelete )
{
	Assert(pGroupToDelete);

#if !__NO_OUTPUT
	if(sm_Tunables.m_DisplayRemovedGroups)
	{
		Displayf("Removing relationship group: %s with hash: %i.", pGroupToDelete->GetName().TryGetCStr(), (int)pGroupToDelete->GetName().GetHash());
	}
#endif

	//Clear all relationships of all pedtypes to mission groups
	const s32 iPoolSize = CRelationshipGroup::GetPool()->GetSize();
	for( s32 i = 0; i < iPoolSize; i++ )
	{
		CRelationshipGroup* pGroup = CRelationshipGroup::GetPool()->GetSlot(i);
		if( pGroup && pGroup != pGroupToDelete)
		{
			pGroup->ClearRelationship(pGroupToDelete);
		}
	}

	//Clear all of the relationships for the group that's being deleted
	pGroupToDelete->ClearAllRelationships();

	delete pGroupToDelete;

	// Remap all peds with missing relatinoship groups to default
	CPed::Pool *pool = CPed::GetPool();
	const s32 iPedPoolSize=pool->GetSize();
	for( s32 i = 0; i < iPedPoolSize; i++ )
	{
		CPed* pPed = pool->GetSlot(i);
		if( pPed )
		{
			pPed->GetPedIntelligence()->FixRelationshipGroup();
			Assert(pPed->GetPedIntelligence()->GetRelationshipGroup());
		}
	}
}

#if __DEV
void CRelationshipManager::PrintRelationshipGroups()
{
	Displayf("\n=== RELATIONSHIP GROUPS ===\n");

	s32 iPoolSize = CRelationshipGroup::GetPool()->GetSize();
	for( s32 i = 0; i < iPoolSize; i++ )
	{
		CRelationshipGroup* pGroup = CRelationshipGroup::GetPool()->GetSlot(i);
		if( pGroup )
		{
			Displayf("%s - %i", pGroup->GetName().GetCStr(), (int)pGroup->GetName().GetHash());
		}
	}
}
#endif
