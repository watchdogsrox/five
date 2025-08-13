// 
// animation/debug/AnimPlacementTool.h
// 
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved. 
// 

#if __BANK

#ifndef ANIM_PLACEMENT_TOOL_H
#define ANIM_PLACEMENT_TOOL_H

// rage includes
#include "bank/bank.h"
#include "fwanimation/animdirector.h"
#include "fwanimation/syncedscene.h"
#include "fwdebug/debugbank.h"

// game includes
#include "animation/debug/AnimDebug.h"
#include "scene/RegdRefTypes.h"
#include "script/script_debug.h"
#include "task/Animation/TaskAnims.h"

//////////////////////////////////////////////////////////////////////////
//	Anim Placement Tool
//	Tool to facilitate accurate placement of ped start positions based
//	on the mover track progression of an anim
//////////////////////////////////////////////////////////////////////////

class CAnimPlacementTool: public datBase
{
public:

	CAnimPlacementTool(): m_rootMatrix(M34_IDENTITY),
		m_pDynamicEntity(NULL),
		m_orientation(VEC3_ZERO),
		m_bOwnsEntity(true), 
		m_attachmentTool(NULL), 
		m_parentEntity(NULL),
		m_animSelector(true, false, false)
	{
		Initialise();
	}
	
	CAnimPlacementTool(CDebugAttachmentTool* pAnimDebug, CPhysical* pParent): m_rootMatrix(M34_IDENTITY),
		m_pDynamicEntity(NULL),
		m_orientation(VEC3_ZERO),
		m_bOwnsEntity(true), 
		m_attachmentTool(pAnimDebug), 
		m_parentEntity(pParent),
		m_animSelector(true, false, false)
	{
		Initialise();
	}

	~CAnimPlacementTool()
	{
		CleanupEntity();
	};

	// PURPOSE: Sets the anim that will be used to position the ped
	// PARAMS:	animDictionaryIndex - The index of the animation dictionary (in the anim manager) containing the required anim
	//			animName - The index of the anim in the given dictionary to use
	void SetAnim(int animDictionaryIndex, int animIndex);

	void SetAnim() { StartAnim(); }

	//PURPOSE:	Call this to init the anim placement tool for use
	//			(Creates a new dynamic entity to use)
	void Initialise(fwModelId pedModelIndex);

	//PURPOSE:	Call this to init the anim placement tool for use
	//			(Creates a new dynamic entity to use)
	void InitialiseVehicle(fwModelId vehicleModelIndex);

	//PURPOSE:	Call this to init the anim placement tool for use
	//			(Creates a new dynamic entity to use)
	void InitialiseObject(fwModelId modelId);

	//PURPOSE:	Call this to init the anim placement tool for use
	//			(Registers and existing in game ped with the tool
	void Initialise(CPed* pPed, bool OwnsEntity = false);

	//PURPOSE:	Call this to init the anim placement tool for use
	//			(Registers and existing in game vehicle with the tool
	void Initialise(CVehicle* pVehicle, bool OwnsEntity = false);


	//PURPOSE:	Call this to init the anim placement tool for use
	//			(Registers and existing in game object with the tool
	void Initialise(CObject* pObject, bool OwnsEntity = false);

	// PURPOSE: Should be called once per frame to update the position and phase of the test entity
	//			based on the selected phase
	// PARAMS:	bOwnedByEditor - set this to true if the system should respond to overrides from the anim placement editor
	//			setting this to false will ignore the anim placement editor's sync mode
	// RETURNS: A boolean signifying if the update was successful, if this returns false, the tool needs to be deleted / restarted
	bool Update(bool bOwnedByEditor = false);

