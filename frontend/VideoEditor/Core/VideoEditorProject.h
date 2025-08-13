/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : VideoEditorProject.h
// PURPOSE : Represents instance data used for editing a given video project file.
//
// AUTHOR  : james.strain
//
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////
#include "control/replay/ReplaySettings.h"

#if defined( GTA_REPLAY ) && GTA_REPLAY

#ifndef VIDEO_EDITOR_PROJECT_H_
#define VIDEO_EDITOR_PROJECT_H_

// rage
#include "atl/hashstring.h"

// game
#include "control/replay/ReplayMarkerInfo.h"
#include "replaycoordinator/ReplayEditorParameters.h"
#include "replaycoordinator/storage/Clip.h"
#include "replaycoordinator/storage/Montage.h"
#include "frontend/VideoEditor/Core/VideoEditorHelpers.h"

namespace rage
{
	class atString;
	class grcTexture;
}

class CImposedImageHandler;
class CVideoEditorInterface;
class CTextOverlayHandle;
class CMusicClipHandle;
class IReplayMarkerStorage;

class CVideoEditorProject
{
public: // declarations and variables

	enum eEDIT_LIMITS
	{
		MAX_MARKERS_PER_CLIP = CMontage::MAX_MARKERS_PER_CLIP,
	};

public: // methods

	// Project level functions
	static size_t ApproximateMinimumSizeForSaving();
	static size_t ApproximateMaximumSizeForSaving();
	size_t ApproximateSizeForSaving() const;

	static bool	IsSpaceAvailableForSaving( char const * const szFilename, size_t const expectedSize );
	bool		IsSpaceAvailableForSaving( char const * const szFilename );
	bool		IsSpaceAvailableForSaving();

	bool		IsSpaceToAddClip();
	bool		IsSpaceToAddMarker();
	bool		IsSpaceToAddText();
    bool		IsSpaceToAddMusic();
    bool		IsSpaceToAddAmbientTrack();

	u64 SaveProject( const char *szFilename, bool const showSpinnerWithText = true );
	bool Validate();
	bool AreAllClipFilesValid();

	bool SetName( const char* szName );
	const char *GetName();

	u32 GetRating();
	u32 GetLongestTrackId();

	//! General Accessors
	bool IsInitialized() const { return m_montage != NULL; }

	//! Clip Functions

	//! Prepares the staging clip and loads it's thumbnail
	bool PrepareStagingClip( char const * const clipName, u64 const ownerId,
		ReplayMarkerTransitionType startTransition = MARKER_TRANSITIONTYPE_NONE, 
		ReplayMarkerTransitionType endTransition = MARKER_TRANSITIONTYPE_NONE );
	bool PrepareStagingClip( u32 const sourceIndex );

	rage::atFinalHashString GetStagingClipTextureName() const;
	bool GetStagingClipTextureName( atString& out_string ) const;

	bool IsStagingClipTextureRequested() const;
	bool IsStagingClipTextureLoaded() const;
	bool IsStagingClipTextureLoadFailed() const;

	//! Adds the staging clip to the actual project at the given index
	bool MoveStagingClipToProject( u32 const destIndex );

	//! Clean up the staging clip, ensuring we unload it's thumbnail
	void InvalidateStagingClip();

	bool BackupClipSettings( u32 const sourceIndex );
	void RestoreBackupToClip( u32 const destIndex );
	void InvalidateBackupClip();

    //! Marker Clipboard
    void CopyMarker( u32 const clipIndex, u32 const markerIndex, eMarkerCopyContext const copyContext );
    void PasteMarker( u32 const clipIndex, u32 const markerIndex, eMarkerCopyContext const copyContext );
    bool HasMarkerOnClipboard( eMarkerCopyContext const copyContext ) const;
    void ClearMarkerClipboard( eMarkerCopyContext const copyContext = COPY_CONTEXT_ALL );

	//! Adds a thumbnail directly at the given location, no staging and no thumbnail loading
	bool AddClip(  u64 const ownerId, char const * const clipName, u32 const clipIndex, 
		ReplayMarkerTransitionType startTransition = MARKER_TRANSITIONTYPE_NONE, 
		ReplayMarkerTransitionType endTransition = MARKER_TRANSITIONTYPE_NONE );

