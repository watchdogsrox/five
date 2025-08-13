#if !__FINAL

#include "blockview.h"
#include "atl/array.h"
#include "atl/string.h"
#include "bank/bkmgr.h"
#include "bank/bank.h"
#include "bank/bank.h"
#include "bank/combo.h"
#include "grcore/debugdraw.h"

#include "camera/CamInterface.h"
#include "camera/debug/DebugDirector.h"
#include "camera/helpers/Frame.h"
#include "peds/PlayerInfo.h"
#include "scene/world/gameWorld.h"
#include "script/script.h"
#include "fwscene/mapdata/mapdatadebug.h"
#include "fwdebug/picker.h"

#define TIME_TO_UPDATE_BLOCKS	(10000)  // every 10 seconds should be enough for the update

struct CBlock
{
	static const int MAX_POINTS = 4;

	atString name;
	atString user;
	atString owner;
	atString time;
	s32 flags;
	s32 numPoints;
	Vector3 points[MAX_POINTS];
	Vector3 min;
	Vector3 max;
};

struct CUserInfo
{
	const char* name;
	atArray<s32> blockList;
	s32 currentIdx;
};

// info about the root of a lod hierarchy, to allow filtering by lod tree
class CLodTreeRootInfo
{
public:
	CEntity*	m_pRootEntity;
	u32			m_nHierarchyIndex;
};

#define NAME_LENGTH 128
#define BLOCKVIEW_MAX_BLOCKS (1400)

DEBUG_DRAW_ONLY(static u32 s_TimeStepCheck);
static s32 s_blockSorts[BLOCKVIEW_MAX_BLOCKS];
static CBlock* s_blocks;
static atArray<const char*> s_blockNames;
static atArray<const char*> s_lodTreeNames;
static atArray<CLodTreeRootInfo> s_lodTreeRootInfo;
static s32 s_numBlocks(0);
static s32 s_currentBlock(0);
static s32 s_currentBlockInside(0);
static s32 s_currentBlockNameIdx(0);
#if __BANK
static s32 s_currentLodTree(0);
#endif
static s32 s_numBlockNames(0);

static bool bValidBlockList = true;
static atArray<const char*> s_users;
static CUserInfo *s_userInfo;
static s32 s_numUsers(0);
static char s_currentBlockName[NAME_LENGTH];
static s32 s_currentUser;
static bool s_IsolateBlock(false);
static bool s_IsolateLodTree(false);
#if __BANK
static bool s_IsolateArtist(false);
static bool s_IsolateDates(false);
#endif
static bool s_DisplayInfo(false);
DEBUG_DRAW_ONLY(static bool s_DisplayPlayerBlock(true));

static bool s_FilterArtist(false);
static bool s_Filter1stPass(false);
static bool s_Filter2ndPassStarted(false);
static bool s_Filter2ndPassOutsourced(false);
static bool s_Filter2ndPassComplete(false);
static bool s_Filter3rdPass(false);
static bool s_FilterComplete(false);

#if __BANK
static s32 s_StartYear(2005);
static s32 s_StartMonth(1);
static s32 s_StartDay(1);
static s32 s_EndYear(2005);
static s32 s_EndMonth(1);
static s32 s_EndDay(1);
#endif

#if __BANK
static bkCombo* s_BlockCombo(NULL);
#endif

#if __DEV
PARAM(NoDisplayBlockInfo,"");
#endif // __DEV

////////////////////////////////////////////////////////////////////////////
// name:	Initialise
// purpose: 
// params:  none
// returns: nothing
////////////////////////////////////////////////////////////////////////////
void CBlockView::Init(unsigned /*initMode*/)
{
	USE_DEBUG_MEMORY();
	RAGE_TRACK(Debug);
	DEBUG_DRAW_ONLY(s_TimeStepCheck = TIME_TO_UPDATE_BLOCKS);
	Assert(!s_blocks);
	s_blocks = rage_new CBlock[BLOCKVIEW_MAX_BLOCKS];
	Assert(!s_userInfo);
	s_userInfo = rage_new CUserInfo[BLOCKVIEW_MAX_BLOCKS];

	for(s32 i=0;i<BLOCKVIEW_MAX_BLOCKS;i++)
	{
		s_blocks[i].name = NULL;
	}
	s_numBlocks = 0;
	s_numUsers = 0;

	s_lodTreeRootInfo.Reset();

#if __DEV
	s_DisplayInfo = !PARAM_NoDisplayBlockInfo.Get();
#endif // __DEV
}

