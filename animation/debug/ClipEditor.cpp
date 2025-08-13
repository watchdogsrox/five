// 
// animation/debug/ClipEditor.cpp 
// 
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved. 
// 

#if __BANK

#include "ClipEditor.h"

// system includes
#include <string>
#include <time.h>

// rage includes
#include "bank/bank.h"
#include "bank/bkmgr.h"
#include "bank/msgbox.h"
#include "bank/slider.h"
#include "crclip/clip.h"
#include "crclip/clipanimation.h"
#include "crmetadata/properties.h"
#include "crmetadata/propertyattributes.h"
#include "crmetadata/tagiterators.h"
#include "crmetadata/tags.h"
#include "fwanimation/directorcomponentsyncedscene.h"

// game includes
#include "animation/debug/AnimViewer.h"
#include "scene/Physical.h"
#include "Objects/object.h"
#include "Peds/ped.h"
#include "system/FileMgr.h"
#include "task/default/TaskAmbient.h"

ANIM_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
//	Clip editor static declarations
//////////////////////////////////////////////////////////////////////////

fwDebugBank* CAnimClipEditor::m_pBank = NULL;

//clip editor RAG widget groups
bkGroup* CAnimClipEditor::m_pTagGroup = NULL;
bkGroup* CAnimClipEditor::m_pPropertyGroup = NULL;

pgRef<crClip> CAnimClipEditor::m_pClip(NULL);

bool CAnimClipEditor::m_bRequestLoadSelectedClipFromDisk = false;
bool CAnimClipEditor::m_bRequestSaveSelectedClipToDisk = false;

CDebugClipSelector CAnimClipEditor::m_clipEditAnimSelector(true, false, false);

atArray<CAnimClipEditor::sAttribute*> CAnimClipEditor::m_specialAttributes;

bool CAnimClipEditor::m_bAllowSaving = false;

int CAnimClipEditor::ms_iApprovalFlag = 0;

int CAnimClipEditor::ms_iSelectedTag = -1;

float CAnimClipEditor::ms_fDistanceInterval = 0.5f;
float CAnimClipEditor::ms_fPhaseIncrement = 0.01f;

//////////////////////////////////////////////////////////////////////////
//	Supporting data for adding new properties
//////////////////////////////////////////////////////////////////////////
atArray< const char * > CAnimClipEditor::m_attributeTypeNames;
int CAnimClipEditor::m_newAttributeType;
char CAnimClipEditor::m_newName[CAnimClipEditor::nameBufferSize];
char CAnimClipEditor::m_newAttributeName[CAnimClipEditor::nameBufferSize];

// clip previewing
atHashString CAnimClipEditor::ms_AuthorName("Not Set",0x789B6CA5);
bool CAnimClipEditor::ms_previewClipOnFocusEntity = false;
float CAnimClipEditor::ms_previewPhase = 0.0f;
float CAnimClipEditor::ms_previewTime = 0.0f;
bkSlider * CAnimClipEditor::ms_pPreviewTimeSlider = NULL;
fwSyncedSceneId CAnimClipEditor::ms_previewScene = -1;

s32 CAnimClipEditor::m_bitSetSize = 0;
float CAnimClipEditor::m_UseLeftFootStrafeTransitionRatio = 0.5f;

const atHashWithStringNotFinal CAnimClipEditor::m_UseLeftFootTransitionTag("USE_LEFT_FOOT_TRANSITION",0x8F7E474);
const atHashWithStringNotFinal CAnimClipEditor::m_UseLeftFootStrafeTransitionTag("USE_LEFT_FOOT_STRAFE_TRANSITION",0xC9AF24C2);
const atHashWithStringNotFinal CAnimClipEditor::m_BlockTransitionTag("BLOCK_TRANSITION",0x70ABE2B4);

atString FormatHashString(u32 hash, const char *string)
{
	if(string && strlen(string) > 0)
	{
		return atString(string);
	}
	else
	{
		return atVarString("%u", hash);
	}
}

/////////////////////////////////////////////////////////////////////////////
////	CAnimClipEditor
////////////////////////////////////////////////////////////////////////////

void CAnimClipEditor::Initialise()
{
	m_pClip = NULL;

	m_attributeTypeNames.PushAndGrow("Select attribute type");
	//init the list of property types

	for(int i=crPropertyAttribute::kTypeNone+1; i<crPropertyAttribute::kTypeNum; ++i)
	{
		const crPropertyAttribute::TypeInfo* pInfo = crPropertyAttribute::FindTypeInfo(crPropertyAttribute::eType(i));
		m_attributeTypeNames.PushAndGrow(pInfo->GetName());
	}

	if (m_pBank)
	{
		m_pBank->Shutdown();
	}

	m_pBank = fwDebugBank::CreateBank("Anim Clip editor", MakeFunctor(CAnimClipEditor::CreateMainWidgets), MakeFunctor(CAnimClipEditor::ShutdownMainWidgets));
}


void CAnimClipEditor::Shutdown()
{
	ShutdownMainWidgets();

	if (m_pClip)
	{
		m_pClip->Release();
		m_pClip = NULL;
	}

	if (m_pBank)
	{
		m_pBank->Shutdown();
		m_pBank=NULL;
	}
}

void CAnimClipEditor::Update()
{
	if (!m_pBank)
	{
		return;
	}

	// Do we want to load?
	if (m_bRequestLoadSelectedClipFromDisk)
	{
		LoadSelectedClipFromDisk();
		m_bRequestLoadSelectedClipFromDisk = false;
	}

	// Do we want to save?
	if (m_bRequestSaveSelectedClipToDisk)
	{
		SaveSelectedClipToDisk();
		m_bRequestSaveSelectedClipToDisk = false;
	}
}

void CAnimClipEditor::CreateMainWidgets()
{
	g_ClipTagMetadataManager.Load();

	// do some setup first
	m_attributeTypeNames.Reset();
	m_attributeTypeNames.PushAndGrow("Select attribute type");
	//init the list of property types
	for(int i=crPropertyAttribute::kTypeNone+1; i<crPropertyAttribute::kTypeNum; ++i)
	{
		const crPropertyAttribute::TypeInfo* pInfo = crPropertyAttribute::FindTypeInfo(crPropertyAttribute::eType(i));
		m_attributeTypeNames.PushAndGrow(pInfo->GetName());
	}

	// add the necessary widgets
	m_clipEditAnimSelector.AddWidgets(m_pBank, SelectClip, SelectClip);

	m_pBank->AddText("Author", &ms_AuthorName, true);
	m_pBank->AddToggle("Preview on focus entity",&ms_previewClipOnFocusEntity, CAnimClipEditor::StartPreviewClip);
	m_pBank->AddSlider("Preview phase", &ms_previewPhase, 0.0f, 1.0f,0.01f,CAnimClipEditor::UpdatePreviewPhase);
	ms_pPreviewTimeSlider =
		m_pBank->AddSlider("Preview time", &ms_previewTime, 0.0f, 1.0f, 0.01f, CAnimClipEditor::UpdatePreviewTime);
	m_pBank->AddCombo("Asset Directory", &CDebugClipDictionary::ms_AssetFolderIndex, ASSET_FOLDERS, &CDebugClipDictionary::ms_AssetFolders[0]); 
	m_pBank->AddButton( "Reload clip from disk", RequestLoadSelectedClipFromDisk );
	m_pBank->AddButton( "Save clip to disk", RequestSaveSelectedClipToDisk );

	const char* approvalStrings[] =
	{
		"None",
		"Placeholder",
		"Complete",
		"Approved",
	};
	
	m_pBank->PushGroup("Animation approval tags", false);
	{
		m_pBank->AddCombo("Pick approval status", &ms_iApprovalFlag, NELEM(approvalStrings), approvalStrings);
		m_pBank->AddButton("Set Approval  status", SetApprovalProperty);
	}
	m_pBank->PopGroup();

	m_pBank->PushGroup("Foot Tags", false);
	{
		m_pBank->AddText("Selected Tag", &ms_iSelectedTag, true);
		m_pBank->AddButton("Update Tag Phase", SetPreviewedTagPhaseFromPreviewSlider);
		m_pBank->AddButton("Next Tag", PreviewNextFootTag);
		m_pBank->AddButton("Previous Tag", PreviewPreviousFootTag);
		m_pBank->AddButton("Clear All Foot Tags", ClearAllFootTagsButton);
		m_pBank->AddButton("Generate USE_LEFT_FOOT_TRANSITION Tags", GenerateUseLeftFootTransitionTagsButton);
		m_pBank->AddButton("Clear All USE_LEFT_FOOT_TRANSITION Tags", ClearAllUseLeftFootTransitionTagsButton);
		m_pBank->AddButton("Generate TO STRAFE USE_LEFT_FOOT_STRAFE_TRANSITION Tags", GenerateToStrafeUseLeftFootStrafeTransitionTagsButton);
		m_pBank->AddButton("Generate TO STRAFE USE_LEFT_FOOT_STRAFE_TRANSITION Tags For Dictionary", GenerateToStrafeUseLeftFootStrafeTransitionTagsForDictionaryButton);
		m_pBank->AddButton("Generate FROM STRAFE USE_LEFT_FOOT_STRAFE_TRANSITION Tags", GenerateFromStrafeUseLeftFootStrafeTransitionTagsButton);
		m_pBank->AddButton("Generate FROM STRAFE USE_LEFT_FOOT_STRAFE_TRANSITION Tags For Dictionary", GenerateFromStrafeUseLeftFootStrafeTransitionTagsForDictionaryButton);
		m_pBank->AddButton("Clear All USE_LEFT_FOOT_STRAFE_TRANSITION Tags", ClearAllUseLeftFootStrafeTransitionTagsButton);
		m_pBank->AddButton("Generate BLOCK_TRANSITION Tags", GenerateBlockTransitionTagsButton);
		m_pBank->AddButton("Generate BLOCK_TRANSITION Tags For Dictionary", GenerateBlockTransitionTagsForDictionaryButton);
		m_pBank->AddButton("Clear All BLOCK_TRANSITION Tags", ClearAllBlockTransitionTagsButton);
	}
	m_pBank->PopGroup();

	m_pBank->PushGroup("Distance Tags", false);
	{
		m_pBank->AddText("Selected Tag", &ms_iSelectedTag, true);
		m_pBank->AddButton("Update Tag Phase", SetPreviewedTagPhaseFromPreviewSlider);
		m_pBank->AddButton("Next Tag", PreviewNextDistanceMarkerTag);
		m_pBank->AddButton("Previous Tag", PreviewPreviousDistanceMarkerTag);
		m_pBank->AddButton("Clear All Distance Marker Tags", ClearAllDistanceMarkerTagsButton);
		m_pBank->AddSlider("Distance Marker Interval", &ms_fDistanceInterval, 0.1f, 5.0f, 0.01f);
		m_pBank->AddSlider("Phase Increment", &ms_fPhaseIncrement, 0.001f, 0.1f, 0.001f);
		m_pBank->AddButton("Generate Distance Marker Tags", GenerateDistanceMarkerTagsButton);
	}
	m_pBank->PopGroup();

	g_ClipTagMetadataManager.AddWidgets(m_pBank);

	m_pBank->PushGroup("Edit tags", true, NULL); // "Anim Events"
	{
		//we'll fill this in later - see RegenerateEventTagWidgets()
		m_pBank->AddButton( "Clear All Tags", ClearAllTagsButton );
		m_pTagGroup = m_pBank->PushGroup("Tags", true, NULL); // "Event list"
		{
		}
		m_pBank->PopGroup();
		m_pBank->PushGroup("Add a new tag", false, NULL);
		{
			m_pBank->AddText("New tag name:", m_newName, nameBufferSize, false );
			m_pBank->AddButton("Add new tag", AddTagButton );
		}
		m_pBank->PopGroup();
	}
	m_pBank->PopGroup();

	m_pBank->PushGroup("Edit properties", true, NULL); // "Anim Events"
	{
		//we'll fill this in later - see RegenerateEventTagWidgets()
		m_pPropertyGroup = m_pBank->PushGroup("Properties", true, NULL); // "Event list"
		{
		}
		m_pBank->PopGroup();
		m_pBank->PushGroup("Add a new property", false, NULL); // Property addition
		{
			m_pBank->AddText("New property name:", m_newName, nameBufferSize, false );
			m_pBank->AddButton("Add new property", AddPropertyButton );
		}
		m_pBank->PopGroup();
	}
	m_pBank->PopGroup();

	CDebugClipDictionary::LoadClipDictionaries(true);

	if (m_pClip)
	{
		m_pClip->Release();
		m_pClip = NULL;
	}
	m_pClip = m_clipEditAnimSelector.GetSelectedClip();
	if (m_pClip)
	{
		m_pClip->AddRef();
	}

	LoadSelectedClipFromDisk();

	Refresh();
}

