//
// FILE :    DrawableDataStructs.h
// PURPOSE : structs to establish data required to draw entities
// AUTHOR :  john.
// CREATED : 20/05/2009
//
/////////////////////////////////////////////////////////////////////////////////



#ifndef INC_DRAWBLDATASTRCTS_H_
#define INC_DRAWBLDATASTRCTS_H_

// rage headers
#include "fwtl/pool.h"
#include "vector/quaternion.h"
#include "breakableglass/breakable.h"
#include "fwrenderer/instancing.h"
#include "grmodel/model.h"

//game headers
#include "Vehicles/VehicleDefines.h"
#include "peds/rendering/pedvariationds.h"
#include "fragment/cache.h"
#include "modelinfo/VehicleModelInfoVariation.h"
#include "breakableglass/breakable.h"
#include "scene/EntityBatch_Def.h"
#include "scene/EntityId.h"


#if __BANK && __PPU
#define START_SPU_STAT_RECORD(p) \
	if (p) \
	{ \
		SPU_SIMPLE_COMMAND(startStatRecord,0); \
	}

#define STOP_SPU_STAT_RECORD(p) \
	if (p) \
	{ \
		SPU_SIMPLE_COMMAND(stopStatRecord,0); \
	}
#else
#define START_SPU_STAT_RECORD(p)
#define STOP_SPU_STAT_RECORD(p)
#endif


class CPedStreamRenderGfx;
class CPedPropRenderGfx;
class CCustomShaderEffectPedProp;
class CEntity;
class CGrassBatch;
class CAddCompositeSkeletonCommand;
class CVehicleStreamRenderGfx;
namespace rage {
	class rmcDrawable;
}

#if __BANK
class CEntityDrawDataCommon
{
public:
	CEntityDrawDataCommon()
		: m_entityId((entitySelectID)ENTITYSELECT_INVALID_ID)
#if __PS3
		, m_contextStats(0)
#endif // __PS3
		, m_captureStats(0)
	{}

	//Initialize necessary member variables from the entity pointer.
	void InitFromEntity(fwEntity* pEntity);

	//This should be called before the entity is being drawn. This will set any necessary shader params for the pending draw.
	bool SetShaderParams(int renderBucket);
	static bool SetShaderParamsStatic(entitySelectID entityId, int renderBucket);

	entitySelectID GetEntityID() const	{ return m_entityId; }
	u8 GetContextStats() const;
	u8 GetCaptureStats() const { return m_captureStats; }

private:
	entitySelectID m_entityId;
#if __PS3
	u8	m_contextStats : 4;
#endif // __PS3
	u8	m_captureStats : 1;
};
#endif // __BANK

// basic entity draw data required for rendering 
class CEntityDrawData {
	friend class CDrawEntityDC;
public:
	CEntityDrawData() {}
	~CEntityDrawData() {}

	void Init(CEntity* pEntity);
#if RAGE_INSTANCED_TECH
	void SetViewportInstancedRenderBit(u8 uBit)	{ m_viewportInstancedRenderBit = uBit; }
#endif
	void Draw();

private:
	__vector4	m_vector0; // these two vectors together are cast to fwTransform
	__vector4	m_vector1;

	rmcDrawable* m_pDrawable;

	u32			m_modelInfoIdx : 16;
	u32 		m_fadeAlpha	: 8;
	u32			m_naturalAmb : 8;

	u32			m_artificialAmb	: 8;
	u32			m_matID	: 8;
	u32			m_phaseLODs : 15;
	u32			m_forceAlphaForWaterReflection : 1;

	u32			m_allowAlphaBlend : 1;
	u32			m_fadeDist : 1;
	u32			m_lastLODIdx : 2;

	u32			m_interior : 1;       

	ATTR_UNUSED u32			m_pad : 3;
#if RAGE_INSTANCED_TECH
	u32			m_viewportInstancedRenderBit : 8;
#endif

#if __BANK
	CEntityDrawDataCommon m_commonData;
#endif // __BANK
};

// basic entity draw data which requires a full matrix for rendering
class CEntityDrawDataFm {
	friend class CDrawEntityFmDC;
public:
	void Init(CEntity* pEntity);
#if RAGE_INSTANCED_TECH
	void SetViewportInstancedRenderBit(u8 uBit)	{ m_pack0.m_viewportInstancedRenderBit = uBit; }
#endif
	void Draw();

