//
// name:		NetworkSynchronisedScene.h
// description:	Support for running synchronised scenes in network games. A synchronised scene is a method
//              of running animations involving multiple entities that have animations that need to match up
//              (e.g. two peds walking up to one another and shaking hands). In network games these scenes may
//              involve entities owned by different machines in the session that need to be coordinated.
// written by:	Daniel Yelland
//

#ifndef NETWORK_SYNCHRONISED_SCENE_H
#define NETWORK_SYNCHRONISED_SCENE_H

#include "fwnet/nettypes.h"
#include "streaming/streamingmodule.h"
#include "vector/quaternion.h"
#include "vector/vector3.h"
#include "task/Animation/AnimTaskFlags.h"
#include "Task/System/TaskHelpers.h"

namespace rage
{
    class netLoggingInterface;
}

class CPed;

class CNetworkSynchronisedScenes
{
public:

    static const unsigned SIZEOF_SCENE_ID = 13;

    //PURPOSE
    // Initialises the network synchronised scenes
    static void Init();

    //PURPOSE
    // Shutdown the network synchronised scenes
    static void Shutdown();

    //PURPOSE
    // Update function - called once per frame
    static void Update();

    //PURPOSE
    // Returns whether we will accept requests from remote machines to start a synced scene on involving the local player. This is to prevent remote
    // players from griefing the local player by adding them to synced scenes and warping them across the map
    static bool AreRemoteSyncSceneLocalPlayerRequestsAllowed() { return ms_AllowRemoteSyncSceneLocalPlayerRequests; }

    //PURPOSE
    // Sets whether we will accept requests from remote machines to start a synced scene on involving the local player. This is to prevent remote
    // players from griefing the local player by adding them to synced scenes and warping them across the map
    //PARAMS
    // allowRemoteRequests - Indicates whether we should allow remote requests
    static void SetAllowRemoteSyncSceneLocalPlayerRequests(bool allowRemoteRequests) { ms_AllowRemoteSyncSceneLocalPlayerRequests = allowRemoteRequests; }

    //PURPOSE
    // Returns whether the specified scene is currently active
    static bool IsSceneActive(int sceneID);

    //PURPOSE
    // Returns the local synchronised scene ID from a network scene ID
    static int GetLocalSceneID(int sceneID);
    
	//PURPOSE
    // Returns the network scene ID from a local synchronised scene ID
	static int GetNetworkSceneID(int localSceneID);

    //PURPOSE
    // Returns the time the scene was started (the time returned is network time)
    //PARAMS
    // networkSceneID - Network scene ID of the scene to check
    static unsigned GetTimeSceneStarted(int networkSceneID);

    //PURPOSE
    // Returns whether the specified network player is in scope of the scene
    //PARAMS
    // player         - The player to check if in scope
    // networkSceneID - The network ID of the scene
    static bool IsInScope(const netPlayer &player, int networkSceneID);

    //PURPOSE
    // Creates a networked synchronised scene ready for parameters to be added to
    //PARAMS
    // position          - Position of the origin of the scene
    // rotation          - Rotation of the origin of the scene
    // holdLastFrame     - Indicates whether this synced scene should hold the anim at the last frame
    // isLooped          - Indicates whether this synced scene should loop the anim
    // phaseToStartScene - The phase to start playing the scene (normally this will be 0.0f)
    // phaseToStopScene  - The phase to stop the synced scene (normally this will be 1.0f)
    // startRate         - The rate to start playing the scene (normally this will be 1.0f)
	// scriptName        - The script name that called the scene creation command
	// scriptThreadPC    - The script threat PC where the scene creation command was called
    static int CreateScene(const Vector3    &position,
                           const Quaternion &rotation,
                           bool              holdLastFrame,
                           bool              isLooped,
                           float             phaseToStartScene,
                           float             phaseToStopScene,
                           float             startRate,
						   const char*		 scriptName = "",
						   int				 scriptThreadPC = -1);

	//PURPOSE
    // Called to release a previously created scene before it has been started
    //PARAMS
    // networkSceneID - The network ID of the scene to release
    static void ReleaseScene(int networkID);

