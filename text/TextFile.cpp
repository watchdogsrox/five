/////////////////////////////////////////////////////////////////////////////////
//
// FILE :      TextFile.cpp
// PURPOSE :   Deals with reading text from the text files
// AUTHOR :    KRH 13/05/98
// NICKED BY : Obbe. (Thanks Keith)
// NICKED FURTHER BY: Derek Payne - modifying all this 17/04/09
//
/////////////////////////////////////////////////////////////////////////////////

// NOTES - this is still work in progress - a lot of stuff still needs shifting around

// C headers
#include <stdio.h>

// Rage headers
#include "string\stringhash.h"
#include "system/criticalsection.h"

// Game headers
#include "Text\TextConversion.h"
#include "Text\TextFile.h"
#include "Text\Text.h"
#include "chunk.h"
#include "cutscene/cutscene_channel.h"
#include "cutscene/cutscenemanagernew.h"
#include "cutscene/CutSceneAssetManager.h"
#include "debug\Debug.h"
#include "frontend\pausemenu.h"
#include "SaveLoad/GenericGameStorage.h"
#include "scene/datafilemgr.h"
#include "script\script.h"
#include "system\filemgr.h"
#include "system/param.h"
#include "system/memory.h"
#include "system/system.h"
#include "fwsys/timer.h"
#include "text\messages.h"


RAGE_DEFINE_CHANNEL(text);
#undef __rage_channel
#define __rage_channel text

#ifdef _DEBUG
const s32 WrappedTextMaxWidth = 640;
#endif

TEXT_OPTIMISATIONS()
//OPTIMISATIONS_OFF()

XPARAM(langfilesuffix);

CTextFile	TheText;		// For all our text needs

// matches sysLanguage
static const char* g_LanguageFiles[] = {
	"american",			// LANGUAGE_ENGLISH=0,
	"french",			// LANGUAGE_FRENCH,
	"german",			// LANGUAGE_GERMAN,
	"italian",			// LANGUAGE_ITALIAN,
	"spanish",			// LANGUAGE_SPANISH,
	"portuguese",		// LANGUAGE_PORTUGUESE,
	"polish",			// LANGUAGE_POLISH,
	"russian",			// LANGUAGE_RUSSIAN,
	"korean",			// LANGUAGE_KOREAN,
	"chinese",			// LANGUAGE_CHINESE_TRADITIONAL,
	"japanese",			// LANGUAGE_JAPANESE,
	"mexican",			// LANGUAGE_MEXICAN,
	"chinesesimp"			// LANGUAGE_CHINESE_SIMPLIFIED
};

enum { TEXT_UNUSED, TEXT_REQUESTED, TEXT_LOADED };

char ErrorString[2][64];

sysCriticalSectionToken g_textFileAccessCS;

CTextFile::CTextFile()
{
	m_TextBlockContainingTheLastStringReturnedByGetFunction = -1;
	m_LanguageCode = 'e';	// default to English
	m_bUsesMultipleTextChunks = false;
	m_bTextIsLoaded = false;
	memset(&m_Text[0], 0, sizeof(m_Text));
}


CTextFile::~CTextFile()
{
	if (m_bTextIsLoaded)
		Unload();
	m_extraTextMetafiles.Reset();
}

char *CTextFile::Get(const char *textcode)
{
	if (!textcode)
	{
		if (CTheScripts::GetCurrentGtaScriptThread())
			Assertf(0, "%s:CTextFile::Get - text key is a NULL pointer", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		else
			Assertf(0, "CTextFile::Get - text key is a NULL pointer - current script thread is NULL");
	}

	if (!textcode || textcode[0] == 0 || textcode[0] == 32)
	{
		bool isRender = CSystem::IsThisThreadId(SYS_THREAD_RENDER);
		ErrorString[isRender][0] = 0;
		return ErrorString[isRender];
	}

	return Get(atStringHash(textcode), textcode);
}

struct SearchByHash 
{
	bool operator()( const CGxt2Entry& a, u32 b ) const
	{
		return a.Hash < b;
	}
};

char* CTextFile::SearchForText(u32 hashcode, CGxt2Header* textSlot)
{
	CGxt2Entry *start = textSlot->Entries, *end = textSlot->Entries + textSlot->Count;
	CGxt2Entry *r = std::lower_bound(start,end,hashcode,SearchByHash());
	// The end test almost isn't necessary because there's a dummy entry with a hash code of GXT2 in it
	// that is extremely unlikely to ever match anything :-)
	if (r != end && r->Hash == hashcode)
	{
		return (char*)textSlot + r->Offset;
	}
	return NULL;
}

// These should really be const char* or const char* but a lot of calling code needs to be repaired first.
char *CTextFile::GetInternal(u32 hashcode)
{
	sysCriticalSection criticalSection(g_textFileAccessCS);
	m_TextBlockContainingTheLastStringReturnedByGetFunction = -1;

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
	const char *pCloudText = CGenericGameStorage::SearchForSaveGameMigrationCloudText(hashcode);
	if (pCloudText)
	{
		return const_cast<char*>(pCloudText);
	}
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

	// Search for updated error text first
	char** updatedErrorText = m_updatedErrorText.Access(hashcode);
	if(updatedErrorText)
	{
		if (*updatedErrorText)
		{
			m_TextBlockContainingTheLastStringReturnedByGetFunction = GTA5_GLOBAL_TEXT_SLOT;
			return *updatedErrorText;
		}
	}

#if __BANK
	char *pGxtString = m_DebugStrings.GetString(hashcode);
	if (pGxtString)
	{
		return pGxtString;
	}
#endif	//	__BANK
	char** dlcText = m_extraGlobals.Access(hashcode);
	if(dlcText)
	{
		if (*dlcText)
		{
			m_TextBlockContainingTheLastStringReturnedByGetFunction = GTA5_GLOBAL_TEXT_SLOT;
			return *dlcText;
		}
	}
	// Search Global text slots first
	char* foundText = NULL;
	if (m_TextState[GTA5_GLOBAL_TEXT_SLOT]==TEXT_LOADED && m_Text[GTA5_GLOBAL_TEXT_SLOT])
	{
#if !__FINAL
		if (!sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_GAME_VIRTUAL)->GetSize(m_Text[GTA5_GLOBAL_TEXT_SLOT]))
			Quitf("CTextFile::m_Text is accessing invalid memory: %p (slot=%d)", m_Text[GTA5_GLOBAL_TEXT_SLOT], GTA5_GLOBAL_TEXT_SLOT);
#endif
		foundText = SearchForText(hashcode,m_Text[GTA5_GLOBAL_TEXT_SLOT]);
		if(foundText)
		{
			m_TextBlockContainingTheLastStringReturnedByGetFunction = GTA5_GLOBAL_TEXT_SLOT;
			return foundText;
		}
	}
	// Search all additional text slots 
	for (int i=0; i<GTA5_GLOBAL_TEXT_SLOT; i++)
	{
		if(m_PatchTextState[i] == TEXT_LOADED && m_PatchText[i])
		{
			foundText = SearchForText(hashcode,m_PatchText[i]);
			if(foundText)
			{
				m_TextBlockContainingTheLastStringReturnedByGetFunction = i;
				return foundText;
			}
		}
		// Explicitly check loaded state first in case the pointer is valid but it's still being streamed in.
		if (m_TextState[i]==TEXT_LOADED && m_Text[i])
		{
#if !__FINAL
			if (!sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_GAME_VIRTUAL)->GetSize(m_Text[i]))
				Quitf("CTextFile::m_Text is accessing invalid memory: %p (slot=%d)", m_Text[i], i);
#endif
			
			foundText = SearchForText(hashcode,m_Text[i]);
			if(foundText)
			{
				m_TextBlockContainingTheLastStringReturnedByGetFunction = i;
				return foundText;
			}			
		}
	}
	return NULL;
}

