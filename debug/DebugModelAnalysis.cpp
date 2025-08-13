// ============================
// debug/DebugModelAnalysis.cpp
// (c) 2012 RockstarNorth
// ============================

#if __BANK

#include "atl/array.h"
#include "grcore/debugdraw.h"
#include "grcore/viewport.h"
#if __PS3
#include <cell/rtc.h>
#include "grcore/edgeExtractgeomspu.h"
#endif // __PS3
#include "grmodel/geometry.h"
#include "vector/color32.h"
#include "vectormath/classes.h"

#include "bank/bkmgr.h"
#include "bank/bank.h"
#include "string/stringutil.h"
#include "system/memory.h"
#include "system/nelem.h"

#include "fwdebug/picker.h"
#include "fwmaths/vectorutil.h"
#include "fwutil/xmacro.h"

#include "camera/viewports/ViewportManager.h"
#include "debug/DebugGeometryUtil.h"
#include "debug/DebugModelAnalysis.h"
#include "debug/DebugVoxelAnalysis.h"
#include "debug/GtaPicker.h"
#include "debug/UiGadget/UiColorScheme.h"
#include "debug/UiGadget/UIGadgetInspector.h"
#include "debug/UiGadget/UiGadgetList.h"
#include "modelinfo/BaseModelInfo.h"
#include "scene/Entity.h"
#include "vector/geometry.h"
#include "Vehicles/vehicle.h"

namespace DMA {

class CDMAHistogramProperties
{
public:
	enum eHistogramMode
	{
		MODE_TRIANGLE_AREA_SQRT,
		MODE_TRIANGLE_AREA,
		MODE_TRIANGLE_EDGE_MAX,
		MODE_TRIANGLE_EDGE_MIN,
		MODE_TRIANGLE_HEIGHT_MAX,
		MODE_TRIANGLE_HEIGHT_MIN,
		MODE_TRIANGLE_ANGLE_MAX,
		MODE_TRIANGLE_ANGLE_MIN,
		MODE_EDGE_LENGTH,
		MODE_COUNT
	};

	enum eHistogramDistScale
	{
		SCALE_NONE,
		SCALE_BY_MODEL_LOD_DISTANCE,
		SCALE_BY_CAMERA_DISTANCE,
		SCALE_COUNT
	};

	CDMAHistogramProperties()
	{
		m_mode          = MODE_TRIANGLE_AREA_SQRT;
		m_distScale     = SCALE_NONE;
		m_bucketCount   = 64;
		m_screenspace   = false;
		m_useLogScale   = true;
		m_rangeLower    = 0.0f;
		m_rangeUpper    = 1.0f;
		m_expBiasX      = 0.0f;
		m_valueRef      = 0.0f;
		m_ratioRef      = 0.0f;
	}

	bool RequiresUpdate(const CDMAHistogramProperties& rhs) const
	{
		if (m_mode        != rhs.m_mode       ) { return true; }
		if (m_distScale   != rhs.m_distScale  ) { return true; }
		if (m_bucketCount != rhs.m_bucketCount) { return true; }
		if (m_screenspace != rhs.m_screenspace) { return true; }
		if (m_useLogScale != rhs.m_useLogScale) { return true; }
		if (m_rangeLower  != rhs.m_rangeLower ) { return true; }
		if (m_rangeUpper  != rhs.m_rangeUpper ) { return true; }
		if (m_expBiasX    != rhs.m_expBiasX   ) { return true; }
		if (m_valueRef    != rhs.m_valueRef   ) { return true; }
		if (m_ratioRef    != rhs.m_ratioRef   ) { return true; }

		return false;
	}

	int   m_mode; // eHistogramMode
	int   m_distScale; // eHistogramDistScale
	int   m_bucketCount;
	bool  m_screenspace;
	bool  m_useLogScale;
	float m_rangeLower; // outliers
	float m_rangeUpper; // outliers
	float m_expBiasX;
	float m_valueRef; // will compute # of samples whose value is <= this reference in m_valueRefCount
	float m_ratioRef; // will compute value of sample @ m_bucketCount times this in m_ratioRefValue
};

class CDMAHistogramWindowSettings
{
public:
	enum eWindowMode
	{
		WINDOW_MODE_LINES,
		WINDOW_MODE_BARS,
		WINDOW_MODE_COUNT
	};

	CDMAHistogramWindowSettings()
	{
		m_windowMode                     = WINDOW_MODE_BARS;
		m_windowX                        = 40;
		m_windowY                        = 100;
		m_windowW                        = 320;
		m_windowH                        = 128;
		m_windowBackgroundColour         = Color32(80,80,80,255);
		m_windowBarColourOutline         = Color32(255,255,255,80);
		m_windowBarColourOutlineSelected = Color32(255,0,0,255);
		m_windowBarColourFill            = Color32(80,80,128,255);
		m_windowBarColourFillSelected    = Color32(255,0,0,255);
		m_windowBarOpacityNotSelected    = 1.0f;
		m_windowExpBiasXScale            = 0.0f;
		m_windowExpBiasY                 = 0.0f;
	}

	int     m_windowMode; // eWindowMode
	int     m_windowX;
	int     m_windowY;
	int     m_windowW;
	int     m_windowH;
	Color32 m_windowBackgroundColour;
	Color32 m_windowBarColourOutline;
	Color32 m_windowBarColourOutlineSelected;
	Color32 m_windowBarColourFill;
	Color32 m_windowBarColourFillSelected;
	float   m_windowBarOpacityNotSelected;
	float   m_windowExpBiasXScale;
	float   m_windowExpBiasY;
};

static bool g_CDMAUpdateValueMinMax = false;
static void CDMAUpdateValueMinMax() { g_CDMAUpdateValueMinMax = true; }

class CDMAHistogramSettings : public CDMAHistogramProperties, public CDMAHistogramWindowSettings
{
public:
	CDMAHistogramSettings()
	{
		m_enabled                  = false;
		m_trackPickerWindow        = true;

		m_valueMinMaxEnabled       = false;
		m_valueMinMaxLocked        = false;
		m_valueMin                 = 0.0f;
		m_valueMax                 = 0.0f;

		m_debugBucketIndex         = -1;

		m_debugDrawMatrix          = Mat34V(V_ZERO);
		m_debugDrawEnabled         = false;
		m_debugDrawColour          = Color32(255,0,0,255);
		m_debugDrawSolid           = false;
		m_debugDrawProjectToScreen = false;
		m_debugDrawShowTriNormals  = false;
		m_debugDrawTriNormalLength = 0.5f;

		m_mouseRayTestEnabled      = false;
		m_mouseRayTestDepth        = 1024.0f;
		m_mouseRayTestOrigin       = Vec3V(V_ZERO);
		m_mouseRayTestEnd          = Vec3V(V_ZERO);
		m_mouseRayTestFraction     = ScalarV(V_ZERO);
		m_mouseRayTestHitPos       = Vec3V(V_ZERO);
		m_mouseRayTestHitNormal    = Vec3V(V_ZERO);
		m_mouseRayTestHitTri[0]    = Vec3V(V_ZERO);
		m_mouseRayTestHitTri[1]    = Vec3V(V_ZERO);
		m_mouseRayTestHitTri[2]    = Vec3V(V_ZERO);
	}

