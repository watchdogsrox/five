// ==========================================
// vfx/vehicleglass/VehicleGlassComponent.cpp
// (c) 2012-2013 Rockstar Games
// ==========================================

#include "vfx/vehicleglass/VehicleGlass.h"

// rage
#include "file/stream.h"
#include "grcore/edgeExtractgeomspu.h"
#include "grcore/effect.h"
#include "grcore/grcorespu.h"
#include "grcore/indexbuffer.h"
#include "grcore/vertexbuffer.h"
#include "grcore/vertexbuffereditor.h"
#include "grmodel/geometry.h"
#include "math/float16.h"
#include "math/vecrand.h"
#include "paging/handle.h"
#include "phbound/boundbvh.h"
#include "phbound/boundcomposite.h"
#include "phBound/support.h"
#include "system/memops.h"
#include "system/typeinfo.h"
#include "system/xtl.h"
#include "vector/geometry.h" // for geomSpheres::TestSphereToTriangle

// framework
#include "fwtl/customvertexpushbuffer.h"
#include "fwmaths/vectorutil.h"
#include "fwsys/glue.h" // [VEHICLE DAMAGE]
#include "vfx/vehicleglass/vehicleglassconstructor.h"
#include "vfx/vehicleglass/vehicleglasswindow.h"

// game
#include "camera/CamInterface.h"
#include "Network/General/NetworkUtil.h"
#include "Network/NetworkInterface.h"
#include "Network/Objects/Entities/NetObjVehicle.h"
#include "Objects/object.h"
#include "physics/gtaArchetype.h"
#include "physics/gtaMaterialManager.h"
#include "physics/physics.h"
#include "physics/WorldProbe/worldprobe.h"
#include "renderer/Entities/VehicleGlassDrawHandler.h"
#include "renderer/lights/lights.h"
#include "scene/Entity.h"
#include "shaders/CustomShaderEffectVehicle.h"
#include "Vehicles/vehicle.h"
#include "Vehicles/Planes.h"
#include "Vehicles/vehicleDamage.h" // [VEHICLE DAMAGE]
#include "vfx/vfx_channel.h"
#include "vfx/decals/DecalCallbacks.h" // [VEHICLE DAMAGE]
#include "vfx/decals/DecalManager.h"
#include "vfx/particles/PtFxManager.h"
#include "vfx/systems/VfxGlass.h"
#include "vfx/systems/VfxWeapon.h"
#include "vfx/vehicleglass/VehicleGlassComponent.h"
#include "vfx/vehicleglass/VehicleGlassManager.h"
#include "vfx/vehicleglass/VehicleGlassSmashTest.h"
#include "vfx/vehicleglass/VehicleGlassVertex.h"

#include <algorithm> // for std::swap

VEHICLE_GLASS_OPTIMISATIONS()
//OPTIMISATIONS_OFF()

#if VEHICLE_GLASS_DEV
	#define VEHICLE_GLASS_DEV_ONLY(x) x
	#define VEHICLE_GLASS_DEV_SWITCH(_if_,_else_) _if_
#else
	#define VEHICLE_GLASS_DEV_ONLY(x)
	#define VEHICLE_GLASS_DEV_SWITCH(_if_,_else_) _else_
#endif

XPARAM(vgasserts);

#define VG_VTX_CNT					(16384U)

#define VG_TRI_AREA_THRESH_MIN      (0.0006f BANK_ONLY(/g_vehicleGlassMan.m_tessellationScaleMin))
#define VG_TRI_AREA_THRESH_MAX      (0.0008f BANK_ONLY(/g_vehicleGlassMan.m_tessellationScaleMax))

#define VG_TESS_RATIO_RANGE         (0.2f)
#define VG_TESS_RATIO_MIN           (0.5f-VG_TESS_RATIO_RANGE*0.5f)

enum
{
	VG_TESS_FIXED_POINT                     = 24,

	VG_MAX_TESSELLATION_RECURSION_DEPTH     = 11,

	// Edge choices includes recursion termination indicators
	VG_MAX_TESSELLATION_NUM_EDGE_CHOICES    = 1<<VG_MAX_TESSELLATION_RECURSION_DEPTH,

	// Number of ratios does not, it is the actual number of edges that get split
	VG_MAX_TESSELLATION_NUM_RATIOS          = VG_MAX_TESSELLATION_NUM_EDGE_CHOICES>>1,

	VG_MAX_TESSELLATION_NUM_TEMP_VTXS       = 3+VG_MAX_TESSELLATION_NUM_RATIOS,

	VG_MAX_TESSELLATION_NUM_TRIS            = 1+VG_MAX_TESSELLATION_NUM_RATIOS,

	VG_MAX_TESSELLATION_NUM_TEMP_IDXS       = VG_MAX_TESSELLATION_NUM_TRIS*3,


	// Mark output edge as 0xfe instead of the usual 0xff so that the reason for
	// stopping tessellation can be distinguished from the case of the triangle
	// becoming too small.  The lsb of this value is then used as the
	// tessellation flag.
	VG_EDGE_STOP_TESS                       = 0xfe,
	VG_EDGE_STOP_TESS_OUTSIDE_SPHERE        = 0xfe,
	VG_EDGE_STOP_TESS_SIZE                  = 0xff,
};


PS3_ONLY(namespace rage { extern u32 g_AllowVertexBufferVramLocks; })

#if RSG_PC
#define MAX_VERT_COUNT 3000//2400
#define MAX_BROKENGLASS_NUM 80

struct BrokenGlassVtxBuffer
{
	grcVertexBuffer *pVtxBuffer;
	bool bNeedUpdate;
	bool bInUse;
};
static BrokenGlassVtxBuffer	g_vehicleBrokenGlassVtxBuffer[MAX_BROKENGLASS_NUM];

#endif

CVehicleGlassComponentEntity::Pool* CVehicleGlassComponentEntity::_ms_pPool = NULL;

static mthVecRand                           g_vehicleGlassVecRandom;
static VtxPushBuffer<VtxPB_PoDiTe6No_trait> g_vehicleGlassVtxBuffer;
static CVehicleGlassShader                  g_vehicleGlassShader;
static const int                            g_vehicleGlassMaxStackTriangles = 2048;
static const int							g_vehicleGlassVBCoolDown = 4;
static const int							g_vehicleGlassVBMemCapMP = 512 * 1024;
#if VEHICLE_GLASS_COMPRESSED_VERTICES

__forceinline void CVehicleGlassTriangle::SetVertexP(int i, Vec3V_In position)
{
	m_vertexPD[i] = Vec4V(position, ScalarV(V_ZERO));
}

__forceinline void CVehicleGlassTriangle::SetVertexD(int i, Vec3V_In damage)
{
	Vec4V dmg = FloatToIntRaw<8>(Vec4V(damage)); // [-0.5..0.5] -> [-128..128]

	dmg = Vec4V(Vec::V4PackSignedIntToSignedShort (dmg.GetIntrin128(), dmg.GetIntrin128()));
	dmg = Vec4V(Vec::V4PackSignedShortToSignedByte(dmg.GetIntrin128(), dmg.GetIntrin128()));

	const ScalarV dmg_ScalarV(dmg.GetIntrin128()); // it's already splatted

	m_vertexPD[i].SetW(dmg_ScalarV);
}

__forceinline void CVehicleGlassTriangle::SetVertexDZero(int i)
{
	m_vertexPD[i].SetW(ScalarV(V_ZERO));
}

// tex=16:16:16:16, nrm=8:8:8:x, col=8:8:8:8
__forceinline void CVehicleGlassTriangle::SetVertexTNC(int i, Vec4V_In texcoord, Vec3V_In normal, Vec4V_In colour)
{
	const Vec4V tex = FloatToIntRaw<16>(texcoord);
	const Vec4V nrm = FloatToIntRaw<7>(Vec4V(V_ONE) + Vec4V(normal)); // [-1..1] -> [0..256]
	const Vec4V col = FloatToIntRaw<8>(colour);

	Vec4V t, nc;
	t  = Vec4V(Vec::V4PackSignedIntToUnsignedShort (tex.GetIntrin128(), tex.GetIntrin128()));
	nc = Vec4V(Vec::V4PackSignedIntToSignedShort   (nrm.GetIntrin128(), col.GetIntrin128()));
	nc = Vec4V(Vec::V4PackSignedShortToUnsignedByte(nc .GetIntrin128(), nc .GetIntrin128()));

	m_vertexTNC[i] = GetFromTwo<Vec::Z1,Vec::W1,Vec::X2,Vec::Y2>(t, nc); // vsldoi
}

__forceinline void CVehicleGlassTriangle::SetVelocity(Vec3V_In velocity)
{
	m_velocityAndSpinAxis = Vec4V(velocity, m_velocityAndSpinAxis.GetW());
}

__forceinline void CVehicleGlassTriangle::SetSpinAxis(Vec3V_In axis)
{
	PackRGBAColor32(((u32*)&m_velocityAndSpinAxis) + 3, AddScaled(Vec4V(V_HALF), Vec4V(V_HALF), Vec4V(axis)));
}

__forceinline void CVehicleGlassTriangle::UpdatePositionsAndNormals(Vec3V_In p0, Vec3V_In p1, Vec3V_In p2)
{
	SetVertexP(0, p0);
	SetVertexP(1, p1);
	SetVertexP(2, p2);

	const Vec3V edge01 = p1 - p0;
	const Vec3V edge12 = p2 - p1;
	const Vec3V normal = NormalizeFast(Cross(edge01, edge12));

	Vec4V nrm = FloatToIntRaw<7>(Vec4V(V_ONE) + Vec4V(normal)); // [-1..1] -> [0..256]

	nrm = Vec4V(Vec::V4PackSignedIntToSignedShort   (nrm.GetIntrin128(), nrm.GetIntrin128()));
	nrm = Vec4V(Vec::V4PackSignedShortToUnsignedByte(nrm.GetIntrin128(), nrm.GetIntrin128()));

	const ScalarV nrm_ScalarV(nrm.GetIntrin128()); // it's already splatted

	m_vertexTNC[0].SetZ(nrm_ScalarV);
	m_vertexTNC[1].SetZ(nrm_ScalarV);
	m_vertexTNC[2].SetZ(nrm_ScalarV);
}

__forceinline void CVehicleGlassTriangle::GetVertexTNC(int i, Vec4V_InOut texcoord, Vec3V_InOut normal, Vec4V_InOut colour) const
{
	const Vec4V temp = m_vertexTNC[i];
	const Vec4V nc   = MergeZWByte(temp, temp); // [0..65535]
#if RSG_CPU_INTEL
	const Vec4V tex  = IntToFloatRaw<16>(MergeXYShort(temp, Vec4V(V_ZERO)));
	const Vec3V nrm  = IntToFloatRaw<16>(MergeXYShort(nc, Vec4V(V_ZERO)).GetXYZ());
	const Vec4V col  = IntToFloatRaw<16>(MergeZWShort(nc, Vec4V(V_ZERO)));
#else
	const Vec4V tex  = IntToFloatRaw<16>(MergeXYShort(Vec4V(V_ZERO), temp));
	const Vec3V nrm  = IntToFloatRaw<16>(MergeXYShort(Vec4V(V_ZERO), nc).GetXYZ());
	const Vec4V col  = IntToFloatRaw<16>(MergeZWShort(Vec4V(V_ZERO), nc));
#endif
	texcoord = tex;
	normal   = AddScaled(Vec3V(V_NEGONE), Vec3V(V_TWO), nrm); // [-1..1]
	colour   = col;
}

__forceinline Vec4V_Out CVehicleGlassTriangle::GetVertexT(int i) const
{
	const Vec4V temp = m_vertexTNC[i];
#if RSG_CPU_INTEL
	const Vec4V tex  = IntToFloatRaw<16>(MergeXYShort(temp, Vec4V(V_ZERO)));
#else
	const Vec4V tex  = IntToFloatRaw<16>(MergeXYShort(Vec4V(V_ZERO), temp));
#endif
	return tex;
}

__forceinline Vec4V_Out CVehicleGlassTriangle::GetVertexC(int i) const
{
	const Vec4V temp = m_vertexTNC[i];
	const Vec4V nc   = MergeZWByte(temp, temp); // [0..65535]
#if RSG_CPU_INTEL
	const Vec4V col  = IntToFloatRaw<16>(MergeZWShort(nc, Vec4V(V_ZERO)));
#else
	const Vec4V col  = IntToFloatRaw<16>(MergeZWShort(Vec4V(V_ZERO), nc));
#endif
	return col;
}

__forceinline Vec3V_Out CVehicleGlassTriangle::GetVelocity() const
{
	return m_velocityAndSpinAxis.GetXYZ();
}

__forceinline Vec3V_Out CVehicleGlassTriangle::GetSpinAxis() const
{
	return AddScaled(Vec3V(V_NEGONE), Vec3V(V_TWO), UnpackRGBAColor32(((u32*)&m_velocityAndSpinAxis) + 3).GetXYZ());
}

#else // not VEHICLE_GLASS_COMPRESSED_VERTICES

__forceinline void CVehicleGlassTriangle::SetVertexP(int i, Vec3V_In position)
{
	m_vertexP[i] = position;
	m_vertexD[i] = Vec3V(V_ZERO);
}

__forceinline void CVehicleGlassTriangle::SetVertexD(int i, Vec3V_In damage)
{
	m_vertexD[i] = damage;
}

__forceinline void CVehicleGlassTriangle::SetVertexDZero(int i)
{
	m_vertexD[i] = Vec3V(V_ZERO);
}

__forceinline void CVehicleGlassTriangle::SetVertexTNC(int i, Vec4V_In texcoord, Vec3V_In normal, Vec4V_In colour)
{
	m_vertexT[i] = texcoord;
	m_vertexN[i] = normal;
	m_vertexC[i] = Color32(colour);
}

__forceinline void CVehicleGlassTriangle::SetVelocity(Vec3V_In velocity)
{
	m_velocity = velocity;
}

__forceinline void CVehicleGlassTriangle::SetSpinAxis(Vec3V_In axis)
{
	m_spinAxis = axis;
}

__forceinline void CVehicleGlassTriangle::UpdatePositionsAndNormals(Vec3V_In p0, Vec3V_In p1, Vec3V_In p2)
{
	SetVertexP(0, p0);
	SetVertexP(1, p1);
	SetVertexP(2, p2);

	const Vec3V edge01 = p1 - p0;
	const Vec3V edge12 = p2 - p1;
	const Vec3V normal = NormalizeFast(Cross(edge01, edge12));

	m_vertexN[0] = normal;
	m_vertexN[1] = normal;
	m_vertexN[2] = normal;
}

__forceinline void CVehicleGlassTriangle::GetVertexTNC(int i, Vec4V_InOut texcoord, Vec3V_InOut normal, Vec4V_InOut colour) const
{
	texcoord = m_vertexT[i];
	normal   = m_vertexN[i];
	colour   = m_vertexC[i].GetRGBA();
}

__forceinline Vec4V_Out CVehicleGlassTriangle::GetVertexT(int i) const
{
	return m_vertexT[i];
}

__forceinline Vec4V_Out CVehicleGlassTriangle::GetVertexC(int i) const
{
	return m_vertexC[i].GetRGBA();
}

__forceinline Vec3V_Out CVehicleGlassTriangle::GetVelocity() const
{
	return m_velocity;
}

__forceinline Vec3V_Out CVehicleGlassTriangle::GetSpinAxis() const
{
	return m_spinAxis;
}

#endif // not VEHICLE_GLASS_COMPRESSED_VERTICES

float CVehicleGlassComponent::MAX_ARMOURED_GLASS_HEALTH = 100.0f;
u8    CVehicleGlassComponent::MAX_DECALS_CLONED_PER_WINDOW = 5;
__COMMENT(static) void CVehicleGlassComponent::StaticInit()
{
	g_vehicleGlassVtxBuffer.Init(drawTris, VG_VTX_CNT);

#if RSG_PC
	// Prepare the flexible vertex format
	for (int i = 0; i < MAX_BROKENGLASS_NUM; i++)
	{
		g_vehicleBrokenGlassVtxBuffer[i].bNeedUpdate = true;
		g_vehicleBrokenGlassVtxBuffer[i].bInUse = false;
		vfxAssertf(g_vehicleBrokenGlassVtxBuffer[i].pVtxBuffer != NULL, "Unable to allocate vertex buffer for vehicle glass");
	}
#endif

	// load shader
	{
		const char* shaderName = "vehicle_vehglass_crack";
		ASSET.PushFolder("common:/shaders");

		grmShader* shader = grmShaderFactory::GetInstance().Create();

		if (shader->Load(shaderName, NULL, false))
		{
			g_vehicleGlassShader.Create(shader);
		}
		else
		{
			vfxAssertf(0, "shader \"%s\" failed to load, is it compiled?", shaderName);
		}

		ASSET.PopFolder();
	}
}

__COMMENT(static) void CVehicleGlassComponent::StaticShutdown()
{
	g_vehicleGlassVtxBuffer.Shutdown();
	g_vehicleGlassShader.Shutdown();

#if RSG_PC
	for (int i = 0; i < MAX_BROKENGLASS_NUM; i++)
	{
		g_vehicleBrokenGlassVtxBuffer[i].bNeedUpdate = true;
		g_vehicleBrokenGlassVtxBuffer[i].bInUse = false;
	}
#endif
}

// Cloud tunables
s32 CVehicleGlassComponent::sm_iSpecialAmmoFMJVehicleBulletproofGlassMinHitsToSmash = 4;
s32 CVehicleGlassComponent::sm_iSpecialAmmoFMJVehicleBulletproofGlassMaxHitsToSmash = 6;
float CVehicleGlassComponent::sm_fSpecialAmmoFMJVehicleBulletproofGlassRandomChanceToSmash = 0.5;

void CVehicleGlassComponent::InitTunables()
{
	sm_iSpecialAmmoFMJVehicleBulletproofGlassMinHitsToSmash		 = ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("GUNRUN_AMMO_FMJ_VEH_GLASSMINHITS", 0xA30D4B5A), sm_iSpecialAmmoFMJVehicleBulletproofGlassMinHitsToSmash);							
	sm_iSpecialAmmoFMJVehicleBulletproofGlassMaxHitsToSmash		 = ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("GUNRUN_AMMO_FMJ_VEH_GLASSMAXHITS", 0x842E7610), sm_iSpecialAmmoFMJVehicleBulletproofGlassMaxHitsToSmash);							
	sm_fSpecialAmmoFMJVehicleBulletproofGlassRandomChanceToSmash = ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("GUNRUN_AMMO_FMJ_VEH_GLASSRANDOMCHANCE", 0xC32E5B97), sm_fSpecialAmmoFMJVehicleBulletproofGlassRandomChanceToSmash);							
}


#if RSG_PC
/*static*/
void CVehicleGlassComponent::CreateStaticVertexBuffers()
{
	// Prepare the flexible vertex format
	grcFvf fvf;
	CVehicleGlassVB::CreateFvf(&fvf);
	for (int i = 0; i < MAX_BROKENGLASS_NUM; i++)
	{
		if (g_vehicleBrokenGlassVtxBuffer[i].pVtxBuffer == NULL)
		{
			g_vehicleBrokenGlassVtxBuffer[i].pVtxBuffer	= grcVertexBuffer::Create(MAX_VERT_COUNT, fvf, rage::grcsBufferCreate_DynamicWrite,rage::grcsBufferSync_None, NULL);
		}
	}
}

/*static*/
void CVehicleGlassComponent::DestroyStaticVertexBuffers()
{
	for (int i = 0; i < MAX_BROKENGLASS_NUM; i++)
	{
		delete g_vehicleBrokenGlassVtxBuffer[i].pVtxBuffer;
		g_vehicleBrokenGlassVtxBuffer[i].pVtxBuffer = NULL;
	}
}
#endif // RSGPC


const rmcDrawable* GetVehicleGlassDrawable(const CPhysical* pPhysical)
{
#if __BANK
	if (g_vehicleGlassMan.GetUseHDDrawable() && pPhysical->GetIsTypeVehicle())
	{
		const CVehicleModelInfo* pVMI = static_cast<const CVehicleModelInfo*>(pPhysical->GetBaseModelInfo());

		if (pVMI && pVMI->GetHDFragType())
		{
			const rmcDrawable* pHDDrawable = pVMI->GetHDFragType()->GetCommonDrawable();

			if (pHDDrawable)
			{
				return pHDDrawable;
			}
		}
	}
#endif // __BANK

	return pPhysical->GetDrawable();
}

#if __BANK
static int CVehicleGlassComponent_FindGeometryIndex(const CPhysical* pPhysical, const char* shaderName, int boneIndex, void* PS3_ONLY(indexData), void* PS3_ONLY(vertexData))
{
	const rmcDrawable* pDrawable = GetVehicleGlassDrawable(pPhysical);
	const rmcLodGroup& lodGroup = pDrawable->GetLodGroup();
	const grmModel& model = lodGroup.GetLodModel0(LOD_HIGH);

#if __ASSERT
	const char* boneName = pDrawable->GetSkeletonData()->GetBoneData(boneIndex)->GetName();
	const int numBones = pDrawable->GetSkeletonData()->GetNumBones();
	Assert(boneIndex < numBones);
#endif // __ASSERT

	for (int geomIndex = 0; geomIndex < model.GetGeometryCount(); geomIndex++)
	{
		const grmShader& shader = pDrawable->GetShaderGroup().GetShader(model.GetShaderIndex(geomIndex));

		if (strcmp(shader.GetName(), shaderName) != 0)
		{
			continue;
		}

		grmGeometry& geom = model.GetGeometry(geomIndex);

#if USE_EDGE
		if (geom.GetType() == grmGeometry::GEOMETRYEDGE)
		{
			const grmGeometryEdge* pGeomEdge = static_cast<const grmGeometryEdge*>(&geom);
#if __DEV
			// check up front how many verts are in processed geometry and assert if too many
			int numI = 0;
			int numV = 0;

			const EdgeGeomPpuConfigInfo* edgeGeomInfos = pGeomEdge->GetEdgeGeomPpuConfigInfos();
			const int count = pGeomEdge->GetEdgeGeomPpuConfigInfoCount();

			for (int i = 0; i < count; i++)
			{
				numI += edgeGeomInfos[i].spuConfigInfo.numIndexes;
				numV += edgeGeomInfos[i].spuConfigInfo.numVertexes;
			}

			if (numI > VEHICLE_GLASS_MAX_INDICES)
			{
				vfxAssertf(0, "%s: index buffer has more indices (%d) than system can handle (%d) (geomIndex=%d, bone='%s')", pPhysical->GetModelName(), numI, VEHICLE_GLASS_MAX_INDICES, geomIndex, boneName);
				return -1;
			}

			if (numV > VEHICLE_GLASS_MAX_VERTS)
			{
				vfxAssertf(0, "%s: vertex buffer has more verts (%d) than system can handle (%d) (geomIndex=%d, bone='%s')", pPhysical->GetModelName(), numV, VEHICLE_GLASS_MAX_VERTS, geomIndex, boneName);
				return -1;
			}
#endif // __DEV

			static Vec4V* edgeVertexStreams[CExtractGeomParams::obvIdxMax] ;
			sysMemSet(&edgeVertexStreams[0], 0, sizeof(edgeVertexStreams));

			u8  BoneIndexesAndWeights[2048];
			int BoneOffset1     = 0;
			int BoneOffset2     = 0;
			int BoneOffsetPoint = 0;
			int BoneIndexOffset = 0;
			int BoneIndexStride = 1;

#if HACK_GTA4_MODELINFOIDX_ON_SPU && USE_EDGE
			CGta4DbgSpuInfoStruct gtaSpuInfoStruct;
			gtaSpuInfoStruct.gta4RenderPhaseID = 0x02;
			gtaSpuInfoStruct.gta4ModelInfoIdx  = pPhysical->GetModelIndex();
			gtaSpuInfoStruct.gta4ModelInfoType = pPhysical->GetBaseModelInfo()->GetModelType();
#endif // HACK_GTA4_MODELINFOIDX_ON_SPU && USE_EDGE

			int numVerts = 0;
			const int numIndices = pGeomEdge->GetVertexAndIndex(
				(Vector4*)vertexData,
				VEHICLE_GLASS_MAX_VERTS * VEHICLE_GLASS_CACHE_SIZE,
				(Vector4**)edgeVertexStreams,
				(u16*)indexData,
				VEHICLE_GLASS_MAX_INDICES * VEHICLE_GLASS_CACHE_SIZE,
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

			vfxAssertf(!(BoneIndexStride != 1 || BoneIndexOffset != 0), "%s: geometry is not hardskinned (geomIndex=%d, bone='%s')", pPhysical->GetModelName(), geomIndex, boneName);

			//const Vec3V* positions = (const Vec3V*)edgeVertexStreams[CExtractGeomParams::obvIdxPositions];
			const u16* indices = (const u16*)indexData;

			for (int i = 0; i < numIndices; i += 3)
			{
				const int index0 = (int)indices[i + 0];
				int blendBoneIndex = BoneIndexesAndWeights[index0*BoneIndexStride + BoneIndexOffset];

				if (blendBoneIndex >= BoneOffsetPoint)
				{
					blendBoneIndex += BoneOffset2 - BoneOffsetPoint;
				}
				else
				{
					blendBoneIndex += BoneOffset1;
				}

				vfxAssertf(blendBoneIndex >= 0 && blendBoneIndex < numBones, "%s: out of range blend bone index (%d), expected [0..%d) (geomIndex=%d, bone='%s')", pPhysical->GetModelName(), blendBoneIndex, numBones, geomIndex, boneName);

				if (blendBoneIndex == boneIndex)
				{
					return geomIndex;
				}
			}
		}
		else
#endif // USE_EDGE
		{
			// Note that the index buffer must be locked before any vertex buffers
			// are locked (e.g. with grcVertexBufferEditor).  This ordering is to
			// prevent deadlocks occurring when two threads lock in opposite order.
			grcIndexBuffer* pIndexBuffer = geom.GetIndexBuffer();
			grcVertexBuffer* pVertexBuffer = geom.GetVertexBuffer();
			const u16* pIndexPtr = pIndexBuffer->LockRO();
			const u16* pMatrixPalette = geom.GetMatrixPalette();

			WIN32PC_ONLY(if (pIndexPtr))
			{
				PS3_ONLY(++g_AllowVertexBufferVramLocks); // PS3: attempting to edit VBs in VRAM
				grcVertexBufferEditor* pvbEditor = rage_new grcVertexBufferEditor(pVertexBuffer, true, true);

				WIN32PC_ONLY(if (pvbEditor->isValid()))
				{
					for (int i = 0; i < pIndexBuffer->GetIndexCount(); i += 3)
					{
						const int index0 = (int)pIndexPtr[i + 0];
#if __ASSERT
						const int index1 = (int)pIndexPtr[i + 1];
						const int index2 = (int)pIndexPtr[i + 2];

						if (Verifyf(pVertexBuffer->GetFvf()->GetBlendWeightChannel(), "%s: no vertex blend weight channel (geomIndex=%d, bone='%s')", pPhysical->GetModelName(), geomIndex, boneName))
						{
							// make sure triangle is full weighted to the first bone
							Assert(Abs<float>(pvbEditor->GetBlendWeights(index0).x - 1.0f) <= 0.001f);
							Assert(Abs<float>(pvbEditor->GetBlendWeights(index1).x - 1.0f) <= 0.001f);
							Assert(Abs<float>(pvbEditor->GetBlendWeights(index2).x - 1.0f) <= 0.001f);
						}

						if (Verifyf(pVertexBuffer->GetFvf()->GetBindingsChannel(), "%s: no vertex blend index channel (geomIndex=%d, bone='%s')", pPhysical->GetModelName(), geomIndex, boneName))
#endif // __ASSERT
						{
							const int blendBoneIndex = pMatrixPalette[pvbEditor->GetBlendIndices(index0).GetRed()];

							vfxAssertf(blendBoneIndex >= 0 && blendBoneIndex < numBones, "%s: out of range blend bone index (%d), expected [0..%d) (geomIndex=%d, bone='%s')", pPhysical->GetModelName(), blendBoneIndex, numBones, geomIndex, boneName);

							if (blendBoneIndex == boneIndex)
							{
								pIndexBuffer->UnlockRO();
								delete pvbEditor;
								return geomIndex;
							}
						}
					}
				}
#if RSG_PC
				else
				{
					vfxAssertf(0, "%s: failed to lock vertex buffer for vehicle glass (geomIndex=%d, bone='%s')", pPhysical->GetModelName(), geomIndex, boneName);
					pIndexBuffer->UnlockRO();
					delete pvbEditor;
					return -1;
				}
#endif // RSG_PC
				PS3_ONLY(--g_AllowVertexBufferVramLocks); // PS3: finished with editing VBs in VRAM
				pIndexBuffer->UnlockRO();
				delete pvbEditor;
			}
#if RSG_PC
			else
			{
				vfxAssertf(0, "%s: failed to lock index buffer for vehicle glass (geomIndex=%d, bone='%s')", pPhysical->GetModelName(), geomIndex, boneName);
				return -1;
			}
#endif // RSG_PC
		}
	}

	return -1;
}
#endif // __BANK

static eHierarchyId GetBoneHierarchyId(const CPhysical* pPhysical, int boneIndex)
{
	if (pPhysical->GetIsTypeVehicle())
	{
		const CVehicle* pVehicle = static_cast<const CVehicle*>(pPhysical);

		for (int id = VEH_FIRST_WINDOW; id <= VEH_LAST_WINDOW; id++)
		{
			if (pVehicle->GetBoneIndex((eHierarchyId)id) == boneIndex)
			{
				return (eHierarchyId)id;
			}
		}
	}

	return VEH_INVALID_ID;
}

void CVehicleGlassComponent::InitComponent(CPhysical* pPhysical, int componentId, phMaterialMgr::Id mtlId)
{
	vfxAssertf(pPhysical->GetIsTypeVehicle() || (pPhysical->GetIsTypeObject() && static_cast<CObject*>(pPhysical)->m_nObjectFlags.bVehiclePart), "CVehicleGlassComponent::InitComponent called on non-vehicle entity! this will probably crash");
	m_pCompEntity = NULL;
	pPhysical->SetBrokenFlag(componentId);
	NetworkInterface::OnEntitySmashed(pPhysical, componentId, true);

	vfxAssertf(pPhysical->m_nFlags.bIsFrag, "%s: non frag??", pPhysical->GetModelName());

	const fragInst* pFragInst = pPhysical->GetFragInst();
	const fragType* pFragType = pFragInst->GetType();

	vfxAssertf(pFragType && pFragType->IsModelSkinned(), "%s: non-skinned frag??", pPhysical->GetModelName());

	const fragTypeChild* pFragChild = pFragInst->GetTypePhysics()->GetChild(componentId);
	const int boneIndex = pFragType->GetBoneIndexFromID(pFragChild->GetBoneID());
	const rmcDrawable* pDrawable = GetVehicleGlassDrawable(pPhysical);

    int mappedBoneIndex = -1;

	m_fArmouredGlassHealth = MAX_ARMOURED_GLASS_HEALTH;
	m_armouredGlassPenetrationDecalCount = 0;

	if (pPhysical->GetIsTypeVehicle())
	{
		const CVehicle* pVehicle = static_cast<const CVehicle*>(pPhysical);

		for (int id = VEH_FIRST_WINDOW; id <= VEH_LAST_WINDOW; id++)
		{
			if (pVehicle->GetBoneIndex((eHierarchyId)id) == boneIndex)
			{
				m_hierarchyId = (eHierarchyId)id;
				break;
			}
		}

        mappedBoneIndex = pVehicle->GetVehicleModelInfo()->GetLodSkeletonBoneIndex(boneIndex);

		if(pVehicle->InheritsFromPlane())
		{
			static_cast<CPlane*>(pPhysical)->UpdateWindowBound(m_hierarchyId);
		}
	}

#if VEHICLE_GLASS_SMASH_TEST
	m_smashTestMaxTriangleErrorP = Vec3V(V_ZERO);
	m_smashTestMaxTriangleErrorT = Vec4V(V_ZERO);
	m_smashTestMaxTriangleErrorN = Vec3V(V_ZERO);
	m_smashTestMaxTriangleErrorC = Vec4V(V_ZERO);
#endif // VEHICLE_GLASS_SMASH_TEST

	m_pAttachedVB              = NULL;

	m_regdPhy                  = pPhysical;
	m_modelIndex               = pPhysical->GetModelIndex();
	m_componentId              = componentId;
	m_hierarchyId              = GetBoneHierarchyId(pPhysical, boneIndex); // we'll compute this below when we have the bone index
	m_materialId               = mtlId;
	m_geomIndex                = -1;
	m_boneIndex                = mappedBoneIndex != -1 ? mappedBoneIndex : boneIndex;
	m_readytoRenderFrame	   = 0;
	m_pWindow                  = GetWindow(pPhysical); // model index and bone index must be set before calling GetWindow

	m_isValid                  = false;
	m_cracked                  = false;
	m_smashed                  = false;
	m_damageDirty              = true;
	m_removeComponent          = false;
	m_forceSmash               = false;
	m_forceSmashDetach         = false;
	m_nothingToDetach          = false;
#if __BANK
	m_updateTessellation       = false;
#endif // __BANK
	m_isVisible[0]             = false;
	m_isVisible[1]             = false;

#if VEHICLE_GLASS_CLUMP_NOISE
	m_clumpNoiseBufferIndex    = (u16)g_DrawRand.GetRanged(0, VEHICLE_GLASS_CLUMP_NOISE_BUFFER_COUNT - 1);
	m_clumpNoiseBufferOffset   = (u16)g_DrawRand.GetRanged(0, 65535);
#endif // VEHICLE_GLASS_CLUMP_NOISE

	m_matrix                   = Mat34V(V_ZERO);
	m_smashSphere              = Vec4V(V_ZERO);
	m_centroid                 = Vec3V(V_ZERO);
	m_distToViewportSqr        = 0.0f;
	m_renderGlobalAlpha        = 0.0f;
	m_renderGlobalNaturalAmbientScale = 0;
	m_renderGlobalArtificialAmbientScale = 0;

	m_numDecals                = 0;
	m_lastFxTime               = 0;
	m_audioEvent			   . Init();
	m_crackTextureScale        = m_pWindow ? m_pWindow->GetTextureScale()/VGW_DEFAULT_TEXTURE_SCALE : 1.f;
	
//	m_shaderData               . Init();

	const rmcLodGroup& lodGroup = pDrawable->GetLodGroup();
	const grmModel* model = &(lodGroup.GetLodModel0(LOD_HIGH));
#if __ASSERT || VEHICLE_GLASS_VERBOSE_DEBUG_OUTPUT
	const char* boneName = pPhysical->GetDrawable()->GetSkeletonData()->GetBoneData(boneIndex)->GetName();
#endif // __ASSERT || VEHICLE_GLASS_VERBOSE_DEBUG_OUTPUT

	if (m_pWindow)
	{
		m_geomIndex = m_pWindow->m_geomIndex;
	}

#if __BANK
	if ((m_geomIndex == -1 && (
#if VEHICLE_GLASS_DEV
		g_vehicleGlassMan.m_distanceFieldSource == VGW_SOURCE_NONE ||
#endif // VEHICLE_GLASS_DEV
		g_vehicleGlassMan.m_allowFindGeometryWithoutWindow)) || g_vehicleGlassMan.m_useHDDrawable)
	{
		if (PARAM_vgasserts.Get() && !g_vehicleGlassMan.m_useHDDrawable)
		{
			vfxAssertf(0, "%s: calling FindGeometryIndex for componentId=%d, boneIndex=%d, bone='%s'", pPhysical->GetModelName(), m_componentId, boneIndex, boneName);
		}

		m_geomIndex = CVehicleGlassComponent_FindGeometryIndex(
			pPhysical,
			"vehicle_vehglass",
			boneIndex,
			PS3_SWITCH(g_vehicleGlassMan.GetEdgeIndexDataBuffer(), NULL),
			PS3_SWITCH(g_vehicleGlassMan.GetEdgeVertexDataBuffer(), NULL)
		);
	}
#endif // __BANK

	if (m_geomIndex != -1)
	{
#if VEHICLE_GLASS_VERBOSE_DEBUG_OUTPUT
		if (g_vehicleGlassMan.m_verbose)
		{
			Displayf(
				"creating vehicle glass group for %s with componentId=%d, geomIndex=%d, boneId=%d, boneIndex=%d, bone='%s'",
				pPhysical->GetModelName(),
				componentId,
				m_geomIndex,
				pFragChild->GetBoneID(),
				boneIndex,
				boneName
			);
		}
#endif // VEHICLE_GLASS_VERBOSE_DEBUG_OUTPUT

		// Nasty temp hack, until I figure out the fuck's going on in there.
		if(m_geomIndex >= model->GetGeometryCount() )
		{
			vfxAssertf(m_geomIndex < model->GetGeometryCount(),"%s : m_geomIndex doesn't match (%d >= %d)",pPhysical->GetModelName(), m_geomIndex, model->GetGeometryCount() );
			model = &(lodGroup.GetLodModel0(LOD_HIGH));
			vfxAssertf(m_geomIndex < model->GetGeometryCount(),"%s : m_geomIndex doesn't match (%d >= %d)",pPhysical->GetModelName(), m_geomIndex, model->GetGeometryCount());
		}


		InitComponentGeom(model->GetGeometry(m_geomIndex), mappedBoneIndex != -1 ? mappedBoneIndex : boneIndex);
		m_isValid = true;

#if VEHICLE_GLASS_SMASH_TEST
		if (!g_vehicleGlassSmashTest.m_smashTest)
#endif // VEHICLE_GLASS_SMASH_TEST
		{
			if (PARAM_vgasserts.Get()) // suppress this assert unless we specifically want it (BS#707811)
			{
				vfxAssertf(GetNumTriangles(false) > 0, "%s: no triangles?? (componentId=%d, bone='%s')", pPhysical->GetModelName(), m_componentId, boneName);
			}
		}
	}
}

void CVehicleGlassComponent::ApplyArmouredGlassDecals(CVehicle* pVehicle)
{
	Assertf(m_pCompEntity != NULL, "Component entity is NULL in CVehicleGlassComponent::ApplyArmouredGlassDecals !");

	if (pVehicle)
	{
		auto materialID = PGTAMATERIALMGR->g_idCarGlassStrong;
		VfxWeaponInfo_s* pFxWeaponInfo = g_vfxWeapon.GetInfo(PGTAMATERIALMGR->GetMtlVfxGroup(materialID), WEAPON_EFFECT_GROUP_RIFLE_ASSAULT, false);

		VfxCollInfo_s decalColnInfo; 
		decalColnInfo.roomId        = 0;
		decalColnInfo.isBloody		= false;
		decalColnInfo.isWater		= false;
		decalColnInfo.isExitFx		= false;
		decalColnInfo.noPtFx		= true;
		decalColnInfo.noPedDamage	= true;
		decalColnInfo.noDecal		= false;
		decalColnInfo.isSnowball	= false;

		const CPhysical* pPhysical = m_regdPhy.Get();
		Vec3V vVel = GTA_ONLY(RCC_VEC3V)(m_regdPhy->GetVelocity());

		int nLosOptions = 0;
		nLosOptions |= WorldProbe::LOS_TEST_VEH_TYRES;
		phInst* pHitInst = pVehicle->GetFragInst();
		if(!pHitInst) pHitInst = pVehicle->GetCurrentPhysicsInst();

		Vec3V point1;
		Vec3V point2;
		Vec3V randomPoint;

		const int VEHICLE_VOID_MATERIAL_ID =  142;		

		for(int itter = 0; itter < m_armouredGlassPenetrationDecalCount && itter <= MAX_DECALS_CLONED_PER_WINDOW; itter++)
		{
			randomPoint = m_boundsMax - m_boundsMin;

			randomPoint -= Vec3V(
				fwRandom::GetRandomNumberInRange(0.f, (randomPoint.GetX().Getf())), 
				fwRandom::GetRandomNumberInRange(0.f, (randomPoint.GetY().Getf())), 
				fwRandom::GetRandomNumberInRange(0.f, (randomPoint.GetZ().Getf())));
			
			switch ((int)m_hierarchyId)
			{
			case VEH_WINDSCREEN:				
			case VEH_WINDSCREEN_R:
				point1.SetX(m_boundsMax.GetX() - randomPoint.GetX());
				point1.SetY(m_boundsMax.GetY());
				point2.SetX(point1.GetX());
				point2.SetY(m_boundsMin.GetY());
				break;
			case VEH_WINDOW_LF:
			case VEH_WINDOW_LR:
			case VEH_WINDOW_LM:				
			case VEH_WINDOW_RF:
			case VEH_WINDOW_RR:
			case VEH_WINDOW_RM:
				point1.SetY(m_boundsMax.GetY() - randomPoint.GetY());				
				point1.SetX(m_boundsMax.GetX());	
				point2.SetY(point1.GetY());			
				point2.SetX(m_boundsMin.GetX());				
				break;
			}

			point1.SetZ(m_boundsMax.GetZ() - randomPoint.GetZ());
			point2.SetZ(point1.GetZ());

			point1 = Transform(pVehicle->GetMatrix(), point1);
			point2 = Transform(pVehicle->GetMatrix(), point2);

			WorldProbe::CShapeTestProbeDesc probeDesc;
			WorldProbe::CShapeTestResults* probe_results = NULL;
			WorldProbe::CShapeTestFixedResults<1> probeResult;
			probe_results = &probeResult;
			probeDesc.SetResultsStructure(probe_results);
			probeDesc.SetStartAndEnd(VEC3V_TO_VECTOR3(point1), VEC3V_TO_VECTOR3(point2));
			probeDesc.SetDomainForTest(WorldProbe::TEST_AGAINST_INDIVIDUAL_OBJECTS);
								
			probeDesc.SetOptions(nLosOptions);
			probeDesc.SetTypeFlags(ArchetypeFlags::GTA_WEAPON_TEST);						
			probeDesc.SetIncludeInstance(pHitInst);

			bool bSuccess = WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc);

			if(bSuccess) 
			{
				auto materialIdFromProbe = (*probe_results)[0].GetMaterialId();
				phMaterialMgr::Id unpackedMaterialIdFromProbe = 0;
				if (materialIdFromProbe)
				{
					unpackedMaterialIdFromProbe = PGTAMATERIALMGR->UnpackMtlId(materialIdFromProbe);
				}

				if (unpackedMaterialIdFromProbe == PGTAMATERIALMGR->g_idCarGlassStrong || unpackedMaterialIdFromProbe == VEHICLE_VOID_MATERIAL_ID) // Allow vehicle void for now (space between concave window)
				{
					decalColnInfo.vNormalWld    = VECTOR3_TO_VEC3V((*probe_results)[0].GetHitNormal());
					decalColnInfo.vPositionWld  = VECTOR3_TO_VEC3V((*probe_results)[0].GetHitPosition());
					decalColnInfo.vPositionWld += vVel*ScalarVFromF32(fwTimer::GetTimeStep());	

					decalColnInfo.vDirectionWld = point2 - point1;
					decalColnInfo.vNormalWld    = Normalize(decalColnInfo.vNormalWld); 		

					decalColnInfo.regdEnt       = pVehicle;

					// Pass the occasional VEHICLE_VOID_MATERIAL_ID as a valid, strong glass ID so that it does not assert in AddWeaponImpact()
					// VEHICLE_VOID_MATERIAL_ID means there is no glass where it should be, or the collision point is in the space within the concavity of the glass (which is okay in this case!)
					decalColnInfo.materialId    = PGTAMATERIALMGR->g_idCarGlassStrong;
					decalColnInfo.componentId   = m_componentId;	

					g_decalMan.AddWeaponImpact(*pFxWeaponInfo, decalColnInfo, this, const_cast<CPhysical*>(pPhysical), m_boneIndex, true);				
				}				
			}			
		}	

		// Always crack at 33% health
		if (m_fArmouredGlassHealth <= MAX_ARMOURED_GLASS_HEALTH * 0.33f)
		{
			m_cracked = true;

			const_cast<CPhysical*>(pPhysical)->SetHiddenFlag(m_componentId);

			if( !m_readytoRenderFrame )
				m_readytoRenderFrame = fwTimer::GetFrameCount();		
		}
	}	
}

