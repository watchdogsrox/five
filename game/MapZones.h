#ifndef	__MAP_ZONES_H
#define	__MAP_ZONES_H

// rage headers
#include "parser/manager.h"
#include "parser/macros.h"
#include "atl/array.h"
#include "atl/hashstring.h"
#include "atl/string.h"
#include "atl/singleton.h"
#include "string/string.h"
#include "system/FileMgr.h"
#include "bank/bank.h"
#include "bank/button.h"
#include "bank/combo.h"
#include "bank/text.h"
#include "bank/slider.h"
#include "bank/bkmgr.h"
#include "bank/msgbox.h"
#include "spatialdata/aabb.h"

#include "scene/DataFileMgr.h"

#if	__BANK

// Shamelessly robbed from James Broads CConvexHullCalculator2D (VehicleHullAI.h)
// and knobbled to better suit my needs. 
//	Fixed an issue where hull construction fails on some areas with only 3 points.
class CConvexHull2D
{
	struct THullVert
	{
		Vector3	m_vVert;
		float	m_fAngle;
		bool	m_bIsInHull;
	};

public:
	CConvexHull2D() {}
	~CConvexHull2D() {}

	bool	BuildConvexHull( atArray<Vector3> &InputData, atArray<Vector3> &OutputData );

private:

	bool	AddVertices(atArray<Vector3> &VertData);
	int		FindMinVertex();
	bool	ConstructHull(atArray<Vector3> &OutputVerts);

	atArray <THullVert> m_InputVerts;
};

#endif	//__BANK



class CMapZoneArea
{
public:
	friend class CMapZoneManager;

	bool	IsPointInArea(const Vector3 &vPos) const;

#if __BANK

	u32		GetZoneAreaPointCount()		
	{
		return m_ZoneAreaPoints.size(); 
	}

	Vector3 &GetZoneAreaPoint(u32 idx)
	{ 
		return m_ZoneAreaPoints[idx];
	}

	u32		AddZoneAreaPoint(Vector3 &point)
	{
		u32	idx = m_ZoneAreaPoints.size();
		m_ZoneAreaPoints.PushAndGrow(point);
		m_ConvexHullMaker.BuildConvexHull(m_ZoneAreaPoints, m_ZoneAreaHull);
		return idx;
	}

	void	UpdateZoneAreaPoint(u32 idx, Vector3 &newPos)
	{
		m_ZoneAreaPoints[idx] = newPos;
		m_ConvexHullMaker.BuildConvexHull(m_ZoneAreaPoints, m_ZoneAreaHull);
	}

	void	DeleteZoneAreaPoint(u32 idx)
	{
		m_ZoneAreaPoints.Delete(idx);
		m_ConvexHullMaker.BuildConvexHull(m_ZoneAreaPoints, m_ZoneAreaHull);
	}

	void	CreateEditPointsFromHull()
	{
		m_ZoneAreaPoints.Reset();
		m_ZoneAreaPoints = m_ZoneAreaHull;
	}

	u32		GetHullPointCount()
	{
		return m_ZoneAreaHull.size();
	}

	Vector3 &GetHullPoint(u32 idx)
	{
		return m_ZoneAreaHull[idx];
	}

	void	DrawHull(bool isCurrentlySelected);

#endif	//BANK

private:

	// Some data to hold the convex hulls, this is all that should be saved
	atArray <Vector3>	m_ZoneAreaHull;

#if __BANK
	// The editable points that specify the the zone
	atArray <Vector3>	m_ZoneAreaPoints;		
	CConvexHull2D		m_ConvexHullMaker;
#endif	//BANK


	PAR_SIMPLE_PARSABLE;
};

class CMapZone
{
public:

	void	SetName(char *pName)
	{ 
		m_Name = atString(pName);
	}
	const char	*GetName()
	{
		return m_Name.c_str();
	}
	u32	GetZoneAreaCount()
	{
		return m_ZoneAreas.size();
	}
	CMapZoneArea	&GetZoneArea(u32 idx)
	{
		return m_ZoneAreas[idx];
	}

	// Only checks 2D at the moment
	bool	IsInBoundBox(Vector3 &vPos)
	{
		return m_BoundBox.ContainsPointFlat(VECTOR3_TO_VEC3V(vPos));
	}

