
#include "ClothPacket.h"

#if GTA_REPLAY

#include "Peds/ped.h"
#include "fragment/typecloth.h"
#include "control/replay/replay.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ReplayClothPacketManager.																								//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

tPacketVersion g_CPacketClothPieces_Version1 = 1;

// Sentinel value in cloth user data 2 to indicate we'll manipulate cloth vertices rather than pole in recorded vertices.
#define REPLAY_CLOTH_IS_REPLAY_CONTROLLED_BUT_NOT_RECORDED	0xdeadbeef

int ReplayClothPacketManager::m_CurrentTime = 0;
int ReplayClothPacketManager::m_CurrentTimeUpdatedFlag = 0;
int ReplayClothPacketManager::m_ClothUpdateLastTime = 0;
int ReplayClothPacketManager::m_ClothUpdateCurrentTime = 0;
bool ReplayClothPacketManager::ms_IsFineScrubbing = false;

ReplayClothPacketManager::ReplayClothPacketManager()
{
}


//----------------------------------------------------------------------------------//
ReplayClothPacketManager::~ReplayClothPacketManager()
{

}

/***************************************************************************************************************************/
void ReplayClothPacketManager::Init(s32 maxPerFrameClothVertices, s32 maxPerFrameClothPieces)
{
	// We allocate *2 so upon playback we can accomodate 2 frames.
	m_VertexCapacity = maxPerFrameClothVertices;
	m_pClothVertices = rage_new Vec3V[maxPerFrameClothVertices*2];
	m_ClothPieceCapacity = maxPerFrameClothPieces;
	m_pClothPieces = rage_new CLOTH_PIECE[maxPerFrameClothPieces*2];

	// Pass the provide and collect function to the cloth update code.
	clothManager::SetCaptureVerticesFunc(CollectClothVertices, (void *)this);
	clothManager::SetInjectVerticesFunc(ProvideClothVertices, (void *)this);

	m_NextFreeVertex = 0;
	m_NextFreeClothPiece = 0;
	m_VertsReservedForPlayer = 0;
	m_CurrentTime = 0;
	m_CurrentTimeUpdatedFlag = 0;
	m_ClothUpdateLastTime = 0;
	m_ClothUpdateCurrentTime = 0;
	m_PacketVersion = 0;
}


//----------------------------------------------------------------------------------//
void ReplayClothPacketManager::CleanUp()
{
	if(m_pClothPieces)
	{
		delete [] m_pClothPieces;
		m_pClothPieces = NULL;
		m_ClothPieceCapacity = 0;
	}

	if(m_pClothVertices)
	{
		delete [] m_pClothVertices;
		m_pClothVertices = NULL;
		m_VertexCapacity = 0;
	}
}


/****************************************************************************************************************************/
// Recording side functions.																								//
/****************************************************************************************************************************/
void ReplayClothPacketManager::RecordCloth(CPhysical *pEntity, rage::clothController *pClothController, u32 idx)
{
	if(CReplayMgr::IsEditModeActive())
		return;

	if(pEntity->GetIsTypePed())
	{
		// We only record player cloth.
		if(static_cast < CPed * > (pEntity)->IsPlayer())
		{
			SetCallbackInfoInClothControllerEtc(pEntity, pClothController, NULL, idx, (cbFlags)(SetReplayIdAndIndex | ComputeClothVertHash), clothController::CLOTH_CaptureVertsForReplay, true);
		}
	}
	else if(pEntity->GetIsTypeVehicle())
	{
		CVehicle *pVehicle = reinterpret_cast < CVehicle * > (pEntity);

		// Only record cars for now, boats take up too many verts with the sails.
		if(pVehicle->GetVehicleType() == VEHICLE_TYPE_CAR)
		{
			SetCallbackInfoInClothControllerEtc(pEntity, pClothController, NULL, idx, (cbFlags)(SetReplayIdAndIndex | ComputeClothVertHash), clothController::CLOTH_CaptureVertsForReplay, true);
		}
	}
}


//----------------------------------------------------------------------------------//
void ReplayClothPacketManager::RecordPackets(CReplayRecorder &recorder)
{
	CPacketClothPieces::CLOTH_PIECES_TO_RECORD toRecord;
	toRecord.pClothPieces = m_pClothPieces;
	toRecord.numClothPieces = m_NextFreeClothPiece;
	toRecord.pVertices = m_pClothVertices;
	toRecord.numVertices = m_NextFreeVertex;
	recorder.RecordPacketWithParam<CPacketClothPieces>(toRecord);

	// Reset for next time.
	m_NextFreeVertex = 0;
	m_NextFreeClothPiece = 0;

}


