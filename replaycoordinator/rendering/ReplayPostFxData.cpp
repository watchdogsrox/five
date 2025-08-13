/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : ReplayPostFxData.cpp
// PURPOSE : Container for data about a given replay Post Effect
//
// AUTHOR  : james.strain
//
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////
#include "ReplayPostFxData.h"

//#if GTA_REPLAY

#include "replaycoordinator/replay_coordinator_channel.h"

REPLAY_COORDINATOR_OPTIMISATIONS();

CReplayPostFxData::CReplayPostFxData()
	: m_name()
{

}

CReplayPostFxData::~CReplayPostFxData()
{

}

bool CReplayPostFxData::IsValid() const
{
	return m_name.IsNotNull();
}

void CReplayPostFxData::Shutdown()
{
	m_name.Clear();
}

//#endif // GTA_REPLAY
