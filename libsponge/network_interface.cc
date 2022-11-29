#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();
    // 看 map 里面有没有这个的映射
    if (_map.find(next_hop_ip) != _map.end()){ // if find send it
        EthernetFrame eth;
        
        eth.header().type = EthernetHeader::TYPE_IPv4; // 设为 ipv4 type
        eth.header().dst = _map[next_hop_ip].second; // set dst
        eth.header().src = _ethernet_address;
        eth.payload() = dgram.serialize();
        // send it
        _frames_out.push(eth);
    }
    else{
        // if not find broadcast
        // and arp 还没有过期！
        if (_wait_arp.find(next_hop_ip) != _wait_arp.end())
            return ;
        ARPMessage arp;
        arp.sender_ip_address = _ip_address.ipv4_numeric();
        arp.sender_ethernet_address = _ethernet_address;
        // arp.target_ethernet_address = ETHERNET_BROADCAST; 以太网设置成广播就行了
        arp.target_ip_address = next_hop_ip;
        arp.opcode = ARPMessage::OPCODE_REQUEST; // 广播 请求
        _wait_arp[next_hop_ip] = 0;
        _wait_internetgram.push_back({next_hop,dgram});
        EthernetFrame arp_broadcast;
        arp_broadcast.payload() = arp.serialize();
        arp_broadcast.header().src = _ethernet_address;
        arp_broadcast.header().dst = ETHERNET_BROADCAST;
        arp_broadcast.header().type =  EthernetHeader::TYPE_ARP;
        _frames_out.push(arp_broadcast);
    }
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    if (frame.header().dst != _ethernet_address && frame.header().dst != ETHERNET_BROADCAST)
        return nullopt;
    if (frame.header().type == EthernetHeader::TYPE_IPv4){ // 如果是 IPv4 直接接受
        InternetDatagram ipv4;
        if (ipv4.parse(frame.payload()) == ParseResult::NoError)
            return ipv4;
    }else if (frame.header().type == EthernetHeader::TYPE_ARP){
        ARPMessage arp;
        if (arp.parse(frame.payload()) != ParseResult::NoError)
            return nullopt;
        if ((arp.opcode == ARPMessage::OPCODE_REPLY && arp.target_ethernet_address == _ethernet_address)
         ||  (arp.opcode == ARPMessage::OPCODE_REQUEST && arp.target_ip_address == _ip_address.ipv4_numeric())){ 
            // 确定是 给自己的 arp
            // 1.update the map
            _map[arp.sender_ip_address] = {0,arp.sender_ethernet_address};
            // 2.发送 reply 的ip数据包
            if (_wait_arp.find(arp.sender_ip_address) != _wait_arp.end()){
            // find the not send data
                for (auto it = _wait_internetgram.begin();it != _wait_internetgram.end();){
                    if (it->first.ipv4_numeric() == arp.sender_ip_address){
                        send_datagram(it->second,it->first);
                        it = _wait_internetgram.erase(it);
                    }else
                        ++it;
                }
                _wait_arp.erase(arp.sender_ip_address);
            } 
        }
            
        if (arp.opcode == ARPMessage::OPCODE_REQUEST){ // reply
            if (arp.target_ip_address == _ip_address.ipv4_numeric()){ // find target ip
                ARPMessage arp_reply;
                arp_reply.sender_ip_address = _ip_address.ipv4_numeric();
                arp_reply.sender_ethernet_address = _ethernet_address;
                arp_reply.target_ethernet_address = arp.sender_ethernet_address;
                arp_reply.target_ip_address = arp.sender_ip_address;
                arp_reply.opcode = ARPMessage::OPCODE_REPLY;
                EthernetFrame eth;
                eth.header().src = _ethernet_address;
                eth.header().dst = arp.sender_ethernet_address;
                eth.header().type = EthernetHeader::TYPE_ARP;
                eth.payload() = arp_reply.serialize();
                
                _frames_out.push(eth);
            }
        }
    }
    return nullopt;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) { 
    for (auto it = _map.begin();it != _map.end();){
        it->second.first += ms_since_last_tick;
        if (it->second.first >= _arp_default_ddl)
            it = _map.erase(it);
        else{
            ++it;
        }
    }

    for (auto it = _wait_arp.begin();it != _wait_arp.end();){
        it->second += ms_since_last_tick;
        if (it->second >= _arp_wait_time){
            ARPMessage arp_request;
            arp_request.opcode = ARPMessage::OPCODE_REQUEST;
            arp_request.sender_ethernet_address = _ethernet_address;
            arp_request.sender_ip_address = _ip_address.ipv4_numeric();
            arp_request.target_ethernet_address = {/* 这里应该置为空*/};
            arp_request.target_ip_address = it->first;

            EthernetFrame eth_frame;
            eth_frame.header() = {/* dst  */ ETHERNET_BROADCAST,
                                  /* src  */ _ethernet_address,
                                  /* type */ EthernetHeader::TYPE_ARP};
            eth_frame.payload() = arp_request.serialize();
            _frames_out.push(eth_frame);

            it->second = 0;
        }
        else
            ++it;
    }
}
