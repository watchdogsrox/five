/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : VideoEditorProject.cpp
// PURPOSE : Represents instance data used for editing a given video project file.
//
// AUTHOR  : james.strain
//
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////
#include "VideoEditorProject.h"

#if defined( GTA_REPLAY ) && GTA_REPLAY

// rage
#include "atl/string.h"
#include "string/string.h"

// framework
#include "fwlocalisation/templateString.h"

// game
#include "audio/radiotrack.h"
#include "control/replay/replay.h"
#include "frontend/ui_channel.h"
#include "frontend/VideoEditor/Core/ImposedImageHandler.h"
#include "frontend/VideoEditor/Core/VideoProjectAudioTrackProvider.h"
#include "frontend/VideoEditor/Core/VideoEditorUtil.h"
#include "replaycoordinator/ReplayMetadata.h"
#include "SaveLoad/savegame_photo_manager.h"

FRONTEND_OPTIMISATIONS();

typedef PathBuffer ThumbnailNameBuffer;

char const * const CVideoEditorProject::sc_projectTextureNameFormat = "clip_%p";
#define STAGING_CLIP_NAME ("StagingClipThumb")

size_t CVideoEditorProject::ApproximateMinimumSizeForSaving()
{
	return CMontage::ApproximateMinimumSizeForSaving();
}

size_t CVideoEditorProject::ApproximateMaximumSizeForSaving()
{
	return CMontage::ApproximateMaximumSizeForSaving();
}

size_t CVideoEditorProject::ApproximateSizeForSaving() const
{
	return IsInitialized() ? m_montage->ApproximateSizeForSaving() : 0;
}

bool CVideoEditorProject::IsSpaceAvailableForSaving( char const * const szFilename, size_t const expectedSize )
{
	return CMontage::IsSpaceAvailableForSaving( szFilename, expectedSize );
}

bool CVideoEditorProject::IsSpaceAvailableForSaving( char const * const szFilename )
{
	return IsInitialized() ? m_montage->IsSpaceAvailableForSaving( szFilename ) : false;
}

bool CVideoEditorProject::IsSpaceAvailableForSaving()
{
	return IsInitialized() ? m_montage->IsSpaceAvailableForSaving() : false;
}

bool CVideoEditorProject::IsSpaceToAddClip()
{
	return IsInitialized() ? m_montage->IsSpaceToAddClip() : false;
}

bool CVideoEditorProject::IsSpaceToAddMarker()
{
	return IsInitialized() ? m_montage->IsSpaceToAddMarker() : false;
}

bool CVideoEditorProject::IsSpaceToAddText()
{
	return IsInitialized() ? m_montage->IsSpaceToAddText() : false;
}

bool CVideoEditorProject::IsSpaceToAddMusic()
{
	return IsInitialized() ? m_montage->IsSpaceToAddMusic() : false;
}

bool CVideoEditorProject::IsSpaceToAddAmbientTrack()
{
    return IsInitialized() ? m_montage->IsSpaceToAddMusic() : false;
}

u64 CVideoEditorProject::SaveProject( const char* szFilename, bool const showSpinnerWithText )
{
	return IsInitialized() && CVideoEditorUtil::IsValidSaveName( szFilename, CMontage::MAX_NAME_SIZE_BYTES ) ? 
        m_montage->Save( szFilename, showSpinnerWithText ) : CReplayEditorParameters::PROJECT_SAVE_RESULT_FAILED;
}

bool CVideoEditorProject::Validate()
{
	return IsInitialized() ? m_montage->Validate() : false;
}

bool CVideoEditorProject::AreAllClipFilesValid()
{
	return IsInitialized() ? m_montage->AreAllClipFilesValid() : false;
}

bool CVideoEditorProject::SetName( const char* szName )
{
    bool result = false;

	if( uiVerifyf( IsInitialized(), "CVideoEditorProject::SetName - Not initialized!" ) && CVideoEditorUtil::IsValidSaveName( szName, CMontage::MAX_NAME_SIZE_BYTES ) )
	{
		m_montage->SetName(szName);
        result = true;
	}

    return result;
}

const char *CVideoEditorProject::GetName()
{
	return IsInitialized() ? m_montage->GetName() : NULL;
}

u32 CVideoEditorProject::GetRating()
{
	return IsInitialized() ? m_montage->GetRating() : 0;
}

u32 CVideoEditorProject::GetLongestTrackId()
{
	u32 trackId = 0;
	if(IsInitialized())
	{
		u32 maxLength = 0;
		for(int i=0; i<m_montage->GetMusicClipCount(); i++)
		{
			u32 length = m_montage->GetMusicDurationMs(i);
			if(length > maxLength)
			{
				maxLength = length;
				trackId = audRadioTrack::FindTextIdForSoundHash(m_montage->GetMusicClipSoundHash(i));
			}
		}
	}
	return trackId;
}

bool CVideoEditorProject::PrepareStagingClip( char const * const clipName, u64 const ownerId,
											 ReplayMarkerTransitionType startTransition,
											 ReplayMarkerTransitionType endTransition )
{
	bool success = false;

	if( uiVerifyf( IsInitialized(), "CVideoEditorProject::PrepareStagingClip - Not initialized!" ) )
	{
		InvalidateStagingClip();
		if( m_stagingClip.Populate( clipName, ownerId, startTransition, endTransition ) )
		{
			RequestStagingClipThumbnail();
			success = true;
		}
	}

	return success;
}

bool CVideoEditorProject::PrepareStagingClip( u32 const sourceIndex )
{
	bool success = false;

	if( uiVerifyf( IsInitialized(), "CVideoEditorProject::PrepareStagingClip - Not initialized!" ) )
	{
		InvalidateStagingClip();
		CClip* sourceClip = m_montage->GetClip( (s32)sourceIndex );
		if( sourceClip )
		{
			m_stagingClip = *sourceClip; 
			RequestStagingClipThumbnail();
			success = true;
		}
	}

	return success;
}

