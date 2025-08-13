
#include "phbound/boundcapsule.h"
#include "phbound/boundcomposite.h"
#include "fwpheffects/ropedatamanager.h"
#include "fwpheffects/ropemanager.h"

#include "Peds\Horse\horseComponent.h"
#include "Peds\Horse\horseTune.h"
#include "Peds\ped.h"
#include "Peds\PedIntelligence.h"
#include "physics/Rope.h"
#include "task/Motion/TaskMotionBase.h"
#include "task/Response/TaskFlee.h"
#include "Peds/PedDebugVisualiser.h"
#if ENABLE_HORSE

#include "Peds/PedIntelligence.h"
#include "Event/Events.h"

#include "horseComponent_parser.h"

AI_OPTIMISATIONS()

namespace rage
{
	EXT_PFD_DECLARE_ITEM(Ropes);
}

// CReins


const char* CReins::sm_tuneFilePath = "common:/data/cloth/";
CReins defaultReins;

CReins* CReins::GetDefaultReins()
{
	return &defaultReins;
}

bool CReins::Init()
{
	ropeDataManager::LoadRopeTexures();

	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	Assert( pRopeManager );

	const float minLength = m_Length;
	const float maxLength = m_Length;
	const float lengthChangeRate = 1.0f;		// default

	Vec3V _zero(V_ZERO);
	Assertf( !m_RopeInstance, "More than one horse component is using the default reins!" );

	const float reinsWeight = 2.5f;
	m_RopeInstance = pRopeManager->AddRope( _zero, _zero, m_Length, minLength, maxLength, lengthChangeRate, ROPE_REINS, m_NumSections, false /*ppuOnly*/, false /*lockFromFront*/, reinsWeight /*weightScale*/, false /*breakable*/, false /*pinned*/);
	Assertf( m_RopeInstance, "Reins failed to get a rope instance. Increase the rope count (or decrease the number of reins)." );

	if(!m_RopeInstance)
	{
		return false;
	}

	m_RopeInstance->SetNewUniqueId();
	m_RopeInstance->SetCustomCallback( datCallback(MFA(CReins::Update), this) );
	m_RopeInstance->SetUpdateOrder(ROPE_UPDATE_LATE);

	const int numVerts = m_NumSections+1;
	m_BindPos.Resize(numVerts);
	m_BindPos2.Resize(numVerts);
	m_SoftPinRadiuses.Resize(numVerts);

	return true;
}


CReins::~CReins()
{
	Assert( m_RopeInstance );
	if (m_RopeInstance)
	{
		m_RopeInstance->DetachVirtualBound();
		CPhysics::GetRopeManager()->RemoveRope(m_RopeInstance);
	}
#if __BANK
	if( m_RAGGroup )
	{
		bkBank* bank = ropeBankManager::GetBank();
		Assert( bank );
		bank->DeleteGroup( *m_RAGGroup );
	}
#endif

	ropeDataManager::UnloadRopeTexures();
	m_CompositeBoundPtr = NULL;
}

class CHorseReinsFileMounter : public CDataFileMountInterface
{
	virtual bool LoadDataFile(const CDataFileMgr::DataFile & file)
	{
		//! Replace the file in the mount code. This allows us to replace the file we accidentally shipped on disc.
		CReins::GetDefaultReins()->Reload(file.m_filename);
		return true;
	}

	virtual void UnloadDataFile(const CDataFileMgr::DataFile & UNUSED_PARAM(file)) 
	{
	}

} g_horseReinsFileMounter;

void CReins::InitClass()
{
	CDataFileMount::RegisterMountInterface(CDataFileMgr::HORSE_REINS_FILE, &g_horseReinsFileMounter);
}

void CReins::Load(const char* pFileName)
{
	parSettings s = parSettings::sm_StrictSettings; 
	PARSER.LoadObject(pFileName, "xml", *this, &s);
}

void CReins::Reload(const char* pFileName)
{
	m_AttachData.Reset();
	m_CollisionData.Reset();
	m_BindPos.Reset();
	m_BindPos2.Reset();
	m_SoftPinRadiuses.Reset();

	Load(pFileName);
}


void CReins::Save()
{
#if !__FINAL

	Assert( m_BindPos.GetCount() == m_RopeInstance->GetVertsCount() );
	environmentCloth* envCloth = m_RopeInstance->GetEnvCloth();
	Assert( envCloth );
	phVerletCloth* cloth = envCloth->GetCloth();
	Assert( cloth );
	Vec3V* RESTRICT verts = cloth->GetClothData().GetVertexPointer();
	Assert( verts );

	if( m_AttachData[0].m_Entity )
	{
		Mat34V boneMat = GetObjectMtx( m_AttachData[0].m_Entity, m_AttachData[0].m_BoneIndex );	
		for( int i = 0; i < m_BindPos.GetCount(); ++i )
		{
			Vector3 localPosition;
			(reinterpret_cast<Matrix34*>(&boneMat))->UnTransform(VEC3V_TO_VECTOR3(verts[i]),localPosition);
			m_BindPos[i] = VECTOR3_TO_VEC3V(localPosition);
		}
	}

	char buff[128];
	formatf( buff, "%s%s", sm_tuneFilePath, m_Name );
	AssertVerify(PARSER.SaveObject( buff, "xml", this, parManager::XML));
#endif
}

void CReins::UnpinVertex(int idx)
{
	Assert(m_RopeInstance);
	environmentCloth* envCloth = m_RopeInstance->GetEnvCloth();
	Assert( envCloth );
	phVerletCloth* cloth = envCloth->GetCloth();
	Assert( cloth );
	cloth->DynamicUnpinVertex(idx);
}

