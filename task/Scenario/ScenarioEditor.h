#ifndef _SCENARIO_SCENARIOEDITOR_H_
#define _SCENARIO_SCENARIOEDITOR_H_

// Rage Includes
#include "atl/array.h"
#include "data/base.h"
#include "parser/macros.h"

// Game Includes
#include "scene/loader/MapData_Extensions.h"
#include "task/Scenario/ScenarioAnimDebugHelper.h"
#include "task/Scenario/ScenarioChaining.h"		// For CScenarioChainingNode in CScenarioPointUndoItem.
#include "task/Scenario/ScenarioPointGroup.h"
#include "templates/CircularArray.h"
#include "tools/EditorWidget.h"

#if SCENARIO_DEBUG

namespace rage {
	class bkCombo;
	class bkGroup;
	class bkButton;
	class bkSlider;
}
class CScenarioInfo;
class CScenarioPointRegion;
class CScenarioPointRegionEditInterface;

class CModelSetNameData
{
public:
	explicit CModelSetNameData(int modelSetType);

	void Init(const char ** fixedStrings);

	const char ** GetNames() 						{ return m_Names.GetElements(); }
	int GetNumNames() const							{ return m_Names.GetCount(); }

	u32 GetHashForIndex(const int index) const		{ return atStringHash(m_Names[index]); }
	int GetIndexFromHash(const u32 hash) const;

protected:
	int						m_ModelSetType;			// Enum value from CAmbientModelSetManager::ModelSetType.

	u32						m_numFixedStrings;		// number of strings at front of list that are not ped names (--Unused-- , --All-- and the like)
	u32						m_numSetsSkipped;

	atArray<const char *>	m_Names;
	atArray<int>			m_SortOdered;
};

//-----------------------------------------------------------------------------

// A struct to hold the data for a point
class EditorPoint
{
public:

	static const u32 kMaskIndex						= 0x3fffffff;
	static const u32 kMaskType						= 0xc0000000;
	static const u32 kPointIdInvalid				= 0xffffffff;
	static const u32 kShiftEntityPointOverrideIndex	= 16;
	static const u32 kShiftClusterIndex				= 16;
	static const u32 kShiftType						= 30;

	enum Type
	{
		kTypeWorldPoint,
		kTypeEntityPoint,
		kTypeEntityPointOverride,
		kTypeClusteredWorldPoint
	};


	EditorPoint()								{ m_PointId = kPointIdInvalid; }

	void	Reset()								{ m_PointId = kPointIdInvalid; }

	bool	operator==(const EditorPoint &rhs) const { return (m_PointId == rhs.m_PointId); }

	u32		GetPointId() const					{ return m_PointId; }
	void	SetPointId(u32 id)					{ m_PointId = id; }

	Type	GetType() const						{ return GetTypeFromId(m_PointId); }

	bool	IsEditable() const;
	bool	IsValid() const						{ return m_PointId != kPointIdInvalid; }
	bool	IsEntityPoint() const				{ Assert(IsValid()); return GetType() == kTypeEntityPoint; }
	bool	IsEntityOverridePoint() const		{ Assert(IsValid()); return GetType() == kTypeEntityPointOverride; }
	bool	IsWorldPoint() const				{ Assert(IsValid()); return GetType() == kTypeWorldPoint; }
	bool	IsClusteredWorldPoint() const		{ Assert(IsValid()); return GetType() == kTypeClusteredWorldPoint; }

	int		GetEntityPointIndex() const			{ Assert(IsEntityPoint()); return (int)(m_PointId & kMaskIndex);	}
	int		GetWorldPointIndex() const			{ Assert(IsWorldPoint()); return (int)(m_PointId & kMaskIndex);	}

	int		GetOverrideIndex() const			{ return GetOverrideIndex(m_PointId); }
	int		GetIndexWithinOverride() const		{ return GetIndexWithinOverride(m_PointId); }

	void	SetWorldPoint(int idx)				{ m_PointId = ((u32)idx) | (kTypeWorldPoint << kShiftType); }
	void	SetEntityPoint(int idx)				{ m_PointId = ((u32)idx) | (kTypeEntityPoint << kShiftType); }
	void	SetEntityPointForOverride(int overrideIndex, int pointIndexInOverride);
	void	SetClusteredWorldPoint(int clusterIdx, int pointIdx);

