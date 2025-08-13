// ========================================
// renderer/Shadows/CascadeShadowsDebug.cpp
// (c) 2011 RockstarNorth
// ========================================

#if __BANK

#include "grcore/debugdraw.h"
#include "grcore/device.h"
#include "grcore/im.h"
#include "system/memops.h"

#include "fwutil/xmacro.h"

#include "renderer/Shadows/CascadeShadowsDebug.h"

RENDER_OPTIMISATIONS()

// ================================================================================================

static __forceinline Vec2V_Out Project2D(Vec3V_In p, Mat44V_In proj, Vec4V_In projWindow)
{
	const Vec4V temp = Multiply(proj, Vec4V(p, ScalarV(V_ONE)));
	const Vec2V scrp = (temp.GetXY()*Vec2V(0.5f, -0.5f))/temp.GetW() + Vec2V(V_HALF); // [0..1]

	const Vec2V projOffset = projWindow.GetXY();
	const Vec2V projScale  = projWindow.GetZW();

	return (projOffset + projScale*scrp)*Vec2V(1.0f/(float)GRCDEVICE.GetWidth(), 1.0f/(float)GRCDEVICE.GetHeight()); // [0..1]
}

static __forceinline Vec2V_Out Project2D_grcVertex(Vec3V_In p, Mat44V_In proj, Vec4V_In projWindow)
{
	return Vec2V(-1.0f, 1.0f) + Vec2V(2.0f, -2.0f)*Project2D(p, proj, projWindow); // [-1..1]
}

// ================================================================================================

CCSMDebugLine::CCSMDebugLine(const Color32& colour, float opacity, int flags, Vec3V_In p0, Vec3V_In p1) : CCSMDebugObject(colour, opacity, flags)
{
	m_points[0] = p0;
	m_points[1] = p1;
}

__COMMENT(virtual) void CCSMDebugLine::Draw_grcVertex(Mat44V_In proj, Vec4V_In projWindow, int opacityShift) const
{
	const Color32 colour = GetColour(opacityShift);

	const Vec2V p0 = Project2D_grcVertex(m_points[0], proj, projWindow);
	const Vec2V p1 = Project2D_grcVertex(m_points[1], proj, projWindow);

	grcBegin(drawLines, 2);
	grcColor(colour);

	grcVertex2f(p0);
	grcVertex2f(p1);

	grcEnd();
}

__COMMENT(virtual) void CCSMDebugLine::Draw_grcVertex(int opacityShift) const
{
	const Color32 colour = GetColour(opacityShift);

	grcBegin(drawLines, 2);
	grcColor(colour);

	grcVertex3f(m_points[0]);
	grcVertex3f(m_points[1]);

	grcEnd();
}

__COMMENT(virtual) void CCSMDebugLine::Draw_fwDebugDraw(Mat44V_In proj, Vec4V_In projWindow, int opacityShift) const
{
	const Color32 colour = GetColour(opacityShift);

	const Vec2V p0 = Project2D(m_points[0], proj, projWindow);
	const Vec2V p1 = Project2D(m_points[1], proj, projWindow);

	grcDebugDraw::Line(p0, p1, colour);
}

__COMMENT(virtual) void CCSMDebugLine::Draw_fwDebugDraw(int opacityShift) const
{
	const Color32 colour = GetColour(opacityShift);

	grcDebugDraw::Line(m_points[0], m_points[1], colour);
}

// ================================================================================================

CCSMDebugTri::CCSMDebugTri(const Color32& colour, float opacity, int flags, Vec3V_In p0, Vec3V_In p1, Vec3V_In p2) : CCSMDebugObject(colour, opacity, flags)
{
	m_points[0] = p0;
	m_points[1] = p1;
	m_points[2] = p2;
}