	s32	GetClipCount() const;
	s32 GetMaxClipCount() const;
	inline bool CanAddClip() const { return GetClipCount() < GetMaxClipCount(); }

	IReplayMarkerStorage* GetClipMarkers( u32 const clipIndex );
	IReplayMarkerStorage const* GetClipMarkers( u32 const clipIndex ) const;

	float GetAnchorTimeMs( u32 anchorIndex ) const;
	u32 GetAnchorCount() const;
	s32 GetNearestAnchor( const float uStartPosMs, const float uEndPosMs ) const;

	float GetClipStartNonDilatedTimeMs( u32 const clipIndex ) const;
	float GetClipStartTimeMs( u32 const clipIndex ) const;

	float GetClipEndNonDilatedTimeMs( u32 const clipIndex ) const;
	float GetClipEndTimeMs( u32 const clipIndex ) const;

	float GetTrimmedNonDilatedTimeToClipMs( u32 const clipIndex ) const;
	float GetTrimmedTimeToClipMs( u32 const clipIndex ) const;

	float GetClipNonDilatedDurationMs( u32 const clipIndex ) const;
	float GetClipDurationTimeMs( u32 const clipIndex ) const;

	float GetTotalClipTimeMs() const;
	float GetMaxDurationMs() const;

	float ConvertClipNonDilatedTimeMsToClipTimeMs( u32 const clipIndex, float const clipTimeMs ) const;
	float ConvertTimeMsToClipNonDilatedTimeMs( u32 const clipIndex, float const realTimeMs ) const;
	float ConvertPlaybackPercentageToClipNonDilatedTimeMs( u32 const clipIndex, float const playbackPercent ) const;
	float ConvertClipPercentageToClipNonDilatedTimeMs( u32 const clipIndex, float const clipPercent ) const;
	float ConvertClipNonDilatedTimeMsToClipPercentage( u32 const clipIndex, float const nonDilatedTimeMs ) const;
	float GetRawEndTimeAsTimeMs( s32 const clipIndex ) const;
	float GetRawEndTimeMs( s32 const clipIndex ) const;

	char const * GetClipName( u32 const clipIndex ) const;
	u64 GetClipOwnerId( u32 const clipIndex ) const;
	bool IsClipReferenceValid( u32 const clipIndex ) const;
	static bool IsValidClipFile( u64 const ownerId, char const * const filename );

	void DeleteClip( u32 const clipIndex );

	void ConvertProjectPlaybackPercentageToClipIndexAndClipNonDilatedTimeMs( float const playbackPercent, u32& out_clipIndex, float& out_clipTimeMs ) const;

	//! Clip Transition Functions
	void ValidateTransitionDuration( u32 const uIndex );

	ReplayMarkerTransitionType GetStartTransition( u32 const clipIndex );
	void SetStartTransition( u32 const clipIndex, ReplayMarkerTransitionType const transition );

	float GetStartTransitionDurationMs( u32 const clipIndex ) const;
	void SetStartTransitionDurationMs( u32 const clipIndex, float const durationMs );

	ReplayMarkerTransitionType GetEndTransition( u32 const clipIndex );
	void SetEndTransition( u32 const clipIndex, ReplayMarkerTransitionType const transition );

	float GetEndTransitionDurationMs( u32 const clipIndex ) const;
	void SetEndTransitionDurationMs( u32 const clipIndex, float const durationMs );

	//! Text Overlay Functions
	CTextOverlayHandle AddOverlayText( u32 const startTimeMs, u32 const durationMs );

	u32 GetOverlayTextCount() const;
	u32 GetMaxOverlayTextCount() const;
	inline bool CanAddText() const { return GetOverlayTextCount() < GetMaxOverlayTextCount(); }

	CTextOverlayHandle GetOverlayText( s32 const index );
	void DeleteOverlayText( s32 const index );

	//! Music Clip Functions
	CMusicClipHandle AddMusicClip( u32 const soundHash, u32 const startTimeMs );

