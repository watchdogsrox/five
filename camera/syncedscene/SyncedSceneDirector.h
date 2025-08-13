//
// camera/cinematic/SyncedSceneDirector.h
// 
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
//

#ifndef SYNCED_SCENE_DIRECTOR_H
#define SYNCED_SCENE_DIRECTOR_H

#include "camera/base/BaseDirector.h"
#include "crclip/clip.h"
#include "streaming/streamingmodule.h"


class camSyncedSceneDirectorMetadata;

class camSyncedSceneDirector : public camBaseDirector
{
	DECLARE_RTTI_DERIVED_CLASS(camSyncedSceneDirector, camBaseDirector);

public:
	//This a list based on priority please speak to the camera programmers before adding a new client. These are unique per use.
	//Highest priority client is 0. 
	enum SyncedSceneAnimatedCameraClients
	{
		ANIM_PLACEMENT_TOOL_CLIENT = 0,
		TASK_ARREST_CLIENT,
		TASK_SHARK_ATTACK_CLIENT,
		NETWORK_SYNCHRONISED_SCENES_CLIENT,
		MAX_CLIENTS		//make sure this is at the end of ennum list
	};


	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor
	
	void StopAnimatingCamera(SyncedSceneAnimatedCameraClients Client, u32 interpolationDuration = 0); 
	
	bool AnimateCamera(const strStreamingObjectName animDictName, const crClip& clip, const Matrix34& sceneOrigin, int sceneId, u32 iFlags, SyncedSceneAnimatedCameraClients Client);

#if __BANK
	void DebugStopRender(SyncedSceneAnimatedCameraClients Client); 
	void DebugStartRender(SyncedSceneAnimatedCameraClients Client); 
#endif
	
private:
	camSyncedSceneDirector(const camBaseObjectMetadata& metadata);
	
	const camSyncedSceneDirectorMetadata& m_Metadata;

public:
	~camSyncedSceneDirector();

protected:
	virtual bool Update(); 
	
	//void UpdateClients(); 
private:
	void CreateCamera(); 

private:
	s32 m_activeClient;
	bool m_StartRenderingNextUpdate;

private:
	camSyncedSceneDirector(const camSyncedSceneDirector& other);
	camSyncedSceneDirector& operator=(const camSyncedSceneDirector& other);

};

#endif