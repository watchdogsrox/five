// 
// animation/debug/AnimDebug.cpp 
// 
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved. 
// 

#if __BANK
#include "animation/debug/AnimDebug.h"

// C includes
#include <functional>

// rage includes
#include "bank/msgbox.h"
#include "bank/slider.h"
#include "cranimation/animtolerance.h"
#include "crclip/clipanimations.h"
#include "crclip/clip.h"
#include "crclip/clipanimation.h"
#include "crmotiontree/nodeanimation.h"
#include "crmotiontree/nodeblendn.h "
#include "crmotiontree/nodeblend.h"
#include "crmotiontree/nodeclip.h"
#include "crmotiontree/nodefilter.h"
#include "crmotiontree/nodeexpression.h"
#include "fwanimation/directorcomponentfacialrig.h"
#include "fwanimation/directorcomponentmotiontree.h"
#include "fwanimation/directorcomponentsyncedscene.h"
#include "fwanimation/animhelpers.h"
#include "grcore/debugdraw.h"
#include "grcore/light.h"
#include "grcore/font.h"
#include "grcore/setup.h"
#include "grblendshapes/manager.h"
#include "profile/profiler.h"
#include "string/stringutil.h"
#include "system/criticalsection.h"
// gta includes
#include "Animation/anim_channel.h"
#include "animation/AnimScene/AnimScene.h"
#include "animation/AnimScene/AnimSceneManager.h"
#include "fwanimation/animdirector.h"
#include "fwanimation/animmanager.h"
#include "debug/DebugScene.h"
#include "ModelInfo/ModelInfo.h"
#include "ModelInfo/ModelInfo_Factories.h"
#include "ModelInfo/PedModelInfo.h"
#include "modelinfo/WeaponModelInfo.h"
#include "Peds/ped.h"
#include "peds/PedFactory.h"
#include "peds/Ped.h"
#include "peds/PedIntelligence.h"
#include "Peds/rendering/pedVariationPack.h"
#include "Renderer/renderer.h"
#include "scene/world/GameWorld.h"
#include "streaming/streaming.h"
#include "system/ControlMgr.h"
#include "system/filemgr.h"
#include "Task/General/TaskBasic.h"
#include "vehicles/VehicleFactory.h"

// Defines

AI_OPTIMISATIONS()
ANIM_OPTIMISATIONS()

const char *eClipContextNames[] = {
	"Select clip source:",
	"Clip Dictionary",
	"Clip Set",
	"Local Path",
};

// debug anim selector static data
atVector<CDebugClipDictionary *> CDebugClipDictionary::m_animDictionary;
atArray<const char*> CDebugClipDictionary::m_animationDictionaryNames;
int CDebugClipDictionary::ms_AssetFolderIndex = 0; 

const char * CDebugClipDictionary::ms_notLoadedMessage = "Anim names failed to load";

atArray<const char*> CDebugPedModelSelector::m_modelNames;

const char * CDebugSelector::ms_emptyMessage = "No items found";
const char * CDebugClipSelector::ms_emptyMessage[] = {"No items found"};

int CDebugClipDictionary::ms_AssetFoldersCount = 0;
const char* CDebugClipDictionary::ms_AssetFolders[CDebugClipDictionary::ms_AssetFoldersMax];

const char * CDebugTimelineControl::ms_modeNames[CDebugTimelineControl::kSliderTypeMax] = { "Phase", "Frame number", "time" };

// Anim dictionary sorting function
class AnimDictPred
{
public:
	bool operator()(const std::vector<CDebugClipDictionary *>::value_type left, const std::vector<CDebugClipDictionary *>::value_type right)
	{
		atString lowerLeft, lowerRight;
		lowerLeft = left->GetName().GetCStr();
		lowerRight = right->GetName().GetCStr();
		lowerLeft.Lowercase();
		lowerRight.Lowercase();

		return strcmp(lowerLeft.c_str(), lowerRight.c_str()) < 0;
	}
};


//////////////////////////////////////////////////////////////////////////
//	Anim dictionary structure methods
//////////////////////////////////////////////////////////////////////////

void CDebugClipDictionary::Initialise(int dictionaryIndex)
{

	const char* p_dictionaryName = fwAnimManager::GetName(strLocalIndex(dictionaryIndex));
	p_name = strStreamingObjectName(p_dictionaryName);
	index = dictionaryIndex;
	m_bAnimsLoaded = false;

}

bool CDebugClipDictionary::PushAssetFolder(const char *szAssetFolder)
{
	USE_DEBUG_ANIM_MEMORY();

	if(szAssetFolder && ms_AssetFoldersCount < ms_AssetFoldersMax)
	{
		int length = (int)strlen(szAssetFolder);
		if(length > 0)
		{
			// Don't add duplicate paths
			for(int i = 0; i < ms_AssetFoldersCount; i ++)
			{
				if(stricmp(ms_AssetFolders[i], szAssetFolder) == 0)
				{
					return false;
				}
			}

			// Copy path
			char *szCopy = rage_new char[length + 1];
			strncpy(szCopy, szAssetFolder, length);
			szCopy[length] = '\0';

			// Lowercase path
			strlwr(szCopy);

			// Add path
			ms_AssetFolders[ms_AssetFoldersCount ++] = szCopy;

			return true;
		}
	}

	return false;
}

void CDebugClipDictionary::DeleteAllAssetFolders()
{
	USE_DEBUG_ANIM_MEMORY();

	for(int i = 0; i < ms_AssetFoldersCount; i ++)
	{
		if(ms_AssetFolders[i])
		{
			delete ms_AssetFolders[i];

			ms_AssetFolders[i] = NULL;
		}
	}

	ms_AssetFoldersCount = 0;
}

void CDebugClipDictionary::InitLevel()
{
	DeleteAllAssetFolders();

	PushAssetFolder("assets:/anim/ingame");
	PushAssetFolder("art:/anim/export_mb");
	PushAssetFolder("assets:/maps/anims");
	PushAssetFolder("art:/ng/anim/export_mb");
}

void CDebugClipDictionary::ShutdownLevel()
{
	USE_DEBUG_ANIM_MEMORY();

	// Get the filenames into in a format that can be used with AddCombo
	for (atVector<CDebugClipDictionary *>::iterator group = m_animDictionary.begin();
		group != m_animDictionary.end(); ++group)
	{
		(*group)->Shutdown();
		delete (*group);
	}

	m_animDictionary.clear();
	m_animationDictionaryNames.clear();

	DeleteAllAssetFolders();
}

void CDebugClipDictionary::Shutdown()
{
	m_bAnimsLoaded = false;
	//animation dictionary index (in the AnimManager)
	index = -1;
	// animation dictionary name
	p_name.Clear();
	// array of animation names (filenames stripped of path and extensions)
	animationStrings.clear();
	// array of pointers to the animation strings (to be used by combo boxes)
	animationNames.clear();
}

bool CDebugClipDictionary::StreamIn()
{
	strLocalIndex animStreamingIndex = fwAnimManager::FindSlot(GetName());
	if (animStreamingIndex != -1)
	{
		//first check if it's been loaded already
		if (CStreaming::HasObjectLoaded(animStreamingIndex,fwAnimManager::GetStreamingModuleId()))
		{
			return true;
		}

		// Is the associated animation dictionary in the image?
		if (CStreaming::IsObjectInImage(animStreamingIndex,fwAnimManager::GetStreamingModuleId()))
		{
			// Try and stream in the animation dictionary
			CStreaming::RequestObject(animStreamingIndex, fwAnimManager::GetStreamingModuleId(), STRFLAG_FORCE_LOAD);
			CStreaming::LoadAllRequestedObjects();
		}

		// Is the animation dictionary streamed in now?
		if (CStreaming::HasObjectLoaded(animStreamingIndex,fwAnimManager::GetStreamingModuleId()))
		{
			return true;
		}
	}

	return false;
}

// PURPOSE: Streams in the animation names and populates the animation names structure
bool CDebugClipDictionary::LoadAnimNames(bool bSort)
{

	//early out if we've already loaded our anims - we don't need to do this again
	if (m_bAnimsLoaded)
		return m_bAnimsLoaded;

	//stream in the animation data for this dictionary

	//Is the associated animation block in the image?
	if (StreamIn())
	{
		// enumerate the anims and save the names to our combo
		int validClipCount = 0;
		if (fwAnimManager::Get(strLocalIndex(index)))
		{

			const int numClips = fwAnimManager::Get(strLocalIndex(index))->GetNumClips();

			for(int clipIndex =0; clipIndex<numClips; ++clipIndex)
			{

				crClip* clip = const_cast<crClip*>(fwAnimManager::Get(strLocalIndex(index))->FindClipByIndex(clipIndex));

				if(clip)
				{
					USE_DEBUG_ANIM_MEMORY();

					const char* p_fullyQualifiedName = clip->GetName();

					// Remove the path name leaving the filename and the extension
					const char* fileNameAndExtension = p_fullyQualifiedName;
					fileNameAndExtension = ASSET.FileName(p_fullyQualifiedName);

					// Remove the extension leaving just the filename
					char fileName[256];
					ASSET.RemoveExtensionFromPath(fileName, 256, fileNameAndExtension);

					// Add the file name to the array of file names
					animationStrings.push_back(atString(&fileName[0]));

					validClipCount++;
				}
			}

			if (validClipCount == 0 || numClips==0)
			{
				USE_DEBUG_ANIM_MEMORY();

				if (numClips==0)
				{
					//Warningf("%s has no clips \n\n", group.p_name);
				}
				else
				{
					//Warningf("%s has clips but no animations \n\n", group.p_name);
				}
				static atString s_emptyString("empty\0");
				animationStrings.push_back(s_emptyString);
			}

			// Get the filenames into in a format that can be used with AddCombo
			for (atVector<atString>::const_iterator nit = animationStrings.begin();
				nit != animationStrings.end(); ++nit)
			{
				animationNames.Grow() = *nit;
			}

			if (animationNames.GetCount()>0)
			{
				if (bSort)
				{
					// Sort the animation names alphabetically
					std::sort(&animationNames[0], &animationNames[0] + animationNames.GetCount(), AlphabeticalSort());
				}
			}
			
			m_bAnimsLoaded = true;
		}
		else
		{
			m_bAnimsLoaded = false;
		}		
	}
	else
	{
		m_bAnimsLoaded = false;
	}

	return m_bAnimsLoaded;
}

void CDebugClipDictionary::LoadClipDictionaries(bool bSort)
{
	USE_DEBUG_ANIM_MEMORY();

	if(m_animDictionary.size() > 0)
		return;

	// fwAnimManager is inherited from CStore<AnimDictionary>
	// where typedef pgDictionary<crAnimation> AnimDictionary

	// For each animation dictionary
	// we add a class object to represent that dictionary
	// The debug dictionary class will lazily stream in the 
	// animation names as necessary
	for (atVector<CDebugClipDictionary *>::iterator group = m_animDictionary.begin();
		group != m_animDictionary.end(); ++group)
	{
		(*group)->Shutdown();
		delete (*group);
	}

	m_animDictionary.clear();
	m_animationDictionaryNames.clear();

	int animDictionaryCount = fwAnimManager::GetSize();
	for (int i = 0; i<animDictionaryCount; i++)
	{
		// Is the current slot valid
		if (fwAnimManager::IsValidSlot(strLocalIndex(i)))
		{	
			// Don't load the anims here anymore, just add the dictionary to the list.
			// We'll do the loading later
			m_animDictionary.push_back(rage_new CDebugClipDictionary(i));
		}
	}

	if (m_animDictionary.size() > 0)
	{
		if (bSort)
		{
			// Sort the animation dictionary names alphabetically
			std::sort(&m_animDictionary[0], &m_animDictionary[0] + m_animDictionary.size(), AnimDictPred());
		}

		// Get the filenames into in a format that can be used with AddCombo
		for (atVector<CDebugClipDictionary *>::iterator group = m_animDictionary.begin();
			group != m_animDictionary.end(); ++group)
		{
			m_animationDictionaryNames.Grow() = (*group)->GetName().GetCStr();
		}
	}
}

bool CDebugClipDictionary::StreamDictionary(int dictionaryIndex, bool bSort)
{
	if (!ClipDictionariesLoaded())
		LoadClipDictionaries(bSort);

	return m_animDictionary[dictionaryIndex]->StreamIn();
}

bool CDebugClipDictionary::SaveClipToDisk(crClip* pClip, const char * dictionaryName, const char * animName)
{
	const char *pLocalPath = CDebugClipDictionary::FindLocalDirectoryForDictionary(dictionaryName);
	animAssert(pLocalPath);
	if(pLocalPath)
	{
		char clipFileNameAndPath[512];
		sprintf(clipFileNameAndPath, "%s\\%s.clip", pLocalPath, animName);
		Displayf("Successfully found %s\n", clipFileNameAndPath);

		crClip * pLoadedClip = crClip::AllocateAndLoad(clipFileNameAndPath, NULL);
		if (pLoadedClip)
		{
			Displayf("Successfully loaded %s\n", clipFileNameAndPath);

			pLoadedClip->GetProperties()->RemoveAllProperties();
			pLoadedClip->GetProperties()->CopyProperties(*pClip->GetProperties());

			pLoadedClip->GetTags()->RemoveAllTags();
			pLoadedClip->GetTags()->CopyTags(*pClip->GetTags());
			
			if (pLoadedClip->Save(clipFileNameAndPath))
			{
				Displayf("Successfully saved %s\n", clipFileNameAndPath);
			}
			else
			{
				char tempString[512];
				sprintf(tempString, "%s failed to save! Is it checked out of perforce.", clipFileNameAndPath);
				bkMessageBox("Animviewer debug", tempString, bkMsgOk, bkMsgInfo);
			}

			delete pLoadedClip;
		}
		else
		{
			char tempString[512];
			sprintf(tempString, "%s failed to load! Have you got latest from perforce.", clipFileNameAndPath);
			bkMessageBox("Animviewer debug", tempString, bkMsgOk, bkMsgInfo);
		}
	}
	else
	{
		char tempString[512];
		sprintf(tempString, "%s failed to find clip for this dictionary!", dictionaryName);
		bkMessageBox("Animviewer debug", tempString, bkMsgOk, bkMsgInfo);
	}
	
	return false;
}

