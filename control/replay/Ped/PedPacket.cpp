//=====================================================================================================
// name:		PedPacket.cpp
//=====================================================================================================

#include "PedPacket.h"

// Replay only setup for win32
#if GTA_REPLAY

#include "control/replay/ReplayExtensions.h"
#include "control/replay/ReplayInterfacePed.h"
#include "control/replay/ReplayInterfaceVeh.h"
#include "control/replay/ReplayInternal.h"
#include "control\replay\Ped\BonedataMap.h"

#include "debug/Debug.h"
#include "fwscene/stores/drawablestore.h"
#include "Peds/PlayerPed.h"
#include "scene/portals/portal.h"
#include "scene/world/GameWorld.h"
#include "camera/CamInterface.h"
#include "camera/replay/ReplayDirector.h"
#include "control/gamelogic.h"
#include "ik/IkManager.h"
#include "ik/solvers/RootSlopeFixupSolver.h"
#include "peds/rendering/PedVariationStream.h"
#include "peds/rendering/PedVariationPack.h"
#include "peds/pedhelmetcomponent.h"
#include "peds/PedIntelligence.h"
#include "physics/Floater.h"
#include "task/motion/taskmotionbase.h"
#include "task/Movement/TaskParachute.h"
#include "Peds/rendering/PedOverlay.h"
#include "renderer/PostScan.h"
#include "script/commands_entity.h"

#if REPLAY_DELAYS_QUANTIZATION
tPedDeferredQuaternionBuffer CPacketPedUpdate::sm_DeferredQuaternionBuffer;
#endif // REPLAY_DELAYS_QUANTIZATION

float DeQuantizeU8(u8 iValue, float fMin, float fMax, int iBits);


//////////////////////////////////////////////////////////////////////////
CReplayPedVariationData::CReplayPedVariationData()
{
}


void CReplayPedVariationData::Store(const CPed* pPed)
{
	for(u32 i=0;i<PV_MAX_COMP;i++)
	{
		u32 compIdx = CPedVariationPack::GetCompVar(pPed, static_cast<ePedVarComp>(i));
		m_VariationTexId[i] = CPedVariationPack::GetTexVar(pPed, static_cast<ePedVarComp>(i));

		CPedVariationPack::GetLocalCompData(pPed, (ePedVarComp)i, compIdx, m_VariationHash[i], m_variationData[i]);
	}
}


bool CReplayPedVariationData::AddPreloadRequests(class CReplayModelManager &/*modelManager*/, u32 /*currGameTime*/) const
{
	return true;
}


void CReplayPedVariationData::Print() const
{
	replayDisplayf("CReplayPedVariationData::Print()....");
}

//////////////////////////////////////////////////////////////////////////
void CReplayPedVariationData::Extract(CPed* pPed, const tPacketVersion /*packetVersion*/) const
{
	ePedVarComp variationOrder[] = {
		PV_COMP_HEAD,
		PV_COMP_HAIR,
		PV_COMP_UPPR,
		PV_COMP_LOWR,
		PV_COMP_HAND,
		PV_COMP_FEET,
		PV_COMP_TEEF,
		PV_COMP_ACCS,
		PV_COMP_TASK,
		PV_COMP_DECL,
		PV_COMP_JBIB,
		PV_COMP_BERD,	// Do the BERD last to ensure that hair is hidden - url:bugstar:3586962
	};
	for(u32 i = 0; i < PV_MAX_COMP; ++i)
	{
		ePedVarComp comp = variationOrder[i];
		u32 compIdx = CPedVariationPack::GetGlobalCompData(pPed, comp, m_VariationHash[comp], m_variationData[comp]);
		pPed->SetVariation(comp, compIdx, 0, m_VariationTexId[comp], 0, 0, false);
	}
}

//////////////////////////////////////////////////////////////////////////
void CReplayPedVariationData::Update(u8 component, u32 variationHash, u32 variationData, u8 texId)
{
	m_VariationTexId[component] = texId;
	m_variationData[component] = variationData;
	m_VariationHash[component] = variationHash;
}

//=====================================================================================================

bool CPacketPedUpdate::ShouldStoreHighQuaternion(eAnimBoneTag uBoneTag)
{
	if(CReplayBonedataMap::IsHighPrecisionBone(uBoneTag))
		return true;
	return false;
}

bool CPacketPedUpdate::ShouldStroreTranslations(const rage::crBoneData *pBoneData)
{
	// We only store translations for bones whose translation can change.
	if(pBoneData->GetDofs() & ((u16)rage::crBoneData::TRANSLATE_X | (u16)rage::crBoneData::TRANSLATE_Y | (u16)rage::crBoneData::TRANSLATE_Z))
		return true;
	return false;
}


bool CPacketPedUpdate::ShouldStoreScales(const rage::crBoneData *pBoneData)
{
	if(pBoneData->GetDofs() & ((u16)rage::crBoneData::SCALE_X | (u16)rage::crBoneData::SCALE_Y | (u16)rage::crBoneData::SCALE_Z))
		return true;
	return false;
}


atHashString CPacketPedUpdate::m_ForceHQPedModelTable[] = 
{
	ATSTRINGHASH("Player_Zero",0xd7114c9),
	ATSTRINGHASH("ig_cletus",0xe6631195)
};


bool CPacketPedUpdate::ShouldStoreHightQualityPed(const CPed *pPed)
{
	if(pPed->IsPlayer() || pPed->GetOwnedBy() == ENTITY_OWNEDBY_CUTSCENE)
		return true;

	// Check the table for models that we know should always use HQ bones
	CBaseModelInfo *pPedModelInfo = pPed->GetBaseModelInfo();
	if( pPedModelInfo )
	{
		int nElements = sizeof(m_ForceHQPedModelTable) / sizeof(atHashString);
		for(int i=0; i<nElements; i++)
		{
			atHashString &tableEntry = m_ForceHQPedModelTable[i];
			if( pPedModelInfo->GetModelNameHash() == tableEntry.GetHash() )
			{
				return true;
			}
		}
	}

	return false;
}


// Bone transformation info.
u8 *CPacketPedUpdate::GetBoneDataBasePtr() const
{
	return (u8*)((u8*)this + GetPacketSize() - GetExtraBoneSize() - GET_PACKET_EXTENSION_SIZE(CPacketPedUpdate));
}


// 

tPacketVersion gPacketPedUpdate_v1 = 1;

void CPacketPedUpdate::Store(const CPed* pPed, bool firstUpdatePacket)
{
	// Must be called here...See calls to GetBoneDataBasePtr() which requires extensions to be set up.
	// TODO:- Fold into packet to simplify.
	PACKET_EXTENSION_RESET(CPacketPedUpdate); 
	REPLAY_UNUSEAD_VAR(m_MoveState);

	replayAssert(pPed);

	CPacketBase::Store(PACKETID_PEDUPDATE, sizeof(CPacketPedUpdate), gPacketPedUpdate_v1);
	CPacketInterp::Store();
	CBasicEntityPacketData::Store(pPed, pPed->GetReplayID());

	m_PedType  = (u8)pPed->GetPedType();
	m_bDead    = pPed->IsDead();
	m_bIsMainPlayer = CPedFactory::GetFactory()->GetLocalPlayer() == pPed;// == (CPed*)FindPlayerPed());
	m_MyVehicle  = pPed->GetVehiclePedInside() ? pPed->GetVehiclePedInside()->GetReplayID().ToInt() : -1;
	m_renderScorched = pPed->m_nPhysicalFlags.bRenderScorched;
	m_highQualityPed = ShouldStoreHightQualityPed(pPed);
	SetIsFirstUpdatePacket(firstUpdatePacket);

	if(m_MyVehicle >= 0)
	{
		m_SeatInVehicle = (u8)pPed->GetVehiclePedInside()->GetSeatManager()->GetPedsSeatIndex(pPed);
		m_bIsExitingVehicle = pPed->IsExitingVehicle();

		replayFatalAssertf(pPed->GetVehiclePedInside()->GetReplayID() != ReplayIDInvalid, "Vehicle is not being recorded!");
	}

	m_bStanding  = pPed->GetIsStanding();
	m_bSitting   = pPed->GetIsSitting();
	//TODO4FIVE m_MoveState  = (u8)pPed->GetMoveState();
	m_isVisible		= pPed->IsVisible();	// Should this be keyed off something else?
	
	m_JitterScale = 0;
	if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle) && pPed->GetMyVehicle())
	{
		// Assuming all vehicles have been recorded to this point.
		m_MyVehicle = pPed->GetMyVehicle()->GetReplayID();
		if (pPed->GetMyVehicle()->GetVehicleType() == VEHICLE_TYPE_BIKE)
		{
			if (pPed->GetWindyClothingScale() != 0.0f)
			{
				// Compress into 5 bits
				float fJitterScale = Clamp(pPed->GetWindyClothingScale(), 0.0f, 1.0f);
				m_JitterScale = (u8)(fJitterScale * 31.0f); 
			}
		}
	}

	// Store foot wetness and water information
	for(int i = 0; i < MAX_NUM_FEET; ++i)
	{
		m_footWaterInfo[i]	= static_cast<u8>(pPed->GetPedAudioEntity()->GetFootStepAudio().m_FootWaterInfo[i]);
		m_footWetness[i]	= pPed->GetPedAudioEntity()->GetFootStepAudio().m_FootWetness[i];
	}

	// Store wet clothing data
	m_wetClothingInfo.Store(pPed->GetWetClothingDataForReplay());
	m_wetClothingFlags = pPed->GetWetClothingFlagsForReplay();

	// Props
	m_PropsPacked.Reset();
	
	//this doesn't work unless we check the GetPedPropRenderGfx is valid,
	//it can report props that wouldn't be normally be rendered otherwise
	if(pPed->GetNumProps() > 0)
		m_PropsPacked = CPedPropsMgr::GetPedPropsPacked(pPed);

	replayFatalAssertf(pPed->GetSkeleton(), "No ped skeleton in CPacketPedUpdate::Store");

#if __BANK
	if (CReplayMgrInternal::sm_bUpdateSkeletonsEnabledInRag)
	{
		pPed->GetSkeleton()->Update();
	}
#endif

	StoreBoneMatrixData(pPed);

	m_bHeadInvisible	= pPed->GetPedResetFlag(CPED_RESET_FLAG_MakeHeadInvisible) || camInterface::ComputeShouldMakePedHeadInvisible(*pPed);
	m_bBuoyant			= !pPed->GetIsSwimming() || ComputeIsBuoyant(pPed);
	m_DisableHelmetCullFPS	= pPed->GetPedResetFlag(CPED_RESET_FLAG_DisableHelmetCullFPS);

	m_bIsParachuting = pPed->GetIsParachuting() || pPed->GetPedResetFlag(CPED_RESET_FLAG_IsFalling);
	m_ParachutingState = 0;
	if(m_bIsParachuting)
	{
		CTaskParachute* task = (CTaskParachute *)pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_PARACHUTE);	
		if(task)
		{
			m_ParachutingState = (u8)task->GetState();
		}
	}

	// constructor for struct should have already init values
	if(ReplayReticuleExtension::HasExtension(pPed))
	{
		m_ReticulePacketInfo.m_iWeaponHash = ReplayReticuleExtension::GetWeaponHash(pPed);
		m_ReticulePacketInfo.m_bIsFirstPerson = ReplayReticuleExtension::GetIsFirstPerson(pPed);
		m_ReticulePacketInfo.m_bHit = ReplayReticuleExtension::GetHit(pPed);
		m_ReticulePacketInfo.m_iZoomLevel = ReplayReticuleExtension::GetZoomLevel(pPed);
		m_ReticulePacketInfo.m_bReadyToFire = ReplayReticuleExtension::GetReadyToFire(pPed);
	}

	if(ReplayHUDOverlayExtension::HasExtension(pPed))
	{
		m_hudOverlayPacketInfo.m_allOverlays = ReplayHUDOverlayExtension::GetFlagsValue(pPed);
	}


	m_bSpecialAbilityActive = 0;
	m_bSpecialAbilityFadingOut = 0;
	m_specialAbilityFractionRemaining  = 0;

	const CPlayerSpecialAbility* pSpecialAbility = pPed->GetSpecialAbility();
	if( pSpecialAbility )
	{
		m_bSpecialAbilityActive = pPed->GetSpecialAbility()->IsActive();
		m_bSpecialAbilityFadingOut = pPed->GetSpecialAbility()->IsFadingOut();
		m_specialAbilityFractionRemaining = (u8)(0x3F * pPed->GetSpecialAbility()->GetRemainingNormalized());
	}

	m_Velocity.Store(pPed->GetVelocity());	

	m_IsFiring = pPed->IsFiring();
}


float g_CPacketPedUpdate_RotationEpsilon	= 0.0125f;
float g_CPacketPedUpdate_TranslationEpsilon = 0.0125f;
float g_CPacketPedUpdate_ScaleEpsilon		= 0.01f;

// For peds inside we reduce the "difference from default" threshold to store more accurate frames (fix for B*2198618 - one bad animation essentially)
float g_CPacketPedUpdate_RotationEpsilon_InteriorScale		= 0.5f;
float g_CPacketPedUpdate_TranslationEpsilon_InteriorScale	= 0.75f;

