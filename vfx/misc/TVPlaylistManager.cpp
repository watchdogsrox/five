#include "fwsys/gameskeleton.h"

#include "game/Clock.h"
#include "math/random.h"

#include "TVPlaylistManager.h"
#include "TVPlaylistManager_parser.h"

#include "Vfx/Misc/MovieManager.h"

#include "Network/Live/NetworkTelemetry.h"

#if GTA_REPLAY
#include "control/replay/replay.h"
#endif
#include "renderer/RenderTargetMgr.h"
#include "cutscene/CutSceneManagerNew.h"

#define		NEXT_MOVIE_PRE_CACHE_TIME	(2.0f)

VFX_MISC_OPTIMISATIONS()

// DON'T COMMIT
//OPTIMISATIONS_OFF()

////////////////////////////////////////////////////////////////////////////////////

int CTVVideoInfo::SortByTimesPlayedFunc(CTVVideoInfo* const*a, CTVVideoInfo* const*b)
{
	return (*a)->m_iNumTimesPlayed - (*b)->m_iNumTimesPlayed;
}

// We only have one filter, which is watershed (for now)
bool	CTVVideoInfo::PassesFilter()
{
	if( m_bNotOnDisk )
	{
		return false;
	}
	return true;
}

////////////////////////////////////////////////////////////////////////////////////

CTVVideoInfo *CTVPlayListSlot::GetVideoForThisSlot()
{
	// An array to hold our list of video's that pass the filter
	atArray < CTVVideoInfo * > pPassedVideoArray;

	// Get all the video's that pass our filter for this time
	for(int i=0;i<m_TVVideoInfoNames.size();i++)
	{
		CTVVideoInfo *pVideoInfo = CTVPlaylistManager::GetVideo( m_TVVideoInfoNames[i] );
		if( Verifyf(pVideoInfo!=NULL, "Unable find video %s referenced in playlist slot %s", m_TVVideoInfoNames[i].GetCStr(), m_Name.GetCStr() ) )
		{
			if( pVideoInfo->PassesFilter() )
			{
				pPassedVideoArray.PushAndGrow(pVideoInfo);
			}
		}
	}

	if (pPassedVideoArray.GetCount() <= 0)
	{
		AssertMsg(0, "Failed to load video");
		return NULL;
	}

	// Now sort our created list by the number of times played
	pPassedVideoArray.QSort(0,-1,CTVVideoInfo::SortByTimesPlayedFunc);

	// Return the lowest.
	return pPassedVideoArray[0];
}

////////////////////////////////////////////////////////////////////////////////////

void	CTVPlaylistContainer::Load()
{
	INIT_PARSER;
	PARSER.LoadObject("common:/data/TVPlaylists", "xml", *this);
}

#if	__BANK
void	CTVPlaylistContainer::Save()
{
	INIT_PARSER;
	PARSER.SaveObject("common:/data/TVPlaylists", "xml", this, parManager::XML);
}
#endif	//__BANK

void	CTVPlaylistContainer::Reset()
{
	m_Playlists.Reset();
	m_PlayListSlots.Reset();
	m_Playlists.Reset();
}

CTVVideoInfo *CTVPlaylistContainer::GetVideo( atHashString name )
{
	for( s32 i=0;i<m_Videos.size();i++ )
	{
		if( m_Videos[i].m_Name.GetHash() == name.GetHash() )
		{
			return &m_Videos[i];
		}
	}
	return NULL;
}

CTVPlayListSlot	*CTVPlaylistContainer::GetPlayListSlot( atHashString name )
{
	for( s32 i=0;i<m_PlayListSlots.size();i++ )
	{
		if( m_PlayListSlots[i].m_Name.GetHash() == name.GetHash() )
		{
			return &m_PlayListSlots[i];
		}
	}
	return NULL;
}