////////////////////////////////////////////////////////////////////////////
// name:	Shutdown
// purpose: 
// params:  none
// returns: nothing
////////////////////////////////////////////////////////////////////////////
void CBlockView::Shutdown(unsigned /*shutdownMode*/)
{
	delete[] s_userInfo;
	s_userInfo = NULL;
	delete[] s_blocks;
	s_blocks = NULL;

	s_users.Reset();
	s_blockNames.Reset();
	s_lodTreeRootInfo.Reset();
}

s32 CBlockView::GetCurrentBlockInside(const Vector3& r_Pos)
{
	fwRect blockRect;
	float minarea = -1.0f;
	s32 retblock = 0;
	Vector2 testPos(r_Pos.x, r_Pos.y);

	for (s32 block_count = 0; block_count < s_numBlocks; block_count++)
	{
		blockRect.Init();
		for (u32 i=0; i<GetNumPoints(block_count); i++)
		{
			blockRect.Add(GetPoint(block_count, i).x, GetPoint(block_count, i).y);
		}
		Assertf(!blockRect.IsInvalidOrZeroArea(), "BlockView: invalid block specified for %s", GetBlockName(block_count));

		if (!blockRect.IsInvalidOrZeroArea() && blockRect.IsInside(testPos))
		{
			float area = blockRect.GetWidth() * blockRect.GetHeight();
			if(minarea == -1.0f || area < minarea)
			{
				retblock = block_count;
				minarea = area;
			}
		}
	}
	return retblock;
}

