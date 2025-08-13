/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : CutSceneAudioEntity.h
// PURPOSE : 
// AUTHOR  : Thomas French
// STARTED : 27/10/09
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef CUTSCENE_SOUNDENTITY_H 
#define CUTSCENE_SOUNDENTITY_H

//Rage Headers
#include "cutscene/cutsaudioentity.h"
#include "cutfile/cutfobject.h"

//Game Headers
#include "cutscene/CutSceneDefine.h"

#if !__NO_OUTPUT
#define cutsceneSoundEntityDebugf(pObject, fmt,...) if ( ((Channel_cutscene.TtyLevel == DIAG_SEVERITY_DEBUG3) || (Channel_cutscene.FileLevel == DIAG_SEVERITY_DEBUG3)) && pObject ) { char debugStr[256]; CCutSceneAudioEntity::CommonDebugStr(pObject, debugStr); cutsceneDebugf1("%s" fmt,debugStr,##__VA_ARGS__); }
#else
#define cutsceneSoundEntityDebugf(pObject, fmt,...) do {} while(false)
#endif //__NO_OUTPUT

class CCutSceneAudioEntity : public cutsUniqueEntity
{
public:	
	CCutSceneAudioEntity (const cutfObject* pObject);
	~CCutSceneAudioEntity ();

	virtual void DispatchEvent( cutsManager *pManager, const cutfObject* pObject, s32 iEventId, const cutfEventArgs* pEventArgs=NULL, const float fTime = 0.0f, const u32 StickyId = 0 );

	s32 GetType() const { return CUTSCENE_SOUND_GAME_ENTITY; } 

	bool IsAudioLoaded() const { return ms_bIsLoaded; }
	
	static bool IsAudioPlaying() { return ms_bIsPlaying; }

	//bool IsAudioRequested() const { return m_bRequested; }

protected:
	// PURPOSE:  Start the audio
	// NOTES:  Calling code must ensure that the asset is prepared (ie Prepare() returns AUD_PREPARED)
	void Play();

	// PURPOSE:  Stop the audio and free resources.
	void Stop(bool bStopAllStreams);
	
	void StopAllCutSceneAudioStreams(bool bWasSkipped = false); 

	void Pause();

	void UnPause();

private:
#if !__NO_OUTPUT
	void CommonDebugStr(const cutfObject* pObject, char * debugStr); 
	//void GetCurrentAudioEntityPlayingBack(const cutsManager* pManager); 
#endif //!__NO_OUTPUT


private:
	static bool ms_bIsLoaded; 
	static bool ms_bIsPlaying; 

#if !__FINAL
	bool m_bWasJogged;
#endif
};

#endif