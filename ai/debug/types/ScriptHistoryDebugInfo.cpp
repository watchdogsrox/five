#include "ai\debug\types\ScriptHistoryDebugInfo.h"

#if AI_DEBUG_OUTPUT_ENABLED

// game headers
#include "Peds\PedDebugVisualiser.h"
#include "Peds\PedIntelligence.h"
#include "script\script.h"

CScriptHistoryDebugInfo::CScriptHistoryDebugInfo(const CPed* pPed, DebugInfoPrintParams printParams)
	: CAIDebugInfo(printParams)
	, m_Ped(pPed)
{

}

void CScriptHistoryDebugInfo::Print()
{
	if (!ValidateInput())
		return;

	WriteLogEvent("SCRIPT TASK HISTORY");
		PushIndent();
			PushIndent();
				PushIndent();
					PrintScriptHistory();

}

bool CScriptHistoryDebugInfo::ValidateInput()
{
	if (!m_Ped)
		return false;

	if (!m_Ped->GetPedIntelligence())
		return false;

	return true;
}

void CScriptHistoryDebugInfo::PrintScriptHistory()
{
#if __DEV
	s32 iRunningScriptCommand = m_Ped->GetPedIntelligence()->GetQueriableInterface()->GetScriptedTaskCommand();
	u8 iTaskStage = m_Ped->GetPedIntelligence()->GetQueriableInterface()->GetScriptedTaskStage();

	if (iRunningScriptCommand != SCRIPT_TASK_INVALID)
	{
		ColorPrintLn(Color_red, "SCRIPT_TASK: %s STAGE: %s", CTheScripts::GetScriptTaskName(iRunningScriptCommand), CTheScripts::GetScriptStatusName(iTaskStage));
	}
	else
	{
		ColorPrintLn(Color_red, "SCRIPT_TASK: None");
	}

	s32 iStart = m_Ped->GetPedIntelligence()->m_iCurrentHistroyTop - 1;
	if (iStart < 0)
		iStart = MAX_DEBUG_SCRIPT_TASK_HISTORY - 1;

	s32 iEnd = iStart - 1;
	if (iEnd < 0)
		iEnd = MAX_DEBUG_SCRIPT_TASK_HISTORY - 1;

	for (s32 i = 0; i > -MAX_DEBUG_SCRIPT_TASK_HISTORY; i--)
	{
		s32 iIndex = iStart + i;
		if (iIndex < 0)
			iIndex += MAX_DEBUG_SCRIPT_TASK_HISTORY;

		Color32 colour = Color_green;
		if (m_Ped->GetPedIntelligence()->m_bNewTaskThisFrame && i == 0)
		{
			colour = Color_red;
		}
		else if (( iIndex & 1 ) == 0)
		{
			colour = Color_blue;
		}
		else
		{
			colour = Color_purple1;
		}

		if (m_Ped->GetPedIntelligence()->m_aScriptHistoryName[iIndex] != NULL)
		{
			float fHowLongAgo = ((float)(fwTimer::GetTimeInMilliseconds() - m_Ped->GetPedIntelligence()->m_aScriptHistoryTime[iIndex])) / 1000.0f;
			ColorPrintLn(Color_red, "%s %s script(%s, %d) - %.2f",	m_Ped->GetPedIntelligence()->m_aScriptHistoryName[iIndex], 
																	CPedDebugVisualiserMenu::ms_menuFlags.m_bDisplayScriptTaskHistoryCodeTasks?m_Ped->GetPedIntelligence()->m_aScriptHistoryTaskName[iIndex]:"", 
																	m_Ped->GetPedIntelligence()->m_szScriptName[iIndex], 
																	m_Ped->GetPedIntelligence()->m_aProgramCounter[iIndex], 
																	fHowLongAgo);
		}
	}
#endif // __DEV
}

#endif // AI_DEBUG_OUTPUT_ENABLED