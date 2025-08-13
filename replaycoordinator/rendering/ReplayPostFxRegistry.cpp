/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : ReplayPostFxRegistry.cpp
// PURPOSE : Class for registering/accessing replay PostFX data
//
// AUTHOR  : james.strain
//
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////
#include "ReplayPostFxRegistry.h"
#include "ReplayPostFxRegistry_parser.h"


//#if GTA_REPLAY

#include "replaycoordinator/replay_coordinator_channel.h"

REPLAY_COORDINATOR_OPTIMISATIONS();

bool CReplayPostFxRegistry::ContainsEffect( char const * const name ) 
{
	bool result = false;

	if( name )
	{
		atHashValue hashedName( name );

		for( ReplayFxDataCollection::const_iterator itr = m_fxData.begin(); itr != m_fxData.end(); ++itr )
		{
			CReplayPostFxData const& fxData =(*itr);
			if( fxData.GetNameHash() == hashedName.GetHash() )
			{
				result = true;
				break;
			}
		}
	}

	return result;
}

u32 CReplayPostFxRegistry::GetPreviousFilter( u32 const index )
{
	u32 prevFilter = index == 0 ?  GetEffectCount() -1 : index - 1;
	return prevFilter;
}

u32 CReplayPostFxRegistry::GetNextFilter( u32 const index )
{
	u32 nextFilter = index + 1 >= GetEffectCount() ? 0 : index + 1;
	return nextFilter;
}

CReplayPostFxData const* CReplayPostFxRegistry::GetEffect( u32 const index )
{
	REPLAY_ONLY(rcAssertf( index < GetEffectCount(), "CReplayPostFxRegistry::GetEffect - Index out of range!" ));
	return index < GetEffectCount() ? &m_fxData[ index ] : NULL;
}

u32 CReplayPostFxRegistry::GetEffectNameHash( u32 const index )
{
	CReplayPostFxData const* effect = GetEffect( index );
	return effect ? effect->GetNameHash() : 0;
}

u32 CReplayPostFxRegistry::GetEffectUiNameHash( u32 const index )
{
	CReplayPostFxData const* effect = GetEffect( index );
	return effect ? effect->GetUiNameHash() : 0;
}

u32 CReplayPostFxRegistry::ChooseRandomEffectIndex()
{
	return rand() % GetEffectCount();
}

//#endif // GTA_REPLAY


