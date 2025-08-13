//
// filename:	StreamingMemUsage.h
// author:		David Muir <david.muir@rockstarnorth.com>
// date:		17 July 2007
// description:	
//

#ifndef INC_STREAMINGMEMUSAGE_H_
#define INC_STREAMINGMEMUSAGE_H_

#include "tools/SectorToolsParam.h"
#if SECTOR_TOOLS_EXPORT

// --- Include Files ----------------------------------------------------------------------

// Game headers
#include "ModelInfo/ModelInfo.h"
#include "ModelInfo/ModelInfo_Factories.h"
#include "streaming/streaming.h"
#include "streaming/streamingdebug.h"
#include "tools/SectorTools.h"

// Framework headers
#include "fwtl/assetstore.h"

// --- Defines ----------------------------------------------------------------------------

// --- Constants --------------------------------------------------------------------------

// --- Structure/Class Definitions --------------------------------------------------------

// Rage forward declarations
namespace rage
{
	template<class _Type,int _Align,class _CounterType> class atArray;
	class atString;
}

// Game forward declarations
namespace rage {
template<class _Type> class fwPool;
}
class CEntity;

class CStreamingMemUsage
{
public:
	static void			Init( );
	static void			Shutdown( );

	static void			Run( );
	static void			Update( );

	static void			ScanAndExportArea( );
	static bool			GetEnabled( );

#if __BANK
	static void			InitWidgets( );
#endif

	//! Per pool object structure
	struct TPoolObject
	{
		rage::atString sName;	//!< Object name
		s32 nDrawable;		//!< Object drawable physical memory usage (bytes)
		s32 nTexture;			//!< Object texture physical memory usage (bytes)
		s32 nBounds;			//!< Object bounds physical memory usage (bytes)
		s32 nDrawableVirtual;	//!< Object drawable virtual memory usage (bytes)
		s32 nTextureVirtual;	//!< Object texture virtual memory usage (bytes)
		s32 nBoundsVirtual;	//!< Object bounds virtual memory usage (bytes)
		bool bInterior;			//!< Object in Interior MILO?

		// Constructor
		TPoolObject( const char* name, s32 draw, s32 tex, s32 bound, bool interior )
		{
			this->sName = name;
			this->nDrawable = draw;
			this->nTexture = tex;
			this->nBounds = bound;
			this->nDrawableVirtual = 0;
			this->nTextureVirtual = 0;
			this->nBoundsVirtual = 0;
			this->bInterior = interior;
		}

		// Constructor for pool object split by virtual and physical memory allocation
		TPoolObject( const char* name, s32 draw, s32 tex, s32 bound, s32 drawVirt, s32 texVirt, s32 boundVirt, bool interior )
		{
			this->sName = name;
			this->nDrawable = draw;
			this->nTexture = tex;
			this->nBounds = bound;
			this->nDrawableVirtual = drawVirt;
			this->nTextureVirtual = texVirt;
			this->nBoundsVirtual = boundVirt;
			this->bInterior = interior;
		}

		// Default constructor
		TPoolObject( )
		{
			this->sName = "";
			this->nDrawable = 0;
			this->nTexture = 0;
			this->nBounds = 0;
			this->nDrawableVirtual = 0;
			this->nTextureVirtual = 0;
			this->nBoundsVirtual = 0;
			this->bInterior = false;
		}
	};