void CAnimClipEditor::ShutdownMainWidgets()
{
	if (m_pBank)
	{
		m_clipEditAnimSelector.RemoveWidgets(m_pBank);

		g_ClipTagMetadataManager.RemoveWidgets();

		m_pBank->RemoveAllMainWidgets();
	}

	ms_pPreviewTimeSlider = NULL;
}

void CAnimClipEditor::Refresh()
{
	if (m_pBank)
	{
		if(ms_previewClipOnFocusEntity)
		{
			ms_previewClipOnFocusEntity = false;

			CAnimClipEditor::StartPreviewClip();

			ms_previewClipOnFocusEntity = true;

			CAnimClipEditor::StartPreviewClip();
		}
	}
}

void CAnimClipEditor::AddPropertyButton()
{
	if (strlen(m_newName)>0 )
	{
		AddProperty( m_newName );
		
		UpdatePropertiesCollectionWidgets();
	}
}

void CAnimClipEditor::AddAttributeToPropertyButton(crProperty* pProp)
{
	if (pProp && m_newAttributeType>crPropertyAttribute::kTypeNone)
	{
		AddAttribute(*pProp, m_newAttributeName, (crPropertyAttribute::eType)(m_newAttributeType));

		UpdateTagWidgets();
		UpdatePropertiesCollectionWidgets();
	}
}

void CAnimClipEditor::RemoveAttributeFromPropertyButton(crProperty* pProp)
{
	if (pProp && strlen(m_newAttributeName)>0)
	{
		atString message("Remove attribute");
		message += m_newAttributeName;
		message += "from tag/property ";
		message += FormatHashString(pProp->GetKey(), pProp->GetName());
		message += "?";

		int result = bkMessageBox("Remove attribute", message.c_str(),bkMsgYesNo, bkMsgQuestion);

		if (result)
		{
			pProp->RemoveAttribute(m_newAttributeName);

			UpdateTagWidgets();
			UpdatePropertiesCollectionWidgets();
		}
	}
}

void CAnimClipEditor::AddTagButton()
{
	if (strlen(m_newName)>0 )
	{
		AddTag( m_newName );
		
		UpdateTagWidgets();
	}
}


void CAnimClipEditor::AddProperty( const char * name )
{
	USE_MEMBUCKET(MEMBUCKET_ANIMATION);

	if (m_pClip)
	{
		crProperties* pProps = m_pClip->GetProperties();

		if (pProps)
		{
			crProperty newProp;
			newProp.SetName(name);

			pProps->AddProperty(newProp);
		}
	}
}

void CAnimClipEditor::AddAttribute( crProperty& prop, const char * name, crPropertyAttribute::eType type )
{
	USE_MEMBUCKET(MEMBUCKET_ANIMATION);
	
	crPropertyAttribute* pNewAttrib = crPropertyAttribute::Allocate( type);

	if(pNewAttrib)
	{
		pNewAttrib->SetName(name);

		prop.AddAttribute(*pNewAttrib);

		//get rid of the original - we don't need it any more
		delete pNewAttrib;
	}
}

void CAnimClipEditor::DeleteProperty(crProperty* pProp)
{

	if (m_pClip && pProp && m_pClip->GetProperties() )
	{

		atString message("Delete property ");
		message += FormatHashString(pProp->GetKey(), pProp->GetName());
		message += "?";

		int result = bkMessageBox("Delete clip property", message.c_str(),bkMsgYesNo, bkMsgQuestion);

		if (result)
		{
			crProperties* pProps = m_pClip->GetProperties();

			if (!pProps->RemoveProperty(pProp->GetKey()))
			{
				animAssertf(0, "Failed to delete property %s from the selected clip", FormatHashString(pProp->GetKey(), pProp->GetName()).c_str());
			}

			UpdatePropertiesCollectionWidgets();
		}
	}
}

void CAnimClipEditor::AddTag(const char * name)
{
	USE_MEMBUCKET(MEMBUCKET_ANIMATION);

	if (m_pClip)
	{
		crTags* pTags = m_pClip->GetTags();

		if (pTags)
		{
			rage::crTag newTag;
			newTag.GetProperty().SetName(name);
			pTags->AddTag(newTag);
		}
	}
}

void CAnimClipEditor::UpdatePropertiesCollectionWidgets()
{
	if (m_pPropertyGroup)
	{
		while (m_pPropertyGroup->GetChild())
		{
			m_pPropertyGroup->GetChild()->Destroy();
		}
	}

	if (m_pClip)
	{
		m_pBank->SetCurrentGroup(*m_pPropertyGroup);
		{
			crProperties* pProps = m_pClip->GetProperties();

			if (pProps)
			{
				PropertiesWidgetIterator it;

				pProps->ForAllProperties(it);
			}
		}
		m_pBank->UnSetCurrentGroup(*m_pPropertyGroup);
	}
}

bool CAnimClipEditor::PropertiesWidgetIterator::Callback(crProperty& prop)
{
		
	m_pBank->PushGroup(prop.GetName());
	{
		AddPropertyWidgets(prop);

		if (prop.GetName())
		{
			m_pBank->AddButton("Remove this property", datCallback(CFA1(DeleteProperty), &prop));
		}

	}
	m_pBank->PopGroup();
	return true;
}

void CAnimClipEditor::AddPropertyWidgets(crProperty& prop)
{
	//add a set of widgets for each attribute
	prop.ForAllAttributes(AddAttributeWidgets, NULL);

	// Add widgets for adding new attributes
	
	m_pBank->PushGroup("Add a new attribute", false, NULL);
	{
		m_pBank->AddText("New attribute name:", m_newAttributeName, nameBufferSize, false);
		m_pBank->AddCombo("New attribute type:", &m_newAttributeType, m_attributeTypeNames.GetCount(), &m_attributeTypeNames[0]);
		m_pBank->AddButton("Add new attribute", datCallback(CFA1(AddAttributeToPropertyButton), (void*)&prop ));
	}
	m_pBank->PopGroup();

	// and for removing them

	m_pBank->PushGroup("Remove an attribute", false, NULL);
	{
		m_pBank->AddText("Attribute to remove:", m_newAttributeName, nameBufferSize, false);
		m_pBank->AddButton("Remove attribute", datCallback(CFA1(RemoveAttributeFromPropertyButton), (void*)&prop ));
	}
	m_pBank->PopGroup();

}

