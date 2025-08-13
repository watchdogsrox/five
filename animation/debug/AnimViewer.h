// 
// animation/debug/AnimViewer.h
// 
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved. 
// 

#ifndef ANIM_VIEWER_H
#define ANIM_VIEWER_H

#if __BANK

#include <string>
#include <vector>

// Rage includes
#include "atl/slist.h"
#include "atl/string.h"
#include "bank/bank.h"
#include "bank/bkmgr.h"
#include "bank/data.h"
#include "crmotiontree/iterator.h"
#include "data/base.h"
#include "fwdebug/debugbank.h"

// Gta includes
#include "animation/animbones.h"
#include "animation/debug/animdebug.h"
#include "animation/moveped.h"
#include "animation/pmdictionarystore.h"
#include "audio/speechaudioentity.h"
#include "fwanimation/directorcomponentragdoll.h"
#include "fwanimation/move.h"
#include "fwanimation/movedump.h"
#include "scene/dynamicentity.h"
#include "scene/regdreftypes.h"
#include "task/Animation/AnimTaskFlags.h"

// Rage forward declarations
namespace rage
{
	class crAnimation;
	class bkToggle;
	class bkCombo;
	class bkSlider;
	class crpmParameterizedMotionData;
	class crpmParameterSampler;
	class crClip;
	class crTagGenericInt;
}

// Gta forward declarations
class CTask;
class CMoveParameterWidget;
class CTaskPreviewMoveNetwork;

#define MAX_DEBUG_SKELETON 10
#define MAX_DEBUG_SKELETON_DESCRIPTION 50



class CMotionTreeVisualiserSimpleIterator : public crmtIterator
{	
public:
	CMotionTreeVisualiserSimpleIterator(crmtNode* referenceNode = NULL)
	{ 
		(void)referenceNode;
		m_noOfTexts = 0;
	};

	~CMotionTreeVisualiserSimpleIterator() {};

	// PURPOSE: Iterator visit override
	virtual void Visit(crmtNode&);
	float CalcVisibleWeight(crmtNode& node);
	int m_noOfTexts;
};


//	fwMotionTreeVisualiser

//
//	Provides functionality to render the state of a running motion tree
//	
//

class fwMotionTreeTester : public crmtIterator
{
public:
	fwMotionTreeTester(){m_complexity=0; m_Depth = 0; m_MaxDepth = 0;};
	virtual ~fwMotionTreeTester(){};

	virtual void PreVisit(crmtNode&);

	// PURPOSE: Iterator visit override
	virtual void Visit(crmtNode& node);

	s32 m_complexity;
	s32 m_Depth;
	s32 m_MaxDepth;
	
	s32 m_numNestedTransitions;
	bool m_bParentWasTransition;

	static s32 ms_maxComplexity;
	static s32 ms_autoPauseComplexity;
	static s32 ms_maxTreeDepth;
	static s32 ms_autoPauseTreeDepth;
};


class fwMotionTreeVisualiser : public crmtIterator
{	
public:
	typedef FixedString<512> FastString;

	fwMotionTreeVisualiser() {};
	~fwMotionTreeVisualiser() {};

	// PURPOSE: Iterator start override
	virtual void Start();

	// PURPOSE: Iterator pre-visit override
	virtual void PreVisit(crmtNode&);

	// PURPOSE: Iterator visit override
	virtual void Visit(crmtNode&);

	// PURPOSE: Helper function. Appends the string for the atHashString if it exists in the string table,
	//			or the hash itself if it does not.
	static void AppendHashStringToString(FastString& string, atHashString hashString)
	{
		const char * cstr = hashString.TryGetCStr();
		
		if (cstr)
		{
			string += cstr;	
		}
		else
		{
			char hashAsString[16];
			sprintf(hashAsString, "H:%u", hashString.GetHash());
			string += hashAsString;
		}
	}

	static void AppendU32(FastString& string, u32 value)
	{
		char buffer[16];
		sprintf(buffer, "%u", value);
		string += buffer;
	}

	static void AppendS32(FastString& string, s32 value)
	{
		char buffer[16];
		sprintf(buffer, "%i", value);
		string += buffer;
	}

	static void AppendFloat(FastString& string, float value)
	{
		char buffer[16];
		sprintf(buffer, "%.4f", value);
		string += buffer;
	}

	static void AddWidgets(bkBank* pBank);

protected:

	Vector2 m_drawPosition;

	void VisualiseClip(crmtNode& node, Vector2 screenPos);
	void VisualiseAnimation(crmtNode& node, Vector2 screenPos);
	void VisualisePair(crmtNode& node, Vector2 screenPos);
	void VisualiseMerge(crmtNode& node, Vector2 screenPos);
	void VisualiseN(crmtNode& node, Vector2 screenPos);
	void VisualiseState(crmtNode& node, Vector2 screenPos);
	void VisualiseTransitions(const mvNodeStateBaseDef* pDef, FastString& string);
	void VisualiseFilter(crmtNode& node, Vector2 screenPos);
	void VisualiseExpression(crmtNode& node, Vector2 screenPos);
	void VisualisePM(crmtNode& node, Vector2 screenPos);

	void GetEventTagText(FastString& text, const crClip* pClip, float startPhase, float endPhase);

	bool GetWeightFromParent(crmtNode& node, float& outWeight);
	bool IsInterestingStateNode (crmtNode& node);
private:
	// PURPOSE: Calculate the visible weight of the current node
	float CalcVisibleWeight(crmtNode *pNode);

	// PURPOSE: Render the text for a node
	void RenderText(const char * text, float nodeWeight);

public:

	CDynamicEntity* m_pDynamicEntity;
	static bool ms_bRenderTree;
	static bool ms_bRenderClass;
	static bool ms_bRenderWeight;
	static bool ms_bRenderLocalWeight;
	static bool ms_bRenderStateActiveName;
	static bool ms_bRenderInvalidNodes;
	static bool ms_bRenderSynchronizerWeights;
	static bool ms_bRenderSynchronizerGraph;
	static bool ms_bRenderRequests;
	static bool ms_bRenderFlags;
	static bool ms_bRenderInputParams;
	static bool ms_bRenderOutputParams;
	static bool ms_bRenderDofs;
	static bool ms_bRenderAddress;
};


// PURPOSE:
class FrameEditorWidgets : public datBase
{
public:

	// PURPOSE:
	FrameEditorWidgets();

	// PURPOSE:
	virtual ~FrameEditorWidgets();

	// PURPOSE:
	void Init(crFrame& frame, const crSkeletonData* skelData, bool readOnly=false);

	// PURPOSE:
	void Shutdown();

	// PURPOSE:
	void AddWidgets(bkGroup&);

	// PURPOSE:
	void ResetWidgets();

	// PURPOSE:
	void UpdateFrame();

private:
	crFrame* m_Frame;
	const crSkeletonData* m_SkelData;
	bool m_ReadOnly;

