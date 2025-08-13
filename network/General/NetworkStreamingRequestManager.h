//
// name:        NetworkStreamingRequestManager.h
// description: Manages streaming requests made by network code based on network messages. This class should be used
//              for cases where a message from another machine is rejected until the model streams in (such as object creation messages)
//              this avoids models streaming in and then being streamed out before the resent network request has been resent
// written by:  Daniel Yelland
//

#ifndef NETWORK_STREAMING_REQUEST_MANAGER_H_
#define NETWORK_STREAMING_REQUEST_MANAGER_H_

#include "entity/archetypemanager.h"
#include "network/General/NetworkUtil.h"
#include "task/system/TaskHelpers.h"

#define DEBUG_RENDER_NETWORK_STREAMING_ANIMS	0
#define DEBUG_ENABLE_NETWORK_STREAMING_ANIMS	1

class CNetworkStreamingRequestManager
{
public:

    //PURPOSE
    // Initialises the streaming request manager at the start of the network session
	static void Init();

    //PURPOSE
    // Shuts down the streaming request manager at the end of the network session
    static void Shutdown();

    //PURPOSE
    // Performs any per frame processing required by the streaming request manager. Called once per frame
    static void Update();

    //PURPOSE
    // Requests a model - if the model is already in memory returns true. If not a streaming request is
    // made and the model will be flagged to stay in memory for a time period to avoid thrashing.
    //PARAMS
    // modelID - The model to stream in
    static bool RequestModel(fwModelId modelID);

private:

	static void UpdateModels();

    //PURPOSE
    // Keeps track of a model request made from some network code
    struct RequestData
    {
        RequestData()
        {
            Reset();
        }

        void Reset()
        {
            m_InUse              = false;
            m_TimeToAllowRemoval = 0;

            m_ModelID.Invalidate();
        }

        bool      m_InUse;              // Indicates whether this structure is currently in use
        fwModelId m_ModelID;            // The model index that has been requested
        u32       m_TimeToAllowRemoval; // The system time this model may be streamed out again (used to prevent thrashing)
    };

    static const unsigned MAX_PENDING_MODEL_REQUESTS = 20;
    static RequestData m_PendingRequests[MAX_PENDING_MODEL_REQUESTS];

	static const u32 DEAD_PED_REQUEST_ORPHAN_TEST_TIME_LIMIT_MS = 5000;
	static u32 m_deadPedTimerMS;		// A timer that gets started if a dead ped has a anim streaming request.
										// after 5 seconds, the request has probably been orphaned so as assert will fire
										// it means the ped has probably migrated (and then died) out of step with the shared network array
										// so the call to RemoveAllAnimRequests( ) has failed.
										// If it fires, we need to add a "garbage collector" style clean up for dead peds in the Update function.
#if !__FINAL
	//Record info of each dead ped seen during the period when m_deadPedTimerMS is first set until it is cleared so it can be displayed in error report.
	struct RecordDeadPedRequest
	{
		RecordDeadPedRequest()
		{
			Reset();
		}

		void Reset()
		{
			m_deadPedID              = NETWORK_INVALID_OBJECT_ID;
			m_Frame = 0;
			m_deadPedTimerMS = 0;
			m_bClone = false;
			m_bValidObject = false;
			m_bRequestedAgain = false;
		}

		ObjectId	m_deadPedID;  
		bool		m_bClone;
		bool		m_bValidObject;
		bool		m_bRequestedAgain;
		u32			m_Frame;
		u32			m_deadPedTimerMS;
	};
	static const unsigned MAX_DEAD_PED_RECORD_INFOS = 20;
	static RecordDeadPedRequest m_DeadPedInfoArray[MAX_DEAD_PED_RECORD_INFOS];
#endif
public:

	static const int INVALID_ANIM_REQUEST_INDEX = -1;
	static const int INVALID_ANIM_HELPER_INDEX  = -1;
	static const int INVALID_ANIM_REQUEST_HASH  = 0;
	static const int MAX_NUM_ANIM_REQUESTS_PER_PED = 7;

	struct NetworkAnimRequestData
	{
		NetworkAnimRequestData()
		{
			Clear();
		}

		void Serialise(CSyncDataBase& serialiser);
		void Clear();
		void Copy(NetworkAnimRequestData &animRequestData);
		bool IsEmpty(void) const;

		netObject* GetNetworkObject() const;

		void SetPedID(ObjectId id)	{ m_Ped.SetPedID(id);		}
		CPed* GetPed(void)			{ return m_Ped.GetPed();	}
		
