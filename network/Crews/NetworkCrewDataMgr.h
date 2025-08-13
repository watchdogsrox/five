#ifndef NETWORK_CLANDATAMANAGER
#define NETWORK_CLANDATAMANAGER

#include "atl/pool.h"
#include "data/base.h"
#include "fwnet/netleaderboardread.h"
#include "fwtl/regdrefs.h"
#include "rline/clan/rlclancommon.h"

#include "Network/Live/ClanNetStatusArray.h"
#include "Network/Crews/NetworkCrewMetadata.h"
#include "Frontend/SimpleTimer.h"

using namespace rage;
namespace rage
{
#if __BANK
	class bkBank;
#endif
	class rlLeaderboard2Row;
}

struct Leaderboard;

///////////////////////////////////////////////////////////////////////////
// A class bringing together all the various bits of info users will want to know
// about Crews and whatnot
///////////////////////////////////////////////////////////////////////////
class CNetworkCrewData
{
	// to eliminate a sea of getters
	friend class CNetworkCrewDataMgr;

public:
	// These are pooled, why not.
	DECLARE_CLASS_NEW_DELETE(CNetworkCrewData);
	static void InitPool(int iSize);
	static void InitPool(int iSize, float spillover);
	static void ShutdownPool();

public:
	CNetworkCrewData();
	~CNetworkCrewData();

	bool Init(const rlClanId newId, bool requestLeaderboard = true);
	void Shutdown();

	netStatus::StatusCode Update();
	bool HasFailed() const;

	const rlClanId	GetRequestedClanId() const { return m_RequestedClan; }

	bool requestLeaderboardData();

protected:
	typedef fwRegdRef<class netLeaderboardRead>	LBRead;
	typedef ClanNetStatusArray<rlClanDesc,1>	ClanInfo;

	// because the LB results are kinda huge, we'll just siphon off the bits we care about
	struct LBXPSubset
	{
		LBXPSubset() : WorldRank(0), WorldXP(0), bReadComplete(false) {};

		s64			WorldXP;
		unsigned	WorldRank;
		bool		bReadComplete;
	};

	void ReadLBXP( const int localGamerIndex, const rlClanId newId );
	void ReleaseLB(LBRead& releaseThis);


	rlClanId					m_RequestedClan;
	ClanInfo					m_ClanInfo;
	NetworkCrewMetadata			m_Metadata;
	LBRead						m_LBXPRead;
	LBXPSubset					m_LBXPSubset;
	CSystemTimer				m_FailTimer;

	union {
		struct {
			u8					Info	 : 2;
			u8					Metadata : 2;
			u8					XPRead	 : 2;
			u8					Unused	 : 2;
		} m_Fails;

		u8 m_AllFailures;
	};

	bool m_bSuccess;
	bool m_bEmblemRefd;
	u8 m_AttemptsCount;
	bool m_requestLeaderboard;
	
	static const Leaderboard*	sm_pLBXPData;

private:
	netStatus::StatusCode StartFailTimer(const char* Reason);

	// hiding these to protect our new'd leaderboard reads
	CNetworkCrewData(const CNetworkCrewData&);
	CNetworkCrewData& operator=(const CNetworkCrewData&);

};

//////////////////////////////////////////////////////////////////////////
/* PURPOSE
	Manages crew data requests and querying, unifying the places to request things 'clan' related
*/
class CNetworkCrewDataMgr : public datBase
{
public:
	CNetworkCrewDataMgr();
	~CNetworkCrewDataMgr();
	void Init();
	void Update();
	void Shutdown(const u32 shutdownMode);
	bool OnSignOffline();

#if __BANK
	void InitWidgets(bkBank& bank);
#endif

	// used to ensure that the data's requested, though not necessarily needed at this time
	void Prefetch(const rlClanId clanToFetchDataFor);

	struct WinLossPair
	{
		WinLossPair() : wins(0), losses(0) {};
		int	wins;
		int	losses;
	};

	netStatus::StatusCode IsEverythingReadyFor( const rlClanId clanToFetchDataFor, bool bAllowRequest = true );

	netStatus::StatusCode GetClanDesc(	const rlClanId clanToFetchDataFor, const rlClanDesc*& out_Result, bool& bOUT_BecauseItsGone);
	netStatus::StatusCode GetClanWorldRank(const rlClanId clanToFetchDataFor, unsigned& out_Result);
	netStatus::StatusCode GetClanWorldXP(const rlClanId clanToFetchDataFor, s64& out_Result);

	netStatus::StatusCode GetClanH2HWinLoss(const rlClanId clanToFetchDataFor,		WinLossPair& out_Result);
	netStatus::StatusCode GetClanChallengeWinLoss(const rlClanId clanToFetchDataFor, WinLossPair& out_Result);

	netStatus::StatusCode GetClanMetadata( const rlClanId clanToFetchDataFor, const NetworkCrewMetadata*& out_Result);
	netStatus::StatusCode GetClanEmblem( const rlClanId clanToFetchDataFor, const char*& out_Result, bool requestLeaderboard);
	bool GetClanEmblemIsRetrying(const rlClanId clanToFetchDataFor);
	//netStatus::StatusCode GetClanRankTitle(const rlClanId clanToFetchDataFor, const int iDesiredRankInLevel, char* out_Result, int outSize);

	bool GetWithSeparateLeaderboardRequests() const { return m_withSeparateLeaderboardRequests; }

	void OnTunableRead();

protected:

	const CNetworkCrewData* const FindOrRequestData(const rlClanId findThis, bool bAllowRequest = true, bool requestLeaderboard = true);
	void Remove(int index);

	int	m_iMaxDataQueueSize;
	u8 m_InFlightQueries;
	atArray<CNetworkCrewData*>	m_DataQueue;

	CNetworkCrewData* m_pLastFetched;

	bool m_withSeparateLeaderboardRequests;

#if __BANK
	void	DebugUpdate();

	void	DebugAddBankWidgets( const CNetworkCrewData* const pNewData );
	void	DebugStartFetching();
	void	DebugFlush();

	int						m_DbgClanId;
	const CNetworkCrewData*	m_pDbgData;
	bkGroup*				m_pDbgGroup;
	bool					m_bDbgFetch;
#endif

};




#endif //NETWORK_CLANDATAMANAGER
