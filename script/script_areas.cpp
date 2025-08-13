
#include "script/script_areas.h"

// Framework headers
#include "fwmaths/angle.h"
#include "fwmaths/Vector.h"

// Game headers
#include "frontend/minimap.h"
#include "physics/WorldProbe/worldprobe.h"
#include "scene/world/GameWorld.h"
#include "script/script.h"
#include "script/script_channel.h"
#include "script/script_debug.h"
#include "vfx/misc/Markers.h"

float CScriptAreas::ms_distThreshNear = 10.0f;
float CScriptAreas::ms_distThreshFar = 20.0f;
s32 CScriptAreas::ms_alphaNear = 140;
s32 CScriptAreas::ms_alphaFar = 160;
bool CScriptAreas::ms_arrowEnabled = false;
float CScriptAreas::ms_arrowOffsetZ = 3.1f;
float CScriptAreas::ms_arrowScaleNear = 0.3f;
float CScriptAreas::ms_arrowScaleFar = 0.3f;
float CScriptAreas::ms_cylinderOffsetZ = 1.85f;
float CScriptAreas::ms_cylinderScaleZNear = 0.5f;
float CScriptAreas::ms_cylinderScaleZFar = 1.0f;
float CScriptAreas::ms_cylinderScaleXYNear = 0.6f;
float CScriptAreas::ms_cylinderScaleXYFar = 0.6f;

Color32 CScriptAreas::OverrideMarkerColour = Color32(0, 0, 0, 0);
bool CScriptAreas::bOverrideMarkerColour = false;


void CScriptAreas::Init(unsigned initMode)
{
    if(initMode == INIT_CORE)
    {
    }
    else if(initMode == INIT_SESSION)
    {
	    OverrideMarkerColour = Color32(0, 0, 0, 0);
	    bOverrideMarkerColour = false;
	}
}

void CScriptAreas::Shutdown(unsigned shutdownMode)
{
    if(shutdownMode == SHUTDOWN_CORE)
    {
	}
    else if(shutdownMode == SHUTDOWN_SESSION)
    {
	}
}

void CScriptAreas::Process(void)
{
}

void CScriptAreas::SetOverrideMarkerColour(int Red, int Green, int Blue, int Alpha)
{
	OverrideMarkerColour = Color32(Red, Green, Blue, Alpha);
	bOverrideMarkerColour = true;
}

void CScriptAreas::HighlightScriptArea(Vector3 &vecCentre/*, float fWidth*/, int CalculationToUse)
{
	if (!CGameWorld::FindLocalPlayer())
		return;

	Color32 MarkerColour = CMiniMap_Common::GetColourFromBlipSettings(BLIP_COLOUR_MISSION_MARKER, TRUE);

	if (bOverrideMarkerColour)
	{	//	should only be set for CALCULATION_COLOURED_CYLINDER which is only used by the DRAW_COLOURED_CYLINDER script command
		MarkerColour = OverrideMarkerColour;
		bOverrideMarkerColour = false;
	}

	scriptAssertf(CTheScripts::GetCurrentGtaScriptThread(), "CTheScripts::HighlightScriptArea - can only call this from a script command");


	Vec3V vCentrePos = Vec3V(vecCentre.x, vecCentre.y, vecCentre.z);
	Vector3 playerPos = CGameWorld::FindLocalPlayerCentreOfWorld();
	Vec3V vPlayerPos = RCC_VEC3V(playerPos);
	float distToMarker = DistXY(vCentrePos, vPlayerPos).Getf();

	float t = CVfxHelper::GetInterpValue(distToMarker, ms_distThreshNear, ms_distThreshFar);

	s32 currAlpha = static_cast<s32>(ms_alphaNear + t*(ms_alphaFar-ms_alphaNear));
	float currArrowScale = ms_arrowScaleNear + t*(ms_arrowScaleFar-ms_arrowScaleNear); 
	float currCylinderScaleXY = ms_cylinderScaleXYNear + t*(ms_cylinderScaleXYFar-ms_cylinderScaleXYNear); 
	float currCylinderScaleZ = ms_cylinderScaleZNear + t*(ms_cylinderScaleZFar-ms_cylinderScaleZNear); 

	// arrow
	if (ms_arrowEnabled)
	{
		MarkerInfo_t markerInfo;
		markerInfo.type = MARKERTYPE_ARROW;
		markerInfo.vPos = Vec3V(vecCentre.x, vecCentre.y, vecCentre.z+ms_arrowOffsetZ);
		markerInfo.vDir = Vec3V(V_ZERO);
		markerInfo.vRot = Vec4V(0.0f, PI, 0.0f, 0.0f);
		markerInfo.vScale = Vec3V(currArrowScale, currArrowScale, currArrowScale);
		markerInfo.col = Color32(MarkerColour.GetRed(), MarkerColour.GetGreen(), MarkerColour.GetBlue(), currAlpha);
		markerInfo.bounce = true;
		markerInfo.faceCam = true;
		g_markers.Register(markerInfo);
	}

	// cylinder
	if (CalculationToUse==CALCULATION_ENTITY_AT_COORD)
	{
		MarkerInfo_t markerInfo;
		markerInfo.type = MARKERTYPE_CYLINDER;
		markerInfo.vPos = Vec3V(vecCentre.x, vecCentre.y, vecCentre.z+ms_cylinderOffsetZ);
		markerInfo.vDir = Vec3V(V_ZERO);
		markerInfo.vRot = Vec4V(V_ZERO);
		markerInfo.vScale = Vec3V(currCylinderScaleXY, currCylinderScaleXY, currCylinderScaleZ);
		markerInfo.col = Color32(MarkerColour.GetRed(), MarkerColour.GetGreen(), MarkerColour.GetBlue(), currAlpha/2);
		markerInfo.txdIdx = -1;
		markerInfo.texIdx = -1;
		markerInfo.bounce = false;
		markerInfo.rotate = false;
		markerInfo.faceCam = false;
		markerInfo.clip = false;
		g_markers.Register(markerInfo);
	}
}