		u32 m_animHashes[MAX_NUM_ANIM_REQUESTS_PER_PED];
		u32 m_numAnims;
		CSyncedPed m_Ped;
	};

	static bool		RequestNetworkAnimRequest(CPed const* pPed, u32 const animHash, bool bRemoveExistingRequests = false);
	static bool		RemoveAllNetworkAnimRequests(CPed const* pPed);
	
private:

	static bool		StreamAnim(CPed const* pPed, u32 const animHash);
	static bool		UnstreamAnim(u32 const animHash);
	static bool		IsAnimStreaming(u32 const animHash);
	static int		GetAnimStreamingHelperSlot(u32 const animHash);
	static int		GetFreeAnimStreamingHelperSlot(u32 const animHash);
	
	static int		GetNetworkAnimRequestIndex(u32 const animHash, CPed const * pPed = NULL);

	static void		GarbageCollectDeadPedAnimRequests(void);
	static void		RemoveOldStreamingAnims();
	static void		AddNewStreamingAnims();
	static void		UpdateStreamingAnims();

    //PURPOSE
    // Requests the highest priority vehicle entry anims for remote players - This is done to 
    // try and get anims in memory ready for remote players entering/exiting vehicles
    static void UpdateVehicleAnimRequests();

    //PURPOSE
    // Requests vehicle anims for the remote physical player in the specified arrays
    //PARAMS
    // remotePhysicalPlayers    - Array of all remote physical players
    // numRemotePhysicalPlayers - Number of all remote physical players
    // playersAtPriority        - Array of indices into above remote physical players array
    // numPlayersAtPriority     - Size of the array of indices
    // numRequestsMade          - Number of vehicle streaming requests made (used to limit number of requests made)
    static void RequestVehicleAnims(const netPlayer * const *remotePhysicalPlayers,
                                    const unsigned           numRemotePhysicalPlayers,
                                    const unsigned           playersAtPriority[MAX_NUM_PHYSICAL_PLAYERS],
                                    const unsigned           numPlayersAtPriority,
                                    unsigned                &numRequestsMade);

#if !__FINAL && DEBUG_RENDER_NETWORK_STREAMING_ANIMS

	static void		DebugRender(void);

#endif /* !__FINAL && DEBUG_RENDER_NETWORK_STREAMING_ANIMS */

#if !__FINAL 

	static void		SanityCheckEntries(void);
	static void		SanityCheckDeadPeds(void);
	static void		DebugPrint(void);
	static void		ResetDeadPedInfos(bool bForceReset = false);
	static void		DebugPrintDeadPedInfos(void);
	static RecordDeadPedRequest*		IsInDeadPedInfos(ObjectId pedId);
	static bool		AddToDeadPedInfos(ObjectId pedId, CPed const* pPed);
	static bool		CheckACurrentDeadPedExceedsTime();
#endif /* !__FINAL */

	static const u32 MAX_NETWORK_ANIM_REQUESTS	= 20;		// X peds * ~10 requests each.... 
	static const u32 NUM_ANIM_REQUEST_HELPERS	= 10;		// How many anims should we stream in at once....

	static void		UpdateAnims();

	/*
	// we want to bake multiple requests for the same get up set down into a single one
	- from:
	Ped 0x2325c23 NMBS_ARMED_1HANDED_GETUPS
	Ped 0x5235667 NMBS_ARMED_1HANDED_GETUPS
	Ped 0x3545378 NMBS_ARMED_1HANDED_GETUPS

	- to:
	AnimStreamingHelper[X].Request(  NMBS_ARMED_1HANDED_GETUPS )
	*/

	struct AnimStreamingHelper
	{
		AnimStreamingHelper()
		{}

		void	Request(u32 const animHash);
		void	Remove();
		bool	IsInUse() const;
		int		GetHash() const;
		void	Update();

		CGetupSetRequestHelper m_helper;
	};

	friend class CAnimRequestArrayHandler;

	static atFixedArray<NetworkAnimRequestData, MAX_NETWORK_ANIM_REQUESTS>	ms_NetSyncedAnimRequests;
	static atFixedArray<AnimStreamingHelper, NUM_ANIM_REQUEST_HELPERS>		ms_AnimStreamingHelpers;

    static CVehicleClipRequestHelper ms_VehicleClipRequestHelper;
};

#endif  // NETWORK_STREAMING_REQUEST_MANAGER_H_
