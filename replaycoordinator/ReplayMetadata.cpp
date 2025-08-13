//
// purpose: Class for storing video metadata in profile stats.
//			There is no way to relate metadata stored in replay projects with the final video. This class is for storing the last so many videos and associating metadata with them
//
#include "replaycoordinator/ReplayMetadata.h"

#if GTA_REPLAY

#include "string/stringhash.h"
#include "control/replay/replay.h"
#include "control/replay/replay_channel.h"
#include "control/replay/ReplayInternal.h"
#include "Stats/StatsInterface.h"
#include "Stats/StatsMgr.h"
#include "Network/Live/NetworkTelemetry.h"
#include "frontend/VideoEditor/VideoEditorInterface.h"

#define REPLAY_VIDEO_NUMBER_CACHED			10
#define REPLAY_VIDEO_FILENAME_STAT_PREFIX	"SP_REPLAY_VIDEO_NAME_"
#define REPLAY_VIDEO_SCORE_STAT_PREFIX		"SP_REPLAY_VIDEO_SCORE_"
#define REPLAY_VIDEO_FLAGS_STAT_PREFIX		"SP_REPLAY_VIDEO_FLAGS_"

CReplayVideoMetadataStore CReplayVideoMetadataStore::sm_Instance;

void CReplayVideoMetadataStore::RegisterVideo(const char* pFilename, const CReplayVideoMetadataStore::VideoMetadata& metadata)
{
	// temporarily disable replay world update so the profile stats are actually set
	bool isInControlOfWorld = CReplayMgr::IsReplayInControlOfWorld();
	CReplayMgr::SetReplayIsInControlOfWorld(false);

	// Shift all other video metadata up the list
	for(int i=REPLAY_VIDEO_NUMBER_CACHED-1; i>0; i--)
	{
		u32 filenameHash;
		VideoMetadata metadata;

		GetMetadata(i-1, filenameHash, metadata);
		SetMetadata(i, filenameHash, metadata);
	}

	replayDisplayf("Registering video %s with hash %u and score %u", pFilename, (u32)atStringHash(pFilename), metadata.score);
	// Set slot 0 with new data
	SetMetadata(0, atStringHash(pFilename), metadata);

	if(NetworkInterface::IsLocalPlayerOnline())
	{
		//Flush the new values
		CLiveManager::GetProfileStatsMgr().Flush(true, false);

		if(Verifyf(CVideoEditorInterface::GetActiveProject(), "No active project"))
		{
			s32 clipCount = CVideoEditorInterface::GetActiveProject()->GetClipCount();
			u32 totalTime = (u32)CVideoEditorInterface::GetActiveProject()->GetTotalClipTimeMs();
			int audioHash[CMontage::MAX_MUSIC_OBJECTS];
			int audioHashCount = 0;
			for(audioHashCount=0; audioHashCount < CVideoEditorInterface::GetActiveProject()->GetMusicClipCount(); audioHashCount++)
			{
				audioHash[audioHashCount]= CVideoEditorInterface::GetActiveProject()->GetMusicClip(audioHashCount).getClipSoundHash();
			}
			u32 timeSpent = CVideoEditorInterface::GetActiveProject()->GetTimeSpent();
			CNetworkTelemetry::AppendVideoEditorSaved(clipCount, audioHash, audioHashCount, totalTime, timeSpent);
		}

	}
	
	CReplayMgr::SetReplayIsInControlOfWorld(isInControlOfWorld);
}

bool CReplayVideoMetadataStore::GetMetadata(const char* pFilename, CReplayVideoMetadataStore::VideoMetadata& metadata)
{
	u32 filenameHash = atStringHash(pFilename);
	for(int i=REPLAY_VIDEO_NUMBER_CACHED-1; i>0; i--)
	{
		u32 filenameHash2;
		VideoMetadata metadata2;

		GetMetadata(i-1, filenameHash2, metadata2);
		if(filenameHash2 == filenameHash)
		{
			metadata = metadata2;
			replayDisplayf("Found video %s with hash %u and score %u", pFilename, filenameHash, metadata.score);
			return true;
		}
	}
	return false;
}

void CReplayVideoMetadataStore::GetMetadata(int index, u32 &filenameHash, CReplayVideoMetadataStore::VideoMetadata& metadata)
{
	char statName[32];

	formatf(statName, REPLAY_VIDEO_FILENAME_STAT_PREFIX "%d", index);
	filenameHash = StatsInterface::GetUInt32Stat(StatId(statName));

	formatf(statName, REPLAY_VIDEO_SCORE_STAT_PREFIX "%d", index);
	metadata.score = StatsInterface::GetUInt32Stat(StatId(statName));

	formatf(statName, REPLAY_VIDEO_FLAGS_STAT_PREFIX "%d", index);
	metadata.trackId = StatsInterface::GetUInt32Stat(StatId(statName));
}

void CReplayVideoMetadataStore::SetMetadata(int index, u32 filenameHash, const CReplayVideoMetadataStore::VideoMetadata& metadata)
{
	char statName[32];

	formatf(statName, REPLAY_VIDEO_FILENAME_STAT_PREFIX "%d", index);
	StatsInterface::SetStatData(StatId(statName), filenameHash, STATUPDATEFLAG_DEFAULT_FORCE_CHANGE);

	formatf(statName, REPLAY_VIDEO_SCORE_STAT_PREFIX "%d", index);
	StatsInterface::SetStatData(StatId(statName), metadata.score, STATUPDATEFLAG_DEFAULT_FORCE_CHANGE);

	formatf(statName, REPLAY_VIDEO_FLAGS_STAT_PREFIX "%d", index);
	StatsInterface::SetStatData(StatId(statName), metadata.trackId, STATUPDATEFLAG_DEFAULT_FORCE_CHANGE);
}


#endif //GTA_REPLAY
