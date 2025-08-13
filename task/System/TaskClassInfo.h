#ifndef TASK_CLASS_INFO_H
#define TASK_CLASS_INFO_H

// Rage headers
#include "atl/singleton.h"

// Game headers
#include "Task/System/TaskTypes.h"

////////////////////////////////////////////////////////////////////////////////

class CTaskClassInfo
{
public:
	CTaskClassInfo()
		: m_iTaskSize(0)
#if !__NO_OUTPUT
		, m_pName(NULL)
		, m_StaticStateNameFunction(NULL)
#endif // !__NO_OUTPUT
	{
	}

	~CTaskClassInfo()
	{
#if !__NO_OUTPUT
/*Can't delete this as it is a static string in the code and not allocated on the heap
		if(m_pName)
		{
			delete m_pName;
		}*/
#endif // !__NO_OUTPUT
	}

	size_t m_iTaskSize;
#if !__NO_OUTPUT
	const char* m_pName;
	typedef const char *(*tStaticStateNameFunction)(s32);
	tStaticStateNameFunction m_StaticStateNameFunction;
#endif // !__NO_OUTPUT
};

////////////////////////////////////////////////////////////////////////////////

class CTaskClassInfoManager
{
public:
	CTaskClassInfoManager();

	s32 GetLargestTaskSize() const;
#if !__NO_OUTPUT
	bool TaskNameExists( s32 iTaskType ) const;
	const char*	GetTaskName( s32 iTaskType ) const;
	const char *GetTaskStateName( s32 iTaskType, s32 iState ) const;
#endif // !__NO_OUTPUT

	static void Init(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);
#if !__NO_OUTPUT
	static void PrintTaskNamesAndSizes();
	static void PrintOnlyLargeTaskNamesAndSizes();
	static void PrintTaskNamesAndSizes(const bool bPrintOnlyLargeTasks);
#endif // !__NO_OUTPUT

private:
	void Init();
	s32 GetLargestTaskIndex() const;

	CTaskClassInfo m_TaskClassInfo[CTaskTypes::MAX_NUM_TASK_TYPES];
};

typedef atSingleton<CTaskClassInfoManager> CTaskInfoManagerSingleton;
#define TASKCLASSINFOMGR CTaskInfoManagerSingleton::InstanceRef()
#define INIT_TASKCLASSINFOMGR										\
	do {															\
		if(!CTaskInfoManagerSingleton::IsInstantiated()) {			\
			CTaskInfoManagerSingleton::Instantiate();				\
		}															\
	} while(0)														\
	//END
#define SHUTDOWN_TASKCLASSINFOMGR CTaskInfoManagerSingleton::Destroy()

#endif // TASK_CLASS_INFO_H
