#ifndef __TVCHANNELMANAGER_H__
#define __TVCHANNELMANAGER_H__

// rage headers
#include "parser/manager.h"
#include "parser/macros.h"
#include "atl/array.h"
#include "atl/hashstring.h"
#include "atl/string.h"
#include "atl/singleton.h"
#include "string/string.h"
#include "system/FileMgr.h"
#include "bank/bank.h"
#include "bank/button.h"
#include "bank/bkmgr.h"

#include "Vfx/Misc/MovieManager.h"
#include "game/Clock.h"

#include "control/replay/ReplaySettings.h"

class CTVPlaylistManager;
class CTVPlayListSlot;

//////////////////////////////////////////////////////////////////////////////////////////

typedef enum
{
	TV_CHANNEL_ID_NONE = -1,

	TV_CHANNEL_ID_1 = 0,
	TV_CHANNEL_ID_2,
	TV_CHANNEL_SPECIAL,			// A normal channel
	TV_CHANNEL_ID_SCRIPT,		// Usage below*
	TV_CHANNEL_ID_3,			// new channel, for arcade
	TV_CHANNEL_ID_MAX,

}	TV_CHANNEL_ID;

//	* Script Channel special channel.
//	Setting script channel will cause the movie at the start of the playlist to be pre-cached, 
//	when swapping to CHANNEL_SCRIPT, it will play from the start from the point that the channel was swapped to.
//	This channel should not be being used anywhere in game except places where a seemless channel switch (to the start of another playlist ala B*1114639), is required.
//	So essentially, this one case

//////////////////////////////////////////////////////////////////////////////////////////

class CTVVideoInfo
{
public:

	CTVVideoInfo()
	{ 
		m_iNumTimesPlayed = 0;

		// Make up some data for the others
		m_fDuration = 0.0f;			// Gametime Seconds..
		m_bNotOnDisk = false;
	}

	float	GetDuration() const { return m_fDuration; }

	bool	PassesFilter();

	atHashString	m_Name;				// ID - used for matchup
	atString		m_VideoFileName;	// The name of the video file

private:

	friend class CTVPlaylistManager;
	friend class CTVPlayListSlot;

	static	int SortByTimesPlayedFunc(CTVVideoInfo* const*a, CTVVideoInfo * const*b);

	// Count of times this video has played
	int			m_iNumTimesPlayed;

	// Cached at very beginning of game
	float		m_fDuration;

#if __ASSERT
	float		m_fRealDuration;
#endif

	bool		m_bNotOnDisk;

	PAR_SIMPLE_PARSABLE;
};

class CTVPlayListSlot
{
public:

	CTVVideoInfo *GetVideoForThisSlot();

	atHashString	m_Name;
	atArray< atHashString >	m_TVVideoInfoNames;			// advert_a_1, advert_a_2, advert_a_3 etc..

	PAR_SIMPLE_PARSABLE;
};

class CTVPlayList
{
public:

	atHashString			m_Name;
	float					m_fDurationOfAllIterations;
	atArray< atHashString >	m_TVPlayListSlotNames;

	PAR_SIMPLE_PARSABLE;
};

class CTVPlaylistContainer
{
public:

	void Load();

#if	__BANK
	void Save();
#endif

	void Reset();

	CTVVideoInfo *GetVideo( atHashString name );
	CTVPlayListSlot	*GetPlayListSlot( atHashString name );
	CTVPlayList	*GetPlayList( atHashString name );

	int			GetPlaylistCount()	{ return m_Playlists.size(); }

#if	__BANK
	const char	*GetPlaylistName(int i)	{ return m_Playlists[i].m_Name.GetCStr(); }
#endif	//__BANK

	atArray< CTVVideoInfo >			m_Videos;
	atArray< CTVPlayListSlot >		m_PlayListSlots;
	atArray< CTVPlayList >			m_Playlists;

	PAR_SIMPLE_PARSABLE;
};

//////////////////////////////////////////////////////////////////////////////////////////

class CTVChannelInfo
{
public:

	CTVChannelInfo() 
	{ 
		m_pPlayList = NULL; 
		m_IsDirty = true;
		m_id = TV_CHANNEL_ID_NONE;
	}

	void		SetPlaylist(const char *pPlaylistName, bool bStartFromNow = false);
	void		SetPlaylistFromHour(const char *pPlaylistName, u32 hour);
	void		ClearPlaylist();
	CTVPlayList *GetPlayList() { return m_pPlayList; }
	void		SetStartFromNow();
	void		SetStartFromHourOfRealDay( u32 hour );
	void		UpdateTime();

	int GetTimeOfDayInSecsSafe() const
	{
		int timeOfDayInSecs = (int)m_pPlayList->m_fDurationOfAllIterations;
		if(timeOfDayInSecs == 0)		// avoid div by 0
			timeOfDayInSecs = 1;
		return timeOfDayInSecs;
	}

