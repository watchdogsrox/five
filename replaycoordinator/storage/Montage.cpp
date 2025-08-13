//========================================================================================================================
// name:		Montage.cpp
// description:	Montage classes.
//========================================================================================================================
#include "Montage.h"

#if GTA_REPLAY

// rage
#include <stdint.h>
#include "stdio.h"
#include "string/stringhash.h"

// framework
#include "fwlocalisation/templateString.h"
#include "fwlocalisation/textUtil.h"

// game
#include "audio/frontendaudioentity.h"
#include "control/replay/Replay.h"
#include "control/replay/ReplaySettings.h"
#include "debug/Debug.h"
#include "FrontEnd/WarningScreen.h"
#include "frontend/VideoEditor/Core/VideoEditorUtil.h"
#include "replaycoordinator/ReplayAudioTrackProvider.h"
#include "replaycoordinator/replay_coordinator_channel.h"
#include "SaveLoad/savegame_messages.h"
#include "system/FileMgr.h"
#include "text/messages.h"
#include "text/text.h"
#include "video/mediaencoder_params.h"

REPLAY_COORDINATOR_OPTIMISATIONS();

// NOTE - Returns true if header data is considered corrupted
template< typename _bcHeaderDataType > static bool defaulHeaderLoadFunction( CMontage::MontageHeader_Current& out_header, FileHandle hFile,
																			u64& inout_resultFlags )
{
	u64 const c_errorMask = CReplayEditorParameters::PROJECT_LOAD_RESULT_UNSAFE_TO_CONTINUE;

	_bcHeaderDataType bcData;

	s32 const c_expectedSize = sizeof(_bcHeaderDataType);

	s32 const c_headerSizeRead = CFileMgr::Read( hFile, (char*)(&bcData), c_expectedSize );
	if( c_headerSizeRead >= c_expectedSize )
	{
		out_header = bcData;
	}
	else
	{
		inout_resultFlags |= CReplayEditorParameters::PROJECT_LOAD_RESULT_EARLY_EOF; 
	}

	return ( inout_resultFlags & c_errorMask ) == 0;
}

void CMontage::MontageHeaderVersionBlob::MakeCurrent()
{
    m_montageVersion    = MONTAGE_VERSION;
    m_clipVersion       = CClip::CLIP_VERSION;
    m_replayVersion     = ReplayHeader::GetVersionNumber();
    m_gameVersion       = ATSTRINGHASH( "GTAV", 0xEAD95154 );
    m_textVersion       = CMontageText::MONTAGE_TEXT_VERSION;
    m_musicVersion      = CMontageMusicClip::MONTAGE_MUSIC_CLIP_VERSION;
    m_markerVersion     = sReplayMarkerInfo::REPLAY_MARKER_VERSION;
}

void CMontage::MontageHeader_Current::CopyData( MontageHeader_V1 const& other )
{
	m_versionBlob.m_montageVersion = other.m_montageVersion;
	m_versionBlob.m_clipVersion	   = other.m_clipVersion;
	m_versionBlob.m_replayVersion  = other.m_replayVersion;
	m_versionBlob.m_gameVersion	   = other.m_gameVersion;
	m_versionBlob.m_textVersion	   = other.m_textVersion;
	m_versionBlob.m_musicVersion   = other.m_musicVersion;
	m_versionBlob.m_markerVersion  = other.m_markerVersion;

	m_cachedDurationMs		= other.m_cachedDurationMs;
	m_timeSpentEditingMs	= other.m_timeSpentEditingMs;
	m_numberOfClipObjects	= other.m_numberOfClipObjects;
	m_numberOfTextObjects	= other.m_numberOfTextObjects;
    m_numberOfMusicClips	= other.m_numberOfMusicClips;
    m_numberOfAmbientClips  = 0;

	m_montageObjectSize		= other.m_montageObjectSize;
	m_clipObjectSize		= other.m_clipObjectSize;
	m_textObjectSize		= other.m_textObjectSize;
	m_musicObjectSize		= other.m_musicObjectSize;
	m_markerObjectSize		= other.m_markerObjectSize;

	m_name.Set( other.m_name.getBuffer() );
}

void CMontage::MontageHeader_Current::CopyData( MontageHeader_V2 const& other )
{
    m_versionBlob           = other.m_versionBlob;
    m_cachedDurationMs		= other.m_cachedDurationMs;
    m_timeSpentEditingMs	= other.m_timeSpentEditingMs;
    m_numberOfClipObjects	= other.m_numberOfClipObjects;
    m_numberOfTextObjects	= other.m_numberOfTextObjects;
    m_numberOfMusicClips	= other.m_numberOfMusicClips;
    m_numberOfAmbientClips  = 0;

    m_montageObjectSize		= other.m_montageObjectSize;
    m_clipObjectSize		= other.m_clipObjectSize;
    m_textObjectSize		= other.m_textObjectSize;
    m_musicObjectSize		= other.m_musicObjectSize;
    m_markerObjectSize		= other.m_markerObjectSize;

    m_name.Set( other.m_name.getBuffer() );
}

void CMontage::MontageHeader_Current::CopyData( MontageHeader_V4 const& other )
{
    m_versionBlob           = other.m_versionBlob;
    m_cachedDurationMs		= other.m_cachedDurationMs;
    m_timeSpentEditingMs	= other.m_timeSpentEditingMs;
    m_numberOfClipObjects	= other.m_numberOfClipObjects;
    m_numberOfTextObjects	= other.m_numberOfTextObjects;
    m_numberOfMusicClips	= other.m_numberOfMusicClips;
    m_numberOfAmbientClips  = other.m_numberOfAmbientClips;

    m_montageObjectSize		= other.m_montageObjectSize;
    m_clipObjectSize		= other.m_clipObjectSize;
    m_textObjectSize		= other.m_textObjectSize;
    m_musicObjectSize		= other.m_musicObjectSize;
    m_markerObjectSize		= other.m_markerObjectSize;

    m_name.Set( other.m_name.getBuffer() );
}

CMontage::~CMontage()
{
	s32 iTextCount = m_aText.GetCount();
	for (s32 textInd = 0; textInd < iTextCount; textInd++)
	{
		delete m_aText[textInd];
	}
	m_aText.Reset();

	s32 iClipCount = m_aClips.GetCount();
	for( s32 clipInd = 0; clipInd < iClipCount; ++clipInd )
	{
		delete m_aClips[clipInd];
	}

	m_aClips.Reset();
}

CClip* CMontage::AddClip(const char* szFileName, u64 const ownerID, ReplayMarkerTransitionType eInType, ReplayMarkerTransitionType eOutType )
{
	return AddClip( szFileName, ownerID, m_aClips.size(), eInType, eOutType );
}

CClip* CMontage::AddClip(const char* szFileName, u64 const ownerID, s32 sIndex, ReplayMarkerTransitionType eInType, ReplayMarkerTransitionType eOutType)
{
	CClip* returnedClip = NULL;

	if( rcVerifyf( GetClipCount() < GetMaxClipCount(), "CMontage::AddClip - No more room for clips. Max limit %d", GetMaxClipCount() ) )
	{
		rcAssert(sIndex <= m_aClips.size());

		if (sIndex == m_aClips.size())
		{
			CClip*& rClip = m_aClips.Grow();
			rClip = rage_new CClip();
			returnedClip = rClip;
		}
		else
		{
			if ( m_aClips.GetCount() == m_aClips.GetCapacity() )
			{
				m_aClips.Grow();
				m_aClips.Pop();
			}

			CClip*& rClip = m_aClips.Insert(sIndex);
			rClip = rage_new CClip();
			returnedClip = rClip;
		}

		if( returnedClip )
		{
			returnedClip->Clear();
			
			if( returnedClip->Populate( szFileName, ownerID, eInType, eOutType ) )
			{
				MarkClipAsUsedInProject( *returnedClip );
				UpdateCachedDurationMs();
			}
			else
			{
				m_aClips.Delete(sIndex);
				delete returnedClip;
				returnedClip = NULL;
			}
		}
	}

	return returnedClip;
}

CClip* CMontage::AddClipCopy( s32 sIndexToCopyFrom, s32 sIndex )
{
	CClip* returnedClip = NULL;

	if( rcVerifyf( GetClipCount() < GetMaxClipCount(), "CMontage::AddClipCopy - No more room for clips. Max limit %d", GetMaxClipCount() ) )
	{
		rcAssert(sIndex <= m_aClips.size());

		if (m_aClips.GetCount() == m_aClips.GetCapacity())
		{
			m_aClips.Grow();
			m_aClips.Pop();
		}

		CClip*& newClip = m_aClips.Insert(sIndex);
		newClip = rage_new CClip();
		if( newClip )
		{
			returnedClip = newClip;

			newClip->Clear();
			*newClip = *m_aClips[sIndexToCopyFrom];
			MarkClipAsUsedInProject( *newClip );
			UpdateCachedDurationMs();
		}
	}

	return returnedClip;
}

CClip* CMontage::AddClipCopy( CClip const& clip, s32 sIndex )
{
	CClip* returnedClip = NULL;

	if( rcVerifyf( GetClipCount() < GetMaxClipCount(), "CMontage::AddClipCopy - No more room for clips. Max limit %d", GetMaxClipCount() ) )
	{
		rcAssert(sIndex <= m_aClips.size());

		if (m_aClips.GetCount() == m_aClips.GetCapacity())
		{
			m_aClips.Grow();
			m_aClips.Pop();
		}

		CClip*& newClip = m_aClips.Insert(sIndex);
		newClip = rage_new CClip();
		if( newClip )
		{
			newClip->Clear();
			*newClip = clip;
			MarkClipAsUsedInProject( *newClip );
			returnedClip = newClip;
			UpdateCachedDurationMs();
		}
	}

	return returnedClip;
}

CClip* CMontage::SetClipCopy( CClip const& clip, s32 sIndex )
{
	CClip* result = NULL;

	if( rcVerify( sIndex >= 0 && sIndex < m_aClips.size() ) )
	{
		CClip*& rClip = m_aClips[ sIndex ];

		MarkClipAsNotUsedInProject( *rClip );
		rClip->Clear();
		*rClip = clip;
		MarkClipAsUsedInProject( *rClip );

		result = rClip;
		UpdateCachedDurationMs();
	}

	return result;
}

s32 CMontage::GetClipCount() const
{
	return m_aClips.GetCount();
}

s32 CMontage::GetMaxClipCount() const
{
	return MAX_CLIP_OBJECTS;
}

CClip* CMontage::GetClip(s32 index)
{
	return index >= 0 && index < m_aClips.GetCount() ? m_aClips[index] : NULL;
}

