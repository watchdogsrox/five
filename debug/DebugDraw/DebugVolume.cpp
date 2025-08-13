// ===============================
// debug/debugdraw/debugvolume.cpp
// (c) 2010 RockstarNorth
// ===============================

#if __BANK

#include "bank/bank.h"
#include "system/memory.h"

#include "fwmaths/vectorutil.h"
#include "fwutil/xmacro.h"

#include "renderer/Util/Util.h" // sadly, this relies on old Vector3 utility functions
#include "renderer/color.h"
#include "debug/debugdraw/debugvolume.h"
#include "debug/debugdraw/debugvolume_kDOP18.h"

RENDER_OPTIMISATIONS()

// ================================================================================================

CDebugVolumeColourStyle::CDebugVolumeColourStyle(const Vector4& colour, const Vector4& colourGlancing, const Vector4& colourExponent, const Vector4& colourBackface)
{
	Set(colour, colourGlancing, colourExponent, colourBackface);
}

void CDebugVolumeColourStyle::Set(const Vector4& colour, const Vector4& colourGlancing, const Vector4& colourExponent, const Vector4& colourBackface)
{
	m_colour         = colour;
	m_colourGlancing = colourGlancing;
	m_colourExponent = colourExponent;
	m_colourExpScale = 1.0f;
	m_colourBackface = colourBackface;
}

void CDebugVolumeColourStyle::Set(const Vector4& colour, const Vector4& colourGlancing, float colourExponent, const Vector4& colourBackface)
{
	Set(colour, colourGlancing, Vector4(1,1,1,1)*colourExponent, colourBackface);
}

Vector4 CDebugVolumeColourStyle::GetColour(float NdotV, bool bFacing, float opacity) const
{
	Vector4 colour(0,0,0,0);

	if (!bFacing)//NdotV < 0.0f)
	{
		colour = m_colourBackface;
	}
	else if (m_colourExpScale == 0.0f)
	{
		colour = m_colour;
	}
	else if (opacity > 0.0f) // extra guard against evaluating powf() in case opacity is zero
	{
		const float tl = logf(Clamp<float>(1.0f - NdotV, 0.0001f, 1.0f))*m_colourExpScale;
		const float tx = expf(tl*m_colourExponent.x);
		const float ty = expf(tl*m_colourExponent.y);
		const float tz = expf(tl*m_colourExponent.z);
		const float tw = expf(tl*m_colourExponent.w);

		colour = m_colour + (m_colourGlancing - m_colour)*Vector4(tx, ty, tz, tw);
	}

	colour.w *= opacity;

	return colour;
}

__COMMENT(explicit) CDebugVolumeColour::CDebugVolumeColour(const Vector4& colour) : m_colour(colour)
{
}

Vector4 CDebugVolumeColour::GetColour(float opacity) const
{
	Vector4 colour = m_colour;
	colour.w *= opacity;
	return colour;
}

// ================================================================================================

void CDebugVolumeEdge::Set(const Vector3& p0, const Vector3& p1, int faceIndex, int adjFaceIndex0, int adjFaceIndex1)
{
	m_points[0]              = p0;
	m_points[1]              = p1;
	m_faceIndex              = faceIndex;
	m_adjacentFaceIndices[0] = adjFaceIndex0;
	m_adjacentFaceIndices[1] = adjFaceIndex1;
}

void CDebugVolumeEdge::Draw(const Vector4& colour) const
{
	if (colour.w > 0.0f)
	{
		grcDebugDraw::Line(m_points[0], m_points[1], Color32(colour));
	}
}

// ================================================================================================

CDebugVolumeFace::CDebugVolumeFace()
{
	Clear();
}

void CDebugVolumeFace::Clear()
{
	m_plane      = Vector4(0,0,0,0);
	m_centroid   = Vector3(0,0,0);
	m_pointCount = 0;
}

void CDebugVolumeFace::SetQuad(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3)
{
	m_plane = BuildPlane(p0, p1, p2); // assume quad is planar, non-degenerate, and convex ..

	AddPoint(p0);
	AddPoint(p1);
	AddPoint(p2);
	AddPoint(p3);

	m_centroid = (p0 + p1 + p2 + p3)*0.25f;
}

void CDebugVolumeFace::AddPoint(const Vector3& p)
{
	if (m_pointCount < MAX_POINTS_PER_FACE)
	{
		m_points[m_pointCount++] = p;
	}
}