//----------------------------------------------------------------------------------//
void ReplayClothPacketManager::CollectClothVertices(clothInstance *pClothInst, int numVerticesTotal, Vec3V *pVertices)
{
	clothInstance *pClothInstance = (clothInstance *)pClothInst;

	if(pClothInstance->GetUserData1())
	{
		sysCriticalSection lock(m_Lock);

		u32 destVertexIndex = 0;
		CReplayID replayID((s32)pClothInstance->GetUserData1());
		s32 destClothPieceIndex = FindClothPiece(replayID, pClothInstance->GetUserData2(), pClothInstance->GetUserData3(), pClothInstance->GetClothController()->GetLOD(), true);

		int startVertex = GetVertexOffsetToRecordFrom(pClothInstance);
		int numVertices = numVerticesTotal - startVertex;

		// Add a new cloth piece if we haven't already got one for the entity.
		if(destClothPieceIndex == -1)
		{
			// Always leave enough room for player cloth to be recorded.
			u32 vertexCapacity = (pClothInst->GetType() == clothManager::Character) ? m_VertexCapacity : (m_VertexCapacity - m_VertsReservedForPlayer);

			if(m_NextFreeVertex + numVertices >= vertexCapacity)
			{
				replayDebugf1("ReplayClothPacketManager::CollectClothVertices()....Out of cloth vertices. (m_NextFreeVertex %d, numVertices %d, capacity %u)", m_NextFreeVertex, numVertices, m_VertexCapacity);
				return;
			}

			if(m_NextFreeClothPiece >= m_ClothPieceCapacity)
			{
				replayDebugf1("ReplayClothPacketManager::CollectClothVertices()....Out of cloth piece records. (m_NextFreeClothPiece %d, ClothPieceCapacity %d)", m_NextFreeClothPiece, m_ClothPieceCapacity);
				return;
			}

			destVertexIndex = m_NextFreeVertex;
			destClothPieceIndex = (s32)m_NextFreeClothPiece;
			
			// Make a cloth piece record.
			m_pClothPieces[destClothPieceIndex].m_ReplayID = pClothInstance->GetUserData1();
			m_pClothPieces[destClothPieceIndex].m_OriginalVertHash = pClothInstance->GetUserData3();
			m_pClothPieces[destClothPieceIndex].m_ClothIdx = pClothInstance->GetUserData2();
			m_pClothPieces[destClothPieceIndex].m_VertIndex = destVertexIndex;
			m_pClothPieces[destClothPieceIndex].m_NumVertices = numVertices;
			m_pClothPieces[destClothPieceIndex].m_LOD = pClothInstance->GetClothController()->GetLOD();

			// Move on for next alloc.
			m_NextFreeVertex += numVertices;
			m_NextFreeClothPiece += 1;
		}
		else
		{
			destVertexIndex = m_pClothPieces[destClothPieceIndex].m_VertIndex;
		}
		replayAssertf(m_pClothPieces[destClothPieceIndex].m_NumVertices == (u32)numVertices, "ReplayClothPacketManager::ProvideClothVertices()...Expecting same vertex count!");
		// Copy off the vertices.
		sysMemCpy(&m_pClothVertices[destVertexIndex], &pVertices[startVertex], sizeof(Vec3V)*numVertices);
	}
}


//----------------------------------------------------------------------------------//
void ReplayClothPacketManager::PreClothSimUpdate(CPed *pLocalPlayer)
{
	if(pLocalPlayer == NULL)
		m_VertsReservedForPlayer = 0;

	u32 vertexTotal = 0;

	// Count the total cloth vertices on the player.
	for(u32 i=0; i<PV_MAX_COMP; i++)
	{
		clothController *pClothController = pLocalPlayer->GetClothController(i);

		if(pClothController)
			vertexTotal += pClothController->GetCloth(0)->GetNumVertices();
	}
	m_VertsReservedForPlayer = vertexTotal;
}


//----------------------------------------------------------------------------------//
void ReplayClothPacketManager::CollectClothVertices(void *pClothProvideAndCollectGlobalParam, void *pClothInstance, int numVertices, Vec3V *pVertices)
{
	if(!CReplayMgr::IsRecording())
		return;

	if(pClothProvideAndCollectGlobalParam)
		static_cast < ReplayClothPacketManager * >(pClothProvideAndCollectGlobalParam)->CollectClothVertices(static_cast < clothInstance * > (pClothInstance), numVertices, pVertices);
}


/****************************************************************************************************************************/
// Playback side functions.																									*/
/****************************************************************************************************************************/
void ReplayClothPacketManager::Extract(class CPacketClothPieces const *pPrevious, class CPacketClothPieces const *pNext, float interp, u32 gameTime)
{
	// Severe cloth links from previous contents.
	ConnectToRealClothInstances(false);

	m_NextFreeClothPiece = 0;
	m_NextFreeVertex = 0;

	CPacketClothPieces::CLOTH_PIECES_EXTRACTION_DEST dest;
	dest.pClothPieces = m_pClothPieces;
	dest.pNumClothPieces = &m_NextFreeClothPiece;
	dest.NumClothPiecesMax = m_ClothPieceCapacity;
		 
	dest.pVertices = m_pClothVertices;
	dest.pNumVertices = &m_NextFreeVertex;
	dest.NumVerticesMax = m_VertexCapacity;

	// Extract the vertices and cloth pieces.
	pPrevious->Extract(dest);

	// Collect the packet version.
	m_PacketVersion = pPrevious->GetPacketVersion();

	if(pNext)
	{
		// Allow the use of the 2nd half of the cloth and vertex buffers.
		dest.NumVerticesMax = m_VertexCapacity*2;
		dest.NumClothPiecesMax = m_ClothPieceCapacity*2;
		pNext->ExtractWithInterp(interp, dest);
	}

	// Connect them to the cloth instances.
	ConnectToRealClothInstances(true);

	// Maintain current time.
	m_CurrentTime = gameTime;
	m_CurrentTimeUpdatedFlag++;
}


//----------------------------------------------------------------------------------//
void ReplayClothPacketManager::ConnectToRealClothInstances(bool connect)
{
	for(u32 i=0; i<m_NextFreeClothPiece; i++)
	{
		CPhysical *pEntity = reinterpret_cast < CPhysical * >(CReplayMgr::GetEntity(m_pClothPieces[i].m_ReplayID));

		if(pEntity && pEntity->GetIsTypePed())
		{
			characterClothController *pCharClothContoller = reinterpret_cast < CPed * > (pEntity)->GetClothController(m_pClothPieces[i].m_ClothIdx);

			if(pCharClothContoller)
			{
				SetCallbackInfoInClothControllerEtc(pEntity, pCharClothContoller, NULL, m_pClothPieces[i].m_ClothIdx, 
					connect ? (cbFlags)(ComputeClothVertHash | SetReplayIdAndIndex | MarkAsReplayControlled) : (cbFlags)0, 
					clothController::CLOTH_CollectVertsFromReplay, connect); /* NOTE:- Character cloth returns to normal cloth simulation */
			}
		}
		else if(pEntity && pEntity->GetIsTypeVehicle())
		{
			fragInst *pFragInst = (reinterpret_cast < CVehicle * > (pEntity))->GetFragInst();

			if(pFragInst && pFragInst->GetCacheEntry() && pFragInst->GetCacheEntry()->GetHierInst())
			{
				environmentCloth *pEnvCloth = pFragInst->GetCacheEntry()->GetHierInst()->envCloth;
				if(pEnvCloth)
				{
					clothController *pClothController = static_cast < clothController * > (pEnvCloth->GetClothController());
					SetCallbackInfoInClothControllerEtc(pEntity, pClothController, pEnvCloth, connect ? m_pClothPieces[i].m_ClothIdx : REPLAY_CLOTH_IS_REPLAY_CONTROLLED_BUT_NOT_RECORDED, 
						connect ? (cbFlags)(ComputeClothVertHash | SetReplayIdAndIndex | MarkAsReplayControlled) : (cbFlags)(SetReplayIdAndIndex | MarkAsReplayControlled), 
						clothController::CLOTH_CollectVertsFromReplay, true); /* NOTE:- Vehicle cloth remains under Replay control - see CPacketFragData_NoDamageBits::Extract() */
				}
			}
		}
		else
		{
			replayAssertf(pEntity == NULL, "ReplayClothPacketManager::ConnectToRealClothInstances()...Should only encounter ped and vehicles!");
		}
	}
}


