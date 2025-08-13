//
// netbandwidthmanager.cpp
//
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//

#if !__FINAL
#include "rline/rltelemetry.h"
#endif 
#include "rline/rlnpbandwidth.h"

#include "NetworkBandwidthManager.h"

#include "frontend/ProfileSettings.h"
#include "Network/Network.h"
#include "network/Cloud/Tunables.h"
#include "Network/Debug/NetworkDebug.h"
#include "Network/Live/NetworkMetrics.h"
#include "Network/Objects/NetworkObjectPopulationMgr.h"
#include "Network/Objects/Entities/NetObjAutomobile.h"
#include "Network/Objects/Synchronisation/SyncNodes/BaseSyncNodes.h"
#include "Network/Players/NetGamePlayer.h"
#include "Peds/Ped.h"

NETWORK_OPTIMISATIONS()

#if __BANK

PARAM(netforcebandwidthtest, "[network] Force the test to run on each call", "Disabled", "All", "");

// contains settings for displaying debug information relating to bandwidth
struct BandwidthDebugSettings
{
    static const unsigned int MIN_TARGET_OUT_BANDWIDTH = 64 * 1024;
    static const unsigned int MAX_TARGET_OUT_BANDWIDTH = 1024 * 1024;
    static const unsigned int MIN_TARGET_IN_BANDWIDTH  = 64 * 1024;
    static const unsigned int MAX_TARGET_IN_BANDWIDTH  = 1024 * 1024;
    static const unsigned int TARGET_BANDWIDTH_STEP    = 1000;
    static const unsigned int SYNC_ELEMENTS_TO_DISPLAY_DEFAULT = 5;
    static const unsigned int MIN_SYNC_ELEMENTS_TO_DISPLAY     = 1;
    static const unsigned int MAX_SYNC_ELEMENTS_TO_DISPLAY     = 30;
    static const unsigned int SYNC_ELEMENTS_TO_DISPLAY_STEP    = 1;

    BandwidthDebugSettings() :
    m_targetInBandwidth(MIN_TARGET_OUT_BANDWIDTH),
    m_targetOutBandwidth(MIN_TARGET_IN_BANDWIDTH),
    m_displayBandwidthUsage(false),
    m_DisplayManagerBandwidthUsage(false),
    m_DisplayPositionalBandwidthUsage(false),
    m_DisplayThrottlingData(false),
    m_DisplaySendQueueLengths(false),
    m_numSyncElementsToDisplay(SYNC_ELEMENTS_TO_DISPLAY_DEFAULT)
    {
    }

    unsigned int m_targetInBandwidth;
    unsigned int m_targetOutBandwidth;
	bool         m_displayBandwidthUsage;
	bool         m_displayTelemetryBandwidthUsage;
    bool         m_DisplayManagerBandwidthUsage;
    bool         m_DisplayPositionalBandwidthUsage;
    bool         m_DisplayThrottlingData;
    bool         m_DisplaySendQueueLengths;
    unsigned int m_numSyncElementsToDisplay;
};

static BandwidthDebugSettings s_netBandwidthDebugSettings;

static unsigned s_CachedNewQueueLengths[MAX_NUM_PHYSICAL_PLAYERS];
static unsigned s_CachedResendQueueLengths[MAX_NUM_PHYSICAL_PLAYERS];
static unsigned s_LastTimeSendQueueLengthsUpdated = 0;

#endif // __BANK

CNetworkBandwidthManager::CNetworkBandwidthManager()
{
}

CNetworkBandwidthManager::~CNetworkBandwidthManager()
{
}

void CNetworkBandwidthManager::Init(netConnectionManager *connectionMgr)
{
    netBandwidthMgr::Init(connectionMgr);

    SetAllowBandwidthThrottling(Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("ENABLE_BANDWIDTH_THROTTLING", 0xfc70fcac), false));

    CProjectBaseSyncDataNode::ResetBaseResendFrequencies();

    for(PhysicalPlayerIndex playerIndex = 0; playerIndex < MAX_NUM_PHYSICAL_PLAYERS; playerIndex++)
    {
        m_TimePingLowerThanBaseResendFreq[playerIndex] = 0;
    }
}

void CNetworkBandwidthManager::Update()
{
#if __BANK
		// we set both the target in and out values to the target upstream bandwidth so the bars are in the same range
		s_netBandwidthDebugSettings.m_targetInBandwidth  = GetTargetUpstreamBandwidth() << 10;
		s_netBandwidthDebugSettings.m_targetOutBandwidth = GetTargetUpstreamBandwidth() << 10;
#endif // __BANK

    netBandwidthMgr::Update();
	m_BandwidthTest.Update();
}

void CNetworkBandwidthManager::PlayerHasLeft(const netPlayer& player)
{
    PhysicalPlayerIndex playerIndex = player.GetPhysicalPlayerIndex();

    if(playerIndex < MAX_NUM_PHYSICAL_PLAYERS)
    {
        CProjectBaseSyncDataNode::SetBaseResendFrequency(playerIndex, CProjectBaseSyncDataNode::UPDATE_RATE_RESEND_UNACKED);

        m_TimePingLowerThanBaseResendFreq[playerIndex] = 0;
    }
}

