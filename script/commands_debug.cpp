
// Rage headers
#include "bank/bank.h"
#include "bank/bkmgr.h"
#include "bank/list.h"
#include "bank/slider.h"
#include "bank/title.h"
#include "script/wrapper.h"
#include "grmodel/setup.h"

// Framework headers
#include "fwdebug/debugbank.h"
#include "fwscene/mapdata/mapdatadebug.h"
#include "fwscene/stores/boxstreamerassets.h"
#include "grcore/debugdraw.h"
#include "grcore/setup.h"

// Game headers
#include "debug/AutomatedTest.h"
#include "Debug/debugnodes.h"
#include "debug/DebugScene.h"
#include "event/system/EventData.h"
#include "peds/ped.h"
#include "Peds/PedDebugVisualiser.h"
#include "Peds/PedEventScanner.h"
#include "Peds/PedIntelligence.h"
#include "Peds/pedplacement.h"
#include "physics/physics.h"
#include "script/Handlers/GameScriptResources.h"
#include "script/script.h"
#include "script/script_debug.h"
#include "script/script_helper.h"
#include "system/system.h"
#include "vehicles/vehiclepopulation.h"
#include "debug/vectormap.h"
#include "vehicles/vehicle.h"
#include "Task/Scenario/ScenarioDebug.h"
#include "tools/SmokeTest.h"
#include "system/AutoGPUCapture.h"
#include "debug/DebugLocation.h"
#include "streaming/streamingdebuggraph.h"
#include "scene/debug/SceneCostTracker.h"
#include "renderer/ScreenshotManager.h"
#include "debug/FrameDump.h"

// network includes
#include "network/NetworkInterface.h"
#include "Network/Live/NetworkTelemetry.h"
#if !__FINAL
#include "Network/Live/NetworkDebugTelemetry.h"
#endif // !__FINAL

#include "system/AutoGPUCapture.h"

#include <stdlib.h>		// for atof

SCRIPT_OPTIMISATIONS ()

PARAM(missiongroup, "[commands_debug] Start mission group parameter for the autoflow automated test script");
PARAM(runscript, "[commands_debug] Force this script to load and run after the main script has started");
PARAM(grid_start_x, "[commands_debug] Start x coordinate of grid visitor script");
PARAM(grid_start_y, "[commands_debug] Start y coordinate of grid visitor script");
PARAM(grid_end_x, "[commands_debug] End x coordinate of grid visitor script");
PARAM(grid_end_y, "[commands_debug] End y coordinate of grid visitor script");
PARAM(grid_size, "[commands_debug] Size of the grid cell");
PARAM(skip_telemetry_samples, "[commands_debug] Size of the grid cell (script)");
PARAM(heatmap_memory_test, "[commands_debug](script)");
PARAM(heatmap_reverse, "[commands_debug](script)");

#if RSG_GEN9
XPARAM(rockySmoketest);

namespace rage {
	XPARAM(bootedfromworkspace);
}
#endif // RSG_GEN9