	void AddWidgets(bkBank& bank)
	{
		const char* modeStrings[] =
		{
			"MODE_TRIANGLE_AREA_SQRT",
			"MODE_TRIANGLE_AREA",
			"MODE_TRIANGLE_EDGE_MAX",
			"MODE_TRIANGLE_EDGE_MIN",
			"MODE_TRIANGLE_HEIGHT_MAX",
			"MODE_TRIANGLE_HEIGHT_MIN",
			"MODE_TRIANGLE_ANGLE_MAX",
			"MODE_TRIANGLE_ANGLE_MIN",
			"MODE_EDGE_LENGTH",
		};
		CompileTimeAssert(NELEM(modeStrings) == MODE_COUNT);

		const char* distScaleStrings[] =
		{
			"",
			"SCALE_BY_MODEL_LOD_DISTANCE",
			"SCALE_BY_CAMERA_DISTANCE",
		};
		CompileTimeAssert(NELEM(distScaleStrings) == SCALE_COUNT);

		bank.AddToggle("Enabled"     , &m_enabled);
		bank.AddToggle("Track Picker", &m_trackPickerWindow);

		bank.AddSeparator();

		bank.AddToggle("Value Min/Max Enabled", &m_valueMinMaxEnabled, CDMAUpdateValueMinMax);
		bank.AddToggle("Value Min/Max Locked" , &m_valueMinMaxLocked, CDMAUpdateValueMinMax);
		bank.AddSlider("Value Min"            , &m_valueMin, 0.0f, 1000.0f, 1.0f/256.0f, CDMAUpdateValueMinMax);
		bank.AddSlider("Value Max"            , &m_valueMax, 0.0f, 1000.0f, 1.0f/256.0f, CDMAUpdateValueMinMax);

		bank.AddSeparator();

		bank.AddCombo ("Mode"           , &m_mode, NELEM(modeStrings), modeStrings);
		bank.AddCombo ("Distance Scale" , &m_distScale, NELEM(distScaleStrings), distScaleStrings);
		bank.AddSlider("Bucket Count"   , &m_bucketCount, 8, 512, 1);
		bank.AddToggle("Screenspace"    , &m_screenspace);
		bank.AddToggle("Use Log Scale"  , &m_useLogScale);
		bank.AddSlider("Range Lower"    , &m_rangeLower, 0.0f, 1.0f, 1.0f/128.0f);
		bank.AddSlider("Range Upper"    , &m_rangeUpper, 0.0f, 1.0f, 1.0f/128.0f);
		bank.AddSlider("Exp Bias X"     , &m_expBiasX, -4.0f, 4.0f, 1.0f/128.0f);
		bank.AddSlider("Value Reference", &m_valueRef, 0.0f, 100.0f, 1.0f/512.0f);
		bank.AddSlider("Ratio Reference", &m_ratioRef, 0.0f, 1.0f, 1.0f/512.0f);

		bank.AddSeparator();

		bank.AddSlider("Debug Bucket Index", &m_debugBucketIndex, -1, 512 - 1, 1);

		bank.AddSeparator();

		bank.AddToggle("Debug Draw Enabled"          , &m_debugDrawEnabled);
		bank.AddColor ("Debug Draw Bucket Colour"    , &m_debugDrawColour);
		bank.AddToggle("Debug Draw Solid"            , &m_debugDrawSolid);
		bank.AddToggle("Debug Draw Project to Screen", &m_debugDrawProjectToScreen);
		bank.AddToggle("Debug Draw Show Tri Normals" , &m_debugDrawShowTriNormals);
		bank.AddSlider("Debug Draw Tri Normal Length", &m_debugDrawTriNormalLength, 0.1f, 10.0f, 1.0f/32.0f);

		bank.AddSeparator();

		bank.AddToggle("Mouse Ray Test Enabled (use middle mouse button to track)", &m_mouseRayTestEnabled);
		bank.AddSlider("Mouse Ray Test Depth", &m_mouseRayTestDepth, 1.0f, 1024.0f, 1.0f);

		bank.AddSeparator();

		const char* windowModeStrings[] =
		{
			"Lines",
			"Bars",
		};
		CompileTimeAssert(NELEM(windowModeStrings) == CDMAHistogramWindowSettings::WINDOW_MODE_COUNT);

		bank.AddCombo ("Window Mode"             , &m_windowMode, NELEM(windowModeStrings), windowModeStrings);
		bank.AddSlider("Window X"                , &m_windowX, 0, 1280, 1);
		bank.AddSlider("Window Y"                , &m_windowY, 0, 720, 1);
		bank.AddSlider("Window W"                , &m_windowW, 0, 1280, 1);
		bank.AddSlider("Window H"                , &m_windowH, 0, 720, 1);
		bank.AddColor ("Window Background"       , &m_windowBackgroundColour);
		bank.AddColor ("Outline Colour"          , &m_windowBarColourOutline);
		bank.AddColor ("Outline Selected"        , &m_windowBarColourOutlineSelected);
		bank.AddColor ("Fill Colour"             , &m_windowBarColourFill);
		bank.AddColor ("Fill Selected"           , &m_windowBarColourFillSelected);
		bank.AddSlider("Bar Opacity Not Selected", &m_windowBarOpacityNotSelected, 0.0f, 1.0f, 1.0f/32.0f);
		bank.AddSlider("Exp Bias X Scale"        , &m_windowExpBiasXScale, 0.0f, 1.0f, 1.0f/32.0f);
		bank.AddSlider("Exp Bias Y"              , &m_windowExpBiasY, -4.0f, 4.0f, 1.0f/128.0f);
	}

	bool    m_enabled;
	bool    m_trackPickerWindow;

	bool    m_valueMinMaxEnabled;
	bool    m_valueMinMaxLocked;
	float   m_valueMin;
	float   m_valueMax;

	int     m_debugBucketIndex;

	Mat34V  m_debugDrawMatrix; // for passing entity transform
	bool    m_debugDrawEnabled;
	Color32 m_debugDrawColour;
	bool    m_debugDrawSolid;
	bool    m_debugDrawProjectToScreen;
	bool    m_debugDrawShowTriNormals;
	float   m_debugDrawTriNormalLength;

	bool    m_mouseRayTestEnabled;
	float   m_mouseRayTestDepth;
	Vec3V   m_mouseRayTestOrigin;
	Vec3V   m_mouseRayTestEnd;
	ScalarV m_mouseRayTestFraction;
	Vec3V   m_mouseRayTestHitPos;
	Vec3V   m_mouseRayTestHitNormal;
	Vec3V   m_mouseRayTestHitTri[3];
};

class CDMAHistogram : public CDMAHistogramProperties
{
public:
	CDMAHistogram()
	{
		m_modelName[0]              = '\0';
		m_buckets                   = NULL;
		m_bucketCount               = 0; // base-class properties bucket count will be non-zero
		m_bucketMin                 = 0;
		m_bucketMax                 = 0;
		m_bucketSum                 = 0;
		m_valueMin                  = 0.0f;
		m_valueMax                  = 0.0f;
		m_valueRefCount             = 0;
		m_ratioRefValue             = 0.0f;
		m_numTriangles              = 0;
		m_numTrianglesDegenerateIdx = 0;
		m_numTrianglesDegenerateVtx = 0;
	}

	void  AddTriangle(atArray<float>& values, int pass, float ex, float scale, Vec3V_In v0, Vec3V_In v1, Vec3V_In v2, u16 index0, u16 index1, u16 index2, CDMAHistogramSettings* pDebugDrawSettings = NULL);
	float GetTriangleValue(Vec3V_In v0, Vec3V_In v1, Vec3V_In v2) const;
	int   GetBucketIndex(float value, float ex) const;
	float GetBucketMinValue(int bucketIndex) const;
	float GetBucketMaxValue(int bucketIndex) const;
	float GetBucketMaxValueCoverage(float percent) const;

	float GetValueMin() const { return m_useLogScale ? expf(m_valueMin) : m_valueMin; }
	float GetValueMax() const { return m_useLogScale ? expf(m_valueMax) : m_valueMax; }

	float GetRatioRefValue() const { return m_useLogScale ? expf(m_ratioRefValue) : m_ratioRefValue; }

	void Update(const Drawable* pDrawable, const char* modelName, const CDMAHistogramProperties& properties, CDMAHistogramSettings* pDebugDrawSettings = NULL);
	void UpdateWindow(const CDMAHistogramWindowSettings& settings, int selectedBucketIndex = -1) const;