void CPacketPedUpdate::StoreBoneMatrixData(const CPed* pPed)
{
	const rage::crSkeletonData *pSkelData = &pPed->GetSkeletonData();
	m_uBoneCount = (u16)pPed->GetSkeletonData().GetNumBones();

	// Allocate memory on the stack for compressed data.
	tBonePositionType *pPositions = Alloca(tBonePositionType, m_uBoneCount);
	CPacketVector3* pPositionsHigh = Alloca(CPacketVector3, m_uBoneCount);
	CPacketQuaternionH *pRotationsH = Alloca(CPacketQuaternionH, m_uBoneCount);
	CPacketQuaternionL *pRotationsL = Alloca(CPacketQuaternionL, m_uBoneCount);

	u8 *pRotationStoredBitFieldMem = Alloca(u8, CBoneBitField::MemorySize(m_uBoneCount));
	CBoneBitField *pRotationStoredBitField = new (pRotationStoredBitFieldMem) CBoneBitField(m_uBoneCount);

	u8 *pPositionIsHOrLBitFieldMem = Alloca(u8, CBoneBitField::MemorySize(m_uBoneCount));
	CBoneBitField *pPositionIsHOrLBitField = new (pPositionIsHOrLBitFieldMem) CBoneBitField(m_uBoneCount);

	u8 *pRotationIsHOrLBitFieldMem = Alloca(u8, CBoneBitField::MemorySize(m_uBoneCount));
	CBoneBitField *pRotationIsHOrLBitField = new (pRotationIsHOrLBitFieldMem) CBoneBitField(m_uBoneCount);

	u8 *pPositionStoredBitFieldMem = Alloca(u8, CBoneBitField::MemorySize(m_uBoneCount));
	CBoneBitField *pPositionStoredBitField = new (pPositionStoredBitFieldMem) CBoneBitField(m_uBoneCount);


#if REPLAY_DELAYS_QUANTIZATION
	Quaternion *pQuaternionsL = Alloca(Quaternion, m_uBoneCount);
	Quaternion *pQuaternionsH = Alloca(Quaternion, m_uBoneCount);
	tPedDeferredQuaternionBuffer::TempBucket LowTempBucket(m_uBoneCount, pQuaternionsL);
	tPedDeferredQuaternionBuffer::TempBucket HighTempBucket(m_uBoneCount, pQuaternionsH);
#endif // REPLAY_DELAYS_QUANTIZATION
	
	pPed->WaitForAnyActiveAnimUpdateToComplete();

	m_numTranslations = 0;
	m_numTranslationsHigh = 0;
	m_numQuaternionsHigh = 0;
	m_numQuaternions = 0;

	float RotationEpsilon = g_CPacketPedUpdate_RotationEpsilon;
	float TranslationEpsilon = g_CPacketPedUpdate_TranslationEpsilon;

	if(m_highQualityPed == false)
	{
		// Is the ped inside an interior ?
		fwInteriorLocation location = pPed->GetInteriorLocation();

		if(location.IsAttachedToRoom() || location.IsAttachedToPortal() || pPed->PopTypeGet() == POPTYPE_MISSION)
		{
			// Store more accurate anims.
			RotationEpsilon *= g_CPacketPedUpdate_RotationEpsilon_InteriorScale;
			TranslationEpsilon *= g_CPacketPedUpdate_TranslationEpsilon_InteriorScale;
		}

		Vec3V translationEpsilon = Vec3V(TranslationEpsilon, TranslationEpsilon, TranslationEpsilon);

		for (int boneIndex = 0; boneIndex < m_uBoneCount; boneIndex++) 
		{
			const rage::crBoneData *pBoneData = pSkelData->GetBoneData(boneIndex);

			QuatV_ConstRef defaultQ = pBoneData->GetDefaultRotation();
			const Mat34V &localMtx = pPed->GetSkeleton()->GetLocalMtx(boneIndex);
			QuatV localQ = QuatVFromMat34V(localMtx);

	#if REPLAY_DELAYS_QUANTIZATION
			if (BANK_SWITCH(CReplayMgrInternal::sm_bQuantizationOptimizationEnabledInRag, true))
			{
				float delta = Max(fabsf(localQ.GetXf() - defaultQ.GetXf()), fabsf(localQ.GetYf() - defaultQ.GetYf()), fabsf(localQ.GetZf() - defaultQ.GetZf()), fabsf(localQ.GetWf() - defaultQ.GetWf()));
		
				// Is the local rotation too far from the default one of the skeleton ?
				if (delta > RotationEpsilon)
				{
					// Store the local one for use instead of the default.
					eAnimBoneTag uBoneTag = pPed->GetBoneTagFromBoneIndex(boneIndex);
					if(ShouldStoreHighQuaternion(uBoneTag))
					{
						StoreBoneRotationHigh(localQ, pRotationsH[m_numQuaternionsHigh], HighTempBucket);
						pRotationIsHOrLBitField->SetBit(boneIndex);
					}
					else
					{
						StoreBoneRotation(localQ, pRotationsL[m_numQuaternions], LowTempBucket);
					}

					pRotationStoredBitField->SetBit(boneIndex);
				}
			}
			else
	#endif // REPLAY_DELAYS_QUANTIZATION
			{
				eAnimBoneTag uBoneTag = pPed->GetBoneTagFromBoneIndex(boneIndex);
				if(ShouldStoreHighQuaternion(uBoneTag))
				{
					// Quantize the local and default bone rotations.
					CPacketQuaternionH defaultQPacket;
					CPacketQuaternionH localQPacket;
					defaultQPacket.StoreQuaternion(RCC_QUATERNION(defaultQ));
					localQPacket.StoreQuaternion(RCC_QUATERNION(localQ));

					// Does the local packet differ from the default one ?
					if(defaultQPacket != localQPacket)
					{
						// Store the local one.
						pRotationsH[m_numQuaternionsHigh++] = localQPacket;
						pRotationStoredBitField->SetBit(boneIndex);
					}
					pRotationIsHOrLBitField->SetBit(boneIndex);
				}
				else
				{
					// Quantize the local and default bone rotations.
					CPacketQuaternionL defaultQPacket;
					CPacketQuaternionL localQPacket;
					defaultQPacket.StoreQuaternion(RCC_QUATERNION(defaultQ));
					localQPacket.StoreQuaternion(RCC_QUATERNION(localQ));

					// Does the local packet differ from the default one ?
					if(defaultQPacket != localQPacket)
					{
						// Store the local one.
						pRotationsL[m_numQuaternions++] = localQPacket;
						pRotationStoredBitField->SetBit(boneIndex);
					}
				}
			}

			// Can this bone have any translation animation?
			if(ShouldStroreTranslations(pBoneData))
			{
				// Are we to store this translation ?
				if(!IsTranslationCloseToDefault(localMtx.GetCol3ConstRef(), pBoneData->GetDefaultTranslation(), translationEpsilon))
				{
					Vec3V diff = localMtx.GetCol3ConstRef() - pBoneData->GetDefaultTranslation();
					Vec3V diffSqr = diff*diff;
					Vec3V rangeMax(USE_NEW_POSITIONS_RANGE, USE_NEW_POSITIONS_RANGE, USE_NEW_POSITIONS_RANGE);
					Vec3V rangeMaxSqr = rangeMax * rangeMax;

					// Are we to store a high or low precision translation ?
					if (IsLessThanAll(diffSqr, rangeMaxSqr))
					{
						pPositions[m_numTranslations++].Store(RCC_VECTOR3(diff));
					}
					else
					{
						pPositionsHigh[m_numTranslationsHigh++].Store(RCC_VECTOR3(diff));
						pPositionIsHOrLBitField->SetBit(boneIndex);
					}
					pPositionStoredBitField->SetBit(boneIndex);
				}
			}
		}
	}
	else
	{
		for (int boneIndex = 0; boneIndex < m_uBoneCount; boneIndex++) 
		{
			eAnimBoneTag uBoneTag = pPed->GetBoneTagFromBoneIndex(boneIndex);
			const Mat34V &objectMtx = pPed->GetSkeleton()->GetObjectMtx(boneIndex);

			if(ShouldStoreHighQuaternion(uBoneTag))
			{
				StoreBoneRotationHigh(objectMtx, pRotationsH[m_numQuaternionsHigh], HighTempBucket);
				pRotationIsHOrLBitField->SetBit(boneIndex);
			}
			else
			{
				StoreBoneRotation(objectMtx, pRotationsL[m_numQuaternions], LowTempBucket);
			}

			pPositionsHigh[m_numTranslationsHigh++].Store(VEC3V_TO_VECTOR3(objectMtx.GetCol3()));
		}
	}

	AddToPacketSize(GetExtraBoneSize());

	// Write the data into the packet.
	tBonePositionType* pPositionsDest = GetBonePositionsL();
	CPacketQuaternionH* pRotationsHDest	= GetBoneRotationsH();
	CPacketQuaternionL* pRotationsLDest = GetBoneRotationsL();
	CBoneBitField* pRotationStoredBitFieldDest = GetBoneRotationStoredBitField();
	CBoneBitField* pPositionIsHOrLBitFieldDest = GetBonePositionIsHOrLBitField();
	CPacketVector3* pPositionsHighDest = GetBonePositionsH();
	CBoneBitField* pRotationIsHOrLBitFieldDest = GetBoneRotationIsHOrLBitField();
	CBoneBitField* pPositionStoredBitFieldDest = GetBonePositionStoredBitField();

	u32 nofOfRotationsToCopyL = m_numQuaternions;
	u32 nofOfRotationsToCopyH = m_numQuaternionsHigh;

#if REPLAY_DELAYS_QUANTIZATION
	if(BANK_SWITCH(CReplayMgrInternal::sm_bQuantizationOptimizationEnabledInRag, true))
	{
		// Add the quaternions to the deferred buffer + remember where they are kept (see QuantizeQuaternions())!
		pRotationsL[0].Set(sm_DeferredQuaternionBuffer.AddQuaternions(LowTempBucket));
		pRotationsH[0].Set(sm_DeferredQuaternionBuffer.AddQuaternions(HighTempBucket));
		nofOfRotationsToCopyL = 1;
		nofOfRotationsToCopyH = 1;
	}
#endif // REPLAY_DELAYS_QUANTIZATION

	memcpy(pPositionsDest, pPositions, GetBonePositionSize(GetNumTranslations()));
	memcpy(pRotationsHDest, pRotationsH, GetRotationsHSize(nofOfRotationsToCopyH));
	memcpy(pRotationsLDest, pRotationsL, GetRotationsSize(nofOfRotationsToCopyL));
	memcpy(pRotationStoredBitFieldDest, pRotationStoredBitFieldMem, GetBoneBitFieldSize(m_uBoneCount));
	memcpy(pPositionIsHOrLBitFieldDest, pPositionIsHOrLBitField, GetBoneBitFieldSize(m_uBoneCount));
	memcpy(pPositionsHighDest, pPositionsHigh, GetBonePositionHSize(GetNumTranslationsH()));
	memcpy(pRotationIsHOrLBitFieldDest, pRotationIsHOrLBitField, GetBoneBitFieldSize(m_uBoneCount));
	memcpy(pPositionStoredBitFieldDest, pPositionStoredBitField, GetBoneBitFieldSize(m_uBoneCount));
}

bool CPacketPedUpdate::IsTranslationCloseToDefault(Vec3V_ConstRef translation, Vec3V_ConstRef defaultTranslation, Vec3V_ConstRef epsilon)
{
	Vec3V diff = translation - defaultTranslation;
	Vec3V absDiff = Abs(diff);

	if(IsLessThanAll(absDiff, epsilon))
		return true;

	return false;
}

const u32 g_CPacketPedUpdate_Extensions_V1 = 1;
const u32 g_CPacketPedUpdate_Extensions_V2 = 2;
const u32 g_CPacketPedUpdate_Extensions_V3 = 3;
const u32 g_CPacketPedUpdate_Extensions_V4 = 4;
const u32 g_CPacketPedUpdate_Extensions_V5 = 5;
const u32 g_CPacketPedUpdate_Extensions_V6 = 6;
const u32 g_CPacketPedUpdate_Extensions_V7 = 7;
const u32 g_CPacketPedUpdate_Extensions_V8 = 8;
const u32 g_CPacketPedUpdate_Extensions_V9 = 9;
const u32 g_CPacketPedUpdate_Extensions_V10 = 10;
const u32 g_CPacketPedUpdate_Extensions_V11 = 11;

void CPacketPedUpdate::StoreExtensions(CPed* pPed, bool firstUpdatePacket) 
{ 
	PACKET_EXTENSION_RESET(CPacketPedUpdate); 	(void)firstUpdatePacket; 

	PacketExtensions *pExtensions = (PacketExtensions *)BEGIN_PACKET_EXTENSION_WRITE(g_CPacketPedUpdate_Extensions_V11, CPacketPedUpdate);

	// Maintain a pointer for variable size data to be written.
	void *pAfterExtensionHeader = (pExtensions+1);

	//---------------------- Extensions V1 ------------------------/
	pExtensions->offsetToBoneAndScales = (u32)((ptrdiff_t)pAfterExtensionHeader - (ptrdiff_t)pExtensions);
	pAfterExtensionHeader = StoreBoneAndScales(pPed, pExtensions->noOfScales, (BONE_AND_SCALE *)pAfterExtensionHeader);

	//---------------------- Extensions V2 ------------------------/
	CRootSlopeFixupIkSolver* pSolver = static_cast<CRootSlopeFixupIkSolver*>(pPed->GetIkManager().GetSolver(IKManagerSolverTypes::ikSolverTypeRootSlopeFixup));
	pExtensions->isUsingRagdoll = pPed && (pPed->GetUsingRagdoll() || (pSolver&&pSolver->IsActive()));

	//---------------------- Extensions V3 ------------------------/
	pExtensions->aiTasksRunning = pPed->GetReplayRelevantAITaskInfo();

	//---------------------- Extensions V4 ------------------------/
	pExtensions->m_bIsEnteringVehicle = pPed->GetIsEnteringVehicle();
	
	pExtensions->m_unused = 0;

	//---------------------- Extensions V5 ------------------------/
	pAfterExtensionHeader = WriteHeadBlendCumulativeInverseMatrices(pPed, pExtensions, pAfterExtensionHeader);

	//---------------------- Extensions V6 ------------------------/
	int alphaOverrideValue = 0;
	pExtensions->m_hasAlphaOverride = CPostScan::GetAlphaOverride(pPed, alphaOverrideValue);
	Assign(pExtensions->m_alphaOverrideValue, alphaOverrideValue);

	//---------------------- Extensions V8 ------------------------/
	pExtensions->m_heatScaleOverride = pPed->GetHeatScaleOverrideU8();

	//---------------------- Extensions V9 ------------------------/
	pExtensions->m_numDecorations = 0;
	pAfterExtensionHeader = WritePedDecorations(pPed, pExtensions, pAfterExtensionHeader);

	//---------------------- Extensions V10 ------------------------/
	const CPedVariationData& pedVarData = pPed->GetPedDrawHandler().GetVarData();
	pExtensions->m_overrideCrewLogoTxdHash = pedVarData.GetOverrideCrewLogoTxdHash();
	pExtensions->m_overrideCrewLogoTexHash = pedVarData.GetOverrideCrewLogoTexHash();
	if(pExtensions->m_overrideCrewLogoTexHash != 0 && pExtensions->m_overrideCrewLogoTxdHash != 0)
		SetShouldPreload(true);

	//---------------------- Extensions V11 ------------------------/
	pExtensions->m_timeOfDeath = pPed->GetTimeOfDeath();

	END_PACKET_EXTENSION_WRITE(pAfterExtensionHeader, CPacketPedUpdate);
}


void *CPacketPedUpdate::StoreBoneAndScales(const CPed* pPed, u32 &numberStored, BONE_AND_SCALE *pDest)
{
	numberStored = 0;
	const rage::crSkeletonData *pSkelData = &pPed->GetSkeletonData();
	
	Vec3V scaleThreshold(g_CPacketPedUpdate_ScaleEpsilon, g_CPacketPedUpdate_ScaleEpsilon, g_CPacketPedUpdate_ScaleEpsilon);
	Vec3V scaleThresholdSqr = scaleThreshold * scaleThreshold;

	for (int boneIndex = 0; boneIndex < m_uBoneCount; boneIndex++) 
	{
		const Mat34V &objectMtx = pPed->GetSkeleton()->GetObjectMtx(boneIndex);
		const rage::crBoneData *pBoneData = pSkelData->GetBoneData(boneIndex);

		if(ShouldStoreScales(pBoneData))
		{
			ScalarV scaleSqrX = Dot(objectMtx.a(), objectMtx.a());
			ScalarV scaleSqrY = Dot(objectMtx.b(), objectMtx.b());
			ScalarV scaleSqrZ = Dot(objectMtx.c(), objectMtx.c());
			// Might be quicker to just do the square roots!
			Vec3V scaleSqr = Vec3V(scaleSqrX, scaleSqrY, scaleSqrZ);
			Vec3V scaleDefaultSqr = pBoneData->GetDefaultScale()*pBoneData->GetDefaultScale();
			Vec3V diff = scaleSqr - scaleDefaultSqr;

			/*
			Vec3V limitLower = scaleThresholdSqr - limitDelta;
			Vec3V limitUpper = scaleThresholdSqr + limitDelta;
			if(!(IsLessThanAll(diff, limitUpper) && IsGreaterThanAll(diff, limitLower)))
			*/
			Vec3V limitDelta = ScalarV(V_TWO)*pBoneData->GetDefaultScale()*scaleThreshold; 
			Vec3V diffFromEpsilonSqr = Abs(diff - scaleThresholdSqr);
			if(!IsLessThanAll(diffFromEpsilonSqr, limitDelta))
			{
				Vec3V scale = Sqrt(scaleSqr);
				pDest[numberStored].boneIndex = (u16)boneIndex;
				pDest[numberStored].scale[0] = scale.GetXf();
				pDest[numberStored].scale[1] = scale.GetYf();
				pDest[numberStored].scale[2] = scale.GetZf();
				numberStored++;
			}
		}
	}
	return &pDest[numberStored];
}

