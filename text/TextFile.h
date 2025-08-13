/////////////////////////////////////////////////////////////////////////////////
//
// FILE :      TextFile.h
// PURPOSE :   Deals with reading text from the text files
// AUTHOR :    KRH 13/05/98
// NICKED BY : Obbe. (Thanks Keith)
// NICKED FURTHER BY: Derek Payne - modifying all this 17/04/09
//
/////////////////////////////////////////////////////////////////////////////////

// NOTES - this is still work in progress - a lot of stuff still needs shifting around

#ifndef TEXTFILE_H
#define TEXTFILE_H

//	Rage headers
#if __BANK
#include "atl/map.h"
#endif	//	__BANK
#include "atl\array.h"
#include "atl/binmap.h"
#include "file\asynchstream.h"
#include "string\unicode.h"

#include "system\tls.h"

// Game headers
#include "..\system\filemgr.h"
#include "..\scene\ExtraContentDefs.h"

namespace rage { class fiPackfile; }

enum
{
	MISSION_TEXT_SLOT,
	PHONE_TEXT_SLOT,
	ODDJOB_TEXT_SLOT,
	MINIGAME_TEXT_SLOT,
	SHOP_TEXT_SLOT,
	OBJECT_TEXT_SLOT,
	MISSION_DIALOGUE_TEXT_SLOT,
	AMBIENT_DIALOGUE_TEXT_SLOT,
	MP_STATS_TEXT_SLOT,
	MENU_TEXT_SLOT,
	PROPERTY_TEXT_SLOT,
	DLC_TEXT_SLOT0,
	DLC_TEXT_SLOT1,
	DLC_TEXT_SLOT2,
	DLC_MISSION_DIALOGUE_TEXT_SLOT,
	DLC_AMBIENT_DIALOGUE_TEXT_SLOT,
	RADIO_WHEEL_TEXT_SLOT,
	BINK_MOVIE_TEXT_SLOT,
	BINK_MOVIE_TEXT_SLOT1,
	BINK_MOVIE_TEXT_SLOT2,
	DLC_MISSION_DIALOGUE_TEXT_SLOT2,
	CREDITS_TEXT_SLOT,
	GTA5_GLOBAL_TEXT_SLOT,
	NUM_TEXT_SLOTS,
};

#define NUM_BINK_MOVIE_TEXT_SLOTS ((BINK_MOVIE_TEXT_SLOT2 - BINK_MOVIE_TEXT_SLOT)+1)
#define MISSION_NAME_LENGTH	(8)
#define TEXT_KEY_SIZE		(16)

#if __BANK
class CDebugString
{
public:
	CDebugString() : m_string(NULL) {}
	~CDebugString() {if(m_string) delete[] m_string;}

	void Set(const char* pText);
	void SetFlaggedForDeletion(bool bDelete) { m_bFlaggedForDeletion = bDelete; }
	bool GetFlaggedForDeletion() { return m_bFlaggedForDeletion; }

	char *GetString() const { return m_string; }
private:
	char* m_string;
	bool m_bFlaggedForDeletion;
};

// Use a critical section to avoid clashes between Add() and GetString()
// Instead of removing entries immediately on the Update thread, flag them for removal and 
// only actually remove the flagged strings in CGtaRenderThreadGameInterface::PerformSafeModeOperations()
class CDebugStrings
{
public:
	void Add(const char *pTextKey, const char *pTextToDisplay);
	void FlagForDeletion(const char *pTextKey);
	void FlagAllForDeletion();

	void DeleteAllFlagged();

	char *GetString(u32 textHash);

private:
	atMap<u32, CDebugString> m_DebugStringMap;
};
#endif //	__BANK


struct CGxt2Entry {
	u32 Hash;			// Hash of the text code
	u32 Offset;			// Byte offset, relative to the header, of the string.
};

struct CGxt2Header {
	u32 Magic;					// 'GXT2' in native endian
	u32 Count;					// Number of strings in file
	CGxt2Entry Entries[0];		// Directory of Count objects, followed by a dummy object with final offset

