/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : ReplayPostFxData.h
// PURPOSE : Container for data about a given replay Post Effect
//
// AUTHOR  : james.strain
//
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////
#include "control/replay/ReplaySettings.h"

//#if GTA_REPLAY

#ifndef REPLAY_POSTFX_DATA_H_
#define REPLAY_POSTFX_DATA_H_

// rage
#include "atl/hashstring.h"
#include "parser/macros.h"

class CReplayPostFxData
{
public: // methods
	CReplayPostFxData();
	~CReplayPostFxData();

	bool IsValid() const;
	void Shutdown();

	inline u32 GetNameHash() const { return m_name.GetHash(); }
	inline u32 GetUiNameHash() const { return m_uiName.GetHash(); }

private: // declarations and variables
	atHashWithStringNotFinal m_name;
	atHashWithStringNotFinal m_uiName;

	PAR_SIMPLE_PARSABLE;
};

#endif // REPLAY_POSTFX_DATA_H_

//#endif // GTA_REPLAY