	// PURPOSE:	Adds the necessary widgets to control this anim placement tool
	// PARAMS:	pBank- A pointer to the bank we want to add the tool to
	//			groupName - The title to give to the bank group
	//			showDeleteButton - True if you want to show the delete button, false otherwise
	//			animSelectedCallback - can be used to specify a callback to notify when the selected anim changes
	//			showStateControls -	If true, widgets will be added to control position, orientation and anim timeline.
	void AddWidgets(bkBank* pBank,  const char * groupName, bool showDeleteButton = false, datCallback animSelectedCallback = NullCB, bool showStateControls = true);
	
	// PURPOSE: Remove any widgets added with AddWidgets
	void RemoveWidgets();

	// PURPOSE: Sets the root matrix transform of the anim placement tool using the matrix provided
	void SetRootMatrix(const Matrix34& mtx);

	// PURPOSE: Sets the root matrix transform of the anim placement tool using the matrix provided
	void SetRootMatrixPosition(const Vector3& pos);

	// PURPOSE: Return true if the tool is currently paused
	bool IsPaused();

	// PURPOSE:	Toggles pause / looping play of the anim
	void TogglePause();

	// PURPOSE: Returns a pointer to the entity (i.e. the ped) being used to display the anim
	inline CDynamicEntity* GetTestEntity() { return m_pDynamicEntity; }

	//PURPOSE: Get the attached entity if we have one
	inline CPhysical* GetParentEntity() { return m_parentEntity; }

	inline CDebugAttachmentTool* GetAnimDebugAttachTool() { return m_attachmentTool; }

	// PURPOSE: comparison operator for deleting from parent structure
	bool operator==(const CAnimPlacementTool& other) const
	{
		return (this == &other);
	}
	
	// PURPOSE: Cleans up any supporting data / widgets used in creating the tool
	void Shutdown();

	// PURPOSE: Get's the script call that will reproduce the anim of the tool in its current state
	//
	const char * GetScriptCall();

	const char * GetCleanupScriptCall();

	// PURPOSE: Provides access to the tools anim selector
	inline CDebugClipSelector& GetAnimSelector() { return m_animSelector; }

	// PURPOSE: Return the current phase on the tool
	float GetPhase(){ return m_timeLine.GetPhase();}

	// PURPOSE: Changes the displayed ped model to one of the given index
	void SetPedModel(fwModelId pedModelIndex);

	// PURPOSE: instructs the anim placement tool to use the given ped
	void SetPed(CPed* pPed, bool OwnsEntity = false);

	// PURPOSE: instructs the anim placement tool to use the given vehicle
	void SetVehicle(CVehicle* pVeh, bool OwnsEntity = false);

	// PURPOSE: instructs the anim placement tool to use the given object
	void SetObject(CObject* pObject, bool OwnsEntity = false);

	// PURPOSE: sets the flags to use when playing the task
	void SetFlags(eSyncedSceneFlagsBitSet flags) {m_flags = flags;}

	// PURPOSE: sets the ragdoll blocking flags to use when playing the task
	void SetRagdollBlockingFlags(eRagdollBlockingFlagsBitSet flags) {m_ragdollFlags = flags;}

	// PURPOSE: sets the ragdoll blocking flags to use when playing the task
	void SetIkControlFlags(eIkControlFlagsBitSet flags) {m_ikFlags = flags;}

	// PURPOSE: Get a reference to the timeline control
	inline CDebugTimelineControl& GetTimeLine() {return m_timeLine;}

	// PURPOSE: Get a reference to the tools root matrix
	inline Matrix34& GetRootMatrix() { return m_rootMatrix; }

	// PURPOSE: Instructs the placement tool to recalculate the ui orientation vector
	//			Call this after manually changing the rotation on the tool's root matrix
	void RecalcOrientation();

	void ClearParentEntity() { m_parentEntity = NULL; }

private:

	//create the ped we're going to use for testing.
	void CreatePed(fwModelId pedModelId);

	//create the ped we're going to use for testing.
	void CreateObject(fwModelId modelId);

	//create the ped we're going to use for testing.
	void CreateVehicle(fwModelId vehicleModelIndex);

	//cleanup the test ped.
	void CleanupEntity();

