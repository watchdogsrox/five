#ifndef _ANIM_SCENE_DICTIONARY_H_
#define _ANIM_SCENE_DICTIONARY_H_

// rage includes
#include "atl/binmap.h"
#include "atl/hashstring.h"
#include "parser/macros.h"

// game includes
#include "AnimScene.h"

//
// animation/AnimScene/AnimSceneDictionary.h
//
// CAnimSceneDictionary
// A related grouping of anim scenes.
//

class CAnimSceneDictionary : datBase
{
public:

	CAnimSceneDictionary();

	virtual ~CAnimSceneDictionary();

	// PURPOSE: Returns the given named scene (if it exists)
	CAnimScene* GetScene(CAnimScene::Id sceneId);

	PAR_PARSABLE;

private:

	// the register of entities in the scene
	atBinaryMap<CAnimScene*, CAnimScene::Id> m_scenes;
};

#endif //_ANIM_SCENE_DICTIONARY_H_
