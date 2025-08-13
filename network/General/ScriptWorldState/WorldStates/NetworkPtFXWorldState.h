//
// name:		NetworkPtFXWorldState.h
// description:	Support for network scripts to play particle effects via the script world state management code
// written by:	Daniel Yelland
//

#ifndef NETWORK_PTFX_WORLD_STATE_H
#define NETWORK_PTFX_WORLD_STATE_H

#include "fwtl/pool.h"
#include "network/general/scriptworldstate/NetworkScriptWorldStateManager.h"
#include "network/general/scriptworldstate/NetworkScriptWorldStateTypes.h"
#include "scene/Entity.h"

//PURPOSE
// Describes a particle effect that has been played by a script
class CNetworkPtFXWorldStateData : public CNetworkWorldStateData
{
public:

    FW_REGISTER_CLASS_POOL(CNetworkPtFXWorldStateData);

    //PURPOSE
    // Class constructor
    CNetworkPtFXWorldStateData();

    //PURPOSE
    // Class constructor
    //PARAMS
    // scriptID      - The ID of the script that has requested the PTFX
    // locallyOwned  - Indicates whether this PTFX was requested by the local machine
    // ptFXHash      - Hash identifying the PTFX to play
    // ptFXAssetHash - Hash identifying the asset the PTFX resides in
    // fxPos         - Position to display the PTFX
    // fxRot         - Rotation to apply to the PTFX
    // scale         - Scale to apply to the PTFX
    // invertAxes    - Flags indicating which axes to invert (if any)
    // scriptPTFX    - The script ID of the PTFX locally created (not synced)
    // startPTFX     - Flag indicating the particle effect should be streamed in and started
    // pTFXRefAdded  - Flag indicating a streaming reference has been added to the PTFX
    CNetworkPtFXWorldStateData(const CGameScriptId &scriptID,
                               bool                 locallyOwned,
                               u32                  ptFXHash,
                               u32                  ptFXAssetHash,
                               const Vector3       &fxPos,
                               const Vector3       &fxRot,
                               float                scale,
                               u8                   invertAxes,
                               int                  scriptPTFX,
                               bool                 startPTFX,
                               bool                 pTFXRefAdded,
							   bool					hasColour = false,
							   u8					r = 0,
							   u8					g = 0,
							   u8					b = 0,
							   ObjectId				entityId = NETWORK_INVALID_OBJECT_ID,
							   int					boneIndex = -1,
							   u32					evoHashA = 0,
							   float				evoValA = 0.f,
							   u32					evoHashB = 0,
							   float				evoValB = 0.f,
							   bool					attachedToWeapon = false,
							   bool                 terminateOnOwnerLeave = false,
							   u64                  ownerPeerID = 0,
							   int					uniqueID = 0);

    //PURPOSE
    // Adds an evolve to replicate for this ptfx world state data
	// returns true if FX changed
	bool AddEvolve(u32 evoHash, float evoVal);

    //PURPOSE
    // Sets the colour of this ptfx world state data
	void SetColour(float r, float g, float b);

    //PURPOSE
    // Returns the type of this world state
    virtual unsigned GetType() const { return NET_WORLD_STATE_PTFX; }

    //PURPOSE
    // Called once per frame
    virtual void Update();
	 
	//PURPOSE
	// Checks if this world state has expired
    virtual bool HasExpired() const;

    //PURPOSE
    // Makes a copy of the world state data, required by the world state data manager
    CNetworkPtFXWorldStateData *Clone() const;

    //PURPOSE   
    // Returns whether the specified override data is equal to the data stored in this instance
    //PARAMS
    // worldState - The world state to compare
    virtual bool IsEqualTo(const CNetworkWorldStateData &worldState) const;

    //PURPOSE
    // Starts the scripted PTFX
    void ChangeWorldState();

    //PURPOSE
    // Stops the scripted PTFX
    void RevertWorldState();

    //PURPOSE
    // Updates newly created world state data with any local only info stored 
    // in this object instance
    //PARAMS
    // hostWorldState - The world state to update from after a host response
    void UpdateHostState(CNetworkWorldStateData &hostWorldState);

    //PURPOSE
    // Starts the scripted PTFX
    //PARAMS
    // scriptID      - The ID of the script that has enabled the road nodes
    // ptFXHash      - Hash identifying the PTFX to play
    // ptFXAssetHash - Hash identifying the asset the PTFX resides in
    // fxPos         - Position to display the PTFX
    // fxRot         - Rotation to apply to the PTFX
    // scale         - Scale to apply to the PTFX
    // invertAxes    - Flags indicating which axes to invert (if any)
    // scriptPTFX    - The script ID of the PTFX locally created (not synced)
    static void AddScriptedPtFX(const CGameScriptId &scriptID,
                                u32                  ptFXHash,
                                u32                  ptFXAssetHash,
                                const Vector3       &fxPos,
                                const Vector3       &fxRot,
                                float                scale,
                                u8                   invertAxes,
                                int                  scriptPTFX,
								CEntity*			 pEntity = NULL,
								int					 boneIndex = -1,
								float				 r = 1.0f,
								float				 g = 1.0f,
								float				 b = 1.0f,
								bool                 terminateOnOwnerLeave = false,
								u64                  ownerPeerID = 0);

