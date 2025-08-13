#ifndef __REPORT_MENU_H__
#define __REPORT_MENU_H__


#include "atl/singleton.h"

#include "fwnet/netplayer.h"
#include "net/status.h"
#include "rline/rlgamerinfo.h"

// Game headers
#include "ReportMenuData.h"
#include "Frontend/Scaleform/ScaleformMgr.h"
#include "Frontend/WarningScreen.h"
#include "Network/Cloud/UserContentManager.h"
#include "rline/socialclub/rlsocialclub.h"

class CRecentMissionDetails
{
public:
	char m_ContentRockstarId[64];
	atString m_ContentId;
	atString m_ContentName;
	atString m_ContentUserName;
	atString m_ContentUserId;
};

class CReportMenu : public ::rage::datBase
{

public:

	enum eMenuType
	{
		eReportMenu_Invalid = -1,
		eReportMenu_ReportMenu,
		eReportMenu_CommendMenu,
		eReportMenu_UGCMenu
	};

	CReportMenu();
	~CReportMenu();

	void Open(const rlGamerHandle& handle, eMenuType eType = eReportMenu_ReportMenu);
	void Close();

	bool IsActive()									{ return m_bActive; }
	int GetCurrentMenu()							{ return m_iSelectedMenu; }

	static void Update();
	static void Render();

	static void ResetAbuseList();

	#if RSG_PC
	void UpdateMouseEvents(const GFxValue* args);
	#endif

	rlGamerHandle& GetReportedGamerHandle()			{ return m_GamerHandle; }

	static void GenerateMPPlayerReport(const netPlayer& fromPlayer, eReportType eReportType);
private:

    static const int MAX_WARNING_MISSIONS = 5;

	void UpdateMenu();
	void RenderMenu();
	void ResetMenuAbuseList();

	bool LoadDataXMLFile(eMenuType eType);
	void UpdateMenuOptions(eReportLink eToMoveTo);
	void Populate(CReportList& list);

	void LoadReportMovie();
	void RemoveReportMovie();
	bool UpdateInput();
	void UpdateThankYouScreen();
	void UpdateConfirmationScreen();
	void ShowReportScreen(u32 uSubtitleHash, bool bAllowCancel = true);
	void SetTitle(u32 uTitleHash);
		
	// For reporting UGC complaints.
	void GenerateReport();
	void GenerateCrewReport();
	void GenerateCustomReport();
	void GenerateMPPlayerReport();
	void GenerateMissionReport();

	void PopulateMissionPage();
	void ShowKeyboard(const char* szTitle);
	bool IsReportAbusive();

	void UpdateUGCFlow();
	const char* FillReportInfo(eReportType eReportType);

	// query delegate callback
	void OnRecentMissionsContentResult(const int nIndex,
		const char* szContentID,
		const rlUgcMetadata* pMetadata,
		const rlUgcRatings* pRatings,
		const char* szStatsJSON,
		const unsigned nPlayers,
		const unsigned nPlayerIndex,
		const rlUgcPlayer* pPlayer);

private:


	void OnAcceptInput();

	struct sAbuseInfo {
		rlGamerHandle	handle;
		int				reportCount;

		public:
			void Init( rlGamerHandle& gamer, int iReportCount)
			{
				handle = gamer;
				reportCount = iReportCount;
			}

	} ;


	atArray<sAbuseInfo> m_AbuseArray;

	bool			m_bAllowCancel;
	bool			m_bActive;
	bool			m_bInactiveThisFrame;
	bool			m_bWasPauseMenuActive;
	bool			m_bInSystemKeyboard;
	
	int				m_iSelectedMissionIndex;
	int				m_iSelectedIndex;
	int				m_iSelectedMenu;
	int				m_iStoredCallbackIndex;
	int				m_iReportAbuseCountThreshold;
	int				m_iReportAbuseTimeThreshold;
	char*			m_szCustomReportInfo;
	char*			m_Name;
	bank_u32		m_iInputDisableDuration;
	eReportType		m_eReportType;
	eMenuType		m_eMenuType;
	
	eWarningButtonFlags m_ButtonFlags;

	datCallback*				m_NetStatusCallback;
	rlGamerHandle				m_GamerHandle;
    netStatus					m_NetStatus;

    static const int            MAX_SIMULTANEOUS_COMPLAINT = 16;
    static netStatus			sm_NetStatusPool[MAX_SIMULTANEOUS_COMPLAINT];

    static netStatus* GetNetStatus();

	enum eUgcMissionsOp
	{
		eUgcMissionsOp_Idle,
		eUgcMissionsOp_Pending,
		eUgcMissionsOp_Finished
	};

	CRecentMissionDetails m_ArrayOfMissions[MAX_WARNING_MISSIONS];
	u32 m_NumberOfMissionsInArray;

	rlUgc::QueryContentDelegate m_QueryMissionsDelegate;
	int m_nContentTotal;
	netStatus m_QueryMissionsStatus;
	eUgcMissionsOp m_QueryMissionsOp;


	CReportArray			m_MenuArray;
	CScaleformMovieWrapper	m_Movie;
	atArray<datCallback>	m_CallbackArray;
	atArray<eReportLink>	m_HistoryStack;
	atArray<u32>			m_NameHashArray;
};


typedef atSingleton<CReportMenu> SReportMenu;

#endif // __REPORT_MENU_H__