__COMMENT(virtual) void CCSMDebugTri::Draw_grcVertex(Mat44V_In proj, Vec4V_In projWindow, int opacityShift) const
{
	const Color32 colour = GetColour(opacityShift);

	const Vec2V p0 = Project2D_grcVertex(m_points[0], proj, projWindow);
	const Vec2V p1 = Project2D_grcVertex(m_points[1], proj, projWindow);
	const Vec2V p2 = Project2D_grcVertex(m_points[2], proj, projWindow);

	grcBegin(drawTris, 3);
	grcColor(colour);

	grcVertex2f(p0);
	grcVertex2f(p1);
	grcVertex2f(p2);

	grcEnd();
}

__COMMENT(virtual) void CCSMDebugTri::Draw_grcVertex(int opacityShift) const
{
	const Color32 colour = GetColour(opacityShift);

	grcBegin(drawTris, 3);
	grcColor(colour);

	grcVertex3f(m_points[0]);
	grcVertex3f(m_points[1]);
	grcVertex3f(m_points[2]);

	grcEnd();
}

__COMMENT(virtual) void CCSMDebugTri::Draw_fwDebugDraw(Mat44V_In proj, Vec4V_In projWindow, int opacityShift) const
{
	const Color32 colour = GetColour(opacityShift);

	const Vec2V p0 = Project2D(m_points[0], proj, projWindow);
	const Vec2V p1 = Project2D(m_points[1], proj, projWindow);
	const Vec2V p2 = Project2D(m_points[2], proj, projWindow);

	grcDebugDraw::Poly(p0, p1, p2, colour);
}

__COMMENT(virtual) void CCSMDebugTri::Draw_fwDebugDraw(int opacityShift) const
{
	const Color32 colour = GetColour(opacityShift);

	grcDebugDraw::Poly(m_points[0], m_points[1], m_points[2], colour);
}

// ================================================================================================

CCSMDebugBox::CCSMDebugBox(const Color32& colour, float opacity, int flags, Vec3V_In boxMin, Vec3V_In boxMax) : CCSMDebugObject(colour, opacity, flags)
{
	const ScalarV x0 = boxMin.GetX();
	const ScalarV y0 = boxMin.GetY();
	const ScalarV z0 = boxMin.GetZ();
	const ScalarV x1 = boxMax.GetX();
	const ScalarV y1 = boxMax.GetY();
	const ScalarV z1 = boxMax.GetZ();

	m_points[0] = Vec3V(x0, y0, z0);
	m_points[1] = Vec3V(x1, y0, z0);
	m_points[2] = Vec3V(x0, y1, z0);
	m_points[3] = Vec3V(x1, y1, z0);
	m_points[4] = Vec3V(x0, y0, z1);
	m_points[5] = Vec3V(x1, y0, z1);
	m_points[6] = Vec3V(x0, y1, z1);
	m_points[7] = Vec3V(x1, y1, z1);
}

CCSMDebugBox::CCSMDebugBox(const Color32& colour, float opacity, int flags, Mat34V_In viewToWorld, ScalarV_In tanHFOV, ScalarV_In tanVFOV, ScalarV_In z0, ScalarV_In z1) : CCSMDebugObject(colour, opacity, flags)
{
	m_points[0] = Transform(viewToWorld, Vec3V(-tanHFOV, -tanVFOV, ScalarV(V_NEGONE))*z0);
	m_points[1] = Transform(viewToWorld, Vec3V(+tanHFOV, -tanVFOV, ScalarV(V_NEGONE))*z0);
	m_points[2] = Transform(viewToWorld, Vec3V(-tanHFOV, +tanVFOV, ScalarV(V_NEGONE))*z0);
	m_points[3] = Transform(viewToWorld, Vec3V(+tanHFOV, +tanVFOV, ScalarV(V_NEGONE))*z0);
	m_points[4] = Transform(viewToWorld, Vec3V(-tanHFOV, -tanVFOV, ScalarV(V_NEGONE))*z1);
	m_points[5] = Transform(viewToWorld, Vec3V(+tanHFOV, -tanVFOV, ScalarV(V_NEGONE))*z1);
	m_points[6] = Transform(viewToWorld, Vec3V(-tanHFOV, +tanVFOV, ScalarV(V_NEGONE))*z1);
	m_points[7] = Transform(viewToWorld, Vec3V(+tanHFOV, +tanVFOV, ScalarV(V_NEGONE))*z1);
}

