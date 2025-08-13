// Rage includes
//#include "curve/mayacurve.h"
#include "input/mouse.h"
#include "vector/color32.h"
#include "vector/colors.h"
#include "vector/geometry.h"
#include "grcore/debugdraw.h"
#include "fwsys/gameskeleton.h"
#include "physics/gtaArchetype.h"
#include "fwmaths/vector.h"
#include "Scene/scene.h"
#include "debug/DebugScene.h"
#include "camera/viewports/ViewportManager.h"
#include "control/gamelogic.h"

#include "MapZones.h"
#include "MapZones_parser.h"

AI_OPTIMISATIONS()

/////////////////////////////////////////////////////
//	CMapZoneArea
/////////////////////////////////////////////////////


bool CMapZoneArea::IsPointInArea(const Vector3 &vPos) const
{
	static const float fEps = 0.0f;

	// If vPos is outside of the plane of any edge, then we're not inside the hull
	u32	nPoints = m_ZoneAreaHull.size();

	// If we have no data, we can't be inside it!
	if( nPoints < 3)
	{
		return false;
	}

	int lastv = nPoints-1;
	for(int i=0; i<nPoints; i++)
	{
		Vector2 vEdge = Vector2(m_ZoneAreaHull[i].x, m_ZoneAreaHull[i].y) - Vector2(m_ZoneAreaHull[lastv].x, m_ZoneAreaHull[lastv].y);
		vEdge.Normalize();
		const Vector2 vNormal(vEdge.y, -vEdge.x);
		const float fPlaneDist = - ((vNormal.x * m_ZoneAreaHull[i].x) + (vNormal.y * m_ZoneAreaHull[i].y));
		const float fDist = ((vNormal.x*vPos.x)+(vNormal.y*vPos.y))+fPlaneDist;
		if(fDist >= fEps)
			return false;
		lastv = i;
	}
	return true;
}


#if __BANK

void CMapZoneArea::DrawHull(bool isCurrentlySelected)
{
	if( m_ZoneAreaHull.size() >= 3)
	{
		#define HULL_DRAW_PUSH_UP_VALUE	(0.01f);

		Color32 colour = Color_pink;
		if(!isCurrentlySelected)
		{
			colour.SetAlpha(128);		// Semi transparent for the non focus areas
		}

		Vector3	fanVert = m_ZoneAreaHull[0];
		fanVert.z += HULL_DRAW_PUSH_UP_VALUE;		// Push up a little to avoid z-fighting
		for(int i=2;i<m_ZoneAreaHull.size();i++)
		{
			Vector3	nextPoints[2];
			nextPoints[0] = m_ZoneAreaHull[i-1];
			nextPoints[1] = m_ZoneAreaHull[i];
			nextPoints[0].z += HULL_DRAW_PUSH_UP_VALUE;
			nextPoints[1].z += HULL_DRAW_PUSH_UP_VALUE;
			grcDebugDraw::Poly(fanVert, nextPoints[0], nextPoints[1], colour, colour, colour );
		}

		if(isCurrentlySelected)
		{
			// Now draw the edges
			colour = Color_green;
			colour.SetAlpha(255);
			for(int i=1;i<m_ZoneAreaHull.size();i++)
			{
				grcDebugDraw::Line(m_ZoneAreaHull[i-1], m_ZoneAreaHull[i], colour);
			}
			grcDebugDraw::Line(m_ZoneAreaHull[m_ZoneAreaHull.size()-1], m_ZoneAreaHull[0], colour);
		}

	}
}

#endif	//BANK



/////////////////////////////////////////////////////
//	CMapZone
/////////////////////////////////////////////////////

bool	CMapZone::IsPointInZone(Vector3 &point)
{
	if(IsInBoundBox(point))
	{
		// It's in this box (flat), check if we're in any of the convex hull areas in this zone
		for(int i=0;i<GetZoneAreaCount();i++)
		{
			CMapZoneArea &currentArea = GetZoneArea(i);

			if( currentArea.IsPointInArea(point))
			{
				return true;
			}
		}
	}
	return false;
}


#if __BANK

