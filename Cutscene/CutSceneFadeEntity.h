/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : CutSceneFadeEntity.h
// PURPOSE : 
// AUTHOR  : Thomas French
// STARTED : 12/8/2009
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef CUTSCENE_FADEENTITY_H 
#define CUTSCENE_FADEENTITY_H 

// Rage Headers 
#include "cutscene/cutsentity.h"
#include "cutscene/cutsmanager.h"
#include "cutfile/cutfeventargs.h"
#include "cutfile/cutfobject.h"
#include "cutscene/CutSceneDefine.h"

// Game Headers

class CCutSceneFadeEntity : public cutsUniqueEntity
{
public:	
	CCutSceneFadeEntity();
	CCutSceneFadeEntity( s32 iObjectId,  const cutfObject* pObject );
	
	~CCutSceneFadeEntity();
	
	virtual void DispatchEvent( cutsManager *pManager, const cutfObject* pObject, s32 iEventId, const cutfEventArgs* pEventArgs=NULL,  const float fTime = 0.0f, const u32 StickyId = 0 );
	
	s32 GetType() const { return CUTSCENE_FADE_GAME_ENITITY; } 

private:
	s32 m_iObjectId;

};

class CCutSceneSubtitleEntity : public cutsUniqueEntity
{
public:
	CCutSceneSubtitleEntity(); 
	CCutSceneSubtitleEntity(const cutfObject* pObject );

	~CCutSceneSubtitleEntity();

	virtual void DispatchEvent( cutsManager *pManager, const cutfObject* pObject, s32 iEventId, const cutfEventArgs* pEventArgs=NULL,  const float fTime = 0.0f, const u32 StickyId = 0 );
	
	s32 GetType() const { return CUTSCENE_SUBTITLE_GAME_ENTITY; }

private:
};


#endif