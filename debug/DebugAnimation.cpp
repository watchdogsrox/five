#include "DebugAnimation.h"
#if DR_ANIMATIONS_ENABLED

#include "animation/debug/AnimViewer.h"
#include "cranimation/framedata.h"
#include "fwanimation/movedump.h"
#include "move/move_network.h"
#include "move/move_statedef.h"
#include "move/move_types_internal.h"
#include "peds/ped.h"
#include "physics/debugEvents.h"
#include "physics/debugplayback.h"
#include "scene/Entity.h"
#include "system/timemgr.h"

static DR_TrackMoveInterface s_TrackMoveInterface;
int AnimMotionTreeTracker::sm_LineOffset;
u32 AnimMotionTreeTracker::sm_iHoveredFrame;

using namespace debugPlayback;

CEntity* GetSelectedEntity()
{
	debugPlayback::phInstId rSelected(debugPlayback::GetCurrentSelectedEntity());
	if (!rSelected.IsValid())
		return 0;

	return (CEntity*)rSelected->GetUserData();
}

struct MoveSetValue : public SetTaggedFloatEvent
{
	MoveSetValue()
	{	}
	PAR_PARSABLE;
	DR_EVENT_DECL(MoveSetValue)
};

struct MoveGetValue : public SetTaggedFloatEvent
{
	MoveGetValue()
	{	}
	PAR_PARSABLE;
	DR_EVENT_DECL(MoveGetValue)
};

struct MoveTransitionStringCollection : public SetTaggedDataCollectionEvent
{
	MoveTransitionStringCollection()
	{	}
	PAR_PARSABLE;
	DR_EVENT_DECL(MoveTransitionStringCollection)
};

struct MoveSetValueStringCollection : public SetTaggedDataCollectionEvent
{
	MoveSetValueStringCollection()
	{	}
	PAR_PARSABLE;
	DR_EVENT_DECL(MoveSetValueStringCollection)
};

struct MoveConditionStringCollection : public SetTaggedDataCollectionEvent
{
	MoveConditionStringCollection()
	{	}
	PAR_PARSABLE;
	DR_EVENT_DECL(MoveConditionStringCollection)
};

void UpdateAnimEventRecordingState()
{
	mvDRTrackInterface::sm_bEnabled =	
				DR_EVENT_ENABLED(MoveTransitionStringCollection)
		||		DR_EVENT_ENABLED(MoveSetValue) 
		||		DR_EVENT_ENABLED(MoveGetValue) 
		||		DR_EVENT_ENABLED(MoveSetValueStringCollection) 
		||		DR_EVENT_ENABLED(MoveConditionStringCollection);
}

struct MoveDumpPrinterTextOutput : public fwMoveDumpPrinterBase
{
	debugPlayback::TextOutputVisitor *mp_Printer;
	int m_iIndent;
	int m_iTotalLines;
	int m_iLinesToSkip;
	bool m_bLineHovered;

	MoveDumpPrinterTextOutput(debugPlayback::TextOutputVisitor &rOutput)
	{
		m_iIndent = 0;
		m_iTotalLines = 0;
		m_iLinesToSkip = 0;
		m_bLineHovered = false;
		mp_Printer = &rOutput;
	}

	void PrintLine(int iIndent, const char *pText)
	{
		//Hack to support indenting
		int iPopit = iIndent;
		while (iIndent)
		{
			mp_Printer->PushIndent();
			--iIndent;
		}

		if (m_iTotalLines > m_iLinesToSkip)
		{
			if (mp_Printer->AddLine( pText ))
			{
				m_bLineHovered = true;
			}
		}

		while (iPopit)
		{
			mp_Printer->PopIndent();
			--iPopit;
		}

		++m_iTotalLines;
	}
};

void AnimMotionTreeTracker::PrintToTTY() const
{
	fwMoveDumpRenderFlags renderFlags(fwMotionTreeVisualiser::ms_bRenderTree,
		fwMotionTreeVisualiser::ms_bRenderClass,
		fwMotionTreeVisualiser::ms_bRenderWeight,
		fwMotionTreeVisualiser::ms_bRenderLocalWeight,
		fwMotionTreeVisualiser::ms_bRenderStateActiveName,
		fwMotionTreeVisualiser::ms_bRenderRequests,
		fwMotionTreeVisualiser::ms_bRenderFlags,
		fwMotionTreeVisualiser::ms_bRenderInputParams,
		fwMotionTreeVisualiser::ms_bRenderOutputParams,
		fwMotionTreeVisualiser::ms_bRenderInvalidNodes,
		fwMotionTreeVisualiser::ms_bRenderSynchronizerWeights,
		fwMotionTreeVisualiser::ms_bRenderSynchronizerGraph,
		fwMotionTreeVisualiser::ms_bRenderDofs,
		fwMotionTreeVisualiser::ms_bRenderAddress);


	Displayf("----------------------------------------");
	Displayf("Motion tree dump - '%s' event index %d", m_Label.TryGetCStr(), m_iEventIndex);
	Displayf("----------------------------------------");
	int iHorz=0;
	fwMoveDumpPrinterTTY printer;
	m_DumpNetwork->Print(printer, iHorz, renderFlags);
	Displayf("----------------------------------------\n");
}

