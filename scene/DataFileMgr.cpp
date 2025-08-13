#include "scene/DataFileMgr.h"
#include "scene/DataFileMgr_parser.h"

// Rage headers
#include "fwsys/fileExts.h"
#include "parser/manager.h"
#include "parsercore/stream.h"
#include "streaming/streamingengine.h"
#include "streaming/streaminginfo.h"
#include "streaming/streamingmodule.h"
#include "streaming/streamingmodulemgr.h"
#include "string/stringutil.h"
#include "system/param.h"
#include "system/platform.h"
#include "system/nelem.h"
#include "scene/dlc_channel.h"
#include "scene/ExtraContent.h"
#include "scene/scene.h"
#include "control/leveldata.h"
#include "control/gamelogic.h"
#include "file/device_relative.h"

eDFMI_UnloadType CDataFileMount::sm_UnloadBehaviours[CDataFileMgr::INVALID_FILE];
atRangeArray<CDataFileMountInterface *, CDataFileMgr::INVALID_FILE> CDataFileMount::sm_Interfaces;
bool CDataFileMount::sm_overrideUnload = true;
eDFMI_UnloadType CDataFileMount::sm_unloadBehaviour = eDFMI_UnloadDefault;

XPARAM(usefatcuts);

bool CDataFileMgr::ChangeSetData::IsValid(bool executing) const
{
	bool returnValue = true;

	// Check if this changeset has any map associations
	if (m_associatedMap.length() > 0)
	{
		s32 currLevelIdx = CGameLogic::GetCurrentLevelIndex();
		s32 reqLevelIdx = CGameLogic::GetRequestedLevelIndex();
		s32 associatedMapIdx = CScene::GetLevelsData().GetLevelIndexFromTitleName(m_associatedMap.c_str());

		// If there's no transition just check for association
		if (currLevelIdx == reqLevelIdx)
			returnValue = associatedMapIdx == currLevelIdx;
		else
		{
			// There's some form of transition, only allow changes for the current map to be reverted
			if (associatedMapIdx == currLevelIdx)
				returnValue = !executing;
			else
				returnValue = associatedMapIdx == reqLevelIdx;
		}

		if (!returnValue)
		{
			NOTFINAL_ONLY(Displayf("ChangeSetData::IsValid - change set is not valid due to map association with [%s] [%s]", 
				m_associatedMap.c_str(), CScene::GetLevelsData().GetFriendlyName(associatedMapIdx));)
		}
	}

	return returnValue;
}

strIndex CDataFileMgr::ResourceReference::Resolve() const
{
	// Get the module first.
	char extension[sizeof(m_Extension)];

	memcpy(extension, m_Extension, sizeof(m_Extension));

	if (extension[0] == '#')
	{
		extension[0] = g_sysPlatform;
	}

	strStreamingModule *module = strStreamingEngine::GetInfo().GetModuleMgr().GetModuleFromFileExtension(extension);

	if (module)
	{
		strLocalIndex slot = module->FindSlot(m_AssetName.c_str());

		if (slot != -1)
		{
			return module->GetStreamingIndex(slot);
		}
	}

	Assertf(false, "Unable to resolve %s.%s", m_AssetName.c_str(), m_Extension);
	return strIndex();
}



CDataFileMgr::CDataFileMgr()
{
	// Set up the environment variables.
	m_EnvVariableMap.Insert(ATSTRINGHASH("PLATFORM", 0x02fbe482d), ConstString(RSG_PLATFORM_ID));
	m_addingPatchFiles = false;
}

#if __BANK
bool CDataFileMgr::GetFatcuts()
{
	return PARAM_usefatcuts.Get();
}
#endif // __BANK

