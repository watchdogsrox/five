/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    scene/streamer/SceneStreamer.h
// PURPOSE : based on the scene streamer from GTAIV
// AUTHOR :  John Whyte, Ian Kiigan
// CREATED : 09/10/09
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef _SCENE_STREAMER_SCENESTREAMER_H_
#define _SCENE_STREAMER_SCENESTREAMER_H_

#include "scene/streamer/SceneStreamerBase.h"

class CEntity;
class CRenderPhase;

namespace rage
{
	class bkGroup;
}

class CSceneStreamer: public CSceneStreamerBase
{
public:
	CSceneStreamer();

	void IssueRequests(BucketEntry::BucketEntryArray &entityList, int bucketCutoff);
	virtual bool IsHighPriorityEntity(const CEntity* RESTRICT pEntity, const Vector3& vCamDir, const Vector3& vCamPos) const;

	int GetPrioRequests() const					{ return m_PrioRequests; }

private:
#if __BANK
	void SendFakeSceneRemainder(const BucketEntry::BucketEntryArray &entityList, int firstEntityIndex);
	void SendFakeSceneRequest(const BucketEntry &entry);
#endif // __BANK

public:
	// Number of priority requests made this frame
	int m_PrioRequests;

#if __BANK
	void AddWidgets(bkGroup &bank);

	// Send the entire list of requests to StreamingVisualize, even stuff that wasn't even requested
	static bool sm_FullSVSceneLog;

#endif // __BANK
	static bool	sm_EnableMasterCutoff;
	static bool sm_NewSceneStreamer;
};

#endif // _SCENE_STREAMER_SCENESTREAMER_H_


