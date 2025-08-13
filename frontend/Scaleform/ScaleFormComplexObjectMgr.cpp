/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : ScaleformComplexObjectMgr.cpp
// PURPOSE : manages the processing of use of Complex Object in Scaleform
//			 and linking directly to movies
//			 keeps complex object code to simple, one line calls
//           since we are going to be moving over to more complex object use then
//           we should maintain this in one place, link up each stored value and
//           track and log output to find movies that have been deleted but still
//           have movieclip references attached.
//			 this allows ui systems to use complex object without using gfxvalue
//			 directly.  This may be handy with expansion for future script support 
//			 of complex objects
//			 complex objects can be access and used on both UT and RT
//			 invokes to the complex objects can be sent through the ScaleformMgr
//			 on UT and RT
// AUTHOR  : Derek Payne
// STARTED : 28/02/2014
//
/////////////////////////////////////////////////////////////////////////////////

// Rage headers
#include "atl/hashstring.h"
#include "Scaleform/scaleform.h"
#include "scr_scaleform/scaleform.h"
#include "frontend/ui_channel.h"
#include "string/stringutil.h"
#include "system/param.h"
#include "vector/color32.h"

// Game headers
#include "Frontend/Scaleform/ScaleFormComplexObjectMgr.h"
#include "Frontend/Scaleform/ScaleFormMgr.h"

SCALEFORM_OPTIMISATIONS();
//OPTIMISATIONS_OFF()

#define __FULL_SANITY_CHECKS (!__FINAL)  // checking whether items are on the stage etc may not be needed in release, we can turn off specific checks here

atArray<CComplexObjectArrayItem> CScaleformComplexObjectMgr::sm_ObjectArray;
s32 CScaleformComplexObjectMgr::ms_iUniqueId = 0;

#if __BANK
bool CScaleformComplexObjectMgr::ms_bUseDetailedLogging = false;
PARAM(complexObjectDetailedLogging, "Enable detailed logging for Complex Object operations");
#endif // __BANK

#define MAX_COMPLEX_OBJECT_DEPTH (100000)  // set a hard max to keep numbers used reasonable.  There is a bug in AS which causes issues with very high depths so lets try to avoid it


bool CComplexObject::IsActiveAndAttached() const
{
	if (IsActive())
	{
		CComplexObjectArrayItem const * const c_obj = CScaleformComplexObjectMgr::FindObjectFromArrayIndex(objectId);

		if( c_obj )
		{
			return ( (c_obj->IsActive()) && (!c_obj->IsPendingCreate()) && (!c_obj->IsPendingAttach()) && (!c_obj->IsPendingGet()) && (!c_obj->IsPendingGetRoot()) );
		}
	}

	return false;
}

bool CComplexObject::HasInvokesPending() const
{
	if (IsActive())
	{
		CComplexObjectArrayItem const * const c_obj = CScaleformComplexObjectMgr::FindObjectFromArrayIndex(objectId);

		if( c_obj )
		{
			return (c_obj->IsPendingInvoke());
		}
	}

	return false;
}

void CComplexObject::Release()
{
	CComplexObjectArrayItem *pObj = CScaleformComplexObjectMgr::FindObjectFromArrayIndex(objectId);

	if (pObj)
	{
#if RSG_BANK
        if( CScaleformComplexObjectMgr::DetailedLogging() )
        {
            int const c_movieId = pObj->GetMovieId();
            sfDisplayf("CComplexObject::Release called on '" HASHFMT "' on movie %s (%d)", HASHOUT(pObj->GetObjectNameHash()), CScaleformMgr::GetMovieFilename( c_movieId ), c_movieId );
            {
                sysStack::PrintStackTrace();
            }
        }
#endif // RSG_BANK

        CComplexObjectArrayItem::ReleaseObject(pObj, false);
        Clear();
	}
}


void CComplexObject::SetPosition(Vector2 const& vPos)
{
	CComplexObjectArrayItem *pObj = CScaleformComplexObjectMgr::FindObjectFromArrayIndex(objectId);

	if (pObj)
	{
		if ( (!pObj->IsInitialised()) || (pObj->GetPosition().x != vPos.x || pObj->GetPosition().y != vPos.y) )
		{
			pObj->SetPosition(vPos);
		}
	}
}

void CComplexObject::SetPositionInPixels(Vector2 const& vPixelPos)
{
	CComplexObjectArrayItem *pObj = CScaleformComplexObjectMgr::FindObjectFromArrayIndex(objectId);

	if (pObj)
	{
		if ( (!pObj->IsInitialised()) || (pObj->GetPositionInPixels().x != vPixelPos.x || pObj->GetPositionInPixels().y != vPixelPos.y) )
		{
			pObj->SetPositionInPixels(vPixelPos);
		}
	}
}

void CComplexObject::SetPosition( Vec2f_In vPos )
{
    SetPosition( Vector2( vPos.GetX(), vPos.GetY() ) );
}

void CComplexObject::SetPositionInPixels( Vec2f_In vPixelPos )
{
    SetPositionInPixels( Vector2( vPixelPos.GetX(), vPixelPos.GetY() ) );
}

void CComplexObject::SetXPositionInPixels(float fPixelPos)
{
	CComplexObjectArrayItem *pObj = CScaleformComplexObjectMgr::FindObjectFromArrayIndex(objectId);

	if (pObj)
	{
		if ( !pObj->IsInitialised() || !AreNearlyEqual( pObj->GetXPositionInPixels(),  fPixelPos ) )
		{
			pObj->SetXPositionInPixels(fPixelPos);
		}
	}
}

void CComplexObject::SetYPositionInPixels( float fPixelPos )
{
	CComplexObjectArrayItem *pObj = CScaleformComplexObjectMgr::FindObjectFromArrayIndex(objectId);

	if (pObj)
	{
		if ( (!pObj->IsInitialised()) || (pObj->GetYPositionInPixels() != fPixelPos) )
		{
			pObj->SetYPositionInPixels(fPixelPos);
		}
	}
}

void CComplexObject::SetSize( Vec2f_In vScale )
{
    SetWidth( vScale.GetX() );
    SetHeight( vScale.GetY() );
}

void CComplexObject::SetSize( Vector2 const& vNewScale )
{
    SetWidth( vNewScale.x );
    SetHeight( vNewScale.y );
}
void CComplexObject::SetScale( Vec2f_In vScale )
{
    SetScale( Vector2( vScale.GetX(), vScale.GetY() ) );
}

void CComplexObject::SetScale(Vector2 const& vNewScale)
{
	CComplexObjectArrayItem *pObj = CScaleformComplexObjectMgr::FindObjectFromArrayIndex(objectId);

	if (pObj)
	{
		if ( (!pObj->IsInitialised()) || (pObj->GetScale().x != vNewScale.x || pObj->GetScale().y != vNewScale.y) )
		{
			pObj->SetScale(vNewScale);
		}
	}
}

void CComplexObject::SetXScale(float fScale)
{
	CComplexObjectArrayItem *pObj = CScaleformComplexObjectMgr::FindObjectFromArrayIndex(objectId);

	if (pObj)
	{
		if ( (!pObj->IsInitialised()) || (pObj->GetScale().x != fScale) )
		{
			pObj->SetXScale(fScale);
		}
	}
}

void CComplexObject::SetRotation( float fRotation )
{
	CComplexObjectArrayItem *pObj = CScaleformComplexObjectMgr::FindObjectFromArrayIndex(objectId);

	if (pObj)
	{
		if ( (!pObj->IsInitialised()) || (pObj->GetRotation() != fRotation) )
		{
			pObj->SetRotation(fRotation);
		}
	}
}

void CComplexObject::SetMember( const char *pInstanceName, bool const c_value)
{
	CComplexObjectArrayItem *pObj = CScaleformComplexObjectMgr::FindObjectFromArrayIndex(objectId);

	if (pObj)
	{
		pObj->SetMember(pInstanceName, c_value);
	}
}

void CComplexObject::AdjustXPositionInPixels(float fPixels)
{
	CComplexObjectArrayItem *pObj = CScaleformComplexObjectMgr::FindObjectFromArrayIndex(objectId);

	if (pObj)
	{
		if (fPixels != 0.0f)
		{
			pObj->AdjustXPositionInPixels(fPixels);
		}
	}
}

void CComplexObject::SetWidth(float fWidth)
{
	CComplexObjectArrayItem *pObj = CScaleformComplexObjectMgr::FindObjectFromArrayIndex(objectId);

	if (pObj)
	{
		if ( (!pObj->IsInitialised()) || (pObj->GetWidth() != fWidth) )
		{
			pObj->SetWidth(fWidth);
		}
	}
}

void CComplexObject::SetHeight( float fHeight )
{
	CComplexObjectArrayItem *pObj = CScaleformComplexObjectMgr::FindObjectFromArrayIndex(objectId);

	if (pObj)
	{
		if ( (!pObj->IsInitialised()) || (pObj->GetHeight() != fHeight) )
		{
			pObj->SetHeight(fHeight);
		}
	}
}

void CComplexObject::SetVisible(bool bValue)
{
	CComplexObjectArrayItem *pObj = CScaleformComplexObjectMgr::FindObjectFromArrayIndex(objectId);

	if (pObj)
	{
		if ( (!pObj->IsInitialised()) || (pObj->IsVisible() != bValue) )
		{
			pObj->SetVisible(bValue);
		}
	}
}

void CComplexObject::GotoFrame(s32 iFrame, bool const andPlay )
{
	CComplexObjectArrayItem *pObj = CScaleformComplexObjectMgr::FindObjectFromArrayIndex(objectId);

	if (pObj)
	{
		if ( (!pObj->IsInitialised()) || (pObj->GetCurrentFrame() != iFrame) )
		{
			pObj->GotoFrame(iFrame, andPlay );
		}
	}
}

void CComplexObject::SetTextField(const char *pText)
{
	CComplexObjectArrayItem *pObj = CScaleformComplexObjectMgr::FindObjectFromArrayIndex(objectId);

	if (pObj)
	{
		pObj->SetTextField(pText);
	}
}

void CComplexObject::SetTextFieldHTML( const char *pHtmlText )
{
	CComplexObjectArrayItem *pObj = CScaleformComplexObjectMgr::FindObjectFromArrayIndex(objectId);

	if (pObj)
	{
		pObj->SetTextFieldHTML(pHtmlText);
	}
}

void CComplexObject::SetColour(Color32 const &colour)
{
	CComplexObjectArrayItem *pObj = CScaleformComplexObjectMgr::FindObjectFromArrayIndex(objectId);

	if (pObj)
	{
		if ( (!pObj->IsInitialised()) || (pObj->GetColour() != colour) )
		{
			pObj->SetColour(colour);
		}
	}
}

void CComplexObject::SetAlpha(u8 const c_alpha)
{
	CComplexObjectArrayItem *pObj = CScaleformComplexObjectMgr::FindObjectFromArrayIndex(objectId);

	if (pObj)
	{
		pObj->SetAlpha(c_alpha);
	}
}

void CComplexObject::SetDepth( s32 const c_depth )
{
	CComplexObjectArrayItem *pObj = CScaleformComplexObjectMgr::FindObjectFromArrayIndex(objectId);

	if (pObj)
	{
		if ( (!pObj->IsInitialised()) || (pObj->GetDepth() != c_depth) )
		{
			pObj->SetDepth(c_depth);
		}
	}
}


Vector2 CComplexObject::GetPosition()
{
	Vector2 vRetPos(0,0);

	CComplexObjectArrayItem *pObj = CScaleformComplexObjectMgr::FindObjectFromArrayIndex(objectId);

	if (pObj)
	{
		if ((pObj->GetActionRequired() & CO_ACTION_UPDATE_POSITION_X) == 0 && (pObj->GetActionRequired() & CO_ACTION_UPDATE_POSITION_Y) == 0)
		{
			CScaleformComplexObjectMgr::CheckAndSetupIfUninitialised(objectId);
		}

		vRetPos = pObj->GetPosition();
	}

	return vRetPos;
}


Vector2 CComplexObject::GetPositionInPixels()
{
	Vector2 vRetPos(0,0);

	CComplexObjectArrayItem *pObj = CScaleformComplexObjectMgr::FindObjectFromArrayIndex(objectId);

	if (pObj)
	{
		if ((pObj->GetActionRequired() & CO_ACTION_UPDATE_POSITION_X) == 0 && (pObj->GetActionRequired() & CO_ACTION_UPDATE_POSITION_Y) == 0)
		{
			CScaleformComplexObjectMgr::CheckAndSetupIfUninitialised(objectId);
		}

		vRetPos = pObj->GetPositionInPixels();
	}

	return vRetPos;
}

float CComplexObject::GetXPositionInPixels()
{
	CComplexObjectArrayItem *pObj = CScaleformComplexObjectMgr::FindObjectFromArrayIndex(objectId);

	if (pObj)
	{
		if ((pObj->GetActionRequired() & CO_ACTION_UPDATE_POSITION_X) == 0)
		{
			CScaleformComplexObjectMgr::CheckAndSetupIfUninitialised(objectId);
		}

		return pObj->GetXPositionInPixels();
	}

	return 0.0f;
}

float CComplexObject::GetYPositionInPixels()
{
	CComplexObjectArrayItem *pObj = CScaleformComplexObjectMgr::FindObjectFromArrayIndex(objectId);

	if (pObj)
	{
		if ((pObj->GetActionRequired() & CO_ACTION_UPDATE_POSITION_Y) == 0)
		{
			CScaleformComplexObjectMgr::CheckAndSetupIfUninitialised(objectId);
		}

		return pObj->GetYPositionInPixels();
	}

	return 0.0f;
}

Vector2 CComplexObject::GetSize()
{
	Vector2 vRetSize(0,0);

	CComplexObjectArrayItem *pObj = CScaleformComplexObjectMgr::FindObjectFromArrayIndex(objectId);

	if (pObj)
	{
		if ( ((pObj->GetActionRequired() & CO_ACTION_UPDATE_WIDTH) == 0) || ((pObj->GetActionRequired() & CO_ACTION_UPDATE_HEIGHT) == 0) )
		{
			CScaleformComplexObjectMgr::CheckAndSetupIfUninitialised(objectId);
		}

		vRetSize = Vector2(pObj->GetWidth() / (float)ACTIONSCRIPT_STAGE_SIZE_X, pObj->GetHeight() / (float)ACTIONSCRIPT_STAGE_SIZE_Y);
	}

	return vRetSize;
}

Vector2 CComplexObject::GetScale()
{
	Vector2 vRetScale(0,0);

	CComplexObjectArrayItem *pObj = CScaleformComplexObjectMgr::FindObjectFromArrayIndex(objectId);

	if (pObj)
	{
		if ((pObj->GetActionRequired() & CO_ACTION_UPDATE_SCALE) == 0)
		{
			CScaleformComplexObjectMgr::CheckAndSetupIfUninitialised(objectId);
		}

		vRetScale = pObj->GetScale();
	}

	return vRetScale;
}


float CComplexObject::GetRotation()
{
	float rotation = 0.f;

	CComplexObjectArrayItem *pObj = CScaleformComplexObjectMgr::FindObjectFromArrayIndex(objectId);

	if (pObj)
	{
		if ((pObj->GetActionRequired() & CO_ACTION_UPDATE_ROTATION) == 0)
		{
			CScaleformComplexObjectMgr::CheckAndSetupIfUninitialised(objectId);
		}

		rotation = pObj->GetRotation();
	}

	return rotation;
}


bool CComplexObject::GetMember(const char *pInstanceName, bool &out_bool)
{
	float valueNumber;
	atString valueString;

	bool const c_success = GetMemberInternal(pInstanceName, out_bool, valueNumber, valueString);

	return c_success;
}

bool CComplexObject::GetMember(const char *pInstanceName, float &out_number)
{
	bool valueBool;
	atString valueString;

	bool const c_success = GetMemberInternal(pInstanceName, valueBool, out_number, valueString);

	return c_success;
}

bool CComplexObject::GetMember(const char *pInstanceName, atString &out_string)
{
	bool valueBool;
	float valueNumber;

	bool const c_success = GetMemberInternal(pInstanceName, valueBool, valueNumber, out_string);

	return c_success;
}