struct MoveDumpPrinterCountLines : public fwMoveDumpPrinterBase
{
	int m_iLines;
	MoveDumpPrinterCountLines():m_iLines(0){}
	void PrintLine(int, const char *) {	++m_iLines;	}
};

void AnimMotionTreeTracker::AddEventOptions(const debugPlayback::Frame &frame, debugPlayback::TextOutputVisitor &rText, bool &bMouseDown) const
{
	PhysicsEvent::AddEventOptions(frame, rText, bMouseDown);
	if (rText.AddLine("[Print MT To TTY]") && bMouseDown)
	{
		PrintToTTY();
		bMouseDown = false;
	}

	fwMoveDumpRenderFlags renderFlags(fwMotionTreeVisualiser::ms_bRenderTree,
		fwMotionTreeVisualiser::ms_bRenderClass,
		fwMotionTreeVisualiser::ms_bRenderWeight,
		fwMotionTreeVisualiser::ms_bRenderLocalWeight,
		fwMotionTreeVisualiser::ms_bRenderStateActiveName,
		fwMotionTreeVisualiser::ms_bRenderRequests,
		fwMotionTreeVisualiser::ms_bRenderFlags,
		fwMotionTreeVisualiser::ms_bRenderInputParams,
		fwMotionTreeVisualiser::ms_bRenderOutputParams,
		fwMotionTreeVisualiser::ms_bRenderInvalidNodes,
		fwMotionTreeVisualiser::ms_bRenderSynchronizerWeights,
		fwMotionTreeVisualiser::ms_bRenderSynchronizerGraph,
		fwMotionTreeVisualiser::ms_bRenderDofs,
		fwMotionTreeVisualiser::ms_bRenderAddress);

	int iHorz=0;
	MoveDumpPrinterCountLines counter;
	m_DumpNetwork->Print(counter, iHorz, renderFlags);
}

bool AnimMotionTreeTracker::DebugEventText(debugPlayback::TextOutputVisitor &rOutput) const
{
	PhysicsEvent::PrintInstName(rOutput);
	fwMoveDumpRenderFlags renderFlags(fwMotionTreeVisualiser::ms_bRenderTree,
		fwMotionTreeVisualiser::ms_bRenderClass,
		fwMotionTreeVisualiser::ms_bRenderWeight,
		fwMotionTreeVisualiser::ms_bRenderLocalWeight,
		fwMotionTreeVisualiser::ms_bRenderStateActiveName,
		fwMotionTreeVisualiser::ms_bRenderRequests,
		fwMotionTreeVisualiser::ms_bRenderFlags,
		fwMotionTreeVisualiser::ms_bRenderInputParams,
		fwMotionTreeVisualiser::ms_bRenderOutputParams,
		fwMotionTreeVisualiser::ms_bRenderInvalidNodes,
		fwMotionTreeVisualiser::ms_bRenderSynchronizerWeights,
		fwMotionTreeVisualiser::ms_bRenderSynchronizerGraph,
		fwMotionTreeVisualiser::ms_bRenderDofs,
		fwMotionTreeVisualiser::ms_bRenderAddress);

	int iHorz=0;
	MoveDumpPrinterTextOutput printer(rOutput);
	printer.m_iLinesToSkip = sm_LineOffset;
	m_DumpNetwork->Print(printer, iHorz, renderFlags);
	
	if (sm_LineOffset > printer.m_iTotalLines-10)
	{
		sm_LineOffset = Max(0, printer.m_iTotalLines-10);
	}

	if (printer.m_bLineHovered)
	{
		//Store off that we were hovered this frame - means that we can track mouse dragging in some other code that 
		sm_iHoveredFrame = TIME.GetFrameCount();

		if (ioMouse::HasWheel() && (ioMouse::GetDZ()!=0))
		{
			if (ioMouse::GetDZ() < 0)
			{
				if (sm_LineOffset < printer.m_iTotalLines-10)
				{
					sm_LineOffset+=3;
				}
			}
			else if (sm_LineOffset)
			{
				sm_LineOffset=Max(0, sm_LineOffset-3);
			}
		}			
	}
	return true;
}

void AnimMotionTreeTracker::Init()
{
	if (m_rInst.IsValid())
	{
		fwEntity *pEntity = (fwEntity *)m_rInst->GetUserData();
		if (pEntity)
		{
			const char *szFilter = NULL;
			if(CAnimViewer::m_iCaptureFilterNameIndex >= 0 && CAnimViewer::m_iCaptureFilterNameIndex < CAnimViewer::m_iCaptureFilterNameCount)
			{
				szFilter = CAnimViewer::m_szCaptureFilterNames[CAnimViewer::m_iCaptureFilterNameIndex];
			}
			else
			{
				szFilter = "BONEMASK_LOD_LO";
			}

			m_DumpNetwork = rage_new fwMoveDumpNetwork(pEntity, CAnimViewer::m_bCaptureDofs, szFilter, CAnimViewer::m_bRelativeToParent);
		}
	}
}