float CDebugVolumeFace::GetNdotV(const Vector3& camPos, const Vector3& camDir) const
{
	UNUSED_VAR(camDir);

	Vector3 P = m_centroid;//m_plane.GetVector3()*(-m_plane.w);
	Vector3 V = camPos - P;
	V.Normalize();

	return -m_plane.Dot3(V);
}

void CDebugVolumeFace::Draw(const Vector4& colour) const
{
	if (colour.w > 0.0f)
	{
		const Color32 c(colour);

		for (int i = 2; i < m_pointCount; i++)
		{
			grcDebugDraw::Poly(RCC_VEC3V(m_points[0]), RCC_VEC3V(m_points[i-1]), RCC_VEC3V(m_points[i]), c, false, true);
		}
	}
}

// ================================================================================================

CDebugVolume::CDebugVolume()
{
	Clear();
}

void CDebugVolume::ClearGeometry()
{
	m_faceCount     = 0;
	m_edgeCount     = 0;
	m_gridLineCount = 0;

	for (int i = 0; i < MAX_FACES; i++)
	{
		m_faces[i].Clear();
	}
}

void CDebugVolume::Clear()
{
	ClearGeometry();

	m_faceColourStyle      = CDebugVolumeColourStyle(Vector4(0.5f, 1.0f, 0.5f, 0.1f), Vector4(0.0f, 1.0f, 0.0f, 0.1f), 2.0f);
	m_edgeColour           = CDebugVolumeColour(Vector4(0.750f, 1.000f, 0.750f, 0.375f));
	m_edgeColourBackface   = CDebugVolumeColour(Vector4(0.750f, 0.750f, 1.000f, 0.375f));
	m_edgeColourSilhouette = CDebugVolumeColour(Vector4(1.000f, 1.000f, 1.000f, 1.000f));
	m_gridColourStyle      = CDebugVolumeColourStyle(Vector4(1.0f, 1.0f, 1.0f, 0.2f), Vector4(0.5f, 1.0f, 0.5f, 0.1f), 2.0f);

	if (gDebugVolumeGlobals.m_faceColourStyleEnabled)
	{
		m_faceColourStyle = gDebugVolumeGlobals.m_faceColourStyle;
	}

	if (gDebugVolumeGlobals.m_edgeColourStyleEnabled)
	{
		m_edgeColour           = gDebugVolumeGlobals.m_edgeColour;
		m_edgeColourBackface   = gDebugVolumeGlobals.m_edgeColourBackface;
		m_edgeColourSilhouette = gDebugVolumeGlobals.m_edgeColourSilhouette;
	}

	if (gDebugVolumeGlobals.m_gridColourStyleEnabled)
	{
		m_gridColourStyle = gDebugVolumeGlobals.m_gridColourStyle;
	}

	m_faceColourStyle     .m_colour        .w *= gDebugVolumeGlobals.m_faceOpacity;
	m_faceColourStyle     .m_colourGlancing.w *= gDebugVolumeGlobals.m_faceOpacity;
	m_faceColourStyle     .m_colourBackface.w *= gDebugVolumeGlobals.m_faceOpacity;
	m_edgeColour          .m_colour        .w *= gDebugVolumeGlobals.m_edgeOpacity;
	m_edgeColourBackface  .m_colour        .w *= gDebugVolumeGlobals.m_edgeOpacity;
	m_edgeColourSilhouette.m_colour        .w *= gDebugVolumeGlobals.m_edgeOpacity;
	m_gridColourStyle     .m_colour        .w *= gDebugVolumeGlobals.m_gridOpacity;
	m_gridColourStyle     .m_colourGlancing.w *= gDebugVolumeGlobals.m_gridOpacity;
	m_gridColourStyle     .m_colourBackface.w *= gDebugVolumeGlobals.m_gridOpacity;
}

__COMMENT(static) CDebugVolume& CDebugVolume::GetGlobalDebugVolume(bool bClearGeometry)
{
	USE_DEBUG_MEMORY();

	static CDebugVolume* vol = NULL;

	if (vol == NULL)
	{
		vol = rage_new CDebugVolume;
	}

	if (bClearGeometry)
	{
		vol->ClearGeometry();
	}

	return *vol;
}