	char  m_modelName[80];
	int*  m_buckets;
	int   m_bucketMin; // smallest bucket size that's not zero
	int   m_bucketMax; // largest bucket size
	int   m_bucketSum; // sum of all buckets
	float m_valueMin;
	float m_valueMax;
	int   m_valueRefCount; // number of samples <= m_valueRef
	float m_ratioRefValue;
	int   m_numTriangles;
	int   m_numTrianglesDegenerateIdx;
	int   m_numTrianglesDegenerateVtx;
};

static float g_modelLODDistance = 0.0f;
static Vec3V g_modelWorldCentre = Vec3V(V_ZERO);
static CDMAHistogramSettings g_settings; // rag controls
static CDMAHistogram g_histogram;

void CDMAHistogram::AddTriangle(atArray<float>& values, int pass, float ex, float scale, Vec3V_In v0, Vec3V_In v1, Vec3V_In v2, u16 index0, u16 index1, u16 index2, CDMAHistogramSettings* pDebugDrawSettings)
{
	if (index0 == index1 ||
		index1 == index2 ||
		index2 == index0)
	{
		if (pass == 0)
		{
			m_numTrianglesDegenerateIdx++;
		}

		return;
	}

	if (IsEqualAll(v0, v1) |
		IsEqualAll(v1, v2) |
		IsEqualAll(v2, v0))
	{
		if (pass == 0)
		{
			m_numTrianglesDegenerateVtx++;
		}

		return;
	}

	Vec3V v[3]; // for calculating triangle/edge value
	Vec3V p[3]; // for debug draw

	if (pDebugDrawSettings && m_screenspace) // this is a bit expensive ..
	{
		const grcViewport& gameVP = *gVpMan.GetUpdateGameGrcViewport();
		const Vec3V camPos = gameVP.GetCameraMtx().GetCol3();

		v[0] = Transform(pDebugDrawSettings->m_debugDrawMatrix, v0);
		v[1] = Transform(pDebugDrawSettings->m_debugDrawMatrix, v1);
		v[2] = Transform(pDebugDrawSettings->m_debugDrawMatrix, v2);

		if (Dot(Cross(v[1] - v[0], v[2] - v[0]), v[0] - camPos).Getf() > 0.0f)
		{
			return; // backfacing triangle
		}

		v[0] = TransformProjective(gameVP.GetFullCompositeMtx(), v[0]);
		v[1] = TransformProjective(gameVP.GetFullCompositeMtx(), v[1]);
		v[2] = TransformProjective(gameVP.GetFullCompositeMtx(), v[2]);
	}
	else
	{
		v[0] = v0;
		v[1] = v1;
		v[2] = v2;
	}

	if (pass == 0)
	{
		m_numTriangles++;
	}
	else if (pass == 1 && pDebugDrawSettings)
	{
		if (pDebugDrawSettings->m_debugDrawEnabled &&
			pDebugDrawSettings->m_debugDrawColour.GetAlpha() != 0)
		{
			p[0] = Transform(pDebugDrawSettings->m_debugDrawMatrix, v0);
			p[1] = Transform(pDebugDrawSettings->m_debugDrawMatrix, v1);
			p[2] = Transform(pDebugDrawSettings->m_debugDrawMatrix, v2);

			if (pDebugDrawSettings->m_debugDrawProjectToScreen)
			{
				const grcViewport& gameVP = *gVpMan.GetUpdateGameGrcViewport();

				const Vec3V   camPos   = +gameVP.GetCameraMtx().GetCol3();
				const Vec3V   camDir   = -gameVP.GetCameraMtx().GetCol2();
				const ScalarV camNearZ = ScalarV(gameVP.GetNearClip() + 0.01f);

				p[0] = camPos + (p[0] - camPos)*(camNearZ/Dot(p[0] - camPos, camDir));
				p[1] = camPos + (p[1] - camPos)*(camNearZ/Dot(p[1] - camPos, camDir));
				p[2] = camPos + (p[2] - camPos)*(camNearZ/Dot(p[2] - camPos, camDir));
			}
		}

		if (pDebugDrawSettings->m_mouseRayTestEnabled && (ioMouse::GetButtons() & ioMouse::MOUSE_MIDDLE))
		{
			const Vec3V triP0 = Transform(pDebugDrawSettings->m_debugDrawMatrix, v0);
			const Vec3V triP1 = Transform(pDebugDrawSettings->m_debugDrawMatrix, v1);
			const Vec3V triP2 = Transform(pDebugDrawSettings->m_debugDrawMatrix, v2);

			const Vec3V triNorm = NormalizeSafe(Cross(triP1 - triP0, triP2 - triP0), Vec3V(V_ZERO));

			if (!IsZeroAll(triNorm))
			{
				const Vec3V ray = pDebugDrawSettings->m_mouseRayTestEnd - pDebugDrawSettings->m_mouseRayTestOrigin;
				float t = 0.0f;
#if 1
				ScalarV fraction(V_ZERO);
				const bool bHit = geomSegments::SegmentTriangleIntersectDirected(
					pDebugDrawSettings->m_mouseRayTestOrigin,
					ray,
					pDebugDrawSettings->m_mouseRayTestEnd,
					triNorm,
					triP0,
					triP1,
					triP2,
					fraction
				);
				t = fraction.Getf();
#else
				const bool bHit = geomSegments::CollideRayTriangle(
					RCC_VECTOR3(triP0),
					RCC_VECTOR3(triP1),
					RCC_VECTOR3(triP2),
					RCC_VECTOR3(pDebugDrawSettings->m_mouseRayTestOrigin),
					RCC_VECTOR3(ray),
					&t,
					false
				);
#endif
				if (bHit && t >= 0.0f && t <= 1.0f)
				{
					if (pDebugDrawSettings->m_mouseRayTestFraction.Getf() >= t)
					{
						pDebugDrawSettings->m_mouseRayTestFraction  = ScalarV(t);
						pDebugDrawSettings->m_mouseRayTestHitPos    = pDebugDrawSettings->m_mouseRayTestOrigin + ray*fraction;
						pDebugDrawSettings->m_mouseRayTestHitNormal = triNorm;
						pDebugDrawSettings->m_mouseRayTestHitTri[0] = triP0;
						pDebugDrawSettings->m_mouseRayTestHitTri[1] = triP1;
						pDebugDrawSettings->m_mouseRayTestHitTri[2] = triP2;
					}
				}
			}
		}
	}

	if (m_mode == MODE_EDGE_LENGTH)
	{
		for (int j = 0; j < 3; j++)
		{
			float value = scale*Mag(v[j] - v[(j + 1)%3]).Getf();

			if (m_useLogScale)
			{
				if (value <= 0.000001f)
				{
					continue;
				}

				value = logf(value);
			}

			if (pass == 0)
			{
				values.PushAndGrow(value);
			}
			else if (pass == 1)
			{
				if (pDebugDrawSettings->m_debugBucketIndex == -1 ||
					pDebugDrawSettings->m_debugBucketIndex == GetBucketIndex(value, ex))
				{
					grcDebugDraw::Line(p[j], p[(j + 1)%3], pDebugDrawSettings->m_debugDrawColour);
				}
			}
		}
	}
	else
	{
		float value = scale*GetTriangleValue(v[0], v[1], v[2]);

		if (m_useLogScale)
		{
			if (value <= 0.000001f)
			{
				return;
			}

			value = logf(value);
		}

		if (pass == 0)
		{
			values.PushAndGrow(value);
		}
		else if (pass == 1)
		{
			if (pDebugDrawSettings->m_debugBucketIndex == -1 ||
				pDebugDrawSettings->m_debugBucketIndex == GetBucketIndex(value, ex))
			{
				grcDebugDraw::Poly(p[0], p[1], p[2], pDebugDrawSettings->m_debugDrawColour, true, pDebugDrawSettings->m_debugDrawSolid);

				if (pDebugDrawSettings->m_debugDrawShowTriNormals)
				{
					const Vec3V centroid = (p[0] + p[1] + p[2])*ScalarVConstant<FLOAT_TO_INT(1.0f/3.0f)>();
					const Vec3V normal   = Normalize(Cross(p[1] - p[0], p[2] - p[0]));

					grcDebugDraw::Line(centroid, AddScaled(centroid, normal, ScalarV(pDebugDrawSettings->m_debugDrawTriNormalLength)), pDebugDrawSettings->m_debugDrawColour);
				}
			}
		}
	}
}

float CDMAHistogram::GetTriangleValue(Vec3V_In v0, Vec3V_In v1, Vec3V_In v2) const
{
	float value = 0.0f;

	switch (m_mode)
	{
	case MODE_TRIANGLE_AREA_SQRT:
	case MODE_TRIANGLE_AREA:
	{
		const Vec3V ab = v1 - v0;
		const Vec3V ac = v2 - v0;

		const ScalarV temp0 = Dot(ab, ab)*Dot(ac, ac);
		const ScalarV temp1 = Dot(ab, ac);

		value = 0.5f*sqrtf((temp0 - temp1*temp1).Getf());

		if (m_mode == MODE_TRIANGLE_AREA_SQRT)
		{
			value = sqrtf(value);
		}

		break;
	}
	case MODE_TRIANGLE_EDGE_MAX:
	case MODE_TRIANGLE_EDGE_MIN:
	{
		const ScalarV e01 = MagSquared(v0 - v1);
		const ScalarV e12 = MagSquared(v1 - v2);
		const ScalarV e20 = MagSquared(v2 - v0);

		if (m_mode == MODE_TRIANGLE_EDGE_MAX)
		{
			value = sqrtf(Max(e01, e12, e20).Getf());
		}
		else
		{
			value = sqrtf(Min(e01, e12, e20).Getf());
		}

		break;
	}
	case MODE_TRIANGLE_HEIGHT_MAX:
	case MODE_TRIANGLE_HEIGHT_MIN:
	{
		const ScalarV temp01 = Dot(v2 - v0, v1 - v0);
		const ScalarV temp12 = Dot(v0 - v1, v2 - v1);
		const ScalarV temp20 = Dot(v1 - v2, v0 - v2);

		const ScalarV h01 = MagSquared(v2 - v0) - temp01*temp01/MagSquared(v1 - v0);
		const ScalarV h12 = MagSquared(v0 - v1) - temp12*temp12/MagSquared(v2 - v1);
		const ScalarV h20 = MagSquared(v1 - v2) - temp20*temp20/MagSquared(v0 - v2);

		if (m_mode == MODE_TRIANGLE_HEIGHT_MAX)
		{
			value = sqrtf(Max(h01, h12, h20).Getf());
		}
		else
		{
			value = sqrtf(Min(h01, h12, h20).Getf());
		}

		break;
	}
	case MODE_TRIANGLE_ANGLE_MAX:
	case MODE_TRIANGLE_ANGLE_MIN:
	{
		const ScalarV temp0 = Dot(v0 - v1, v1 - v2);
		const ScalarV temp1 = Dot(v1 - v2, v2 - v0);
		const ScalarV temp2 = Dot(v2 - v0, v0 - v1);

		const ScalarV d0 = temp0*temp0/(MagSquared(v0 - v1)*MagSquared(v1 - v2));
		const ScalarV d1 = temp1*temp1/(MagSquared(v1 - v2)*MagSquared(v2 - v0));
		const ScalarV d2 = temp2*temp2/(MagSquared(v2 - v0)*MagSquared(v0 - v1));

		if (m_mode == MODE_TRIANGLE_ANGLE_MAX)
		{
			value = RtoD*acosf(sqrtf(Min(d0, d1, d2).Getf()));
		}
		else
		{
			value = RtoD*acosf(sqrtf(Max(d0, d1, d2).Getf()));
		}

		break;
	}}

	if (value > 0.0f) // make sure we don't return -0.0f
	{
		return value;
	}

	return 0.0f;
}

int CDMAHistogram::GetBucketIndex(float value, float ex) const
{
	Assert(m_valueMin <= m_valueMax);

	if (m_valueMin == m_valueMax)
	{
		return 0;
	}

	const float x = powf(Clamp<float>((value - m_valueMin)/(m_valueMax - m_valueMin), 0.0f, 1.0f), ex);
	return Clamp<int>((int)(x*(float)m_bucketCount), 0, m_bucketCount - 1);
}

float CDMAHistogram::GetBucketMinValue(int bucketIndex) const
{
	const float value = m_valueMin + (m_valueMax - m_valueMin)*(float)(bucketIndex + 0)/(float)m_bucketCount;
	return m_useLogScale ? expf(value) : value;
}

float CDMAHistogram::GetBucketMaxValue(int bucketIndex) const
{
	const float value = m_valueMin + (m_valueMax - m_valueMin)*(float)(bucketIndex + 1)/(float)m_bucketCount;
	return m_useLogScale ? expf(value) : value;
}

float CDMAHistogram::GetBucketMaxValueCoverage(float percent) const
{
	if (percent <= 0.0f)
	{
		return m_valueMin;
	}
	else if (percent < 100.0f)
	{
		int sum = 0;

		for (int i = 0; i < m_bucketCount; i++)
		{
			if (100.0f*(float)sum >= percent*(float)m_bucketSum)
			{
				return GetBucketMaxValue(i - 1); // [0..i-1] covers the percentage of values we need
			}

			sum += m_buckets[i];
		}
	}

	return m_valueMax;
}

static void SortFloats(float* values, int count)
{
	class CompareFunc { public: static s32 func(const float* a, const float* b)
	{
		if      (*a > *b) return +1;
		else if (*a < *b) return -1;
		return 0;
	}};

	qsort(values, count, sizeof(float), (int(*)(const void*, const void*))CompareFunc::func);
}

#if __PS3

static Vec4V* g_extractVertStreams[CExtractGeomParams::obvIdxMax] ;
static Vec4V* g_extractVerts = NULL;
static u16*   g_extractIndices = NULL;

#endif // __PS3

void CDMAHistogram::Update(const Drawable* pDrawable, const char* modelName, const CDMAHistogramProperties& properties, CDMAHistogramSettings* pDebugDrawSettings)
{
	USE_DEBUG_MEMORY();

	bool bRequiresUpdate = (properties.RequiresUpdate(*this) || strcmp(m_modelName, modelName) != 0);

	if (properties.m_screenspace ||
		properties.m_distScale == SCALE_BY_CAMERA_DISTANCE)
	{
		bRequiresUpdate = true; // TODO -- only force update when camera changes
	}

	if (g_CDMAUpdateValueMinMax)
	{
		g_CDMAUpdateValueMinMax = false;
		bRequiresUpdate = true;
	}

	if (bRequiresUpdate)
	{
		strcpy(m_modelName, modelName);

		if (m_bucketCount != properties.m_bucketCount)
		{
			if (m_buckets)
			{
				delete[] m_buckets;
			}

			m_buckets = rage_new int[properties.m_bucketCount];
			m_bucketCount = properties.m_bucketCount;
		}

		sysMemSet(m_buckets, 0, m_bucketCount*sizeof(int));

		m_mode        = properties.m_mode;
		m_distScale   = properties.m_distScale;
		m_screenspace = properties.m_screenspace;
		m_useLogScale = properties.m_useLogScale;
		m_rangeLower  = properties.m_rangeLower;
		m_rangeUpper  = properties.m_rangeUpper;
		m_expBiasX    = properties.m_expBiasX;
		m_valueRef    = properties.m_valueRef;
		m_ratioRef    = properties.m_ratioRef;

		m_bucketMin                 = 0;
		m_bucketMax                 = 0;
		m_bucketSum                 = 0;
		m_valueMin                  = 0;
		m_valueMax                  = 0.0f;
		m_valueRefCount             = 0;
		m_ratioRefValue             = 0.0f;
		m_numTriangles              = 0;
		m_numTrianglesDegenerateIdx = 0;
		m_numTrianglesDegenerateVtx = 0;
	}

	if (pDebugDrawSettings &&
		pDebugDrawSettings->m_debugBucketIndex > m_bucketCount - 1)
	{
		pDebugDrawSettings->m_debugBucketIndex = m_bucketCount - 1; // always clamp this
	}

	if (!bRequiresUpdate)
	{
		if (pDebugDrawSettings && pDebugDrawSettings->m_debugDrawEnabled && pDebugDrawSettings->m_debugDrawColour.GetAlpha() != 0)
		{
			// ok, need to debug draw ..
		}
		else if (pDebugDrawSettings && pDebugDrawSettings->m_mouseRayTestEnabled)
		{
			// ok, need to update mouse ray test ..
		}
		else
		{
			return;
		}
	}

	if (pDebugDrawSettings == NULL)
	{
		m_screenspace = false; // don't apply screenspace if we're not using debug draw, we might be processing drawables without a camera
	}

	if (pDebugDrawSettings && pDebugDrawSettings->m_mouseRayTestEnabled && (ioMouse::GetButtons() & ioMouse::MOUSE_MIDDLE))
	{
		const grcViewport& gameVP = *gVpMan.GetUpdateGameGrcViewport();

		const ScalarV tanHFOV = ScalarV(gameVP.GetTanHFOV());
		const ScalarV tanVFOV = ScalarV(gameVP.GetTanVFOV());
		const ScalarV screenX = ScalarV(-1.0f + 2.0f*(float)ioMouse::GetX()/(float)GRCDEVICE.GetWidth());
		const ScalarV screenY = ScalarV(+1.0f - 2.0f*(float)ioMouse::GetY()/(float)GRCDEVICE.GetHeight()); // flip y
		const ScalarV screenZ = ScalarV(pDebugDrawSettings->m_mouseRayTestDepth);

		pDebugDrawSettings->m_mouseRayTestOrigin    = gameVP.GetCameraMtx().GetCol3();
		pDebugDrawSettings->m_mouseRayTestEnd       = Transform(gameVP.GetCameraMtx(), Vec3V(tanHFOV*screenX, tanVFOV*screenY, ScalarV(V_NEGONE))*screenZ);
		pDebugDrawSettings->m_mouseRayTestFraction  = ScalarV(V_FLT_MAX);
		pDebugDrawSettings->m_mouseRayTestHitPos    = Vec3V(V_ZERO);
		pDebugDrawSettings->m_mouseRayTestHitNormal = Vec3V(V_ZERO);
		pDebugDrawSettings->m_mouseRayTestHitTri[0] = Vec3V(V_ZERO);
		pDebugDrawSettings->m_mouseRayTestHitTri[1] = Vec3V(V_ZERO);
		pDebugDrawSettings->m_mouseRayTestHitTri[2] = Vec3V(V_ZERO);
	}

	const float ex = powf(2.0f, m_expBiasX);
	float scale = 1.0f;

	if (m_distScale == SCALE_BY_MODEL_LOD_DISTANCE)
	{
		if (g_modelLODDistance > 0.0f)
		{
			scale = 1.0f/g_modelLODDistance;
		}
	}
	else if (m_distScale == SCALE_BY_CAMERA_DISTANCE)
	{
		const grcViewport& gameVP = *gVpMan.GetUpdateGameGrcViewport();
		const Vec3V camPos = +gameVP.GetCameraMtx().GetCol3();
		const Vec3V camDir = -gameVP.GetCameraMtx().GetCol2();

		scale = 1.0f/Abs(Dot(g_modelWorldCentre - camPos, camDir)).Getf();
	}

	atArray<float> values;

	for (int pass = 0; pass < 2; pass++)
	{
		if (pass == 0)
		{
			if (!bRequiresUpdate)
			{
				continue;
			}
		}
		else if (pass == 1)
		{
			if (bRequiresUpdate)
			{
				const int count = values.GetCount();

				if (count > 0)
				{
					SortFloats(&values[0], count);

					const int index0 = Clamp<int>((int)(m_rangeLower*(float)count), 0, count - 1);
					const int index1 = Clamp<int>((int)(m_rangeUpper*(float)count), index0 + 1, count);

					// first determine min/max value ..
					if (pDebugDrawSettings &&
						pDebugDrawSettings->m_valueMinMaxEnabled)
					{
						m_valueMin = m_useLogScale ? logf(pDebugDrawSettings->m_valueMin) : pDebugDrawSettings->m_valueMin;
						m_valueMax = m_useLogScale ? logf(pDebugDrawSettings->m_valueMax) : pDebugDrawSettings->m_valueMax;
					}
					else
					{
						m_valueMin = values[index0];
						m_valueMax = values[index0];

						for (int i = index0 + 1; i < index1; i++)
						{
							const float value = values[i];

							m_valueMin = Min<float>(value, m_valueMin);
							m_valueMax = Max<float>(value, m_valueMax);
						}

						if (pDebugDrawSettings &&
							pDebugDrawSettings->m_valueMinMaxLocked == false)
						{
							pDebugDrawSettings->m_valueMin = m_useLogScale ? expf(m_valueMin) : m_valueMin;
							pDebugDrawSettings->m_valueMax = m_useLogScale ? expf(m_valueMax) : m_valueMax;
						}
					}

					// .. and then calculate buckets
					{
						const float valueRef = m_useLogScale ? logf(m_valueRef) : m_valueRef;

						for (int i = index0; i < index1; i++)
						{
							const float value = values[i];

							m_buckets[GetBucketIndex(value, ex)]++;
							m_bucketSum++;

							if (value <= valueRef)
							{
								m_valueRefCount++;
							}
						}

						m_ratioRefValue = values[index0 + Clamp<int>((int)(m_ratioRef*(float)m_bucketSum), 0, m_bucketSum - 1)];
					}

					m_bucketMin = m_buckets[0];
					m_bucketMax = m_buckets[0];

					for (int i = 1; i < m_bucketCount; i++)
					{
						if (m_buckets[i] > 0)
						{
							if (m_bucketMin == 0)
							{
								m_bucketMin = m_buckets[i];
							}
							else
							{
								m_bucketMin = Min<int>(m_buckets[i], m_bucketMin);
							}

							m_bucketMax = Max<int>(m_buckets[i], m_bucketMax);
						}
					}
				}
				else
				{
					m_valueMin = 0.0f;
					m_valueMax = 0.0f;
				}

				if (1) // not sure why we would have degenerate triangles, but let's report if it happens
				{
					Assertf(m_numTrianglesDegenerateIdx == 0, "%s: has %d degenerate triangles (by index)", m_modelName, m_numTrianglesDegenerateIdx);
					Assertf(m_numTrianglesDegenerateVtx == 0, "%s: has %d degenerate triangles (by vertex)", m_modelName, m_numTrianglesDegenerateVtx);
				}

				if (pDebugDrawSettings &&
					pDebugDrawSettings->m_debugDrawEnabled &&
					pDebugDrawSettings->m_debugDrawColour.GetAlpha() != 0)
				{
					// ok, need to debug draw
				}
				else
				{
					break;
				}
			}
		}

		const rmcLodGroup& lodGroup = pDrawable->GetLodGroup();

		if (lodGroup.ContainsLod(LOD_HIGH))
		{
			const rmcLod& lod = lodGroup.GetLod(LOD_HIGH);

			for (int lodModelIndex = 0; lodModelIndex < lod.GetCount(); lodModelIndex++)
			{
				const grmModel* pModel = lod.GetModel(lodModelIndex);

				if (pModel)
				{
					const grmModel& model = *pModel;

					for (int geomIndex = 0; geomIndex < model.GetGeometryCount(); geomIndex++)
					{
						const int shaderId = model.GetShaderIndex(geomIndex);
						const grmShader& shader = pDrawable->GetShaderGroup().GetShader(shaderId);

						if (strcmp(shader.GetName(), "cable") == 0) // don't use cable shaders, since all their triangles are degenerate
						{
							continue;
						}

						grmGeometry& geom = model.GetGeometry(geomIndex);
#if __PS3
						if (geom.GetType() == grmGeometry::GEOMETRYEDGE)
						{
							grmGeometryEdge *geomEdge = static_cast<grmGeometryEdge*>(&geom);

#if HACK_GTA4_MODELINFOIDX_ON_SPU && USE_EDGE
							CGta4DbgSpuInfoStruct gtaSpuInfoStruct;
							gtaSpuInfoStruct.gta4RenderPhaseID = 0x02; // called by Object
							gtaSpuInfoStruct.gta4ModelInfoIdx  = 0;
							gtaSpuInfoStruct.gta4ModelInfoType = 0;
#endif // HACK_GTA4_MODELINFOIDX_ON_SPU && USE_EDGE

							// check up front how many verts are in processed geometry and assert if too many
							int totalI = 0;
							int totalV = 0;

							for (int i = 0; i < geomEdge->GetEdgeGeomPpuConfigInfoCount(); i++)
							{
								totalI += geomEdge->GetEdgeGeomPpuConfigInfos()[i].spuConfigInfo.numIndexes;
								totalV += geomEdge->GetEdgeGeomPpuConfigInfos()[i].spuConfigInfo.numVertexes;
							}

							if (totalI > GeometryUtil::EXTRACT_MAX_INDICES)
							{
								Assertf(0, "%s: index buffer has more indices (%d) than system can handle (%d)", m_modelName, totalI, GeometryUtil::EXTRACT_MAX_INDICES);
								return;
							}

							if (totalV > GeometryUtil::EXTRACT_MAX_VERTICES)
							{
								Assertf(0, "%s: vertex buffer has more verts (%d) than system can handle (%d)", m_modelName, totalV, GeometryUtil::EXTRACT_MAX_VERTICES);
								return;
							}

							GeometryUtil::AllocateExtractData(&g_extractVerts, &g_extractIndices);

							sysMemSet(&g_extractVertStreams[0], 0, sizeof(g_extractVertStreams));

							int numVerts = 0;

							const int numIndices = geomEdge->GetVertexAndIndex(
								(Vector4*)g_extractVerts,
								GeometryUtil::EXTRACT_MAX_VERTICES,
								(Vector4**)g_extractVertStreams,
								g_extractIndices,
								GeometryUtil::EXTRACT_MAX_INDICES,
								NULL,//BoneIndexesAndWeights,
								0,//sizeof(BoneIndexesAndWeights),
								NULL,//&BoneIndexOffset,
								NULL,//&BoneIndexStride,
								NULL,//&BoneOffset1,
								NULL,//&BoneOffset2,
								NULL,//&BoneOffsetPoint,
								(u32*)&numVerts,
#if HACK_GTA4_MODELINFOIDX_ON_SPU
								&gtaSpuInfoStruct,
#endif // HACK_GTA4_MODELINFOIDX_ON_SPU
								NULL,
								CExtractGeomParams::extractPos
							);

							for (int i = 0; i < numIndices; i += 3)
							{
								const u16 index0 = g_extractIndices[i + 0];
								const u16 index1 = g_extractIndices[i + 1];
								const u16 index2 = g_extractIndices[i + 2];

								if (!AssertVerify((int)Max<u16>(index0, index1, index2) < numVerts))
								{
									continue;
								}

								const Vec3V v0 = g_extractVerts[index0].GetXYZ();
								const Vec3V v1 = g_extractVerts[index1].GetXYZ();
								const Vec3V v2 = g_extractVerts[index2].GetXYZ();

								AddTriangle(values, pass, ex, scale, v0, v1, v2, index0, index1, index2, pDebugDrawSettings);
							}
						}
						else
#endif // __PS3
						{
							grcVertexBuffer* vb = geom.GetVertexBuffer(true);
							grcIndexBuffer*  ib = geom.GetIndexBuffer (true);

							Assert(vb && ib);

							PS3_ONLY(++g_AllowVertexBufferVramLocks); // PS3: attempting to edit VBs in VRAM
							grcVertexBufferEditor vertexBufferEditor(vb, true, true); // lock=true, readOnly=true

							Assert(geom.GetPrimitiveType() == drawTris);
							Assert(geom.GetPrimitiveCount()*3 == (u32)ib->GetIndexCount());

							const int  numVerts     = vb->GetVertexCount();
							const int  numTriangles = ib->GetIndexCount()/3;
							const u16* indexData    = ib->LockRO();

							for (int i = 0; i < numTriangles; i++)
							{
								const u16 index0 = indexData[i*3 + 0];
								const u16 index1 = indexData[i*3 + 1];
								const u16 index2 = indexData[i*3 + 2];

								if (!AssertVerify((int)Max<u16>(index0, index1, index2) < numVerts))
								{
									continue;
								}

								const Vec3V v0 = VECTOR3_TO_VEC3V(vertexBufferEditor.GetPosition(index0));
								const Vec3V v1 = VECTOR3_TO_VEC3V(vertexBufferEditor.GetPosition(index1));
								const Vec3V v2 = VECTOR3_TO_VEC3V(vertexBufferEditor.GetPosition(index2));

								AddTriangle(values, pass, ex, scale, v0, v1, v2, index0, index1, index2, pDebugDrawSettings);
							}

							PS3_ONLY(--g_AllowVertexBufferVramLocks); // PS3: finished with editing VBs in VRAM

							ib->UnlockRO();
						}
					}
				}
			}
		}
	}

	if (pDebugDrawSettings && pDebugDrawSettings->m_mouseRayTestEnabled)
	{
		if (pDebugDrawSettings->m_mouseRayTestFraction.Getf() <= 1.0f)
		{
			const grcViewport& gameVP = *gVpMan.GetUpdateGameGrcViewport();

			const Vec3V   camPos   = +gameVP.GetCameraMtx().GetCol3();
			const Vec3V   camDir   = -gameVP.GetCameraMtx().GetCol2();
			const ScalarV camNearZ = ScalarV(gameVP.GetNearClip() + 0.01f);

			const Vec3V   p = pDebugDrawSettings->m_mouseRayTestHitPos;
			const Vec3V   n = pDebugDrawSettings->m_mouseRayTestHitNormal;
			const ScalarV z = Dot(p - camPos, camDir);

			const Vec3V pp[3] =
			{
				camPos + (pDebugDrawSettings->m_mouseRayTestHitTri[0] - camPos)*(camNearZ/Dot(pDebugDrawSettings->m_mouseRayTestHitTri[0] - camPos, camDir)),
				camPos + (pDebugDrawSettings->m_mouseRayTestHitTri[1] - camPos)*(camNearZ/Dot(pDebugDrawSettings->m_mouseRayTestHitTri[1] - camPos, camDir)),
				camPos + (pDebugDrawSettings->m_mouseRayTestHitTri[2] - camPos)*(camNearZ/Dot(pDebugDrawSettings->m_mouseRayTestHitTri[2] - camPos, camDir)),
			};

			grcDebugDraw::Sphere(p, 0.05f, Color32(255,0,0,255), false);
			grcDebugDraw::Line(p, p + n, Color32(0,0,255,255));
			grcDebugDraw::Poly(pp[0], pp[1], pp[2], Color32(255,0,0,76), true, true); // solid (alpha=30%)
			grcDebugDraw::Poly(pp[0], pp[1], pp[2], Color32(255,255,0,255), true, false); // edges

			grcDebugDraw::AddDebugOutput("mouse ray test: pos=%f,%f,%f, z=%f, norm:%f,%f,%f", VEC3V_ARGS(p), z.Getf(), VEC3V_ARGS(n));
		}
		else
		{
			grcDebugDraw::AddDebugOutput("mouse ray test = NO INTERSECTION");
		}
	}
}

void CDMAHistogram::UpdateWindow(const CDMAHistogramWindowSettings& settings, int selectedBucketIndex) const
{
	const int resW = 1280;
	const int resH = 720;

	const float wx = (float)settings.m_windowX/(float)resW;
	const float wy = (float)settings.m_windowY/(float)resH;
	const float ww = (float)settings.m_windowW/(float)resW;
	const float wh = (float)settings.m_windowH/(float)resH;

	if (settings.m_windowBackgroundColour.GetAlpha() != 0)
	{
		grcDebugDraw::Quad(Vec2V(wx, wy), Vec2V(wx + ww, wy), Vec2V(wx + ww, wy + wh), Vec2V(wx, wy + wh), settings.m_windowBackgroundColour, true, true);
	}

	const float ex = powf(2.0f, -settings.m_windowExpBiasXScale*m_expBiasX);
	const float ey = powf(2.0f, -settings.m_windowExpBiasY);

	for (int pass = 0; pass < 2; pass++) // fill, outline
	{
		float x0 = wx;
		float y1_prev = 0.0f;

		if (pass == 0 && settings.m_windowMode == CDMAHistogramWindowSettings::WINDOW_MODE_LINES)
		{
			continue;
		}

		for (int i = 0; i < m_bucketCount; i++)
		{
			const float f1 = (float)(i + 1)/(float)m_bucketCount;
			const float x1 = wx + ww*powf(f1, ex);

			const float y0 = wy + wh;
			const float y1 = y0 - wh*powf((float)m_buckets[i]/(float)m_bucketMax, ey);

			Color32 edge = settings.m_windowBarColourOutline;
			Color32 fill = settings.m_windowBarColourFill;

			if (selectedBucketIndex == i)
			{
				// selected bucket background
				{
					const Color32 bkg = settings.m_windowBarColourFillSelected.MultiplyAlpha(48);
					grcDebugDraw::Quad(Vec2V(x0, wy), Vec2V(x1, wy), Vec2V(x1, wy + wh), Vec2V(x0, wy + wh), bkg, true, true);
				}

				if (settings.m_windowMode == CDMAHistogramWindowSettings::WINDOW_MODE_LINES)
				{
					edge = settings.m_windowBarColourOutlineSelected;
				}
				else if (settings.m_windowMode == CDMAHistogramWindowSettings::WINDOW_MODE_BARS)
				{
					fill = settings.m_windowBarColourFillSelected;
				}
			}
			else if (selectedBucketIndex != -1)
			{
				edge.SetAlpha((u8)(0.5f + 255.0f*settings.m_windowBarOpacityNotSelected*(float)edge.GetAlphaf()));
				fill.SetAlpha((u8)(0.5f + 255.0f*settings.m_windowBarOpacityNotSelected*(float)fill.GetAlphaf()));
			}

			if (settings.m_windowMode == CDMAHistogramWindowSettings::WINDOW_MODE_LINES)
			{
				const float x = (x1 + x0)*0.5f;

				grcDebugDraw::Line(Vec2V(x, y0), Vec2V(x, y1), edge);
			}
			else if (settings.m_windowMode == CDMAHistogramWindowSettings::WINDOW_MODE_BARS)
			{
				if (pass == 0)
				{
					grcDebugDraw::Quad(Vec2V(x0, y0), Vec2V(x1, y0), Vec2V(x1, y1), Vec2V(x0, y1), fill, true, true);
				}
				else if (pass == 1)
				{
					grcDebugDraw::Line(Vec2V(x0, y1), Vec2V(x1, y1), edge); // top

					if (i == 0)
					{
						grcDebugDraw::Line(Vec2V(x0, y0), Vec2V(x0, y1), edge);
					}
					else
					{
						Color32 edge2 = settings.m_windowBarColourOutline;

						if (selectedBucketIndex == i ||
							selectedBucketIndex == i - 1)
						{
							//edge2 = settings.m_windowBarColourOutlineSelected;
						}
						else if (selectedBucketIndex != -1)
						{
							edge2.SetAlpha((u8)(0.5f + 255.0f*settings.m_windowBarOpacityNotSelected*(float)edge2.GetAlphaf()));
						}

						grcDebugDraw::Line(Vec2V(x0, y0), Vec2V(x0, Min<float>(y1, y1_prev)), edge2);
					}

					if (i == m_bucketCount - 1)
					{
						grcDebugDraw::Line(Vec2V(x1, y0), Vec2V(x1, y1), edge);
					}
				}
			}

			x0 = x1;
			y1_prev = y1;
		}
	}

	if (settings.m_windowMode == CDMAHistogramWindowSettings::WINDOW_MODE_BARS)
	{
		grcDebugDraw::Line(Vec2V(wx, wy + wh), Vec2V(wx + ww, wy + wh), settings.m_windowBarColourOutline);
	}
}

} // namespace DMA

