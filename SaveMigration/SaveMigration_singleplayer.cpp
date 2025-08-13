#include "SaveMigration_singleplayer.h"

// Rage headers
#include "rline/savemigration/rlsavemigration.h"

// Game headers
#include "SaveLoad/GenericGameStorage.h"
#include "Network/NetworkInterface.h"
#include "Network/Live/NetworkTelemetry.h"

#if __BANK
#include "bank/bkmgr.h"
#include "bank/bank.h"
#include "bank/title.h"
#include "data/callback.h"
#include "system/bit.h"
#endif // __BANK

SAVEGAME_OPTIMISATIONS()

#if __BANK
enum eSinglePlayerSaveMigrationBankRequestsFlags
{
	SPSMBR_NONE					= BIT0,
	SPSMBR_GET_SP_SAVE_STATE	= BIT1,
};
#endif // __BANK


//////////////////////////////////////////////////////////////////////////

#if GEN9_STANDALONE_ENABLED

CSaveMigrationGetAvailableSingleplayerSaves::CSaveMigrationGetAvailableSingleplayerSaves() :
	m_ErrorCode(0)
{
}

bool CSaveMigrationGetAvailableSingleplayerSaves::OnConfigure()
{
	m_recordMetadata.Reset();

	savemigrationDebug1("CSaveMigrationGetAvailableSingleplayerSaves::OnConfigure : localGamerIndex %d", NetworkInterface::GetLocalGamerIndex());
	return rlSaveMigration::GetAvailableSingleplayerSaves(NetworkInterface::GetLocalGamerIndex(), &m_recordMetadata, &m_netStatus);
}

void CSaveMigrationGetAvailableSingleplayerSaves::OnCompleted()
{
	m_status = SM_STATUS_OK;

	savemigrationDebug1("CSaveMigrationGetAvailableSingleplayerSaves::OnCompleted : localGamerIndex %d Found %d available saves", NetworkInterface::GetLocalGamerIndex(), m_recordMetadata.GetCount());
}

void CSaveMigrationGetAvailableSingleplayerSaves::OnFailed()
{
	m_ErrorCode = m_netStatus.GetResultCode();

	m_status = SM_STATUS_ERROR;

	savemigrationDebug1("CSaveMigrationGetAvailableSingleplayerSaves::OnFailed : localGamerIndex %d Result Code %d", NetworkInterface::GetLocalGamerIndex(), m_ErrorCode);
}

void CSaveMigrationGetAvailableSingleplayerSaves::OnCanceled()
{
	savemigrationDebug1("CSaveMigrationGetAvailableSingleplayerSaves::OnCanceled : localGamerIndex %d", NetworkInterface::GetLocalGamerIndex());
}

//////////////////////////////////////////////////////////////////////////

CSaveMigrationDownloadSingleplayerSave::CSaveMigrationDownloadSingleplayerSave() :
	m_recordMetadata(),
	m_ErrorCode(0)
{
}

bool CSaveMigrationDownloadSingleplayerSave::Setup(const rlSaveMigrationRecordMetadata& recordMetadata)
{
	if (!IsActive())
	{
		savemigrationDebug1("CSaveMigrationUploadSingleplayerSave::Setup");
		m_recordMetadata = recordMetadata;
		return true;
	}

	savemigrationDebug1("CSaveMigrationUploadSingleplayerSave::Setup failed - already Active");
	return false;
}

bool CSaveMigrationDownloadSingleplayerSave::OnConfigure()
{
	auto ProcessResponseFunc = [](const void* pData, const unsigned nDataSize)
	{
#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
		CGenericGameStorage::RequestImportSinglePlayerSavegameIntoFixedSlot((const u8 *)pData, nDataSize);
#else
		(void)pData;
		(void)nDataSize;
#endif // __ALLOW_IMPORT_OF_SP_SAVEGAMES && __BANK
	};

	savemigrationDebug1("CSaveMigrationGetAvailableSingleplayerSaves::OnConfigure : localGamerIndex %d", NetworkInterface::GetLocalGamerIndex());
	return rlSaveMigration::DownloadSingleplayerSave(NetworkInterface::GetLocalGamerIndex(), m_recordMetadata, ProcessResponseFunc, &m_netStatus);
}