	atArray<Vec3V> m_WidgetValues;
	atArray<bool> m_WidgetInvalid;
};

//
// CAnimViewer
//
// Provides debug animation functionality
//
// I suggest you also use the command line arguments -nopeds and -nocars to turn off the ambient
// spawning 
// 
// Pick any animation from the combo box.
// Press the Play button to play the animation. Pressing the play button again will restart the animation.
// Set the playback rate using the slider.
// Press the Pause button to pause/upause the animation.
// Set the phase using the slider (while paused). It will update itself during playback.
// *To use the sliders you can type in a value or use the up/down arrows or hold down the 
// slider button down and move the mouse left or right
// You can toggle the animation to loop or hold using a toggle.
//
// There is no custom camera support at the moment. The debug camera functionality* should suffice
// for the time being. 
// *Focus the window using the mouse. Press "d" on the keyboard. Now using the joypad use the left 
// analog stick to look around, the square button to move forward and the cross button to move backward
//

class CEntityCapture : public datBase
{
public:

	/* Destructor */

	~CEntityCapture();

	/* Constructors */

	CEntityCapture(CDynamicEntity *pEntity, bkBank *pBank, bkGroup *pParentGroup);

	/* Properties */

	CDynamicEntity *GetEntity() const { return m_pEntity; }
	bkBank *GetBank() const { return m_pBank; }
	bkGroup *GetParentGroup() const { return m_pParentGroup; }
	bkGroup *GetGroup() const { return m_pGroup; }

	/* Operations */

	void SetPedDefaults();
	void SetHorseDefaults();
	void ShowHideOrigin();
	void ResetOrigin();
	void InitialMoverChanged();

	void Update();
	void Start();
	void RenderUpdate();
	void Stop();
	void SingleFrame();

	void RemoveEntity();

protected:

	/* Data */

	RegdDyn m_pEntity;
	bkBank *m_pBank;
	bkGroup *m_pParentGroup;
	bkGroup *m_pGroup;

	atString m_GroupName;
	char m_szAnimationName[256];
	char m_szCompressionFile[256];
	char m_szSkelFile[256];
	int m_iCaptureFilterNameIndex;
	atArray< const crFrame * > m_FrameArray;
	Mat34V m_InitialMover;
	Vec3V m_InitialMoverPosition;
	float m_InitialMoverRotation[3]; // XYZ Eulers
	CAuthoringHelper *m_pAuthoringHelper;
	bool m_bStarted;
	bool m_bShowOrigin;

private:

	/* Constructors */

	CEntityCapture();
	CEntityCapture(const CEntityCapture &);

	/* Operators */

	CEntityCapture &operator=(const CEntityCapture &);
};

class CAnimViewer
{
public:
	friend class CIkManager;
	friend class CPedBoneTagConvertor;

	struct sPmDictionary
	{
		// animation dictionary name
		strStreamingObjectName p_name;

		// array of pm names (filenames stripped of path and extensions)
		atVector<atString> pmStrings;

		// array of pm names 
		atArray<const char*> pmNames;
	};

	struct sDebugSkeleton
	{
		Color32 m_color;
		Matrix34 m_parentMat;
		Matrix34 m_skelParentMat;
		crSkeleton m_skeleton;
		char m_description[MAX_DEBUG_SKELETON_DESCRIPTION];
		bool m_bRender;

		sDebugSkeleton() :
			m_parentMat(M34_IDENTITY),
			m_skelParentMat(M34_IDENTITY),
			m_bRender(true)
		{
		}

		~sDebugSkeleton()
		{
		}
	};

	struct sBoneHistory
	{
		Matrix34	m_boneMatrix;
		Matrix34	m_moverMatrix;
		float		m_phase;

		sBoneHistory() :
			m_boneMatrix(M34_IDENTITY),
			m_moverMatrix(M34_IDENTITY),
			m_phase(0.0f)
			{

			};

		~sBoneHistory()	{ };
	};


	struct sDebugExpressions
	{
		FrameEditorWidgets m_FrameInWidgets;
		FrameEditorWidgets m_FrameOutWidgets;
		FrameEditorWidgets m_FrameOverrideWidgets;

		CDynamicEntity* m_pDynamicEntity;
		fwAnimDirectorComponent::ePhase m_Phase;

		atArray<float> m_Weights;

		sDebugExpressions(fwAnimDirectorComponent::ePhase phase)
			: m_pDynamicEntity(NULL)
			, m_Phase(phase)
			{
			}

		void AddExpressionDebugWidgets(CDynamicEntity& dynamicEntity, bkGroup& bk);
		void RemoveExpressionDebugWidgets();
		void UpdateExpressionDebugWidgets();
		void ResetExpressionDebugWidgets();

		bool LoadHashNames(const char* hashFilename);
	};

	//////////////////////////////////////////////////////////////////////////

	static void InitModelNameCombo();
#if USE_PM_STORE
	static void InitPmDictNameCombo();
#endif // USE_PM_STORE
	static void InitBoneAndMoverHistory();
	static void Init();
	static void Shutdown(unsigned shutdownMode);
	static void Process();
	static void DebugRender();
	static void OldDebugRender();
 
	static void OnDynamicEntityPreRender();

	static void AddDebugSkeleton(Color32 color, const char * debugText, bool bWhenPaused = false);
	static void AddDebugSkeleton(CDynamicEntity &dynamicEntity, Color32 color, bool bWhenPaused = false);
	static void AddDebugSkeleton(CDynamicEntity &dynamicEntity, Color32 color, const char * debugText, bool bWhenPaused = false);
	static void AddDebugSkeleton(CDynamicEntity &dynamicEntity, const crClip& anim, Matrix34& fixupTransform, float time, Color32 color);
	static void AddDebugSkeleton(const crSkeleton& skel, Color32 color, const char * debugText, bool bWhenPaused = false);

	static void UpdateDOFTrackingForEKG();
//private:
	static void InternalInitialise();
	static void InternalShutdown();

#if __BANK
	static void InitLevelWidgets();
	static void CreatePermanentWidgets(fwDebugBank* pBank);

	static void ActivateBank();
	static void DeactivateBank();

	static void ShutdownLevelWidgets();

	static void ReloadAllNetworks();

	static void ClearAllWidgetPointers();

#endif // __BANK
	static void LoadPreviewNetwork(); 
	static void GoToStartAnim();
	static void StepForwardAnim();
	static void PlayForwardsAnim();
	static void PauseAnim();
	static void PlayBackwardsAnim();
	static void StepBackwardAnim();
	static void GoToEndAnim();
	static void StopAnim(); 
	static void ReloadAnim(); 
	static void SetPhase(); 
	static void StartBlendAnim();
	static void SetBlendPhase(); 
	static void SetAnimBlendPhase(); 
	static void SetFrame(); 
	static void SetRate();
	static void UpdatePhase(); 
	static void RemoveNetwork(); 
	static void UpdateFrameRange(); 
	static crClip* GetSelectedClip();
	static float GetPhase();