    //PURPOSE
    // Attaches the specified synced scene to the specified entity (this can only be called on synced
    // scenes that have not been started yet)
    //PARAMS
    // networkSceneID - The scene to attach
    // physical       - The physical entity to attach the scene to
    // boneIndex      - The bone index of the entity to attach the scene to
    static void AttachSyncedSceneToEntity(int networkSceneID, const CPhysical *physical, int boneIndex);

    //PURPOSE
    // Add a ped and the animations they are to play to the scene. The animation will
    // only begin playing when the scene is started
    //PARAMS
    // ped                  - the ped to add to the scene
    // sceneID              - the scene to add the ped to
    // animDictName         - animation dictionary to use
    // animName             - animation to use
    // blendIn              - blend in rate
    // blendOut             - blend out rate
    // flags                - flags
    // ragdollBlockingFlags - ragdoll blocking flags
    // moverBlendIn         - mover blend in rate
    // ikFlags              - IK flags
    static void AddPedToScene(CPed       *ped, 
                              int         sceneID,
                              const char *animDictName,
                              const char *animName,
                              float       blendIn,
                              float       blendOut,
                              int         flags,
                              int         ragdollBlockingFlags,
                              float       moverBlendIn,
                              int         ikFlags);

    //PURPOSE
    // Add an entity and the animation to be played upon it during the scene. The animation
    // will only begin playing when the scene is started. This should not be used on Peds. Use AddPedToScene() instead
    //PARAMS
    // physical     - the physical entity to add to the scene
    // sceneID      - the scene to add the entity to
    // animDictName - animation dictionary to use
    // animName     - animation to use
    // blendIn      - blend in rate
    // blendOut     - blend out rate
    // flags        - flags
    static void AddEntityToScene(CPhysical  *physical,
                                 int         sceneID,
                                 const char *animDictName,
                                 const char *animName,
                                 float       blendIn,
                                 float       blendOut,
                                 int         flags);

	//PURPOSE
	// Adds a map entity to the synced scene. This will be used on all players involved in the scene - local and remote.
	// PARAMS
	// entityModelHash - Model hash of the entity to add
	// entityPosition  - World position of the entity to add
	// sceneID         - The ID of the scene to use a camera with
	static void AddMapEntityToScene(int sceneID, u32 entityModelHash, Vector3 entityPosition, const char *animDictName, const char *animName, float blendIn, float blendOut, int flags);

    //PURPOSE
    // Adds a camera to the synchronised scene. This will be used on all players involved in the scene - local and remote.
    //PARAMS
    // sceneID      - The ID of the scene to use a camera with
    // animDictName - animation dictionary to use
    // animName     - animation to use
    static void AddCameraToScene(int sceneID, const char *animDictName, const char *animName);

    //PURPOSE
    // Forces the local use of any camera attached to the synchronised scene to play, even if the local
    // player ped has not been added to the scene. If this is not called the camera will only animate for
    // scenes with the player ped involved. This only affects the local player as this data is not synced
    static void ForceLocalUseOfCamera(int sceneID);

    //PURPOSE
    // Updates the rate value of a scene description.
    //PARAMS
    // sceneID      - The ID of the scene to use a camera with
    // rate			- The new rate we wish to use
	static bool SetSceneRate(int sceneID, float const rate);

    //PURPOSE
    // Get the rate value of a scene description
    //PARAMS
    // sceneID      - The ID of the scene to use a camera with
    // rate			- The rate value
	static bool GetSceneRate(int sceneID, float& rate);

    //PURPOSE
    // Trys to starts a previous created synchronised scene running. For this to succeed all
    // entities involved in the scene must exist on this machine
    //PARAMS
    // sceneID - The ID of the scene to start running
    static bool TryToStartSceneRunning(int sceneID);

    //PURPOSE
    // Starts a previous created synchronised scene running. If this is a locally controlled
    // scene a network event will be sent to other players in the session to start the scene.
    //PARAMS
    // sceneID - The ID of the scene to start running
    static void StartSceneRunning(int sceneID);

    //PURPOSE
    // Stops a previous started synchronised scene running. If this is a locally controlled
    // scene a network event will be sent to other players in the session to stop the scene.
    //PARAMS
    // sceneID - The ID of the scene to stop running
    static void StopSceneRunning(int sceneID LOGGING_ONLY(, const char *stopReason));