void CMapZone::MakeZoneBoundBox()
{
	m_BoundBox.Invalidate();
	for(int i=0;i<GetZoneAreaCount();i++)
	{
		CMapZoneArea &currentArea = GetZoneArea(i);
		for(int j=0; j<currentArea.GetHullPointCount(); j++)
		{
			Vector3 &point = currentArea.GetHullPoint(j);
			m_BoundBox.GrowPoint(VECTOR3_TO_VEC3V(point));
		}
	}
}

void CMapZone::DrawZoneBoundBox()
{
	s32 v;

	Color32 color = Color_blue;

	const Vector3 &vMin = m_BoundBox.GetMinVector3();
	const Vector3 &vMax = m_BoundBox.GetMaxVector3();

	Vector3 vCorners[8] = {
		Vector3(vMin.x, vMin.y, vMin.z), Vector3(vMax.x, vMin.y, vMin.z), Vector3(vMax.x, vMax.y, vMin.z), Vector3(vMin.x, vMax.y, vMin.z),
		Vector3(vMin.x, vMin.y, vMax.z), Vector3(vMax.x, vMin.y, vMax.z), Vector3(vMax.x, vMax.y, vMax.z), Vector3(vMin.x, vMax.y, vMax.z)
	};

	for(v=0; v<4; v++)
	{
		grcDebugDraw::Line(vCorners[v], vCorners[(v+1)%4], color);
		grcDebugDraw::Line(vCorners[v+4], vCorners[((v+1)%4)+4], color);
		grcDebugDraw::Line(vCorners[v], vCorners[v+4], color);
	}
}

#endif	//BANK

/////////////////////////////////////////////////////
//	CMapZonesContainer
/////////////////////////////////////////////////////



/////////////////////////////////////////////////////
//	CMapZoneManager
/////////////////////////////////////////////////////

CMapZonesContainer	CMapZoneManager::m_MapZones;

void CMapZoneManager::Init(unsigned initMode)
{
	if (initMode == INIT_AFTER_MAP_LOADED)
	{
		LoadMapZones();

#if	__BANK
		InitWidgets();
#endif	//_BANK

		PostCreation();

	}
}

void CMapZoneManager::Shutdown(unsigned UNUSED_PARAM(shutdownMode))
{
	ResetAllZones();
}

void CMapZoneManager::Update()
{

#if __BANK
	UpdateEditor();
#endif	//__BANK

}

u32 CMapZoneManager::GetZoneAtCoords(Vector3 &vPos)
{
	for(int i=0;i<m_MapZones.GetNumZones();i++)
	{
		if( m_MapZones.GetZone(i).IsPointInZone(vPos) )
		{
			atHashString temp( m_MapZones.GetZone(i).GetName() );
			return temp.GetHash();
		}
	}
	return 0;
}


void CMapZoneManager::PostCreation()
{
}

void CMapZoneManager::ResetAllZones()
{
	m_MapZones.Reset();
}

void CMapZoneManager::RemoveZone(u32 zoneID)
{
	m_MapZones.GetZones().Delete(zoneID);
}

void CMapZoneManager::LoadMapZones()
{
	ResetAllZones();

	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetLastFile(CDataFileMgr::MAPZONES_FILE);

	// Did we get a valid entry?
	if(DATAFILEMGR.IsValid(pData))
	{
		// yes, load it.
		INIT_PARSER;
		PARSER.LoadObject(pData->m_filename, "", *&m_MapZones);
	}

#if __BANK
	ResetEditor();
	UpdateZoneComboBox();
	SetEditPointsFromHulls();
#endif	//__BANK

}





#if	__BANK

/////////////////////////////////////////////////////
//	CMapZoneManager - editor section
/////////////////////////////////////////////////////

bool	CMapZoneManager::m_bEditing = false;
bool	CMapZoneManager::m_bTestPicker = false;
s32		CMapZoneManager::m_CurrentZoneID = -1;
s32		CMapZoneManager::m_CurrentZoneAreaID = -1;
s32		CMapZoneManager::m_CurrentPointID = -1;
char	CMapZoneManager::m_ZoneName[128];

