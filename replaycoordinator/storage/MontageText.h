//=====================================================================================================
// name:		MontageText.h
// description:	Text display over a montage sequence.
//=====================================================================================================
#ifndef INC_MONTAGE_TEXT_H_
#define INC_MONTAGE_TEXT_H_

#include "control/replay/ReplaySettings.h"

#if GTA_REPLAY

// rage
#include "vector/vector2.h"

// framework
#include "fwlocalisation/templateString.h"

// game
#include "renderer/color.h"
#include "system/FileMgr.h"

#define MAX_TEXT_LINES (4)

class CMontageText
{
public: // declarations and variables

	//-------------------------------------------------------------------------------------------------
	// format: 1 based incrementing
	static const u32 MONTAGE_TEXT_VERSION = 2;
	static const u32 MAX_MONTAGE_TEXT = 96;	// # of characters
	static const u32 MAX_MONTAGE_TEXT_BYTES = MAX_MONTAGE_TEXT * 3; // # of bytes

	template< size_t _characterCount > 
	struct TextInstance
	{
		typedef rage::fwTemplateString< _characterCount > MontageTextBuffer;

		CRGBA				m_color;
		Vector2				m_position;
		Vector2				m_scale;
		s32					m_style;
		bool				m_isSet;
		MontageTextBuffer	m_text;

		TextInstance()
		{
			Reset();
		}

		bool HasText() const
		{
			return !m_text.IsNullOrEmpty();
		}

		void Reset()
		{
			m_color = CRGBA_White(0);
			m_position = Vector2( 0.f, 0.f );
			m_scale = Vector2( 0.f, 0.f );
			m_style = 0;
			m_isSet = false;
			m_text.Clear();
		}
	};

	struct MontageTextData_V1
	{
		typedef TextInstance<256> MontageTextInstance;

		MontageTextInstance		m_lines[2];
		u32						m_startTimeMs;
		u32						m_durationMs;

		MontageTextData_V1()
			: m_startTimeMs( 0 )
			, m_durationMs( 0 )
		{

		}

		MontageTextData_V1( u32 const startTimeMs, u32 const durationMs )
			: m_startTimeMs( startTimeMs )
			, m_durationMs( durationMs )
		{

		}

		void Invalidate()
		{
			m_startTimeMs = m_durationMs = 0;

			for( u32 index = 0; index < 2; ++index )
			{
				m_lines[index].Reset();
			}
		}

		bool IsLineSet( u32 const lineIndex ) const
		{
			bool const c_isSet = lineIndex < 2 ? m_lines[ lineIndex ].m_isSet : false;
			return c_isSet;
		}

		static u32 GetMaxLineCount() { return 2; }
	};

	struct MontageTextData_Current
	{
		typedef TextInstance<MAX_MONTAGE_TEXT> MontageTextInstance;

		MontageTextInstance		m_lines[MAX_TEXT_LINES];
		u32						m_startTimeMs;
		u32						m_durationMs;

		MontageTextData_Current()
			: m_startTimeMs( 0 )
			, m_durationMs( 0 )
		{

		}

		MontageTextData_Current( u32 const startTimeMs, u32 const durationMs )
			: m_startTimeMs( startTimeMs )
			, m_durationMs( durationMs )
		{

		}

		MontageTextData_Current( MontageTextData_V1 const& other )
		{
			CopyData( other );
		}

		MontageTextData_Current& operator=( MontageTextData_V1 const& rhs )
		{			
			CopyData( rhs );
			return *this;
		}

		void CopyData( MontageTextData_V1 const& other )
		{
			m_startTimeMs = other.m_startTimeMs;
			m_durationMs = other.m_durationMs;

			u32 const c_otherMax = other.GetMaxLineCount();
			u32 const c_maxLines = GetMaxLineCount();
			for( u32 index = 0; index < c_maxLines; ++index )
			{
				MontageTextInstance& currentInstance = m_lines[index];
				
				if( index < c_otherMax )
				{
					MontageTextData_V1::MontageTextInstance const& otherInstance = other.m_lines[index];

					currentInstance.m_color		= otherInstance.m_color;		
					currentInstance.m_position	= otherInstance.m_position;	
					currentInstance.m_scale		= otherInstance.m_scale;		
					currentInstance.m_style		= otherInstance.m_style;	
					currentInstance.m_isSet		= otherInstance.m_isSet;
					currentInstance.m_text.Set( otherInstance.m_text.getBuffer() );
				}
				else
				{
					currentInstance.Reset();
				}
			}
		}
			
		void Invalidate()
		{
			m_startTimeMs = m_durationMs = 0;

			for( u32 index = 0; index < MAX_TEXT_LINES; ++index )
			{
				m_lines[index].Reset();
			}
		}

