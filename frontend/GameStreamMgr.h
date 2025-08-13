
/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : GameStreamMgr.h
// PURPOSE : manages the Scaleform hud live info/updates
//
// See: http://rsgediwiki1/wiki/index.php/HUD_Game_Stream for reference.
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef __GAMESTREAMMGR_H__
#define __GAMESTREAMMGR_H__

// rage
#include "atl/array.h"
#include "Scaleform/scaleform.h"
#include "vector/vector2.h"

// game
#include "frontend/GameStream.h"
#include "system/control.h"

#define GAMESTREAM_UNLOAD_UPDATEWAIT	40	// number of updates to wait before unloading the movie.

enum eLoadStat
{
	GAMESTREAM_UNLOADED = 0,
	GAMESTREAM_LOADING,
	GAMESTREAM_LOADED,
	GAMESTREAM_UNLOADING
};



class CGamestreamXMLMgr
{
public:
	static void RegisterXMLReader(atHashWithStringBank objName, datCallback handleFunc);
	static void LoadXML( bool bForceReload = false);

	static void DoNothing(s32, parTreeNode*) {};


private:
	typedef atMap<atHashWithStringBank, datCallback> XMLLoadTypeMap;
	static XMLLoadTypeMap sm_gsXmlLoadMap;
	static bool sm_bGSXmlLoadedOnce;

};

#define REGISTER_GAMESTREAM_XML(func, name)			CGamestreamXMLMgr::RegisterXMLReader(name, datCallback(CFA1(func), 0, true))
#define REGISTER_GAMESTREAM_XML_CB(func, name, data)	CGamestreamXMLMgr::RegisterXMLReader(name, datCallback(CFA2(func), CallbackData(data)))


class CGameStreamMgr
{
public:

	struct sToolTip
	{
		int tipId;
		KEYBOARD_MOUSE_ONLY(int inputDevice;)
	};

	static void Init(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);
	static void Update();
	static void Render();
	static void LoadXmlData(bool bLoadGameStreamDataOnly);
	static void RequestAssets();

	static void HandleXML( parTreeNode* pGameStreamNode );
	static void HandleTooltipXml( parTreeNode* pGameStreamNode  );

#if RSG_PC
	static void DeviceLost();
	static void DeviceReset();
#endif // RSG_PC

	static void CheckIncomingFunctions(atHashWithStringBank methodName, const GFxValue* args);
	
	static CGameStream* GetGameStream();

	//These don't fit here particularly well. Will consider a move to somewhere more appropriate.
	static int GetNumMpToolTips() { return m_MultiPlayerToolTips.GetCount(); }  
	static int GetNumSpToolTips() { return m_SinglePlayerToolTips.GetCount(); }
	static int GetNumReplayToolTips() {return m_ReplayToolTips.GetCount(); }

	static int GetMpToolTipId(int index);
	static int GetSpToolTipId(int index);
	static int GetReplayToolTipId(int index);

	WIN32PC_ONLY(static int GetNextShowOnLoadTipId();)
	KEYBOARD_MOUSE_ONLY(static bool GetIsToolTipKeyboardSpecific(int index);)


#if __BANK
	static void InitWidgets();
#endif // __BANK

private:

	struct movSetup
	{
		Vector2 moviePos;
		Vector2 movieSize;
		u8 alignX;
		u8 alignY;
	};

	static void ReadSetup( const char* tag, movSetup* pDest, parTreeNode* pGameStreamNode );
	static int GetTipLast( const char* tag, parTreeNode* pGameStreamNode );
	static void UpdateLoadUnload();
	static void UpdateChangeMovieParams();
	static void SetMovieLoadStatus(eLoadStat loadStatus);
	static void SetMovieID(int iMovieID);

	static void GetPosAndSize(Vector2& alignedPos, Vector2& alignedSize);


	static CGameStream*	ms_pGameStream;
	static s32 ms_GameStreamMovieId;
	static Vector2 ms_GameStreamPos;
	static Vector2 ms_GameStreamSize;
	static u8 ms_AlignX;
	static u8 ms_AlignY;


	static movSetup	ms_GameStreamIngame;
	static movSetup	ms_GameStreamLoading;

	static int ms_GameTipLast;
	static int ms_OnlineTipLast;
	static eLoadStat ms_LoadStatus;
	static int ms_UnloadUpdateCount;

	static atArray<sToolTip> m_SinglePlayerToolTips;
	static atArray<sToolTip> m_MultiPlayerToolTips;
	static atArray<sToolTip> m_ReplayToolTips;
	static Vector2 ms_vLastAlignedPos;
	static Vector2 ms_vLastAlignedSize;

#if RSG_PC
	static atArray<sToolTip> m_ShowOnLoadToolTips;
	static int ms_ShowOnLoadTipIdIndex;
#endif // RSG_PC

#if __BANK
	static void ShutdownWidgets();
	static void DebugUpdate();
	static void DebugFlushQueue();
	static void DebugForceFlushQueue();
	static void DebugForceRenderOn();
	static void DebugForceRenderOff();
	static void DebugSetShowDebugBoundsEnabled();
	static void DebugPause();
	static void DebugResume();
	static void DebugHideThisUpdate();
	static void DebugHide();
	static void DebugShow();
	static void DebugMessageTextDoPost();
	static void DebugStatsDoPost();
	static void DebugTickerDoPost();
	static void DebugTickerF10DoPost();
	static void DebugAwardDoPost();
	static void DebugCrewTagDoPost();
	static void DebugCrewRankupDoPost();
	static void DebugVersusDoPost();
	static void DebugReplayDoPost();
	static void DebugUnlockDoPost();
	static void DebugGameTipRandomPost();
	static void DebugGameTipPost();
	static void DebugCreateTheFeedBankWidgets();
	static void DebugSpinnerOn();
	static void DebugSpinnerOff();
	static void DebugSetImportantParams();
	static void DebugResetImportantParams();
	static void DebugSetNextPostBackgroundColor();
	static void	DebugSetScriptedMenuHeight();
	static void AutoPostGameTipOn();
	static void AutoPostGameTipOff();
	static void DebugClearFrozenPost();
	static void DebugUnloadGameStream();
	static void DebugReloadGameStream();
	static const char* GetLocAttempt(const char* stringToTest);
	
#endif // __BANK

};


#endif // __GAMESTREAMMGR_H__
