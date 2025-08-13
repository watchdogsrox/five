//
// entity/entitybatch_def.h : Common definitions used by batched entities
//
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#ifndef SCENE_ENTITY_BATCH_DEF_H_
#define SCENE_ENTITY_BATCH_DEF_H_

#include "shader_source/util/BatchInstancing.fxh" //For GRASS_BATCH_CS_CULLING defines... and some others
#include "system/noncopyable.h"
#include "grcore/effect_typedefs.h"
#include "grmodel/shadergroupvar.h"
#include "system/pix.h"

#define GRASS_BATCH_CS_PRE_ALLOCATE_BUFFERS (1 && GRASS_BATCH_CS_CULLING && RSG_PC)
#if GRASS_BATCH_CS_PRE_ALLOCATE_BUFFERS
#	define GRASS_BATCH_CS_PRE_ALLOCATE_BUFFERS_ONLY(...)  __VA_ARGS__
#	define GRASS_BATCH_CS_PRE_ALLOCATE_BUFFERS_SWITCH(_if_CS_,_else_)  _if_CS_
#else
#	define GRASS_BATCH_CS_PRE_ALLOCATE_BUFFERS_ONLY(...)
#	define GRASS_BATCH_CS_PRE_ALLOCATE_BUFFERS_SWITCH(_if_CS_,_else_)  _else_
#endif

#define GRASS_BATCH_CS_DYNAMIC_BUFFERS		(GRASS_BATCH_CS_CULLING && !GRASS_BATCH_CS_PRE_ALLOCATE_BUFFERS)
#if GRASS_BATCH_CS_DYNAMIC_BUFFERS
#	define GRASS_BATCH_CS_DYNAMIC_BUFFERS_ONLY(...) __VA_ARGS__
#	define GRASS_BATCH_CS_DYNAMIC_BUFFERS_SWITCH(_if_CS_,_else_)  _if_CS_
#else
#	define GRASS_BATCH_CS_DYNAMIC_BUFFERS_ONLY(...)
#	define GRASS_BATCH_CS_DYNAMIC_BUFFERS_SWITCH(_if_CS_,_else_)  _else_
#endif

#if RSG_PC &&__D3D11
#	include "grcore/buffer_d3d11.h"
#elif RSG_DURANGO
#	include "grcore/buffer_d3d11.h"
#	include "grcore/buffer_durango.h"
#elif RSG_ORBIS
#	include "grcore/buffer_gnm.h"
#endif

#include "grcore/device.h"

namespace rage {
	class fwGrassInstanceListDef;
}

namespace EBStatic {
#if GRASS_BATCH_CS_CULLING
#if GRASS_BATCH_CS_PRE_ALLOCATE_BUFFERS

	typedef atRangeArray<atArray<atArray<u32> >, LOD_COUNT> IndirectArgsDrawableOffsetMap;
#endif //GRASS_BATCH_CS_PRE_ALLOCATE_BUFFERS

#if RSG_ORBIS
	BEGIN_ALIGNED(8) //sce::Gnm::kAlignmentOfIndirectArgsInBytes
	struct IndirectArgParams : public sce::Gnm::DrawIndexIndirectArgs
	{
		static const u32 sIndexCountPerInstanceOffset;
		static const u32 sInstanceCountOffset; // = offsetof(IndirectArgParams, m_instanceCount);
		static const u32 sStartIndexLocationOffset;
		static const u32 sBaseVertexLocationOffset;
		static const u32 sStartInstanceLocationOffset;
		static const u32 sInvalidInstanceCount = static_cast<u32>(-1);
		IndirectArgParams()
		{
			m_indexCountPerInstance = m_startIndexLocation = m_baseVertexLocation = m_startInstanceLocation = 0;
			m_instanceCount = sInvalidInstanceCount; //m_instanceCount = 1;
		}
	} ALIGNED(8); //sce::Gnm::kAlignmentOfIndirectArgsInBytes

	CompileTimeAssert(__alignof(EBStatic::IndirectArgParams) == sce::Gnm::kAlignmentOfIndirectArgsInBytes);
#else //RSG_ORBIS
	struct IndirectArgParams
	{
		static const u32 sIndexCountPerInstanceOffset;
		static const u32 sInstanceCountOffset;
		static const u32 sStartIndexLocationOffset;
		static const u32 sBaseVertexLocationOffset;
		static const u32 sStartInstanceLocationOffset;
		IndirectArgParams() : m_indexCountPerInstance(0), m_instanceCount(0), m_startIndexLocation(0), m_baseVertexLocation(0), m_startInstanceLocation(0) { }