	const char *GetStringByIndex(size_t i);
	const char *GetStringByHash(u32 hash);
};

class CExtraTextFile
{
public:
	CExtraTextFile(bool hasAdditionalTextSlots, bool hasGlobalTextSlots, bool isTitleUpdate);
	~CExtraTextFile();

	void Init(const char* deviceName);
	void Shutdown();
	const char* GetMountName() {return m_mountName;}
	fiPackfile* GetPackFile() {return m_PackFile;}
	bool GetHasGlobalText() {return m_bHasGlobalTextSlot;}
	bool GetHasAdditionalText() {return m_bHasAdditionalTextSlots;}
	bool GetIsTitleUpdate() {return m_bIsTitleUpdate;}
private:
	fiPackfile *m_PackFile;
	bool m_bIsLoaded;
	bool m_bHasGlobalTextSlot;
	bool m_bHasAdditionalTextSlots;
	bool m_bIsTitleUpdate;
	char m_mountName[RAGE_MAX_PATH];
};

//
//
//
class CTextFile
{
public:
	CTextFile(void);
	~CTextFile(void);
	enum eSource
	{
		TEXT_SOURCE_DISK,
		TEXT_SOURCE_TU,
		TEXT_SOURCE_TU2,
		TEXT_SOURCE_PATCH,
		TEXT_SOURCE_DLC,
	};

	enum eLocationOfTextBlock
	{
		TEXT_LOCATION_GTA5_WITH_POSSIBLE_PATCH,		//	We expect the block name to exist in the GTA5 text. We'll also search for an additional patch in the DLC text.
		TEXT_LOCATION_ADDED_FOR_DLC,				//	Only search DLC for the block name
		TEXT_LOCATION_CHECK_BOTH_GTA5_AND_DLC		//	If block name exists in GTA5 text then search for an additional patch in the DLC text. If name doesn't exist in GTA5 then search for the block in DLC
	};

//	void GetNameOfLoadedMissionText(char *pStringToFill);
	bool IsBasicTextLoaded(void) { return m_bTextIsLoaded; }
	void LoadOrReadSizes(bool bLoad, u32 GxtFileIndx, u32 &OffsetsTableInfo, u32 &TextKeyChunkInfo, u32 &TextDataChunkInfo, bool bUseSystemLanguage);
	void ReloadAfterLanguageChange();
	void Load(bool bUseSystemLanguage = false);
	void InitAdditionalText(int SlotNumber);
	s32 GetAdditionalTextIndex(const char* pMissionTextStem);
	void RequestAdditionalText(const char *pMissionTextStem, s32 SlotNumber, eLocationOfTextBlock textLocation = TEXT_LOCATION_GTA5_WITH_POSSIBLE_PATCH);
	bool HasAdditionalTextLoaded(int SlotNumber);
	bool HasThisAdditionalTextLoaded(const char *pMissionTextStem, s32 SlotNumber);
	bool IsRequestingAdditionalText(int SlotNumber);
	bool IsRequestingThisAdditionalText(const char *pMissionTextStem, s32 SlotNumber);
	const char* GetTextStemForAdditonalSlot(const int idx) const;
	void ProcessStreamingAdditionalText();
	void LoadAdditionalText(fiStream* pStream, s32 SlotNumber);
	void LoadAdditionalText(const char *pMissionTextStem, s32 SlotNumber, eLocationOfTextBlock textLocation = TEXT_LOCATION_GTA5_WITH_POSSIBLE_PATCH);
	const char* GetGxtDirectory(s32 index);
	const char* GetLanguageFile(char *buffer, size_t bufferSize, bool bUseSystemLanguage, eSource eTextSource, bool isDLCpatch = false);
	const char* GetMissionTextFilename(s32 missionTextIndex, char* pBuffer);
	const char* GetGxtFilename(s32 missionTextIndex, char* pBuffer, bool bUseSystemLanguage);