void CVehicleGlassComponent::ShutdownComponent()
{
#if VEHICLE_GLASS_SMASH_TEST
	g_vehicleGlassSmashTest.ShutdownComponent(this);
#endif // VEHICLE_GLASS_SMASH_TEST

	ShutdownComponentGroup();

#if PGHANDLE_REF_COUNT
	m_shaderData.m_DiffuseTex  = NULL;
	m_shaderData.m_DirtTex     = NULL;
	m_shaderData.m_SpecularTex = NULL;
#endif // PGHANDLE_REF_COUNT

	if(m_pCompEntity)
	{
		m_pCompEntity->RemoveRef();
	}
	g_vehicleGlassMan.m_componentList.ReturnToPool(this, g_vehicleGlassMan.m_componentPool);
	if (m_regdPhy)
	{
		m_regdPhy = NULL;
	}

#if __BANK
	for (const atSNode<Vec4V>* pCurNode = m_listExplosionHistory.PopHead(); pCurNode; pCurNode = m_listExplosionHistory.PopHead())
	{
		delete pCurNode;
	}
#endif // __BANK
}

void CVehicleGlassComponent::ShutdownComponentGroup()
{
	if (m_numDecals > 0)
	{
		g_decalMan.Remove(m_regdPhy, -1, this);
	}

	for (CVehicleGlassTriangle* pTriangle = m_triangleListDetached.GetHead(); pTriangle; )
	{
		pTriangle = m_triangleListDetached.ReturnToPool_GetNext(pTriangle, g_vehicleGlassMan.m_trianglePool);
	}

	m_audioEvent.Shutdown();
	m_numDecals = 0;
	m_isValid = false;

	if (m_pAttachedVB)
	{
		CVehicleGlassVB::Destroy(m_pAttachedVB);
		m_pAttachedVB = NULL;
	}

	m_removeComponent = true; // No need to keep this component
}

void CVehicleGlassComponent::ClearDetached()
{
	for (CVehicleGlassTriangle* pTriangle = m_triangleListDetached.GetHead(); pTriangle; )
	{
		pTriangle = m_triangleListDetached.ReturnToPool_GetNext(pTriangle, g_vehicleGlassMan.m_trianglePool);
	}
}

bool CVehicleGlassComponent::GetVehicleDamageDeformation(const CPhysical* pVehicle, Vec3V* damageBuffer) const
{
	Assert(m_pAttachedVB && m_pAttachedVB->GetVertexCount() > 0);

	fwTexturePayload* pDamageTex = NULL;
	float damageRadius = 0.0f;
	GTA_ONLY(float damageMult = 1.0f);
	g_pDecalCallbacks->GetDamageData(pVehicle, &pDamageTex, damageRadius GTA_ONLY(, damageMult));

	Vec3V dmgOffsetOr(V_ZERO); // bitwise-OR of all damage offsets, will be zero if there is no damage

	if (pDamageTex && pVehicle->GetIsTypeVehicle())
	{
		void* pDamageTexBasePtr = pDamageTex->AcquireBasePtr(grcsRead);
		const CVehicle *ptrVehiclePtr = static_cast<const CVehicle *>(pVehicle);

		if (AssertVerify(pDamageTexBasePtr) && ptrVehiclePtr->GetVehicleDamage() && ptrVehiclePtr->GetVehicleDamage()->GetDeformation())
		{
			for (int iCurVert = 0; iCurVert < m_pAttachedVB->GetVertexCount(); iCurVert++)
			{
				const Vec3V position = m_pAttachedVB->GetVertexP(iCurVert);
				const Vec3V dmgOffset = ptrVehiclePtr->GetVehicleDamage()->GetDeformation()->ReadFromVectorOffset(pDamageTexBasePtr, position);

				dmgOffsetOr |= dmgOffset;
				damageBuffer[iCurVert] = dmgOffset;
			}
		}
	}

	return !IsZeroAll(dmgOffsetOr);
}

bool CVehicleGlassComponent::UpdateComponent(float deltaTime)
{
	CPhysical* pPhysical = m_regdPhy.Get();

	// delete the component if it's parent object has been deleted
	if (pPhysical == NULL || pPhysical->GetDrawable() == NULL)
	{
		m_renderGlobalAlpha = 1.0f;
		m_renderGlobalNaturalAmbientScale = 255;
		m_renderGlobalArtificialAmbientScale = 0;

		ShutdownComponentGroup();
	}
	else
	{
		if (pPhysical->GetIsVisibleInSomeViewportThisFrame())
		{
			SetVisible();
		}

		// update the matrix for this vehicle glass component
		Assert(m_boneIndex >= 0);
		CVfxHelper::GetMatrixFromBoneIndex_Smash(m_matrix, pPhysical, m_boneIndex);

		// add reference to archetype so that CSE doesn't get deleted while it's being rendered
		gDrawListMgr->AddArchetypeReference(pPhysical->GetModelIndex());

		// grab the window again in case the fragType was defragmented
		m_pWindow = GetWindow(pPhysical);

#if __BANK
		if (m_updateTessellation)
		{
			const u64 prevSeed = g_DrawRand.GetFullSeed();

			g_DrawRand.Reset(g_vehicleGlassMan.m_smashWindowUpdateRandomSeed);

			for (CVehicleGlassTriangle* pTriangle = m_triangleListDetached.GetHead(); pTriangle; )
			{
				pTriangle = m_triangleListDetached.ReturnToPool_GetNext(pTriangle, g_vehicleGlassMan.m_trianglePool);
			}

			const rmcDrawable* pDrawable = GetVehicleGlassDrawable(pPhysical);
			const rmcLodGroup& lodGroup = pDrawable->GetLodGroup();
			const grmModel& model = lodGroup.GetLodModel0(LOD_HIGH);
			const grmGeometry& geom = model.GetGeometry(m_geomIndex);

			int unused = 0;

			InitComponentGeom(geom, m_boneIndex);
			ProcessTessellation(pPhysical, NULL, 1.0f, false, true, unused);

			// We may run out of available vertex buffers due to the cool down
			// If we just keep trying one should be available eventually
			if (m_pAttachedVB)
			{
				m_updateTessellation = false;
			}

			g_DrawRand.SetFullSeed(prevSeed);
		}
		else if (this == CVehicleGlassManager::GetSmashWindowTargetComponent())
		{
			g_vehicleGlassMan.m_smashWindowCurrentSmashSphere = m_smashSphere;
		}
#endif // __BANK

		const float updateTime = deltaTime BANK_ONLY(*g_vehicleGlassMan.m_timeScale);

		m_distToViewportSqr        = CVfxHelper::GetDistSqrToCamera(pPhysical->GetTransform().GetPosition());
		m_renderGlobalAlpha        = (1.0f/255.0f)*(float)pPhysical->GetAlpha();
		m_renderGlobalNaturalAmbientScale = (u8)pPhysical->GetNaturalAmbientScale();
		m_renderGlobalArtificialAmbientScale = (u8)pPhysical->GetArtificialAmbientScale();

		const ScalarV vUpdateTime              = ScalarV(updateTime);
		const ScalarV vUpdateTimeRotationSpeed = BANK_ONLY(g_vehicleGlassMan.m_disableSpinAxis ? ScalarV(V_ZERO) :) ScalarV(updateTime*VEHICLEGLASSPOLY_ROTATION_SPEED);
		const ScalarV vUpdateTimeGravity       = ScalarV(updateTime*VEHICLEGLASSPOLY_GRAVITY_SCALE*VEHICLE_GLASS_GRAVITY);
		const Vec3V velocityDampening(VEHICLEGLASSPOLY_VEL_DAMP, VEHICLEGLASSPOLY_VEL_DAMP, VEHICLEGLASSPOLY_VEL_DAMP);

		// Get the ground information ground information
		Vec4V groundPlane;
		bool touchingGround = false;
		GetGroundInfo(pPhysical, groundPlane, touchingGround);

		for (CVehicleGlassTriangle* pTriangle = m_triangleListDetached.GetHead(); pTriangle; )
		{
			CVehicleGlassTriangle* pNextTriangle = m_triangleListDetached.GetNext(pTriangle);
			if (pNextTriangle)
			{
				PrefetchObject(pNextTriangle);
			}

			Vec3V p0 = pTriangle->GetDamagedVertexP(0);
			Vec3V p1 = pTriangle->GetDamagedVertexP(1);
			Vec3V p2 = pTriangle->GetDamagedVertexP(2);

			vfxAssertf(IsFiniteStable(p0), "%s: vehicle glass p[0] is invalid before transform (%f,%f,%f)", pPhysical->GetModelName(), VEC3V_ARGS(p0));
			vfxAssertf(IsFiniteStable(p1), "%s: vehicle glass p[1] is invalid before transform (%f,%f,%f)", pPhysical->GetModelName(), VEC3V_ARGS(p1));
			vfxAssertf(IsFiniteStable(p2), "%s: vehicle glass p[2] is invalid before transform (%f,%f,%f)", pPhysical->GetModelName(), VEC3V_ARGS(p2));

			// Simulate detached triangles

			Vec3V velocity = pTriangle->GetVelocity();
			vfxAssertf(IsFiniteStable(velocity), "%s: vehicle glass triangle velocity is invalid after transform (%f,%f,%f)", pPhysical->GetModelName(), VEC3V_ARGS(velocity));

			const Vec3V centroid = (p0 + p1 + p2)*ScalarVConstant<FLOAT_TO_INT(1.0f/3.0f)>();
			const Vec3V centroidPlusVeloStep = BANK_ONLY(g_vehicleGlassMan.m_disablePosUpdate ? centroid :) AddScaled(centroid, velocity, vUpdateTime);

			// dampen the velocity on the XY components for next update tick
			velocity = SubtractScaled(velocity, Scale(velocity, velocityDampening), vUpdateTime);

			// update the velocity with gravity for next update tick
			pTriangle->SetVelocity(SubtractScaled(velocity, Vec3V(V_Z_AXIS_WZERO), vUpdateTimeGravity));

			const Vec3V axis = pTriangle->GetSpinAxis();
			Mat34V rot;
			Mat34VFromAxisAngle(rot, axis, vUpdateTimeRotationSpeed, centroidPlusVeloStep);
			p0 = Transform(rot, Subtract(p0, centroid));
			p1 = Transform(rot, Subtract(p1, centroid));
			p2 = Transform(rot, Subtract(p2, centroid));

			vfxAssertf(IsFiniteStable(p0), "%s: vehicle glass p[0] is invalid after update (%f,%f,%f) - axis is (%f,%f,%f)", pPhysical->GetModelName(), VEC3V_ARGS(p0), VEC3V_ARGS(axis));
			vfxAssertf(IsFiniteStable(p1), "%s: vehicle glass p[1] is invalid after update (%f,%f,%f) - axis is (%f,%f,%f)", pPhysical->GetModelName(), VEC3V_ARGS(p1), VEC3V_ARGS(axis));
			vfxAssertf(IsFiniteStable(p2), "%s: vehicle glass p[2] is invalid after update (%f,%f,%f) - axis is (%f,%f,%f)", pPhysical->GetModelName(), VEC3V_ARGS(p2), VEC3V_ARGS(axis));

			// check if triangle needs removed
			const ScalarV distCentroidPlane = PlaneDistanceTo(groundPlane, centroidPlusVeloStep);

			if (IsLessThanAll(distCentroidPlane, ScalarV(V_ZERO)))
			{
				// totally underground - remove triangle from the group and return to the pool
				m_triangleListDetached.ReturnToPool(pTriangle, g_vehicleGlassMan.m_trianglePool);

				if (touchingGround)
				{
					// AUDIO: glass shard hitting ground
					Vec3V audioPos = centroidPlusVeloStep - groundPlane.GetXYZ() * distCentroidPlane;

					// try to catch BS#854118
					vfxAssertf(IsFiniteStable(audioPos), "%s: vehicle glass audio position is invalid (%f,%f,%f)", pPhysical->GetModelName(), VEC3V_ARGS(audioPos));

					BANK_ONLY(if (g_vehicleGlassMan.m_groundEffectAudio))
					{
						const CVehicle* pVehicle = static_cast<const CVehicle*>(pPhysical);
						m_audioEvent.ReportPolyHitGround(RCC_VECTOR3(audioPos),pVehicle);
					}

					BANK_ONLY(if (g_vehicleGlassMan.m_groundEffectParticle))
					{
						// play glass smashing on ground effect
						if (m_distToViewportSqr < VFXRANGE_GLASS_SMASH_SQR)
						{
							ptfxGlassShatterInfo glassShatterInfo;
							glassShatterInfo.vPos = audioPos;
							g_ptFxManager.StoreGlassShatter(glassShatterInfo);
						}
					}
				}
			}
			else // still above ground - update the position and normal
			{
				pTriangle->UpdatePositionsAndNormals(p0, p1, p2);
			}

			pTriangle = pNextTriangle;
		}

		const u32 numTriangles = GetNumTriangles(false);

		if (numTriangles > 0)
		{
			const rmcDrawable* pDrawable = GetVehicleGlassDrawable(pPhysical);
			const rmcLodGroup& lodGroup = pDrawable->GetLodGroup();
			const grmModel* model = &(lodGroup.GetLodModel0(LOD_HIGH));

			if(m_geomIndex >= model->GetGeometryCount() )
			{
				vfxAssertf(m_geomIndex < model->GetGeometryCount(),"%s : m_geomIndex doesn't match (%d >= %d)", pPhysical->GetModelName(), m_geomIndex, model->GetGeometryCount() );
				model = &(lodGroup.GetLodModel0(LOD_HIGH));
				vfxAssertf(m_geomIndex < model->GetGeometryCount(),"%s : m_geomIndex doesn't match (%d >= %d)", pPhysical->GetModelName(), m_geomIndex, model->GetGeometryCount());
			}

			const int shaderIndex = model->GetShaderIndex(m_geomIndex);
			const grmShader* pShader = pDrawable->GetShaderGroup().GetShaderPtr(shaderIndex);

			m_shaderData.GetStandardShaderVariables(pShader);

			fwDrawData* pDrawData = pPhysical->GetDrawHandlerPtr();
			const fwCustomShaderEffect* pCSE = pDrawData->GetShaderEffect();

			if (pCSE && AssertVerify(pCSE->GetType() == CSE_VEHICLE))
			{
				const CCustomShaderEffectVehicle* pVehicleCSE = static_cast<const CCustomShaderEffectVehicle*>(pCSE);

#if USE_GPU_VEHICLEDEFORMATION
				// Get the damage values for the shader regardless of the LOD
				m_shaderData.GetDamageShaderVariables(pVehicleCSE);
#endif // USE_GPU_VEHICLEDEFORMATION

				if (Verifyf(pVehicleCSE, "VehicleGlassComponent parent missing its custom shader"))
				{
					m_shaderData.GetCustomShaderVariables(pVehicleCSE);
				}

				if (pVehicleCSE->GetCseType()->GetIsHighDetail())
				{
					// When using the HD shader we need to grab the dirt texture from the HIGH_LOD shader since
					//  we only store the shader index belongs to the HIGH_LOD model
					CVehicleDrawHandler* pVehicleDrawHandler = static_cast<CVehicleDrawHandler*>(pDrawData);

					const fwCustomShaderEffect* pCSE_HLOD = pVehicleDrawHandler->GetShaderEffectSD();
					const CCustomShaderEffectVehicle* pVehicleCSE_HLOD = static_cast<const CCustomShaderEffectVehicle*>(pCSE_HLOD);

					if (Verifyf(pVehicleCSE_HLOD, "VehicleGlassComponent: SD shader is NULL"))
					{
						m_shaderData.GetDirtTexture(pVehicleCSE_HLOD, shaderIndex ASSERT_ONLY(, GetModelName()));
					}
				}
				else
				{
					m_shaderData.GetDirtTexture(pVehicleCSE, shaderIndex ASSERT_ONLY(, GetModelName()));
				}
			}

#if !USE_GPU_VEHICLEDEFORMATION
			if (pPhysical->GetIsTypeVehicle()) // it might not be a vehicle - it could be a vehicle part, which is just a CPhysical ..
			{
				if (m_damageDirty BANK_ONLY(|| g_vehicleGlassMan.m_forceVehicleDamageUpdate))
				{
					if (m_pAttachedVB && m_pAttachedVB->GetVertexCount() > 0)
					{
						Vec3V* damageBuffer = Alloca(Vec3V, m_pAttachedVB->GetVertexCount()); // Allocate enough room for all the offsets

						if (GetVehicleDamageDeformation(pPhysical, damageBuffer))
						{
							UpdateVBDamage(damageBuffer); // Update the VB with the new damage data
						}
					}

					m_damageDirty = false;
					BANK_ONLY(g_vehicleGlassMan.m_forceVehicleDamageUpdate = false);
				}
			}
#endif // !USE_GPU_VEHICLEDEFORMATION

			// Store the light bounding box here because we may lose the entity by the time we render this component
			m_lightsBoundBox = Lights::CalculatePartialDrawableBound(*pPhysical, BUCKETMASK_GENERATE(CRenderer::RB_ALPHA), 0);
		}
		else // no triangles, just delete it
		{
			ShutdownComponentGroup();
		}
	}

	return m_isValid;
}

void CVehicleGlassComponent::RenderComponent()
{
	static pgRef<grcTexture> crackTexture(NULL);

	if (crackTexture == NULL)
	{
#if GTA_VERSION
		const strLocalIndex txdSlot = strLocalIndex(CPhysics::GetGlassTxdSlot());
#else
		const int txdSlot = CPhysics::GetGlassTxdSlot();
#endif
		if (txdSlot != -1)
		{
			const fwTxd* txd = g_TxdStore.Get(txdSlot);

			if (txd)
			{
				crackTexture = txd->Lookup(ATSTRINGHASH("CrackTempered", 0x0fc840c31));
			}
		}
	}

#if 0 && !__FINAL
	if (crackTexture == NULL)
	{
		crackTexture = grcTextureFactory::CreateTexture(RS_PROJROOT "/art/Debug/graphics/smashable_glass_RGB.dds");
	}
#endif // !__FINAL

	vfxAssertf(crackTexture, "vehicle glass crack texture is NULL!");

	g_vehicleGlassShader.SetShaderVariables(m_shaderData);

	// Choose the crack scale based on the camera mode
	float crackTextureScale = m_crackTextureScale;
	if (camInterface::IsRenderedCameraInsideVehicle() || CVfxHelper::GetCamInVehicleFlag())
	{
		crackTextureScale *= BANK_SWITCH(g_vehicleGlassMan.m_crackTextureScaleFP, VEHICLE_GLASS_DEFAULT_CRACK_TEXTURE_SCALE_FP);
	}
	else
	{
		crackTextureScale *= BANK_SWITCH(g_vehicleGlassMan.m_crackTextureScale, VEHICLE_GLASS_DEFAULT_CRACK_TEXTURE_SCALE);
	}

	Vec4V crackTint = BANK_SWITCH(g_vehicleGlassMan.m_crackTint, VEHICLE_GLASS_DEFAULT_CRACK_TINT);

	g_vehicleGlassShader.SetCrackTextureParams(
		crackTexture,
		BANK_SWITCH(g_vehicleGlassMan.m_crackTextureAmount    , VEHICLE_GLASS_DEFAULT_CRACK_TEXTURE_AMOUNT),
		crackTextureScale,
		BANK_SWITCH(g_vehicleGlassMan.m_crackTextureBumpAmount, VEHICLE_GLASS_DEFAULT_CRACK_TEXTURE_BUMP_AMOUNT),
		BANK_SWITCH(g_vehicleGlassMan.m_crackTextureBumpiness , VEHICLE_GLASS_DEFAULT_CRACK_TEXTURE_BUMPINESS),
		crackTint
	);

	const grmShader* pShader = g_vehicleGlassShader.m_shader;

	// Preserve the world matrix
	grcViewport* view = grcViewport::GetCurrent();
	const Mat34V prevWorldMat = view->GetWorldMtx();

	// Render the attached buffer if it contains anything
	if (m_pAttachedVB && m_pAttachedVB->GetVertexCount() > 0)
	{
#if RSG_PC
		// Update vertex buffer with data
		if (m_pAttachedVB->UpdateVB())
#endif
		{
			(void)Verifyf(pShader->BeginDraw(grmShader::RMC_DRAW, true) == 1, "Glass shader should only have one pass");
			pShader->Bind(0);

			view->SetWorldMtx(m_matrix); // Everything should already be in local space
			m_pAttachedVB->BindVB();
			GRCDEVICE.DrawPrimitive(drawTris, 0,
#if RSG_PC
				Min(m_pAttachedVB->GetVertexCount(), MAX_VERT_COUNT)
#else
				m_pAttachedVB->GetVertexCount()
#endif
				);
			GRCDEVICE.ClearStreamSource(0);

			pShader->UnBind();
			pShader->EndDraw();
		}
	}

	// Render the detached triangles if any
	if (m_triangleListDetached.GetNumItems() > 0)
	{
		// Reduce the tint for falling pieces
		const float fallingTintScale = BANK_SWITCH(g_vehicleGlassMan.m_fallingTintScale, VEHICLE_GLASS_DEFAULT_FALLING_TINT_SCALE);
		Vector4 tintColor;
		tintColor.Min(m_shaderData.m_DiffuseColorTint * fallingTintScale, Vector4(1.0f));
		tintColor.w = 0.0f;
		g_vehicleGlassShader.SetTintVariable(tintColor);

		// Scale the crack tint by the glass tint but keep the alpha the way it is
		Vec4V crackTint = BANK_SWITCH(g_vehicleGlassMan.m_crackTint, VEHICLE_GLASS_DEFAULT_CRACK_TINT);
		ScalarV tintAlpha = crackTint.GetW();
		crackTint *= RCC_VEC4V(tintColor);
		crackTint.SetW(tintAlpha);

		g_vehicleGlassShader.SetCrackTextureParams(
			crackTexture,
			BANK_SWITCH(g_vehicleGlassMan.m_crackTextureAmount    , VEHICLE_GLASS_DEFAULT_CRACK_TEXTURE_AMOUNT),
			crackTextureScale,
			BANK_SWITCH(g_vehicleGlassMan.m_crackTextureBumpAmount, VEHICLE_GLASS_DEFAULT_CRACK_TEXTURE_BUMP_AMOUNT),
			BANK_SWITCH(g_vehicleGlassMan.m_crackTextureBumpiness , VEHICLE_GLASS_DEFAULT_CRACK_TEXTURE_BUMPINESS),
			crackTint
			);

		(void)Verifyf(pShader->BeginDraw(grmShader::RMC_DRAW, true) == 1, "Glass shader should only have one pass");
		pShader->Bind(0);

		view->SetWorldIdentity(); // Everything should already be in world space
		RenderTriangleList(m_triangleListDetached);

		pShader->UnBind();
		pShader->EndDraw();
	}

#if PGHANDLE_REF_COUNT
	// When PGHANDLE_REF_COUNT make sure we clear out all the textures so
	// that we don't get any false positives.
	g_vehicleGlassShader.ClearShaderVariables();
#endif // PGHANDLE_REF_COUNT

	// Restore the viewport's world matrix
	view->SetWorldMtx(prevWorldMat);

	m_isVisible[gRenderThreadInterface.GetRenderBuffer()] = false; // Clear the visibility flag
}

