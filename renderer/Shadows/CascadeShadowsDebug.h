// ======================================
// renderer/Shadows/CascadeShadowsDebug.h
// (c) 2011 RockstarNorth
// ======================================

#ifndef _RENDERER_SHADOWS_CASCADESHADOWSDEBUG_H_
#define _RENDERER_SHADOWS_CASCADESHADOWSDEBUG_H_

#if __BANK

#include "vectormath/classes.h"
#include "vector/color32.h"

class CCSMDebugObject
{
public:
	CCSMDebugObject() {}
	CCSMDebugObject(const Color32& colour, float opacity, int flags) : m_flags(flags) { SetColour(colour, opacity); }
	virtual ~CCSMDebugObject() {}

	__forceinline bool IsFlagSet(int i) const;
	__forceinline bool IsWorldspace() const;
	__forceinline Color32 GetColour(int opacityShift = 0) const;
	__forceinline void SetColour(const Color32& colour, float opacity);

	virtual void Draw_grcVertex(Mat44V_In proj, Vec4V_In projWindow, int opacityShift = 0) const { UNUSED_VAR(proj); UNUSED_VAR(projWindow); UNUSED_VAR(opacityShift); }
	virtual void Draw_grcVertex(int opacityShift = 0) const { UNUSED_VAR(opacityShift); }
	virtual void Draw_fwDebugDraw(Mat44V_In proj, Vec4V_In projWindow, int opacityShift = 0) const { UNUSED_VAR(proj); UNUSED_VAR(projWindow); UNUSED_VAR(opacityShift); }
	virtual void Draw_fwDebugDraw(int opacityShift = 0) const { UNUSED_VAR(opacityShift); }

protected:
	Color32 m_colour;
	int     m_flags;
};

__forceinline bool CCSMDebugObject::IsFlagSet(int i) const
{
	return (m_flags & (1<<i)) != 0;
}

__forceinline bool CCSMDebugObject::IsWorldspace() const
{
	return m_flags == 0;
}

__forceinline Color32 CCSMDebugObject::GetColour(int opacityShift) const
{
	Color32 colour = m_colour;
	colour.SetAlpha(m_colour.GetAlpha() >> opacityShift);
	return colour;
}

__forceinline void CCSMDebugObject::SetColour(const Color32& colour, float opacity)
{
	m_colour = colour;
	m_colour.SetAlpha((int)Clamp<float>(opacity*(float)colour.GetAlpha(), 0.0f, 255.0f));
}

class CCSMDebugLine : public CCSMDebugObject
{
public:
	CCSMDebugLine(const Color32& colour, float opacity, int flags, Vec3V_In p0, Vec3V_In p1);

	virtual void Draw_grcVertex(Mat44V_In proj, Vec4V_In projWindow, int opacityShift = 0) const;
	virtual void Draw_grcVertex(int opacityShift = 0) const;
	virtual void Draw_fwDebugDraw(Mat44V_In proj, Vec4V_In projWindow, int opacityShift = 0) const;
	virtual void Draw_fwDebugDraw(int opacityShift = 0) const;

	Vec3V m_points[2];
};

class CCSMDebugTri : public CCSMDebugObject
{
public:
	CCSMDebugTri(const Color32& colour, float opacity, int flags, Vec3V_In p0, Vec3V_In p1, Vec3V_In p2);

	virtual void Draw_grcVertex(Mat44V_In proj, Vec4V_In projWindow, int opacityShift = 0) const;
	virtual void Draw_grcVertex(int opacityShift = 0) const;
	virtual void Draw_fwDebugDraw(Mat44V_In proj, Vec4V_In projWindow, int opacityShift = 0) const;
	virtual void Draw_fwDebugDraw(int opacityShift = 0) const;

	Vec3V m_points[3];
};

class CCSMDebugBox : public CCSMDebugObject
{
public:
	CCSMDebugBox(const Color32& colour, float opacity, int flags, Vec3V_In boxMin, Vec3V_In boxMax);
	CCSMDebugBox(const Color32& colour, float opacity, int flags, Mat34V_In viewToWorld, ScalarV_In tanHFOV, ScalarV_In tanVFOV, ScalarV_In z0, ScalarV_In z1);

	virtual void Draw_grcVertex(Mat44V_In proj, Vec4V_In projWindow, int opacityShift = 0) const;
	virtual void Draw_grcVertex(int opacityShift = 0) const;
	virtual void Draw_fwDebugDraw(Mat44V_In proj, Vec4V_In projWindow, int opacityShift = 0) const;
	virtual void Draw_fwDebugDraw(int opacityShift = 0) const;

	Vec3V m_points[8];
};

class CCSMDebugSphere : public CCSMDebugObject
{
public:
	CCSMDebugSphere(const Color32& colour, float opacity, int flags, Vec4V_In sphere);

	virtual void Draw_grcVertex(Mat44V_In proj, Vec4V_In projWindow, int opacityShift = 0) const;
	virtual void Draw_grcVertex(int opacityShift = 0) const;
	virtual void Draw_fwDebugDraw(Mat44V_In proj, Vec4V_In projWindow, int opacityShift = 0) const;
	virtual void Draw_fwDebugDraw(int opacityShift = 0) const;

	enum { kNumSegments = 64 };

	Vec4V m_sphere;
	Vec3V m_points[kNumSegments]; // TODO -- i'm using these temporarily to define the projected circle vertices, but really we should be able to derive these ..
};

#endif // __BANK
#endif // _RENDERER_SHADOWS_CASCADESHADOWSDEBUG_H_
