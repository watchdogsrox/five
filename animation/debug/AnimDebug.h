// 
// animation/debug/AnimDebug.h 
// 
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved. 
// 

#if __BANK

#ifndef ANIM_DEBUG_H
#define ANIM_DEBUG_H

#include <string>
#include <vector>

// Rage includes
#include "atl/functor.h"
#include "atl/string.h"
#include "bank/bank.h"
#include "bank/bkmgr.h"
#include "bank/button.h"
#include "bank/combo.h"
#include "bank/text.h"

// Gta includes
#include "Scene/DynamicEntity.h"
#include "animation\AnimScene\AnimSceneEntity.h"

// Rage forward declarations
namespace rage
{
	class crAnimation;
	class bkToggle;
	class bkCombo;
	class crClip;
	class crClipDictionary;
	class fwDebugBank;
}

#define ASSET_FOLDERS (CDebugClipDictionary::ms_AssetFoldersCount)

//////////////////////////////////////////////////////////////////////////
//	CDebugRenderer
//	Stores debug calls to be rendered during the debug rendering
//	portion of the game loop (so that they still operate when the
//	game is paused by the user or in script)
//////////////////////////////////////////////////////////////////////////

enum eClipContext
{
	kClipContextNone,
	kClipContextClipDictionary,
	kClipContextAbsoluteClipSet,
	kClipContextLocalFile,

	kNumClipContext
};

extern const char *eClipContextNames[];

class CDebugRenderer
{

public:

	CDebugRenderer()
	{
		m_2dTextVertDist = 0.02f;
		m_2dTextHorizontalBorder = 0.04f;
		m_2dTextVerticalBorder = 0.04f;
	};

	~CDebugRenderer(){};

	//PURPOSE: Adds a new line rendering series to the render helper
	//RETURNS: An ID number that can be used to add entries to and clear the series
	int AddSeries(const char * name, Color32 colour);

	//PURPOSE: Add an entry to the specified series
	void AddEntry(const Vector3& value, int seriesID);

	// PURPOSE: Clears all data from the specified series
	// NOTE:	The Reset() method below does not affect series. This method must be used to clear all series when you're done with the data
	void ClearSeries();

	//PURPOSE: Adds a 3d axis that will be rendered later
	void Axis (const Matrix34& mtx, const float scale, bool drawArrows = false);

	// PURPOSE: Add 2d text to a 3d location in the world
	void Text2d(const int line, const Color32 col, const char *pText);
	void Text3d(const Vector3& pos, const Color32 col, const char *pText, bool drawBGQuad = true);
	void Text3d(const Vector3& pos, const Color32 col, const s32 iScreenSpaceXOffset, const s32 iScreenSpaceYOffset, const char *pText, bool drawBGQuad = true);	

	// PURPOSE: Adds a line that will be rendered later
	void Line (const Vector3& v1, const Vector3& v2, const Color32 colour1);
	void Line (const Vector3& v1, const Vector3& v2, const Color32 colour1, const Color32 colour2);

	// PURPOSE: Adds a sphere that will be rendered later
	void Sphere (const Vector3& centre, const float r, const Color32 col, bool filled = true);

	// PURPOSE: Renders the stored debug render calls and series to the world 
	void Render();

	// PURPOSE: Clear any stored debug render calls
	// NOTE:	Does not affect stored series. Use the ClearSeries() method to remove series entries
	void Reset();

private:

	// PURPOSE: Stores a boolean value as a bit in an s32 based dynamic array
	void StoreBool(atArray<s32>& container, int entry, bool value);

	// PURPOSE: Retrieves a boolean value from a bit in an s32 based dynamic array
	bool GetBool(atArray<s32>& container, int entry);

	//stores a series of positions that can be rendered in a 3d graph form
	struct DebugSeries
	{
		atArray<Vector3>	data;
		Color32				color;
		const char *		name;
	};

	atArray<DebugSeries> m_series;

	//axis rendering
	struct DebugAxis
	{
		Matrix34	axis;
		float		scale;
		bool		drawArrows;
	};

	atArray<DebugAxis>	m_axes;

	//3d line rendering
	struct Debug3dLine
	{
		Vector3		start;
		Vector3		end;
		Color32		color1;
		Color32		color2;
	};
	atArray<Debug3dLine>	m_3dLine;

	//sphere rendering

	struct DebugSphere
	{
		Vector3		centre;
		float		radius;
		Color32		colour;
		bool		filled;
	};

	atArray<DebugSphere>	m_sphere;

	//3d text rendering

	struct Debug3dText
	{
		Vector3			position;
		Color32			colour;
		const char *	text;
		bool			drawBackground;
		s32			screenOffsetX;
		s32			screenOffsetY;
	};

	atArray<Debug3dText>	m_3dText;

	//2d text rendering

	struct Debug2dText
	{
		int				line;
		Color32			colour;
		atString		text;
	};

	atArray<Debug2dText>	m_2dText;

	float m_2dTextVertDist;
	float m_2dTextHorizontalBorder;
	float m_2dTextVerticalBorder;
};

//////////////////////////////////////////////////////////////////////////
//	Rag control for editing phase / time / frame (interchangeable)
//////////////////////////////////////////////////////////////////////////

class CDebugTimelineControl
{
public:
	// can use this to specify the type of slider to include (phase, time or frame)
	enum ePhaseSliderMode
	{
		kSliderTypePhase = 0,
		kSliderTypeFrame,
		kSliderTypeTime,
		kSliderTypeMax
	};

	CDebugTimelineControl():
		m_mode(kSliderTypePhase),
			m_lastMode(kSliderTypePhase),
			m_pClip(NULL),
			m_value(0.0f)
		{
			//nothing to see here...
		}

		CDebugTimelineControl(ePhaseSliderMode startMode):
		m_mode(startMode),
			m_lastMode(startMode),
			m_pClip(NULL),
			m_value(0.0f)
		{
			//move along...
		}