bool CAnimClipEditor::AddAttributeWidgets(const crPropertyAttribute& constAttrib,  void* UNUSED_PARAM(data))
{
	USE_MEMBUCKET(MEMBUCKET_ANIMATION);

	// seems to be the only way to grab a non-const reference to an attribute...
	crPropertyAttribute& attrib = const_cast<crPropertyAttribute&>(constAttrib);

	atString groupName(FormatHashString(attrib.GetKey(), attrib.GetName()));
	groupName +="(";
	groupName+= attrib.GetTypeName();
	groupName += ")";

	m_pBank->PushGroup(groupName.c_str());
	{
		switch(attrib.GetType())
		{
		case crPropertyAttribute::kTypeBool:
			{
				crPropertyAttributeBool& derivedAttrib = static_cast<crPropertyAttributeBool&>(attrib);
				bool& attribValue = derivedAttrib.GetBool();
				m_pBank->AddToggle("value", &attribValue);
			}
			break;
		case crPropertyAttribute::kTypeFloat:
			{
				crPropertyAttributeFloat& derivedAttrib = static_cast<crPropertyAttributeFloat&>(attrib);
				float& attribValue = derivedAttrib.GetFloat();
				m_pBank->AddSlider( "value", &attribValue, bkSlider::FLOAT_MIN_VALUE, bkSlider::FLOAT_MAX_VALUE, 0.01f );
			}
			break;
		case crPropertyAttribute::kTypeInt:
			{
				crPropertyAttributeInt& derivedAttrib = static_cast<crPropertyAttributeInt&>(attrib);
				s32& attribValue = derivedAttrib.GetInt();
				m_pBank->AddText( "value", &attribValue, false );
			}
			break;
		case crPropertyAttribute::kTypeVector3:
			{
				crPropertyAttributeVector3& derivedAttrib = static_cast<crPropertyAttributeVector3&>(attrib);
				Vec3V_Ref attribValue = derivedAttrib.GetVector3();
				m_pBank->AddVector( "value", &attribValue, bkSlider::FLOAT_MIN_VALUE, bkSlider::FLOAT_MAX_VALUE, 0.01f );
			}
			break;
		case crPropertyAttribute::kTypeVector4:
			{
				crPropertyAttributeVector4& derivedAttrib = static_cast<crPropertyAttributeVector4&>(attrib);
				Vec4V_Ref attribValue = derivedAttrib.GetVector4();
				m_pBank->AddVector( "value", &attribValue, bkSlider::FLOAT_MIN_VALUE, bkSlider::FLOAT_MAX_VALUE, 0.01f );
			}
			break;
		//case crPropertyAttribute::kTypeMatrix34: // TODO - uncomment this when AddVector(Mat34V) has been implemented
		//	{
		//		crPropertyAttributeMatrix34& derivedAttrib = static_cast<crPropertyAttributeMatrix34&>(attrib);
		//		Mat34V_Ref attribValue = derivedAttrib.GetMatrix34();
		//		m_pBank->AddVector( "value", &attribValue, -FLT_MAX, FLT_MAX, 0.01f );
		//	}
		//	break;
		case crPropertyAttribute::kTypeString:
			{
				crPropertyAttributeString& derivedAttrib = static_cast<crPropertyAttributeString&>(attrib);
				atString attribValue = derivedAttrib.GetString();
				sAttributeString* newString = rage_new sAttributeString();
				newString->m_pAttrib = &attrib;
				newString->m_bIsHash = false;
				animAssertf(attribValue.length() < (sAttributeString::bufferSize - 1), "Clip property attribute string \"%s\" is longer than RAG widget buffer %i and will be truncated!", attribValue.c_str(), sAttributeString::bufferSize);
				formatf(newString->m_buffer, "%s", attribValue.c_str());
				m_specialAttributes.PushAndGrow(newString);
				m_pBank->AddText( "string", newString->m_buffer, sAttributeString::bufferSize, false, datCallback(MFA(sAttribute::Save), newString));
			}
			break;
		case crPropertyAttribute::kTypeHashString:
			{
				crPropertyAttributeHashString& derivedAttrib = static_cast<crPropertyAttributeHashString&>(attrib);
				atHashString attribValue = derivedAttrib.GetHashString();
				sAttributeString* newString = rage_new sAttributeString();
				newString->m_pAttrib = &attrib;
				newString->m_bIsHash = true;
				animAssertf(attribValue.GetLength() < (sAttributeString::bufferSize - 1), "Clip property attribute has string \"%s\" is longer than RAG widget buffer %i and will be truncated!", attribValue.GetCStr(), sAttributeString::bufferSize);
				formatf(newString->m_buffer, "%s", attribValue.GetCStr());
				m_specialAttributes.PushAndGrow(newString);
				m_pBank->AddText( "string", newString->m_buffer, sAttributeString::bufferSize, false, datCallback(MFA(sAttribute::Save), newString));
			}
			break;
		case crPropertyAttribute::kTypeBitSet:
			{
				crPropertyAttributeBitSet& derivedAttrib = static_cast<crPropertyAttributeBitSet&>(attrib);
				atBitSet& attribValue = derivedAttrib.GetBitSet();
				for (int i=0; i<attribValue.GetNumBits(); i++)
				{
					char buf[64];
					formatf(buf, "BIT%d", i);
					m_pBank->AddToggle( buf, &attribValue, (u8)i );
				}
				m_pBank->AddText("Resize", &m_bitSetSize, false, datCallback(CFA1(ResizeBitset), (CallbackData)&attribValue));
			}
			break;
		case crPropertyAttribute::kTypeQuaternion:
			{
				crPropertyAttributeQuaternion& derivedAttrib = static_cast<crPropertyAttributeQuaternion&>(attrib);
				Quaternion attribValue = QUATV_TO_QUATERNION(derivedAttrib.GetQuaternion());
				sAttributeEuler* newEuler = rage_new sAttributeEuler();
				newEuler->m_pAttrib = &attrib;
				attribValue.ToEulers(newEuler->m_euler); 
				newEuler->m_bIsSituation = false;
				m_specialAttributes.PushAndGrow(newEuler);
				m_pBank->AddAngle( "x", &newEuler->m_euler.x, bkAngleType::RADIANS, datCallback(MFA(sAttribute::Save), newEuler) );
				m_pBank->AddAngle( "y", &newEuler->m_euler.y, bkAngleType::RADIANS, datCallback(MFA(sAttribute::Save), newEuler) );
				m_pBank->AddAngle( "z", &newEuler->m_euler.z, bkAngleType::RADIANS, datCallback(MFA(sAttribute::Save), newEuler) );
			}
			break;
		case crPropertyAttribute::kTypeSituation:
			{
				crPropertyAttributeSituation& derivedAttrib = static_cast<crPropertyAttributeSituation&>(attrib);
				TransformV& attribValue =derivedAttrib.GetSituation();
				
				//vector bit
				Vec3V_Ref position = attribValue.GetPositionRef();
				m_pBank->AddVector( "position", &position, bkSlider::FLOAT_MIN_VALUE, bkSlider::FLOAT_MAX_VALUE, 0.01f );

				//quaternion bit here
				Quaternion orientation = QUATV_TO_QUATERNION(attribValue.GetRotation());
				sAttributeEuler* newEuler = rage_new sAttributeEuler();
				newEuler->m_pAttrib = &attrib;
				orientation.ToEulers(newEuler->m_euler); 
				newEuler->m_bIsSituation = true;
				m_specialAttributes.PushAndGrow(newEuler);
				m_pBank->AddAngle( "orientation x", &newEuler->m_euler.x, bkAngleType::RADIANS, datCallback(MFA(sAttribute::Save), newEuler)  );
				m_pBank->AddAngle( "orientation y", &newEuler->m_euler.y, bkAngleType::RADIANS, datCallback(MFA(sAttribute::Save), newEuler)  );
				m_pBank->AddAngle( "orientation z", &newEuler->m_euler.z, bkAngleType::RADIANS, datCallback(MFA(sAttribute::Save), newEuler)  );


			}
			break;
		default:
			{
				m_pBank->AddTitle("Attribute type not supported");
			}
			break;
		}
	}
	m_pBank->PopGroup();

	return true;
}

void CAnimClipEditor::ResizeBitset( atBitSet* pSet)
{
	// back up our bits so we don't lose them
	atBitSet set;
	set = *pSet;

	// resize the bitset
	pSet->Init(m_bitSetSize);

	// copy the bits that still remain back in
	for (int i=0; i< pSet->GetNumBits(); i++)
	{
		if (i<set.GetNumBits())
		{
			pSet->Set(i, set.IsSet(i));
		}
	}

	UpdateTagWidgets();
	UpdatePropertiesCollectionWidgets();
}

void CAnimClipEditor::UpdateTagWidgets()
{
	if (m_pClip)
	{
		if (m_pClip->GetTags())
		{
			m_pClip->GetTags()->SortTags();
		}
	}

	//delete all the previous widgets
	if (m_pTagGroup)
	{
		while (m_pTagGroup->GetChild())
		{
			m_pTagGroup->GetChild()->Destroy();
		}
	}
	
	m_pBank->SetCurrentGroup(*m_pTagGroup);
	{
		// run through the tags and add widgets for each one
		crTags* pTags = m_pClip->GetTags();
		if(pTags)
		{
			crTagIterator it(*pTags);
			while(*it)
			{
				crTag* pTag = const_cast<crTag*>(*it);
				
				m_pBank->PushGroup(FormatHashString(pTag->GetKey(), pTag->GetName()));
				{
					// add the start and end sliders
					sIntervalFloat* pInterval = (sIntervalFloat*)pTag;
					m_pBank->AddSlider("start", &pInterval->start, 0.0f, 1.0f,  0.0001f, datCallback(CFA1(LimitTagPhaseStart), pInterval));
					m_pBank->AddSlider("end", &pInterval->end, 0.0f, 1.0f,  0.0001f, datCallback(CFA1(LimitTagPhaseEnd), pInterval) );
					m_pBank->AddButton("Set from preview phase",  datCallback(CFA1(SetPhaseFromPreviewSlider), pTag));

					crProperty& prop = pTag->GetProperty();

					AddPropertyWidgets(prop);

					m_pBank->AddButton("Remove this tag", datCallback(CFA1(RemoveTag), pTag));
				}
				m_pBank->PopGroup();

				++it;
			}
			m_pBank->AddButton( "Sort tags", UpdateTagWidgets );
		}
	}
	m_pBank->UnSetCurrentGroup(*m_pTagGroup);

	ms_iSelectedTag = -1;
}

void CAnimClipEditor::RemoveTag(crTag* pTag)
{
	if (m_pClip && m_pClip->GetTags() && pTag)
	{
		atString message("Remove tag ");
		message += FormatHashString(pTag->GetKey(), pTag->GetName());
		message += "?";

		int result = bkMessageBox("Delete clip property", message.c_str(),bkMsgYesNo, bkMsgQuestion);

		if (result)
		{
			m_pClip->GetTags()->RemoveTag(*pTag);

			UpdateTagWidgets();
		}
	}
}

