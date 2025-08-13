#include "ai/debug/system/AIDebugInfo.h"

#if AI_DEBUG_OUTPUT_ENABLED

// rage headers
#include "ai/aichannel.h"
#include "fwnet/netlogutils.h"
#include "grcore/debugdraw.h"

// game headers
#include "Peds/Ped.h"
#include "Peds/PedDebugVisualiser.h"

extern const char* parser_ePedConfigFlags_Strings[];
extern const char* parser_ePedResetFlags_Strings[];

CAIDebugInfo::CAIDebugInfo(DebugInfoPrintParams printParams) 
	: m_PrintParams(printParams) 
	, m_NumberOfLines(0)
	, m_IndentLevel(0)
	, m_SpacesPerIndentScreen(1)
	, m_SpacesPerIndentLog(2)
{

}

void CAIDebugInfo::PrintLn(const char* fmt, ...)
{
	va_list argptr;
	va_start(argptr, fmt);
	const u32 uNumberOfSpacesForIndent = m_IndentLevel * GetSpacesPerIndent();
	char* pStartPoint = &m_TextBuffer[uNumberOfSpacesForIndent];
	ApplyIndentation();
	const u32 uSizeRemaining = 1023 - uNumberOfSpacesForIndent;
	vsnprintf(pStartPoint, uSizeRemaining, fmt, argptr);
	va_end(argptr);
	Print_Internal();
}

void CAIDebugInfo::ColorPrintLn(Color32 color, const char* fmt, ...)
{
	Color32 prevColor = m_PrintParams.Color;
	m_PrintParams.Color = color;
	m_PrintParams.Color.SetAlpha(m_PrintParams.Alpha);
	char TextBuffer[200];
	va_list args;
	va_start(args,fmt);
	vsprintf(TextBuffer,fmt,args);
	va_end(args);
	PrintLn(TextBuffer);
	m_PrintParams.Color = prevColor;
}

void CAIDebugInfo::WriteHeader(const char* fmt, ...)
{
	Color32 prevColor = m_PrintParams.Color;
	m_PrintParams.Color = Color_orange;
	m_PrintParams.Color.SetAlpha(m_PrintParams.Alpha);
	char TextBuffer[200];
	va_list args;
	va_start(args,fmt);
	vsprintf(TextBuffer,fmt,args);
	va_end(args);
	PrintLn(TextBuffer);
	m_PrintParams.Color = prevColor;
}

void CAIDebugInfo::WriteDataValue(const char* dataName, const char* dataValue, ...)
{
	char TextBuffer[200];
	va_list args;
	va_start(args,dataValue);
	vsprintf(TextBuffer,dataValue,args);
	va_end(args);

	if (m_PrintParams.LogType == AILogging::File)
	{
		CAILogManager::GetLog().WriteDataValue(dataName, TextBuffer);
	}
	else
	{
		PrintLn(dataName, TextBuffer);
	}
}

void CAIDebugInfo::WriteLogEvent(const char* nodeName)
{
	if (m_PrintParams.LogType == AILogging::File)
	{
		NetworkLogUtils::WriteLogEvent(CAILogManager::GetLog(), nodeName, "");
	}
	else
	{
		m_PrintParams.Color = Color_white;
		m_PrintParams.Color.SetAlpha(m_PrintParams.Alpha);
		PrintLn(nodeName);
		m_PrintParams.Color = Color_blue;
		m_PrintParams.Color.SetAlpha(m_PrintParams.Alpha);
	}
}

void CAIDebugInfo::WritePedConfigFlag(const CPed& rPed, ePedConfigFlags flag, bool bStripPrefix)
{
	const char* szName = parser_ePedConfigFlags_Strings[flag];
	if (bStripPrefix)
	{	
		szName = AILogging::GetStringWithoutPrefix("CPED_CONFIG_FLAG_", parser_ePedConfigFlags_Strings[flag]);
	}

	WriteBoolValueTrueIsBad(szName, rPed.GetPedConfigFlag(flag));
}

