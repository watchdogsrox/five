//
// filename:	streamedscripts.cpp
// description:	
//

// --- Include Files ------------------------------------------------------------

// C headers
// Rage headers
#include "system/param.h"
#include "system/timer.h"

// Game headers
#include "script\streamedscripts.h"
#include "script/script_helper.h"
#include "streaming/streaming.h"
#include "streaming/packfilemanager.h"
#include "streaming/streamingengine.h"
#include "streaming/streamingmodule.h"
#include "tools/smoketest.h"
#include "fwsys/fileExts.h" 

PARAM(validatescripts, "[streamedscripts] validate all scripts for unrecognised native commands by loading them all");


// --- Defines ------------------------------------------------------------------

// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------

// --- Globals ------------------------------------------------------------------

CStreamedScripts g_StreamedScripts;

// --- Static Globals -----------------------------------------------------------

// --- Static class members -----------------------------------------------------

// --- Code ---------------------------------------------------------------------

CStreamedScripts::CStreamedScripts() : fwAssetRscStore<scrProgram, fwAssetNameDef<scrProgram> >("ScriptStore", SCRIPT_FILE_ID, CONFIGURED_FROM_FILE, 1170, true, scrProgram::RORC_VERSION)
{
	m_BGScriptPackStreamIndex = (strIndex)strIndex::INVALID_INDEX;
}

u32 CStreamedScripts::GetStreamerReadFlags() const
{
	return pgStreamer::ENCRYPTED;
}

// strLocalIndex CStreamedScripts::Register(const char* name)
// {
// 	atString test(name);
// 
// 	test.Lowercase();
// 	if (test[0] == 'm' && test[1] == 'o' && test[2] == 'd' && test[3] == 'm' && test[4] == 'e' && test[5] == 'n' && test[6] == 'u')
// 	{
// 		FastAssert(false);
// 		CTheScripts::Frack2();
// 	}
// 
// 	return fwAssetRscStore<scrProgram, fwAssetNameDef<scrProgram> >::Register(name);
// }

	

//
// name:		fwAssetStore<scrProgram, fwAssetNameDef<scrProgram> >::Remove
// description:	Delete script. We have to special case this as the destructor for a script program is private
template<> void fwAssetRscStore<scrProgram, fwAssetNameDef<scrProgram> >::Remove(strLocalIndex index)
{
	fwAssetNameDef<scrProgram>* pDef = GetSlot(index);

	scriptAssertf(pDef, "No script at this slot");

	// Free the attached program if there is one.  That in turn releases the pagetable.  Sigh.
	// Displayf("1. Removing resourced script '%s'...",pDef->m_name.GetCStr());
	SAFE_REMOVE_KNOWN_REF(pDef->m_pObject);
	// Displayf("2. Releasing pagetable");
	ASSERT_ONLY(int result =) pDef->m_pObject->Release();
	Assertf(!result,"Program '%s' didn't release properly, will leak!",pDef->m_name.GetCStr());
	// Displayf("3. Removed resourced script '%s'...",pDef->m_name.GetCStr());
	pDef->m_pObject = NULL;
}

strLocalIndex CStreamedScripts::Register(const char* name)
{
	strLocalIndex index = fwAssetRscStore<scrProgram, fwAssetNameDef<scrProgram> >::Register(name);

	OUTPUT_ONLY(printf("register slot (%d) : name (%s)\n", index.Get(), name);)

	return(index);
}

void CStreamedScripts::StoreBGScriptPackStreamIndex(strIndex val)
{
	Assertf(m_BGScriptPackStreamIndex.IsInvalid() || (m_BGScriptPackStreamIndex == val), "Should only be setting BG script pack once or to the same value (Current: %u, New: %u)", m_BGScriptPackStreamIndex.Get(), val.Get());
	m_BGScriptPackStreamIndex = val;
}

