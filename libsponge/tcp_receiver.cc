#include "tcp_receiver.hh"
#include <iostream>
#include <util.hh>
// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    // DUMMY_CODE(seg);
    /*
    1.设置ISN
    2.push data to the _reassembler
        如何确定 byte 的 absolute ackno index如何确定
        通过报文中的 seqno 来确定
    */ 
    if (seg.header().syn == 1){ // then set isn   
        auto rd = get_random_generator();    
        uniform_int_distribution<uint32_t> dist32{0, numeric_limits<uint32_t>::max()};
        const WrappingInt32 isn{dist32(rd)};
        _isn = isn;
    }
    string data = seg.payload().copy(); // 是否需要判断 capacity
    // if (data.size() > _capacity) 无需处理，push里面会丢弃多的部分
        
    _reassembler.push_substring(data,unwrap(seg.header().seqno,_isn,_checkpoint),seg.header().fin);
}
// 返回接受方想要接受的第一个字节
optional<WrappingInt32> TCPReceiver::ackno() const { return _reassembler._curIndex; }

size_t TCPReceiver::window_size() const { return _reassembler.stream_out()._remain_capacity; }