rage::atFinalHashString CVideoEditorProject::GetStagingClipTextureName() const
{
	atFinalHashString streamingName;

	atString nameString;
	if( GetStagingClipTextureName( nameString ) )
	{
		streamingName.SetFromString( nameString.c_str() );
	}

	return streamingName;
}

bool CVideoEditorProject::GetStagingClipTextureName( atString& out_string ) const
{
	bool success = false;

	if( m_stagingClip.IsValid() )
	{
		out_string = STAGING_CLIP_NAME;
		success = true;
	}

	return success;
}

bool CVideoEditorProject::IsStagingClipTextureRequested() const
{
	bool requested = false;
	if( m_stagingClip.IsValid() && IsInitialized() )
	{
		requested = m_imageHandler.isImageRequested( GetStagingClipTextureName() );
	}

	return requested;
}

bool CVideoEditorProject::IsStagingClipTextureLoaded() const
{
	bool loaded = false;
	if( m_stagingClip.IsValid() && IsInitialized() )
	{
		loaded = m_imageHandler.isImageLoaded( GetStagingClipTextureName() );
	}

	return loaded;
}

bool CVideoEditorProject::IsStagingClipTextureLoadFailed() const
{
	bool loaded = false;
	if( m_stagingClip.IsValid() && IsInitialized() )
	{
		loaded = m_imageHandler.hasImageRequestFailed( GetStagingClipTextureName() );
	}

	return loaded;
}

bool CVideoEditorProject::MoveStagingClipToProject( u32 const destIndex )
{
	bool success = false;

	if( uiVerifyf( IsInitialized(), "CVideoEditorProject::AddStagingClip - Not initialized!" ) &&
		uiVerifyf( m_stagingClip.IsValid(), "CVideoEditorProject::AddStagingClip - No valid staging clip!" ) )
	{
		m_montage->AddClipCopy( m_stagingClip, (s32)destIndex );
		InvalidateStagingClip();
	}

	return success;
}

void CVideoEditorProject::InvalidateStagingClip()
{
	if( m_stagingClip.IsValid() )
	{
		ReleaseStagingClipThumbnail();
		m_stagingClip.Clear();
	}
}

bool CVideoEditorProject::BackupClipSettings( u32 const sourceIndex )
{
	bool success = false;

	if( uiVerifyf( IsInitialized(), "CVideoEditorProject::BackupClipSettings - Not initialized!" ) )
	{
		InvalidateBackupClip();
		CClip* sourceClip = m_montage->GetClip( (s32)sourceIndex );
		if( sourceClip )
		{
			m_backupClip = *sourceClip; 
			success = true;
		}
	}

	return success;
}

void CVideoEditorProject::RestoreBackupToClip( u32 const destIndex )
{
	if( m_backupClip.IsValid() )
	{
		m_montage->SetClipCopy( m_backupClip, (s32)destIndex );
		InvalidateBackupClip();
	}
}

void CVideoEditorProject::InvalidateBackupClip()
{
	m_backupClip.Clear();
}

void CVideoEditorProject::CopyMarker( u32 const clipIndex, u32 const markerIndex, eMarkerCopyContext const copyContext )
{
    CClip const * const c_clip = IsInitialized() ? m_montage->GetClip( clipIndex ) : NULL;
    sReplayMarkerInfo const * const c_marker = c_clip ? c_clip->TryGetMarker( markerIndex ) : NULL;
    if( c_marker )
    {
        m_markerClipboard.CopyData( *c_marker, copyContext );
        m_markerClipboardMask |= copyContext;
    }
}

void CVideoEditorProject::PasteMarker( u32 const clipIndex, u32 const markerIndex, eMarkerCopyContext const copyContext )
{
    CClip * const clip = IsInitialized() && HasMarkerOnClipboard( copyContext ) ? m_montage->GetClip( clipIndex ) : NULL;
    sReplayMarkerInfo * const marker = clip ? clip->TryGetMarker( markerIndex ) : NULL;
    if( marker )
    {
        marker->CopyData( m_markerClipboard, copyContext );
    }
}

bool CVideoEditorProject::HasMarkerOnClipboard( eMarkerCopyContext const copyContext ) const
{
    return ( m_markerClipboardMask & copyContext ) != 0;
}

void CVideoEditorProject::ClearMarkerClipboard( eMarkerCopyContext const copyContext )
{
    m_markerClipboardMask &= ~copyContext;
}

bool CVideoEditorProject::AddClip( u64 const ownerId, char const * const clipName, u32 const clipIndex, 
								  ReplayMarkerTransitionType startTransition , ReplayMarkerTransitionType endTransition )
{
	bool success = false;

	uiAssertf( IsInitialized(), "CVideoEditorProject::AddClip - Not initialized!" );
	uiAssertf( clipName, "CVideoEditorProject::AddClip - Null clip name!" );
	if( IsInitialized() && clipName && IsValidClipFile( ownerId, clipName ) )
	{
		success = m_montage->AddClip( clipName, ownerId, (s32)clipIndex, startTransition, endTransition ) != NULL;
	}

	return success;
}

s32 CVideoEditorProject::GetClipCount() const
{
	return IsInitialized() ? m_montage->GetClipCount() : -1;
}

s32 CVideoEditorProject::GetMaxClipCount() const
{
	return IsInitialized() ? m_montage->GetMaxClipCount() : -1;
}

IReplayMarkerStorage* CVideoEditorProject::GetClipMarkers( u32 const clipIndex )
{
	IReplayMarkerStorage* markerStorage = NULL;

	uiAssertf( clipIndex < GetClipCount(), "CVideoEditorProject::GetClipMarkers - Index out of bounds!" );
	if( IsInitialized() && clipIndex < GetClipCount() )
	{
		markerStorage = m_montage->GetClip( clipIndex );
	}

	return markerStorage;
}