CClip const* CMontage::GetClip(s32 index) const
{
	return index >= 0 &&index < m_aClips.GetCount() ? m_aClips[index] : NULL;
}

CClip* CMontage::GetClipByName(const char* szName)
{
	for (s32 clip = 0; clip < m_aClips.GetCount(); clip++)
	{
		CClip* rClip = m_aClips[clip];

		if ( rClip && strcmp( rClip->GetName(), szName) == 0 )
		{
			return rClip;
		}
	}

	return NULL;
}

CClip const* CMontage::GetClipByName( const char* szName ) const
{
	for (s32 clip = 0; clip < m_aClips.GetCount(); clip++)
	{
		CClip* rClip = m_aClips[clip];

		if ( rClip && strcmp( rClip->GetName(), szName) == 0 )
		{
			return rClip;
		}
	}

	return NULL;
}

float CMontage::GetTrimmedNonDilatedTimeToClipMs(s32 const index) const
{
	rcAssert(index < m_aClips.GetCount());

	float len = 0;
	for (s32 clip = 0; clip < index; clip++)
	{
		CClip* rClip = m_aClips[clip];
		len += rClip->GetTrimmedNonDilatedTimeMs();
	}

	return len;
}

float CMontage::GetTrimmedTimeToClipMs(s32 const index) const
{
	rcAssert(index < m_aClips.GetCount());

	float len = 0;
	for (s32 clip = 0; clip < index; clip++)
	{
		CClip* rClip = m_aClips[clip];
		len += rClip->GetTrimmedTimeMs();
	}

	return len;
}

u64 CMontage::GetTrimmedTimeToClipNs(s32 const index) const
{
	rcAssert(index < m_aClips.GetCount());

	u64 len = 0;
	for (s32 clip = 0; clip < index; clip++)
	{
		CClip* rClip = m_aClips[clip];
		len += FLOAT_MILLISECONDS_TO_NANOSECONDS(rClip->GetTrimmedTimeMs());
	}

	return len;
}

float CMontage::GetTotalClipLengthTimeMs() const
{
	float len = 0;
	for (s32 clip = 0; clip < m_aClips.GetCount(); clip++)
	{
		CClip* rClip = m_aClips[clip];
		len += rClip->GetTrimmedTimeMs();
	}

	return len;
}

float CMontage::ConvertClipNonDilatedTimeMsToClipTimeMs( s32 const index, float const clipNonDilatedTimeMs ) const
{
	CClip const* clip = GetClip( index );
	return clip ? clip->ConvertNonDilatedTimeMsToTimeMs( clipNonDilatedTimeMs ) : 0;
}

float CMontage::ConvertTimeMsToClipNonDilatedTimeMs( s32 const index, float const timeMs ) const
{
	CClip const* clip = GetClip( index );
	return clip ? clip->ConvertTimeMsToNonDilatedTimeMs( timeMs ) : 0;
}

float CMontage::ConvertPlaybackPercentageToClipNonDilatedTimeMs( u32 const clipIndex, float const playbackPercent ) const
{
	CClip const* clip = GetClip( clipIndex );
	return clip ? clip->ConvertPlaybackPercentageToNonDilatedTimeMs( playbackPercent ) : 0;
}

float CMontage::ConvertClipPercentageToClipNonDilatedTimeMs( u32 const clipIndex, float const clipPercent ) const
{
	CClip const* clip = GetClip( clipIndex );
	return clip ? clip->ConvertClipPercentageToNonDilatedTimeMs( clipPercent ) : 0;
}

float CMontage::ConvertClipNonDilatedTimeMsToClipPercentage( u32 const clipIndex, float const nonDilatedTimeMs ) const
{
	CClip const* clip = GetClip( clipIndex );
	return clip ? clip->ConvertClipNonDilatedTimeMsToClipPercentage( nonDilatedTimeMs ) : 0;
}

float CMontage::GetRawEndTimeAsTimeMs( s32 const index ) const
{
	CClip const* clip = GetClip( index );
	return clip ? clip->GetRawEndTimeAsTimeMs() : 0;
}

float CMontage::GetRawEndTimeMs( s32 const index ) const
{
	CClip const* clip = GetClip( index );
	return clip ? clip->GetRawDurationMs() : 0;
}

bool CMontage::IsValidClipReference( s32 const index ) const
{
	CClip const* clip = GetClip( index );
	bool const c_isValid = clip ? clip->HasValidFileReference() : false;
	return c_isValid;
}

bool CMontage::IsValidClipFile( u64 const ownerId, char const * const filename )
{
	bool const c_isValid = CReplayMgr::IsClipFileValid( ownerId, filename, false ) && CReplayMgr::IsClipFileValid( ownerId, filename, true );
	return c_isValid;
}

bool CMontage::DoesClipHaveModdedContent( s32 const index ) const
{
	CClip const* clip = GetClip( index );
	bool const c_hasMods = clip ? clip->HasModdedContent() : false;
	return c_hasMods;
}

bool CMontage::AreAllClipFilesValid()
{
	for (s32 clip = 0; clip < m_aClips.GetCount(); clip++)
	{
		if (!IsValidClipReference(clip))
			return false;
	}

	return true;
}

bool CMontage::AreAnyClipsOfModdedContent()
{
	for (s32 clip = 0; clip < m_aClips.GetCount(); clip++)
	{
		if (DoesClipHaveModdedContent(clip))
			return true;
	}

	return false;
}

void CMontage::DeleteClip(const char* szName)
{
	s32 const c_clipCount = m_aClips.GetCount();
	for (s32 clipIndex = 0; clipIndex < c_clipCount; ++clipIndex )
	{
		CClip* clip = m_aClips[clipIndex];
		if( clip && strcmp( clip->GetName(), szName ) == 0)
		{
			DeleteClip( clipIndex );
			break;
		}
	}
}

void CMontage::DeleteClip( s32 index )
{
	if ( index >= 0 && index < m_aClips.GetCount() )
	{
		CClip* clip = m_aClips[index];
		clip->Clear();
		m_aClips.Delete(index);

		MarkClipAsNotUsedInProject( *clip );

		delete clip;
		UpdateCachedDurationMs();
	}
}

void CMontage::SetName(const char* szName)
{
	if( rcVerifyf(szName && (szName[0] != '\0'), "CMontage::SetName: Invalid name" ) &&
		m_szName != szName )
	{
		m_szName.Set( szName );
	}
}

void CMontage::ValidateTransitionDuration( u32 const uIndex ) 
{
	CClip* clip = GetClip( uIndex );
	if( clip )
	{
		clip->ValidateTransitionTimes();
	}
}

ReplayMarkerTransitionType CMontage::GetStartTransition( u32 const uIndex ) const
{
	CClip const* clip = GetClip( uIndex );
	return clip ? clip->GetStartTransition() : MARKER_TRANSITIONTYPE_NONE;
}

void CMontage::SetStartTransition( u32 const uIndex,  ReplayMarkerTransitionType const eInType ) 
{
	CClip* clip = GetClip( uIndex );
	if( clip )
	{
		clip->SetStartTransition( eInType );
	}
}

float CMontage::GetStartTransitionDurationMs( u32 const uIndex ) const
{
	CClip const* clip = GetClip( uIndex );
	return clip ? clip->GetStartTransitionDurationMs() : 0;
}

void CMontage::SetStartTransitionDurationMs( u32 const uIndex, float const durationMs )
{
	CClip* clip = GetClip( uIndex );
	if( clip )
	{
		clip->SetStartTransitionDurationMs( durationMs );
	}
}

ReplayMarkerTransitionType CMontage::GetEndTransition( u32 const uIndex ) const
{
	CClip const* clip = GetClip( uIndex );
	return clip ? clip->GetEndTransition() : MARKER_TRANSITIONTYPE_NONE;
}

void CMontage::SetEndTransition( u32 const uIndex,  ReplayMarkerTransitionType const eOutType ) 
{
	CClip* clip = GetClip( uIndex );
	if( clip )
	{
		clip->SetEndTransition(eOutType);
	}
}

float CMontage::GetEndTransitionDurationMs( u32 const uIndex ) const
{
	CClip const* clip = GetClip( uIndex );
	return clip ? clip->GetEndTransitionDurationMs() : 0;
}

void CMontage::SetEndTransitionDurationMs( u32 const uIndex, float const durationMs )
{
	CClip* clip = GetClip( uIndex );
	if( clip )
	{
		clip->SetEndTransitionDurationMs( durationMs );
	}
}

void CMontage::GetActiveTransition( s32 const activeClipIndex, float const clipTime,
								   ReplayMarkerTransitionType& out_transition, float& out_fraction ) const
{
	out_transition = MARKER_TRANSITIONTYPE_MAX;
	out_fraction = 0.f;

	CClip const* clip = GetClip( activeClipIndex );
	if( clip )
	{
		s32 const c_startTransitionDurationMs = (s32)clip->GetStartTransitionDurationMs();
		s32 const c_endTransitionDurationMs = (s32)clip->GetEndTransitionDurationMs();

		s32 const c_startTimeMs = (s32)clip->GetStartTimeMs() + 40;
		s32 const c_endTimeMs = (s32)clip->GetEndTimeMs() - 40;
		s32 const c_clipRealTimeMs = (s32)clip->ConvertNonDilatedTimeMsToTimeMs( (float)clipTime ) + c_startTimeMs;

		s32 const c_startTransitionThresholdMs =  c_startTimeMs + c_startTransitionDurationMs;
		s32 const c_endTransitionThresholdMs =  c_endTimeMs - c_endTransitionDurationMs;

		ReplayMarkerTransitionType const c_startTransition = clip->GetStartTransition();
		ReplayMarkerTransitionType const c_endTransition = clip->GetEndTransition();

		// Figure out the transition type to apply
		out_transition = ( c_clipRealTimeMs < c_startTransitionThresholdMs ) ? c_startTransition :
			( c_clipRealTimeMs > c_endTransitionThresholdMs ) ? c_endTransition : MARKER_TRANSITIONTYPE_NONE;
		
		bool const c_hasStartTransition = out_transition == MARKER_TRANSITIONTYPE_FADEIN;
		bool const c_hasEndTransition = out_transition == MARKER_TRANSITIONTYPE_FADEOUT;

		s32 const c_minThreshold = c_hasStartTransition ? c_startTimeMs : c_hasEndTransition ? c_endTransitionThresholdMs : 0;

		s32 const c_offsetTimeMs = c_clipRealTimeMs - c_minThreshold;
		s32 const c_transitionDurationMs = c_hasStartTransition ? c_startTransitionDurationMs : c_hasEndTransition ? c_endTransitionDurationMs : 0;

		out_fraction = ( 1.f / ( c_transitionDurationMs / 1000.f ) ) * ( c_offsetTimeMs / 1000.f);
		out_fraction = Clamp(out_fraction, 0.0f, 1.0f);
		out_fraction = c_hasStartTransition ? 1.f - out_fraction : out_fraction;
	}
}