float CScriptAreas::CalculateZ(float CentreX, float CentreY, float InZ, float Width, float Depth, bool Do3dCheck, int CalculationToUse)
{
	float ReturnZ = 0.0f;
	if (Do3dCheck)
	{
		switch (CalculationToUse)
		{
			case CALCULATION_ENTITY_IN_AREA :
			case CALCULATION_ENTITY_AT_ENTITY :
			case CALCULATION_OBJECT_IN_AREA :
			case CALCULATION_PED_IN_AREA :
			case CALCULATION_LOCATE_PED_OBJECT :
			case CALCULATION_LOCATE_PED_CAR :
			case CALCULATION_LOCATE_PED_PED :
			case CALCULATION_THIS_PED_SHOOTING_IN_AREA :	//	Do3dCheck is always false for this one so should never get here
			case CALCULATION_ANY_PED_SHOOTING_IN_AREA :	//	Do3dCheck is always true for this one	//	old calculation was ( (TargetZ1 + TargetZ2) * 0.5f)
			case CALCULATION_CAR_IN_AREA :
			case CALCULATION_COLOURED_CYLINDER :
			{
				ReturnZ = InZ;	//	Either the z of the target entity or the lower z of the area cuboid
			}
			break;

			case CALCULATION_ENTITY_AT_COORD :
			case CALCULATION_LOCATE_OBJECT :
			case CALCULATION_LOCATE_PED :
			case CALCULATION_LOCATE_CAR :
			{
				if (Depth < 0.0f)	//	InZ is the midpoint of the area cuboid
				{
					ReturnZ = InZ + Depth;
				}
				else
				{
					ReturnZ = InZ - Depth;
				}
			}
			break;

			default :
				scriptAssertf(0, "CTheScripts::CalculateZ - unexpected value for CalculationToUse");
				break;
		}
	}
	else
	{
//		ReturnZ = WorldProbe::FindGroundZForCoord(CentreX, CentreY);	//	 + 2.0f;	// + 2.0f to make sure shadow is also cast on higher bits

// now the z check starting point from the point given by the level designer
#define MAX_DIFF_TO_LOOK_FOR (1.5f)  // 1.5 metres is the max amount we look down from the original highest point found

		// we need to look for 4 points and use the lowest:
		float fNorth[4];
		if (InZ <= INVALID_MINIMUM_WORLD_Z)
		{
			fNorth[0] = WorldProbe::FindGroundZForCoord(TOP_SURFACE, CentreX, CentreY+Width);
			fNorth[1] = WorldProbe::FindGroundZForCoord(TOP_SURFACE, CentreX, CentreY-Width);
			fNorth[2] = WorldProbe::FindGroundZForCoord(TOP_SURFACE, CentreX+Width, CentreY);
			fNorth[3] = WorldProbe::FindGroundZForCoord(TOP_SURFACE, CentreX-Width, CentreY);
		}
		else
		{
			fNorth[0] = WorldProbe::FindGroundZFor3DCoord(TOP_SURFACE, CentreX, CentreY+Width, InZ);
			fNorth[1] = WorldProbe::FindGroundZFor3DCoord(TOP_SURFACE, CentreX, CentreY-Width, InZ);
			fNorth[2] = WorldProbe::FindGroundZFor3DCoord(TOP_SURFACE, CentreX+Width, CentreY, InZ);
			fNorth[3] = WorldProbe::FindGroundZFor3DCoord(TOP_SURFACE, CentreX-Width, CentreY, InZ);
		}

		// ensure we have the highest we can find:
		float HighestZ = fNorth[0];
		for (s32 i = 1; i < 4; i++)
		{
			HighestZ = rage::Max(fNorth[i], HighestZ);
		}
		ReturnZ = HighestZ;

		// now find the lowest point within MAX_DIFF_TO_LOOK_FOR of the highest point:
		for (s32 i = 0; i < 4; i++)
		{
			if (fNorth[i] > HighestZ-MAX_DIFF_TO_LOOK_FOR)
			{
				ReturnZ = rage::Min(fNorth[i], ReturnZ);
			}
		}
	}

	return ReturnZ;
}

