// ====================================
// debug/DebugDraw/DebugVolume_kDOP18.h
// (c) 2011 RockstarNorth
// ====================================

#ifndef _DEBUG_DEBUGDRAW_DEBUGVOLUME_KDOP18_H_
#define _DEBUG_DEBUGDRAW_DEBUGVOLUME_KDOP18_H_

#if __BANK

#include "vector/vector4.h"

class CkDOP18Face
{
public:
	static const int kDOP18_MAX_POINTS = 8;

	Vector4 m_plane;
	Vector3 m_points[kDOP18_MAX_POINTS];
	int     m_pointCount;

	CkDOP18Face();
	CkDOP18Face(
		const Vector4& plane,
		const Vector3& p0,
		const Vector3& p1,
		const Vector3& p2,
		const Vector3& p3,
		const Vector3& p4,
		const Vector3& p5,
		const Vector3& p6,
		const Vector3& p7,
		float          tolerance,
		bool           bFlipped
		);

	void AddPoint(const Vector3& p, float tolerance);

	bool HasArea(float tolerance) const; // true if face has non-zero area
};

class CkDOP18_Compressed // 24 bytes
{
public:
	s16 px,nx,py,ny,pz,nz;
	u8  pxpy,nxpy,pxny,nxny; // xy edges
	u8  pypz,nypz,pynz,nynz; // yz edges
	u8  pzpx,nzpx,pznx,nznx; // zx edges
};

class CkDOP18
{
public:
	bool bInited;

	// faces
	float px;
	float nx;
	float py;
	float ny;
	float pz;
	float nz;

	// edges
	float pxpy;
	float nxpy;
	float pxny;
	float nxny;
	float pypz;
	float nypz;
	float pynz;
	float nynz;
	float pzpx;
	float nzpx;
	float pznx;
	float nznx;

	// corners
	//float pxpypz;
	//float nxpypz;
	//float pxnypz;
	//float nxnypz;
	//float pxpynz;
	//float nxpynz;
	//float pxnynz;
	//float nxnynz;

	CkDOP18();

	void Clear();

	void AddPoints(const Vector3* points, int pointCount);
	void AddPoint(const Vector3& p);

	void RemoveDiagonalZFaces();
	void SimulateQuantisation(float scale, bool bNormaliseDiagonals);

	int BuildFaces(CkDOP18Face* faces, Vector3* points96, float tolerance) const; // returns number of faces

	bool IntersectQ(const Vector3& point) const;
	bool IntersectQ(const CkDOP18& kDOP18) const;
};

#endif // __BANK
#endif // _DEBUG_DEBUGDRAW_DEBUGVOLUME_KDOP18_H_