AnimMotionTreeTracker::~AnimMotionTreeTracker()
{
	delete m_DumpNetwork;
}

bool StoreSkeleton::DebugEventText(debugPlayback::TextOutputVisitor &rOutput) const
{
	//Just display the cached name, this is all about the 3d display
	PhysicsEvent::PrintInstName(rOutput);
	rOutput.AddLine("Pos: <%f, %f, %f>", m_SkeletonName.GetCStr(), m_Pos.GetXf(), m_Pos.GetYf(), m_Pos.GetZf());
	rOutput.AddLine("SkeletonName: %s", m_SkeletonName.GetCStr());
	rOutput.AddLine("BonePosCount: %d", m_BonePositions.GetCount());
	rOutput.AddLine("BoneRotCount: %d", m_BoneRotations.GetCount());
	rOutput.AddLine("Signature: 0x%x", m_DataSignature);

	if (GameRecorder::GetAppLevelInstance())
	{
		const StoredSkelData *pSkelData = GameRecorder::GetAppLevelInstance()->GetSkelData(m_DataSignature);
		if (pSkelData)
		{
			rOutput.AddLine("SkelSignature: 0x%x", pSkelData->m_SkelSignature);
			rOutput.AddLine("SkelBoneCount: %d", pSkelData->m_BoneIndexToFilteredIndex.GetCount());
		}
	}
/*
	rOutput.AddLine("FULL DATA");
	rOutput.PushIndent();
	PhysicsEvent::DebugEventText(rOutput);
	rOutput.PopIndent();
*/
	return true;
}

void StoredSkelData::Create(const crSkeletonData &rData, crFrameFilter *pFilter)
{
	//Count the number of links we're going to need to display
	m_SkelSignature = rData.GetSignature();
	int iLinks = 0;
	int iNumBones = rData.GetNumBones();
	int iNumFilteredBones = pFilter ? 0 : iNumBones;
	float fInOutWeight;
	for (u32 i = 0; i < iNumBones; i++)
	{
		const crBoneData *boneData =  rData.GetBoneData(i);
		u16 boneId = boneData->GetBoneId();
		if (pFilter)
		{
			if (	pFilter->FilterDof(kTrackBoneTranslation, boneId, fInOutWeight) 
				||	pFilter->FilterDof(kTrackBoneRotation, boneId, fInOutWeight)
				)
			{
				if (boneData->GetParent())
				{
					u16 parentBoneId = boneData->GetParent()->GetBoneId();
					if (	pFilter->FilterDof(kTrackBoneTranslation, parentBoneId, fInOutWeight) 
						||	pFilter->FilterDof(kTrackBoneRotation, parentBoneId, fInOutWeight)
						)
					{
						++iLinks;
					}
				}
				++iNumFilteredBones;
			}
		}
		else
		{
			if (boneData->GetParent())
			{
				++iLinks;
			}
			++iNumFilteredBones;
		}
	}

	//Store a map from bones filtered to actual bones
	m_FilteredIndexToBoneIndex.Resize( iNumFilteredBones );
	m_BoneIndexToFilteredIndex.Resize( iNumBones );
	int iNumBonesMapped = 0;
	for (u32 i = 0; i < iNumBones; i++)
	{
		const crBoneData *boneData =  rData.GetBoneData(i);
		if (pFilter)
		{
			u16 boneId = boneData->GetBoneId();
			if (	pFilter->FilterDof(kTrackBoneTranslation, boneId, fInOutWeight) 
				||	pFilter->FilterDof(kTrackBoneRotation, boneId, fInOutWeight)
				)
			{
				Assign(m_FilteredIndexToBoneIndex[iNumBonesMapped], i);
				Assign(m_BoneIndexToFilteredIndex[i], iNumBonesMapped);
				++iNumBonesMapped;
			}
			else
			{
				Assign(m_BoneIndexToFilteredIndex[i], 0xffff);
			}
		}
		else
		{
			Assign(m_FilteredIndexToBoneIndex[iNumBonesMapped], i);
			Assign(m_BoneIndexToFilteredIndex[i], i);
			++iNumBonesMapped;
		}
	}

	//And store links between actual bones
	m_BoneLinks.Resize( iLinks );
	int iLinksStored = 0;
	for (u32 i = 0; i < iNumBones; i++)
	{
		const crBoneData *boneData =  rData.GetBoneData(i);
		if (boneData->GetParent())
		{
			u16 iFilteredIndex0 = m_BoneIndexToFilteredIndex[ boneData->GetParent()->GetIndex() ];
			if (iFilteredIndex0 != 0xffff)
			{
				u16 iFilteredIndex1 = m_BoneIndexToFilteredIndex[ boneData->GetIndex() ];
				if (iFilteredIndex1 != 0xffff)
				{
					Assign(m_BoneLinks[iLinksStored].m_iBone0, iFilteredIndex0);
					Assign(m_BoneLinks[iLinksStored].m_iBone1, iFilteredIndex1);
					++iLinksStored;
				}
			}
		}
	}
}