void CReins::Update()
{
	environmentCloth* envCloth = m_RopeInstance->GetEnvCloth();
	Assert( envCloth );
	phVerletCloth* cloth = envCloth->GetCloth();
	Assert( cloth );

	const int collisionDataCount = m_CollisionData.GetCount();
	Assert( !collisionDataCount || cloth->m_VirtualBound );			// if there is collision data then virtual bound must exist

	Vec3V oldPos0 = cloth->GetClothData().GetVertexPosition(0);

	// NOTE: update attach data
	for( int i = 0; i < m_AttachData.GetCount(); ++i )
	{
		const CReinsAttachData& rAttachData = m_AttachData[i];

		if( rAttachData.m_Entity && (rAttachData.m_Entity->GetSkeleton() || !rAttachData.m_Entity->GetIsTypePed()) )
		{
			Mat34V boneMat = GetObjectMtx( rAttachData.m_Entity, rAttachData.m_BoneIndex );
			Vec3V pos = Transform( boneMat, rAttachData.m_Offset );

			envCloth->ControlVertex( rAttachData.m_VertexIndex, pos );
		}
	}

	// NOTE: update collision data
	for( int i = 0; i < collisionDataCount; ++i )
	{
		const CReinsCollisionData& rCollisionData = m_CollisionData[i];

		if( rCollisionData.m_Entity && (rCollisionData.m_Entity->GetSkeleton() || !rCollisionData.m_Entity->GetIsTypePed()) )
		{
			Mat34V boneMat;
			crSkeleton* skel = rCollisionData.m_Entity->GetSkeleton();
			if( skel )
			{
				Assertf( rCollisionData.m_BoneIndex > -1 && rCollisionData.m_BoneIndex < skel->GetBoneCount(), "Invalid bone index: %d", rCollisionData.m_BoneIndex );
				if( rCollisionData.m_BoneIndex > -1 && rCollisionData.m_BoneIndex < skel->GetBoneCount() )
					boneMat = skel->GetObjectMtx( rCollisionData.m_BoneIndex );
				else
					boneMat.SetIdentity3x3();
			}
			else
			{
				boneMat.SetIdentity3x3();
			}

			Mat34VRotateLocalZ( boneMat, rCollisionData.m_Rotation );
			((phBoundComposite*)cloth->m_VirtualBound.ptr)->SetCurrentMatrix( rCollisionData.m_BoundIndex, boneMat );
		}
	}

	const bool forceToBind = m_NumBindFrames ? true: false;
	m_NumBindFrames -= (u8)forceToBind;

	Vec3V pos0 = cloth->GetClothData().GetVertexPosition(0);
	Vec3V tempV = Subtract( pos0, oldPos0 );
	if( forceToBind )
	{
		UpdateToBind( m_BindPos2 );
	}
	else if( IsGreaterThanAll( Dot(tempV, tempV), ScalarV(V_ONE)) != 0 )
	{
		UpdateToBind( m_BindPos );
	}

	UpdateSoftPinning(m_UseSoftPinning);


#if __PFDRAW
	if( PFD_Ropes.Begin(true) )
	{
		environmentCloth* envCloth = m_RopeInstance->GetEnvCloth();
		Assert( envCloth );
		phVerletCloth* cloth = envCloth->GetCloth();
		Assert( cloth );
		Vec3V* RESTRICT verts = cloth->GetClothData().GetVertexPointer();
		Assert( verts );

		grcColor(Color_green);
		for( int i = 0; i < cloth->GetNumVertices(); ++i )
		{
			grcDrawSphere( m_SoftPinRadiuses[i], verts[i] );
		}

		PFD_Ropes.End();
	}
#endif

}

void CReins::UpdateToBind(atArray<Vec3V>& pose)
{
	environmentCloth* envCloth = m_RopeInstance->GetEnvCloth();
	Assert( envCloth );
	phVerletCloth* cloth = envCloth->GetCloth();
	Assert( cloth );
	Vec3V* RESTRICT verts = cloth->GetClothData().GetVertexPointer();
	Vec3V* RESTRICT prevverts = cloth->GetClothData().GetVertexPrevPointer();
	Assert( verts );

	if( m_AttachData[0].m_Entity )
	{
		Mat34V boneMat = GetObjectMtx( m_AttachData[0].m_Entity, m_AttachData[0].m_BoneIndex );

		for( int i = 0; i < pose.GetCount(); ++i )
		{
			if( !cloth->IsDynamicPinned(i) )
			{
				Vec3V newPos = Transform( boneMat, pose[i] );
				verts[i] = newPos;
				prevverts[i] = newPos;
			}
		}
	}
}

void CReins::UpdateSoftPinning(bool useSoftPinning)
{
	if( !useSoftPinning )
		return;

	environmentCloth* envCloth = m_RopeInstance->GetEnvCloth();
	Assert( envCloth );
	phVerletCloth* cloth = envCloth->GetCloth();
	Assert( cloth );
	Vec3V* RESTRICT verts = cloth->GetClothData().GetVertexPointer();
	Assert( verts );

	if( m_AttachData[0].m_Entity && ( !m_AttachData[0].m_Entity->GetIsTypePed() || m_AttachData[0].m_Entity->GetSkeleton() ) )
	{
		Mat34V boneMat = GetObjectMtx( m_AttachData[0].m_Entity, m_AttachData[0].m_BoneIndex );

		for( int i = 0; i < m_BindPos.GetCount(); ++i )
		{
			if( !cloth->IsDynamicPinned(i) )
			{
				Vec3V bindPos = Transform( boneMat, m_BindPos[i] );
				Vec3V tempV = Subtract( verts[i], bindPos );
				ScalarV mag2 = Dot(tempV, tempV);
				const float pinRadius = m_SoftPinRadiuses[i];
				ScalarV dist2Threshold = Scale( ScalarVFromF32(pinRadius), ScalarVFromF32(pinRadius) );
				if( IsGreaterThanAll(mag2,dist2Threshold) != 0 )
				{				
					verts[i] = AddScaled(bindPos, Normalize(tempV), ScalarVFromF32(pinRadius));
				}
			}
		}
	}
}

