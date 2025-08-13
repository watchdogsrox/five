#ifndef _ANIM_SCENE_MANAGER_H_
#define _ANIM_SCENE_MANAGER_H_

#include "AnimScene.h"
#include "AnimSceneEditor.h"

// Defines
#define ANIM_SCENE_DICT_INDEX_INVALID (-1)
#define ANIM_SCENE_INST_INVALID (0)

//
// fwAnimManager
// Storage and retrieval of anim scene metadata files
//
class CAnimSceneManager
{
protected:
	CAnimSceneManager();

	CAnimSceneManager(s32 maxScenes);

public:
	typedef u32 InstId;

	virtual ~CAnimSceneManager() {};

	// PURPOSE: Static initialisation
	static void Init(unsigned initMode);
    
	// PURPOSE: Static shutdown
	static void Shutdown(unsigned shutdownMode);

#if USE_ANIM_SCENE_DICTIONARIES
	// PURPOSE: Create a new anim scene instance from the given dictionary and hash
	static InstId CreateAnimScene(atHashString dictHash, CAnimScene::Id sceneId, eAnimSceneFlagsBitSet* pFlags = NULL);
#endif // USE_ANIM_SCENE_DICTIONARIES

	// PURPOSE: Create a new anim scene from the given scene id
	static InstId CreateAnimScene( CAnimScene::Id sceneId, eAnimSceneFlagsBitSet* pFlags = NULL, CAnimScenePlaybackList::Id listId = ANIM_SCENE_PLAYBACK_LIST_ID_INVALID);

#if __BANK
	// PURPOSE: Create a new instance of an anim scene by loading it from disk
	static InstId CreateAnimScene(const char * filename, eAnimSceneFlagsBitSet* pFlags = NULL, CAnimScenePlaybackList::Id listId = ANIM_SCENE_PLAYBACK_LIST_ID_INVALID);
	// PURPOSE: Create an empty anim scene and return the pointer directly. Only for use by the editing widgets
	static CAnimScene* BankCreateAnimScene();
	// PURPOSE: Remove an anim scene created by the store
	static void BankDestroyAnimScene(CAnimScene* pScene);
	// PURPOSE: 
	static bool IsDebugBankActive() { return m_pBank && m_pBank->IsActive(); }
	// PURPOSE: Find the inst id from the scene pointer (debug only, this is slow).
	//			if we end up needing it for genuine reasons, cache it in the scene itself.
	static InstId FindInstId(CAnimScene* pScene);
	// PURPOSE: Enable or disable force loading of scenes from disk.
	static void SetForceLoadScenesFromDisk(bool bEnabled) { sm_bForceLoadScenesFromDisk = bEnabled; }
	// PURPOSE: Is force loading from disk enabled
	static bool IsForceLoadScenesFromDiskEnabled() { return sm_bForceLoadScenesFromDisk; }
#endif //__BANK

	// PURPOSE: Does the provided instance Id represent a valid scene
	static bool IsValidInstId(InstId instId);

	// PURPOSE: Remove an anim scene created by the store
	static void DestroyAnimScene(InstId instId);

	// PURPOSE: Remove all anim scenes created by the store
	static void DestroyAllAnimScenes();

	// PURPOSE: Ref counting mechanism for anim scenes
	static void AddRefAnimScene(InstId instId);

	// PURPOSE: Ref counting mechanism for anim scenes
	static void ReleaseAnimScene(InstId instId);

	// PURPOSE: Per frame update of all anim scenes
	static void Update(AnimSceneUpdatePhase phase);

	// PURPOSE: Trigger the pre-anim update phase (need this as its called directly from the game framework)
	static void UpdatePreAnim() { Update(kAnimSceneUpdatePreAnim);}

	// PURPOSE: Retrieve an anim scene instance from its id
	static CAnimScene* GetAnimScene(InstId instId);

	// PURPOSE: Should the minimap be displayed this frame?
	static bool GetDisplayMiniMapThisUpdate();

#if __BANK
	// PURPOSE: Builds the rag widgets that are always available (even if the bank is inactive)
	static void AddPersistentWidgets(fwDebugBank* pBank);

	// PURPOSE: Builds the anim scene manager rag editing and testing widgets
	static void AddWidgets(bkBank* pBank);

	// PURPOSE: Builds the anim scene manager rag editing and testing widgets
	static void AddSceneToWidgets(CAnimScene& scene);

	// PURPOSE: Activates the widget editing
	static void ActivateBank();