		u32 m_indexCountPerInstance;
		u32 m_instanceCount;
		u32 m_startIndexLocation;
		int m_baseVertexLocation;
		u32 m_startInstanceLocation;
	};
#endif //RSG_ORBIS

#if GRASS_BATCH_CS_PRE_ALLOCATE_BUFFERS
	template<u32 _BufferSizeInBytes>
	class PreAllocatedIndirectArgBufferResource : public grcBufferBasic
	{
	public:
		typedef grcBufferBasic parent_type;
		static const u32 sSizeInBytes = _BufferSizeInBytes;
		PreAllocatedIndirectArgBufferResource() : parent_type(ORBIS_ONLY(grcBuffer_Structured, true)) {}
#if NV_SUPPORT
		bool bNeedsNVFlaging;
#endif // NV_SUPPORT

		void Create( ASSERT_ONLY( bool preCreate ) )
		{
			Assertf(GRCDEVICE.CheckThreadOwnership() || !preCreate, "WARNING: Make sure to lock the device context before pre-allocating.");
	#if RSG_ORBIS
			const sce::Gnm::SizeAlign accumSizeAlign = { sSizeInBytes, sce::Gnm::kAlignmentOfIndirectArgsInBytes };
			Allocate(accumSizeAlign, true));
			Assertf(GRCDEVICE.CheckThreadOwnership() || preCreate, "WARNING: If this happens because of allocation during overflow, then ignore it and just increase the pre-allocated pool size.");
	#elif RSG_DURANGO
			Initialise(sSizeInBytes, 1, grcBindNone, NULL, grcBufferMisc_DrawIndirectArgs);
			Assertf(GRCDEVICE.CheckThreadOwnership() || preCreate, "WARNING: If this happens because of allocation during overflow, then ignore it and just increase the pre-allocated pool size.");
	#else // RSG_PC
			// url:bugstar:2322748 and url:bugstar:2206695: This is not critical on PC due to more flexible memory requirements + likely due to extreme draw distances. In these situations just rely on 
			// dynamic allocation if we overflow + print out a message to say we're overflowing. Ideally this shouldn't happen much but downgrading this from an assert for now on Alan's advice.
			// I've checked the code - and as we only use an ID3D11Device (whose calls are thread-safe) and not a ID3D11DeviceContext - we don't need to do a LockContext() here on PC.
			// See bug for more details.
			Initialise(sSizeInBytes, 1, grcBindNone, grcsBufferCreate_ReadWrite, grcsBufferSync_None, NULL, false, grcBufferMisc_DrawIndirectArgs);
			ASSERT_ONLY( if (!(GRCDEVICE.CheckThreadOwnership() || preCreate)) { Warningf("WARNING: If this happens because of allocation during overflow, then ignore it and just increase the pre-allocated pool size."); } );
	#endif

#if NV_SUPPORT
			static const bool isNV = GRCDEVICE.GetManufacturer() == NVIDIA;
			bNeedsNVFlaging = isNV;
#endif // NV_SUPPORT
		}

