/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : AnimatedLightEntity.cpp
// PURPOSE : Receives events from the cut scene manager and passes them to the 
//			 light objetcs. There is one entity per object. The entity owns
//			 the object and if is deleted deletes the light object. 
// AUTHOR  : Thomas French
// STARTED : 11/9/2009
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef CUTSCENE_LIGHT_ENTITY_H 
#define CUTSCENE_LIGHT_ENTITY_H

//Rage Headers
#include "cutfile/cutfobject.h"
#include "cutscene/cutsentity.h"

//Game Headers
#include "cutscene/CutsceneLightObject.h"

/////////////////////////////////////////////////////////////////////////////////

ANIM_OPTIMISATIONS()

/////////////////////////////////////////////////////////////////////////////////

#if !__NO_OUTPUT
#define cutsceneLightEntityDebugf(fmt,...) if ( ((Channel_cutscene.TtyLevel == DIAG_SEVERITY_DEBUG3) || (Channel_cutscene.FileLevel == DIAG_SEVERITY_DEBUG3)) && GetObject() ) { char debugStr[512]; CCutSceneLightEntity::CommonDebugStr(GetObject(), debugStr); cutsceneDebugf1("%s" fmt,debugStr,##__VA_ARGS__); }
#else
#define cutsceneLightEntityDebugf(fmt,...) do {} while(false)
#endif //!__NO_OUTPUT

class CCutSceneLightEntity : public cutsUniqueEntity
{
public:	
	//Create the our game side object.
	CCutSceneLightEntity(const cutfObject* pObject); 
	
	~CCutSceneLightEntity();
	
	//Dispatch events to the game side light
	virtual void DispatchEvent(cutsManager *pManager, const cutfObject* pObject, s32 iEventId, const cutfEventArgs* pEventArgs=NULL, const float fTime = 0.0f, const u32 StickyId = 0);

	//Get a pointer to the game light
	CCutSceneLight* GetCutSceneLight() const { return m_pLight; }

#if __DEV
	virtual void DebugDraw() const; 
#endif

#if !__NO_OUTPUT
	void CommonDebugStr(const cutfObject* pObject, char * debugStr);
#endif //!__NO_OUTPUT

	s32 GetType() const { return CUTSCENE_LIGHT_GAME_ENITITY; } 

private:
	//A pointer to the game side light.
	CCutSceneLight* m_pLight; 
};

#endif