	static void SetFacialIdleClip();
	static void PlayOneShotFacialClip();
	static void PlayOneShotFacialClipNoBlend();
	static void PlayVisemeClip();

	static void FindNearestAnimatedBuildingToPlayer();

	//static bool EnumerateAnim(crClip &clip, u32 hashCode, void* data);
	static bool EnumeratePm(const crpmParameterizedMotionData *pmData, u32, void* data);
	static void Reset();

	static void SelectPed();
	static void SelectMotionClipSet();
	static void SelectWeaponClipSet();
	static void ClearWeaponClipSet();
	static void SelectFacialClipSet();
	static void ToggleEnableFacial();
	static void SelectAnimDictionary();
	static void SelectAnim();
	static void SelectPmDictionary();
	static void SelectPm();

	static void InitBoneNames();
	static void SelectBone();

	static void InitSpringNames();
	static void SelectSpring();
	static void PopulateSpringWidgets();

#if ENABLE_BLENDSHAPES
	static void InitBlendshapeNames();
	static void SelectBlendShape();
#endif // ENABLE_BLENDSHAPES

	static void ToggleRenderPeds();
	static void ToggleBoneRotationOverride();
	static void ToggleBoneTranslationOverride();
	static void ToggleBoneScaleOverride();
	static void UpdateBoneLimits();
	static void ToggleLockPosition();
	static void ToggleUninterruptableTask();
	static void ToggleExtractZ();
	static void NextAnim();

	static void FocusEntityChanged(CEntity* pEntity);

	static void OnPressedLeftMouseButton();
	static void OnHoldingDownLeftMouseButton();
	static void OnReleasedLeftMouseButton();

	static void OnPressedRightMouseButton();
	static void OnHoldingDownRightMouseButton();
	static void OnReleasedRightMouseButton();
#if __BANK
	static void OnSelectingCutSceneEntity(); 
#endif // __BANK

	static void SetFocusEntityFromIndex();
	static void UpdateSelectedDynamicEntity();
	static void UpdateSelectedBone();

#if ENABLE_BLENDSHAPES
	static void PrintBlendshapeReport();
	static void LogBlendshapesAfterPose();
	static void LogBlendshapesAfterFinalize();
#endif // ENABLE_BLENDSHAPES
	static void LogShaderVarAfterPose();
	static void LogShaderVarAfterDraw();

	static void RenderGrid();
	static void RenderEntityInfo();

	static void SetRenderMoverTrackAtEntity();
	static void SetRenderMoverTrackAtTransform(const Matrix34& mtx);
	static void SetRenderMoverTrackAtFocusTransform();
	static void RenderMoverTrack(const crClip* pClip, const Matrix34& mtx);

#if __DEV	

	//point cloud rendering
	static void RenderPointCloud();
	static void DumpPosematcher();
	static void DumpNMBS();

#endif //__DEV

	static void StartAnimTask();
	static void StopAnimTask();
	static void StartObjectAdditiveAnim();
	static void StopObjectAdditiveAnim();

	static void AutoAddDebugSkeleton();
	static void AddDebugSkeleton();
	static void RenderSkeleton(const crSkeleton& skeleton, Color32 color, crFrameFilter* filter = NULL);
	static void RenderSkeletonUsingLocals(const crSkeleton& skeleton, Color32 color);

	// render to bone history array
	static inline void StartRecordingBoneHistory() {m_autoAddBoneHistoryEveryFrame = true;}
	static inline void StopRecordingBoneHistory() {m_autoAddBoneHistoryEveryFrame = false;}
	static void RenderBoneHistory();
	static void AddBoneHistoryEntry();
	static void ClearBoneHistory();
	static void OutputBoneHistory(); //outputs the bone history array to a csv file

	static void SpawnSelectedObject();	//Spawns an instance of an object currently selected in the object selector 
	static void SelectLastSpawnedObject(); // Select the last spawned object

	//////////////////////////////////////////////////////////////////////////
	//	MoVE network preview functionality
	//////////////////////////////////////////////////////////////////////////

	static void LoadNetworkButton();

	static void LoadNetworkFromFile(const char * filePath);

	static void RemoveCurrentPreviewNetwork();

	static void AddParamWidget();

	static void ClearParamWidgets();

	static void SaveParamWidgets();

	static void ForceSelectState();

	static void AddParamWidgetsFromEntity(CDynamicEntity* pDynamicEntity);
	static void RemoveParamWidgetsOnEntity(CDynamicEntity* pDynamicEntity);

	static void RequestParamWidgetOptionsRebuild();
	static void RebuildParamWidgetOptions();

#if __DEV

	static const int ms_NetworkPreviewLiveEdit_NetworkLength;
	static char      ms_NetworkPreviewLiveEdit_Network[];
	static const int ms_NetworkPreviewLiveEdit_NetworkClipSetVariableLength;
	static char      ms_NetworkPreviewLiveEdit_NetworkClipSetVariable[];
	static const int ms_NetworkPreviewLiveEdit_NetworkClipSetLength;
	static char      ms_NetworkPreviewLiveEdit_NetworkClipSet[];
	static const int ms_NetworkPreviewLiveEdit_ParamLength;
	static char      ms_NetworkPreviewLiveEdit_Param[];
	static float     ms_NetworkPreviewLiveEdit_Real;
	static bool      ms_NetworkPreviewLiveEdit_Bool;
	static bool      ms_NetworkPreviewLiveEdit_Flag;
	static const int ms_NetworkPreviewLiveEdit_RequestLength;
	static char      ms_NetworkPreviewLiveEdit_Request[];
	static bool      ms_NetworkPreviewLiveEdit_Event;
	static const int ms_NetworkPreviewLiveEdit_ClipFilenameLength;
	static char      ms_NetworkPreviewLiveEdit_ClipFilename[];
	static const int ms_NetworkPreviewLiveEdit_ClipSetVariableLength;
	static char      ms_NetworkPreviewLiveEdit_ClipSetVariable[];
	static const int ms_NetworkPreviewLiveEdit_ClipSetLength;
	static char      ms_NetworkPreviewLiveEdit_ClipSet[];
	static const int ms_NetworkPreviewLiveEdit_ClipDictionaryLength;
	static char      ms_NetworkPreviewLiveEdit_ClipDictionary[];
	static const int ms_NetworkPreviewLiveEdit_ClipLength;
	static char      ms_NetworkPreviewLiveEdit_Clip[];
	static const int ms_NetworkPreviewLiveEdit_ExpressionFilenameLength;
	static char      ms_NetworkPreviewLiveEdit_ExpressionFilename[];
	static const int ms_NetworkPreviewLiveEdit_ExpressionDictionaryLength;
	static char      ms_NetworkPreviewLiveEdit_ExpressionDictionary[];
	static const int ms_NetworkPreviewLiveEdit_ExpressionLength;
	static char      ms_NetworkPreviewLiveEdit_Expression[];
	static atMap< u32, const crExpressions * > ms_NetworkPreviewLiveEdit_LoadedExpressionMap;
	static const int ms_NetworkPreviewLiveEdit_FilterFilenameLength;
	static char      ms_NetworkPreviewLiveEdit_FilterFilename[];
	static const int ms_NetworkPreviewLiveEdit_FilterDictionaryLength;
	static char      ms_NetworkPreviewLiveEdit_FilterDictionary[];
	static const int ms_NetworkPreviewLiveEdit_FilterLength;
	static char      ms_NetworkPreviewLiveEdit_Filter[];
	static atMap< u32, crFrameFilter * > ms_NetworkPreviewLiveEdit_LoadedFilterMap;

