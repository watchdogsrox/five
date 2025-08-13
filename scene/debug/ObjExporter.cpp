/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    scene/debug/ObjExporter.h
// PURPOSE : simple utility to save geometry to an .obj file
// AUTHOR :  Flavius Alecu
// CREATED : 07/01/11
//
/////////////////////////////////////////////////////////////////////////////////

#include "scene/debug/ObjExporter.h"
#include "math/float16.h"

#if __BANK

SCENE_OPTIMISATIONS();

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	AddUVs
// PURPOSE:		prints out the uvs specified
//////////////////////////////////////////////////////////////////////////
void CObjExporter::AddUVs(FileHandle file, Vector4* uvs, u32 numUvs)
{
	Assert(file);

	char str[512] = { 0 };
	for (u32 i = 0; i < numUvs; ++i)
	{
        if (uvs)
            sprintf(str, "vt %f %f\n", uvs[i].x, uvs[i].y);
        else
            sprintf(str, "vt %f %f\n", 0.f, 0.f); // edgegeomextract somatimes returns null pointers for uvs
		CFileMgr::Write(file, str, istrlen(str));
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	AddTriangles
// PURPOSE:		iterates through the index buffer and prints out a set of faces to the obj file.
//				it is assumed that the indices represent a triangle list
//////////////////////////////////////////////////////////////////////////
void CObjExporter::AddTriangles(FileHandle file, grcIndexBuffer* ib, u32 indexOffset, bool hasUVs)
{
	Assert(file && ib);
	const u16* indices = ib->LockRO();

	char str[512] = { 0 };
	for (u32 i = 0; i < ib->GetIndexCount(); i += 3)
	{
		const u32 i0 = indices[i];
		const u32 i1 = indices[i + 1];
		const u32 i2 = indices[i + 2];

		if (hasUVs)
			sprintf(str, "f %d/%d %d/%d %d/%d\n", i0 + indexOffset, i0 + indexOffset, i1 + indexOffset, i1 + indexOffset, i2 + indexOffset, i2 + indexOffset);
		else
			sprintf(str, "f %d %d %d\n", i0 + indexOffset, i1 + indexOffset, i2 + indexOffset);
		CFileMgr::Write(file, str, istrlen(str));
	}

	AddNewLine(file);
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	AddTriangles
// PURPOSE:		iterates through the index list and prints out a set of faces to the obj file.
//				it is assumed that the indices represent a triangle list
//////////////////////////////////////////////////////////////////////////
void CObjExporter::AddTriangles(FileHandle file, u16* indices, u32 numIndices, u32 indexOffset, bool hasUVs)
{
	Assert(file && indices);

	char str[512] = { 0 };
	for (u32 i = 0; i < numIndices; i += 3)
	{
		const u32 i0 = indices[i];
		const u32 i1 = indices[i + 1];
		const u32 i2 = indices[i + 2];

		if (hasUVs)
			sprintf(str, "f %d/%d %d/%d %d/%d\n", i0 + indexOffset, i0 + indexOffset, i1 + indexOffset, i1 + indexOffset, i2 + indexOffset, i2 + indexOffset);
		else
			sprintf(str, "f %d %d %d\n", i0 + indexOffset, i1 + indexOffset, i2 + indexOffset);
		CFileMgr::Write(file, str, istrlen(str));
	}

	AddNewLine(file);
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	AddNewLine
// PURPOSE:		writes a newline to the obj file
//////////////////////////////////////////////////////////////////////////
void CObjExporter::AddNewLine(FileHandle file)
{
	const char* newLine = "\n";
	CFileMgr::Write(file, newLine, istrlen(newLine));
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	WriteVertexBuffer
// PURPOSE:		iterates through the vertex buffer and prints all the vertices to the obj file.
//				if a matrix is specified, the verts are transformed before written to the file.
//////////////////////////////////////////////////////////////////////////
void CObjExporter::WriteVertexBuffer(FileHandle file, grcVertexBuffer* vb, const Matrix34* matrix, bool addUVs)
{
	Assert(file && vb);

	const grcFvf* fvf = vb->GetFvf();
	Assert(fvf);

	grcVertexBuffer::LockHelper helper(vb, true);
	u8* data = reinterpret_cast<u8*>(helper.GetLockPtr());
	u8* vertData = data;

	Vector3 pos;
	char str[512] = { 0 };
	for (u32 i = 0; i < vb->GetVertexCount(); ++i)
	{
		if (fvf->GetDataSizeType(grcFvf::grcfcPosition) == grcFvf::grcdsHalf4 || fvf->GetDataSizeType(grcFvf::grcfcPosition) == grcFvf::grcdsHalf3)
		{
			Float16* f = (Float16*)vertData;
			pos.Set(f[0].GetFloat32_FromFloat16(), f[1].GetFloat32_FromFloat16(), f[2].GetFloat32_FromFloat16());
		}
		else if (fvf->GetDataSizeType(grcFvf::grcfcPosition) == grcFvf::grcdsFloat4 || fvf->GetDataSizeType(grcFvf::grcfcPosition) == grcFvf::grcdsFloat3)
		{
			float *f = (float*)vertData;
			pos.Set(f[0], f[1], f[2]);
		}
		else
		{
			Assert(false);
		}

		if (matrix)
		{
			matrix->Transform(pos);
		}

		sprintf(str, "v %f %f %f\n", pos.GetX(), pos.GetY(), pos.GetZ());
		CFileMgr::Write(file, str, istrlen(str));

		vertData += vb->GetVertexStride();
	}
	AddNewLine(file);

	if (addUVs && fvf->GetTextureChannel(0))
	{
		u8* uvData = data + fvf->GetOffset(static_cast<grcFvf::grcFvfChannels>(grcFvf::grcfcTexture0));

		Vector2 uv;
		for (u32 i = 0; i < vb->GetVertexCount(); ++i)
		{
			if (fvf->GetDataSizeType(grcFvf::grcfcTexture0) == grcFvf::grcdsHalf2 || fvf->GetDataSizeType(grcFvf::grcfcTexture0) == grcFvf::grcdsHalf4) 
			{
				Float16* f = (Float16*)uvData;
				uv.Set(f[0].GetFloat32_FromFloat16(), f[1].GetFloat32_FromFloat16());
			}
			else if (fvf->GetDataSizeType(grcFvf::grcfcTexture0) == grcFvf::grcdsFloat2 || fvf->GetDataSizeType(grcFvf::grcfcTexture0) == grcFvf::grcdsFloat4) 
			{
				float *f = (float*)uvData;
				uv.Set(f[0], f[1]);
			}
			else
			{
				Assert(false);
			}

			sprintf(str, "vt %f %f\n", uv.x, 1.f - uv.y);
			CFileMgr::Write(file, str, istrlen(str));

			uvData += vb->GetVertexStride();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	WriteVertexVectors
// PURPOSE:		iterates through a list of Vector3s and prints all the vertices to the obj file.
//				if a matrix is specified, the verts are transformed before written to the file.
//////////////////////////////////////////////////////////////////////////
void CObjExporter::WriteVertexVectors(FileHandle file, Vector4* verts, u32 numVerts, const Matrix34* matrix)
{
	Assert(file && verts);
	Assert(numVerts > 0);

	Vector3 pos;
	char str[512] = { 0 };
	for (u32 i = 0; i < numVerts; ++i)
	{
		pos = verts[i].GetVector3();

		if (matrix)
		{
			matrix->Transform(pos);
		}

		sprintf(str, "v %f %f %f\n", pos.GetX(), pos.GetY(), pos.GetZ());
		CFileMgr::Write(file, str, istrlen(str));
	}
}

#endif //__BANK