__COMMENT(static) void CDebugModelAnalysisInterface::AddWidgets(bkBank& bank)
{
	bank.PushGroup("Model Analysis", false);
	DMA::g_settings.AddWidgets(bank);
	bank.PopGroup();

	bank.PushGroup("Dump selected entities to OBJ", false); // TODO -- move this to DebugGeometryUtil.cpp i think
	{
		static char g_dumpSelectedEntitiesOBJPath[80]                    = "assets:/non_final/dumpgeom.obj";
		static bool g_dumpSelectedEntitiesOBJUseColourPerEntity          = false;
		static u32  g_dumpSelectedEntitiesOBJEntityLODs                  = ~0U;
		static u32  g_dumpSelectedEntitiesOBJDrawableLODs                = ~0U;
		static u32  g_dumpSelectedEntitiesOBJRequiredBuckets             = 0;
		static u32  g_dumpSelectedEntitiesOBJExcludedBuckets             = 0;
		static bool g_dumpSelectedEntitiesOBJVehicleTest                 = false;
		static char g_dumpSelectedEntitiesOBJVehicleShaderNameFilter[80] = "";
		static char g_dumpSelectedEntitiesOBJVehicleBoneName[80]         = "";
		static bool g_dumpSelectedEntitiesOBJFlip                        = false;
		static bool g_dumpSelectedEntitiesOBJVerbose                     = false;

		class DumpSelectedEntitiesToOBJ_button { public: static void func()
		{
			const int numEntities = g_PickerManager.GetNumberOfEntities();

			if (numEntities > 0)
			{
				static CDumpGeometryToOBJ* g_dumpGeometry = NULL;
				static const grmGeometry*  s_geomPtr = NULL;
				static Vec3V               g_entityBoundsMin(V_ZERO);
				static Vec3V               g_entityBoundsMax(V_ZERO);

				class DumpSelectedEntitiesToOBJ_geometry { public: static bool func(const grmModel& model, const grmGeometry&, const grmShader&, const grmShaderGroup& shaderGroup, int lodIndex, void*)
				{
					if ((g_dumpSelectedEntitiesOBJDrawableLODs & BIT(lodIndex)) == 0)
					{
						return false;
					}

					const u32 maskDefault    = CRenderer::GenerateSubBucketMask(CRenderer::RB_MODEL_DEFAULT   );
					const u32 maskShadow     = CRenderer::GenerateSubBucketMask(CRenderer::RB_MODEL_SHADOW    );
					const u32 maskReflection = CRenderer::GenerateSubBucketMask(CRenderer::RB_MODEL_REFLECTION);
					const u32 maskMirror     = CRenderer::GenerateSubBucketMask(CRenderer::RB_MODEL_MIRROR    );
					const u32 maskWater      = CRenderer::GenerateSubBucketMask(CRenderer::RB_MODEL_WATER     );

					u32 maskRequired = 0;
					u32 maskExcluded = 0;

					if (g_dumpSelectedEntitiesOBJRequiredBuckets & BIT(CRenderer::RB_MODEL_DEFAULT   )) { maskRequired |= maskDefault   ; }
					if (g_dumpSelectedEntitiesOBJRequiredBuckets & BIT(CRenderer::RB_MODEL_SHADOW    )) { maskRequired |= maskShadow    ; }
					if (g_dumpSelectedEntitiesOBJRequiredBuckets & BIT(CRenderer::RB_MODEL_REFLECTION)) { maskRequired |= maskReflection; }
					if (g_dumpSelectedEntitiesOBJRequiredBuckets & BIT(CRenderer::RB_MODEL_MIRROR    )) { maskRequired |= maskMirror    ; }
					if (g_dumpSelectedEntitiesOBJRequiredBuckets & BIT(CRenderer::RB_MODEL_WATER     )) { maskRequired |= maskWater     ; }

					if (g_dumpSelectedEntitiesOBJExcludedBuckets & BIT(CRenderer::RB_MODEL_DEFAULT   )) { maskExcluded |= maskDefault   ; }
					if (g_dumpSelectedEntitiesOBJExcludedBuckets & BIT(CRenderer::RB_MODEL_SHADOW    )) { maskExcluded |= maskShadow    ; }
					if (g_dumpSelectedEntitiesOBJExcludedBuckets & BIT(CRenderer::RB_MODEL_REFLECTION)) { maskExcluded |= maskReflection; }
					if (g_dumpSelectedEntitiesOBJExcludedBuckets & BIT(CRenderer::RB_MODEL_MIRROR    )) { maskExcluded |= maskMirror    ; }
					if (g_dumpSelectedEntitiesOBJExcludedBuckets & BIT(CRenderer::RB_MODEL_WATER     )) { maskExcluded |= maskWater     ; }

					const u32 mask = CRenderer::GetSubBucketMask(model.ComputeBucketMask(shaderGroup));

					if ((maskRequired & ~mask) |
						(maskExcluded &  mask))
					{
						return false;
					}

					return true;
				}
				static bool func_vehicletest(const grmModel& model, const grmGeometry& geom, const grmShader& shader, const grmShaderGroup& shaderGroup, int lodIndex, void* userData)
				{
					if (!func(model, geom, shader, shaderGroup, lodIndex, userData))
					{
						return false;
					}

					if (s_geomPtr && s_geomPtr != &geom)
					{
						return false;
					}

					if (g_dumpSelectedEntitiesOBJVehicleShaderNameFilter[0] != '\0' && stristr(shader.GetName(), g_dumpSelectedEntitiesOBJVehicleShaderNameFilter) == NULL)
					{
						return false;
					}

					return true;
				}};

				class DumpSelectedEntitiesToOBJ_triangle { public: static void func(Vec3V_In v0, Vec3V_In v1, Vec3V_In v2, int, int, int, void*)
				{
					if (g_dumpSelectedEntitiesOBJFlip)
					{
						g_dumpGeometry->AddTriangle(v2, v1, v0);
					}
					else
					{
						g_dumpGeometry->AddTriangle(v0, v1, v2);
					}

					g_entityBoundsMin = Min(v0, v1, v2, g_entityBoundsMin);
					g_entityBoundsMax = Max(v0, v1, v2, g_entityBoundsMax);
				}};

				if (g_dumpSelectedEntitiesOBJVehicleTest)
				{
					const CEntity* pEntity = static_cast<const CEntity*>(g_PickerManager.GetEntity(0));

					if (pEntity && pEntity->GetIsTypeVehicle())
					{
						const Drawable* pDrawable = pEntity->GetDrawable();

						if (pDrawable)
						{
							const rmcLodGroup& lodGroup = pDrawable->GetLodGroup();

							if (lodGroup.ContainsLod(LOD_HIGH))
							{
								const rmcLod& lod = lodGroup.GetLod(LOD_HIGH);

								for (int lodModelIndex = 0; lodModelIndex < lod.GetCount(); lodModelIndex++)
								{
									const grmModel* pModel = lod.GetModel(lodModelIndex);

									if (pModel)
									{
										const grmModel& model = *pModel;

										const CVehicle* pVehicle = static_cast<const CVehicle*>(pEntity);
										const fragInst* pFragInst = pVehicle->GetFragInst();

										const phBound* pBound = pFragInst->GetArchetype()->GetBound();
										const int componentCount = (pBound->GetType() == phBound::COMPOSITE) ? static_cast<const phBoundComposite*>(pBound)->GetNumBounds() : 1;

										for (int componentId = 0; componentId < componentCount; componentId++)
										{
											const fragTypeChild* pFragChild = pFragInst->GetTypePhysics()->GetChild(componentId);
											const int boneIndex = pVehicle->GetFragInst()->GetType()->GetBoneIndexFromID(pFragChild->GetBoneID());
											const char* boneName = pVehicle->GetDrawable()->GetSkeletonData()->GetBoneData(boneIndex)->GetName();

											if (stricmp(boneName, g_dumpSelectedEntitiesOBJVehicleBoneName) == 0)
											{
												char dumpPath[RAGE_MAX_PATH] = "";
												strcpy(dumpPath, g_dumpSelectedEntitiesOBJPath);
												char* ext = strrchr(dumpPath, '.');
												if (ext) { strcpy(ext, atVarString("_%s.obj", boneName).c_str()); }
												else     { strcat(ext, atVarString("_%s.obj", boneName).c_str()); }

												CDumpGeometryToOBJ dump(dumpPath, "materials.mtl");
												int geomsAttachedToBoneCount = 0;

												for (int geomIndex = 0; geomIndex < model.GetGeometryCount(); geomIndex++)
												{
													const grmGeometry& geom = model.GetGeometry(geomIndex);
													const grmShader& shader = pDrawable->GetShaderGroup().GetShader(model.GetShaderIndex(geomIndex));

													g_dumpGeometry    = &dump;
													s_geomPtr         = &geom;
													g_entityBoundsMin = Vec3V(V_FLT_MAX);
													g_entityBoundsMax = -g_entityBoundsMin;

													const int numTrianglesAttachedToBone = GeometryUtil::CountTrianglesForDrawable(pDrawable, LOD_HIGH, pEntity->GetModelName(), DumpSelectedEntitiesToOBJ_geometry::func, NULL, boneIndex);

													if (numTrianglesAttachedToBone > 0)
													{
														const char* materialNames[] =
														{
															"white",
															"red",
															"green",
															"blue",
															"cyan",
															"magenta",
															"yellow",
														};

														dump.MaterialBegin(materialNames[geomsAttachedToBoneCount%NELEM(materialNames)]);
														GeometryUtil::AddTrianglesForDrawable(pDrawable, LOD_HIGH, pEntity->GetModelName(), DumpSelectedEntitiesToOBJ_geometry::func_vehicletest, DumpSelectedEntitiesToOBJ_triangle::func, Mat34V(V_IDENTITY), NULL, boneIndex);
														dump.MaterialEnd();

														if (g_dumpSelectedEntitiesOBJVerbose)
														{
															Displayf("%d triangles with shader \"%s\" attached to geometry %d (%s) and bone '%s'", numTrianglesAttachedToBone, g_dumpSelectedEntitiesOBJVehicleShaderNameFilter, geomIndex, shader.GetName(), g_dumpSelectedEntitiesOBJVehicleBoneName);
														}

														geomsAttachedToBoneCount++;
													}
												}

												if (g_dumpSelectedEntitiesOBJVerbose)
												{
													Displayf("%d geometries with shader \"%s\" attached to bone '%s'", geomsAttachedToBoneCount, g_dumpSelectedEntitiesOBJVehicleShaderNameFilter, g_dumpSelectedEntitiesOBJVehicleBoneName);
												}

												dump.Close();
											}
										}
									}
								}
							}
						}
					}
				}
				else
				{
					CDumpGeometryToOBJ dump(g_dumpSelectedEntitiesOBJPath, "materials.mtl");

					for (int i = 0; i < numEntities; i++)
					{
						const CEntity* pEntity = static_cast<const CEntity*>(g_PickerManager.GetEntity(i));

						if (pEntity)
						{
							if ((g_dumpSelectedEntitiesOBJEntityLODs & BIT(pEntity->GetLodData().GetLodType())) == 0)
							{
								continue;
							}

							g_dumpGeometry    = &dump;
							g_entityBoundsMin = Vec3V(V_FLT_MAX);
							g_entityBoundsMax = -g_entityBoundsMin;

							if (g_dumpSelectedEntitiesOBJUseColourPerEntity)
							{
								const char* materialNames[] =
								{
									"white",
									"red",
									"green",
									"blue",
									"cyan",
									"magenta",
									"yellow",
								};

								dump.MaterialBegin(materialNames[i%NELEM(materialNames)]);
							}

							const int numTriangles = GeometryUtil::AddTrianglesForEntity(pEntity, -1, DumpSelectedEntitiesToOBJ_geometry::func, DumpSelectedEntitiesToOBJ_triangle::func);

							if (g_dumpSelectedEntitiesOBJVerbose)
							{
								Displayf("dumped %d triangles for %s, local bounds min = %f,%f,%f, max = %f,%f,%f", numTriangles, pEntity->GetModelName(), VEC3V_ARGS(g_entityBoundsMin), VEC3V_ARGS(g_entityBoundsMax));
								Displayf("  matrix col0 = %f,%f,%f", VEC3V_ARGS(pEntity->GetMatrix().GetCol0()));
								Displayf("  matrix col1 = %f,%f,%f", VEC3V_ARGS(pEntity->GetMatrix().GetCol1()));
								Displayf("  matrix col2 = %f,%f,%f", VEC3V_ARGS(pEntity->GetMatrix().GetCol2()));
								Displayf("  matrix col3 = %f,%f,%f", VEC3V_ARGS(pEntity->GetMatrix().GetCol3()));
							}

							if (g_dumpSelectedEntitiesOBJUseColourPerEntity)
							{
								dump.MaterialEnd();
							}
						}
					}

					dump.Close();
				}
			}
		}};

		bank.AddText  ("Path"                  , &g_dumpSelectedEntitiesOBJPath[0], sizeof(g_dumpSelectedEntitiesOBJPath), false);
		bank.AddToggle("Use colour per entity", &g_dumpSelectedEntitiesOBJUseColourPerEntity);
		bank.AddSeparator();
		bank.AddToggle("Entity LOD - HD      ", &g_dumpSelectedEntitiesOBJEntityLODs, BIT(LODTYPES_DEPTH_HD      ));
		bank.AddToggle("Entity LOD - ORPHANHD", &g_dumpSelectedEntitiesOBJEntityLODs, BIT(LODTYPES_DEPTH_ORPHANHD));
		bank.AddToggle("Entity LOD - LOD     ", &g_dumpSelectedEntitiesOBJEntityLODs, BIT(LODTYPES_DEPTH_LOD     ));
		bank.AddToggle("Entity LOD - SLOD1   ", &g_dumpSelectedEntitiesOBJEntityLODs, BIT(LODTYPES_DEPTH_SLOD1   ));
		bank.AddToggle("Entity LOD - SLOD2   ", &g_dumpSelectedEntitiesOBJEntityLODs, BIT(LODTYPES_DEPTH_SLOD2   ));
		bank.AddToggle("Entity LOD - SLOD3   ", &g_dumpSelectedEntitiesOBJEntityLODs, BIT(LODTYPES_DEPTH_SLOD3   ));
		bank.AddToggle("Entity LOD - SLOD4   ", &g_dumpSelectedEntitiesOBJEntityLODs, BIT(LODTYPES_DEPTH_SLOD4   ));
		bank.AddSeparator();
		bank.AddToggle("Drawable LOD - HIGH", &g_dumpSelectedEntitiesOBJDrawableLODs, BIT(LOD_HIGH));
		bank.AddToggle("Drawable LOD - MED ", &g_dumpSelectedEntitiesOBJDrawableLODs, BIT(LOD_MED ));
		bank.AddToggle("Drawable LOD - LOW ", &g_dumpSelectedEntitiesOBJDrawableLODs, BIT(LOD_LOW ));
		bank.AddToggle("Drawable LOD - VLOW", &g_dumpSelectedEntitiesOBJDrawableLODs, BIT(LOD_VLOW));
		bank.AddSeparator();
		bank.AddToggle("Required Bucket - DEFAULT   ", &g_dumpSelectedEntitiesOBJRequiredBuckets, BIT(CRenderer::RB_MODEL_DEFAULT   ));
		bank.AddToggle("Required Bucket - SHADOW    ", &g_dumpSelectedEntitiesOBJRequiredBuckets, BIT(CRenderer::RB_MODEL_SHADOW    ));
		bank.AddToggle("Required Bucket - REFLECTION", &g_dumpSelectedEntitiesOBJRequiredBuckets, BIT(CRenderer::RB_MODEL_REFLECTION));
		bank.AddToggle("Required Bucket - MIRROR    ", &g_dumpSelectedEntitiesOBJRequiredBuckets, BIT(CRenderer::RB_MODEL_MIRROR    ));
		bank.AddToggle("Required Bucket - WATER     ", &g_dumpSelectedEntitiesOBJRequiredBuckets, BIT(CRenderer::RB_MODEL_WATER     ));
		bank.AddSeparator();
		bank.AddToggle("Excluded Bucket - DEFAULT   ", &g_dumpSelectedEntitiesOBJExcludedBuckets, BIT(CRenderer::RB_MODEL_DEFAULT   ));
		bank.AddToggle("Excluded Bucket - SHADOW    ", &g_dumpSelectedEntitiesOBJExcludedBuckets, BIT(CRenderer::RB_MODEL_SHADOW    ));
		bank.AddToggle("Excluded Bucket - REFLECTION", &g_dumpSelectedEntitiesOBJExcludedBuckets, BIT(CRenderer::RB_MODEL_REFLECTION));
		bank.AddToggle("Excluded Bucket - MIRROR    ", &g_dumpSelectedEntitiesOBJExcludedBuckets, BIT(CRenderer::RB_MODEL_MIRROR    ));
		bank.AddToggle("Excluded Bucket - WATER     ", &g_dumpSelectedEntitiesOBJExcludedBuckets, BIT(CRenderer::RB_MODEL_WATER     ));
		bank.AddSeparator();
		bank.AddToggle("Vehicle Test"              , &g_dumpSelectedEntitiesOBJVehicleTest);
		bank.AddText  ("Vehicle Shader Name Filter", &g_dumpSelectedEntitiesOBJVehicleShaderNameFilter[0], sizeof(g_dumpSelectedEntitiesOBJVehicleShaderNameFilter), false);
		bank.AddText  ("Vehicle Bone Name"         , &g_dumpSelectedEntitiesOBJVehicleBoneName[0], sizeof(g_dumpSelectedEntitiesOBJVehicleBoneName), false);
		bank.AddToggle("Flip"                      , &g_dumpSelectedEntitiesOBJFlip);
		bank.AddToggle("Verbose"                   , &g_dumpSelectedEntitiesOBJVerbose);
		bank.AddButton("Dump to OBJ"               , DumpSelectedEntitiesToOBJ_button::func);
	}
	bank.PopGroup();
}