	u32 GetTimeSinceStartOfDay()
	{
		return (m_PlayTime + m_PlayOffset) % GetTimeOfDayInSecsSafe();
	}

	u32 GetStartOfCurrentDayTime()
	{
		int timeOfDayInSecs = GetTimeOfDayInSecsSafe();
		return ((m_PlayTime + m_PlayOffset) / timeOfDayInSecs) * timeOfDayInSecs;
	}

	void	AdjustPlayOffset(s32 value)	{m_PlayOffset += value;}

	void	SetDirty(bool which = true);
	bool	IsDirty()					{ return m_IsDirty; }
	
	void	SetChannelId(s32 theID)		{ Assertf(m_id == TV_CHANNEL_ID_NONE,"Re-setting a channel ID! BAD!" );   m_id = theID; }

private:

	CTVPlayList *m_pPlayList;
	u32			m_PlayTime;					// Seconds from 2000
	s32			m_PlayOffset;				// Seconds from PlayTime, used for play now and skip functionality
	bool		m_IsDirty;					// Signifies that something has changed and the movie must be re-set
	s32			m_id;						// What channel this is. (may not be required)
};

//////////////////////////////////////////////////////////////////////////////////////////

class CTVPlaylistManager
{

	friend class CTVPlayListSlot;

public:

	//	TV Channel Playback
#define MAX_TV_SHOW_CACHE	(4)
	
	CTVPlaylistManager();

	static void Init(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);
	static void Update();

	static void	SetTVChannel(s32 channel);
	static s32	GetTVChannel()
	{
		return m_TVCurrentChannel;
	}

	static void		SetTVVolume(float volume);
	static float	GetTVVolume() { return m_TVVolume; }
	static void		UpdateChannelScript();

	static u32		GetCurrentShowNameHash(void);
	static float	SetToNearestKeyFrame(u32 handle, float time);
	static void		UpdateTVPlayback();
	static u32		GetCurrentlyPlayingHandle()
	{
		if(m_pCurrentCachePlaying)
		{
			return m_pCurrentCachePlaying->m_TVShowHandle;
		}
		return INVALID_MOVIE_HANDLE;
	}
	static void		CacheCurrentlyPlayingColour(int r, int g, int b, int a)
	{
		if(m_pCurrentCachePlaying)
		{
			m_pCurrentCachePlaying->m_colour.SetRed(r);
			m_pCurrentCachePlaying->m_colour.SetGreen(g);
			m_pCurrentCachePlaying->m_colour.SetBlue(b);
			m_pCurrentCachePlaying->m_colour.SetAlpha(a);
		}
	}

	static void SetTVChannelPlaylist(s32 channel, const char *PlaylistName, bool startFromNow = false);
	static void SetTVChannelPlaylistFromHour(s32 channel, const char *PlaylistName, int hour);

	static void ClearTVChannelPlaylist(s32 channel);

	static bool IsPlaylistOnChannel(s32 channel, u32 nameHash);
	static bool IsTVShowCurrentlyPlaying(u32 nameHash);

	static void SetEntityForAudio(CEntity *entity)
	{
		m_EntityForAudio = entity;
	}
	static void SetIsAudioFrontend(const bool isFrontend) { m_IsAudioFrontend = isFrontend; }

	static void SetForceKeyframeWait(bool bOnOff) { m_bWaitForKeyframeSyncOnSwitch = bOnOff; }

	static float ScaleToGameTime(float realSeconds);
	static float ScaleToRealTime(float gameSeconds);

#if __BANK
	static void PrevShowCB();
	static void NextShowCB();
#endif

private:

	static void	Load();

	static void	PostLoadProcess();

	static CTVPlaylistContainer	m_PlayListContainer;

	static CTVVideoInfo	*GetVideo( atHashString name );
	static CTVPlayListSlot	*GetPlayListSlot( atHashString name );

public:

	static CTVPlayList	*GetPlayList( atHashString name );

	static CTVVideoInfo			*GetCurrentlyPlayingVideo()
	{
		if(m_pCurrentCachePlaying)
		{
			return m_pCurrentCachePlaying->m_pTVShow;
		}
		return NULL;
	}

	static float				GetCurrentTVShowTime()	{ return m_CurrentVideoTime; }

#if __BANK
	static void					Save();
#endif //__BANK

private:

	static bool		HaveAllVideosBeenProcessed(CTVPlayList &playlist);
	static float	GetPlayListTimeTotalIterationTime(CTVPlayList &playlist);

	// Get which video we should be playing at this time
	static bool	GetVideoInfo( int channelID, int &slotID );

	static CTVVideoInfo			*GetCurrentTVShow()		{ return m_pCurrentVideo; }

	static CTVVideoInfo			*GetNextTVShow()		{ return m_pNextVideo; }
	static float				GetNextTVShowTime()		{ return m_NextVideoTime; }

	static s32					m_TVDesiredChannel;
	static s32					m_TVCurrentChannel;

	// Which channel is using which playlist
	static CTVChannelInfo		m_Channels[ TV_CHANNEL_ID_MAX ];

