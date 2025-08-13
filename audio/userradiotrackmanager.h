#ifndef __AEUSERRADIOTRACKMANAGER_H
#define __AEUSERRADIOTRACKMANAGER_H

#if __WIN32PC

#ifndef __UNICODE_FILENAMES
#define __UNICODE_FILENAMES 1
#endif

//#if __UNICODE_FILENAMES
//
//#include "userradiotrackmanager_ru.h"
//
//#else

#define USER_TRACK_LOOKUP_FILENAME	L"User Music\\usertracks"

#define USER_TRACK_PATH "\\User Music\\"

#include "audiohardware/mediareader.h"
#include "system/FileMgr.h"
#include "atl/map.h"
#include "atl/bitset.h"
#include "system/xtl.h"
#include "radiostationhistory.h"

#define MAX_SEARCH_LEVELS 15 // how deep (in terms of directories) the search for media will go (potential stack overflow if this value is set too high)

struct tUserTrackFileLookup
{
	char Artist[32];
	char Title[32];
	u32 offset, length; // file name, in characters (not bytes)
	u32 duration;
	u32 skipStart,skipEnd;
	u32 gain;
	USER_TRACK_FORMAT format;

};

enum SCANSTATUS
{
	SCAN_STATE_UNSCANNED,
	SCAN_STATE_IDLE,
	SCAN_STATE_SCANNING,
	SCAN_STATE_SUCCEEDED,
	SCAN_STATE_FAILED
};

enum USERRADIO_PLAYMODE
{
	USERRADIO_PLAY_MODE_RADIO,
	USERRADIO_PLAY_MODE_SEQUENTIAL,
	USERRADIO_PLAY_MODE_RANDOM,	
};

class audUserRadioTrackDatabase
{
public:

	audUserRadioTrackDatabase()
	{

	}

	~audUserRadioTrackDatabase()
	{
		for(s32 i = 0; i < GetNumTracks(); i++)
		{
			if(m_Data[i].FileName)
			{
				delete [] m_Data[i].FileName;
			}			
		}
	}

	s32 GetNumTracks() const { return static_cast<s32>(m_Data.size()); }

	struct TrackData
	{
		TrackData()
			: FileName(NULL)
			, DurationMs(0)
			, SkipStartMs(0)
			, SkipEndMs(0)
			, Loudness(0)
			, format(TRACK_FORMAT_UNKNOWN)
			, FileNameLength(0)
		{

		}

		const WCHAR *FileName;
		char Artist[32];
		char Title[32];		
		u32 DurationMs;
		u32 SkipStartMs;
		u32 SkipEndMs;
		u32 Loudness;
		USER_TRACK_FORMAT format;
		u32 FileNameLength;
	};

	// PURPOSE
	//	Copies track data to list - takes ownership of string ptrs
	s32 AddTrack(const TrackData &track);

	const TrackData &GetTrack(const s32 trackId) const
	{
		return m_Data[trackId];
	}

	TrackData &GetTrack(const s32 trackId)
	{
		return m_Data[trackId];
	}

	s32 FindTrackIndex(const WCHAR *fileName);

	bool Save(const WCHAR *fileName) const;
	bool Load(const WCHAR *fileName);

private:
	atMap<const WCHAR *, s32> m_SearchCache;
	atArray<TrackData> m_Data;
};

class audUserRadioTrackManager
{
public:
	audUserRadioTrackManager();
	~audUserRadioTrackManager();

	bool Initialise();
	void Shutdown();
	s32 GetNextTrack();
	
	bool StartMediaScan(bool completeScan = false);
	void AbortMediaScan();

	bool IsScanning() const { return m_ScanStatus == SCAN_STATE_SCANNING; }
	bool IsScanningIdle() const { return m_ScanStatus == SCAN_STATE_IDLE; }