void StoreSkeleton::CustomDebugDraw3d(Color32 colLine, Color32 colJoint) const
{
	//Draw the skeleton using the stored data and the skeleton data structure
	const u32 iNumBonePositions = m_BonePositions.GetCount();
	colJoint = debugPlayback::DebugRecorder::ModifyAlpha(colJoint);
	colLine = debugPlayback::DebugRecorder::ModifyAlpha(colLine);
	grcColor(colJoint);
	for (u32 iBone = 0; iBone < iNumBonePositions; iBone++)
	{
		Vec3V vPos = m_BonePositions[ iBone ].Get();
		grcDrawSphere(0.02f, vPos + m_Pos, 4, true, true);
	}
	grcWorldIdentity();	//Recover from grcDrawSpheres abuse

	GameRecorder *pRecorder = GameRecorder::GetAppLevelInstance();
	if (!pRecorder->HasSkelData(m_DataSignature))
	{
		grcColor(Color32(255,0,0,debugPlayback::DebugRecorder::GetGlobalAlphaScale()));
		grcDrawLabel(m_Pos, "Failed to find matching skel data", true);

		//Perhaps we can draw the archetype here?
		return;
	}

	StoredSkelData *pSkelData = pRecorder->GetSkelData(m_DataSignature);
	if(pSkelData)
	{
		//And draw all the links
		const u32 iTotalLinks = pSkelData->m_BoneLinks.GetCount();
		for (u32 iBoneLink = 0; iBoneLink < iTotalLinks; iBoneLink++)
		{
			Vec3V vPos0 = m_BonePositions[ pSkelData->m_BoneLinks[iBoneLink].m_iBone0 ].Get();
			vPos0 += m_Pos;
			Vec3V vPos1 = m_BonePositions[ pSkelData->m_BoneLinks[iBoneLink].m_iBone1 ].Get();
			vPos1 += m_Pos;
			grcDrawLine(vPos0, vPos1, colLine);
		}
	}
}

template<typename T>
static T Quantize10(float fValue, float fMin, float fMax)
{
	fValue = Clamp(fValue, fMin, fMax);
	fValue -= fMin;
	fValue *= 1.0f/( (fMax - fMin) );
	fValue += 0.00048875855327468231f ;		// 0.5f * 0.00097751710654936461f;		//Reduce rounding errors
	fValue *= 1023.0f;						//  (1<<10 - 1) = 1023 ---> 1.0f / 1023.f	= 0.00097751710654936461
	return (T)fValue;
}
template<typename T>
static float DeQuantize10(T iValue, float fMin, float fMax)
{
	float fValue = iValue * 0.00097751710654936461f;			//  (1<<10 - 1) = 1023 ---> 1.0f / 1023.f	= 0.00097751710654936461
	fValue *= (fMax-fMin);
	fValue += fMin;
	return fValue;
}

void StoreSkeleton::u32Quaternion::Set(QuatV_In r)
{
	float vx = r.GetXf();
	float vy = r.GetYf();
	float vz = r.GetZf();
	float vw = r.GetWf();

	//Smallest 3 quantization (Quaternion assumed normalized)
	u32 iLargest = 0;
	float fLargest = vx;
	float fLargestAbs = Abs(fLargest);


	float fTest = vy;
	float fTestAbs = Abs(fTest);
	if (fTestAbs > fLargestAbs)
	{
		iLargest = 1;
		fLargest = fTest;
		fLargestAbs = fTestAbs;
	}
	fTest = vz;
	fTestAbs = Abs(fTest);
	if (fTestAbs > fLargestAbs)
	{
		iLargest = 2;
		fLargest = fTest;
		fLargestAbs = fTestAbs;
	}
	fTest = vw;
	fTestAbs = Abs(fTest);
	if (fTestAbs > fLargestAbs)
	{
		iLargest = 3;
		fLargest = fTest;
		fLargestAbs = fTestAbs;
	}

	//Do we need to flip the quaternion?
	if ((fLargest > 0) ^ (fLargestAbs > 0))
	{
		vx = -vx;
		vy = -vy;
		vz = -vz;
		vw = -vw;
	}

	//Serialize the index of the larget value
	m_uPackedData = iLargest<<30;

	float fRemaining[3];
	switch(iLargest)
	{
	case 0:
		fRemaining[0] = vy;
		fRemaining[1] = vz;
		fRemaining[2] = vw;
		break;
	case 1:
		fRemaining[0] = vx;
		fRemaining[1] = vz;
		fRemaining[2] = vw;
		break;
	case 2:
		fRemaining[0] = vx;
		fRemaining[1] = vy;
		fRemaining[2] = vw;
		break;
	default:
		fRemaining[0] = vx;
		fRemaining[1] = vy;
		fRemaining[2] = vz;
		break;
	}

	//Quantize each value from -0.707 to 0.707
	//const float fOneOverSqrt2 = 1.0f / sqrtf(2.0f);		// disabled by Svetli - replaced with hardcoded value
	const float fOneOverSqrt2 = 0.7071067811865475f;			// 1.0f / sqrtf(2.0f) (1.4142135623730950488f)
	Assert(Abs(fRemaining[0]) <= fOneOverSqrt2);
	Assert(Abs(fRemaining[1]) <= fOneOverSqrt2);
	Assert(Abs(fRemaining[2]) <= fOneOverSqrt2);
	u32 iTmpVal;
	iTmpVal = Quantize10<u32>(fRemaining[0], -fOneOverSqrt2, fOneOverSqrt2);
	m_uPackedData |= iTmpVal<<20;
	iTmpVal = Quantize10<u32>(fRemaining[1], -fOneOverSqrt2, fOneOverSqrt2);
	m_uPackedData |= iTmpVal<<10;
	iTmpVal = Quantize10<u32>(fRemaining[2], -fOneOverSqrt2, fOneOverSqrt2);
	m_uPackedData |= iTmpVal;	
}