	//updates the anim combo box when the dictionary changes
	void SetDictionary();

	// PURPOSE: Sets the anim that will be used to position the ped
	void StartAnim();

	// PURPOSE: Builds the root matrix from the heading, pitch and roll values
	void BuildMatrix();

	// PURPOSE: set up some generic starting values
	void Initialise();


public:
	static bool		sm_bRenderMoverTracks;	// Set this to false to turn off mover track rendering on all tools
	static bool		sm_bRenderAnimValues;	// Set this to false to turn off mover track rendering on all tools
	static bool		sm_bRenderDebugDraw;

private:
	RegdDyn	m_pDynamicEntity;	// The dynamic entity this tool uses
	//RegdPed			m_pPed;			// The ped created for testing
	Matrix34		m_rootMatrix;	// The root matrix for playing the anim
	//CAnimPlayer*	m_pPlayer;		// The AnimPlayer we're using to position the ped

	CDebugTimelineControl m_timeLine;	// RAG widget for controlling the phase in the anim

	Vector3			m_orientation;		// The heading pitch and roll values displayed in rag - used to build the root matrix

	atArray<bkGroup*>	m_toolGroups;		// A pointer to the tool's RAG widget group kept for cleanup purposes

	CDebugClipSelector m_animSelector;		// An anim selector to pick the anim

	eSyncedSceneFlagsBitSet		m_flags;
	eRagdollBlockingFlagsBitSet m_ragdollFlags;
	eIkControlFlagsBitSet		m_ikFlags;

	atString		m_scriptCall;			// String storage for the script call that will reproduce this anim

	datCallback	m_animChosenCallback;	// custom callback that can be called when an anim is chosen in the tool

	bool			m_bOwnsEntity;			//If true, the tool has created its own ped to use
										// if false, the ped was passed in when the tool was created, and should not be deleted or changed

	CDebugAttachmentTool* m_attachmentTool; //pointer to an attachment tool

	RegdPhy	m_parentEntity;				//Attached Entity			

	bool m_bLoopWithinScene;
	bool m_bPedWasInRagdoll;
	u32 m_uExitSceneTime;			// time the entity first exited the scene
};

//////////////////////////////////////////////////////////////////////////
//	CSyncedSceneEntityManager
//////////////////////////////////////////////////////////////////////////

class CSyncedSceneEntityManager : public fwSyncedSceneEntityManager
{
public:

	virtual ~CSyncedSceneEntityManager();

	CSyncedSceneEntityManager();

	virtual bool Save(const char * name, const char * folder);

	virtual bool Load(const char * name, const char * folder);

protected:

	void CreateAttachedEntites(u32 index);

	void CreateAttachedWidgets(u32 index, fwEntity* pEnt);

	void CreateSyncSceneWidgets();

	void StoreSceneInfo();

	void PopulateEntityList();

	void GenerateLocationInfoFromSceneInfo();
};

//////////////////////////////////////////////////////////////////////////
//	CLocationInfo - For the possible locations of the synced scene
//////////////////////////////////////////////////////////////////////////

class CLocationInfo
{
public:
	CLocationInfo()
	{
		m_name[0] = '\0';
		m_masterRootMatrix.Set(M34_IDENTITY);
		m_masterOrientation.Set(VEC3_ZERO);
		m_bSelected = false;
	}

	~CLocationInfo() {}

	// Name
	char m_name[64];

	// The root matrix for all tools to use in sync mode
	Matrix34 m_masterRootMatrix;
	Vector3	 m_masterOrientation; // The heading pitch and roll values displayed in rag - used to build the master root matrix

	// Is this the currently selected location?
	bool m_bSelected;
};

//////////////////////////////////////////////////////////////////////////
//	Anim placement editor
//////////////////////////////////////////////////////////////////////////

class CAnimPlacementEditor : datBase
{
public:

	static void Initialise();
	static void ActivateBank();

	static void Update();

	static void DeactivateBank();
	static void Shutdown(); 

