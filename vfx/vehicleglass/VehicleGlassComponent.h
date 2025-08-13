// ========================================
// vfx/vehicleglass/VehicleGlassComponent.h
// (c) 2012-2013 Rockstar Games
// ========================================

#ifndef VEHICLEGLASSCOMPONENT_H
#define VEHICLEGLASSCOMPONENT_H

#include "vfx/vehicleglass/VehicleGlass.h"

// framework
#include "vfx/vfxlist.h"
#include "vfx/vehicleglass/vehicleglasswindow.h"

// game
#include "audio/glassaudioentity.h"
#include "renderer/HierarchyIds.h"
#include "scene/Physical.h"
#include "scene/RegdRefTypes.h"
#include "shaders/CustomShaderEffectVehicle.h"
#include "vfx/VfxHelper.h"

#define VEHICLE_GLASS_CLUMP_NOISE                   (1 && __DEV)
#define VEHICLE_GLASS_RANDOM_DETACH                 (1 && __DEV)
#define VEHICLE_GLASS_DEBUG_COMPARE_CORRECT_CALCS   (0 && __DEV)

namespace rage { class grmModel; }
namespace rage { class rmcDrawable; }

//Forward Declarations
class CVehicleGlassComponent;
struct VfxCollInfo_s;
struct VfxExpInfo_s;

// typesafe vfxList
template <typename T> class vfxListT : public vfxList
{
public:
	inline T* RemoveHead() { return (T*)vfxList::RemoveHead(); }
	inline T* RemoveHeadPrefetchNext() { return (T*)vfxList::RemoveHeadPrefetchNext(); }
	inline T* GetHead() { return (T*)vfxList::GetHead(); }
	inline const T* GetHead() const { return (const T*)vfxList::GetHead(); }
	inline T* GetNext(const T* item) { return (T*)vfxList::GetNext(const_cast<T*>(item)); }
	inline const T* GetNext(const T* item) const { return (const T*)vfxList::GetNext(item); }
	inline T* GetFromPool(vfxListT<T>* pool) { T* item = pool->RemoveHeadPrefetchNext(); if (item) { vfxList::AddItem(item); } return item; }
	inline void ReturnToPool(T* item, vfxListT<T>* pool) { vfxList::RemoveItem(item); pool->AddItem(item); }
	inline T* ReturnToPool_GetNext(T* item, vfxListT<T>* pool) { T* next = GetNext(item); vfxList::RemoveItem(item); pool->AddItem(item); return next; }
};

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4324)   // warning C4324: 'vfxPoolT<T,N>' : structure was padded due to __declspec(align())
#endif