	CScenarioPoint& GetScenarioPoint(CScenarioPointRegion* pRegion) const;
	const CScenarioPoint& GetScenarioPoint(const CScenarioPointRegion* pRegion) const;
	CScenarioPoint* GetScenarioPointPtr(CScenarioPointRegion* pRegion) const { return GetScenarioPointFromId(m_PointId, pRegion); }

	static Type GetTypeFromId(u32 id)			{ return (Type)((id & kMaskType) >> kShiftType); }

	static int GetOverrideIndex(u32 id)			{ Assert(GetTypeFromId(id) == kTypeEntityPointOverride); return (id & kMaskIndex) >> kShiftEntityPointOverrideIndex; }
	static int GetIndexWithinOverride(u32 id)	{ Assert(GetTypeFromId(id) == kTypeEntityPointOverride); return id & ((1 << kShiftEntityPointOverrideIndex) - 1); }
	static int GetClusterIndex(u32 id)			{ Assert(GetTypeFromId(id) == kTypeClusteredWorldPoint); return (id & kMaskIndex) >> kShiftClusterIndex; }
	static int GetClusterPointIndex(u32 id)		{ Assert(GetTypeFromId(id) == kTypeClusteredWorldPoint); return id & ((1 << kShiftClusterIndex) - 1); }
	static int GetWorldPointIndexFromId(u32 id)	{ Assert(GetTypeFromId(id) == kTypeWorldPoint); return (int)(id & kMaskIndex); }

	static CScenarioPoint* GetScenarioPointFromId(u32 pointId, CScenarioPointRegion* pRegion);

protected:
	u32		m_PointId;
};

//-----------------------------------------------------------------------------

//PERHAPS THIS SHOULD GO IN ITS OWN .h/.cpp?
// Also perhaps use Vec3V?
class MousePosition
{
public:
	MousePosition();

	void Update();

	bool IsValid() const;
		
	Vector3 GetDirection() const;
	Vector3 GetPosScreen() const;
	Vector3 GetPosFar() const;

	float DistToScreen(Vector3::Vector3Param point) const;
	float Dist2ToScreen(Vector3::Vector3Param point) const;
	float DistToFar(Vector3::Vector3Param point) const;
	float Dist2ToFar(Vector3::Vector3Param point) const;

private:
	Vector3 m_MouseScreen;
	Vector3 m_MouseFar;
	bool m_Valid;
};

//-----------------------------------------------------------------------------
class ScenarioEditorSelection
{
public:
	bool WantMultiSelect() const;
	bool IsSelected(EditorPoint& checkPoint) const;
	bool IsMultiSelecting() const;
	bool HasSelection() const;

	void Select(EditorPoint& checkPoint);
	void Deselect(EditorPoint& checkPoint);
	void DeselectAll();

	typedef void (*SelPntActionFunction) (EditorPoint& editPoint, void* userdata);
	void ForAll(SelPntActionFunction func, void* userdata);

	EditorPoint FindEditorPointForPoint(const CScenarioPoint& toFind, const CScenarioPointRegion& checkRegion) const;
	EditorPoint GetOnlyPoint() const; //used to perform operations on things that are not multi selected.

	//Multi selection manipulation could remove a point from an array that would leave other selected
	// points invalid that could cause assets or crashes
	void UpdateSelectionForRemovedPoint(const int removedPID);
	void UpdateSelectionForRemovedClusterPoint(const int removedCID, const int removedCPID);

private:

	atArray<EditorPoint> m_Selection;
};

//-----------------------------------------------------------------------------

class CScenarioEditor : public rage::datBase
{
protected:
	class CScenarioPointUndoItem;

	enum eUndoAction {
		SPU_ModifyPoint,
		SPU_DeletePoint,
		SPU_InsertPoint,
		SPU_AttachPoint,
		SPU_DetachPoint,
		SPU_DisableSpawningOnEntity,
		SPU_RemoveOverridesOnEntity,
		SPU_ModifyGroup,
		SPU_DeleteGroup,
		SPU_InsertGroup,
		SPU_ModifyChainingEdge,
		SPU_DeleteChainingEdge,
		SPU_InsertChainingEdge,
		SPU_ModifyRegion,
		SPU_DeleteRegion,
		SPU_InsertRegion,
		SPU_Undefined
	};

public:
	CScenarioEditor();
	~CScenarioEditor();

	// Startup and shutdown the editor (adds/removes widgets)
	void Init();
	void Shutdown();