	//PURPOSE
	// Just a count of peds in scene currently useful for checking that scene only has one ped when coordinating scene phase with remote in peds CTaskSynchronizedScene
	//PARAMS
	// networkSceneID - The scene to attach
	static u32 GetNumberPedsInScene(int networkSceneID);

    //PURPOSE
    // Serialises the scene using the specified serialiser and scene ID
    //PARAMS
    // serialiser - The serialiser to use
    // sceneID    - ID of the scene to serialise from/to
    template <class Serialiser> static void Serialise(Serialiser &serialiser, int networkSceneID)
    {
        const unsigned SIZEOF_POSITION               = 26;
        const unsigned SIZEOF_QUAT                   = 30;
        const unsigned SIZEOF_ATTACH_BONE            = 8;
        const unsigned SIZEOF_ANIM_HASH              = 32;
        const unsigned SIZEOF_BLEND_IN               = 30;
        const unsigned SIZEOF_BLEND_OUT              = 30;
        const unsigned SIZEOF_ANIM_FLAGS             = 32;
        const unsigned SIZEOF_RAGDOLL_BLOCKING_FLAGS = 32;
        const unsigned SIZEOF_IK_FLAGS               = eIkControlFlags_NUM_ENUMS + 1;
        const unsigned SIZEOF_PHASE                  = 9;
		const unsigned SIZEOF_RATE					 = 8;
		const unsigned SIZEOF_NAME_HASH				 = 32;

        const float MAX_BLEND_IN_RATE  = 1001.0f;
        const float MAX_BLEND_OUT_RATE = 1001.0f;

        SceneDescription *sceneDescription = GetSceneDescriptionFromNetworkID(networkSceneID);

        if(!sceneDescription)
        {
            sceneDescription = AllocateNewSceneDescription(networkSceneID);
            
            if(!gnetVerifyf(sceneDescription, "Failed to allocate new scene description! Too many concurrent synced scenes running!"))
            {
                return;
            }
        }

        SceneDescription &sceneData = *sceneDescription;

        bool sceneActive = sceneData.m_Active;
        SERIALISE_BOOL(serialiser, sceneActive);
        gnetAssertf(sceneActive, "Serialising a synced scene that is not active!");
        sceneData.m_Active = sceneActive;

        SERIALISE_POSITION_SIZE(serialiser, sceneData.m_ScenePosition, 0, SIZEOF_POSITION);
        SERIALISE_QUATERNION(serialiser, sceneData.m_SceneRotation, SIZEOF_QUAT);
        
        bool hasAttachEntity = sceneData.m_AttachToEntityID != NETWORK_INVALID_OBJECT_ID;
        SERIALISE_BOOL(serialiser, hasAttachEntity);

        if(hasAttachEntity)
        {
            SERIALISE_OBJECTID(serialiser, sceneData.m_AttachToEntityID);
            SERIALISE_INTEGER(serialiser, sceneData.m_AttachToBone, SIZEOF_ATTACH_BONE);
        }

        bool hasDefaultStopPhase = IsClose(sceneData.m_PhaseToStopScene, 1.0f);
        SERIALISE_BOOL(serialiser, hasDefaultStopPhase);

        if(!hasDefaultStopPhase)
        {
            SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, sceneData.m_PhaseToStopScene, 1.0f, SIZEOF_PHASE);
        }
        else
        {
            sceneData.m_PhaseToStopScene = 1.0f;
        }

        bool hasDefaultRate = IsClose(sceneData.m_Rate, 1.0f);
        SERIALISE_BOOL(serialiser, hasDefaultRate);