void CSaveMigrationDownloadSingleplayerSave::OnCompleted()
{
	m_status = SM_STATUS_OK;

	savemigrationDebug1("CSaveMigrationDownloadSingleplayerSave::OnCompleted : localGamerIndex %d Mission Name Hash %u Completion Percentage %.1f%%", NetworkInterface::GetLocalGamerIndex(), m_recordMetadata.m_LastCompletedMissionId, m_recordMetadata.m_CompletionPercentage);
}

void CSaveMigrationDownloadSingleplayerSave::OnFailed()
{
	m_ErrorCode = m_netStatus.GetResultCode();

	m_status = SM_STATUS_ERROR;

	savemigrationDebug1("CSaveMigrationDownloadSingleplayerSave::OnFailed : localGamerIndex %d Result Code %d", NetworkInterface::GetLocalGamerIndex(), m_ErrorCode);
}

void CSaveMigrationDownloadSingleplayerSave::OnCanceled()
{
	savemigrationDebug1("CSaveMigrationDownloadSingleplayerSave::OnCanceled : localGamerIndex %d", NetworkInterface::GetLocalGamerIndex());
}

//////////////////////////////////////////////////////////////////////////

CSaveMigrationCompleteSingleplayerMigration::CSaveMigrationCompleteSingleplayerMigration() :
	m_recordMetadata(),
	m_ErrorCode(0)
{
}

bool CSaveMigrationCompleteSingleplayerMigration::Setup(const rlSaveMigrationRecordMetadata& recordMetadata)
{
	if (!IsActive())
	{
		savemigrationDebug1("CSaveMigrationUploadSingleplayerSave::Setup");
		m_recordMetadata = recordMetadata;
		return true;
	}

	savemigrationDebug1("CSaveMigrationUploadSingleplayerSave::Setup failed - already Active");
	return false;
}

bool CSaveMigrationCompleteSingleplayerMigration::OnConfigure()
{
	savemigrationDebug1("CSaveMigrationGetAvailableSingleplayerSaves::OnConfigure : localGamerIndex %d", NetworkInterface::GetLocalGamerIndex());
#if IS_GEN9_PLATFORM
	return rlSaveMigration::CompleteSingleplayerMigration(NetworkInterface::GetLocalGamerIndex(), m_recordMetadata.m_Token.c_str(), &m_netStatus);
#else
	return rlSaveMigration::CompleteSingleplayerMigration(NetworkInterface::GetLocalGamerIndex(), &m_netStatus);
#endif // IS_GEN9_PLATFORM
}

void CSaveMigrationCompleteSingleplayerMigration::OnCompleted()
{
	m_status = SM_STATUS_OK;

	savemigrationDebug1("CSaveMigrationDownloadSingleplayerSave::OnCompleted : localGamerIndex %d Mission Name Hash %u Completion Percentage %.1f%%", NetworkInterface::GetLocalGamerIndex(), m_recordMetadata.m_LastCompletedMissionId, m_recordMetadata.m_CompletionPercentage);
}

void CSaveMigrationCompleteSingleplayerMigration::OnFailed()
{
	m_ErrorCode = m_netStatus.GetResultCode();

	m_status = SM_STATUS_ERROR;

	savemigrationDebug1("CSaveMigrationDownloadSingleplayerSave::OnFailed : localGamerIndex %d Result Code %d", NetworkInterface::GetLocalGamerIndex(), m_ErrorCode);
}

void CSaveMigrationCompleteSingleplayerMigration::OnCanceled()
{
	savemigrationDebug1("CSaveMigrationDownloadSingleplayerSave::OnCanceled : localGamerIndex %d", NetworkInterface::GetLocalGamerIndex());
}

#endif // GEN9_STANDALONE_ENABLED

//////////////////////////////////////////////////////////////////////////

CSaveMigrationGetSingleplayerSaveState::CSaveMigrationGetSingleplayerSaveState()
	: m_recordMetadata(),
	m_ErrorCode(0)
{}