	// PURPOSE: Adds the timeline widgets to a rag bank
	void AddWidgets(bkBank* pBank, datCallback sliderCallback = NullCB);

	// PURPOSE:	Removes any widgets created by AddWidgets
	void RemoveWidgets();

	// PURPOSE: Call at end of level to remove the control safely
	void Shutdown();

	// PURPOSE: Gets the phase / frame number or time for the timeline position selected on the control
	float GetPhase();
	float GetFrame();
	float GetTime();

	// PURPOSE: Sets the timeline position of the control to the specified phase, frame or time
	void SetPhase(float phase);
	void SetFrame(float frame);
	void SetTime(float time);

	// PURPOSE: Sets the anim player used to calculate time and frame values
	//			Setting this to NULL will deactivate the control
	inline void SetClip(const crClip* pClip) { m_pClip=pClip; SetUpSlider(0.0f); }

	// PURPOSE: Returns the anim player pointer the timeline control currently refers to
	inline const crClip* GetClip() { return m_pClip; } 

	// PURPOSE: Sets the mode of the slider (phase / frame number / time)
	void SetTimelineMode(ePhaseSliderMode mode);

private:

	void SetMode();

	void SetUpSlider(float startPhase);

	int				m_mode;		// the mode the slider is in
	int				m_lastMode;	

	atArray<bkSlider*>	m_sliders;	// a pointer to the controlling slider 

	const crClip*	m_pClip;	// a pointer to the relevant anim player
	
	float			m_value;	// The currently selected value (attached to the slider)

	static const char * ms_modeNames[kSliderTypeMax];	//used to select the mode of the slider
};


//////////////////////////////////////////////////////////////////////////
// Structure containing debug names for anim dictionaries
//////////////////////////////////////////////////////////////////////////
class CDebugClipDictionary
{	
public:

	//////////////////////////////////////////////////////////////////////////
	//	Static dictionary management methods
	//////////////////////////////////////////////////////////////////////////

	// PURPOSE: returns true if the dictionary names have been loaded. False if not
	static inline bool ClipDictionariesLoaded() { return (m_animationDictionaryNames.GetCount()>0);}

	// PURPOSE: returns a pointer to the list of dictionary names for use with RAG combo boxes
	static inline const char ** GetClipDictionaryNameList() {return &(m_animationDictionaryNames[0]);}

	// PURPOSE: returns the number of dictionary names in the name list
	static inline int CountClipDictionaryNames() {return m_animationDictionaryNames.GetCount();}

	// PURPOSE: Call this once to load the dictionary names
	static void LoadClipDictionaries(bool bSort = true);

	// PURPOSE: Attempts to stream a dictionary into memory
	static bool StreamDictionary(int dictionaryIndex, bool bSort = true);

	// PURPOSE: Accessors for debug dictionary structure
	static inline atVector<CDebugClipDictionary *>::iterator GetFirstDictionary() { return m_animDictionary.begin();}
	static inline atVector<CDebugClipDictionary *>::iterator GetLastDictionary() { return m_animDictionary.end();}

	// PURPOSE: Return a reference to the dictionary at index dictionaryIndex
	static inline CDebugClipDictionary& GetClipDictionary(int dictionaryIndex) { return *m_animDictionary[dictionaryIndex]; }
	static inline CDebugClipDictionary* FindClipDictionary(u32 dictionaryHash, s32& index) 
	{ 
		index = -1;
		for (int i=0; i<m_animationDictionaryNames.GetCount(); i++)
		{
			if (m_animDictionary[i]->GetHash()==dictionaryHash)
			{
				index = i;
				return m_animDictionary[i];
			}
		}
		return NULL; 
	}

	// PURPOSE: Use to save the passed in clip back to the hard disk
	static bool SaveClipToDisk(crClip* pClip, const char * dictionaryName, const char * animName);

	// PURPOSE: Callback function for the clip iterator. Will be called once for each clip found on disk.
	//			PARAMS:		crClip*:			Will contain a pointer to the clip found by the iterator
	//						crClipDictionary*:	Will contain a pointer to the dictionary the clip is in
	//						int :				Contains the dictionary index in the dictionary store
	typedef Functor3<crClip*, crClipDictionary*, strLocalIndex>	CClipImageIteratorCallback;

	// PURPOSE: Iterates over all the clips in the clip dictionaries, streams them in and calls
	//			the provided callback function
	static void IterateClips(CClipImageIteratorCallback callback);

	// PURPOSE: Use this to initialise the system on level start
	static void InitLevel();

	// PURPOSE: USe this to shutdown the system on level end
	static void ShutdownLevel();

	//////////////////////////////////////////////////////////////////////////
	// Local file system searching methods
	//////////////////////////////////////////////////////////////////////////

	//	Searches the local directory structure trying to find a match for the supplied dictionary
	//	e.g. amb@drink_bottle becomes "...\amb@\drink_\bottle\"
	//	The local directory structure must be up to date with the image for this to be successful
	static const char *FindLocalDirectoryForDictionary(const char *dictionaryName);
	static const char *FindLocalDirectoryForDictionary(const char *localPath, const char *dictName);
	static void FindFileCallback(const fiFindData &data, void *);

	//////////////////////////////////////////////////////////////////////////
	//	Constructors
	//////////////////////////////////////////////////////////////////////////

	CDebugClipDictionary() :
	  index(-1),
		  p_name(),
		  m_bAnimsLoaded(false)
	  {

	  }

	  CDebugClipDictionary(int dictionaryIndex)
	  {
		  Initialise(dictionaryIndex);
	  }

	  // PURPOSE: Tie this debug dictionary to an anim dictionary in the AnimManager
	  // PARAMS:	index = the index of the animDictionary in the AnimManager that this object will represent
	  void Initialise(int dictionaryIndex);

	  // PURPOSE: Return the index in the anim manager
	  int GetIndex()	{ return index; }

