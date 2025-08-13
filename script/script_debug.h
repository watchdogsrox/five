#ifndef _SCRIPT_DEBUG_H_
	#define _SCRIPT_DEBUG_H_

// Rage includes
#include "bank/bank.h"
#include "script/script_memory_tracking.h"
#include "script/thread.h"
#include "vector/color32.h"
#include "vector/colors.h"

// Game headers
#include "Animation/debug/AnimDebug.h"

#if __BANK
class CDebugAttachmentTool
{
public:

	CDebugAttachmentTool() :
		m_parent(NULL),
		m_child(NULL)
		{
			//nothing to do here
		}

		// PURPOSE: Add the necessary widgets to control this attachment to the given bank
		void AddWidgets(bkBank* pBank, const char * title);

		// PURPOSE: Removes any widgets added by this tool
		void RemoveWidgets();

		// PURPOSE: Removes any references to added widgets and zeros any values (for level shutdown)
		void Shutdown();

		// PURPOSE: Initialises the attachment tool with the provided CPhysical objects
		// PARAMS:	pChild, a pointer to the child object to be attached - must be non-null
		//			pParent, an optional pointer to the parent object to be attached to (if null, the child will be attached to the world)
		void Initialise(CPhysical* pChild, CPhysical* pParent = NULL);

		// PURPOSE: Attach the entities based on the current settings. Will detach and reattach if necessary
		void Attach();

		// PURPOSE: Detach the entities
		void Detach();
	
		// PURPOSE: Per frame update for the tool
		void Update();

		// PURPOSE: Get the script call that will reproduce the current behaviour of the attachment tool
		const char * GetScriptCall();

		s16 GetParentBone() {return (s16)(m_parentBone-1);}
		s16 GetChildBone() {return (s16)(m_childBone-1);}

		CPhysical* GetParent() const { return m_parent; }

		Vector3 GetOffset() const { return m_positionOffset; }

		Vector3 GetRotation() const { return m_rotationOffset; }

		void SetPosition(const Vector3& vVec) { m_positionOffset = vVec; }

		void SetRotation(const Vector3& vVec) { m_rotationOffset = vVec; }

		void SetParentBone(s16 sParentBone) { m_parentBone = sParentBone + 1; }

		void SetChildBone(s16 sChildBone) { m_childBone = sChildBone + 1; }

		void RefreshBoneList(bool rebuildWidgets = false)
		{
			LoadBoneList();

			if (rebuildWidgets)
			{
				TriggerRebuild();
			}
		}

private:

	// PURPOSE: zero some initial settings during shutdown and initialisation
	void ClearSettings();

	// PURPOSE: helper method for adding and regenerating widgets
	void CreateWidgets(bkBank* pBank);

	// PURPOSE: called by the update system when widgets need to be regenerated
	void RegenerateWidgets();

	// PURPOSE: used by the callback system to signal when widgets need to be regenerated
	void TriggerRebuild();

	// PURPOSE: Sets offset position and orientation from the selected child bone
	void BuildOffsetsFromChildBone();

	// PURPOSE: Writes the script call necessary to reproduce the tools current behaviour to the 
	//			console and debug output text.
	void OutputScript();

	// PURPOSE: Locates and returns a reference to the bank containing the specified widget
	// RETURNS: A pointer to the bank if found
	//			NULL if bank not found
	bkBank* FindBank(bkWidget* pWidget);

	// PURPOSE: Destroys all child widgets attached to the specified widget
	void DestroyAllChildren(bkWidget* pWidget);

	// Purpose: Loads the list of bones into the pointer array
	void LoadBoneList();

	// PURPOSE: Callback method to update the position offset using the RAG sliders
	void UpdatePositionOffset();

	// PURPOSE: Callback method to update the rotation offset using the RAG sliders
	void UpdateRotationOffset();

	// PURPOSE: helper function for attaching peds in script mode
	void ScriptAttachPed();

	// PURPOSE: helper function for attaching vehicles and objects in script mode
	void ScriptAttachOther();


	RegdPhy m_parent;	//pointer to the parent object (the one we're attaching to). If null the child will be attached to the world
	RegdPhy m_child;	//pointer to the object that is attached

	Vector3 m_positionOffset;		//The position offset
	Vector3 m_rotationOffset;	//The rotation offset

