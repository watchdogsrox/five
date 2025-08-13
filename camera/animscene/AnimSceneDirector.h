//
// camera/animscene/AnimSceneDirector.h
// 
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
//

#ifndef ANIM_SCENE_DIRECTOR_H
#define ANIM_SCENE_DIRECTOR_H

#include "animation/AnimScene/AnimScene.h"
#include "animation/AnimScene/events/AnimScenePlayAnimEvent.h"
#include "camera/base/BaseDirector.h"
#include "crclip/clip.h"
#include "streaming/streamingmodule.h"

class camAnimatedCamera;
class camAnimSceneDirectorMetadata;

class camAnimSceneDirector : public camBaseDirector
{
	DECLARE_RTTI_DERIVED_CLASS(camAnimSceneDirector, camBaseDirector);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor
	
	void StopAnimatingCamera(); 
	
	void BlendOutCameraAnimation(u32 interpolationDuration); 

	//PURPOSE: This ensures that StopAnimatingCamera() is called when the blendout finishes,
	//and so means the user can forget about it after setting both this and the blend out camera animation.
	void SetCleanUpCameraAfterBlendOut(bool val = true);

	bool AnimateCamera(const strStreamingObjectName animDictName, const crClip& clip, const Matrix34& sceneOrigin, float startTime, float currentTime, u32 iFlags, u32 blendInTime);

	camAnimatedCamera* GetCamera(); 

#if __BANK
	void DebugStopRender(); 
	void DebugStartRender(); 
#endif
	
private:
	camAnimSceneDirector(const camBaseObjectMetadata& metadata);
	
	const camAnimSceneDirectorMetadata& m_Metadata;

	bool m_cleanUpCameraAfterBlendout;

public:
	~camAnimSceneDirector();

protected:
	virtual bool Update(); 
	
	//void UpdateClients(); 
private:
	void CreateCamera(); 

private:
	camAnimSceneDirector(const camAnimSceneDirector& other);
	camAnimSceneDirector& operator=(const camAnimSceneDirector& other);

};

#endif