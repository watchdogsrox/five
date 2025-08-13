// ===========================
// debug/DebugGeometryUtil.cpp
// (c) 2013 RockstarNorth
// ===========================

#if __BANK

#include "atl/array.h"
#include "file/stream.h"
#if __PS3
#include <cell/rtc.h>
#include "grcore/edgeExtractgeomspu.h"
#endif // __PS3
#include "grcore/fvf.h"
#include "grcore/vertexbuffer.h"
#include "grcore/indexbuffer.h"
#include "grmodel/geometry.h"
#include "grmodel/model.h"
#include "rmcore/drawable.h"
#include "string/stringutil.h"
#include "system/memory.h"
#include "vectormath/classes.h"

#include "fwmaths/vectorutil.h"
#include "fwutil/xmacro.h"

#include "debug/DebugGeometryUtil.h"
#include "scene/Entity.h"

int GeometryUtil::CountTrianglesForEntity(const CEntity* pEntity, int lod, GeometryFunc gfunc, void* userData, int boneIndex)
{
	return AddTrianglesForEntity(pEntity, lod, gfunc, NULL, false, Mat34V(V_ZERO), userData, boneIndex);
}

int GeometryUtil::AddTrianglesForEntity(const CEntity* pEntity, int lod, GeometryFunc gfunc, TriangleFunc tfunc, bool bUseEntityMatrix, Mat34V_In postTransform, void* userData, int boneIndex)
{
	if (pEntity)
	{
		Mat34V matrix;

		if (bUseEntityMatrix)
		{
			Transform(matrix, postTransform, pEntity->GetMatrix());
		}
		else
		{
			matrix = postTransform;
		}

		return AddTrianglesForDrawable(pEntity->GetDrawable(), lod, pEntity->GetModelName(), gfunc, tfunc, matrix, userData, boneIndex);
	}
	else
	{
		return 0;
	}
}

int GeometryUtil::CountTrianglesForDrawable(const rmcDrawable* pDrawable, int lodIndex, const char* name, GeometryFunc gfunc, void* userData, int boneIndex)
{
	return AddTrianglesForDrawable(pDrawable, lodIndex, name, gfunc, NULL, Mat34V(V_ZERO), userData, boneIndex);
}

#if __PS3

namespace rage { extern u32 g_AllowVertexBufferVramLocks; }

namespace GeometryUtil {

static Vec4V* g_extractVertStreams[CExtractGeomParams::obvIdxMax] ;
static Vec4V* g_extractVerts = NULL;
static u16*   g_extractIndices = NULL;

} // namespace GeometryUtil

void GeometryUtil::AllocateExtractData(Vec4V** out_extractVerts, u16** out_extractIndices)
{
	USE_DEBUG_MEMORY();

	if (g_extractVerts   == NULL) { g_extractVerts   = rage_aligned_new(16) Vec4V[EXTRACT_MAX_VERTICES*1]; } // pos-only
	if (g_extractIndices == NULL) { g_extractIndices = rage_aligned_new(16) u16[EXTRACT_MAX_INDICES]; }

	if (out_extractVerts  ) { *out_extractVerts   = g_extractVerts; }
	if (out_extractIndices) { *out_extractIndices = g_extractIndices; }
}

#endif // __PS3