void CReins::AttachToBone(const CEntity* entity, int index)
{
	Assert( entity );
	Assert( index > -1 && index < m_AttachData.GetCount() );
	m_AttachData[index].m_Entity = entity;

	if( entity->GetIsTypePed() && m_AttachData[index].m_BoneIndex > -1 )
	{
		const crSkeleton* skel = entity->GetSkeleton();
		if(skel)
		{
			const crSkeletonData& skelData = skel->GetSkeletonData();
			const crBoneData* boneData = skelData.FindBoneData( m_AttachData[index].m_BoneName.c_str() );
			Assertf( boneData, "Ped skeleton is not complete(i.e. broken) ?  Bone: %s   is missing from the skeleton. Default 0 will be used!", m_AttachData[index].m_BoneName.c_str() );
			if( boneData )
			{
				m_AttachData[index].m_BoneIndex = boneData->GetIndex();
			}
			else
			{
				m_AttachData[index].m_BoneIndex = 0;
			}
		}
	}
}

void CReins::AttachToBones(const CEntity* entity)
{
	Assert( entity );
	for( int i = 0; i < 3; ++i )
	{
		AttachToBone( entity, i );
	}
}

void CReins::AttachBound(const CEntity* entity, int index)
{
	Assert( entity );
	Assert( index > -1 && index < m_CollisionData.GetCount() );
	Assert( m_RopeInstance );

	CReinsCollisionData& rCollisionData = m_CollisionData[index];
	rCollisionData.m_Entity = entity;

	m_RopeInstance->AttachVirtualBoundCapsule( rCollisionData.m_CapsuleRadius, rCollisionData.m_CapsuleLen, GetObjectMtx(entity, rCollisionData.m_BoneIndex), rCollisionData.m_BoundIndex );

	Assert( m_CompositeBoundPtr );
	rCollisionData.m_BoundPtr = m_CompositeBoundPtr->GetBound( rCollisionData.m_BoundIndex );
	Assert( rCollisionData.m_BoundPtr );
	rCollisionData.m_Enabled = true;
}

void CReins::AttachBounds(const CEntity* entity)
{
	Assert( entity );
	const int numBounds = m_CollisionData.GetCount();
	if( numBounds )
	{
		Assert( m_RopeInstance );
		const Mat34V* parentMat;

		if(entity->GetSkeleton())
		{
			parentMat = entity->GetSkeleton()->GetParentMtx();
		}
		else
		{
			Assert( entity->GetTransform().IsMatrixTransform() );
			parentMat = static_cast<const fwMatrixTransform*>( &entity->GetTransform() )->GetMatrixPtr();
		}
		Assert( parentMat );

		m_RopeInstance->CreateVirtualBound( numBounds , parentMat );

		Assert( m_RopeInstance->GetEnvCloth() );
		Assert( m_RopeInstance->GetEnvCloth()->GetCloth() );
		Assertf( !m_CompositeBoundPtr, "More than one horse component is using the default reins!" );
		m_CompositeBoundPtr = (phBoundComposite*)m_RopeInstance->GetEnvCloth()->GetCloth()->m_VirtualBound.ptr;
		Assert( m_CompositeBoundPtr );

		for( int i = 0; i < numBounds; ++i )
		{
			AttachBound(entity, i);
		}
	}
}

void CReins::AttachTwoHands(const CEntity* entity)
{
	if( !m_HasRider )
		return;

	m_ExpectedAttachState = 2;
	Assert( entity );

	if( !entity->GetSkeleton() )
		return;

	CReinsAttachData& attachData2 = GetAttachData( 2 );
	attachData2.m_Entity = NULL;
	attachData2.m_BoneIndex	= -1;
	UnpinVertex( attachData2.m_VertexIndex );

	CReinsAttachData& attachData3 = GetAttachData( 3 );
	attachData3.m_Entity = NULL;
	attachData3.m_BoneIndex	= -1;
	UnpinVertex( attachData3.m_VertexIndex );

	const crSkeletonData& skelData = entity->GetSkeletonData();

	CReinsAttachData& attachData4 = GetAttachData( 4 );
	const crBoneData* boneData4 = skelData.FindBoneData( attachData4.m_BoneName.c_str() );
	Assert( boneData4 );
	if (!boneData4)
		return;
	attachData4.m_BoneIndex	= boneData4->GetIndex();
	attachData4.m_Entity = entity;

	CReinsAttachData& attachData5 = GetAttachData( 5 );
	const crBoneData* boneData5 = skelData.FindBoneData( attachData5.m_BoneName.c_str() );
	Assert( boneData5 );
	if (!boneData5)
		return;
	attachData5.m_BoneIndex	= boneData5->GetIndex();
	attachData5.m_Entity = entity;
}

void CReins::AttachOneHand(const CEntity* entity)
{
	if( !m_HasRider )
		return;

	m_ExpectedAttachState = 1;
	Assert( entity );

	if( !entity->GetSkeleton() )
		return;

	CReinsAttachData& attachData2 = GetAttachData( 2 );
	attachData2.m_BoneIndex	= -1;
	attachData2.m_Entity = NULL;
	UnpinVertex( attachData2.m_VertexIndex );

	const crSkeletonData& skelData = entity->GetSkeletonData();
	CReinsAttachData& attachData3 = GetAttachData( 3 );	
	const crBoneData* boneData3 = skelData.FindBoneData( attachData3.m_BoneName.c_str() );
	Assert( boneData3 );
	if (!boneData3)
		return;
	attachData3.m_BoneIndex	= boneData3->GetIndex();
	attachData3.m_Entity = entity;

	CReinsAttachData& attachData4 = GetAttachData( 4 );
	UnpinVertex( attachData4.m_VertexIndex );
	attachData4.m_Entity = NULL;
	attachData4.m_BoneIndex	= -1;

	CReinsAttachData& attachData5 = GetAttachData( 5 );
	UnpinVertex( attachData5.m_VertexIndex );
	attachData5.m_Entity = NULL;
	attachData5.m_BoneIndex	= -1;
}