// PURPOSE
//	Searches the local directory structure trying to find a match for the supplied dictionary
//	e.g. amb@drink_bottle becomes "...\amb@\drink_\bottle\". Searches the asset folders in
//	reverse order so most recent DLC is prioritized
const char *CDebugClipDictionary::FindLocalDirectoryForDictionary(const char *dictionaryName)
{
	for(int i = ms_AssetFoldersCount - 1; i >= 0; i --)
	{
		const char *localDirectory = FindLocalDirectoryForDictionary(ms_AssetFolders[i], dictionaryName);
		if(localDirectory && strlen(localDirectory) > 0)
		{
			ms_AssetFolderIndex = i;
			return localDirectory;
		}
	}
	return "";
}

const char *CDebugClipDictionary::FindLocalDirectoryForDictionary(const char *localPath, const char *dictName)
{
	char buf[255];

	atArray<fiFindData*> results;

	//This method is getting stuck in a recursive loop when the dictionary name is not found
	//add in a guard to check for this
	static u32 depth = 0;
	const char * result;
	depth++;

	if(strlen(dictName)==0)
	{
		depth--;
		return StringDuplicate(localPath);
	}

	ASSET.EnumFiles(localPath, FindFileCallback, &results);

	const char *longestMatch = "";
	u32 longestLength = 0;

	for(s32 i = 0 ; i < results.GetCount(); i++)
	{
		if(results[i]->m_Attributes & FILE_ATTRIBUTE_DIRECTORY && strcmp(results[i]->m_Name, ".") && strcmp(results[i]->m_Name, ".."))
		{
			char *pathElem = results[i]->m_Name;

			while(*pathElem=='\\' || *pathElem=='/')
				pathElem++;

			// compare pathElem with the start of dictName;

			const char *dictPtr = dictName;
			char *pathElemPtr = pathElem;
			while(*pathElemPtr && *dictPtr)
			{
				if(toupper(*dictPtr) != toupper(*pathElemPtr))
				{
					break;
				}
				dictPtr++;
				pathElemPtr++;
			}

			if(!*pathElemPtr && strlen(pathElem) > longestLength )
			{
				longestLength = ustrlen(pathElem);
				longestMatch = pathElem;
			}
		}
	}

	sprintf(buf, "%s\\%s", localPath, longestMatch);

	for(s32 i = 0 ; i < results.GetCount(); i++)
	{
		delete results[i];
	}

	if (depth>50) //we've gone too deep, bail before we run out of memory
	{
		depth--;
		return "";
	}
	else
	{	
		result =  FindLocalDirectoryForDictionary(buf, dictName+longestLength);
		depth--;
		return result;
	}	
}

void CDebugClipDictionary::FindFileCallback(const fiFindData &data, void *user)
{
	USE_DEBUG_ANIM_MEMORY();

	animAssert(user);
	atArray<fiFindData*> *results = reinterpret_cast<atArray<fiFindData*>*>(user);
	results->PushAndGrow(rage_new fiFindData(data));
}