	s32 m_parentBone;	//The bone on the parent to attach to
	s32 m_childBone;		//The bone on the child to attach to
	
	bool m_bDetachOnDeath;
	bool m_bDetachOnRagdoll;

	bool m_bAttachPhysically;

	bool m_bPhysicalConstrainRotation;
	float m_strength;					// Higher values make the physical attachment harder to break
	Vector3 m_physicalPivotOffset;		// The offset on the target object to pivot about when attaching objects physically
	bool m_bChildCollidesWithParent;

	bool m_bFixRotation;		// if true, the peds rotation will be fixed when attached physically to the world

	bool m_bNeedsRebuild;		// if true, the tool will regenerate its widgets on the next update

	atArray<bkGroup*> m_bankInstances;		//pointers to the added widgets

	atArray<const char *> m_parentBoneList;	//pointers to the bone names for the current parent
	atArray<const char *> m_childBoneList;	//pointers to the bone names for the current parent

};
#endif	//	__BANK

#if __BANK

enum eWidgetType
{
	WIDGETTYPE_STANDARD,
	WIDGETTYPE_GROUP,
	WIDGETTYPE_TEXT,
	WIDGETTYPE_BITFIELD
};


class CScriptWidgetTree
{
private:
	class CScriptWidgetNode
	{
	public:
		CScriptWidgetNode(u32 UniqueId, eWidgetType Type);
		~CScriptWidgetNode();

		void AddChildNode(CScriptWidgetNode *pNewChild);
		CScriptWidgetNode *FindWidget(u32 UniqueId);

		eWidgetType GetWidgetType() { return m_WidgetType; }
		CScriptWidgetNode *GetParent() { return m_pParent; }

		u32 GetUniqueId() { return m_UniqueId; }

	private:
		void ClearChildrenAndData();
		void RemoveSelf();
		void ClearData();

	private:
		u32 m_UniqueId;
		CScriptWidgetNode *m_pParent;
		CScriptWidgetNode *m_pChild;
		CScriptWidgetNode *m_pSibling;

		eWidgetType m_WidgetType;
	};

public:
	CScriptWidgetTree();
	~CScriptWidgetTree();

	u32 AddWidget(bkWidget *pWidget, eWidgetType WidgetType, const char *pScriptName);
	void DeleteWidget(u32 UniqueId, bool bIsWidgetGroup, const char *pScriptName);

	void SetCurrentWidgetGroup(u32 UniqueId, const char *pScriptName);
	void ClearCurrentWidgetGroup(u32 UniqueId, const char *pScriptName);
	bool HasCurrentWidgetGroup() const { return ms_pCurrentWidgetNode != 0; }

	void RemoveAllWidgetNodes();

	u32 GetUniqueWidgetIdOfTopLevelWidget(const char *pScriptName);

	static void CloseWidgetGroup();

	static void CheckWidgetGroupsHaveBeenClosed(const char *pScriptName);

	static bkWidget *GetWidgetFromUniqueWidgetId(u32 UniqueId);

private:
	CScriptWidgetNode *FindWidgetNodeWithThisUniqueId(u32 UniqueId);

private:
	CScriptWidgetNode *m_pTopLevelWidget;

	static CScriptWidgetNode *ms_pCurrentWidgetNode;
};


class CScriptDebugTextWidgets
{
	struct TextEditWidgetForScript
	{
		static const s32 MAX_CHARACTERS_FOR_TEXT_WIDGET	= 64;
		char WidgetTextContents[MAX_CHARACTERS_FOR_TEXT_WIDGET];
		bkText *pWidget;
	};

public:
	void Init();

	s32 AddTextWidget(const char *pName);
	void ClearTextWidget(bkText *pWidgetToMatch);

	const char *GetContentsOfTextWidget(s32 UniqueTextWidgetIndex);
	void SetContentsOfTextWidget(s32 UniqueTextWidgetIndex, const char *pNewContents);

private:
	s32 FindFreeTextWidget();

private:

	static const s32 MAX_NUMBER_OF_TEXT_WIDGETS	= 500;
	TextEditWidgetForScript TextWidgets[MAX_NUMBER_OF_TEXT_WIDGETS];
};

