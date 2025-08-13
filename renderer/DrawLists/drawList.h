/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    drawlist.h
// PURPOSE : intermediate data between game entities and command buffers. Gives us
// independent data which can be handed off to the render thread
// AUTHOR :  john.
// CREATED : 30/8/06
//
/////////////////////////////////////////////////////////////////////////////////


#ifndef INC_DRAWLIST_H_
#define INC_DRAWLIST_H_

// Rage Headers
#include "atl/map.h"
#include "grcore/effect_config.h"
#include "system/memory.h"
#include "vector/quaternion.h"
#include "grcore/Effect.h"
#include "grcore/im.h"
#include "grmodel/model.h"
#include "vectormath/threadvector.h"

// Framework headers
#include "fwmaths/rect.h"
#include "fwdrawlist/drawlist.h"

// Game Headers
#include "debug/debug.h"
#include "frontend/MiniMap.h"
#include "renderer/color.h"
#include "renderer/DrawLists/DrawableDataStructs.h"
#include "renderer/renderer.h"
#include "renderer/deferred/DeferredConfig.h"

#include "text/text.h"
#include "vehicles/VehicleDefines.h"

extern bank_bool g_cache_entities;

namespace rage {
class dlCmdBase;
class fragInst;
class ptxEffectInst;
class rmcInstanceData;
class grmShader;
class grmGeometry;
class grmModel;
class grmMatrixSet;
class phInst;
}

class CBaseModelInfo;
class CDynamicEntity;
class CGrassBatch;
class CPedStreamRenderGfx;
class CPtFxSortedEntity;
class CVehicleGlassComponent;
class CVehicleGlassComponentEntity;
class CVehicleModelInfo;
class CViewport;
class CCustomShaderEffectBase;
class CEntityDrawHandler;
class CVehicleVariationInstance;

// GTA-specific draw commands
enum eGtaInstructionId {
	// commands for drawing the entity objects (render lists)
	DC_DrawEntity					= 128,
	DC_DrawEntityFM					= 129,
	DC_DrawSkinnedEntity			= 130,

	// ped type drawing
	DC_DrawPedBIG					= 131,
	DC_DrawStreamPed				= 132,
	DC_DrawDetachedPedProp			= 133,

	// frag drawing
	DC_DrawFrag						= 134,
	DC_DrawFragType					= 135,

	// prototype drawing
	DC_DrawPrototypeBatch			= 136,

	// shader related commands
	DC_CustomShaderEffect			= 137,

	// hud related commands
	DC_MiniMap_UpdateBlips			= 138,
	DC_RenderColouredQuad			= 139,
	DC_MiniMap_RenderStateStruct	= 140,
	DC_MiniMap_AddSonarBlipToStage = 141,
	DC_MiniMap_ResetBlipConeFlags	= 142,
	DC_MiniMap_RemoveUnusedBlipConesFromStage	= 143,
	DC_MiniMap_RenderBlipCone		= 144,
	DC_DrawSprite					= 145,
	DC_DrawSpriteUV					= 146,
	DC_DrawRect						= 147,
	DC_DrawSpritePersp				= 149,
	DC_DrawRadioHudText				= 153,
	DC_RenderText					= 154,

	// misc
	//DC_SmashMan_SetDrawBuckets	= 172,
	//DC_SmashMan_ResetDrawBuckets	= 173,

	DC_DrawPtxEffectInst			= 174,
	DC_DrawVehicleGlassComponent	= 175,

 	DC_SetDrawableLODCalcPos		= 177,

	DC_DrawBreakableGlass			= 178,

	DC_BeginRender					= 179,
	DC_StaticRenderImposterModel	= 180,
	DC_RenderPhasesDrawInit			= 181,
	DC_RenderLeafColour				= 182,

	DC_PlantMgrRender				= 183,
	DC_PlantMgrRenderDecal			= 184,
	DC_PlantMgrShadowRender 		= 185,
	DC_VisualEffectsRenderSky		= 186,
	DC_WaterRender					= 187,
	DC_StaticRenderCall				= 188,

	DC_EndRender					= 189,
	DC_DeferredCallback				= 190,
	DC_CopyDepthBufferToVS			= 191,
	DC_ProcessNonDepthFX			= 192,
	DC_Debug3dStaticRender			= 193,
	DC_NewHudRender					= 194,
	DC_Debug2dStaticRender			= 195,
	DC_DebugRenderReleaseInfo		= 196,

	// skeleton storage
	DC_AddSkeleton					= 197,
	DC_AddCompositeSkeleton			= 198,

	DC_DrawDrawable					= 199,
	
	DC_RopeShadowRender				= 200,
	DC_DrawSeeThroughQuads			= 201,
	DC_BeginOcclusionQuery			= 202,
	DC_EndOcclusionQuery			= 203,

	DC_DrawVehicleVariation			= 204,

	DC_DrawForceLowerLOD			= 205,
	DC_SetupTimeCycleFrameInfo		= 206,
	DC_ClearTimeCycleFrameInfo		= 207,
	DC_SetupLightsFrameInfo			= 208,
	DC_ClearLightsFrameInfo			= 209,
	
	DC_DrawGlowQuads				= 210,
	DC_OverrideSkeleton				= 211,
	DC_SetDrawableStatContext		= 212,
	DC_ParticleShadowRender			= 213,
	DC_ParticleShadowRenderAllCascades = 214,

	DC_SetupLODLightsFrameInfo      = 215,
	DC_ClearLODLightsFrameInfo      = 216,
	DC_BoxOcclusionQueries			= 217,
	DC_BoxConditionalQueries		= 218,
	DC_dlCmdPickupBlaster			= 219,
	DC_dlCmdLightOverride			= 220,

	DC_PedDamageSetRender			= 221,
	DC_PedCompressedDamageSetRender = 222,

	DC_SetupCoronasFrameInfo		= 223,
	DC_ClearCoronasFrameInfo		= 224,
	DC_DistantCarsRender			= 225,
	DC_DrawScriptIM					= 226,
	DC_DrawEntityInstanced			= 227,
	DC_DrawGrassBatch				= 228,

