//========================================================================================================================
// name:		MontageText.cpp
// description:	Text display over a montage sequence.
//========================================================================================================================
#include "MontageText.h"

#if GTA_REPLAY

// framework
#include "fwutil/ProfanityFilter.h"

// game
#include "replaycoordinator/replay_coordinator_channel.h"
#include "replaycoordinator/ReplayEditorParameters.h"
#include "text/text.h"

REPLAY_COORDINATOR_OPTIMISATIONS();

template< typename _bcTextDataType > static void defaultTextLoadFunction( FileHandle hFile, size_t const serializedSize, 
																		   CMontageText::MontageTextData_Current& inout_targetData, u64& inout_resultFlags )
{
	_bcTextDataType dataBlob;
	size_t const c_expectedSize = sizeof(dataBlob);

	inout_resultFlags |= c_expectedSize == serializedSize ? inout_resultFlags : CReplayEditorParameters::TEXT_LOAD_RESULT_BAD_SIZE;

	if( rcVerifyf( ( inout_resultFlags & CReplayEditorParameters::TEXT_LOAD_RESULT_BAD_SIZE ) == 0, 
		"CMontageText::Load (defaultTextLoadFunction) - Size change without version change? Not supported!"
		"Expected %" SIZETFMT "u but serialized as %" SIZETFMT "u", c_expectedSize, serializedSize ) )
	{
		// Read in all data
		s32 sizeRead = CFileMgr::Read( hFile, (char*)(&dataBlob), c_expectedSize );

		inout_resultFlags |= sizeRead != c_expectedSize ? CReplayEditorParameters::TEXT_LOAD_RESULT_EARLY_EOF : inout_resultFlags;
		if( ( inout_resultFlags & CReplayEditorParameters::TEXT_LOAD_RESULT_EARLY_EOF ) == 0 )
		{
			inout_targetData = dataBlob;
		}
	}
}

CMontageText::CMontageText() 
	: m_data()
{

}

CMontageText::CMontageText( CMontageText const& other )
	: m_data( other.m_data )
{
	
}

CMontageText::CMontageText( u32 const startTimeMs, u32 const durationMs )
	: m_data( startTimeMs, durationMs )
{

}

CMontageText::~CMontageText()
{
}


CMontageText& CMontageText::operator=( const CMontageText& rhs )
{
	if( this != &rhs )
	{
		m_data = rhs.m_data;
	}

	return *this;
}

void CMontageText::Invalidate()
{
	m_data.Invalidate();
}

bool CMontageText::SetLine( u32 const lineIndex, char const * const text, Vector2 const& position, Vector2 const& scale, 
						   CRGBA const& color, s32 const style )
{
	bool success = false;

	if( lineIndex < GetMaxLineCount() )
	{
		MontageTextData_Current::MontageTextInstance& textLine = m_data.m_lines[ lineIndex ];

		textLine.m_text.Set( text );
		textLine.m_position = position;
		textLine.m_scale = scale; 
		textLine.m_color = color;
		textLine.m_style = style;
		textLine.m_isSet = true;

		success = true;
	}

	return success;
}

void CMontageText::ResetLine( u32 const lineIndex )
{
	if( lineIndex < GetMaxLineCount() )
	{
		m_data.m_lines[ lineIndex ].Reset();
	}
}

bool CMontageText::IsLineSet( u32 const lineIndex ) const
{
	return m_data.IsLineSet( lineIndex );
}

size_t CMontageText::ApproximateMinimumSizeForSaving()
{
	size_t const c_sizeNeeded = sizeof(MontageTextData_Current);
	return c_sizeNeeded;
}

size_t CMontageText::ApproximateMaximumSizeForSaving()
{
	size_t const c_sizeNeeded = sizeof(MontageTextData_Current);
	return c_sizeNeeded;
}

size_t CMontageText::ApproximateSizeForSaving() const
{
	size_t const c_sizeNeeded = sizeof(MontageTextData_Current);
	return c_sizeNeeded;
}