////////////////////////////////////////////////////////////////////////////
// name:	Update
// purpose: 
// params:  none
// returns: nothing
////////////////////////////////////////////////////////////////////////////
void CBlockView::Update()
{
	if(s_numBlocks==0)
		return;

#if DEBUG_DRAW
	if(s_DisplayPlayerBlock)
	{
		if (s_TimeStepCheck >= TIME_TO_UPDATE_BLOCKS)  // only check once in a while...
		{
			s_TimeStepCheck = 0;

			CEntity* pEntity = (CEntity*) g_PickerManager.GetSelectedEntity();
			if (pEntity)
			{
				if(pEntity->GetTransformPtr())
					s_currentBlockInside = GetCurrentBlockInside(VEC3V_TO_VECTOR3(pEntity->GetTransformPtr()->GetPosition()));
			}
			else
			{
				camDebugDirector& debugDirector = camInterface::GetDebugDirector();

				if (debugDirector.IsFreeCamActive())
					s_currentBlockInside = GetCurrentBlockInside(debugDirector.GetFreeCamFrame().GetPosition());
				else
					s_currentBlockInside = GetCurrentBlockInside(CGameWorld::FindLocalPlayerCentreOfWorld());
			}
		}

		s_TimeStepCheck += fwTimer::GetTimeStepInMilliseconds();
	}
#endif // DEBUG_DRAW

	if (!s_DisplayInfo)
		return;

	// only display block selected stuff when a block is selected
#if __BANK
	bkBank* pBankFind = BANKMGR.FindBank("Blocks");

	if(pBankFind && pBankFind->AreAnyWidgetsShown())
	{
		char debugText[512];

		Vector2 vCurrentPoint[4];  // for the 4 points
		int c=0;
		vCurrentPoint[0] = Vector2(CBlockView::GetPoint(s_currentBlock, c).x,   CBlockView::GetPoint(s_currentBlock, c).y);
		vCurrentPoint[1] = Vector2(CBlockView::GetPoint(s_currentBlock, c+1).x, CBlockView::GetPoint(s_currentBlock, c+1).y);
		vCurrentPoint[2] = Vector2(CBlockView::GetPoint(s_currentBlock, c+2).x, CBlockView::GetPoint(s_currentBlock, c+2).y);
		vCurrentPoint[3] = Vector2(CBlockView::GetPoint(s_currentBlock, c+3).x, CBlockView::GetPoint(s_currentBlock, c+3).y);

		sprintf (debugText, "BLOCK SELECTED: %d - %s (%s) (%s) (a: %f,%f b: %f,%f c: %f,%f d:%f,%f)  ",
			s_currentBlock,
			(const char*)(s_blocks[s_currentBlock].name),
			(const char*)(s_blocks[s_currentBlock].user),
			(const char*)(s_blocks[s_currentBlock].owner),
			
			vCurrentPoint[0].x,vCurrentPoint[0].y,
			vCurrentPoint[1].x,vCurrentPoint[1].y,
			vCurrentPoint[2].x,vCurrentPoint[2].y,
			vCurrentPoint[3].x,vCurrentPoint[3].y
			);			


		if(s_blocks[s_currentBlock].flags & BLOCKFLAGS_FIRST_PASS)
			strcat (debugText, ": 1st pass");
		if(s_blocks[s_currentBlock].flags & BLOCKFLAGS_SECOND_PASS_STARTED)
			strcat (debugText, ": 2nd pass");
		if(s_blocks[s_currentBlock].flags & BLOCKFLAGS_SECOND_PASS_OUTSOURCED)
			strcat (debugText, ": 2nd pass outsourced");
		if(s_blocks[s_currentBlock].flags & BLOCKFLAGS_SECOND_PASS_COMPLETE)
			strcat (debugText, ": 2nd pass complete");
		if(s_blocks[s_currentBlock].flags & BLOCKFLAGS_THIRD_PASS_COMPLETE)
			strcat (debugText, ": 3rd pass complete");
		if(s_blocks[s_currentBlock].flags & BLOCKFLAGS_COMPLETE)
			strcat (debugText, ": complete");

		grcDebugDraw::AddDebugOutput(debugText);  // add all the text to the debug view

		if(s_DisplayPlayerBlock)
		{
			vCurrentPoint[0] = Vector2(CBlockView::GetPoint(s_currentBlockInside, c).x,   CBlockView::GetPoint(s_currentBlockInside, c).y);
			vCurrentPoint[1] = Vector2(CBlockView::GetPoint(s_currentBlockInside, c+1).x, CBlockView::GetPoint(s_currentBlockInside, c+1).y);
			vCurrentPoint[2] = Vector2(CBlockView::GetPoint(s_currentBlockInside, c+2).x, CBlockView::GetPoint(s_currentBlockInside, c+2).y);
			vCurrentPoint[3] = Vector2(CBlockView::GetPoint(s_currentBlockInside, c+3).x, CBlockView::GetPoint(s_currentBlockInside, c+3).y);

			sprintf (debugText, "BLOCK INSIDE: %d - %s (%s) (%s) (a: %f,%f b: %f,%f c: %f,%f d:%f,%f)  ",
				s_currentBlock,
				(const char*)(s_blocks[s_currentBlockInside].name),
				(const char*)(s_blocks[s_currentBlockInside].user),
				(const char*)(s_blocks[s_currentBlockInside].owner),
				vCurrentPoint[0].x,vCurrentPoint[0].y,
				vCurrentPoint[1].x,vCurrentPoint[1].y,
				vCurrentPoint[2].x,vCurrentPoint[2].y,
				vCurrentPoint[3].x,vCurrentPoint[3].y
				);

			if(s_blocks[s_currentBlockInside].flags & BLOCKFLAGS_FIRST_PASS)
				strcat (debugText, ": 1st pass");
			if(s_blocks[s_currentBlockInside].flags & BLOCKFLAGS_SECOND_PASS_STARTED)
				strcat (debugText, ": 2nd pass");
			if(s_blocks[s_currentBlockInside].flags & BLOCKFLAGS_SECOND_PASS_OUTSOURCED)
				strcat (debugText, ": 2nd pass outsourced");
			if(s_blocks[s_currentBlockInside].flags & BLOCKFLAGS_SECOND_PASS_COMPLETE)
				strcat (debugText, ": 2nd pass complete");
			if(s_blocks[s_currentBlockInside].flags & BLOCKFLAGS_THIRD_PASS_COMPLETE)
				strcat (debugText, ": 3rd pass complete");
			if(s_blocks[s_currentBlockInside].flags & BLOCKFLAGS_COMPLETE)
				strcat (debugText, ": complete");

			grcDebugDraw::AddDebugOutput(debugText);  // add all the text to the debug view
		}
	}
#endif // __BANK
}