CTVPlayList	*CTVPlaylistContainer::GetPlayList( atHashString name )
{
	for( s32 i=0;i<m_Playlists.size();i++ )
	{
		if( m_Playlists[i].m_Name.GetHash() == name.GetHash() )
		{
			return &m_Playlists[i];
		}
	}
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////////

void	CTVChannelInfo::SetPlaylist(const char *pPlaylistName, bool bStartFromNow)
{
	CTVPlayList *pPlayList = CTVPlaylistManager::GetPlayList(atHashString(pPlaylistName));
	Assertf(pPlayList, "Playlist %s - Doesn't exist!", pPlaylistName);
	m_pPlayList = pPlayList;

	if( pPlayList )
	{
		if( bStartFromNow )
		{
			SetStartFromNow();
		}
		else
		{
			// Not starting from now, no offset
			u32 curTimeInSeconds = CClock::GetSecondsSince(2000);

			// DEBUG
			//Displayf("TVPLAYLISTS: StartTime = %d:%d:%d on Channel %d", CClock::GetHour(), CClock::GetMinute(), CClock::GetSecond(), m_id );
			//Displayf("TVPLAYLISTS: Seconds Since 2000 at start of playlist == %d", curTimeInSeconds );
			// DEBUG

			m_PlayTime = curTimeInSeconds;
			m_PlayOffset = 0;
		}
		SetDirty(true);
	}
}

void	CTVChannelInfo::SetPlaylistFromHour(const char *pPlaylistName, u32 hour)
{
	CTVPlayList *pPlayList = CTVPlaylistManager::GetPlayList(atHashString(pPlaylistName));
	Assertf(pPlayList, "Playlist %s - Doesn't exist!", pPlaylistName);
	m_pPlayList = pPlayList;

	if( pPlayList )
	{
		SetStartFromHourOfRealDay(hour);
		SetDirty(true);
	}
}


void	CTVChannelInfo::ClearPlaylist()
{
	m_PlayTime = 0;
	m_PlayOffset = 0;
	m_pPlayList = NULL;
}

void	CTVChannelInfo::SetStartFromNow()
{
	if(m_pPlayList)
	{
		m_PlayTime = CClock::GetSecondsSince(2000);
		// Set the offset to be the negative time from midnight to now
		int	timeOfDayInSeconds = GetTimeOfDayInSecsSafe();

		m_PlayOffset = ((m_PlayTime/timeOfDayInSeconds)*timeOfDayInSeconds) - m_PlayTime;
	}
}


void	CTVChannelInfo::SetStartFromHourOfRealDay( u32 hour )
{
	if(m_pPlayList)
	{
		m_PlayTime = CClock::GetSecondsSince(2000);

		// Calculate an offset that equates to the correct hour
		int secondsToHour = SECONDS_IN_HOUR * hour;
		secondsToHour -= m_PlayTime % SECONDS_IN_DAY;	// Make offset from now

		// Set the offset to be the negative time from midnight to our desired hour
		int	timeOfDayInSeconds = GetTimeOfDayInSecsSafe();

		m_PlayOffset = ((m_PlayTime/timeOfDayInSeconds)*timeOfDayInSeconds) - m_PlayTime;
		m_PlayOffset -= secondsToHour;
	}
}

// Update the time stored for this channel
void	CTVChannelInfo::UpdateTime()
{
	u32 curTimeInSeconds = CClock::GetSecondsSince(2000);

	// DEBUG
//	Displayf("TVPLAYLISTS: UpdateTime = %d:%d:%d on channel %d", CClock::GetHour(), CClock::GetMinute(), CClock::GetSecond(), m_id );
//	Displayf("TVPLAYLISTS: Seconds Since 2000 during update == %d", curTimeInSeconds );
	// DEBUG


	m_PlayTime = curTimeInSeconds;
}

////////////////////////////////////////////////////////////////////////////////////

CTVPlaylistContainer	CTVPlaylistManager::m_PlayListContainer;

s32						CTVPlaylistManager::m_TVDesiredChannel;
s32						CTVPlaylistManager::m_TVCurrentChannel;

CTVChannelInfo			CTVPlaylistManager::m_Channels[ TV_CHANNEL_ID_MAX ];

#if __BANK
CTVVideoInfo			*CTVPlaylistManager::m_pPrevVideo;
float					CTVPlaylistManager::m_PrevVideoTime;
#endif

CTVVideoInfo			*CTVPlaylistManager::m_pCurrentVideo;
float					CTVPlaylistManager::m_CurrentVideoTime;
CTVVideoInfo			*CTVPlaylistManager::m_pNextVideo;
float					CTVPlaylistManager::m_NextVideoTime;

u32						CTVPlaylistManager::m_LastTODStart;
int						CTVPlaylistManager::m_LastSlotID;

bool					CTVPlaylistManager::m_bWaitForKeyframeSyncOnSwitch = false;
float					CTVPlaylistManager::m_TimeToWaitFor = -1.0f;	// No key

float					CTVPlaylistManager::m_TVVolume;

RegdEnt					CTVPlaylistManager::m_EntityForAudio;
bool					CTVPlaylistManager::m_IsAudioFrontend;

// Two possible shows cached at any one time
CTVPlaylistManager::CachedTVShow	CTVPlaylistManager::m_TVShowCache[MAX_TV_SHOW_CACHE];
// Pointer to the current cache used for playing
CTVPlaylistManager::CachedTVShow	*CTVPlaylistManager::m_pCurrentCachePlaying;

CTVPlaylistManager::CTVPlaylistManager()
{
}

// Load any par files
void	CTVPlaylistManager::Init(unsigned initMode)
{

	if (initMode==INIT_CORE)
	{
		// Set the channel ID's in each channel
		for(int i=0;i<TV_CHANNEL_ID_MAX;i++)
		{
			m_Channels[i].SetChannelId(i);
		}

#if	0			// TEST, creates a debug playlist if set to 1
#if	__DEV
		// Something just to get an XML file
		CreateTestData();
		Save();
#endif // __DEV
#else	//0

		// Load the pargen'd XML
		Load();

#endif	//0

		// Setup anything required after the load
		PostLoadProcess();

#if __BANK

		InitBank();

#endif	//__BANK
	}

	// TV Channel
	m_LastSlotID = -1;
	m_LastTODStart = 0xffffffff;

	m_TVVolume = 0.0f;
	StopTV();
	m_TVCurrentChannel = m_TVDesiredChannel	= TV_CHANNEL_ID_NONE;
	m_EntityForAudio = NULL;
	m_IsAudioFrontend = true;
}

void	CTVPlaylistManager::Shutdown(unsigned shutdownMode)
{
	if (shutdownMode==SHUTDOWN_WITH_MAP_LOADED || shutdownMode == SHUTDOWN_SESSION)
	{
		// Stop all TV channels playing
		StopTV();
	}
	if (shutdownMode==SHUTDOWN_CORE)
	{
		m_PlayListContainer.Reset();
	}
}

void	CTVPlaylistManager::Update()
{
#if GTA_REPLAY
	if( CReplayMgr::IsEditModeActive() )
	{
		// If we're playing back a clip, then stop the TV playlist manager
		m_TVDesiredChannel = TV_CHANNEL_ID_NONE;
		m_TVCurrentChannel = TV_CHANNEL_ID_NONE;
		StopTV();
		return;
	}
#endif	//GTA_REPLAY

	UpdateChannelScript();
	UpdateTVPlayback();
}

void CTVPlaylistManager::Load()
{
	m_PlayListContainer.Load();
}

// Setup anything required after the load
void	CTVPlaylistManager::PostLoadProcess()
{
#if __ASSERT

	// If this is an assert build, check the times for the movies are correct with the data as it stands.
	// Replace if needs be and warn to re-save. Could do that automatically, but the file needs checking out of P4.

	// Pre check all video filenames and get data such as duration, or whether they're not on the disc at all.
	for(int i=0;i<m_PlayListContainer.m_Videos.size();i++)
	{
		CTVVideoInfo *pVideo = &m_PlayListContainer.m_Videos[i];

		char movieName[80];
		strcpy(movieName,pVideo->m_VideoFileName.c_str());
		//strcat(movieName,".bik");

		// Get the movie duration in seconds (in real seconds)
		pVideo->m_fRealDuration = g_movieMgr.GetMovieDurationQuick(movieName);
		if( pVideo->m_fRealDuration > 0.0f )
		{
			pVideo->m_bNotOnDisk = false;

			// Scale the duration into gametime
			float duration = ScaleToGameTime(pVideo->m_fRealDuration);

			Assertf(duration == pVideo->m_fDuration, "TVPlaylists:: Movie time for Movie %s is incorrect, updating. Please re-save the playlist!",pVideo->m_VideoFileName.c_str());

			pVideo->m_fDuration = duration;
		}
		else
		{
			pVideo->m_fDuration = 0.0f;
			pVideo->m_bNotOnDisk = true;

			Assertf(pVideo->m_bNotOnDisk == false, "TVPlaylists:: Movie %s doesn't exist!", pVideo->m_VideoFileName.c_str());
		}
	}

#endif

	// Post Process each playlist to calculate it's total iteration duration
	//	(this is the total time for every combination of show/advert when looped)
	for(int i=0;i<m_PlayListContainer.m_Playlists.size();i++)
	{
		float totalDuration = GetPlayListTimeTotalIterationTime(m_PlayListContainer.m_Playlists[i]);
		//Assertf(totalDuration == m_PlayListContainer.m_Playlists[i].m_fDurationOfAllIterations, "Total Playlist time incorrect for playlist %s, updating. Please re-save the playlist!", m_PlayListContainer.m_Playlists[i].m_Name.GetCStr() );
		m_PlayListContainer.m_Playlists[i].m_fDurationOfAllIterations = totalDuration;
	}

}

float	CTVPlaylistManager::ScaleToGameTime(float realSeconds)
{
	float msPerMinute = static_cast<float>(CClock::GetMsPerGameMinute());
	float minutes = (realSeconds * 1000.0f) / msPerMinute;
	return (minutes * 60.0f);
}

float	CTVPlaylistManager::ScaleToRealTime(float gameSeconds)
{
	float msPerMinute = static_cast<float>(CClock::GetMsPerGameMinute());
	gameSeconds /= 60.0f;
	float seconds = (gameSeconds * msPerMinute) * 0.001f;	//	/ 1000.0f;
	return seconds;
}

CTVVideoInfo *CTVPlaylistManager::GetVideo( atHashString name )
{
	return m_PlayListContainer.GetVideo(name);
}

CTVPlayListSlot	*CTVPlaylistManager::GetPlayListSlot( atHashString name )
{
	return m_PlayListContainer.GetPlayListSlot(name);
}

CTVPlayList	*CTVPlaylistManager::GetPlayList( atHashString name )
{
	return m_PlayListContainer.GetPlayList(name);
}

bool CTVPlaylistManager::HaveAllVideosBeenProcessed(CTVPlayList &playlist)
{
	// See if this we've exhausted all possible play combinations
	for(int i=0;i<playlist.m_TVPlayListSlotNames.size();i++)
	{
		CTVPlayListSlot *pSlot = GetPlayListSlot( playlist.m_TVPlayListSlotNames[i] );
		for(int j=0;j<pSlot->m_TVVideoInfoNames.size();j++)
		{
			CTVVideoInfo *pVideoInfo = CTVPlaylistManager::GetVideo( pSlot->m_TVVideoInfoNames[j] );
			if(pVideoInfo->m_iNumTimesPlayed == 0)
			{
				return false;
			}
		}
	}
	return true;
}

float	CTVPlaylistManager::GetPlayListTimeTotalIterationTime(CTVPlayList &playlist)
{
	// Reset the play counts of all videos
	for(int i=0;i<m_PlayListContainer.m_Videos.size();i++)
	{
		m_PlayListContainer.m_Videos[i].m_iNumTimesPlayed = 0;
	}

	u32		playListSlotIDX = 0;
	float	timeSinceDayStart = 0;
	bool	everyVideoPlayed = false;
	atMap<CTVVideoInfo*,u32>	m_DupesCheckMap;	// Used to check for dupes across slots, as this would be bad.

	while( !everyVideoPlayed )
	{
		CTVPlayListSlot *pSlot = GetPlayListSlot( playlist.m_TVPlayListSlotNames[playListSlotIDX] );

		Assertf(pSlot, "Unable to find PlayListSlot %s", playlist.m_TVPlayListSlotNames[playListSlotIDX].GetCStr() );

		float	videoDuration = 0.0f;
		CTVVideoInfo *pVideoToPlay = pSlot->GetVideoForThisSlot();
		Assertf(pVideoToPlay, "CTVPlaylistManager - No Video To Play");
		if (pVideoToPlay == NULL)
			break;

		// Up it's play count
		pVideoToPlay->m_iNumTimesPlayed++;

		//Assertf( !m_DupesCheckMap.Access(pVideoToPlay), "CTVPlaylistManager - Video %s is included twice in playlist", pVideoToPlay->m_VideoFileName.c_str() );
		m_DupesCheckMap.Insert(pVideoToPlay, 1);

		videoDuration = pVideoToPlay->GetDuration();
		timeSinceDayStart += videoDuration;

		if( HaveAllVideosBeenProcessed(playlist))
		{
			everyVideoPlayed = true;
		}

		// Next slot (and wrap)
		playListSlotIDX++;
		if( playListSlotIDX >= playlist.m_TVPlayListSlotNames.size() )
		{
			playListSlotIDX = 0;
			m_DupesCheckMap.Reset();	// Reset the map, we only want dures
		}
	}

	return timeSinceDayStart;
}


// Get which video we should be playing at this time
// Reset all times played to 0.
// Seed randomness on day
// Work serially through each VideoPlayListSlot in the relevant VideoPlayList from mChannels for that channel
// Check VideoPlayListSlot for VideoInfos that match valid conditions
// Return all VideoInfos that have the lowest and equal number of times played
// Roll into that list, choose that video, add 1 to its times played, subtract video time from current time.
// Repeat if time has remainder, else return video info + time into.
// Should probably move this into ChannelInfo
bool CTVPlaylistManager::GetVideoInfo( int channelID, int &slotID )
{
	CTVChannelInfo *pChannel = &m_Channels[channelID];

	// Check playlistID
	CTVPlayList *pPlayList = pChannel->GetPlayList();

#if __BANK
	m_pPrevVideo = NULL;
#endif

	m_pCurrentVideo = NULL;
	m_pNextVideo = NULL;

	if( !pPlayList )
	{
		return false;
	}

	float timeSinceDayStart = (float)pChannel->GetTimeSinceStartOfDay();

	// Reset the play counts of all videos
	for(int i=0;i<m_PlayListContainer.m_Videos.size();i++)
	{
		m_PlayListContainer.m_Videos[i].m_iNumTimesPlayed = 0;
	}

	// Whizz through
	float	slotTime = 0.0f;
	slotID = 0;
	u32		playListSlotIDX = 0;
	while( slotTime <= timeSinceDayStart )
	{
		CTVPlayListSlot *pSlot = GetPlayListSlot( pPlayList->m_TVPlayListSlotNames[playListSlotIDX] );
		if( Verifyf(pSlot!=NULL, "Unable find videoslot %s referenced in playlist %s", pPlayList[playListSlotIDX].m_Name.GetCStr(), pPlayList->m_Name.GetCStr() ) )
		{
			float	videoDuration = 0.0f;
			CTVVideoInfo *pVideoToPlay = pSlot->GetVideoForThisSlot();

			if( pVideoToPlay )
			{
				// Up it's play count
				pVideoToPlay->m_iNumTimesPlayed++;

				videoDuration = pVideoToPlay->GetDuration();

				// See if this video is currently the one that should be playing
				if( slotTime + videoDuration > timeSinceDayStart )
				{
					// YES!
					m_pCurrentVideo = pVideoToPlay;
					// Calculate the time into the video
					m_CurrentVideoTime = (timeSinceDayStart - slotTime);

					// Calculate the next video and time

					// add on time to the current slotTime
					slotTime += videoDuration;
					// Next slot (and wrap)
					playListSlotIDX++;
					if( playListSlotIDX >= pPlayList->m_TVPlayListSlotNames.size() )
					{
						playListSlotIDX = 0;
					}
					pSlot = GetPlayListSlot( pPlayList->m_TVPlayListSlotNames[playListSlotIDX] );
					m_pNextVideo = pSlot->GetVideoForThisSlot();
					m_NextVideoTime = slotTime - timeSinceDayStart;


					return true;
				}

#if __BANK
				// Keep a track of the previous video to play
				m_pPrevVideo = pVideoToPlay;
				m_PrevVideoTime = pVideoToPlay->GetDuration();
#endif
			}

			// add on time to the current slotTime
			slotTime += videoDuration;
			slotID++;
		}

		// Next slot (and wrap)
		playListSlotIDX++;
		if( playListSlotIDX >= pPlayList->m_TVPlayListSlotNames.size() )
		{
			playListSlotIDX = 0;
		}
	}
	return false;
}

void CTVPlaylistManager::SetTVChannel(s32 channel)
{
	// Check that the channel exists?
	m_TVDesiredChannel = channel;
	if(m_TVDesiredChannel >= 0)
	{
		Assertf( (m_TVDesiredChannel < TV_CHANNEL_ID_MAX) , "Trying to set a TV channel that doesn't exist" );
		m_Channels[m_TVDesiredChannel].SetDirty(true);
		m_TimeToWaitFor = -1.0f;
	}
}

void CTVPlaylistManager::SetTVChannelPlaylist(s32 channel, const char *pPlaylistName, bool startFromNow)
{
	m_Channels[channel].SetPlaylist( pPlaylistName, startFromNow );
}

void CTVPlaylistManager::SetTVChannelPlaylistFromHour(s32 channel, const char *pPlaylistName, int hour)
{
	m_Channels[channel].SetPlaylistFromHour( pPlaylistName, hour );
}

void CTVPlaylistManager::ClearTVChannelPlaylist(s32 channel)
{
	m_Channels[channel].ClearPlaylist();
}

bool CTVPlaylistManager::IsPlaylistOnChannel(s32 channel, u32 nameHash)
{
	if(channel >= 0)
	{
		Assertf( (channel < TV_CHANNEL_ID_MAX) , "CTVPlaylistManager::IsPlaylistOnChannel(): Trying to query a TV channel that doesn't exist" );

		CTVChannelInfo *pChannel = &m_Channels[channel];
		CTVPlayList *pPlayList = pChannel->GetPlayList();
		if( pPlayList )
		{
			if( pPlayList->m_Name.GetHash() == nameHash )
			{
				return true;
			}
		}
	}
	return false;
}

bool CTVPlaylistManager::IsTVShowCurrentlyPlaying(u32 nameHash)
{
	CTVVideoInfo *pVideo = GetCurrentlyPlayingVideo();
	if(pVideo)
	{
		if(pVideo->m_Name.GetHash() == nameHash)
		{
			return true;
		}
	}
	return false;
}

// Returns cache IDX or -1
int CTVPlaylistManager::FindEmptyTVShowCacheSlot()
{
	for(int i=0;i<MAX_TV_SHOW_CACHE;i++)
	{
		if( m_TVShowCache[i].m_TVShowHandle == INVALID_MOVIE_HANDLE)
		{
			return i;
		}
	}
	return -1;
}

// Returns cache IDX or -1
int CTVPlaylistManager::FindTVShowInCache(CTVVideoInfo *pShow)
{
	for(int i=0;i<MAX_TV_SHOW_CACHE;i++)
	{
		if( m_TVShowCache[i].m_pTVShow == pShow )
		{
			return i;
		}
	}
	return -1;
}

// Cache a TV Show
int CTVPlaylistManager::CacheTVShow(CTVVideoInfo *pShow)
{
	int	cacheIDX = FindTVShowInCache(pShow);
	if( cacheIDX == -1)
	{
		cacheIDX = FindEmptyTVShowCacheSlot();		// If this fails we're fucked, but it should never fail as there can only ever be 4 Shows at once (the current channel, the next channel)
		if( cacheIDX >= 0)
		{
			// It's not cached and there is a free slot
			CachedTVShow *pCache = &m_TVShowCache[cacheIDX];
			if( pCache->Cache(&g_movieMgr, pShow) == INVALID_MOVIE_HANDLE )
			{
				cacheIDX = -1;
			}
		}
	}
	return cacheIDX;
}

// Just stops everything
void CTVPlaylistManager::StopTV()
{
	for(int i=0;i<MAX_TV_SHOW_CACHE;i++)
	{
		m_TVShowCache[i].Release(&g_movieMgr);
	}
	m_pCurrentCachePlaying = NULL;
	m_TimeToWaitFor = -1.0f;
}

void CTVPlaylistManager::SetTVVolume(float volume)
{
	m_TVVolume = volume;
	if(GetCurrentlyPlayingVideo())
	{
		g_movieMgr.SetVolume(m_pCurrentCachePlaying->m_TVShowHandle, m_TVVolume);
	}
}

void CTVPlaylistManager::UpdateChannelScript()
{
	// Is the channel currently playing, or any channel?
	if( m_TVCurrentChannel != TV_CHANNEL_ID_NONE &&
		m_TVCurrentChannel != TV_CHANNEL_ID_SCRIPT )
	{
		// No
		CTVVideoInfo	*pCurrentShow = NULL;
		CTVChannelInfo	*pChannel = &m_Channels[TV_CHANNEL_ID_SCRIPT];

		// Do we have a playlist?
		if( pChannel->GetPlayList() )
		{
			// If we have a play list on the script channel, reset it's start time to be now each frame
			pChannel->SetStartFromNow();

			// Get the current show
			int	slotID;
			GetVideoInfo( TV_CHANNEL_ID_SCRIPT, slotID );
			pCurrentShow = GetCurrentTVShow();

			if( pCurrentShow )
			{
				// Cache the show, if it's not already cached
				int idx = FindTVShowInCache(pCurrentShow);
				if( idx == -1)
				{
					idx = CacheTVShow(pCurrentShow);
					if(idx != -1)
					{
						// Set a "FORCED BY SCRIPT FLAG"?, to prevent cleanup
						m_TVShowCache[idx].SetForced(true);
					}
				}
			}
		}
		else
		{
			// Remove any forced flags that might be set from the script channel
			// So those shows will be cleaned up as part of the normal cleanup.
			for(int i=0;i<MAX_TV_SHOW_CACHE;i++)
			{
				m_TVShowCache[i].SetForced(false);
			}
		}
	}
}

// Returns the time difference between the time we wanted, and the keyframe found.
// NOTE: Time passed is in gametime, comvert to realtime
float CTVPlaylistManager::SetToNearestKeyFrame(u32 handle, float time)
{
	float realTime = ScaleToRealTime(time);
	// If the time is really small, assume beginning
	if( realTime < 1.0f )
	{
		realTime = 0.0f;
	}
	float deltaTime = g_movieMgr.SetTimeReal(handle, realTime);
	return ScaleToGameTime(deltaTime);
}

u32 CTVPlaylistManager::GetCurrentShowNameHash()
{
	CTVVideoInfo	*pCurrentShow = NULL;

	if( m_TVCurrentChannel  != TV_CHANNEL_ID_NONE )
	{
		pCurrentShow = GetCurrentTVShow();

		if (pCurrentShow)
		{
			return(pCurrentShow->m_Name.GetHash());
		}
	}

	return(0);
}

void CTVPlaylistManager::UpdateTVPlayback()
{
	m_TVCurrentChannel = m_TVDesiredChannel;

	if( CClock::WasSetFromScriptThisFrame() )
	{
		return;
	}

	CTVVideoInfo	*pCurrentShow = NULL;
	CTVVideoInfo	*pNextShow = NULL;
	CTVChannelInfo	*pChannel = NULL;
	int				slotID = -1;

	if( m_TVCurrentChannel  != TV_CHANNEL_ID_NONE )
	{
		// Get what video's should be playing on this channel
		GetVideoInfo( m_TVDesiredChannel, slotID );

		pCurrentShow = GetCurrentTVShow();
		pNextShow = GetNextTVShow();

		pChannel = &m_Channels[m_TVCurrentChannel];
	}

#if __BANK
	if (ms_bShowClipName && (pCurrentShow != NULL))
	{
		grcDebugDraw::TextFontPush(grcSetup::GetProportionalFont());

		atString fileName = pCurrentShow->m_VideoFileName;
		const atVarString temp("current clip name: %s (%s) (duration: %f)(hash: %d)", pCurrentShow->m_Name.TryGetCStr(), (fileName.GetLength() ? fileName.c_str() : ""), pCurrentShow->GetDuration(), GetCurrentShowNameHash());

		grcDebugDraw::Text(Vector2(270.0f, 270.0f), DD_ePCS_Pixels, CRGBA_White(), temp, true, 1.0f, 1.0f);

		grcDebugDraw::TextFontPop();
	}
#endif //__BANK

	UpdateTelemetry(m_TVCurrentChannel, pCurrentShow);

	CTVVideoInfo	*pCurrentlyPlaying = GetCurrentlyPlayingVideo();

	u32	startTOD = 0xffffffff;
	if( pChannel && pChannel->GetPlayList() )
	{
			startTOD = pChannel->GetStartOfCurrentDayTime();
	}

	// B*1552555, if the clock thinks it's paused, set looping to true.
	if( m_pCurrentCachePlaying )
	{
		static bool wasPaused = false;
		if( CClock::GetIsPaused() )
		{
			if( !wasPaused )
			{
				g_movieMgr.SetLooping(m_pCurrentCachePlaying->m_TVShowHandle, true);
				wasPaused = true;
			}
		}
		else
		{
			if( wasPaused )
			{
				g_movieMgr.SetLooping(m_pCurrentCachePlaying->m_TVShowHandle, false);
				wasPaused = false;
			}
		}
	}

	if( pCurrentlyPlaying != pCurrentShow || (pCurrentShow && (m_LastSlotID != slotID || m_LastTODStart != startTOD ) ) )
	{
		m_LastTODStart = startTOD;
		m_LastSlotID = slotID;

		// We're playing, remove any shows that aren't currently playing
		// We don't need to check for intentionally queued since by the time we get here, the show must have changed and the pending movie is now current
		// NOTE: at this time, we're still playing the previous show until we stop later on in this func.
		for(int i=0;i<MAX_TV_SHOW_CACHE;i++)
		{
			CachedTVShow *pCachedShow = &m_TVShowCache[i];
			if( pCachedShow->m_TVShowHandle != INVALID_MOVIE_HANDLE )
			{
				if( m_pCurrentCachePlaying != pCachedShow &&
					pCachedShow->m_pTVShow != pCurrentShow &&
					pCachedShow->GetForced() == false )
				{
					pCachedShow->Release(&g_movieMgr);
				}
			}
		}

		if(pCurrentShow != NULL)
		{
			// Next show or changing channels

			// Sort out the current program
			int idx = FindTVShowInCache(pCurrentShow);
			if( idx == -1 )
			{
				// This should only happen on a channel change or first play as subsequent shows will be already cached.
				idx = CacheTVShow(pCurrentShow);
			}

#if __BANK
			DumpCacheContentInfo();
#endif

			if( idx != -1 )
			{
				// Make sure the other thread has PreLoaded the movie
				CMovieMgr::CMovie* movie = g_movieMgr.FindMovie(m_TVShowCache[idx].m_TVShowHandle);
				if( movie && movie->IsLoaded() )
				{

					// Wait until we're approaching a keyframe before allowing swap
					bool	canStart = false;
					if( m_bWaitForKeyframeSyncOnSwitch )
					{
						u32	showHandle = m_TVShowCache[idx].m_TVShowHandle;
						float	time = ScaleToRealTime(GetCurrentTVShowTime());

						// No Key? get one
						if( m_TimeToWaitFor < 0.0f )
						{
							m_TimeToWaitFor = g_movieMgr.GetNextKeyFrame(showHandle, time) / g_movieMgr.GetPlaybackRate(showHandle);
						}

						if( (m_TimeToWaitFor - time) < 0.25f )
						{
							m_TimeToWaitFor = -1.0f;
							canStart = true;
						}
					}
					else
					{
						canStart = true;
					}

					if( canStart )
					{
						// We also know that it can't be dirty or we wouldn't have come in this path
						pChannel->SetDirty(false);

						CachedTVShow *pPreviousShow = m_pCurrentCachePlaying;
						m_pCurrentCachePlaying = &m_TVShowCache[idx];

						bool bSameMovie = (m_pCurrentCachePlaying == pPreviousShow) ? true : false;

							// If this movie was force loaded on the script channel, it's playing now, so no longer forced.
						m_pCurrentCachePlaying->SetForced(false);

						if ( !bSameMovie )
						{
							g_movieMgr.AttachAudioToEntity(m_pCurrentCachePlaying->m_TVShowHandle, m_EntityForAudio);
							g_movieMgr.SetFrontendAudio(m_pCurrentCachePlaying->m_TVShowHandle, m_IsAudioFrontend);
							g_movieMgr.Play(m_pCurrentCachePlaying->m_TVShowHandle);
							g_movieMgr.SetVolume(m_pCurrentCachePlaying->m_TVShowHandle, m_TVVolume);
						}
						// Move the cursor
						if( m_TVCurrentChannel != TV_CHANNEL_ID_SCRIPT )
						{
							// Move to the current position in the show
							//	float percent = ( GetCurrentTVShowTime() / pCurrentShow->GetDuration() ) * 100.0f;
							//	g_movieMgr.SetTime(m_pCurrentCachePlaying->m_TVShowHandle, percent);
							float offset = SetToNearestKeyFrame(m_pCurrentCachePlaying->m_TVShowHandle, GetCurrentTVShowTime());
							pChannel->AdjustPlayOffset((s32)offset);
						}

						if ( !bSameMovie )
						{
							g_movieMgr.SetLooping(m_pCurrentCachePlaying->m_TVShowHandle, false);
							if( pPreviousShow  )
							{
								g_movieMgr.Stop(pPreviousShow->m_TVShowHandle );
							}
						}



					///////////////////////////////////////////////////////////////////////////////////
					////////////	DEBUG
					///////////////////////////////////////////////////////////////////////////////////
		//				static u32	timeSince = 0;
		//				u32	delta = pChannel->GetTimeSinceStartOfDay() - timeSince;
		//				timeSince = pChannel->GetTimeSinceStartOfDay();
		//				float realTime = ScaleToRealTime((float)delta);
		//				Displayf("TVPLAYLISTS:: gametime==%i : realtime == %f",delta, realTime);
					///////////////////////////////////////////////////////////////////////////////////

						// Now we've started the current show, the previous is stopped & no longer required
						// Find all the currently cached shows and remove any that aren't relevant
						for(int i=0;i<MAX_TV_SHOW_CACHE;i++)
						{
							CachedTVShow *pCachedShow = &m_TVShowCache[i];
							if( pCachedShow && pCachedShow->m_TVShowHandle != INVALID_MOVIE_HANDLE && pCachedShow->GetForced() == false )
							{
								// Don't try to release anything unless it's fully loaded (or it'll block until it is!)
								CMovieMgr::CMovie* cachedMovie = g_movieMgr.FindMovie(pCachedShow->m_TVShowHandle);
								if( cachedMovie && cachedMovie->IsLoaded() )
								{
									CTVVideoInfo *pTVShow = pCachedShow->m_pTVShow;

									// Can't throw us away if we are the current show!
									if (pTVShow != pCurrentShow)
									{
										// But if it is not the next show (or it is the next show but it's ourside our PRE_CACHE_TIME - in which case we'll reload it)
										if ( pTVShow != pNextShow || GetNextTVShowTime() > ScaleToGameTime(NEXT_MOVIE_PRE_CACHE_TIME))
										{
											// Throw it away.
											pCachedShow->Release(&g_movieMgr);
										}
									}
								}
							}
						}
					}
				}
			}
		}
		else
		{
			// STOPPING

			// Firstly, stop any currently playing show
			if( m_pCurrentCachePlaying )
			{
				g_movieMgr.Stop(m_pCurrentCachePlaying->m_TVShowHandle);
			}
			m_pCurrentCachePlaying = NULL;

			// Find all the currently cached shows and remove any that aren't relevant
			// TODO: move into cache manager.
			for(int i=0;i<MAX_TV_SHOW_CACHE;i++)
			{
				CachedTVShow *pCachedShow = &m_TVShowCache[i];

				if( pCachedShow->m_TVShowHandle != INVALID_MOVIE_HANDLE )
				{
					pCachedShow->Release(&g_movieMgr);
				}
			}

			m_LastSlotID = -1;
			m_LastTODStart = 0xffffffff;
			m_TimeToWaitFor = -1.0f;
		}
	}
	else if(pCurrentShow != NULL)
	{
		// We may have changed channel, but the new channel has the same show at a different position
		if(pChannel && pChannel->IsDirty() )
		{
			pChannel->SetDirty(false);

			int idx = FindTVShowInCache(pCurrentShow);
			if( idx != -1)
			{
				m_pCurrentCachePlaying = &m_TVShowCache[idx];
				// Move to the current position in the show
				//float percent = ( GetCurrentTVShowTime() / pCurrentShow->GetDuration() ) * 100.0f;
				//g_movieMgr.SetTime(m_pCurrentCachePlaying->m_TVShowHandle, percent);
				float offset = SetToNearestKeyFrame(m_pCurrentCachePlaying->m_TVShowHandle, GetCurrentTVShowTime());
				pChannel->AdjustPlayOffset((s32)offset);
			}
		}
	}

	// Sort out the next program
	// See if we need to think about loading it
	// If the time until we need to play it is < 5 seconds (real time), then cache it.
	if(pNextShow != NULL)
	{
		if( GetNextTVShowTime() < ScaleToGameTime(NEXT_MOVIE_PRE_CACHE_TIME) )
		{
			int idx = FindTVShowInCache(pNextShow);
			if( idx == -1 )
			{
				CacheTVShow(pNextShow);
			}
		}
	}

	// Update Time
	if(pChannel)
	{
		// Update channel time.
		pChannel->UpdateTime();

		if(m_pCurrentCachePlaying && m_pCurrentCachePlaying->m_pTVShow == pCurrentShow)
		{
			if( NetworkInterface::IsGameInProgress() == false )
			{
				CMovieMgr::CMovie* movie = g_movieMgr.FindMovie(m_pCurrentCachePlaying->m_TVShowHandle);
				if( movie && movie->GetMovie() )
				{
					float movieTime = ScaleToGameTime(movie->GetMovie()->GetMovieTimeReal());
					float playlistTime = GetCurrentTVShowTime();
					float timeDelta = movieTime - playlistTime;
					pChannel->AdjustPlayOffset((s32)timeDelta);
				}
			}

#if (__BANK && 0)
			// DEBUG
			{
				CTVVideoInfo *pTVShowInfo = m_pCurrentCachePlaying->m_pTVShow;
				CMovieMgr::CMovie* movie = g_movieMgr.FindMovie(m_pCurrentCachePlaying->m_TVShowHandle);

				float movieTime = 0.0f;

				if( movie->GetMovie() )
				{
					movieTime = movie->GetMovie()->GetMovieTimeReal();
					Displayf("Movie Time == %f = %f (percent) on channel", movieTime, movie->GetMovie()->GetMovieTime(), m_TVCurrentChannel);
				}
				float showTime = ScaleToRealTime(GetCurrentTVShowTime());

				Displayf("Show Time == %f = %f (percent) on channel", showTime, (GetCurrentTVShowTime() / pTVShowInfo->GetDuration()) *100.0f, m_TVCurrentChannel );
				Displayf("Delta time = %f", movieTime - showTime);
				Displayf("Next Show Time == %f", ScaleToRealTime(GetNextTVShowTime()) );
			}
			// DEBUG
#endif	// BANK

		}

	}

	if( m_TVCurrentChannel == TV_CHANNEL_ID_NONE )
	{
		// There's no show playing, there's no show supposed to be playing
		// Make sure the cache is emptied.
		for(int i=0;i<MAX_TV_SHOW_CACHE;i++)
		{
			CachedTVShow *pCachedShow = &m_TVShowCache[i];

			if( pCachedShow->m_TVShowHandle != INVALID_MOVIE_HANDLE )
			{
				pCachedShow->Release(&g_movieMgr);
			}
		}
	}

#if __BANK

	DumpCacheContentInfo();

	// DEBUG
	// Stuff for rendering this to the debug texture on screen (See MovieManager.cpp)
	// A bit convoluted, but needed to ensure doesn't bust other functionality.
	static bool s_bLastRenderOnDebugTexture = false;
	if(m_pCurrentCachePlaying && ms_bRenderOnDebugTexture)
	{
		g_movieMgr.ms_curMovieHandle = m_pCurrentCachePlaying->m_TVShowHandle;
		g_movieMgr.ms_debugRender = true;
		g_movieMgr.ms_movieSize = 50;
	}
	else
	{
		if( s_bLastRenderOnDebugTexture == true && ms_bRenderOnDebugTexture == false  )
		{
			g_movieMgr.ms_curMovieHandle = INVALID_MOVIE_HANDLE;
			g_movieMgr.ms_debugRender = false;
//			g_movieMgr.ms_movieSize = 100;
		}
	}
	s_bLastRenderOnDebugTexture = ms_bRenderOnDebugTexture;
#endif	//__BANK

}

//**************************************************************************
//							Telemetry Stuff
//**************************************************************************

#if __ASSERT
u32				CTVPlaylistManager::m_TelemWatchingFrameNumber = 0;
#endif	//__ASSERT
bool			CTVPlaylistManager::m_TelemPlayerIsWatchingTV = false;		// Note, this is designed to be set every frame by script.
bool			CTVPlaylistManager::m_TelemPlayerWasWatchingTV = false;
s32				CTVPlaylistManager::m_TelemLastTVChannelID = -1;
u32				CTVPlaylistManager::m_TelemLastChangeTime = 0;
atHashString	CTVPlaylistManager::m_TelemLastPlaylistName;
atHashString	CTVPlaylistManager::m_TelemLastProgramName;

void CTVPlaylistManager::SetWatchingTVThisFrame()
{
#if __ASSERT
	// Try to trap this going on and off rapidly
	u32	frameCount = fwTimer::GetFrameCount();
	u32	countDelta = frameCount - m_TelemWatchingFrameNumber;
	Assertf( countDelta <= 1 || countDelta > 10, "SET_TV_PLAYER_WATCHING_THIS_FRAME() is being spammed on/off quickly" );
	m_TelemWatchingFrameNumber = frameCount;
#endif

	m_TelemPlayerIsWatchingTV = true;
}

void CTVPlaylistManager::GetNamesSafe(s32 TvChannelID, CTVVideoInfo *pVideoInfo, atHashString &playlistName, atHashString &videoName)
{
	playlistName.Clear();
	videoName.Clear();
	if(TvChannelID != TV_CHANNEL_ID_NONE)
	{
		// Maybe there are some valid names
		CTVChannelInfo *pChannel = &m_Channels[TvChannelID];
		CTVPlayList *pPlaylist = pChannel->GetPlayList();
		if( pPlaylist )
		{
			playlistName = pPlaylist->m_Name;
		}

		if( pVideoInfo )
		{
			videoName = pVideoInfo->m_Name;
		}
	}
}

void CTVPlaylistManager::RecordTelemetryStart(s32 TvChannelID, atHashString playlistName, atHashString videoName)
{
	m_TelemLastTVChannelID = TvChannelID;
	m_TelemLastChangeTime = fwTimer::GetTimeInMilliseconds();
	m_TelemLastProgramName = videoName;
	m_TelemLastPlaylistName = playlistName;
}

void CTVPlaylistManager::UpdateTelemetry(s32 TvChannelID, CTVVideoInfo *pVideoInfo)
{
	if( m_TelemPlayerIsWatchingTV )
	{
		atHashString playListName;
		atHashString videoName;
		GetNamesSafe(TvChannelID, pVideoInfo, playListName, videoName);

		// From the TV off state, we're playing no channel
		if(m_TelemLastTVChannelID == TV_CHANNEL_ID_NONE && TvChannelID != TV_CHANNEL_ID_NONE)
		{
			// We weren't playing any channel, but are now. No output required, just record start info.
			RecordTelemetryStart(TvChannelID, playListName, videoName);
		}

		// Check if anything has changed.
		if( m_TelemLastTVChannelID != TvChannelID ||
			m_TelemLastPlaylistName != playListName ||
			m_TelemLastProgramName != videoName )
		{
			u32	thisTime = fwTimer::GetTimeInMilliseconds() - m_TelemLastChangeTime;
			CNetworkTelemetry::AppendMetricTvShow( m_TelemLastPlaylistName.GetHash(), m_TelemLastProgramName.GetHash(), m_TelemLastTVChannelID, thisTime );
			RecordTelemetryStart(TvChannelID, playListName, videoName);
		}

	}
	else if( m_TelemPlayerWasWatchingTV )
	{
		// The player isn't watching, but was
		// Check there was a show registered before we attempt to append it, in case you can sit down and "watch TV" with the TV off, in which case there isn't anything there
		if( m_TelemLastTVChannelID != TV_CHANNEL_ID_NONE )
		{
			u32	thisTime = fwTimer::GetTimeInMilliseconds() - m_TelemLastChangeTime;
			CNetworkTelemetry::AppendMetricTvShow( m_TelemLastPlaylistName.GetHash(), m_TelemLastProgramName.GetHash(), m_TelemLastTVChannelID, thisTime );
			m_TelemLastTVChannelID = TV_CHANNEL_ID_NONE;	// To ensure we start up next time correctly.
		}
	}

	m_TelemPlayerWasWatchingTV = m_TelemPlayerIsWatchingTV;
	m_TelemPlayerIsWatchingTV = false;
}

//**************************************************************************
//							Telemetry Stuff
//**************************************************************************


#if __BANK

void CTVPlaylistManager::DumpCacheContentInfo()
{
	if( ms_bDumpCacheContentInfo )
	{
		// Print out the currently cached shows
		static int showCountMax = 0;
		int	showCount = 0;
		Displayf( "TVPlaylistManager:: Cache Currently contains:-" );
		for(int i=0; i<MAX_TV_SHOW_CACHE; i++)
		{
			CachedTVShow *pShow = &m_TVShowCache[i];
			if( pShow->m_TVShowHandle != INVALID_MOVIE_HANDLE )
			{
				// Show is in cache.
				bool	isCacheLoaded = g_movieMgr.FindMovie(pShow->m_TVShowHandle)->IsLoaded();
				Displayf( "TVPlaylistManager::Slot %i - %s - Loaded == %s", i, pShow->m_pTVShow->m_Name.GetCStr(), isCacheLoaded ? "true" : "false" );
				showCount++;
			}
		}
		Displayf( "TVPlaylistManager:: Total %d shows", showCount );
		if( showCount > showCountMax )
			showCountMax = showCount;
		Displayf( "TVPlaylistManager:: Max shows cached = %d", showCountMax );
	}
}


void	CTVPlaylistManager::Save()
{
	m_PlayListContainer.Save();
}

// Creates some test data
void	CTVPlaylistManager::CreateTestData()
{
	char names[256];

	static const char *tracknames[9] =
	{
		"ad1_pt1",
		"ad1_pt2",
		"ad1_pt3",
		"ad1_pt4",
	};

	// Create 4 video's
	for(int i=0;i<4;i++)
	{
		sprintf(names,"video%.1d",i);
		CTVVideoInfo thisVideo;
		thisVideo.m_Name = atHashString(names);
		thisVideo.m_VideoFileName = atString(tracknames[i]);
		m_PlayListContainer.m_Videos.PushAndGrow(thisVideo);
	}

	// Create some playlistslots
	CTVPlayListSlot thisPlayListSlot;

	// 0
	thisPlayListSlot.m_Name = atHashString("playlistslot0",0x5BB75FD2);
	thisPlayListSlot.m_TVVideoInfoNames.PushAndGrow( atHashString("video0",0xC255E0) );
	thisPlayListSlot.m_TVVideoInfoNames.PushAndGrow( atHashString("video2",0xE56F9F3B) );
	thisPlayListSlot.m_TVVideoInfoNames.PushAndGrow( atHashString("video3",0xE19F08F) );
	thisPlayListSlot.m_TVVideoInfoNames.PushAndGrow( atHashString("video1",0xAA76A946) );
	m_PlayListContainer.m_PlayListSlots.PushAndGrow(thisPlayListSlot);

	// 1
	thisPlayListSlot.m_TVVideoInfoNames.Reset();
	thisPlayListSlot.m_Name = atHashString("playlistslot1",0xD995DB99);
	thisPlayListSlot.m_TVVideoInfoNames.PushAndGrow( atHashString("video2",0xE56F9F3B) );
	thisPlayListSlot.m_TVVideoInfoNames.PushAndGrow( atHashString("video1",0xAA76A946) );
	m_PlayListContainer.m_PlayListSlots.PushAndGrow(thisPlayListSlot);

	// 2
	thisPlayListSlot.m_TVVideoInfoNames.Reset();
	thisPlayListSlot.m_Name = atHashString("playlistslot2",0x6E4C04FB);
	thisPlayListSlot.m_TVVideoInfoNames.PushAndGrow( atHashString("video0",0xC255E0) );
	m_PlayListContainer.m_PlayListSlots.PushAndGrow(thisPlayListSlot);
	thisPlayListSlot.m_TVVideoInfoNames.Reset();

	// Create a playlist
	CTVPlayList thisPlayList;

	// 0
	thisPlayList.m_Name = atHashString("Playlist0",0xCE95B524);
	thisPlayList.m_TVPlayListSlotNames.PushAndGrow( atHashString("playlistslot0",0x5BB75FD2) );
	thisPlayList.m_TVPlayListSlotNames.PushAndGrow( atHashString("playlistslot1",0xD995DB99) );
	thisPlayList.m_TVPlayListSlotNames.PushAndGrow( atHashString("playlistslot2",0x6E4C04FB) );
	m_PlayListContainer.m_Playlists.PushAndGrow(thisPlayList);

	thisPlayList.m_TVPlayListSlotNames.Reset();

	// 1
	thisPlayList.m_Name = atHashString("Playlist1",0x19304A5C);
	thisPlayList.m_TVPlayListSlotNames.PushAndGrow( atHashString("playlistslot2",0x6E4C04FB) );
	thisPlayList.m_TVPlayListSlotNames.PushAndGrow( atHashString("playlistslot0",0x5BB75FD2) );
	thisPlayList.m_TVPlayListSlotNames.PushAndGrow( atHashString("playlistslot1",0xD995DB99) );
	m_PlayListContainer.m_Playlists.PushAndGrow(thisPlayList);

	thisPlayList.m_TVPlayListSlotNames.Reset();
}



bkBank *CTVPlaylistManager::ms_pBank = NULL;
bkButton *CTVPlaylistManager::ms_pCreateButton = NULL;

int		CTVPlaylistManager::ms_ChannelSelection = 0;
int		CTVPlaylistManager::ms_PlaylistSelection = 0;
const char **CTVPlaylistManager::ms_pPlaylistNames = NULL;
bool	CTVPlaylistManager::ms_bRenderOnDebugTexture = false;
u32		CTVPlaylistManager::ms_StartHour = 0;
bool	CTVPlaylistManager::ms_bDumpCacheContentInfo = false;
bool	CTVPlaylistManager::ms_bShowClipName = false;

void CTVPlaylistManager::SetChannelCB()
{
	SetTVChannel( ms_ChannelSelection - 1 );
}

void CTVPlaylistManager::SetPlaylistCB()
{
	int	channel = (ms_ChannelSelection-1);
	if( channel >= 0 )
	{
		m_Channels[channel].SetPlaylist( ms_pPlaylistNames[ms_PlaylistSelection], false );
	}
}

void CTVPlaylistManager::SetPlaylistNowCB()
{
	int	channel = (ms_ChannelSelection-1);
	if( channel >= 0 )
	{
		m_Channels[channel].SetPlaylist( ms_pPlaylistNames[ms_PlaylistSelection], true );
	}
}

void CTVPlaylistManager::SetPlaylistHourCB()
{
	int	channel = (ms_ChannelSelection-1);
	if( channel >= 0 )
	{
		m_Channels[channel].SetPlaylistFromHour( ms_pPlaylistNames[ms_PlaylistSelection], ms_StartHour );
	}
}

void CTVPlaylistManager::ClearPlaylistCB()
{
	int	channel = (ms_ChannelSelection-1);
	if( channel >= 0 )
	{
		m_Channels[channel].ClearPlaylist();
	}
}

void CTVPlaylistManager::PrevShowCB()
{
	if( m_TVCurrentChannel != TV_CHANNEL_ID_NONE )
	{
		int	slotID;
		GetVideoInfo(m_TVCurrentChannel, slotID);
		if(GetPrevTVShow() && GetCurrentTVShow())
		{
			float time = GetPrevTVShowTime();	// The duration of the last show
			float thisTime = GetCurrentTVShowTime();
			m_Channels[m_TVCurrentChannel].AdjustPlayOffset( -(int)(time + thisTime));
			m_Channels[m_TVCurrentChannel].SetDirty(true);
		}
	}
}

void CTVPlaylistManager::NextShowCB()
{
	if( m_TVCurrentChannel != TV_CHANNEL_ID_NONE )
	{
		int	slotID;
		GetVideoInfo(m_TVCurrentChannel, slotID);
		if(GetNextTVShow())
		{
			float time = GetNextTVShowTime();	// Time until the next show
			m_Channels[m_TVCurrentChannel].AdjustPlayOffset((int)time);
			m_Channels[m_TVCurrentChannel].SetDirty(true);
		}
	}
}

void CTVPlaylistManager::DumpTodaysPlaylistToTTYCB()
{
	if( m_TVCurrentChannel != TV_CHANNEL_ID_NONE )
	{
		CTVChannelInfo *pChannel = &m_Channels[m_TVCurrentChannel];
		// Check playlistID
		CTVPlayList *pPlayList = pChannel->GetPlayList();
		if( !pPlayList )
		{
			return;
		}
		float timeSinceDayStart = (float)(pPlayList->m_fDurationOfAllIterations-1);

		// Reset the play counts of all videos
		for(int i=0;i<m_PlayListContainer.m_Videos.size();i++)
		{
			m_PlayListContainer.m_Videos[i].m_iNumTimesPlayed = 0;
		}
		// Whizz through
		float	slotTime = 0.0f;
		u32		playListSlotIDX = 0;
		while( slotTime <= timeSinceDayStart )
		{
			CTVPlayListSlot *pSlot = GetPlayListSlot( pPlayList->m_TVPlayListSlotNames[playListSlotIDX] );
			if( Verifyf(pSlot!=NULL, "Unable find videoslot %s referenced in playlist %s", pPlayList[playListSlotIDX].m_Name.GetCStr(), pPlayList->m_Name.GetCStr() ) )
			{
				float	videoDuration = 0.0f;
				CTVVideoInfo *pVideoToPlay = pSlot->GetVideoForThisSlot();
				if( pVideoToPlay )
				{
					// Up it's play count
					pVideoToPlay->m_iNumTimesPlayed++;

					Displayf("Time %-10f(s) - Video: %s - Filename: %s", slotTime, pVideoToPlay->m_Name.GetCStr(), pVideoToPlay->m_VideoFileName.c_str());

					videoDuration = pVideoToPlay->GetDuration();
					// See if this video is currently the one that should be playing
					if( slotTime + videoDuration >= timeSinceDayStart )
					{
						// YES!
						return;
					}
				}

				// add on time to the current slotTime
				slotTime += videoDuration;
			}

			// Next slot (and wrap)
			playListSlotIDX++;
			if( playListSlotIDX >= pPlayList->m_TVPlayListSlotNames.size() )
			{
				playListSlotIDX = 0;
			}
		}
	}
}

void CTVPlaylistManager::SavePlaylistCB()
{
	CTVPlaylistManager::Save();
}

void CTVPlaylistManager::SetupLester1CB()
{
	SetTVChannel(TV_CHANNEL_ID_NONE);
	m_Channels[TV_CHANNEL_ID_1].ClearPlaylist();
	m_Channels[TV_CHANNEL_ID_2].ClearPlaylist();
	m_Channels[TV_CHANNEL_SPECIAL].ClearPlaylist();
	m_Channels[TV_CHANNEL_ID_SCRIPT].ClearPlaylist();
	m_Channels[TV_CHANNEL_ID_3].ClearPlaylist();

	SetTVChannelPlaylist(TV_CHANNEL_ID_SCRIPT, "PL_SP_INV_EXP", false);
	SetTVChannelPlaylist(TV_CHANNEL_ID_1, "PL_SP_INV", true);
	SetTVChannel(TV_CHANNEL_ID_1);
}


void CTVPlaylistManager::InitBank()
{
	if(ms_pBank)
	{
		ShutdownBank();
	}

	// Create the weapons bank
	ms_pBank = &BANKMGR.CreateBank("TVPlaylists", 0, 0, false);
	if(Verifyf(ms_pBank, "Failed to create TVPlaylists bank"))
	{
		ms_pCreateButton = ms_pBank->AddButton("Create TVPlaylists widgets", &CTVPlaylistManager::CreateBank);
	}
}



void CTVPlaylistManager::CreateBank()
{
	Assertf(ms_pBank, "TVPlaylists bank needs to be created first");
	Assertf(ms_pCreateButton, "TVPlaylists bank needs to be created first");

	ms_pCreateButton->Destroy();
	ms_pCreateButton = NULL;

	bkBank* pBank = ms_pBank;

	pBank->AddSeparator("TV Channels");
	const char *channelNames[] = { "None", "Channel 1", "Channel 2", "Channel Special", "Channel Script" };
	pBank->AddCombo("Current Channel", &ms_ChannelSelection, NELEM(channelNames), channelNames, datCallback(SetChannelCB), "The Current TV Channel");

	pBank->AddSeparator("Playlists");
	// Create the list of playlist names
	int	count = m_PlayListContainer.GetPlaylistCount();
	if( count )
	{
		// Create the entries for the combo
		ms_pPlaylistNames = rage_new const char *[count];
		for(int i=0;i<count;i++)
		{
			ms_pPlaylistNames[i] = m_PlayListContainer.GetPlaylistName(i);
		}
		// Add the combo
		pBank->AddCombo("Current Playlist", &ms_PlaylistSelection, count, ms_pPlaylistNames, NullCB, "Playlists");

		pBank->AddButton("Set Playlist On Selected Channel", SetPlaylistCB);
		pBank->AddButton("Set Playlist On Selected Channel (Start from now)", SetPlaylistNowCB);
		pBank->AddButton("Set Playlist On Selected Channel Starting at Hour", SetPlaylistHourCB);
		pBank->AddSlider("Hour to start", &ms_StartHour, 0, 23, 1);

		pBank->AddButton("Clear Playlist On Selected Channel", ClearPlaylistCB);
	}

	pBank->AddButton("Setup Lester1", SetupLester1CB);

	pBank->AddSeparator("Skip");
	pBank->AddButton("Prev Show", PrevShowCB);
	pBank->AddButton("Next Show", NextShowCB);
	pBank->AddSeparator("Dump");
	pBank->AddButton("Dump Todays Playlist to TTY", DumpTodaysPlaylistToTTYCB);
	pBank->AddSeparator("Save");
	pBank->AddButton("Save Playlist", SavePlaylistCB);

	pBank->AddToggle("Render To Debug Texture", &ms_bRenderOnDebugTexture);
	pBank->AddToggle("Enable Subtitles", &CMovieMgr::CMovie::ms_bSubsEnabled);
	pBank->AddToggle("Enable Force Wait For KF before Switch", &m_bWaitForKeyframeSyncOnSwitch);

	pBank->AddToggle("Dump Cache Content Info", &ms_bDumpCacheContentInfo);

	pBank->AddToggle("Show clip name", &ms_bShowClipName);

	// Needed to get debug render working
	g_movieMgr.InitRenderStateBlocks();
}

void CTVPlaylistManager::ShutdownBank()
{
	if(ms_pBank)
	{
		BANKMGR.DestroyBank(*ms_pBank);
	}
	ms_pBank = NULL;
	ms_pCreateButton = NULL;

	delete []ms_pPlaylistNames;
}

#endif	//__BANK




////////////////////////////////////////////////////////////////////////////////////

void CTVChannelInfo::SetDirty(bool which)
{
#if __BANK
	if(which)
	{
		// For url:bugstar:2079210:
		diagLoggedPrintf("CTVChannelInfo::SetDirty(true) called. Dumping C++ and script stacks...\n");
		sysStack::PrintStackTrace();
		scrThread::PrePrintStackTrace();
	}
#endif
	m_IsDirty = which;
}
