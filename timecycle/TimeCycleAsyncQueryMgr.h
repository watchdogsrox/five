#ifndef TIMECYCLE_ASYNC_QUERY_MGR_H_
#define TIMECYCLE_ASYNC_QUERY_MGR_H_

class CEntity;
class CPed;

namespace CTimeCycleAsyncQueryMgr
{
	void Init();
	void Shutdown();

	void StartAsyncProcessing();
	void WaitForAsyncProcessingToFinish();

	void AddNonPedEntity(CEntity* pEntity);
	void AddPed(CPed* pPed, float incaredNess);

#if __ASSERT
	bool IsRunning();
#endif
};

#endif