		void PreCreate()
		{
			Create( ASSERT_ONLY( true ) );
#if NV_SUPPORT
			// Avoid calling this on non-Nvidia platforms.  The NvAPI calls ought to be harmless on all platforms.
			// If this breaks on laptops with Intel and Nvidia graphics, then ask Adrian. :-)
			static const bool isNV = GRCDEVICE.GetManufacturer() == NVIDIA;
			if (isNV)
			{
				FlagForNoSLITransfer();
			}

			bNeedsNVFlaging = false;
#endif // NV_SUPPORT
		}

#if NV_SUPPORT
		// These buffers are written every frame with no inter-frame dependencies.  However, for SLI, the Nvidia driver is not able to
		// determine that no inter-frame dependencies exist because the buffer is partially updated.  In theory, parts that were updated
		// in the previous frame could be consumed by the draw call.  Hence, flag it explicitly as not requiring transfer between GPUs.
		// This makes a huge difference to SLI performance.
		void FlagForNoSLITransfer()
		{
			GRCDEVICE.LockContext();

			NVDX_ObjectHandle pNvAPIObj = 0;

			if (GRCDEVICE.GetCurrent())
			{
				NvAPI_D3D_GetObjectHandleForResource(GRCDEVICE.GetCurrent(), GetD3DBuffer(), &pNvAPIObj);

				if (pNvAPIObj)
				{
					NvU32 hint = 1;
					NvAPI_D3D_SetResourceHint(GRCDEVICE.GetCurrent(), pNvAPIObj, NVAPI_D3D_SRH_CATEGORY_SLI, NVAPI_D3D_SRH_SLI_APP_CONTROLLED_INTERFRAME_CONTENT_SYNC, &hint);
				}
			}

			GRCDEVICE.UnlockContext();
			bNeedsNVFlaging = false;
		}
#endif //NV_SUPPORT
	};
	typedef PreAllocatedIndirectArgBufferResource<1024 * 4> PreAllocatedIndirectArgBuffer;

	template<u32 _MaxBufferCount>
	class PreAllocatedAppendBufferResource : public grcBufferUAV
	{
	public:
		typedef grcBufferUAV parent_type;
		static const u32 sMaxBufferCount = _MaxBufferCount;
		PreAllocatedAppendBufferResource() : parent_type(ORBIS_ONLY(grcBuffer_Structured, true)) {}
		
		void Create(ASSERT_ONLY(bool /*preCreate*/))
		{
#if RSG_ORBIS
			const sce::Gnm::SizeAlign accumSizeAlign = { GrassBatchCSInstanceData_Size * sMaxBufferCount, GrassBatchCSInstanceData_Align };
			Allocate(accumSizeAlign, true, NULL, GrassBatchCSInstanceData_Size));
#elif RSG_DURANGO
			Initialise(sMaxBufferCount, GrassBatchCSInstanceData_Size, grcBindShaderResource|grcBindUnorderedAccess, NULL, grcBufferMisc_BufferStructured, grcBuffer_UAV_FLAG_APPEND);
#else // RSG_PC
			Initialise(sMaxBufferCount, GrassBatchCSInstanceData_Size, grcBindShaderResource|grcBindUnorderedAccess, grcsBufferCreate_ReadWriteOnceOnly, grcsBufferSync_None, NULL, false, grcBufferMisc_BufferStructured, grcBuffer_UAV_FLAG_APPEND);
#endif
		}

		void PreCreate()
		{
			Create(ASSERT_ONLY(true));
		}
	};

	typedef PreAllocatedAppendBufferResource<1024 * 4> PreAllocatedAppendBuffer;

	struct DrawableDeviceResources
	{
		DrawableDeviceResources(rmcDrawable &drawable);

		PreAllocatedIndirectArgBuffer m_IndirectArgsBuffer;
		IndirectArgsDrawableOffsetMap m_OffsetMap;

		NON_COPYABLE(DrawableDeviceResources);
	};
#endif //GRASS_BATCH_CS_PRE_ALLOCATE_BUFFERS
	struct GrassCSVars
	{
		GrassCSVars();

		bool IsValid() const	{ return m_IsValid; }
		void UpdateValidity();

		int m_ShaderIndex;

		atRangeArray<grcEffectVar, LOD_COUNT> m_idVarAppendInstBuffer;
		DURANGO_ONLY(GRASS_BATCH_CS_DYNAMIC_BUFFERS_ONLY(grcEffectVar m_idIndirectArgs));
		grcEffectVar m_idInstCullParams;
		grcEffectVar m_idNumClipPlanes;
		grcEffectVar m_idClipPlanes;

		grcEffectVar m_idCameraPosition;
		grcEffectVar m_idLodInstantTransition;
#if RSG_DURANGO || RSG_PC
		grcEffectVar m_idGrassSkipInstance;
#endif // RSG_DURANGO || RSG_PC...
		grcEffectVar m_idLodThresholds;
		grcEffectVar m_idCrossFadeDistance;

		grcEffectVar m_idIsShadowPass;
		grcEffectVar m_idLodFadeControlRange;
		DURANGO_ONLY(grcEffectVar m_idIndirectCountPerLod);

		grmShaderGroupVar m_idVarInstanceBuffer;
		grmShaderGroupVar m_idVarRawInstBuffer;
		grmShaderGroupVar m_idVarUseCSOutputBuffer;

		grmShaderGroupVar m_idAabbMin;
		grmShaderGroupVar m_idAabbDelta;
		grmShaderGroupVar m_idScaleRange;

	private:
		bool m_IsValid;
	};

	//These params are set on the update thread and stored in the draw handler for use on the render thread.
	struct GrassCSBaseParams
	{
	public:
		GrassCSBaseParams();

		bool IsValid() const	{ return m_IsValid; }
		void UpdateValidity();

		const fwGrassInstanceListDef *m_InstanceList;	//Smaller than saving the Vec3Vs for params (AABB min/delta & scale range) and since we use resources from list anyway, it's safe to keep a pointer.
		const GrassCSVars *m_Vars;
		u32 m_InstanceCount;
		float m_InstAabbRadius;

		GRASS_BATCH_CS_DYNAMIC_BUFFERS_ONLY(u32 m_IndirectArgCount);
		GRASS_BATCH_CS_DYNAMIC_BUFFERS_ONLY(atRangeArray<u32, LOD_COUNT> m_IndirectArgLodOffsets);
		const grcBufferUAV *m_RawInstBuffer;
		GRASS_BATCH_CS_PRE_ALLOCATE_BUFFERS_ONLY(PreAllocatedIndirectArgBuffer *m_IndirectArgsBuffer);
		GRASS_BATCH_CS_PRE_ALLOCATE_BUFFERS_ONLY(IndirectArgsDrawableOffsetMap *m_OffsetMap);

		PIX_TAGGING_ONLY(int m_ModelIndex);

	private:
		bool m_IsValid;
	};

