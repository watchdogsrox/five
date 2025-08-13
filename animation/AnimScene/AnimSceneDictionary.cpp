#include "AnimSceneDictionary.h"
#include "AnimSceneDictionary_parser.h"

//////////////////////////////////////////////////////////////////////////
CAnimSceneDictionary::CAnimSceneDictionary()
{
}

//////////////////////////////////////////////////////////////////////////

CAnimSceneDictionary::~CAnimSceneDictionary()
{
}

//////////////////////////////////////////////////////////////////////////

CAnimScene* CAnimSceneDictionary::GetScene(CAnimScene::Id sceneId)
{
	if(sceneId != ANIM_SCENE_ID_INVALID)
	{
		CAnimScene **ppScene = m_scenes.SafeGet(sceneId);
		if(ppScene)
		{
			return *ppScene;
		}
	}

	return NULL;
}