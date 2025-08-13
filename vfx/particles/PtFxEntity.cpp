///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	PtFxEntity.cpp
//	BY	: 	Alex Hadjadj (and Mark Nicholson)
//	FOR	:	Rockstar North
//	ON	:	
//	WHAT:	Scene sorted particle systems
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

#include "PtFxEntity.h"

// rage
#include "system/param.h"
#include "rmptfx/ptxmanager.h"

// framework
#include "fwrenderer/renderlistgroup.h"
#include "vfx/channel.h"
#include "vfx/ptfx/ptfxattach.h"
#include "vfx/ptfx/ptfxconfig.h"
#include "vfx/ptfx/ptfxflags.h"
#include "Vfx/Particles/PtFxManager.h"

// framework
#include "fwdebug/picker.h"

// game
#include "audio/ambience/ambientaudioentity.h"
#include "Camera/CamInterface.h"
#include "Renderer/Renderer.h"
#include "Renderer/RenderListGroup.h"
#include "renderer/DrawLists/drawListMgr.h"
#include "renderer/Entities/PtFxDrawHandler.h"
#include "renderer/RenderPhases/RenderPhase.h"
#include "renderer/RenderPhases/RenderPhaseMirrorReflection.h"


#if PTFX_ALLOW_INSTANCE_SORTING


///////////////////////////////////////////////////////////////////////////////
//  OPTIMISATIONS - TURN ON IN OPTIMISATIONS.H
///////////////////////////////////////////////////////////////////////////////

VFX_PTFX_OPTIMISATIONS()


///////////////////////////////////////////////////////////////////////////////
//  GLOBAL VARIABLES
///////////////////////////////////////////////////////////////////////////////

CPtFxSortedEntity::Pool* CPtFxSortedEntity::_ms_pPool = NULL;
atArray<CPtFxSortedEntity*>	CPtFxSortedManager::m_activeEntities;
sysCriticalSectionToken CPtFxSortedManager::m_SortedEntityListCS;


///////////////////////////////////////////////////////////////////////////////
//  CODE
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  CLASS CPtFxSortedEntity
///////////////////////////////////////////////////////////////////////////////
#if RAGE_INSTANCED_TECH
	#if ENABLE_MATRIX_MEMBER
	CompileTimeAssertSize(CPtFxSortedEntity, 88, 224);
	#else
	CompileTimeAssertSize(CPtFxSortedEntity,88,152);
	#endif
#else
	#if ENABLE_MATRIX_MEMBER
	CompileTimeAssertSize(CPtFxSortedEntity, 88, 216);
	#else
	CompileTimeAssertSize(CPtFxSortedEntity,88,144);
	#endif
#endif



///////////////////////////////////////////////////////////////////////////////
//  InitPool
///////////////////////////////////////////////////////////////////////////////

void CPtFxSortedEntity::InitPool(const MemoryBucketIds membucketId, int redZone) 
{
	sysMemUseMemoryBucket membucket( membucketId );
	ptfxAssertf(!_ms_pPool, "CPtFxSortedEntity::InitPool - trying to initialise a pool that already exists"); 
	const char* poolName = "PtFxSortedEntity";
	_ms_pPool = rage_new Pool(fwConfigManager::GetInstance().GetSizeOfPool(atHashValue(poolName), CONFIGURED_FROM_FILE), poolName, redZone);
	// _ms_pPool->SetCanDealWithNoMemory(true); // We don't want no nasty ran out of memory asserts.
}

void CPtFxSortedEntity::InitPool(int size, const MemoryBucketIds membucketId, int redZone) 
{
	sysMemUseMemoryBucket membucket( membucketId ); 
	ptfxAssertf(!_ms_pPool, "CPtFxSortedEntity::InitPool - trying to initialise a pool that already exists"); 
	const char* poolName = "PtFxSortedEntity";
	_ms_pPool = rage_new Pool(fwConfigManager::GetInstance().GetSizeOfPool(atHashValue(poolName), size), poolName, redZone);
	// _ms_pPool->SetCanDealWithNoMemory(true);
}


///////////////////////////////////////////////////////////////////////////////
//  Constructor
///////////////////////////////////////////////////////////////////////////////

