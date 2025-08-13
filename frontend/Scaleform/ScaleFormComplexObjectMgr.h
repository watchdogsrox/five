/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : ScaleformComplexObjectMgr.h
// PURPOSE : manages the processing of use of Complex Object in Scaleform
//			 and linking directly to movies
//			 keeps complex object code to simple, one line calls
// AUTHOR  : Derek Payne
// STARTED : 28/02/2014
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef _SCALEFORM_COMPLEX_OBJECT_MGR_H_
#define _SCALEFORM_COMPLEX_OBJECT_MGR_H_

// Rage headers
#include "atl/array.h"
#include "atl/hashstring.h"
#include "atl/string.h"
#include "Scaleform/scaleform.h"
#include "system/criticalsection.h"
#include "system/system.h"
#include "vector/color32.h"

// framework
#include "fwlocalisation/templateString.h"

#define INVALID_COMPLEX_OBJECT_ID (-1)
typedef s32 COMPLEX_OBJECT_ID;

enum
{
	CO_BUFFER_UPDATE = 0,	// update thread buffer
	CO_BUFFER_RENDER,		// render thread buffer
	MAX_CO_BUFFERS			// max buffers
};

#define CO_ACTION_UPDATE_POSITION_X		(1<<1)
#define CO_ACTION_UPDATE_POSITION_Y		(1<<2)
#define CO_ACTION_UPDATE_WIDTH			(1<<3)
#define CO_ACTION_UPDATE_HEIGHT			(1<<4)
#define CO_ACTION_UPDATE_VISIBILITY		(1<<5)
#define CO_ACTION_UPDATE_FRAME			(1<<6)
#define CO_ACTION_UPDATE_TEXTFIELD		(1<<7)
#define CO_ACTION_UPDATE_TEXTFIELD_HTML	(1<<8)
#define CO_ACTION_UPDATE_SCALE			(1<<9)
#define CO_ACTION_UPDATE_COLOUR			(1<<10)
#define CO_ACTION_UPDATE_ALPHA			(1<<11)
#define CO_ACTION_UPDATE_DEPTH			(1<<12)
#define CO_ACTION_UPDATE_ROTATION		(1<<13)
#define CO_ACTION_UPDATE_MEMBERS		(1<<14)

enum eMemberValueType
{
	MEMBER_VALUE_TYPE_INVALID = 0,
	MEMBER_VALUE_TYPE_NUMBER,
	MEMBER_VALUE_TYPE_BOOL,
	MEMBER_VALUE_TYPE_STRING
};

struct sComplectObjectMembers
{
    ConstString m_instanceName;
    ConstString value_string;

	union
	{
		bool value_bool;
		float value_number;
	};

    eMemberValueType m_type;
    bool m_update;
};

struct sComplexObjectProperties final
{
    atArray<sComplectObjectMembers> m_member;
    ConstString cTextfield;
    Vector2	    vPosition;
    Vector2	    vScale;
	Color32     colour;
	s32         iActionRequired;
    s32		    iFrame;
    s32		    depth;
    float	    fWidth;
    float	    fHeight;
    float	    fRotation;
    bool	    bVisible;
    bool	    bFrameJumpPlay;
};

#define CO_STATE_PENDING_ATTACH 		(1<<1)
#define CO_STATE_PENDING_CREATE 		(1<<2)
#define CO_STATE_PENDING_GET_OBJECT		(1<<3)
#define CO_STATE_PENDING_RELEASE		(1<<4)
#define CO_STATE_PENDING_GET_ROOT		(1<<5)
#define CO_STATE_PENDING_INVOKE			(1<<6)
#define CO_STATE_ACTIVE					(1<<7)
#define CO_STATE_FLAGGED_FOR_RELEASE	(1<<8)
#define CO_STATE_ATTACHED_BY_CODE		(1<<9)

class CComplexObject final
{
public:

	CComplexObject() { Init(); }
	~CComplexObject() { Clear(); }

