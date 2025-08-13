
#ifndef MOVIE_MESH_INFO_H
#define MOVIE_MESH_INFO_H

// rage headers
#include "parser/macros.h"
#include "vector/Vector3.h"
#include "vector/Vector4.h"
#include "atl/array.h"
#include "string/string.h"

struct MovieMeshInfo {
	MovieMeshInfo	();
	void		Init();
	ConstString m_ModelName;
	ConstString m_TextureName;
	Vector3		m_TransformA;
	Vector3		m_TransformB;
	Vector3		m_TransformC;
	Vector3		m_TransformD;

	PAR_SIMPLE_PARSABLE;
};

struct MovieMeshInfoList {
	atArray<MovieMeshInfo> m_Data;
	atArray<Vector4> m_BoundingSpheres;
	ConstString	m_InteriorName;
	Vector3		m_InteriorPosition;
	Vector3		m_InteriorMaxPosition;
	bool		m_UseInterior;
	PAR_SIMPLE_PARSABLE;
};


#endif // MOVIE_MESH_INFO_H