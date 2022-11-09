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
        _isn = seg.header().seqno; // 因为 seqno 就是 一个 WrapppingInt32
        _syn = 1;
    }
    if (_syn == 0)
        return;
    string data = seg.payload().copy(); // 是否需要判断 capacity
    // if (data.size() > _capacity) 无需处理，push里面会丢弃多的部分
    uint64_t absolute_ackno = unwrap(seg.header().seqno,_isn,_reassembler.stream_out().bytes_written() + 1) - 1 + seg.header().syn;
    
    _reassembler.push_substring(data,absolute_ackno,seg.header().fin);
    // if (seg.header().fin == 1){
    //     _reassembler.stream_out().input_ended(); 不用调因为在 push 里面判断了的
    // }
}
// 返回接受方想要接受的第一个字节 通过 byteWriten来完成 curIndex为什么不能通过这个来完成？
optional<WrappingInt32> TCPReceiver::ackno() const {
    if (_syn){
        uint64_t absolute_ackno = _reassembler.stream_out().bytes_written();
        if (_reassembler.stream_out().input_ended())
            return wrap(absolute_ackno+2,_isn);
        else
            return wrap(absolute_ackno+1,_isn);
    }
    else
        return nullopt;
}

size_t TCPReceiver::window_size() const { return _capacity - _reassembler.stream_out().buffer_size(); }