CPtFxSortedEntity::CPtFxSortedEntity(const eEntityOwnedBy ownedBy, ptxEffectInst *pFxInst) // We don't want no nasty ran out of memory asserts.
	: CEntity( ownedBy )
{ 
	SetTypePrtSys(); 
	SetFxInst(pFxInst);

	SetBaseFlag(fwEntity::HAS_ALPHA);
}


///////////////////////////////////////////////////////////////////////////////
//  PoolFullCallback
///////////////////////////////////////////////////////////////////////////////

#if __DEV
namespace rage { 
	template<> void fwPool<CPtFxSortedEntity>::PoolFullCallback() 
	{
		/* NoOp */
	}
}
#endif


///////////////////////////////////////////////////////////////////////////////
//  SetFxInst
///////////////////////////////////////////////////////////////////////////////

void CPtFxSortedEntity::SetFxInst(ptxEffectInst *pFxInst)
{
	m_pFxInst = pFxInst;
	ptfxAssertf(m_pFxInst, "CPtFxSortedEntity::SetFxInst - called with an invalid effect instance");
	ptfxAssertf(m_pFxInst->GetFlag(PTXEFFECTFLAG_MANUAL_DRAW), "CPtFxSortedEntity::SetFxInst - the effect inst doesn't have the manual draw flag set");
	ptfxAssertf(m_pFxInst->GetFlag(PTXEFFECTFLAG_KEEP_RESERVED), "CPtFxSortedEntity::SetFxInst - the effect inst doesn't have the reserved flag set");
	ptfxAssertf(m_pFxInst->GetFlag(PTFX_RESERVED_SORTED), "CPtFxSortedEntity::SetFxInst - the effect inst doesn't have the reserved for sorting flag set");
}


///////////////////////////////////////////////////////////////////////////////
//  GetBoundBox
///////////////////////////////////////////////////////////////////////////////

FASTRETURNCHECK(const spdAABB &) CPtFxSortedEntity::GetBoundBox(spdAABB& box) const
{
	// we have to abs the W of the sphere, because depending on the interpolation at the time 
	// of calculation, our bounding sphere might be inverted. bitch.
	box.SetAsSphereV4(Andc(m_pFxInst->GetBoundingSphere(), Vec4VConstant<0,0,0,U32_NEGZERO>()));
	return box;
}

///////////////////////////////////////////////////////////////////////////////
//  AllocateDrawHandler
///////////////////////////////////////////////////////////////////////////////

fwDrawData* CPtFxSortedEntity::AllocateDrawHandler(rmcDrawable* pDrawable)
{
	return rage_new CPtFxDrawHandler(this, pDrawable);
}


///////////////////////////////////////////////////////////////////////////////
//  CLASS CPtFxSortedManager
///////////////////////////////////////////////////////////////////////////////

CPtFxSortedManager::CPtFxSortedManager()
{
}


CPtFxSortedManager::~CPtFxSortedManager()
{
	// Hopefully, ShutdownLevel() would have been called before. We assert
	// here to give us a chance to catch leaks.
}

///////////////////////////////////////////////////////////////////////////////
//  Init
///////////////////////////////////////////////////////////////////////////////

void CPtFxSortedManager::Init()
{
	const int maxSortedInstances = CPtFxSortedEntity::GetPool()->GetSize();

	// init the scene sorted effects
	// - Note: back when PTFX_MAX_SORTED_INSTANCES existed (couldn't be changed
	//   through the configuration file), we did this, always reserving 32
	//   regardless of the value of PTFX_MAX_SORTED_INSTANCES (which was 32):
	//		m_activeEntities.Reserve(32);
	//   However, I suspect that was a bug, and that the correct thing to do
	//   is to make m_activeEntities large enough to contain all
	//   CPtFxSortedEntity objects we have in the pool. /FF
	m_activeEntities.Reserve(maxSortedInstances);

}

///////////////////////////////////////////////////////////////////////////////
//  InitLevel
///////////////////////////////////////////////////////////////////////////////

void CPtFxSortedManager::InitLevel()
{
	// Nothing to do here, but it's more clear to have this function for
	// symmetry with ShutdownLevel(). /FF
}

///////////////////////////////////////////////////////////////////////////////
//  Shutdown
///////////////////////////////////////////////////////////////////////////////

