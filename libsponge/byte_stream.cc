#include "byte_stream.hh"
#include <iostream>
// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity)
    : _capacity(capacity),_head(0),_tail(0),_buffer(),_written_size(0),_read_size(0),_remain_capacity(capacity),_end_input(false),_empty(true),_error(false) {}

size_t ByteStream::write(const string &data) {
    // DUMMY_CODE(data);
    // 1.judge is full
    if (_end_input)
        return 0;
    size_t strlen = min(data.length(),_remain_capacity);
    for (size_t i = 0;i < strlen;++ i){
        _buffer[_head++] = data[i];
        if (_head >= _capacity)
            _head -= _capacity;
    }
    _written_size += strlen;
    _remain_capacity -= strlen;
    return strlen;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    size_t strlen = min(len, buffer_size());
    string res = "";
    size_t i = _tail;
    while (strlen--){
        res += _buffer[i++];
        if (i >= _capacity)
            i -= _capacity;
    }
    return res;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) { 
    // DUMMY_CODE(len);
    size_t strlen = min(len,buffer_size()); 
    _tail += strlen;
    // tail ---------------- head
    if (_tail >= _capacity)
        _tail -= _capacity;
    _read_size += strlen;
    _remain_capacity += strlen;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    // DUMMY_CODE(len);
    string res = peek_output(len);
    pop_output(len);
    
    return res;
}

void ByteStream::end_input() { this->_end_input = true; } // 由别人来调用

bool ByteStream::input_ended() const { return this->_end_input; }

size_t ByteStream::buffer_size() const { return this->_capacity - _remain_capacity; }
    
bool ByteStream::buffer_empty() const { return _remain_capacity == _capacity; }

bool ByteStream::eof() const { return _end_input && _head == _tail; }

size_t ByteStream::bytes_written() const { return this->_written_size; }

size_t ByteStream::bytes_read() const { return this->_read_size; }

size_t ByteStream::remaining_capacity() const { return this->_remain_capacity; }