////////////////////////////////////////////////////////////////////////////
int CbCompareBlocks(const void* pVoidA, const void* pVoidB)
{
	const s32* pA = (const s32*)pVoidA;
	const s32* pB = (const s32*)pVoidB;

	return stricmp(s_blocks[*pA].name,s_blocks[*pB].name);
}

////////////////////////////////////////////////////////////////////////////
// name:	InitLevelWidgets
// purpose: 
// params:  none
// returns: nothing
////////////////////////////////////////////////////////////////////////////
#if __BANK
void CBlockView::InitLevelWidgets()
{
	s32 i;

	if(s_numBlocks==0)
		return;

	bkBank* pBank = &BANKMGR.CreateBank("Blocks");

	if (AssertVerify(pBank))
	{
		for(i=0;i<s_numBlocks;i++)
		{		
			s_blockSorts[i] = i;
		}

		qsort(s_blockSorts,s_numBlocks,sizeof(s32),CbCompareBlocks);

		for(i=0;i<s_numBlocks;i++)
		{		
			s_blockNames.PushAndGrow(s_blocks[s_blockSorts[i]].name);
		}

		s_numBlockNames = s_numBlocks;

		for(i=0;i<s_numUsers;i++)
		{		
			s_users.PushAndGrow(s_userInfo[i].name);
		}

		s_lodTreeNames.Reset();

		for (i=0; i<s_lodTreeRootInfo.size(); i++)
		{
			CLodTreeRootInfo* pInfo = &s_lodTreeRootInfo[i];
			const char* pszName = pInfo->m_pRootEntity->GetBaseModelInfo()->GetModelName();
			s_lodTreeNames.PushAndGrow(pszName);
		}

		pBank->AddToggle("Display Info", &s_DisplayInfo);
		pBank->AddToggle("Display Info On Ped Pos", &s_DisplayPlayerBlock);

		pBank->AddToggle("Filter Artist", &s_FilterArtist, UpdateBlockList);

		pBank->AddToggle("Filter 1st Pass", &s_Filter1stPass, UpdateBlockList);
		pBank->AddToggle("Filter 2nd Pass Started", &s_Filter2ndPassStarted, UpdateBlockList);
		pBank->AddToggle("Filter 2nd Pass Outsourced", &s_Filter2ndPassOutsourced, UpdateBlockList);
		pBank->AddToggle("Filter 2nd Pass Complete", &s_Filter2ndPassComplete, UpdateBlockList);
		pBank->AddToggle("Filter 3rd Pass", &s_Filter3rdPass, UpdateBlockList);
		pBank->AddToggle("Filter Complete", &s_FilterComplete, UpdateBlockList);

		s_BlockCombo = pBank->AddCombo("Block", &s_currentBlockNameIdx, s_blockNames.GetCount(), &s_blockNames[0], UpdateCurrentBlock);
		pBank->AddButton("Warp to Selected",SelectBlock);
		pBank->AddText("Block Name", s_currentBlockName, NAME_LENGTH);
		pBank->AddButton("Warp to Name",SelectBlockByName);
		pBank->AddToggle("Isolate Block", &s_IsolateBlock);
		pBank->AddCombo("Artist", &s_currentUser, s_users.GetCount(), &s_users[0], UpdateBlockList);
		pBank->AddButton("Warp to Artist",SelectUser);
		pBank->AddButton("Next",SelectNext);
		pBank->AddButton("Previous",SelectPrevious);
		pBank->AddToggle("Isolate Artist", &s_IsolateArtist);
		if (s_lodTreeNames.GetCount())
		{
			pBank->AddToggle("Isolate Lod Tree", &s_IsolateLodTree);
			pBank->AddCombo("LodTree", &s_currentLodTree, s_lodTreeNames.GetCount(), &s_lodTreeNames[0]);
		}

		pBank->AddSlider("Start Year", &s_StartYear, 2004, 2010, 1);
		pBank->AddSlider("Start Month", &s_StartMonth, 1, 12, 1);
		pBank->AddSlider("Start Day", &s_StartDay, 1, 31, 1);
		pBank->AddSlider("End Year", &s_EndYear, 2004, 2010, 1);
		pBank->AddSlider("End Month", &s_EndMonth, 1, 12, 1);
		pBank->AddSlider("End Day", &s_EndDay, 1, 31, 1);
		pBank->AddToggle("Isolate Dates", &s_IsolateDates);
	}

	UpdateBlockList();
}