void CReins::Detach(const CEntity* /*entity*/)
{
	CReinsAttachData& attachData0 = GetAttachData( 0 );	

	CReinsAttachData& attachData2 = GetAttachData( 2 );	
	attachData2.m_Entity = attachData0.m_Entity;
	if( attachData2.m_Entity )
	{
		const crSkeleton* skel = attachData2.m_Entity->GetSkeleton();
		if(skel)
		{
			const crSkeletonData& skelData = skel->GetSkeletonData();
			const crBoneData* boneData2 = skelData.FindBoneData( attachData2.m_BoneName.c_str() );
			Assert( boneData2 );
			attachData2.m_BoneIndex	= boneData2->GetIndex();
		}
	}

	CReinsAttachData& attachData3 = GetAttachData( 3 );
	attachData3.m_Entity = NULL;
	attachData3.m_BoneIndex = -1;
	UnpinVertex( attachData3.m_VertexIndex );

	CReinsAttachData& attachData4 = GetAttachData( 4 );
	attachData4.m_Entity = NULL;
	attachData4.m_BoneIndex = -1;
	UnpinVertex( attachData4.m_VertexIndex );

	CReinsAttachData& attachData5 = GetAttachData( 5 );
	attachData5.m_Entity = NULL;
	attachData5.m_BoneIndex = -1;
	UnpinVertex( attachData5.m_VertexIndex );
}

void CReins::ReserveCollisionData(const int arrayCapacity)
{
	Assert( arrayCapacity > 0 );
	m_CollisionData.Reserve( arrayCapacity );
}

void CReins::ReserveAttachData(const int arrayCapacity)
{
	Assert( arrayCapacity > 0 );
	m_AttachData.Reserve( arrayCapacity );
}

void CReins::Hide()
{
	Assert( m_RopeInstance );
	m_RopeInstance->SetDrawEnabled(false);
}

Mat34V_Out CReins::GetObjectMtx(const CEntity *entity, int boneIndex)
{
	Assert( entity );
	crSkeleton* skel = entity->GetSkeleton();
	if( skel )
	{
		Assertf( boneIndex > -1 && boneIndex < skel->GetBoneCount(), "Invalid bone index: %d", boneIndex );
		if( boneIndex > -1 && boneIndex < skel->GetBoneCount() )
		{
			Mat34V parentMat = *(skel->GetParentMtx());
			Mat34V objMat;
			Transform( objMat, parentMat, skel->GetObjectMtx( boneIndex ) );
			return objMat;
		}
	}
	
	return entity->GetMatrix();
}


#if __BANK

void CReins::ToggleBound(void* pData)
{
	Assert( pData );
	CReinsCollisionData* collisionData = (CReinsCollisionData*)pData;

	Assert( m_CompositeBoundPtr );
	// NOTE: access bound array directly to avoid ref counting through SetBound
	phBound** bounds = m_CompositeBoundPtr->GetBoundArray();
	Assert( bounds );
	bounds[collisionData->m_BoundIndex] = collisionData->m_Enabled ? collisionData->m_BoundPtr: NULL;	
}

void CReins::UpdateBound(void* pData)
{
	Assert( pData );
	CReinsCollisionData* collisionData = (CReinsCollisionData*)pData;

	Assert( collisionData->m_BoundPtr->GetType() == phBound::CAPSULE );
	phBoundCapsule* bndCapsule = (phBoundCapsule*)collisionData->m_BoundPtr;
	Assert( bndCapsule );
	bndCapsule->SetCapsuleSize( collisionData->m_CapsuleRadius, collisionData->m_CapsuleLen );
}

void CReins::AddWidgets()
{
	bkBank* bank = ropeBankManager::GetBank();
	if( bank )
	{
		m_RAGGroup = bank->PushGroup("ReinsData");

		//		bank->AddButton("Load", datCallback( MFA( CReins::Load ), this) );
		bank->AddButton("Save", datCallback( MFA( CReins::Save ), this) );
		bank->AddToggle("UseSoftPinning", &m_UseSoftPinning );

		bank->PushGroup("AttachDataList");
		for(int i = 0; i < m_AttachData.GetCount(); ++i)
		{
			CReinsAttachData& attachData = m_AttachData[i];
			char buff[32];
			formatf(buff, "#%d", i );
			bank->PushGroup( buff );
			bank->AddSlider( "VertexIndex", &attachData.m_VertexIndex, 0, m_RopeInstance->GetEnvCloth()->GetNumRopeVertices(), 1 );
			bank->AddVector( "Offset", &attachData.m_Offset, -10.0f, 10.0f, 0.0001f );
			bank->PopGroup();
		}
		bank->PopGroup();

		bank->PushGroup("CollisionDataList");
		for(int i = 0; i < m_CollisionData.GetCount(); ++i)
		{
			CReinsCollisionData& collisionData = m_CollisionData[i];
			char buff[32];
			formatf(buff, "#%d", i );
			bank->PushGroup( buff );
			bank->AddSlider( "CapsuleRadius", &collisionData.m_CapsuleRadius, 0.0f, 10.0f, 0.00001f, datCallback( MFA1( CReins::UpdateBound ), this, &collisionData) );
			bank->AddSlider( "CapsuleLength", &collisionData.m_CapsuleLen, 0.0f, 10.0f, 0.00001f, datCallback( MFA1( CReins::UpdateBound ), this, &collisionData) );
			bank->AddSlider( "BoundIndex", &collisionData.m_BoundIndex, 0, 10, 1 );
			bank->AddToggle( "Enabled", &collisionData.m_Enabled, datCallback( MFA1( CReins::ToggleBound ), this, &collisionData) );						
			bank->PopGroup();
		}
		bank->PopGroup();

		bank->PopGroup();
	}
}


