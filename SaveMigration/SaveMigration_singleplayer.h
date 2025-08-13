#ifndef SAVEMIGRATION_SINGLEPLAYER_H
#define SAVEMIGRATION_SINGLEPLAYER_H

// Rage headers
#include "atl/array.h"
#include "atl/vector.h"
#include "rline/savemigration/rlsavemigrationcommon.h"

// Framework headers
#include "fwutil/flags.h"
#include "fwutil/Gen9Settings.h"

// Game headers
#include "SaveMigration/savemigration_common.h"


// Forward declarations
#if __BANK
namespace rage
{
	class bkGroup;
	class bkBank;
}
#endif // __BANK


class CSaveMigrationGetSingleplayerSaveState
	: public CSaveMigrationTask
{
public:
	CSaveMigrationGetSingleplayerSaveState();
	virtual ~CSaveMigrationGetSingleplayerSaveState() {}

	const rlSaveMigrationRecordMetadata& GetRecordMetadata() const { return m_recordMetadata; };
	s32 GetErrorCode() const { return m_ErrorCode; }

protected:
	virtual bool OnConfigure() override;
	virtual void OnCompleted() override;

	virtual void OnFailed() override;
	virtual void OnCanceled() override;

private:
	rlSaveMigrationRecordMetadata m_recordMetadata;
	s32 m_ErrorCode;
};

//////////////////////////////////////////////////////////////////////////

#if GEN9_STANDALONE_ENABLED

class CSaveMigrationGetAvailableSingleplayerSaves
	: public CSaveMigrationTask
{
public:
#if IS_GEN9_PLATFORM
	typedef atVector<rlSaveMigrationRecordMetadata> MigrationRecordCollection;
#else
	typedef atArray<rlSaveMigrationRecordMetadata> MigrationRecordCollection;
#endif

	CSaveMigrationGetAvailableSingleplayerSaves();
	virtual ~CSaveMigrationGetAvailableSingleplayerSaves() {}

	const MigrationRecordCollection& GetRecordMetadata() const { return m_recordMetadata; };
	s32 GetErrorCode() const { return m_ErrorCode; }

protected:
	//	virtual void OnWait() override;		// I'm not sure if I need this one yet
	virtual bool OnConfigure() override;
	virtual void OnCompleted() override;

	virtual void OnFailed() override;
	virtual void OnCanceled() override;

private:
	MigrationRecordCollection m_recordMetadata;

	s32 m_ErrorCode;
};

//////////////////////////////////////////////////////////////////////////

class CSaveMigrationDownloadSingleplayerSave : public CSaveMigrationTask
{
public:
	CSaveMigrationDownloadSingleplayerSave();
	virtual ~CSaveMigrationDownloadSingleplayerSave() {}

	bool Setup(const rlSaveMigrationRecordMetadata& recordMetadata);

	const rlSaveMigrationRecordMetadata& GetRecordMetadata() const { return m_recordMetadata; };
	s32 GetErrorCode() const { return m_ErrorCode; }

protected:
	virtual bool OnConfigure() override;
	virtual void OnCompleted() override;

	virtual void OnFailed() override;
	virtual void OnCanceled() override;

private:
	rlSaveMigrationRecordMetadata m_recordMetadata;

	s32 m_ErrorCode;
};

//////////////////////////////////////////////////////////////////////////

class CSaveMigrationCompleteSingleplayerMigration : public CSaveMigrationTask
{
public:
	CSaveMigrationCompleteSingleplayerMigration();
	virtual ~CSaveMigrationCompleteSingleplayerMigration() {}

	bool Setup(const rlSaveMigrationRecordMetadata& recordMetadata);

	const rlSaveMigrationRecordMetadata& GetRecordMetadata() const { return m_recordMetadata; };
	s32 GetErrorCode() const { return m_ErrorCode; }

protected:
	virtual bool OnConfigure() override;
	virtual void OnCompleted() override;

	virtual void OnFailed() override;
	virtual void OnCanceled() override;

private:
	rlSaveMigrationRecordMetadata m_recordMetadata;

	s32 m_ErrorCode;
};

#endif // GEN9_STANDALONE_ENABLED

//////////////////////////////////////////////////////////////////////////

class CSaveMigrationUploadSingleplayerSave
	: public CSaveMigrationTask
{
public:
	CSaveMigrationUploadSingleplayerSave();
	virtual ~CSaveMigrationUploadSingleplayerSave() {}

	bool Setup(const u32 savePosixTime, const float completionPercentage, const u32 lastCompletedMissionId, const u8* pSavegameDataBuffer, const unsigned sizeOfSavegameDataBuffer);

	s32 GetErrorCode() const { return m_ErrorCode; }

protected:
//	virtual void OnWait() override;		// I'm not sure if I need this one yet
	virtual bool OnConfigure() override;
	virtual void OnCompleted() override;

	virtual void OnFailed() override;
	virtual void OnCanceled() override;

private:
	u32 m_SavePosixTime;
	float m_fCompletionPercentage;
	u32 m_LastCompletedMissionId;
	const u8* m_pSavegameDataBuffer;
	unsigned m_SizeOfSavegameDataBuffer;

	s32 m_ErrorCode;
};


