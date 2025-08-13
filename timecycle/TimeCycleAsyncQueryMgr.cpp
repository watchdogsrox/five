#include "timecycle/TimeCycleAsyncQueryMgr.h"

// rage headers
#include "atl/array.h"
#include "profile/timebars.h"
#include "system/ipc.h"

// framework headers
#include "system/dependencyscheduler.h"

// game headers
#include "Peds/Ped.h"
#include "scene/Entity.h"
#include "timecycle/CalcAmbientScaleAsync.h"
#include "timecycle/tcmodifier.h"
#include "timecycle/TimeCycle.h"
#include "vfx/VfxHelper.h"

atArray<CEntity*>					g_aNonPedEntities;
atArray<CalcAmbientScalePedData>	g_aPedEntities;
sysDependency						g_TimeCycleAsyncQueryDependency;
u32									g_TimeCycleAsyncQueryDependencyRunning;

namespace CTimeCycleAsyncQueryMgr
{
	void Init()
	{
		u32 flags = 0;
		g_TimeCycleAsyncQueryDependency.Init( CalcAmbientScaleAsync::RunFromDependency, 0, flags );
		g_TimeCycleAsyncQueryDependency.m_Priority = sysDependency::kPriorityLow;
	}

	void Shutdown()
	{
	}

	void StartAsyncProcessing()
	{
		if (g_aPedEntities.GetCount() == 0 && g_aNonPedEntities.GetCount() == 0)
		{
			return;
		}

		g_timeCycle.BuildModifierTreeIfNeeded();
		g_timeCycle.SetModifierBoxTreeRebuildProhibited(true);
		tcModifier::SetOnlyMainThreadIsUpdating(false);

		g_TimeCycleAsyncQueryDependency.m_Params[0].m_AsPtr = &g_aNonPedEntities;
		g_TimeCycleAsyncQueryDependency.m_Params[1].m_AsPtr = &g_aPedEntities;
		g_TimeCycleAsyncQueryDependency.m_Params[2].m_AsPtr = &g_TimeCycleAsyncQueryDependencyRunning;

		g_TimeCycleAsyncQueryDependencyRunning = 1;

		sysDependencyScheduler::Insert(&g_TimeCycleAsyncQueryDependency);
	}

	void WaitForAsyncProcessingToFinish()
	{
		PF_PUSH_TIMEBAR("CTimeCycleAsyncQueryMgr WaitForAsyncProcessingToFinish");

		while(true)
		{
			volatile u32 *pDependencyRunning = &g_TimeCycleAsyncQueryDependencyRunning;
			if(*pDependencyRunning == 0)
			{
				break;
			}

			sysIpcYield(PRIO_NORMAL);
		}

		g_aNonPedEntities.ResetCount();
		g_aPedEntities.ResetCount();

		g_timeCycle.SetModifierBoxTreeRebuildProhibited(false);
		tcModifier::SetOnlyMainThreadIsUpdating(true);

		PF_POP_TIMEBAR();
	}

	void AddNonPedEntity(CEntity* pEntity)
	{
		Assert(!pEntity->GetIsTypePed());
		Assert(!g_TimeCycleAsyncQueryDependencyRunning);
		g_aNonPedEntities.PushAndGrow(pEntity);
	}

	void AddPed(CPed* pPed, float incaredNess)
	{
		Assert(!g_TimeCycleAsyncQueryDependencyRunning);
		CalcAmbientScalePedData& data = g_aPedEntities.Grow();
		data.m_pPed = pPed;
		data.m_incaredNess = incaredNess;
	}

#if __ASSERT
	bool IsRunning()
	{
		return (g_TimeCycleAsyncQueryDependencyRunning != 0);
	}
#endif

} // end of namespace CTimeCycleAsyncQueryMgr

