#pragma once

#include "ai\task\task.h"
#include "scene\RegdRefTypes.h"

#define AI_DEBUG_OUTPUT_ENABLED __BANK	// TODO: We could have some file output in release builds

#if AI_DEBUG_OUTPUT_ENABLED

#define AI_DEBUG_OUTPUT_ONLY(...)  __VA_ARGS__
#else
#define AI_DEBUG_OUTPUT_ONLY(...)
#endif

#if ENABLE_SIMPLE_TASK_STATE_HISTORY_DEBUG
#define ENABLE_SIMPLE_TASK_STATE_HISTORY_DEBUG_ONLY(...)  __VA_ARGS__
#else
#define ENABLE_SIMPLE_TASK_STATE_HISTORY_DEBUG_ONLY(...)
#endif

#if AI_DEBUG_OUTPUT_ENABLED
#define AI_LOG_REJECTION_WITH_REASON(ENT_PTR, REASON) if (CAILogManager::IsLogInitialised()) AILogging::LogEntityRejectionWithReason(ENT_PTR, __FUNCTION__, REASON)
#define AI_LOG_ACCEPTANCE_WITH_REASON(ENT_PTR, REASON) if (CAILogManager::IsLogInitialised()) AILogging::LogEntityAcceptanceWithReason(ENT_PTR, __FUNCTION__, REASON)
#define AI_LOG_REJECTION_WITH_IDENTIFIER_AND_REASON(IDENTIFIER, ENT_PTR, REASON) if (CAILogManager::IsLogInitialised()) AILogging::LogEntityRejectionWithIdentifierAndReason(IDENTIFIER, ENT_PTR, REASON)
#define AI_LOG_ACCEPTANCE_WITH_IDENTIFIER_AND_REASON(IDENTIFIER, ENT_PTR, REASON) if (CAILogManager::IsLogInitialised()) AILogging::LogEntityAcceptanceWithIdentifierAndReason(IDENTIFIER, ENT_PTR, REASON)
#define AI_FUNCTION_LOG_WITH_ARGS(STR, ...) if (CAILogManager::IsLogInitialised()) AILogging::FunctionLog(__FUNCTION__, STR, __VA_ARGS__)
#define AI_FUNCTION_LOG(STR) if (CAILogManager::IsLogInitialised()) AILogging::FunctionLog(__FUNCTION__, STR)
#define AI_LOG(STR) if (CAILogManager::IsLogInitialised()) CAILogManager::GetLog().Log(STR)
#define AI_LOG_WITH_ARGS(STR, ...)  if (CAILogManager::IsLogInitialised()) CAILogManager::GetLog().Log(STR, __VA_ARGS__)
#define AI_LOG_IF_LOCAL_PLAYER(PED_PTR, STR)  if (PED_PTR->IsLocalPlayer() && CAILogManager::IsLogInitialised()) CAILogManager::GetLog().Log(STR)
#define AI_LOG_WITH_ARGS_IF_LOCAL_PLAYER(PED_PTR, STR, ...)  if (PED_PTR->IsLocalPlayer() && CAILogManager::IsLogInitialised()) CAILogManager::GetLog().Log(STR, __VA_ARGS__)
#define AI_LOG_IF_PLAYER(PED_PTR, STR, ...)  if (PED_PTR->IsAPlayerPed() && CAILogManager::IsLogInitialised()) CAILogManager::GetLog().Log(STR, __VA_ARGS__)
#define AI_LOG_IF_SCRIPT_PED(PED_PTR, STR)  if (PED_PTR->PopTypeIsMission() && CAILogManager::IsLogInitialised()) CAILogManager::GetLog().Log(STR)
#define AI_LOG_WITH_ARGS_IF_SCRIPT_PED(PED_PTR, STR, ...)  if (PED_PTR->PopTypeIsMission() && CAILogManager::IsLogInitialised()) CAILogManager::GetLog().Log(STR, __VA_ARGS__)
#define AI_LOG_WITH_ARGS_IF_SCRIPT_OR_PLAYER_PED(PED_PTR, STR, ...)  if ((PED_PTR->PopTypeIsMission() || PED_PTR->IsAPlayerPed()) && CAILogManager::IsLogInitialised()) CAILogManager::GetLog().Log(STR, __VA_ARGS__)
#define AI_LOG_WITH_ARGS_IF_SCRIPT_OR_PLAYER_PED_REF(PED_REF, STR, ...)  if ((PED_REF.PopTypeIsMission() || PED_REF.IsAPlayerPed()) && CAILogManager::IsLogInitialised()) CAILogManager::GetLog().Log(STR, __VA_ARGS__)
#define AI_LOG_STACK_TRACE(DEPTH) AILogging::StackTraceLog(DEPTH)
#define AI_LOG_STACK_TRACE_IF_SCRIPT_OR_PLAYER_PED(PED_PTR, DEPTH) if (PED_PTR && (PED_PTR->PopTypeIsMission() || PED_PTR->IsAPlayerPed()) && CAILogManager::IsLogInitialised())  AILogging::StackTraceLog(DEPTH)
#define AI_LOG_STACK_TRACE_IF_SCRIPT_OR_PLAYER_PED_REF(PED_REF, DEPTH) if ((PED_REF.PopTypeIsMission() || PED_REF.IsAPlayerPed()) && CAILogManager::IsLogInitialised())  AILogging::StackTraceLog(DEPTH)
#else // AI_DEBUG_OUTPUT_ENABLED
#define AI_LOG_REJECTION_WITH_REASON(ENT_PTR, REASON)
#define AI_LOG_ACCEPTANCE_WITH_REASON(ENT_PTR, REASON)
#define AI_LOG_REJECTION_WITH_IDENTIFIER_AND_REASON(IDENTIFIER, ENT_PTR, REASON)
#define AI_LOG_ACCEPTANCE_WITH_IDENTIFIER_AND_REASON(IDENTIFIER, ENT_PTR, REASON)
#define AI_FUNCTION_LOG_WITH_ARGS(STR, ...)
#define AI_FUNCTION_LOG(STR)
#define AI_LOG(STR)
#define AI_LOG_WITH_ARGS(STR, ...)
#define AI_LOG_IF_LOCAL_PLAYER(PED_PTR, STR)
#define AI_LOG_IF_PLAYER(PED_PTR, STR, ...)
#define AI_LOG_WITH_ARGS_IF_LOCAL_PLAYER(PED_PTR, STR, ...)
#define AI_LOG_IF_SCRIPT_PED(PED_PTR, STR) 
#define AI_LOG_WITH_ARGS_IF_SCRIPT_PED(PED_PTR, STR, ...) 
#define AI_LOG_WITH_ARGS_IF_SCRIPT_OR_PLAYER_PED(PED_PTR, STR, ...)
#define AI_LOG_WITH_ARGS_IF_SCRIPT_OR_PLAYER_PED_REF(PED_REF, STR, ...) 
#define AI_LOG_STACK_TRACE(DEPTH)
#define AI_LOG_STACK_TRACE_IF_SCRIPT_OR_PLAYER_PED(PED_PTR, DEPTH)
#define AI_LOG_STACK_TRACE_IF_SCRIPT_OR_PLAYER_PED_REF(PED_REF, DEPTH) 
#endif // AI_DEBUG_OUTPUT_ENABLED

