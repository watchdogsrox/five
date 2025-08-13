//
// netbandwidthmanager.h
//
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//


#ifndef NETWORK_BANDWIDTH_MGR_H
#define NETWORK_BANDWIDTH_MGR_H

#include "fwnet/netbandwidthmgr.h"

class CNetworkBandwidthTest
{
public:
    CNetworkBandwidthTest();

	void Update();

    //PURPOSE
    //  True if we have data from a previous test
    bool HasValidResult() const;

    //PURPOSE
    //  True while a test is in progress
    bool IsTestActive() const;

    //PURPOSE
    //  The test result data
    const netBandwidthTestResult GetTestResult() const;
    void ClearTestResult();

    bool TestBandwidth(bool force, netStatus* status);
    void AbortBandwidthTest();

    void ReportTestResult();

private:
    static u64 GenerateNextTestTime(const u64 lastTestTime);

private:
    u64 m_LastReportTime;
	bool m_ReportPending;
};

class CNetworkBandwidthManager : public netBandwidthMgr
{
public:

	CNetworkBandwidthManager();
	~CNetworkBandwidthManager();

    void Init(netConnectionManager *connectionMgr);
    void Update();

    //PURPOSE
    //  Called when a player leaves the session.
    void PlayerHasLeft(const netPlayer& player);

    CNetworkBandwidthTest& BandwidthTest() { return m_BandwidthTest; }

#if __BANK

    static unsigned GetDebugTargetInBandwidth();
    static unsigned GetDebugTargetOutBandwidth();

    static void AddDebugWidgets();
    static bool ShouldDisplayManagerStatusBars();

    void DisplayDebugInfo();
#endif // __BANK

private:

#if __BANK
	void DisplayTelemetryBandwidthUsage(s32 &displayX, s32 &displayY, char *str);
#endif

    virtual void OnTargetUpstreamBandwidthChanged(const unsigned bandwidth);

    //PURPOSE
    // Calculate the update levels for the players.
    //PARAMS
    // numLowLevelPlayers    - The number of player assigned a low update level
    // numMediumLevelPlayers - The number of player assigned a medium update level
    // numHighLevelPlayers   - The number of player assigned a high update level
    virtual unsigned CalculatePlayerUpdateLevels(unsigned &numLowLevelPlayers,
                                                 unsigned &numMediumLevelPlayers,
                                                 unsigned &numHighLevelPlayers);

    //PURPOSE
    // Updates the player base resend frequencies based on ping times. This is done to
    // prevent the game sending constant resend messages for players on high latency connections
    void UpdatePlayerBaseResendFrequencies();

#if __BANK
    // PURPOSE
    // Keeps track of the recent bandwidth users that have appeared in the top
    // ranked by usage list - used to prevent the sorted list changing too frequently
    // when debugging
    struct RecentTopBandwidthUsers
    {
        netBandwidthRecorderWrapper *m_Recorder;  // Bandwidth recorder
        unsigned                     m_TimeAdded; // Time added - used to keep on debug display for a fixed time interval
    };

    static const unsigned MAX_TOP_BANDWIDTH_USERS = 10;
    
    RecentTopBandwidthUsers m_TopBandwidthUsersIn [MAX_TOP_BANDWIDTH_USERS];
    RecentTopBandwidthUsers m_TopBandwidthUsersOut[MAX_TOP_BANDWIDTH_USERS];
#endif // __BANK

    unsigned m_TimePingLowerThanBaseResendFreq[MAX_NUM_PHYSICAL_PLAYERS];
    CNetworkBandwidthTest m_BandwidthTest;
};

#endif  // NETWORK_BANDWIDTH_MGR_H
