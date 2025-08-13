//
// filename:	StreamingMemJunctionStats.h
// author:		Derek Ward <derek.ward@rockstarnorth.com>
// date:		01 Decemeber 2011
// description:	
//

#ifndef INC_StreamingMemJunctionStats_H_
#define INC_StreamingMemJunctionStats_H_

#include "tools/SectorToolsParam.h"
#if SECTOR_TOOLS_EXPORT

// --- Include Files ----------------------------------------------------------------------

// Game headers
#include "ModelInfo/ModelInfo.h"
#include "ModelInfo/ModelInfo_Factories.h"
#include "streaming/streaming.h"
#include "streaming/streamingdebug.h"
#include "tools/SectorTools.h"
#include "scene/debug/SceneCostTracker.h"

// Framework headers
#include "fwtl/assetstore.h"

// --- Defines ----------------------------------------------------------------------------

#define MAX_JUNCTIONS 500

// --- Constants --------------------------------------------------------------------------

// --- Structure/Class Definitions --------------------------------------------------------

// Rage forward declarations
namespace rage
{
	template<class _Type,int _Align,class _CounterType> class atArray;
	class atString;
}

// Game forward declarations
namespace rage {
template<class _Type> class fwPool;
}
class CEntity;

enum eJunctionMemStatState
{
	E_JUNC_MEM_STATE_INVALID = -1,	
	E_JUNC_MEM_STATE_STARTUP,
	E_JUNC_MEM_STATE_NEXT_JUNCTION,
	E_JUNC_MEM_STATE_SPAWN,
	E_JUNC_MEM_STATE_SPIN1,
	E_JUNC_MEM_STATE_STOP_STREAMING,
	E_JUNC_MEM_STATE_SPIN2,
	E_JUNC_MEM_STATE_CAPTURE_START,
	E_JUNC_MEM_STATE_PRE_CAPTURE_DELAY,
	E_JUNC_MEM_STATE_CAPTURE,
	E_JUNC_MEM_STATE_CAPTURE_FINISH,
	E_JUNC_MEM_STATE_WAIT,
	E_JUNC_MEM_STATE_FINISHED
};

enum eCaptureType
{
	E_CAPTURE_TYPE_INVALID = -1,
	E_CAPTURE_TYPE_SPIN,
	E_CAPTURE_TYPE_SIMPLE,
	E_CAPTURE_TYPE_NEW
};

#define MAX_JUNCTION_NAME_LENGTH 30

#if __BANK
class CJunctionTestResults
{
public:
	void Init()
	{
		m_availableMem_Virtual = 0;
		m_availableMem_Physical = 0;

		m_sceneCost_Virtual = 0;
		m_sceneCost_Physical = 0;

		m_overMem_Virtual = 0;
		m_overMem_Physical = 0;

		m_archetypeMem = 0;
		m_numArchetypes = 0;

		m_unavailableMem_Virtual = 0;
		m_unavailableMem_Physical = 0;
	}

	void Set(const CSceneMemoryCheckResult& results)
	{
		m_availableMem_Virtual		= ((s32) results.GetStat( CSceneMemoryCheckResult::STAT_MAIN_FREE_AFTER_FLUSH )) >> 10;
		m_availableMem_Physical		= ((s32) results.GetStat( CSceneMemoryCheckResult::STAT_VRAM_FREE_AFTER_FLUSH )) >> 10;

		m_sceneCost_Virtual			= ((s32) results.GetStat( CSceneMemoryCheckResult::STAT_MAIN_SCENE_REQUIRED )) >> 10;
		m_sceneCost_Physical		= ((s32) results.GetStat( CSceneMemoryCheckResult::STAT_VRAM_SCENE_REQUIRED )) >> 10;

		m_overMem_Virtual			= ((s32) results.GetMainOverOrUnder()) >> 10;
		m_overMem_Physical			= ((s32) results.GetVramOverOrUnder()) >> 10;

		m_unavailableMem_Virtual	= ((s32) results.GetStat( CSceneMemoryCheckResult::STAT_MAIN_USED_AFTER_FLUSH )) >> 10;
		m_unavailableMem_Physical	= ((s32) results.GetStat( CSceneMemoryCheckResult::STAT_VRAM_USED_AFTER_FLUSH )) >> 10;
	}

