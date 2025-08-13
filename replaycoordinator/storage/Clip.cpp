//========================================================================================================================
// name:		Clip.cpp
// description:	Clip class.
//========================================================================================================================
#include "Clip.h"

#if GTA_REPLAY

// rage
#include "audiohardware/driverutil.h"
#include "debug/Debug.h"
#include "file/device.h"
#include "file/stream.h"
#include "grcore/image.h"
#include "math/amath.h"
#include "stdio.h"

// game
#include "audio/northaudioengine.h"
#include "control/replay/replay.h"
#include "control/replay/File/ReplayFileManager.h"
#include "renderer/ScreenshotManager.h"
#include "replaycoordinator/ReplayCoordinator.h"
#include "replaycoordinator/replay_coordinator_channel.h"
#include "SaveLoad/savegame_photo_buffer.h"
#include "SaveLoad/savegame_photo_manager.h"

REPLAY_COORDINATOR_OPTIMISATIONS();

// NOTE - Returns true if marker data is considered corrupted
template< typename _bcMarkerDataType > static bool defaultMarkerLoadFunction( CClip& ownerClip, FileHandle hFile, u32 const markerCount, 
																				   size_t const serializedSize, u64& inout_resultFlags )
{
	u64 const c_earlyOutCompareValue = CReplayEditorParameters::CLIP_LOAD_RESULT_OVERLAPPING_MARKERS | 
						CReplayEditorParameters::CLIP_LOAD_RESULT_BAD_MARKER_SIZE | CReplayEditorParameters::CLIP_LOAD_RESULT_UNSAFE_TO_CONTINUE;

	_bcMarkerDataType bcData;
	s32 dummyIndex;

	s32 const c_expectedSize = sizeof(_bcMarkerDataType);

	for( u32 i = 0; ( inout_resultFlags & c_earlyOutCompareValue ) == 0 && i < markerCount; ++i )
	{
		s32 const c_markerSizeRead = CFileMgr::Read( hFile, (char*)(&bcData), c_expectedSize );
		if( c_markerSizeRead >= c_expectedSize )
		{
			s32 const c_sizeDiff = (s32)serializedSize - c_markerSizeRead;
			if( rcVerifyf( c_sizeDiff == 0,
				"CClip::Load (defaultMarkerLoadFunction) - "
				"Size difference when de-serializing marker. Padding hasn't been accounted for in backwards compatible load?"
				" Read %d, expected %" SIZETFMT "u", c_markerSizeRead, serializedSize ) )
			{
				if( i < CClip::MAX_MARKERS )
				{
					sReplayMarkerInfo* newMarker = rage_new sReplayMarkerInfo( bcData );
					if( rcVerifyf( newMarker, "CClip::Load (defaultMarkerLoadFunction) - Unable to allocate marker. Things are about to get very bad." ) )
					{
						if( !ownerClip.AddMarker( newMarker, true, i == markerCount - 1, false, dummyIndex ) )
						{
							rcDisplayf( "CClip::Load (defaultMarkerLoadFunction) - Overlapping marker data exists in clip! This likely means corrupt marker data" );
							delete newMarker;
							inout_resultFlags |= CReplayEditorParameters::CLIP_LOAD_RESULT_OVERLAPPING_MARKERS;
						}
					}
					else
					{
						inout_resultFlags |= CReplayEditorParameters::CLIP_LOAD_RESULT_OOM;
					}
				}
				else
				{
					inout_resultFlags |= CReplayEditorParameters::CLIP_LOAD_RESULT_TOO_MANY_MARKERS;
				}
			}
			else
			{
				inout_resultFlags |= CReplayEditorParameters::CLIP_LOAD_RESULT_BAD_MARKER_SIZE;
			}
		}
		else
		{
			inout_resultFlags |= CReplayEditorParameters::CLIP_LOAD_RESULT_EARLY_EOF; 
		}
		
	}

	return ( inout_resultFlags & c_earlyOutCompareValue ) != 0;
}

// NOTE - Returns true if header data is considered corrupted
template< typename _bcHeaderDataType > static bool defaulHeaderLoadFunction( CClip::ClipHeader_Current& out_header, FileHandle hFile,
																			 u64& inout_resultFlags )
{
	u64 const c_errorMask = CReplayEditorParameters::CLIP_LOAD_RESULT_UNSAFE_TO_CONTINUE;

	_bcHeaderDataType bcData;

	s32 const c_expectedSize = sizeof(_bcHeaderDataType);

	s32 const c_headerSizeRead = CFileMgr::Read( hFile, (char*)(&bcData), c_expectedSize );
	if( c_headerSizeRead >= c_expectedSize )
	{
		out_header = bcData;
	}
	else
	{
		inout_resultFlags |= CReplayEditorParameters::CLIP_LOAD_RESULT_EARLY_EOF; 
	}

	return ( inout_resultFlags & c_errorMask ) == 0;
}

// ClipHeader
void CClip::ClipHeader_Current::CopyData( ClipHeader_V2 const& rhs )
{
	m_clipUID = rhs.m_clipUID;
	m_ownerId = 0; // V2 pre-dates owner ID's, but also only shipped on PC where user ID is 0
	m_numberOfMarkers = rhs.m_numberOfMarkers;
	m_rawDurationMs = rhs.m_rawDurationMs;
	m_thumbnailBufferSize = rhs.m_thumbnailBufferSize;
	m_name = rhs.m_name;
}

void CClip::ClipHeader_Current::CopyData( ClipHeader_V1 const& rhs )
{
	m_numberOfMarkers = rhs.m_numberOfMarkers;
	m_rawDurationMs = rhs.m_clipTimeMs;
	m_thumbnailBufferSize = rhs.m_thumbnailBufferSize;
	m_name = rhs.m_name;

	// V1 pre-dates owner ID's, but also only shipped on PC where user ID is 0
	m_ownerId = 0;

	// V1 pre-dates unique ID's, so just take whatever UID we can get
	ClipUID const * const uid = CReplayMgr::GetClipUID( m_ownerId, m_name.getBuffer() );
	m_clipUID = uid ? *uid : ClipUID();
}

bool CClip::ClipHeader_Current::IsValid() const
{
	bool const c_baseValidity = m_rawDurationMs > 0 && !m_name.IsNullOrEmpty();

	ClipUID const * const uid = c_baseValidity ? CReplayMgr::GetClipUID( m_ownerId, m_name.getBuffer() ) : NULL;
	u32 const c_cachedDurationMs = c_baseValidity ? CReplayMgr::LoadReplayLengthFromFile( m_ownerId, m_name.getBuffer() ) : 0;

	bool const c_result = c_baseValidity && m_rawDurationMs == c_cachedDurationMs && uid && m_clipUID == *uid;

	if( !c_result )
	{
		rcDisplayf( "CClip::ClipHeader_Current::IsValid - Failed on clip %s. Duration and name was %s, uid was %u, expected %u and cached duration matching raw duration was %u, expected %u.", 
			m_name.getBuffer(), c_baseValidity ? "valid" : "invalid", m_clipUID.get(), uid ? uid->get() : 0, m_rawDurationMs, c_cachedDurationMs );
	}

	return c_result;
}

// CClip
CClip::CClip() 
	: m_szName()
	, m_clipUID()
	, m_aoMarkers()
	, m_rawDurationMs( 0 )
	, m_startTimeMs( 0 )
	, m_endTimeMs( 33 )
	, m_thumbnailBuffer( NULL )
	, m_thumbnailBufferSize( 0 )
	, m_ownerId( 0 )
{
	
}

CClip::CClip( CClip const& other )
	: m_szName( other.m_szName )
	, m_clipUID( other.m_clipUID )
	, m_aoMarkers()
	, m_rawDurationMs( other.m_rawDurationMs )
	, m_startTimeMs( other.m_startTimeMs )
	, m_endTimeMs( other.m_endTimeMs )
	, m_thumbnailBuffer( NULL )
	, m_thumbnailBufferSize( 0 )
	, m_ownerId( other.m_ownerId )
{
	CopyMarkerData( other );
	CopyBespokeThumbnail( other );
}

CClip::~CClip()
{
	Clear();
}

CClip& CClip::operator=(const CClip& rhs) 
{
	if (this != &rhs)
	{
		SetName(rhs.GetName());
		m_clipUID = rhs.GetUID();

		// Time
		SetRawDurationMs( rhs.GetRawDurationMs() );

		// Real Time
		m_startTimeMs = rhs.m_startTimeMs;
		m_endTimeMs = rhs.m_endTimeMs;
		m_ownerId = rhs.m_ownerId;

		SetStartTransition( rhs.GetStartTransition() );
		SetStartTransitionDurationMs( rhs.GetStartTransitionDurationMs() );
		SetEndTransition( rhs.GetEndTransition() );
		SetEndTransitionDurationMs( rhs.GetEndTransitionDurationMs() );

		CopyMarkerData( rhs );
		CopyBespokeThumbnail( rhs );
	}

	return *this;
}

bool CClip::Populate( char const * const szFileName, u64 const ownerId,
							  ReplayMarkerTransitionType const eInType, ReplayMarkerTransitionType const eOutType )
{
	bool success = false;

	PathBuffer szClipName( szFileName );
	fwTextUtil::TruncateFileExtension( szClipName.getWritableBuffer() );
	SetName(szClipName);
	SetOwnerId( ownerId );

	ClipUID const * const uid = CReplayMgr::GetClipUID( ownerId, szClipName );
	m_clipUID = uid ? *uid : ClipUID::INVALID_UID;

	float const durationMs = CReplayMgr::LoadReplayLengthFromFileFloat( ownerId, szClipName );
	if( m_clipUID.IsValid() && durationMs > 0.f )
	{
		SetRawDurationMs( durationMs );

		CleanupMarkers();
		AddMarkersForStartAndEndPoints();

		SetEndNonDilatedTimeMs( durationMs );

		SetStartTransition(eInType);
		SetStartTransitionDurationMs( CLIP_TRANSITION_DEFAULT_DURATION_MS );

		SetEndTransition(eOutType);
		SetEndTransitionDurationMs( CLIP_TRANSITION_DEFAULT_DURATION_MS );
		ValidateTimeData();

		success = true;
	}

	return success;
}

bool CClip::IsValid() const
{
	bool const valid = IsValidName() && GetRawDurationMs() > 0 && GetEndNonDilatedTimeMs() > 0 &&
										GetEndTimeMs() > 0;

	return valid;
}

bool CClip::HasValidFileReference() const
{
	char const * const c_clipName = GetName();
	bool const c_hasValidPath = IsValid() && CReplayMgr::IsClipFileValid( m_ownerId, c_clipName ) && CReplayMgr::IsClipFileValid( m_ownerId, c_clipName, true );
	return c_hasValidPath;
}

