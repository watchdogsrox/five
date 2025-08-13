//
// scene/portals/FrustumDebug.cpp
// 
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved.
//

#if __BANK

// Rage headers
#include "grcore/debugdraw.h"
#include "grcore/viewport.h"
#include "math/channel.h"
#include "system/nelem.h"

// Framework headers
#include "fwmaths/Vector.h"
#include "fwmaths/vectorutil.h"

// Game headers
#include "camera/viewports/ViewportManager.h"
#include "scene/portals/FrustumDebug.h"
#include "shaders/ShaderLib.h"

void CFrustumDebug::Set(const grcViewport& viewport)
{
	const u32 _GrcViewportPlanes[] =
	{
		grcViewport::CLIP_PLANE_LEFT,	// FRUSTUM_LEFT 
		grcViewport::CLIP_PLANE_RIGHT,	// FRUSTUM_RIGHT
		grcViewport::CLIP_PLANE_BOTTOM,	// FRUSTUM_BOTTOM
		grcViewport::CLIP_PLANE_TOP,	// FRUSTUM_TOP
		grcViewport::CLIP_PLANE_NEAR,	// FRUSTUM_NEAR
		grcViewport::CLIP_PLANE_FAR		// FRUSTUM_FAR
	};

	for(u32 i=0; i<MAX_FRUSTUM_PLANES; i++)
		m_Planes[i] = VEC4V_TO_VECTOR4(viewport.GetFrustumClipPlane(_GrcViewportPlanes[i]));
}

void CFrustumDebug::Set(const Matrix34 &worldMatrix, const float nearClip, const float farClip, const float fov, const float screenWidth, const float screenHeight)
{
	const float fovY	=	fov * 0.5f * DtoR;
	const float tanFovY	=	rage::Tanf(fovY);
	const float aspect	=	screenWidth / screenHeight;
	const float fovX	=	rage::Atan2f(tanFovY * aspect, 1.0f);

	const float cosFovX = rage::Cosf(fovX);
	const float sinFovX = rage::Sinf(fovX);
	const float cosFovY = rage::Cosf(fovY);
	const float sinFovY = rage::Sinf(fovY);

	Vector3 localPlanes[4];
	localPlanes[FRUSTUM_LEFT]	= Vector3(-cosFovX, -sinFovX, 0.0f);
	localPlanes[FRUSTUM_RIGHT]	= Vector3(cosFovX, -sinFovX, 0.0f);
	localPlanes[FRUSTUM_BOTTOM]	= Vector3(0.0f, -sinFovY, -cosFovY);
	localPlanes[FRUSTUM_TOP]	= Vector3(0.0f, -sinFovY, cosFovY);

	Matrix34 orientationMatrix = worldMatrix;
	orientationMatrix.d.Zero();
	const Vector3& position = worldMatrix.d;

	//Unfortunately cant transform all these vectors in one fell swoop due to data-structures.
	for(u32 i=0; i<=FRUSTUM_TOP; i++)
	{
		//Calculate normal for world space plane.
		orientationMatrix.Transform(localPlanes[i]);
		m_Planes[i].SetVector3LeaveW(localPlanes[i]);

		//Calculate offset for world space plane.
		m_Planes[i].w = -DotProduct(localPlanes[i], position);
	}

	const Vector3& front = worldMatrix.b;
	const Vector3 nearPlaneNormal = -front;
	m_Planes[FRUSTUM_NEAR].SetVector3LeaveW(nearPlaneNormal);
	const Vector3 pointOnNearPlane = position + (front * nearClip);
	m_Planes[FRUSTUM_NEAR].w = -DotProduct(nearPlaneNormal, pointOnNearPlane);

	const Vector3& farPlaneNormal = front;
	m_Planes[FRUSTUM_FAR].SetVector3LeaveW(farPlaneNormal);
	const Vector3 pointOnFarPlane = position + (front * farClip);
	m_Planes[FRUSTUM_FAR].w = -DotProduct(farPlaneNormal, pointOnFarPlane);
}