#if __BANK

unsigned CNetworkBandwidthManager::GetDebugTargetInBandwidth()
{
    return s_netBandwidthDebugSettings.m_targetInBandwidth;
}

unsigned CNetworkBandwidthManager::GetDebugTargetOutBandwidth()
{
    return s_netBandwidthDebugSettings.m_targetOutBandwidth;
}

static void NetworkBank_SetSendInterval()
{
    CNetwork::GetBandwidthManager().SetSendInterval(CNetwork::GetBandwidthManager().GetSendInterval());
}

void CNetworkBandwidthManager::AddDebugWidgets()
{
    bkBank *bank = BANKMGR.FindBank("Network");

    if(gnetVerifyf(bank, "Unable to find network bank!"))
    {
        bank->PushGroup("Debug Bandwidth", false);
			bank->AddToggle("Display bandwidth usage",                &s_netBandwidthDebugSettings.m_displayBandwidthUsage);
			bank->AddToggle("Display Telemetry bandwidth usage",      &s_netBandwidthDebugSettings.m_displayTelemetryBandwidthUsage);
            bank->AddToggle("Display manager bandwidth usage",        &s_netBandwidthDebugSettings.m_DisplayManagerBandwidthUsage);
            bank->AddToggle("Display positional bandwidth usage",     &s_netBandwidthDebugSettings.m_DisplayPositionalBandwidthUsage);
            bank->AddToggle("Display throttling data",                &s_netBandwidthDebugSettings.m_DisplayThrottlingData);
            bank->AddToggle("Display send queue lengths",             &s_netBandwidthDebugSettings.m_DisplaySendQueueLengths);
            bank->AddToggle("Override Target Upstream Bandwidth",     &CNetwork::GetBandwidthManager().m_OverrideTargetUpstreamBandwidth);
            bank->AddSlider("Overridden Target Upstream Bandwidth",   &CNetwork::GetBandwidthManager().m_OverriddenTargetUpstreamBandwidth, /*CNetwork::GetBandwidthManager().MINIMUM_BANDWIDTH*/4, CNetwork::GetBandwidthManager().MAXIMUM_BANDWIDTH, /*16*/4);
            bank->AddSlider("Target outgoing bandwidth",              &s_netBandwidthDebugSettings.m_targetOutBandwidth, BandwidthDebugSettings::MIN_TARGET_OUT_BANDWIDTH, BandwidthDebugSettings::MAX_TARGET_OUT_BANDWIDTH, BandwidthDebugSettings::TARGET_BANDWIDTH_STEP);
            bank->AddSlider("Target incoming bandwidth",              &s_netBandwidthDebugSettings.m_targetInBandwidth,  BandwidthDebugSettings::MIN_TARGET_IN_BANDWIDTH,  BandwidthDebugSettings::MAX_TARGET_IN_BANDWIDTH,  BandwidthDebugSettings::TARGET_BANDWIDTH_STEP);
#if __DEV
            bank->AddSlider("Bandwidth increase interval",            &BANDWIDTH_ADVANCE_INTERVAL, 0, 30000, 500);
#endif
            bank->AddSlider("Reliable resend threshold",              &RELIABLE_RESEND_THRESHOLD, 1, 100, 1);
            bank->AddSlider("Unreliable resend threshold",            &UNRELIABLE_RESEND_THRESHOLD, 1, 1000, 1);

            bank->AddSlider("Num sync elements to display",           &s_netBandwidthDebugSettings.m_numSyncElementsToDisplay, BandwidthDebugSettings::MIN_SYNC_ELEMENTS_TO_DISPLAY, BandwidthDebugSettings::MAX_SYNC_ELEMENTS_TO_DISPLAY, BandwidthDebugSettings::SYNC_ELEMENTS_TO_DISPLAY_STEP);
            bank->AddSlider("Sync Failure Threshold",                 &CNetworkObjectMgr::m_syncFailureThreshold, 1, 20, 1);
            bank->AddSlider("Max Starving Object Threshold",          &CNetworkObjectMgr::m_maxStarvingObjectThreshold, 1, 20, 1);
            bank->AddSlider("Max Objects Dumped Per Update",          &CNetworkObjectMgr::m_maxObjectsToDumpPerUpdate, 1, 20, 1);
            bank->AddSlider("Dump Excess Object Interval",            &CNetworkObjectMgr::m_dumpExcessObjectInterval, 0, 10000, 50);
            bank->AddSlider("Delay Before Deleting Excess Objects",   &CNetworkObjectMgr::m_DelayBeforeDeletingExcessObjects, 0, 300000, 1000);
            bank->AddSlider("Too Many Objects ACK Timeout Balancing", &CNetworkObjectMgr::m_tooManyObjectsACKTimeoutBalancing, 5000, 300000, 1000);
            bank->AddSlider("Too Many Objects ACK Timeout Proximity", &CNetworkObjectMgr::m_tooManyObjectsACKTimeoutProximity, 1000, 5000, 500);
#if __DEV
            bank->AddSlider("Dumped Ambient Entity Timer",            &DUMPED_AMBIENT_ENTITY_TIMER, 1000, 30000, 100);
            bank->AddSlider("Dumped Ambient Entity Increase Interval",&DUMPED_AMBIENT_ENTITY_INCREASE_INTERVAL, 0, 10000, 100);
#endif

            bank->AddToggle("Override Player Bandwidth Targets", &CNetwork::GetBandwidthManager().m_OverridePlayerBandwidthTargets);
            bank->PushGroup("Player Bandwidth Targets", false);
                for(PhysicalPlayerIndex index = 0; index < MAX_NUM_PHYSICAL_PLAYERS; index++)
                {
                    char msg[128];
                    sprintf(msg, "Player %d", index);
                    bank->AddSlider(msg, &CNetwork::GetBandwidthManager().m_PlayerBandwidthInfo[index].m_TargetBandwidth, 0, 10240000, 1000);
                }
            bank->PopGroup();
            bank->AddSlider("Send Interval (ms)", &CNetwork::GetBandwidthManager().m_SendInterval, 0, 1000, 1, datCallback(NetworkBank_SetSendInterval));
        bank->PopGroup();
    }
}