QuatV_Out StoreSkeleton::u32Quaternion::Get() const
{
	//Smallest 3 quantization (Quaternion assumed normalized)

	//Serialize the index of the largest value
	u32 iLargest = m_uPackedData>>30;
	float fOthers[3];

	//Quantize each value from -0.707 to 0.707
	//const float fOneOverSqrt2 = 1.0f / sqrtf(2.0f);			// disabled by Svetli - replaced with hardcoded value
	const float fOneOverSqrt2 = 0.7071067811865475f;			// 1.0f / sqrtf(2.0f) (1.4142135623730950488f)
	float fTotal2=0.0f;

	u32 iTmpVal;
	float fValue;
	//const int iTenBits = ((1<<10) - 1);							// disabled by Svetli - replaced with hardcoded value
	const int iTenBits = 1023;											
	iTmpVal = (m_uPackedData>>20) & iTenBits;
	fValue = DeQuantize10<u32>(iTmpVal, -fOneOverSqrt2, fOneOverSqrt2);
	fTotal2 += fValue * fValue;
	fOthers[0] = fValue;
	iTmpVal = (m_uPackedData>>10) & iTenBits;
	fValue = DeQuantize10<u32>(iTmpVal, -fOneOverSqrt2, fOneOverSqrt2);
	fTotal2 += fValue * fValue;
	fOthers[1] = fValue;
	iTmpVal = m_uPackedData & iTenBits;
	fValue = DeQuantize10<u32>(iTmpVal, -fOneOverSqrt2, fOneOverSqrt2);
	fTotal2 += fValue * fValue;
	fOthers[2] = fValue;

	//Recalculate the largest value (quantization defined it as being positive by flipping the quaternion if needed)
	float fLargest = sqrtf(1.0f - fTotal2);

	//Fill the quaternion
	switch(iLargest)
	{
	case 0:
		return QuatV(fLargest, fOthers[0], fOthers[1], fOthers[2]);
	case 1:
		return QuatV(fOthers[0], fLargest, fOthers[1], fOthers[2]);
	case 2:
		return QuatV(fOthers[0], fOthers[1], fLargest, fOthers[2]);
	}	
	return QuatV(fOthers[0], fOthers[1], fOthers[2], fLargest);
}

bool StoreSkeleton::ms_bUseLowLodSkeletons = true;