	static void AddNetworkPreviewLiveEditWidgets(bkBank *pLiveEditBank);
	static void RemoveNetworkPreviewLiveEditWidgets(bkBank *pLiveEditBank);
	static void NetworkPreviewLiveEdit_LoadNetwork();
	static void NetworkPreviewLiveEdit_SetNetworkClipSetVariable();
	static void NetworkPreviewLiveEdit_SendReal();
	static void NetworkPreviewLiveEdit_SendBool();
	static void NetworkPreviewLiveEdit_SendFlag();
	static void NetworkPreviewLiveEdit_SendRequest();
	static void NetworkPreviewLiveEdit_SendEvent();
	static void NetworkPreviewLiveEdit_SendClip();
	static void NetworkPreviewLiveEdit_SendExpression();
	static void NetworkPreviewLiveEdit_SendFilter();

#endif // __DEV

	static void UpdateMoveClipSet();

	static char ms_clipSetVariableName[64];
	static bool ms_updateClipSetVariables;
	static void CreateClipSetVariable();
	static void DeleteClipSetVariable();
	static void UpdateClipSetVariables();

	static void CreateClipSetsFromGroups();

	static CTaskPreviewMoveNetwork* FindMovePreviewTask();

	static void ActivatePicker();
	static void EnableDebugRecorder();

	//////////////////////////////////////////////////////////////////////////
	//	Motion tree visualisation
	//////////////////////////////////////////////////////////////////////////

	static void VisualiseMotionTree();
	static void ToggleMotionTreeFull();
	static void VisualiseMotionTreeSimple();
	static void ToggleMotionTreeSimple();
	static void ClearPrimaryTaskOnSelectedPed();
	static void ClearSecondaryTaskOnSelectedPed();
	static void ClearMotionTaskOnSelectedPed();
	static void RunMoVETestTask();
	static void StopMoVETestTask();

	//////////////////////////////////////////////////////////////////////////
	// Clip file procedures
	//////////////////////////////////////////////////////////////////////////

	static void DumpClip();
	static void DumpAnimWRTSelectedEntity();
	static void DumpPose();
	static void DumpPosePrint(const char *szFormat, ...);
	static bool m_bDumpPose;
	static bool m_bDumpPosePrintEulers;
	static bool m_bDumpPosePrintOnlyDOFsInClip;
	static int m_DumpPosePrintIndent;
	static int m_DumpPoseFrameIndex;
	static int m_DumpPoseFrameCount;
	static fiStream *m_pDumpPoseFile;
	static CGenericClipHelper m_DumpPoseClipHelper;

	enum ClipProcess
	{
		CP_ADD_TRACK_IK_ROOT = 0,
		CP_ADD_TRACK_IK_HEAD,
		CP_ADD_TRACK_SPINE_WEIGHT,
		CP_ADD_TRACK_ROOT_HEIGHT,
		CP_ADD_TRACK_HIGH_HEEL,
		CP_ADD_TRACK_FIRST_PERSON_CAMERA_WEIGHT,
		CP_ADD_TAGS_LOOK_IK,
		CP_CHECK_CLIP_ANIM_DURATION,
		CP_CHECK_TAGS_RANGE,
		CP_CHECK_IK_ROOT,
		CP_CHECK_CONSTRAINT_HELPER,
		CP_RESET_IK_ROOT,
		CP_CHECK_ANIM_DATA_MOVER_ONLY,
		CP_CONVERT_FIRST_PERSON_DATA,
		CP_DELETE_TRACK_ARM_ROLL,
		CP_UPDATE_AUDIO_RELEVANT_WEIGHT_THRESHOLD,
		CP_CHECK_FOR_AUDIO_TAGS,
		CP_MAX
	};

	struct FindAndReplaceData
	{
		char propertyName[32];
		char attributeName[32];
		char findWhat[32];
		char replaceWith[32];
	};

	struct ConstraintHelperValidation
	{
		float m_fTolerance;
		bool m_bRight;
	};

	static void BatchProcessClipsOnDisk();
	static void BatchProcessClipsInMemory();

	static bool ProcessClipOnDisk(crClip* pClip, fiFindData fileData, const char * folderName);
	static void ProcessClipInImage(crClip* pClip, crClipDictionary* pDict, strLocalIndex dictIndex);

	static void CheckClipAnimDuration(rage::crClip* pClip, atString& clipName, const char* directory);
	static void CheckTagsRange(rage::crClip* pClip, atString& clipName, const char* directory);
	static void CheckIkRoot(crAnimation* pAnim, atString& clipName);
	static void CheckAnimDataMoverOnly(crAnimation* pAnim, atString& clipName);
	static void CheckConstraintHelper(crAnimation* pAnim, atString& clipName);
	static bool UpdateAudioRelevantWeightThreshold(crClip* pClip, atString& clipName);
	static void CheckForAudioTags(crClip* pClip, atString& clipName);

	static bool AddTrackIKRoot(crAnimation* pAnim, atString& clipName);
	static bool AddTrackIKHead(crAnimation* pAnim, atString& clipName);
	static bool AddTrackSpineStabilizationWeight(crAnimation* pAnim, atString& clipName);
	static bool AddTrackRootHeight(crClip* pClip, crAnimation* pAnim, atString& clipName);
	static bool AddTrackHighHeelControl(crAnimation* pAnim, atString& clipName);
	static bool AddTrackFirstPersonCameraWeight(crAnimation *pAnim, atString &clipName);

	static bool AddTagsLookIK(crClip* pClip, atString& clipName);

	static bool ResetIkRoot(crAnimation* pAnim, atString& clipName);
	static bool DeleteTrackArmRoll(crAnimation* pAnim, atString& clipName);

	static bool ConvertFirstPersonData(crClip* pClip, crAnimation* pAnim, atString& clipName);

	static char m_BatchProcessInputFolder[RAGE_MAX_PATH];
	static char m_BatchProcessOutputFolder[RAGE_MAX_PATH];
	static bool m_abClipProcessEnabled[CP_MAX];
	static bool m_bDefaultHighHeelControlToOne;
	static bool m_bVerbose;

	static void BatchProcessFindAndReplace();
	static bool ProcessFindAndReplace(crClip* pClip, fiFindData fileData, const char* folderName);