bool CSaveMigrationGetSingleplayerSaveState::OnConfigure()
{
	m_recordMetadata.Clear();

	savemigrationDebug1("CSaveMigrationGetSingleplayerSaveState::OnConfigure : localGamerIndex %d", NetworkInterface::GetLocalGamerIndex());
	return rlSaveMigration::GetSingleplayerSaveState(NetworkInterface::GetLocalGamerIndex(), &m_recordMetadata, &m_netStatus);
}

void CSaveMigrationGetSingleplayerSaveState::OnCompleted()
{
	m_status = SM_STATUS_OK;

	savemigrationDebug1("CSaveMigrationGetSingleplayerSaveState::OnCompleted : localGamerIndex %d Mission Name Hash %u Completion Percentage %.1f%%", NetworkInterface::GetLocalGamerIndex(), m_recordMetadata.m_LastCompletedMissionId, m_recordMetadata.m_CompletionPercentage);
}

void CSaveMigrationGetSingleplayerSaveState::OnFailed()
{
	m_ErrorCode = m_netStatus.GetResultCode();

	//Result Code is 'RLSM_ERROR_NO_MIGRATION_RECORDS', we can upload a savegame
	if (m_ErrorCode == (s32)RLSM_ERROR_NO_MIGRATION_RECORDS)
	{
		savemigrationDebug1("CSaveMigrationGetSingleplayerSaveState::OnFailed : localGamerIndex %d Result Code is 'RLSM_ERROR_NO_MIGRATION_RECORDS', we can upload a savegame.", NetworkInterface::GetLocalGamerIndex());
		m_status = SM_STATUS_OK;
		return;
	}

	savemigrationDebug1("CSaveMigrationGetSingleplayerSaveState::OnFailed : localGamerIndex %d Result Code %d", NetworkInterface::GetLocalGamerIndex(), m_ErrorCode);
}

void CSaveMigrationGetSingleplayerSaveState::OnCanceled()
{
	savemigrationDebug1("CSaveMigrationGetSingleplayerSaveState::OnCanceled : localGamerIndex %d", NetworkInterface::GetLocalGamerIndex());
}

//////////////////////////////////////////////////////////////////////////

CSaveMigrationUploadSingleplayerSave::CSaveMigrationUploadSingleplayerSave()
	: m_SavePosixTime(0),
	m_fCompletionPercentage(0.0f),
	m_LastCompletedMissionId(0),
	m_pSavegameDataBuffer(NULL),
	m_SizeOfSavegameDataBuffer(0),
	m_ErrorCode(0)
{}

bool CSaveMigrationUploadSingleplayerSave::Setup(const u32 savePosixTime, const float completionPercentage, const u32 lastCompletedMissionId, const u8* pSavegameDataBuffer, const unsigned sizeOfSavegameDataBuffer)
{
	if (!IsActive())
	{
		savemigrationDebug1("CSaveMigrationUploadSingleplayerSave::Setup");
		m_SavePosixTime = savePosixTime;
		m_fCompletionPercentage = completionPercentage;
		m_LastCompletedMissionId = lastCompletedMissionId;
		m_pSavegameDataBuffer = pSavegameDataBuffer;
		m_SizeOfSavegameDataBuffer = sizeOfSavegameDataBuffer;
		return true;
	}

	savemigrationDebug1("CSaveMigrationUploadSingleplayerSave::Setup failed - already Active");
	return false;
}

bool CSaveMigrationUploadSingleplayerSave::OnConfigure()
{
	savemigrationDebug1("CSaveMigrationUploadSingleplayerSave::OnConfigure : localGamerIndex=%d SaveTime=%u CompletionPercentage=%.1f MissionHash=%u BufferSize=%u", 
		NetworkInterface::GetLocalGamerIndex(), m_SavePosixTime, m_fCompletionPercentage, m_LastCompletedMissionId, m_SizeOfSavegameDataBuffer);

	return rlSaveMigration::UploadSingleplayerSave(NetworkInterface::GetLocalGamerIndex(), 
		m_SavePosixTime, m_fCompletionPercentage, m_LastCompletedMissionId, 
		m_pSavegameDataBuffer, m_SizeOfSavegameDataBuffer, 
		&m_netStatus);
}

