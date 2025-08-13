#ifndef REPLAY_MARKER_MANANGER_H
#define REPLAY_MARKER_MANANGER_H

#include "ReplaySettings.h"

#if GTA_REPLAY

#include "control/replay/File/device_replay.h"

enum MarkerImportance
{
	IMPORTANCE_LOWEST,
	IMPORTANCE_LOW,
	IMPORTANCE_NORMAL,
	IMPORTANCE_HIGH,
	IMPORTANCE_HIGHEST,
};

enum eMissionMarkerState
{
	MISSION_MARKER_STATE_IDLE,
	MISSION_MARKER_STATE_ENUMERATE,
	MISSION_MARKER_STATE_CHECK_ENUMERATE,
	MISSION_MARKER_STATE_GET_MISSION_INDEX,
	MISSION_MARKER_STATE_DELETE,
	MISSION_MARKER_STATE_CHECK_DELETE,
};

enum MarkerOverlap
{
	OVERLAP_IGNORE,
	OVERLAP_VEHICLE_DAMAGE,
};

enum eMarkerSource
{
	CODE = 0,				// Code	 (as in markers set by gamecode)
	SCRIPT,					// Script
	CODE_MANUAL,			// Manual (as in user manual recordings)
#if __BANK
	CODE_RAG_TEST = CODE_MANUAL
#endif
};

#include <math.h>

#if __BANK
struct MarkerDebugInfo
{
#define MARKER_AVERAGE_BLOCK_SAMPLE_SIZE 256

	MarkerDebugInfo() : m_Top(0)
		,m_Bottom(0)
		,m_AveBlockSize(0)
		,m_MaxBlockSize(0)
		,m_Variance(0)
		,m_StdDeviation(0)
	{
		for(int i = 0; i < MARKER_AVERAGE_BLOCK_SAMPLE_SIZE; ++i)
		{
			m_PrevSizes[i] = 0;
			m_DifFromMean[i] = 0;
		}
	}

	void SetMaxBlockSize(u32 size)
	{
		if( size > m_MaxBlockSize )
		{
			m_MaxBlockSize = size;
		}

		m_PrevSizes[m_Top] = size;

		IncrementOffset(m_Top);

		if( m_Top == m_Bottom )
		{
			IncrementOffset(m_Bottom);
		}

		CalcAveBlockSize();
	}

	void IncrementOffset(u32& offset)
	{
		offset++;
		if(offset >= MARKER_AVERAGE_BLOCK_SAMPLE_SIZE)
		{
			offset = 0;
		}
	}

	void CalcAveBlockSize()
	{
		double blockTotal = 0;
		for(int i = 0; i < GetCount(); ++i)
		{
			blockTotal += m_PrevSizes[i];
		}
		m_AveBlockSize = u32(blockTotal) / GetCount();

		blockTotal = 0;
		for(int i = 0; i < GetCount(); ++i)
		{
			m_DifFromMean[i] = m_PrevSizes[i] - m_AveBlockSize;

			blockTotal += pow((float)m_DifFromMean[i], 2);
		}
		
		m_Variance = u32(blockTotal / GetCount());

		m_StdDeviation = u32(sqrt((float)m_Variance));
	}

	u32 GetCount() const
	{
		if(m_Top < m_Bottom || m_Top == MARKER_AVERAGE_BLOCK_SAMPLE_SIZE-1)
		{
			return MARKER_AVERAGE_BLOCK_SAMPLE_SIZE;
		}

		return m_Top-m_Bottom;
	}

	u32 m_MaxBlockSize;
	u32 m_AveBlockSize;
	u32 m_PrevSizes[MARKER_AVERAGE_BLOCK_SAMPLE_SIZE];
	s32 m_DifFromMean[MARKER_AVERAGE_BLOCK_SAMPLE_SIZE];
	u32 m_Variance;
	u32 m_StdDeviation;

	u32 m_Bottom;
	u32 m_Top;
};
#endif // __BANK

