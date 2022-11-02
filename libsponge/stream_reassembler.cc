#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) :_auxiliary(),_check(),_curIndex(0),_count(0),_unassembled_bytes(0), _output(capacity), _capacity(capacity),_last_assembled_index(0),_last_btye_index(-1){
    _auxiliary.resize(capacity);
    _check.resize(capacity);
}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    // DUMMY_CODE(data, index, eof);

   // restart 重新思考，重新
   /*
    1.存储数据 store data to _auxiliaru
    存的测试:
        _capacity 1 
        ab 0  --> b被丢弃，a被写入 _last = 1
        abc 0 --> b被写入 _last = 2
        _capacity 8
        abc 0  -->  abc写入 ，_last = 3
        ghx 6  -->  gh 存入， X丢弃
        cdef 2 --> 2 < _last c拒绝写入 d 3 + 1 - 4 = 0
         _capacity 8
         ghx 6 --> gh
         cdef 2 --> 写入
         abc 0 --> ab
   */
    if (eof)
        _last_btye_index = index + data.size(); 
    size_t last = _last_assembled_index;
   for (size_t i = 0;i < data.size();++ i){
        if (index + i >= _last_assembled_index){ // 不要把已经发过的数据存进来了
            if (_unassembled_bytes == _capacity) // 满了就丢弃
                break;
            if (!_check[(index + i) % _capacity]){
                if (_last_assembled_index > (index+i)%_capacity){ // 如果最后一个的位置 超出了 容量
                    _auxiliary[(index + i) % _capacity] = data[i];
                    _check[(index + i) % _capacity] = true;
                }
                else if (index + i >= _capacity) // 如果 最后一个下标前面还没发，那么说明这里超了，需要全部丢掉
                    break;
                else{
                    _auxiliary[index + i] = data[i];
                    _check[index + i] = true;
                }
                ++_unassembled_bytes;
                
                while (index + i == last){
                    ++_count;
                    ++last;  // a b
                }
            }
        }
    }
    while(_check[last % _capacity] && _count < _unassembled_bytes){
        ++_count;
        ++last;
    }
    // cout << "lastasm index:" << _last_assembled_index << " curIndex:" << _curIndex << " index:" << index << " count:" << _count << " data:" << data << endl;
    if (_curIndex >= index % _capacity){
        string res = "";
        while(_count && _capacity - _output.buffer_size() > res.size()){
            _check[_curIndex] = false;
            res += _auxiliary[_curIndex++];
            _auxiliary[_curIndex-1] = '\0';
            --_unassembled_bytes;
            --_count;
            ++_last_assembled_index;
            if (_curIndex == _capacity)
                _curIndex = 0;
        }
        _output.write(res);
        // cout << "write:" << _output.write(res) << " _remain:" << _capacity - _output.buffer_size() << endl;
        // cout << "res:" << res << " write:" << _output.write(res) << " _remain:" << _capacity - _output.buffer_size() << endl;
        if (_last_assembled_index >= _last_btye_index)
            _output.end_input();
    }

}

size_t StreamReassembler::unassembled_bytes() const { return _unassembled_bytes; }

bool StreamReassembler::empty() const { return _unassembled_bytes == 0; }