s32 CMontage::AddMusicClip( u32 const soundHash, u32 const uTime ) 
{
	s32 resultIndex = -1;

	if( rcVerifyf( GetMusicClipCount() < GetMaxMusicClipCount(), 
		"CMontage::AddMusicClip - No more room for music tracks. Max limit %d", GetMaxMusicClipCount() ) )
	{
		m_musicClips.PushAndGrow( CMontageMusicClip( soundHash, uTime ) );
		resultIndex = m_musicClips.size() > 0 ? m_musicClips.size() - 1 : 0;
	}	

	return resultIndex;
}

CMontageMusicClip* CMontage::GetMusicClip( u32 const uIndex )
{
	CMontageMusicClip* musicClip = uIndex < m_musicClips.size() ? &m_musicClips[ uIndex ] : NULL;
	return musicClip;
}

CMontageMusicClip const* CMontage::GetMusicClip( u32 const uIndex ) const
{
	CMontageMusicClip const* musicClip = uIndex < m_musicClips.size() ? &m_musicClips[ uIndex ] : NULL;
	return musicClip;
}

u32 CMontage::GetMusicClipSoundHash( u32 const uIndex ) const
{
	// 09/19/2008 [GH] - Commented out assert.  Handled by returning NULL string since audio
	// will check regardless if music clip is added.
	//	replayAssert((int)uIndex < m_aoMusicClipNames.GetCount());

	CMontageMusicClip const* musicClip = GetMusicClip( uIndex );
	u32 soundHash = musicClip == NULL ? 
		0 : musicClip->getSoundHash();

	return soundHash;
}

u32 CMontage::GetMusicClipMusicOnlyCount() const
{
	u32 count = 0;

	size_t const c_musicCount = m_musicClips.size();
	for( s32 musicInd = 0; musicInd < c_musicCount; ++musicInd )
	{
		CMontageMusicClip const* musicClip = GetMusicClip( musicInd );
		if( musicClip->IsRadioTrack() )
			++count;
	}

	return count;
}

u32 CMontage::GetMusicClipCommercialsOnlyCount() const
{
	u32 count = 0;

	size_t const c_musicCount = m_musicClips.size();
	for( s32 musicInd = 0; musicInd < c_musicCount; ++musicInd )
	{
		CMontageMusicClip const* musicClip = GetMusicClip( musicInd );
		if( musicClip->IsCommercialTrack() )
			++count;
	}

	return count;
}

u32 CMontage::GetMusicClipScoreOnlyCount() const
{
	u32 count = 0;

	size_t const c_musicCount = m_musicClips.size();
	for( s32 musicInd = 0; musicInd < c_musicCount; ++musicInd )
	{
		CMontageMusicClip const* musicClip = GetMusicClip( musicInd );
		if( musicClip->IsScoreTrack() )
			++count;
	}

	return count;
}

s32 CMontage::GetMusicStartTimeMs( u32 const uIndex ) const
{
	// 09/19/2008 [GH] - Commented out assert.  Handled by returning NULL string since audio
	// will check regardless if music clip is added.
	//replayAssert((int)uIndex < m_aoMusicStartTimes.GetCount());

	CMontageMusicClip const* musicClip = GetMusicClip( uIndex );
	s32 const c_startTimeMs =  musicClip == NULL ? 
		0 : musicClip->getStartTimeMs();

	return c_startTimeMs;
}

void CMontage::SetMusicStartTimeMs( u32 const uIndex, u32 const uTimeMs )
{
	CMontageMusicClip* musicClip = GetMusicClip( uIndex );
	if( musicClip )
	{
		musicClip->setStartTimeMs( uTimeMs );
	}
}

u32 CMontage::GetMusicDurationMs( u32 const uIndex ) const
{
	CMontageMusicClip const* musicClip = GetMusicClip( uIndex );
	u32 const c_durationMs =  musicClip == NULL ? 
		0 : musicClip->getTrimmedDurationMs();

	return c_durationMs;
}

u32	CMontage::GetMusicStartOffsetMs( u32 const uIndex) const
{
	CMontageMusicClip const* musicClip = GetMusicClip( uIndex );
	u32 const c_offsetMs =  musicClip == NULL ? 
		0 : musicClip->getStartOffsetMs();

	return c_offsetMs;
}

bool CMontage::DoesClipContainMusicClipWithBeatMarkers( u32 const clipIndex ) const
{
	CMontageMusicClip const* firstMusicClipFound = NULL;
	CClip const * const c_clip = GetClip( clipIndex );

	float const c_clipStartTimeMs = GetTrimmedTimeToClipMs( clipIndex );
	float const c_clipEndTimeMs = c_clipStartTimeMs + ( c_clip ? c_clip->GetEndTimeMs() : 0 );

	size_t const c_musicCount = m_musicClips.size();
	for( s32 musicInd = 0; firstMusicClipFound == NULL && musicInd < c_musicCount; ++musicInd )
	{
		CMontageMusicClip const* musicClip = GetMusicClip( musicInd );
		if( musicClip && musicClip->ContainsBeatMarkers() )
		{
			float const c_musicClipStartTimeMs = musicClip ? (float)musicClip->getStartTimeMs() + (float)musicClip->getStartOffsetMs() : 0.f;
			float const c_musicClipEndTimeMs = c_musicClipStartTimeMs + ( musicClip ? (float)musicClip->getTrimmedDurationMs() : 0.f );

			bool const c_overlapsWithClip = ( c_musicClipStartTimeMs >= c_clipStartTimeMs && c_musicClipStartTimeMs < c_clipEndTimeMs ) ||
				( c_musicClipEndTimeMs >= c_clipStartTimeMs && c_musicClipEndTimeMs <= c_clipEndTimeMs ) || 
				( c_musicClipStartTimeMs < c_clipStartTimeMs && c_musicClipEndTimeMs > c_clipEndTimeMs );

			firstMusicClipFound = c_overlapsWithClip ? musicClip : NULL;
		}
	}
	
	return firstMusicClipFound != NULL;
}

u32 CMontage::GetHashOfFirstInvalidMusicTrack() const
{
	u32 firstInvalidTrackHash = 0;

	float const c_montageDurationMs = GetTotalClipLengthTimeMs();
    float trackStartTimeMs = 0.f;
	s32 const c_musicCount = m_musicClips.GetCount();
	for( s32 musicInd = c_musicCount - 1; trackStartTimeMs < c_montageDurationMs && firstInvalidTrackHash == 0 && musicInd >= 0; --musicInd )
	{
		CMontageMusicClip const * musicClip = GetMusicClip( musicInd );
		
		bool const c_isRadioTrack = musicClip ? musicClip->IsRadioTrack() : false;
		trackStartTimeMs = musicClip ? (float)musicClip->getStartTimeMs() : 0.f;
		float const c_trackDurationMs = musicClip ? (float)musicClip->getTrimmedDurationMs() : 0.f;

		bool const c_willTrackBeTruncated = c_isRadioTrack ? (( c_montageDurationMs - trackStartTimeMs ) < MIN_MUSIC_DURATION_MS && c_musicCount > 1) ||
			( c_trackDurationMs < MIN_MUSIC_DURATION_MS ) : false;

		firstInvalidTrackHash = musicClip && c_willTrackBeTruncated ? musicClip->getSoundHash() : 0;
	}

	return firstInvalidTrackHash;
}

void CMontage::DeleteMusicClip( u32 const uIndex )
{
	if( IsValidMusicClipIndex( uIndex ) )
	{
		m_musicClips.Delete( uIndex );
	}
}

// Ambient track
s32 CMontage::AddAmbientClip( u32 const soundHash, u32 const uTime ) 
{
	s32 resultIndex = -1;

	if( rcVerifyf( GetAmbientClipCount() < GetMaxAmbientClipCount(), 
		"CMontage::AddAmbientClip - No more room for ambient tracks. Max limit %d", GetMaxAmbientClipCount() ) )
	{
		m_ambientClips.PushAndGrow( CMontageMusicClip( soundHash, uTime ) );
		resultIndex = m_ambientClips.size() > 0 ? m_ambientClips.size() - 1 : 0;
	}	

	return resultIndex;
}

CMontageMusicClip* CMontage::GetAmbientClip( u32 const uIndex )
{
	CMontageMusicClip* ambientClip = uIndex < m_ambientClips.size() ? &m_ambientClips[ uIndex ] : NULL;
	return ambientClip;
}

CMontageMusicClip const* CMontage::GetAmbientClip( u32 const uIndex ) const
{
	CMontageMusicClip const* ambientClip = uIndex < m_ambientClips.size() ? &m_ambientClips[ uIndex ] : NULL;
	return ambientClip;
}

u32 CMontage::GetAmbientClipSoundHash( u32 const uIndex ) const
{
	CMontageMusicClip const* ambientClip = GetAmbientClip( uIndex );
	u32 soundHash = ambientClip == NULL ? 
		g_NullSoundHash : ambientClip->getSoundHash();

	return soundHash;
}

s32 CMontage::GetAmbientStartTimeMs( u32 const uIndex ) const
{
	CMontageMusicClip const* ambientClip = GetAmbientClip( uIndex );
	s32 const c_startTimeMs =  ambientClip == NULL ? 
		0 : ambientClip->getStartTimeMs();

	return c_startTimeMs;
}

void CMontage::SetAmbientStartTimeMs( u32 const uIndex, u32 const uTimeMs )
{
	CMontageMusicClip* ambientClip = GetAmbientClip( uIndex );
	if( ambientClip )
	{
		ambientClip->setStartTimeMs( uTimeMs );
	}
}

u32 CMontage::GetAmbientDurationMs( u32 const uIndex ) const
{
	CMontageMusicClip const* ambientClip = GetAmbientClip( uIndex );
	u32 const c_durationMs =  ambientClip == NULL ? 
		0 : ambientClip->getTrimmedDurationMs();

	return c_durationMs;
}

u32	CMontage::GetAmbientStartOffsetMs( u32 const uIndex) const
{
	CMontageMusicClip const* ambientClip = GetAmbientClip( uIndex );
	u32 const c_offsetMs =  ambientClip == NULL ? 
		0 : ambientClip->getStartOffsetMs();

	return c_offsetMs;
}

void CMontage::DeleteAmbientClip( u32 const uIndex )
{
	if( IsValidAmbientClipIndex( uIndex ) )
	{
		m_ambientClips.Delete( uIndex );
	}
}