	DC_UIWorldIconIM				= 229,

	DC_SetupFogVolumesFrameInfo		= 230,
	DC_ClearFogVolumesFrameInfo		= 231,
	DC_SetupVfxLightningFrameInfo	= 232,
	DC_ClearVfxLightningFrameInfo	= 233,
	DC_DrawFullScreenGlowQuads		= 234,

#if ENABLE_FRAG_OPTIMIZATION
	DC_DrawFragTypeVehicle			= 235,
#endif
};


#if __DEV
struct DrawCommandDebugInfo
{
	const char *GetExtraDebugInfo(char * buffer, size_t bufferSize);

	DrawCommandDebugInfo()	{}
	DrawCommandDebugInfo(CEntity *entity);

	strLocalIndex m_ModelIndex;
};

#else // __DEV
struct DrawCommandDebugInfo
{
	DrawCommandDebugInfo()						{}
	DrawCommandDebugInfo(CEntity * /*entity*/)	{}
};

#endif // __DEV



// class to hold command buffer &
class CDrawCommandBuffer : public dlDrawCommandBuffer {

public:
/*	CDrawCommandBuffer() {}
	~CDrawCommandBuffer() {}
*/
	void Initialise();
	void Shutdown();
	void Reset();

	virtual  void	FlipBuffers(void);

#if !__FINAL
	virtual void	DumpMemoryInfo();
#endif // !__FINAL

	DrawListAddress::Parameter LookupWriteShaderCopy(const CCustomShaderEffectBase* originalAddr);
};

// ------ available draw commands --------
// don't increase this - I'm assuming I can pack the bit field into a u32 for now
#define MAX_FRAG_CHILDREN	(64)			
void FragDraw(int currentLOD, CBaseModelInfo* pModelInfo,  const grmMatrixSet* ms, const Matrix34& matrix, u64 damagedBits, u64 destroyedBits, float& dist, u8 flag, float lodMult, bool damaged, const fragType* pFragType);

void CopyOffShader(const CCustomShaderEffectBase* pShader, u32 dataSize, DrawListAddress &drawListOffset, bool bDoubleBuffered, CCustomShaderEffectBaseType *pType);


// -- drawing object commands --

// draw an game entity drawing command
class CDrawEntityDC : public dlCmdBase{
	friend class CEntity;
public:
	enum {
		INSTRUCTION_ID = DC_DrawEntity,
	};
	CDrawEntityDC(CEntity* pEntity);
	inline CDrawEntityDC() {}

	s32 GetCommandSize()	{return(sizeof(*this));}
	static void ExecuteStatic(dlCmdBase &cmd) { ((CDrawEntityDC &) cmd).Execute(); }
	DEV_ONLY(static const char *GetExtraDebugInfo(dlCmdBase & base, char * buffer, size_t bufferSize, Color32 &color));
	void Execute();

	static DrawListAddress SharedData(CEntity * pEntity);
	CEntityDrawData&	GetDrawData(void) { return *reinterpret_cast<CEntityDrawData*>(gDCBuffer->GetDataBlock(sizeof(CEntityDrawData), m_dataAddress)); }
private:
    DrawListAddress m_dataAddress;
	DrawCommandDebugInfo m_DebugInfo;
};

// clone of CDrawEntityDC:
// draw an game entity drawing command with full matrix
class CDrawEntityFmDC : public dlCmdBase{
public:
	enum {
		INSTRUCTION_ID = DC_DrawEntityFM,
	};
	CDrawEntityFmDC(CEntity* pEntity);
	CDrawEntityFmDC() {}

	s32 GetCommandSize()	{return(sizeof(*this));}
	static void ExecuteStatic(dlCmdBase &cmd) { ((CDrawEntityFmDC &) cmd).Execute(); }
	DEV_ONLY(static const char *GetExtraDebugInfo(dlCmdBase & base, char * buffer, size_t bufferSize, Color32 &color));
	void Execute();

	CEntityDrawDataFm&	GetDrawData(void) { return *reinterpret_cast<CEntityDrawDataFm*>(gDCBuffer->GetDataBlock(sizeof(CEntityDrawDataFm), m_dataAddress)); }
	static DrawListAddress SharedData(CEntity * pEntity);
private:
	DrawListAddress m_dataAddress;
	DrawCommandDebugInfo m_DebugInfo;
};

class CDrawEntityInstancedDC : public dlCmdBase{
	friend class CEntity;
public:
	enum {
		INSTRUCTION_ID = DC_DrawEntityInstanced,
	};
	typedef CEntityInstancedDrawData EntityDrawData;
	CDrawEntityInstancedDC(CEntity* pEntity, grcInstanceBufferList &list, int lod);
	CDrawEntityInstancedDC(CEntity* pEntity, grcInstanceBuffer *ib, int lod);
	inline CDrawEntityInstancedDC() {}

	s32 GetCommandSize()	{return(sizeof(*this));}
	static void ExecuteStatic(dlCmdBase &cmd) { ((CDrawEntityInstancedDC &) cmd).Execute(); }
	DEV_ONLY(static const char *GetExtraDebugInfo(dlCmdBase & base, char * buffer, size_t bufferSize, Color32 &color));
	void Execute();

	static DrawListAddress SharedData(CEntity * pEntity, grcInstanceBuffer *ib, int lod);
	EntityDrawData&	GetDrawData(void) { return *reinterpret_cast<EntityDrawData*>(gDCBuffer->GetDataBlock(sizeof(EntityDrawData), m_dataAddress)); }
private:
	DrawListAddress m_dataAddress;
	DrawCommandDebugInfo m_DebugInfo;
};

class CDrawGrassBatchDC : public dlComputeCmdBase{
public:
	enum {
		INSTRUCTION_ID = DC_DrawGrassBatch,
	};
	typedef dlComputeCmdBase parent_type;
	typedef CGrassBatchDrawData EntityDrawData;
	CDrawGrassBatchDC(CGrassBatch* pEntity);

