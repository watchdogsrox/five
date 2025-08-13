#ifndef DEBUG_WINDOW_H_
#define DEBUG_WINDOW_H_

// =============================================================================================== //
// INCLUDES
// =============================================================================================== //

// rage
#include "vector/vector2.h"
#include "grcore/effect.h"

// game
#include "Debug/Rendering/DebugRendering.h"

#if __BANK

// =============================================================================================== //
// CLASS
// =============================================================================================== //

enum eDebugViewTypes
{
	DEBUGVIEW_LIGHTGRAPH,
	DEBUGVIEW_TONEMAPPING_GRAPH,
	DEBUGVIEW_SCREEN_HISTOGRAM,
	DEBUGVIEW_EXPOSURE_CURVE,
	DEBUGVIEW_NUM
};

struct DebugViewInfo
{
	bool m_show;
	Vector2 m_position;
	Vector2 m_size;
	atString m_name;
	float m_minX;
	float m_maxX;
	float m_minY;
	float m_maxY;

	Vector2 GetScreenPos(float fx, float fy)
	{
		float cx = (m_position.x + (fx * m_size.x)) / (float)GRCDEVICE.GetWidth();
		float cy = (m_position.y + ((1.0f - fy) * m_size.y)) / (float)GRCDEVICE.GetHeight();
		return Vector2(cx, cy);
	}
};

class DebugView : public DebugRendering
{
public:

	// Functions
	static void Init();

	static void Draw();
	static void Update();
	static void AddWidgets(bkBank *bk);

	static void Enable(eDebugViewTypes viewType) { m_views[viewType].m_show = true; }
	static void Toggle(eDebugViewTypes viewType) { m_views[viewType].m_show = !m_views[viewType].m_show; }
	static void Disable(eDebugViewTypes viewType) { m_views[viewType].m_show = false; }

	static bool IsEnabled(eDebugViewTypes viewType) { return m_views[viewType].m_show; }

	static void GenerateHistogram();

	// Variables
	static DebugViewInfo m_views[];

private:
	
	static void DrawLightGraph();
	static void DrawTonemappingCurve();
	static void DrawHistogram();
	static void DrawExposureCurve();

	static Vector3 m_mousePos;
	static u32 m_histogram[3][256];
	static u32 m_maxHistogramValue;
};

#endif

#endif