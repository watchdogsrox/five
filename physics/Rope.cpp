
#if __BANK

#include "bank/bkmgr.h"
#include "bank/bank.h"
#include "bank/combo.h"
#include "bank/slider.h"
#include "debug/DebugScene.h"
#include "grcore/debugdraw.h"
#include "input/keyboard.h"
#include "input/keys.h"
#include "scene/Entity.h"

#include "physics.h"
#include "Rope.h"
#include "fwpheffects/ropemanager.h"
#include "fwpheffects/ropedatamanager.h"
#include "physics/gtaArchetype.h"

#include "Peds/Rendering/PedVariationStream.h"
#include "Peds/ped.h"
#include "renderer/Entities/PedDrawHandler.h"

//#pragma optimize("", off)

namespace rage
{


EXT_PFD_DECLARE_ITEM(Ropes);

bkBank* ropeBank		= NULL;
bkButton* clearButton	= NULL;
bkButton* reloadButton	= NULL;
bkButton* addBankButton = NULL;

s32 iComponent= s32(NULL);
Vector3 s_Offset = Vector3( 0.0f, 0.0f, 0.0f );
Vec3V s_RopeStart;
s32 iClicked = 0;


#define		BIG_FLOAT		999999.0f
#define		BONE_NAME_LEN		16

phInst* entityEnd = NULL;
CEntity* pEntityEnd = NULL;
Vector3 ropeEnd( BIG_FLOAT, BIG_FLOAT, BIG_FLOAT );

const char* s_RopeTypeNames[NUM_ROPE_TYPES_IN_XML] = { NULL };
char s_BoneNameA[BONE_NAME_LEN];
char s_BoneNameB[BONE_NAME_LEN];


// TODO: work in progress - cloth debug purposes
CEntity* pLastPickedPed = NULL;

void clothBankManager::DetachFromDrawableCallback(clothVariationData* pClothVarData)
{
	Assert( pLastPickedPed );
	Assert( pClothVarData );

	const ePedVarComp varComp = (const ePedVarComp)clothManager::ms_ComponentIndex;//PV_COMP_UPPR;

	CPed* pPed = (CPed*)pLastPickedPed;

	CPedDrawHandler* pPedDrawHandler = (CPedDrawHandler*)&(pLastPickedPed->GetDrawHandler());
	Assert( pPedDrawHandler );
	CPedStreamRenderGfx* pGfxData = pPedDrawHandler->GetPedRenderGfx();
	Assert( pGfxData );

	CPedVariationData& varData = pPedDrawHandler->GetVarData();
	varData.ResetClothData( varComp );

	pGfxData->m_clothData[varComp] = NULL;
#if HAS_RENDER_MODE_SHADOWS
	pPed->ClearRenderFlags(CEntity::RenderFlag_USE_CUSTOMSHADOWS);
#endif

	pPed->m_CClothController[varComp] = NULL;

	pClothVarData->DeleteBuffers();


// TODO: do something ??

//	pCharCloth->SetEntityVerticesPtr( pClothVarData->GetClothVertices(clothManager::GetBufferIndex()) );
//	pCharCloth->SetEntityParentMtxPtr( pClothVarData->GetClothParentMatrixPtr() );

//	const crSkeleton* pSkeleton = pPed->GetSkeleton();
//	Assert( pSkeleton );

//	const Mat34V* parentMtx = pSkeleton->GetParentMtx();
//	Assert( parentMtx );
//	pCharCloth->SetParentMtx( *parentMtx );

//	gDrawListMgr->CharClothRemove( pClothVarData );
}


void clothBankManager::AttachToDrawableCallback(clothVariationData* pClothVarData)
{
	Assert( pLastPickedPed );
	Assert( pClothVarData );
	
	const ePedVarComp varComp = (const ePedVarComp)clothManager::ms_ComponentIndex; //PV_COMP_UPPR;

	CPed* pPed = (CPed*)pLastPickedPed;

	CPedDrawHandler* pPedDrawHandler = (CPedDrawHandler*)&(pLastPickedPed->GetDrawHandler());
	Assert( pPedDrawHandler );
	CPedStreamRenderGfx* pGfxData = pPedDrawHandler->GetPedRenderGfx();
	Assert( pGfxData );

	CPedVariationData& varData = pPedDrawHandler->GetVarData();
	varData.SetClothData( varComp, pClothVarData );
		
	pGfxData->m_clothData[varComp] = pClothVarData;
#if HAS_RENDER_MODE_SHADOWS
	pPed->SetRenderFlag(CEntity::RenderFlag_USE_CUSTOMSHADOWS);
#endif

	characterCloth* pCharCloth = pClothVarData->GetCloth();
	Assert( pCharCloth );
	characterClothController* pCharClothController = pCharCloth->GetClothController();
	Assert( pCharClothController );
	pPed->m_CClothController[varComp] = pCharClothController;


// NOTE: needed only for rendering
	const int numVerts = pCharClothController->GetCurrentCloth()->GetNumVertices();
	pClothVarData->AllocateBuffers( numVerts );
	const int bufferIdx = clothManager::GetBufferIndex();
	pCharCloth->SetEntityVerticesPtr( pClothVarData->GetClothVertices(bufferIdx) );
	pCharCloth->SetEntityParentMtxPtr( pClothVarData->GetClothParentMatrixPtr(bufferIdx) );

	const crSkeleton* pSkeleton = pPed->GetSkeleton();
	Assert( pSkeleton );

	const Mat34V* parentMtx = pSkeleton->GetParentMtx();
	Assert( parentMtx );
	pCharCloth->SetParentMtx( *parentMtx );

	for(int i = 0; i < NUM_CLOTH_BUFFERS; ++i)
	{
		Vec3V* RESTRICT verts = pClothVarData->GetClothVertices(i);
		Assert( verts );
		// NOTE: don't want to crash if the game run out of streaming memory 
		if( verts )
		{
			Vec3V* RESTRICT origVerts = pCharClothController->GetCloth(0)->GetClothData().GetVertexPointer();
			Assert( origVerts );
			// origVerts should exists, if this fails there is bug somewhere in cloth instancing
			for(int j=0; j<numVerts; ++j)
			{
				verts[j] = origVerts[j];
#if RSG_PC || RSG_DURANGO || RSG_ORBIS
				// clear out W channel to prevent NaNs in shader vars
				verts[j].SetW(MAGIC_ZERO);
#endif
			}
		}
	}
}


void clothBankManager::PickPed()
{
	if( clothManager::ms_EnablePedPick )
	{
		Vector3 l_Offset = Vector3( 0.0f, 0.0f, 0.0f );
		s32 iComp = s32(NULL);
		CEntity* pEntity = CDebugScene::GetEntityUnderMouse( &l_Offset, &iComp, ArchetypeFlags::GTA_PED_TYPE );
		if( pEntity && pEntity->GetIsPhysical() && ioMapper::DebugKeyPressed(fwClothDebug::sm_ConfirmSelectionKey)  )
		{
			clothManager::ms_PedSkeleton = (const crSkeleton*)pEntity->GetSkeleton();
			clothManager::ms_EnablePedPick = false;
	
#if CLOTH_INSTANCE_FROM_DATA
			clothManager* pClothManager = CPhysics::GetClothManager();
			Assert( pClothManager );
			pClothManager->m_AttachToDrawableCB = clothBankManager::AttachToDrawableCallback;
			pClothManager->m_DetachFromDrawableCB = clothBankManager::DetachFromDrawableCallback;
#endif

			pLastPickedPed = pEntity;
		}
	}
}

void clothBankManager::PickVehicle()
{
	if( clothManager::ms_EnableVehiclePick )
	{
		Vector3 l_Offset = Vector3( 0.0f, 0.0f, 0.0f );
		s32 iComp = s32(NULL);
		CEntity* pEntity = CDebugScene::GetEntityUnderMouse( &l_Offset, &iComp, ArchetypeFlags::GTA_VEHICLE_TYPE );
		if( pEntity && pEntity->GetIsPhysical() && ioMapper::DebugKeyPressed(fwClothDebug::sm_ConfirmSelectionKey)  )
		{
			Assert( pEntity->GetCurrentPhysicsInst() );
			clothManager::ms_VehicleFrame = (const Mat34V*)&(pEntity->GetCurrentPhysicsInst()->GetMatrix());

			clothManager::ms_EnableVehiclePick = false;
		}
	}
}

bkBank* ropeBankManager::GetBank()
{
	return ropeBank;
}

ropeBankManager::ropeBankManager()
	: m_RopeManager(NULL)
	, m_pLastSelected(NULL)
	, m_MaxRopeLen(5.0f)
	, m_MinRopeLen(0.5f)
	, m_RopeLen(5.0f)
	, m_LenA(2.5f)
	, m_LenB(2.5f)
	, m_RopeLenChangeRate(0.5f)
	, m_Selection(false)
	, m_CreateEnabled(false)
	, m_UseDefaultPosRopeStart(false)
	, m_UseDefaultPosRopeEnd(false)
	, m_HasCollision(false)
	, m_ComponentPartA(0)
	, m_ComponentPartB(0)
	, m_NumSegments(6)
	, m_NumIterations(DEFAULT_ROPE_ITERATIONS)
	, m_WorldPos(Vector3(Vector3::ZeroType))
	, m_AttachAtPos(false)
	, m_PPUonly(false)
	, m_LockFromFront(true)
	, m_TimeMultiplier(1.0f)
	, m_Friction(1.0f)
	, m_Breakable(false)
	, m_Pinned(true)
	, m_RopeTypeIndex(0)
{
	s_BoneNameA[0] = 0;
	s_BoneNameB[0] = 0;
}

ropeBankManager::~ropeBankManager()
{
	AssertMsg(m_RopeList.GetHead()==NULL , "Leaked a ropeInstance in the active list");
}

void ropeBankManager::AddBank()
{
	ropeBankManager* bankMgr = CPhysics::GetRopeBankManager();
	Assert( bankMgr );

	ropeBank->Remove( *(bkWidget*)addBankButton );
	addBankButton = NULL;

	ropeDataManager::LoadRopeTexures();

	CPhysics::GetRopeManager()->AddWidgets(ropeBank);

	int ropeTypesCount = ropeDataManager::GetTypeDataCount();
	for( int i = 0; i < ropeTypesCount; ++i )
	{
		s_RopeTypeNames[i] = ropeDataManager::GetRopeDataMgr()->GetTypeDataName(i);
	}

	ropeBank->AddCombo( "Rope Types", &bankMgr->m_RopeTypeIndex, ropeTypesCount, s_RopeTypeNames );

	ropeBank->AddToggle( "Create rope", &bankMgr->m_CreateEnabled );
	ropeBank->AddToggle( "Attach to Entity", &bankMgr->m_Selection );
	ropeBank->AddToggle( "Use Default Entity Position Rope Start", &bankMgr->m_UseDefaultPosRopeStart );
	ropeBank->AddToggle( "Use Default Entity Position Rope End", &bankMgr->m_UseDefaultPosRopeEnd );

	ropeBank->AddToggle( "Attach at world pos", &bankMgr->m_AttachAtPos );
	ropeBank->AddSlider( "World pos", &bankMgr->m_WorldPos, 
		VEC3V_TO_VECTOR3(Vec3VFromF32(-100000.0f)), 
		VEC3V_TO_VECTOR3(Vec3VFromF32( 100000.0f)), 
		VEC3V_TO_VECTOR3(Vec3V(V_FLT_SMALL_5)) );


	ropeBank->AddToggle( "Pick Edges", &bankMgr->m_DebugPickEdges );
	ropeBank->AddToggle( "Enable rope collision", &bankMgr->m_HasCollision );
	ropeBank->AddToggle( "Rope on PPU ONLY", &bankMgr->m_PPUonly );
	ropeBank->AddToggle( "Lock rope from front", &bankMgr->m_LockFromFront );
	ropeBank->AddToggle( "Break rope from gunshot", &bankMgr->m_Breakable );
	ropeBank->AddToggle( "Pin rope by default", &bankMgr->m_Pinned );	

	ropeBank->AddSlider( "Rope friction", &bankMgr->m_Friction, 0.0f, 1.0f, bankMgr->m_Friction, datCallback( MFA( ropeBankManager::SetFrictionCB ), bankMgr) );
	ropeBank->AddSlider( "Rope weight scale", &bankMgr->m_TimeMultiplier, 0.001f, 20.0f, bankMgr->m_TimeMultiplier );
	ropeBank->AddSlider( "Rope Max Length", &bankMgr->m_MaxRopeLen, 0.1f, 100.0f, bankMgr->m_MaxRopeLen );
	ropeBank->AddSlider( "Rope Min Length", &bankMgr->m_MinRopeLen, 0.1f, 100.0f, bankMgr->m_MinRopeLen );	
	ropeBank->AddSlider( "Rope Length", &bankMgr->m_RopeLen, 0.1f, 100.0f, bankMgr->m_RopeLen );
	ropeBank->AddSlider( "Rope A Length", &bankMgr->m_LenA, 0.1f, 100.0f, bankMgr->m_LenA );
	ropeBank->AddSlider( "Rope B Length", &bankMgr->m_LenB, 0.1f, 100.0f, bankMgr->m_LenB );
	ropeBank->AddSlider( "Rope Length Change Rate", &bankMgr->m_RopeLenChangeRate, 0.1f, 10.0f, bankMgr->m_RopeLenChangeRate );	
	ropeBank->AddSlider( "Component part A", &bankMgr->m_ComponentPartA, 0, 100, 1 );
	ropeBank->AddSlider( "Component part B", &bankMgr->m_ComponentPartB, 0, 100, 1 );
	ropeBank->AddSlider( "Number of rope segments", &bankMgr->m_NumSegments, 0, 100, 1 );
	ropeBank->AddSlider( "Number of iterations", &bankMgr->m_NumIterations, 1, 100, 1 );
	ropeBank->AddText( "Bone Name part A", s_BoneNameA, BONE_NAME_LEN, false );
	ropeBank->AddText( "Bone Name part B", s_BoneNameB, BONE_NAME_LEN, false );

	clearButton = ropeBank->AddButton( "Press to clear end pos", ropeBankManager::ClearEndPos );
	Assert( clearButton );

	reloadButton = ropeBank->AddButton( "Reload all ropes", ropeBankManager::ReloadAllNodes );
	Assert( reloadButton );

	ropeDataManager::AddWidgets(*ropeBank);

	bankMgr->LoadFromRopeManager();
}

void ropeBankManager::SetFrictionCB()
{
	// set rope friction to the ropemanager, not the material
}

void ropeBankManager::LoadFromRopeManager()
{
	Assert( m_RopeManager );
	rage::atDNode<ropeInstance*>* node = m_RopeManager->GetActiveList()->GetHead();
	while( node )
	{
		ropeInstance* rope = (ropeInstance*)node->Data;
		Assert( rope );

		bkGroup* ropeGroup = rope->m_RAGGroup;
		if( ropeGroup )
		{
			ropeBank->DeleteGroup( *ropeGroup );
		}

		phInst* attachedA = rope->GetInstanceA(); 
		if( !attachedA )
			attachedA = rope->GetAttachedTo();
		AddRopeGroup( rope, attachedA, rope->GetInstanceB() );
		node = node->GetNext();
	}
}

void ropeBankManager::InitWidgets(ropeManager* ptr)
{
	Assert(ptr);
	m_RopeManager = ptr;

	ropeBank = &BANKMGR.CreateBank("Rope Management");
	addBankButton = ropeBank->AddButton( "Create Widgets", ropeBankManager::AddBank );
}

void ropeBankManager::DestroyWidgets()
{
	if( ropeBank )
	{
		BANKMGR.DestroyBank( *ropeBank );
		ropeBank = 0;
	}
}

void ropeBankManager::DetachFromObjectB( void* data )
{
	Assert( data );
	atDNode<RopeBankData> *node = (atDNode<RopeBankData>*)data;
	ropeInstance* rope = ((RopeBankData*)&node->Data)->rope;
	Assert( rope );
	bkButton* button = ((RopeBankData*)&node->Data)->button;
	Assert( button );

	phInst* inst	= ((RopeBankData*)&node->Data)->instB;
	Assert( inst );
	rope->DetachFromObject( inst );

	ropeBank->Remove( *(bkWidget*)button );

	m_RopeList.PopNode( *node );
	delete node;
}

void ropeBankManager::DetachFromObjectA( void* data )
{
	Assert( data );
	atDNode<RopeBankData> *node = (atDNode<RopeBankData>*)data;
	ropeInstance* rope = ((RopeBankData*)&node->Data)->rope;
	Assert( rope );
	bkButton* button = ((RopeBankData*)&node->Data)->button;
	Assert( button );

	phInst* inst	= ((RopeBankData*)&node->Data)->instA;
	Assert( inst );
	rope->DetachFromObject( inst );

	ropeBank->Remove( *(bkWidget*)button );

	m_RopeList.PopNode( *node );
	delete node;
}

void ropeBankManager::DeleteAllNodes( ropeInstance* rope )
{
	// go through all nodes and delete those sharing this rope
	bool searchAgain;
	do 
	{
		searchAgain = false;
		for (atDNode<RopeBankData> *node = m_RopeList.GetHead(); node; node=node->GetNext())
		{
			if( ((RopeBankData*)&node->Data)->rope == rope )
			{
				ropeBank->Remove( *(bkWidget*)(((RopeBankData*)&node->Data)->button) );
				m_RopeList.PopNode( *node );
				delete node;
				searchAgain = true;
				break;
			}
		}
	} while ( searchAgain );
}

void ropeBankManager::ReloadAllNodes()
{
	Assert( ropeBank );
	rage::atDList<ropeBankManager::RopeBankData>& nodeList = CPhysics::GetRopeBankManager()->GetRopeList();
	atDNode<RopeBankData> *node = nodeList.GetHead();
	while( node )
	{
		ropeBank->Remove( *(bkWidget*)(((ropeBankManager::RopeBankData*)&node->Data)->button) );

		nodeList.PopNode( *node );
		delete node;		
		node = nodeList.GetHead();
	}

	CPhysics::GetRopeBankManager()->LoadFromRopeManager();
}

void ropeBankManager::DeleteRope( void* data )
{
	Assert( data );
	atDNode<RopeBankData> *node = (atDNode<RopeBankData>*)data;
	ropeInstance* rope = ((RopeBankData*)&node->Data)->rope;
	Assert( rope );

	bkGroup* ropeGroup = rope->m_RAGGroup;
	Assert( ropeGroup );

	bkButton* button = 	((RopeBankData*)&node->Data)->button;
	Assert( button );

	ropeBank->Remove( *(bkWidget*)button );
	m_RopeManager->RemoveRope( rope );

	m_RopeList.PopNode( *node );

	DeleteAllNodes( rope );

	delete node;

	ropeBank->DeleteGroup( *ropeGroup );
}

void ropeBankManager::AttachToArray( void* data )
{
	Assert( data );
	atDNode<RopeBankData> *node = (atDNode<RopeBankData>*)data;
	ropeInstance* rope = ((RopeBankData*)&node->Data)->rope;
	Assert( rope );

	//  NOTE: this is the last rope start

	static float constraintRateChange = 1.0f;

	rope->AttachObjectsToConstraintArray( s_RopeStart, VECTOR3_TO_VEC3V(s_Offset), ((RopeBankData*)&node->Data)->instA,  m_pLastSelected->GetCurrentPhysicsInst(), 0, 0, m_RopeLen, constraintRateChange );
}


void ropeBankManager::StartWinding( void* data )
{
	Assert( data );
	atDNode<RopeBankData> *node = (atDNode<RopeBankData>*)data;
	ropeInstance* rope = ((RopeBankData*)&node->Data)->rope;
	Assert( rope );
	rope->StartWindingFront();	
}

#if !__RESOURCECOMPILER
void ropeBankManager::Load( void* data )
{
	Assert( data );
	atDNode<RopeBankData> *node = (atDNode<RopeBankData>*)data;
	ropeInstance* rope = ((RopeBankData*)&node->Data)->rope;
	Assert( rope );
	rope->Load( "ropeDebug" );	
}

void ropeBankManager::Save( void* data )
{
	Assert( data );
	atDNode<RopeBankData> *node = (atDNode<RopeBankData>*)data;
	ropeInstance* rope = ((RopeBankData*)&node->Data)->rope;
	Assert( rope );
	rope->Save( "ropeDebug" );	
}
#endif // !__RESOURCECOMPILER

void ropeBankManager::ToggleDebugInfo( void* data )
{
	Assert( data );
	atDNode<RopeBankData> *node = (atDNode<RopeBankData>*)data;
	ropeInstance* rope = ((RopeBankData*)&node->Data)->rope;
	Assert( rope );
	rope->ToggleDebugInfo();	
}

void ropeBankManager::StopWinding( void* data )
{
	Assert( data );
	atDNode<RopeBankData> *node = (atDNode<RopeBankData>*)data;
	ropeInstance* rope = ((RopeBankData*)&node->Data)->rope;
	Assert( rope );
	rope->StopWindingFront();	
}

void ropeBankManager::StartUnwindingFront( void* data )
{
	Assert( data );
	atDNode<RopeBankData> *node = (atDNode<RopeBankData>*)data;
	ropeInstance* rope = ((RopeBankData*)&node->Data)->rope;
	Assert( rope );
	rope->StartUnwindingFront();
}

void ropeBankManager::StopUnwindingFront( void* data )
{
	Assert( data );
	atDNode<RopeBankData> *node = (atDNode<RopeBankData>*)data;
	ropeInstance* rope = ((RopeBankData*)&node->Data)->rope;
	Assert( rope );
	rope->StopUnwindingFront();
}

void ropeBankManager::StartUnwindingBack( void* data )
{
	Assert( data );
	atDNode<RopeBankData> *node = (atDNode<RopeBankData>*)data;
	ropeInstance* rope = ((RopeBankData*)&node->Data)->rope;
	Assert( rope );
	rope->StartUnwindingBack();
}

void ropeBankManager::StopUnwindingBack( void* data )
{
	Assert( data );
	atDNode<RopeBankData> *node = (atDNode<RopeBankData>*)data;
	ropeInstance* rope = ((RopeBankData*)&node->Data)->rope;
	Assert( rope );
	rope->StopUnwindingBack();
}

void ropeBankManager::ToggleRopeCollision( void* data )
{
	Assert( data );
	atDNode<RopeBankData> *node = (atDNode<RopeBankData>*)data;
	ropeInstance* rope = ((RopeBankData*)&node->Data)->rope;
	Assert( rope );

	int typeFlags, includeFlags;
	rope->GetPhysInstFlags( typeFlags, includeFlags );

	if( typeFlags || includeFlags )
	{
		rope->SetPhysInstFlags( 0, 0 );		
	}
	else
	{
		rope->SetPhysInstFlags( ArchetypeFlags::GTA_OBJECT_TYPE, ArchetypeFlags::GTA_OBJECT_INCLUDE_TYPES & (~ArchetypeFlags::GTA_VEHICLE_TYPE) );
	}
}

void ropeBankManager::BreakRope( void* data )
{
	Assert( data );
	atDNode<RopeBankData> *node = (atDNode<RopeBankData>*)data;
	ropeInstance* rope = ((RopeBankData*)&node->Data)->rope;
	Assert( rope );

	ropeInstance* ropeA = NULL;
	ropeInstance* ropeB = NULL;
	m_RopeManager->BreakRope( rope, ropeA, ropeB, m_LenA, m_LenB, m_MinRopeLen, m_NumSegments );

	Assert( ropeA );		// technically this should never happen
	Assert( ropeB );		// technically this should never happen

	AddRopeGroup( ropeA, rope->GetInstanceA(), rope->GetInstanceB() );
	AddRopeGroup( ropeB, rope->GetInstanceA(), rope->GetInstanceB() );

	DeleteRope( data );
}


void ropeBankManager::Update(float /*elapsed*/)
{
	if( m_DebugPickEdges )
	{
		Assert( m_RopeManager );
		rage::atDNode<ropeInstance*>* node = m_RopeManager->GetActiveList()->GetHead();
		while( node )
		{
			ropeInstance* rope = (ropeInstance*)node->Data;
			Assert( rope );			
			m_RopeManager->PickEdges( Vector3(Vector3::ZeroType), rope->GetEnvCloth()->GetCloth() );
			node = node->GetNext();
		}		
	}

	if( !m_CreateEnabled || !(ioMapper::DebugKeyDown(KEY_CONTROL)) )
		return;

	bool ropeAdded = false;
	ropeInstance* rope = NULL;
	phInst* entityStart = NULL;

	if( ioMouse::GetPressedButtons() & ioMouse::MOUSE_LEFT )
	{
		iClicked |= ioMouse::MOUSE_LEFT;
	}

	if( ioMouse::GetPressedButtons() & ioMouse::MOUSE_RIGHT )
	{
		iClicked |= ioMouse::MOUSE_RIGHT;
	}

	if( ropeBankManager::m_Selection )
	{
		PF_DRAW_SPHERE_COLOR(Ropes, 0.1f, ropeEnd, Color32(1.0f, 1.0f, 0.0f, 1.0f));

		CEntity* pEntity = CDebugScene::GetEntityUnderMouse( &s_Offset, &iComponent, ArchetypeFlags::GTA_ALL_TYPES_MOVER|ArchetypeFlags::GTA_RAGDOLL_TYPE|ArchetypeFlags::GTA_PROJECTILE_TYPE);
		if(pEntity && pEntity->GetIsPhysical())
		{
			m_pLastSelected = pEntity;

			const Vector3 vLastSelectedPosition = VEC3V_TO_VECTOR3(m_pLastSelected->GetTransform().GetPosition());
			Vector3 localOffset = s_Offset - vLastSelectedPosition;

			entityStart = m_pLastSelected->GetCurrentPhysicsInst();

			char buff[256];
			sprintf( buff, " x: %f, y: %f, z: %f, 0x%p", localOffset.x, localOffset.y, localOffset.z, entityStart );
			grcDebugDraw::Text( s_Offset, Color32(1.0f, 1.0f, 1.0f), 0,0, buff );

			PF_DRAW_SPHERE_COLOR(Ropes, 0.1f, s_Offset, Color32(1.0f, 1.0f, 0.0f, 1.0f));

			if( (ioMouse::GetReleasedButtons() & ioMouse::MOUSE_RIGHT) && (iClicked & ioMouse::MOUSE_RIGHT) )
			{				
				ropeEnd = s_Offset;
				if( m_UseDefaultPosRopeEnd )
					ropeEnd = vLastSelectedPosition;
				entityEnd = entityStart;

				pEntityEnd = m_pLastSelected;
			}

			if( (ioMouse::GetReleasedButtons() & ioMouse::MOUSE_LEFT) && (iClicked & ioMouse::MOUSE_LEFT) )
			{
				Vec3V rot( 0.0f, 0.0f, -0.5f*PI );

				s_RopeStart = VECTOR3_TO_VEC3V(s_Offset);
				if( m_UseDefaultPosRopeStart )
					s_RopeStart = m_pLastSelected->GetTransform().GetPosition();

				rope = m_RopeManager->AddRope( s_RopeStart, rot, m_RopeLen, m_MinRopeLen, m_MaxRopeLen, m_RopeLenChangeRate, m_RopeTypeIndex, m_NumSegments, m_PPUonly, m_LockFromFront, m_TimeMultiplier, m_Breakable, m_Pinned, m_NumIterations );
				Assert( rope );						
				rope->SetNewUniqueId();

				if( entityEnd )
				{
					int endVertex = rope->GetEnvCloth()->GetNumRopeVertices()-1;
					rope->Pin( endVertex, VECTOR3_TO_VEC3V(ropeEnd) );

					Assert( m_pLastSelected );
					Assert( pEntityEnd );

					int notEmptyStringA = strcmp(s_BoneNameA,"");
					int notEmptyStringB = strcmp(s_BoneNameB,"");

					const crSkeleton* pSkelPartA = ( notEmptyStringA ? m_pLastSelected->GetSkeleton() : NULL);
					const crSkeleton* pSkelPartB = ( notEmptyStringB ? pEntityEnd->GetSkeleton() : NULL);

					if( notEmptyStringA )
						s_RopeStart = Vec3V(V_ZERO);

					if( notEmptyStringB )
						ropeEnd.Zero();

					rope->AttachObjects( s_RopeStart, VECTOR3_TO_VEC3V(ropeEnd), entityStart, entityEnd, m_ComponentPartA, m_ComponentPartB, m_MaxRopeLen, 0.0f, 1.0f, 1.0f, true, pSkelPartA, pSkelPartB, notEmptyStringA ? s_BoneNameA: NULL, notEmptyStringB ? s_BoneNameB: NULL );
				}
				else
				{
					int notEmptyStringA = strcmp(s_BoneNameA,"");
					const crSkeleton* skel = notEmptyStringA ? (pEntity->GetIsTypePed() ? (static_cast<CEntity*>(pEntity))->GetSkeleton(): NULL) : NULL;

					if( notEmptyStringA )
						s_RopeStart = Vec3V(V_ZERO);

					rope->AttachToObject( s_RopeStart, entityStart, m_ComponentPartA, skel, notEmptyStringA ? s_BoneNameA: NULL );
				}

				if( m_HasCollision )
					rope->SetPhysInstFlags( ArchetypeFlags::GTA_OBJECT_TYPE, ArchetypeFlags::GTA_OBJECT_INCLUDE_TYPES & (~ArchetypeFlags::GTA_VEHICLE_TYPE) );

				ropeAdded = true;
				iClicked = 0;
			}

		}
	}
	else
	{
		PF_DRAW_SPHERE_COLOR(Ropes, 0.1f, ropeEnd, Color32(1.0f, 1.0f, 0.0f, 1.0f));

		Vector3	Pos;

		if( m_AttachAtPos )
		{
			Pos = m_WorldPos;
		}
		else
		{
			CDebugScene::GetWorldPositionUnderMouse(Pos, ArchetypeFlags::GTA_OBJECT_TYPE|ArchetypeFlags::GTA_PICKUP_TYPE|ArchetypeFlags::GTA_MAP_TYPE_WEAPON, NULL, NULL, false);
		}

		if( (ioMouse::GetReleasedButtons() & ioMouse::MOUSE_RIGHT) && (iClicked & ioMouse::MOUSE_RIGHT) )
		{				
			ropeEnd = Pos;
		}

		if( (ioMouse::GetReleasedButtons() & ioMouse::MOUSE_LEFT) && (iClicked & ioMouse::MOUSE_LEFT) )
		{
			Vec3V rot( 0.0f, 0.0f, -0.5f*PI );
			rope = m_RopeManager->AddRope( VECTOR3_TO_VEC3V(Pos), rot, m_RopeLen, m_MinRopeLen, m_MaxRopeLen, m_RopeLenChangeRate, m_RopeTypeIndex, m_NumSegments, m_PPUonly, m_LockFromFront, m_TimeMultiplier, m_Breakable, m_Pinned );
			Assert( rope );
			rope->SetNewUniqueId();

			if( ropeEnd.x < BIG_FLOAT )
			{
				int endVertex = rope->GetEnvCloth()->GetNumRopeVertices()-1;
				rope->Pin( endVertex, VECTOR3_TO_VEC3V(ropeEnd) );
			}

			ClearEndPos();

			if( m_HasCollision )
				rope->SetPhysInstFlags( ArchetypeFlags::GTA_OBJECT_TYPE, ArchetypeFlags::GTA_OBJECT_INCLUDE_TYPES & (~ArchetypeFlags::GTA_VEHICLE_TYPE) );

			ropeAdded = true;
			iClicked = 0;
		}
	}

	if( ropeAdded )
	{
		Assert(rope);
		AddRopeGroup( rope, entityStart, entityEnd );
		entityEnd = NULL;
		pEntityEnd = NULL;
	}
}

void ropeBankManager::ClearEndPos()
{
	ropeEnd.x = ropeEnd.y = ropeEnd.z = BIG_FLOAT;
}

void ropeBankManager::AddRopeGroup( ropeInstance* pRope, phInst* instA, phInst* instB )
{
	if( addBankButton ) 
		return;

	Assert( pRope );
	char groupTag[256];
	sprintf( groupTag, "Rope: 0x%p", pRope );
	pRope->m_RAGGroup = ropeBank->PushGroup( groupTag ,false );

	//ragGroup->AddToggle("Pick Verts", &cInstance->m_DebugPickVerts, datCallback( MFA1( clothManager::ResetSelection0 ), this, cInstance ), "Activate vertex/vertices selection for this cloth." );
	pRope->m_RAGGroup->AddToggle("Pick Verts", &pRope->m_DebugPickVerts );

	AddButton( "Delete rope", pRope, instA, instB, MFA1(ropeBankManager::DeleteRope) );

	if( instA )
	{
		char buttonTitleA[256];
		sprintf( buttonTitleA, "Detach rope from object: 0x%p", instA );
		AddButton( buttonTitleA, pRope, instA, instB, MFA1(ropeBankManager::DetachFromObjectA) );
	}
	if( instB )
	{
		char buttonTitleB[256];
		sprintf( buttonTitleB, "Detach rope from object: 0x%p", instB );
		AddButton( buttonTitleB, pRope, instA, instB, MFA1(ropeBankManager::DetachFromObjectB) );
	}

	AddButton( "Start winding", pRope, instA, instB, MFA1(ropeBankManager::StartWinding) );
	AddButton( "Stop winding", pRope, instA, instB, MFA1(ropeBankManager::StopWinding) );
	AddButton( "Start unwinding from front", pRope, instA, instB, MFA1(ropeBankManager::StartUnwindingFront) );
	AddButton( "Stop unwinding from front", pRope, instA, instB, MFA1(ropeBankManager::StopUnwindingFront) );

	if( instA && instB )
	{
		AddButton( "Start unwinding from back", pRope, instA, instB, MFA1(ropeBankManager::StartUnwindingBack) );
		AddButton( "Stop unwinding from back", pRope, instA, instB, MFA1(ropeBankManager::StopUnwindingBack) );
	}

	if( instA && !instB )
	{
		AddButton( "Attach in array last selected", pRope, instA, instB, MFA1(ropeBankManager::AttachToArray) );
	}

	AddButton( "Break rope", pRope, instA, instB, MFA1(ropeBankManager::BreakRope) );
	AddButton( "Debug Info On/Off", pRope, instA, instB, MFA1(ropeBankManager::ToggleDebugInfo) );

	AddButton( "Load", pRope, instA, instB, MFA1(ropeBankManager::Load) );
	AddButton( "Save", pRope, instA, instB, MFA1(ropeBankManager::Save) );

	ropeBank->PopGroup();
}

void ropeBankManager::AddButton( const char* buttonText, ropeInstance* rope, phInst* instA, phInst* instB, rage::Member1 cbaddress )
{
	atDNode<RopeBankData> *nodeInfo = rage_new atDNode<RopeBankData>;
	Assert(nodeInfo);

	RopeBankData* pinfo = &nodeInfo->Data;
	pinfo->rope = rope;

	bkButton* ropeButton = ropeBank->AddButton( buttonText, datCallback( cbaddress, this, (CallbackData)nodeInfo, false ) );
	Assert(ropeButton);
	pinfo->button = ropeButton;
	pinfo->instA = instA;
	pinfo->instB = instB;

	m_RopeList.Append( *nodeInfo );
}



} // namespace rage

#endif // __BANK ... etc