bool StoreSkeleton::Replay(phInst *pInst) const
{
	if (!pInst)
	{
		return false;
	}

	GameRecorder *pRecorder = GameRecorder::GetAppLevelInstance();
	if (!pRecorder->HasSkelData(m_DataSignature))
	{
		return false;
	}

	fwEntity *pEntity = (fwEntity *)pInst->GetUserData();
	if (!pEntity)
	{
		return false;
	}
	
	crSkeleton *pSkel = pEntity->GetSkeleton();
	if (!pSkel)
	{
		return false;
	}

	const StoredSkelData *pSkelData = pRecorder->GetSkelData(m_DataSignature);
	if (pSkelData)
	{
		if (Verifyf(pSkelData->m_SkelSignature == pSkel->GetSkeletonData().GetSignature(), "Recorded skel does not match current skel, cannot play"))
		{
			const int kBoneCount = m_BonePositions.GetCount();
			if (m_BoneRotations.GetCount() == kBoneCount)
			{
				//This has the potentially useful but likely annoying side effect of leaving the ped at this location.
				//setting the matrix throws assertions when attached.
				//if (!pEntity->GetIsAttached())
				//{
				//	//Set the position for draw culling purposes
				//	pEntity->SetPosition(RCC_VECTOR3(m_Pos));
				//}

				for (int i=0 ; i<kBoneCount ; i++)
				{
					Mat34V mat;
					QuatV vRot = m_BoneRotations[i].Get();
					Mat34VFromQuatV(mat, vRot, m_BonePositions[ i ].Get() + m_Pos);					
					pSkel->SetGlobalMtx(pSkelData->m_FilteredIndexToBoneIndex[i], mat);
				}

				//Fill in the rest, hoping all parent matrices are filled out in order
				for (int i=0 ; i<pSkel->GetSkeletonData().GetNumBones() ; i++)
				{
					if (pSkelData->m_BoneIndexToFilteredIndex[i] == 0xffff)
					{
						const crBoneData *boneData = pSkel->GetSkeletonData().GetBoneData(i);
						if (boneData)
						{
							if (Verifyf(boneData->GetParent() && (boneData->GetParent()->GetIndex() < i), "Need parent earlier in list to fill in data"))
							{
								Mat34V matLocal;
								QuatV vRot = boneData->GetDefaultRotation();
								Mat34VFromQuatV(matLocal, vRot, boneData->GetDefaultTranslation());					
								
								//Need to multiply this into the parent bone matrix
								Mat34V matParent;
								pSkel->GetGlobalMtx(boneData->GetParent()->GetIndex(), matParent);
								Mat34V matBone;
								Transform(matBone, matParent, matLocal);
								pSkel->SetGlobalMtx(i, matBone);
							}
						}
					}
				}
			}
		}
	}
	return false;
}

void StoreSkeleton::DebugDraw3d(debugPlayback::eDrawFlags drawFlags) const
{
	Color32 colLine(m_iColor);
	Color32 colJoint(0,0,200,255);

	if ((TIME.GetFrameCount() & 0x7) < 0x3)
	{
		if (drawFlags & eDrawSelected)
		{
			colJoint.SetRed(255);
			colLine.SetBlue(0);
		}
		if (drawFlags & eDrawHovered)
		{
			colJoint.SetGreen(255);
			colLine.SetBlue(128);
		}
		GameRecorder *pRecorder = GameRecorder::GetAppLevelInstance();
		if (pRecorder)
		{
			const EventBase *pSelected = pRecorder->GetSelectedEvent();
			if (pSelected)
			{
				if(pSelected->IsPhysicsEvent())
				{
					const PhysicsEvent *pPhysEv = static_cast<const PhysicsEvent*>(pSelected);
					if (pPhysEv->IsInstInvolved(m_rInst))
					{
						//Looks like we're browsing an event that is this inst, highlight the skeleton
						colJoint.Set(255,0,0);
					}
				}
			}
			const EventBase *pHovered = pRecorder->GetWasHoveredEvent();
			if (pHovered)
			{
				if(pHovered->IsPhysicsEvent())
				{
					const PhysicsEvent *pPhysEv = static_cast<const PhysicsEvent*>(pHovered);
					if (pPhysEv->IsInstInvolved(m_rInst))
					{
						//Looks like we're browsing an event that is this inst, highlight the skeleton
						colJoint.Set(0,255,0);
					}
				}
			}
		}
	}
	DebugRecorder::ModifyAlpha(colJoint);
	DebugRecorder::ModifyAlpha(colLine);
	CustomDebugDraw3d(colLine, colJoint);
}

bool StoreSkeleton::IsSkeletonEvent(const debugPlayback::EventBase *pEvent)
{
	const char *pEventTypeId = pEvent->GetEventType();
	return (StoreSkeletonPreUpdate::GetStaticEventType() == pEventTypeId) 
		|| (StoreSkeletonPostUpdate::GetStaticEventType() == pEventTypeId);
}