void CFrustumDebug::DebugDump(const CFrustumDebug* const frustum)
{
	Vector3 frustPoints[MAX_FRUSTUM_CORNERS];
	frustum->ComputeFrustumCornersDebug(frustPoints);
	{
		mthDisplayf(" FRUST POINTS (%3.0f,%3.0f,%3.0f) (%3.0f,%3.0f,%3.0f) (%3.0f,%3.0f,%3.0f) (%3.0f,%3.0f,%3.0f) (%3.0f,%3.0f,%3.0f) (%3.0f,%3.0f,%3.0f) (%3.0f,%3.0f,%3.0f) (%3.0f,%3.0f,%3.0f))",
			frustPoints[0].x, frustPoints[0].y, frustPoints[0].z,
			frustPoints[1].x, frustPoints[1].y, frustPoints[1].z,
			frustPoints[2].x, frustPoints[2].y, frustPoints[2].z,
			frustPoints[3].x, frustPoints[3].y, frustPoints[3].z,
			frustPoints[4].x, frustPoints[4].y, frustPoints[4].z,
			frustPoints[5].x, frustPoints[5].y, frustPoints[5].z,
			frustPoints[6].x, frustPoints[6].y, frustPoints[6].z,
			frustPoints[7].x, frustPoints[7].y, frustPoints[7].z);

		mthDisplayf(" planes (%0.2f,%0.2f,%0.2f,%3.0f) (%0.2f,%0.2f,%0.2f,%3.0f) (%0.2f,%0.2f,%0.2f,%3.0f) (%0.2f,%0.2f,%0.2f,%3.0f) (%0.2f,%0.2f,%0.2f,%3.0f) (%0.2f,%0.2f,%0.2f,%3.0f)",
			frustum->m_Planes[0].x, frustum->m_Planes[0].y, frustum->m_Planes[0].z, frustum->m_Planes[0].w,
			frustum->m_Planes[1].x, frustum->m_Planes[1].y, frustum->m_Planes[1].z, frustum->m_Planes[1].w,
			frustum->m_Planes[2].x, frustum->m_Planes[2].y, frustum->m_Planes[2].z, frustum->m_Planes[2].w,
			frustum->m_Planes[3].x, frustum->m_Planes[3].y, frustum->m_Planes[3].z, frustum->m_Planes[3].w,
			frustum->m_Planes[4].x, frustum->m_Planes[4].y, frustum->m_Planes[4].z, frustum->m_Planes[4].w,
			frustum->m_Planes[5].x, frustum->m_Planes[5].y, frustum->m_Planes[5].z, frustum->m_Planes[5].w);
	}
}

static bool    g_frustumDebugAlwaysRenderAdvanced   = false;
static bool    g_frustumDebugAlwaysRenderSimple     = false;
static bool    g_frustumDebugNoDepthWrites          = true;
static bool    g_frustumDebugPointsEnabled          = false;
static bool    g_frustumDebugUseOldIntersectCode    = false;
// opacity
static float   g_frustumDebugOpacity                = 1.0f;
static float   g_frustumDebugOpacityScreen          = 0.0f;
static float   g_frustumDebugOpacityLeft            = 1.0f;
static float   g_frustumDebugOpacityTop             = 1.0f;
static float   g_frustumDebugOpacityRight           = 1.0f;
static float   g_frustumDebugOpacityBottom          = 1.0f;
static float   g_frustumDebugOpacityNear            = 1.0f;
static float   g_frustumDebugOpacityFar             = 1.0f;
// front
static bool    g_frustumDebugFrontEnabled           = true;
static float   g_frustumDebugFrontOpacity           = 1.0f;
static Color32 g_frustumDebugFrontFillColour        = Color32(0,255,0,128);
static Color32 g_frustumDebugFrontGridColour        = Color32(0,0,255,64);
static Color32 g_frustumDebugFrontEdgeColour        = Color32(0,0,255,255);
// back
static bool    g_frustumDebugBackEnabled            = true;
static float   g_frustumDebugBackOpacity            = 1.0f;
static Color32 g_frustumDebugBackFillColour         = Color32(0,255,0,0);
static Color32 g_frustumDebugBackGridColour         = Color32(0,0,255,0);
static Color32 g_frustumDebugBackEdgeColour         = Color32(0,0,255,0);
// silhouette
static bool    g_frustumDebugSilhouetteEnabled      = true;
static float   g_frustumDebugSilhouetteOpacity      = 1.0f;
static Color32 g_frustumDebugSilhouetteColour       = Color32(0,255,0,128);//Color32(255,255,255,255);
// fill
static bool    g_frustumDebugFillEnabled            = true;
static float   g_frustumDebugFillOpacity            = 1.0f;
// edge
static bool    g_frustumDebugEdgeEnabled            = true;
static float   g_frustumDebugEdgeOpacity            = 1.0f;
// grid
static bool    g_frustumDebugGridEnabled            = true;
static float   g_frustumDebugGridOpacity            = 1.0f;
static int     g_frustumDebugGridNumDivisions       = 32;
static bool    g_frustumDebugGridPerspectiveEnabled = true;
static float   g_frustumDebugGridPerspectiveAmount  = 1.0f;
static float   g_frustumDebugGridPerspectiveBias    = -1.0f;