FASTRETURNCHECK(const char *) CDataFileMgr::RemapDataFileName(const char *origFile, char *NOTFINAL_ONLY(outTargetBuffer), size_t NOTFINAL_ONLY(targetBufferSize))
{
#if !__FINAL
	if (PARAM_usefatcuts.Get())
	{
		const char *filename = ASSET.FileName(origFile);
		size_t string_length_of_path = filename - origFile;

		if (PARAM_usefatcuts.Get())
		{
			Displayf("Testing %s", filename);
			if (stricmp(filename, "common_cutscene.meta") == 0)
			{
				memcpy(outTargetBuffer, origFile, Min(string_length_of_path, targetBufferSize));
				outTargetBuffer[Min(string_length_of_path, targetBufferSize)] = 0;
				safecat(outTargetBuffer, "/", targetBufferSize);
				safecat(outTargetBuffer, "common_cutscene_fat.meta", targetBufferSize);

				return outTargetBuffer;
			}
			else if (stricmp(filename, "cuts.meta") == 0)
			{
				memcpy(outTargetBuffer, origFile, Min(string_length_of_path, targetBufferSize));
				outTargetBuffer[Min(string_length_of_path, targetBufferSize)] = 0;
				safecat(outTargetBuffer, "/", targetBufferSize);
				safecat(outTargetBuffer, "cuts_fat.meta", targetBufferSize);

				return outTargetBuffer;
			}
		}
	}
#endif // !__FINAL

	return origFile;
}

#if __ASSERT
void CDataFileMgr::VerifyXmlContents(ContentsOfDataFileXml& XmlContents, const char* pFilename) const
{
	// Fill temporary array with change sets' enabled and disabled files.
	atArray<const char*> tempArray;
	for(int j=0; j<XmlContents.m_contentChangeSets.GetCount(); ++j)
	{
		const ContentChangeSet* pChangeSet = &(XmlContents.m_contentChangeSets[j]);
		for(int i=0; i<pChangeSet->m_filesToEnable.GetCount(); ++i)
		{
			tempArray.PushAndGrow(pChangeSet->m_filesToEnable[i].c_str());
		}
		for(int i=0; i<pChangeSet->m_filesToDisable.GetCount(); ++i)
		{
			tempArray.PushAndGrow(pChangeSet->m_filesToDisable[i].c_str());
		}

		for(int k=0; k<pChangeSet->m_mapChangeSetData.GetCount(); ++k)
		{
			const ChangeSetData* pData = &(pChangeSet->m_mapChangeSetData[k]);
			for(int i=0; i<pData->m_filesToEnable.GetCount(); ++i)
			{
				tempArray.PushAndGrow(pData->m_filesToEnable[i].c_str());
			}
			for(int i=0; i<pChangeSet->m_mapChangeSetData[k].m_filesToDisable.GetCount(); ++i)
			{
				tempArray.PushAndGrow(pData->m_filesToDisable[i].c_str());
			}
		}
	}

	// Check for actual data file dupes. Let's hope not.
	for (int i = 0; i < XmlContents.m_dataFiles.GetCount(); ++i) {
		for (int j = i+1; j < XmlContents.m_dataFiles.GetCount(); ++j) {
			bool bFilesMatch = (strnicmp(XmlContents.m_dataFiles[i].m_filename,  XmlContents.m_dataFiles[j].m_filename, 128) == 0);
			Assertf(!bFilesMatch, "Duplicate data file '%s' in file '%s' (indices %i and %i)",  XmlContents.m_dataFiles[i].m_filename, pFilename, i, j);
		}
	}
	
	// Check if content.xml belongs to DLC device or title update.
	bool isDLCorUpdate = ( !strnicmp(pFilename, "DLC", 3) || !strnicmp(pFilename, "update", 6) );
	if(isDLCorUpdate)
	{
		// Automatically add the content.xml's folder to allowed ones.
		AddDefaultAllowedFolders(XmlContents, pFilename);
	}

	// Check the temporary array for issues.
	for(int i=0; i<tempArray.GetCount(); ++i)
	{
		// Disallowed folders.
		if(XmlContents.m_allowedFolders.GetCount() && isDLCorUpdate)
		{
			Verifyf(IsInAllowedFolder(XmlContents, tempArray[i]), "Data file %s is not placed in one of the allowed folders", tempArray[i]);
		}

		// Duplicate entries.
		for(int j=i+1; j<tempArray.GetCount(); ++j)
		{
			if(!Verifyf(strcmp(tempArray[i], tempArray[j]), "Duplicate entry of %s found in %s.", tempArray[i], pFilename))
			{
				break;
			}
		}
	}
}

void CDataFileMgr::AddDefaultAllowedFolders(ContentsOfDataFileXml& XmlContents, const char* pFilename) const
{
	if(const char* pChar = stristr(pFilename, ":")){
		atString &pStr = XmlContents.m_allowedFolders.Grow();
		pStr.Set(pFilename, RAGE_MAX_PATH, 0, static_cast<int>(pChar-pFilename)+1);
	}

	if(const char* pChar = stristr(pFilename, "CRC:"))
	{
		atString &pStr = XmlContents.m_allowedFolders.Grow();
		pStr.Set(pFilename, RAGE_MAX_PATH, 0, static_cast<int>(pChar-pFilename));
		pStr += ":";
	}
}