class CScriptDebugBitFieldWidgets
{
	struct WidgetBitFieldStruct
	{
		bkGroup* pGroup;
		bool bBits[32];
	//		bkToggle *pBitToggles[32];		//	or should these be pointers to bkWidget. Do I actually need to store these at all? Are they ever referred to?
		s32 *pBitFieldWithinScript;
		s32 PreviousFrameBitField;

		void Init();
	};

public:
	void Init();

	s32 AddBitFieldWidget(const char *pName, s32& value);
	void ClearBitFieldWidget(bkGroup *pWidgetGroupToMatch);

	void UpdateBitFields();

private:
	s32 FindFreeBitFieldWidget();

private:
	static const s32 MAX_NUMBER_OF_BIT_FIELD_WIDGETS = 200;
	WidgetBitFieldStruct BitFields[MAX_NUMBER_OF_BIT_FIELD_WIDGETS];
};


class CScriptDebugLocateHandler
{
public:
	CScriptDebugLocateHandler();

	void CreateWidgets(bkBank* pBank);

	void DisplayScriptLocates();

	void DisplaySpheresForNonAxisAlignedLocate();

private:
	void GetLocateDebugInfo();

private:
	char m_AxisAlignLocateName[256];
	char m_NonAxisAlignLocateName[256];

	Vector3 m_VecLocateDimensions;

	float m_fLocateOffset;
	float m_fLocateOffset2;

	float m_AreaWidth;
	float m_Radius;

	bool m_AxisAlignedLocate;
	bool m_NonAxisAlignedLocate;
	bool m_SphereLocate;
};

class CScriptDebugPedHandler
{
public:
	void CreateWidgets(bkBank* pBank);

	void Process();

	CPed* CreateScriptPed(u32 modelIndex);
	
	static bool GetVectorFromTextBox(Vector3 &vPos, const char* TextBox); 
	static bool GetFloatFromTextBox(float &fHeading, const char* TextBox);

private:
	void CreateScriptPedUsingTheSelectedModel();
	void DeleteScriptPed();

	void SetDebugPedName();
	void GetPedDebugInfo();
	void SetPedPosCB(); 
	void SetPedHeadingCB(); 
	
private:
	char m_PedPos[256];
	char m_PedHeading[256];
	char m_PedName[15]; 
	char m_PedModelName[256];

	CDebugPedModelSelector m_pedSelector;

	char m_searchPedName[256]; 
};



class CScriptDebugVehicleHandler
{
public:
	void CreateWidgets(bkBank* pBank);
	void RemoveWidgets();

	void Process();

	CVehicle* CreateScriptVehicle(u32 modelIndex);

private:
	void CreateScriptVehicleUsingTheSelectedModel();
	void DeleteScriptVehicle();

	void SetDebugVehicleName();
	void SetVehicleOnGround();
	void GetVehicleDebugInfo();
	void SetVehiclePosCB(); 
	void SetVehicleHeadingCB(); 
private:
	char m_VehiclePos[256];
	char m_VehicleHeading[256];
	char m_VehicleName[15]; 
	char m_VehicleModelName[256];

	CDebugVehicleSelector m_vehicleSelector;
	char m_vehicleSearchName[256]; 
};


struct BuidlingDescription  
{
	RegdEnt pEnt; 
	char ObjectModelName[STORE_NAME_LENGTH];
};

class CScriptDebugObjectHandler
{
public:


	void CreateWidgets(bkBank* pBank);
	void RemoveWidgets();

	void Process();

	CObject* CreateScriptObject(u32 modelIndex);

private:
	void CreateScriptObjectUsingTheSelectedModel();
	void DeleteScriptObject();

	void ActivateObjectPhysics();
	void ReloadModelList();
	void GetObjectDebugInfo();
	
	void SetObjectPosCB(); 
	void SetObjectRotCB(); 
	void PopulateBuidingList(); 

private:
	char m_ObjectPos[256];
	char m_ObjectRot[256]; 
	char m_ObjectName[256];
	char m_ObjectDebugName[256];
	char m_ObjectType[256];
	bkCombo* m_CreatedBuildingsList; 
	s32 CreatedBuildingIndex; 
	atArray<BuidlingDescription>m_CreatedBuildings; 