#if REPLAY_DELAYS_QUANTIZATION

void CPacketPedUpdate::QuantizeQuaternions()
{
#if RSG_BANK
	if (CReplayMgrInternal::sm_bQuantizationOptimizationEnabledInRag)
#endif
	{
		if(m_numQuaternions > 0)
		{
			CPacketQuaternionL* pRotationsL = GetBoneRotationsL();
			sm_DeferredQuaternionBuffer.PackQuaternions(pRotationsL[0].Get(), m_numQuaternions, pRotationsL);
		}

		if(m_numQuaternionsHigh > 0)
		{
			CPacketQuaternionH* pRotationsH = GetBoneRotationsH();
			sm_DeferredQuaternionBuffer.PackQuaternions((u32)pRotationsH[0].Get(), m_numQuaternionsHigh, pRotationsH);
		}
	}
}

void CPacketPedUpdate::ResetDelayedQuaternions()
{
	sm_DeferredQuaternionBuffer.Reset();
}

#endif // REPLAY_DELAYS_QUANTIZATION


//////////////////////////////////////////////////////////////////////////
void CPacketPedUpdate::PreUpdate(const CInterpEventInfo<CPacketPedUpdate, CPed>& info) const
{
	CPed* pPed = info.GetEntity();
	if (pPed)
	{
		if( pPed->IsLocalPlayer() )
		{
			// Stash off our current interior proxy index
			CReplayMgr::SetReplayPlayerInteriorProxyIndex( GetInteriorLocation().GetInteriorProxyIndex() );
		}

		const CPacketPedUpdate* pNextPacket = info.GetNextPacket();
		float interp = info.GetInterp();
		CBasicEntityPacketData::Extract(pPed, (HasNextOffset() && !IsNextDeleted() && pNextPacket) ? pNextPacket : NULL, interp, info.GetPlaybackFlags(), true, IsPedInsideLuxorPlane(pPed));

		if (camInterface::GetReplayDirector().GetActiveCameraTarget()==pPed)
		{
			if(pPed->IsBaseFlagSet(fwEntity::REMOVE_FROM_WORLD) == TRUE || IsNextDeleted())
			{
				pPed->SetIsVisibleForModule(SETISVISIBLE_MODULE_REPLAY, false);
			}
			else if (interp == 0.0f || (IsMainPlayer() && IsDead() && (pNextPacket && !pNextPacket->IsDead())) || GetWarpedThisFrame() || pNextPacket->GetWarpedThisFrame())
			{
				ExtractBoneMatrices(pPed);
			}
			else if (HasNextOffset() && pNextPacket)
			{
				if(m_highQualityPed != pNextPacket->m_highQualityPed)
				{
					replayDebugf1("CPacketPedUpdate::Extract().....frames are different quality level...likely a player change happened");
					ExtractBoneMatrices(pPed);
				}
				else
				{
					ExtractBoneMatrices(pPed, pNextPacket, interp);
				}
			}
			else
			{
				replayAssert(0 && "CPacketPedUpdate::Load - someone pooped the ped interpolation packet");
			}

			// Now that the global bones have been extracted, update the local bones.
			// (needed for camera collision)
			crSkeleton* pSkeleton = pPed->GetSkeleton();
			if(pSkeleton)
			{
				pSkeleton->InverseUpdate();
			}

			if(GET_PACKET_EXTENSION_VERSION(CPacketPedUpdate) >= g_CPacketPedUpdate_Extensions_V2)
			{
				PacketExtensions *pExtensions = (PacketExtensions *)GET_PACKET_EXTENSION_READ_PTR_THIS(CPacketPedUpdate);

				if (pExtensions)
				{
					pPed->SetReplayUsingRagdoll(pExtensions->isUsingRagdoll);

					if(GET_PACKET_EXTENSION_VERSION(CPacketPedUpdate) >= g_CPacketPedUpdate_Extensions_V3)
					{
						pPed->GetReplayRelevantAITaskInfo() = pExtensions->aiTasksRunning;

						if(GET_PACKET_EXTENSION_VERSION(CPacketPedUpdate) >= g_CPacketPedUpdate_Extensions_V4)
						{
							pPed->SetReplayEnteringVehicle(pExtensions->m_bIsEnteringVehicle);
						}
					}
				}
			}
		}		

		//---------------------- Extensions V5 ------------------------/
		ExtractHeadBlendCumulativeInverseMatrices(pPed);

		ExtractExtensions(pPed, info);

		pPed->SetPedResetFlag(CPED_RESET_FLAG_SkipAiUpdateProcessControl, true);
		pPed->RemoveSceneUpdateFlags(CGameWorld::SU_START_ANIM_UPDATE_PRE_PHYSICS | CGameWorld::SU_END_ANIM_UPDATE_PRE_PHYSICS | CGameWorld::SU_START_ANIM_UPDATE_POST_CAMERA);
		pPed->AddSceneUpdateFlags(CGameWorld::SU_UPDATE);
	}
}

int getNumProps(const CPackedPedProps& props)
{
	int count = 0;
	for(int i = 0; i < MAX_PROPS_PER_PED; ++i)
	{
		u32 hash;
		u32 data;
		props.GetPackedPedPropData(i, hash, data);
		if(data != 0 || hash != 0)
			++count;
	}
	return count;
}


bool packedPropsContainsProp(const CPackedPedProps& props, u32 prop)
{
	for(int i = 0; i < MAX_PROPS_PER_PED; ++i)
	{
		u32 hash;
		u32 data;
		props.GetPackedPedPropData(i, hash, data);
		if(data == prop)
			return true;
	}
	return false;
}


bool packedPropsMatchOutOfOrder(const CPackedPedProps& props1, const CPackedPedProps& props2)
{
	if(getNumProps(props1) != getNumProps(props2))
		return false;

	for(int i = 0; i < MAX_PROPS_PER_PED; ++i)
	{
		u32 hash;
		u32 data;
		props1.GetPackedPedPropData(i, hash, data);
		if(data != 0 && !packedPropsContainsProp(props2, data))
			return false;
	}

	for(int i = 0; i < MAX_PROPS_PER_PED; ++i)
	{
		u32 hash;
		u32 data;
		props2.GetPackedPedPropData(i, hash, data);
		if(data != 0 && !packedPropsContainsProp(props1, data))
			return false;
	}

	return true;
}

//=====================================================================================================
void CPacketPedUpdate::Extract(const CInterpEventInfo<CPacketPedUpdate, CPed>& info, CVehicle* pVehicle) const
{
	CPed* pPed = info.GetEntity();
	if (pPed)
	{
		const CPacketPedUpdate* pNextPacket = info.GetNextPacket();
		float interp = info.GetInterp();
		replayFatalAssertf(pPed->GetSkeleton(), "No ped skeleton in CPacketPedUpdate::Store");

		//we want hide the ped and not process the update packet to mimic what happens in the game when peds are removed
		if(pPed->IsBaseFlagSet(fwEntity::REMOVE_FROM_WORLD) == TRUE || IsNextDeleted())
		{
			pPed->SetIsVisibleForModule(SETISVISIBLE_MODULE_REPLAY, false);
		}
		else if (interp == 0.0f || (IsMainPlayer() && IsDead() && (pNextPacket && !pNextPacket->IsDead())) || GetWarpedThisFrame() || pNextPacket->GetWarpedThisFrame())
		{
			ExtractBoneMatrices(pPed);
		}
		else if (HasNextOffset() && pNextPacket)
		{
			if(m_highQualityPed != pNextPacket->m_highQualityPed)
			{
				replayDebugf1("CPacketPedUpdate::Extract().....frames are different quality level...likely a player change happened");
				ExtractBoneMatrices(pPed);
			}
			else
			{
				ExtractBoneMatrices(pPed, pNextPacket, interp);
			}
		}
		else
		{
			replayAssert(0 && "CPacketPedUpdate::Load - someone pooped the ped interpolation packet");
		}

		// Now that the global bones have been extracted, update the local bones.
		// (needed for camera collision)
		crSkeleton* pSkeleton = pPed->GetSkeleton();
		if(pSkeleton)
		{
			pSkeleton->InverseUpdate();
		}

		// Set the foot water and wetness information
		for(int i = 0; i < MAX_NUM_FEET; ++i)
		{
			pPed->GetPedAudioEntity()->GetFootStepAudio().m_FootWaterInfo[i]	= static_cast<audFootWaterInfo>(m_footWaterInfo[i]);
			pPed->GetPedAudioEntity()->GetFootStepAudio().m_FootWetness[i]		= m_footWetness[i];
		}

		// Set wet clothing information.
		Vector4 wetClothingData;
		m_wetClothingInfo.Load(wetClothingData);
		pPed->SetWetClothingDataForReplay(wetClothingData, m_wetClothingFlags);

		// Set dead state for dead peds, but not the player. (so we don't trigger the game's player death fx)
		if (m_bDead && !pPed->IsAPlayerPed())
		{
			// Need to set player health to zero, in addition to setting death state, so IsDead call works correctly.
			pPed->SetHealth(0.0f);
			pPed->SetDeathState(DeathState_Dead);
		}
		else if (!m_bDead && pPed->GetHealth() == 0.0f && pPed->GetDeathState() == DeathState_Dead)
		{
			// Have to undo what we did to dead peds for rewinding.
			pPed->SetHealth(pPed->GetInjuredHealthThreshold());
			pPed->SetDeathState(DeathState_Alive);
		}

 		pPed->SetIsStanding((bool)m_bStanding);
 		pPed->SetIsSitting((bool)m_bSitting);
		pPed->m_nPhysicalFlags.bRenderScorched = m_renderScorched;
		//TODO4FIVE pPed->SetMoveState((eMoveState)m_MoveState);

		ReplayPedExtension* pExtension = ReplayPedExtension::GetExtension(pPed);
		if(!pExtension)
		{
			pExtension = ReplayPedExtension::Add(pPed);
		}
		pExtension->SetIsRecordedVisible(m_isVisible);

		// set wet clothing stuff
		pPed->ProcessBuoyancy((float)fwTimer::GetReplayTime().GetTimeStepInMilliseconds() / 1000.0f, false);

		// vehicle
		ProcessVehicle(pPed, pVehicle, info);
		
		pPed->SetPedResetFlag(CPED_RESET_FLAG_MakeHeadInvisible, m_bHeadInvisible);
		pPed->SetPedResetFlag(CPED_RESET_FLAG_DisableHelmetCullFPS, m_DisableHelmetCullFPS);

		// TODO: add a GTA_REPLAY only reset flag or track in ReplayDirector?
		//pPed->m_PedResetFlags.SetFlag(CPED_RESET_FLAG_PedIsBuoyant, m_bBuoyant);

		if(m_bIsParachuting == 1)
		{
			if(!ReplayParachuteExtension::HasExtension(pPed))
			{
				ReplayParachuteExtension::Add(pPed);
			}

			if(ReplayParachuteExtension::HasExtension(pPed))
			{
				ReplayParachuteExtension::SetIsParachuting(pPed, m_bIsParachuting == 1 ? true : false);
				ReplayParachuteExtension::SetParachutingState(pPed, m_ParachutingState);
			}
		}

		if(GetPacketVersion() >= gPacketPedUpdate_v1)
		{
			pPed->GetPedAudioEntity()->SetWasFiringForReplay(m_IsFiring);
		}

		// reticule information ...mostly for sniper scope
		if (pPed->IsLocalPlayer())
		{
			if(!ReplayReticuleExtension::HasExtension(pPed))
			{
				ReplayReticuleExtension::Add(pPed);
			}

			if(ReplayReticuleExtension::HasExtension(pPed))
			{
				ReplayReticuleExtension::SetWeaponHash(pPed, m_ReticulePacketInfo.m_iWeaponHash);
				ReplayReticuleExtension::SetIsFirstPerson(pPed, m_ReticulePacketInfo.m_bIsFirstPerson);
				ReplayReticuleExtension::SetHit(pPed, m_ReticulePacketInfo.m_bHit);
				ReplayReticuleExtension::SetZoomLevel(pPed, m_ReticulePacketInfo.m_iZoomLevel);
				ReplayReticuleExtension::SetReadyToFire(pPed, m_ReticulePacketInfo.m_bReadyToFire);
			}

			if(!ReplayHUDOverlayExtension::HasExtension(pPed))
			{
				ReplayHUDOverlayExtension::Add(pPed);
			}

			if(ReplayHUDOverlayExtension::HasExtension(pPed))
			{
				ReplayHUDOverlayExtension::SetFlagsValue(pPed, m_hudOverlayPacketInfo.m_allOverlays);
			}
		}

		const CPlayerSpecialAbility* pSpecialAbility = pPed->GetSpecialAbility();
		if( pSpecialAbility )
		{
			pPed->GetSpecialAbility()->OverrideActiveStateForReplay((bool)m_bSpecialAbilityActive);
			pPed->GetSpecialAbility()->OverrideFadingOutForReplay((bool)m_bSpecialAbilityFadingOut);
			pPed->GetSpecialAbility()->OverrideRemainingNormalizedForReplay(m_specialAbilityFractionRemaining/(f32)0x3F);
		}

		if( pPed->GetPedAudioEntity() )
		{
			Vector3 velocity;
			m_Velocity.Load(velocity);
			pPed->GetPedAudioEntity()->SetReplayVelocity(velocity);
		}

		//set the pedprops in the extract to allow them time to stream in
		const CPackedPedProps currentProps = CPedPropsMgr::GetPedPropsPacked(pPed);
		if(m_PropsPacked != currentProps && !packedPropsMatchOutOfOrder(currentProps, m_PropsPacked))
			CPedPropsMgr::SetPedPropsPacked(pPed, m_PropsPacked);		

		ReplayPedExtension *extension = ReplayPedExtension::GetExtension(pPed);
		if(!extension)
		{
			extension = ReplayPedExtension::Add(pPed);
		}
		extension->SetPedShelteredInVehicle(m_ShelteredInVehicle);

		if(extension->GetPedSetForcePin())
			pPed->SetClothForcePin(1);
		extension->SetPedSetForcePin(false);

		if(GET_PACKET_EXTENSION_VERSION(CPacketPedUpdate) >= g_CPacketPedUpdate_Extensions_V2)
		{
			PacketExtensions *pExtensions = (PacketExtensions *)GET_PACKET_EXTENSION_READ_PTR_THIS(CPacketPedUpdate);

			if (pExtensions)
			{
				pPed->SetReplayUsingRagdoll(pExtensions->isUsingRagdoll);

				if(GET_PACKET_EXTENSION_VERSION(CPacketPedUpdate) >= g_CPacketPedUpdate_Extensions_V3)
				{
					pPed->GetReplayRelevantAITaskInfo() = pExtensions->aiTasksRunning;
				}
			}
		}

		ExtractExtensions(pPed, info);
	}
}