	static FindAndReplaceData m_FindAndReplaceData;
	static ConstraintHelperValidation m_ConstraintHelperValidation;

	//////////////////////////////////////////////////////////////////////////
	// Entity selection
	//////////////////////////////////////////////////////////////////////////

	static void SetSelectedEntity(CDynamicEntity* pEntity);

	// Anim Sets debugging

#if __DEV
	static void DumpClipSetsCSV();
	static void DumpFiltersCSV();
	static void LoadFilters();
	static void SaveFilters();
#endif // __DEV


	// Expression debugging

	static void AddExpressionDebugWidgets();
	static void RemoveExpressionDebugWidgets();
	static void UpdateExpressionDebugWidgets();
	static void ResetExpressionDebugWidgets();

public:

	// UI information
	static Vector2 m_vScale;
	static int m_iNoOfTexts;			static float m_fVertDist;
	static float m_fHorizontalBorder;
	static float m_fVerticalBorder;

	// Pointer to the UI banks
	static fwDebugBank* m_pBank;

	// Pointer to the bone name combo
	static const int NUM_BONE_COMBOS = 4;
	static bkCombo *m_pBoneCombo[NUM_BONE_COMBOS];

	// Pointer to the spring name combo
	static bkCombo *m_pSpringBoneCombo;
	static bkGroup *m_pLinearPhysicalExpressionGroup;

	// Struct to hold pointers to spring dof widgets
	struct SpringDof
	{
		bkTitle* m_pLabel;
		bkVector3* m_pStrength;
		bkVector3* m_pDamping;
		bkSeparator* m_pSeparator;
		bkVector3* m_pMinConstraint;
		bkVector3* m_pMaxConstraint;
	};

	static SpringDof m_linearSpringDofs;
	static SpringDof m_angularSpringDofs;
	static bkVector3* m_pSpringGravity;
	static bkVector3* m_pSpringDirection;

#if ENABLE_BLENDSHAPES
	// Pointer to the blendshape combo
	static bkCombo *m_pBlendshapeCombo;
	static bkBank* m_pBlendshapeBank;
#endif // ENABLE_BLENDSHAPES
	static int  ms_iSelectedMemoryGroup;
	static bool m_filterByMemoryGroup;

	// Pointer to a character
	static RegdDyn m_pDynamicEntity;

	// output angles in degrees if true otherwise  output angles in radians
	static bool m_degrees;

	// output rotations as euler angles if true otherwise output rotations as a 3x3 matrix
	static bool m_eulers;

	// Rendering options
	static bool m_drawAllBonesMat;
	static float m_drawAllBonesMatSize;
	static bool m_drawSelectedEntityMat;
	static float m_drawSelectedEntityMatSize;
	static bool m_drawSelectedEntityMatTranslation;
	static bool m_drawSelectedEntityMatRotation;
	static bool m_drawSelectedEntityMatTransAndRotWrtCutscene;
	static bool m_drawSelectedEntityMoverTranslation;
	static bool m_drawSelectedEntityMoverRotation;
	static bool m_drawSelectedEntityHair;
	static bool m_drawSelectedEntityHairMeasure;
	static float m_drawSelectedEntityHairMeasureHeight;
	static bool m_drawSelectedBoneMat;
	static bool m_drawSelectedParentBoneMat;
	static bool m_drawVehicleComponentBoundingBox;
	static bool m_drawSelectedBoneMatTranslation;
	static bool m_drawSelectedBoneMatRotation;
	static bool m_drawSelectedBoneMatTransAndRotWrtCutscene;
	static bool m_drawSelectedBoneMatScale;
	static float m_drawSelectedBoneMatSize;
	static bool m_drawSelectedBoneJointLimits;
	static bool m_drawFirstPersonCamera;
	static bool m_drawConstraintHelpers;
	static bool m_drawMotionBlurData;

	//mover track rendering
	static bool m_drawMoverTrack;
	static float m_drawMoverTrackSize;
	static bool m_renderMoverTrackAtEntity;
	static Matrix34 m_moverTrackTransform;

	static bool m_renderTagsForSelectedClip;
	static bool m_renderTagsVisuallyForSelectedClip;
	static float m_renderTagsVisuallyForSelectedClipOffset;

	static bool m_drawDebugSkeletonArray;
	static bool m_drawDebugSkeletonsUsingLocals;


	static bool m_drawSelectedAnimSkeleton;
	static float m_drawSelectedAnimSkeletonPhase; 
	static float m_drawSelectedAnimSkeletonOffset;

	static bool  m_drawSelectedAnimSkeletonDOFTest;
	static u8	 m_drawSelectedAnimSkeletonDOFTestTrack;
	static u16	 m_drawSelectedAnimSkeletonDOFTestId;
	static u8	 m_drawSelectedAnimSkeletonDOFTestType;

	// array of model names 
	static atArray<const char*> m_modelNames;
	
	// array of model names (as string)
	static atVector<atString> m_modelStrings;

	// the current model index
	static int m_modelIndex;

	// the current motion clip set index
	static int m_motionClipSetIndex;

	// the current motion clip set index
	static int m_weaponClipSetIndex;

	// the current facial clip set index
	static int m_facialClipSetIndex;
	static fwClipSetRequestHelper m_facialClipSetHelper;
	static bool m_bEnableFacial;

	// the current gesture clip set index
	static int m_gestureClipSetIndex;

	// array of pm dictionary
	static atVector<CAnimViewer::sPmDictionary> m_pmDictionary;

	// array of pm names 
	static atArray<const char*> m_pmDictionaryNames;

	// array of bone names 
	static atArray<const char*> m_boneNames;

#if ENABLE_BLENDSHAPES
	// array of blendshape names 
	static atArray<const char*> m_blendShapeNames;
#endif // ENABLE_BLENDSHAPES

	// array of spring names
	static atArray<const char*> m_springNames;

#if ENABLE_BLENDSHAPES
	// array of blend shape blend values
	static atArray<float> m_blendShapeBlends;
#endif // ENABLE_BLENDSHAPES

	// array of bone history structures
	static atArray<sBoneHistory> m_boneHistoryArray;

	// the current matix index
	static int m_matIndex;

	// the current pm dictionary index
	static int m_pmDictionaryIndex;

	// the current animation index
	static int m_pmIndex;

	// true if in animation viewer mode
	static bool m_active;

	static Vector3 m_componentRoot; 

	static bool m_advancedMode;
	static bool m_unInterruptableTask;
	
	static bool m_bLockPosition;
	static bool m_bLockRotation;
	static bool m_bResetMatrix;
	static bool m_extractZ;

	static eScriptedAnimFlagsBitSet ms_AnimFlags;
	static eIkControlFlagsBitSet ms_IkControlFlags;

	static CDebugFilterSelector ms_taskFilterSelector;

