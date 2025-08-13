#pragma once

#include "ai/debug/system/AIDebugLogManager.h"

#if AI_DEBUG_OUTPUT_ENABLED

// rage headers
#include "vector/vector3.h"
#include "vector/color32.h"

// game headers
#include "Peds/PedFlagsMeta.h"

class CPed;

class CAIDebugInfo
{
public:

	struct DebugInfoPrintParams
	{
		DebugInfoPrintParams() : LogType(AILogging::File), Alpha(255) {}
		DebugInfoPrintParams(const Vector3& vWorldCoods, u8 uAlpha) : LogType(AILogging::Screen), WorldCoords(vWorldCoods), Color(0,0,255,uAlpha), Alpha(uAlpha) {}

		AILogging::eLogType LogType;
		Vector3 WorldCoords;
		Color32 Color;	// TODO: Replace with color scheme? default color, header color, boolean colors etc
		u8 Alpha;
	};

	CAIDebugInfo(DebugInfoPrintParams printParams = DebugInfoPrintParams());
	virtual ~CAIDebugInfo() {}

	virtual void Print() = 0;
	virtual void Visualise() {}

	void SetLogType(AILogging::eLogType logType) { m_PrintParams.LogType = logType; }
	void PushIndent(s32 i=1) { m_IndentLevel += i; }
	void PopIndent(s32 i=1) { if ((m_IndentLevel - i) > 0 ) { m_IndentLevel -= i; } }
	void ResetIndent() { m_IndentLevel = 0; }

	void PrintLn(const char* fmt, ...);
	void ColorPrintLn(Color32 color, const char* fmt, ...);
	void WriteHeader(const char* fmt, ...);
	void WriteDataValue(const char* dataName, const char* dataValue, ...);
	void WriteBoolValueTrueIsGood(const char* dataName, bool val) { WriteBoolValue(dataName, val, true); }
	void WriteBoolValueTrueIsBad(const char* dataName, bool val) { WriteBoolValue(dataName, val, false); }
	void WriteLogEvent(const char* nodeName);
	void WritePedConfigFlag(const CPed& rPed, ePedConfigFlags flag, bool bStripPrefix = false);
	void WritePedResetFlag(const CPed& rPed, ePedResetFlags flag, bool bStripPrefix = false);
	s32  GetNumberOfLines() const { return m_NumberOfLines; }

protected:
	u32 m_NumberOfLines;
	DebugInfoPrintParams m_PrintParams;

private:

	void WriteBoolValue(const char* dataName, bool val, bool trueIsGood);
	void Print_Internal();
	void PrintLineToTTY();
	void PrintLineToFile();
	void PrintLineToScreen();
	bool ShouldPrintLineToScreen2D() const;

	void ApplyIndentation();
	u32 GetSpacesPerIndent();

	u32 m_IndentLevel;
	u32 m_SpacesPerIndentScreen;
	u32 m_SpacesPerIndentLog;

	char m_TextBuffer[1024];
};

#endif // AI_DEBUG_OUTPUT_ENABLED