void CDebugClipDictionary::IterateClips(CDebugClipDictionary::CClipImageIteratorCallback callback)
{
	for (int i=0; i<fwAnimManager::GetNumUsedSlots(); i++)
	{
		fwClipDictionaryLoader loader(i);
		if(loader.IsLoaded())
		{
			crClipDictionary* pDict = loader.GetDictionary();
			if(pDict)
			{
				//Displayf("%s", fwAnimManager::GetName(i));


				for (u32 j=0; j<pDict->GetNumClips(); j++)
				{

					crClip* pClip = pDict->FindClipByIndex(j);
					if (pClip)
					{
						callback(pClip, pDict, strLocalIndex(i));
					}
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
//	Common Debug selector methods
//////////////////////////////////////////////////////////////////////////

void CDebugSelector::AddWidgets(bkBank* pBank, const char * widgetTitle, datCallback selectionChangedCallback, bool bLoadNamesIfNeeded)
{
	//check the model names have been loaded
	if (bLoadNamesIfNeeded && GetList().GetCount()==0)
	{
		LoadNames();
	}

	if (GetList().GetCount()>0)
	{
		//add the combo box widget to the bank
		m_pCombos.PushAndGrow(pBank->AddCombo(widgetTitle, &m_selectedIndex, GetList().GetCount(), &((GetList())[0]),
			datCallback(MFA(CDebugSelector::Select), (datBase*)this)));
	}
	else
	{
		//add the combo box widget to the bank
		m_pCombos.PushAndGrow(pBank->AddCombo(widgetTitle, &m_selectedIndex, 1, &ms_emptyMessage,
			datCallback(MFA(CDebugSelector::Select), (datBase*)this)));
	}


	m_selectionChangedCallback = selectionChangedCallback;
}

void CDebugSelector::RemoveWidgets()
{
	for (int i=0; i<m_pCombos.GetCount(); i++)
	{
		m_pCombos[i]->Destroy();
	}
	m_pCombos.clear();
}

const char *CDebugSelector::GetSelectedName() const
{
	if(m_selectedIndex>=0 && m_selectedIndex < GetConstList().GetCount())
	{
		return (GetConstList())[m_selectedIndex];
	}
	else
	{
		return ""; 
	}
}

void CDebugSelector::Select()
{

	SelectionChanged();

	//make the custom callback if one has been specified
	if (m_selectionChangedCallback.GetType() != datCallback::NULL_CALLBACK)
	{
		m_selectionChangedCallback.Call();
	}
}

void CDebugSelector::Select(const char * itemName, bool doCallback)
{
	if (GetList().GetCount()==0)
	{
		LoadNames();
	}

	m_selectedIndex = FindIndex(itemName);

	if (doCallback)
	{
		SelectionChanged();
	}
}

s32 CDebugSelector::FindIndex(const char * itemName)
{
	atArray<const char *>& list = GetList();

	for (s32 i=0; i<list.GetCount(); i++)
	{
		if (!stricmp(list[i], itemName))
		{
			return i;
		}
	}

	return -1;
}

void CDebugSelector::UpdateCombo()
{
	if (GetList().GetCount()>0)
	{
		m_selectedIndex = Min(m_selectedIndex, GetList().GetCount() - 1);
	for (int i=0; i< m_pCombos.GetCount(); i++)
	{
			m_pCombos[i]->UpdateCombo(m_pCombos[i]->GetTitle(), &m_selectedIndex,
				GetList().GetCount(), 
				&(GetList()[0]), 
				*m_pCombos[i]->GetCallback(),
				m_pCombos[i]->GetTooltip());
		}
	}
		else
		{
		m_selectedIndex = 0;
		for (int i=0; i< m_pCombos.GetCount(); i++)
		{
			m_pCombos[i]->UpdateCombo(m_pCombos[i]->GetTitle(), &m_selectedIndex,
				1, 
				&ms_emptyMessage, 
				*m_pCombos[i]->GetCallback(),
				m_pCombos[i]->GetTooltip());
		}
	}
}

//default selection changed implementation - just calls the registered callback if there is one 
void CDebugSelector::SelectionChanged()
{
	if (m_selectionChangedCallback.GetType()!=datCallback::NULL_CALLBACK)
	{
		m_selectionChangedCallback.Call();
	}
}

//////////////////////////////////////////////////////////////////////////
//	Debug file selector
//////////////////////////////////////////////////////////////////////////

void CDebugFileSelector::AddWidgets(bkBank* pBank, const char * widgetTitle, datCallback selectionChangedCallback, bool bLoadNames)
{
	CDebugSelector::AddWidgets(pBank,widgetTitle, selectionChangedCallback, bLoadNames);

	//add an additional button to rescan the source directory
	pBank->AddButton("rescan", datCallback(MFA(CDebugFileSelector::LoadNames), (datBase*)this), "Reloads the list of files");
}

void CDebugFileSelector::SelectionChanged()
{
	if (HasSelection())
	{
		//fill in the  full name and path of the selected file
		m_selectedFile.Clear();

		m_selectedFile += m_searchDirectory;
		m_selectedFile += "/";
		m_selectedFile += m_fileNames[m_selectedIndex];
	}

	//call the parent implementation to handle any callbacks
	CDebugSelector::SelectionChanged();
}

void CDebugFileSelector::LoadNames()
{
	m_fileStrings.clear();
	m_fileNames.clear();

	//add the (no selection) option
	m_fileStrings.PushAndGrow(atString("---Select file---"));

	atArray<fiFindData*> results;

	if(strlen(m_searchDirectory)==0)
		return;

	if (ASSET.Exists(m_searchDirectory,""))
	{
		ASSET.EnumFiles(m_searchDirectory, CDebugClipDictionary::FindFileCallback, &results);

		for ( int i=0; i<results.GetCount(); i++ )
		{
			atString fileName( results[i]->m_Name );
			
			if (m_extension==NULL || fileName.EndsWith(m_extension))
			{
				m_fileStrings.PushAndGrow(fileName);
			}
		}
	}

	for(int i = 0; i < m_fileStrings.GetCount(); i ++)
	{
		m_fileNames.PushAndGrow(m_fileStrings[i].c_str());
	}

	// if the widget has already been created, update the combo box contents
	if (m_pCombos.GetCount()>0)
	{
		m_selectedIndex = 0;
		UpdateCombo();
		Select();
	}
}

void CDebugFileSelector::DefaultFileCallback(const fiFindData &data, void *user)
{
	USE_DEBUG_ANIM_MEMORY();

	animAssert(user);
	atArray<fiFindData*> *results = reinterpret_cast<atArray<fiFindData*>*>(user);
	results->PushAndGrow(rage_new fiFindData(data));
}


//////////////////////////////////////////////////////////////////////////
//	Debug clip set selector
//////////////////////////////////////////////////////////////////////////

void CDebugClipSetSelector::RefreshList()
{
	LoadNames();
	UpdateCombo();
}

void CDebugClipSetSelector::LoadNames()
{
	//add the (no selection) option
	//static const char * s_noClipSetString = "---Select clip set---";
	//m_Names.PushAndGrow(s_noClipSetString);
	
	m_Names.clear();

	atArray<const char *>& clipSetNames = fwClipSetManager::GetClipSetNames();
	for ( int i=0; i<clipSetNames.GetCount(); i++ )
	{
		atString searchText(m_searchText);
		atString clipSetName(clipSetNames[i]);
		searchText.Lowercase(); 
		clipSetName.Lowercase();
		if (searchText.length()==0 || (searchText.length()>0 && clipSetName.IndexOf(searchText)>=0))
		{
			m_Names.PushAndGrow(clipSetNames[i]);
		}
	}

	if (m_Names.GetCount()>1)
	{
		// Sort the  names alphabetically
		std::sort(&m_Names[0], &m_Names[0] + m_Names.GetCount(), AlphabeticalSort());
	}
}

//////////////////////////////////////////////////////////////////////////
//	Debug animscene selector
//////////////////////////////////////////////////////////////////////////

void CDebugAnimSceneSelector::RefreshList()
{
	LoadNames();
	UpdateCombo();
}

void CDebugAnimSceneSelector::LoadNames()
{
	USE_DEBUG_ANIM_MEMORY();

	m_Names.clear();

	atArray<const char *>& animSceneNames = g_AnimSceneManager->GetAnimSceneNames();

	for ( int i=0; i<animSceneNames.GetCount(); i++ )
	{
		atString searchText(m_searchText);
		atString clipSetName(animSceneNames[i]);
		searchText.Lowercase(); 
		clipSetName.Lowercase();
		if (searchText.length()==0 || (searchText.length()>0 && clipSetName.IndexOf(searchText)>=0))
		{
			if (m_bShowDeprecatedAnimScenes)
			{
				m_Names.PushAndGrow(animSceneNames[i]);
			}
			else
			{
				atString searchLetter("@");
				if (clipSetName.IndexOf(searchLetter)>=0)
				{
					m_Names.PushAndGrow(animSceneNames[i]);
				}
			}
			
		}
	}

	if (m_Names.GetCount()>1)
	{
		// Sort the names alphabetically
		std::sort(&m_Names[0], &m_Names[0] + m_Names.GetCount(), AlphabeticalSort());
	}
}

//////////////////////////////////////////////////////////////////////////
//	Debug anim scene entity selector functionality
//////////////////////////////////////////////////////////////////////////

void CDebugAnimSceneEntitySelector::RebuildEntityList()
{
	m_entityNames.clear();
	m_entityNames.PushAndGrow("No Entity");
	animAssert(m_pScene);
	if (m_pScene)
	{
		m_pScene->GetEntityNames(m_entityNames);
		m_selectedEntityIndex=0;
	}

	CAnimSceneEntity::Id selectedId;
	
	if (HasSelection())
	{
		selectedId = GetSelectedEntityName();
	}

	if (m_pEntityCombo)
	{
		m_pEntityCombo->UpdateCombo("Entity:", &m_selectedEntityIndex, m_entityNames.GetCount(), &m_entityNames[0], 0,  datCallback(MFA(CDebugAnimSceneEntitySelector::OnWidgetChanged),this));
	}

	// set the selected entity appropriately
	if (selectedId.GetHash()!=0)
	{
		Select(selectedId);
	}
}

void CDebugAnimSceneEntitySelector::AddWidgets(bkBank& bank, CAnimScene* pScene, EntityChangedCB callback, const char * label /* =NULL*/)
{
	animAssertf(pScene, "Anim scene entity debug selector must have a valid scene!");
	if(pScene)
	{
		m_pScene = pScene;
		m_callback = callback;
		bank.AddButton("Refresh entity list", datCallback(MFA1(CDebugAnimSceneEntitySelector::RebuildEntityList),this, m_pScene));
		m_pEntityCombo = bank.AddCombo(label==NULL ? "Entity:" : label, &m_selectedEntityIndex, 0, NULL, datCallback(MFA(CDebugAnimSceneEntitySelector::OnWidgetChanged),this));
		RebuildEntityList();
	}
}

void CDebugAnimSceneEntitySelector::OnWidgetChanged()
{
	// call the functor
	if (m_callback)
	{
		m_callback();
	}
}

bool CDebugAnimSceneEntitySelector::Select(CAnimSceneEntity::Id entityName)
{
	// clear out if the hash is null
	if (entityName.GetHash()==0)
	{
		m_selectedEntityIndex=0;
		return true;
	}

	// find the entry in the array
	for (s32 i=0; i<m_entityNames.GetCount(); i++) 
	{
		CAnimSceneEntity::Id name(m_entityNames[i]);
		if (name.GetHash()==entityName.GetHash())
		{
			m_selectedEntityIndex=i;
			return true;
		}
	}

	// couldn't find the entry
	return false;
}

void CDebugAnimSceneEntitySelector::ForceComboBoxRedraw()
{
	if(m_pEntityCombo)
	{
		//[B*2067991] Did this to get around m_pEntityCombo not updating when we recreate them after a selection
		//has been made (and to force it to update is a protected method)
		m_pEntityCombo->UpdateCombo("Entity:", 
			&m_selectedEntityIndex, 
			m_entityNames.GetCount(), 
			&m_entityNames[0], 
			0,  
			datCallback(MFA(CDebugAnimSceneEntitySelector::OnWidgetChanged),
			this));
	}
}

//////////////////////////////////////////////////////////////////////////
//	Debug anim scene section selector functionality
//////////////////////////////////////////////////////////////////////////

const char * CDebugAnimSceneSectionSelector::sm_FromTimeMarkerString("*");

void CDebugAnimSceneSectionSelector::LoadNames()
{
	m_names.clear();
	m_names.PushAndGrow(sm_FromTimeMarkerString);
	animAssert(m_pScene);
	if (m_pScene)
	{
		CAnimSceneSectionIterator it(*m_pScene);

		while (*it)
		{
			m_names.PushAndGrow(it.GetId().GetCStr());
			++it;
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//	Debug Clip selector control implementation
//////////////////////////////////////////////////////////////////////////

CDebugClipSelector::~CDebugClipSelector()
{
	USE_DEBUG_ANIM_MEMORY();

	delete[] m_searchText; m_searchText = NULL;

	if(m_clipContextNameCount > 0)
	{
		delete[] m_clipContextNames; m_clipContextNames = NULL;
		m_clipContextNameCount = 0;
	}

	if(m_clipDictionaryNameCount > 0)
	{
		for(int clipDictionaryNameIndex = 0; clipDictionaryNameIndex < m_clipDictionaryNameCount; clipDictionaryNameIndex ++)
		{
			delete[] m_clipDictionaryNames[clipDictionaryNameIndex]; m_clipDictionaryNames[clipDictionaryNameIndex] = NULL;
		}

		delete[] m_clipDictionaryNames; m_clipDictionaryNames = NULL;
		m_clipDictionaryNameCount = 0;
	}

	if(m_clipDictionaryClipNameCount > 0)
	{
		for(int clipItemNameIndex = 0; clipItemNameIndex < m_clipDictionaryClipNameCount; clipItemNameIndex ++)
		{
			delete[] m_clipDictionaryClipNames[clipItemNameIndex]; m_clipDictionaryClipNames[clipItemNameIndex] = NULL;
		}

		delete[] m_clipDictionaryClipNames; m_clipDictionaryClipNames = NULL;
		m_clipDictionaryClipNameCount = 0;
	}

	if(m_clipSetNameCount > 0)
	{
		for(int clipSetNameIndex = 0; clipSetNameIndex < m_clipSetNameCount; clipSetNameIndex ++)
		{
			delete[] m_clipSetNames[clipSetNameIndex]; m_clipSetNames[clipSetNameIndex] = NULL;
		}

		delete[] m_clipSetNames; m_clipSetNames = NULL;
		m_clipSetNameCount = 0;
	}

	if(m_clipSetClipNameCount > 0)
	{
		for(int clipItemNameIndex = 0; clipItemNameIndex < m_clipSetClipNameCount; clipItemNameIndex ++)
		{
			delete[] m_clipSetClipNames[clipItemNameIndex]; m_clipSetClipNames[clipItemNameIndex] = NULL;
		}

		delete[] m_clipSetClipNames; m_clipSetClipNames = NULL;
		m_clipSetClipNameCount = 0;
	}

	delete[] m_localFile; m_localFile = NULL;
	delete[] m_localFileLoaded; m_localFileLoaded = NULL;
}

bool CDebugClipSelector::HasSelection() const
{
	switch(m_clipContext)
	{
	case kClipContextNone:
		{
			return false;
		}
	case kClipContextClipDictionary:
		{
			if(m_clipDictionaryIndex >= 0 && m_clipDictionaryIndex < m_clipDictionaryNameCount)
			{
				if(m_clipDictionaryClipIndex >= 0 && m_clipDictionaryClipIndex < m_clipDictionaryClipNameCount)
				{
					return true;
				}
			}
		} break;
	case kClipContextAbsoluteClipSet:
		{
			if(m_clipSetIndex >= 0 && m_clipSetIndex < m_clipSetNameCount)
			{
				if(m_clipSetClipIndex >= 0 && m_clipSetClipIndex < m_clipSetClipNameCount)
				{
					return true;
				}
			}
		} break;
	case kClipContextLocalFile:
		{
			if(m_localFileClip != NULL)
			{
				return true;
			}
		} break;
	default:
		{
		} break;
	}

	return false;
}

void CDebugClipSelector::SelectClip(int clipDictionaryIndex, int clipIndex)
{
	if(m_clipContext != kClipContextClipDictionary)
	{
		for(m_clipContextIndex = 0; m_clipContextIndex < m_clipContextNameCount; m_clipContextIndex ++)
		{
			if(m_clipContextNames[m_clipContextIndex] == eClipContextNames[kClipContextClipDictionary])
			{
				break;
			}
		}
		OnClipContextSelected();
	}

	crClipDictionary *pClipDictionary = NULL;

	m_clipDictionaryIndex = 0;

	if(g_ClipDictionaryStore.IsValidSlot(strLocalIndex(clipDictionaryIndex)))
	{
		pClipDictionary = g_ClipDictionaryStore.Get(strLocalIndex(clipDictionaryIndex));

		const char *clipDictionaryName = g_ClipDictionaryStore.GetName(strLocalIndex(clipDictionaryIndex));
		if(clipDictionaryName)
		{
			u32 clipDictionaryHash = atStringHash(clipDictionaryName);
			for(int index = 0; index < m_clipDictionaryNameCount; index ++)
			{
				u32 hash = atStringHash(m_clipDictionaryNames[index]);
				if(clipDictionaryHash == hash)
				{
					m_clipDictionaryIndex = index;

					break;
				}
			}
		}
	}

	// Update the clip combo - not sure if the callback will work when setting from code
	OnClipDictionarySelected();

	m_clipDictionaryClipIndex = 0;

	if(pClipDictionary)
	{

		if(clipIndex >= 0 && clipIndex < pClipDictionary->GetNumClips())
		{
			const char *clipName = pClipDictionary->FindClipByIndex(clipIndex)->GetName();

			if(clipName)
			{
				u32 clipHash = atStringHash(clipName);
				for(int index = 0; index < m_clipDictionaryClipNameCount; index ++)
				{
					u32 hash = atStringHash(m_clipDictionaryClipNames[index]);
					if(clipHash == hash)
					{
						m_clipDictionaryClipIndex = index;

						break;
					}
				}
			}
		}
	}
}

void CDebugClipSelector::SelectClip(u32 clipDictionaryHash, u32 clipHash)
{
	if(m_clipContext != kClipContextClipDictionary)
	{
		for(m_clipContextIndex = 0; m_clipContextIndex < m_clipContextNameCount; m_clipContextIndex ++)
		{
			if(m_clipContextNames[m_clipContextIndex] == eClipContextNames[kClipContextClipDictionary])
			{
				break;
			}
		}
		OnClipContextSelected();
	}

	crClipDictionary *pClipDictionary = NULL;

	m_clipDictionaryIndex = 0;

	int clipDictionaryIndex = g_ClipDictionaryStore.FindSlotFromHashKey(clipDictionaryHash).Get();
	if(g_ClipDictionaryStore.IsValidSlot(strLocalIndex(clipDictionaryIndex)))
	{
		pClipDictionary = g_ClipDictionaryStore.Get(strLocalIndex(clipDictionaryIndex));

		for(int index = 0; index < m_clipDictionaryNameCount; index ++)
		{
			u32 hash = atStringHash(m_clipDictionaryNames[index]);
			if(clipDictionaryHash == hash)
			{
				m_clipDictionaryIndex = index;

				break;
			}
		}
	}

	// Update the clip combo - not sure if the callback will work when setting from code
	OnClipDictionarySelected();

	m_clipDictionaryClipIndex = 0;

	if(pClipDictionary)
	{
		for(int index = 0; index < m_clipDictionaryClipNameCount; index ++)
		{
			u32 hash = atStringHash(m_clipDictionaryClipNames[index]);
			if(clipHash == hash)
			{
				m_clipDictionaryClipIndex = index;

				break;
			}
		}
	}
}

void CDebugClipSelector::SetCachedClip(u32 clipDictionaryHash, u32 clipHash)
{
	m_bUseCachedClip = true;
	m_cachedClipDictionaryHash = clipDictionaryHash;
	m_cachedClipHash = clipHash;
}


bool CDebugClipSelector::StreamSelectedClipDictionary()
{
	if(m_clipDictionaryIndex >= 0 && m_clipDictionaryIndex < m_clipDictionaryNameCount)
	{
		const char *clipDictionaryName = m_clipDictionaryNames[m_clipDictionaryIndex];
		if(clipDictionaryName)
		{
			int clipDictionaryIndex;
			CDebugClipDictionary *pDebugClipDictionary = CDebugClipDictionary::FindClipDictionary(atStringHash(clipDictionaryName), clipDictionaryIndex);
			if(pDebugClipDictionary)
			{
				return pDebugClipDictionary->StreamIn();
			}
		}
	}

	return false;
}

crClip* CDebugClipSelector::GetSelectedClip() const
{
	if(!HasSelection())
	{
		return NULL;
	}

	switch(m_clipContext)
	{
	case kClipContextNone:
		{
			return NULL;
		}
	case kClipContextClipDictionary:
		{
			crClip *clip = NULL;

			int clipDictionaryIndex = g_ClipDictionaryStore.FindSlot(m_clipDictionaryNames[m_clipDictionaryIndex]).Get();

			Assert(clipDictionaryIndex != -1);

			fwClipDictionaryLoader loader(clipDictionaryIndex);
			if(loader.IsLoaded())
			{
				crClipDictionary* clipDictionary = loader.GetDictionary();
				if (clipDictionary)
				{
					clip = clipDictionary->GetClip(m_clipDictionaryClipNames[m_clipDictionaryClipIndex]);
				}
			}

			return clip;
		}
	case kClipContextAbsoluteClipSet:
		{
			fwMvClipSetId clipSetId(m_clipSetNames[m_clipSetIndex]);
			fwMvClipId clipId(m_clipSetClipNames[m_clipSetClipIndex]);
			return fwClipSetManager::GetClip(clipSetId, clipId);
		}
	case kClipContextLocalFile:
		{
			return m_localFileClip;
		}
	default:
		{
		} break;
	}

	return NULL;
}

void CDebugClipSelector::AddWidgets(bkGroup* pParentGroup, datCallback clipDictionaryCallback, datCallback clipCallback)
{
	bkBank* pParentBank = FindBank(pParentGroup);
	if (m_WidgetsMap.Access(pParentBank) != NULL)
	{
		RemoveWidgets(pParentBank);
	}

	if(m_WidgetsMap.Access(pParentBank) == NULL)
	{
		Widgets &widgets = m_WidgetsMap.Insert(pParentBank).data;

		if(m_clipContextNameCount == 0)
		{
			USE_DEBUG_ANIM_MEMORY();

			m_clipContextNameCount += 1; // for context none (this always exists, but crucially loads no dictionaries)
			m_clipContextNameCount += m_bEnableClipDictionaries ? 1 : 0;
			m_clipContextNameCount += m_bEnableClipSets ? 1 : 0;
			m_clipContextNameCount += m_bEnableLocalPaths ? 1 : 0;

			Assert(m_clipContextNameCount > 0);

			m_clipContextNames = rage_new const char *[m_clipContextNameCount];

			int index = 0;
			m_clipContextNames[index ++] = eClipContextNames[kClipContextNone];
			if(m_bEnableClipDictionaries) { m_clipContextNames[index ++] = eClipContextNames[kClipContextClipDictionary]; }
			if(m_bEnableClipSets) { m_clipContextNames[index ++] = eClipContextNames[kClipContextAbsoluteClipSet]; }
			if(m_bEnableLocalPaths) { m_clipContextNames[index ++] = eClipContextNames[kClipContextLocalFile]; }

			m_clipContextIndex = 0;
			for(m_clipContext = 0; m_clipContext < kNumClipContext; m_clipContext ++)
			{
				if(m_clipContextNames[m_clipContextIndex] == eClipContextNames[m_clipContext])
				{
					break;
				}
			}
			Assert(m_clipContext < kNumClipContext);
		}

		LoadClipDictionaryNames();
		LoadClipDictionaryClipNames();

		LoadClipSetNames();
		LoadClipSetClipNames();
		
		//if there's no clip dictionary names, we need to load them up
		if (!CDebugClipDictionary::ClipDictionariesLoaded())
		{
			CDebugClipDictionary::LoadClipDictionaries();
			animAssertf (CDebugClipDictionary::ClipDictionariesLoaded(), "The clip dictionary names aren't loaded!");
		}

		widgets.m_clipDictionaryCallback = clipDictionaryCallback;
		widgets.m_clipCallback = clipCallback;

		widgets.m_pGroup = pParentGroup->AddGroup("Clip Selector");

		// If we're using cached data, then use it now
		if (m_bUseCachedClip)
		{
			m_clipContextIndex = GetClipContextIndexFromClipContext(kClipContextClipDictionary);
			if (m_clipContextIndex<0)
			{
				m_clipContextIndex = 0;
			}
			else
			{
				m_clipContext = kClipContextClipDictionary;
			}
		}

		// Add a combo box containing the names of all of the clip contexts
		widgets.m_pClipContextCombo = widgets.m_pGroup->AddCombo("Clip Context",
			&m_clipContextIndex,
			m_clipContextNameCount,
			m_clipContextNames,
			datCallback(MFA(CDebugClipSelector::OnClipContextSelected), (datBase*)this),
			"List the clip contexts");

		switch(m_clipContext)
		{
		case kClipContextNone:
			{
				//add no widgets
				
			} break;
		case kClipContextAbsoluteClipSet:
			{
				// Add a text box for searching
				widgets.m_pSearchText = widgets.m_pGroup->AddText("Search:",
					m_searchText,
					m_searchTextLength,
					datCallback(MFA(CDebugClipSelector::OnSearchTextChanged), (datBase*)this));

				// Add a combo box containing the names of all of the clip sets
				widgets.m_pClipSetCombo = widgets.m_pGroup->AddCombo("Clip Set",
					&m_clipSetIndex,
					m_clipSetNameCount,
					m_clipSetNames,
					datCallback(MFA(CDebugClipSelector::OnClipSetSelected), (datBase*)this),
					"Lists the clip sets");

				// Add a combo box containing the names of all of the clips
				widgets.m_pClipCombo = widgets.m_pGroup->AddCombo("Clip",
					&m_clipSetClipIndex,
					m_clipSetClipNameCount,
					m_clipSetClipNames,
					NullCB,
					"Lists the clips contained in the above clip dictionary");
			} break;
		case kClipContextClipDictionary:
			{
				// Add a text box for searching
				widgets.m_pSearchText = widgets.m_pGroup->AddText("Search:",
					m_searchText,
					m_searchTextLength,
					datCallback(MFA(CDebugClipSelector::OnSearchTextChanged), (datBase*)this));

				// We have cached data to use for the clip dictionary
				if (m_bUseCachedClip)
				{
					LoadClipDictionaryNames();
					m_clipDictionaryIndex = 0;

					strLocalIndex clipDictionaryIndex = strLocalIndex(g_ClipDictionaryStore.FindSlotFromHashKey(m_cachedClipDictionaryHash));
					if(clipDictionaryIndex != -1 && g_ClipDictionaryStore.IsValidSlot(clipDictionaryIndex))
					{
						for(int index = 0; index < m_clipDictionaryNameCount; index ++)
						{
							u32 hash = atStringHash(m_clipDictionaryNames[index]);
							if(m_cachedClipDictionaryHash == hash)
							{
								m_clipDictionaryIndex = index;
								break;
							}
						}
					}
				}

				// Add a combo box containing the names of all of the clip dictionaries
				widgets.m_pClipDictionaryCombo = widgets.m_pGroup->AddCombo("Clip Dictionary",
					&m_clipDictionaryIndex,
					m_clipDictionaryNameCount,
					m_clipDictionaryNames,
					datCallback(MFA(CDebugClipSelector::OnClipDictionarySelected), (datBase*)this),
					"Lists the clip dictionaries");

				// We have cached data to use for the clip dictionary
				if (m_bUseCachedClip)
				{
					LoadClipDictionaryClipNames();
					m_clipDictionaryClipIndex = 0;

					strLocalIndex clipDictionaryIndex = strLocalIndex(g_ClipDictionaryStore.FindSlotFromHashKey(m_cachedClipDictionaryHash));
					if (clipDictionaryIndex != STRLOCALINDEX_INVALID)
					{
						crClipDictionary *pCachedClipDictionary = g_ClipDictionaryStore.Get(clipDictionaryIndex);

						if (pCachedClipDictionary)
						{
							for (int index = 0; index < m_clipDictionaryClipNameCount; index++)
							{
								u32 hash = atStringHash(m_clipDictionaryClipNames[index]);
								if (m_cachedClipHash == hash)
								{
									m_clipDictionaryClipIndex = index;
									break;
								}
							}
						}
					}
				}

				// Add a combo box containing the names of all of the clips
				widgets.m_pClipCombo = widgets.m_pGroup->AddCombo("Clip",
					&m_clipDictionaryClipIndex,
					m_clipDictionaryClipNameCount,
					m_clipDictionaryClipNames,
					clipCallback,
					"Lists the clips contained in the above clip dictionary");
			} break;
		case kClipContextLocalFile:
			{
				// Add a text box for entering the local file path
				widgets.m_pLocalFileText = widgets.m_pGroup->AddText("Local File:",
					m_localFile,
					m_localFileLength,
					datCallback(MFA(CDebugClipSelector::OnLocalFileChanged), (datBase*)this));

				// Add a text box for displaying the loaded local file path
				widgets.m_pLocalFileLoadedText = widgets.m_pGroup->AddText("Loaded:",
					m_localFileLoaded,
					m_localFileLoadedLength,
					true);
			} break;
		default:
			{
			} break;
		}
	}

	m_bUseCachedClip = false;
}


void CDebugClipSelector::RemoveAllWidgets()
{
	while (m_WidgetsMap.GetNumUsed()>0)
	{
		atMap<bkBank*, Widgets>::Iterator it = m_WidgetsMap.CreateIterator();
		RemoveWidgets(it.GetKey());
	}
}

void CDebugClipSelector::RemoveWidgets(bkBank* pBank)
{
	Widgets *widgets = m_WidgetsMap.Access(pBank);
	if(widgets != NULL)
	{
		if(widgets->m_pGroup)
		{
			widgets->m_pGroup = NULL;
		}

		if(widgets->m_pClipContextCombo)
		{
			widgets->m_pClipContextCombo->Destroy(); widgets->m_pClipContextCombo = NULL;
		}

		if(widgets->m_pSearchText)
		{
			widgets->m_pSearchText->Destroy(); widgets->m_pSearchText = NULL;
		}

		if(widgets->m_pClipSetCombo)
		{
			widgets->m_pClipSetCombo->Destroy(); widgets->m_pClipSetCombo = NULL;
		}

		if(widgets->m_pClipDictionaryCombo)
		{
			widgets->m_pClipDictionaryCombo->Destroy(); widgets->m_pClipDictionaryCombo = NULL;
		}

		if(widgets->m_pClipCombo)
		{
			widgets->m_pClipCombo->Destroy(); widgets->m_pClipCombo = NULL;
		}

		if(widgets->m_pLocalFileText)
		{
			widgets->m_pLocalFileText->Destroy(); widgets->m_pLocalFileText = NULL;
		}

		if(widgets->m_pLocalFileLoadedText)
		{
			widgets->m_pLocalFileLoadedText->Destroy(); widgets->m_pLocalFileLoadedText = NULL;
		}

		m_WidgetsMap.Delete(pBank);
	}
}

void CDebugClipSelector::Shutdown()
{
	RemoveAllWidgets();

	// Scoping - USE_DEBUG_ANIM_MEMORY
	{
	USE_DEBUG_ANIM_MEMORY();

	if(m_clipDictionaryNameCount > 0)
	{
		for(int clipDictionaryNameIndex = 0; clipDictionaryNameIndex < m_clipDictionaryNameCount; clipDictionaryNameIndex ++)
		{
			delete[] m_clipDictionaryNames[clipDictionaryNameIndex]; m_clipDictionaryNames[clipDictionaryNameIndex] = NULL;
		}

		delete[] m_clipDictionaryNames; m_clipDictionaryNames = NULL;
		m_clipDictionaryNameCount = 0;
	}

	if(m_clipDictionaryClipNameCount > 0)
	{
		USE_DEBUG_ANIM_MEMORY();

		for(int clipItemNameIndex = 0; clipItemNameIndex < m_clipDictionaryClipNameCount; clipItemNameIndex ++)
		{
			delete[] m_clipDictionaryClipNames[clipItemNameIndex]; m_clipDictionaryClipNames[clipItemNameIndex] = NULL;
		}

		delete[] m_clipDictionaryClipNames; m_clipDictionaryClipNames = NULL;
		m_clipDictionaryClipNameCount = 0;
	}

	if(m_clipSetNameCount > 0)
	{
		USE_DEBUG_ANIM_MEMORY();

		for(int clipSetNameIndex = 0; clipSetNameIndex < m_clipSetNameCount; clipSetNameIndex ++)
		{
			delete[] m_clipSetNames[clipSetNameIndex]; m_clipSetNames[clipSetNameIndex] = NULL;
		}

		delete[] m_clipSetNames; m_clipSetNames = NULL;
		m_clipSetNameCount = 0;
	}

	if(m_clipSetClipNameCount > 0)
	{
		USE_DEBUG_ANIM_MEMORY();

		for(int clipItemNameIndex = 0; clipItemNameIndex < m_clipSetClipNameCount; clipItemNameIndex ++)
		{
			delete[] m_clipSetClipNames[clipItemNameIndex]; m_clipSetClipNames[clipItemNameIndex] = NULL;
		}

		delete[] m_clipSetClipNames; m_clipSetClipNames = NULL;
		m_clipSetClipNameCount = 0;
	}

	memset(m_searchText, '\0', m_searchTextLength);
	m_clipSetIndex = 0;
	m_clipSetClipIndex = 0;

	m_clipDictionaryIndex = 0;
	m_clipDictionaryClipIndex = 0;
	memset(m_localFile, '\0', sizeof(char) * m_localFileLength);
	memset(m_localFileLoaded, '\0', sizeof(char) * m_localFileLoadedLength);
	m_localFileClip = NULL;
	}

	m_clipSetHelper.Release();
}

int CDebugClipSelector::GetClipContextIndexFromClipContext(int iClipContext)
{
	for(int iContext = 0; iContext < m_clipContextNameCount; iContext ++)
	{
		if(m_clipContextNames[iContext] == eClipContextNames[iClipContext])
		{
			return iContext;
			break;
		}
	}

	return -1;
}

void CDebugClipSelector::OnClipContextSelected()
{
	for(atMap< bkBank *, Widgets >::Iterator It = m_WidgetsMap.CreateIterator(); !It.AtEnd(); It.Next())
	{
		Widgets &widgets = It.GetData();

		if(widgets.m_pSearchText)
		{
			widgets.m_pSearchText->Destroy(); widgets.m_pSearchText = NULL;
		}

		if(widgets.m_pClipSetCombo)
		{
			widgets.m_pClipSetCombo->Destroy(); widgets.m_pClipSetCombo = NULL;
		}

		if(widgets.m_pClipDictionaryCombo)
		{
			widgets.m_pClipDictionaryCombo->Destroy(); widgets.m_pClipDictionaryCombo = NULL;
		}

		if(widgets.m_pClipCombo)
		{
			widgets.m_pClipCombo->Destroy(); widgets.m_pClipCombo = NULL;
		}

		if(widgets.m_pLocalFileText)
		{
			widgets.m_pLocalFileText->Destroy(); widgets.m_pLocalFileText = NULL;
		}

		if(widgets.m_pLocalFileLoadedText)
		{
			widgets.m_pLocalFileLoadedText->Destroy(); widgets.m_pLocalFileLoadedText = NULL;
		}

		for(m_clipContext = 0; m_clipContext < kNumClipContext; m_clipContext ++)
		{
			if(m_clipContextNames[m_clipContextIndex] == eClipContextNames[m_clipContext])
			{
				break;
			}
		}
		Assert(m_clipContext < kNumClipContext);

		int zeroIndex = 0;
		switch(m_clipContext)
		{
		case kClipContextNone:
			{
				//add no widgets
			} break;
		case kClipContextAbsoluteClipSet:
			{
				// Add a text box for searching
				widgets.m_pSearchText = widgets.m_pGroup->AddText("Search:",
					m_searchText,
					m_searchTextLength,
					datCallback(MFA(CDebugClipSelector::OnSearchTextChanged), (datBase*)this));

				LoadClipSetNames();

				// Add a combo box containing the names of all of the clip sets
				widgets.m_pClipSetCombo = widgets.m_pGroup->AddCombo("Clip Set",
					m_clipSetNameCount == 0 ? &zeroIndex : &m_clipSetIndex,
					m_clipSetNameCount == 0 ? 1 : m_clipSetNameCount,
					m_clipSetNameCount == 0 ? ms_emptyMessage : m_clipSetNames,
					datCallback(MFA(CDebugClipSelector::OnClipSetSelected), (datBase*)this),
					"Lists the clip sets");
				m_clipSetIndex = 0;
				LoadClipSetClipNames();
				m_clipSetClipIndex = 0;
				// Add a combo box containing the names of all of the clips
				widgets.m_pClipCombo = widgets.m_pGroup->AddCombo("Clip", 
					m_clipSetClipNameCount == 0 ? &zeroIndex : &m_clipSetClipIndex,
					m_clipSetClipNameCount == 0 ? 1 : m_clipSetClipNameCount,
					m_clipSetClipNameCount == 0 ? ms_emptyMessage : m_clipSetClipNames,
					NullCB,
					"Lists the clips contained in the above clip dictionary");
			} break;
		case kClipContextClipDictionary:
			{
				// Add a text box for entering the local file path
				widgets.m_pSearchText = widgets.m_pGroup->AddText("Search:",
					m_searchText,
					m_searchTextLength,
					datCallback(MFA(CDebugClipSelector::OnSearchTextChanged), (datBase*)this));

				LoadClipDictionaryNames();

				// Add a combo box containing the names of all of the clip dictionaries
				widgets.m_pClipDictionaryCombo = widgets.m_pGroup->AddCombo("Clip Dictionary",
					m_clipDictionaryNameCount == 0 ? &zeroIndex : &m_clipDictionaryIndex,
					m_clipDictionaryNameCount == 0 ? 1 : m_clipDictionaryNameCount,
					m_clipDictionaryNameCount == 0 ? ms_emptyMessage : m_clipDictionaryNames,
					datCallback(MFA(CDebugClipSelector::OnClipDictionarySelected), (datBase*)this),
					"Lists the clip dictionaries");
				m_clipDictionaryIndex = 0;

				LoadClipDictionaryClipNames();

				m_clipDictionaryClipIndex= 0;
				
				// Add a combo box containing the names of all of the clips
				widgets.m_pClipCombo = widgets.m_pGroup->AddCombo("Clip",
					m_clipDictionaryClipNameCount == 0 ? &zeroIndex : &m_clipDictionaryClipIndex,
					m_clipDictionaryClipNameCount == 0 ? 1 : m_clipDictionaryClipNameCount,
					m_clipDictionaryClipNameCount == 0 ? ms_emptyMessage : m_clipDictionaryClipNames,
					widgets.m_clipCallback,
					"Lists the clips contained in the above clip dictionary");
			} break;
		case kClipContextLocalFile:
			{
				// Add a text box for entering the local file path
				widgets.m_pLocalFileText = widgets.m_pGroup->AddText("Local File:",
					m_localFile,
					m_localFileLength,
					datCallback(MFA(CDebugClipSelector::OnLocalFileChanged), (datBase*)this));

				// Adda a text box for displaying the loaded local file path
				widgets.m_pLocalFileLoadedText = widgets.m_pGroup->AddText("Loaded:",
					m_localFileLoaded,
					m_localFileLoadedLength,
					true);
			} break;
		default:
			{
			} break;
		}
	}
}

void CDebugClipSelector::OnSearchTextChanged()
{
	switch(m_clipContext)
	{
	case kClipContextNone:
		{
		} break;
	case kClipContextAbsoluteClipSet:
		{
			LoadClipSetNames();
			//Partially reset so we're pointing to a valid clip (or 'no item found')
			m_clipSetIndex = 0;
			m_clipSetClipIndex = 0;
			OnClipSetSelected();
		} break;
	case kClipContextClipDictionary:
		{
			LoadClipDictionaryNames();
			//Partially reset so we're pointing to a valid clip (or 'no item found')
			m_clipDictionaryIndex = 0;
			m_clipDictionaryClipIndex= 0;
			OnClipDictionarySelected();
		} break;
	case kClipContextLocalFile:
		{
		} break;
	default:
		{
		} break;
	}
}

void CDebugClipSelector::OnClipSetSelected()
{
	for(atMap< bkBank *, Widgets >::Iterator It = m_WidgetsMap.CreateIterator(); !It.AtEnd(); It.Next())
	{
		Widgets &widgets = It.GetData();

		if (widgets.m_pClipSetCombo)
		{
			int zeroIndex = 0;
			widgets.m_pClipSetCombo->UpdateCombo("Clip Set",
				m_clipSetNameCount == 0 ? &zeroIndex : &m_clipSetIndex,
				m_clipSetNameCount == 0 ? 1 : m_clipSetNameCount,
				m_clipSetNameCount == 0 ? ms_emptyMessage : m_clipSetNames,
				datCallback(MFA(CDebugClipSelector::OnClipSetSelected), (datBase*)this),
				"Lists the clip sets");
		}

		// Reset the animation index
		m_clipSetClipIndex = 0;

		if (widgets.m_pClipCombo)
		{
				LoadClipSetClipNames();

				widgets.m_pClipCombo->UpdateCombo("Clip",
					&m_clipSetClipIndex,
				m_clipSetClipNameCount == 0 ? 1 : m_clipSetClipNameCount,
				m_clipSetClipNameCount == 0 ? ms_emptyMessage : m_clipSetClipNames,
					NullCB,
					"Lists the clips contained in the above clip dictionary");
			}

		//make the custom callback if one has been specified
		if (widgets.m_clipDictionaryCallback.GetType() != datCallback::NULL_CALLBACK)
		{
			widgets.m_clipDictionaryCallback.Call();
		}
	}
}

void CDebugClipSelector::OnClipDictionarySelected()
{
	for(atMap< bkBank *, Widgets >::Iterator It = m_WidgetsMap.CreateIterator(); !It.AtEnd(); It.Next())
	{
		Widgets &widgets = It.GetData();

		if (widgets.m_pClipDictionaryCombo)
		{
			int zeroIndex = 0;
			widgets.m_pClipDictionaryCombo->UpdateCombo("Clip Dictionary",
				m_clipDictionaryNameCount == 0 ? &zeroIndex : &m_clipDictionaryIndex,
				m_clipDictionaryNameCount == 0 ? 1 : m_clipDictionaryNameCount,
				m_clipDictionaryNameCount == 0 ? ms_emptyMessage : m_clipDictionaryNames,
				datCallback(MFA(CDebugClipSelector::OnClipDictionarySelected), (datBase*)this),
				"Lists the clip dictionaries");
		}

		// Reset the clip index
		m_clipDictionaryClipIndex = 0;

		if (widgets.m_pClipCombo)
		{
			if (!(m_clipDictionaryIndex >= 0 && m_clipDictionaryIndex < m_clipDictionaryNameCount))
			{
				animAssertf(m_clipDictionaryNameCount == 0, "Invalid clip dictionary index!");
			}
				LoadClipDictionaryClipNames();

				widgets.m_pClipCombo->UpdateCombo("Clip",
					&m_clipDictionaryClipIndex,
				m_clipDictionaryClipNameCount == 0 ? 1 : m_clipDictionaryClipNameCount,
				m_clipDictionaryClipNameCount == 0 ? ms_emptyMessage : m_clipDictionaryClipNames,
					widgets.m_clipCallback,
					"Lists the clips contained in the above clip dictionary");
			}

		//make the custom callback if one has been specified
		if (widgets.m_clipDictionaryCallback.GetType() != datCallback::NULL_CALLBACK)
		{
			widgets.m_clipDictionaryCallback.Call();
		}
	}
}

void CDebugClipSelector::OnLocalFileChanged()
{
	strcpy(m_localFileLoaded, "");
	if(m_localFileClip)
	{
		m_localFileClip->Release(); m_localFileClip = NULL;
	}

	atString filePath(m_localFile);
	filePath.Lowercase();

	// Is clip name a filename ending in .clip?
	bool endsWithDotClip = filePath.EndsWith(".clip");
	if(endsWithDotClip)
	{
		// Tokenize into filename + extension
		atArray< atString > clipNameTokens;
		filePath.Split(clipNameTokens, ".");
		if(clipNameTokens.GetCount() == 2)
		{
			bool assetExists = ASSET.Exists(clipNameTokens[0], clipNameTokens[1]);
			if (assetExists)
			{
				// lossless compression on hot-loaded animations - keep compression techniques cheap
				crAnimToleranceSimple tolerance(SMALL_FLOAT, SMALL_FLOAT, SMALL_FLOAT, SMALL_FLOAT, crAnimTolerance::kDecompressionCostDefault, crAnimTolerance::kCompressionCostLow);

				// load animation directly, compress on load
				crAnimLoaderCompress loader;
				loader.Init(tolerance);

				// WARNING: This is known to be slow and dynamic memory intense.
				// Do NOT profile or bug performance of this call, it is DEV only.
				m_localFileClip = crClip::AllocateAndLoad(filePath.c_str(), &loader);
				Assertf(m_localFileClip, "Cannot retrieve a clip from a local file when the local file will not load! (filePath: %s)", filePath.c_str());

				strcpy(m_localFileLoaded, filePath.c_str());
			}
		}
	}
}

bool CDebugClipSelector::SaveSelectedClipToDisk()
{
	if (HasSelection())
	{
		crClip* pClip = GetSelectedClip();
		if (pClip!=NULL)
		{
			return CDebugClipDictionary::SaveClipToDisk(pClip, GetSelectedClipDictionary(), GetSelectedClipName());
		}
	}
	return false;
}

void CDebugClipSelector::LoadClipDictionaryNames()
{
	if(m_clipDictionaryNameCount > 0)
	{
		USE_DEBUG_ANIM_MEMORY();

		for(int clipDictionaryNameIndex = 0; clipDictionaryNameIndex < m_clipDictionaryNameCount; clipDictionaryNameIndex ++)
		{
			delete[] m_clipDictionaryNames[clipDictionaryNameIndex]; m_clipDictionaryNames[clipDictionaryNameIndex] = NULL;
		}

		delete[] m_clipDictionaryNames; m_clipDictionaryNames = NULL;
		m_clipDictionaryNameCount = 0;
	}

	bool bFilter = m_searchText && strlen(m_searchText) > 0;

	m_clipDictionaryNameCount = 0;
	for(int slotIndex = 0; slotIndex < g_ClipDictionaryStore.GetCount(); slotIndex ++)
	{
		if(g_ClipDictionaryStore.IsValidSlot(strLocalIndex(slotIndex)))
		{
			const char *clipDictionaryName = g_ClipDictionaryStore.GetName(strLocalIndex(slotIndex));
			if(clipDictionaryName && (!bFilter || stristr(clipDictionaryName, m_searchText) != NULL))
			{
				m_clipDictionaryNameCount ++;
			}
		}
	}

	// Reset the dict index
	m_clipDictionaryIndex = -1;
	m_clipDictionaryClipIndex = -1;
	if(m_clipDictionaryNameCount == 0)
		{
		m_clipDictionaryClipNameCount = 0;
	}

	if(m_clipDictionaryNameCount > 0)
	{
		USE_DEBUG_ANIM_MEMORY();

		m_clipDictionaryNames = rage_new const char *[m_clipDictionaryNameCount];

		int clipDictionaryNameIndex = 0;

		for(int slotIndex = 0; slotIndex < g_ClipDictionaryStore.GetCount(); slotIndex ++)
		{
			if(g_ClipDictionaryStore.IsValidSlot(strLocalIndex(slotIndex)))
			{
				const char *clipDictionaryName = g_ClipDictionaryStore.GetName(strLocalIndex(slotIndex));
				if(clipDictionaryName && (!bFilter || stristr(clipDictionaryName, m_searchText) != NULL))
				{
					int clipDictionaryNameLength = istrlen(clipDictionaryName);

					char *tempClipDictionaryName = rage_new char[clipDictionaryNameLength + 1];
					strcpy(tempClipDictionaryName, clipDictionaryName);

					m_clipDictionaryNames[clipDictionaryNameIndex] = tempClipDictionaryName;
					clipDictionaryNameIndex ++;
				}
			}
		}

		std::sort(&m_clipDictionaryNames[0], &m_clipDictionaryNames[m_clipDictionaryNameCount], AlphabeticalSort());
	}
}

void CDebugClipSelector::LoadClipSetNames()
{
	if(m_clipSetNameCount > 0)
	{
		USE_DEBUG_ANIM_MEMORY();

		for(int clipSetNameIndex = 0; clipSetNameIndex < m_clipSetNameCount; clipSetNameIndex ++)
		{
			delete[] m_clipSetNames[clipSetNameIndex]; m_clipSetNames[clipSetNameIndex] = NULL;
		}

		delete[] m_clipSetNames; m_clipSetNames = NULL;
		m_clipSetNameCount = 0;
	}

	bool bFilter = m_searchText && strlen(m_searchText) > 0;
	for(int clipSetIndex = 0; clipSetIndex < fwClipSetManager::GetClipSetCount(); clipSetIndex ++)
	{
		fwMvClipSetId clipSetId = fwClipSetManager::GetClipSetIdByIndex(clipSetIndex);
		const char *clipSetName = clipSetId.GetCStr();
		if(clipSetName && (!bFilter || stristr(clipSetName, m_searchText) != NULL))
		{
			m_clipSetNameCount ++;
		}
	}
	
	//Partially reset so we're pointing to an invalid value 
	m_clipSetIndex = -1;
	m_clipSetClipIndex = -1;
	if(m_clipSetNameCount == 0)
	{
		m_clipSetClipNameCount = 0;
	}

	if(m_clipSetNameCount > 0)
	{
		USE_DEBUG_ANIM_MEMORY();
		m_clipSetNames = rage_new const char *[m_clipSetNameCount];

		int clipSetNameIndex = 0;

		for(int clipSetIndex = 0; clipSetIndex < fwClipSetManager::GetClipSetCount(); clipSetIndex ++)
		{
			fwMvClipSetId clipSetId = fwClipSetManager::GetClipSetIdByIndex(clipSetIndex);
			const char *clipSetName = clipSetId.GetCStr();
			if(clipSetName && (!bFilter || stristr(clipSetName, m_searchText) != NULL))
			{
				int nameLength = istrlen(clipSetName);

				char *tempClipSetName = rage_new char[nameLength + 1];
				strcpy(tempClipSetName, clipSetName);

				m_clipSetNames[clipSetNameIndex] = tempClipSetName;
				clipSetNameIndex ++;
			}
		}

		std::sort(&m_clipSetNames[0], &m_clipSetNames[m_clipSetNameCount], AlphabeticalSort());
	}
}

void BuildClipDictionaryClipNamesMap(u32 clipDictionaryHash, atMap< u32, atString > &clipNamesMap)
{
	Assert(clipDictionaryHash != 0);
	if(clipDictionaryHash != 0)
	{
		int clipDictionaryIndex = g_ClipDictionaryStore.FindSlotFromHashKey(clipDictionaryHash).Get();

		fwClipDictionaryLoader loader(clipDictionaryIndex);
		if(loader.IsLoaded())
		{
			crClipDictionary* clipDictionary = loader.GetDictionary();
			if (clipDictionary)
			{
				for(int clipIndex = 0; clipIndex < clipDictionary->GetNumClips(); clipIndex ++)
				{
					u32 clipHash = clipDictionary->FindKeyByIndex(clipIndex);

					crClip *clip = clipDictionary->GetClip(clipHash);
					if(clip)
					{
						atString clipName(clip->GetName());
						if(clipName.GetLength() > 0)
						{
							clipName.Replace("pack:/", "");
							clipName.Replace(".clip", "");

							if(clipNamesMap.Access(clipHash) == NULL)
							{
								clipNamesMap.Insert(clipHash, atString(clipName));
							}
						}
					}
				}
			}
		}
	}
}

void CDebugClipSelector::LoadClipDictionaryClipNames()
{
	/* Check clip names exist */
	if(m_clipDictionaryClipNameCount > 0)
	{
		USE_DEBUG_ANIM_MEMORY();

		/* Iterate through clip names */
		for(int clipNameIndex = 0; clipNameIndex < m_clipDictionaryClipNameCount; clipNameIndex ++)
		{
			/* Release clip name */
			delete[] m_clipDictionaryClipNames[clipNameIndex]; m_clipDictionaryClipNames[clipNameIndex] = NULL;
		}

		/* Release clip names array */
		delete[] m_clipDictionaryClipNames; m_clipDictionaryClipNames = NULL;
		m_clipDictionaryClipNameCount = 0;
	}

	/* Check clip dictionary is selected */
	if(m_clipDictionaryIndex >= 0 && m_clipDictionaryIndex < m_clipDictionaryNameCount)
	{
		/* Get clip dictionary */
		u32 clipDictionaryHash(atStringHash(m_clipDictionaryNames[m_clipDictionaryIndex]));
		Assert(clipDictionaryHash != 0);
		if(clipDictionaryHash != 0)
		{
			/* Build clip names map */
			atMap< u32, atString > clipNamesMap;
			BuildClipDictionaryClipNamesMap(clipDictionaryHash, clipNamesMap);

			/* Check clip names exist */
			m_clipDictionaryClipNameCount = clipNamesMap.GetNumUsed();
			if(m_clipDictionaryClipNameCount)
			{
				USE_DEBUG_ANIM_MEMORY();

				/* Allocate clip names array */
				m_clipDictionaryClipNames = rage_new const char *[m_clipDictionaryClipNameCount];

				/* Iterate through clip names map */
				int clipIndex = 0;
				for(atMap< u32, atString >::Iterator It = clipNamesMap.CreateIterator(); !It.AtEnd(); It.Next())
				{
					atString &clipNameString = It.GetData();

					/* Copy clip name */
					int nameLength = istrlen(clipNameString.c_str());
					char *clipName = rage_new char[nameLength + 1];
					strcpy(clipName, clipNameString.c_str());
					m_clipDictionaryClipNames[clipIndex] = clipName;

					clipIndex ++;
				}

				/* Sort clip name array */
				std::sort(&m_clipDictionaryClipNames[0], &m_clipDictionaryClipNames[m_clipDictionaryClipNameCount], AlphabeticalSort());
			}
		}
	}
}

void BuildClipSetClipNamesMap(fwMvClipSetId clipSetId, atMap< u32, atString > &clipNamesMap)
{
	Assert(clipSetId != CLIP_SET_ID_INVALID);
	if(clipSetId != CLIP_SET_ID_INVALID)
	{
			fwClipSet *clipSet = fwClipSetManager::GetClipSet(clipSetId);
			if(clipSet)
			{
				crClipDictionary *clipDictionary = clipSet->GetClipDictionary();
				if(clipDictionary)
				{
					if(clipSet->GetClipItemCount() == 0)
					{

						for(int clipIndex = 0; clipIndex < clipDictionary->GetNumClips(); clipIndex ++)
						{
							u32 clipHash = clipDictionary->FindKeyByIndex(clipIndex);

							crClip *clip = clipDictionary->GetClip(clipHash);
							if(clip)
							{
								atString clipName(clip->GetName());
								if(clipName.GetLength() > 0)
								{
									clipName.Replace("pack:/", "");
									clipName.Replace(".clip", "");

									if(clipNamesMap.Access(clipHash) == NULL)
									{
										clipNamesMap.Insert(clipHash, atString(clipName));
									}
								}
							}
						}
					}
					else
					{
						for(int clipIndex = 0; clipIndex < clipSet->GetClipItemCount(); clipIndex ++)
						{
							u32 clipHash = clipSet->GetClipItemIdByIndex(clipIndex).GetHash();
							crClip *clip = clipDictionary->GetClip(clipHash);
							if(clip)
							{
								atString clipName(clip->GetName());
								if(clipName.GetLength() > 0)
								{
									clipName.Replace("pack:/", "");
									clipName.Replace(".clip", "");

									if(clipNamesMap.Access(clipHash) == NULL)
									{
										clipNamesMap.Insert(clipHash, atString(clipName));
									}
								}
							}
						}
					}
				}

				fwMvClipSetId fallbackClipSetId = clipSet->GetFallbackId();
				if(fallbackClipSetId != CLIP_SET_ID_INVALID)
				{
					BuildClipSetClipNamesMap(fallbackClipSetId, clipNamesMap);
				}
			}
		}
}

void CDebugClipSelector::LoadClipSetClipNames()
{
	/* Check clip names exist */
	if(m_clipSetClipNameCount > 0)
	{
		USE_DEBUG_ANIM_MEMORY();

		/* Iterate through clip names */
		for(int clipItemNameIndex = 0; clipItemNameIndex < m_clipSetClipNameCount; clipItemNameIndex ++)
		{
			/* Release clip name */
			delete[] m_clipSetClipNames[clipItemNameIndex]; m_clipSetClipNames[clipItemNameIndex] = NULL;
		}

		/* Release clip names array */
		delete[] m_clipSetClipNames; m_clipSetClipNames = NULL;
		m_clipSetClipNameCount = 0;
	}

	/* Check clip set is selected */
	if(m_clipSetIndex >= 0 && m_clipSetIndex < m_clipSetNameCount)
	{
		/* Get clip set */
		fwMvClipSetId clipSetId(m_clipSetNames[m_clipSetIndex]);
		Assert(clipSetId != CLIP_SET_ID_INVALID);
		if(clipSetId != CLIP_SET_ID_INVALID)
		{
			/* Force load clip set */
			Verifyf(m_clipSetHelper.Request(clipSetId, true), "Could not load clip set %s!", m_clipSetNames[m_clipSetIndex]);

			/* Build clip names map */
			atMap< u32, atString > clipNamesMap;
			if(m_bShowClipSetClipNames)
			{
				BuildClipSetClipNamesMap(clipSetId, clipNamesMap);
			}
			else
			{
				clipNamesMap.Insert(0, atString("(none)"));
			}

			/* Check clip names exist */
			m_clipSetClipNameCount = clipNamesMap.GetNumUsed();
			if(m_clipSetClipNameCount)
			{
				USE_DEBUG_ANIM_MEMORY();

				/* Allocate clip names array */
				m_clipSetClipNames = rage_new const char *[m_clipSetClipNameCount];

				/* Iterate through clip names map */
				int clipIndex = 0;
				for(atMap< u32, atString >::Iterator It = clipNamesMap.CreateIterator(); !It.AtEnd(); It.Next())
				{
					atString &clipNameString = It.GetData();

					/* Copy clip name */
					int nameLength = istrlen(clipNameString.c_str());
					char *clipName = rage_new char[nameLength + 1];
					strcpy(clipName, clipNameString.c_str());
					m_clipSetClipNames[clipIndex] = clipName;

					clipIndex ++;
				}

				/* Sort clip name array */
				std::sort(&m_clipSetClipNames[0], &m_clipSetClipNames[m_clipSetClipNameCount], AlphabeticalSort());
			}
		}
	}
}

void CDebugClipSelector::ReloadLocalFileClip()
{
	OnLocalFileChanged();
}

bkBank* CDebugClipSelector::FindBank(bkWidget* pWidget)
{
	while (pWidget)
	{
		if (pWidget)
		{
			bkBank* pBank = dynamic_cast<bkBank*>(pWidget);

			if (pBank)
			{
				return pBank;
			}
		}	

		pWidget = pWidget->GetParent();
	}

	return NULL;
}

const char * CDebugClipSelector::GetSelectedClipName() const
{
	Assert(m_clipContext == kClipContextClipDictionary); 
	if(Verifyf(m_clipDictionaryClipIndex >= 0 && m_clipDictionaryClipNameCount > m_clipDictionaryClipIndex,"Dictionary clip index requested: %d, number of clips available: %d",
		m_clipDictionaryClipIndex, m_clipDictionaryClipNameCount))
	{
		return m_clipDictionaryClipNames[m_clipDictionaryClipIndex];
	}
	return NULL;
}

const char * CDebugClipSelector::GetSelectedClipDictionary() const
{
	Assert(m_clipContext == kClipContextClipDictionary);
	if(Verifyf(m_clipDictionaryIndex >= 0 && m_clipDictionaryNameCount >m_clipDictionaryIndex,"Clip dictionary index requested: %d, number of clips available: %d",
		m_clipDictionaryIndex, m_clipDictionaryNameCount))
	{
		return m_clipDictionaryNames[m_clipDictionaryIndex];
	}
	return NULL;
}

rage::fwMvClipId CDebugClipSelector::GetSelectedClipId() const
{
	Assert(m_clipContext == kClipContextAbsoluteClipSet);
	
	if(Verifyf(m_clipSetClipIndex >= 0 && m_clipSetClipNameCount >m_clipSetClipIndex,"Clip set index requested: %d, number of clips available: %d",
		m_clipSetClipIndex, m_clipSetClipNameCount))
	{
		return fwMvClipId(m_clipSetClipNames[m_clipSetClipIndex]);
	}
	return CLIP_ID_INVALID;
}

rage::fwMvClipSetId CDebugClipSelector::GetSelectedClipSetId() const
{
	Assert(m_clipContext == kClipContextAbsoluteClipSet);
	if(Verifyf(m_clipSetIndex >= 0 && m_clipSetNameCount >m_clipSetIndex,"Clip set index requested: %d, number of clips available: %d",
		m_clipSetIndex, m_clipSetNameCount))
	{
		return fwMvClipSetId(m_clipSetNames[m_clipSetIndex]);
	}
	return CLIP_SET_ID_INVALID;
}

//////////////////////////////////////////////////////////////////////////
//	Anim timeline control implementation
//////////////////////////////////////////////////////////////////////////

void CDebugTimelineControl::AddWidgets(bkBank* pBank, datCallback sliderCallBack)
{
	m_sliders.PushAndGrow(
		pBank->AddSlider("Anim timeline", &m_value, 0.0f, 1.0f, 0.0f, sliderCallBack)
		);
	pBank->AddCombo("Timeline mode:", &m_mode, kSliderTypeMax, &ms_modeNames[0], datCallback(MFA(CDebugTimelineControl::SetMode), (datBase*)this));
	
}

void CDebugTimelineControl::RemoveWidgets()
{
	while (m_sliders.GetCount()>0)
	{
		if (m_sliders.Top()->GetNext())
		{
			m_sliders.Top()->GetNext()->Destroy();	//remove the mode selection combo
		}
		m_sliders.Top()->Destroy();	//remove the slider
		m_sliders.Top() = NULL;
		m_sliders.Pop();
	}
}

void CDebugTimelineControl::Shutdown()
{
	m_pClip = NULL;
}

void CDebugTimelineControl::SetMode()
{
	SetTimelineMode((ePhaseSliderMode)m_mode);
}

void CDebugTimelineControl::SetTimelineMode(ePhaseSliderMode mode)
{
	m_mode = m_lastMode;

	float tempPhase = GetPhase();

	m_mode = mode;

	SetPhase(tempPhase);

	m_lastMode = m_mode;
}

void CDebugTimelineControl::SetUpSlider(float startPhase)
{
	if(m_pClip)
	{
		for (int i=0; i<m_sliders.GetCount(); i++)
		{
			bkSlider* pSlider = m_sliders[i];

			switch(m_mode)
			{
			case kSliderTypePhase:
				{
					m_value = startPhase;
					pSlider->SetRange(0.0f,1.0f);
					pSlider->SetStep(0.01f);
				}
				break;
			case kSliderTypeFrame:
				{
					m_value = m_pClip->ConvertPhaseTo30Frame(startPhase);
					pSlider->SetRange(0.0f, m_pClip->GetNum30Frames());
					pSlider->SetStep(1.0f);
				}
				break;
			case kSliderTypeTime:
				{
					m_value = m_pClip->ConvertPhaseToTime(startPhase);
					pSlider->SetRange(0.0f, m_pClip->GetDuration());
					pSlider->SetStep(m_pClip->GetDuration()/100.0f);
				}
				break;
			}
		}
		
	}
}

float CDebugTimelineControl::GetPhase()
{
	if(m_pClip)
	{
		switch(m_mode)
		{
		case kSliderTypePhase:
			{
				return Clamp(m_value, 0.0f, 1.0f);
			}
		case kSliderTypeFrame:
			{
				return Clamp(m_pClip->Convert30FrameToPhase(m_value), 0.0f, 1.0f);
			}
		case kSliderTypeTime:
			{
				return Clamp(m_pClip->ConvertTimeToPhase(m_value), 0.0f, 1.0f);
			}
		}
	}
	
	return 0.0f;
}

float CDebugTimelineControl::GetFrame()
{
	if(m_pClip)
	{
		switch(m_mode)
		{
		case kSliderTypePhase:
			{
				return Clamp(m_pClip->ConvertPhaseTo30Frame(m_value), 0.0f, m_pClip->GetNum30Frames());
			}
		case kSliderTypeFrame:
			{
				return Clamp(m_value, 0.0f,m_pClip->GetNum30Frames());
			}
		case kSliderTypeTime:
			{
				return Clamp(m_pClip->ConvertTimeTo30Frame(m_value), 0.0f, m_pClip->GetNum30Frames());
			}
		}
	}

	return 0.0f;

}

float CDebugTimelineControl::GetTime()
{	
	if(m_pClip)
	{
		switch(m_mode)
		{
		case kSliderTypePhase:
			{
				return Clamp(m_pClip->ConvertPhaseToTime(m_value), 0.0f, m_pClip->GetDuration());
			}
		case kSliderTypeFrame:
			{
				return Clamp(m_pClip->Convert30FrameToTime(m_value), 0.0f, m_pClip->GetDuration());
			}
		case kSliderTypeTime:
			{
				return Clamp(m_value, 0.0f, m_pClip->GetDuration());
			}
		}
	}

	return 0.0f;
}

void CDebugTimelineControl::SetPhase(float phase)
{
	if(m_pClip)
	{
		switch(m_mode)
		{
		case kSliderTypePhase:
			{
				m_value = phase;
			}
			break;
		case kSliderTypeFrame:
			{
				m_value = m_pClip->ConvertPhaseTo30Frame(phase);
			}
			break;
		case kSliderTypeTime:
			{
				m_value = m_pClip->ConvertPhaseToTime(phase);
			}
			break;
		}
	}
}

void CDebugTimelineControl::SetFrame(float frame)
{
	if(m_pClip)
	{
		switch(m_mode)
		{
		case kSliderTypePhase:
			{
				m_value = m_pClip->Convert30FrameToPhase(frame);
			}
			break;
		case kSliderTypeFrame:
			{
				m_value = (float)frame;
			}
			break;
		case kSliderTypeTime:
			{
				m_value = m_pClip->Convert30FrameToTime(frame);
			}
			break;
		}
	}
}

void CDebugTimelineControl::SetTime(float time)
{
	if(m_pClip)
	{
		switch(m_mode)
		{
		case kSliderTypePhase:
			{
				m_value = m_pClip->ConvertTimeToPhase(time);
			}
			break;
		case kSliderTypeFrame:
			{
				m_value = m_pClip->ConvertTimeTo30Frame(time);
			}
			break;
		case kSliderTypeTime:
			{
				m_value = time;
			}
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
//	Debug object model selector
//////////////////////////////////////////////////////////////////////////

void CDebugObjectSelector::RefreshList()
{
	LoadNames();
	UpdateCombo();
}

void CDebugObjectSelector::LoadNames()
{
	m_modelNames.clear();

	//look through the permanent archetypes
	const u32 maxModelInfos = CModelInfo::GetMaxModelInfos();
	for (u32 i=CModelInfo::GetStartModelInfos(); i<maxModelInfos; i++)
	{
		CBaseModelInfo* modelInfo = CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(i)));

		if (modelInfo && modelInfo->GetModelName())
		{
			const char* modelName = modelInfo->GetModelName();
			if (strlen(m_searchText) != 0 && stristr(modelName, m_searchText) != NULL)
			{
				if(modelInfo->GetModelType() != MI_TYPE_PED && modelInfo->GetModelType()!=MI_TYPE_VEHICLE)
				{
					if (modelInfo->GetModelName())
					{
						if(m_bSearchDynamicObjects)
						{
							if(modelInfo->GetIsTypeObject())	
							{
								m_modelNames.PushAndGrow(modelInfo->GetModelName());
							}
						}
						
						if(m_bSearchNonDynamicObjects)
						{
							if(modelInfo->GetIsFixed())
							{
								m_modelNames.PushAndGrow(modelInfo->GetModelName());
							}
						}
							
						if( !m_bSearchDynamicObjects && !m_bSearchNonDynamicObjects )
						{
							// Add the object name to the array of model names
							m_modelNames.PushAndGrow(modelInfo->GetModelName());
						}
					}
				}
			}
		}
	}

	if (m_modelNames.GetCount()>1)
	{
		// Sort the model names alphabetically
		std::sort(&m_modelNames[0], &m_modelNames[0] + m_modelNames.GetCount(), AlphabeticalSort());
	}
}

bool CDebugObjectSelector::StreamSelected() const
{
	//make sure the object is streamed in
	fwModelId modelId = GetSelectedModelId();
	if (!CModelInfo::HaveAssetsLoaded(modelId))
	{
		animDisplayf("Requesting object %s", GetSelectedObjectName());
		CModelInfo::RequestAssets(modelId, STRFLAG_DONTDELETE|STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
		CStreaming::LoadAllRequestedObjects(true);
	}
	else
	{
		return true;
	}

	//if the model streamed in ok, make any necessary callbacks
	if (CModelInfo::HaveAssetsLoaded(modelId))
	{	
		return true;
	}

	return false;
}

void CDebugObjectSelector::Select(const char * itemName)
{
	formatf(m_searchText, 15, "%s", itemName);
	CDebugSelector::Select(itemName);
}

CObject* CDebugObjectSelector::CreateObject(const Mat34V& location) const
{
	CObject* pObject = NULL;

	if (StreamSelected())
	{
		pObject = CObjectPopulation::CreateObject(GetSelectedModelId(), ENTITY_OWNEDBY_SCRIPT, true, true);

		if (pObject)
		{

			pObject->SetMatrix(MAT34V_TO_MATRIX34(location));

			// Add Object to world after its position has been set
			CGameWorld::Add(pObject, CGameWorld::OUTSIDE );
			pObject->GetPortalTracker()->RequestRescanNextUpdate();
			pObject->GetPortalTracker()->Update(VEC3V_TO_VECTOR3(pObject->GetTransform().GetPosition()));

			pObject->SetupMissionState();
		}
	}

	if (pObject)
	{	
		if (pObject->GetDrawable())
		{
			// lazily create a skeleton if one doesn't exist
			if (!pObject->GetSkeleton())
			{
				pObject->CreateSkeleton();
			}

			// Lazily create the animBlender if it doesn't exist
			if (!pObject->GetAnimDirector() && pObject->GetSkeleton())
			{
				pObject->CreateAnimDirector(*pObject->GetDrawable());
			}

			// make sure the object is on the process control list
			pObject->AddToSceneUpdate();
		}
	}

	return pObject;
}

fwModelId CDebugObjectSelector::GetSelectedModelId() const
{
	fwModelId modelId;
	CModelInfo::GetBaseModelInfoFromName(GetSelectedObjectName(), &modelId);
	return(modelId);
}

//////////////////////////////////////////////////////////////////////////
//	Debug vehicle model selector
//////////////////////////////////////////////////////////////////////////

void CDebugVehicleSelector::RefreshList()
{
	LoadNames();
	UpdateCombo();
}

void CDebugVehicleSelector::LoadNames()
{
	m_modelNames.clear();

	//look through the complete store
	const u32 maxModelInfos = CModelInfo::GetMaxModelInfos();
	for (u32 i=0; i<maxModelInfos; i++)
	{
		CBaseModelInfo* modelInfo = CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(i)));

		if (modelInfo && modelInfo->GetModelName())
		{
			const char* modelName = modelInfo->GetModelName();
			if (strlen(m_searchText) != 0 && stristr(modelName, m_searchText) != NULL)
			{
				if(modelInfo->GetModelType()==MI_TYPE_VEHICLE)
				{
					if (modelInfo->GetModelName())
					{
						// Add the object name to the array of model names
						m_modelNames.PushAndGrow(modelInfo->GetModelName());
					}
				}
			}
		}
	}

	if (m_modelNames.GetCount()>1)
	{
		// Sort the model names alphabetically
		std::sort(&m_modelNames[0], &m_modelNames[0] + m_modelNames.GetCount(), AlphabeticalSort());
	}
}

bool CDebugVehicleSelector::StreamSelected() const
{
	//make sure the object is streamed in
	fwModelId modelId = GetSelectedModelId();
	if (!CModelInfo::HaveAssetsLoaded(modelId))
	{
		animDisplayf("Requesting object %s", GetSelectedObjectName());
		CModelInfo::RequestAssets(modelId, STRFLAG_DONTDELETE|STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
		CStreaming::LoadAllRequestedObjects(true);
	}
	else
	{
		return true;
	}

	//if the model streamed in ok, make any necessary callbacks
	if (CModelInfo::HaveAssetsLoaded(modelId))
	{	
		return true;
	}

	return false;
}

CVehicle* CDebugVehicleSelector::CreateVehicle(const Mat34V& location, ePopType popType) const
{
	CVehicle* pVeh = NULL;

	if (StreamSelected())
	{
		Matrix34 matrix = MAT34V_TO_MATRIX34(location);
		pVeh = CVehicleFactory::GetFactory()->Create(GetSelectedModelId(), ENTITY_OWNEDBY_DEBUG, popType, &matrix);
		if( pVeh)
		{		
			CGameWorld::Add(pVeh, CGameWorld::OUTSIDE);
			pVeh->GetPortalTracker()->RequestRescanNextUpdate();
			pVeh->GetPortalTracker()->Update(VEC3V_TO_VECTOR3(pVeh->GetTransform().GetPosition()));
			return pVeh;
		}
	}
	return NULL;
}

fwModelId CDebugVehicleSelector::GetSelectedModelId() const
{
	fwModelId modelId;
	CModelInfo::GetBaseModelInfoFromName(GetSelectedObjectName(), &modelId);
	return(modelId);
}

void CDebugVehicleSelector::Select(const char * itemName)
{
	formatf(m_searchText, 15, "%s", itemName);
	CDebugSelector::Select(itemName);
}

//////////////////////////////////////////////////////////////////////////
//	Debug filter selector
//////////////////////////////////////////////////////////////////////////

bool AddName(crFrameFilter&, u32 hash, void* pNames)
{
	atArray<const char *>& names = *static_cast<atArray<const char *>*>(pNames);
	atHashString hashName(hash);
	if (hashName.TryGetCStr())
	{
		names.PushAndGrow(hashName.GetCStr());
	}
	return true;
};

void CDebugFilterSelector::RefreshList()
{
	LoadNames();
	UpdateCombo();
}

void CDebugFilterSelector::LoadNames()
{
	m_Names.clear();

	strLocalIndex slot = strLocalIndex(g_FrameFilterDictionaryStore.FindSlotFromHashKey(FILTER_SET_PLAYER));

	if (g_FrameFilterDictionaryStore.IsValidSlot(slot))
	{
		// Get filter set
		crFrameFilterDictionary *frameFilterDictionary = g_FrameFilterDictionaryStore.Get(slot);
		if(frameFilterDictionary)
		{
			frameFilterDictionary->ForAll(&AddName, &m_Names);
		}
	}

	if (m_Names.GetCount()>1)
	{
		// Sort the names alphabetically
		std::sort(&m_Names[0], &m_Names[0] + m_Names.GetCount(), AlphabeticalSort());
	}
}

bool CDebugFilterSelector::StreamSelected()
{
	return true;
}

crFrameFilter* CDebugFilterSelector::GetSelectedFilter()
{
	if (HasSelection())
	{
		return g_FrameFilterDictionaryStore.FindFrameFilter(fwMvFilterId(GetSelectedObjectName()));
	}
	else
	{
		return NULL;
	}
}

//////////////////////////////////////////////////////////////////////////
//	Debug ped model selector implementation
//////////////////////////////////////////////////////////////////////////

void CDebugPedModelSelector::AddWidgets(bkBank* pBank, const char * widgetTitle, datCallback modelChangedCallback)
{
	//check the model names have been loaded
	if (m_modelNames.GetCount()==0)
	{
		CDebugPedModelSelector::LoadModelNames();
	}

	//add the combo box widget to the bank
	pBank->AddCombo(widgetTitle, &m_selectedModel, m_modelNames.GetCount(), &m_modelNames[0],
		datCallback(MFA(CDebugPedModelSelector::SelectModel), (datBase*)this), "Lists the models available");

	m_modelChangedCallback = modelChangedCallback;
}

fwModelId CDebugPedModelSelector::GetSelectedModelId() const
{
	fwModelId modelId;
	CModelInfo::GetBaseModelInfoFromName(GetSelectedModelName(), &modelId);
	return(modelId);
}

void CDebugPedModelSelector::LoadModelNames()
{
	m_modelNames.clear();

	fwArchetypeDynamicFactory<CPedModelInfo>& pedModelInfoStore = CModelInfo::GetPedModelInfoStore();
	atArray<CPedModelInfo*> pedTypeArray;
	pedModelInfoStore.GatherPtrs(pedTypeArray);

	int numPedsAvail = pedTypeArray.GetCount();

	for (int i=0; i<numPedsAvail; i++)
	{
		CPedModelInfo& pedModelInfo = *pedTypeArray[i];

		bool allowCutscenePeds = false;
#if __DEV
		// Force the player to start in the world if that parameter is set
		XPARAM(allowCutscenePeds);
		if( PARAM_allowCutscenePeds.Get() )
		{
			allowCutscenePeds = true;
		}
#endif //__DEV

		if((strncmp(pedModelInfo.GetModelName(), "CS_", 3) != 0) || allowCutscenePeds)
		{
			// Add the file name to the array of model names
			m_modelNames.PushAndGrow(pedModelInfo.GetModelName());
		}
	}

	// Sort the model names alphabetically
	std::sort(&m_modelNames[0], &m_modelNames[0] + m_modelNames.GetCount(), AlphabeticalSort());

}

void CDebugPedModelSelector::Select(const char * modelName)
{
	if (m_modelNames.GetCount()==0)
	{
		LoadModelNames();
	}

	for (int i=0; i<m_modelNames.GetCount(); i++)
	{	
		if (!stricmp(modelName, m_modelNames[i]))
		{
			Select(i);
			return;
		}
	}
}

void CDebugPedModelSelector::SelectModel()
{

	if (GetRegisteredEntity())
	{
		CDynamicEntity* pEntity = GetRegisteredEntity();

		// remove old model if appropriate (i.e. ref count is 0 and don't delete flags not set)
		if (pEntity->GetBaseModelInfo()->GetNumRefs() == 0)
		{
			if (!(CModelInfo::GetAssetStreamFlags(pEntity->GetModelId()) & STR_DONTDELETE_MASK))
			{
				CModelInfo::RemoveAssets(pEntity->GetModelId());
			}
		}

		// Load the selected model
		fwModelId modelId;
		CModelInfo::GetBaseModelInfoFromName(m_modelNames[m_selectedModel], &modelId);

		if (!CModelInfo::HaveAssetsLoaded(modelId))
		{
			CModelInfo::RequestAssets(modelId, STRFLAG_DONTDELETE|STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
			CStreaming::LoadAllRequestedObjects(true);
		}

		
		if (pEntity->GetIsTypePed())
		{
			// destroy and recreate the ped
			CPed* pPed = static_cast<CPed*>(pEntity);

			if (pPed->IsPlayer())
			{
				pPed = static_cast<CPed*>(CGameWorld::ChangePedModel(*pPed, modelId));
				animAssert(pPed);
			}
			else
			{
				RecreatePed();
			}
		}
	}

	if (m_modelChangedCallback.GetType() != datCallback::NULL_CALLBACK)
		m_modelChangedCallback.Call();

}

CPed* CDebugPedModelSelector::CreatePed(const Mat34V& location, ePopType popType, bool generateDefaultTask /* = true */) const
{
	CPed* pPed = NULL;

	fwModelId pedModelId = GetSelectedModelId();
	// ensure that the model is loaded and ready for drawing for this ped
	if (!CModelInfo::HaveAssetsLoaded(pedModelId))
	{
		CModelInfo::RequestAssets(pedModelId, STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
		CStreaming::LoadAllRequestedObjects(true);
	}

	Matrix34 TempMat(MAT34V_TO_MATRIX34(location));

	const CControlledByInfo localAiControl(false, false);
	//create the new one
	pPed = CPedFactory::GetFactory()->CreatePed(localAiControl, pedModelId, &TempMat, true, true, true, false);

	pPed->PopTypeSet(popType);
	pPed->SetDefaultDecisionMaker();
	pPed->GetPedIntelligence()->SetDefaultRelationshipGroup();

	if (generateDefaultTask)
	{
		CEventGivePedTask event( PED_TASK_PRIORITY_PRIMARY, rage_new CTaskDoNothing(-1), false);
		pPed->GetPedIntelligence()->AddEvent( event );
	}

	pPed->SetCharParamsBasedOnManagerType();

	CGameWorld::Add(pPed, CGameWorld::OUTSIDE);

	pPed->GetPortalTracker()->RequestRescanNextUpdate();
	pPed->GetPortalTracker()->Update(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()));
	
	return pPed;
}

void CDebugPedModelSelector::RecreatePed()
{
	USE_DEBUG_ANIM_MEMORY();

	if (GetRegisteredEntity())
	{
		CPed* pPed = static_cast<CPed*>(GetRegisteredEntity());

		// Get the relevant data for the new ped
		Matrix34 TempMat(MAT34V_TO_MATRIX34(pPed->GetMatrix()));
		u32 pedInfoIndex = (u32)m_selectedModel;
		const CControlledByInfo localAiControl(pPed->GetControlledByInfo());
		ePopType popType = pPed->PopTypeGet();

		// don't allow the creation of player ped type - it doesn't work!
		if (pedInfoIndex == CGameWorld::FindLocalPlayer()->GetModelIndex()) {return;}

		fwModelId pedModelId((strLocalIndex(pedInfoIndex)));
		// ensure that the model is loaded and ready for drawing for this ped
		if (!CModelInfo::HaveAssetsLoaded(pedModelId))
		{
			CModelInfo::RequestAssets(pedModelId, STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
			CStreaming::LoadAllRequestedObjects(true);
		}

		//destroy the old ped
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_DontStoreAsPersistent, true );
		CPedFactory::GetFactory()->DestroyPed(pPed);

		//create the new one
		*m_pRegisteredEntity = CPedFactory::GetFactory()->CreatePed(localAiControl, pedModelId, &TempMat, true, true, true, false);

		pPed = static_cast<CPed*>(*m_pRegisteredEntity);

		pPed->PopTypeSet(popType);
		pPed->SetDefaultDecisionMaker();
		pPed->GetPedIntelligence()->SetDefaultRelationshipGroup();

		CEventGivePedTask event( PED_TASK_PRIORITY_PRIMARY, rage_new CTaskDoNothing(-1), false);
		pPed->GetPedIntelligence()->AddEvent( event );

		pPed->SetCharParamsBasedOnManagerType();

		CGameWorld::Add(pPed, CGameWorld::OUTSIDE);

		pPed->GetPortalTracker()->RequestRescanNextUpdate();
		pPed->GetPortalTracker()->Update(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()));
	}	

}

//////////////////////////////////////////////////////////////////////////
// Debug render helper implementation
//////////////////////////////////////////////////////////////////////////

void CDebugRenderer::Render()
{
	//render 3d lines
	for (int i=0; i<m_3dLine.GetCount(); i++)
	{
		grcDebugDraw::Line(
			RCC_VEC3V(m_3dLine[i].start), 
			RCC_VEC3V(m_3dLine[i].end),
			m_3dLine[i].color1,
			m_3dLine[i].color2
			);
	}

	//spheres
	for (int i=0; i<m_sphere.GetCount(); i++)
	{
		grcDebugDraw::Sphere(
			RCC_VEC3V(m_sphere[i].centre),
			m_sphere[i].radius,
			m_sphere[i].colour,
			m_sphere[i].filled
			);
	}

	//3d text
	for (int i=0; i<m_3dText.GetCount(); i++)
	{
		grcDebugDraw::Text(
			RCC_VEC3V(m_3dText[i].position), 
			m_3dText[i].colour,
			m_3dText[i].screenOffsetX, 
			m_3dText[i].screenOffsetY,
			m_3dText[i].text,
			m_3dText[i].drawBackground
			);
	}

	//2d text
	for (int i=0; i<m_2dText.GetCount(); i++)
	{
		Vec2V vTextRenderPos(m_2dTextHorizontalBorder, m_2dTextVerticalBorder + (m_2dText[i].line)*m_2dTextVertDist);
		grcDebugDraw::Text(
			vTextRenderPos, 
			m_2dText[i].colour,
			m_2dText[i].text
			);
	}

	//axes
	for (int i=0; i<m_axes.GetCount(); i++)
	{
		grcDebugDraw::Axis(
			RCC_MAT34V(m_axes[i].axis),
			m_axes[i].scale,
			m_axes[i].drawArrows
			);
	}

	// line series
	for (int i=0; i<m_series.GetCount(); i++)
	{
		Vector3* lastVector = NULL;
		for (int j=0; j<m_series[i].data.GetCount(); j++)
		{


			// draw the point
			grcDebugDraw::Sphere(RCC_VEC3V(m_series[i].data[j]), 0.005f,Color_white);

			// render the index of every 10th entry (so we can see the order more easily)
			TUNE_INT(debugHelperIndexRenderInterval, 10, 1, 100, 1);
			if ((j%debugHelperIndexRenderInterval)==0)
			{
				char index[5];
				sprintf(index, "%d", j);
				grcDebugDraw::Text(m_series[i].data[j], m_series[i].color, index);
			}


			// draw a line from the last point to the current one
			if (lastVector)
			{
				grcDebugDraw::Line(*lastVector, m_series[i].data[j],m_series[i].color,m_series[i].color);
			}
			else
			{
				//render the series name at the start position
				grcDebugDraw::Text(m_series[i].data[j], m_series[i].color,0,10,m_series[i].name);
			}
			// Keep a pointer to the last point in the series
			lastVector = &m_series[i].data[j];
		}
	}
}

void CDebugRenderer::Reset()
{
	//clear 3d lines
	m_3dLine.Reset();


	//spheres
	m_sphere.Reset();

	//3d text
	m_3dText.Reset();

	//2d text
	m_2dText.Reset();

	//axis
	m_axes.Reset();


}

void CDebugRenderer::Axis(const Matrix34& mtx, const float scale, bool drawArrows /* = false */)
{
	DebugAxis call;

	call.axis = mtx;
	call.scale = scale;
	call.drawArrows = drawArrows;

	m_axes.PushAndGrow(call);
}

void CDebugRenderer::Text2d(const int line, const Color32 col, const char *pText)
{
	Debug2dText call;
	call.line = line;
	call.colour = col;
	call.text = pText;
	m_2dText.PushAndGrow(call);
}

void CDebugRenderer::Text3d(const Vector3& pos, const Color32 col, const char *pText, bool drawBGQuad /* = true */)
{
	Text3d(pos, col, 0, 0, pText, drawBGQuad);
}

void CDebugRenderer::Text3d(const Vector3& pos, const Color32 col, const s32 iScreenSpaceXOffset, const s32 iScreenSpaceYOffset, const char *pText, bool drawBGQuad /* = true */)
{
	Debug3dText call;

	call.position = pos;
	call.colour = col;
	call.screenOffsetX = iScreenSpaceXOffset;
	call.screenOffsetY = iScreenSpaceYOffset;
	call.text = pText;
	call.drawBackground = drawBGQuad;

	m_3dText.PushAndGrow(call);
}

void CDebugRenderer::Line(const Vector3& v1, const Vector3& v2, const Color32 colour1)
{
	Line(v1,v2,colour1,colour1);
}

void CDebugRenderer::Line(const Vector3& v1, const Vector3& v2, const Color32 colour1, const Color32 colour2)
{
	Debug3dLine call;

	call.start = v1;
	call.end = v2;
	call.color1 = colour1;
	call.color2 = colour2;

	m_3dLine.PushAndGrow(call);

}

void CDebugRenderer::Sphere(const Vector3& centre, const float r, const Color32 col, bool filled)
{
	DebugSphere call;

	call.centre = centre;
	call.radius = r;
	call.colour = col;
	call.filled = filled;

	m_sphere.PushAndGrow(call);

}

void CDebugRenderer::StoreBool(atArray<s32>& container, int entry, bool value)
{	
	//set the necessary bits in the container, growing if necessary
	if ((entry%32)==0)
	{
		container.PushAndGrow(0);
	}

	int slot = entry/32;
	// set the bit to represent this bool
	container[slot] |= (value<<(entry%32));
}

bool CDebugRenderer::GetBool(atArray<s32>& container, int entry)
{	//retrieve a stored bit value from the dynamic array
	return ((container[entry/32]) & (1<<(entry%32))) != 0;
}

int CDebugRenderer::AddSeries(const char * name, Color32 colour)
{
	DebugSeries series;

	series.color = colour;
	series.name = name;

	m_series.PushAndGrow(series);

	return (m_series.GetCount()-1);
}

//PURPOSE: Add an entry to the specified series
void CDebugRenderer::AddEntry(const Vector3& value, int seriesID)
{
	if (m_series.GetCount()>= (seriesID+1))
	{
		m_series[seriesID].data.PushAndGrow(value);
	}
}

// PURPOSE: Clears all data from the specified series
// NOTE:	The Reset() method below does not affect series. This method must be used to clear any series when you're done with the data
void CDebugRenderer::ClearSeries()
{
	//clear the value arrays
	for (int i=0; i<m_series.GetCount(); i++)
	{
		m_series[i].data.Reset();
	}

	//clear the series array
	m_series.Reset();
}

void CClipFileIterator::Iterate()
{

	atString clipExtension(".clip");

	if(m_searchDirectory.length()==0)
		return;

	IterateClips(m_searchDirectory.c_str(), clipExtension.c_str(), m_recursive);

}

void CClipFileIterator::IterateClips(const char * path, const char * fileType,  bool recursive)
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
				crClip* pClip = crClip::AllocateAndLoad(fileName);
				bool bSaveClip = false;

				if (pClip)
				{
					bSaveClip = m_clipCallback(pClip, *folderResults[i], path);

					if (bSaveClip)
					{
						pClip->Save(fileName);
					}

					pClip->Shutdown();

					delete pClip;
				}
			}
			else if ((fileName.LastIndexOf('.')<0) && recursive)
			{
				// This is a folder, enumerate it too
				IterateClips(fileName, fileType, true);
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

fiFindData* CClipFileIterator::FindFileData(const char * filename, const char * directory)
{
	atArray<fiFindData*> results;

	if (ASSET.Exists(directory,""))
	{
		ASSET.EnumFiles(directory, CDebugClipDictionary::FindFileCallback, &results);

		for ( int i=0; i<results.GetCount(); i++ )
		{
			if ( !strncmp(results[i]->m_Name, filename, strlen(results[i]->m_Name) ) )
			{				
				return results[i];
			}
		}
	}

	return NULL;
}


atString CClipLogIterator::GetClipDebugInfo(const crClip* pClip, float Time, float weight)
{
	const char *clipDictionaryName = NULL;

	if (pClip->GetDictionary())
	{
		/* Iterate through the clip dictionaries in the clip dictionary store */
		const crClipDictionary *clipDictionary = NULL;
		int clipDictionaryIndex = 0, clipDictionaryCount = g_ClipDictionaryStore.GetSize();
		for(; clipDictionaryIndex < clipDictionaryCount && clipDictionaryName==NULL; clipDictionaryIndex ++)
		{
			if(g_ClipDictionaryStore.IsValidSlot(strLocalIndex(clipDictionaryIndex)))
			{
				clipDictionary = g_ClipDictionaryStore.Get(strLocalIndex(clipDictionaryIndex));
				if(clipDictionary == pClip->GetDictionary())
				{
					clipDictionaryName = g_ClipDictionaryStore.GetName(strLocalIndex(clipDictionaryIndex));
				}
			}
		}
	}

	char outText[256] = "";

	sprintf(outText, "W:%.3f", weight );
	//const crClip* pClip = pClipNode->GetClipPlayer().GetClip();
	Assert(pClip);
	atString clipName(pClip->GetName());
	fwAnimHelpers::GetDebugClipName(*pClip, clipName);
	
	sprintf(outText, "%s, %s/%s, T:%.3f", outText, clipDictionaryName ? clipDictionaryName : "UNKNOWN", clipName.c_str(), Time);

	if (pClip->GetDuration()>0.0f)
	{
		sprintf(outText, "%s, P:%.3f", outText, Time / pClip->GetDuration());
	}

	atString outAtString(outText);

	return outAtString; 
}

void CClipLogIterator::Visit(crmtNode& node)
{
	crmtIterator::Visit(node);

	if (node.GetNodeType()==crmtNode::kNodeClip)
	{

		crmtNodeClip* pClipNode = static_cast<crmtNodeClip*>(&node);
		if (pClipNode 
			&& pClipNode->GetClipPlayer().HasClip())
		{
			//get the weight from the parent node (if it exists)
			float weight = CalcVisibleWeight(node);
			//if (weight > 0.0f)
			{
				const crClip* clip = pClipNode->GetClipPlayer().GetClip();

				/* To display the clip dictionary name we need to find the clip dictionary index in the clip dictionary store */
				atString outText = GetClipDebugInfo(clip, pClipNode->GetClipPlayer().GetTime(), weight); 
				animDisplayf("%s", outText.c_str());
			}
		}
	}
}

float CClipLogIterator::CalcVisibleWeight(crmtNode& node)
{
	crmtNode * pNode = &node;

	float weight = 1.0f;

	crmtNode* pParent = pNode->GetParent();

	while (pParent)
	{
		float outWeight = 1.0f;

		switch(pParent->GetNodeType())
		{
		case crmtNode::kNodeBlend:
			{
				crmtNodePairWeighted* pPair = static_cast<crmtNodePairWeighted*>(pParent);

				if (pNode==pPair->GetFirstChild())
				{
					outWeight = 1.0f - pPair->GetWeight();
				}
				else
				{
					outWeight = pPair->GetWeight();
				}
			}
			break;
		case crmtNode::kNodeBlendN:
			{
				crmtNodeN* pN = static_cast<crmtNodeN*>(pParent);
				u32 index = pN->GetChildIndex(*pNode);
				outWeight = pN->GetWeight(index);
			}
			break;
		default:
			outWeight = 1.0f;
			break;
		}

		weight*=outWeight;

		pNode = pParent;
		pParent = pNode->GetParent();
	}

	return weight;
}

bool CAnimDebug::LogAnimations(const CDynamicEntity& dynamicEntity)
{
	if (dynamicEntity.GetAnimDirector())
	{
		CClipLogIterator it(NULL);
		dynamicEntity.GetAnimDirector()->GetMove().GetMotionTree().Iterate(it);

		fwAnimDirectorComponentMotionTree* pComponentMotionTreeMidPhysics = dynamicEntity.GetAnimDirector()->GetComponentByPhase<fwAnimDirectorComponentMotionTree>(fwAnimDirectorComponent::kPhaseMidPhysics);
		if (pComponentMotionTreeMidPhysics)
		{
			crmtNode *pRootNode = pComponentMotionTreeMidPhysics->GetMotionTree().GetRootNode();
			if (pRootNode)
			{
				it.Start();
				it.Iterate(*pRootNode);
				it.Finish();
			}
		}
	}

	return true;
}

void CDebugFileList::FindFiles()
{
	// Clear previous found file list
	m_FoundFiles.clear();

	if(m_searchDirectory.length()==0)
		return;

	FindFilesInDirectory(m_searchDirectory.c_str(), m_fileExtension.c_str(), m_recursive, m_storeFilePath, m_storeFileExtension);

}

void CDebugFileList::FindFilesInDirectory(const char * path, const char * fileType,  bool recursive, bool storeFilePath, bool storeFileExtension)
{
	USE_DEBUG_ANIM_MEMORY();

	atArray<fiFindData*> folderResults;

	if (ASSET.Exists(path,""))
	{
		ASSET.EnumFiles(path, CDebugClipDictionary::FindFileCallback, &folderResults);

		for (int i = 0; i < folderResults.GetCount(); i++)
		{
			atString fullFilePath( path );
			fullFilePath+= "/";
			fullFilePath += folderResults[i]->m_Name;

			if (fullFilePath.EndsWith(fileType) || strlen(fileType)==0)
			{
				// Generate the file name to save
				atString storedFilename;
				if (storeFilePath)
				{
					storedFilename += path;
					storedFilename += "/";
				}

				storedFilename += folderResults[i]->m_Name;

				// Strip extension?
				if (!storeFileExtension)
				{
					int extensionIndex = storedFilename.LastIndexOf('.');
					Assert(extensionIndex != -1);

					storedFilename.Truncate(extensionIndex);
				}

				// Save file name
				m_FoundFiles.PushAndGrow(storedFilename);
			}
			else if ((fullFilePath.LastIndexOf('.')<0) && recursive)
			{
				// This is a folder, enumerate it too
				FindFilesInDirectory(fullFilePath, fileType, true, storeFilePath, storeFileExtension);
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

#endif // __BANK