	s32 GetCommandSize()	{return(sizeof(*this));}
	static void ExecuteStatic(dlCmdBase &cmd) { ((CDrawGrassBatchDC &) cmd).Execute(); }
	DEV_ONLY(static const char *GetExtraDebugInfo(dlCmdBase & base, char * buffer, size_t bufferSize, Color32 &color));
	void Execute();

	static DrawListAddress SharedData(CGrassBatch *pEntity);
	EntityDrawData&	GetDrawData(void) { return *reinterpret_cast<EntityDrawData*>(gDCBuffer->GetDataBlock(sizeof(EntityDrawData), m_dataAddress)); }

	void DispatchComputeShader()	{ GetDrawData().DispatchComputeShader(); }
	WIN32PC_ONLY(void CopyStructureCount()		{ GetDrawData().CopyStructureCount(); })
private:
	DrawListAddress m_dataAddress;
	DrawCommandDebugInfo m_DebugInfo;
};

class CAddSkeletonCommand {
public:
	CAddSkeletonCommand(CDynamicEntity *entity, int skelMatrixMode = -1, bool damaged = false);
	CAddSkeletonCommand(crSkeleton* pSourceSkel, eSkelMatrixMode skelMatMode);
	CAddSkeletonCommand(bool disabled);	// This particular constructor will create a NOP CAddSkeletonCommand

	void Execute();

	u32 GetLodIdx() const { return m_lodIdx; }
	bool GetDamaged() const { return m_damaged != 0; }

	bool GetIsStrippedHead() const { return m_strippedHead != 0; }

private:
	// Position of the skeleton, can be -1 if a dlCmdDataBlock is to follow
	s32							m_skelDrawListOffset;
	u32							m_skelDataSize : 24;
	u32							m_lodIdx : 6;
	u32							m_damaged : 1;
	u32							m_strippedHead : 1;
};

class CAddCompositeSkeletonCommand {
public:
	CAddCompositeSkeletonCommand(CDynamicEntity *entity, int skelMatrixMode = -1);
	CAddCompositeSkeletonCommand(bool disabled);	// This particular constructor will create a NOP CAddCompositeSkeletonCommand

	void Execute(u32 idx);

	u32 GetLodIdx() const { return m_lodIdx; }

	bool GetIsStrippedHead() const { return m_strippedHead != 0; }

private:
	// Position of the skeleton, can be -1 if a dlCmdDataBlock is to follow
	s32							m_skelDrawListOffset[PV_MAX_COMP];
	u32							m_skelDataSize[PV_MAX_COMP];
	u8							m_numHeadBones;
	u8							m_lodIdx : 7;
	u8							m_strippedHead : 1;
};

// EJ: Overloaded function that creates draw data objects and invokes constructor. This
// generates a proper vtable, enabling polymorphic behavior.
template<class EntityDrawData>
DrawListAddress CopyOffEntityVirtual(CEntity * pEntity);

class CDrawPedBIGDC : public dlCmdBase{

public:
	enum {
		INSTRUCTION_ID = DC_DrawPedBIG,
	};
	CDrawPedBIGDC(CEntity* pEntity);

	s32 GetCommandSize()	{return(sizeof(*this));}
	static void ExecuteStatic(dlCmdBase &cmd) { ((CDrawPedBIGDC &) cmd).Execute(); }
	void Execute();

	// EJ: Memory Optimization
	// EJ: Either GetDrawData or GetDrawDataWithProps should be used (not both)
	template <u32 SIZE>
	CEntityDrawDataPedBIGWithProps<SIZE>& GetDrawDataWithProps(void) {return *reinterpret_cast<CEntityDrawDataPedBIGWithProps<SIZE>*>(gDCBuffer->GetDataBlock(sizeof(CEntityDrawDataPedBIGWithProps<SIZE>), m_dataAddress));}
	CEntityDrawDataPedBIG& GetDrawData(void) { return *reinterpret_cast<CEntityDrawDataPedBIG*>(gDCBuffer->GetDataBlock(sizeof(CEntityDrawDataPedBIG), m_dataAddress)); }

	// EJ: Either SharedData or SharedDataWithProps should be used (not both)
	template <u32 SIZE>
	static DrawListAddress SharedDataWithProps(CEntity* pEntity) {return CopyOffEntityVirtual<CEntityDrawDataPedBIGWithProps<SIZE> >(pEntity);}
	static DrawListAddress SharedData(CEntity* pEntity);

	DEV_ONLY(static const char *GetExtraDebugInfo(dlCmdBase & base, char * buffer, size_t bufferSize, Color32 &color));

private:
	CAddSkeletonCommand			m_AddSkeleton;		// For the implicit dlCmdAddSkeleton call
	DrawListAddress             m_dataAddress;
	DrawCommandDebugInfo		m_DebugInfo;
	u8 m_numProps;
};

class CDrawStreamPedDC : public dlCmdBase{

public:
	enum {
		INSTRUCTION_ID = DC_DrawStreamPed,
	};
	CDrawStreamPedDC(CEntity* pEntity);
	CDrawStreamPedDC(CEntity* pEntity, bool noImplicitAddSkeleton);

	s32 GetCommandSize()	{return(sizeof(*this));}
	static void ExecuteStatic(dlCmdBase &cmd) { ((CDrawStreamPedDC &) cmd).Execute(); }
	void Execute();

	// EJ: Memory Optimization
	// EJ: Either GetDrawData or GetDrawDataWithProps should be used (not both)
	template <u32 SIZE, u32 NUM_MATRIX_SETS>
	CEntityDrawDataStreamPedWithProps<SIZE, NUM_MATRIX_SETS>& GetDrawDataWithProps(void) { return *reinterpret_cast<CEntityDrawDataStreamPedWithProps<SIZE, NUM_MATRIX_SETS>*>(gDCBuffer->GetDataBlock(sizeof(CEntityDrawDataStreamPedWithProps<SIZE, NUM_MATRIX_SETS>), m_dataAddress)); }
	CEntityDrawDataStreamPed& GetDrawData(void) { return *reinterpret_cast<CEntityDrawDataStreamPed*>(gDCBuffer->GetDataBlock(sizeof(CEntityDrawDataStreamPed), m_dataAddress)); }
	
