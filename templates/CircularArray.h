/////////////////////////////////////////////////////////////////////////////////
// Title	:	CircularArray.h
// Author	:	Adam Croston
// Started	:	12/06/08
//
// Added at there are many times when a circular ring buffer is helpful.
// generally you will want to instantiate the template with a relatively small
// number for N and then feed in a stream of data (usually per frame) to the
// circular array.  Useful for things with limited resources like skidmarks, etc.
/////////////////////////////////////////////////////////////////////////////////
#ifndef CIRCULAR_ARRAY_H_
#define CIRCULAR_ARRAY_H_

// rage includes

// game includes
#include "basetypes.h"		// For s32
#include "debug/Debug.h"	// For Assertf


/////////////////////////////////////////////////////////////////////////////////
// TCircularArray a.k.a. a ring buffer class.
//
// Not optimal as the get and append functions use the % operator, but should be
// fine for most purposes.
/////////////////////////////////////////////////////////////////////////////////
template<class T, size_t N>
class TCircularArray
{
public:
	TCircularArray();
	~TCircularArray();

	size_t	Size		(void) const;		// The number of storage units (T) allocated.
	int		GetCount	(void) const;		// The current number of items in the queue.
	void	Clear		(void);				// Clear and reset the array.
	void	Append		(const T& data);	// Add a new item to the queue- possibly pushing the oldest item out of the queue
	T&		Get			(int queuePos);		// Get the item at a given queue position.
	const T*Pop			();					// Get the last item appended to the queue and reduce the queue size
	T&		operator[]	(int queuePos);		// Get the item at a given queue position.

protected:
	s32	m_count;
	s32	m_end;// Start rec is implied as 0 or endRec+1.
	T		m_data[N];
};


template<class T, size_t N>
TCircularArray<T,N>::TCircularArray()
:
m_count(0),
m_end(0)
{
	;
}


template<class T, size_t N>
TCircularArray<T,N>::~TCircularArray()
{;}


template<class T, size_t N>
size_t TCircularArray<T,N>::Size(void) const
{
	return N;
}


template<class T, size_t N>
int TCircularArray<T,N>::GetCount(void) const
{
	return m_count;
}


template<class T, size_t N>
void TCircularArray<T,N>::Clear(void)
{
	m_count = 0;
	m_end = 0;
#if __DEV
	memset(m_data, 0, sizeof(m_data));
#endif // __DEV
}


template<class T, size_t N>
void TCircularArray<T,N>::Append(const T& data)
{
	m_data[m_end] = data;
	m_end = (m_end + 1) % N;
	if(m_count < static_cast<s32>(N))
	{
		++m_count;
	}
}


template<class T, size_t N>
const T* TCircularArray<T, N>::Pop()
{
	if (m_count<=0)
		return NULL;
	m_end--;
	if (m_end < 0)
		m_end = N-1;
	m_count--;
	return &m_data[m_end];
}


template<class T, size_t N>
T& TCircularArray<T,N>::Get(int queuePos)
{
	Assertf((queuePos < m_count), "The requested queue position is greater than the count in the array.");
	s32 startRec = m_end - m_count;
	if(startRec < 0)
		startRec += N;

	const s32 slot = (startRec + queuePos) % N;
	return m_data[slot];
}


template<class T, size_t N>
T& TCircularArray<T,N>::operator[](int queuePos)
{
	return Get(queuePos);
}


#endif // CIRCULAR_ARRAY_H_
