//
// name:		VehiclePlanePacket.cpp
//

#include "control/replay/Vehicle/VehiclePlanePacket.h"

// Replay only setup for win32
#if GTA_REPLAY

#include "control/replay/Misc/ReplayPacket.h"
#include "control/replay/ReplayInternal.h"
#include "vehicles/LandingGear.h"
#include "vehicles/Planes.h"
#include "scene/world/GameWorld.h"
#include "control/replay/Compression/Quantization.h"

/*TODO4FIVE extern audVolumeCone gSecondaryPlaneCone[TTYPE_NUMBEROFTHEM_ONLY_PLANES];
extern audGtaOcclusionGroup *pPlaneOcclusionGroups[TTYPE_NUMBEROFTHEM_ONLY_PLANES];*/

//========================================================================================================================
CPacketPlaneDelete::CPacketPlaneDelete()
: CPacketBase(PACKETID_PLANEDELETE, sizeof(CPacketPlaneDelete))
{
}

//========================================================================================================================
void CPacketPlaneDelete::Init(s32 replayID)
{
	CPacketBase::Store(PACKETID_PLANEDELETE, sizeof(CPacketPlaneDelete));
	m_ReplayID = replayID;
}

tPacketVersion g_PacketPlaneUpdate_v1 = 1;
tPacketVersion g_PacketPlaneUpdate_v2 = 2;
//////////////////////////////////////////////////////////////////////////
void CPacketPlaneUpdate::Store(CPlane* pPlane, bool storeCreateInfo)
{
	PACKET_EXTENSION_RESET(CPacketPlaneUpdate);
	replayAssert(pPlane);

	for (int i=0; i<2; i++)
	{
		SetSectionHealth(i, pPlane->GetAircraftDamage().GetSectionHealth((CAircraftDamage::eAircraftSection)(CAircraftDamage::ENGINE_L+i)));
	}

	m_EngineSpeed = pPlane->GetEngineSpeed();

	CPacketVehicleUpdate::Store(PACKETID_PLANEUPDATE, sizeof(CPacketPlaneUpdate), pPlane, storeCreateInfo);
}
//////////////////////////////////////////////////////////////////////////
void CPacketPlaneUpdate::StoreExtensions(CPlane* pPlane, bool storeCreateInfo) 
{ 
	PACKET_EXTENSION_RESET(CPacketPlaneUpdate);

	CPacketVehicleUpdate::StoreExtensions(PACKETID_PLANEUPDATE, sizeof(CPacketPlaneUpdate), pPlane, storeCreateInfo);

	(void)pPlane;
	(void)storeCreateInfo;

	PacketExtensions *pExtensions = (PacketExtensions *)BEGIN_PACKET_EXTENSION_WRITE(g_PacketPlaneUpdate_v2, CPacketPlaneUpdate);

	//---------------------- Extensions V1 ------------------------/
	pExtensions->m_LandingGearState = (u8)pPlane->GetLandingGear().GetInternalState();
	pExtensions->m_unused[0] = 0;
	pExtensions->m_unused[1] = 0;
	pExtensions->m_unused[2] = 0;

	//---------------------- Extensions V2 ------------------------/
	pExtensions->m_verticalFlightRatio = pPlane->GetVerticalFlightModeRatio();

	END_PACKET_EXTENSION_WRITE(pExtensions+1, CPacketPlaneUpdate);
}

//////////////////////////////////////////////////////////////////////////
void CPacketPlaneUpdate::Extract(const CInterpEventInfo<CPacketPlaneUpdate, CPlane>& info) const
{
	CPacketVehicleUpdate::Extract(info);

	CPlane* pPlane = info.GetEntity();

	if( pPlane )
	{
		for (int i=0; i<2; i++)
		{
			pPlane->GetAircraftDamage().SetSectionHealth((CAircraftDamage::eAircraftSection)(CAircraftDamage::ENGINE_L+i), GetSectionHealth(i));
		}
		
		pPlane->SetEngineSpeed(m_EngineSpeed);	

		ExtractExtensions(pPlane);
	}
}

//////////////////////////////////////////////////////////////////////////
void CPacketPlaneUpdate::ExtractExtensions(CPlane* pPlane) const
{
	if(GET_PACKET_EXTENSION_VERSION(CPacketPlaneUpdate) >= g_PacketPlaneUpdate_v1)
	{
		PacketExtensions *pExtensions = (PacketExtensions *)GET_PACKET_EXTENSION_READ_PTR_THIS(CPacketPlaneUpdate);

		pPlane->GetLandingGear().SetState((CLandingGear::eLandingGearState)pExtensions->m_LandingGearState);

		if(GET_PACKET_EXTENSION_VERSION(CPacketPlaneUpdate) >= g_PacketPlaneUpdate_v2)
		{
			pPlane->SetVerticalFlightModeRatio(pExtensions->m_verticalFlightRatio);
		}
	}
}


//========================================================================================================================
CPacketCarriageDelete::CPacketCarriageDelete()
: CPacketBase(PACKETID_CARRIAGEDELETE, sizeof(CPacketCarriageDelete))
{
}

