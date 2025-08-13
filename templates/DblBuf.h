 #ifndef PCH_PRECOMPILED

//
// name:		DblBuf.h
// description:	Template class for double buffering generic data & access functions
// written by:	Adam Fowler
// date:		9/2/01
//
#ifndef INC_DBLBUF_H_
#define INC_DBLBUF_H_

#include "renderer\renderthread.h"

template<class T>
class CDblBuf
{
public:
	CDblBuf() {}

	// Set the two arrays to the passed value.
	void Set(const T& value) { m_array[0] = value; m_array[1] = value; }
	
	// Read & Write style
	const T&		GetReadBuf(void) const { return(m_array[gRenderThreadInterface.GetRenderBuffer()]); }
	T&				GetWriteBuf(void) { return(m_array[gRenderThreadInterface.GetUpdateBuffer()]); }

	// Render & Update style
	T&				GetRenderBuf(void) { return(m_array[gRenderThreadInterface.GetRenderBuffer()]); }
	const T&		GetRenderBuf(void) const { return(m_array[gRenderThreadInterface.GetRenderBuffer()]); }
	T&				GetUpdateBuf(void) { return(m_array[gRenderThreadInterface.GetUpdateBuffer()]); }
	const T&		GetUpdateBuf(void) const { return(m_array[gRenderThreadInterface.GetUpdateBuffer()]); }

	// Operators
	// NOTE: EJ - Memory Optimization
	T&				operator[](int index) {Assertf(index >= 0 && index < 2, "Invalid subset!"); return m_array[index];}
	const T&		operator[](int index) const {Assertf(index >= 0 && index < 2, "Invalid subset!"); return m_array[index];}

private:
	T		m_array[2];
};


#endif	//INC_DBLBUF_H_

#endif
