#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time_since_last_segment_received; }

void TCPConnection::segment_received(const TCPSegment &seg) { 
    // 接受到数据需要进行的判断
    /*
    2.rst
    1.syn
    3.fin
    4.ack
    */
    _need_empty_ack = seg.length_in_sequence_space();

    _time_since_last_segment_received = 0; // 清零计时器
    _receiver.segment_received(seg); // 让receiver来处理，因为足够鲁棒应该不需要考虑其他的东西
    
    if (seg.header().rst){ // 判断 rst 包 对于rst的处理，就直接永久关闭连接
        _set_rst(false);
        return ;
    }

    if (seg.header().ack){ // 判断 ack
        _sender.ack_received(seg.header().ackno,seg.header().win);
        if (_need_empty_ack && !_sender.segments_out().empty()) // 可以捎带，就不用单独发个空包
            _need_empty_ack = false;
    }


    //? 接下来就是对某些个别的状态迁移的管理
    // server receiver is listen && sender is closed
    // because we at first deal with the seg so the _receiver in advance into the SYN_RECV form LISTEN
    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::SYN_RECV
    &&  TCPState::state_summary(_sender)   == TCPSenderStateSummary::CLOSED){
        // server need connect
        connect();
        return ;
        // 如果是 client ，是自己调用 connecct
    }

    // close wait 当 输入 早于 输出结束，就不用 lingering
    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV
    &&  TCPState::state_summary(_sender)   == TCPSenderStateSummary::SYN_ACKED){
        _linger_after_streams_finish = false;
    }

    // closed
    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV
    &&  TCPState::state_summary(_sender)   == TCPSenderStateSummary::FIN_ACKED
    &&  !_linger_after_streams_finish){
        _active = false; // 半关闭状态？
        return ;
    }
    // send 留到后面判断，可能会影响到前面的状态判断
    if (_receiver.ackno().has_value() and (seg.length_in_sequence_space() == 0) // respond to keep-alive data
        and seg.header().seqno == _receiver.ackno().value() - 1) {
        _sender.send_empty_segment();
    }
    if (_need_empty_ack)
        _sender.send_empty_segment();

    _trans_data();
}

bool TCPConnection::active() const { return _active; }

size_t TCPConnection::write(const string &data) {
    // DUMMY_CODE(data);
    /*
    1.sender.write
    2.sender.fillwindow
    */
    size_t writtensize = _sender.stream_in().write(data);
    _sender.fill_window();
    _trans_data();
    return writtensize;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    _sender.tick(ms_since_last_tick); // tell sender aboute pass time
    if (_sender.consecutive_retransmissions() > _cfg.MAX_RETX_ATTEMPTS){
        // abort connection
        // send rst seg
        _set_rst(true);
        return ;
    }

    _time_since_last_segment_received += ms_since_last_tick;

    // time wait 超时了，就断开连接，不要等了，对 remote peer 已经收到 ACK 充分自信了
    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV
    &&  TCPState::state_summary(_sender)   == TCPSenderStateSummary::FIN_ACKED
    &&  _linger_after_streams_finish
    && _time_since_last_segment_received >= 10 * _cfg.rt_timeout){
        _active = false;
        _linger_after_streams_finish = false;
    }
    _trans_data(); // 如果没有超过一定次数，就继续重传

}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input(); // shutdown output
    _sender.fill_window();
    _trans_data();
}

void TCPConnection::connect() {
    // 1.发送SYN报文
    // 2.填充 out stream 的 window
    _sender.fill_window(); // fill window 会 设置 syn 以及 处理 fin
    _active = true; // 激活状态
    _trans_data(); // 发送数据
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            _set_rst(true); // 发送 rst segment to the peer
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
// when meet rst seg，then call _set_rst
void TCPConnection::_set_rst(bool rst){
    if (rst){ // 如果是要发送 rst segment
    // sendrst
        TCPSegment rstseg;
        rstseg.header().rst = true;
        _segments_out.push(rstseg);        
    }
    // 先发送再设置为 error 状态
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
    _linger_after_streams_finish = false;
    _active = false;
}

void TCPConnection::_trans_data(){
    while (!_sender.segments_out().empty()){
        TCPSegment seg = _sender.segments_out().front();
        _sender.segments_out().pop();
        if (_receiver.ackno().has_value()){
            seg.header().ack = 1;
            seg.header().ackno = _receiver.ackno().value(); // 因为返回的是 optional
            seg.header().win = _receiver.window_size();
        }


        _segments_out.push(seg);
    }
}