bool CClip::HasModdedContent() const
{
	char const * const c_clipName = GetName();
	bool const c_hasModdedContent = CReplayMgr::DoesClipHaveModdedContent( m_ownerId, c_clipName );
	return c_hasModdedContent;
}

//////////////////////////////////////////////////////////////////////////
// Markers
void CClip::CopyMarkerData( u32 const index, sReplayMarkerInfo& out_destMarker )
{
    sReplayMarkerInfo const * const c_marker = TryGetMarker( index );
    out_destMarker = c_marker ? *c_marker : out_destMarker;
}

void CClip::PasteMarkerData( u32 const indexToPasteTo, sReplayMarkerInfo const&  sourceMarker, eMarkerCopyContext const context  )
{
    sReplayMarkerInfo * const  marker = TryGetMarker( indexToPasteTo );
    if( marker )
    {
        marker->CopyData( sourceMarker, context );
    }
}

bool CClip::AddMarker( bool const sortMarkersAfterAdd, bool const inheritMarkerProperties, float const nonDilatedTimeMs, s32& out_Index )
{
	bool newMarkerAdded = false;

	sReplayMarkerInfo* newMarker = GetMarkerCount() < GetMaxMarkerCount() ? rage_new sReplayMarkerInfo() : NULL;
	if( newMarker )
	{
		newMarker->SetNonDilatedTimeMs( nonDilatedTimeMs );
		newMarkerAdded = AddMarker( newMarker, false, sortMarkersAfterAdd, inheritMarkerProperties, out_Index );

		if( !newMarkerAdded )
		{
			delete newMarker;
		}
	}

	return newMarkerAdded;
}

bool CClip::AddMarker( bool const sortMarkersAfterAdd, bool const inheritMarkerProperties, s32& out_Index )
{
	return AddMarker( sortMarkersAfterAdd, inheritMarkerProperties, CReplayMgr::GetCurrentTimeRelativeMsFloat(), out_Index );
}

bool CClip::AddMarker( sReplayMarkerInfo* marker, bool const ignoreBoundary, bool const sortMarkersAfterAdd, bool const inheritMarkerProperties, s32& out_Index )
{
	bool newMarkerAdded = false;

	if( marker && CanPlaceMarkerAtNonDilatedTimeMs( marker->GetNonDilatedTimeMs(), ignoreBoundary ) )
	{
		m_aoMarkers.PushAndGrow( marker );

		if( sortMarkersAfterAdd )
		{
			m_aoMarkers.QSort( 0, -1, ReplayMarkerCompareFunct );
		}

		out_Index = (s32)m_aoMarkers.Find( marker );

		if( inheritMarkerProperties && out_Index > 0 )
		{
			sReplayMarkerInfo const* previousMarker = TryGetMarker( out_Index - 1 );
			sReplayMarkerInfo const* nextMarker = NULL;
			if (out_Index<(m_aoMarkers.GetCount()-1))
			{
				nextMarker = TryGetMarker( out_Index + 1 );
			}
			
			if( previousMarker )
			{
				marker->InterhitProperties( *previousMarker, nextMarker );
			}
		}
		newMarkerAdded = true;
	}

	return newMarkerAdded;
}

void CClip::RemoveMarker( u32 const index )
{
	u32 const c_markerCount = GetMarkerCount();
	//! First and last markers can never be removed, as they are our transition points!
	if( c_markerCount > 2 && index != 0 && index != c_markerCount - 1 )
	{
		bool const c_wasSecondLastMarker = ( index == c_markerCount - 2);
		sReplayMarkerInfo* marker = PopMarker( index );
		if( marker )
		{
			delete marker;

			if( c_wasSecondLastMarker )
			{
				// If we removed the second last marker we need to call the below function
				// to ensure that the dependency between the last marker and second-last marker is maintained
				BlendTypeUpdatedOnMarker( index - 1 );
			}
		}
	}
}

void CClip::ResetMarkerToDefaultState( u32 const markerIndex )
{
	sReplayMarkerInfo* markerToReset = TryGetMarker( markerIndex );
	sReplayMarkerInfo* nextMarker = TryGetMarker( markerIndex + 1 );
	if( rcVerifyf( markerToReset, "CClip::ResetMarkerToDefaultState - Attempting to reset unsupported marker type?" ) )
	{
		sReplayMarkerInfo defaultMarker;

		// Set any values we need to keep
		defaultMarker.SetNonDilatedTimeMs( markerToReset->GetNonDilatedTimeMs() );

		// If we are the last marker, we need to do some other special mojo
		if( nextMarker == NULL )
		{
			defaultMarker.SetCameraType( (ReplayCameraType)markerToReset->GetCameraType() );
		}

		// Reset via the inherit path
		markerToReset->InterhitProperties( defaultMarker, nextMarker );
	}
}

bool CClip::CanPlaceMarkerAtNonDilatedTimeMs( float const nonDilatedTimeMs, bool const ignoreBoundary ) const
{
	s32 const c_previousMarkerIndex = GetClosestMarkerIndex( nonDilatedTimeMs, CLOSEST_MARKER_LESS_OR_EQUAL );
	s32 const c_nextMarkerIndex = GetClosestMarkerIndex( nonDilatedTimeMs, CLOSEST_MARKER_MORE_OR_EQUAL );

	sReplayMarkerInfo const * const c_previousMarker = TryGetMarker( c_previousMarkerIndex );
	sReplayMarkerInfo const * const c_nextMarker = TryGetMarker( c_nextMarkerIndex );

	float const c_prevMinBoundaryNonDilatedTimeMs = c_previousMarker && !ignoreBoundary ? 
		c_previousMarker->GetFrameBoundaryForThisMarker() : MARKER_MIN_BOUNDARY_MS;

	float const c_currentMinBoundaryNonDilatedTimeMs = c_nextMarker && !ignoreBoundary ? 
		c_nextMarker->GetFrameBoundaryForThisMarker() : MARKER_MIN_BOUNDARY_MS;

	float const c_lowerBoundMs = c_previousMarker ? c_previousMarker->GetNonDilatedTimeMs() + c_prevMinBoundaryNonDilatedTimeMs : 0.f;
	float const c_upperBoundMs = c_nextMarker ? c_nextMarker->GetNonDilatedTimeMs() - c_currentMinBoundaryNonDilatedTimeMs : GetRawDurationMs();

	bool const c_canAdd = nonDilatedTimeMs >= c_lowerBoundMs && nonDilatedTimeMs <= c_upperBoundMs;
	return c_canAdd;
}

bool CClip::CanIncrementMarkerNonDilatedTimeMs( u32 const markerIndex, float const nonDilatedTimeDelta, float& inout_markerNonDilatedTime )
{
	bool success = false;

	sReplayMarkerInfo const* marker = TryGetMarker( markerIndex );
	if( marker )
	{
		float startConstraintNonDilatedTimeMs;
		float endConstraintNonDilatedTimeMs;

		GetPotentialMarkerNonDilatedTimeConstraintsMs( markerIndex, nonDilatedTimeDelta, 
				startConstraintNonDilatedTimeMs, endConstraintNonDilatedTimeMs );

		float const c_wantedMarkerTime = marker->GetNonDilatedTimeMs() + nonDilatedTimeDelta;
		float const c_newMarkerTime = Clamp( c_wantedMarkerTime, startConstraintNonDilatedTimeMs, endConstraintNonDilatedTimeMs );

		inout_markerNonDilatedTime = c_newMarkerTime;
		success = true;
	}

	return success;
}

bool CClip::GetMarkerNonDilatedTimeConstraintsMs( u32 const markerIndex, float& out_minNonDilatedTimeMs, float& out_maxNonDilatedTimeMs ) const
{
	bool success = false;

	sReplayMarkerInfo const* previousMarker = TryGetMarker( markerIndex - 1 );
	sReplayMarkerInfo const* marker = TryGetMarker( markerIndex );
	sReplayMarkerInfo const* nextMarker = TryGetMarker( markerIndex + 1 );
	if( marker )
	{
		bool const c_isStartMarker = previousMarker == NULL;
		bool const c_isEndMarker = nextMarker == NULL;

		float const c_startNonDilatedTimeMs = 0.f;
		float const c_endNonDilatedTimeMs = GetRawDurationMs();

		float const c_prevMinBoundaryNonDilatedTimeMs = previousMarker ? previousMarker->GetFrameBoundaryForThisMarker() : 0.f;
		float const c_currentMinBoundaryNonDilatedTimeMs = marker->GetFrameBoundaryForThisMarker();

		float const c_previousMarkerNonDilatedTimeMs = previousMarker ? previousMarker->GetNonDilatedTimeMs() : 0.f;
		float const c_nextMarkerNonDilatedTimeMs = nextMarker ? nextMarker->GetNonDilatedTimeMs() : c_endNonDilatedTimeMs;

		out_minNonDilatedTimeMs = c_previousMarkerNonDilatedTimeMs;
		out_maxNonDilatedTimeMs = c_nextMarkerNonDilatedTimeMs;

		out_minNonDilatedTimeMs = c_previousMarkerNonDilatedTimeMs > out_minNonDilatedTimeMs ? c_previousMarkerNonDilatedTimeMs : out_minNonDilatedTimeMs;
		out_maxNonDilatedTimeMs = c_nextMarkerNonDilatedTimeMs < out_maxNonDilatedTimeMs ? c_nextMarkerNonDilatedTimeMs : out_maxNonDilatedTimeMs;

		float const c_potentialMinNonDilatedTimeMs = c_isStartMarker ? c_startNonDilatedTimeMs : out_minNonDilatedTimeMs + c_prevMinBoundaryNonDilatedTimeMs;
		float const c_potentialMaxNonDilatedTimeMs = c_isEndMarker ? c_endNonDilatedTimeMs :  out_maxNonDilatedTimeMs - c_currentMinBoundaryNonDilatedTimeMs;

        out_minNonDilatedTimeMs = Clamp( c_potentialMinNonDilatedTimeMs, out_minNonDilatedTimeMs, out_maxNonDilatedTimeMs );
        out_maxNonDilatedTimeMs = Clamp( c_potentialMaxNonDilatedTimeMs, out_minNonDilatedTimeMs, out_maxNonDilatedTimeMs );

		out_minNonDilatedTimeMs = Clamp( out_minNonDilatedTimeMs, c_startNonDilatedTimeMs, c_endNonDilatedTimeMs );
		out_maxNonDilatedTimeMs = Clamp( out_maxNonDilatedTimeMs, c_startNonDilatedTimeMs, c_endNonDilatedTimeMs );

		success = true;
	}

	return success;
}

