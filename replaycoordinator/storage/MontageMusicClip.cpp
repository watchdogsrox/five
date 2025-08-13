//========================================================================================================================
// name:		MontageMusicClip.cpp
// description:	Music display over a montage sequence.
//========================================================================================================================
#include "MontageMusicClip.h"

#if GTA_REPLAY

// rage
#include "math/amath.h"

// game
#include "audio/radiotrack.h"
#include "replaycoordinator/replay_coordinator_channel.h"
#include "replaycoordinator/ReplayAudioTrackProvider.h"
#include "replaycoordinator/ReplayEditorParameters.h"
#include "text/text.h"

REPLAY_COORDINATOR_OPTIMISATIONS();

template< typename _bcMusicDataType > static void defaultMusicLoadFunction( FileHandle hFile, size_t const serializedSize, 
																		   CMontageMusicClip::MusicClipData_Current& inout_targetData, u64& inout_resultFlags )
{
	_bcMusicDataType dataBlob;
	size_t const c_expectedSize = sizeof(dataBlob);

	inout_resultFlags |= c_expectedSize == serializedSize ? inout_resultFlags : CReplayEditorParameters::MUSIC_LOAD_RESULT_BAD_SIZE;

	if( rcVerifyf( ( inout_resultFlags & CReplayEditorParameters::MUSIC_LOAD_RESULT_BAD_SIZE ) == 0, 
		"CMontageMusicClip::Load (defaultMusicLoadFunction) - Size change without version change? Not supported!"
		"Expected %" SIZETFMT "u but serialized as %" SIZETFMT "u", c_expectedSize, serializedSize ) )
	{
		// Read in all data
		s32 sizeRead = CFileMgr::Read( hFile, (char*)(&dataBlob), c_expectedSize );

		inout_resultFlags |= sizeRead != c_expectedSize ? CReplayEditorParameters::MUSIC_LOAD_RESULT_EARLY_EOF : inout_resultFlags;
		if( ( inout_resultFlags & CReplayEditorParameters::MUSIC_LOAD_RESULT_EARLY_EOF ) == 0 )
		{
			inout_targetData = dataBlob;
		}
	}
}

CMontageMusicClip::CMontageMusicClip() 
	: m_data()
{

}

CMontageMusicClip::CMontageMusicClip(  u32 const soundHash, s32 const startTimeMs )
	: m_data( soundHash, startTimeMs, CReplayAudioTrackProvider::GetMusicTrackDurationMs( soundHash ), 0u )
{
    Validate();
}

CMontageMusicClip::CMontageMusicClip( CMontageMusicClip const& other )
	: m_data( other.m_data )
{
	Validate();
}

CMontageMusicClip::~CMontageMusicClip()
{
}

CMontageMusicClip& CMontageMusicClip::operator=( CMontageMusicClip const& rhs ) 
{
	if( this != &rhs )
	{
		m_data = rhs.m_data;
	}

	return *this;
}

void CMontageMusicClip::Validate()
{
    m_data.m_durationMs = rage::Clamp<u32>(  m_data.m_durationMs, getMinDurationMs(), getMaxDurationMs() );
}

void CMontageMusicClip::Invalidate()
{
	m_data.Invalidate();
}

size_t CMontageMusicClip::ApproximateMinimumSizeForSaving()
{
	size_t const c_sizeNeeded = sizeof(MusicClipData_Current);
	return c_sizeNeeded;
}

size_t CMontageMusicClip::ApproximateMaximumSizeForSaving()
{
	size_t const c_sizeNeeded = sizeof(MusicClipData_Current);
	return c_sizeNeeded;
}

size_t CMontageMusicClip::ApproximateSizeForSaving() const
{
	size_t const c_sizeNeeded = sizeof(MusicClipData_Current);
	return c_sizeNeeded;
}

u64 CMontageMusicClip::Save(FileHandle hFile)
{
	rcAssertf( hFile, "CMontageMusicClip::Save - Invalid handle");

	s32 const c_dataSizeExpected = sizeof(m_data);
	s32 const c_dataSizeWritten  = CFileMgr::Write( hFile, (char*)(&m_data), c_dataSizeExpected );

	u64 const c_result = c_dataSizeExpected == c_dataSizeWritten ? 
		CReplayEditorParameters::PROJECT_SAVE_RESULT_SUCCESS : CReplayEditorParameters::PROJECT_SAVE_RESULT_WRITE_FAIL;

	return c_result;
}