#endif // __BANK



////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// CHorseComponent //////////////////////////////
////////////////////////////////////////////////////////////////////////////////

bool CHorseComponent::ms_MPAutoFleeBeforeMountedEnabled = false;
bool CHorseComponent::ms_MPAutoFleeAfterMountedEnabled = true;
u32 CHorseComponent::ms_MPAutoFleeBeforeMountedWaitTime = 10000;
u32 CHorseComponent::ms_MPAutoFleeAfterMountedWaitTime = 1500;
bool CHorseComponent::ms_MPAutoFleeAfterMountedMaintainMBR = false;

CHorseComponent::CHorseComponent()
: m_pHrsSimTune(0)
, m_SpurredThisFrame(false)
, m_bSuppressSpurAnim(false)
, m_ForceHardBraking(false)
, m_Reins(NULL)
, m_ReplicatedSpeedFactor(1.0f)
, m_bLeftStirrupEnable(false)
, m_bRightStirrupEnable(false)
, m_fRightStirrupBlendPhase(0)
, m_fLeftStirrupBlendPhase(0)
{
	DetachTuning();
}

CHorseComponent::~CHorseComponent()
{
	if( m_Reins )
	{
		delete m_Reins;
		m_Reins = NULL;
	}
}


////////////////////////////////////////////////////////////////////////////////

void CHorseComponent::Init(CPed* owner)
{
	m_pPed = owner;
	DetachTuning();

	m_SpurredThisFrame = false;
	m_bSuppressSpurAnim = false;
	m_SpeedCtrl.ClearTopSpeedOverride();
	m_FreshnessCtrl.SetFreshnessLocked(false);
	m_ObstacleAvoidanceCtrl.Init(owner);
	ResetSimTune();
	Reset();

	m_WasEverMounted = false;
	m_bHasBeenOwnedByPlayer = false;
}

void CHorseComponent::HideReins()
{
	if( m_Reins )
	{
		m_Reins->Hide();
	}
}

void CHorseComponent::InitReins()
{
	Assert( m_pPed );

// TODO: need proper model name
	Assert( !m_Reins );
	m_Reins = rage_new CReins( defaultReins );
	Assert( m_Reins );
	
	if(!m_Reins->Init())
	{
		// failed to create reins - abort
		delete m_Reins;
		m_Reins = NULL;

		return;
	}

	m_Reins->AttachToBones( m_pPed );
	m_Reins->AttachBounds( m_pPed );
	m_Reins->Update();
	m_Reins->UpdateToBind( m_Reins->GetDefaultBindPose() );

#if __BANK
 	m_Reins->AddWidgets();
#endif
}

////////////////////////////////////////////////////////////////////////////////

void CHorseComponent::Shutdown()
{
	DetachTuning();
}


////////////////////////////////////////////////////////////////////////////////

void CHorseComponent::Reset()
{
	m_FreshnessCtrl.Reset();
	m_SpeedCtrl.Reset();
	m_AgitationCtrl.Reset();	
	m_BrakeCtrl.Reset();	
	m_ObstacleAvoidanceCtrl.Reset();
	m_SpurredThisFrame = false;
	m_bSuppressSpurAnim = false;
	ClearHeadingOverride();
	OnUpdateMountState();
	m_LastTimeCouldNotFleeInMillis = fwTimer::GetGameTimer().GetTimeInMilliseconds();
}

////////////////////////////////////////////////////////////////////////////////

void CHorseComponent::ResetGaits()
{
	m_SpeedCtrl.Reset();
	m_FreshnessCtrl.Reset();
}

////////////////////////////////////////////////////////////////////////////////


// PURPOSE:
//	Find the correct CHorseTune for this horse
void CHorseComponent::ResetSimTune()
{
	const hrsSimTune * pSimTune = NULL;
	
	// use the horse's name for determining tuning. TODO
	// Find the enum name for our actor:		
	//		pSimTune = HRSSIMTUNEMGR.FindSimTune(pEnumName);
	
	// If, for some reason, there is no tune data, pull in the default.
	if(!pSimTune)
	{
		pSimTune = HRSSIMTUNEMGR.FindSimTune("default");
		Assert(pSimTune);
	}
	if(pSimTune)
	{
		AttachTuning(*pSimTune);
	}
}


////////////////////////////////////////////////////////////////////////////////

void CHorseComponent::Update()
{
	if(m_pPed->IsNetworkClone())
	{
		// Keep speed control values updated so a moving horse won't suddenly stop if it gets migrated to us
		float fMoveBlendRatio = m_pPed->GetMotionData()->GetDesiredMbrY();

		m_SpeedCtrl.SetTgtSpeed(fMoveBlendRatio / MOVEBLENDRATIO_SPRINT);
		m_SpeedCtrl.SetCurrSpeed(fMoveBlendRatio / MOVEBLENDRATIO_SPRINT);
	}
	else
	{
		m_SpeedCtrl.UpdateMaterialSpeedMult(TIME.GetSeconds(), m_pPed->GetPackedGroundMaterialId());
	}

	AutoFlee();
}