static CUiGadgetInspector* g_histogramInspectorWindow = NULL;
static bool                g_histogramInspectorWindowAttached = false;

const int NUM_DMA_INSPECTOR_ROWS = 12;

static void CDMA_PopulateInspectorWindowCB(CUiGadgetText* pResult, u32 row, u32 col)
{
	if (pResult && row < NUM_DMA_INSPECTOR_ROWS)
	{
		if (DMA::g_histogram.m_modelName[0] == '\0')
		{
			pResult->SetString("");
			return;
		}

		if (col == 0) { switch (row)
		{
			case 0: pResult->SetString("LOD distance"); return;
			case 1: pResult->SetString("value range"); return;
			case 2: pResult->SetString("bucket range"); return;
			case 3: pResult->SetString("# values"); return;
			case 4: pResult->SetString(atVarString("# values <= %f", DMA::g_histogram.m_valueRef).c_str()); return;
			case 5: pResult->SetString(atVarString("value @ ref %f", DMA::g_histogram.m_ratioRef).c_str()); return;
		}}
		else if (col == 1) { switch (row)
		{
			case 0: pResult->SetString(atVarString("%f", DMA::g_modelLODDistance).c_str()); return;
			case 1: pResult->SetString(atVarString("%f..%f", DMA::g_histogram.GetValueMin(), DMA::g_histogram.GetValueMax()).c_str()); return;
			case 2: pResult->SetString(atVarString("%d..%d", DMA::g_histogram.m_bucketMin, DMA::g_histogram.m_bucketMax).c_str()); return;
			case 3: pResult->SetString(atVarString("%d", DMA::g_histogram.m_bucketSum).c_str()); return;
			case 4: pResult->SetString(atVarString("%d (%.2f%%)", DMA::g_histogram.m_valueRefCount, 100.0f*(float)DMA::g_histogram.m_valueRefCount/(float)DMA::g_histogram.m_bucketSum).c_str()); return;
			case 5: pResult->SetString(atVarString("%f", DMA::g_histogram.GetRatioRefValue()).c_str()); return;
		}}

		if (DMA::g_settings.m_debugBucketIndex >= 0 &&
			DMA::g_settings.m_debugBucketIndex < DMA::g_histogram.m_bucketCount)
		{
			if (col == 0) { switch (row)
			{
				case 6: pResult->SetString("debug bucket idx"); return;
				case 7: pResult->SetString("... value range"); return;
				case 8: pResult->SetString("... # values"); return;
			}}
			else if (col == 1) { switch (row)
			{
				case 6: pResult->SetString(atVarString("%d", DMA::g_settings.m_debugBucketIndex).c_str()); return;
				case 7: pResult->SetString(atVarString("%f..%f", DMA::g_histogram.GetBucketMinValue(DMA::g_settings.m_debugBucketIndex), DMA::g_histogram.GetBucketMaxValue(DMA::g_settings.m_debugBucketIndex)).c_str()); return;
				case 8: pResult->SetString(atVarString("%d (%.2f%%)", DMA::g_histogram.m_buckets[DMA::g_settings.m_debugBucketIndex], 100.0f*(float)DMA::g_histogram.m_buckets[DMA::g_settings.m_debugBucketIndex]/(float)DMA::g_histogram.m_bucketSum).c_str()); return;
			}}
		}
		else
		{
			pResult->SetString("");
		}
	}
}