bool CClip::GetPotentialMarkerNonDilatedTimeConstraintsMs( u32 const markerIndex, float const nonDilatedTimeDelta, 
														  float& out_minNonDilatedTimeMs, float& out_maxNonDilatedTimeMs )
{
	bool success = false;

	sReplayMarkerInfo* marker = TryGetMarker( markerIndex );
	if( marker )
	{
		float const c_startingNonDilatedTimeForMarker = marker->GetNonDilatedTimeMs();

		// Calculate extents base on where we currently are
		GetMarkerNonDilatedTimeConstraintsMs( markerIndex, out_minNonDilatedTimeMs, out_maxNonDilatedTimeMs );

		// Clamp to current extents
		float const c_wantedMarkerNonDilatedTimeMs = marker->GetNonDilatedTimeMs() + nonDilatedTimeDelta;
		float const c_newMarkerNonDilatedTimeMs = Clamp( c_wantedMarkerNonDilatedTimeMs, out_minNonDilatedTimeMs, out_maxNonDilatedTimeMs );

		// Temporarily move the marker, calculate it's new extents
		marker->SetNonDilatedTimeMs( c_newMarkerNonDilatedTimeMs );
		success = GetMarkerNonDilatedTimeConstraintsMs( markerIndex, out_minNonDilatedTimeMs, out_maxNonDilatedTimeMs );

		// Restore the marker to it's original position
		marker->SetNonDilatedTimeMs( c_startingNonDilatedTimeForMarker );
	}

	return success;
}

float CClip::UpdateMarkerNonDilatedTimeForMarkerDragMs( u32 const markerIndex, float const clipPlaybackPercentage )
{
	float nonDilatedTimeMs = 0.f;

	//sReplayMarkerInfo* previousMarker = TryGetMarker( markerIndex - 1 );
	sReplayMarkerInfo* marker = TryGetMarker( markerIndex );
	if( marker )
	{
		// calculate the current marker percentage
		float markerPlaybackPercentage = CalculatePlaybackPercentageForMarker( markerIndex );

		float percentageDelta = clipPlaybackPercentage - markerPlaybackPercentage;

		float baseIncrement = MARKER_MIN_BOUNDARY_MS;
		float increment;
		float percentageThreshold = 0.0001f;

		bool iteratePosition = true;

		while( iteratePosition )
		{	
			increment = ( percentageDelta > 0.f ) ? baseIncrement : -baseIncrement;

			float const c_oldTimeMs = nonDilatedTimeMs;
			bool const c_canIncrement = CanIncrementMarkerNonDilatedTimeMs( markerIndex, increment, nonDilatedTimeMs );
			if( c_canIncrement && c_oldTimeMs != nonDilatedTimeMs )
			{
				marker->SetNonDilatedTimeMs( nonDilatedTimeMs );
				RecalculateDilatedTimeStartEndPoints();
				markerPlaybackPercentage = CalculatePlaybackPercentageForMarker( markerIndex );

				percentageDelta = clipPlaybackPercentage - markerPlaybackPercentage;
				iteratePosition = abs( percentageDelta ) > percentageThreshold;
				percentageThreshold += 0.00001f;
			}
			else
			{
				break;
			}
		}

		nonDilatedTimeMs = marker->GetNonDilatedTimeMs();
	}

	return nonDilatedTimeMs;
}

void CClip::SetMarkerNonDilatedTimeMs( u32 const markerIndex, float const newNonDilatedTimeMs )
{
	sReplayMarkerInfo* marker = TryGetMarker( markerIndex );
	if( marker )
	{
		marker->SetNonDilatedTimeMs( newNonDilatedTimeMs );
		RecalculateDilatedTimeStartEndPoints();
		ValidateTimeData();
	}
}

float CClip::GetMarkerDisplayTimeMs( u32 const markerIndex ) const
{
	float const c_displayTimeMs = CalculateTimeForMarkerMs( markerIndex ) + GetStartNonDilatedTimeMs();
	return c_displayTimeMs;
}

void CClip::IncrememtMarkerSpeed( u32 const markerIndex, s32 const delta )
{
	sReplayMarkerInfo* marker = TryGetMarker( markerIndex );
	if( marker )
	{
		marker->UpdateMarkerSpeed( delta );
		RecalculateDilatedTimeStartEndPoints();
	}
}

float CClip::GetMarkerFloatSpeedAtCurrentNonDilatedTimeMs( float const nonDilatedTimeMs ) const
{
	s32 const c_markerIndex = GetClosestMarkerIndex( nonDilatedTimeMs, CLOSEST_MARKER_LESS, NULL, NULL, true );
	sReplayMarkerInfo const* marker = TryGetMarker( c_markerIndex );

	float const c_speed = marker ? marker->GetSpeedValueFloat() : 1.f;
	return c_speed;
}

u32 CClip::GetMarkerSpeedAtCurrentNonDilatedTimeMs( float const nonDilatedTimeMs ) const
{
	s32 const c_markerIndex = GetClosestMarkerIndex( nonDilatedTimeMs, CLOSEST_MARKER_LESS, NULL, NULL, true );
	sReplayMarkerInfo const* marker = TryGetMarker( c_markerIndex );

	u32 const c_speed = marker ? marker->GetSpeedValueU32() : 100;
	return c_speed;
}

void CClip::RecalculateMarkerPositionsOnSpeedChange( u32 const startingMarkerIndex )
{
	s32 const c_markerCount = GetMarkerCount();
	s32 const c_offsetIndex = startingMarkerIndex > 0 ? startingMarkerIndex : 1;

	for( s32 index = c_offsetIndex; index < c_markerCount; ++index )
	{
		sReplayMarkerInfo* marker = TryGetMarker( index );

		float startConstraintNonDilatedTimeMs;
		float endConstraintNonDilatedTimeMs;
		GetMarkerNonDilatedTimeConstraintsMs( index, startConstraintNonDilatedTimeMs, endConstraintNonDilatedTimeMs );

		float const c_wantedMarkerNonDilatedTimeMs = marker->GetNonDilatedTimeMs();
		float const c_newMarkerNonDilatedTimeMs = Clamp( c_wantedMarkerNonDilatedTimeMs, startConstraintNonDilatedTimeMs, endConstraintNonDilatedTimeMs );
		marker->SetNonDilatedTimeMs( c_newMarkerNonDilatedTimeMs );
	}

	RecalculateDilatedTimeStartEndPoints();
}

void CClip::RemoveMarkersOutsideOfRangeNonDilatedTimeMS( float const startNonDilatedTimeMs, float const endNonDilatedTimeMs )
{
	for( s32 index = 0; index < GetMarkerCount(); )
	{
		sReplayMarkerInfo* pMarker = m_aoMarkers[index];
		if( pMarker && 
			( pMarker->GetNonDilatedTimeMs() < startNonDilatedTimeMs || pMarker->GetNonDilatedTimeMs() > endNonDilatedTimeMs ) )
		{
			ReplayMarkerInfoCollection::iterator iter = m_aoMarkers.begin() + index;
			m_aoMarkers.erase( iter );
			delete pMarker;
		}
		else
		{
			++index;
		}
	}

	RecalculateDilatedTimeStartEndPoints();
}

sReplayMarkerInfo* CClip::GetMarker( u32 const index )
{
	sReplayMarkerInfo* result = NULL;

	if( Verifyf( index < m_aoMarkers.size(), "CClip::GetMarker - Invalid marker index!" ) )
	{
		result = m_aoMarkers[ index ];
	}

	return result;
}

sReplayMarkerInfo const* CClip::GetMarker( u32 const index ) const
{
	sReplayMarkerInfo const* result = NULL;

	if( Verifyf( index < m_aoMarkers.size(), "CClip::GetMarker - Invalid marker index!" ) )
	{
		result = m_aoMarkers[ index ];
	}

	return result;
}

sReplayMarkerInfo* CClip::TryGetMarker( u32 const index )
{
	sReplayMarkerInfo* result = index < m_aoMarkers.size() ? m_aoMarkers[ index ] : NULL;
	return result;
}

sReplayMarkerInfo const* CClip::TryGetMarker( u32 const index ) const
{
	sReplayMarkerInfo const* result = index < m_aoMarkers.size() ? m_aoMarkers[ index ] : NULL;
	return result;
}

sReplayMarkerInfo* CClip::GetFirstMarker()
{
	return TryGetMarker( 0 );
}

sReplayMarkerInfo const* CClip::GetFirstMarker() const
{
	return TryGetMarker( 0 );
}

sReplayMarkerInfo* CClip::GetLastMarker()
{
	s32 markerCount = GetMarkerCount();
	return TryGetMarker( markerCount - 1 );
}

sReplayMarkerInfo const* CClip::GetLastMarker() const
{
	s32 markerCount = GetMarkerCount();
	return TryGetMarker( markerCount - 1 );
}

s32 CClip::GetPrevMarkerIndexWrapAround( u32 const startingIndex, float const startNonDilatedTimeMs, float const endNonDilatedTimeMs ) const
{
	s32 prevIndex = INDEX_NONE;

	bool const useTimeBoundaries = endNonDilatedTimeMs > startNonDilatedTimeMs;
	if( useTimeBoundaries )
	{
		if( startingIndex == 0 )
		{
			prevIndex = GetClosestMarkerIndex( endNonDilatedTimeMs, CLOSEST_MARKER_LESS_OR_EQUAL );
		}
		else
		{
			s32 tempIndex = startingIndex - 1;
			sReplayMarkerInfo const* replayMarker = TryGetMarker( tempIndex );
			
			prevIndex = replayMarker && replayMarker->GetNonDilatedTimeMs() > startNonDilatedTimeMs ? 
					tempIndex : GetClosestMarkerIndex( endNonDilatedTimeMs, CLOSEST_MARKER_LESS_OR_EQUAL );
		}
	}
	else
	{
		s32 const markerCount = GetMarkerCount();

		prevIndex = markerCount <= 0 ? INDEX_NONE :
			startingIndex <= 0 ? markerCount - 1 : startingIndex - 1;
	}

	return prevIndex;
}

s32 CClip::GetNextMarkerIndexWrapAround( u32 const startingIndex, float const startNonDilatedTimeMs, float const endNonDilatedTimeMs ) const
{
	s32 nextIndex = INDEX_NONE;

	bool const useTimeBoundaries = endNonDilatedTimeMs > startNonDilatedTimeMs;
	if( useTimeBoundaries )
	{
		if( startingIndex >= GetMarkerCount() - 1 )
		{
			nextIndex = GetClosestMarkerIndex( startNonDilatedTimeMs, CLOSEST_MARKER_MORE_OR_EQUAL );
		}
		else
		{
			s32 tempIndex = startingIndex + 1;
			sReplayMarkerInfo const* replayMarker = TryGetMarker( tempIndex );

			nextIndex = replayMarker && replayMarker->GetNonDilatedTimeMs() > startNonDilatedTimeMs ? 
				tempIndex : GetClosestMarkerIndex( startNonDilatedTimeMs, CLOSEST_MARKER_MORE_OR_EQUAL );
		}
	}
	else
	{
		s32 const markerCount = GetMarkerCount();

		nextIndex = markerCount <= 0 ? INDEX_NONE :
			startingIndex >= markerCount - 1 ? 0 : startingIndex + 1;
	}

	return nextIndex;
}