void CVehicleGlassComponent::RenderTriangleList(const vfxListT<CVehicleGlassTriangle>& triList) const
{
	g_vehicleGlassVtxBuffer.Begin();

	int       numTrisLeftToDraw = triList.GetNumItems();
	const int maxVtxBatchSize   = (int)g_vehicleGlassVtxBuffer.GetVtxBatchSize();
	const int maxTriBatchSize   = (int)maxVtxBatchSize/3;

	const CVehicleGlassTriangle* pTriangle = triList.GetHead();

	ASSERT_ONLY(int triBatch = 0);

	while (numTrisLeftToDraw > 0)
	{
		ASSERT_ONLY(++triBatch);
		ASSERT_ONLY(const int numTrisLeftToDraw_prev = numTrisLeftToDraw);
		const int trisToDraw = Min<int>(maxTriBatchSize, numTrisLeftToDraw);

		numTrisLeftToDraw -= trisToDraw;

		const int vtxCount = trisToDraw*3;
		g_vehicleGlassVtxBuffer.Lock(vtxCount);

		for (int vtx = 0; vtx < vtxCount; vtx += 3)
		{
			// to help catch BS#509139, i'm putting lots of info in the assert here ..
			if (!Verifyf(pTriangle, "%s: triangle is NULL, batch=%d, vtx=%d, vtxCount=%d, trisToDraw=%d, numTrisLeftToDraw=%d(%d), maxTriBatchSize=%d", GetModelName(), triBatch, vtx, vtxCount, trisToDraw, numTrisLeftToDraw, numTrisLeftToDraw_prev, maxTriBatchSize))
			{
				break;
			}

			for (int j = 0; j < 3; j++)
			{
				Vec3V pos;
				Vec4V tex;
				Vec3V nrm;
				Vec4V col;

				pos = pTriangle->GetVertexP(j); // Damage will be applied on the GPU

				pTriangle->GetVertexTNC(j, tex, nrm, col);

#if __BANK && !VEHICLE_GLASS_COMPRESSED_VERTICES
				if (g_vehicleGlassMan.m_testNormalQuantisationBits < 32)
				{
					float nx = Clamp<float>((nrm.GetXf() + 1.0f)*0.5f, 0.0f, 1.0f); // [0..1]
					float ny = Clamp<float>((nrm.GetYf() + 1.0f)*0.5f, 0.0f, 1.0f); // [0..1]
					float nz = Clamp<float>((nrm.GetZf() + 1.0f)*0.5f, 0.0f, 1.0f); // [0..1]

					const float scale = (float)((1UL<<g_vehicleGlassMan.m_testNormalQuantisationBits) - 1);

					// quantise and convert to -1..1 range
					nx = 2.0f*floorf(0.5f + nx*scale)/scale - 1.0f;
					ny = 2.0f*floorf(0.5f + ny*scale)/scale - 1.0f;
					nz = 2.0f*floorf(0.5f + nz*scale)/scale - 1.0f;

					nrm = Vec3V(nx, ny, nz);
				}
#endif // __BANK && !VEHICLE_GLASS_COMPRESSED_VERTICES

				// VtxPB requires a specific ordering here
				g_vehicleGlassVtxBuffer.BeginVertex();
				g_vehicleGlassVtxBuffer.SetPosition(RCC_VECTOR3(pos));
				g_vehicleGlassVtxBuffer.SetNormal(RCC_VECTOR3(nrm));
				g_vehicleGlassVtxBuffer.SetCPV(Color32(col));
				g_vehicleGlassVtxBuffer.SetUV(RCC_VECTOR4(tex));
				g_vehicleGlassVtxBuffer.SetUV1(Vector2(0.0f,0.0f));
				g_vehicleGlassVtxBuffer.EndVertex();
			}

			pTriangle = triList.GetNext(pTriangle);
		}

		g_vehicleGlassVtxBuffer.UnLock();
	}

	g_vehicleGlassVtxBuffer.End();
}

void CVehicleGlassComponent::SetCommonEntityStates() const
{
	Lights::UseLightsInArea(m_lightsBoundBox, false, false, false);
	CShaderLib::SetGlobalAlpha(m_renderGlobalAlpha);
	CShaderLib::SetGlobalNaturalAmbientScale(CShaderLib::DivideBy255(m_renderGlobalNaturalAmbientScale));
	CShaderLib::SetGlobalArtificialAmbientScale(CShaderLib::DivideBy255(m_renderGlobalArtificialAmbientScale));
}

bool CVehicleGlassComponent::IsVisible() const
{
#if __BANK
	if (g_vehicleGlassMan.m_forceVisible)
	{
		return true;
	}
	else
#endif // __BANK
	{
		return m_isVisible[gRenderThreadInterface.GetUpdateBuffer()];
	}
}

bool CVehicleGlassComponent::CanRender() const
{
	if( m_readytoRenderFrame == fwTimer::GetFrameCount() )
		return false;

	if (m_cracked && (m_pAttachedVB || m_triangleListDetached.GetNumItems() > 0))
	{
#if __BANK
		if (g_vehicleGlassMan.m_forceVisible)
		{
			return true;
		}
		else
#endif // __BANK
		{
			bool bVisible = m_isVisible[gRenderThreadInterface.GetCurrentBuffer()];
#if RSG_PC
			if (!bVisible && m_pAttachedVB)
			{
				m_pAttachedVB->DestroyVB();
			}
#endif
			return bVisible;
		}
	}

	return false;
}

void CVehicleGlassComponent::GetGroundInfo(const CPhysical* pPhysical, Vec4V_InOut groundPlane, bool& touchingGround, Vec3V* pHitPos) const
{
	Assert(pPhysical);

	touchingGround = false;

	if (m_triangleListDetached.GetNumItems() > 0 || pHitPos)
	{
		Vec3V groundNorm(V_ZERO);
		Vec3V groundPos(V_ZERO);

		if (pPhysical->GetIsTypeVehicle())
		{
			const CVehicle* pVehicle = static_cast<const CVehicle*>(pPhysical);
			float numTouching = 0.0f;
			const s32 numWheels = pVehicle->GetNumWheels();

			for (int i = 0; i < numWheels; i++)
			{
				if (pVehicle->GetWheel(i))
				{
					// Check if at least one wheel is touching the ground (or was before the vehicle physics was put to rest)
					if (pVehicle->GetWheel(i)->GetIsTouching() || pVehicle->GetWheel(i)->GetWasTouching())
					{
						touchingGround = true;

						// Sum the position and normal so we can find the average one
						groundPos += VECTOR3_TO_VEC3V(pVehicle->GetWheel(i)->GetHitPos());
						groundNorm += VECTOR3_TO_VEC3V(pVehicle->GetWheel(i)->GetHitNormal());
						numTouching += 1.0f;
					}
				}
			}

			// Average the position and normal
			groundPos *= ScalarV(1.0f/(float)numTouching);
			groundNorm *= ScalarV(1.0f/(float)numTouching);
		}
		else if (pPhysical->IsRecordingCollisionHistory())
		{
			const CCollisionRecord* pColRecord = pPhysical->GetFrameCollisionHistory()->GetMostSignificantCollisionRecord();

			if (pColRecord)
			{
				touchingGround = true;
				groundPos = GTA_ONLY(RCC_VEC3V)(pColRecord->m_MyCollisionPos);
				groundNorm = GTA_ONLY(RCC_VEC3V)(pColRecord->m_MyCollisionNormal);
			}
		}

		if (!touchingGround)
		{
			// If the vehicle is in the air or our tests failed, we use the bounding box
			spdAABB physicalBound;
			pPhysical->GetExtendedBoundBox(physicalBound);
			groundPos = physicalBound.GetCenter();
			groundPos.SetZ(physicalBound.GetMin().GetZ() - ScalarV(V_ONE));

			groundNorm = Vec3V(V_Z_AXIS_WZERO);
		}

		groundPlane = BuildPlane(groundPos, groundNorm);

		if (pHitPos)
		{
			*pHitPos = groundPos;
		}
	}
}

#if __BANK

const char* CVehicleGlassComponent::GetModelName() const
{
	return CBaseModelInfo::GetModelName(m_modelIndex GTA_ONLY(.Get()));
}

void CVehicleGlassComponent::RenderComponentDebug() const
{
	const CPhysical* pPhysical = m_regdPhy.Get();

	if (pPhysical)
	{
		const Vec3V vCamPos = GTA_ONLY(RCC_VEC3V)(camInterface::GetPos());

		Vec3V pos[3];
		Vec3V norm[3];
		bool exposed[3];

		if (m_pAttachedVB && m_pAttachedVB->GetVertexCount() > 0)
		{
#if USE_GPU_VEHICLEDEFORMATION
			// GPU damage is not going to work with the shader we use for debug and we don't store the damaged position in the VB
			// Instead we have to get the current damage offsets and add them to the non-damaged position
			Vec3V* damageBuffer = Alloca(Vec3V, m_pAttachedVB->GetVertexCount()); // Allocate enough room for all the offsets
			bool bUseDamage = pPhysical->GetIsTypeVehicle() && GetVehicleDamageDeformation(pPhysical, damageBuffer);
#endif // USE_GPU_VEHICLEDEFORMATION

			for (int iCurVert = 0; iCurVert < m_pAttachedVB->GetVertexCount(); iCurVert += 3)
			{
				for (int i = 0; i < 3; i++)
				{
					pos[i] = m_pAttachedVB->GetDamagedVertexP(iCurVert + i);
#if USE_GPU_VEHICLEDEFORMATION
					if (bUseDamage)
					{
						pos[i] += damageBuffer[iCurVert + i];
					}
#endif // #if USE_GPU_VEHICLEDEFORMATION

					// Get the normal and the color
					Vec4V texUV; // We don't care about the UV
					Vec4V col;
					m_pAttachedVB->GetVertexTNC(iCurVert + i, texUV, norm[i], col);

					exposed[i] = col.GetZf() < 0.01f;
				}

				bool bTessellated;
				bool bCanDetach;
				bool bHasError;

				m_pAttachedVB->GetFlags(iCurVert, bTessellated, bCanDetach, bHasError);
				RenderComponentDebugTriangle(vCamPos, pos, norm, exposed, bTessellated, bCanDetach, bHasError, true);
			}
		}

		for (const CVehicleGlassTriangle* pTriangle = m_triangleListDetached.GetHead(); pTriangle; pTriangle = m_triangleListDetached.GetNext(pTriangle))
		{
			for (int i = 0; i < 3; i++)
			{
				pos[i]     = pTriangle->GetVertexP(i);
				norm[i]    = pTriangle->GetVertexN(i);
				exposed[i] = false;
			}

			RenderComponentDebugTriangle(vCamPos, pos, norm, exposed, pTriangle->m_bTessellated, pTriangle->m_bCanDetach, pTriangle->m_bHasError, false);
		}

		if (g_vehicleGlassMan.m_renderDebugSmashSphere)
		{
			const Vec3V centre = Transform(m_matrix, m_smashSphere.GetXYZ());

			grcDebugDraw::Sphere(centre, m_smashSphere.GetWf(), Color32(255,0,0,255), false, 1, 16, true);
		}

		if (g_vehicleGlassMan.m_renderGroundPlane)
		{
			Vec4V groundPlane;
			bool touchingGround;
			Vec3V hitPos;
			GetGroundInfo(pPhysical, groundPlane, touchingGround, &hitPos);

			// Draw an arrow from the hit position in the hit normal direction
			const Vec3V groundPlaneUp = groundPlane.GetXYZ();
			grcDebugDraw::Arrow(hitPos, hitPos + groundPlaneUp * ScalarV(V_TWO), 0.1f, touchingGround ? Color_red : Color_green);

			// Render a quad representing the ground plane
			spdAABB physicalBound;
			pPhysical->GetExtendedBoundBox(physicalBound);
			Mat34V physicalMat = pPhysical->GetMatrix();

			Mat34V PlaneCoord(V_IDENTITY);
			Vec3V tempForward = Cross(groundPlaneUp, physicalMat.a());
			PlaneCoord.Seta(Cross(tempForward, groundPlaneUp));
			PlaneCoord.Setb(Cross(groundPlaneUp, PlaneCoord.a()));
			PlaneCoord.Setc(groundPlaneUp);
			PlaneCoord.Setd(hitPos);

			Vec3V v0(3.0f, 3.0f, 0.0f);
			Vec3V v1(3.0f, -3.0f, 0.0f);
			Vec3V v2(-3.0f, -3.0f, 0.0f);
			Vec3V v3(-3.0f, 3.0f, 0.0f);
			grcDebugDraw::Quad(Transform(PlaneCoord, v0), Transform(PlaneCoord, v1), Transform(PlaneCoord, v2), Transform(PlaneCoord, v3), touchingGround ? Color_red : Color_green, true, false);
		}

		if (g_vehicleGlassMan.m_renderDebugFieldOpacity > 0.0f)
		{
			const fwVehicleGlassWindowProxy* pProxy = GetWindowProxy(pPhysical);

			if (pProxy)
			{
				const fwVehicleGlassWindow* pWindow = &pProxy->GetWindow();

				if (pWindow && pWindow->m_dataRLE)
				{
					Mat34V gridToLocal;
					InvertTransformFull(gridToLocal, pWindow->m_basis);

					Vec3V localWindowNormal(V_ZERO);

					for (int iCurVer = 0; iCurVer < m_pAttachedVB->GetVertexCount(); iCurVer += 3)
					{
						localWindowNormal += m_pAttachedVB->GetVertexN(iCurVer);
					}

					if (Dot(localWindowNormal, gridToLocal.GetCol2()).Getf() < 0.0f)
					{
						gridToLocal.GetCol2Ref() *= ScalarV(V_NEGONE);
					}

					Mat34V gridToWorld;
					Transform(gridToWorld, m_matrix, gridToLocal);

					for (int j = 0; j < pWindow->m_dataRows; j++)
					{
						for (int i = 0; i < pWindow->m_dataCols; i++)
						{
							const Vec3V p00 = Transform(gridToWorld, Vec3V((float)(i + 0), (float)(j + 0), g_vehicleGlassMan.m_renderDebugFieldOffset));
							const Vec3V p10 = Transform(gridToWorld, Vec3V((float)(i + 1), (float)(j + 0), g_vehicleGlassMan.m_renderDebugFieldOffset));
							const Vec3V p01 = Transform(gridToWorld, Vec3V((float)(i + 0), (float)(j + 1), g_vehicleGlassMan.m_renderDebugFieldOffset));
							const Vec3V p11 = Transform(gridToWorld, Vec3V((float)(i + 1), (float)(j + 1), g_vehicleGlassMan.m_renderDebugFieldOffset));

							const int code = pWindow->GetDataValueCode(i, j);
							Vec3V colour(V_ONE);

							if (g_vehicleGlassMan.m_renderDebugFieldColours)
							{
								switch (code)
								{
								case 0: colour = Vec3V(V_ZERO); break;
								case 1: colour = Vec3V(1.0f, 0.0f, 1.0f); break; // inside first RLE span - magenta
								case 2: colour = Vec3V(0.0f, 1.0f, 1.0f); break; // inside second RLE span - cyan
								case 3: colour = Vec3V(V_ONE); break; // between spans - white
								}
							}

							const float value = (pWindow->GetDataValue(i, j) - pWindow->m_dataMin)/(pWindow->m_dataMax - pWindow->m_dataMin);

							grcDebugDraw::Quad(p00, p10, p11, p01, Color32(Vec4V(colour*ScalarV(value), ScalarV(g_vehicleGlassMan.m_renderDebugFieldOpacity))), true, true);
						}
					}
				}
			}
		}

		if (g_vehicleGlassMan.m_renderDebugTriCount)
		{
			const Vec3V centroidWorld = Transform(m_matrix, m_centroid);
			char strTriCount[5] = "";
			formatf(strTriCount, "%d", m_pAttachedVB ? m_pAttachedVB->GetVertexCount()/3 : 0);
			grcDebugDraw::Text(centroidWorld, Color_red, strTriCount); // Render the text
		}

		if (g_vehicleGlassMan.m_renderDebugExplosionSmashSphereHistory)
		{
			for (const atSNode<Vec4V>* pCurNode = m_listExplosionHistory.GetHead(); pCurNode; pCurNode = pCurNode->GetNext())
			{
				grcDebugDraw::Sphere(Transform(m_matrix, pCurNode->Data.GetXYZ()), pCurNode->Data.GetWf(), Color_yellow, false);
			}
		}
	}
}

void CVehicleGlassComponent::RenderComponentDebugTriangle(Vec3V_In vCamPos, const Vec3V pos[3], const Vec3V norm[3], const bool exposed[3], bool bTessellated, bool bCanDetach, bool bHasError, bool bAttached) const
{
	Color32 triangleColour(255,255,255,255);
	Color32 edgeColour(128,0,0,128);
	bool bIsTriangleError = false;

	if (bAttached)
	{
#if VEHICLE_GLASS_CLUMP_NOISE
		if (g_vehicleGlassMan.m_clumpNoiseDebugDrawAngle && m_pWindow)
		{
			const Vec3V c = (pos[0] + pos[1] + pos[2])*ScalarVConstant<FLOAT_TO_INT(1.0f/3.0f)>();
			const Vec2V v = Transform(m_pWindow->m_basis, c).GetXY() - Vec2V(0.5f*(float)m_pWindow->m_dataCols, 0.5f*(float)m_pWindow->m_dataRows);
			const float a = atan2f(v.GetYf(), v.GetXf()); // [-PI..+PI]
			const float t = 0.5f + 0.5f*a/PI; // [0..1]

			triangleColour = Color32(triangleColour.GetRGBA() + (Vec4V(0.0f,0.0f,1.0f,1.0f) - triangleColour.GetRGBA())*ScalarV(t));
			edgeColour.SetRed(triangleColour.GetRed()/2);
			edgeColour.SetGreen(triangleColour.GetGreen()/2);
			edgeColour.SetBlue(triangleColour.GetBlue()/2);
		}
		else
#endif // VEHICLE_GLASS_CLUMP_NOISE
		{
			if (bCanDetach)
			{
				if (bTessellated) { triangleColour = Color32(255,0,0,128); } // red
				else              { triangleColour = Color32(0,0,255,128); } // blue
			}
			else
			{
				if (bTessellated) { triangleColour = Color32(255,255,0,128); } // yellow
				else              { triangleColour = Color32(255,0,255,128); } // magenta
			}
		}

		if (bHasError)
		{
			triangleColour = Color32(255,0,0,255);
			bIsTriangleError = true;
		}

		if (g_vehicleGlassMan.m_renderDebugErrorsOnly && !bIsTriangleError)
		{
			return;
		}
	}

	Vec3V posWld[3];

	for (int j = 0; j < 3; j++)
	{
		posWld[j] = bAttached ? Transform(m_matrix, pos[j]) : pos[j];

		if (g_vehicleGlassMan.m_renderDebugOffsetTriangle)
		{
			posWld[j] += Normalize(vCamPos - posWld[j])*ScalarV(V_FLT_SMALL_1);
		}
	}

	Vec3V triNormal = Normalize(Cross(posWld[1] - posWld[0], posWld[2] - posWld[0])); // [TRIANGLE-NORMAL]
	Vec3V triCentre = (posWld[0] + posWld[1] + posWld[2])*ScalarVConstant<FLOAT_TO_INT(1.0f/3.0f)>();
	Vec3V triOffset = Vec3V(V_ZERO);

	if (g_vehicleGlassMan.m_renderDebugOffsetSpread != 0.0f)
	{
		triOffset += Normalize(Normalize(triCentre - m_matrix.GetCol3()) + triNormal*ScalarV(V_FLT_SMALL_1))*ScalarV(g_vehicleGlassMan.m_renderDebugOffsetSpread);
	}

	posWld[0] += triOffset;
	posWld[1] += triOffset;
	posWld[2] += triOffset;
	triCentre += triOffset;

	// check for degenerate triangles and render a sphere
	if (IsEqualAll(posWld[0], posWld[1]) |
		IsEqualAll(posWld[1], posWld[2]) |
		IsEqualAll(posWld[2], posWld[0]))
	{
		grcDebugDraw::Sphere(triCentre, 0.1f, Color32(255,0,0,255), false, 1, 16, true);
		grcDebugDraw::Line(triCentre, Vec3V(V_ZERO), Color32(255,0,0,255)); // line to world origin, in case this triangle is out of view
	}

	// skip if facing the wrong way
	if (g_vehicleGlassMan.m_disableFrontFaceRender ||
		g_vehicleGlassMan.m_disableBackFaceRender)
	{
		const float facing = Dot(triNormal, vCamPos - triCentre).Getf();

		if (g_vehicleGlassMan.m_disableFrontFaceRender && facing > 0.0f) { return; }
		if (g_vehicleGlassMan.m_disableBackFaceRender  && facing < 0.0f) { return; }
	}

	Color32 edgeColour0 = edgeColour;
	Color32 edgeColour1 = edgeColour;
	Color32 edgeColour2 = edgeColour;

	grcDebugDraw::Poly(posWld[0], posWld[1], posWld[2], triangleColour);
	grcDebugDraw::Line(posWld[0], posWld[1], edgeColour0);
	grcDebugDraw::Line(posWld[1], posWld[2], edgeColour1);
	grcDebugDraw::Line(posWld[2], posWld[0], edgeColour2);

	if (g_vehicleGlassMan.m_renderDebugTriangleNormals)
	{
		grcDebugDraw::Line(triCentre, triCentre + triNormal*ScalarV(V_FLT_SMALL_1), Color32(0,0,255,255));
	}

	if (g_vehicleGlassMan.m_renderDebugVertNormals)
	{
		for (int j = 0; j < 3; j++)
		{
			Vec3V normal = norm[j];

			if (bAttached)
			{
				normal = Transform3x3(m_matrix, normal);
			}

			grcDebugDraw::Line(posWld[j], posWld[j] + normal*ScalarV(V_FLT_SMALL_1), Color32(0,255,255,255));
		}
	}

	if (g_vehicleGlassMan.m_renderDebugExposedVert)
	{
		for (int j = 0; j < 3; j++)
		{
			if (exposed[j])
			{
				grcDebugDraw::Sphere(posWld[j], 0.02f, Color32(0,128,75,255), false, 1, 16, true);
			}
		}
	}
}

#endif // __BANK

int CVehicleGlassComponent::ProcessComponentCollision(const VfxCollInfo_s& info, bool bSmash, bool bProcessDetached)
{
	CPhysical* pPhysical = m_regdPhy.Get();

	// Don't bother if we are missing any of this stuff
	if (pPhysical == NULL || pPhysical->GetDrawable() == NULL || m_pAttachedVB == NULL || m_pAttachedVB->GetVertexCount() <= 0)
	{
		return 0;
	}
	else if (pPhysical->GetIsVisibleInSomeViewportThisFrame())
	{
		// NOTE -- We need to call SetVisible here (before UpdateComponent), otherwise
		// the first hit will produce no detached triangles .. and we need to call it
		// in UpdateComponent as well to maintain continuous visibility.
		SetVisible();
	}

#if __DEV
	if (ProcessVehicleGlassCrackTestComponent(info))
	{
		ShutdownComponent();
		return 1; // we've deleted the RegdPhy, don't complain about it being NULL
	}
#endif // __DEV

	if (IsEqualAll(info.vPositionWld, Vec3V(V_FLT_MAX)))
	{
		m_forceSmash = true;

		if (info.force == VEHICLE_GLASS_FORCE_SMASH_DETACH_ALL)
		{
#if VEHICLE_GLASS_VERBOSE_DEBUG_OUTPUT
			if (g_vehicleGlassMan.m_verboseHit)
			{
				Displayf("%s: force smash detach all", pPhysical->GetModelName());
			}
#endif // VEHICLE_GLASS_VERBOSE_DEBUG_OUTPUT

			m_forceSmashDetach = true;
		}
	}
	else if (m_nothingToDetach BANK_ONLY(&& !g_vehicleGlassMan.m_disableCanDetachTest))
	{
		// The window has no detachable triangles and we are not forcing it to smash - nothing more to do
		return 1;
	}

	int numSmashedGroups = 0;
	int numTexturedGroups = 0;

	ProcessGroupCollision(info, bSmash, bProcessDetached);

	if (m_cracked)
	{
		pPhysical->SetHiddenFlag(m_componentId);

		if( !m_readytoRenderFrame )
			m_readytoRenderFrame = fwTimer::GetFrameCount();

		numSmashedGroups++;
	}

	if (m_numDecals > 0)
	{
		numTexturedGroups++;
	}

	return numSmashedGroups + numTexturedGroups;
}

int CVehicleGlassComponent::ProcessComponentExplosion(const VfxExpInfo_s& info, bool bSmash, bool bProcessDetached)
{
	CPhysical* pPhysical = m_regdPhy.Get();

	// Don't bother if we are missing any of this stuff
	if (pPhysical == NULL || pPhysical->GetDrawable() == NULL || m_pAttachedVB == NULL || m_pAttachedVB->GetVertexCount() <= 0)
	{
		return 0;
	}
	else if (pPhysical->GetIsVisibleInSomeViewportThisFrame())
	{
		// NOTE -- We need to call SetVisible here (before UpdateComponent), otherwise
		// the first hit will produce no detached triangles .. and we need to call it
		// in UpdateComponent as well to maintain continuous visibility.
		SetVisible();
	}

	if ((info.flags & VfxExpInfo_s::EXP_FORCE_DETACH) != 0 && BANK_SWITCH(g_vehicleGlassMan.m_detachAllOnExplosion, true))
	{
		m_forceSmashDetach = true;
	}
	else if (m_nothingToDetach BANK_ONLY(&& !g_vehicleGlassMan.m_disableCanDetachTest))
	{
		// The window has no detachable triangles and we are not forcing it to smash - nothing more to do
		return 1;
	}
#if __ASSERT
	else if (pPhysical->GetIsTypeVehicle() && PARAM_vgasserts.Get())
	{
		if (m_pWindow && static_cast<const CVehicle*>(pPhysical)->GetVehicleModelInfo()->GetDoesWindowHaveExposedEdges((eHierarchyId)m_hierarchyId))
		{
			vfxAssertf(m_pWindow->GetFlags() & fwVehicleGlassWindow::VGW_FLAG_HAS_EXPOSED_EDGES, "%s: has exposed edges in modelinfo but not in frag", pPhysical->GetModelName());
		}
	}
#endif // __ASSERT

	ProcessGroupExplosion(info, bSmash, bProcessDetached);

	if (m_cracked)
	{
		pPhysical->SetHiddenFlag(m_componentId);

		if( !m_readytoRenderFrame )
			m_readytoRenderFrame = fwTimer::GetFrameCount();

		return 1;
	}

	return 0;
}

#define ProcessTessellation_GetSmashRadius(p) (m_smashSphere.GetW())

int CVehicleGlassComponent::ProcessTessellation_TestPointInSphere(Vec3V_In p) const
{
	return IsLessThanAll(MagSquared(p - m_smashSphere.GetXYZ()), square(ProcessTessellation_GetSmashRadius(BANK_ONLY(p))));
}

bool CVehicleGlassComponent::ProcessTessellation_TestTriangleInSphere(Vec3V_In P0, Vec3V_In P1, Vec3V_In P2, int BANK_ONLY(numVertsToTest)) const
{
#if __BANK
	const Vec3V smashCentre = m_smashSphere.GetXYZ();
	const float smashRadius = ProcessTessellation_GetSmashRadius((P0 + P1 + P2)*ScalarVConstant<FLOAT_TO_INT(1.0f/3.0f)>()).Getf();

	if (numVertsToTest == VEHICLE_GLASS_TEST_TRIANGLE)
	{
		const Vec3V verts[3] = {P0,P1,P2};
		const Vec3V normal = Normalize(Cross(P1 - P0, P2 - P1));

		// TODO -- pass in m_smashSphere directly if we don't need adjustment in ProcessTessellation_GetSmashRadius
		if (geomSpheres::TestSphereToTriangle(Vec4V(smashCentre, ScalarV(smashRadius)), verts, normal))
		{
			return true;
		}
	}
	else if (numVertsToTest >= VEHICLE_GLASS_TEST_1_VERTEX && numVertsToTest <= VEHICLE_GLASS_TEST_3_VERTICES)
	{
		int numVertsPassingThreshold = 0;

		numVertsPassingThreshold += ProcessTessellation_TestPointInSphere(P0);
		numVertsPassingThreshold += ProcessTessellation_TestPointInSphere(P1);
		numVertsPassingThreshold += ProcessTessellation_TestPointInSphere(P2);

		if (numVertsPassingThreshold >= numVertsToTest - VEHICLE_GLASS_TEST_1_VERTEX + 1)
		{
			return true;
		}
	}
	else if (numVertsToTest == VEHICLE_GLASS_TEST_CENTROID)
#endif // __BANK
	{
		// make sure this is the default, as this is the only codepath for non-bank
		CompileTimeAssert(VEHICLE_GLASS_TEST_DEFAULT == VEHICLE_GLASS_TEST_CENTROID);
		
		if (ProcessTessellation_TestPointInSphere((P0 + P1 + P2)*ScalarVConstant<FLOAT_TO_INT(1.0f/3.0f)>()))
		{
			return true;
		}
	}

	return false;
}