#if AI_DEBUG_OUTPUT_ENABLED

// rage headers
#include "fwnet\netlogginginterface.h"
#include "fwnet\netlog.h"

class CEntity;
class CDynamicEntity;
class CPed;
class CVehicle;

namespace AILogging
{
	void LogEntityRejectionWithReason(const CDynamicEntity* pEntity, const char* szFunctionName, const char* szReason);
	void LogEntityAcceptanceWithReason(const CDynamicEntity* pEntity, const char* szFunctionName, const char* szReason);
	void LogEntityRejectionWithIdentifierAndReason(const char* szIdentifier,const CDynamicEntity* pEntity, const char* szReason);
	void LogEntityAcceptanceWithIdentifierAndReason(const char* szIdentifier, const CDynamicEntity* pEntity, const char* szReason);
	void FunctionLog(const char* szFunctionName, const char* szReason, ...);
	void StackTraceLog(const unsigned destCount = 6);
	const char* GetDynamicEntityIsCloneStringSafe(const CDynamicEntity* pEntity);
	const char* GetDynamicEntityOwnerGamerNameSafe(const CDynamicEntity* pEntity);
	const char* GetDynamicEntityNameSafe(const CDynamicEntity* pEntity);
	const char* GetBooleanAsString(bool bVal);
	const char* GetStringWithoutPrefix(const char* szPrefixToStrip, const char* szString);
	atString GetEntityDescription(const CEntity* pEntity);
	atString GetEntityLocalityNameAndPointer(const CDynamicEntity* pEntity);
	enum eLogType
	{
		TTY,
		Screen,
		File
	};
}

class CAILogManager
{
public:

	static void Init(unsigned initMode);
	static void ShutDown(unsigned initMode);

	static void PrintAllGameplayLogs();
	static void PrintAllDebugInfosForAllPeds();
	static void PrintDebugInfosForPedToLog(CPed& rPed, const CVehicle* pVehicle = NULL);
	static void PrintAllDebugInfosForAllVehicles();
	static void PrintDebugInfosForVehicleToLog(CVehicle& rVehicle);
	static void PrintAllCameraSystemLogs();
	
	static bool IsLogInitialised() { return m_Log ? true : false; }
	static bool ShouldSpewOutputLogWhenBugAdded() { return m_ShouldSpewOutputLogWhenBugAdded; }
	static bool ShouldAutoAddLogWhenBugAdded() { return m_ShouldAutoAddLogWhenBugAdded; }
	static netLoggingInterface& GetLog() { if (!m_Log) { Init(0); } return *m_Log; }

#if __BANK
	static bkBank* ms_pBank;
	static void	InitBank();
	static void AddWidgets(bkBank& bank);
	static void	ShutdownBank(); 

#endif // __BANK

private:

	static netLog* m_Log;
	static bool m_IsInitialised;
	static bool m_ShouldSpewOutputLogWhenBugAdded;
	static bool m_ShouldSpewPedDetails;
	static bool m_ShouldSpewVehicleDetails;
	static bool m_ShouldAutoAddLogWhenBugAdded;
};

#endif // AI_DEBUG_OUTPUT_ENABLED