	s32 m_availableMem_Virtual;
	s32 m_availableMem_Physical;

	s32 m_sceneCost_Virtual;
	s32 m_sceneCost_Physical;	

	s32 m_overMem_Virtual;
	s32 m_overMem_Physical;

	s32 m_unavailableMem_Virtual;
	s32 m_unavailableMem_Physical;

	s32 m_archetypeMem;
	s32 m_numArchetypes;
};
#endif // __BANK

class CStreamingMemJunction
{
	public:
		Vector3 GetPos() { return m_pos; }
		void SetPos(const Vector3 & vPos) { m_pos = vPos; }
		char* GetName() { return m_name; }
		void SetName(const char* pName) { strcpy(m_name,pName); }
		bool IsValid() { return m_bIsValid; }
		void SetValid(const bool bIsValid) { m_bIsValid = bIsValid; }
		
	private:
		Vector3 m_pos;
		char m_name[MAX_JUNCTION_NAME_LENGTH];
		bool m_bIsValid;
};

class CStreamingMemJunctionStats
{
public:
	static void			Init( );
	static void			Shutdown( );

	static void			Run( );
	static void			Update( )				{}

	static bool			GetEnabled( )			{ return m_bEnabled; }
	static void			SetEnabled( bool bSet ) { m_bEnabled = bSet; }

#if __BANK
	static void			InitWidgets( );
#endif

private:

	static void			Spawn();
	static void			StartCapture();
	static void			Capture();
	static void			FinishCapture();
#if __BANK
	static void			StoreCapture(CJunctionTestResults& results);
#endif // __BANK
	static void			Finish();
	static void			DisplayDebug();

	static void			NextJunction( );
	static void			LoadJunctions( );
	static void			SetWeatherAndTime();
	static Color32		StateColour();
	static const char*	StateString(char *pStr);
	static void			PreReadDstFile();
	static void			InvalidateJunction(const char *name);
	static void			WriteHeader();
	static void			SetDefaultCaptureDstFilename();
	static void			RunSpinCaptureStateMachine();
	static void			RunSimpleCaptureStateMachine();
	static void			RunNewCaptureStateMachine();
	static float		GetCaptureDelayRemainingTime();

	static void			HandleCommandLineArgs();
	static void			HandleSrcDstFiles();
	static void			HandleCaptureRange();
	static void			HandlePreCaptureDelay();
	static void			HandleCaptureType();
	static void			HandleSpins();
	static void			HandleSetTimeEachFrame();
	static void			HandleSpinRate();

	static CStreamingMemJunction *m_junctions;
	static s32		m_junctionId;
	static s32	    m_numJunctions;
	static bool		m_captured;
	static bool		m_bEnabled;	

	static float	m_textSize;									
	static Vector2	m_textPos;					
	static Color32	m_textColour;		
	static Color32	m_textColourCapturing;
	static Color32	m_textColourCaptured;
	static bool		m_textBackground;		
	static bool		m_capturing;
	static bool		m_respawn;
	static s32		m_respawns;
	static s32		m_maxRespawns;

	static float	m_camHeading; 
	static float	m_camHeadingSpeed;
	static eJunctionMemStatState m_state;
	static eCaptureType m_captureType;
	static float	m_numSettleSpins;
	static float	m_numStartupSpins;
	static char		m_junctionsSrcFile[MAX_PATH];
	static char		m_junctionsDstFile[MAX_PATH];
	static s32		m_junctionIdEnd;
	static float	m_timePreCaptureStarted;
	static float	m_preCaptureDelay;
	static sysTimer m_timer;
	static bool		m_setTimeEachFrame;
	static u32		m_hourOfDay;
};

#endif // SECTOR_TOOLS_EXPORT

#endif // !INC_StreamingMemJunctionStats_H_