	// PURPOSE: Deactivates the widget editing
	static void DeactivateBank();

	// PURPOSE: Return the name of the file on disk
	static atString GetFileNameFromSceneName(const char * name);

	// PURPOSE: Creates a new anim scene and displays the editing widgets for it.
	static void CreateScene();

	// PURPOSE:  Request functions for the below 3 Load functions below so that they can be processed on the main thread.
	static void RequestLoadSceneFromDisk();
	static void RequestLoadAndSaveAllScenesFromDisk();
	static void RequestLoadSceneFromRPF();

	// PURPOSE: Loads an anim scene from disk and displays the editing widgets for it.
	static void LoadSceneFromDisk();

	// PURPOSE: Load and resave all scenes from disk
	static void LoadAndSaveAllScenesFromDisk();

	// PURPOSE: Loads an anim scene from the in memory rpf and displays the editing widgets for it.
	static void LoadSceneFromRPF();

	// PURPOSE:Init the scene editor with teh correct data
	static void InitSceneEditor(CAnimScene* pNewScene);

	// PURPOSE: Access to the scene editor
	CAnimSceneEditor& GetSceneEditor() { return m_sceneEditor; }

#if USE_ANIM_SCENE_DICTIONARIES
	// PURPOSE: Loads an anim scene from disk and displays the editing widgets for it.
	static void LoadSceneFromDictionary();

	static void LoadDictionaryNameList();
#endif //USE_ANIM_SCENE_DICTIONARIES

	static void LoadSceneNameList();

	static atArray<const char *>& GetAnimSceneNames() { return sm_SceneNames;}

	static void IterateScenesOnDisk(const char * path, const char * fileType,  bool recursive);

	// PURPOSE: Debug rendering functionality
	static void ProcessDebugRender();

	// PURPOSE: Returen true if the left mouse button has just been pressed
	static bool IsMouseDown() { return (ioMouse::GetPressedButtons() & ioMouse::MOUSE_LEFT) != 0; }

	//PURPOSE: Update the authoring helpers for each anim scene
	static void UpdateAnimSceneMatrixHelpers();

	static void OnGameEntityRegister(); 

	static void NewSceneNameChanged();

	static bool IsSceneNameValid(const char* pSceneName);
#endif //__BANK

#if !__NO_OUTPUT
	static atString GetSceneDescription(CAnimScene* pScene, CAnimSceneManager::InstId scene = ANIM_SCENE_INST_INVALID);

	static atString GetSceneDescription(CAnimSceneManager::InstId scene);
#endif // !__NO_OUTPUT 
	
private:

	// PURPOSE: Non static debug render function for the manager
	void DebugRender();

	InstId CreateInstId();

	atMap<InstId, CAnimScene*> m_animSceneMap;
	InstId m_lastInstId;
	s32 m_maxScenes;

#if __BANK

	// debug rendering members
	bool m_bDisplaySceneDebug; // Shows the current list of anim scenes with summary information
	bool m_bDisplaySceneEditing; // Activates the on screen anim scene editor

	CAnimSceneEditor m_sceneEditor;

	static bool sm_bForceLoadScenesFromDisk;

	static bool ms_bRequestedLoadSceneFromDisk;
	static bool ms_bRequestedLoadAndSaveAllScenesFromDisk;
	static bool ms_bRequestedLoadSceneFromRPF;

	static fwDebugBank* m_pBank;
	static bkGroup* m_pSceneWidgets;
	static bkGroup* m_pEditorWidgets;
	static atHashString sm_NewSceneName;
	static atHashString sm_NewScenePathText;
	static atHashString sm_CreateStatusText;

#if USE_ANIM_SCENE_DICTIONARIES
	static atArray<atString> sm_DictStrings;
	static atArray<const char *> sm_DictNames;
	static s32 sm_SelectedDict;
	static bkCombo* sm_pDictCombo;
#endif // USE_ANIM_SCENE_DICTIONARIES

	static atArray<atString> sm_SceneFileNames;
	static atArray<atString> sm_SceneStrings;
	static atArray<const char *> sm_SceneNames;
	static s32 sm_SelectedScene;
	static CDebugAnimSceneSelector ms_animSceneSelector;
	static bkCombo* sm_pSceneCombo;
#endif // __BANK
};

// Global instance to access all the virtual functions
extern CAnimSceneManager *g_AnimSceneManager;

#endif //_ANIM_SCENE_MANAGER_H_