CDebugVolume& CDebugVolume::SetFrustum(const Matrix34& basis, float z0, float z1, float tanVFOV, float aspect)
{
	const float x = tanVFOV*aspect;
	const float y = tanVFOV;
	Vector3 p[8];

	p[0] = VEC3V_TO_VECTOR3(Transform(RCC_MAT34V(basis), Vec3V(-x*z0, -y*z0, -z0)));
	p[1] = VEC3V_TO_VECTOR3(Transform(RCC_MAT34V(basis), Vec3V(+x*z0, -y*z0, -z0)));
	p[2] = VEC3V_TO_VECTOR3(Transform(RCC_MAT34V(basis), Vec3V(-x*z0, +y*z0, -z0)));
	p[3] = VEC3V_TO_VECTOR3(Transform(RCC_MAT34V(basis), Vec3V(+x*z0, +y*z0, -z0)));

	p[4] = VEC3V_TO_VECTOR3(Transform(RCC_MAT34V(basis), Vec3V(-x*z1, -y*z1, -z1)));
	p[5] = VEC3V_TO_VECTOR3(Transform(RCC_MAT34V(basis), Vec3V(+x*z1, -y*z1, -z1)));
	p[6] = VEC3V_TO_VECTOR3(Transform(RCC_MAT34V(basis), Vec3V(-x*z1, +y*z1, -z1)));
	p[7] = VEC3V_TO_VECTOR3(Transform(RCC_MAT34V(basis), Vec3V(+x*z1, +y*z1, -z1)));

	// 6---------------7
	// |\             /|
	// | \    [3]    / |
	// |  \         /  |
	// |   2-------3   |
	// |   |       |   |
	// |[0]|  [4]  |[1]|
	// |   |       |   |
	// |   0-------1   |
	// |  /         \  |
	// | /    [2]    \ |
	// |/             \|
	// 4---------------5

	ClearGeometry();

	const int   gridDivisions   = BANK_SWITCH(gDebugVolumeGlobals.m_gridDivisions, 12);
	const float gridPerspective = BANK_SWITCH(gDebugVolumeGlobals.m_gridPerspective, 1.0f);

	AddQuad(p[2],p[0],p[4],p[6], 4,2,5,3, z0, z1,     gridDivisions, gridPerspective); // face[0] nx: points={2,0,4,6} adjacentFaces={4,2,5,3}
	AddQuad(p[1],p[3],p[7],p[5], 4,3,5,2, z0, z1,     gridDivisions, gridPerspective); // face[1] px: points={1,3,7,5} adjacentFaces={4,3,5,2}
	AddQuad(p[0],p[1],p[5],p[4], 4,1,5,0, z0, z1,     gridDivisions, gridPerspective); // face[2] ny: points={0,1,5,4} adjacentFaces={4,1,5,0}
	AddQuad(p[3],p[2],p[6],p[7], 4,0,5,1, z0, z1,     gridDivisions, gridPerspective); // face[3] py: points={3,2,6,7} adjacentFaces={4,0,5,1}
	AddQuad(p[1],p[0],p[2],p[3], 2,0,3,1, 1.0f, 1.0f, gridDivisions, gridPerspective); // face[4] nz: points={1,0,2,3} adjacentFaces={2,0,3,1}
	AddQuad(p[4],p[5],p[7],p[6], 2,1,3,0, 1.0f, 1.0f, gridDivisions, gridPerspective); // face[5] pz: points={4,5,7,6} adjacentFaces={2,1,3,0}

	return *this;
}