//rage
#include "atl/array.h"
#include "bank/bank.h"
#include "fwsys/timer.h"
#include "atl/string.h"

//game
#include "control/replay/ReplayControl.h"

//system
#include <time.h>

#define MINIMUM_CLIP_LENGTH 3000
#define MAX_FILTER_LENGTH	128

class ReplayBufferMarker
{
public:
	ReplayBufferMarker()
	{
		Reset();
	}

	void	SetUnixTime(u64 time)				{ m_UnixTime = time;			}
	u64		GetUnixTime() const					{ return m_UnixTime;			}

	void	SetStartTime(u32 time)				{ m_StartTime = time;			}
	u32		GetStartTime() const				{ return m_StartTime;			}
	u32		GetRelativeStartTime() const		{ return m_StartTime - m_ClipStartTime;	}

	void	SetEndTime(u32 time)				{ m_EndTime = time;				}
	u32		GetEndTime() const					{ return m_EndTime;				}
	u32		GetRelativeEndTime() const			{ return m_EndTime - m_ClipStartTime;	}

	void	SetTimeInPast(u32 time)				{ m_TimeInPast = time;			}
	u32		GetTimeInPast() const				{ return m_TimeInPast;			}

	void	SetTimeInFuture(u32 time)			{ m_TimeInFuture = time;		}
	u32		GetTimeInFuture() const				{ return m_TimeInFuture;		}

	void	SetEndBlockIndex(u16 index)			{ m_EndBlockIndex = index;		}
	u16		GetEndBlockIndex() const			{ return m_EndBlockIndex;		}

	s32		GetLength() const					{ return (s32)(m_EndTime - m_StartTime); }

	void	SetStartBlockIndex(u16 index)		{ m_StartBlockIndex = index;	}
	u16		GetStartBlockIndex() const			{ return m_StartBlockIndex;		}

	void	SetImportance(MarkerImportance importance)	{ m_Importance = importance;	}
	u8		GetImportance() const						{ return (u8)m_Importance;		}

	void	SetPlayerType(s8 playerType)		{ m_PlayerType = playerType;	}
	s8		GetPlayerType() const				{ return m_PlayerType;			}

	void	SetMissionName(const char* missionName)			{ strcpy_s(m_MissionName, MAX_FILTER_LENGTH, missionName);	}
	atString GetMissionName() const							{ return atString(m_MissionName);	}

	void	SetMontageFilter(const char* montageFilter)		{ strcpy_s(m_MontageFilter, MAX_FILTER_LENGTH, montageFilter);	}
	atString GetMontageFilter() const						{ return atString(m_MontageFilter);	}

	void    SetClipStartTime(u32 time)          { m_ClipStartTime = time;       }
	u32     GetClipStartTime() const            { return m_ClipStartTime;       }
	
	u64  	GetUnixStartTime() const			{ return m_UnixTime - (m_TimeInPast / 1000);	}
	u64  	GetUnixEndTime() const				{ return m_UnixTime + (m_TimeInFuture/ 1000);	}

	void	SetSource(u8 source)				{ m_Source = source;							}
	u8		GetSource() const					{ return m_Source;								}

	void	SetMarkerOverlap(MarkerOverlap overlapid)		{ m_OverlapId = overlapid;			}
	MarkerOverlap	GetMarkerOverlap()						{ return m_OverlapId;				}

	void	SetMissionIndex(u32 index)				{ m_MissionIndex = index;					}
	u32		GetMissionIndex() const					{ return m_MissionIndex;					}

	bool 	IsValid() const { return m_StartTime != 0 && m_EndTime != 0 && (m_EndTime - m_StartTime >= 1000); }

