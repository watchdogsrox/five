#ifndef INC_LADDER_INFO_MANAGER_H
#define INC_LADDER_INFO_MANAGER_H

#include "LadderMetadata.h"

//rage
#include "atl/array.h"

class CLadderMetadataManager
{
public:

	// PURPOSE: Initialise
	static void Init(u32 uInitMode);

	// PURPOSE: Shutdown
	static void Shutdown(u32 uShutdownMode);

	// PURPOSE: Access a Ladder metadata
	static const CLadderMetadata* GetLadderDataFromHash(u32 uNameHash) 
	{
		for(s32 i = 0; i < ms_Instance.m_LadderData.GetCount(); i++)
		{
			if(ms_Instance.m_LadderData[i].m_Name.GetHash() == uNameHash)
			{
				return &ms_Instance.m_LadderData[i];
			}
		}
		return NULL;
	}

	// This is quite a optimization, since it is quite a waste getting this information out of the
	// entities every frame all the time, so we just grab it from the metadata directly instead.
	// Just making sure we don't add different clipsets to the metadata, if so, we should assert here
	// and if we do we might need to reconsider this but I doubt it will happend
	static u32 GetLadderClipSet()
	{
		u32 UniqueClipSet = 0;
		for(s32 i = 0; i < ms_Instance.m_LadderData.GetCount(); i++)
		{
#if __ASSERT
			if (UniqueClipSet != 0 && UniqueClipSet != ms_Instance.m_LadderData[i].m_ClipSet.GetHash())
			{
				Assertf(0, "Unexpected amount of clipsets in ladder metadata");
			}
#endif
			UniqueClipSet = ms_Instance.m_LadderData[i].m_ClipSet.GetHash();
		}

		return UniqueClipSet;
	}

	static u32 GetLadderFemaleClipSet()
	{
		u32 UniqueClipSet = 0;
		for(s32 i = 0; i < ms_Instance.m_LadderData.GetCount(); i++)
		{
#if __ASSERT
			if (UniqueClipSet != 0 && UniqueClipSet != ms_Instance.m_LadderData[i].m_FemaleClipSet.GetHash())
			{
				Assertf(0, "Unexpected amount of female clipsets in ladder metadata");
			}
#endif
			UniqueClipSet = ms_Instance.m_LadderData[i].m_FemaleClipSet.GetHash();
		}

		return UniqueClipSet;
	}

#if __BANK
	// Add widgets
	static void AddWidgets(bkBank& bank);
#endif // __BANK

private:

	// Delete the data
	static void Reset();

	// Load the data
	static void Load();

#if __BANK
	// Save the data
	static void Save();
#endif // __BANK

	// The ladder data storage
	atArray<CLadderMetadata> m_LadderData;

	// Static manager object
	static CLadderMetadataManager ms_Instance;

	PAR_SIMPLE_PARSABLE;
};

#endif // INC_LADDER_INFO_MANAGER_H