	CDebugObjectSelector m_objectSelector;	// object selection combo box - used by the object spawner
	char m_ObjectModelName[STORE_NAME_LENGTH];				// string for typing in a model name, useful if you already know it
};

#endif


//	Forward declaration
class GtaThread;

class CScriptDebug
{
public:
//	Public functions
	static void Init(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);
	static void Process(void);

	static void DrawDebugSquare(float X1, float Y1, float X2, float Y2);
	static void DrawDebugCube(float X1, float Y1, float Z1, float X2, float Y2, float Z2);
	static void DrawDebugAngledSquare(float X1, float Y1, float X2, float Y2, float X3, float Y3, float X4, float Y4);
	static void DrawDebugAngledCube(float X1, float Y1, float Z1, float X2, float Y2, float Z2, float X3, float Y3, float X4, float Y4, const Color32 &colour = Color_blue);

#if __BANK
	static bool ShouldThisScriptBeDebugged(const char *pScriptName);
	static const char *GetScriptDebuggerPath() { return ms_pScriptDebuggerPath; }

	static void SetComboBoxOfThreadNamesNeedsUpdated(bool bNeedsUpdated) { sm_ComboBoxOfThreadNamesNeedsUpdated = bNeedsUpdated; }

	static void InitWidgets();

	static u32 AddWidget(bkWidget *pWidget, eWidgetType WidgetType);

	static u32 GetUniqueWidgetIdOfTopLevelWidget();

	static void SetContentsOfFadeCommandTextWidget(const char *pFadeCommandString);

	static const char *GetNameOfScriptDebugOutputFile() { return "debug:/temp_debug.txt"; }

	// PURPOSE: Outputs the string provided to the RAG console and the temp_debug text file on the hard drive
	//			Used by script tools.
	static void OutputScript(const char * text);

	static inline const char * GetScriptBool( bool value ) { return value ? "TRUE" : "FALSE"; }

	static CEntity* CreateScriptEntity(u32 modelIndex);

	static Vector3 &GetClickedPos() { return ms_vClickedPos; }
	static Vector3 &GetClickedPos2() { return ms_vClickedPos2; }
	static Vector3 &GetClickedNormal2() { return ms_vClickedNormal2; }
	static bool GetDrawDebugLinesAndSpheres() { return ms_bDrawDebugLinesAndSpheres; }
	static void SetDrawDebugLinesAndSpheres(bool bDrawDebugLines) { ms_bDrawDebugLinesAndSpheres = bDrawDebugLines; }

	static bool GetOutputScriptDisplayTextCommands() { return ms_bOutputScriptDisplayTextCommands; }

	static bool GetDebugMaskCommands() { return ms_bDebugMaskCommands; }

	static fwDebugBank *GetWidgetBank() { return ms_pBank; }

	static CScriptDebugBitFieldWidgets &GetBitFieldWidgets() { return ms_BitFieldWidgets; }
	static CScriptDebugTextWidgets &GetTextWidgets() { return ms_TextWidgets; }

	static bool &GetAutoSaveEnabled() { return ms_bAutoSaveEnabled; }
	static bool &GetPerformAutoSave() { return ms_bPerformAutoSave; }
#if RSG_PC
	static bool &GetFakeTampering() { return ms_bFakeTampering; }
#endif 

#if SCRIPT_PROFILING
	static void DisplayProfileDataForThisThread(GtaThread *pThread);
#endif	//	SCRIPT_PROFILING

	static bool &GetEnablePedDragging() { return ms_bEnablePedDragging; }
	static bool &GetUseLineofSight() { return ms_bUseLineofSight; }

	static bool &GetOutputListOfAllScriptSpritesWhenLimitIsHit() { return ms_bOutputListOfAllScriptSpritesWhenLimitIsHit; }

	static bool GetWriteNetworkCommandsToScriptLog() { return ms_bWriteNetworkCommandsToScriptLog; }
	static bool GetDisplayNetworkCommandsInConsole() { return  ms_bDisplayNetworkCommandsInConsole; }