char *CTextFile::Get(u32 hashcode,const char *NOTFINAL_ONLY(textcode))
{
#if __BANK
	if (bShowTextIdOnly)
	{
		const char* pAttempt = textcode && textcode[0] != '\0' ? textcode : atHashString::TryGetString(hashcode);
		bool isRender = CSystem::IsThisThreadId(SYS_THREAD_RENDER);
		formatf(ErrorString[isRender],"%s",pAttempt);
		return ErrorString[isRender];
	}
#endif // __BANK

	char *result = GetInternal(hashcode);
	if (result)
		return result;

#if __ASSERT
	if (textcode != NULL && hashcode != ATSTRINGHASH("Streetname",0x2dad6942))
	{	//	Willie asked me to take this assert out for Streetnames but it's still useful for other text
		char fileName[RAGE_MAX_PATH];
		GetLanguageFile(fileName, sizeof(fileName), false, TEXT_SOURCE_DISK);
		rageDisplayf("CTextFile::Get - %s label missing from %s GXT file", textcode, fileName);
		rageAssertf(0, "CTextFile::Get - %s label missing from %s GXT file", textcode, fileName);
	}
#endif // __ASSERT

	bool isRender = CSystem::IsThisThreadId(SYS_THREAD_RENDER);
#if __FINAL
	ErrorString[isRender][0] = 0;
#else	//	__FINAL
	formatf(ErrorString[isRender],"%s missing",textcode);
#endif	//	__FINAL
	return ErrorString[isRender];
}

bool CTextFile::DoesTextLabelExist(const char *Ident) 
{ 
	return GetInternal(atStringHash(Ident)) != 0;
}

bool CTextFile::DoesTextLabelExist(u32 hashcode) 
{ 
	return GetInternal(hashcode) != 0;
}

bool CTextFile::IsDLCDevice(const char* deviceName) const
{
	return (strnicmp(deviceName, "dlc", 3) == 0);
}

const char* CTextFile::GetLanguageFile(char *buffer, size_t bufferSize, bool bUseSystemLanguage, eSource eTextSource, bool isDLCpatch)
{
	u32 iLanguage;
	const char *suffix = "";

	PARAM_langfilesuffix.Get(suffix);

#if __FINAL
	suffix = "_rel";
#endif

	if (bUseSystemLanguage)
	{
		iLanguage = CPauseMenu::GetLanguageFromSystemLanguage();
		// If system language isnt available use English
		if(iLanguage == LANGUAGE_UNDEFINED)
			iLanguage = LANGUAGE_ENGLISH;
	}
	else
	{
		iLanguage = CPauseMenu::GetMenuPreference(PREF_CURRENT_LANGUAGE);
	}

	switch(eTextSource)
	{
		case TEXT_SOURCE_DISK:
			formatf(buffer, bufferSize, "platform:/data/lang/%s%s.rpf", g_LanguageFiles[iLanguage], suffix );
			break;
		case TEXT_SOURCE_TU:
			formatf(buffer, bufferSize, "update:/" RSG_PLATFORM_ID "/data/lang/%s%s.rpf", g_LanguageFiles[iLanguage], suffix );
			break;
		case TEXT_SOURCE_TU2:
			formatf( buffer, bufferSize, "update2:/" RSG_PLATFORM_ID "/data/lang/%s%s.rpf", g_LanguageFiles[iLanguage], suffix );
			break;
		case TEXT_SOURCE_PATCH:
			formatf(buffer, bufferSize, "/patch/data/lang/%s%s.rpf", g_LanguageFiles[iLanguage], (isDLCpatch ? "" : suffix) );
			break;
		case TEXT_SOURCE_DLC:
			formatf(buffer, bufferSize, "/data/lang/%sdlc.rpf", g_LanguageFiles[iLanguage] );
			break;
	}

	return buffer;
}