	struct ALIGNAS(16) PackedSet0
	{
		float m_matrixCol0_xyz[3]; // DO NOT STORE DATA HERE, THIS IS ALIASED TO THE MATRIX DATA
#if RAGE_INSTANCED_TECH
		u32 m_viewportInstancedRenderBit;
#else // RAGE_INSTANCED_TECH
		u32 m_pad;
#endif // RAGE_INSTANCED_TECH

	} ;

	struct ALIGNAS(16) PackedSet1
	{
		float m_matrixCol1_xyz[3]; // DO NOT STORE DATA HERE, THIS IS ALIASED TO THE MATRIX DATA

		u32	m_modelInfoIdx : 16;
		u32 m_fadeAlpha	: 8;
		u32	m_naturalAmb : 8;
	} ;

	struct ALIGNAS(16) PackedSet2
	{
		float m_matrixCol2_xyz[3]; // DO NOT STORE DATA HERE, THIS IS ALIASED TO THE MATRIX DATA

		u32	m_artificialAmb	: 8;
		u32	m_matID	: 8;
		u32 m_forceAlphaForWaterReflection : 1;
		u32 m_allowAlphaBlend : 1;
#if __BANK
#if __PS3
		u32	m_contextStats : 4;
#endif // __PS3
		u32	m_captureStats : 1;
		u32 m_pad1 : 9;
#else
		u32	m_pad1 : 14;
#endif // __BANK
	} ;

	struct ALIGNAS(16) PackedSet3
	{
		float m_matrixCol3_xyz[3]; // DO NOT STORE DATA HERE, THIS IS ALIASED TO THE MATRIX DATA

		u32	m_phaseLODs : 15;
		u32 m_lastLODIdx : 2;
		u32 m_fadeDist : 1;
		u32	m_interior : 1; 
		u32	m_closeToCam : 1; 
		u32	m_fpv : 1;
		u32 m_forceAlpha : 1;
		u32 m_pad : 10;
	} ;

private:

	BANK_ONLY(inline bool SetShaderParams(int renderBucket)	{ return CEntityDrawDataCommon::SetShaderParamsStatic(m_entityId, renderBucket); })

	ATTR_UNUSED PackedSet0 m_pack0;		// matrix data is aliased here, so it really is used.
	PackedSet1 m_pack1;
	PackedSet2 m_pack2;
	PackedSet3 m_pack3;

#if __BANK
	entitySelectID m_entityId;
#endif //__BANK

};
CompileTimeAssert(sizeof(CEntityDrawDataFm::PackedSet0) == sizeof(__vector4));
CompileTimeAssert(sizeof(CEntityDrawDataFm::PackedSet1) == sizeof(__vector4));
CompileTimeAssert(sizeof(CEntityDrawDataFm::PackedSet2) == sizeof(__vector4));
CompileTimeAssert(sizeof(CEntityDrawDataFm::PackedSet3) == sizeof(__vector4));

//Instanced entity draw data required for rendering 
CompileTimeAssert(sizeof(DrawListAddress) == sizeof(u32));
class CEntityInstancedDrawData {
	friend class CDrawEntityInstancedDC;
public:
	void Init(CEntity* pEntity, grcInstanceBuffer *ib, int lod);
	void Draw();

private:
	union IB_Address
	{
		grcInstanceBuffer *InstanceBufferPtr;
		u32	InstanceBufferOffset; //DrawListAddress	type
	};

	IB_Address		m_IBAddr;
	u32				m_modelInfoIdx : 16;

	//Per-Instance data is pre-stored in the instance buffer. But we must store the per-model data.
	u32				m_matID	: 8;

	u32				m_LODIndex : 7;		//Could really be less. LOD_COUNT is 4.
	bool			m_HasDLAddress : 1;
#if RAGE_INSTANCED_TECH
	u32				m_viewportInstancedRenderBit : 8;
#endif

	//Not sure how is this gonna work for instanced geometry yet.
#if __BANK
	CEntityDrawDataCommon m_commonData;
#endif
};

//Instanced entity draw data required for rendering 
class CGrassBatchDrawData {
	friend class CDrawGrassBatchDC;
public:
	void Init(CGrassBatch* pEntity);
	void Draw();

	void DispatchComputeShader();
	WIN32PC_ONLY(void CopyStructureCount());

	u32 GetStartInstance() const	{ return m_startInst; }

private:
	//Instance data is pre-stored in CS Params|vertex buffer
	GRASS_BATCH_CS_CULLING_SWITCH(EBStatic::GrassCSParams m_CSParams, const grcVertexBuffer *m_vb);
	u32				m_startInst;
	u32				m_modelInfoIdx : 16;
	u32				m_matID	: 8;
#if !GRASS_BATCH_CS_CULLING
	u32				m_LODIndex : 8;
#endif //GRASS_BATCH_CS_CULLING