void CAnimClipEditor::LimitTagPhaseStart(sIntervalFloat* pInterval)
{
	if ( pInterval )
	{
		if ( pInterval->start > pInterval->end )
		{
			// Push out the end
			pInterval->end = pInterval->start;
		}
	}
}

void CAnimClipEditor::LimitTagPhaseEnd(sIntervalFloat* pInterval)
{
	if ( pInterval )
	{
		if ( pInterval->end < pInterval->start )
		{
			// Bring forward the start
			pInterval->start = pInterval->end;
		}
	}
}

void CAnimClipEditor::SetPhaseFromPreviewSlider(crTag* pTag)
{
	if (m_pClip && m_pClip->GetTags() && pTag)
	{
		pTag->SetStartEnd(ms_previewPhase, ms_previewPhase);
	}
}

void CAnimClipEditor::ClearAllTags()
{
	if (m_pClip)
	{
		m_pClip->GetTags()->RemoveAllTags();
	}
}

void CAnimClipEditor::ClearAllFootTags()
{
	if (m_pClip)
	{
		m_pClip->GetTags()->RemoveAllTags(CClipEventTags::Foot);
	}
}

void CAnimClipEditor::GenerateUseLeftFootTransitionTags()
{
	if (m_pClip)
	{
		crTags* pTags = m_pClip->GetTags();
		if (pTags)
		{
			ClearAllMoveEventTags(m_pClip, m_UseLeftFootTransitionTag);

			pTags->SortTags();

			atArray<crTag> useLeftFootTransitionData;

			float fRightFootPhase = 0.f;
			bool bLastFootRight = false;

			crTagIterator it(*pTags);
			while (*it)
			{
				const crTag* pTag = (*it);
				++it;

				if (pTag->GetKey() == CClipEventTags::Foot)
				{
					bool bRightFoot = false;
					const crPropertyAttributeBool* pAttribRight = static_cast<const crPropertyAttributeBool*>(pTag->GetProperty().GetAttribute(CClipEventTags::Right.GetHash()));
					if (pAttribRight && pAttribRight->GetBool() == true)
					{
						bRightFoot = true;
					}

					if (bRightFoot)
					{
						fRightFootPhase = pTag->GetStart();
						bLastFootRight = true;
					}
					else
					{
						if (!IsClose(pTag->GetStart(), 0.f, 0.001f))
						{
							crTag newTag;
							crProperty& newProperty = newTag.GetProperty();
							newProperty.SetName("MoveEvent");

							crPropertyAttributeHashString moveEvent;
							moveEvent.SetName("MoveEvent");
							moveEvent.GetHashString() = m_UseLeftFootTransitionTag;
							newProperty.AddAttribute(moveEvent);

							float fEndPhase = Max(pTag->GetStart() - (0.033f / m_pClip->GetDuration()), 0.f);
							newTag.SetStartEnd(fRightFootPhase, fEndPhase);

							useLeftFootTransitionData.PushAndGrow(newTag);
						}

						bLastFootRight = false;
					}
				}
			}

			if (bLastFootRight)
			{
				crTag newTag;
				crProperty& newProperty = newTag.GetProperty();
				newProperty.SetName("MoveEvent");

				crPropertyAttributeHashString moveEvent;
				moveEvent.SetName("MoveEvent");
				moveEvent.GetHashString() = m_UseLeftFootTransitionTag;
				newProperty.AddAttribute(moveEvent);

				float fEndPhase = Max(1.f - (0.033f / m_pClip->GetDuration()), 0.f);
				newTag.SetStartEnd(fRightFootPhase, fEndPhase);

				useLeftFootTransitionData.PushAndGrow(newTag);
			}

			int iCount = useLeftFootTransitionData.GetCount();
			for(int i = 0; i < iCount; i++)
			{
				pTags->AddTag(useLeftFootTransitionData[i]);
			}
		}
	}
}

bool CAnimClipEditor::GenerateUseLeftFootStrafeTransitionTags(crClip* pClip, fiFindData UNUSED_PARAM(fileData), const char* UNUSED_PARAM(folderName))
{
	if (pClip)
	{
		crTags* pTags = pClip->GetTags();
		if (pTags)
		{
			ClearAllMoveEventTags(pClip, m_UseLeftFootStrafeTransitionTag);

			pTags->SortTags();

			atArray<crTag> useLeftFootTransitionData;
			const crTag* pFirstRightTag = NULL;
			const crTag* pLastRightTag = NULL;
			bool bFirstTagLeft = false;
			bool bLastTagLeft = false;

			crTagIterator it(*pTags);
			while (*it)
			{
				const crTag* pTag = (*it);
				++it;

				if (pTag->GetKey() == CClipEventTags::Foot)
				{
					const crPropertyAttributeBool* pAttribRight = static_cast<const crPropertyAttributeBool*>(pTag->GetProperty().GetAttribute(CClipEventTags::Right.GetHash()));
					if (pAttribRight && pAttribRight->GetBool() == false)
					{
						if (pFirstRightTag == NULL)
						{
							bFirstTagLeft = true;
						}

						// Get the next right foot
						const crTag* pNextTag = NULL;
						crTagIterator itNext(*pTags, pTag->GetStart());
						while (*itNext && pNextTag == NULL)
						{
							const crTag* pTagNext = (*itNext);
							++itNext;

							if (pTagNext->GetKey() == CClipEventTags::Foot)
							{
								const crPropertyAttributeBool* pAttribRight = static_cast<const crPropertyAttributeBool*>(pTagNext->GetProperty().GetAttribute(CClipEventTags::Right.GetHash()));
								if (pAttribRight && pAttribRight->GetBool() == true)
								{
									pNextTag = pTagNext;
								}
							}
						}

						if (pNextTag == NULL)
						{
							// Use the first right tag
							pNextTag = pFirstRightTag;
						}

						const crTag* pPrevTag = pLastRightTag;
						if (pPrevTag == NULL)
						{
							// Need to get the end right
							crTagIterator itPrev(*pTags);
							itPrev.End();
							while (*itPrev && pPrevTag == NULL)
							{
								const crTag* pTagPrev = (*itPrev);
								--itPrev;

								if (pTagPrev->GetKey() == CClipEventTags::Foot)
								{
									const crPropertyAttributeBool* pAttribRight = static_cast<const crPropertyAttributeBool*>(pTagPrev->GetProperty().GetAttribute(CClipEventTags::Right.GetHash()));
									if (pAttribRight && pAttribRight->GetBool() == true)
									{
										pPrevTag = pTagPrev;
									}
									else
									{
										bLastTagLeft = true;
									}
								}
							}
						}

						if (pNextTag && pPrevTag)
						{
							crTag newTag;
							crProperty& newProperty = newTag.GetProperty();
							newProperty.SetName("MoveEvent");

							crPropertyAttributeHashString moveEvent;
							moveEvent.SetName("MoveEvent");
							moveEvent.GetHashString() = m_UseLeftFootStrafeTransitionTag;
							newProperty.AddAttribute(moveEvent);

							if (pPrevTag->GetStart() > pTag->GetStart())
							{
								if (bFirstTagLeft && bLastTagLeft)
								{
									float fEndPhase = pTag->GetStart() + (pNextTag->GetStart() - pTag->GetStart()) * (1.f-m_UseLeftFootStrafeTransitionRatio);
									fEndPhase = Max(fEndPhase - (0.033f / pClip->GetDuration()), 0.f);
									newTag.SetStartEnd(0.f, fEndPhase);
									useLeftFootTransitionData.PushAndGrow(newTag);
								}
								else
								{
									float fPhaseToStart = pTag->GetStart();
									float fPhaseFromEnd = 1.f - pPrevTag->GetStart();
									if (fPhaseToStart > ((fPhaseToStart+fPhaseFromEnd)*m_UseLeftFootStrafeTransitionRatio))
									{
										float fStartPhase = pTag->GetStart() - (fPhaseToStart + fPhaseFromEnd) * m_UseLeftFootStrafeTransitionRatio;
										float fEndPhase = pTag->GetStart() + (pNextTag->GetStart() - pTag->GetStart()) * (1.f-m_UseLeftFootStrafeTransitionRatio);
										fEndPhase = Max(fEndPhase - (0.033f / pClip->GetDuration()), 0.f);
										newTag.SetStartEnd(fStartPhase, fEndPhase);
										useLeftFootTransitionData.PushAndGrow(newTag);
									}
									else
									{
										float fEndPhase = pTag->GetStart() + (pNextTag->GetStart() - pTag->GetStart()) * (1.f-m_UseLeftFootStrafeTransitionRatio);
										fEndPhase = Max(fEndPhase - (0.033f / pClip->GetDuration()), 0.f);
										newTag.SetStartEnd(0.f, fEndPhase);
										useLeftFootTransitionData.PushAndGrow(newTag);

										float fStartPhase = 1.f - (((fPhaseToStart + fPhaseFromEnd) * m_UseLeftFootStrafeTransitionRatio) - fPhaseToStart);
										newTag.SetStartEnd(fStartPhase, 1.f);
										useLeftFootTransitionData.PushAndGrow(newTag);
									}
								}
							}
							else if (pNextTag->GetStart() < pTag->GetStart())
							{
								if(bFirstTagLeft && bLastTagLeft)
								{
									float fStartPhase = pTag->GetStart() - (pTag->GetStart() - pPrevTag->GetStart()) * m_UseLeftFootStrafeTransitionRatio;
									newTag.SetStartEnd(fStartPhase, 1.f);
									useLeftFootTransitionData.PushAndGrow(newTag);
								}
								else
								{
									float fPhaseToEnd = 1.f - pTag->GetStart();
									float fPhaseFromStart = pNextTag->GetStart();
									if (fPhaseToEnd > ((fPhaseToEnd+fPhaseFromStart)*(1.f-m_UseLeftFootStrafeTransitionRatio)))
									{
										float fStartPhase = pTag->GetStart() - (pTag->GetStart() - pPrevTag->GetStart()) * m_UseLeftFootStrafeTransitionRatio;
										float fEndPhase = pTag->GetStart() + (fPhaseToEnd + fPhaseFromStart) * (1.f-m_UseLeftFootStrafeTransitionRatio);
										fEndPhase = Max(fEndPhase - (0.033f / pClip->GetDuration()), 0.f);
										newTag.SetStartEnd(fStartPhase, fEndPhase);
										useLeftFootTransitionData.PushAndGrow(newTag);
									}
									else
									{
										float fStartPhase = pTag->GetStart() - (pTag->GetStart() - pPrevTag->GetStart()) * m_UseLeftFootStrafeTransitionRatio;
										newTag.SetStartEnd(fStartPhase, 1.f);
										useLeftFootTransitionData.PushAndGrow(newTag);

										float fEndPhase = (fPhaseToEnd + fPhaseFromStart) * (1.f-m_UseLeftFootStrafeTransitionRatio) - fPhaseToEnd;
										fEndPhase = Max(fEndPhase - (0.033f / pClip->GetDuration()), 0.f);
										newTag.SetStartEnd(0.f, fEndPhase);
										useLeftFootTransitionData.PushAndGrow(newTag);
									}
								}
							}
							else
							{
								float fStartPhase = pTag->GetStart() - (pTag->GetStart() - pPrevTag->GetStart()) * m_UseLeftFootStrafeTransitionRatio;
								float fEndPhase = pTag->GetStart() + (pNextTag->GetStart() - pTag->GetStart()) * (1.f-m_UseLeftFootStrafeTransitionRatio);
								fEndPhase = Max(fEndPhase - (0.033f / pClip->GetDuration()), 0.f);
								newTag.SetStartEnd(fStartPhase, fEndPhase);
								useLeftFootTransitionData.PushAndGrow(newTag);
							}
						}
						else
						{
							Assert(0);
						}
					}
					else
					{
						if(pFirstRightTag == NULL)
						{
							pFirstRightTag = pTag;
						}

						pLastRightTag = pTag;
					}
				}
			}

			int iCount = useLeftFootTransitionData.GetCount();
			for(int i = 0; i < iCount; i++)
			{
				pTags->AddTag(useLeftFootTransitionData[i]);
			}
		}
	}

	return true;
}

