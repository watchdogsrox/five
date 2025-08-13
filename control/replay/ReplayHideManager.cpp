#include "ReplayHideManager.h"

#if GTA_REPLAY

#include "scene/building.h"
#include "Objects/DummyObject.h"
#include "control/replay/ReplayController.h"

//====================================================================================
//====================================================================================

atArray<CutsceneNonRPObjectHide> ReplayHideManager::m_CutsceneNoneRPTrackedObjectHides;
atArray<CutsceneNonRPObjectHide> ReplayHideManager::m_CutsceneNoneRPTrackedObjectHidesBlockChange;

void	ReplayHideManager::OnRecordStart()
{
	OnBlockChange();
}

void ReplayHideManager::OnBlockChange()
{
	m_CutsceneNoneRPTrackedObjectHides.Reset();
	m_CutsceneNoneRPTrackedObjectHidesBlockChange.Reset();
	ForAllNonReplayTrackedObjects(CaptureAllCutsceneHidesCB);
}

void	ReplayHideManager::AddNewHide(CEntity *pEntity, bool bVisible)
{
	CBaseModelInfo *pModelInfo = pEntity->GetBaseModelInfo();
	if(pModelInfo)
	{
		CutsceneNonRPObjectHide entry;
		entry.shouldHide = !bVisible;
		entry.modelNameHash = pModelInfo->GetModelNameHash();
		entry.searchRadius = pEntity->GetBoundRadius();
		Vector3	pos = pEntity->GetBoundCentre();
		entry.searchPos.Store(pos);
		m_CutsceneNoneRPTrackedObjectHides.Grow() = entry;
	}
}

void	ReplayHideManager::RecordData(ReplayController& controller, bool blockChange)
{
	if(blockChange)
	{
		for(int i=0;i<m_CutsceneNoneRPTrackedObjectHidesBlockChange.size();i++)
		{
			CPacketCutsceneNonRPObjectHide* pPacket = controller.RecordPacket<CPacketCutsceneNonRPObjectHide>(m_CutsceneNoneRPTrackedObjectHidesBlockChange[i]);
			pPacket->SetIsInitialStatePacket(true); // Only played at the start of a clip
		}
		m_CutsceneNoneRPTrackedObjectHidesBlockChange.Reset();
	}
	else
	{
		for(int i=0;i<m_CutsceneNoneRPTrackedObjectHides.size();i++)
		{
			controller.RecordPacket<CPacketCutsceneNonRPObjectHide>(m_CutsceneNoneRPTrackedObjectHides[i]);
		}
		m_CutsceneNoneRPTrackedObjectHides.Reset();
	}
}

s32		ReplayHideManager::GetMemoryUsageForFrame(bool blockChange)
{
	if(blockChange)
	{
		return sizeof(CPacketCutsceneNonRPObjectHide) * m_CutsceneNoneRPTrackedObjectHidesBlockChange.size();
	}
	else
	{
		return sizeof(CPacketCutsceneNonRPObjectHide) * m_CutsceneNoneRPTrackedObjectHides.size();
	}
}

void	ReplayHideManager::OnEntry()
{
		// Nothing, just yet
}

void	ReplayHideManager::OnExit()
{
	ForAllNonReplayTrackedObjects(RemoveAllReplayCutsceneHidesCB);
}

void	ReplayHideManager::Process()
{
	// Nothing, just yet
}

bool ReplayHideManager::IsNonReplayTrackedObjectType(CEntity *pEntity)
{
	if( pEntity->GetIsTypeBuilding() || pEntity->GetIsTypeDummyObject() )
	{
		return true;
	}
	return false;
}

void	ReplayHideManager::ForAllNonReplayTrackedObjects( void (*cbFn)(CEntity *pEntity) )
{
	// Buildings
	{
		CBuilding::Pool* pool = CBuilding::GetPool();
		for(int i=0;i<pool->GetSize();i++)
		{
			CBuilding* pBuilding = pool->GetSlot(i);
			cbFn(pBuilding);
		}
	}

	// DummyObjects
	{
		CDummyObject::Pool* pool = CDummyObject::GetPool();
		for(int i=0;i<pool->GetSize();i++)
		{
			CDummyObject* pDummy = pool->GetSlot(i);
			cbFn(pDummy);
		}
	}
}

// Find anything that was hidden by cutscene and add a packet for it.
void	ReplayHideManager::CaptureAllCutsceneHidesCB(CEntity *pEntity)
{
	if(pEntity && pEntity->GetBaseModelInfo())
	{
		if( pEntity->GetIsVisibleForModule(SETISVISIBLE_MODULE_CUTSCENE) == false)
		{
			CBaseModelInfo *pModelInfo = pEntity->GetBaseModelInfo();

			// Got one.
			CutsceneNonRPObjectHide entry;
			entry.shouldHide = true;
			entry.modelNameHash = pModelInfo->GetModelNameHash();
			entry.searchRadius = pEntity->GetBoundRadius();
			Vector3	pos = pEntity->GetBoundCentre();
			entry.searchPos.Store(pos);
			m_CutsceneNoneRPTrackedObjectHidesBlockChange.Grow() = entry;
		}
	}
}

// Find anything that was hidden by replay, and unhide it.
void	ReplayHideManager::RemoveAllReplayCutsceneHidesCB(CEntity *pEntity)
{
	if(pEntity)
	{
		if( pEntity->GetIsVisibleForModule(SETISVISIBLE_MODULE_REPLAY) == false )
		{
			pEntity->SetIsVisibleForModule(SETISVISIBLE_MODULE_REPLAY, true);
		}
	}
}

#endif	//GTA_REPLAY
