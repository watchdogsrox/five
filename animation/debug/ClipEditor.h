// 
// animation/debug/ClipEditor.h 
// 
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved. 
// 

#if __BANK

#ifndef CLIP_EDITOR_H
#define CLIP_EDITOR_H

// rage includes
#include "atl/string.h"
#include "crmetadata/property.h"
#include "crmetadata/properties.h"
#include "crmetadata/propertyattributes.h"
#include "crmetadata/tag.h"
#include "paging/ref.h" // pgRef
#include "crclip/clip.h"

// game includes
#include "animation/EventTags.h"
#include "animation/debug/AnimDebug.h"
#include "animation/debug/ClipTagMetadata.h"
#include "fwanimation/animdirector.h"

// forward declarations
namespace rage
{
	class crTag;
	class bkGroup;
	class bkBank;
	class crProperty;
}

//////////////////////////////////////////////////////////////////////////
//	Anim clip editor
//	Creates a set of RAG widgets that can be used to load and save
//	events and properties to clips in memory
//////////////////////////////////////////////////////////////////////////

class CAnimClipEditor
{
public:

	class PropertiesWidgetIterator : public crProperties::PropertiesCallback
	{
		public:

		bool Callback(crProperty&);
	};

	// struct used for in memory editing of 
	// tag intervals (So we don't have to keep copies
	// of the data). This may be incredibly stupid...
	struct sIntervalFloat
	{
		u8 padding[sizeof(crProperty)];
		float start;
		float end;
	};

	// Base property class - used to represent properties whose values cannot be directly accessed by rag widgets
	// override to handle individual property types
	struct sAttribute : datBase
	{
		sAttribute()
			: m_pAttrib(NULL)
		{}
		virtual ~sAttribute()
		{}

		// PURPOSE: call this to save the attribute data
		void Save();

		// A pointer to the property name in the properties collection
		crPropertyAttribute* m_pAttrib;

	protected:
		// PURPOSE: override this to save the necessary derived data to the appropriate clip property type
		virtual void SaveToAttribute(crPropertyAttribute& pAttrib) = 0;
	};

	struct sAttributeString : sAttribute
	{
		static const u32 bufferSize = 1024;

		char m_buffer[bufferSize];
		bool m_bIsHash;
	protected:
		void SaveToAttribute(crPropertyAttribute& pAttrib);
	};

	struct sAttributeEuler : sAttribute
	{
		Vector3 m_euler;
		bool m_bIsSituation;
	protected:
		void SaveToAttribute(crPropertyAttribute& pAttrib);
	};

	//////////////////////////////////////////////////////////////////////////
	//	Clip editor methods
	//////////////////////////////////////////////////////////////////////////

	static void Initialise();		//startup the clip editor

	static void Update();			//per frame update for the clip editor

	static void CreateMainWidgets(); // create the main widgets for the editor

	static void ShutdownMainWidgets(); // remove the main widgets for the editor
	
	static void Shutdown();			//close down the clip editor

	static void SelectClip();		//	callback for the clip editor anim selector

	static void Refresh();

	//////////////////////////////////////////////////////////////////////////
	//	Clip loading and saving
	//////////////////////////////////////////////////////////////////////////

	static void LoadSelectedClip();	//	Calls the individual loading methods to retrieve the events and properties for the given clip file

	static void LoadTags(crClip* pClip);
	static void LoadProperties(crClip* pClip);

	static void LoadSelectedClipFromDisk();
	static void SaveSelectedClipToDisk();

	static void RequestLoadSelectedClipFromDisk();
	static void RequestSaveSelectedClipToDisk();

	//////////////////////////////////////////////////////////////////////////
	//	Event tag editing
	//////////////////////////////////////////////////////////////////////////

	static void AddTag(const char * name);
	static void AddTagButton();
	// Custom tag generation
	// Rag
	static void ClearAllTagsButton();
	static void ClearAllFootTagsButton();
	static void ClearAllMoveEventTagsButton(const atHashWithStringNotFinal& moveEvent);
	static void ClearAllUseLeftFootTransitionTagsButton();
	static void ClearAllUseLeftFootStrafeTransitionTagsButton();
	static void ClearAllBlockTransitionTagsButton();
	static void ClearAllDistanceMarkerTagsButton();
	static void GenerateUseLeftFootTransitionTagsButton();
	static void GenerateToStrafeUseLeftFootStrafeTransitionTagsButton();
	static void GenerateToStrafeUseLeftFootStrafeTransitionTagsForDictionaryButton();
	static void GenerateFromStrafeUseLeftFootStrafeTransitionTagsButton();
	static void GenerateFromStrafeUseLeftFootStrafeTransitionTagsForDictionaryButton();
	static void GenerateBlockTransitionTagsButton();
	static void GenerateBlockTransitionTagsForDictionaryButton();
	static void GenerateDistanceMarkerTagsButton();
	// Implementation
	static void ClearAllTags();
	static void ClearAllFootTags();
	static void ClearAllMoveEventTags(crClip* pClip, const atHashWithStringNotFinal& moveEvent);
	static void ClearAllDistanceMarkerTags();
	static void GenerateUseLeftFootTransitionTags();
	static bool GenerateUseLeftFootStrafeTransitionTags(crClip* pClip, fiFindData fileData, const char* folderName);
	static void GenerateUseLeftFootStrafeTransitionTagsForDictionary();
	static bool GenerateBlockTransitionTags(crClip* pClip, fiFindData fileData, const char* folderName);
	static void GenerateBlockTransitionTagsForDictionary();
	static void GenerateDistanceMarkerTags();