//----------------------------------------------------------------------------------//
clothManager::enPostInjectVerticesAction ReplayClothPacketManager::ProvideClothVertices(clothInstance *pClothInstance, int numVerticesTotal, Vec3V *pVertices)
{
	// Detect fine scrubbing has started (OnFineScrubbingEnd() is cslled by higher level code).
	if(CReplayMgr::IsFineScrubbing() && (ms_IsFineScrubbing == false))
		ms_IsFineScrubbing = true;

	//----------------------------------------------------------------------------------//

	{
		// Scoop the latest current time upon 1st cloth piece encountered.
		sysCriticalSection lock(m_Lock);

		if(m_CurrentTimeUpdatedFlag != 0)
		{
			m_ClothUpdateLastTime = m_ClothUpdateCurrentTime;
			m_ClothUpdateCurrentTime = m_CurrentTime;
			m_CurrentTimeUpdatedFlag = 0;
		}
	}

	//----------------------------------------------------------------------------------//

	// Do we have a Replay ID set ?
	if(pClothInstance->GetUserData1())
	{
		CReplayID replayID((s32)pClothInstance->GetUserData1());

		// Is this a recorded cloth piece ?
		if(pClothInstance->GetUserData2() != REPLAY_CLOTH_IS_REPLAY_CONTROLLED_BUT_NOT_RECORDED)
		{
			s32 idx = 0;
			int startVertex = m_PacketVersion >= g_CPacketClothPieces_Version1 ? GetVertexOffsetToRecordFrom(pClothInstance) : 0;
			int numVertices = numVerticesTotal - startVertex;

			if((idx = FindClothPiece(replayID, pClothInstance->GetUserData2(), pClothInstance->GetUserData3(), pClothInstance->GetClothController()->GetLOD(), m_PacketVersion >= g_CPacketClothPieces_Version1)) != -1)
			{
				if(m_pClothPieces[idx].m_NumVertices != (u32)numVertices)
				{
					// Data in game mismatches what we expected from the recording...
					// Just let the cloth manager simulate instead...might not look quite right but it won't be broke.
					replayDebugf1("ReplayClothPacketManager::ProvideClothVertices()...Differing vertex count. Most likely owing to LOD mismatch. %u, %u", m_pClothPieces[idx].m_NumVertices, (u32)numVertices);
					return clothManager::runSimulation;
				}

				// Supply the vertices.
				sysMemCpy(&pVertices[startVertex], &m_pClothVertices[m_pClothPieces[idx].m_VertIndex], sizeof(Vec3V)*numVertices);

				// Check against collision + update drawable geometry.
				return (clothManager::enPostInjectVerticesAction)(clothManager::checkAganstCollisionInsts | clothManager::updateDrawableGeometry);
			}
			else
			{
				if(pClothInstance->GetType() == clothManager::Character) // ResetClothVerticesIfPausedAndWhatNot()  won't work with character cloth...Just run cloth sim. as normal.
					return clothManager::runSimulation;

				// Use usual processing as a fall-back. We don't expect to reach here!
				return ResetClothVerticesIfPausedAndWhatNot(replayID, pClothInstance, pVertices, numVerticesTotal);
			}
		}
		else
		{
			return ResetClothVerticesIfPausedAndWhatNot(replayID, pClothInstance, pVertices, numVerticesTotal);
		}
	}
	return clothManager::runSimulation;
}


//----------------------------------------------------------------------------------//
clothManager::enPostInjectVerticesAction ReplayClothPacketManager::ProvideClothVertices(void *pClothProvideAndCollectGlobalParam, void *pClothInstance, int numVertices, Vec3V *pVertices)
{
	if(!CReplayMgr::IsEditModeActive())
		return clothManager::runSimulation;

	if(pClothProvideAndCollectGlobalParam)
		return static_cast < ReplayClothPacketManager * >(pClothProvideAndCollectGlobalParam)->ProvideClothVertices(static_cast < clothInstance * > (pClothInstance), numVertices, pVertices);

	return clothManager::runSimulation;
}


//----------------------------------------------------------------------------------//
void ReplayClothPacketManager::SetClothAsReplayControlled(CPhysical *pEntity, fragInst *pFragInst)
{
	fragCacheEntry* pFragCacheEntry = pFragInst->GetCacheEntry();

	if(pFragCacheEntry && pFragCacheEntry->GetHierInst() && pFragCacheEntry->GetHierInst()->envCloth)
	{
		environmentCloth *pEnvCloth = pFragCacheEntry->GetHierInst()->envCloth;
		clothController *pClothController = pEnvCloth->GetClothController();

		clothInstance *pClothInstance = reinterpret_cast < clothInstance * > (pClothController->GetClothInstance());

		// Only set up the call back info etc through this code path once (so as not to alter on-going settings via ConnectToRealClothInstances()).
		if(pClothInstance->GetUserData1() == 0)
			SetCallbackInfoInClothControllerEtc(pEntity, pClothController, pEnvCloth, REPLAY_CLOTH_IS_REPLAY_CONTROLLED_BUT_NOT_RECORDED, (cbFlags)(SetReplayIdAndIndex | MarkAsReplayControlled), clothController::CLOTH_CollectVertsFromReplay, true);
	}
}