	  // PURPOSE: return a string containing the animation dictionary name
	  const strStreamingObjectName& GetName() const	{ return p_name; }

	  // PURPOSE: return a hash of the anim dictionary name for debug purposes
	  const u32 GetHash() const	{ return p_name.GetHash(); }


	  // PURPOSE: Return a string containing the name of the animation at index animIndex
	  inline const char * GetClipName( int animIndex, bool bSort = true ) { return LoadAnimNames(bSort)? animationNames[animIndex]: ms_notLoadedMessage; }

	  // PURPOSE: Return the number of animations in this anim dictionary
	  inline int GetClipCount() { return LoadAnimNames()? animationNames.GetCount(): 0;}

	  // PURPOSE: Returns a pointer to a list of animation names that can be used with combo boxes
	  const char** GetClipNameList() { return LoadAnimNames()? &(animationNames[0]): &ms_notLoadedMessage; }

	  // PURPOSE: Finds the path to the directory of the anim dictionary on the local hard drive
	  const char * FindLocalDirectory() { return FindLocalDirectoryForDictionary(GetName().GetCStr());}

	  // PURPOSE: Checks if the specified dictionary is streamed in and streams it if necessary
	  // RETURNS: True if the dictionary was streamed successfully, false if not
	  bool StreamIn();

	  // PURPOSE: Returns true if the anim names have been successfully loaded for this anim
	  inline bool ClipNamesLoaded() { return m_bAnimsLoaded; }

	  static int ms_AssetFolderIndex;  

	  static bool PushAssetFolder(const char *szAssetFolder);
	  static void DeleteAllAssetFolders();

	  static const int ms_AssetFoldersMax = 16;
	  static int ms_AssetFoldersCount;
	  static const char* ms_AssetFolders[ms_AssetFoldersMax]; 
		
private:

	// PURPOSE: Streams in the animation names and populates the animation names structure
	// RETURNS: True if the list of names loaded successfully
	bool LoadAnimNames(bool bSort = true);

	// PURPOSE: Removes any non static data created when loading anims
	//			Called by ShutdownLevel()
	void Shutdown();

	//	Statics
	static atVector<CDebugClipDictionary *> m_animDictionary;	// Debug structures to represent all of the anim dictionaries
	static atArray<const char*> m_animationDictionaryNames;		// A list of the dictionary names (to be used with combo boxes in RAG)
	
	static const char * ms_notLoadedMessage;

	bool m_bAnimsLoaded;

	//animation dictionary index (in the AnimManager)
	int index;

	// animation dictionary name
	strStreamingObjectName p_name;

	// array of animation names (filenames stripped of path and extensions)
	atVector<atString> animationStrings;

	// array of pointers to the animation strings (to be used by combo boxes)
	atArray<const char*> animationNames;

private:

	CDebugClipDictionary(const CDebugClipDictionary &);
	CDebugClipDictionary &operator =(const CDebugClipDictionary &);
};

//////////////////////////////////////////////////////////////////////////
//	Rag debug selector - defines some common functionality for 
//	the various debug selector widgets
//////////////////////////////////////////////////////////////////////////

class CDebugSelector: public datBase
{
public:

	virtual void AddWidgets(bkBank* pBank, const char * widgetTitle, datCallback selectionChangedCallback = NullCB, bool bLoadNamesIfNeeded = true);

	virtual void RemoveWidgets();

	// PURPOSE: Returns the model index for the selected object model
	virtual inline s32 GetSelectedIndex(){ return m_selectedIndex; }

	// PURPOSE: Returns the model index for the selected object model
	const char * GetSelectedName() const;

	// PURPOSE: Select the item in the list with the specified name
	virtual void Select (const char * itemName) { Select(itemName, true); }
	virtual void Select(const char * itemName, bool doCallback);

	inline bool HasSelection() { return m_selectedIndex>0; }

	// PURPOSE: Has the selector been initialised.
	inline bool IsInitialised() { return GetList().GetCount()>0; }

	// PURPOSE: Refresh the list of names
	void Refresh() 
	{
		if (IsInitialised())
		{
			LoadNames(); 
			UpdateCombo(); 
		}
	}

protected:

	virtual atArray<const char *>& GetList() = 0;
	virtual const atArray<const char *>& GetConstList() const = 0;

	// PURPOSE: Updates the list with the current data
	void UpdateCombo();

	// PURPOSE: Loads the list we want to select from
	virtual void LoadNames() = 0;

	// PURPOSE: locate the index of an item in the list using string comparisons
	s32 FindIndex(const char * itemName);

	// PURPOSE: Callback method for the combo box
	void Select();

	// PURPOSE: Call to derived classes to notify when a selection has been changed
	virtual void SelectionChanged();

	// The index of the model name currently selected in the combo box
	int m_selectedIndex;

	// The callback to fire once a new model is chosen
	datCallback m_selectionChangedCallback;

	//string to display in the combo box when the list of names is empty
	static const char * ms_emptyMessage;

	atArray<bkCombo*>	m_pCombos;	//	The rag combo box we're using to choose things
};

//////////////////////////////////////////////////////////////////////////
//	File selector - builds a rag combo containing a list of files
//	in a directory on the local disk
//////////////////////////////////////////////////////////////////////////

class CDebugFileSelector: public CDebugSelector
{
public:
	
	CDebugFileSelector(const char * directory, const char * extension = NULL):
		m_searchDirectory(directory),
		m_extension(extension)
	{
	}

	void AddWidgets(bkBank* pBank, const char * widgetTitle, datCallback selectionChangedCallback  = NullCB, bool bLoadNames = true );

	const char * GetDirectoryPath()  const { return m_searchDirectory; }
	atString& GetSelectedFilePath() { return m_selectedFile; }

private:

	inline atArray<const char *>& GetList() { return m_fileNames; }
	const atArray<const char *>& GetConstList() const { return m_fileNames; }