// Mouse stuff
Vector3 CMapZoneManager::m_vMousePos;
Vector3 CMapZoneManager::m_vMouseNormal;
bool	CMapZoneManager::m_bMouseHasPosition = false;
int		CMapZoneManager::m_MouseScreenX;
int		CMapZoneManager::m_MouseScreenY;
int		CMapZoneManager::m_MouseLeftClickStartX;
int		CMapZoneManager::m_MouseLeftClickStartY;
bool	CMapZoneManager::m_bLeftButtonPressed;
bool	CMapZoneManager::m_bLeftButtonHeld = false;
bool	CMapZoneManager::m_bRightButtonPressed;
bool	CMapZoneManager::m_bRightButtonHeld = false;

// Cone stuff
float	CMapZoneManager::m_ConeRadius = 0.1f;
float	CMapZoneManager::m_ConeHeight = 0.2f;

// Stuff for updating RAG

// Widgets
bkBank *CMapZoneManager::ms_pBank = NULL;
bkButton *CMapZoneManager::ms_pCreateButton = NULL;

// Zone area slider
int		CMapZoneManager::m_CurrentZoneAreaSliderSelection = -1;
bkSlider *CMapZoneManager::m_ZoneAreaSliderHandle = NULL;

// Zone Combo Box
bkCombo *CMapZoneManager::m_ZoneComboHandle;
atArray<const char *> CMapZoneManager::m_ZoneNames;
int		CMapZoneManager::m_ComboZoneSelection;

// Position Text Box
char	CMapZoneManager::m_PointInfo[128];
bkText *CMapZoneManager::m_pPointInfoTextHandle;

void	CMapZoneManager::InitWidgets()
{
	if(ms_pBank)
	{
		ShutdownBank();
	}

	// Create the MapZones bank
	ms_pBank = &BANKMGR.CreateBank("MapZones", 0, 0, false);
	if(Verifyf(ms_pBank, "Failed to create MapZones bank"))
	{
		ms_pCreateButton = ms_pBank->AddButton("Create MapZone widgets", &CMapZoneManager::CreateBank);
	}
}

void CMapZoneManager::CreateBank()
{
	Assertf(ms_pBank, "MapZone bank needs to be created first");
	Assertf(ms_pCreateButton, "MapZone bank needs to be created first");

	ms_pCreateButton->Destroy();
	ms_pCreateButton = NULL;

	bkBank* pBank = ms_pBank;

	pBank->AddToggle("Enable Editing", &CMapZoneManager::m_bEditing);
	pBank->AddText("New Zone Name", m_ZoneName, sizeof(m_ZoneName) - 1, false);
	pBank->AddButton("Create New Zone", AddZone);

	pBank->AddSeparator("ZoneSelectionCombo");

	RebuildZoneNamesArray();

	m_ZoneComboHandle = pBank->AddCombo("Current Zone", &m_ComboZoneSelection, m_ZoneNames.GetCount(), m_ZoneNames.GetElements(), CurrentZoneChanged);
	pBank->AddButton("Delete Current Zone", DeleteZone);

	pBank->AddSeparator("Zone Area Tools");

	pBank->AddButton("Create New Zone Area", AddZoneArea);
	m_ZoneAreaSliderHandle = pBank->AddSlider("Current Area", &m_CurrentZoneAreaSliderSelection, -1, -1, 1, CurrentZoneAreaChanged );
	pBank->AddButton("Delete Zone Area", DeleteZoneArea);

	pBank->AddSeparator("Zone Point Tools");

	m_pPointInfoTextHandle = pBank->AddText("Point Position (x,y,z)", m_PointInfo, sizeof(m_PointInfo) - 1, false);
	pBank->AddButton("Set Point Position on selected point", SetPointPosition);

	pBank->AddButton("Delete selected point", DeletePoint);

	pBank->AddSeparator("Load / Save");
	pBank->AddButton("Load", LoadMapZones);
	pBank->AddButton("Save", SaveMapZones);

	pBank->AddSeparator("Zone Area Test Tools");

	pBank->AddToggle("Enable Test Picker", &CMapZoneManager::m_bTestPicker);


	ResetEditor();
}

void CMapZoneManager::ShutdownBank()
{
	if(ms_pBank)
	{
		BANKMGR.DestroyBank(*ms_pBank);
	}
	ms_pBank = NULL;
	ms_pCreateButton = NULL;
}


void CMapZoneManager::RebuildZoneNamesArray()
{
	const int numZones = m_MapZones.GetNumZones();

	m_ZoneNames.Reset();
	m_ZoneNames.Reserve(numZones + 1);
	m_ZoneNames.Append() = "No Zone";
	for(int i=0;i<numZones;i++)
	{
		const char* name = m_MapZones.GetZone(i).GetName();
		m_ZoneNames.Push( name );
	}
}