__COMMENT(static) void CDebugModelAnalysisInterface::Update(const CEntity* pEntity)
{
	CGtaPickerInterface* pPickerInterface = static_cast<CGtaPickerInterface*>(g_PickerManager.GetInterface());

	if (pPickerInterface)
	{
		if (DMA::g_settings.m_enabled)
		{
			if (!g_histogramInspectorWindow)
			{
				CUiColorScheme colorScheme;
				const float afColumnOffsets[] = { 0.0f, 128.0f };

				g_histogramInspectorWindow = rage_new CUiGadgetInspector(0.0f, 0.0f, 256.0f, NUM_DMA_INSPECTOR_ROWS, NELEM(afColumnOffsets), afColumnOffsets, colorScheme);
				g_histogramInspectorWindow->SetUpdateCellCB(CDMA_PopulateInspectorWindowCB);
				g_histogramInspectorWindow->SetNumEntries(NUM_DMA_INSPECTOR_ROWS);
			}

			if (!g_histogramInspectorWindowAttached)
			{
				pPickerInterface->AttachInspectorChild(g_histogramInspectorWindow);
				g_histogramInspectorWindowAttached = true;
			}
		}
		else
		{
			if (g_histogramInspectorWindowAttached)
			{
				pPickerInterface->DetachInspectorChild(g_histogramInspectorWindow);
				g_histogramInspectorWindowAttached = false;
			}
		}
	}

	// ============================

	if (pPickerInterface->GetCurrentInspector() &&
		pPickerInterface->GetCurrentInspector() == g_histogramInspectorWindow)
	{
		if (pEntity == NULL ||
			pEntity->GetDrawable() == NULL ||
			pEntity->GetDrawable()->GetLodGroup().ContainsLod(LOD_HIGH) == false)
		{
			DMA::g_histogram.m_modelName[0] = '\0';
			return;
		}

		if (!pEntity->GetIsTypeBuilding() &&
			!pEntity->GetIsTypeObject() &&
			!pEntity->GetIsTypeDummyObject())
		{
			DMA::g_histogram.m_modelName[0] = '\0';
			return; // don't support stuff which might be skinned, for now ..
		}

		const CBaseModelInfo* pModelInfo = pEntity->GetBaseModelInfo();

		if (pModelInfo)
		{
			DMA::g_modelLODDistance = pModelInfo->GetLodDistanceUnscaled();
			DMA::g_modelWorldCentre = pEntity->GetTransform().GetPosition();
		}
		else
		{
			DMA::g_histogram.m_modelName[0] = '\0';
			DMA::g_modelLODDistance = 0.0f;
		}

		if (DMA::g_settings.m_trackPickerWindow && g_histogramInspectorWindow)
		{
			const CUiGadgetSimpleListAndWindow* pPickerWindow = pPickerInterface->GetMainWindow();

			if (pPickerWindow)
			{
				const fwBox2D inspectorBounds = g_histogramInspectorWindow->GetBounds();
				const fwBox2D pickerWinBounds = pPickerWindow->GetBounds();

				DMA::g_settings.m_windowX = (int)pickerWinBounds.x0;
				DMA::g_settings.m_windowY = (int)pickerWinBounds.y1 + 2;
				DMA::g_settings.m_windowW = (int)inspectorBounds.x1 - (int)pickerWinBounds.x0;
			}
		}

		DMA::g_settings.m_debugDrawMatrix = pEntity->GetMatrix();
		DMA::g_histogram.Update(pEntity->GetDrawable(), pEntity->GetModelName(), DMA::g_settings, &DMA::g_settings);
		DMA::g_histogram.UpdateWindow(DMA::g_settings, DMA::g_settings.m_debugBucketIndex);

		if (DMA::g_settings.m_trackPickerWindow && g_histogramInspectorWindow)
		{
			const CUiGadgetSimpleListAndWindow* pPickerWindow = pPickerInterface->GetMainWindow();

			if (pPickerWindow)
			{
				const fwBox2D inspectorBounds = g_histogramInspectorWindow->GetBounds();
				const Vec2V scale(1.0f/1280.0f, 1.0f/720.0f);
				fwBox2D histogramBounds;
				histogramBounds.x0 = (float)DMA::g_settings.m_windowX;
				histogramBounds.y0 = (float)DMA::g_settings.m_windowY;
				histogramBounds.x1 = (float)DMA::g_settings.m_windowX + (float)DMA::g_settings.m_windowW;
				histogramBounds.y1 = (float)DMA::g_settings.m_windowY + (float)DMA::g_settings.m_windowH;

				// draw outline around inspector
				grcDebugDraw::Quad(
					scale*Vec2V(inspectorBounds.x0 - 1.0f, inspectorBounds.y0 - 1.0f),
					scale*Vec2V(inspectorBounds.x1 + 1.0f, inspectorBounds.y0 - 1.0f),
					scale*Vec2V(inspectorBounds.x1 + 1.0f, inspectorBounds.y1 + 1.0f),
					scale*Vec2V(inspectorBounds.x0 - 1.0f, inspectorBounds.y1 + 1.0f),
					Color32(255,255,255,255),
					false
				);

				// draw outline around histogram
				grcDebugDraw::Quad(
					scale*Vec2V(histogramBounds.x0 - 1.0f, histogramBounds.y0 - 1.0f),
					scale*Vec2V(histogramBounds.x1 + 1.0f, histogramBounds.y0 - 1.0f),
					scale*Vec2V(histogramBounds.x1 + 1.0f, histogramBounds.y1 + 1.0f),
					scale*Vec2V(histogramBounds.x0 - 1.0f, histogramBounds.y1 + 1.0f),
					Color32(255,255,255,255),
					false
				);
			}
		}
	}
}

#endif // __BANK