	void LoadNames();

	void SelectionChanged();

	// PURPOSE: Default callback used to load files. Need to add the facility to override this
	static void DefaultFileCallback(const fiFindData &data, void *);

	const char * m_searchDirectory;		// A pointer to the directory path we'll be searching

	atString m_selectedFile;			//stores the full name and path of the selected file

	atArray<atString> m_fileStrings;	// stores the file names in a dynamic structure
	atArray<const char *> m_fileNames;	// stores a list of pointers to the file names

	const char * m_extension;

};

//////////////////////////////////////////////////////////////////////////
//	Object selector - selects an object based on the list of object model
//	infos
//////////////////////////////////////////////////////////////////////////

class CDebugObjectSelector: public CDebugSelector
{
public:
	CDebugObjectSelector() :
	  m_bSearchDynamicObjects(true),
	  m_bSearchNonDynamicObjects(false)
	{
		memset(&m_searchText[0], 0, 16);
		sprintf(m_searchText, "");
	}

	// PURPOSE: Returns the model index for the selected object model
	fwModelId GetSelectedModelId() const;

	// PURPOSE: Returns the model index for the selected object model
	inline const char * GetSelectedObjectName() const {return CDebugSelector::GetSelectedName();}

	// PURPOSE: Returns true if list is empty
	bool IsEmpty() const { return GetConstList().GetCount() == 0; }

	// PURPOSE: Attempts to stream the selected model into memory and returns true if successfull
	bool StreamSelected() const;

	// PURPOSE: Create and return an instance of the selected object at the provided location
	CObject* CreateObject(const Mat34V& location) const;

	virtual void Select(const char * itemName);

	// PURPOSE: Reloads the list of model names and updates the associated combo box
	void RefreshList();

	// PURPOSE: Adds a search box to the object selector widget. Since the list of objects is so long,
	//			we don't want to be creating a list with every model in the game in it.
	virtual void AddWidgets(bkBank* pBank, const char * widgetTitle, datCallback selectionChangedCallback  = NullCB, bool bLoadNames = false )
	{
		pBank->PushGroup("Object Search", false);
			pBank->AddToggle("Objects (Dynamic)", &m_bSearchDynamicObjects, NullCB, "Objects (Dynamic)"); 
			pBank->AddToggle("Buildings (Non-Dynamic)", &m_bSearchNonDynamicObjects, NullCB, "Objects (non Dynamic)"); 
			pBank->AddButton("Refresh List", datCallback(MFA(CDebugObjectSelector::RefreshList), (datBase*)this)); 
		pBank->PopGroup(); 

		m_searchBoxes.PushAndGrow(pBank->AddText("Enter search text:", &m_searchText[0],16,false, datCallback(MFA(CDebugObjectSelector::RefreshList), (datBase*)this)));

		//call the parent add widgets
		CDebugSelector::AddWidgets(pBank, widgetTitle, selectionChangedCallback, bLoadNames);
	}

	virtual void RemoveWidgets()
	{
		for (int i=0; i<m_searchBoxes.GetCount(); i++)
		{
			m_searchBoxes[i]->Destroy();
		}
		m_searchBoxes.clear();

		CDebugSelector::RemoveWidgets();
	}

protected:

	inline virtual atArray<const char *>& GetList() { return m_modelNames; }
	const atArray<const char *>& GetConstList() const { return m_modelNames; }

	// PURPOSE: Loads the list of model names from the static store
	virtual void LoadNames();

	// a list of model names useable by a RAG combo box
	atArray<const char *> m_modelNames;

	// a 16 character search box for narrowing down the names
	atArray<bkText*> m_searchBoxes;
	char m_searchText[16];
	bool m_bSearchDynamicObjects; 
	bool m_bSearchNonDynamicObjects; 

};

//////////////////////////////////////////////////////////////////////////
//	Ped Model Selector
//	Provides a reuseable interface in RAG for selecting ped models
//////////////////////////////////////////////////////////////////////////#

class CDebugPedModelSelector: public datBase
{
public:

	CDebugPedModelSelector()
		: m_pRegisteredEntity(NULL)
		, m_selectedModel(-1)
	{

	}

	void AddWidgets(bkBank* pBank, const char * widgetTitle, datCallback modelChangedCallback = NullCB);

	inline void SetRegisteredEntity(CDynamicEntity **entitySlot) {m_pRegisteredEntity=entitySlot;}

	// PURPOSE: returns a pointer to the entity currently registered with the model selector
	inline CDynamicEntity* GetRegisteredEntity(){ return m_pRegisteredEntity ? *m_pRegisteredEntity: NULL;}

	// PURPOSE: Returns the model index for the selected ped model
	fwModelId GetSelectedModelId() const;

	// PURPOSE: Returns the model index for the selected ped model
	inline const char * GetSelectedModelName() const
	{
		if (m_selectedModel>-1) 
			return m_modelNames[m_selectedModel];
		else
			return "";
	}

	inline bool HasSelection() const { return m_selectedModel>0; }

	// PURPOSE: Returns the number of models in the list
	static inline int GetModelCount() { return m_modelNames.GetCount(); }

	// PURPOSE: Sets the current selection to the specified list entry
	inline void Select(int listEntry){ m_selectedModel = listEntry; SelectModel(); }

	// PURPOSE: Sets the selection to the specified model by name
	void Select(const char * modelName);

	// PURPOSE: Create a new ped using the selected model.
	CPed* CreatePed(const Mat34V& location, ePopType popType, bool generateDefaultTask = true) const;

private:

	// PURPOSE: Loads the list of model names from the static store
	static void LoadModelNames();

	// PURPOSE: Callback method for the model combo box - changes the model of the registered dynamic entity
	void SelectModel();

	// PURPOSE: Recreates the registered ped
	void RecreatePed();

	// The index of the model name currently selected in the combo box
	s32 m_selectedModel;

