
#ifndef SAVEGAME_DELETE_H
#define SAVEGAME_DELETE_H

#include "Network/Cloud/CloudManager.h"
#include "SaveLoad/savegame_defines.h"


class CloudFileDeleter : public CloudListener
{
public:
	CloudFileDeleter();

	virtual ~CloudFileDeleter() {}

	bool RequestDeletionOfCloudFile(const char *pPathToFileToDelete);

	virtual void OnCloudEvent(const CloudEvent* pEvent);

	bool GetIsRequestPending() { return m_bIsRequestPending; }
	bool GetLastDeletionSucceeded() { return m_bLastDeletionSucceeded; }

private:
	void Initialise();

	// Private data
	CloudRequestID m_CloudRequestId;
	bool m_bIsRequestPending;
	bool m_bLastDeletionSucceeded;
};


enum eSavegameDeleteState
{
	SAVEGAME_DELETE_DO_NOTHING = 0,
	SAVEGAME_DELETE_BEGIN_INITIAL_CHECKS,
	SAVEGAME_DELETE_INITIAL_CHECKS,
	SAVEGAME_DELETE_BEGIN_LOCAL_DELETE,
	SAVEGAME_DELETE_DO_LOCAL_DELETE,
	SAVEGAME_DELETE_BEGIN_CLOUD_DELETE,
	SAVEGAME_DELETE_DO_CLOUD_DELETE,
//	SAVEGAME_DELETE_WAIT_FOR_DELETING_MESSAGE_TO_FINISH,
	SAVEGAME_DELETE_HANDLE_ERRORS
};


class CSavegameDelete
{
public:
	//	Public functions
	static void Init(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);

	static bool Begin(const char *pFilename, bool bDeleteFromCloud, bool bIsAPhoto);
	static MemoryCardError Update();

	static void SetDeleteStatus(eSavegameDeleteState DeleteStatus) { ms_DeleteStatus = DeleteStatus; }
	static eSavegameDeleteState GetDeleteStatus() { return ms_DeleteStatus; }

private:
	//	Private data
	static CloudFileDeleter *ms_pCloudFileDeleter;

	static eSavegameDeleteState	ms_DeleteStatus;

	static bool ms_bDeleteFromCloud;
#if RSG_ORBIS
	static bool ms_bDeleteMount;			//	On PS4, photos are all stored in a single mount directory. Other file types (e.g. savegames) are stored as one file per mount directory.
#endif	//	RSG_ORBIS

	//	Private functions
	static void EndDelete();
};

#endif	//	SAVEGAME_DELETE_H