sReplayMarkerInfo* CClip::GetPrevMarkerWrapAround( u32 const startingIndex, float const startNonDilatedTimeMs, float const endNonDilatedTimeMs )
{
	s32 const c_prevIndex = GetPrevMarkerIndexWrapAround( startingIndex, startNonDilatedTimeMs, endNonDilatedTimeMs );
	return TryGetMarker( c_prevIndex );
}

sReplayMarkerInfo* CClip::GetNextMarkerWrapAround( u32 const startingIndex, float const startNonDilatedTimeMs, float const endNonDilatedTimeMs )
{
	s32 const c_nextIndex = GetNextMarkerIndexWrapAround( startingIndex, startNonDilatedTimeMs, endNonDilatedTimeMs );
	return TryGetMarker( c_nextIndex );
}

s32 CClip::GetNextMarkerIndex( u32 const startingIndex, bool const ignoreAnchors ) const
{
	s32 resultIndex = -1;

	u32 const c_markerCount = (u32)GetMarkerCount();
	for( u32 index = startingIndex + 1; resultIndex == -1 && index < c_markerCount; ++index )
	{
		sReplayMarkerInfo const* currentMarker = TryGetMarker( index );
		resultIndex = currentMarker && ( !ignoreAnchors || !currentMarker->IsAnchor() ) ? (s32)index : -1;
	}

	return resultIndex;
}

sReplayMarkerInfo* CClip::GetNextMarker( u32 const startingIndex, bool const ignoreAnchors )
{
	s32 const c_nextIndex = GetNextMarkerIndex( startingIndex, ignoreAnchors );
	return TryGetMarker( c_nextIndex );
}

s32 CClip::GetMarkerCount() const
{
	return m_aoMarkers.GetCount();
}

s32 CClip::GetMaxMarkerCount() const
{
	return (s32)MAX_MARKERS;
}

// PURPOSE: Returns the index of the first marker before the given time.
// PARAMS:
//		clipTimeMs - Time in ms representing the start of the valid time range to check
// RETURNS:
//		Index of the marker, or -1 if no appropriate marker found.
s32 CClip::GetFirstValidMarkerIndex( float const nonDilatedTimeMs ) const
{
	s32 result = INDEX_NONE;
	for( s32 index = 0; index < m_aoMarkers.GetCount(); ++index )
	{
		sReplayMarkerInfo* pMarker = m_aoMarkers[index];
		if( pMarker && pMarker->GetNonDilatedTimeMs() >= nonDilatedTimeMs )
		{
			result = index;
			break;
		}
	}

	return result;
}

// PURPOSE: Returns the index of the last marker before the given time.
// PARAMS:
//		clipTimeMs - Time in ms representing the end of the valid time range to check
// RETURNS:
//		Index of the marker, or -1 if no appropriate marker found.
s32 CClip::GetLastValidMarkerIndex( float const nonDilatedTimeMs ) const
{
	s32 index;
	for( index = m_aoMarkers.GetCount() - 1; index >= 0; --index )
	{
		sReplayMarkerInfo* pMarker = m_aoMarkers[index];
		if( pMarker && pMarker->GetNonDilatedTimeMs() <= nonDilatedTimeMs )
		{
			break;
		}
	}

	return index;
}

sReplayMarkerInfo* CClip::GetFirstValidMarker( float const nonDilatedTimeMs )
{
	s32 firstMarkerIndex = GetFirstValidMarkerIndex( nonDilatedTimeMs );
	return TryGetMarker( firstMarkerIndex );
}

sReplayMarkerInfo* CClip::GetLastValidMarker( float const nonDilatedTimeMs )
{
	s32 lastMarkerIndex = GetLastValidMarkerIndex( nonDilatedTimeMs );
	return TryGetMarker( lastMarkerIndex );
}

// PURPOSE: Returns the index of the closest marker to the given time
// PARAMS:
//		clipTimeMs - Time in Ms we are looking for markers at
//		preference - Determine how we preferentially treat what is "closest". Names of enum values are self explanatory.
//		out_timeDiff - Optional parameter to contain the actual time difference
//		markerToIgnore - Optional pointer to a marker we want to ignore. Useful for if we want to find the closest marker to an existing marker, without returning itself
//		ignoreAnchors - Optional flag on whether we should ignore anchors in our search
// RETURNS:
//		Index of the marker. -1 if none found
s32 CClip::GetClosestMarkerIndex( float const nonDilatedTimeMs, eClosestMarkerPreference const preference, float* out_nonDilatedTimeDiff, sReplayMarkerInfo const* markerToIgnore, bool const ignoreAnchors ) const
{
	s32 resultIndex = INDEX_NONE;
	float timeDiff = -1;

	for( s32 index = 0; index < m_aoMarkers.GetCount() && timeDiff != 0; ++index )
	{
		sReplayMarkerInfo* pMarker = m_aoMarkers[index];
		if( rcVerifyf( pMarker, "CClip::GetClosestMarker - NULL marker in marker info collection! Bad add logic somewhere?" ) && pMarker != markerToIgnore &&
			( !ignoreAnchors || !pMarker->IsAnchor() ) )
		{
			float currentTimeDiff =  pMarker->GetNonDilatedTimeMs() - nonDilatedTimeMs;

			if( preference == CLOSEST_MARKER_ANY )
			{
				// We only care about closest match, so use absolute values
				float const absTimeDiff = abs( timeDiff );
				float const absCurrentTimeDiff = abs( currentTimeDiff );

				if( absCurrentTimeDiff < absTimeDiff || timeDiff == -1 )
				{
					resultIndex = index;
					timeDiff = currentTimeDiff;
				}
			}
            else if( preference == CLOSEST_MARKER_LESS )
            {
                if( currentTimeDiff < 0 && ( currentTimeDiff > timeDiff || timeDiff == -1 ) )
                {
                    resultIndex = index;
                    timeDiff = currentTimeDiff;
                }
            }
			else if( preference == CLOSEST_MARKER_LESS_OR_EQUAL )
			{
				if( currentTimeDiff <= 0 && ( currentTimeDiff > timeDiff || timeDiff == -1 ) )
				{
					resultIndex = index;
					timeDiff = currentTimeDiff;
				}
			}
            else if( preference == CLOSEST_MARKER_MORE )
            {
                if( currentTimeDiff > 0 && ( currentTimeDiff < timeDiff || timeDiff == -1 ) )
                {
                    resultIndex = index;
                    timeDiff = currentTimeDiff;
                }
            }
			else if( preference == CLOSEST_MARKER_MORE_OR_EQUAL )
			{
				if( currentTimeDiff >= 0 && ( currentTimeDiff < timeDiff || timeDiff == -1 ) )
				{
					resultIndex = index;
					timeDiff = currentTimeDiff;
				}
			}
		}
	}

	if( out_nonDilatedTimeDiff )
	{
		*out_nonDilatedTimeDiff = timeDiff;
	}

	return resultIndex;
}

// PURPOSE: Returns the marker closest to the given time
// PARAMS:
//		clipTimeMs - Time in Ms we are looking for markers at
//		preference - Determine how we preferentially treat what is "closest". Names of enum values are self explanatory.
//		out_timeDiff - Optional parameter to contain the actual time difference
//		markerToIgnore - Optional pointer to a marker we want to ignore. Useful for if we want to find the closest marker to an existing marker, without returning itself
//		ignoreAnchors - Optional flag on whether we should ignore anchors in our search
// RETURNS:
//		Pointer to the marker, if one exists
sReplayMarkerInfo* CClip::GetClosestMarker( float const nonDilatedTimeMs, eClosestMarkerPreference const preference, float* out_nonDilatedTimeDiff, sReplayMarkerInfo const* markerToIgnore, bool const ignoreAnchors )
{
	s32 index = GetClosestMarkerIndex( nonDilatedTimeMs, preference, out_nonDilatedTimeDiff, markerToIgnore, ignoreAnchors );
	return TryGetMarker(index);
}

float CClip::GetAnchorNonDilatedTimeMs( u32 const index ) const
{
	float timeMs = 0;

	u32 currentIndex = 0;
	u32 rawIndex = 0;
	for( ReplayMarkerInfoCollection::const_iterator itr = m_aoMarkers.begin(); itr != m_aoMarkers.end(); ++itr )
	{
		sReplayMarkerInfo const* markerInfo = (*itr);
		if( markerInfo && markerInfo->IsAnchorOrEditableAnchor() )
		{
			if( currentIndex == index )
			{
				timeMs = markerInfo->GetNonDilatedTimeMs();
				break;
			}

			++currentIndex;
		}

		++rawIndex;
	}

	return timeMs;
}

float CClip::GetAnchorTimeMs( u32 const index ) const
{
	float const c_anchorNonDilatedTimeMs = GetAnchorNonDilatedTimeMs( index );
	float const c_anchorTimeMs = CalculateTimeForNonDilatedTimeMs( c_anchorNonDilatedTimeMs );
	return c_anchorTimeMs;
}

s32 CClip::GetAnchorCount() const
{
	s32 count = 0;

	for( ReplayMarkerInfoCollection::const_iterator itr = m_aoMarkers.begin(); itr != m_aoMarkers.end(); ++itr )
	{
		sReplayMarkerInfo const* markerInfo = (*itr);
		if( markerInfo && 
			( markerInfo->IsAnchorOrEditableAnchor() ) )
		{
			++count;
		}
	}

	return count;
}

IReplayMarkerStorage::eEditRestrictionReason CClip::IsMarkerControlEditable( eMarkerControls const controlType ) const 
{
	IReplayMarkerStorage::eEditRestrictionReason reason = IReplayMarkerStorage::EDIT_RESTRICTION_NONE;

	switch( controlType )
	{
	case MARKER_CONTROL_SPEED:
		{
			reason = CReplayCoordinator::DoesCurrentClipContainCutscene() ? IReplayMarkerStorage::EDIT_RESTRICTION_CUTSCENE : reason;
			break;
		}
	case MARKER_CONTROL_CAMERA:
	case MARKER_CONTROL_CAMERA_TYPE:
		{
			reason = CReplayCoordinator::DoesCurrentClipContainCutscene() ? IReplayMarkerStorage::EDIT_RESTRICTION_CUTSCENE :
					CReplayCoordinator::DoesCurrentClipContainFirstPersonCamera() ? IReplayMarkerStorage::EDIT_RESTRICTION_FIRST_PERSON :
					CReplayCoordinator::DoesCurrentClipDisableCameraMovement() ? IReplayMarkerStorage::EDIT_RESTRICTION_CAMERA_BLOCKED : reason;
			
			break;
		}
	default:
		{
			// NOP
			break;
		}
	}

	return reason;
}

