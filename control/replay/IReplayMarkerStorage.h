/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : IReplayMarkerStorage.h
// PURPOSE : Interface for access and manipulating replay markers
//
// AUTHOR  : james.strain
//
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////
#include "control/replay/ReplaySettings.h"

#ifndef INC_I_REPLAY_MARKER_STORAGE_H_
#define INC_I_REPLAY_MARKER_STORAGE_H_

#if GTA_REPLAY

// game
#include "ReplayMarkerInfo.h"

//! Preference on how to prioritize closest marker searches
enum eClosestMarkerPreference
{
    CLOSEST_MARKER_ANY,		        // No preference, just pick which is closest
    CLOSEST_MARKER_LESS,	        // We want the markers _before_ the given time
    CLOSEST_MARKER_LESS_OR_EQUAL,	// We want the markers _before_ or equal to the given time
    CLOSEST_MARKER_MORE,	        // We want markers _after_ the given time
	CLOSEST_MARKER_MORE_OR_EQUAL	// We want markers _after_ or equal to the given time
};

class IReplayMarkerStorage
{
public: // declarations and variables
	enum eEditRestrictionReason
	{
		EDIT_RESTRICTION_FIRST = 0,
		EDIT_RESTRICTION_NONE = EDIT_RESTRICTION_FIRST,
		EDIT_RESTRICTION_FIRST_PERSON,
		EDIT_RESTRICTION_CUTSCENE,
		EDIT_RESTRICTION_CAMERA_BLOCKED,
		EDIT_RESTRICTION_DOF_DISABLED,
		EDIT_RESTRICTION_IS_ANCHOR,
		EDIT_RESTRICTION_IS_ENDPOINT,
		EDIT_RESTRICTION_REQUIRES_CAMERA_BLEND,

		EDIT_RESTRICTION_MAX
	};

    

public: // methods
	inline bool IsValidMarkerIndex( s32 const index ) { return index >= 0 && index < GetMarkerCount(); }

    virtual void CopyMarkerData( u32 const index, sReplayMarkerInfo& out_destMarker ) = 0;
    virtual void PasteMarkerData( u32 const indexToPasteTo, sReplayMarkerInfo const&  sourceMarker, eMarkerCopyContext const context = COPY_CONTEXT_ALL ) = 0;

	virtual bool AddMarker( bool const sortMarkersAfterAdd, bool const inheritMarkerProperties, float const nonDilatedTimeMs, s32& out_Index ) = 0;
	virtual bool AddMarker( bool const sortMarkersAfterAdd, bool const inheritMarkerProperties, s32& out_Index ) = 0;
	virtual bool AddMarker( sReplayMarkerInfo* marker, bool const ignoreBoundary, bool const sortMarkersAfterAdd, bool const inheritMarkerProperties, s32& out_Index ) = 0;
	virtual void RemoveMarker( u32 const index ) = 0;

	virtual void ResetMarkerToDefaultState( u32 const markerIndex ) = 0;

	virtual bool CanPlaceMarkerAtNonDilatedTimeMs( float const nonDilatedTimeMs, bool const ignoreBoundary ) const = 0;
	virtual bool CanIncrementMarkerNonDilatedTimeMs( u32 const markerIndex, float const nonDilatedTimeDelta, float& inout_markerNonDilatedTime ) = 0;
	virtual bool GetMarkerNonDilatedTimeConstraintsMs( u32 const markerIndex, float& out_minNonDilatedTimeMs, float& out_maxNonDilatedTimeMs ) const = 0;
	virtual bool GetPotentialMarkerNonDilatedTimeConstraintsMs( u32 const markerIndex, float const nonDilatedTimeDelta, 
		float& out_minNonDilatedTimeMs, float& out_maxNonDilatedTimeMs ) = 0;

	virtual float UpdateMarkerNonDilatedTimeForMarkerDragMs( u32 const markerIndex, float const clipPlaybackPercentage ) = 0;
		
	virtual void SetMarkerNonDilatedTimeMs( u32 const markerIndex, float const newNonDilatedTimeMs ) = 0;
	virtual float GetMarkerDisplayTimeMs( u32 const markerIndex ) const = 0;
	virtual void IncrememtMarkerSpeed( u32 const markerIndex, s32 const delta ) = 0;
	virtual float GetMarkerFloatSpeedAtCurrentNonDilatedTimeMs( float const nonDilatedTimeMs ) const = 0;
	virtual u32 GetMarkerSpeedAtCurrentNonDilatedTimeMs( float const nonDilatedTimeMs ) const = 0;