        if(!hasDefaultRate)
        {
            SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, sceneData.m_Rate, 2.0f, SIZEOF_RATE);
        }
        else
        {
            sceneData.m_Rate = 1.0f;
        }

        SERIALISE_BOOL(serialiser, sceneData.m_HoldLastFrame);
        SERIALISE_BOOL(serialiser, sceneData.m_Looped);
        SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, sceneData.m_Phase, 1.0f, SIZEOF_PHASE);
        SERIALISE_BOOL(serialiser, sceneData.m_UseCamera);

        if(sceneData.m_UseCamera)
        {
            u32 animHash = sceneData.m_CameraAnim.GetHash();
            SERIALISE_UNSIGNED(serialiser, animHash, SIZEOF_ANIM_HASH);
            sceneData.m_CameraAnim.SetHash(animHash);
        }

        u32 animDictHash = sceneData.m_AnimDict.GetHash();
        SERIALISE_UNSIGNED(serialiser, animDictHash, SIZEOF_ANIM_HASH);
        sceneData.m_AnimDict.SetHash(animDictHash);

        for(unsigned pedIndex = 0; pedIndex < CNetworkSynchronisedScenes::SceneDescription::MAX_PEDS_IN_SCENE; pedIndex++)
        {
            PedData &pedData = sceneData.m_PedData[pedIndex];

            bool hasPedData = pedData.m_PedID != NETWORK_INVALID_OBJECT_ID;

            SERIALISE_BOOL(serialiser, hasPedData);

            if(hasPedData)
            {
                SERIALISE_OBJECTID(serialiser, pedData.m_PedID);

                SERIALISE_UNSIGNED(serialiser, pedData.m_AnimPartialHash, SIZEOF_ANIM_HASH);
                SERIALISE_PACKED_FLOAT(serialiser, pedData.m_BlendIn,      MAX_BLEND_IN_RATE,  SIZEOF_BLEND_IN);
                SERIALISE_PACKED_FLOAT(serialiser, pedData.m_BlendOut,     MAX_BLEND_OUT_RATE, SIZEOF_BLEND_OUT);
                SERIALISE_PACKED_FLOAT(serialiser, pedData.m_MoverBlendIn, MAX_BLEND_IN_RATE,  SIZEOF_BLEND_IN);
                SERIALISE_INTEGER(serialiser, pedData.m_Flags, SIZEOF_ANIM_FLAGS);
                SERIALISE_INTEGER(serialiser, pedData.m_RagdollBlockingFlags, SIZEOF_RAGDOLL_BLOCKING_FLAGS);
                SERIALISE_INTEGER(serialiser, pedData.m_IkFlags,              SIZEOF_IK_FLAGS);
            }
            else
            {
                pedData.m_PedID = NETWORK_INVALID_OBJECT_ID;
            }
        }

        for(unsigned nonPedIndex = 0; nonPedIndex < CNetworkSynchronisedScenes::SceneDescription::MAX_NON_PED_ENTITIES_IN_SCENE; nonPedIndex++)
        {
            NonPedEntityData &nonPedEntityData = sceneData.m_NonPedEntityData[nonPedIndex];

            bool hasData = nonPedEntityData.m_EntityID != NETWORK_INVALID_OBJECT_ID;

            SERIALISE_BOOL(serialiser, hasData);

            if(hasData)
            {
                u32 animHash = nonPedEntityData.m_Anim.GetHash();

                SERIALISE_OBJECTID(serialiser, nonPedEntityData.m_EntityID);

                SERIALISE_UNSIGNED(serialiser, animHash, SIZEOF_ANIM_HASH);
                SERIALISE_PACKED_FLOAT(serialiser, nonPedEntityData.m_BlendIn,  MAX_BLEND_IN_RATE, SIZEOF_BLEND_IN);
                SERIALISE_PACKED_FLOAT(serialiser, nonPedEntityData.m_BlendOut, MAX_BLEND_OUT_RATE, SIZEOF_BLEND_OUT);
                SERIALISE_INTEGER(serialiser, nonPedEntityData.m_Flags, SIZEOF_ANIM_FLAGS);

                nonPedEntityData.m_Anim.SetHash(animHash);
            }
            else
            {
                nonPedEntityData.m_EntityID = NETWORK_INVALID_OBJECT_ID;
            }
        }

		for (unsigned mapEntityIndex = 0; mapEntityIndex < CNetworkSynchronisedScenes::SceneDescription::MAX_MAP_ENTITIES_IN_SCENE; mapEntityIndex++)
		{
			MapEntityData& mapEntityData = sceneData.m_MapEntityData[mapEntityIndex];
			
			bool hasData = mapEntityData.m_EntityNameHash != 0;
			SERIALISE_BOOL(serialiser, hasData);

			if(hasData)
			{
				SERIALISE_UNSIGNED(serialiser, mapEntityData.m_EntityNameHash, SIZEOF_NAME_HASH);
				SERIALISE_POSITION(serialiser, mapEntityData.m_EntityPosition);
				
				SERIALISE_PACKED_FLOAT(serialiser, mapEntityData.m_BlendIn, MAX_BLEND_IN_RATE, SIZEOF_BLEND_IN);
				SERIALISE_PACKED_FLOAT(serialiser, mapEntityData.m_BlendOut, MAX_BLEND_OUT_RATE, SIZEOF_BLEND_OUT);
				SERIALISE_INTEGER(serialiser, mapEntityData.m_Flags, SIZEOF_ANIM_FLAGS);
				
				u32 animHash = mapEntityData.m_Anim.GetHash();
				SERIALISE_UNSIGNED(serialiser, animHash, SIZEOF_ANIM_HASH);
				mapEntityData.m_Anim.SetHash(animHash);
			}
		}
    }

    //PURPOSE
    // Called when a synced scene update has been received from the network
    //PARAMS
    // sceneID            - ID of the scene that has been updated
    // networkTimeStarted - The network time this scene was started
    static void PostSceneUpdateFromNetwork(int sceneID, unsigned networkTimeStarted);

    //PURPOSE
    // Called when a task update has been received for a clone ped
    //PARAMS
    // ped - ped that has had it's task updated
    static void PostTaskUpdateFromNetwork(CPed *ped);

    //PURPOSE
    // Check the synced scene state for a specified ped. Ensure a ped playing a synced scene
    // on the owner machine is playing the synced scene on this machine too. If not attempts
    // to fix this up
    //PARAMS
    // ped - ped that has had it's task updated
    static void CheckSyncedSceneStateForPed(CPed *ped);

    //PURPOSE
    // Called when a ped aborts a synced scene task
    //PARAMS
    // ped            - The ped aborting it's synced scene task
    // networkSceneID - The network ID of the scene being aborted
    // eventName      - The name of the AI event causing the abort
    static void OnPedAbortingSyncedScene(CPed *ped, int networkSceneID, const char *eventName);

    //PURPOSE
    // Called when a ped aborts a synced scene before it has been added to
    // the synced scene by the AI/animation code - this means that code
    // cannot stop the scene for any other entities involved in the scene
    // if necessary as it has not reference. The safest fix is to handle this here
    //PARAMS
    // ped            - The ped aborting it's synced scene task early
    // networkSceneID - The network ID of the scene being aborted early
    static void OnPedAbortingSyncedSceneEarly(CPed *ped, int networkSceneID);

	// PURPOSE
	// Used to block starting of synced scenes on clones
	static bool IsCloneBlockedFromPlayingScene(int networkSceneID);

    //PURPOSE
    // Logs the contents of the scene with the specified ID
    // log     - The logging interface to log to
    // sceneID - The ID of the scene to log
