#ifndef REPLAY_MONTAGE_GENERATOR_H_
#define REPLAY_MONTAGE_GENERATOR_H_

#include "control/replay/ReplaySettings.h"
#include "string/string.h"

#if GTA_REPLAY

#include "replaycoordinator/storage/Montage.h"
#include "replaycoordinator/ReplayEditorParameters.h"
#include "frontend/VideoEditor/DataProviders/FileDataProvider.h"

class CFileData;
class ReplayBufferMarker;

namespace rage
{
	class atString;
}

//////////////////////////////////////////////////////////////////////////
class CMissionMontageGenerator
{
public:

	struct sMontageClipCache 
	{
		CFileData* data;
		u64 unixStartTime;
		u64 unixEndTime;
		u64 writeTime;
		u32 startTime;
		u32 endTime;
		u32 length;
		u32 relativeStartTime;
		u32 relativeEndTime;
		u32 clipStartTime;
		u8 importance;
		u32 missionIndex;
	};

	enum sMontageGenerationState
	{
		MONTAGE_GEN_IDLE = 0,
		MONTAGE_GEN_ENUMERATE_FILES,
		MONTAGE_GEN_CHECK_ENUMERATE,
		MONTAGE_GEN_START_GET_HEADER,
		MONTAGE_GEN_CHECK_HEADER,
		MONTAGE_GEN_PROCESS_HEADER,
		MONTAGE_GEN_PROCESS_MARKERS,
	};

	struct sMontageMarker
	{
		void	SetStartTime(u32 time)				{ m_StartTime = time;			}
		u32		GetStartTime() const				{ return m_StartTime;			}
		u32		GetRelativeStartTime() const		{ return m_StartTime - m_ClipStartTime;	}

		void	SetEndTime(u32 time)				{ m_EndTime = time;				}
		u32		GetEndTime() const					{ return m_EndTime;				}
		u32		GetRelativeEndTime() const			{ return m_EndTime - m_ClipStartTime;	}

		void	SetClipTime(float time)				{ m_fClipTime = time;			}
		float	GetClipTime() const					{ return m_fClipTime;			}

		void	SetImportance(MarkerImportance importance)	{ m_Importance = importance;	}
		MarkerImportance GetImportance() const				{ return m_Importance;		}

		void	SetPlayerType(s8 playerType)		{ m_PlayerType = playerType;	}
		s8		GetPlayerType() const				{ return m_PlayerType;			}

		void		SetData(CFileData* data)		{ m_Data = data;  }
		CFileData*  GetData() const			{ return m_Data; }

		void	SetTimeInPast(u32 time)				{ m_TimeInPast = time;			}
		u32		GetTimeInPast() const				{ return m_TimeInPast;			}

		void	SetTimeInFuture(u32 time)			{ m_TimeInFuture = time;		}
		u32		GetTimeInFuture() const				{ return m_TimeInFuture;		}

		void	SetUnixTime(u64 time)				{ m_UnixTime = time;			}
		u64		GetUnixTime() const					{ return m_UnixTime;			}

		u64  	GetUnixStartTime() const			{ return m_UnixTime - (m_TimeInPast / 1000);	}
		u64  	GetUnixEndTime() const				{ return m_UnixTime + (m_TimeInFuture/ 1000);	}

		u32		GetLength() const					{ return m_EndTime - m_StartTime; }

		void    SetClipStartTime(u32 time)          { m_ClipStartTime = time;       }
		u32     GetClipStartTime() const            { return m_ClipStartTime;       }

		void	SetMissionIndex(u32 index)			{ m_MissionIndex = index;		}
		u32		GetMissionIndex() const				{ return m_MissionIndex;		}

		bool 	IsValid() const { return m_StartTime != 0 && m_EndTime != 0 && (m_EndTime - m_StartTime >= 1000); }		

	private:
		u32					m_StartTime;
		u32					m_EndTime;
		u32					m_ClipStartTime;
		float				m_fClipTime;
		MarkerImportance	m_Importance;
		s8					m_PlayerType;

		u64					m_UnixTime;
		u32					m_TimeInPast;
		u32					m_TimeInFuture;
		u32					m_MissionIndex;

		CFileData*			m_Data;
	};

	//Filter - pass in "" for every clip or a element from GetValidMissions()
	CMissionMontageGenerator( CReplayEditorParameters::eAutoGenerationDuration duration, const atString& filter, const bool filterMission = false, const s8 pedtype = -1);
	~CMissionMontageGenerator() {}

	static bool IsProcessing() { return m_MontageState != MONTAGE_GEN_IDLE; }
	static bool HasProcessingFailed() { return m_HasFailed; }

	static bool CheckClipCanBeUsed();

	static bool StartMontageGeneration();
	static void ProcessMontages();

	//! Returns NULL on failure or not started. Caller takes ownership if valid pointer returned.
	static CMontage* GetGeneratedMontage();

	void GetValidMissions(atArray<atString>& validMissions, bool includeMissionString);
	void GetValidPlayerTypes(atArray<s8>& validPlayers); //ePedType

private:
	static void AddClipToCache(const sMontageMarker& marker, u8 importance, CFileData& data, float fClipTime, u32 startTime, u32 endTime, u32 startRelativeTime, u32 endRelativeTime);
	static void AddClipsToMontage(CMontage* pMontage);
	static bool CanMarkerBeAdded(const sMontageMarker& markerToAdd, u8 importance, const CFileData& data, u32& startTime, u32& endTime, u32& startRelativeTime, u32& endRelativeTime);
	static void RemoveOverlappingClips(const sMontageMarker& markerToAdd, const CFileData& data, u32& startTime, u32& endTime, u32& startRelativeTime, u32& endRelativeTime);

	static bool IsCacheFull(u8 importance);
	static void PopulateMonageWithClips(CMontage* pMontage);

	static bool IsValid(u64 aStartTime, u64 aEndTime, u64 bStartTime, u64 bEndTime);

	static u32 m_Duration;
	static atString m_Filter;

	static FileDataStorage sm_DataStorage;
	static atArray<sMontageClipCache> m_ClipCache;
	static atArray<sMontageMarker> sm_ProcessedMarkers;
	static u32 m_ClipCacheLength;
	static bool m_FilterMission;
	static s8 m_FilterPedType;
	static u32 m_MontageState;
	static bool	m_HasFailed;
	static u32 sm_CurrentHeaderIndex;
	static ReplayHeader sm_CurrentHeader;
	static CMontage* sm_Montage;
};

#endif //GTA_REPLAY

#endif //REPLAY_MONTAGE_GENERATOR_H_
