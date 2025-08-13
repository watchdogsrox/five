#ifndef INC_DEBUG_DRAW_STORE_H_
#define INC_DEBUG_DRAW_STORE_H_

// Rage headers
#include "grcore/debugdraw.h"
#include "fwsys/timer.h"
#include "fwtl/pool.h"
#include "system/criticalsection.h"
#include "vectormath/classes.h"

// Game headers

#if DEBUG_DRAW

class CDebugDrawable
{
public:
	CDebugDrawable(const Color32& colour, u32 uExpiryTime, u32 uKey)
		: m_colour(colour)
		, m_uCreationTime(fwTimer::GetTimeInMilliseconds())
		, m_uExpiryTime(uExpiryTime)
		, m_uKey(uKey)
	{
	}

	virtual ~CDebugDrawable() {}

	virtual void Render() const = 0;

	u32 GetCreationTime() const { return m_uCreationTime; }
	bool GetHasExpired() const  { return m_uExpiryTime != 0 && fwTimer::GetTimeInMilliseconds() > (m_uCreationTime + m_uExpiryTime); }
	u32 GetKey() const          { return m_uKey; }

protected:
	Color32 m_colour;
	u32  m_uCreationTime;
	u32  m_uExpiryTime;
	u32  m_uKey;
};

class CDebugDrawableAxis : public CDebugDrawable
{
public:
	CDebugDrawableAxis(Mat34V_In mtx, const Color32& colour, float scale, bool drawArrows, u32 uExpiryTime, u32 uKey)
		: CDebugDrawable(colour, uExpiryTime, uKey)
		, m_mtx(mtx)
		, m_fScale(scale)
		, m_bDrawArrows(drawArrows)
	{
	}

	virtual void Render() const { grcDebugDraw::Axis(m_mtx, m_fScale, m_bDrawArrows); }

private:
	Mat34V	m_mtx;
	float	m_fScale;
	bool	m_bDrawArrows;
};

class CDebugDrawableLine : public CDebugDrawable
{
public:
	CDebugDrawableLine(Vec3V_In vStart, Vec3V_In vEnd, const Color32& colour, u32 uExpiryTime, u32 uKey)
		: CDebugDrawable(colour, uExpiryTime, uKey)
		, m_vStart(vStart)
		, m_vEnd(vEnd)
	{
	}

	virtual void Render() const { grcDebugDraw::Line(m_vStart, m_vEnd, m_colour); }

private:
	Vec3V m_vStart;
	Vec3V m_vEnd;
};

class CDebugDrawableLine2d : public CDebugDrawable
{
public:
	CDebugDrawableLine2d(const Vector2& vStart, const Vector2& vEnd, const Color32& colour, u32 uExpiryTime, u32 uKey)
		: CDebugDrawable(colour, uExpiryTime, uKey)
		, m_vStart(vStart)
		, m_vEnd(vEnd)
	{
	}

	virtual void Render() const { grcDebugDraw::Line(m_vStart, m_vEnd, m_colour); }

private:
	Vector2 m_vStart;
	Vector2 m_vEnd;
};

class CDebugDrawableVectorMapLine : public CDebugDrawable
{
public:
	CDebugDrawableVectorMapLine(Vec3V_In vStart, Vec3V_In vEnd, const Color32& colour, u32 uExpiryTime, u32 uKey)
		: CDebugDrawable(colour, uExpiryTime, uKey)
		, m_vStart(vStart)
		, m_vEnd(vEnd)
	{
	}

	virtual void Render() const; 

private:
	Vec3V m_vStart;
	Vec3V m_vEnd;
};

class CDebugDrawableVectorMapCircle : public CDebugDrawable
{
public:
	CDebugDrawableVectorMapCircle(Vec3V_In vPos, float fRadius, const Color32& colour, u32 uExpiryTime, u32 uKey)
		: CDebugDrawable(colour, uExpiryTime, uKey)
		, m_vPos(vPos)
		, m_fRadius(fRadius)
	{
	}

	virtual void Render() const; 

private:
	Vec3V m_vPos;
	float   m_fRadius;
};

class CDebugDrawableSphere : public CDebugDrawable
{
public:
	CDebugDrawableSphere(Vec3V_In vPos, float fRadius, const Color32& colour, u32 uExpiryTime, u32 uKey, bool bSolid, u32 uNumSteps)
		: CDebugDrawable(colour, uExpiryTime, uKey)
		, m_vPos(vPos)
		, m_fRadius(fRadius)
		, m_bSolid(bSolid)
		, m_uNumSteps(uNumSteps)
	{
	}

	virtual void Render() const { grcDebugDraw::Sphere(m_vPos, m_fRadius, m_colour, m_bSolid, 1, m_uNumSteps); }

private:
	Vec3V m_vPos;
	float   m_fRadius;
	u32		m_uNumSteps;
	bool	m_bSolid;
};