//----------------------------------------------------------------------------------//
clothManager::enPostInjectVerticesAction ReplayClothPacketManager::ResetClothVerticesIfPausedAndWhatNot(CReplayID replayID, clothInstance *pClothInstance, Vec3V *pVertices, u32 numVerts)
{
	CPhysical *pEntity = reinterpret_cast < CPhysical * >(CReplayMgr::GetEntity(replayID));

	if(!pEntity)
		return clothManager::runSimulation; // We don't expect to reach here!

	clothManager::enPostInjectVerticesAction retVal;

	if(CReplayMgr::IsScrubbing() )
	{
		retVal = ProcessBigDeltas(pEntity, pClothInstance, pVertices, numVerts); // Run simulation.
	}
	else if(ms_IsFineScrubbing)
	{
		if(fwTimer::IsGamePaused())
			retVal = clothManager::updateDrawableGeometry; // Carry over current geometry into drawable for next frame.
		else
			retVal = ProcessBigDeltas(pEntity, pClothInstance, pVertices, numVerts); // Run simulation.
	}
	else if(CReplayMgr::IsReplayCursorJumping())
	{
		Mat34V entityMatrix = pEntity->GetMatrix();

		// Reset cloth vertices.
		ProcessClothLODs(pEntity, pClothInstance, pVertices, numVerts, entityMatrix, true);

		// Process collisions, update drawable geometry and reset the simulation deltas (so it's ok once we start up again).
		retVal = (clothManager::enPostInjectVerticesAction )(clothManager::checkAganstCollisionInsts | clothManager::updateDrawableGeometry | clothManager::resetDeltas);
	}
	else if(CReplayMgr::IsPlaybackPaused())
	{
		// Carry over current geometry into drawable for next frame.
		retVal =  clothManager::updateDrawableGeometry;
	}
	else
	{
		// Just usual update...Update simulation.
		retVal = ProcessBigDeltas(pEntity, pClothInstance, pVertices, numVerts); // Run simulation.
	}

	// Update the previous matrix.
	ReplayEntityExtension *pEntityExtension = ReplayEntityExtension::GetExtension(pEntity);
	replayAssertf(pEntityExtension, "ReplayClothPacketManager::ProcessBigDeltas()....Expecting extension");
	Mat34V entityMatrix = pEntity->GetMatrix();
	pEntityExtension->SetMatrix(entityMatrix);

	return retVal;
}


// In milliseconds, so about 3 frames.
#define BIG_DELTA 49
// About 1 frames.
#define BIG_DELTA_ADJUST 16

//----------------------------------------------------------------------------------//
clothManager::enPostInjectVerticesAction ReplayClothPacketManager::ProcessBigDeltas(CPhysical *pEntity, clothInstance *pClothInstance, Vec3V *pVertices, u32 numVerts)
{
	int deltaT = abs(m_ClothUpdateCurrentTime - m_ClothUpdateLastTime);

	if(abs(m_ClothUpdateCurrentTime - m_ClothUpdateLastTime) <= BIG_DELTA)
		return clothManager::runSimulation;

	float k = 1.0f - (float)BIG_DELTA_ADJUST/(float)deltaT;

	ReplayEntityExtension *pEntityExtension = ReplayEntityExtension::GetExtension(pEntity);
	replayAssertf(pEntityExtension, "ReplayClothPacketManager::ProcessBigDeltas()....Expecting extension");
	QuatV lastOrientation = pEntityExtension->GetOrientation();
	
	QuatV currentOrientation;
	Mat34V entityMatrix = pEntity->GetMatrix();
	currentOrientation = QuatVFromMat34V(entityMatrix);
	currentOrientation = PrepareSlerp(lastOrientation, currentOrientation);

	// Check for orientations being equal.
	if(fabs(Dot(lastOrientation, currentOrientation).Getf()) > (1.0f - SMALL_FLOAT))
		return clothManager::runSimulation;

	// Slerp to about a frames worth behind current orientation (i.e. a point from which we can safely run the simulation).
	QuatV safeToRunSimO = Slerp(ScalarV(k), lastOrientation, currentOrientation);

	Mat33V safeToRunSimM;
	Mat33VFromQuatV(safeToRunSimM, safeToRunSimO);
	Mat33V initialM;
	Mat33VFromQuatV(initialM, lastOrientation);
	Mat33V invInitialM;
	Transpose(invInitialM, initialM);

	// Transform from the initial space into the new safe to run sim from space.
	Mat33V finalTransfrom;
	Multiply(finalTransfrom, safeToRunSimM, invInitialM);
	
	Mat34V M;
	M.SetCol0(finalTransfrom.GetCol0());
	M.SetCol1(finalTransfrom.GetCol1());
	M.SetCol2(finalTransfrom.GetCol2());
	M.SetCol3(Vec3V(V_ZERO));
	ProcessClothLODs(pEntity, pClothInstance, pVertices, numVerts, M, false);

	return (clothManager::enPostInjectVerticesAction )(clothManager::checkAganstCollisionInsts | clothManager::updateDrawableGeometry | clothManager::resetDeltas | clothManager::runSimulation);
}


//----------------------------------------------------------------------------------//
void ReplayClothPacketManager::ProcessClothLODs(CPhysical *pEntity, clothInstance *pClothInstance, Vec3V *pVertices, u32 numVerts, Mat34V &mat, bool resetVertices)
{
	if(pEntity->GetIsTypePed())
	{
		return ProcessCharacterCloth(pClothInstance, pVertices, numVerts, mat, resetVertices);
	}
	else
	{
		return ProcessEnvClothLODs(pEntity, pClothInstance, mat, resetVertices);
	}
}