		bool IsLineSet( u32 const lineIndex ) const
		{
			bool const c_isSet = lineIndex < MAX_TEXT_LINES ? m_lines[ lineIndex ].m_isSet : false;
			return c_isSet;
		}

		static u32 GetMaxLineCount() { return MAX_TEXT_LINES; }
	};

public: // methods

	CMontageText();
	CMontageText( CMontageText const& other );
	CMontageText( u32 const startTimeMs, u32 const durationMs );
	~CMontageText();

	CMontageText& operator=(const CMontageText& rhs);

	void Invalidate();

	bool SetLine( u32 const lineIndex, char const * const text, 
		Vector2 const& position, Vector2 const& scale, CRGBA const& color, s32 const style );
	void ResetLine( u32 const lineIndex );

	bool IsLineSet( u32 const lineIndex ) const;
	inline static u32 GetMaxLineCount() { return MAX_TEXT_LINES; }

	static size_t ApproximateMinimumSizeForSaving();
	static size_t ApproximateMaximumSizeForSaving();
	size_t ApproximateSizeForSaving() const;
	u64 Save( FileHandle hFile );
	u64 Load( FileHandle hFile, u32 const serializedVersion, size_t const serializedSize );

	bool ShouldRender( u32 const timeMs ) const;

	inline u32 getStartTimeMs() const {return m_data.m_startTimeMs; }
	inline void setStartTimeMs( u32 const timeMs ) { m_data.m_startTimeMs = timeMs; }

	inline u32 getDurationMs() const { return m_data.m_durationMs; };
	inline void setDurationMs( u32 const durationMs ) { m_data.m_durationMs = durationMs; }

	inline char const * getText( u32 const lineIndex ) const 
	{ 
		MontageTextData_Current::MontageTextInstance const* instance = GetInstanceData( lineIndex );
		return instance ? instance->m_text.getBuffer() : ""; 
	}

	inline void setText( u32 const lineIndex, char const * const text ) 
	{ 
		MontageTextData_Current::MontageTextInstance* instance = GetInstanceData( lineIndex );
		if( instance )
		{
			instance->m_text.Set( text ); 
		}
	}

	inline u32 getTextStyle( u32 const lineIndex ) const 
	{ 
		MontageTextData_Current::MontageTextInstance const* instance = GetInstanceData( lineIndex );
		return instance ? instance->m_style : 0; 
	}

	inline void setTextStyle( u32 const lineIndex, u32 const style ) 
	{ 
		MontageTextData_Current::MontageTextInstance* instance = GetInstanceData( lineIndex );
		if( instance )
		{
			instance->m_style = style; 
		}
	}

	inline Vector2 getTextPosition( u32 const lineIndex ) const 
	{ 
		MontageTextData_Current::MontageTextInstance const* instance = GetInstanceData( lineIndex );
		return instance	? instance->m_position : Vector2( 0.f, 0.f ); 
	}

	inline void setTextPosition( u32 const lineIndex, Vector2 const& position ) 
	{ 
		MontageTextData_Current::MontageTextInstance* instance = GetInstanceData( lineIndex );
		if( instance )
		{
			instance->m_position = position;
		}	
	}

	inline Vector2 getTextScale( u32 const lineIndex ) const 
	{ 
		MontageTextData_Current::MontageTextInstance const* instance = GetInstanceData( lineIndex );
		return instance ? instance->m_scale : Vector2( 1.f, 1.f ); 
	}

	inline void setTextScale( u32 const lineIndex, Vector2 const& scale ) 
	{ 
		MontageTextData_Current::MontageTextInstance* instance = GetInstanceData( lineIndex );
		if( instance )
		{
			instance->m_scale = scale; 
		}	
	}

	inline CRGBA getTextColor( u32 const lineIndex ) const 
	{ 
		MontageTextData_Current::MontageTextInstance const* instance = GetInstanceData( lineIndex );
		return instance ? instance->m_color : CRGBA(); 
	}

	inline void setTextColor( u32 const lineIndex, CRGBA const& color ) 
	{ 
		MontageTextData_Current::MontageTextInstance* instance = GetInstanceData( lineIndex );
		if( instance )
		{
			instance->m_color = color; 
		}
	}

private: // declarations and variables
	MontageTextData_Current m_data;
	
private: // methods
	
	inline MontageTextData_Current::MontageTextInstance* GetInstanceData( u32 const index )
	{
		MontageTextData_Current::MontageTextInstance* instance = index < GetMaxLineCount() ? &m_data.m_lines[index] : NULL;
		return instance;
	}

	inline MontageTextData_Current::MontageTextInstance const* GetInstanceData( u32 const index ) const
	{
		MontageTextData_Current::MontageTextInstance const* instance = index < GetMaxLineCount() ? &m_data.m_lines[index] : NULL;
		return instance;
	}
};

#endif // GTA_REPLAY

#endif // INC_MONTAGE_TEXT_H_
