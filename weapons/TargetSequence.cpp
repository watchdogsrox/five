//
// weapons/TargetSequence.cpp
//
// Copyright (C) 1999-2011 Rockstar Games. All Rights Reserved.
//
#include "TargetSequence.h"


#if USE_TARGET_SEQUENCES

#include "TargetSequence_parser.h"
#include "Peds/ped.h"
#include "vector/matrix34.h"
    
    
//////////////////////////////////////////////
// CTargetSequenceEntry
//////////////////////////////////////////////
    
CTargetSequenceEntry::CTargetSequenceEntry()
: m_TargetBone((eAnimBoneTag)0)
, m_Offset(0.0f,  0.0f,  0.0f)
, m_ShotDelay(0.0f)
{
}

CTargetSequenceEntry::~CTargetSequenceEntry()
{
}

    
//////////////////////////////////////////////
// CTargetSequence
//////////////////////////////////////////////
    
CTargetSequence::CTargetSequence()
{
}

CTargetSequence::~CTargetSequence()
{
}

////////////////////////////////////////////////////////////////////////////////

void CTargetSequence::GetTargetPosition(CPed* pTargetPed, Vector3& position, s32 shotIdx) const
{
	const CTargetSequenceEntry* pTarget = GetEntry(shotIdx % GetNumEntries());
	if (pTarget && pTargetPed)
	{
		// grab the necessary bone position from the cache
		position = pTargetPed->GetBonePositionCached(pTarget->GetTargetBone());

		// Transform the offset into ped space and add it to the cached bone
		if(pTarget->GetOffset().Mag2()>0.0f)
		{
			Vector3 offset(pTarget->GetOffset());
			const Matrix34& pedMat = RCC_MATRIX34(pTargetPed->GetMatrixRef());
			pedMat.Transform3x3(offset);

			position+=offset;
		}
	}
}

//////////////////////////////////////////////
// CTargetSequenceGroup
//////////////////////////////////////////////

CTargetSequenceGroup::CTargetSequenceGroup()
{
}

CTargetSequenceGroup::~CTargetSequenceGroup()
{
}

    
//////////////////////////////////////////////
// CTargetSequenceManager
//////////////////////////////////////////////
    
CTargetSequenceManager::CTargetSequenceManager()
{
}

CTargetSequenceManager::~CTargetSequenceManager()
{
}

//////////////////////////////////////////////////////////////////////////

const char *g_targetSequenceDataDirectory = "common:/data/ai";

CTargetSequenceManager g_WeaponTargetSequenceManager;
//////////////////////////////////////////////////////////////////////////

void CTargetSequenceManager::InitMetadata( u32 mode )
{
	if(mode == INIT_SESSION)
	{
		ASSET.PushFolder(g_targetSequenceDataDirectory);
		PARSER.LoadObject("WeaponTargetSequences", "meta", g_WeaponTargetSequenceManager);
		ASSET.PopFolder();
	}
}

//////////////////////////////////////////////////////////////////////////

void CTargetSequenceManager::ShutdownMetadata( u32 mode )
{
	if (mode == SHUTDOWN_SESSION)
	{
		g_WeaponTargetSequenceManager.Reset();
	}
}

//////////////////////////////////////////////////////////////////////////
// CTargetSequenceHelper
//////////////////////////////////////////////////////////////////////////

bool CTargetSequenceHelper::InitSequence(atHashWithStringNotFinal sequenceGroup, CPed* UNUSED_PARAM(pFiringPed), CPed* UNUSED_PARAM(pTargetPed))
{
	Reset();

	const CTargetSequenceGroup* pGroup = g_WeaponTargetSequenceManager.FindGroup(sequenceGroup);

	if (pGroup)
	{
		// for now just pick a random sequence from the group
		// TODO - add flags to allow picking different sequences based on the situation (relative ped orientations / etc)?
		m_pCurrentSequence=pGroup->GetSequence(fwRandom::GetRandomNumberInRange(0, pGroup->GetNumSequences()));

		return m_pCurrentSequence!=NULL;
	}
	else
	{
		weaponAssertf(0, "Unable to find sequence group for id %s(%u)", sequenceGroup.TryGetCStr()? sequenceGroup.GetCStr() : "UNKNOWN", sequenceGroup.GetHash());
	}
	return false;

}

//////////////////////////////////////////////////////////////////////////

void CTargetSequenceHelper::UpdateTargetPosition(CPed* pTargetPed, Vector3& position)
{
	weaponAssertf(m_pCurrentSequence, "No current sequence!");
	if (m_pCurrentSequence)
	{
		m_pCurrentSequence->GetTargetPosition(pTargetPed, position, m_ShotCount);
		m_CurrentTargetPosition.Set(position);
	}
}

#endif // USE_TARGET_SEQUENCES