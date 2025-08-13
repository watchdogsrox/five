// File header
#include "Event/System/EventDataManager.h"

// Rage headers
#include "Parser/manager.h"

// Game headers
#include "core/game.h"
#include "Event/Decision/EventDecisionMakerManager.h"
#include "Event/System/EventData.h"
#include "scene/DataFileMgr.h"
#include "scene/dlc_channel.h"

// Macro to disable optimisations if set
AI_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// CEventDataManager
//////////////////////////////////////////////////////////////////////////

// Static initialisation
CEventDataManager CEventDataManager::ms_instance;


// CEventDataFileMounter for overriding CEventDataManager infos.
class CEventDataFileMounter : public CDataFileMountInterface
{
	virtual bool LoadDataFile(const CDataFileMgr::DataFile & file)
	{
		dlcDebugf3("CEventDataFileMounter::LoadDataFile: %s type = %d", file.m_filename, file.m_fileType);
		CEventDataManager::Override(file.m_filename);
 		return true;
 	}
 
 	virtual void UnloadDataFile(const CDataFileMgr::DataFile & OUTPUT_ONLY(file)) 
	{
		dlcDebugf3("CEventDataFileMounter::UnloadDataFile %s type = %d", file.m_filename, file.m_fileType);
		CEventDataManager::Revert();
	}
 
} g_EventDataFileMounter;


CEventDataManager::CEventDataManager()
{
}

void CEventDataManager::Override(const char* fileName)
{
	// Shutdown pools
	CEventDecisionMakerManager::Shutdown();

	Verifyf(ms_instance.LoadXmlMeta(fileName), "CEventDataManager::Init - failed to load data.");

	// Initialise pools
	CEventDecisionMakerManager::Init();
}


void CEventDataManager::Revert()
{
	CEventDataManager::Override("common:/data/events");
}


void CEventDataManager::Init(unsigned initMode)
{
    if(initMode == INIT_AFTER_MAP_LOADED)
    {
		CDataFileMount::RegisterMountInterface(CDataFileMgr::EVENTS_OVERRIDE_FILE, &g_EventDataFileMounter, eDFMI_UnloadFirst);

		Verifyf(ms_instance.LoadXmlMeta("common:/data/events"), "CEventDataManager::Init - failed to load data.");

	    // Initialise pools
	    CEventDecisionMakerManager::Init();
    }
}

void CEventDataManager::Shutdown(unsigned shutdownMode)
{
    if(shutdownMode == SHUTDOWN_WITH_MAP_LOADED)
    {
	    // Shutdown pools
	    CEventDecisionMakerManager::Shutdown();
    }
}

#if !__NO_OUTPUT
EXTERN_PARSER_ENUM(eEventType);
const char * CEventDataManager::GetEventTypeName(enum eEventType event) {
	Assert((int)event >= 0 && (int)event < eEVENTTYPE_MAX);
	return PARSER_ENUM(eEventType).m_Names[(int)event];
};
#endif

const CEventDataDecisionMaker * CEventDataManager::GetDecisionMaker(const u32 hash) const
{
	int makerCount = m_eventDecisionMaker.GetCount();
	const CEventDataDecisionMaker*const *maker = m_eventDecisionMaker.GetElements();

	for(int i=0;i<makerCount;i++)
	{
		if ((*maker)->GetName().GetHash() == hash)
			return *maker;
		maker++;
	}

	return NULL;
}

const CEventDecisionMakerResponse * CEventDataManager::GetDecisionMakerResponse(const u32 hash) const
{
	int responseDataCount = m_eventDecisionMakerResponseData.GetCount();
	const CEventDecisionMakerResponse*const * responseData = m_eventDecisionMakerResponseData.GetElements();

	for(int i=0;i<responseDataCount;i++)
	{
		if ((*responseData)->GetName().GetHash() == hash)
			return *responseData;
		responseData++;
	}
	return NULL;
}

const CEventDataResponseTask * CEventDataManager::GetEventResponseTask(const u32 hash) const
{
	int responseTaskDataCount = m_eventResponseTaskData.GetCount();
	const CEventDataResponseTask *const *responseTask = m_eventResponseTaskData.GetElements();

	for(int i=0;i<responseTaskDataCount;i++)
	{
		if ((*responseTask)->GetName().GetHash() == hash)
			return *responseTask;

		responseTask++;
	}

	return NULL;
}

//
// Xml ParGen based version
//

bool CEventDataManager::LoadXmlMeta(const char * const pFilename)
{
	if(PARSER.LoadObject(pFilename, "meta", *this))
	{
		// Call PostLoad() on all the decision-makers. Potentially this could be done
		// as parser callbacks, but that would require a more strict order, so that the
		// parent decision-maker is always defined before the child.
		for(int i = 0; i < m_eventDecisionMaker.GetCount(); i++)
		{
			if(m_eventDecisionMaker[i])
			{
				m_eventDecisionMaker[i]->PostLoad();
			}
		}

		return true;
	}
	return false;
}

#if __DEV

void CEventDataManager::SaveXmlMeta(const char * const filename)
{
	PARSER.SaveObject(filename, "meta", this);
}

#endif // __DEV