Vec3V_Out CVehicleGlassComponent::ProcessTessellation(const CPhysical* pPhysical, const VfxCollInfo_s* pCollisionInfo, float tessellationScale, bool bForceSmashDetach, bool bProcessDetached, int& numInstantBreakPieces)
{
	if (m_pAttachedVB == NULL)
	{
		numInstantBreakPieces = 0;
		return Vec3V(V_ZERO);
	}

	bool bAnyTessellated = false;
	bool bAnyDetached = false;

	CVGStackTriangle* triangleBuffer = Alloca(CVGStackTriangle, g_vehicleGlassMaxStackTriangles);
	u32 nextFreeTriangle = 0;

	if (BANK_SWITCH(g_vehicleGlassMan.m_tessellationEnabled, true))
	{
		const fwVehicleGlassWindowProxy* pProxy = GetWindowProxy(m_regdPhy.Get());
		Mat34V distFieldBasis(V_IDENTITY);
		if (pProxy)
		{
			distFieldBasis = pProxy->GetBasis(VEHICLE_GLASS_DEV_SWITCH(g_vehicleGlassMan.m_distanceFieldUncompressed, false));
		}

		const Vec3V smashCentre = m_smashSphere.GetXYZ();
		const ScalarV smashRadius = m_smashSphere.GetW();
		const u32 tessellationScaleU32 = (u32)(tessellationScale*(1<<VG_TESS_FIXED_POINT)+0.5f);

		for (int iCurVert = 0; iCurVert < m_pAttachedVB->GetVertexCount(); iCurVert += 3)
		{
			const Vec3V P0 = m_pAttachedVB->GetVertexP(iCurVert + 0);
			const Vec3V P1 = m_pAttachedVB->GetVertexP(iCurVert + 1);
			const Vec3V P2 = m_pAttachedVB->GetVertexP(iCurVert + 2);
			const Vec3V verts[3] = {P0, P1, P2};

			bool bTessellated;
			bool bCanDetach;
			BANK_ONLY(bool bHasError);
			m_pAttachedVB->GetFlags(iCurVert, bTessellated, bCanDetach BANK_ONLY(, bHasError));

			Vec3V N0, N1, N2;
			Vec4V T0, T1, T2;
			Vec4V C0, C1, C2;
			m_pAttachedVB->GetVertexTNC(iCurVert + 0, T0, N0, C0);
			m_pAttachedVB->GetVertexTNC(iCurVert + 1, T1, N1, C1);
			m_pAttachedVB->GetVertexTNC(iCurVert + 2, T2, N2, C2);

			if (!bTessellated)
			{
				Vec3V faceNormal = Normalize(Cross(P1-P0, P2-P0));
				if (BANK_ONLY(!g_vehicleGlassMan.m_tessellationSphereTest ||) geomSpheres::TestSphereToTriangle(m_smashSphere, verts, faceNormal))
				{
					AddTriangleTessellated(
						triangleBuffer, nextFreeTriangle,
						distFieldBasis,
						smashCentre, smashRadius,
						tessellationScaleU32,
						faceNormal,
						P0, P1, P2,
						T0, T1, T2,
						N0, N1, N2,
						C0, C1, C2
					);
					bAnyTessellated = true;
					continue;
				}
			}

			// Triangle is not affected - just copy it to the triangle stack array
			if (nextFreeTriangle < g_vehicleGlassMaxStackTriangles)
			{
				CVGStackTriangle& triangle = triangleBuffer[nextFreeTriangle++];
				triangle.SetVertexP(0, P0);
				triangle.SetVertexP(1, P1);
				triangle.SetVertexP(2, P2);
				triangle.SetVertexTNC(0, T0, N0, C0);
				triangle.SetVertexTNC(1, T1, N1, C1);
				triangle.SetVertexTNC(2, T2, N2, C2);
				triangle.SetFlags(bTessellated, bCanDetach BANK_ONLY(, bHasError));
			}
			else if (PARAM_vgasserts.Get())
			{
				vfxAssertf(0, "%s: (ProcessTessellation) failed to create vehicle glass triangle, overflowed stack size of %d", GetModelName(), g_vehicleGlassMaxStackTriangles);
			}
		}
#if VEHICLE_GLASS_VERBOSE_DEBUG_OUTPUT
		if (g_vehicleGlassMan.m_verboseHit)
		{
			Displayf("ProcessTessellation used %d triangles out of %d", nextFreeTriangle, g_vehicleGlassMaxStackTriangles);
		}
#endif // VEHICLE_GLASS_VERBOSE_DEBUG_OUTPUT
	}

	Vec3V vInstantBreakCentrePos(V_ZERO);
	numInstantBreakPieces = 0;

	if ((bAnyTessellated || bForceSmashDetach) BANK_ONLY(&& !g_vehicleGlassMan.m_disableDetaching))
	{
		const ScalarV forceScale(BANK_ONLY(pCollisionInfo == NULL ? 1.0f :) pCollisionInfo->force*VEHICLEGLASSPOLY_VEL_MULT_COLN);
		const Vec3V entityVelocity = GTA_ONLY(RCC_VEC3V)(pPhysical->GetVelocity());

		const bool bIsVisible = IsVisible(); // This function is not inlined
		for (int iCurTriangle = 0; iCurTriangle < nextFreeTriangle; iCurTriangle++)
		{
			CVGStackTriangle& triangle = triangleBuffer[iCurTriangle];

			if (bForceSmashDetach || triangle.CanDetach())
			{
				const int numVertsToDetach = BANK_SWITCH(g_vehicleGlassMan.m_sphereTestDetachNumVerts, (int)VEHICLE_GLASS_TEST_DEFAULT);
				const bool bSkipSphereTest = triangle.Tessellated() && BANK_SWITCH(g_vehicleGlassMan.m_skipSphereTestOnDetach, VEHICLE_GLASS_SKIP_SPHERE_ON_DETACH_DEFAULT);

				const Vec3V P0 = triangle.GetVertexP(0);
				const Vec3V P1 = triangle.GetVertexP(1);
				const Vec3V P2 = triangle.GetVertexP(2);

				// Triangles that are potentially inside the sphere will have the tessellated flag set
				if (bSkipSphereTest || ProcessTessellation_TestTriangleInSphere(P0, P1, P2, numVertsToDetach))
				{
					// When the vehicle is not visible or too far we can just remove the pieces that are about to detach so we won't have to simulate them
					if (pCollisionInfo && bProcessDetached && bIsVisible REPLAY_ONLY(&& CReplayMgr::IsPreparingFrame() == false))
					{
						// Get a new triangle
						CVehicleGlassTriangle* pTriangle = m_triangleListDetached.GetFromPool(g_vehicleGlassMan.m_trianglePool);

						if (pTriangle == NULL) // try to free some triangles from furthest component
						{
							g_vehicleGlassMan.RemoveFurthestComponent(pPhysical, this);
							pTriangle = m_triangleListDetached.GetFromPool(g_vehicleGlassMan.m_trianglePool);
						}

						if (pTriangle)
						{
							const Vec3V centroid = (P0 + P1 + P2)*ScalarVConstant<FLOAT_TO_INT(1.0f/3.0f)>();
							const ScalarV distance = Mag(centroid - m_smashSphere.GetXYZ());
							const ScalarV velFalloff = Max(ScalarV(V_ZERO), ScalarV(V_ONE) - distance/m_smashSphere.GetW());
							const ScalarV force = forceScale*velFalloff*velFalloff;
							const Vec3V velocity = pCollisionInfo->vNormalWld*force + entityVelocity;
							const ScalarV randForce = force*ScalarV(VEHICLEGLASSPOLY_VEL_RAND_COLN); // add randomness to initial velocity

							pTriangle->SetVelocity(AddScaled(velocity, g_vehicleGlassVecRandom.GetSignedVec3V(), randForce));
							pTriangle->SetSpinAxis(NormalizeSafe(g_vehicleGlassVecRandom.GetSignedVec3V(), Vec3V(V_Z_AXIS_WZERO), Vec3V(V_FLT_SMALL_6)));

							vInstantBreakCentrePos += P0;
							vInstantBreakCentrePos += P1;
							vInstantBreakCentrePos += P2;
							numInstantBreakPieces++;

							// transform positions into world space
							const Vec3V detachedP0 = Transform(m_matrix, triangle.GetVertexP(0));
							const Vec3V detachedP1 = Transform(m_matrix, triangle.GetVertexP(1));
							const Vec3V detachedP2 = Transform(m_matrix, triangle.GetVertexP(2));

							vfxAssertf(IsFiniteStable(detachedP0), "%s: vehicle glass P0 is invalid after transform (%f,%f,%f)", pPhysical->GetModelName(), VEC3V_ARGS(detachedP0));
							vfxAssertf(IsFiniteStable(detachedP1), "%s: vehicle glass P1 is invalid after transform (%f,%f,%f)", pPhysical->GetModelName(), VEC3V_ARGS(detachedP1));
							vfxAssertf(IsFiniteStable(detachedP2), "%s: vehicle glass P2 is invalid after transform (%f,%f,%f)", pPhysical->GetModelName(), VEC3V_ARGS(detachedP2));

							// Damage is now part of the world position and doesn't need to change anymore
							pTriangle->SetVertexDZero(0);
							pTriangle->SetVertexDZero(1);
							pTriangle->SetVertexDZero(2);

							// Copy all the data into the detached triangle
							Vec4V detachedT0, detachedT1, detachedT2;
							Vec3V detachedN0, detachedN1, detachedN2;
							Vec4V detachedC0, detachedC1, detachedC2;

							triangle.GetVertexTNC(0, detachedT0, detachedN0, detachedC0);
							triangle.GetVertexTNC(1, detachedT1, detachedN1, detachedC1);
							triangle.GetVertexTNC(2, detachedT2, detachedN2, detachedC2);

							pTriangle->SetVertexTNC(0, detachedT0, detachedN0, detachedC0);
							pTriangle->SetVertexTNC(1, detachedT1, detachedN1, detachedC1);
							pTriangle->SetVertexTNC(2, detachedT2, detachedN2, detachedC2);

							pTriangle->UpdatePositionsAndNormals(detachedP0, detachedP1, detachedP2);
						}
					}

					triangle.SetDetached(); // Mark as detached
					bAnyDetached = true;
				}
			}
		}
	}

	// Update the VB if anything changed
	if (bAnyTessellated || bAnyDetached)
	{
		UpdateVB(triangleBuffer, nextFreeTriangle); // Update the VB with the new triangle setup
	}

	// Check if decals should be updated
	if (bAnyDetached && m_numDecals > 0)
	{
		// Pass to the decal manager for processing
		if (!g_decalMan.VehicleGlassStateChanged(pPhysical, this))
		{
			// Info didn't get added - remove decal refs to be on the safe side
			g_decalMan.Remove(pPhysical, 0, this);
			m_numDecals = 0;
		}
	}

	if (numInstantBreakPieces > 0)
	{
		vInstantBreakCentrePos *= ScalarV(1.0f/(float)(3*numInstantBreakPieces));
	}

	return vInstantBreakCentrePos;
}

void CVehicleGlassComponent::InitComponentGeom(const grmGeometry& geom, int boneIndex)
{
#if __ASSERT
	const CPhysical* pPhysical = m_regdPhy.Get();
	const rmcDrawable* pDrawable = pPhysical->GetDrawable();
	const char* boneName = pDrawable->GetSkeletonData()->GetBoneData(boneIndex)->GetName();
	const int numBones = pDrawable->GetSkeletonData()->GetNumBones();
	Assert(boneIndex < numBones);
#endif // __ASSERT

	m_boundsMin = Vec3V(V_FLT_MAX);
	m_boundsMax = -m_boundsMin;

	// extract triangles from geom
	{
#if USE_EDGE
		if (geom.GetType() == grmGeometry::GEOMETRYEDGE)
		{
			const grmGeometryEdge* pGeomEdge = static_cast<const grmGeometryEdge*>(&geom);
#if __BANK
			// check up front how many verts are in processed geometry and assert if too many
			int numI = 0;
			int numV = 0;

			const EdgeGeomPpuConfigInfo* edgeGeomInfos = pGeomEdge->GetEdgeGeomPpuConfigInfos();
			const int count = pGeomEdge->GetEdgeGeomPpuConfigInfoCount();

			Assert(count == 1);

			for (int i = 0; i < count; i++)
			{
				numI += edgeGeomInfos[i].spuConfigInfo.numIndexes;
				numV += edgeGeomInfos[i].spuConfigInfo.numVertexes;
			}

#if __DEV
			if (numI > VEHICLE_GLASS_MAX_INDICES)
			{
				vfxAssertf(0, "%s: index buffer has more indices (%d) than system can handle (%d) (bone='%s')", pPhysical->GetModelName(), numI, VEHICLE_GLASS_MAX_INDICES, boneName);
				return;
			}

			if (numV > VEHICLE_GLASS_MAX_VERTS)
			{
				vfxAssertf(0, "%s: vertex buffer has more verts (%d) than system can handle (%d) (bone='%s')", pPhysical->GetModelName(), numV, VEHICLE_GLASS_MAX_VERTS, boneName);
				return;
			}
#endif // __DEV
#endif // __BANK

#if VEHICLE_GLASS_VERBOSE_DEBUG_OUTPUT
			if (g_vehicleGlassMan.m_verbose)
			{
				Displayf("%s: vehicle glass has %d vertices and %d indices", pPhysical->GetModelName(), numV, numI);
			}
#endif // VEHICLE_GLASS_VERBOSE_DEBUG_OUTPUT

			// Try to fetch a cache entry for the model
			int cacheIdx = g_vehicleGlassMan.GetModelCache(pGeomEdge, m_modelIndex GTA_ONLY(.Get()), m_geomIndex);
			if (cacheIdx < 0 BANK_ONLY(|| !g_vehicleGlassMan.m_bReuseExtractedGeom))
			{
				// Get a free cache entry and extract the model
				g_vehicleGlassMan.AddModelCache(pGeomEdge, m_modelIndex GTA_ONLY(.Get()), m_geomIndex);
				cacheIdx = g_vehicleGlassMan.GetModelCache(pGeomEdge, m_modelIndex GTA_ONLY(.Get()), m_geomIndex);
			}

			// Fetch the mesh data from the cache
			int BoneOffset1;
			int BoneOffset2;
			int BoneOffsetPoint;
			int BoneIndexOffset;
			int BoneIndexStride;
			u8*  BoneIndexesAndWeights = g_vehicleGlassMan.GetCacheBoneData(cacheIdx, BoneOffset1, BoneOffset2, BoneOffsetPoint, BoneIndexOffset, BoneIndexStride);

			int numIndices = g_vehicleGlassMan.GetCacheNumIndices(cacheIdx);
			int numVerts = g_vehicleGlassMan.GetCacheNumVertices(cacheIdx);

			const u16* indices = g_vehicleGlassMan.GetCacheIndices(cacheIdx);
			const Vec3V* positions = g_vehicleGlassMan.GetCachePositions(cacheIdx);
			const Vec4V* texcoords = g_vehicleGlassMan.GetCacheTexcoords(cacheIdx);
			const Vec3V* normals   = g_vehicleGlassMan.GetCacheNormals(cacheIdx);
			const Vec4V* colours   = g_vehicleGlassMan.GetCacheColors(cacheIdx);

			vfxAssertf(positions, "%s: no position vertex stream", pPhysical->GetModelName());
			vfxAssertf(texcoords, "%s: no texcoord vertex stream", pPhysical->GetModelName());
			vfxAssertf(normals, "%s: no normal vertex stream", pPhysical->GetModelName());
			vfxAssertf(colours, "%s: no colour vertex stream", pPhysical->GetModelName()); // BS#928076

			if (colours == NULL)
			{
				colours = texcoords; // just point to something else, so we don't have to check for each triangle
			}

			vfxAssertf(!(BoneIndexStride != 1 || BoneIndexOffset != 0), "%s: geometry is not hardskinned (bone='%s')", pPhysical->GetModelName(), boneName);

			CVGStackTriangle* triangleBuffer = Alloca(CVGStackTriangle, g_vehicleGlassMaxStackTriangles);
			u32 nextFreeTriangle = 0;

			for (int i = 0; i < numIndices; i += 3)
			{
				const int index0 = (int)indices[i + 0];
				const int index1 = (int)indices[i + 1];
				const int index2 = (int)indices[i + 2];

				int blendBoneIndex = BoneIndexesAndWeights[index0*BoneIndexStride + BoneIndexOffset];

				if (blendBoneIndex >= BoneOffsetPoint)
				{
					blendBoneIndex += BoneOffset2 - BoneOffsetPoint;
				}
				else
				{
					blendBoneIndex += BoneOffset1;
				}

				vfxAssertf(blendBoneIndex >= 0 && blendBoneIndex < numBones, "%s: out of range blend bone index (%d), expected [0..%d) (bone='%s')", pPhysical->GetModelName(), blendBoneIndex, numBones, boneName);

				if (blendBoneIndex == boneIndex)
				{
					const Vec4V pnpn = MergeXY(Vec4V(V_ONE), Vec4V(V_NEGONE));

					Vec4V texcoord0 = AddScaled(Vec4V(V_Y_AXIS_WONE), texcoords[index0], pnpn);
					Vec4V texcoord1 = AddScaled(Vec4V(V_Y_AXIS_WONE), texcoords[index1], pnpn);
					Vec4V texcoord2 = AddScaled(Vec4V(V_Y_AXIS_WONE), texcoords[index2], pnpn);

#if !VEHICLE_GLASS_COMPRESSED_VERTICES
#if VEHICLE_GLASS_SMASH_TEST
					if (!g_vehicleGlassSmashTest.m_smashTest) // don't do this if we're running the smash test, we might need the raw texcoords
#endif // VEHICLE_GLASS_SMASH_TEST
					{
						texcoord0 = Clamp(texcoord0, Vec4V(V_ZERO), Vec4V(V_ONE)); // clamp texcoords, because we might want to compress as fixed point in the future
						texcoord1 = Clamp(texcoord1, Vec4V(V_ZERO), Vec4V(V_ONE));
						texcoord2 = Clamp(texcoord2, Vec4V(V_ZERO), Vec4V(V_ONE));
					}
#endif // !VEHICLE_GLASS_COMPRESSED_VERTICES

					AddTriangle(
						triangleBuffer,
						nextFreeTriangle,
						positions[index0],
						positions[index1],
						positions[index2],
						texcoord0,
						texcoord1,
						texcoord2,
						normals[index0],
						normals[index1],
						normals[index2],
						colours[index0],
						colours[index1],
						colours[index2]
					);

					m_boundsMin = Min(positions[index0], positions[index1], positions[index2], m_boundsMin);
					m_boundsMax = Max(positions[index0], positions[index1], positions[index2], m_boundsMax);
				}
			}

#if VEHICLE_GLASS_VERBOSE_DEBUG_OUTPUT
			if (g_vehicleGlassMan.m_verboseHit)
			{
				Displayf("%s: AddTriangleTessellated used %d triangles out of %d", pPhysical->GetModelName(), nextFreeTriangle, g_vehicleGlassMaxStackTriangles);
			}
#endif // VEHICLE_GLASS_VERBOSE_DEBUG_OUTPUT

			UpdateVB(triangleBuffer, nextFreeTriangle); // Update the VB with the new triangle setup
		}
		else
#endif // USE_EDGE
		{
			// Note that the index buffer must be locked before any vertex buffers
			// are locked (e.g. with grcVertexBufferEditor).  This ordering is to
			// prevent deadlocks occurring when two threads lock in opposite order.
			const grcIndexBuffer* pIndexBuffer = geom.GetIndexBuffer();
			const grcVertexBuffer* pVertexBuffer = geom.GetVertexBuffer();
			const u16* pIndexPtr = pIndexBuffer->LockRO();
			const u16* pMatrixPalette = geom.GetMatrixPalette();

			vfxAssertf(pVertexBuffer->GetFvf()->GetTextureChannel(0)   , "%s: no vertex UV[0] channel (bone='%s')", pPhysical->GetModelName(), boneName);
			vfxAssertf(pVertexBuffer->GetFvf()->GetTextureChannel(1)   , "%s: no vertex UV[1] channel (bone='%s')", pPhysical->GetModelName(), boneName);
			vfxAssertf(pVertexBuffer->GetFvf()->GetNormalChannel()     , "%s: no vertex normal channel (bone='%s')", pPhysical->GetModelName(), boneName);
			vfxAssertf(pVertexBuffer->GetFvf()->GetDiffuseChannel()    , "%s: no vertex colour channel (bone='%s')", pPhysical->GetModelName(), boneName);
			vfxAssertf(pVertexBuffer->GetFvf()->GetBlendWeightChannel(), "%s: no vertex blend weight channel (bone='%s')", pPhysical->GetModelName(), boneName);
			vfxAssertf(pVertexBuffer->GetFvf()->GetBindingsChannel()   , "%s: no vertex blend index channel (bone='%s')", pPhysical->GetModelName(), boneName);

			const bool bHasColours = pVertexBuffer->GetFvf()->GetDiffuseChannel(); // temp hack for vehicles without colour channel (BS#928076)

			WIN32PC_ONLY(if (pIndexPtr))
			{
				PS3_ONLY(++g_AllowVertexBufferVramLocks); // PS3: attempting to edit VBs in VRAM
				grcVertexBufferEditor* pvbEditor = rage_new grcVertexBufferEditor(const_cast<grcVertexBuffer*>(pVertexBuffer), true, true);

				WIN32PC_ONLY(if (pvbEditor->isValid()))
				{
					CVGStackTriangle* triangleBuffer = Alloca(CVGStackTriangle, g_vehicleGlassMaxStackTriangles);
					u32 nextFreeTriangle = 0;

#if VEHICLE_GLASS_VERBOSE_DEBUG_OUTPUT
					if (g_vehicleGlassMan.m_verbose)
					{
						Displayf("Vehicle glass for model %s has %d vertices and %d indices", pPhysical->GetModelName(), pvbEditor->GetVertexBuffer()->GetVertexCount(), pIndexBuffer->GetIndexCount());
					}
#endif // VEHICLE_GLASS_VERBOSE_DEBUG_OUTPUT
	

					for (int i = 0; i < pIndexBuffer->GetIndexCount(); i += 3)
					{
						const int index0 = (int)pIndexPtr[i + 0];
						const int index1 = (int)pIndexPtr[i + 1];
						const int index2 = (int)pIndexPtr[i + 2];

						const int blendBoneIndex = pMatrixPalette[pvbEditor->GetBlendIndices(index0).GetRed()];

						if (blendBoneIndex == boneIndex)
						{
							const Vec3V position0 = VECTOR3_TO_VEC3V(pvbEditor->GetPosition(index0));
							const Vec3V position1 = VECTOR3_TO_VEC3V(pvbEditor->GetPosition(index1));
							const Vec3V position2 = VECTOR3_TO_VEC3V(pvbEditor->GetPosition(index2));

							const Vector2 uv0 = pvbEditor->GetUV(index0, 0);
							const Vector2 uv1 = pvbEditor->GetUV(index1, 0);
							const Vector2 uv2 = pvbEditor->GetUV(index2, 0);

							const Vector2 uv0_sec = pvbEditor->GetUV(index0, 1);
							const Vector2 uv1_sec = pvbEditor->GetUV(index1, 1);
							const Vector2 uv2_sec = pvbEditor->GetUV(index2, 1);

							Vec4V texcoord0 = Vec4V(uv0.x, 1.0f - uv0.y, uv0_sec.x, 1.0f - uv0_sec.y);
							Vec4V texcoord1 = Vec4V(uv1.x, 1.0f - uv1.y, uv1_sec.x, 1.0f - uv1_sec.y);
							Vec4V texcoord2 = Vec4V(uv2.x, 1.0f - uv2.y, uv2_sec.x, 1.0f - uv2_sec.y);

#if !VEHICLE_GLASS_COMPRESSED_VERTICES
#if VEHICLE_GLASS_SMASH_TEST
							if (!g_vehicleGlassSmashTest.m_smashTest) // don't do this if we're running the smash test, we might need the raw texcoords
#endif // VEHICLE_GLASS_SMASH_TEST
							{
							#if 0 
								// B*1868876 - Snow covered Burrito van glass has -ve UV coords, clamping below causes texture streaks when broken.
								// Not sure we need to clamp anyway - compressed code path doesn't.
								texcoord0 = Clamp(texcoord0, Vec4V(V_ZERO), Vec4V(V_ONE)); // clamp texcoords, because we might want to compress as fixed point in the future
								texcoord1 = Clamp(texcoord1, Vec4V(V_ZERO), Vec4V(V_ONE));
								texcoord2 = Clamp(texcoord2, Vec4V(V_ZERO), Vec4V(V_ONE));
							#endif // 0
							}
#endif // !VEHICLE_GLASS_COMPRESSED_VERTICES

							Vec4V colour0 = bHasColours ? pvbEditor->GetDiffuse(index0).GetRGBA() : Vec4V(V_ONE);
							Vec4V colour1 = bHasColours ? pvbEditor->GetDiffuse(index1).GetRGBA() : Vec4V(V_ONE);
							Vec4V colour2 = bHasColours ? pvbEditor->GetDiffuse(index2).GetRGBA() : Vec4V(V_ONE);

							AddTriangle(
								triangleBuffer,
								nextFreeTriangle,
								position0,
								position1,
								position2,
								texcoord0,
								texcoord1,
								texcoord2,
								VECTOR3_TO_VEC3V(pvbEditor->GetNormal(index0)),
								VECTOR3_TO_VEC3V(pvbEditor->GetNormal(index1)),
								VECTOR3_TO_VEC3V(pvbEditor->GetNormal(index2)),
								colour0,
								colour1,
								colour2
							);

							m_boundsMin = Min(position0, position1, position2, m_boundsMin);
							m_boundsMax = Max(position0, position1, position2, m_boundsMax);
						}
					}
#if VEHICLE_GLASS_VERBOSE_DEBUG_OUTPUT
					if (g_vehicleGlassMan.m_verboseHit)
					{
						Displayf("AddTriangleTessellated used %d triangles out of %d", nextFreeTriangle, g_vehicleGlassMaxStackTriangles);
					}
#endif // VEHICLE_GLASS_VERBOSE_DEBUG_OUTPUT

					UpdateVB(triangleBuffer, nextFreeTriangle); // Update the VB with the new triangle setup
				}
#if RSG_PC
				else
				{
					vfxAssertf(0, "%s: failed to lock vertex buffer for vehicle glass (bone='%s')", pPhysical->GetModelName(), boneName);
					pIndexBuffer->UnlockRO();
					delete pvbEditor;
					return;
				}
#endif // RSG_PC
				PS3_ONLY(--g_AllowVertexBufferVramLocks); // PS3: finished with editing VBs in VRAM
				pIndexBuffer->UnlockRO();
				delete pvbEditor;
			}
#if RSG_PC
			else
			{
				vfxAssertf(0, "%s: failed to lock index buffer for vehicle glass (bone='%s')", pPhysical->GetModelName(), boneName);
				return;
			}
#endif // RSG_PC
		}
	}

	m_centroid = (m_boundsMax + m_boundsMin)*ScalarV(V_HALF);
}

int CVehicleGlassComponent::GetNumTriangles(bool bOnlyAttachedTriangles) const
{
	const int numAttachedTriangles = m_pAttachedVB ? m_pAttachedVB->GetVertexCount()/3 : 0;

	if (bOnlyAttachedTriangles)
	{
		return numAttachedTriangles;
	}
	else
	{
		return numAttachedTriangles + m_triangleListDetached.GetNumItems();
	}
}

const fwVehicleGlassWindow* CVehicleGlassComponent::GetWindow(const CPhysical* pPhysical) const
{
	const fwVehicleGlassWindow* pWindow = NULL;
#if __ASSERT
	bool bWindowDataMissing = false;
#endif // __ASSERT

#if VEHICLE_GLASS_DEV
	const fragType* pFragType = pPhysical->GetFragInst()->GetType();
	bool bIsFromHD = false;
	bool bAllowExposedEdges = true;

	// allow exposed edges only if existing window data indicates that vehicle has been exported since we started packing the exposed edge channel in cpv.b
	{
		const fwVehicleGlassWindowData* pWindowData = static_cast<const gtaFragType*>(pPhysical->GetFragInst()->GetType())->m_vehicleWindowData;

		if (pWindowData)
		{
			const fwVehicleGlassWindow* pExistingWindow = fwVehicleGlassWindowData::GetWindow(pWindowData, m_componentId);

			if (pExistingWindow)
			{
				const u32 existingFlags = pExistingWindow->GetFlags();
				const u32 existingVersion = (existingFlags & fwVehicleGlassWindow::VGW_FLAG_VERSION_MASK) >> fwVehicleGlassWindow::VGW_FLAG_VERSION_SHIFT;

				if (existingVersion >= 2 && (existingFlags & fwVehicleGlassWindow::VGW_FLAG_HAS_EXPOSED_EDGES) != 0)
				{
					bAllowExposedEdges = true;
				}
			}
		}
	}

	if (g_vehicleGlassMan.m_useHDDrawable && pPhysical->GetIsTypeVehicle())
	{
		const CVehicleModelInfo* pVMI = static_cast<const CVehicleModelInfo*>(pPhysical->GetBaseModelInfo());

		if (pVMI && pVMI->GetHDFragType())
		{
			pFragType = pVMI->GetHDFragType();
			bIsFromHD = true;
		}
	}

	if (g_vehicleGlassMan.m_distanceFieldSource == VGW_SOURCE_DEBUG_STORAGE_WINDOW_PROXY)
	{
		const fwVehicleGlassWindowProxy* pProxy = fwVehicleGlassConstructorInterface::GetOrCreateNewWindowProxy(
			pPhysical->GetModelName(),
			pFragType,
			g_vehicleGlassMan.m_useHDDrawable ? LOD_HIGH : LOD_MED,
			m_modelIndex GTA_ONLY(.Get()),
			m_componentId,
			true, // bCreate
			g_vehicleGlassMan.m_distanceFieldOutputFiles,
			bIsFromHD,
			bAllowExposedEdges
		);

		if (pProxy)
		{
			pWindow = &pProxy->GetWindow();
		}
	}
	else if (g_vehicleGlassMan.m_distanceFieldSource == VGW_SOURCE_DEBUG_STORAGE_WINDOW_DATA)
	{
		const fwVehicleGlassWindowData* pWindowData = fwVehicleGlassConstructorInterface::GetOrCreateNewWindowData(
			pPhysical->GetModelName(),
			pFragType,
			g_vehicleGlassMan.m_useHDDrawable ? LOD_HIGH : LOD_MED,
			m_modelIndex GTA_ONLY(.Get()),
			true, // bCreate
			g_vehicleGlassMan.m_distanceFieldOutputFiles,
			bIsFromHD,
			bAllowExposedEdges
		);

		if (pWindowData)
		{
			pWindow = fwVehicleGlassWindowData::GetWindow(pWindowData, m_componentId);
		}
		else
		{
			bWindowDataMissing = true;
		}
	}
	else if (g_vehicleGlassMan.m_distanceFieldSource == VGW_SOURCE_VEHICLE_MODEL_WINDOW_DATA)
#endif // VEHICLE_GLASS_DEV
	{
		const fwVehicleGlassWindowData* pWindowData = static_cast<const gtaFragType*>(pPhysical->GetFragInst()->GetType())->m_vehicleWindowData;

		if (Verifyf(pWindowData, "%s: missing window data", pPhysical->GetModelName()))
		{
			pWindow = fwVehicleGlassWindowData::GetWindow(pWindowData, m_componentId);

#if __ASSERT
			//if (PARAM_vgasserts.Get())
			{
				if (pWindow)
				{
					if (pWindow->GetVersion() == 0)
					{
						if (pWindow->m_dataRLE)
						{
							vfxAssertf(
								0, "%s: window for componentId=%d (bone='%s') is version %d (flags = 0x%08x), please re-convert this vehicle",
								pPhysical->GetModelName(),
								m_componentId,
								pPhysical->GetDrawable()->GetSkeletonData()->GetBoneData(m_boneIndex)->GetName(),
								pWindow->GetVersion(),
								pWindow->GetFlags()
							);
						}
						else
						{
							// it's probably a bulletproof window (with no distance field).
							// these windows didn't get the version number saved in them, so
							// i'm skipping the assert in this case
						}
					}

					if (pWindow->GetFlags() & fwVehicleGlassWindow::VGW_FLAG_ERROR_MASK)
					{
						char errorFlagsStr[512] = "";

						if (pWindow->GetFlags() & fwVehicleGlassWindow::VGW_FLAG_ERROR_MULTIPLE_MATERIALS ) { strcat(errorFlagsStr, "\n\t* multiple materials"); }
						if (pWindow->GetFlags() & fwVehicleGlassWindow::VGW_FLAG_ERROR_MIXED_MATERIALS    ) { strcat(errorFlagsStr, "\n\t* mixed materials"); }
						if (pWindow->GetFlags() & fwVehicleGlassWindow::VGW_FLAG_ERROR_MULTIPLE_GEOMETRIES) { strcat(errorFlagsStr, "\n\t* multiple geometries"); }
						if (pWindow->GetFlags() & fwVehicleGlassWindow::VGW_FLAG_ERROR_DAMAGE_SCALE_NOT_01) { strcat(errorFlagsStr, "\n\t* exposed edge vertex painting not 0,1"); }
						if (pWindow->GetFlags() & fwVehicleGlassWindow::VGW_FLAG_ERROR_NO_BOUNDARY_EDGES  ) { strcat(errorFlagsStr, "\n\t* no boundary edges (double-sided?)"); }

						vfxAssertf(
							0, "%s: window for componentId=%d (bone='%s') has error flags 0x%08x%s",
							pPhysical->GetModelName(),
							m_componentId,
							pPhysical->GetDrawable()->GetSkeletonData()->GetBoneData(m_boneIndex)->GetName(),
							pWindow->GetFlags() & fwVehicleGlassWindow::VGW_FLAG_ERROR_MASK,
							errorFlagsStr
						);
					}
				}
			}
#endif // __ASSERT
		}
#if __ASSERT
		else
		{
			bWindowDataMissing = true;
		}
#endif // __ASSERT
	}

#if __ASSERT
	if (pWindow == NULL && !bWindowDataMissing)
	{
		vfxAssertf(
			0, "%s: missing window for componentId=%d (bone='%s')",
			pPhysical->GetModelName(),
			m_componentId,
			pPhysical->GetDrawable()->GetSkeletonData()->GetBoneData(m_boneIndex)->GetName()
		);
	}
#endif // __ASSERT

	return pWindow;
}