CDebugVolume& CDebugVolume::SetFlatCone(const Matrix34& basis, float angleInDegrees, float length, int slices, int stacks)
{
	ClearGeometry();

	float* sinTheta = rage_new float[slices];
	float* cosTheta = rage_new float[slices];

	for (int i = 0; i < slices; i++) // precalculate sin/cos pairs (i know, this is stupid)
	{
		const float theta = 2.0f*PI*(float)i/(float)slices;

		sinTheta[i] = sinf(theta);
		cosTheta[i] = cosf(theta);
	}

	const float r_base = length*tanf(0.5f*DtoR*angleInDegrees); // radius at base
	const float r_apex = r_base/64.0f; // radius at apex

	for (int j = 0; j < stacks; j++)
	{
		const float t0 = (float)(j + 0)/(float)stacks;
		const float t1 = (float)(j + 1)/(float)stacks;

		Vector3 p0;
		Vector3 p1;

		// add first two vertices
		{
			const float r0 = r_apex + (r_base - r_apex)*t0;
			const float r1 = r_apex + (r_base - r_apex)*t1;

			p0 = VEC3V_TO_VECTOR3(Transform(RCC_MAT34V(basis), Vec3V(r0, 0.0f, -t0*length)));
			p1 = VEC3V_TO_VECTOR3(Transform(RCC_MAT34V(basis), Vec3V(r1, 0.0f, -t1*length)));
		}

		for (int i = 0; i < slices; i++)
		{
			const int ii = (i + 1)%slices;

			const float r0 = r_apex + (r_base - r_apex)*t0;
			const float r1 = r_apex + (r_base - r_apex)*t1;

			const Vector3 p2 = VEC3V_TO_VECTOR3(Transform(RCC_MAT34V(basis), Vec3V(r0*cosTheta[ii], r0*sinTheta[ii], -t0*length)));
			const Vector3 p3 = VEC3V_TO_VECTOR3(Transform(RCC_MAT34V(basis), Vec3V(r1*cosTheta[ii], r1*sinTheta[ii], -t1*length)));

			AddQuad(
				p2,
				p3,
				p1,
				p0,
				Clamp<int>(j - 0, 0, stacks - 1)*slices + (slices + i + 1)%slices, // adjacent to p2,p3
				Clamp<int>(j + 1, 0, stacks - 1)*slices + (slices + i - 0)%slices, // adjacent to p3,p1
				Clamp<int>(j + 0, 0, stacks - 1)*slices + (slices + i - 1)%slices, // adjacent to p1,p0
				Clamp<int>(j - 1, 0, stacks - 1)*slices + (slices + i + 0)%slices, // adjacent to p0,p2
				1.0f, // z0
				1.0f, // z1
				0,    // gridDivisions
				0.0f  // gridPerspective
			);

			p0 = p2;
			p1 = p3;
		}
	}

	delete[] sinTheta;
	delete[] cosTheta;

	return *this;
}

static __forceinline float _exp2(float x)
{
	return powf(2.0f, x);
}

static __forceinline float _log2(float x)
{
	return logf(x)/logf(2.0f);
}

void CDebugVolume::AddQuad(
	const Vector3& p0,
	const Vector3& p1,
	const Vector3& p2,
	const Vector3& p3,
	int            adjFaceIndex0,
	int            adjFaceIndex1,
	int            adjFaceIndex2,
	int            adjFaceIndex3,
	float          z0,
	float          z1,
	int            gridDivisions,
	float          gridPerspective
	)
{
	if (m_faceCount < MAX_FACES)
	{
		//       [2]
		//    3-------2
		//    |       |
		// [3]|       |[1]
		//    |       |
		//    0-------1
		//       [0]

	//	const int   gridDivisions   = BANK_SWITCH(gDebugVolumeGlobals.m_gridDivisions, 12);
	//	const float gridPerspective = BANK_SWITCH(gDebugVolumeGlobals.m_gridPerspective, 1.0f);
		const int   faceIndex       = m_faceCount++;

		m_faces[faceIndex].SetQuad(p0, p1, p2, p3);

		if (adjFaceIndex0 != INDEX_NONE) { AddEdge(p0, p1, faceIndex, adjFaceIndex0); }
		if (adjFaceIndex1 != INDEX_NONE) { AddEdge(p1, p2, faceIndex, adjFaceIndex1); }
		if (adjFaceIndex2 != INDEX_NONE) { AddEdge(p2, p3, faceIndex, adjFaceIndex2); }
		if (adjFaceIndex3 != INDEX_NONE) { AddEdge(p3, p0, faceIndex, adjFaceIndex3); }

		if (gridDivisions > 1)
		{
			for (int i = 1; i < gridDivisions; i++)
			{
				const float t = (float)i/(float)gridDivisions;
				const float r = t;

				AddGridLine(p0 + (p1 - p0)*r, p3 + (p2 - p3)*r, faceIndex);
			}

			for (int i = 1; i < gridDivisions; i++)
			{
				const float t = (float)i/(float)gridDivisions;
#if 0 // old way of doing nonlinear grids
				const float p = z1*t - gridPerspective*(z1 - z0)*t;
				const float q = z1   - gridPerspective*(z1 - z0)*t;
				const float r = p/q;
#else // new way of doing nonlinear grids
				const float r_nonlinear = (z0 == z1) ? t : (_exp2(_log2(z0) + t*(_log2(z1) - _log2(z0))) - z0)/(z1 - z0);
				const float r_linear    = t;
				const float r           = r_linear + (r_nonlinear - r_linear)*gridPerspective;
#endif
				AddGridLine(p0 + (p3 - p0)*r, p1 + (p2 - p1)*r, faceIndex);
			}
		}
	}
}