void CAnimClipEditor::GenerateUseLeftFootStrafeTransitionTagsForDictionary()
{
	const char *pLocalPath = CDebugClipDictionary::FindLocalDirectoryForDictionary(m_clipEditAnimSelector.GetSelectedClipDictionary());
	if (strlen(pLocalPath)>0)
	{
		CClipFileIterator iter(MakeFunctorRet(CAnimClipEditor::GenerateUseLeftFootStrafeTransitionTags), pLocalPath, false);
		iter.Iterate();
	}
}

bool CAnimClipEditor::GenerateBlockTransitionTags(crClip* pClip, fiFindData UNUSED_PARAM(fileData), const char* UNUSED_PARAM(folderName))
{
	if (pClip)
	{
		crTags* pTags = pClip->GetTags();
		if (pTags)
		{
			ClearAllMoveEventTags(pClip, m_BlockTransitionTag);

			pTags->SortTags();

			atArray<crTag> blockTransitionData;
			crTagIterator it(*pTags);
			while (*it)
			{
				const crTag* pTag = (*it);
				++it;

				if (pTag->GetKey() == CClipEventTags::Foot)
				{
					if (!IsClose(pTag->GetStart(), 0.f, 0.001f))
					{
						crTag newTag;
						crProperty& newProperty = newTag.GetProperty();
						newProperty.SetName("MoveEvent");

						crPropertyAttributeHashString moveEvent;
						moveEvent.SetName("MoveEvent");
						moveEvent.GetHashString() = m_BlockTransitionTag;
						newProperty.AddAttribute(moveEvent);

						float fStartPhase = pTag->GetStart();
						fStartPhase -= 0.07f / pClip->GetDuration();
						fStartPhase = Max(fStartPhase, 0.f);
						newTag.SetStartEnd(fStartPhase, pTag->GetStart());

						blockTransitionData.PushAndGrow(newTag);
					}
				}
			}

			int iCount = blockTransitionData.GetCount();
			for(int i = 0; i < iCount; i++)
			{
				pTags->AddTag(blockTransitionData[i]);
			}
		}
	}

	return true;
}

void CAnimClipEditor::GenerateBlockTransitionTagsForDictionary()
{
	const char *pLocalPath = CDebugClipDictionary::FindLocalDirectoryForDictionary(m_clipEditAnimSelector.GetSelectedClipDictionary());
	if (strlen(pLocalPath)>0)
	{
		CClipFileIterator iter(MakeFunctorRet(CAnimClipEditor::GenerateBlockTransitionTags), pLocalPath, false);
		iter.Iterate();
	}
}

void CAnimClipEditor::GenerateDistanceMarkerTags()
{
	ClearAllDistanceMarkerTags();

	if (m_pClip)
	{
		crTags* pTags = m_pClip->GetTags();
		if (pTags)
		{
			pTags->SortTags();

			atArray<crTag> distanceTagData;
			float fCurrentPhase = 0.0f;

			Vector3 vTranslation = fwAnimHelpers::GetMoverTrackTranslationDiff(*m_pClip, fCurrentPhase, 1.0f);
			float fPreviousDist = vTranslation.XYMag();
			aiAssert(fPreviousDist >= 0.0f);

			bool bFinished = false;
			while (!bFinished)
			{
				Vector3 vTranslation = fwAnimHelpers::GetMoverTrackTranslationDiff(*m_pClip, fCurrentPhase, 1.0f);
				float fCurrentDist = vTranslation.XYMag();

				float fCurrentDelta = Abs(fCurrentDist - fPreviousDist);

				static dev_float DELTA_TOL = 0.05f;
				if ((ms_fDistanceInterval - fCurrentDelta) < DELTA_TOL || fCurrentDelta >= ms_fDistanceInterval)
				{
					crTag newTag;
					crProperty& newProperty = newTag.GetProperty();
					newProperty.SetName("DistanceMarker");
					crPropertyAttributeFloat distanceEvent;
					distanceEvent.SetName("Distance");
					distanceEvent.GetFloat() = fCurrentDist;
					newProperty.AddAttribute(distanceEvent);
					newTag.SetStartEnd(fCurrentPhase, fCurrentPhase);
					distanceTagData.PushAndGrow(newTag);
					fPreviousDist = fCurrentDist;
				}

				fCurrentPhase += ms_fPhaseIncrement;

				if (fCurrentPhase >= 1.0f)
				{
					bFinished = true;
				}
			}

			int iCount = distanceTagData.GetCount();
			for(int i = 0; i < iCount; i++)
			{
				pTags->AddTag(distanceTagData[i]);
			}
		}
	}
}

void CAnimClipEditor::ClearAllDistanceMarkerTags()
{
	if (m_pClip)
	{
		m_pClip->GetTags()->RemoveAllTags(CClipEventTags::DistanceMarker);
	}
}

void CAnimClipEditor::ClearAllMoveEventTags(crClip* pClip, const atHashWithStringNotFinal& moveEvent)
{
	if (pClip)
	{
		crTags* pTags = pClip->GetTags();
		if (pTags)
		{
			// Loop through until we haven't removed any events
			bool bRemovedEvent = true;
			while (bRemovedEvent)
			{
				bRemovedEvent = false;

				crTagIterator it(*pTags);
				while (*it)
				{
					crTag* pTag = const_cast<crTag*>(*it);
					++it;

					if (pTag->GetKey() == CClipEventTags::MoveEvent)
					{
						const crPropertyAttribute* pAttrib = pTag->GetProperty().GetAttribute("MoveEvent");
						if (pAttrib && pAttrib->GetType() == crPropertyAttribute::kTypeHashString)
						{
							if (moveEvent.GetHash() == static_cast<const crPropertyAttributeHashString*>(pAttrib)->GetHashString().GetHash())
							{
								pTags->RemoveTag(*pTag);
								bRemovedEvent = true;
							}
						}
					}
				}
			}
		}
	}
}

void CAnimClipEditor::PreviewNextFootTag()
{
	PreviewNextTag(CClipEventTags::Foot);
}

void CAnimClipEditor::PreviewPreviousFootTag()
{
	PreviewPreviousTag(CClipEventTags::Foot);
}

void CAnimClipEditor::PreviewNextDistanceMarkerTag()
{
	PreviewNextTag(CClipEventTags::DistanceMarker);
}

void CAnimClipEditor::PreviewPreviousDistanceMarkerTag()
{
	PreviewPreviousTag(CClipEventTags::DistanceMarker);
}

void CAnimClipEditor::PreviewNextTag(CClipEventTags::Key tagKey)
{
	if (m_pClip)
	{
		crTags* pTags = m_pClip->GetTags();
		if (pTags)
		{
			s32 iNextTag = -1;
			float fNextPhase = 1.f;

			s32 iNumTags = pTags->GetNumTags();
			for (s32 i = 0; i < iNumTags; i++)
			{
				crTag* pTag = pTags->GetTag(i);
				if (pTag && pTag->GetKey() == tagKey)
				{
					if ((pTag->GetStart() > ms_previewPhase || (ms_iSelectedTag == -1 && pTag->GetStart() >= ms_previewPhase)) && pTag->GetStart() <= fNextPhase)
					{
						iNextTag = i;
						fNextPhase = pTag->GetStart();
					}
				}
			}

			if (iNextTag != -1)
			{
				ms_iSelectedTag = iNextTag;
				ms_previewPhase = fNextPhase;
				UpdatePreviewPhase();
			}
		}
	}
}

void CAnimClipEditor::PreviewPreviousTag(CClipEventTags::Key tagKey)
{
	if (m_pClip)
	{
		crTags* pTags = m_pClip->GetTags();
		if (pTags)
		{
			s32 iPreviousTag = -1;
			float fPreviousPhase = 0.f;

			s32 iNumTags = pTags->GetNumTags();
			for (s32 i = 0; i < iNumTags; i++)
			{
				crTag* pTag = pTags->GetTag(i);
				if (pTag && pTag->GetKey() == tagKey)
				{
					if ((pTag->GetStart() < ms_previewPhase || (ms_iSelectedTag == -1 && pTag->GetStart() <= ms_previewPhase)) && pTag->GetStart() >= fPreviousPhase)
					{
						iPreviousTag = i;
						fPreviousPhase = pTag->GetStart();
					}
				}
			}

			if (iPreviousTag != -1)
			{
				ms_iSelectedTag = iPreviousTag;
				ms_previewPhase = fPreviousPhase;
				UpdatePreviewPhase();
			}
		}
	}
}