bool CDataFileMgr::IsInAllowedFolder(const ContentsOfDataFileXml& XmlContents, const char* filePath) const
{
	// If there are no specified folders then assume the file is setup correctly (for backwards compatibility)
	if(XmlContents.m_allowedFolders.empty())
	{
		return true;
	}

	// Check if file lies within specified folders.
	for(int k=0; k<XmlContents.m_allowedFolders.GetCount(); ++k)
	{
		if(	!strnicmp(XmlContents.m_allowedFolders[k].c_str(), filePath, XmlContents.m_allowedFolders[k].GetLength()) )
		{
			return true;
		}
	}

	return false;
}

bool CDataFileMgr::CheckCRCValidity(const DataFile* dataFile)
{
	const DataFile* pFile = DATAFILEMGR.GetFirstFile(dataFile->m_fileType);
	if( !strnicmp(pFile->m_filename, "commoncrc:", 10) && !strnicmp(dataFile->m_filename, "dlc", 3) )
	{
		char device[RAGE_MAX_PATH] = {0};
		strncpy(device, dataFile->m_filename, static_cast<int>(strcspn(dataFile->m_filename, "/")) + 1);
		bool isCompatPack = EXTRACONTENT.GetContentByDevice(device)->GetIsCompatibilityPack();
		if(!stristr(dataFile->m_filename, "CRC:") && isCompatPack)
		{	
			return false;
		}
	}
	return true;
}
#endif // __ASSERT