__COMMENT(virtual) void CCSMDebugBox::Draw_grcVertex(Mat44V_In proj, Vec4V_In projWindow, int opacityShift) const
{
	const Color32 colour = GetColour(opacityShift);

	const Vec2V p0 = Project2D_grcVertex(m_points[0], proj, projWindow);
	const Vec2V p1 = Project2D_grcVertex(m_points[1], proj, projWindow);
	const Vec2V p2 = Project2D_grcVertex(m_points[2], proj, projWindow);
	const Vec2V p3 = Project2D_grcVertex(m_points[3], proj, projWindow);
	const Vec2V p4 = Project2D_grcVertex(m_points[4], proj, projWindow);
	const Vec2V p5 = Project2D_grcVertex(m_points[5], proj, projWindow);
	const Vec2V p6 = Project2D_grcVertex(m_points[6], proj, projWindow);
	const Vec2V p7 = Project2D_grcVertex(m_points[7], proj, projWindow);

	grcBegin(drawLines, 24);
	grcColor(colour);

	grcVertex2f(p0); grcVertex2f(p2);
	grcVertex2f(p2); grcVertex2f(p3);
	grcVertex2f(p3); grcVertex2f(p1);
	grcVertex2f(p1); grcVertex2f(p0);

	grcVertex2f(p4); grcVertex2f(p6);
	grcVertex2f(p6); grcVertex2f(p7);
	grcVertex2f(p7); grcVertex2f(p5);
	grcVertex2f(p5); grcVertex2f(p4);

	grcVertex2f(p0); grcVertex2f(p4);
	grcVertex2f(p1); grcVertex2f(p5);
	grcVertex2f(p2); grcVertex2f(p6);
	grcVertex2f(p3); grcVertex2f(p7);

	grcEnd();
}

__COMMENT(virtual) void CCSMDebugBox::Draw_grcVertex(int opacityShift) const
{
	const Color32 colour = GetColour(opacityShift);

	grcBegin(drawLines, 24);
	grcColor(colour);

	grcVertex3f(m_points[0]); grcVertex3f(m_points[2]);
	grcVertex3f(m_points[2]); grcVertex3f(m_points[3]);
	grcVertex3f(m_points[3]); grcVertex3f(m_points[1]);
	grcVertex3f(m_points[1]); grcVertex3f(m_points[0]);

	grcVertex3f(m_points[4]); grcVertex3f(m_points[6]);
	grcVertex3f(m_points[6]); grcVertex3f(m_points[7]);
	grcVertex3f(m_points[7]); grcVertex3f(m_points[5]);
	grcVertex3f(m_points[5]); grcVertex3f(m_points[4]);

	grcVertex3f(m_points[0]); grcVertex3f(m_points[4]);
	grcVertex3f(m_points[1]); grcVertex3f(m_points[5]);
	grcVertex3f(m_points[2]); grcVertex3f(m_points[6]);
	grcVertex3f(m_points[3]); grcVertex3f(m_points[7]);

	grcEnd();
}

__COMMENT(virtual) void CCSMDebugBox::Draw_fwDebugDraw(Mat44V_In proj, Vec4V_In projWindow, int opacityShift) const
{
	const Color32 colour = GetColour(opacityShift);

	const Vec2V p0 = Project2D(m_points[0], proj, projWindow);
	const Vec2V p1 = Project2D(m_points[1], proj, projWindow);
	const Vec2V p2 = Project2D(m_points[2], proj, projWindow);
	const Vec2V p3 = Project2D(m_points[3], proj, projWindow);
	const Vec2V p4 = Project2D(m_points[4], proj, projWindow);
	const Vec2V p5 = Project2D(m_points[5], proj, projWindow);
	const Vec2V p6 = Project2D(m_points[6], proj, projWindow);
	const Vec2V p7 = Project2D(m_points[7], proj, projWindow);

	grcDebugDraw::Line(p0, p2, colour);
	grcDebugDraw::Line(p2, p3, colour);
	grcDebugDraw::Line(p3, p1, colour);
	grcDebugDraw::Line(p1, p0, colour);

	grcDebugDraw::Line(p4, p6, colour);
	grcDebugDraw::Line(p6, p7, colour);
	grcDebugDraw::Line(p7, p5, colour);
	grcDebugDraw::Line(p5, p4, colour);

	grcDebugDraw::Line(p0, p4, colour);
	grcDebugDraw::Line(p1, p5, colour);
	grcDebugDraw::Line(p2, p6, colour);
	grcDebugDraw::Line(p3, p7, colour);
}