bool CNetworkBandwidthManager::ShouldDisplayManagerStatusBars()
{
    return s_netBandwidthDebugSettings.m_DisplayManagerBandwidthUsage;
}

void CNetworkBandwidthManager::DisplayDebugInfo()
{
    static bank_s32 displayX      = 5;
    static bank_s32 displayStartY = 22;
    static bank_s32 offsetX       = 75;

    const unsigned MAX_DEBUG_TEXT = 128;
    char str[MAX_DEBUG_TEXT];

    s32 displayY = displayStartY;

	if (!NetworkInterface::IsGameInProgress())
	{
		DisplayTelemetryBandwidthUsage(displayX, displayY, str);
		return;
	}

    if(s_netBandwidthDebugSettings.m_displayBandwidthUsage)
    {
        float gameIn	= netBandwidthStatistics::GetTotalBandwidthInRate();
	    float gameOut	= netBandwidthStatistics::GetTotalBandwidthOutRate();

        const char *highestInName   = "";
        const char *highestOutName  = "";
        float       highestInValue  = 0.0f;
        float       highestOutValue = 0.0f;

        unsigned currTime   = fwTimer::GetSystemTimeInMilliseconds();
        bool     exists     = false;
        unsigned freeSlot   = MAX_TOP_BANDWIDTH_USERS;
        unsigned oldestSlot = MAX_TOP_BANDWIDTH_USERS;

        for(unsigned bandwidthIndex = 0; bandwidthIndex < s_netBandwidthDebugSettings.m_numSyncElementsToDisplay; bandwidthIndex++)
        {
            // Update incoming bandwidth stats
            netBandwidthRecorderWrapper *recorder = netBandwidthStatistics::GetHighestBandwidthIn (bandwidthIndex, highestInName,  highestInValue);

            if(recorder && highestInValue > 0.0f)
            {
                exists     = false;
                freeSlot   = MAX_TOP_BANDWIDTH_USERS;
                oldestSlot = MAX_TOP_BANDWIDTH_USERS;

                for(unsigned index = 0; index < MAX_TOP_BANDWIDTH_USERS && !exists; index++)
                {
                    if(m_TopBandwidthUsersIn[index].m_Recorder == recorder)
                    {
                        m_TopBandwidthUsersIn[index].m_TimeAdded = currTime;
                        exists                                   = true;
                    }
                    else if(freeSlot == MAX_TOP_BANDWIDTH_USERS)
                    {
                        if(m_TopBandwidthUsersIn[index].m_Recorder == 0)
                        {
                            freeSlot = index;
                        }
                    }
                    else
                    {
                        if(oldestSlot == MAX_TOP_BANDWIDTH_USERS || (m_TopBandwidthUsersIn[index].m_TimeAdded < m_TopBandwidthUsersIn[oldestSlot].m_TimeAdded))
                        {
                            oldestSlot = index;
                        }
                    }
                }

                if(!exists)
                {
                    if(freeSlot != MAX_TOP_BANDWIDTH_USERS)
                    {
                        m_TopBandwidthUsersIn[freeSlot].m_Recorder  = recorder;
                        m_TopBandwidthUsersIn[freeSlot].m_TimeAdded = currTime;
                    }
                    else
                    {
                        m_TopBandwidthUsersIn[oldestSlot].m_Recorder  = recorder;
                        m_TopBandwidthUsersIn[oldestSlot].m_TimeAdded = currTime;
                    }
                }
            }

            // Update outgoing bandwidth stats
            recorder = netBandwidthStatistics::GetHighestBandwidthOut(bandwidthIndex, highestOutName, highestOutValue);

            if(recorder && highestOutValue > 0.0f)
            {
                exists     = false;
                freeSlot   = MAX_TOP_BANDWIDTH_USERS;
                oldestSlot = MAX_TOP_BANDWIDTH_USERS;

                for(unsigned index = 0; index < MAX_TOP_BANDWIDTH_USERS && !exists; index++)
                {
                    if(m_TopBandwidthUsersOut[index].m_Recorder == recorder)
                    {
                        m_TopBandwidthUsersOut[index].m_TimeAdded = currTime;
                        exists                                    = true;
                    }
                    else if(freeSlot == MAX_TOP_BANDWIDTH_USERS)
                    {
                        if(m_TopBandwidthUsersOut[index].m_Recorder == 0)
                        {
                            freeSlot = index;
                        }
                    }
                    else
                    {
                        if(oldestSlot == MAX_TOP_BANDWIDTH_USERS || (m_TopBandwidthUsersOut[index].m_TimeAdded < m_TopBandwidthUsersOut[oldestSlot].m_TimeAdded))
                        {
                            oldestSlot = index;
                        }
                    }
                }

                if(!exists)
                {
                    if(freeSlot != MAX_TOP_BANDWIDTH_USERS)
                    {
                        m_TopBandwidthUsersOut[freeSlot].m_Recorder  = recorder;
                        m_TopBandwidthUsersOut[freeSlot].m_TimeAdded = currTime;
                    }
                    else
                    {
                        m_TopBandwidthUsersOut[oldestSlot].m_Recorder  = recorder;
                        m_TopBandwidthUsersOut[oldestSlot].m_TimeAdded = currTime;
                    }
                }
            }
        }

        // check for expired slots
        const bank_u32 TIME_TO_KEEP_TOP_BANDWIDTH_USERS = 10000;

        for(unsigned index = 0; index < MAX_TOP_BANDWIDTH_USERS; index++)
        {
            if(m_TopBandwidthUsersIn[index].m_Recorder != 0)
            {
                unsigned timeAdded   = m_TopBandwidthUsersIn[index].m_TimeAdded;
                unsigned timeElapsed = currTime - timeAdded;

                if(timeElapsed > TIME_TO_KEEP_TOP_BANDWIDTH_USERS)
                {
                    m_TopBandwidthUsersIn[index].m_Recorder  = 0;
                    m_TopBandwidthUsersIn[index].m_TimeAdded = 0;
                }
            }

            if(m_TopBandwidthUsersOut[index].m_Recorder != 0)
            {
                unsigned timeAdded   = m_TopBandwidthUsersOut[index].m_TimeAdded;
                unsigned timeElapsed = currTime - timeAdded;

                if(timeElapsed > TIME_TO_KEEP_TOP_BANDWIDTH_USERS)
                {
                    m_TopBandwidthUsersOut[index].m_Recorder  = 0;
                    m_TopBandwidthUsersOut[index].m_TimeAdded = 0;
                }
            }
        }

        sprintf(str, "Total game in : %.2f (%.2f Kb/s)", gameIn, gameIn/1024.0f);
        grcDebugDraw::PrintToScreenCoors(str, displayX, displayY);
        sprintf(str, "Total game out : %.2f (%.2f Kb/s)", gameOut, gameOut/1024.0f);
        grcDebugDraw::PrintToScreenCoors(str, displayX + offsetX, displayY++);
        displayY++;

        for(unsigned index = 0; index < MAX_TOP_BANDWIDTH_USERS; index++)
        {
            bool increaseY = false;

            if (m_TopBandwidthUsersIn[index].m_Recorder)
			{
                float highestInValue = m_TopBandwidthUsersIn[index].m_Recorder->GetBandwidthInRate();
				sprintf(str, "%-25s : %.2f", m_TopBandwidthUsersIn[index].m_Recorder->GetName(), highestInValue);
				grcDebugDraw::PrintToScreenCoors(str, displayX, displayY);

                increaseY = true;
			}

			if (m_TopBandwidthUsersOut[index].m_Recorder)
			{
                float highestOutValue = m_TopBandwidthUsersOut[index].m_Recorder->GetBandwidthOutRate();
				sprintf(str, "%-25s : %.2f", m_TopBandwidthUsersOut[index].m_Recorder->GetName(), highestOutValue);
				grcDebugDraw::PrintToScreenCoors(str, displayX + offsetX, displayY);

                increaseY = true;
			}

            if (increaseY)
			{
				displayY++;
			}
        }

        displayY++;

        // now display the size of the reliable message queues
		unsigned                 numRemotePhysicalPlayers = netInterface::GetNumRemotePhysicalPlayers();
        const netPlayer * const *remotePhysicalPlayers    = netInterface::GetRemotePhysicalPlayers();
        
	    for(unsigned index = 0; index < numRemotePhysicalPlayers; index++)
        {
            const netPlayer *pPlayer = remotePhysicalPlayers[index];

            int connectionID = pPlayer->GetConnectionId();

            unsigned numUnackedMessagesQueued = CNetwork::GetConnectionManager().GetUnAckedCount(connectionID);

            if(numUnackedMessagesQueued > 0)
            {
                sprintf(str, "%-25s : %-5d reliable messages queued", pPlayer->GetLogName(), numUnackedMessagesQueued);
                grcDebugDraw::PrintToScreenCoors(str, displayX, displayY++);
            }
        }

        displayY++;
    }

	DisplayTelemetryBandwidthUsage(displayX, displayY, str);

    if(s_netBandwidthDebugSettings.m_DisplayPositionalBandwidthUsage)
    {
        CPedSyncTree *pedSyncTree = SafeCast(CPedSyncTree, CNetObjPed::GetStaticSyncTree());

        if(pedSyncTree)
        {
            if(pedSyncTree->GetSectorNode())
            {
                RecorderId sectorRecorderID = pedSyncTree->GetSectorNode()->GetBandwidthRecorderID();

                float sectorInValue  = CNetwork::GetObjectManager().GetBandwidthStatistics().GetBandwidthInRate(sectorRecorderID);
                float sectorOutValue = CNetwork::GetObjectManager().GetBandwidthStatistics().GetBandwidthOutRate(sectorRecorderID);

		        sprintf(str, "%-25s : %.2f", "Sector In", sectorInValue);
		        grcDebugDraw::PrintToScreenCoors(str, displayX, displayY);
		        sprintf(str, "%-25s : %.2f", "Sector Out", sectorOutValue);
		        grcDebugDraw::PrintToScreenCoors(str, displayX + offsetX, displayY);
                displayY++;
            }

            if(pedSyncTree->GetMapPositionNode() && pedSyncTree->GetNavMeshPositionNode())
            {
                RecorderId sectorMapPosRecorderID     = pedSyncTree->GetMapPositionNode()->GetBandwidthRecorderID();
                RecorderId sectorNavMeshPosRecorderID = pedSyncTree->GetNavMeshPositionNode()->GetBandwidthRecorderID();

                float sectorMapPosInValue      = CNetwork::GetObjectManager().GetBandwidthStatistics().GetBandwidthInRate(sectorMapPosRecorderID);
                float sectorMapPosOutValue     = CNetwork::GetObjectManager().GetBandwidthStatistics().GetBandwidthOutRate(sectorMapPosRecorderID);
                float sectorNavMeshPosInValue  = CNetwork::GetObjectManager().GetBandwidthStatistics().GetBandwidthInRate(sectorNavMeshPosRecorderID);
                float sectorNavMeshPosOutValue = CNetwork::GetObjectManager().GetBandwidthStatistics().GetBandwidthOutRate(sectorNavMeshPosRecorderID);

                float sectorPosInValue  = sectorMapPosInValue  + sectorNavMeshPosInValue;
                float sectorPosOutValue = sectorMapPosOutValue + sectorNavMeshPosOutValue;
                sprintf(str, "%-25s : %.2f", "Ped Sector Pos In", sectorPosInValue);
		        grcDebugDraw::PrintToScreenCoors(str, displayX, displayY);
		        sprintf(str, "%-25s : %.2f", "Ped Sector Pos Out", sectorPosOutValue);
		        grcDebugDraw::PrintToScreenCoors(str, displayX + offsetX, displayY);
                displayY++;
            }
        }

        CPhysicalSyncTreeBase *autoSyncTree = SafeCast(CPhysicalSyncTreeBase, CNetObjAutomobile::GetStaticSyncTree());

        if(autoSyncTree)
        {
            if(autoSyncTree->GetOrientationNode())
            {
                RecorderId orientationRecorderID = autoSyncTree->GetOrientationNode()->GetBandwidthRecorderID();

                float orientationInValue  = CNetwork::GetObjectManager().GetBandwidthStatistics().GetBandwidthInRate(orientationRecorderID);
                float orientationOutValue = CNetwork::GetObjectManager().GetBandwidthStatistics().GetBandwidthOutRate(orientationRecorderID);

                sprintf(str, "%-25s : %.2f", "Orientation In", orientationInValue);
		        grcDebugDraw::PrintToScreenCoors(str, displayX, displayY);
		        sprintf(str, "%-25s : %.2f", "Orientation Out", orientationOutValue);
		        grcDebugDraw::PrintToScreenCoors(str, displayX + offsetX, displayY);
            }
        }
    }

    if(s_netBandwidthDebugSettings.m_DisplayThrottlingData)
    {
        u32                      currentTime              = sysTimer::GetSystemMsTime();
        unsigned                 numRemotePhysicalPlayers = netInterface::GetNumRemotePhysicalPlayers();
        const netPlayer * const *remotePhysicalPlayers    = netInterface::GetRemotePhysicalPlayers();

        for(unsigned index = 0; index < numRemotePhysicalPlayers; index++)
        {
            const netPlayer *pPlayer = remotePhysicalPlayers[index];

            if(pPlayer)
            {
                PhysicalPlayerIndex playerIndex = pPlayer->GetPhysicalPlayerIndex();

                if(playerIndex != INVALID_PLAYER_INDEX)
                {
                    u32 timeSinceLastMessage = (currentTime > m_PlayerThrottleData[playerIndex].m_LastMessageReceivedTime) ? (currentTime - m_PlayerThrottleData[playerIndex].m_LastMessageReceivedTime) : 0;
                    formatf(str, "%s : Time since last message: %d, Throttle timer: %d", pPlayer->GetLogName(),  timeSinceLastMessage, m_PlayerThrottleData[playerIndex].m_PreventThrottleTimer);
                    grcDebugDraw::PrintToScreenCoors(str, displayX, displayY);
                    
                    displayY++;
                }
            }
        }
    }

    if(s_netBandwidthDebugSettings.m_DisplaySendQueueLengths)
    {
        const unsigned UPDATE_INTERVAL = 500;

        bool timeToUpdateCachedSendQueues = (sysTimer::GetSystemMsTime() - s_LastTimeSendQueueLengthsUpdated) > UPDATE_INTERVAL;

        if(timeToUpdateCachedSendQueues)
        {
            for(unsigned index = 0; index < MAX_NUM_PHYSICAL_PLAYERS; index++)
            {
				s_CachedNewQueueLengths[index] = 0;
				s_CachedResendQueueLengths[index] = 0;
            }

            s_LastTimeSendQueueLengthsUpdated = sysTimer::GetSystemMsTime();
        }

        unsigned                 numRemotePhysicalPlayers = netInterface::GetNumRemotePhysicalPlayers();
        const netPlayer * const *remotePhysicalPlayers    = netInterface::GetRemotePhysicalPlayers();

        for(unsigned index = 0; index < numRemotePhysicalPlayers; index++)
        {
            const netPlayer *pPlayer = remotePhysicalPlayers[index];

            if(pPlayer)
            {
                PhysicalPlayerIndex playerIndex  = pPlayer->GetPhysicalPlayerIndex();
                int                 connectionID = pPlayer->GetConnectionId();

                if(connectionID != -1 && playerIndex < MAX_NUM_PHYSICAL_PLAYERS)
                {
                    if(timeToUpdateCachedSendQueues)
                    {
                        unsigned newQendQueueLength = CNetwork::GetConnectionManager().GetNewQueueLength(connectionID);

                        s_CachedNewQueueLengths[playerIndex] = newQendQueueLength;

						unsigned resendQueueLength = CNetwork::GetConnectionManager().GetResendQueueLength(connectionID);

						s_CachedResendQueueLengths[playerIndex] = resendQueueLength;
                    }

                    formatf(str, "%s : Send Queue Lengths: New: %d, Resend: %d", pPlayer->GetLogName(), s_CachedNewQueueLengths[playerIndex], s_CachedResendQueueLengths[playerIndex]);
                    grcDebugDraw::PrintToScreenCoors(str, displayX, displayY);

                    displayY++;
                }
            }
        }
    }
}