	CDynamicEntity **m_pRegisteredEntity;

	// The callback to fire once a new ped model is chosen
	datCallback m_modelChangedCallback;

	// a list of model names useable by a RAG combo box
	static atArray<const char *> m_modelNames;
};

//////////////////////////////////////////////////////////////////////////
// Vehicle selector - provides a list of vehicles to load
//////////////////////////////////////////////////////////////////////////
class CDebugVehicleSelector: public CDebugSelector
{
public:
	CDebugVehicleSelector()
	{
		memset(&m_searchText[0], 0, 16);
		sprintf(m_searchText, "");
	}

	// PURPOSE: Returns the model index for the selected object model
	fwModelId GetSelectedModelId() const;

	// PURPOSE: Returns the model index for the selected object model
	inline const char * GetSelectedObjectName() const {return CDebugSelector::GetSelectedName();}

	// PURPOSE: Attempts to stream the selected model into memory and returns true if successful
	bool StreamSelected() const;

	// PURPOSE: Reloads the list of model names and updates the associated combo box
	void RefreshList();

	// PURPOSE: Create a new vehicle using the selected model.
	CVehicle* CreateVehicle(const Mat34V& location, ePopType popType) const;

	// PURPOSE: Adds a search box to the object selector widget. Since the list of objects is so long,
	//			we don't want to be creating a list with every model in the game in it.
	virtual void AddWidgets(bkBank* pBank, const char * widgetTitle, datCallback selectionChangedCallback  = NullCB, bool bLoadNames = false )
	{
		pBank->PushGroup("Vehicle Search", false);
		pBank->AddButton("Refresh List", datCallback(MFA(CDebugVehicleSelector::RefreshList), (datBase*)this)); 
		pBank->PopGroup(); 

		m_searchBoxes.PushAndGrow(pBank->AddText("Enter search text:", &m_searchText[0],16,false, datCallback(MFA(CDebugObjectSelector::RefreshList), (datBase*)this)));

		//call the parent add widgets
		CDebugSelector::AddWidgets(pBank, widgetTitle, selectionChangedCallback, bLoadNames);
	}

	virtual void RemoveWidgets()
	{
		for (int i=0; i<m_searchBoxes.GetCount(); i++)
		{
			m_searchBoxes[i]->Destroy();
		}
		m_searchBoxes.clear();

		CDebugSelector::RemoveWidgets();
	}

	virtual void Select(const char * itemName);

protected:

	inline virtual atArray<const char *>& GetList() { return m_modelNames; }
	const atArray<const char *>& GetConstList() const { return m_modelNames; }

	// PURPOSE: Loads the list of model names from the static store
	virtual void LoadNames();

	// a list of model names useable by a RAG combo box
	atArray<const char *> m_modelNames;

	// a 16 character search box for narrowing down the names
	atArray<bkText*> m_searchBoxes;
	char m_searchText[16];
};
//////////////////////////////////////////////////////////////////////////
// Filter selector - provides a list of filters to load
//////////////////////////////////////////////////////////////////////////
class CDebugFilterSelector: public CDebugSelector
{
public:
	CDebugFilterSelector()
	{
		memset(&m_searchText[0], 0, 16);
		sprintf(m_searchText, "");
	}

	// PURPOSE: Returns the model index for the selected object model
	crFrameFilter* GetSelectedFilter();

	fwMvFilterId GetSelectedFilterId(){ 
		if (HasSelection())
		{
			fwMvFilterId filterId(GetSelectedObjectName()); 
			return filterId;
		}
		else
		{
			return FILTER_ID_INVALID;
		}
	}

	// PURPOSE: Returns the model index for the selected object model
	inline const char * GetSelectedObjectName() {return HasSelection() ? CDebugSelector::GetSelectedName() : NULL;}

	// PURPOSE: Attempts to stream the selected model into memory and returns true if successful
	bool StreamSelected();

	// PURPOSE: Reloads the list of filter names and updates the associated combo box
	void RefreshList();

	 //PURPOSE: Adds a search box to the object selector widget. Since the list of objects is so long,
		//		we don't want to be creating a list with every model in the game in it.
	virtual void AddWidgets(bkBank* pBank, const char * widgetTitle, datCallback selectionChangedCallback  = NullCB, bool bLoadNames = false )
	{
		pBank->PushGroup("Filter Search", false);
		pBank->AddButton("Refresh List", datCallback(MFA(CDebugFilterSelector::RefreshList), (datBase*)this)); 
		pBank->PopGroup(); 

		m_searchBoxes.PushAndGrow(pBank->AddText("Enter search text:", &m_searchText[0],16,false, datCallback(MFA(CDebugFilterSelector::RefreshList), (datBase*)this)));

		//call the parent add widgets
		CDebugSelector::AddWidgets(pBank, widgetTitle, selectionChangedCallback, bLoadNames);
	}

	virtual void RemoveWidgets()
	{
		for (int i=0; i<m_searchBoxes.GetCount(); i++)
		{
			m_searchBoxes[i]->Destroy();
		}
		m_searchBoxes.clear();

		CDebugSelector::RemoveWidgets();
	}

protected:

	inline virtual atArray<const char *>& GetList() { return m_Names; }
	const atArray<const char *>& GetConstList() const { return m_Names; }

	// PURPOSE: Loads the list of model names from the static store
	virtual void LoadNames();

	// a list of model names useable by a RAG combo box
	atArray<const char *> m_Names;

	// a 16 character search box for narrowing down the names
	atArray<bkText*> m_searchBoxes;
	char m_searchText[16];
};

//////////////////////////////////////////////////////////////////////
//	Clip selector
//	Provides a reusable rag widget for selecting clips
//	based on clip dictionary name and clip name
//////////////////////////////////////////////////////////////////////////

class CDebugClipSelector: public datBase
{
public:
	virtual ~CDebugClipSelector();

