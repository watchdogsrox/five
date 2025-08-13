//
// weapons/TargetSequence.h
//
// Copyright (C) 1999-2011 Rockstar Games. All Rights Reserved.
//

#ifndef WEAPONS_TARGETSEQUENCE_H
#define WEAPONS_TARGETSEQUENCE_H

#define USE_TARGET_SEQUENCES 0

#if USE_TARGET_SEQUENCES
#	define TARGET_SEQUENCE_ONLY(...)  __VA_ARGS__
#else
#	define TARGET_SEQUENCE_ONLY(...)
#endif

#if USE_TARGET_SEQUENCES

// framework
#include "fwanimation/boneids.h"

// game 
#include "atl/array.h"
#include "atl/hashstring.h"
#include "data/base.h"
#include "fwsys/timer.h"
#include "parser/macros.h"
#include "vector/vector3.h"
#include "animation/anim_channel.h"


class CPed;
        
/////////////////////////////////////////////////////////////////////////////////////////
/// CTargetSequenceEntry
/////////////////////////////////////////////////////////////////////////////////////////
    
class CTargetSequenceEntry : public ::rage::datBase
{
public:
    CTargetSequenceEntry();                 // Default constructor
    virtual ~CTargetSequenceEntry();        // Virtual destructor
    
	eAnimBoneTag GetTargetBone() const
	{
		return m_TargetBone;
	}

	const Vector3& GetOffset() const
	{
		return m_Offset;
	}

	float GetShotDelay() const
	{
		return m_ShotDelay;
	}

private:
    eAnimBoneTag           m_TargetBone;
    ::rage::Vector3        m_Offset;
    float                  m_ShotDelay;
    
    PAR_PARSABLE;
};

        
/////////////////////////////////////////////////////////////////////////////////////////
/// CTargetSequence
/////////////////////////////////////////////////////////////////////////////////////////
    
class CTargetSequence : public ::rage::datBase
{
public:
    CTargetSequence();                 // Default constructor
    virtual ~CTargetSequence();        // Virtual destructor
    
	const CTargetSequenceEntry* GetEntry (s32 index) const
	{
		if (index>=0 && index<m_Entries.GetCount())
		{
			return &m_Entries[index];
		}
		return NULL;
	}

	s32 GetNumEntries() const
	{
		return m_Entries.GetCount();
	}

	void GetTargetPosition(CPed* pTargetPed, Vector3& position, s32 shotIdx) const;

private:
    ::rage::atArray< CTargetSequenceEntry, 0, ::rage::u16>         m_Entries;
    
    PAR_PARSABLE;
};


/////////////////////////////////////////////////////////////////////////////////////////
/// CTargetSequenceGroup
/////////////////////////////////////////////////////////////////////////////////////////


class CTargetSequenceGroup : public ::rage::datBase
{
public:
	CTargetSequenceGroup();                 // Default constructor
	virtual ~CTargetSequenceGroup();        // Virtual destructor

	atHashString GetId() const { return m_Id; }

	const CTargetSequence* GetSequence(u32 index) const
	{ 
		if (index<m_Sequences.GetCount())
		{
			return &m_Sequences[index];
		}
		return NULL;
	}

	u32 GetNumSequences() const { return m_Sequences.GetCount();} 

private:
	::rage::atArray< CTargetSequence, 0, ::rage::u16>         m_Sequences;
	atHashString	m_Id;

	PAR_PARSABLE;
};


/////////////////////////////////////////////////////////////////////////////////////////
/// CTargetSequenceManager
/////////////////////////////////////////////////////////////////////////////////////////

    
class CTargetSequenceManager : public ::rage::datBase
{
public:
    CTargetSequenceManager();                 // Default constructor
    virtual ~CTargetSequenceManager();        // Virtual destructor
    
	static void InitMetadata(u32 initMode);
	static void ShutdownMetadata(u32 shutdownMode);

	void Reset(){ m_Groups.Reset(); }

	const CTargetSequenceGroup* FindGroup(u32 groupHash) const
	{ 
		for (s32 i=0; i<m_Groups.GetCount(); i++)
		{
			if (m_Groups[i].GetId().GetHash()==groupHash)
			{
				return &m_Groups[i];
			}
		}
		return NULL;
	}

private:

    ::rage::atArray< CTargetSequenceGroup, 0, ::rage::u16>         m_Groups;
    
    PAR_PARSABLE;
};

extern CTargetSequenceManager g_WeaponTargetSequenceManager;

//////////////////////////////////////////////////////////////////////////
/// CTargetSequenceHelper
//////////////////////////////////////////////////////////////////////////

class CTargetSequenceHelper
{
public:
	CTargetSequenceHelper()
		: m_SequenceTime(0.0f)
		, m_ShotCount(0)
		, m_pCurrentSequence(NULL)
		, m_CurrentTargetPosition(VEC3_ZERO)
	{
	}

	~CTargetSequenceHelper()
	{

	}

	bool InitSequence(atHashWithStringNotFinal sequenceGroup, CPed* pFiringPed, CPed* pTargetPed);

	void Reset()
	{
		m_SequenceTime=0.0f;
		m_ShotCount=0;
		m_pCurrentSequence=NULL;
	}

	bool HasSequence() const { return m_pCurrentSequence!=NULL && m_ShotCount<m_pCurrentSequence->GetNumEntries(); }

	bool IsSequenceFinished() const { return m_pCurrentSequence!=NULL && m_ShotCount==(u32)m_pCurrentSequence->GetNumEntries(); }
	// PURPOSE: Call once per frame to update the sequence time (used for delaying targets)
	void UpdateTime()
	{
		m_SequenceTime+=fwTimer::GetTimeStep();
	}

	// PURPOSE: Call every time a shot is fired to increment the target sequence
	void IncrementShotCount()
	{
		m_ShotCount++;
		m_SequenceTime = 0.0f;
	}

	bool IsReadyToFire() const
	{
		if (HasSequence())
		{
			return m_ShotCount==0 || (m_ShotCount<m_pCurrentSequence->GetNumEntries() && m_SequenceTime > m_pCurrentSequence->GetEntry(m_ShotCount)->GetShotDelay());
		}
		return true;
	}

	void ForceShotReady()
	{
		if (HasSequence())
		{
			m_SequenceTime = m_pCurrentSequence->GetEntry(m_ShotCount)->GetShotDelay();
		}
	}

	void UpdateTargetPosition(CPed* pTargetPed, Vector3& position);

	void GetTargetPosition(Vector3& position) const
	{
		position = m_CurrentTargetPosition;
	}

private:

	float m_SequenceTime;
	u32 m_ShotCount;
	const CTargetSequence* m_pCurrentSequence;
	Vector3 m_CurrentTargetPosition;
};

#endif //USE_TARGET_SEQUENCES


#endif