void CDataFileMgr::Load(const char *pFilename, bool bCullOtherPlatformData /*= true*/)
{
	ContentsOfDataFileXml XmlContents;

	char FilenameWithoutExtension[256];
	ASSET.RemoveExtensionFromPath(FilenameWithoutExtension, 256, pFilename);

	parSettings s = PARSER.Settings();
	s.SetFlag(parSettings::CULL_OTHER_PLATFORM_DATA, bCullOtherPlatformData);
	//@@: location CDATAFILEMGR_LOAD_META_EXISTS
	if (ASSET.Exists(FilenameWithoutExtension, "meta"))
	{
		PARSER.LoadObject(FilenameWithoutExtension, "meta", XmlContents, &s);
	}
	else if (ASSET.Exists(pFilename, META_FILE_EXT))
	{
		Verifyf(fwPsoStoreLoader::LoadDataIntoObject(pFilename, META_FILE_EXT, XmlContents), "Failed to load %s", pFilename);
	}
	else
	{
		PARSER.LoadObject(FilenameWithoutExtension, "xml", XmlContents, &s);
	}

#if __ASSERT
	// Check contents of content.xml for errors.
	VerifyXmlContents(XmlContents, pFilename);
#endif // __ASSERT

	for(int i=0;i<XmlContents.m_dataFiles.GetCount();i++)
	{
		const char* currentFile = XmlContents.m_dataFiles[i].m_filename;
		//First check if this file should get replaced (ptfx/script/fatcuts etc)
		char replacementPath[RAGE_MAX_PATH];
		if(CFileLoader::CheckFileForReplacements(currentFile,replacementPath,RAGE_MAX_PATH))
		{
			Displayf("Changing %s",XmlContents.m_dataFiles[i].m_filename);
			strcpy(XmlContents.m_dataFiles[i].m_filename,replacementPath);
			Displayf("to %s",XmlContents.m_dataFiles[i].m_filename);
		}
		if(!fiDevice::CheckFileValidity(currentFile))
		{
			//Delete invalid files
			Assertf(false,"invalid devicename in the datafile %s",currentFile);
			XmlContents.m_dataFiles.Delete(i);
			i--;
		}
		// Check if DLC files are properly CRC'd.
		Assertf(CheckCRCValidity(&(XmlContents.m_dataFiles[i])), "%s in %s should be CRC'd but is not!", XmlContents.m_dataFiles[i].m_filename, pFilename);
	}

	// Check for any collisions between CCSs
	for (int x = 0; x < XmlContents.m_contentChangeSets.GetCount(); x++)
	{
		ContentChangeSet& changeSet = XmlContents.m_contentChangeSets[x];

		// If we have a collision we cache the collider, remove all its files from this content XML data preserving any other non colliding CCSs
		ContentChangeSet* higherOrderCCS = m_contentChangeSets.Access(changeSet.m_changeSetName.c_str());
		if (higherOrderCCS)
		{
		#if !__FINAL
			// If we have a collision verify that the files identical between the two CCSs
			if (higherOrderCCS->m_filesToEnable.GetCount() == changeSet.m_filesToEnable.GetCount())
			{
				char highOrderFile[256] = { 0 };
				char lowOrderFile[256] = { 0 };
				char* highOrderFileNoDevice = NULL;
				char* lowOrderFileNoDevice = NULL;

				for (u32 i = 0; i < higherOrderCCS->m_filesToEnable.GetCount(); i++)
				{
					formatf(highOrderFile, sizeof(highOrderFile), "%s", higherOrderCCS->m_filesToEnable[i].c_str());
					formatf(lowOrderFile, sizeof(lowOrderFile), "%s", changeSet.m_filesToEnable[i].c_str());

					highOrderFileNoDevice = strstr(highOrderFile, ":/");
					lowOrderFileNoDevice = strstr(lowOrderFile, ":/");

					if (strcmp(highOrderFileNoDevice, lowOrderFileNoDevice) != 0)
					{
						Assertf(0, "CDataFileMgr::Load - %s CCSs Collide but they are not identical, check order of files! [%s vs %s]", 
							changeSet.m_changeSetName.c_str(), highOrderFileNoDevice, lowOrderFileNoDevice);	

						break;
					}
				}
			}
			else
			{
				Assertf(0, "CDataFileMgr::Load - %s CCSs Collide but they are not identical, check file counts! [%i vs %i]",
					changeSet.m_changeSetName.c_str(), higherOrderCCS->m_filesToEnable.GetCount(), changeSet.m_filesToEnable.GetCount());
			}
		#endif

			atHashString newCollision = changeSet.m_changeSetName.c_str();

			dlcDebugf3("CDataFileMgr::Load - There is a collision between CCSs [%s]", changeSet.m_changeSetName.c_str());

			m_changeSetCollisions.PushAndGrow(newCollision, 1);

			for (u32 i = 0; i < changeSet.m_filesToEnable.GetCount(); i++)
			{
				for (u32 j = 0; j < XmlContents.m_dataFiles.GetCount(); j++)
				{
					if (strcmp(XmlContents.m_dataFiles[j].m_filename, changeSet.m_filesToEnable[i].c_str()) == 0)
					{
						dlcDebugf3("CDataFileMgr::Load - Deleting file reference [%s]", XmlContents.m_dataFiles[j].m_filename);

						XmlContents.m_dataFiles.Delete(j);
						j--;
					}
				}
			}

			XmlContents.m_contentChangeSets.DeleteFast(x);
			x--;
		}
	}

	s32 index = 0;
	const s32 numOfDisabledFiles = XmlContents.m_disabledFiles.GetCount();
	while(index < numOfDisabledFiles)
	{
		// Printf("CDataFileMgr::Load - index = %d disabled_filename = %s\n", index, XmlContents.m_disabledFiles[index].c_str());
		AddDisabledFile(XmlContents.m_disabledFiles[index].c_str());

		index++;
	}

	const s32 numOfIncludedDataFiles = XmlContents.m_includedDataFiles.GetCount();
	for (int x=0; x<numOfIncludedDataFiles; x++)
	{
		char fileBuffer[RAGE_MAX_PATH];
		const char *dataFileName = XmlContents.m_includedDataFiles[x].c_str();
		const char *mappedDataFile = RemapDataFileName(dataFileName, fileBuffer, sizeof(fileBuffer));
		Load(mappedDataFile);
	}

	s32 xml_file_index = 0;
	//@@: location CDATAFILEMGR_LOAD_GET_XML_COUNT
	const s32 numOfIncludedXmlFiles = XmlContents.m_includedXmlFiles.GetCount();
	while(xml_file_index < numOfIncludedXmlFiles)
	{
		index = 0;
		const s32 numOfDataFilesInThisXmlFile = XmlContents.m_includedXmlFiles[xml_file_index].m_dataFiles.GetCount();
		while (index < numOfDataFilesInThisXmlFile)
		{
			//Printf("CDataFileMgr::Load (included xml) - index = %d filetype = %d filename = %s\n", index, XmlContents.m_includedXmlFiles[xml_file_index].m_dataFiles[index].m_fileType,
			//	XmlContents.m_includedXmlFiles[xml_file_index].m_dataFiles[index].m_filename);


			const DataFile &dataFile = XmlContents.m_includedXmlFiles[xml_file_index].m_dataFiles[index];
			AddFile(dataFile,pFilename);

			index++;
		}

		xml_file_index++;
	}

	index = 0;
	const s32 numOfDataFiles = XmlContents.m_dataFiles.GetCount();
	while(index < numOfDataFiles)
	{
		//Printf("CDataFileMgr::Load - index = %d filetype = %d filename = %s\n", index, XmlContents.m_dataFiles[index].m_fileType,
		//	XmlContents.m_dataFiles[index].m_filename);

		AddFile(XmlContents.m_dataFiles[index],pFilename);

		index++;
	}

	for (int x=0; x<XmlContents.m_contentChangeSets.GetCount(); x++)
	{
		ContentChangeSet &changeSet = XmlContents.m_contentChangeSets[x];
		for(int y=0;y<changeSet.m_filesToEnable.GetCount();y++)
		{
			const char* currentFile = changeSet.m_filesToEnable[y].c_str();
			char replacementPath[RAGE_MAX_PATH];
			if(CFileLoader::CheckFileForReplacements(currentFile,replacementPath,RAGE_MAX_PATH))
			{
				Displayf("Changing %s",changeSet.m_filesToEnable[y].c_str());
				changeSet.m_filesToEnable[y].Set(replacementPath,RAGE_MAX_PATH,0);
				Displayf("to %s",changeSet.m_filesToEnable[y].c_str());
			}
				
		}
		m_contentChangeSets[changeSet.m_changeSetName.c_str()] = changeSet;
	}

	const s32 numOfPatchFiles = XmlContents.m_patchFiles.GetCount();
	for (int x = 0; x < numOfPatchFiles; x++)
	{
		const char* patchFileName = XmlContents.m_patchFiles[x].c_str();
		PARSER.AddPatchFileToList(patchFileName);
	}

}