void CFrustumDebug::AddWidgets(bkBank& bk)
{
	bk.PushGroup("Frustum Debug", false);
	{
		bk.AddToggle("Always Render Advanced"  , &g_frustumDebugAlwaysRenderAdvanced);
		bk.AddToggle("Always Render Simple"    , &g_frustumDebugAlwaysRenderSimple);
		bk.AddToggle("No Depth Writes"         , &g_frustumDebugNoDepthWrites);
		bk.AddToggle("Points Enabled"          , &g_frustumDebugPointsEnabled);
		bk.AddToggle("Use Old Intersect Code"  , &g_frustumDebugUseOldIntersectCode);
		// opacity
		bk.AddSlider("Opacity"                 , &g_frustumDebugOpacity, 0.0f, 1.0f, 1.0f/32.0f);
		bk.AddSlider("Opacity Screen"          , &g_frustumDebugOpacityScreen, 0.0f, 1.0f, 1.0f/32.0f);
		bk.AddSlider("Opacity Left"            , &g_frustumDebugOpacityLeft, 0.0f, 1.0f, 1.0f/32.0f);
		bk.AddSlider("Opacity Top"             , &g_frustumDebugOpacityTop, 0.0f, 1.0f, 1.0f/32.0f);
		bk.AddSlider("Opacity Right"           , &g_frustumDebugOpacityRight, 0.0f, 1.0f, 1.0f/32.0f);
		bk.AddSlider("Opacity Bottom"          , &g_frustumDebugOpacityBottom, 0.0f, 1.0f, 1.0f/32.0f);
		bk.AddSlider("Opacity Near"            , &g_frustumDebugOpacityNear, 0.0f, 1.0f, 1.0f/32.0f);
		bk.AddSlider("Opacity Far"             , &g_frustumDebugOpacityFar, 0.0f, 1.0f, 1.0f/32.0f);
		// front
		bk.AddToggle("Front Enabled"           , &g_frustumDebugFrontEnabled);
		bk.AddSlider("Front Opacity"           , &g_frustumDebugFrontOpacity, 0.0f, 1.0f, 1.0f/32.0f);
		bk.AddColor ("Front Fill Colour"       , &g_frustumDebugFrontFillColour);
		bk.AddColor ("Front Grid Colour"       , &g_frustumDebugFrontGridColour);
		bk.AddColor ("Front Edge Colour"       , &g_frustumDebugFrontEdgeColour);
		// back
		bk.AddToggle("Back Enabled"            , &g_frustumDebugBackEnabled);
		bk.AddSlider("Back Opacity"            , &g_frustumDebugBackOpacity, 0.0f, 1.0f, 1.0f/32.0f);
		bk.AddColor ("Back Fill Colour"        , &g_frustumDebugBackFillColour);
		bk.AddColor ("Back Grid Colour"        , &g_frustumDebugBackGridColour);
		bk.AddColor ("Back Edge Colour"        , &g_frustumDebugBackEdgeColour);
		// silhouette
		bk.AddToggle("Silhouette Enabled"      , &g_frustumDebugSilhouetteEnabled);
		bk.AddSlider("Silhouette Opacity"      , &g_frustumDebugSilhouetteOpacity, 0.0f, 1.0f, 1.0f/32.0f);
		bk.AddColor ("Silhouette Colour"       , &g_frustumDebugSilhouetteColour);
		// fill
		bk.AddToggle("Fill Enabled"            , &g_frustumDebugFillEnabled);
		bk.AddSlider("Fill Opacity"            , &g_frustumDebugFillOpacity, 0.0f, 1.0f, 1.0f/32.0f);
		// edge
		bk.AddToggle("Edge Enabled"            , &g_frustumDebugEdgeEnabled);
		bk.AddSlider("Edge Opacity"            , &g_frustumDebugEdgeOpacity, 0.0f, 1.0f, 1.0f/32.0f);
		// grid
		bk.AddToggle("Grid Enabled"            , &g_frustumDebugGridEnabled);
		bk.AddSlider("Grid Opacity"            , &g_frustumDebugGridOpacity, 0.0f, 1.0f, 1.0f/32.0f);
		bk.AddSlider("Grid Num Divisions"      , &g_frustumDebugGridNumDivisions, 1, 128, 1);
		bk.AddToggle("Grid Perspective Enabled", &g_frustumDebugGridPerspectiveEnabled);
		bk.AddSlider("Grid Perspective Amount" , &g_frustumDebugGridPerspectiveAmount, 0.0f, 1.0f, 1.0f/32.0f);
		bk.AddSlider("Grid Perspective Bias"   , &g_frustumDebugGridPerspectiveBias, -8.0f, 8.0f, 1.0f/32.0f);
	}
	bk.PopGroup();
}

BEGIN_ALIGNED(16)
class CFrustumDebugNoDepthWrites : public grcDrawProxy
{
public:
	CFrustumDebugNoDepthWrites() {}

	virtual int  Size() const { return sizeof(*this); }
	virtual void Draw() const
	{
		grcStateBlock::SetDepthStencilState(CShaderLib::DSS_TestOnly_LessEqual_Invert);
	}
} ALIGNED(16);