void CSaveMigrationUploadSingleplayerSave::OnCompleted()
{
	m_status = SM_STATUS_OK;

	savemigrationDebug1("CSaveMigrationUploadSingleplayerSave::OnCompleted : localGamerIndex %d", NetworkInterface::GetLocalGamerIndex());

	CNetworkTelemetry::AppendSinglePlayerSaveMigration(true, m_SavePosixTime, m_fCompletionPercentage, m_LastCompletedMissionId, m_SizeOfSavegameDataBuffer);
}

void CSaveMigrationUploadSingleplayerSave::OnFailed()
{
	savemigrationDebug1("CSaveMigrationUploadSingleplayerSave::OnFailed : localGamerIndex %d Result Code %d", NetworkInterface::GetLocalGamerIndex(), m_netStatus.GetResultCode() );
	m_ErrorCode = m_netStatus.GetResultCode();

	CNetworkTelemetry::AppendSinglePlayerSaveMigration(false, m_SavePosixTime, m_fCompletionPercentage, m_LastCompletedMissionId, m_SizeOfSavegameDataBuffer);
}

void CSaveMigrationUploadSingleplayerSave::OnCanceled()
{
	savemigrationDebug1("CSaveMigrationUploadSingleplayerSave::OnCanceled : localGamerIndex %d", NetworkInterface::GetLocalGamerIndex());
}

//////////////////////////////////////////////////////////////////////////

CSaveMigration_singleplayer::CSaveMigration_singleplayer() :
	m_getSaveStateTask(),
	m_uploadSingleplayerSaveTask()
#if GEN9_STANDALONE_ENABLED
	,
	m_shouldMarkSingleplayerMigrationAsComplete(false)
#endif // GEN9_STANDALONE_ENABLED
{
#if __BANK
	m_BkRequest = SPSMBR_NONE;
	m_BkGroup = nullptr;
	m_bBankCreated = false;

	m_bStartRequestSinglePlayerSaveState = false;
#endif
}

CSaveMigration_singleplayer::~CSaveMigration_singleplayer()
{
}

void CSaveMigration_singleplayer::Init()
{
}

void CSaveMigration_singleplayer::Shutdown()
{
	Cancel();

	m_getSaveStateTask.Reset();
	m_uploadSingleplayerSaveTask.Reset();

#if GEN9_STANDALONE_ENABLED
	m_getAvailableSingleplayerSavesTask.Reset();
	m_downloadSingleplayerSaveTask.Reset();
	m_completeSingleplayerMigrationTask.Reset();
#endif // GEN9_STANDALONE_ENABLED
}

void CSaveMigration_singleplayer::Update()
{
	if (m_getSaveStateTask.IsActive())
	{
		m_getSaveStateTask.Update();
	}

	if (m_uploadSingleplayerSaveTask.IsActive())
	{
		m_uploadSingleplayerSaveTask.Update();
	}

#if GEN9_STANDALONE_ENABLED
	// Initiate CompleteSingleplayerMigration task if flag has been set
	if (m_shouldMarkSingleplayerMigrationAsComplete)
	{
		RequestCompleteSingleplayerMigration(m_chosenRecordMetadata);
		m_shouldMarkSingleplayerMigrationAsComplete = false;
	}

	if (m_getAvailableSingleplayerSavesTask.IsActive())
	{
		m_getAvailableSingleplayerSavesTask.Update();
	}

	if (m_downloadSingleplayerSaveTask.IsActive())
	{
		m_downloadSingleplayerSaveTask.Update();
	}

	if (m_completeSingleplayerMigrationTask.IsActive())
	{
		m_completeSingleplayerMigrationTask.Update();
	}
#endif // GEN9_STANDALONE_ENABLED
}

//////////////////////////////////////////////////////////////////////////

bool CSaveMigration_singleplayer::RequestSinglePlayerSaveState()
{
	if (NetworkInterface::IsMigrationAvailable())
	{
		if (m_getSaveStateTask.Start())
		{
			return true;
		}
	}
	return false;
}

eSaveMigrationStatus CSaveMigration_singleplayer::GetSinglePlayerSaveStateStatus() const
{
	return m_getSaveStateTask.GetStatus();
}

s32 CSaveMigration_singleplayer::GetSinglePlayerSaveStateResult() const
{
	return m_getSaveStateTask.GetErrorCode();
}