u64 CMontageMusicClip::Load( FileHandle hFile, u32 const serializedVersion, size_t const serializedSize )
{
	rcAssertf( hFile, "CMontageMusicClip::Load - Invalid handle" );

	u64 result = CReplayEditorParameters::MUSIC_LOAD_RESULT_SUCCESS;

	result = serializedVersion > MONTAGE_MUSIC_CLIP_VERSION ? 
		CReplayEditorParameters::MUSIC_LOAD_RESULT_BAD_VERSION : result;

	if( rcVerifyf( ( result & CReplayEditorParameters::MUSIC_LOAD_RESULT_BAD_VERSION ) == 0, 
		"CMontageMusicClip::Load - Music Clip Version %d greater than current version %d. Have you been abusing time-travel?", 
		serializedVersion, MONTAGE_MUSIC_CLIP_VERSION ) )
	{
		//! Store the offset before markers, in-case anything goes wrong
		s32 const c_offsetBefore = CFileMgr::Tell( hFile );
		bool isCorrupted = true;

		//! Current version
		if( serializedVersion == MONTAGE_MUSIC_CLIP_VERSION )
		{
			defaultMusicLoadFunction<MusicClipData_Current>( hFile, serializedSize, m_data, result );
			isCorrupted = ( result & CReplayEditorParameters::MUSIC_LOAD_RESULT_EARLY_EOF ) != 0;
		}
		else if( serializedVersion == 1 )
		{
			defaultMusicLoadFunction<MusicClipData_V1>( hFile, serializedSize, m_data, result );
			isCorrupted = ( result & CReplayEditorParameters::MUSIC_LOAD_RESULT_EARLY_EOF ) != 0;

			result |= isCorrupted ? result : CReplayEditorParameters::MUSIC_LOAD_RESULT_VERSION_UPGRADED;
		}
		else
		{
			rcWarningf( "CMontageMusicClip::Load - Encountered unsupported music track version %d, handling it as corrupted", serializedVersion );
			result |= CReplayEditorParameters::MUSIC_LOAD_RESULT_UNSUPPORTED_VERSION;
		}

		if( !isCorrupted )
		{
			CReplayAudioTrackProvider::eTRACK_VALIDITY_RESULT const c_trackValidResult = CReplayAudioTrackProvider::IsValidSoundHash( m_data.m_soundHash );
				
			if( c_trackValidResult == CReplayAudioTrackProvider::TRACK_VALIDITY_VALID )
			{
				CReplayAudioTrackProvider::UpdateTracksUsedInProject( m_data.m_soundHash, true );
			}
			else
			{
				isCorrupted = true;
			}

			result |= c_trackValidResult == CReplayAudioTrackProvider::TRACK_VALIDITY_ALREADY_USED ? CReplayEditorParameters::MUSIC_LOAD_RESULT_ALREADY_USED : 
				c_trackValidResult == CReplayAudioTrackProvider::TRACK_VALIDITY_INVALID ? CReplayEditorParameters::MUSIC_LOAD_RESULT_INVALID_SOUNDHASH : result;
		}

		if( isCorrupted )
		{
			// Skip unsupported/corrupt versions
			s32 const c_offset = (s32)c_offsetBefore + (s32)serializedSize;
			s32 const c_newPos = CFileMgr::Seek( hFile, c_offset );

			result |= c_offset != c_newPos ? CReplayEditorParameters::MUSIC_LOAD_RESULT_EARLY_EOF : result;
			Invalidate();
		}
        else
        {
            Validate();
        }
	}

	return result;
}

void CMontageMusicClip::setTrimmedDurationMs( u32 const durationTimeMs )
{
	m_data.m_durationMs = durationTimeMs;
    Validate();
}

u32 CMontageMusicClip::getMinDurationMs() const
{
    return IsAmbientTrack() ? MIN_AMBIENT_DURATION_MS : IsRadioTrack() ? MIN_MUSIC_DURATION_MS : MIN_SCORE_DURATION_MS;
}

u32 CMontageMusicClip::getMaxDurationMs() const
{
    return IsAmbientTrack() ? MAX_AMBIENT_DURATION_MS : CReplayAudioTrackProvider::GetMusicTrackDurationMs( getSoundHash() );
}

u32 CMontageMusicClip::getTrackTotalDurationMs() const
{
	return IsAmbientTrack() ? MAX_AMBIENT_DURATION_MS : CReplayAudioTrackProvider::GetMusicTrackDurationMs( getSoundHash() );
}

bool CMontageMusicClip::IsRadioTrack() const
{
	eReplayMusicType const c_musicType = CReplayAudioTrackProvider::GetMusicType( m_data.m_soundHash );
	bool const c_isCommercial = audRadioTrack::IsCommercial( m_data.m_soundHash );

	// Duration is valid if it's a commercial, if it's a score or if it's over our threshold
	bool const c_isRadioTrack = c_musicType == REPLAY_RADIO_MUSIC && !c_isCommercial;
	return c_isRadioTrack;
}

bool CMontageMusicClip::IsCommercialTrack() const
{
	bool const c_isCommercialTrack = CReplayAudioTrackProvider::IsCommercial( m_data.m_soundHash );
	return c_isCommercialTrack;
}

bool CMontageMusicClip::IsScoreTrack() const
{
	eReplayMusicType const c_musicType = CReplayAudioTrackProvider::GetMusicType( m_data.m_soundHash );

	// Duration is valid if it's a commercial, if it's a score or if it's over our threshold
	bool const c_isScoreTrack = c_musicType == REPLAY_SCORE_MUSIC;
	return c_isScoreTrack;
}

bool CMontageMusicClip::IsAmbientTrack() const
{
    eReplayMusicType const c_musicType = CReplayAudioTrackProvider::GetMusicType( m_data.m_soundHash );

    // Duration is valid if it's a commercial, if it's a score or if it's over our threshold
    bool const c_isAmbientTrack = c_musicType == REPLAY_AMBIENT_TRACK;
    return c_isAmbientTrack;
}

bool CMontageMusicClip::ContainsBeatMarkers() const
{
	bool const c_containsBeatMarkers = CReplayAudioTrackProvider::HasBeatMarkers( m_data.m_soundHash );
	return c_containsBeatMarkers;
}

#endif // GTA_REPLAY