void CFrustumDebug::DebugRender() const
{
	if (g_frustumDebugAlwaysRenderSimple && !g_frustumDebugAlwaysRenderAdvanced)
	{
		DebugRender(this, Color32(0,255,0,64));
		return;
	}

	if (g_frustumDebugNoDepthWrites)
	{
		grcDebugDraw::Proxy(CFrustumDebugNoDepthWrites());
	}

	Vec3V corners[8];
	ComputeFrustumCornersDebug((Vector3*)corners);

	//      [1]
	//     0---1
	//    /|  /|
	//   4---5 |
	//[0]| | | |[2] 
	//   | 3-|-2
	//   |/  |/
	//   7---6
	//    [3]

	if (g_frustumDebugPointsEnabled)
	{
		for (int i = 0; i < NELEM(corners); i++)
		{
			grcDebugDraw::Sphere(corners[i], 0.5f, i < NELEM(corners)/2 ? Color32(255,255,0,255) : Color32(255,0,0,255), false);
		}
	}

	class Quad
	{
	public:
		Quad(int i0, int i1, int i2, int i3)
		{
			m_indices[0] = i0;
			m_indices[1] = i1;
			m_indices[2] = i2;
			m_indices[3] = i3;
			m_neighbours[0] = -1;
			m_neighbours[1] = -1;
			m_neighbours[2] = -1;
			m_neighbours[3] = -1;
		}

		int m_indices[4]; // indices into points
		int m_neighbours[4]; // indices into quads
	};

	const int numQuads = 6;
	Quad quads[numQuads] =
	{
		Quad(7,3,0,4), // left
		Quad(4,0,1,5), // top
		Quad(5,1,2,6), // right
		Quad(6,2,3,7), // bottom
		Quad(4,5,6,7), // near
		Quad(3,2,1,0), // far
	};

	// find neighbour quad indices - we could store these in the table above but i'm lazy
	for (int quadIndex = 0; quadIndex < numQuads; quadIndex++)
	{
		for (int sideIndex = 0; sideIndex < 4; sideIndex++)
		{
			const int index0 = quads[quadIndex].m_indices[(sideIndex + 0)];
			const int index1 = quads[quadIndex].m_indices[(sideIndex + 1)%4];

			for (int quadIndex2 = 0; quadIndex2 < numQuads; quadIndex2++)
			{
				if (quadIndex2 != quadIndex)
				{
					for (int sideIndex2 = 0; sideIndex2 < 4; sideIndex2++)
					{
						if (index0 == quads[quadIndex2].m_indices[(sideIndex2 + 1)%4] &&
							index1 == quads[quadIndex2].m_indices[(sideIndex2 + 0)])
						{
							Assert(quads[quadIndex].m_neighbours[sideIndex] == -1);
							quads[quadIndex].m_neighbours[sideIndex] = quadIndex2;
						}
					}
				}
			}

			Assert(quads[quadIndex].m_neighbours[sideIndex] != -1);
		}
	}

	enum
	{
		DRAW_PASS_BACK = 0,
		DRAW_PASS_FRONT,
		DRAW_PASS_COUNT,

		DRAW_SUBPASS_FILL = 0,
		DRAW_SUBPASS_GRID,
		DRAW_SUBPASS_EDGE,
		DRAW_SUBPASS_SILHOUETTE,
		DRAW_SUBPASS_COUNT
	};

	const Vec3V   camPos   = +gVpMan.GetCurrentGameGrcViewport()->GetCameraMtx().GetCol3();
	const Vec3V   camDir   = -gVpMan.GetCurrentGameGrcViewport()->GetCameraMtx().GetCol2();
	const ScalarV camNearZ = ScalarV(gVpMan.GetCurrentGameGrcViewport()->GetNearClip() + 0.01f);

	bool frontFacing[numQuads];

	for (int quadIndex = 0; quadIndex < numQuads; quadIndex++)
	{
		const Vec3V p0 = corners[quads[quadIndex].m_indices[0]];
		const Vec3V p1 = corners[quads[quadIndex].m_indices[1]];
		const Vec3V p2 = corners[quads[quadIndex].m_indices[2]];

		frontFacing[quadIndex] = Dot(camPos - p0, Cross(p1 - p0, p2 - p1)).Getf() < 0.0f;
	}

	const float frontOpacity = (g_frustumDebugFrontEnabled ? 1.0f : 0.0f)*g_frustumDebugFrontOpacity;
	Color32 frontFillColour = g_frustumDebugFrontFillColour;
	Color32 frontGridColour = g_frustumDebugFrontGridColour;
	Color32 frontEdgeColour = g_frustumDebugFrontEdgeColour;

	const float backOpacity = (g_frustumDebugBackEnabled ? 1.0f : 0.0f)*g_frustumDebugBackOpacity;
	Color32 backFillColour = g_frustumDebugBackFillColour;
	Color32 backGridColour = g_frustumDebugBackGridColour;
	Color32 backEdgeColour = g_frustumDebugBackEdgeColour;

	const float silhouetteOpacity = (g_frustumDebugSilhouetteEnabled ? 1.0f : 0.0f)*g_frustumDebugSilhouetteOpacity;
	Color32 silhouetteColour = g_frustumDebugSilhouetteColour;

	const float fillOpacity = (g_frustumDebugFillEnabled ? 1.0f : 0.0f)*g_frustumDebugFillOpacity;
	const float gridOpacity = (g_frustumDebugGridEnabled ? 1.0f : 0.0f)*g_frustumDebugGridOpacity;
	const float edgeOpacity = (g_frustumDebugEdgeEnabled ? 1.0f : 0.0f)*g_frustumDebugEdgeOpacity;

	frontFillColour = frontFillColour.MultiplyAlpha((u8)(0.5f + 255.0f*g_frustumDebugOpacity*frontOpacity*fillOpacity));
	frontGridColour = frontGridColour.MultiplyAlpha((u8)(0.5f + 255.0f*g_frustumDebugOpacity*frontOpacity*gridOpacity));
	frontEdgeColour = frontEdgeColour.MultiplyAlpha((u8)(0.5f + 255.0f*g_frustumDebugOpacity*frontOpacity*edgeOpacity));

	backFillColour = backFillColour.MultiplyAlpha((u8)(0.5f + 255.0f*g_frustumDebugOpacity*backOpacity*fillOpacity));
	backGridColour = backGridColour.MultiplyAlpha((u8)(0.5f + 255.0f*g_frustumDebugOpacity*backOpacity*gridOpacity));
	backEdgeColour = backEdgeColour.MultiplyAlpha((u8)(0.5f + 255.0f*g_frustumDebugOpacity*backOpacity*edgeOpacity));

	silhouetteColour = silhouetteColour.MultiplyAlpha((u8)(0.5f + 255.0f*g_frustumDebugOpacity*silhouetteOpacity));

	const float quadOpacity[] =
	{
		g_frustumDebugOpacityLeft,
		g_frustumDebugOpacityTop,
		g_frustumDebugOpacityRight,
		g_frustumDebugOpacityBottom,
		g_frustumDebugOpacityNear,
		g_frustumDebugOpacityFar,
	};

	for (int projectToScreen = 0; projectToScreen <= 1; projectToScreen++)
	{
		float opacity;

		if (projectToScreen) { opacity = 0.0f + g_frustumDebugOpacityScreen; }
		else                 { opacity = 1.0f - g_frustumDebugOpacityScreen; }

		if (opacity <= 0.0f)
		{
			continue;
		}

		const ScalarV quadProjection(projectToScreen ? 0.95f : 0.00f);
		const ScalarV edgeProjection(projectToScreen ? 1.00f : 0.05f);
		Vec3V quadPoints[8];
		Vec3V edgePoints[8];

		for (int i = 0; i < NELEM(corners); i++)
		{
			const Vec3V p0 = corners[i];
			const Vec3V p1 = camPos + (p0 - camPos)*(camNearZ/Max(camNearZ, Dot(p0 - camPos, camDir))); // projected to screen

			quadPoints[i] = p0 + (p1 - p0)*quadProjection;
			edgePoints[i] = p0 + (p1 - p0)*edgeProjection;
		}

		for (int pass = 0; pass < DRAW_PASS_COUNT; pass++)
		{
			for (int subpass = 0; subpass < DRAW_SUBPASS_COUNT; subpass++)
			{
				for (int quadIndex = 0; quadIndex < numQuads; quadIndex++)
				{
					if (subpass == DRAW_SUBPASS_FILL)
					{
						const Vec3V p0 = quadPoints[quads[quadIndex].m_indices[0]];
						const Vec3V p1 = quadPoints[quads[quadIndex].m_indices[1]];
						const Vec3V p2 = quadPoints[quads[quadIndex].m_indices[2]];
						const Vec3V p3 = quadPoints[quads[quadIndex].m_indices[3]];

						Color32 colour(0);

						if      (pass == DRAW_PASS_FRONT) { if ( frontFacing[quadIndex]) { colour = frontFillColour; }}
						else if (pass == DRAW_PASS_BACK ) { if (!frontFacing[quadIndex]) { colour = backFillColour; }}

						colour = colour.MultiplyAlpha((u8)(0.5f + 255.0f*opacity*quadOpacity[quadIndex]));

						if (colour.GetAlpha() > 0)
						{
							grcDebugDraw::Quad(p0, p1, p2, p3, colour, true, true);
						}
					}
					else if (subpass == DRAW_SUBPASS_GRID)
					{
						const Vec3V p0 = edgePoints[quads[quadIndex].m_indices[0]];
						const Vec3V p1 = edgePoints[quads[quadIndex].m_indices[1]];
						const Vec3V p2 = edgePoints[quads[quadIndex].m_indices[2]];
						const Vec3V p3 = edgePoints[quads[quadIndex].m_indices[3]];

						Color32 colour(0);

						if      (pass == DRAW_PASS_FRONT) { if ( frontFacing[quadIndex]) { colour = frontGridColour; }}
						else if (pass == DRAW_PASS_BACK ) { if (!frontFacing[quadIndex]) { colour = backGridColour; }}

						colour = colour.MultiplyAlpha((u8)(0.5f + 255.0f*opacity*quadOpacity[quadIndex]));

						if (colour.GetAlpha() > 0)
						{
							const float len01 = powf(Mag(p1 - p0).Getf(), powf(2.0f, g_frustumDebugGridPerspectiveBias))*(g_frustumDebugGridPerspectiveEnabled ? 1.0f : 0.0f);
							const float len12 = powf(Mag(p2 - p1).Getf(), powf(2.0f, g_frustumDebugGridPerspectiveBias))*(g_frustumDebugGridPerspectiveEnabled ? 1.0f : 0.0f);
							const float len23 = powf(Mag(p3 - p2).Getf(), powf(2.0f, g_frustumDebugGridPerspectiveBias))*(g_frustumDebugGridPerspectiveEnabled ? 1.0f : 0.0f);
							const float len30 = powf(Mag(p0 - p3).Getf(), powf(2.0f, g_frustumDebugGridPerspectiveBias))*(g_frustumDebugGridPerspectiveEnabled ? 1.0f : 0.0f);

							for (int i = 1; i < g_frustumDebugGridNumDivisions; i++)
							{
								// 0---1
								// |   |
								// 3---2

								// between edges p0-p1 and p3-p2
								{
									float t = (float)i/(float)g_frustumDebugGridNumDivisions;

									const float a = len01;
									const float b = len23;

									if (a != b)
									{
										t += (a*(powf(b/a, t) - 1.0f)/(b - a) - t)*g_frustumDebugGridPerspectiveAmount;
									}

									const Vec3V ep0 = p0 + (p3 - p0)*ScalarV(t);
									const Vec3V ep1 = p1 + (p2 - p1)*ScalarV(t);

									grcDebugDraw::Line(ep0, ep1, colour);
								}

								// between edges p1-p2 and p0-p3
								{
									float t = (float)i/(float)g_frustumDebugGridNumDivisions;

									const float a = len12;
									const float b = len30;

									if (a != b)
									{
										t += (a*(powf(b/a, t) - 1.0f)/(b - a) - t)*g_frustumDebugGridPerspectiveAmount;
									}

									const Vec3V ep0 = p1 + (p0 - p1)*ScalarV(t);
									const Vec3V ep1 = p2 + (p3 - p2)*ScalarV(t);

									grcDebugDraw::Line(ep0, ep1, colour);
								}
							}
						}
					}
					else if (subpass == DRAW_SUBPASS_EDGE)
					{
						for (int sideIndex = 0; sideIndex < 4; sideIndex++)
						{
							const int quadIndex2 = quads[quadIndex].m_neighbours[sideIndex];

							if (quadIndex < quadIndex2)
							{
								Color32 colour(0);

								if      (pass == DRAW_PASS_FRONT) { if ( frontFacing[quadIndex] &&  frontFacing[quadIndex2]) { colour = frontEdgeColour; }}
								else if (pass == DRAW_PASS_BACK ) { if (!frontFacing[quadIndex] && !frontFacing[quadIndex2]) { colour = backEdgeColour; }}

								colour = colour.MultiplyAlpha((u8)(0.5f + 255.0f*opacity*Max<float>(quadOpacity[quadIndex], quadOpacity[quadIndex2])));

								if (colour.GetAlpha() > 0)
								{
									const Vec3V ep0 = edgePoints[quads[quadIndex].m_indices[(sideIndex + 0)]];
									const Vec3V ep1 = edgePoints[quads[quadIndex].m_indices[(sideIndex + 1)%4]];

									grcDebugDraw::Line(ep0, ep1, colour);
								}
							}
						}
					}
					else if (subpass == DRAW_SUBPASS_SILHOUETTE && pass == DRAW_PASS_FRONT)
					{
						for (int sideIndex = 0; sideIndex < 4; sideIndex++)
						{
							const int quadIndex2 = quads[quadIndex].m_neighbours[sideIndex];

							if (quadIndex < quadIndex2 && frontFacing[quadIndex] != frontFacing[quadIndex2])
							{
								Color32 colour = silhouetteColour;

								colour = colour.MultiplyAlpha((u8)(0.5f + 255.0f*opacity*Max<float>(quadOpacity[quadIndex], quadOpacity[quadIndex2])));

								if (colour.GetAlpha() > 0)
								{
									const Vec3V ep0 = edgePoints[quads[quadIndex].m_indices[(sideIndex + 0)]];
									const Vec3V ep1 = edgePoints[quads[quadIndex].m_indices[(sideIndex + 1)%4]];

									grcDebugDraw::Line(ep0, ep1, colour);
								}
							}
						}
					}
				}
			}
		}
	}
}

