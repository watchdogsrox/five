// ======================================
// debug/textureviewer/textureviewerDTQ.h
// (c) 2010 RockstarNorth
// ======================================

#ifndef _DEBUG_TEXTUREVIEWER_TEXTUREVIEWERDTQ_H_
#define _DEBUG_TEXTUREVIEWER_TEXTUREVIEWERDTQ_H_

#include "debug/textureviewer/textureviewerprivate.h"

#if __BANK

namespace rage { class Color32; }
namespace rage { class Vector3; }
namespace rage { class grcTexture; }
namespace rage { class fwBox2D; }

// ================================================================================================

namespace DebugTextureViewerDTQ { // DTQ stands for "Draw Textured Quad"

enum eDTQPixelCoordSpace
{
	eDTQPCS_Normalised, // [-1..1]
	eDTQPCS_NormalisedZeroToOne, // [0..1]
	eDTQPCS_Pixels,
};

enum eDTQTexelCoordSpace
{
	eDTQTCS_Normalised, // [0..1]
	eDTQTCS_Texels,
};

enum eDTQViewMode
{
	eDTQVM_RGB         ,
	eDTQVM_RGB_plus_A  ,
	eDTQVM_R_G_B_A     ,
	eDTQVM_RGBA        ,
	eDTQVM_R           ,
	eDTQVM_G           ,
	eDTQVM_B           ,
	eDTQVM_R_monochrome,
	eDTQVM_G_monochrome,
	eDTQVM_B_monochrome,
	eDTQVM_A_monochrome,
	eDTQVM_RGB_invert_A,
	eDTQVM_specular    ,
	eDTQVM_COUNT,
	eDTQVM_FORCE32 = 0x7fffffff
};

const char** GetUIStrings_eDTQViewMode();

// NOTE: this enum must be kept in sync between
// %RS_CODEBRANCH%\game\debug\TextureViewer\TextureViewerDTQ.h, and
// %RS_CODEBRANCH%\game\shader_source\Debug\debug_DTQ.fx
enum eDTQTextureType
{
	eDTQTT_2D,
	eDTQTT_3D,
	eDTQTT_Cube,
	eDTQTT_2DArray,
	eDTQTT_3DArray, // not used
	eDTQTT_CubeArray,
};

// NOTE: this enum must be kept in sync between
// %RS_CODEBRANCH%\game\debug\TextureViewer\TextureViewerDTQ.h, and
// %RS_CODEBRANCH%\game\shader_source\Debug\debug_DTQ.fx
enum eDTQCubeProjection
{
	eDTQCP_Faces,
	eDTQCP_4x3Cross,
	eDTQCP_Panorama,
	eDTQCP_Cylinder,
	eDTQCP_MirrorBall,
	eDTQCP_Spherical,
	eDTQCP_SphericalSquare,
	eDTQCP_Paraboloid,
	eDTQCP_ParaboloidSquare,
	eDTQCP_Count
};

enum eDTQCubeFace // 3dsmax order
{
	eDTQCF_PZ_UP,
	eDTQCF_NZ_DOWN,
	eDTQCF_NX_LEFT,
	eDTQCF_PX_RIGHT,
	eDTQCF_PY_FRONT,
	eDTQCF_NY_BACK,
	eDTQCF_Count
};

void DTQInit();

void DTQBlitSolidColour(
	eDTQPixelCoordSpace pixelCoordSpace,
	float               x1,
	float               y1,
	float               x2,
	float               y2,
	float               z,
	const Color32&      colour,
	bool                bSolid
	);

void DTQBlit(
	eDTQViewMode        viewMode          ,
	bool                bFiltered         ,
	float               LOD               ,
	int                 layer             ,
	float               volumeSlice       ,
	int                 volumeCoordSwap   ,
	bool                cubeForceOpaque   ,
	bool                cubeSampleAllMips ,
	float               cubeCylinderAspect,
	float               cubeFaceZoom      ,
	eDTQCubeFace        cubeFace          ,
	Vec3V_In            cubeRotationYPR   ,
	eDTQCubeProjection  cubeProjection    ,
	bool                cubeProjectionClip,
	bool                cubeNegate        ,
	float               cubeRefDots       ,
	const Vector3&      specularL         ,
	float               specularBumpiness ,
	float               specularExponent  ,
	float               specularScale     ,
	eDTQPixelCoordSpace pixelCoordSpace   ,
	const fwBox2D&      dstBounds         ,
	float               dstZ              ,
	eDTQTexelCoordSpace texelCoordSpace   ,
	const fwBox2D&      srcBounds         ,
	const Color32&      colour            ,
	float               brightness        ,
	const grcTexture*   texture
	);

} // namespace DebugTextureViewerDTQ

#endif // __BANK
#endif // _DEBUG_TEXTUREVIEWER_TEXTUREVIEWERDTQ_H_