//========================================================================================================================
void CPacketCarriageDelete::Init(s32 replayID)
{
	CPacketBase::Store(PACKETID_CARRIAGEDELETE, sizeof(CPacketCarriageDelete));
	m_ReplayID = replayID;
}

//========================================================================================================================
CPacketCarriageUpdate::CPacketCarriageUpdate()
: CPacketBase(PACKETID_CARRIAGEUPDATE, sizeof(CPacketCarriageUpdate))
{
}

//========================================================================================================================
CPacketCarriageUpdate::~CPacketCarriageUpdate()
{
}

//========================================================================================================================
void CPacketCarriageUpdate::Store(const Matrix34& rMatrix)
{
	Quaternion q;
	rMatrix.ToQuaternion(q);

	m_CarriageQuaternion = QuantizeQuaternionS3(q);

	m_Position[0] = rMatrix.d.x;
	m_Position[1] = rMatrix.d.y;
	m_Position[2] = rMatrix.d.z;
}

//========================================================================================================================
void CPacketCarriageUpdate::Load(Matrix34& rMatrix)
{
	Quaternion q;
	DeQuantizeQuaternionS3(q, m_CarriageQuaternion);
	rMatrix.FromQuaternion(q);
	replayAssert(rMatrix.IsOrthonormal());

	rMatrix.d.x = m_Position[0];
	rMatrix.d.y = m_Position[1];
	rMatrix.d.z = m_Position[2];
}

//========================================================================================================================
void CPacketCarriageUpdate::Store(CPhysical* pCarriage, u8 uSlotIndex)
{
	replayAssert(pCarriage);
	replayAssert(pCarriage->GetIsTypeBuilding());

	CPacketBase::Store(PACKETID_CARRIAGEUPDATE, sizeof(CPacketCarriageUpdate));
	CPacketInterp::Store();

	m_ReplayID		= pCarriage->GetReplayID();
	m_ModelIndex	= (u16)pCarriage->GetModelIndex();
	m_CarriageSlot	= uSlotIndex;
	m_AlphaFade		= pCarriage->GetAlpha();

	Store( MAT34V_TO_MATRIX34(pCarriage->GetMatrix()) );
}

//========================================================================================================================
void CPacketCarriageUpdate::Extract(CPhysical* pCarriage, float fInterp)
{
	if (pCarriage)
	{
		replayAssert(pCarriage->GetIsTypeBuilding());

		pCarriage->SetAlpha(m_AlphaFade);
		//pCarriage->Remove();

		Matrix34 oMatrix;
		if (pCarriage->IsBaseFlagSet(fwEntity::REMOVE_FROM_WORLD) == TRUE || fInterp == 0.0f || IsNextDeleted())
		{
			Load(oMatrix);
		}
		else if (HasNextOffset())
		{
			replayFatalAssertf(false, "TODO");
// 			CPacketCarriageUpdate* pNextPacket = NULL;
// 			GetNextPacket(this, pNextPacket);
// 
// 			replayAssert(pNextPacket->ValidatePacket());
// 
// 			Quaternion rQuatNew;
// 			Vector3 rVecNew;
// 			pNextPacket->FetchQuaternionAndPos(rQuatNew, rVecNew);
// 			Load(oMatrix, rQuatNew, rVecNew, fInterp);
		}
		else
		{
			replayAssert(0 && "CPacketCarriageUpdate::Load - interpolation couldn't find a next packet");
		}

		pCarriage->SetMatrix(oMatrix);

		CGameWorld::Add(pCarriage, CGameWorld::OUTSIDE );

		/*TODO4FIVE CPlane::pPlaneCarriageOnSkyTrack[m_CarriageSlot] = pCarriage;*/
	}
	else
	{
		Printf("CPacketCarriageUpdate::Load: Invalid carriage entity");
	}
}

//=====================================================================================================
void CPacketCarriageUpdate::Load(Matrix34& rMatrix, Quaternion& rQuatNew, Vector3& rVecNew, float fInterp)
{
	Quaternion rQuatPrev;
	DeQuantizeQuaternionS3(rQuatPrev, m_CarriageQuaternion);

	Quaternion rQuatInterp;
	rQuatPrev.PrepareSlerp(rQuatNew);
	rQuatInterp.Slerp(fInterp, rQuatPrev, rQuatNew);

	rMatrix.FromQuaternion(rQuatInterp);
	replayAssert(rMatrix.IsOrthonormal());

	rMatrix.d.x = m_Position[0];
	rMatrix.d.y = m_Position[1];
	rMatrix.d.z = m_Position[2];

	rMatrix.d = rMatrix.d * (1.0f - fInterp) + rVecNew * fInterp;
}

//=====================================================================================================
void CPacketCarriageUpdate::FetchQuaternionAndPos(Quaternion& rQuat, Vector3& rPos)
{
	DeQuantizeQuaternionS3(rQuat, m_CarriageQuaternion);

	rPos.x = m_Position[0];
	rPos.y = m_Position[1];
	rPos.z = m_Position[2];
}

#endif // GTA_REPLAY

