

#include "ScenarioEditor.h"

// Rage includes
#include "bank/button.h"
#include "bank/msgbox.h"
#include "bank/slider.h"
//#include "curve/mayacurve.h"
#include "input/mouse.h"
#include "vector/geometry.h"
#include "vector/vector3.h"
#include "spatialdata/sphere.h"

// Framework Includes
#include "fwmaths/geomutil.h"
#include "fwsys/timer.h"
#include "fwscene/world/WorldLimits.h"

// Game includes
#include "AI/Ambient/AmbientModelSetManager.h"
#include "camera/debug/DebugDirector.h"
#include "camera/helpers/Frame.h"
#include "camera/CamInterface.h"
#include "debug/DebugScene.h"
#include "game/Clock.h"
#include "game/weather.h"
#include "modelinfo/ModelInfo.h"
#include "modelinfo/ModelInfo_Factories.h"
#include "Network/Network.h"
#include "physics/gtaArchetype.h"
#include "scene/loader/MapData_Extensions.h"
#include "scene/world/GameWorld.h"
#include "scene/WarpManager.h"
#include "task/Default/AmbientAnimationManager.h"
#include "task/Default/TaskAmbient.h"
#include "task/General/TaskBasic.h"
#include "task/Scenario/Info/ScenarioInfo.h"
#include "task/Scenario/Info/ScenarioInfoManager.h"
#include "task/Scenario/ScenarioDebug.h"
#include "task/Scenario/ScenarioManager.h"
#include "task/Scenario/ScenarioPoint.h"
#include "task/Scenario/ScenarioPointManager.h"
#include "task/Scenario/ScenarioPointRegion.h"
#include "task/Scenario/TaskScenarioAnimDebug.h"
#include "Task/Scenario/Info/ScenarioTypes.h"
#include "Task/Scenario/Types/TaskUseScenario.h"
#include "Task/Scenario/Types/TaskVehicleScenario.h"
#include "Streaming/streaming.h"
#include "Streaming/streamingmodule.h"
#include "Peds/Ped.h"
#include "peds/PedIntelligence.h"
#include "Peds/PedPopulation.h"
#include "peds/pedPlacement.h"
#include "Peds/PedFactory.h"
#include "scene/EntityIterator.h"
#include "Vehicles/cargen.h"
#include "Vehicles/VehicleFactory.h"
#include "Vehicles/vehiclepopulation.h"
#include "vfx/VfxHelper.h"

AI_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()

#if SCENARIO_DEBUG

//
// Statics
//
static dev_float DIST_IN_FRONT = 5.0f;
atArray<const char*> CScenarioEditor::m_DLCNames;
int CScenarioEditor::m_ComboDLC = 0;
const float		CScenarioEditor::sm_DefaultCameraDistance = 5.0f;	// distance to put the camera from the spawn point when editing
const float		CScenarioEditor::sm_cHandleSelectRange = 40.0f;
static const char* s_AvailableInMpSpNames[] =
{
	"Both",
	"Only in SP",
	"Only in MP"
};
CompileTimeAssert(CSpawnPoint::kBoth == 0);
CompileTimeAssert(CSpawnPoint::kOnlySp == 1);
CompileTimeAssert(CSpawnPoint::kOnlyMp == 2);
CompileTimeAssert(CScenarioPoint::kBoth == 0);
CompileTimeAssert(CScenarioPoint::kOnlySp == 1);
CompileTimeAssert(CScenarioPoint::kOnlyMp == 2);

static const char* s_NavModeNames[] =
{
	"Direct",
	"NavMesh",
	"Roads (for vehicles only)"
};
CompileTimeAssert(NELEM(s_NavModeNames) == CScenarioChainingEdge::kNumNavModes);

static const char* s_NavSpeedNames[] =
{
	"5 mph",
	"10 mph",
	"15 mph",
	"25 mph",
	"35 mph",
	"45 mph",
	"55 mph",
	"65 mph",
	"80 mph",
	"100 mph",
	"125 mph",
	"150 mph",
	"200 mph",
	"Walk",
	"Run",
	"Sprint"
};
CompileTimeAssert(NELEM(s_NavSpeedNames) == CScenarioChainingEdge::kNumNavSpeeds);

static char gCurrentWarpPosition[256];
static float gWarpStreamingRadius = 600.0f;

void WarpPlayerPed(void)
{
	Vector3 vecPos(VEC3_ZERO);
	Vector3 vecVel(VEC3_ZERO);
	float fHeading = 0.0f;

	// Try to parse the line.
	int nItems = 0;
	{
		//	Make a copy of this string as strtok will destroy it.
		Assertf( (strlen(gCurrentWarpPosition) < 256), "Warp position line is too long to copy.");
		char warpPositionLine[256];
		strncpy(warpPositionLine, gCurrentWarpPosition, 256);

		// Get the locations to store the float values into.
		float* apVals[7] = {&vecPos.x, &vecPos.y, &vecPos.z, &fHeading, &vecVel.x, &vecVel.y, &vecVel.z };

		// Parse the line.
		char* pToken = NULL;
		const char* seps = " ,\t";
		pToken = strtok(warpPositionLine, seps);
		while(pToken)
		{
			// Try to read the token as a float.
			int success = sscanf(pToken, "%f", apVals[nItems]);
			Assertf(success, "Unrecognized token '%s' in warp position line.",pToken);
			if(success)
			{
				++nItems;
				Assertf((nItems < 8), "Too many tokens in warp position line." );
			}
			pToken = strtok(NULL, seps);
		}
	}

	if(nItems)
	{
		CWarpManager::SetWarp(vecPos, vecVel, fHeading, true, true, gWarpStreamingRadius);
	}
}

//-----------------------------------------------------------------------------

bool EditorPoint::IsEditable() const
{
	if(!IsValid())
	{
		return false;
	}
	
	if(IsWorldPoint())
	{
		return true;
	}
	else if(IsClusteredWorldPoint())
	{
		return true;
	}
	else if(IsEntityOverridePoint())
	{
		return true;
	}

	return false;
}


void EditorPoint::SetEntityPointForOverride(int overrideIndex, int pointIndexInOverride)
{
	Assert(overrideIndex >= 0);
	Assert(pointIndexInOverride >= 0);
	Assert(overrideIndex < (1 << (kShiftType - kShiftEntityPointOverrideIndex)));
	Assert(pointIndexInOverride < (1 << kShiftEntityPointOverrideIndex));
	m_PointId = (kTypeEntityPointOverride << kShiftType) | (overrideIndex << kShiftEntityPointOverrideIndex) | pointIndexInOverride;
}

void EditorPoint::SetClusteredWorldPoint(int clusterIdx, int pointIdx)
{
	Assert(clusterIdx >= 0);
	Assert(pointIdx >= 0);
	Assert(clusterIdx < (1 << (kShiftType - kShiftClusterIndex)));
	Assert(pointIdx < (1 << kShiftClusterIndex));
	m_PointId = (kTypeClusteredWorldPoint << kShiftType) | (clusterIdx << kShiftClusterIndex) | pointIdx;
}

CScenarioPoint& EditorPoint::GetScenarioPoint(CScenarioPointRegion* pRegion) const
{
	CScenarioPoint* pt = GetScenarioPointFromId(m_PointId, pRegion);
	Assert(pt);
	return *pt;
}

const CScenarioPoint& EditorPoint::GetScenarioPoint(const CScenarioPointRegion* pRegion) const
{
	// Note: the const_cast here is a bit ugly, but it beats code duplication and should be safe enough.
	const CScenarioPoint* pt = GetScenarioPointFromId(m_PointId, const_cast<CScenarioPointRegion*>(pRegion));
	Assert(pt);
	return *pt;
}

CScenarioPoint* EditorPoint::GetScenarioPointFromId(u32 pointId, CScenarioPointRegion* pRegion)
{
	if(pointId == kPointIdInvalid)
	{
		return NULL;
	}

	if(GetTypeFromId(pointId) == kTypeEntityPoint)
	{
		const int entityPointIndex = (int)(pointId & kMaskIndex);
		if(entityPointIndex < SCENARIOPOINTMGR.GetNumEntityPoints())
		{
			return &SCENARIOPOINTMGR.GetEntityPoint(entityPointIndex);
		}
		else
		{
			// This can probably happen if we haven't validated the point yet (when called through
			// IsPointSelected()). Storing the index of the entity point is pretty unreliable as
			// points in this array will get added and removed as the props stream in and out.
			// The old IsPointSelected() function would do a similar check.
			return NULL;
		}
	}

	if(!pRegion)
	{
		Assertf(pRegion, "EditorPoint::GetScenarioPointFromId Expected region pointer.");
		return NULL;
	}

	CScenarioPointRegionEditInterface bankRegion(*pRegion);
	if(GetTypeFromId(pointId) == kTypeEntityPointOverride)
	{
		const int overrideIndex = GetOverrideIndex(pointId);
		const int indexWithinOverride = GetIndexWithinOverride(pointId);
		if(Verifyf(overrideIndex >= 0 && overrideIndex < pRegion->GetNumEntityOverrides(),
				"Invalid override index %d.", overrideIndex))
		{
			const CScenarioEntityOverride& override = bankRegion.GetEntityOverride(overrideIndex);
			if(Verifyf(indexWithinOverride >= 0 && indexWithinOverride < override.m_ScenarioPointsInUse.GetCount(),
					"Invalid override point index %d.", indexWithinOverride))
			{
				return override.m_ScenarioPointsInUse[indexWithinOverride];
			}
		}
	}
	else if(GetTypeFromId(pointId) == kTypeClusteredWorldPoint)
	{
		const int pointIndex = GetClusterPointIndex(pointId);
		const int clusterIndex = GetClusterIndex(pointId);
		if	(
				Verifyf(clusterIndex < bankRegion.GetNumClusters(), "Got invalid cluster index %d.", (int)clusterIndex) && 
				Verifyf(pointIndex < bankRegion.GetNumClusteredPoints(clusterIndex), "Got invalid point index %d for cluster %d.", (int)pointIndex, (int)clusterIndex)
			)
		{
			return &bankRegion.GetClusteredPoint(clusterIndex, pointIndex);
		}
	}
	else
	{
		const int pointIndex = (int)(pointId & kMaskIndex);
		if(Verifyf(pointIndex < bankRegion.GetNumPoints(), "Got invalid point index %d.", (int)pointIndex))
		{
			return &bankRegion.GetPoint(pointIndex);
		}
	}
	return NULL;
}

//-----------------------------------------------------------------------------

//PERHAPS THIS SHOULD GO IN ITS OWN .h/.cpp?
// Also perhaps use Vec3V?

MousePosition::MousePosition()
: m_Valid(false)
{
	m_MouseScreen.Zero();
	m_MouseFar.Zero();
}

void MousePosition::Update()
{
	m_Valid = false;
	CDebugScene::GetMousePointing(m_MouseScreen, m_MouseFar, false);
	if(m_MouseScreen==m_MouseScreen && m_MouseFar==m_MouseFar)	// nan check
	{
		m_Valid = true;
	}
}

bool MousePosition::IsValid() const
{
	return m_Valid;
}

Vector3 MousePosition::GetDirection() const
{
	return m_MouseFar - m_MouseScreen;
}

Vector3 MousePosition::GetPosScreen() const
{
	return m_MouseScreen;
}

Vector3 MousePosition::GetPosFar() const
{
	return m_MouseFar;
}

float MousePosition::DistToScreen(Vector3::Vector3Param point) const
{
	return m_MouseScreen.Dist(point);
}

float MousePosition::Dist2ToScreen(Vector3::Vector3Param point) const
{
	return m_MouseScreen.Dist2(point);
}

float MousePosition::DistToFar(Vector3::Vector3Param point) const
{
	return m_MouseFar.Dist(point);
}

float MousePosition::Dist2ToFar(Vector3::Vector3Param point) const
{
	return m_MouseFar.Dist2(point);
}

//-----------------------------------------------------------------------------

bool ScenarioEditorSelection::WantMultiSelect() const
{
	return (ioMapper::DebugKeyDown(KEY_SHIFT)) ? true : false;
}

bool ScenarioEditorSelection::IsSelected(EditorPoint& checkPoint) const
{
	Assert(checkPoint.IsValid());

	for (int sel = 0; sel < m_Selection.GetCount(); sel++)
	{
		if (m_Selection[sel] == checkPoint)
		{
			return true;
		}
	}

	return false;
}

bool ScenarioEditorSelection::IsMultiSelecting() const
{
	return (m_Selection.GetCount() > 1) ? true : false;
}

bool ScenarioEditorSelection::HasSelection() const
{
	return (m_Selection.GetCount() > 0) ? true : false;
}

void ScenarioEditorSelection::Select(EditorPoint& checkPoint)
{
	if (Verifyf(!IsSelected(checkPoint), "Trying to select the same point again"))
	{
		m_Selection.PushAndGrow(checkPoint);
	}
}

void ScenarioEditorSelection::Deselect(EditorPoint& checkPoint)
{
	for (int sel = 0; sel < m_Selection.GetCount(); sel++)
	{
		if (m_Selection[sel] == checkPoint)
		{
			m_Selection.DeleteFast(sel);
			break;
		}
	}

	Assertf(!IsSelected(checkPoint), "Seems we have multiple selections of the same point.");
}

void ScenarioEditorSelection::DeselectAll()
{
	m_Selection.ResetCount();
}

void ScenarioEditorSelection::ForAll(SelPntActionFunction func, void* userdata)
{
	for (int sel = 0; sel < m_Selection.GetCount(); sel++)
	{
		func(m_Selection[sel], userdata);
	}
}

EditorPoint ScenarioEditorSelection::FindEditorPointForPoint(const CScenarioPoint& toFind, const CScenarioPointRegion& checkRegion) const
{
	EditorPoint retval;
	for (int sel = 0; sel < m_Selection.GetCount(); sel++)
	{
		//Pretty sad that i have to do this ... but is nessecary ...
		CScenarioPointRegion* nonConstRegion = (CScenarioPointRegion*)&checkRegion;
		if (m_Selection[sel].GetScenarioPointPtr(nonConstRegion) == &toFind)
		{
			retval = m_Selection[sel];
		}
	}
	return retval;
}

EditorPoint ScenarioEditorSelection::GetOnlyPoint() const
{
	EditorPoint retval;
	if (m_Selection.GetCount() && Verifyf(!IsMultiSelecting(), "Trying to get single point from a selection that has multiple points."))
	{
		retval = m_Selection[0];
	}
	return retval;
}

void ScenarioEditorSelection::UpdateSelectionForRemovedPoint(const int removedPID)
{
	for (int sel = 0; sel < m_Selection.GetCount(); sel++)
	{
		EditorPoint& point = m_Selection[sel];
		if (point.IsWorldPoint())
		{
			if (removedPID < point.GetWorldPointIndex())
			{
				point.SetWorldPoint(point.GetWorldPointIndex()-1);
			}
		}
	}
}

void ScenarioEditorSelection::UpdateSelectionForRemovedClusterPoint(const int removedCID, const int removedCPID)
{
	for (int sel = 0; sel < m_Selection.GetCount(); sel++)
	{
		EditorPoint& point = m_Selection[sel];
		if (point.IsClusteredWorldPoint())
		{
			const int cID = EditorPoint::GetClusterIndex(point.GetPointId());
			if (cID == removedCID)
			{
				const int cpID = EditorPoint::GetClusterPointIndex(point.GetPointId());
				if (removedCPID < cpID)
				{
					point.SetClusteredWorldPoint(cID, cpID-1);
				}
			}
			
		}
	}
}

//-----------------------------------------------------------------------------

//
// Code
//

CScenarioEditor::CScenarioEditor()
							: m_bAutoChainNewPoints(false)
							, m_bIgnoreWater(false)
							, m_bAutoResourceSavedRegions(false)
							, m_bIsEditing(false)
							, m_SliderPoint(-1)
							, m_LastSliderPoint(-1)
							, m_CurrentChainingEdge(-1)
							, m_CurrentScenarioGroup(0)
							, m_SelectingHandle(false)
							, m_MouseOverHandle(false)
							, m_UpdatingWidgets(false)
							, m_CurrentPointType(kPointTypeInvalid)
							, m_EditingPointWidgetGroup(NULL)
							, m_EditingPointAnimDebugGroup(NULL)
							, m_EditingPointSlider(NULL)
							, m_EditingChainingEdgeSlider(NULL)
							, m_ScenarioGroupCombo(NULL)
							, m_ScenarioGroupWidgetGroup(NULL)
							, m_UndoButtonWidget(NULL)
							, m_RedoButtonWidget(NULL)
							, m_SelectedOverrideWithNoPropIndex(-1)
							, m_ScenarioGroupEnabledByDefault(false)
							, m_vSelectedPos(V_ZERO)
							, m_iAvailableInMpSp(0)
							, m_iSpawnTypeIndex(0)
							, m_iPedTypeIndex(0)
							, m_iVehicleTypeIndex(0)
							, m_iGroup(CScenarioPointManager::kNoGroup)
							, m_iSpawnStartTime(0)
							, m_iSpawnEndTime(0)
							, m_Probability(0.0f)
							, m_TimeTillPedLeaves(0.0f)
							, m_Radius(0.0f)
							, m_ChainingEdgeAction(CScenarioChainingEdge::kActionDefault)
							, m_ChainingEdgeNavMode(CScenarioChainingEdge::kNavModeDefault)
							, m_ChainingEdgeNavSpeed(CScenarioChainingEdge::kNavSpeedDefault)
							, m_OverrideEntityMayNotAlwaysExist(false)
							, m_PedNames(CAmbientModelSetManager::kPedModelSets)
							, m_VehicleNames(CAmbientModelSetManager::kVehicleModelSets)
							, m_CurrentRegion(0)
							, m_ComboRegion(0)
							, m_RegionCombo(NULL)
							, m_DLCCombo(NULL)
							, m_EditingRegionWidgetGroup(NULL)
							, m_RegionTransferWidgetGroup(NULL)
							, m_RegionTransferCombo(NULL)
							, m_TransferRegion(0)
							, m_RegionBoundingBoxMax(V_ZERO)
							, m_RegionBoundingBoxMin(V_ZERO)
							, m_TimeForNextInteriorUpdateMs(0)
							, m_bFreePlacementMode(false)
							, m_AttemptPedVehicleSpawnAtPoint(0)
							, m_AttemptPedVehicleSpawnUsePoint(0)
							, m_bSpawnInRandomOrientation(true)
							, m_bIgnorePavementChecksWhenLeavingScenario(false)
							, m_SpawnOrientation(0.0f)
							, m_MaxChainUsers(0)
							, m_iMergeToCluster(0)
							, m_FoundClusterIdNames(NULL)
							, m_NumFoundClusterIdNames(0)
{
	m_RequiredImapName[0] = '\0';
	m_ScenarioGroupName[0] = '\0';
	m_ScenarioRegionName[0] = '\0';
	m_TransferVolMax.Zero();
	m_TransferVolMin.Zero();

	//Only allow the widget to send messages about translating 
	m_EditorWidget.Init(CEditorWidget::TRANSLATE_ALL);
	m_EditorWidgetDelegate.Bind(this, &CScenarioEditor::OnEditorWidgetEvent);
	m_EditorWidget.m_Delegator.AddDelegate(&m_EditorWidgetDelegate);
}


CScenarioEditor::~CScenarioEditor()
{
	Shutdown();
}


void CScenarioEditor::Init()
{

}


void CScenarioEditor::Shutdown()
{
	ResetClusterIdNames();
}