void CClip::BlendTypeUpdatedOnMarker( u32 const index )
{
	s32 const c_nextMarkerIndex = (s32)index + 1;
	sReplayMarkerInfo const* currentMarker = TryGetMarker( index );
	sReplayMarkerInfo* nextMarker = TryGetMarker( c_nextMarkerIndex );

	// B*2246755 - When setting a blend on the second last marker, for the last marker to be a free camera
	if( currentMarker && nextMarker && c_nextMarkerIndex == GetMarkerCount() - 1 )
	{
		bool const c_endMarkerFreeCamera = currentMarker->IsFreeCamera() && currentMarker->HasBlend();
		ReplayCameraType const c_newCameraType = c_endMarkerFreeCamera ? RPLYCAM_FREE_CAMERA : RPLYCAM_INERHITED;
		nextMarker->SetCameraType( c_newCameraType );

		nextMarker->SetAttachTargetId(currentMarker->GetAttachTargetId());
		nextMarker->SetAttachTargetIndex(currentMarker->GetAttachTargetIndex());
		nextMarker->SetLookAtTargetId(currentMarker->GetLookAtTargetId());
		nextMarker->SetLookAtTargetIndex(currentMarker->GetLookAtTargetIndex());
	}
}

// End Markers
//////////////////////////////////////////////////////////////////////////

ReplayMarkerTransitionType CClip::GetStartTransition() const
{	
	sReplayMarkerInfo const* firstMarker = GetFirstMarker();
	ReplayMarkerTransitionType const c_transType = firstMarker ? (ReplayMarkerTransitionType)firstMarker->GetTransitionType() : MARKER_TRANSITIONTYPE_NONE;

	return c_transType;
}

void CClip::SetStartTransition( ReplayMarkerTransitionType const eType)
{
	sReplayMarkerInfo* firstMarker = GetFirstMarker();
	if( firstMarker )
	{
		firstMarker->SetTransitionType( (u8)eType );
	}
}

float CClip::GetStartTransitionDurationMs() const
{
	sReplayMarkerInfo const* firstMarker = GetFirstMarker();
	float const c_transDuration = firstMarker ? firstMarker->GetTransitionDurationMs() : GetMinTransitionDurationMs();
	return c_transDuration;
}

void CClip::SetStartTransitionDurationMs( float const durationMs )
{
	sReplayMarkerInfo* firstMarker = GetFirstMarker();
	if( firstMarker )
	{
		firstMarker->SetTransitionDurationMs( Clamp<float>( (float)durationMs, GetMinTransitionDurationMs(), GetMaxTransitionDurationMs() ) );
	}
}

ReplayMarkerTransitionType CClip::GetEndTransition() const
{
	sReplayMarkerInfo const* lastMarker = GetLastMarker();
	ReplayMarkerTransitionType const c_transType = lastMarker ? (ReplayMarkerTransitionType)lastMarker->GetTransitionType() : MARKER_TRANSITIONTYPE_NONE;

	return c_transType;
}

void CClip::SetEndTransition( ReplayMarkerTransitionType const eType)
{
	sReplayMarkerInfo* lastMarker = GetLastMarker();
	if( lastMarker )
	{
		lastMarker->SetTransitionType( (u8)eType );
	}
}

float CClip::GetEndTransitionDurationMs() const
{
	sReplayMarkerInfo const* lastMarker = GetLastMarker();
	float const c_transDuration = lastMarker ? lastMarker->GetTransitionDurationMs() : GetMinTransitionDurationMs();
	return c_transDuration;
}

void CClip::SetEndTransitionDurationMs( float const durationMs )
{
	sReplayMarkerInfo* lastMarker = GetLastMarker();
	if( lastMarker )
	{
		lastMarker->SetTransitionDurationMs( Clamp( durationMs, GetMinTransitionDurationMs(), GetMaxTransitionDurationMs() ) );
	}
}

float CClip::GetStartNonDilatedTimeMs() const
{
	sReplayMarkerInfo const* firstMarker = GetFirstMarker();
	float const c_startTime = firstMarker ? firstMarker->GetNonDilatedTimeMs() : 0;
	return c_startTime;
}

float CClip::GetEndNonDilatedTimeMs() const
{
	sReplayMarkerInfo const* lastMarker = GetLastMarker();
	float const c_endTime = lastMarker ? lastMarker->GetNonDilatedTimeMs() : m_rawDurationMs;
	return c_endTime;
}

float CClip::ConvertNonDilatedTimeMsToTimeMs( float const nonDilatedTimeMs ) const
{
	float const c_realTimeMs = CalculateTimeForNonDilatedTimeMs( nonDilatedTimeMs );
	return c_realTimeMs;
}

float CClip::ConvertTimeMsToNonDilatedTimeMs( float const timeMs ) const
{
	float const c_clipTimeMs = CalculateNonDilatedTimeForTimeMs( timeMs );
	return c_clipTimeMs;
}

float CClip::ConvertPlaybackPercentageToNonDilatedTimeMs( float const playbackPercent ) const
{
	float const c_clipTimeMs = CalculateNonDilatedTimeForTrimmedPercentMs( playbackPercent );
	return c_clipTimeMs;
}

float CClip::ConvertClipPercentageToNonDilatedTimeMs( float const clipPercent ) const
{
	float const c_clipTimeMs = CalculateNonDilatedTimeForTotalPercentMs( clipPercent );
	return c_clipTimeMs;
}

float CClip::ConvertClipNonDilatedTimeMsToClipPercentage( float const nonDilatedTimeMs ) const
{
	float const c_percentage = CalculateTotalPercentForNonDilatedTimeForMs( nonDilatedTimeMs );
	return c_percentage;
}

float CClip::GetRawEndTimeAsTimeMs() const
{
	float const c_realTimeMs = CalculateTimeForNonDilatedTimeMs( GetRawDurationMs() ) + GetStartNonDilatedTimeMs();
	return c_realTimeMs;
}

size_t CClip::ApproximateMinimumSizeForSaving()
{
	size_t sizeNeeded = 0;

	CClip dummyClip;

	// estimate files size to see if we have enough space before writing
	sizeNeeded = sizeof(ClipHeader_Current);
	sizeNeeded += sizeof(sReplayMarkerInfo) * 2;

	return sizeNeeded;
}

size_t CClip::ApproximateMaximumSizeForSaving()
{
	size_t sizeNeeded = 0;

	CClip dummyClip;

	// estimate files size to see if we have enough space before writing
	sizeNeeded = sizeof(ClipHeader_Current);
	sizeNeeded += sizeof(sReplayMarkerInfo) * MAX_MARKERS;
	sizeNeeded += GetExpectedThumbnailSize();

	return sizeNeeded;
}

size_t CClip::ApproximateSizeForSaving() const
{
	size_t sizeNeeded = 0;

	ClipHeader_Current header;

	// estimate files size to see if we have enough space before writing
	sizeNeeded = sizeof(header);

	for (ReplayMarkerInfoCollection::const_iterator itr = m_aoMarkers.begin(); itr != m_aoMarkers.end(); ++itr )
	{
		sizeNeeded += sizeof(sReplayMarkerInfo);
	}

	if( HasBespokeThumbnail() )
	{
		sizeNeeded += m_thumbnailBufferSize;
	}

	return sizeNeeded;
}

u64 CClip::Save(FileHandle hFile)
{
	rcAssertf( hFile, "CClip::Save: Invalid handle");

	bool const c_timeWasValid = rcVerifyf( !ValidateTimeData(), "CClip::Save - Bad time data corrected in clip %s", m_szName.getBuffer() );

	u64 resultFlags = c_timeWasValid ? CReplayEditorParameters::PROJECT_SAVE_RESULT_SUCCESS : CReplayEditorParameters::PROJECT_SAVE_RESULT_BAD_TIME_DATA;

	ClipHeader_Current header;

	header.m_clipUID = m_clipUID;
	header.m_numberOfMarkers = (u32)(m_aoMarkers.size());
	header.m_thumbnailBufferSize = m_thumbnailBufferSize;
	header.m_name = m_szName;

	header.m_rawDurationMs = (u32)m_rawDurationMs;
	header.m_ownerId = m_ownerId;

	s32 const c_headerSizeExpected = sizeof(header);
	s32 const c_headerSizeWritten = CFileMgr::Write( hFile, (char*)&header, c_headerSizeExpected );
	resultFlags |= c_headerSizeExpected == c_headerSizeWritten ? 0 : CReplayEditorParameters::PROJECT_SAVE_RESULT_WRITE_FAIL;

	RemoveMarkersOutsideOfRangeNonDilatedTimeMS( GetStartNonDilatedTimeMs(), GetEndNonDilatedTimeMs() );

	u64 const c_optOutFlags = CReplayEditorParameters::PROJECT_SAVE_RESULT_UNSAFE_TO_CONTINUE;
	if( ( resultFlags & c_optOutFlags ) == 0  )
	{
		if( m_aoMarkers.size() < 2 )
		{
			resultFlags |= CReplayEditorParameters::PROJECT_SAVE_RESULT_NOT_ENOUGH_MARKERS;
			
			CleanupMarkers();
			AddMarkersForStartAndEndPoints();
		}

		s32 const c_markerSizeExpected = sizeof(sReplayMarkerInfo);
		s32 sizeWritten = 0;

		// Write out each marker.
		for (ReplayMarkerInfoCollection::iterator itr = m_aoMarkers.begin(); ( resultFlags & c_optOutFlags ) == 0 && itr != m_aoMarkers.end(); ++itr )
		{
			sReplayMarkerInfo* currentMarker = *itr;
			if( currentMarker )
			{
				sizeWritten = CFileMgr::Write(hFile,(char*)currentMarker, c_markerSizeExpected );
				resultFlags |= c_markerSizeExpected == sizeWritten ? 0 : CReplayEditorParameters::PROJECT_SAVE_RESULT_WRITE_FAIL;
			}
			else
			{
				resultFlags |= CReplayEditorParameters::PROJECT_SAVE_RESULT_INVALID_MARKERS;
			}
		}

		// Tack on the thumbnail, if it exists
		if( HasBespokeThumbnail() &&  ( resultFlags & c_optOutFlags ) == 0 )
		{
			sizeWritten = CFileMgr::Write(hFile, (char*)m_thumbnailBuffer, m_thumbnailBufferSize );
			resultFlags |= m_thumbnailBufferSize == sizeWritten ? 0 : CReplayEditorParameters::PROJECT_SAVE_RESULT_WRITE_FAIL;
		}
	}

	return resultFlags;
}