// Text

CMontageText* CMontage::AddText( u32 const startTimeMs, u32 const durationMs )
{
	CMontageText* newText = NULL;

	if( rcVerifyf( GetTextCount() < GetMaxTextCount(), 
		"CMontage::AddText - No more room for text. Max limit %d", GetMaxTextCount() ) )
	{
		newText = rage_new CMontageText( startTimeMs, durationMs );
		if( rcVerifyf( newText, "CMontage::AddText Unable to allocate CMontageText" ) )
		{
			m_aText.Grow();
			m_aText[m_aText.GetCount() - 1] = newText;
		}
	}

	return newText;
}

CMontageText* CMontage::GetText( s32 const index)
{
	CMontageText* foundText = index >= 0 && index < m_aText.GetCount() ? 
		m_aText[index] : NULL;

	return foundText;
}

CMontageText const* CMontage::GetText( s32 const index ) const
{
	CMontageText const* foundText = index >= 0 && index < m_aText.GetCount() ? 
		m_aText[index] : NULL;

	return foundText;
}

s32 CMontage::GetTextIndex( CMontageText const* target ) const
{
	s32 textIndex = -1;

	s32 textCount = m_aText.GetCount();
	for( s32 index = 0; index < textCount; ++index )
	{
		if ( m_aText[index] == target )
		{
			textIndex = index;
			break;
		}
	}

	return textIndex;
}

void CMontage::DeleteText( CMontageText*& item )
{
	s32 iTextCount = m_aText.GetCount();
	for (s32 textInd = 0; textInd < iTextCount; textInd++)
	{
		if (m_aText[textInd] == item)
		{
			delete m_aText[textInd];
			m_aText.Delete(textInd);
			item = NULL;
			break;
		}
	}
}

void CMontage::DeleteText( s32 const index )
{
	bool validIndex = rcVerifyf( IsValidTextIndex( index ), "CMontage::DeleteText - Invalid index %d!", index );
	if( validIndex )
	{
		delete m_aText[index];
		m_aText.Delete(index);
	}
}

s32 CMontage::GetActiveTextIndex( s32 const activeClipIndex, float const clipTime ) const
{
	s32 resultingIndex = INDEX_NONE;

	float const c_timeToClipMs = GetTrimmedTimeToClipMs( activeClipIndex );
	float const c_realTimeMs = ConvertClipNonDilatedTimeMsToClipTimeMs( activeClipIndex, (float)clipTime );
	float const c_projectRealTimeMs = c_timeToClipMs + c_realTimeMs;

	size_t const c_textCount = m_aText.size();
	for( s32 textInd = 0; textInd < c_textCount; ++textInd )
	{
		if( m_aText[textInd]->ShouldRender( (u32)c_projectRealTimeMs ) )
		{
			resultingIndex = textInd;
			break;
		}
	}

	return resultingIndex;
}

bool CMontage::GetActiveEffectParameters( s32 const activeClipIndex, float const clipTimeMs, s32& out_effectIndex, 
										 float& out_intensity, float& out_saturation, float& out_contrast, float& out_brightness, float& out_vignette ) const
{
	out_effectIndex = INDEX_NONE;

	CClip const * clip = GetClip( activeClipIndex );
	if( clip )
	{
		float timeDiff = 0;
		s32 markerIndex = clipTimeMs == clip->GetStartNonDilatedTimeMs() ? 
			0 : clip->GetClosestMarkerIndex( clipTimeMs, CLOSEST_MARKER_LESS_OR_EQUAL, &timeDiff, clip->GetLastMarker(), true );

		if( markerIndex >= 0 && timeDiff <= 0 )
		{
			sReplayMarkerInfo const* marker = clip->GetMarker( markerIndex );
			out_effectIndex = marker && !marker->IsAnchor() ? marker->GetActivePostFx() : INDEX_NONE;
			out_intensity = marker && !marker->IsAnchor() ? marker->GetPostFxIntensity() : 0.f;
			out_saturation = marker && !marker->IsAnchor() ? marker->GetSaturationIntensity() : 0.f;
			out_contrast = marker && !marker->IsAnchor() ? marker->GetContrastIntensity() : 0.f;
			out_brightness = marker && !marker->IsAnchor() ? marker->GetBrightness() : 0.f;
			out_vignette = marker && !marker->IsAnchor() ? marker->GetVignetteIntensity() : 0.f;
		}
	}

	return out_effectIndex != INDEX_NONE;
}

bool CMontage::GetActiveRenderIntensities( s32 const activeClipIndex, float const clipTimeMs, 
										  float& out_saturation, float& out_contrast, float& out_vignette ) const
{
	bool success = false;

	CClip const * clip = GetClip( activeClipIndex );
	if( clip )
	{
		float timeDiff = 0;
		s32 markerIndex = clipTimeMs == clip->GetStartNonDilatedTimeMs() ? 
			0 : clip->GetClosestMarkerIndex( clipTimeMs, CLOSEST_MARKER_LESS_OR_EQUAL, &timeDiff, clip->GetLastMarker(), true );

		if( markerIndex >= 0 && timeDiff <= 0 )
		{
			sReplayMarkerInfo const* marker = clip->GetMarker( markerIndex );
			out_saturation = marker && !marker->IsAnchor() ? marker->GetSaturationIntensity() : 0.f;
			out_contrast = marker && !marker->IsAnchor() ? marker->GetContrastIntensity() : 0.f;
			out_vignette = marker && !marker->IsAnchor() ? marker->GetVignetteIntensity() : 0.f;
		}
	}

	return success;
}

s32 CMontage::GetActiveMusicIndex( s32 const activeClipIndex, float const clipTimeMs ) const
{
	s32 resultingIndex = INDEX_NONE;
	float latestStartTime = 0;
	const CMontageMusicClip* selectedMusicClip = NULL;

	float const c_realTimeMs = ConvertClipNonDilatedTimeMsToClipTimeMs( activeClipIndex, clipTimeMs );	
	float offsetTime = c_realTimeMs;

	for( s32 clip = 0; clip < activeClipIndex; ++clip )
	{
		offsetTime += m_aClips[clip]->GetTrimmedTimeMs();
	}

	size_t const c_musicCount = m_musicClips.size();
	for( s32 musicInd = 0; musicInd < c_musicCount; ++musicInd )
	{
		CMontageMusicClip const* musicClip = GetMusicClip( musicInd );

		if( Verifyf( musicClip, "Failed to find music clip for track %d", musicInd ) )
		{			
			// Don't allow music to actually start playing until we're past the start offset
			float const c_startTime = float(musicClip->getStartTimeMs() + musicClip->getStartOffsetMs());			

			if( offsetTime >= c_startTime && offsetTime - c_startTime <= float(musicClip->getTrimmedDurationMs()) )
			{
				if( c_startTime > latestStartTime || resultingIndex == INDEX_NONE )
				{
					// Later tracks override earlier tracks, so we need to find the valid track with the latest
					// start time
					selectedMusicClip = musicClip;
					latestStartTime = c_startTime;
					resultingIndex = (s32)musicInd;
				}		
			}
		}		
	}

	// Even if we've found a valid clip, we might be past the end of the playback
	if(selectedMusicClip)
	{
		// Tracks can have a negative start time (which just represents a start offset of x ms) so ignore this 
		// when working out the actual audible duration of the track
		if( offsetTime > Max(0, selectedMusicClip->getStartTimeMs()) + selectedMusicClip->getTrimmedDurationMs() + selectedMusicClip->getStartOffsetMs() )
		{
			resultingIndex = INDEX_NONE;
		}
	}

	return resultingIndex;
}

s32 CMontage::GetActiveAmbientTrackIndex( s32 const activeClipIndex, float const clipTimeMs ) const
{
	s32 resultingIndex = INDEX_NONE;
	float latestStartTime = 0;
	const CMontageMusicClip* selectedMusicClip = NULL;

	float const c_realTimeMs = ConvertClipNonDilatedTimeMsToClipTimeMs( activeClipIndex, clipTimeMs );	
	float offsetTime = c_realTimeMs;

	for( s32 clip = 0; clip < activeClipIndex; ++clip )
	{
		offsetTime += m_aClips[clip]->GetTrimmedTimeMs();
	}

	size_t const c_musicCount = m_ambientClips.size();
	for( s32 musicInd = 0; musicInd < c_musicCount; ++musicInd )
	{
		CMontageMusicClip const* musicClip = GetAmbientClip( musicInd );

		if( Verifyf( musicClip, "Failed to find music clip for track %d", musicInd ) )
		{			
			// Don't allow music to actually start playing until we're past the start offset
			float const c_startTime = float(musicClip->getStartTimeMs() + musicClip->getStartOffsetMs());			

			if( offsetTime >= c_startTime && offsetTime - c_startTime < float(musicClip->getTrimmedDurationMs()) )
			{
				if( c_startTime > latestStartTime || resultingIndex == INDEX_NONE )
				{
					// Later tracks override earlier tracks, so we need to find the valid track with the latest
					// start time
					selectedMusicClip = musicClip;
					latestStartTime = c_startTime;
					resultingIndex = (s32)musicInd;
				}		
			}
		}		
	}

	// Even if we've found a valid clip, we might be past the end of the playback
	if(selectedMusicClip)
	{
		// Tracks can have a negative start time (which just represents a start offset of x ms) so ignore this 
		// when working out the actual audible duration of the track
		if( offsetTime >= Max(0, selectedMusicClip->getStartTimeMs()) + selectedMusicClip->getTrimmedDurationMs() + selectedMusicClip->getStartOffsetMs() )
		{
			resultingIndex = INDEX_NONE;
		}
	}

	return resultingIndex;
}

