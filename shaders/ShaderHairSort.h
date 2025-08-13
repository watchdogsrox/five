//
// Filename:	ShaderHairSort.h
// Description:	special skinned hair renderer able to sort geometries in a drawable using the hair shader
// Written by:	Andrzej
//
//	17/01/2006	-	John:		- inititial;
//	04/09/2007	-	Andrzej:	- code update & revival;
//
//
//
#ifndef __SHADER_HAIR_SORT_H__
#define __SHADER_HAIR_SORT_H__

// forward declarations
namespace rage {
	class rmcDrawable;
	class Matrix34;
	class grmMatrixSet;
	class rmcLodGroup;
	class grmShader;
	class grmShaderGroup;
	class atBitSet;
	class grmModel;
	class bkBank;
	class Vector3;
	class Vec3V;
}

//
//
//
//
class CShaderHairSort
{
public:
	CShaderHairSort();
	~CShaderHairSort();

	static void	Init(unsigned initMode);
#if __BANK
	static void InitWidgets(bkBank& bank);
#endif
public:

	enum eHairSortMode
	{
		HAIRSORT_ORDERED	= 0,	// standard ordered hair (ped_hair_sorted_alpha_exp)
		HAIRSORT_FUR		= 1,	// fur (ped_fur)
		HAIRSORT_SPIKED		= 2,	// cutouts with external normal layer (ped_hair_spiked)
		HAIRSORT_LONGHAIR	= 3,
		HAIRSORT_CUTOUT		= 4		// ped_hair_cutout_alpha/_cloth
	};
	static void DrawableDrawSkinned(const rmcDrawable *pDrawable, const Matrix34 &rootMatrix,const grmMatrixSet &ms,int bucketMask,int lod,eHairSortMode mode=HAIRSORT_ORDERED,Vector3 *windVector=NULL);

	static void PushHairTintTechnique();
	static void PopHairTintTechnique();

private:
	enum
	{
		PASS_ALL = -1
	};

	static void LodGroupDrawMulti(const rmcLodGroup& lodGroup, const grmShaderGroup &group,const Matrix34 &rootMatrix,const grmMatrixSet &ms,int bucketMask,int lodIdx,const atBitSet *enables, eHairSortMode mode, Vector3 *windVector=NULL);
	static void ModelDrawSkinned(const grmModel &model, const grmShaderGroup &group,const grmMatrixSet &ms,int bucketMask,int lod, eHairSortMode mode,  float fFurLOD, Vector3 *windVector);
	static void ShaderDrawSkinned(const grmShader &shader, const grmModel &model,int geom,const grmMatrixSet &ms,int /*lod*/,bool restoreState, int forcedPass, int hOrder, eHairSortMode hairMode, bool bIsShadowPass, bool bIsForwardPass);
	static void ShaderDrawSkinned_LongHair(const grmShader &shader, const grmModel &model,int geom,const grmMatrixSet &ms,int /*lod*/,bool restoreState, int forcedPass, eHairSortMode mode);

#if (__BANK)
	// RAG widget controls:
	static bool			furOverride;
	static float		furLayerShadowMin;
	static u32			furMinLayers;
	static u32			furMaxLayers;
	static float		furLength;
	static float		furStiffness;
	static float		furLodDist;	
	static float		furDefaultFOVY;
	static float		furGravityToWindRatio;
	static Vec3V		furAttenuationCoeff;
	static float		furNoiseUVScale;
	static float		furAOBlend;
#endif
};

#endif //__SHADER_HAIR_SORT_H__...