void CPtFxSortedManager::Shutdown()
{
}

///////////////////////////////////////////////////////////////////////////////
//  ShutdownLevel
///////////////////////////////////////////////////////////////////////////////

void CPtFxSortedManager::ShutdownLevel()
{
	while (m_activeEntities.GetCount()>0)
	{
		delete m_activeEntities[0];
		m_activeEntities.DeleteFast(0);
	}
}


///////////////////////////////////////////////////////////////////////////////
//  Add
///////////////////////////////////////////////////////////////////////////////

void CPtFxSortedManager::Add(ptxEffectInst* pFxInst)
{
	SYS_CS_SYNC(m_SortedEntityListCS);
	if (CPtFxSortedEntity::_ms_pPool->GetNoOfFreeSpaces()>0) 
	{
		ptfxFlags::SetUserReservedFlag(pFxInst, PTFX_RESERVED_SORTED, true);
		CPtFxSortedEntity* pPtFxSortedEntity = rage_new CPtFxSortedEntity(ENTITY_OWNEDBY_VFX, pFxInst);
		ptfxAssertf(pPtFxSortedEntity, "CPtFxSortedManager::CleanUp - invalid entry found in the way in list");
		pPtFxSortedEntity->SetDrawHandler(pPtFxSortedEntity->AllocateDrawHandler(NULL));
		m_activeEntities.Append() = pPtFxSortedEntity;
	}
}


///////////////////////////////////////////////////////////////////////////////
//  Remove
///////////////////////////////////////////////////////////////////////////////

void CPtFxSortedManager::Remove(ptxEffectInst* pFxInst)
{
	SYS_CS_SYNC(m_SortedEntityListCS);
	// mark as no longer sorted
	ptfxFlags::ClearUserReservedFlag(pFxInst, PTFX_RESERVED_SORTED, true);
}


///////////////////////////////////////////////////////////////////////////////
//  AddToRenderList
///////////////////////////////////////////////////////////////////////////////