void CHorseComponent::AutoFlee()
{
	if (NetworkInterface::IsGameInProgress() /*|| m_pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_EnableHorseMPAutoFleeInSP) || m_pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_Traffic)*/)
	{
		if (!m_pPed->IsNetworkClone())
		{
			const bool hasImportantTask = m_pPed->GetPedIntelligence()->GetTaskManager()->GetActiveTaskPriority(PED_TASK_TREE_PRIMARY) < PED_TASK_PRIORITY_DEFAULT;
			const bool autoFleeEnabled = m_WasEverMounted ? ms_MPAutoFleeAfterMountedEnabled : ms_MPAutoFleeBeforeMountedEnabled;
			const bool disableAutoFleeFlag = false; //m_pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DisableHorseMPAutoFlee);
			const bool canFlee = autoFleeEnabled && !(GetRider() || hasImportantTask || disableAutoFleeFlag);

			if (canFlee)
			{
				const u32 timeToFlee = m_WasEverMounted ? ms_MPAutoFleeAfterMountedWaitTime : ms_MPAutoFleeBeforeMountedWaitTime;

				if (m_LastTimeCouldNotFleeInMillis > 0 && (fwTimer::GetTimeInMilliseconds() - m_LastTimeCouldNotFleeInMillis >= timeToFlee))
				{
					if (!(m_pPed->GetPedIntelligence()->GetTaskSmartFlee() != NULL))
					{
						CAITarget target(VEC3V_TO_VECTOR3(m_pPed->GetTransform().GetPosition()));
						CTaskSmartFlee* pSmartFleeTask = rage_new CTaskSmartFlee(target);
						if (ms_MPAutoFleeAfterMountedMaintainMBR)
						{
							Vector2 vCurrentMoveBlendRatio;
							m_pPed->GetMotionData()->GetCurrentMoveBlendRatio(vCurrentMoveBlendRatio);
							pSmartFleeTask->SetMoveBlendRatio(vCurrentMoveBlendRatio.Mag());
						}
						CEventGivePedTask evt(PED_TASK_PRIORITY_PRIMARY, pSmartFleeTask);
						m_pPed->GetPedIntelligence()->AddEvent(evt);
					}
					m_LastTimeCouldNotFleeInMillis = 0;
				}
			}
		}
	}
}

void CHorseComponent::UpdateInput(bool bSpur, float fTimeSinceSpur, bool bHardBrake, float fBrake, bool bMaintainSpeed, bool bAiming, Vector2& vStick, float fMatchSpeed)
{		
	float fMoveBlendRatio = m_pPed->GetMotionData()->GetCurrentMbrY();
	if (m_bSuppressSpurAnim && !bSpur && m_SpurredThisFrame)
		m_bSuppressSpurAnim = false; //reset!
	m_SpurredThisFrame = bSpur;
	
	float fCurrSpdMS = Mag(VECTOR3_TO_VEC3V(m_pPed->GetVelocity())).Getf();
	
	bool isJumping = m_pPed->GetPedResetFlag(CPED_RESET_FLAG_IsJumping);
	if ( !isJumping )
		UpdateObstacleAvoidance(fCurrSpdMS, vStick.x );
	bool avoidanceBraking = m_ObstacleAvoidanceCtrl.GetBrake() > 0.0f;
	float fSoftBrake = ( avoidanceBraking ? m_ObstacleAvoidanceCtrl.GetBrake() : fBrake );
	static dev_float CLIFF_BRAKE_INSTANT_STOP_THRESHOLD = 0.6f;
	if (avoidanceBraking && m_ObstacleAvoidanceCtrl.GetBrake() > CLIFF_BRAKE_INSTANT_STOP_THRESHOLD) {
		m_BrakeCtrl.RequestInstantStop();		
	}
	UpdateFreshness(fCurrSpdMS, m_SpurredThisFrame);
	UpdateAgitation(m_SpurredThisFrame, m_FreshnessCtrl.GetCurrFreshness());		
	UpdateBrake(fSoftBrake, bHardBrake || m_ForceHardBraking, fMoveBlendRatio);	

	if ( !m_pPed->GetPedResetFlag( CPED_RESET_FLAG_DontChangeHorseMbr ))
	{	
		UpdateSpeed(fCurrSpdMS, m_SpurredThisFrame,
			bMaintainSpeed, fTimeSinceSpur, 
			vStick, bAiming, 
			fMatchSpeed);	
	}

	m_ForceHardBraking=false;
}


bool CHorseComponent::AdjustHeading(float & fInOutGoalHeading)
{	
	bool bHeadingWasAdjusted = false; //m_AvoidanceCtrl.AdjustHeading(bHeadingInput, fInOutGoalHeading, fInOutDestHeading);

	if(!bHeadingWasAdjusted)
	{
		bHeadingWasAdjusted = m_ObstacleAvoidanceCtrl.AdjustHeading(fInOutGoalHeading);
	}

	return bHeadingWasAdjusted;
}

////////////////////////////////////////////////////////////////////////////////

void CHorseComponent::HandleOnMount(float fRiderSpeedNorm)
{
	// NOTE: We will eventually want to be more selective about which
	// subsystems we reset on mount.  For now, just reset everything:
	Reset();

	// Apply the rider's speed to agitation, and speed:
	m_AgitationCtrl.OnMount(fRiderSpeedNorm);	

	float fCurrSpdMS = Mag(VECTOR3_TO_VEC3V(m_pPed->GetVelocity())).Getf();

	// Update the obstacle avoidance
	bool bZeroSpeed = false;
	UpdateObstacleAvoidance(fCurrSpdMS, 0.0f, true);
	bZeroSpeed = m_ObstacleAvoidanceCtrl.GetAvoiding();

	m_SpeedCtrl.OnMount(fCurrSpdMS, bZeroSpeed);
}

////////////////////////////////////////////////////////////////////////////////

void CHorseComponent::HandleCollision()
{
	m_SpeedCtrl.HandleCollision();
}

//////////////////////////////////////////////////////////////////////////
CPed* CHorseComponent::GetRider() { return m_pPed ? m_pPed->GetSeatManager()->GetDriver() : NULL;}
const CPed* CHorseComponent::GetRider() const { return m_pPed ? m_pPed->GetSeatManager()->GetDriver() : NULL; }