IReplayMarkerStorage const* CVideoEditorProject::GetClipMarkers( u32 const clipIndex ) const
{
	IReplayMarkerStorage const* markerStorage = NULL;

	uiAssertf( clipIndex <= GetClipCount(), "CVideoEditorProject::GetClipMarkers - Index out of bounds!" );
	if( IsInitialized() && clipIndex < GetClipCount() )
	{
		markerStorage = m_montage->GetClip( clipIndex );
	}

	return markerStorage;
}

float CVideoEditorProject::GetAnchorTimeMs( u32 anchorIndex ) const
{
	float timeMs = 0;

	for( u32 clipIndex = 0; clipIndex < GetClipCount(); ++clipIndex )
	{
		IReplayMarkerStorage const* markerStorage = GetClipMarkers( clipIndex );
		u32 const c_clipAnchorCount = markerStorage ? markerStorage->GetAnchorCount() : 0;

		if( anchorIndex >= c_clipAnchorCount )
		{
			// Anchor not relative to this clip, add the time of this clip and move to the next
			timeMs += GetClipDurationTimeMs( clipIndex );

			anchorIndex -= c_clipAnchorCount;
		}
		else
		{
			float const c_anchorTimeMs = markerStorage ? markerStorage->GetAnchorTimeMs( anchorIndex ) : 0;
			timeMs += c_anchorTimeMs;
			break;
		}
	}

	return timeMs;
}

u32 CVideoEditorProject::GetAnchorCount() const
{
	u32 count = 0;
	
	for( u32 clipIndex = 0; clipIndex < GetClipCount(); ++clipIndex )
	{
		IReplayMarkerStorage const* markerStorage = GetClipMarkers( clipIndex );
		count += markerStorage ? markerStorage->GetAnchorCount() : 0;
	}

	return count;
}

s32 CVideoEditorProject::GetNearestAnchor( const float uStartPosMs, const float uEndPosMs ) const
{
	s32 iAnchorFound = -1;
	float uAnchorFoundPosMs = 0;

	const float uStart = (uEndPosMs >= uStartPosMs) ? uStartPosMs : uEndPosMs;
	const float uEnd = (uEndPosMs >= uStartPosMs) ? uEndPosMs : uStartPosMs;

	for (s32 iCount = 0; iCount < GetAnchorCount(); iCount++)
	{
		const float uAnchorTime = GetAnchorTimeMs((u32)iCount);

		if (uAnchorTime > uStart && uAnchorTime < uEnd) // found an anchor between the times
		{
			// anchor is nearer to the start point than any previously found (or the 1st one we have found)
			if ( (iAnchorFound == -1) ||
				(uEndPosMs >= uStartPosMs && uAnchorTime < uAnchorFoundPosMs) ||
				(uEndPosMs <= uStartPosMs && uAnchorTime > uAnchorFoundPosMs) )
			{
				uAnchorFoundPosMs = uAnchorTime;  // use it for checks in next iteration 
				iAnchorFound = iCount;
			}
		}
	}

	return (iAnchorFound);  // return the anchor we found (or -1 if we didnt find one)
}

float CVideoEditorProject::GetClipStartNonDilatedTimeMs( u32 const clipIndex ) const
{
	float const c_time = ( IsInitialized() && clipIndex < GetClipCount() ) ?
		m_montage->GetClip( (int)clipIndex )->GetStartNonDilatedTimeMs() : 0;

	return c_time;
}

float CVideoEditorProject::GetClipStartTimeMs( u32 const clipIndex ) const
{
	float const c_time = ( IsInitialized() && clipIndex < GetClipCount() ) ?
		m_montage->GetClip( (int)clipIndex )->GetStartTimeMs() : 0;

	return c_time;
}

float CVideoEditorProject::GetClipEndNonDilatedTimeMs( u32 const clipIndex ) const
{
	float const c_time = ( IsInitialized() && clipIndex < GetClipCount() ) ?
		m_montage->GetClip( (int)clipIndex )->GetEndNonDilatedTimeMs() : 0;

	return c_time;
}

float CVideoEditorProject::GetClipEndTimeMs( u32 const clipIndex ) const
{
	float const c_time = ( IsInitialized() && clipIndex < GetClipCount() ) ?
		m_montage->GetClip( (int)clipIndex )->GetEndTimeMs() : 0;

	return c_time;
}

float CVideoEditorProject::GetTrimmedNonDilatedTimeToClipMs( u32 const clipIndex ) const
{
	float time = 0;

	uiAssertf( clipIndex < GetClipCount(), "CVideoEditorProject::GetTrimmedTimeToClipMs - Index out of bounds!" );
	if( IsInitialized() && clipIndex < GetClipCount() )
	{
		time = m_montage->GetTrimmedNonDilatedTimeToClipMs( (s32)clipIndex );
	}

	return time;
}

float CVideoEditorProject::GetTrimmedTimeToClipMs( u32 const clipIndex ) const
{
	float time = 0;

	uiAssertf( clipIndex < GetClipCount(), "CVideoEditorProject::GetTrimmedTimeToClipMs - Index out of bounds!" );
	if( IsInitialized() && clipIndex < GetClipCount() )
	{
		time = m_montage->GetTrimmedTimeToClipMs( (s32)clipIndex );
	}

	return time;
}

float CVideoEditorProject::GetClipNonDilatedDurationMs( u32 const clipIndex ) const
{
	float time = 0;

	if( IsInitialized() && clipIndex < GetClipCount() )
	{
		CClip* sourceClip = m_montage->GetClip( (s32)clipIndex );
		time = sourceClip->GetTrimmedNonDilatedTimeMs();
	}

	return time;
}