	// EJ: Either SharedData or SharedDataWithProps should be used (not both)
	template <u32 SIZE, u32 NUM_MATRIX_SETS>
	static DrawListAddress SharedDataWithProps(CEntity* pEntity) {return CopyOffEntityVirtual<CEntityDrawDataStreamPedWithProps<SIZE, NUM_MATRIX_SETS> >(pEntity);}
	static DrawListAddress SharedData(CEntity* pEntity);

	DEV_ONLY(static const char *GetExtraDebugInfo(dlCmdBase & base, char * buffer, size_t bufferSize, Color32 &color));

private:
	CAddCompositeSkeletonCommand	m_AddCompositeSkeleton;		// For the implicit dlCmdAddCompositeSkeleton call
	DrawListAddress                 m_dataAddress;
	DrawCommandDebugInfo			m_DebugInfo;
	u32 m_numProps : 7;
	u32 m_isFPVPed : 1;
	u32 m_perComponentWetness : 24;
};

class CDrawDetachedPedPropDC : public dlCmdBase{
public:
	enum {
		INSTRUCTION_ID = DC_DrawDetachedPedProp,
	};
	CDrawDetachedPedPropDC(CEntity* pEntity);
    CEntityDrawDataDetachedPedProp& GetDrawData(void) { return *reinterpret_cast<CEntityDrawDataDetachedPedProp*>(gDCBuffer->GetDataBlock(sizeof(CEntityDrawDataDetachedPedProp), m_dataAddress)); }

	s32 GetCommandSize()	{return(sizeof(*this));}
	static void ExecuteStatic(dlCmdBase &cmd) { ((CDrawDetachedPedPropDC &) cmd).Execute(); }
	void Execute();
	DEV_ONLY(static const char *GetExtraDebugInfo(dlCmdBase & base, char * buffer, size_t bufferSize, Color32 &color));

private:
	DrawListAddress m_dataAddress;
	DrawCommandDebugInfo m_DebugInfo;
};

class CDrawVehicleVariationDC : public dlCmdBase{

public:
	enum {
		INSTRUCTION_ID = DC_DrawVehicleVariation,
	};
	CDrawVehicleVariationDC(CEntity* pEntity);

	s32 GetCommandSize()	{return(sizeof(*this));}
	static void ExecuteStatic(dlCmdBase &cmd) { ((CDrawVehicleVariationDC &) cmd).Execute(); }
	void Execute();

	void SetupBurstWheels(const u8* burstAndSideRatios);

	CEntityDrawDataVehicleVar&	GetDrawData(void) { return *reinterpret_cast<CEntityDrawDataVehicleVar*>(gDCBuffer->GetDataBlock(sizeof(CEntityDrawDataVehicleVar), m_dataAddress)); }

private:
	DrawListAddress             m_dataAddress;
};

void FragDrawCarWheels(CVehicleModelInfo* pModelInfo, const Matrix34& rootMatrix, const grmMatrixSet *ms, u64 destroyedBits, int bucket, bool burstRatios, u8 wheelBurstRatios[NUM_VEH_CWHEELS_MAX][2], float dist, u8 flags, CVehicleStreamRenderGfx* renderGfx, bool canHaveRearWheel, float lodMult);

class CDrawFragDC : public dlCmdBase{
public:
	enum {
		INSTRUCTION_ID = DC_DrawFrag,
	};
	enum eDrawFragFlags
	{
		CAR_PLUS_WHEELS		= BIT(0),
		CAR_WHEEL			= BIT(1),
		WHEEL_BURST_RATIOS	= BIT(2),
		IS_LOCAL_PLAYER		= BIT(3),
		HD_REQUIRED			= BIT(4),
		FORCE_HI_LOD		= BIT(5)
	};

	CDrawFragDC(CEntity* pEntity, bool damaged);

	s32 GetCommandSize()	{return(sizeof(*this));}
	static void ExecuteStatic(dlCmdBase &cmd) { ((CDrawFragDC &) cmd).Execute(); }
	void Execute();

	void SetupBurstWheels(const u8* burstAndSideRatios);
	void SetFlag(u8 nFlag) {GetDrawData().m_flags |= nFlag;}

	CEntityDrawDataFrag&	GetDrawData(void) { return *reinterpret_cast<CEntityDrawDataFrag*>(gDCBuffer->GetDataBlock(sizeof(CEntityDrawDataFrag), m_dataAddress)); }
	DEV_ONLY(static const char *GetExtraDebugInfo(dlCmdBase & base, char * buffer, size_t bufferSize, Color32 &color));

private:
	CAddSkeletonCommand		m_AddSkeleton;		// For the implicit dlCmdAddSkeleton call
	DrawListAddress         m_dataAddress;
	DrawCommandDebugInfo	m_DebugInfo;
};

class CDrawBreakableGlassDC : public dlCmdBase{
public:
	enum {
		INSTRUCTION_ID = DC_DrawBreakableGlass,
	};
	CDrawBreakableGlassDC(bgDrawable& drawable, const bgDrawableDrawData &bgDrawData, const CEntity* entity) : 
	m_data(drawable, bgDrawData, entity) {}

	s32 GetCommandSize()	{return(sizeof(*this));}
	static void ExecuteStatic(dlCmdBase &cmd) { ((CDrawBreakableGlassDC &) cmd).Execute(); }
	void Execute();

	CEntityDrawDataBreakableGlass&	GetDrawData(void) { return(m_data); }
private:
	CEntityDrawDataBreakableGlass	m_data;
};

class CDrawFragTypeDC : public dlCmdBase{
public:
	enum {
		INSTRUCTION_ID = DC_DrawFragType,
	};
	CDrawFragTypeDC(CEntity* pEntity);
	CDrawFragTypeDC() {}