// fingerprint the current script store, using the offsets of all the files in the pack
u32 CStreamedScripts::CalculateScriptStoreFingerprint(u32 startHash, bool bDump)
{
	if (bDump)
	{
		gnetDebug3("Calc script store fingerprint : starting hash %d", startHash);
	}

	u32 count = fwAssetRscStore<scrProgram, fwAssetNameDef<scrProgram> >::GetCount();
	u32 scriptStoreHash = startHash;
	atFixedArray<s32, 32> m_localRequests;
	m_localRequests.Reset();
	
	// make sure all the info about pack files are in memory
	for(u32 idx = 0; idx< count; idx++)
	{
		if (GetSlot(strLocalIndex(idx)) != NULL)
		{	
			strStreamingInfo* info = strStreamingEngine::GetInfo().GetStreamingInfo(GetStreamingIndex(strLocalIndex(idx)));

			if (info && (m_BGScriptPackStreamIndex != GetStreamingIndex(strLocalIndex(idx))))
			{
				const strFileHandle reFileHandle = info->GetHandle();
				const fiCollection *device = fiCollection::GetCollection(reFileHandle);

				if (device)			//don't request bgScripts
				{
					s32 imageIndex = strPackfileManager::GetImageFileIndexFromHandle(reFileHandle).Get();
					if (imageIndex != -1 && (m_localRequests.Find(imageIndex) == -1))
					{
						if (CStreaming::GetObjectIsDeletable(strLocalIndex(imageIndex), strPackfileManager::GetStreamingModuleId()))
						{
							m_localRequests.Push(imageIndex);
							CStreaming::RequestObject(strLocalIndex(imageIndex), strPackfileManager::GetStreamingModuleId(), STRFLAG_FORCE_LOAD|STRFLAG_DONTDELETE);
						}
					}
				}
			}
		}
	}

	g_strStreamingInterface->LoadAllRequestedObjects();

	for(u32 idx = 0; idx< count; idx++)
	{
		if (GetSlot(strLocalIndex(idx)) != NULL)
		{
			strStreamingInfo* info = strStreamingEngine::GetInfo().GetStreamingInfo(GetStreamingIndex(strLocalIndex(idx)));
			if (info && (m_BGScriptPackStreamIndex != GetStreamingIndex(strLocalIndex(idx))))		// not bgScripts
			{
				const strFileHandle reFileHandle = info->GetHandle();
				const fiCollection *device = fiCollection::GetCollection(reFileHandle);
				if (device)			
				{
					const fiPackEntry &resource = device->GetEntryInfo(reFileHandle);
					u32 fileOffset = resource.GetFileOffset();
					scriptStoreHash = atDataHash(&fileOffset, sizeof(fileOffset), scriptStoreHash);

#if !__NO_OUTPUT
					if (bDump)
					{
						strStreamingFile* pFile = strPackfileManager::GetImageFileFromHandle(reFileHandle);
						gnetDebug3("slot (%03d) : fileoffset (%08x) : currHash (%08x)  name '%s' : pack '%s'", idx, fileOffset, scriptStoreHash, GetName(idx), pFile ? pFile->m_name.GetCStr() : "<NULL>");
					}
#endif
				}
#if !__NO_OUTPUT
				else
				{
					if (bDump)
					{
						gnetDebug3("slot %d : no device", idx);
					}
				}
#endif
			}
#if !__NO_OUTPUT
			else
			{
				if (bDump)
				{
					gnetDebug3("slot %d : no info", idx);
				}
			}
#endif
		}
	}

	// cleanup
	for(u32 idx = 0; idx< m_localRequests.GetCount(); idx++)
	{
		CStreaming::ClearRequiredFlag(strLocalIndex(m_localRequests[idx]), strPackfileManager::GetStreamingModuleId(), STRFLAG_DONTDELETE);
	}

#if !__NO_OUTPUT
	if (bDump)
	{
		gnetDebug3("Final script store hash : %x", scriptStoreHash);
	}
#endif

	return(scriptStoreHash);
}


//
// RemoveRef(): remove object referencing this script. If no objects
//			are left referencing this script delete it
//
void CStreamedScripts::RemoveRef(strLocalIndex index, strRefKind refKind)
{
	fwAssetRscStore<scrProgram, fwAssetNameDef<scrProgram> >::RemoveRef(index, refKind);
	fwAssetNameDef<scrProgram>* pDef = GetSlot(index);

	if(GetNumRefs(index) + pDef->m_pObject->GetNumRefs() <= 0)
	{
#ifdef DEBUG
		Displayf("Removing object %s\n", pDef->m_name);
#endif		
		StreamingRemove(index);
	}
}

