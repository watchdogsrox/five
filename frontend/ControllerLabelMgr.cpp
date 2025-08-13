//
// ControllerLabelMgr.cpp
//
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//

// --- Include Files ------------------------------------------------------------
#include "parser/tree.h"
#include "parser/treenode.h"
#include "parser/manager.h"

#include "ControllerLabelMgr.h"
#include "Text/TextConversion.h"
#include "frontend/ui_channel.h"
#include "frontend/PauseMenu.h"
#include "Scene/DataFileMgr.h"

#include "fwsys/timer.h"
#include "fwsys/gameskeleton.h"

FRONTEND_MENU_OPTIMISATIONS()


u32 CControllerLabelMgr::m_uLabelSwapDelay;
u32 CControllerLabelMgr::m_uTimeOfLastSwap;
bool CControllerLabelMgr::m_bLabelsDirty;
CControllerLabelMgr::ControllerMap CControllerLabelMgr::m_AllControls;
u32 CControllerLabelMgr::m_uCurrentContext;
u32 CControllerLabelMgr::m_uCurrentConfig;
atArray<sControlGroup::Label> CControllerLabelMgr::m_CurrentControlLabels;

CControllerLabelMgr::CControllerLabelMgr() {}
CControllerLabelMgr::~CControllerLabelMgr() {}

void CControllerLabelMgr::Update()
{
	if(m_uLabelSwapDelay > 0)
	{
		if(sysTimer::GetSystemMsTime() - m_uTimeOfLastSwap > m_uLabelSwapDelay)
		{
			AdjustLabels();
			m_uTimeOfLastSwap = sysTimer::GetSystemMsTime();
		}
	}
}

void CControllerLabelMgr::Init(unsigned initMode)
{
	m_AllControls.Reset();
	m_uLabelSwapDelay = 0;
	m_uTimeOfLastSwap = 0;
	m_bLabelsDirty = false;

	LoadDataXMLFile(initMode);
}

void CControllerLabelMgr::Shutdown(unsigned UNUSED_PARAM(shutdownMode) )
{
	m_AllControls.Reset();
	m_uLabelSwapDelay = 0;
	m_uTimeOfLastSwap = 0;
	m_bLabelsDirty = false;
}

void CControllerLabelMgr::LoadDataXMLFile(const unsigned initMode)
{
	if (INIT_CORE == initMode)
	{
		Shutdown(SHUTDOWN_CORE);
		const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile((CDataFileMgr::DataFileType)GetPlatformControlsFile());
		if (DATAFILEMGR.IsValid(pData))
		{
			// Read in this particular file
			LoadDataXMLFile(pData->m_filename);

			// Get next file
			pData = DATAFILEMGR.GetPrevFile(pData);
		}
	}
}

