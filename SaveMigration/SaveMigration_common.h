#ifndef SAVEMIGRATION_COMMON_H
#define SAVEMIGRATION_COMMON_H

// Rage headers

#include "diag/channel.h"
#include "net/status.h"

RAGE_DECLARE_CHANNEL(savemigration);

#define savemigrationDebug(fmt, ...)					RAGE_DEBUGF1(savemigration, fmt, ##__VA_ARGS__)
#define savemigrationDebug1(fmt, ...)					RAGE_DEBUGF1(savemigration, fmt, ##__VA_ARGS__)
#define savemigrationDebug2(fmt, ...)					RAGE_DEBUGF2(savemigration, fmt, ##__VA_ARGS__)
#define savemigrationDebug3(fmt, ...)					RAGE_DEBUGF3(savemigration, fmt, ##__VA_ARGS__)
#define savemigrationWarning(fmt, ...)					RAGE_WARNINGF(savemigration, fmt, ##__VA_ARGS__)
#define savemigrationError(fmt, ...)					RAGE_ERRORF(savemigration, fmt, ##__VA_ARGS__)
#define savemigrationCondLogf(cond, severity, fmt, ...)	RAGE_CONDLOGF(savemigration, cond, severity, fmt, ##__VA_ARGS__)
#define savemigrationVerify(cond)						RAGE_VERIFY(savemigration, cond)
#define savemigrationVerifyf(cond, fmt, ...)			RAGE_VERIFYF(savemigration, cond, fmt, ##__VA_ARGS__)
#define savemigrationAssert(cond) 						RAGE_ASSERT(savemigration, cond)
#define savemigrationAssertf(cond, fmt, ...) 			RAGE_ASSERTF(savemigration, cond, fmt, ##__VA_ARGS__)

#define SAVEMIGRATION_DEFAULT_POOL_TIME_MS				3000	// 3 seconds
#define SAVEMIGRATION_DEFAULT_MAX_POOL_TIME_MS			120000	// 2 mins
#define SAVEMIGRATION_DEFAULT_NUM_ATTEMPTS				4

enum eSaveMigrationStatus
{
	SM_STATUS_NONE = 0,
	SM_STATUS_OK,

	SM_STATUS_PENDING,

	SM_STATUS_INPROGRESS,
	SM_STATUS_CANCELED,
	SM_STATUS_ROLLEDBACK,
	SM_STATUS_ERROR,
};

enum eSaveMigrationTaskState
{
	SM_TASK_NONE = 0,
	SM_TASK_START,
	SM_TASK_PENDING,
	SM_TASK_DONE,
};

class CSaveMigrationTask
{
public:

	CSaveMigrationTask()
		: m_status(SM_STATUS_NONE)
		, m_taskState(SM_TASK_NONE)
		, m_netStatus()
	{}
	virtual ~CSaveMigrationTask() {}

	eSaveMigrationStatus GetStatus() const { return m_status; }

	bool IsActive() const;
	bool Start();
	void Update();
	void Cancel();
	void Reset();

protected:

	virtual void OnWait();

	virtual bool OnConfigure() = 0;
	virtual void OnCompleted() = 0;

	virtual void OnFailed() {};
	virtual void OnCanceled() {};

protected:

	eSaveMigrationStatus		m_status;
	eSaveMigrationTaskState		m_taskState;
	netStatus					m_netStatus;
};

extern const char* GetSaveMigrationStatusChar(eSaveMigrationStatus status);

#endif // SAVEMIGRATION_COMMON_H
