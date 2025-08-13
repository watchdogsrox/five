/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    scene/debug/ObjExporter.h
// PURPOSE : simple utility to save geometry to an .obj file
// AUTHOR :  Flavius Alecu
// CREATED : 07/01/11
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef _SCENE_DEBUG_OBJEXPORTER_H_
#define _SCENE_DEBUG_OBJEXPORTER_H_

#if __BANK

#include <stdio.h>

#include "grcore/indexbuffer.h"
#include "grcore/vertexbuffer.h"
#include "vectormath/mat34v.h"

#include "system/FileMgr.h"


class CObjExporter
{
public:
	static inline FileHandle	OpenFileForWriting(const char* filename);
	static inline void			CloseFile(FileHandle file);

	static inline void 			AddGroup(FileHandle file, const char* groupName);
	static inline void 			AddComment(FileHandle file, const char* comment);

	static inline void 			AddVertices(FileHandle file, grcVertexBuffer* vb, bool addUVs);
	static inline void			AddVertices(FileHandle file, Vector4* verts, u32 numVerts);
	static inline void 			AddVerticesTransformed(FileHandle file, grcVertexBuffer* vb, const Matrix34* matrix, bool addUVs);
	static inline void 			AddVerticesTransformed(FileHandle file, Vector4* verts, u32 numVerts, const Matrix34* matrix);

	static void					AddUVs(FileHandle file, Vector4* uvs, u32 numUvs);

	static void 				AddTriangles(FileHandle file, grcIndexBuffer* ib, u32 indexOffset, bool hasUVs);
	static void 				AddTriangles(FileHandle file, u16* indices, u32 numIndices, u32 indexOffset, bool hasUVs);

	static void					AddNewLine(FileHandle file);
private:
	static void					WriteVertexBuffer(FileHandle file, grcVertexBuffer* vb, const Matrix34* matrix, bool addUVs);
	static void					WriteVertexVectors(FileHandle file, Vector4* verts, u32 numVerts, const Matrix34* matrix);
};

inline FileHandle CObjExporter::OpenFileForWriting(const char* filename)
{
	return CFileMgr::OpenFileForWriting(filename);
}

inline void CObjExporter::CloseFile(FileHandle file)
{
	CFileMgr::CloseFile(file);
}

inline void CObjExporter::AddGroup(FileHandle file, const char* groupName)
{
	char str[128];
	sprintf(str, "g %s\n", groupName );
	CFileMgr::Write(file, str, istrlen(str));
}

inline void CObjExporter::AddComment(FileHandle file, const char* comment)
{
	char str[256];
	sprintf(str, "# %s\n", comment );
	CFileMgr::Write(file, str, istrlen(str));
}

inline void CObjExporter::AddVertices(FileHandle file, grcVertexBuffer* vb, bool addUVs)
{
	WriteVertexBuffer(file, vb, NULL, addUVs);
}

inline void CObjExporter::AddVertices(FileHandle file, Vector4* verts, u32 numVerts)
{
	WriteVertexVectors(file, verts, numVerts, NULL);
}

inline void CObjExporter::AddVerticesTransformed(FileHandle file, grcVertexBuffer* vb, const Matrix34* matrix, bool addUVs)
{
	WriteVertexBuffer(file, vb, matrix, addUVs);
}

inline void CObjExporter::AddVerticesTransformed(FileHandle file, Vector4* verts, u32 numVerts, const Matrix34* matrix)
{
	WriteVertexVectors(file, verts, numVerts, matrix);
}
#endif //__BANK

#endif //_SCENE_DEBUG_OBJEXPORTER_H_