	COMPLEX_OBJECT_ID GetId() const { return objectId; }
	void SetObjectId(COMPLEX_OBJECT_ID newId) { objectId = newId; }
	bool IsActive() const { return objectId != INVALID_COMPLEX_OBJECT_ID; }
	bool IsActiveAndAttached() const;
	bool HasInvokesPending() const;
	void Clear() { objectId = INVALID_COMPLEX_OBJECT_ID; }
	void Init() { Clear(); }
	s32 GetMovieId() const;

	void Release();

	// 'set' functions
	void SetPosition(Vector2 const& vPos);
    void SetPositionInPixels(Vector2 const& vPixelPos);
    void SetPosition( Vec2f_In vPos);
    void SetPositionInPixels( Vec2f_In vPixelPos);
	void SetXPositionInPixels(float fPixelPos);
    void SetYPositionInPixels(float fPixelPos);
    void SetSize( Vec2f_In vScale );
    void SetSize(Vector2 const& vNewScale);
    void SetScale( Vec2f_In vScale );
	void SetScale(Vector2 const& vNewScale);
	void SetXScale(float fScale);
	void SetRotation( float fRotation );
	void SetMember( const char *pInstanceName, bool const c_value);
	void SetMember( const char *pInstanceName, float const c_value);
	void SetMember( const char *pInstanceName, const char *c_value);
    void AdjustXPositionInPixels(float fPixels);
	void SetWidth(float fWidth);
	void SetHeight(float fHeight);
	void SetVisible(bool bValue);
	void GotoFrame(s32 iFrame, bool const andPlay = false);
	void SetTextField(const char *pText);
	void SetTextFieldHTML(const char *pHtmlText);
	void SetColour(Color32 const &colour);
	void SetAlpha( u8 const c_alpha );
	void SetDepth( s32 const c_depth );

	Vector2 GetPosition();
	Vector2 GetSize();
	Vector2 GetPositionInPixels();
	float GetXPositionInPixels();
	float GetYPositionInPixels();
	Vector2 GetScale();
	float GetRotation();
	bool GetMember(const char *pInstanceName, bool &out_bool);
	bool GetMember(const char *pInstanceName, float &out_number);
	bool GetMember(const char *pInstanceName, atString &out_string);
	float GetWidth();
	float GetHeight();
	bool IsVisible();
	s32 GetCurrentFrame();
	const char *GetTextField();
	Color32 GetColour();
	u8 GetAlpha();
	s32 GetDepth();
	atHashWithStringBank GetObjectNameHash();

	static CComplexObject GetStageRoot(s32 iMovieId);  // required to get root
	CComplexObject GetObject(const char *pMovieClipName1, const char *pMovieClipName2 = NULL, const char *pMovieClipName3 = NULL);
	CComplexObject CreateEmptyMovieClip(const char *pMovieClipName, s32 iDepth);
	CComplexObject AttachMovieClip(const char *pMovieClipName, s32 iDepth);
	CComplexObject AttachMovieClipInstance(const char *pMovieClipName, s32 iDepth);

private:
    friend class CScaleformMgr;
    COMPLEX_OBJECT_ID objectId;

private: // methods
	GFxValue *GetComplexObjectGfxValue();
	bool GetMemberInternal(const char *pInstanceName, bool &out_valueBool, float &out_valueNumber, atString &out_valueString);
};



class CComplexObjectArrayItem final
{
public:

	CComplexObjectArrayItem();

	GFxValue *GetGfxValue() { return &m_asObject; }
	s32 GetMovieId() const { return m_iMovieId; }

	void SetToInvokeOnObject(bool bSet);

private: // declarations and variables
    friend class CComplexObject;
    friend class CScaleformComplexObjectMgr;
    sComplexObjectProperties m_properties[MAX_CO_BUFFERS];
    GFxValue m_asObject;
    ConstString m_cPendingString[3];
    s32 m_iPendingDepth;
    COMPLEX_OBJECT_ID m_iPendingParentObjectUniqueId;

    atHashWithStringBank m_ObjectNameHash;
    u32 m_uState;
    s32 m_iMovieId;
    COMPLEX_OBJECT_ID m_iUniqueId;
    bool m_bInitialised;
    bool m_bInitialisedEarly;

private://  methods

	static bool ReleaseObject(CComplexObjectArrayItem *pObject, bool bForce = false);