//	coordinates could be two corners or centre and (width, height, depth)
void CScriptAreas::NewHighlightImportantArea(float TargetX1, float TargetY1, float TargetZ1, float TargetX2, float TargetY2, float TargetZ2, bool Do3dCheck, int CalculationToUse)
{
	Vector3 vecCentre = Vector3(0.0f, 0.0f, 0.0f);
	float Width = 0.0f;
	float Depth = 0.0f;

	switch (CalculationToUse)
	{
		case CALCULATION_ENTITY_IN_AREA :
		case CALCULATION_OBJECT_IN_AREA :
		case CALCULATION_PED_IN_AREA :
		case CALCULATION_THIS_PED_SHOOTING_IN_AREA :
		case CALCULATION_ANY_PED_SHOOTING_IN_AREA :
		case CALCULATION_CAR_IN_AREA :
		{
			vecCentre.x = (TargetX1 + TargetX2) * 0.5f;
			vecCentre.y = (TargetY1 + TargetY2) * 0.5f;
			Width = MAX( (TargetX2 - vecCentre.x), (TargetY2 - vecCentre.y) );
			Depth = 0.0f;	//	not required for these cases
		}
		break;

		case CALCULATION_ENTITY_AT_COORD :
		case CALCULATION_ENTITY_AT_ENTITY :
		case CALCULATION_LOCATE_OBJECT :
		case CALCULATION_LOCATE_PED_OBJECT :
		case CALCULATION_LOCATE_PED_CAR :
		case CALCULATION_LOCATE_PED_PED :
		case CALCULATION_LOCATE_PED :
		case CALCULATION_LOCATE_CAR :
		case CALCULATION_COLOURED_CYLINDER :
		{	//	TargetX2, TargetY2, TargetZ2 are actually Width, Height, Depth
			vecCentre.x = TargetX1;
			vecCentre.y = TargetY1;
			Width = MAX( TargetX2, TargetY2 );
			Depth = TargetZ2;
		}
		break;

		default :
			scriptAssertf(0, "CTheScripts::NewHighlightImportantArea - unexpected value for CalculationToUse");
			break;
	}

	vecCentre.z = CalculateZ(vecCentre.x, vecCentre.y, TargetZ1, Width, Depth, Do3dCheck, CalculationToUse);

	HighlightScriptArea(vecCentre/*, Width*/,CalculationToUse);
}

// Name			:	CTheScripts::HighlightImportantAngledArea
// Purpose		:	Highlights an area of the map which is important in a mission
// Parameters	:	A unique ID for the area and it's coordinates
// Returns		:	nothing
void CScriptAreas::HighlightImportantAngledArea(u32 UNUSED_PARAM(AreaID), float X1, float Y1, float X2, float Y2,
												float X3, float Y3, float X4, float Y4, float CentreZ)
{
	Vector3	Temp;

	float Centre1X = (X1 + X2) * 0.5f;
	float Centre1Y = (Y1 + Y2) * 0.5f;
	float Centre2X = (X2 + X3) * 0.5f;
	float Centre2Y = (Y2 + Y3) * 0.5f;
	float Centre3X = (X3 + X4) * 0.5f;
	float Centre3Y = (Y3 + Y4) * 0.5f;
	float Centre4X = (X4 + X1) * 0.5f;
	float Centre4Y = (Y4 + Y1) * 0.5f;

	float MinX = MIN(Centre1X, Centre2X);
	float MaxX = MAX(Centre1X, Centre2X);
	float MinY = MIN(Centre1Y, Centre2Y);
	float MaxY = MAX(Centre1Y, Centre2Y);

	MinX = MIN(MinX, Centre3X);
	MinX = MIN(MinX, Centre4X);

	MaxX = MAX(MaxX, Centre3X);
	MaxX = MAX(MaxX, Centre4X);

	MinY = MIN(MinY, Centre3Y);
	MinY = MIN(MinY, Centre4Y);

	MaxY = MAX(MaxY, Centre3Y);
	MaxY = MAX(MaxY, Centre4Y);

	Temp.x = (MinX + MaxX) * 0.5f;
	Temp.y = (MinY + MaxY) * 0.5f;
//	Temp.z = FindPlayerCoors().z;

	if (CentreZ <= INVALID_MINIMUM_WORLD_Z)
	{
		Temp.z = WorldProbe::FindGroundZForCoord(TOP_SURFACE, Temp.x, Temp.y);
	}
	else
	{
		Temp.z = CentreZ;
	}

	HighlightScriptArea(Temp/*, MAX( (MaxX - Temp.x), (MaxY - Temp.y ) )*/, -1);
}