void CMapZoneManager::CurrentZoneChanged()
{
	ResetEditor();
	m_CurrentZoneID = m_ComboZoneSelection-1;
	UpdateZoneAreaSlider();
}

s32	CMapZoneManager::FindNamedZoneID(char *pName)
{
	for(int i=0;i<m_MapZones.GetNumZones();i++ )
	{
		CMapZone &currentZone = m_MapZones.GetZone(i);
		if( strcmp(currentZone.GetName(), pName) == 0)
		{
			return i;
		}
	}
	return -1;
}


void CMapZoneManager::UpdateZoneComboBox()
{
	// Update the combo box
	RebuildZoneNamesArray();
	m_ComboZoneSelection = m_CurrentZoneID + 1;
	if(m_ZoneComboHandle)
	{
		m_ZoneComboHandle->UpdateCombo("Current Zone", &m_ComboZoneSelection, m_ZoneNames.GetCount(), m_ZoneNames.GetElements(), CurrentZoneChanged);
	}

	UpdateZoneAreaSlider();
}


void CMapZoneManager::AddZone()
{
	// Do nothing if we're not editing
	if( m_bEditing )
	{
		if( FindNamedZoneID(m_ZoneName) == -1 )
		{
			ResetEditor();
			CMapZone NewZone;
			NewZone.SetName(m_ZoneName);
			m_MapZones.GetZones().PushAndGrow(NewZone);
			m_CurrentZoneID = m_MapZones.GetNumZones()-1;

			// Update the combo box to reflect the new zone
			UpdateZoneComboBox();
		}
		else
		{
			//Pop up a message box saying you can't have two named the same
			char buff[1024];
			formatf(buff, "A Zone called '%s' already exists, the names must be unique.", m_ZoneName);
			bkMessageBox("MapZone Editor Error", buff, bkMsgOk, bkMsgError);
		}
	}
}

void CMapZoneManager::DeleteZone()
{
	// Do nothing if we're not editing
	if( m_bEditing && m_CurrentZoneID >= 0 )
	{
		m_MapZones.GetZones().Delete(m_CurrentZoneID);

		// ResetEditor()
		m_CurrentZoneID = -1;
		m_CurrentZoneAreaID = -1;
		m_CurrentPointID = -1;

		// Update the combo box to reflect the removal of this zone
		UpdateZoneComboBox();
	}
}



void CMapZoneManager::CurrentZoneAreaChanged()
{
	m_CurrentZoneAreaID = (s32)m_CurrentZoneAreaSliderSelection;
	m_CurrentPointID = -1;
}

void CMapZoneManager::UpdateZoneAreaSlider()
{
	if(m_ZoneAreaSliderHandle)
	{
		if( m_CurrentZoneID >= 0 )
		{
			s32 nAreas = m_MapZones.GetZone(m_CurrentZoneID).GetZoneAreaCount();
			m_ZoneAreaSliderHandle->SetRange(-1, (float)(nAreas-1) );
			m_CurrentZoneAreaSliderSelection = m_CurrentZoneAreaID;
		}
		else
		{
			m_ZoneAreaSliderHandle->SetRange(-1, -1 );
			m_CurrentZoneAreaSliderSelection = m_CurrentZoneAreaID;
		}
	}
}


void CMapZoneManager::AddZoneArea()
{
	if( m_bEditing && m_CurrentZoneID >= 0 )
	{

		m_CurrentZoneAreaID = m_MapZones.GetZone(m_CurrentZoneID).AddZoneArea();
		m_CurrentPointID = -1;

		// Update the Slider to reflect the adding of this zone
		UpdateZoneAreaSlider();
	}
}

void CMapZoneManager::DeleteZoneArea()
{
	if( m_bEditing && m_CurrentZoneID >= 0 && m_CurrentZoneAreaID >=0 )
	{
		m_MapZones.GetZone(m_CurrentZoneID).DeleteZoneArea(m_CurrentZoneAreaID);
		m_CurrentZoneAreaID = -1;

		// Update the Slider to reflect the deletion of this zone
		UpdateZoneAreaSlider();
	}
}