void CDataFileMgr::GetCCSArchivesToEnable(atHashString changeSetName, bool execute, atMap<s32, bool>& archives)
{
	if (ContentChangeSet* ccs = m_contentChangeSets.Access(changeSetName))
	{
		s32 imgIdx = -1;
		char expandedFilename[RAGE_MAX_PATH] = { 0 };

		for (s32 i = 0; i < ccs->m_filesToEnable.GetCount(); i++)
		{
			DATAFILEMGR.ExpandFilename(ccs->m_filesToEnable[i].c_str(), expandedFilename, sizeof(expandedFilename));
			imgIdx = strPackfileManager::FindImage(expandedFilename);

			if (imgIdx != -1 && archives.Access(imgIdx) == NULL)
				archives.Insert(imgIdx, true);
		}

		for (s32 i = 0; i < ccs->m_mapChangeSetData.GetCount(); i++)
		{
			ChangeSetData& currData = ccs->m_mapChangeSetData[i];

			if (currData.IsValid(execute))
			{
				for (s32 j = 0; j < currData.m_filesToEnable.GetCount(); j++)
				{
					DATAFILEMGR.ExpandFilename(currData.m_filesToEnable[j].c_str(), expandedFilename, sizeof(expandedFilename));
					imgIdx = strPackfileManager::FindImage(expandedFilename);

					if (imgIdx != -1 && archives.Access(imgIdx) == NULL)
						archives.Insert(imgIdx, true);
				}
			}
		}
	}
}

void CDataFileMgr::DestroyDependentsGraph()
{
	strStreamingEngine::GetInfo().DestroyDependentGraphs();
}

