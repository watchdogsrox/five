
#include "savegame_queue.h"

// Rage headers
#include "bank/bank.h"

// Framework headers
#include "fwsys/gameskeleton.h"

// Game headers
#include "SaveLoad/savegame_channel.h"
#include "SaveLoad/savegame_messages.h"
#include "SaveLoad/savegame_queued_operations.h"
#include "system/system.h"


SAVEGAME_OPTIMISATIONS()

RAGE_DEFINE_SUBCHANNEL(savegame, queue, DIAG_SEVERITY_DEBUG3)
#undef __savegame_channel 
#define __savegame_channel savegame_queue


atQueue<CSavegameQueuedOperation*, CSavegameQueue::MAX_SIZE_OF_SAVEGAME_QUEUE> CSavegameQueue::sm_QueueOfOperations;

#if __BANK
char CSavegameQueue::ms_SavegameQueueDebugDescriptions[MAX_SIZE_OF_SAVEGAME_QUEUE][MAX_LENGTH_OF_SAVEGAME_QUEUE_DEBUG_DESCRIPTION];
s32 CSavegameQueue::ms_SavegameQueueCount = 0;
#endif	//	__BANK




void CSavegameQueue::Init(unsigned initMode)
{
	if (INIT_CORE == initMode)
	{
		sm_QueueOfOperations.Reset();
	}
}


void CSavegameQueue::Shutdown(unsigned UNUSED_PARAM(shutdownMode))
{
	if (!sm_QueueOfOperations.IsEmpty())
	{
		CSavegameQueuedOperation *pQueuedOp = sm_QueueOfOperations.Top();

#if !__NO_OUTPUT
	#if __BANK
		savegameDisplayf("CSavegameQueue::Shutdown - about to Shutdown() and reset Status for the %s operation at the top of the queue", pQueuedOp->GetNameOfOperation());
	#else	//	__BANK
		savegameDisplayf("CSavegameQueue::Shutdown - about to Shutdown() and reset Status for the operation of type %d at the top of the queue", pQueuedOp->GetOperationType());
	#endif	//	__BANK
#endif	//	!__NO_OUTPUT

		pQueuedOp->Shutdown();
		pQueuedOp->m_Status = MEM_CARD_ERROR;
	}

	sm_QueueOfOperations.Reset();
}


bool CSavegameQueue::Push(CSavegameQueuedOperation *pNewOperation)
{
	savegameAssertf(CSystem::IsThisThreadId(SYS_THREAD_UPDATE), "CSavegameQueue::Push - can only call this on the Update thread");

	if (savegameVerifyf(pNewOperation, "CSavegameQueue::Push - pointer to new operation is NULL"))
	{
		pNewOperation->m_Status = MEM_CARD_ERROR;

		savegameDisplayf("CSavegameQueue::Push - number of items at start of push %d of %d", sm_QueueOfOperations.GetCount(), sm_QueueOfOperations.GetSize());

		if (savegameVerifyf(!sm_QueueOfOperations.IsFull(), "CSavegameQueue::Push - queue is full"))
		{
			//	assert if this type already exists in the queue?
			//	if (savegameVerifyf(!sm_QueueOfOperations.Find(Operation), "CSavegameQueue::Push - this operation already exists in the queue"))
			{
				pNewOperation->m_Status = MEM_CARD_BUSY;

				sm_QueueOfOperations.Push(pNewOperation);

#if !__FINAL
	#if __BANK
				savegameDisplayf("CSavegameQueue::Push - pushing %s operation. Setting its status to MEM_CARD_BUSY", pNewOperation->GetNameOfOperation());
	#else	//	__BANK
				savegameDisplayf("CSavegameQueue::Push - pushing operation of type %d. Setting its status to MEM_CARD_BUSY", pNewOperation->GetOperationType());
	#endif	//	__BANK
#endif	//	!__FINAL

				savegameDisplayf("CSavegameQueue::Push - number of items at end of push %d of %d", sm_QueueOfOperations.GetCount(), sm_QueueOfOperations.GetSize());

				return true;
			}
		}
	}

	return false;
}

bool CSavegameQueue::ContainsAnAutosave()
{
	s32 NumberOfEntriesInQueue = sm_QueueOfOperations.GetCount();

	for (s32 loop = 0; loop < NumberOfEntriesInQueue; loop++)
	{
		if (sm_QueueOfOperations[loop]->IsAutosave())
		{
			return true;
		}
	}

	return false;
}

