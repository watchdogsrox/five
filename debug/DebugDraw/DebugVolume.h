// =============================
// debug/debugdraw/debugvolume.h
// (c) 2010 RockstarNorth
// =============================

#ifndef _DEBUG_DEBUGDRAW_DEBUGVOLUME_H_
#define _DEBUG_DEBUGDRAW_DEBUGVOLUME_H_

#if __BANK

#include "vector/vector4.h"

#include "grcore/debugdraw.h" // NOTE -- this header includes debugdraw.h

class CkDOP18;

// ================================================================================================

class CDebugVolumeColourStyle // better name for this?
{
public:
	Vector4 m_colour;
	Vector4 m_colourGlancing;
	Vector4 m_colourExponent;
	float   m_colourExpScale; // so that values > 1.0 can be set with bank color picker
	Vector4 m_colourBackface;

	CDebugVolumeColourStyle() {}
	CDebugVolumeColourStyle(const Vector4& colour, const Vector4& colourGlancing, const Vector4& colourExponent, const Vector4& colourBackface = Vector4(0,0,0,0));

	void Set(const Vector4& colour, const Vector4& colourGlancing, const Vector4& colourExponent, const Vector4& colourBackface = Vector4(0,0,0,0));
	void Set(const Vector4& colour, const Vector4& colourGlancing, float          colourExponent, const Vector4& colourBackface = Vector4(0,0,0,0));

	Vector4 GetColour(float NdotV, bool bFacing, float opacity) const;
};

class CDebugVolumeColour
{
public:
	Vector4 m_colour;

	CDebugVolumeColour() {}
	explicit CDebugVolumeColour(const Vector4& colour);

	Vector4 GetColour(float opacity) const;
};

class CDebugVolumeEdge
{
public:
	Vector3 m_points[2];
	int     m_faceIndex;
	int     m_adjacentFaceIndices[2];

	void Set(const Vector3& p0, const Vector3& p1, int faceIndex, int adjFaceIndex0, int adjFaceIndex1);

	void Draw(const Vector4& colour) const;
};

class CDebugVolumeFace
{
public:
	static const int MAX_POINTS_PER_FACE = 32;

	Vector4 m_plane;
	Vector3 m_centroid;
	Vector3 m_points[MAX_POINTS_PER_FACE];
	int     m_pointCount;

	CDebugVolumeFace();

	void Clear();

	void SetQuad(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3);

	void AddPoint(const Vector3& p);

	float GetNdotV(const Vector3& camPos, const Vector3& camDir) const;
	void Draw(const Vector4& colour) const;
};

class CDebugVolume
{
public:
	static const int MAX_FACES      =  32 PS3_ONLY(*1);
	static const int MAX_EDGES      = 128 PS3_ONLY(*1);
	static const int MAX_GRID_LINES = 256 PS3_ONLY(*8);

	CDebugVolumeColourStyle m_faceColourStyle;
	CDebugVolumeFace        m_faces[MAX_FACES];
	int                     m_faceCount;

	CDebugVolumeColour      m_edgeColour;
	CDebugVolumeColour      m_edgeColourBackface;
	CDebugVolumeColour      m_edgeColourSilhouette;
	CDebugVolumeEdge        m_edges[MAX_EDGES];
	int                     m_edgeCount;

	CDebugVolumeColourStyle m_gridColourStyle;
	CDebugVolumeEdge        m_gridLines[MAX_GRID_LINES];
	int                     m_gridLineCount;

	CDebugVolume();

	void ClearGeometry();
	void Clear();

	static CDebugVolume& GetGlobalDebugVolume(bool bClearGeometry);

	CDebugVolume& SetFrustum(const Matrix34& basis, float z0, float z1, float tanVFOV, float aspect);
	CDebugVolume& SetFlatCone(const Matrix34& basis, float angleInDegrees, float length, int slices = 32, int stacks = 32);

	void AddQuad(
		const Vector3& p0,
		const Vector3& p1,
		const Vector3& p2,
		const Vector3& p3,
		int            adjFaceIndex0,
		int            adjFaceIndex1,
		int            adjFaceIndex2,
		int            adjFaceIndex3,
		float          z0 = 1.0f,
		float          z1 = 1.0f,
		int            gridDivisions = 12,
		float          gridPerspective = 1.0f
		);
	void AddEdge(const Vector3& p0, const Vector3& p1, int adjFaceIndex0, int adjFaceIndex1);
	void AddGridLine(const Vector3& p0, const Vector3& p1, int faceIndex);

	// alternate construction method: clip planes against each other
	CDebugVolumeFace* AddFace();
	void BuildFaces(const Vector4* planes, int planeCount, const Vector3& origin, const Vector3& axis, float extent);
	void BuildEdges(float tolerance = 0.001f);
	void AddClippedGridLines(const Vector4& plane);

	void BuildFromKDOP18(const CkDOP18& kDOP18, float explode, float tolerance);
	void ConvertToKDOP18(                       float explode, float tolerance);

	void BuildBounds(Vector3& centre, Vector3& extent, const Vector3& basisX = Vector3(1,0,0), const Vector3& basisY = Vector3(0,1,0), const Vector3& basisZ = Vector3(0,0,1)) const;

	void Draw(const Vector3& camPos, const Vector3& camDir, float opacity) const;
};

class CDebugVolumeFrustum : public CDebugVolume
{
public:
	CDebugVolumeFrustum(const Matrix34& basis, float z0, float z1, float tanVFOV, float aspect)
	{
		Clear();
		SetFrustum(basis, z0, z1, tanVFOV, aspect);
	}
};

class CDebugVolumeFlatCone : public CDebugVolume
{
public:
	CDebugVolumeFlatCone(const Matrix34& basis, float angleInDegrees, float length, int slices = 32, int stacks = 32)
	{
		Clear();
		SetFlatCone(basis, angleInDegrees, length, slices, stacks);
	}
};

class CDebugVolumeGlobals
{
public:
	float                   m_faceOpacity;
	bool                    m_faceColourStyleEnabled;
	CDebugVolumeColourStyle m_faceColourStyle;

	float                   m_edgeOpacity;
	bool                    m_edgeColourStyleEnabled;
	CDebugVolumeColour      m_edgeColour;
	CDebugVolumeColour      m_edgeColourBackface;
	CDebugVolumeColour      m_edgeColourSilhouette;

	float                   m_gridOpacity;
	bool                    m_gridColourStyleEnabled;
	CDebugVolumeColourStyle m_gridColourStyle;
	int                     m_gridDivisions;
	float                   m_gridPerspective;

	bool                    m_fixDepthTest; // must be false by default, for now ..

	CDebugVolumeGlobals();

	void AddWidgets(bkBank& bk);
};

extern CDebugVolumeGlobals gDebugVolumeGlobals;

#endif // __BANK
#endif // _DEBUG_DEBUGDRAW_DEBUGVOLUME_H_