s32 CMontage::GetNextActiveMusicIndex( s32 const activeClipIndex, float const clipTimeMs ) const
{
	s32 musicClipIndex = -1;

	CClip const * const c_clip = GetClip( activeClipIndex );

	float const c_realTimeMs = ConvertClipNonDilatedTimeMsToClipTimeMs( activeClipIndex, clipTimeMs );

	float const c_clipStartTimeMs = GetTrimmedTimeToClipMs( activeClipIndex ); 
	float const c_clipStartTimeOffsetMs = c_clipStartTimeMs + c_realTimeMs;

	float const c_clipEndTimeMs = c_clipStartTimeMs + ( c_clip ? c_clip->GetEndTimeMs() : 0 );
	size_t const c_musicCount = m_musicClips.size();

	s32 earliestStartTime = -1;

	for( s32 musicInd = 0; musicInd < c_musicCount; ++musicInd )
	{
		CMontageMusicClip const* musicClip = GetMusicClip( musicInd );
		s32 const c_musicClipStartTimeMs = musicClip ? musicClip->getStartTimeMs() + musicClip->getStartOffsetMs() : 0;
		s32 const c_musicClipEndTimeMs = c_musicClipStartTimeMs + ( musicClip ? musicClip->getTrimmedDurationMs() : 0 );

		bool const c_overlapsWithClip = ( c_musicClipStartTimeMs >= c_clipStartTimeOffsetMs && c_musicClipStartTimeMs < c_clipEndTimeMs ) ||
			( c_musicClipEndTimeMs >= c_clipStartTimeOffsetMs && c_musicClipEndTimeMs <= c_clipEndTimeMs ) || 
			( c_musicClipStartTimeMs < c_clipStartTimeOffsetMs && c_musicClipEndTimeMs > c_clipEndTimeMs );

		if(c_overlapsWithClip)
		{
			// We could have multiple music clips that satisfy the overlap condition within the current clip, so we need to find 
			// the closest one to the current playback point
			if(earliestStartTime == -1 || c_musicClipStartTimeMs < earliestStartTime)
			{
				earliestStartTime = c_musicClipStartTimeMs;
				musicClipIndex = musicInd;
			}
		}		
	}

	return musicClipIndex;
}

s32 CMontage::GetPreviousActiveMusicIndex( s32 const activeClipIndex, float const clipTimeMs ) const
{
	s32 musicClipIndex = -1;

	float const c_realTimeMs = ConvertClipNonDilatedTimeMsToClipTimeMs( activeClipIndex, (float)clipTimeMs );
	float const c_clipStartTimeMs = GetTrimmedTimeToClipMs( activeClipIndex );
	float const c_clipEndTimeMs = c_clipStartTimeMs + c_realTimeMs;

	s32 const c_musicCount = m_musicClips.GetCount();
	s32 latestEndTime = -1;

	for( s32 musicInd = c_musicCount - 1; musicInd >= 0; --musicInd )
	{
		CMontageMusicClip const* musicClip = GetMusicClip( musicInd );
		s32 const c_musicClipStartTimeMs = musicClip ? musicClip->getStartTimeMs() + musicClip->getStartOffsetMs() : 0;
		s32 const c_musicClipEndTimeMs = c_musicClipStartTimeMs + ( musicClip ? musicClip->getTrimmedDurationMs() : 0 );

		bool const c_overlapsWithClip = ( c_musicClipStartTimeMs >= c_clipStartTimeMs && c_musicClipStartTimeMs < c_clipEndTimeMs ) ||
			( c_musicClipEndTimeMs >= c_clipStartTimeMs && c_musicClipEndTimeMs <= c_clipEndTimeMs ) || 
			( c_musicClipStartTimeMs < c_clipStartTimeMs && c_musicClipEndTimeMs > c_clipEndTimeMs );

		if(c_overlapsWithClip)
		{
			// We could have multiple music clips that satisfy the overlap condition within the current clip, so we need to find 
			// the closest one to the current playback point
			if(latestEndTime == -1 || c_musicClipEndTimeMs > latestEndTime)
			{
				latestEndTime = c_musicClipEndTimeMs;
				musicClipIndex = musicInd;
			}
		}
	}

	return musicClipIndex;
}

void CMontage::ConvertMusicBeatTimeToNonDilatedTimeMs( float const beatTimeMs, s32& clipIndex, float& clipTimeMs ) const
{
	float clipStartTime = 0;
	float clipEndTime = 0;

	// For each clip in the montage...
	for( s32 clip = 0; clip < GetClipCount(); ++clip )
	{
		clipStartTime = clipEndTime;
		clipEndTime += (float)m_aClips[clip]->GetTrimmedTimeMs();

		// ...if the beat lies within this particular clip...
		if(beatTimeMs >= clipStartTime && beatTimeMs < clipEndTime)
		{
			// ...calculate the time relative to the clip start time
			float const c_clipLocalTimeMs = beatTimeMs - clipStartTime;
			clipTimeMs = ConvertTimeMsToClipNonDilatedTimeMs( clip, c_clipLocalTimeMs );
			clipIndex = clip;
			return;
		}
	}

	clipIndex = -1;
	clipTimeMs = 0;
}

size_t CMontage::ApproximateMinimumSizeForSaving()
{
	size_t sizeNeeded = 0;

	// following section must match save writing
	sizeNeeded = sizeof(MontageHeader_Current);
	sizeNeeded += CClip::ApproximateMinimumSizeForSaving();

	return sizeNeeded;
}

size_t CMontage::ApproximateMaximumSizeForSaving()
{
	size_t sizeNeeded = 0;

	// following section must match save writing
	sizeNeeded = sizeof(MontageHeader_Current);
	sizeNeeded += CClip::ApproximateMaximumSizeForSaving() * MAX_CLIP_OBJECTS;
	sizeNeeded += CMontageText::ApproximateMaximumSizeForSaving() * MAX_TEXT_OBJECTS;
    sizeNeeded += CMontageMusicClip::ApproximateMaximumSizeForSaving() * MAX_MUSIC_OBJECTS; 
    sizeNeeded += CMontageMusicClip::ApproximateMaximumSizeForSaving() * MAX_AMBIENT_OBJECTS;

	return sizeNeeded;
}

size_t CMontage::ApproximateSizeForSaving() const
{
	MontageHeader_Current header;

	header.m_numberOfClipObjects = (u32)m_aClips.size();
	header.m_numberOfTextObjects = (u32)m_aText.size();
	header.m_numberOfMusicClips = (u32)m_musicClips.size();

	size_t const c_sizeNeeded = ApproximateSizeForSavingInternal( header );
	return c_sizeNeeded;
}

bool CMontage::IsSpaceAvailableForSaving( char const * const szFilename, size_t const expectedSize )
{
#if RSG_ORBIS
	ReplayFileManager::RefreshNonClipDirectorySizes();
#endif

	bool const c_isSpace = ReplayFileManager::CheckAvailableDiskSpaceForMontage( (u64)expectedSize, szFilename );
	return c_isSpace;
}

bool CMontage::IsSpaceAvailableForSaving( char const * const szFilename )
{
	// estimate files size to see if we have enough space before writing
	size_t const fileSizeEstimate = ApproximateSizeForSaving();

	bool const c_isSpace = IsSpaceAvailableForSaving( szFilename, fileSizeEstimate );
	return c_isSpace;
}

bool CMontage::IsSpaceAvailableForSaving()
{
	PathBuffer directory;
	CVideoEditorUtil::getVideoProjectDirectoryForCurrentUser( directory.getBufferRef() );

	PathBuffer path( directory.getBuffer() );
    path.append( GetName() );
    path.append( STR_MONTAGE_FILE_EXT );
	return IsSpaceAvailableForSaving( path );
}

bool CMontage::IsSpaceToAddClip()
{
	// estimate files size to see if we have enough space before writing
	size_t const fileSizeEstimate = ApproximateSizeForSaving() + CClip::ApproximateMinimumSizeForSaving();

	PathBuffer directory;
	CVideoEditorUtil::getVideoProjectDirectoryForCurrentUser( directory.getBufferRef() );

	PathBuffer path( directory.getBuffer() );
    path.append( GetName() );
    path.append( STR_MONTAGE_FILE_EXT );
	return IsSpaceAvailableForSaving( path, fileSizeEstimate );
}

bool CMontage::IsSpaceToAddMarker()
{
	// estimate files size to see if we have enough space before writing
	size_t const fileSizeEstimate = ApproximateSizeForSaving() + sizeof(sReplayMarkerInfo);

	PathBuffer directory;
	CVideoEditorUtil::getVideoProjectDirectoryForCurrentUser( directory.getBufferRef() );

    PathBuffer path( directory.getBuffer() );
    path.append( GetName() );
    path.append( STR_MONTAGE_FILE_EXT );

	return IsSpaceAvailableForSaving( path, fileSizeEstimate );
}

bool CMontage::IsSpaceToAddText()
{
	// estimate files size to see if we have enough space before writing
	size_t const fileSizeEstimate = ApproximateSizeForSaving() + CMontageText::ApproximateMinimumSizeForSaving();

	PathBuffer directory;
	CVideoEditorUtil::getVideoProjectDirectoryForCurrentUser( directory.getBufferRef() );

    PathBuffer path( directory.getBuffer() );
    path.append( GetName() );
    path.append( STR_MONTAGE_FILE_EXT );

	return IsSpaceAvailableForSaving( path, fileSizeEstimate );
}

bool CMontage::IsSpaceToAddMusic()
{
	// estimate files size to see if we have enough space before writing
	size_t const fileSizeEstimate = ApproximateSizeForSaving() + CMontageMusicClip::ApproximateMinimumSizeForSaving();

	PathBuffer directory;
	CVideoEditorUtil::getVideoProjectDirectoryForCurrentUser( directory.getBufferRef() );

    PathBuffer path( directory.getBuffer() );
    path.append( GetName() );
    path.append( STR_MONTAGE_FILE_EXT );

	return IsSpaceAvailableForSaving( path, fileSizeEstimate );
}

bool CMontage::IsSpaceToAddAmbientTrack()
{
    // estimate files size to see if we have enough space before writing
    size_t const fileSizeEstimate = ApproximateSizeForSaving() + CMontageMusicClip::ApproximateMinimumSizeForSaving();

    PathBuffer directory;
    CVideoEditorUtil::getVideoProjectDirectoryForCurrentUser( directory.getBufferRef() );

    PathBuffer path( directory.getBuffer() );
    path.append( GetName() );
    path.append( STR_MONTAGE_FILE_EXT );
    return IsSpaceAvailableForSaving( path, fileSizeEstimate );
}