//
// name:		GetNumRefs
// description:	Return the number of objects referencing this script
//
int CStreamedScripts::GetNumRefs(strLocalIndex index) const
{
	const fwAssetNameDef<scrProgram>* pDef = GetSlot(index);

	Assertf(pDef, "No object at this slot");

	int numRefs = fwAssetRscStore<scrProgram, fwAssetNameDef<scrProgram> >::GetNumRefs(index);
	if(pDef->m_pObject)
		numRefs += (pDef->m_pObject->GetNumRefs()-1);
	return numRefs;
}

void CStreamedScripts::ShutdownLevel()
{
	// Force a full remove: we shouldn't have to, but it seems scripts are too stupid to know when we're restarting...
	for(int i=0; i<GetSize(); i++)
	{
		if(IsValidSlot(strLocalIndex(i)) && Get(strLocalIndex(i)))
		{
			ClearRequiredFlag(i,STR_DONTDELETE_MASK);
		}
	}	
	
	RemoveAll();
}

#if __DEV

//
// name:		CStreamedScripts::GetIndexOfScriptImgContainingScoFile
// description:	Convert the id of the .sco file within the streamed script module
//					to the index of the script image file that contains the .sco
u8 CStreamedScripts::GetIndexOfScriptImgContainingScoFile(int nScriptId)
{
	strIndex nStreamingIndex = GetStreamingModule()->GetStreamingIndex(strLocalIndex(nScriptId));
	strStreamingInfo *pStreamingInfo = strStreamingEngine::GetInfo().GetStreamingInfo(nStreamingIndex);

	return (u8)strPackfileManager::GetImageFileIndexFromHandle(pStreamingInfo->GetHandle()).Get();
}

//
// name:		CStreamedScripts::ValidateAllScriptsInASingleImg
// description:	Load the named startup script (the one containing GLOBALS TRUE) and all other .sco files
//				that belong to the same script img file. Loading a script leads to its native commands being validated.
void CStreamedScripts::ValidateAllScriptsInASingleImg(const strStreamingObjectNameString pNameOfStartupScript)
{
	strLocalIndex startupScriptId = FindSlot(pNameOfStartupScript);

	if(startupScriptId != -1)
	{
		u8 nIndexOfScriptImg = GetIndexOfScriptImgContainingScoFile(startupScriptId.Get());
//	Get the name of the img file from the image index
		strStreamingFile *pStreamingFile = strPackfileManager::GetImageFile(nIndexOfScriptImg);
		if (pStreamingFile)
		{
			Displayf("Validating commands for %s\n", pStreamingFile->m_name.GetCStr());
		}

		//	Load the startup script so that it gets validated
		StreamingRequest(startupScriptId, 0);
		strStreamingEngine::GetLoader().LoadAllRequestedObjects();

		//	Go through all .sco files and load any whose image index matches nIndexOfScriptImg
		int nStoreSize = GetSize();
		for (int scoLoop = 0; scoLoop < nStoreSize; scoLoop++)
		{
			if ( (scoLoop != startupScriptId.Get()) && (IsValidSlot(strLocalIndex(scoLoop))) )	//	Skip startupScriptId as it has already been loaded
			{
				u8 nScriptImgIndexForCurrentSco = GetIndexOfScriptImgContainingScoFile(scoLoop);
				if (nScriptImgIndexForCurrentSco == nIndexOfScriptImg)
				{	//	This sco belongs to the same script.img as the startup script so load it to validate it
					StreamingRequest(strLocalIndex(scoLoop), 0);
					strStreamingEngine::GetLoader().LoadAllRequestedObjects();
				}
			}
		}

		RemoveAll();	//	Remove all the .sco files that have been loaded so that the number of global variables is reset
		if (pStreamingFile)
		{
			Displayf("Finished validating commands for %s\n", pStreamingFile->m_name.GetCStr());
		}
	}
}