	void AddWidgets(bkBank* bank);

	bool IsEditing() const						{ return m_bIsEditing; }
	bool IsEditingRegion(int index) const		{ return m_ComboRegion-1 == index; }

	void Update();
	void PostLoadData();
	void PreSaveData();

	bool IsEntitySelected(const CEntity* pEnt) const	{ return m_SelectedEntityWithOverride == pEnt; }
	bool IsRegionSelected(int regionToCheck) const;
	bool IsPointSelected(const CScenarioPoint& pointToCheck) const;
	bool IsPointHandleHovered(const CScenarioPoint& pointToCheck) const;
	bool IsEdgeSelected(const CScenarioChainingEdge& edgeToCheck) const;
	int  GetSelectedOverrideWithNoPropIndex() const { return m_SelectedOverrideWithNoPropIndex; }

	static const char* GetSelectedDLCDevice();

	void Render();

	int GetSelectedSpawnTypeIndex() const { return m_iSpawnTypeIndex; }

	//This call back is used by scenario clustering to identify regions that have been edited by editing the clustering specific widgets.
	static void RegionEditedCallback();
protected:
	void UpdateEditingPointWidgets();							// rebuild the widgets for the point currently being edited
	//bool ValidateEditingPoint(const CScenarioPointRegion& region);		// make sure m_iEditingPoint is in range (makes it so) and returns false if the editing point is not valid (usually an empty array of points)
	void AddNewPoint( CScenarioPointRegion* region, const Vector3& vPos );
	CScenarioPoint* GetSelectedPointToChainFrom(CScenarioChainingEdge& edgeDataInOut);
	int	 FindCurrentRegion() const;//find the region the camera is located in.
	void ScenarioPointDataUpdate(const Vec3V* position, const float* direction); //called to sync data in the selected scenario point.
	void BeforeScenarioPointDataUpdate(CScenarioPoint& point, bool& needToCallOnAddOut);
	void AfterScenarioPointDataUpdate(CScenarioPointRegion& region, CScenarioPoint& point, bool needToCallOnAdd);

	void AddChainWidgets(bkGroup* parentGroup, const CScenarioChain& chain);
	void UpdateCurrentScenarioRegionWidgets();
	void UpdateCurrentScenarioGroupWidgets();
	void UpdateScenarioGroupNameWidgets();
	void UpdateRegionNameWidgets();
	void RebuildScenarioGroupNameList();
	void RebuildScenarioNames();
	void RebuildRegionNames();
	void RebuildPedNames();
	void RebuildVehicleNames();
	void ResetClusterIdNames();

	void PushUndoAction(const eUndoAction action, const int pointOrGroupOrRegionIndex, const bool clearRedoList, const bool bRedo = false/*set if we want to add to the redo list*/);
	void PushUndoItem(const CScenarioPointUndoItem& item, bool clearRedoList, bool redo);
	void Undo(const bool bRedo = false/*set if we want to redo (not undo)*/);

	bool TryToAddChainingEdge(const CScenarioPoint& fromPoint, const CScenarioPoint& toPoint, const CScenarioChainingEdge* edgeDataToCopy);
	
	void PeriodicallyUpdateInterior();
	void UpdateInterior();

	bool PurgeInvalidSelections();
	void ResetSelections();
	CScenarioPointRegion* GetCurrentRegion();
	const CScenarioPointRegion* GetCurrentRegion() const;