void CDataFileMgr::CreateDependentsGraph(atHashString changeSetName, bool execute)
{
	if (ContentChangeSet* ccs = m_contentChangeSets.Access(changeSetName))
	{
		atArray<s32> filesToEnableCache;
		atArray<s32> filesToInvalidateCache;
		s32 imgIdx = -1;
		char expandedFilename[RAGE_MAX_PATH] = { 0 };

		for (s32 i = 0; i < ccs->m_mapChangeSetData.GetCount(); i++)
		{
			ChangeSetData& currData = ccs->m_mapChangeSetData[i];

			if (currData.IsValid(execute))
			{
				for (s32 j = 0; j < currData.m_filesToEnable.GetCount(); j++)
				{
					DATAFILEMGR.ExpandFilename(currData.m_filesToEnable[j].c_str(), expandedFilename, sizeof(expandedFilename));
					imgIdx = strPackfileManager::FindImage(expandedFilename);

					if (imgIdx != -1)
					{
						filesToEnableCache.PushAndGrow(imgIdx, 1);
					}
				}

				for (s32 j = 0; j < currData.m_filesToInvalidate.GetCount(); j++)
				{
					DATAFILEMGR.ExpandFilename(currData.m_filesToInvalidate[j].c_str(), expandedFilename, sizeof(expandedFilename));
					imgIdx = strPackfileManager::FindImage(expandedFilename);

					if (imgIdx != -1)
					{
						if (const atArray<s32>* associatedFiles = strStreamingEngine::GetInfo().GetPackfileRelationships(imgIdx))
						{
							for (s32 k = 0; k < associatedFiles->GetCount(); k++)
							{
								imgIdx = (*associatedFiles)[k];

								if (imgIdx != -1)
									filesToInvalidateCache.PushAndGrow(imgIdx, (u16)associatedFiles->GetCount());
							}
						}
						else
						{
							filesToInvalidateCache.PushAndGrow(imgIdx, 1);
						}
					}
				}
			}
		}

		strStreamingEngine::GetInfo().CreateDependentsGraph(filesToEnableCache);
		strStreamingEngine::GetInfo().CreateDependentsGraph(filesToInvalidateCache);
	}
}

void CDataFileMgr::Unload(const char *pFilename, bool bCullOtherPlatformData /*= true*/)
{
	ContentsOfDataFileXml XmlContents;

	char FilenameWithoutExtension[256];
	ASSET.RemoveExtensionFromPath(FilenameWithoutExtension, 256, pFilename);

	parSettings s = PARSER.Settings();
	s.SetFlag(parSettings::CULL_OTHER_PLATFORM_DATA, bCullOtherPlatformData);

	if (ASSET.Exists(FilenameWithoutExtension, "meta"))
		PARSER.LoadObject(FilenameWithoutExtension, "meta", XmlContents, &s);
	else
		PARSER.LoadObject(FilenameWithoutExtension, "xml", XmlContents, &s);

	s32 index = 0;
	const s32 numOfDisabledFiles = XmlContents.m_disabledFiles.GetCount();
	while(index < numOfDisabledFiles)
	{
		RemoveDisabledFile(XmlContents.m_disabledFiles[index].c_str());
		index++;
	}

	index = 0;
	const s32 numOfDataFiles = XmlContents.m_dataFiles.GetCount();
	while(index < numOfDataFiles)
	{
		RemoveFile(XmlContents.m_dataFiles[index].m_filename);
		index++;
	}

	s32 xml_file_index = 0;
	const s32 numOfIncludedXmlFiles = XmlContents.m_includedXmlFiles.GetCount();
	while(xml_file_index < numOfIncludedXmlFiles)
	{
		index = 0;
		const s32 numOfDataFilesInThisXmlFile = XmlContents.m_includedXmlFiles[xml_file_index].m_dataFiles.GetCount();
		while (index < numOfDataFilesInThisXmlFile)
		{
			const DataFile &dataFile = XmlContents.m_includedXmlFiles[xml_file_index].m_dataFiles[index];
			RemoveFile(dataFile.m_filename);
			index++;
		}

		xml_file_index++;
	}

	for (int x = 0; x < XmlContents.m_contentChangeSets.GetCount(); x++)
	{
		ContentChangeSet& changeSet = XmlContents.m_contentChangeSets[x];

		m_contentChangeSets.Delete(changeSet.m_changeSetName.c_str());
	}
}

const CDataFileMgr::ContentChangeSet *CDataFileMgr::GetContentChangeSet(atHashString contentChangeSetName) const
{
	return m_contentChangeSets.Access(contentChangeSetName);
}

CDataFileMgr::DataFile *CDataFileMgr::FindDataFile(atHashString fileName)
{
	int count = m_dataFiles.GetCount();

	for (int x=0; x<count; x++)
	{
		if (fileName == m_dataFiles[x].m_filename)
		{
			return &m_dataFiles[x];
		}
	}

	return NULL;
}