StoreSkeleton::StoreSkeleton(const debugPlayback::CallstackHelper rCallstack, const phInst *pInst, const fwEntity *pEntity, const char *pSkeletonName, Color32 color)
	:debugPlayback::PhysicsEvent(rCallstack, pInst)
	,m_SkeletonName(pSkeletonName)
	,m_DataSignature(0)
	,m_iColor(color.GetColor())
{	
	if (pEntity)
	{
		//Cache the skeleton temp data (global matrices for easy playback
		const crSkeleton *pSkel = pEntity->GetSkeleton();
		if (pSkel)
		{
			const int kBoneCount = pSkel->GetBoneCount();
			const crSkeletonData &rSkelData = pSkel->GetSkeletonData();
			m_DataSignature = rSkelData.GetSignature();

			crFrameFilter *filter = ms_bUseLowLodSkeletons ? pEntity->GetLowLODFilter() : 0;
			m_DataSignature ^= filter ? 0x12345678 : 0;
			GameRecorder *pRecorder = GameRecorder::GetAppLevelInstance();
			if (!pRecorder->HasSkelData(m_DataSignature))
			{
				StoredSkelData *pSkelData = rage_new StoredSkelData;
				pSkelData->Create( rSkelData, filter );
				pRecorder->AddSkelData(pSkelData, m_DataSignature);
			}

			u32 iNumBonesToSave = kBoneCount;
			if (filter)
			{
				iNumBonesToSave = 0;
				for (int boneIndex = 0; boneIndex < kBoneCount; boneIndex++)
				{
					const crBoneData *boneData = rSkelData.GetBoneData(boneIndex);
					float inOutWeight;
					if (filter->FilterDof(kTrackBoneTranslation, boneData->GetBoneId(), inOutWeight) || filter->FilterDof(kTrackBoneRotation, boneData->GetBoneId(), inOutWeight))
					{
						++iNumBonesToSave;
					}
				}
			}

			m_BonePositions.Resize( iNumBonesToSave );

			if (pRecorder->PosePedsIfPossible())
			{
				m_BoneRotations.Resize( iNumBonesToSave );
			}

			int iNumSavedBones = 0;
			for (int boneIndex=0 ; boneIndex<kBoneCount ; boneIndex++)
			{
				if(filter)
				{
					const crBoneData *boneData = rSkelData.GetBoneData(boneIndex);
					float inOutWeight;
					if (!filter->FilterDof(kTrackBoneTranslation, boneData->GetBoneId(), inOutWeight) && !filter->FilterDof(kTrackBoneRotation, boneData->GetBoneId(), inOutWeight))
					{
						continue;
					}
				}

				Mat34V mat;
				pSkel->GetGlobalMtx(boneIndex, mat);
				if (pRecorder->PosePedsIfPossible())
				{
					QuatV vQ = QuatVFromMat34V(mat);
					vQ = Normalize( vQ );
					m_BoneRotations[iNumSavedBones].Set( vQ );
				}
				m_BonePositions[iNumSavedBones].Set(mat.GetCol3() - m_Pos);
				++iNumSavedBones;
			}
		}
	}
}

DR_EVENT_IMP(MoveGetValue);
DR_EVENT_IMP(MoveSetValue);
DR_EVENT_IMP(MoveTransitionStringCollection);
DR_EVENT_IMP(MoveSetValueStringCollection);
DR_EVENT_IMP(MoveConditionStringCollection);
DR_EVENT_IMP(AnimMotionTreeTracker);
DR_EVENT_IMP(StoreSkeletonPreUpdate);
DR_EVENT_IMP(StoreSkeletonPostUpdate);

//With a mvNetwork we need to find a creature object. This can be compared with entity
//associated with the current selected phInst's creature object.

const char *GetStringForCondition(int iConditionType)
{
	switch(iConditionType)
	{
	case mvConstant::kConditionInRange:
		return "InRange";
	case mvConstant::kConditionOutOfRange:
		return "OutOfRange";
	case mvConstant::kConditionOnRequest:
		return "OnRequest";
	case mvConstant::kConditionOnFlag:
		return "OnFlag";
	case mvConstant::kConditionAtEvent:
		return "AtEvent";
	case mvConstant::kConditionGreaterThan:
		return ">";
	case mvConstant::kConditionGreaterThanEqual:
		return ">=";
	case mvConstant::kConditionLessThan:
		return "<";
	case mvConstant::kConditionLessThanEqual:
		return "<=";
	case mvConstant::kConditionLifetimeGreaterThan:
		return "Lifetime>";
	case mvConstant::kConditionLifetimeLessThan:
		return "Lifetime<";
	case mvConstant::kConditionBoolEquals:
		return "BoolEquals";
	default:
		Assertf(0, "Unknown condition type %d", iConditionType);
	}
	return "<unknown condition>";
}


const char *GetStringForResult(char *buffer, int iBufferSize, int iConditionIndex, int iConditionResult)
{
	const char *pStr = "passed";
	switch(iConditionResult)
	{
	case -1:
		pStr = "prepassed";
		break;
	case 0:
		pStr = "failed";
		break;
	default:
		break;
	}
	formatf(buffer, iBufferSize, "[%d] %s", iConditionIndex, pStr);
	return buffer;
}

//Find a nice way of making these threadsafe...
#if __DEV || __BANK
__THREAD  const mvNodeStateBaseDef* lastFrom;
__THREAD  const mvNodeStateBaseDef* lastTo;
__THREAD  u32 iLastFrame;
__THREAD MoveConditionStringCollection *pCurrentCollection;

void DR_TrackMoveInterface::RecordEndConditions() const
{
	pCurrentCollection = 0;
}

