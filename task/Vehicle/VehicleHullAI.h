#ifndef VEHICLE_HULL_AI_H
#define VEHICLE_HULL_AI_H

//********************************************************************************
// Filename : VehicleHull.h
// Description : Classes for creating 2d convex hulls for complex vehicles. Peds
// can then navigate around the hulls to avoid doors, missle hardpoints, etc
// Author : James Broad (22/6/09)
//

// Rage includes

// Framework headers
////#include "fwmaths/Maths.h"

// Game headers
#include "ModelInfo/PedModelInfo.h"
#include "ModelInfo/VehicleModelInfoEnums.h"
#include "Renderer/HierarchyIds.h"
#include "Scene/RegdRefTypes.h"

// Forwards declarations
namespace rage { class Matrix34; }
class CVehicle;
class CVehicleHullAI;
class CPointRoute;
//******************************************************************************************
// CConvexHullCalculator2D
// This uses a simple gift-wrapping approach which is more robust than 'Graham Scan' method

class CConvexHullCalculator2D
{
	struct THullVert
	{
		Vector2 m_vVert;
		float m_fAngle;
		bool m_bIsInHull;
	};

public:

	static const int ms_iMaxNumInputVertices = 160;
	static const int ms_iMaxNumHullVertices = 64;

	CConvexHullCalculator2D();
	~CConvexHullCalculator2D();

	static bool AddVertices(const Vector2 * pVerts, const int iNumVerts);
	static bool AddVertices(const Vector3 * pVerts, const int iNumVerts);

	static void Calculate();
	static void Reset();

	static int GetNumInputVerts() { return ms_iNumVertices; }
	static int GetNumHullVerts() { return ms_iNumFinalVerts; }
	static const Vector2 & GetHullVert(const int v) { return ms_FinalVerts[v]; }

protected:

	static int FindMinVertex();
	static void SortVertices();
	static void ConstructHull();

	static int ms_iNumVertices;
	static THullVert * ms_pFirstHullVert;
	static THullVert ms_HullVertices[ms_iMaxNumInputVertices];

	static int ms_iNumFinalVerts;
	static Vector2 ms_FinalVerts[ms_iMaxNumHullVertices];
};

//*********************************************************************************
// CVehicleHullCalculator
// A class for calculating the convex hull of a specified vehicle, including
// whatever components of the vehicle are necessary.  The convex hull is returned
// as a list of points which can be used to navigate around the vehicle's outline.

class CVehicleHullCalculator
{
	static const int ms_iMaxNumComponents = 40;

public:
	CVehicleHullCalculator();
	~CVehicleHullCalculator();

	static void Reset();
	static void CalculateHullForVehicle(const CVehicle * pVehicle, CVehicleHullAI * pVehicleHull, const float fExpandSize, eHierarchyId * pExcludeIDs=NULL, const float fCullMinZ=-FLT_MAX, const float fCullMaxZ=FLT_MAX);

public:

	// Helper
	static bool IsVehicleLandedOnBlimp(const CVehicle * pVehicle);

protected:

	static void GetComponentsForCar(const CVehicle& rVeh);
	static void GetComponentsForBike();
	static void GetComponentsForHeli();
	static void GetComponentsForPlane();

	static s32 GenerateExcludeListForSpecificModel(const CVehicle * pVehicle, eHierarchyId * pExcludeIDs, const float fCullMinZ, const float fCullMaxZ);

	static bool Create2dOrientatedBoxesForGadgets(const CVehicle* pVehicle, float fExpandSize);
	static bool Create2dOrientatedBoxesForComponentGroup(const eHierarchyId eComponent, const float fExpandSize);
	static bool Create2dOrientatedBoxesFoSpecificModels(const CVehicle * pVehicle, const float fExpandSize);
	static bool ShouldSubBoundBeCulled(const Vector3 & vMin, const Vector3 & vMax, const Matrix34 & mSubBound);
	static void ProjectBoundingBoxOntoZPlane(const Vector3 &vMin, const Vector3 &vMax, const Matrix34 & mOrientation, Vector2 * pOutVerts);