__COMMENT(virtual) void CCSMDebugBox::Draw_fwDebugDraw(int opacityShift) const
{
	const Color32 colour = GetColour(opacityShift);

	grcDebugDraw::Line(m_points[0], m_points[2], colour);
	grcDebugDraw::Line(m_points[2], m_points[3], colour);
	grcDebugDraw::Line(m_points[3], m_points[1], colour);
	grcDebugDraw::Line(m_points[1], m_points[0], colour);

	grcDebugDraw::Line(m_points[4], m_points[6], colour);
	grcDebugDraw::Line(m_points[6], m_points[7], colour);
	grcDebugDraw::Line(m_points[7], m_points[5], colour);
	grcDebugDraw::Line(m_points[5], m_points[4], colour);

	grcDebugDraw::Line(m_points[0], m_points[4], colour);
	grcDebugDraw::Line(m_points[1], m_points[5], colour);
	grcDebugDraw::Line(m_points[2], m_points[6], colour);
	grcDebugDraw::Line(m_points[3], m_points[7], colour);
}

// ================================================================================================

CCSMDebugSphere::CCSMDebugSphere(const Color32& colour, float opacity, int flags, Vec4V_In sphere) : CCSMDebugObject(colour, opacity, flags)
{
	m_sphere = sphere;

	sysMemSet(m_points, 0, sizeof(m_points));
}

__COMMENT(virtual) void CCSMDebugSphere::Draw_grcVertex(Mat44V_In proj, Vec4V_In projWindow, int opacityShift) const
{
	const Color32 colour = GetColour(opacityShift);

	grcBegin(drawLineStrip, kNumSegments + 1);
	grcColor(colour);

	for (int i = 0; i < kNumSegments; i++)
	{
		grcVertex2f(Project2D_grcVertex(m_points[i], proj, projWindow));
	}

	grcVertex2f(Project2D_grcVertex(m_points[0], proj, projWindow));

	grcEnd();
}

__COMMENT(virtual) void CCSMDebugSphere::Draw_grcVertex(int opacityShift) const
{
	const Color32 colour = GetColour(opacityShift);

	grcBegin(drawLineStrip, kNumSegments + 1);
	grcColor(colour);

	for (int i = 0; i < kNumSegments; i++)
	{
		grcVertex3f(m_points[i]);
	}

	grcVertex3f(m_points[0]);

	grcEnd();
}

__COMMENT(virtual) void CCSMDebugSphere::Draw_fwDebugDraw(Mat44V_In proj, Vec4V_In projWindow, int opacityShift) const
{
	const Color32 colour = GetColour(opacityShift);

	Vec2V p0 = Project2D(m_points[kNumSegments - 1], proj, projWindow);

	for (int i = 0; i < kNumSegments; i++)
	{
		const Vec2V p1 = Project2D(m_points[i], proj, projWindow);

		grcDebugDraw::Line(p0, p1, colour);
		p0 = p1;
	}
}

__COMMENT(virtual) void CCSMDebugSphere::Draw_fwDebugDraw(int opacityShift) const
{
	const Color32 colour = GetColour(opacityShift);

	Vec3V p0 = m_points[kNumSegments - 1];

	for (int i = 0; i < kNumSegments; i++)
	{
		const Vec3V p1 = m_points[i];

		grcDebugDraw::Line(p0, p1, colour);
		p0 = p1;
	}
}

#endif // __BANK