// PURPOSE: Populate this clip with the contents of the given file.
// RETURNS:
//		u64 containing CReplayEditorParameters::eCLIP_LOAD_RESULT_FLAGS representing load result
u64 CClip::Load( FileHandle hFile, u32 const serializedVersion, size_t const serializedSize,
				 u32 const markerSerializedVersion, size_t const markerSerializedSize, bool const ignoreValidity )
{
	(void)serializedSize;
	rcAssertf( hFile, "CClip::Load - Invalid handle");

	u64 result = serializedVersion > CLIP_VERSION ? CReplayEditorParameters::CLIP_LOAD_RESULT_BAD_VERSION : CReplayEditorParameters::CLIP_LOAD_RESULT_SUCCESS;
	result |= markerSerializedVersion > sReplayMarkerInfo::REPLAY_MARKER_VERSION ? CReplayEditorParameters::CLIP_LOAD_RESULT_BAD_MARKER_VERSION : result;

	if( rcVerifyf( ( result & CReplayEditorParameters::CLIP_LOAD_RESULT_BAD_VERSION ) == 0, 
		"CClip::Load - Clip Version %d greater than current version %d. Have you been abusing time-travel?", serializedVersion, CLIP_VERSION ) && 
		rcVerifyf( ( result & CReplayEditorParameters::CLIP_LOAD_RESULT_BAD_MARKER_VERSION ) == 0, 
		"CClip::Load - Marker Version %d greater than current version %d. Have you been abusing time-travel?", 
		markerSerializedVersion, sReplayMarkerInfo::REPLAY_MARKER_VERSION ) )
	{
		result = CReplayEditorParameters::CLIP_LOAD_RESULT_SUCCESS;

		bool headerLoadSuccessful = false;
		ClipHeader_Current header;

		switch( serializedVersion )
		{
		case CLIP_VERSION:
			{
				headerLoadSuccessful = 
					defaulHeaderLoadFunction<ClipHeader_Current>( header, hFile, result );

				result |= (ignoreValidity || header.IsValid()) ? result :  CReplayEditorParameters::CLIP_LOAD_RESULT_HEADER_INVALID;
				break;
			}

		case 3:
		case 2:
			{
				headerLoadSuccessful = 
					defaulHeaderLoadFunction<ClipHeader_V2>( header, hFile, result );

				result |= (ignoreValidity || header.IsValid()) ? result :  CReplayEditorParameters::CLIP_LOAD_RESULT_HEADER_INVALID;
				break;
			}

		case 1:
			{
				headerLoadSuccessful = 
					defaulHeaderLoadFunction<ClipHeader_V1>( header, hFile, result );

				result |= (ignoreValidity || header.IsValid()) ? result : CReplayEditorParameters::CLIP_LOAD_RESULT_HEADER_INVALID;
				break;
			}

		default:
			{
				rcWarningf( "CClip::Load - Encountered unsupported clip version %d", serializedVersion );
				result |= CReplayEditorParameters::CLIP_LOAD_RESULT_BAD_VERSION;
				headerLoadSuccessful = false;
				break;
			}
		}

		// Even if the header was invalid we attempt to continue the load to ensure we read over all the relevant data and
		// leave the stream in the correct place to read the next clip
		if( headerLoadSuccessful )
		{
			m_rawDurationMs = (float)header.m_rawDurationMs;
			m_szName = header.m_name;
			m_clipUID = header.m_clipUID;
			m_ownerId = header.m_ownerId;

			//! Store the offset before markers, in-case anything goes wrong
			s32 const c_offsetBeforeMarkers = CFileMgr::Tell( hFile );
			bool markersWereCorrupted = false;

			switch( markerSerializedVersion )
			{
            case sReplayMarkerInfo::REPLAY_MARKER_VERSION:
                {
                    markersWereCorrupted = 
                        defaultMarkerLoadFunction<sReplayMarkerInfo::sMarkerData_Current>
                        ( *this, hFile, header.m_numberOfMarkers, markerSerializedSize, result );

                    break;
                }

            case 11:
                {
                    markersWereCorrupted = 
                        defaultMarkerLoadFunction<sReplayMarkerInfo::sMarkerData_V11>
                        ( *this, hFile, header.m_numberOfMarkers, markerSerializedSize, result );

                    break;
                }

			case 10:
				{
					markersWereCorrupted = 
						defaultMarkerLoadFunction<sReplayMarkerInfo::sMarkerData_V10>
						( *this, hFile, header.m_numberOfMarkers, markerSerializedSize, result );

					break;
				}

			case 9:
				{
					markersWereCorrupted = 
						defaultMarkerLoadFunction<sReplayMarkerInfo::sMarkerData_V9>
						( *this, hFile, header.m_numberOfMarkers, markerSerializedSize, result );

					break;
				}

			case 8:
				{
					markersWereCorrupted = 
						defaultMarkerLoadFunction<sReplayMarkerInfo::sMarkerData_V8>
						( *this, hFile, header.m_numberOfMarkers, markerSerializedSize, result );

					break;
				}

			case 7:
				{
					markersWereCorrupted = 
						defaultMarkerLoadFunction<sReplayMarkerInfo::sMarkerData_V7>
						( *this, hFile, header.m_numberOfMarkers, markerSerializedSize, result );

					break;
				}

			case 6:
				{
					markersWereCorrupted = 
						defaultMarkerLoadFunction<sReplayMarkerInfo::sMarkerData_V6>
						( *this, hFile, header.m_numberOfMarkers, markerSerializedSize, result );

					break;
				}

			case 5:
				{
					markersWereCorrupted = 
						defaultMarkerLoadFunction<sReplayMarkerInfo::sMarkerData_V5>
						( *this, hFile, header.m_numberOfMarkers, markerSerializedSize, result );

					break;
				}

			case 4:
				{
					markersWereCorrupted = 
						defaultMarkerLoadFunction<sReplayMarkerInfo::sMarkerData_V4>
						( *this, hFile, header.m_numberOfMarkers, markerSerializedSize, result );

					break;
				}

			case 3:
				{
					markersWereCorrupted = 
						defaultMarkerLoadFunction<sReplayMarkerInfo::sMarkerData_V3>
						( *this, hFile, header.m_numberOfMarkers, markerSerializedSize, result );

					break;
				}

			case 2:
				{
					markersWereCorrupted = 
						defaultMarkerLoadFunction<sReplayMarkerInfo::sMarkerData_V2>
						( *this, hFile, header.m_numberOfMarkers, markerSerializedSize, result );

					break;
				}

			case 1:
				{
					markersWereCorrupted = 
						defaultMarkerLoadFunction<sReplayMarkerInfo::sMarkerData_V1>
						( *this, hFile, header.m_numberOfMarkers, markerSerializedSize, result );

					break;
				}

			default:
				{
					rcWarningf( "CClip::Load - Encountered unsupported marker version %d, discarding markers", markerSerializedVersion );
					markersWereCorrupted = true;
					result |= CReplayEditorParameters::CLIP_LOAD_RESULT_UNSPPORTED_MARKER_VERSION;
				}
			}

			//! We need at least two markers, so catch the case here
			markersWereCorrupted = markersWereCorrupted || GetMarkerCount() < 2;

			if( ( result & CReplayEditorParameters::CLIP_LOAD_RESULT_UNSAFE_TO_CONTINUE ) == 0 )
			{
				if( markersWereCorrupted )
				{
					// Clean-up any partial marker creation
					CleanupMarkers();

					s32 const c_offset = c_offsetBeforeMarkers + s32( markerSerializedSize * header.m_numberOfMarkers );
					s32 const c_newPos = CFileMgr::Seek( hFile, c_offset );

					result |= c_offset != c_newPos ? CReplayEditorParameters::CLIP_LOAD_RESULT_EARLY_EOF : result;

					// Insert in some default trimming markers, since we don't support whatever marker version this was
					AddMarkersForStartAndEndPoints();
				}

				PostLoadMarkerValidation();

				if( header.m_thumbnailBufferSize > 0 )
				{
					CleanupBespokeThumbnail();

					// BC hack - allow loading older (larger) thumbnails clamped to safe range
					s32 thumbnailBytesToSkip = 0;
					if( serializedVersion == 2 )
					{
						s32 const c_upperLimit = (s32)GetExpectedThumbnailSize();
						thumbnailBytesToSkip = header.m_thumbnailBufferSize - c_upperLimit;
						header.m_thumbnailBufferSize = GetExpectedThumbnailSize();
					}

					if( IsValidThumbnailBufferSize( header.m_thumbnailBufferSize ) )
					{
						m_thumbnailBufferSize = header.m_thumbnailBufferSize;
						m_thumbnailBuffer = rage_new u8[ m_thumbnailBufferSize ];
						result |= m_thumbnailBuffer == NULL ? CReplayEditorParameters::CLIP_LOAD_RESULT_OOM : result;

						if( m_thumbnailBuffer )
						{
							result |= CFileMgr::Read( hFile, (char*)m_thumbnailBuffer, m_thumbnailBufferSize ) == m_thumbnailBufferSize 
								? result : CReplayEditorParameters::CLIP_LOAD_RESULT_EARLY_EOF;

							// We want to skip any additional un-used jpeg bytes that should be invalid
							if( thumbnailBytesToSkip > 0 )
							{
								CFileMgr::Seek( hFile, CFileMgr::Tell( hFile ) + thumbnailBytesToSkip );
							}

							MakeBespokeThumbnailBufferFileName();
						}
						else
						{
							CleanupBespokeThumbnail();
						}
					}
					else
					{
						result |= CReplayEditorParameters::CLIP_LOAD_RESULT_BAD_THUMBNAIL_SIZE;
					}
				}
			}

			// Validate time last, as everything should exist now
			result |= ValidateTimeData() ? CReplayEditorParameters::CLIP_LOAD_RESULT_BAD_TIME_DATA : result;
		}
	}

	return result;
}

void CClip::Clear()
{
	m_szName.Clear();
	m_rawDurationMs = 0;

	CleanupMarkers();
	CleanupBespokeThumbnail();
	CleanupBespokeThumbnailCapture();
}