//----------------------------------------------------------------------------------//
void ReplayClothPacketManager::ProcessCharacterCloth(clothInstance *pClothInstance, Vec3V *pVertices, u32 numVerts, Mat34V &mat, bool resetVertices)
{
	(void)mat;
	(void)resetVertices;
	// Just reset verts for now. We don't expect to reach here.
	characterClothController *pCharContoller = reinterpret_cast < characterClothController * > (pClothInstance->GetClothController());
	Vec3V const *pOriginalVerts = pCharContoller->GetOriginalPos()->GetElements();
	sysMemCpy(pVertices, pOriginalVerts, sizeof(Vec3V)*numVerts);
}


//----------------------------------------------------------------------------------//
void ReplayClothPacketManager::ProcessEnvClothLODs(CPhysical *pEntity, clothInstance *pClothInstance, Mat34V &mat, bool resetVertices)
{
	fragInst *pFragInst = pEntity->GetFragInst();

	if(pFragInst)
	{
		const rage::fragType *pType = pFragInst->GetType();

		if(pType)
		{
			environmentCloth const *pEnvClothTemplate = &pType->GetTypeEnvCloth(0)->m_Cloth;

			// Check pointers just in case!
			if(pFragInst->GetCacheEntry() && pFragInst->GetCacheEntry()->GetHierInst() && pFragInst->GetCacheEntry()->GetHierInst()->envCloth)
			{
				// Reset the cloths last position.
				pClothInstance->SetLastPosition(mat.d());
				// NOTE: for vehicles we need the cloth in local space (copied from clothManager::InstanceCloth()).
				mat.SetCol3(Vec3V(V_ZERO));

				if(resetVertices)
				{
					// Reset all LODs.
					for(u32 i=0; i<LOD_COUNT; i++)
					{
						phVerletCloth const *pVerletClothTemplate = pEnvClothTemplate->GetClothController()->GetCloth(i);
						phVerletCloth *pVerletCloth = pFragInst->GetCacheEntry()->GetHierInst()->envCloth->GetCloth(i);

						if( pVerletCloth  && pVerletClothTemplate)
						{
							Vec3V *pNewPositions = pVerletCloth->GetClothData().GetVertexPointer();

							// Copy over initial vertices.
							sysMemCpy(pNewPositions, pVerletClothTemplate->GetClothData().GetVertexPointer(), sizeof(Vec3V)*pVerletClothTemplate->GetClothData().GetNumVerts());

							// Transform vertices into their original place.
							pVerletCloth->GetClothData().TransformVertexPositions( mat, 0 );
							// Reset the deltas
							pVerletCloth->ZeroDeltaPositions_WithCompression();

							// TODO:- Normals maybe.

							// Re-compute bounding volume.
							pVerletCloth->ComputeClothBoundingVolume();
						}
					}
				}
				else
				{
					Vector3 unused;

					// Transform current vertices (can only do current LOD).
					pClothInstance->GetClothController()->GetCurrentCloth()->GetClothData().TransformVertexPositions( mat, 0 );
				}
			}
		}
	}
}

/****************************************************************************************************************************/
// Common functions.																										//
/****************************************************************************************************************************/

void ReplayClothPacketManager::SetCallbackInfoInClothControllerEtc(CPhysical *pEntity, rage::clothController *pClothController, environmentCloth *pEnvCloth, u32 idx, cbFlags callbackFlags, clothController::enFlags clothFlags, bool clothFlagsValue)
{
	clothInstance *pClothInstance = reinterpret_cast < clothInstance * > (pClothController->GetClothInstance());

	// Set the callback info.
	u32 idxToUse = idx;
	CReplayID replayId(0);

	if(callbackFlags & SetReplayIdAndIndex)
	{
		idxToUse = idx;
		replayId = pEntity->GetReplayID();
	}

	pClothController->SetFlag(clothFlags, clothFlagsValue);
	pClothInstance->SetUserData1And2(replayId.ToInt(), idxToUse);

	//----------------------------------------------------------------------------------//

	// Set Replay controlled-ness.
	if(callbackFlags & MarkAsReplayControlled)
		pClothInstance->SetTypeModifiers(clothManager::ReplayControlled);
	else
		pClothInstance->ClearTypeModifiers(clothManager::ReplayControlled);

	if(pEnvCloth)
	{
		//  Copied from clothManager::InstanceClothDrawable().
		grmGeometry& geom = pEnvCloth->GetDrawable()->GetLodGroup().GetLod(0).GetModel(0)->GetGeometry(0);
		((grmGeometryQB &)geom).SetUseSecondaryQBIndex((callbackFlags & MarkAsReplayControlled) ? true : false);
	}

	//----------------------------------------------------------------------------------//

	// Compute vertex hash if required.
	if(callbackFlags & ComputeClothVertHash)
	{
		if(pEntity->GetIsTypePed())
		{
			replayAssertf(pClothInstance->GetType() == clothManager::Character, "ReplayClothPacketManager::SetCallbackInfoInClothContoller()...Expecting character cloth");
			SetCharacterClothHash(pClothController, pClothInstance);
		}
		else if(pEntity->GetIsTypeVehicle())
		{
			replayAssertf(pClothInstance->GetType() == clothManager::Vehicle, "ReplayClothPacketManager::SetCallbackInfoInClothContoller()...Expecting vehicle cloth");
			SetVehicleClothHash(pClothController, pClothInstance, reinterpret_cast < CVehicle * > (pEntity));
		}
		else
		{
			replayAssertf((callbackFlags & ComputeClothVertHash) == 0, "ReplayClothPacketManager::SetCallbackInfoInClothContoller()...Not expecting to compute vertex hash on an object");
		}
	}
}


//----------------------------------------------------------------------------------//
s32 ReplayClothPacketManager::FindClothPiece(CReplayID replayID, u32 clothIdx, u32 originalVertexHash, u32 LOD, bool takeLODintoAccount)
{
	for(u32 i=0; i<m_NextFreeClothPiece; i++)
	{
		if((m_pClothPieces[i].m_ReplayID == replayID) && (m_pClothPieces[i].m_ClothIdx == clothIdx) && (m_pClothPieces[i].m_OriginalVertHash == originalVertexHash))
		{
			if(takeLODintoAccount && (m_pClothPieces[i].m_LOD != LOD))
				continue;

			return (s32)i;
		}
	}
	return -1;
}