bool CComplexObject::GetMemberInternal(const char *pInstanceName, bool &out_valueBool, float &out_valueNumber, atString &out_valueString)
{
	out_valueBool = false;
	out_valueNumber = 0.0f;
	out_valueString.Clear();

	CComplexObjectArrayItem *pObj = CScaleformComplexObjectMgr::FindObjectFromArrayIndex(objectId);

	if (pObj)
	{
		if ((pObj->GetActionRequired() & CO_ACTION_UPDATE_MEMBERS) == 0)
		{
			CScaleformComplexObjectMgr::CheckAndSetupIfUninitialised(objectId);
		}

		eMemberValueType const c_typeUsed = pObj->GetMember(pInstanceName, out_valueBool, out_valueNumber, out_valueString);

		if (c_typeUsed == MEMBER_VALUE_TYPE_INVALID)  // we didnt find a cached value so create it from the object itself
		{
#if __SCALEFORM_CRITICAL_SECTIONS
			SYS_CS_SYNC(gs_ScaleformMovieCS[CScaleformComplexObjectMgr::FindObjectFromArrayIndex(objectId)->GetMovieId()]);
#endif

			GFxValue memberValue;
			pObj->m_asObject.GetMember(pInstanceName, &memberValue);

			if (memberValue.IsDefined() && (memberValue.GetType() == GFxValue::VT_Boolean || memberValue.GetType() == GFxValue::VT_Number || memberValue.GetType() == GFxValue::VT_String))
			{
				sComplectObjectMembers *newSetMember = &(pObj->m_properties[CScaleformComplexObjectMgr::GetBuffer()].m_member).Grow();

				newSetMember->m_instanceName = pInstanceName;
				newSetMember->m_update = false;

				switch (memberValue.GetType())
				{
					case GFxValue::VT_Boolean:
					{
						out_valueBool = memberValue.GetBool();
						newSetMember->value_bool = out_valueBool;
						newSetMember->m_type = MEMBER_VALUE_TYPE_BOOL;
						return true;
					}

					case GFxValue::VT_Number:
					{
						out_valueNumber = (float)memberValue.GetNumber();
						newSetMember->value_number = out_valueNumber;
						newSetMember->m_type = MEMBER_VALUE_TYPE_NUMBER;
						return true;
					}

					case GFxValue::VT_String:
					{
						out_valueString = memberValue.GetString();
						newSetMember->value_string = out_valueString;
						newSetMember->m_type = MEMBER_VALUE_TYPE_STRING;
						return true;
					}

					default:
					{
						// NOP
					}
				}
			}
		}
		else
		{
			return true;
		}
	}

	return false;
}


float CComplexObject::GetWidth()
{
	CComplexObjectArrayItem *pObj = CScaleformComplexObjectMgr::FindObjectFromArrayIndex(objectId);

	if (pObj)
	{
		if ((pObj->GetActionRequired() & CO_ACTION_UPDATE_WIDTH) == 0)
		{
			CScaleformComplexObjectMgr::CheckAndSetupIfUninitialised(objectId);
		}

		return pObj->GetWidth();
	}

	return 0.0f;
}


float CComplexObject::GetHeight()
{
	CComplexObjectArrayItem *pObj = CScaleformComplexObjectMgr::FindObjectFromArrayIndex(objectId);

	if (pObj)
	{
		if ((pObj->GetActionRequired() & CO_ACTION_UPDATE_HEIGHT) == 0)
		{
			CScaleformComplexObjectMgr::CheckAndSetupIfUninitialised(objectId);
		}

		return pObj->GetHeight();
	}

	return 0.0f;
}


bool CComplexObject::IsVisible()
{
	CComplexObjectArrayItem *pObj = CScaleformComplexObjectMgr::FindObjectFromArrayIndex(objectId);

	if (pObj)
	{
		if ((pObj->GetActionRequired() & CO_ACTION_UPDATE_VISIBILITY) == 0)
		{
			CScaleformComplexObjectMgr::CheckAndSetupIfUninitialised(objectId);
		}

		return pObj->IsVisible();
	}

	return false;
}

s32 CComplexObject::GetCurrentFrame()
{
	CComplexObjectArrayItem *pObj = CScaleformComplexObjectMgr::FindObjectFromArrayIndex(objectId);

	if (pObj)
	{
		if ((pObj->GetActionRequired() & CO_ACTION_UPDATE_FRAME) == 0)
		{
			CScaleformComplexObjectMgr::CheckAndSetupIfUninitialised(objectId);
		}

		return pObj->GetCurrentFrame();
	}

	return 1;
}


const char *CComplexObject::GetTextField()
{
	CComplexObjectArrayItem *pObj = CScaleformComplexObjectMgr::FindObjectFromArrayIndex(objectId);

	if (pObj)
	{
		if ((pObj->GetActionRequired() & CO_ACTION_UPDATE_TEXTFIELD) == 0)
		{
			CScaleformComplexObjectMgr::CheckAndSetupIfUninitialised(objectId);
		}

		return pObj->GetTextField();
	}

	return NULL;
}

Color32 CComplexObject::GetColour()
{
	CComplexObjectArrayItem *pObj = CScaleformComplexObjectMgr::FindObjectFromArrayIndex(objectId);

	if (pObj)
	{
		if ((pObj->GetActionRequired() & CO_ACTION_UPDATE_COLOUR) == 0)
		{
			return pObj->GetColour();
		}
	}

	return Color32(0,0,0,255);
}

u8 CComplexObject::GetAlpha()
{
	CComplexObjectArrayItem *pObj = CScaleformComplexObjectMgr::FindObjectFromArrayIndex(objectId);

	if (pObj)
	{
		if ((pObj->GetActionRequired() & CO_ACTION_UPDATE_ALPHA) == 0)
		{
			CScaleformComplexObjectMgr::CheckAndSetupIfUninitialised(objectId);
		}

		return pObj->GetAlpha();
	}

	return 255;
}

s32 CComplexObject::GetDepth()
{
	CComplexObjectArrayItem *pObj = CScaleformComplexObjectMgr::FindObjectFromArrayIndex(objectId);

	if (pObj)
	{
		if ((pObj->GetActionRequired() & CO_ACTION_UPDATE_DEPTH) == 0)
		{
			CScaleformComplexObjectMgr::CheckAndSetupIfUninitialised(objectId);
		}

		return pObj->GetDepth();
	}

	return 0;
}

atHashWithStringBank CComplexObject::GetObjectNameHash()
{
	CComplexObjectArrayItem *pObj = CScaleformComplexObjectMgr::FindObjectFromArrayIndex(objectId);

	if (pObj)
	{
		return pObj->GetObjectNameHash();
	}

	return atHashWithStringBank::Null();
}