void CControllerLabelMgr::LoadDataXMLFile(const char* pFileName)
{
	if (AssertVerify(pFileName))
	{
		parTree* pTree = PARSER.LoadTree(pFileName, "xml");
		if(pTree)
		{
			parTreeNode* pRootNode = pTree->GetRoot();
			if(pRootNode)
			{
				m_uLabelSwapDelay = pRootNode->GetElement().FindAttributeIntValue("display_duration", 0);

				parTreeNode::ChildNodeIterator contexts = pRootNode->BeginChildren();
				for(; contexts != (*contexts)->EndChildren(); ++contexts)
				{
					if(stricmp((*contexts)->GetElement().GetName(), "context")    == 0)
					{
						u32 hashKey = atStringHash((*contexts)->GetElement().FindAttributeAnyCase("scheme")->GetStringValue());
						if(hashKey > 0)
						{
							sControlGroup ctrlGrp;
							m_AllControls.Insert(hashKey, ctrlGrp);

							parTreeNode::ChildNodeIterator labelGrps = (*contexts)->FindChildWithName("labelgroups");
							if((*labelGrps))
							{
								parTreeNode::ChildNodeIterator lblGrp = (*labelGrps)->BeginChildren();
								for(; lblGrp != (*lblGrp)->EndChildren(); ++lblGrp)
								{
									atArray<u32> lblArray;
									int lblId = (*lblGrp)->GetElement().FindAttributeIntValue("id", -1);
									parTreeNode::ChildNodeIterator labels = (*lblGrp)->BeginChildren();
									m_AllControls.Access(hashKey)->m_LabelGroups.Insert(lblId);
									for(; labels != (*labels)->EndChildren(); ++labels)
									{
										u32 labelHash = atStringHash((*labels)->GetData());
										// Try to find a special context attribute on the label element
										parAttribute* labelAtt = (*labels)->GetElement().FindAttributeAnyCase("context");
										u32 contextHash = labelAtt ? atStringHash(labelAtt->GetStringValue()) : 0;
										m_AllControls.Access(hashKey)->m_LabelGroups.Access(lblId)->PushAndGrow(sControlGroup::Label(labelHash, contextHash));
									}
								}
							}

							parTreeNode::ChildNodeIterator configGrps = (*contexts)->FindChildWithName("controlgroups");
							for(; configGrps != (*configGrps)->EndChildren(); ++configGrps)
							{
								parTreeNode::ChildNodeIterator controls = (*configGrps)->BeginChildren();
								for(; controls != (*controls)->EndChildren(); ++controls)
								{
									u32 controlIndex = (*controls)->GetElement().FindAttributeIntValue("type", 0);
									m_AllControls.Access(hashKey)->m_Configs.Insert(controlIndex);
									
									parTreeNode::ChildNodeIterator inputs = (*controls)->BeginChildren();
									for(; inputs != (*inputs)->EndChildren(); ++inputs)
									{
										int labelId = (*inputs)->GetElement().FindAttributeIntValue("labelgroup_id", 0);
										m_AllControls.Access(hashKey)->m_Configs.Access(controlIndex)->PushAndGrow(labelId);
									}
								}
							}
						}
					}
				}
			}

			delete pTree;
		}
	}
}

void CControllerLabelMgr::SetLabelScheme(u32 uContext, u32 uConfig)
{
	m_uTimeOfLastSwap = sysTimer::GetSystemMsTime();
	m_uCurrentContext = uContext;
	m_uCurrentConfig = uConfig;
	m_CurrentControlLabels.clear();

	sControlGroup* pCtrlGrp = m_AllControls.Access(m_uCurrentContext);
	if(pCtrlGrp)
 	{
		atArray<int>* pConfigArray = pCtrlGrp->m_Configs.Access(m_uCurrentConfig);
		if(pConfigArray)
		{
 			for(int i = 0; i < pConfigArray->GetCount(); ++i)
 			{
				atArray<sControlGroup::Label>* pLabels = pCtrlGrp->m_LabelGroups.Access((*pConfigArray)[i]);
				if(pLabels)
				{
					int iIndexToUse = 0;
					if (pLabels->GetCount() > 1)
					{
						while(ShouldSkipLabelDueToContext((*pLabels)[iIndexToUse].m_ContextHash))
						{
							iIndexToUse++;
							if (iIndexToUse > (pLabels->GetCount() - 1))
							{
								uiAssertf(false, "Couldn't find a valid controller label. Make sure a label is possible for slot %d", i);
								iIndexToUse = 0;
								break;
							}
						}
					}
 					m_CurrentControlLabels.PushAndGrow((*pLabels)[iIndexToUse]);
				}
 			}
 		}
 	}

	m_bLabelsDirty = true;
}