// Sets the current points position as text in the text box,can be overwritten
void CMapZoneManager::UpdatePointPositionTextBox()
{
	if( m_bEditing && m_CurrentZoneID >= 0 && m_CurrentZoneAreaID >= 0 && m_CurrentPointID >= 0  )
	{
		Vector3 &position = m_MapZones.GetZone(m_CurrentZoneID).GetZoneArea(m_CurrentZoneAreaID).GetZoneAreaPoint(m_CurrentPointID);

		sprintf(m_PointInfo, "%.4f,%.4f,%.4f", position.x, position.y, position.z );
		m_pPointInfoTextHandle->SetString(m_PointInfo);
	}
}


// Sets the point position from the text in the point position text box
void CMapZoneManager::SetPointPosition()
{
char PositionLine[128];

	if( m_bEditing && m_CurrentZoneID >= 0 && m_CurrentZoneAreaID >= 0 && m_CurrentPointID >= 0 )
	{

		Vector3 vecPos(VEC3_ZERO);

		int nItems = 0;

		//Copy the string
		strncpy(PositionLine, m_PointInfo, 128);

		// Get the locations to store the float values into.
		float* apVals[3] = {&vecPos.x, &vecPos.y, &vecPos.z };

		// Parse the line.
		char* pToken = NULL;
		const char* seps = " ,\t";
		pToken = strtok(PositionLine, seps);
		while(pToken)
		{
			// Try to read the token as a float.
			int success = sscanf(pToken, "%f", apVals[nItems]);
			Assertf(success, "Unrecognized token '%s' in position line.",pToken);
			if(success)
			{
				++nItems;
				Assertf((nItems <= 3), "Too many tokens in position line." );
			}
			pToken = strtok(NULL, seps);
		}

		// Mouse is moving and we've got a selected point, update it
		CMapZone &currentZone = m_MapZones.GetZone(m_CurrentZoneID);
		CMapZoneArea &currentArea = currentZone.GetZoneArea(m_CurrentZoneAreaID);
		currentArea.UpdateZoneAreaPoint( m_CurrentPointID, vecPos );
		currentZone.MakeZoneBoundBox();
	}
}


void CMapZoneManager::DeletePoint()
{
	if( m_bEditing && m_CurrentZoneID >= 0 && m_CurrentZoneAreaID >= 0 && m_CurrentPointID >= 0 )
	{
		CMapZone &currentZone = m_MapZones.GetZone(m_CurrentZoneID);
		CMapZoneArea &currentArea = currentZone.GetZoneArea(m_CurrentZoneAreaID);
		currentArea.DeleteZoneAreaPoint(m_CurrentPointID);
		m_CurrentPointID = -1;
		currentZone.MakeZoneBoundBox();
	}
}

// Now unused.
bool CMapZoneManager::LinesIntersect2D( Vector3 &line1Start, Vector3 &line1End, Vector3 &line2Start, Vector3 &line2End, Vector3 &result)
{
	float denom  = (line2End.y - line2Start.y) * (line1End.x - line1Start.x) - (line2End.x - line2Start.x) * (line1End.y - line1Start.y);
	float numera = (line2End.x - line2Start.x) * (line1Start.y - line2Start.y) - (line2End.y - line2Start.y) * (line1Start.x - line2Start.x);
	float numerb = (line1End.x - line1Start.x) * (line1Start.y - line2Start.y) - (line1End.y - line1Start.y) * (line1Start.x - line2Start.x);

	// Are the lines parallel
	if (fabs(denom) < FLT_EPSILON)
	{
		return false;
	}

	float mua = numera / denom;
	float mub = numerb / denom;
	if (mua < 0.0f || mua > 1.0f || mub < 0.0f || mub > 1.0f)
	{
		return false;
	}

	result.x = line1Start.x + mua * (line1End.x - line1Start.x);
	result.y = line1Start.y + mua * (line1End.y - line1Start.y);

	return true;
}


void CMapZoneManager::ResetEditor()
{
	m_CurrentZoneID = -1;
	m_CurrentZoneAreaID = -1;
	m_CurrentPointID = -1;
}