	const WCHAR *GetFileName(const s32 TrackID) const;
	u32 GetTrackDuration(const s32 trackId) const;
	u32 GetStartOffsetMs(const s32 trackId) const;
	f32 ComputeTrackMakeUpGain(const s32 trackId) const;
	u32 GetTrackPostRoll(const s32 trackId) const;

	const char *GetTrackTitle(const s32 trackId) const;
	const char *GetTrackArtist(const s32 trackId) const;

	s32 GetNumTracksToScan() const				{ return m_nTracksToScan; }
	bool GetIsCompleteScanRunning()	const		{ return m_IsCompleteScanning; }
	u32 GetScanCount() const					{ return m_ScanCount; }
	s32 GetNumTracks() const					{ return (m_CurrentDatabase ? m_CurrentDatabase->GetNumTracks() : 0); }

	bool HasTracks() const						{ return GetNumTracks() > 2; }

	USERRADIO_PLAYMODE GetRadioMode() const;

	void SetCurrentTrack(s32 currentTrack)		{ Assert(currentTrack < GetNumTracks()); m_CurrentTrack = currentTrack; }
	s32 GetCurrentTrack()						{ return m_CurrentTrack; }

	s32 GetTrackIdFromHistory(s32 index)		{ return m_History[index]; }
	void SetNextTrack(s32 trackId)		{ m_ForceNextTrack = trackId; }
	
	bool LoadFileLookup();
	
	static USER_TRACK_FORMAT GetFileFormat(const WCHAR *fileName);

	void UpdateScanningUI(const bool showCompletionMessage = true);

	audRadioStationHistory *GetHistory() { return &m_History; }

	void PostLoad();

private:

	struct utScanState
	{
		atArray<tUserTrackFileLookup*> lookup;
		atMap<const WCHAR *,u32> hashtable;
		atBitSet reftable;
		FileHandle nameTableHandle;
		u32 currentOffset;
		u32 trackFormatCount[TRACK_FORMAT_COUNT];
	};


	static void AnalyzeTrack(const WCHAR *trackPath, audMediaReader *pTrack, u32 &duration, u32 &skipStart, u32 &skipEnd, u32 &gain);
	static void AnalyzeBuffer(const s32 samplesRead, const u32 rmsWindowSize, s32 &firstNonZeroSample, s32 &lastNonZeroSample, f32 &maxPeak, f32 &maxRms);
	void SetStatus(SCANSTATUS status)
	{
		m_ScanStatus = status;
	}

	bool ParseMP4Duration(const WCHAR *fileName, u32 &duration);
	bool PossiblyAddTrack(const WCHAR *target, audUserRadioTrackDatabase *database);

	static DECLARE_THREAD_FUNC(RebuildFileDatabaseCB);

	void RebuildFileDatabase();

	const WCHAR *GetDataFileLocation() const;

	u32 CatalogueDirectory(const WCHAR *srcDir, audUserRadioTrackDatabase *database, s32 level);
	bool IsFileLink(const WCHAR *pDir);
	const WCHAR *GetLnkTarget(const WCHAR *lnk);
	void AddTrackToHistory(s32 track);
	bool IsTrackInHistory(s32 track);

	audUserRadioTrackDatabase *m_CurrentDatabase;
	audUserRadioTrackDatabase *m_NewDatabase;
		
	bool					m_bInitialised;
		
	bool					m_bIsDecoderAvailable[TRACK_FORMAT_COUNT];
	
	sysIpcThreadId			m_Thread;
	volatile SCANSTATUS		m_ScanStatus;
	volatile bool			m_CompleteScan;

	s32						m_CurrentTrack;
	s32						m_ForceNextTrack;

	volatile bool			m_AbortRequested;
	
	volatile u32			m_ScanCount;
	volatile bool			m_IsCompleteScanning;
	volatile s32			m_nTracksToScan;

	audRadioStationHistory m_History;

};

//#endif //__UNICODE_FILENAMES

#endif // __WIN32PC

#endif