float CVideoEditorProject::GetClipDurationTimeMs( u32 const clipIndex ) const
{
	float time = 0;

	if( IsInitialized() && clipIndex < GetClipCount() )
	{
		CClip* sourceClip = m_montage->GetClip( (s32)clipIndex );
		time = sourceClip->GetTrimmedTimeMs();
	}

	return time;
}

float CVideoEditorProject::GetTotalClipTimeMs() const
{
	float const c_time = ( IsInitialized() ) ? m_montage->GetTotalClipLengthTimeMs() : 0.f;
	return c_time;
}

float CVideoEditorProject::GetMaxDurationMs() const
{
	float const c_time = ( IsInitialized() ) ? m_montage->GetMaxDurationMs() : 0.f;
	return c_time;
}

float CVideoEditorProject::ConvertClipNonDilatedTimeMsToClipTimeMs( u32 const clipIndex, float const clipTimeMs ) const
{
	float const c_time = IsInitialized() ? m_montage->ConvertClipNonDilatedTimeMsToClipTimeMs( clipIndex, clipTimeMs ) : 0;
	return c_time;
}

float CVideoEditorProject::ConvertTimeMsToClipNonDilatedTimeMs( u32 const clipIndex, float const realTimeMs ) const
{
	float const c_time = IsInitialized() ? m_montage->ConvertTimeMsToClipNonDilatedTimeMs( clipIndex, realTimeMs ) : 0;
	return c_time;
}

float CVideoEditorProject::ConvertPlaybackPercentageToClipNonDilatedTimeMs( u32 const clipIndex, float const playbackPercent ) const
{
	float const c_time = IsInitialized() ? m_montage->ConvertPlaybackPercentageToClipNonDilatedTimeMs( clipIndex, playbackPercent ) : 0;
	return c_time;
}

float CVideoEditorProject::ConvertClipPercentageToClipNonDilatedTimeMs( u32 const clipIndex, float const clipPercent ) const
{
	float const c_time = IsInitialized() ? m_montage->ConvertClipPercentageToClipNonDilatedTimeMs( clipIndex, clipPercent ) : 0;
	return c_time;
}

float CVideoEditorProject::ConvertClipNonDilatedTimeMsToClipPercentage( u32 const clipIndex, float const nonDilatedTimeMs ) const
{
	float const c_time = IsInitialized() ? m_montage->ConvertClipNonDilatedTimeMsToClipPercentage( clipIndex, nonDilatedTimeMs ) : 0;
	return c_time;
}

float CVideoEditorProject::GetRawEndTimeAsTimeMs( s32 const clipIndex ) const
{
	float const c_time = IsInitialized() ? m_montage->GetRawEndTimeAsTimeMs( clipIndex ) : 0;
	return c_time;
}

float CVideoEditorProject::GetRawEndTimeMs( s32 const clipIndex ) const
{
	float const c_time = IsInitialized() ? m_montage->GetRawEndTimeMs( clipIndex ) : 0;
	return c_time;
}

char const * CVideoEditorProject::GetClipName( u32 const clipIndex ) const
{
	char const * result = NULL;

	if( IsInitialized() && clipIndex < GetClipCount() )
	{
		result = m_montage->GetClip( (int)clipIndex )->GetName();
	}

	return result;
}

u64 CVideoEditorProject::GetClipOwnerId( u32 const clipIndex ) const
{
	u64 result = 0;

	if( IsInitialized() && clipIndex < GetClipCount() )
	{
		result = m_montage->GetClip( (int)clipIndex )->GetOwnerId();
	}

	return result;
}

bool CVideoEditorProject::IsClipReferenceValid( u32 const clipIndex ) const
{
	bool const c_isValid = IsInitialized() && clipIndex < GetClipCount() ? m_montage->IsValidClipReference( (s32)clipIndex ) : false;
	return c_isValid;
}

bool CVideoEditorProject::IsValidClipFile( u64 const ownerId, char const * const filename )
{
	bool const c_isValid = CMontage::IsValidClipFile( ownerId, filename );
	return c_isValid;
}

void CVideoEditorProject::DeleteClip( u32 const clipIndex )
{
	if( clipIndex < GetClipCount() )
	{
		m_montage->DeleteClip( (s32)clipIndex );
	}
}

void CVideoEditorProject::ConvertProjectPlaybackPercentageToClipIndexAndClipNonDilatedTimeMs( float const playbackPercent, u32& out_clipIndex, float& out_clipTimeMs ) const
{
	if( IsInitialized() )
	{
		out_clipIndex = 0;
		out_clipTimeMs = 0;

		float const c_totalRealTimeMs = GetTotalClipTimeMs();

		int const c_clipCount = GetClipCount();
		float currentPercent = 0.f;
		float clipLocalPercent = playbackPercent;

		for( int index = 0; index < c_clipCount && currentPercent < playbackPercent; ++index )
		{
			float const c_currentRealTimeMs = GetClipDurationTimeMs( index );

			float const c_realTimeDeltaMs =  float( c_currentRealTimeMs - out_clipTimeMs );

			float const c_percentDelta = c_realTimeDeltaMs / c_totalRealTimeMs;
			float const c_nextPercent = currentPercent + c_percentDelta;

			if( c_nextPercent >= playbackPercent )
			{
				float const c_percentLeftToCalculate = c_percentDelta - ( c_nextPercent - playbackPercent );
				clipLocalPercent = c_percentLeftToCalculate / c_percentDelta;
				out_clipIndex = index;
			}

			currentPercent = c_nextPercent;
		}

		float const c_clipLocalTimeMs = ConvertPlaybackPercentageToClipNonDilatedTimeMs( out_clipIndex, clipLocalPercent );
		out_clipTimeMs = c_clipLocalTimeMs;
	}
}

void CVideoEditorProject::ValidateTransitionDuration( u32 const uIndex )
{
	if( uiVerifyf( IsInitialized(), "CVideoEditorProject::GetMinTransitionDurationMs - Not initialized!" ) )
	{
		m_montage->ValidateTransitionDuration( uIndex );
	}
}

