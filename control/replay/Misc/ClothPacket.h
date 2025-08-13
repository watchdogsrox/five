#ifndef __CLOTH_PACKET_H__
#define __CLOTH_PACKET_H__

#include "Control/replay/ReplaySettings.h"

#if GTA_REPLAY

#include "control/replay/Misc/ReplayPacket.h"
#include "Control/replay/PacketBasics.h"
#include "Control/replay/Misc/LinkedPacket.h"
#include "control/replay/ReplaySupportClasses.h"
#include "fwpheffects/clothmanager.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ReplayClothPacketManager.																								//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class CReplayRecorder;

class ReplayClothPacketManager
{
public:
	typedef struct CLOTH_PIECE
	{
		CReplayID m_ReplayID;
		u32 m_ClothIdx;
		u32 m_VertIndex;
		u32 m_NumVertices;
		u32 m_OriginalVertHash;
		u32 m_LOD;
	} CLOTH_PIECE;
public:
	ReplayClothPacketManager();
	~ReplayClothPacketManager();

public:
	void Init(s32 maxPerFrameClothvertices, s32 maxPerFrameClothPieces);
	void CleanUp();

public:
	// Recording side functions.
	void RecordCloth(CPhysical *pEntity, rage::clothController *pClothController, u32 idx);
	void RecordPackets(class CReplayRecorder &recorder);
	void CollectClothVertices(clothInstance *pClothInstance, int numVertices, Vec3V *pVertices);
	void PreClothSimUpdate(CPed *pLocalPlayer);
	static void CollectClothVertices(void *pClothProvideAndCollectGlobalParam, void *pClothInstance, int numVertices, Vec3V *pVertices);

public:
	// Playback side functions.
	void Extract(class CPacketClothPieces const *pPrevious, class CPacketClothPieces const *pNext, float interp, u32 gameTime);
	void ConnectToRealClothInstances(bool connect);
	clothManager::enPostInjectVerticesAction ProvideClothVertices(clothInstance *pClothInstance, int numVertices, Vec3V *pVertices);
	static clothManager::enPostInjectVerticesAction ProvideClothVertices(void *pClothProvideAndCollectGlobalParam, void *pClothInstance, int numVertices, Vec3V *pVertices);

	void SetClothAsReplayControlled(CPhysical *pEntity, fragInst *pFragInst);
	clothManager::enPostInjectVerticesAction ResetClothVerticesIfPausedAndWhatNot(CReplayID replayID, clothInstance *pClothInstance, Vec3V *pVertices, u32 numVerts);
	clothManager::enPostInjectVerticesAction ProcessBigDeltas(CPhysical *pEntity, clothInstance *pClothInstance, Vec3V *pVertices, u32 numVerts);
	void ProcessClothLODs(CPhysical *pEntity, clothInstance *pClothInstance, Vec3V *pVertices, u32 numVerts, Mat34V &mat, bool resetVertices);
	void ProcessCharacterCloth(clothInstance *pClothInstance, Vec3V *pVertices, u32 numVerts, Mat34V &mat, bool resetVertices);
	void ProcessEnvClothLODs(CPhysical *pEntity, clothInstance *pClothInstance, Mat34V &mat, bool resetVertices);

public:
	// Misc functions.
	static void OnFineScrubbingEnd() { ms_IsFineScrubbing = false; 	}
	static void OnEntry();
	static void OnExit();

private:
	// Common functions.
	enum cbFlags
	{
		MarkAsReplayControlled = 1 << 0,
		SetReplayIdAndIndex = 1 << 1,
		ComputeClothVertHash = 1 << 2,
	};