	bool	IsPointInZone(Vector3 &point);

#if __BANK

	void MakeZoneBoundBox();

	void DrawZoneBoundBox();

	u32	AddZoneArea()
	{
		u32	idx = m_ZoneAreas.size();
		CMapZoneArea NewZoneArea;
		m_ZoneAreas.PushAndGrow(NewZoneArea);
		return idx;
	}

	void DeleteZoneArea(u32 idx)
	{
		m_ZoneAreas.Delete(idx);
	}

#endif	//__BANK

private:

	atString				m_Name;				// The name of the zone
	atArray <CMapZoneArea>	m_ZoneAreas;		// Multiple areas for each zone

	spdAABB					m_BoundBox;			// The bound box that will encompass the entirety of all the areas (TODO)

	PAR_SIMPLE_PARSABLE;
};


class CMapZonesContainer
{
public:

	void		Reset()
	{
		m_Zones.Reset();
	}

	CMapZone	&GetZone(u32 idx)
	{ 
		return m_Zones[idx]; 
	}

	s32			GetNumZones()
	{
		return m_Zones.size();
	}

	atArray<CMapZone> &GetZones()
	{ 
		return m_Zones;
	}

private:

	atArray<CMapZone> m_Zones;
	PAR_SIMPLE_PARSABLE;
};


class CMapZoneManager
{
public:

	static void Init(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);
	static void Update();

	static u32	GetZoneAtCoords(Vector3 &vPos);

private:

	static void PostCreation();
	static void ResetAllZones();
	static void RemoveZone(u32 zoneID);		// Bank Only?
	static void LoadMapZones();

	static CMapZonesContainer	m_MapZones;

#if __BANK

	static bool	m_bEditing;
	static bool	m_bTestPicker;
	static s32	m_CurrentZoneID;
	static s32	m_CurrentZoneAreaID;
	static s32	m_CurrentPointID;
	static char	m_ZoneName[128];

	// Mouse stuff
	static Vector3 m_vMousePos;
	static Vector3 m_vMouseNormal;
	static bool	m_bMouseHasPosition;
	static int	m_MouseScreenX;
	static int	m_MouseScreenY;
	static int	m_MouseLeftClickStartX;
	static int	m_MouseLeftClickStartY;
	static bool	m_bLeftButtonPressed;
	static bool	m_bLeftButtonHeld;
	static bool	m_bRightButtonPressed;
	static bool	m_bRightButtonHeld;

	static bkBank				*ms_pBank;
	static bkButton				*ms_pCreateButton;

	static char					m_PointInfo[128];
	static bkText				*m_pPointInfoTextHandle;

	static int					m_CurrentZoneAreaSliderSelection;
	static bkSlider				*m_ZoneAreaSliderHandle;

	static bkCombo				*m_ZoneComboHandle;						// Zone Combo Box
	static atArray<const char*>	m_ZoneNames;							// For Zone Combo Box
	static int					m_ComboZoneSelection;					// ID after selection via combo box

	static float				m_ConeRadius;
	static float				m_ConeHeight;

	static void		InitWidgets();
	static void		CreateBank();
	static void		ShutdownBank();

	static void		RebuildZoneNamesArray();
	static void		CurrentZoneChanged();
	static s32		FindNamedZoneID(char *pName);
	static void		UpdateZoneComboBox();
	static void		AddZone();
	static void		DeleteZone();

	static void		CurrentZoneAreaChanged();
	static void		UpdateZoneAreaSlider();
	static void		AddZoneArea();
	static void		DeleteZoneArea();

	static void		UpdatePointPositionTextBox();
	static void		SetPointPosition();
	static void		DeletePoint();

	static bool		LinesIntersect2D( Vector3 &line1Start, Vector3 &line1End, Vector3 &line2Start, Vector3 &line2End, Vector3 &result);

	static void		ResetEditor();
	static void		UpdateMouseButtons();
	static s32		FindClosestZonePoint(Vector3 &pos, float range);
	static float	GetDistanceScaleForPoint(Vector3 &point);
	static void		UpdateEditor();
	static void		SaveMapZones();
	static void		DrawMapZoneAreaEditPoints(s32 zoneID, s32 areaID);
	static void		SetEditPointsFromHulls();

#endif	//__BANK

};




#endif	//__MAP_ZONES_H