int GeometryUtil::AddTrianglesForDrawable(const rmcDrawable* pDrawable, int lodIndex, const char* ASSERT_ONLY(name), GeometryFunc gfunc, TriangleFunc tfunc, Mat34V_In matrix, void* userData, int boneIndex)
{
	int numTrianglesTotal = 0;

	if (pDrawable)
	{
		const rmcLodGroup& lodGroup = pDrawable->GetLodGroup();

		const int lodIndex0 = (lodIndex >= 0) ? (lodIndex + 0) : 0;
		const int lodIndex1 = (lodIndex >= 0) ? (lodIndex + 1) : LOD_COUNT;

		for (lodIndex = lodIndex0; lodIndex < lodIndex1; lodIndex++)
		{
			if (lodGroup.ContainsLod(lodIndex))
			{
				const rmcLod& lod = lodGroup.GetLod(lodIndex);

				for (int lodModelIndex = 0; lodModelIndex < lod.GetCount(); lodModelIndex++)
				{
					const grmModel* pModel = lod.GetModel(lodModelIndex);

					if (pModel)
					{
						const grmModel& model = *pModel;

						for (int geomIndex = 0; geomIndex < model.GetGeometryCount(); geomIndex++)
						{
							const grmGeometry& geom = model.GetGeometry(geomIndex);
							const grmShader& shader = pDrawable->GetShaderGroup().GetShader(model.GetShaderIndex(geomIndex));

							if (gfunc && !gfunc(model, geom, shader, pDrawable->GetShaderGroup(), lodIndex, userData))
							{
								continue;
							}
#if __PS3
							if (geom.GetType() == grmGeometry::GEOMETRYEDGE)
							{
								const grmGeometryEdge *geomEdge = static_cast<const grmGeometryEdge*>(&geom);

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

								if (tfunc || boneIndex != -1)
								{
									if (totalI > EXTRACT_MAX_INDICES)
									{
										Assertf(0, "%s: index buffer has more indices (%d) than system can handle (%d)", name, totalI, EXTRACT_MAX_INDICES);
										return 0;
									}

									if (totalV > EXTRACT_MAX_VERTICES)
									{
										Assertf(0, "%s: vertex buffer has more verts (%d) than system can handle (%d)", name, totalV, EXTRACT_MAX_VERTICES);
										return 0;
									}

									AllocateExtractData();

									sysMemSet(&g_extractVertStreams[0], 0, sizeof(g_extractVertStreams));

									u8  BoneIndexesAndWeights[2048];
									int BoneOffset1     = 0;
									int BoneOffset2     = 0;
									int BoneOffsetPoint = 0;
									int BoneIndexOffset = 0;
									int BoneIndexStride = 1;

									int numVerts = 0;

									const int numIndices = geomEdge->GetVertexAndIndex(
										(Vector4*)g_extractVerts,
										EXTRACT_MAX_VERTICES,
										(Vector4**)g_extractVertStreams,
										g_extractIndices,
										EXTRACT_MAX_INDICES,
										BoneIndexesAndWeights,
										sizeof(BoneIndexesAndWeights),
										&BoneIndexOffset,
										&BoneIndexStride,
										&BoneOffset1,
										&BoneOffset2,
										&BoneOffsetPoint,
										(u32*)&numVerts,
#if HACK_GTA4_MODELINFOIDX_ON_SPU
										&gtaSpuInfoStruct,
#endif // HACK_GTA4_MODELINFOIDX_ON_SPU
										NULL,
										CExtractGeomParams::extractPos
									);

									// make sure these match
									Assertf(numIndices == totalI, "numIndices=%d, totalI=%d", numIndices, totalI);

									for (int i = 0; i < numIndices; i += 3)
									{
										const u16 index0 = g_extractIndices[i + 0];
										const u16 index1 = g_extractIndices[i + 1];
										const u16 index2 = g_extractIndices[i + 2];

										const Vec3V v0 = g_extractVerts[index0].GetXYZ();
										const Vec3V v1 = g_extractVerts[index1].GetXYZ();
										const Vec3V v2 = g_extractVerts[index2].GetXYZ();

										const Vec3V p0 = Transform(matrix, v0);
										const Vec3V p1 = Transform(matrix, v1);
										const Vec3V p2 = Transform(matrix, v2);

										if (boneIndex != INDEX_NONE)
										{
											int blendBoneIndex = BoneIndexesAndWeights[index0*BoneIndexStride + BoneIndexOffset];

											if (blendBoneIndex >= BoneOffsetPoint)
											{
												blendBoneIndex += BoneOffset2 - BoneOffsetPoint;
											}
											else
											{
												blendBoneIndex += BoneOffset1;
											}

											if (blendBoneIndex != boneIndex)
											{
												continue;
											}
										}

										if (tfunc)
										{
											tfunc(p0, p1, p2, index0, index1, index2, userData);
										}

										numTrianglesTotal++;
									}
								}
								else
								{
									numTrianglesTotal += totalI/3;
								}
							}
							else
#endif // __PS3
							{
								if (tfunc)
								{
									grcVertexBuffer* vb = const_cast<grmGeometry&>(geom).GetVertexBuffer(true);
									grcIndexBuffer*  ib = const_cast<grmGeometry&>(geom).GetIndexBuffer (true);

									Assert(vb && ib);
									Assert(geom.GetPrimitiveType() == drawTris);
									Assert(geom.GetPrimitiveCount()*3 == (u32)ib->GetIndexCount());

									const int  numTriangles  = ib->GetIndexCount()/3;
									const u16* indexData     = ib->LockRO();
									const u16* matrixPalette = geom.GetMatrixPalette();

									PS3_ONLY(++g_AllowVertexBufferVramLocks); // PS3: attempting to edit VBs in VRAM
									grcVertexBufferEditor vertexBufferEditor(vb, true, true); // lock=true, readOnly=true
									const int vertexCount = vertexBufferEditor.GetVertexBuffer()->GetVertexCount();

									for (int i = 0; i < numTriangles; i++)
									{
										const u16 index0 = indexData[i*3 + 0];
										const u16 index1 = indexData[i*3 + 1];
										const u16 index2 = indexData[i*3 + 2];

										if (Max<u16>(index0, index1, index2) < vertexCount)
										{
											const Vec3V v0 = VECTOR3_TO_VEC3V(vertexBufferEditor.GetPosition(index0));
											const Vec3V v1 = VECTOR3_TO_VEC3V(vertexBufferEditor.GetPosition(index1));
											const Vec3V v2 = VECTOR3_TO_VEC3V(vertexBufferEditor.GetPosition(index2));

											const Vec3V p0 = Transform(matrix, v0);
											const Vec3V p1 = Transform(matrix, v1);
											const Vec3V p2 = Transform(matrix, v2);

											if (boneIndex != INDEX_NONE && matrixPalette && vb->GetFvf()->GetBindingsChannel())
											{
												const int blendBoneIndex = matrixPalette[vertexBufferEditor.GetBlendIndices(index0).GetRed()];

												if (blendBoneIndex != boneIndex)
												{
													continue;
												}
											}

											if (tfunc)
											{
												tfunc(p0, p1, p2, index0, index1, index2, userData);
											}
										}
										else
										{
											Assertf(0, "%s: indices out of range! %d,%d,%d should be < %d (0x%p + %d)", name, index0, index1, index2, vertexCount, indexData, i*3);
											break;
										}
									}

									PS3_ONLY(--g_AllowVertexBufferVramLocks); // PS3: finished with editing VBs in VRAM

									ib->UnlockRO();
								}

								numTrianglesTotal += geom.GetPrimitiveCount();
							}
						}
					}
				}
			}
		}
	}

	return numTrianglesTotal;
}