	//
	// Widget callbacks
	//
	void LoadManifestPressed();
	void ReloadAllPressed();
	void SaveAllPressed();
	void RemDupEdgeAllPressed();
	void ReloadPressed();
	void SavePressed();
	void LoadSaveAllPressed();
	void UndoPressed();
	void RedoPressed();
	void CurrentGroupComboChanged();
	void CurrentRegionComboChanged();
	void CurrentDLCComboChanged();
	void SelectPlayerContainingRegion();
	void CurrentPointSliderChanged();
	void DeleteNonRegionPointsPressed();
	void CurrentChainingEdgeSliderChanged();
	void SetCameraFocusPoint(Vec3V_In posV);
	void PositionEdited();
	void DeletePressed();	// Delete the point being currently edited 
	void TtyPrintPressed();
	void UpdateChangedGroupWidgetData();
	void UpdateChangedPointWidgetData();						// Copy the data out of the widget data into the currently edited point and then call UpdateChangedPointData to copy into the appropriate C2dEffct
	void UpdateChangedOverrideWidgetData();
	void FlipChainEdgeDirection();
	void FlipFullChainEdgeDirections();
	void AddRegionPressed();
	void RemoveRegionPressed();
	void ScenarioRegionRenamed();
	void AddGroupPressed();
	void RemoveGroupPressed();
	void ScenarioGroupRenamed();
	void FlagEntityOverridesWithMayNotExistPressed();
	int  FindClosestNonPropScenarioPointWithOverride(Vec3V_In pos, ScalarV_In maxDist);
	void TransferPressed();
	void TransferVolumePressed();
	void ToggleAnimDebugPressed();
	void DisableSpawningPressed();
	void RemoveOverridesOnCurrentPropPressed();
	void RemoveOverridesWithNoPropPressed();
	void AddPropScenarioOverridePressed();
	void AttachToEntityPressed();
	void DetachFromEntityPressed();
	int FindSelectedChainIndex() const;
	void ChainUsersChanged();
	void CreateClusterPressed();
	void DeleteClusterPressed();
	void RemoveFromClusterPressed();
	void MergeUnclusteredPressed();
	void MergeToClusterPressed();
	void ReduceNumCarGensOnChainPressed();

	static CEntity* FindEntityToAttachTo(const CScenarioPoint& pt);
	void DoAttachPoint(int pointIndexInRegion, CScenarioPointRegion& region, CEntity& attachEntity, int overrideIndex, int indexWithinOverride);
	void DoDetachPoint(CScenarioPoint& pt, CScenarioPointRegion& region, int insertIndex);
	void DoDisableSpawningOnEntity(CEntity& entity, CScenarioPointRegion& region);
	void DoRemoveOverrideOnEntity(CEntity& entity, CScenarioPointRegion& region, bool expectDisabledSpawnOnly);

	void DoRemoveGroup(int groupIndex);
	void DoRenameScenarioGroup(int groupIndex, const char* name);
	void DoRemoveRegion(int regionIndex);
	void DoRenameRegion(int regionIndex, const char* name);

	void DestroyEntitiesAtScenarioPoint(CScenarioPoint& scenarioPoint, float extraClearRadius = 0.0f);
	void SpawnPedOrVehicleToUseCurrentPointPressed();
	void SpawnPedOrVehicleAtCurrentPointPressed();
	bool SpawnPedToUseCurrentPoint(CScenarioPoint& scenarioPoint, bool instant, bool fallbackToAnyModel );
	bool SpawnVehicleAtCurrentPoint(CScenarioPoint& scenarioPoint);
	void SpawnClusterOfCurrentPointPressed();

	static void ClearAllPedTaskCachedPoints();

	static bool IsValidGroupName(const char* name);
	static bool IsValidRegionName(const char* name);
	static bool IsPositionValidForChaining(const Vector3& position, const CScenarioPoint* exceptionPoint = NULL);
	static bool IsEntityValidForAttachment(CEntity& entity);

	void AttemptSpawnPedOrVehicle();

	void OnEditorWidgetEvent (const CEditorWidget::EditorWidgetEvent& _event);

	//Selected Point actions ...
	static void CheckPointHandleSelection(EditorPoint& editPoint, void* userdata);
	static void TtyPrint(EditorPoint& editPoint, void* userdata);
	static void UpdateSelectedPointFromWidgets(EditorPoint& editPoint, void* userdata);
	static void GatherUniqueChainIds(EditorPoint& editPoint, void* userdata);
	static void GatherInvalidEditorPoints(EditorPoint& editPoint, void* userdata);
	static void DisableSpawningForSelectedEntityPoints(EditorPoint& editPoint, void* userdata);
	static void GatherSelectedClusters(EditorPoint& editPoint, void* userdata);
	static void AddToCluster(EditorPoint& editPoint, void* userdata);
	static void RemoveFromCluster(EditorPoint& editPoint, void* userdata);
	static void TriggerClusterSpawn(EditorPoint& editPoint, void* userdata);