	void Unload(bool unloadExtra = false);
	void UnloadCore(){Unload(false);}
	void UnloadExtra(){Unload(true);}
	char* Get(const char *Ident);
    char* Get(const u32 hashkey, const char *);
    char* Get( atHashString const locKey ) { return Get( locKey.GetHash(), locKey.TryGetCStr() ); }
	bool IsDLCDevice(const char* deviceName) const;
	bool DoesTextLabelExist(const char *Ident);
	bool DoesTextLabelExist(const u32 hashkey);
	void AppendToExtraGlobals(const char* device, bool remove, bool titleUpdate);
	void SetUpdatedErrorText(CGxt2Header* text, bool remove = false);
	inline bool IsJapanese(void) { return (m_LanguageCode=='j');}

	s32 GetBlockContainingLastReturnedString() { return m_TextBlockContainingTheLastStringReturnedByGetFunction; }

	int GetLanguageFromName(const char* pLanguage);

#if __BANK
	bool bShowTextIdOnly;

	void AddToDebugStringMap(const char *pTextKey, const char *pTextToDisplay);
	void FlagForDeletionFromDebugStringMap(const char *pTextKey);
	void FlagAllForDeletionFromDebugStringMap();

	void DeleteAllFlaggedEntriesFromDebugStringMap();
#endif	//	__BANK

	void FreeTextSlot(s32 slot, bool extraText);
	atMap<atHashString,CExtraTextFile*>& GetExtraText() {return m_extraText;}
	void AddExtracontentText(const char* deviceName, bool hasAdditionalTextSlots, bool hasGlobalTextSlot, bool isTitleUpdate);
	void AddExtracontentTextFromMetafile(const char* fileName);
	void RemoveExtracontentTextFromMetafile(const char* fileName);
	void RemoveExtracontentText(const char* deviceName);
private:
	char *GetInternal(u32 hashcode);
	s32 GetTextIndexInternal(const char* pMissionTextStem, fiPackfile* packFile);
	char *SearchForText(u32 hashcode, CGxt2Header* textSlot);
	void LoadTextIntoSlot(s32 slot,const char *stem,bool blocking, eLocationOfTextBlock textLocation);
	void StreamIntoSlots(const char* name, bool blocking,CGxt2Header *& _textSlot,  u8 &_TextState, char* _TextBlockLoadedIntoThisSlot,const char* stem BANK_ONLY(, s32 slot));
	atRangeArray<CGxt2Header *,NUM_TEXT_SLOTS> m_Text;		// We load global table into slot 0, everything else is biased by one
	atRangeArray<CGxt2Header *,NUM_TEXT_SLOTS> m_PatchText;		// We load global table into slot 0, everything else is biased by one
	char		m_TextBlockLoadedIntoThisSlot[NUM_TEXT_SLOTS][MISSION_NAME_LENGTH];
	fiPackfile	*m_Packfile;
	atRangeArray<u8,NUM_TEXT_SLOTS> m_TextState;
	atRangeArray<u8,NUM_TEXT_SLOTS> m_PatchTextState;
	atMap<atHashString, CExtraTextFile*> m_extraText;
	atBinaryMap<char*, u32> m_extraGlobals;
	atBinaryMap<char*, u32> m_updatedErrorText;
	atMap<atHashString, CExtraTextMetaFile*> m_extraTextMetafiles;

#if __BANK
	CDebugStrings	m_DebugStrings;
#endif

	s32		m_TextBlockContainingTheLastStringReturnedByGetFunction;

	char		m_LanguageCode;
	bool		m_bUsesMultipleTextChunks;
	bool		m_bTextIsLoaded;
	
	char		GetUpperCase (const char C);
};

// wrapper class needed to interface with game skeleton code
class CTextFileWrapper
{
public:

    static void Load(unsigned);

};

extern CTextFile	TheText;		// For all our text needs

#endif