void CBlockView::ShutdownLevelWidgets()
{
	bkBank* pBankFind = BANKMGR.FindBank("Blocks");

	if (pBankFind)
		BANKMGR.DestroyBank(*pBankFind);
}
#endif // __BANK

bool CBlockView::IsCurrentUser(s32 index)
{
	if(stricmp(s_blocks[index].user,s_users[s_currentUser]) != 0)
		return true;
	else
		return false;
}

////////////////////////////////////////////////////////////////////////////
// name:	UpdateBlockList
// purpose: 
// params:  none
// returns: nothing
////////////////////////////////////////////////////////////////////////////
void CBlockView::UpdateBlockList()
{
	s_numBlockNames = 0;

	for(s32 i=0;i<s_numBlocks;i++)
	{
		if(s_Filter1stPass && !(s_blocks[i].flags & BLOCKFLAGS_FIRST_PASS))
			continue;
		if(s_Filter2ndPassStarted && !(s_blocks[i].flags & BLOCKFLAGS_SECOND_PASS_STARTED))
			continue;
		if(s_Filter2ndPassOutsourced && !(s_blocks[i].flags & BLOCKFLAGS_SECOND_PASS_OUTSOURCED))
			continue;
		if(s_Filter2ndPassComplete && !(s_blocks[i].flags & BLOCKFLAGS_SECOND_PASS_COMPLETE))
			continue;
		if(s_Filter3rdPass && !(s_blocks[i].flags & BLOCKFLAGS_THIRD_PASS_COMPLETE))
			continue;
		if(s_FilterComplete && !(s_blocks[i].flags & BLOCKFLAGS_COMPLETE))
			continue;

		if(s_FilterArtist)
		{
			if(stricmp(s_blocks[i].user,s_users[s_currentUser]) != 0)
				continue;
		}

		s_blockSorts[s_numBlockNames] = i;
		s_numBlockNames++;
	}

	qsort(s_blockSorts,s_numBlockNames,sizeof(s32),CbCompareBlocks);
	s_blockNames.Reset();

	for(s32 i=0;i<s_numBlockNames;i++)
	{		
		s_blockNames.PushAndGrow(s_blocks[s_blockSorts[i]].name);
	}

#if __BANK
	if(!s_BlockCombo)
		return;

	if(s_numBlockNames == 0)
	{
		s_currentBlockNameIdx = 0;
		s_BlockCombo->SetString(0, "N/A");
		s_BlockCombo->UpdateCombo("Block", &s_currentBlockNameIdx, 1, NULL, UpdateCurrentBlock);
	}
	else
	{
		s_currentBlock = s_blockSorts[0];

		for (int i = 0; i < s_numBlockNames; ++i)
		{
			s_BlockCombo->SetString(i, s_blockNames[i]);
		}

		if (!bValidBlockList)
		{
			s_BlockCombo->UpdateCombo("Block", &s_currentBlockNameIdx, s_numBlockNames, &s_blockNames[0], UpdateCurrentBlock);
		}

	}

	bValidBlockList = (s_numBlockNames > 0);
#endif
}

////////////////////////////////////////////////////////////////////////////
void CBlockView::UpdateCurrentBlock()
{
	s_currentBlock = s_blockSorts[s_currentBlockNameIdx];
}

////////////////////////////////////////////////////////////////////////////
int CbCompareIndices(const s32* pA, const s32* pB)
{
	return (*pA - *pB);
}

////////////////////////////////////////////////////////////////////////////
int CbCompareUsers(const void* pVoidA, const void* pVoidB)
{
	const CUserInfo* pA = (const CUserInfo*)pVoidA;
	const CUserInfo* pB = (const CUserInfo*)pVoidB;

	return stricmp(pA->name,pB->name);
}

void CBlockView::AddLodTree(CEntity* pRootEntity)
{
	if (pRootEntity)
	{
		CLodTreeRootInfo treeInfo;
		treeInfo.m_pRootEntity = pRootEntity;
		treeInfo.m_nHierarchyIndex = 0;
		s_lodTreeRootInfo.PushAndGrow(treeInfo);
	}
}