void CControllerLabelMgr::AdjustLabels()
{
	sControlGroup* pCtrlGrp = m_AllControls.Access(m_uCurrentContext);
	if(pCtrlGrp)
	{
		atArray<int>* pConfigArray = pCtrlGrp->m_Configs.Access(m_uCurrentConfig);
		if(pConfigArray)
		{
			for(int i = 0; i < pConfigArray->GetCount(); ++i)
			{
				atArray<sControlGroup::Label>* pLabels = pCtrlGrp->m_LabelGroups.Access((*pConfigArray)[i]);
				if(pLabels)
				{
					int index = pLabels->Find(m_CurrentControlLabels[i]);
					index++;
					if (index > (pLabels->GetCount() - 1))
					{
						index = 0;
					}

					int iStartingIndex = index;
					while(ShouldSkipLabelDueToContext((*pLabels)[index].m_ContextHash))
					{
						index++;
						if (index > (pLabels->GetCount() - 1))
						{
							index = 0;
						}

						if(index == iStartingIndex)
						{
							uiAssertf(false, "Couldn't find a valid controller label. Make sure a label is possible for slot %d", i);
							index = 0;
							break;
						}
					}

					m_CurrentControlLabels[i] = (*pLabels)[index];
				}
			}
		}
	}

	m_uTimeOfLastSwap = sysTimer::GetSystemMsTime();
	m_bLabelsDirty = true;
}

bool CControllerLabelMgr::ShouldSkipLabelDueToContext(u32 uContext)
{
	if (uContext == ATSTRINGHASH("USING_ALTERNATE_HANDBRAKE", 0x2a056586))
	{
		// Skip this label if it's marked to only be used when we ARE using alternate handbrake controls and we're NOT using alternate handbrake controls
		return CPauseMenu::GetMenuPreference(PREF_ALTERNATE_HANDBRAKE) <= 0;
	}
	
	if (uContext ==  ATSTRINGHASH("NOT_USING_ALTERNATE_HANDBRAKE", 0x47ac2904))
	{
		// Skip this label if it's marked to only be used when NOT using alternate handbrake controls and we ARE using alternate handbrake controls
		return CPauseMenu::GetMenuPreference(PREF_ALTERNATE_HANDBRAKE) > 0;
	}

	if(uContext == ATSTRINGHASH("USING_ALTERNATE_HANDBRAKE_SPC", 0x8667D645))
	{
		bool bAltDriveBy = CPauseMenu::GetMenuPreference(PREF_ALTERNATE_DRIVEBY) == TRUE;
		bool bAltHandbrake = CPauseMenu::GetMenuPreference(PREF_ALTERNATE_HANDBRAKE) <= 0;
		return  bAltHandbrake && !bAltDriveBy;
	}

	if(uContext == ATSTRINGHASH("ALLOW_MOVE_WHEN_ZOOMED", 0x8ED2CFD1))
	{
		return !(CPauseMenu::GetMenuPreference(PREF_SNIPER_ZOOM) == TRUE);
	}

	if(uContext == ATSTRINGHASH("NO_MOVE_WHEN_ZOOMED", 0x33FDA9B9))
	{
		return !(CPauseMenu::GetMenuPreference(PREF_SNIPER_ZOOM) == FALSE);
	}

	if(uContext == ATSTRINGHASH("AIM_AND_SHOOT", 0x8611F8C5))
	{
		return !(CPauseMenu::GetMenuPreference(PREF_ALTERNATE_DRIVEBY) == TRUE);
	}

	if(uContext == ATSTRINGHASH("NO_AIM_JUST_SHOOT", 0x4071A217))
	{
		return !(CPauseMenu::GetMenuPreference(PREF_ALTERNATE_DRIVEBY) == FALSE);
	}

	return false;
}

int CControllerLabelMgr::GetPlatformControlsFile()
{
#if __XENON
	return CDataFileMgr::CONTROLLER_LABELS_FILE_360;
#elif __PPU
	if(CPauseMenu::GetMenuPreference(PREF_ACCEPT_IS_CROSS))
		return CDataFileMgr::CONTROLLER_LABELS_FILE_PS3;
	else
		return CDataFileMgr::CONTROLLER_LABELS_FILE_PS3_JPN;
#elif RSG_ORBIS
	if(CPauseMenu::GetMenuPreference(PREF_ACCEPT_IS_CROSS))
		return CDataFileMgr::CONTROLLER_LABELS_FILE_ORBIS;
	else
		return CDataFileMgr::CONTROLLER_LABELS_FILE_ORBIS_JPN;
#elif RSG_DURANGO
	return CDataFileMgr::CONTROLLER_LABELS_FILE_DURANGO;
#else
	return CDataFileMgr::CONTROLLER_LABELS_FILE;
#endif
}