//////////////////////////////////////////////////////////////////////////
void CPacketPedUpdate::ExtractExtensions(CPed* pPed, const CInterpEventInfo<CPacketPedUpdate, CPed>& /*eventInfo*/) const
{
	PacketExtensions *pExtensions = (PacketExtensions *)GET_PACKET_EXTENSION_READ_PTR_THIS(CPacketPedUpdate);
	if(GET_PACKET_EXTENSION_VERSION(CPacketPedUpdate) >= g_CPacketPedUpdate_Extensions_V6)
	{
		if(pExtensions->m_hasAlphaOverride)
		{
			int currentAlpha = 0;
			bool IsOnAlphaOverrideList = CPostScan::GetAlphaOverride(pPed, currentAlpha);
			if(!IsOnAlphaOverrideList || currentAlpha != pExtensions->m_alphaOverrideValue)
			{
				bool useSmoothAlpha = !pPed->GetShouldUseScreenDoor();
				entity_commands::CommandSetEntityAlphaEntity(pPed, (int)pExtensions->m_alphaOverrideValue, useSmoothAlpha BANK_PARAM(entity_commands::CMD_OVERRIDE_ALPHA_BY_NETCODE));
			}
		}
		else if(CPostScan::IsOnAlphaOverrideList(pPed))
		{
			entity_commands::CommandResetEntityAlphaEntity(pPed);
		}
	}

	if(GET_PACKET_EXTENSION_VERSION(CPacketPedUpdate) >= g_CPacketPedUpdate_Extensions_V8)
	{
		pPed->SetHeatScaleOverride(pExtensions->m_heatScaleOverride);
	}

	if(GET_PACKET_EXTENSION_VERSION(CPacketPedUpdate) >= g_CPacketPedUpdate_Extensions_V10)
	{
		CPedVariationData& pedVarData = pPed->GetPedDrawHandler().GetVarData();
		if(pExtensions->m_overrideCrewLogoTxdHash == 0 || pExtensions->m_overrideCrewLogoTexHash == 0)
		{
			if(pExtensions->m_overrideCrewLogoTxdHash == 0)
			{
				pedVarData.ClearOverrideCrewLogoTxdHash();
			}
			if(pExtensions->m_overrideCrewLogoTexHash == 0)
			{
				pedVarData.ClearOverrideCrewLogoTexHash();
			}
		}
		else
		{
			strLocalIndex index = g_TxdStore.FindSlot(pExtensions->m_overrideCrewLogoTxdHash);
			if(CStreaming::HasObjectLoaded(index, g_TxdStore.GetStreamingModuleId()))
			{
				pedVarData.SetOverrideCrewLogoTxdHash(pExtensions->m_overrideCrewLogoTxdHash);
				pedVarData.SetOverrideCrewLogoTexHash(pExtensions->m_overrideCrewLogoTexHash);
			}
		}
	}

	if(GET_PACKET_EXTENSION_VERSION(CPacketPedUpdate) >= g_CPacketPedUpdate_Extensions_V11)
	{
		pPed->SetTimeOfDeath(pExtensions->m_timeOfDeath);
	}

	ExtractPedDecorations(pPed);
}


//////////////////////////////////////////////////////////////////////////
bool CPacketPedUpdate::ShouldPreload() const
{
	return GetOverrideCrewLogoTxd() != 0;
}


//////////////////////////////////////////////////////////////////////////
u32	CPacketPedUpdate::GetOverrideCrewLogoTxd() const
{
	if(GET_PACKET_EXTENSION_VERSION(CPacketPedUpdate) >= g_CPacketPedUpdate_Extensions_V10)
	{
		PacketExtensions *pExtensions = (PacketExtensions *)GET_PACKET_EXTENSION_READ_PTR_THIS(CPacketPedUpdate);
		return pExtensions->m_overrideCrewLogoTxdHash;
	}
	return 0;
}


//////////////////////////////////////////////////////////////////////////
#define REPLAYVEHICLEATTACHDEBUG(...) //replayDebugf1(__VA_ARGS__)
void CPacketPedUpdate::ProcessVehicle(CPed* pPed, CVehicle* pVehicle, const CInterpEventInfo<CPacketPedUpdate, CPed>& info) const
{
	const CPacketPedUpdate* pNextPacket = info.GetNextPacket();
	float interp = info.GetInterp();

	if(m_MyVehicle >= 0)
	{
		// If we're rewinding it's actually possible the vehicle could have been delete for 
		// a single frame while the ped things it should be there...
		replayAssertf(pVehicle || info.GetPlaybackFlags().IsSet(REPLAY_DIRECTION_FWD) == false, "Ped can't get into a NULL vehicle...missing vehicle is 0x%08X, current packet is %p, ped is 0x%08X", GetVehicleID().ToInt(), this, GetReplayID().ToInt());

		if(pVehicle)
		{
			if(pPed->GetIsInVehicle() && ((u8)pPed->GetVehiclePedInside()->GetSeatManager()->GetPedsSeatIndex(pPed) != m_SeatInVehicle || pPed->GetMyVehicle() != pVehicle))
			{
				REPLAYVEHICLEATTACHDEBUG("Removing Ped 0x%08X %s from Vehicle 0x%08X (Vehicle or Seat changed)", pPed->GetReplayID().ToInt(), pPed->IsPlayer() ? "(PLAYER)" : "", pPed->GetVehiclePedInside()->GetReplayID().ToInt());
				pPed->SetPedOutOfVehicle(CPed::PVF_Warp | CPed::PVF_DontAllowDetach);
			}

			if (!pPed->GetIsInVehicle())
			{
				if(pPed->GetMyVehicle() != pVehicle)
				{
#if __ASSERT
					CPed* pedCurrentlyInSeat = pVehicle->GetPedInSeat(m_SeatInVehicle);
					replayAssertf(pedCurrentlyInSeat != pPed, "Ped is in vehicle, but doesn't think it is");
#endif

					CPed* pPedInSeat = pVehicle->GetPedInSeat(m_SeatInVehicle);
					if(pPedInSeat != NULL)
					{
						pPedInSeat->SetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle, FALSE );
						pVehicle->RemovePedFromSeat(pPedInSeat);
						pPedInSeat->SetMyVehicle(NULL);
						pPedInSeat->UpdateSpatialArrayTypeFlags();

						REPLAYVEHICLEATTACHDEBUG("Ejecting Ped 0x%08X %s from Vehicle 0x%08X... (Someone else wants vehicle)", pPedInSeat->GetReplayID().ToInt(), pPed->IsPlayer() ? "(PLAYER)" : "", pVehicle->GetReplayID().ToInt());
					}
				}

				pPed->SetPedInVehicle(pVehicle, m_SeatInVehicle, CPed::PVF_Warp);
				pPed->SetAttachCarSeatIndex(m_SeatInVehicle);	
				REPLAYVEHICLEATTACHDEBUG("Attached Ped 0x%08X %s to Vehicle 0x%08X...", pPed->GetReplayID().ToInt(), pPed->IsPlayer() ? "(PLAYER)" : "", pVehicle->GetReplayID().ToInt());
			}
			else if (m_bIsExitingVehicle)
			{
				// Override the attachment system
				CBasicEntityPacketData::Extract(pPed, (HasNextOffset() && !IsNextDeleted() && pNextPacket) ? pNextPacket : NULL, interp, info.GetPlaybackFlags(), true, IsPedInsideLuxorPlane(pPed));
			}
		}
	}
	else if(pPed->GetIsInVehicle())
	{
		REPLAYVEHICLEATTACHDEBUG("Setting Ped 0x%08X %s out of Vehicle 0x%08X...", pPed->GetReplayID().ToInt(), pPed->IsPlayer() ? "(PLAYER)" : "", pPed->GetMyVehicle()->GetReplayID().ToInt());
		pPed->SetPedOutOfVehicle(CPed::PVF_Warp | CPed::PVF_DontAllowDetach);

		// Prevent pop on final frame of animation
		CBasicEntityPacketData::Extract(pPed, (HasNextOffset() && !IsNextDeleted() && pNextPacket) ? pNextPacket : NULL, interp, info.GetPlaybackFlags(), true, IsPedInsideLuxorPlane(pPed));
	}
}



void CPacketPedUpdate::ExtractBoneMatrices(CPed* pPed) const
{
	const rage::crSkeletonData *pSkelData = &pPed->GetSkeletonData();
	const Matrix34 &pedWorldspaceMtx = RCC_MATRIX34(*(pPed->GetSkeleton()->GetParentMtx()));

	CBoneBitField *pRotationStoredBitField = GetBoneRotationStoredBitField();
	CBoneBitField *pRotationIsHOrLBitField = GetBoneRotationIsHOrLBitField();
	CBoneBitField *pPositionStoredBitField = GetBonePositionStoredBitField();
	CBoneBitField *pPositionIsHOrLBitField = GetBonePositionIsHOrLBitField();
	CBoneDataIndexer indexer(GetBonePositionsL(), GetBonePositionsH(), GetBoneRotationsH(), GetBoneRotationsL());

	Matrix34 *pGlobalMatrices = (Matrix34 *)Alloca(Matrix34, m_uBoneCount);

	u16 boneCount = Min(m_uBoneCount, (u16)pSkelData->GetNumBones());
	if(boneCount != m_uBoneCount)
	{
		replayDebugf1("Bone counts in ped (%u) and packet (%d) differ!  This is probably a data-clip mismatch", m_uBoneCount, pSkelData->GetNumBones());
	}

	if(m_highQualityPed == false)
	{
		FormWorlSpaceMatricesFromPacketLocalMatricies(boneCount, pSkelData, pedWorldspaceMtx, pRotationStoredBitField, pRotationIsHOrLBitField, pPositionStoredBitField, pPositionIsHOrLBitField, indexer, pGlobalMatrices);
	}
	else
	{
		for (u16 boneIndex = 0; boneIndex < boneCount; boneIndex++) 
		{
			ASSERT_ONLY(eAnimBoneTag uBoneTag = pPed->GetBoneTagFromBoneIndex(boneIndex));
			replayFatalAssertf(uBoneTag != BONETAG_INVALID, "Bone Tag is invalid");

			Quaternion rot;
			Matrix34 boneMatrix; 
			boneMatrix.Identity();

			// Extract rotation.
			if(pRotationIsHOrLBitField->GetBit(boneIndex))
			{
				indexer.GetNextQuaternionsHigh(rot);
			}
			else
			{
				indexer.GetNextQuaternionsLow(rot);
			}

			boneMatrix.FromQuaternion(rot);
			replayAssert(boneMatrix.IsOrthonormal());

			// Extract translation.
			indexer.GetNextTranslationHigh(boneMatrix.d);

			// Form a world-space matrix.
			pGlobalMatrices[boneIndex].Dot(boneMatrix, pedWorldspaceMtx);
		}
	}

	// Apply scale if it`s stored.
	if(GET_PACKET_EXTENSION_VERSION(CPacketPedUpdate) > 0)
	{
		PacketExtensions *pExtensions = (PacketExtensions *)GET_PACKET_EXTENSION_READ_PTR_THIS(CPacketPedUpdate);
		BONE_AND_SCALE *pBonesAndScales = (BONE_AND_SCALE *)(((u8 *)pExtensions) + pExtensions->offsetToBoneAndScales);
		ApplyStoredScales(pPed, boneCount, pBonesAndScales, pGlobalMatrices);
	}

	for (u16 boneIndex = 0; boneIndex < boneCount; boneIndex++) 
		pPed->SetGlobalMtx((s32)boneIndex, pGlobalMatrices[boneIndex]);
}


#if __BANK
bool CPacketPedUpdate::ms_InterpolateHighQualityInWorlSpace = false;
#endif //__BANK