//////////////////////////////////////////////////////////////////////////
CPed* CHorseComponent::GetLastRider() { return m_pPed ? m_pPed->GetSeatManager()->GetLastPedInSeat(Seat_driver) : NULL;}
const CPed* CHorseComponent::GetLastRider() const { return m_pPed ? m_pPed->GetSeatManager()->GetLastPedInSeat(Seat_driver) : NULL; }

//////////////////////////////////////////////////////////////////////////
void CHorseComponent::SetLastRider(CPed* pPed) { if(m_pPed) { m_pPed->GetSeatManager()->SetLastPedInSeat(pPed, Seat_driver); } }

////////////////////////////////////////////////////////////////////////////////

void CHorseComponent::AttachTuning(const hrsSimTune & rTune)
{
	m_pHrsSimTune = &rTune;
	m_FreshnessCtrl.AttachTuning(m_pHrsSimTune->GetFreshnessTune());
	m_SpeedCtrl.AttachTuning(m_pHrsSimTune->GetSpeedTune());	
	m_AgitationCtrl.AttachTuning(m_pHrsSimTune->GetAgitationTune());	
	m_BrakeCtrl.AttachTuning(m_pHrsSimTune->GetBrakeTune());		
#if __BANK
	formatf(m_AttachedTuneName, "%s", m_pHrsSimTune->GetHrsTypeName());
#endif
}

////////////////////////////////////////////////////////////////////////////////

void CHorseComponent::DetachTuning()
{
	m_FreshnessCtrl.DetachTuning();
	m_SpeedCtrl.DetachTuning();	
	m_AgitationCtrl.DetachTuning();
	m_BrakeCtrl.DetachTuning();	
#if __BANK
	m_AttachedTuneName[0] = 0;
#endif
}

////////////////////////////////////////////////////////////////////////////////

void CHorseComponent::UpdateMountState()
{	
	OnUpdateMountState();

	Assert(m_pPed->GetSeatManager());
	CPed* pRider = m_pPed->GetSeatManager()->GetDriver();
	bool bIsMounted = pRider!=NULL;
	m_WasEverMounted = m_WasEverMounted || !bIsMounted;
	m_LastTimeCouldNotFleeInMillis = fwTimer::GetTimeInMilliseconds();

	AutoFlee();
}

void CHorseComponent::OnUpdateMountState()
{
	Assert(m_pPed->GetSeatManager());
	CPed* pRider = m_pPed->GetSeatManager()->GetDriver();

	bool bIsMounted = pRider!=NULL;
	bool bIsRiderPlayer = pRider && pRider->IsPlayer();
	bool bIsHitched = false; //TODO

	m_FreshnessCtrl.UpdateMountState(bIsMounted, bIsRiderPlayer, bIsHitched);
	m_SpeedCtrl.UpdateMountState(bIsMounted, bIsRiderPlayer, bIsHitched);	
	m_AgitationCtrl.UpdateMountState(bIsMounted, bIsRiderPlayer,bIsHitched);	
	m_BrakeCtrl.UpdateMountState(bIsMounted, bIsRiderPlayer, bIsHitched);	
	m_ObstacleAvoidanceCtrl.UpdateMountState(bIsMounted, bIsRiderPlayer, bIsHitched);
}

////////////////////////////////////////////////////////////////////////////////

void CHorseComponent::UpdateSpeed(float fCurrMvrSpdMS,
	const bool bSpurredThisFrame, const bool bMaintainSpeed, const float fTimeSinceSpur, const Vector2 & vStick,
	const bool bIsAiming, float speedMatchSpeed)
{
	CTaskMotionBase* pMotionTask = m_pPed->GetCurrentMotionTask();
	if (pMotionTask)
	{
		CMoveBlendRatioSpeeds moveSpeeds;
		pMotionTask->GetMoveSpeeds(moveSpeeds);
		m_SpeedCtrl.UpdateSprintSpeed( moveSpeeds.GetSprintSpeed() ); 
		//m_SpeedCtrl.UpdateSprintSpeed(pAnim->GetActionSet()->GetNavigation()->ConvertGaitTypeToSpeedMPS(aniGait::GAIT_SPRINT)); 
	}

	float fWaterMaxSpd = m_ObstacleAvoidanceCtrl.CalculateWaterMaxSpeed();
	u32 uHorseSpeedFlags = hrsSpeedControl::HSF_AllowSlowdownAndSpeedGain;
	if (bSpurredThisFrame) uHorseSpeedFlags|=hrsSpeedControl::HSF_SpurredThisFrame;
	if (bMaintainSpeed)
	{
		uHorseSpeedFlags |= hrsSpeedControl::HSF_MaintainSpeed;

		const CPed* pRider = GetRider();
		if (pRider && pRider->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_PLAYER_HORSE_FOLLOW))
		{
			uHorseSpeedFlags |= hrsSpeedControl::HSF_NoBoost;
		}
	}
	if (bIsAiming) uHorseSpeedFlags|=hrsSpeedControl::HSF_Aiming;
	static dev_float sf_MinHorseSlideSpeed = 0.1f;
	if (m_pPed->GetSlideSpeed() > sf_MinHorseSlideSpeed) 
	{
		static dev_float sf_UphillTolerance = 0.25f;
		float fForwardZf = m_pPed->GetTransform().GetForward().GetZf();		
		bool bIsDownHill = fForwardZf < sf_UphillTolerance;
		if (!bIsDownHill)
			uHorseSpeedFlags|=hrsSpeedControl::HSF_Sliding;
	}

	m_SpeedCtrl.Update(uHorseSpeedFlags, m_FreshnessCtrl.GetCurrFreshness(), fCurrMvrSpdMS, 
		ComputeMaxSpeedNorm(), m_pPed->GetMotionData()->GetCurrentMbrY()/MOVEBLENDRATIO_SPRINT, fTimeSinceSpur, fWaterMaxSpd, vStick,
		m_BrakeCtrl, TIME.GetSeconds(), speedMatchSpeed);
}