	// Telemetry Stuff
#if __ASSERT
	static u32			m_TelemWatchingFrameNumber;
#endif	//__ASSERT
	static bool			m_TelemPlayerIsWatchingTV;
	static bool			m_TelemPlayerWasWatchingTV;
	static s32			m_TelemLastTVChannelID;
	static atHashString	m_TelemLastPlaylistName;
	static atHashString	m_TelemLastProgramName;
	static u32			m_TelemLastChangeTime;
public:
	static void	SetWatchingTVThisFrame();
private:
	static void GetNamesSafe(s32 TvChannelID, CTVVideoInfo *pVideoInfo, atHashString &playlistName, atHashString &videoName);
	static void	RecordTelemetryStart(s32 TvChannelID, atHashString playlistName, atHashString videoName);
	static void	UpdateTelemetry(s32 TvChannelID, CTVVideoInfo *pVideoInfo);
	// Telemetry Stuff

#if __BANK

	static void CreateTestData();

	static CTVVideoInfo			*GetPrevTVShow()		{ return m_pPrevVideo; }
	static float				GetPrevTVShowTime()		{ return m_PrevVideoTime; }
	static CTVVideoInfo			*m_pPrevVideo;
	static float				m_PrevVideoTime;		// The duration of the previous video
#endif	//__BANK

	static CTVVideoInfo			*m_pCurrentVideo;
	static float				m_CurrentVideoTime;		// The time through the current video
	static CTVVideoInfo			*m_pNextVideo;
	static float				m_NextVideoTime;		// The time until the next video

	static u32					m_LastTODStart;
	static int					m_LastSlotID;

	static bool					m_bWaitForKeyframeSyncOnSwitch;
	static float				m_TimeToWaitFor;		// -ve == no time

	// TV Channel Stuff
	static int					FindEmptyTVShowCacheSlot();					// Returns cache idx
	static int					FindTVShowInCache(CTVVideoInfo *pShow);		// Returns cache idx
	static int					CacheTVShow(CTVVideoInfo *pShow);			// Returns cache idx
	static void					StopTV();

	class CachedTVShow
	{
	public:

		CachedTVShow()
		{ 
			m_pTVShow = NULL; 
			m_TVShowHandle = INVALID_MOVIE_HANDLE; 
			m_bForced = false;

			m_colour.Set(255, 255, 255, 255);
		}

		void Release(CMovieMgr *pManager)
		{
			if(m_TVShowHandle != INVALID_MOVIE_HANDLE)
			{
				if(pManager->IsPlaying(m_TVShowHandle))
				{
					pManager->Stop(m_TVShowHandle);
				}
				pManager->Release(m_TVShowHandle);
				m_TVShowHandle = INVALID_MOVIE_HANDLE;
				m_pTVShow = NULL;
				m_bForced = false;
			}
		}

		u32 Cache(CMovieMgr *pManager, CTVVideoInfo *pTVShow)
		{
			m_TVShowHandle = pManager->PreloadMovie(pTVShow->m_VideoFileName.c_str(), true);
			if(m_TVShowHandle != INVALID_MOVIE_HANDLE)
			{
				m_pTVShow = pTVShow;
			}
			return m_TVShowHandle;
		}

		void SetForced(bool forced) { m_bForced = forced; }
		bool GetForced() { return m_bForced; }

		CTVVideoInfo		*m_pTVShow;
		u32					m_TVShowHandle;
		bool				m_bForced;

		Color32				m_colour;
	};

	static float			m_TVVolume;

	// Two possible shows cached at any one time
	static CachedTVShow		m_TVShowCache[MAX_TV_SHOW_CACHE];
	// Pointer to the current cache used for playing
	static CachedTVShow		*m_pCurrentCachePlaying;

	// Script can supply an entity to attach the audio from the stream
	static RegdEnt			m_EntityForAudio;
	// Frontend audio plays 2D, non-frontend audio plays positionally
	static bool				m_IsAudioFrontend;

#if __BANK

	static bkBank	*ms_pBank;
	static bkButton	*ms_pCreateButton;
	static int		ms_ChannelSelection;
	static int		ms_PlaylistSelection;
	static const char **ms_pPlaylistNames;
	static bool		ms_bRenderOnDebugTexture;
	static u32		ms_StartHour;
	static bool		ms_bDumpCacheContentInfo;
	static bool		ms_bShowClipName;

	static void DumpCacheContentInfo();

	// Set the TV Channel playing
	static void SetChannelCB();
	static void SetPlaylistCB();
	static void SetPlaylistNowCB();
	static void SetPlaylistHourCB();
	static void ClearPlaylistCB();
	static void DumpTodaysPlaylistToTTYCB();
	static void SavePlaylistCB();
	static void SetupLester1CB();

	static void InitBank();
	static void CreateBank();
	static void ShutdownBank();

#endif	//__BANK

};

#endif	//__TVCHANNELMANAGER_H__

