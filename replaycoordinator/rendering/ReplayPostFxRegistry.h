/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : ReplayPostFxRegistry.h
// PURPOSE : Class for registering/accessing replay PostFX data
//
// AUTHOR  : james.strain
//
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////
#include "control/replay/ReplaySettings.h"

//#if GTA_REPLAY

#ifndef REPLAY_POSTFX_REGISTRY_H_
#define REPLAY_POSTFX_REGISTRY_H_

// rage
#include "atl/array.h"
#include "parser/macros.h"

// game
#include "ReplayPostFxData.h"

class CReplayPostFxRegistry
{
public: // methods
	bool ContainsEffect( char const * const name );

	u32 GetEffectCount() { return (u32)m_fxData.size(); }
	u32 GetPreviousFilter( u32 const index );
	u32 GetNextFilter( u32 const index );
	CReplayPostFxData const* GetEffect( u32 const index );
	u32 GetEffectNameHash( u32 const index );
	u32 GetEffectUiNameHash( u32 const index );

	u32 ChooseRandomEffectIndex();

private: // declarations and variables
	typedef atArray< CReplayPostFxData > ReplayFxDataCollection;
	ReplayFxDataCollection m_fxData;

	PAR_SIMPLE_PARSABLE;
};

#endif // REPLAY_POSTFX_REGISTRY_H_

//#endif // GTA_REPLAY