const rlSaveMigrationRecordMetadata& CSaveMigration_singleplayer::GetSinglePlayerSaveStateDetails() const
{
	return m_getSaveStateTask.GetRecordMetadata();
}

//////////////////////////////////////////////////////////////////////////

bool CSaveMigration_singleplayer::RequestUploadSingleplayerSave(const u32 savePosixTime, const float completionPercentage, const u32 lastCompletedMissionId, 
							  const u8* pSavegameDataBuffer, const unsigned sizeOfSavegameDataBuffer)
{
	if (NetworkInterface::IsMigrationAvailable())
	{
		if (m_uploadSingleplayerSaveTask.Setup(savePosixTime, completionPercentage, lastCompletedMissionId, pSavegameDataBuffer, sizeOfSavegameDataBuffer)
			&& m_uploadSingleplayerSaveTask.Start())
		{
			return true;
		}
	}
	return false;
}

eSaveMigrationStatus CSaveMigration_singleplayer::GetUploadSingleplayerSaveStatus() const
{
	return m_uploadSingleplayerSaveTask.GetStatus();
}

s32 CSaveMigration_singleplayer::GetUploadSingleplayerSaveResult() const
{
	return m_uploadSingleplayerSaveTask.GetErrorCode();
}

//////////////////////////////////////////////////////////////////////////

#if GEN9_STANDALONE_ENABLED
bool CSaveMigration_singleplayer::RequestAvailableSingleplayerSaves()
{
	const bool c_startTaskSuccessful = m_getAvailableSingleplayerSavesTask.Start();
	return c_startTaskSuccessful;
}

bool CSaveMigration_singleplayer::HasCompletedRequestAvailableSingleplayerSaves() const
{
	return !m_getAvailableSingleplayerSavesTask.IsActive() && m_getAvailableSingleplayerSavesTask.GetStatus() != SM_STATUS_NONE;
}

const CSaveMigrationGetAvailableSingleplayerSaves::MigrationRecordCollection& CSaveMigration_singleplayer::GetAvailableSingleplayerSaves() const
{
	return m_getAvailableSingleplayerSavesTask.GetRecordMetadata();
}

bool CSaveMigration_singleplayer::RequestDownloadSingleplayerSave(const rlSaveMigrationRecordMetadata& recordMetadata)
{
	const bool c_startTaskSuccessful = m_downloadSingleplayerSaveTask.Setup(recordMetadata)
		&& m_downloadSingleplayerSaveTask.Start();
	return c_startTaskSuccessful;
}

bool CSaveMigration_singleplayer::HasCompletedRequestDownloadSingleplayerSave() const
{
	return !m_downloadSingleplayerSaveTask.IsActive() && m_downloadSingleplayerSaveTask.GetStatus() != SM_STATUS_NONE;
}

bool CSaveMigration_singleplayer::RequestCompleteSingleplayerMigration(const rlSaveMigrationRecordMetadata& recordMetadata)
{
	const bool c_startTaskSuccessful = m_completeSingleplayerMigrationTask.Setup(recordMetadata)
		&& m_completeSingleplayerMigrationTask.Start();
	return c_startTaskSuccessful;
}

bool CSaveMigration_singleplayer::HasCompletedRequestCompleteSingleplayerMigration() const
{
	return !m_completeSingleplayerMigrationTask.IsActive() && m_completeSingleplayerMigrationTask.GetStatus() != SM_STATUS_NONE;
}

void CSaveMigration_singleplayer::SetChosenSingleplayerSave(const rlSaveMigrationRecordMetadata& recordMetadata)
{
	m_chosenRecordMetadata = recordMetadata;
}

void CSaveMigration_singleplayer::SetShouldMarkSingleplayerMigrationAsComplete()
{
	m_shouldMarkSingleplayerMigrationAsComplete = true;
}
#endif // GEN9_STANDALONE_ENABLED

//////////////////////////////////////////////////////////////////////////

void CSaveMigration_singleplayer::Cancel()
{
	m_getSaveStateTask.Cancel();
	m_uploadSingleplayerSaveTask.Cancel();
}

#if __BANK
void CSaveMigration_singleplayer::Bank_InitWidgets(bkBank* bank)
{
	if (bank)
	{
		m_BkGroup = bank->PushGroup("Single Player", false);
		{
			bank->AddButton("Create SP widgets", datCallback(MFA(CSaveMigration_singleplayer::Bank_CreateWidgets), (datBase*)this));
		}
		bank->PopGroup();
	}
}

