/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    drawlistNY.h
// PURPOSE : intermediate data between game entitites and command buffers. This contains
//	commands which need to call functions in the project specific code
// AUTHOR :  john.
// CREATED : 20/4/07
//
/////////////////////////////////////////////////////////////////////////////////


#ifndef INC_DRAWLISTNY_H_
#define INC_DRAWLISTNY_H_

#include "renderer/DrawLists/DrawList.h"


enum eInstructionIdNY {
	DC_Invalid = DC_NumBaseDrawCommands,
	DC_DrawPhone_NY,
    DC_DrawNetworkPlayerName_NY,
    DC_DrawNetworkDebugOHD_NY,
	DC_NumNYDrawCommands
};

// initialise correct sizes of debug structs
void InitNYDrawListDebug(void);
void ShutdownNYDrawListDebug(void);

void InitNYDrawList(void);


// draw the hud phone
class CDrawPhoneDC_NY : public dlCmdBase{
public:
	enum {
		INSTRUCTION_ID = DC_DrawPhone_NY, 
		REQUIRED_ALIGNMENT = 16
	};
	s32 GetCommandSize()  {return(sizeof(*this));}
	void Execute();
	static void ExecuteStatic(dlCmdBase &cmd) { ((CDrawPhoneDC_NY &) cmd).Execute(); }

	enum eDrawPhoneMode {
		DRAWPHONE_ALL = 0,
		DRAWPHONE_BODY_ONLY,
		DRAWPHONE_SCREEN_ONLY,
	};

	CDrawPhoneDC_NY(
		u32 modelInfoIdx, 
		Vector3& pos, 
		float scale, 
		Vector3& angle, 
		EulerAngleOrder RotationOrder, 
		float brightness, 
		float screenBrightness, 
		float naturalAmbientScale,
		float artificialAmbientScale,
		int paletteId,
		Matrix34 &camMatrix,
		eDrawPhoneMode drawMode,
		bool inInterior);

private:
	Matrix34 m_camMatrix;
	Vector3 m_pos;
	Vector3	m_angle;
	float	m_scale;
	float	m_brightness;
	float	m_screenBrightness;
	float   m_naturalAmbientScale;
	float   m_artificialAmbientScale;
	int		m_paletteId;
	strLocalIndex	m_modelInfoIdx;
	EulerAngleOrder m_RotationOrder;
	eDrawPhoneMode	m_DrawMode;
	bool m_inInterior;
};

// render a player name
class CDrawNetworkPlayerName_NY : public dlCmdBase
{
public:
	enum {
		INSTRUCTION_ID = DC_DrawNetworkPlayerName_NY,
		REQUIRED_ALIGNMENT = 0,
	};

    s32 GetCommandSize()  {return(sizeof(*this));}
	void Execute();
	static void ExecuteStatic(dlCmdBase &cmd) { ((CDrawNetworkPlayerName_NY &) cmd).Execute(); }

	CDrawNetworkPlayerName_NY(const Vector2 &pos, const Color32 &col, float scale, const char *playerName);

private:

    static const unsigned MAX_PLAYER_NAME = 256;

    Vector2 m_pos;
    Color32	m_col;
    float   m_scale;
    char    m_playerName[MAX_PLAYER_NAME];
};

#if __BANK

// render an OHD debug element
class CDrawNetworkDebugOHD_NY : public dlCmdBase
{
public:
	enum {
		INSTRUCTION_ID = DC_DrawNetworkDebugOHD_NY,
		REQUIRED_ALIGNMENT = 0,
	};

    s32 GetCommandSize()  {return(sizeof(*this));}
	void Execute();
	static void ExecuteStatic(dlCmdBase &cmd) { ((CDrawNetworkDebugOHD_NY &) cmd).Execute(); }

	CDrawNetworkDebugOHD_NY(const Vector2 &pos, const Color32 &col, float scale, const char *debugText);

private:

    static const unsigned MAX_TEXT_LEN = 128;

    Vector2 m_pos;
    Color32	m_col;
    float   m_scale;
    char    m_debugText[MAX_TEXT_LEN];
};

#endif

#endif // INC_DRAWLISTNY_H_