//----------------------------------------------------------------------------------//
void ReplayClothPacketManager::SetCharacterClothHash(rage::clothController *pClothController, clothInstance *pClothInstance)
{
	replayAssertf(pClothInstance->GetType() == clothManager::Character, "ReplayClothPacketManager::SetCharacterClothHash()...Expecting character cloth");

	// Compute a CRC of the original vertices, this way we can detect model changes.
	if(pClothInstance->GetUserData3() == 0)
	{
		characterClothController *pCharContoller = reinterpret_cast < characterClothController * > (pClothController);
		Vec3V const *pVerts = pCharContoller->GetOriginalPos()->GetElements();
		u32 hash = CReplayMgr::hash((u32 *)pVerts, pCharContoller->GetOriginalPos()->GetCount()*4, 0);
		pClothInstance->SetUserData3(hash);
	}
}


//----------------------------------------------------------------------------------//
void ReplayClothPacketManager::SetVehicleClothHash(rage::clothController *pClothController, clothInstance *pClothInstance, CVehicle *pVehicle)
{
	(void)pClothController;
	replayAssertf(pClothInstance->GetType() == clothManager::Vehicle, "ReplayClothPacketManager::SetVehicleClothHash()...Expecting vehicle cloth");
	
	if(pClothInstance->GetUserData3() == 0)
	{
		fragInst *pFragInst = pVehicle->GetFragInst();

		if(pFragInst)
		{
			const rage::fragType *pType = pFragInst->GetType();

			if(pType)
			{
				environmentCloth const  *pEnvClothType = &pType->GetTypeEnvCloth(0)->m_Cloth;
				phClothData const &clothData = pEnvClothType->GetCloth()->GetClothData();

				u32 hash = CReplayMgr::hash((u32 *)clothData.GetVertexPointer(), clothData.GetNumVerts()*4, 0);
				pClothInstance->SetUserData3(hash);
			}
		}
	}
}


//----------------------------------------------------------------------------------//
void ReplayClothPacketManager::PrintState(char OUTPUT_ONLY(*pStr), bool OUTPUT_ONLY(allowDouble))
{
	replayDisplayf("ReplayClothPacketManager::PrintState().......Start %s", pStr);

	replayDisplayf("Cloth pieces = %d[%d], No of verts %d[%d]", m_NextFreeClothPiece, !allowDouble ? m_ClothPieceCapacity : m_ClothPieceCapacity*2, m_NextFreeVertex, !allowDouble ? m_VertexCapacity : m_VertexCapacity*2);

	for(u32 i=0; i< m_NextFreeClothPiece; i++)
	{
		Vec3V pos = Vec3V(0.0f, 0.0f, 0.0f);
		CEntity *pEntity = CReplayMgr::GetEntity(m_pClothPieces[i].m_ReplayID);

		if(pEntity)
			pos = pEntity->GetMatrix().d();

		replayDisplayf("%d) %08x: [%d->%d] %d (%f, %f, %f), ", i, m_pClothPieces[i].m_ReplayID.ToInt(), m_pClothPieces[i].m_VertIndex, m_pClothPieces[i].m_VertIndex + m_pClothPieces[i].m_NumVertices, m_pClothPieces[i].m_NumVertices, pos.GetXf(), pos.GetYf(), pos.GetZf());
	}

	replayDisplayf("ReplayClothPacketManager::PrintState().......End");
}


//----------------------------------------------------------------------------------//
u32 ReplayClothPacketManager::GetVertexOffsetToRecordFrom(clothInstance *pClothInstance)
{
	if(pClothInstance->GetType() & (u32)(clothManager::Vehicle | clothManager::Environment))
	{
		environmentCloth *pEnvCloth = reinterpret_cast < environmentCloth * > (pClothInstance->GetClothController()->GetOwner());
		phVerletCloth* pCloth = pEnvCloth->GetCloth();
		// Record only the non-pinned vertices (i.e. the ones which move freely).
		return pCloth->GetClothData().GetNumPinVerts();
	}
	// Character cloth we record all vertices for now.
	return 0;
}


//----------------------------------------------------------------------------------//
void ReplayClothPacketManager::OnEntry()
{
	m_CurrentTime = 0;
	m_CurrentTimeUpdatedFlag = 0;
	m_ClothUpdateLastTime = 0;
	m_ClothUpdateCurrentTime = 0;
}