	s32 GetCommandSize()	{return(sizeof(*this));}
	static void ExecuteStatic(dlCmdBase &cmd) { ((CDrawFragTypeDC &) cmd).Execute(); }
	DEV_ONLY(static const char *GetExtraDebugInfo(dlCmdBase & base, char * buffer, size_t bufferSize, Color32 &color));
	void Execute();

	CEntityDrawDataFragType&	GetDrawData(void) { return *reinterpret_cast<CEntityDrawDataFragType*>(gDCBuffer->GetDataBlock(sizeof(CEntityDrawDataFragType), m_dataAddress)); }

private:
	DrawListAddress m_dataAddress;
	DrawCommandDebugInfo m_DebugInfo;
};

#if ENABLE_FRAG_OPTIMIZATION
class CDrawFragTypeVehicleDC : public dlCmdBase{
public:
	enum {
		INSTRUCTION_ID = DC_DrawFragTypeVehicle,
	};
	CDrawFragTypeVehicleDC(CEntity* pEntity);
	CDrawFragTypeVehicleDC() {}

	s32 GetCommandSize()	{return(sizeof(*this));}
	static void ExecuteStatic(dlCmdBase &cmd) { ((CDrawFragTypeVehicleDC &) cmd).Execute(); }
	DEV_ONLY(static const char *GetExtraDebugInfo(dlCmdBase & base, char * buffer, size_t bufferSize, Color32 &color));
	void Execute();

	CEntityDrawDataFragTypeVehicle&	GetDrawData(void) { return *reinterpret_cast<CEntityDrawDataFragTypeVehicle*>(gDCBuffer->GetDataBlock(sizeof(CEntityDrawDataFragTypeVehicle), m_dataAddress)); }

private:
	DrawListAddress m_dataAddress;
	DrawCommandDebugInfo m_DebugInfo;
};
#endif // ENABLE_FRAG_OPTIMIZATION

class IDrawListPrototype
{
public:
	virtual ~IDrawListPrototype() {}
	virtual void* AddDataForEntity(CEntity * entity, const CEntityDrawHandler * drawHandler, void * data) = 0;
	virtual void  Draw(u32 batchSize, void * data) = 0;
	virtual u32   SizeOfElement() = 0;
};

class CDrawPrototypeBatchDC : public dlCmdBase{
public:
	enum {
		INSTRUCTION_ID = DC_DrawPrototypeBatch,
	};
	CDrawPrototypeBatchDC(IDrawListPrototype * prototype, u32 batchSize, DrawListAddress data);
	~CDrawPrototypeBatchDC() {}

	s32 GetCommandSize()	{return(sizeof(CDrawPrototypeBatchDC) + 0);}
	static void ExecuteStatic(dlCmdBase &cmd) { ((CDrawPrototypeBatchDC &) cmd).Execute(); }
	void Execute();

private:
	IDrawListPrototype * m_Prototype;
	u32 m_BatchSize;
	DrawListAddress m_Data;
};

class CDrawSkinnedEntityDC : public dlCmdBase{
public:
	enum {
		INSTRUCTION_ID = DC_DrawSkinnedEntity,
	};
	CDrawSkinnedEntityDC(CEntity* pEntity);

	s32 GetCommandSize()	{return(sizeof(*this));}
	static void ExecuteStatic(dlCmdBase &cmd) { ((CDrawSkinnedEntityDC &) cmd).Execute(); }
	DEV_ONLY(static const char *GetExtraDebugInfo(dlCmdBase & base, char * buffer, size_t bufferSize, Color32 &color));
	void Execute();

	CEntityDrawDataSkinned&	GetDrawData(void) { return *reinterpret_cast<CEntityDrawDataSkinned*>(gDCBuffer->GetDataBlock(sizeof(CEntityDrawDataSkinned), m_dataAddress)); }

private:
	CAddSkeletonCommand			m_AddSkeleton;		// For the implicit dlCmdAddSkeleton call
	DrawListAddress             m_dataAddress;
	DrawCommandDebugInfo		m_DebugInfo;
};

/// -- end of entity drawing commands ---

class CCustomShaderEffectDC : public dlCmdBase {
public:
	enum {
		INSTRUCTION_ID = DC_CustomShaderEffect,
	};
	CCustomShaderEffectDC(const CCustomShaderEffectBase& shaderEffect, u32 modelIndex, bool bExecute, CCustomShaderEffectBaseType *pType);
	CCustomShaderEffectDC(void *ptr);

	s32 GetCommandSize()	{return(sizeof(*this));}
	static void ExecuteStatic(dlCmdBase &cmd) { ((CCustomShaderEffectDC &) cmd).Execute(); }
	void Execute();

	CCustomShaderEffectBase*	GetCsePtr();

	static CCustomShaderEffectBase* GetLatestShader() { return(ms_pLatestShader); }
	static void SetLatestShader(CCustomShaderEffectBase * shader) { ms_pLatestShader = shader; }
private:
#if __DEV
	s32				m_shaderOffsetDup;
#endif // __DEV
	DrawListAddress		m_shaderOffset;

	u32	m_modelIndex;				// TEMP: drawable ptr required to SetVar the effect against on the RT

	u32				m_dataSize				: 16;
	u32				m_bExecute				: 1;
	u32				m_bResetLatestShader	: 1;
	u32				m_bDoubleBuffered		: 1;
	u32				m_bBrokenOffPart		: 1;
	ATTR_UNUSED u32	m_pad0					: 12;

	static DECLARE_MTR_THREAD CCustomShaderEffectBase* ms_pLatestShader;
};

// ----------------------------------------------------------------------------------------------- //

// updates blips on the RT
class CMiniMap_UpdateBlips : public dlCmdBase{
public:
	enum {
		INSTRUCTION_ID = DC_MiniMap_UpdateBlips,
	};
	s32 GetCommandSize()  {return(sizeof(*this));}
	static void ExecuteStatic(dlCmdBase &cmd) { ((CMiniMap_UpdateBlips &) cmd).Execute(); }
	void Execute();