void CFrustumDebug::DebugRender(const CFrustumDebug* const frustum, const Color32 colour)
{
	if (g_frustumDebugAlwaysRenderAdvanced && !g_frustumDebugAlwaysRenderSimple)
	{
		frustum->DebugRender();
		return;
	}

	const u32 numPolys = 12;
	const u32 numLines = 12;

	const s32 lines[2 * numLines] =
	{
		0, 1,		// 0
		1, 2,		// 1
		2, 3,		// 2
		3, 0,		// 3
		4, 5,		// 4
		5, 6,		// 5
		6, 7,		// 6
		7, 4,		// 7
		1, 5,		// 8
		2, 6,		// 9
		3, 7,		// 10
		0, 4		// 11
	};

	const s32 vi[3 * numPolys] =
	{
		0, 1, 2,	// 0
		2, 3, 0,	// 1
		3, 2, 6,	// 2
		6, 7, 3,	// 3
		6, 4, 7,	// 4
		6, 5, 4,	// 5
		5, 1, 4,	// 6
		4, 1, 0,	// 7
		0, 3, 4,	// 8
		4, 3, 7,	// 9
		5, 2, 1,	// 10
		5, 6, 2		// 11
	};

	Vector3 frustPoints[MAX_FRUSTUM_CORNERS];
	frustum->ComputeFrustumCornersDebug(frustPoints);
	{
		if (g_frustumDebugPointsEnabled)
		{
			for (int i = 0; i < NELEM(frustPoints); i++)
			{
				grcDebugDraw::Sphere(frustPoints[i], 0.5f, i < NELEM(frustPoints)/2 ? Color32(255,255,0,255) : Color32(255,0,0,255), false);
			}
		}

		DebugDrawArrayOfLines(numLines, frustPoints, (const s32* const)lines, colour, colour); 
		DebugDrawPolys(frustPoints, vi, numPolys, colour);
	}
}