u64 CMontage::Save( const char* szFilename, bool const showSpinnerWithText )
{
	u64 resultFlags = CReplayEditorParameters::PROJECT_SAVE_RESULT_NO_CLIPS;

	if( rcVerifyf( m_aClips.GetCount() > 0, "CMontage::Save - Attempting to save a project with zero clips?" ) )
	{
		resultFlags = CReplayEditorParameters::PROJECT_SAVE_RESULT_BAD_NAME;

		szFilename = (szFilename == NULL) || (szFilename[0] == '\0') ? GetName() : szFilename;
		bool const c_savingUnderNewName = strcmpi( GetName(), szFilename ) != 0;

		u32 const c_filenameByteCount = fwTextUtil::GetByteCount( szFilename );
		if( rcVerifyf( c_filenameByteCount > 0 && c_filenameByteCount <= MAX_NAME_SIZE_BYTES, 
			"CMontage::Save - Name %s of %u bytes is unspported. Max size %u bytes", szFilename, c_filenameByteCount, MAX_NAME_SIZE_BYTES ) )
		{
			resultFlags = CReplayEditorParameters::PROJECT_SAVE_RESULT_BAD_PATH;

			PathBuffer directory;
			CVideoEditorUtil::getVideoProjectDirectoryForCurrentUser( directory.getBufferRef() );

			// create projects directory if it doesn't already exist yet
			if( ASSET.CreateLeadingPath( directory.getBuffer() ) )
			{
				resultFlags = CReplayEditorParameters::PROJECT_SAVE_RESULT_NO_SPACE;

				PathBuffer fileNameWithExt( szFilename );
                fileNameWithExt += STR_MONTAGE_FILE_EXT;

				rcDebugf1("Saving Montage : %s", fileNameWithExt.getBuffer() );

				PathBuffer path( directory.getBuffer() );
                path += fileNameWithExt;

				if( IsSpaceAvailableForSaving( path ) )
				{
					resultFlags = CReplayEditorParameters::PROJECT_SAVE_RESULT_FILE_ACCESS_ERROR;

					FileHandle hFile = CFileMgr::OpenFileForWriting( path.getBuffer() );
					if( hFile )
					{
						resultFlags = CReplayEditorParameters::PROJECT_SAVE_RESULT_SUCCESS;

						UpdateCachedDurationMs();

						CSavingMessage::eTypeOfMessage const c_messageType = showSpinnerWithText ? 
							CSavingMessage::STORAGE_DEVICE_MESSAGE_SAVING_REPLAY_PROJECT : CSavingMessage::STORAGE_DEVICE_MESSAGE_SAVING_REPLAY_PROJECT_NO_TEXT;

						CSavingMessage::BeginDisplaying( c_messageType );

                        m_cachedVersionBlob.MakeCurrent();
						MontageHeader_Current header;
						header.m_versionBlob = m_cachedVersionBlob;

						header.m_cachedDurationMs = (u32)GetCachedDurationMs();
						header.m_timeSpentEditingMs = UpdateTimeSpentEditingMs();

						header.m_numberOfClipObjects = (u32)m_aClips.size();
						header.m_numberOfTextObjects = (u32)m_aText.size();
                        header.m_numberOfMusicClips = (u32)m_musicClips.size();
                        header.m_numberOfAmbientClips = (u32)m_ambientClips.size();

						header.m_montageObjectSize = sizeof(CMontage);
						header.m_clipObjectSize = sizeof(CClip);
						header.m_textObjectSize = sizeof(CMontageText);
						header.m_musicObjectSize = sizeof(CMontageMusicClip);
						header.m_markerObjectSize = sizeof(sReplayMarkerInfo);

						header.m_name = m_szName;

						s32 const c_headerSizeExpected = sizeof(header);
						s32 const c_headerSizeWritten = CFileMgr::Write( hFile, (char*)&header, c_headerSizeExpected );
						resultFlags |= c_headerSizeExpected == c_headerSizeWritten ? 0 : CReplayEditorParameters::PROJECT_SAVE_RESULT_WRITE_FAIL;

						u64 const c_optOutFlags = CReplayEditorParameters::PROJECT_SAVE_RESULT_UNSAFE_TO_CONTINUE;
						
						//! Clips...
						for( u32 clip = 0; ( resultFlags & c_optOutFlags ) == 0 && clip < header.m_numberOfClipObjects; ++clip )
						{
							resultFlags |= m_aClips[clip]->Save( hFile );
						}

						// Text...
						for( u32 textInd = 0; ( resultFlags & c_optOutFlags ) == 0 && textInd < header.m_numberOfTextObjects; ++textInd )
						{
							resultFlags |= m_aText[textInd]->Save( hFile );
						}

						// Music...
						for( u32 musicClip = 0; ( resultFlags & c_optOutFlags ) == 0 && musicClip < header.m_numberOfMusicClips; ++musicClip )
						{
							resultFlags |= m_musicClips[musicClip].Save( hFile );
						}

                        // Ambient...
                        for( u32 ambientClip = 0; ( resultFlags & c_optOutFlags ) == 0 && ambientClip < header.m_numberOfAmbientClips; ++ambientClip )
                        {
                            resultFlags |= m_ambientClips[ambientClip].Save( hFile );
                        }

						u64 const c_outputSize = CFileMgr::Tell( hFile );

						s32 const c_closeResult = CFileMgr::CloseFile( hFile );
						resultFlags |= c_closeResult == 0 ? 0 : CReplayEditorParameters::PROJECT_SAVE_RESULT_CLOSE_FAIL;

						m_userId = CReplayMgr::GetCurrentReplayUserId();

						if( !c_savingUnderNewName )
						{
							//Remove the old project with the same name as we'll re-init the new one
							ReplayFileManager::RemoveCachedProject( path, m_userId );
						}

						// Only re-add to the cache if it succeeded
						if( ( resultFlags & CReplayEditorParameters::PROJECT_SAVE_RESULT_FAILED ) == 0 )
                        {    
							ReplayFileManager::AddProjectToCache( path, m_userId, c_outputSize, (u32)GetCachedDurationMs() );
							ReplayFileManager::SetShouldUpdateMontagesSize();

							// Add clips to project cache
							for( u32 clipInd = 0; clipInd < header.m_numberOfClipObjects; ++clipInd )
							{
								CClip* clip = m_aClips[clipInd];
								if( clip )
								{
									ReplayFileManager::AddClipToProjectCache( path, m_userId, clip->GetUID() );
								}
							}

#if __BANK
							Displayf( "CMontage::Save - %s saved successfully. Calculating rating meta-data:", fileNameWithExt.getBuffer() );
							GetRating();
#endif
						}	
					}
				}
			}

            // Now we've saved, ensure our name is set up correctly
            SetName( szFilename );
		}	
	}

	CReplayEditorParameters::PrintProjectSaveResultStrings( resultFlags );
	return resultFlags;
}