void CScenarioEditor::Update()
{
	CScenarioPointRegion* region = NULL;
	if (m_CurrentRegion > 0)
	{
		region = GetCurrentRegion();
		if (!region)
		{
			//it is probably trying to stream in ... 
			UpdateCurrentScenarioRegionWidgets();
		}
	}
	else if (m_ComboRegion > 0 && m_ComboRegion != m_CurrentRegion)
	{
		//if we are not updated try to update cause may have just dropped player into another region
		UpdateCurrentScenarioRegionWidgets();
	}

	// Update the widgets showing the bounding box - we could try to do this when it changes, but
	// it's much simpler to just do it once per update.
	if(m_CurrentRegion > 0)
	{
		const spdAABB& aabb = SCENARIOPOINTMGR.GetRegionAABB(m_CurrentRegion - 1);
		m_RegionBoundingBoxMax = aabb.GetMax();
		m_RegionBoundingBoxMin = aabb.GetMin();
	}
	else
	{
		m_RegionBoundingBoxMax = Vec3V(V_ZERO);
		m_RegionBoundingBoxMin = Vec3V(V_ZERO);
	}

	PeriodicallyUpdateInterior();
	if(PurgeInvalidSelections())
	{
		UpdateEditingPointWidgets();
	}
	
	// first off get the mouse inputs irrespective of what we are going to do with them
	m_CurrentMousePosition.Update();	

	CEntity* pEntityUnderMouse = NULL;
	Vector3 vPos;
	bool bHasPosition = false;
	if(m_CurrentMousePosition.IsValid())
	{
		Vec3V oceanPos;
		//check water first 
		Vector3 mpScreen = m_CurrentMousePosition.GetPosScreen();
		Vector3 mpFar = m_CurrentMousePosition.GetPosFar();
		if (!m_bIgnoreWater && CVfxHelper::TestLineAgainstWater(RCC_VEC3V(mpScreen), RCC_VEC3V(mpFar), oceanPos))
		{
			bHasPosition = true;
			vPos = VEC3V_TO_VECTOR3(oceanPos);
		}

		Vector3 vNormal;
		void *entity = NULL;
		Vector3 worldPos;
		if(CDebugScene::GetWorldPositionUnderMouse(worldPos, ArchetypeFlags::GTA_ALL_MAP_TYPES | ArchetypeFlags::GTA_OBJECT_TYPE, &vNormal, &entity))
		{
			// If we don't already have a position or if the new position we found was closer than
			// the old (water) position, use it.
			if(!bHasPosition || m_CurrentMousePosition.Dist2ToScreen(worldPos) <= m_CurrentMousePosition.Dist2ToScreen(vPos))
			{
				vPos = worldPos;
				bHasPosition = true;
				pEntityUnderMouse = reinterpret_cast<CEntity*>(entity);
			}
		}
	}

	if (m_bFreePlacementMode)
	{
		Vector3 dir = m_CurrentMousePosition.GetDirection();
		dir.Normalize();
		vPos = m_CurrentMousePosition.GetPosScreen() + dir*DIST_IN_FRONT;

		bHasPosition = true;

		if (m_EditorWidget.WantHoverFocus())
		{
			bHasPosition = false;
		}
	}

	m_MouseOverHandle = false;
	if (bHasPosition && m_CurrentMousePosition.IsValid())
	{
		m_SelectedEditingPoints.ForAll(CScenarioEditor::CheckPointHandleSelection, &m_CurrentMousePosition);
	}

	// Get the raw mouse position.
	const int  mouseScreenX = ioMouse::GetX();
	const int  mouseScreenY = ioMouse::GetY();
	static bool sbLeftButtonHeld = false;
	static bool sbRightButtonHeld = false;
	static bool sbMiddleButtonHeld = false;
	static int mouseLeftClickStartX = 0;
	static int mouseLeftClickStartY = 0;
	bool bLeftButtonPressed = false;
	bool bRightButtonPressed = false;
	bool bMiddleButtonPressed = false;
	if( ioMouse::GetPressedButtons() & ioMouse::MOUSE_LEFT )
	{
		mouseLeftClickStartX = mouseScreenX;
		mouseLeftClickStartY = mouseScreenY;
		bLeftButtonPressed = true;
		sbLeftButtonHeld = true;
	}
	if( sbLeftButtonHeld &&
		(ioMouse::GetButtons() & ioMouse::MOUSE_LEFT)==0 )
	{
		sbLeftButtonHeld = false;
	}
	if( ioMouse::GetPressedButtons() & ioMouse::MOUSE_RIGHT )
	{
		bRightButtonPressed = true;
		sbRightButtonHeld = true;
	}
	if( sbRightButtonHeld &&
		(ioMouse::GetButtons() & ioMouse::MOUSE_RIGHT)==0 )
	{
		sbRightButtonHeld = false;
	}

	if( ioMouse::GetPressedButtons() & ioMouse::MOUSE_MIDDLE )
	{
		bMiddleButtonPressed = true;
		sbMiddleButtonHeld = true;
	}
	if( sbMiddleButtonHeld &&
		(ioMouse::GetButtons() & ioMouse::MOUSE_MIDDLE)==0 )
	{
		sbMiddleButtonHeld = false;
	}

	if (bLeftButtonPressed && bHasPosition)
	{
		// initial press of left button finds the closest point and selects it
		m_SelectingHandle = false;

		m_SelectedEntityWithOverride = NULL;
		m_SelectedOverrideWithNoPropIndex = -1;
		// We are trying to select something here
		if (m_MouseOverHandle)
		{
			// we are clicking on the rotation handle
			m_SelectingHandle = true;
		}
		else if (m_SelectedEditingPoints.WantMultiSelect())
		{
			EditorPoint thePoint = FindClosestDataPoint(vPos, CScenarioDebug::ms_fPointDrawRadius * 2.0f);
			if (thePoint.IsValid())
			{
				if (m_SelectedEditingPoints.IsSelected(thePoint))
				{
					m_SelectedEditingPoints.Deselect(thePoint);
					m_SliderPoint = -1;
					m_CurrentChainingEdge = -1;
					UpdateEditingPointWidgets();
				}
				else
				{
					m_SelectedEditingPoints.Select(thePoint);

					//Update the slider point
					if(thePoint.IsWorldPoint())
					{
						m_SliderPoint = thePoint.GetWorldPointIndex();
					}
					m_CurrentChainingEdge = -1;
					UpdateEditingPointWidgets();
				}
			}
		}
		else
		{
			ResetSelections();//Deselect everything ...

			//Entity override selection ... 
			EditorPoint thePoint = FindClosestDataPoint(vPos, CScenarioDebug::ms_fPointDrawRadius * 2.0f);
			if(!thePoint.IsValid() && region)
			{
				CScenarioPointRegionEditInterface bankRegion(*region);
				//see if we are trying to select an override.
				if(pEntityUnderMouse && (pEntityUnderMouse->GetIsTypeObject() || pEntityUnderMouse->GetIsTypeBuilding()) && pEntityUnderMouse->GetBaseModelInfo())
				{
					CScenarioEntityOverride* pOverride = bankRegion.FindOverrideForEntity(*pEntityUnderMouse);
					//Only allow selection of entities that dont have scenario point overrides 
					// as deleting the points that are overriden for the entity will return it back to the 
					// art generated state ...
					if(pOverride && !pOverride->m_ScenarioPoints.GetCount())
					{
						m_SelectedEntityWithOverride = pEntityUnderMouse;
					}
				}

				//Try to select a chain edge ... 
				if(!m_SelectedEntityWithOverride)
				{
					m_CurrentChainingEdge = bankRegion.FindClosestChainingEdge(RCC_VEC3V(vPos));
					if (m_CurrentChainingEdge < 0)
					{
						int iOverrideIndex = FindClosestNonPropScenarioPointWithOverride(RCC_VEC3V(vPos), ScalarV(CScenarioDebug::ms_fPointDrawRadius * 9.5f));
						if (iOverrideIndex >= 0)
						{
							// This has no prop
							m_SelectedOverrideWithNoPropIndex = iOverrideIndex;
							Displayf("Selected an override with no prop!");
						}
					}
				}
			}
			else if (!m_SelectedEditingPoints.IsSelected(thePoint))
			{
				m_SelectedEditingPoints.Select(thePoint);

				//Update the slider point
				if(thePoint.IsWorldPoint())
				{
					m_SliderPoint = thePoint.GetWorldPointIndex();
				}
			}

			UpdateEditingPointWidgets();
		}
	}
	else if (sbLeftButtonHeld && !m_SelectedEditingPoints.IsMultiSelecting()) //not valid with multiple selected items
	{
		// left held 
		if (mouseScreenX != mouseLeftClickStartX || mouseScreenY != mouseLeftClickStartY )
		{
			EditorPoint onlyPoint = m_SelectedEditingPoints.GetOnlyPoint();
			// mouse moving
			if (region && onlyPoint.IsValid())
			{
				if (mouseLeftClickStartX != 9999)
				{
					// just started moving, add a delete point
					PushUndoAction(SPU_ModifyPoint, (int)onlyPoint.GetPointId(), true);
				}

				if (bHasPosition)	// if no position we still want to act as if the button is held down - just not update positions/rotations
				{
					CScenarioPoint& point = onlyPoint.GetScenarioPoint(region);

					if (m_SelectingHandle)
					{
						// Move the handle
						if(m_CurrentMousePosition.IsValid())	// nan check
						{
							// rotation is only around Z once the data gets into the CScenarioPoint
							const float angle = YAXIS.AngleZ(vPos-VEC3V_TO_VECTOR3(point.GetPosition()));
							Quaternion q;
							q.FromRotation(ZAXIS, angle);
							Matrix34 mat;
							mat.FromQuaternion(q);
							Vector3 vDir;
							vDir = mat.b;
							float direction = rage::Atan2f(-vDir.x, vDir.y);
							ScenarioPointDataUpdate(NULL, &direction);
						}
					}
					else
					{
						//If the point is part of a chain need to make sure that it is at a proper distance for chaining.
						bool valid = true;
						if (point.IsChained())
						{
							valid = IsPositionValidForChaining(vPos, &point);
						}

						// move the point
						if (valid)
						{
							Vec3V position = VECTOR3_TO_VEC3V(vPos);
							ScenarioPointDataUpdate(&position, NULL);
						}
					}
				}
				// invalidate the start position so we don't get a dead spot
				mouseLeftClickStartX = 9999;
				mouseLeftClickStartY = 9999;
			}
		}
	}
	else
		m_SelectingHandle = false;	// unselect the handle as soon as we release the left mouse button

	if (bRightButtonPressed && !bLeftButtonPressed && bHasPosition && region)
	{
		bool create = true;
		if(m_bAutoChainNewPoints && !IsPositionValidForChaining(vPos))
		{
			//There is a point close here so move on without creating the point.
			create = false;
		}

		//Dont create points if we are currently multiselecting ...
		if (m_SelectedEditingPoints.IsMultiSelecting())
		{
			create = false;
		}

		if (create)
		{
			AddNewPoint(region, vPos);
		}
	}

	if(bMiddleButtonPressed && bHasPosition && !m_SelectedEditingPoints.IsMultiSelecting())
	{
		EditorPoint thePoint = m_SelectedEditingPoints.GetOnlyPoint();
		if(!m_MouseOverHandle)
			thePoint = FindClosestDataPoint(vPos, CScenarioDebug::ms_fPointDrawRadius * 2.0f);
		
		if(thePoint.IsValid() && !m_SelectedEditingPoints.IsSelected(thePoint))
		{
			//Try to create a chain edge between the points ... 
			CScenarioChainingEdge edgeDataToCopy;

			const CScenarioPoint* fromPoint = GetSelectedPointToChainFrom(edgeDataToCopy);
			const CScenarioPoint* toPoint = NULL;
			if(thePoint.IsValid())
			{
				toPoint = &thePoint.GetScenarioPoint(region);
			}

			if(fromPoint && toPoint && fromPoint != toPoint 
				&& IsPositionValidForChaining(VEC3V_TO_VECTOR3(fromPoint->GetPosition()), fromPoint) 
				&& IsPositionValidForChaining(VEC3V_TO_VECTOR3(toPoint->GetPosition()), toPoint)
			  )
			{
				if(TryToAddChainingEdge(*fromPoint, *toPoint, &edgeDataToCopy))
				{
					ResetSelections();
					UpdateEditingPointWidgets();
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	Mat34V current(V_IDENTITY);
	CScenarioPoint* pSelPoint = NULL;
	if (!m_SelectedEditingPoints.IsMultiSelecting())
	{
		EditorPoint onlyPoint = m_SelectedEditingPoints.GetOnlyPoint();
		if(onlyPoint.IsValid())
		{
			if(onlyPoint.IsEditable())
			{
				CScenarioPointRegion* region = GetCurrentRegion();
				Assert(region);
				pSelPoint = &onlyPoint.GetScenarioPoint(region);

				if (pSelPoint)
				{
					current.SetCol3(pSelPoint->GetPosition());
				}
			}
			else
			{
				pSelPoint = &SCENARIOPOINTMGR.GetEntityPoint(onlyPoint.GetEntityPointIndex());
			}
		}
	}

	CEntity* pEntity = CDebugScene::FocusEntities_Get(0);
	if (pEntity && !pEntity->GetIsTypePed())
	{
		pEntity = NULL;
	}

	//Update the anim debug queue.
	m_ScenarioAnimDebugHelper.Update(pSelPoint, pEntity);

	//Update widget
	if (m_bFreePlacementMode)
	{		
		m_EditorWidget.Update(current);
	}

	//Update spawning attempts
	if (m_AttemptPedVehicleSpawnAtPoint || m_AttemptPedVehicleSpawnUsePoint)
	{
		AttemptSpawnPedOrVehicle();
	}
}


void CScenarioEditor::Render()
{
#if ENABLE_DEBUG_HEAP
	sysMemAllocator* debugAllocator = sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_DEBUG_VIRTUAL);
	if (g_sysHasDebugHeap && debugAllocator)
	{
		char buff[128];
		const int memLeftKb = (int)(debugAllocator->GetMemoryAvailable() >> 10);

		if(memLeftKb <= 512)
		{
			formatf(buff, "Remaining Debug Heap: %d kB", memLeftKb);

			Color32 col = (memLeftKb <= 128) ? Color_red : Color_yellow;
			grcDebugDraw::Text(Vec2V(0.1f, 0.1f), col, buff, false, 1.5f, 1.5f);
		}
	}
#endif	//ENABLE_DEBUG_HEAP

	CScenarioPointRegion* region = GetCurrentRegion();
	if(region)
	{
		Vec2V textPos(0.1f, 0.125f);

		CScenarioPointRegionEditInterface bankRegion(*region);
		const int maxScenarioPointsAndEdgesPerRegion = CScenarioPointRegionEditInterface::GetMaxScenarioPointsAndEdgesPerRegion();
		const int warnScenarioPointsAndEdgesPerRegion = (3*maxScenarioPointsAndEdgesPerRegion)/4;
		const int cost = bankRegion.CountPointsAndEdgesForBudget();
		if(cost >= warnScenarioPointsAndEdgesPerRegion)
		{
			char buff[128];
			Color32 col;
			float scale;
			if(cost >= maxScenarioPointsAndEdgesPerRegion)
			{
				col = Color_red;
				scale = 1.5f;
			}
			else
			{
				col = Color_yellow;
				scale = 1.0f;
			}
			formatf(buff, "Points + edges used: %d max: %d", cost, maxScenarioPointsAndEdgesPerRegion);
			grcDebugDraw::Text(textPos, col, buff, false, scale, scale);
			textPos.SetYf(textPos.GetYf() + scale*(float)grcDebugDraw::GetScreenSpaceTextHeight()/(float)grcViewport::GetDefaultScreen()->GetHeight());
		}

		const int maxCarGensPerRegion = CScenarioPointRegionEditInterface::GetMaxCarGensPerRegion();
		const int warnCarGensPerRegion = maxCarGensPerRegion/2;		// Warning threshold at 50%, a bit more loose than what we do for the point/edge count.
		const int carGenCost = bankRegion.CountCarGensForBudget();
		if(carGenCost >= warnCarGensPerRegion)
		{
			char buff[128];
			Color32 col;
			float scale;
			if(carGenCost >= maxCarGensPerRegion)
			{
				col = Color_red;
				scale = 1.5f;
			}
			else
			{
				col = Color_yellow;
				scale = 1.0f;
			}
			formatf(buff, "Car generators used: %d max: %d", carGenCost, maxCarGensPerRegion);
			grcDebugDraw::Text(textPos, col, buff, false, scale, scale);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// On Screen display of regions that need saving.
	char buff[128];
	float lines = 0;
	bool first = false;
	float offset = 0.01f;
	const int rcount = SCENARIOPOINTMGR.GetNumRegions();
	for (int r = 0; r < rcount; r++)
	{
		if (SCENARIOPOINTMGR.RegionIsEdited(r))
		{
			if (!first)
			{
				grcDebugDraw::Text(Vec2V(0.05f, 0.14f), Color_OrangeRed2, "Edited Regions:", false, 1.0f, 1.0f);
				lines+= 1.0f;
				first = true;
			}
			const char* name = SCENARIOPOINTMGR.GetRegionName(r);
			int length = istrlen(name);
			int noff = 0;
			for (int c = 0; c < length; c++)
			{
				if (name[c] == '/' || name[c] == '\\')
				{
					noff = c+1;
				}
			}
			formatf(buff, "%s", name+noff);
			grcDebugDraw::Text(Vec2V(0.05f, 0.15f + (lines*offset)), Color_OrangeRed2, buff, false, 0.8f, 0.8f);
			lines+=1.0f;
		}
	}

	if (SCENARIOPOINTMGR.ManifestIsEdited())
	{
		grcDebugDraw::Text(Vec2V(0.05f, 0.12f), Color_OrangeRed2, "Manifest Needs Save", false, 1.0f, 1.0f);
	}

	//////////////////////////////////////////////////////////////////////////
	// Transfer box -- cant draw using the CVolumeBox because 
	//					this call is not in the render thread
	//					thus i am using the bounds (which is axis aligned) instead
	if (m_TransferRegion > 0)
	{
		Vec3V volMin = RCC_VEC3V(m_TransferVolMin);
		Vec3V volMax = RCC_VEC3V(m_TransferVolMax);
		Color32 color(0,255,0,128);
		Color32 color2(0,128,0,255);
		grcDebugDraw::BoxAxisAligned(volMin, volMax, color, true);
		grcDebugDraw::BoxAxisAligned(volMin, volMax, color2, false);
	}
	//////////////////////////////////////////////////////////////////////////

	m_ScenarioAnimDebugHelper.Render();

	//Only valid and editable points can be manipulated with widget
	if(m_bFreePlacementMode && m_SelectedEditingPoints.HasSelection() && !m_SelectedEditingPoints.IsMultiSelecting())
	{
		EditorPoint onlyPoint = m_SelectedEditingPoints.GetOnlyPoint();
		Assert(onlyPoint.IsValid());
		if (onlyPoint.IsEditable())
		{
			m_EditorWidget.Render();
			m_EditorWidget.RenderOptions();
		}
	}
}

//When this is called we just need to set the current region as edited ... 
void CScenarioEditor::RegionEditedCallback()
{
	
	CScenarioPointRegion* region = CScenarioDebug::ms_Editor.GetCurrentRegion();
	if(!region)
	{
		return;
	}

	if (Verifyf(CScenarioDebug::ms_Editor.m_CurrentRegion > 0, "CScenarioEditor::RegionEditedCallback Region is invalid."))
	{
		//This region is now considered edited.
		SCENARIOPOINTMGR.FlagRegionAsEdited(CScenarioDebug::ms_Editor.m_CurrentRegion - 1);
	}
}

// In ScenarioPointFlags_parser.h:
extern const char* parser_CScenarioPointFlags__Flags_Strings[];

void CScenarioEditor::AddWidgets(bkBank* bank)
{
	RebuildPedNames();
	RebuildVehicleNames();
	RebuildScenarioNames();
	RebuildRegionNames();
	RebuildScenarioGroupNameList();

	bank->AddText("Warp Player x y z h vx vy vz", gCurrentWarpPosition, sizeof(gCurrentWarpPosition), false, &WarpPlayerPed);

	//Add Weather/Time Overrides
	bank->PushGroup("Weather and Time Overrides", false);
	{
		bank->AddCombo("Override Prev Type", (s32*)&g_weather.GetOverridePrevTypeIndexRef(), g_weather.GetNumTypes()+1, g_weather.GetTypeNames(0));
		bank->AddCombo("Override Next Type", (s32*)&g_weather.GetOverrideNextTypeIndexRef(), g_weather.GetNumTypes()+1, g_weather.GetTypeNames(0));
		bank->AddSlider("Override Interp", &g_weather.GetOverrideInterpRef(), -0.01f, 1.0f, 0.01f);

		bank->AddToggle("Override Time", &CClock::GetTimeOverrideFlagRef());
		bank->AddSlider("Curr Time",&CClock::GetTimeOverrideValRef(), 0, (24*60)-1, 1);
		bank->AddSlider("Time Speed Mult", &CClock::GetTimeSpeedMultRef(), 0.0f, 50.0f, 1.0f);
	}
	bank->PopGroup();
	m_DLCCombo = SCENARIOPOINTMGR.AddWorkingManifestCombo(bank);
	bank->AddButton("Load individual manifest",datCallback(MFA(CScenarioEditor::LoadManifestPressed), this));
	bank->AddButton("Reload Disk and DLC data",datCallback(MFA(CScenarioEditor::ReloadAllPressed), this));
	// Add the widgets
	bank->PushGroup("Scenario Point Editor", false);
	bank->AddToggle("Editing", &m_bIsEditing);
	bank->AddToggle("Free Placement Mode", &m_bFreePlacementMode);
	bank->AddToggle("Resource Saved Regions", &m_bAutoResourceSavedRegions);
	bank->AddSlider("Point Render Distance", &CScenarioDebug::ms_fRenderDistance, 1.0f, 10000.0f, 10.0f);
	bank->AddButton("Save All", datCallback(MFA(CScenarioEditor::SaveAllPressed), this));
	bank->AddButton("Remove All Duplicate Edges", datCallback(MFA(CScenarioEditor::RemDupEdgeAllPressed), this));
	m_UndoButtonWidget = bank->AddButton("Undo", datCallback(MFA(CScenarioEditor::UndoPressed), this));
	m_RedoButtonWidget = bank->AddButton("Redo", datCallback(MFA(CScenarioEditor::RedoPressed), this));

	//////////////////////////////////////////////////////////////////////////
	bank->PushGroup("Regions");
	bank->AddButton("Add New Region", datCallback(MFA(CScenarioEditor::AddRegionPressed), this));
	bank->AddButton("Remove Current Region", datCallback(MFA(CScenarioEditor::RemoveRegionPressed), this));
	bank->AddButton("Reload", datCallback(MFA(CScenarioEditor::ReloadPressed), this));
	bank->AddButton("Save", datCallback(MFA(CScenarioEditor::SavePressed), this));
	//bank->AddButton("Load & Save all regions", datCallback(MFA(CScenarioEditor::LoadSaveAllPressed), this));
	bank->AddToggle("Automatically Chain New Points", &m_bAutoChainNewPoints);
	bank->AddToggle("Ignore Water for placement", &m_bIgnoreWater);
	if (m_RegionNames.GetCount() > 1)
	{
		m_ComboRegion = FindCurrentRegion(); //start out in a real region not "no region"
	}
	m_RegionCombo = bank->AddCombo("Current Region", &m_ComboRegion, m_RegionNames.GetCount(), m_RegionNames.GetElements(), datCallback(MFA(CScenarioEditor::CurrentRegionComboChanged), this));
	bank->AddButton("Make Region Containing Player Current Region", datCallback(MFA(CScenarioEditor::SelectPlayerContainingRegion), this));
	m_EditingRegionWidgetGroup = bank->PushGroup("Current Region");
	//Filled by other function
	bank->PopGroup();
	m_EditingPointWidgetGroup = bank->PushGroup("Current Point or Chaining Edge");
	//Filled by other function
	bank->PopGroup();
	bank->PushGroup("Point Transfer (chained points not transferable)", false);
	m_RegionTransferCombo = bank->AddCombo("Transfer To Region", &m_TransferRegion, m_RegionNames.GetCount(), m_RegionNames.GetElements());
	bank->AddButton("Transfer Selected (Currently NOT undoable)", datCallback(MFA(CScenarioEditor::TransferPressed), this));
	bank->AddVector("Volume Max", &m_TransferVolMax, WORLDLIMITS_XMIN, WORLDLIMITS_XMAX, 1.0f);
	bank->AddVector("Volume Min", &m_TransferVolMin, WORLDLIMITS_XMIN, WORLDLIMITS_XMAX, 1.0f);
	bank->AddButton("Transfer All in Volume (Currently NOT undoable)", datCallback(MFA(CScenarioEditor::TransferVolumePressed), this));
	bank->PopGroup();
	bank->PopGroup();

	//////////////////////////////////////////////////////////////////////////
	bank->PushGroup("Groups");
	m_ScenarioGroupCombo = bank->AddCombo("Current Group", &m_CurrentScenarioGroup, m_GroupNames.GetCount(), m_GroupNames.GetElements(), datCallback(MFA(CScenarioEditor::CurrentGroupComboChanged), this));
	bank->AddButton("Add New Group", datCallback(MFA(CScenarioEditor::AddGroupPressed), this));
	bank->AddButton("Remove Current Group", datCallback(MFA(CScenarioEditor::RemoveGroupPressed), this));
	m_ScenarioGroupWidgetGroup = bank->PushGroup("Current Group");
	//Filled by other function
	bank->PopGroup();
	bank->PopGroup();

	//////////////////////////////////////////////////////////////////////////
	m_EditingPointAnimDebugGroup = bank->PushGroup("Scenario Animation Debugging", false);
	//Filled by other function
	bank->PopGroup();
	m_ScenarioAnimDebugHelper.Init(m_EditingPointAnimDebugGroup);

	bank->PopGroup();
	
	UpdateCurrentScenarioRegionWidgets();
	UpdateCurrentScenarioGroupWidgets();
}

void CScenarioEditor::UpdateEditingPointWidgets()
{
	if (!m_EditingPointWidgetGroup)
		return;

	// I had some issues with crashes with recursive widget callbacks, callstacks
	// like this:
	//	CScenarioEditor::CurrentPointSliderChanged()
	//	rage::datCallback::Call(void*)
	//	rage::bkWidget::Changed()
	//	rage::bkSliderVal<int>::RemoteHandler(rage::bkRemotePacket const&)
	//	rage::bkRemotePacket::ReceivePackets()
	//	rage::bkWidget::Destroy()
	//	rage::bkGroup::Remove(rage::bkWidget&)
	//	CScenarioEditor::UpdateEditingPointWidgets()
	//	CScenarioEditor::CurrentPointSliderChanged()
	//	rage::datCallback::Call(void*)
	//	rage::bkWidget::Changed()
	//	rage::bkSliderVal<int>::RemoteHandler(rage::bkRemotePacket const&)
	//	rage::bkRemotePacket::ReceivePackets()
	//	rage::bkManager::Update()
	// and this m_UpdatingWidgets thing was added to hopefully prevent those.
	if(m_UpdatingWidgets)
	{
		return;
	}
	m_UpdatingWidgets = true;

	while(m_EditingPointWidgetGroup->GetChild())
		m_EditingPointWidgetGroup->Remove(*m_EditingPointWidgetGroup->GetChild());

	// Will be set further down, if a valid point is selected.
	m_CurrentPointType = kPointTypeInvalid;

	if(m_LastSliderPoint != m_SliderPoint)
	{
		// If we switch from one point to another, we probably want to clear out
		// the previous ped/vehicle override, to prevent inadvertently getting some old
		// value back if switching from ped to vehicle scenario type or the other way around.
		m_iPedTypeIndex = m_iVehicleTypeIndex = 0;
		m_LastSliderPoint = m_SliderPoint;
	}

	//No region means we just reset everything and moveon.
	CScenarioPointRegion* region = GetCurrentRegion();
	if (region)
	{
		CScenarioPointRegionEditInterface bankRegion(*region);
		if (m_SelectedEditingPoints.IsMultiSelecting())
		{
			atArray<int> foundClusters;
			m_SelectedEditingPoints.ForAll(CScenarioEditor::GatherSelectedClusters, &foundClusters);

			const int foundCount = foundClusters.GetCount();
			if (!foundCount)//If no clusters selected.
			{
				m_EditingPointWidgetGroup->AddButton("Create Cluster", datCallback(MFA(CScenarioEditor::CreateClusterPressed), this));
			}
			else if (foundCount == 1)//If one cluster selected
			{
				m_EditingPointWidgetGroup->AddButton("Create New Cluster", datCallback(MFA(CScenarioEditor::CreateClusterPressed), this));
				m_EditingPointWidgetGroup->AddButton("Delete Cluster", datCallback(MFA(CScenarioEditor::DeleteClusterPressed), this));
				m_EditingPointWidgetGroup->AddButton("Remove Points From Cluster", datCallback(MFA(CScenarioEditor::RemoveFromClusterPressed), this));
				m_EditingPointWidgetGroup->AddButton("Add Unclustered", datCallback(MFA(CScenarioEditor::MergeUnclusteredPressed), this));
				const int clusterIdx = foundClusters[0];
				bankRegion.AddClusterWidgets(clusterIdx, m_EditingPointWidgetGroup);
			}
			else //If Multiple clusters selected
			{
				m_EditingPointWidgetGroup->AddButton("Create New Cluster", datCallback(MFA(CScenarioEditor::CreateClusterPressed), this));

				ResetClusterIdNames();
				m_NumFoundClusterIdNames = foundCount;
				m_FoundClusterIdNames = rage_new char*[foundCount];
				for (int i = 0; i < foundCount; i++)
				{
					m_FoundClusterIdNames[i] = rage_new char[16];
					formatf(m_FoundClusterIdNames[i], 16, "%d", foundClusters[i]);
				}
				m_EditingPointWidgetGroup->AddCombo("Selected Clusters", &m_iMergeToCluster, m_NumFoundClusterIdNames, (const char**)m_FoundClusterIdNames, 0);
				m_EditingPointWidgetGroup->AddButton("Merge Selected Into Combo Selected Cluster", datCallback(MFA(CScenarioEditor::MergeToClusterPressed), this));
			}
		}
		else
		{
			EditorPoint onlyPoint = m_SelectedEditingPoints.GetOnlyPoint();
			const int numSpawnPoints = bankRegion.GetNumPoints();
			const int numEdges = bankRegion.GetChainingGraph().GetNumEdges();
			if (onlyPoint.IsValid())
			{
				if(onlyPoint.IsWorldPoint() || onlyPoint.IsClusteredWorldPoint() || onlyPoint.IsEntityOverridePoint())
				{
					CScenarioPoint& rPoint = onlyPoint.GetScenarioPoint(region);

					m_EditingPointWidgetGroup->AddButton("Dump Info to TTY", datCallback(MFA(CScenarioEditor::TtyPrintPressed), this));

					if(rPoint.IsEntityOverridePoint())
					{
						m_EditingPointWidgetGroup->AddButton("Detach from Entity", datCallback(MFA(CScenarioEditor::DetachFromEntityPressed), this),
							"Detach the point from the prop it's attached to.");
					}
					else
					{
						m_EditingPointWidgetGroup->AddButton("Attach to Nearby Entity", datCallback(MFA(CScenarioEditor::AttachToEntityPressed), this),
							"Attach the point to the prop it's on top of, or closest to.");
					}

					if (rPoint.IsChained())
					{
						m_EditingPointWidgetGroup->AddButton("Flip Full Chain Direction", datCallback(MFA(CScenarioEditor::FlipFullChainEdgeDirections), this));
					}

					if (rPoint.IsInCluster())
					{
						m_EditingPointWidgetGroup->AddButton("Spawn Cluster", datCallback(MFA(CScenarioEditor::SpawnClusterOfCurrentPointPressed), this));
					}
					else
					{
						m_EditingPointWidgetGroup->AddButton("Spawn Ped/Vehicle To Use Point", datCallback(MFA(CScenarioEditor::SpawnPedOrVehicleToUseCurrentPointPressed), this));
						m_EditingPointWidgetGroup->AddButton("Spawn Ped/Vehicle At Point", datCallback(MFA(CScenarioEditor::SpawnPedOrVehicleAtCurrentPointPressed), this));
						m_EditingPointWidgetGroup->AddToggle("Spawn in random orientation", &m_bSpawnInRandomOrientation);
						m_EditingPointWidgetGroup->AddSlider("Spawn orientation", &m_SpawnOrientation, 0.0f, TWO_PI, 0.01f);
					}

					m_vSelectedPos = rPoint.GetPosition();//update this here so we start with a valid position.
					m_EditingPointWidgetGroup->AddVector("Position", &m_vSelectedPos, WORLDLIMITS_XMIN, WORLDLIMITS_XMAX, 0.5f, datCallback(MFA(CScenarioEditor::PositionEdited), this));
					
					m_iSpawnStartTime = rPoint.GetTimeStartOverride();
					m_iSpawnEndTime = rPoint.GetTimeEndOverride();

					m_EditingPointWidgetGroup->AddSlider("Start Time", &m_iSpawnStartTime, 0, 24, 1, datCallback(MFA(CScenarioEditor::UpdateChangedPointWidgetData), this));
					m_EditingPointWidgetGroup->AddSlider("End Time", &m_iSpawnEndTime, 0, 24, 1, datCallback(MFA(CScenarioEditor::UpdateChangedPointWidgetData), this));

					m_iSpawnTypeIndex = rPoint.GetScenarioTypeVirtualOrReal();
					m_EditingPointWidgetGroup->AddCombo("Spawn Type", &m_iSpawnTypeIndex, m_ScenarioNames.GetCount(), m_ScenarioNames.GetElements(), 0, datCallback(MFA(CScenarioEditor::UpdateChangedPointWidgetData), this));

					if(m_iSpawnTypeIndex >= 0 && m_iSpawnTypeIndex < SCENARIOINFOMGR.GetScenarioCount(true))
					{
						if(CScenarioManager::IsVehicleScenarioType(m_iSpawnTypeIndex))
						{
							m_CurrentPointType = kPointTypeVehicle;
						}
						else
						{
							m_CurrentPointType = kPointTypePed;
						}
					}

					// Add the combo box for the ped or vehicle model set to use.
					// Note: we intentionally use separate variables for these two, and don't clear
					// the other, so that if you switch the type from ped to vehicle and back,
					// you don't lose your previous selection.
					if(m_CurrentPointType == kPointTypeVehicle)
					{
						u32 setHash = CScenarioManager::MODEL_SET_USE_POPULATION;
						if (rPoint.GetModelSetIndex() != CScenarioPointManager::kNoCustomModelSet)
						{
							setHash = CAmbientModelSetManager::GetInstance().GetModelSet(CAmbientModelSetManager::kVehicleModelSets, rPoint.GetModelSetIndex()).GetHash();
						}
						m_iVehicleTypeIndex = m_VehicleNames.GetIndexFromHash(setHash);
						m_EditingPointWidgetGroup->AddCombo("Vehicle Type", &m_iVehicleTypeIndex, m_VehicleNames.GetNumNames(), (const char**)m_VehicleNames.GetNames(), 0, datCallback(MFA(CScenarioEditor::UpdateChangedPointWidgetData), this));
					}
					else
					{
						u32 setHash = CScenarioManager::MODEL_SET_USE_POPULATION;
						if (rPoint.GetModelSetIndex() != CScenarioPointManager::kNoCustomModelSet)
						{
							setHash = CAmbientModelSetManager::GetInstance().GetModelSet(CAmbientModelSetManager::kPedModelSets, rPoint.GetModelSetIndex()).GetHash();
						}
						m_iPedTypeIndex = m_PedNames.GetIndexFromHash(setHash);
						m_EditingPointWidgetGroup->AddCombo("Ped Type", &m_iPedTypeIndex, m_PedNames.GetNumNames(), (const char**)m_PedNames.GetNames(), 0, datCallback(MFA(CScenarioEditor::UpdateChangedPointWidgetData), this));
					}

					m_iAvailableInMpSp = rPoint.GetAvailability();

					m_EditingPointWidgetGroup->AddCombo("Available in MP/SP", &m_iAvailableInMpSp, NELEM(s_AvailableInMpSpNames), s_AvailableInMpSpNames, datCallback(MFA(CScenarioEditor::UpdateChangedPointWidgetData), this), "Specify if enabled in multiplayer/singleplayer.");

					m_Probability = rPoint.HasProbabilityOverride() ? rPoint.GetProbabilityOverride() : 0.0f;
					m_EditingPointWidgetGroup->AddSlider("Probability", &m_Probability, 0.0f, 1.0f, 0.01f*CScenarioPoint::kProbResolution, datCallback(MFA(CScenarioEditor::UpdateChangedPointWidgetData), this));

					m_TimeTillPedLeaves = rPoint.GetTimeTillPedLeaves();
					m_EditingPointWidgetGroup->AddSlider("Time till Ped Leaves", &m_TimeTillPedLeaves, -1.0f, (float)CScenarioPoint::kTimeTillPedLeavesMax, 1.0f, datCallback(MFA(CScenarioEditor::UpdateChangedPointWidgetData), this));

					m_Radius = rPoint.GetRadius();
					m_EditingPointWidgetGroup->AddSlider("Radius", &m_Radius, 0.0f, 255.0f, 1.0f, datCallback(MFA(CScenarioEditor::UpdateChangedPointWidgetData), this));

					m_PointFlags = rPoint.GetAllFlags();
					for(int i = 0; i < CScenarioPointFlags::Flags_NUM_ENUMS; i++)
					{
						m_EditingPointWidgetGroup->AddToggle(parser_CScenarioPointFlags__Flags_Strings[i], &m_PointFlags, (1 << i), datCallback(MFA(CScenarioEditor::UpdateChangedPointWidgetData), this));
					}

					m_iGroup = rPoint.GetScenarioGroup();
					m_EditingPointWidgetGroup->AddCombo("Group", &m_iGroup, m_GroupNames.GetCount(), (const char**)m_GroupNames.GetElements(), 0, datCallback(MFA(CScenarioEditor::UpdateChangedPointWidgetData), this));

					const char* imapStr = NULL;
					if(rPoint.GetRequiredIMap() != CScenarioPointManager::kNoRequiredIMap)
					{
						atHashString str(SCENARIOPOINTMGR.GetRequiredIMapHash(rPoint.GetRequiredIMap()));
						imapStr = str.GetCStr();
					}
					if(imapStr)
					{
						safecpy(m_RequiredImapName, imapStr);
					}
					else
					{
						m_RequiredImapName[0] = '\0';
					}
					m_EditingPointWidgetGroup->AddText("Required IMAP", m_RequiredImapName, sizeof(m_RequiredImapName), datCallback(MFA(CScenarioEditor::UpdateChangedPointWidgetData), this));

					// Add the IMAP widget only if we have space in the CScenarioPointExtraData pool
					// or if we already have an associated CScenarioPointExtraData object, to reduce the
					// risk of losing data by blowing the pool during editing.
// 					if(CScenarioPointExtraData::GetPool()->GetNoOfFreeSpaces()
// 						|| SCENARIOPOINTMGR.FindExtraData(rPoint))
// 					{
// 						m_EditingPointWidgetGroup->AddText("Required IMAP", m_RequiredImapName, sizeof(m_RequiredImapName), datCallback(MFA(CScenarioEditor::UpdateChangedPointWidgetData), this));
// 					}
// 					else
// 					{
// 						m_EditingPointWidgetGroup->AddTitle("CScenarioPointExtraData pool full (%d).", CScenarioPointExtraData::GetPool()->GetSize());
// 					}

					if (rPoint.IsChained())
					{
						const CScenarioChainingGraph& graph = bankRegion.GetChainingGraph();
						const int nodeIndex = graph.FindChainingNodeIndexForScenarioPoint(rPoint);
						const int chainIndex = graph.GetNodesChainIndex(nodeIndex);
						AddChainWidgets(m_EditingPointWidgetGroup, graph.GetChain(chainIndex));
					}

					if(rPoint.IsEntityOverridePoint())
					{
						CEntity* pEntity = rPoint.GetEntity();
						if(pEntity)
						{
							CScenarioEntityOverride* pOverride = bankRegion.FindOverrideForEntity(*pEntity);
							if(pOverride)
							{
								m_OverrideEntityMayNotAlwaysExist = pOverride->m_EntityMayNotAlwaysExist;
								m_EditingPointWidgetGroup->AddToggle("Override Entity May Not Always Exist", &m_OverrideEntityMayNotAlwaysExist, datCallback(MFA(CScenarioEditor::UpdateChangedPointWidgetData), this));
							}
						}
					}	

					if (rPoint.IsInCluster())
					{
						m_EditingPointWidgetGroup->AddButton("Remove Point From Cluster", datCallback(MFA(CScenarioEditor::RemoveFromClusterPressed), this));
						const int clusterIdx = bankRegion.FindPointsClusterIndex(rPoint);
						Assert(clusterIdx != CScenarioPointCluster::INVALID_ID);
						bankRegion.AddClusterWidgets(clusterIdx, m_EditingPointWidgetGroup);
					}
					else
					{
						m_EditingPointWidgetGroup->AddToggle("Ignore Pavement Checks When Leaving Scenario", &m_bIgnorePavementChecksWhenLeavingScenario);
					}

					m_EditingPointWidgetGroup->AddButton("Delete", datCallback(MFA(CScenarioEditor::DeletePressed), this));
				}
				else if(onlyPoint.IsEntityPoint())
				{
					m_EditingPointWidgetGroup->AddButton("Suppress Art-placed Scenario Points on This Prop", datCallback(MFA(CScenarioEditor::DisableSpawningPressed), this),
						"Add an override in the current scenario region, which will suppress any art-attached scenario points on this prop.");
					m_EditingPointWidgetGroup->AddButton("Override Art-placed Scenario Points on This Prop", datCallback(MFA(CScenarioEditor::AddPropScenarioOverridePressed), this),
						"Convert all art-attached scenario points on this prop into editable attached points in the current region.");

					m_EditingPointWidgetGroup->AddToggle("Ignore Pavement Checks When Leaving Scenario", &m_bIgnorePavementChecksWhenLeavingScenario);
					m_EditingPointWidgetGroup->AddButton("Spawn Ped/Vehicle To Use Point", datCallback(MFA(CScenarioEditor::SpawnPedOrVehicleToUseCurrentPointPressed), this));
					m_EditingPointWidgetGroup->AddButton("Spawn Ped/Vehicle At Point", datCallback(MFA(CScenarioEditor::SpawnPedOrVehicleAtCurrentPointPressed), this));
					m_EditingPointWidgetGroup->AddToggle("Spawn in random orientation", &m_bSpawnInRandomOrientation);
					m_EditingPointWidgetGroup->AddSlider("Spawn orientation", &m_SpawnOrientation, 0.0f, TWO_PI, 0.01f);
				}
			}
			else if(m_CurrentChainingEdge >= 0)
			{
				const CScenarioChainingGraph& graph = bankRegion.GetChainingGraph();
				const CScenarioChainingEdge& edge = graph.GetEdge(m_CurrentChainingEdge);

				m_ChainingEdgeAction= edge.GetAction();
				m_EditingPointWidgetGroup->AddCombo("Action", &m_ChainingEdgeAction, CScenarioChainingEdge::kNumActions, CScenarioChainingEdge::sm_ActionNames, 0, datCallback(MFA(CScenarioEditor::UpdateChangedPointWidgetData), this));

				m_ChainingEdgeNavMode = edge.GetNavMode();
				m_EditingPointWidgetGroup->AddCombo("Nav Mode", &m_ChainingEdgeNavMode, CScenarioChainingEdge::kNumNavModes, s_NavModeNames, 0, datCallback(MFA(CScenarioEditor::UpdateChangedPointWidgetData), this));

				m_ChainingEdgeNavSpeed = edge.GetNavSpeed();
				m_EditingPointWidgetGroup->AddCombo("Nav Speed", &m_ChainingEdgeNavSpeed, CScenarioChainingEdge::kNumNavSpeeds, s_NavSpeedNames, 0, datCallback(MFA(CScenarioEditor::UpdateChangedPointWidgetData), this));
				m_EditingPointWidgetGroup->AddButton("Flip Direction", datCallback(MFA(CScenarioEditor::FlipChainEdgeDirection), this));
				m_EditingPointWidgetGroup->AddButton("Flip Full Chain Direction", datCallback(MFA(CScenarioEditor::FlipFullChainEdgeDirections), this));

				const int chainIndex = graph.GetNodesChainIndex(edge.GetNodeIndexFrom());
				AddChainWidgets(m_EditingPointWidgetGroup, graph.GetChain(chainIndex));

				if(m_CurrentChainingEdge < numEdges)
				{
					m_EditingPointWidgetGroup->AddButton("Delete", datCallback(MFA(CScenarioEditor::DeletePressed), this));
				}
			}
			else if(m_SelectedEntityWithOverride)
			{
				m_EditingPointWidgetGroup->AddButton("Restore Art-placed Scenario Points on Current Prop", datCallback(MFA(CScenarioEditor::RemoveOverridesOnCurrentPropPressed), this),
					"Remove the override that's suppressing art-attached scenario points on this prop, making them come back.");

				CScenarioEntityOverride* pOverride = bankRegion.FindOverrideForEntity(*m_SelectedEntityWithOverride);
				if(pOverride)	// This doesn't always seem to exist currently, if you undo after suppressing points - probably more or less a bug.
				{
					m_OverrideEntityMayNotAlwaysExist = pOverride->m_EntityMayNotAlwaysExist;
					m_EditingPointWidgetGroup->AddToggle("Override Entity May Not Always Exist", &m_OverrideEntityMayNotAlwaysExist, datCallback(MFA(CScenarioEditor::UpdateChangedOverrideWidgetData), this));
				}
			}
			else if (m_SelectedOverrideWithNoPropIndex >= 0)
			{
				m_EditingPointWidgetGroup->AddButton("Delete Prop Override", datCallback(MFA(CScenarioEditor::RemoveOverridesWithNoPropPressed), this),
					"Remove the override");
			}

			if (m_EditingPointSlider)
				m_EditingPointSlider->SetRange(-1.0f, float(numSpawnPoints-1));

			if(m_EditingChainingEdgeSlider)
			{
				m_EditingChainingEdgeSlider->SetRange(-1.0f, float(numEdges - 1));
			}
		}
	}

	m_UpdatingWidgets = false;
}

void CScenarioEditor::AddNewPoint( CScenarioPointRegion* region, const Vector3& vPos )
{
	CScenarioPointRegionEditInterface bankRegion(*region);

	// place down a point with the right button
	CExtensionDefSpawnPoint newPoint;
	bool found = false;
	EditorPoint onlyPoint = m_SelectedEditingPoints.GetOnlyPoint();
	if (onlyPoint.IsValid())
	{
		CScenarioPoint& pt = onlyPoint.GetScenarioPoint(region);
		pt.CopyToDefinition(&newPoint);

		// If it's an attached point, make sure it's the world space direction that
		// we store in the CExtensionDefSpawnPoint. The position doesn't have this problem
		// since we always set it when placing the point anyway.
		if(pt.GetEntity())
		{
			const Vec3V worldFwdDirV = pt.GetWorldDirectionFwd();
			const float x = worldFwdDirV.GetXf();
			const float y = worldFwdDirV.GetYf();
			if(x*x + y*y > square(0.01f))
			{
				const float dir = rage::Atan2f(-x, y);
				Quaternion q;
				q.FromRotation(ZAXIS, dir);
				newPoint.m_offsetRotation.Set(q.xyzw);
			}
		}

		found = true;
	}
	else if(m_CurrentChainingEdge >= 0)
	{
		// If we have an edge selected, copy the data from its destination point, if possible.
		const CScenarioChainingGraph& graph = bankRegion.GetChainingGraph();
		const int chainedNodeIndex = graph.GetEdge(m_CurrentChainingEdge).GetNodeIndexTo();
		const CScenarioPoint* pt = graph.GetChainingNodeScenarioPoint(chainedNodeIndex);
		if(pt)
		{
			pt->CopyToDefinition(&newPoint);
		}
	}

	if(!found)
	{
		// assign some reasonable defaults for the first scenario point
		if (m_LastSelectedScenarioType.GetHash())
		{
			if (CScenarioManager::GetScenarioTypeFromHashKey(m_LastSelectedScenarioType.GetHash()) >= 0)
			{
				newPoint.m_spawnType = m_LastSelectedScenarioType.GetHash();
			}
			else
			{
				m_LastSelectedScenarioType.Clear();//clear it because this name is no longer valid as a scenario type
				const CScenarioInfo * scenarioInfo = CScenarioManager::GetScenarioInfo(0);
				newPoint.m_spawnType = scenarioInfo->GetHash();
			}
		}
		else
		{
			const CScenarioInfo * scenarioInfo = CScenarioManager::GetScenarioInfo(0);
			newPoint.m_spawnType = scenarioInfo->GetHash();
		}

		newPoint.m_pedType = CScenarioManager::MODEL_SET_USE_POPULATION;
		newPoint.m_start = 0;
		newPoint.m_end = 0;
		newPoint.m_availableInMpSp = CSpawnPoint::kBoth;
		newPoint.m_probability = 0.0f;
		newPoint.m_timeTillPedLeaves = CScenarioPoint::kTimeTillPedLeavesNone;
		newPoint.m_radius = 0.0f;
		newPoint.m_highPri = false;
		newPoint.m_extendedRange = false;
		newPoint.m_shortRange = false;

		// rotation is only around Z once the data gets into the CScenarioPoint
		Quaternion q;
		q.FromRotation(ZAXIS, 0.0f/*m_fDirection*/);
		newPoint.m_offsetRotation.Set(q.xyzw);
	}
	// set position to world position off mouse input
	newPoint.m_offsetPosition = vPos;

	// When auto-chaining, it probably makes sense to set the NoSpawn flag automatically, since
	// that is probably what we will want to use most of the time for those.
	// - Update: this is now considered undesirable, see B* 934796: "When creating a chained route
	//   using 'automatically chain new routes', can the automatic addition of a 'no spawn'
	//   flag to future points be removed?"
	//	if(m_bAutoChainNewPoints)
	//	{
	//		newPoint.m_flags.Set(CScenarioPointFlags::NoSpawn, true);
	//	}

	// If we're going to autochain, look for the point we are chaining from,
	// and set the direction of the new point to match the direction of the link
	// we will soon add.
	CScenarioChainingEdge edgeInitData;
	const CScenarioPoint* fromChainPoint = NULL;
	if(m_bAutoChainNewPoints)
	{
		CScenarioPoint* fromPoint = GetSelectedPointToChainFrom(edgeInitData);
		if(fromPoint)
		{
			fromChainPoint = fromPoint;
			Vector3 fromPos = VEC3V_TO_VECTOR3(fromChainPoint->GetPosition());
			Vector3 toPos = vPos;
			float dir = fwAngle::GetRadianAngleBetweenPoints(toPos.x, toPos.y, fromPos.x, fromPos.y);

			Quaternion q;
			q.FromRotation(ZAXIS, dir);
			newPoint.m_offsetRotation.Set(q.xyzw);

			//reorient the from point ... only if it is not already in a chain
			CScenarioPointRegionEditInterface bankRegion(*region);
			if (bankRegion.GetChainingGraph().FindChainingNodeIndexForScenarioPoint(*fromChainPoint) == -1)
			{
				fromPoint->SetDirection(dir);
			}
		}
	}

	int newPointIndex = bankRegion.AddPoint(newPoint);
	if (newPointIndex >= 0)
	{
		CScenarioPoint& newSP = bankRegion.GetPoint(newPointIndex);
		newSP.MarkAsFromRsc(); //points added to the region are RSC points.

		SCENARIOPOINTMGR.UpdateInteriorForPoint(newSP);

		//update the region bound so that if we are chaining this point we find the point we want to chain too.
		SCENARIOPOINTMGR.UpdateRegionAABB(m_CurrentRegion-1);

		m_UndoHistory.StartUndoStep();

		PushUndoAction(SPU_InsertPoint, newPointIndex, true);	// push the insert undo action AFTER inserting the point

		if(m_bAutoChainNewPoints)
		{
			if(fromChainPoint && fromChainPoint != &newSP)
			{
				TryToAddChainingEdge(*fromChainPoint, newSP, &edgeInitData);
			}
		}

		m_UndoHistory.EndUndoStep();

		ResetSelections(); //Clear selection because multiselect add is not allowed.

		EditorPoint toSelect;
		toSelect.SetWorldPoint( newPointIndex );
		Assert(toSelect.IsWorldPoint());
		m_SliderPoint = toSelect.GetWorldPointIndex();

		m_SelectedEditingPoints.Select(toSelect);				
		UpdateEditingPointWidgets();
	}
}

void CScenarioEditor::AddChainWidgets(bkGroup* parentGroup, const CScenarioChain& chain)
{
	bkGroup* myGroup = parentGroup->AddGroup("Chain Settings");
	m_MaxChainUsers = chain.GetMaxUsers();
	myGroup->AddSlider("Max Chain Users", &m_MaxChainUsers, 1, 255, 1, datCallback(MFA(CScenarioEditor::ChainUsersChanged), this));

	myGroup->AddButton("Reduce # of Car Generators on Chain", datCallback(MFA(CScenarioEditor::ReduceNumCarGensOnChainPressed), this));
}

void CScenarioEditor::UpdateCurrentScenarioRegionWidgets()
{
	if(!m_EditingRegionWidgetGroup)
	{
		return;
	}

	if(m_UpdatingWidgets)
	{
		return;
	}
	m_UpdatingWidgets = true;

	if (m_ComboRegion != m_CurrentRegion)
	{
		//Clear widgets ... 
		while(1)
		{
			bkWidget* pWidget = m_EditingRegionWidgetGroup->GetChild();
			if(!pWidget)
			{
				break;
			}
			m_EditingRegionWidgetGroup->Remove(*pWidget);
		}

		ResetSelections();
		m_CurrentRegion = 0;
		m_UndoHistory.Clear();
		m_RedoHistory.Clear();

		CScenarioPointRegion* region = NULL;
		SCENARIOPOINTMGR.ForceLoadRegion(m_ComboRegion-1);
		if (m_ComboRegion > 0)
			region = SCENARIOPOINTMGR.GetRegion(m_ComboRegion-1);

		if (region)//could be streamed out at present
		{
			CScenarioPointRegionEditInterface bankRegion(*region);
			m_CurrentRegion = m_ComboRegion;
			formatf(m_ScenarioRegionName, "%s", m_RegionNames[m_CurrentRegion]);
			m_EditingRegionWidgetGroup->AddText("Name", m_ScenarioRegionName, sizeof(m_ScenarioRegionName) - 1, datCallback(MFA(CScenarioEditor::ScenarioRegionRenamed), this));
			m_EditingRegionWidgetGroup->AddVector("Bounding Box Max", &m_RegionBoundingBoxMax, 0.0f, 0.0f, 0.0f);
			m_EditingRegionWidgetGroup->AddVector("Bounding Box Min", &m_RegionBoundingBoxMin, 0.0f, 0.0f, 0.0f);
			m_EditingPointSlider = m_EditingRegionWidgetGroup->AddSlider("Current Point", &m_SliderPoint, -1, bankRegion.GetNumPoints()-1, 1, datCallback(MFA(CScenarioEditor::CurrentPointSliderChanged), this));
			m_EditingChainingEdgeSlider = m_EditingRegionWidgetGroup->AddSlider("Current Chaining Edge", &m_CurrentChainingEdge, -1, bankRegion.GetChainingGraph().GetNumEdges() - 1, 1, datCallback(MFA(CScenarioEditor::CurrentChainingEdgeSliderChanged), this));
			m_EditingRegionWidgetGroup->AddButton("Delete loose points within region", datCallback(MFA(CScenarioEditor::DeleteNonRegionPointsPressed), this));

			// Could be enabled if needed.
			//	m_EditingRegionWidgetGroup->AddButton("Flag Entity Overrides with May Not Exist", datCallback(MFA(CScenarioEditor::FlagEntityOverridesWithMayNotExistPressed), this));

			UpdateInterior();
		}		
	}
	
	m_UpdatingWidgets = false;

	UpdateEditingPointWidgets();
}

void CScenarioEditor::UpdateCurrentScenarioGroupWidgets()
{
	if(!m_ScenarioGroupWidgetGroup)
	{
		return;
	}

	if(m_UpdatingWidgets)
	{
		return;
	}
	m_UpdatingWidgets = true;

	while(1)
	{
		bkWidget* pWidget = m_ScenarioGroupWidgetGroup->GetChild();
		if(!pWidget)
		{
			break;
		}
		m_ScenarioGroupWidgetGroup->Remove(*pWidget);
	}

	if(m_CurrentScenarioGroup > 0)
	{
		const int index = m_CurrentScenarioGroup - 1;
		CScenarioPointGroup& grp = SCENARIOPOINTMGR.GetGroupByIndex(index);

		formatf(m_ScenarioGroupName, "%s", grp.m_Name.GetCStr());
		m_ScenarioGroupEnabledByDefault = grp.GetEnabledByDefault();

		m_ScenarioGroupWidgetGroup->AddText("Name", m_ScenarioGroupName, sizeof(m_ScenarioGroupName) - 1, datCallback(MFA(CScenarioEditor::ScenarioGroupRenamed), this));
		m_ScenarioGroupWidgetGroup->AddToggle("Enabled by Default", &m_ScenarioGroupEnabledByDefault, datCallback(MFA(CScenarioEditor::UpdateChangedGroupWidgetData), this));
	}

	m_UpdatingWidgets = false;
}


void CScenarioEditor::UpdateScenarioGroupNameWidgets()
{
	if(!m_ScenarioGroupCombo)
	{
		return;
	}

	RebuildScenarioGroupNameList();
	m_ScenarioGroupCombo->UpdateCombo("Current Group", &m_CurrentScenarioGroup, m_GroupNames.GetCount(), m_GroupNames.GetElements(), datCallback(MFA(CScenarioEditor::CurrentGroupComboChanged), this));
	CScenarioDebug::UpdateScenarioGroupCombo();
}

void CScenarioEditor::UpdateRegionNameWidgets()
{
	if(!m_RegionCombo && !m_RegionTransferCombo)
	{
		return;
	}

	RebuildRegionNames();
	if (m_RegionCombo)
	{
		m_RegionCombo->UpdateCombo("Current Region", &m_ComboRegion, m_RegionNames.GetCount(), m_RegionNames.GetElements(), datCallback(MFA(CScenarioEditor::CurrentRegionComboChanged), this));
	}

	if (m_RegionTransferCombo)
	{
		m_RegionTransferCombo->UpdateCombo("Transfer To Region", &m_TransferRegion, m_RegionNames.GetCount(), m_RegionNames.GetElements());
	}
}


void CScenarioEditor::RebuildScenarioGroupNameList()
{
	m_GroupNames.Reset();
	const int numGrps = SCENARIOPOINTMGR.GetNumGroups();
	m_GroupNames.Reserve(numGrps + 1);
	m_GroupNames.Append() = "No Group";
	for(int i = 0; i < numGrps; i++)
	{
		m_GroupNames.Append() = SCENARIOPOINTMGR.GetGroupByIndex(i).GetName();
	}
}

void CScenarioEditor::RebuildScenarioNames()
{
	const int numScenarios = SCENARIOINFOMGR.GetScenarioCount(true);

	m_ScenarioNames.Reset();
	m_ScenarioNames.Reserve(numScenarios);

	for(int i=0;i<numScenarios;i++)
	{
		m_ScenarioNames.Push(SCENARIOINFOMGR.GetNameForScenario(i));
	}
}

void CScenarioEditor::RebuildRegionNames()
{
	const int numRegions = SCENARIOPOINTMGR.GetNumRegions();

	m_RegionNames.Reset();
	m_RegionNames.Reserve(numRegions + 1);
	m_RegionNames.Append() = "No Region";
	for(int i=0;i<numRegions;i++)
	{
		const char* name = SCENARIOPOINTMGR.GetRegionName(i);
		int length = istrlen(name);
		int offset = 0;
		for (int c = 0; c < length; c++)
		{
			if (name[c] == '/' || name[c] == '\\')
			{
				offset = c+1;
			}
		}
		m_RegionNames.Push( name+offset );
	}
}

void CScenarioEditor::RebuildPedNames()
{
	// Generate lists of ped names for the widgets
	static const char * extraValues[] = { "UsePopulation", NULL };
	m_PedNames.Init(&extraValues[0]);
}

void CScenarioEditor::RebuildVehicleNames()
{
	// Generate lists of vehicle names for the widgets
	static const char * extraValues[] = { "UsePopulation", NULL };
	m_VehicleNames.Init(&extraValues[0]);
}

void CScenarioEditor::ResetClusterIdNames()
{
	if (m_FoundClusterIdNames)
	{
		for (int n = 0; n < m_NumFoundClusterIdNames; n++)
		{
			delete [] m_FoundClusterIdNames[n];
			m_FoundClusterIdNames[n] = NULL;
		}
		m_NumFoundClusterIdNames = 0;

		delete [] m_FoundClusterIdNames;
		m_FoundClusterIdNames = NULL;
	}
}
void CScenarioEditor::LoadManifestPressed()
{
	char buff[1024];
	formatf(buff, "Any unsaved/resourced data will be lost. OK to continue?");
	if(bkMessageBox("Scenario Editor", buff, bkMsgOkCancel, bkMsgQuestion) != 1)
	{
		return;
	}
	m_GroupNames.Reset();
	SCENARIOPOINTMGR.WorkingManifestChanged();
	RebuildPedNames();
	RebuildRegionNames();
	RebuildScenarioGroupNameList();
	RebuildScenarioNames();
	RebuildVehicleNames();
	PostLoadData();
}
void CScenarioEditor::ReloadAllPressed()
{
	char buff[1024];
	formatf(buff, "Any unsaved/resourced data will be lost. OK to continue?");
	if(bkMessageBox("Scenario Editor", buff, bkMsgOkCancel, bkMsgQuestion) != 1)
	{
		return;
	}

	SCENARIOPOINTMGR.BankLoad();
	PostLoadData();
}

void CScenarioEditor::SaveAllPressed()
{
	// When saving, we may rearrange the points due to the spatial data structure,
	// so point indices in tasks are not going to be valid.
	ClearAllPedTaskCachedPoints();

	PreSaveData();
	SCENARIOPOINTMGR.Save(m_bAutoResourceSavedRegions);
}

void CScenarioEditor::RemDupEdgeAllPressed()
{
	if (SCENARIOPOINTMGR.RemoveDuplicateChainingEdges())
	{
		//reset the selected chain edge
		m_CurrentChainingEdge = -1;
	}
}

void CScenarioEditor::ReloadPressed()
{
	if (m_CurrentRegion > 0)
	{
		char buff[1024];
		const char* regionName = SCENARIOPOINTMGR.GetRegionName(m_CurrentRegion-1);
		formatf(buff, "Any unsaved/resourced data for region '%s' be lost. OK to continue?", regionName);
		if(bkMessageBox("Scenario Editor", buff, bkMsgOkCancel, bkMsgQuestion) != 1)
		{
			return;
		}

		SCENARIOPOINTMGR.ReloadRegion(m_CurrentRegion-1);
		PostLoadData();
	}
}

void CScenarioEditor::SavePressed()
{
	// When saving, we may rearrange the points due to the spatial data structure,
	// so point indices in tasks are not going to be valid.
	ClearAllPedTaskCachedPoints();

	if (m_CurrentRegion > 0)
	{
		PreSaveData();
		SCENARIOPOINTMGR.SaveRegion(m_CurrentRegion-1, m_bAutoResourceSavedRegions);
	}
}

void CScenarioEditor::LoadSaveAllPressed()
{
	SCENARIOPOINTMGR.LoadSaveAllRegion(false /*resourceSaved*/);
}

void CScenarioEditor::UndoPressed()
{
	Undo();	// undo
}
void CScenarioEditor::RedoPressed()
{
	Undo(true);	// redo
}

void CScenarioEditor::CurrentGroupComboChanged()
{
	UpdateCurrentScenarioGroupWidgets();
}

void CScenarioEditor::CurrentRegionComboChanged()
{
	// If we only allow selection of entities with overrides in the current region,
	// it makes sense to clear out the selection if the region changes, I think.
	m_SelectedEntityWithOverride = NULL;

	UpdateCurrentScenarioRegionWidgets();
}

void CScenarioEditor::CurrentDLCComboChanged()
{
	Displayf("DLC Combo: %d selected : %s", m_ComboDLC, m_DLCNames[m_ComboDLC]);
}

const char* CScenarioEditor::GetSelectedDLCDevice()
{
	return m_DLCNames[m_ComboDLC];
}

void CScenarioEditor::SelectPlayerContainingRegion()
{
	if (m_RegionNames.GetCount() > 1)
	{
		int newRegion = FindCurrentRegion(); //start out in a real region not "no region"
		//Dont need to update if we are selecting the same region.
		if (m_ComboRegion != newRegion)
		{
			m_ComboRegion = newRegion;
			CurrentRegionComboChanged();//called to refresh everything.
		}
	}
}

void CScenarioEditor::CurrentPointSliderChanged()
{
	if(m_SliderPoint >= 0)
	{
		m_CurrentChainingEdge = -1;
	}

	m_SelectedEditingPoints.DeselectAll();

	EditorPoint sliderSelect;
	sliderSelect.SetWorldPoint(m_SliderPoint);

	// move the camera to look at this point
	if(sliderSelect.IsValid())
	{
		const CScenarioPointRegion* region = GetCurrentRegion();
		Assert(region);
		const CScenarioPointRegionEditInterface bankRegion(*region);
		const CScenarioPoint& point = bankRegion.GetPoint(sliderSelect.GetWorldPointIndex());
		SetCameraFocusPoint(point.GetPosition());
	}

	m_SelectedEditingPoints.Select(sliderSelect);

	// change the point widgets
	UpdateEditingPointWidgets();
}

void CScenarioEditor::DeleteNonRegionPointsPressed()
{
	Assert(m_CurrentRegion-1 >= 0);
	const char* regionName = SCENARIOPOINTMGR.GetRegionName(m_CurrentRegion-1);

	//This will delete any points that are within the current region that are not members of the current region.
	char buff[1024];
	formatf(buff, "This will delete any points that are within the current region[%s] that are not members of the region. If the point is in a chain the chain links will be broken. This operation can not be undone. Ok to continue?", regionName);
	if(bkMessageBox("Scenario Editor", buff, bkMsgOkCancel, bkMsgQuestion) != 1)
	{
		return;
	}

	//reset selected just in case we delete the selected point somehow ... 
	ResetSelections();

	SCENARIOPOINTMGR.DeleteLoosePointsWithinRegion(m_CurrentRegion-1);
}

void CScenarioEditor::CurrentChainingEdgeSliderChanged()
{
	if(m_CurrentChainingEdge >= 0)
	{
		m_SelectedEditingPoints.DeselectAll();
		m_SliderPoint = -1;
	}

	UpdateEditingPointWidgets();

	const CScenarioPointRegion* region = GetCurrentRegion();
	Assert(region);
	const CScenarioPointRegionEditInterface bankRegion(*region);
	// move the camera to look at this edge
	const CScenarioChainingGraph& graph = bankRegion.GetChainingGraph();
	const int numEdges = graph.GetNumEdges();
	if(m_CurrentChainingEdge >= 0 && m_CurrentChainingEdge < numEdges)
	{
		const CScenarioChainingEdge& edge = graph.GetEdge(m_CurrentChainingEdge);
		const CScenarioChainingNode& node1 = graph.GetNode(edge.GetNodeIndexFrom());
		const CScenarioChainingNode& node2 = graph.GetNode(edge.GetNodeIndexTo());

		const Vec3V edgePos1V = node1.m_Position;
		const Vec3V edgePos2V = node2.m_Position;
		const Vec3V tgt = Average(edgePos1V, edgePos2V);

		SetCameraFocusPoint(tgt);
	}
}

void CScenarioEditor::SetCameraFocusPoint(Vec3V_In posV)
{
	camDebugDirector& debugDirector = camInterface::GetDebugDirector();
	const bool freeCamActive = debugDirector.IsFreeCamActive();
	if(freeCamActive)
	{
		camFrame& freeCamFrame = debugDirector.GetFreeCamFrameNonConst();

		// calculate an offset position to look at the spawn point - use the existing look direction, pitched down 10degrees
		const float fPitchAngle = -PI*0.0675f;
		Vector3 cameraPositionOffset = freeCamFrame.GetFront();
		float cosine,sine;
		cos_and_sin(cosine,sine, fPitchAngle);
		cameraPositionOffset.Scale(cosine);
		cameraPositionOffset.AddScaled(ZAXIS,sine);

		freeCamFrame.SetPosition(VEC3V_TO_VECTOR3(posV) - cameraPositionOffset * sm_DefaultCameraDistance  );
	}
}

void CScenarioEditor::PositionEdited()
{
	CScenarioPointRegion* region = GetCurrentRegion();
	Assert(region);

	Assert(!m_SelectedEditingPoints.IsMultiSelecting());

	EditorPoint onlyPoint = m_SelectedEditingPoints.GetOnlyPoint();
	//Only allow this to manipulate points in free placement mode
	if (!m_bFreePlacementMode)
	{
		CScenarioPoint& point = onlyPoint.GetScenarioPoint(region);
		m_vSelectedPos = point.GetPosition();
		return;
	}

	if (onlyPoint.IsValid())
	{
		PushUndoAction(SPU_ModifyPoint, (int)onlyPoint.GetPointId(), true);
		ScenarioPointDataUpdate(&m_vSelectedPos, NULL);
	}
}

// Delete widget button pressed - DeleteCurrentEditingPoint
void CScenarioEditor::DeletePressed()
{
	bool updateWidgets = false;

	CScenarioPointRegion* region = GetCurrentRegion();
	Assert(region);
	CScenarioPointRegionEditInterface bankRegion(*region);

	EditorPoint onlyPoint = m_SelectedEditingPoints.GetOnlyPoint();

	if (onlyPoint.IsValid())
	{
		// Note: if we delete a point, we automatically delete any chaining edges leading to
		// or from it. This currently can't be undone. This could probably be solved by deleting
		// those edges here first, and pushing undo actions for each one (ideally with some designation
		// that they should not be individually undone). This hasn't been implemented yet, though.

		if(onlyPoint.IsEntityOverridePoint())
		{
			const u32 pointIdBefore = onlyPoint.GetPointId();

			CScenarioPoint& pt = onlyPoint.GetScenarioPoint(region);
			CEntity* pAttachEntity = pt.GetEntity();
			Assert(pAttachEntity);

			CExtensionDefSpawnPoint copyOfPointDataForUndo;
			pt.CopyToDefinition(&copyOfPointDataForUndo);

			const int indexInRegionAfterOperation = bankRegion.GetNumPoints();
			DoDetachPoint(pt, *region, indexInRegionAfterOperation);

			CScenarioPointUndoItem undoItem;
			undoItem.SetPoint(SPU_DetachPoint, indexInRegionAfterOperation, copyOfPointDataForUndo);
			undoItem.SetPointIdBeforeOperation(pointIdBefore);
			undoItem.SetAttachEntity(pAttachEntity);

			m_UndoHistory.StartUndoStep();
			PushUndoItem(undoItem, true, false);
			PushUndoAction(SPU_DeletePoint, indexInRegionAfterOperation, true);
			m_UndoHistory.EndUndoStep();

			bankRegion.DeletePoint(indexInRegionAfterOperation);
		}
		else if (onlyPoint.IsClusteredWorldPoint())
		{
			const u32 pointId = onlyPoint.GetPointId();
			const int pointIndex = EditorPoint::GetClusterPointIndex(pointId);
			const int clusterIndex = EditorPoint::GetClusterIndex(pointId);

			bankRegion.DeleteClusterPoint(clusterIndex, pointIndex);
			bankRegion.PurgeInvalidClusters();
		}
		else
		{
			Assert(onlyPoint.IsWorldPoint());
			PushUndoAction(SPU_DeletePoint, (int)onlyPoint.GetPointId(), true);
			bankRegion.DeletePoint(onlyPoint.GetWorldPointIndex());
		}

		ClearAllPedTaskCachedPoints();
		updateWidgets = true;
	}

	if(m_CurrentChainingEdge >= 0 && m_CurrentChainingEdge < bankRegion.GetChainingGraph().GetNumEdges())
	{
		const CScenarioChainingGraph& graph = bankRegion.GetChainingGraph();
		const CScenarioChainingEdge& edge = graph.GetEdge(m_CurrentChainingEdge);

		const CScenarioChainingNode& nodeFrom = graph.GetNode(edge.GetNodeIndexFrom());
		const CScenarioChainingNode& nodeTo = graph.GetNode(edge.GetNodeIndexTo());

		CScenarioPointUndoItem item;
		item.SetDeleteChainingEdge(nodeFrom, nodeTo, edge);
		PushUndoItem(item, true, false);
		SCENARIOPOINTMGR.FlagRegionAsEdited(m_CurrentRegion-1);

		bankRegion.DeleteChainingEdgeByIndex(m_CurrentChainingEdge);

		updateWidgets = true;

		if(m_CurrentChainingEdge >= graph.GetNumEdges())
		{
			m_CurrentChainingEdge = graph.GetNumEdges() - 1;
		}
	}

	if(updateWidgets)
	{
		SCENARIOPOINTMGR.UpdateRegionAABB(m_CurrentRegion-1);
		PurgeInvalidSelections();
		UpdateEditingPointWidgets();
	}
}

void CScenarioEditor::TtyPrintPressed()
{
	Displayf("*******************************************************");
	m_SelectedEditingPoints.ForAll(CScenarioEditor::TtyPrint,NULL);
}

void CScenarioEditor::UpdateChangedGroupWidgetData()
{
	if(m_CurrentScenarioGroup <= 0)
	{
		return;
	}
	CScenarioPointGroup& grp = SCENARIOPOINTMGR.GetGroupByIndex(m_CurrentScenarioGroup - 1);

	// Add an undo point prior to changing
	PushUndoAction(SPU_ModifyGroup, m_CurrentScenarioGroup - 1, true);

	grp.m_EnabledByDefault = m_ScenarioGroupEnabledByDefault;
	SCENARIOPOINTMGR.SetManifestNeedSave();
}


// Copy data out of widgets and into the current point (and then into the runtime data)
void CScenarioEditor::UpdateChangedPointWidgetData()
{
	if (!m_SelectedEditingPoints.HasSelection())
	{
		//If no points selected see if the edge is selected.
		CScenarioPointRegion* region = GetCurrentRegion();
		if(m_CurrentChainingEdge >= 0 && region)
		{
			// Add an undo action prior to changing
			PushUndoAction(SPU_ModifyChainingEdge, m_CurrentChainingEdge, true);

			CScenarioPointRegionEditInterface bankRegion(*region);
			const CScenarioChainingGraph& graph = bankRegion.GetChainingGraph();
			const CScenarioChainingEdge& edge = graph.GetEdge(m_CurrentChainingEdge);

			CScenarioChainingEdge updater;
			updater.CopyFull(edge);
			updater.SetAction((CScenarioChainingEdge::eAction)m_ChainingEdgeAction);
			updater.SetNavMode((CScenarioChainingEdge::eNavMode)m_ChainingEdgeNavMode);
			updater.SetNavSpeed((CScenarioChainingEdge::eNavSpeed)m_ChainingEdgeNavSpeed);

			bankRegion.UpdateChainEdge(m_CurrentChainingEdge, updater);
		}
	}
	else
	{
		if (m_SelectedEditingPoints.IsMultiSelecting())
		{
			m_UndoHistory.StartUndoStep();
		}
		
		//Update all the selected points...
		m_SelectedEditingPoints.ForAll(CScenarioEditor::UpdateSelectedPointFromWidgets, NULL);
		
		if (m_SelectedEditingPoints.IsMultiSelecting())
		{
			m_UndoHistory.EndUndoStep();
		}
	}
}

void CScenarioEditor::UpdateChangedOverrideWidgetData()
{
	if(!m_SelectedEntityWithOverride)
	{
		return;
	}

	CScenarioPointRegion* region = GetCurrentRegion();
	if(!region)
	{
		return;
	}

	// Note: to be proper, ideally we would have undo support here:
	CScenarioPointRegionEditInterface bankRegion(*region);
	CScenarioEntityOverride* pOverride = bankRegion.FindOverrideForEntity(*m_SelectedEntityWithOverride);
	if(aiVerifyf(pOverride, "Expected override."))
	{
		pOverride->m_EntityMayNotAlwaysExist = m_OverrideEntityMayNotAlwaysExist;

		SCENARIOPOINTMGR.FlagRegionAsEdited(m_CurrentRegion - 1);
	}
}

void CScenarioEditor::FlipChainEdgeDirection()
{
	CScenarioPointRegion* region = GetCurrentRegion();
	Assert(region);
	CScenarioPointRegionEditInterface bankRegion(*region);

	if (!m_SelectedEditingPoints.HasSelection())
	{
		if(m_CurrentChainingEdge >= 0)
		{
			// Add an undo action prior to changing
			PushUndoAction(SPU_ModifyChainingEdge, m_CurrentChainingEdge, true);

			bankRegion.FlipChainEdgeDirection(m_CurrentChainingEdge);
		}
	}
}

void CScenarioEditor::FlipFullChainEdgeDirections()
{
	CScenarioPointRegion* region = GetCurrentRegion();
	Assert(region);
	CScenarioPointRegionEditInterface bankRegion(*region);
	const CScenarioChainingGraph& chainGraph = bankRegion.GetChainingGraph();

	atArray<int> gatheredChains;
	m_SelectedEditingPoints.ForAll(CScenarioEditor::GatherUniqueChainIds, &gatheredChains);

	if (!gatheredChains.GetCount())
	{
		//Try the selected edge... 
		if(m_CurrentChainingEdge >= 0)
		{
			//Just grab one of the ids
			const CScenarioChainingEdge& edge = chainGraph.GetEdge(m_CurrentChainingEdge);
			const int chainId = chainGraph.GetNodesChainIndex(edge.GetNodeIndexFrom());
			gatheredChains.PushAndGrow(chainId);
		}
	}

	if (gatheredChains.GetCount())
	{
		m_UndoHistory.StartUndoStep();
		for (int c = 0; c < gatheredChains.GetCount(); c++)
		{
			const CScenarioChain& chain = chainGraph.GetChain(gatheredChains[c]);
			for (int eid = 0; eid < chain.GetNumEdges(); eid++)
			{
				const int e = chain.GetEdgeId(eid);
				// Add an undo action prior to changing
				PushUndoAction(SPU_ModifyChainingEdge, e, true);

				bankRegion.FlipChainEdgeDirection(e);
			}
		}
		m_UndoHistory.EndUndoStep();
	}
}

void CScenarioEditor::AddRegionPressed()
{
	char buff[1024];
	int i = 0;
	while(1)
	{
		if(i > 10000)
		{
			Warningf("Failed to add region.");
			return;
		}
		if(i == 0)
		{
			formatf(buff, "NewRegion");
		}
		else
		{
			formatf(buff, "NewRegion%d", i);
		}

		u32 hash = atStringHash(buff);	// Note: basically assumes that atHashString would use atStringHash().
		if(!SCENARIOPOINTMGR.GetRegionByNameHash(hash))
		{
			break;
		}
		i++;
	}

	SCENARIOPOINTMGR.AddRegion(buff);
	m_ComboRegion = SCENARIOPOINTMGR.GetNumRegions();

	PushUndoAction(SPU_InsertRegion, m_ComboRegion - 1, true);	// push the insert undo action AFTER inserting the group

	UpdateRegionNameWidgets();
	UpdateCurrentScenarioRegionWidgets();
}

void CScenarioEditor::RemoveRegionPressed()
{
	if(m_ComboRegion == 0)
	{
		// Can't remove this default group.
		return;
	}

	const int removedIndex = m_ComboRegion - 1;
	const CScenarioPointRegion* region = SCENARIOPOINTMGR.GetRegion(removedIndex);

	//The region could be streamed out ... 
	if (region)
	{
		const CScenarioPointRegionEditInterface bankRegion(*region);
		int numObjects = bankRegion.GetNumPoints();
		int numClusteredObjects = 0;
		for (int c =0; c < bankRegion.GetNumClusters(); c++)
		{
			numClusteredObjects += bankRegion.GetNumClusteredPoints(c);
		}		
		int numChainObjs = bankRegion.GetChainingGraph().GetNumNodes();
		int numChainEdges = bankRegion.GetChainingGraph().GetNumEdges();

		if(numObjects || numChainObjs || numChainEdges || numClusteredObjects)
		{
			char buff[1024];
			const char* regionName = SCENARIOPOINTMGR.GetRegionName(removedIndex);
			formatf(buff, "Region '%s' has %d scenario points, %d clustered points, %d chained points, and %d chain edges. All will be deleted if you delete this region. OK to continue? Deletion of the points, chain points, and chain edges cannot be undone.", regionName, numObjects, numClusteredObjects, numChainObjs, numChainEdges);
			if(bkMessageBox("Scenario Editor", buff, bkMsgOkCancel, bkMsgQuestion) != 1)
			{
				return;
			}

			m_UndoHistory.Clear();
			m_RedoHistory.Clear();
		}
	}

	PushUndoAction(SPU_DeleteRegion, removedIndex, true);
	DoRemoveRegion(removedIndex);
}

void CScenarioEditor::ScenarioRegionRenamed()
{
	if(m_CurrentRegion <= 0)
	{
		return;
	}
	
	const u32 oldHash = atHashString(SCENARIOPOINTMGR.GetRegionName(m_CurrentRegion - 1));
	const u32 newHash = atHashString(m_ScenarioRegionName);
	if(oldHash == newHash)
	{
		return;
	}	

	if(!IsValidRegionName(m_ScenarioRegionName))
	{
		char buff[1024];
		formatf(buff, "'%s' is not a valid name for a scenario region.", m_ScenarioRegionName);
		bkMessageBox("Scenario Editor Error", buff, bkMsgOk, bkMsgError);
		return;
	}

	if(SCENARIOPOINTMGR.GetRegionByNameHash(newHash))
	{
		char buff[1024];
		formatf(buff, "A region called '%s' already exists, the names must be unique.", m_ScenarioRegionName);
		bkMessageBox("Scenario Editor Error", buff, bkMsgOk, bkMsgError);
		return;
	}

	// Add an undo point prior to changing
	PushUndoAction(SPU_ModifyRegion, m_CurrentRegion - 1, true);

	DoRenameRegion(m_CurrentRegion - 1, m_ScenarioRegionName);
}

void CScenarioEditor::AddGroupPressed()
{
	char buff[1024];
	int i = 0;
	while(1)
	{
		if(i > 10000)
		{
			Warningf("Failed to add group.");
			return;
		}
		if(i == 0)
		{
			formatf(buff, "New group");
		}
		else
		{
			formatf(buff, "New group %d", i);
		}
		Assert(IsValidGroupName(buff));

		if(SCENARIOPOINTMGR.FindGroupByNameHash(atHashString(buff), true) == CScenarioPointManager::kNoGroup)
		{
			break;
		}
		i++;
	}

	SCENARIOPOINTMGR.AddGroup(buff);
	m_CurrentScenarioGroup = SCENARIOPOINTMGR.GetNumGroups();

	PushUndoAction(SPU_InsertGroup, m_CurrentScenarioGroup - 1, true);	// push the insert undo action AFTER inserting the group

	UpdateScenarioGroupNameWidgets();
	UpdateCurrentScenarioGroupWidgets();
	UpdateEditingPointWidgets();
}


void CScenarioEditor::RemoveGroupPressed()
{
	if(m_CurrentScenarioGroup == CScenarioPointManager::kNoGroup)
	{
		// Can't remove this default group.
		return;
	}

	const int removedIndex = m_CurrentScenarioGroup - 1;
	char buff[1024];
	const char* groupName = SCENARIOPOINTMGR.GetGroupByIndex(removedIndex).GetName();
	formatf(buff, "All users of group '%s' will be left around but get the group cleared. OK to continue? This clearing cannot be undone, but you can undo the group removal.", groupName);
	if(bkMessageBox("Scenario Editor", buff, bkMsgOkCancel, bkMsgQuestion) != 1)
	{
		return;
	}

	m_UndoHistory.Clear();
	m_RedoHistory.Clear();

	PushUndoAction(SPU_DeleteGroup, removedIndex, true);
	DoRemoveGroup(removedIndex);
}


void CScenarioEditor::ScenarioGroupRenamed()
{
	if(m_CurrentScenarioGroup <= 0)
	{
		return;
	}
	CScenarioPointGroup& grp = SCENARIOPOINTMGR.GetGroupByIndex(m_CurrentScenarioGroup - 1);

	const u32 oldHash = grp.GetNameHash();
	const u32 newHash = atHashString(m_ScenarioGroupName);
	if(oldHash == newHash)
	{
		return;
	}	

	if(!IsValidGroupName(m_ScenarioGroupName))
	{
		char buff[1024];
		formatf(buff, "'%s' is not a valid name for a scenario group.", m_ScenarioGroupName);
		bkMessageBox("Scenario Editor Error", buff, bkMsgOk, bkMsgError);
		return;
	}

	if(SCENARIOPOINTMGR.FindGroupByNameHash(atHashString(m_ScenarioGroupName), true) != CScenarioPointManager::kNoGroup)
	{
		char buff[1024];
		formatf(buff, "A scenario group called '%s' already exists, the names must be unique.", m_ScenarioGroupName);
		bkMessageBox("Scenario Editor Error", buff, bkMsgOk, bkMsgError);
		return;
	}

	// Add an undo point prior to changing
	PushUndoAction(SPU_ModifyGroup, m_CurrentScenarioGroup - 1, true);

	DoRenameScenarioGroup(m_CurrentScenarioGroup - 1, m_ScenarioGroupName);
}

void CScenarioEditor::FlagEntityOverridesWithMayNotExistPressed()
{
	CScenarioPointRegion* pRegion = GetCurrentRegion();
	if(!pRegion)
	{
		return;
	}

	bool changed = false;
	CScenarioPointRegionEditInterface regionInterface(*pRegion);
	const int numOverrides = regionInterface.GetNumEntityOverrides();
	for(int i = 0; i < numOverrides; i++)
	{
		CScenarioEntityOverride& override = regionInterface.GetEntityOverride(i);
		CEntity* pEntity = override.m_EntityCurrentlyAttachedTo;
		if(pEntity)
		{
			if(CScenarioEntityOverride::GetDefaultMayNotAlwaysExist(*pEntity))
			{
				if(!override.m_EntityMayNotAlwaysExist)
				{
					override.m_EntityMayNotAlwaysExist = true;
				}

				grcDebugDraw::Line(override.m_EntityPosition, override.m_EntityPosition + Vec3V(0.0f, 0.0f, 20.0f), Color_green, 10);
			}
		}
	}

	if(changed)
	{
		if(m_CurrentRegion > 0)
		{
			SCENARIOPOINTMGR.FlagRegionAsEdited(m_CurrentRegion - 1);
		}
	}
}

int CScenarioEditor::FindClosestNonPropScenarioPointWithOverride(Vec3V_In pos, ScalarV_In maxDist)
{	
	Assert(IsGreaterThanAll(maxDist, ScalarV(V_ZERO)));
	int iClosestOverrideIndex = -1;
	CScenarioPointRegion* pRegion = GetCurrentRegion();
	if(!pRegion)
	{
		return iClosestOverrideIndex;
	}
	Vec3V mouseScreen, mouseFar;
	CDebugScene::GetMousePointing(RC_VECTOR3(mouseScreen), RC_VECTOR3(mouseFar), false);
	const Vec3V dV = Normalize(Subtract(pos, mouseScreen));
	const Vec3V endPos = AddScaled(pos, dV, ScalarV(V_EIGHT));
	CScenarioPointRegionEditInterface regionInterface(*pRegion);
	const int numOverrides = regionInterface.GetNumEntityOverrides();
	ScalarV t0, t1;
	ScalarV closestOverride = ScalarV(V_FLT_MAX);
	
	for(int i = 0; i < numOverrides; i++)
	{
		CScenarioEntityOverride& override = regionInterface.GetEntityOverride(i);
		CEntity* pEntity = override.m_EntityCurrentlyAttachedTo;
		if (!pEntity)
		{
			int numIntersections = geomSegments::SegmentToSphereIntersections(override.m_EntityPosition, mouseScreen, endPos, maxDist, t0, t1);
			if (numIntersections > 0 && IsLessThanAll(t0, closestOverride))
			{
				closestOverride = t0;
				iClosestOverrideIndex = i;
			}
		}
	}
	return iClosestOverrideIndex;
}

void CScenarioEditor::TransferPressed()
{
	//TODO: multi select transfer support ... this currently not supported because
	// index data of the other selected points could be changed as points are removed
	// from one region and placed in another ... this could be solved by simply 
	// gathering the pointers for the selected data and then using those to 
	// search in the region and transfer information as we go ...
	if (m_SelectedEditingPoints.IsMultiSelecting())
	{
		Errorf("Not allowed to transfer multiselected points ... yet");
		return;
	}

	if(m_TransferRegion <= 0)
	{
		Errorf("Selected transfer to region is invalid please select a valid region");
		return;
	}

	if (m_ComboRegion <= 0)
	{
		Errorf("Selected transfer to region is invalid please select a valid region");
		return;
	}

	if (m_TransferRegion == m_ComboRegion)
	{
		Errorf("Trying to transfer point(s) to same region (%s) -> (%s)", SCENARIOPOINTMGR.GetRegionName(m_ComboRegion-1), SCENARIOPOINTMGR.GetRegionName(m_TransferRegion-1));
		return;
	}

	CScenarioPointRegion* fromRegion = SCENARIOPOINTMGR.GetRegion(m_ComboRegion-1);
	CScenarioPointRegion* toRegion = SCENARIOPOINTMGR.GetRegion(m_TransferRegion-1);

	if (!toRegion)
	{
		//Try to force load the region
		SCENARIOPOINTMGR.ForceLoadRegion(m_TransferRegion-1);
		toRegion = SCENARIOPOINTMGR.GetRegion(m_TransferRegion-1);
		if (!toRegion)
		{
			Errorf("Trying to transfer point(s) to region (%s), but it is not streamed in.", SCENARIOPOINTMGR.GetRegionName(m_TransferRegion-1));
			return;
		}
		else
		{
			SCENARIOPOINTMGR.FlagRegionAsEdited(m_TransferRegion-1);
		}
	}

	if (!fromRegion)
	{
		//Try to force load the region
		SCENARIOPOINTMGR.ForceLoadRegion(m_ComboRegion-1);
		fromRegion = SCENARIOPOINTMGR.GetRegion(m_ComboRegion-1);
		if (!fromRegion)
		{
			Errorf("Trying to transfer point(s) from region (%s), but it is not streamed in.", SCENARIOPOINTMGR.GetRegionName(m_ComboRegion-1));
			return;
		}
		else
		{
			SCENARIOPOINTMGR.FlagRegionAsEdited(m_ComboRegion-1);
		}
	}

	//Currently NOT undoable
	CScenarioPointRegionEditInterface bankFromRegion(*fromRegion);
	CScenarioPointRegionEditInterface bankToRegion(*toRegion);
	const CScenarioChainingGraph& graph = bankFromRegion.GetChainingGraph();

	//Stats
	int pcount = bankFromRegion.GetNumPoints() + bankFromRegion.GetTotalNumClusteredPoints() + bankFromRegion.GetNumEntityOverrides();
	int ecount = graph.GetNumEdges();

	EditorPoint onlyPoint = m_SelectedEditingPoints.GetOnlyPoint();
	if (onlyPoint.IsValid())
	{
		const CScenarioPoint& selPoint = onlyPoint.GetScenarioPoint(fromRegion);

		if (selPoint.IsEntityOverridePoint())
		{
			//for entity overrides we need to find which override this point belongs too then transfer it over ... 
			char buff[1024];
			formatf(buff, "Selected point is a entity override, because of this the entire entity override will be transfered. OK to continue? This cannot be undone.");
			if(bkMessageBox("Scenario Editor", buff, bkMsgOkCancel, bkMsgQuestion) != 1)
			{
				return;
			}

			Assert(selPoint.GetEntity());
			Assertf(!selPoint.IsChained() && !selPoint.IsInCluster(), "The entity override point you are trying to transfer is in a chain or a cluster. This is not supported for transfers.");

			//transfer this entity's overrides ... 
			CScenarioEntityOverride* fromOverride = bankFromRegion.FindOverrideForEntity(*selPoint.GetEntity());
			if (fromOverride)
			{
				CScenarioEntityOverride& toOverride = bankToRegion.FindOrAddOverrideForEntity(*selPoint.GetEntity());
				toOverride.m_EntityPosition = fromOverride->m_EntityPosition;
				toOverride.m_EntityType = fromOverride->m_EntityType;
				toOverride.m_ScenarioPoints = fromOverride->m_ScenarioPoints;
				toOverride.m_EntityMayNotAlwaysExist = fromOverride->m_EntityMayNotAlwaysExist;
				toOverride.m_SpecificallyPreventArtPoints = fromOverride->m_SpecificallyPreventArtPoints;

				//NON-parser data
				toOverride.m_ScenarioPointsInUse = fromOverride->m_ScenarioPointsInUse;
				toOverride.m_EntityCurrentlyAttachedTo = fromOverride->m_EntityCurrentlyAttachedTo;
				toOverride.m_VerificationStatus = fromOverride->m_VerificationStatus;

				//Remove the from override ... 
				const int fromOverrideIndex = bankFromRegion.GetIndexForEntityOverride(*fromOverride);
				bankFromRegion.DeleteEntityOverride(fromOverrideIndex);
			}
		}
		else
		{
			if(selPoint.IsChained())
			{
				char buff[1024];
				formatf(buff, "Selected point is in a chain, because of this the entire chain will be transfered along with any clusters that happen to be part of the chain. OK to continue? This cannot be undone.");
				if(bkMessageBox("Scenario Editor", buff, bkMsgOkCancel, bkMsgQuestion) != 1)
				{
					return;
				}
			}
			else if(selPoint.IsInCluster())
			{
				char buff[1024];
				formatf(buff, "Selected point is in a cluster, because of this the entire cluster will be transfered along with any chains that happen to be linked to the cluster. OK to continue? This cannot be undone.");
				if(bkMessageBox("Scenario Editor", buff, bkMsgOkCancel, bkMsgQuestion) != 1)
				{
					return;
				}
			}
			bankFromRegion.TransferPoint(selPoint, bankToRegion);
		}
	}
	else if (m_SelectedEntityWithOverride)
	{
		//transfer this entity's overrides ... 
		CScenarioEntityOverride* fromOverride = bankFromRegion.FindOverrideForEntity(*m_SelectedEntityWithOverride);
		if (fromOverride)
		{
			CScenarioEntityOverride& toOverride = bankToRegion.FindOrAddOverrideForEntity(*m_SelectedEntityWithOverride);
			toOverride.m_EntityPosition = fromOverride->m_EntityPosition;
			toOverride.m_EntityType = fromOverride->m_EntityType;
			toOverride.m_ScenarioPoints = fromOverride->m_ScenarioPoints;
			toOverride.m_EntityMayNotAlwaysExist = fromOverride->m_EntityMayNotAlwaysExist;
			toOverride.m_SpecificallyPreventArtPoints = fromOverride->m_SpecificallyPreventArtPoints;

			//NON-parser data
			toOverride.m_ScenarioPointsInUse = fromOverride->m_ScenarioPointsInUse;
			toOverride.m_EntityCurrentlyAttachedTo = fromOverride->m_EntityCurrentlyAttachedTo;
			toOverride.m_VerificationStatus = fromOverride->m_VerificationStatus;

			//Remove the from override ... 
			const int fromOverrideIndex = bankFromRegion.GetIndexForEntityOverride(*fromOverride);
			bankFromRegion.DeleteEntityOverride(fromOverrideIndex);
		}
	}

	//reset selected
	ResetSelections();

	int tp = pcount - (bankFromRegion.GetNumPoints() + bankFromRegion.GetTotalNumClusteredPoints() + bankFromRegion.GetNumEntityOverrides());
	int te = ecount - graph.GetNumEdges();

	//If we transfered anything then we need to mark the regions as edited.
	if (tp || te)
	{
		SCENARIOPOINTMGR.FlagRegionAsEdited(m_ComboRegion-1);
		SCENARIOPOINTMGR.FlagRegionAsEdited(m_TransferRegion-1);
	}

	Displayf("Transfer (%d) point(s), (%d) chain edge(s) from region (%s) to region (%s)", tp, te, SCENARIOPOINTMGR.GetRegionName(m_ComboRegion-1), SCENARIOPOINTMGR.GetRegionName(m_TransferRegion-1));
	SCENARIOPOINTMGR.UpdateRegionAABB(m_ComboRegion-1);
	SCENARIOPOINTMGR.UpdateRegionAABB(m_TransferRegion-1);
	UpdateEditingPointWidgets();

	ClearAllPedTaskCachedPoints();
}

void CScenarioEditor::TransferVolumePressed()
{
	if(m_TransferRegion <= 0)
	{
		Errorf("Selected transfer to region is invalid please select a valid region");
		return;
	}

	if (m_ComboRegion <= 0)
	{
		Errorf("Selected transfer to region is invalid please select a valid region");
		return;
	}

	if (m_TransferRegion == m_ComboRegion)
	{
		Errorf("Trying to transfer point(s) to same region (%s) -> (%s)", SCENARIOPOINTMGR.GetRegionName(m_ComboRegion-1), SCENARIOPOINTMGR.GetRegionName(m_TransferRegion-1));
		return;
	}

	CScenarioPointRegion* fromRegion = SCENARIOPOINTMGR.GetRegion(m_ComboRegion-1);
	CScenarioPointRegion* toRegion = SCENARIOPOINTMGR.GetRegion(m_TransferRegion-1);

	if (!toRegion)
	{
		//Try to force load the region
		SCENARIOPOINTMGR.ForceLoadRegion(m_TransferRegion-1);
		toRegion = SCENARIOPOINTMGR.GetRegion(m_TransferRegion-1);
		if (!toRegion)
		{
			Errorf("Trying to transfer point(s) to region (%s), but it is not streamed in.", SCENARIOPOINTMGR.GetRegionName(m_TransferRegion-1));
			return;
		}
		else
		{
			SCENARIOPOINTMGR.FlagRegionAsEdited(m_TransferRegion-1);
		}
	}

	if (!fromRegion)
	{
		//Try to force load the region
		SCENARIOPOINTMGR.ForceLoadRegion(m_ComboRegion-1);
		fromRegion = SCENARIOPOINTMGR.GetRegion(m_ComboRegion-1);
		if (!fromRegion)
		{
			Errorf("Trying to transfer point(s) from region (%s), but it is not streamed in.", SCENARIOPOINTMGR.GetRegionName(m_ComboRegion-1));
			return;
		}
		else
		{
			SCENARIOPOINTMGR.FlagRegionAsEdited(m_ComboRegion-1);
		}
	}

	char buff[1024];
	formatf(buff, "Do you want to transfer any chained, clustered, or entity override data that is in the volume? The entire chain, cluster, or entity override will be transfered if yes. This cannot be undone.");
	int choice = bkMessageBox("Scenario Editor", buff, bkMsgYesNo, bkMsgQuestion);

	//Find the true min/max box ... 
	Vec3V haveMin = RCC_VEC3V(m_TransferVolMin), haveMax = RCC_VEC3V(m_TransferVolMax);
		
	Vec3V foundMin = Min(haveMin, haveMax);
	Vec3V foundMax = Max(haveMin, haveMax);

	spdAABB usedBox(foundMin, foundMax);

	//Currently NOT undoable
	CScenarioPointRegionEditInterface bankFromRegion(*fromRegion);
	CScenarioPointRegionEditInterface bankToRegion(*toRegion);
	const CScenarioChainingGraph& graph = bankFromRegion.GetChainingGraph();

	//Stats
	const int pcount = bankFromRegion.GetNumPoints() + bankFromRegion.GetTotalNumClusteredPoints() + bankFromRegion.GetNumEntityOverrides();
	const int ecount = graph.GetNumEdges();

	for (int i = 0; i < bankFromRegion.GetNumPoints(); i++)
	{
		CScenarioPoint& selPoint = bankFromRegion.GetPoint(i);
		if (selPoint.IsEditable())
		{
			if (usedBox.ContainsPoint(selPoint.GetPosition()))
			{
				if(selPoint.IsChained() && !choice)
					continue;

				if(selPoint.IsInCluster() && !choice) //safety ...
					continue;

				if (bankFromRegion.TransferPoint(selPoint, bankToRegion))
				{
					i = -1; //reset to beginning ... cause not sure where the nodes got pulled from if chained/clustered... 
				}
			}
		}
	}

	//If we did not say yes to allowing point clusters to be transfered then no need to run through those points.
	if(choice)
	{
		for (int c = 0; c < bankFromRegion.GetNumClusters(); c++)
		{
			for (int cp = 0; cp < bankFromRegion.GetNumClusteredPoints(c); cp++)
			{
				CScenarioPoint& selPoint = bankFromRegion.GetClusteredPoint(c, cp);
				if (selPoint.IsEditable())
				{
					if (usedBox.ContainsPoint(selPoint.GetPosition()))
					{
						if (bankFromRegion.TransferPoint(selPoint, bankToRegion))
						{
							// If the transfer was successful we no longer need to care about this cluster as 
							// all clustered points were transfered ... so just break out and go on to the next cluster
							break;
						}
					}
				}
			}
		}

		for (int eo = 0; eo < bankFromRegion.GetNumEntityOverrides(); eo++)
		{
			CScenarioEntityOverride& fromOverride = bankFromRegion.GetEntityOverride(eo);

			if (usedBox.ContainsPoint(fromOverride.m_EntityPosition))
			{
				CScenarioEntityOverride* toOverride = NULL;
				if (fromOverride.m_EntityCurrentlyAttachedTo)
				{
					//find or add one for the entity that is loaded ... 
					toOverride = &bankToRegion.FindOrAddOverrideForEntity(*fromOverride.m_EntityCurrentlyAttachedTo);
				}
				else
				{
					//add one at the end ... 
					toOverride = &bankToRegion.AddBlankEntityOverride(bankToRegion.GetNumEntityOverrides());
				}
				
				toOverride->m_EntityPosition = fromOverride.m_EntityPosition;
				toOverride->m_EntityType = fromOverride.m_EntityType;
				toOverride->m_ScenarioPoints = fromOverride.m_ScenarioPoints;
				toOverride->m_EntityMayNotAlwaysExist = fromOverride.m_EntityMayNotAlwaysExist;
				toOverride->m_SpecificallyPreventArtPoints = fromOverride.m_SpecificallyPreventArtPoints;

				//NON-parser data
				toOverride->m_ScenarioPointsInUse = fromOverride.m_ScenarioPointsInUse;
				toOverride->m_EntityCurrentlyAttachedTo = fromOverride.m_EntityCurrentlyAttachedTo;
				toOverride->m_VerificationStatus = fromOverride.m_VerificationStatus;

				//Remove the from override ... 
				bankFromRegion.DeleteEntityOverride(eo);
				eo--; //step back ... 
			}
		}
	}

	//reset selected
	ResetSelections();
	
	int tp = pcount - (bankFromRegion.GetNumPoints() + bankFromRegion.GetTotalNumClusteredPoints() + bankFromRegion.GetNumEntityOverrides());
	int te = ecount - graph.GetNumEdges();

	//If we transfered anything then we need to mark the regions as edited.
	if (tp || te)
	{
		SCENARIOPOINTMGR.FlagRegionAsEdited(m_ComboRegion-1);
		SCENARIOPOINTMGR.FlagRegionAsEdited(m_TransferRegion-1);
	}

	Displayf("Transfer (%d) point(s), (%d) chain edge(s) from region (%s) to region (%s)", tp, te, SCENARIOPOINTMGR.GetRegionName(m_ComboRegion-1), SCENARIOPOINTMGR.GetRegionName(m_TransferRegion-1));
	SCENARIOPOINTMGR.UpdateRegionAABB(m_ComboRegion-1);
	SCENARIOPOINTMGR.UpdateRegionAABB(m_TransferRegion-1);
	UpdateEditingPointWidgets();

	ClearAllPedTaskCachedPoints();
}


void CScenarioEditor::ToggleAnimDebugPressed()
{
	//Get the selected entity ... 
	CEntity* pEntity = CDebugScene::FocusEntities_Get(0);

	CPlayerInfo* pinfo = CGameWorld::GetMainPlayerInfo();
	Assert(pinfo);
	pEntity = pinfo->GetPlayerPed();

	if (pEntity)
	{
		if (pEntity->GetIsTypePed())
		{
			CPed* pPed = static_cast<CPed*>(pEntity);
			CTask* task = pPed->GetPedIntelligence()->GetTaskAtPriority(PED_TASK_PRIORITY_PRIMARY);
			if (task)
			{
				if (task->GetTaskType() == CTaskTypes::TASK_SCENARIO_ANIM_DEBUG)
				{
					//Toggle it off
					pPed->GetPedIntelligence()->AddTaskAtPriority(rage_new CTaskDoNothing(1), PED_TASK_PRIORITY_PRIMARY , true);
				}
				else
				{
					//Toggle it on
					pPed->GetPedIntelligence()->AddTaskAtPriority(rage_new CTaskScenarioAnimDebug(m_ScenarioAnimDebugHelper), PED_TASK_PRIORITY_PRIMARY , true);
				}
			}
			else
			{
				//Toggle it on if no primary task
				pPed->GetPedIntelligence()->AddTaskAtPriority(rage_new CTaskScenarioAnimDebug(m_ScenarioAnimDebugHelper), PED_TASK_PRIORITY_PRIMARY , true);
			}
		}
	}
}


void CScenarioEditor::DisableSpawningPressed()
{
	CScenarioPointRegion* region = GetCurrentRegion();
	if(!region)
	{
		return;
	}

	m_UndoHistory.StartUndoStep();
	m_SelectedEditingPoints.ForAll(CScenarioEditor::DisableSpawningForSelectedEntityPoints, NULL);
	m_UndoHistory.EndUndoStep();

	ResetSelections();
	UpdateEditingPointWidgets();

	SCENARIOPOINTMGR.FlagRegionAsEdited(m_CurrentRegion - 1);
	SCENARIOPOINTMGR.UpdateRegionAABB(m_CurrentRegion - 1);
}


void CScenarioEditor::RemoveOverridesOnCurrentPropPressed()
{
	CEntity* pEntity = m_SelectedEntityWithOverride;
	if(!pEntity || (!pEntity->GetIsTypeObject() && !pEntity->GetIsTypeBuilding()))
	{
		Displayf("No valid entity selected.");
		return;
	}

	CScenarioPointRegion* pRegion = GetCurrentRegion();
	if(!pRegion)
	{
		Displayf("Region not selected or not streamed in.");
		return;
	}

	DoRemoveOverrideOnEntity(*pEntity, *pRegion, true);

	CScenarioPointUndoItem undoItem;
	undoItem.SetEntity(SPU_RemoveOverridesOnEntity, pEntity);
	PushUndoItem(undoItem, true, false);

	ResetSelections();

	UpdateEditingPointWidgets();

	SCENARIOPOINTMGR.UpdateRegionAABB(m_CurrentRegion - 1);
}

void CScenarioEditor::RemoveOverridesWithNoPropPressed()
{
	if (m_SelectedOverrideWithNoPropIndex < 0)
	{
		Displayf("No selected override with no prop!");
		return;
	}
	
	CScenarioPointRegion* pRegion = GetCurrentRegion();
	if(!pRegion)
	{
		Displayf("Region not selected or not streamed in.");
		return;
	}
	
	CScenarioPointRegionEditInterface bankRegion(*pRegion);
	bankRegion.DeleteEntityOverride(m_SelectedOverrideWithNoPropIndex);

	ResetSelections();
	UpdateEditingPointWidgets();
	SCENARIOPOINTMGR.UpdateRegionAABB(m_CurrentRegion - 1);
	m_UndoHistory.Clear();
	m_RedoHistory.Clear();
}


void CScenarioEditor::AddPropScenarioOverridePressed()
{
	if (!m_SelectedEditingPoints.HasSelection() || m_SelectedEditingPoints.IsMultiSelecting())
	{
		return; //Operation not allowed if nothing selected or multi selections
	}

	EditorPoint onlyPoint = m_SelectedEditingPoints.GetOnlyPoint();

	if(!onlyPoint.IsValid() || onlyPoint.IsWorldPoint())
	{
		return;
	}

	CScenarioPointRegion* region = GetCurrentRegion();
	if(!region)
	{
		return;
	}
	CScenarioPointRegionEditInterface bankRegion(*region);
	CScenarioPoint& propPoint1 = SCENARIOPOINTMGR.GetEntityPoint(onlyPoint.GetEntityPointIndex());

	CEntity* pEntity = propPoint1.GetEntity();
	if(!pEntity)
	{
		return;
	}

	Assert(!propPoint1.IsEntityOverridePoint());

	static const int kMaxAttachedPoints = 16;
	CExtensionDefSpawnPoint newPoints[kMaxAttachedPoints];
	Vec3V pointLocalPos[kMaxAttachedPoints];
	float pointLocalDir[kMaxAttachedPoints];
	int numAttachedPoints = 0;
	int selectedAttachedPointIndex = -1;

	int numEntityPoints = SCENARIOPOINTMGR.GetNumEntityPoints();
	for(int entityPointIndex = 0; entityPointIndex < numEntityPoints; entityPointIndex++)
	{
		const CScenarioPoint& attachedPoint = SCENARIOPOINTMGR.GetEntityPoint(entityPointIndex);
		if(attachedPoint.GetEntity() != pEntity)	// Not attached to this entity.
		{
			continue;
		}

		if(!Verifyf(numAttachedPoints < kMaxAttachedPoints, "Too many attached points on this prop, max %d supported.", kMaxAttachedPoints))
		{
			break;
		}


		if(&attachedPoint == &propPoint1)
		{
			selectedAttachedPointIndex = numAttachedPoints;
		}

		CExtensionDefSpawnPoint& newPoint = newPoints[numAttachedPoints];
		attachedPoint.CopyToDefinition(&newPoint);

		pointLocalPos[numAttachedPoints] = attachedPoint.GetLocalPosition();
		pointLocalDir[numAttachedPoints] = attachedPoint.GetLocalDirectionf();

		newPoint.m_offsetPosition = VEC3V_TO_VECTOR3(attachedPoint.GetWorldPosition());

		const Vec3V worldFwdDirV = attachedPoint.GetWorldDirectionFwd();
		const float x = worldFwdDirV.GetXf();
		const float y = worldFwdDirV.GetYf();
		float dir = 0.0f;
		if(x*x + y*y > square(0.01f))
		{
			dir = rage::Atan2f(-x, y);
		}

		Quaternion q;
		q.FromRotation(ZAXIS, dir);
		newPoint.m_offsetRotation.Set(q.xyzw);

		numAttachedPoints++;
	}

	if(!numAttachedPoints)
	{
		return;
	}

	ResetSelections();

	m_UndoHistory.StartUndoStep();

	for(int i = 0; i < numAttachedPoints; i++)
	{
		const CExtensionDefSpawnPoint& newPoint = newPoints[i];

		int newPointIndex = bankRegion.AddPoint(newPoint);
		CScenarioPoint& newSP = bankRegion.GetPoint(newPointIndex);
		newSP.MarkAsFromRsc(); //points added to the region are RSC points.

		PushUndoAction(SPU_InsertPoint, newPointIndex, true);

		CScenarioEntityOverride& override = bankRegion.FindOrAddOverrideForEntity(*pEntity);

		const int overrideIndex = bankRegion.GetIndexForEntityOverride(override);
		const int indexWithinOverride = override.m_ScenarioPoints.GetCount();
		DoAttachPoint(newPointIndex, *region, *pEntity, overrideIndex, indexWithinOverride);

		EditorPoint tempEditorPoint;
		tempEditorPoint.SetEntityPointForOverride(overrideIndex, indexWithinOverride);
		CScenarioPointUndoItem undoItem;
		undoItem.SetPoint(SPU_AttachPoint, (int)tempEditorPoint.GetPointId(), newPoint);
		undoItem.SetPointIdBeforeOperation(newPointIndex);

		PushUndoItem(undoItem, true, false);

		CScenarioPoint* pt = override.m_ScenarioPointsInUse[indexWithinOverride];
		if(pt)
		{
			Assert(pt->GetEntity() == pEntity);
			pt->SetLocalPosition(pointLocalPos[i]);
			pt->SetLocalDirection(pointLocalDir[i]);
		}

		if(selectedAttachedPointIndex == i)
		{
			EditorPoint newSelect;
			newSelect.SetEntityPointForOverride(overrideIndex, indexWithinOverride);
			m_SelectedEditingPoints.Select(newSelect);
		}

	}

	m_UndoHistory.EndUndoStep();

	UpdateEditingPointWidgets();

	SCENARIOPOINTMGR.FlagRegionAsEdited(m_CurrentRegion - 1);
	SCENARIOPOINTMGR.UpdateRegionAABB(m_CurrentRegion - 1);
}


void CScenarioEditor::AttachToEntityPressed()
{
	if (!m_SelectedEditingPoints.HasSelection() || m_SelectedEditingPoints.IsMultiSelecting())
	{
		return; //Operation not allowed if nothing selected or multi selections
	}

	EditorPoint onlyPoint = m_SelectedEditingPoints.GetOnlyPoint();

	if(!onlyPoint.IsValid() || !onlyPoint.IsWorldPoint())
	{
		return;
	}

	CScenarioPointRegion* region = GetCurrentRegion();
	if(!region)
	{
		return;
	}
	CScenarioPointRegionEditInterface bankRegion(*region);

	CScenarioPoint& point = onlyPoint.GetScenarioPoint(region);

	CEntity* pAttachEntity = FindEntityToAttachTo(point);
	if(!pAttachEntity)
	{
		Displayf("Didn't find any entity close enough to attach to.");
		return;
	}

	// TODO: think more about uprooted objects.
	if(pAttachEntity->GetIsTypeObject())
	{
		Assert(!static_cast<CObject*>(pAttachEntity)->m_nObjectFlags.bHasBeenUprooted);
	}

	const int pointIndexInregion = onlyPoint.GetWorldPointIndex();
	const u32 pointIdBefore = onlyPoint.GetPointId();

	CScenarioEntityOverride& override = bankRegion.FindOrAddOverrideForEntity(*pAttachEntity);
	const int overrideIndex = bankRegion.GetIndexForEntityOverride(override);
	const int indexWithinOverride = override.m_ScenarioPoints.GetCount();

	CExtensionDefSpawnPoint copyOfPointDataForUndo;
	point.CopyToDefinition(&copyOfPointDataForUndo);

	DoAttachPoint(pointIndexInregion, *region, *pAttachEntity, overrideIndex, indexWithinOverride);

	SCENARIOPOINTMGR.FlagRegionAsEdited(m_CurrentRegion - 1);

	ResetSelections();

	EditorPoint newSelect;
	newSelect.SetEntityPointForOverride(overrideIndex, indexWithinOverride);
	if(newSelect.IsValid())
	{
		CScenarioPointUndoItem undoItem;
		undoItem.SetPoint(SPU_AttachPoint, (int)newSelect.GetPointId(), copyOfPointDataForUndo);
		undoItem.SetPointIdBeforeOperation(pointIdBefore);

		PushUndoItem(undoItem, true, false);
	}

	m_SelectedEditingPoints.Select(newSelect);

	ClearAllPedTaskCachedPoints();
	UpdateEditingPointWidgets();

	SCENARIOPOINTMGR.UpdateRegionAABB(m_CurrentRegion - 1);
}


void CScenarioEditor::DetachFromEntityPressed()
{
	if (!m_SelectedEditingPoints.HasSelection() || m_SelectedEditingPoints.IsMultiSelecting())
	{
		return; //Operation not allowed if nothing selected or multi selections
	}

	EditorPoint onlyPoint = m_SelectedEditingPoints.GetOnlyPoint();

	if(!onlyPoint.IsValid() || onlyPoint.IsWorldPoint())
	{
		return;
	}

	CScenarioPointRegion* region = GetCurrentRegion();
	if(!region)
	{
		return;
	}

	CScenarioPointRegionEditInterface bankRegion(*region);

	CScenarioPoint& pt = onlyPoint.GetScenarioPoint(region);

	const u32 pointIdBefore = onlyPoint.GetPointId();

	CEntity* pAttachEntity = pt.GetEntity();
	Assert(pAttachEntity);

	CExtensionDefSpawnPoint copyOfPointDataForUndo;
	pt.CopyToDefinition(&copyOfPointDataForUndo);

	const int indexInRegionAfterOperation = bankRegion.GetNumPoints();
	DoDetachPoint(pt, *region, indexInRegionAfterOperation);

	CScenarioPointUndoItem undoItem;
	undoItem.SetPoint(SPU_DetachPoint, indexInRegionAfterOperation, copyOfPointDataForUndo);
	undoItem.SetPointIdBeforeOperation(pointIdBefore);
	undoItem.SetAttachEntity(pAttachEntity);
	PushUndoItem(undoItem, true, false);

	SCENARIOPOINTMGR.FlagRegionAsEdited(m_CurrentRegion - 1);

	ClearAllPedTaskCachedPoints();

	ResetSelections();

	EditorPoint newSelect;
	newSelect.SetWorldPoint(indexInRegionAfterOperation);
	m_SliderPoint = indexInRegionAfterOperation;
	m_SelectedEditingPoints.Select(newSelect);

	UpdateEditingPointWidgets();

	SCENARIOPOINTMGR.UpdateRegionAABB(m_CurrentRegion - 1);
}


int CScenarioEditor::FindSelectedChainIndex() const
{
	if ((!m_SelectedEditingPoints.HasSelection() && m_CurrentChainingEdge < 0) || m_SelectedEditingPoints.IsMultiSelecting())
	{
		return -1; //Operation not allowed if nothing selected or multi selections
	}

	const CScenarioPointRegion* region = GetCurrentRegion();

	if (Verifyf(region, "Trying to operate on a region that is not valid"))
	{
		CScenarioPointRegionEditInterface bankRegion(*region);
		const CScenarioChainingGraph& graph = bankRegion.GetChainingGraph();

		EditorPoint onlyPoint = m_SelectedEditingPoints.GetOnlyPoint();
		if (onlyPoint.IsValid())
		{
			const CScenarioPoint& rPoint = onlyPoint.GetScenarioPoint(region);
			if (Verifyf(rPoint.IsChained(),"Trying to change the max users for an unchained point."))
			{
				const int nodeIndex = graph.FindChainingNodeIndexForScenarioPoint(rPoint);
				const int chainIndex = graph.GetNodesChainIndex(nodeIndex);
				return chainIndex;
			}
		}
		else if (m_CurrentChainingEdge >= 0)
		{
			const CScenarioChainingEdge& edge = bankRegion.GetChainingGraph().GetEdge(m_CurrentChainingEdge);
			const int chainIndex = graph.GetNodesChainIndex(edge.GetNodeIndexFrom());
			return chainIndex;
		}
	}

	return -1;
}


void CScenarioEditor::ChainUsersChanged()
{
	const int chainIndex = FindSelectedChainIndex();
	if(chainIndex < 0)
	{
		return;
	}

	CScenarioPointRegion* region = GetCurrentRegion();
	CScenarioPointRegionEditInterface bankRegion(*region);
	bankRegion.UpdateChainMaxUsers(chainIndex, m_MaxChainUsers);
	SCENARIOPOINTMGR.FlagRegionAsEdited(m_CurrentRegion - 1);
}


void CScenarioEditor::CreateClusterPressed()
{
	//Displayf("******************************************* CreateClusterPressed");
	Assert(m_SelectedEditingPoints.HasSelection());
	CScenarioPointRegion* region = GetCurrentRegion();
	Assert(region);
	CScenarioPointRegionEditInterface bankRegion(*region);

	const int newCluster = bankRegion.AddNewCluster();
	m_SelectedEditingPoints.ForAll(CScenarioEditor::AddToCluster, (void*)&newCluster);

	SCENARIOPOINTMGR.UpdateRegionAABB(m_CurrentRegion - 1);
	SCENARIOPOINTMGR.FlagRegionAsEdited(m_CurrentRegion - 1);
	bankRegion.UpdateChainingGraphMappings();
	UpdateEditingPointWidgets();
}

void CScenarioEditor::DeleteClusterPressed()
{
	//Displayf("******************************************* DeleteClusterPressed");
	Assert(m_SelectedEditingPoints.HasSelection());
	CScenarioPointRegion* region = GetCurrentRegion();
	Assert(region);
	CScenarioPointRegionEditInterface bankRegion(*region);

	atArray<int> foundIds;
	m_SelectedEditingPoints.ForAll(CScenarioEditor::GatherSelectedClusters, &foundIds);

	for (int c = 0; c < foundIds.GetCount(); c++)
	{
		const int toDelete = foundIds[c];
		bankRegion.DeleteCluster(toDelete);
		foundIds.DeleteFast(c);
		c--;//step back ... 

		//because we dont know what cluster we are trying to remove we need to adjust the found ids.
		for (int sub = 0; sub < foundIds.GetCount(); sub++)
		{
			foundIds[sub] = (foundIds[sub] > toDelete)? foundIds[sub]-1 : foundIds[sub];
		}
	}

	SCENARIOPOINTMGR.FlagRegionAsEdited(m_CurrentRegion - 1);
	bankRegion.PurgeInvalidClusters();//remove any bad clusters.
	SCENARIOPOINTMGR.UpdateRegionAABB(m_CurrentRegion - 1);
	ResetSelections();
	bankRegion.UpdateChainingGraphMappings();
	UpdateEditingPointWidgets();
}

void CScenarioEditor::RemoveFromClusterPressed()
{
	//Displayf("******************************************* RemoveFromClusterPressed");
	Assert(m_SelectedEditingPoints.HasSelection());

	m_SelectedEditingPoints.ForAll(CScenarioEditor::RemoveFromCluster, NULL);
	
	CScenarioPointRegion* region = GetCurrentRegion();
	Assert(region);
	CScenarioPointRegionEditInterface bankRegion(*region);
	SCENARIOPOINTMGR.FlagRegionAsEdited(m_CurrentRegion - 1);
	bankRegion.PurgeInvalidClusters();//remove any bad clusters.
	SCENARIOPOINTMGR.UpdateRegionAABB(m_CurrentRegion - 1);
	bankRegion.UpdateChainingGraphMappings();
	UpdateEditingPointWidgets();
}

void CScenarioEditor::MergeUnclusteredPressed()
{
	//Displayf("******************************************* MergeUnclusteredPressed");
	Assert(m_SelectedEditingPoints.HasSelection());

	atArray<int> foundIds;
	m_SelectedEditingPoints.ForAll(CScenarioEditor::GatherSelectedClusters, &foundIds);

	if (Verifyf(foundIds.GetCount() == 1, "Add Unclustered assumes only one cluster was found for the selected points. %d were found. Skipping operation.", foundIds.GetCount()))
	{
		const int clusterID = foundIds[0];
		m_SelectedEditingPoints.ForAll(CScenarioEditor::AddToCluster, (void*)&clusterID);
	}

	CScenarioPointRegion* region = GetCurrentRegion();
	Assert(region);
	CScenarioPointRegionEditInterface bankRegion(*region);
	SCENARIOPOINTMGR.FlagRegionAsEdited(m_CurrentRegion - 1);
	bankRegion.PurgeInvalidClusters();//remove any bad clusters.
	SCENARIOPOINTMGR.UpdateRegionAABB(m_CurrentRegion - 1);
	bankRegion.UpdateChainingGraphMappings();
	UpdateEditingPointWidgets();
}

void CScenarioEditor::MergeToClusterPressed()
{
	//Displayf("******************************************* MergeToClusterPressed");
	Assert(m_SelectedEditingPoints.HasSelection());
	Assert(m_iMergeToCluster >= 0);
	Assert(m_iMergeToCluster < m_NumFoundClusterIdNames);

	const int clusterID = atoi(m_FoundClusterIdNames[m_iMergeToCluster]);
	m_SelectedEditingPoints.ForAll(CScenarioEditor::AddToCluster, (void*)&clusterID);

	CScenarioPointRegion* region = GetCurrentRegion();
	Assert(region);
	CScenarioPointRegionEditInterface bankRegion(*region);
	SCENARIOPOINTMGR.FlagRegionAsEdited(m_CurrentRegion - 1);
	bankRegion.PurgeInvalidClusters();//remove any bad clusters.
	SCENARIOPOINTMGR.UpdateRegionAABB(m_CurrentRegion - 1);
	bankRegion.UpdateChainingGraphMappings();
	UpdateEditingPointWidgets();
}


void CScenarioEditor::ReduceNumCarGensOnChainPressed()
{
	const int chainIndex = FindSelectedChainIndex();
	if(chainIndex < 0)
	{
		return;
	}

	CScenarioPointRegion* region = GetCurrentRegion();
	CScenarioPointRegionEditInterface regionInterface(*region);

	const CScenarioChain& chain = regionInterface.GetChainingGraph().GetChain(chainIndex);
	atArray<int> nodes;
	regionInterface.FindCarGensOnChain(chain, nodes);
	const int numBefore = nodes.GetCount();

	int numToRemove = nodes.GetCount()/2;
	while(numToRemove > 0 && nodes.GetCount() > 0)
	{
		const int randomIndex = fwRandom::GetRandomNumberInRange(0, nodes.GetCount());
		const int nodeIndex = nodes[randomIndex];
		nodes.DeleteFast(randomIndex);

		CScenarioPoint* pt = regionInterface.GetChainingGraph().GetChainingNodeScenarioPoint(nodeIndex);
		if(pt && pt->IsHighPriority())
		{
			// Leave high priority points alone is probably safest.
			continue;
		}

		if(AssertVerify(pt))
		{
			Assert(!pt->IsFlagSet(CScenarioPointFlags::NoSpawn));

			bool needToCallOnAdd = false;
			BeforeScenarioPointDataUpdate(*pt, needToCallOnAdd);

			pt->SetFlag(CScenarioPointFlags::NoSpawn, true);

			AfterScenarioPointDataUpdate(*region, *pt, needToCallOnAdd);
		}
		numToRemove--;
	}

	Displayf("Reduced number of car generators on chain from %d to %d.", numBefore, regionInterface.CountCarGensOnChain(chain));

	SCENARIOPOINTMGR.FlagRegionAsEdited(m_CurrentRegion - 1);
}


CEntity* CScenarioEditor::FindEntityToAttachTo(const CScenarioPoint& pt)
{
	const Vec3V offsStartV(0.0f, 0.0f, 0.2f);
	const Vec3V offsEndV(0.0f, 0.0f, -0.5f);

	const Vec3V posV = pt.GetWorldPosition();
	const Vec3V segStartV = Add(posV, offsStartV);
	const Vec3V segEndV = Add(posV, offsEndV);

	// First, do a vertical probe to see if the point is placed right on top of something. If so,
	// that's probably what we should attach to.
	CEntity* pHitEntity = NULL;
	WorldProbe::CShapeTestProbeDesc probeDesc;
	WorldProbe::CShapeTestFixedResults<> probeResult;
	probeDesc.SetResultsStructure(&probeResult);
	probeDesc.SetStartAndEnd(RCC_VECTOR3(segStartV), RCC_VECTOR3(segEndV));
	probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES | ArchetypeFlags::GTA_OBJECT_TYPE);
	if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
	{
		phInst* pInst = probeResult[0].GetHitInst();
		if(pInst)
		{
			pHitEntity = CPhysics::GetEntityFromInst(pInst);
		}
	}

	if(pHitEntity && IsEntityValidForAttachment(*pHitEntity))
	{
		return pHitEntity;
	}

	// Search for the closest CObject, up to a maximum range.
	const ScalarV maxDistSqV(square(5.0f));
	ScalarV closestDistSqV = maxDistSqV;
	CEntity* pClosestEntity = NULL;
	const Vec3V refPosV = pt.GetPosition();
	const int numObjs = (int) CObject::GetPool()->GetSize();
	for(int i = 0; i < numObjs; i++)
	{
		CObject* pObj = CObject::GetPool()->GetSlot(i);
		if(!pObj || !IsEntityValidForAttachment(*pObj))
		{
			continue;
		}

		const Vec3V objPosV = pObj->GetTransform().Transform(Average(VECTOR3_TO_VEC3V(pObj->GetBoundingBoxMin()), VECTOR3_TO_VEC3V(pObj->GetBoundingBoxMax())));
		const ScalarV distSqV = DistSquared(refPosV, objPosV);
		if(IsLessThanAll(distSqV, closestDistSqV))
		{
			pClosestEntity = pObj;
			closestDistSqV = distSqV;
		}
	}

	// Search for the closest CBuilding, if closer than the closest CObject.
	const int numBuildings = CBuilding::GetPool()->GetSize();
	for(int i = 0; i < numBuildings; i++)
	{
		CBuilding* pBuilding = CBuilding::GetPool()->GetSlot(i);
		if(!pBuilding || !IsEntityValidForAttachment(*pBuilding))
		{
			continue;
		}

		const Vec3V objPosV = pBuilding->GetTransform().Transform(Average(VECTOR3_TO_VEC3V(pBuilding->GetBoundingBoxMin()), VECTOR3_TO_VEC3V(pBuilding->GetBoundingBoxMax())));
		const ScalarV distSqV = DistSquared(refPosV, objPosV);
		if(IsLessThanAll(distSqV, closestDistSqV))
		{
			pClosestEntity = pBuilding;
			closestDistSqV = distSqV;
		}
	}

	return pClosestEntity;
}


void CScenarioEditor::DoAttachPoint(int pointIndexInRegion, CScenarioPointRegion& region, CEntity& attachEntity,
		int overrideIndex, int indexWithinOverride)
{
	CScenarioPointRegionEditInterface bankRegion(region);
	CScenarioPoint* pPoint = &bankRegion.GetPoint(pointIndexInRegion);

	if(!Verifyf(!pPoint->IsEntityOverridePoint(), "Already seems to be an override point, somehow."))
	{
		return;
	}
	if(!Verifyf(!pPoint->GetEntity(), "Already seems to be attached."))
	{
		return;
	}

	CScenarioPoint* pNewPoint = CScenarioPoint::CreateFromPool();
	if(!pNewPoint)
	{
		return;
	}

	const CScenarioEntityOverride* pFoundOverride = bankRegion.FindOverrideForEntity(attachEntity);
	if(pFoundOverride)
	{
		Assert(bankRegion.GetIndexForEntityOverride(*pFoundOverride) == overrideIndex);
	}
	else
	{
		bankRegion.InsertOverrideForEntity(overrideIndex, attachEntity);
	}

	pPoint = bankRegion.RemovePoint(pointIndexInRegion, false /* don't delete chains */);

	*pNewPoint = *pPoint;
	CScenarioPoint::DestroyForEditor(*pPoint);
	pPoint = pNewPoint;

	const Vec3V worldPosV = pPoint->GetPosition();
	const Vec3V worldFwdDirV = pPoint->GetDirection();

	pPoint->SetEntity(&attachEntity);

	const float x = worldFwdDirV.GetXf();
	const float y = worldFwdDirV.GetYf();
	float dir = 0.0f;
	if(x*x + y*y > square(0.01f))
	{
		dir = rage::Atan2f(-x, y);
	}

	pPoint->SetWorldPosition(worldPosV);
	pPoint->SetWorldDirection(dir);

	CExtensionDefSpawnPoint pointData;
	pPoint->CopyToDefinition(&pointData);

	CScenarioEntityOverride& override = bankRegion.GetEntityOverride(overrideIndex);

	if(!override.m_ScenarioPoints.GetCount())
	{
		SCENARIOPOINTMGR.DeletePointsWithAttachedEntity(&attachEntity);
	}

	// Hacky way of making the arrays large enough...
	override.m_ScenarioPoints.Grow();
	override.m_ScenarioPoints.Pop();
	override.m_ScenarioPointsInUse.Grow();
	override.m_ScenarioPointsInUse.Pop();

	override.m_ScenarioPoints.Insert(indexWithinOverride) = pointData;
	override.m_ScenarioPointsInUse.Insert(indexWithinOverride) = pPoint;

	pPoint->SetIsEntityOverridePoint(true);
	SCENARIOPOINTMGR.AddEntityPoint(*pPoint);

	attachEntity.m_nFlags.bHasSpawnPoints = true;
	fwBaseEntityContainer* pAttachEntityContainer = attachEntity.GetOwnerEntityContainer();
	if(pAttachEntityContainer && pAttachEntityContainer->GetOwnerSceneNode())
	{
		pAttachEntityContainer->GetOwnerSceneNode()->SetContainsSpawnPoints(true);
	}

	AfterScenarioPointDataUpdate(region, *pPoint, false);
}


void CScenarioEditor::DoDetachPoint(CScenarioPoint& origPt, CScenarioPointRegion& region, int insertIndex)
{
	bool found = false;
	CScenarioPointRegionEditInterface bankRegion(region);
	const int numOverrides = region.GetNumEntityOverrides();
	for(int i = 0; i < numOverrides && !found; i++)
	{
		CScenarioEntityOverride& override = bankRegion.GetEntityOverride(i);
		for(int j = 0; j < override.m_ScenarioPointsInUse.GetCount(); j++)
		{
			if(override.m_ScenarioPointsInUse[j] == &origPt)
			{
				override.m_ScenarioPoints.Delete(j);
				override.m_ScenarioPointsInUse.Delete(j);
				found = true;

				if(override.m_ScenarioPoints.GetCount() == 0 && !override.m_SpecificallyPreventArtPoints)
				{
					bankRegion.DeleteEntityOverride(i);

					// Now when the override is gone, it would be expected that the entity
					// has its regular art-attached points, so we add those back here.
					CEntity* pEntity = override.m_EntityCurrentlyAttachedTo;
					if(pEntity)
					{
						CGameWorld::AddScenarioPointsForEntity(*pEntity, false);
					}

#if SCENARIO_VERIFY_ENTITY_OVERRIDES
					CScenarioEntityOverrideVerification::GetInstance().ClearInfoForRegion(region);
#endif	// SCENARIO_VERIFY_ENTITY_OVERRIDES
				}
				break;
			}
		}
	}

	if(!Verifyf(found, "Failed to find point override, unable to detach safely."))
	{
		return;
	}

	found = false;
	const int numEntityPts = SCENARIOPOINTMGR.GetNumEntityPoints();
	for(int i = 0; i < numEntityPts; i++)
	{
		if(&SCENARIOPOINTMGR.GetEntityPoint(i) == &origPt)
		{
			SCENARIOPOINTMGR.RemoveEntityPoint(i);
			found = true;
			break;
		}
	}
	Assertf(found, "Failed to find entity point in scenario point manager.");

	CScenarioPoint* pNewPoint = CScenarioPoint::CreateForEditor();
	*pNewPoint = origPt;
	CScenarioPoint::DestroyFromPool(origPt);

	const Vec3V posV = pNewPoint->GetPosition();
	const Vec3V dirV = pNewPoint->GetDirection();
	pNewPoint->SetEntity(NULL);

	pNewPoint->SetPosition(posV);

	const float x = dirV.GetXf();
	const float y = dirV.GetYf();
	float dir = 0.0f;
	if(x*x + y*y > square(0.01f))
	{
		dir = rage::Atan2f(-x, y);
	}
	pNewPoint->SetDirection(dir);

	Assert(pNewPoint->IsEntityOverridePoint());
	pNewPoint->SetIsEntityOverridePoint(false);

	bankRegion.InsertPoint(insertIndex, *pNewPoint);

	AfterScenarioPointDataUpdate(region, *pNewPoint, false);
}


void CScenarioEditor::DoDisableSpawningOnEntity(CEntity& entity, CScenarioPointRegion& region)
{
	CScenarioPointRegionEditInterface bankRegion(region);
	CScenarioEntityOverride& override = bankRegion.FindOrAddOverrideForEntity(entity);
#if __ASSERT
	Assert(!override.m_ScenarioPoints.GetCount());
#endif
	override.m_SpecificallyPreventArtPoints = true;			// Make sure we don't remove this even if it doesn't add any points.

	Verifyf(region.ApplyOverridesForEntity(entity, true), "Something probably wrong with the override.");
}


void CScenarioEditor::DoRemoveOverrideOnEntity(CEntity& entity, CScenarioPointRegion& region, bool expectDisabledSpawnOnly)
{
	CScenarioPointRegionEditInterface bankRegion(region);
	CScenarioEntityOverride* pOverride = bankRegion.FindOverrideForEntity(entity);
	if(!Verifyf(pOverride, "Override not found."))
	{
		return;
	}

	if(expectDisabledSpawnOnly)
	{
		if(!Verifyf(pOverride->m_ScenarioPoints.GetCount() == 0, "Override still has %d points, for some reason.", pOverride->m_ScenarioPoints.GetCount()))
		{
			return;
		}

		Assert(pOverride->m_SpecificallyPreventArtPoints);
	}

	const int overrideIndex = bankRegion.GetIndexForEntityOverride(*pOverride);
	bankRegion.DeleteEntityOverride(overrideIndex);

	SCENARIOPOINTMGR.DeletePointsWithAttachedEntity(&entity);

	CGameWorld::AddScenarioPointsForEntity(entity, false);

#if SCENARIO_VERIFY_ENTITY_OVERRIDES
	CScenarioEntityOverrideVerification::GetInstance().ClearInfoForRegion(region);
#endif	// SCENARIO_VERIFY_ENTITY_OVERRIDES
}


void CScenarioEditor::DoRemoveGroup(int groupIndex)
{
	const u32 removedHash = SCENARIOPOINTMGR.GetGroupByIndex(groupIndex).GetNameHash();
	SCENARIOPOINTMGR.ResetGroupForUsersOf(removedHash);
	SCENARIOPOINTMGR.DeleteGroupByIndex(groupIndex);

	if(m_CurrentScenarioGroup > SCENARIOPOINTMGR.GetNumGroups())
	{
		m_CurrentScenarioGroup = SCENARIOPOINTMGR.GetNumGroups();
	}

	UpdateScenarioGroupNameWidgets();
	UpdateCurrentScenarioGroupWidgets();
	UpdateEditingPointWidgets();
}


void CScenarioEditor::DoRenameScenarioGroup(int groupIndex, const char* name)
{
	//Add a new group to the list ... 
	SCENARIOPOINTMGR.AddGroup(name);
	const int newGroupId = SCENARIOPOINTMGR.FindGroupByNameHash(name);
	const int oldGroupId = groupIndex + 1;

	//NOTE: this rename loads regions that are currently streamed out 
	//	so need to make sure the name exists so loading is done properly.
	SCENARIOPOINTMGR.RenameGroup(oldGroupId, newGroupId); // point everything to the new group

	SCENARIOPOINTMGR.DeleteGroupByIndex(groupIndex);//Delete the old group ... 

	UpdateScenarioGroupNameWidgets();
	UpdateCurrentScenarioGroupWidgets();
	UpdateEditingPointWidgets();

	// Unlike if we remove or add groups, we need to manually
	// make sure we update the widgets for enabled groups, outside
	// of the editor.
	SCENARIOPOINTMGR.UpdateScenarioGroupWidgets();
}

void CScenarioEditor::DoRemoveRegion(int regionIndex)
{
	SCENARIOPOINTMGR.DeleteRegionByIndex(regionIndex);

	m_CurrentRegion = -1;//reset so we rebuild widgets ...

	if(m_ComboRegion > SCENARIOPOINTMGR.GetNumRegions())
	{
		m_ComboRegion = SCENARIOPOINTMGR.GetNumRegions();
	}

	if(m_TransferRegion > SCENARIOPOINTMGR.GetNumRegions())
	{
		m_TransferRegion = SCENARIOPOINTMGR.GetNumRegions();
	}

	UpdateRegionNameWidgets();
	UpdateCurrentScenarioRegionWidgets();
}

void CScenarioEditor::DoRenameRegion(int regionIndex, const char* name)
{
	SCENARIOPOINTMGR.RenameRegionByIndex(regionIndex, name);

	UpdateRegionNameWidgets();
	UpdateCurrentScenarioRegionWidgets();
}


// bool CScenarioEditor::ValidateEditingPoint(const CScenarioPointRegion& region)
// {
// 	if(!m_iEditingPoint.IsValid())
// 	{
// 		m_iEditingPoint.Reset();
// 		m_SliderPoint = -1;
// 		return false;
// 	}
// 	if( m_iEditingPoint.IsWorldPoint() )
// 	{
// 		const int numSpawnPoints = region.GetNumPoints();
// 		if (numSpawnPoints <= 0)
// 		{
// 			m_iEditingPoint.Reset();
// 			m_SliderPoint = -1;
// 			return false;
// 		}
// 		if (m_iEditingPoint.GetWorldPointIndex() >= numSpawnPoints)
// 		{
// 			m_iEditingPoint.SetWorldPoint(numSpawnPoints -1);
// 			m_SliderPoint = m_iEditingPoint.GetWorldPointIndex();
// 		}
// 		return true;
// 	}
// 	else
// 	{
// 		if(m_iEditingPoint.IsEntityOverridePoint())
// 		{
// 			const int overrideIndex = EditorPoint::GetOverrideIndex(m_iEditingPoint.GetPointId());
// 			const int indexWithinOverride = EditorPoint::GetIndexWithinOverride(m_iEditingPoint.GetPointId());
// 			if(overrideIndex >= 0 && overrideIndex < region.GetNumEntityOverrides())
// 			{
// 				const CScenarioEntityOverride& override = region.GetEntityOverride(overrideIndex);
// 				if(indexWithinOverride >= 0 && indexWithinOverride < override.m_ScenarioPointsInUse.GetCount())
// 				{
// 					CScenarioPoint* pt = override.m_ScenarioPointsInUse[indexWithinOverride];
// 					if(pt)
// 					{
// 						return true;
// 					}
// 				}
// 			}
// 
// 			m_iEditingPoint.Reset();
// 			m_SliderPoint = -1;
// 			return false;
// 		}
// 		else
// 		{
// 			const int index = m_iEditingPoint.GetEntityPointIndex();
// 			if(index < 0 || index >= SCENARIOPOINTMGR.GetNumEntityPoints())
// 			{
// 				m_iEditingPoint.Reset();
// 				m_SliderPoint = -1;
// 				return false;
// 			}
// 			// Not sure:
// 			return false;
// 		}
// 	}
// }

/*
PURPOSE
	Get either the currently selected scenario point, or the destination point of the currently
	selected edge.
*/
CScenarioPoint* CScenarioEditor::GetSelectedPointToChainFrom(CScenarioChainingEdge& edgeDataInOut)
{
	CScenarioPoint* fromPoint = NULL;

	CScenarioPointRegion* region = GetCurrentRegion();
	Assert(region);
	CScenarioPointRegionEditInterface bankRegion(*region);

	EditorPoint onlyPoint = m_SelectedEditingPoints.GetOnlyPoint();

	const CScenarioChainingGraph& graph = bankRegion.GetChainingGraph();
	if(onlyPoint.IsValid())
	{
		fromPoint = &onlyPoint.GetScenarioPoint(region);

		if(fromPoint)
		{
			const int numEdges = graph.GetNumEdges();
			for(int i = 0; i < numEdges; i++)
			{
				const CScenarioChainingEdge& edge = graph.GetEdge(i);
				const CScenarioPoint* edgeDest = graph.GetChainingNodeScenarioPoint(edge.GetNodeIndexTo());
				if(edgeDest == fromPoint)
				{
					// This edge leads to the selected point. Let's copy the data from this.
					// It's possible that the selected point has more than one edge leading to it,
					// in which case this just arbitrarily picks the first one - it will probably
					// be common for edges to contain the same data (speed, navigation mode) anyway,
					// if they connect to the same point.
					edgeDataInOut = edge;
					break;
				}
			}
		}
	}
	else if(m_CurrentChainingEdge >= 0)
	{
		const int fromNodeIndex = graph.GetEdge(m_CurrentChainingEdge).GetNodeIndexTo();
		fromPoint = graph.GetChainingNodeScenarioPoint(fromNodeIndex);

		edgeDataInOut = graph.GetEdge(m_CurrentChainingEdge);
	}

	return fromPoint;
}

int CScenarioEditor::FindCurrentRegion() const
{
	//Find the first one that contains the camera position.
	Vec3V vPos = VECTOR3_TO_VEC3V(camInterface::GetPos());
	
	//If we have a player then try to use that instead of the camera position.
	CPed* localPlayer = CGameWorld::FindLocalPlayer();
	if (localPlayer)
	{
		vPos = localPlayer->GetTransform().GetPosition();
	}

	int count = SCENARIOPOINTMGR.GetNumRegions();
	for (int r = 0; r < count; r++)
	{
		const spdAABB& aabb = SCENARIOPOINTMGR.GetRegionAABB(r);
		if (aabb.ContainsPoint(vPos))
		{
			return r+1; //add one because "No Region" is index 0
		}
	}

	//not in any region.
	if (count)
	{
		return 1;//just select the first in the list
	}

	return 0;//no region to select.
}

void CScenarioEditor::ScenarioPointDataUpdate(const Vec3V* position, const float* direction)
{
	CScenarioPointRegion* region = GetCurrentRegion();
	Assert(region);
	CScenarioPointRegionEditInterface bankRegion(*region);

	EditorPoint selected = m_SelectedEditingPoints.GetOnlyPoint();
	CScenarioPoint& rPointDef = selected.GetScenarioPoint(region);

	bool needToCallOnAdd = false;
	BeforeScenarioPointDataUpdate(rPointDef, needToCallOnAdd);

	bool updateAABB = false;

	if (position)
	{
		rPointDef.SetWorldPosition(*position);
		updateAABB = true;
		m_vSelectedPos = *position;

		// We have potentially moved the point to a new position, so previous interior
		// information may not be valid. The call to AfterScenarioPointDataUpdate()
		// further down will update it to match the new position.
		rPointDef.SetInterior(CScenarioPointManager::kNoInterior);

		// The acceleration grid is potentially invalidated by the movement of points.
		bankRegion.ClearAccelerationGrid();
	}

	if (direction)
	{
		rPointDef.SetWorldDirection(*direction);
	}

	if ((int)rPointDef.GetScenarioTypeVirtualOrReal() != m_iSpawnTypeIndex)
	{
		rPointDef.SetScenarioType((unsigned)m_iSpawnTypeIndex);
		m_ScenarioAnimDebugHelper.RebuildUI();
	}

	// Determine if the point is a vehicle or ped scenario.
	CurrentPointType newPointType = m_CurrentPointType;
	if(m_iSpawnTypeIndex >= 0 && m_iSpawnTypeIndex < SCENARIOINFOMGR.GetScenarioCount(true))
	{
		if(CScenarioManager::IsVehicleScenarioType(m_iSpawnTypeIndex))
		{
			newPointType = kPointTypeVehicle;
		}
		else
		{
			newPointType = kPointTypePed;
		}
	}

	// Update the model set override in the point. Note that this is done using
	// the new (ped/vehicle) type of the point, potentially using an older value
	// of m_iPedTypeIndex/m_iVehicleTypeIndex if switching between ped/vehicle types.
	if(newPointType == kPointTypeVehicle)
	{
		rPointDef.DetermineModelSetId(m_VehicleNames.GetHashForIndex(m_iVehicleTypeIndex),true);
	}
	else
	{
		rPointDef.DetermineModelSetId(m_PedNames.GetHashForIndex(m_iPedTypeIndex),false);
	}

	rPointDef.SetAvailability((CScenarioPoint::AvailabilityMpSp)m_iAvailableInMpSp);
	rPointDef.SetTimeTillPedLeaves(m_TimeTillPedLeaves);
	rPointDef.SetRadius(m_Radius);
	rPointDef.SetScenarioGroup((u16)m_iGroup);

	const bool highPriBefore = rPointDef.IsHighPriority();
	rPointDef.SetAllFlags(m_PointFlags);
	const bool highPriAfter = rPointDef.IsHighPriority();

	if(highPriBefore != highPriAfter)
	{
		updateAABB = true;
	}

	rPointDef.SetProbabilityOverride(m_Probability);

	if(m_RequiredImapName[0])
	{
		atHashString requiredImap(m_RequiredImapName);
		rPointDef.SetRequiredIMap(SCENARIOPOINTMGR.FindOrAddRequiredIMapByHash(requiredImap));
	}
	else
	{
		rPointDef.SetRequiredIMap(CScenarioPointManager::kNoRequiredIMap);
	}

	taskAssert(m_iSpawnStartTime >= 0 && m_iSpawnStartTime <= 0xff);
	taskAssert(m_iSpawnEndTime >= 0 && m_iSpawnEndTime <= 0xff);
	rPointDef.SetTimeStartOverride((u8)m_iSpawnStartTime);
	rPointDef.SetTimeEndOverride((u8)m_iSpawnEndTime);

	if(selected.IsEntityOverridePoint())
	{
		const int overrideIndex = selected.GetOverrideIndex();
		const int indexWithinOverride = selected.GetIndexWithinOverride();
		bankRegion.UpdateEntityOverrideFromPoint(overrideIndex, indexWithinOverride);

		CEntity* pEntity = rPointDef.GetEntity();
		if(pEntity)
		{
			CScenarioEntityOverride* pOverride = bankRegion.FindOverrideForEntity(*pEntity);
			if(pOverride)
			{
				pOverride->m_EntityMayNotAlwaysExist = m_OverrideEntityMayNotAlwaysExist;
			}
		}

	}

	if(updateAABB)
	{
		SCENARIOPOINTMGR.UpdateRegionAABB(m_CurrentRegion-1);
	}

	AfterScenarioPointDataUpdate(*region, rPointDef, needToCallOnAdd);

	// Check if we switched between ped and vehicle type of the scenario, in which
	// case we need to rebuild the widgets to get the model set override combo box
	// updated.
	if(newPointType != m_CurrentPointType)
	{
		// This doesn't feel too safe, essentially rebuilding the widgets that may have
		// called us through a callback... Haven't seen any problems with it in practice
		// though, if any come up, we can probably just set a flag somewhere and rebuild
		// the widgets on the next update.
		UpdateEditingPointWidgets();
	}
}


void CScenarioEditor::BeforeScenarioPointDataUpdate(CScenarioPoint& point, bool& needToCallOnAddOut)
{
	needToCallOnAddOut = false;
	if(point.GetOnAddCalled())
	{
		CScenarioManager::OnRemoveScenario(point);
		needToCallOnAddOut = true;
	}
}


void CScenarioEditor::AfterScenarioPointDataUpdate(CScenarioPointRegion& region, CScenarioPoint& point, bool needToCallOnAdd)
{
	// Whenever we are manipulating a point, try to update the interior info if it's not
	// set already.
	SCENARIOPOINTMGR.UpdateInteriorForPoint(point);

	//update our chain information if we are chained
	if (point.IsChained())
	{
		CScenarioPointRegionEditInterface bankRegion(region);
		int chainIndex = bankRegion.GetChainingGraph().FindChainingNodeIndexForScenarioPoint(point);
		if (chainIndex != -1)
		{
			bankRegion.UpdateChainNodeFromPoint(chainIndex, point);
		}
	}

	if(needToCallOnAdd)
	{
		CScenarioManager::OnAddScenario(point);
	}
}


void CScenarioEditor::PushUndoAction( const eUndoAction action, const int pointOrGroupOrRegionIndex, const bool clearRedoList, const bool bRedo)
{
	if (bRedo)
		Printf("Push redo : %d index %d\n", int(action), pointOrGroupOrRegionIndex);
	else
		Printf("Push undo : %d index %d\n", int(action), pointOrGroupOrRegionIndex);

	CScenarioPointUndoItem item;
	if(CScenarioPointUndoItem::IsPointAction(action))
	{
		Assert(pointOrGroupOrRegionIndex != (int)EditorPoint::kPointIdInvalid);

		CScenarioPointRegion* region = GetCurrentRegion();
		Assert(region);

		bool needPoint = (EditorPoint::GetTypeFromId(pointOrGroupOrRegionIndex) != EditorPoint::kTypeEntityPointOverride);

		CScenarioPoint* pt = EditorPoint::GetScenarioPointFromId(pointOrGroupOrRegionIndex, region);
		if(Verifyf(pt || !needPoint, "Failed to find scenario point."))
		{
			if(pt)
			{
				item.SetPoint(action, pointOrGroupOrRegionIndex, *pt);
			}
			else
			{
				const CScenarioPointRegionEditInterface regionInterface(*region);

				const int overrideIndex = EditorPoint::GetOverrideIndex(pointOrGroupOrRegionIndex);
				const int indexWithinOverride = EditorPoint::GetIndexWithinOverride(pointOrGroupOrRegionIndex);

				item.SetPoint(action, pointOrGroupOrRegionIndex, regionInterface.GetEntityOverride(overrideIndex).m_ScenarioPoints[indexWithinOverride]);
			}
		}
		SCENARIOPOINTMGR.FlagRegionAsEdited(m_CurrentRegion-1);
	}
	else if(CScenarioPointUndoItem::IsGroupAction(action))
	{
		Assert(pointOrGroupOrRegionIndex >= 0);

		item.SetGroup(action, pointOrGroupOrRegionIndex, SCENARIOPOINTMGR.GetGroupByIndex(pointOrGroupOrRegionIndex));
	}
	else if(CScenarioPointUndoItem::IsRegionAction(action))
	{
		Assert(pointOrGroupOrRegionIndex >= 0);

		atHashString nameHash = SCENARIOPOINTMGR.GetRegionName(pointOrGroupOrRegionIndex);
		item.SetRegion(action, pointOrGroupOrRegionIndex, nameHash);
		SCENARIOPOINTMGR.FlagRegionAsEdited(pointOrGroupOrRegionIndex);
	}
	else
	{
		Assert(pointOrGroupOrRegionIndex >= 0);

		Assert(CScenarioPointUndoItem::IsChainingEdgeAction(action));

		const CScenarioPointRegion* region = GetCurrentRegion();
		Assert(region);
		const CScenarioPointRegionEditInterface bankRegion(*region);
		const CScenarioChainingGraph& graph = bankRegion.GetChainingGraph();
		const CScenarioChainingEdge& edge = graph.GetEdge(pointOrGroupOrRegionIndex);

		item.SetChainingEdge(action, pointOrGroupOrRegionIndex, edge);
		SCENARIOPOINTMGR.FlagRegionAsEdited(m_CurrentRegion-1);
	}

	PushUndoItem(item, clearRedoList, bRedo);
}


void CScenarioEditor::PushUndoItem(const CScenarioPointUndoItem& item, bool clearRedoList, bool bRedo)
{
	if (bRedo)
	{
		m_RedoHistory.Append(item);
	}
	else
	{
		m_UndoHistory.Append(item);
	}

	Assert(!bRedo || !clearRedoList);	// assert if adding to redo and clearing the redo list at the same time
	if (clearRedoList)
		m_RedoHistory.Clear();

	// Grey out the undo/redo widgets when their queues are empty
	if (!bRedo && m_UndoButtonWidget)
	{
		const char * fill = m_UndoButtonWidget->GetFillColor();
		if ((fill==NULL) != (m_UndoHistory.GetCount() > 0))
		{
			if (fill==NULL)
				m_UndoButtonWidget->SetFillColor("ARGBColor:128:128:128:128");
			else
				m_UndoButtonWidget->SetFillColor("ARGBColor:0:0:0:0");
		}
	}
	if ((bRedo || clearRedoList) && m_RedoButtonWidget)
	{
		const char * fill = m_RedoButtonWidget->GetFillColor();
		if ((fill==NULL) != (m_RedoHistory.GetCount() > 0))
		{
			if (fill==NULL)
				m_RedoButtonWidget->SetFillColor("ARGBColor:128:128:128:128");
			else
				m_RedoButtonWidget->SetFillColor("ARGBColor:0:0:0:0");
		}
	}
}

void CScenarioEditor::Undo(const bool bRedo)
{
	atFixedArray<CScenarioPointUndoItem, UNDO_QUEUE_SIZE> & undolist = bRedo ? m_RedoHistory.m_Queue : m_UndoHistory.m_Queue;

	if (!undolist.GetCount())
	{
		return;//nothing to undo.
	}

	// We don't know at this point if the step we are undoing consists of one
	// action or multiple actions, but regardless, we want to consider it a single
	// step if we redo it, so we start a new undo step on the other buffer
	// (the one we will push on).
	if(bRedo)
	{
		m_UndoHistory.StartUndoStep();
	}
	else
	{
		m_RedoHistory.StartUndoStep();
	}

	bool startOfStep = false;
	do
	{
		CScenarioPointUndoItem& undoItem = undolist.Pop();
		startOfStep = undoItem.GetStartOfStep();

		//Process item
		const int undopointOrGroupOrRegionIndex = undoItem.GetPointOrGroupOrRegionIndex();
		switch(undoItem.GetAction()) {
		case SPU_DeletePoint:
			{
				CScenarioPointRegion* region = GetCurrentRegion();
				Assert(region);
				CScenarioPointRegionEditInterface bankRegion(*region);

				// undo the delete (add a new point)
				Printf("%s delete : %d\n", bRedo ? "Redo" : "Undo", undopointOrGroupOrRegionIndex);
				int newPointIndex = bankRegion.AddPoint(undoItem.GetPointData());

				// TODO: fix this for attached points.
				if(newPointIndex >= 0)
				{
					CScenarioPoint& newSP = bankRegion.GetPoint(newPointIndex);
					newSP.MarkAsFromRsc(); //points added to the region are RSC points.

					// push the redo event
					PushUndoAction(SPU_InsertPoint, newPointIndex, false, !bRedo);

					ResetSelections();

					EditorPoint newSelect;
					newSelect.SetWorldPoint( newPointIndex );
					m_SliderPoint = newPointIndex;
					m_SelectedEditingPoints.Select(newSelect);
					
					// refresh the widgets
					UpdateEditingPointWidgets();
					SCENARIOPOINTMGR.UpdateRegionAABB(m_CurrentRegion-1);
				}
				else
				{
					// Pushing it back again probably makes sense, it may not be safe to
					// go further back in the queue if we failed to undo something, as
					// indices stored in the queue may be off.
					undolist.Push(undoItem);
					startOfStep = true;//want to get out 
				}

				ClearAllPedTaskCachedPoints();

				break;
			}
		case SPU_InsertPoint:
			{
				CScenarioPointRegion* region = GetCurrentRegion();
				Assert(region);
				CScenarioPointRegionEditInterface bankRegion(*region);

				// push a delete to the redo buffer
				PushUndoAction(SPU_DeletePoint, undopointOrGroupOrRegionIndex, false, !bRedo);
				// undo the insertion (delete) - dont use the delete function (so as not to add undo history)
				Printf("%s insert : %d\n", bRedo ? "Redo" : "Undo", undopointOrGroupOrRegionIndex);
				bankRegion.DeletePoint(undopointOrGroupOrRegionIndex);

				ResetSelections();

				EditorPoint newSelect;
				newSelect.SetWorldPoint( undopointOrGroupOrRegionIndex > 0 ? undopointOrGroupOrRegionIndex-1 : 0 );
				m_SliderPoint = newSelect.GetWorldPointIndex();
				m_SelectedEditingPoints.Select(newSelect);

				// refresh the widgets
				UpdateEditingPointWidgets();
				SCENARIOPOINTMGR.UpdateRegionAABB(m_CurrentRegion-1);

				ClearAllPedTaskCachedPoints();
			}
			break;
		case SPU_ModifyPoint:
			{
				CScenarioPointRegion* region = GetCurrentRegion();
				Assert(region);
				CScenarioPointRegionEditInterface bankRegion(*region);
				// push current state into the redo buffer
				PushUndoAction(SPU_ModifyPoint, undopointOrGroupOrRegionIndex, false, !bRedo);
				// now undo the point
				Printf("%s modify : %d\n", bRedo ? "Redo" : "Undo", undopointOrGroupOrRegionIndex);

				bool needPoint = (EditorPoint::GetTypeFromId(undopointOrGroupOrRegionIndex) != EditorPoint::kTypeEntityPointOverride);

				CScenarioPoint* pt = EditorPoint::GetScenarioPointFromId(undopointOrGroupOrRegionIndex, region);
				if(Verifyf(pt || !needPoint, "Failed to find point."))
				{
					bool needToCallOnAdd = false;
					if(pt)
					{
						BeforeScenarioPointDataUpdate(*pt, needToCallOnAdd);

						// If the position changed by a relevant amount due to this undo action
						// being carried out, clear out any previous interior info - the
						// call to AfterScenarioPointDataUpdate() will look it up again.
						if(IsGreaterThanAll(DistSquared(pt->GetPosition(), RCC_VEC3V(undoItem.GetPointData().m_offsetPosition)), ScalarV(square(0.01f))))
						{
							pt->SetInterior(CScenarioPointManager::kNoInterior);

							bankRegion.ClearAccelerationGrid();
						}

						CScenarioPointContainer::CopyDefIntoPoint(undoItem.GetPointData(), *pt);
					}

					// refresh the widgets
					ResetSelections();
					EditorPoint newSelect;
					newSelect.SetPointId((u32)undopointOrGroupOrRegionIndex);
					if(newSelect.IsWorldPoint())
					{
						m_SliderPoint = newSelect.GetWorldPointIndex();
					}

					// If this is an entity override, make sure we keep the data in CScenarioEntityOverride
					// up to date.
					if(newSelect.IsEntityOverridePoint())
					{
						const int overrideIndex = newSelect.GetOverrideIndex();
						const int indexWithinOverride = newSelect.GetIndexWithinOverride();

						// If there is an actual point (the prop is streamed in), update the data from that,
						// otherwise, just copy straight from the CScenarioPointUndoItem (we could always do it
						// that way, but for other purposes, like when saving the region, the data in the
						// CScenarioPoint is considered authoritative).
						if(pt)
						{
							bankRegion.UpdateEntityOverrideFromPoint(overrideIndex, indexWithinOverride);
						}
						else
						{
							bankRegion.GetEntityOverride(overrideIndex).m_ScenarioPoints[indexWithinOverride] = undoItem.GetPointData();
						}
					}

					if(pt)
					{
						AfterScenarioPointDataUpdate(*region, *pt, needToCallOnAdd);
					}

					UpdateEditingPointWidgets();
					SCENARIOPOINTMGR.UpdateRegionAABB(m_CurrentRegion-1);
				}
			}
			break;

		case SPU_AttachPoint:
			{
				const u32 idBeforeOperation = undoItem.GetPointIdBeforeOperation();
				const int indexBeforeOperation = EditorPoint::GetWorldPointIndexFromId(idBeforeOperation);

				CScenarioPointRegion* region = GetCurrentRegion();
				Assert(region);
				// now undo the point
				Printf("%s attach : %d\n", bRedo ? "Redo" : "Undo", undopointOrGroupOrRegionIndex);

				CScenarioPoint* pt = EditorPoint::GetScenarioPointFromId(undopointOrGroupOrRegionIndex, region);
				if(Verifyf(pt, "Expected point, to undo attachment."))
				{
					CEntity* pAttachEntity = pt->GetEntity();
					Assert(pAttachEntity);

					CExtensionDefSpawnPoint copyOfPointDataForUndo;
					pt->CopyToDefinition(&copyOfPointDataForUndo);

					DoDetachPoint(*pt, *region, indexBeforeOperation);

					pt->InitArchetypeExtensionFromDefinition(&undoItem.GetPointData(), NULL);

					CScenarioPointUndoItem redoItem;
					redoItem.SetPoint(SPU_DetachPoint, (int)idBeforeOperation, copyOfPointDataForUndo);
					redoItem.SetPointIdBeforeOperation((u32)undopointOrGroupOrRegionIndex);
					redoItem.SetAttachEntity(pAttachEntity);
					PushUndoItem(redoItem, false, !bRedo);
				}

				ResetSelections();

				EditorPoint newSelect;
				newSelect.SetWorldPoint(indexBeforeOperation);
				m_SliderPoint = indexBeforeOperation;
				m_SelectedEditingPoints.Select(newSelect);

				ClearAllPedTaskCachedPoints();

				UpdateEditingPointWidgets();
				SCENARIOPOINTMGR.UpdateRegionAABB(m_CurrentRegion-1);

			}
			break;

		case SPU_DetachPoint:
			{
				const u32 pointIdBeforeOperation = undoItem.GetPointIdBeforeOperation();
				CEntity* pEntity = undoItem.GetAttachEntity();

				CScenarioPointRegion* region = GetCurrentRegion();
				Assert(region);
				// now undo the point
				Printf("%s detach : %d\n", bRedo ? "Redo" : "Undo", undopointOrGroupOrRegionIndex);

				ResetSelections();

				CScenarioPoint* pt = EditorPoint::GetScenarioPointFromId(undopointOrGroupOrRegionIndex, region);

				if(Verifyf(pt, "Expected point, to undo attachment."))
				{
					const int overrideIndex = EditorPoint::GetOverrideIndex(pointIdBeforeOperation);
					const int indexWithinOverride = EditorPoint::GetIndexWithinOverride(pointIdBeforeOperation);

					if(Verifyf(pEntity, "Failed to find entity to attach to."))
					{
						CExtensionDefSpawnPoint copyOfPointDataForUndo;
						pt->CopyToDefinition(&copyOfPointDataForUndo);
						pt = NULL;

						DoAttachPoint(undopointOrGroupOrRegionIndex, *region, *pEntity, overrideIndex, indexWithinOverride);

						EditorPoint newSelect;
						newSelect.SetEntityPointForOverride(overrideIndex, indexWithinOverride);

						if(newSelect.IsValid())
						{
							pt = &newSelect.GetScenarioPoint(region);
							pt->InitArchetypeExtensionFromDefinition(&undoItem.GetPointData(), NULL);

							CScenarioPointUndoItem redoItem;
							redoItem.SetPoint(SPU_AttachPoint, (int)pointIdBeforeOperation, copyOfPointDataForUndo);
							redoItem.SetPointIdBeforeOperation((u32)undopointOrGroupOrRegionIndex);
							PushUndoItem(redoItem, false, !bRedo);

							m_SelectedEditingPoints.Select(newSelect);
						}
					}
				}

				ClearAllPedTaskCachedPoints();

				UpdateEditingPointWidgets();
				SCENARIOPOINTMGR.UpdateRegionAABB(m_CurrentRegion-1);

			}
			break;

		case SPU_DisableSpawningOnEntity:
			{
				CScenarioPointRegion* region = GetCurrentRegion();
				Assert(region);

				CEntity* pEntity = undoItem.GetAttachEntity();
				if(pEntity)
				{
					DoRemoveOverrideOnEntity(*pEntity, *region, true);

					CScenarioPointUndoItem redoItem;
					redoItem.SetEntity(SPU_RemoveOverridesOnEntity, pEntity);
					PushUndoItem(redoItem, false, !bRedo);
				}

				ResetSelections();
				m_SelectedEntityWithOverride = pEntity;
				UpdateEditingPointWidgets();

				SCENARIOPOINTMGR.UpdateRegionAABB(m_CurrentRegion-1);
			}
			break;

		case SPU_RemoveOverridesOnEntity:
			{
				CScenarioPointRegion* region = GetCurrentRegion();
				Assert(region);

				CEntity* pEntity = undoItem.GetAttachEntity();
				if(pEntity)
				{
					DoDisableSpawningOnEntity(*pEntity, *region);

					CScenarioPointUndoItem redoItem;
					redoItem.SetEntity(SPU_DisableSpawningOnEntity, pEntity);
					PushUndoItem(redoItem, false, !bRedo);
				}

				ResetSelections();
				UpdateEditingPointWidgets();

				SCENARIOPOINTMGR.UpdateRegionAABB(m_CurrentRegion-1);
			}
			break;

		case SPU_ModifyGroup:
			{
				// push current state into the redo buffer
				PushUndoAction(SPU_ModifyGroup, undopointOrGroupOrRegionIndex, false, !bRedo);
				// now undo the point
				Printf("%s modify : %d\n", bRedo ? "Redo" : "Undo", undopointOrGroupOrRegionIndex);

				const CScenarioPointGroup& undoGrp = undoItem.GetGroupData();
				if(undoGrp.GetNameHash() != SCENARIOPOINTMGR.GetGroupByIndex(undopointOrGroupOrRegionIndex).GetNameHash())
				{
					DoRenameScenarioGroup(undopointOrGroupOrRegionIndex, undoGrp.GetName());
				}

				SCENARIOPOINTMGR.RestoreGroupFromUndoData(undopointOrGroupOrRegionIndex, undoItem.GetGroupData());

				m_CurrentScenarioGroup = undopointOrGroupOrRegionIndex + 1;

				UpdateCurrentScenarioGroupWidgets();
				break;
			}
		case SPU_InsertGroup:
			// push a delete to the redo buffer
			PushUndoAction(SPU_DeleteGroup, undopointOrGroupOrRegionIndex, false, !bRedo);
			// undo the insertion (delete) - dont use the delete function (so as not to add undo history)
			Printf("%s insert : %d\n", bRedo ? "Redo" : "Undo", undopointOrGroupOrRegionIndex);

			DoRemoveGroup(undopointOrGroupOrRegionIndex);
			break;

		case SPU_DeleteGroup:
			{
				// undo the delete (add a new point)
				Printf("%s delete : %d\n", bRedo ? "Redo" : "Undo", undopointOrGroupOrRegionIndex);

				SCENARIOPOINTMGR.AddGroup(undoItem.GetGroupData().GetName());
				m_CurrentScenarioGroup = SCENARIOPOINTMGR.GetNumGroups();

				// push the redo event
				PushUndoAction(SPU_InsertGroup, m_CurrentScenarioGroup - 1, false, !bRedo);

				UpdateScenarioGroupNameWidgets();
				UpdateCurrentScenarioGroupWidgets();
				UpdateEditingPointWidgets();
				break;
			}

		case SPU_ModifyChainingEdge:
			{
				CScenarioPointRegion* region = GetCurrentRegion();
				Assert(region);
				CScenarioPointRegionEditInterface bankRegion(*region);

				// push current state into the redo buffer
				PushUndoAction(SPU_ModifyChainingEdge, undopointOrGroupOrRegionIndex, false, !bRedo);
				// now undo the edge
				Printf("%s modify : %d\n", bRedo ? "Redo" : "Undo", undopointOrGroupOrRegionIndex);

				//This is a full undo so need to copy the from/to index data as well.
				bankRegion.FullUpdateChainEdge(undopointOrGroupOrRegionIndex, undoItem.GetChainingEdgeData());

				// refresh the widgets
				ResetSelections();
				m_CurrentChainingEdge = undopointOrGroupOrRegionIndex;

				UpdateEditingPointWidgets();
				break;
			}

		case SPU_DeleteChainingEdge:
			{
				CScenarioPointRegion* region = GetCurrentRegion();
				Assert(region);
				CScenarioPointRegionEditInterface bankRegion(*region);

				// undo the delete (add a new edge)
				Printf("%s delete : %d\n", bRedo ? "Redo" : "Undo", undopointOrGroupOrRegionIndex);
				int newEdgeIndex = bankRegion.AddChainingEdge(undoItem.GetNodeFrom(), undoItem.GetNodeTo());
				bankRegion.UpdateChainEdge(newEdgeIndex, undoItem.GetChainingEdgeData());

				// push the redo event
				PushUndoAction(SPU_InsertChainingEdge, newEdgeIndex, false, !bRedo);

				// refresh the widgets
				ResetSelections();
				m_CurrentChainingEdge = newEdgeIndex;

				UpdateEditingPointWidgets();

				// Update the bounding box, since this may have been a chain to an entity point.
				// which wouldn't be included in the AABB when not chained.
				SCENARIOPOINTMGR.UpdateRegionAABB(m_CurrentRegion - 1);

				break;
			}

		case SPU_InsertChainingEdge:
			{
				CScenarioPointRegion* region = GetCurrentRegion();
				Assert(region);
				CScenarioPointRegionEditInterface bankRegion(*region);
				const CScenarioChainingGraph& graph = bankRegion.GetChainingGraph();

				// push a delete to the redo buffer
				const CScenarioChainingEdge& edge = graph.GetEdge(undopointOrGroupOrRegionIndex);
				const CScenarioChainingNode& nodeFrom = graph.GetNode(edge.GetNodeIndexFrom());
				const CScenarioChainingNode& nodeTo = graph.GetNode(edge.GetNodeIndexTo());

				CScenarioPointUndoItem item;
				item.SetDeleteChainingEdge(nodeFrom, nodeTo, edge);
				PushUndoItem(item, false, !bRedo);

				// undo the insertion (delete) - dont use the delete function (so as not to add undo history)
				Printf("%s insert : %d\n", bRedo ? "Redo" : "Undo", undopointOrGroupOrRegionIndex);

				bankRegion.DeleteChainingEdgeByIndex(undopointOrGroupOrRegionIndex);

				// refresh the widgets
				ResetSelections();
				UpdateEditingPointWidgets();

				// Update the bounding box, since this may be a chain to an entity point.
				// which wasn't included in the AABB when not chained.
				SCENARIOPOINTMGR.UpdateRegionAABB(m_CurrentRegion - 1);

				break;
			}
		case SPU_ModifyRegion:
			{
				// push current state into the redo buffer
				PushUndoAction(SPU_ModifyRegion, undopointOrGroupOrRegionIndex, false, !bRedo);
				// now undo the point
				Printf("%s modify : %d\n", bRedo ? "Redo" : "Undo", undopointOrGroupOrRegionIndex);

				const atHashString& undoName = undoItem.GetRegionName();
				if(undoName != atHashString(SCENARIOPOINTMGR.GetRegionName(undopointOrGroupOrRegionIndex)))
				{
					DoRenameRegion(undopointOrGroupOrRegionIndex, undoName.GetCStr());
				}

				m_ComboRegion = undopointOrGroupOrRegionIndex + 1;

				UpdateCurrentScenarioRegionWidgets();
				break;
			}
		case SPU_InsertRegion:
			{
				// push a delete to the redo buffer
				PushUndoAction(SPU_DeleteRegion, undopointOrGroupOrRegionIndex, false, !bRedo);
				// undo the insertion (delete) - dont use the delete function (so as not to add undo history)
				Printf("%s insert : %d\n", bRedo ? "Redo" : "Undo", undopointOrGroupOrRegionIndex);

				DoRemoveRegion(undopointOrGroupOrRegionIndex);
			}
			break;

		case SPU_DeleteRegion:
			{
				// undo the delete (add a new point)
				Printf("%s delete : %d\n", bRedo ? "Redo" : "Undo", undopointOrGroupOrRegionIndex);

				SCENARIOPOINTMGR.AddRegion(undoItem.GetRegionName().GetCStr());
				m_ComboRegion = SCENARIOPOINTMGR.GetNumRegions();

				// push the redo event
				PushUndoAction(SPU_InsertRegion, m_ComboRegion - 1, false, !bRedo);

				UpdateRegionNameWidgets();
				UpdateCurrentScenarioRegionWidgets();
				break;
			}
		default:
			Assert(0);
			break;
		}
	}
	while(undolist.GetCount() && !startOfStep);

	if(bRedo)
	{
		m_UndoHistory.EndUndoStep();
	}
	else
	{
		m_RedoHistory.EndUndoStep();
	}
}

bool CScenarioEditor::TryToAddChainingEdge(const CScenarioPoint& fromPoint, const CScenarioPoint& toPoint,
		const CScenarioChainingEdge* edgeDataToCopy)
{
	Assert(&fromPoint != &toPoint);

	// Next, we'll check to see if the scenarios have any flags set to disallow the connection.
	bool allowEdge = true;

	// Check for the NoOutgoingLinks flag in the point we are connecting from.
	if(!SCENARIOINFOMGR.IsVirtualIndex(fromPoint.GetScenarioTypeVirtualOrReal()))
	{
		CScenarioInfo* pFromInfo = SCENARIOINFOMGR.GetScenarioInfoByIndex(fromPoint.GetScenarioTypeVirtualOrReal());
		if(pFromInfo && pFromInfo->GetIsFlagSet(CScenarioInfoFlags::NoOutgoingLinks))
		{
			allowEdge = false;
			Displayf("Scenario point of type %s doesn't allow outgoing links.", pFromInfo->GetName());
		}
	}

	// Check for the NoIncomingLinks flag in the point we are connecting to.
	if(!SCENARIOINFOMGR.IsVirtualIndex(toPoint.GetScenarioTypeVirtualOrReal()))
	{
		CScenarioInfo* pToInfo = SCENARIOINFOMGR.GetScenarioInfoByIndex(toPoint.GetScenarioTypeVirtualOrReal());
		if(pToInfo && pToInfo->GetIsFlagSet(CScenarioInfoFlags::NoIncomingLinks))
		{
			allowEdge = false;
			Displayf("Scenario point of type %s doesn't allow incoming links.", pToInfo->GetName());
		}
	}

	if(allowEdge)
	{
		CScenarioPointRegion* region = GetCurrentRegion();
		Assert(region);
		CScenarioPointRegionEditInterface bankRegion(*region);

		const int numBefore = bankRegion.GetChainingGraph().GetNumEdges();
		m_CurrentChainingEdge = bankRegion.AddChainingEdge(fromPoint, toPoint);

		if(edgeDataToCopy)
		{
			bankRegion.UpdateChainEdge(m_CurrentChainingEdge, *edgeDataToCopy);
		}

		// Update the bounding box, since this may be a chain to an entity point.
		// which wasn't included in the AABB when unchained.
		SCENARIOPOINTMGR.UpdateRegionAABB(m_CurrentRegion - 1);

		if(numBefore != bankRegion.GetChainingGraph().GetNumEdges())
		{
			PushUndoAction(SPU_InsertChainingEdge, m_CurrentChainingEdge, true, false);	// push the insert undo action AFTER inserting the edge
		}

		return true;
	}
	return false;
}

void CScenarioEditor::PeriodicallyUpdateInterior()
{
	// Updating the interior info for all points in the current region is
	// potentially a bit expensive, so we do it only occasionally, to pick up any interiors
	// that get streamed in while a region is being edited.

	const u32 currentTime = fwTimer::GetTimeInMilliseconds();
	if(currentTime > m_TimeForNextInteriorUpdateMs)
	{
		UpdateInterior();
		m_TimeForNextInteriorUpdateMs = currentTime + 10000;	// 10 s
	}	
}

void CScenarioEditor::UpdateInterior()
{
	if(GetCurrentRegion())
	{
		SCENARIOPOINTMGR.UpdateInteriorForRegion(m_CurrentRegion - 1);
	}
}

bool CScenarioEditor::PurgeInvalidSelections()
{
	atArray<EditorPoint> invalid;
	m_SelectedEditingPoints.ForAll(CScenarioEditor::GatherInvalidEditorPoints, &invalid);

	//Deselect the invalid
	if (invalid.GetCount())
	{
		for (int i = 0; i < invalid.GetCount(); i++)
		{
			m_SelectedEditingPoints.Deselect(invalid[i]);
		}

		m_SliderPoint = -1;

		// Return true if something got purged.
		return true;
	}
	return false;
}

void CScenarioEditor::ResetSelections()
{
	m_SelectedEditingPoints.DeselectAll();
	m_SliderPoint = -1;
	m_CurrentChainingEdge = -1;
	m_SelectedEntityWithOverride = NULL;
	m_SelectedOverrideWithNoPropIndex = -1;
}

CScenarioPointRegion* CScenarioEditor::GetCurrentRegion()
{
	if (m_CurrentRegion > 0)
	{
		return SCENARIOPOINTMGR.GetRegion(m_CurrentRegion-1);
	}
	return NULL;
}

const CScenarioPointRegion* CScenarioEditor::GetCurrentRegion() const
{
	if (m_CurrentRegion > 0)
	{
		return SCENARIOPOINTMGR.GetRegion(m_CurrentRegion-1);
	}
	return NULL;
}

//
// Functions called after to loading and prior to saving
// Make sure the editor state is cleaned up
//
void CScenarioEditor::PostLoadData()
{
	// clear the history after loading
	m_UndoHistory.Clear();
	m_RedoHistory.Clear();

	ResetSelections();
	m_ComboRegion = FindCurrentRegion(); //start out in a real region not "no region"
	m_CurrentRegion = -1;
	m_TransferRegion = 0;

	// Update widgets and stuff to avoid problems, as the data could have totally
	// changed by the call to Load().
	if(m_CurrentScenarioGroup > SCENARIOPOINTMGR.GetNumGroups())
	{
		m_CurrentScenarioGroup = CScenarioPointManager::kNoGroup;
	}
	UpdateScenarioGroupNameWidgets();
	UpdateCurrentScenarioGroupWidgets();
	UpdateEditingPointWidgets();
	UpdateRegionNameWidgets();
	UpdateCurrentScenarioRegionWidgets();
}

void CScenarioEditor::PreSaveData()
{
	m_UndoHistory.Clear();
	m_RedoHistory.Clear();
}

bool CScenarioEditor::IsRegionSelected(int regionToCheck) const
{
	return (m_ComboRegion-1 == regionToCheck);
}

bool CScenarioEditor::IsPointSelected(const CScenarioPoint& pointToCheck) const
{
	const CScenarioPointRegion* region = GetCurrentRegion();
	if (region)
	{
		EditorPoint found = m_SelectedEditingPoints.FindEditorPointForPoint(pointToCheck, *region);
		return found.IsValid();
	}
	return false;
}

bool CScenarioEditor::IsPointHandleHovered(const CScenarioPoint& pointToCheck) const
{
	const CScenarioPointRegion* region = GetCurrentRegion();
	if (region)
	{
		//Point must be selected and editable to be be able to hover on the handle
		EditorPoint found = m_SelectedEditingPoints.FindEditorPointForPoint(pointToCheck, *region);
		if (found.IsValid() && found.IsEditable())
		{
			return m_MouseOverHandle;
		}
	}
	return false;
}

bool CScenarioEditor::IsEdgeSelected(const CScenarioChainingEdge& edgeToCheck) const
{
	if (m_CurrentChainingEdge != -1)
	{
		const CScenarioPointRegion* region = GetCurrentRegion();
		if( region )
		{
			CScenarioPointRegionEditInterface bankRegion(*region);
			if (&edgeToCheck == &bankRegion.GetChainingGraph().GetEdge(m_CurrentChainingEdge))
			{
				return true;
			}	
		}
	}

	return false;
}

void CScenarioEditor::CScenarioPointUndoItem::SetPoint( const eUndoAction action, const int pointId, const CScenarioPoint& pointData )
{
	Assert(IsPointAction(action));

	m_Action = action;
	m_PointOrGroupOrRegionIndex = pointId;
	pointData.CopyToDefinition(&m_PointData);
}


void CScenarioEditor::CScenarioPointUndoItem::SetPoint( const eUndoAction action, const int pointId, const CExtensionDefSpawnPoint& pointData )
{
	Assert(IsPointAction(action));

	m_Action = action;
	m_PointOrGroupOrRegionIndex = pointId;
	m_PointData = pointData;
}


void CScenarioEditor::CScenarioPointUndoItem::SetEntity(const eUndoAction action, CEntity* pEntity)
{
	Assert(action == SPU_DisableSpawningOnEntity || action == SPU_RemoveOverridesOnEntity);
	m_Action = action;
	m_PointOrGroupOrRegionIndex = -1;
	m_AttachEntity = pEntity;
}


void CScenarioEditor::CScenarioPointUndoItem::SetChainingEdge( const eUndoAction action, const int edgeIndex, const CScenarioChainingEdge& edgeData)
{
	Assert(IsChainingEdgeAction(action));

	m_Action = action;
	m_PointOrGroupOrRegionIndex = edgeIndex;
	m_ChainingEdgeData = edgeData;
}


void CScenarioEditor::CScenarioPointUndoItem::SetGroup(const eUndoAction action, const int groupIndex, const CScenarioPointGroup& groupData)
{
	Assert(IsGroupAction(action));

	m_Action = action;
	m_PointOrGroupOrRegionIndex = groupIndex;
	m_GroupData = groupData;
}


void CScenarioEditor::CScenarioPointUndoItem::SetDeleteChainingEdge(const CScenarioChainingNode& nodeFrom, const CScenarioChainingNode& nodeTo, const CScenarioChainingEdge& edgeData)
{
	m_Action = SPU_DeleteChainingEdge;
	m_NodeFrom = nodeFrom;
	m_NodeTo = nodeTo;
	m_PointOrGroupOrRegionIndex = -1;
	m_ChainingEdgeData = edgeData;
}

void CScenarioEditor::CScenarioPointUndoItem::SetRegion(const eUndoAction action, const int regionIndex, const atHashString& regionNameHash)
{
	Assert(IsRegionAction(action));

	m_Action = action;
	m_PointOrGroupOrRegionIndex = regionIndex;
	m_RegionName = regionNameHash;
}

const CExtensionDefSpawnPoint& CScenarioEditor::CScenarioPointUndoItem::GetPointData() const
{
	Assert(m_Action != SPU_Undefined);
	Assert(IsPointAction(m_Action));
	return m_PointData;
}


const CScenarioPointGroup& CScenarioEditor::CScenarioPointUndoItem::GetGroupData() const
{
	Assert(m_Action != SPU_Undefined);
	Assert(IsGroupAction(m_Action));
	return m_GroupData;
}

const atHashString& CScenarioEditor::CScenarioPointUndoItem::GetRegionName() const
{
	Assert(m_Action != SPU_Undefined);
	Assert(IsRegionAction(m_Action));
	return m_RegionName;
}

void CScenarioEditor::DestroyEntitiesAtScenarioPoint(CScenarioPoint& scenarioPoint, float extraClearRadius)
{
	if(CPedPopulation::IsEffectInUse(scenarioPoint) )
	{
		// Find the ped that is using it and delete them!
		CEntityIterator globalEntityIterator(IteratePeds);
		CPed *pFoundPed = static_cast<CPed*>(globalEntityIterator.GetNext());
		while(pFoundPed)
		{
			CTask	*pScenarioTask = pFoundPed->GetPedIntelligence()->GetTaskActive();
			if(pScenarioTask)
			{
				if( pScenarioTask->GetScenarioPoint() == &scenarioPoint )
				{
					if(pFoundPed->GetIsInVehicle())
					{
						CVehicle* pVehicle = pFoundPed->GetMyVehicle();
						if(pVehicle)
						{
							CVehicleFactory::GetFactory()->Destroy(pVehicle);
						}
					}
					else
					{
						// De-spawn it
						CPedFactory::GetFactory()->DestroyPed(pFoundPed);
					}
				}
			}
			pFoundPed = static_cast<CPed*>(globalEntityIterator.GetNext());
		}
	}

	if(extraClearRadius > 0.0f)
	{
		const Vec3V posV = scenarioPoint.GetPosition();
		CGameWorld::ClearPedsFromArea(RCC_VECTOR3(posV), extraClearRadius);
		CGameWorld::ClearCarsFromArea(RCC_VECTOR3(posV), extraClearRadius, false, false);
	}
}

void	CScenarioEditor::SpawnPedOrVehicleToUseCurrentPointPressed()
{
	if (m_SelectedEditingPoints.HasSelection() && !m_SelectedEditingPoints.IsMultiSelecting())
	{
		m_AttemptPedVehicleSpawnUsePoint = MAX_SPAWN_FRAMES; // try MAX_SPAWN_FRAMES to spawn a ped/vehicle
	}
}

void	CScenarioEditor::SpawnPedOrVehicleAtCurrentPointPressed()
{
	if (m_SelectedEditingPoints.HasSelection() && !m_SelectedEditingPoints.IsMultiSelecting())
	{
		m_AttemptPedVehicleSpawnAtPoint = MAX_SPAWN_FRAMES; // try MAX_SPAWN_FRAMES to spawn a ped/vehicle
	}
}


bool CScenarioEditor::SpawnPedToUseCurrentPoint(CScenarioPoint& scenarioPoint, bool bInstant, bool fallbackToAnyModel )
{
	DestroyEntitiesAtScenarioPoint(scenarioPoint);

	//for any point we are attempting to spawn at we should make sure there is no history 
	// that could prevent spawning ... 
	CScenarioManager::GetSpawnHistory().ClearHistoryForPoint(scenarioPoint);

	int iRealScenarioType = CScenarioManager::ChooseRealScenario(scenarioPoint.GetScenarioTypeVirtualOrReal(), NULL);
	const CScenarioInfo* pScenarioInfo = CScenarioManager::GetScenarioInfo(iRealScenarioType);

	if(pScenarioInfo && pScenarioInfo->GetIsClass<CScenarioCoupleInfo>())
	{
		float maxFrustumDistToCheck = 0.0f;
		bool allowDeepInteriors = false;
		u32 iSpawnOptions = CPedPopulation::SF_forceSpawn | CPedPopulation::SF_ignoreScenarioHistory;
		int numSpawned = CScenarioManager::SetupCoupleScenario(scenarioPoint, maxFrustumDistToCheck, allowDeepInteriors, iSpawnOptions, iRealScenarioType);
		return (numSpawned == 2)? true : false;//couples spawn 2 peds or the spawn failed ... 
	}

	if(pScenarioInfo && pScenarioInfo->GetIsClass<CScenarioLookAtInfo>())
	{
		aiErrorf("No support for spawning peds to use with look-at scenario points");
		return false;
	}

	// Calculate the spawn point, use the Scenario Point and orientation
	float	fHeading = 0.0f;
	s32		iFlags = 0;
	Vector3	spawnPoint;

	spawnPoint = VEC3V_TO_VECTOR3(scenarioPoint.GetPosition());

	if (!bInstant)
	{
		float	direction = m_bSpawnInRandomOrientation ? fwRandom::GetRandomNumberInRange(0.0f, TWO_PI) : m_SpawnOrientation;
		float	offsX = (rage::Cosf(direction) - rage::Sinf( direction )) * 3.0f;
		float	offsY = (rage::Cosf(direction) + rage::Sinf( direction )) * 3.0f;

		spawnPoint.x += offsX;
		spawnPoint.y += offsY;

		// Set the heading to face the point
		fHeading = rage::Atan2f(offsX, -offsY);
	}
	else
	{
		const Vector3 vOffset = VEC3V_TO_VECTOR3(scenarioPoint.GetDirection());
		iFlags = CTaskUseScenario::SF_SkipEnterClip;

		// Set the heading to be the direction of the point
		fHeading = rage::Atan2f(-vOffset.x, vOffset.y);
	}

	s32 iRoomIdFromCollision = 0;
	CInteriorInst* pInteriorInst = NULL;

	// Do a ground check here to find the spawnpoint on the ground
	CPedPlacement::FindZCoorForPed(BOTTOM_SURFACE,&spawnPoint, NULL, &iRoomIdFromCollision, &pInteriorInst, 1.0f, 99.5f, ArchetypeFlags::GTA_ALL_MAP_TYPES|ArchetypeFlags::GTA_VEHICLE_TYPE, NULL, false);

	// Spawn a ped of the right type at the spawnpoint
	strLocalIndex	newPedModelIndex = strLocalIndex(fwModelId::MI_INVALID);

	CPed	*pPed = NULL;

	const CAmbientModelVariations* variations = NULL;
	if (CScenarioManager::GetPedTypeForScenario(iRealScenarioType, scenarioPoint, newPedModelIndex, spawnPoint, 0, &variations))
	{
		fwModelId newPedModelId(newPedModelIndex);
		if( newPedModelId.IsValid() )
		{
			// Ensure that the model is loaded and ready for drawing for this ped.
			// Follows the usage from the else case.
			if (!CModelInfo::HaveAssetsLoaded(newPedModelId))
			{
				CModelInfo::RequestAssets(newPedModelId, STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
				CStreaming::LoadAllRequestedObjects(true);
			}

			// Ensure that the model is loaded and ready for drawing for this ped.
			if (CModelInfo::HaveAssetsLoaded(newPedModelId))
			{
				DefaultScenarioInfo defaultScenarioInfo(DS_Invalid, NULL);
				if( iRoomIdFromCollision != 0 )
				{
					aiFatalAssertf(pInteriorInst, "NULL interior inst pointer!");
					pPed = CPedPopulation::AddPed(newPedModelId.GetModelIndex(), spawnPoint, fHeading, NULL, true, iRoomIdFromCollision, pInteriorInst, true, false, false, false, defaultScenarioInfo, true, false, 0, 0, variations);
				}
				else
				{
					if (bInstant)
					{
						CTask* pScenarioTask = CScenarioManager::SetupScenarioAndTask( iRealScenarioType, spawnPoint, fHeading, scenarioPoint, 0, false, true, true);
						if (pScenarioTask)
						{
							CScenarioManager::PedTypeForScenario pedType;
							pedType.m_PedModelIndex = newPedModelIndex.Get();
							pedType.m_NeedsToBeStreamed = false;
							pedType.m_Variations = variations;

							pPed = CScenarioManager::CreateScenarioPed( iRealScenarioType, scenarioPoint, pScenarioTask, spawnPoint, 0.f, true, fHeading, CPedPopulation::SF_ignoreScenarioHistory, 0, false, NULL, &pedType);
						}
					}
					else
					{
						pPed = CPedPopulation::AddPed(newPedModelId.GetModelIndex(), spawnPoint, fHeading, NULL, false, -1, NULL, true, false, false, false, defaultScenarioInfo, true, false, 0, 0, variations);
					}
				}
			}
		}
	}
	else if (fallbackToAnyModel)
	{
		// Last ditch, get any model
		fwArchetypeDynamicFactory<CPedModelInfo>& pedModelInfoStore = CModelInfo::GetPedModelInfoStore();
		atArray<CPedModelInfo*> pedTypesArray;
		pedModelInfoStore.GatherPtrs(pedTypesArray);

		int	pedIDX;

		//crash during delete!!! pedIDX = CGameWorld::FindLocalPlayer()->GetModelIndex();
		pedIDX = 5;		// 0-3 crash during delete... so we're using 5

		// Get the modelInfo associated with this ped type.
		CPedModelInfo& pedModelInfo = *pedTypesArray[pedIDX];
		newPedModelIndex = CModelInfo::GetModelIdFromName(pedModelInfo.GetModelName()).GetModelIndex();
		fwModelId newPedModelId(newPedModelIndex);

		// Ensure that the model is loaded and ready for drawing for this ped.
		if (!CModelInfo::HaveAssetsLoaded(newPedModelId))
		{
			CModelInfo::RequestAssets(newPedModelId, STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
			CStreaming::LoadAllRequestedObjects(true);
		}

		// Ensure that the model is loaded and ready for drawing for this ped.
		if (CModelInfo::HaveAssetsLoaded(newPedModelId))
		{
			Matrix34 TempMat;
			TempMat.Identity();
			TempMat.RotateZ( fHeading );
			TempMat.d = spawnPoint;

			const CControlledByInfo localAiControl(false, false);
			pPed = CPedFactory::GetFactory()->CreatePed(localAiControl, newPedModelId, &TempMat, true, false, false);
			if (pPed)
			{
				pPed->SetVarRandom(PV_FLAG_NONE, PV_FLAG_NONE, NULL, NULL, NULL, PV_RACE_UNIVERSAL);
				pPed->PopTypeSet(POPTYPE_RANDOM_AMBIENT);
				CGameWorld::Add(pPed, CGameWorld::OUTSIDE );
			}
		}
	}

	if (pPed)
	{
		pPed->UpdateRagdollMatrix(true);
		pPed->SetDesiredHeading(fHeading);

		CTask *pTask = CScenarioManager::SetupScenarioAndTask(iRealScenarioType, spawnPoint, fHeading, scenarioPoint, iFlags, !bInstant, true, true, bInstant);

		if(bInstant && pTask && pTask->GetTaskType() == CTaskTypes::TASK_USE_SCENARIO)
		{
			static_cast<CTaskUseScenario*>(pTask)->CreateProp();
			static_cast<CTaskUseScenario*>(pTask)->ShouldIgnorePavementChecksWhenLeavingScenario(m_bIgnorePavementChecksWhenLeavingScenario);
		}

		aiTask *pControlTask = CScenarioManager::SetupScenarioControlTask(*pPed, *pScenarioInfo, pTask, &scenarioPoint, iRealScenarioType);

		// This stuff may help to prevent physics activation if we spawn at a distance, which
		// otherwise has potential to cause vertical misplacement.
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_IsStanding, true);
		pPed->GetPedAiLod().ClearBlockedLodFlag(CPedAILod::AL_LodPhysics);
		pPed->GetPedAiLod().SetForcedLodFlag(CPedAILod::AL_LodPhysics);

		// Tell him to do a new task
		pPed->GetPedIntelligence()->AddTaskDefault( pControlTask );
		// Rebuild the queriable state as soon as the task has been allocated
		pPed->GetPedIntelligence()->BuildQueriableState();
		pPed->SetDefaultDecisionMaker();
		pPed->SetCharParamsBasedOnManagerType();
		pPed->GetPedIntelligence()->SetDefaultRelationshipGroup();

		// Set the ped config flags based on the properties of the point.
		if (scenarioPoint.IsFlagSet(CScenarioPointFlags::SpawnedPedIsArrestable))
		{
			pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_CanBeArrested, true);
		}	
	}
	else
	{
	  return false; //no ped created.
	} 

	return true;
}


bool CScenarioEditor::SpawnVehicleAtCurrentPoint(CScenarioPoint& scenarioPoint)
{
	DestroyEntitiesAtScenarioPoint(scenarioPoint, 6.0f);

	//for any point we are attempting to spawn at we should make sure there is no history 
	// that could prevent spawning ... 
	CScenarioManager::GetSpawnHistory().ClearHistoryForPoint(scenarioPoint);
	
	// Clear the users array on the chain, for now we just delete the current users completely
	ClearChainUsersForDebugSpawn(scenarioPoint);

	const int scenarioType = scenarioPoint.GetScenarioTypeVirtualOrReal();
	if(scenarioType < 0)
	{
		return false;
	}

	// TODO: Support virtual ones here?
	taskAssert(!SCENARIOINFOMGR.IsVirtualIndex(scenarioType));

	const CScenarioInfo* pScenarioInfo = SCENARIOINFOMGR.GetScenarioInfoByIndex(scenarioType);
	if(!pScenarioInfo)
	{
		return false;
	}

	if(!pScenarioInfo->GetIsClass<CScenarioVehicleInfo>())
	{
		return false;
	}

	const CCarGenerator* pCarGen = scenarioPoint.GetCarGen();
	if(!pCarGen)
	{
		return false;
	}

	if (scenarioPoint.IsChained() && scenarioPoint.IsUsedChainFull())
	{
		return false;
	}

	const CScenarioVehicleInfo& info = static_cast<const CScenarioVehicleInfo&>(*pScenarioInfo);

	fwModelId vehModelId;

	const CBaseModelInfo* pVehModelInfo = NULL;
	const char* modelName = NULL;

	u32 modelHash = CScenarioManager::GetRandomVehicleModelHashOverrideFromIndex(info, scenarioPoint.GetModelSetIndex());
	if(modelHash != 0)
	{
		atHashString str(modelHash);
		modelName = str.TryGetCStr();

		pVehModelInfo = CModelInfo::GetBaseModelInfoFromHashKey(modelHash, &vehModelId);
		if(!taskVerifyf(pVehModelInfo, "Failed to find CBaseModelInfo for vehicle '%s'", modelName))
		{
			return false;
		}
	}
	else
	{
		// No model sets specified in CScenarioVehicleInfo or CScenarioPoint.

		fwModelId trailer;
		u32 modelIndex = pCarGen->PickSuitableCar(trailer);
		if(!CModelInfo::IsValidModelInfo(modelIndex))
		{
			return false;
		}

		vehModelId.SetModelIndex(modelIndex);
		pVehModelInfo = CModelInfo::GetBaseModelInfo(vehModelId);
		modelName = pVehModelInfo ? pVehModelInfo->GetModelName() : "?";
	}

	if(!taskVerifyf(pVehModelInfo && pVehModelInfo->GetModelType() == MI_TYPE_VEHICLE, "'%s' is not a vehicle model.", modelName))
	{
		return false;
	}

	fwModelId trailerId;
	if(!CScenarioManager::GetCarGenTrailerToUse(*pCarGen, trailerId))
	{
		trailerId.Invalidate();
	}

	// Note: partly adapted from CreateCarAtMatrix() in 'VehicleFactory.cpp' - not sure if it will be worth
	// refactoring to try to share more code.

	bool bForceLoad = false;
	if(!CModelInfo::HaveAssetsLoaded(vehModelId))
	{
		CModelInfo::RequestAssets(vehModelId, STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
		bForceLoad = true;
	}

	if(trailerId.IsValid())
	{
		const CBaseModelInfo* pTrailerModelInfo = NULL;
		pTrailerModelInfo = CModelInfo::GetBaseModelInfo(trailerId);

		if(!CModelInfo::HaveAssetsLoaded(trailerId))
		{
			CModelInfo::RequestAssets(trailerId, STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
			bForceLoad = true;
		}
	}

	// TODO: We may also want to force-load driver/passengers and surrounding scenario peds,
	// but we probably don't even do that for the regular on-foot scenarios at the moment.

	if(bForceLoad)
	{
		CStreaming::LoadAllRequestedObjects(true);
	}

	if(!CModelInfo::HaveAssetsLoaded(vehModelId))
	{
		return false;
	}

	if(trailerId.IsValid() && !CModelInfo::HaveAssetsLoaded(trailerId))
	{
		return false;
	}

	Mat34V mtrx;
	pCarGen->BuildCreationMatrix(&RC_MATRIX34(mtrx), vehModelId.GetModelIndex());
	CVehicle *pNewVehicle = CVehicleFactory::GetFactory()->Create(vehModelId, ENTITY_OWNEDBY_DEBUG, POPTYPE_RANDOM_AMBIENT, &RCC_MATRIX34(mtrx));
	if(!pNewVehicle)
	{
		return false;
	}	

	CVehicle* pTrailer = NULL;
	if(trailerId.IsValid())
	{
		const bool bUseBiggerBoxesForCollisionTest = false;
		pTrailer = CVehicleFactory::GetFactory()->CreateAndAttachTrailer(trailerId, pNewVehicle, RCC_MATRIX34(mtrx), ENTITY_OWNEDBY_DEBUG, POPTYPE_RANDOM_AMBIENT, bUseBiggerBoxesForCollisionTest);
		if(!pTrailer)
		{
			CVehicleFactory::GetFactory()->Destroy(pNewVehicle);
			return false;
		}
	}

	//Normally these things are taken care of in cargen, but we're not actually going through that codepath with the editor.
	pNewVehicle->SwitchEngineOff();
	pNewVehicle->SetIsAbandoned();

	if(!CScenarioManager::PopulateCarGenVehicle(*pNewVehicle, *pCarGen, false))
	{
		CVehicleFactory::GetFactory()->Destroy(pNewVehicle);

		// Note: we intentionally don't destroy the trailer here - I had a crash when I tried
		// that, I think because it's already attached to the main vehicle and got destroyed
		// together with it.

		return false;
	}

	// This may be necessary for livery/color changes done by PopulateCarGenVehicle() to take effect.
	pNewVehicle->UpdateBodyColourRemapping();

	CGameWorld::Add(pNewVehicle, CGameWorld::OUTSIDE);
	if(pTrailer)
	{
		CGameWorld::Add(pTrailer, CGameWorld::OUTSIDE);
	}

	CPortalTracker* pPT = pNewVehicle->GetPortalTracker();
	if(pPT)
	{
		pPT->RequestRescanNextUpdate();
		pPT->Update(VEC3V_TO_VECTOR3(pNewVehicle->GetVehiclePosition()));
	}

	pNewVehicle->PlaceOnRoadAdjust();
	if (pTrailer)
	{
		pTrailer->PlaceOnRoadAdjust();
	}

	CPed* pDriver = pNewVehicle->GetDriver();
	if (pDriver)
	{
		pDriver->InstantAIUpdate();
		pDriver->SetPedResetFlag(CPED_RESET_FLAG_SkipAiUpdateProcessControl, true);
	}

	bool bDummyConversionFailed = false;
	CVehiclePopulation::MakeVehicleIntoDummyBasedOnDistance(*pNewVehicle, pNewVehicle->GetVehiclePosition(), bDummyConversionFailed);
	
	return true;
}

void CScenarioEditor::SpawnClusterOfCurrentPointPressed()
{
	m_SelectedEditingPoints.ForAll(CScenarioEditor::TriggerClusterSpawn, NULL);
}

void CScenarioEditor::ClearAllPedTaskCachedPoints()
{
	CPed::Pool* pPedPool = CPed::GetPool();
	const int maxPeds = pPedPool->GetSize();
	for(int i =0; i < maxPeds; i++)
	{
		CPed* pPed = pPedPool->GetSlot(i);
		if(!pPed)
		{
			continue;
		}

		CTask* pTask = pPed->GetPedIntelligence()->FindTaskByType(CTaskTypes::TASK_USE_SCENARIO);
		if(pTask)
		{
			CTaskUseScenario* pTaskUseScenario = static_cast<CTaskUseScenario*>(pTask);
			pTaskUseScenario->ClearCachedWorldScenarioPointIndex();
		}

		pTask = pPed->GetPedIntelligence()->FindTaskByType(CTaskTypes::TASK_USE_VEHICLE_SCENARIO);
		if(pTask)
		{
			CTaskUseVehicleScenario* pVehicleScenarioTask = static_cast<CTaskUseVehicleScenario*>(pTask);
			pVehicleScenarioTask->ClearCachedPrevAndNextWorldScenarioPointIndex();
		}
	}
}


bool CScenarioEditor::IsValidGroupName(const char* name)
{
	if(!name || !name[0])
	{
		return false;
	}

	for(int i = 0; name[i]; i++)
	{
		char c = name[i];
		if(c >= 'a' && c <= 'z')
		{
			continue;
		}
		if(c >= 'A' && c <= 'Z')
		{
			continue;
		}
		if(c >= '0' && c <= '9')
		{
			continue;
		}
		if(c == '_' || c == '@' || c == '-')
		{
			continue;
		}
		if(c == ' ' && i != 0 && name[i + 1])	// Probably shouldn't allow a name to start or end with space.
		{
			continue;
		}
		return false;
	}

	return true;
}

bool CScenarioEditor::IsValidRegionName(const char* name)
{
	if(!name || !name[0])
	{
		return false;
	}

	for(int i = 0; name[i]; i++)
	{
		char c = name[i];
		if(c >= 'a' && c <= 'z')
		{
			continue;
		}
		if(c >= 'A' && c <= 'Z')
		{
			continue;
		}
		if(c >= '0' && c <= '9')
		{
			continue;
		}
		if(c == '_')
		{
			continue;
		}
		return false;
	}

	return true;
}

bool CScenarioEditor::IsPositionValidForChaining(const Vector3& position, const CScenarioPoint* exceptionPoint/* = NULL*/)
{
	bool valid = true;
	//have to enforce a min distance to create points within ... 
	// chaining uses CScenarioChainingGraph::sm_PointSearchRadius when looking for points during the add of an edge.
	CScenarioPoint* areaPoints[CScenarioPointManager::kMaxPtsInArea];
	u32 foundCnt = SCENARIOPOINTMGR.FindPointsInArea(RCC_VEC3V(position), ScalarV(CScenarioChainingGraph::sm_PointSearchRadius), areaPoints, NELEM(areaPoints));
	if (foundCnt)
	{
		if (foundCnt == 1 && areaPoints[0] == exceptionPoint)
		{
			//not a failure because the exception point is the only thing that is in the way.
		}
		else
		{
			//Dont care about exception point because we came up with multiple points in our check.
			Vec3V foundPos = areaPoints[0]->GetPosition();
			//There is a point close here so print an error message
			taskWarningf("Point {%.2f, %.2f, %.2f} is too close to point {%.2f, %.2f, %.2f} for possible use in chaining.",
				position.x,
				position.y,
				position.z,
				foundPos.GetXf(),
				foundPos.GetYf(),
				foundPos.GetZf()
				);
			valid = false;
		}
	}

	return valid;
}


bool CScenarioEditor::IsEntityValidForAttachment(CEntity& entity)
{
	if(!entity.GetIsTypeObject() && !entity.GetIsTypeBuilding())
	{
		// Not the right type.
		return false;
	}

	if(!entity.GetBaseModelInfo())
	{
		// This seems to happen for some CBuildings, perhaps terrain.
		return false;
	}

	// TODO: Maybe check if the entity came from map data, or if it's
	// a reasonable size, etc.

	return true;
}


void CScenarioEditor::AttemptSpawnPedOrVehicle()
{
	if (m_SelectedEditingPoints.IsMultiSelecting()) //not allowed when multi selecting 
		return;

	// Since this happens in the CDebug::Update phase, and vehicles haven't had their AIData cached yet,
	// make sure to do it now in case the spawned ped or vehicle needs to know
	CVehicle::StartAiUpdate();

	EditorPoint onlyPoint = m_SelectedEditingPoints.GetOnlyPoint();
	if (onlyPoint.IsValid())
	{
		CScenarioPointRegion* region = GetCurrentRegion();
		Assert(region);
		CScenarioPoint& scenarioPoint = onlyPoint.GetScenarioPoint(region);
		
		if(CScenarioManager::IsVehicleScenarioType(scenarioPoint.GetScenarioTypeVirtualOrReal()))
		{
			if (m_AttemptPedVehicleSpawnAtPoint)
			{
				if (SpawnVehicleAtCurrentPoint(scenarioPoint))
				{
					//Success
					Displayf("Spawned vehicle for point");
					m_AttemptPedVehicleSpawnAtPoint = 0;
				}
				else
				{
					m_AttemptPedVehicleSpawnAtPoint--;
					if (m_AttemptPedVehicleSpawnAtPoint)
					{
						Warningf("Attempt to spawn vehicle for scenario failed %d more attempts left", m_AttemptPedVehicleSpawnAtPoint);
					}
					else
					{
						Errorf("Attempt to spawn vehicle for scenario failed %d more attempts left", m_AttemptPedVehicleSpawnAtPoint);
					}
				}
			}

			if (m_AttemptPedVehicleSpawnUsePoint)
			{
				if (SpawnVehicleAtCurrentPoint(scenarioPoint))
				{
					//Success
					Displayf("Spawned vehicle for point");
					m_AttemptPedVehicleSpawnUsePoint = 0;
				}
				else
				{
					m_AttemptPedVehicleSpawnUsePoint--;
					if (m_AttemptPedVehicleSpawnUsePoint)
					{
						Warningf("Attempt to spawn vehicle for scenario failed %d more attempts left", m_AttemptPedVehicleSpawnUsePoint);
					}
					else
					{
						Errorf("Attempt to spawn vehicle for scenario failed %d more attempts left", m_AttemptPedVehicleSpawnUsePoint);
					}
				}
			}
		}
		else
		{
			if (m_AttemptPedVehicleSpawnAtPoint)
			{
				//if this is the last time around and we have not yet streamed in the model then just use the fallback model
				bool useFallBack = (m_AttemptPedVehicleSpawnAtPoint == 1) ? true : false;
				if (SpawnPedToUseCurrentPoint(scenarioPoint, true, useFallBack))
				{
					//Success
					Displayf("Spawned Ped for point");
					m_AttemptPedVehicleSpawnAtPoint = 0;
				}
				else
				{
					m_AttemptPedVehicleSpawnAtPoint--;
					if (m_AttemptPedVehicleSpawnAtPoint)
					{
						Warningf("Attempt to spawn ped for scenario failed %d more attempts left", m_AttemptPedVehicleSpawnAtPoint);
					}
					else
					{
						Errorf("Attempt to spawn ped for scenario failed %d more attempts left", m_AttemptPedVehicleSpawnAtPoint);
					}
				}
			}

			if (m_AttemptPedVehicleSpawnUsePoint)
			{
				//if this is the last time around and we have not yet streamed in the model then just use the fallback model
				bool useFallBack = (m_AttemptPedVehicleSpawnUsePoint == 1) ? true : false;
				if (SpawnPedToUseCurrentPoint(scenarioPoint, false, useFallBack))
				{
					//Success
					Displayf("Spawned Ped for point");
					m_AttemptPedVehicleSpawnUsePoint = 0;
				}
				else
				{
					m_AttemptPedVehicleSpawnUsePoint--;
					if (m_AttemptPedVehicleSpawnUsePoint)
					{
						Warningf("Attempt to spawn ped for scenario failed %d more attempts left", m_AttemptPedVehicleSpawnUsePoint);
					}
					else
					{
						Errorf("Attempt to spawn ped for scenario failed %d more attempts left", m_AttemptPedVehicleSpawnUsePoint);
					}
				}
			}
		}
	}
	else
	{
		m_AttemptPedVehicleSpawnAtPoint = 0;
		m_AttemptPedVehicleSpawnUsePoint = 0;
	}
}

void CScenarioEditor::OnEditorWidgetEvent (const CEditorWidget::EditorWidgetEvent& _event)
{
	if (m_SelectedEditingPoints.IsMultiSelecting()) //not allowed when multi selecting 
		return;

	EditorPoint onlyPoint = m_SelectedEditingPoints.GetOnlyPoint();

	CScenarioPoint* pSelPoint = NULL;
	if(onlyPoint.IsValid())
	{
		CScenarioPointRegion* region = GetCurrentRegion();
		Assert(region);
		pSelPoint = &onlyPoint.GetScenarioPoint(region);
	}

	if (!pSelPoint)
		return;

	ScalarV transStep(0.01f);

	if (_event.IsTranslate())
	{
		Vec3V newPos = pSelPoint->GetPosition();
		if (_event.TranslateX())
		{
			newPos = newPos + Vec3V(V_X_AXIS_WZERO) * (transStep * _event.GetDelta());
		}
		if (_event.TranslateY())
		{
			newPos = newPos + Vec3V(V_Y_AXIS_WZERO) * (transStep * _event.GetDelta());
		}
		if (_event.TranslateZ())
		{
			newPos = newPos + Vec3V(V_Z_AXIS_WZERO) * (transStep * _event.GetDelta());
		}
		ScenarioPointDataUpdate(&newPos, NULL);
	}
}

void CScenarioEditor::CheckPointHandleSelection(EditorPoint& editPoint, void* userdata)
{
	if (editPoint.IsEditable() && Verifyf(userdata, "CheckPointHandleSelection requires userdata in the form of a MousePosition to figure out if you are hovering on the handle"))
	{
		CScenarioPointRegion* region = CScenarioDebug::ms_Editor.GetCurrentRegion();
		Assert(region);
		// see if the mouse is over the rotation handle of the currently selected point (irrespective of whether the mouse buttons are down)
		CScenarioPoint& point = editPoint.GetScenarioPoint(region);

		Vec3V direction = point.GetDirection();
		Vec3V position = point.GetPosition();

		MousePosition* mousePos = (MousePosition*)userdata;
		Vector3 mouseRayDirection;
		mouseRayDirection.Normalize(mousePos->GetDirection());

		spdSphere sphere = spdSphere(position + direction*ScalarV(CScenarioDebug::ms_fPointHandleLength), ScalarV(CScenarioDebug::ms_fPointHandleRadius*2.0f));
		Vector3 out1, out2;
		if(fwGeom::IntersectSphereRay(sphere, mousePos->GetPosScreen(), mouseRayDirection, out1, out2) && out1.Dist2(mousePos->GetPosScreen()) < sm_cHandleSelectRange*sm_cHandleSelectRange)
		{
			CScenarioDebug::ms_Editor.m_MouseOverHandle = true;
		}
	}
}

void CScenarioEditor::TtyPrint(EditorPoint& editPoint, void* /*userdata*/)
{
	//Print out the selected point's info to the TTY
	CScenarioPoint* pPoint = NULL;
	if(editPoint.IsValid())
	{
		CScenarioPointRegion* region = CScenarioDebug::ms_Editor.GetCurrentRegion();
		pPoint = &editPoint.GetScenarioPoint(region);
	}

	if (pPoint)
	{
		CExtensionDefSpawnPoint def;
		pPoint->CopyToDefinition(&def);

		Vec3V dir = pPoint->GetDirection();

		Displayf("Type %s", def.m_spawnType.GetCStr());
		Displayf("Position %.2f,%.2f,%.2f", def.m_offsetPosition.x, def.m_offsetPosition.y, def.m_offsetPosition.z);
		Displayf("Direction %.2f,%.2f,%.2f", dir.GetXf(), dir.GetYf(), dir.GetZf());
		Displayf("Group %s", def.m_group.GetCStr());
		Displayf("ModelIndex %s", def.m_pedType.GetCStr());
		Displayf("Start Time Override %d", def.m_start);
		Displayf("End Time Override %d", def.m_end);
		Displayf("Probability %f", def.m_probability);
		Displayf("Time Till Leave %.2f", def.m_timeTillPedLeaves);
		Displayf("Radius %.2f", def.m_radius);
		Displayf("Availability %s", s_AvailableInMpSpNames[def.m_availableInMpSp]);
		Displayf("Extended range %s", (def.m_extendedRange)?"true":"false");
		Displayf("Short range %s", (def.m_shortRange) ? "true" : "false");
		Displayf("High Priority %s", (def.m_highPri)?"true":"false");
		if (pPoint->GetEntity())
		{
			Displayf("Entity %s", pPoint->GetEntity()->GetModelName());
		}
		// TODO: print attachment info here?
		Displayf("*******************************************************");
	}
}

void CScenarioEditor::UpdateSelectedPointFromWidgets(EditorPoint& editPoint, void* /*userdata*/)
{
	if(editPoint.IsValid())
	{
		CScenarioDebug::ms_Editor.m_LastSelectedScenarioType = CScenarioDebug::ms_Editor.m_ScenarioNames[CScenarioDebug::ms_Editor.m_iSpawnTypeIndex];
	}
	
	// Add an undo point prior to changing
	CScenarioDebug::ms_Editor.PushUndoAction(SPU_ModifyPoint, (int)editPoint.GetPointId(), true);
	CScenarioDebug::ms_Editor.ScenarioPointDataUpdate(NULL, NULL);
}

void CScenarioEditor::GatherUniqueChainIds(EditorPoint& editPoint, void* userdata)
{
	if (editPoint.IsValid() && Verifyf(userdata, "GatherUniqueChainIds requires userdata in the form of a atArray<int> to return the chain ids"))
	{
		CScenarioPointRegion* region = CScenarioDebug::ms_Editor.GetCurrentRegion();
		Assert(region);
		CScenarioPoint& point = editPoint.GetScenarioPoint(region);
		if (point.IsChained())
		{
			CScenarioPointRegionEditInterface bankRegion(*region);
			const CScenarioChainingGraph& graph = bankRegion.GetChainingGraph();
			const int index = graph.FindChainingNodeIndexForScenarioPoint(point);
			Assert(index > -1);
			const int chainId = graph.GetNodesChainIndex(index);
			atArray<int>* foundIds = (atArray<int>*)userdata;
			if (foundIds->Find(chainId) < 0) //only if it is not found already
			{
				foundIds->PushAndGrow(chainId);
			}
		}
	}
}

void CScenarioEditor::GatherInvalidEditorPoints(EditorPoint& editPoint, void* userdata)
{
	if (Verifyf(userdata, "GatherInvalidEditorPoints requires userdata in the form of a atArray<EditorPoint> to return the invalid points"))
	{
		bool invalid = false;
		if (!editPoint.IsValid())
		{
			invalid = true;
		}
		else if (editPoint.IsWorldPoint())
		{
			CScenarioPointRegion* region = CScenarioDebug::ms_Editor.GetCurrentRegion();
			if (!region)
			{
				invalid = true;
			}
			else
			{
				CScenarioPointRegionEditInterface bankRegion(*region);
				const int numSpawnPoints = bankRegion.GetNumPoints();
				if (numSpawnPoints <= 0)
				{
					invalid = true;
				}

				if (editPoint.GetWorldPointIndex() >= numSpawnPoints)
				{
					invalid = true;
				}
			}
		}
		else if(editPoint.IsEntityOverridePoint())
		{
			CScenarioPointRegion* region = CScenarioDebug::ms_Editor.GetCurrentRegion();
			if(!region)
			{
				invalid = true;
			}
			else
			{
				invalid = true;
				CScenarioPointRegionEditInterface bankRegion(*region);
				const int overrideIndex = EditorPoint::GetOverrideIndex(editPoint.GetPointId());
				const int indexWithinOverride = EditorPoint::GetIndexWithinOverride(editPoint.GetPointId());
				if(overrideIndex >= 0 && overrideIndex < region->GetNumEntityOverrides())
				{
					const CScenarioEntityOverride& override = bankRegion.GetEntityOverride(overrideIndex);
					if(indexWithinOverride >= 0 && indexWithinOverride < override.m_ScenarioPointsInUse.GetCount())
					{
						CScenarioPoint* pt = override.m_ScenarioPointsInUse[indexWithinOverride];
						if(pt)
						{
							invalid = false;
						}
					}
				}
			}
		}
		else if (editPoint.IsClusteredWorldPoint())
		{
			CScenarioPointRegion* region = CScenarioDebug::ms_Editor.GetCurrentRegion();
			if(!region)
			{
				invalid = true;
			}
			else
			{
				CScenarioPointRegionEditInterface bankRegion(*region);

				const u32 pointId = editPoint.GetPointId();
				const int pointIndex = EditorPoint::GetClusterPointIndex(pointId);
				const int clusterIndex = EditorPoint::GetClusterIndex(pointId);

				const int ccount = bankRegion.GetNumClusters();
				if (ccount > 0)
				{
					if (clusterIndex >= 0 && clusterIndex < ccount )
					{
						const int pcount = bankRegion.GetNumClusteredPoints(clusterIndex);
						if (pcount > 0)
						{
							if (pointIndex >= 0 && pointIndex < pcount )
							{
								//This is valid ... 
							}
							else
							{
								invalid = true;
							}
						}
						else
						{
							invalid = true;
						}
					}
					else
					{
						invalid = true;
					}
				}
				else
				{
					invalid = true;
				}
			}
		}
		else
		{
			const int index = editPoint.GetEntityPointIndex();
			if(index < 0 || index >= SCENARIOPOINTMGR.GetNumEntityPoints())
			{
				invalid = true;
			}
		}

		if(invalid)
		{
			atArray<EditorPoint>* invalidArray = (atArray<EditorPoint>*)userdata;
			invalidArray->PushAndGrow(editPoint);
		}
	}
}

void CScenarioEditor::DisableSpawningForSelectedEntityPoints(EditorPoint& editPoint, void* /*userdata*/)
{
	if(!editPoint.IsValid() || editPoint.IsWorldPoint())
	{
		return;
	}

	CScenarioPointRegion* region = CScenarioDebug::ms_Editor.GetCurrentRegion();
	if(!region)
	{
		return;
	}

	CScenarioPoint& propPoint = SCENARIOPOINTMGR.GetEntityPoint(editPoint.GetEntityPointIndex());
	Assert(!propPoint.IsEntityOverridePoint());

	CEntity* pEntity = propPoint.GetEntity();
	if(!pEntity)
	{
		return;
	}

	CScenarioDebug::ms_Editor.DoDisableSpawningOnEntity(*pEntity, *region);

	CScenarioPointUndoItem undoItem;
	undoItem.SetEntity(SPU_DisableSpawningOnEntity, pEntity);
	CScenarioDebug::ms_Editor.PushUndoItem(undoItem, true, false);
}

void CScenarioEditor::GatherSelectedClusters(EditorPoint& editPoint, void* userdata)
{
	if (editPoint.IsValid() && Verifyf(userdata, "GatherSelectedClusters requires userdata in the form of a atArray<int> to return the selected clusters"))
	{
		CScenarioPointRegion* region = CScenarioDebug::ms_Editor.GetCurrentRegion();
		Assert(region);
		CScenarioPoint& point = editPoint.GetScenarioPoint(region);
		if (point.IsInCluster())
		{
			CScenarioPointRegionEditInterface bankRegion(*region);
			const int clusterId = bankRegion.FindPointsClusterIndex(point);
			if (clusterId != CScenarioPointCluster::INVALID_ID)
			{
				atArray<int>* foundIds = (atArray<int>*)userdata;
				if (foundIds->Find(clusterId) < 0) //only if it is not found already
				{
					foundIds->PushAndGrow(clusterId);
				}
			}
		}
	}
}

void CScenarioEditor::AddToCluster(EditorPoint& editPoint, void* userdata)
{
	if (editPoint.IsValid() && Verifyf(userdata, "AddToCluster requires userdata in the form of an int to know what cluster to add to"))
	{
		//Only world points?
		if (editPoint.IsWorldPoint())
		{
			CScenarioPointRegion* region = CScenarioDebug::ms_Editor.GetCurrentRegion();
			Assert(region);
			CScenarioPointRegionEditInterface bankRegion(*region);

			const int clusterId = *((int*)userdata);
			const int pointId = editPoint.GetWorldPointIndex();

			CScenarioPoint* removed = bankRegion.RemovePoint(pointId, false /*dont delete edges*/);
			const int cpID = bankRegion.AddPointToCluster(clusterId, *removed);
			
			//edit the selected point to be a cluster point now ... 
			editPoint.SetClusteredWorldPoint(clusterId, cpID);

			CScenarioDebug::ms_Editor.m_SelectedEditingPoints.UpdateSelectionForRemovedPoint(pointId);
		}
		else if (editPoint.IsClusteredWorldPoint())
		{
			CScenarioPointRegion* region = CScenarioDebug::ms_Editor.GetCurrentRegion();
			Assert(region);
			CScenarioPointRegionEditInterface bankRegion(*region);

			const int clusterId = *((int*)userdata);
			const int oldCID = EditorPoint::GetClusterIndex(editPoint.GetPointId());

			if (clusterId != oldCID)
			{
				const int oldCPID = EditorPoint::GetClusterPointIndex(editPoint.GetPointId());

				CScenarioPoint* removed = bankRegion.RemovePointfromCluster(oldCID, oldCPID, false /* leave the chains alone */);
				const int cpID = bankRegion.AddPointToCluster(clusterId, *removed);

				//edit the selected point to be a right cluster point ... 
				editPoint.SetClusteredWorldPoint(clusterId, cpID);

				CScenarioDebug::ms_Editor.m_SelectedEditingPoints.UpdateSelectionForRemovedClusterPoint(oldCID, oldCPID);
			}
		}
		else
		{
			Errorf("A selected point is not a world point. Not adding that one to the cluster.");
		}
	}
}

void CScenarioEditor::RemoveFromCluster(EditorPoint& editPoint, void* /*userdata*/)
{
	if (editPoint.IsValid())
	{
		if (editPoint.IsClusteredWorldPoint())
		{
			CScenarioPointRegion* region = CScenarioDebug::ms_Editor.GetCurrentRegion();
			Assert(region);
			CScenarioPointRegionEditInterface bankRegion(*region);

			const int cID = EditorPoint::GetClusterIndex(editPoint.GetPointId());
			const int cpID = EditorPoint::GetClusterPointIndex(editPoint.GetPointId());

			CScenarioPoint* removed = bankRegion.RemovePointfromCluster(cID, cpID, false /* leave the chains alone */);
			const int pID = bankRegion.AddPoint(*removed);

			//edit the selected point to be a world point now ... 
			editPoint.SetWorldPoint(pID);

			CScenarioDebug::ms_Editor.m_SelectedEditingPoints.UpdateSelectionForRemovedClusterPoint(cID, cpID);
		}
	}
}

void CScenarioEditor::TriggerClusterSpawn(EditorPoint& editPoint, void* /*userdata*/)
{
	if (editPoint.IsValid())
	{
		if (editPoint.IsClusteredWorldPoint())
		{
			CScenarioPointRegion* region = CScenarioDebug::ms_Editor.GetCurrentRegion();
			Assert(region);
			CScenarioPointRegionEditInterface bankRegion(*region);

			const int cID = EditorPoint::GetClusterIndex(editPoint.GetPointId());
			bankRegion.TriggerClusterSpawn(cID);
		}
	}
}

EditorPoint CScenarioEditor::FindClosestDataPoint(const Vector3 & position, const float range) const
{
	EditorPoint thePoint;

	if(m_CurrentMousePosition.IsValid())	// nan check
	{
		// Extend the ray slightly to account for objects near the floor
		Vector3 mouseScreen = m_CurrentMousePosition.GetPosScreen();
		Vector3	dV = position - mouseScreen;
		dV.Normalize();
		Vector3 endPos = position + (range * dV);
		float minT = FLT_MAX;

		const CScenarioPointRegion* region = GetCurrentRegion();

		if( region )
		{
			CScenarioPointRegionEditInterface bankRegion(*region);
			EditorPoint wPoint = FindClosestWorldPoint(bankRegion, mouseScreen, endPos, range, minT);
			if (wPoint.IsValid())
			{
				thePoint = wPoint;
			}

			EditorPoint cPoint = FindClosestClusterPoint(bankRegion, mouseScreen, endPos, range, minT);
			if (cPoint.IsValid())
			{
				thePoint = cPoint;
			}

			EditorPoint eoPoint = FindClosestEntityOverridePoint(bankRegion, mouseScreen, endPos, range, minT);
			if (eoPoint.IsValid())
			{
				thePoint = eoPoint;
			}
		}

		EditorPoint ePoint = FindClosestEntityPoint(mouseScreen, endPos, range, minT);
		if (ePoint.IsValid())
		{
			thePoint = ePoint;
		}
	}

	return thePoint;
}

EditorPoint CScenarioEditor::FindClosestWorldPoint(const CScenarioPointRegionEditInterface& curRegion, const Vector3& startPos, const Vector3& endPos, const float range, float& minT_InOut) const
{
	EditorPoint thePoint;
	const int pcount = curRegion.GetNumPoints();
	for (int p = 0; p < pcount; p++)
	{
		const CScenarioPoint& spoint = curRegion.GetPoint(p);
		if (m_bFreePlacementMode)
		{
			ScalarV mintt(minT_InOut);
			if (PointIsCloserThanMinT(spoint, RCC_VEC3V(startPos), RCC_VEC3V(endPos), mintt))
			{
				minT_InOut = mintt.Getf();
				thePoint.SetWorldPoint(p);
			}
		}
		else 
		{
			if (PointIsCloserThanMinT(spoint, startPos, endPos, range, minT_InOut))
			{
				thePoint.SetWorldPoint(p);
			}
		}
	}

	return thePoint;
}

EditorPoint CScenarioEditor::FindClosestClusterPoint(const CScenarioPointRegionEditInterface& curRegion, const Vector3& startPos, const Vector3& endPos, const float range, float& minT_InOut) const
{
	EditorPoint thePoint;
	const int ccount = curRegion.GetNumClusters();
	for (int c = 0; c < ccount; c++)
	{
		const int cpcount = curRegion.GetNumClusteredPoints(c);
		for (int cp = 0; cp < cpcount; cp++)
		{
			const CScenarioPoint& spoint = curRegion.GetClusteredPoint(c, cp);
			if (m_bFreePlacementMode)
			{
				ScalarV mintt(minT_InOut);
				if (PointIsCloserThanMinT(spoint, RCC_VEC3V(startPos), RCC_VEC3V(endPos), mintt))
				{
					minT_InOut = mintt.Getf();
					thePoint.SetClusteredWorldPoint(c, cp);
				}
			}
			else 
			{
				if (PointIsCloserThanMinT(spoint, startPos, endPos, range, minT_InOut))
				{
					thePoint.SetClusteredWorldPoint(c, cp);
				}
			}
		}
	}

	return thePoint;
}

EditorPoint CScenarioEditor::FindClosestEntityOverridePoint(const CScenarioPointRegionEditInterface& curRegion, const Vector3& startPos, const Vector3& endPos, const float range, float& minT_InOut) const
{
	EditorPoint thePoint;
	const int eocount = curRegion.GetNumEntityOverrides();
	for (int eo = 0; eo < eocount; eo++)
	{
		const CScenarioEntityOverride& eover = curRegion.GetEntityOverride(eo);
		const int numOverridePoints = eover.m_ScenarioPointsInUse.GetCount();
		for(int op = 0; op < numOverridePoints; op++)
		{
			if (!eover.m_ScenarioPointsInUse[op])
				continue;

			const CScenarioPoint& spoint = *(eover.m_ScenarioPointsInUse[op].Get());
			if (!spoint.GetEntity())
				continue;

			if (m_bFreePlacementMode)
			{
				ScalarV mintt(minT_InOut);
				if (PointIsCloserThanMinT(spoint, RCC_VEC3V(startPos), RCC_VEC3V(endPos), mintt))
				{
					minT_InOut = mintt.Getf();
					thePoint.SetEntityPointForOverride(eo, op);
				}
			}
			else 
			{
				if (PointIsCloserThanMinT(spoint, startPos, endPos, range, minT_InOut))
				{
					thePoint.SetEntityPointForOverride(eo, op);
				}
			}
		}
	}

	return thePoint;
}

EditorPoint CScenarioEditor::FindClosestEntityPoint(const Vector3& startPos, const Vector3& endPos, const float range, float& minT_InOut) const
{
	EditorPoint thePoint;
	const int numScenarioPoints = SCENARIOPOINTMGR.GetNumEntityPoints();
	for(int i = 0; i < numScenarioPoints; i++)
	{
		const CScenarioPoint& spoint = SCENARIOPOINTMGR.GetEntityPoint(i);

		if (!spoint.GetEntity())
			continue;

		//Skip entity override points...
		if (spoint.IsEntityOverridePoint())
			continue;

		//Skip if this is a non rsc point
		if (!spoint.IsFromRsc())
			continue;

		//Entity points cant be free placed so not checking that here ... 
		if (PointIsCloserThanMinT(spoint, startPos, endPos, range, minT_InOut))
		{
			thePoint.SetEntityPoint(i);
		}
	}

	return thePoint;
}

bool CScenarioEditor::PointIsCloserThanMinT(const CScenarioPoint& point, const Vector3& startPos, const Vector3& endPos, const float range, float& minT_InOut) const
{
	bool retval = false;
	float distance2, t;
	float bestRange2 = range*range;
	ScalarV tVal;
	const Vec3V testPoint = point.GetPosition();
	const Vec3V deltaV = Subtract(VECTOR3_TO_VEC3V(endPos), VECTOR3_TO_VEC3V(startPos));
	const Vec3V pointOnLine = geomPoints::FindClosestPointAndTValSegToPoint(VECTOR3_TO_VEC3V(startPos), deltaV, testPoint, tVal);
	t = tVal.Getf();
	distance2 = DistSquared(testPoint, pointOnLine).Getf();

	if( (distance2 <= bestRange2 ) && ( t < minT_InOut ) )
	{
		minT_InOut = t;
		retval = true;
	}

	return retval;
}

bool CScenarioEditor::PointIsCloserThanMinT(const CScenarioPoint& point, Vec3V_In startPoint, Vec3V_In endPoint, ScalarV_InOut minT_InOut) const
{
	bool retval = false;
	Vec3V position = point.GetPosition();
	ScalarV segmentT1, unusedT2, sphereRadius(CScenarioDebug::ms_fPointDrawRadius*2.0f);
	if (geomSegments::SegmentToSphereIntersections(position, startPoint, endPoint, sphereRadius, segmentT1, unusedT2))
	{
		if (IsLessThanAll(segmentT1, minT_InOut))
		{
			minT_InOut = segmentT1;
			retval = true;
		}
	}

	return retval;
}

int CScenarioEditor::GetChainUsersOfChainScenarioPointIsIn( const atArray<CScenarioPointChainUseInfo *> *& arr, const CScenarioPoint & rPoint, CScenarioChainingGraph *& pGraphOut )
{
	if (!rPoint.IsChained())
	{
		return 0;
	}
	for (int i = 0; i < SCENARIOPOINTMGR.GetNumActiveRegions(); ++i)
	{
		CScenarioPointRegion &rRegion = SCENARIOPOINTMGR.GetActiveRegion(i);
		CScenarioChainingGraph & rGraph = const_cast<CScenarioChainingGraph &>(rRegion.GetChainingGraph());
		int iChainNodeIndex = rGraph.FindChainingNodeIndexForScenarioPoint(rPoint);
		if (iChainNodeIndex >= 0)
		{
			int iChainIndex = rGraph.GetNodesChainIndex(iChainNodeIndex);
			if (iChainIndex >= 0)
			{
				CScenarioChain &rChain = rGraph.GetChain(iChainIndex);
				arr = &rChain.GetUsers();
				if (arr)
				{
					pGraphOut = &rGraph;
					return arr->GetCount();
				}
				break;
			}
		}
	}
	return 0;
}

void CScenarioEditor::ClearChainUsersForDebugSpawn( const CScenarioPoint & rPoint )
{
	// Clear the users array on the chain, for now we just delete the current users completely
	const atArray<CScenarioPointChainUseInfo *> * pUsers = NULL;
	CScenarioChainingGraph * pGraph = NULL;
	int iNumFound = GetChainUsersOfChainScenarioPointIsIn(pUsers, rPoint, pGraph);
	for (int i = 0; i < iNumFound; ++i)
	{
		Assert(pUsers && pGraph);
		CScenarioPointChainUseInfo * pInfo = (*pUsers)[i];
		Assert(pInfo);
		// Deleting the user ped/vehicle will remove the chain user
		// pGraph->RemoveChainUser(pInfo);
		CVehicle * pVehicle = pInfo->GetVehicle();
		CPed * pPed = pInfo->GetPed();
		if (pVehicle && pVehicle->CanBeDeleted())
		{
			CVehicleFactory::GetFactory()->Destroy(pVehicle);
		}
		if (pPed && pPed->CanBeDeleted())
		{
			CPedFactory::GetFactory()->DestroyPed(pPed);
		}
	}
}

////////////////////////////////////////////////////////////////////////////

// Used by CbCompareSetInfos(), user has to set before calling.
static CAmbientModelSetManager::ModelSetType s_SortModelSetType = CAmbientModelSetManager::kPedModelSets;

static int CbCompareSetInfos(const void* pVoidA, const void* pVoidB)
{
	const s32* pA = (const s32*)pVoidA;
	const s32* pB = (const s32*)pVoidB;

	return stricmp(CAmbientModelSetManager::GetInstance().GetModelSet(s_SortModelSetType, *pA).GetName(),CAmbientModelSetManager::GetInstance().GetModelSet(s_SortModelSetType, *pB).GetName());
}

CModelSetNameData::CModelSetNameData(int modelSetType)
		: m_ModelSetType(modelSetType)
		, m_numSetsSkipped(0)
		, m_numFixedStrings(0)
{
}


void CModelSetNameData::Init(const char ** fixedStrings)
{
	// Fixed strings are added in before the ped names
	m_numFixedStrings = 0;
	if (fixedStrings)
	{
		const char ** p = fixedStrings;
		while(*p)
		{
			m_numFixedStrings++;
			p++;
		}
	}

	// Pulled from CPedVariationDebug::UpdatePedList()
	u32 i=0;

	m_SortOdered.Reset();
	m_Names.Reset();

	CAmbientModelSetManager::ModelSetType setType = (CAmbientModelSetManager::ModelSetType)m_ModelSetType;

	s32 numSetNames = CAmbientModelSetManager::GetInstance().GetNumModelSets(setType);

	m_SortOdered.Resize(numSetNames);
	m_Names.Reserve(numSetNames + m_numFixedStrings);

	// Add the 'fixed' strings
	for(int i=0;i<m_numFixedStrings;i++)
	{
		m_Names.PushAndGrow(fixedStrings[i]);
	}

	for(i=0;i<numSetNames;i++) {
		m_SortOdered[i] = i;
	}

	s_SortModelSetType = setType;
	qsort(m_SortOdered.GetElements(),numSetNames,sizeof(s32),CbCompareSetInfos);

	m_numSetsSkipped = 0;
	for(i=0;i<numSetNames;i++)
	{
		m_Names.PushAndGrow(CAmbientModelSetManager::GetInstance().GetModelSet(setType, m_SortOdered[i]).GetName());
	}
}

// Slow, maybe make an array with the hash values?
int CModelSetNameData::GetIndexFromHash( const u32 hash ) const
{
	const int numNames = m_Names.GetCount();
	for(int i=0;i<numNames; i++)
	{
		if (atStringHash(m_Names[i])==hash)
			return i;
	}
	return -1;
}




#endif // SCENARIO_DEBUG