const fwVehicleGlassWindowProxy* CVehicleGlassComponent::GetWindowProxy(const CPhysical* VEHICLE_GLASS_DEV_ONLY(pPhysical)) const
{
#if VEHICLE_GLASS_DEV
	static fwVehicleGlassWindowProxy s_windowProxy;
	const fwVehicleGlassWindowProxy* pProxy = NULL;

	bool bIsFromHD = false;
	bool bAllowExposedEdges = true;

	// allow exposed edges only if existing window data indicates that vehicle has been exported since we started packing the exposed edge channel in cpv.b
	{
		const fwVehicleGlassWindowData* pWindowData = static_cast<const gtaFragType*>(pPhysical->GetFragInst()->GetType())->m_vehicleWindowData;

		if (pWindowData)
		{
			const fwVehicleGlassWindow* pExistingWindow = fwVehicleGlassWindowData::GetWindow(pWindowData, m_componentId);

			if (pExistingWindow)
			{
				const u32 existingFlags = pExistingWindow->GetFlags();
				const u32 existingVersion = (existingFlags & fwVehicleGlassWindow::VGW_FLAG_VERSION_MASK) >> fwVehicleGlassWindow::VGW_FLAG_VERSION_SHIFT;

				if (existingVersion >= 2 && (existingFlags & fwVehicleGlassWindow::VGW_FLAG_HAS_EXPOSED_EDGES) != 0)
				{
					bAllowExposedEdges = true;
				}
			}
		}
	}

	if (g_vehicleGlassMan.m_distanceFieldSource == VGW_SOURCE_DEBUG_STORAGE_WINDOW_PROXY)
	{
		pProxy = fwVehicleGlassConstructorInterface::GetOrCreateNewWindowProxy(
			pPhysical->GetModelName(),
			pPhysical->GetFragInst()->GetType(),
			g_vehicleGlassMan.m_useHDDrawable ? LOD_HIGH : LOD_MED,
			m_modelIndex GTA_ONLY(.Get()),
			m_componentId,
			true, // bCreate
			g_vehicleGlassMan.m_distanceFieldOutputFiles,
			bIsFromHD,
			bAllowExposedEdges
		);
	}
	else if (g_vehicleGlassMan.m_distanceFieldSource == VGW_SOURCE_DEBUG_STORAGE_WINDOW_DATA)
	{
		const fwVehicleGlassWindowData* pWindowData = fwVehicleGlassConstructorInterface::GetOrCreateNewWindowData(
			pPhysical->GetModelName(),
			pPhysical->GetFragInst()->GetType(),
			g_vehicleGlassMan.m_useHDDrawable ? LOD_HIGH : LOD_MED,
			m_modelIndex GTA_ONLY(.Get()),
			true, // bCreate
			g_vehicleGlassMan.m_distanceFieldOutputFiles,
			bIsFromHD,
			bAllowExposedEdges
		);

		if (pWindowData)
		{
			const fwVehicleGlassWindow* pWindow = fwVehicleGlassWindowData::GetWindow(pWindowData, m_componentId);

			if (pWindow)
			{
				s_windowProxy.m_data = NULL;
				s_windowProxy.m_window = const_cast<fwVehicleGlassWindow*>(pWindow);
				pProxy = &s_windowProxy;
			}
		}
	}
	else if (g_vehicleGlassMan.m_distanceFieldSource == VGW_SOURCE_VEHICLE_MODEL_WINDOW_DATA)
	{
		if (m_pWindow)
		{
			s_windowProxy.m_data = NULL;
			s_windowProxy.m_window = const_cast<fwVehicleGlassWindow*>(m_pWindow);
			pProxy = &s_windowProxy;
		}
	}

	return pProxy;
#else
	return m_pWindow;
#endif
}

void CVehicleGlassComponent::ProcessGroupCollision(const VfxCollInfo_s& info, bool DEV_ONLY(bSmash), bool bProcessDetached)
{
	const CPhysical* pPhysical = m_regdPhy.Get();

	Assert(m_boneIndex >= 0);
	CVfxHelper::GetMatrixFromBoneIndex_Smash(m_matrix, pPhysical, m_boneIndex);

	if (IsZeroAll(m_matrix.GetCol0()))
	{
		return;
	}

#if __ASSERT
	const float det = Determinant3x3(m_matrix).Getf();

	if (det < 0.0f)// && PARAM_vgasserts.Get())
	{
		const char* boneName = pPhysical->GetDrawable()->GetSkeletonData()->GetBoneData(m_boneIndex)->GetName();

		// e.g. BS#841877
		vfxAssertf(0, "%s: bone matrix is flipped (componentId=%d, boneIndex=%d, bone='%s', det=%f)", pPhysical->GetModelName(), m_componentId, m_boneIndex, boneName, det);
	}
#endif // __ASSERT

	Vec3V vSmashPosWld = info.vPositionWld;
	Vec3V vSmashPosObj = UnTransformOrtho(m_matrix, vSmashPosWld);

	if (m_forceSmash) // if we passed in a 'fake' collision to force a smash then calc the real position now
	{
		int numVerts = 0;
		vSmashPosObj = Vec3V(V_ZERO);

		if (m_pAttachedVB)
		{
			for (; numVerts < m_pAttachedVB->GetVertexCount(); numVerts++)
			{
				vSmashPosObj += m_pAttachedVB->GetVertexP(numVerts);
			}
		}

		if (numVerts > 0)
		{
			vSmashPosObj *= ScalarV(1.0f/(float)numVerts);
		}
		else
		{
			return;
		}

		vSmashPosWld = Transform(m_matrix, vSmashPosObj);
	}

	// TODO -- what about explosions? should we not test the full sphere?
	const ScalarV r = m_smashSphere.GetW();

	// When not forcing a smash, skip this hit if its fully inside the smashed area as there is not glass to break there
	if (!m_forceSmash && IsLessThanOrEqualAll(MagSquared(vSmashPosObj - m_smashSphere.GetXYZ()), r*r))
	{
		return;
	}

	// calc radius of smash based on force etc.

	const float radius = m_forceSmash ? VEHICLEGLASSGROUP_SMASH_RADIUS : info.force*VEHICLEGLASSGROUP_RADIUS_MULT;
	vfxAssertf(radius > 0.0f, "has a non-positive radius");

	phMaterialMgr::Id mtlId = PGTAMATERIALMGR->UnpackMtlId(info.materialId);

#if __BANK
	if (g_vehicleGlassMan.m_allGlassIsWeak)
	{
		mtlId = PGTAMATERIALMGR->g_idCarGlassWeak;
	}
	// url:bugstar:1823575
	// MN - removing this assert from network games due to it being hit now and again when some dodgy looking faked probes are passed down
	//      these can originate from CWeapon::ReceiveFireMessage in network games where the probe results have their hit component overriden by the component passed into that function
	//      after discussing with Dan/Richard/Chris we have decided that the code in CWeapon::ReceiveFireMessage is best left alone and this assert is removed instead
#if __ASSERT
	else if (m_materialId != mtlId && !CNetwork::IsGameInProgress())// && PARAM_vgasserts.Get())
	{
		const char* boneName = pPhysical->GetDrawable()->GetSkeletonData()->GetBoneData(m_boneIndex)->GetName();

		char mtlIdName1[128] = "";
		PGTAMATERIALMGR->GetMaterialName(mtlId, mtlIdName1, sizeof(mtlIdName1));

		char mtlIdName2[128] = "";
		PGTAMATERIALMGR->GetMaterialName(m_materialId, mtlIdName2, sizeof(mtlIdName2));

		vfxAssertf(0, "%s: collision info material = %s, but material for componentId=%d, boneIndex=%d, bone='%s' is \"%s\"", pPhysical->GetModelName(), mtlIdName1, m_componentId, m_boneIndex, boneName, mtlIdName2);
	}
#endif // __ASSERT
#endif // __BANK

#if VEHICLE_GLASS_VERBOSE_DEBUG_OUTPUT
	if (g_vehicleGlassMan.m_verbose ||
		g_vehicleGlassMan.m_verboseHit)
	{
		const char* boneName = pPhysical->GetDrawable()->GetSkeletonData()->GetBoneData(m_boneIndex)->GetName();

		char mtlIdName[128] = "";
		PGTAMATERIALMGR->GetMaterialName(m_materialId, mtlIdName, sizeof(mtlIdName));
		Displayf("Collision hit componentId=%d '%s' [%s] on %s, radius=%f%s", m_componentId, boneName, mtlIdName, pPhysical->GetModelName(), radius, m_forceSmashDetach ? " - force smash detach" : m_forceSmash ? " - force smash" : DEV_SWITCH(bSmash, false) ? " - smash" : "");

	}
#endif // VEHICLE_GLASS_VERBOSE_DEBUG_OUTPUT
	DEV_ONLY((void)bSmash); // don't complain about bSmash being unreferenced please

	// should we apply a decal?
	bool bApplyDecal = false;
	bool bApplyBulletproofGlassTextureSwap = false;

	bool bHasBulletResistantGlass = false;
	bool bHasUnsmashableBulletproofGlass = false;
	if (pPhysical->GetIsTypeVehicle())
	{
		const CVehicle* pVehicle = static_cast<const CVehicle*>(pPhysical);
		bHasBulletResistantGlass = pVehicle && pVehicle->HasBulletResistantGlass();
		bHasUnsmashableBulletproofGlass = MI_PLANE_AVENGER.IsValid() && pVehicle && pVehicle->GetModelIndex() == MI_PLANE_AVENGER;
	}

	if (!m_smashed)
	{
		bool bCanSmash = true;

		if (info.isSnowball && !m_cracked)
		{
			// never smash windows with snowballs unless already weakened
			bCanSmash = false;
			bApplyDecal = true;
		}
		else if ((MI_HELI_HAVOK.IsValid() && pPhysical->GetModelIndex()==MI_HELI_HAVOK) || 
			(MI_PLANE_SEABREEZE.IsValid() && pPhysical->GetModelIndex()==MI_PLANE_SEABREEZE) ||
			(MI_CAR_STROMBERG.IsValid() && pPhysical->GetModelIndex()==MI_CAR_STROMBERG) || 
			(MI_HELI_SEASPARROW.IsValid() && pPhysical->GetModelIndex()==MI_HELI_SEASPARROW)||
			(MI_CAR_TOREADOR.IsValid() && pPhysical->GetModelIndex()==MI_CAR_TOREADOR))
		{
			if (IsEqualAll(info.vPositionWld, Vec3V(V_FLT_MAX)))
			{
				return;
			}

			bCanSmash = false;
			bApplyDecal = true;
		}
		else if (info.isFMJAmmo)
		{
			if (mtlId == PGTAMATERIALMGR->g_idCarGlassBulletproof || mtlId == PGTAMATERIALMGR->g_idCarGlassOpaque)
			{
				if (bHasUnsmashableBulletproofGlass)
				{
					// Some vehicles have bulletproof glass that we never want to break with FMJ ammo
					bApplyDecal = true;
					bCanSmash = false;
				}
				else if (m_numDecals < (sm_iSpecialAmmoFMJVehicleBulletproofGlassMinHitsToSmash - 1) || (m_numDecals < (sm_iSpecialAmmoFMJVehicleBulletproofGlassMaxHitsToSmash - 1) && g_DrawRand.GetRanged(0.0f, 1.0f) > sm_fSpecialAmmoFMJVehicleBulletproofGlassRandomChanceToSmash))
				{
					bApplyDecal = true;
				}
			}
			else
			{
				m_fArmouredGlassHealth = 0.0f;
				bApplyDecal = false;
				bCanSmash = true;	
			}				
		}
		else if (mtlId == PGTAMATERIALMGR->g_idCarGlassMedium)
		{
			if (m_numDecals < 2 || (m_numDecals < 6 && g_DrawRand.GetRanged(0.0f, 1.0f) > 0.3f))
			{
				bApplyDecal = true;
			}
		}
		else if (mtlId == PGTAMATERIALMGR->g_idCarGlassStrong)
		{
			CBaseModelInfo* pModelInfo = pPhysical->GetBaseModelInfo();
			if (pModelInfo && (pModelInfo->GetModelNameHash()==atHashValue("INSURGENT", 0x9114EADA) || 
							   pModelInfo->GetModelNameHash()==atHashValue("INSURGENT2", 0x7B7E56F0) ||
							   pModelInfo->GetModelNameHash()==atHashValue("INSURGENT3", 0x8D4B7A8A) ))
			{
				if (m_numDecals < 12 || (m_numDecals < 15 && g_DrawRand.GetRanged(0.0f, 1.0f) > 0.3f))
				{
					bApplyDecal = true;
				}
			}
			else if (bHasBulletResistantGlass)
			{
				if (info.armouredGlassShotByPedInsideVehicle)
				{
					// Instantly smash out the glass when shooting from FPS mode inside your own vehicle.
					m_fArmouredGlassHealth = 0.0f;
					bApplyDecal = false;
					bCanSmash = true;
				}
				else
				{
					// Don't apply decals if doing a force smash, set glass health to 0.0f.
					if (m_forceSmash)
					{
						bCanSmash = true;
						bApplyDecal = false;
						m_fArmouredGlassHealth = 0.0f;
					}
					else
					{
						// Apply weapon damage to the glass.
						m_fArmouredGlassHealth -= (MAX_ARMOURED_GLASS_HEALTH * (info.armouredGlassWeaponDamage / 100));

						if (m_fArmouredGlassHealth < 0.0f)
						{
							m_fArmouredGlassHealth = 0.0f;
						}

						// Apply decal and don't smash if window still has enough health left.
						if (m_fArmouredGlassHealth > 0.0f)
						{
							m_armouredGlassPenetrationDecalCount++;
							bCanSmash = false;
							bApplyDecal = true;
							bApplyBulletproofGlassTextureSwap = true;
						}
					}
				}
			}
			else
			{
				if (m_numDecals < 6 || (m_numDecals < 10 && g_DrawRand.GetRanged(0.0f, 1.0f) > 0.3f))
				{
					bApplyDecal = true;
				}
			}
		}
		else if (mtlId != PGTAMATERIALMGR->g_idCarGlassWeak) // e.g. bulletproof or opaque
		{
			if (m_forceSmash)
			{
				// This glass type doesn't smash and our position is invalid
				// Just indicate it didn't smash and return
				m_forceSmash = false;
				return;
			}
			else
			{
				bApplyDecal = true;
				bCanSmash = false; // This window doesn't smash by a collision
			}
		}

		if (bCanSmash)
		{
			// don't apply a texture if we're forcing a smash
			// don't apply a texture if the force is above the smash threshold
			if (m_forceSmash || info.force > VEHICLEGLASSFORCE_NO_TEXTURE_THRESH)
			{
				bApplyDecal = false;
				m_cracked = true;
			}
		}
	}

#if __BANK
	if (g_vehicleGlassMan.m_onlyApplyDecals)
	{
		bApplyDecal = true;
	}
	else if (g_vehicleGlassMan.m_neverApplyDecals)
	{
		bApplyDecal = false;
	}
#endif // __BANK

	if (bApplyDecal)
	{
		VfxWeaponInfo_s* pFxWeaponInfo = g_vfxWeapon.GetInfo(PGTAMATERIALMGR->GetMtlVfxGroup(mtlId), info.weaponGroup, info.isBloody);

		if (pFxWeaponInfo)
		{
			if (smashVerifyf(IsEqualAll(info.vPositionWld, Vec3V(V_FLT_MAX)) == 0, "trying to apply a decal to vehicle glass with an invalid position"))
			{
				VfxCollInfo_s decalColnInfo = info;

				Vec3V vVel = GTA_ONLY(RCC_VEC3V)(pPhysical->GetVelocity());
				decalColnInfo.vPositionWld += vVel*ScalarVFromF32(fwTimer::GetTimeStep());

				const bool bLookupCentreOfVehicleFromBone = MI_CAR_SLAMTRUCK.IsValid() && pPhysical && pPhysical->GetModelIndex() == MI_CAR_SLAMTRUCK;
				if (g_decalMan.AddWeaponImpact(*pFxWeaponInfo, decalColnInfo, this, const_cast<CPhysical*>(pPhysical), m_boneIndex, bApplyBulletproofGlassTextureSwap, bLookupCentreOfVehicleFromBone) != 0)
				{
					ptfxGlassImpactInfo glassImpactInfo;

					glassImpactInfo.pFxWeaponInfo = pFxWeaponInfo;
					glassImpactInfo.RegdEnt       = info.regdEnt;
					glassImpactInfo.vPos          = vSmashPosWld;
					glassImpactInfo.vNormal       = info.vNormalWld;
					glassImpactInfo.isExitFx      = info.isExitFx;

#if GTA_REPLAY
					//don't add vfx or audio if we're loading a replay, otherwise they appear on the first frame
					if(CReplayMgr::IsPreCachingScene() == false)
#endif
					{
						if (!info.noPtFx)
						{
							g_ptFxManager.StoreGlassImpact(glassImpactInfo);
						}

						// AUDIO: bullet hitting glass caused a shatter texture to be applied
						if (!info.isSnowball)
						{
							m_audioEvent.ReportGlassCrack(info.regdEnt,RCC_VECTOR3(vSmashPosWld));
						}
					}

					m_numDecals++;

					if (mtlId == PGTAMATERIALMGR->g_idCarGlassMedium)
					{
						if (m_numDecals >= 2 && !info.isSnowball)
						{
							m_cracked = true;
						}
					}
					else if (mtlId == PGTAMATERIALMGR->g_idCarGlassStrong)
					{
						// Bullet resistant glass: always crack at 75% health regardless of number of decals.
						if ((m_numDecals >= 3 && !bHasBulletResistantGlass && !info.isSnowball) || (bHasBulletResistantGlass && m_fArmouredGlassHealth <= MAX_ARMOURED_GLASS_HEALTH * 0.33f))
						{
							m_cracked = true;
						}
					}
				}
#if VEHICLE_GLASS_VERBOSE_DEBUG_OUTPUT
				else
				if (g_vehicleGlassMan.m_verbose ||
					g_vehicleGlassMan.m_verboseHit)
				{
					Displayf("### no decal added because g_decalMan.AddWeaponImpact returned 0");
				}
#endif // VEHICLE_GLASS_VERBOSE_DEBUG_OUTPUT
			}
		}
#if VEHICLE_GLASS_VERBOSE_DEBUG_OUTPUT
		else
		if (g_vehicleGlassMan.m_verbose ||
			g_vehicleGlassMan.m_verboseHit)
		{
			// note: group 10 is SHOTGUN
			Displayf("### no decal added because g_vfxWeapon.GetInfo returned NULL for weapon group %d", info.weaponGroup);
		}
#endif // VEHICLE_GLASS_VERBOSE_DEBUG_OUTPUT

		return;
	}

	if (info.isExitFx) // don't try to detach any triangles when the bullet is exiting
	{
		return;
	}

	if (!m_smashed)
	{
		// This is a bit of nasty code to inform the network object of the vehicle being damaged that one of its windscreens has smashed.
		// This is to get around the problem where it is difficult to tell whether these are smashed without having to recurse through the
		// smash groups every time. When the object updates itself over the network it needs to do this and it is a bit too processor heavy.
		CVehicle* pNetworkVehicle = NetworkUtils::GetVehicleFromNetworkObject(pPhysical->GetNetworkObject());

		if (pNetworkVehicle)
		{
			if (m_hierarchyId == VEH_WINDSCREEN)
			{
				static_cast<CNetObjVehicle*>(pPhysical->GetNetworkObject())->SetFrontWindscreenSmashed();
			}
			else if (m_hierarchyId == VEH_WINDSCREEN_R)
			{
				static_cast<CNetObjVehicle*>(pPhysical->GetNetworkObject())->SetRearWindscreenSmashed();
			}
		}

		m_cracked = true;
		m_smashed = true;
	}

	UpdateSmashSphere(vSmashPosObj, radius);


	// Don't bother with process tessellation if there are no attached triangles
	int numInstantBreakPieces = 0;
	Vec3V vInstantBreakCentrePos(V_ZERO);

	// Avoid tessellating when there is no room for additional detached triangles
	if (m_forceSmashDetach && g_vehicleGlassMan.m_trianglePool->GetNumItems() == 0)
	{
		// Try to free some detached triangles
		g_vehicleGlassMan.RemoveFurthestComponent(pPhysical, this);

		if (g_vehicleGlassMan.m_trianglePool->GetNumItems() == 0)
		{
			// There is no room for more detached triangles - remove all attached triangles
			Assert(m_pAttachedVB); // If we got here this should be a valid pointer
			CVehicleGlassVB::Destroy(m_pAttachedVB);
			m_pAttachedVB = NULL;
			m_cracked = true;
		}
	}

	if (Verifyf(m_pAttachedVB && m_pAttachedVB->GetVertexCount() > 0 BANK_ONLY( || (m_forceSmashDetach && g_vehicleGlassMan.m_trianglePool->GetNumItems() == 0)),
		"Trying to break vehicle glass with no VB (m_forceSmashDetach is %s and detached pool size is %d)", m_forceSmashDetach ? "TRUE" : "FALSE", g_vehicleGlassMan.m_trianglePool->GetNumItems()))
	{
		const float tessellationScale = m_forceSmashDetach ? VEHICLEGLASSTESSELLATIONSCALE_FORCE_SMASH : 1.0f;

		vInstantBreakCentrePos = ProcessTessellation(pPhysical, &info, tessellationScale, m_forceSmashDetach, bProcessDetached, numInstantBreakPieces);
	}

#if GTA_REPLAY
	//don't add vfx/audio if we're loading the scene otherwise they will play when the clip starts
	if(CReplayMgr::IsPreparingFrame())
	{
		return;
	}
#endif

	// vfx
	if (m_forceSmash)
	{
		// play entire window smash vfx
		if (numInstantBreakPieces > 0)
		{
			// check if it's been a while since the last one
			const u32 currTime = fwTimer::GetTimeInMilliseconds();

			// check if the glass is within fx creation range
			if (m_distToViewportSqr < VFXRANGE_GLASS_SMASH_SQR && currTime - m_lastFxTime > 200)
			{
				phMaterialMgr::Id smashObjMtlId = m_materialId;

				if (PGTAMATERIALMGR->GetMtlVfxGroup(smashObjMtlId) == VFXGROUP_CAR_GLASS)
				{
					ptfxGlassSmashInfo glassSmashInfo;

					glassSmashInfo.regdPhy      = m_regdPhy;
					glassSmashInfo.vPosHit      = vSmashPosWld;
					glassSmashInfo.vPosCenter   = Transform(m_matrix, vInstantBreakCentrePos);
					glassSmashInfo.vNormal      = info.vNormalWld;
					glassSmashInfo.force        = info.force;
					glassSmashInfo.isWindscreen = (m_hierarchyId == VEH_WINDSCREEN || m_hierarchyId == VEH_WINDSCREEN_R || smashObjMtlId == PGTAMATERIALMGR->g_idCarGlassBulletproof);

					g_ptFxManager.StoreGlassSmash(glassSmashInfo);
					m_audioEvent.ReportSmash(info.regdEnt, RCC_VECTOR3(vSmashPosWld), numInstantBreakPieces, true);
					m_lastFxTime = currTime;
				}
			}
		}
	}
	else BANK_ONLY(if (g_vehicleGlassMan.m_impactEffectParticle))
	{
		// play bullet impact vfx
		VfxWeaponInfo_s* pFxWeaponInfo = g_vfxWeapon.GetInfo(PGTAMATERIALMGR->GetMtlVfxGroup(mtlId), info.weaponGroup, info.isBloody);

		if (pFxWeaponInfo)
		{
			ptfxGlassImpactInfo glassImpactInfo;

			glassImpactInfo.pFxWeaponInfo = pFxWeaponInfo;
			glassImpactInfo.RegdEnt       = info.regdEnt;
			glassImpactInfo.vPos          = vSmashPosWld;
			glassImpactInfo.vNormal       = info.vNormalWld;
			glassImpactInfo.isExitFx      = info.isExitFx;

			g_ptFxManager.StoreGlassImpact(glassImpactInfo);
			m_audioEvent.ReportSmash(info.regdEnt, RCC_VECTOR3(vSmashPosWld), numInstantBreakPieces);
		}
	}
}