	//! Per pool structure to store memory usage
	struct TPoolMemoryUsage
	{
		s32 nNumTotalObjects;			//!< Number of total objects in pool (ignoring position)
		s32 nNumModelInfos;			//!< Number of Model Infos for pool (in current sector)
		s32 nNumModelInfosInterior;	//!< Number of interior Model Infos for pool (in current sector)
		s32 nTotal;					//!< Total physical memory for pool
		s32 nTotalVirtual;			//!< Total virtual memory for pool
		s32 nTotalInterior;			//!< Total physical memory for interior objects in pool
		s32 nTotalInteriorVirtual;	//!< Total virtual memory for interior objects in pool
		s32 nDrawable;				//!< Drawable physical memory for pool
		s32 nDrawableVirtual;			//!< Drawable virtual memory for pool
		s32 nDrawableInterior;		//!< Drawable physical memory for interior objects in pool
		s32 nDrawableInteriorVirtual;	//!< Drawable virtual memory for interior objects in pool
		s32 nTexture;					//!< Texture physical memory for pool
		s32 nTextureVirtual;			//!< Texture virtual memory for pool
		s32 nTextureInterior;			//!< Texture physical memory for interior objects in pool
		s32 nTextureInteriorVirtual;	//!< Texture virtual memory for interior objects in pool
		s32 nBounds;					//!< Bounds physical memory for pool
		s32 nBoundsVirtual;			//!< Bounds virtual memory for pool
		s32 nBoundsInterior;			//!< Bounds physical memory for interior objects in pool
		s32 nBoundsInteriorVirtual;	//!< Bounds virtual memory for interior objects in pool
		rage::atArray<TPoolObject> vObjects;

		// Default constructor
		TPoolMemoryUsage( )
			: nNumTotalObjects( 0 )
			, nNumModelInfos( 0 ), nTotal( 0 ), nTotalVirtual( 0 )
			, nDrawable( 0 ), nDrawableVirtual( 0 )
			, nTexture( 0 ), nTextureVirtual( 0 )
			, nBounds( 0 ), nBoundsVirtual( 0 )
			, nTotalInterior( 0 ), nTotalInteriorVirtual( 0 )
			, nDrawableInterior( 0 ), nDrawableInteriorVirtual( 0 )
			, nTextureInterior( 0 ), nTextureInteriorVirtual( 0 )
			, nBoundsInterior( 0 ), nBoundsInteriorVirtual( 0 )
		{
			vObjects.Reset( );
		}
	};

	//! Store object record
	struct TStoreObject
	{
		atString sName;		//!< Object name
		s32 nVirtual;		//!< Object virtual size
		s32 nPhysical;	//!< Object physical size

		// Default constructor
		TStoreObject()
			: sName( "" ), nVirtual( 0 ), nPhysical( 0 )
		{
		}

		// Constructor
		TStoreObject( const char* name, s32 virt, s32 physical )
			: sName( name ), nVirtual( virt ), nPhysical( physical )
		{			
		}
	};

	//! Per store structure to store memory usage
	struct TStoreMemoryUsage
	{
		s32 nTotal;
		s32 nVirtual;
		s32 nPhysical;
		s32 nLargest;
		rage::atArray<TStoreObject> vObjects;

		// Default constructor
		TStoreMemoryUsage( )
			: nTotal( 0 ), nVirtual( 0 ), nPhysical( 0 ), nLargest( 0 )
		{
			vObjects.Reset();
		}
	};

private:

	/**
	 * Mode enumeration
	 */
	enum etMode
	{
		kDisabled,		//!< Default disabled mode
		kExportArea		//!< Export sector grouping
	};

	/**
	 * Pass enumeration
	 */
	enum etPass
	{
		kNavmeshLoad,	//!< Navmesh-load pass (used to ensure navmeshes are loaded before finding a position nearby)
		kObjectLoad,	//!< Object-load pass (used to prevent empty sectors taking time)
		kStatsCapture	//!< Statistics capture pass
	};

	static void		SetMode( etMode eMode );
	static void		SetEnabled( bool bEnable ) { m_bEnabled = bEnable; }
	static void		MoveToNextSector( );
	static void		SetOutputFolder( );	
	static void		SetupWorldCoordinates( );
	
	static void		UpdateState( );
	
	static void		GetSectorMemoryUsageForModelInfoIds( rage::atArray<u32>& modelInfosIds,
														 rage::atArray<u32>& modelInfosInteriorIds,
														 TPoolMemoryUsage& poolMemoryUsage );

	static void		GetStoreObjects( s32 moduleId, rage::atArray<TStoreObject>& objects );
	static void		GetStoreObjectsList( const rage::atArray<strIndex>& objsInMemory, rage::atArray<TStoreObject>& objects );