bool CScriptAreas::IsVecIn2dAngledArea(const Vector3& vecPos, float TargetX1, float TargetY1, float TargetX2, float TargetY2, float DistanceFrom1To4, bool bDrawDebug)
{
	float RadiansBetweenFirstTwoPoints = fwAngle::GetRadianAngleBetweenPoints(TargetX1, TargetY1, TargetX2, TargetY2);
	float RadiansBetweenPoints1and4 = RadiansBetweenFirstTwoPoints + (PI / 2);

	while (RadiansBetweenPoints1and4 < 0.0f)
	{
		RadiansBetweenPoints1and4 += (2 * PI);
	}

	while (RadiansBetweenPoints1and4 > (2 * PI))
	{
		RadiansBetweenPoints1and4 -= (2 * PI);
	}

	float TargetX3 = (DistanceFrom1To4 * rage::Sinf(RadiansBetweenPoints1and4)) + TargetX2;
	float TargetY3 = (DistanceFrom1To4 * -rage::Cosf(RadiansBetweenPoints1and4)) + TargetY2;

	float TargetX4 = (DistanceFrom1To4 * rage::Sinf(RadiansBetweenPoints1and4)) + TargetX1;
	float TargetY4 = (DistanceFrom1To4 * -rage::Cosf(RadiansBetweenPoints1and4)) + TargetY1;

	Vector2 vec1To2 = Vector2((TargetX2 - TargetX1), (TargetY2 - TargetY1));
	Vector2 vec1To4 = Vector2((TargetX4 - TargetX1), (TargetY4 - TargetY1));

	float DistanceFrom1To2 = vec1To2.Mag();
	float DistanceFrom1To4Test = vec1To4.Mag();


	Vector2 vec1ToPos = Vector2((vecPos.x - TargetX1), (vecPos.y - TargetY1));
	vec1To2.Normalize();
	float TestDistance = vec1ToPos.Dot(vec1To2);


	//////////////////////////
	bool IsWithinRange = FALSE;

	//	Check that the position Passed is within the specified area
	if ((TestDistance >= 0.0f) && (TestDistance <= DistanceFrom1To2))
	{
		vec1To4.Normalize();
		TestDistance = vec1ToPos.Dot(vec1To4);

		if ((TestDistance >= 0.0f)
			&& (TestDistance <= DistanceFrom1To4Test))
		{
			IsWithinRange = TRUE;
		}
	}

	if (bDrawDebug)
	{
		CScriptDebug::DrawDebugAngledSquare(TargetX1, TargetY1, TargetX2, TargetY2, TargetX3, TargetY3, TargetX4, TargetY4);
	}

	return IsWithinRange;
}


bool CScriptAreas::IsVecIn3dAngledArea(const Vector3& vecPointToBeChecked, const Vector3& vecFacePoint1, const Vector3& vecFacePoint2, float fDistanceToOppositeFace, bool bDrawDebug)
{
	float TargetZ1 = vecFacePoint1.z;
	float TargetZ2 = vecFacePoint2.z;
	if (TargetZ1 > TargetZ2)
	{
		float temp_float = TargetZ1;
		TargetZ1 = TargetZ2;
		TargetZ2 = temp_float;
	}

	if ((vecPointToBeChecked.z >= TargetZ1) && (vecPointToBeChecked.z <= TargetZ2))
	{
		if (IsVecIn2dAngledArea(vecPointToBeChecked, vecFacePoint1.x, vecFacePoint1.y, vecFacePoint2.x, vecFacePoint2.y, fDistanceToOppositeFace, bDrawDebug))
		{
			return true;
		}
	}

	return false;
}