void CFrustumDebug::DebugDrawArrayOfLines(const u32 numLines, const Vector3* const points, const s32* const lines, const Color32 startColour,
									   const Color32 endColour)
{
	for(u32 i=0; i<numLines; i++)
	{
		grcDebugDraw::Line(RCC_VEC3V(points[lines[i*2]]), RCC_VEC3V(points[lines[(i*2)+1]]), startColour, endColour);
	}
}

void CFrustumDebug::DebugDrawPolys(const Vector3* const points, const s32* const idx, const u32 numPolys, const Color32 colour,
								const bool shouldDrawBackFaces)
{
	for(u32 i=0; i<numPolys; i++)
	{
		const Vector3& v1 = points[idx[i * 3]];
		const Vector3& v2 = points[idx[(i * 3) + 1]];
		const Vector3& v3 = points[idx[(i * 3) + 2]];
		grcDebugDraw::Poly(RCC_VEC3V(v1), RCC_VEC3V(v2), RCC_VEC3V(v3), colour, shouldDrawBackFaces);
	}
}

void CFrustumDebug::DebugRenderPlanes(Vector4* planes, const u32 numPlanes, const Color32 colour)
{
	const u32 maxPoints = 20;
	Vector3	points[maxPoints];
	u32 numPointsComputed = 0;

	for(u32 i=0; i<numPlanes; i++)
	{
		for(u32 j=0; j<numPlanes; j++)
		{
			for(u32 k=0; k<numPlanes; k++)
			{
				if(i!=j && j!=k && k!=i)
				{
					//3 different planes.
					if((numPointsComputed < maxPoints) && CFrustumDebug::ComputeIntersectionOf3PlanesDebug(planes[i], planes[j],	planes[k], points[numPointsComputed]))
					{
						numPointsComputed++;
					}
				}
			}

		}
	}

	for(u32 i=0; i<numPointsComputed; i++)
	{
		for(u32 j=0; j<numPointsComputed; j++)
		{
			for(u32 k=0; k<numPointsComputed; k++)
			{
				if(i!=j && j!=k && k!=i)
				{
					//3 different verts.
					grcDebugDraw::Poly(RCC_VEC3V(points[i]), RCC_VEC3V(points[j]), RCC_VEC3V(points[k]), colour, true);

					grcDebugDraw::Line(RCC_VEC3V(points[i]), RCC_VEC3V(points[j]), Color_white);
					grcDebugDraw::Line(RCC_VEC3V(points[j]), RCC_VEC3V(points[k]), Color_white);
					grcDebugDraw::Line(RCC_VEC3V(points[k]), RCC_VEC3V(points[i]), Color_white);
				}
			}
		}
	}
}