class CDebugDrawableCone : public CDebugDrawable
{
public:
	CDebugDrawableCone(Vec3V_In vStart, Vec3V_In vEnd, float fRadius, const Color32& colour, bool cap, bool solid, int numSides, u32 uExpiryTime, u32 uKey)
		: CDebugDrawable(colour, uExpiryTime, uKey)
		, m_vStart(vStart)
		, m_vEnd(vEnd)
		, m_fRadius(fRadius)
		, m_bCap(cap)
		, m_bSolid(solid)
		, m_iNumSides(numSides)
	{
	}

	virtual void Render() const { grcDebugDraw::Cone(m_vStart, m_vEnd, m_fRadius, m_colour, m_bCap, m_bSolid, m_iNumSides); }

private:
	Vec3V	m_vStart;
	Vec3V	m_vEnd;
	float   m_fRadius;
	bool	m_bCap;
	bool	m_bSolid;
	int		m_iNumSides;
};

class CDebugDrawableCapsule : public CDebugDrawable
{
public:
	CDebugDrawableCapsule(Vec3V_In vStart, Vec3V_In vEnd, float fRadius, const Color32& colour, u32 uExpiryTime, u32 uKey, bool bSolid)
		: CDebugDrawable(colour, uExpiryTime, uKey)
		, m_vStart(vStart)
		, m_vEnd(vEnd)
		, m_fRadius(fRadius)
		, m_bSolid(bSolid)
	{
	}

	virtual void Render() const { grcDebugDraw::Capsule(m_vStart, m_vEnd, m_fRadius, m_colour, m_bSolid); }

private:
	Vec3V	m_vStart;
	Vec3V	m_vEnd;
	float   m_fRadius;
	bool	m_bSolid;
};

class CDebugDrawableText : public CDebugDrawable
{
public:
	CDebugDrawableText(Vec3V_In vPos, const s32 iScreenOffsetX, const s32 iScreenOffsetY, const char* text, const Color32& colour, u32 uExpiryTime, u32 uKey, bool is2D = false)
		: CDebugDrawable(colour, uExpiryTime, uKey)
		, m_vPos(vPos)
		, m_iScreenOffsetX(iScreenOffsetX)
		, m_iScreenOffsetY(iScreenOffsetY)
		, m_is2D(is2D)
	{
		Assertf(strlen(text) < 64, "Text too big");
		strcpy(m_text, text);
	}

	virtual void Render() const { if (m_is2D) grcDebugDraw::Text(Vec2V(m_vPos.GetXf(), m_vPos.GetYf()), m_colour, m_text); else grcDebugDraw::Text(m_vPos, m_colour, m_iScreenOffsetX, m_iScreenOffsetY, m_text); }

private:
	Vec3V	m_vPos;
	s32	m_iScreenOffsetX;
	s32	m_iScreenOffsetY;
	char    m_text[64];
	bool	m_is2D;
};

class CDebugDrawableArrow : public CDebugDrawable
{
public:
	CDebugDrawableArrow(Vec3V_In vStart, Vec3V_In vEnd, float fArrowHeadSize, const Color32& colour, u32 uExpiryTime, u32 uKey)
		: CDebugDrawable(colour, uExpiryTime, uKey)
		, m_vStart(vStart)
		, m_vEnd(vEnd)
		, m_fArrowHeadSize(fArrowHeadSize)
	{
	}

	virtual void Render() const { grcDebugDraw::Arrow(m_vStart, m_vEnd, m_fArrowHeadSize, m_colour); }

private:
	Vec3V m_vStart;
	Vec3V m_vEnd;
	float	m_fArrowHeadSize;
};

class CDebugDrawableCross : public CDebugDrawable
{
public:
	CDebugDrawableCross(Vec3V_In vPos, float fSize, const Color32& colour, u32 uExpiryTime, u32 uKey)
		: CDebugDrawable(colour, uExpiryTime, uKey)
		, m_vPos(vPos)
		, m_fSize(fSize)
	{
	}

	virtual void Render() const { grcDebugDraw::Cross(m_vPos, m_fSize, m_colour); }

private:
	Vec3V	m_vPos;
	float   m_fSize;
};

class CDebugDrawableBoxOrientated : public CDebugDrawable
{
public:
	CDebugDrawableBoxOrientated(Vec3V_In vMin, Vec3V_In vMax, Mat34V_In mat, const Color32& color, u32 uExpiryTime, u32 uKey)
		: CDebugDrawable(color, uExpiryTime, uKey)
		, m_vMin(vMin)
		, m_vMax(vMax)
		, m_mat(mat)
	{
	}