#if !__FINAL
    static void LogSceneDescription(netLoggingInterface &log, int sceneID);
#endif

#if __BANK
    //PURPOSE
    // Adds debug widgets for testing the system
    static void AddDebugWidgets();

    //PURPOSE
    // Displays debug text to show the current state of the system
    static void DisplayDebugText();
#endif // __BANK

private:

    //PURPOSE
    // describes the data for a ped that has been added to a scene
    struct PedData
    {
        ObjectId               m_PedID;                // the ped to add to the scene
        u32                    m_AnimPartialHash;      // partial hash of the animation to use
        float                  m_BlendIn;              // blend in rate
        float                  m_BlendOut;             // blend out rate
        float                  m_MoverBlendIn;         // mover blend in rate
        int                    m_Flags;                // flags
        int                    m_RagdollBlockingFlags; // ragdoll blocking flags
        int                    m_IkFlags;              // IK flags
        bool                   m_WaitingForTaskUpdate; // once a clone ped is added to the scene we wait for the task update to confirm they are running on the owner
    };

    //PURPOSE
    // describes the data for an entity (that is not a ped) that has been added to a scene
    struct NonPedEntityData
    {
        ObjectId               m_EntityID;             // the entity to add to the scene
        strStreamingObjectName m_Anim;                 // animation to use
        float                  m_BlendIn;              // blend in rate
        float                  m_BlendOut;             // blend out rate
        int                    m_Flags;                // flags
    };

	struct MapEntityData
	{
		u32 m_EntityNameHash;					// the entity's name hash we are trying to add
		Vector3 m_EntityPosition;				// the entity's position we are tring to add
		strStreamingObjectName m_Anim;			// animation to use
		float                  m_BlendIn;		// blend in rate
		float                  m_BlendOut;		// blend out rate
		int                    m_Flags;			// flags
	};

    //PURPOSE
    // describes a synchronised scene
    struct SceneDescription
    {
        void Reset();

        static const unsigned MAX_PEDS_IN_SCENE = 10;
        PedData m_PedData[MAX_PEDS_IN_SCENE];

        static const unsigned MAX_NON_PED_ENTITIES_IN_SCENE = 5;
        NonPedEntityData m_NonPedEntityData[MAX_NON_PED_ENTITIES_IN_SCENE];

		static const unsigned MAX_MAP_ENTITIES_IN_SCENE = 1;
		MapEntityData m_MapEntityData[MAX_MAP_ENTITIES_IN_SCENE];

        int                    m_NetworkSceneID;
		fwSyncedSceneId        m_LocalSceneID;
        unsigned               m_NetworkTimeStarted;
        Vector3                m_ScenePosition;
        Quaternion             m_SceneRotation;
        ObjectId               m_AttachToEntityID;
        int                    m_AttachToBone;
        bool                   m_HoldLastFrame;
        bool                   m_Looped;
        float                  m_PhaseToStopScene;
        strStreamingObjectName m_AnimDict;
        strStreamingObjectName m_CameraAnim;
        bool                   m_UseCamera;
        bool                   m_LocallyCreated;
        bool                   m_Active;
        bool                   m_PendingStart;
        bool                   m_Started;
        bool                   m_PendingStop;
        bool                   m_Stopping;
        bool                   m_ForceLocalUseOfCamera;
        float                  m_Phase;
		float				   m_Rate;
        CClipRequestHelper     m_ClipRequestHelper;    // helper for streaming in required anims
		atString			   m_ScriptName; // Script debug info 
		int					   m_ScriptThreadPC; // Script debug info
    };

    //PURPOSE
    // Allocates a new scene description
    //PARAMS
    // networkID - Optional parameter to specify the networkID to use for the newly allocated scene
    static SceneDescription *AllocateNewSceneDescription(const int networkSceneID = -1);

    //PURPOSE
    // Releases a previously allocated scene description
    //PARAMS
    // networkID - The networkID of the scene to return
    static void ReleaseSceneDescription(int networkID);

    //PURPOSE
    // Returns the scene description from the specified network ID
    //PARAMS
    // networkID - The networkID of the scene to return
    static SceneDescription *GetSceneDescriptionFromNetworkID(int networkID);

    //PURPOSE
    // Returns the scene description from the specified local ID
    //PARAMS
    // networkID - The localID of the scene to return
    static SceneDescription *GetSceneDescriptionFromLocalID(int networkID);

	//PURPOSE
	// Returns whether there is a free scene description - this can be used to
	// check whether the maximum supported number of network synchronised scenes
	// are not current active
	static bool HasFreeSceneDescription();

    //PURPOSE
    // Returns when the non-ped models involved in the scene have been loaded
    //PARAMS
    // sceneData - The scene to check whether the models are loaded
    static bool HaveNonPedModelsBeenLoaded(SceneDescription &sceneData);

    //PURPOSE
    // Starts a synchronised scene running on the local machine
    //PARAMS
    // scene - The scene description used to create the scene
    static void StartLocalSynchronisedScene(SceneDescription &scene);

    //PURPOSE
    // Stops a synchronised scene running on the local machine
    //PARAMS
    // scene - The scene description used to create the scene
    static void StopLocalSynchronisedScene(SceneDescription &scene, const char *LOGGING_ONLY(stopReason));

    //PURPOSE
    // Starts a synchronised scene task on a ped on the local machine
    //PARAMS
    // sceneData - The scene data for the scene to run on the ped
    // pedData   - Describes the ped to start running the scene on
    static void StartLocalSyncedSceneOnPed(SceneDescription &sceneData, PedData &pedData);

	//PURPOSE
	// Starts a synchronised scene on a physical object
	// PARAMS
	// sceneData	- The scene data for the scene to run on the ped
	// entity		- physical entity
	// anim			- animation to play
	// blendIn		- blend in rate
	// blendOut		- blend out rate
	// flags		- anim flags
	static bool StartLocalSyncedSceneOnPhysical(SceneDescription &sceneData, CPhysical* entity, strStreamingObjectName anim, float blendIn, float blendOut, int flags, unsigned* failReason = nullptr);

	//PURPOSE
    // Stops a synchronised scene task on a ped on the local machine
    //PARAMS
    // pedData - Describes the ped to stop running the scene on
    static void StopLocalSyncedSceneOnPed(PedData &pedData);

	//PURPOSE
	// Stops a synchronised scene on a physical object
	// PARAMS
	// sceneData	- The scene data for the scene to run on the ped
	// entity		- physical entity
	// anim			- animation to play
	// blendIn		- blend in rate
	// flags		- anim flags
	static void StopLocalSyncedSceneOnPhysical(SceneDescription &sceneData, CPhysical* entity, strStreamingObjectName anim, float blendIn, int flags);

    //PURPOSE
    // Removes the specified ped from any started scenes. This is necessary
    // to prevent a newly starting scene from being stopped on the ped when the
    // previous one is finished
    //PARAMS
    // pedID           - Network ID of the ped to remove from the started scenes
    // sceneIDToIgnore - Scene ID to not remove the ped from (used when adding the ped to a new scene)
    static void RemovePedFromStartedScenes(ObjectId pedID, int sceneIDToIgnore);

    //PURPOSE
    // Removes the specified non ped from any started scenes. This is necessary
    // to prevent a newly starting scene from being stopped on the non ped when the
    // previous one is finished
    //PARAMS
    // nonPedID        - Network ID of the non ped to remove from the started scenes
    // sceneIDToIgnore - Scene ID to not remove the non ped from (used when adding the non ped to a new scene)
    static void RemoveNonPedFromStartedScenes(ObjectId nonPedID, int sceneIDToIgnore);

    //PURPOSE
    // Returns whether the ped with the specified ID is part of any currently pending synced scene
    //PARAMS
    // pedID - Network ID of the ped to check
    static bool IsPedPartOfPendingSyncedScene(ObjectId pedID);

    static const unsigned MAX_NUM_SCENES = 64;
    static SceneDescription ms_SyncedScenes[MAX_NUM_SCENES];

    // indicates whether we will accept requests from remote machines to start a synced scene on involving the local player
    static bool ms_AllowRemoteSyncSceneLocalPlayerRequests;
#if __BANK || __FINAL_LOGGING
	enum
	{
		SSSFR_NONE = 0,
		SSSFR_NO_DRAWABLE,
		SSSFR_NO_SKELETON,
		SSSFR_NO_SKELETON_DATA,
		SSSFR_NO_ANIM_DIRECTOR,
	};

	static const char* GetSSSFailReason(unsigned reason)
	{
		switch (reason)
		{
		case SSSFR_NONE:
			return "SSSFR_NONE";
		case SSSFR_NO_DRAWABLE:
			return "SSSFR_NO_DRAWABLE";
		case SSSFR_NO_SKELETON:
			return "SSSFR_NO_SKELETON";
		case SSSFR_NO_SKELETON_DATA:
			return "SSSFR_NO_SKELETON_DATA";
		case SSSFR_NO_ANIM_DIRECTOR:
			return "SSSFR_NO_ANIM_DIRECTOR";
		default:
			gnetAssertf(0, "GetSSSFailReason has invalid reason passed in: %d", reason);
			return "MISSING";
		}
	}

#endif // __BANK
};

#endif  // NETWORK_SYNCHRONISED_SCENE_H