// ================================================================================================

CDumpGeometryToOBJ::CDumpGeometryToOBJ(const char* path, const char* materialPath)
{
	m_stream        = fiStream::Create(path);
	m_materialPath  = NULL;
	m_materialCount = 0;
	m_materialIndex = INDEX_NONE;
	m_numTriangles  = 0;
	m_numQuads      = 0;

	if (materialPath && materialPath[0] != '\0')
	{
		const int materialPathLen = istrlen(materialPath);
		m_materialPath = rage_new char[materialPathLen + 1];
		sysMemCpy(m_materialPath, materialPath, materialPathLen + 1);
	}
}

CDumpGeometryToOBJ::~CDumpGeometryToOBJ()
{
	Assert(m_stream == NULL && m_materialPath == NULL && m_numTriangles == 0 && m_numQuads == 0);
}

void CDumpGeometryToOBJ::MaterialGroupBegin(const char* materialName, const char* groupName)
{
	Assert(m_materialIndex == INDEX_NONE);

	if (m_stream)
	{
		if (materialName && m_materialCount == 0 && AssertVerify(m_materialPath))
		{
			fprintf(m_stream, "mtllib %s\n", m_materialPath);
		}

		if (groupName)
		{
			fprintf(m_stream, "g %s\n", groupName);
		}
		else
		{
			fprintf(m_stream, "g %d\n", m_materialCount + 1);
		}
		
		if (materialName)
		{
			fprintf(m_stream, "usemtl %s\n", materialName);
		}
	}

	m_materialIndex = m_materialCount++;
}

void CDumpGeometryToOBJ::MaterialGroupEnd()
{
	Assert(m_numTriangles == 0 || m_numQuads == 0); // can't have both in the same material group!
	Assert((m_materialCount == 0 && m_materialIndex == INDEX_NONE) || (m_materialCount > 0 && m_materialIndex == m_materialCount - 1));

	if (m_stream)
	{
		for (int i = 0; i < m_numTriangles; i++)
		{
			if (m_materialIndex == INDEX_NONE)
			{
				fprintf(m_stream, "f %d %d %d\n", 1 + 3*i, 2 + 3*i, 3 + 3*i);
			}
			else
			{
				fprintf(m_stream, "f %d %d %d\n", -(3 + 3*i), -(2 + 3*i), -(1 + 3*i));
			}
		}

		for (int i = 0; i < m_numQuads; i++)
		{
			if (m_materialIndex == INDEX_NONE)
			{
				fprintf(m_stream, "f %d %d %d %d\n", 1 + 4*i, 2 + 4*i, 3 + 4*i, 4 + 4*i);
			}
			else
			{
				fprintf(m_stream, "f %d %d %d %d\n", -(4 + 4*i), -(3 + 4*i), -(2 + 4*i), -(1 + 4*i));
			}
		}
	}

	m_materialIndex = INDEX_NONE;
	m_numTriangles  = 0;
	m_numQuads      = 0;
}