	CMiniMap_UpdateBlips() {}
};

class CMiniMap_ResetBlipConeFlags : public dlCmdBase{
public:
	enum {
		INSTRUCTION_ID = DC_MiniMap_ResetBlipConeFlags,
	};
	s32 GetCommandSize()  {return(sizeof(*this));}
	static void ExecuteStatic(dlCmdBase &cmd) { ((CMiniMap_ResetBlipConeFlags &) cmd).Execute(); }
	void Execute();

	CMiniMap_ResetBlipConeFlags() {}
};

class CMiniMap_RemoveUnusedBlipConesFromStage : public dlCmdBase{
public:
	enum {
		INSTRUCTION_ID = DC_MiniMap_RemoveUnusedBlipConesFromStage,
	};
	s32 GetCommandSize()  {return(sizeof(*this));}
	static void ExecuteStatic(dlCmdBase &cmd) { ((CMiniMap_RemoveUnusedBlipConesFromStage &) cmd).Execute(); }
	void Execute();

	CMiniMap_RemoveUnusedBlipConesFromStage() {}
};

class CMiniMap_RenderBlipCone : public dlCmdBase{
public:
	enum {
		INSTRUCTION_ID = DC_MiniMap_RenderBlipCone,
	};

	s32 GetCommandSize()  {return(sizeof(*this));}
	static void ExecuteStatic(dlCmdBase &cmd) { ((CMiniMap_RenderBlipCone &) cmd).Execute(); }
	void Execute();

	CMiniMap_RenderBlipCone(CBlipCone *pBlipCone) : m_BlipCone(*pBlipCone) {}

private:
	CBlipCone m_BlipCone;
};

// adds stealth/sonar blips on the RT
class CMiniMap_AddSonarBlipToStage : public dlCmdBase{
public:
	enum {
		INSTRUCTION_ID = DC_MiniMap_AddSonarBlipToStage,
	};
	s32 GetCommandSize()  {return(sizeof(*this));}
	static void ExecuteStatic(dlCmdBase &cmd) { ((CMiniMap_AddSonarBlipToStage &) cmd).Execute(); }
	void Execute();

	CMiniMap_AddSonarBlipToStage(const sSonarBlipStruct &sonarBlip);

private:
	sSonarBlipStructRT m_SonarBlip;
};

// sets the player struct to use on the minimap
class CMiniMap_RenderState_Setup : public dlCmdBase{
public:
	enum {
		INSTRUCTION_ID = DC_MiniMap_RenderStateStruct,
	};
	s32 GetCommandSize()  {return(sizeof(*this));}
	static void ExecuteStatic(dlCmdBase &cmd) { ((CMiniMap_RenderState_Setup &) cmd).Execute(); }
	void Execute();

	// default constructor creates a command that hides minimap
	CMiniMap_RenderState_Setup() {
		m_StoredMiniMapRenderStruct.m_bShouldProcessMiniMap = false; 
		m_StoredMiniMapRenderStruct.m_bShouldRenderMiniMap = false;
	}
	CMiniMap_RenderState_Setup(sMiniMapRenderStateStruct *pRenderStateStruct);

private:
	sMiniMapRenderStateStruct m_StoredMiniMapRenderStruct;
};


// add a reference to the txd because a texture from it needs to stay resident till frame rendered
class CDrawRadioHudTextDC : public dlCmdBase{
public:
	enum {
		INSTRUCTION_ID = DC_DrawRadioHudText,
	};
	s32 GetCommandSize()  {return(sizeof(*this));}
	static void ExecuteStatic(dlCmdBase &cmd) { ((CDrawRadioHudTextDC &) cmd).Execute(); }
	void Execute();

	CDrawRadioHudTextDC(Vector2 pos[4], const grcTexture* pTex, Color32 col);

private:
	Vector2 m_pos[4];
	const grcTexture* m_pTex;
	Color32 m_col;
};


// add a reference to the txd because a texture from it needs to stay resident till frame rendered
class CRenderTextDC : public dlCmdBase{
public:
	enum {
		INSTRUCTION_ID = DC_RenderText,
	};
	s32 GetCommandSize()  {return(sizeof(*this));}
	static void ExecuteStatic(dlCmdBase &cmd) { ((CRenderTextDC &) cmd).Execute(); }
	void Execute();

	CRenderTextDC(CTextLayout Layout, Vector2 vPosition, char *pString);

private:
	CTextLayout m_TextLayout;
	Vector2 m_vPos;
	char m_cTextString[1024];
};



// draw a sprite
class CDrawSpriteDC : public dlCmdBase{
public:
	enum {
		INSTRUCTION_ID = DC_DrawSprite,
	};
	s32 GetCommandSize()  {return(sizeof(*this));}
	static void ExecuteStatic(dlCmdBase &cmd) { ((CDrawSpriteDC &) cmd).Execute(); }
	void Execute();

	CDrawSpriteDC(Vector2& v1, Vector2& v2, Vector2& v3, Vector2& v4, Color32, const grcTexture *pTex);

private:
	Vector2 m_pos[4];
	const grcTexture* m_pTex;
	Color32 m_col;
};

// draw a sprite with UV
class CDrawSpriteUVDC : public dlCmdBase{
public:
	enum {
		INSTRUCTION_ID = DC_DrawSpriteUV,
	};
	s32 GetCommandSize()  {return(sizeof(*this));}
	static void ExecuteStatic(dlCmdBase &cmd) { ((CDrawSpriteUVDC &) cmd).Execute(); }
	void Execute();