//----------------------------------------------------------------------------------//
void ReplayClothPacketManager::OnExit()
{

}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CPacketClothPieces.																										//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CPacketClothPieces::Store(CLOTH_PIECES_TO_RECORD &clothToRecord)
{
	PACKET_EXTENSION_RESET(CPacketClothPieces); 
	CPacketBase::Store(PACKETID_CLOTH_PIECES, sizeof(CPacketClothPieces), g_CPacketClothPieces_Version1);

	m_NoOfClothPieces = clothToRecord.numClothPieces;
	m_NoOfVertices = clothToRecord.numVertices;
	AddToPacketSize(sizeof(CLOTH_PIECE)*m_NoOfClothPieces + sizeof(CPacketVector3Comp <  CLOTH_VERTEX_COMPONENT_TYPE >)*m_NoOfVertices);

	CLOTH_PIECE *pDestPieces = GetClothPieces();
	CPacketVector3Comp <  CLOTH_VERTEX_COMPONENT_TYPE > *pDestVertices = GetClothVertices();

	for(u32 i=0; i<clothToRecord.numClothPieces; i++)
	{
		Vec3V boxMin(FLT_MAX, FLT_MAX, FLT_MAX);
		Vec3V boxMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);
		pDestPieces[i].m_ClothIdx = (u16)clothToRecord.pClothPieces[i].m_ClothIdx;
		pDestPieces[i].m_NofVertices = (u16)clothToRecord.pClothPieces[i].m_NumVertices;
		pDestPieces[i].m_ReplayID = clothToRecord.pClothPieces[i].m_ReplayID;
		pDestPieces[i].m_VertexIndex = clothToRecord.pClothPieces[i].m_VertIndex;
		pDestPieces[i].m_OriginalVertHash = clothToRecord.pClothPieces[i].m_OriginalVertHash;
		pDestPieces[i].m_LOD = clothToRecord.pClothPieces[i].m_LOD;
		pDestPieces[i].m_Unused2 = 0;

		Vec3V *pBase = &clothToRecord.pVertices[clothToRecord.pClothPieces[i].m_VertIndex];
		CPacketVector3Comp <  CLOTH_VERTEX_COMPONENT_TYPE > *pBaseDest = &pDestVertices[clothToRecord.pClothPieces[i].m_VertIndex];

		for(u32 vtx=0; vtx<clothToRecord.pClothPieces[i].m_NumVertices; vtx++)
		{
			boxMin = Min(pBase[vtx], boxMin);
			boxMax = Max(pBase[vtx], boxMax);
		}

		pDestPieces[i].m_BoxMin[0] = boxMin.GetXf();
		pDestPieces[i].m_BoxMin[1] = boxMin.GetYf();
		pDestPieces[i].m_BoxMin[2] = boxMin.GetZf();

		pDestPieces[i].m_BoxMax[0] = boxMax.GetXf();
		pDestPieces[i].m_BoxMax[1] = boxMax.GetYf();
		pDestPieces[i].m_BoxMax[2] = boxMax.GetZf();

		// Assume CLOTH_VERTEX_COMPONENT_TYPE is not a float.
		float compressedRange = (float)((0x1 << (sizeof(CLOTH_VERTEX_COMPONENT_TYPE)*8)) - 1);
		Vec3V compressScale = Vec3V(compressedRange, compressedRange, compressedRange)/(boxMax - boxMin);

		for(u32 vtx=0; vtx<clothToRecord.pClothPieces[i].m_NumVertices; vtx++)
		{
			Vec3V compressed = (pBase[vtx] - boxMin)*compressScale;
			pBaseDest[vtx].Set((CLOTH_VERTEX_COMPONENT_TYPE)compressed.GetXf(), (CLOTH_VERTEX_COMPONENT_TYPE)compressed.GetYf(), (CLOTH_VERTEX_COMPONENT_TYPE)compressed.GetZf()); 
		}
	}
}


//----------------------------------------------------------------------------------//
void CPacketClothPieces::StoreExtensions(CLOTH_PIECES_TO_RECORD &clothToRecord)
{
	(void)clothToRecord;
}


/****************************************************************************************************************************/
// Playback side functions.																									*/
/****************************************************************************************************************************/
void CPacketClothPieces::Extract(CLOTH_PIECES_EXTRACTION_DEST &dest) const
{
	replayAssertf(m_NoOfClothPieces < dest.NumClothPiecesMax, "CPacketClothPieces::Extract()...Too many cloth pieces in packet (%d, %d)", m_NoOfClothPieces, dest.NumClothPiecesMax);
	replayAssertf(m_NoOfVertices < dest.NumVerticesMax, "CPacketClothPieces::Extract()...Too many cloth vertices in packet (%d, %d)", m_NoOfVertices, dest.NumVerticesMax);
	*dest.pNumClothPieces = m_NoOfClothPieces;
	*dest.pNumVertices = m_NoOfVertices;

	CLOTH_PIECE *pSrcClothPieces = GetClothPieces();
	CPacketVector3Comp <  CLOTH_VERTEX_COMPONENT_TYPE > *pSrcVertices = GetClothVertices();

	for(u32 i=0; i<m_NoOfClothPieces; i++)
	{
		dest.pClothPieces[i].m_ClothIdx = pSrcClothPieces[i].m_ClothIdx;
		dest.pClothPieces[i].m_NumVertices = pSrcClothPieces[i].m_NofVertices;
		dest.pClothPieces[i].m_ReplayID = pSrcClothPieces[i].m_ReplayID;
		dest.pClothPieces[i].m_VertIndex = pSrcClothPieces[i].m_VertexIndex;
		dest.pClothPieces[i].m_OriginalVertHash = pSrcClothPieces[i].m_OriginalVertHash;
		dest.pClothPieces[i].m_LOD = 0;

		if(GetPacketVersion() >= g_CPacketClothPieces_Version1)
			dest.pClothPieces[i].m_LOD = pSrcClothPieces[i].m_LOD;

		Vec3V *pDestBase = &dest.pVertices[pSrcClothPieces[i].m_VertexIndex];
		CPacketVector3Comp <  CLOTH_VERTEX_COMPONENT_TYPE > *pSrcBase = &pSrcVertices[pSrcClothPieces[i].m_VertexIndex];

		Vec3V boxMin = Vec3V(pSrcClothPieces[i].m_BoxMin[0], pSrcClothPieces[i].m_BoxMin[1], pSrcClothPieces[i].m_BoxMin[2]);
		Vec3V boxMax = Vec3V(pSrcClothPieces[i].m_BoxMax[0], pSrcClothPieces[i].m_BoxMax[1], pSrcClothPieces[i].m_BoxMax[2]);

		replayAssertf(pSrcClothPieces[i].m_VertexIndex < dest.NumVerticesMax && (pSrcClothPieces[i].m_VertexIndex + pSrcClothPieces[i].m_NofVertices) < dest.NumVerticesMax, "CPacketClothPieces::Extract()....Vertices out of range.");
		UnpackVertices(pDestBase, pSrcBase, pSrcClothPieces[i].m_NofVertices, boxMin, boxMax);
	}
}