	bool IsUnused() const { return (m_uState == 0 ? true : false); }
	bool IsActive() const { return ((m_uState & CO_STATE_ACTIVE) ? true : false); }

	void RemoveObject();
	
	static bool ReleaseObjectInternal(s32 iIndex, bool bForce = false);  // release individual objects we have got
	
	void SetObject(GFxValue newObject);

	void SetMovieId(s32 const iNewMovieId);
	void SetUniqueId();
	bool IsInitialised() const { return m_bInitialised; }
	s32 GetUniqueId() const { return m_iUniqueId; }

	u32 GetActionRequired(s32 iBuf = -1);

	bool IsPendingAttach() const { return ((m_uState & CO_STATE_PENDING_ATTACH) ? true : false); }
	bool IsPendingCreate() const { return ((m_uState & CO_STATE_PENDING_CREATE) ? true : false); }
	bool IsPendingGet() const { return ((m_uState & CO_STATE_PENDING_GET_OBJECT) ? true : false); }
	bool IsPendingRelease() const { return ((m_uState & CO_STATE_PENDING_RELEASE) ? true : false); }
	bool IsPendingFlaggedForRelease() const { return ((m_uState & CO_STATE_FLAGGED_FOR_RELEASE) ? true : false); }
	bool IsPendingGetRoot() const { return ((m_uState & CO_STATE_PENDING_GET_ROOT) ? true : false); }
	bool IsPendingInvoke() const { return ((m_uState & CO_STATE_PENDING_INVOKE) ? true : false); }

    bool CanInvoke() const 
    {
        return IsInitialised() && !IsPendingRelease() && !IsPendingFlaggedForRelease();
    }

	static CComplexObject GetStageRoot(s32 iMovieId);  // required to get root
	static COMPLEX_OBJECT_ID GetObject(CComplexObjectArrayItem *pObject, CComplexObjectArrayItem *pNewObject, const char *pMovieClipName1, const char *pMovieClipName2 = NULL, const char *pMovieClipName3 = NULL);
	static COMPLEX_OBJECT_ID AttachMovieClip(CComplexObjectArrayItem *pObject, CComplexObjectArrayItem *pNewObject, const char *pMovieClipName, s32 iDepth);
	static COMPLEX_OBJECT_ID AttachMovieClipInstance(CComplexObjectArrayItem *pObject, CComplexObjectArrayItem *pNewObject, const char *pMovieClipName, s32 iDepth);
	static COMPLEX_OBJECT_ID CreateEmptyMovieClip(CComplexObjectArrayItem *pObject, CComplexObjectArrayItem *pNewObject, const char *pMovieClipName, s32 iDepth);

	// 'set' functions
	void SetPosition(Vector2 const& vPos);
	void SetPositionInPixels(Vector2 const& vPixelPos);
	void SetXPositionInPixels(float fPixelPos);
	void SetYPositionInPixels(float fPixelPos);
	void SetScale(Vector2 const& vNewScale);
	void SetXScale(float fScale);
	void SetRotation( float const fRotation );
	void SetMember( const char *pInstanceName, bool const c_valueBool );
	void SetMember( const char *pInstanceName, float const c_valueNumber );
	void SetMember( const char *pInstanceName, char const * const c_valueString );
	void AdjustXPositionInPixels(float fPixels);
	void SetWidth(float fWidth);
	void SetHeight(float fHeight);
	void SetVisible(bool bValue);
	void GotoFrame(s32 iFrame, bool const andPlay = false );
	void SetTextField(const char *pText);
	void SetTextFieldHTML(const char *pHtmlText);
	void SetColour(Color32 const &colour);
	void SetAlpha( u8 const c_alpha );
	void SetDepth( s32 const c_depth );

	// 'get' functions
	Vector2 GetPosition();
	Vector2 GetPositionInPixels();
	float GetXPositionInPixels();
	float GetYPositionInPixels();
	Vector2 GetScale();
	float GetRotation();
	eMemberValueType GetMember(const char *pInstanceName, bool &out_valueBool, float &out_valueNumber, atString &out_valueString);
	float GetWidth();
	float GetHeight();
	bool IsVisible();
	s32 GetCurrentFrame();
	bool PlayOnFrameJump();
	const char *GetTextField();
	const char *GetTextFieldHTML();
	Color32 GetColour();
	u8 GetAlpha();
	s32 GetDepth();