////////////////////////////////////////////////////////////////////////////////

void CHorseComponent::UpdateFreshness(const float fCurrSpdMS, const bool bSpurredThisFrame)
{
	float fCurrSpdNorm = m_SpeedCtrl.ComputeNormalizedSpeed(fCurrSpdMS);

	if (m_pPed->GetPedResetFlag( CPED_RESET_FLAG_InfiniteStamina ))
	{		
		m_FreshnessCtrl.SetCurrFreshness(1.0f);		
	}
	else
	{
		m_FreshnessCtrl.Update(fCurrSpdNorm, bSpurredThisFrame, TIME.GetSeconds());
	}
}

////////////////////////////////////////////////////////////////////////////////

void CHorseComponent::UpdateAgitation(const bool bSpurredThisFrame, const float fFreshness)
{
	m_AgitationCtrl.Update(bSpurredThisFrame, fFreshness, TIME.GetSeconds());
}

////////////////////////////////////////////////////////////////////////////////

void CHorseComponent::UpdateBrake(const float fSoftBrake, const bool bHardBrake, const float fMoveBlendRatio)
{
	m_BrakeCtrl.Update(fSoftBrake, bHardBrake, fMoveBlendRatio);
}

////////////////////////////////////////////////////////////////////////////////
void CHorseComponent::UpdateObstacleAvoidance(const float fCurrSpeedMS, const float fStickTurn, const bool bOnMountCheck)
{
	if( m_ObstacleAvoidanceCtrl.IsEnabled() )
	{
		float fStopDist = m_BrakeCtrl.ComputeSoftBrakeStopDist(m_SpeedCtrl.GetCurrSpeed(), fCurrSpeedMS);
		Matrix34 mtx = MAT34V_TO_MATRIX34(m_pPed->GetMatrix());
		// If It is swimming do not use m_pPed->GetGroundPos()
		// It has a -10000000 z when not standing and that 
		// makes the obstacle avoidance think that there is no navmesh, 
		// so it is near a cliff.
		if ( !m_pPed->GetIsSwimming() )
		{
			mtx.MakeTranslate(m_pPed->GetGroundPos());
		}
		m_ObstacleAvoidanceCtrl.Update( fCurrSpeedMS, mtx, m_pPed->GetCurrentHeading(), fStopDist, fStickTurn, bOnMountCheck);
	}
}


////////////////////////////////////////////////////////////////////////////////

float CHorseComponent::ComputeMaxSpeedNorm() const
{
	return 1.f;
}

////////////////////////////////////////////////////////////////////////////////

void CHorseComponent::SetupMissionState()
{
	m_bHasBeenOwnedByPlayer = true;
}

////////////////////////////////////////////////////////////////////////////////

#if __BANK

void WarpPlayerOntoHorseCB()
{	
	CPed * pFocus = CPedDebugVisualiserMenu::GetFocusPed();		
	if( pFocus )
	{		
		CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
		pPlayerPed->SetPedOnMount(pFocus); 
	}
	else if(CPedFactory::GetLastCreatedPed())
	{
		CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
		s32 seat = -1;
		if( pPlayerPed->GetPedConfigFlag( CPED_CONFIG_FLAG_OnMount ) &&
			pPlayerPed->GetMyMount() )
		{
			seat = pPlayerPed->GetMyMount()->GetSeatManager()->GetPedsSeatIndex(pPlayerPed);			
			if( CPedFactory::GetLastCreatedPed() != pPlayerPed->GetMyMount() )
			{
				seat = 0;
			}
			else
			{
				++seat;
				if( seat >= pPlayerPed->GetMyMount()->GetPedModelInfo()->GetModelSeatInfo()->GetNumSeats() )
					seat = 0;
			}
			pPlayerPed->SetPedOffMount();
		}
		else
		{
			pPlayerPed->GetPedIntelligence()->FlushImmediately();
			seat = 0;
		}
		pPlayerPed->SetPedOnMount(CPedFactory::GetLastCreatedPed(), seat);
	}
}


void CHorseComponent::AddStaticWidgets(bkBank &b)
{
	b.PushGroup("Config", false);
	b.AddToggle("MP Autoflee Before Mounted", &ms_MPAutoFleeBeforeMountedEnabled);
	b.AddSlider("MP Autoflee Before Mounted Delay", &ms_MPAutoFleeBeforeMountedWaitTime, 0, 120000, 500);
	b.AddToggle("MP Autoflee After Mounted", &ms_MPAutoFleeAfterMountedEnabled);
	b.AddSlider("MP Autoflee After Mounted Delay", &ms_MPAutoFleeAfterMountedWaitTime, 0, 120000, 500);
	b.AddToggle("MP Autoflee Maintain Move Blend Ratio", &ms_MPAutoFleeAfterMountedMaintainMBR);
	b.PopGroup();
	b.AddButton("Warp player onto last Mount", WarpPlayerOntoHorseCB);
}

// PMD - Not sure why but this seems to be dead code
#if 0
void CHorseComponent::AddWidgets(bkBank &b)
{
	b.PushGroup("Horse", false);
	b.AddText("Attached Tuning", m_AttachedTuneName, kTuneNameMaxLen, true);
	m_FreshnessCtrl.AddWidgets(b);
	m_SpeedCtrl.AddWidgets(b);
	m_AgitationCtrl.AddWidgets(b);
	m_BrakeCtrl.AddWidgets(b);
	m_ObstacleAvoidanceCtrl.AddWidgets(b);
	b.PopGroup();
}
#endif

#endif

#endif // ENABLE_HORSE