void CPtFxSortedManager::AddToRenderList(u32 renderFlags, s32 entityListIndex)
{
	SYS_CS_SYNC(m_SortedEntityListCS);

	extern bool g_render_lock;
	if (g_render_lock)
	{
		return;
	}

#if __BANK
	if (g_ptFxManager.GetGameDebugInterface().GetDisableSortedRender())
	{
		return;
	}
#endif

	// this is OK - because we are always calling this from the context of a render phase
	if (!(renderFlags & RENDER_SETTING_RENDER_PARTICLES))
	{
		return;
	}

	Vec3V vCamPos = RCC_VEC3V(camInterface::GetPos());
	Vec3V vCamForward = RCC_VEC3V(camInterface::GetFront());
	if(entityListIndex == CRenderPhaseMirrorReflectionInterface::GetRenderPhase()->GetEntityListIndex() && CRenderPhaseMirrorReflectionInterface::GetViewport())
	{
		vCamPos = CRenderPhaseMirrorReflectionInterface::GetViewport()->GetCameraPosition();
		vCamForward = CRenderPhaseMirrorReflectionInterface::GetViewport()->GetCameraMtx().GetCol2();
	}
	ptfxAssertf(IsFiniteAll(vCamPos), "invalid camPos %.3f, %.3f, %.3f", VEC3V_ARGS(vCamPos));

	for (int i=0; i<m_activeEntities.GetCount(); i++)
	{
		ptxEffectInst* pFxInst = m_activeEntities[i]->GetFxInst();
		if ( !pFxInst->GetIsFinished() && pFxInst->GetNumActivePoints() )
		{
			ptfxAssertf(pFxInst->GetFlag(PTXEFFECTFLAG_MANUAL_DRAW), "CPtFxSortedManager::AddToRenderList - active effect instance doesn't have its manual draw flag set");
			ptfxAssertf(pFxInst->GetFlag(PTFX_RESERVED_SORTED), "CPtFxSortedManager::AddToRenderList - active effect instance doesn't have its reserved for sorting flag set");

			Vec3V vFxInstPos = pFxInst->GetMatrix().GetCol3();
			ptfxAssertf(IsFiniteAll(vFxInstPos), "invalid fxInstPos %.3f, %.3f, %.3f", VEC3V_ARGS(vFxInstPos));


			if(entityListIndex == CRenderPhaseDrawSceneInterface::GetRenderPhase()->GetEntityListIndex())
			{
				//Adjustments to be made only for the main viewport 

				// this code forces any scene sorted effects on vehicles to draw after the vehicle itself
				// alpha polys on the vehicle can end up cutting out the particles if this isn't done

#if __BANK
				if (g_ptFxManager.GetGameDebugInterface().GetDisableSortedVehicleOffsets()==false)
#endif
				{
					void* pAttachEntity = ptfxAttach::GetAttachEntity(pFxInst);
					if (pAttachEntity!=(void*)(0xffffffff) && pAttachEntity!=(void*)(0x00000000))
					{
						CEntity* pEntity = (CEntity*)pAttachEntity;
						if (pEntity->GetIsTypeVehicle())
						{
							CVehicle* pVehicle = static_cast<CVehicle*>(pEntity);
							const fwTransform& transform = pEntity->GetTransform();

							const Vec3V vVehPos = transform.GetPosition();

							ptfxAssertf(IsFiniteAll(vVehPos), "invalid vehPos %.3f, %.3f, %.3f", VEC3V_ARGS(vVehPos));


							// To sort the particles, 
							// we work out which "side" of the vehicle's extent the effect is the closest to (either A or B)
							// and then get the one on sides facing the camera to move forward, and push the back one further based on the test on that side.
							//															A
							//			-----------------------------------------------------------------------------------------------------
							//			|												^													|
							//			|												|													|
							//			|												| normalA (tangent)									|
							//			|												|													|
							//			|			normalB (direction)					|													|
							//			|<-----------------------------------------------													|
							//		B	|																									| B
							//			|																									|
							//			|																									|
							//			|																									|
							//			|																									|
							//			-----------------------------------------------------------------------------------------------------
							//															A
							//
							//					 <------------------------------------	Vehicle is facing


							spdAABB box;
							box = pVehicle->GetLocalSpaceBoundBox(box);
							const Vec3V extent = box.GetExtent();
							const ScalarV extentALength = extent.GetX();
							const ScalarV extentBLength = extent.GetY();

							Vec3V vehToFxInstDir = vFxInstPos - vVehPos;

							const ScalarV offset = BANK_SWITCH(g_ptFxManager.GetGameDebugInterface().GetSortedEffectsVehicleCamDirOffset(), PTFX_SORTED_VEHICLE_CAM_DIR_OFFSET);

							const bool isVehicleWheelAttachedTrailingFx = pFxInst->GetFlag(PTFX_EFFECTINST_VEHICLE_WHEEL_ATTACHED_TRAILING);
							//Let the min distance check be based on velocity. There is a frame delay in getting the results so faster the car is, the effect goes out of the distance check
							const ScalarV minDistCheck = BANK_SWITCH(g_ptFxManager.GetGameDebugInterface().GetSortedEffectsVehicleMinDistFromCenter(), PTFX_SORTED_VEHICLE_MIN_DIST_FROM_CENTER);
							ScalarV dirOffset = offset;
							if(IsLessThanAll(DistSquared(vFxInstPos, vVehPos), minDistCheck*minDistCheck) != 0)
							{
								//If it's close to the center of the vehicle, let's just push it out further from the center in direction of the camera
								dirOffset = offset;
							}
							else
							{
								Vec3V camToVehDir = vVehPos - vCamPos;
								const Vec3V normalA = transform.GetA();
								const Vec3V normalB = transform.GetB();
								const Vec3V extentA = extentALength * normalA;
								const Vec3V extentB = extentBLength * normalB;

								const ScalarV dotA = Dot(vehToFxInstDir,extentA);
								const ScalarV dotB = Dot(vehToFxInstDir,extentB);

								//Figure out which side of the car is the camera on
								//If camera is in Left or right the vehicle, use Left/Right planes for the orient test
								//We un-transform and un scale the vector to get the right side of the vehicle no matter the scale/orientation of the vehicle/camera
								const Vec3V camToVehDirUnTransformed = transform.UnTransform3x3(camToVehDir) / extent;
								const BoolV useA = IsGreaterThan(Abs(Dot(camToVehDirUnTransformed, Vec3V(V_X_AXIS_WONE))), Abs(Dot(camToVehDirUnTransformed, Vec3V(V_Y_AXIS_WONE))));

								// Orient vector : based on scaled dot product between extent and (veh->vfx) vector.
								Vec3V orient = SelectFT(useA, dotB * normalB, dotA * normalA);
								if(isVehicleWheelAttachedTrailingFx && IsFalse(useA))
								{
									//this one needs special treatment. If the camera is in the front or rear of the vehicle, we should always push the effect to the rear of the vehicle
									orient = -normalB;
								}

								// Calculate forward vector based on extent.camForward.
								const ScalarV dotCam = Dot(orient,vCamForward);
								const BoolV isPositive = IsGreaterThanOrEqual(dotCam,ScalarV(V_ZERO));

								//Let's push the offset a little more based on how far the effect is from the center of the vehicles bounding box. 
								//"orient" already has the direction + distance from center, we just re-use that along with constant multiplier
								dirOffset = SelectFT(isPositive,-offset,offset);

							}

							const Vec3V finalOffset = vCamForward * dirOffset;

							//we should offset the position from the existing position itself and not the vehicle position, so particles dont interfere with each other
							//but for vehicle wheel attached vfx, we need to do something special as it trails all the way back from the front wheels. so starting from vehice position
							//in that case							
							if(isVehicleWheelAttachedTrailingFx)
							{
								vFxInstPos = vVehPos;
							}
							vFxInstPos += finalOffset;
							vFxInstPos += NormalizeSafe(vehToFxInstDir, Vec3VFromF32(SQRT3INVERSE)) * BANK_SWITCH(g_ptFxManager.GetGameDebugInterface().GetSortedEffectsVehicleCamPosOffset(), PTFX_SORTED_VEHICLE_CAM_POS_OFFSET);			


#if __BANK
							if(g_ptFxManager.GetGameDebugInterface().GetRenderSortedVehicleEffectsBoundingBox())
							{
								//Debug only the selected entity
								CEntity *pSelectedEntity = static_cast< CEntity * >(g_PickerManager.GetSelectedEntity());
								if(pEntity && pEntity->GetIsTypeVehicle())
								{
									CVehicle* pSelectedVehicle = static_cast<CVehicle*>(pSelectedEntity);
									if(pSelectedVehicle == pVehicle )
									{
										spdAABB bbox;
										bbox = m_activeEntities[i]->GetBoundBox(bbox);
										grcDebugDraw::Arrow(pFxInst->GetMatrix().GetCol3(), vFxInstPos, 0.2f, Color_red);
										bbox.SetAsSphereV4(Vec4V(vFxInstPos, Mag(bbox.GetExtent())));
										grcDebugDraw::BoxAxisAligned(bbox.GetMin(), bbox.GetMax(), Color_green, false);
										spdAABB bboxCar;
										bboxCar = pVehicle->GetLocalSpaceBoundBox(bboxCar);
										grcDebugDraw::BoxOriented(bboxCar.GetMin(), bboxCar.GetMax(), transform.GetMatrix(), Color_blue, false);

										if(g_ptFxManager.GetGameDebugInterface().GetRenderSortedVehicleEffectsCamDebug())
										{
											//Debug to see which side of the vehicle is selected based on the camera
											Mat34V matrix = transform.GetMatrix();
											Vec3V camToVehDir = vVehPos - vCamPos;

											//Figure out which side of the car is the camera on
											//If camera is in front or behind the vehicle, use Left/Right planes for the orient test
											//We un-transform and un scale the vector to get the right side of the vehicle no matter the scale/orientation of the vehicle/camera
											const Vec3V camToVehDirUnTransformed = transform.UnTransform3x3(camToVehDir) / extent;
											const BoolV useA = IsGreaterThan(Abs(Dot(camToVehDirUnTransformed, Vec3V(V_X_AXIS_WONE))), Abs(Dot(camToVehDirUnTransformed, Vec3V(V_Y_AXIS_WONE))));

											Vec3V points[8] = 
											{
												Vec3V(-extent.GetX(),-extent.GetY(),-extent.GetZ()), //LEFT, FRONT, BOTTOM
												Vec3V(+extent.GetX(),-extent.GetY(),-extent.GetZ()), //RIGHT, FRONT, BOTTOM
												Vec3V(+extent.GetX(),+extent.GetY(),-extent.GetZ()), //RIGHT, BACK, BOTTOM
												Vec3V(-extent.GetX(),+extent.GetY(),-extent.GetZ()), //LEFT, BACK, BOTTOM
												Vec3V(-extent.GetX(),-extent.GetY(),+extent.GetZ()), //LEFT, FRONT, TOP
												Vec3V(+extent.GetX(),-extent.GetY(),+extent.GetZ()), //RIGHT, FRONT, TOP
												Vec3V(+extent.GetX(),+extent.GetY(),+extent.GetZ()), //RIGHT, BACK, TOP
												Vec3V(-extent.GetX(),+extent.GetY(),+extent.GetZ()), //LEFT, BACK, TOP
											};
											for(int i=0; i<8; i++)
											{
												points[i] = Transform(matrix, points[i]);
											}

											if(IsEqualIntAll(useA, BoolV(V_TRUE)) != 0)
											{									
												grcDebugDraw::Quad(points[0], points[3], points[7], points[4], Color32(0.0f, 0.0f, 1.0f, 0.5f), true, true );
												grcDebugDraw::Quad(points[1], points[2], points[6], points[5], Color32(0.0f, 0.0f, 1.0f, 0.5f), true, true );
											}
											else
											{
												grcDebugDraw::Quad(points[0], points[1], points[5], points[4], Color32(0.0f, 0.0f, 1.0f, 0.5f), true, true );
												grcDebugDraw::Quad(points[2], points[3], points[7], points[6], Color32(0.0f, 0.0f, 1.0f, 0.5f), true, true );
											}
										}
									}

								}

							}
#endif
						}
					}
				}

			}
			
			const Vec3V vDiff = vCamPos - vFxInstPos;
			const float dist = rage::Max(Mag(vDiff).Getf() + pFxInst->GetCameraBias(), 0.0f);

#if __BANK
			if(g_ptFxManager.GetGameDebugInterface().GetRenderSortedEffectsBoundingBox())
			{
				spdAABB bbox;
				bbox = m_activeEntities[i]->GetBoundBox(bbox);
				grcDebugDraw::BoxAxisAligned(bbox.GetMin(), bbox.GetMax(), Color_red, false);
			}
#endif
			//Using deferred Add Entity as this is being called in Safe Execution
			CGtaRenderListGroup::DeferredAddEntity(entityListIndex, m_activeEntities[i], 0, dist, RPASS_ALPHA, 0);			
		}
	}
};