	//Find Functions for selection
	EditorPoint FindClosestDataPoint(const Vector3 & position, const float range) const;
	EditorPoint FindClosestWorldPoint(const CScenarioPointRegionEditInterface& curRegion, const Vector3& startPos, const Vector3& endPos, const float range, float& minT_InOut) const;
	EditorPoint FindClosestClusterPoint(const CScenarioPointRegionEditInterface& curRegion, const Vector3& startPos, const Vector3& endPos, const float range, float& minT_InOut) const;
	EditorPoint FindClosestEntityOverridePoint(const CScenarioPointRegionEditInterface& curRegion, const Vector3& startPos, const Vector3& endPos, const float range, float& minT_InOut) const;
	EditorPoint FindClosestEntityPoint(const Vector3& startPos, const Vector3& endPos, const float range, float& minT_InOut) const;
	bool PointIsCloserThanMinT(const CScenarioPoint& point, const Vector3& startPos, const Vector3& endPos, const float range, float& minT_InOut) const;
	bool PointIsCloserThanMinT(const CScenarioPoint& point, Vec3V_In startPoint, Vec3V_In endPoint, ScalarV_InOut minT_InOut) const;
protected:
	
	int	 GetChainUsersOfChainScenarioPointIsIn(const atArray<CScenarioPointChainUseInfo *> *& arr, const CScenarioPoint & rPoint, CScenarioChainingGraph *& pGraphOut);
	void ClearChainUsersForDebugSpawn(const CScenarioPoint & rPoint);
	
	MousePosition				m_CurrentMousePosition;
	CScenarioAnimDebugHelper	m_ScenarioAnimDebugHelper;
	CEditorWidget				m_EditorWidget;
	CEditorWidget::Delegate		m_EditorWidgetDelegate;
	bool						m_bAutoResourceSavedRegions;
	bool						m_bAutoChainNewPoints;				// if set, automatically chain newly placed points with the selected point.
	bool						m_bIgnoreWater;						// when placing points ignore water as an intersection point for mouse input.
	bool						m_bIsEditing;						// used to determine if mouse update needed
	bool						m_bFreePlacementMode;				// used to place points in-space without regard for world intersections.
	ScenarioEditorSelection		m_SelectedEditingPoints;			// All the selected entity points ... 
	int							m_SliderPoint;
	int							m_LastSliderPoint;					// used to reinitialize some ped/vehicle widget variables
	atHashWithStringBank		m_LastSelectedScenarioType;			// stores the last selected scenario type (Default is 0)
	RegdEnt						m_SelectedEntityWithOverride;
	int							m_SelectedOverrideWithNoPropIndex;
	int							m_CurrentChainingEdge;
	int							m_CurrentScenarioGroup;
	int							m_CurrentRegion;
	int							m_ComboRegion;
	int							m_TransferRegion;
	Vector3						m_TransferVolMax;
	Vector3						m_TransferVolMin;
	Vec3V						m_RegionBoundingBoxMax;
	Vec3V						m_RegionBoundingBoxMin;
	u32							m_TimeForNextInteriorUpdateMs;		// Time stamp used for periodic updates of interior info.
	u8							m_AttemptPedVehicleSpawnAtPoint;	// number of frames to attempt a spawn of a ped/vehicle when editor button pressed
	u8							m_AttemptPedVehicleSpawnUsePoint;	// number of frames to attempt a spawn of a ped/vehicle when editor button pressed
	bool						m_bSpawnInRandomOrientation;
	bool						m_bIgnorePavementChecksWhenLeavingScenario;
	float						m_SpawnOrientation;
	u32							m_MaxChainUsers;

	// PURPOSE:	Enum for keeping track of whether we are currently editing a ped scenario point
	//			or a vehicle scenario point, needed for maintaining some widget stuff.
	enum CurrentPointType
	{
		kPointTypeInvalid,
		kPointTypePed,
		kPointTypeVehicle
	} m_CurrentPointType;

	bkGroup *				m_EditingPointWidgetGroup;
	bkGroup *				m_EditingPointAnimDebugGroup;
	bkSlider *				m_EditingPointSlider;
	bkSlider *				m_EditingChainingEdgeSlider;
	bkCombo *				m_ScenarioGroupCombo;
	bkCombo *				m_DLCCombo;
	bkCombo *				m_RegionCombo;
	bkCombo *				m_RegionTransferCombo;
	bkGroup *				m_RegionTransferWidgetGroup;
	bkGroup *				m_EditingRegionWidgetGroup;
	bkGroup *				m_ScenarioGroupWidgetGroup;
	bkButton *				m_UndoButtonWidget;
	bkButton *				m_RedoButtonWidget;