// PURPOSE: Populate this montage with the contents of the given file.
// PARAMS:
//		szFilename - Name of the montage file to load.
//		previewOnly - True if the intention is to preview the contents of the file only, false if we want to load for editing.
// RETURNS:
//		u64 containing CReplayEditorParameters::ePROJECT_LOAD_RESULT_FLAGS representing load result
u64 CMontage::Load( const char* szFullPath, u64 const userId, bool const previewOnly, const char * const fileNameForCheck )
{
	u64 result = CReplayEditorParameters::PROJECT_LOAD_RESULT_INVALID_FILE;

	FileHandle hFile = szFullPath && szFullPath[0] != '\0' ? CFileMgr::OpenFile( szFullPath ) : NULL;
	if ( Verifyf( hFile, "CMontage::Load - Unable to open montage %s . Bad path?", szFullPath ) )
	{
		result = CReplayEditorParameters::PROJECT_LOAD_RESULT_SUCCESS;

		bool headerLoadSuccessful = false;
		MontageHeader_Current header;

		u32 versionNumber = 0;
		int const c_versionNumberSize = sizeof(versionNumber);
		result = CFileMgr::Read(hFile, (char*)&versionNumber, c_versionNumberSize ) == c_versionNumberSize ? result : CReplayEditorParameters::PROJECT_LOAD_RESULT_EARLY_EOF;

		if( ( result & CReplayEditorParameters::PROJECT_LOAD_RESULT_EARLY_EOF ) == 0 )
		{
			CFileMgr::Seek( hFile, 0 );

			switch( versionNumber )
			{
			case MONTAGE_VERSION:
                {
                    headerLoadSuccessful = 
                        defaulHeaderLoadFunction<MontageHeader_Current>( header, hFile, result );

                    result |= headerLoadSuccessful ? result : CReplayEditorParameters::PROJECT_LOAD_RESULT_BAD_VERSION;
                    break;
                }

            case 4:
                {
                    headerLoadSuccessful = 
                        defaulHeaderLoadFunction<MontageHeader_V4>( header, hFile, result );

                    result |= headerLoadSuccessful ? result : CReplayEditorParameters::PROJECT_LOAD_RESULT_BAD_VERSION;
                    break;
                }

            case 3: // Version 3 never changed the header structure, was just incremented for B/C camera purposes
            case 2:
				{
					headerLoadSuccessful = 
						defaulHeaderLoadFunction<MontageHeader_V2>( header, hFile, result );

					result |= headerLoadSuccessful ? result : CReplayEditorParameters::PROJECT_LOAD_RESULT_BAD_VERSION;
					break;
				}

			case 1:
				{
					headerLoadSuccessful = 
						defaulHeaderLoadFunction<MontageHeader_V1>( header, hFile, result );

					result |= headerLoadSuccessful ? result : CReplayEditorParameters::PROJECT_LOAD_RESULT_BAD_VERSION;
					break;
				}

			default:
				{
					rcWarningf( "CClip::Load - Encountered unsupported clip version %u", versionNumber );
					result |= CReplayEditorParameters::PROJECT_LOAD_RESULT_BAD_VERSION;
					headerLoadSuccessful = false;
					break;
				}
			}
		}

		if( headerLoadSuccessful )
		{
			result = header.m_versionBlob.m_montageVersion > MONTAGE_VERSION ? 
				CReplayEditorParameters::PROJECT_LOAD_RESULT_BAD_VERSION : result;

			result |= header.m_versionBlob.m_replayVersion > ReplayHeader::GetVersionNumber() ? CReplayEditorParameters::PROJECT_LOAD_RESULT_BAD_REPLAY_VERSION : result;

			result |= header.m_cachedDurationMs == 0 ? CReplayEditorParameters::PROJECT_LOAD_RESULT_BAD_CACHED_DURATION : result;

			if( rcVerifyf( ( result & CReplayEditorParameters::PROJECT_LOAD_RESULT_BAD_VERSION ) == 0, 
				"CMontage::Load - Montage Version %d greater than current version %d. Have you been abusing time-travel?", header.m_versionBlob.m_montageVersion, MONTAGE_VERSION ) && 
				rcVerifyf( ( result & CReplayEditorParameters::PROJECT_LOAD_RESULT_BAD_REPLAY_VERSION ) == 0, 
				"CMontage::Load - Replay Version %d greater than current version %d. Have you been abusing time-travel?", 
				header.m_versionBlob.m_replayVersion, ReplayHeader::GetVersionNumber() ) &&
				rcVerifyf( ( result & CReplayEditorParameters::PROJECT_LOAD_RESULT_BAD_CACHED_DURATION ) == 0, 
				"CMontage::Load - Cached duration is zero. Header is likely corrupt" ) )
			{
				m_cachedDurationMs = (float)header.m_cachedDurationMs;
				m_timeSpentEditingMs = header.m_timeSpentEditingMs;
				m_editTimeStampMs = fwTimer::GetSystemTimeInMilliseconds();
				m_userId = userId;

				m_szName = header.m_name;

				if( !previewOnly && fileNameForCheck )
				{
					MontageName nameBuffer( fileNameForCheck );
					if( m_szName != nameBuffer )
					{
						replayWarningf( "Project named %s, but cached name is %s. Player has likely been tampering with files...", 
							fileNameForCheck, m_szName.getBuffer() );

						m_szName = nameBuffer;
					}
				}

				//! Load the clips
				m_aClips.Reset();

				for( u32 clip = 0; ( result & CReplayEditorParameters::PROJECT_LOAD_RESULT_UNSAFE_TO_CONTINUE ) == 0 && 
					clip < header.m_numberOfClipObjects; ++clip )
				{
					if( clip < GetMaxClipCount() )
					{
						CClip* newClip = rage_new CClip();
						if( newClip )
						{
							u64 const c_clipLoadResult = 
							newClip->Load( hFile, header.m_versionBlob.m_clipVersion, header.m_clipObjectSize, 
								header.m_versionBlob.m_markerVersion, header.m_markerObjectSize, previewOnly );

							CReplayEditorParameters::PrintClipLoadResultStrings( c_clipLoadResult );

							result |= ( c_clipLoadResult & CReplayEditorParameters::CLIP_LOAD_RESULT_EARLY_EOF ) != 0 ?
								CReplayEditorParameters::PROJECT_LOAD_RESULT_EARLY_EOF : result;

							if( ( c_clipLoadResult & CReplayEditorParameters::CLIP_LOAD_RESULT_UNSAFE_TO_CONTINUE ) == 0 )
							{
								if( ( c_clipLoadResult & CReplayEditorParameters::CLIP_LOAD_RESULT_CULL_FROM_PROJECT ) == 0 )
								{
									// Translate across any clip load results we may want to let the user know about
									result |= ( c_clipLoadResult & CReplayEditorParameters::CLIP_LOAD_RESULT_MARKERS_RESET ) != 0 ?
										CReplayEditorParameters::PROJECT_LOAD_RESULT_CLIP_MARKERS_RESET : result;

									result |= ( c_clipLoadResult & CReplayEditorParameters::CLIP_LOAD_RESULT_MARKERS_TRUNCATED ) != 0 ?
										CReplayEditorParameters::PROJECT_LOAD_RESULT_CLIP_MARKERS_TRUNCATED : result;

									result |= ( c_clipLoadResult & CReplayEditorParameters::CLIP_LOAD_RESULT_BAD_TIME_DATA ) != 0 ?
										CReplayEditorParameters::PROJECT_LOAD_RESULT_CLIP_TIME_DATA_RESET : result;

									result |= ( c_clipLoadResult & CReplayEditorParameters::CLIP_LOAD_RESULT_MARKER_VERSION_UPGRADED ) != 0 ?
										CReplayEditorParameters::PROJECT_LOAD_RESULT_VERSION_UPGRADED : result;

									// We've taken ownership so nullify the pointer
									m_aClips.PushAndGrow( newClip );
									newClip = NULL;
								}
								else
								{
									result |= CReplayEditorParameters::PROJECT_LOAD_RESULT_CLIPS_TRUNCATED;
								}
							}
							else
							{
								result |= CReplayEditorParameters::PROJECT_LOAD_RESULT_UNSAFE_TO_CONTINUE | 
									CReplayEditorParameters::PROJECT_LOAD_RESULT_BAD_CLIP_DATA;
							}
						}
						else
						{
							result |= CReplayEditorParameters::PROJECT_LOAD_RESULT_OOM;
						}

						//! Clean up if ownership hasn't been taken from the inner loop
						if( newClip )
						{
							delete newClip;
						}
					}
					else
					{
						result |= CReplayEditorParameters::PROJECT_LOAD_RESULT_CLIPS_TRUNCATED;
					}
				}

				if( !previewOnly && ( result & CReplayEditorParameters::PROJECT_LOAD_RESULT_UNSAFE_TO_CONTINUE ) == 0 )
				{
					// Now the text
					m_aText.Reset();

					CMontageText loaderText;
					for( u32 textInd = 0; ( result & CReplayEditorParameters::PROJECT_LOAD_RESULT_UNSAFE_TO_CONTINUE ) == 0 && 
						textInd < header.m_numberOfTextObjects; ++textInd )
					{
						u64 const c_textLoadResult = 
							loaderText.Load( hFile, header.m_versionBlob.m_textVersion, header.m_textObjectSize );

						CReplayEditorParameters::PrintTextLoadResultStrings( c_textLoadResult );

						result |= ( c_textLoadResult & CReplayEditorParameters::TEXT_LOAD_RESULT_EARLY_EOF ) != 0 ?
							CReplayEditorParameters::PROJECT_LOAD_RESULT_EARLY_EOF : result;

						result |= ( c_textLoadResult & CReplayEditorParameters::TEXT_LOAD_RESULT_PROFANE ) != 0 ?
							CReplayEditorParameters::PROJECT_LOAD_RESULT_TEXT_HAD_PROFANITY : result;

						result |= ( c_textLoadResult & CReplayEditorParameters::TEXT_LOAD_RESULT_UNSUPPORTED_VERSION ) != 0 ?
							CReplayEditorParameters::PROJECT_LOAD_RESULT_TEXT_RESET : result;

						if( ( c_textLoadResult & CReplayEditorParameters::TEXT_LOAD_RESULT_FAILED ) == 0 )
						{
							// Ignore text outside supported range, or if the load was successful but we want to cull it anyway
							if( ( c_textLoadResult & CReplayEditorParameters::TEXT_LOAD_RESULT_CULL_FROM_PROJECT ) == 0 &&
								textInd < GetMaxTextCount() )
							{
								CMontageText* newText = rage_new CMontageText( loaderText );
								if( newText )
								{
									// Translate across any music load results we may want to let the user know about
									result |= ( c_textLoadResult & CReplayEditorParameters::TEXT_LOAD_RESULT_VERSION_UPGRADED ) != 0 ?
										CReplayEditorParameters::PROJECT_LOAD_RESULT_VERSION_UPGRADED : result;

									m_aText.PushAndGrow( newText );
								}
								else
								{
									result |= CReplayEditorParameters::PROJECT_LOAD_RESULT_OOM;
								}
							}
							else
							{
								result |= CReplayEditorParameters::PROJECT_LOAD_RESULT_TEXT_TRUNCATED;
							}
						}
						else
						{
							result |= 
								CReplayEditorParameters::PROJECT_LOAD_RESULT_UNSAFE_TO_CONTINUE | CReplayEditorParameters::PROJECT_LOAD_RESULT_BAD_TEXT_DATA;
						}
					}
				}

				if( !previewOnly && ( result & CReplayEditorParameters::PROJECT_LOAD_RESULT_UNSAFE_TO_CONTINUE ) == 0 )
				{
					// Music
					m_musicClips.Reset();
					CMontageMusicClip loaderMusic;

					for( u32 musicClipIndex = 0; ( result & CReplayEditorParameters::PROJECT_LOAD_RESULT_UNSAFE_TO_CONTINUE ) == 0 &&
						musicClipIndex < header.m_numberOfMusicClips; ++musicClipIndex )
					{
						u64 const c_musicLoadResult = 
							loaderMusic.Load( hFile, header.m_versionBlob.m_musicVersion, header.m_musicObjectSize );

						CReplayEditorParameters::PrintMusicLoadResultStrings( c_musicLoadResult );

						result |= ( c_musicLoadResult & CReplayEditorParameters::MUSIC_LOAD_RESULT_EARLY_EOF ) != 0 ?
							CReplayEditorParameters::PROJECT_LOAD_RESULT_EARLY_EOF : result;

						result |= ( c_musicLoadResult & CReplayEditorParameters::MUSIC_LOAD_RESULT_INVALID_SOUNDHASH ) != 0 ?
							CReplayEditorParameters::PROJECT_LOAD_RESULT_MUSIC_INVALID_SOUNDHASH : result;

						result |= ( c_musicLoadResult & CReplayEditorParameters::MUSIC_LOAD_RESULT_ALREADY_USED ) != 0 ?
							CReplayEditorParameters::PROJECT_LOAD_RESULT_MUSIC_ALREADY_USED : result;

						result |= ( c_musicLoadResult & CReplayEditorParameters::MUSIC_LOAD_RESULT_UNSUPPORTED_VERSION ) != 0 ?
							CReplayEditorParameters::PROJECT_LOAD_RESULT_MUSIC_RESET : result;

						if( ( c_musicLoadResult & CReplayEditorParameters::MUSIC_LOAD_RESULT_FAILED ) == 0 )
						{
							// Ignore music outside supported range, or if the load was successful but we want to cull it anyway
							if( ( c_musicLoadResult & CReplayEditorParameters::MUSIC_LOAD_RESULT_CULL_FROM_PROJECT ) == 0 &&
								musicClipIndex < GetMaxMusicClipCount() )
							{
								// Translate across any music load results we may want to let the user know about
								result |= ( c_musicLoadResult & CReplayEditorParameters::MUSIC_LOAD_RESULT_VERSION_UPGRADED ) != 0 ?
									CReplayEditorParameters::PROJECT_LOAD_RESULT_VERSION_UPGRADED : result;

								m_musicClips.PushAndGrow( loaderMusic );
							}
							else
							{
								result |= CReplayEditorParameters::PROJECT_LOAD_RESULT_MUSIC_TRUNCATED;
							}
						}
						else
						{
							result |= 
								CReplayEditorParameters::PROJECT_LOAD_RESULT_UNSAFE_TO_CONTINUE | CReplayEditorParameters::PROJECT_LOAD_RESULT_BAD_MUSIC_DATA;
						}
					}
				}

                if( !previewOnly && ( result & CReplayEditorParameters::PROJECT_LOAD_RESULT_UNSAFE_TO_CONTINUE ) == 0 )
                {
                    // Finally ambient tracks
                    m_ambientClips.Reset();
                    CMontageMusicClip loaderAmbient;

                    for( u32 ambientClipIndex = 0; ( result & CReplayEditorParameters::PROJECT_LOAD_RESULT_UNSAFE_TO_CONTINUE ) == 0 &&
                        ambientClipIndex < header.m_numberOfAmbientClips; ++ambientClipIndex )
                    {
                        u64 const c_ambientLoadResult = 
                            loaderAmbient.Load( hFile, header.m_versionBlob.m_musicVersion, header.m_musicObjectSize );

                        CReplayEditorParameters::PrintMusicLoadResultStrings( c_ambientLoadResult );

                        result |= ( c_ambientLoadResult & CReplayEditorParameters::MUSIC_LOAD_RESULT_EARLY_EOF ) != 0 ?
                            CReplayEditorParameters::PROJECT_LOAD_RESULT_EARLY_EOF : result;

                        result |= ( c_ambientLoadResult & CReplayEditorParameters::MUSIC_LOAD_RESULT_INVALID_SOUNDHASH ) != 0 ?
                            CReplayEditorParameters::PROJECT_LOAD_RESULT_MUSIC_INVALID_SOUNDHASH : result;

                        result |= ( c_ambientLoadResult & CReplayEditorParameters::MUSIC_LOAD_RESULT_ALREADY_USED ) != 0 ?
                            CReplayEditorParameters::PROJECT_LOAD_RESULT_MUSIC_ALREADY_USED : result;

                        result |= ( c_ambientLoadResult & CReplayEditorParameters::MUSIC_LOAD_RESULT_UNSUPPORTED_VERSION ) != 0 ?
                            CReplayEditorParameters::PROJECT_LOAD_RESULT_MUSIC_RESET : result;

                        if( ( c_ambientLoadResult & CReplayEditorParameters::MUSIC_LOAD_RESULT_FAILED ) == 0 )
                        {
                            // Ignore music outside supported range, or if the load was successful but we want to cull it anyway
                            if( ( c_ambientLoadResult & CReplayEditorParameters::MUSIC_LOAD_RESULT_CULL_FROM_PROJECT ) == 0 &&
                                ambientClipIndex < GetMaxMusicClipCount() )
                            {
                                // Translate across any music load results we may want to let the user know about
                                result |= ( c_ambientLoadResult & CReplayEditorParameters::MUSIC_LOAD_RESULT_VERSION_UPGRADED ) != 0 ?
                                    CReplayEditorParameters::PROJECT_LOAD_RESULT_VERSION_UPGRADED : result;

                                m_ambientClips.PushAndGrow( loaderAmbient );
                            }
                            else
                            {
                                result |= CReplayEditorParameters::PROJECT_LOAD_RESULT_MUSIC_TRUNCATED;
                            }
                        }
                        else
                        {
                            result |= 
                                CReplayEditorParameters::PROJECT_LOAD_RESULT_UNSAFE_TO_CONTINUE | CReplayEditorParameters::PROJECT_LOAD_RESULT_BAD_MUSIC_DATA;
                        }
                    }
                }
			}

            m_cachedVersionBlob = header.m_versionBlob;
		}
		else
		{
			// Assume file was too small for now since we only have a single header version
			result |= CReplayEditorParameters::CLIP_LOAD_RESULT_EARLY_EOF;
		}

		CFileMgr::CloseFile( hFile );
	}

	CReplayEditorParameters::PrintProjectLoadResultStrings( result );
	return result;
}