void	CMapZoneManager::UpdateMouseButtons()
{
	// first off get the mouse inputs irrespective of what we are going to do with them
	void *entity;
	m_bMouseHasPosition = CDebugScene::GetWorldPositionUnderMouse(m_vMousePos, ArchetypeFlags::GTA_ALL_MAP_TYPES, &m_vMouseNormal, &entity);

	// Get the raw mouse position.
	m_MouseScreenX = ioMouse::GetX();
	m_MouseScreenY = ioMouse::GetY();
	m_bLeftButtonPressed = false;
	m_bRightButtonPressed = false;

	m_MouseLeftClickStartX = 0;
	m_MouseLeftClickStartY = 0;

	if( ioMouse::GetPressedButtons() & ioMouse::MOUSE_LEFT )
	{
		m_MouseLeftClickStartX = m_MouseScreenX;
		m_MouseLeftClickStartY = m_MouseScreenY;
		m_bLeftButtonPressed = true;
		m_bLeftButtonHeld = true;
	}
	if( m_bLeftButtonHeld && (ioMouse::GetButtons() & ioMouse::MOUSE_LEFT)==0 )
	{
		m_bLeftButtonHeld = false;
	}

	if( ioMouse::GetPressedButtons() & ioMouse::MOUSE_RIGHT )
	{
		m_bRightButtonPressed = true;
		m_bRightButtonHeld = true;
	}
	if( m_bRightButtonHeld && (ioMouse::GetButtons() & ioMouse::MOUSE_RIGHT)==0 )
	{
		m_bRightButtonHeld = false;
	}

}

s32	CMapZoneManager::FindClosestZonePoint(Vector3 &position, float range)
{
	float minT = FLT_MAX;
	s32	 foundPoint = -1;

	if( m_CurrentZoneID >= 0 && m_CurrentZoneAreaID >= 0 )
	{
		Vector3 mouseScreen, mouseFar;
		CDebugScene::GetMousePointing(mouseScreen, mouseFar, false);

		if(mouseScreen==mouseScreen)	// nan check
		{
			// Extend the ray slightly to account for objects near the floor
			Vector3	dV = position - mouseScreen;
			dV.Normalize();
			Vector3 endPos = position + (range * dV);

			// Search through the points
			CMapZone &currentZone = m_MapZones.GetZone(m_CurrentZoneID);
			CMapZoneArea &currentArea = currentZone.GetZoneArea(m_CurrentZoneAreaID);

			const int numPoints = currentArea.GetZoneAreaPointCount();
			for(int i = 0; i < numPoints; i++)
			{
				float distance2, t;
				Vector3 &point = currentArea.GetZoneAreaPoint(i);

				float pointScale = GetDistanceScaleForPoint(point);
				float scaledRange = range * pointScale;
				float bestRange2 = scaledRange*scaledRange;

				//mayaCurve::NearestPointOnLine( mouseScreen, endPos, point, t, &distance2);
				ScalarV tVal;
				const Vec3V testPoint = VECTOR3_TO_VEC3V(point);
				const Vec3V deltaV = Subtract(VECTOR3_TO_VEC3V(endPos), VECTOR3_TO_VEC3V(mouseScreen));
				const Vec3V pointOnLine = geomPoints::FindClosestPointAndTValSegToPoint(VECTOR3_TO_VEC3V(mouseScreen), deltaV, testPoint, tVal);
				t = tVal.Getf();
				distance2 = DistSquared(testPoint, pointOnLine).Getf();

				if( (distance2 <= bestRange2 ) && ( t < minT ) )
				{
					minT = t; 
					foundPoint = i;
				}
			}		
		}
	}
	return foundPoint;
}

//Generate a scale for the rendering and picking of points based on distance from the camera
float	CMapZoneManager::GetDistanceScaleForPoint(Vector3 &point)
{
	const grcViewport* viewPort = gVpMan.GetCurrentGameGrcViewport();
	Vector3 vCamPos = RCC_MATRIX34(viewPort->GetCameraMtx()).d;
	// Min distance is 1m

	float distance = (point - vCamPos).Mag();
	distance*= 0.25f;
	if(distance < 1.0f)
	{
		distance = 1.0f;
	}

	return distance;
}

