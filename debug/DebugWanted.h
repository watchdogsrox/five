#ifndef DEBUG_DEBUGWANTED_H 
#define DEBUG_DEBUGWANTED_H 

#include "physics/debugPlayback.h"
#if DR_ENABLED

namespace debugWanted
{
	void UpdateRecording(int iWantedLevel);
	void StartWantedRecording(int iFrameStep);
	void StopWantedRecording();
	void OnDebugDraw3d(const debugPlayback::Frame *pSelectedFrame, const debugPlayback::EventBase *pSelectedEvent, const debugPlayback::EventBase *pHoveredEvent);
}

#endif //DR_ENABLED
#endif //DEBUG_DEBUGWANTED_H