	static inline void ToggleEditor()
	{
		if (m_pBank)
		{
			m_pBank->ToggleActive();
		}
	}

	//static void CreateBank();
	static void RebuildBank();
	static void AddWidgets(bkBank* pBank);
	static void RemoveWidgets();

	static void AddSyncedSceneFlagWidgets(bkBank* pBank);
	static void AddRagdollBlockingFlagWidgets(bkBank* pBank);
	static void AddIkControlFlagWidgets(bkBank* pBank);

	static void AudioWidgetChanged();

	static void AddAnimPlacementToolSelectedPed();		//Creates a new anim placement tool using an existing ped
	static void AddAnimPlacementToolCB();					//creates a new anim placement tool and appends it to the end of the editor bank
	static void AddAnimPlacementTool(fwModelId ModelId, const Matrix34 &SceneMat); 

	static void AddAnimPlacementToolSelectedObject();		//Creates a new anim placement tool using an existing ped
	static void AddAnimPlacementToolObjectCB();				//creates a new anim placement tool and appends it to the end of the editor bank
	static void AddAnimPlacementToolObject(fwModelId ModelId, const Matrix34 &SceneMat); 

	static void AddAnimPlacementToolSelectedVehicle();		//Creates a new anim placement tool using an existing vehicle
	static void AddAnimPlacementToolVehicleCB();			//creates a new anim placement tool and appends it to the end of the editor bank
	static void AddAnimPlacementToolVehicle(fwModelId ModelId, const Matrix34 &SceneMat); 

	static void AddAnimPlacementToolAttachedProp(fwModelId ModelId, CDynamicEntity* pEnt ); 

	static void DeleteTool(CAnimPlacementTool* tool);	//remove the specified tool

	static void ActivateSyncMode();	// activates the synchronised editing mode

	// PURPOSE: Builds the root matrix from the heading, pitch and roll values
	static void BuildMasterMatrix();

	// PURPOSE: Outputs the script calls for the current set of anim placement tools
	static void OutputScriptCalls();

	// PURPOSE: Updates mouse click and drag control for the placement tools
	static void UpdateMouseControl();

	// PURPOSE: Plays / pauses all anims 
	static void TogglePause();

	// PURPOSE: Callback to tell us when the selected entity is changed
	static void FocusEntityChanged(CEntity* pNewEntity);

	// PURPOSE: Returns true if one of it's child tools owns the specified ped, false otherwise
	static bool IsUsingPed(CPed* pPed);

	// PURPOSE: Rotates the tool that owns the specified ped by the specified amount around its local z axis
	static void RotateTool(CPed* pPed, float deltaHeading);

	// PURPOSE: Translates the tool that owns the specified ped by the provided position delta
	static void TranslateTool(CPed* pPed, const Vector3 &deltaPosition);

	// PURPOSE: Adds an animated prop attachment to the selected tool ped
	static void CreateAnimatedProp();

	// PURPOSE: Creates an animated camera using the selected anim
	static void SelectCameraAnim();

	// PURPOSE: Attempts to attach the scene to the selected entity
	static void AttachScene();

	// PURPOSE: Detaches the placement editor scene if necessary
	static void DetachScene();

	// PURPOSE: Callback function from entity change. Updates the attach bones combo with the
	//			bones from the selected entity.
	static void UpdateAttachBoneList(CEntity* pEnt);

	// PURPOSE: Enumerates the files and folders found in the scene folder
	static void SceneFolderEnumerator(const fiFindData &findData, void *pUserArg);

	// PURPOSE: Callback to tell us when the scene folder is changed
	static void SceneFolderChanged();

	// PURPOSE: Callback to tell us when the scene name is changed
	static void SceneNameChanged();

	//PURPOSE: Save the contents of the widgets to a xml file
	static void SaveSyncedSceneFile();
	
	//PURPOSE: Creates a synced scene and all the widgets from the xml file
	static void LoadSyncedScene();