void CSaveMigration_singleplayer::Bank_ShutdownWidgets(bkBank* bank)
{
	if (bank && m_BkGroup)
	{
		bank->DeleteGroup(*m_BkGroup);
	}
	m_BkGroup = nullptr;
	m_bBankCreated = false;
}

void CSaveMigration_singleplayer::Bank_CreateWidgets()
{
	bkBank* bank = BANKMGR.FindBank("SaveMigration");
	if (bank && m_BkGroup)
	{
		bank->DeleteGroup(*m_BkGroup);
	}
	m_BkGroup = nullptr;
	m_bBankCreated = false;

	if ((!m_bBankCreated) && bank && !m_BkGroup)
	{
		m_bBankCreated = true;

		m_BkGroup = bank->PushGroup("Single Player", true);
		{
// 			bank->PushGroup("Overrides", true, "Lets you force the return result from the request.");
// 			{
// 				bank->AddToggle("Enable", &g_SavemigrationMPAccountStatusOverrideEnabled);
// 				bank->AddCombo("Account Status Result", &g_SavemigrationMPAccountStatusOverride, 6, &g_SavemigrationMPStatusOverrideNames[0], 0, NullCB, "");
// 				bank->AddSeparator();
// 			}
// 			bank->PopGroup();

// 			bank->AddSeparator();
 			bank->PushGroup("Tests", true, "Use to test the SP save migration API");
 			{
				bank->AddButton("Request SP Save State", datCallback(MFA(CSaveMigration_singleplayer::Bank_RequestSinglePlayerSaveState), (datBase*)this));
				{
					strcpy(m_getSpSaveStateStatus, "None");
					m_getSpSaveStateResult = 0;
					m_getSpSaveStateLastCompletedMissionId = 0;
					bank->AddText("Status", m_getSpSaveStateStatus, 32, true);
					bank->AddText("Result", &m_getSpSaveStateResult, false);
					bank->AddText("Mission Hash", (int*)&m_getSpSaveStateLastCompletedMissionId, false);
				}
 			}
 			bank->PopGroup();
 		}
 		bank->PopGroup();
	}
}

void CSaveMigration_singleplayer::Bank_Update()
{
	if (m_BkRequest.IsFlagSet(SPSMBR_GET_SP_SAVE_STATE))
	{
		strcpy(m_getSpSaveStateStatus, GetSaveMigrationStatusChar(GetSinglePlayerSaveStateStatus()));

		if (GetSinglePlayerSaveStateStatus() != SM_STATUS_PENDING)
		{
			m_getSpSaveStateResult = static_cast<int>(GetSinglePlayerSaveStateResult());

			if (GetSinglePlayerSaveStateStatus() == SM_STATUS_OK)
			{
				m_getSpSaveStateLastCompletedMissionId = GetSinglePlayerSaveStateDetails().m_LastCompletedMissionId;
			}

			savemigrationDebug1("Bank_RequestSinglePlayerSaveState : Completed : %s", m_getSpSaveStateStatus);
			m_BkRequest.ClearFlag(SPSMBR_GET_SP_SAVE_STATE);
		}
	}

	if (m_bStartRequestSinglePlayerSaveState) // I can set this to trigger RequestSinglePlayerSaveState() when Rag isn't available
	{
		RequestSinglePlayerSaveState();
		m_bStartRequestSinglePlayerSaveState = false;
	}
}

void CSaveMigration_singleplayer::Bank_RequestSinglePlayerSaveState()
{
	if (!m_BkRequest.IsFlagSet(SPSMBR_GET_SP_SAVE_STATE))
	{
		if (RequestSinglePlayerSaveState())
		{
			savemigrationDebug1("Bank_RequestSinglePlayerSaveState : Requested");

			strcpy(m_getSpSaveStateStatus, "Requested");
			m_BkRequest.SetFlag(SPSMBR_GET_SP_SAVE_STATE);
		}
		else
		{
			strcpy(m_getSpSaveStateStatus, "Failed");
		}
	}
}

#endif // __BANK