ReplayMarkerTransitionType CVideoEditorProject::GetStartTransition( u32 const clipIndex )
{
	uiAssertf( IsInitialized(), "CVideoEditorProject::GetStartTransition - Not initialized!" );
	return IsInitialized() ? m_montage->GetStartTransition( clipIndex ) : MARKER_TRANSITIONTYPE_NONE;
}

void CVideoEditorProject::SetStartTransition( u32 const clipIndex, ReplayMarkerTransitionType const transition )
{
	uiAssertf( clipIndex < GetClipCount(), "CVideoEditorProject::SetClipStartTransition - Index out of bounds!" );
	if( clipIndex < GetClipCount() )
	{
		m_montage->GetClip( (s32)clipIndex )->SetStartTransition( transition );
	}
}

float CVideoEditorProject::GetStartTransitionDurationMs( u32 const clipIndex ) const
{
	uiAssertf( IsInitialized(), "CVideoEditorProject::GetStartTransitionDurationMs - Not initialized!" );
	return IsInitialized() ? m_montage->GetStartTransitionDurationMs( clipIndex ) : 0;
}

void CVideoEditorProject::SetStartTransitionDurationMs( u32 const clipIndex, float const durationMs )
{
	uiAssertf( clipIndex < GetClipCount(), "CVideoEditorProject::SetStartTransitionDurationMs - Index out of bounds!" );
	if( clipIndex < GetClipCount() )
	{
		m_montage->GetClip( (s32)clipIndex )->SetStartTransitionDurationMs( durationMs );
	}
}

ReplayMarkerTransitionType CVideoEditorProject::GetEndTransition( u32 const clipIndex )
{
	uiAssertf( IsInitialized(), "CVideoEditorProject::GetStartTransition - Not initialized!" );
	return IsInitialized() ? m_montage->GetEndTransition( clipIndex ) : MARKER_TRANSITIONTYPE_NONE;
}

void CVideoEditorProject::SetEndTransition( u32 const clipIndex, ReplayMarkerTransitionType const transition )
{
	uiAssertf( clipIndex < GetClipCount(), "CVideoEditorProject::SetClipEndTransition - Index out of bounds!" );
	if( clipIndex < GetClipCount() )
	{
		m_montage->GetClip( (s32)clipIndex )->SetEndTransition( transition );
	}
}

float CVideoEditorProject::GetEndTransitionDurationMs( u32 const clipIndex ) const
{
	uiAssertf( IsInitialized(), "CVideoEditorProject::GetEndTransitionDurationMs - Not initialized!" );
	return IsInitialized() ? m_montage->GetEndTransitionDurationMs( clipIndex ) : 0;
}

void CVideoEditorProject::SetEndTransitionDurationMs( u32 const clipIndex, float const durationMs )
{
	uiAssertf( clipIndex < GetClipCount(), "CVideoEditorProject::SetEndTransitionDurationMs - Index out of bounds!" );
	if( clipIndex < GetClipCount() )
	{
		m_montage->GetClip( (s32)clipIndex )->SetEndTransitionDurationMs( durationMs );
	}
}

CTextOverlayHandle CVideoEditorProject::AddOverlayText( u32 const startTimeMs, u32 const durationMs )
{
	CTextOverlayHandle resultHandle;

	uiAssertf( IsInitialized(), "CVideoEditorProject::AddText - Not initialized!" );
	if( IsInitialized() )
	{
		CMontageText* newText = m_montage->AddText( startTimeMs, durationMs );
		if( newText )
		{
			s32 index = m_montage->GetTextIndex( newText );
			resultHandle.Initialize( m_montage, index );
		}
	}

	return resultHandle;
}

u32 CVideoEditorProject::GetOverlayTextCount() const
{
	u32 const c_count = IsInitialized() ? m_montage->GetTextCount() : 0;
	return c_count;
}

u32 CVideoEditorProject::GetMaxOverlayTextCount() const
{
	u32 const c_count = IsInitialized() ? m_montage->GetMaxTextCount() : 0;
	return c_count;
}

CTextOverlayHandle CVideoEditorProject::GetOverlayText( s32 const index )
{
	CTextOverlayHandle resultHandle;

	uiAssertf( IsInitialized(), "CVideoEditorProject::GetOverlayText - Not initialized!" );
	if( IsInitialized() )
	{
		if( m_montage->IsValidTextIndex( index ) )
		{
			resultHandle.Initialize( m_montage, index );
		}
	}

	return resultHandle;
}

void CVideoEditorProject::DeleteOverlayText( s32 const index )
{
	uiAssertf( IsInitialized(), "CVideoEditorProject::DeleteOverlayText - Not initialized!" );
	if( IsInitialized() )
	{
		m_montage->DeleteText( index );
	}
}

CMusicClipHandle CVideoEditorProject::AddMusicClip( u32 const soundHash, u32 const startTimeMs )
{
	CMusicClipHandle resultHandle;

	uiAssertf( IsInitialized(), "CVideoEditorProject::AddMusicClip - Not initialized!" );
	if( IsInitialized() )
	{
		s32 clipIndex = m_montage->AddMusicClip( soundHash, startTimeMs );
		if( clipIndex != INDEX_NONE )
		{
			resultHandle.Initialize( m_montage, clipIndex );
			CVideoProjectAudioTrackProvider::UpdateTracksUsedInProject( soundHash, true );
		}
	}

	return resultHandle;
}

u32 CVideoEditorProject::GetMusicClipCount() const
{
	u32 const c_count = IsInitialized() ? m_montage->GetMusicClipCount() : 0;
	return c_count;
}

u32 CVideoEditorProject::GetMusicClipMusicOnlyCount() const
{
	u32 const c_count = IsInitialized() ? m_montage->GetMusicClipMusicOnlyCount() : 0;
	return c_count;
}