void CVehicleGlassComponent::ProcessGroupExplosion(const VfxExpInfo_s& info, bool DEV_ONLY(bSmash), bool bProcessDetached)
{
	const CPhysical* pPhysical = m_regdPhy.Get();

	Assert(m_boneIndex >= 0);
	CVfxHelper::GetMatrixFromBoneIndex_Smash(m_matrix, pPhysical, m_boneIndex);

	if (IsZeroAll(m_matrix.GetCol0()))
	{
		return;
	}

#if __ASSERT
	const float det = Determinant3x3(m_matrix).Getf();

	if (det < 0.0f)// && PARAM_vgasserts.Get())
	{
		const char* boneName = pPhysical->GetDrawable()->GetSkeletonData()->GetBoneData(m_boneIndex)->GetName();

		// e.g. BS#841877
		vfxAssertf(0, "%s: bone matrix is flipped (componentId=%d, boneIndex=%d, bone='%s', det=%f)", pPhysical->GetModelName(), m_componentId, m_boneIndex, boneName, det);
	}
#endif // __ASSERT

	const Vec3V   vSmashPosWld = info.vPositionWld;
	const Vec3V   vSmashPosObj = UnTransformOrtho(m_matrix, vSmashPosWld);
	const ScalarV vSmashRadSqr = ScalarV(info.radius*info.radius);

	// check if any collision has occurred by checking if any of the attached faces are close by
	bool hitOccurred = false;

	if (m_pAttachedVB)
	{
		for (int iCurVer = 0; iCurVer < m_pAttachedVB->GetVertexCount(); iCurVer += 3)
		{
			const Vec3V P0 = m_pAttachedVB->GetVertexP(iCurVer + 0);
			const Vec3V P1 = m_pAttachedVB->GetVertexP(iCurVer + 1);
			const Vec3V P2 = m_pAttachedVB->GetVertexP(iCurVer + 2);

			// NOTE -- this function does not actually do a proper sphere-triangle intersection, it just tests for intersection with the vertices
			if (VehicleGlassIsTriangleInRange(vSmashPosObj, vSmashRadSqr, P0, P1, P2))
			{
				hitOccurred = true;
				break;
			}
		}
	}

	// return if no faces were hit (unless this is a component that has been flagged to totally smash by the explosion)
	if (!hitOccurred)
	{
		return;
	}

	// Avoid tessellating when there is no room for additional detached triangles
	if (m_forceSmashDetach && g_vehicleGlassMan.m_trianglePool->GetNumItems() == 0)
	{
		// Try to free some detached triangles
		g_vehicleGlassMan.RemoveFurthestComponent(pPhysical, this);

		if (g_vehicleGlassMan.m_trianglePool->GetNumItems() == 0)
		{
			// There is no room for more detached triangles - remove all attached triangles
			Assert(m_pAttachedVB); // If we got here this should be a valid pointer
			CVehicleGlassVB::Destroy(m_pAttachedVB);
			m_pAttachedVB = NULL;
		}
	}

	// calc radius of smash based on force etc.
	const float radius = info.radius;
	vfxAssertf(radius > 0.0f, "has a non-positive radius");

	phMaterialMgr::Id mtlId = PGTAMATERIALMGR->UnpackMtlId(m_materialId);

#if __BANK
	if (g_vehicleGlassMan.m_allGlassIsWeak)
	{
		mtlId = PGTAMATERIALMGR->g_idCarGlassWeak;
	}
#endif // __BANK

#if VEHICLE_GLASS_VERBOSE_DEBUG_OUTPUT
	if (g_vehicleGlassMan.m_verbose ||
		g_vehicleGlassMan.m_verboseHit)
	{
		const char* boneName = pPhysical->GetDrawable()->GetSkeletonData()->GetBoneData(m_boneIndex)->GetName();

		char mtlIdName[128] = "";
		PGTAMATERIALMGR->GetMaterialName(mtlId, mtlIdName, sizeof(mtlIdName));
		Displayf("Explosion hit componentId=%d '%s' [%s] on %s, radius=%f%s", m_componentId, boneName, mtlIdName, pPhysical->GetModelName(), radius, m_forceSmashDetach ? " - force smash detach" : m_forceSmash ? " - force smash" : DEV_SWITCH(bSmash, false) ? " - smash" : "");
	}
#endif // VEHICLE_GLASS_VERBOSE_DEBUG_OUTPUT
	DEV_ONLY((void)bSmash); // don't complain about bSmash being unreferenced please

	if (!m_smashed)
	{
		// Only allow bulletproof glass to smash on explosions if the object is wrecked
		if (mtlId == PGTAMATERIALMGR->g_idCarGlassBulletproof && m_regdPhy->GetStatus() != STATUS_WRECKED)
		{
			return;
		}
			
		// This is a bit of nasty code to inform the network object of the vehicle being damaged that one of its windscreens has smashed.
		// This is to get around the problem where it is difficult to tell whether these are smashed without having to recurse through the
		// smash groups every time. When the object updates itself over the network it needs to do this and it is a bit too processor heavy.
		CVehicle* pNetworkVehicle = NetworkUtils::GetVehicleFromNetworkObject(pPhysical->GetNetworkObject());

		if (pNetworkVehicle)
		{
			if (m_hierarchyId == VEH_WINDSCREEN)
			{
				static_cast<CNetObjVehicle*>(pPhysical->GetNetworkObject())->SetFrontWindscreenSmashed();
			}
			else if (m_hierarchyId == VEH_WINDSCREEN_R)
			{
				static_cast<CNetObjVehicle*>(pPhysical->GetNetworkObject())->SetRearWindscreenSmashed();
			}
		}

		m_cracked = true;
		m_smashed = true;
	}

#if __BANK
	atSNode<Vec4V>* pNode = NULL;

	if (m_listExplosionHistory.GetNumItems() >= 15)
	{
		pNode = m_listExplosionHistory.PopHead();
	}
	else
	{
		pNode = rage_new atSNode<Vec4V>();
	}

	pNode->Data = m_smashSphere;
	m_listExplosionHistory.Append(*pNode);
#endif // __BANK

	UpdateSmashSphere(vSmashPosObj, radius);

	bool bAnyTessellated = false;
	bool bAnyDetached = false;

	CVGStackTriangle* triangleBuffer = Alloca(CVGStackTriangle, g_vehicleGlassMaxStackTriangles);
	u32 nextFreeTriangle = 0;

	if (m_pAttachedVB && BANK_SWITCH(g_vehicleGlassMan.m_tessellationEnabled, true))
	{
		const fwVehicleGlassWindowProxy* pProxy = GetWindowProxy(m_regdPhy.Get());
		Mat34V distFieldBasis(V_IDENTITY);
		if (pProxy)
		{
			distFieldBasis = pProxy->GetBasis(VEHICLE_GLASS_DEV_SWITCH(g_vehicleGlassMan.m_distanceFieldUncompressed, false));
		}

		const Vec3V smashCentre = m_smashSphere.GetXYZ();
		const float tessellationScale = m_forceSmashDetach ? VEHICLEGLASSTESSELLATIONSCALE_EXPLOSION : 1.0f; // Only use lower tessellation when not forcing detach
		const u32 tessellationScaleU32 = (u32)(tessellationScale*(1<<VG_TESS_FIXED_POINT)+0.5f);

		for (int iCurVert = 0; iCurVert < m_pAttachedVB->GetVertexCount(); iCurVert += 3)
		{
			const Vec3V P0 = m_pAttachedVB->GetVertexP(iCurVert + 0);
			const Vec3V P1 = m_pAttachedVB->GetVertexP(iCurVert + 1);
			const Vec3V P2 = m_pAttachedVB->GetVertexP(iCurVert + 2);
			const Vec3V verts[3] = {P0, P1, P2};

			bool bTessellated;
			bool bCanDetach;
			BANK_ONLY(bool bHasError);
			m_pAttachedVB->GetFlags(iCurVert, bTessellated, bCanDetach BANK_ONLY(, bHasError));

			Vec4V T0, T1, T2;
			Vec3V N0, N1, N2;
			Vec4V C0, C1, C2;
			m_pAttachedVB->GetVertexTNC(iCurVert + 0, T0, N0, C0);
			m_pAttachedVB->GetVertexTNC(iCurVert + 1, T1, N1, C1);
			m_pAttachedVB->GetVertexTNC(iCurVert + 2, T2, N2, C2);

			if (!bTessellated)
			{
				const ScalarV smashRadius = ProcessTessellation_GetSmashRadius((P0 + P1 + P2)*ScalarVConstant<FLOAT_TO_INT(1.0f/3.0f)>());
				const Vec3V faceNormal = Normalize(Cross(P1-P0, P2-P1));
				if (BANK_ONLY(!g_vehicleGlassMan.m_tessellationSphereTest ||) geomSpheres::TestSphereToTriangle(Vec4V(smashCentre, smashRadius), verts, faceNormal))
				{
					AddTriangleTessellated(
						triangleBuffer, nextFreeTriangle,
						distFieldBasis,
						smashCentre, smashRadius,
						tessellationScaleU32,
						faceNormal,
						P0, P1, P2,
						T0, T1, T2,
						N0, N1, N2,
						C0, C1, C2
					);
					bAnyTessellated = true;
					continue;
				}
			}
			else
			{
				// Triangle is not affected - just copy it to the triangle stack array
				if (nextFreeTriangle < g_vehicleGlassMaxStackTriangles)
				{
					CVGStackTriangle& triangle = triangleBuffer[nextFreeTriangle++];
					triangle.SetVertexP(0, P0);
					triangle.SetVertexP(1, P1);
					triangle.SetVertexP(2, P2);
					triangle.SetVertexTNC(0, T0, N0, C0);
					triangle.SetVertexTNC(1, T1, N1, C1);
					triangle.SetVertexTNC(2, T2, N2, C2);
					triangle.SetFlags(bTessellated, bCanDetach BANK_ONLY(, bHasError));
				}
				else if (PARAM_vgasserts.Get())
				{
					vfxAssertf(0, "%s: (ProcessGroupExplosion) failed to create vehicle glass triangle, overflowed stack size of %d", GetModelName(), g_vehicleGlassMaxStackTriangles);
				}
			}
		}
#if VEHICLE_GLASS_VERBOSE_DEBUG_OUTPUT
		if (g_vehicleGlassMan.m_verboseHit)
		{
			Displayf("AddTriangleTessellated used %d triangles out of %d", nextFreeTriangle, g_vehicleGlassMaxStackTriangles);
		}
#endif // VEHICLE_GLASS_VERBOSE_DEBUG_OUTPUT
	}

	if ((bAnyTessellated || m_forceSmashDetach) BANK_ONLY(&& !g_vehicleGlassMan.m_disableDetaching))
	{
		const Vec3V entityVelocity = GTA_ONLY(RCC_VEC3V)(pPhysical->GetVelocity());

		const bool bIsVisible = IsVisible(); // This function is not inlined
		const float radiusSqr = radius*radius;
		for (int iCurTriangle = 0; iCurTriangle < nextFreeTriangle; iCurTriangle++)
		{
			CVGStackTriangle& triangle = triangleBuffer[iCurTriangle];

			if (m_forceSmashDetach || triangle.CanDetach())
			{
				const Vec3V centroid = (triangle.GetVertexP(0) + triangle.GetVertexP(1) + triangle.GetVertexP(2))*ScalarVConstant<FLOAT_TO_INT(1.0f/3.0f)>();
				const Vec3V vExplosionVecWld = Transform3x3(m_matrix, centroid - vSmashPosObj);

				if (MagSquared(vExplosionVecWld).Getf() <= radiusSqr)
				{
					// When the vehicle is not visible or too far we can just remove the pieces that are about to detach so we won't have to simulate them
					if (bProcessDetached && bIsVisible REPLAY_ONLY(&& CReplayMgr::IsPreCachingScene() == false))
					{
						// Get a new triangle
						CVehicleGlassTriangle* pTriangle = m_triangleListDetached.GetFromPool(g_vehicleGlassMan.m_trianglePool);

						if (pTriangle == NULL) // try to free some triangles from furthest component
						{
							g_vehicleGlassMan.RemoveFurthestComponent(pPhysical, this);
							pTriangle = m_triangleListDetached.GetFromPool(g_vehicleGlassMan.m_trianglePool);
						}

						if (pTriangle)
						{
							const Vec3V velocity = NormalizeSafe(vExplosionVecWld, Vec3V(V_Z_AXIS_WZERO))*ScalarV(info.force) + entityVelocity;
							const ScalarV randForce = ScalarV(info.force*VEHICLEGLASSPOLY_VEL_RAND_EXPL); // add randomness to initial velocity

							pTriangle->SetVelocity(AddScaled(velocity, g_vehicleGlassVecRandom.GetSignedVec3V(), randForce));
							pTriangle->SetSpinAxis(NormalizeSafe(g_vehicleGlassVecRandom.GetSignedVec3V(), Vec3V(V_Z_AXIS_WZERO)));

							// transform positions into world space
							const Vec3V detachedP0 = Transform(m_matrix, triangle.GetVertexP(0));
							const Vec3V detachedP1 = Transform(m_matrix, triangle.GetVertexP(1));
							const Vec3V detachedP2 = Transform(m_matrix, triangle.GetVertexP(2));

							vfxAssertf(IsFiniteStable(detachedP0), "%s: vehicle glass P0 is invalid after transform (%f,%f,%f)", pPhysical->GetModelName(), VEC3V_ARGS(detachedP0));
							vfxAssertf(IsFiniteStable(detachedP1), "%s: vehicle glass P1 is invalid after transform (%f,%f,%f)", pPhysical->GetModelName(), VEC3V_ARGS(detachedP1));
							vfxAssertf(IsFiniteStable(detachedP2), "%s: vehicle glass P2 is invalid after transform (%f,%f,%f)", pPhysical->GetModelName(), VEC3V_ARGS(detachedP2));

							// Damage is now part of the world position and doesn't need to change anymore
							pTriangle->SetVertexDZero(0);
							pTriangle->SetVertexDZero(1);
							pTriangle->SetVertexDZero(2);

							// Copy all the data into the detached triangle
							Vec4V detachedT0, detachedT1, detachedT2;
							Vec3V detachedN0, detachedN1, detachedN2;
							Vec4V detachedC0, detachedC1, detachedC2;

							triangle.GetVertexTNC(0, detachedT0, detachedN0, detachedC0);
							triangle.GetVertexTNC(1, detachedT1, detachedN1, detachedC1);
							triangle.GetVertexTNC(2, detachedT2, detachedN2, detachedC2);

							pTriangle->SetVertexTNC(0, detachedT0, detachedN0, detachedC0);
							pTriangle->SetVertexTNC(1, detachedT1, detachedN1, detachedC1);
							pTriangle->SetVertexTNC(2, detachedT2, detachedN2, detachedC2);

							pTriangle->UpdatePositionsAndNormals(detachedP0, detachedP1, detachedP2);
						}
					}

					triangle.SetDetached(); // Mark as detached
					bAnyDetached = true;
				}
			}
		}
	}

	// Update the VB if anything changed
	if (bAnyTessellated || bAnyDetached)
	{
		UpdateVB(triangleBuffer, nextFreeTriangle); // Update the VB with the new triangle setup
	}

	// we are smashing the glass so we need to remove any associated textures
	if (bAnyDetached && m_numDecals > 0)
	{
		g_decalMan.Remove(pPhysical, -1, this);
		m_numDecals = 0;
	}
}

void CVehicleGlassComponent::AddTriangle(
	CVGStackTriangle* triangleArray,
	u32& nextFreeTriangle,
	Vec3V_In P0, Vec3V_In P1, Vec3V_In P2,
	Vec4V_In T0, Vec4V_In T1, Vec4V_In T2,
	Vec3V_In N0, Vec3V_In N1, Vec3V_In N2,
	Vec4V_In C0, Vec4V_In C1, Vec4V_In C2
	)
{
	// Area calculation
#if __ASSERT
	const Vec3V edge01 = P1 - P0;
	const Vec3V edge20 = P0 - P2;
	const Vec3V edgeCross = Cross(edge01, edge20);
	float triArea = Mag(edgeCross).Getf()*0.5f;
	const CPhysical* pPhysical = m_regdPhy.Get();
	vfxAssertf(triArea > 0.0f, "CVehicleGlassComponent::AddTriangle trying to add a triangle with zero area for model %s", pPhysical ? pPhysical->GetModelName() : "Unknown");
#endif // __ASSERT

	if (nextFreeTriangle < g_vehicleGlassMaxStackTriangles)
	{
		CVGStackTriangle& triangle = triangleArray[nextFreeTriangle++];

		triangle.SetVertexP(0, P0);
		triangle.SetVertexP(1, P1);
		triangle.SetVertexP(2, P2);

		Vec3V normal0 = N0;
		Vec3V normal1 = N1;
		Vec3V normal2 = N2;

		triangle.SetVertexTNC(0, T0, normal0, C0);
		triangle.SetVertexTNC(1, T1, normal1, C1);
		triangle.SetVertexTNC(2, T2, normal2, C2);

		bool bTessellated = false;
		bool bCanDetach = false;
		BANK_ONLY(bool bHasError = false;)

		triangle.SetFlags(bTessellated, bCanDetach BANK_ONLY(, bHasError));

//#if VEHICLE_GLASS_SMASH_TEST
//		g_vehicleGlassSmashTest.SmashTestAddTriangle(this, &triangle, P0,P1,P2, T0,T1,T2, N0,N1,N2, C0,C1,C2);
//#endif // VEHICLE_GLASS_SMASH_TEST
	}
	else if (PARAM_vgasserts.Get())
	{
		vfxAssertf(0, "%s: failed to create vehicle glass triangle, overflowed stack size of %d", GetModelName(), g_vehicleGlassMaxStackTriangles);
	}
}

struct CVehicleGlassComponent::CVGTmpVertex
{
	// Position
	//
	// Input/output for CreateTempVertsFromTessellation, input to CheckDistanceField
	//  pos.x:  float position x-coordinate
	//  pos.y:  float position y-coordinate
	//  pos.z:  float position z-coordinate
	//  pos.w:  undefined
	//
	// Output from CheckDistanceField
	//  pos.x:  float position x-coordinate
	//  pos.y:  float position y-coordinate
	//  pos.z:  float position z-coordinate
	//  pos.w:  colour in device format
	//
	Vec::Vector_4V pos;

	// Texcoords
	Vec::Vector_4V tex;

	// Normal
	Vec::Vector_4V nrm;

	// Colour (RGBA f32)
	Vec::Vector_4V col;

	// Distance field value is used to store different (distance field related)
	// information at different steps.
	//
	// Input/output for CreateTempVertsFromTessellation, input to CheckDistanceField
	//      dif.x: float distance field coord i
	//      dif.y: float distance field coord j
	//
	// Temporary intermediate values in CheckDistanceField
	//      dif.x: int dist field coord i (floored)
	//      dif.y: int dist field coord j (floored)
	//      dif.z: float dist field lerp i
	//      dif.w: float dist field lerp j
	//
	// Output from CheckDistanceField
	//      dif.z: int 1 iff greater than distance threshold, else 0
	//
	Vec::Vector_4V dif;
};

void CVehicleGlassComponent::AddTriangleTessellated(
	CVGStackTriangle* triangleArray,
	u32& nextFreeTriangle,
	Mat34V_In distFieldBasis,
	Vec3V_In smashCentre,
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
)
{
	using namespace Vec;

	// We know that the triangle's plane intersects the sphere.  Intersecting
	// them gives a circle to test the tessellated triangles against.  Form a 2D
	// space with the circle's center at the origin, and transform the intial
	// triangle's vertices into this space.
	const ScalarV negDistPlaneToOrigin = Dot(faceNormal, P0);
	const ScalarV distPlaneToSphereCenter = Dot(smashCentre, faceNormal) - negDistPlaneToOrigin;
	Assert(IsLessThanAll(Abs(distPlaneToSphereCenter), smashRadius));
	const ScalarV circleRadiusSq = smashRadius*smashRadius - distPlaneToSphereCenter*distPlaneToSphereCenter;
	const Vec3V circleCenter = smashCentre - distPlaneToSphereCenter * faceNormal;
	const Vec3V circleBasisZ = faceNormal;
	const Vec3V circleBasisX(-circleBasisZ.GetY(), circleBasisZ.GetZ(), -circleBasisZ.GetX()); // rotate pi/2 about two axes to ensure perpendicular
	const Vec3V circleBasisY = Cross(circleBasisX, circleBasisZ);
	const Mat33V circleBasis(circleBasisX, circleBasisY, circleBasisZ);
	const Vec3V P0_2d = Multiply(circleBasis, P0-circleCenter);
	const Vec3V P1_2d = Multiply(circleBasis, P1-circleCenter);
	const Vec3V P2_2d = Multiply(circleBasis, P2-circleCenter);

	// Determine how to tessellate the triangles so that they are either no
	// longer intersecting the sphere, or have reached the minimum size.  This
	// step does not calculate the new vertices, rather it just determines which
	// edges will be split.
	u8 edges[VG_MAX_TESSELLATION_NUM_EDGE_CHOICES];
	u32 ratios[VG_MAX_TESSELLATION_NUM_RATIOS];
	u8 *pEdges = edges;
	u32 *pRatios = ratios;
	const Vec3V edge01 = P1-P0;
	const Vec3V edge12 = P2-P1;
	const Vec3V edge20 = P0-P2;
	const float fLen01 = Mag(edge01).Getf();
	const float fLen12 = Mag(edge12).Getf();
	const float fLen20 = Mag(edge20).Getf();
	const u32 len01 = (u32)(fLen01*(1<<VG_TESS_FIXED_POINT)+0.5f);
	const u32 len12 = (u32)(fLen12*(1<<VG_TESS_FIXED_POINT)+0.5f);
	const u32 len20 = (u32)(fLen20*(1<<VG_TESS_FIXED_POINT)+0.5f);
	const float fArea = Mag(Cross(edge01,edge12)).Getf()*0.5f;
	const u32 area = (u32)(fArea*(1<<VG_TESS_FIXED_POINT)+0.5f);
#if __BANK
	if (!g_vehicleGlassMan.m_tessellationSphereTest)
	{
		RecursiveTessellateNoSphereTest(
			&pEdges, &pRatios, VG_MAX_TESSELLATION_RECURSION_DEPTH-1,
			len01, len12, len20, area, tessellationScale
#			if VEHICLE_GLASS_DEBUG_COMPARE_CORRECT_CALCS
				, fLen01, fLen12, fLen20, fArea
#			endif
			);
	}
	else
#endif
	{
		RecursiveTessellateWithSphereTest(
			&pEdges, &pRatios, VG_MAX_TESSELLATION_RECURSION_DEPTH-1,
			len01, len12, len20, area, tessellationScale,
			*(const Vec2f*)&P0_2d, *(const Vec2f*)&P1_2d, *(const Vec2f*)&P2_2d, circleRadiusSq.Getf()
#			if VEHICLE_GLASS_DEBUG_COMPARE_CORRECT_CALCS
				, fLen01, fLen12, fLen20, fArea
#			endif
			);
	}

	// Using the edges selected for tessellation splitting above, generate the
	// vertices and indices.  Start by filling in the first three vertices from
	// the input triangle.  Stored with each vertex is a 2D distance field
	// coordinate that will be used later.
	//
	// Note that we need to round up VG_MAX_TESSELLATION_NUM_TEMP_VTXS to a
	// multiple of four to handle the loop unrolling in CheckDistanceField.
	//
	CVGTmpVertex tmpVtxs[(VG_MAX_TESSELLATION_NUM_TEMP_VTXS+3)&~3];
	const Vector_4V p0 = P0.GetIntrin128();
	const Vector_4V p1 = P1.GetIntrin128();
	const Vector_4V p2 = P2.GetIntrin128();
	const Vector_4V half = V4VConstant(V_HALF);
	tmpVtxs[0].pos = p0;
	tmpVtxs[0].tex = T0.GetIntrin128();
	tmpVtxs[0].nrm = N0.GetIntrin128();
	tmpVtxs[0].col = C0.GetIntrin128();
	tmpVtxs[0].dif = V4Subtract(Transform(distFieldBasis, Vec3V(p0)).GetIntrin128(), half);
	tmpVtxs[1].pos = p1;
	tmpVtxs[1].tex = T1.GetIntrin128();
	tmpVtxs[1].nrm = N1.GetIntrin128();
	tmpVtxs[1].col = C1.GetIntrin128();
	tmpVtxs[1].dif = V4Subtract(Transform(distFieldBasis, Vec3V(p1)).GetIntrin128(), half);
	tmpVtxs[2].pos = p2;
	tmpVtxs[2].tex = T2.GetIntrin128();
	tmpVtxs[2].nrm = N2.GetIntrin128();
	tmpVtxs[2].col = C2.GetIntrin128();
	tmpVtxs[2].dif = V4Subtract(Transform(distFieldBasis, Vec3V(p2)).GetIntrin128(), half);
	u16 tmpIdxs[VG_MAX_TESSELLATION_NUM_TEMP_IDXS];
#if __BE
	// StoreStackTris accesses pointer into array-3 on big endian platforms, so leave extra space
	u8 tessFlagsBuf[VG_MAX_TESSELLATION_NUM_TRIS+3];
	u8 *const tessFlags = tessFlagsBuf+3;
#else
	u8 tessFlags[VG_MAX_TESSELLATION_NUM_TRIS];
#endif
	unsigned numVtxs, numIdxs;
	CreateTempVertsFromTessellation(tmpVtxs, tmpIdxs, tessFlags, &numVtxs, &numIdxs, edges, ratios);

	// Reprocess each vertex, and using the distance field coordinate stored
	// there, do the distance field lookup.
	const fwVehicleGlassWindowProxy* pProxy = GetWindowProxy(m_regdPhy.Get());
	const float distanceToDetach = BANK_SWITCH(g_vehicleGlassMan.m_distanceFieldThresholdMin, VEHICLE_GLASS_DEFAULT_DISTANCE_THRESHOLD_TO_DETACH_MIN);
	CheckDistanceField(tmpVtxs, numVtxs, pProxy, distanceToDetach);

	// Convert the temporary indices and vertices to CVGStackTriangles.
	const unsigned numTris = Min(numIdxs/3, g_vehicleGlassMaxStackTriangles-nextFreeTriangle);
	const unsigned numVertsToDetach = BANK_SWITCH(g_vehicleGlassMan.m_distanceFieldNumVerts, (int)VEHICLE_GLASS_DEFAULT_DISTANCE_FIELD_NUM_VERTS_TO_DETACH);
	const unsigned detachThresh = numVertsToDetach - VEHICLE_GLASS_TEST_1_VERTEX + 1;
	StoreStackTris(triangleArray+nextFreeTriangle, tmpVtxs, tmpIdxs, (const bool*)tessFlags, numTris, detachThresh);
	nextFreeTriangle += numTris;
}

void CVehicleGlassComponent::RecursiveTessellateNoSphereTest(
	u8 **outEdges,
	u32 **outRatios,
	unsigned remainingRecursionAllowed,
	u32 len01,
	u32 len12,
	u32 len20,
	u32 area,
	u32 tessellationScale
#	if VEHICLE_GLASS_DEBUG_COMPARE_CORRECT_CALCS
		, float fLen01
		, float fLen12
		, float fLen20
		, float fArea
#	endif
	)
{
	// Try to keep everything here in integer registers for the benefit of PPC
	// platforms where branching based on floating point registers is very
	// expensive.  Unfortunately even integer multiplies are expensive so they
	// need to be avoided where feasible too :/  Thankfully though, accuracy in
	// this function is not very important.  We only need to select the longest
	// edge and stop the tessellation at roughly the correct point.  Even if the
	// wrong edge is selected, it is not really important when the edges are
	// similar in length.
	//
	// Why all the branch free code here?  It probably isn't really all that
	// useful for RecursiveTessellateNoSphereTest, but in
	// RecursiveTessellateWithSphereTest, it is important to allow the compiler
	// to schedule all the floating point calculations there in parallel.
	// Because of that, it is nice to keep the code between these two functions
	// similar, though RecursiveTessellateNoSphereTest is called a lot more, so
	// if diverging the code and adding branches here will help, then that may
	// be worth doing.
	//

#if !__ASSERT
	if (remainingRecursionAllowed--)
#endif
	{
		// Randomize the area threshold upon which tessellation will be stopped.
		// mthRandom::GetInt returns [0..0x7fffffff], hence the shift right by
		// 31 not 32.
		const u32 randAreaThreshRange = (u32)((VG_TRI_AREA_THRESH_MAX-VG_TRI_AREA_THRESH_MIN)*(1<<VG_TESS_FIXED_POINT)+0.5f) + 1;
		const u32 randAreaThreshMin = (u32)(VG_TRI_AREA_THRESH_MIN*(1<<VG_TESS_FIXED_POINT)+0.5f);
		u32 areaThresh = (u32)(((u64)(u32)g_DrawRand.GetInt()*randAreaThreshRange)>>31) + randAreaThreshMin;
		areaThresh = (u32)(((u64)areaThresh*tessellationScale)>>VG_TESS_FIXED_POINT);

#if __ASSERT
		if (!remainingRecursionAllowed--)
		{
			if (area > areaThresh && PARAM_vgasserts.Get())
			{
				const CPhysical* pPhysical = m_regdPhy.Get();
				const char* boneName = pPhysical->GetDrawable()->GetSkeletonData()->GetBoneData(m_boneIndex)->GetName();
#if VEHICLE_GLASS_DEBUG_COMPARE_CORRECT_CALCS
#define EXACT_AREA_FMT_STR  ", exact area=%f"
#define EXACT_AREA_ARG      , fArea
#else
#define EXACT_AREA_FMT_STR
#define EXACT_AREA_ARG
#endif
				vfxAssertf(0, "%s: Tessellation recursion limit reached for componentId=%d, boneIndex=%d, bone='%s', "
					"threshold=%f, approx area=%f" EXACT_AREA_FMT_STR,
					pPhysical->GetModelName(), m_componentId, m_boneIndex, boneName,
					(double)areaThresh*(1.0/(1<<VG_TESS_FIXED_POINT)),
					(double)area*(1.0/(1<<VG_TESS_FIXED_POINT)) EXACT_AREA_ARG);
#undef EXACT_AREA_ARG
#undef EXACT_AREA_FMT_STR
			}
		}
		else
#endif // __ASSERT

		if (area > areaThresh)
		{
			// Determine the longest edge of the input triangle.  This is the
			// edge that will be split inorder to generate two smaller
			// triangles.
			const u32 len12sublen01     = len12-len01;
			const u32 mask01gt12        = ((s32)len12sublen01)>>31;
			const u32 edge01or12        = mask01gt12+1;
			const u32 maxLen01or12      = len12 - (len12sublen01&mask01gt12);
			const u32 len20sublen01or12 = len20 - maxLen01or12;
			const u32 mask01or12gt20    = ((s32)len20sublen01or12)>>31;
			const u32 longestEdge       = (2&~mask01or12gt20) | (edge01or12&mask01or12gt20);
			const u32 longestEdgeLen    = len20 - (len20sublen01or12&mask01or12gt20);
			const u32 maskLongestIs01   =  mask01gt12 & mask01or12gt20;
			const u32 maskLongestIs12   = ~mask01gt12 & mask01or12gt20;
			const u32 maskLongestIs20   = ~mask01or12gt20;

			// Randomize where along the longest edge it will be split.
			const u32 randSplitRange = (u32)(VG_TESS_RATIO_RANGE*(1<<VG_TESS_FIXED_POINT)+0.5f) + 1;
			const u32 randSplitMin   = (u32)(VG_TESS_RATIO_MIN*(1<<VG_TESS_FIXED_POINT)+0.5f);
			const u32 ratio = (u32)(((u64)(u32)g_DrawRand.GetInt()*randSplitRange)>>31) + randSplitMin;
			const u32 splitLenA = (u32)(((u64)longestEdgeLen*ratio)>>VG_TESS_FIXED_POINT);
			const u32 splitLenB = longestEdgeLen - splitLenA;

			// This code can be quite handy when debugging.  When it is enabled,
			// the true properly calculated values can be sanity checked against
			// the fixed point integer approximations/kludges when stepping
			// through the code.
#			if VEHICLE_GLASS_DEBUG_COMPARE_CORRECT_CALCS
				//
				//
				//               +        -  -  -  -  -  -  -  -  --
				//              /| \\                              |
				//             / |  \  \                           |
				//            /  |   \    \                        |
				//  fOrigLenA/   |    \      \fOrigLenB         fHeight
				//          /    |     \        \                  |
				//         /     |      \          \               |
				//        /      |       \            \            |
				//       +-------+--------+--------------+     -  --
				//       Q       R        S              T
				//
				//      |QS| = fSplitLenA
				//      |ST| = fSplitLenB
				//      |QR| = fDropLenA
				//      |RT| = fDropLenB
				//      |RS| = fDropLenDiff
				//
				//
				const float fLongestEdgeLen = Max(fLen01,fLen12,fLen20);
				const float fOrigLenA = maskLongestIs01 ? fLen20 : fLen01;
				const float fOrigLenB = maskLongestIs12 ? fLen20 : fLen12;
				const float fRatio = (float)ratio/(float)(1<<VG_TESS_FIXED_POINT);
				const float fSplitLenA = fRatio*fLongestEdgeLen;
				const float fSplitLenB = fLongestEdgeLen-fSplitLenA;
				const float fHeight = 2.f*fArea/fLongestEdgeLen;
				const float fDropLenA = sqrtf(fOrigLenA*fOrigLenA-fHeight*fHeight);
				const float fDropLenB = sqrtf(fOrigLenB*fOrigLenB-fHeight*fHeight);
				const float fDropLenDiff = Max(fDropLenA-fSplitLenA, fDropLenB-fSplitLenB);
				const float fDropAreaDiff = 0.5f*fHeight*fDropLenDiff;
				float fAreaA = 0.5f*fHeight*fDropLenA;
				float fAreaB = 0.5f*fHeight*fDropLenB;
				if (fDropLenA<fSplitLenA)
				{
					fAreaA += fDropAreaDiff;
					fAreaB -= fDropAreaDiff;
				}
				else
				{
					fAreaA -= fDropAreaDiff;
					fAreaB += fDropAreaDiff;
				}
				const float fNewEdgeLen = sqrtf(fHeight*fHeight+fDropLenDiff*fDropLenDiff);
#			endif

			// This area is very approximate, but since we are dividing the
			// longest edge somewhere near the middle it is a reasonable
			// approximation.  The error increases the further away from the
			// middle the split is.
			const u32 areaA = (u32)(((u64)area*ratio)>>VG_TESS_FIXED_POINT);
			const u32 areaB = area - areaA;

			// The length of the new edge is even more approximate.  But
			// accuracy here is not particuarly important.  This is just to
			// determine which edges to split, and if the decision is very
			// close, selecting a different edge is not a problem.  Accurate
			// calculations are done later when determining the vertex
			// positions.
			//
			// When ratio of two non-split edges is 1:1, the new edge len is the
			// height of the triangle if the split is 50:50.  As the ratio of
			// the non-split edges increases, the new edge len approaches
			// roughly half the split edge length.
			//
			// Despite how ridiculous this kludge first seems, when comparing
			// the result to fNewEdgeLen, it is suprisingly good, and fine for
			// our purposes.
			//
			const u32 height = (u32)(((u64)area<<(VG_TESS_FIXED_POINT+1))/longestEdgeLen);
			const u32 origLenA = (len20&maskLongestIs01) | (len01&~maskLongestIs01);
			const u32 origLenB = (len20&maskLongestIs12) | (len12&~maskLongestIs12);
			const u32 maxOrigLenAB = BranchlessMax(origLenA, origLenB);
			const u32 minOrigLenAB = (origLenA ^ origLenB) ^ maxOrigLenAB;
			const u32 origEdgeRatio = (u32)(((u64)minOrigLenAB<<VG_TESS_FIXED_POINT)/maxOrigLenAB);
			const u32 newEdgeLen = (u32)(((u64)((1<<VG_TESS_FIXED_POINT)-origEdgeRatio)*(longestEdgeLen>>1) + ((u64)origEdgeRatio*height)) >> VG_TESS_FIXED_POINT);

			*(*outEdges)++  = (u8)longestEdge;
			*(*outRatios)++ = ratio;

			// The following diagrams show the edge/vertex order for the two
			// subdivided triangles based on which edge is the longest.
			//
			// The later code in CreateTempVertsFromTessellation needs to
			// carefully follow this order to interpolate the correct vertices.
			//
			//
			//                    2
		    //                    +
		    //                  / | \
			//                /   |   \
			//           iii/     |     \iv
			//            /     ii|v      \
			//          /         |         \
			//        /           |           \
			//    0 +-------------+-------------+ 1
			//              i          vi
			//
			//                    0
		    //                    +
		    //                  / | \
			//                /   |   \
			//             i/     |     \v
			//            /    iii|vi     \
			//          /         |         \
			//        /           |           \
			//    1 +-------------+-------------+ 2
			//             ii          iv
			//
			//                    1
		    //                    +
		    //                  / | \
			//                /   |   \
			//            iv/     |     \i
			//            /     vi|ii     \
			//          /         |         \
			//        /           |           \
			//    2 +-------------+-------------+ 0
			//               v         iii
			//
			//
			//

			RecursiveTessellateNoSphereTest(outEdges, outRatios, remainingRecursionAllowed,
				(len01&~maskLongestIs01) | (splitLenA&maskLongestIs01),
				(newEdgeLen&~maskLongestIs12) | (splitLenA&maskLongestIs12),
				(len20&maskLongestIs01) | (newEdgeLen&maskLongestIs12) | (splitLenB&maskLongestIs20),
				areaA, tessellationScale
#				if VEHICLE_GLASS_DEBUG_COMPARE_CORRECT_CALCS
					, maskLongestIs01 ? fSplitLenA : fLen01
					, maskLongestIs12 ? fSplitLenA : fNewEdgeLen
					, maskLongestIs01 ? fLen20 : maskLongestIs12 ? fNewEdgeLen : fSplitLenB
					, fAreaA
#				endif
				);

			RecursiveTessellateNoSphereTest(outEdges, outRatios, remainingRecursionAllowed,
				(len12&~maskLongestIs12) | (splitLenB&maskLongestIs12),
				(newEdgeLen&maskLongestIs01) | (len20&maskLongestIs12) | (splitLenB&maskLongestIs20),
				(newEdgeLen&~maskLongestIs01) | (splitLenB&maskLongestIs01),
				areaB, tessellationScale
#				if VEHICLE_GLASS_DEBUG_COMPARE_CORRECT_CALCS
					, maskLongestIs12 ? fSplitLenB : fLen12
					, maskLongestIs01 ? fNewEdgeLen : maskLongestIs12 ? fLen20 : fSplitLenB
					, maskLongestIs01 ? fSplitLenB : fNewEdgeLen
					, fAreaB
#				endif
				);

			return;
		}
	}

	// Indicate tessellation stopped due to triangle becoming too small (or max
	// recursion depth reached).
	*(*outEdges)++ = VG_EDGE_STOP_TESS_SIZE;
}