	u32 			m_fadeAlpha	: 8;
	u32				m_naturalAmb : 8;
	u32				m_artificialAmb	: 8;
	u32				m_interior : 1; 

#if __BANK
	CEntityDrawDataCommon m_commonData;
#endif
};

// EJ: Memory Optimization
// Create a ped props class that is designed to be opt-in. We shouldn't waste memory or draw command
// space if a ped doesn't really have props. Non-template base class avoids template bloat.
class CEntityDrawDataPedPropsBase
{
private:
	CPedPropRenderGfx* m_pPropGfxData;
	CPedPropData m_propData;

protected:
	void Init(CEntity* pEntity, Matrix34* pMatrix, u32 boneCount, u32 matrixSetCount, s32* propDataIndices);

public:
	CEntityDrawDataPedPropsBase() : m_pPropGfxData(NULL) {}
	virtual ~CEntityDrawDataPedPropsBase() {}

	CPedPropRenderGfx* GetGfxData() const {return m_pPropGfxData;}
	CPedPropData& GetPropData() {return m_propData;}

	virtual s32* GetPropDataIndices() = 0;
	virtual Matrix34* GetMatrices(size_t matrixSetIdx) = 0;
	virtual u32 GetMatrixCount() = 0;
	virtual u32 GetNumMatrixSets() = 0;
};

// EJ: Memory Optimization
// Templatized class for 1, 2, or 3 Matrix34 objects
// Also templatized for two matrix sets (For first and third person)
template <u32 SIZE, u32 NUM_MATRIX_SETS>
class CEntityDrawDataPedProps : public CEntityDrawDataPedPropsBase
{
	CompileTimeAssert(SIZE >= 0 && SIZE <= MAX_PROPS_PER_PED);

private:
	Matrix34 m_propMtxs[SIZE * NUM_MATRIX_SETS];
	s32 m_propDataIndices[SIZE];

public:
	void Init(CEntity* pEntity) {CEntityDrawDataPedPropsBase::Init(pEntity, m_propMtxs, SIZE, NUM_MATRIX_SETS, m_propDataIndices);}
	
	virtual s32* GetPropDataIndices() {return m_propDataIndices;}
	virtual Matrix34* GetMatrices(size_t matrixSetIdx) {Assert(matrixSetIdx < NUM_MATRIX_SETS); return m_propMtxs + matrixSetIdx*SIZE;}
	virtual u32 GetMatrixCount() {return SIZE;}
	virtual u32 GetNumMatrixSets() {return NUM_MATRIX_SETS;}
};

class CEntityDrawDataStreamPed
{
	friend class CDrawStreamPedDC;

	template <u32 SIZE, u32 NUM_MATRIX_SETS>
	friend class CEntityDrawDataStreamPedWithProps;

public:
	CEntityDrawDataStreamPed() {}
	~CEntityDrawDataStreamPed() {}

	void Init(CEntity* pEntity);
#if RAGE_INSTANCED_TECH
	void SetViewportInstancedRenderBit(u8 uBit)	{ m_viewportInstancedRenderBit = uBit; }
#endif
	void Draw(CAddCompositeSkeletonCommand& skeletonCmd, u8 lodIdx, u32 perComponentWetness) {Draw(skeletonCmd, NULL, lodIdx, perComponentWetness);}

private:
	void Draw(CAddCompositeSkeletonCommand& skeletonCmd, CEntityDrawDataPedPropsBase* pProps, u8 lodIdx, u32 perComponentWetness);

	CPedStreamRenderGfx*	m_pGfxData;
	Matrix34				m_rootMatrix;
	Matrix34				m_rootMatrixFPV;

	BANK_ONLY(CEntityDrawDataCommon m_commonData);

	u32				m_modelInfoIdx : 16;
	u32				m_fadeAlpha : 8;
	u32				m_naturalAmb : 8;

	u32				m_artificialAmb	: 8;
	u32				m_matID : 8;
#if RAGE_INSTANCED_TECH
	u32				m_viewportInstancedRenderBit : 8;
#endif
	u32				m_bAddToMotionBlurMask : 1;
	u32				m_bIsInInterior : 1;
	u32				m_bIsFPSSwimming : 1;
	u32				m_bIsFPV : 1;
	u32				m_bSupportsHairTint : 1;
	u32				m_bBlockHideInFirstPersonFlag : 1;
};