	static void		WriteLog( const TPoolMemoryUsage& sAnimatedUsage,
							  const TPoolMemoryUsage& sBuildingUsage,
							  const TPoolMemoryUsage& sDummyUsage,
							  const TPoolMemoryUsage& sInteriorUsage,
							  const TPoolMemoryUsage& sPortalUsage,
							  const TPoolMemoryUsage& sPhysicalUsage,
							  const TStoreMemoryUsage& sPhysicsUsage,
							  const TStoreMemoryUsage& sStaticBoundsUsage);
	static void		WritePoolMemoryUsage( rage::fiStream* stream, 
										  const char* sPoolName,
										  const TPoolMemoryUsage& usage );
	static void		WriteStoreMemoryUsage( rage::fiStream* stream,
										   const char* sStoreName,
										   const TStoreMemoryUsage& usage );

	//-------------------------------------------------------------------------------------
	// State
	//-------------------------------------------------------------------------------------

	static bool		m_bEnabled;	
	static etMode	m_eMode;
	static etPass	m_ePass;
	static char		m_sOutputDirectory[MAX_OUTPUT_PATH];

	//-------------------------------------------------------------------------------------
	// State Tracking
	//-------------------------------------------------------------------------------------

	static u32	m_msTimer;			//!< Timer used to move between states
	static rage::fiStream* m_pProgressFile;	//!< Progress file stream pointer
	
	static bool		m_bSectorOK;		//!<

	//-------------------------------------------------------------------------------------
	// Position Tracking
	//-------------------------------------------------------------------------------------

	static int		m_nCurrentSectorX;	//!< Current sector x being checked
	static int		m_nCurrentSectorY;	//!< Current sector y being checked
	static int		m_nStartSectorX;	//!< Start sector x being checked
	static int		m_nStartSectorY;	//!< Start sector y being checked
	static int		m_nEndSectorX;		//!< End sector x being checked
	static int		m_nEndSectorY;		//!< End sector y being checked

	static float	m_startScanY;		//!< Height at which to cast down to find ground. (start)
	static float	m_endScanY;			//!< Height at which to cast down to find ground. (end)
	static float	m_scanYInc;			//!< Height at which to cast down to find ground. (the increment)
	static float	m_probeYEnd;		//!< Bottom of vertical probe	

	static char		m_description[SECTOR_TOOLS_MAX_DESC_LEN];
	static int		m_sectorNum;
	static char		m_result[SECTOR_TOOLS_MAX_RESULT_LEN];
	static char		m_startTime[SECTOR_TOOLS_MAX_START_TIME_LEN];
};

// --- Globals ----------------------------------------------------------------------------

template<typename T, typename S> 
void GetStoreMemoryStats( rage::fwAssetRscStore<T, S>& store, CStreamingMemUsage::TStoreMemoryUsage& usage )
{
	sysMemSet( &usage, 0, sizeof( CStreamingMemUsage::TStoreMemoryUsage ) );

	for ( s32 i = 0; i < store.GetCount(); ++i )
	{
		if ( !store.IsValidSlot( strLocalIndex(i) ) )
			continue; // Skip invalid store elements.

		S* pStoreObject = store.GetSlot( strLocalIndex(i) );
		if ( !pStoreObject )
			continue; // Skip invalid pointers in store.

		s32 virtSize = CStreaming::GetObjectVirtualSize( strLocalIndex(i), store.GetStreamingModuleId() );
		s32 physSize = CStreaming::GetObjectPhysicalSize( strLocalIndex(i), store.GetStreamingModuleId() );
		s32 totalSize = virtSize + physSize;

		usage.nTotal += totalSize;
		usage.nVirtual += virtSize;
		usage.nPhysical += physSize;

		if ( totalSize > usage.nLargest )
			usage.nLargest = totalSize;
	}
}

#if !__CONSOLE
template <typename T>
void GetSectorModelInfoIdsForPool( T* pPool, rage::atArray<u32>& modelInfosIds, int nX, int nY )
{
	Assertf( pPool, "Object pool undefined." );
	if ( !pPool )
		return;

	int nPoolSize = (int) pPool->GetSize( );

	for ( int i = 0; i < nPoolSize; ++i )
	{
		T::ValueType* pObject = pPool->GetSlot( i );
		if ( !pObject )
			continue;
		if ( !CModelInfo::IsValidModelInfo(pObject->GetModelIndex()) )
			continue;

		const Vector3 pos = VEC3V_TO_VECTOR3(pObject->GetTransform().GetPosition( ));
		if ( CSectorTools::VecWithinCurrentSector( pos, nX, nY ) )
			modelInfosIds.PushAndGrow( pObject->GetModelIndex( ) );
	}
}
#endif