void CVehicleGlassComponent::RecursiveTessellateWithSphereTest(u8 **outEdges, u32 **outRatios,
	unsigned remainingRecursionAllowed,
	u32 len01, u32 len12, u32 len20, u32 area, u32 tessellationScale,
	Vec2f_In vtx0, Vec2f_In vtx1, Vec2f_In vtx2, float radSq
#	if VEHICLE_GLASS_DEBUG_COMPARE_CORRECT_CALCS
		, float fLen01, float fLen12, float fLen20, float fArea
#	endif
	)
{
	// Same as RecursiveTessellateNoSphereTest but with added smash sphere test.
	// A fair amount of the code is just copied.  Comments have not been copied
	// though so see above for comments on those bits of code.  Comments here
	// are only for the new parts of this function.
	//
	// We do have some floating point branches in this code, but they seem to be
	// unavoidable.  Doing the sphere test with fixed point integers suffers too
	// much from the non-pipelined integer multiplies.  Two floating point
	// branches seems to be the lesser of the two evils.
	//
	// The good thing about using floats here though is that they can pipeline
	// well with all the other integer code.
	//
	// Floats were chosen over vector operations because the sphere test doesn't
	// really seem to vectorize too well, especially after we have simplified it
	// to a two dimensional test.
	//

#if !__ASSERT
	if (remainingRecursionAllowed--)
#endif
	{
		// Calculate ratio up front asap so that we can avoid a lhs when
		// converting the double to float.
		const u32 randSplitRange = (u32)(VG_TESS_RATIO_RANGE*(1<<VG_TESS_FIXED_POINT)+0.5f) + 1;
		const u32 randSplitMin   = (u32)(VG_TESS_RATIO_MIN*(1<<VG_TESS_FIXED_POINT)+0.5f);
		const u32 ratio = (u32)(((u64)(u32)g_DrawRand.GetInt()*randSplitRange)>>31) + randSplitMin;
		const double ratioDbl = (double)ratio;

		const u32 randAreaThreshRange = (u32)((VG_TRI_AREA_THRESH_MAX-VG_TRI_AREA_THRESH_MIN)*(1<<VG_TESS_FIXED_POINT)+0.5f) + 1;
		const u32 randAreaThreshMin = (u32)(VG_TRI_AREA_THRESH_MIN*(1<<VG_TESS_FIXED_POINT)+0.5f);
		u32 areaThresh = (u32)(((u64)(u32)g_DrawRand.GetInt()*randAreaThreshRange)>>31) + randAreaThreshMin;
		areaThresh = (u32)(((u64)areaThresh*tessellationScale)>>VG_TESS_FIXED_POINT);

#if __ASSERT
		if (!remainingRecursionAllowed--)
		{
			if (area > areaThresh && PARAM_vgasserts.Get())
			{
				const CPhysical* pPhysical = m_regdPhy.Get();
				const char* boneName = pPhysical->GetDrawable()->GetSkeletonData()->GetBoneData(m_boneIndex)->GetName();
#if VEHICLE_GLASS_DEBUG_COMPARE_CORRECT_CALCS
#define EXACT_AREA_FMT_STR  ", exact area=%f"
#define EXACT_AREA_ARG      , fArea
#else
#define EXACT_AREA_FMT_STR
#define EXACT_AREA_ARG
#endif
				vfxAssertf(0, "%s: Tessellation recursion limit reached for componentId=%d, boneIndex=%d, bone='%s', "
					"threshold=%f, approx area=%f" EXACT_AREA_FMT_STR,
					pPhysical->GetModelName(), m_componentId, m_boneIndex, boneName,
					(double)areaThresh*(1.0/(1<<VG_TESS_FIXED_POINT)),
					(double)area*(1.0/(1<<VG_TESS_FIXED_POINT)) EXACT_AREA_ARG);
#undef EXACT_AREA_ARG
#undef EXACT_AREA_FMT_STR
			}
		}
		else
#endif // __ASSERT

		if (area > areaThresh)
		{
			// Based on "Optimizing a sphere-triangle intersection test",
			// http://realtimecollisiondetection.net/blog/?p=103.
			// This is a 2D versions of the SAT test.

			// Test vertices
			const float aa = Dot(vtx0, vtx0);
			const float ab = Dot(vtx0, vtx1);
			const float ac = Dot(vtx0, vtx2);
			const float bb = Dot(vtx1, vtx1);
			const float bc = Dot(vtx1, vtx2);
			const float cc = Dot(vtx2, vtx2);
			const float vtx0in = aa-radSq; // inside sphere if negative
			const float vtx1in = bb-radSq;
			const float vtx2in = cc-radSq;
			const float sep2 = Selectf(ab-aa, Selectf(ac-aa, vtx0in, -1), -1);   // separated if positive
			const float sep3 = Selectf(ab-bb, Selectf(bc-bb, vtx1in, -1), -1);
			const float sep4 = Selectf(ab-cc, Selectf(bc-cc, vtx2in, -1), -1);

			// Test edges
			const Vec2f AB = vtx1-vtx0;
			const Vec2f BC = vtx2-vtx1;
			const Vec2f CA = vtx0-vtx2;
			const float d1 = ab-aa;
			const float d2 = bc-bb;
			const float d3 = ac-cc;
			const float e1 = Dot(AB, AB);
			const float e2 = Dot(BC, BC);
			const float e3 = Dot(CA, CA);
			const Vec2f Q1 = vtx0*e1 - d1*AB;
			const Vec2f Q2 = vtx1*e2 - d2*BC;
			const Vec2f Q3 = vtx2*e3 - d3*CA;
			const Vec2f QC = vtx2*e1 - Q1;
			const Vec2f QA = vtx0*e2 - Q2;
			const Vec2f QB = vtx1*e3 - Q3;
			const float sep5 = Selectf(Dot(Q1,Q1)-radSq*e1*e1, Dot(Q1,QC), -1);  // separated if positive
			const float sep6 = Selectf(Dot(Q2,Q2)-radSq*e2*e2, Dot(Q2,QA), -1);
			const float sep7 = Selectf(Dot(Q3,Q3)-radSq*e3*e3, Dot(Q3,QB), -1);

			// Combine the seperating axes tests
			const float containedIfNeg = Selectf(vtx0in, 0, Selectf(vtx0in, 0, vtx2in));
			const float separatedVtxIfPos = Selectf(sep2, 0, Selectf(sep3, 0, sep4));
			const float separatedEdgeIfPos = Selectf(sep5, 0, Selectf(sep6, 0, sep7));
			const float separatedIfPos = Selectf(separatedVtxIfPos, 0, separatedEdgeIfPos);



			const u32 len12sublen01     = len12-len01;
			const u32 mask01gt12        = ((s32)len12sublen01)>>31;
			const u32 edge01or12        = mask01gt12+1;
			const u32 maxLen01or12      = len12 - (len12sublen01&mask01gt12);
			const u32 len20sublen01or12 = len20 - maxLen01or12;
			const u32 mask01or12gt20    = ((s32)len20sublen01or12)>>31;
			const u32 longestEdge       = (2&~mask01or12gt20) | (edge01or12&mask01or12gt20);
			const u32 longestEdgeLen    = len20 - (len20sublen01or12&mask01or12gt20);
			const u32 maskLongestIs01   =  mask01gt12 & mask01or12gt20;
			const u32 maskLongestIs12   = ~mask01gt12 & mask01or12gt20;
			const u32 maskLongestIs20   = ~mask01or12gt20;

			const u32 splitLenA = (u32)(((u64)longestEdgeLen*ratio) >> VG_TESS_FIXED_POINT);
			const u32 splitLenB = longestEdgeLen - splitLenA;

#			if VEHICLE_GLASS_DEBUG_COMPARE_CORRECT_CALCS
				const float fLongestEdgeLen = Max(fLen01,fLen12,fLen20);
				const float fOrigLenA = maskLongestIs01 ? fLen20 : fLen01;
				const float fOrigLenB = maskLongestIs12 ? fLen20 : fLen12;
				const float fRatio = (float)ratio/(float)(1<<VG_TESS_FIXED_POINT);
				const float fSplitLenA = fRatio*fLongestEdgeLen;
				const float fSplitLenB = fLongestEdgeLen-fSplitLenA;
				const float fHeight = 2.f*fArea/fLongestEdgeLen;
				const float fDropLenA = sqrtf(fOrigLenA*fOrigLenA-fHeight*fHeight);
				const float fDropLenB = sqrtf(fOrigLenB*fOrigLenB-fHeight*fHeight);
				const float fDropLenDiff = Max(fDropLenA-fSplitLenA, fDropLenB-fSplitLenB);
				const float fDropAreaDiff = 0.5f*fHeight*fDropLenDiff;
				float fAreaA = 0.5f*fHeight*fDropLenA;
				float fAreaB = 0.5f*fHeight*fDropLenB;
				if (fDropLenA<fSplitLenA)
				{
					fAreaA += fDropAreaDiff;
					fAreaB -= fDropAreaDiff;
				}
				else
				{
					fAreaA -= fDropAreaDiff;
					fAreaB += fDropAreaDiff;
				}
				const float fNewEdgeLen = sqrtf(fHeight*fHeight+fDropLenDiff*fDropLenDiff);
#			endif

			const u32 areaA = (u32)(((u64)area*ratio)>>VG_TESS_FIXED_POINT);
			const u32 areaB = area - areaA;

			const u32 height = (u32)(((u64)area<<(VG_TESS_FIXED_POINT+1))/longestEdgeLen);
			const u32 origLenA = (len20&maskLongestIs01) | (len01&~maskLongestIs01);
			const u32 origLenB = (len20&maskLongestIs12) | (len12&~maskLongestIs12);
			const u32 maxOrigLenAB = BranchlessMax(origLenA, origLenB);
			const u32 minOrigLenAB = (origLenA ^ origLenB) ^ maxOrigLenAB;
			const u32 origEdgeRatio = (u32)(((u64)minOrigLenAB<<VG_TESS_FIXED_POINT)/maxOrigLenAB);
			const u32 newEdgeLen = (u32)(((u64)((1<<VG_TESS_FIXED_POINT)-origEdgeRatio)*(longestEdgeLen>>1) + ((u64)origEdgeRatio*height)) >> VG_TESS_FIXED_POINT);

			const float ratioFlt = (float)(ratioDbl * (1.0/(1<<VG_TESS_FIXED_POINT)));
			// maskLongestIs* will be either 0 or -1, so neg of that gives us an
			// index into the fltBool array.  fltBool[*] values are either
			// negative or positive to input into the fsel.
			static const float fltBool[] = {-1.f, 0.f};
			const float oneSubRatioFltIfLongest01 = Selectf(fltBool[-(s32)maskLongestIs01], 1.f-ratioFlt, 0.f);
			const float oneSubRatioFltIfLongest12 = Selectf(fltBool[-(s32)maskLongestIs12], 1.f-ratioFlt, 0.f);
			const float ratioFltIfLongest20       = Selectf(fltBool[-(s32)maskLongestIs20],     ratioFlt, 0.f);
			const float ratioFltIfLongest12       = Selectf(fltBool[-(s32)maskLongestIs12],     ratioFlt, 0.f);
			const float ratioFltLongest01         = Selectf(fltBool[-(s32)maskLongestIs01],     ratioFlt, 0.f);
			const float oneSubRatioFltIfLongest20 = Selectf(fltBool[-(s32)maskLongestIs20], 1.f-ratioFlt, 0.f);


			// Precalculate all the function arguments for if we do recurse.
			// Doing that here helps to give enough time for the float SAT calcs
			// to complete.
			const u32 len01A = (len01&~maskLongestIs01) | (splitLenA&maskLongestIs01);
			const u32 len12A = (newEdgeLen&~maskLongestIs12) | (splitLenA&maskLongestIs12);
			const u32 len20A = (len20&maskLongestIs01) | (newEdgeLen&maskLongestIs12) | (splitLenB&maskLongestIs20);
			const Vec2f vtx0A = vtx0;
			const Vec2f vtx1A = vtx1 + (vtx0-vtx1)*oneSubRatioFltIfLongest01;
			const Vec2f vtx2A = vtx2 + (vtx1-vtx2)*oneSubRatioFltIfLongest12 + (vtx0-vtx2)*ratioFltIfLongest20;
#			if VEHICLE_GLASS_DEBUG_COMPARE_CORRECT_CALCS
				const float fLen01A = maskLongestIs01 ? fSplitLenA : fLen01;
				const float fLen12A = maskLongestIs12 ? fSplitLenA : fNewEdgeLen;
				const float fLen20A = maskLongestIs01 ? fLen20 : maskLongestIs12 ? fNewEdgeLen : fSplitLenB;
#			endif
			const u32 len01B = (len12&~maskLongestIs12) | (splitLenB&maskLongestIs12);
			const u32 len12B = (newEdgeLen&maskLongestIs01) | (len20&maskLongestIs12) | (splitLenB&maskLongestIs20);
			const u32 len20B = (newEdgeLen&~maskLongestIs01) | (splitLenB&maskLongestIs01);
			const Vec2f vtx0B = vtx1 + (vtx2-vtx1)*ratioFltIfLongest12;
			const Vec2f vtx1B = vtx2;
			const Vec2f vtx2B = vtx0 + (vtx1-vtx0)*ratioFltLongest01 + (vtx2-vtx0)*oneSubRatioFltIfLongest20;
#			if VEHICLE_GLASS_DEBUG_COMPARE_CORRECT_CALCS
				const float fLen01B = maskLongestIs12 ? fSplitLenB : fLen12;
				const float fLen12B = maskLongestIs01 ? fNewEdgeLen : maskLongestIs12 ? fLen20 : fSplitLenB;
				const float fLen20B = maskLongestIs01 ? fSplitLenB : fNewEdgeLen;
#			endif

			// If the triangle is completely outside of the smash sphere, then
			// stop further tessellation.
			if (separatedIfPos >= 0.f)
			{
				*(*outEdges)++ = VG_EDGE_STOP_TESS_OUTSIDE_SPHERE;
			}
			else
			{
				*(*outEdges)++  = (u8)longestEdge;
				*(*outRatios)++ = ratio;

				// If the triangle is completely contained within the smash
				// sphere, then the recursion can switch to the cheaper
				// RecursiveTessellateNoSphereTest function since there is no
				// point in testing against the sphere any further.
				if (containedIfNeg < 0.f)
				{
					RecursiveTessellateNoSphereTest(outEdges, outRatios, remainingRecursionAllowed,
						len01A, len12A, len20A, areaA, tessellationScale
#					if VEHICLE_GLASS_DEBUG_COMPARE_CORRECT_CALCS
							, fLen01A, fLen12A, fLen20A, fAreaA
#					endif
						);

					RecursiveTessellateNoSphereTest(outEdges, outRatios, remainingRecursionAllowed,
						len01B, len12B, len20B, areaB, tessellationScale
#					if VEHICLE_GLASS_DEBUG_COMPARE_CORRECT_CALCS
							, fLen01B, fLen12B, fLen20B, fAreaB
#					endif
						);
				}

				// If triangle intersects the surface of the sphere, then need
				// to keep recursing using the more expensive sphere test.
				else
				{
					RecursiveTessellateWithSphereTest(outEdges, outRatios, remainingRecursionAllowed,
						len01A, len12A, len20A, areaA, tessellationScale, vtx0A, vtx1A, vtx2A, radSq
#					if VEHICLE_GLASS_DEBUG_COMPARE_CORRECT_CALCS
							, fLen01A, fLen12A, fLen20A, fAreaA
#					endif
						);

					RecursiveTessellateWithSphereTest(outEdges, outRatios, remainingRecursionAllowed,
						len01B, len12B, len20B, areaB, tessellationScale, vtx0B, vtx1B, vtx2B, radSq
#					if VEHICLE_GLASS_DEBUG_COMPARE_CORRECT_CALCS
							, fLen01B, fLen12B, fLen20B, fAreaB
#					endif
						);
				}
			}

			return;
		}
	}

	// Indicate tessellation stopped due to triangle becoming too small (or max
	// recursion depth reached).
	*(*outEdges)++ = VG_EDGE_STOP_TESS_SIZE;
}

void CVehicleGlassComponent::CreateTempVertsFromTessellation(CVGTmpVertex *tmpVtxs, u16 *tmpIdxs, u8 *tessFlags, unsigned *numVtxs, unsigned *numIdxs, const u8 *edges, const u32 *ratios)
{
	using namespace Vec;

	// Use a stack here to avoid the additional overhead of tail recursion.
 	u16 idxStack[VG_MAX_TESSELLATION_RECURSION_DEPTH*3];
	ASSERT_ONLY(sysMemSet(idxStack, 0xff, sizeof(idxStack));)
	u32 idxStackTop = 0;

	u32 idx0=0, idx1=1, idx2=2, nextIdx=3;
	u16 *outIdxs = tmpIdxs;
	for (;;)
	{
		const u32 edge = *edges++;

		// Tessellation stopped?
		if (edge >= VG_EDGE_STOP_TESS)
		{
			// Store the output triangle indices.
			Assert(outIdxs+3 <= tmpIdxs+VG_MAX_TESSELLATION_NUM_TEMP_IDXS);
			*outIdxs++ = (u16)idx0;
			*outIdxs++ = (u16)idx1;
			*outIdxs++ = (u16)idx2;

			// Lsb of edge flag indicates whether the triangle was fully
			// tessellated and stopped only because of the size becoming too
			// small (edge==0xff), or if the triangle is not fully tessellated
			// and stopped because it no longer intersects the smash sphere
			// (edge==0xfe).
			*tessFlags++ = (edge&1);

			// If the stack is empty, then we have finished.
			if (!idxStackTop)
			{
				break;
			}

			// Pop the next triangle off the stack and loop around to process
			// it.
			Assert(idxStackTop >= 3);
			idx0 = idxStack[idxStackTop-3];
			idx1 = idxStack[idxStackTop-2];
			idx2 = idxStack[idxStackTop-1];
			idxStackTop -= 3;
			continue;
		}

		const Vector_4V ratio = V4SplatX(V4IntToFloatRaw<VG_TESS_FIXED_POINT>(V4LoadLeft(ratios++)));

		const u32 maskLongestIs01 = (u32)((s32)(edge-1)>>1);
		const u32 maskLongestIs12 = (u32)((s32)(edge<<31)>>31);
		const u32 maskLongestIs20 = (u32)((s32)(edge<<30)>>31);

		// Rotate the triangle indices so that interpolation is done from idxA
		// to idxB.  idxP is the other input vertex, and idxN is the new
		// interpolated vertex.
		//
		//                    P
	    //                    +
	    //                  / | \
		//                /   |   \
		//              /     |     \
		//            /       |       \
		//          /         |         \
		//        /           |           \
		//    A +-------------+-------------+ B
		//                    N
		//
		const u32 idxA = ((idx0&maskLongestIs01) | (idx1&maskLongestIs12) | (idx2&maskLongestIs20));
		const u32 idxB = ((idx1&maskLongestIs01) | (idx2&maskLongestIs12) | (idx0&maskLongestIs20));
		ASSERT_ONLY(const u32 idxP = ((idx2&maskLongestIs01) | (idx0&maskLongestIs12) | (idx1&maskLongestIs20));)
		Assert(idxA < nextIdx);
		Assert(idxB < nextIdx);
		Assert(idxP < nextIdx);
		const u32 idxN = nextIdx++;
		Assert(nextIdx < VG_MAX_TESSELLATION_NUM_TEMP_VTXS);

		// Fetch the input vertices.
		const Vector_4V posA = tmpVtxs[idxA].pos;
		const Vector_4V posB = tmpVtxs[idxB].pos;
		const Vector_4V texA = tmpVtxs[idxA].tex;
		const Vector_4V texB = tmpVtxs[idxB].tex;
		const Vector_4V nrmA = tmpVtxs[idxA].nrm;
		const Vector_4V nrmB = tmpVtxs[idxB].nrm;
		const Vector_4V colA = tmpVtxs[idxA].col;
		const Vector_4V colB = tmpVtxs[idxB].col;
		const Vector_4V difA = tmpVtxs[idxA].dif;
		const Vector_4V difB = tmpVtxs[idxB].dif;

		// Interpolate to generate new vertex.
		const Vector_4V posN = V4AddScaled(posA,V4Subtract(posB,posA),ratio);
		const Vector_4V texN = V4AddScaled(texA,V4Subtract(texB,texA),ratio);
		const Vector_4V nrmN = V3NormalizeFast(V4AddScaled(nrmA,V4Subtract(nrmB,nrmA),ratio));
		const Vector_4V colN = V4AddScaled(colA,V4Subtract(colB,colA),ratio);
		const Vector_4V difN = V4AddScaled(difA,V4Subtract(difB,difA),ratio);

		// Store to idxN
		tmpVtxs[idxN].pos = posN;
		tmpVtxs[idxN].tex = texN;
		tmpVtxs[idxN].nrm = nrmN;
		tmpVtxs[idxN].col = colN;
		tmpVtxs[idxN].dif = difN;

		// Push triangle B onto stack
		// The indices need to be orderred to match edges iv, v and vi in RecursiveTessellate*
		//
		//            longest edge
		//  index     0    1    2
		//    0       1    N    1
		//    1       2    2    2
		//    2       N    0    N
		//
		idxStack[idxStackTop+0] = (u16)((idx1&~maskLongestIs12) | (idxN&maskLongestIs12));
		idxStack[idxStackTop+1] = (u16)idx2;
		idxStack[idxStackTop+2] = (u16)((idxN&~maskLongestIs12) | (idx0&maskLongestIs12));
		idxStackTop += 3;
		Assert(idxStackTop <= NELEM(idxStack));

		// Loop around and process triangle A
		// The indices need to be orderred to match edges i, ii and iii in RecursiveTessellate*
		//
		//            longest edge
		//  index     0    1    2
		//    0       0    0    0
		//    1       N    1    1
		//    2       2    N    N
		//
	  //idx0 = idx0;
		idx1 = (idx1&~maskLongestIs01) | (idxN&maskLongestIs01);
		idx2 = (idxN&~maskLongestIs01) | (idx2&maskLongestIs01);
	}

	*numVtxs = nextIdx;
	*numIdxs = (unsigned)(outIdxs-tmpIdxs);
}