void CAnimClipEditor::SetPreviewedTagPhaseFromPreviewSlider()
{
	if (ms_iSelectedTag != -1)
	{
		if (m_pClip)
		{
			crTags* pTags = m_pClip->GetTags();
			if (pTags && Verifyf(ms_iSelectedTag >= 0 && ms_iSelectedTag < pTags->GetNumTags(), "ms_iSelectedTag [%d] is out of range 0..%d", ms_iSelectedTag, pTags->GetNumTags()))
			{
				crTag* pTag = pTags->GetTag(ms_iSelectedTag);
				if (pTag && Verifyf(pTag->GetKey() == CClipEventTags::Foot, "ms_iSelectedTag [%d] is not a Foot tag", ms_iSelectedTag))
				{
					pTag->SetStartEnd(ms_previewPhase, ms_previewPhase);
				}
			}
		}
	}
}

bool CAnimClipEditor::VerifyTagsWithinClipRange(crClip* pClip)
{
	animAssertf(pClip, "null clip passed to VerifyTagsWithinClipRange");

	bool bResult = true;

	crTags* pTags = pClip->GetTags();
	if (pTags)
	{
		crTagIterator it(*pTags);
		while(*it)
		{
			crTag* pTag = const_cast<crTag*>(*it);
			if (pTag)
			{
				float fTagStart = pTag->GetStart();
				float fTagEnd = pTag->GetEnd();

				char errorMsg[256];

				if (fTagStart < 0.0f || fTagStart > 1.0f)
				{
					formatf(errorMsg, "Clip %s - Tag %s has start phase (%.4f) that is not within the 0.0 - 1.0 range.\n", pClip->GetName(), pTag->GetName(), fTagStart);
					bkMessageBox("Invalid Tag Found", errorMsg, bkMsgOk, bkMsgError);
					bResult = false;
				}
				
				if (fTagEnd < 0.0f || fTagEnd > 1.0f)
				{
					formatf(errorMsg, "Clip %s - Tag %s has end phase (%.4f) that is not within the 0.0 - 1.0 range.\n", pClip->GetName(), pTag->GetName(), fTagEnd);
					bkMessageBox("Invalid Tag Found", errorMsg, bkMsgOk, bkMsgError);
					bResult = false;
				}
				
				if (fTagStart > fTagEnd)
				{
					formatf(errorMsg, "Clip %s - Tag %s has start phase (%.4f) that is after the end phase (%.4f).\n", pClip->GetName(), pTag->GetName(), fTagStart, fTagEnd);
					bkMessageBox("Invalid Tag Found", errorMsg, bkMsgOk, bkMsgError);
					bResult = false;
				}
			}

			++it;
		}
	}

	// Tags were all within range
	return bResult;
}

void CAnimClipEditor::SelectClip()
{
	if (m_pClip)
	{
		m_pClip->Release();
		m_pClip = NULL;
	}
	m_pClip = m_clipEditAnimSelector.GetSelectedClip();
	if (m_pClip)
	{
		m_pClip->AddRef();
	}

	LoadSelectedClipFromDisk();

	StartPreviewClip();
}

void CAnimClipEditor::StartPreviewClip()
{
	if (ms_previewClipOnFocusEntity)
	{
		CTaskAmbientClips::BLOCK_ALL_AMBIENT_IDLES = true;

		if (CAnimViewer::m_pDynamicEntity)
		{
			fwAnimDirector* pDirector = CAnimViewer::m_pDynamicEntity->GetAnimDirector();

			if (pDirector)
			{
				ms_previewScene = fwAnimDirectorComponentSyncedScene::StartSynchronizedScene();
				fwAnimDirectorComponentSyncedScene::SetSyncedScenePhase(ms_previewScene, ms_previewPhase);
				fwAnimDirectorComponentSyncedScene::SetSyncedScenePaused(ms_previewScene, true);
				Matrix34 mat = MAT34V_TO_MATRIX34(CAnimViewer::m_pDynamicEntity->GetMatrix());
				fwAnimDirectorComponentSyncedScene::SetSyncedSceneOrigin(ms_previewScene, mat);

				// If we're already playing the synced scene, stop it
				fwAnimDirectorComponentSyncedScene* componentSyncedScene = pDirector->GetComponent<fwAnimDirectorComponentSyncedScene>();
				if (componentSyncedScene && componentSyncedScene->IsPlayingSyncedScene())
				{
					componentSyncedScene->StopSyncedScenePlayback();
				}

				// play the clip
				if(!componentSyncedScene)
				{
					componentSyncedScene = pDirector->CreateComponentSyncedScene();
				}
				int clipDictionaryIndex = g_ClipDictionaryStore.FindSlot(m_clipEditAnimSelector.GetSelectedClipDictionary()).Get();
				componentSyncedScene->StartSyncedScenePlayback(
					clipDictionaryIndex, 
					m_clipEditAnimSelector.GetSelectedClip(),
					NULL,
					ms_previewScene,
					CClipNetworkMoveInfo::ms_NetworkSyncedScene);

				UpdatePreviewPhase();

				if (CAnimViewer::m_pDynamicEntity->GetIsPhysical())
				{
					CPhysical* pEntity = static_cast<CPhysical*>(CAnimViewer::m_pDynamicEntity.Get());
					pEntity->DisableCollision();
				}
				if (CAnimViewer::m_pDynamicEntity->GetIsTypePed())
				{
					CPed* pPed = static_cast<CPed*>(CAnimViewer::m_pDynamicEntity.Get());
					pPed->SetUseExtractedZ(true);
				}
			}
		}
	}
	else
	{
		CTaskAmbientClips::BLOCK_ALL_AMBIENT_IDLES = false;

		if (CAnimViewer::m_pDynamicEntity)
		{
			fwAnimDirector* pDirector = CAnimViewer::m_pDynamicEntity->GetAnimDirector();

			if (pDirector)
			{
				fwAnimDirectorComponentSyncedScene* componentSyncedScene = pDirector->GetComponent<fwAnimDirectorComponentSyncedScene>();
				if (componentSyncedScene && componentSyncedScene->IsPlayingSyncedScene())
				{
					// stop playback
					componentSyncedScene->StopSyncedScenePlayback();
				}

				// reactivate collision and gravity
				if (CAnimViewer::m_pDynamicEntity->GetIsPhysical())
				{
					CPhysical* pEntity = static_cast<CPhysical*>(CAnimViewer::m_pDynamicEntity.Get());
					pEntity->EnableCollision();
				}
				if (CAnimViewer::m_pDynamicEntity->GetIsTypePed())
				{
					CPed* pPed = static_cast<CPed*>(CAnimViewer::m_pDynamicEntity.Get());
					pPed->SetUseExtractedZ(false);
				}
			}
		}
	}
}

void CAnimClipEditor::UpdatePreviewPhase()
{
	if (fwAnimDirectorComponentSyncedScene::IsValidSceneId(ms_previewScene))
	{
		fwAnimDirectorComponentSyncedScene::SetSyncedScenePhase(ms_previewScene, ms_previewPhase);
		ms_pPreviewTimeSlider->SetRange(0.0f,fwAnimDirectorComponentSyncedScene::GetSyncedSceneDuration(ms_previewScene));
		ms_previewTime = fwAnimDirectorComponentSyncedScene::GetSyncedSceneTime(ms_previewScene);
	}
}

void CAnimClipEditor::UpdatePreviewTime()
{
	if (fwAnimDirectorComponentSyncedScene::IsValidSceneId(ms_previewScene))
	{
		ms_previewTime = Min(ms_previewTime,fwAnimDirectorComponentSyncedScene::GetSyncedSceneDuration(ms_previewScene));
		fwAnimDirectorComponentSyncedScene::SetSyncedSceneTime(ms_previewScene, ms_previewTime);
		ms_previewPhase = fwAnimDirectorComponentSyncedScene::GetSyncedScenePhase(ms_previewScene);
	}
}

void CAnimClipEditor::LoadSelectedClip()
{
	USE_MEMBUCKET(MEMBUCKET_ANIMATION);

	for (int i=0; i<m_specialAttributes.GetCount(); i++)
	{
		delete m_specialAttributes[i];
		m_specialAttributes[i]= NULL;
	}

	m_specialAttributes.Reset();

	ms_AuthorName.SetFromString("Not Set");

	if (m_pClip)
	{
		crProperties *pProperties = m_pClip->GetProperties();
		if(pProperties)
		{
			crProperty *pProperty = pProperties->FindProperty("SourceUserName");
			if(pProperty)
			{
				crPropertyAttribute *pPropertyAttribute = pProperty->GetAttribute("SourceUserName");
				if(pPropertyAttribute && pPropertyAttribute->GetType() == crPropertyAttribute::kTypeInt)
				{
					crPropertyAttributeInt *pPropertyAttributeInt = static_cast< crPropertyAttributeInt * >(pPropertyAttribute);

					s32 iHash = pPropertyAttributeInt->GetInt();
					u32 uHash = *reinterpret_cast< u32 * >(&iHash);

					ms_AuthorName.SetHash(uHash);

					if(!ms_AuthorName.TryGetCStr())
					{
						ms_AuthorName.SetFromString(atVarString("Unknown %u", uHash));
					}
				}
			}
		}

		LoadTags(m_pClip);

		LoadProperties(m_pClip);
	}
}

void CAnimClipEditor::ClearAllTagsButton()
{	
	atString message("Clear all event tags?");

	int result = bkMessageBox("Clear all event tags", message.c_str(), bkMsgYesNo, bkMsgQuestion);

	if (result)
	{
		ClearAllTags();
		UpdateTagWidgets();
	}
}

