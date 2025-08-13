#include "AnimSceneManager.h"

// Rage headers


// framework headers
#include "fwsys/gameskeleton.h"
#include "fwsys/metadatastore.h"

// game headers
#include "AnimSceneDictionary.h"
#include "peds/ped.h"
#include "peds/PedFactory.h"


ANIM_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// --- CAnimSceneManager -------------------------------------------------

// public static interface

CAnimSceneManager *g_AnimSceneManager;

#if __BANK
//pointer to the rag bank
bool CAnimSceneManager::sm_bForceLoadScenesFromDisk = false;
bool CAnimSceneManager::ms_bRequestedLoadSceneFromDisk = false;
bool CAnimSceneManager::ms_bRequestedLoadAndSaveAllScenesFromDisk = false;
bool CAnimSceneManager::ms_bRequestedLoadSceneFromRPF = false;
fwDebugBank* CAnimSceneManager::m_pBank = NULL;
bkGroup* CAnimSceneManager::m_pSceneWidgets = NULL;
bkGroup* CAnimSceneManager::m_pEditorWidgets = NULL;
atHashString CAnimSceneManager::sm_NewSceneName;
atHashString CAnimSceneManager::sm_NewScenePathText;
atHashString CAnimSceneManager::sm_CreateStatusText;

#if USE_ANIM_SCENE_DICTIONARIES
atArray<atString> CAnimSceneManager::sm_DictStrings;
atArray<const char *> CAnimSceneManager::sm_DictNames;
s32 CAnimSceneManager::sm_SelectedDict = -1;
bkCombo* CAnimSceneManager::sm_pDictCombo = NULL;
#endif // USE_ANIM_SCENE_DICTIONARIES

atArray<atString> CAnimSceneManager::sm_SceneStrings;
atArray<atString> CAnimSceneManager::sm_SceneFileNames;
atArray<const char *> CAnimSceneManager::sm_SceneNames;
s32 CAnimSceneManager::sm_SelectedScene = 0;
CDebugAnimSceneSelector CAnimSceneManager::ms_animSceneSelector;
bkCombo* CAnimSceneManager::sm_pSceneCombo = NULL;
#endif //__BANK

//////////////////////////////////////////////////////////////////////////

void CAnimSceneManager::Init(unsigned initMode)
{
    if(initMode == INIT_CORE)
    {
		int animScenes = fwConfigManager::GetInstance().GetSizeOfPool(ATSTRINGHASH("AnimScenes", 0x7567ba93), 1);
		animAssertf(animScenes >= 1, "There should be at least one anim scene allowed in gameconfig.xml!");
		animScenes = Max(1, animScenes);

		Assert(!g_AnimSceneManager);
		g_AnimSceneManager = rage_new CAnimSceneManager(animScenes);

		// TODO - resident scenes?

		// Disable in-place loading for anim scenes because they contain maps
		parStructure* animSceneStruct = CAnimScene::parser_GetStaticStructure();
		animSceneStruct->GetFlags().Set(parStructure::NOT_IN_PLACE_LOADABLE);

#if __BANK
		CAnimScene::InitEntityEditing();
		CAnimSceneEventList::InitEventEditing();		
		m_pBank = fwDebugBank::CreateBank("Anim Scenes", MakeFunctor(CAnimSceneManager::ActivateBank), MakeFunctor(CAnimSceneManager::DeactivateBank), MakeFunctor(CAnimSceneManager::AddPersistentWidgets));
#endif //__BANK
	}
}

//////////////////////////////////////////////////////////////////////////

void CAnimSceneManager::Shutdown(unsigned shutdownMode)
{
    if(shutdownMode == SHUTDOWN_CORE)
    {
#if __BANK
		if(m_pBank)
		{
			m_pBank->Shutdown();
			m_pBank = NULL;
		}
#endif //__BANK

		// delete the scene manager instance
		if (g_AnimSceneManager)
		{
			g_AnimSceneManager->DestroyAllAnimScenes();
			delete g_AnimSceneManager;
			g_AnimSceneManager = NULL;
		}
	}
}

//////////////////////////////////////////////////////////////////////////

void CAnimSceneManager::Update(AnimSceneUpdatePhase phase)
{
	if (fwTimer::GetSingleStepThisFrame() || (!fwTimer::IsUserPaused() && !fwTimer::IsScriptPaused() && !fwTimer::IsGamePaused()))
	{
		// iterate the scenes in the map and update them
		atMap<InstId, CAnimScene*>::Iterator it = g_AnimSceneManager->m_animSceneMap.CreateIterator();

		while (!it.AtEnd())
		{
			if (it.GetData())
			{
				it.GetData()->Update(phase);
			}
			it.Next();
		}

#if __BANK
		if(phase == kAnimSceneUpdatePreAnim)
		{
			UpdateAnimSceneMatrixHelpers(); 
		}
#endif
	}
}

//////////////////////////////////////////////////////////////////////////