void CTextFile::AppendToExtraGlobals(const char* deviceName, bool remove, bool titleUpdate)
{
	fiPackfile* packFile = rage_aligned_new(16) fiPackfile;
	char fileName[RAGE_MAX_PATH];

	GetLanguageFile(fileName,sizeof(fileName),false,titleUpdate?TEXT_SOURCE_PATCH:TEXT_SOURCE_DLC, IsDLCDevice(deviceName));

	char finalTarget[RAGE_MAX_PATH];
	formatf(finalTarget,sizeof(finalTarget),"%s:/%%PLATFORM%%%s",deviceName,fileName);

	DATAFILEMGR.ExpandFilename(finalTarget,fileName,sizeof(fileName));

	if (!Verifyf(packFile->Init(fileName,true,fiPackfile::CACHE_NONE),"Extra content localization pack file missing: %s",fileName))
	{
		delete packFile;
		packFile = NULL;
		return;
	}
	packFile->MountAs("dlcTempText:/");
	safecpy(fileName,"dlcTempText:/global.gxt2");

	CGxt2Header* text;
	u32 fileSize;
	pgStreamer::Handle h = pgStreamer::Open(fileName,&fileSize,0);
	if (h == pgStreamer::Error)
	{
		Errorf("CTextFile - Cannot find %s",fileName);
		delete packFile;
		packFile = NULL;
		return;
	}
	text = (CGxt2Header*) rage_aligned_new(16) char[fileSize];

	datResourceChunk destList = { 0, text, fileSize };
	Displayf("Loading DLC Globals from device : %s", deviceName);
	sysIpcSema s = sysIpcCreateSema(false);
	while (pgStreamer::Read(h,&destList,1,0,s,0) == NULL)
		sysIpcSleep(10);
	sysIpcWaitSema(s);
	sysIpcDeleteSema(s);
	Displayf("Finished loading DLC Globals");
	CGxt2Entry *start = text->Entries, *end = text->Entries + text->Count;
	for(CGxt2Entry* entry = start, *nextEntry = start+1; entry!= end;entry++,nextEntry++)
	{
		bool alreadyExists = m_extraGlobals.Has(entry->Hash);
		if(!remove&&!alreadyExists)
		{	
			u32 entryLen = nextEntry->Offset-entry->Offset;
			char* temp = rage_aligned_new(16) char[entryLen];
			safecpy(temp,(char*)text+entry->Offset, entryLen);
			m_extraGlobals.Insert(entry->Hash, temp);
		}
		else if(remove && alreadyExists)
		{
			int idx = 0;
			// Need to do a linear search because m_extraGlobals may not be sorted, so SafeGet, Access, etc. won't work
			for(atBinaryMap<char*, u32>::Iterator iter = m_extraGlobals.Begin(); iter != m_extraGlobals.End(); ++iter)
			{
				if (iter.GetKey() == entry->Hash)
				{
					delete *iter;
					m_extraGlobals.Remove(idx);
					break;
				}
				idx++;
			}
		}
	}
	m_extraGlobals.FinishInsertion();
	delete[] (char*)text;
	fiDevice::Unmount(*packFile);
	delete packFile;
}

void CTextFile::SetUpdatedErrorText(CGxt2Header* text, bool remove)
{
	if(text == NULL && remove)
	{
		for(atBinaryMap<char*, u32>::Iterator it=m_updatedErrorText.Begin(); it != m_updatedErrorText.End();++it)
		{
			char*& text = *it;
			delete[] text;
			text = NULL;
		}
		m_updatedErrorText.Reset();

		return;
	}

	CGxt2Entry *start = text->Entries, *end = text->Entries + text->Count;
	for(CGxt2Entry* entry = start, *nextEntry = start+1; entry!= end;entry++,nextEntry++)
	{
		bool alreadyExists = m_updatedErrorText.Has(entry->Hash);
		if(!remove&&!alreadyExists)
		{	
			u32 entryLen = nextEntry->Offset-entry->Offset;
			char* temp = rage_aligned_new(16) char[entryLen];
			safecpy(temp,(char*)text+entry->Offset, entryLen);
			m_updatedErrorText.Insert(entry->Hash, temp);
		}
		else if(remove && alreadyExists)
		{
			int idx = 0;
			// Need to do a linear search because m_updatedErrorText may not be sorted, so SafeGet, Access, etc. won't work
			for(atBinaryMap<char*, u32>::Iterator iter = m_updatedErrorText.Begin(); iter != m_updatedErrorText.End(); ++iter)
			{
				if (iter.GetKey() == entry->Hash)
				{
					delete *iter;
					m_updatedErrorText.Remove(idx);
					break;
				}
				idx++;
			}
		}
	}
}

void CTextFile::RemoveExtracontentTextFromMetafile(const char* fileName)
{
	atHashString fileNameHash = atHashString(fileName);
	if(m_extraTextMetafiles.Access(fileNameHash))
	{
		sysCriticalSection criticalSection(g_textFileAccessCS);
		atHashString deviceNameHash = atHashString(m_extraTextMetafiles[fileNameHash]->m_deviceName);
		if(m_extraText.Access(deviceNameHash))
		{
			//Don't remove extra globals
			//AppendToExtraGlobals(m_extraTextMetafiles[fileNameHash]->m_deviceName,true,m_extraTextMetafiles[fileNameHash]->m_isTitleUpdate);
			m_extraText[deviceNameHash]->Shutdown();
			m_extraText.Delete(deviceNameHash);
		}
		m_extraTextMetafiles.Delete(fileNameHash);
	}
}