// EJ: Memory Optimization
// Use containment model to minimize overhead and maximize reusability
template <u32 SIZE, u32 NUM_MATRIX_SETS>
class CEntityDrawDataStreamPedWithProps
{
private:
	CEntityDrawDataStreamPed m_ped;
	CEntityDrawDataPedProps<SIZE, NUM_MATRIX_SETS> m_props;

public:
	CEntityDrawDataStreamPedWithProps() {}
	~CEntityDrawDataStreamPedWithProps() {}

	void Init(CEntity* pEntity) {m_ped.Init(pEntity); m_props.Init(pEntity);}
#if RAGE_INSTANCED_TECH
	void SetViewportInstancedRenderBit(u8 uBit)	{ m_ped.SetViewportInstancedRenderBit(uBit); }
#endif
	void Draw(CAddCompositeSkeletonCommand& skeletonCmd, u8 lodIdx, u32 perComponentWetness) {m_ped.Draw(skeletonCmd, &m_props, lodIdx, perComponentWetness);}
};

// ped entity draw data for rendering
class CEntityDrawDataPedBIG
{
	friend class CDrawPedBIGDC;
	template <u32 SIZE>
	friend class CEntityDrawDataPedBIGWithProps;

public:
	CEntityDrawDataPedBIG() {}
	~CEntityDrawDataPedBIG() {}

	void Init(CEntity* pEntity);
#if RAGE_INSTANCED_TECH
	void SetViewportInstancedRenderBit(u8 uBit)	{ m_viewportInstancedRenderBit = uBit; }
#endif
	void Draw(u8 lodIdx) {Draw(lodIdx, NULL);}

private:
	void Draw(u8 lodIdx, CEntityDrawDataPedPropsBase* pProps);

	CPedVariationData	m_varData;
	Matrix34			m_rootMatrix;

	BANK_ONLY(CEntityDrawDataCommon m_commonData);

	u32				m_modelInfoIdx : 16;
	u32				m_fadeAlpha : 8;
	u32				m_naturalAmb : 8;

	u32				m_artificialAmb	: 8;
	u32				m_matID : 8;
	u32				m_hiLOD : 1;
	u32				m_bUseBlendShapes : 1;
	u32				m_bAddToMotionBlurMask : 1;
	u32				m_bIsInInterior : 1;
	u32				m_bIsFPSSwimming : 1;
	ATTR_UNUSED u32	m_pad : 3;
#if RAGE_INSTANCED_TECH
	u32				m_viewportInstancedRenderBit : 8;
#endif
};

// EJ: Memory Optimization
// Use containment model to minimize overhead and maximize reusability
template <u32 SIZE>
class CEntityDrawDataPedBIGWithProps
{
private:
	CEntityDrawDataPedBIG m_ped;
	CEntityDrawDataPedProps<SIZE, 1> m_props;

public:
	CEntityDrawDataPedBIGWithProps() {}
	~CEntityDrawDataPedBIGWithProps() {}

	void Init(CEntity* pEntity) {m_ped.Init(pEntity); m_props.Init(pEntity);}
#if RAGE_INSTANCED_TECH
	void SetViewportInstancedRenderBit(u8 uBit)	{ m_ped.SetViewportInstancedRenderBit(uBit); }
#endif
	void Draw(u8 lodIdx) {m_ped.Draw(lodIdx, &m_props);}
};

// a single ped prop (which has become detached from the ped) render data
class CEntityDrawDataDetachedPedProp {
	friend class CDrawDetachedPedPropDC;
public:
	CEntityDrawDataDetachedPedProp() {}
	~CEntityDrawDataDetachedPedProp() {}

	void Init(CEntity* pEntity);
#if RAGE_INSTANCED_TECH
	void SetViewportInstancedRenderBit(u8 uBit)	{  m_viewportInstancedRenderBit = uBit; }
#endif
	void Draw();

private:
	Matrix34		m_mat;

	BANK_ONLY(CEntityDrawDataCommon m_commonData);

	u32				m_TexHash;
	u32				m_modelHash;

	u32				m_modelInfoIdx : 16;
	u32				m_fadeAlpha : 8;
	u32				m_naturalAmb : 8;

	u32				m_artificialAmb	: 8;
	u32				m_bAddToMotionBlurMask : 1;
	u32				m_bIsInterior : 1;
#if RAGE_INSTANCED_TECH
	u32				m_viewportInstancedRenderBit : 8;