void CNetworkBandwidthManager::DisplayTelemetryBandwidthUsage(s32 &displayX, s32 &displayY, char* str)
{
	if(s_netBandwidthDebugSettings.m_displayTelemetryBandwidthUsage)
	{
#if !__FINAL
		rlTelemetryBandwidth& telemetryBandwidth = rlTelemetry::GetBandwidthRecords();

		for (int i=0; i<telemetryBandwidth.m_records.GetCount(); i++)
		{
			rlTelemetryBandwidth::sRecord& record = telemetryBandwidth.m_records[i];
			rlTelemetryBandwidth::sRecord& recordTotal = telemetryBandwidth.m_totalHistory[i];

			const u32 elapsed = sysTimer::GetSystemMsTime() - record.m_time;
			const float sentkb = (float)record.m_bytesSent/1024;

			Color32 col = Color32(0.90f,0.90f,0.90f);
			if(elapsed<=1000)
				col = Color32(0.80f,0.36f,0.36f);

			sprintf(str, "Telemetry (%s) : Compression ratio: %d/%d = %0.2f%% (%0.2f to 1, %.2f Kb, [%.2f Kb])"
				,telemetryBandwidth.GetRecordName((rlTelemetryBandwidth::eRecordTypes)i)
				,record.m_produced
				,record.m_consumed
				,100*(1.0-((double)record.m_produced/record.m_consumed))
				,(double)record.m_consumed/record.m_produced
				,sentkb
				,(elapsed<=1000) ? sentkb : 0.0f);

			grcDebugDraw::PrintToScreenCoors(str, displayX, displayY++, col);

			sprintf(str, "                    TOTAL :  %d/%d = %0.2f%% (%0.2f to 1, %.2f Kb)"
				,recordTotal.m_produced
				,recordTotal.m_consumed
				,100*(1.0-((double)recordTotal.m_produced/recordTotal.m_consumed))
				,(double)recordTotal.m_consumed/recordTotal.m_produced
				,(float)recordTotal.m_bytesSent/1024);

			grcDebugDraw::PrintToScreenCoors(str, displayX, displayY++);
			displayY++;
		}
#endif // !__FINAL
	}
}