void CTextFile::AddExtracontentTextFromMetafile(const char* fileName)
{
	atHashString fileNameHash = atHashString(fileName);
	if(!m_extraTextMetafiles.Access(fileNameHash))
	{
		sysCriticalSection criticalSection(g_textFileAccessCS);
		CExtraTextMetaFile* metaFile = rage_new CExtraTextMetaFile;
		if(PARSER.LoadObject(fileName,NULL,*metaFile))
		{
			metaFile->m_deviceName.Set(fileName,RAGE_MAX_PATH,0,static_cast<int>(strcspn(fileName,":")));
			AddExtracontentText(metaFile->m_deviceName,metaFile->m_hasAdditionalText,metaFile->m_hasGlobalTextFile, metaFile->m_isTitleUpdate);
			m_extraTextMetafiles.Insert(fileNameHash,metaFile);
		}
		else
		{
			delete metaFile;
		}
	}
}

void CTextFile::AddExtracontentText(const char* deviceName, bool hasAdditionalTextSlots, bool hasGlobalTextSlot, bool isTitleUpdate)
{
	if(!m_extraText.Access(atHashString(deviceName)))
	{
		CExtraTextFile* extraText = rage_aligned_new(16) CExtraTextFile(hasAdditionalTextSlots,hasGlobalTextSlot, isTitleUpdate);
		extraText->Init(deviceName);
		m_extraText.Insert(atHashString(deviceName),extraText);
	}
}
void CTextFile::RemoveExtracontentText(const char* deviceName)
{
	atHashString deviceNameHash = atHashString(deviceName);
	CExtraTextFile** textFile = m_extraText.Access(deviceNameHash);
	if(textFile)
	{
		//Don't remove globals
// 		if((*textFile)->GetHasGlobalText())
// 			AppendToExtraGlobals(deviceName,true,(*textFile)->GetIsTitleUpdate());
		(*textFile)->Shutdown();
		m_extraText.Delete(deviceNameHash);
	}
}

CExtraTextFile::CExtraTextFile(bool hasAdditionalTextSlots, bool hasGlobalTextSlot, bool isTitleUpdate)
	:m_bIsLoaded(false),m_bHasAdditionalTextSlots(hasAdditionalTextSlots), m_bHasGlobalTextSlot(hasGlobalTextSlot),m_bIsTitleUpdate(isTitleUpdate),m_PackFile(NULL)
{
}
CExtraTextFile::~CExtraTextFile()
{
	Shutdown();
}
void CExtraTextFile::Shutdown()
{
	if(m_bHasAdditionalTextSlots)
	{		
		if(m_PackFile)
		{
			fiDevice::Unmount(*m_PackFile);
			delete (m_PackFile);
			m_PackFile = NULL;			
		}
	}
	m_bIsLoaded = false;
}
void CExtraTextFile::Init(const char* deviceName)
{
	if(m_bHasGlobalTextSlot)
	{
		TheText.AppendToExtraGlobals(deviceName,false,m_bIsTitleUpdate);
	}
	if(m_bHasAdditionalTextSlots)
	{
		m_bIsLoaded = false;
		m_PackFile = rage_aligned_new(16) fiPackfile;
		char fileName[RAGE_MAX_PATH];
		TheText.GetLanguageFile(fileName,sizeof(fileName),false,m_bIsTitleUpdate?CTextFile::TEXT_SOURCE_PATCH:CTextFile::TEXT_SOURCE_DLC, TheText.IsDLCDevice(deviceName));
		char finalTarget[RAGE_MAX_PATH];
		formatf(finalTarget,sizeof(finalTarget),"%s:/%%PLATFORM%%%s",deviceName,fileName);			
		DATAFILEMGR.ExpandFilename(finalTarget,fileName,sizeof(fileName));
		if (!Verifyf(m_PackFile->Init(fileName,true,fiPackfile::CACHE_NONE),"Extra content localization pack file missing: %s",fileName))
		{
			delete m_PackFile;
			m_PackFile = NULL;
			return;
		}
		formatf(m_mountName,sizeof(m_mountName),"dlcTXT%u:/",atHashString(deviceName).GetHash());
		m_PackFile->MountAs(m_mountName);
	}
	m_bIsLoaded = true;	
}

int CTextFile::GetLanguageFromName(const char* pLanguage)
{
	for(int i=0; i<MAX_LANGUAGES; i++)
	{
		if(!strcmp(pLanguage, g_LanguageFiles[i]))
			return i;
	}
	return LANGUAGE_UNDEFINED;
}

void CTextFile::Load(bool bUseSystemLanguage)
{
	USE_MEMBUCKET(MEMBUCKET_SCRIPT);
	Unload();

#if __BANK
	bShowTextIdOnly = false;
#endif // __BANK

	m_Packfile = rage_aligned_new(16) fiPackfile;
	char fileName[RAGE_MAX_PATH];
	GetLanguageFile(fileName, sizeof(fileName), bUseSystemLanguage, TEXT_SOURCE_TU2);		// text packfile can't mount from update2: without explicit path, so check the tu2 first...
	if(!ASSET.Exists(fileName,""))
	{
		GetLanguageFile(fileName, sizeof(fileName), bUseSystemLanguage, TEXT_SOURCE_TU);		// text packfile can't mount from update: without explicit path, so check the tu first...
		if(!ASSET.Exists(fileName,""))
		{
			GetLanguageFile(fileName, sizeof(fileName), bUseSystemLanguage, TEXT_SOURCE_DISK);	// ...if no replacement, load the original
		}
	}
	Debugf2(2,"Language file %s",fileName);
	if (!m_Packfile->Init(fileName,true,fiPackfile::CACHE_NONE))
	{
		delete m_Packfile;
		m_Packfile = NULL;
		return;
	}
	m_Packfile->MountAs("language:/");	

	m_bTextIsLoaded = true;
	LoadTextIntoSlot(GTA5_GLOBAL_TEXT_SLOT, "GLOBAL", true, TEXT_LOCATION_GTA5_WITH_POSSIBLE_PATCH);
	for(atMap<atHashString, CExtraTextMetaFile*>::Iterator it=m_extraTextMetafiles.CreateIterator(); !it.AtEnd();it.Next())
	{
		CExtraTextMetaFile*& text = it.GetData();
		AddExtracontentText(text->m_deviceName,text->m_hasAdditionalText,text->m_hasGlobalTextFile,text->m_isTitleUpdate);
	}
}