void CAIDebugInfo::WritePedResetFlag(const CPed& rPed, ePedResetFlags flag, bool bStripPrefix)
{
	const char* szName = parser_ePedResetFlags_Strings[flag];
	if (bStripPrefix)
	{	
		szName = AILogging::GetStringWithoutPrefix("CPED_RESET_FLAG_", parser_ePedResetFlags_Strings[flag]);
	}
	
	WriteBoolValueTrueIsBad(szName, rPed.GetPedResetFlag(flag));
}

void CAIDebugInfo::WriteBoolValue(const char* dataName, bool val, bool trueIsGood)
{
	if (m_PrintParams.LogType == AILogging::File)
	{
		CAILogManager::GetLog().WriteDataValue(dataName, AILogging::GetBooleanAsString(val));
	}
	else
	{
		m_PrintParams.Color = trueIsGood ? (val ? Color_green : Color_red) : (val ? Color_red : Color_green);
		m_PrintParams.Color.SetAlpha(m_PrintParams.Alpha);
		char TextBuffer[100];
		formatf(TextBuffer,"%s : %s", dataName, AILogging::GetBooleanAsString(val));
		PrintLn(TextBuffer);
		m_PrintParams.Color = Color_blue;
		m_PrintParams.Color.SetAlpha(m_PrintParams.Alpha);
	}
}

void CAIDebugInfo::Print_Internal() 
{ 
	switch (m_PrintParams.LogType)
	{
		case AILogging::TTY : PrintLineToTTY(); return;
		case AILogging::Screen : PrintLineToScreen(); return;
		case AILogging::File : PrintLineToFile(); return;
		default: return;
	}
}

void CAIDebugInfo::PrintLineToTTY()
{
	aiDisplayf("%s", m_TextBuffer); // TODO: Option to change channel / priority?
}

void CAIDebugInfo::PrintLineToFile()
{
	atString string(m_TextBuffer);
	string += '\n'; // TODO: Optimize?
	CAILogManager::GetLog().Log(string.c_str());
}

bool CAIDebugInfo::ShouldPrintLineToScreen2D() const
{
	return (CPedDebugVisualiserMenu::IsDisplayingOnlyForFocus() && CPedDebugVisualiserMenu::IsFocusPedDisplayIn2D());
}

void CAIDebugInfo::PrintLineToScreen()
{
	if (ShouldPrintLineToScreen2D())
	{
		s32 nYOffset = (s32)(m_NumberOfLines++ * grcDebugDraw::GetScreenSpaceTextHeight());

		float yOffset = ((float)nYOffset / (float)grcDevice::GetHeight());

		Color32 drawColor = m_PrintParams.Color;

		grcDebugDraw::Text(0.02f, 0.02f + yOffset, drawColor, m_TextBuffer, true, 1.0f, 1.0f, 1);
	}
	else
	{
		// TODO: Font size option?
		grcDebugDraw::Text(m_PrintParams.WorldCoords, m_PrintParams.Color, 0, m_NumberOfLines++ * grcDebugDraw::GetScreenSpaceTextHeight(), m_TextBuffer);
	}
}

void CAIDebugInfo::ApplyIndentation()
{	
	const u32 spacesPerIndent = GetSpacesPerIndent();

	for (u32 i=0; i<spacesPerIndent*m_IndentLevel; i=i+spacesPerIndent) // TODO: Optimize?
	{ 
		for (u32 j=i; j<i+spacesPerIndent; ++j)
		{
			m_TextBuffer[j] = ' '; 
		}
	}
}

u32 CAIDebugInfo::GetSpacesPerIndent()
{
	return m_PrintParams.LogType == AILogging::Screen ? m_SpacesPerIndentScreen : m_SpacesPerIndentLog;
}

#endif // AI_DEBUG_OUTPUT_ENABLED
