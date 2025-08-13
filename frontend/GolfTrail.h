// ======================
// frontend/GolfTrail.h
// (c) 2011 RockstarNorth
// ======================

#ifndef _FRONTEND_GOLFTRAIL_H_
#define _FRONTEND_GOLFTRAIL_H_

class CGolfTrailInterface
{
public:
	static void Init();
#if __DEV
	static void InitWidgets();
#endif // __DEV
	static void Shutdown();
	static void UpdateAndRender(bool bRenderBeforePostFX);

	static void Script_SetTrailEnabled(bool bEnabled);
	static void Script_SetTrailPath(Vec3V_In positionStart, Vec3V_In velocityStart, float velocityScale, float z1, bool bAscending);
	static void Script_SetTrailRadius(float radiusStart, float radiusMiddle, float radiusEnd);
	static void Script_SetTrailColour(Color32 colourStart, Color32 colourMiddle, Color32 colourEnd);
	static void Script_SetTrailTessellation(int numControlPoints, int tessellation);
	static void Script_SetTrailFixedControlPointsEnabled(bool bEnable);
	static void Script_SetTrailFixedControlPoint(int controlPointIndex, Vec3V_In position, float radius, Color32 colour);
	static void Script_SetTrailShaderParams(float pixelThickness, float pixelExpansion, float fadeOpacity, float fadeExponentBias, float textureFill);
	static void Script_SetTrailFacing(bool bFacing);
	static void Script_ResetTrail();

	static float     Script_GetTrailMaxHeight();
	static Vec3V_Out Script_GetTrailVisualControlPoint(int controlPointIndex);
};

#endif // _FRONTEND_GOLFTRAIL_H_