//
// name:		CStreamedScripts::ValidateAllScripts
// description:	Loads all .sco files to cause them to be checked for unrecognised native commands by scrThread::Validate inside scrProgram::Load
void CStreamedScripts::ValidateAllScripts()
{
	if (PARAM_validatescripts.Get())
	{
		SmokeTests::DisplaySmokeTestStarted("validatescripts");
		sysTimer timeToValidate;
		//	The script code will complain if a startup script which contains GLOBALS TRUE is not loaded first.
		//	Also, we have to shutdown any single player scripts to wipe the global variables 
		//	before loading the network startup that contains a different number of global variables.
		ValidateAllScriptsInASingleImg(strStreamingObjectNameString("startup"));
		ValidateAllScriptsInASingleImg(strStreamingObjectNameString("network_startup"));
		float fTimeToValidateAllScripts = timeToValidate.GetMsTime();
		Displayf("Time taken to validate scripts = %.2f milliseconds \n", fTimeToValidateAllScripts);
		SmokeTests::DisplaySmokeTestFinished();
	}
}
#endif


//
// name:		CStreamedScripts::GetProgramId
// description:	Return program id for script
scrProgramId CStreamedScripts::GetProgramId(s32 index)
{
	fwAssetNameDef<scrProgram>* pDef = GetSlot(strLocalIndex(index));

	scriptAssertf(pDef, "No script at this slot");
	scriptAssertf(pDef->m_pObject, "%s:Script is not in memory", pDef->m_name.GetCStr());

	return pDef->m_pObject->GetProgramId();
}

const char* CStreamedScripts::GetScriptName(s32 index)
{
	fwAssetNameDef<scrProgram>* pDef = GetSlot(strLocalIndex(index));

	scriptAssertf(pDef, "No script at this slot");

	if (pDef)
	{
		return pDef->m_name.GetCStr();
	}
	else
	{
		return "-undefined-";
	}
}

#if __SCRIPT_MEM_CALC
void CStreamedScripts::CalculateMemoryUsage(u32& nVirtualSize, u32& nPhysicalSize, const bool display_info)
{
	nVirtualSize = 0;
	nPhysicalSize = 0;	
	u32 cnt = 0;
	for(s32 objIdx = 0; objIdx < GetCount(); ++objIdx)
	{
		u32 vSize = 0;
		u32 phSize = 0;		
		strLocalIndex lclIdx(objIdx);
		strStreamingInfo * info = GetStreamingInfo(lclIdx);
		strIndex index = GetStreamingIndex(lclIdx);
		if ( IsObjectInImage(lclIdx) )
		{
			bool bLoaded  = HasObjectLoaded(lclIdx);
			bool bRequested = IsObjectRequested(lclIdx) || IsObjectLoading(lclIdx);
			if( bLoaded || bRequested )
			{
				vSize = info->ComputeOccupiedVirtualSize(index, true);
				phSize = info->ComputePhysicalSize(index, true);
				nVirtualSize += vSize;
				nPhysicalSize += phSize;

				cnt++;
			}
		}
	}

	grcDebugDraw::AddDebugOutput("#Scripts (%d) : %dK %dK", cnt, nVirtualSize>>10, nPhysicalSize>>10);
	for(s32 objIdx = 0; objIdx < GetCount(); ++objIdx)
	{
		u32 vSize = 0;
		u32 phSize = 0;
		strLocalIndex lclIdx(objIdx);
		strStreamingInfo * info = GetStreamingInfo(lclIdx);
		strIndex index = GetStreamingIndex(lclIdx);
		if ( IsObjectInImage(lclIdx) )
		{
			bool bLoaded  = HasObjectLoaded(lclIdx);
			bool bRequested = IsObjectRequested(lclIdx) || IsObjectLoading(lclIdx);
			if( bLoaded || bRequested )
			{				
				vSize = info->ComputeOccupiedVirtualSize(index, true);
				phSize = info->ComputePhysicalSize(index, true);
				
				if(display_info)
				{
					const char *fileName   = strStreamingEngine::GetObjectName( index );
					grcDebugDraw::AddDebugOutput("Script %s: %dK %dK", 
						fileName,						
						vSize>>10, phSize>>10);
				}
			}
		}
	}
}
#endif