void	CMapZoneManager::UpdateEditor()
{
	if(m_bEditing)
	{
		UpdateMouseButtons();

		// Don't do anything if we have no zone selected
		if(m_CurrentZoneID >= 0) 
		{
			CMapZone &currentZone = m_MapZones.GetZone(m_CurrentZoneID);

			if(m_CurrentZoneAreaID >= 0)
			{
				CMapZoneArea &currentArea = currentZone.GetZoneArea(m_CurrentZoneAreaID);

				// Right button press creates a new point
				if(m_bRightButtonPressed && m_bMouseHasPosition)
				{
					// Create a new point
					m_CurrentPointID = currentArea.AddZoneAreaPoint(m_vMousePos);
					UpdatePointPositionTextBox();
					currentZone.MakeZoneBoundBox();
				}
				else if(m_bLeftButtonPressed && m_bMouseHasPosition)
				{
					// See if we're picking up an existing point 
					m_CurrentPointID = FindClosestZonePoint(m_vMousePos, m_ConeRadius);
					UpdatePointPositionTextBox();
				}
				else if (m_bLeftButtonHeld && m_CurrentPointID>=0 )
				{
					// left held 
					if (m_MouseScreenX != m_MouseLeftClickStartX || m_MouseScreenY != m_MouseLeftClickStartY )
					{
						// Mouse is moving and we've got a selected point, update it
						currentArea.UpdateZoneAreaPoint( m_CurrentPointID, m_vMousePos );
						UpdatePointPositionTextBox();
						currentZone.MakeZoneBoundBox();
					}
				}

				// Draw the current Zone editor points (if any)
				DrawMapZoneAreaEditPoints(m_CurrentZoneID, m_CurrentZoneAreaID);
			}

			// Draw the Zone BB
			currentZone.DrawZoneBoundBox();

			// Draw the convex hulls for this area (draw the current editing zone first)
			if( m_CurrentZoneAreaID >=0 )
			{
				currentZone.GetZoneArea(m_CurrentZoneAreaID).DrawHull( true );
			}
			// Now draw the others
			for(int i=0;i<currentZone.GetZoneAreaCount();i++)
			{
				if( i != m_CurrentZoneAreaID)
				{
					currentZone.GetZoneArea(i).DrawHull( false );
				}
			}
		}
	}
	else if(m_bTestPicker)
	{
		UpdateMouseButtons();

		if(m_bLeftButtonPressed && m_bMouseHasPosition)
		{
			atHashString result = GetZoneAtCoords(m_vMousePos);
			Displayf("Clicked point is part of zone %s\n", result.GetCStr());
		}

		// Draw all the zones
		for(int i=0;i<m_MapZones.GetNumZones();i++)
		{
			CMapZone &Zone = m_MapZones.GetZone(i);

			for(int j=0;j<Zone.GetZoneAreaCount();j++)
			{
				Zone.GetZoneArea(j).DrawHull(false);
			}
		}
	}
}


void CMapZoneManager::SaveMapZones()
{
char fileName[256];

	const char* levelName = CScene::GetLevelsData().GetFriendlyName(CGameLogic::GetRequestedLevelIndex());
	sprintf(fileName, "common:/data/levels/%s/MapZones", levelName);
	INIT_PARSER;
	PARSER.SaveObject(fileName, "xml", &m_MapZones, parManager::XML);
}

void CMapZoneManager::DrawMapZoneAreaEditPoints(s32 zoneID, s32 areaID)
{
	if(zoneID >= 0 && areaID >= 0)
	{
		CMapZone &currentZone = m_MapZones.GetZone(zoneID);
		CMapZoneArea &currentArea = currentZone.GetZoneArea(areaID);

		for(int i=0;i<currentArea.GetZoneAreaPointCount();i++)
		{
			Vector3 &currentPoint = currentArea.GetZoneAreaPoint(i);

			float pointScale = GetDistanceScaleForPoint(currentPoint);

			const Vec3V vPos = VECTOR3_TO_VEC3V(currentPoint);
			const Vec3V vPos2 = vPos + Vec3V(0.0f,0.0f,m_ConeHeight*pointScale);

			Color32	colour = Color_blue;
			if( i == m_CurrentPointID )
			{
				colour = Color_red;
			}
			grcDebugDraw::Cone(vPos, vPos2, m_ConeRadius*pointScale, colour, true);
		}
	}
}