void CVehicleGlassComponent::CheckDistanceField(CVGTmpVertex *tmpVtxs, unsigned numVtxs, const fwVehicleGlassWindowProxy *pProxy, float distThresh)
{
	using namespace Vec;

	CVGTmpVertex *vtx;
	if (pProxy)
	{
		// Initially each vertex contains the floating point distance field
		// coordinates in the x and y components.  Convert these to integer
		// distance field coordinates in x and y, and lerp values in z and w.
		//
		// Also do the colour compression in this same loop to make better use
		// of vector intstruction latencies.
		//
		vtx = tmpVtxs;
		const unsigned numVtxs4 = (numVtxs+3)&~3;
		for (unsigned vi=0; vi<numVtxs4; vi+=4)
		{
#define LOAD_VERTEX(N)                                                         \
			const Vector_4V fcoord_##N = vtx[N].dif;                           \
			const Vec4V colourF4_##N(vtx[N].col)

			LOAD_VERTEX(0);
			LOAD_VERTEX(1);
			LOAD_VERTEX(2);
			LOAD_VERTEX(3);

#undef LOAD_VERTEX

			if (vi+8 < numVtxs)
			{
				PrefetchDC(vtx+8);
			}

#define PROCESS_VERTEX(N)                                                      \
			/* Distance field coordinate conversion */                         \
			const Vector_4V floored_##N = V4RoundToNearestIntNegInf(fcoord_##N); \
			const Vector_4V lerp_##N    = V4Subtract(fcoord_##N,floored_##N);  \
			const Vector_4V icoord_##N  = V4FloatToIntRaw<0>(floored_##N);     \
			vtx[N].dif = V4PermuteTwo<X1,Y1,X2,Y2>(icoord_##N,lerp_##N);       \
                                                                               \
			/* Colour compression */                                           \
			Color32_StoreDeviceFromRGBA((u32*)&(vtx[N].pos)+3, colourF4_##N)

			PROCESS_VERTEX(0);
			PROCESS_VERTEX(1);
			PROCESS_VERTEX(2);
			PROCESS_VERTEX(3);

#undef PROCESS_VERTEX

			vtx += 4;
		}

		// Given the integer distance field coordinates and lerp values, lookup
		// the distance value and store back into z component of the fifth
		// vector.  Having a seperate loop here avoid lots of LHSs from the
		// vector calculations for the distance field coordinates above.
		vtx = tmpVtxs;
		const bool bWindowUseUncompressedData = VEHICLE_GLASS_DEV_SWITCH(g_vehicleGlassMan.m_distanceFieldUncompressed, false);
		const ScalarV detachableTrue(V_INT_1);
		const ScalarV distThreshV(distThresh);
		for (unsigned vi=0; vi<numVtxs; ++vi)
		{
			s32*   pi = (s32*)  &(vtx->dif);
			float* pf = (float*)&(vtx->dif);
			const unsigned i0 = pi[0];
			const unsigned j0 = pi[1];
			const Vector_4V lerpi = V4LoadLeft(pf+2);
			const Vector_4V lerpj = V4LoadLeft(pf+3);

			if (vi+2 < numVtxs)
			{
				PrefetchDC(vtx+2);
			}

			const ScalarV dist = pProxy->GetDistanceLerpIJ(i0, j0, ScalarV(V4SplatX(lerpi)), ScalarV(V4SplatX(lerpj)), bWindowUseUncompressedData);
			const ScalarV mask = ScalarV(IsGreaterThan(dist, distThreshV).GetIntrin128());
			StoreScalar32FromScalarV(pf[3], And(mask, detachableTrue));
			++vtx;
		}
	}
	else
	{
		vtx = tmpVtxs;
		for (unsigned vi=0; vi<numVtxs; ++vi)
		{
			s32* pi = (s32*)&(vtx->dif);
			pi[3] = 0;
			++vtx;
		}
	}
}

void CVehicleGlassComponent::StoreStackTris(CVGStackTriangle *outTris, const CVGTmpVertex *tmpVtxs, const u16 *tmpIdxs, const bool *tessFlags, unsigned numTris, u32 detachThresh)
{
	using namespace Vec;

#if VEHICLE_GLASS_COMPRESSED_STACK_TRIANGLES
	CVGStackTriangle *tri = outTris;
	for (unsigned t=0; t<numTris; ++t)
	{
		const unsigned i0 = tmpIdxs[t*3+0];
		const unsigned i1 = tmpIdxs[t*3+1];
		const unsigned i2 = tmpIdxs[t*3+2];
		const Vec3V P0(tmpVtxs[i0].pos);
		const Vec3V P1(tmpVtxs[i1].pos);
		const Vec3V P2(tmpVtxs[i2].pos);
		const Vec4V T0(tmpVtxs[i0].tex);
		const Vec4V T1(tmpVtxs[i1].tex);
		const Vec4V T2(tmpVtxs[i2].tex);
		const Vec3V N0(tmpVtxs[i0].nrm);
		const Vec3V N1(tmpVtxs[i1].nrm);
		const Vec3V N2(tmpVtxs[i2].nrm);
		const Vec4V C0(tmpVtxs[i0].col);
		const Vec4V C1(tmpVtxs[i1].col);
		const Vec4V C2(tmpVtxs[i2].col);
		const s32 canDetach0 = GetWi(tmpVtxs[i0].dif);
		const s32 canDetach1 = GetWi(tmpVtxs[i1].dif);
		const s32 canDetach2 = GetWi(tmpVtxs[i2].dif);
		const bool bTessellated = tessFlags[t];
		const bool bCanDetach = ((canDetach0+canDetach1+canDetach2) >= detachThresh);
		BANK_ONLY(const bool bHasError = false;)
		tri->SetVertexP(0, P0);
		tri->SetVertexP(1, P1);
		tri->SetVertexP(2, P2);
		tri->SetVertexTNC(0, T0, N0, C0);
		tri->SetVertexTNC(1, T1, N1, C1);
		tri->SetVertexTNC(2, T2, N2, C2);
		tri->SetFlags(bTessellated, bCanDetach BANK_ONLY(, bHasError));
		++tri;
	}

#else
	const Vector_4V vIntOne = V4VConstant(V_INT_1);
	const Vector_4V detachThreshSub1 = V4SubtractInt(V4LoadScalar32IntoSplatted(detachThresh), vIntOne);
	const Vector_4V canDetachMask = V4VConstant<0,0,0,CVGStackTriangle::TF_CanDetach>();
	CVGStackTriangle *tri = outTris;
	for (unsigned t=0; t<numTris; ++t)
	{
		const unsigned i0 = tmpIdxs[t*3+0];
		const unsigned i1 = tmpIdxs[t*3+1];
		const unsigned i2 = tmpIdxs[t*3+2];
		const Vec4V PC0(tmpVtxs[i0].pos);
		const Vec4V PC1(tmpVtxs[i1].pos);
		const Vec4V PC2(tmpVtxs[i2].pos);
		const Vec4V T0(tmpVtxs[i0].tex);
		const Vec4V T1(tmpVtxs[i1].tex);
		const Vec4V T2(tmpVtxs[i2].tex);
		const Vec4V N0(tmpVtxs[i0].nrm);
		const Vec4V N1(tmpVtxs[i1].nrm);
		const Vec4V N2(tmpVtxs[i2].nrm);
		const Vector_4V canDetach0  = tmpVtxs[i0].dif;
		const Vector_4V canDetach1  = tmpVtxs[i1].dif;
		const Vector_4V canDetach2  = tmpVtxs[i2].dif;
		const Vector_4V canDetach01 = V4AddInt(canDetach0, canDetach1);
		const Vector_4V threshSub2  = V4SubtractInt(detachThreshSub1, canDetach2);
		const Vector_4V canDetach   = V4And(V4IsGreaterThanIntV(canDetach01, threshSub2), canDetachMask);
		CompileTimeAssert(CVGStackTriangle::TF_Tessellated == 1);
#if __BE
		const Vector_4V tessellated = V4And(V4LoadUnalignedSafe<4>(tessFlags+t-3), vIntOne);
#else
		const Vector_4V tessellated = V4And(V4LoadUnalignedSafe<1>(tessFlags+t),   vIntOne);
#endif
		const Vec4V NF0(V4Or(V4PermuteTwo<X1,Y1,Z1,X2>(N0.GetIntrin128(), tessellated), canDetach));
		tri->SetVertexPC(0, PC0);
		tri->SetVertexPC(1, PC1);
		tri->SetVertexPC(2, PC2);
		tri->SetVertexTNF(0, T0, NF0);
		tri->SetVertexTNF(1, T1, N1);
		tri->SetVertexTNF(2, T2, N2);
		++tri;
	}

#endif
}

void CVehicleGlassComponent::UpdateVB(const CVGStackTriangle* triangleArray, u32 nextFreeTriangle)
{
	if (m_pAttachedVB)
	{
		CVehicleGlassVB::Destroy(m_pAttachedVB);
		m_pAttachedVB = NULL;
	}

	u32 attachedTriangleCount = 0;

	for (int iCurTriangle = 0; iCurTriangle < nextFreeTriangle; iCurTriangle++)
	{
		if (!triangleArray[iCurTriangle].Detached())
		{
			attachedTriangleCount++;
		}
	}

	if (attachedTriangleCount == 0)
	{
		m_nothingToDetach = true;

		return; // No need to allocate a buffer for fully broken glass
	}

	// Create a new VB object
	m_pAttachedVB = CVehicleGlassVB::Create();

	if (m_pAttachedVB == NULL)
	{
		if (PARAM_vgasserts.Get())
		{
			vfxAssertf(0, "%s: CVehicleGlassComponent::UpdateVB - VB pool is full (Current size = %d)", GetModelName(), g_vehicleGlassMan.m_componentVBPool->GetNumItems());
		}

		return;
	}

	// Preallocate the memory from the streaming heap
	const int vertCount = attachedTriangleCount*3;

	if (!m_pAttachedVB->AllocateBuffer(vertCount))
	{
		// Streaming heap is full?
		if (PARAM_vgasserts.Get())
		{
			vfxAssertf(0, "%s: CVehicleGlassComponent::UpdateVB - AllocateBuffer failed (%d verts)", GetModelName(), vertCount);
		}

		CVehicleGlassVB::Destroy(m_pAttachedVB, true);
		m_pAttachedVB = NULL;
		return;
	}

	// Go over all the attached triangles and add them to the buffer
	int iVert = 0;

#if !USE_GPU_VEHICLEDEFORMATION
	void* pDamageTexBasePtr = NULL;
	ScalarV invDamageRadiusV;
	CPhysical* pPhysical = m_regdPhy.Get();

	if (pPhysical && pPhysical->GetIsTypeVehicle())
	{
		fwTexturePayload* pDamageTex = NULL;
		float damageRadius = 0.0f;
		g_pDecalCallbacks->GetDamageData(pPhysical, &pDamageTex, damageRadius);

		if (pDamageTex)
		{
			invDamageRadiusV = ScalarV(1.0f/damageRadius);
			pDamageTexBasePtr = pDamageTex->AcquireBasePtr(grcsRead);
		}
	}
#endif // !USE_GPU_VEHICLEDEFORMATION

#if ADD_PED_SAFE_ZONE_TO_VEHICLES
	const CVehicle* pVehicle = static_cast<const CVehicle*>(m_regdPhy.Get());
#endif 

	m_nothingToDetach = true;

	for (int iCurTriangle = 0; iCurTriangle < nextFreeTriangle; iCurTriangle++)
	{
		CVGStackTriangle triangle = triangleArray[iCurTriangle];

		if (!triangle.Detached())
		{
			ScalarV dmgMult = ScalarV(V_ONE);
#if ADD_PED_SAFE_ZONE_TO_VEHICLES
			Vec3V vCenterPoint = triangle.GetVertexP(0) + triangle.GetVertexP(1) + triangle.GetVertexP(2);
			vCenterPoint *= ScalarV(1.0f/3.0f);
			dmgMult = pVehicle->GetVehicleDamage()->GetDeformation()->GetPedSafeAreaMultiplier(vCenterPoint);
#endif

			for (int j = 0; j < 3; j++, iVert++)
			{
				// Gather the vertex uncompressed data
				Vec3V pos = triangle.GetVertexP(j);
				Vec3V nrm;
				Vec4V tex;
				Vec4V col;
				Vec3V damagedPos = pos;
#if !USE_GPU_VEHICLEDEFORMATION
				if (pDamageTexBasePtr)
				{
					damagedPos += CVehicleDeformation::ReadFromVectorOffset(pDamageTexBasePtr, pos, invDamageRadiusV);
				}
#endif // !USE_GPU_VEHICLEDEFORMATION

				triangle.GetVertexTNC(j, tex, nrm, col);

				const bool bTessellated = triangle.Tessellated();
				const bool bCanDetach = triangle.CanDetach();

				m_pAttachedVB->SetVertex(iVert, pos, damagedPos, dmgMult, nrm, tex, col, bTessellated, bCanDetach BANK_ONLY(, triangle.HasError()));

				// Only fully damaged if all triangles are fully tessellated and can't detach
				m_nothingToDetach = m_nothingToDetach && bTessellated && !bCanDetach;

				/*Vec3V pos1;
				Vec3V nrm1;
				Vec4V tex1;
				Vec4V col1;
				pos1 = m_pAttachedVB->GetVertexP(iVert);
				Assert(Dist(pos, pos1).Getf() < 0.005f);
				nrm1 = m_pAttachedVB->GetVertexN(iVert);
				Assert(Dist(nrm, nrm1).Getf() < 0.005f);
				m_pAttachedVB->GetVertexTNC(iVert, tex1, nrm1, col1);
				Assert(Dist(nrm, nrm1).Getf() < 0.005f);
				Assert(Dist(tex, tex1).Getf() < 0.005f);
				Assert(Dist(col, col1).Getf() < 0.005f);*/
			}
		}
	}

#if !RSG_PC
	// Allocate the vertex buffer
	if (!m_pAttachedVB->AllocateVB())
	{
		if (PARAM_vgasserts.Get())
		{
			vfxAssertf(0, "%s: CVehicleGlassComponent::UpdateVB - AllocateVB failed (%d verts)", GetModelName(), vertCount);
		}

		CVehicleGlassVB::Destroy(m_pAttachedVB, true);
		m_pAttachedVB = NULL;
	}
#endif

	m_damageDirty = false; // The new VB contains the updated damage information
}

void CVehicleGlassComponent::UpdateVBDamage(const Vec3V* damageArray)
{
	Assert(damageArray);
	Assert(m_pAttachedVB && m_pAttachedVB->GetVertexCount() > 0);

	CVehicleGlassVB* prevVB = m_pAttachedVB;

	// Create a new VB object
	m_pAttachedVB = CVehicleGlassVB::Create();

	if (m_pAttachedVB == NULL)
	{
		if (PARAM_vgasserts.Get())
		{
			vfxAssertf(0, "%s: CVehicleGlassComponent::UpdateVB - VB pool is full (Current size = %d)", GetModelName(), g_vehicleGlassMan.m_componentVBPool->GetNumItems());
		}

		CVehicleGlassVB::Destroy(prevVB); // Clean the previous VB

		return;
	}

	// Preallocate the memory from the streaming heap
	const int vertCount = prevVB->GetVertexCount();

	if (!m_pAttachedVB->AllocateBuffer(vertCount))
	{
		// Streaming heap is full?
		if (PARAM_vgasserts.Get())
		{
			vfxAssertf(0, "%s: CVehicleGlassComponent::UpdateVB - AllocateBuffer failed (%d verts)", GetModelName(), vertCount);
		}

		CVehicleGlassVB::Destroy(prevVB); // Clean the previous VB
		CVehicleGlassVB::Destroy(m_pAttachedVB, true);
		m_pAttachedVB = NULL;
		return;
	}

	// Go over all the triangles and copy them to the new buffer with the new damage offsets
	for (int iCurVert = 0; iCurVert < prevVB->GetVertexCount(); iCurVert++)
	{
		// Gather the vertex uncompressed data
		Vec3V pos = prevVB->GetVertexP(iCurVert);
		Vec3V damagedPos = pos + damageArray[iCurVert];
		Vec3V nrm;
		Vec4V tex;
		Vec4V col;
		prevVB->GetVertexTNC(iCurVert, tex, nrm, col);

		bool bTessellated;
		bool bCanDetach;
		BANK_ONLY(bool bHasError);
		prevVB->GetFlags(iCurVert, bTessellated, bCanDetach BANK_ONLY(, bHasError));

		ScalarV dmgMult = ScalarV(V_ONE);

#if ADD_PED_SAFE_ZONE_TO_VEHICLES
		const CVehicle* pVehicle = static_cast<const CVehicle*>(m_regdPhy.Get());
		dmgMult = pVehicle->GetVehicleDamage()->GetDeformation()->GetPedSafeAreaMultiplier(pos);
#endif 
		m_pAttachedVB->SetVertex(iCurVert, pos, damagedPos, dmgMult, nrm, tex, col, bTessellated, bCanDetach BANK_ONLY(, bHasError));
	}

#if !RSG_PC
	// Allocate the vertex buffer
	if (!m_pAttachedVB->AllocateVB())
	{
		if (PARAM_vgasserts.Get())
		{
			vfxAssertf(0, "%s: CVehicleGlassComponent::UpdateVB - AllocateVB failed (%d verts)", GetModelName(), vertCount);
		}

		CVehicleGlassVB::Destroy(m_pAttachedVB, true);
		m_pAttachedVB = NULL;
	}
#endif

	CVehicleGlassVB::Destroy(prevVB); // Clean the previous VB
}

void CVehicleGlassComponent::UpdateSmashSphere(Vec3V_In vSmashPosObj, float radius)
{
	const bool bWindowHasExposedEdge = m_pWindow && (m_pWindow->GetFlags() & fwVehicleGlassWindow::VGW_FLAG_HAS_EXPOSED_EDGES) != 0;
	if (IsEqualAll(m_smashSphere.GetW(), ScalarV(V_ZERO))) // this is the first smash
	{
		// Windows with exposed edges should start with a larger radius
		float minRadius = bWindowHasExposedEdge ? VEHICLE_GLASS_SMASH_SPHERE_RADIUS_MIN_EXPOSED : VEHICLE_GLASS_SMASH_SPHERE_RADIUS_MIN;
		m_smashSphere = Vec4V(vSmashPosObj, ScalarV(Max<float>(radius, minRadius)));
	}
	else // we need to calc a new sphere which encompasses the new collision
	{
		spdSphere sphere0(m_smashSphere);
		spdSphere sphere1(vSmashPosObj, ScalarV(radius));
		sphere0.GrowSphere(sphere1);

		// When exposed edges are involved, we want to just fully break the window once the radius is large enough
		if (bWindowHasExposedEdge)
		{
			const float fNewRadius = sphere0.GetRadiusf();

			if (fNewRadius >= VEHICLEGLASSGROUP_EXPOSED_SMASH_RADIUS)
			{
				sphere0.SetRadiusf(VEHICLEGLASSGROUP_SMASH_RADIUS);
			}
		}

		m_smashSphere = sphere0.GetV4();
	}
}

grcFvf                CVehicleGlassVB::m_fvf;
grcVertexDeclaration* CVehicleGlassVB::m_vtxStaticDecl = NULL;

int CVehicleGlassVB::ms_MemoryUsed = 0;

void CVehicleGlassVB::CreateFvf(grcFvf *pFvf)
{
		pFvf->SetPosChannel(true, grcFvf::grcdsHalf4); // POSITION
		pFvf->SetNormalChannel(true, grcFvf::grcdsHalf4); // NORMAL
		pFvf->SetDiffuseChannel(true, grcFvf::grcdsColor); // COLOR0
		pFvf->SetTextureChannel(0, true, grcFvf::grcdsHalf2); // TEXCOORD0
		pFvf->SetTextureChannel(1, true, grcFvf::grcdsHalf2); // TEXCOORD1
#if USE_GPU_VEHICLEDEFORMATION
		pFvf->SetTextureChannel(2, true, grcFvf::grcdsHalf2); // TEXCOORD2
#endif
}

CVehicleGlassVB* CVehicleGlassVB::Create()
{
	// Prepare the flexible vertex format
	if (m_fvf.GetTotalSize() == 0)
	{
		CreateFvf(&m_fvf);

		m_vtxStaticDecl = grmModelFactory::BuildDeclarator(&m_fvf, NULL, NULL, NULL);
	}

	CVehicleGlassVB* pNewVB = g_vehicleGlassMan.m_componentVBList.GetFromPool(g_vehicleGlassMan.m_componentVBPool);

	if (pNewVB)
	{
		pNewVB->m_pVBPreBuffer = NULL;
#if RSG_PC
		pNewVB->m_BGvtxBufferIndex = -1;
		pNewVB->m_pVertexBuffer = NULL;
#else
		pNewVB->m_pVertexBuffer = NULL;
#endif
		pNewVB->m_lastRendered = 0;
	}

	return pNewVB;
}

#if RSG_PC
void CVehicleGlassVB::DestroyVB()
{
	if (m_BGvtxBufferIndex != -1)
	{
		g_vehicleBrokenGlassVtxBuffer[m_BGvtxBufferIndex].bInUse = false;
		g_vehicleBrokenGlassVtxBuffer[m_BGvtxBufferIndex].bNeedUpdate = true;
		m_pVertexBuffer = NULL;
		m_BGvtxBufferIndex = -1;
	}
}
#endif

void CVehicleGlassVB::Destroy(CVehicleGlassVB* pVB, bool bImmediate)
{
	Assert(pVB);

	if (bImmediate || pVB->m_lastRendered == 0)
	{
		if (pVB->m_pVertexBuffer)
		{
#if RSG_PC
			pVB->DestroyVB();
#else
			delete pVB->m_pVertexBuffer;
			pVB->m_pVertexBuffer = NULL;
#endif
		}

		if (pVB->m_pVBPreBuffer)
		{
			ms_MemoryUsed -= pVB->m_vertexCount * m_fvf.GetTotalSize();

			{
				MEM_USE_USERDATA(MEMUSERDATA_VEHGLASS);
				strStreamingEngine::GetAllocator().Free(pVB->m_pVBPreBuffer);
			}

			pVB->m_pVBPreBuffer = NULL;
		}

		g_vehicleGlassMan.m_componentVBList.ReturnToPool(pVB, g_vehicleGlassMan.m_componentVBPool);

#if VEHICLE_GLASS_VERBOSE_DEBUG_OUTPUT
		if (g_vehicleGlassMan.m_verboseHit)
		{
			Displayf("CVehicleGlassVB::Destroy 0x%p return to pool immediate - lists size %d %d", pVB, g_vehicleGlassMan.m_componentVBList.GetNumItems(), g_vehicleGlassMan.m_componentCooldownVBList.GetNumItems());
		}
#endif // VEHICLE_GLASS_VERBOSE_DEBUG_OUTPUT
	}
	else
	{
		// Start the cooldown
		for (CVehicleGlassVB* pCurVB = g_vehicleGlassMan.m_componentVBList.GetHead(); pCurVB; pCurVB = g_vehicleGlassMan.m_componentVBList.GetNext(pCurVB))
		{
			if (pCurVB == pVB)
			{
				g_vehicleGlassMan.m_componentVBList.RemoveItem(pCurVB);
				g_vehicleGlassMan.m_componentCooldownVBList.AddItem(pCurVB);
				break;
			}

			vfxAssertf(pCurVB, "CVehicleGlassVB::Destroy called but VB 0x%p not found", pVB);
		}

#if VEHICLE_GLASS_VERBOSE_DEBUG_OUTPUT
		if (g_vehicleGlassMan.m_verboseHit)
		{
			Displayf("CVehicleGlassVB::Destroy 0x%p start cool down - lists size %d %d", pVB, g_vehicleGlassMan.m_componentVBList.GetNumItems(), g_vehicleGlassMan.m_componentCooldownVBList.GetNumItems());
		}
#endif // VEHICLE_GLASS_VERBOSE_DEBUG_OUTPUT
	}
}

void CVehicleGlassVB::Update()
{
	u32 frameCount = fwTimer::GetFrameCount();
	for (CVehicleGlassVB* pCurVB = g_vehicleGlassMan.m_componentCooldownVBList.GetHead(); pCurVB; )
	{
		// Check if the cooldown is over
		if (pCurVB->m_lastRendered > 0 && pCurVB->m_lastRendered + g_vehicleGlassVBCoolDown <= frameCount)
		{
#if VEHICLE_GLASS_VERBOSE_DEBUG_OUTPUT
			if (g_vehicleGlassMan.m_verboseHit)
			{
				Displayf("CVehicleGlassVB::Update 0x%p cool down over - lists size %d %d", pCurVB, g_vehicleGlassMan.m_componentVBList.GetNumItems(), g_vehicleGlassMan.m_componentCooldownVBList.GetNumItems());
			}
#endif // VEHICLE_GLASS_VERBOSE_DEBUG_OUTPUT

			if (pCurVB->m_pVertexBuffer)
			{
#if RSG_PC
				pCurVB->DestroyVB();
#else
				delete pCurVB->m_pVertexBuffer;
				pCurVB->m_pVertexBuffer = NULL;
#endif	
			}

			if (pCurVB->m_pVBPreBuffer)
			{
				ms_MemoryUsed -= pCurVB->m_vertexCount * m_fvf.GetTotalSize();;

				{
					MEM_USE_USERDATA(MEMUSERDATA_VEHGLASS);
					strStreamingEngine::GetAllocator().Free(pCurVB->m_pVBPreBuffer);
				}

				pCurVB->m_pVBPreBuffer = NULL;
			}

			pCurVB = g_vehicleGlassMan.m_componentCooldownVBList.ReturnToPool_GetNext(pCurVB, g_vehicleGlassMan.m_componentVBPool);

			continue;
		}

#if VEHICLE_GLASS_VERBOSE_DEBUG_OUTPUT
		if (g_vehicleGlassMan.m_verboseHit)
		{
			Displayf("CVehicleGlassVB::Update 0x%p in cool down (%d) - lists size %d %d", pCurVB, frameCount - pCurVB->m_lastRendered, g_vehicleGlassMan.m_componentVBList.GetNumItems(), g_vehicleGlassMan.m_componentCooldownVBList.GetNumItems());
		}
#endif // VEHICLE_GLASS_VERBOSE_DEBUG_OUTPUT

		pCurVB = g_vehicleGlassMan.m_componentCooldownVBList.GetNext(pCurVB);
	}
}

bool CVehicleGlassVB::AllocateBuffer(int vertCount)
{
	// Calculate the size of the memory chunk used by the new buffer
	const int bufferSize = vertCount*m_fvf.GetTotalSize();

	// Make sure previous values are cleared
	m_pVBPreBuffer = NULL;
	m_vertexCount = 0;

	// If the game is running in MP mode inforce the memory cap
	if(ms_MemoryUsed + bufferSize <= g_vehicleGlassVBMemCapMP || (!CNetwork::IsGameInProgress() BANK_ONLY(&& !g_vehicleGlassMan.m_testMPMemCap) ))
	{
		MEM_USE_USERDATA(MEMUSERDATA_VEHGLASS);
		m_pVBPreBuffer = strStreamingEngine::GetAllocator().Allocate(bufferSize, RAGE_VERTEXBUFFER_ALIGNMENT, MEMTYPE_RESOURCE_VIRTUAL);

		if(m_pVBPreBuffer)
		{
			ms_MemoryUsed += bufferSize;
			m_vertexCount = vertCount;
		}
	}

	return m_pVBPreBuffer != NULL;
}

#if RSG_PC
bool CVehicleGlassVB::UpdateVB()
{
	if (m_pVertexBuffer == NULL)
	{
		int vtxBufIndex = 0;
		int index = 0;
		for (index = 0; index < MAX_BROKENGLASS_NUM; index++)
		{
			if (g_vehicleBrokenGlassVtxBuffer[index].bInUse == false)
			{
				vtxBufIndex = index;
				break;
			}
		}

		if (index == MAX_BROKENGLASS_NUM)
		{
			vfxAssertf(false, "Max number of broken glass has reached. not rendering this broken glass.\n");
			return false;
		}

		g_vehicleBrokenGlassVtxBuffer[vtxBufIndex].bInUse = true;
		g_vehicleBrokenGlassVtxBuffer[vtxBufIndex].bNeedUpdate = true;
		m_pVertexBuffer = g_vehicleBrokenGlassVtxBuffer[vtxBufIndex].pVtxBuffer;
		m_BGvtxBufferIndex = vtxBufIndex;
	}

	if (g_vehicleBrokenGlassVtxBuffer[m_BGvtxBufferIndex].bNeedUpdate)
	{
		ASSERT_ONLY(if (m_vertexCount > MAX_VERT_COUNT) vfxWarningf("Max vert count exceeded for broken glass. Current vert count is %d.\n",m_vertexCount));

		grcLockType eLockType = grcsNoOverwrite;
		m_pVertexBuffer->Lock(eLockType, 0, m_fvf.GetTotalSize() * MAX_VERT_COUNT);

		void *p = m_pVertexBuffer->GetLockPtr();
		memcpy(p, m_pVBPreBuffer, m_fvf.GetTotalSize() * Min(m_vertexCount, MAX_VERT_COUNT));

		m_pVertexBuffer->UnlockRW();
		g_vehicleBrokenGlassVtxBuffer[m_BGvtxBufferIndex].bNeedUpdate = false;
	}

	return true;
}
#endif

bool CVehicleGlassVB::AllocateVB()
{
#if !RSG_DURANGO
	const bool bReadWrite = true;
#if __PS3
	const bool bDynamic = true; // PS3 expects VB allocated on main memory to be dynamic
#else
	const bool bDynamic = false;
#endif // __PS3
#endif

#if !RSG_DURANGO
	m_pVertexBuffer = grcVertexBuffer::Create(m_vertexCount, m_fvf, bReadWrite, bDynamic, m_pVBPreBuffer);
#else // !RSG_DURANGO
	m_pVertexBuffer = grcVertexBuffer::CreateWithData(m_vertexCount, m_fvf, m_pVBPreBuffer);
#endif // !RSG_DURANGO

	return m_pVertexBuffer != NULL;
}

Vec3V_Out CVehicleGlassVB::GetVertexP(int idx) const
{
	return VehicleGlassVertex_GetVertexP((u8*)m_pVBPreBuffer + idx*m_fvf.GetTotalSize());
}

Vec3V_Out CVehicleGlassVB::GetDamagedVertexP(int idx) const
{
	return VehicleGlassVertex_GetDamagedVertexP((u8*)m_pVBPreBuffer + idx*m_fvf.GetTotalSize());
}

Vec3V_Out CVehicleGlassVB::GetVertexN(int idx) const
{
	return VehicleGlassVertex_GetVertexN((u8*)m_pVBPreBuffer + idx*m_fvf.GetTotalSize());
}

void CVehicleGlassVB::GetVertexTNC(int idx, Vec4V_InOut tex, Vec3V_InOut nrm, Vec4V_InOut col) const
{
	return VehicleGlassVertex_GetVertexTNC((u8*)m_pVBPreBuffer + idx*m_fvf.GetTotalSize(), tex, nrm, col);
}

void CVehicleGlassVB::GetFlags(int idx, bool& bTessellated, bool& bCanDetach BANK_ONLY(, bool& bHasError)) const
{
	// We store the flags in teh green component of the colour
	const u8* pRawVB = (u8*)m_pVBPreBuffer + idx*m_fvf.GetTotalSize();
	const u32 *ptrU32 = (u32*)(pRawVB + 2*sizeof(Float16Vec4));
	Color32 col32;
	col32.SetFromDeviceColor(*ptrU32);
	const u8 flags = col32.GetGreen();
	bTessellated = (flags & TF_Tessellated) != 0;
	bCanDetach = (flags & TF_CanDetach) != 0;
	BANK_ONLY(bHasError = (flags & TF_HasError) != 0);
}

void CVehicleGlassVB::SetVertex(int idx, Vec3V_In pos, Vec3V_In dmgPos, ScalarV_In dmgMult, Vec3V_In nrm, Vec4V_In tex, Vec4V_In col, bool bTessellated, bool bCanDetach BANK_ONLY(, bool bHasError))
{
	// Copy the data into the buffer
	u8* pRawVB = (u8*)m_pVBPreBuffer + idx*m_fvf.GetTotalSize();
	Float16Vec4* ptrFloat16Vec4 = (Float16Vec4*)pRawVB;
#if USE_GPU_VEHICLEDEFORMATION
	Float16Vec4Pack(ptrFloat16Vec4++, pos);
#else
	Float16Vec4Pack(ptrFloat16Vec4++, dmgPos); // Damaged position has to be set as the actual position so we use it for rendering
#endif // USE_GPU_VEHICLEDEFORMATION
	Float16Vec4Pack(ptrFloat16Vec4++, nrm);
	u32 *ptrU32 = (u32*)(pRawVB + 2*sizeof(Float16Vec4));
	Color32 col32(col);
	col32.SetGreen((bTessellated ? TF_Tessellated : 0) | (bCanDetach ? TF_CanDetach : 0) BANK_ONLY(| (bHasError ? TF_HasError : 0)));

	// LDS DMC TEMP:-
	//(void)tex;
	//Vec4V texT = Vec4V(V_ZERO);

	*(ptrU32++) = col32.GetDeviceColor();
	ptrFloat16Vec4 = (Float16Vec4*)(pRawVB + 2*sizeof(Float16Vec4) + sizeof(u32));
	Float16Vec4Pack(ptrFloat16Vec4++, tex);

#if USE_GPU_VEHICLEDEFORMATION
	(void)dmgPos; // Not needed when we process damage on the GPU
#if ADD_PED_SAFE_ZONE_TO_VEHICLES
	//Re-enable this to tune ped safe zone in-game for glass to match the rest of the vehicle, otherwise this damage multiplier is already pre-calculated inside the green channel
	Vec2V dmgMultV(dmgMult.Getf(), 1.0);
#else
	Vec2V dmgMultV(1.0f, 1.0f);
#endif
	Float16Vec2* ptrFloat16Vec2 = (Float16Vec2*)(pRawVB + 3*sizeof(Float16Vec4) + sizeof(u32));
	Float16Vec2Pack(ptrFloat16Vec2++,dmgMultV);
#else
	Float16Vec4Pack(ptrFloat16Vec4++, pos); // Store the non-damaged position for reference
#endif // USE_GPU_VEHICLEDEFORMATION
}

const void *CVehicleGlassVB::GetRawVertices() const
{
	return m_pVBPreBuffer;
}

unsigned CVehicleGlassVB::GetVertexStride() const
{
	return m_fvf.GetTotalSize();
}

void CVehicleGlassVB::BindVB()
{
	m_lastRendered = fwTimer::GetFrameCount();
	GRCDEVICE.SetVertexDeclaration(m_vtxStaticDecl);
	GRCDEVICE.SetStreamSource(0, *m_pVertexBuffer, 0, m_pVertexBuffer->GetVertexStride());
}

__forceinline void CVGStackTriangle::SetVertexP(int i, Vec3V_In position)
{
#if VEHICLE_GLASS_COMPRESSED_STACK_TRIANGLES
	Float16Vec4Pack(&m_vertexP[i], position);
#else
	m_vertexPC[i].SetXYZ(position);
#endif
}

#if !VEHICLE_GLASS_COMPRESSED_STACK_TRIANGLES
__forceinline void CVGStackTriangle::SetVertexPC(int i, Vec4V_In positionXYZColourW)
{
	m_vertexPC[i] = positionXYZColourW;
}

__forceinline void CVGStackTriangle::SetVertexTNF(int i, Vec4V_In texcoord, Vec4V_In normalXYZFlagsW)
{
	m_vertexT[i] = texcoord;
	m_vertexNF[i] = normalXYZFlagsW;
}
#endif // !VEHICLE_GLASS_COMPRESSED_STACK_TRIANGLES

__forceinline void CVGStackTriangle::SetVertexTNC(int i, Vec4V_In texcoord, Vec3V_In normal, Vec4V_In colour)
{
#if VEHICLE_GLASS_COMPRESSED_STACK_TRIANGLES
	Float16Vec4Pack(&m_vertexT[i], texcoord);
	Float16Vec4Pack(&m_vertexN[i], normal);
	m_vertexC[i] = Color32(colour).GetDeviceColor();
#else
	m_vertexT[i] = texcoord;
	m_vertexNF[i].SetXYZ(normal);
	Color32_StoreDeviceFromRGBA((u32*)(m_vertexPC+i)+3, colour);
#endif
}

__forceinline void CVGStackTriangle::SetFlags(bool bTessellated, bool bCanDetach BANK_ONLY(, bool bHasError))
{
#if VEHICLE_GLASS_COMPRESSED_STACK_TRIANGLES
	m_bTessellated = bTessellated;
	m_bCanDetach = bCanDetach;
	m_bDetached = false;
	BANK_ONLY(m_bHasError = bHasError);
#else
	u32 flags = bTessellated ? TF_Tessellated : 0;
	flags |= bCanDetach ? TF_CanDetach : 0;
	BANK_ONLY(flags |= bHasError ? TF_HasError : 0);
	m_vertexNF[0].SetWi(flags);
#endif // VEHICLE_GLASS_COMPRESSED_STACK_TRIANGLES
}

__forceinline void CVGStackTriangle::SetDetached()
{
#if VEHICLE_GLASS_COMPRESSED_STACK_TRIANGLES
	m_bDetached = true;
#else
	u32 flags = m_vertexNF[0].GetWi();
	flags |= TF_Detached;
	m_vertexNF[0].SetWi(flags);
#endif // VEHICLE_GLASS_COMPRESSED_STACK_TRIANGLES
}

__forceinline Vec3V_Out CVGStackTriangle::GetVertexP(int i) const
{
#if VEHICLE_GLASS_COMPRESSED_STACK_TRIANGLES
	return Float16Vec4Unpack(&m_vertexP[i]).GetXYZ();
#else
	return m_vertexPC[i].GetXYZ();
#endif // VEHICLE_GLASS_COMPRESSED_STACK_TRIANGLES
}

__forceinline void CVGStackTriangle::GetVertexTNC(int i, Vec4V_InOut texcoord, Vec3V_InOut normal, Vec4V_InOut colour)
{
#if VEHICLE_GLASS_COMPRESSED_STACK_TRIANGLES
	texcoord = Float16Vec4Unpack(&m_vertexT[i]);
	normal = Float16Vec4Unpack(&m_vertexN[i]).GetXYZ();
	Color32 c;
	c.SetFromDeviceColor(m_vertexC[i]);
	colour = c.GetRGBA();
#else
	texcoord = m_vertexT[i];
	normal = m_vertexNF[i].GetXYZ();
	Color32 c;
	c.SetFromDeviceColor(m_vertexPC[i].GetWi());
	colour = c.GetRGBA();
#endif // VEHICLE_GLASS_COMPRESSED_STACK_TRIANGLES
}

__forceinline bool CVGStackTriangle::Tessellated() const
{
#if VEHICLE_GLASS_COMPRESSED_STACK_TRIANGLES
	return m_bTessellated;
#else
	const u32 flags = m_vertexNF[0].GetWi();
	return (flags & TF_Tessellated) != 0;
#endif // VEHICLE_GLASS_COMPRESSED_STACK_TRIANGLES
}

__forceinline bool CVGStackTriangle::CanDetach() const
{
#if VEHICLE_GLASS_COMPRESSED_STACK_TRIANGLES
	return m_bCanDetach;
#else
	const u32 flags = m_vertexNF[0].GetWi();
	return (flags & TF_CanDetach) != 0;
#endif // VEHICLE_GLASS_COMPRESSED_STACK_TRIANGLES
}

__forceinline bool CVGStackTriangle::Detached() const
{
#if VEHICLE_GLASS_COMPRESSED_STACK_TRIANGLES
	return m_bDetached;
#else
	const u32 flags = m_vertexNF[0].GetWi();
	return (flags & TF_Detached) != 0;
#endif // VEHICLE_GLASS_COMPRESSED_STACK_TRIANGLES
}

#if __BANK
__forceinline bool CVGStackTriangle::HasError() const
{
#if VEHICLE_GLASS_COMPRESSED_STACK_TRIANGLES
	return m_bHasError;
#else
	const u32 flags = m_vertexNF[0].GetWi();
	return (flags & TF_HasError) != 0;
#endif // VEHICLE_GLASS_COMPRESSED_STACK_TRIANGLES
}
#endif // __BANK


///////////////////////////////////////////////////////////////////////////////
//  CLASS CVehicleGlassComponentEntity
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  InitPool
///////////////////////////////////////////////////////////////////////////////

void CVehicleGlassComponentEntity::InitPool(const MemoryBucketIds membucketId, int redZone) 
{
	sysMemUseMemoryBucket membucket( membucketId );
	vfxAssertf(!_ms_pPool, "CVehicleGlassComponentEntity::InitPool - trying to initialise a pool that already exists"); 
	const char* poolName = "VehicleGlassComponentEntity";
	_ms_pPool = rage_new Pool(fwConfigManager::GetInstance().GetSizeOfPool(atHashValue(poolName), VEHICLE_GLASS_MAX_COMPONENTS), poolName, redZone);
	// _ms_pPool->SetCanDealWithNoMemory(true); // We don't want no nasty ran out of memory asserts.
}

void CVehicleGlassComponentEntity::InitPool(int size, const MemoryBucketIds membucketId, int redZone) 
{
	sysMemUseMemoryBucket membucket( membucketId ); 
	vfxAssertf(!_ms_pPool, "CVehicleGlassComponentEntity::InitPool - trying to initialise a pool that already exists"); 
	const char* poolName = "VehicleGlassComponentEntity";
	_ms_pPool = rage_new Pool(fwConfigManager::GetInstance().GetSizeOfPool(atHashValue(poolName), size), poolName, redZone);
	// _ms_pPool->SetCanDealWithNoMemory(true);
}


///////////////////////////////////////////////////////////////////////////////
//  Constructor
///////////////////////////////////////////////////////////////////////////////

CVehicleGlassComponentEntity::CVehicleGlassComponentEntity(const eEntityOwnedBy ownedBy, CVehicleGlassComponent *pVehGlassComponent) // We don't want no nasty ran out of memory asserts.
	: CEntity( ownedBy )
{ 
	m_refCount = 0;
	SetTypeVehicleGlassComponent(); 
	SetVehicleGlassComponent(pVehGlassComponent);

	SetBaseFlag(fwEntity::HAS_ALPHA);
}


///////////////////////////////////////////////////////////////////////////////
//  PoolFullCallback
///////////////////////////////////////////////////////////////////////////////

#if __DEV
namespace rage { 
	template<> void fwPool<CVehicleGlassComponentEntity>::PoolFullCallback() 
	{
		/* NoOp */
	}
}
#endif


///////////////////////////////////////////////////////////////////////////////
//  SetVehicleGlassComponent
///////////////////////////////////////////////////////////////////////////////

void CVehicleGlassComponentEntity::SetVehicleGlassComponent(CVehicleGlassComponent *pVehGlassComponent)
{
	m_pVehGlassComponent = pVehGlassComponent;
	vfxAssertf(m_pVehGlassComponent, "CVehicleGlassComponentEntity::SetVehicleGlassComponent - called with an invalid Vehicle Glass Component");
}


///////////////////////////////////////////////////////////////////////////////
//  GetBoundBox
///////////////////////////////////////////////////////////////////////////////

FASTRETURNCHECK(const spdAABB &) CVehicleGlassComponentEntity::GetBoundBox(spdAABB& box) const
{
	box = m_pVehGlassComponent->GetBoundBox();
	return box;
}

///////////////////////////////////////////////////////////////////////////////
//  AllocateDrawHandler
///////////////////////////////////////////////////////////////////////////////

fwDrawData* CVehicleGlassComponentEntity::AllocateDrawHandler(rmcDrawable* pDrawable)
{
	return rage_new CVehicleGlassDrawHandler(this, pDrawable);
}