CAnimScene* CAnimSceneManager::GetAnimScene(InstId instId)
{
	// access the map
	CAnimScene** ppScene = g_AnimSceneManager->m_animSceneMap.Access(instId);

	if (ppScene)
	{
		return *ppScene;
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////

bool CAnimSceneManager::GetDisplayMiniMapThisUpdate()
{
#if __BANK
	if (g_AnimSceneManager)
	{
		return g_AnimSceneManager->m_sceneEditor.GetDisplayMiniMapThisUpdate();
	}
#endif // __BANK

	// Display minimap otherwise
	return true;
}

#if USE_ANIM_SCENE_DICTIONARIES
//////////////////////////////////////////////////////////////////////////

CAnimSceneManager::InstId CAnimSceneManager::CreateAnimScene(atHashString dictHash, CAnimScene::Id sceneId, eAnimSceneFlagsBitSet* pFlags /*= NULL*/)
{
	// store loading
	s32 metaFileIndex = g_fwMetaDataStore.FindSlotFromHashKey(dictHash);
	animAssertf(metaFileIndex != -1, "Unable to find anim scene dictionary %s!", dictHash.GetCStr());
	if (g_fwMetaDataStore.IsValidSlot(metaFileIndex))
	{
		g_fwMetaDataStore.StreamingRequest(metaFileIndex, STRFLAG_PRIORITY_LOAD | STRFLAG_FORCE_LOAD | STRFLAG_DONTDELETE);
		strStreamingEngine::GetLoader().LoadAllRequestedObjects();

		animAssertf(g_fwMetaDataStore.HasObjectLoaded(metaFileIndex), "Anim scene dictionary %s is not loaded!", dictHash.GetCStr());
		if (g_fwMetaDataStore.HasObjectLoaded(metaFileIndex))
		{
			if (g_fwMetaDataStore.Get(metaFileIndex))
			{
				CAnimSceneDictionary* pDict  = g_fwMetaDataStore.Get(metaFileIndex)->GetObject< CAnimSceneDictionary >();
				if (pDict)
				{
					CAnimScene* pScene = pDict->GetScene(sceneId);
					animAssertf(pScene, "Anim scene %s does not exist in dictionary %s", sceneId.GetCStr(), dictHash.GetCStr());
					if (pScene)
					{
						animAssertf(g_AnimSceneManager->m_animSceneMap.GetNumUsed()<g_AnimSceneManager->m_maxScenes, "Ran out of anim scenes!");
						if (g_AnimSceneManager->m_animSceneMap.GetNumUsed()<g_AnimSceneManager->m_maxScenes)
						{
							CAnimScene* pNewScene = rage_new CAnimScene();
							pScene->CopyTo(pNewScene);

							if (!pNewScene)
							{
								animAssertf(0, "Failed to allocate new anim scene! Out of memory? new scene name: %s", sceneId.GetCStr());
								return ANIM_SCENE_INST_INVALID;
							}

							if (pFlags)
							{
								// intersect the flags with the passed in ones
								pNewScene->GetFlags().BitSet().Union(pFlags->BitSet());
							}

							// metadata loaded successfully. Generate a new instance Id and add the scene to the map
							InstId newId = g_AnimSceneManager->CreateInstId();
							pNewScene->SetInstId(newId);

							g_AnimSceneManager->m_animSceneMap.Insert(newId, pNewScene);

							return newId;
						}
					}
				}
			}
		}
	}

	return ANIM_SCENE_INST_INVALID;
}

#endif //USE_ANIM_SCENE_DICTIONARIES

//////////////////////////////////////////////////////////////////////////

CAnimSceneManager::InstId CAnimSceneManager::CreateAnimScene( CAnimScene::Id sceneId, eAnimSceneFlagsBitSet* pFlags /*= NULL*/, CAnimScenePlaybackList::Id listId /*= ANIM_SCENE_PLAYBACK_LIST_ID_INVALID*/)
{
#if __BANK
	if (sm_bForceLoadScenesFromDisk)
	{
		return CreateAnimScene(sceneId.GetCStr(), pFlags, listId);
	}
#endif //__BANK

	// store loading
	strLocalIndex metaFileIndex = g_fwMetaDataStore.FindSlotFromHashKey(sceneId);
	animAssertf(metaFileIndex != -1, "Unable to find anim scene %s!", sceneId.GetCStr());
	if (g_fwMetaDataStore.IsValidSlot(metaFileIndex))
	{		
		animAssertf(g_AnimSceneManager->m_animSceneMap.GetNumUsed()<g_AnimSceneManager->m_maxScenes, "Ran out of anim scenes!");
		if (g_AnimSceneManager->m_animSceneMap.GetNumUsed()<g_AnimSceneManager->m_maxScenes)
		{
			CAnimScene* pNewScene = rage_new CAnimScene(sceneId, pFlags, listId);

			if (!pNewScene)
			{
				animAssertf(0, "Failed to allocate new anim scene! Out of memory? new scene name: %s", sceneId.GetCStr());
				return ANIM_SCENE_INST_INVALID;
			}

			// Generate a new instance Id and add the scene to the map
			InstId newId = g_AnimSceneManager->CreateInstId();
			pNewScene->SetInstId(newId);

			g_AnimSceneManager->m_animSceneMap.Insert(newId, pNewScene);

			return newId;
		}
	}

	return ANIM_SCENE_INST_INVALID;
}

//////////////////////////////////////////////////////////////////////////

#if __BANK
// PURPOSE: Create a new instance of an anim scene by loading it from disk
CAnimSceneManager::InstId CAnimSceneManager::CreateAnimScene(const char * filename, eAnimSceneFlagsBitSet* pFlags /*= NULL*/, CAnimScenePlaybackList::Id listId /*= ANIM_SCENE_PLAYBACK_LIST_ID_INVALID*/)
{
	animAssertf(g_AnimSceneManager->m_animSceneMap.GetNumUsed()<g_AnimSceneManager->m_maxScenes, "Ran out of anim scenes! used: %d, max: %d, new scene name: %s", g_AnimSceneManager->m_animSceneMap.GetNumUsed(), g_AnimSceneManager->m_maxScenes, filename);

	if (g_AnimSceneManager->m_animSceneMap.GetNumUsed()<g_AnimSceneManager->m_maxScenes)
	{
		// create the new scene instance
		CAnimScene* pScene = rage_new CAnimScene();

		if (!pScene)
		{
			animAssertf(0, "Failed to allocate new anim scene! Out of memory? new scene name: %s", filename);
			return ANIM_SCENE_INST_INVALID;
		}

		// Load the metadata from file
		if (!pScene->Load(filename))
		{
			animAssertf(0, "Failed to load anim scene from file: %s!", filename);
			delete pScene;
			return ANIM_SCENE_INST_INVALID;
		}

		if (pFlags)
		{
			// intersect the flags with the passed in ones
			pScene->GetFlags().BitSet().Union(pFlags->BitSet());
		}

		pScene->SetPlayBackList(listId);

		// metadata loaded successfully. Generate a new instance Id and add the scene to the map
		InstId newId = g_AnimSceneManager->CreateInstId();
		pScene->SetInstId(newId);

		g_AnimSceneManager->m_animSceneMap.Insert(newId, pScene);

		return newId;
	}

	return ANIM_SCENE_INST_INVALID;
}

//////////////////////////////////////////////////////////////////////////

CAnimScene* CAnimSceneManager::BankCreateAnimScene()
{
	animAssertf(g_AnimSceneManager->m_animSceneMap.GetNumUsed()<g_AnimSceneManager->m_maxScenes, "Ran out of anim scenes! used: %d, max: %d", g_AnimSceneManager->m_animSceneMap.GetNumUsed(), g_AnimSceneManager->m_maxScenes);

	if (g_AnimSceneManager->m_animSceneMap.GetNumUsed()<g_AnimSceneManager->m_maxScenes)
	{
		// create the new scene instance
		CAnimScene* pScene = rage_new CAnimScene();
		pScene->SetFileName(sm_NewSceneName.GetCStr());

		if (!pScene)
		{
			animAssertf(0, "Failed to allocate new anim scene! Out of memory?");
			return NULL;
		}

		// metadata loaded successfully. Generate a new instance Id and add the scene to the map
		InstId newId = g_AnimSceneManager->CreateInstId();
		pScene->SetInstId(newId);

		g_AnimSceneManager->m_animSceneMap.Insert(newId, pScene);

		return pScene;
	}
	return NULL;
}

//////////////////////////////////////////////////////////////////////////

void CAnimSceneManager::BankDestroyAnimScene(CAnimScene* pScene)
{
	InstId id = FindInstId(pScene);

	if (id != ANIM_SCENE_INST_INVALID)
	{
		DestroyAnimScene(id);
	}
}

//////////////////////////////////////////////////////////////////////////

CAnimSceneManager::InstId CAnimSceneManager::FindInstId(CAnimScene* pScene)
{
	InstId id = ANIM_SCENE_INST_INVALID;

	// find the id
	atMap<InstId, CAnimScene*>::Iterator it = g_AnimSceneManager->m_animSceneMap.CreateIterator();
	
	for( ; !it.AtEnd() && id==ANIM_SCENE_INST_INVALID; it.Next())
	{
		if (it.GetData()==pScene)
		{
			id=it.GetKey();
		}
	}

	return id;
}

#endif //__BANK

//////////////////////////////////////////////////////////////////////////

void CAnimSceneManager::DestroyAnimScene(InstId instId)
{
	CAnimScene** ppScene = g_AnimSceneManager->m_animSceneMap.Access(instId);

	if (ppScene)
	{
		animAssertf(*ppScene, "Trying to delete NULL anim scene in Anim scene manager map!");
		animAssertf((*ppScene)->GetNumRefs()==0, "Trying to delete anim scene '%s' that still has refs!", (*ppScene)->GetName().GetCStr());
		delete *ppScene;
		*ppScene = NULL;
		
		g_AnimSceneManager->m_animSceneMap.Delete(instId);
	}
	else
	{
		// assert here? (unknown scene instance id)
		animAssertf(0, "Trying to delete invalid anim scene instance. Inst id: %d", instId);
	}
}

//////////////////////////////////////////////////////////////////////////

void CAnimSceneManager::AddRefAnimScene(InstId instId)
{
	CAnimScene** ppScene = g_AnimSceneManager->m_animSceneMap.Access(instId);

	if (ppScene)
	{
		CAnimScene* pScene = (*ppScene);
		pScene->AddRef();
	}
	else
	{
		animAssertf(0, "Trying to add ref invalid anim scene instance. Inst id: %d", instId);
	}
}

//////////////////////////////////////////////////////////////////////////

void CAnimSceneManager::ReleaseAnimScene(InstId instId)
{
	CAnimScene** ppScene = g_AnimSceneManager->m_animSceneMap.Access(instId);

	if (ppScene)
	{
		CAnimScene* pScene = (*ppScene);
		pScene->RemoveRefWithoutDelete();
		if (pScene->GetNumRefs()==0)
		{
			delete *ppScene;
			*ppScene = NULL;

			g_AnimSceneManager->m_animSceneMap.Delete(instId);
		}
	}
	else
	{
		animAssertf(0, "Trying to release invalid anim scene instance. Inst id: %d", instId);
	}
}


//////////////////////////////////////////////////////////////////////////

void CAnimSceneManager::DestroyAllAnimScenes()
{
	atMap<InstId, CAnimScene*>::Iterator it = g_AnimSceneManager->m_animSceneMap.CreateIterator();

	while (!it.AtEnd())
	{
		if (it.GetData())
		{
			delete it.GetData();
			it.GetData()=NULL;
		}
		it.Next();
	}

	g_AnimSceneManager->m_animSceneMap.Reset();
}

//////////////////////////////////////////////////////////////////////////
bool CAnimSceneManager::IsValidInstId(InstId instId)
{
	CAnimScene** ppScene = g_AnimSceneManager->m_animSceneMap.Access(instId);
	if (ppScene)
	{
		return true;
	}
	
	return false;
}

//////////////////////////////////////////////////////////////////////////
// protected local methods

CAnimSceneManager::CAnimSceneManager()
	: m_maxScenes(1)
	, m_lastInstId(0)
#if __BANK
	, m_bDisplaySceneDebug(false)
	, m_bDisplaySceneEditing(false)
#endif //__BANK
{
}

CAnimSceneManager::CAnimSceneManager(s32 maxScenes)
	: m_maxScenes(maxScenes)
	, m_lastInstId(0)
#if __BANK
	, m_bDisplaySceneDebug(false)
	, m_bDisplaySceneEditing(false)
#endif //__BANK
{
}

//////////////////////////////////////////////////////////////////////////

CAnimSceneManager::InstId CAnimSceneManager::CreateInstId()
{
	m_lastInstId++;

	// handle wrapping
	if (m_lastInstId==ANIM_SCENE_INST_INVALID)
		m_lastInstId++;

	return m_lastInstId;
}

#if __BANK

//////////////////////////////////////////////////////////////////////////

atString CAnimSceneManager::GetFileNameFromSceneName(const char * name)
{
	for (s32 i=0; i<sm_SceneNames.GetCount(); i++)
	{
		if (!strcmpi(name,sm_SceneNames[i]))
		{
			return sm_SceneFileNames[i];
		}
	}
	// no existing file name found - just create a new one
	atString newName(name);

	// TODO JA REMOVE TEMP CODE
	// This code will allow new and old style animscenes to co-exist for one week until they have all been moved over
	atString searchText("@");
	if (newName.IndexOf(searchText)>0)
	{
		newName.Replace("@", "/@");
	}
	else
	{
		newName.Replace("_", "/");
	}
	// TODO JA REMOVE TEMP CODE

	//newName.Replace("@", "/@");

	return newName;
}

//////////////////////////////////////////////////////////////////////////

void CAnimSceneManager::ActivateBank()
{
	AddWidgets(m_pBank);
}

//////////////////////////////////////////////////////////////////////////

void CAnimSceneManager::DeactivateBank()
{
	g_AnimSceneManager->m_sceneEditor.Shutdown();

	// first delete all the anim scenes we own
	atMap<InstId, CAnimScene*>::Iterator it = g_AnimSceneManager->m_animSceneMap.CreateIterator();
	
	for ( ; !it.AtEnd(); it.Next())
	{
		if (it.GetData() && it.GetData()->IsOwnedByEditor())
		{
			it.GetData()->ShutdownWidgets();
			DestroyAnimScene(FindInstId(it.GetData()));
			// The iterator is now invalid. restart it.
			it = g_AnimSceneManager->m_animSceneMap.CreateIterator();
		}
	}

	ms_animSceneSelector.RemoveWidgets();

	while (m_pBank && m_pBank->GetFirstMainWidget())
	{
		m_pBank->GetFirstMainWidget()->Destroy();
	}
}

//////////////////////////////////////////////////////////////////////////

void CAnimSceneManager::AddPersistentWidgets(fwDebugBank* pBank)
{
	// early out if the bank doesn't exist
	if (!pBank)
	{
		return;
	}

	pBank->AddToggle("Force load anim scenes from disk", &sm_bForceLoadScenesFromDisk);
}

//////////////////////////////////////////////////////////////////////////

void CAnimSceneManager::AddWidgets(bkBank* pBank)
{
	// early out if the bank doesn't exist
	if (!pBank)
	{
		return;
	}

	pBank->AddToggle("Activate scene editor", &g_AnimSceneManager->m_bDisplaySceneEditing);
	pBank->PushGroup("Debug rendering");
	{
		pBank->AddToggle("Render scene info", &g_AnimSceneManager->m_bDisplaySceneDebug);
	}
	pBank->PopGroup();

	// add a text box to load the scene from disk
	pBank->PushGroup("Create new scene");
	pBank->AddText("New scene name", &sm_NewSceneName, false, CAnimSceneManager::NewSceneNameChanged);
	pBank->AddText("New path name", &CAnimSceneManager::sm_NewScenePathText, true);
	
	sm_CreateStatusText = "";
	pBank->AddText("", &sm_CreateStatusText, false);
	pBank->AddButton("Create", CreateScene);
	pBank->PopGroup();
	
	pBank->PushGroup("Load scene");
	// add a combo box to load from a dictionary
#if USE_ANIM_SCENE_DICTIONARIES
	sm_pDictCombo = pBank->AddCombo("Dictionary:", &sm_SelectedDict,  0, NULL );
#endif // USE_ANIM_SCENE_DICTIONARIES
	//sm_pSceneCombo = pBank->AddCombo("Scene name:", &sm_SelectedScene, 0, NULL );
	ms_animSceneSelector.AddWidgets(pBank, "Scene name selector: ");
#if USE_ANIM_SCENE_DICTIONARIES
	pBank->AddButton("Load scene from dictionary", LoadSceneFromDictionary);
#else // USE_ANIM_SCENE_DICTIONARIES
	pBank->AddButton("Load scene from disk", RequestLoadSceneFromDisk);
	pBank->AddButton("Load and save all scenes from disk", RequestLoadAndSaveAllScenesFromDisk);
	pBank->AddButton("Load scene from rpf", RequestLoadSceneFromRPF);
#endif // USE_ANIM_SCENE_DICTIONARIES
	pBank->PopGroup();

#if USE_ANIM_SCENE_DICTIONARIES
	LoadDictionaryNameList();
#endif // USE_ANIM_SCENE_DICTIONARIES
	LoadSceneNameList();
	ms_animSceneSelector.RefreshList();

	m_pSceneWidgets = pBank->PushGroup("Anim scenes");
// 	{
// 		atMap<InstId, CAnimScene*>::Iterator it = g_AnimSceneManager->m_animSceneMap.CreateIterator();
// 
// 		while (!it.AtEnd())
// 		{
// 			if (it.GetData())
// 				it.GetData()->AddWidgets(pBank);
// 			it.Next();
// 		}
// 	}
	pBank->PopGroup(); // Anim scenes

	m_pEditorWidgets = pBank->PushGroup("Editor Settings", false);
	{
		g_AnimSceneManager->m_sceneEditor.AddEditorWidgets(m_pEditorWidgets);
	}
	pBank->PopGroup(); // Editor Settings
}

//////////////////////////////////////////////////////////////////////////

void CAnimSceneManager::AddSceneToWidgets(CAnimScene& UNUSED_PARAM(scene))
{
// 	// early out if the bank doesn't exist
// 	if (!m_pBank || !m_pSceneWidgets)
// 	{
// 		return;
// 	}
// 
// 	if (m_pBank && m_pSceneWidgets)
// 	{
// 		m_pBank->SetCurrentGroup(*m_pSceneWidgets);
// 		scene.AddWidgets(m_pBank);
// 		m_pBank->UnSetCurrentGroup(*m_pSceneWidgets);
// 	}
}

void CAnimSceneManager::InitSceneEditor(CAnimScene* pScene)
{
	const float ASE_SCREEN_EDGE_PADDING_TOP = 20.0f;
	const float ASE_SCREEN_EDGE_PADDING_BOTTOM = 50.0f;
	const float ASE_SCREEN_EDGE_PADDING_LEFT = 50.0f;
	const float ASE_SCREEN_EDGE_PADDING_RIGHT = 50.0f;

	if (pScene)
	{
		if (g_AnimSceneManager->m_sceneEditor.IsActive())
			g_AnimSceneManager->m_sceneEditor.Shutdown();

		g_AnimSceneManager->m_bDisplaySceneEditing = true;
		g_AnimSceneManager->m_sceneEditor.Init(pScene, CScreenExtents(	ASE_SCREEN_EDGE_PADDING_LEFT,
																		ASE_SCREEN_EDGE_PADDING_TOP,
																		(float)GRCDEVICE.GetWidth() - ASE_SCREEN_EDGE_PADDING_RIGHT,
																		(float)GRCDEVICE.GetHeight() - ASE_SCREEN_EDGE_PADDING_BOTTOM));

		if (!g_AnimSceneManager->m_sceneEditor.HasWidgets())
		{
			g_AnimSceneManager->m_sceneEditor.AddWidgets(m_pSceneWidgets);
		}
		// 		pScene->SetOwnedByEditor();
		// 		AddSceneToWidgets(*pScene);
	}
}

//////////////////////////////////////////////////////////////////////////

void CAnimSceneManager::CreateScene()
{
	// early out if the bank doesn't exist
	if (!m_pBank || !m_pSceneWidgets || !m_pEditorWidgets)
	{
		return;
	}

	// Only create a new scene if the scene name specified is valid
	if (IsSceneNameValid(sm_NewSceneName.GetCStr()))
	{
		// start by loading the scene with the given name
		CAnimScene* pNewScene = BankCreateAnimScene();

		if (pNewScene)
		{
			sm_CreateStatusText = "Create Scene Successful";
		}
		else
		{
			sm_CreateStatusText = "Create Scene Failed";
		}

		// default the scene location to the current location

		CPed* pPlayer = FindPlayerPed();

		if (pPlayer && pNewScene)
			pNewScene->SetSceneOrigin(pPlayer->GetMatrix());


		InitSceneEditor(pNewScene);
	}
	else
	{
		sm_CreateStatusText = "Create Scene Failed - Invalid Scene Name";
	}
}

//////////////////////////////////////////////////////////////////////////

void CAnimSceneManager::RequestLoadSceneFromDisk()
{
	ms_bRequestedLoadSceneFromDisk = true;
	ms_bRequestedLoadAndSaveAllScenesFromDisk = false;
	ms_bRequestedLoadSceneFromRPF = false;
}

void CAnimSceneManager::RequestLoadAndSaveAllScenesFromDisk()
{
	ms_bRequestedLoadSceneFromDisk = false;
	ms_bRequestedLoadAndSaveAllScenesFromDisk = true;
	ms_bRequestedLoadSceneFromRPF = false;
}

void CAnimSceneManager::RequestLoadSceneFromRPF()
{
	ms_bRequestedLoadSceneFromDisk = false;
	ms_bRequestedLoadAndSaveAllScenesFromDisk = false;
	ms_bRequestedLoadSceneFromRPF = true;
}

//////////////////////////////////////////////////////////////////////////

void CAnimSceneManager::LoadSceneFromDisk()
{
	// early out if the bank doesn't exist
	if (!m_pBank || !m_pSceneWidgets || !m_pEditorWidgets)
	{
		return;
	}
	
	// check we have a valid scene selected
	if (ms_animSceneSelector.GetSelectedIndex() <0 || ms_animSceneSelector.GetSelectedIndex() >= ms_animSceneSelector.GetList().GetCount())
	{
		return;
	}

	// start by loading the scene with the given name
	CAnimSceneManager::InstId sceneId = CreateAnimScene(ms_animSceneSelector.GetSelectedName());

	CAnimScene* pScene = GetAnimScene(sceneId);

	InitSceneEditor(pScene);
}

//////////////////////////////////////////////////////////////////////////

void CAnimSceneManager::LoadAndSaveAllScenesFromDisk()
{
	// early out if the bank doesn't exist
	if (!m_pBank || !m_pSceneWidgets || !m_pEditorWidgets)
	{
		return;
	}


	for (s32 i=0; i<sm_SceneNames.GetCount(); i++)
	{
		// start by loading the scene with the given name
		CAnimSceneManager::InstId sceneId = CreateAnimScene(sm_SceneNames[i]);

		CAnimScene* pScene = GetAnimScene(sceneId);

		if (pScene)
		{
			// copy event list to default list - Remove this once all scenes are converted
//			CAnimSceneEventList& sectionEvents = pScene->GetSection(ANIM_SCENE_SECTION_DEFAULT)->GetEvents();
			
// 			for (s32 i=0; i<pScene->m_events.GetEventCount(); i++)
// 			{
// 				sectionEvents.AddEvent(pScene->m_events.GetEvent(i)->Copy());
// 			}

			pScene->Save();

			DestroyAnimScene(sceneId);
			pScene = NULL;
		}		
	}
}

//////////////////////////////////////////////////////////////////////////

void CAnimSceneManager::LoadSceneFromRPF()
{
	// early out if the bank doesn't exist
	if (!m_pBank || !m_pSceneWidgets || !m_pEditorWidgets)
	{
		return;
	}

	// check we have a valid scene selected
	if (ms_animSceneSelector.GetSelectedIndex() <0 || ms_animSceneSelector.GetSelectedIndex() >= ms_animSceneSelector.GetList().GetCount())
	{
		return;
	}

	CAnimScene::Id sceneId(ms_animSceneSelector.GetSelectedName());

	// start by loading the scene with the given name
	CAnimSceneManager::InstId instId = CreateAnimScene(sceneId);

	CAnimScene* pScene = GetAnimScene(instId);
	InitSceneEditor(pScene);
}

#if USE_ANIM_SCENE_DICTIONARIES

//////////////////////////////////////////////////////////////////////////

void CAnimSceneManager::LoadDictionaryNameList()
{
	sm_DictStrings.clear();

	// run over the export folder and grab the sub folders
	USE_DEBUG_ANIM_MEMORY();

	atArray<fiFindData*> folderResults;
	const char * path = CAnimScene::sm_LoadSaveDirectory.GetCStr();

	if (ASSET.Exists(path,""))
	{
		ASSET.EnumFiles(path, CDebugClipDictionary::FindFileCallback, &folderResults);

		for ( int i=0; i<folderResults.GetCount(); i++ )
		{
			atString fileName( folderResults[i]->m_Name );

			if ((fileName.LastIndexOf('.')<0))
			{
				// This is a folder, must be an anim scene dictionary
				sm_DictStrings.PushAndGrow(fileName);
			}
		}

		//delete the contents of the folder results array
		while (folderResults.GetCount())
		{
			delete folderResults.Top();
			folderResults.Top() = NULL;
			folderResults.Pop();
		}
	}

	folderResults.clear();

	sm_DictNames.clear();
	for (s32 i=0; i<sm_DictStrings.GetCount(); i++)
	{
		sm_DictNames.PushAndGrow(sm_DictStrings[i].c_str());
	}

	sm_pDictCombo->UpdateCombo("Dictionary:", &sm_SelectedDict, sm_DictNames.GetCount(), sm_DictNames.GetCount()>0 ? &sm_DictNames[0] : NULL);

	LoadSceneNameList();

}

#endif // USE_ANIM_SCENE_DICTIONARIES

//////////////////////////////////////////////////////////////////////////

void CAnimSceneManager::LoadSceneNameList()
{
	sm_SceneFileNames.clear();
	sm_SceneStrings.clear();

	// run over the export folder and grab the sub folders
	USE_DEBUG_ANIM_MEMORY();

	atString path(CAnimScene::sm_LoadSaveDirectory.GetCStr());

	IterateScenesOnDisk(path, ".pso.meta", true);

	sm_SceneNames.clear();
	for (s32 i=0; i<sm_SceneStrings.GetCount(); i++)
	{
		sm_SceneNames.PushAndGrow(sm_SceneStrings[i].c_str());
	}
	
	//sm_pSceneCombo->UpdateCombo("Scene name:", &sm_SelectedScene, sm_SceneNames.GetCount(), sm_SceneNames.GetCount() ? &sm_SceneNames[0] : NULL);
	//ms_animSceneSelector.RefreshList();
}

void CAnimSceneManager::IterateScenesOnDisk(const char * path, const char * fileType,  bool recursive)
{
	USE_DEBUG_ANIM_MEMORY();

	atArray<fiFindData*> folderResults;

	if (ASSET.Exists(path,""))
	{
		ASSET.EnumFiles(path, CDebugClipDictionary::FindFileCallback, &folderResults);

		for ( int i=0; i<folderResults.GetCount(); i++ )
		{
			atString fileName( path );
			fileName+= "/";
			fileName += folderResults[i]->m_Name;

			if (fileName.EndsWith(fileType) || strlen(fileType)==0)
			{
				atString stripFolder(CAnimScene::sm_LoadSaveDirectory.GetCStr());
				stripFolder+="/";
				fileName.Replace(stripFolder, "");
				fileName.Replace(fileType, "");
				sm_SceneFileNames.PushAndGrow(fileName);

				// TODO JA REMOVE TEMP CODE
				// This code will allow new and old style animscenes to co-exist for one week until they have all been moved over
				atString searchText("@");
				if (fileName.IndexOf(searchText)>=0)
				{
					fileName.Replace("/", "");
				}
				else
				{
					fileName.Replace("/", "_"); //
				}
				// TODO JA REMOVE TEMP CODE

				//fileName.Replace("/", "");

				// file of the correct type
				sm_SceneStrings.PushAndGrow(fileName);
			}
			else if ((fileName.LastIndexOf('.')<0) && recursive)
			{
				// This is a folder, enumerate it too
				IterateScenesOnDisk(fileName, fileType, true);
			}
		}

		//delete the contents of the folder results array
		while (folderResults.GetCount())
		{
			delete folderResults.Top();
			folderResults.Top() = NULL;
			folderResults.Pop();
		}
	}

	folderResults.clear();
}

#if USE_ANIM_SCENE_DICTIONARIES

//////////////////////////////////////////////////////////////////////////

void CAnimSceneManager::LoadSceneFromDictionary()
{
	// early out if the bank doesn't exist
	if (!m_pBank || !m_pSceneWidgets || !m_pEditorWidgets)
	{
		return;
	}

	// start by loading the scene with the given name
	InstId newScene = CreateAnimScene(sm_DictNames[sm_SelectedDict], sm_SceneNames[sm_SelectedScene]);

	if (IsValidInstId(newScene))
	{
		CAnimScene* pNewScene = GetAnimScene(newScene);
		pNewScene->SetOwnedByEditor();
	}
}


#endif // USE_ANIM_SCENE_DICTIONARIES

void CAnimSceneManager::ProcessDebugRender()
{
	g_AnimSceneManager->DebugRender();
}

void CAnimSceneManager::DebugRender()
{
	if (m_pBank->IsActive())
	{
		if (CSystem::IsThisThreadId(SYS_THREAD_UPDATE))
		{
			// Process loading requests
			if (ms_bRequestedLoadSceneFromDisk)
			{
				LoadSceneFromDisk();
				ms_bRequestedLoadSceneFromDisk = false;
			}
			else if (ms_bRequestedLoadAndSaveAllScenesFromDisk)
			{
				LoadAndSaveAllScenesFromDisk();
				ms_bRequestedLoadAndSaveAllScenesFromDisk = false;
			}
			else if (ms_bRequestedLoadSceneFromRPF)
			{
				LoadSceneFromRPF();
				ms_bRequestedLoadSceneFromRPF = false;
			}

			if (m_bDisplaySceneEditing && m_animSceneMap.GetNumUsed()>0)
			{
				m_sceneEditor.Process_MainThread();
			}

#if __DEV
			if (m_bDisplaySceneDebug)
			{
				static float YMax = 600.0f;
				static float YLineOffset = 10.0f;

				const float  mouseScreenX = static_cast<float>(ioMouse::GetX());
				const float  mouseScreenY = static_cast<float>(ioMouse::GetY());

				debugPlayback::OnScreenOutput SceneOutput;
				SceneOutput.m_fXPosition = 50.0f;
				SceneOutput.m_fYPosition = 50.0f;
				SceneOutput.m_fMaxY = YMax;
				SceneOutput.m_fMouseX = mouseScreenX;
				SceneOutput.m_fMouseY = mouseScreenY;
				SceneOutput.m_fPerLineYOffset = YLineOffset;
				SceneOutput.m_Color.Set(160,160,160,255);
				SceneOutput.m_HighlightColor.Set(255,0,0,255);
				SceneOutput.AddLine("%d anim scenes:", m_animSceneMap.GetNumUsed());

				atMap<InstId, CAnimScene*>::Iterator it = g_AnimSceneManager->m_animSceneMap.CreateIterator();

				s32 index=0;
				for( ; !it.AtEnd(); it.Next())
				{
					if (it.GetData())
					{
						CAnimScene* pScene = it.GetData();
						if (SceneOutput.AddLine("index: %d id: %u %s", index, it.GetKey(), pScene->GetDebugSummary().c_str()) && IsMouseDown())
						{
							// select or deselect the scene
							pScene->SetDebugSelected(!pScene->IsDebugSelected());
						}

						pScene->DebugRender(SceneOutput);
					}
					index++;
				}
			}
#endif //__DEV
		}
		else
		{
			if (m_bDisplaySceneEditing)
			{
				m_sceneEditor.Process_RenderThread();
			}
		}
	}
}

void CAnimSceneManager::UpdateAnimSceneMatrixHelpers()
{
	for(int i=0; i < CAnimSceneMatrix::m_AnimSceneMatrix.GetCount(); i++)
	{
		CAnimSceneMatrix* pAnimSceneMatrix = CAnimSceneMatrix::m_AnimSceneMatrix[i]; 
		pAnimSceneMatrix->Update(); 
	}
}

void  CAnimSceneManager::OnGameEntityRegister()
{
	g_AnimSceneManager->m_sceneEditor.OnGameEntityRegister();
}


void  CAnimSceneManager::NewSceneNameChanged()
{
	// Validate the typed in name
	bool bValidSceneName = IsSceneNameValid(sm_NewSceneName.GetCStr());

	if (bValidSceneName)
	{
		atString newName(CAnimScene::sm_LoadSaveDirectory.GetCStr());
		newName += "/";
		newName += sm_NewSceneName.GetCStr();
		newName += ".pso.meta";

		// TODO JA REMOVE TEMP CODE
		// This code will allow new and old style animscenes to co-exist for one week until they have all been moved over
		atString searchText("@");
		if (newName.IndexOf(searchText)>0)
		{
			newName.Replace("@", "/@");
		}
		else
		{
			newName.Replace("_", "/");
		}
		sm_NewScenePathText.SetFromString(newName);
	}
	else
	{
		sm_NewScenePathText.SetFromString("");
	}
}

bool CAnimSceneManager::IsSceneNameValid(const char* pSceneName)
{
	if (pSceneName==NULL)
	{
		return false;
	}

	// Length check
	const int iTextLength = (int)strlen(pSceneName);
	if (iTextLength <= 0)
	{
		return false; // Too short.  Nothing typed in?
	}

	// Check characters are alphanumeric or '@' only.  Check that '@' does not appear twice in a row.
	int iLastAtSymbolPos = -99;	
	for (int i = 0; i < iTextLength; i++)
	{
		bool bIsAtSymbol = pSceneName[i] == '@';
		bool bIsAlphaNumeric = isalnum(pSceneName[i]) != 0 || pSceneName[i] == '_';

		if (bIsAtSymbol)
		{
			if (iLastAtSymbolPos == (i-1))
			{
				return false; // Two '@' in a row
			}
			else
			{
				iLastAtSymbolPos = i;
			}
		}

		if (!bIsAlphaNumeric && !bIsAtSymbol)
		{
			return false; // Illegal character
		}
	}

	return true;
}

#endif //__BANK

#if !__NO_OUTPUT
atString CAnimSceneManager::GetSceneDescription(CAnimScene* pScene, CAnimSceneManager::InstId scene /* = ANIM_SCENE_INST_INVALID*/ )
{
	return atVarString("%d(%p):%s(%s)", scene, pScene, pScene ? pScene->GetName().GetCStr() : "none", pScene ? pScene->GetStateName() : "");
}

atString CAnimSceneManager::GetSceneDescription(CAnimSceneManager::InstId scene)
{
	CAnimScene* pScene = g_AnimSceneManager->GetAnimScene(scene);

	return GetSceneDescription(pScene, scene);
}
#endif // !__NO_OUTPUT 