bool CSavegameQueue::ContainsASinglePlayerSave()
{
	s32 NumberOfEntriesInQueue = sm_QueueOfOperations.GetCount();

	for (s32 loop = 0; loop < NumberOfEntriesInQueue; loop++)
	{
		switch (sm_QueueOfOperations[loop]->GetOperationType())
		{
			case SAVE_QUEUED_MANUAL_SAVE :
			case SAVE_QUEUED_AUTOSAVE :
#if GTA_REPLAY
			case SAVE_QUEUED_REPLAY_SAVE :
#endif // GTA_REPLAY
			case SAVE_QUEUED_MISSION_REPEAT_SAVE :
				return true;

			default:
				break;
		}
	}

	return false;
}

bool CSavegameQueue::ExistsInQueue(CSavegameQueuedOperation *pOperationToCheck, bool &bAtTop)
{
	bAtTop = false;
	if (!sm_QueueOfOperations.IsEmpty())
	{
		if (sm_QueueOfOperations.Find(pOperationToCheck))
		{
			if (sm_QueueOfOperations.Top() == pOperationToCheck)
			{
				bAtTop = true;
			}
			return true;
		}
	}

	return false;
}

void CSavegameQueue::Remove(CSavegameQueuedOperation *pOperationToRemove)
{
	if (!sm_QueueOfOperations.IsEmpty())
	{
		int indexOfQueueEntry = -1;
		if (sm_QueueOfOperations.Find(pOperationToRemove, &indexOfQueueEntry))
		{
			sm_QueueOfOperations.Delete(indexOfQueueEntry);
			pOperationToRemove->ClearStatus();	//	This sets it to MEM_CARD_COMPLETE
		}
	}
}



void CSavegameQueue::Update()
{
	savegameAssertf(CSystem::IsThisThreadId(SYS_THREAD_UPDATE), "CSavegameQueue::Update - can only call this on the Update thread");

//	CSavegameDialogScreens::SetMessageAsProcessing(false);
	CSavegameDialogScreens::BeforeUpdatingTheSavegameQueue();

	if (!sm_QueueOfOperations.IsEmpty())
	{
		CSavegameQueuedOperation *pQueuedOp = sm_QueueOfOperations.Top();
		if (pQueuedOp->Update())
		{
#if !__FINAL
	#if __BANK
			if (pQueuedOp->GetStatus() == MEM_CARD_COMPLETE)
			{
				savegameDisplayf("CSavegameQueue::Update - popping %s operation MEM_CARD_COMPLETE", pQueuedOp->GetNameOfOperation() );
			}
			else
			{
				savegameDisplayf("CSavegameQueue::Update - popping %s operation MEM_CARD_ERROR", pQueuedOp->GetNameOfOperation() );
			}
	#else	//	__BANK
			savegameDisplayf("CSavegameQueue::Update - popping operation of type %d", pQueuedOp->GetOperationType());
	#endif	//	__BANK
#endif	//	!__FINAL

			savegameAssertf(pQueuedOp->GetStatus() != MEM_CARD_BUSY, "CSavegameQueue::Update - operation status should be COMPLETE or ERROR at this stage");
			sm_QueueOfOperations.Drop();
		}
	}

	CSavegameDialogScreens::AfterUpdatingTheSavegameQueue();
}


#if __BANK
void CSavegameQueue::InitWidgets(bkBank *pParentBank)
{
	char WidgetTitle[32];

	pParentBank->PushGroup("Savegame Queue");

		ms_SavegameQueueCount = 0;
		pParentBank->AddSlider("Number of entries in queue (read-only)", &ms_SavegameQueueCount, 0, MAX_SIZE_OF_SAVEGAME_QUEUE, 1);

		for (u32 loop = 0; loop < MAX_SIZE_OF_SAVEGAME_QUEUE; loop++)
		{
			formatf(WidgetTitle, "Widget Index %u", loop);
			safecpy(ms_SavegameQueueDebugDescriptions[loop], "");
			pParentBank->AddText(WidgetTitle, ms_SavegameQueueDebugDescriptions[loop], MAX_LENGTH_OF_SAVEGAME_QUEUE_DEBUG_DESCRIPTION, true);
		}
		
	pParentBank->PopGroup();
}

void CSavegameQueue::UpdateWidgets()
{
	ms_SavegameQueueCount = sm_QueueOfOperations.GetCount();

	for (s32 loop = 0; loop < MAX_SIZE_OF_SAVEGAME_QUEUE; loop++)
	{
		safecpy(ms_SavegameQueueDebugDescriptions[loop], "");
		if (loop < ms_SavegameQueueCount)
		{
			if (savegameVerifyf(sm_QueueOfOperations[loop], "CSavegameQueue::UpdateWidgets - expected element %d (relative to tail) to have a valid entry", loop))
			{
				safecpy(ms_SavegameQueueDebugDescriptions[loop], sm_QueueOfOperations[loop]->GetNameOfOperation());
			}
		}
	}
}
#endif	//	__BANK