	//////////////////////////////////////////////////////////////////////////
	//	Clip selection methods
	//////////////////////////////////////////////////////////////////////////

	// PURPOSE: Checks if an clip is selected in the clip selector
	bool HasSelection() const;

	inline int GetClipContext() const { return m_clipContext; }

	fwMvClipSetId GetSelectedClipSetId() const;

	fwMvClipId GetSelectedClipId() const;

	const char * GetSelectedClipDictionary() const;

	const char * GetSelectedClipName() const;

	// PURPOSE: Returns the selected clip
	crClip* GetSelectedClip() const;
	
	void SetShowClipSetClipNames(bool b) { m_bShowClipSetClipNames = b; }

	// PURPOSE:	Adds the necessary widgets to select a clip in RAG
	void AddWidgets(bkGroup* pGroup, datCallback clipDictionaryCallback = NullCB, datCallback clipCallback = NullCB);

	// PURPOSE:	Removes any widgets created by AddWidgets (if applicable)
	void RemoveAllWidgets();

	// PURPOSE:	Removes any widgets created by AddWidgets (if applicable)
	void RemoveWidgets(bkBank* pBank);

	// PURPOSE: Call at end of level to remove the control safely
	void Shutdown();

	// PURPOSE: Sets the selected clip using the provided clip dictionary and clip index
	void SelectClip(int clipDictionaryIndex, int clipIndex);
	
	// PURPOSE: Sets the selected clip using the provided clip dictionary and clip hash
	void SelectClip(u32 clipDictionaryHash, u32 clipHash);

	// PURPOSE: Sets a cached clip using the provided clip dictionary and clip hash that will be used when widgets are rebuilt.
	void SetCachedClip(u32 clipDictionaryHash, u32 clipHash);

	// PURPOSE: Attempts to stream the currently selected clip dictionary into memory
	// NOTES:	See static StreamDictionary() method for more info
	bool StreamSelectedClipDictionary();

	// PURPOSE: Saves the in memory copy of the selected clip back to the hard drive
	//			(wrapper for CDebugClipDictionary::SaveClipToDisk)
	bool SaveSelectedClipToDisk();

	// PURPOSE:
	void LoadClipDictionaryNames();

	// PURPOSE:
	void LoadClipDictionaryClipNames();

	// PURPOSE:
	void LoadClipSetNames();

	// PURPOSE:
	void LoadClipSetClipNames();

	// PURPOSE:
	void ReloadLocalFileClip();

	static bkBank* FindBank(bkWidget* pGroup);

	CDebugClipSelector(bool bEnableClipDictionaries, bool bEnableClipSets, bool bEnableLocalPaths)
	: m_bEnableClipDictionaries(bEnableClipDictionaries)
	, m_bEnableClipSets(bEnableClipSets)
	, m_bEnableLocalPaths(bEnableLocalPaths)
	, m_bShowClipSetClipNames(true)
	, m_clipContextNameCount(0)
	, m_clipContextNames(NULL)
	, m_clipDictionaryNameCount(0)
	, m_clipDictionaryNames(NULL)
	, m_clipDictionaryClipNameCount(0)
	, m_clipDictionaryClipNames(NULL)
	, m_clipSetNameCount(0)
	, m_clipSetNames(NULL)
	, m_clipSetClipNameCount(0)
	, m_clipSetClipNames(NULL)
	, m_clipContext(kClipContextNone)
	, m_clipContextIndex(0)
	, m_searchTextLength(MAX_PATH)
	, m_searchText(NULL)
	, m_clipDictionaryIndex(0)
	, m_clipDictionaryClipIndex(0)
	, m_clipSetIndex(0)
	, m_clipSetClipIndex(0)
	, m_localFileLength(MAX_PATH)
	, m_localFile(NULL)
	, m_localFileLoadedLength(MAX_PATH)
	, m_localFileLoaded(NULL)
	, m_localFileClip(NULL)
	, m_bUseCachedClip(false)
	, m_cachedClipDictionaryHash(0)
	, m_cachedClipHash(0)
	{
		USE_DEBUG_ANIM_MEMORY();
		m_searchText = rage_new char[m_searchTextLength];
		memset(m_searchText, '\0', sizeof(char) * m_searchTextLength);
		m_localFile = rage_new char[m_localFileLength];
		memset(m_localFile, '\0', sizeof(char) * m_localFileLength);
		m_localFileLoaded = rage_new char[m_localFileLoadedLength];
		memset(m_localFileLoaded, '\0', sizeof(char) * m_localFileLoadedLength);
	}

private:

	int GetClipContextIndexFromClipContext(int iClipContext);

	// PURPOSE: Updates the clip selection widgets base on the selected clip context
	void OnClipContextSelected();

	// PURPOSE: 
	void OnSearchTextChanged();

	// PURPOSE: Updates the contents of the clip combo box based on the selcted clip set
	void OnClipSetSelected();

	// PURPOSE: Updates the contents of the clip combo box based on the selected clip dictionary
	void OnClipDictionarySelected();

	// PURPOSE: 
	void OnLocalFileChanged();

	bool m_bEnableClipDictionaries;
	bool m_bEnableClipSets;
	bool m_bEnableLocalPaths;
	bool m_bShowClipSetClipNames;

	int m_clipContextNameCount;
	const char **m_clipContextNames;

	int m_clipDictionaryNameCount;
	const char **m_clipDictionaryNames;

	int m_clipDictionaryClipNameCount;
	const char **m_clipDictionaryClipNames;

	int m_clipSetNameCount;
	const char **m_clipSetNames;

	int m_clipSetClipNameCount;
	const char **m_clipSetClipNames;

	int m_clipContext;
	int m_clipContextIndex;

	int m_searchTextLength;
	char *m_searchText;

	int m_clipDictionaryIndex; // The index of the selected clip dictionary
	int m_clipDictionaryClipIndex; // The index of the selected clip

