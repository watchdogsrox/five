#ifndef PED_SCRIPTED_TASK_RECORD_H
#define PED_SCRIPTED_TASK_RECORD_H

#include "Debug/Debug.h"
#include "Scene/Entity.h"
#include "Event/Event.h"
#include "Scene/RegdRefTypes.h"
#include "ai/task/taskregdref.h"

namespace rage { class aiTask; }
class CEventScriptCommand;

//This class is used to track the progress of a script command from script function to event to task.
class CPedScriptedTaskRecordData
{
public:

	friend class CPedScriptedTaskRecord;
	
	enum
	{
		EVENT_STAGE,
		ACTIVE_TASK_STAGE,
		DORMANT_TASK_STAGE,
		VACANT_STAGE,
		GROUP_TASK_STAGE,
		UNUSED_ATTRACTOR_SCRIPT_TASK_STAGE,	//	GW - 30/08/06 - once the script grabs the head of the queue, he is no longer considered part of the attractor
		SECONDARY_TASK_STAGE,
		MAX_NUM_STAGES
	};

	CPedScriptedTaskRecordData();
	~CPedScriptedTaskRecordData()
	{
	}
	
	bool IsVacant() const
	{
		return (!m_pEvent && !m_pTask && !m_pPed);
	}	
	void Flush();
#if __DEV
	void Set(CPed* pPed, const s32 iCommandType, const CEventScriptCommand* pEvent, const char* pString);
#else
	void Set(CPed* pPed, const s32 iCommandType, const CEventScriptCommand* pEvent);
#endif
	void Set(CPed* pPed, const s32 iCommandType, const aiTask* pTask);
	void SetAsAttractorScriptTask(CPed* pPed, const s32 iCommandType, const aiTask* pTask);
	void SetAsGroupTask(CPed* pPed, const s32 iCommandType, const aiTask* pTask);
	void AssociateWithEvent(CEventScriptCommand* p); 
	void AssociateWithTask(aiTask* p);

public:

	s32 m_iCommandType;
	RegdConstEvent m_pEvent;
	RegdConstAITask m_pTask;
	RegdPed m_pPed;	
	int m_iStage;
};

class CPedScriptedTaskRecord
{
public:

	enum
	{
		MAX_NUM_SCRIPTED_TASKS=175
	};
	
	CPedScriptedTaskRecord(){}
	~CPedScriptedTaskRecord(){}
	
	//Get an unused slot in the array.
	static int GetVacantSlot();
	
	//Update all the slots that are being used.
	static void Process();
	
	//Get the status of a ped on a specific command.
	static int GetStatus(const CPed* pPed, const int iCommandType );
	//Get the status of a ped on any command.
	static int GetStatus(const CPed* pPed);
	
	static CPedScriptedTaskRecordData* GetRecordAssociatedWithEvent(CEventScriptCommand* p);
	static CPedScriptedTaskRecordData* GetRecordAssociatedWithTask(aiTask* p);
	
	static CPedScriptedTaskRecordData ms_scriptedTasks[MAX_NUM_SCRIPTED_TASKS];

#if __DEV
	static void CheckUniqueScriptCommand(CPed* pPed, const int iCommandType, const CEventScriptCommand* pEvent, const char* pString);
#endif	

private:	

#if !__FINAL
	static void SpewRecords();
#endif
};

#endif