template<class T> 
void GetSectorModelInfoIdsForPool( fwPool<T>* pPool, rage::atArray<u32>& modelInfosIds, int nX, int nY )
{
	Assertf( pPool, "Object pool undefined." );
	if ( !pPool )
		return;

	int nPoolSize = pPool->GetSize( );

	for ( int i = 0; i < nPoolSize; ++i )
	{
		T* pObject = pPool->GetSlot( i );
		if ( !pObject )
			continue;
		if ( !CModelInfo::IsValidModelInfo(pObject->GetModelIndex()) )
			continue;

		const Vector3 pos = VEC3V_TO_VECTOR3(pObject->GetTransform().GetPosition( ));
		if ( CSectorTools::VecWithinCurrentSector( pos, nX, nY ) )
			modelInfosIds.PushAndGrow( pObject->GetModelIndex( ) );
	}
}

#if !__CONSOLE
template<class T> 
void GetSectorInteriorModelInfoIdsForPool( T* pPool, rage::atArray<u32>& modelInfosIds, int nX, int nY )
{
	Assertf( pPool, "Object pool undefined." );
	if ( !pPool )
		return;

	int nPoolSize = (int) pPool->GetSize( );

	for ( int i = 0; i < nPoolSize; ++i )
	{
		T::ValueType* pObject = pPool->GetSlot( i );
		if ( !pObject )
			continue;
		if ( !CModelInfo::IsValidModelInfo(pObject->GetModelIndex()) )
			continue;

		const Vector3 pos = VEC3V_TO_VECTOR3(pObject->GetTransform().GetPosition( ));
		if ( CSectorTools::VecWithinCurrentSector( pos, nX, nY ) && pObject->m_nFlags.bInMloRoom )
			modelInfosIds.PushAndGrow( pObject->GetModelIndex( ) );
	}
}
#endif

template<class T> 
void GetSectorInteriorModelInfoIdsForPool( fwPool<T>* pPool, rage::atArray<u32>& modelInfosIds, int nX, int nY )
{
	Assertf( pPool, "Object pool undefined." );
	if ( !pPool )
		return;

	int nPoolSize = pPool->GetSize( );

	for ( int i = 0; i < nPoolSize; ++i )
	{
		T* pObject = pPool->GetSlot( i );
		if ( !pObject )
			continue;
		if ( !CModelInfo::IsValidModelInfo(pObject->GetModelIndex()) )
			continue;

		const Vector3 pos = VEC3V_TO_VECTOR3(pObject->GetTransform().GetPosition( ));
		if ( CSectorTools::VecWithinCurrentSector( pos, nX, nY ) && pObject->m_nFlags.bInMloRoom )
			modelInfosIds.PushAndGrow( pObject->GetModelIndex( ) );
	}
}

#if !__CONSOLE
template<class T> 
void GetSectorObjectsForPool( T* pPool, rage::atArray<CStreamingMemUsage::TPoolObject>& objects, int nX, int nY )
{
	Assertf( pPool, "Object pool undefined." );
	if ( !pPool )
		return;

	int nPoolSize = (int) pPool->GetSize( );
	rage::atArray<u32> modelInfos;

	for ( int i = 0; i < nPoolSize; ++i )
	{
		T::ValueType* pObject = pPool->GetSlot( i );
		if ( !pObject )
			continue;
		if ( !CModelInfo::IsValidModelInfo(pObject->GetModelIndex( )) )
			continue;

		const Vector3 pos = VEC3V_TO_VECTOR3(pObject->GetTransform().GetPosition( ));
		if ( CSectorTools::VecWithinCurrentSector( pos, nX, nY ) )
		{
			rage::atString name( CModelInfo::GetBaseModelInfoName( pObject->GetModelId() ) );
			s32 total = 0;
			s32 draw = 0;
			s32 tex = 0;
			s32 bound = 0;
			s32 totalVirtual = 0;
			s32 drawVirtual = 0;
			s32 texVirtual = 0;
			s32 boundVirtual = 0;
			bool isInterior = static_cast<bool>( pObject->m_nFlags.bInMloRoom );

			// Get individual object's memory stats
			modelInfos.Reset( );
			modelInfos.PushAndGrow( pObject->GetModelIndex( ) );
			CStreamingDebug::GetMemoryStatsByAllocation( modelInfos, total, totalVirtual, draw, drawVirtual, tex, texVirtual, bound, boundVirtual );

			CStreamingMemUsage::TPoolObject obj( name, draw, tex, bound, drawVirtual, texVirtual, boundVirtual, isInterior );
			objects.PushAndGrow( obj );
		}
	}
}
#endif