////////////////////////////////////////////////////////////////////////////
// name:	AddBlock
// purpose: 
// params:  none
// returns: nothing
////////////////////////////////////////////////////////////////////////////
//void CBlockView::AddBlock(const char* pName,const char* pUser,const char* pTime,s32 flags,s32 numPoints,const Vector3* points)
void CBlockView::AddBlock(const char* pName,const char* pExportedBy,const char* pOwner,const char* pTime,s32 flags,s32 numPoints,const Vector3* points)
{
	if (flags & BLOCKFLAGS_UNUSED)  // dont add any Unused blocks to the list... just return out
		return;

	if (!Verifyf(s_numBlocks < BLOCKVIEW_MAX_BLOCKS, "BlockView - Exceeded the maximum number of blocks - see a programmer!"))
		return;

	static s32 LastUser = -1;

	char actualname[128];
	safecpy(actualname,pName);
	s32 i = 0;

	s_blocks[s_numBlocks].name = actualname;
	s_blocks[s_numBlocks].user = pExportedBy;
	s_blocks[s_numBlocks].owner = pOwner;
	s_blocks[s_numBlocks].time = pTime;
	s_blocks[s_numBlocks].flags = flags;
	s_blocks[s_numBlocks].numPoints = 0;

	if((numPoints > 0) && points)
	{
		if(numPoints > CBlock::MAX_POINTS)
			numPoints = CBlock::MAX_POINTS;

		for(i=0;i<numPoints;i++)
		{
			s_blocks[s_numBlocks].points[i] = points[i];
		}

		s_blocks[s_numBlocks].numPoints = numPoints;
	}

	//do we need to add a new user
	bool found = false;

	if(LastUser != -1)
	{
		if(stricmp(s_userInfo[LastUser].name,pExportedBy) == 0)
		{
			 i = LastUser;
			 found = true;
		}
	}

	if(found == false)
	{
		for(i=0;i<s_numUsers;i++)
		{
			if(stricmp(s_userInfo[i].name,pExportedBy) == 0)
			{
				found = true;
				break;
			}
		}
	}

	if(found)
	{
		s_userInfo[i].blockList.PushAndGrow(s_numBlocks);
		s_userInfo[i].blockList.QSort(0,s_userInfo[i].blockList.GetCount(),CbCompareIndices);
	}
	else
	{
		s_userInfo[s_numUsers].name = s_blocks[s_numBlocks].user;
		s_userInfo[s_numUsers].currentIdx = 0;
		s_userInfo[s_numUsers].blockList.PushAndGrow(s_numBlocks);
		s_numUsers++;

		qsort(s_userInfo,s_numUsers,sizeof(CUserInfo),CbCompareUsers);
	}

	s_numBlocks++;
}

////////////////////////////////////////////////////////////////////////////
// name:	GetNumberOfBlocks
// purpose: 
// params:  none
// returns: nothing
////////////////////////////////////////////////////////////////////////////
s32 CBlockView::GetNumberOfBlocks()
{
	return s_numBlocks;
}

////////////////////////////////////////////////////////////////////////////
// name:	GetBlockName
// purpose: 
// params:  none
// returns: nothing
////////////////////////////////////////////////////////////////////////////
const char*	CBlockView::GetBlockName(s32 index)
{
	if (index < s_numBlocks)
	{
		return s_blocks[index].name;
	}
	return "";
}



////////////////////////////////////////////////////////////////////////////
// name:	GetOwnerName
// purpose: returns the owner name of the selected block
// params:  none
// returns: nothing
////////////////////////////////////////////////////////////////////////////
const char*	CBlockView::GetBlockOwner(s32 index)
{
	if (index < s_numBlocks)
	{
		return s_blocks[index].owner;
	}
	return "";
}



const char*	CBlockView::GetBlockUser(s32 index)
{
	if (index < s_numBlocks)
	{
		return s_blocks[index].user;
	}
	return "";
}

const char* CBlockView::GetTime(s32 index)
{
	Assert(index < s_numBlocks);

	return s_blocks[index].time;
}


s32 CBlockView::GetCurrentBlockSelected()
{
	return (s_currentBlock);
}