	u32 GetMusicClipCount() const;
	u32 GetMusicClipMusicOnlyCount() const;
	u32 GetMusicClipCommercialsOnlyCount() const;
	u32 GetMusicClipScoreOnlyCount() const;
	u32 GetMaxMusicClipCount() const;
	inline bool CanAddMusic() const { return GetMusicClipCount() < GetMaxMusicClipCount(); }
	bool DoesClipContainMusicClipWithBeatMarkers( u32 const clipIndex ) const;

	CMusicClipHandle GetMusicClip( s32 const index );
	void DeleteMusicClip( s32 const index );

	u32 GetHashOfFirstInvalidMusicTrack() const;

	//! Ambient Clip Functions
	CAmbientClipHandle AddAmbientClip( u32 const soundHash, u32 const startTimeMs );

	u32 GetAmbientClipCount() const;
	u32 GetMaxAmbientClipCount() const;
	inline bool CanAddAmbient() const { return GetAmbientClipCount() < GetMaxAmbientClipCount(); }

	CAmbientClipHandle GetAmbientClip( s32 const index );
	void DeleteAmbientClip( s32 const index );

	//! Clip Thumbnail Functions
	bool RequestClipThumbnail( u32 const clipIndex );
	bool IsClipThumbnailRequested( u32 const clipIndex );
	bool IsClipThumbnailLoaded( u32 const clipIndex );
	bool HasClipThumbnailLoadFailed( u32 const clipIndex );
	grcTexture const* GetClipThumbnailTexture(  u32 const clipIndex );

	rage::atFinalHashString getClipTextureName( u32 const clipIndex ) const;
	bool getClipTextureName( u32 const clipIndex, atString& out_string ) const;

	void CaptureBespokeThumbnailForClip( u32 const clipIndex );
	bool DoesClipHaveBespokeThumbnail( u32 const clipIndex ) const;
	bool DoesClipHavePendingBespokeCapture( u32 const clipIndex ) const;

	void ReleaseClipThumbnail( u32 const clipIndex );
	void ReleaseAllClipThumbnails();

	u32 GetTimeSpent();

    u32 GetCachedProjectVersion() const;
    u32 GetCachedClipVersion() const;
    u32 GetCachedReplayVersion() const;
    u32 GetCachedGameVersion() const;
    u32 GetCachedTextVersion() const;
    u32 GetCachedMusicVersion() const;
    u32 GetCachedMarkerVersion() const;

private: // declarations and variables
	friend class CVideoEditorInterface; // for private construction

	enum eLoadState
	{
		LOADSTATE_INVALID,
		LOADSTATE_PENDING_LOAD,
		LOADSTATE_FAILED,
		LOADSTATE_LOADED
	};

	static char const * const sc_projectTextureNameFormat;
	
	CClip					m_stagingClip;
	CClip					m_backupClip;
    sReplayMarkerInfo       m_markerClipboard;
	CImposedImageHandler&	m_imageHandler;
	CMontage*				m_montage;
    u32                     m_markerClipboardMask;
	eLoadState				m_loadstate;
    
private: // methods
	
	CVideoEditorProject( CImposedImageHandler& imageHandler );
	~CVideoEditorProject();

	bool InitializeNew( char const * const name );

	bool StartLoad( char const * const path );
	bool IsLoaded() const;
	bool IsPendingLoad() const;
	bool HasLoadFailed() const;
	u64 GetProjectLoadExtendedResultData() const;

	bool InitializeFromMontage( CMontage* montage );

	void update();
	void Shutdown();

	void RequestStagingClipThumbnail();
	void ReleaseStagingClipThumbnail();

	inline CMontage* GetMontage() { return m_montage; }

	char const* GetBespokeThumbnailPath( u32 const clipIndex ) const;
	bool DoesStagingClipHaveBespokeThumbnail() const;

	void UpdateBespokeThumbnailCaptures();
};

#endif // VIDEO_EDITOR_PROJECT_H_

#endif // defined( GTA_REPLAY ) && GTA_REPLAY