// PURPOSE: Validates the contents of this montage. Ensures all clips actually exist and can be loaded.
// RETURNS:
//		True if we were valid. False if we weren't valid and had to remove bad data.
bool CMontage::Validate()
{
	bool wasValid( true );

	for( s32 clipIndex = 0; clipIndex < GetClipCount(); )
	{
		CClip const* c_currentClip = GetClip( clipIndex );
		if( c_currentClip && c_currentClip->HasValidFileReference() )
		{
			++clipIndex;
		}
		else
		{
			DeleteClip(clipIndex);
			wasValid = false;
		}
	}

	return wasValid;
}

CMontage* CMontage::Create()
{
	return rage_new CMontage();
}

void CMontage::Destroy(CMontage*& pMontage)
{
	if (pMontage)
	{
		delete pMontage;
		pMontage = NULL;
	}
}

bool CMontage::IsValidMontageFile( u64 const userId, char const * const filename )
{
	bool const c_isValid = CReplayMgr::IsProjectFileValid( userId, filename, false ) && CReplayMgr::IsProjectFileValid( userId, filename, true );
	return c_isValid;
}

u32 CMontage::UpdateTimeSpentEditingMs()
{
	u32 const c_currentSysTimeMs = fwTimer::GetSystemTimeInMilliseconds();
	u64 const c_diffMs = c_currentSysTimeMs - m_editTimeStampMs;

	u64 const c_newTimeSpendEditingMs = m_timeSpentEditingMs + c_diffMs;
	
	m_timeSpentEditingMs = c_newTimeSpendEditingMs > UINT32_MAX ? (u32)UINT32_MAX : (u32)c_newTimeSpendEditingMs;
	m_editTimeStampMs = c_currentSysTimeMs;

	return m_timeSpentEditingMs;
}

u32 CMontage::GetRating()
{
	u32 uRating = 0;

	u32 auEditTimeScore[6][2] =
	{
		{ 10800, 50 },
		{  7200, 40 },
		{  3600, 30 },
		{  1800, 20 },
		{   900, 10 },
		{     0,  5 }
	};

	u32 auClipCountScore[6][2] =
	{
		{ 120, 50 },
		{ 60, 40 },
		{ 30, 30 },
		{ 15, 20 },
		{  7, 10 },
		{  0,  5 }
	};

	u32 auMarkerCountScore[6][2] =
	{
		{ 250, 50 },
		{ 125, 40 },
		{ 60, 30 },
		{ 30, 20 },
		{ 15, 10 },
		{  0,  5 }
	};

	// Edit Time
	u32 const uEditTimeInSec = UpdateTimeSpentEditingMs() / 1000;
	u32 diff(0);

	for (u32 uSlot = 0; uSlot < 6; uSlot++)
	{
		if (uEditTimeInSec >= auEditTimeScore[uSlot][0])
		{
			diff = auEditTimeScore[uSlot][1];
			Displayf( "Rating of %u based on time %u seconds spend editing", diff, uEditTimeInSec );

			uRating += diff;
			break;
		}
	}

	// Clip Count
	u32 const uClipCount = (u32)GetClipCount();

	for (u32 uSlot = 0; uSlot < 6; uSlot++)
	{
		if (uClipCount >= auClipCountScore[uSlot][0])
		{
			diff = auClipCountScore[uSlot][1];
			Displayf( "Rating of %u based on %u clips", diff, uClipCount );

			uRating += diff;
			break;
		}
	}

	// Marker Count
	u32 uTotalMarkers = 0;

	for (s32 iClip = 0; iClip < GetClipCount(); iClip++)
	{
		CClip* pClip = GetClip(iClip);
		uTotalMarkers += pClip->GetMarkerData()->GetCount();
	}

	for (u32 uSlot = 0; uSlot < 6; uSlot++)
	{
		if (uTotalMarkers >= auMarkerCountScore[uSlot][0])
		{
			diff = (u32)auMarkerCountScore[uSlot][1];
			Displayf( "Rating of %u based on %u markers", diff, uTotalMarkers );

			uRating += diff;
			break;
		}
	}

	Displayf( "Total Rating of %u", uRating );

	return uRating;
}

void CMontage::UpdateBespokeThumbnailCaptures()
{
	s32 const c_clipCount = GetClipCount();
	for( s32 clipIndex = 0; clipIndex < c_clipCount; ++clipIndex )
	{
		CClip* currentClip = GetClip( clipIndex );
		if( currentClip )
		{
			currentClip->UpdateBespokeCapture();
		}
	}
}

CMontage::CMontage() 
	: m_timeSpentEditingMs( 0 )
    , m_szName( "untitled" )
{
	m_aClips.Reset();
	m_aText.Reset();
	m_musicClips.Reset();
	g_FrontendAudioEntity.SetSpeechOn(true);

	m_editTimeStampMs = fwTimer::GetSystemTimeInMilliseconds();
    m_cachedVersionBlob.MakeCurrent();
}

CClip* CMontage::AddClipCopy( CClip const& clip ) 
{
	return AddClipCopy( clip, m_aClips.size() );
}

CMontageText* CMontage::AddTextCopy( CMontageText const * const pText) 
{
	CMontageText* newText = NULL;

	if( rcVerifyf( pText, "CMontage::AddTextCopy - Unable to copy from null text object" ) )
	{
		newText = rage_new CMontageText(*pText);
		if( rcVerifyf( newText, "CMontage::AddText Unable to allocate CMontageText" ) )
		{
			m_aText.Grow();
			m_aText[m_aText.GetCount() - 1] = newText;
		}
	}

	return newText;
}

size_t CMontage::ApproximateSizeForSavingInternal( MontageHeader_Current const& header ) const
{
	size_t sizeNeeded = 0;

	// following section must match save writing
	sizeNeeded = sizeof(header);

	for (u32 clipIndex = 0; clipIndex < header.m_numberOfClipObjects; ++clipIndex)
	{
		CClip const* currentClip = clipIndex < m_aClips.size() ? m_aClips[ clipIndex ] : NULL;
		sizeNeeded += currentClip ? currentClip->ApproximateSizeForSaving() : 0;
	}

	for( u32 textIndex = 0; textIndex < header.m_numberOfTextObjects; ++textIndex )
	{
		CMontageText const* montageText = textIndex < m_aText.size() ? m_aText[ textIndex ] : NULL;
		sizeNeeded += montageText ? montageText->ApproximateSizeForSaving() : 0;
	}

	for( u32 musicIndex = 0; musicIndex < header.m_numberOfMusicClips; ++musicIndex )
	{
		if( musicIndex < m_musicClips.size() )
		{
			CMontageMusicClip const& montageMusic = m_musicClips[ musicIndex ];
			sizeNeeded += montageMusic.ApproximateSizeForSaving();
		}
	}

	return sizeNeeded;
}

void CMontage::MarkClipAsUsedInProject( CClip const& clip )
{
	(void)clip;
	/*PathBuffer directory;
	CVideoEditorUtil::getVideoProjectDirectoryForCurrentUser( directory.getBufferRef() );

	PathBuffer path( "%s%s%s", directory.getBuffer(), GetName(), STR_MONTAGE_FILE_EXT );
	m_userId = ReplayFileManager::getUserIdFromPath( path.getBuffer() );
	ReplayFileManager::AddClipToProjectCache( path.getBuffer(), m_userId, clip.GetUID() );*/
}

void CMontage::MarkClipAsNotUsedInProject( CClip const& clip )
{
	(void)clip;
	/*PathBuffer directory;
	CVideoEditorUtil::getVideoProjectDirectoryForCurrentUser( directory.getBufferRef() );

	PathBuffer path( "%s%s%s", directory.getBuffer(), GetName(), STR_MONTAGE_FILE_EXT );
	m_userId = ReplayFileManager::getUserIdFromPath( path.getBuffer() );
	ReplayFileManager::RemoveClipFromProjectCache( path.getBuffer(), m_userId, clip.GetUID() );*/
}

#endif // GTA_REPLAY