void CDebugVolume::AddEdge(const Vector3& p0, const Vector3& p1, int adjFaceIndex0, int adjFaceIndex1)
{
	if (m_edgeCount < MAX_EDGES)
	{
		m_edges[m_edgeCount++].Set(p0, p1, INDEX_NONE, adjFaceIndex0, adjFaceIndex1);
	}
}

void CDebugVolume::AddGridLine(const Vector3& p0, const Vector3& p1, int faceIndex)
{
	if (m_gridLineCount < MAX_GRID_LINES)
	{
		m_gridLines[m_gridLineCount++].Set(p0, p1, faceIndex, INDEX_NONE, INDEX_NONE);
	}
}

CDebugVolumeFace* CDebugVolume::AddFace()
{
	if (m_faceCount < MAX_FACES)
	{
		return &m_faces[m_faceCount++];
	}

	return NULL;
}

void CDebugVolume::BuildFaces(const Vector4* planes, int planeCount, const Vector3& origin, const Vector3& axis, float extent)
{
	Clear();

	for (int j = 0; j < planeCount && m_faceCount < MAX_FACES; j++)
	{
		const Vector3 p = PlaneProject(planes[j], origin);

		Vector3 x;
		Vector3 y;
		Vector3 z = planes[j].GetVector3();

		if (axis.Mag2() > 0.0f)
		{
			x.Cross(y, z);
			x.Normalize();
			y.Cross(z, x);
		}
		else
		{
			x.Cross(z, FindMinAbsAxis(z));
			x.Normalize();
			y.Cross(x, z);
		}

		x *= extent;
		y *= extent;

		Vector3 points[CDebugVolumeFace::MAX_POINTS_PER_FACE];
		int     pointCount = 0;

		points[pointCount++] = p - x - y;
		points[pointCount++] = p + x - y;
		points[pointCount++] = p + x + y;
		points[pointCount++] = p - x + y;

		for (int i = 0; i < planeCount; i++)
		{
			if (i != j)
			{
				Vector3 temp[CDebugVolumeFace::MAX_POINTS_PER_FACE];
				int     tempCount = PolyClip((Vec3V*)temp, CDebugVolumeFace::MAX_POINTS_PER_FACE, (const Vec3V*)points, pointCount, ((const Vec4V*)planes)[i]);

				sysMemCpy(points, temp, tempCount*sizeof(Vector3));
				pointCount = tempCount;

				if (pointCount == 0)
				{
					break;
				}
			}
		}

		if (pointCount > 0)
		{
			CDebugVolumeFace& face = m_faces[m_faceCount++];

			face.m_plane      = planes[j];
			face.m_centroid   = Vector3(0,0,0);
			face.m_pointCount = 0;

			for (int i = 0; i < pointCount; i++)
			{
				face.m_points[face.m_pointCount++] = points[i];
				face.m_centroid += points[i];
			}

			face.m_centroid *= 1.0f/(float)pointCount;
		}
	}
}