bool CScriptAreas::IsPointInAngledArea(const Vector3& vPoint, Vector3& vAreaStart, Vector3& vAreaEnd, float AreaWidth, bool HighlightArea, bool Do3dCheck, bool bDrawDebug)
{
	// Tests if the given entity is contained within a non-axis aligned rectangle which is defined by the midpoints of two parallel edges
	// and the length of those sides:
	//
	// <--AreaWidth-->
	//  vAreaStart
	// -------x-------
	// |             |
	// |             |
	// -------x-------
	//  vAreaEnd

	bool bResult = false;

	Vector3 temp_vec;

	float RadiansBetweenFirstTwoPoints;
	float RadiansBetweenPoints1and4;
	float TestDistance;

	Vector2 vec1ToEntity;

	float DistanceFrom1To2, DistanceFrom1To4Test;

	if (Do3dCheck)
	{
		if (vAreaStart.z > vAreaEnd.z)
		{
			temp_vec = vAreaStart;
			vAreaStart = vAreaEnd;
			vAreaEnd = temp_vec;
		}
	}
	else
	{
		vAreaStart.z = INVALID_MINIMUM_WORLD_Z;
		vAreaEnd.z = vAreaStart.z;
	}

	// calculate the angle between vectors 
	RadiansBetweenFirstTwoPoints = fwAngle::GetRadianAngleBetweenPoints(vAreaStart.x, vAreaStart.y, vAreaEnd.x, vAreaEnd.y);

	// get the right angle from these 2 points
	RadiansBetweenPoints1and4 = RadiansBetweenFirstTwoPoints + (PI / 2);

	//work out a right angle from point

	while (RadiansBetweenPoints1and4 < 0.0f)
	{
		RadiansBetweenPoints1and4 += (2 * PI);
	}

	while (RadiansBetweenPoints1and4 > (2 * PI))
	{
		RadiansBetweenPoints1and4 -= (2 * PI);
	}

	//calculate the two points at right angles to the passed in vectors

	const Vector2 vBottomLeft  (((AreaWidth * 0.5f) * rage::Sinf(RadiansBetweenPoints1and4)) + vAreaStart.x, ((AreaWidth * 0.5f) * -rage::Cosf(RadiansBetweenPoints1and4)) + vAreaStart.y) ;
	const Vector2 vBottomRight (((AreaWidth * 0.5f) * rage::Sinf(RadiansBetweenPoints1and4)) + vAreaEnd.x, ((AreaWidth * 0.5f) * -rage::Cosf(RadiansBetweenPoints1and4)) + vAreaEnd.y);

	const Vector2 vTopLeft( vAreaStart.x, vAreaStart.y );
	const Vector2 vTopRight( vAreaEnd.x, vAreaEnd.y );

	//calculate the vectors across the locate and to one corner, used when checking against the players vector
	Vector2 vec1To2 = vTopRight - vTopLeft;
	Vector2 vec1To4 = vTopLeft - vBottomLeft;

	// used for drawing the locate area
	Vector2 vec1To4Copy = vec1To4;

	DistanceFrom1To2 = vec1To2.Mag();
	DistanceFrom1To4Test = vec1To4.Mag();

	vec1ToEntity = Vector2((vPoint.x - vTopLeft.x), (vPoint.y - vTopLeft.y));
	vec1To2.Normalize();

	TestDistance = vec1ToEntity.Dot(vec1To2);

	//	Check that the entity is within the specified area
	if ((TestDistance >= 0.0f)
		&& (TestDistance <= DistanceFrom1To2))
	{
		vec1To4.Normalize();
		TestDistance = vec1ToEntity.Dot(vec1To4);

		if (fabsf(TestDistance) <= DistanceFrom1To4Test)
		{
			if (Do3dCheck)
			{
				if ((vPoint.z >= vAreaStart.z) && (vPoint.z <= vAreaEnd.z))
				{
					bResult = true;
				}
			}
			else
			{
				bResult = true;
			}
		}
	}

	if (HighlightArea)
	{
		if (Do3dCheck)
		{
			CScriptAreas::HighlightImportantAngledArea( (u32)(CTheScripts::GetCurrentGtaScriptThread()->GetIDForThisLineInThisThread()),
				vAreaStart.x+ vec1To4Copy.x, vAreaStart.y+ vec1To4Copy.y,
				vAreaEnd.x+ vec1To4Copy.x, vAreaEnd.y+ vec1To4Copy.y,
				vBottomRight.x, vBottomRight.y,
				vBottomLeft.x, vBottomLeft.y,
				vAreaStart.z);
		}
		else
		{
			CScriptAreas::HighlightImportantAngledArea( (u32)(CTheScripts::GetCurrentGtaScriptThread()->GetIDForThisLineInThisThread()),
				vAreaStart.x+ vec1To4Copy.x, vAreaStart.y+ vec1To4Copy.y,
				vAreaEnd.x+ vec1To4Copy.x, vAreaEnd.y+ vec1To4Copy.y,
				vBottomRight.x, vBottomRight.y,
				vBottomLeft.x, vBottomLeft.y,
				INVALID_MINIMUM_WORLD_Z);
		}
	}

	if (bDrawDebug)
	{
		if (Do3dCheck)
		{
			CScriptDebug::DrawDebugAngledCube(vAreaStart.x + vec1To4Copy.x, vAreaStart.y + vec1To4Copy.y, vAreaStart.z, vAreaEnd.x + vec1To4Copy.x, vAreaEnd.y + vec1To4Copy.y, vAreaEnd.z, vBottomRight.x, vBottomRight.y, 
				vBottomLeft.x, vBottomLeft.y);
		}
		else
		{
			CScriptDebug::DrawDebugAngledSquare(vAreaStart.x+ vec1To4Copy.x, vAreaStart.y+ vec1To4Copy.y, vAreaEnd.x+ vec1To4Copy.x, vAreaEnd.y+ vec1To4Copy.y, vBottomRight.x, vBottomRight.y,
				vBottomLeft.x, vBottomLeft.y);
		}
	}

	return(bResult);
}