	ATTR_UNUSED u32	m_pad : 14;
#else
	ATTR_UNUSED u32	m_pad : 22;
#endif
};

// vehicle variation draw data for rendering
class CEntityDrawDataVehicleVar {
	friend class CDrawVehicleVariationDC;
public:
	CEntityDrawDataVehicleVar() {}
	~CEntityDrawDataVehicleVar() {}

	void Init(CEntity* pEntity);
#if RAGE_INSTANCED_TECH
	void SetViewportInstancedRenderBit(u8 uBit)	{ m_viewportInstancedRenderBit = uBit; }
#endif
	void Draw();

private:
	CVehicleVariationInstance m_variation;

	Matrix34		m_mat[VMT_RENDERABLE + MAX_LINKED_MODS];
	Matrix34		m_rootMatrix;

	BANK_ONLY(CEntityDrawDataCommon m_commonData);

	u8				m_wheelBurstRatios[NUM_VEH_CWHEELS_MAX][2];
	u64				m_damagedBits;
	u64				m_destroyedBits;
    float           m_lodMult;
    float           m_lowLodMult;

	u32				m_modelInfoIdx : 16;
	u32				m_fadeAlpha : 8;
	u32				m_naturalAmb : 8;

	u32				m_artificialAmb	: 8;
	u32				m_matID : 8;
	u32				m_matLocalPlayerEmblem : 1;
	u32				m_hasBurstWheels : 1;
	u32				m_bAddToMotionBlurMask : 1;
	u32				m_bIsInterior : 1;
	u32				m_isHd : 1;
	u32				m_isWrecked : 1;
	u32				m_clampedLod : 8;
	u32				m_forceSetMaterial : 1;
#if RAGE_INSTANCED_TECH
	u32				m_viewportInstancedRenderBit : 8;
#endif
	ATTR_UNUSED u32	m_pad : 1;
};


// frag type object render data (usually a vehicle or breakable object)
struct CEntityDrawDataFrag {
	friend class CDrawFragDC;
public:
	CEntityDrawDataFrag() {}
	~CEntityDrawDataFrag() {}

	void Init(CEntity* pEntity);
#if RAGE_INSTANCED_TECH
	void SetViewportInstancedRenderBit(u8 uBit)	{ m_viewportInstancedRenderBit = uBit; }
#endif
	void Draw(bool damaged);

private:
	Matrix34		m_rootMatrix;
	Matrix34		m_modMatrix;
	u64				m_damagedBits;
	u64				m_destroyedBits;
    float           m_lodMult;
	float			m_lowLodMult;
	union
	{
		s32			m_modAssetIdx;
		CVehicleStreamRenderGfx* m_renderGfx;
	};

	BANK_ONLY(CEntityDrawDataCommon m_commonData);

	u8				m_wheelBurstRatios[NUM_VEH_CWHEELS_MAX][2];
	u32				m_modelInfoIdx :16;
	u32				m_fadeAlpha : 8;
	u32				m_naturalAmb : 8;

	u32				m_artificialAmb	: 8;
	u32				m_matID : 8;
	u32				m_flags : 8;
#if RAGE_INSTANCED_TECH
	u32				m_viewportInstancedRenderBit : 8;
#endif

	u32				m_phaseLODs : 15;
	u32				m_lastLODIdx : 2;
	u32				m_forceAlphaForWaterReflection : 1;
	u32				m_isWrecked : 1;
	u32				m_drawModOnTop : 1; // mod doesn't overwrite original drawable, it draws on top of it
	u32				m_fragMod : 1;
	u32				m_bIsInterior : 1;
	u32				m_forceSetMaterial : 1;
	u32				m_matLocalPlayerEmblem : 1;

	s8				m_currentLOD; // TODO -- u32:2
	s8				m_clampedLod;
};

class CEntityDrawDataBreakableGlass {
	friend class CDrawBreakableGlassDC;
public:
	CEntityDrawDataBreakableGlass(bgDrawable& drawable, const bgDrawableDrawData &bgDrawData, const CEntity *entity);
	~CEntityDrawDataBreakableGlass() {}

	void Draw();
private:

	Matrix34				m_matrix;
	Vector4					m_crackTexMatrix;
	Vector4					m_crackTexOffset;

	bgDrawable*				m_drawable;
	float*					m_transforms;
	bgGpuBuffers*			m_pBuffers;
	const bgCrackStarMap*	m_pCrackStarMap;

