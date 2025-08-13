/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    scene/ipl/IplCullBox.h
// PURPOSE : management of artist-placed aabbs to prevent specified map data files from loading
// AUTHOR :  Ian Kiigan
// CREATED : 31/03/2011
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef _SCENE_IPL_IPLCULLBOX_H_
#define _SCENE_IPL_IPLCULLBOX_H_

#include "atl/array.h"
#include "atl/hashstring.h"
#include "atl/string.h"
#include "parser/macros.h"
#include "spatialdata/aabb.h"
#include "vector/vector3.h"

#if __BANK
#include "scene/ipl/IplCullBoxEditor.h"
#endif

// a means of referencing a "container" (a section of map data to be culled)
class CIplCullBoxContainer
{
public:
	atHashString m_name;
	s32 m_iplSlotIndex;
};

class CIplCullBoxCullList
{
public:
	void Init(s32 numIplSlots);
	void SetActive(bool bActive);
	void MarkIplSlot(s32 iplSlot) { s32 i, j; GetCullBit(iplSlot, i, j); m_bitSet[i] |= ( 1 << j); }
	bool IsSet(s32 i, s32 j) const { return (m_bitSet[i] & (1 << j)) != 0;}

	void Reset() { m_bitSet.clear(); }
	void GetCullBit(s32 iplSlot, s32& cullSlot, s32& offset) const { cullSlot = (iplSlot / BITS_PER_CULL_SLOT); offset = (iplSlot % BITS_PER_CULL_SLOT); }
	bool IsMapDataSlotMarked(s32 slot) { s32 i, j; GetCullBit(slot, i, j); return (m_bitSet[i] & (1 << j)) != 0; }

private:
	enum
	{
		BITS_PER_CULL_SLOT = 32
	};

	s32 GetIplSlot(s32 cullSlot, s32 offset) const { return (cullSlot * BITS_PER_CULL_SLOT) + offset; }
	

	atArray<u32> m_bitSet;

#if __BANK
public:
	void GetSlotList(atArray<s32>& iplSlotList);
#endif	//__BANK
};


// runtime cull box representation - an aabb + a list of container hashes (and IPL indices) to cull
class CIplCullBoxEntry
{
public:
	CIplCullBoxEntry() : m_bEnabled(true), m_bActive(false) {}

	void Reset() {m_cullList.Reset(); m_culledContainerHashes.Reset();}

	void SetActive(bool bActive) { m_bActive=bActive; m_cullList.SetActive(bActive); }
	void GenerateCullList();
	bool IsCullingContainer(u32 containerHash);
	void AddContainer(u32 containerHash);
	void RemoveContainer(u32 containerHash);

	atHashString m_name;
	spdAABB m_aabb;
	CIplCullBoxCullList m_cullList;
	atArray<u32> m_culledContainerHashes;						// array of container name hashes
	bool m_bEnabled;
	bool m_bActive;

private:
	void RecursivelyAddCulledDeps(s32 iplSlot);

	PAR_SIMPLE_PARSABLE;
};

// contains a number of cull boxes
class CIplCullBoxFile
{
public:
	void Reset()							{ m_entries.Reset(); }
	void Add(CIplCullBoxEntry& newEntry)	{ m_entries.PushAndGrow(newEntry); }
	void Remove(s32 index)					{ m_entries.Delete(index); }
	CIplCullBoxEntry& Get(u32 index)		{ return m_entries[index]; }
	s32 Size()								{ return m_entries.size(); }

private:
	atArray<CIplCullBoxEntry> m_entries;

	PAR_SIMPLE_PARSABLE;
};

// manages loading / unloading of cull data, and flagging of IPLs to be culled
class CIplCullBox
{
public:

	static void Initialise(unsigned);

	static void Reset();
	static void Update(const Vector3& vPlayerPos);
	static void RegisterContainerName(const char* pszContainerName, s32 iplSlotIndex);
	static void Load();
	static void LoadDLCFile(const char* filePath);
	static void UnloadDLCFile(const char* filePath);
	static void SetBoxEnabled(u32 cullBoxNameHash, bool bEnabled);	// used by script commands
	static bool IsBoxEnabled(u32 cullBoxNameHash); 
	static void GenerateCullListForBox(u32 cullBoxNameHash);
	static void RegenerateAllCullBoxLists();

	static CIplCullBoxEntry const* GetCullboxEntry(u32 cullBoxNameHash);
	static void RemoveCulledAtPosition(Vec3V_In vPos, atArray<u32>& slotList);
#if __BANK
	static void Save();
#endif

	static void SetEnabled(bool enabled)
	{
		bool changed = ms_bEnabled != enabled;
		ms_bEnabled = enabled;
		
		if (changed)
		{
			ResetCulled();
		}
	}
	static void RegisterContainers();

private:
	static void ReadFile(const char* filePath, const char* extension);
	static void AddNewBox(CIplCullBoxEntry& newBox);
	static void ResetCulled();

	static atArray<CIplCullBoxContainer> ms_containerList;
	static atString m_dlcFilePath;
	static CIplCullBoxFile ms_boxList;
	static bool ms_bCullEverything;
	static bool ms_bLoaded;

#if __BANK
	static void CheckForLeaks();

	friend class CIplCullBoxEditor;
	static bool ms_bCheckForLeaks;
	static bool ms_bTestAgainstCameraPos;
#endif	//__BANK
	static bool ms_bEnabled;
};

#endif	//_SCENE_IPL_IPLCULLBOX_H_