void CTextFile::Unload(bool unloadExtra /* = false */)
{
	// Make sure that any messages being displayed are removed.
	CMessages::ClearAllMessagesDisplayedByGame();	
	

	if(!unloadExtra)
	{
		m_bTextIsLoaded = false;
		for (int i=0; i<NUM_TEXT_SLOTS; i++)
		{
			if (m_Text[i])
			{
				delete[] (char*) m_Text[i];
				m_Text[i] = NULL;
			}
			if (m_PatchText[i])
			{
				delete[] (char*) m_PatchText[i];
				m_PatchText[i] = NULL;
			}
		}
		if(m_Packfile)
		{
			fiDevice::Unmount(*m_Packfile);
			delete (m_Packfile);
			m_Packfile = NULL;	
		}
		memset(&m_TextState[0], TEXT_UNUSED, sizeof(m_TextState));
		memset(&m_PatchTextState[0], TEXT_UNUSED, sizeof(m_PatchTextState));
	}
	else
	{
		for(int i=DLC_TEXT_SLOT0; i<=DLC_TEXT_SLOT2;i++)
		{
			if(m_Text[i])
			{
				delete[] (char*) m_Text[i];
				delete[] (char*) m_PatchText[i];
				m_Text[i] = NULL;
				m_PatchText[i] = NULL;
				m_TextState[i] = TEXT_UNUSED;
				m_PatchTextState[i] = TEXT_UNUSED;
			}
		}
	}
	for(atMap<atHashString, CExtraTextFile*>::Iterator it=m_extraText.CreateIterator(); !it.AtEnd();it.Next())
	{
		CExtraTextFile*& text = it.GetData();
		text->Shutdown();	
	}
	for(atBinaryMap<char*, u32>::Iterator it=m_extraGlobals.Begin(); it != m_extraGlobals.End();++it)
	{
		char*& text = *it;
		delete[] text;
		text = NULL;
	}
	for(atBinaryMap<char*, u32>::Iterator it=m_updatedErrorText.Begin(); it != m_updatedErrorText.End();++it)
	{
		char*& text = *it;
		delete[] text;
		text = NULL;
	}
	m_extraText.Reset();
	m_extraGlobals.Reset();
	m_updatedErrorText.Reset();
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CTextFile::ReloadAfterLanguageChange()
// PURPOSE:	deals with reloading text after a language change and reloads the slots
//			that may be loaded in aswell
/////////////////////////////////////////////////////////////////////////////////////
void CTextFile::ReloadAfterLanguageChange()
{
	s32 slot_loop;

	// check and store locally what text is currently loading into what slots:
	char textBlockLoaded[GTA5_GLOBAL_TEXT_SLOT][MISSION_NAME_LENGTH];
	u8 textStatus[GTA5_GLOBAL_TEXT_SLOT];

	for (slot_loop = 0; slot_loop < GTA5_GLOBAL_TEXT_SLOT; slot_loop++)
	{
		strncpy(textBlockLoaded[slot_loop], m_TextBlockLoadedIntoThisSlot[slot_loop], MISSION_NAME_LENGTH);
		textStatus[slot_loop] = m_TextState[slot_loop];
	}

	// we are now safe to delete the text:
	Load();  // reload the main text (this also unloads any current text slots, so now they are gone)

	// because Load() deletes the text slots, we need to reload them again:
	for (slot_loop = 0; slot_loop < GTA5_GLOBAL_TEXT_SLOT; slot_loop++)
	{
		// re-load the additional text slot:
		if (textStatus[slot_loop] == TEXT_LOADED)
		{
			LoadTextIntoSlot(slot_loop,textBlockLoaded[slot_loop], true, slot_loop>=DLC_TEXT_SLOT0?TEXT_LOCATION_ADDED_FOR_DLC:TEXT_LOCATION_GTA5_WITH_POSSIBLE_PATCH);
		}
		// re-request the additional text slot:
		if (textStatus[slot_loop] == TEXT_REQUESTED)
		{		
			LoadTextIntoSlot(slot_loop,textBlockLoaded[slot_loop], false, slot_loop>=DLC_TEXT_SLOT0?TEXT_LOCATION_ADDED_FOR_DLC:TEXT_LOCATION_GTA5_WITH_POSSIBLE_PATCH);
		}
	}
	// all text should now be re-loaded (or re-requested) at this point
}

void CTextFile::RequestAdditionalText(const char *pMissionTextStem, s32 SlotNumber, eLocationOfTextBlock textLocation)
{
	Assert(SlotNumber >= 0 && SlotNumber < GTA5_GLOBAL_TEXT_SLOT);
	if(textLocation != TEXT_LOCATION_ADDED_FOR_DLC)
		LoadTextIntoSlot(SlotNumber,pMissionTextStem,false,textLocation);
	else
	{
		Assertf( (SlotNumber>=DLC_TEXT_SLOT0&&SlotNumber<=DLC_AMBIENT_DIALOGUE_TEXT_SLOT) || (SlotNumber==DLC_MISSION_DIALOGUE_TEXT_SLOT2),"Trying to load text in GTA5 slots for DLC");
		LoadTextIntoSlot(SlotNumber, pMissionTextStem, false, textLocation);
	}
}

void CTextFile::LoadAdditionalText(const char *pMissionTextStem, s32 SlotNumber, eLocationOfTextBlock textLocation)
{
	Assert(SlotNumber >= 0 && SlotNumber < GTA5_GLOBAL_TEXT_SLOT);
	if(textLocation != TEXT_LOCATION_ADDED_FOR_DLC)
		LoadTextIntoSlot(SlotNumber,pMissionTextStem,true, textLocation);
	else 
	{
		Assertf(SlotNumber>=DLC_TEXT_SLOT0&&SlotNumber<=DLC_TEXT_SLOT2,"Trying to load text in GTA5 slots for DLC");
		LoadTextIntoSlot(SlotNumber, pMissionTextStem, true, textLocation);
	}
}

static void setStateLoaded(void *userArg, void *,u32 ,u32 )
{
	*(u8*)userArg = TEXT_LOADED;
	Displayf("REQUEST_ADDITIONAL_TEXT - finished load");	//	It would be good to print the slot number and stem here
}


void CTextFile::FreeTextSlot(s32 SlotNumber, bool /*extraText*/)
{
	USE_MEMBUCKET(MEMBUCKET_SCRIPT);

	Assertf(m_TextState[SlotNumber] == TEXT_LOADED, "ERROR: CTextFile - Trying to free an unloaded text slot");

	if (SlotNumber < GTA5_GLOBAL_TEXT_SLOT)	// The old text system didn't attempt to clear global text so don't do that here either (global text is in slot 0)
	{
		CMessages::ClearAllDisplayedMessagesThatBelongToThisTextBlock(SlotNumber, true);
	}

	if (m_Text[SlotNumber]) 
	{
		delete [] (char*) m_Text[SlotNumber];
		m_Text[SlotNumber] = NULL;
	}
	if (m_PatchText[SlotNumber]) 
	{
		delete [] (char*) m_PatchText[SlotNumber];
		m_PatchText[SlotNumber] = NULL;
	}
	m_TextState[SlotNumber] = TEXT_UNUSED;
	m_PatchTextState[SlotNumber] = TEXT_UNUSED;
	m_TextBlockLoadedIntoThisSlot[SlotNumber][0] = 0;
}


void CTextFile::LoadTextIntoSlot(s32 slot, const char *stem, bool blocking, eLocationOfTextBlock textLocation)
{
	char name[64];
	char patchName[64];

	bool bFoundInMainText=false;

	if(textLocation != TEXT_LOCATION_ADDED_FOR_DLC)
	{	//	Search the main GTA5 text
#if __ASSERT	
		Assert(m_Packfile);
#endif // __ASSERT
		formatf(name,"language:/%s.gxt2",stem);
		if(ASSET.Exists(name,"")) 
		{	//	TEXT_LOCATION_CHECK_BOTH_GTA5_AND_DLC will make use of bFoundInMainText later
			bFoundInMainText = true;
		}
		else
		{
			if (textLocation == TEXT_LOCATION_GTA5_WITH_POSSIBLE_PATCH)
			{
				Assertf(false, "GXT2 File missing: %s", name);
				return;
			}
		}
	}
	bool foundInDLC=false;
	
	for(atMap<atHashString,CExtraTextFile*>::Iterator it=m_extraText.CreateIterator(); !it.AtEnd();it.Next())
	{
		CExtraTextFile*& textFile = it.GetData();
		if(textFile->GetHasAdditionalText()&&textFile->GetPackFile())
		{
#if __ASSERT	
			Assert(textFile->GetPackFile());
#endif // __ASSERT

			const char *pNameToCheck = NULL;
			switch (textLocation)
			{
				case TEXT_LOCATION_GTA5_WITH_POSSIBLE_PATCH :
					Assertf(bFoundInMainText, "CTextFile::LoadTextIntoSlot - Just a safety check. We should have returned before now if the GXT2 File %s was missing from the main GTA5 text", stem);
					formatf(patchName,"%s%s.gxt2", textFile->GetMountName(),stem);
					pNameToCheck = patchName;
					break;

				case TEXT_LOCATION_ADDED_FOR_DLC :
					Assertf(!bFoundInMainText, "CTextFile::LoadTextIntoSlot - Just a safety check. The GXT2 File %s shouldn't have been found in the main GTA5 text", stem);
					formatf(name,"%s%s.gxt2", textFile->GetMountName(),stem);
					pNameToCheck = name;
					break;

				case TEXT_LOCATION_CHECK_BOTH_GTA5_AND_DLC :
					if (bFoundInMainText)
					{
						formatf(patchName,"%s%s.gxt2", textFile->GetMountName(),stem);
						pNameToCheck = patchName;
					}
					else
					{
						formatf(name,"%s%s.gxt2", textFile->GetMountName(),stem);
						pNameToCheck = name;
					}
					break;
			}

			if(ASSET.Exists(pNameToCheck,""))
			{
				foundInDLC = true;
				break;
			}
		}
	}

	if(!foundInDLC)
	{
		switch (textLocation)
		{
		case TEXT_LOCATION_GTA5_WITH_POSSIBLE_PATCH :
			//	We've already found the block in the GTA5 text so it doesn't matter if we didn't find the block name in DLC
			Assertf(bFoundInMainText, "CTextFile::LoadTextIntoSlot - Just a safety check. We should have returned before now if the GXT2 File %s was missing from the main GTA5 text", name);
			break;

		case TEXT_LOCATION_ADDED_FOR_DLC :
			//	Return if we expected to find the text in DLC and we didn't find it
			return;
//			break;

		case TEXT_LOCATION_CHECK_BOTH_GTA5_AND_DLC :
			if (!bFoundInMainText)
			{
				Assertf(0, "CTextFile::LoadTextIntoSlot - The GXT2 File %s was not found in the main GTA5 text or in any DLC text", name);
				return;
			}
			break;
		}
	}
	
#if __ASSERT
	if (CutSceneManager::GetInstancePtr())
	{
		CCutSceneAssetMgrEntity* pCutAssetMgr = CutSceneManager::GetInstancePtr()->GetAssetManager();
		if (pCutAssetMgr)
		{
			cutsceneAssertf((slot)!=MISSION_DIALOGUE_TEXT_SLOT || !pCutAssetMgr->IsLoadingSubtitles() || pCutAssetMgr->IsLoadingAdditionalText(stem),"Requesting new additional dialogue text '%s' whilst the old text '%s' is still in use by the cutscene!", stem, pCutAssetMgr->GetAdditionalTextBlockName());
		}
	}
#endif // __ASSERT

	if (IsRequestingAdditionalText(slot))
	{
		if (stricmp((m_TextBlockLoadedIntoThisSlot[slot]), stem))
		{
			Assertf(0, "REQUEST_ADDITIONAL_TEXT - Attempting to request %s for slot %d when %s has already been requested", stem, slot, m_TextBlockLoadedIntoThisSlot[slot]);
		}
		else
		{
			Displayf("REQUEST_ADDITIONAL_TEXT - %s is already requested into slot %d", stem, slot);
		}
		return;
	}

	if (HasThisAdditionalTextLoaded(stem,slot))
	{
		Displayf("REQUEST_ADDITIONAL_TEXT - %s is already loaded into slot %d", stem, slot);
		return;
	}

	USE_MEMBUCKET(MEMBUCKET_SCRIPT);
	

	if (slot < GTA5_GLOBAL_TEXT_SLOT)	// The old text system didn't attempt to clear global text so don't do that here either (global text is in slot 0)
	{	// Pass in the slot number so that only those messages and previous messages from the old slot are cleared
		CMessages::ClearAllDisplayedMessagesThatBelongToThisTextBlock(slot, true);
	}
	m_TextState[slot] = TEXT_REQUESTED;
	if(bFoundInMainText&&foundInDLC)
		m_PatchTextState[slot] = TEXT_REQUESTED;
	else
		m_PatchTextState[slot] = TEXT_UNUSED;

	if (m_Text[slot]) 
	{
		delete [] (char*) m_Text[slot];
		m_Text[slot] = NULL;
	}
	if (m_PatchText[slot]) 
	{
		delete [] (char*) m_PatchText[slot];
		m_PatchText[slot] = NULL;
	}
	m_TextBlockLoadedIntoThisSlot[slot][0] = 0;
	StreamIntoSlots(name,blocking,m_Text[slot],m_TextState[slot], m_TextBlockLoadedIntoThisSlot[slot],stem BANK_ONLY(,slot));
	if(bFoundInMainText&&foundInDLC)
	{
		StreamIntoSlots(patchName,blocking,m_PatchText[slot],m_PatchTextState[slot],m_TextBlockLoadedIntoThisSlot[slot],stem BANK_ONLY(,slot));
	}
}


void CTextFile::StreamIntoSlots(const char* name, bool blocking,CGxt2Header *& _textSlot, u8 &_TextState,char* _TextBlockLoadedIntoThisSlot,const char* stem BANK_ONLY(, s32 slot))
{
	u32 fileSize;
	pgStreamer::Handle h = pgStreamer::Open(name,&fileSize,0);
	if (h == pgStreamer::Error)
	{
		Errorf("CTextFile - Cannot find %s",name);
		return;
	}

	safecpy(_TextBlockLoadedIntoThisSlot, stem, MISSION_NAME_LENGTH);

	_textSlot = (CGxt2Header*) rage_aligned_new(16) char[fileSize];

	datResourceChunk destList = { 0, _textSlot, fileSize };
	if (blocking)
	{
		BANK_ONLY(Displayf("LOAD_ADDITIONAL_TEXT - starting blocking load of %s into slot %d", stem, slot));
		sysIpcSema s = sysIpcCreateSema(false);
		while (pgStreamer::Read(h,&destList,1,0,s,0) == NULL)
			sysIpcSleep(10);
		sysIpcWaitSema(s);
		sysIpcDeleteSema(s);
		_TextState = TEXT_LOADED;
		BANK_ONLY(Displayf("LOAD_ADDITIONAL_TEXT - finished blocking load of %s into slot %d", stem, slot));
	}
	else
	{
		BANK_ONLY(Displayf("REQUEST_ADDITIONAL_TEXT - starting load of %s into slot %d", stem, slot));
		while (pgStreamer::Read(h,&destList,1,0,setStateLoaded,&_TextState,pgStreamer::HARDDRIVE) == NULL)
			sysIpcSleep(10);
	}
}

s32 CTextFile::GetTextIndexInternal(const char* pMissionTextStem, fiPackfile* packFile)
{
	char buf[64];
	safecpy(buf,pMissionTextStem);
	safecat(buf,".gxt2");
	const fiPackEntry *i = packFile->FindEntry(buf), *begin = packFile->GetEntries();
	if(i)
	{
		return ptrdiff_t_to_int(i-begin);
	}
	else
		return -1;
}

s32 CTextFile::GetAdditionalTextIndex(const char* pMissionTextStem)
{
	if (!m_Packfile)
		return -1;
	fiPackfile* curPackFile = m_Packfile;
	s32 curIdx = GetTextIndexInternal(pMissionTextStem,curPackFile);
	if(curIdx != -1)
	{
		return curIdx;
	}
	else
	{
		for(atMap<atHashString,CExtraTextFile*>::Iterator it=m_extraText.CreateIterator(); !it.AtEnd();it.Next())
		{
			CExtraTextFile*& textFile = it.GetData();
			if(textFile->GetHasAdditionalText()&&textFile->GetPackFile())
			{
				curPackFile = textFile->GetPackFile();
#if __ASSERT	
				Assert(curPackFile);
#endif // __ASSERT
				curIdx = GetTextIndexInternal(pMissionTextStem,curPackFile);
				if(curIdx != -1)
				{
					return curIdx;
				}
			}
		}
	}
	return -1;
}

bool CTextFile::HasThisAdditionalTextLoaded(const char *pMissionTextStem, s32 SlotNumber)
{
	Assert(SlotNumber >= 0 && SlotNumber < NUM_TEXT_SLOTS);
	bool hasTextLoaded = ((m_TextState[SlotNumber] == TEXT_LOADED) && ((m_PatchTextState[SlotNumber] == TEXT_LOADED)||(m_PatchTextState[SlotNumber]== TEXT_UNUSED)));
	return (hasTextLoaded && !stricmp(pMissionTextStem,m_TextBlockLoadedIntoThisSlot[SlotNumber]));
}

bool CTextFile::HasAdditionalTextLoaded(s32 SlotNumber)
{
	Assert(SlotNumber >= 0 && SlotNumber < NUM_TEXT_SLOTS);
	bool hasTextLoaded = ((m_TextState[SlotNumber] == TEXT_LOADED) &&((m_PatchTextState[SlotNumber] == TEXT_LOADED) || (m_PatchTextState[SlotNumber] == TEXT_UNUSED)));
	return hasTextLoaded;
}

bool CTextFile::IsRequestingAdditionalText(int SlotNumber)
{
	Assert(SlotNumber >= 0 && SlotNumber < NUM_TEXT_SLOTS);
	bool hasTextRequested = (m_TextState[SlotNumber] == TEXT_REQUESTED) || (m_PatchTextState[SlotNumber] == TEXT_REQUESTED);
	return hasTextRequested;
}

bool CTextFile::IsRequestingThisAdditionalText(const char *pMissionTextStem, s32 SlotNumber)
{
	Assert(SlotNumber >= 0 && SlotNumber < NUM_TEXT_SLOTS);
	bool hasTextRequested = (m_TextState[SlotNumber] == TEXT_REQUESTED) || (m_PatchTextState[SlotNumber] == TEXT_REQUESTED);
	return (hasTextRequested && !stricmp(pMissionTextStem,m_TextBlockLoadedIntoThisSlot[SlotNumber]));
}

const char* CTextFile::GetTextStemForAdditonalSlot(const int idx) const
{
	return Verifyf(idx < NUM_TEXT_SLOTS && idx >= 0, "Trying to find the text stem of a TextSlot that is out of bounds idx=%i, min=%i, max=%i.", idx, CREDITS_TEXT_SLOT, NUM_TEXT_SLOTS) ? m_TextBlockLoadedIntoThisSlot[idx] : "";
}

void CTextFile::ProcessStreamingAdditionalText()
{
}

void CTextFileWrapper::Load(unsigned)
{
    TheText.Load();
}

#if __BANK
void CDebugString::Set(const char* pText)
{
	if(m_string)
		delete[] m_string;

	s32 string_length = istrlen(pText);
	m_string = rage_aligned_new(16) char[string_length+1];	//	+1 for the NULL terminator
	m_string[0] = 0;
	
	safecpy( m_string, pText, string_length+1 );

	m_bFlaggedForDeletion = false;
}

sysCriticalSectionToken		g_DebugStringCriticalToken;


void CDebugStrings::Add(const char *pTextKey, const char *pTextToDisplay)
{
	sysCriticalSection criticalSection(g_DebugStringCriticalToken);

	u32 HashKey = atStringHash(pTextKey);
	if (Verifyf(!CSystem::IsThisThreadId(SYS_THREAD_RENDER), "CDebugStrings::Add - didn't expect this to be called by the render thread"))
	{
		m_DebugStringMap[HashKey].Set(pTextToDisplay);
	}
}

void CDebugStrings::FlagForDeletion(const char *pTextKey)
{
	if (Verifyf(!CSystem::IsThisThreadId(SYS_THREAD_RENDER), "CDebugStrings::FlagForDeletion - didn't expect this to be called by the render thread"))
	{
		CDebugString *pDebugString = m_DebugStringMap.Access(atStringHash(pTextKey));
		if (pDebugString)
		{
			return pDebugString->SetFlaggedForDeletion(true);
		}
	}
}

void CDebugStrings::FlagAllForDeletion()
{
	if (Verifyf(!CSystem::IsThisThreadId(SYS_THREAD_RENDER), "CTextFile::ClearDebugStringMap - didn't expect this to be called by the render thread"))
	{
		atMap<u32, CDebugString>::Iterator MapIterator = m_DebugStringMap.CreateIterator();
		while (!MapIterator.AtEnd())
		{
			MapIterator.GetData().SetFlaggedForDeletion(true);

			MapIterator.Next();
		}
	}
}

void CDebugStrings::DeleteAllFlagged()
{
	atMap<u32, CDebugString>::Iterator MapIterator = m_DebugStringMap.CreateIterator();
	while (!MapIterator.AtEnd())
	{
		if (MapIterator.GetData().GetFlaggedForDeletion())
		{
			u32 KeyOfEntryToBeDeleted = MapIterator.GetKey();
			m_DebugStringMap.Delete(KeyOfEntryToBeDeleted);
			MapIterator.Start();	//	Start from the beginning of the map again after deleting an entry
		}
		else
		{
			MapIterator.Next();
		}
	}
}

char *CDebugStrings::GetString(u32 textHash)
{
	sysCriticalSection criticalSection(g_DebugStringCriticalToken);

	const CDebugString *pDebugString = m_DebugStringMap.Access(textHash);
	if (pDebugString)
	{
		return pDebugString->GetString();
	}

	return NULL;
}


void CTextFile::AddToDebugStringMap(const char *pTextKey, const char *pTextToDisplay)
{
	m_DebugStrings.Add(pTextKey, pTextToDisplay);
}

void CTextFile::FlagForDeletionFromDebugStringMap(const char *pTextKey)
{
	m_DebugStrings.FlagForDeletion(pTextKey);
}

void CTextFile::FlagAllForDeletionFromDebugStringMap()
{
	m_DebugStrings.FlagAllForDeletion();
}

void CTextFile::DeleteAllFlaggedEntriesFromDebugStringMap()
{
	m_DebugStrings.DeleteAllFlagged();
}
#endif //	__BANK

#undef __rage_channel
// eof