    //PURPOSE
    // Adds an evolve to replicate for the PTFX
    //PARAMS
    // scriptID      - The ID of the script that has enabled the road nodes
	// evoHash		 - The evolve hash
    // evoVal		 - The evolve value
	// scriptPTFX    - The script ID of the PTFX locally created (not synced)
	static void ModifyScriptedEvolvePtFX(const CGameScriptId& scriptID, u32 evoHash, float evoVal, int scriptPTFX);

	static void ApplyScriptedEvolvePtFXFromNetwork(int worldStateUniqueID, u32 evoHash, float evoVal);

    //PURPOSE
    // Modifies the colour of the scripted PTFX
    //PARAMS
    // scriptID      - The ID of the script that has enabled the road nodes
	// r 			 - The RGB red 0 - 1.0
	// g			 - The RGB green 0 - 1.0
	// b			 - The RGB blue 0 - 1.0
    // scriptPTFX    - The script ID of the PTFX locally created (not synced)
	static void ModifyScriptedPtFXColour(const CGameScriptId &scriptID, float r, float g, float b, int scriptPTFX);

    //PURPOSE
    // Stops the scripted PTFX
    //PARAMS
    // scriptID      - The ID of the script that has enabled the road nodes
    // scriptPTFX    - The script ID of the PTFX locally created (not synced)
    static void RemoveScriptedPtFX(const CGameScriptId &scriptID,
                                   int                  scriptPTFX);

    //PURPOSE
    // Returns the name of this world state data
    const char *GetName() const { return "PTFX"; };

	//PURPOSE
	// Returns a unique ID valid across network
	int GetUniqueID() const { return m_uniqueID; }

    //PURPOSE
    // Logs the world state data
    //PARAMS
    // log - log file to use
    void LogState(netLoggingInterface &log) const;

    //PURPOSE
    // Reads the contents of an instance of this class via a read serialiser
    //PARAMS
    // serialiser - The serialiser to use
    void Serialise(CSyncDataReader &serialiser);

    //PURPOSE
    // Write the contents of this instance of the class via a write serialiser
    //PARAMS
    // serialiser - The serialiser to use
    void Serialise(CSyncDataWriter &serialiser);

#if __DEV
    //PURPOSE
    // Returns whether this world state data is conflicting with the specified world state data.
    // We don't allow the same particle effect to be requested at the same location
    //PARAMS
    // worldState - The world state to compare
    virtual bool IsConflicting(const CNetworkWorldStateData &worldState) const;
#endif // __DEV


#if __BANK
    //PURPOSE
    // Adds debug widgets to test this world state
    static void AddDebugWidgets();

    //PURPOSE
    // Displays debug text describing this world state
    static void DisplayDebugText();
#endif // __BANK

private:

    //PURPOSE
    // Write the contents of this instance of the class via a write serialiser
    //PARAMS
    // serialiser - The serialiser to use
    template <class Serialiser> void SerialiseWorldState(Serialiser &serialiser);

	Vector3  m_FxPos;         // Position to display the PTFX
	Vector3  m_FxRot;         // Rotation to apply to the PTFX
    u32      m_PtFXHash;      // Hash identifying the PTFX to play
    u32      m_PtFXAssetHash; // Hash identifying the asset the PTFX resides in
    float    m_Scale;         // Scale to apply to the PTFX
	int      m_ScriptPTFX;    // ID of the locally created PTFX for this world state (this is not synced to other machines)
	int		 m_boneIndex;
	float    m_evoValA;
	float	 m_evoValB;
	u32		 m_evoHashA;
	u32		 m_evoHashB;
	ObjectId m_EntityID;
	u8       m_InvertAxes;    // Flags indicating which axes to invert (if any)
	u8		 m_r;
	u8		 m_g;
	u8		 m_b;
	u8		 m_evolveCount;
    bool     m_StartPTFX;     // Flag indicating the particle effect should be streamed in and started
    bool     m_PTFXRefAdded;  // Flag indicating we have added a streaming reference
	bool     m_hasColour;
	bool	 m_bUseEntity;
	bool	 m_bAttachedToWeapon;
	bool     m_bTerminateOnOwnerLeave;
	u64      m_ownerPeerID;
	int		 m_uniqueID; // valid across network
};

#endif  // NETWORK_PTFX_WORLD_STATE_H