	static float ms_fPreviewAnimTaskBlendIn;
	static float ms_fPreviewAnimTaskBlendOut;
	static float ms_fPreviewAnimTaskStartPhase;

	static bool ms_bPreviewAnimTaskAdvanced;
	static Vector3 ms_vPreviewAnimTaskAdvancedPosition;
	static Vector3 ms_vPreviewAnimTaskAdvancedOrientation;

	static bool ms_bPreviewNetworkAdditive;
	static bool ms_bPreviewNetworkDisableIkSolvers;

	//
	// Debug rendering options
	//
	static bool ms_bRenderEntityInfo;
	static bool ms_bRenderSkelInfo;

#if __DEV

	//point cloud rendering settings
	static bool m_renderPointCloud;
	static float m_pointCloudBoneSize;
	static bool m_renderPointCloudAtSkelParent;
	static bool m_renderLastMatchedPose;

#endif //__DEV

	// true if we want to render the skeleton
	static bool m_renderSkeleton;

#if FPS_MODE_SUPPORTED
	static bool m_renderThirdPersonSkeleton;
#endif // FPS_MODE_SUPPORTED

	// true if we want to render the skin
	static bool m_renderSkin;

	static s32 m_entityScriptIndex;

	//
	// Bone information
	//

	// the index of the bone we are currently interested in
	static s32 m_boneIndex;

	// the index of the spring we are currently interested in
	static s32 m_springIndex;

	// bone override settings
	static bool ms_bApplyOverrideRelatively;	// If true, the override will be added rather than overwriting the existing local transform
	static bool ms_bOverrideBoneRotation;		// If true, the selected bones rotation will be overriden
	static bool ms_bOverrideBoneTranslation;	// If true, the selected bones translation will be overriden
	static bool ms_bOverrideBoneScale;			// If true, the selected bones scale will be overriden
	static float ms_fOverrideBoneBlend;			//

	// the limits of the bone
	static Vector3 m_rotation; 
	static Vector3 m_translation;
	static Vector3 m_scale;

	//
	// Bone matrix history rendering
	//
	static bool m_renderBoneHistory;
	static bool m_autoAddBoneHistoryEveryFrame;

#if ENABLE_BLENDSHAPES
	//
	// Blendshape information
	//

	// the index of the blendshape we are currently interested in
	static int m_blendShapeIndex;
	
	// the blend of the selected blendshape 0.0 to 1.0
	static float m_blendShapeBlend;

	// override blendshape toggle
	static bool  m_overrideBlendShape;
	static bool  m_displayBlendShape;
#endif // ENABLE_BLENDSHAPES

	//
	// Debug skeleton array
	//

	static atArray<sDebugSkeleton> m_debugSkeletons;
	static s32 m_skeletonIndex;
	static bool m_autoAddSkeletonEveryFrame;
	static bool m_renderDebugSkeletonNames;
	static bool m_renderDebugSkeletonParentMatrices;

	static Vector3 m_startPosition;

	//anim selector tool
	static CDebugClipSelector m_animSelector;
	static CDebugClipSelector m_blendAnimSelector;

	static CDebugClipSelector m_additiveBaseSelector;
	static float m_fAdditiveBaseRate;
	static float m_fAdditiveBasePhase;
	static bool m_bAdditiveBaseLooping;
	static CDebugClipSelector m_additiveOverlaySelector;
	static float m_fAdditiveOverlayRate;
	static float m_fAdditiveOverlayPhase;
	static bool m_bAdditiveOverlayLooping;

	//preview camera
	static bool ms_bUsePreviewCamera;
	static bool ms_bUsingPreviewCamera;
	static int ms_iPreviewCameraIndex;
	static int ms_iPreviewCameraCount;
	static const char *ms_szPreviewCameraNames[];

	//object selector tool, used to select an object to spawn using RAG
	static CDebugObjectSelector m_objectSelector;

	static RegdEnt m_spawnedObject;

	static u32 m_boneWeightSignature;

	//////////////////////////////////////////////////////////////////////////
	//	MoVE network previewing members
	//////////////////////////////////////////////////////////////////////////

	// file selection widget to allow loading of files from a directory
	static CDebugFileSelector m_moveFileSelector;
	//static int m_moveNetworkPrecedence; 
	//static const char * m_precedenceNames[CMovePed::kNumPrecedence];
	static int m_moveNetworkClipSet;

	// parameter widget options
	static bool m_paramWidgetUpdateEveryFrame;
	static float m_paramWidgetMinValue;
	static float m_paramWidgetMaxValue;
	static bool m_paramWidgetOutputMode;
	static int m_paramWidgetAnalogueControlElement;
	static int m_paramWidgetControlElement; 

	static const char * m_analogueControlElementNames[7];
	static const char * m_controlElementNames[7]; 

#if CRMT_NODE_2_DEF_MAP
	static bool m_bEnableNetworksDebug;
	static char m_networkDebugNetworkFilter[256];
	static bkText* m_pNetworkNodePropertyData;
	static const int m_networkDebugDataLength;
	static char m_networkDebugData[];
	static void DumpNetworkNodePropertyData();
	static void UpdateNetworkNodePropertyData();
#endif //CRMT_NODE_2_DEF_MAP

	static bkGroup* m_previewNetworkGroup;
	static bkGroup* m_clipSetVariableGroup;
	static bkGroup* m_widgetOptionsGroup;
	static bkGroup* m_secondaryMotionTuningGroup;
	//static atArray<crParameterWidget*> m_paramWidgets; // stores the widgets we want to edit in RAG
	static int m_newParamType;		
	static char m_newParamName[64];				// Stores the param name for new parameter widgets
	static char m_stateMachine[64];
	static char m_forceToState[64];

	static bool ms_bCreateSecondary;		// if true, the move preview network will be run as a secondary task

	//////////////////////////////////////////////////////////////////////////
	//	Motion tree visualisation
	//////////////////////////////////////////////////////////////////////////
	static bool m_bVisualiseMotionTree;
	static bool m_bVisualiseMotionTreeSimple;
	static fwAnimDirectorComponent::ePhase m_eVisualiseMotionTreePhase;
	static bool m_bDumpMotionTreeSimple;
	static bool m_bVisualiseMotionTreeNewStyle;
	static bool m_bCaptureMotionTree;
	static bool m_bCapturingMotionTree;
	static bool m_bCaptureDofs;
	static int m_iCaptureFilterNameIndex;
	static int m_iCaptureFilterNameCount;
	static const char **m_szCaptureFilterNames;
	static bool m_bRelativeToParent;
	static bool m_bApplyCapturedDofs;
	static u32 m_uNextApplyCaptureDofsPreRenderFrame;
	static bool m_bTrackLargestMotionTree;
	static u32 m_uRenderMotionTreeCaptureFrame;
	static char m_szMotionTreeCapture[];
	static u32 m_uMotionTreeCaptureLength;
	static fwMoveDumpFrameSequence *m_MoveDump;
	static void LoadMotionTreeCapture();
	static void SaveMotionTreeCapture();
	static void PrintMotionTreeCapture();
	static void PrintAllMotionTreeCapture();

#if CALLSTACK_ON_BLOCKING_ANIM_PREPHYSICS_UPDATE
	static void PotentialMotionTreeBlockCallstackCollector();
#endif //CALLSTACK_ON_BLOCKING_ANIM_PREPHYSICS_UPDATE