bool CClip::CaptureBespokeThumbnail()
{
	bool success = false;

	CleanupBespokeThumbnail();

	u32 const c_width = GetThumbnailWidth();
	u32 const c_height = GetThumbnailHeight();

	if( m_asyncThumbnailBuffer.Initialize( c_width, c_height ) )
	{
		success = CPhotoManager::RequestSaveScreenshotToBuffer( 
			m_asyncThumbnailBuffer, c_width, c_height );
	}

	return success;
}

void CClip::UpdateBespokeCapture()
{
	if( m_asyncThumbnailBuffer.IsInitialized() )
	{
		if( m_asyncThumbnailBuffer.IsPopulated() )
		{
			AllocateBespokeThumbnail();

			int const c_sizeWritten = grcImage::SaveRGBA8SurfaceToJPEG( GetBespokeThumbnailFileName(), 75, 
				m_asyncThumbnailBuffer.GetRawBuffer(), m_asyncThumbnailBuffer.GetWidth(), m_asyncThumbnailBuffer.GetHeight(), 
				m_asyncThumbnailBuffer.GetPitch() );

			// Now we know the exact size, let's downsize the space we need
			if( c_sizeWritten > 0 && c_sizeWritten < m_thumbnailBufferSize )
			{
				u8* minJpgBuffer = rage_new u8[c_sizeWritten];
				if( minJpgBuffer )
				{
					sysMemCpy( minJpgBuffer, m_thumbnailBuffer, c_sizeWritten);
					delete m_thumbnailBuffer;
					m_thumbnailBuffer = minJpgBuffer;
					m_thumbnailBufferSize = c_sizeWritten;

					// Rebuild filename since the pointer changed
					MakeBespokeThumbnailBufferFileName();
				}
			}

			CleanupBespokeThumbnailCapture();
		}
	}
}

void CClip::ValidateTransitionTimes()
{
	// Feed these back through so they are correctly clamped
	SetStartTransitionDurationMs( GetMaxTransitionDurationMs() );
	SetEndTransitionDurationMs( GetMaxTransitionDurationMs() );
}

void CClip::SetStartNonDilatedTimeMs( float const nonDilatedTimeMs )
{
	sReplayMarkerInfo* marker = GetFirstMarker();
	if( marker )
	{
		marker->SetNonDilatedTimeMs( nonDilatedTimeMs );
		RecalculateDilatedTimeStartEndPoints();
	}
}

void CClip::SetEndNonDilatedTimeMs( float const nonDilatedTimeMs )
{
	sReplayMarkerInfo* marker = GetLastMarker();
	if( marker )
	{
		marker->SetNonDilatedTimeMs( nonDilatedTimeMs );
		RecalculateDilatedTimeStartEndPoints();
	}
}

void CClip::AddMarkersForStartAndEndPoints()
{
	if( replayVerifyf( GetMarkerCount() == 0, "CClip::AddMarkersForStartAndEndPoints - Should only be called for new clips, not loaded clips" ) )
	{
		s32 index = -1;
		AddMarker( false, false, 0.f, index );
		sReplayMarkerInfo* marker = TryGetMarker( index );
		replayAssertf( marker, "CClip::AddMarkersForStartAndEndPoints - Can't add marker for start " );

		index = -1;
		AddMarker( true, false, MARKER_FRAME_BOUNDARY_MS * 2, index );
		marker = TryGetMarker( index );
		replayAssertf( marker, "CClip::AddMarkersForStartAndEndPoints - Can't add marker for end " );
	}
}

bool CClip::ValidateTimeData()
{
	bool timeDataChanged = false;

	float startTime = GetStartNonDilatedTimeMs();
	if( !replayVerifyf( startTime >= 0.f, "Clip start time (%f) must be >= 0!", startTime ) )
	{
		SetStartNonDilatedTimeMs( 0 );
		timeDataChanged = true;
	}

	float endTime = GetEndNonDilatedTimeMs();
	if( !replayVerifyf( endTime <= m_rawDurationMs, "Clip end time (%f) must be <= its length (%f)!", endTime, m_rawDurationMs ) )
	{
		SetEndNonDilatedTimeMs( m_rawDurationMs );
		timeDataChanged = true;
	}

	startTime = GetStartNonDilatedTimeMs();
	endTime = GetEndNonDilatedTimeMs();

	if( !replayVerifyf( startTime < endTime, "Clip start time (%f) must be < its end time (%f)!", startTime, endTime ) )
	{
		SetStartNonDilatedTimeMs( 0 );
		SetEndNonDilatedTimeMs( m_rawDurationMs );
	}

	RemoveMarkersOutsideOfRangeNonDilatedTimeMS( GetStartNonDilatedTimeMs(), GetEndNonDilatedTimeMs() );
	RecalculateDilatedTimeStartEndPoints();
	ValidateTransitionTimes();

	return timeDataChanged;
}

void CClip::ValidateThumbnailBufferSize()
{
	if( HasBespokeThumbnail() )
	{
		if( !IsValidThumbnailBufferSize( m_asyncThumbnailBuffer.GetBufferSize() ) )
		{
			CleanupBespokeThumbnailCapture();
		}
	}
}

bool CClip::IsValidThumbnailBufferSize( size_t const bufferSize )
{
	size_t const c_expectedBufferSize = GetExpectedThumbnailSize();
	return bufferSize > 0 && bufferSize <= c_expectedBufferSize;
}

void CClip::CleanupMarkers()
{
	for( int i = 0; i < m_aoMarkers.GetCount(); ++i )
	{
		delete m_aoMarkers[i];
		m_aoMarkers[i] = NULL;
	}

	m_aoMarkers.clear();
}

void CClip::CopyMarkerData( CClip const& other )
{
	CleanupMarkers();

	for( int index = 0; index < other.m_aoMarkers.GetCount(); ++index ) 
	{
		sReplayMarkerInfo const* sourceMarker = other.m_aoMarkers[ index ];
		if( rcVerifyf( sourceMarker, "CClip::operator= - Invalid marker source! Skipping." ) )
		{
			sReplayMarkerInfo* pMarkerCopy = rage_new sReplayMarkerInfo( *sourceMarker );
			if( rcVerifyf( pMarkerCopy, "CClip::operator= - Unable to allocate sReplayMarkerInfo!" ) )
			{
				m_aoMarkers.PushAndGrow(pMarkerCopy);
			}
		}		
	}
}

void CClip::AllocateBespokeThumbnail()
{
	CleanupBespokeThumbnail();

	m_thumbnailBufferSize = GetExpectedThumbnailSize();
	m_thumbnailBuffer = rage_new u8[ m_thumbnailBufferSize ];
	m_thumbnailBufferSize = m_thumbnailBuffer ? m_thumbnailBufferSize : 0;

	MakeBespokeThumbnailBufferFileName();
}

u32 CClip::GetExpectedThumbnailSize()
{
	return 80 * 1024;
}

void CClip::CleanupBespokeThumbnail()
{
	if( m_thumbnailBuffer )
	{
		delete[] m_thumbnailBuffer;
		m_thumbnailBuffer = NULL;
	}

	m_thumbnailBufferSize = 0;
	m_thumbnailBufferMemFileName.Clear();
}

void CClip::CleanupBespokeThumbnailCapture()
{
	if( m_asyncThumbnailBuffer.IsInitialized() )
	{
		m_asyncThumbnailBuffer.Shutdown();
	}
}

void CClip::CopyBespokeThumbnail( CClip const& other )
{
	CleanupBespokeThumbnail();

	if( other.HasBespokeThumbnail() )
	{
		m_thumbnailBufferSize = other.GetBespokeThumbnailBufferSize();
		m_thumbnailBuffer = rage_new u8[ m_thumbnailBufferSize ];
		m_thumbnailBufferSize = m_thumbnailBuffer ? m_thumbnailBufferSize : 0;

		if( m_thumbnailBuffer )
		{
			sysMemCpy( m_thumbnailBuffer, other.GetBespokeThumbnailBuffer(), m_thumbnailBufferSize );
			MakeBespokeThumbnailBufferFileName();
		}
	}
}

void CClip::MakeBespokeThumbnailBufferFileName()
{
	m_thumbnailBufferMemFileName.Clear();

	if( m_thumbnailBuffer )
	{
		fiDevice::MakeMemoryFileName(
			m_thumbnailBufferMemFileName.getWritableBuffer(), (int)m_thumbnailBufferMemFileName.GetMaxByteCount(), 
			GetBespokeThumbnailBuffer(), GetBespokeThumbnailBufferSize(), false, "" );
	}
}

float CClip::CalculateTimeForNonDilatedTimeMs( float const nonDilatedTimeMs ) const
{
	float resultTimeMs = 0.f;
	float currentNonDilatedTimeMs = GetStartNonDilatedTimeMs();

	int const c_markerCount = m_aoMarkers.GetCount();

	for( int index = 0; index != c_markerCount  && currentNonDilatedTimeMs < nonDilatedTimeMs; ++index )
	{
		sReplayMarkerInfo const* currentMarker = m_aoMarkers[index];
		if( currentMarker && !currentMarker->IsAnchor() )
		{
			s32 const c_markerIndex = GetNextMarkerIndex( index, true );
			sReplayMarkerInfo const* nextMarker = c_markerIndex >= 0 ? m_aoMarkers[ c_markerIndex ] : NULL;
			if( !nextMarker || ( nextMarker && !nextMarker->IsAnchor() ) )
			{
				float const c_nextNonDilatedTimeMs = nextMarker ? nextMarker->GetNonDilatedTimeMs() : GetRawDurationMs();
				float const c_speedValue = currentMarker->GetSpeedValueFloat();

				float const c_nonDilatedTimeDelta = c_nextNonDilatedTimeMs - currentMarker->GetNonDilatedTimeMs();
				float const c_timeDelta = c_nonDilatedTimeDelta / c_speedValue;

				if( c_nextNonDilatedTimeMs < nonDilatedTimeMs )
				{
					resultTimeMs += c_timeDelta;
				}
				else
				{
					float const c_diffNonDilatedTimeMs = nonDilatedTimeMs - currentNonDilatedTimeMs;
					float const c_diffClipTimeMs = c_diffNonDilatedTimeMs / currentMarker->GetSpeedValueFloat();

					resultTimeMs += c_diffClipTimeMs;
					break;
				}

				currentNonDilatedTimeMs = c_nextNonDilatedTimeMs;
			}
		}
	}

	return resultTimeMs;
}

float CClip::CalculateTimeForMarkerMs( u32 const markerIndex ) const
{
	sReplayMarkerInfo const* marker = TryGetMarker( markerIndex );

	float const c_realTimeMs = marker ? CalculateTimeForNonDilatedTimeMs( marker->GetNonDilatedTimeMs() ) : 0;
	return c_realTimeMs;
}