void CPacketPedUpdate::ExtractBoneMatrices(CPed* pPed, const CPacketPedUpdate *pNextPacket, float interp) const
{
	float oneMinusInterp = 1.0f - interp;
	const rage::crSkeletonData *pSkelData = &pPed->GetSkeletonData();

	CBoneBitField *pRotationStoredBitField = GetBoneRotationStoredBitField();
	CBoneBitField *pRotationIsHOrLBitField = GetBoneRotationIsHOrLBitField();
	CBoneBitField *pPositionStoredBitField = GetBonePositionStoredBitField();
	CBoneBitField *pPositionIsHOrLBitField = GetBonePositionIsHOrLBitField();
	CBoneDataIndexer indexer(GetBonePositionsL(), GetBonePositionsH(), GetBoneRotationsH(), GetBoneRotationsL());

	CBoneBitField *pRotationStoredBitFieldNext = pNextPacket->GetBoneRotationStoredBitField();
	CBoneBitField *pRotationIsHOrLBitFieldNext = pNextPacket->GetBoneRotationIsHOrLBitField();
	CBoneBitField *pPositionStoredBitFieldNext = pNextPacket->GetBonePositionStoredBitField();
	CBoneBitField *pPositionIsHOrLBitFieldNext = pNextPacket->GetBonePositionIsHOrLBitField();
	CBoneDataIndexer indexerNext(pNextPacket->GetBonePositionsL(), pNextPacket->GetBonePositionsH(), pNextPacket->GetBoneRotationsH(), pNextPacket->GetBoneRotationsL());

	u16 boneCount = Min(m_uBoneCount, (u16)pSkelData->GetNumBones());
	if(boneCount != m_uBoneCount)
	{
		replayDebugf1("Bone counts in ped (%u) and packet (%d) differ!  This is probably a data-clip mismatch", m_uBoneCount, pSkelData->GetNumBones());
	}

	// We need to interpolate in world-space. Object space + root matrix is not "smooth" over a transition from rag doll to animated (for example
	// when the player gets up after rolling as a result of exiting a fast moving vehicle).
	Matrix34 pedMatrixPrev;
	Matrix34 pedMatrixNext;
	GetMatrix(pedMatrixPrev);
	pNextPacket->GetMatrix(pedMatrixNext);

	Matrix34 *pGlobalMatricesPrev = (Matrix34 *)Alloca(Matrix34, m_uBoneCount);

	if(m_highQualityPed == false)
	{
		Matrix34 *pGlobalMatricesNext = (Matrix34 *)Alloca(Matrix34, m_uBoneCount);

		FormWorlSpaceMatricesFromPacketLocalMatricies(boneCount, pSkelData, pedMatrixPrev, pRotationStoredBitField, pRotationIsHOrLBitField, pPositionStoredBitField, pPositionIsHOrLBitField, indexer, pGlobalMatricesPrev);
		FormWorlSpaceMatricesFromPacketLocalMatricies(boneCount, pSkelData, pedMatrixNext, pRotationStoredBitFieldNext, pRotationIsHOrLBitFieldNext, pPositionStoredBitFieldNext, pPositionIsHOrLBitFieldNext, indexerNext, pGlobalMatricesNext);

		for (u16 boneIndex = 0; boneIndex < boneCount; boneIndex++) 
		{
			Matrix34 boneMatrix;
			Quaternion rQuatNext;
			Quaternion rQuatPrev; 

			// Interpolate in world-space.
			rQuatPrev.FromMatrix34(pGlobalMatricesPrev[boneIndex]);
			rQuatNext.FromMatrix34(pGlobalMatricesNext[boneIndex]);

			Quaternion rQuatInterp;
			rQuatPrev.PrepareSlerp(rQuatNext);
			rQuatInterp.Slerp(interp, rQuatPrev, rQuatNext);

			// Form the matrix.
			boneMatrix.FromQuaternion(rQuatInterp);
			replayAssert(boneMatrix.IsOrthonormal());
			boneMatrix.d = pGlobalMatricesPrev[boneIndex].d*oneMinusInterp + interp*pGlobalMatricesNext[boneIndex].d;
			boneMatrix.d.w = 1.0f;

			pGlobalMatricesPrev[boneIndex] = boneMatrix;
		}
	}
	else
	{
	#if __BANK
		if(CPacketPedUpdate::ms_InterpolateHighQualityInWorlSpace)
			InterpolateHighQualityInWorldSpace(pPed, interp, boneCount, pRotationIsHOrLBitField, pRotationIsHOrLBitFieldNext, indexer, indexerNext, pedMatrixPrev, pedMatrixNext, pGlobalMatricesPrev);
		else
	#endif //__BANK

		{
			const Matrix34 &pedWorldspaceMtx = RCC_MATRIX34(*(pPed->GetSkeleton()->GetParentMtx()));
			BONE_TRANSFORMATION *pPrevLocalTransformations = (BONE_TRANSFORMATION *)Alloca(BONE_TRANSFORMATION, boneCount);
			BONE_TRANSFORMATION *pNextLocalTransformations = (BONE_TRANSFORMATION *)Alloca(BONE_TRANSFORMATION, boneCount);

			// Build local transformations.
			BuildLocalTransformationsFromObjectMatrices(pPed, boneCount, pRotationIsHOrLBitField, indexer, pedMatrixPrev, pPrevLocalTransformations);
			BuildLocalTransformationsFromObjectMatrices(pPed, boneCount, pRotationIsHOrLBitFieldNext, indexerNext, pedMatrixNext, pNextLocalTransformations);

			// Interpolate in local space and form new worldspace matrices.
			for (u16 boneIndex = 0; boneIndex < boneCount; boneIndex++) 
			{
				const rage::crBoneData *pBoneData = pSkelData->GetBoneData(boneIndex);

				Quaternion rQuatInterp;
				pPrevLocalTransformations[boneIndex].rotation.PrepareSlerp(pNextLocalTransformations[boneIndex].rotation);
				rQuatInterp.Slerp(interp, pPrevLocalTransformations[boneIndex].rotation, pNextLocalTransformations[boneIndex].rotation);

				Matrix34 boneLocalMatrix;
				boneLocalMatrix.FromQuaternion(rQuatInterp);
				boneLocalMatrix.d = oneMinusInterp*pPrevLocalTransformations[boneIndex].translation + interp*pNextLocalTransformations[boneIndex].translation;

				if(pBoneData->GetParent() == NULL)
					pGlobalMatricesPrev[boneIndex].Dot(boneLocalMatrix, pedWorldspaceMtx);
				else
					pGlobalMatricesPrev[boneIndex].Dot(boneLocalMatrix, pGlobalMatricesPrev[pBoneData->GetParent()->GetIndex()]);  // Transform by parent.
			}
		}
	}

	// Apply scale if it`s stored.
	if(GET_PACKET_EXTENSION_VERSION(CPacketPedUpdate) > 0)
	{
		PacketExtensions *pExtensionsPrev = (PacketExtensions *)GET_PACKET_EXTENSION_READ_PTR_THIS(CPacketPedUpdate);
		PacketExtensions *pExtensionsNext = (PacketExtensions *)GET_PACKET_EXTENSION_READ_PTR_OTHER(pNextPacket, CPacketPedUpdate);
		BONE_AND_SCALE *pBonesAndScalesPrev = pExtensionsPrev->noOfScales != 0 ? (BONE_AND_SCALE *)(((u8 *)pExtensionsPrev) + pExtensionsPrev->offsetToBoneAndScales) : nullptr;
		BONE_AND_SCALE *pBonesAndScalesNext = pExtensionsNext->noOfScales != 0 ? (BONE_AND_SCALE *)(((u8 *)pExtensionsNext) + pExtensionsNext->offsetToBoneAndScales) : nullptr;

		if(pBonesAndScalesPrev && pBonesAndScalesNext)
			ApplyStoredScales(pPed, boneCount, pBonesAndScalesPrev, pBonesAndScalesNext, interp, pGlobalMatricesPrev);
		else if(pBonesAndScalesPrev)
			ApplyStoredScales(pPed, boneCount, pBonesAndScalesPrev, pGlobalMatricesPrev);
	}

	for (u16 boneIndex = 0; boneIndex < boneCount; boneIndex++) 
		pPed->SetGlobalMtx((s32)boneIndex, pGlobalMatricesPrev[boneIndex]);

}


void CPacketPedUpdate::BuildLocalTransformationsFromObjectMatrices(CPed *pPed, u16  boneCount, CBoneBitField *pRotationIsHOrLBitField, CBoneDataIndexer &indexer, Matrix34 &pedMatrixFromPacket, BONE_TRANSFORMATION *pLocalTransformations) const
{
	const rage::crSkeletonData *pSkelData = &pPed->GetSkeletonData();
	Matrix34 *pObjectMatrices = (Matrix34 *)Alloca(Matrix34, boneCount);
	const Matrix34 &pedWorldspaceMtx = RCC_MATRIX34(*(pPed->GetSkeleton()->GetParentMtx()));

	for (u16 boneIndex = 0; boneIndex < boneCount; boneIndex++) 
	{
		Quaternion rQuat;
		Matrix34 boneObjectMatrix; 
		Mat34V boneLocalMatrix; 
		ASSERT_ONLY(eAnimBoneTag uBoneTag = pPed->GetBoneTagFromBoneIndex(boneIndex));
		replayFatalAssertf(uBoneTag != BONETAG_INVALID, "Bone Tag is invalid");
		const rage::crBoneData *pBoneData = pSkelData->GetBoneData(boneIndex);

		// Build the object-space matrix,
		if(pRotationIsHOrLBitField->GetBit(boneIndex))
			indexer.GetNextQuaternionsHigh(rQuat);
		else
			indexer.GetNextQuaternionsLow(rQuat);

		boneObjectMatrix.FromQuaternion(rQuat);
		indexer.GetNextTranslationHigh(boneObjectMatrix.d);	
		pObjectMatrices[boneIndex] = boneObjectMatrix;

		if(pBoneData->GetParent() == NULL)
		{
			Matrix34 boneWorldSpaceMatrix;
			// Take care with bones which connect directly to the "pivot" - We need to do this to handle transitions from rag doll to normal animated.
			boneWorldSpaceMatrix.Dot(boneObjectMatrix, pedMatrixFromPacket);
			UnTransformOrtho(boneLocalMatrix, MATRIX34_TO_MAT34V(pedWorldspaceMtx), MATRIX34_TO_MAT34V(boneWorldSpaceMatrix));
		}
		else
		{
			UnTransformOrtho(boneLocalMatrix, MATRIX34_TO_MAT34V(pObjectMatrices[pBoneData->GetParent()->GetIndex()]), MATRIX34_TO_MAT34V(pObjectMatrices[boneIndex]));
		}

		// Convert the local matrix into quaternions for interpolation.
		pLocalTransformations[boneIndex].rotation.FromMatrix34(MAT34V_TO_MATRIX34(boneLocalMatrix));
		pLocalTransformations[boneIndex].translation = VEC3V_TO_VECTOR3(boneLocalMatrix.GetCol3());
	}
}

#if __BANK

// Interpolates high quality ped data in worldspace.
void CPacketPedUpdate::InterpolateHighQualityInWorldSpace(CPed* ASSERT_ONLY(pPed), float interp, u16 boneCount, CBoneBitField *pRotationIsHOrLBitField, CBoneBitField *ASSERT_ONLY(pRotationIsHOrLBitFieldNext), CBoneDataIndexer &indexer, CBoneDataIndexer &indexerNext, Matrix34 &pedMatrixPrev, Matrix34 &pedMatrixNext,  Matrix34 *pDest) const
{
	float oneMinusInterp = 1.0f - interp;

	for (u16 boneIndex = 0; boneIndex < boneCount; boneIndex++) 
	{
		Matrix34 boneMatrix; 
		ASSERT_ONLY(eAnimBoneTag uBoneTag = pPed->GetBoneTagFromBoneIndex(boneIndex));
		replayFatalAssertf(uBoneTag != BONETAG_INVALID, "Bone Tag is invalid");

		// Extract rotation.
		Quaternion rQuatNext;
		Quaternion rQuatPrev;
		replayAssert(pRotationIsHOrLBitField->GetBit(boneIndex) == pRotationIsHOrLBitFieldNext->GetBit(boneIndex));	

		if(pRotationIsHOrLBitField->GetBit(boneIndex))
		{
			indexer.GetNextQuaternionsHigh(rQuatPrev);
			indexerNext.GetNextQuaternionsHigh(rQuatNext);
		}
		else
		{
			indexer.GetNextQuaternionsLow(rQuatPrev);
			indexerNext.GetNextQuaternionsLow(rQuatNext);
		}

		Matrix34 objectMatrixPrev;
		Matrix34 objectMatrixNext;
		Matrix34 worldMatrixPrev;
		Matrix34 worldMatrixNext;

		// Form the world space matrices.
		objectMatrixPrev.FromQuaternion(rQuatPrev);
		indexer.GetNextTranslationHigh(objectMatrixPrev.d);
		worldMatrixPrev.Dot(objectMatrixPrev, pedMatrixPrev);

		objectMatrixNext.FromQuaternion(rQuatNext);
		indexerNext.GetNextTranslationHigh(objectMatrixNext.d);
		worldMatrixNext.Dot(objectMatrixNext, pedMatrixNext);

		// Interpolate in world-space.
		rQuatPrev.FromMatrix34(worldMatrixPrev);
		rQuatNext.FromMatrix34(worldMatrixNext);

		Quaternion rQuatInterp;
		rQuatPrev.PrepareSlerp(rQuatNext);
		rQuatInterp.Slerp(interp, rQuatPrev, rQuatNext);

		// Form the matrix.
		boneMatrix.FromQuaternion(rQuatInterp);
		replayAssert(boneMatrix.IsOrthonormal());
		boneMatrix.d = worldMatrixPrev.d*oneMinusInterp + interp*worldMatrixNext.d;
		boneMatrix.d.w = 1.0f;

		pDest[boneIndex] = boneMatrix;
	}
}

#endif // __BANK

// Forms world-space matrices from a packets data.
void CPacketPedUpdate::FormWorlSpaceMatricesFromPacketLocalMatricies(u16 boneCount, const rage::crSkeletonData *pSkelData, const Matrix34 &pedWorldspaceMtx, CBoneBitField *pRotationStoredBitField, CBoneBitField *pRotationIsHOrLBitField, CBoneBitField *pPositionStoredBitField, CBoneBitField *pPositionIsHOrLBitField, CBoneDataIndexer &indexer, Matrix34 *pDest) const
{
	for (u16 boneIndex = 0; boneIndex < boneCount; boneIndex++) 
	{
		Matrix34 boneMatrix;
		const rage::crBoneData *pBoneData = pSkelData->GetBoneData(boneIndex);
		Quaternion rQuat = RCC_QUATERNION(pBoneData->GetDefaultRotation());

		// Extract rotation.
		if(pRotationStoredBitField->GetBit(boneIndex))
		{
			if(pRotationIsHOrLBitField->GetBit(boneIndex))
				indexer.GetNextQuaternionsHigh(rQuat);
			else
				indexer.GetNextQuaternionsLow(rQuat);
		}

		// Form the local  matrix.
		boneMatrix.FromQuaternion(rQuat);
		replayAssert(boneMatrix.IsOrthonormal());

		// Extract translation.
		if(ShouldStroreTranslations(pBoneData))
		{
			if((pPositionStoredBitField->GetBit(boneIndex)))
			{
				if (pPositionIsHOrLBitField->GetBit(boneIndex))
					indexer.GetNextTranslationHigh(boneMatrix.d);
				else
					indexer.GetNextTranslation(boneMatrix.d);

				boneMatrix.d += RCC_VECTOR3(pBoneData->GetDefaultTranslation());
			}
			else
				boneMatrix.d = RCC_VECTOR3(pBoneData->GetDefaultTranslation());
		}
		else
		{
			boneMatrix.d = RCC_VECTOR3(pBoneData->GetDefaultTranslation());
		}
		boneMatrix.d.w = 1.0f;

		// Form a world-space matrix.
		if(pBoneData->GetParent() == NULL)
			pDest[boneIndex].Dot(boneMatrix, pedWorldspaceMtx);
		else
			pDest[boneIndex].Dot(boneMatrix, pDest[pBoneData->GetParent()->GetIndex()]);  // Transform by parent.
	}
}


void CPacketPedUpdate::ApplyStoredScales(CPed *pPed, u16 boneCount, BONE_AND_SCALE *pBonesAndScales, Matrix34 *pDest) const
{
	const rage::crSkeletonData *pSkelData = &pPed->GetSkeletonData();

	for (u16 boneIndex = 0; boneIndex < boneCount; boneIndex++) 
	{
		const rage::crBoneData *pBoneData = pSkelData->GetBoneData(boneIndex);

		if(ShouldStoreScales(pBoneData))
		{
			Vector3 scale = VEC3V_TO_VECTOR3(pBoneData->GetDefaultScale());

			if(pBonesAndScales->boneIndex == boneIndex)
			{
				scale = Vector3(pBonesAndScales->scale[0], pBonesAndScales->scale[1], pBonesAndScales->scale[2]);
				pBonesAndScales++;
			}
			pDest->a = pDest->a*scale.x;
			pDest->b = pDest->b*scale.y;
			pDest->c = pDest->c*scale.z;
		}
		pDest++;
	}
}