u32 CVideoEditorProject::GetMusicClipCommercialsOnlyCount() const
{
	u32 const c_count = IsInitialized() ? m_montage->GetMusicClipCommercialsOnlyCount() : 0;
	return c_count;
}

u32 CVideoEditorProject::GetMusicClipScoreOnlyCount() const
{
	u32 const c_count = IsInitialized() ? m_montage->GetMusicClipScoreOnlyCount() : 0;
	return c_count;
}

u32 CVideoEditorProject::GetMaxMusicClipCount() const
{
	u32 const c_count = IsInitialized() ? m_montage->GetMaxMusicClipCount() : 0;
	return c_count;
}

bool CVideoEditorProject::DoesClipContainMusicClipWithBeatMarkers( u32 const clipIndex ) const
{
	bool const c_doesContainMusicClip = IsInitialized() ? m_montage->DoesClipContainMusicClipWithBeatMarkers( clipIndex ) : false;
	return c_doesContainMusicClip;
}

CMusicClipHandle CVideoEditorProject::GetMusicClip( s32 const index )
{
	CMusicClipHandle resultHandle;

	uiAssertf( IsInitialized(), "CVideoEditorProject::GetMusicClip - Not initialized!" );
	if( IsInitialized() )
	{
		if( m_montage->IsValidMusicClipIndex( index ) )
		{
			resultHandle.Initialize( m_montage, index );
		}
	}

	return resultHandle;
}

void CVideoEditorProject::DeleteMusicClip( s32 const index )
{
	if( uiVerifyf( IsInitialized(), "CVideoEditorProject::DeleteMusicClip - Not initialized!" ) )
	{
		u32 const c_soundHash = m_montage->GetMusicClipSoundHash( index );
		CVideoProjectAudioTrackProvider::UpdateTracksUsedInProject( c_soundHash, false );
		m_montage->DeleteMusicClip( index );
	}
}

u32 CVideoEditorProject::GetHashOfFirstInvalidMusicTrack() const
{
	u32 const c_firstHash = uiVerifyf( IsInitialized(), "CVideoEditorProject::DeleteMusicClip - Not initialized!" ) ?
		m_montage->GetHashOfFirstInvalidMusicTrack() : 0;

	return c_firstHash;
}

// Ambient track
CAmbientClipHandle CVideoEditorProject::AddAmbientClip( u32 const soundHash, u32 const startTimeMs )
{
	CAmbientClipHandle resultHandle;

	uiAssertf( IsInitialized(), "CVideoEditorProject::AddAmbientClip - Not initialized!" );
	if( IsInitialized() )
	{
		s32 clipIndex = m_montage->AddAmbientClip( soundHash, startTimeMs );
		if( clipIndex != INDEX_NONE )
		{
			resultHandle.Initialize( m_montage, clipIndex );
		}
	}

	return resultHandle;
}

u32 CVideoEditorProject::GetAmbientClipCount() const
{
	u32 const c_count = IsInitialized() ? m_montage->GetAmbientClipCount() : 0;
	return c_count;
}

u32 CVideoEditorProject::GetMaxAmbientClipCount() const
{
	u32 const c_count = IsInitialized() ? m_montage->GetMaxAmbientClipCount() : 0;
	return c_count;
}

CAmbientClipHandle CVideoEditorProject::GetAmbientClip( s32 const index )
{
	CAmbientClipHandle resultHandle;

	uiAssertf( IsInitialized(), "CVideoEditorProject::GetAmbientClip - Not initialized!" );
	if( IsInitialized() )
	{
		if( m_montage->IsValidAmbientClipIndex( index ) )
		{
			resultHandle.Initialize( m_montage, index );
		}
	}

	return resultHandle;
}

void CVideoEditorProject::DeleteAmbientClip( s32 const index )
{
	if( uiVerifyf( IsInitialized(), "CVideoEditorProject::DeleteAmbientClip - Not initialized!" ) )
	{
		m_montage->DeleteAmbientClip( index );
	}
}


bool CVideoEditorProject::RequestClipThumbnail( u32 const clipIndex )
{
	bool succeeded = false;

	uiAssertf( clipIndex < GetClipCount(), "CVideoEditorProject::RequestClipThumbnail - Index out of bounds!" );
	if( clipIndex < GetClipCount() )
	{
		ThumbnailNameBuffer pathBuffer;

		if( DoesClipHaveBespokeThumbnail( clipIndex ) )
		{
			pathBuffer.Set( GetBespokeThumbnailPath( clipIndex ) );
		}
		else
		{
			char const * clipName = GetClipName( clipIndex );
			uiAssertf( clipName, "Unable to get clip name for clip %u", clipIndex );
			if( clipName )
			{
				u64 const c_ownerId = GetClipOwnerId( clipIndex );
				char path[RAGE_MAX_PATH] = { 0 };
				CVideoEditorUtil::getClipDirectoryForUser( c_ownerId, path );
				pathBuffer.sprintf( "%s%s%s", path, clipName, THUMBNAIL_FILE_EXTENSION );
			}
		}

		if( !pathBuffer.IsNullOrEmpty() )
		{
			succeeded = m_imageHandler.requestImage( pathBuffer, getClipTextureName( clipIndex ), VideoResManager::DOWNSCALE_ONE );
		}
	}

	return succeeded;
}

bool CVideoEditorProject::IsClipThumbnailRequested( u32 const clipIndex )
{
	bool isRequested = false;

	if( clipIndex < GetClipCount() )
	{
		isRequested = m_imageHandler.isImageRequested( getClipTextureName( clipIndex ) );
	}

	return isRequested;
}