	//////////////////////////////////////////////////////////////////////////
	// Entity capture
	//////////////////////////////////////////////////////////////////////////
	static bkGroup *m_pEntitiesToCaptureGroup;
	static atVector< CEntityCapture * > m_EntityCaptureArray;
	static bool m_EntityCaptureLock30Fps;
	static bool m_EntityCaptureLock60Fps;
	static void EntityCaptureAddEntity();
	static void EntityCaptureUpdate();
	static void EntityCaptureToggleLock30Fps();
	static void EntityCaptureToggleLock60Fps();
	static void EntityCaptureStart();
	static void EntityCaptureRenderUpdate();
	static void EntityCaptureStop();
	static void EntityCaptureSingleFrame();

	//////////////////////////////////////////////////////////////////////////
	// Camera capture
	//////////////////////////////////////////////////////////////////////////
	static int m_iCameraCapture_CameraIndex;
	static char m_szCameraCapture_AnimationName[];
	static u32 m_uCameraCapture_AnimationNameLength;
	static atArray< const crFrame * > m_CameraCapture_FrameArray;
	static Matrix34 m_CameraCapture_InitialMover;
	static int m_iCameraCapture_CameraNameIndex;
	static int m_iCameraCapture_CameraNameCount;
	static const char **m_szCameraCapture_CameraNames;
	static bool m_bCameraCapture_Started;
	static void CameraCapture_CameraNameIndexChanged();
	static void CameraCapture_Start(int cameraIndex, const char *animName);
	static void CameraCapture_Start();
	static void CameraCapture_Update();
	static void CameraCapture_Stop();
	static void CameraCapture_SingleFrame();

	//////////////////////////////////////////////////////////////////////////
	// Blend-out capture
	//////////////////////////////////////////////////////////////////////////
	struct BlendOutData
	{
		BlendOutData();
		void Init(const CDynamicEntity& entity, const CNmBlendOutSetList& blendOutSetList, eNmBlendOutSet matchedSet, CNmBlendOutItem* matchedItem, const char* preTitle = NULL);
		void AddWidgets(bkGroup* group);
		void OnMatchChanged();
		~BlendOutData();

		crSkeletonData* m_SkeletonData;
		Mat34V m_RagdollTransform;
		Mat34V m_SelectedTransform;
		Mat34V m_MatchedTransform;

		ConstString m_Title;

		crFrame* m_RagdollFrame;
		crFrame* m_SelectedFrame;
		crFrame* m_MatchedFrame;
		crFrameFilter* m_LowLODFrameFilter;
		CNmBlendOutItem* m_MatchedItem;
		fwPoseMatcher m_PoseMatcher;
		bkGroup* m_BlendOutGroup;

		eNmBlendOutSet m_MatchedSet;
		fwMvClipSetId m_MatchedClipSet;
		fwMvClipId m_MatchedClip;

		CNmBlendOutSetList m_BlendOutSetList;

		s16 m_SelectedBlendOutSet;
		s16 m_SelectedBlendOut;
		s16 m_BlendOutSet;
		bool m_DrawSelectedBlendOutSkeleton;
		bool m_DrawMatchedBlendOutSkeleton;
		bool m_DrawRagdollFrameSkeleton;
		bool m_UseLowLODFilter;
	};

	static bool	m_bForceBlendoutSet;
	static eNmBlendOutSet m_ForcedBlendOutSet;
	static atSList<BlendOutData> m_BlendOutData;
	static bkGroup* m_BlendOutDataGroup;
	static bool m_BlendOutDataEnabled;
	static char m_BlendOutAnimationName[];
	static Vector3 m_BlendOutOrientationOffset;
	static Vector3 m_BlendOutTranslationOffset;
	static BlendOutData* CreateNewBlendOutData();
	static void BlendOutCapture(const CDynamicEntity* entity, const CNmBlendOutSetList& blendOutSetList, const eNmBlendOutSet& matchedSet, CNmBlendOutItem* matchedItem, const char* preTitle = NULL);
	static void OnSelectedBlendOutSetListChanged(BlendOutData* blendOutData);
	static void OnSelectedBlendOutSetChanged(BlendOutData* blendOutData);
	static void OnSelectedBlendOutChanged(BlendOutData* blendOutData);
	static void OnRemoveBlendOutSet(BlendOutData* blendOutData);
	static void OnAddBlendOutSet(BlendOutData* blendOutData);
	static void OnRecalculateMatch(BlendOutData* blendOutData);
	static void OnLoadRagdollPose(BlendOutData* blendOutData);
	static void OnSaveRagdollPose(BlendOutData* blendOutData);
	static void OnPoseSelectedPedUsingCurrentlyDrawnSkelecton(BlendOutData* blendOutData);
	static void OnSetSelectedPedPositionUsingCurrentlyDrawnSkeleton(BlendOutData* blendOutData);
	static void OnForceSelectedPedIntoGetUpTask();
	static void ReactivateSelectedPed();
	static void ClearBlendOutData();

	// Expression debugging
	static sDebugExpressions m_debugPreRenderExpressions;
	static sDebugExpressions m_debugMidPhysicsExpressions;
	static bkGroup* m_pExpressionWidgetGroup;
	static bkGroup* m_pExpressionParentGroup;

	//////////////////////////////////////////////////////////////////////////
	//	Debug info rendering values
	//////////////////////////////////////////////////////////////////////////
	static int m_horizontalScroll;
	static int m_verticalScroll;
	static int m_dictionaryScroll;

	//////////////////////////////////////////////////////////////////////////
	// Synchronized scene debugging
	//////////////////////////////////////////////////////////////////////////
	static bool m_renderSynchronizedSceneDebug;
	static int m_renderSynchronizedSceneDebugHorizontalScroll;
	static int m_renderSynchronizedSceneDebugVerticalScroll;

	//////////////////////////////////////////////////////////////////////////
	// Facial variation debugging
	//////////////////////////////////////////////////////////////////////////
	static bool m_renderFacialVariationDebug;
	static int m_renderFacialVariationTrackerCount;

	//////////////////////////////////////////////////////////////////////////
	// FrameFilter Dictionary debugging
	//////////////////////////////////////////////////////////////////////////
	static bool m_renderFrameFilterDictionaryDebug;

	//////////////////////////////////////////////////////////////////////////
	// Paired anim debugging
	//////////////////////////////////////////////////////////////////////////
	static void PositionPedRelativeToVehicleSeatCB();
	static void PositionPedsCB();
	static void ResetPedsCB();
	static void AddAnimToFirstPedsListCB();
	static void AddAnimToSecondPedsListCB();
	static void ClearFirstPedsAnimsCB();
	static void ClearSecondPedsAnimsCB();
	static void PlayAnimsCB();