void CPacketClothPieces::ExtractWithInterp(float interp, CLOTH_PIECES_EXTRACTION_DEST &dest) const
{
	CLOTH_PIECE *pSrcClothPieces = GetClothPieces();
	CPacketVector3Comp <  CLOTH_VERTEX_COMPONENT_TYPE > *pSrcVertices = GetClothVertices();

	// Collect the current cloth and vertex count.
	u32 currentNumClothPieces = *dest.pNumClothPieces;
	u32 currentNumVertices = *dest.pNumVertices;

	for(u32 i=0; i<m_NoOfClothPieces; i++)
	{
		// Find a match amongst the cloth already extracted.
		s32 existingIdx = FindClothPiece(pSrcClothPieces[i], dest.pClothPieces, *dest.pNumClothPieces, GetPacketVersion() >= g_CPacketClothPieces_Version1);

		CPacketVector3Comp <  CLOTH_VERTEX_COMPONENT_TYPE > *pSrcBase = &pSrcVertices[pSrcClothPieces[i].m_VertexIndex];
		Vec3V boxMin = Vec3V(pSrcClothPieces[i].m_BoxMin[0], pSrcClothPieces[i].m_BoxMin[1], pSrcClothPieces[i].m_BoxMin[2]);
		Vec3V boxMax = Vec3V(pSrcClothPieces[i].m_BoxMax[0], pSrcClothPieces[i].m_BoxMax[1], pSrcClothPieces[i].m_BoxMax[2]);

		if(existingIdx != -1)
		{
			// NOTE:- We use the vertex index of the existing matching cloth piece.
			Vec3V *pDestBase = &dest.pVertices[dest.pClothPieces[existingIdx].m_VertIndex];
			// Interpolate into the existing cloth piece (from previous frame).
			UnpackVerticesWithInterp(interp, pDestBase, pSrcBase, pSrcClothPieces[i].m_NofVertices, boxMin, boxMax);
		}
		else
		{
			replayAssertf(currentNumClothPieces < dest.NumClothPiecesMax, "CPacketClothPieces::ExtractWithInterp()...Out of dest cloth piece capacity.");
			replayAssertf((currentNumVertices + pSrcClothPieces[i].m_NofVertices) <= dest.NumVerticesMax, "CPacketClothPieces::ExtractWithInterp()...Out of dest cloth piece capacity.");

			// Add this new one on the end.
			dest.pClothPieces[currentNumClothPieces].m_ClothIdx = pSrcClothPieces[i].m_ClothIdx;
			dest.pClothPieces[currentNumClothPieces].m_NumVertices = pSrcClothPieces[i].m_NofVertices;
			dest.pClothPieces[currentNumClothPieces].m_ReplayID = pSrcClothPieces[i].m_ReplayID;
			dest.pClothPieces[currentNumClothPieces].m_VertIndex = currentNumVertices;
			dest.pClothPieces[currentNumClothPieces].m_OriginalVertHash = pSrcClothPieces[i].m_OriginalVertHash;
			dest.pClothPieces[currentNumClothPieces].m_LOD = 0;

			if(GetPacketVersion() >= g_CPacketClothPieces_Version1)
				dest.pClothPieces[i].m_LOD = pSrcClothPieces[i].m_LOD;

			// Unpack to end of vertex list.
			Vec3V *pDestBase = &dest.pVertices[currentNumVertices];
			UnpackVertices(pDestBase, pSrcBase, pSrcClothPieces[i].m_NofVertices, boxMin, boxMax);

			// Increment counts.
			currentNumVertices += pSrcClothPieces[i].m_NofVertices;
			currentNumClothPieces++;
		}
	}
	// Update cloth and vertex count.
	*dest.pNumClothPieces = currentNumClothPieces;
	*dest.pNumVertices = currentNumVertices;
}


s32 CPacketClothPieces::FindClothPiece(CLOTH_PIECE &piece, ReplayClothPacketManager::CLOTH_PIECE *pClothPieces, s32 count, bool takeLODintoAccount)
{
	for(s32 i=0; i<count; i++)
	{
		if((piece.m_ReplayID == (u32)pClothPieces[i].m_ReplayID.ToInt()) 
		&& (piece.m_ClothIdx == pClothPieces[i].m_ClothIdx) 
		&& (piece.m_OriginalVertHash == pClothPieces[i].m_OriginalVertHash) 
		&& (piece.m_NofVertices == pClothPieces[i].m_NumVertices))
		{
			if(takeLODintoAccount && (piece.m_LOD != pClothPieces[i].m_LOD))
				continue;

			return i;
		}
	}
	return - 1;
}


void CPacketClothPieces::UnpackVertices(Vec3V *pDestBase, CPacketVector3Comp <  CLOTH_VERTEX_COMPONENT_TYPE > *pSrcBase, u32 noOfVertices, Vec3V &boxMin, Vec3V &boxMax)
{
	// Assume CLOTH_VERTEX_COMPONENT_TYPE is not a float.
	float compressedRange = (float)((0x1 << (sizeof(CLOTH_VERTEX_COMPONENT_TYPE)*8)) - 1);
	Vec3V decompressScale = (boxMax - boxMin)/Vec3V(compressedRange, compressedRange, compressedRange);

	for(u32 i=0; i<noOfVertices; i++)
	{
		CLOTH_VERTEX_COMPONENT_TYPE x, y, z;
		pSrcBase[i].Get(x, y, z);
		Vec3V v = Vec3V((float)x, (float)y, (float)z); 
		pDestBase[i] = v*decompressScale + boxMin;
	}
}


void CPacketClothPieces::UnpackVerticesWithInterp(float interp, Vec3V *pDestBase, CPacketVector3Comp <  CLOTH_VERTEX_COMPONENT_TYPE > *pSrcBase, u32 noOfVertices, Vec3V &boxMin, Vec3V &boxMax)
{
	ScalarV K(interp);
	ScalarV OneMinusK(1.0f - interp);
	// Assume CLOTH_VERTEX_COMPONENT_TYPE is not a float.
	float compressedRange = (float)((0x1 << (sizeof(CLOTH_VERTEX_COMPONENT_TYPE)*8)) - 1);
	Vec3V decompressScale = (boxMax - boxMin)/Vec3V(compressedRange, compressedRange, compressedRange);

	for(u32 i=0; i<noOfVertices; i++)
	{
		CLOTH_VERTEX_COMPONENT_TYPE x, y, z;
		pSrcBase[i].Get(x, y, z);
		Vec3V v = Vec3V((float)x, (float)y, (float)z); 
		v = v*decompressScale + boxMin;
		pDestBase[i] = pDestBase[i]*OneMinusK + v*K;
	}

}

#endif // GTA_REPLAY
