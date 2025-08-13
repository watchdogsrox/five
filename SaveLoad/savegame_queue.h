
#ifndef SAVEGAME_QUEUE_H
#define SAVEGAME_QUEUE_H


// Rage headers
#include "atl/queue.h"

// Game headers
#include "SaveLoad/savegame_defines.h"


// Forward declarations
class CSavegameQueuedOperation;

#if __BANK
namespace rage {
	class bkBank;
}
#endif	//	__BANK

class CSavegameQueue
{
public:
	//	Public functions
	static void Init(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);

	static void Update();

	static bool Push(CSavegameQueuedOperation *pNewOperation);	//	assert if this type already exists in the queue?

	static bool ContainsAnAutosave();
	static bool ContainsASinglePlayerSave();

	static bool IsEmpty() { return sm_QueueOfOperations.IsEmpty(); }

	static bool ExistsInQueue(CSavegameQueuedOperation *pOperationToCheck, bool &bAtTop);

	static void Remove(CSavegameQueuedOperation *pOperationToRemove);

#if __BANK
	static void InitWidgets(bkBank *pParentBank);
	static void UpdateWidgets();
#endif	//	__BANK

private:

	static const u32 MAX_SIZE_OF_SAVEGAME_QUEUE = 16;
	static atQueue<CSavegameQueuedOperation*, MAX_SIZE_OF_SAVEGAME_QUEUE> sm_QueueOfOperations;

#if __BANK
	#define MAX_LENGTH_OF_SAVEGAME_QUEUE_DEBUG_DESCRIPTION	(32)
	static char ms_SavegameQueueDebugDescriptions[MAX_SIZE_OF_SAVEGAME_QUEUE][MAX_LENGTH_OF_SAVEGAME_QUEUE_DEBUG_DESCRIPTION];
	static s32 ms_SavegameQueueCount;
#endif	//	__BANK
};

#endif	//	SAVEGAME_QUEUE_H