	void SetCallbackInfoInClothControllerEtc(CPhysical *pEntity, rage::clothController *pClothController, environmentCloth *pEnvCloth, u32 idx, cbFlags callbackFlags, clothController::enFlags clothFlags, bool clothFlagsValue);
	s32 FindClothPiece(CReplayID replayID, u32 clothIdx, u32 originalVertexHash, u32 LOD, bool takeLODintoAccount);
	void SetCharacterClothHash(rage::clothController *pClothController, clothInstance *pClothInstance);
	void SetVehicleClothHash(rage::clothController *pClothController, clothInstance *pClothInstance, CVehicle *pVehicle);
	void PrintState(char OUTPUT_ONLY(*pStr), bool OUTPUT_ONLY(allowDouble));
	u32 GetVertexOffsetToRecordFrom(clothInstance *pClothInstance);

private:
	u32 m_VertexCapacity;
	u32 m_NextFreeVertex;
	u32 m_VertsReservedForPlayer;
	Vec3V *m_pClothVertices;
	u32 m_NextFreeClothPiece;
	u32 m_ClothPieceCapacity;
	CLOTH_PIECE *m_pClothPieces;
	tPacketVersion m_PacketVersion;
	sysCriticalSectionToken	m_Lock;
	static int m_CurrentTime;
	static int m_CurrentTimeUpdatedFlag;
	static int m_ClothUpdateLastTime;
	static int m_ClothUpdateCurrentTime;
	static bool ms_IsFineScrubbing;
};


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CPacketClothPieces.																										//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class CPacketClothPieces : public CPacketLinked<CPacketClothPieces>
{
public:
	typedef u8 CLOTH_VERTEX_COMPONENT_TYPE;

	typedef struct CLOTH_PIECES_TO_RECORD
	{
		ReplayClothPacketManager::CLOTH_PIECE *pClothPieces;
		u32 numClothPieces;
		Vec3V *pVertices;
		u32 numVertices;
	} CLOTH_PIECES_TO_RECORD;

	typedef struct CLOTH_PIECES_EXTRACTION_DEST
	{
		ReplayClothPacketManager::CLOTH_PIECE *pClothPieces;
		u32 *pNumClothPieces;
		u32 NumClothPiecesMax;
		Vec3V *pVertices;
		u32 *pNumVertices;
		u32 NumVerticesMax;
	} CLOTH_PIECES_EXTRACTION_DEST;

	// Structure saved into the clip.
	typedef struct CLOTH_PIECE
	{
		float m_BoxMin[3];
		float m_BoxMax[3];
		u16 m_ClothIdx; 
		u16 m_NofVertices;
		u32 m_ReplayID;
		u32 m_VertexIndex;
		u32 m_OriginalVertHash;
		u32 m_LOD; // Version 1 upwards.
		u32 m_Unused2;
	} CLOTH_PIECE;

public:
	CPacketClothPieces()
	: CPacketLinked(PACKETID_CLOTH_PIECES, sizeof(CPacketClothPieces))
	{}

public:
	void Store(CLOTH_PIECES_TO_RECORD &clothToRecord);
	void StoreExtensions(CLOTH_PIECES_TO_RECORD &clothToRecord);

public:
	void Extract(CLOTH_PIECES_EXTRACTION_DEST &dest) const;
	void ExtractWithInterp(float interp, CLOTH_PIECES_EXTRACTION_DEST &dest) const;
	static s32 FindClothPiece(CLOTH_PIECE &piece, ReplayClothPacketManager::CLOTH_PIECE *pClothPieces, s32 count, bool takeLODintoAccount);
	static void	UnpackVertices(Vec3V *pDestBase, CPacketVector3Comp <  CLOTH_VERTEX_COMPONENT_TYPE > *pSrcBase, u32 nofVertices, Vec3V &boxMin, Vec3V &boxMax);
	static void UnpackVerticesWithInterp(float interp, Vec3V *pDestBase, CPacketVector3Comp <  CLOTH_VERTEX_COMPONENT_TYPE > *pSrcBase, u32 noOfVertices, Vec3V &boxMin, Vec3V &boxMax);

public:
	CLOTH_PIECE *GetClothPieces() const
	{
		return (CLOTH_PIECE *)((u8 *)GetClothVertices() - sizeof(CLOTH_PIECE)*m_NoOfClothPieces);
	}
	CPacketVector3Comp <  CLOTH_VERTEX_COMPONENT_TYPE > *GetClothVertices() const
	{
		return (CPacketVector3Comp <  CLOTH_VERTEX_COMPONENT_TYPE > *)((u8 *)this + GetPacketSize() - GET_PACKET_EXTENSION_SIZE(CPacketClothPieces) - sizeof(CPacketVector3Comp <  CLOTH_VERTEX_COMPONENT_TYPE >)*m_NoOfVertices);
	}
	bool ValidatePacket() const	
	{	
		replayAssertf(GetPacketID() == PACKETID_CLOTH_PIECES, "Validation of ReplayClothPacketManager Failed!, 0x%08X", GetPacketID());
		replayAssertf(VALIDATE_PACKET_EXTENSION(CPacketClothPieces), "Validation of ReplayClothPacketManager extension failed!, 0x%08X", GetPacketID());
		return (GetPacketID() == PACKETID_CLOTH_PIECES) && CPacketBase::ValidatePacket() && VALIDATE_PACKET_EXTENSION(CPacketClothPieces);	
	}
private:
	u32 m_NoOfClothPieces;
	u32 m_NoOfVertices;
	DECLARE_PACKET_EXTENSION(CPacketClothPieces);
};

#endif // GTA_REPLAY

#endif // __CLOTH_PACKET_H__