bool CScriptAreas::IsAngledAreaInAngledArea(Vector3& vArea1Start, Vector3& vArea1End, float Area1Width, bool Area1Do3dCheck, Vector3& vArea2Start, Vector3& vArea2End, float Area2Width, bool HighlightArea, bool Do3dCheck, bool bDrawDebug)
{
	// Tests if the given area (1) is contained within a non-axis aligned rectangle which is defined by the midpoints of two parallel edges
	// and the length of those sides:
	//
	// <--AreaWidth-->
	//  vAreaStart
	// -------x-------
	// |             |
	// |             |
	// -------x-------
	//  vAreaEnd

	bool bResult = false;

	Vector3 temp_vec;

	float Area1RadiansBetweenFirstTwoPoints;
	float Area1RadiansBetweenPoints1and4;
	float Area2RadiansBetweenFirstTwoPoints;
	float Area2RadiansBetweenPoints1and4;
	float TestDistance;

	Vector2 vec1ToEntity;

	float DistanceFrom1To2, DistanceFrom1To4Test;

	if (Area1Do3dCheck == false && Do3dCheck == true)
	{
		// can't be contained (inner area is a 2d area, outer is 3d)
		return false;
	}

	if (Do3dCheck)
	{
		if (vArea2Start.z > vArea2End.z)
		{
			temp_vec = vArea2Start;
			vArea2Start = vArea2End;
			vArea2End = temp_vec;
		}
	}

	if (Area1Do3dCheck)
	{
		if (vArea1Start.z > vArea1End.z)
		{
			temp_vec = vArea1Start;
			vArea1Start = vArea1End;
			vArea1End = temp_vec;
		}
	}


	// calculate the angle between vectors 
	Area2RadiansBetweenFirstTwoPoints = fwAngle::GetRadianAngleBetweenPoints(vArea2Start.x, vArea2Start.y, vArea2End.x, vArea2End.y);

	// get the right angle from these 2 points
	Area2RadiansBetweenPoints1and4 = Area2RadiansBetweenFirstTwoPoints + (PI / 2.0f);

	//work out a right angle from point

	while (Area2RadiansBetweenPoints1and4 < 0.0f)
	{
		Area2RadiansBetweenPoints1and4 += (2.0f * PI);
	}
	while (Area2RadiansBetweenPoints1and4 > (2.0f * PI))
	{
		Area2RadiansBetweenPoints1and4 -= (2.0f * PI);
	}

	//calculate the two points at right angles to the passed in vectors
	const Vector2 vBottomLeft2  (((Area2Width * 0.5f) * rage::Sinf(Area2RadiansBetweenPoints1and4)) + vArea2Start.x, ((Area2Width * 0.5f) * -rage::Cosf(Area2RadiansBetweenPoints1and4)) + vArea2Start.y) ;
	const Vector2 vBottomRight2 (((Area2Width * 0.5f) * rage::Sinf(Area2RadiansBetweenPoints1and4)) + vArea2End.x, ((Area2Width * 0.5f) * -rage::Cosf(Area2RadiansBetweenPoints1and4)) + vArea2End.y);

	const Vector2 vTopLeft2( vArea2Start.x, vArea2Start.y );
	const Vector2 vTopRight2( vArea2End.x, vArea2End.y );

	//calculate the vectors across the locate and to one corner
	Vector2 vec1To2 = vTopRight2 - vTopLeft2;
	Vector2 vec1To4 = vTopLeft2 - vBottomLeft2;

	// used for drawing the locate area
	Vector2 vec1To4Copy = vec1To4;



	// Only need to do the 3d check if the outer check area is 3d (if the inner is 3d it doesn't matter, would be contained in Z by a 2d check)
	if (Do3dCheck && ((vArea1Start.z < vArea2Start.z) || (vArea1End.z > vArea2End.z)))
	{
		// failed the height test
		bResult = false;
	}
	else
	{
		// calculate the angle between vectors 
		Area1RadiansBetweenFirstTwoPoints = fwAngle::GetRadianAngleBetweenPoints(vArea1Start.x, vArea1Start.y, vArea1End.x, vArea1End.y);

		// get the right angle from these 2 points
		Area1RadiansBetweenPoints1and4 = Area1RadiansBetweenFirstTwoPoints + (PI / 2.0f);

		//work out a right angle from point

		while (Area1RadiansBetweenPoints1and4 < 0.0f)
		{
			Area1RadiansBetweenPoints1and4 += (2.0f * PI);
		}
		while (Area1RadiansBetweenPoints1and4 > (2.0f * PI))
		{
			Area1RadiansBetweenPoints1and4 -= (2.0f * PI);
		}

		//calculate the two points at right angles to the passed in vectors
		Vector2 area1vertices[4];

		area1vertices[0].Set(((Area1Width * 0.5f) * rage::Sinf(Area1RadiansBetweenPoints1and4)) + vArea1Start.x, ((Area1Width * 0.5f) * -rage::Cosf(Area1RadiansBetweenPoints1and4)) + vArea1Start.y) ;	//vBottomLeft1
		area1vertices[1].Set(((Area1Width * 0.5f) * rage::Sinf(Area1RadiansBetweenPoints1and4)) + vArea1End.x, ((Area1Width * 0.5f) * -rage::Cosf(Area1RadiansBetweenPoints1and4)) + vArea1End.y);	//vBottomRight1

		area1vertices[2].Set( vArea1Start.x, vArea1Start.y );	//vTopLeft1
		area1vertices[3].Set( vArea1End.x, vArea1End.y );	// vTopRight1

		DistanceFrom1To2 = vec1To2.Mag();
		DistanceFrom1To4Test = vec1To4.Mag();

		for(int i=0;i<4 && bResult==true;i++)
		{
			vec1ToEntity = Vector2((area1vertices[i].x - vTopLeft2.x), (area1vertices[i].y - vTopLeft2.y));
			vec1To2.Normalize();

			TestDistance = vec1ToEntity.Dot(vec1To2);

			//	Check that the entity is within the specified area
			if ((TestDistance < 0.0f) || (TestDistance > DistanceFrom1To2))
			{
				bResult = false;
				break;
			}

			vec1To4.Normalize();
			TestDistance = vec1ToEntity.Dot(vec1To4);

			if (fabsf(TestDistance) > DistanceFrom1To4Test)
			{
				bResult = false;
				break;
			}
		}
	}

	if (HighlightArea)
	{
		if (Do3dCheck)
		{
			CScriptAreas::HighlightImportantAngledArea( (u32)(CTheScripts::GetCurrentGtaScriptThread()->GetIDForThisLineInThisThread()),
				vArea2Start.x+ vec1To4Copy.x, vArea2Start.y+ vec1To4Copy.y,
				vArea2End.x+ vec1To4Copy.x, vArea2End.y+ vec1To4Copy.y,
				vBottomRight2.x, vBottomRight2.y,
				vBottomLeft2.x, vBottomLeft2.y,
				vArea2Start.z);
		}
		else
		{
			CScriptAreas::HighlightImportantAngledArea( (u32)(CTheScripts::GetCurrentGtaScriptThread()->GetIDForThisLineInThisThread()),
				vArea2Start.x+ vec1To4Copy.x, vArea2Start.y+ vec1To4Copy.y,
				vArea2End.x+ vec1To4Copy.x, vArea2End.y+ vec1To4Copy.y,
				vBottomRight2.x, vBottomRight2.y,
				vBottomLeft2.x, vBottomLeft2.y,
				INVALID_MINIMUM_WORLD_Z);
		}
	}

	if (bDrawDebug)
	{
		if (Do3dCheck)
		{
			CScriptDebug::DrawDebugAngledCube(vArea2Start.x + vec1To4Copy.x, vArea2Start.y + vec1To4Copy.y, vArea2Start.z, vArea2End.x + vec1To4Copy.x, vArea2End.y + vec1To4Copy.y, vArea2End.z, vBottomRight2.x, vBottomRight2.y, 
				vBottomLeft2.x, vBottomLeft2.y);
		}
		else
		{
			CScriptDebug::DrawDebugAngledSquare(vArea2Start.x+ vec1To4Copy.x, vArea2Start.y+ vec1To4Copy.y, vArea2End.x+ vec1To4Copy.x, vArea2End.y+ vec1To4Copy.y, vBottomRight2.x, vBottomRight2.y,
				vBottomLeft2.x, vBottomLeft2.y);
		}
	}

	return(bResult);
}