	void	Reset()
	{
		m_UnixTime = 0;

		m_EndTime = 0;
		m_StartTime = 0;

		m_TimeInPast = 0;
		m_TimeInFuture = 0;

		m_Importance = IMPORTANCE_NORMAL;

		m_StartBlockIndex = 0;
		m_EndBlockIndex = 0xffff;	// Sentinel value
		m_ClipStartTime = 0;

		m_PlayerType = -1;

		m_MissionIndex = 0;

		m_Source = 0;

		memset(m_MissionName, 0, sizeof(m_MissionName));
		memset(m_MontageFilter, 0, sizeof(m_MontageFilter));
	}

private:
	u64					m_UnixTime;
	u32					m_StartTime;
	u32					m_EndTime;
	u32					m_ClipStartTime;
	u32					m_TimeInPast;
	u32					m_TimeInFuture;
	u16					m_StartBlockIndex;
	u16					m_EndBlockIndex;
	u32					m_MissionIndex;
	MarkerImportance	m_Importance;
	s8					m_PlayerType;
	char				m_MissionName[MAX_FILTER_LENGTH];
	char				m_MontageFilter[MAX_FILTER_LENGTH];
	u8					m_Source;
	MarkerOverlap		m_OverlapId;
};

typedef atArray<ReplayBufferMarker> ReplayMarkers;
typedef atFixedArray<ReplayBufferMarker, REPLAY_HEADER_EVENT_MARKER_MAX> atFixedArrayReplayMarkers;


class ReplayBufferMarkerMgr
{
	friend class CReplayControl;

public:
	static	void InitSession(unsigned initMode);

	static	void ShutdownSession(unsigned);

	static void InitWidgets();

	static void AddMarker(u32 msInThePast, u32 msInTheFuture, MarkerImportance importance, eMarkerSource source = CODE, MarkerOverlap markeroverlap = OVERLAP_IGNORE);

	static atFixedArrayReplayMarkers& GetProcessedMarkers() { return sm_ProcessedMarkers; }

	static void Process();

	static void ClipUnprocessedMarkerStartTimesToCurrentEndTime(bool includeManualMarkers);
	static bool ClipUnprocessedMarkerEndTimesToCurrentEndTime(bool includeManualMarkers);
	static bool CheckForMarkerOverlap(u32 msInThePast, u32 msInTheFuture, MarkerOverlap markerOverlap);

	static void Flush();

	static s32 GetMarkerCount(u16 index) { return sm_BlockMarkerCounts[index]; }

	static void DeleteMissionMarkerClips();

#if __BANK
	static void DrawDebug(bool drawBlocks);
	static MarkerDebugInfo& GetDebugInfo() { return sm_DebugInfo; }
#endif //__BANK

private:
#if __BANK
	static void AddMarkerCallback();
	
	static s32 sm_TimeInFuture;
	static s32 sm_TimeInPast;
	static MarkerDebugInfo sm_DebugInfo;
#endif //__BANK

	static bool AddMarkerInternal(u32 msInThePast, u32 msInTheFuture, MarkerImportance importance, eMarkerSource source = CODE, MarkerOverlap markeroverlap = OVERLAP_IGNORE);

	static void Reset();

	static void ProcessMarkers();
	static void CheckForSplittingManualMarker();
	static void ProcessSaving();
	static bool GetProcessedMarkerWithLowestStartTime(u32 &index);
	static bool MarkBlocksForSave(u8& importance);
	static bool SaveMarkers();

	static void FlushInternal();

	static bool OnSaveCallback(int retCode);

	static bool IsSaving() { return sm_Saving || sm_StartFlush; }

	static void UpdateMissionMarkerClips();
	
	static eMissionMarkerState			sm_MissionMarkerState;
	static FileDataStorage				sm_DataStorage; 

	static atFixedArrayReplayMarkers	sm_SavedMarkers;
	static atFixedArrayReplayMarkers	sm_ProcessedMarkers;
	static atFixedArrayReplayMarkers	sm_Markers;
	static s32							sm_BlockMarkerCounts[MAX_NUM_REPLAY_BLOCKS];
	static bool							sm_Saving;
	static bool							sm_Enabled;
	static bool							sm_Flush;
	static bool							sm_StartFlush;
};


#endif //GTA_REPLAY

#endif //REPLAY_MARKER_MANANGER_H