void CAnimClipEditor::ClearAllFootTagsButton()
{	
	atString message("Clear all foot event tags?");

	int result = bkMessageBox("Clear all foot event tags", message.c_str(), bkMsgYesNo, bkMsgQuestion);

	if (result)
	{
		ClearAllFootTags();
		UpdateTagWidgets();
	}
}

void CAnimClipEditor::ClearAllMoveEventTagsButton(const atHashWithStringNotFinal& moveEvent)
{
	atString message("Clear all ");
	message += moveEvent.GetCStr();
	message += " event tags?";

	int result = bkMessageBox(message.c_str(), message.c_str(), bkMsgYesNo, bkMsgQuestion);

	if (result)
	{
		ClearAllMoveEventTags(m_pClip, moveEvent);
		UpdateTagWidgets();
	}
}

void CAnimClipEditor::ClearAllUseLeftFootTransitionTagsButton()
{
	ClearAllMoveEventTagsButton(m_UseLeftFootTransitionTag);
}

void CAnimClipEditor::ClearAllUseLeftFootStrafeTransitionTagsButton()
{
	ClearAllMoveEventTagsButton(m_UseLeftFootStrafeTransitionTag);
}

void CAnimClipEditor::ClearAllBlockTransitionTagsButton()
{
	ClearAllMoveEventTagsButton(m_BlockTransitionTag);
}

void CAnimClipEditor::ClearAllDistanceMarkerTagsButton()
{
	atString message("Clear all distance marker event tags?");

	int result = bkMessageBox("Clear all distance marker event tags", message.c_str(), bkMsgYesNo, bkMsgQuestion);

	if (result)
	{
		ClearAllDistanceMarkerTags();
		UpdateTagWidgets();
	}
}

void CAnimClipEditor::GenerateUseLeftFootTransitionTagsButton()
{
	GenerateUseLeftFootTransitionTags();
	UpdateTagWidgets();
}

void CAnimClipEditor::GenerateToStrafeUseLeftFootStrafeTransitionTagsButton()
{
	static dev_float RATIO_FROM_PREVIOUS_TAG  = 0.9f;
	m_UseLeftFootStrafeTransitionRatio = RATIO_FROM_PREVIOUS_TAG;

	fiFindData fileData;
	fileData.m_Name[0] = '\0';
	fileData.m_Size = 0;
	fileData.m_LastWriteTime = 0;
	fileData.m_Attributes = 0;
	GenerateUseLeftFootStrafeTransitionTags(m_pClip, fileData, NULL);
	UpdateTagWidgets();
}

void CAnimClipEditor::GenerateToStrafeUseLeftFootStrafeTransitionTagsForDictionaryButton()
{
	static dev_float RATIO_FROM_PREVIOUS_TAG  = 0.9f;
	m_UseLeftFootStrafeTransitionRatio = RATIO_FROM_PREVIOUS_TAG;

	GenerateUseLeftFootStrafeTransitionTagsForDictionary();
	UpdateTagWidgets();
}

void CAnimClipEditor::GenerateFromStrafeUseLeftFootStrafeTransitionTagsButton()
{
	static dev_float RATIO_FROM_PREVIOUS_TAG  = 0.65f;
	m_UseLeftFootStrafeTransitionRatio = RATIO_FROM_PREVIOUS_TAG;

	fiFindData fileData;
	fileData.m_Name[0] = '\0';
	fileData.m_Size = 0;
	fileData.m_LastWriteTime = 0;
	fileData.m_Attributes = 0;
	GenerateUseLeftFootStrafeTransitionTags(m_pClip, fileData, NULL);
	UpdateTagWidgets();
}

void CAnimClipEditor::GenerateFromStrafeUseLeftFootStrafeTransitionTagsForDictionaryButton()
{
	static dev_float RATIO_FROM_PREVIOUS_TAG  = 0.65f;
	m_UseLeftFootStrafeTransitionRatio = RATIO_FROM_PREVIOUS_TAG;

	GenerateUseLeftFootStrafeTransitionTagsForDictionary();
	UpdateTagWidgets();
}

void CAnimClipEditor::GenerateBlockTransitionTagsButton()
{
	fiFindData fileData;
	fileData.m_Name[0] = '\0';
	fileData.m_Size = 0;
	fileData.m_LastWriteTime = 0;
	fileData.m_Attributes = 0;
	GenerateBlockTransitionTags(m_pClip, fileData, NULL);
	UpdateTagWidgets();
}

void CAnimClipEditor::GenerateBlockTransitionTagsForDictionaryButton()
{
	GenerateBlockTransitionTagsForDictionary();
	UpdateTagWidgets();
}

void CAnimClipEditor::GenerateDistanceMarkerTagsButton()
{
	GenerateDistanceMarkerTags();
	UpdateTagWidgets();
}

void CAnimClipEditor::LoadProperties(crClip* UNUSED_PARAM(pClip))
{
	UpdatePropertiesCollectionWidgets();
}

void CAnimClipEditor::LoadTags(crClip* UNUSED_PARAM(pClip))
{
	UpdateTagWidgets();
}

void CAnimClipEditor::LoadSelectedClipFromDisk()
{
	if (!m_pClip) //if no clip is selected - bug out
		return;

	m_bAllowSaving = false;

	const char *pLocalPath = CDebugClipDictionary::FindLocalDirectoryForDictionary(m_clipEditAnimSelector.GetSelectedClipDictionary());
	if(strlen(pLocalPath)>0)
	{
		char clipFileNameAndPath[512];
		formatf(clipFileNameAndPath, "%s\\%s.clip", pLocalPath, m_clipEditAnimSelector.GetSelectedClipName());
		Displayf("Successfully found %s\n", clipFileNameAndPath);

		crClip * pLoadedClip = crClip::AllocateAndLoad(clipFileNameAndPath, NULL);
		if (pLoadedClip)
		{
			Displayf("Successfully loaded %s\n", clipFileNameAndPath);

			if (m_pClip)
			{
				m_pClip->GetProperties()->RemoveAllProperties();
				*m_pClip->GetProperties() = *pLoadedClip->GetProperties();
				m_pClip->GetTags()->RemoveAllTags();
				*m_pClip->GetTags() = *pLoadedClip->GetTags();
				VerifyTagsWithinClipRange(m_pClip);

				m_bAllowSaving = true;
			}

			delete pLoadedClip; pLoadedClip = NULL;
		}
	}

	if (!m_bAllowSaving)
	{
		animErrorf("Unable to load the clip '%s' from disk. Clip saving will be disabled.", m_pClip->GetName());
	}

	if (m_pClip)
	{
		Vector3 vOffset = fwAnimHelpers::GetMoverTrackTranslationDiff(*m_pClip, 1.f, 0.f);
		Quaternion qStartRotation = fwAnimHelpers::GetMoverTrackRotation(*m_pClip, 0.f);
		const float fInitialHeadingOffset = qStartRotation.TwistAngle(ZAXIS);
		// Account for any initial z rotation
		vOffset.RotateZ(-fInitialHeadingOffset);
		// Get the total z axis rotation over the course of the get_in anim excluding the initial rotation
		Quaternion qRotTotal = fwAnimHelpers::GetMoverTrackRotationDiff(*m_pClip, 1.f, 0.f);	
		const float fTotalHeadingRotation = qRotTotal.TwistAngle(ZAXIS);
		vOffset.RotateZ(fTotalHeadingRotation);
		aiDisplayf("Initial Z Axis Rotation = %.4f", fInitialHeadingOffset);
		aiDisplayf("Translation x=\"%.4f\" y=\"%.4f\" z=\"%.4f\" />", vOffset.x, vOffset.y, vOffset.z);
		aiDisplayf("HeadingChange value=\"%.4f\" />", fTotalHeadingRotation);
	}

	LoadSelectedClip();
}

void CAnimClipEditor::SaveSelectedClipToDisk()
{
	if (!m_pClip) //if no clip is selected - bug out
		return;

	if (!m_bAllowSaving)
	{
		bkMessageBox("Save unavailable!", "Unable to save the clip. Make sure it is in your export folder and reload it.", bkMsgOk, bkMsgError);
		return;
	}

	// Check that tags on the clip are all within range.
	if (!VerifyTagsWithinClipRange(m_pClip))
	{
		bkMessageBox("Aborting Save", "Unable to save the clip as it contains tags that have invalid start or end times.", bkMsgOk, bkMsgError);
		return;
	}

	m_clipEditAnimSelector.SaveSelectedClipToDisk();
}

void CAnimClipEditor::RequestLoadSelectedClipFromDisk()
{
	m_bRequestLoadSelectedClipFromDisk = true;
}

void CAnimClipEditor::RequestSaveSelectedClipToDisk()
{
	m_bRequestSaveSelectedClipToDisk = true;
}

