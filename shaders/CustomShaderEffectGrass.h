//
// Filename:	CustomShaderEffectGrass.h
// Description:	Class for controlling grass shader variables
//
//

#ifndef __CCUSTOMSHADEREFFECTGRASS_H__
#define __CCUSTOMSHADEREFFECTGRASS_H__

#include "rmcore\drawable.h"
#include "shaders\CustomShaderEffectBase.h"
#include "shaders\CustomShaderEffectTint.h"
#include "renderer\PlantsGrassRendererSwitches.h"
#include "scene\EntityBatch_Def.h"


#if !HACK_RDR3
#define ENABLE_TRAIL_MAP 0
#endif

//
//
//
//
class CCustomShaderEffectGrassType : public CCustomShaderEffectBaseType
{
	friend class CCustomShaderEffectGrass;

public:
	enum { NUM_COL_VEH = 4	};	// support up to 4 vehicles deforming bushes

public:
	CCustomShaderEffectGrassType() : CCustomShaderEffectBaseType(CSE_GRASS) {}
	virtual bool							Initialise(rmcDrawable *pDrawable);
	virtual CCustomShaderEffectBase*		CreateInstance(CEntity *pEntity);

	inline const EBStatic::GrassCSVars &	GetCSVars() const									{ return m_CSVars;				}

protected:
	virtual ~CCustomShaderEffectGrassType();

	inline float							GetMaxVehicleCollisionDist() const					{ return m_fMaxVehicleCollisionDist;	}
	inline bool								IsVehicleDeformable() const							{ return (true);						}

private:
	EBStatic::GrassCSVars					m_CSVars;
	u8										m_idOrientToTerrain;
	u8										m_idPlayerPos;
	u8										m_idLodFadeInstRange;

	u8										m_idWindBending;
	u8										m_idCollParams;
	u8										m_idFadeAlphaDistUmTimer;
	u8										m_idFakedGrassNormal;

	u8										m_idDebugSwitches;
#if DEVICE_MSAA
	u8										m_idAlphaToCoverageScale;
#endif

#if ENABLE_TRAIL_MAP
	u8										m_idTrailMapTexture;
	u8										m_idMapSizeWorldSpace;
#endif

	u8										m_idVarVehCollEnabled[NUM_COL_VEH];
	u8										m_idVarVehCollB0[NUM_COL_VEH];
	u8										m_idVarVehCollM0[NUM_COL_VEH];
	u8										m_idVarVehCollR0[NUM_COL_VEH];

	float									m_fMaxVehicleCollisionDist;
};


//
//
//
//
class CCustomShaderEffectGrass : public CCustomShaderEffectBase
{
	friend class CCustomShaderEffectGrassType;

public:
	CCustomShaderEffectGrass();
	~CCustomShaderEffectGrass();

public:
	virtual void						SetShaderVariables(rmcDrawable* pDrawable);
	virtual void						AddToDrawList(u32 modelIndex, bool bExecute);
	virtual void						AddToDrawListAfterDraw();

	virtual void						Update(fwEntity *pEntity);

	// Instancing support:
	virtual bool						HasPerBatchShaderVars() const			{ return true; }
	virtual void						AddBatchRenderStateToDrawList() const;
	virtual void						AddBatchRenderStateToDrawListAfterDraw() const;

public:
	Vec3V_Out							GetLodFadeInstRange() const				{ return m_LodFadeInstRange; }

#if __BANK
	static void							RenderDebug();
	static void							AddWidgets(bkBank &bank);

	// Note: This is only meant to be used to update values changed in Rag.
	void								SetScaleRange(Vec3V_In val)				{ m_scaleRange = val; m_scaleRange.SetWZero(); }
	void								SetOrientToTerrain(float val)			{ m_orientToTerrain = val; }
	static void							SetDbgAlphaOverdraw(bool enable);
#endif

	//Interface to allow script to use vehicle-style flattening effect for spawn points.
	static void							ClearScriptVehicleFlatten();
	static void							SetScriptVehicleFlatten(const spdAABB &aabb, Vec3V_In look, ScalarV_In groundZ = ScalarV(V_FLT_MAX));
	static void							SetScriptVehicleFlatten(const spdSphere &sphere, Vec3V_In look, ScalarV_In groundZ = ScalarV(V_FLT_MAX));
	static bool							IntersectsScriptFlattenAABB(fwEntity *pEntity);

private:
	#define VEHCOL_DIST_LIMIT			(2.0f)	// skip vehicle deformation if plant is too far away from the vehicle/ground
	void								SetGlobalVehCollisionParams(u32 n, bool bEnable, const Vector3& vecB, const Vector3& vecM, float radius, float groundZ, float vehColDistLimit);

private:
	CCustomShaderEffectGrassType*		m_pType;

	Vec3V								m_AabbMin;
	Vec3V								m_AabbDelta;
	Vec3V								m_scaleRange;
	Vec3V								m_LodFadeInstRange;

	float								m_umPhaseShift;
	float								m_orientToTerrain;
	float								m_collPlayerScaleSqr;

	bool								m_bVehCollisionEnabled	[CCustomShaderEffectGrassType::NUM_COL_VEH];
	Vector4								m_vecVehCollisionB		[CCustomShaderEffectGrassType::NUM_COL_VEH];
	Vector4								m_vecVehCollisionM		[CCustomShaderEffectGrassType::NUM_COL_VEH];
	Vector4								m_vecVehCollisionR		[CCustomShaderEffectGrassType::NUM_COL_VEH];
};

#endif //__CCUSTOMSHADEREFFECTGRASS_H__...