void CDumpGeometryToOBJ::AddTriangle(Vec3V_In p0, Vec3V_In p1, Vec3V_In p2)
{
	//Assert(m_numQuads == 0); // can't have both in the same material group!
	Assert((m_materialCount == 0 && m_materialIndex == INDEX_NONE) || (m_materialCount > 0 && m_materialIndex == m_materialCount - 1));

	if (m_stream)
	{
		if (m_numQuads > 0)
		{
			fprintf(m_stream, "v %f %f %f\n", VEC3V_ARGS(p0));
			fprintf(m_stream, "v %f %f %f\n", VEC3V_ARGS(p1));
			fprintf(m_stream, "v %f %f %f\n", VEC3V_ARGS(p2));
			fprintf(m_stream, "v %f %f %f\n", VEC3V_ARGS(p2));
			m_numQuads++;
		}
		else
		{
			fprintf(m_stream, "v %f %f %f\n", VEC3V_ARGS(p0));
			fprintf(m_stream, "v %f %f %f\n", VEC3V_ARGS(p1));
			fprintf(m_stream, "v %f %f %f\n", VEC3V_ARGS(p2));
			m_numTriangles++;
		}
	}
}

void CDumpGeometryToOBJ::AddQuad(Vec3V_In p0, Vec3V_In p1, Vec3V_In p2, Vec3V_In p3)
{
	//Assert(m_numTriangles == 0); // can't have both in the same material group!
	Assert((m_materialCount == 0 && m_materialIndex == INDEX_NONE) || (m_materialCount > 0 && m_materialIndex == m_materialCount - 1));

	if (m_stream)
	{
		if (m_numTriangles > 0)
		{
			fprintf(m_stream, "v %f %f %f\n", VEC3V_ARGS(p0));
			fprintf(m_stream, "v %f %f %f\n", VEC3V_ARGS(p1));
			fprintf(m_stream, "v %f %f %f\n", VEC3V_ARGS(p2));
			m_numTriangles++;

			fprintf(m_stream, "v %f %f %f\n", VEC3V_ARGS(p0));
			fprintf(m_stream, "v %f %f %f\n", VEC3V_ARGS(p2));
			fprintf(m_stream, "v %f %f %f\n", VEC3V_ARGS(p3));
			m_numTriangles++;
		}
		else
		{
			fprintf(m_stream, "v %f %f %f\n", VEC3V_ARGS(p0));
			fprintf(m_stream, "v %f %f %f\n", VEC3V_ARGS(p1));
			fprintf(m_stream, "v %f %f %f\n", VEC3V_ARGS(p2));
			fprintf(m_stream, "v %f %f %f\n", VEC3V_ARGS(p3));
			m_numQuads++;
		}
	}
}

void CDumpGeometryToOBJ::AddCylinder(Vec3V_In p0, Vec3V_In p1, ScalarV_In radius, int numSides)
{
	Assert(!IsEqualAll(p0, p1));

	Vec3V basisC = Vec3V(MinElementMask_alt(p1 - p0)) & Vec3V(V_ONE);
	Vec3V basisX = Normalize(Cross(p1 - p0, basisC));
	Vec3V basisY = Normalize(Cross(p1 - p0, basisX));

	if (Determinant(Mat33V(basisX, basisY, p1 - p0)).Getf() < 0.0f)
	{
		basisY = -basisY;
	}

	for (int i = 0; i < numSides; i++)
	{
		// TODO -- pass sin/cos tables instead of recalculating here for each cylinder
		const float theta0 = 2.0f*PI*(float)(i + 0)/(float)numSides;
		const float theta1 = 2.0f*PI*(float)(i + 1)/(float)numSides;

		const float c0 = cosf(theta0);
		const float s0 = sinf(theta0);
		const float c1 = cosf(theta1);
		const float s1 = sinf(theta1);

		const Vec3V v0 = AddScaled(p0, basisX*ScalarV(c0) + basisY*ScalarV(s0), radius);
		const Vec3V v1 = AddScaled(p1, basisX*ScalarV(c0) + basisY*ScalarV(s0), radius);
		const Vec3V v2 = AddScaled(p0, basisX*ScalarV(c1) + basisY*ScalarV(s1), radius);
		const Vec3V v3 = AddScaled(p1, basisX*ScalarV(c1) + basisY*ScalarV(s1), radius);

		AddTriangle(v0, v1, v2);
		AddTriangle(v1, v3, v2);
	}
}