///////////////////////////////////////////////////////////////////////////////
//  CleanUp
///////////////////////////////////////////////////////////////////////////////

void CPtFxSortedManager::CleanUp()
{	
	SYS_CS_SYNC(m_SortedEntityListCS);
	// clean up any effects that are now finished and inactive - instead of waiting until they are recycled
	for (int j=0; j<m_activeEntities.GetCount(); j++)
	{
		if (m_activeEntities[j]->GetFxInst()->GetIsFinished() || m_activeEntities[j]->GetFxInst()->GetIsDeactivated())
		{
			ptfxFlags::ClearUserReservedFlag(m_activeEntities[j]->GetFxInst(), PTFX_RESERVED_SORTED, true);
			delete m_activeEntities[j];
			m_activeEntities.DeleteFast(j);
			j--; // Due to the way the array delete stuff, we need to ensure we stay on the same line...
		}
	}

#if __DEV
	// go through the list checking that we don't have an effect inst in there twice
	for (int i=0; i<m_activeEntities.GetCount(); i++)
	{
		for (int j=i+1; j<m_activeEntities.GetCount(); j++)
		{
			if (m_activeEntities[i]->GetFxInst() == m_activeEntities[j]->GetFxInst())
			{
				// this effect inst is in the list twice
				ptfxAssertf(0, "CPtFxSortedManager::CleanUp - effect inst found twice in active list");
			}
		}
	}
#endif
}

#endif // PTFX_ALLOW_INSTANCE_SORTING