void CDataFileMgr::ExpandFilename(const char *filename, char *outBuffer, size_t bufferSize)
{
	// Copy the string, and find all special characters on the way.
	// We'll copy the string into the target buffer first, including the variables. As we'll
	// encounter the end of a varible, we'll make use of the fact that it's null-terminated at
	// that point so we can easily look it up and replace it.
	char *lastVarBegin = NULL;

	bufferSize--;

	while (*filename && bufferSize > 0)
	{
		char chr = *(filename++);

		if (chr != '%')
		{
			*(outBuffer++) = chr;
			bufferSize--;
		}
		else
		{
			// This could be the beginning or end of a variable.
			if (!lastVarBegin)
			{
				// It's the beginning of a variable.
				lastVarBegin = outBuffer;

				*(outBuffer++) = chr;
				bufferSize--;
			}
			else
			{
				// It's the end of a variable. Let's look it up.
				// If it's two % signs right after one another, let's assume the user intended to
				// to escape a single % sign.
				if (lastVarBegin + 1 != outBuffer)
				{
					// We still need to terminate the buffer, it could contain data if we previously expanded
					// a longer variable name with a shorter string.
					*outBuffer = 0;

					ConstString *var = m_EnvVariableMap.Access(atStringHash(&lastVarBegin[1]));

					// If this doesn't resolve to a variable, just leave the string untouched, as DOS would do.
					if (var)
					{
						// It IS a proper variable, so replace it.
						size_t varLength = strlen(var->c_str());
						bufferSize += outBuffer - lastVarBegin;
						strncpy(lastVarBegin, var->c_str(), bufferSize);
						bufferSize = Max((size_t) 0, bufferSize - varLength);
						outBuffer = lastVarBegin + varLength;
					}
				}

				lastVarBegin = NULL;
			}
		}
	}

	*outBuffer = 0;
}

void CDataFileMgr::RemoveMapDataFiles()
{
	atArray<DataFile> dataFilesToKeep;

	s32 numberOfEntries = m_dataFiles.GetCount();
	s32 loop = 0;
	for (loop = 0; loop < numberOfEntries; loop++)
	{
		if (m_dataFiles[loop].m_persistent)
		{
			dataFilesToKeep.PushAndGrow(m_dataFiles[loop]);
		}
		else
		{
			switch (m_dataFiles[loop].m_fileType)
			{
				case RPF_FILE :
				case IDE_FILE :
				case DELAYED_IDE_FILE :
				case IPL_FILE :
				case PERMANENT_ITYP_FILE :
					//	don't copy these types across to dataFilesToKeep
					break;

				default:
					dataFilesToKeep.PushAndGrow(m_dataFiles[loop]);
					break;
			}
		}
	}

	// Fix up the m_handle of all the entries in dataFilesToKeep
	numberOfEntries = dataFilesToKeep.GetCount();
	for (loop = 0; loop < numberOfEntries; loop++)
	{
		dataFilesToKeep[loop].m_handle = loop;
	}

	m_dataFiles.Reset();
	m_dataFiles = dataFilesToKeep;

	//	Also clear the array of disabled files?
	m_disabledFiles.Reset();
}

void CDataFileMgr::Clear()
{
	m_dataFiles.Reset();
	m_disabledFiles.Reset();

	m_LockedFilesCount = 0;
	m_LockedDisabledFilesCount = 0;
}

void CDataFileMgr::SnapshotFiles()
{
	m_LockedFilesCount = m_dataFiles.GetCount();
	m_LockedDisabledFilesCount = m_disabledFiles.GetCount();
}

void CDataFileMgr::RecallSnapshot()
{
	m_dataFiles.Resize(m_LockedFilesCount);
	m_disabledFiles.Resize(m_LockedDisabledFilesCount);
}

#if __BANK
void CDataFileMount::DebugOutputTable()
{
	Displayf("=============================================");
	Displayf("======= CDataFileMount interface table ======");
	Displayf("=============================================");
#if PARSER_ALL_METADATA_HAS_NAMES
	for(int i=0; i < sm_Interfaces.GetMaxCount(); ++i)
	{
		Displayf("[%s]:  %s", parser_DataFileType_Strings[i], sm_Interfaces[i]? "valid":"NULL");
	}
#else
	Displayf("CAN'T DISPLAY LIST IF !PARSER_ALL_METADATA_HAS_NAMES");
#endif
	Displayf("=============================================");
}
#endif