	static bool GetWriteIS_NETWORK_SESSIONCommand() { return ms_bWriteIS_NETWORK_SESSIONCommand; }
	static bool GetWriteNETWORK_HAVE_SUMMONSCommand() { return ms_bWriteNETWORK_HAVE_SUMMONSCommand; }
	static bool GetWriteIS_NETWORK_GAME_RUNNINGCommand() { return ms_bWriteIS_NETWORK_GAME_RUNNINGCommand; }
	static bool GetWriteIS_NETWORK_PLAYER_ACTIVECommand() { return ms_bWriteIS_NETWORK_PLAYER_ACTIVECommand; }
	static bool GetWritePLAYER_WANTS_TO_JOIN_NETWORK_GAMECommand() { return ms_bWritePLAYER_WANTS_TO_JOIN_NETWORK_GAMECommand; }
#endif	//	__BANK

#if __FINAL
	static bool GetDbgFlag() { return false; }
#else
	static bool GetDbgFlag() { return ms_bDbgFlag; }
	static void SetDbgFlag(bool bDebugFlag) { ms_bDbgFlag = bDebugFlag; }

	static bool GetDisplayScriptRequests() { return ms_bDisplayScriptRequests; }
#endif

#if !__FINAL
	static bool GetDisableDebugCamAndPlayerWarping() { return ms_bDisableDebugCamAndPlayerWarping; }	//	For build for Image Metrics

	//	Some variables for controlling what gets displayed in the script model usage debug text
//	static bool &GetDisplayModelUsagePerScript() { return ms_bDisplayModelUsagePerScript; }
//	static bool &GetCountModelUsage() { return ms_bCountModelUsage; }
//	static bool &GetCountAnimUsage() { return ms_bCountAnimUsage; }
//	static bool &GetCountTextureUsage() { return ms_bCountTextureUsage; }

	static void SetPlayerCoordsHaveBeenOverridden(bool bOverridden) {sm_bPlayerCoordsHaveBeenOverridden = bOverridden; }
	static bool GetPlayerCoordsHaveBeenOverridden() { return sm_bPlayerCoordsHaveBeenOverridden; }
#endif

#if __SCRIPT_MEM_DISPLAY

	static bool &GetDisplayScriptMemoryUsage() { return ms_bDisplayScriptMemoryUsage; }
	static bool &GetDisplayDetailedScriptMemoryUsage() { return ms_bDisplayDetailedScriptMemoryUsage; }
	static bool &GetDisplayDetailedScriptMemoryUsageFiltered() { return ms_bDisplayDetailedScriptMemoryUsageFiltered; }
	static bool &GetExcludeScriptFromScriptMemoryUsage() { return ms_bExcludeScriptFromMemoryUsage; }

#endif	//	__SCRIPT_MEM_DISPLAY

private:
//	Private functions
#if __BANK
	static void DeactivateBank(); 
	static void CreatePermanentWidgets(fwDebugBank* pBank);
	static void ActivateBank(); 

	static void ProcessScriptDebugTools();
	static void UpdateCreatePos();
	static void UpdateFloatingCreatePos(Vector3 &CreatePos);

	static void ToggleClickingObjectsCB();
	static void ActivateClickingObjectsCB(); 

	static void AttachSelectedObjects(); // Create an attachment between the two selected objects
	static void ClearAttachments();
	static void UpdateAttachmentTools();

	static void LaunchScriptDebuggerForSelectedId();
	static void LaunchDebuggerForThreadSelectedInCombo();
	static void EnableProfilingForThreadSelectedInCombo();

#if SCRIPT_PROFILING
	static void DisplayProfileOverviewOfAllThreads();
#endif	//	SCRIPT_PROFILING

#endif	//	__BANK

private:
//	Private data
#if __BANK
	static bool ms_bDisplayRunningScripts;
	static bool ms_bDisplayNumberOfScriptResources;
	static bool ms_bPrintCurrentContentsOfCleanupArray;

	static bool ms_bAutoSaveEnabled;
	static bool ms_bPerformAutoSave;
	static bool ms_bFadeInAfterLoadReadOnly;
	static bool ms_bDrawDebugLinesAndSpheres;
	static bool ms_bDebugMaskCommands;
	static bool ms_bPrintSuppressedCarModels;
	static bool ms_bPrintSuppressedPedModels;
	static bool ms_bPrintRestrictedPedModels;

	static bool ms_bOutputScriptDisplayTextCommands;
	static s32 ms_NumberOfFramesToOutputScriptDisplayTextCommands;