	static const CVehicle * m_pVehicle;

	// Any bounds entirely below than this Z will be culled
	static float m_fCullMinZ;
	// Any bounds entirely above than this Z will be culled
	static float m_fCullMaxZ;

	static int ms_iNumComponents;
	static eHierarchyId ms_iComponents[ms_iMaxNumComponents];
};

//*************************************************************************************************
// CVehicleHullAI
// A class which contains the 2d convex hull for a vehicle, and has a number of helper functions
// for testing points and lines against this hull.

class CVehicleHullAI
{
	friend class CVehicleHullCalculator;

public:
	CVehicleHullAI(CVehicle * pVehicle);
	~CVehicleHullAI();

	void Init(const float fExpandSize, eHierarchyId * pExcludeIDs=NULL, const float fCullMinZ=-FLT_MAX, const float fCullMaxZ=FLT_MAX);
	void Scale(const Vector2 & vScale);

	inline int GetNumVerts() const { return m_iNumVerts; }
	inline const Vector2 & GetVert(const int v) const { return m_vHullVerts[v]; }
	inline const Vector2 & GetCentre() const { return m_vCentre; }

	bool LiesInside(const Vector2 & vPos) const;
	bool Intersects(const Vector2 & vStart, const Vector2 & vEnd) const;
	bool MoveOutside(Vector2 & vPos, const float fAmount=ms_fMoveOutsideEps);
	bool MoveOutside(Vector2 & vPos, const Vector2 & vDirection, const float fAmount=ms_fMoveOutsideEps);

	bool CalcShortestRouteToTarget(const CPed * pPed, const Vector2 & vStart, const Vector2 & vTarget, CPointRoute * pRoute, const float fZHeight=0.0f, const u32 iLosTestFlags=0, const float fRadius=0.01f, s32 * piHullDirection=NULL, const float fNormalMinZ=10.0f BANK_ONLY(, bool bLogHitEntities = false));

private:

	struct TEndPointInfo
	{
		TEndPointInfo(const Vector3 & vPos)
		{
			m_vPosition = vPos;
			m_vClosestPositionOnHull = vPos;
			m_fClosestDistSqr = FLT_MAX;
			m_iClosestSegment = -1;
		}
		Vector3 m_vPosition;
		Vector3 m_vClosestPositionOnHull;
		float m_fClosestDistSqr;
		int m_iClosestSegment;
	};
	static dev_float ms_fMoveOutsideEps;
	static dev_float ms_fCloseRouteLength;

	Vector2 m_vHullVerts[CConvexHullCalculator2D::ms_iMaxNumHullVertices];
	Vector2 m_vCentre;
	int m_iNumVerts;
	RegdVeh m_pVehicle;

	bool TestIsRouteClear(const CPed * pPed, CPointRoute * pRoute, const u32 iLosTestFlags, const float fRadius, float & fDistToObstruction, const float fNormalMinZ BANK_ONLY(, bool bLogHitEntities = false)) const;
};

#if __BANK

class CVehicleComponentVisualiser
{
public:
	CVehicleComponentVisualiser();
	~CVehicleComponentVisualiser();

	static void Process();
	static void VisualiseComponents(const CVehicle * pVehicle, eHierarchyId eComponent, bool bDrawBoundingBox);
	static void VisualiseComponents(const CVehicle * pVehicle, s32 iBoneIndex, bool bDrawBoundingBox);
	static void DrawPolyhedronBound(const CVehicle * pVehicle, const phBound * pBound, const Matrix34 & mat, Color32 iCol, bool bDrawBoundingBox);

	static void InitWidgets();

	static bool m_bEnabled;
	static bool m_bInitialised;
	static bool m_bDrawConvexHull;
	static bool m_bDebugIgnorePedZ;
	static bool m_bDebugDrawBoxTests;
	static bkCombo * m_pComponentsCombo;
	static int m_iSelectedComponentComboIndex;
	static const eHierarchyId m_iComponents[];
	static const char * m_pComponentNames[];
};

#endif	// __BANK

#endif // VEHICLE_HULL_H
