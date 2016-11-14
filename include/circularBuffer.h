/* 
 * NVIDIA SDK (?)
 */
#ifndef __GLES2_DEBUGGER_CIRCULAR_BUFFER_INCLUDE_H__
#define __GLES2_DEBUGGER_CIRCULAR_BUFFER_INCLUDE_H__

template <class T>
class CircularBuffer
{
public:
	/** */
	inline CircularBuffer(const uint capacity = 4096) :
		_capacity(capacity), _front(0), _full(false) {
		_data.resize(capacity);
	}
	virtual ~CircularBuffer() { }

	virtual inline void resize(const uint& size) {
		if (size == _capacity)
			return;
		_data.resize(size);
		_capacity = size;
	}

	/** */
	virtual inline void reset() {
		_front = 0;
		_full = false;
	}

	/** */
	virtual inline void insert(const T d)
	{
		_data[_front++] = d;
		if (_front == _capacity)
		{
			_front = 0;
			_full = true;
		}
	}
	/** no min/max */
	virtual inline void append(const T d)
	{
		_data[_front++] = d;
		if (_front == _capacity)
		{
			_front = 0;
			_full = true;
		}
	}

	/** */
	virtual inline uint capacity() const { return _capacity; }

	virtual inline void capacity(const uint& c) { _capacity = c; }

	virtual inline uint size() const { return _full ? _capacity : _front; }

	virtual inline const T operator[](const uint i) const { return _data[i]; }

	virtual inline const T operator()(const uint i) const
	{
		return _data[(i + _full*_front) % _capacity];
	}

	virtual inline const T last() { return _data[(_front - 1 + _capacity) % _capacity]; }

	virtual inline const T secondToLast() { return _data[(_front - 2 + _capacity) % _capacity]; }

protected:
	uint _capacity;

private:
	// for implementing the circularQ
	Vector<T> _data;
	uint _front;
	bool _full;

};	// End of class CircularBuffer

#endif	// __GLES2_DEBUGGER_CIRCULAR_BUFFER_INCLUDE_H__