u64 CMontageText::Save( FileHandle hFile )
{
	rcAssertf( hFile, "CMontageText::Save - Invalid handle");

	s32 const c_dataSizeExpected = sizeof(m_data);
	s32 const c_dataSizeWritten  = CFileMgr::Write( hFile, (char*)(&m_data), c_dataSizeExpected );

	u64 const c_result = c_dataSizeExpected == c_dataSizeWritten ? 
		CReplayEditorParameters::PROJECT_SAVE_RESULT_SUCCESS : CReplayEditorParameters::PROJECT_SAVE_RESULT_WRITE_FAIL;

	return c_result;
}

u64 CMontageText::Load( FileHandle hFile, u32 const serializedVersion, size_t const serializedSize )
{
	rcAssertf( hFile, "CMontageText::Load - Invalid handle" );

	u64 result = CReplayEditorParameters::TEXT_LOAD_RESULT_SUCCESS;

	result = serializedVersion > MONTAGE_TEXT_VERSION ? 
		CReplayEditorParameters::TEXT_LOAD_RESULT_BAD_VERSION : result;

	if( rcVerifyf( ( result & CReplayEditorParameters::TEXT_LOAD_RESULT_BAD_VERSION ) == 0, 
		"CMontageText::Load - Text Version %d greater than current version %d. Have you been abusing time-travel?", 
		serializedVersion, MONTAGE_TEXT_VERSION ) )
	{
		//! Store the offset before markers, in-case anything goes wrong
		s32 const c_offsetBefore = CFileMgr::Tell( hFile );
		bool isCorrupted = true;

		//! Current version
		if( serializedVersion == MONTAGE_TEXT_VERSION )
		{
			defaultTextLoadFunction<MontageTextData_Current>( hFile, serializedSize, m_data, result );
			isCorrupted = ( result & CReplayEditorParameters::TEXT_LOAD_RESULT_EARLY_EOF ) != 0;
		}
		else if( serializedVersion == 1 )
		{
			defaultTextLoadFunction<MontageTextData_V1>( hFile, serializedSize, m_data, result );
			isCorrupted = ( result & CReplayEditorParameters::TEXT_LOAD_RESULT_EARLY_EOF ) != 0;

			result |= isCorrupted ? result : CReplayEditorParameters::TEXT_LOAD_RESULT_VERSION_UPGRADED;
		}
		else
		{
			rcWarningf( "CMontageText::Load - Encountered unsupported text version %d, handling it as corrupted", serializedVersion );
			result |= CReplayEditorParameters::TEXT_LOAD_RESULT_UNSUPPORTED_VERSION;
		}

		if( !isCorrupted )
		{
			rage::fwProfanityFilter const& c_filter = rage::fwProfanityFilter::GetInstance();

			for( u32 index = 0; !isCorrupted && index < MAX_TEXT_LINES; ++index )
			{
				MontageTextData_Current::MontageTextInstance const* textLine = GetInstanceData( index );
				rage::fwProfanityFilter::eRESULT const c_result = textLine && textLine->HasText() ?
					c_filter.CheckForProfanityAndReservedTerms( textLine->m_text.getBuffer() ) : textLine == NULL ? 
					rage::fwProfanityFilter::RESULT_PROFANE : rage::fwProfanityFilter::RESULT_VALID;

				isCorrupted = c_result != rage::fwProfanityFilter::RESULT_VALID;
			}

			result |= isCorrupted ? CReplayEditorParameters::TEXT_LOAD_RESULT_PROFANE : result;
		}

		if( isCorrupted )
		{
			// Skip unsupported/corrupt versions
			s32 const c_offset = (s32)c_offsetBefore + (s32)serializedSize;
			s32 const c_newPos = CFileMgr::Seek( hFile, c_offset );

			result |= c_offset != c_newPos ? CReplayEditorParameters::TEXT_LOAD_RESULT_EARLY_EOF : result;
			Invalidate();
		}
	}

	return result;
}

bool CMontageText::ShouldRender( u32 const timeMs ) const
{
	return ( timeMs >= m_data.m_startTimeMs && timeMs <= m_data.m_startTimeMs + m_data.m_durationMs );
}

#endif // GTA_REPLAY