	virtual void RecalculateMarkerPositionsOnSpeedChange( u32 const startingMarkerIndex ) = 0;
	virtual void RemoveMarkersOutsideOfRangeNonDilatedTimeMS( float const startNonDilatedTimeMs, float const endNonDilatedTimeMs ) = 0;

	virtual sReplayMarkerInfo* GetMarker( u32 const index ) = 0;
	virtual sReplayMarkerInfo const* GetMarker( u32 const index ) const = 0;
	virtual sReplayMarkerInfo* TryGetMarker( u32 const index ) = 0;
	virtual sReplayMarkerInfo const* TryGetMarker( u32 const index ) const = 0;

	virtual sReplayMarkerInfo* GetFirstMarker() = 0;
	virtual sReplayMarkerInfo const* GetFirstMarker() const = 0;
	virtual sReplayMarkerInfo* GetLastMarker() = 0;
	virtual sReplayMarkerInfo const* GetLastMarker() const = 0;

	virtual s32 GetPrevMarkerIndexWrapAround( u32 const startingIndex, float const startNonDilatedTimeMs = 0, float const endNonDilatedTimeMs = 0 ) const = 0;
	virtual s32 GetNextMarkerIndexWrapAround( u32 const startingIndex, float const startNonDilatedTimeMs = 0, float const endNonDilatedTimeMs = 0 ) const = 0;

	virtual sReplayMarkerInfo* GetPrevMarkerWrapAround( u32 const startingIndex, float const startNonDilatedTimeMs = 0, float const endNonDilatedTimeMs = 0 ) = 0;
	virtual sReplayMarkerInfo* GetNextMarkerWrapAround( u32 const startingIndex, float const startNonDilatedTimeMs = 0, float const endNonDilatedTimeMs = 0 ) = 0;

	virtual s32 GetNextMarkerIndex( u32 const startingIndex, bool const ignoreAnchors = false ) const = 0;
	virtual sReplayMarkerInfo* GetNextMarker( u32 const startingIndex, bool const ignoreAnchors = false ) = 0;

	virtual s32 GetMarkerCount() const = 0;
	virtual s32 GetMaxMarkerCount() const = 0;

	virtual s32 GetFirstValidMarkerIndex( float const nonDilatedTimeMs ) const = 0;
	virtual s32 GetLastValidMarkerIndex( float const nonDilatedTimeMs ) const = 0;
	virtual sReplayMarkerInfo* GetFirstValidMarker( float const nonDilatedTimeMs ) = 0;
	virtual sReplayMarkerInfo* GetLastValidMarker( float const nonDilatedTimeMs ) = 0;

	virtual s32 GetClosestMarkerIndex( float const nonDilatedTimeMs, eClosestMarkerPreference const preference = CLOSEST_MARKER_ANY, float* out_nonDilatedTimeDiffMs = NULL, sReplayMarkerInfo const* markerToIgnore = NULL, bool const ignoreAnchors = false ) const = 0;
	virtual sReplayMarkerInfo* GetClosestMarker( float const nonDilatedTimeMs, eClosestMarkerPreference const preference = CLOSEST_MARKER_ANY, float* out_nonDilatedTimeDiffMs = NULL, sReplayMarkerInfo const* markerToIgnore = NULL, bool const ignoreAnchors = false ) = 0;

	virtual float GetAnchorNonDilatedTimeMs( u32 const index ) const = 0;
	virtual float GetAnchorTimeMs( u32 const index ) const = 0;
	virtual s32 GetAnchorCount() const = 0;

	virtual eEditRestrictionReason IsMarkerControlEditable( eMarkerControls const controlType ) const = 0;

	virtual void BlendTypeUpdatedOnMarker( u32 const index ) = 0;

	static inline char const * const GetEditRestrictionLngKey( eEditRestrictionReason const reason )
	{
		static char const * const s_restrictionStrings[ EDIT_RESTRICTION_MAX ] =
		{
			"", "VEUI_EDIT_DIS_FPCAM", "VEUI_EDIT_DIS_CUTSCENE", "VEUI_EDIT_DIS_CAMBLOCK", "VEUI_DOF_DISABLED",
			"VEUI_IS_ANCHOR", "VEUI_IS_ENDPOINT", "VEUI_NEEDS_BLEND"
		};

		char const * const lngKey = reason >= EDIT_RESTRICTION_FIRST && reason < EDIT_RESTRICTION_MAX ? 
			s_restrictionStrings[ reason ] : "";

		return lngKey;
	}
};

#endif // GTA_REPLAY

#endif // INC_I_REPLAY_MARKER_STORAGE_H_
