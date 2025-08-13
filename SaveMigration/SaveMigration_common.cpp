#include "SaveMigration_common.h"

// Rage headers
#include "rline/savemigration/rlsavemigration.h"

//////////////////////////////////////////////////////////////////////////

RAGE_DEFINE_CHANNEL(savemigration)

//////////////////////////////////////////////////////////////////////////

bool CSaveMigrationTask::IsActive() const
{
	return (m_taskState > SM_TASK_NONE) && (m_taskState < SM_TASK_DONE);
}

bool CSaveMigrationTask::Start()
{
	if (!IsActive())
	{
		m_taskState = SM_TASK_START;
		m_status = SM_STATUS_PENDING;
	}
	return m_taskState == SM_TASK_START;
}

void CSaveMigrationTask::Update()
{
	if (m_taskState == SM_TASK_START)
	{
		if (m_netStatus.GetStatus() == rage::netStatus::NET_STATUS_NONE)
		{
			if (OnConfigure())
			{
				m_taskState = SM_TASK_PENDING;
			}
			else
			{
				m_taskState = SM_TASK_DONE;
				m_status = SM_STATUS_ERROR;
				m_netStatus.Reset();

				OnFailed();
			}
		}
	}
	else if (m_taskState == SM_TASK_PENDING)
	{
		OnWait();
	}
}

void CSaveMigrationTask::OnWait()
{
	if (m_netStatus.GetStatus() != rage::netStatus::NET_STATUS_PENDING)
	{
		switch (m_netStatus.GetStatus())
		{
		case rage::netStatus::NET_STATUS_FAILED:
			m_status = SM_STATUS_ERROR;
			OnFailed();
			break;
		case rage::netStatus::NET_STATUS_CANCELED:
			m_status = SM_STATUS_CANCELED;
			OnCanceled();
			break;
		default:
			OnCompleted();
			break;
		}
		m_netStatus.Reset();
		m_taskState = SM_TASK_DONE;
	}
}

void CSaveMigrationTask::Cancel()
{
	if (m_netStatus.GetStatus() == rage::netStatus::NET_STATUS_PENDING)
	{
		rlSaveMigration::Cancel(&m_netStatus);
	}
}

void CSaveMigrationTask::Reset()
{
	m_status = SM_STATUS_NONE;
	m_taskState = SM_TASK_NONE;
	m_netStatus.Reset();
}

//////////////////////////////////////////////////////////////////////////

const char* GetSaveMigrationStatusChar(eSaveMigrationStatus status)
{
	switch (status)
	{
	case SM_STATUS_OK:
		return "OK";
	case SM_STATUS_PENDING:
		return "Pending";
	case SM_STATUS_INPROGRESS:
		return "In Progress";
	case SM_STATUS_CANCELED:
		return "Canceled";
	case SM_STATUS_ROLLEDBACK:
		return "Rolled Back";
	case SM_STATUS_ERROR:
		return "Error";
	default:
		return "None";
		break;
	}
}