	int						m_numTransforms;
	int						m_lod;
	u32						m_naturalAmb : 8;
	u32						m_artificialAmb : 8;
	bool					m_bIsInInterior;
	int						m_arrPieceIndexCount[bgBreakable::sm_iMaxBatches];
	int						m_arrCrackIndexCount[bgBreakable::sm_iMaxBatches];
};

// frag type object which hasn't instanced into a full blown frag (yet)
class CEntityDrawDataFragType {
	friend class CDrawFragTypeDC;
public:
	CEntityDrawDataFragType(){}
	ENABLE_FRAG_OPTIMIZATION_ONLY(virtual) ~CEntityDrawDataFragType() {}

	void Init(CEntity* pEntity);
#if RAGE_INSTANCED_TECH
	void SetViewportInstancedRenderBit(u8 uBit)	{ m_viewportInstancedRenderBit = uBit; }
#endif
	void Draw();

#if ENABLE_FRAG_OPTIMIZATION
	virtual void InitExtras(CEntity* UNUSED_PARAM(pEntity)) {}
	virtual void DrawExtras() {}

	virtual grmMatrixSet* GetSharedMatrixSetOverride() { return NULL; }
#endif // ENABLE_FRAG_OPTIMIZATION

protected:
	Matrix34		m_matrix;

	BANK_ONLY(CEntityDrawDataCommon m_commonData);

	u32				m_modelInfoIdx : 16;
	u32				m_fadeAlpha : 8;
	u32				m_naturalAmb : 8;

	u32				m_artificialAmb	: 8;
	u32				m_matID : 8;	
	u32				m_phaseLODs : 15;
	u32				m_forceAlphaForWaterReflection : 1;

	u32				m_lastLODIdx : 2;
	u32				m_bIsInterior : 1;
#if RAGE_INSTANCED_TECH
	u32				m_viewportInstancedRenderBit : 8;
#endif
};

#if ENABLE_FRAG_OPTIMIZATION
class CEntityDrawDataFragTypeVehicle : public CEntityDrawDataFragType {
	friend class CDrawFragTypeVehicleDC;
public:
	CEntityDrawDataFragTypeVehicle() : m_dataSize(0) {}
	virtual ~CEntityDrawDataFragTypeVehicle() {}

	virtual void InitExtras(CEntity* pEntity);
	virtual void DrawExtras();

	virtual grmMatrixSet* GetSharedMatrixSetOverride()
	{
#if __PS3
		return reinterpret_cast<grmMatrixSet*>(gDCBuffer->ResolveDrawListAddress(m_dataAddress));
#else
		return reinterpret_cast<grmMatrixSet*>(gDCBuffer->GetDataBlock(m_dataSize, m_dataAddress));
#endif
	}

private:
	DrawListAddress m_dataAddress;
	u32 m_dataSize;
};
#endif // ENABLE_FRAG_OPTIMIZATION

class CEntityDrawDataSkinned {
	friend class CDrawSkinnedEntityDC;
public:
	CEntityDrawDataSkinned() {}
	~CEntityDrawDataSkinned() {}

	void Init(CEntity* pEntity);
#if RAGE_INSTANCED_TECH
	void SetViewportInstancedRenderBit(u8 uBit)	{ m_viewportInstancedRenderBit = uBit; }
#endif
	void Draw();
	void SetIsHD(bool hd) { m_isHD = hd; }
	void SetIsWeaponModelInfo(bool isWeaponModelInfo) {m_isWeaponModelInfo = isWeaponModelInfo; }

private:
	Matrix34		m_rootMatrix;
	Matrix34		m_rootMatrixFPV;

	u32				m_modelInfoIdx : 16;
	u32				m_fadeAlpha : 8;
	u32				m_naturalAmb : 8;

	u32				m_artificialAmb	: 8;
	u32				m_matID : 8;
	u32				m_phaseLODs : 15;
	u32				m_forceAlphaForWaterReflection : 1;

	u32				m_lastLODIdx : 2;
	u32			    m_isWeaponModelInfo : 1;
	u32			    m_isHD : 1;
	u32				m_bAddToMotionBlurMask : 1;
	u32				m_bIsInterior : 1;
	u32				m_bIsFPV : 1;
#if RAGE_INSTANCED_TECH
	u32				m_viewportInstancedRenderBit : 8;
#endif
#if __BANK
	CEntityDrawDataCommon m_commonData;
#endif // __BANK
};

#endif // INC_DRAWBLDATASTRCTS_H_

