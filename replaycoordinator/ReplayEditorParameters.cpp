//=====================================================================================================
// name:		ReplayEditorParameters.cpp
// description:	Parameters used for editing/manipulating a replay editor objects from a higher level
//=====================================================================================================
#include "ReplayEditorParameters.h"

#if GTA_REPLAY

#include "replaycoordinator/replay_coordinator_channel.h"

REPLAY_COORDINATOR_OPTIMISATIONS();

void CReplayEditorParameters::PrintClipLoadResultStrings( u64 const resultFlags )
{
#if __BANK

	static char const * const s_clipLoadResultStrings[ CLIP_LOAD_RESULT_USED_BITFIELDS ] =
	{
		"Unexpected end of file", "Out of memory", "Bad clip version", "Unsupported clip version, missing back compat loading?", 
		"Header is invalid", "Bad marker version", "Unsupported marker version, missing back compat loading?", 
		"Bad marker size, markers reset to default", "Too many markers, excess culled", "Overlapping markers, markers reset to default", 
		"Bad thumbnail size", "Bad time data",

		"Marker version upgraded"
	};

	if( resultFlags != CLIP_LOAD_RESULT_SUCCESS )
	{
		rcDisplayf( "Clip load generated the flags %016lX;", resultFlags );

		for( u32 bitIndex = 0; bitIndex < CLIP_LOAD_RESULT_USED_BITFIELDS; ++bitIndex )
		{
			u64 const c_comparisonValue = 1 << bitIndex;
			if( ( resultFlags & c_comparisonValue ) != 0 )
			{
				// Note - uses %s formatting to work around false-positive buffer-overrun warning
				rcDisplayf( "%s", s_clipLoadResultStrings[ bitIndex ] );
			}
		}

		rcDisplayf( "=================================" );
	}
#else

	(void)resultFlags;

#endif
}

void CReplayEditorParameters::PrintTextLoadResultStrings( u64 const resultFlags )
{
#if __BANK

	static char const * const s_textLoadResultStrings[ TEXT_LOAD_RESULT_USED_BITFIELDS ] =
	{
		"Unexpected end of file", "Out of memory", "Bad text version", "Unsupported text version, missing back compat loading?", 
		"Bad text size", "Text contained profanity", "Text version upgraded"
	};

	if( resultFlags != TEXT_LOAD_RESULT_SUCCESS )
	{
		rcDisplayf( "Text load generated the flags %016lX;", resultFlags );

		for( u32 bitIndex = 0; bitIndex < TEXT_LOAD_RESULT_USED_BITFIELDS; ++bitIndex )
		{
			u64 const c_comparisonValue = 1 << bitIndex;
			if( ( resultFlags & c_comparisonValue ) != 0 )
			{
				// Note - uses %s formatting to work around false-positive buffer-overrun warning
				rcDisplayf( "%s", s_textLoadResultStrings[ bitIndex ] );
			}
		}

		rcDisplayf( "=================================" );
	}
#else

	(void)resultFlags;

#endif
}

void CReplayEditorParameters::PrintMusicLoadResultStrings( u64 const resultFlags )
{
#if __BANK

	static char const * const s_musicLoadResultStrings[ MUSIC_LOAD_RESULT_USED_BITFIELDS ] =
	{
		"Unexpected end of file", "Out of memory", "Bad music track version", "Unsupported music track version, missing back compat loading?", 
		"Bad music track size", "Sound-hash was invalid", "Copyrighted track already used once in project", "Music track version upgraded"
	};

	if( resultFlags != MUSIC_LOAD_RESULT_SUCCESS )
	{
		rcDisplayf( "Music load generated the flags %016lX;", resultFlags );

		for( u32 bitIndex = 0; bitIndex < MUSIC_LOAD_RESULT_USED_BITFIELDS; ++bitIndex )
		{
			u64 const c_comparisonValue = 1 << bitIndex;
			if( ( resultFlags & c_comparisonValue ) != 0 )
			{
				// Note - uses %s formatting to work around false-positive buffer-overrun warning
				rcDisplayf( "%s", s_musicLoadResultStrings[ bitIndex ] );
			}
		}

		rcDisplayf( "=================================" );
	}
#else

	(void)resultFlags;

#endif
}

