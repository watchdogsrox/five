//
// purpose: Class for storing video metadata in profile stats.
//			There is no way to relate metadata stored in replay projects with the final video. This class is for storing the last so many videos and associating metadata with them
//
#include "control/replay/replay.h"

#ifndef REPLAY_METADATA_H_
#define REPLAY_METADATA_H_

#if GTA_REPLAY

class CReplayVideoMetadataStore
{
public:
	struct VideoMetadata
	{
		u32 score;
		u32 trackId;
	};

	CReplayVideoMetadataStore() {}

	static CReplayVideoMetadataStore& GetInstance() {return sm_Instance;}

	void RegisterVideo(const char* pFilename, const VideoMetadata& metadata);
	bool GetMetadata(const char* pFilename, VideoMetadata& metadata);

private:
	void GetMetadata(int index, u32 &filenameHash, VideoMetadata& metadata);
	void SetMetadata(int index, u32 filenameHash, const VideoMetadata& metadata);

	static CReplayVideoMetadataStore sm_Instance;
};

#endif //GTA_REPLAY

#endif //REPLAY_MONTAGE_GENERATOR_H_