//////////////////////////////////////////////////////////////////////////
// Shortcuts
//////////////////////////////////////////////////////////////////////////
void CAnimClipEditor::SetApprovalProperty()
{
	const char* approvalStrings[] =
	{
		"None",
		"Placeholder",
		"Complete",
		"Approved",
	};

	if (m_pClip)
	{
		crProperties* pProps = m_pClip->GetProperties();
		if (pProps)
		{
			atHashString statusHash(approvalStrings[ms_iApprovalFlag]);

			s32 iTodaysDateDay = 0;
			s32	iTodaysDateMonth = 0;
			s32 iTodaysDateYear = 0;

			time_t rawtime;
			struct tm * pTimeInfo;
			time ( &rawtime );
			pTimeInfo = localtime ( &rawtime );

			if (pTimeInfo)
			{
				iTodaysDateDay = pTimeInfo->tm_mday + 1;
				iTodaysDateMonth = pTimeInfo->tm_mon + 1;
				iTodaysDateYear = pTimeInfo->tm_year + 1900;
			}
			Displayf("TODAYS DATE IS: %02d-%02d-%04d", iTodaysDateDay, iTodaysDateMonth, iTodaysDateYear);

			crProperty* pProp = NULL;
			pProp = pProps->FindProperty(crProperties::CalcKey("Approval_DO_NOT_RESOURCE"));
			if (!pProp)
			{
				crProperty newProp;
				newProp.SetName("Approval_DO_NOT_RESOURCE");

				AddAttribute(newProp, "Status", crPropertyAttribute::kTypeHashString);
				AddAttribute(newProp, "Day", crPropertyAttribute::kTypeInt);
				AddAttribute(newProp, "Month", crPropertyAttribute::kTypeInt);
				AddAttribute(newProp, "Year", crPropertyAttribute::kTypeInt);

				// set the values
				static_cast<crPropertyAttributeHashString*>(newProp.GetAttribute("Status"))->GetHashString()= statusHash;
				static_cast<crPropertyAttributeInt*>(newProp.GetAttribute("Day"))->GetInt()=iTodaysDateDay;
				static_cast<crPropertyAttributeInt*>(newProp.GetAttribute("Month"))->GetInt()=iTodaysDateMonth;
				static_cast<crPropertyAttributeInt*>(newProp.GetAttribute("Year"))->GetInt()=iTodaysDateYear;

				pProps->AddProperty(newProp);
			}
			else
			{
				// set the values
				static_cast<crPropertyAttributeHashString*>(pProp->GetAttribute("Status"))->GetHashString()= statusHash;
				static_cast<crPropertyAttributeInt*>(pProp->GetAttribute("Day"))->GetInt()=iTodaysDateDay;
				static_cast<crPropertyAttributeInt*>(pProp->GetAttribute("Month"))->GetInt()=iTodaysDateMonth;
				static_cast<crPropertyAttributeInt*>(pProp->GetAttribute("Year"))->GetInt()=iTodaysDateYear;

			}
		}
		UpdatePropertiesCollectionWidgets();
	}
}

//////////////////////////////////////////////////////////////////////////

void CAnimClipEditor::ActionButton(ClipTagAction *clipTagAction)
{
	if (m_pClip)
	{
		crTags* pTags = m_pClip->GetTags();
		if (pTags)
		{
			rage::crTag tag;
			tag.SetStartEnd(ms_previewPhase, ms_previewPhase);

			ClipTagProperty &clipTagProperty = clipTagAction->GetProperty();
			crProperty &property = tag.GetProperty();

			property.SetName(clipTagProperty.GetName());

			for(int attributeIndex = 0; attributeIndex < clipTagAction->GetProperty().GetAttributeCount(); attributeIndex ++)
			{
				ClipTagAttribute *clipTagAttribute = clipTagAction->GetProperty().GetAttribute(attributeIndex);

				switch(clipTagAttribute->GetType())
				{
				case kTypeFloat:
					{
						AddAttribute(property, clipTagAttribute->GetName(), crPropertyAttribute::kTypeFloat);

						ClipTagFloatAttribute *clipTagFloatAttribute = static_cast< ClipTagFloatAttribute * >(clipTagAttribute);
						crPropertyAttributeFloat *propertyAttributeFloat = static_cast< crPropertyAttributeFloat * >(property.GetAttribute(clipTagAttribute->GetName()));

						propertyAttributeFloat->GetFloat() = clipTagFloatAttribute->GetValue();
					} break;
				case kTypeInt:
					{
						AddAttribute(property, clipTagAttribute->GetName(), crPropertyAttribute::kTypeInt);

						ClipTagIntAttribute *clipTagIntAttribute = static_cast< ClipTagIntAttribute * >(clipTagAttribute);
						crPropertyAttributeInt *propertyAttributeInt = static_cast< crPropertyAttributeInt * >(property.GetAttribute(clipTagAttribute->GetName()));

						propertyAttributeInt->GetInt() = clipTagIntAttribute->GetValue();
					} break;
				case kTypeBool:
					{
						AddAttribute(property, clipTagAttribute->GetName(), crPropertyAttribute::kTypeBool);

						ClipTagBoolAttribute *clipTagBoolAttribute = static_cast< ClipTagBoolAttribute * >(clipTagAttribute);
						crPropertyAttributeBool *propertyAttributeBool = static_cast< crPropertyAttributeBool * >(property.GetAttribute(clipTagAttribute->GetName()));

						propertyAttributeBool->GetBool() = clipTagBoolAttribute->GetValue();
					} break;
				case kTypeString:
					{
						AddAttribute(property, clipTagAttribute->GetName(), crPropertyAttribute::kTypeString);

						ClipTagStringAttribute *clipTagStringAttribute = static_cast< ClipTagStringAttribute * >(clipTagAttribute);
						crPropertyAttributeString *propertyAttributeString = static_cast< crPropertyAttributeString * >(property.GetAttribute(clipTagAttribute->GetName()));

						propertyAttributeString->GetString() = clipTagStringAttribute->GetValue();
					} break;
				case kTypeVector3:
					{
						AddAttribute(property, clipTagAttribute->GetName(), crPropertyAttribute::kTypeVector3);

						ClipTagVector3Attribute *clipTagVector3Attribute = static_cast< ClipTagVector3Attribute * >(clipTagAttribute);
						crPropertyAttributeVector3 *propertyAttributeVector3 = static_cast< crPropertyAttributeVector3 * >(property.GetAttribute(clipTagAttribute->GetName()));

						propertyAttributeVector3->GetVector3() = VECTOR3_TO_VEC3V(clipTagVector3Attribute->GetValue());
					} break;
				case kTypeVector4:
					{
						AddAttribute(property, clipTagAttribute->GetName(), crPropertyAttribute::kTypeVector4);

						ClipTagVector4Attribute *clipTagVector4Attribute = static_cast< ClipTagVector4Attribute * >(clipTagAttribute);
						crPropertyAttributeVector4 *propertyAttributeVector4 = static_cast< crPropertyAttributeVector4 * >(property.GetAttribute(clipTagAttribute->GetName()));

						propertyAttributeVector4->GetVector4() = VECTOR4_TO_VEC4V(clipTagVector4Attribute->GetValue());
					} break;
				case kTypeQuaternion:
					{
						AddAttribute(property, clipTagAttribute->GetName(), crPropertyAttribute::kTypeQuaternion);

						ClipTagQuaternionAttribute *clipTagQuaternionAttribute = static_cast< ClipTagQuaternionAttribute * >(clipTagAttribute);
						crPropertyAttributeQuaternion *propertyAttributeQuaternion = static_cast< crPropertyAttributeQuaternion * >(property.GetAttribute(clipTagAttribute->GetName()));

						propertyAttributeQuaternion->GetQuaternion() = QUATERNION_TO_QUATV(clipTagQuaternionAttribute->GetValue());
					} break;
				case kTypeMatrix34:
					{
						AddAttribute(property, clipTagAttribute->GetName(), crPropertyAttribute::kTypeMatrix34);

						ClipTagMatrix34Attribute *clipTagMatrix34Attribute = static_cast< ClipTagMatrix34Attribute * >(clipTagAttribute);
						crPropertyAttributeMatrix34 *propertyAttributeMatrix34 = static_cast< crPropertyAttributeMatrix34 * >(property.GetAttribute(clipTagAttribute->GetName()));

						propertyAttributeMatrix34->GetMatrix34() = MATRIX34_TO_MAT34V(clipTagMatrix34Attribute->GetValue());
					} break;
				case kTypeData:
					{
						AddAttribute(property, clipTagAttribute->GetName(), crPropertyAttribute::kTypeData);

						ClipTagDataAttribute *clipTagDataAttribute = static_cast< ClipTagDataAttribute * >(clipTagAttribute);
						crPropertyAttributeData *propertyAttributeData = static_cast< crPropertyAttributeData * >(property.GetAttribute(clipTagAttribute->GetName()));

						propertyAttributeData->GetData() = clipTagDataAttribute->GetValue();
					} break;
				case kTypeHashString:
					{
						AddAttribute(property, clipTagAttribute->GetName(), crPropertyAttribute::kTypeHashString);

						ClipTagHashStringAttribute *clipTagHashStringAttribute = static_cast< ClipTagHashStringAttribute * >(clipTagAttribute);
						crPropertyAttributeHashString *propertyAttributeHashString = static_cast< crPropertyAttributeHashString * >(property.GetAttribute(clipTagAttribute->GetName()));

						propertyAttributeHashString->GetHashString() = clipTagHashStringAttribute->GetValue();
					} break;
				}
			}

			pTags->AddTag(tag);
		}

		UpdateTagWidgets();
	}
}

//////////////////////////////////////////////////////////////////////////
//	Attribute editing
//////////////////////////////////////////////////////////////////////////

void CAnimClipEditor::sAttribute::Save()
{
	if (animVerifyf(m_pAttrib!=NULL, "No attribute to modify!"))
	{
		SaveToAttribute(*m_pAttrib);
	}
}

//////////////////////////////////////////////////////////////////////////
// String and hash string

void CAnimClipEditor::sAttributeString::SaveToAttribute(crPropertyAttribute& attrib)
{
	if (m_bIsHash)
	{
		crPropertyAttributeHashString& derivedAttrib = static_cast<crPropertyAttributeHashString&>(attrib);
		derivedAttrib.GetHashString().SetFromString(m_buffer);
	}
	else
	{
		crPropertyAttributeString& derivedAttrib = static_cast<crPropertyAttributeString&>(attrib);
		derivedAttrib.GetString() = m_buffer;
	}
}

//////////////////////////////////////////////////////////////////////////
// euler angles

void CAnimClipEditor::sAttributeEuler::SaveToAttribute(crPropertyAttribute& attrib)
{
	Quaternion q(Quaternion::sm_I);

	q.FromEulers(m_euler);

	if (m_bIsSituation)
	{
		crPropertyAttributeSituation& derivedAttrib = static_cast<crPropertyAttributeSituation&>(attrib);
		derivedAttrib.GetSituation().SetRotation(QUATERNION_TO_QUATV(q));
	}
	else
	{
		crPropertyAttributeQuaternion& derivedAttrib = static_cast<crPropertyAttributeQuaternion&>(attrib);
		derivedAttrib.GetQuaternion() = QUATERNION_TO_QUATV(q);
	}
}

#endif //__BANK
