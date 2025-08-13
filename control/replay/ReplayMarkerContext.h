/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : ReplayMarkerContext.h
// PURPOSE : Context for manipulating and accessing replay markers markers. 
//
// AUTHOR  : james.strain
//
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////
#include "control/replay/ReplaySettings.h"

#if GTA_REPLAY

#ifndef INC_REPLAY_MARKER_CONTEXT_H_
#define INC_REPLAY_MARKER_CONTEXT_H_

class IReplayMarkerStorage;

//=====================================================================================================
class CReplayMarkerContext
{
public: // declarations and variables

	enum eEditWarnings
	{
		EDIT_WARNING_NONE = -1,
		EDIT_WARNING_FIRST = 0,

		EDIT_WARNING_MAX_RANGE = EDIT_WARNING_FIRST,	// Free camera has hit max range whilst in edit mode
		EDIT_WARNING_OUT_OF_RANGE,						// Free camera was out of range and had to be "snapped" to a good position
		EDIT_WARNING_COLLISION,							// Free camera hit collision and had to be "snapped" to a good position
		EDIT_WARNING_BLEND_COLLISION,					// Free camera hit collision while blending
		EDIT_WARNING_LOS_COLLISION,						// Free camera had no line of sight to the move with target, and had to be "snapped" to a good position

		EDIT_WARNING_MAX
	};


public: // methods
	static void Reset();
	static void ResetWarning();

	static inline s32 GetCurrentMarkerIndex() { return ms_currentMarkerIndex; }
	static inline void SetCurrentMarkerIndex( s32 const currentMarkerIndex ) { ms_currentMarkerIndex = currentMarkerIndex; }

	static inline s32 GetEditingMarkerIndex() { return ms_editingMarkerIndex; }
	static inline void SetEditingMarkerIndex( s32 const editingMarkerIndex ) { ms_editingMarkerIndex = editingMarkerIndex; }

	static inline s32 GetPreviousActiveMarkerIndex() { return ms_previousActiveMarkerIndex; }
	static inline void SetPreviousActiveMarkerIndex( s32 const previousActiveMarkerIndex ) { ms_previousActiveMarkerIndex = previousActiveMarkerIndex; }

	static inline bool IsJumpingOverrideTimeSet() { return ms_jumpingOverrideTimeMs >= 0; }
	static inline float GetJumpingOverrideNonDilatedTimeMs() { return ms_jumpingOverrideTimeMs; }
	static inline void SetJumpingOverrideNonDilatedTimeMs( float const timeMs ) { ms_jumpingOverrideTimeMs = timeMs; }

	static inline bool IsShiftingCurrentMarker() { return ms_shiftingCurrentMarker; }
	static inline void SetShiftingCurrentMarker( bool const shiftingMarker ) { ms_shiftingCurrentMarker = shiftingMarker; }

	static inline bool ShouldForceCameraShake() { return ms_forceCameraShake; }
	static inline void SetForceCameraShake( bool const forceCameraShake ) { ms_forceCameraShake = forceCameraShake; }

	static inline IReplayMarkerStorage* GetMarkerStorage() { return ms_currentMarkerStorage; }
	static inline void SetCurrentMarkerStorage( IReplayMarkerStorage* currentMarkerStorage ) { ms_currentMarkerStorage = currentMarkerStorage; }

	static char const * GetWarningLngKey( eEditWarnings const warning );
	static inline char const * GetWarningLngKey() { return GetWarningLngKey( ms_lastEditWarning ); }

	static inline void SetWarning( eEditWarnings const warning ) { ms_lastEditWarning = warning; }
	static inline eEditWarnings GetWarning() { return ms_lastEditWarning; }
	static inline bool HasWarning() { return ms_lastEditWarning != EDIT_WARNING_NONE; }

	static inline bool IsCameraUsingMouse() { return ms_cameraMouseOverride; }
	static inline void SetCameraIsUsingMouse( bool const isUsingMouse ) { ms_cameraMouseOverride = isUsingMouse; }

private: // declarations and variables

	static s32						ms_currentMarkerIndex;
	static s32						ms_editingMarkerIndex;
	static s32						ms_previousActiveMarkerIndex;
	static float					ms_jumpingOverrideTimeMs;
	static eEditWarnings			ms_lastEditWarning;

	static bool ms_shiftingCurrentMarker;
	static bool ms_forceCameraShake;
	static bool ms_cameraMouseOverride;
	static IReplayMarkerStorage*	ms_currentMarkerStorage;

	static char const * const		sc_warningTextKeys[ EDIT_WARNING_MAX ];
};

#endif // INC_REPLAY_MARKER_CONTEXT_H_

#endif // GTA_REPLAY