#endif // __BANK

void CNetworkBandwidthManager::OnTargetUpstreamBandwidthChanged(const unsigned bandwidth)
{
    CNetworkObjectPopulationMgr::SetMaxBandwidthTargetLevels(bandwidth);
}

unsigned CNetworkBandwidthManager::CalculatePlayerUpdateLevels(unsigned &numLowLevelPlayers,
                                                               unsigned &numMediumLevelPlayers,
                                                               unsigned &numHighLevelPlayers)
{
    unsigned numPhysicalPlayers = 0;

    CPed * localPlayerPed = FindPlayerPed();

	unsigned                 numRemotePhysicalPlayers = netInterface::GetNumRemotePhysicalPlayers();
    const netPlayer * const *remotePhysicalPlayers    = netInterface::GetRemotePhysicalPlayers();
    
    for(unsigned index = 0; index < numRemotePhysicalPlayers; index++)
    {
        const CNetGamePlayer *pPlayer = SafeCast(const CNetGamePlayer, remotePhysicalPlayers[index]);

		if (localPlayerPed && pPlayer->GetPlayerPed())
		{
            const float LOW_UPDATE_DIST    =  400.0f;
            const float MEDIUM_UPDATE_DIST =  200.0f;

			float   distanceSquared = DistSquared(pPlayer->GetPlayerPed()->GetTransform().GetPosition(), localPlayerPed->GetTransform().GetPosition()).Getf();

			if(distanceSquared > rage::square(LOW_UPDATE_DIST))
			{
				m_PlayerBandwidthInfo[pPlayer->GetPhysicalPlayerIndex()].m_UpdateLevel = PlayerBandwidthData::LOW_UPDATE_LEVEL;
				numLowLevelPlayers++;
			}
			else if(distanceSquared > rage::square(MEDIUM_UPDATE_DIST))
			{
				m_PlayerBandwidthInfo[pPlayer->GetPhysicalPlayerIndex()].m_UpdateLevel = PlayerBandwidthData::MEDIUM_UPDATE_LEVEL;
				numMediumLevelPlayers++;
			}
			else
			{
				m_PlayerBandwidthInfo[pPlayer->GetPhysicalPlayerIndex()].m_UpdateLevel = PlayerBandwidthData::HIGH_UPDATE_LEVEL;
				numHighLevelPlayers++;
			}

			numPhysicalPlayers++;
		}
		else
		{
			// players that have not cloned their peds yet are automatically give a low update level
			m_PlayerBandwidthInfo[pPlayer->GetPhysicalPlayerIndex()].m_UpdateLevel = PlayerBandwidthData::LOW_UPDATE_LEVEL;
			numLowLevelPlayers++;
			numPhysicalPlayers++;
		}
	}

    return numPhysicalPlayers;
}