template<class T> 
void GetSectorObjectsForPool( fwPool<T>* pPool, rage::atArray<CStreamingMemUsage::TPoolObject>& objects, int nX, int nY )
{
	Assertf( pPool, "Object pool undefined." );
	if ( !pPool )
		return;

	int nPoolSize = pPool->GetSize( );
	rage::atArray<u32> modelInfos;

	for ( int i = 0; i < nPoolSize; ++i )
	{
		T* pObject = pPool->GetSlot( i );
		if ( !pObject )
			continue;
		if ( !CModelInfo::IsValidModelInfo(pObject->GetModelIndex( )) )
			continue;

		const Vector3 pos = VEC3V_TO_VECTOR3(pObject->GetTransform().GetPosition( ));
		if ( CSectorTools::VecWithinCurrentSector( pos, nX, nY ) )
		{
			rage::atString name( CModelInfo::GetBaseModelInfoName( pObject->GetModelId() ) );
			s32 total = 0;
			s32 draw = 0;
			s32 tex = 0;
			s32 bound = 0;
			s32 totalVirtual = 0;
			s32 drawVirtual = 0;
			s32 texVirtual = 0;
			s32 boundVirtual = 0;
			bool isInterior = static_cast<bool>( pObject->m_nFlags.bInMloRoom );
	
			// Get individual object's memory stats
			modelInfos.Reset( );
			modelInfos.PushAndGrow( pObject->GetModelIndex( ) );
			CStreamingDebug::GetMemoryStatsByAllocation( modelInfos, total, totalVirtual, draw, drawVirtual, tex, texVirtual, bound, boundVirtual );

			CStreamingMemUsage::TPoolObject obj( name, draw, tex, bound, drawVirtual, texVirtual, boundVirtual, isInterior );
			objects.PushAndGrow( obj );
		}
	}
}

template<class T> 
void GetSectorObjectsForPoolNoMemoryStats( fwPool<T>* pPool, rage::atArray<CStreamingMemUsage::TPoolObject>& objects, int nX, int nY )
{
	Assertf( pPool, "Object pool undefined." );
	if ( !pPool )
		return;

	int nPoolSize = pPool->GetSize( );
	rage::atArray<s32> modelInfos;

	for ( int i = 0; i < nPoolSize; ++i )
	{
		T* pObject = pPool->GetSlot( i );
		if ( !pObject )
			continue;
		if ( !CModelInfo::IsValidModelInfo(pObject->GetModelIndex( )) )
			continue;

		const Vector3 pos = VEC3V_TO_VECTOR3(pObject->GetTransform().GetPosition( ));
		if ( CSectorTools::VecWithinCurrentSector( pos, nX, nY ) )
		{
			rage::atString name( CModelInfo::GetBaseModelInfoName( pObject->GetModelId() ) );
			bool isInterior = static_cast<bool>( pObject->m_nFlags.bInMloRoom );

			// Get individual object's memory stats
			modelInfos.Reset( );
			modelInfos.PushAndGrow( pObject->GetModelIndex( ) );

			CStreamingMemUsage::TPoolObject obj( name, 0, 0, 0, isInterior );
			objects.PushAndGrow( obj );
		}
	}
}

#endif // SECTOR_TOOLS_EXPORT

#endif // !INC_STREAMINGMEMUSAGE_H_