	int m_clipSetIndex; // The index of the selected clip set
	int m_clipSetClipIndex; // The index of the selceted clip

	fwClipSetRequestHelper m_clipSetHelper;

	int m_localFileLength;
	char *m_localFile;
	int m_localFileLoadedLength;
	char *m_localFileLoaded;
	crClip *m_localFileClip;

	//Use when no clip is available
	static const char * ms_emptyMessage[];

	// Cached clip
	bool m_bUseCachedClip;
	u32 m_cachedClipDictionaryHash;
	u32 m_cachedClipHash;

	struct Widgets
	{
		Widgets()
			: m_pGroup(NULL)
			, m_pClipContextCombo(NULL)
			, m_pSearchText(NULL)
			, m_pClipSetCombo(NULL)
			, m_pClipDictionaryCombo(NULL)
			, m_pClipCombo(NULL)
			, m_pLocalFileText(NULL)
			, m_pLocalFileLoadedText(NULL)
			, m_clipDictionaryCallback(NullCB)
			, m_clipCallback(NullCB)
		{
		}

		bkGroup *m_pGroup;
		bkCombo *m_pClipContextCombo;
		bkText *m_pSearchText;
		bkCombo *m_pClipSetCombo;
		bkCombo *m_pClipDictionaryCombo;
		bkCombo *m_pClipCombo;
		bkText *m_pLocalFileText;
		bkText *m_pLocalFileLoadedText;

		datCallback m_clipDictionaryCallback; // Allows the user to specify a custom callback when the clip dictionary is changed in RAG
		datCallback m_clipCallback;
	};

	atMap< bkBank *, Widgets > m_WidgetsMap;
};

class CDebugClipSetSelector : public CDebugSelector
{
public:
	CDebugClipSetSelector()
		: CDebugSelector()
	{
		memset(&m_searchText[0], 0, 64);
		sprintf(m_searchText, "");
	}

	// PURPOSE: 
	void SelectNone() { m_selectedIndex=0;}

	// PURPOSE: Reloads the list of clipset names and updates the associated combo box
	void RefreshList();

	//PURPOSE: Adds a search box to the clip set selector widget. Since the list of clipsets is so long,
	//	       we don't want to be creating a list with every clipset in the game in it.
	virtual void AddWidgets(bkBank* pBank, const char * widgetTitle, datCallback selectionChangedCallback  = NullCB, bool bLoadNames = true)
	{
		pBank->PushGroup(widgetTitle, false);
		//pBank->AddButton("Refresh List", datCallback(MFA(CDebugFilterSelector::RefreshList), (datBase*)this)); 

		m_searchBoxes.PushAndGrow(pBank->AddText("Enter search text:", &m_searchText[0],64,false, datCallback(MFA(CDebugFilterSelector::RefreshList), (datBase*)this)));
		//call the parent add widgets

		CDebugSelector::AddWidgets(pBank, "Select clipset", selectionChangedCallback, bLoadNames);
		pBank->PopGroup(); 
	}

	virtual void RemoveWidgets()
	{
		for (int i=0; i<m_searchBoxes.GetCount(); i++)
		{
			m_searchBoxes[i]->Destroy();
		}
		m_searchBoxes.clear();

		CDebugSelector::RemoveWidgets();
	}

private:

	inline atArray<const char *>& GetList() { return m_Names; }
	const atArray<const char *>& GetConstList() const { return m_Names; }

	void LoadNames();

	atArray<const char *> m_Names;	// stores a list of pointers to the clipset names
	
	// a 64 character search box for narrowing down the names
	atArray<bkText*> m_searchBoxes;
	char m_searchText[64];
};

class CAnimScene;


class CDebugAnimSceneSelector : public CDebugSelector
{
public:
	CDebugAnimSceneSelector()
		: CDebugSelector()
	{
		memset(&m_searchText[0], 0, 64);
		sprintf(m_searchText, "");

		m_bShowDeprecatedAnimScenes = false;
	}

	// PURPOSE: 
	void SelectNone() { m_selectedIndex=0;}

	// PURPOSE: Reloads the list of animscene names and updates the associated combo box
	void RefreshList();

	//PURPOSE: Adds a search box to the animscene selector widget. Since the list of animscenes is so long,
	//	       we don't want to be creating a list with every animscene in the game in it.
	virtual void AddWidgets(bkBank* pBank, const char * widgetTitle, datCallback selectionChangedCallback  = NullCB, bool bLoadNames = true)
	{
		pBank->PushGroup(widgetTitle, false);
		//pBank->AddButton("Refresh List", datCallback(MFA(CDebugFilterSelector::RefreshList), (datBase*)this)); 
		
		pBank->AddToggle("Show deprecated animscenes", &m_bShowDeprecatedAnimScenes, datCallback(MFA(CDebugFilterSelector::RefreshList), (datBase*)this), NULL);

		m_searchBoxes.PushAndGrow(pBank->AddText("Enter search text:", &m_searchText[0],64,false, datCallback(MFA(CDebugFilterSelector::RefreshList), (datBase*)this)));
		//call the parent add widgets

		CDebugSelector::AddWidgets(pBank, "Select animscene", selectionChangedCallback, bLoadNames);
		pBank->PopGroup(); 
	}

	virtual void RemoveWidgets()
	{
		for (int i=0; i<m_searchBoxes.GetCount(); i++)
		{
			m_searchBoxes[i]->Destroy();
		}
		m_searchBoxes.clear();

		CDebugSelector::RemoveWidgets();
	}

	// PURPOSE: Reloads the list of animscene names and updates the associated combo box
	void ToggleAnim(atArray<const char *>& animSceneNames);

	inline atArray<const char *>& GetList() { return m_Names; }
	const atArray<const char *>& GetConstList() const { return m_Names; }

private:
	
	virtual void LoadNames();

	atArray<const char *> m_Names;	// stores a list of pointers to the animscene names