	static CComplexObjectArrayItem *GetStageRootInternal(CComplexObjectArrayItem *pNewObject);  // required to get root

	bool IsActionRequiredOnEitherBuffer(u32 uflag);
	void InitProperties();
	CComplexObjectArrayItem *AttachMovieClipInternal(CComplexObjectArrayItem *pNewObject, const char *pMovieClipName, s32 iDepth, const char *pInstanceName);
	CComplexObjectArrayItem *CreateEmptyMovieClipInternal(CComplexObjectArrayItem *pNewObject, s32 iDepth, const char *pInstanceName);
	CComplexObjectArrayItem *GetObjectInternal(CComplexObjectArrayItem *pNewObject, const char *pMovieClipName1, const char *pMovieClipName2, const char *pMovieClipName3);

	void SetObjectName(const char *pObjectName);
	void SetMemberInternal(const char *pInstanceName, eMemberValueType const c_type, bool const c_valueBool, float const c_valueNumber, char const * const c_valueString);

    atHashWithStringBank GetObjectNameHash() { return m_ObjectNameHash; }
};

class CScaleformComplexObjectMgr
{
public:

	static void Init();

	static s32 FindNumberOfObjectsInUseByMovie(s32 iScaleformMovieId);

	static void SyncBuffers();
	static void PerformAllOutstandingActionsOnRT();
	static void ReleaseAnyFlaggedObjects();
	static void ForceRemoveAllObjectsInUseByMovie(s32 const iScaleformMovieId);
	static void ReleaseAllObjectsInUseByMovie(s32 const iScaleformMovieId);

    static CComplexObjectArrayItem * FindObjectFromArrayIndex(COMPLEX_OBJECT_ID const objectId)
    {
        CComplexObjectArrayItem const * const c_result = FindObjectFromArrayIndexConst( objectId );
        return const_cast<CComplexObjectArrayItem*>(c_result);
    }
    static CComplexObjectArrayItem const * FindObjectFromArrayIndexConst(COMPLEX_OBJECT_ID const objectId);

#if __BANK
	static void OutputMovieClipsInUseByMovieToLog(s32 iScaleformMovieId);
#endif // __BANK

	static CComplexObjectArrayItem *GetObjectFromUniqueId(COMPLEX_OBJECT_ID objectId);

    static void GetGfxValueFromUniqueIdForInvoke( COMPLEX_OBJECT_ID const objectId, GFxValue *& out_object, bool& out_canInvoke );
    
protected:

	friend class CComplexObjectArrayItem;
	friend class CComplexObject;

	static void CheckAndSetupIfUninitialised(COMPLEX_OBJECT_ID objectId);
	
	static CComplexObjectArrayItem *FindOrCreateSlot();
 	static inline s32 GetBuffer() { return (CSystem::IsThisThreadId(SYS_THREAD_RENDER) ? GetRenderBuffer() : GetUpdateBuffer()); }
 
 	static inline s32 GetUpdateBuffer() { return CO_BUFFER_UPDATE; }
 	static inline s32 GetRenderBuffer() { return CO_BUFFER_RENDER; }

#if __BANK
	static bool DetailedLogging() { return ms_bUseDetailedLogging; }
#endif // __BANK

	static s32 FindObjectIndex(CComplexObjectArrayItem *pObject);

	static atArray<CComplexObjectArrayItem> sm_ObjectArray;

#if __BANK
	static bool ms_bUseDetailedLogging;
#endif // __BANK

	static s32 GetCurrentUniqueId() { return ms_iUniqueId; }
	static void SetCurrentUniqueId(s32 iNewId) { ms_iUniqueId = iNewId; }

private:

	static void CheckForAnyPendingInvokes(COMPLEX_OBJECT_ID objectId);
	static void AttachOrGetObjectOnStage(COMPLEX_OBJECT_ID objectId);

	static s32 ms_iUniqueId;
};

#endif // _SCALEFORM_COMPLEX_OBJECT_MGR_H_

// eof