	CDrawSpriteUVDC(Vector2& v1, Vector2& v2, Vector2& v3, Vector2& v4, Vector2& vU1, Vector2& vU2, Vector2& vU3, Vector2& vU4, Color32 col, const grcTexture *pTex, s32 txdId = -1, grcBlendStateHandle blendState = grcStateBlock::BS_Default);

private:
	Vector2 m_pos[8];
	const grcTexture* m_pTex;
	Color32 m_col;
	grcBlendStateHandle m_blendState;
};
class CDrawUIWorldIcon : public dlCmdBase{
public:
	enum {
		INSTRUCTION_ID = DC_UIWorldIconIM,
	};
	s32 GetCommandSize()  {return(sizeof(*this));}
	static void ExecuteStatic(dlCmdBase &cmd) { ((CDrawUIWorldIcon &) cmd).Execute(); }
	void Execute();

	CDrawUIWorldIcon(Vector3& v1, Vector3& v2, Vector3& v3, Vector3& v4, Vector2& vU1, Vector2& vU2, Vector2& vU3, Vector2& vU4, Color32 col, const grcTexture *pTex, s32 txdId = -1, grcBlendStateHandle blendState = grcStateBlock::BS_Default);

private:
	Vector3 m_pos[4];
	Vector2 m_uv[4];
	const grcTexture* m_pTex;
	Color32 m_col;
	grcBlendStateHandle m_blendState;
};

// draw a sprite
class CDrawRectDC : public dlCmdBase{
public:
	enum {
		INSTRUCTION_ID = DC_DrawRect,
	};
	s32 GetCommandSize()  {return(sizeof(*this));}
	static void ExecuteStatic(dlCmdBase &cmd) { ((CDrawRectDC &) cmd).Execute(); }
	void Execute();

	CDrawRectDC(const fwRect& rect, Color32);

private:
	fwRect m_rect;
	Color32 m_col;
};

class CDrawPtxEffectInst : public dlCmdBase{

public:
	enum {
		INSTRUCTION_ID = DC_DrawPtxEffectInst,
	};
	CDrawPtxEffectInst(CPtFxSortedEntity *prtEntity BANK_ONLY(, bool debugRender));

	s32 GetCommandSize()	{return(sizeof(*this));}
	static void ExecuteStatic(dlCmdBase &cmd) { ((CDrawPtxEffectInst &) cmd).Execute(); }
	void Execute();
private:
	ptxEffectInst *m_effect;
#if __BANK
	bool		m_debugRender;
#endif // __BANK

public:
};

class CDrawVehicleGlassComponent : public dlCmdBase{

public:
	enum {
		INSTRUCTION_ID = DC_DrawVehicleGlassComponent,
	};
	CDrawVehicleGlassComponent(CVehicleGlassComponentEntity *vehGlassEntity BANK_ONLY(, bool debugRender));

	s32 GetCommandSize()	{return(sizeof(*this));}
	static void ExecuteStatic(dlCmdBase &cmd) { ((CDrawVehicleGlassComponent &) cmd).Execute(); }
	void Execute();
private:
	CVehicleGlassComponent *m_vehGlassComponent;
#if __BANK
	bool		m_debugRender;
#endif // __BANK

public:
};

class CSetDrawableLODCalcPos : public dlCmdBase{

public:
	enum {
		INSTRUCTION_ID = DC_SetDrawableLODCalcPos,
	};
	CSetDrawableLODCalcPos(const Vector3& pos);

	s32 GetCommandSize()	{return(sizeof(*this));}
	static void ExecuteStatic(dlCmdBase &cmd) { ((CSetDrawableLODCalcPos &) cmd).Execute(); }
	void Execute();

	static void		SetRenderLockCamPos(const Vector3& pos) {ms_RenderLockLODCalcPos = (ThreadVector3&)pos;}

private:
	Vector3	m_LODCalcPos;
	static	ThreadVector3 ms_RenderLockLODCalcPos;		// if enabling render lock, then LODs are calced from the original camera position

public:
};

#if DRAWABLE_STATS
class CSetDrawableStatContext : public dlCmdBase{
public:
	enum {
		INSTRUCTION_ID = DC_SetDrawableStatContext
	};
	CSetDrawableStatContext(u16 stat);

	s32 GetCommandSize()	{return(sizeof(*this));}
	static void ExecuteStatic(dlCmdBase &cmd) { ((CSetDrawableStatContext &) cmd).Execute(); }
	void Execute();

private:
	u16 m_DrawableStatContext;

public:
};
#endif // DRAWABLE_STATS

class dlCmdBeginOcclusionQuery : public dlCmdBase{
public:
	enum {
		INSTRUCTION_ID = DC_BeginOcclusionQuery,
	};

	dlCmdBeginOcclusionQuery(grcOcclusionQuery q);

	s32 GetCommandSize()	{return(sizeof(*this));}
	static void ExecuteStatic(dlCmdBase &cmd) { ((dlCmdBeginOcclusionQuery &) cmd).Execute(); }
	void Execute();
private:
	grcOcclusionQuery query;
};

class dlCmdEndOcclusionQuery : public dlCmdBase{
public:
	enum {
		INSTRUCTION_ID = DC_EndOcclusionQuery,
	};

	dlCmdEndOcclusionQuery(grcOcclusionQuery q);

	s32 GetCommandSize()	{return(sizeof(*this));}
	static void ExecuteStatic(dlCmdBase &cmd) { ((dlCmdEndOcclusionQuery &) cmd).Execute(); }
	void Execute();
private:
	grcOcclusionQuery query;
};

// skeleton storage
class dlCmdAddSkeleton : public dlCmdBase{

public:
	enum {
		INSTRUCTION_ID = DC_AddSkeleton,
	};

	dlCmdAddSkeleton(crSkeleton* pSourceSkel, eSkelMatrixMode skelMatMode);

	s32 GetCommandSize()  {return(sizeof(dlCmdAddSkeleton));}
	static void ExecuteStatic(dlCmdBase &cmd) { ((dlCmdAddSkeleton &) cmd).Execute(); }
	void Execute();

	static void ExecuteCore(u32 skelDataSize, s32 skelDrawListOffset);

	static	grmMatrixSet* GetCurrentMatrixSet() { return(ms_pCurrentMatSet); }