CScriptAngledArea::CScriptAngledArea() { }

CScriptAngledArea::CScriptAngledArea(const Vector3 & vPt1, const Vector3 & vPt2, const float fWidth, Vector3 * pvOutMinMax)
{
	Set(vPt1, vPt2, fWidth, pvOutMinMax);
}

void CScriptAngledArea::Set(const Vector3 & vPt1, const Vector3 & vPt2, const float fWidth, Vector3 * pvOutMinMax)
{
	s32 e;

	m_vPt1 = vPt1;
	m_vPt2 = vPt2;

	// Ensure that 
	if (m_vPt1.z > m_vPt2.z)
	{
		SwapEm(m_vPt1.z, m_vPt2.z);
	}

	if(m_vPt2.z - m_vPt1.z < 1.0f)
		m_vPt2.z  = m_vPt1.z + 1.0f;
	m_vPt1.z -= 0.5f;

	Vector3 vEdge3 = m_vPt2 - m_vPt1;
	vEdge3.z = 0.0f;
	vEdge3.Normalize();
	Vector3 vNormal3 = CrossProduct(ZAXIS, vEdge3);

	Vector3 vCorners3[4] =
	{
		m_vPt1 - (vNormal3 * fWidth * 0.5f),
		m_vPt2 - (vNormal3 * fWidth * 0.5f),
		m_vPt2 + (vNormal3 * fWidth * 0.5f),
		m_vPt1 + (vNormal3 * fWidth * 0.5f)
	};
	Vector2 vCorners[4];
	for(e=0; e<4; e++)
	{
		vCorners[e] = Vector2(vCorners3[e], Vector2::kXY);
	}

	s32 laste = 3;
	for(e=0; e<4; e++)
	{
		Vector2 vEdge = vCorners[e] - vCorners[laste];
		vEdge.Normalize();
		Vector2 vNormal = Vector2(-vEdge.y, vEdge.x);
		vEdgePlanes[e].x = vNormal.x;
		vEdgePlanes[e].y = vNormal.y;
		vEdgePlanes[e].z = - vNormal.Dot(vCorners[e]);
		laste = e;
	}

	m_fWidth = fWidth;


	if(pvOutMinMax)
	{
		Vector3 vMin(FLT_MAX, FLT_MAX, FLT_MAX);
		Vector3 vMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);

		for(s32 s=0; s<4; s++)
		{
			vMin.x = Min(vMin.x, vCorners[s].x);
			vMin.y = Min(vMin.y, vCorners[s].y);
			vMax.x = Max(vMax.x, vCorners[s].x);
			vMax.y = Max(vMax.y, vCorners[s].y);
		}
		pvOutMinMax[0] = vMin;
		pvOutMinMax[1] = vMax;
	}
}
bool CScriptAngledArea::TestPoint(const Vector3 & vPos) const
{
	if(vPos.z < m_vPt1.z || vPos.z > m_vPt2.z)
		return false;

	for(s32 e=0; e<4; e++)
	{
		// Outside any edge plane?
		if( (vEdgePlanes[e].x * vPos.x + vEdgePlanes[e].y * vPos.y) + vEdgePlanes[e].z < 0.0f )
		{
			return false;
		}
	}
	return true;
}