bool CVideoEditorProject::IsClipThumbnailLoaded( u32 const clipIndex )
{
	bool isLoaded = false;

	if( clipIndex < GetClipCount() )
	{
		isLoaded = m_imageHandler.isImageLoaded( getClipTextureName( clipIndex ) );
	}

	return isLoaded;
}

bool CVideoEditorProject::HasClipThumbnailLoadFailed( u32 const clipIndex )
{
	bool loadFailed = false;

	if( clipIndex < GetClipCount() )
	{
		loadFailed = m_imageHandler.hasImageRequestFailed( getClipTextureName( clipIndex ) );
	}

	return loadFailed;
}

grcTexture const* CVideoEditorProject::GetClipThumbnailTexture( u32 const clipIndex )
{
	grcTexture const * thumbnail = NULL;

	uiAssertf( clipIndex < GetClipCount(), "CVideoEditorProject::IsClipThumbnailLoaded - Index out of bounds!" );
	if( clipIndex < GetClipCount() )
	{
		thumbnail = m_imageHandler.getTexture( getClipTextureName( clipIndex ) );
	}

	return thumbnail;
}

rage::atFinalHashString CVideoEditorProject::getClipTextureName( u32 const clipIndex ) const
{
	atFinalHashString streamingName;

	atString nameString;
	if( getClipTextureName( clipIndex, nameString ) )
	{
		streamingName.SetFromString( nameString.c_str() );
	}

	return streamingName;
}

bool CVideoEditorProject::getClipTextureName( u32 const clipIndex, atString& out_string ) const
{
	bool success = false;
	out_string.Reset();

	if( uiVerifyf( clipIndex < GetClipCount(), "CVideoEditorProject::getClipTextureName - Index out of bounds!" ) )
	{
		ThumbnailNameBuffer nameBuffer;

		void const* clip = (void*)m_montage->GetClip( clipIndex );
		if( uiVerifyf( clip, "Unable to get clip %u", clipIndex ) )
		{
			nameBuffer.sprintf( sc_projectTextureNameFormat, clip );
		}

		if( !nameBuffer.IsNullOrEmpty() )
		{
			out_string = nameBuffer.getBuffer();
			success = true;
		}
	}

	return success;
}

void CVideoEditorProject::CaptureBespokeThumbnailForClip( u32 const clipIndex )
{
	if( uiVerifyf( clipIndex < GetClipCount(), "CVideoEditorProject::CaptureBespokeThumbnailForClip - Index out of bounds!" ) )
	{
		bool const c_wasThumbnailRequested = IsClipThumbnailRequested( clipIndex );
		if( c_wasThumbnailRequested )
		{
			ReleaseClipThumbnail( clipIndex );
		}

		CClip* clip = IsInitialized() && clipIndex < GetClipCount() ? m_montage->GetClip( (s32)clipIndex ) : NULL;
		if( clip )
		{
			clip->CaptureBespokeThumbnail();
		}
	}
}

bool CVideoEditorProject::DoesClipHaveBespokeThumbnail( u32 const clipIndex ) const
{
	CClip const* c_clip = IsInitialized() && clipIndex < GetClipCount() ? m_montage->GetClip( (s32)clipIndex ) : NULL;
	bool const c_hasBespoke = c_clip ? c_clip->HasBespokeThumbnail() : false;

	return c_hasBespoke;
}

bool CVideoEditorProject::DoesClipHavePendingBespokeCapture( u32 const clipIndex ) const
{
	CClip const* c_clip = IsInitialized() && clipIndex < GetClipCount() ? m_montage->GetClip( (s32)clipIndex ) : NULL;
	bool const c_hasBespokePending = c_clip ? c_clip->HasBespokeCapturePending() : false;

	return c_hasBespokePending;
}

void CVideoEditorProject::ReleaseClipThumbnail( u32 const clipIndex )
{
	if( uiVerifyf( clipIndex < GetClipCount(), "CVideoEditorProject::ReleaseClipThumbnail - Index out of bounds!" ) )
	{
		m_imageHandler.releaseImage( getClipTextureName( clipIndex ) );
	}
}

void CVideoEditorProject::ReleaseAllClipThumbnails()
{
	int const c_clipCount = GetClipCount();
	for( int index = 0; index < c_clipCount; ++index )
	{
		ReleaseClipThumbnail( index );
	}
}

CVideoEditorProject::CVideoEditorProject( CImposedImageHandler& imageHandler )
	: m_stagingClip()
	, m_backupClip()
	, m_imageHandler( imageHandler )
	, m_montage( NULL )
    , m_markerClipboardMask( 0 )
	, m_loadstate( LOADSTATE_INVALID )
{

}

CVideoEditorProject::~CVideoEditorProject()
{

}

bool CVideoEditorProject::InitializeNew( char const * const name )
{
	Shutdown();

	uiAssertf( name, "CVideoEditorProject::InitializeNew - Invalid name for project!" );
	if( name )
	{
		m_montage = CMontage::Create();
		uiAssertf( m_montage, "CVideoEditorProject::InitializeNew - Unable to create new montage!" );
		if( m_montage )
		{
			m_montage->SetName( name );
			m_loadstate = LOADSTATE_LOADED;
            ClearMarkerClipboard();
		}
	}

	return IsInitialized();
}

bool CVideoEditorProject::StartLoad( char const * const path )
{
	bool result = false;

	Shutdown();

	uiAssertf( path, "CVideoEditorProject::InitializeLoad - Invalid path to load from!" );
	if( path )
	{
		m_montage = CMontage::Create();
		uiAssertf( m_montage, "CVideoEditorProject::InitializeLoad - Unable to create new montage!" );
		if( m_montage )
		{
			result = CReplayMgr::StartLoadMontage( path, m_montage );
			m_loadstate = result ? LOADSTATE_PENDING_LOAD : LOADSTATE_FAILED;
		}
	}

	return result;
}

bool CVideoEditorProject::IsLoaded() const
{
	return m_loadstate == LOADSTATE_LOADED;
}

