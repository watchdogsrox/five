// =========================
// debug/DebugGeometryUtil.h
// (c) 2013 RockstarNorth
// =========================

#ifndef _DEBUG_DEBUGGEOMETRYUTIL_H_
#define _DEBUG_DEBUGGEOMETRYUTIL_H_

#if __BANK

#include "vectormath/classes.h"

namespace rage { class fiStream; }
namespace rage { class grmModel; }
namespace rage { class grmGeometry; }
namespace rage { class grmShader; }
namespace rage { class rmcDrawable; }

class CEntity;

namespace GeometryUtil
{
	typedef bool (*GeometryFunc)(const grmModel& model, const grmGeometry& geom, const grmShader& shader, const grmShaderGroup& shaderGroup, int lodIndex, void* userData);
	typedef void (*TriangleFunc)(Vec3V_In v0, Vec3V_In v1, Vec3V_In v2, int index0, int index1, int index2, void* userData);

	int CountTrianglesForEntity(const CEntity* pEntity, int lod, GeometryFunc gfunc = NULL, void* userData = NULL, int boneIndex = -1);
	int AddTrianglesForEntity(const CEntity* pEntity, int lod, GeometryFunc gfunc, TriangleFunc tfunc, bool bUseEntityMatrix = true, Mat34V_In postTransform = Mat34V(V_IDENTITY), void* userData = NULL, int boneIndex = -1);
	int CountTrianglesForDrawable(const rmcDrawable* pDrawable, int lodIndex, const char* name, GeometryFunc gfunc = NULL, void* userData = NULL, int boneIndex = -1);
	int AddTrianglesForDrawable(const rmcDrawable* pDrawable, int lodIndex, const char* name, GeometryFunc gfunc, TriangleFunc tfunc, Mat34V_In matrix = Mat34V(V_IDENTITY), void* userData = NULL, int boneIndex = -1);

#if __PS3
	static const int EXTRACT_MAX_INDICES  = 24*1024*3;
	static const int EXTRACT_MAX_VERTICES = 24*1024;

	// TODO -- we can hide this function once all debug systems are converted over to the new geometry extraction interface
	void AllocateExtractData(Vec4V** out_extractVerts = NULL, u16** out_extractIndices = NULL);
#endif // __PS3
}

class CDumpGeometryToOBJ
{
public:
	CDumpGeometryToOBJ(const char* path, const char* materialPath = "../materials.mtl");
	~CDumpGeometryToOBJ();

	void MaterialGroupBegin(const char* materialName, const char* groupName);
	void MaterialGroupEnd();
	void MaterialBegin(const char* materialName) { MaterialGroupBegin(materialName, NULL); }
	void MaterialEnd() { MaterialGroupEnd(); }
	void GroupBegin(const char* groupName) { MaterialGroupBegin(NULL, groupName); }
	void GroupEnd() { MaterialGroupEnd(); }

	void AddTriangle(Vec3V_In p0, Vec3V_In p1, Vec3V_In p2);
	void AddQuad(Vec3V_In p0, Vec3V_In p1, Vec3V_In p2, Vec3V_In p3);
	void AddCylinder(Vec3V_In p0, Vec3V_In p1, ScalarV_In radius, int numSides);
	void AddBox(Vec3V_In bmin, Vec3V_In bmax);
	void AddBoxEdges(Vec3V_In bmin, Vec3V_In bmax, float radius, int numSides);
	void AddText(const char* format, ...);
	void Close();

private:
	fiStream* m_stream;
	char*     m_materialPath;
	int       m_materialCount;
	int       m_materialIndex;
	int       m_numTriangles;
	int       m_numQuads;
};

#endif // __BANK
#endif // _DEBUG_DEBUGGEOMETRYUTIL_H_