void CReplayEditorParameters::PrintProjectLoadResultStrings( u64 const resultFlags )
{
#if __BANK

	static char const * const s_projectLoadResultStrings[ PROJECT_LOAD_RESULT_USED_BITFIELDS ] =
	{
		"Invalid file", "Unexpected end of file", "Out of memory", "Bad project version", "Unsupported project version, missing back compat loading?", 
		"Bad replay version", "Unsupported replay version", "Bad cached duration", 
		
		"Bad clip data", "Some clip Markers reset", "Some clip markers truncated", "Some clip timing data reset", "Some clips were truncated", 
		
		"Bad text data", "Some text data reset", "Some text contained profanity", "Some text was truncated", 
		
		"Bad music track data", "Some music data reset", "Some music had an invalid sound hash", "Some copyrighted music was used multiple times in project",
		"Some music was truncated", 

		"Project version upgraded"
	};

	if( resultFlags != PROJECT_LOAD_RESULT_SUCCESS )
	{
		rcDisplayf( "Project load generated the flags %016lX;", resultFlags );

		for( u32 bitIndex = 0; bitIndex < PROJECT_LOAD_RESULT_USED_BITFIELDS; ++bitIndex )
		{
			u64 const c_comparisonValue = 1 << bitIndex;
			if( ( resultFlags & c_comparisonValue ) != 0 )
			{
				// Note - uses %s formatting to work around false-positive buffer-overrun warning
				rcDisplayf( "%s", s_projectLoadResultStrings[ bitIndex ] );
			}
		}

		rcDisplayf( "=================================" );
	}
#else

	(void)resultFlags;

#endif
}

void CReplayEditorParameters::PrintProjectSaveResultStrings( u64 const resultFlags )
{
#if __BANK

	static char const * const s_projectSaveResultStrings[ PROJECT_SAVE_RESULT_USED_BITFIELDS ] =
	{
		"Bad file name", "Bad path", "No disk space", 
		
		"File access error", "File write fail", "File close fail", 
		
		"No clips in project", "Bad time data in clip (corrected)", "Not enough markers (corrected)", "Invalid markers"
	};

	if( resultFlags != PROJECT_SAVE_RESULT_SUCCESS )
	{
		rcDisplayf( "Project save generated the flags %016lX;", resultFlags );

		for( u32 bitIndex = 0; bitIndex < PROJECT_SAVE_RESULT_USED_BITFIELDS; ++bitIndex )
		{
			u64 const c_comparisonValue = 1 << bitIndex;
			if( ( resultFlags & c_comparisonValue ) != 0 )
			{
				// Note - uses %s formatting to work around false-positive buffer-overrun warning
				rcDisplayf( "%s", s_projectSaveResultStrings[ bitIndex ] );
			}
		}

		rcDisplayf( "=================================" );
	}
#else

	(void)resultFlags;

#endif
}

u32 CReplayEditorParameters::GetAutoGenerationDuration(eAutoGenerationDuration duration)
{
	u32 length = 0;
	switch (duration)
	{
		case AUTO_GENERATION_SMALL:
			{
				length = AUTO_GENERATION_SMALL_LENGTH;
				break;
			}

		case AUTO_GENERATION_MEDIUM:
			{
				length = AUTO_GENERATION_MEDIUM_LENGTH;
				break;
			}

		case AUTO_GENERATION_LARGE:
			{
				length = AUTO_GENERATION_LARGE_LENGTH;
				break;
			}

		case AUTO_GENERATION_EXTRA_LARGE:
			{
				length = AUTO_GENERATION_EXTRA_LARGE_LENGTH;
				break;
			}
		
		case AUTO_GENERATION_MAX:
		default:
			{
				length = 0;
				break;
			}
	}

	return length;
}

rage::atHashString const CReplayEditorParameters::sc_autogenDurationLngKey[AUTO_GENERATION_MAX] =
{
	atHashString("VE_LEN_SHORT"), 
	atHashString("VE_LEN_EPISODE"), atHashString("VE_LEN_FILM"), atHashString("VE_LEN_EPIC")
};

atHashString const& CReplayEditorParameters::GetAutogenDurationLngKey( eAutoGenerationDuration const eType )
{
	return eType >= 0 && eType < AUTO_GENERATION_MAX ? sc_autogenDurationLngKey[ eType ] : sc_autogenDurationLngKey[ 0 ];
}

#endif // GTA_REPLAY