void DR_TrackMoveInterface::RecordCondition(const mvNetworkDef* networkDef, const crCreature* creature, int iPassed, int iConditionType, int iConditionIndex, const mvNodeStateBaseDef* from, const mvNodeStateBaseDef* to)
{
	const phInst *pInst = debugPlayback::GetInstFromCreature(creature);
	if (!pInst)
	{
		return;
	}

	if (	(pCurrentCollection)
		&&	(lastFrom == from)
		&&	(lastTo == to)
		&&	(iLastFrame == TIME.GetFrameCount())
		)
	{
		//Just leave pCurrentCollection as is
	}
	else
	{
		//New collection
		pCurrentCollection = AddPhysicsEvent<MoveConditionStringCollection>(pInst);
		if (pCurrentCollection)
		{
			atString networkNameString(networkDef->GetName());
			if(networkNameString.StartsWith("memory:$"))
			{
				int index = networkNameString.LastIndexOf(":");
				if(index != -1)
				{
					atString tempString(&networkNameString.c_str()[index + 1]);
					networkNameString = tempString;
				}
			}

			atHashString toStr(to->m_id);
			atVarString name("MV: %s ?-> %s", networkNameString.c_str(), toStr.TryGetCStr());
			pCurrentCollection->SetName(name.c_str());
			//Cache the network name
			pCurrentCollection->AddString("network", networkDef->GetName());

			//Notate the nodes being potentially transitioned
			char bufferFrom[64];
			atHashString fromStr(from->m_id);
			formatf(bufferFrom,"from %s", fromStr.TryGetCStr());
			char bufferTo[64];
			formatf(bufferTo,"to %s", toStr.TryGetCStr());
			pCurrentCollection->AddString(bufferFrom, bufferTo);

			//Store information that allows us to make sure we continue to track the same
			//transition. TODO - replace with a token stored in the calling code.
			iLastFrame = TIME.GetFrameCount();
			lastFrom = from;
			lastTo = to;
		}
	}

	if (pCurrentCollection) 
	{
		//Add conditionals to the list
		char buffer[64];
		pCurrentCollection->AddString(GetStringForResult(buffer, sizeof(buffer), iConditionIndex, iPassed), GetStringForCondition(iConditionType));
	}
}

void DR_TrackMoveInterface::RecordTransition(const mvNetworkDef* networkDef, const crCreature* creature, const mvNodeStateBaseDef* stateMachine, const mvNodeStateBaseDef* from, const mvNodeStateBaseDef* to)
{
	const phInst *pInst = debugPlayback::GetInstFromCreature(creature);
	if (!pInst)
	{
		return;
	}

	MoveTransitionStringCollection *pEvent = AddPhysicsEvent<MoveTransitionStringCollection>(pInst);
	if (pEvent)
	{
		atString networkNameString(networkDef->GetName());
		if(networkNameString.StartsWith("memory:$"))
		{
			int index = networkNameString.LastIndexOf(":");
			if(index != -1)
			{
				atString tempString(&networkNameString.c_str()[index + 1]);
				networkNameString = tempString;
			}
		}
		atVarString name("%s: TRANSITION", networkNameString.c_str());
		pEvent->SetName(name.c_str());
		pEvent->AddString("network", networkNameString);

		//Notate the nodes being potentially transitioned
		char bufferFrom[64];
		char bufferTo[128];
		atHashString machineStr(stateMachine->m_id);
		atHashString fromStr(from->m_id);
		atHashString toStr(to->m_id);
		formatf(bufferFrom,"in %s", machineStr.TryGetCStr());
		formatf(bufferTo,"from %s to %s", fromStr.TryGetCStr(), toStr.TryGetCStr());
		pEvent->AddString(bufferFrom, bufferTo);
	}
}

void DR_TrackMoveInterface::RecordSetAnimValue(const phInst *pInst, const char *pValueName, float fValue) const
{
	MoveSetValue *pEvent = AddPhysicsEvent<MoveSetValue>(pInst);
	if (pEvent)
	{
		char buffer[64];
		formatf(buffer,"MV:SET: %s", pValueName);
		pEvent->SetData(fValue, buffer);
	}
}

void DR_TrackMoveInterface::RecordGetAnimValue(const phInst *pInst, const char *pValueName, float fValue) const
{
	MoveGetValue *pEvent = AddPhysicsEvent<MoveGetValue>(pInst);
	if (pEvent)
	{
		char buffer[64];
		formatf(buffer,"MV:GET: %s", pValueName);
		pEvent->SetData(fValue, buffer);
	}
}

void DR_TrackMoveInterface::RecordSetAnimValue(const phInst *pInst, const char *pStringName, 
											   const char *pString1id, const char *pString1, 
											   const char *pString2id, const char *pString2) const
{
	MoveSetValueStringCollection *pEvent = AddPhysicsEvent<MoveSetValueStringCollection>(pInst);
	if (pEvent)
	{
		pEvent->SetName(pStringName);
		pEvent->AddString(pString1id, pString1);
		if (pString2id && Verifyf(pString2, "%s identified but no value string passed in", pString2id))
		{
			pEvent->AddString(pString2id, pString2);
		}
	}
}
#endif

//Included last to get all the class declarations within the CPP file
using namespace rage;
using namespace debugPlayback;
#include "DebugAnimation_parser.h"

#endif //DR_ANIMATIONS_ENABLED