	//PURPOSE: Recreates the location widgets
	static void UpdateLocationWidgets();
	
	//PURPOSE: Adds a location to the list
	static void AddLocation();

	//PURPOSE: Removes a location from the list
	static void RemoveLocation(CLocationInfo* pLocationInfo);

	//PURPOSE: Selects a location from the list
	static void SelectLocation(CLocationInfo* pLocationInfo);

	// structure for anim placement tools
	static atArray<CAnimPlacementTool> m_animPlacementTools;

	// If true, the anims will operate in synchronised mode (i.e. a single root matrix will be
	// used for all anim tools, and their authored initial offsets will be applied)
	static bool sm_bSyncMode;

	// Locations
	static const u32 locationNameBufferSize = 64;
	static char m_newLocationName[locationNameBufferSize];
	static bkGroup* sm_pLocationsGroup;
	static atArray<CLocationInfo*> sm_Locations;
	static int	sm_SelectedLocationIdx;

	static float sm_masterPhase;					// the current phase value

	static float sm_masterDuration;					// the current duration value

	static bool sm_masterPause;					// If true, the tool will be paused

	static atHashString sm_AudioWidget;
	static atHashString sm_Audio;
	static bool sm_IsAudioSkipped;
	static bool sm_IsAudioPrepared;
	static bool sm_IsAudioPlaying;

	static CDebugPedModelSelector sm_modelSelector;		// The ped model to use when adding new placement tools
	static CDebugVehicleSelector sm_vehicleSelector;	// The vehicle model to use when adding new placement tools
	static CDebugObjectSelector sm_objectSelector;		// The ped model to use when adding new placement tools

	static float sm_BlendInDelta;
	static float sm_MoverBlendInDelta;
	static float sm_BlendOutDelta;

	static eSyncedSceneFlagsBitSet sm_SyncedSceneFlags;
	static eRagdollBlockingFlagsBitSet sm_RagdollBlockingFlags;
	static eIkControlFlagsBitSet sm_IkControlFlags;

	static CAnimPlacementTool* sm_selectedTool;		//The currently selected tool for the purposes of mouse control

	static fwDebugBank* m_pBank;							//pointer to the main rag bank

	static atArray<bkGroup*> m_toolGroups;			// store pointers to all the groups we've added our widgets to

	static bool m_rebuildWidgets;					// set this to true to force the system to rebuild its widgets on the next update

	//////////////////////////////////////////////////////////////////////////
	// Camera Editing
	//////////////////////////////////////////////////////////////////////////
	static bool m_bOverrideCam; 
	static Vector3 m_vCameraPos; 
	static Vector3 m_vCameraRot; 

	static bool m_bOverrideCamUsingMatrix;
	static Matrix34 m_cameraMtx;

	static float m_fFov; 
	static Vector4 m_vDofPlanes; 
	static bool m_bShouldOverrideUseLightDof;
	static float m_fMotionBlurStrength;

	//////////////////////////////////////////////////////////////////////////
	// Animated prop support
	//////////////////////////////////////////////////////////////////////////
	static CDebugObjectSelector ms_propSelector;	// Selection box for choosing models when attaching animated props

	static fwSyncedSceneId ms_sceneId;

	static CDebugClipSelector ms_cameraAnimSelector;
	static bool ms_UseAnimatedCamera;

	static atArray<bkCombo*> ms_attachBoneSelectors;
	static int ms_selectedAttachBone;

	static CSyncedSceneEntityManager m_SyncedSceneEntityManager; 

	static bkCombo *ms_pSceneNameCombo;
	static char ms_SceneFolder[RAGE_MAX_PATH];
	static atArray< atString > ms_SceneNameStrings;
	static atArray< const char * > ms_SceneNames;
	static int ms_iSceneName;
	static char ms_SceneName[RAGE_MAX_PATH];
};

#endif // ANIM_PLACEMENT_TOOL_H

#endif // __BANK