void CPacketPedUpdate::ApplyStoredScales(CPed *pPed, u16 boneCount, BONE_AND_SCALE *pBonesAndScalesPrev, BONE_AND_SCALE *pBonesAndScalesNext, float interp,  Matrix34 *pDest) const
{
	float oneMinusInterp = 1.0f - interp;
	const rage::crSkeletonData *pSkelData = &pPed->GetSkeletonData();

	for (u16 boneIndex = 0; boneIndex < boneCount; boneIndex++) 
	{
		const rage::crBoneData *pBoneData = pSkelData->GetBoneData(boneIndex);

		if(ShouldStoreScales(pBoneData))
		{
			Vector3 scalePrev = VEC3V_TO_VECTOR3(pBoneData->GetDefaultScale());
			Vector3 scaleNext = VEC3V_TO_VECTOR3(pBoneData->GetDefaultScale());

			if(pBonesAndScalesPrev->boneIndex == boneIndex)
			{
				scalePrev = Vector3(pBonesAndScalesPrev->scale[0], pBonesAndScalesPrev->scale[1], pBonesAndScalesPrev->scale[2]);
				pBonesAndScalesPrev++;
			}

			if(pBonesAndScalesNext->boneIndex == boneIndex)
			{
				scaleNext = Vector3(pBonesAndScalesNext->scale[0], pBonesAndScalesNext->scale[1], pBonesAndScalesNext->scale[2]);
				pBonesAndScalesNext++;
			}

			Vector3 scale = oneMinusInterp*scalePrev + interp*scaleNext;
			pDest->a = pDest->a*scale.x;
			pDest->b = pDest->b*scale.y;
			pDest->c = pDest->c*scale.z;
		}
		pDest++;
	}
}


//////////////////////////////////////////////////////////////////////////
bool CPacketPedUpdate::ComputeIsBuoyant(const CPed* pPed) const
{
	//Force camera buoyancy off if the attach ped is swimming under water, as this should be reliable.
	if(!pPed->GetUsingRagdoll() && pPed->GetIsSwimming() && pPed->GetWaterLevelOnPed() >= 1.0f)
	{
		return false;
	}
	else
	{
		return true;
	}
}

//////////////////////////////////////////////////////////////////////////
void CPacketPedUpdate::Print() const
{
	replayDebugf1("Replay ID %d", GetReplayID().ToInt());
}


//////////////////////////////////////////////////////////////////////////
void CPacketPedUpdate::PrintXML(FileHandle handle) const
{
	CPacketBase::PrintXML(handle);

	char str[1024];
	snprintf(str, 1024, "\t\t<ReplayID>0x%08X</ReplayID>\n", GetReplayID().ToInt());
	CFileMgr::Write(handle, str, istrlen(str));

	snprintf(str, 1024, "\t\t<IsFirstUpdate>%s</IsFirstUpdate>\n", GetIsFirstUpdatePacket() == true ? "True" : "False");
	CFileMgr::Write(handle, str, istrlen(str));

	snprintf(str, 1024, "\t\t<IsMainPlayer>%s</IsMainPlayer>\n", m_bIsMainPlayer ? "True" : "False");
	CFileMgr::Write(handle, str, istrlen(str));

	snprintf(str, 1024, "\t\t<MyVehicle>%d</MyVehicle>\n", m_MyVehicle.ToInt());
	CFileMgr::Write(handle, str, istrlen(str));
}


//////////////////////////////////////////////////////////////////////////
u32	CPacketPedUpdate::GetExtraBoneSize() const
{
	u32 boneSize = 0;

	// Same order as in memory (see GetBoneRotationsH() et al).
	boneSize += GetBonePositionSize(m_numTranslations);		// Positions L
	boneSize += GetRotationsHSize(m_numQuaternionsHigh);	// Rotations H
	boneSize += GetRotationsSize(m_numQuaternions);			// Rotation L
	boneSize += GetBoneBitFieldSize(m_uBoneCount);			// Rotation stored
	boneSize += GetBoneBitFieldSize(m_uBoneCount);				// Position stored is H or L
	boneSize += GetBonePositionHSize(m_numTranslationsHigh);	// Positions L
	boneSize += GetBoneBitFieldSize(m_uBoneCount);				// Rotations stored is H or L.
	boneSize += GetBoneBitFieldSize(m_uBoneCount);			// Positions stored.

	// Allow room for alignment.
	boneSize += PED_BONE_DATA_ALIGNMENT;

	return boneSize;
}


//////////////////////////////////////////////////////////////////////////
// Store a bone rotation
void CPacketPedUpdate::StoreBoneRotation(const Mat34V& rMatrix, CPacketQuaternionL& storeLoc, tPedDeferredQuaternionBuffer::TempBucket &tempBucket)
{
	(void) storeLoc;
	(void)tempBucket;
	QuatV localQ = QuatVFromMat34V(rMatrix);
	Quaternion q = QUATV_TO_QUATERNION(localQ);
	q.Normalize();

#if REPLAY_DELAYS_QUANTIZATION
	tempBucket.AddQuaternion(q);
#else
	storeLoc.StoreQuaternion(q);
#endif
	m_numQuaternions++;
}


void CPacketPedUpdate::StoreBoneRotation(const QuatV &rQuat, CPacketQuaternionL& storeLoc, tPedDeferredQuaternionBuffer::TempBucket &tempBucket)
{
	(void) storeLoc;
	(void)tempBucket;
	Quaternion q = QUATV_TO_QUATERNION(rQuat);
	q.Normalize();

#if REPLAY_DELAYS_QUANTIZATION
	tempBucket.AddQuaternion(q);
#else
	storeLoc.StoreQuaternion(q);
#endif
	m_numQuaternions++;
}


//////////////////////////////////////////////////////////////////////////
// Store a bone rotation with Higher precision
void CPacketPedUpdate::StoreBoneRotationHigh(const Mat34V& rMatrix, CPacketQuaternionH& storeLoc, tPedDeferredQuaternionBuffer::TempBucket &tempBucket)
{
	(void) storeLoc;
	(void)tempBucket;
	QuatV localQ = QuatVFromMat34V(rMatrix);
	Quaternion q = QUATV_TO_QUATERNION(localQ);
	q.Normalize();

#if REPLAY_DELAYS_QUANTIZATION
	tempBucket.AddQuaternion(q);
#else
	storeLoc.StoreQuaternion(q);
#endif
	m_numQuaternionsHigh++;
}


void CPacketPedUpdate::StoreBoneRotationHigh(const QuatV &rQuat, CPacketQuaternionH& storeLoc, tPedDeferredQuaternionBuffer::TempBucket &tempBucket)
{
	(void) storeLoc;
	(void)tempBucket;
	Quaternion q = QUATV_TO_QUATERNION(rQuat);
	q.Normalize();

#if REPLAY_DELAYS_QUANTIZATION
	tempBucket.AddQuaternion(q);
#else
	storeLoc.StoreQuaternion(q);
#endif
	m_numQuaternionsHigh++;
}

//////////////////////////////////////////////////////////////////////////
// Load a bone position
void CPacketPedUpdate::ExtractBoneRotation(Matrix34& rMatrix, const CPacketQuaternionL& storeLoc) const
{
	Quaternion q;
	storeLoc.LoadQuaternion(q);
	rMatrix.FromQuaternion(q);
	replayAssert(rMatrix.IsOrthonormal());
}

//////////////////////////////////////////////////////////////////////////
// Load a bone position with Higher precision
void CPacketPedUpdate::ExtractBoneRotationHigh(Matrix34& rMatrix, const CPacketQuaternionH& storeLoc) const
{
	Quaternion q;
	storeLoc.LoadQuaternion(q);
	rMatrix.FromQuaternion(q);
	replayAssert(rMatrix.IsOrthonormal());
}

//////////////////////////////////////////////////////////////////////////
// SLERP a bone rotation from this packet to the next by an interp value
void CPacketPedUpdate::ExtractBoneRotation(Matrix34& rMatrix, const CPacketQuaternionL& storeLoc, const CPacketQuaternionL& storeLocNext, float fInterp) const
{
	Quaternion rQuatNext;
	storeLocNext.LoadQuaternion(rQuatNext);

	Quaternion rQuatPrev;
	storeLoc.LoadQuaternion(rQuatPrev);

	Quaternion rQuatInterp;
	rQuatPrev.PrepareSlerp(rQuatNext);
	rQuatInterp.Slerp(fInterp, rQuatPrev, rQuatNext);

	rMatrix.FromQuaternion(rQuatInterp);
	replayAssert(rMatrix.IsOrthonormal());
}

//////////////////////////////////////////////////////////////////////////
// SLERP a bone rotation (higher precision) from this packet to the next by an interp value
void CPacketPedUpdate::ExtractBoneRotationHigh(Matrix34& rMatrix, const CPacketQuaternionH& storeLoc, const CPacketQuaternionH& storeLocNext, float fInterp) const
{
	Quaternion rQuatNext;
	storeLocNext.LoadQuaternion(rQuatNext);

	Quaternion rQuatPrev;
	storeLoc.LoadQuaternion(rQuatPrev);

	Quaternion rQuatInterp;
	rQuatPrev.PrepareSlerp(rQuatNext);
	rQuatInterp.Slerp(fInterp, rQuatPrev, rQuatNext);

	rMatrix.FromQuaternion(rQuatInterp);
	replayAssert(rMatrix.IsOrthonormal());
}

//////////////////////////////////////////////////////////////////////////
// LERP a bone position from this packet to the next by an interp value
void CPacketPedUpdate::ExtractOnlyPosition(Matrix34& rMatrix, const Vector3& rVecNew, float fInterp, const tBonePositionType& storeLoc) const
{
	storeLoc.Load(rMatrix);

	rMatrix.d = rMatrix.d * (1.0f - fInterp) + rVecNew * fInterp;
}


//////////////////////////////////////////////////////////////////////////
void *CPacketPedUpdate::WriteHeadBlendCumulativeInverseMatrices(CPed *pPed, PacketExtensions *pExtensions, void *pAfterExtensionHeader)
{
	CPacketPositionAndQuaternion20 *pDest = (CPacketPositionAndQuaternion20 *)pAfterExtensionHeader;
	pExtensions->m_noOfCumulativeInvMatrices = 0;
	pExtensions->m_offsetToCumulativeInvMatrices = 0;

	// Only store for MP player heads (single player seems to work ok!). 
	if(pPed->HasHeadBlend() && NetworkInterface::IsGameInProgress())
	{
		if(pPed->GetFragInst() && pPed->GetFragInst()->GetSkeleton())
		{
			crSkeletonData const &skelData = pPed->GetFragInst()->GetSkeleton()->GetSkeletonData();
			Mat34V const *pCumulativeInverseJoints = skelData.GetCumulativeInverseJoints();

			pExtensions->m_offsetToCumulativeInvMatrices = (u32)((ptrdiff_t)pDest - (ptrdiff_t)pExtensions);

			// Copy over the cumulative inverse used to render the head blend with (it's not necessarily from the loaded drawable for MP sessions).
			for(u32 boneIdx = 0; boneIdx < skelData.GetNumBones(); boneIdx++)
			{
				char const *pBoneName = skelData.GetBoneData(boneIdx)->GetName();

				// We're only interested in head related bones.
				if(!strnicmp("FB_", pBoneName, 3))
					pDest[pExtensions->m_noOfCumulativeInvMatrices++].StoreMatrix(MAT34V_TO_MATRIX34(pCumulativeInverseJoints[boneIdx]));
			}
		}
	}
	return &pDest[pExtensions->m_noOfCumulativeInvMatrices];
}


void CPacketPedUpdate::ExtractHeadBlendCumulativeInverseMatrices(CPed *pPed) const
{
	if(GET_PACKET_EXTENSION_VERSION(CPacketPedUpdate) >= g_CPacketPedUpdate_Extensions_V5)
	{
		PacketExtensions *pExtensions = (PacketExtensions *)GET_PACKET_EXTENSION_READ_PTR_THIS(CPacketPedUpdate);

		int count = pExtensions->m_noOfCumulativeInvMatrices;

		// V6 of the extension introduced a bug where we accidentally overwrote the
		// m_noOfCumulativeInvMatrices variable on recording.  In this instance we
		// attempt to calculate how many matrices were stored based on the offset
		// and the end of the extension as a whole.
		if(GET_PACKET_EXTENSION_VERSION(CPacketPedUpdate) == g_CPacketPedUpdate_Extensions_V6 && pExtensions->m_offsetToCumulativeInvMatrices)
		{
			CPacketPositionAndQuaternion20 *pSrc = reinterpret_cast < CPacketPositionAndQuaternion20 * > (((ptrdiff_t)pExtensions + (ptrdiff_t)pExtensions->m_offsetToCumulativeInvMatrices));
			u64 size = (((u64)pExtensions + (u64)GET_PACKET_EXTENSION_SIZE(CPacketPedUpdate) - sizeof(CPacketExtension::PACKET_EXTENSION_GUARD)) - (u64)pSrc);
			count = (int)(size / (u64)sizeof(CPacketPositionAndQuaternion20));
		}

		if(pExtensions && count)
		{
			if(pPed->GetFragInst() && pPed->GetFragInst()->GetSkeleton())
			{
				crSkeletonData const &skelData = pPed->GetFragInst()->GetSkeleton()->GetSkeletonData();
				Mat34V *pDest = const_cast < Mat34V * >(skelData.GetCumulativeInverseJoints());
				CPacketPositionAndQuaternion20 *pSrc = reinterpret_cast < CPacketPositionAndQuaternion20 * > (((ptrdiff_t)pExtensions + (ptrdiff_t)pExtensions->m_offsetToCumulativeInvMatrices));

				for(u32 boneIdx = 0; boneIdx < skelData.GetNumBones(); boneIdx++)
				{
					char const *pBoneName = skelData.GetBoneData(boneIdx)->GetName();

					if(!strnicmp("FB_", pBoneName, 3))
					{
						Matrix34 m;
						pSrc->LoadMatrix(m);
						pDest[boneIdx] = MATRIX34_TO_MAT34V(m);
						pSrc++;
					}
				}
			}
		}
	}
}


void* CPacketPedUpdate::WritePedDecorations(CPed* pPed, CPacketPedUpdate::PacketExtensions* pExtension, void* pAfterExtensionHeader)
{
	pExtension->m_numDecorations = 0;
	pExtension->m_offsetToDecorations = 0;

	if(pPed->IsAPlayerPed())
	{
		pExtension->m_offsetToDecorations = (u32)((u8*)pAfterExtensionHeader - (u8*)pExtension);

		ReplayPedExtension* pPedExt = ReplayPedExtension::GetExtension(pPed);
		if(pPedExt)
		{
			sReplayPedDecorationData* pPedDec = (sReplayPedDecorationData*)pAfterExtensionHeader;
			for(u32 i = 0; i < pPedExt->GetNumPedDecorations(); ++i, ++pPedDec)
			{
				*pPedDec = pPedExt->GetPedDecorationData(i);
				++pExtension->m_numDecorations;
			}
			pAfterExtensionHeader = (void*)pPedDec;
		}
	}

	return pAfterExtensionHeader;
}