s32 CComplexObject::GetMovieId() const
{
	CComplexObjectArrayItem const * const c_obj = CScaleformComplexObjectMgr::FindObjectFromArrayIndexConst(objectId);
	return c_obj ? c_obj->GetMovieId() : INDEX_NONE;
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CComplexObjectArrayItem::GetStageRoot
// PURPOSE: gets the root object (stage) so we can add/get objects starting from here
/////////////////////////////////////////////////////////////////////////////////////
CComplexObject CComplexObject::GetStageRoot(s32 iMovieId)
{
	return CComplexObjectArrayItem::GetStageRoot(iMovieId);
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CComplexObjectArrayItem::GetObject
// PURPOSE: returns pointer to the object requested.  Will create an instance to use
//			if required or use an existing one previously set up.
/////////////////////////////////////////////////////////////////////////////////////
CComplexObject CComplexObject::GetObject(const char *pMovieClipName1, const char *pMovieClipName2, const char *pMovieClipName3)
{
	CComplexObject returnItem;

	CComplexObjectArrayItem * pObj = CScaleformComplexObjectMgr::FindObjectFromArrayIndex(objectId);
	if (pObj)
	{

#if __SCALEFORM_CRITICAL_SECTIONS
		SYS_CS_SYNC(gs_ScaleformMovieCS[pObj->GetMovieId()]);
#endif

        CComplexObjectArrayItem *pNewObject = CScaleformComplexObjectMgr::FindOrCreateSlot();
        // Grab again, as the "find or create" may grow our array, breaking the earlier pointer
        pObj = CScaleformComplexObjectMgr::FindObjectFromArrayIndex(objectId);
		returnItem.objectId = CComplexObjectArrayItem::GetObject(pObj, pNewObject, pMovieClipName1, pMovieClipName2, pMovieClipName3);
	}

	return returnItem;
}



////////////////////////////////////////////////////////////////////////////////////
// NAME:	CComplexObjectArrayItem::CreateEmptyMovieClip
// PURPOSE: creates an empty movie clip onto the stage at the specified depth.
//			Creates an object link to it and returns it.   Name is the ref name
/////////////////////////////////////////////////////////////////////////////////////
CComplexObject CComplexObject::CreateEmptyMovieClip(const char *pMovieClipName, s32 iDepth)
{
	CComplexObject returnItem;

	CComplexObjectArrayItem * pObj = CScaleformComplexObjectMgr::FindObjectFromArrayIndex(objectId);
	if (pObj)
	{

#if __SCALEFORM_CRITICAL_SECTIONS
		SYS_CS_SYNC(gs_ScaleformMovieCS[pObj->GetMovieId()]);
#endif

		CComplexObjectArrayItem *pNewObject = CScaleformComplexObjectMgr::FindOrCreateSlot();
        // Grab again, as the "find or create" may grow our array, breaking the earlier pointer
        pObj = CScaleformComplexObjectMgr::FindObjectFromArrayIndex(objectId);
		returnItem.objectId = (CComplexObjectArrayItem::CreateEmptyMovieClip(pObj, pNewObject, pMovieClipName, iDepth));
	}

	return returnItem;
}



////////////////////////////////////////////////////////////////////////////////////
// NAME:	CComplexObjectArrayItem::AttachMovieClip
// PURPOSE: attaches a movieclip onto the stage at the specified depth.  Creates
//			an object link to it and returns it.   Name is the ref name
/////////////////////////////////////////////////////////////////////////////////////
CComplexObject CComplexObject::AttachMovieClip(const char *pMovieClipName, s32 iDepth)
{
	CComplexObject returnItem;

	CComplexObjectArrayItem * pObj = CScaleformComplexObjectMgr::FindObjectFromArrayIndex(objectId);
	if (pObj)
	{

#if __SCALEFORM_CRITICAL_SECTIONS
		SYS_CS_SYNC(gs_ScaleformMovieCS[pObj->GetMovieId()]);
#endif

        CComplexObjectArrayItem *pNewObject = CScaleformComplexObjectMgr::FindOrCreateSlot();
        // Grab again, as the "find or create" may grow our array, breaking the earlier pointer
        pObj = CScaleformComplexObjectMgr::FindObjectFromArrayIndex(objectId);
		if (sfVerifyf(pMovieClipName, "AttachMovieClip requires a valid symbol name"))
		{
			returnItem.objectId = (CComplexObjectArrayItem::AttachMovieClip(pObj, pNewObject, pMovieClipName, iDepth));
		}
	}

	return returnItem;
}


////////////////////////////////////////////////////////////////////////////////////
// NAME:	CComplexObjectArrayItem::AttachMovieClip
// PURPOSE: attaches a movieclip onto the stage at the specified depth.  Creates
//			an object link to it and returns it.   Name is the ref name
/////////////////////////////////////////////////////////////////////////////////////
CComplexObject CComplexObject::AttachMovieClipInstance(const char *pMovieClipName, s32 iDepth)
{
	CComplexObject returnItem;

	CComplexObjectArrayItem * pObj = CScaleformComplexObjectMgr::FindObjectFromArrayIndex(objectId);
	if (pObj)
	{

#if __SCALEFORM_CRITICAL_SECTIONS
		SYS_CS_SYNC(gs_ScaleformMovieCS[pObj->GetMovieId()]);
#endif

        CComplexObjectArrayItem *pNewObject = CScaleformComplexObjectMgr::FindOrCreateSlot();
        // Grab again, as the "find or create" may grow our array, breaking the earlier pointer
        pObj = CScaleformComplexObjectMgr::FindObjectFromArrayIndex(objectId);
		if (sfVerifyf(pMovieClipName, "AttachMovieClipInstance requires a valid symbol name"))
		{
			returnItem.objectId = (CComplexObjectArrayItem::AttachMovieClipInstance(pObj, pNewObject, pMovieClipName, iDepth));
		}
	}

	return returnItem;
}



GFxValue *CComplexObject::GetComplexObjectGfxValue()
{
	CComplexObjectArrayItem *pObj = CScaleformComplexObjectMgr::FindObjectFromArrayIndex(objectId);

	if (pObj)
	{
		return pObj->GetGfxValue();
	}

	return NULL;
}

void CComplexObjectArrayItem::RemoveObject()
{
	if (m_asObject.IsDefined())
	{
		if (m_uState & CO_STATE_ATTACHED_BY_CODE)  // only call removeMovieClip on objects that we attached
		{
#if __BANK
			if (CScaleformComplexObjectMgr::DetailedLogging())
			{
				sfDisplayf("COMPLEXOBJECT: removeMovieClip called on '%s' on movie %s (%d)", GetObjectNameHash().GetCStr(), CScaleformMgr::GetMovieFilename(m_iMovieId), m_iMovieId);
			}
#endif // __BANK

			m_asObject.Invoke("removeMovieClip");
		}

		m_asObject.SetUndefined();
	}

    m_cPendingString[0] = NULL;
	m_cPendingString[1] = NULL;
	m_cPendingString[2] = NULL;

	for (s32 iBuf = 0; iBuf < MAX_CO_BUFFERS; iBuf++)
	{
        m_properties[iBuf].cTextfield = NULL;
		m_properties[iBuf].m_member.Reset(true);

	}

	m_bInitialised = false;
	m_uState = 0;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CComplexObjectArrayItem::CComplexObjectArrayItem
// PURPOSE: constructor - initialise
/////////////////////////////////////////////////////////////////////////////////////
CComplexObjectArrayItem::CComplexObjectArrayItem()
    : m_iPendingDepth( 0 )
    , m_iPendingParentObjectUniqueId( INVALID_COMPLEX_OBJECT_ID )
    , m_uState( 0 )
    , m_iMovieId( -1 )
    , m_iUniqueId( INVALID_COMPLEX_OBJECT_ID )
    , m_bInitialised( false )
    , m_bInitialisedEarly( false )
{
	InitProperties();  // default these
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CComplexObjectArrayItem::SetObject
// PURPOSE: sets the GFxValue
/////////////////////////////////////////////////////////////////////////////////////
void CComplexObjectArrayItem::SetObject(GFxValue newObject)
{
	m_asObject = newObject;
	InitProperties();
	m_bInitialised = true;
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CComplexObjectArrayItem::SetUniqueId
// PURPOSE: stores a unique identifier, used for checking later
/////////////////////////////////////////////////////////////////////////////////////
void CComplexObjectArrayItem::SetUniqueId()
{
	m_iUniqueId = CScaleformComplexObjectMgr::GetCurrentUniqueId();
}

bool CComplexObjectArrayItem::IsActionRequiredOnEitherBuffer(u32 uflag)
{
	for (s32 iBuf = 0; iBuf < MAX_CO_BUFFERS; iBuf++)
	{
		if (GetActionRequired(iBuf) & uflag)
		{
			return true;
		}
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CComplexObjectArrayItem::InitProperties
// PURPOSE: initialises the starting values of the properties on both buffers
/////////////////////////////////////////////////////////////////////////////////////
void CComplexObjectArrayItem::InitProperties()
{
	bool bObjectValid = (m_asObject.IsDefined() && m_asObject.IsDisplayObject());
	GFxValue::DisplayInfo displayInfo;

	if (bObjectValid)
		m_asObject.GetDisplayInfo(&displayInfo);

	for (s32 iBuf = 0; iBuf < MAX_CO_BUFFERS; iBuf++)  // initialise on both buffers unless (as nothing is using this yet) unless its been flagged for setting up
	{
		// initial values from the object itself:
		if (!IsActionRequiredOnEitherBuffer(CO_ACTION_UPDATE_POSITION_X))
		{
			if (bObjectValid)
				m_properties[iBuf].vPosition.x = (float)displayInfo.GetX() / ACTIONSCRIPT_STAGE_SIZE_X;
			else
				m_properties[iBuf].vPosition.x = 0.0f;
		}

		if (!IsActionRequiredOnEitherBuffer(CO_ACTION_UPDATE_POSITION_Y))
		{
			if (bObjectValid)
				m_properties[iBuf].vPosition.y = (float)displayInfo.GetY() / ACTIONSCRIPT_STAGE_SIZE_Y;
			else
				m_properties[iBuf].vPosition.y = 0.0f;
		}

		if (!IsActionRequiredOnEitherBuffer(CO_ACTION_UPDATE_SCALE))
		{
			if (bObjectValid)
				m_properties[iBuf].vScale = Vector2((float)displayInfo.GetXScale(), (float)displayInfo.GetYScale());
			else
				m_properties[iBuf].vScale = Vector2(100.0f, 100.0f);
		}

		if (!IsActionRequiredOnEitherBuffer(CO_ACTION_UPDATE_ROTATION))
		{
			if (bObjectValid)
				m_properties[iBuf].fRotation = (float)displayInfo.GetRotation();
			else
				m_properties[iBuf].fRotation = 0.f;
		}

		if (!IsActionRequiredOnEitherBuffer(CO_ACTION_UPDATE_MEMBERS))
		{
			m_properties[iBuf].m_member.Reset();
		}

		if (!IsActionRequiredOnEitherBuffer(CO_ACTION_UPDATE_WIDTH))
		{
			if (bObjectValid)
			{
				GFxValue widthValue;
				m_asObject.GetMember("_width", &widthValue);
				m_properties[iBuf].fWidth = (float)widthValue.GetNumber();
			}
			else
			{
				m_properties[iBuf].fWidth = 0.0f;
			}
		}

		if (!IsActionRequiredOnEitherBuffer(CO_ACTION_UPDATE_HEIGHT))
		{
			if (bObjectValid)
			{
				GFxValue heightValue;
				m_asObject.GetMember("_height", &heightValue);
				m_properties[iBuf].fHeight = (float)heightValue.GetNumber();
			}
			else
			{
				m_properties[iBuf].fHeight = 0.0f;
			}
		}

		if (!IsActionRequiredOnEitherBuffer(CO_ACTION_UPDATE_VISIBILITY))
		{
			if (bObjectValid)
			{
				m_properties[iBuf].bVisible = displayInfo.GetVisible();
			}
			else
			{
				m_properties[iBuf].bVisible = true;
			}
		}
		
		if (!IsActionRequiredOnEitherBuffer(CO_ACTION_UPDATE_FRAME))
		{
			m_properties[iBuf].iFrame = 1;
			m_properties[iBuf].bFrameJumpPlay = false;
		}

		if ( (!IsActionRequiredOnEitherBuffer(CO_ACTION_UPDATE_TEXTFIELD)) && (!IsActionRequiredOnEitherBuffer(CO_ACTION_UPDATE_TEXTFIELD_HTML)) )
        {
            m_properties[iBuf].cTextfield = NULL;
        }

		if (!IsActionRequiredOnEitherBuffer(CO_ACTION_UPDATE_COLOUR))
		{
			m_properties[iBuf].colour.SetRed(0);
			m_properties[iBuf].colour.SetGreen(0);
			m_properties[iBuf].colour.SetBlue(0);

			if (!IsActionRequiredOnEitherBuffer(CO_ACTION_UPDATE_ALPHA))
				m_properties[iBuf].colour.SetAlpha(s32( ( (float)displayInfo.GetAlpha() / 100.0f ) * 255.0f) );
		}

		if( !IsActionRequiredOnEitherBuffer(CO_ACTION_UPDATE_DEPTH) )
		{
			m_properties[iBuf].depth = 0;

			if (bObjectValid)
			{
				GFxValue result;
				if( m_asObject.Invoke("getDepth", &result ) )
				{
					m_properties[iBuf].depth = (s32)result.GetNumber();
				}
			}
		}
	}
}




/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CComplexObjectArrayItem::GetObject
// PURPOSE: returns pointer to the object requested.  Will create an instance to use
//			if required or use an existing one previously set up.
/////////////////////////////////////////////////////////////////////////////////////
COMPLEX_OBJECT_ID CComplexObjectArrayItem::GetObject(CComplexObjectArrayItem *pObject, CComplexObjectArrayItem *pNewObject, const char *pMovieClipName1, const char *pMovieClipName2, const char *pMovieClipName3)
{
	if (sfVerifyf(pObject, "CComplexObjectArrayItem::GetObject - object passed is invalid!"))
	{
		// just got to pass the parent object id into here and get it after the new one created instead OF calling it on it for it to work
		if (pNewObject)
		{
			pNewObject->SetMovieId(pObject->GetMovieId());
			pNewObject->SetUniqueId();
			pNewObject->SetObjectName(pMovieClipName1);

			for (s32 iBuf = 0; iBuf < MAX_CO_BUFFERS; iBuf++)
			{
				pNewObject->m_properties[iBuf].iActionRequired = 0;
			}

			if (CSystem::IsThisThreadId(SYS_THREAD_RENDER))
			{
				pObject->GetObjectInternal(pNewObject, pMovieClipName1, pMovieClipName2, pMovieClipName3);
			}
			else
			{
				pNewObject->m_cPendingString[0] = NULL;
				pNewObject->m_cPendingString[1] = NULL;
				pNewObject->m_cPendingString[2] = NULL;

				s32 iIndex = 0;

				if (pMovieClipName1)
					pNewObject->m_cPendingString[iIndex++] = pMovieClipName1;

				if (pMovieClipName2)
					pNewObject->m_cPendingString[iIndex++] = pMovieClipName2;

				if (pMovieClipName3)
					pNewObject->m_cPendingString[iIndex++] = pMovieClipName3;

				pNewObject->m_iPendingParentObjectUniqueId = pObject->GetUniqueId();

				pNewObject->m_uState |= CO_STATE_PENDING_GET_OBJECT;
			}

			return pNewObject->GetUniqueId();
		}
	}

	return INVALID_COMPLEX_OBJECT_ID;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CComplexObjectArrayItem::GetObjectInternal
// PURPOSE: returns pointer to the object requested.  Will create an instance to use
//			if required or use an existing one previously set up.
/////////////////////////////////////////////////////////////////////////////////////
CComplexObjectArrayItem *CComplexObjectArrayItem::GetObjectInternal(CComplexObjectArrayItem *pNewObject, const char *pMovieClipName1, const char *pMovieClipName2, const char *pMovieClipName3)
{
	//
	// attach or get any items that this clip relies on now
	//
	if (IsPendingGetRoot())
	{
		CComplexObjectArrayItem::GetStageRootInternal(this);
	}

	if (IsPendingCreate())
	{
		if (m_iPendingParentObjectUniqueId != INVALID_COMPLEX_OBJECT_ID)
		{
			CComplexObjectArrayItem *pParentObject = CScaleformComplexObjectMgr::GetObjectFromUniqueId(m_iPendingParentObjectUniqueId);

			if (sfVerifyf(pParentObject, "CComplexObjectArrayItem::GetObjectInternal - Parent object with unique id %d not available to attach items to!", m_iPendingParentObjectUniqueId))
			{
				pParentObject->CreateEmptyMovieClipInternal(this, m_iPendingDepth, m_cPendingString[0].c_str());
			}
		}
	}

	if (IsPendingAttach())
	{
		if (m_iPendingParentObjectUniqueId != INVALID_COMPLEX_OBJECT_ID)
		{
			CComplexObjectArrayItem *pParentObject = CScaleformComplexObjectMgr::GetObjectFromUniqueId(m_iPendingParentObjectUniqueId);

			if (sfVerifyf(pParentObject, "CComplexObjectArrayItem::GetObjectInternal - Parent object with unique id %d not available to attach items to!", m_iPendingParentObjectUniqueId))
			{
				pParentObject->AttachMovieClipInternal(this, m_cPendingString[0].c_str(), m_iPendingDepth, m_cPendingString[1].c_str());
			}
		}
	}

	if (IsPendingGet())
	{
		if (m_iPendingParentObjectUniqueId != INVALID_COMPLEX_OBJECT_ID)
		{
			CComplexObjectArrayItem *pParentObject = CScaleformComplexObjectMgr::GetObjectFromUniqueId(m_iPendingParentObjectUniqueId);

			if (sfVerifyf(pParentObject, "CComplexObjectArrayItem::GetObjectInternal - Parent object with unique id %d not available to get items on!", m_iPendingParentObjectUniqueId))
			{
				pParentObject->GetObjectInternal(this, m_cPendingString[0].c_str(), m_cPendingString[1].c_str(), m_cPendingString[2].c_str());
			}
		}
	}
	
	//
	// get:
	//
	if (sfVerifyf(m_asObject.IsDefined(), "CComplexObjectArrayItem::GetObjectInternal - Parent object has an invalid GfxValue!"))
	{
		const char *pMovieClipName = NULL;

		if (pMovieClipName3 && pMovieClipName3[0] != '\0')
		{
			pMovieClipName = pMovieClipName3;
		}
		else if (pMovieClipName2 && pMovieClipName2[0] != '\0')
		{
			pMovieClipName = pMovieClipName2;
		}
		else if (pMovieClipName1 && pMovieClipName1[0] != '\0')
		{
			pMovieClipName = pMovieClipName1;
		}
		else
		{
			sfAssertf(0, "CComplexObjectArrayItem::GetObject: No valid movie clip passed");
			return NULL;
		}

		if (pMovieClipName3 && pMovieClipName3[0] != '\0')
		{
			GFxValue pTempValue1, pTempValue2;
			m_asObject.GetMember(pMovieClipName1, &pTempValue1);
			pTempValue1.GetMember(pMovieClipName2, &pTempValue2);
			pTempValue2.GetMember(pMovieClipName3, &(pNewObject->m_asObject));
		}
		else if (pMovieClipName2 && pMovieClipName2[0] != '\0')
		{
			GFxValue pTempValue;
			m_asObject.GetMember(pMovieClipName1, &pTempValue);
			pTempValue.GetMember(pMovieClipName2, &(pNewObject->m_asObject));
		}
		else
		{
			m_asObject.GetMember(pMovieClipName1, &(pNewObject->m_asObject));
		}

		if (sfVerifyf(pNewObject->m_asObject.IsDefined(), "CComplexObjectArrayItem::GetObjectInternal - object '%s' is not available on '%s'", pMovieClipName, GetObjectNameHash().GetCStr()))
		{
		#if __BANK
			if (CScaleformComplexObjectMgr::DetailedLogging())
			{
				sfDisplayf("COMPLEXOBJECT: created object '%s' on movie %s (%d)", pMovieClipName, CScaleformMgr::GetMovieFilename(m_iMovieId), m_iMovieId);
			}
		#endif // __BANK

			pNewObject->m_uState &= ~CO_STATE_PENDING_GET_OBJECT;

			// empty any pending info here as we have now set it up
            pNewObject->m_cPendingString[0] = NULL;
			pNewObject->m_cPendingString[1] = NULL;
			pNewObject->m_cPendingString[2] = NULL;
			pNewObject->m_iPendingDepth = 0;
			pNewObject->m_iPendingParentObjectUniqueId = INVALID_COMPLEX_OBJECT_ID;

			pNewObject->m_uState |= CO_STATE_ACTIVE;

			pNewObject->InitProperties();

			return pNewObject;
		}
	}

	return NULL;
}




void CComplexObjectArrayItem::SetToInvokeOnObject(bool bSet)
{
	if (bSet)
	{
		m_uState |= CO_STATE_PENDING_INVOKE;
	}
	else
	{
		m_uState &= ~CO_STATE_PENDING_INVOKE;
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CComplexObjectArrayItem::ReleaseObjectInternal
// PURPOSE: removes the object once finished with and deletes the entry from the
//			array
/////////////////////////////////////////////////////////////////////////////////////
bool CComplexObjectArrayItem::ReleaseObjectInternal(s32 iIndex, bool bForce)
{
	if (CSystem::IsThisThreadId(SYS_THREAD_RENDER) || bForce)  // can only release on RT
	{
		if (sfVerifyf(iIndex < CScaleformComplexObjectMgr::sm_ObjectArray.GetCount(), "CComplexObjectArrayItem::ReleaseObject - Trying to remove object outside of array (%d/%d)", iIndex, CScaleformComplexObjectMgr::sm_ObjectArray.GetCount()))
		{
	#if __BANK
			if (CScaleformComplexObjectMgr::DetailedLogging())
			{
				sfDisplayf("COMPLEXOBJECT: Object '%s' on movie %s (%d) has been removed", CScaleformComplexObjectMgr::sm_ObjectArray[iIndex].GetObjectNameHash().GetCStr(), CScaleformMgr::GetMovieFilename(CScaleformComplexObjectMgr::sm_ObjectArray[iIndex].GetMovieId()), CScaleformComplexObjectMgr::sm_ObjectArray[iIndex].GetMovieId());
			}
	#endif // __BANK

			CComplexObjectArrayItem& currentObject = CScaleformComplexObjectMgr::sm_ObjectArray[iIndex];
            /*
            // TODO - We could forcible remove invokes here rather than wait until later.
            // Another workaround was found that was less intrusive though
            if( currentObject.IsPendingInvoke() )
            {
                CScaleformMgr::RemoveInvokesForComplexObject( currentObject.GetUniqueId() );
            }
            */

            currentObject.SetObjectName("\0");
            currentObject.RemoveObject();

			// if removing last in the array, then remove it, however
			if (iIndex == CScaleformComplexObjectMgr::sm_ObjectArray.GetCount()-1)
			{
				CScaleformComplexObjectMgr::sm_ObjectArray.Delete(iIndex);
			}

			return true;
		}
	}
	else
	{
		// flag to be released
		CScaleformComplexObjectMgr::sm_ObjectArray[iIndex].m_uState |= CO_STATE_FLAGGED_FOR_RELEASE;

		CScaleformComplexObjectMgr::sm_ObjectArray[iIndex].SetObjectName("\0");
	}

	return false;
}




bool CComplexObjectArrayItem::ReleaseObject(CComplexObjectArrayItem *pObject, bool bForce)
{
	if (CSystem::IsThisThreadId(SYS_THREAD_RENDER) || bForce)  // can only release on RT
	{
		// this must be called within the safe zone
		s32 iIndex = CScaleformComplexObjectMgr::FindObjectIndex(pObject);

		if (iIndex >= 0)
		{
			return ReleaseObjectInternal(iIndex, bForce);
		}
	}
	else
	{
		// flag to be released
		pObject->m_uState |= CO_STATE_FLAGGED_FOR_RELEASE;
	}

	return false;
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CComplexObjectArrayItem::FindOrCreateSlot
// PURPOSE: reuse an empty slot or create a new one
/////////////////////////////////////////////////////////////////////////////////////
CComplexObjectArrayItem *CScaleformComplexObjectMgr::FindOrCreateSlot()
{
	for (s32 i = 0; i < CScaleformComplexObjectMgr::sm_ObjectArray.GetCount(); i++)
	{
		if (CScaleformComplexObjectMgr::sm_ObjectArray[i].IsUnused())
		{
			CScaleformComplexObjectMgr::SetCurrentUniqueId(i);
			return (&CScaleformComplexObjectMgr::sm_ObjectArray[i]);
		}
	}
	
	CScaleformComplexObjectMgr::SetCurrentUniqueId(sm_ObjectArray.GetCount());
	return &sm_ObjectArray.Grow();
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CComplexObjectArrayItem::AttachMovieClip
// PURPOSE: attaches a movieclip onto the stage at the specified depth.  Creates
//			an object link to it and returns it.   Name is the ref name
/////////////////////////////////////////////////////////////////////////////////////
COMPLEX_OBJECT_ID CComplexObjectArrayItem::AttachMovieClip(CComplexObjectArrayItem *pObject, CComplexObjectArrayItem *pNewObject, const char *pMovieClipName, s32 iDepth)
{
	if (sfVerifyf(pObject, "CComplexObjectArrayItem::AttachMovieClip - object passed is invalid!"))
	{
		if (pNewObject)
		{
            s32 const c_movieID = pObject->GetMovieId();
            pNewObject->SetMovieId( c_movieID );
			pNewObject->SetUniqueId();

			char cInstanceName[64];
			formatf(cInstanceName, "%s_%d", pMovieClipName, pNewObject->GetUniqueId(), NELEM(cInstanceName));  // unique instance name based on the unique id
			pNewObject->SetObjectName(cInstanceName);

			for (s32 iBuf = 0; iBuf < MAX_CO_BUFFERS; iBuf++)
			{
				pNewObject->m_properties[iBuf].iActionRequired = 0;
			}

			if (CSystem::IsThisThreadId(SYS_THREAD_RENDER))
			{
				pObject->AttachMovieClipInternal(pNewObject, pMovieClipName, iDepth, cInstanceName);
			}
			else
			{
				pNewObject->m_cPendingString[0] = pMovieClipName;
				pNewObject->m_cPendingString[1] = cInstanceName;
				pNewObject->m_iPendingDepth = iDepth;
				pNewObject->m_iPendingParentObjectUniqueId = pObject->GetUniqueId();

				pNewObject->m_uState |= CO_STATE_PENDING_ATTACH;
			}

			return pNewObject->GetUniqueId();
		}
	}
	
	return INVALID_COMPLEX_OBJECT_ID;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CComplexObjectArrayItem::CreateEmptyMovieClip
// PURPOSE: creates an empty movieclip onto the stage at the specified depth. Creates
//			an object link to it and returns it.   Name is the ref name
/////////////////////////////////////////////////////////////////////////////////////
COMPLEX_OBJECT_ID CComplexObjectArrayItem::CreateEmptyMovieClip(CComplexObjectArrayItem *pObject, CComplexObjectArrayItem *pNewObject, const char *pMovieClipName, s32 iDepth)
{
	if (sfVerifyf(pObject, "CComplexObjectArrayItem::CreateEmptyMovieClip - object passed is invalid!"))
	{
		if (pNewObject)
		{
            s32 const c_movieID = pObject->GetMovieId();
            pNewObject->SetMovieId( c_movieID );
			pNewObject->SetUniqueId();

			char cInstanceName[64];
			formatf(cInstanceName, "%s_%d", pMovieClipName, pNewObject->GetUniqueId(), NELEM(cInstanceName));  // unique instance name based on the unique id
			pNewObject->SetObjectName(cInstanceName);

			for (s32 iBuf = 0; iBuf < MAX_CO_BUFFERS; iBuf++)
			{
				pNewObject->m_properties[iBuf].iActionRequired = 0;
			}

			if (CSystem::IsThisThreadId(SYS_THREAD_RENDER))
			{
				pObject->CreateEmptyMovieClipInternal(pNewObject, iDepth, cInstanceName);
			}
			else
			{
				pNewObject->m_cPendingString[0] = cInstanceName;
				pNewObject->m_iPendingDepth = iDepth;
				pNewObject->m_iPendingParentObjectUniqueId = pObject->GetUniqueId();

				pNewObject->m_uState |= CO_STATE_PENDING_CREATE;
			}

			return pNewObject->GetUniqueId();
		}
	}

	return INVALID_COMPLEX_OBJECT_ID;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CComplexObjectArrayItem::AttachMovieClipInstance
// PURPOSE: attaches a movieclip onto the stage at the specified depth.  Creates
//			an object link to it and returns it - attaches it using the depth as
//			the ref name
/////////////////////////////////////////////////////////////////////////////////////
COMPLEX_OBJECT_ID CComplexObjectArrayItem::AttachMovieClipInstance(CComplexObjectArrayItem *pObject, CComplexObjectArrayItem *pNewObject, const char *pMovieClipName, s32 iDepth)
{
	if (sfVerifyf(pObject, "CComplexObjectArrayItem::GetObject - object passed is invalid!"))
	{
		if (pNewObject)
		{
			for (s32 iBuf = 0; iBuf < MAX_CO_BUFFERS; iBuf++)
			{
				pNewObject->m_properties[iBuf].iActionRequired = 0;
			}

            s32 const c_movieID = pObject->GetMovieId();
			pNewObject->SetMovieId( c_movieID );
			pNewObject->SetUniqueId();

			char cInstanceName[64];
			formatf(cInstanceName, "%s_%d", pMovieClipName, pNewObject->GetUniqueId(), NELEM(cInstanceName));  // unique instance name based on the unique id
			pNewObject->SetObjectName(cInstanceName);

			if (CSystem::IsThisThreadId(SYS_THREAD_RENDER))
			{
				pObject->AttachMovieClipInternal(pNewObject, pMovieClipName, iDepth, cInstanceName);
			}
			else
			{
				pNewObject->m_cPendingString[0] = pMovieClipName;
				pNewObject->m_cPendingString[1] = cInstanceName;
				pNewObject->m_iPendingDepth = iDepth;
				pNewObject->m_iPendingParentObjectUniqueId = pObject->GetUniqueId();

				pNewObject->m_uState |= CO_STATE_PENDING_ATTACH;
			}

			return pNewObject->GetUniqueId();
		}
	}

	return INVALID_COMPLEX_OBJECT_ID;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CComplexObjectArrayItem::AttachMovieClipInternal
// PURPOSE: attaches a movieclip onto the stage at the specified depth.
/////////////////////////////////////////////////////////////////////////////////////
CComplexObjectArrayItem *CComplexObjectArrayItem::AttachMovieClipInternal(CComplexObjectArrayItem *pNewObject, const char *pMovieClipName, s32 iDepth, const char *pInstanceName)
{
	//
	// attach or get any items that this clip relies on now
	//
	if (IsPendingGetRoot())
	{
		CComplexObjectArrayItem::GetStageRootInternal(this);
	}

	if (IsPendingCreate())
	{
		if (m_iPendingParentObjectUniqueId != INVALID_COMPLEX_OBJECT_ID)
		{
			CComplexObjectArrayItem *pParentObject = CScaleformComplexObjectMgr::GetObjectFromUniqueId(m_iPendingParentObjectUniqueId);

			if (sfVerifyf(pParentObject, "CComplexObjectArrayItem::AttachMovieClipInternal - Parent object with unique id %d not available to attach items to!", m_iPendingParentObjectUniqueId))
			{
				pParentObject->CreateEmptyMovieClipInternal(this, m_iPendingDepth, m_cPendingString[0].c_str());
			}
		}
	}

	if (IsPendingAttach())
	{
		if (m_iPendingParentObjectUniqueId != INVALID_COMPLEX_OBJECT_ID)
		{
			CComplexObjectArrayItem *pParentObject = CScaleformComplexObjectMgr::GetObjectFromUniqueId(m_iPendingParentObjectUniqueId);

			if (sfVerifyf(pParentObject, "CComplexObjectArrayItem::AttachMovieClipInternal - Parent object with unique id %d not available to attach items to!", m_iPendingParentObjectUniqueId))
			{
				pParentObject->AttachMovieClipInternal(this, m_cPendingString[0].c_str(), m_iPendingDepth, m_cPendingString[1].c_str());
			}
		}
	}

	if (IsPendingGet())
	{
		if (m_iPendingParentObjectUniqueId != INVALID_COMPLEX_OBJECT_ID)
		{
			CComplexObjectArrayItem *pParentObject = CScaleformComplexObjectMgr::GetObjectFromUniqueId(m_iPendingParentObjectUniqueId);

			if (sfVerifyf(pParentObject, "CComplexObjectArrayItem::AttachMovieClipInternal - Parent object with unique id %d not available to get items on!", m_iPendingParentObjectUniqueId))
			{
				pParentObject->GetObjectInternal(this, m_cPendingString[0].c_str(), m_cPendingString[1].c_str(), m_cPendingString[2].c_str());
			}
		}
	}

	//
	// attach:
	//
	if (sfVerifyf(m_asObject.IsDisplayObject(), "CComplexObjectArrayItem::AttachMovieClipInternal - Parent object has an invalid GfxValue!"))
	{
#if __ASSERT

#if __SCALEFORM_CRITICAL_SECTIONS
	SYS_CS_SYNC(gs_ScaleformMovieCS[GetMovieId()]);
#endif
		if (sfVerifyf(!m_asObject.GFxValue::HasMember(pInstanceName), "CComplexObjectArrayItem::AttachMovieClipInternal - Trying to attach movieclip '%s' to object '%s' when it is already attached!", pMovieClipName, GetObjectNameHash().GetCStr()))
#endif // __ASSERT
		{
			sfAssertf(iDepth < MAX_COMPLEX_OBJECT_DEPTH, "CComplexObjectArrayItem::AttachMovieClipInternal - Trying to attach an object at depth %d when max is %d", iDepth, MAX_COMPLEX_OBJECT_DEPTH);

#if __ASSERT
			//
			// check if there is already an object at this depth:
			//
			GFxValue returnValue;
			GFxValue args[1];

			args[0].SetNumber(iDepth);
			m_asObject.Invoke("getInstanceAtDepth", &returnValue, args, 1);

			if (returnValue.IsDefined())
			{
				GFxValue instanceName;
				m_asObject.GetMember("_name", &instanceName);
				sfAssertf(0, "CComplexObjectArrayItem::AttachMovieClipInternal - Trying to attach %s at depth %d but '%s' object is already on the stage at this depth!", pMovieClipName, iDepth, instanceName.GetString());
			}
#endif // __ASSERT

			GFxValue asNewObject;
			if (sfVerifyf(m_asObject.AttachMovie(&asNewObject, pMovieClipName, pInstanceName, iDepth), "CComplexObjectArrayItem::AttachMovieClipInternal - Failed to attach symbol '%s', name '%s' to object '%s' - Object possibly not on stage", pMovieClipName, pInstanceName, GetObjectNameHash().GetCStr()))
			{
				pNewObject->SetObject(asNewObject);

#if __BANK
				if (CScaleformComplexObjectMgr::DetailedLogging())
				{
					sfDisplayf("COMPLEXOBJECT: attached object '%s' on movie %s (%d) at depth %d", GetObjectNameHash().GetCStr(), CScaleformMgr::GetMovieFilename(m_iMovieId), m_iMovieId, iDepth);
				}
#endif // __BANK

				// now set it active
				pNewObject->m_uState &= ~CO_STATE_PENDING_ATTACH;

				// empty any pending info here as we have now set it up
				pNewObject->m_cPendingString[0] = NULL;
				pNewObject->m_cPendingString[1] = NULL;
				pNewObject->m_iPendingDepth = 0;
				pNewObject->m_iPendingParentObjectUniqueId = INVALID_COMPLEX_OBJECT_ID;

				pNewObject->m_uState |= CO_STATE_ACTIVE;
				pNewObject->m_uState |= CO_STATE_ATTACHED_BY_CODE;  // flag that this was attached in code (so we can removemovieclip on it when we release this object)

				pNewObject->InitProperties();

				return pNewObject;
			}
		}
	}

	return NULL;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CComplexObjectArrayItem::CreateEmptyMovieClipInternal
// PURPOSE: creates an empty movieclip onto the stage at the specified depth.
/////////////////////////////////////////////////////////////////////////////////////
CComplexObjectArrayItem *CComplexObjectArrayItem::CreateEmptyMovieClipInternal(CComplexObjectArrayItem *pNewObject, s32 iDepth, const char *pInstanceName)
{
	//
	// attach or get any items that this clip relies on now
	//
	if (IsPendingGetRoot())
	{
		CComplexObjectArrayItem::GetStageRootInternal(this);
	}

	if (IsPendingCreate())
	{
		if (m_iPendingParentObjectUniqueId != INVALID_COMPLEX_OBJECT_ID)
		{
			CComplexObjectArrayItem *pParentObject = CScaleformComplexObjectMgr::GetObjectFromUniqueId(m_iPendingParentObjectUniqueId);

			if (sfVerifyf(pParentObject, "CComplexObjectArrayItem::CreateEmptyMovieClipInternal - Parent object with unique id %d not available to attach items to!", m_iPendingParentObjectUniqueId))
			{
				pParentObject->CreateEmptyMovieClipInternal(this, m_iPendingDepth, m_cPendingString[0].c_str());
			}
		}
	}

	if (IsPendingAttach())
	{
		if (m_iPendingParentObjectUniqueId != INVALID_COMPLEX_OBJECT_ID)
		{
			CComplexObjectArrayItem *pParentObject = CScaleformComplexObjectMgr::GetObjectFromUniqueId(m_iPendingParentObjectUniqueId);

			if (sfVerifyf(pParentObject, "CComplexObjectArrayItem::CreateEmptyMovieClipInternal - Parent object with unique id %d not available to attach items to!", m_iPendingParentObjectUniqueId))
			{
				pParentObject->AttachMovieClipInternal(this, m_cPendingString[0].c_str(), m_iPendingDepth, m_cPendingString[1].c_str());
			}
		}
	}

	if (IsPendingGet())
	{
		if (m_iPendingParentObjectUniqueId != INVALID_COMPLEX_OBJECT_ID)
		{
			CComplexObjectArrayItem *pParentObject = CScaleformComplexObjectMgr::GetObjectFromUniqueId(m_iPendingParentObjectUniqueId);

			if (sfVerifyf(pParentObject, "CComplexObjectArrayItem::CreateEmptyMovieClipInternal - Parent object with unique id %d not available to get items on!", m_iPendingParentObjectUniqueId))
			{
				pParentObject->GetObjectInternal(this, m_cPendingString[0].c_str(), m_cPendingString[1].c_str(), m_cPendingString[2].c_str());
			}
		}
	}

	//
	// create:
	//
	if (sfVerifyf(m_asObject.IsDisplayObject(), "CComplexObjectArrayItem::CreateEmptyMovieClipInternal - Parent object has an invalid GfxValue!"))
	{
#if __ASSERT

#if __SCALEFORM_CRITICAL_SECTIONS
		SYS_CS_SYNC(gs_ScaleformMovieCS[GetMovieId()]);
#endif
		if (sfVerifyf(!m_asObject.GFxValue::HasMember(pInstanceName), "CComplexObjectArrayItem::CreateEmptyMovieClipInternal - Trying to create empty movieclip '%s' to object '%s' when it is already attached!", pInstanceName, GetObjectNameHash().GetCStr()))
#endif
		{
			sfAssertf(iDepth < MAX_COMPLEX_OBJECT_DEPTH, "CComplexObjectArrayItem::CreateEmptyMovieClipInternal - Trying to create an empty movie clip object at depth %d when max is %d", iDepth, MAX_COMPLEX_OBJECT_DEPTH);

#if __ASSERT
			//
			// check if there is already an object at this depth:
			//
			GFxValue returnValue;
			GFxValue args[1];

			args[0].SetNumber(iDepth);
			m_asObject.Invoke("getInstanceAtDepth", &returnValue, args, 1);

			if (returnValue.IsDefined())
			{
				GFxValue instanceName;
				m_asObject.GetMember("_name", &instanceName);
				sfAssertf(0, "CComplexObjectArrayItem::CreateEmptyMovieClipInternal - Trying to createEmptyMovieClip %s at depth %d but '%s' object is already on the stage at this depth!", pInstanceName, iDepth, instanceName.GetString());
			}
#endif // __ASSERT

			GFxValue asNewObject;

			if (sfVerifyf(m_asObject.CreateEmptyMovieClip(&asNewObject, pInstanceName, iDepth), "CComplexObjectArrayItem::CreateEmptyMovieClipInternal - Failed to createEmptyMovieClip '%s' to object '%s' - Object possibly not on stage", pInstanceName, GetObjectNameHash().GetCStr()))
			{
				pNewObject->SetObject(asNewObject);

#if __BANK
				if (CScaleformComplexObjectMgr::DetailedLogging())
				{
					sfDisplayf("COMPLEXOBJECT: created EmptyMovieClip object '%s' on movie %s (%d) at depth %d", GetObjectNameHash().GetCStr(), CScaleformMgr::GetMovieFilename(m_iMovieId), m_iMovieId, iDepth);
				}
#endif // __BANK

				// now set it active
				pNewObject->m_uState &= ~CO_STATE_PENDING_CREATE;

				// empty any pending info here as we have now set it up
				pNewObject->m_cPendingString[0] = NULL;
				pNewObject->m_iPendingDepth = 0;
				pNewObject->m_iPendingParentObjectUniqueId = INVALID_COMPLEX_OBJECT_ID;

				pNewObject->m_uState |= CO_STATE_ACTIVE;
				pNewObject->m_uState |= CO_STATE_ATTACHED_BY_CODE;  // flag that this was attached in code (so we can removemovieclip on it when we release this object)

				pNewObject->InitProperties();

				return pNewObject;
			}
		}
	}

	return NULL;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CComplexObjectArrayItem::GetStageRoot
// PURPOSE: gets the root object (stage) so we can add/get objects starting from here
/////////////////////////////////////////////////////////////////////////////////////
CComplexObject CComplexObjectArrayItem::GetStageRoot(s32 iMovieId)
{
#if __SCALEFORM_CRITICAL_SECTIONS
	SYS_CS_SYNC(gs_ScaleformMovieCS[iMovieId]);
#endif

	CComplexObjectArrayItem *pNewObject = CScaleformComplexObjectMgr::FindOrCreateSlot();
	CComplexObject newItem;

	if (pNewObject)
	{
		pNewObject->SetMovieId(iMovieId);
		pNewObject->SetUniqueId();
		pNewObject->SetObjectName("_root");

		for (s32 iBuf = 0; iBuf < MAX_CO_BUFFERS; iBuf++)
		{
			pNewObject->m_properties[iBuf].iActionRequired = 0;
		}

		if (CSystem::IsThisThreadId(SYS_THREAD_RENDER))
		{
			GetStageRootInternal(pNewObject);
		}
		else
		{
			pNewObject->m_uState |= CO_STATE_PENDING_GET_ROOT;
		}

		
		newItem.SetObjectId(pNewObject->GetUniqueId());
		return newItem;
	}

	newItem.SetObjectId(INVALID_COMPLEX_OBJECT_ID);
	return newItem;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CComplexObjectArrayItem::GetStageRootInternal
// PURPOSE: gets the root object (stage) so we can add/get objects starting from here
/////////////////////////////////////////////////////////////////////////////////////
CComplexObjectArrayItem *CComplexObjectArrayItem::GetStageRootInternal(CComplexObjectArrayItem *pNewObject)
{
	pNewObject->SetObject(CScaleformMgr::GetActionScriptObjectFromRoot(pNewObject->GetMovieId()));

#if __BANK
	if (CScaleformComplexObjectMgr::DetailedLogging())
	{
		sfDisplayf("COMPLEXOBJECT: created object '%s' on movie %s (%d)", pNewObject->GetObjectNameHash().GetCStr(), CScaleformMgr::GetMovieFilename(pNewObject->GetMovieId()), pNewObject->GetMovieId());
	}
#endif // __BANK

	// now set it active
	pNewObject->m_uState &= ~CO_STATE_PENDING_GET_ROOT;

	// empty any pending info here as we have now set it up
	pNewObject->m_cPendingString[0] = NULL;
	pNewObject->m_cPendingString[1] = NULL;
	pNewObject->m_iPendingDepth = 0;
	pNewObject->m_iPendingParentObjectUniqueId = INVALID_COMPLEX_OBJECT_ID;

	pNewObject->m_uState |= CO_STATE_ACTIVE;

	pNewObject->InitProperties();

	return pNewObject;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CComplexObjectArrayItem::GotoFrame
// PURPOSE: goto a frame of an object
/////////////////////////////////////////////////////////////////////////////////////
void CComplexObjectArrayItem::GotoFrame(s32 iNewFrame, bool const andPlay)
{
	s32 iThisBuffer = CScaleformComplexObjectMgr::GetBuffer();

	if (CSystem::IsThisThreadId(SYS_THREAD_RENDER))
	{
#if __BANK
		if (CScaleformComplexObjectMgr::DetailedLogging())
		{
			sfDisplayf("COMPLEXOBJECT: GotoAndStop(%d) called on object '%s' on movie %s (%d)", iNewFrame, GetObjectNameHash().GetCStr(), CScaleformMgr::GetMovieFilename(m_iMovieId), m_iMovieId);
		}
#endif // __BANK

		if( andPlay )
		{
			m_asObject.GotoAndPlay(iNewFrame);
		}
		else
		{
			m_asObject.GotoAndStop(iNewFrame);
		}
	}
	else
	{
		// update the position
		m_properties[iThisBuffer].iActionRequired |= CO_ACTION_UPDATE_FRAME;
	}

	m_properties[iThisBuffer].iFrame = iNewFrame;
	m_properties[iThisBuffer].bFrameJumpPlay = andPlay;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CComplexObjectArrayItem::GetCurrentFrame
// PURPOSE: gets the current frame this object is set to
/////////////////////////////////////////////////////////////////////////////////////
s32 CComplexObjectArrayItem::GetCurrentFrame()
{
	return m_properties[CScaleformComplexObjectMgr::GetBuffer()].iFrame;
}


bool CComplexObjectArrayItem::PlayOnFrameJump()
{
	return m_properties[CScaleformComplexObjectMgr::GetBuffer()].bFrameJumpPlay;
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CComplexObjectArrayItem::GetTextField
// PURPOSE: gets any text currently in the text field
/////////////////////////////////////////////////////////////////////////////////////
const char *CComplexObjectArrayItem::GetTextField()
{
	return m_properties[CScaleformComplexObjectMgr::GetBuffer()].cTextfield.c_str();
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CComplexObjectArrayItem::GetTextFieldHTML
// PURPOSE: gets any html text currently in the text field
/////////////////////////////////////////////////////////////////////////////////////
const char * CComplexObjectArrayItem::GetTextFieldHTML()
{
	return m_properties[CScaleformComplexObjectMgr::GetBuffer()].cTextfield.c_str();
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CComplexObjectArrayItem::GetColour
// PURPOSE: gets the current colour of the object
/////////////////////////////////////////////////////////////////////////////////////
Color32 CComplexObjectArrayItem::GetColour()
{
	return m_properties[CScaleformComplexObjectMgr::GetBuffer()].colour;
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CComplexObjectArrayItem::GetColour
// PURPOSE: gets the current colour of the object
/////////////////////////////////////////////////////////////////////////////////////
u8 CComplexObjectArrayItem::GetAlpha()
{
	return m_properties[CScaleformComplexObjectMgr::GetBuffer()].colour.GetAlpha();
}

s32 CComplexObjectArrayItem::GetDepth()
{
	return m_properties[CScaleformComplexObjectMgr::GetBuffer()].depth;
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CComplexObjectArrayItem::SetTextField
// PURPOSE: sets text in the text field on the object
/////////////////////////////////////////////////////////////////////////////////////
void CComplexObjectArrayItem::SetTextField(const char *pText)
{
	s32 iThisBuffer = CScaleformComplexObjectMgr::GetBuffer();

	if (CSystem::IsThisThreadId(SYS_THREAD_RENDER))
	{
		if (sfVerifyf(m_asObject.IsDefined(), "CComplexObjectArrayItem::SetTextField - Object '%s' gfxvalue is undefined!", GetObjectNameHash().GetCStr()))
		{
	#if __BANK
			if (CScaleformComplexObjectMgr::DetailedLogging())
			{
				sfDisplayf("COMPLEXOBJECT: SetText(%s) called on object '%s' on movie %s (%d)", pText, GetObjectNameHash().GetCStr(), CScaleformMgr::GetMovieFilename(m_iMovieId), m_iMovieId);
			}
	#endif // __BANK

			m_asObject.SetText(pText);
		}
	}
	else
	{
		m_properties[iThisBuffer].iActionRequired |= CO_ACTION_UPDATE_TEXTFIELD;
	}

	m_properties[iThisBuffer].cTextfield = pText;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CComplexObjectArrayItem::SetTextFieldHTML
// PURPOSE: sets html text in the text field on the object
/////////////////////////////////////////////////////////////////////////////////////
void CComplexObjectArrayItem::SetTextFieldHTML( const char *pHtmlText )
{
	s32 iThisBuffer = CScaleformComplexObjectMgr::GetBuffer();

	if (CSystem::IsThisThreadId(SYS_THREAD_RENDER))
	{
		if (sfVerifyf(m_asObject.IsDefined(), "CComplexObjectArrayItem::SetTextFieldHTML - Object '%s' gfxvalue is undefined!", GetObjectNameHash().GetCStr()))
		{
	#if __BANK
			if (CScaleformComplexObjectMgr::DetailedLogging())
			{
				sfDisplayf("COMPLEXOBJECT: SetText(%s) called on object '%s' on movie %s (%d)", pHtmlText, GetObjectNameHash().GetCStr(), CScaleformMgr::GetMovieFilename(m_iMovieId), m_iMovieId);
			}
	#endif // __BANK

			m_asObject.SetTextHTML(pHtmlText);
		}
	}
	else
	{
		m_properties[iThisBuffer].iActionRequired |= CO_ACTION_UPDATE_TEXTFIELD_HTML;
	}

	m_properties[iThisBuffer].cTextfield = pHtmlText;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CComplexObjectArrayItem::SetColour
// PURPOSE: sets colour of the object
/////////////////////////////////////////////////////////////////////////////////////
void CComplexObjectArrayItem::SetColour(Color32 const &colour)
{
	s32 iThisBuffer = CScaleformComplexObjectMgr::GetBuffer();

	if (CSystem::IsThisThreadId(SYS_THREAD_RENDER))
	{
		if (sfVerifyf(m_asObject.IsDefined(), "CComplexObjectArrayItem::SetColour - Object '%s' gfxvalue is undefined!", GetObjectNameHash().GetCStr()))
		{
	#if __BANK
			if (CScaleformComplexObjectMgr::DetailedLogging())
			{
				sfDisplayf("COMPLEXOBJECT: SetColour(%d,%d,%d,%d) called on object '%s' on movie %s (%d)", colour.GetRed(), colour.GetGreen(), colour.GetBlue(), colour.GetAlpha(), GetObjectNameHash().GetCStr(), CScaleformMgr::GetMovieFilename(m_iMovieId), m_iMovieId);
			}
	#endif // __BANK

			m_asObject.SetColorTransform(colour);

			GFxValue::DisplayInfo displayInfo;

			displayInfo.SetAlpha(colour.GetAlphaf() * 100.0f);

			m_asObject.SetDisplayInfo(displayInfo);
		}

		m_properties[iThisBuffer].colour.SetAlpha(colour.GetAlpha());  // set the alpha here as on RT the passed colour is the previously set 'SetAlpha' version of the alpha
	}
	else
	{
		m_properties[iThisBuffer].iActionRequired |= CO_ACTION_UPDATE_COLOUR;

		if (m_properties[iThisBuffer].iActionRequired & CO_ACTION_UPDATE_ALPHA)  // no longer need to do alpha as we can do this during the colour
		{
			m_properties[iThisBuffer].iActionRequired &= ~CO_ACTION_UPDATE_ALPHA;
		}
		else
		{
			m_properties[iThisBuffer].colour.SetAlpha(colour.GetAlpha());  // if not set alpha then we use the alpha passed
		}
	}

	m_properties[iThisBuffer].colour.SetRed(colour.GetRed());
	m_properties[iThisBuffer].colour.SetGreen(colour.GetGreen());
	m_properties[iThisBuffer].colour.SetBlue(colour.GetBlue());
	// do not set alpha here from the passed colour as we wish to retain whatever has been passed in as an alpha via SetAlpha
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CComplexObjectArrayItem::SetAlpha
// PURPOSE: sets just the alpha of the object
/////////////////////////////////////////////////////////////////////////////////////
void CComplexObjectArrayItem::SetAlpha( u8 const c_alpha )
{
	s32 const c_thisBuffer = CScaleformComplexObjectMgr::GetBuffer();

	if (CSystem::IsThisThreadId(SYS_THREAD_RENDER))
	{
		if (sfVerifyf(m_asObject.IsDefined(), "CComplexObjectArrayItem::SetAlpha - Object '%s' gfxvalue is undefined!", GetObjectNameHash().GetCStr()))
		{
#if __BANK
			if (CScaleformComplexObjectMgr::DetailedLogging())
			{
				sfDisplayf("COMPLEXOBJECT: SetAlpha(%u) called on object '%s' on movie %s (%d)", c_alpha, GetObjectNameHash().GetCStr(), CScaleformMgr::GetMovieFilename(m_iMovieId), m_iMovieId);
			}
#endif // __BANK

			GFxValue::DisplayInfo displayInfo;

			displayInfo.SetAlpha(float((float)c_alpha / 255.0f) * 100.0f);

			m_asObject.SetDisplayInfo(displayInfo);
		}
	}
	else
	{
		if (m_properties[c_thisBuffer].iActionRequired & CO_ACTION_UPDATE_COLOUR)  // if already pending colour, then adjust the alpha and set the colour again
		{
			m_properties[c_thisBuffer].colour.SetAlpha(c_alpha);
			SetColour(m_properties[c_thisBuffer].colour);
			return;
		}
		else
		{
			m_properties[c_thisBuffer].iActionRequired |= CO_ACTION_UPDATE_ALPHA;
		}
	}

	m_properties[c_thisBuffer].colour.SetAlpha(c_alpha);
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CComplexObjectArrayItem::SetDepth
// PURPOSE: sets depth of the object
/////////////////////////////////////////////////////////////////////////////////////
void CComplexObjectArrayItem::SetDepth( s32 const c_depth )
{
	s32 iThisBuffer = CScaleformComplexObjectMgr::GetBuffer();

	if (CSystem::IsThisThreadId(SYS_THREAD_RENDER))
	{
		if (sfVerifyf(m_asObject.IsDefined(), "CComplexObjectArrayItem::SetDepth - Object '%s' gfxvalue is undefined!", GetObjectNameHash().GetCStr()))
		{
#if __BANK
			if (CScaleformComplexObjectMgr::DetailedLogging())
			{
				sfDisplayf("COMPLEXOBJECT: SetDepth(%d) called on object '%s' on movie %s (%d)", c_depth, GetObjectNameHash().GetCStr(), CScaleformMgr::GetMovieFilename(m_iMovieId), m_iMovieId);
			}
#endif // __BANK

			GFxValue args[1];
			args[0].SetNumber(c_depth);

			GFxValue result;
			m_asObject.Invoke("swapDepths", &result, args, 1);
		}
	}
	else
	{
		m_properties[iThisBuffer].iActionRequired |= CO_ACTION_UPDATE_DEPTH;
	}

	m_properties[iThisBuffer].depth = c_depth;
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CComplexObjectArrayItem::SetWidth
// PURPOSE: sets the width of the object
/////////////////////////////////////////////////////////////////////////////////////
void CComplexObjectArrayItem::SetWidth(float fNewWidth)
{
	s32 iThisBuffer = CScaleformComplexObjectMgr::GetBuffer();

	if (CSystem::IsThisThreadId(SYS_THREAD_RENDER))
	{
		if (sfVerifyf(m_asObject.IsDefined(), "CComplexObjectArrayItem::SetWidth - Object '%s' gfxvalue is undefined!", GetObjectNameHash().GetCStr()))
		{
	#if __BANK
			if (CScaleformComplexObjectMgr::DetailedLogging())
			{
				sfDisplayf("COMPLEXOBJECT: SetWidth(%0.2f) called on object '%s' on movie %s (%d)", fNewWidth, GetObjectNameHash().GetCStr(), CScaleformMgr::GetMovieFilename(m_iMovieId), m_iMovieId);
			}
	#endif // __BANK

	#if __FULL_SANITY_CHECKS
			if (sfVerifyf(m_asObject.HasMember("_width"), "CComplexObjectArrayItem::SetWidth - Cannot find member '_width'"))
	#endif // __FULL_SANITY_CHECKS
			{
				GFxValue widthValue;
				widthValue.SetNumber(fNewWidth);

				m_asObject.SetMember("_width", widthValue);
			}
		}
	}
	else
	{
		// update the position
		m_properties[iThisBuffer].iActionRequired |= CO_ACTION_UPDATE_WIDTH;
	}

	m_properties[iThisBuffer].fWidth = fNewWidth;
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CComplexObjectArrayItem::SetHeight
// PURPOSE: sets the height of the object
/////////////////////////////////////////////////////////////////////////////////////
void CComplexObjectArrayItem::SetHeight( float fHeight )
{
	s32 iThisBuffer = CScaleformComplexObjectMgr::GetBuffer();

	if (CSystem::IsThisThreadId(SYS_THREAD_RENDER))
	{
		if (sfVerifyf(m_asObject.IsDefined(), "CComplexObjectArrayItem::SetHeight - Object '%s' gfxvalue is undefined!", GetObjectNameHash().GetCStr()))
		{
	#if __BANK
			if (CScaleformComplexObjectMgr::DetailedLogging())
			{
				sfDisplayf("COMPLEXOBJECT: SetHeight(%0.2f) called on object '%s' on movie %s (%d)", fHeight, GetObjectNameHash().GetCStr(), CScaleformMgr::GetMovieFilename(m_iMovieId), m_iMovieId);
			}
	#endif // __BANK

	#if __FULL_SANITY_CHECKS
			if (sfVerifyf(m_asObject.HasMember("_height"), "CComplexObjectArrayItem::SetHeight - Cannot find member '_height'"))
	#endif // __FULL_SANITY_CHECKS
			{
				GFxValue heightValue;
				heightValue.SetNumber(fHeight);

				m_asObject.SetMember("_height", heightValue);
			}
		}
	}
	else
	{
		// update the position
		m_properties[iThisBuffer].iActionRequired |= CO_ACTION_UPDATE_HEIGHT;
	}

	m_properties[iThisBuffer].fHeight = fHeight;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CComplexObjectArrayItem::GetWidth
// PURPOSE: returns the width of the object
/////////////////////////////////////////////////////////////////////////////////////
float CComplexObjectArrayItem::GetWidth()
{
	return m_properties[CScaleformComplexObjectMgr::GetBuffer()].fWidth;
}


float CComplexObjectArrayItem::GetHeight()
{
	return m_properties[CScaleformComplexObjectMgr::GetBuffer()].fHeight;
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CComplexObjectArrayItem::SetXScale
// PURPOSE: Sets the X scale using a percentage value
/////////////////////////////////////////////////////////////////////////////////////
void CComplexObjectArrayItem::SetXScale(float fScale)
{
	SetScale(Vector2(fScale, GetScale().y));
}


void CComplexObjectArrayItem::SetRotation( float const fRotation )
{
	s32 iThisBuffer = CScaleformComplexObjectMgr::GetBuffer();

	if (CSystem::IsThisThreadId(SYS_THREAD_RENDER))
	{
		if (sfVerifyf(m_asObject.IsDefined(), "CComplexObjectArrayItem::SetRotation - Object '%s' gfxvalue is undefined!", GetObjectNameHash().GetCStr()))
		{

#if __BANK
			if (CScaleformComplexObjectMgr::DetailedLogging())
			{
				sfDisplayf("COMPLEXOBJECT: SetRotation(%0.2f) called on object '%s' on movie %s (%d)", fRotation, GetObjectNameHash().GetCStr(), CScaleformMgr::GetMovieFilename(m_iMovieId), m_iMovieId);
			}
#endif // __BANK

			GFxValue::DisplayInfo displayInfo;

			displayInfo.SetRotation( fRotation );
			m_asObject.SetDisplayInfo(displayInfo);
		}
	}
	else
	{
		m_properties[iThisBuffer].iActionRequired |= CO_ACTION_UPDATE_ROTATION;
	}

	m_properties[iThisBuffer].fRotation = fRotation;
}

void CComplexObjectArrayItem::SetMember( const char *pInstanceName, bool const c_valueBool)
{
	atString nullString;
	SetMemberInternal(pInstanceName, MEMBER_VALUE_TYPE_BOOL, c_valueBool, 0.0f, nullString);
}

void CComplexObjectArrayItem::SetMember( const char *pInstanceName, float const c_valueNumber)
{
	atString nullString;
	SetMemberInternal(pInstanceName, MEMBER_VALUE_TYPE_BOOL, false, c_valueNumber, nullString);
}

void CComplexObjectArrayItem::SetMember( const char *pInstanceName, char const * const c_valueString)
{
	SetMemberInternal(pInstanceName, MEMBER_VALUE_TYPE_BOOL, false, 0.0f, c_valueString);
}

void CComplexObjectArrayItem::SetMemberInternal(const char *pInstanceName, eMemberValueType const c_type, bool const c_valueBool, float const c_valueNumber, char const * const c_valueString)
{
	s32 iThisBuffer = CScaleformComplexObjectMgr::GetBuffer();

	if (CSystem::IsThisThreadId(SYS_THREAD_RENDER))
	{
		if (sfVerifyf(m_asObject.IsDefined(), "CComplexObjectArrayItem::SetMember - Object '%s' gfxvalue is undefined!", GetObjectNameHash().GetCStr()))
		{

#if __BANK
			if (CScaleformComplexObjectMgr::DetailedLogging())
			{
				sfDisplayf("COMPLEXOBJECT: SetMember called on object '%s' on movie %s (%d)", GetObjectNameHash().GetCStr(), CScaleformMgr::GetMovieFilename(m_iMovieId), m_iMovieId);
			}
#endif // __BANK

			GFxValue gfxValue;

			switch (c_type)
			{
				case MEMBER_VALUE_TYPE_BOOL:
				{
					gfxValue.SetBoolean(c_valueBool);
					break;
				}

				case MEMBER_VALUE_TYPE_NUMBER:
				{
					gfxValue.SetNumber((double)c_valueNumber);
					break;
				}

				case MEMBER_VALUE_TYPE_STRING:
				{
					gfxValue.SetString(c_valueString);
					break;
				}

				default:
				{
					// NOP
				}
			}
			
			m_asObject.SetMember(pInstanceName, gfxValue);
		}
	}
	else
	{
		m_properties[iThisBuffer].iActionRequired |= CO_ACTION_UPDATE_MEMBERS;
	}

	sComplectObjectMembers *newSetMember = NULL;

	const atHashString c_instanceNameHash = atHashString(pInstanceName);

	for (s32 i = 0; i < m_properties[iThisBuffer].m_member.GetCount(); i++)
	{
		const atHashString c_thisInstanceNameHash = atHashString(m_properties[iThisBuffer].m_member[i].m_instanceName);

		if (c_instanceNameHash == c_thisInstanceNameHash)
		{
			newSetMember = &m_properties[iThisBuffer].m_member[i];
			break;
		}
	}

	if (!newSetMember)
	{
		 newSetMember = &(m_properties[iThisBuffer].m_member).Grow();
	}

	newSetMember->m_instanceName = pInstanceName;
	newSetMember->m_update = true;

	switch (c_type)
	{
		case MEMBER_VALUE_TYPE_BOOL:
		{
			newSetMember->value_bool = c_valueBool;
			newSetMember->m_type = MEMBER_VALUE_TYPE_BOOL;
			break;
		}

		case MEMBER_VALUE_TYPE_NUMBER:
		{
			newSetMember->value_number = c_valueNumber;
			newSetMember->m_type = MEMBER_VALUE_TYPE_NUMBER;
			break;
		}

		case MEMBER_VALUE_TYPE_STRING:
		{
			newSetMember->value_string = c_valueString;
			newSetMember->m_type = MEMBER_VALUE_TYPE_STRING;
			break;
		}

		default:
		{
			// NOP
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CComplexObjectArrayItem::SetScale
// PURPOSE: sets the scale based on a percentage
/////////////////////////////////////////////////////////////////////////////////////
void CComplexObjectArrayItem::SetScale(Vector2 const& vNewScale)
{
	s32 iThisBuffer = CScaleformComplexObjectMgr::GetBuffer();

	if (CSystem::IsThisThreadId(SYS_THREAD_RENDER))
	{
		if (sfVerifyf(m_asObject.IsDefined(), "CComplexObjectArrayItem::SetScale - Object '%s' gfxvalue is undefined!", GetObjectNameHash().GetCStr()))
		{

	#if __BANK
			if (CScaleformComplexObjectMgr::DetailedLogging())
			{
				sfDisplayf("COMPLEXOBJECT: SetScale(%0.2f,%0.2f) called on object '%s' on movie %s (%d)", vNewScale.x, vNewScale.y, GetObjectNameHash().GetCStr(), CScaleformMgr::GetMovieFilename(m_iMovieId), m_iMovieId);
			}
	#endif // __BANK

			GFxValue::DisplayInfo displayInfo;

			displayInfo.SetScale(vNewScale.x, vNewScale.y);

			m_asObject.SetDisplayInfo(displayInfo);
		}
	}
	else
	{
		m_properties[iThisBuffer].iActionRequired |= CO_ACTION_UPDATE_SCALE;
	}

	m_properties[iThisBuffer].vScale = vNewScale;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CComplexObjectArrayItem::GetScale
// PURPOSE: returns the current scale value of the object
/////////////////////////////////////////////////////////////////////////////////////
Vector2 CComplexObjectArrayItem::GetScale()
{
	s32 iThisBuffer = CScaleformComplexObjectMgr::GetBuffer();

	return Vector2(m_properties[iThisBuffer].vScale.x, m_properties[iThisBuffer].vScale.y);
}


float CComplexObjectArrayItem::GetRotation()
{
	s32 iThisBuffer = CScaleformComplexObjectMgr::GetBuffer();

	return m_properties[iThisBuffer].fRotation;
}

eMemberValueType CComplexObjectArrayItem::GetMember(const char *pInstanceName, bool &out_valueBool, float &out_valueNumber, atString &out_valueString)
{
	s32 const c_thisBuffer = CScaleformComplexObjectMgr::GetBuffer();
	const atHashString c_instanceNameHash = atHashString(pInstanceName);

	for (s32 i = 0; i < m_properties[c_thisBuffer].m_member.GetCount(); i++)
	{
		const atHashString c_thisInstanceNameHash = atHashString(m_properties[c_thisBuffer].m_member[i].m_instanceName);

		if (c_instanceNameHash == c_thisInstanceNameHash)
		{
			switch (m_properties[c_thisBuffer].m_member[i].m_type)
			{
				case MEMBER_VALUE_TYPE_BOOL:
				{
					out_valueBool = m_properties[c_thisBuffer].m_member[i].value_bool;
					return m_properties[c_thisBuffer].m_member[i].m_type;
				}

				case MEMBER_VALUE_TYPE_NUMBER:
				{
					out_valueNumber = m_properties[c_thisBuffer].m_member[i].value_number;
					return m_properties[c_thisBuffer].m_member[i].m_type;
				}

				case MEMBER_VALUE_TYPE_STRING:
				{
					out_valueString = m_properties[c_thisBuffer].m_member[i].value_string;
					return m_properties[c_thisBuffer].m_member[i].m_type;
				}

				default:
				{
					// NOP
				}
			}

			break;
		}
	}

	return MEMBER_VALUE_TYPE_INVALID;
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CComplexObjectArrayItem::SetPosition
// PURPOSE: sets the pos of an object in 0-1 space.  Does conversion to stage size
//			(assume standard 720p size for GTAV) here, so code can continue to
//			work in 0-1
/////////////////////////////////////////////////////////////////////////////////////
void CComplexObjectArrayItem::SetPosition(Vector2 const& vPos)
{
	SetPositionInPixels(Vector2(vPos.x * ACTIONSCRIPT_STAGE_SIZE_X, vPos.y * ACTIONSCRIPT_STAGE_SIZE_Y));
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CComplexObjectArrayItem::SetPositionInPixels
// PURPOSE: sets the position of the object in pixels relative to the AS stage
/////////////////////////////////////////////////////////////////////////////////////
void CComplexObjectArrayItem::SetPositionInPixels(Vector2 const& vPixelPos)
{
	s32 iThisBuffer = CScaleformComplexObjectMgr::GetBuffer();

	if (CSystem::IsThisThreadId(SYS_THREAD_RENDER))
	{
		if (sfVerifyf(m_asObject.IsDefined(), "CComplexObjectArrayItem::SetPositionInPixels - Object '%s' gfxvalue is undefined!", GetObjectNameHash().GetCStr()))
		{
	#if __BANK
			if (CScaleformComplexObjectMgr::DetailedLogging())
			{
				sfDisplayf("COMPLEXOBJECT: SetPosition(%0.2f,%0.2f) called on object '%s' on movie %s (%d)", vPixelPos.x, vPixelPos.y, GetObjectNameHash().GetCStr(), CScaleformMgr::GetMovieFilename(m_iMovieId), m_iMovieId);
			}
	#endif // __BANK

			GFxValue::DisplayInfo displayInfo;

			displayInfo.SetPosition(vPixelPos.x, vPixelPos.y);

			m_asObject.SetDisplayInfo(displayInfo);
		}
	}
	else
	{
		m_properties[iThisBuffer].iActionRequired |= CO_ACTION_UPDATE_POSITION_X;
		m_properties[iThisBuffer].iActionRequired |= CO_ACTION_UPDATE_POSITION_Y;
	}

	m_properties[iThisBuffer].vPosition.x = vPixelPos.x / ACTIONSCRIPT_STAGE_SIZE_X;
	m_properties[iThisBuffer].vPosition.y = vPixelPos.y / ACTIONSCRIPT_STAGE_SIZE_Y;

}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CComplexObjectArrayItem::GetPosition
// PURPOSE: returns the position of the object on the screen in 0-1 space
//			this is currently relative to its parent, but we probably want to return
//			it in screen space to make this useful
/////////////////////////////////////////////////////////////////////////////////////
Vector2 CComplexObjectArrayItem::GetPosition()
{
	s32 iThisBuffer = CScaleformComplexObjectMgr::GetBuffer();

	return Vector2(m_properties[iThisBuffer].vPosition.x, m_properties[iThisBuffer].vPosition.y);
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CComplexObjectArrayItem::GetPositionInPixels
// PURPOSE: returns the position of the object in pixels relative to its parent
//			object
/////////////////////////////////////////////////////////////////////////////////////
Vector2 CComplexObjectArrayItem::GetPositionInPixels()
{
	return Vector2(m_properties[CScaleformComplexObjectMgr::GetBuffer()].vPosition.x * ACTIONSCRIPT_STAGE_SIZE_X, m_properties[CScaleformComplexObjectMgr::GetBuffer()].vPosition.y * ACTIONSCRIPT_STAGE_SIZE_Y);
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CComplexObjectArrayItem::GetXPositionInPixels
// PURPOSE: returns the X position in pixels relative to its parent object
/////////////////////////////////////////////////////////////////////////////////////
float CComplexObjectArrayItem::GetXPositionInPixels()
{
	return m_properties[CScaleformComplexObjectMgr::GetBuffer()].vPosition.x * ACTIONSCRIPT_STAGE_SIZE_X;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CComplexObjectArrayItem::GetYPositionInPixels
// PURPOSE: returns the Y position in pixels relative to its parent object
/////////////////////////////////////////////////////////////////////////////////////
float CComplexObjectArrayItem::GetYPositionInPixels()
{
	return m_properties[CScaleformComplexObjectMgr::GetBuffer()].vPosition.y * ACTIONSCRIPT_STAGE_SIZE_Y;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CComplexObjectArrayItem::SetXPositionInPixels
// PURPOSE: sets the x position of the object in pixels relative to its parent object
/////////////////////////////////////////////////////////////////////////////////////
void CComplexObjectArrayItem::SetXPositionInPixels(float fPixelPos)
{
	s32 iThisBuffer = CScaleformComplexObjectMgr::GetBuffer();

	if (CSystem::IsThisThreadId(SYS_THREAD_RENDER))
	{
		if (sfVerifyf(m_asObject.IsDefined(), "CComplexObjectArrayItem::SetXPositionInPixels - Object '%s' gfxvalue is undefined!", GetObjectNameHash().GetCStr()))
		{
	#if __BANK
			if (CScaleformComplexObjectMgr::DetailedLogging())
			{
				sfDisplayf("COMPLEXOBJECT: SetXPositionInPixels(%0.2f) called on object '%s' on movie %s (%d)", fPixelPos, GetObjectNameHash().GetCStr(), CScaleformMgr::GetMovieFilename(m_iMovieId), m_iMovieId);
			}
	#endif // __BANK

			GFxValue::DisplayInfo displayInfo;

			displayInfo.SetX( fPixelPos );

			m_asObject.SetDisplayInfo(displayInfo);
		}
	}
	else
	{
		m_properties[iThisBuffer].iActionRequired |= CO_ACTION_UPDATE_POSITION_X;
	}

	m_properties[iThisBuffer].vPosition.x = fPixelPos / ACTIONSCRIPT_STAGE_SIZE_X;
}


void CComplexObjectArrayItem::SetYPositionInPixels( float fPixelPos )
{
	s32 iThisBuffer = CScaleformComplexObjectMgr::GetBuffer();

	if (CSystem::IsThisThreadId(SYS_THREAD_RENDER))
	{
		if (sfVerifyf(m_asObject.IsDefined(), "CComplexObjectArrayItem::SetYPositionInPixels - Object '%s' gfxvalue is undefined!", GetObjectNameHash().GetCStr()))
		{
	#if __BANK
			if (CScaleformComplexObjectMgr::DetailedLogging())
			{
				sfDisplayf("COMPLEXOBJECT: SetYPositionInPixels(%0.2f) called on object '%s' on movie %s (%d)", fPixelPos, GetObjectNameHash().GetCStr(), CScaleformMgr::GetMovieFilename(m_iMovieId), m_iMovieId);
			}
	#endif // __BANK

			GFxValue::DisplayInfo displayInfo;

			displayInfo.SetY( fPixelPos );

			m_asObject.SetDisplayInfo(displayInfo);
		}
	}
	else
	{
		m_properties[iThisBuffer].iActionRequired |= CO_ACTION_UPDATE_POSITION_Y;
	}

	m_properties[iThisBuffer].vPosition.y = fPixelPos / ACTIONSCRIPT_STAGE_SIZE_Y;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CComplexObjectArrayItem::AdjustXPositionInPixels
// PURPOSE: increments/decrements the current position by X amount of pixels
/////////////////////////////////////////////////////////////////////////////////////
void CComplexObjectArrayItem::AdjustXPositionInPixels(float fPixels)
{
	if (fPixels == 0.0f)  // if we are trying to adjust by 0.0f then lets not bother!
		return;

	s32 iThisBuffer = CScaleformComplexObjectMgr::GetBuffer();

	if (CSystem::IsThisThreadId(SYS_THREAD_RENDER))
	{
		if (sfVerifyf(m_asObject.IsDefined(), "CComplexObjectArrayItem::AdjustXPositionInPixels - Object '%s' gfxvalue is undefined!", GetObjectNameHash().GetCStr()))
		{
			GFxValue::DisplayInfo displayInfo;
			m_asObject.GetDisplayInfo(&displayInfo);

	#if __BANK
			if (CScaleformComplexObjectMgr::DetailedLogging())
			{
				sfDisplayf("COMPLEXOBJECT: Adjusting X position from %0.2f by %0.2f resulting in %0.02f called on object '%s' on movie %s (%d)", displayInfo.GetX(), fPixels, displayInfo.GetX()+fPixels, GetObjectNameHash().GetCStr(), CScaleformMgr::GetMovieFilename(m_iMovieId), m_iMovieId);
			}
	#endif // __BANK

			displayInfo.SetX(displayInfo.GetX()+fPixels);

			m_asObject.SetDisplayInfo(displayInfo);
		}
	}
	else
	{
		m_properties[iThisBuffer].iActionRequired |= CO_ACTION_UPDATE_POSITION_X;
	}

	m_properties[iThisBuffer].vPosition.x += (fPixels / ACTIONSCRIPT_STAGE_SIZE_X);
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CComplexObjectArrayItem::SetVisible
// PURPOSE: sets the object to be visible on the stage
/////////////////////////////////////////////////////////////////////////////////////
void CComplexObjectArrayItem::SetVisible(bool bValue)
{
	s32 iThisBuffer = CScaleformComplexObjectMgr::GetBuffer();

	if (CSystem::IsThisThreadId(SYS_THREAD_RENDER))
	{
		if (sfVerifyf(m_asObject.IsDefined(), "CComplexObjectArrayItem::SetVisible - Object '%s' gfxvalue is undefined!", GetObjectNameHash().GetCStr()))
		{
	#if __BANK
			if (CScaleformComplexObjectMgr::DetailedLogging())
			{
				sfDisplayf("COMPLEXOBJECT: SetVisible(%s) called on object '%s' on movie %s (%d)", (bValue ? "TRUE" : "FALSE"), GetObjectNameHash().GetCStr(), CScaleformMgr::GetMovieFilename(m_iMovieId), m_iMovieId);
			}
	#endif // __BANK

			GFxValue::DisplayInfo displayInfo;
			displayInfo.SetVisible(bValue);
			m_asObject.SetDisplayInfo(displayInfo);
		}
	}
	else
	{
#if __BANK
		if (CScaleformComplexObjectMgr::DetailedLogging())
		{
			sfDisplayf("COMPLEXOBJECT: Wants to set visible to object '%s' on %s on movie %s (%d)", (bValue ? "TRUE" : "FALSE"), GetObjectNameHash().GetCStr(), CScaleformMgr::GetMovieFilename(m_iMovieId), m_iMovieId);
		}
#endif
		m_properties[iThisBuffer].iActionRequired |= CO_ACTION_UPDATE_VISIBILITY;
	}

	m_properties[iThisBuffer].bVisible = bValue;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CComplexObjectArrayItem::IsVisible
// PURPOSE: is this object currently visible on the stage?
/////////////////////////////////////////////////////////////////////////////////////
bool CComplexObjectArrayItem::IsVisible()
{
	return m_properties[CScaleformComplexObjectMgr::GetBuffer()].bVisible;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CComplexObjectArrayItem::GetActionRequired
// PURPOSE: gets what action is required so we can process each object
/////////////////////////////////////////////////////////////////////////////////////
u32 CComplexObjectArrayItem::GetActionRequired(s32 iBuf)
{
	if (iBuf == -1)
	{
		return m_properties[CScaleformComplexObjectMgr::GetBuffer()].iActionRequired;
	}
	else
	{
		return m_properties[iBuf].iActionRequired;
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CComplexObjectArrayItem::CheckAndSetupIfUninitialised
// PURPOSE: checks if uninitialised, and if so lock and initialise this object
//			and any parents it may have on the update thread
/////////////////////////////////////////////////////////////////////////////////////
void CScaleformComplexObjectMgr::CheckAndSetupIfUninitialised(COMPLEX_OBJECT_ID objectId)
{
	const s32 c_iIndex = (s32)objectId;

	if (sfVerifyf(c_iIndex >= 0 && c_iIndex < sm_ObjectArray.GetCount(), "CScaleformComplexObjectMgr::CheckAndSetupIfUninitialised - Object '%d' outside of sm_ObjectArray", c_iIndex))
	{
		if ( (!sm_ObjectArray[c_iIndex].IsInitialised()) && (!CSystem::IsThisThreadId(SYS_THREAD_RENDER)) )  // if uninitialised, and called on the update thread, then set it up now
		{
			CScaleformComplexObjectMgr::AttachOrGetObjectOnStage(objectId);
		}
	}
}



void CComplexObjectArrayItem::SetObjectName(const char *pObjectName)
{
	m_ObjectNameHash = atHashWithStringBank(pObjectName);
}

void CComplexObjectArrayItem::SetMovieId(s32 const iNewMovieId)
{
    sfFatalAssertf( CScaleformMgr::IsMovieIdInRange( iNewMovieId ), "CComplexObjectArrayItem::SetMovieId - Attaching to invalid movie %d", iNewMovieId );
     m_iMovieId = iNewMovieId;
}

void CScaleformComplexObjectMgr::Init()
{
	sm_ObjectArray.Reset();

#if RSG_BANK
	ms_bUseDetailedLogging = PARAM_complexObjectDetailedLogging.Get();
#endif // RSG_BANK
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformComplexObjectMgr::FindObjectIndex
// PURPOSE: returns the index in the array of the passed object
/////////////////////////////////////////////////////////////////////////////////////
s32 CScaleformComplexObjectMgr::FindObjectIndex(CComplexObjectArrayItem *pObject)
{
	for (s32 i = 0; i < sm_ObjectArray.GetCount(); i++)
	{
		if (sm_ObjectArray[i].GetUniqueId() == pObject->GetUniqueId())
		{
			return i;
		}
	}

	sfAssertf(0, "CScaleformComplexObjectMgr::FindObjectIndex: Could not find index from the object!");

	return -1;
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformComplexObjectMgr::FindObjectIndex
// PURPOSE: returns the object with the passed unique ID
/////////////////////////////////////////////////////////////////////////////////////
CComplexObjectArrayItem const * CScaleformComplexObjectMgr::FindObjectFromArrayIndexConst( COMPLEX_OBJECT_ID const objectId )
{
	if (objectId != INVALID_COMPLEX_OBJECT_ID)
	{
		if (sfVerifyf(objectId >= 0 && objectId < sm_ObjectArray.GetCount(), "CScaleformComplexObjectMgr::FindObjectFromArrayIndex - Object '%d' outside of sm_ObjectArray", objectId))
		{
			return &sm_ObjectArray[objectId];
		}
	}

	return NULL;
}
	


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformComplexObjectMgr::FindNumberOfObjectsInUseByMovie
// PURPOSE: returns the number of objects that is currently set up under the passed
//			movie id.  This allows us to detect left overs when a movie gets deleted
/////////////////////////////////////////////////////////////////////////////////////
s32 CScaleformComplexObjectMgr::FindNumberOfObjectsInUseByMovie(s32 iScaleformMovieId)
{
	s32 iCount = 0;

	for (s32 i = 0; i < sm_ObjectArray.GetCount(); i++)
	{
		if (sm_ObjectArray[i].GetMovieId() == iScaleformMovieId)
		{
			if (sm_ObjectArray[i].IsActive() && !sm_ObjectArray[i].IsPendingRelease() && !sm_ObjectArray[i].IsPendingFlaggedForRelease())
			{
				iCount++;
			}
		}
	}

	return iCount;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformComplexObjectMgr::AttachOrGetObjectOnStage
// PURPOSE: attaches or gets an object on the stage
/////////////////////////////////////////////////////////////////////////////////////
void CScaleformComplexObjectMgr::AttachOrGetObjectOnStage(COMPLEX_OBJECT_ID objectId)
{
#if __SCALEFORM_CRITICAL_SECTIONS
	SYS_CS_SYNC(gs_ScaleformMovieCS[sm_ObjectArray[objectId].GetMovieId()]);
#endif

	if (sfVerifyf(objectId >= 0 && objectId < sm_ObjectArray.GetCount(), "CScaleformComplexObjectMgr::AttachOrGetObjectOnStage - Object '%d' outside of sm_ObjectArray", objectId))
	{
		//
		// get any roots we need
		//
		if (sm_ObjectArray[objectId].IsPendingGetRoot())
		{
			CComplexObjectArrayItem::GetStageRootInternal(&sm_ObjectArray[objectId]);
		}
		else if (sm_ObjectArray[objectId].IsPendingCreate())
		{
			CComplexObjectArrayItem *pParentObject = CScaleformComplexObjectMgr::GetObjectFromUniqueId(sm_ObjectArray[objectId].m_iPendingParentObjectUniqueId);

			if (sfVerifyf(pParentObject, "CComplexObjectArrayItem::PerformAllOutstandingActionsOnRT - Parent object with unique id %d not available to createEmptyMovieClip items to!", sm_ObjectArray[objectId].m_iPendingParentObjectUniqueId))
			{
				pParentObject->CreateEmptyMovieClipInternal(&sm_ObjectArray[objectId], sm_ObjectArray[objectId].m_iPendingDepth, sm_ObjectArray[objectId].m_cPendingString[0].c_str());

				// if we are creating this early, then set visibility to false as we dont want to show it until its been fully set up
				if (sm_ObjectArray[objectId].m_asObject.IsDefined())
				{
					sm_ObjectArray[objectId].m_bInitialised = true;

					GFxValue::DisplayInfo displayInfo;
					displayInfo.SetVisible(false);
					sm_ObjectArray[objectId].m_asObject.SetDisplayInfo(displayInfo);
					sm_ObjectArray[objectId].m_bInitialisedEarly = true;
				}
			}
		}
		else if (sm_ObjectArray[objectId].IsPendingAttach())
		{
			CComplexObjectArrayItem *pParentObject = CScaleformComplexObjectMgr::GetObjectFromUniqueId(sm_ObjectArray[objectId].m_iPendingParentObjectUniqueId);

			if (sfVerifyf(pParentObject, "CComplexObjectArrayItem::PerformAllOutstandingActionsOnRT - Parent object with unique id %d not available to attach items to!", sm_ObjectArray[objectId].m_iPendingParentObjectUniqueId))
			{
				pParentObject->AttachMovieClipInternal(&sm_ObjectArray[objectId], sm_ObjectArray[objectId].m_cPendingString[0].c_str(), sm_ObjectArray[objectId].m_iPendingDepth, sm_ObjectArray[objectId].m_cPendingString[1].c_str());

				// if we are attaching this early, then set visibility to false as we dont want to show it until its been fully set up
				if (sm_ObjectArray[objectId].m_asObject.IsDefined())
				{
					sm_ObjectArray[objectId].m_bInitialised = true;

					GFxValue::DisplayInfo displayInfo;
					displayInfo.SetVisible(false);
					sm_ObjectArray[objectId].m_asObject.SetDisplayInfo(displayInfo);
					sm_ObjectArray[objectId].m_bInitialisedEarly = true;
				}
			}
		}
		else if (sm_ObjectArray[objectId].IsPendingGet())
		{
			CComplexObjectArrayItem *pParentObject = CScaleformComplexObjectMgr::GetObjectFromUniqueId(sm_ObjectArray[objectId].m_iPendingParentObjectUniqueId);

			if (sfVerifyf(pParentObject, "CComplexObjectArrayItem::PerformAllOutstandingActionsOnRT - Parent object with unique id %d not available to get items on!", sm_ObjectArray[objectId].m_iPendingParentObjectUniqueId))
			{
				pParentObject->GetObjectInternal(&sm_ObjectArray[objectId], sm_ObjectArray[objectId].m_cPendingString[0].c_str(), sm_ObjectArray[objectId].m_cPendingString[1].c_str(), sm_ObjectArray[objectId].m_cPendingString[2].c_str());

				// if we are getting this early, then set visibility to false as we dont want to show it until its been fully set up
				if (sm_ObjectArray[objectId].m_asObject.IsDefined())
				{
					sm_ObjectArray[objectId].m_bInitialised = true;

					GFxValue::DisplayInfo displayInfo;
					displayInfo.SetVisible(false);
					sm_ObjectArray[objectId].m_asObject.SetDisplayInfo(displayInfo);
					sm_ObjectArray[objectId].m_bInitialisedEarly = true;
				}
			}
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformComplexObjectMgr::PerformAllOutstandingActionsOnRT
// PURPOSE: goes though each object and if we have actions that have been set on the
//			UT, we action these here on the RT
/////////////////////////////////////////////////////////////////////////////////////
void CScaleformComplexObjectMgr::PerformAllOutstandingActionsOnRT()
{
    s32 const c_buffer = GetBuffer();
    sfAssertf( c_buffer == CO_BUFFER_RENDER, "Processing actions on wrong thread" );

    //
    // if this was initialised early, then turn the visiblility back on now
    //
    for (s32 i = 0; i < sm_ObjectArray.GetCount(); i++)
    {
        CComplexObjectArrayItem& objRef = sm_ObjectArray[i];

#if __SCALEFORM_CRITICAL_SECTIONS
        int const c_movieId = objRef.GetMovieId();
        SYS_CS_SYNC(gs_ScaleformMovieCS[ c_movieId ]);
#endif

        if ( objRef.m_bInitialised && objRef.m_bInitialisedEarly && !objRef.IsPendingInvoke())
        {
            if ( objRef.IsVisible())  // only set it to true if the object has been set to be visible otherwise simply keep it invisible
            {
                GFxValue::DisplayInfo displayInfo;
                displayInfo.SetVisible(true);
                sm_ObjectArray[i].m_asObject.SetDisplayInfo(displayInfo);
            }

            objRef.m_bInitialisedEarly = false;
        }
    }

    //
    // set depth of any items already on the stage 1st before we attach anything new (so we dont get new items on old items depths before they are updated)
    //
    for (s32 i = 0; i < sm_ObjectArray.GetCount(); i++)
    {
#if __SCALEFORM_CRITICAL_SECTIONS
        SYS_CS_SYNC(gs_ScaleformMovieCS[sm_ObjectArray[i].GetMovieId()]);
#endif
        CComplexObjectArrayItem& objRef = sm_ObjectArray[ i ];

        if( objRef.IsActive() || objRef.IsPendingFlaggedForRelease() )
        {
            if( objRef.GetActionRequired() != 0 )
            {
                if (objRef.m_asObject.IsDefined())
                {
                    if (objRef.GetActionRequired() & CO_ACTION_UPDATE_DEPTH)
                    {
                        objRef.SetDepth(objRef.GetDepth());

                        objRef.m_properties[c_buffer ].iActionRequired &= ~CO_ACTION_UPDATE_DEPTH;
                    }
                }
            }
        }
    }

    //
    // get any roots we need
    //
    for (s32 i = 0; i < sm_ObjectArray.GetCount(); i++)
    {
#if __SCALEFORM_CRITICAL_SECTIONS
        SYS_CS_SYNC(gs_ScaleformMovieCS[sm_ObjectArray[i].GetMovieId()]);
#endif
        CComplexObjectArrayItem& objRef = sm_ObjectArray[i];

        if ( objRef.IsPendingGetRoot())
        {
            CComplexObjectArrayItem::GetStageRootInternal(&objRef );
        }
    }

    //
    //  create the object on the stage
    //
    for (s32 i = 0; i < sm_ObjectArray.GetCount(); i++)
    {
#if __SCALEFORM_CRITICAL_SECTIONS
        SYS_CS_SYNC(gs_ScaleformMovieCS[sm_ObjectArray[i].GetMovieId()]);
#endif
        CComplexObjectArrayItem& objRef = sm_ObjectArray[i];

        if ( objRef.IsPendingCreate())
        {
            CComplexObjectArrayItem *pParentObject = CScaleformComplexObjectMgr::GetObjectFromUniqueId( objRef.m_iPendingParentObjectUniqueId);

            if (sfVerifyf(pParentObject, "CComplexObjectArrayItem::PerformAllOutstandingActionsOnRT - Parent object with unique id %d not available to attach items to!", sm_ObjectArray[i].m_iPendingParentObjectUniqueId))
            {
                pParentObject->CreateEmptyMovieClipInternal(&objRef, objRef.m_iPendingDepth, objRef.m_cPendingString[0].c_str());

                // if we are creating this early, then set visibility to false as we dont want to show it until its been fully set up
                if (sm_ObjectArray[i].m_asObject.IsDefined())
                {
                    GFxValue::DisplayInfo displayInfo;
                    displayInfo.SetVisible(false);
                    objRef.m_asObject.SetDisplayInfo(displayInfo);
                    objRef.m_bInitialisedEarly = true;
                }
            }
        }
    }


    //
    // attach the object to the stage
    //
    for (s32 i = 0; i < sm_ObjectArray.GetCount(); i++)
    {
#if __SCALEFORM_CRITICAL_SECTIONS
        SYS_CS_SYNC(gs_ScaleformMovieCS[sm_ObjectArray[i].GetMovieId()]);
#endif
        CComplexObjectArrayItem& objRef = sm_ObjectArray[i];

        if ( objRef.IsPendingAttach())
        {
            CComplexObjectArrayItem *pParentObject = CScaleformComplexObjectMgr::GetObjectFromUniqueId( objRef.m_iPendingParentObjectUniqueId);

            if (sfVerifyf(pParentObject, "CComplexObjectArrayItem::PerformAllOutstandingActionsOnRT - Parent object with unique id %d not available to attach items to!", sm_ObjectArray[i].m_iPendingParentObjectUniqueId))
            {
                pParentObject->AttachMovieClipInternal(&objRef, objRef.m_cPendingString[0].c_str(), objRef.m_iPendingDepth, objRef.m_cPendingString[1].c_str());

                // if we are creating this early, then set visibility to false as we dont want to show it until its been fully set up
                if ( objRef.m_asObject.IsDefined())
                {
                    GFxValue::DisplayInfo displayInfo;
                    displayInfo.SetVisible(false);
                    objRef.m_asObject.SetDisplayInfo(displayInfo);
                    objRef.m_bInitialisedEarly = true;
                }
            }
        }
    }


    //
    // get the object on the stage
    //
    for (s32 i = 0; i < sm_ObjectArray.GetCount(); i++)
    {
#if __SCALEFORM_CRITICAL_SECTIONS
        SYS_CS_SYNC(gs_ScaleformMovieCS[sm_ObjectArray[i].GetMovieId()]);
#endif
        CComplexObjectArrayItem& objRef = sm_ObjectArray[i];

        if ( objRef.IsPendingGet())
        {
            if ( objRef.m_iPendingParentObjectUniqueId != INVALID_COMPLEX_OBJECT_ID)
            {
                CComplexObjectArrayItem *pParentObject = CScaleformComplexObjectMgr::GetObjectFromUniqueId( objRef.m_iPendingParentObjectUniqueId);

                if (sfVerifyf(pParentObject, "CComplexObjectArrayItem::PerformAllOutstandingActionsOnRT - Parent object with unique id %d not available to get items on!", sm_ObjectArray[i].m_iPendingParentObjectUniqueId))
                {
                    pParentObject->GetObjectInternal(&objRef, objRef.m_cPendingString[0].c_str(), objRef.m_cPendingString[1].c_str(), objRef.m_cPendingString[2].c_str());
                }
            }
        }
    }

    //
    // at this point, anything that needed setting up will be done, so we are free to set any properties on the object:
    //
    for (s32 i = 0; i < sm_ObjectArray.GetCount(); i++)
    {
#if __SCALEFORM_CRITICAL_SECTIONS
        SYS_CS_SYNC(gs_ScaleformMovieCS[sm_ObjectArray[i].GetMovieId()]);
#endif
        CComplexObjectArrayItem& objRef = sm_ObjectArray[ i ];

        if( objRef.IsActive() || objRef.IsPendingFlaggedForRelease() )
        {
            if( objRef.GetActionRequired() != 0 )
            {
                if (objRef.m_asObject.IsDefined())
                {
                    if (objRef.GetActionRequired() & CO_ACTION_UPDATE_POSITION_X)
                    {
                        objRef.SetXPositionInPixels(objRef.GetXPositionInPixels());
                    }

                    if (objRef.GetActionRequired() & CO_ACTION_UPDATE_POSITION_Y)
                    {
                        objRef.SetYPositionInPixels(objRef.GetYPositionInPixels());
                    }

                    if (objRef.GetActionRequired() & CO_ACTION_UPDATE_WIDTH)
                    {
                        objRef.SetWidth(objRef.GetWidth());
                    }

                    if (objRef.GetActionRequired() & CO_ACTION_UPDATE_HEIGHT)
                    {
                        objRef.SetHeight(objRef.GetHeight());
                    }

                    if (objRef.GetActionRequired() & CO_ACTION_UPDATE_VISIBILITY)
                    {
                        objRef.SetVisible(objRef.IsVisible());
                    }

                    if (objRef.GetActionRequired() & CO_ACTION_UPDATE_FRAME)
                    {
                        objRef.GotoFrame(objRef.GetCurrentFrame(), objRef.PlayOnFrameJump() );
                    }

                    if (objRef.GetActionRequired() & CO_ACTION_UPDATE_TEXTFIELD)
                    {
                        objRef.SetTextField(objRef.GetTextField());
                    }

                    if (objRef.GetActionRequired() & CO_ACTION_UPDATE_TEXTFIELD_HTML)
                    {
                        objRef.SetTextFieldHTML(objRef.GetTextFieldHTML());
                    }

                    if (objRef.GetActionRequired() & CO_ACTION_UPDATE_SCALE)
                    {
                        objRef.SetScale(objRef.GetScale());
                    }

                    if (objRef.GetActionRequired() & CO_ACTION_UPDATE_ROTATION)
                    {
                        objRef.SetRotation(objRef.GetRotation());
                    }

                    if (objRef.GetActionRequired() & CO_ACTION_UPDATE_MEMBERS)
                    {
                        for (s32 i = 0; i < objRef.m_properties->m_member.GetCount(); i++)
                        {
                            if (objRef.m_properties->m_member[i].m_update)
                            {
                                if (objRef.m_properties->m_member[i].m_type == MEMBER_VALUE_TYPE_BOOL)
                                {
                                    objRef.SetMember(objRef.m_properties->m_member[i].m_instanceName, objRef.m_properties->m_member[i].value_bool);
                                }

                                if (objRef.m_properties->m_member[i].m_type == MEMBER_VALUE_TYPE_NUMBER)
                                {
                                    objRef.SetMember(objRef.m_properties->m_member[i].m_instanceName, objRef.m_properties->m_member[i].value_number);
                                }

                                if (objRef.m_properties->m_member[i].m_type == MEMBER_VALUE_TYPE_STRING)
                                {
                                    objRef.SetMember(objRef.m_properties->m_member[i].m_instanceName, objRef.m_properties->m_member[i].value_string);
                                }

                                objRef.m_properties->m_member[i].m_update = false;
                            }
                        }
                    }

                    if (objRef.GetActionRequired() & CO_ACTION_UPDATE_COLOUR)
                    {
                        objRef.SetColour(objRef.GetColour());
                    }

                    if (objRef.GetActionRequired() & CO_ACTION_UPDATE_ALPHA)
                    {
                        objRef.SetAlpha(objRef.GetAlpha());
                    }

                    objRef.m_properties[c_buffer].iActionRequired = 0;  // reset action flag once done
                }
            }
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformComplexObjectMgr::ReleaseAnyFlaggedObjects
// PURPOSE: goes though each object and if we have flagged it for removal, then do it
/////////////////////////////////////////////////////////////////////////////////////
void CScaleformComplexObjectMgr::ReleaseAnyFlaggedObjects()
{
	//
	// finally release anything we dont want anymore - only aslong as there are no pending invokes
	//
	for (s32 i = 0; i < sm_ObjectArray.GetCount(); i++)
	{
#if __SCALEFORM_CRITICAL_SECTIONS
		SYS_CS_SYNC(gs_ScaleformMovieCS[sm_ObjectArray[i].GetMovieId()]);
#endif

		if (sm_ObjectArray[i].IsPendingRelease())
		{
			CComplexObjectArrayItem::ReleaseObjectInternal(i, false);
			continue;
		}

		if ( (sm_ObjectArray[i].GetActionRequired()) == 0 && (sm_ObjectArray[i].IsPendingFlaggedForRelease()) && (!sm_ObjectArray[i].IsPendingInvoke()) )
		{
			sm_ObjectArray[i].m_uState |= CO_STATE_PENDING_RELEASE;
		}
	}
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformComplexObjectMgr::ForceRemoveAllObjectsInUseByMovie
// PURPOSE: goes though each object and remove it regardless of whether it was
//			flagged for removal
/////////////////////////////////////////////////////////////////////////////////////
void CScaleformComplexObjectMgr::ForceRemoveAllObjectsInUseByMovie(s32 const iScaleformMovieId)
{
	//
	// finally release anything we dont want anymore - only aslong as there are no pending invokes
	//
	for (s32 i = 0; i < sm_ObjectArray.GetCount(); i++)
	{
#if __SCALEFORM_CRITICAL_SECTIONS
		SYS_CS_SYNC(gs_ScaleformMovieCS[sm_ObjectArray[i].GetMovieId()]);
#endif
		if (sm_ObjectArray[i].GetMovieId() == iScaleformMovieId)
		{
			CComplexObjectArrayItem::ReleaseObjectInternal(i, true);
		}
	}
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformComplexObjectMgr::ReleaseAllObjectsInUseByMovie
// PURPOSE: goes though each object and if we have flagged it for removal, then do it
/////////////////////////////////////////////////////////////////////////////////////
void CScaleformComplexObjectMgr::ReleaseAllObjectsInUseByMovie(s32 const iScaleformMovieId)
{
	//
	// flag all objects used by this movie for removal
	//
	for (s32 i = 0; i < sm_ObjectArray.GetCount(); i++)
	{
		if (sm_ObjectArray[i].GetMovieId() == iScaleformMovieId)
		{
			CComplexObjectArrayItem *pObj = CScaleformComplexObjectMgr::FindObjectFromArrayIndex(i);

			if (pObj)
			{
				CComplexObjectArrayItem::ReleaseObject(pObj, false);
			}
		}
	}
}





/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformComplexObjectMgr::SyncBuffers
// PURPOSE: flips the buffers and syncs them
/////////////////////////////////////////////////////////////////////////////////////
void CScaleformComplexObjectMgr::SyncBuffers()
{
    // copy contents of UT buffer to RT buffer and reset UT actions
    s32 const c_count = sm_ObjectArray.GetCount();
    for( s32 i = 0; i < c_count; ++i )
    {
        sComplexObjectProperties& rtProps = sm_ObjectArray[i].m_properties[ CO_BUFFER_RENDER ];
        sComplexObjectProperties& utProps = sm_ObjectArray[i].m_properties[ CO_BUFFER_UPDATE ];

        // Avoid an unecessary copy of properties if nothing has changed
        if( utProps.iActionRequired != 0 )
        {
            // If someone calls SyncBuffers twice before we process actions, we need to ensure
            // we don't lose any buffered flags, so calculate the new set
            s32 const c_newFlags = rtProps.iActionRequired | utProps.iActionRequired;

            rtProps = utProps;

            // Reset flags on the UT and assign the coalesced flags for RT processing
            utProps.iActionRequired = 0;
            rtProps.iActionRequired = c_newFlags;
        }
    }
}



CComplexObjectArrayItem *CScaleformComplexObjectMgr::GetObjectFromUniqueId(COMPLEX_OBJECT_ID objectId)
{
	if (objectId != INVALID_COMPLEX_OBJECT_ID)
	{
		for (s32 i = 0; i < sm_ObjectArray.GetCount(); i++)
		{
			if (sm_ObjectArray[i].GetUniqueId() == objectId)
			{
				return (&sm_ObjectArray[i]);
			}
		}
	}

	return NULL;
}

void CScaleformComplexObjectMgr::GetGfxValueFromUniqueIdForInvoke( COMPLEX_OBJECT_ID const objectId, GFxValue *& out_object, bool& out_canInvoke )
{
    out_object = nullptr;
    out_canInvoke = false;

    if (objectId != INVALID_COMPLEX_OBJECT_ID)
    {
        CComplexObjectArrayItem * const pObject = GetObjectFromUniqueId( objectId );
        out_canInvoke = pObject && pObject->CanInvoke();

        if( out_canInvoke ) // If we can't invoke, return null
        {
            out_object = pObject->GetGfxValue();
        }
    }
}

#if __BANK
/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformComplexObjectMgr::OutputMovieClipsInUseByMovieToLog
// PURPOSE: outputs a list of objects in use by the passed movie id to the log for
//          debugging purposes.
/////////////////////////////////////////////////////////////////////////////////////
void CScaleformComplexObjectMgr::OutputMovieClipsInUseByMovieToLog(s32 iScaleformMovieId)
{
	s32 iCount = 0;

	sfDisplayf("COMPLEXOBJECT: Current movieclips active on movie on movie %s (%d):", CScaleformMgr::GetMovieFilename(iScaleformMovieId), iScaleformMovieId);

	for (s32 i = 0; i < sm_ObjectArray.GetCount(); i++)
	{
		if (sm_ObjectArray[i].GetMovieId() == iScaleformMovieId)
		{
			const char *c_namehash = sm_ObjectArray[i].GetObjectNameHash().GetCStr();

			if (c_namehash)
			{
				sfDisplayf("               (%d) %s (state=%u)", iCount, sm_ObjectArray[i].GetObjectNameHash().GetCStr(), sm_ObjectArray[i].m_uState);
			}

			iCount++;
		}
	}

	sfDisplayf("COMPLEXOBJECT: (end)");
}
#endif // __BANK

// eof