void CDebugVolume::BuildEdges(float tolerance)
{
	for (int faceIndex0 = 0; faceIndex0 < m_faceCount; faceIndex0++)
	{
		const CDebugVolumeFace& face0 = m_faces[faceIndex0];

		for (int side0 = 0; side0 < face0.m_pointCount; side0++)
		{
			const Vector3& face0_p0 = face0.m_points[(side0 + 0)];
			const Vector3& face0_p1 = face0.m_points[(side0 + 1)%face0.m_pointCount];

			if (MaxElement(Abs(RCC_VEC3V(face0_p1)) - Abs(RCC_VEC3V(face0_p0))).Getf() > tolerance)
			{
				int adjacentFaceIndex = INDEX_NONE;
				int adjacentFaceCount = 0;

				for (int faceIndex1 = 0; faceIndex1 < m_faceCount; faceIndex1++)
				{
					if (faceIndex1 != faceIndex0)
					{
						const CDebugVolumeFace& face1 = m_faces[faceIndex1];

						for (int side1 = 0; side1 < face1.m_pointCount; side1++)
						{
							const Vector3& face1_p0 = face1.m_points[(side1 + 0)];
							const Vector3& face1_p1 = face1.m_points[(side1 + 1)%face1.m_pointCount];

							if (MaxElement(Abs(RCC_VEC3V(face1_p1)) - Abs(RCC_VEC3V(face1_p0))).Getf() > tolerance)
							{
								const Vector3 d0 = face1_p0 - face0_p1;
								const Vector3 d1 = face1_p1 - face0_p0;

								const float dx = Max<float>(Abs<float>(d0.x), Abs<float>(d1.x));
								const float dy = Max<float>(Abs<float>(d0.y), Abs<float>(d1.y));
								const float dz = Max<float>(Abs<float>(d0.z), Abs<float>(d1.z));

								const float d = Max<float>(Max<float>(dx, dy), dz);

								if (d <= tolerance)
								{
									adjacentFaceIndex = faceIndex1;
									adjacentFaceCount++;
								}
							}
						}
					}
				}

				if (adjacentFaceCount != 1)
				{
					adjacentFaceIndex = INDEX_NONE; // treat as open edge
				}

				if (m_edgeCount < MAX_EDGES)
				{
					CDebugVolumeEdge& edge = m_edges[m_edgeCount++];

					edge.m_points[0]              = face0_p0;
					edge.m_points[1]              = face0_p1;
					edge.m_faceIndex              = INDEX_NONE;
					edge.m_adjacentFaceIndices[0] = faceIndex0;
					edge.m_adjacentFaceIndices[1] = adjacentFaceIndex;
				}
				else
				{
					return; // no more edges
				}
			}
		}
	}
}

void CDebugVolume::AddClippedGridLines(const Vector4& plane)
{
	for (int faceIndex = 0; faceIndex < m_faceCount; faceIndex++)
	{
		const CDebugVolumeFace& face = m_faces[faceIndex];

		if (face.m_pointCount > 2)
		{
			Vector3 edgePoints[2];
			bool    edgeFlags[2] = {false, false};

			Vector3 p0 = face.m_points[face.m_pointCount - 1];
			float   d0 = PlaneDistanceTo(plane, p0);

			for (int i = 0; i < face.m_pointCount; i++)
			{
				Vector3 p1 = face.m_points[i];
				float   d1 = PlaneDistanceTo(plane, p1);

				if (d0 < 0.0f && d1 >= 0.0f)
				{
					edgePoints[0] = (p0*d1 - p1*d0)/(d1 - d0);
					edgeFlags[0] = true;
				}
				else if (d1 < 0.0f && d0 >= 0.0f)
				{
					edgePoints[1] = (p0*d1 - p1*d0)/(d1 - d0);
					edgeFlags[1] = true;
				}

				p0 = p1;
				d0 = d1;
			}

			if (edgeFlags[0] && edgeFlags[1])
			{
				AddGridLine(edgePoints[0], edgePoints[1], faceIndex);
			}
		}
	}
}

void CDebugVolume::BuildFromKDOP18(const CkDOP18& kDOP18, float explode, float tolerance)
{
	CkDOP18Face kDOP18Faces[18];
	const int faceCount = kDOP18.BuildFaces(kDOP18Faces, NULL, tolerance);

	for (int faceIndex = 0; faceIndex < faceCount; faceIndex++)
	{
		CDebugVolumeFace* face = AddFace();

		if (face)
		{
			face->m_plane = PlaneNormalise(-kDOP18Faces[faceIndex].m_plane);

			for (int i = 0; i < kDOP18Faces[faceIndex].m_pointCount; i++)
			{
				const Vector3 p = kDOP18Faces[faceIndex].m_points[i] - face->m_plane.GetVector3()*explode;

				face->AddPoint(p);
				face->m_centroid += p;
			}

			face->m_centroid *= 1.0f/(float)kDOP18Faces[faceIndex].m_pointCount;
		}
	}

	BuildEdges();
}