void CPacketPedUpdate::ExtractPedDecorations(CPed* pPed) const
{
	if(GET_PACKET_EXTENSION_VERSION(CPacketPedUpdate) >= g_CPacketPedUpdate_Extensions_V9)
	{
		PacketExtensions *pExtensions = (PacketExtensions *)GET_PACKET_EXTENSION_READ_PTR_THIS(CPacketPedUpdate);

		if(pExtensions->m_offsetToDecorations != 0)
		{
			ReplayPedExtension* pPedExt = ReplayPedExtension::GetExtension(pPed);

			bool updateDecorations = false;
			if(pPedExt->GetNumPedDecorations() != pExtensions->m_numDecorations)
				updateDecorations = true;

			if(!updateDecorations)
			{
				sReplayPedDecorationData* pStoreDec = (sReplayPedDecorationData*)((u8*)pExtensions + pExtensions->m_offsetToDecorations);
				for(u32 i = 0; i < pExtensions->m_numDecorations; ++i, ++pStoreDec)
				{
					if(*pStoreDec != pPedExt->GetPedDecorationData(i))
					{	
						updateDecorations = true;
						break;
					}
				}
			}

			if(updateDecorations)
			{
				pPed->ClearDecorations();

				sReplayPedDecorationData* pStoreDec = (sReplayPedDecorationData*)((u8*)pExtensions + pExtensions->m_offsetToDecorations);
				for(u32 i = 0; i < pExtensions->m_numDecorations; ++i, ++pStoreDec)
				{
					if( pStoreDec->compressed )
						PEDDECORATIONMGR.AddCompressedPedDecoration(pPed, pStoreDec->collectionName, pStoreDec->presetName, pStoreDec->crewEmblemVariation);
					else
						PEDDECORATIONMGR.AddPedDecoration(pPed, pStoreDec->collectionName, pStoreDec->presetName, pStoreDec->crewEmblemVariation, pStoreDec->alpha);
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// Hack for for B*2341671.
bool CPacketPedUpdate::IsPedInsideLuxorPlane(CPed *pPed)
{
	CVehicle *pVehicle = pPed->GetVehiclePedInside();
	if(pVehicle && (pVehicle->GetModelNameHash() == ATSTRINGHASH("luxor2", 0xb79f589e) || pVehicle->GetModelNameHash() == ATSTRINGHASH("nimbus", 0xb2cf7250) || pVehicle->GetModelNameHash() == ATSTRINGHASH("VOLATUS", 0x920016f1)))
		return true;
	return false;
}

//////////////////////////////////////////////////////////////////////////
// Constructor needed for the array of these in the interface
CPacketPedCreate::CPacketPedCreate()
: CPacketBase(PACKETID_PEDCREATE, sizeof(CPacketPedCreate))
, CPacketEntityInfo()
, m_FrameCreated(FrameRef::INVALID_FRAME_REF)
, m_PedClanId(RL_INVALID_CLAN_ID)
, m_numPedDecorations(0)
{
}


//////////////////////////////////////////////////////////////////////////
// Store data in a Packet for Creating a Ped
const tPacketVersion CPacketPedCreateExtensions_V1 = 1;
const tPacketVersion CPacketPedCreateExtensions_V2 = 2;

const tPacketVersion gPacketPedCreate_v1 = 1; 

void CPacketPedCreate::Store(const CPed* pPed, bool firstCreationPacket, u16 sessionBlockIndex, eReplayPacketId packetId, tPacketSize packetSize)
{
	(void)sessionBlockIndex;
	PACKET_EXTENSION_RESET(CPacketPedCreate);
	CPacketBase::Store(packetId, packetSize, gPacketPedCreate_v1);
	CPacketEntityInfo::Store(pPed->GetReplayID());

	m_firstCreationPacket = firstCreationPacket;
	SetShouldPreload(firstCreationPacket);

	m_bIsMainPlayer		= (pPed == (CPed*)FindPlayerPed());
	m_ModelNameHash		= pPed->GetArchetype()->GetModelNameHash();

	if(m_bIsMainPlayer)
	{
		g_SpeechManager.GetPlayedPainForReplayVars(AUD_PAIN_VOICE_TYPE_PLAYER, m_NextVariationForPain, m_NumTimesPainPlayed, m_NextTimeToLoadPain, m_CurrentPainBank);
	}

	m_posAndRot.StoreMatrix(MAT34V_TO_MATRIX34(pPed->GetMatrix()));

	m_InitialVariationData.Store(pPed);
	m_LatestVariationData.Store(pPed);
	m_VariationDataVerified = false;

	m_multiplayerHeadFlags = 0;		
	m_numPedDecorations = 0;



	ReplayPedExtension *extension = ReplayPedExtension::GetExtension(pPed);
	if( extension )
	{
		m_multiplayerHeadFlags = extension->GetMultiplayerHeadDataFlags();
		bool hasHeadBlendData = (m_multiplayerHeadFlags & REPLAY_HEAD_BLEND) != 0;
		bool hasHeadOverlayData = (m_multiplayerHeadFlags & REPLAY_HEAD_OVERLAY) != 0;
		bool hasHeadOverlayTintData = (m_multiplayerHeadFlags & REPLAY_HEAD_OVERLAYTINTDATA) != 0;
		bool hasHairTintData = (m_multiplayerHeadFlags & REPLAY_HEAD_HAIRTINTDATA) != 0;

		m_numPedDecorations = extension->GetNumPedDecorations();

		//Adjust the packet size based on the headblenddata and ped decorations
		if( hasHeadBlendData || hasHeadOverlayData )		
			AddToPacketSize(sizeof(sReplayMultiplayerHeadData));
		if( m_numPedDecorations > 0 )
			AddToPacketSize( sizeof(sReplayPedDecorationData) * m_numPedDecorations);

		if( hasHeadOverlayTintData )
			AddToPacketSize(sizeof(sReplayHeadOverlayTintData));
		if( hasHairTintData )
			AddToPacketSize(sizeof(sReplayHairTintData));

		if( hasHeadBlendData || hasHeadOverlayData )
		{
			const sReplayMultiplayerHeadData& headBlendData = extension->GetReplayMultiplayerHeadData();			

			// Write the data into the packet.
			sReplayMultiplayerHeadData* pDest = (sReplayMultiplayerHeadData*)GetHeadBlendDataPtr(sizeof(CPacketPedCreate));
			memcpy(pDest, &headBlendData, sizeof(sReplayMultiplayerHeadData));
		}
		
		sReplayPedDecorationData* pedDecorationData = (sReplayPedDecorationData*)GetPedDecorationDataPtr(sizeof(CPacketPedCreate));
		for(u32 i = 0; i < m_numPedDecorations; i++)
		{
			const sReplayPedDecorationData& decoration = extension->GetPedDecorationData(i);
			memcpy(pedDecorationData, &decoration, sizeof(sReplayPedDecorationData));

			pedDecorationData++;
		}		
		
		//Set ped head blend palette
		for( u8 i = 0; i < MAX_PALETTE_COLORS; i++ )
		{
			u8 r, g, b;
			bool forcePalette;
			bool hasBeenSet;
			extension->GetHeadBlendPaletteColor(i, r, g, b, forcePalette, hasBeenSet);

			m_HeadBlendPaletteData.m_paletteRed[i] = r;
			m_HeadBlendPaletteData.m_paletteGreen[i] = g;
			m_HeadBlendPaletteData.m_paletteBlue[i] = b;
			m_HeadBlendPaletteData.m_forcePalette[i] = forcePalette;
			m_HeadBlendPaletteData.m_paletteSet[i] = hasBeenSet;
		}

		// Additional misc data
		m_eyeColorIndex = extension->GetEyeColor();

		// head overlay tint data
		if( hasHeadOverlayTintData )
		{
			const sReplayHeadOverlayTintData& headOverlayTintData = extension->GetHeadOverlayTintData();			

			// Write the data into the packet.
			sReplayHeadOverlayTintData* pDest = (sReplayHeadOverlayTintData*)GetHeadOverlayTintDataPtr(sizeof(CPacketPedCreate));
			memcpy(pDest, &headOverlayTintData, sizeof(sReplayHeadOverlayTintData));
		}

		// hair tint data
		if( hasHairTintData )
		{
			const sReplayHairTintData& hairTintData = extension->GetHairTintData();			

			// Write the data into the packet.
			sReplayHairTintData* pDest = (sReplayHairTintData*)GetHairTintDataPtr(hasHeadOverlayTintData, sizeof(CPacketPedCreate));
			memcpy(pDest, &hairTintData, sizeof(sReplayHairTintData));
		}
	}
	
	CPedDamageManager::GetClanIdForPed(pPed, m_PedClanId);
}


//////////////////////////////////////////////////////////////////////////
void CPacketPedCreate::StoreExtensions(const CPed* pPed, bool firstCreationPacket, u16 sessionBlockIndex)
{
	(void)firstCreationPacket; (void)sessionBlockIndex; 
	PACKET_EXTENSION_RESET(CPacketPedCreate); 
	PedCreateExtension *pExtension = (PedCreateExtension*)BEGIN_PACKET_EXTENSION_WRITE(CPacketPedCreateExtensions_V2, CPacketPedCreate);

	pExtension->m_parentID = ReplayIDInvalid;
	//---------------------- CPacketPedCreateExtensions_V1 ------------------------/
	if(pPed->GetDynamicComponent() && pPed->GetDynamicComponent()->GetAttachmentExtension() && pPed->GetDynamicComponent()->GetAttachmentExtension()->GetAttachParent())
	{
		pExtension->m_parentID = ((CPhysical*)pPed->GetDynamicComponent()->GetAttachmentExtension()->GetAttachParent())->GetReplayID();
	}

	//---------------------- CPacketPedCreateExtensions_V2 ------------------------/

	pExtension->m_RecordedClassSize = sizeof(CPacketPedCreate);

	END_PACKET_EXTENSION_WRITE(pExtension+1, CPacketPedCreate);
}


//////////////////////////////////////////////////////////////////////////
CReplayID CPacketPedCreate::GetParentID() const
{
	if(GET_PACKET_EXTENSION_VERSION(CPacketPedCreate) >= CPacketPedCreateExtensions_V1)
	{
		PedCreateExtension *pExtension = (PedCreateExtension*)GET_PACKET_EXTENSION_READ_PTR_THIS(CPacketPedCreate);
		return pExtension->m_parentID;
	}
	return ReplayIDInvalid;
}


//////////////////////////////////////////////////////////////////////////
void CPacketPedCreate::Extract(CPed* pPed) const
{
	if( pPed )
	{
		u32 pedCreateClassSize = sizeof(CPacketPedCreate);
		if(GET_PACKET_EXTENSION_VERSION(CPacketPedCreate) >= CPacketPedCreateExtensions_V2)
		{
			PedCreateExtension *pExtensions = (PedCreateExtension *)GET_PACKET_EXTENSION_READ_PTR_THIS(CPacketPedCreate);
			pedCreateClassSize = pExtensions->m_RecordedClassSize;
		}

		pPed->SetPedClanIdForReplay(m_PedClanId);

		bool hasHeadBlendData = (m_multiplayerHeadFlags & REPLAY_HEAD_BLEND) != 0;
		bool hasHeadOverlayData = (m_multiplayerHeadFlags & REPLAY_HEAD_OVERLAY) != 0;
		bool hasMicroMorphData = (m_multiplayerHeadFlags & REPLAY_HEAD_MICROMORPH) != 0;
		bool hasHeadOverlayTintData = (m_multiplayerHeadFlags & REPLAY_HEAD_OVERLAYTINTDATA) != 0;
		bool hasHairTintData = (m_multiplayerHeadFlags & REPLAY_HEAD_HAIRTINTDATA) != 0;		
		
		sReplayMultiplayerHeadData headBlendData;
		
		if( GetPacketVersion() >= gPacketPedCreate_v1 )
		{
			sReplayMultiplayerHeadData *headBlendDataPtr = (sReplayMultiplayerHeadData*)GetHeadBlendDataPtr(pedCreateClassSize);
			memcpy(&headBlendData, headBlendDataPtr, sizeof(sReplayMultiplayerHeadData));			
		}
		else
		{
			//Old Version - copy from the old structure into the new one, only difference is the micromorphs are u8 in the old
			//dequantise and convert to the new float structure.
			sReplayMultiplayerHeadData_OLD* headBlendData_old = (sReplayMultiplayerHeadData_OLD*)GetHeadBlendDataPtr(pedCreateClassSize);
			headBlendData.m_pedHeadBlendData = headBlendData_old->m_pedHeadBlendData;
			for( u32 i = 0; i < HOS_MAX; i++ )
			{
				headBlendData.m_overlayAlpha[i] = headBlendData_old->m_overlayAlpha[i];
				headBlendData.m_overlayNormAlpha[i] = headBlendData_old->m_overlayNormAlpha[i];
				headBlendData.m_overlayTex[i] = headBlendData_old->m_overlayTex[i];
			}
			for( u32 slot = 0; slot < MMT_MAX; slot++ )
			{				
				headBlendData.m_microMorphBlends[slot] = DeQuantizeU8(headBlendData_old->m_microMorphBlends[slot], 0.0, 1.0, 8);
			}
		}
		
		if( hasHeadBlendData )
		{
			CPedHeadBlendData blendData;
			blendData.m_head0 = (u8)headBlendData.m_pedHeadBlendData.m_head0;
			blendData.m_head1 = (u8)headBlendData.m_pedHeadBlendData.m_head1;
			blendData.m_head2 = (u8)headBlendData.m_pedHeadBlendData.m_head2;
			blendData.m_tex0 = (u8)headBlendData.m_pedHeadBlendData.m_tex0;
			blendData.m_tex1 = (u8)headBlendData.m_pedHeadBlendData.m_tex1;
			blendData.m_tex2 = (u8)headBlendData.m_pedHeadBlendData.m_tex2;
			blendData.m_headBlend = headBlendData.m_pedHeadBlendData.m_headBlend;
			blendData.m_texBlend = headBlendData.m_pedHeadBlendData.m_texBlend;
			blendData.m_varBlend = headBlendData.m_pedHeadBlendData.m_varBlend;
			blendData.m_isParent = headBlendData.m_pedHeadBlendData.m_isParent;
			pPed->SetHeadBlendData(blendData);
		}

		if( hasHeadOverlayData )
		{
			for( u32 slot = 0; slot < HOS_MAX; slot++ )
			{
				float overlayAlpha = DeQuantizeU8(headBlendData.m_overlayAlpha[slot], 0.0, 1.0, 8);
				float overlayNormAlpha = DeQuantizeU8(headBlendData.m_overlayNormAlpha[slot], 0.0, 1.0, 8);
				pPed->SetHeadOverlay(static_cast<eHeadOverlaySlots>(slot), headBlendData.m_overlayTex[slot], overlayAlpha, overlayNormAlpha);
			}
		}

		if( hasMicroMorphData )
		{
			for( u32 slot = 0; slot < MMT_MAX; slot++ )
			{
				float microMorph = headBlendData.m_microMorphBlends[slot];
				pPed->MicroMorph(static_cast<eMicroMorphType>(slot), microMorph);
			}
		}

		sReplayPedDecorationData* pedDecorationData = (sReplayPedDecorationData*)GetPedDecorationDataPtr(pedCreateClassSize);	
		for( u32 i = 0; i < m_numPedDecorations; i++ )
		{		
			if( pedDecorationData->compressed )
				PEDDECORATIONMGR.AddCompressedPedDecoration(pPed, pedDecorationData->collectionName, pedDecorationData->presetName, pedDecorationData->crewEmblemVariation);
			else
				PEDDECORATIONMGR.AddPedDecoration(pPed, pedDecorationData->collectionName, pedDecorationData->presetName, pedDecorationData->crewEmblemVariation, pedDecorationData->alpha);

			pedDecorationData++;
		}

		//Store the head blend data that requires a valid head blend handle in the replay extension.
		ReplayPedExtension *extension = ReplayPedExtension::GetExtension(pPed);
		if(!extension)
		{
			extension = ReplayPedExtension::Add(pPed);
			if(!extension)
			{
				replayAssertf(false, "Failed to add a pPed extension...ran out?");
				return;
			}
			extension->Reset();
		}

		for( u8 i = 0; i < MAX_PALETTE_COLORS; i++ )
		{			
			if( m_HeadBlendPaletteData.m_paletteSet[i] )
			{
				extension->SetHeadBlendDataPaletteColor(pPed, m_HeadBlendPaletteData.m_paletteRed[i], m_HeadBlendPaletteData.m_paletteGreen[i], m_HeadBlendPaletteData.m_paletteBlue[i], i, m_HeadBlendPaletteData.m_forcePalette[i]);
			}
		}	

		if( hasHeadOverlayTintData )
		{
			sReplayHeadOverlayTintData* headTintData = (sReplayHeadOverlayTintData*)GetHeadOverlayTintDataPtr(pedCreateClassSize);
			for( u32 i = 0; i < HOS_MAX; i++ )
			{				
				extension->SetHeadOverlayTintData(pPed, (eHeadOverlaySlots)i, (eRampType)headTintData->m_overlayRampType[i], headTintData->m_overlayTintIndex[i], headTintData->m_overlayTintIndex2[i]);
			}
		}

		if( hasHairTintData )
		{
			sReplayHairTintData* hairTintData = (sReplayHairTintData*)GetHairTintDataPtr(hasHeadOverlayTintData, pedCreateClassSize);		
			extension->SetHairTintIndex(pPed, hairTintData->m_HairTintIndex, hairTintData->m_HairTintIndex2);
		}	

		if( m_eyeColorIndex != -1 )
		{
			extension->SetEyeColor(pPed, m_eyeColorIndex);
		}

		if( hasHeadBlendData)
			pPed->FinalizeHeadBlend();
	}
}

u8* CPacketPedCreate::GetHeadBlendDataPtr(u32 pedCreateClassSize) const
{
	return ((u8*)this + pedCreateClassSize);
}

u8* CPacketPedCreate::GetPedDecorationDataPtr(u32 pedCreateClassSize) const
{
	bool hasheadBlendData = (m_multiplayerHeadFlags & REPLAY_HEAD_BLEND ) || (m_multiplayerHeadFlags & REPLAY_HEAD_OVERLAY );
	u32 headBlendDataSize = sizeof(sReplayMultiplayerHeadData);
	if( GetPacketVersion() < gPacketPedCreate_v1 )
		headBlendDataSize = sizeof(sReplayMultiplayerHeadData_OLD);

	return (GetHeadBlendDataPtr(pedCreateClassSize) + (headBlendDataSize * hasheadBlendData));
}

u8* CPacketPedCreate::GetHeadOverlayTintDataPtr(u32 pedCreateClassSize) const
{
	return (GetPedDecorationDataPtr(pedCreateClassSize) + (sizeof(sReplayPedDecorationData) * m_numPedDecorations));
}

u8* CPacketPedCreate::GetHairTintDataPtr(bool hasHeadOverlayData, u32 pedCreateClassSize) const
{
	return (GetHeadOverlayTintDataPtr(pedCreateClassSize) + (sizeof(sReplayHeadOverlayTintData) * hasHeadOverlayData));
}


//////////////////////////////////////////////////////////////////////////
void CPacketPedCreate::PrintXML(FileHandle handle) const
{
	CPacketBase::PrintXML(handle);
	CPacketEntityInfo::PrintXML(handle);

	char str[1024];
	snprintf(str, 1024, "\t\t<CreationFrame>%u : %u</CreationFrame>\n", GetFrameCreated().GetBlockIndex(), GetFrameCreated().GetFrame());
	CFileMgr::Write(handle, str, istrlen(str));

	if(IsMainPlayer())
	{
		snprintf(str, 1024, "\t\t<IsMainPlayer>true</IsMainPlayer>\n");
		CFileMgr::Write(handle, str, istrlen(str));
	}
}


//////////////////////////////////////////////////////////////////////////
// Store data in a Packet for Deleting a Ped
void CPacketPedDelete::Store(const CPed* pPed, const CPacketPedCreate* pLastCreatedPacket)
{
	//clone our last know good creation packet and setup the correct new ID and size
	CloneBase<CPacketPedCreate>(pLastCreatedPacket);
	
	//We need to use the version from the create packet to version the delete packet as it inherits from it.
	CPacketBase::Store(PACKETID_PEDDELETE, sizeof(CPacketPedDelete), pLastCreatedPacket->GetPacketVersion());

	//The create packet has extra variable sized data on the end which we need to copy onto the end of the delete packet.
	//Find the difference in size between the last create packet and the (CPacketPedCreate - extension data) and add it onto the delete packet size. 
	s32 extraDataSize = pLastCreatedPacket->GetPacketSize() - sizeof(CPacketPedCreate) - GET_PACKET_EXTENSION_SIZE_OTHER(pLastCreatedPacket, CPacketPedCreate);
	Assert(extraDataSize >= 0);
	if( extraDataSize > 0 )
	{
		AddToPacketSize((tPacketSize)extraDataSize);	
		//Memcpy the data from the end of the create packet to the end of the delete packet.
		sysMemCpy((u8*)this + sizeof(CPacketPedDelete), (u8*)pLastCreatedPacket + sizeof(CPacketPedCreate), extraDataSize);
	}

	FrameRef creationFrame;
	pPed->GetCreationFrameRef(creationFrame);
	SetFrameCreated(creationFrame);

	replayFatalAssertf(GetPacketSize() <= MAX_PED_DELETION_SIZE, "Size of Ped Deletion packet is too large! (%u)", GetPacketSize());
}

void CPacketPedDelete::StoreExtensions(const CPed*, const CPacketPedCreate* pLastCreatedPacket)
{ 
	CPacketPedDelete::CloneExtensionData(pLastCreatedPacket);

	//We store the original create packet class size in the class so we know how far to offset to the end
	//change this to the delete packet size so we offset to the end of the delete packet where the variable sized data is.
	PedCreateExtension *pExtension = (PedCreateExtension *)GET_PACKET_EXTENSION_READ_PTR_OTHER((CPacketPedCreate*)this, CPacketPedCreate);
	pExtension->m_RecordedClassSize = sizeof(CPacketPedDelete);
	PACKET_EXTENSION_RECOMPUTE_DATA_CRC(CPacketPedCreate);

	PACKET_EXTENSION_RESET(CPacketPedDelete);
}

//////////////////////////////////////////////////////////////////////////
void CPacketPedDelete::PrintXML(FileHandle handle) const
{
	CPacketBase::PrintXML(handle);

	char str[1024];
	snprintf(str, 1024, "\t\t<ReplayID>0x%08X</ReplayID>\n", GetReplayID().ToInt());
	CFileMgr::Write(handle, str, istrlen(str));
}



//////////////////////////////////////////////////////////////////////////
void CPedInterp::Init(const ReplayController& controller, CPacketPedUpdate const* pPrevPacket)
{
	// Setup the previous packet
	SetPrevPacket(pPrevPacket);

	CPacketPedUpdate const* pPedPrevPacket = GetPrevPacket();
	m_sPrevFullPacketSize += pPedPrevPacket->GetPacketSize();

	// Look for/Setup the next packet
	if (pPrevPacket->IsNextDeleted())
	{
		m_sNextFullPacketSize = 0;
		SetNextPacket(NULL);
	}
	else if (pPrevPacket->HasNextOffset())
	{
		CPacketPedUpdate const* pNextPacket = NULL;
		controller.GetNextPacket(pPrevPacket, pNextPacket);
		SetNextPacket(pNextPacket);

		CPacketPedUpdate const* pPedNextPacket = GetNextPacket();
#if __ASSERT
		CPacketPedUpdate const* pPedPrevPacket = GetPrevPacket();
		replayAssert(pPedPrevPacket->GetReplayID() == pPedNextPacket->GetReplayID());
		replayAssert(pPedPrevPacket->GetBoneCount() == pPedNextPacket->GetBoneCount());
#endif // __ASSERT

		m_sNextFullPacketSize += pPedNextPacket->GetPacketSize();
	}
	else
	{
		replayAssertf(false, "CPedInterp::Init");
	}
}


//////////////////////////////////////////////////////////////////////////
void CPedInterp::Reset()
{
	CInterpolator<CPacketPedUpdate>::Reset();

	m_sPrevFullPacketSize = 0;
	m_sNextFullPacketSize = 0;
}

//////////////////////////////////////////////////////////////////////////
// Ped outfit change

CPacketPedVariationChange::CPacketPedVariationChange(CPed* pPed, u8 component, u32 oldVariationHash, u32 oldVarationData, u8 oldTexId)
	: CPacketEvent(PACKETID_PEDVARIATIONCHANGE, sizeof(CPacketPedVariationChange))
	, CPacketEntityInfo()
{
	m_component = component;

	//make sure we update the latest value of this in the ped create packet
	iReplayInterface* pI = CReplayMgrInternal::GetReplayInterface("CPed");
	replayFatalAssertf(pI, "Interface is NULL");
	CReplayInterfacePed* pPedInterface = verify_cast<CReplayInterfacePed*>(pI);
	replayFatalAssertf(pPedInterface, "Incorrect interface");

	CPacketPedCreate* pCreatePacket = pPedInterface->GetCreatePacket(pPed->GetReplayID());

	u32 compIdx = CPedVariationPack::GetCompVar(pPed, static_cast<ePedVarComp>(component));
	u8 texId = CPedVariationPack::GetTexVar(pPed, static_cast<ePedVarComp>(component));
	u32 variationHash;
	u32 variationData;
	CPedVariationPack::GetLocalCompData(pPed, (ePedVarComp)component, compIdx, variationHash, variationData);


	if(pCreatePacket != NULL)
		pCreatePacket->GetLatestVariationData()->Update(component, variationHash, variationData, texId);

	m_prevValues.m_variationHash	= oldVariationHash;
	m_prevValues.m_variationData	= oldVarationData;
	m_prevValues.m_texId			= oldTexId;

	m_nextValues.m_variationHash	= variationHash;
	m_nextValues.m_variationData	= variationData;
	m_nextValues.m_texId			= texId;	
}

//////////////////////////////////////////////////////////////////////////
void CPacketPedVariationChange::Extract(const CEventInfo<CPed>& eventInfo) const
{
	CPed* pPed = eventInfo.GetEntity();
	if(!pPed)
		return;

	varData const* var = &m_nextValues;
	if(eventInfo.GetPlaybackFlags().GetState() & REPLAY_DIRECTION_BACK)
	{
		var = &m_prevValues;	

		// Fix for url:bugstar:2812258...
		// The game sometimes sets a variation twice in the same frame which means our reverse strategy doesn't work.
		// If we're reversing and the previous and next are the same then this has happened and we should ignore it completely
		u32 prevCompIdx = CPedVariationPack::GetGlobalCompData(pPed, (ePedVarComp)m_component, m_prevValues.m_variationHash, m_prevValues.m_variationData);
		u32 nextCompIdx = CPedVariationPack::GetGlobalCompData(pPed, (ePedVarComp)m_component, m_nextValues.m_variationHash, m_nextValues.m_variationData);
		if(prevCompIdx == nextCompIdx && m_prevValues.m_texId == m_nextValues.m_texId)
		{
			return;
		}

		// Here we only allow the first packet in a frame to rewind properly.
		// For example if we have 2 packets that have prev and next values of 0, 1 and 1, 2
		// then without this check on rewind we'd set 0 from the first packet and then 1 from the second
		// resulting in the wrong value being set going backwards.
		ReplayPedExtension* pPedExtension = ReplayPedExtension::GetExtension(pPed);
		if(pPedExtension)
		{
			if(pPedExtension->IsRewindVariationSet(m_component))
				return;

			pPedExtension->RewindVariationSet(m_component);
		}
	}

	u32 compIdx = CPedVariationPack::GetGlobalCompData(pPed, (ePedVarComp)m_component, var->m_variationHash, var->m_variationData);
	pPed->SetVariation(static_cast<ePedVarComp>(m_component), compIdx, 0, var->m_texId, 0, 0, false);
}

//Micro morph change

CPacketPedMicroMorphChange::CPacketPedMicroMorphChange(u32 type, float blend, float previousMicroMorphBlend)
	: CPacketEvent(PACKETID_PEDMICROMORPHCHANGE, sizeof(CPacketPedMicroMorphChange))
	, CPacketEntityInfo()
	, m_type(type)
	, m_blend(blend)
	, m_previousBlend(previousMicroMorphBlend)
{
}

//////////////////////////////////////////////////////////////////////////
void CPacketPedMicroMorphChange::Extract(const CEventInfo<CPed>& eventInfo) const
{
	CPed* pPed = eventInfo.GetEntity();
	if(!pPed)
		return;

	float blend = m_blend;

	if(eventInfo.GetPlaybackFlags().GetState() & REPLAY_DIRECTION_BACK)
	{
		blend = m_previousBlend;	
	}

	pPed->MicroMorph(static_cast<eMicroMorphType>(m_type), blend);
}

//Head Blend change

CPacketPedHeadBlendChange::CPacketPedHeadBlendChange(const sReplayHeadPedBlendData& replayData, const sReplayHeadPedBlendData& previousReplayData)
	: CPacketEvent(PACKETID_PEDHEADBLENDCHANGE, sizeof(CPacketPedHeadBlendChange))
	, CPacketEntityInfo()
{	
	m_data = replayData;
	m_previousData = previousReplayData;
}

//////////////////////////////////////////////////////////////////////////
void CPacketPedHeadBlendChange::Extract(const CEventInfo<CPed>& eventInfo) const
{
	CPed* pPed = eventInfo.GetEntity();
	if(!pPed)
		return;

	CPedHeadBlendData blendData;

	const sReplayHeadPedBlendData *newBlendData = &m_data;
	if(eventInfo.GetPlaybackFlags().GetState() & REPLAY_DIRECTION_BACK)
	{
		newBlendData = &m_previousData;	
	}

	blendData.m_head0 = (u8)newBlendData->m_head0;
	blendData.m_head1 = (u8)newBlendData->m_head1;
	blendData.m_head2 = (u8)newBlendData->m_head2;
	blendData.m_tex0 = (u8)newBlendData->m_tex0;
	blendData.m_tex1 = (u8)newBlendData->m_tex1;
	blendData.m_tex2 = (u8)newBlendData->m_tex2;
	blendData.m_headBlend = newBlendData->m_headBlend;
	blendData.m_texBlend = newBlendData->m_texBlend;
	blendData.m_varBlend = newBlendData->m_varBlend;
	blendData.m_isParent = newBlendData->m_isParent;

	pPed->SetHeadBlendData(blendData);
}

#endif // GTA_REPLAY