void CNetworkBandwidthManager::UpdatePlayerBaseResendFrequencies()
{
    const unsigned LATENCY_RESEND_REDUCE_THRESHOLD = 100;

    PhysicalPlayerIndex localPhysicalPlayerIndex = NetworkInterface::GetLocalPhysicalPlayerIndex();

    // check players ping times and adjust resend interval appropriately
    for(PhysicalPlayerIndex playerIndex = 0; playerIndex < MAX_NUM_PHYSICAL_PLAYERS; playerIndex++)
    {
        if(playerIndex != localPhysicalPlayerIndex)
        {
            u32 averageRTT     = static_cast<u32>(NetworkInterface::GetAverageSyncRTT(playerIndex));
            u32 baseResendFreq = CProjectBaseSyncDataNode::GetBaseResendFrequency(playerIndex);

            if(averageRTT > baseResendFreq)
            {
                const u32 EXTRA_PING_DELAY = 50;

                u32 newResendFrequency = Clamp(averageRTT + EXTRA_PING_DELAY, (u32)CProjectBaseSyncDataNode::UPDATE_RATE_RESEND_UNACKED, (u32)CProjectBaseSyncDataNode::MAX_BASE_RESEND_FREQUENCY);

                CProjectBaseSyncDataNode::SetBaseResendFrequency(playerIndex, newResendFrequency);

                m_TimePingLowerThanBaseResendFreq[playerIndex] = 0;
            }
            else if(baseResendFreq != CProjectBaseSyncDataNode::UPDATE_RATE_RESEND_UNACKED)
            {
                if(averageRTT < (baseResendFreq - LATENCY_RESEND_REDUCE_THRESHOLD))
                {
                    const unsigned TIME_BEFORE_REDUCING_RESEND = 10000;

                    // wait a while before lowering the resend frequency to take into account of spiky connections
                    m_TimePingLowerThanBaseResendFreq[playerIndex] += fwTimer::GetSystemTimeStepInMilliseconds();

                    if(m_TimePingLowerThanBaseResendFreq[playerIndex] > TIME_BEFORE_REDUCING_RESEND)
                    {
                        u32 newResendFrequency = Clamp(baseResendFreq - LATENCY_RESEND_REDUCE_THRESHOLD, (u32)CProjectBaseSyncDataNode::UPDATE_RATE_RESEND_UNACKED, (u32)CProjectBaseSyncDataNode::MAX_BASE_RESEND_FREQUENCY);

                        CProjectBaseSyncDataNode::SetBaseResendFrequency(playerIndex, newResendFrequency);

                        m_TimePingLowerThanBaseResendFreq[playerIndex] = 0;
                    }
                }
                else
                {
                    m_TimePingLowerThanBaseResendFreq[playerIndex] = 0;
                }
            }
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
//  CNetworkBandwidthTest
///////////////////////////////////////////////////////////////////////////////
CNetworkBandwidthTest::CNetworkBandwidthTest()
    : m_LastReportTime(0)
	, m_ReportPending(false)
{

}

bool CNetworkBandwidthTest::HasValidResult() const
{
#if RL_NP_BANDWIDTH
    return g_rlNp.GetNpBandwidth().HasValidResult();
#else
    return false;
#endif
}

bool CNetworkBandwidthTest::IsTestActive() const
{
#if RL_NP_BANDWIDTH
    return g_rlNp.GetNpBandwidth().IsTestActive();
#else
    return false;
#endif
}

const netBandwidthTestResult CNetworkBandwidthTest::GetTestResult() const
{
#if RL_NP_BANDWIDTH
    return g_rlNp.GetNpBandwidth().GetTestResult();
#else
    return netBandwidthTestResult();
#endif
}

void CNetworkBandwidthTest::ClearTestResult()
{
#if RL_NP_BANDWIDTH
    g_rlNp.GetNpBandwidth().ClearTestResult();
#endif
}

void CNetworkBandwidthTest::Update()
{
	if (!m_ReportPending || IsTestActive())
	{
		return;
	}

	// Always report, including when it failed
	ReportTestResult();
}

bool CNetworkBandwidthTest::TestBandwidth(bool force, netStatus* RL_NP_BANDWIDTH_ONLY(status))
{
    rtry
    {
        BANK_ONLY(force = force || PARAM_netforcebandwidthtest.Get());

        rcheck(Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("TEST_BANDWITH", 0xCB4F69AA), true), catchall, gnetDebug1("The bandwidth test is disabled"));

        CProfileSettings& settings = CProfileSettings::GetInstance();
        rcheck(settings.AreSettingsValid() || force, catchall, gnetWarning("TestBandwidth - Profile save data is not available"));

        const u64 timeNow = rlGetPosixTime();
        const u64 posixTime = (u64)settings.GetInt(CProfileSettings::BANDWIDTH_TEST_TIME);

        rcheck((timeNow > posixTime) || force, catchall, gnetDebug1("TestBandwidth - The next allowed test is at %" I64FMT "u", posixTime));

        bool ok = false;

#if RL_NP_BANDWIDTH
        ok = g_rlNp.GetNpBandwidth().TestBandwidth(status);
#endif

        // We count a started test as a valid test without awaiting a successful test
        if (ok)
        {
			m_ReportPending = true;
			
			// Since it's a bit randomized we store the next test time, not the last one. Can be changed should it cause any issues.
            const u64 nextTime = GenerateNextTestTime(timeNow);
            gnetDebug1("TestBandwidth - The next bandwidth test will be at %" I64FMT "u", nextTime);

            // Will most likely be saved as soon as the loading ends so no need to force a safe
            settings.Set(CProfileSettings::BANDWIDTH_TEST_TIME, (int)nextTime);
        }

        return ok;
    }
    rcatchall
    {
    }

    return false;
}

u64 CNetworkBandwidthTest::GenerateNextTestTime(const u64 lastTestTime)
{
    const int DEFAULT_DELAY_HOURS = (24 * 4); // 4 days
    const int RANGE_HOURS = (24 * 3); // 3 days
    const int interSecMin = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("BANDWIDTH_TEST_INTERVAL_MIN", 0xD0708F3D), DEFAULT_DELAY_HOURS) * 60 * 60;
    const int interSecRange = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("BANDWIDTH_TEST_INTERVAL_RANGE", 0x849750CF), RANGE_HOURS) * 60 * 60;

    const int seconds = netRandom::GetRangedCrypto(interSecMin, interSecMin + interSecRange);

    return lastTestTime + (u64)seconds;
}