	static void SetStrippedHead(bool val) { ms_bStrippedHead = val; }
	static bool GetIsStrippedHead() { return ms_bStrippedHead; }

private:
	CAddSkeletonCommand		m_AddSkeleton;		// For the implicit dlCmdAddSkeleton call
	static DECLARE_MTR_THREAD grmMatrixSet*	ms_pCurrentMatSet;
	static DECLARE_MTR_THREAD bool				ms_bStrippedHead;
};

// skeleton storage
class dlCmdOverrideSkeleton : public dlCmdBase{

public:
	enum {
		INSTRUCTION_ID = DC_OverrideSkeleton,
	};

	dlCmdOverrideSkeleton(bool bDisableOverride = true);
	dlCmdOverrideSkeleton(grmMatrixSet* pMtxSetOverride, Mat34V_In rootMatOverride);
	dlCmdOverrideSkeleton(Mat34V_In rootMatOverride);

	s32 GetCommandSize()  {return(sizeof(dlCmdOverrideSkeleton));}
	static void ExecuteStatic(dlCmdBase &cmd) { ((dlCmdOverrideSkeleton &) cmd).Execute(); }
	void Execute();

private:
	Mat34V			m_rootMtxOverride;
	grmMatrixSet*	m_mtxSetOverride;
	bool			m_bOnlyOverrideRootMtx;
};

class dlCmdAddCompositeSkeleton : public dlCmdBase{

public:
	enum {
		INSTRUCTION_ID = DC_AddCompositeSkeleton,
	};

	dlCmdAddCompositeSkeleton();

	s32 GetCommandSize()  {return(sizeof(dlCmdAddCompositeSkeleton));}
	static void ExecuteStatic(dlCmdBase &cmd) { ((dlCmdAddCompositeSkeleton &) cmd).Execute(0); }
	void Execute(u32 idx);

	static void ExecuteCore(u32 skelDataSize, s32 skelDrawListOffset, u8 numHeadBones);

	static grmMatrixSet* GetCurrentMatrixSet() { return(ms_pCurrentMatSet); }

	static void SetCurrentMatrixSetOverride(grmMatrixSet* inMtxSet, Mat34V_In rootMtxIn) { ms_pCurrentMatSetOverride = inMtxSet; ms_rootMatrixOverride = (ThreadMat34V&)rootMtxIn; ms_bOnlyOverrideRootMtx = false; } 
	static void SetCurrentRootMatrixOverride(Mat34V_In rootMtxIn) { ms_rootMatrixOverride = (ThreadMat34V&)rootMtxIn; ms_bOnlyOverrideRootMtx = true; } 

	static bool IsCurrentMatrixSetOverriden() { return ms_pCurrentMatSetOverride != NULL; }
	static bool IsCurrentRootMatrixOverriden() { return ms_bOnlyOverrideRootMtx; }
	static void GetCurrentMatrixSetOverride(const grmMatrixSet*& pMtxSet, Mat34V_Ref rootMtx) { pMtxSet = ms_pCurrentMatSetOverride; rootMtx = (Mat34V&)ms_rootMatrixOverride; } 
	static void GetCurrentRootMatrixOverride(Mat34V_Ref rootMtx) { rootMtx = (Mat34V&)ms_rootMatrixOverride; } 

	static void SetStrippedHead(bool val) { ms_bStrippedHead = val; }
	static bool GetIsStrippedHead() { return ms_bStrippedHead; }


	static u8 GetNumHeadBones() { return ms_numHeadBones; }

private:
	CAddCompositeSkeletonCommand m_AddCompositeSkeleton;		// For the implicit dlCmdAddSkeleton call
	static DECLARE_MTR_THREAD grmMatrixSet*	ms_pCurrentMatSet;
	static DECLARE_MTR_THREAD grmMatrixSet*	ms_pCurrentMatSetOverride;
	static DECLARE_MTR_THREAD ThreadMat34V	ms_rootMatrixOverride;
	static DECLARE_MTR_THREAD u8				ms_numHeadBones;
	static DECLARE_MTR_THREAD bool			ms_bOnlyOverrideRootMtx;
	static DECLARE_MTR_THREAD bool			ms_bStrippedHead;
};

// -- drawing object commands --

class dlCmdDrawTwoSidedDrawable : public dlCmdBase{
public:
	enum {
		INSTRUCTION_ID = DC_DrawDrawable,
	};

	dlCmdDrawTwoSidedDrawable(rmcDrawable *drawable, Vector3& vOffset, s32 modelInfoIdx, u32 alphaFade, u32 naturalAmb, u32 artificialAmb, u32 matID, bool inInterior, grcRasterizerStateHandle rsHandle, grcDepthStencilStateHandle dssHandle, bool captureStats, bool shadowWithShaders=false, bool bIsVehicleCloth=false, fwEntity* pEntityForId=NULL);

	s32 GetCommandSize()	{return(sizeof(*this));}
	static void ExecuteStatic(dlCmdBase &cmd) { ((dlCmdDrawTwoSidedDrawable &) cmd).Execute(); }
	void Execute();

private:
	void UpdateVehicleCloth();

	Vector3						m_offset;

	rmcDrawable*				m_drawable;

	u8							m_fadeAlpha;
	u8							m_naturalAmb;
	u8							m_matID;
	u8							m_artificialAmb;

	grcRasterizerStateHandle	m_rsState;
	grcDepthStencilStateHandle	m_dssState;

#if HACK_GTA4_MODELINFOIDX_ON_SPU
	u16							m_modelInfoIdx;
#endif // HACK_GTA4_MODELINFOIDX_ON_SPU	

#if __BANK
	u8							m_captureStats	:1;
	ATTR_UNUSED u8				m_pad			:7;
	entitySelectID				m_entityId;
#endif // __BANK

	u8							m_bShadowWithShaders:1;
	u8							m_bIsVehicleCloth	:1;
	u8							m_bIsInInterior		:1;
	ATTR_UNUSED u8				m_bPad06			:5;
};

// --- inline functions --------------------------------------------------------------------------------------------

#endif // INC_DRAWLIST_H_