	char					m_ScenarioRegionName[128];
	char					m_ScenarioGroupName[32];
	bool					m_ScenarioGroupEnabledByDefault;

	// we edit these with widgets (rather than the point directly) so we can handle undo correctly
	Vec3V					m_vSelectedPos;
	int						m_iAvailableInMpSp;
	int						m_iSpawnTypeIndex;
	int						m_iPedTypeIndex;
	int						m_iVehicleTypeIndex;
	int						m_iGroup;
	char					m_RequiredImapName[128];
	int						m_iSpawnStartTime;
	int						m_iSpawnEndTime;
	float					m_Probability;						// Probability value set for the currently selected point, where 0.0f means no override.
	float					m_TimeTillPedLeaves;
	float					m_Radius;							// How far around this point is considered this point's "territory."
	u32						m_PointFlags;						// Flags for the point currently being edited - could be CScenarioPointFlags::FlagsBitSet, except that that would make it trickier with the widget.
	int						m_ChainingEdgeAction;				// For editing CScenarioChainingEdge::m_Action.
	int						m_ChainingEdgeNavMode;				// For editing CScenarioChainingEdge::m_NavMode.
	int						m_ChainingEdgeNavSpeed;				// For editing CScenarioChainingEdge::m_NavSpeed.
	bool					m_OverrideEntityMayNotAlwaysExist;	// For editing CScenarioEntityOverride.m_EntityMayNotAlwaysExist.
	int						m_iMergeToCluster;					// For merging clusters into one
	atArray<const char *>	m_ScenarioNames;					// for combo box
	atArray<const char *>	m_RegionNames;						// for combo box
	CModelSetNameData		m_PedNames;							// for combo box
	CModelSetNameData		m_VehicleNames;						// for combo box
	atArray<const char *>	m_GroupNames;						// for combo box
	char **					m_FoundClusterIdNames;				// for combo box
	int						m_NumFoundClusterIdNames;			// for m_FoundClusterIdNames

	bool					m_MouseOverHandle;					// set if the mouse pointer is over the handle of the editing point (irrespective of mouse button state)
	bool					m_SelectingHandle;					// set if we are rotating the handle of the editing point

	bool					m_UpdatingWidgets;					// set to true temporarily when updating widgets to prevent some widget callback issues.

	// Some consts used by the editor
	static const float		sm_DefaultCameraDistance;
	static const float		sm_cHandleSelectRange;
	static const float		sm_cPointDrawDist;
	static atArray<const char *>	m_DLCNames;							// for combo box
	static int							m_ComboDLC;
	static const int		UNDO_QUEUE_SIZE = 20;				// can undo/redo up to 20 steps
	static const u8			MAX_SPAWN_FRAMES = 10;				// Number of frames to attempt a spawn of a ped/vehicle

	// Container for the undo data
	class CScenarioPointUndoItem {
	public:
		CScenarioPointUndoItem() : m_Action(SPU_Undefined), m_PointIdBeforeOperation(EditorPoint::kPointIdInvalid), m_StartOfStep(true)					{ }
		void SetPoint(const eUndoAction action, const int pointId, const CScenarioPoint& pointData);
		void SetPoint(const eUndoAction action, const int pointId, const CExtensionDefSpawnPoint& pointData);
		void SetEntity(const eUndoAction action, CEntity* pEntity);
		void SetChainingEdge(const eUndoAction action, const int edgeIndex, const CScenarioChainingEdge& edgeData);
		void SetGroup(const eUndoAction action, const int groupIndex, const CScenarioPointGroup& groupData);
		void SetDeleteChainingEdge(const CScenarioChainingNode& nodeFrom, const CScenarioChainingNode& nodeTo, const CScenarioChainingEdge& edgeData);
		void SetRegion(const eUndoAction action, const int regionIndex, const atHashString& regionNameHash);
		eUndoAction GetAction() const										{ Assert(m_Action!=SPU_Undefined); return m_Action; }
		const CExtensionDefSpawnPoint& GetPointData() const;
		const CScenarioPointGroup& GetGroupData() const;
		const atHashString& GetRegionName() const;
		const CScenarioChainingEdge& GetChainingEdgeData() const			{ return m_ChainingEdgeData; }
		int GetPointOrGroupOrRegionIndex() const							{ Assert(m_Action!=SPU_Undefined); return m_PointOrGroupOrRegionIndex; }