#if GRASS_BATCH_CS_DYNAMIC_BUFFERS
	struct GrassCSDeviceResources
	{
		GrassCSDeviceResources()	{}

#	if RSG_ORBIS
		grcBufferConstructable<grcBufferUAV, grcBuffer_Structured, true> m_AppendInstBuffer;
#	else
		grcBufferUAV m_AppendInstBuffer;
#	endif

		NON_COPYABLE(GrassCSDeviceResources);
	};
#endif //GRASS_BATCH_CS_DYNAMIC_BUFFERS


	struct GrassCSParams
	{
		GrassCSParams();
		GrassCSParams(const GrassCSBaseParams &base);

		inline bool IsActive() const
		{
			bool active = m_Base.IsValid() && m_Active;
			GRASS_BATCH_CS_PRE_ALLOCATE_BUFFERS_ONLY(active = active && m_AppendInstBuffer[0] != NULL);
			GRASS_BATCH_CS_DYNAMIC_BUFFERS_ONLY(active = active && m_AppendBufferMem[0] != NULL);
			WIN32PC_ONLY(active = active && GRCDEVICE.SupportsFeature(COMPUTE_SHADER_50));
			return active;
		}

		const GrassCSBaseParams &m_Base; //Make this a reference to cut down on drawlist memory usage & mem copying

		//All non-base value should be setup when used in the frame... Base params can be set once in batch entity and used as a template for DL copy.
		const grcViewport *m_CurrVp;

		GRASS_BATCH_CS_PRE_ALLOCATE_BUFFERS_ONLY(atRangeArray<grcBufferUAV *, LOD_COUNT> m_AppendInstBuffer); //Not in device resources b/c this isn't the owner of the resource. Resource is preallocated and this is just a ptr.
		GRASS_BATCH_CS_PRE_ALLOCATE_BUFFERS_ONLY(PreAllocatedIndirectArgBuffer *m_LocalIndirectArgBuffer);

#if GRASS_BATCH_CS_DYNAMIC_BUFFERS
		ORBIS_ONLY(atRangeArray<grcBufferConstructable<grcBufferUAV, grcBuffer_Structured, false>, LOD_COUNT> m_AppendDeviceBuffer);
		
		atRangeArray<void *, LOD_COUNT> m_AppendBufferMem;
		grcCrossContextAllocTyped<IndirectArgParams> m_IndirectBufferMem;
#endif //GRASS_BATCH_CS_DYNAMIC_BUFFERS

		u32 m_phaseLODs : 15;
		u32 m_lastLODIdx : 2;
		u32 m_LODIdx : 2;
		bool m_UseAltfadeDist : 1;

		bool m_Active;

		static const GrassCSBaseParams sm_InvalidBaseForDefaultConstructor;
	};
#endif //GRASS_BATCH_CS_CULLING
}

#endif //SCENE_ENTITY_BATCH_DEF_H_