	static bool ms_bOutputListOfAllScriptSpritesWhenLimitIsHit;

	static bool ms_bWriteNetworkCommandsToScriptLog;	//	Display all strings passed to network_commands::WriteToScriptLogFile to the Script.log file
	static bool ms_bDisplayNetworkCommandsInConsole;	//	Display all strings passed to network_commands::WriteToScriptLogFile in the console window

	static bool ms_bFadeInAfterDeathArrestReadOnly;
	static bool ms_bPauseDeathArrestRestartReadOnly;
	static bool ms_bIgnoreNextRestartReadOnly;

	static bool ms_bWriteIS_NETWORK_SESSIONCommand;					//	These flags determine
	static bool ms_bWriteNETWORK_HAVE_SUMMONSCommand;				//	whether specific
	static bool ms_bWriteIS_NETWORK_GAME_RUNNINGCommand;			//	commands will be
	static bool ms_bWriteIS_NETWORK_PLAYER_ACTIVECommand;			//	written to script.log
	static bool ms_bWritePLAYER_WANTS_TO_JOIN_NETWORK_GAMECommand;	//	and console

	static bool ms_bEnablePedDragging;
	static char ms_CreatePos[256];
#if RSG_PC
	static bool	ms_bFakeTampering;
#endif
	static CScriptDebugPedHandler ms_PedHandler;

	static CScriptDebugVehicleHandler ms_VehicleHandler;

	static CScriptDebugObjectHandler ms_ObjectHandler;

	static CScriptDebugLocateHandler ms_LocateHandler;

	static Vector3 ms_vClickedPos;
	static Vector3 ms_vClickedNormal;
	static Vector3 ms_vClickedPos2;
	static Vector3 ms_vClickedNormal2;
	static float ms_fPosFromCameraFront;
	static bool ms_bUseLineofSight;

	#define MAX_LENGTH_OF_FADE_COMMAND_TEXT_WIDGET	(256)
	static char ms_FadeCommandTextContents[MAX_LENGTH_OF_FADE_COMMAND_TEXT_WIDGET];

	static fwDebugBank *ms_pBank;
	static bkCombo *ms_pComboBoxOfScriptThreads;
	static atArray<const char*> sm_ThreadNamesForDebugger;
	static s32 sm_ArrayIndexOfThreadToAttachDebuggerTo;
	static bool sm_ComboBoxOfThreadNamesNeedsUpdated;

	static CScriptDebugTextWidgets ms_TextWidgets;
	static CScriptDebugBitFieldWidgets ms_BitFieldWidgets;

#if SCRIPT_PROFILING
	static bool ms_bDisplayProfileOverview;
	static s32 ms_ProfileDisplayStartRow;
#endif	//	SCRIPT_PROFILING

	static atArray<CDebugAttachmentTool> ms_attachmentTools;
	static bkGroup* ms_pAttachmentGroup;

	static const char *ms_pScriptDebuggerPath;
	static atArray<atString> ms_ArrayOfNamesOfScriptsToBeDebugged;

	static char ms_ScriptThreadIdForDebugging[];
#endif	//	__BANK

#if !__FINAL
	static bool ms_bDbgFlag;					// If true stuff gets printed out

	static bool ms_bDisplayScriptRequests;

	static bool ms_bDisableDebugCamAndPlayerWarping;	//	For build for Image Metrics

	//	Some variables for controlling what gets displayed in the script model usage debug text
//	static bool ms_bDisplayModelUsagePerScript;
//	static bool ms_bCountModelUsage;
//	static bool ms_bCountAnimUsage;
//	static bool ms_bCountTextureUsage;

	static bool sm_bPlayerCoordsHaveBeenOverridden;
#endif	//	!__FINAL

#if __SCRIPT_MEM_DISPLAY
	static bool ms_bDisplayScriptMemoryUsage;
	static bool ms_bDisplayDetailedScriptMemoryUsage;
	static bool ms_bDisplayDetailedScriptMemoryUsageFiltered;
	static bool ms_bExcludeScriptFromMemoryUsage;
#endif	//	__SCRIPT_MEM_DISPLAY
};

#endif	//	_SCRIPT_DEBUG_H_