		const CScenarioChainingNode& GetNodeFrom() const					{ return m_NodeFrom; }
		const CScenarioChainingNode& GetNodeTo() const						{ return m_NodeTo; }

		void SetPointIdBeforeOperation(u32 pointId) { m_PointIdBeforeOperation = pointId; }
		u32 GetPointIdBeforeOperation() const { return m_PointIdBeforeOperation; }

		void SetAttachEntity(CEntity* ptr) { m_AttachEntity = ptr; }
		CEntity* GetAttachEntity() const { return m_AttachEntity; }

		void SetStartOfStep(bool b)											{ m_StartOfStep = b; }
		bool GetStartOfStep() const											{ return m_StartOfStep; }

		static bool IsPointAction(eUndoAction a)
		{	return a == SPU_ModifyPoint || a == SPU_DeletePoint || a == SPU_InsertPoint || a == SPU_AttachPoint || a == SPU_DetachPoint;	}

		static bool IsGroupAction(eUndoAction a)
		{	return a == SPU_ModifyGroup || a == SPU_DeleteGroup || a == SPU_InsertGroup;	}

		static bool IsRegionAction(eUndoAction a)
		{	return a == SPU_ModifyRegion || a == SPU_DeleteRegion || a == SPU_InsertRegion;	}		

		static bool IsChainingEdgeAction(eUndoAction a)
		{	return a == SPU_ModifyChainingEdge || SPU_DeleteChainingEdge || a == SPU_InsertChainingEdge;	}

	protected:
		enum eUndoAction m_Action;
		int m_PointOrGroupOrRegionIndex;
		CExtensionDefSpawnPoint m_PointData;
		CScenarioPointGroup m_GroupData;
		CScenarioChainingEdge m_ChainingEdgeData;
		atHashString m_RegionName;

		CScenarioChainingNode m_NodeFrom;
		CScenarioChainingNode m_NodeTo;

		// PURPOSE:	For the SPU_DetachPoint operation, this is the entity we were attached to,
		//			so that if we undo that, we get attached to the same entity again.
		RegdEnt m_AttachEntity;

		// PURPOSE:	For SPU_AttachPoint and SPU_DetachPoint(), this is the ID of the point before
		//			the attachment/detachment, so if we undo the operation, we can insert them where
		//			they get their old ID back.
		u32 m_PointIdBeforeOperation;

		// PURPOSE:	If set, break the undo procedure after undoing this action. Otherwise,
		//			keep undoing the next action.
		// NOTES:	Used to undo multiple actions in one step, from the user's point of view.
		bool m_StartOfStep;

		// Note: no undo action uses both m_PointData and m_GroupData, so potentially memory could be
		// saved by trying to combine them (though they have constructors, so it's probably not as simple
		// as using a union). We only have UNDO_QUEUE_SIZE (20) of these objects though, so it shouldn't
		// be a whole lot of memory lost here.
	};

	class UndoHistory
	{
	public:
		UndoHistory()
				: m_InUndoStep(false)
		{}

		void Clear()
		{	m_Queue.Reset();	}

		void Append(const CScenarioPointUndoItem& item)
		{
			//To many items to undo so take from the end
			if (m_Queue.GetCount()+1 > m_Queue.GetMaxCount())
			{
				m_Queue.Delete(0);//slow way to do this... 
			}
			m_Queue.Push(item);

			// Consider the pushed item the start of a step if the user has not explicitly started
			// a step (meaning that each item should be a step), or if this is truly the first item in the step.
			m_Queue[m_Queue.GetCount() - 1].SetStartOfStep(!m_InUndoStep || m_FirstInStep);
			m_FirstInStep = false;
		}

		int GetCount() const
		{	return m_Queue.GetCount();	}

		void StartUndoStep()
		{
			Assert(!m_InUndoStep);
			m_InUndoStep = true;
			m_FirstInStep = true;
		}

		void EndUndoStep()
		{
			Assert(m_InUndoStep);
			m_InUndoStep = false;
			m_FirstInStep = false;
		}

		atFixedArray<CScenarioPointUndoItem, UNDO_QUEUE_SIZE>	m_Queue;
		bool		m_InUndoStep;
		bool		m_FirstInStep;
	};

	UndoHistory m_UndoHistory;
	UndoHistory m_RedoHistory;

	friend class CScenarioDebug;
};

#endif // SCENARIO_DEBUG
#endif // _SCENARIO_SCENARIOEDITOR_H_