	static s32	 ms_iSeat;
	static bool	 ms_bComputeOffsetFromAnim;
	static float ms_fExtraZOffsetForBothPeds;
	static float ms_fXRelativeOffset;
	static float ms_fYRelativeOffset;
	static float ms_fZRelativeOffset;
	static float ms_fRelativeHeading;
	static atArray<CDebugClipDictionary*>	ms_aFirstPedDicts;
	static atArray<CDebugClipDictionary*>	ms_aSecondPedDicts;
	static atArray<const char*>				ms_szFirstPedsAnimNames;
	static atArray<const char*>				ms_szSecondPedsAnimNames;

	//////////////////////////////////////////////////////////////////////////
	// Hash translator
	//////////////////////////////////////////////////////////////////////////

	static void UpdateValueFromString();
	static void UpdateStringFromValue();
	static atHashString ms_hashString;
	static char ms_hashValue[16];

	//////////////////////////////////////////////////////////////////////////
	// Gesture tester
	//////////////////////////////////////////////////////////////////////////

	static void BlendInGesture();
	static void BlendOutGestures();
	static void SelectGestureClipSet();
	static audGestureData ms_gestureData;
	static atHashString ms_gestureClipNameHash;

	//////////////////////////////////////////////////////////////////////////
	// Camera curve viewer
	//////////////////////////////////////////////////////////////////////////
	static void RenderCameraCurve();
	static Vec2V ConvertPointOnCurveToScreenSpace(Vec2V point);
	static float ApplyCustomBlendCurve(float sourceLevel, s32 curveType, bool shouldBlendIn);
	static bool ms_bRenderCameraCurve;
	static float ms_fRenderCameraCurveP1X;
	static float ms_fRenderCameraCurveP1Y;
	static float ms_fRenderCameraCurveP2X;
	static float ms_fRenderCameraCurveP2Y;
	static float ms_fRenderCameraCurveScale;
	static float ms_fRenderCameraCurveOffsetX;
	static float ms_fRenderCameraCurveOffsetY;
	static int ms_iSelectedCurveType;
	static bool ms_bShouldBlendIn;
	//////////////////////////////////////////////////////////////////////////
	// Dof capturing ekg options
	//////////////////////////////////////////////////////////////////////////
	static const u32 MAX_DOFS_TO_TRACK = 3;
	static s32 ms_DofTrackerBoneIndices[MAX_DOFS_TO_TRACK];
	static bool ms_DofTrackerUpdatePostPhysics;

	//////////////////////////////////////////////////////////////////////////
	//	Old CAnimDebug functionality
	//////////////////////////////////////////////////////////////////////////

	static const int MAX_LINES_IN_TABLE = 30;

	struct sAnimDictSummary
	{
		const char* pAnimDictName;
//		fwMvClipSetId clipSetId;
		s32 animDictSize;
		s32 animCount;		// number of clips (-1 if not loaded)
		s32 refCount;		// number of references
		bool isResident;	// resident or streaming (in the anim groups)
		bool isInImage;		// present in the anim image or missing
		bool hasLoaded;		// loaded in memory or unloaded
		atHashString memoryGroup;
	};

	static void SceneUpdateCountAnimsCb(fwEntity &entity, void *userData);
	static void PrintMemoryUsageReport();
	static void PrintClipDictionaryContents();
	static void RenderSkeleton(const crSkeleton& skeleton, Color32 boneColor, Color32 jointColor, float jointRadius);
	static void LogAllAnims();
	static void LogAllExpressions();

	static void PrintClipDictionariesWithMemoryGroup();
	static void PrintClipDictionariesReferencedByAnyCipSet();

private:

	static void AddDebugGroupWidgets();
	static void ScrollUp();
	static void ScrollDown();

	static void IsCode();
	static void IsScript();

	static void IsResisdent();
	static void IsStreaming();

	static void IsNotLoaded();
	static void IsLoaded();

	static void ToggleSortByName();
	static void ToggleSortBySize();



	static void ReadHashKeyFile();

	static void UpdateAnimatedBuilding();
	static float ms_AnimatedBuildingRate;
	static void AnimatedBuildingAnimRateChanged();
	static void RandomAnimatedBuildingAnimRate();
	static float ms_AnimatedBuildingPhase;
	static void AnimatedBuildingAnimPhaseChanged();
	static void RandomAnimatedBuildingAnimPhase();

	static void UpdatePreviewCamera();

private:

	// UI information

	static bool m_showAnimMemory;
	static bool m_showAnimDicts;

	static bool m_showPercentageOfMGColumn;
	static bool m_showPercentageOfMGBudgetColumn;
	static bool m_showClipCountColumn;
	static bool m_showLoadedColumn;
	static bool m_showPolicyColumn;
	static bool m_showReferenceCountColumn;
	static bool m_showMemoryGroupColumn;
	static bool m_showCompressionColumn;
	static bool m_isLoaded;
	static bool m_isNotLoaded;
	static bool m_isResident;
	static bool m_isStreaming;
	static bool m_haveReferences;
	static bool m_haveNoReferences;
	
	static bool m_sortByName;
	static bool m_sortBySize;

	static bool m_showDictPyhsicalMemory;
	static bool m_showDictVirtualMemory;
	static bool m_showDictStreamingPolicy;
	static bool m_showDictOrigin;
	static bool m_showDictIsLoaded;

	static bool m_showAnimLODInfo;
	static bool m_showAnimPoolInfo;

	static int m_startLine;
	static int m_endLine;

	static int m_selectedCount;

	static bool m_bLogAnimDictIndexAndHashKeys;

	static CMoveNetworkHelper m_MoveNetworkHelper;

	static bool m_bNeedsParamWidgetOptionsRebuild;

	static float m_OldPhase;
	static float m_Phase;
	static float m_Frame;
	static float m_Rate;
	static float m_OldAnimBlendPhase;
	static float m_AnimBlendPhase;
	static float m_BlendPhase;

	static bkSlider* m_pFrameSlider; 
	
	static atHashString m_AnimHash; 
	static atHashString m_BlendAnimHash;
	
	// array of anim groups
	static atVector<sAnimDictSummary> m_selectedAnimDictSummary;

	static const fwMvBooleanId ms_LoopedId;
	static const fwMvClipId ms_ClipId;
	static const fwMvClipId ms_BlendClipId;	
	static const fwMvFloatId ms_PhaseId;
	static const fwMvFloatId ms_BlendPhaseId;
	static const fwMvFloatId ms_BlendAnimPhaseId;
	static const fwMvFloatId ms_PhaseOutId;
	static const fwMvFloatId ms_RateId;
	static const fwMvFloatId ms_RateOutId;
};

#endif // __BANK

#endif // ANIM_VIEWER_H