void CFrustumDebug::ComputeFrustumCornersDebug(Vector3 corners[MAX_FRUSTUM_CORNERS]) const
{
	u32 planeCodeTable[MAX_FRUSTUM_CORNERS][3] =
	{
		{ FRUSTUM_LEFT,		FRUSTUM_TOP,	FRUSTUM_FAR },	// LEFT  TOP FAR	0
		{ FRUSTUM_RIGHT,	FRUSTUM_TOP,	FRUSTUM_FAR },	// RIGHT TOP FAR	1
		{ FRUSTUM_RIGHT,	FRUSTUM_BOTTOM,	FRUSTUM_FAR },	// RIGHT BOT FAR	2
		{ FRUSTUM_LEFT,		FRUSTUM_BOTTOM,	FRUSTUM_FAR },	// LEFT  BOT FAR	3
		{ FRUSTUM_LEFT,		FRUSTUM_TOP,	FRUSTUM_NEAR },	// LEFT  TOP NEAR	4
		{ FRUSTUM_RIGHT,	FRUSTUM_TOP,	FRUSTUM_NEAR },	// RIGHT TOP NEAR	5
		{ FRUSTUM_RIGHT,	FRUSTUM_BOTTOM,	FRUSTUM_NEAR },	// RIGHT BOT NEAR	6
		{ FRUSTUM_LEFT,		FRUSTUM_BOTTOM,	FRUSTUM_NEAR }	// LEFT  BOT NEAR	7
	};

	//   0---1
	//  /|  /|
	// 4---5 |
	// | | | |
	// | 3-|-2
	// |/  |/
	// 7---6

	for(u32 i=0; i<MAX_FRUSTUM_CORNERS; i++)
	{
		const u32 *planeCodes = planeCodeTable[i];

		if (g_frustumDebugUseOldIntersectCode)
		{
			if (ComputeIntersectionOf3PlanesDebug(
				m_Planes[planeCodes[0]],
				m_Planes[planeCodes[1]],
				m_Planes[planeCodes[2]],
				corners[i]
			))
			{
				// ok
			}
			else
			{
				Assertf(0, "corner %d did not compute properly", i);
				corners[i] = Vector3(0,0,0);
			}
		}
		else
		{
			corners[i] = VEC3V_TO_VECTOR3(ComputeIntersectionOf3Planes(
				RCC_VEC4V(m_Planes[planeCodes[0]]),
				RCC_VEC4V(m_Planes[planeCodes[1]]),
				RCC_VEC4V(m_Planes[planeCodes[2]])
			));
		}
	}
}