	static void UpdateTagWidgets();

	static void RemoveTag(crTag* pTag);

	static void LimitTagPhaseStart(sIntervalFloat* pInterval);
	static void LimitTagPhaseEnd(sIntervalFloat* pInterval);

	static void SetPhaseFromPreviewSlider(crTag* pTag);

	static void PreviewNextFootTag();
	static void PreviewPreviousFootTag();
	static void PreviewNextDistanceMarkerTag();
	static void PreviewPreviousDistanceMarkerTag();
	static void PreviewNextTag(CClipEventTags::Key tagKey);
	static void PreviewPreviousTag(CClipEventTags::Key tagKey);
	static void SetPreviewedTagPhaseFromPreviewSlider();

	static bool VerifyTagsWithinClipRange(crClip* pClip);

	//////////////////////////////////////////////////////////////////////////
	//	Property editing
	//////////////////////////////////////////////////////////////////////////
	
	// PURPOSE: Adds a new property to the selected clips properties collection
	static void AddProperty( const char * name );

	// PURPOSE: Removes a property from the loaded clips properties collection by name
	static void DeleteProperty(crProperty* pProp);

	//Property button callbacks
	static void AddPropertyButton();

	// Adds an attribute to a property
	static void AddAttributeToPropertyButton(crProperty* pProp);
	// Removes an attribute from the property
	static void RemoveAttributeFromPropertyButton(crProperty* pProp);

	// PURPOSE: adds a set of widgets for editing the clips properties collection
	static void UpdatePropertiesCollectionWidgets();

	// PURPOSE: adds a set of widgets for editing the selected property
	static void AddPropertyWidgets(crProperty& prop);

	//////////////////////////////////////////////////////////////////////////
	// Attribute editing
	//////////////////////////////////////////////////////////////////////////

	// PURPOSE: Adds a new attribute to the provided property (could be a tag property or one in the clip collection)
	static void AddAttribute(crProperty& prop, const char * name, crPropertyAttribute::eType type);

	// PURPOSE: Pushes a set of widgets on the current group for editing the selected attribute
	static bool AddAttributeWidgets(const crPropertyAttribute& constAttrib,  void* data);

	// PURPOSE: Resizes the bitset based on the currently entered value
	static void ResizeBitset( atBitSet* pSet);

	//////////////////////////////////////////////////////////////////////////
	// Clip previewing methods
	//////////////////////////////////////////////////////////////////////////

	static void StartPreviewClip();
	static void UpdatePreviewPhase();
	static void UpdatePreviewTime();

	//////////////////////////////////////////////////////////////////////////
	//	Approval property
	//////////////////////////////////////////////////////////////////////////
	static void SetApprovalProperty();

	//////////////////////////////////////////////////////////////////////////
	//	Shortcut methods for commonly used tags
	//////////////////////////////////////////////////////////////////////////

	static void ActionButton(ClipTagAction *clipTagAction);

	//////////////////////////////////////////////////////////////////////////
	//	Properties
	//////////////////////////////////////////////////////////////////////////

	static pgRef<crClip> m_pClip;	// The currently selected clip for editing

	static fwDebugBank* m_pBank;	// Pointer to the clip editor bank

	static bool m_bAllowSaving;		// Will be set to true if the clip is successfully loaded from disk.
									// otherwise saving is disabled. This is because named information is
									// not available from clips in the resourced build.

	// Pointers to the clip editor RAG widget groups
	static bkGroup *m_pTagGroup;
	static bkGroup *m_pPropertyGroup;

	//////////////////////////////////////////////////////////////////////////
	//	Supporting data for adding new properties
	//////////////////////////////////////////////////////////////////////////

	static const u32 nameBufferSize = 64;

	static atArray< const char * > m_attributeTypeNames;
	static int m_newAttributeType;
	static char m_newAttributeName[nameBufferSize];

	static char m_newName[nameBufferSize];

	//anim selector tool for the clip editor
	static CDebugClipSelector m_clipEditAnimSelector;

	// If true, the clip will be previewed on the current focus entity and its phase driven based on any sliders
	static atHashString ms_AuthorName;
	static bool ms_previewClipOnFocusEntity;
	static float ms_previewPhase;
	static float ms_previewTime;
	static bkSlider * ms_pPreviewTimeSlider;
	static fwSyncedSceneId ms_previewScene;
	static int ms_iApprovalFlag;

	static int ms_iSelectedTag;
	static float ms_fDistanceInterval;
	static float ms_fPhaseIncrement;

	// Request clip loads and saves to be done in the update.
	static bool m_bRequestLoadSelectedClipFromDisk;
	static bool m_bRequestSaveSelectedClipToDisk;

	// Required for attributes types that cannot be hooked directly to a widget
	static atArray<sAttribute*> m_specialAttributes;

	//////////////////////////////////////////////////////////////////////////
	//	Attribute specific options and values
	//////////////////////////////////////////////////////////////////////////
	static s32 m_bitSetSize;
	static float m_UseLeftFootStrafeTransitionRatio;

	//////////////////////////////////////////////////////////////////////////
	//  Tag hashes
	//////////////////////////////////////////////////////////////////////////
	static const atHashWithStringNotFinal m_UseLeftFootTransitionTag;
	static const atHashWithStringNotFinal m_UseLeftFootStrafeTransitionTag;
	static const atHashWithStringNotFinal m_BlockTransitionTag;
};



#endif // CLIPEDITOR_H

#endif // __BANK