void CDebugVolume::ConvertToKDOP18(float explode, float tolerance)
{
	CkDOP18 kDOP18;

	for (int faceIndex = 0; faceIndex < m_faceCount; faceIndex++)
	{
		const CDebugVolumeFace& face = m_faces[faceIndex];

		for (int i = 0; i < face.m_pointCount; i++)
		{
			kDOP18.AddPoint(face.m_points[i]);
		}
	}

	BuildFromKDOP18(kDOP18, explode, tolerance);
}

void CDebugVolume::BuildBounds(Vector3& centre, Vector3& extent, const Vector3& basisX, const Vector3& basisY, const Vector3& basisZ) const
{
	Vector3 boundsMin(0,0,0);
	Vector3 boundsMax(0,0,0);

	for (int faceIndex = 0; faceIndex < m_faceCount; faceIndex++)
	{
		const CDebugVolumeFace& face = m_faces[faceIndex];

		for (int i = 0; i < face.m_pointCount; i++)
		{
			const Vector3& p = face.m_points[i];
			const Vector3  b = Vector3(basisX.Dot(p), basisY.Dot(p), basisZ.Dot(p));

			if (faceIndex == 0 && i == 0)
			{
				boundsMin = b;
				boundsMax = b;
			}
			else
			{
				boundsMin.x = Min<float>(b.x, boundsMin.x);
				boundsMin.y = Min<float>(b.y, boundsMin.y);
				boundsMin.z = Min<float>(b.z, boundsMin.z);
				boundsMax.x = Max<float>(b.x, boundsMax.x);
				boundsMax.y = Max<float>(b.y, boundsMax.y);
				boundsMax.z = Max<float>(b.z, boundsMax.z);
			}
		}
	}

	centre = (boundsMax + boundsMin)*0.5f;
	extent = (boundsMax - boundsMin)*0.5f;
}

enum eDebugVolumePass // used internally
{
	eDVP_NONE       = INDEX_NONE,
	eDVP_FRONT      = 0,
	eDVP_BACK       = 1,
	eDVP_SILHOUETTE = 2,
	eDVP_COUNT
};

void CDebugVolume::Draw(const Vector3& camPos, const Vector3& camDir, float opacity) const
{
	Vector4 edgeColours[eDVP_COUNT];

	edgeColours[eDVP_FRONT     ] = m_edgeColour          .GetColour(opacity);
	edgeColours[eDVP_BACK      ] = m_edgeColourBackface  .GetColour(opacity);
	edgeColours[eDVP_SILHOUETTE] = m_edgeColourSilhouette.GetColour(opacity);

	float            faceNdotV[MAX_FACES];
	eDebugVolumePass faceCodes[MAX_FACES];
	eDebugVolumePass edgeCodes[MAX_EDGES];

	for (int i = 0; i < m_faceCount; i++)
	{
		faceNdotV[i] = m_faces[i].GetNdotV(camPos, camDir);
		faceCodes[i] = (faceNdotV[i] >= 0.0f) ? eDVP_FRONT : eDVP_BACK;
	}

	for (int i = 0; i < m_edgeCount; i++)
	{
		const CDebugVolumeEdge& edge = m_edges[i];

		if (edge.m_adjacentFaceIndices[0] == INDEX_NONE ||
			edge.m_adjacentFaceIndices[1] == INDEX_NONE)
		{
			edgeCodes[i] = eDVP_SILHOUETTE; // open edge
		}
		else if (edge.m_adjacentFaceIndices[0] > edge.m_adjacentFaceIndices[1])
		{
			edgeCodes[i] = eDVP_NONE; // don't draw flipped edge ..
		}
		else
		{
			const bool bFacing0 = (faceCodes[edge.m_adjacentFaceIndices[0]] == eDVP_FRONT);
			const bool bFacing1 = (faceCodes[edge.m_adjacentFaceIndices[1]] == eDVP_FRONT);

			if (bFacing0 != bFacing1)
			{
				edgeCodes[i] = eDVP_SILHOUETTE;
			}
			else if (bFacing0)
			{
				edgeCodes[i] = eDVP_FRONT;
			}
			else
			{
				edgeCodes[i] = eDVP_BACK;
			}
		}
	}

	for (int pass = 0; pass < eDVP_COUNT; pass++)
	{
		Vector4 gridColours[MAX_FACES];
		bool    gridFlags  [MAX_FACES]; // bitvector

		sysMemSet(gridFlags, 0, sizeof(gridFlags));

		for (int i = 0; i < m_gridLineCount; i++)
		{
			const CDebugVolumeEdge& gridLine = m_gridLines[i];
			const int faceIndex = gridLine.m_faceIndex;

			if ((int)faceCodes[faceIndex] == pass)
			{
				if (!gridFlags[faceIndex])
				{
					gridColours[faceIndex] = m_gridColourStyle.GetColour(faceNdotV[faceIndex], pass == eDVP_FRONT, opacity);
					gridFlags  [faceIndex] = true;
				}

				gridLine.Draw(gridColours[faceIndex]);
			}
		}

		for (int i = 0; i < m_edgeCount; i++)
		{
			if ((int)edgeCodes[i] == pass)
			{
				m_edges[i].Draw(edgeColours[pass]);
			}
		}

		for (int i = 0; i < m_faceCount; i++)
		{
			if ((int)faceCodes[i] == pass)
			{
				m_faces[i].Draw(m_faceColourStyle.GetColour(faceNdotV[i], pass == eDVP_FRONT, opacity));
			}
		}
	}
}