namespace debug_commands
{

#if __FINAL
// No sense giving the hackers any extra info
static const char *s_pNotInFinalBuildError = " ";
#elif !__DEV
static const char *s_pNotInFinalBuildError = "Debug command not in non-DEV build";
#endif


const char* CommandGetCommandlineParam(const char* NOTFINAL_ONLY(param))
{
#if !__FINAL
	return sysParam::SearchArgArray(param);
#else
	return s_pNotInFinalBuildError;
#endif // !__FINAL
}

bool CommandGetCommandlineParamExists(const char* NOTFINAL_ONLY(param))
{
#if !__FINAL
	return sysParam::SearchArgArray(param) != NULL;
#else
	return false;
#endif // !__FINAL
}

float CommandGetCommandlineParamFloat(const char* NOTFINAL_ONLY(param))
{
	float result = 0.0f;
#if !__FINAL
	const char *s = sysParam::SearchArgArray(param);
	if (s && *s)
		result = (float) atof(s);
#endif // !__FINAL
	return result;
}

float CommandGetCommandlineParamFloatByIndex(const char* NOTFINAL_ONLY(param),int NOTFINAL_ONLY(index))
{
	float result = 0.0f;
#if !__FINAL
	const char *s = sysParam::SearchArgArray(param);
	if (s) {
		while (index) {
			s = strchr(s, ',');
			if (!s)
				return 0;
			else
				++s, --index;
		}
		result = (float) atof(s);
	}
#endif // !__FINAL
	return result;
}

int CommandGetCommandlineParamInt(const char* NOTFINAL_ONLY(param))
{
	int result = 0;
#if !__FINAL
	const char *s = sysParam::SearchArgArray(param);
	if (s && *s)
		result = atoi(s);
#endif // !__FINAL
	return result;
}

bool CommandGetCommandlineParamBool(const char* NOTFINAL_ONLY(param))
{
	bool result = false;
#if !__FINAL
	result = sysParam::SearchArgArray(param) != NULL;
#endif // !__FINAL
	return result;
}

void CommandSetDebugActive(bool BANK_ONLY(bActive))
{
#if __BANK
	CScriptDebug::SetDbgFlag(bActive);
#endif
}

//
// name:		SetDebugTextVisible
// description:	Set the debug text and timebars to be visible or not
void SetDebugTextVisible(bool BANK_ONLY(bVisible))
{
#if __BANK
	grcDebugDraw::SetDisplayDebugText(bVisible);
#endif
}

#if SCRIPT_PROFILING
void CommandSetProfilingOfThisScript(bool bProfiling)
{
	if (CTheScripts::GetCurrentGtaScriptThread())
	{
		CTheScripts::GetCurrentGtaScriptThread()->m_bDisplayProfileData = bProfiling;
	}
}
#else	//	SCRIPT_PROFILING
void CommandSetProfilingOfThisScript(bool UNUSED_PARAM(bProfiling))
{
}
#endif	//	SCRIPT_PROFILING

#if __ASSERT
void CommandScriptAssert(const char *pErrorString)
{
	SCRIPT_TIME_CUTOUT()

	// Changed the format of this so different script asserts can reported as separate bugs
	char message[1024];
	sprintf(message, "Assert: %%s %s", pErrorString);

	if (CTheScripts::GetCurrentGtaScriptThread())
	{
		diagLogf(Channel_script, DIAG_SEVERITY_ASSERT, 
			CTheScripts::GetCurrentGtaScriptThread()->GetScriptName(), CTheScripts::GetCurrentGtaScriptThread()->GetProgramCounter(0), 
			message, CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
	else
	{
		diagLogf(Channel_script, DIAG_SEVERITY_ASSERT, 
			__FILE__, __LINE__, 
			message, CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}
#else
void CommandScriptAssert(const char *UNUSED_PARAM(pErrorString))
{
}
#endif

#if SCRIPT_DEBUGGING
void CommandTraceNativeCommand(const char *pCommandName)
{
	if (!pCommandName || strlen(pCommandName) == 0)
	{
		scriptDisplayf("%s has cleared TRACE_NATIVE_COMMAND/BREAK_ON_NATIVE_COMMAND\n", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		scrThread::SetFaultOnFunc();
	}
	else
	{
		scriptDisplayf("%s has called TRACE_NATIVE_COMMAND for %s\n", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pCommandName);
		scrThread::SetFaultOnFunc(pCommandName, true, false);
	}
}

void CommandBreakOnNativeCommand(const char *pCommandName, bool bBreakOnFirstCallOnly)
{
	if (!pCommandName || strlen(pCommandName) == 0)
	{
		scriptDisplayf("%s has cleared BREAK_ON_NATIVE_COMMAND/TRACE_NATIVE_COMMAND\n", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		scrThread::SetFaultOnFunc();
	}
	else
	{
		scriptDisplayf("%s has called BREAK_ON_NATIVE_COMMAND for %s\n", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pCommandName);
		scrThread::SetFaultOnFunc(pCommandName, false, bBreakOnFirstCallOnly);
	}
}
#else
void CommandTraceNativeCommand(const char *)
{
}

void CommandBreakOnNativeCommand(const char *, bool )
{
}
#endif


#if __BANK
static FileHandle fileId = INVALID_FILE_HANDLE;
#endif
/*
void CommandOpenDebugFile()
{
#if __BANK
	if (fileId == INVALID_FILE_HANDLE)
	{
		fileId = CFileMgr::OpenFileForAppending(CScriptDebug::GetNameOfScriptDebugOutputFile());
	}
#endif
}
*/

bool CommandOpenNamedDebugFile(const char *BANK_ONLY (pPathName), const char * BANK_ONLY (pFileName) )
{
#if __BANK
#if RSG_GEN9
	if(PARAM_bootedfromworkspace.Get()) {
		return false;
	}
#endif

	CFileMgr::SetDir(pPathName);
		
	if (SCRIPT_VERIFY(fileId == INVALID_FILE_HANDLE, "The file is already open, call CLOSE_DEBUG_FILE before opening a new file"))
	{
		fileId = CFileMgr::OpenFileForAppending(pFileName);
	}

	if (fileId == INVALID_FILE_HANDLE)
	{
		fileId = CFileMgr::OpenFileForWriting(pFileName);
	}

	CFileMgr::SetDir("");

	if (fileId == INVALID_FILE_HANDLE)
	{
		return false;
	}
	else
	{
		return true;
	}
#else
		return false;
#endif
}

bool CommandOpenDebugFile()
{
#if __BANK
#if RSG_GEN9
	if(PARAM_bootedfromworkspace.Get()) {
		return false;
	}
#endif

	return CommandOpenNamedDebugFile("" , CScriptDebug::GetNameOfScriptDebugOutputFile());
#else
	return false;
#endif
}



void CommandCloseDebugFile()
{
#if __BANK
#if RSG_GEN9
	if(PARAM_bootedfromworkspace.Get()) {
		return false;
	}
#endif

	if (CFileMgr::IsValidFileHandle(fileId))
	{
		CFileMgr::CloseFile(fileId);
		fileId = INVALID_FILE_HANDLE;
	}

#endif
}

void CommandSaveIntToNamedDebugFile(int BANK_ONLY(IntToSave), const char * BANK_ONLY (pPathName), const char * BANK_ONLY (pFileName))
{
#if __BANK
#if RSG_GEN9
	if(PARAM_bootedfromworkspace.Get()) {
		return false;
	}
#endif

	char temp_string[20];
	s32 temp_string_length;

	bool bCloseAfterWriting = false;
	if (fileId == INVALID_FILE_HANDLE)
	{
		CFileMgr::SetDir(pPathName);	
		fileId = CFileMgr::OpenFileForAppending(pFileName);
		bCloseAfterWriting = true;
		CFileMgr::SetDir("");
	}
	if (fileId == INVALID_FILE_HANDLE)
	{
		CFileMgr::SetDir(pPathName);
		fileId = CFileMgr::OpenFileForWriting(pFileName);
		CFileMgr::SetDir("");
	}
	if(SCRIPT_VERIFY(CFileMgr::IsValidFileHandle(fileId), "SAVE_INT_TO_DEBUG_FILE - Couldn't open temp_debug.txt"))
	{
		sprintf(temp_string, "%d", IntToSave);
		temp_string_length = istrlen(temp_string);
		ASSERT_ONLY(s32 NumBytes =) CFileMgr::Write(fileId, (char *) temp_string, temp_string_length);
		scriptAssertf(NumBytes > 0, "%s:SAVE_INT_TO_DEBUG_FILE - Wrote 0 bytes", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (bCloseAfterWriting)
		{
			CFileMgr::CloseFile(fileId);
			fileId = INVALID_FILE_HANDLE;
		}
	}
#endif
}

void CommandSaveIntToDebugFile(int BANK_ONLY(IntToSave))
{
#if __BANK
	CommandSaveIntToNamedDebugFile(IntToSave, "", CScriptDebug::GetNameOfScriptDebugOutputFile());
#endif
}

void CommandSaveFloatToNamedDebugFile(float BANK_ONLY(FloatToSave), const char * BANK_ONLY (pPathName), const char * BANK_ONLY (pFileName))
{
#if __BANK
#if RSG_GEN9
	if(PARAM_bootedfromworkspace.Get()) {
		return false;
	}
#endif

	char temp_string[20];
	s32 temp_string_length;

	bool bCloseAfterWriting = false;
	if (fileId == INVALID_FILE_HANDLE)
	{
		CFileMgr::SetDir(pPathName);
		fileId = CFileMgr::OpenFileForAppending(pFileName);
		bCloseAfterWriting = true;
		CFileMgr::SetDir("");
	}
	if (fileId == INVALID_FILE_HANDLE)
	{
		CFileMgr::SetDir(pPathName);	
		fileId = CFileMgr::OpenFileForWriting(pFileName);
		CFileMgr::SetDir("");
	}
	if(SCRIPT_VERIFY(CFileMgr::IsValidFileHandle(fileId), "SAVE_FLOAT_TO_DEBUG_FILE - Couldn't open temp_debug.txt"))
	{
		sprintf(temp_string, "%.4f", FloatToSave);
		temp_string_length = istrlen(temp_string);
		ASSERT_ONLY(s32 NumBytes =) CFileMgr::Write(fileId, (char *) temp_string, temp_string_length);
		scriptAssertf(NumBytes > 0, "%s:SAVE_FLOAT_TO_DEBUG_FILE - Wrote 0 bytes", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (bCloseAfterWriting)
		{
			CFileMgr::CloseFile(fileId);
			fileId = INVALID_FILE_HANDLE;
		}
	}
#endif
}

void CommandSaveFloatToDebugFile(float BANK_ONLY(FloatToSave))
{
#if __BANK
	CommandSaveFloatToNamedDebugFile(FloatToSave, "", CScriptDebug::GetNameOfScriptDebugOutputFile());
#endif
}


void CommandSaveNewlineToNamedDebugFile(const char * BANK_ONLY (pPathName), const char * BANK_ONLY (pFileName))
{
#if __BANK
#if RSG_GEN9
	if(PARAM_bootedfromworkspace.Get()) {
		return false;
	}
#endif

	char temp_string[20];
	s32 temp_string_length;

	bool bCloseAfterWriting = false;
	if (fileId == INVALID_FILE_HANDLE)
	{
		CFileMgr::SetDir(pPathName);
		fileId = CFileMgr::OpenFileForAppending(pFileName);
		bCloseAfterWriting = true;
		CFileMgr::SetDir("");
	}
	if (fileId == INVALID_FILE_HANDLE)
	{
		CFileMgr::SetDir(pPathName);
		fileId = CFileMgr::OpenFileForWriting(pFileName);
		CFileMgr::SetDir("");

	}
	if(SCRIPT_VERIFY(CFileMgr::IsValidFileHandle(fileId), "SAVE_NEWLINE_TO_DEBUG_FILE - Couldn't open temp_debug.txt"))
	{
		sprintf(temp_string, "\r\n");
		temp_string_length = istrlen(temp_string);
		ASSERT_ONLY(s32 NumBytes =) CFileMgr::Write(fileId, (char *) temp_string, temp_string_length);
		scriptAssertf(NumBytes > 0, "%s:SAVE_NEWLINE_TO_DEBUG_FILE - Wrote 0 bytes", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (bCloseAfterWriting)
		{
			CFileMgr::CloseFile(fileId);
			fileId = INVALID_FILE_HANDLE;
		}
	}
#endif
}

void CommandSaveNewlineToDebugFile()
{
#if __BANK
	CommandSaveNewlineToNamedDebugFile("", CScriptDebug::GetNameOfScriptDebugOutputFile());
#endif
}


void CommandSaveStringToNamedDebugFile(const char *BANK_ONLY(pStringToSave), const char * BANK_ONLY (pPathName), const char * BANK_ONLY (pFileName))
{
#if __BANK
#if RSG_GEN9
	if(PARAM_bootedfromworkspace.Get()) {
		return false;
	}
#endif

	s32 temp_string_length;

	bool bCloseAfterWriting = false;
	if (fileId == INVALID_FILE_HANDLE)
	{
		CFileMgr::SetDir(pPathName);
		fileId = CFileMgr::OpenFileForAppending(pFileName);
		bCloseAfterWriting = true;
		CFileMgr::SetDir("");

	}
	if (fileId == INVALID_FILE_HANDLE)
	{
		CFileMgr::SetDir(pPathName);	
		fileId = CFileMgr::OpenFileForWriting(pFileName);
		CFileMgr::SetDir("");
	}
	if(SCRIPT_VERIFY(CFileMgr::IsValidFileHandle(fileId), "SAVE_STRING_TO_DEBUG_FILE - Couldn't open temp_debug.txt"))
	{
		if (pStringToSave)
		{
			temp_string_length = istrlen(pStringToSave);	//	debug_string);
			if (temp_string_length > 0)
			{
				CFileMgr::Write(fileId, (char *) pStringToSave, temp_string_length);
			}
		}
		if (bCloseAfterWriting)
		{
			CFileMgr::CloseFile(fileId);
			fileId = INVALID_FILE_HANDLE;
		}
	}
#endif
}

void CommandSaveStringToDebugFile(const char *BANK_ONLY(pStringToSave))
{
#if __BANK
	CommandSaveStringToNamedDebugFile(pStringToSave, "", CScriptDebug::GetNameOfScriptDebugOutputFile());
#endif
}


void CommandClearNamedDebugFile(const char * BANK_ONLY (pPathName), const char * BANK_ONLY (pFileName))
{
#if __BANK	
	if (fileId == INVALID_FILE_HANDLE)
	{
		CFileMgr::SetDir(pPathName);
		fileId = CFileMgr::OpenFile(pFileName);
		
		if (fileId)
		{
			CFileMgr::CloseFile(fileId);
			fileId = INVALID_FILE_HANDLE;

			fileId = CFileMgr::OpenFileForWriting(pFileName);
			if (scriptVerifyf(CFileMgr::IsValidFileHandle(fileId), "%s: CLEAR_NAMED_DEBUG_FILE - failed to open %s for writing", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pFileName))
			{
				CFileMgr::CloseFile(fileId);
				fileId = INVALID_FILE_HANDLE;
			}
		}			
			
		CFileMgr::SetDir("");
		
		fileId = INVALID_FILE_HANDLE;
	}
#endif
}

const char *CommandGetModelNameOfVehicleForDebugOnly(int DEV_ONLY(VehicleIndex))
{
#if __DEV
	const char *sEmptyString = "";
	const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);
	if(pVehicle)
	{	
		SCRIPT_ASSERT(pVehicle->m_nDEflags.bCheckedForDead == TRUE, "GET_MODEL_NAME_OF_CAR_FOR_DEBUG_ONLY - Check car is alive this frame");

		return CModelInfo::GetBaseModelInfoName(pVehicle->GetModelId());
	}
	return sEmptyString;
#else
	return s_pNotInFinalBuildError;
#endif
}

const char *CommandGetModelNameForDebug(int DEV_ONLY(ModelHashKey))
{
#if __DEV
	const char *sEmptyString = "";
	fwModelId ModelId;
	CModelInfo::GetBaseModelInfoFromHashKey((u32) ModelHashKey, &ModelId);		//	ignores return value

	if(scriptVerifyf(ModelId.IsValid(), "%s: GET_MODEL_NAME_FOR_DEBUG - couldn't find a model for the given hash key [%d]", CTheScripts::GetCurrentScriptNameAndProgramCounter(), ModelHashKey))
	{
		return CModelInfo::GetBaseModelInfoName(ModelId);
	}
	return sEmptyString;
#else
	return s_pNotInFinalBuildError;
#endif
}

// name:		SetCurrentGroup
// description:	Forces subsequent widget additions to get added to the specified group. ANNOT BE USED DURING NORMAL
//				widget additions this is intended for adding widgets to the specified bank AFTER creation and at
//				runtime.
void CommandSetCurrentWidgetGroup(s32 BANK_ONLY(id))
{
#if __BANK
	if (scriptVerifyf(CTheScripts::GetCurrentGtaScriptThread(), "SET_CURRENT_WIDGET_GROUP - expected current gta script thread to be set here"))
	{
		CTheScripts::GetCurrentGtaScriptThread()->m_WidgetTree.SetCurrentWidgetGroup(id, 
#if __DEV
			CTheScripts::GetCurrentScriptNameAndProgramCounter()
#else
			CTheScripts::GetCurrentGtaScriptThread()->GetScriptName()
#endif
			);
	}
#endif
}

//
// name:		UnSetCurrentGroup
// description:	Pair with SetGroup to leave the bank in a valid state
void CommandClearCurrentWidgetGroup(s32 BANK_ONLY(id))
{
#if __BANK
	if (scriptVerifyf(CTheScripts::GetCurrentGtaScriptThread(), "CLEAR_CURRENT_WIDGET_GROUP - expected current gta script thread to be set here"))
	{
		CTheScripts::GetCurrentGtaScriptThread()->m_WidgetTree.ClearCurrentWidgetGroup(id, 
#if __DEV
			CTheScripts::GetCurrentScriptNameAndProgramCounter()
#else
			CTheScripts::GetCurrentGtaScriptThread()->GetScriptName()
#endif
			);
	}
#endif
}

//
// name:		CreateWidgetGroup
// description:	script command CREATE_WIDGET_GROUP: Create a widget group 
s32 CommandStartWidgetGroup(const char* BANK_ONLY(pName))
{
#if __BANK
	if (Verifyf(CScriptDebug::GetWidgetBank(), "There is no script widget bank"))
	{
		bkGroup* pGroup = CScriptDebug::GetWidgetBank()->PushGroup(pName, false);
		
	# if SCRIPT_PROFILING
		if (scrThread::IsProfilingEnabled() && CTheScripts::GetCurrentGtaScriptThread() && !CTheScripts::GetCurrentGtaScriptThread()->m_WidgetTree.HasCurrentWidgetGroup())
			pGroup->AddToggle("Profile This Thread",&CTheScripts::GetCurrentGtaScriptThread()->m_bDisplayProfileData);
	# endif

		return CScriptDebug::AddWidget(pGroup, WIDGETTYPE_GROUP);
	}
#endif // __BANK

	return 0;
}

//
// name:		EndWidgetGroup
// description:	
void CommandStopWidgetGroup()
{
#if __BANK
	CScriptWidgetTree::CloseWidgetGroup();
#endif
}

//
// name:		DeleteWidgetGroup
// description:	
void CommandDeleteWidgetGroup(s32 BANK_ONLY(id))
{
#if __BANK
	if (scriptVerifyf(CTheScripts::GetCurrentGtaScriptThread(), "DELETE_WIDGET_GROUP - expected current gta script thread to be set here"))
	{
		CTheScripts::GetCurrentGtaScriptThread()->m_WidgetTree.DeleteWidget(id, true, 
#if __DEV
			CTheScripts::GetCurrentScriptNameAndProgramCounter()
#else
			CTheScripts::GetCurrentGtaScriptThread()->GetScriptName()
#endif
			);
	}
#endif	//	__BANK
}

//
// name:		DeleteTextWidget
// description:	
void CommandDeleteTextWidget(s32 BANK_ONLY(id))
{
#if __BANK
	if (SCRIPT_VERIFY(id > 0, "DELETE_TEXT_WIDGET - widget id should be greater than 0"))
	{
		if (scriptVerifyf(CTheScripts::GetCurrentGtaScriptThread(), "DELETE_TEXT_WIDGET - expected current gta script thread to be set here"))
		{
			CTheScripts::GetCurrentGtaScriptThread()->m_WidgetTree.DeleteWidget(id, false, 
#if __DEV
				CTheScripts::GetCurrentScriptNameAndProgramCounter()
#else
				CTheScripts::GetCurrentGtaScriptThread()->GetScriptName()
#endif
				);
		}
	}
#endif
}

//
// name:		DoesWidgetGroupExist
// description:	
bool CommandDoesWidgetGroupExist(s32 BANK_ONLY(id))
{
#if __BANK
	bkWidget* pWidget = CScriptWidgetTree::GetWidgetFromUniqueWidgetId(id);
	if (pWidget)
	{
		return true;	//	pWidget->IsClosedGroup();
	}
#endif
	return false;
}

bool CommandDoesTextWidgetExist(s32 BANK_ONLY(id))
{
#if __BANK
	bkWidget* pWidget = CScriptWidgetTree::GetWidgetFromUniqueWidgetId(id);
	if (pWidget)
	{
		return true;
	}
#endif
	return false;
}

s32 CommandGetIdOfTopLevelWidgetGroup()
{
#if __BANK
	return CScriptDebug::GetUniqueWidgetIdOfTopLevelWidget();
#else
	return 0;
#endif
}


//
// name:		AddSlider
// description:	
void CommandAddWidgetIntSlider(const char* BANK_ONLY(pName), s32& BANK_ONLY(value), 
				s32 BANK_ONLY(min), s32 BANK_ONLY(max), s32 BANK_ONLY(step))
{
#if __BANK
	if (Verifyf(CScriptDebug::GetWidgetBank(), "There is no script widget bank"))
	{
		bkSlider* pSlider = CScriptDebug::GetWidgetBank()->AddSlider(pName, &value, min, max, step);
		CScriptDebug::AddWidget(pSlider, WIDGETTYPE_STANDARD);
	}
#endif
}


void CommandAddWidgetFloatSlider(const char* BANK_ONLY(pName), float& BANK_ONLY(value), 
								  float BANK_ONLY(min), float BANK_ONLY(max), float BANK_ONLY(step))
{
#if __BANK
	if (Verifyf(CScriptDebug::GetWidgetBank(), "There is no script widget bank"))
	{
		bkSlider* pSlider = CScriptDebug::GetWidgetBank()->AddSlider(pName, &value, min, max, step);
		CScriptDebug::AddWidget(pSlider, WIDGETTYPE_STANDARD);
	}
#endif
}

//
// name:		AddSliderReadOnly
// description:	
void CommandAddWidgetIntReadOnly(const char* BANK_ONLY(pName), s32& BANK_ONLY(value))
{
#if __BANK
	s32 min  = INT_MIN;
	s32 max  = INT_MAX;
	s32 step = 0;
	bkSlider* pSlider = CScriptDebug::GetWidgetBank()->AddSlider(pName, &value, min, max, step, NullCB, 0, NULL, false, true);
	CScriptDebug::AddWidget(pSlider, WIDGETTYPE_STANDARD);
#endif
}

//
// name:		AddSliderReadOnlyFloat
// description:	
void AddWidgetFloatReadOnly(const char* BANK_ONLY(pName), float& BANK_ONLY(value))
{
#if __BANK
	if (Verifyf(CScriptDebug::GetWidgetBank(), "There is no script widget bank"))
	{
		float min  = bkSlider::GetFloatMinValue();
		float max  = bkSlider::GetFloatMaxValue();
		float step = 0.0f;
		bkSlider* pSlider = CScriptDebug::GetWidgetBank()->AddSlider(pName, &value, min, max, step, NullCB, 0, NULL, false, true);
		CScriptDebug::AddWidget(pSlider, WIDGETTYPE_STANDARD);
	}
#endif
}

//
// name:		AddToggle
// description:	
void CommandAddWidgetBool(const char* BANK_ONLY(pName), s32& BANK_ONLY(value))
{
#if __BANK
	if (Verifyf(CScriptDebug::GetWidgetBank(), "There is no script widget bank"))
	{
		bkWidget* pWidget = CScriptDebug::GetWidgetBank()->AddToggle(pName, &value);
		CScriptDebug::AddWidget(pWidget, WIDGETTYPE_STANDARD);
	}
#endif
}

//
// name:		AddString
// description:	
void CommandAddWidgetString(const char* BANK_ONLY(pStringToShow))
{
#if __BANK
	if (Verifyf(CScriptDebug::GetWidgetBank(), "There is no script widget bank"))
	{
		bkTitle* pTitle = CScriptDebug::GetWidgetBank()->AddTitle(pStringToShow);
		CScriptDebug::AddWidget(pTitle, WIDGETTYPE_STANDARD);
	}
#endif
}

#if __BANK
#define MAX_NUM_OF_ENTRIES_IN_SCRIPT_WIDGET_COMBO	(80)
#define MAX_NUM_CHARS_IN_SCRIPT_WIDGET_COMBO_ENTRY	(36)

static char ScriptWidgetComboEntries[MAX_NUM_OF_ENTRIES_IN_SCRIPT_WIDGET_COMBO][MAX_NUM_CHARS_IN_SCRIPT_WIDGET_COMBO_ENTRY];
static char *ScriptWidgetComboEntryPointers[MAX_NUM_OF_ENTRIES_IN_SCRIPT_WIDGET_COMBO];
static int NumberOfEntriesInScriptWidgetCombo;
#endif

void CommandStartNewWidgetCombo()
{
#if __BANK
	NumberOfEntriesInScriptWidgetCombo = 0;
#endif
}

void CommandAddToWidgetCombo(const char *BANK_ONLY(pNewComboEntryName))
{
#if __BANK
	if(scriptVerifyf(strlen(pNewComboEntryName) < MAX_NUM_CHARS_IN_SCRIPT_WIDGET_COMBO_ENTRY, "ADD_TO_WIDGET_COMBO - string [%s] is too long", pNewComboEntryName))
	{
		if(SCRIPT_VERIFY(NumberOfEntriesInScriptWidgetCombo < MAX_NUM_OF_ENTRIES_IN_SCRIPT_WIDGET_COMBO, "ADD_TO_WIDGET_COMBO - too many entries in list"))
		{
			strncpy(ScriptWidgetComboEntries[NumberOfEntriesInScriptWidgetCombo], pNewComboEntryName, MAX_NUM_CHARS_IN_SCRIPT_WIDGET_COMBO_ENTRY);
			ScriptWidgetComboEntryPointers[NumberOfEntriesInScriptWidgetCombo] = &ScriptWidgetComboEntries[NumberOfEntriesInScriptWidgetCombo][0];
			NumberOfEntriesInScriptWidgetCombo++;
		}
	}
#endif
}

void CommandStopWidgetCombo(const char* BANK_ONLY(pComboName), s32& BANK_ONLY(value))
{
#if __BANK
	if (Verifyf(CScriptDebug::GetWidgetBank(), "There is no script widget bank"))
	{
		bkCombo *pScriptCombo = CScriptDebug::GetWidgetBank()->AddCombo(pComboName, &value, NumberOfEntriesInScriptWidgetCombo, (const char **) ScriptWidgetComboEntryPointers, NullCB);
		CScriptDebug::AddWidget(pScriptCombo, WIDGETTYPE_STANDARD);
	}
#endif
}


int CommandAddTextWidget(const char *BANK_ONLY(pTextWidgetName))
{
#if __BANK
	return CScriptDebug::GetTextWidgets().AddTextWidget(pTextWidgetName);
#else
	return 0;
#endif
}


const char *CommandGetContentsOfTextWidget(int BANK_ONLY(TextWidgetIndex))
{
	
	static const char *pDummyString = "empty";
#if __BANK
	const char *pContents = CScriptDebug::GetTextWidgets().GetContentsOfTextWidget(TextWidgetIndex);
	if (pContents)
	{
		return pContents;
	}
#endif
	return pDummyString;
}


void CommandSetContentsOfTextWidget(int BANK_ONLY(TextWidgetIndex), const char *BANK_ONLY(pNewString))
{
#if __BANK
	CScriptDebug::GetTextWidgets().SetContentsOfTextWidget(TextWidgetIndex, pNewString);
#endif
}


//	Not sure if there is a way to remove a bit field widget
//	There doesn't seem to be for text widgets. The only place that clears NumberOfTextWidgets
//	is CTheScripts::Init(INIT_AFTER_MAP_LOADED)
void CommandAddBitFieldWidget(const char* BANK_ONLY(pName), s32& BANK_ONLY(value))
{
#if __BANK
	CScriptDebug::GetBitFieldWidgets().AddBitFieldWidget(pName, value);
#endif
}


void CommandAddDebugString(const char* BANK_ONLY(pTextKey), const char* BANK_ONLY(pTextToDisplay))
{
#if __BANK
	TheText.AddToDebugStringMap(pTextKey, pTextToDisplay);
#endif	//	__BANK
}

void CommandRemoveDebugStringWithThisKey(const char* BANK_ONLY(pTextKey))
{
#if __BANK
	TheText.FlagForDeletionFromDebugStringMap(pTextKey);
#endif	//	__BANK
}

void CommandClearAllDebugStrings()
{
#if __BANK
	TheText.FlagAllForDeletionFromDebugStringMap();
#endif	//	__BANK
}


const char *CommandGetRoomNameFromPedDebug(int DEV_ONLY(PedIndex))
{
#if __DEV
	const char* EmptyRoomName = "";
	const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
	if(pPed)
	{
		CPortalTracker* pPT = const_cast<CPortalTracker*>(pPed->GetPortalTracker());
		CInteriorInst* pIntInst = pPT->GetInteriorInst();
		if ((!pIntInst) || (pPT->m_roomIdx == 0)){
			return EmptyRoomName;
		}

		const char *pReturnName = pPT->GetCurrRoomName();
		if (pReturnName)
		{
			return pReturnName;
		}
	}
	
	return EmptyRoomName;
#else
	return s_pNotInFinalBuildError;
#endif
}



bool CommandGetNetworkRestartNodeDebug( int DEV_ONLY(index), Vector3 &vecReturn, float &Heading )
{
	vecReturn.x = vecReturn.y = vecReturn.z = 0.0f;
	Heading = 0.0f;

#if __DEV
	CNodeAddress ResultNode;
	int	searchIndex = index;

	ResultNode = ThePaths.FindNodeWithIndex(searchIndex, false, false, false, false, true);

	if (ResultNode.IsEmpty())
	{
		return false;
	}

	if(SCRIPT_VERIFY(ThePaths.IsRegionLoaded(ResultNode), "GET_NETWORK_RESTART_NODE_DEBUG - Region has not loaded yet"))
	{
		const CPathNode	*pNode = ThePaths.FindNodePointer(ResultNode);
		pNode->GetCoors(vecReturn);
		Heading = pNode->m_2.m_density * (360.0f / 16.0f);
		return true;
	}
	else
	{
		return false;
	}
#else
	return false;
#endif
}

void CommandPrintMissionDescription(const char *UNUSED_PARAM(pTextLabel), int UNUSED_PARAM(iDuration))  
{
}

const char *CommandGetNameOfScriptToAutomaticallyStart()
{
#if !__FINAL
	const char *scriptName = 0;
	if(PARAM_runscript.Get(scriptName))
	{
		return scriptName;
	}
#endif

	static char RetString[STORE_NAME_LENGTH] = "NULL";
	return (const char *)&RetString[0];
}

Vector3 GetScriptDebugMousePointer()
{
	#if __BANK
		return CScriptDebug::GetClickedPos(); 
	#else
		return VEC3_ZERO; 
	#endif
}

int CommandGetNumberOfInstructionsExecuted()
{
	if (CTheScripts::GetCurrentGtaScriptThread())
	{
		return CTheScripts::GetCurrentGtaScriptThread()->GetInstructionsUsed();
	}
	return -1;
}

int CommandGetFocusEntityIndex()
{
#if __BANK
	CEntity* pEnt = CDebugScene::FocusEntities_Get(0);
	if(pEnt && (pEnt->GetIsPhysical()))
	{
		CPhysical* pPhysical = static_cast<CPhysical*>(pEnt); 
		return CTheScripts::GetGUIDFromEntity(*pPhysical); 
	}
#endif
	return NULL_IN_SCRIPTING_LANGUAGE; 
}

void CommandDisableScenariosWithNoAnimData(bool SCENARIO_DEBUG_ONLY(bDisable))
{
#if SCENARIO_DEBUG
	CScenarioDebug::ms_bDisableScenariosWithNoClipData = bDisable;
#endif // SCENARIO_DEBUG
}

void CommandPhysicsDebugSetSleepEnabled(bool DEV_ONLY(bEnableSleep))
{
#if __DEV
	CPhysics::GetSimulator()->SetSleepEnabled(bEnableSleep);
#endif // __DEV
}

float CommandGetProfileStatsFrameTime(const char* STATS_ONLY(pageName), const char* STATS_ONLY(groupName), const char* STATS_ONLY(timerName))
{
#if __STATS
	pfPage* page = GetRageProfiler().GetPageByName(pageName);
	if (Verifyf(page, "Couldn't find profiler page with name '%s'.", pageName))
	{
		for (int groupIndex = 0; groupIndex < page->GetNumGroups(); ++groupIndex)
		{
			pfGroup* group = page->GetGroup(groupIndex);

			if (stricmp(group->GetName(), groupName) == 0)
			{
				pfTimer* timer = group->GetNextTimer(NULL);

				do
				{
					if (stricmp(timer->GetName(), timerName) == 0)
					{
						return timer->GetFrameTime();
					}

					timer = group->GetNextTimer(timer);
				}
				while (timer);

				Assertf(timer, "Couldn't find profiler timer with name '%s' under group '%s' on page '%s'", timerName, groupName, pageName);

				return 0.0f;
			}
		}

		Assertf(false, "Couldn't find profiler group with name '%s' on page '%s'", groupName, pageName);
	}

#endif // __STATS

	return 0.0f;
}

// Smoke test synchronization
bool CommandSmokeTestStarted()
{
#if !__FINAL
	return SmokeTests::SmokeTestStarted();	
#else
	return false;
#endif // !___FINAL 
}

void CommandSmokeTestEnd()
{
#if !__FINAL
	SmokeTests::SmokeTestEnd();
#endif // !___FINAL 
}

void CommandSmokeTestStartCaptureSection(const char* STATS_CAPTURE_ONLY(name))
{
#if ENABLE_STATS_CAPTURE
	MetricsCapture::SmokeTestSectionCaptureStart(name);
#endif // ENABLE_STATS_CAPTURE
}

void CommandSmokeTestStopCaptureSection()
{
#if ENABLE_STATS_CAPTURE
	MetricsCapture::SmokeTestSectionCaptureStop();
#endif // ENABLE_STATS_CAPTURE
}

void CommandSaveScreenshot(const char* NOTFINAL_ONLY(name))
{
#if !__FINAL
	CSystem::GetRageSetup()->SetScreenShotNamingConvention(grcSetup::OVERWRITE_SCREENSHOT);
	CSystem::GetRageSetup()->SetScreenShotName(name);
	CDebug::RequestTakeScreenShot(true);
#endif // !___FINAL
}

void CommandStartContinuousCapture(const char* BANK_ONLY(name), bool BANK_ONLY(jpeg), int /*quality*/)
{
#if __BANK
	CFrameDump::StartCapture(name, jpeg);
#endif // __BANK
}

void CommandStopContinuousCapture()
{
#if __BANK
	CFrameDump::StopCapture();
#endif // __BANK
}

// Debug location
const char* CommandGetCurrentDebugLocationName()
{
#if ENABLE_STATS_CAPTURE
	int current = MetricsCapture::g_LocationList->GetCurrentLocation();
	if (current >= 0 && current < MetricsCapture::g_LocationList->GetLocationsCount())
	{
		return MetricsCapture::g_LocationList->GetLocationName(current);
	}
#endif // ENABLE_STATS_CAPTURE
	return "<unknown>";
}

int CommandGetDebugLocationCount()
{
#if ENABLE_STATS_CAPTURE
	return MetricsCapture::g_LocationList->GetLocationsCount();
#else
	return 0;
#endif // ENABLE_STATS_CAPTURE
}

bool CommandSetCurrentDebugLocation(int STATS_CAPTURE_ONLY(index))
{
#if ENABLE_STATS_CAPTURE
	scriptDisplayf("CommandSetCurrentDebugLocation %d (%s)", index, MetricsCapture::g_LocationList->GetLocationName(index));
	if (scriptVerifyf(index >= 0 && index < MetricsCapture::g_LocationList->GetLocationsCount(), "Invalid index %d", index))
	{
		MetricsCapture::g_LocationList->SetCurrentLocation(index);
		return true;
	}
#endif // ENABLE_STATS_CAPTURE
	return false;
}

bool CommandSetCurrentDebugLocationByName(const char* STATS_CAPTURE_ONLY(name))
{
#if ENABLE_STATS_CAPTURE
	return MetricsCapture::g_LocationList->SetLocationByName(name);
#else
	return false;
#endif // ENABLE_STATS_CAPTURE
}

// Automated test
void CommandAutomatedTestBegin()
{
#if ENABLE_AUTOMATED_TESTS
	AutomatedTest::BeginTest();
#endif // ENABLE_AUTOMATED_TESTS
}

void CommandAutomatedTestStartIteration()
{
#if ENABLE_AUTOMATED_TESTS
	AutomatedTest::StartIteration();
#endif // ENABLE_AUTOMATED_TESTS
}

bool CommandAutomatedTestNextIteration()
{
#if ENABLE_AUTOMATED_TESTS
	return AutomatedTest::NextIteration();
#else // ENABLE_AUTOMATED_TESTS
	return false;
#endif // ENABLE_AUTOMATED_TESTS
}

// Metrics
void CommandMetricsZonesClear()
{
#if ENABLE_STATS_CAPTURE
	MetricsCapture::MetricsZonesClear();
#endif // ENABLE_STATS_CAPTURE
}

void CommandMetricsZonesShow()
{
#if ENABLE_STATS_CAPTURE
	MetricsCapture::MetricsZonesDisplay(true);
#endif // ENABLE_STATS_CAPTURE
}

void CommandMetricsZonesHide()
{
#if ENABLE_STATS_CAPTURE
	MetricsCapture::MetricsZonesDisplay(false);
#endif // ENABLE_STATS_CAPTURE
}

void CommandMetricsZoneStart(const char *STATS_CAPTURE_ONLY(zoneName))
{
#if __XENON && !__FINAL
	if(CDebug::sm_bPgoLiteTrace)
	{
		CDebug::sm_iPgoLiteTraceTime = fwTimer::GetTimeInMilliseconds() + PGOLITETRACE_DELAY;
	}
#endif // __XENON && !__FINAL

#if ENABLE_STATS_CAPTURE
	MetricsCapture::MetricsZoneStart(zoneName);
#endif // ENABLE_STATS_CAPTURE
}

void CommandMetricsZoneStop()
{
#if ENABLE_STATS_CAPTURE
	MetricsCapture::MetricsZoneStop();
#endif
}

void CommandMetricsZoneSave()
{
#if ENABLE_STATS_CAPTURE
	MetricsCapture::MetricsZonesSave();
#endif
}

void CommandMetricsZoneSaveToFile(const char * STATS_CAPTURE_ONLY(fileName))
{
#if ENABLE_STATS_CAPTURE
	MetricsCapture::MetricsZonesSaveToFilename(fileName);
#endif
}

void CommandMetricsZoneSaveTelemetry()
{
#if ENABLE_STATS_CAPTURE
	MetricsCapture::g_ZoneCapture.SendToTelemetry();
#endif	//ENABLE_STATS_CAPTURE
}

// Metrics SmokeTest
void CommandMetricsSmokeTestClear()
{
#if ENABLE_STATS_CAPTURE
	MetricsCapture::MetricsSmokeTestClear();
#endif // ENABLE_STATS_CAPTURE
}

void CommandMetricsSmokeTestShow()
{
#if ENABLE_STATS_CAPTURE
	MetricsCapture::MetricsSmokeTestDisplay(true);
#endif // ENABLE_STATS_CAPTURE
}

void CommandMetricsSmokeTestHide()
{
#if ENABLE_STATS_CAPTURE
	MetricsCapture::MetricsSmokeTestDisplay(false);
#endif // ENABLE_STATS_CAPTURE
}

void CommandMetricsSmokeTestStart(const char *STATS_CAPTURE_ONLY(zoneName))
{
#if __XENON && !__FINAL
	if(CDebug::sm_bPgoLiteTrace)
	{
		CDebug::sm_iPgoLiteTraceTime = fwTimer::GetTimeInMilliseconds() + PGOLITETRACE_DELAY;
	}
#endif // __XENON && !__FINAL

#if RSG_GEN9 && USE_PROFILER
	if (PARAM_rockySmoketest.Get())
	{
		SmokeTests::PerformanceMetrics::Start(Profiler::Mode::DEFAULT_SMOKETEST);
	}
	else
#endif // USE_PROFILER
	{
#if ENABLE_STATS_CAPTURE
		MetricsCapture::MetricsSmokeTestStart(zoneName);
#endif // ENABLE_STATS_CAPTURE
	}
}

void CommandMetricsSmokeTestStop()
{
#if RSG_GEN9 && USE_PROFILER
	if (!PARAM_rockySmoketest.Get())
#endif // USE_PROFILER
	{
#if ENABLE_STATS_CAPTURE
		MetricsCapture::MetricsSmokeTestStop();
#endif
	}
}

void CommandMetricsSmokeTestSave()
{
#if RSG_GEN9 && USE_PROFILER
	if (!PARAM_rockySmoketest.Get())
#endif // USE_PROFILER
	{
#if ENABLE_STATS_CAPTURE
		MetricsCapture::MetricsSmokeTestSave();
#endif
	}
}

void CommandMetricsSmokeTestSaveToFile(const char * STATS_CAPTURE_ONLY(fileName))
{
#if RSG_GEN9 && USE_PROFILER
	if (PARAM_rockySmoketest.Get())
	{
		SmokeTests::PerformanceMetrics::StopAndSave(fileName);
	}
	else
#endif // USE_PROFILER
	{
#if ENABLE_STATS_CAPTURE
		MetricsCapture::MetricsSmokeTestSaveToFilename(fileName);
#endif
	}
}

void CommandMetricsPrintAllScriptCPUTimes()
{
#if ENABLE_STATS_CAPTURE
	MetricsCapture::SetScriptReportAllScriptTimes();
#endif
}

float CommandMetricsGetUpdateTimeForThread(s32 STATS_CAPTURE_ONLY(threadID))
{
#if ENABLE_STATS_CAPTURE
	return MetricsCapture::GetScriptUpdateTime(static_cast<scrThreadId>(threadID));
#else
	return 0.0f;
#endif
}

float CommandMetricsGetAverageTimeForThread(s32 STATS_CAPTURE_ONLY(threadID))
{
#if ENABLE_STATS_CAPTURE
	float peak, average;
	if( MetricsCapture::GetScriptPeakAverageTime(static_cast<scrThreadId>(threadID), peak, average ) )
	{
		return average;
	}
#endif
	return 0.0f;
}

float CommandMetricsGetPeakTimeForThread(s32 STATS_CAPTURE_ONLY(threadID))
{
#if ENABLE_STATS_CAPTURE
	float peak, average;
	if( MetricsCapture::GetScriptPeakAverageTime(static_cast<scrThreadId>(threadID), peak, average ) )
	{
		return peak;
	}
#endif
	return 0.0f;
}

void  CommandMetricsLogTimebars( )
{
#if !__FINAL
	RAGE_TIMEBARS_ONLY( CNetworkDebugTelemetry::CollectMetricTimebars(); )
#endif
}

void CommandValidateEnumValues(int ASSERT_ONLY(iEventCount) )
{
	Assertf( iEventCount == NUM_NETWORK_EVENTTYPE, "ENUM EVENT_NAMES enum is out of sync between code and script! This will break a lot of script AI! Code(%i) Script(%i)", NUM_NETWORK_EVENTTYPE, iEventCount );
}

#if !__FINAL
static const char* CommandGetGameVersionName()
{
#if !__FINAL
	return CDebug::GetVersionNumber();
#else
	return s_pNotInFinalBuildError;
#endif
}
#endif // !__FINAL

void CommandSetStreamGraphDisplay(bool BANK_ONLY(bTurnOn))
{
#if __BANK
	DEBUGSTREAMGRAPH.SetDisplayMemTracking(bTurnOn);
#endif // __BANK
}



void CommandStartHeatmapTest()
{
#if	__BANK
	g_SceneMemoryCheck.Start();
#endif	//__BANK
}

bool CommandIsHeatmapTestComplete()
{
#if	__BANK
	return g_SceneMemoryCheck.IsFinished();
#else	//__BANK
	return false;
#endif	//__BANK
}

void CommandHeatmapTestWrite()
{
#if	__BANK
	if( Verifyf( g_SceneMemoryCheck.IsFinished() == true, "ERROR: Trying to write heatmap data when test is still running")  )
	{
		CSceneMemoryCheckResult &results = g_SceneMemoryCheck.GetResult();
		u32 mainShortfall = results.GetMainShortfall();		// amount over in main ram (bytes)
		u32 vramShortfall = results.GetVramShortfall();		// amount over in vram (bytes)
		g_SceneMemoryCheck.Stop();
#if !__FINAL
		CNetworkDebugTelemetry::AppendMetricHeatMap(mainShortfall, vramShortfall);
#endif
	}
#endif	//__BANK
}


#if	ENABLE_STATS_CAPTURE

// Use m_Samples to see if the data is valid
MetricsCapture::SampledValue<float>	gShapetestResults;

#endif	//__BANK

void CommandDetermineShapetestCost(Vector3 &STATS_CAPTURE_ONLY(start), Vector3 &STATS_CAPTURE_ONLY(end), float STATS_CAPTURE_ONLY(granularity), float STATS_CAPTURE_ONLY(radius))
{

#if	ENABLE_STATS_CAPTURE

	sysTimer	timer;

	gShapetestResults.Reset();

	float width = end.x - start.x;
	float depth = end.y - start.y;

	int	numX = (int)(width / granularity);
	int	numY = (int)(depth / granularity);

	for(int x=0; x<numX; x++)
	{
		for(int y=0; y<numY; y++)
		{
			// Prepare all the vars external to any timing.

			float xPos = start.x + (x * granularity);
			float yPos = start.y + (y * granularity);

			Vector3 groundTestPosStart(xPos, yPos, 2000.0f);
			Vector3 groundTestPosEnd(xPos, yPos, -1000.0f);

			// Do the worldprobe to find the ground position
			WorldProbe::CShapeTestProbeDesc probeDesc;
			WorldProbe::CShapeTestFixedResults<> probeResults;
			probeDesc.SetResultsStructure(&probeResults);
			probeDesc.SetStartAndEnd(groundTestPosStart, groundTestPosEnd);
			probeDesc.SetExcludeEntity(FindPlayerPed());
			probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
			if(!WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
			{
				// We didn't hit anything!
				Assertf(0, "commands_debug::CommandDetermineShapetestCost: Could not find the ground position!");
			}

			// We've hit something
			if (Verifyf(probeResults[0].GetHitDetected(), "probe test reports a hit but results aren't valid????"))
			{
				Vector3	hitPos = probeResults[0].GetHitPosition();

				Vector3	capsuleStart( hitPos.x, hitPos.y, hitPos.z + 15.0f);
				Vector3	capsuleEnd( hitPos.x, hitPos.y, hitPos.z - 15.0f);

				WorldProbe::CShapeTestCapsuleDesc CapsuleTest;
				WorldProbe::CShapeTestFixedResults<> CapsuleTestResults;
				CapsuleTest.SetResultsStructure(&CapsuleTestResults);
				CapsuleTest.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
				CapsuleTest.SetExcludeEntity(FindPlayerPed());
				CapsuleTest.SetIsDirected(false);										// CAPSULE_TEST
				CapsuleTest.SetCapsule(capsuleStart, capsuleEnd, radius );				// This barfs if the capsule is too large

				// Get start time
				timer.Reset();
				float startTime = timer.GetMsTime();

				// Do the test
				WorldProbe::GetShapeTestManager()->SubmitTest(CapsuleTest);				// Mode defaults to PERFORM_SYNCHRONOUS_TEST

				// Get end time before anything else
				float overallTime = timer.GetMsTime() - startTime;

				gShapetestResults.AddSample(overallTime);
			}
		}
	}

	/*
	if( gShapetestResults.Samples() )
	{
		Displayf("ShapeTest Results");
		Displayf("Min = %f", gShapetestResults.Min());
		Displayf("Max = %f", gShapetestResults.Max());
		Displayf("Avg = %f\n", gShapetestResults.Mean());
	}
	*/

#endif	//ENABLE_STATS_CAPTURE

}

bool CommandSendShapetestCostToTelemetry()
{
	bool results = false;

#if	ENABLE_STATS_CAPTURE
	if(gShapetestResults.Samples())
	{
		float min = gShapetestResults.Min();
		float max = gShapetestResults.Max();
		float avg = gShapetestResults.Mean();
#if !__FINAL
		CNetworkDebugTelemetry::AppendMetricShapetestCost(min, max, avg);
#endif
		results = true;
	}
#endif	//ENABLE_STATS_CAPTURE
	return results;
}

void CommandDbgSetMapDataStats(bool BANK_ONLY(bEnabled))
{
#if __BANK
	fwMapDataDebug::ms_bShowLoadedStats = bEnabled;
#endif
}

s32 CommandDbgGetNumMapDataLoaded(s32 BANK_ONLY(mapDataType))
{
#if __BANK
	switch (mapDataType)
	{
	case 0:
		return fwMapDataDebug::ms_dbgNumLoaded[fwBoxStreamerAsset::ASSET_MAPDATA_LOW];
	case 1:
		return fwMapDataDebug::ms_dbgNumLoaded[fwBoxStreamerAsset::ASSET_MAPDATA_MEDIUM];
	case 2:
		return fwMapDataDebug::ms_dbgNumLoaded[fwBoxStreamerAsset::ASSET_MAPDATA_HIGH];
	default:
		break;
	}
#endif
	return -1;
}

s32 CommandDbgGetNumMapDataRequired(s32 BANK_ONLY(mapDataType))
{
#if __BANK
	switch (mapDataType)
	{
	case 0:
		return fwMapDataDebug::ms_dbgNumRequired[fwBoxStreamerAsset::ASSET_MAPDATA_LOW];
	case 1:
		return fwMapDataDebug::ms_dbgNumRequired[fwBoxStreamerAsset::ASSET_MAPDATA_MEDIUM];
	case 2:
		return fwMapDataDebug::ms_dbgNumRequired[fwBoxStreamerAsset::ASSET_MAPDATA_HIGH];
	default:
		break;
	}
#endif
	return -1;
}

bool CommandHavePlayerCoordsBeenOverridden()
{
#if __FINAL
	return false;
#else
	return CScriptDebug::GetPlayerCoordsHaveBeenOverridden();
#endif
}

void CommandMonitorScriptArrayForSizeOverwrite(int& NOTFINAL_ONLY(AddressTemp))
{
#if !__FINAL
	s32 *pAddressToMonitor = &AddressTemp;
	scrThread::StartMonitoringScriptArraySize(pAddressToMonitor);
#endif	//	!__FINAL
}

void SetupScriptCommands()
{
	SCR_REGISTER_UNUSED(SCRIPT_ASSERT,0x37e1d85a2a3321c5,								CommandScriptAssert);
	SCR_REGISTER_UNUSED(GET_NAME_OF_SCRIPT_TO_AUTOMATICALLY_START,0x9a823c33fc60a8d2, CommandGetNameOfScriptToAutomaticallyStart);

	SCR_REGISTER_UNUSED(HAVE_PLAYER_COORDS_BEEN_OVERRIDDEN,0x92c8de189b5c14b6, CommandHavePlayerCoordsBeenOverridden);

	SCR_REGISTER_UNUSED(GET_COMMANDLINE_PARAM,0x61608341e5737e5f,						CommandGetCommandlineParam);
	SCR_REGISTER_UNUSED(GET_COMMANDLINE_PARAM_EXISTS,0xacc05862fbd8fde5,				CommandGetCommandlineParamExists);
	SCR_REGISTER_UNUSED(GET_COMMANDLINE_PARAM_BOOL,0xde8417512afd99c8,					CommandGetCommandlineParamBool);
	SCR_REGISTER_UNUSED(GET_COMMANDLINE_PARAM_INT,0x70f0df147a7e148b,					CommandGetCommandlineParamInt);
	SCR_REGISTER_UNUSED(GET_COMMANDLINE_PARAM_FLOAT,0x5fc6222a3c7e7b12,				CommandGetCommandlineParamFloat);
	SCR_REGISTER_UNUSED(GET_COMMANDLINE_PARAM_FLOAT_BY_INDEX,0xbb0f4a7512f460cf,		CommandGetCommandlineParamFloatByIndex);

	SCR_REGISTER_UNUSED(SET_DEBUG_ACTIVE,0xc9441448f237ec71,							CommandSetDebugActive);
	SCR_REGISTER_UNUSED(SET_DEBUG_TEXT_VISIBLE,0x36f37a089707fc50,						SetDebugTextVisible);
	SCR_REGISTER_UNUSED(SET_PROFILING_OF_THIS_SCRIPT,0x3d58e61e8f7f8bee,				CommandSetProfilingOfThisScript);
	SCR_REGISTER_UNUSED(TRACE_NATIVE_COMMAND,0x3baaf5156ad7b58a,						CommandTraceNativeCommand);
	SCR_REGISTER_UNUSED(BREAK_ON_NATIVE_COMMAND,0x0dcc8c6d57f275ab,					CommandBreakOnNativeCommand);

	SCR_REGISTER_UNUSED(OPEN_DEBUG_FILE,0x32e242335d480468,							CommandOpenDebugFile);
	SCR_REGISTER_UNUSED(CLOSE_DEBUG_FILE,0x22167ab70ea9074e,							CommandCloseDebugFile);
	SCR_REGISTER_UNUSED(SAVE_INT_TO_DEBUG_FILE,0xccb023e95ffa6675,						CommandSaveIntToDebugFile);
	SCR_REGISTER_UNUSED(SAVE_FLOAT_TO_DEBUG_FILE,0xb483977efcbd8174,					CommandSaveFloatToDebugFile);
	SCR_REGISTER_UNUSED(SAVE_NEWLINE_TO_DEBUG_FILE,0xa1de54c939543cd8,					CommandSaveNewlineToDebugFile);
	SCR_REGISTER_UNUSED(SAVE_STRING_TO_DEBUG_FILE,0x799589159e77d6f1,					CommandSaveStringToDebugFile);
	SCR_REGISTER_UNUSED(GET_MODEL_NAME_OF_VEHICLE_FOR_DEBUG_ONLY,0x980ec4e20c81fc81,	CommandGetModelNameOfVehicleForDebugOnly);
	SCR_REGISTER_UNUSED(GET_MODEL_NAME_FOR_DEBUG,0xbf04fa25640e9235,					CommandGetModelNameForDebug);
	
	SCR_REGISTER_UNUSED(OPEN_NAMED_DEBUG_FILE,0xf838845ad041df6f,						CommandOpenNamedDebugFile);
	SCR_REGISTER_UNUSED(SAVE_INT_TO_NAMED_DEBUG_FILE,0xb78da46bbbffe697,				CommandSaveIntToNamedDebugFile);
	SCR_REGISTER_UNUSED(SAVE_FLOAT_TO_NAMED_DEBUG_FILE,0x94cc0d35eeb50750,				CommandSaveFloatToNamedDebugFile);
	SCR_REGISTER_UNUSED(SAVE_STRING_TO_NAMED_DEBUG_FILE,0x4f0c3061e8025888,			CommandSaveStringToNamedDebugFile); 
	SCR_REGISTER_UNUSED(SAVE_NEWLINE_TO_NAMED_DEBUG_FILE,0x2c97b16506c3f72d,			CommandSaveNewlineToNamedDebugFile);
	SCR_REGISTER_UNUSED(CLEAR_NAMED_DEBUG_FILE,0x49f6c6837ff3922f,						CommandClearNamedDebugFile);
	
	SCR_REGISTER_UNUSED(GET_ROOM_NAME_FROM_PED_DEBUG,0xf88dcd16dfc4f17d,				CommandGetRoomNameFromPedDebug);

	// bank widget commands
	SCR_REGISTER_UNUSED(SET_CURRENT_WIDGET_GROUP,0xd7cf1dc4baaca470, CommandSetCurrentWidgetGroup);
	SCR_REGISTER_UNUSED(CLEAR_CURRENT_WIDGET_GROUP,0x8ca9fb4895c97fc7, CommandClearCurrentWidgetGroup);
	SCR_REGISTER_UNUSED(START_WIDGET_GROUP,0x43ec0feab3829b5a, CommandStartWidgetGroup);
	SCR_REGISTER_UNUSED(STOP_WIDGET_GROUP,0x083184ea2265df87, CommandStopWidgetGroup);
	SCR_REGISTER_UNUSED(ADD_WIDGET_INT_SLIDER,0x74015e8111dbffef, CommandAddWidgetIntSlider);
	SCR_REGISTER_UNUSED(ADD_WIDGET_FLOAT_SLIDER,0x7227d0c7e8d54fde, CommandAddWidgetFloatSlider);
	SCR_REGISTER_UNUSED(ADD_WIDGET_INT_READ_ONLY,0x391bb643669f4ebf, CommandAddWidgetIntReadOnly);
	SCR_REGISTER_UNUSED(ADD_WIDGET_FLOAT_READ_ONLY,0x4758e60839d423b6, AddWidgetFloatReadOnly);
	SCR_REGISTER_UNUSED(ADD_WIDGET_BOOL,0xae96280cf484f907, CommandAddWidgetBool);
	SCR_REGISTER_UNUSED(ADD_WIDGET_STRING,0xf448ba469bfdfa52, CommandAddWidgetString);
	SCR_REGISTER_UNUSED(DELETE_WIDGET_GROUP,0x6187c1ed7467ede4, CommandDeleteWidgetGroup);
	SCR_REGISTER_UNUSED(DELETE_TEXT_WIDGET,0xd268b8a702074186, CommandDeleteTextWidget);
	SCR_REGISTER_UNUSED(DOES_WIDGET_GROUP_EXIST,0x2fffad519592c13f, CommandDoesWidgetGroupExist);
	SCR_REGISTER_UNUSED(DOES_TEXT_WIDGET_EXIST,0xda53aba8ea85e865, CommandDoesTextWidgetExist);
	SCR_REGISTER_UNUSED(GET_ID_OF_TOP_LEVEL_WIDGET_GROUP,0xf36a18017af7d65f, CommandGetIdOfTopLevelWidgetGroup);

	// console commands
	SCR_REGISTER_UNUSED(START_NEW_WIDGET_COMBO,0x7808d27c1dde6d52, CommandStartNewWidgetCombo);
	SCR_REGISTER_UNUSED(ADD_TO_WIDGET_COMBO,0xa6fe8f276cea9017, CommandAddToWidgetCombo);
	SCR_REGISTER_UNUSED(STOP_WIDGET_COMBO,0x65260ceb9d06c20c, CommandStopWidgetCombo);

	SCR_REGISTER_UNUSED(ADD_TEXT_WIDGET,0x2835f632d23c8ecf, CommandAddTextWidget);
	SCR_REGISTER_UNUSED(GET_CONTENTS_OF_TEXT_WIDGET,0x843d4e4d887ec17b, CommandGetContentsOfTextWidget);
	SCR_REGISTER_UNUSED(SET_CONTENTS_OF_TEXT_WIDGET,0x5f715e762f04cde0, CommandSetContentsOfTextWidget);

	SCR_REGISTER_UNUSED(ADD_BIT_FIELD_WIDGET,0x2c12383c6b60b963, CommandAddBitFieldWidget);

	SCR_REGISTER_UNUSED(ADD_DEBUG_STRING,0x5ee7aab423e0d2d7, CommandAddDebugString);
	SCR_REGISTER_UNUSED(REMOVE_DEBUG_STRING_WITH_THIS_KEY,0xd9f0ab18d80da20f, CommandRemoveDebugStringWithThisKey);
	SCR_REGISTER_UNUSED(CLEAR_ALL_DEBUG_STRINGS,0x58b4d4f0ac6fe243, CommandClearAllDebugStrings);


	SCR_REGISTER_UNUSED(GET_NETWORK_RESTART_NODE_DEBUG,0xdac86a0f037e257b, CommandGetNetworkRestartNodeDebug);

	SCR_REGISTER_UNUSED(PRINT_MISSION_DESCRIPTION, 0x61668a58, CommandPrintMissionDescription);

	//mouse commands
	SCR_REGISTER_UNUSED(GET_SCRIPT_MOUSE_POINTER_IN_WORLD_COORDS,0x0462df823be75a10, GetScriptDebugMousePointer);

	SCR_REGISTER_UNUSED(GET_NUMBER_OF_INSTRUCTIONS_EXECUTED,0x2339945490fd1a00, CommandGetNumberOfInstructionsExecuted);

	//debug entity 
	SCR_REGISTER_UNUSED(GET_FOCUS_ENTITY_INDEX,0xfd109c50560d00ef, CommandGetFocusEntityIndex);

	// Scenario debug
	SCR_REGISTER_UNUSED(DISABLE_INCOMPLETE_SCENARIOS,0xbf0034b2971f106b, CommandDisableScenariosWithNoAnimData);

	// Physics debug:
	SCR_REGISTER_UNUSED(PHYSICS_DEBUG_SET_SLEEP_ENABLED,0x261840193762c6f3, CommandPhysicsDebugSetSleepEnabled);

	// Profile stats (EKG):
	SCR_REGISTER_UNUSED(GET_PROFILE_STATS_FRAME_TIME,0x21cdd51335b3ec91, CommandGetProfileStatsFrameTime);

	// Smoke test synchronization
	SCR_REGISTER_UNUSED(SMOKETEST_STARTED,0x2be6ecf537a50330, CommandSmokeTestStarted);
	SCR_REGISTER_UNUSED(SMOKETEST_END,0x7b43ebace329ebcb, CommandSmokeTestEnd);
	SCR_REGISTER_UNUSED(SMOKETEST_START_CAPTURE_SECTION,0x7cfbeaf53e941de2, CommandSmokeTestStartCaptureSection);
	SCR_REGISTER_UNUSED(SMOKETEST_STOP_CAPTURE_SECTION,0x2bbd4a403df3fa47, CommandSmokeTestStopCaptureSection);
	SCR_REGISTER_UNUSED(SAVE_SCREENSHOT,0x9f31211e267bec5d, CommandSaveScreenshot);

	// Frame capture
	SCR_REGISTER_UNUSED(START_FRAME_CAPTURE,0x152c18ae972ef737, CommandStartContinuousCapture);
	SCR_REGISTER_UNUSED(STOP_FRAME_CAPTURE,0x3f8ec46956958660, CommandStopContinuousCapture);

	// Debug location
	SCR_REGISTER_UNUSED(GET_CURRENT_DEBUG_LOCATION_NAME,0xe5014709f82e4c4e, CommandGetCurrentDebugLocationName);
	SCR_REGISTER_UNUSED(GET_DEBUG_LOCATION_COUNT,0xd4e4b88593992901, CommandGetDebugLocationCount);
	SCR_REGISTER_UNUSED(SET_CURRENT_DEBUG_LOCATION,0x4e2d6a82bf188b5a, CommandSetCurrentDebugLocation);
	SCR_REGISTER_UNUSED(SET_CURRENT_DEBUG_LOCATION_BY_NAME,0x505a631e261af27c, CommandSetCurrentDebugLocationByName);

	// Metrics
	SCR_REGISTER_UNUSED(METRICS_SMOKETEST_CLEAR,0x552324442cb70e28, CommandMetricsSmokeTestClear);
	SCR_REGISTER_UNUSED(METRICS_SMOKETEST_SHOW,0x3276153724a91a6f, CommandMetricsSmokeTestShow);
	SCR_REGISTER_UNUSED(METRICS_SMOKETEST_HIDE,0x17d2d5323cc8578d, CommandMetricsSmokeTestHide);
	SCR_REGISTER_UNUSED(METRICS_SMOKETEST_START,0x5acdbf6833438920, CommandMetricsSmokeTestStart);
	SCR_REGISTER_UNUSED(METRICS_SMOKETEST_STOP,0x79eda528c2093155, CommandMetricsSmokeTestStop);
	SCR_REGISTER_UNUSED(METRICS_SMOKETEST_SAVE,0x4f6356450e497da9, CommandMetricsSmokeTestSave);
	SCR_REGISTER_UNUSED(METRICS_SMOKETEST_SAVE_TO_FILE,0x1e02a1b435c8198b, CommandMetricsSmokeTestSaveToFile);
	SCR_REGISTER_UNUSED(METRICS_ZONES_CLEAR,0x9e91cadb6d6b1baa, CommandMetricsZonesClear);
	SCR_REGISTER_UNUSED(METRICS_ZONES_SHOW,0x86c3dd783b38f078, CommandMetricsZonesShow);
	SCR_REGISTER_UNUSED(METRICS_ZONES_HIDE,0xbe72bf3ef0807c1b, CommandMetricsZonesHide);
	SCR_REGISTER_UNUSED(METRICS_ZONE_START,0x088de6cf540487f9, CommandMetricsZoneStart);
	SCR_REGISTER_UNUSED(METRICS_ZONE_STOP,0x228d28ca792bb99a, CommandMetricsZoneStop);
	SCR_REGISTER_UNUSED(METRICS_ZONE_SAVE,0xe2bcf94771089012, CommandMetricsZoneSave);
	SCR_REGISTER_UNUSED(METRICS_ZONE_SAVE_TO_FILE,0x24df8b2ce0c048d2, CommandMetricsZoneSaveToFile);
	SCR_REGISTER_UNUSED(METRICS_ZONE_SAVE_TELEMETRY,0x0685f602f6eda361, CommandMetricsZoneSaveTelemetry);
	
	SCR_REGISTER_UNUSED(METRICS_PRINT_SCRIPT_PROCESSING_TIMES,0x8a5895a3855b2503, CommandMetricsPrintAllScriptCPUTimes);
	SCR_REGISTER_UNUSED(METRICS_GET_UPDATE_TIME_FOR_THREAD,0x5f17bd5e28977f36, CommandMetricsGetUpdateTimeForThread);
	SCR_REGISTER_UNUSED(METRICS_GET_AVERAGE_UPDATE_TIME_FOR_THREAD,0x603d5ec109b5b015, CommandMetricsGetAverageTimeForThread);
	SCR_REGISTER_UNUSED(METRICS_GET_PEAK_UPDATE_TIME_FOR_THREAD,0x8be3cb2616fd74dc, CommandMetricsGetPeakTimeForThread);

	SCR_REGISTER_UNUSED(METRICS_LOG_TIMEBARS,0xebdfbe5c1bdedc4b, CommandMetricsLogTimebars);

	SCR_REGISTER_UNUSED(VALIDATE_ENUM_VALUES,0x1c910789c011e3f0,CommandValidateEnumValues);

	SCR_REGISTER_UNUSED(GET_GAME_VERSION_NAME,0x91f21bab5f932a1e,  CommandGetGameVersionName);

	SCR_REGISTER_UNUSED(SET_DISPLAY_STREAM_GRAPH,0x18f2e3cec6745f44,  CommandSetStreamGraphDisplay);

	// Heatmap test
	SCR_REGISTER_UNUSED(HEATMAP_TEST_START,0xd82a22c65d2d2106,  CommandStartHeatmapTest);
	SCR_REGISTER_UNUSED(HEATMAP_TEST_IS_COMPLETE,0x9fc6eaef514636b5,  CommandIsHeatmapTestComplete);
	SCR_REGISTER_UNUSED(HEATMAP_TEST_WRITE_TELEMETRY,0xca960d484df81d21,  CommandHeatmapTestWrite);

	// Determine physics cost per location
	SCR_REGISTER_UNUSED(DETERMINE_SHAPETEST_COST,0x3c30297ad332330c, CommandDetermineShapetestCost);
	SCR_REGISTER_UNUSED(SHAPETEST_COST_WRITE_TELEMETRY,0x29750f04e1ade996, CommandSendShapetestCostToTelemetry);

	BANK_ONLY(DebugNodeSys::SetupScriptCommands());

	// map data stats
	SCR_REGISTER_UNUSED(DBG_SET_MAPDATA_STATS,0x8967e79ba76d616f, CommandDbgSetMapDataStats);
	SCR_REGISTER_UNUSED(DBG_GET_NUM_MAPDATA_LOADED,0x5b438223aab86efc, CommandDbgGetNumMapDataLoaded);
	SCR_REGISTER_UNUSED(DBG_GET_NUM_MAPDATA_REQUIRED,0x7c66a684004a66b6, CommandDbgGetNumMapDataRequired);

	SCR_REGISTER_UNUSED(AUTOMATED_TEST_BEGIN,0xdbf061ca20d165d5, CommandAutomatedTestBegin);
	SCR_REGISTER_UNUSED(AUTOMATED_TEST_START_ITERATION,0x3488c6867bdc647a, CommandAutomatedTestStartIteration);
	SCR_REGISTER_UNUSED(AUTOMATED_TEST_NEXT_ITERATION,0xdefc6ac003d2e9f7, CommandAutomatedTestNextIteration);

	SCR_REGISTER_UNUSED(MONITOR_SCRIPT_ARRAY_FOR_SIZE_OVERWRITE,0x9d5a8f0c4aef81d0, CommandMonitorScriptArrayForSizeOverwrite);
}

}	//	end of namespace debug_commands