s32 CBlockView::GetCurrentBlockInside()
{
	return (s_currentBlockInside);
}



////////////////////////////////////////////////////////////////////////////
// name:	GetBlockFlags
// purpose: 
// params:  none
// returns: nothing
////////////////////////////////////////////////////////////////////////////
s32 CBlockView::GetBlockFlags(s32 index)
{
	Assert(index < s_numBlocks);

	return s_blocks[index].flags;
}

////////////////////////////////////////////////////////////////////////////
// name:	GetBlockPos
// purpose: 
// params:  none
// returns: nothing
////////////////////////////////////////////////////////////////////////////
void CBlockView::GetBlockPos(s32 index,Vector3& r_vecGet)
{
	Assert(index < s_numBlocks);

	r_vecGet = Vector3(0,0,0);

	for(s32 i=0;i<CBlockView::GetNumPoints(index);i++)
	{
		r_vecGet += CBlockView::GetPoint(index,i);
	}

	r_vecGet /= (float)CBlockView::GetNumPoints(index);
}

////////////////////////////////////////////////////////////////////////////
// name:	SelectBlock
// purpose: 
// params:  none
// returns: nothing
////////////////////////////////////////////////////////////////////////////
void CBlockView::SelectBlock()
{
	if (bValidBlockList)
	{
		WarpToCurrent();
	}
}

////////////////////////////////////////////////////////////////////////////
// name:	SelectBlockByName
// purpose: 
// params:  none
// returns: nothing
////////////////////////////////////////////////////////////////////////////
void CBlockView::SelectBlockByName()
{
	for(s32 i=0;i<s_numBlocks;i++)
	{
		if(stricmp(s_blocks[i].name,s_currentBlockName) == 0)
		{
			s_currentBlock = i;
			WarpToCurrent();
			return;
		}
	}
}

////////////////////////////////////////////////////////////////////////////
// name:	SelectUser
// purpose: 
// params:  none
// returns: nothing
////////////////////////////////////////////////////////////////////////////
void CBlockView::SelectUser()
{
	s_currentBlock = s_userInfo[s_currentUser].blockList[0];
	SelectBlock();
}

////////////////////////////////////////////////////////////////////////////
// name:	SelectNext
// purpose: 
// params:  none
// returns: nothing
////////////////////////////////////////////////////////////////////////////
void CBlockView::SelectNext()
{
	s_userInfo[s_currentUser].currentIdx++;

	if(s_userInfo[s_currentUser].currentIdx >= s_userInfo[s_currentUser].blockList.GetCount())
	{
		s_userInfo[s_currentUser].currentIdx = 0;
	}

	s_currentBlock = s_userInfo[s_currentUser].blockList[s_userInfo[s_currentUser].currentIdx];
	SelectBlock();
}

////////////////////////////////////////////////////////////////////////////
// name:	SelectPrevious
// purpose: 
// params:  none
// returns: nothing
////////////////////////////////////////////////////////////////////////////
void CBlockView::SelectPrevious()
{
	s_userInfo[s_currentUser].currentIdx--;

	if(s_userInfo[s_currentUser].currentIdx < 0)
	{
		s_userInfo[s_currentUser].currentIdx = s_userInfo[s_currentUser].blockList.GetCount() - 1;
	}

	s_currentBlock = s_userInfo[s_currentUser].blockList[s_userInfo[s_currentUser].currentIdx];
	SelectBlock();
}

////////////////////////////////////////////////////////////////////////////
// name:	WarpToCurrent
// purpose: 
// params:  none
// returns: nothing
////////////////////////////////////////////////////////////////////////////
void CBlockView::WarpToCurrent()
{
//	if(!s_blocks[s_currentBlock].posSetOnce)
//		return;

	Vector3 pos;
	GetBlockPos(s_currentBlock,pos);
	pos.z += 50.0f;

	camFrame& freeCamFrame = camInterface::GetDebugDirector().GetFreeCamFrameNonConst();
	freeCamFrame.SetWorldMatrix(M34_IDENTITY);
	freeCamFrame.SetPosition(pos);
}

////////////////////////////////////////////////////////////////////////////
// name:	GetNumPoints
// purpose: 
// params:  none
// returns: nothing
////////////////////////////////////////////////////////////////////////////
s32 CBlockView::GetNumPoints(s32 index)
{
	return s_blocks[index].numPoints;
}