// ================================================================================================

CDebugVolumeGlobals gDebugVolumeGlobals;

CDebugVolumeGlobals::CDebugVolumeGlobals()
{
	sysMemSet(this, 0, sizeof(*this));

	m_faceColourStyle.m_colourExponent = Vector4(1,1,1,1);
	m_faceColourStyle.m_colourExpScale = 4.0f;
	m_gridColourStyle.m_colourExponent = Vector4(1,1,1,1);
	m_gridColourStyle.m_colourExpScale = 4.0f;

	m_faceOpacity     = 1.0f;
	m_edgeOpacity     = 1.0f;
	m_gridOpacity     = 0.5f;
	m_gridDivisions   = 18;
	m_gridPerspective = 1.0f;
}

void CDebugVolumeGlobals::AddWidgets(bkBank& bk)
{
	bk.PushGroup("Debug Volume", false);
	{
		ADD_WIDGET(bk, Slider, m_faceOpacity, 0.0f, 1.0f, 1.0f/512.0f);
		ADD_WIDGET(bk, Toggle, m_faceColourStyleEnabled);
		ADD_WIDGET(bk, Color , m_faceColourStyle.m_colour);
		ADD_WIDGET(bk, Color , m_faceColourStyle.m_colourGlancing);
		ADD_WIDGET(bk, Color , m_faceColourStyle.m_colourExponent);
		ADD_WIDGET(bk, Slider, m_faceColourStyle.m_colourExpScale, 0.0f, 8.0f, 1.0f/512.0f);
		ADD_WIDGET(bk, Color , m_faceColourStyle.m_colourBackface);

		ADD_WIDGET(bk, Slider, m_edgeOpacity, 0.0f, 1.0f, 1.0f/512.0f);
		ADD_WIDGET(bk, Toggle, m_edgeColourStyleEnabled);
		ADD_WIDGET(bk, Color , m_edgeColour.m_colour);
		ADD_WIDGET(bk, Color , m_edgeColourBackface.m_colour);
		ADD_WIDGET(bk, Color , m_edgeColourSilhouette.m_colour);

		ADD_WIDGET(bk, Slider, m_gridOpacity, 0.0f, 1.0f, 1.0f/512.0f);
		ADD_WIDGET(bk, Toggle, m_gridColourStyleEnabled);
		ADD_WIDGET(bk, Color , m_gridColourStyle.m_colour);
		ADD_WIDGET(bk, Color , m_gridColourStyle.m_colourGlancing);
		ADD_WIDGET(bk, Color , m_gridColourStyle.m_colourExponent);
		ADD_WIDGET(bk, Slider, m_gridColourStyle.m_colourExpScale, 0.0f, 8.0f, 1.0f/512.0f);
		ADD_WIDGET(bk, Color , m_gridColourStyle.m_colourBackface);
		ADD_WIDGET(bk, Slider, m_gridDivisions, 1, 32, 1);
		ADD_WIDGET(bk, Slider, m_gridPerspective, 0.0f, 1.0f, 1.0f/512.0f);

		ADD_WIDGET(bk, Toggle, m_fixDepthTest);
	}
	bk.PopGroup();
}

#endif // __BANK