template <typename T, int N> class vfxPoolT : public vfxListT<T>
{
public:
	inline void Init() { for (int i = 0; i < N; i++) { vfxListT<T>::AddItem(&m_items[i]); } }
	inline void Shutdown() { vfxListT<T>::RemoveAll(); }
	inline T& operator [](int index) { return m_items[index]; }
	inline const T& operator [](int index) const { return m_items[index]; }
private:
	T m_items[N];
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#if VEHICLE_GLASS_COMPRESSED_VERTICES
// When VEHICLE_GLASS_COMPRESSED_VERTICES is enabled, CVehicleGlassTriangle is
// 128 bytes.  So aligning to 128 forces it into one cache line on PS3 & 360,
// and two cache lines on Durango, Orbis and most PCs.
#define CVEHICLEGLASSTRIANGLE_ALIGN     128
#else
// Otherwise just use minimum alignment required for a vector.
#define CVEHICLEGLASSTRIANGLE_ALIGN     16
#endif

class ALIGNAS(CVEHICLEGLASSTRIANGLE_ALIGN) CVehicleGlassTriangle : public vfxListItem
{
public:
	__forceinline void SetVertexP(int i, Vec3V_In position); // forces damage to zero
	__forceinline void SetVertexD(int i, Vec3V_In damage);
	__forceinline void SetVertexDZero(int i); // set damage to zero
	__forceinline void SetVertexTNC(int i, Vec4V_In texcoord, Vec3V_In normal, Vec4V_In colour);
	__forceinline void SetVelocity(Vec3V_In velocity);
	__forceinline void SetSpinAxis(Vec3V_In axis);

	__forceinline void UpdatePositionsAndNormals(Vec3V_In p0, Vec3V_In p1, Vec3V_In p2);

	__forceinline Vec3V_Out GetVertexP(int i) const;
	__forceinline Vec3V_Out GetDamagedVertexP(int i) const;
	__forceinline void      GetVertexTNC(int i, Vec4V_InOut texcoord, Vec3V_InOut normal, Vec4V_InOut colour) const;
	__forceinline Vec4V_Out GetVertexT(int i) const;
	__forceinline Vec3V_Out GetVertexN(int i) const;
	__forceinline Vec4V_Out GetVertexC(int i) const;
	__forceinline Vec3V_Out GetVelocity() const;
	__forceinline Vec3V_Out GetSpinAxis() const;

#if __BANK && VEHICLE_GLASS_COMPRESSED_VERTICES
	__forceinline Vec4V_Out GetCompressedVertexPD_debug(int i) const { return m_vertexPD[i]; }
	__forceinline Vec4V_Out GetCompressedVertexTNC_debug(int i) const { return m_vertexTNC[i]; }
#endif // __BANK && VEHICLE_GLASS_COMPRESSED_VERTICES

	bool    m_bTessellated : 1;
	bool    m_bCanDetach   : 1;
#if __BANK
	bool    m_bHasError    : 1;
#endif // __BANK

private:
#if VEHICLE_GLASS_COMPRESSED_VERTICES // compressed vertices brings this class down from 208 bytes to 128 bytes
	Vec4V   m_vertexPD[3]; // xyz=position, w=damage(8.8.8.x)
	Vec4V   m_vertexTNC[3]; // xy=texcoord(16.16.16.16), z=normal(8.8.8.x), w=colour(8.x.x.8) (note that colour only uses r,a channels in the shader)
	Vec4V   m_velocityAndSpinAxis; // xyz=velocity, w=axis(8.8.8.x)
#else
	Vec3V   m_vertexP[3]; // if attached then this is stored in 'default' space (as in the vertex buffer) otherwise it's 'world' space when detached
	Vec3V   m_vertexD[3];
	Vec4V   m_vertexT[3];
	Vec3V   m_vertexN[3];
	Color32 m_vertexC[3];
	Vec3V   m_velocity;
	Vec3V   m_spinAxis;
#endif
};

#if VEHICLE_GLASS_COMPRESSED_VERTICES
// If the size increases over 128, then we are probably wasting a lot of memory
// due to the 128-byte alignment.  If the size increase is necissary, need to
// revisit the alignment that is specified.
CompileTimeAssert(sizeof(CVehicleGlassTriangle) == 128);
#endif


#if VEHICLE_GLASS_COMPRESSED_VERTICES

__forceinline Vec3V_Out CVehicleGlassTriangle::GetVertexP(int i) const
{
	return m_vertexPD[i].GetXYZ();
}

__forceinline Vec3V_Out CVehicleGlassTriangle::GetDamagedVertexP(int i) const
{
	Vec4V temp = m_vertexPD[i];

	temp = Vec4V(Vec::V4UnpackHighSignedByte (temp.GetIntrin128()));
	temp = Vec4V(Vec::V4UnpackHighSignedShort(temp.GetIntrin128()));

	const Vec4V dmg = IntToFloatRaw<8>(temp); // [-0.5..0.5)

	return (m_vertexPD[i] + dmg).GetXYZ();
}

__forceinline Vec3V_Out CVehicleGlassTriangle::GetVertexN(int i) const
{
	const Vec4V temp = m_vertexTNC[i];
	const Vec4V nc   = MergeZWByte(temp, temp); // [0..65535]
#if RSG_CPU_INTEL
	const Vec3V nrm  = IntToFloatRaw<16>(MergeXYShort(nc, Vec4V(V_ZERO)).GetXYZ());
#else
	const Vec3V nrm  = IntToFloatRaw<16>(MergeXYShort(Vec4V(V_ZERO), nc).GetXYZ());
#endif
	return AddScaled(Vec3V(V_NEGONE), Vec3V(V_TWO), nrm); // [-1..1]
}

#else // not VEHICLE_GLASS_COMPRESSED_VERTICES

__forceinline Vec3V_Out CVehicleGlassTriangle::GetVertexP(int i) const
{
	return m_vertexP[i];
}

__forceinline Vec3V_Out CVehicleGlassTriangle::GetDamagedVertexP(int i) const
{
	return m_vertexP[i] + m_vertexD[i];
}

__forceinline Vec3V_Out CVehicleGlassTriangle::GetVertexN(int i) const
{
	return m_vertexN[i];
}

#endif // not VEHICLE_GLASS_COMPRESSED_VERTICES

#if VEHICLE_GLASS_COMPRESSED_VERTICES && __PS3
CompileTimeAssert(sizeof(CVehicleGlassTriangle) == 128);
#endif // VEHICLE_GLASS_COMPRESSED_VERTICES && __PS3

class CVGStackTriangle
{
public:
	__forceinline void SetVertexP(int i, Vec3V_In position); // forces damage to zero
	__forceinline void SetVertexD(int i, Vec3V_In damage);
	__forceinline void SetVertexDZero(int i); // set damage to zero
	__forceinline void SetVertexTNC(int i, Vec4V_In texcoord, Vec3V_In normal, Vec4V_In colour);
	__forceinline void SetFlags(bool bTessellated, bool bCanDetach BANK_ONLY(, bool bHasError));
	__forceinline void SetDetached();

#if !VEHICLE_GLASS_COMPRESSED_STACK_TRIANGLES
	__forceinline void SetVertexPC(int i, Vec4V_In positionXYZColourW); // forces damage to zero
	__forceinline void SetVertexTNF(int i, Vec4V_In texcoord, Vec4V_In normalXYZFlagsW);
#endif // !VEHICLE_GLASS_COMPRESSED_STACK_TRIANGLES

	__forceinline Vec3V_Out GetVertexP(int i) const;
	__forceinline void GetVertexTNC(int i, Vec4V_InOut texcoord, Vec3V_InOut normal, Vec4V_InOut colour);
	__forceinline bool Tessellated() const;
	__forceinline bool CanDetach() const;
	__forceinline bool Detached() const;
#if __BANK
	__forceinline bool HasError() const;
#endif // __BANK

#if !VEHICLE_GLASS_COMPRESSED_STACK_TRIANGLES
	enum TF
	{
		TF_Tessellated = 1,
		TF_CanDetach = 2,
		TF_Detached = 4
		BANK_ONLY(,TF_HasError = 8)
	};
#endif // !VEHICLE_GLASS_COMPRESSED_STACK_TRIANGLES

private:
#if VEHICLE_GLASS_COMPRESSED_STACK_TRIANGLES
	Float16Vec4 m_vertexP[3];
	Float16Vec4 m_vertexT[3];
	Float16Vec4 m_vertexN[3];
	u32         m_vertexC[3];

	bool m_bTessellated : 1;
	bool m_bCanDetach   : 1;
	bool m_bDetached    : 1;
#if __BANK
	bool m_bHasError    : 1;
#endif // __BANK

#else
	Vec4V m_vertexPC[3]; // Position in XYZ with colour in W
	Vec4V m_vertexT[3];
	Vec4V m_vertexNF[3]; // Normal in XYZ and flags in W
#endif // VEHICLE_GLASS_COMPRESSED_STACK_TRIANGLES
};

// Helper class for managing the attached triangles static vertex buffer
// This class handles the allocation and the deletion cool down
class CVehicleGlassVB : public vfxListItem
{
public:
	static CVehicleGlassVB* Create(); // Get a new VB
	static void Destroy(CVehicleGlassVB* pVB, bool bImmediate = false); // Mark a VB as not being used by the component (starts the cooldown if bImmediate == false)
	static void Update(); // Update handles the cooldown and garbage collection

	bool AllocateBuffer(int numVerts);
	bool AllocateVB();

	static void CreateFvf(grcFvf *pFvf);
#if RSG_PC
	void DestroyVB();
	bool UpdateVB();
#endif

	Vec3V_Out GetVertexP(int idx) const;
	Vec3V_Out GetDamagedVertexP(int idx) const;
	Vec3V_Out GetVertexN(int idx) const;
	void GetVertexTNC(int idx, Vec4V_InOut tex, Vec3V_InOut nrm, Vec4V_InOut col) const;
	void GetFlags(int idx, bool& bTessellated, bool& bCanDetach BANK_ONLY(, bool& bHasError)) const;
	void SetVertex(int idx, Vec3V_In pos, Vec3V_In dmgPos, ScalarV_In dmgMult, Vec3V_In nrm, Vec4V_In tex, Vec4V_In col, bool bTessellated, bool bCanDetach BANK_ONLY(, bool bHasError));
	int GetVertexCount() const { return m_vertexCount; }
	const void *GetRawVertices() const;
	unsigned GetVertexStride() const;
	void BindVB();

#if __BANK
	static int GetMemoryUsed() { return ms_MemoryUsed; }
#endif // __BANK

private:
	// Flag bits definition
	// All the flags are stored in the green channel of the vertex colour
	enum TF
	{
		TF_Tessellated = 1,
		TF_CanDetach = 2
		BANK_ONLY(,TF_HasError = 8)
	};

	static grcFvf                m_fvf;
	static grcVertexDeclaration* m_vtxStaticDecl;

	int              m_vertexCount;
	void*            m_pVBPreBuffer;
	grcVertexBuffer* m_pVertexBuffer; // Pointer to the actual vertex buffer
#if RSG_PC
	int				 m_BGvtxBufferIndex;
#endif
	u32              m_lastRendered; // This helps with the cooldown before deletion (avoid deleting stuff set to be rendered)

	// Total memory allocated for VB
	// This is used to cap memory usage during MP and to track total memory allocation in general
	static int ms_MemoryUsed;

	friend class CVehicleGlassManager;
};


///////////////////////////////////////////////////////////////////////////////
//  CVehicleGlassComponentEntity
///////////////////////////////////////////////////////////////////////////////

class CVehicleGlassComponentEntity : public CEntity
{	
	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

public: ///////////////////////////

	FW_REGISTER_CLASS_POOL(CVehicleGlassComponentEntity);

	CVehicleGlassComponentEntity						(const eEntityOwnedBy ownedBy, CVehicleGlassComponent* pGlassComponent);
	~CVehicleGlassComponentEntity						()								{ delete m_pDrawHandler; m_pDrawHandler = NULL; };

	virtual FASTRETURNCHECK(const spdAABB &) GetBoundBox(spdAABB& box) const;

	void					SetDrawHandler				(fwDrawData *drawdata)			{ m_pDrawHandler = drawdata; }
	virtual fwDrawData*		AllocateDrawHandler			(rmcDrawable* pDrawable);

	void					SetVehicleGlassComponent	(CVehicleGlassComponent* pGlassComponent);
	CVehicleGlassComponent*	GetVehicleGlassComponent	()								{return m_pVehGlassComponent;}

	inline	void			AddRef						()								{m_refCount++;}
	inline	bool			RemoveRef					()								{m_refCount--; return m_refCount<1;}
	inline	s32				GetRef						()								{return m_refCount;}


protected: ////////////////////////

	CVehicleGlassComponentEntity	(const eEntityOwnedBy ownedBy)	: CEntity(ownedBy)	{}


	///////////////////////////////////
	// VARIABLES 
	///////////////////////////////////		

protected: ////////////////////////

	CVehicleGlassComponent*		m_pVehGlassComponent;
	s32							m_refCount;

};

class CVehicleGlassComponent : public vfxListItem
{
public:
	inline const CVehicleGlassVB* GetStaticVB() const { return m_pAttachedVB; } // used by decal manager
	inline const CPhysical* GetPhysical() const { return m_regdPhy.Get(); } // used by smashableaudioentity
	inline phMaterialMgr::Id GetMaterialId() const { return m_materialId; } // used by smashableaudioentity

	float GetArmouredGlassHealth() const { return m_fArmouredGlassHealth; }
	void  SetArmouredGlassHealth(float health)						 { m_fArmouredGlassHealth = health; }
	u8    GetArmouredGlassPenetrationDecalsCount() const		     { return m_armouredGlassPenetrationDecalCount; }
	void  SetArmouredGlassPenetrationDecalsCount(u8 decals)			 { m_armouredGlassPenetrationDecalCount = decals; }
	
	void ApplyArmouredGlassDecals(CVehicle* pVehicle);

	// Cloud tunables
	static void InitTunables(); 
	
	static float MAX_ARMOURED_GLASS_HEALTH;
	static u8    MAX_DECALS_CLONED_PER_WINDOW;
private:
	enum
	{
		VEHICLE_GLASS_TEST_1_VERTEX,
		VEHICLE_GLASS_TEST_2_VERTICES,
		VEHICLE_GLASS_TEST_3_VERTICES,
		VEHICLE_GLASS_TEST_CENTROID,
		VEHICLE_GLASS_TEST_TRIANGLE,
		VEHICLE_GLASS_TEST_COUNT,

		VEHICLE_GLASS_TEST_DEFAULT = VEHICLE_GLASS_TEST_CENTROID
	};

	static void StaticInit();
	static void StaticShutdown();

#if RSG_PC
	static void	CreateStaticVertexBuffers();
	static void	DestroyStaticVertexBuffers();
#endif // RSGPC

	void InitComponent(CPhysical* pPhysical, int componentId, phMaterialMgr::Id mtlId);
	void ShutdownComponent();
	void ShutdownComponentGroup();
	void ClearDetached();

	bool GetVehicleDamageDeformation(const CPhysical* pVehicle, Vec3V* damageBuffer) const;

	bool UpdateComponent(float deltaTime);
	void RenderComponent();

	void RenderTriangleList(const vfxListT<CVehicleGlassTriangle>& triList) const;

	// Set render states common to all components belonging to the same entity
	void SetCommonEntityStates() const;

	const spdAABB& GetBoundBox() const { return m_lightsBoundBox; }
	inline void SetVisible() { m_isVisible[gRenderThreadInterface.GetUpdateBuffer()] = true; }

	bool IsVisible() const;
	bool CanRender() const;

	void GetGroundInfo(const CPhysical* pPhysical, Vec4V_InOut groundPlane, bool& touchingGround, Vec3V* pHitPos = NULL) const;
	void SetComponentEntity(CVehicleGlassComponentEntity* pCompEntity)	{m_pCompEntity = pCompEntity; if(m_pCompEntity != NULL) {m_pCompEntity->AddRef();}}
#if __BANK
	const char* GetModelName() const;
	void RenderComponentDebug() const;
	void RenderComponentDebugTriangle(Vec3V_In vCamPos, const Vec3V pos[3], const Vec3V norm[3], const bool exposed[3], bool bTessellated, bool bCanDetach, bool bHasError, bool bAttached) const;
#endif // __BANK

	int ProcessComponentCollision(const VfxCollInfo_s& info, bool bSmash, bool bProcessDetached);
	int ProcessComponentExplosion(const VfxExpInfo_s& info, bool bSmash, bool bProcessDetached);

	ScalarV_Out ProcessTessellation_GetSmashRadius(BANK_ONLY(Vec3V_In p)) const;
	int         ProcessTessellation_TestPointInSphere(Vec3V_In p) const;
	bool        ProcessTessellation_TestTriangleInSphere(Vec3V_In P0, Vec3V_In P1, Vec3V_In P2, int BANK_ONLY(numVertsToTest)) const;
	Vec3V_Out   ProcessTessellation(const CPhysical* pPhysical, const VfxCollInfo_s* pCollisionInfo, float tessellationScale, bool bForceSmashDetach, bool bProcessDetached, int& numInstantBreakPieces);

	void InitComponentGeom(const grmGeometry& geom, int boneIndex);

	int GetNumTriangles(bool bOnlyAttachedTriangles) const;

	// ============================================

	const fwVehicleGlassWindow*      GetWindow     (const CPhysical* pPhysical) const;
	const fwVehicleGlassWindowProxy* GetWindowProxy(const CPhysical* pPhysical) const;

	void ProcessGroupCollision(const VfxCollInfo_s& info, bool bSmash, bool bProcessDetached);
	void ProcessGroupExplosion(const VfxExpInfo_s& info, bool bSmash, bool bProcessDetached);

#if __DEV
	bool ProcessVehicleGlassCrackTestComponent(const VfxCollInfo_s& info); // implemented in VehicleGlassSmashTest.cpp
#endif // __DEV

	void AddTriangle(
		CVGStackTriangle* triangleArray,
		u32& nextFreeTriangle,
		Vec3V_In P0, Vec3V_In P1, Vec3V_In P2,
		Vec4V_In T0, Vec4V_In T1, Vec4V_In T2,
		Vec3V_In N0, Vec3V_In N1, Vec3V_In N2,
		Vec4V_In C0, Vec4V_In C1, Vec4V_In C2
	);

	struct CVGTmpVertex;

	void AddTriangleTessellated(
		CVGStackTriangle* triangleArray,
		u32& nextFreeTriangle,
		Mat34V_In distFieldBasis,
		Vec3V_In smashCenter,
		ScalarV_In smashRadius,
		u32 tessellationScale,
		Vec3V_In faceNormal,
		Vec3V_In P0, Vec3V_In P1, Vec3V_In P2,
		Vec4V_In T0, Vec4V_In T1, Vec4V_In T2,
		Vec3V_In N0, Vec3V_In N1, Vec3V_In N2,
#if __XENON
		// Microsoft (R) 32-bit C/C++ Optimizing Compiler Version 16.00.11886.00 for PowerPC
		// Appears to have a bug passing more than 16 vector registers by value.
		// So make the 17th and 18th pass by const reference instead.
		Vec4V_In C0, const Vec4V& C1, const Vec4V& C2
#else
		Vec4V_In C0, Vec4V_In C1, Vec4V_In C2
#endif
	);

	void RecursiveTessellateNoSphereTest(
		u8** outEdges,
		u32** outRatios,
		unsigned remainingRecursionAllowed,
		u32 len01,
		u32 len12,
		u32 len20,
		u32 area,
		u32 tessellationScale
#if VEHICLE_GLASS_DEBUG_COMPARE_CORRECT_CALCS
		, float fLen01
		, float fLen12
		, float fLen20
		, float fArea
#endif
	);

	void RecursiveTessellateWithSphereTest(
		u8** outEdges,
		u32** outRatios,
		unsigned remainingRecursionAllowed,
		u32 len01,
		u32 len12,
		u32 len20,
		u32 area,
		u32 tessellationScale,
		Vec2f_In vtx0,
		Vec2f_In vtx1,
		Vec2f_In vtx2,
		float radSq
#if VEHICLE_GLASS_DEBUG_COMPARE_CORRECT_CALCS
		, float fLen01
		, float fLen12
		, float fLen20
		, float fArea
#endif
	);

	void CreateTempVertsFromTessellation(
		CVGTmpVertex* tmpVtxs,
		u16* tmpIdxs,
		u8* tessFlags,
		unsigned* numVtxs,
		unsigned* numIdxs,
		const u8* edges,
		const u32* ratios);

	void CheckDistanceField(
		CVGTmpVertex* tmpVtxs,
		unsigned numVtxs,
		const fwVehicleGlassWindowProxy* pProxy,
		float distThresh);

	void StoreStackTris(
		CVGStackTriangle* outTris,
		const CVGTmpVertex* tmpVtxs,
		const u16* tmpIdxs,
		const bool* tessFlags,
		unsigned numTris,
		u32 detachThresh);

	void UpdateVB(const CVGStackTriangle* triangleArray, u32 nextFreeTriangle); // Update the VB with the current data in the stack triangles

	void UpdateVBDamage(const Vec3V* damageArray);

	void UpdateSmashSphere(Vec3V_In vSmashPosObj, float radius);

#if VEHICLE_GLASS_SMASH_TEST
	Vec3V m_smashTestMaxTriangleErrorP;
	Vec4V m_smashTestMaxTriangleErrorT;
	Vec3V m_smashTestMaxTriangleErrorN;
	Vec4V m_smashTestMaxTriangleErrorC;
#endif // VEHICLE_GLASS_SMASH_TEST

	vfxListT<CVehicleGlassTriangle> m_triangleListDetached; // List with the detached triangles
	const fwVehicleGlassWindow*     m_pWindow;

	CVehicleGlassVB*        m_pAttachedVB; // Render VB for the attached glass
	CVehicleGlassComponentEntity* m_pCompEntity; //Pointer to corresponding proxy entity (for shutdown purposes)

	RegdPhy                 m_regdPhy;
	strLocalIndex           m_modelIndex;
	int                     m_componentId;
	eHierarchyId            m_hierarchyId; // only applies to standard windows
	phMaterialMgr::Id       m_materialId;
	int                     m_geomIndex;
	int                     m_boneIndex;
	unsigned int			m_readytoRenderFrame;
	bool                    m_isValid            : 1;
	bool                    m_cracked            : 1;
	bool                    m_smashed            : 1;
	bool                    m_damageDirty        : 1; // requires damage update
	bool                    m_removeComponent    : 1;
	bool                    m_forceSmash         : 1;
	bool                    m_forceSmashDetach   : 1;
	bool                    m_nothingToDetach    : 1; // Indicates that all triangles left can't detach or tessellate anymore
#if __BANK
	bool                    m_updateTessellation : 1;
#endif // __BANK
	bool                    m_isVisible[2];

#if VEHICLE_GLASS_CLUMP_NOISE
	u16                     m_clumpNoiseBufferIndex;
	u16                     m_clumpNoiseBufferOffset;
#endif // VEHICLE_GLASS_CLUMP_NOISE

	Mat34V                  m_matrix;
	Vec4V                   m_smashSphere; // xyz=centre, w=radius
	Vec3V                   m_centroid; // used for sorting back-to-front
	Vec3V					m_boundsMin;        
	Vec3V					m_boundsMax;
	float                   m_distToViewportSqr;
	float                   m_renderGlobalAlpha;
	u8						m_renderGlobalNaturalAmbientScale;
	u8						m_renderGlobalArtificialAmbientScale;
	u16						m_padding;

	int                     m_numDecals;
	u32                     m_lastFxTime;
	audSmashableGlassEvent  m_audioEvent;
	float                   m_crackTextureScale;

	CVehicleGlassShaderData m_shaderData;

	float                   m_triAreaThreshMin;
	float                   m_triAreaThreshMax;

	float					m_fArmouredGlassHealth;
	u8                      m_armouredGlassPenetrationDecalCount;
	// Bounding box used for gathering the closest lights for forward rendering
	spdAABB                 m_lightsBoundBox;

#if __BANK
	// List with the explosion smash sphere history
	atSList<Vec4V> m_listExplosionHistory;
#endif // __BANK

	// Cloud tunables
	static s32	 sm_iSpecialAmmoFMJVehicleBulletproofGlassMinHitsToSmash;
	static s32	 sm_iSpecialAmmoFMJVehicleBulletproofGlassMaxHitsToSmash;
	static float sm_fSpecialAmmoFMJVehicleBulletproofGlassRandomChanceToSmash;

	friend class CVehicleGlassSmashTestManager;
	friend class CVehicleGlassManager;
	friend class CVehicleGlassComponentEntity;
};

#endif // VEHICLEGLASSCOMPONENT_H