float CClip::CalculateNonDilatedTimeForTimeMs( float const timeMs ) const
{
	float resultNonDilatedTimeMs = GetStartNonDilatedTimeMs();
	float currentTimeMs = 0;

	int const c_markerCount = m_aoMarkers.GetCount();
	for( int index = 0; index != c_markerCount - 1 && currentTimeMs < timeMs; ++index )
	{
		sReplayMarkerInfo const* currentMarker = m_aoMarkers[index];
		if( currentMarker && !currentMarker->IsAnchor() )
		{
			s32 const c_markerIndex = GetNextMarkerIndex( index, true );
			sReplayMarkerInfo const* nextMarker = c_markerIndex >= 0 ? m_aoMarkers[ c_markerIndex ] : NULL;
			if( nextMarker && !nextMarker->IsAnchor() )
			{
				float const c_nextNonDilatedTimeMs = nextMarker->GetNonDilatedTimeMs();

				float const c_nonDilatedTimeDelta = c_nextNonDilatedTimeMs - currentMarker->GetNonDilatedTimeMs();
				float const c_timeDelta = c_nonDilatedTimeDelta / currentMarker->GetSpeedValueFloat();

				float const c_nextTimeMs = currentTimeMs + c_timeDelta;

				if( c_nextTimeMs < timeMs )
				{
					resultNonDilatedTimeMs = c_nextNonDilatedTimeMs;
				}
				else
				{
					float const c_diffTimeMs = timeMs - currentTimeMs;
					float const c_diffClipNonDilatedTimeMs = c_diffTimeMs * currentMarker->GetSpeedValueFloat();

					resultNonDilatedTimeMs += c_diffClipNonDilatedTimeMs;
					break;
				}

				currentTimeMs = c_nextTimeMs;
			}
		}
	}

	return resultNonDilatedTimeMs;
}

float CClip::CalculateNonDilatedTimeForTrimmedPercentMs( float const playbackPercent ) const
{
	float timeMs = GetStartNonDilatedTimeMs();

	float const c_totalRealTimeMs = GetTrimmedTimeMs();

	int const c_markerCount = m_aoMarkers.GetCount();
	float currentPercent = 0.f;

	for( int index = 0; index < c_markerCount - 1 && currentPercent < playbackPercent; ++index )
	{
		sReplayMarkerInfo const* currentMarker = m_aoMarkers[index];
		if( currentMarker && !currentMarker->IsAnchor() )
		{
			s32 const c_markerIndex = GetNextMarkerIndex( index, true );
			sReplayMarkerInfo const* nextMarker = c_markerIndex >= 0 ? m_aoMarkers[ c_markerIndex ] : NULL;
			if( nextMarker && !nextMarker->IsAnchor() )
			{
				float const c_nextTime = nextMarker->GetNonDilatedTimeMs();

				float const c_timeDeltaMs = c_nextTime - currentMarker->GetNonDilatedTimeMs();
				float const c_realTimeDeltaMs =  c_timeDeltaMs / currentMarker->GetSpeedValueFloat();

				float const c_percentDelta = c_realTimeDeltaMs / c_totalRealTimeMs;
				float const c_nextPercent = currentPercent + c_percentDelta;

				if( c_nextPercent < playbackPercent )
				{
					timeMs = c_nextTime;
				}
				else
				{
					float const c_percentLeftToCalculate = c_percentDelta - ( c_nextPercent - playbackPercent );
					float const c_percentLeftToCalculateLocal = c_percentLeftToCalculate / c_percentDelta;
					float const c_timeLeftToAddMs = c_timeDeltaMs * c_percentLeftToCalculateLocal;

					timeMs += c_timeLeftToAddMs;
				}

				currentPercent = c_nextPercent;
			}
		}
	}

	return timeMs;
}

//! Clip percent is 0-1 in Dilated Time, as different bar sections have different speeds
float CClip::CalculateNonDilatedTimeForTotalPercentMs( float const clipPercent ) const
{
	float nonDilatedtimeMs = 0;
	float const c_nonDilatedDurationMs = GetRawDurationMs();
	float const c_dilatedDurationMs = GetRawEndTimeAsTimeMs();

	int const c_markerCount = m_aoMarkers.GetCount();
	float currentPercent = 0.f;

	for( int index = -1; index < c_markerCount && currentPercent < clipPercent; ++index )
	{
		sReplayMarkerInfo const* currentMarker = index >= 0 ? m_aoMarkers[index] : NULL;
		if( !currentMarker || ( currentMarker && !currentMarker->IsAnchor() ) )
		{
			s32 const c_markerIndex = GetNextMarkerIndex( index, true );
			sReplayMarkerInfo const* nextMarker = c_markerIndex >= 0 ? m_aoMarkers[ c_markerIndex ] : NULL;
			if( !nextMarker || ( nextMarker && !nextMarker->IsAnchor() ) )
			{
				float const c_currentTime = currentMarker ? currentMarker->GetNonDilatedTimeMs() : 0;
				float const c_nextTime = nextMarker ? nextMarker->GetNonDilatedTimeMs() : c_nonDilatedDurationMs;

				float const c_timeDeltaMs = c_nextTime - c_currentTime;
				float const c_speedValue = currentMarker ? currentMarker->GetSpeedValueFloat() : 1.f;
				float const c_realTimeDeltaMs =  c_timeDeltaMs / c_speedValue;

				float const c_percentDelta = c_realTimeDeltaMs / c_dilatedDurationMs;
				float const c_nextPercent = currentPercent + c_percentDelta;

				if( c_nextPercent < clipPercent )
				{
					nonDilatedtimeMs = c_nextTime;
				}
				else
				{
					float const c_percentLeftToCalculate = c_percentDelta - ( c_nextPercent - clipPercent );
					float const c_percentLeftToCalculateLocal = c_percentLeftToCalculate / c_percentDelta;
					float const c_timeLeftToAddMs = c_timeDeltaMs * c_percentLeftToCalculateLocal;

					nonDilatedtimeMs += c_timeLeftToAddMs;
				}

				currentPercent = c_nextPercent;
			}
		}
	}

	return nonDilatedtimeMs;
}

float CClip::CalculateTotalPercentForNonDilatedTimeForMs( float const nonDilatedTimeMs ) const
{
	float resultPercent = 0.f;

	float const c_dilatedDurationMs = GetRawEndTimeAsTimeMs();
	int const c_markerCount = m_aoMarkers.GetCount();

	for( int index = -1; index < c_markerCount; ++index )
	{
		sReplayMarkerInfo const* currentMarker = index >= 0 ? m_aoMarkers[index] : NULL;
		if( !currentMarker || ( currentMarker && !currentMarker->IsAnchor() ) )
		{
			s32 const c_markerIndex = GetNextMarkerIndex( index, true );
			sReplayMarkerInfo const* nextMarker = c_markerIndex >= 0 ? m_aoMarkers[ c_markerIndex ] : NULL;
			if( !nextMarker || ( nextMarker && !nextMarker->IsAnchor() ) )
			{
				float const c_currentTime = currentMarker ? currentMarker->GetNonDilatedTimeMs() : 0;
				float const c_nextMarkerTime = nextMarker ? nextMarker->GetNonDilatedTimeMs() : GetRawDurationMs();
				float const c_nextTime = nonDilatedTimeMs < c_nextMarkerTime ? nonDilatedTimeMs : c_nextMarkerTime;

				float const c_timeDeltaMs = c_nextTime - c_currentTime;
				float const c_speedValue = currentMarker ? currentMarker->GetSpeedValueFloat() : 1.f;
				float const c_realTimeDeltaMs =  c_timeDeltaMs / c_speedValue;

				float const c_percentDelta = c_realTimeDeltaMs / c_dilatedDurationMs;
				float const c_nextPercent = resultPercent + c_percentDelta;

				resultPercent = c_nextPercent;

				if( c_nextTime >= nonDilatedTimeMs )
				{
					break;
				}
			}
		}
	}

	return resultPercent;
}

float CClip::CalculatePlaybackPercentageForMarker( u32 const markerIndex ) const
{
	float resultPercent = 0.f;

	float const c_dilatedDurationMs = GetRawEndTimeAsTimeMs();

	for( int index = -1; index < (int)markerIndex; ++index )
	{
		sReplayMarkerInfo const* currentMarker = index >= 0 ? m_aoMarkers[index] : NULL;
		if( !currentMarker || ( currentMarker && !currentMarker->IsAnchor() ) )
		{
			s32 const c_markerIndex = GetNextMarkerIndex( index, true );
			sReplayMarkerInfo const* nextMarker = c_markerIndex >= 0 ? m_aoMarkers[ c_markerIndex ] : NULL;
			if( !nextMarker || ( nextMarker && !nextMarker->IsAnchor() ) )
			{
				float const c_currentTime = currentMarker ? currentMarker->GetNonDilatedTimeMs() : 0;
				float const c_nextTime = nextMarker ? nextMarker->GetNonDilatedTimeMs() :  GetRawDurationMs();

				float const c_timeDeltaMs = c_nextTime - c_currentTime;
				float const c_speedValue = currentMarker ? currentMarker->GetSpeedValueFloat() : 1.f;
				float const c_realTimeDeltaMs =  c_timeDeltaMs / c_speedValue;

				float const c_percentDelta = c_realTimeDeltaMs / c_dilatedDurationMs;
				float const c_nextPercent = resultPercent + c_percentDelta;

				resultPercent = c_nextPercent;
			}
		}
	}

	return resultPercent;
}

void CClip::RecalculateDilatedTimeStartEndPoints()
{
	m_startTimeMs = GetStartNonDilatedTimeMs();
	m_endTimeMs = CalculateTimeForMarkerMs( GetMarkerCount() - 1 ) + m_startTimeMs;
}

// PURPOSE: Make any post-loading adjustment to marker behaviour for backwards compatability
void CClip::PostLoadMarkerValidation()
{
	sReplayMarkerInfo* lastMarker = GetLastMarker();
	if( lastMarker )
	{
		u8 const c_cameraType = lastMarker->GetCameraType();
		ReplayCameraType const c_validatedCameraType = c_cameraType == RPLYCAM_FREE_CAMERA ? RPLYCAM_FREE_CAMERA : RPLYCAM_INERHITED;
		lastMarker->SetCameraType( c_validatedCameraType );
	}
}

sReplayMarkerInfo* CClip::PopMarker( u32 const index )
{
	sReplayMarkerInfo* marker = TryGetMarker( index );
	if( marker )
	{
		ReplayMarkerInfoCollection::iterator iter = m_aoMarkers.begin() + index;
		m_aoMarkers.erase( iter );

		RecalculateDilatedTimeStartEndPoints();
	}

	return marker;
}

#endif // GTA_REPLAY