	virtual void Render() const { grcDebugDraw::BoxOriented(m_vMin, m_vMax, m_mat, m_colour, false); }

private:
	Vec3V m_vMin;
	Vec3V m_vMax;
	Mat34V m_mat;
};

class CDebugDrawablePoly : public CDebugDrawable
{
public:
	CDebugDrawablePoly(Vec3V_In v1, Vec3V_In v2, Vec3V_In v3, const Color32& color, u32 uExpiryTime, u32 uKey)
		: CDebugDrawable(color, uExpiryTime, uKey)
		, m_v1(v1)
		, m_v2(v2)
		, m_v3(v3)
	{
	}

	virtual void Render() const { grcDebugDraw::Poly(m_v1, m_v2, m_v3, m_colour,false,false); }

private:
	Vec3V m_v1;
	Vec3V m_v2;
	Vec3V m_v3;
};

class CDebugDrawStore
{
public:

	CDebugDrawStore();
	CDebugDrawStore(s32 iStoreMax);

	// Initialize the debug draw store size
	void InitStoreSize(s32 iStoreMax);

	// Add a axis
	void AddAxis(Mat34V_In mtx, float scale, bool drawArrows, u32 uExpiryTime = 0, u32 uKey = 0);

	// Add a line
	void AddLine(Vec3V_In vStart, Vec3V_In vEnd, const Color32& colour, u32 uExpiryTime = 0, u32 uKey = 0);

	// Add a line to vector map
	void AddVectorMapLine(Vec3V_In vStart, Vec3V_In vEnd, const Color32& colour, u32 uExpiryTime = 0, u32 uKey = 0);

	// Add a line in screen space coordinates
	void AddLine(const Vector2& vStart, const Vector2& vEnd, const Color32& colour, u32 uExpiryTime = 0, u32 uKey = 0);

	// Add a circle to vector map
	void AddVectorMapCircle(Vec3V_In vPos, float fRadius, const Color32& colour, u32 uExpiryTime = 0, u32 uKey = 0);

	// Add a sphere
	void AddSphere(Vec3V_In vPos, float fRadius, const Color32& colour, u32 uExpiryTime = 0, u32 uKey = 0, bool bSolid = true, u32 uNumSteps=8);

	// Add a cone
	void AddCone(Vec3V_In start, Vec3V_In end, float r, const Color32 col, bool cap = true, bool solid = true, int numSides = 12, u32 uExpiryTime = 0, u32 uKey = 0);
	
	// Add a capsule
	void AddCapsule(Vec3V_In vStart, Vec3V_In vEnd, float fRadius, const Color32& colour, u32 uExpiryTime = 0, u32 uKey = 0, bool bSolid = true);

	// Add text
	void AddText(Vec3V_In vPos, const s32 iScreenOffsetX, const s32 iScreenOffsetY, const char* szText, const Color32& colour, u32 uExpiryTime = 0, u32 uKey = 0);

	void Add2DText(Vec3V_In vPos, const char* szText, const Color32& colour, u32 uExpiryTime = 0, u32 uKey = 0);

	// Add an orientated bounding box
	// min and max are in local space of matrix
	void AddOBB(Vec3V_In vMin, Vec3V_In vMax, const Mat34V& mat, const Color32& colour, u32 uExpiryTime = 0, u32 uKey = 0);

	// Add a triangle
	void AddPoly(Vec3V_In v1, Vec3V_In v2, Vec3V_In v3, const Color32& colour, u32 uExpiryTime = 0, u32 uKey = 0);

	// Add arrow
	void AddArrow(Vec3V_In vStart, Vec3V_In vEnd, float fArrowHeadSize, const Color32& colour, u32 uExpiryTime = 0, u32 uKey = 0);

	// Add cross
	void AddCross(Vec3V_In pos, const float size, const Color32& col, u32 uExpiryTime = 0, u32 uKey = 0);

	// Render
	void Render();

	// Delete all
	void ClearAll();

	// Delete specific drawable
	void Clear(u32 uKey);

private:

	// Returns a pointer to the storage address
	// If a key is valid, it will return a pointer to the first item with that key
	// If the pool is full, it will delete the oldest drawable to make space
	CDebugDrawable* GetStorage(u32 uKey);

	// Delete the oldest drawable
	void DeleteOldestDrawable();

	// Delete drawable
	void Delete(CDebugDrawable* pDrawable);

	//
	// Members
	//

	// Our pool of CDebugDrawable's
	typedef fwPool<CDebugDrawable> DebugDrawPool;
	DebugDrawPool* m_pPool;

	// For thread safety
	sysCriticalSectionToken m_criticalSection;
};

#endif // DEBUG_DRAW

#endif // INC_DEBUG_DRAW_STORE_H_