void CScriptAngledArea::DebugDraw(const Color32 &colour)
{
	Vector3 vAreaStart = m_vPt1;
	Vector3 vAreaEnd = m_vPt2;

	float RadiansBetweenFirstTwoPoints;
	float RadiansBetweenPoints1and4;

	Vector3 temp_vec;

	if (vAreaStart.z > vAreaEnd.z)
	{
		temp_vec = vAreaStart;
		vAreaStart = vAreaEnd;
		vAreaEnd = temp_vec;
	}

	// calculate the angle between vectors 
	RadiansBetweenFirstTwoPoints = fwAngle::GetRadianAngleBetweenPoints(vAreaStart.x, vAreaStart.y, vAreaEnd.x, vAreaEnd.y);

	// get the right angle from these 2 points
	RadiansBetweenPoints1and4 = RadiansBetweenFirstTwoPoints + (PI / 2);

	//work out a right angle from point

	while (RadiansBetweenPoints1and4 < 0.0f)
	{
		RadiansBetweenPoints1and4 += (2 * PI);
	}

	while (RadiansBetweenPoints1and4 > (2 * PI))
	{
		RadiansBetweenPoints1and4 -= (2 * PI);
	}

	const Vector2 vBottomLeft  (((m_fWidth * 0.5f) * rage::Sinf(RadiansBetweenPoints1and4)) + vAreaStart.x, ((m_fWidth * 0.5f) * -rage::Cosf(RadiansBetweenPoints1and4)) + vAreaStart.y) ;
	const Vector2 vBottomRight (((m_fWidth * 0.5f) * rage::Sinf(RadiansBetweenPoints1and4)) + vAreaEnd.x, ((m_fWidth * 0.5f) * -rage::Cosf(RadiansBetweenPoints1and4)) + vAreaEnd.y);

	const Vector2 vTopLeft( vAreaStart.x, vAreaStart.y );
	const Vector2 vTopRight( vAreaEnd.x, vAreaEnd.y );

	//calculate the vectors across the locate and to one corner, used when checking against the players vector
	// Vector2 vec1To2 = vTopRight - vTopLeft;
	Vector2 vec1To4 = vTopLeft - vBottomLeft;

	// used for drawing the locate area
	Vector2 vec1To4Copy = vec1To4;

	CScriptDebug::DrawDebugAngledCube(vAreaStart.x + vec1To4Copy.x, 
		vAreaStart.y + vec1To4Copy.y, 
		vAreaStart.z, 
		vAreaEnd.x + vec1To4Copy.x, 
		vAreaEnd.y + vec1To4Copy.y, 
		vAreaEnd.z, 
		vBottomRight.x, 
		vBottomRight.y, 
		vBottomLeft.x, 
		vBottomLeft.y, colour);

}