void CDumpGeometryToOBJ::AddBox(Vec3V_In bmin, Vec3V_In bmax)
{
	const Vec3V p000 = bmin;
	const Vec3V p100 = GetFromTwo<Vec::X2,Vec::Y1,Vec::Z1>(bmin, bmax);
	const Vec3V p010 = GetFromTwo<Vec::X1,Vec::Y2,Vec::Z1>(bmin, bmax);
	const Vec3V p110 = GetFromTwo<Vec::X2,Vec::Y2,Vec::Z1>(bmin, bmax);
	const Vec3V p001 = GetFromTwo<Vec::X1,Vec::Y1,Vec::Z2>(bmin, bmax);
	const Vec3V p101 = GetFromTwo<Vec::X2,Vec::Y1,Vec::Z2>(bmin, bmax);
	const Vec3V p011 = GetFromTwo<Vec::X1,Vec::Y2,Vec::Z2>(bmin, bmax);
	const Vec3V p111 = bmax;

	AddQuad(p000, p100, p101, p001);
	AddQuad(p001, p101, p111, p011);
	AddQuad(p011, p111, p110, p010);
	AddQuad(p010, p110, p100, p000);
	AddQuad(p000, p001, p011, p010);
	AddQuad(p100, p110, p111, p101);
}

void CDumpGeometryToOBJ::AddBoxEdges(Vec3V_In bmin, Vec3V_In bmax, float radius, int numSides)
{
	const Vec3V p000 = bmin;
	const Vec3V p100 = GetFromTwo<Vec::X2,Vec::Y1,Vec::Z1>(bmin, bmax);
	const Vec3V p010 = GetFromTwo<Vec::X1,Vec::Y2,Vec::Z1>(bmin, bmax);
	const Vec3V p110 = GetFromTwo<Vec::X2,Vec::Y2,Vec::Z1>(bmin, bmax);
	const Vec3V p001 = GetFromTwo<Vec::X1,Vec::Y1,Vec::Z2>(bmin, bmax);
	const Vec3V p101 = GetFromTwo<Vec::X2,Vec::Y1,Vec::Z2>(bmin, bmax);
	const Vec3V p011 = GetFromTwo<Vec::X1,Vec::Y2,Vec::Z2>(bmin, bmax);
	const Vec3V p111 = bmax;

	AddCylinder(p000, p001, ScalarV(radius), numSides);
	AddCylinder(p100, p101, ScalarV(radius), numSides);
	AddCylinder(p010, p011, ScalarV(radius), numSides);
	AddCylinder(p110, p111, ScalarV(radius), numSides);

	AddCylinder(p000, p100, ScalarV(radius), numSides);
	AddCylinder(p010, p110, ScalarV(radius), numSides);
	AddCylinder(p001, p101, ScalarV(radius), numSides);
	AddCylinder(p011, p111, ScalarV(radius), numSides);

	AddCylinder(p000, p010, ScalarV(radius), numSides);
	AddCylinder(p001, p011, ScalarV(radius), numSides);
	AddCylinder(p100, p110, ScalarV(radius), numSides);
	AddCylinder(p101, p111, ScalarV(radius), numSides);
}

void CDumpGeometryToOBJ::AddText(const char* format, ...)
{
	if (m_stream)
	{
		char temp[4096] = "";
		va_list args;
		va_start(args, format);
		vsnprintf(temp, sizeof(temp), format, args);
		va_end(args);

		fprintf(m_stream, "%s\n", temp);
	}
}

void CDumpGeometryToOBJ::Close()
{
	if (m_numTriangles > 0 || m_numQuads > 0)
	{
		MaterialEnd();
	}

	if (m_stream)
	{
		m_stream->Close();
		m_stream = NULL;
	}

	if (m_materialPath)
	{
		delete[] m_materialPath;
		m_materialPath = NULL;
	}
}

#endif // __BANK