	// a 64 character search box for narrowing down the names
	atArray<bkText*> m_searchBoxes;
	char m_searchText[64];

	bool m_bShowDeprecatedAnimScenes;
};

class CDebugAnimSceneEntitySelector: public datBase
{
public:
	typedef Functor0 EntityChangedCB; // dictionary, clip

	CDebugAnimSceneEntitySelector()
		: m_pScene(NULL)
		, m_selectedEntityIndex(0)
		, m_pEntityCombo(NULL)
	{
		m_entityNames.clear();
	}

	bool HasSelection() const { return m_pScene && m_selectedEntityIndex>0 && m_selectedEntityIndex<m_entityNames.GetCount(); }
	CAnimSceneEntity::Id GetSelectedEntityName() const { animAssert(HasSelection()); if (HasSelection()) return m_entityNames[m_selectedEntityIndex]; else return CAnimSceneEntity::Id((u32)0); }

	bool Select(CAnimSceneEntity::Id entityName);
	void ForceComboBoxRedraw();

	void AddWidgets(bkBank& bank, CAnimScene* pScene, EntityChangedCB callback, const char * label = NULL);
	void OnWidgetChanged();

private:

	void RebuildEntityList();

	CAnimScene* m_pScene;
	s32 m_selectedEntityIndex;
	atArray<const char *> m_entityNames;
	bkCombo* m_pEntityCombo;
	EntityChangedCB m_callback;
};

class CDebugAnimSceneSectionSelector: public CDebugSelector
{
public:
	CDebugAnimSceneSectionSelector()
		: CDebugSelector()
		, m_pScene(NULL)
	{
		m_names.clear();
	}

	virtual ~CDebugAnimSceneSectionSelector()
	{

	}

	// PURPOSE: Set the anim scene to ge tthe sections from. Call this before adding widgets.
	void SetAnimScene(CAnimScene* pScene) { m_pScene = pScene; if (!m_pScene) m_names.clear(); }

	static const char * sm_FromTimeMarkerString;

private:	
	virtual atArray<const char *>& GetList() { return m_names; }
	const atArray<const char *>& GetConstList() const { return m_names; }

	virtual void LoadNames();

	CAnimScene* m_pScene;
	atArray<const char *> m_names;

};

//////////////////////////////////////////////////////////////////////////
//	Clip iterator - utility class to help with batch editing of clips
//	on disk. Will search a specified folder (and optionally subfolders)
//	load each clip it finds and call a provided callback for each one.   
//	Can optionally save the clips back to disk after editing.
//////////////////////////////////////////////////////////////////////////

// PURPOSE: Callback function for the clip iterator. Will be called once for each clip found on disk.
//			PARAMS:		crClip*:		Will contain a pointer to the clip found by the iterator
//						fiFindData:		Will contain file information on disk - filename, last modified date, etc
//						const char *:	Will contain the directory path the file is stored in
//			RETURNS:	Return true if if you want the clip iterator to save the clip back to disk before closing it.
typedef Functor3Ret<bool, crClip*, fiFindData, const char *>	CClipFileIteratorClipCallback;

class CClipFileIterator
{
public:

	CClipFileIterator(CClipFileIteratorClipCallback callback, const char * searchDirectory, bool searchSubFolders = false) 
		: m_clipCallback(callback)
		, m_searchDirectory(searchDirectory)
		, m_recursive(searchSubFolders)
	{

	}

	// PURPOSE: Iterates over all the clip files in the specified folder
	//			and calls the overridden process method provided
	void Iterate();
	static fiFindData* FindFileData(const char * filename, const char * directory);

protected:

	void IterateClips(const char * path, const char * fileType, bool recursive = false);

private:

	// The callback for each clip found
	CClipFileIteratorClipCallback m_clipCallback;

	// The directory to search for clips
	atString m_searchDirectory;

	// Should we search sub folders or not?
	bool m_recursive;

};

class CDebugFileList
{
public:

	CDebugFileList(const char * searchDirectory, const char * extension = NULL, bool searchSubFolders = false, bool storeFilePath = true, bool storeFileExtension = true) 
		: m_searchDirectory(searchDirectory)
		, m_fileExtension(extension)
		, m_recursive(searchSubFolders)
		, m_storeFilePath(storeFilePath)
		, m_storeFileExtension(storeFileExtension)
	{

	}

	// PURPOSE: Iterates over all the files in the specified folder
	void FindFiles();

	int GetNumFilesFound() const { return m_FoundFiles.GetCount(); }
	const atString& GetFilename(int index) const { Assert(index >= 0 && index < m_FoundFiles.GetCount()); return m_FoundFiles[index];}

protected:

	void FindFilesInDirectory(const char * path, const char * fileType, bool recursive = false, bool storeFilePath = true, bool storeFileExtension = true);

private:

	// The directory to search
	atString m_searchDirectory;

	// The file extension of the files to find
	atString m_fileExtension;

	// Should we search sub folders or not?
	bool m_recursive;

	// When storing the found files, should we include the path and/or file extension too?
	bool m_storeFilePath;
	bool m_storeFileExtension;

	atArray<atString> m_FoundFiles;

};

// Alphabetical sorting function used in a number of anim systems
class AlphabeticalSort
{
public:
	bool operator()(const char* left, const char* right) const
	{
		return stricmp(left, right) < 0;
	}
};

class CClipLogIterator : public crmtIterator
{	
public:
	CClipLogIterator(crmtNode* referenceNode = NULL)
	{ 
		(void)referenceNode;
	};

	~CClipLogIterator() {};

	// PURPOSE: Iterator visit override
	virtual void Visit(crmtNode&);
	static atString GetClipDebugInfo(const crClip* pClip, float Time, float weight); 
	float CalcVisibleWeight(crmtNode& node);
};

class CAnimDebug
{
public:
	static bool LogAnimations(const CDynamicEntity& dynamicEntity);
};

#endif // ANIM_DEBUG_H
#endif // __BANK