//////////////////////////////////////////////////////////////////////////

class CSaveMigration_singleplayer
{
public:
	CSaveMigration_singleplayer();
	~CSaveMigration_singleplayer();

	void Init();
	void Shutdown();
	void Update();

//////////////////////////////////////////////////////////////////////////

	// PURPOSE:
	// Begin a cloud query to check if a single player savegame has already been uploaded or fully migrated
	bool RequestSinglePlayerSaveState();

	// PURPOSE:
	// Get the status of the single player save state query
	eSaveMigrationStatus GetSinglePlayerSaveStateStatus() const;

	// PURPOSE:
	// Get the result of querying if a single player savegame has already been uploaded or fully migrated
	s32 GetSinglePlayerSaveStateResult() const;

	// PURPOSE:
	// Get the details of the single player savegame that has been uploaded but not yet fully migrated to gen9
	const rlSaveMigrationRecordMetadata& GetSinglePlayerSaveStateDetails() const;

//////////////////////////////////////////////////////////////////////////

	// PURPOSE:
	// Begin uploading a single player savegame
	bool RequestUploadSingleplayerSave(const u32 savePosixTime, const float completionPercentage, const u32 lastCompletedMissionId, 
		const u8* pSavegameDataBuffer, const unsigned sizeOfSavegameDataBuffer);

	// PURPOSE:
	// Get the status of the single player save upload
	eSaveMigrationStatus GetUploadSingleplayerSaveStatus() const;

	// PURPOSE:
	// Get the result of of the single player save upload
	s32 GetUploadSingleplayerSaveResult() const;

//////////////////////////////////////////////////////////////////////////

	bool RequestAvailableSingleplayerSaves();
	bool HasCompletedRequestAvailableSingleplayerSaves() const;

#if GEN9_STANDALONE_ENABLED
	const CSaveMigrationGetAvailableSingleplayerSaves::MigrationRecordCollection& GetAvailableSingleplayerSaves() const;

	void SetChosenSingleplayerSave(const rlSaveMigrationRecordMetadata& recordMetadata);
	const rlSaveMigrationRecordMetadata& GetChosenSingleplayerSave() const { return m_chosenRecordMetadata; }
	bool RequestDownloadSingleplayerSave(const rlSaveMigrationRecordMetadata& recordMetadata);
	bool HasCompletedRequestDownloadSingleplayerSave() const;

	bool RequestCompleteSingleplayerMigration(const rlSaveMigrationRecordMetadata& recordMetadata);
	bool HasCompletedRequestCompleteSingleplayerMigration() const;

	void SetShouldMarkSingleplayerMigrationAsComplete();
#endif // GEN9_STANDALONE_ENABLED

//////////////////////////////////////////////////////////////////////////

	// PURPOSE:
	// Cancel any in flight transactions
	void Cancel();

#if __BANK
	void Bank_InitWidgets(bkBank* bank);
	void Bank_ShutdownWidgets(bkBank* bank);
	void Bank_Update();
#endif 

private:

#if __BANK
	void Bank_CreateWidgets();

	void Bank_RequestSinglePlayerSaveState();
#endif 

private:
	CSaveMigrationGetSingleplayerSaveState		m_getSaveStateTask;
	CSaveMigrationUploadSingleplayerSave		m_uploadSingleplayerSaveTask;

#if GEN9_STANDALONE_ENABLED
	CSaveMigrationGetAvailableSingleplayerSaves	m_getAvailableSingleplayerSavesTask;
	CSaveMigrationDownloadSingleplayerSave	m_downloadSingleplayerSaveTask;
	CSaveMigrationCompleteSingleplayerMigration m_completeSingleplayerMigrationTask;

	rlSaveMigrationRecordMetadata m_chosenRecordMetadata;

	bool m_shouldMarkSingleplayerMigrationAsComplete;
#endif // GEN9_STANDALONE_ENABLED

#if __BANK
	fwFlags32	m_BkRequest;
	bkGroup*	m_BkGroup;
	bool		m_bBankCreated;

	bool m_bStartRequestSinglePlayerSaveState;	// I can set this to trigger RequestSinglePlayerSaveState() when Rag isn't available

	char		m_getSpSaveStateStatus[32];
	int			m_getSpSaveStateResult;
	u32			m_getSpSaveStateLastCompletedMissionId;
#endif // __BANK
};


#endif // SAVEMIGRATION_SINGLEPLAYER_H
