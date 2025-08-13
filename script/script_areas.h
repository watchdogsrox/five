#ifndef _SCRIPT_AREAS_H_
	#define _SCRIPT_AREAS_H_

// Rage headers
#include "vector/color32.h"

enum
{
	CALCULATION_ENTITY_IN_AREA = 0,
	CALCULATION_ENTITY_AT_COORD,
	CALCULATION_ENTITY_AT_ENTITY,
	CALCULATION_OBJECT_IN_AREA,
	CALCULATION_LOCATE_OBJECT,
	CALCULATION_PED_IN_AREA,
	CALCULATION_LOCATE_PED,
	CALCULATION_LOCATE_PED_OBJECT,
	CALCULATION_LOCATE_PED_CAR,
	CALCULATION_LOCATE_PED_PED,
	CALCULATION_THIS_PED_SHOOTING_IN_AREA,
	CALCULATION_ANY_PED_SHOOTING_IN_AREA,
	CALCULATION_CAR_IN_AREA,
	CALCULATION_LOCATE_CAR,
	CALCULATION_COLOURED_CYLINDER
};


namespace rage
{
class Vector3;
}

class CScriptAreas
{
public:
	static void Init(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);
	static void Process(void);

	static void NewHighlightImportantArea(float TargetX1, float TargetY1, float TargetZ1, float TargetX2, float TargetY2, float TargetZ2, bool Do3dCheck, int CalculationToUse);

	static void HighlightImportantAngledArea(u32 UNUSED_PARAM(AreaID), float X1, float Y1, float X2, float Y2,
												float X3, float Y3, float X4, float Y4, float CentreZ);

	static bool IsPointInAngledArea(const Vector3& vPoint, Vector3& vAreaStart, Vector3& vAreaEnd, float AreaWidth, bool HighlightArea, bool Do3dCheck, bool bDrawDebug);

	static bool IsAngledAreaInAngledArea(Vector3& vArea1Start, Vector3& vArea1End, float Area1Width, bool Area1Do3dCheck, Vector3& vArea2Start, Vector3& vArea2End, float Area2Width, bool HighlightArea, bool Do3dCheck, bool bDrawDebug);


	static bool IsVecIn3dAngledArea(const Vector3& vecPointToBeChecked, const Vector3& vecFacePoint1, const Vector3& vecFacePoint2, float fDistanceToOppositeFace, bool bDrawDebug);

	static void SetOverrideMarkerColour(int Red, int Green, int Blue, int Alpha);


public:
	static float ms_distThreshNear;
	static float ms_distThreshFar;
	static s32 ms_alphaNear;
	static s32 ms_alphaFar;
	static bool	ms_arrowEnabled;
	static float ms_arrowOffsetZ;
	static float ms_arrowScaleNear;
	static float ms_arrowScaleFar;
	static float ms_cylinderOffsetZ;
	static float ms_cylinderScaleXYNear;
	static float ms_cylinderScaleXYFar;
	static float ms_cylinderScaleZNear;
	static float ms_cylinderScaleZFar;

private:
	static void HighlightScriptArea(Vector3 &vecCentre/*, float fWidth*/, int CalculationToUse);

	static float CalculateZ(float CentreX, float CentreY, float InZ, float Width, float Depth, bool Do3dCheck, int CalculationToUse);

	static bool IsVecIn2dAngledArea(const Vector3& vecPos, float TargetX1, float TargetY1, float TargetX2, float TargetY2, float DistanceFrom1To4, bool bDrawDebug);

private:
	static Color32 OverrideMarkerColour;
	static bool bOverrideMarkerColour;
};

// CLASS : CScriptAngledArea
// PURPOSE : Defines an angled area in which vPt1 and vPt2 are endpoints of a corridor, which is fWidth wide
// To define height of this region the vPt1.z & vPt2.z may be set to define a range.
// This class is suitable if we have a region which may need to be tested multiple times (setup maths is done once)
class CScriptAngledArea
{
public:
	CScriptAngledArea();

	// Set up the area
	// pvMinMax is on optional parameter which if used will return the min/max extents of the area
	// it should point to an array of two vectors : ie. Vector3[2]
	CScriptAngledArea(const Vector3 & vPt1, const Vector3 & vPt2, const float fWidth, Vector3 * pvOutMinMax);
	void Set(const Vector3 & vPt1, const Vector3 & vPt2, const float fWidth, Vector3 * pvOutMinMax);

	// Test a point for containment in the region
	bool TestPoint(const Vector3 & vPos) const;

	void DebugDraw(const Color32 &colour);

	Vector3 m_vPt1;
	Vector3 m_vPt2;
	float m_fWidth;
	Vector3 vEdgePlanes[4];
};


#endif	//	_SCRIPT_AREAS_H_