void CNetworkBandwidthTest::AbortBandwidthTest()
{
#if RL_NP_BANDWIDTH
    return g_rlNp.GetNpBandwidth().AbortBandwidthTest();
#endif
}

void CNetworkBandwidthTest::ReportTestResult()
{
    rtry
    {
		m_ReportPending = false; // Flagging regardless of the outcome

        rcheck(!IsTestActive(), catchall, gnetError("ReportTestResult skipped - Test in progress"));
        netBandwidthTestResult result = GetTestResult();

        rcheck(result.m_TestTime > m_LastReportTime, catchall, gnetDebug1("ReportTestResult skipped - Test result already reported or invalid"));
        rcheck(result.m_ResultCode != 0 || result.IsValid(), catchall, gnetWarning("ReportTestResult skipped - Empty result"));

        m_LastReportTime = result.m_TestTime;
        gnetDebug1("Reporting bandwidth test. up[%" I64FMT"u] down[%" I64FMT"u] source[%u] resultCode[%d]", result.m_UploadBps, result.m_DownloadBps, result.m_Source, result.m_ResultCode);

		MetricBandwidthTest m(result.m_UploadBps, result.m_DownloadBps, result.m_Source, result.m_ResultCode);
		APPEND_METRIC(m);
    }
    rcatchall
    {
    }
}
