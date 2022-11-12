#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>
#include <iostream>
// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity) 
    , _timeout(retx_timeout) {}

uint64_t TCPSender::bytes_in_flight() const { return _output_bytes; }
//? fill_window() 
void TCPSender::fill_window() {
    // 1.从 input bytestream 里面去读数据，然后尽可能多发送数据
    size_t curwindowsize = _window_size == 0 ? 1 : _window_size;
    
    while (curwindowsize > _output_bytes){ // 填充
        TCPSegment seg;
        // 判断 syn
        if (!_set_syn){
            _set_syn = true;
            seg.header().syn = true;
        }
        // 设置 seqno
        seg.header().seqno = next_seqno(); //! 是否需要对 seqno ++？

        // 设置 payload
        const size_t payload_size =
            min(TCPConfig::MAX_PAYLOAD_SIZE, curwindowsize - _output_bytes - seg.header().syn);
        string payload = _stream.read(payload_size);
        // 判断 Fin  是否读完了 以及 现在是否有余量来放 fin bytes 因为 fin 也要占 1个 byte
        if(!_set_fin && _stream.eof() && payload.size() + _output_bytes < curwindowsize){
            _set_fin = seg.header().fin = true;
        }
        seg.payload() = Buffer(move(payload));

        
        if (_outstanding_seg.empty()){
            _timeout = _initial_retransmission_timeout;
            _timeout_count = 0;
        }
        // 判断 有无 数据
        if (seg.length_in_sequence_space() == 0)
            break;
         // 压入队列中,发送出去
        _segments_out.push(seg);

        // 追踪数据包 _outstanding_seg
        _output_bytes += seg.length_in_sequence_space(); // 记录正在发送但未被承认的字节
        _outstanding_seg[_next_seqno] = seg; // 这里没用 insert 可能埋下了一个伏笔

        // 更新 next_seqno
        _next_seqno += seg.length_in_sequence_space();

        if (seg.header().fin == 1) // 如果是最后一次，就不必再 fill window 了
            break;
    }
}
//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) { 
    // DUMMY_CODE(ackno, window_size);
    _retransmission_count = 0;
    _window_size = window_size;
    uint64_t absolute_ackno = unwrap(ackno,_isn,_next_seqno); // _next_seqno 作为 checkpoint
    if (absolute_ackno > _next_seqno) // 传过来不可靠，因为根本还没发送这个 ackno 的字节
        return ;
    for(auto iter = _outstanding_seg.begin();iter != _outstanding_seg.end();){
        size_t no = iter->first;
        TCPSegment seg = iter->second;
        if (no + seg.length_in_sequence_space() <= absolute_ackno){ // 移除 ackno 之前，代表已经接受到了
            _output_bytes -= seg.length_in_sequence_space();
            iter = _outstanding_seg.erase(iter); // 删掉并更新 iter

            // 更新重传时间以及重传次数
            _timeout = _initial_retransmission_timeout;
            _timeout_count = 0;
        }
        else
            break;
    }
    fill_window(); // 再次 call  fill_window
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    _timeout_count += ms_since_last_tick;

    auto iter = _outstanding_seg.begin();
    if (iter != _outstanding_seg.end() && _timeout_count >= _timeout){
        if (_window_size > 0)
            _timeout *= 2;
        _segments_out.push(iter->second);
        _timeout_count = 0;
        _retransmission_count++;
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _retransmission_count; }

void TCPSender::send_empty_segment() {
    TCPSegment  emptyseg;
    emptyseg.header().seqno = next_seqno();
    _segments_out.push(emptyseg);
}