// Copies the convex hull points into the editor array, so they can be edited after load
void	CMapZoneManager::SetEditPointsFromHulls()
{
	for(int i=0;i<m_MapZones.GetNumZones();i++)
	{
		CMapZone &currentZone = m_MapZones.GetZone(i);

		for(int j=0;j<currentZone.GetZoneAreaCount();j++)
		{
			CMapZoneArea &currentArea = currentZone.GetZoneArea(j);
			currentArea.CreateEditPointsFromHull();
		}
	}
}


/////////////////////////////////////////////////////
//	CConvexHull2D
/////////////////////////////////////////////////////

bool	CConvexHull2D::BuildConvexHull( atArray<Vector3> &InputData, atArray<Vector3> &OutputVerts )
{
	m_InputVerts.Reset();
	OutputVerts.Reset();

	AddVertices(InputData);

	return ConstructHull(OutputVerts);
}

bool	CConvexHull2D::AddVertices(atArray<Vector3> &VertData)
{
	for(int i=0; i<VertData.size(); i++)
	{
		THullVert thisVert;
		thisVert.m_vVert = VertData[i];
		thisVert.m_fAngle = 0.0f;
		thisVert.m_bIsInHull = false;
		m_InputVerts.PushAndGrow(thisVert);
	}
	return true;
}


int CConvexHull2D::FindMinVertex()
{
	int iStartVertex = -1;
	Vector3 vStartVertex(FLT_MAX, FLT_MAX, FLT_MAX);

	for(int i=0; i<m_InputVerts.size(); i++)
	{
		// Lower Y?
		if(m_InputVerts[i].m_vVert.y < vStartVertex.y)
		{
			vStartVertex = m_InputVerts[i].m_vVert;
			iStartVertex = i;
		}
		// If same Y, then select the one with the lower X
		else if(m_InputVerts[i].m_vVert.y == vStartVertex.y)
		{
			if(m_InputVerts[i].m_vVert.x < vStartVertex.x)
			{
				vStartVertex = m_InputVerts[i].m_vVert;
				iStartVertex = i;
			}
		}
	}
	return iStartVertex;
}


bool CConvexHull2D::ConstructHull(atArray<Vector3> &OutputVerts)
{
	if(m_InputVerts.size() < 3)
		return false;

	int iStartVertex = FindMinVertex();
	Assert(iStartVertex != -1);
	if(iStartVertex==-1)
		return false;

	THullVert *pFirstHullVert = &m_InputVerts[iStartVertex];
	pFirstHullVert->m_bIsInHull = true;

	Vector2 vLastHullEdgeVec(1.0f, 0.0f);
	THullVert *pLastHullEdgeVert = pFirstHullVert;
	THullVert *pMostColinearVert = NULL;
	Vector2		vBestAngleEdgeVec;

	OutputVerts.PushAndGrow(pFirstHullVert->m_vVert);

	bool bHullClosed = false;

	while(!bHullClosed)
	{
		float fMaxAngleSoFar = -1.0f;

		for(int i=0; i<m_InputVerts.size(); i++)
		{
			if(!m_InputVerts[i].m_bIsInHull || (&m_InputVerts[i] == pFirstHullVert && OutputVerts.size() > 1) )
			{
				Vector2 vToVertex = Vector2(m_InputVerts[i].m_vVert.x,m_InputVerts[i].m_vVert.y) - Vector2(pLastHullEdgeVert->m_vVert.x, pLastHullEdgeVert->m_vVert.y);

				if(vToVertex.Mag2())
					vToVertex.Normalize();
				m_InputVerts[i].m_fAngle = DotProduct(vToVertex, vLastHullEdgeVec);

				if(m_InputVerts[i].m_fAngle > fMaxAngleSoFar)
				{
					fMaxAngleSoFar = m_InputVerts[i].m_fAngle;
					pMostColinearVert = &m_InputVerts[i];
					vBestAngleEdgeVec = vToVertex;
				}
			}
		}

		Assert(pMostColinearVert);
		if(pMostColinearVert==pFirstHullVert)
			break;

		OutputVerts.PushAndGrow(pMostColinearVert->m_vVert);

		pMostColinearVert->m_bIsInHull = true;
		vLastHullEdgeVec = vBestAngleEdgeVec;
		pLastHullEdgeVert = pMostColinearVert;
	}
	return true;
}

#endif	//__BANK