//Find the intersection point of three plane equations.
FASTRETURNCHECK(bool) CFrustumDebug::ComputeIntersectionOf3PlanesDebug(const Vector4& plane1, const Vector4& plane2, const Vector4& plane3, Vector3& dest)
{
	const Vector3 normal1(plane1.x, plane1.y, plane1.z);
	const Vector3 normal2(plane2.x, plane2.y, plane2.z);
	const Vector3 normal3(plane3.x, plane3.y, plane3.z);
	const float dist1 = -plane1.w;
	const float dist2 = -plane2.w;
	const float dist3 = -plane3.w;

	/*
	http://astronomy.swin.edu.au/~pbourke/geometry/3planes/  <- DW - algorithm stolen from this, code is mine though.

	The intersection of three planes, if no two of them are parallel, is a point. The three planes can be written as 

	N1 . p = d1
	N2 . p = d2
	N3 . p = d3 

	In the above and what follows, "." signifies the dot product and "*" is the cross product. 
	The intersection point P is given by: 

	d1 ( N2 * N3 ) + d2 ( N3 * N1 ) + d3 ( N1 * N2 )  
	P =  -------------------------------------------------------------------------  
	N1 . ( N2 * N3 )  

	Note that the denominator is 0 if any two of the planes are parallel. 
	If (N2 * N3) is zero it means they are parallel. 
	Otherwise (N2 * N3) gives a vector that is perpendicular to both N2 and N3. 
	The only time the dot product is zero is if the two vectors are perpendicular. 
	So, if N1 dot (N2 * N3) is zero it means that N1 is the same as either N2 or N3 
	... hence the planes are parallel and therefore there is no intersection point. 
	*/

	//Check cross products in the correct order of parameters passed!
	const Vector3 c1 = CrossProduct(normal2, normal3);
	const Vector3 c2 = CrossProduct(normal3, normal1);
	const Vector3 c3 = CrossProduct(normal1, normal2);
	const float	denominator = DotProduct(normal1, c1);
	const Vector3 numerator = (dist1 * c1) + (dist2 * c2) + (dist3 * c3);

	if(denominator == 0.0f)
	{
		return false;
	}

	dest = numerator / denominator;
	return true;
}

#endif // __BANK