bool CVideoEditorProject::IsPendingLoad() const
{
	return m_loadstate == LOADSTATE_PENDING_LOAD;
}

bool CVideoEditorProject::HasLoadFailed() const
{
	return m_loadstate == LOADSTATE_FAILED;
}

u64 CVideoEditorProject::GetProjectLoadExtendedResultData() const
{
	u64 const c_resultData = ( m_loadstate == LOADSTATE_LOADED || m_loadstate == LOADSTATE_FAILED ) ?
		CReplayMgr::GetOpExtendedResultData() : 0;

	return c_resultData;
}

bool CVideoEditorProject::InitializeFromMontage( CMontage* montage )
{
	Shutdown();

	uiAssertf( montage, "CVideoEditorProject::InitializeFromMontage - NULL montage provided" );
	if( montage )
	{
		m_montage = montage;

		//! Filter any invalid tracks out, just to be safe!
		CVideoProjectAudioTrackProvider::RemoveInvalidTracksAndUpdateUsage( *m_montage );

        m_loadstate = LOADSTATE_LOADED;
        ClearMarkerClipboard();
	}

	return IsInitialized();
}

void CVideoEditorProject::update()
{
	if( m_loadstate == LOADSTATE_PENDING_LOAD )
	{
		bool result = false;
		bool const c_callSucceeded = CReplayMgr::CheckLoadMontage( result );

		if( c_callSucceeded )
		{
			if( result )
			{
				//! Filter any invalid tracks out, just to be safe!
				CVideoProjectAudioTrackProvider::RemoveInvalidTracksAndUpdateUsage( *m_montage );
                m_loadstate = LOADSTATE_LOADED;
                ClearMarkerClipboard();
			}
			else
			{
				m_loadstate = LOADSTATE_FAILED;
			}
		}
	}
}

void CVideoEditorProject::Shutdown()
{
	if( IsInitialized() )
	{
        ReleaseStagingClipThumbnail();
        ReleaseAllClipThumbnails();
		InvalidateStagingClip();
		CMontage::Destroy( m_montage );
		m_loadstate = LOADSTATE_INVALID;
	}
}

void CVideoEditorProject::RequestStagingClipThumbnail()
{
	if( !IsStagingClipTextureRequested() && m_stagingClip.IsValid() )
	{
		ThumbnailNameBuffer pathBuffer;

		if( DoesStagingClipHaveBespokeThumbnail() )
		{
			pathBuffer.Set( m_stagingClip.GetBespokeThumbnailFileName() );
		}
		else
		{
			char const * clipName = m_stagingClip.GetName();
			
			uiAssertf( clipName, "CVideoEditorProject::RequestStagingClipThumbnail - Unable to get clip name" );
			if( clipName )
			{
				char path[RAGE_MAX_PATH] = { 0 };
				CVideoEditorUtil::getClipDirectoryForUser( m_stagingClip.GetOwnerId(), path );
				pathBuffer.sprintf( "%s%s%s", path, clipName, THUMBNAIL_FILE_EXTENSION );
			}
		}

		if( !pathBuffer.IsNullOrEmpty() )
		{
			m_imageHandler.requestImage( pathBuffer, GetStagingClipTextureName(), VideoResManager::DOWNSCALE_QUARTER );
		}
	}
}

void CVideoEditorProject::ReleaseStagingClipThumbnail()
{
	if( IsStagingClipTextureRequested() )
	{
		m_imageHandler.releaseImage( GetStagingClipTextureName() );
	}
}

char const* CVideoEditorProject::GetBespokeThumbnailPath( u32 const clipIndex ) const
{
	char const * c_path = IsInitialized() && clipIndex < GetClipCount() ? 
		m_montage->GetClip( (s32)clipIndex )->GetBespokeThumbnailFileName() : NULL;

	return c_path;
}

bool CVideoEditorProject::DoesStagingClipHaveBespokeThumbnail() const
{
	bool const c_hasBespoke = m_stagingClip.HasBespokeThumbnail();
	return c_hasBespoke;
}

void CVideoEditorProject::UpdateBespokeThumbnailCaptures()
{
	if( IsInitialized() && m_loadstate == LOADSTATE_LOADED )
	{
		m_montage->UpdateBespokeThumbnailCaptures();
	}
}

u32 CVideoEditorProject::GetTimeSpent()
{
	if( IsInitialized() )
	{
		return m_montage->UpdateTimeSpentEditingMs();
	}
	return 0;
}

u32 CVideoEditorProject::GetCachedProjectVersion() const
{
    u32 const c_version = m_montage ? m_montage->GetCachedMontageVersion() : 0;
    return c_version;
}

u32 CVideoEditorProject::GetCachedClipVersion() const
{
    u32 const c_version = m_montage ? m_montage->GetCachedClipVersion() : 0;
    return c_version;
}

u32 CVideoEditorProject::GetCachedReplayVersion() const
{
    u32 const c_version = m_montage ? m_montage->GetCachedReplayVersion() : 0;
    return c_version;
}

u32 CVideoEditorProject::GetCachedGameVersion() const
{
    u32 const c_version = m_montage ? m_montage->GetCachedGameVersion() : 0;
    return c_version;
}

u32 CVideoEditorProject::GetCachedTextVersion() const
{
    u32 const c_version = m_montage ? m_montage->GetCachedTextVersion() : 0;
    return c_version;
}

u32 CVideoEditorProject::GetCachedMusicVersion() const
{
    u32 const c_version = m_montage ? m_montage->GetCachedMusicVersion() : 0;
    return c_version;
}

u32 CVideoEditorProject::GetCachedMarkerVersion() const
{
    u32 const c_version = m_montage ? m_montage->GetCachedMarkerVersion() : 0;
    return c_version;
}

#endif // defined( GTA_REPLAY ) && GTA_REPLAY
