/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : ReplayMarkerContext.cpp
// PURPOSE : Context for manipulating and accessing replay markers markers. 
//
// AUTHOR  : james.strain
//
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////
#include "ReplayMarkerContext.h"

#if GTA_REPLAY

REPLAY_COORDINATOR_OPTIMISATIONS();

s32 CReplayMarkerContext::ms_currentMarkerIndex									= -1;
s32 CReplayMarkerContext::ms_editingMarkerIndex									= -1;
s32 CReplayMarkerContext::ms_previousActiveMarkerIndex							= -1;
float CReplayMarkerContext::ms_jumpingOverrideTimeMs							= -1.f;
CReplayMarkerContext::eEditWarnings CReplayMarkerContext::ms_lastEditWarning	= CReplayMarkerContext::EDIT_WARNING_NONE;

bool CReplayMarkerContext::ms_shiftingCurrentMarker						= false;
bool CReplayMarkerContext::ms_forceCameraShake							= false;
bool CReplayMarkerContext::ms_cameraMouseOverride						= false;

IReplayMarkerStorage* CReplayMarkerContext::ms_currentMarkerStorage		= NULL;

// String keys to display on the ui
char const * const CReplayMarkerContext::sc_warningTextKeys[ CReplayMarkerContext::EDIT_WARNING_MAX ] = 
{
	"VEUI_WARN_MAX_RANGE", "VEUI_WARN_OO_RANGE", "VEUI_WARN_COLLISION", "VEUI_WARN_BLEND_COLLISION", "VEUI_WARN_LOS_COLLISION"
};

void CReplayMarkerContext::Reset()
{
	ms_currentMarkerIndex = -1;
	ms_editingMarkerIndex = -1;
	ms_previousActiveMarkerIndex = -1;
	ms_jumpingOverrideTimeMs = -1;

	ms_shiftingCurrentMarker = false;
	ms_forceCameraShake = false;

	ms_currentMarkerStorage = NULL;

	ResetWarning();
}

void CReplayMarkerContext::ResetWarning()
{
	ms_lastEditWarning = CReplayMarkerContext::EDIT_WARNING_NONE;
}

char const * CReplayMarkerContext::GetWarningLngKey( eEditWarnings const warning )
{
	char const * const lngKey = warning >= EDIT_WARNING_FIRST && warning < EDIT_WARNING_MAX ? 
		sc_warningTextKeys[ warning ] : "";

	return lngKey;
}

#endif // GTA_REPLAY