////////////////////////////////////////////////////////////////////////////
// name:	GetPoint
// purpose: 
// params:  none
// returns: nothing
////////////////////////////////////////////////////////////////////////////
Vector3 CBlockView::GetPoint(s32 index,s32 point)
{
	return s_blocks[index].points[point];
}

////////////////////////////////////////////////////////////////////////////
// name:	ShouldDisplayInfo
// purpose: returns whether to display block info
// params:  none
// returns: true or false
////////////////////////////////////////////////////////////////////////////
bool CBlockView::ShouldDisplayInfo()
{
	return (s_DisplayInfo && s_numBlocks > 0);
}


void CBlockView::SetDisplayInfo(bool bDisplayInfo)
{
	s_DisplayInfo = bDisplayInfo;
}




////////////////////////////////////////////////////////////////////////////
// name:	ShouldRenderToMap
// purpose: returns whether to display block info based on the flag
// params:  none
// returns: s32 containing what flags to render to the map
////////////////////////////////////////////////////////////////////////////
bool CBlockView::ShouldRenderToMap(s32 flag)
{
	if (flag == BLOCKFLAGS_FIRST_PASS)
		return s_Filter1stPass;
	if (flag == BLOCKFLAGS_SECOND_PASS_STARTED)
		return s_Filter2ndPassStarted;
	if (flag == BLOCKFLAGS_SECOND_PASS_OUTSOURCED)
		return s_Filter2ndPassOutsourced;
	if (flag == BLOCKFLAGS_SECOND_PASS_COMPLETE)
		return s_Filter2ndPassComplete;
	if (flag == BLOCKFLAGS_THIRD_PASS_COMPLETE)
		return s_Filter3rdPass;
	if (flag == BLOCKFLAGS_COMPLETE)
		return s_FilterComplete;

	return false;
}

bool CBlockView::FilterArtist()
{
	return s_FilterArtist;
}

bool CBlockView::FilterBlock()
{
	return ( s_IsolateBlock && bValidBlockList );
}

bool CBlockView::FilterLodTree()
{
	return s_IsolateLodTree;
}

bool CBlockView::IsCurrentBlock(s32 index)
{
	return (s_currentBlock == index);
}

////////////////////////////////////////////////////////////////////////////
// name:	ShouldDraw
// purpose: 
// params:  none
// returns: nothing
////////////////////////////////////////////////////////////////////////////
bool CBlockView::ShouldDraw(CEntity* BANK_ONLY(pEntity))
{
#if __BANK
	if(	(pEntity->GetType() == ENTITY_TYPE_VEHICLE) || 
		(pEntity->GetType() == ENTITY_TYPE_PED))
		return true;

	if(!FilterBlock() && !s_IsolateArtist && !s_IsolateDates && !s_IsolateLodTree)
		return true;

	s32 blockIdx = fwMapDataDebug::GetBlockIndex(pEntity->GetIplIndex());

	if(blockIdx < 0 || blockIdx >= s_numBlocks)
		return false;

	if(s_IsolateBlock && blockIdx != s_currentBlock)
		return false;

	if(s_IsolateArtist && (stricmp(s_userInfo[s_currentUser].name,s_blocks[blockIdx].user) != 0))
		return false;

	if(s_IsolateDates)
	{
		char StartDate[128];
		char EndDate[128];

		sprintf(StartDate,"%d:%02d:%02d:0:0:0",s_StartYear,s_StartMonth,s_StartDay);
		sprintf(EndDate,"%d:%02d:%02d:23:59:59",s_EndYear,s_EndMonth,s_EndDay);

		if(stricmp(s_blocks[blockIdx].time,StartDate) < 0)
			return false;

		if(stricmp(s_blocks[blockIdx].time,EndDate) > 0)
			return false;
	}

	if (s_IsolateLodTree)
	{
		if (pEntity->GetLodData().IsOrphanHd())
			return false;

		if (pEntity->GetRootLod() != s_lodTreeRootInfo[s_currentLodTree].m_pRootEntity)
			return false;
	}

	return true;
#else
	return false;
#endif
}

#endif //#if !__FINAL
