/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    scene/streamer/SceneLoader.h
// PURPOSE : handles sync/async loading of scene data at new position
// AUTHOR :  Ian Kiigan
// CREATED : 09/08/10
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef _SCENE_STREAMER_SCENELOADER_H_
#define _SCENE_STREAMER_SCENELOADER_H_

#include "atl/array.h"
#include "entity/entity.h"
#include "scene/RegdRefTypes.h"
#include "vector/vector3.h"

class CEntity;
class CInteriorInst;

enum
{
	SCENELOADER_STAGE_1 = 0,
	SCENELOADER_STAGE_2,
	SCENELOADER_STAGE_3,
	SCENELOADER_STAGE_4,

	SCENELOADER_STAGE_TOTAL
};

enum
{
	SCENELOADER_ASYNCLOAD_STATE_PROCESS = 0,
	SCENELOADER_ASYNCLOAD_STATE_WAITINGFORSTREAMING,
	SCENELOADER_ASYNCLOAD_STATE_DONE
};

class CSceneLoader
{
public:
	CSceneLoader() : m_bSafeToDelete(true), m_bLoadingScene(false), m_nState(SCENELOADER_ASYNCLOAD_STATE_DONE) {}

	void LoadScene(const Vector3& vPos);
	void StartLoadScene(const Vector3& vPos);
	bool UpdateLoadScene();
	void StopLoadScene();
	void LoadInteriorRoom(CInteriorInst* pInterior, u32 roomIdx);
	inline bool IsLoading() { return m_bLoadingScene; }
	inline bool LoadSceneThisFrame() { return m_bLoadSceneThisFrame; }
	inline void ClearLoadSceneThisFrame() { m_bLoadSceneThisFrame = false; }
	inline const Vector3& GetLoadPos() { return m_vLoadPos; }

private:
	inline bool SafeToDeleteEntities() { return m_bSafeToDelete; }

	void Process(u32 eStage);
	void RequestAroundPos(const Vector3& vPos, float fLodRadius, float fHdRadius);
	
	s32					m_nState;
	s32					m_nCurrentStage;
	bool				m_bLoadingScene;
	Vector3				m_vLoadPos;
	bool				m_bSafeToDelete;
	bool				m_bLoadSceneThisFrame;
	u32					m_asyncTimeout;		// some multiplayer scripts are mistakenly assuming async loadscene will always complete
};

#endif //_SCENE_STREAMER_SCENELOADER_H_
