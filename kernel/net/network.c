#include "network.h"
#include "../kernel.h"
#include "../drivers/e1000.h"
#include <stdint.h>
#include <stddef.h>

// Глобальные переменные
uint8_t my_mac[6] = {0x52, 0x54, 0x00, 0x12, 0x34, 0x56};
uint32_t my_ip = 0x0A00020F; // 10.0.2.15
uint32_t gateway_ip = 0x0A000202; // 10.0.2.2

// ARP table
static arp_entry_t arp_table[16];
static int arp_table_count = 0;

// Ping request tracking
#define MAX_PINGS 3
static struct {
    uint32_t ip;
    uint16_t id;
    uint16_t seq;
    int tries;
    int waiting;
} ping_request;

// Network byte order functions
static uint16_t ntohs(uint16_t n) {
    return (n >> 8) | (n << 8);
}

static uint16_t htons(uint16_t n) {
    return ntohs(n);
}

static uint32_t ntohl(uint32_t n) {
    return (n >> 24) | ((n >> 8) & 0xFF00) | ((n << 8) & 0xFF0000) | (n << 24);
}

static uint32_t htonl(uint32_t n) {
    return ntohl(n);
}

// Calculate checksum
static uint16_t checksum(uint16_t* addr, int len) {
    uint32_t sum = 0;
    
    for (int i = 0; i < len / 2; i++) {
        sum += addr[i];
    }
    
    if (len & 1) {
        sum += ((uint8_t*)addr)[len-1];
    }
    
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    
    return ~sum;
}

// Print IP address
void print_ip(uint32_t ip) {
    print_dec(ip & 0xFF);
    print(".");
    print_dec((ip >> 8) & 0xFF);
    print(".");
    print_dec((ip >> 16) & 0xFF);
    print(".");
    print_dec((ip >> 24) & 0xFF);
}

// Print MAC address
static void print_mac(uint8_t* mac) {
    for (int i = 0; i < 6; i++) {
        print_hex(mac[i]);
        if (i < 5) print(":");
    }
}

// Send Ethernet frame
static void ethernet_send(uint8_t* dest_mac, uint16_t type, uint8_t* data, uint16_t len) {
    uint8_t buffer[1514];
    eth_frame_t* eth = (eth_frame_t*)buffer;
    
    for (int i = 0; i < 6; i++) {
        eth->dest_mac[i] = dest_mac[i];
        eth->src_mac[i] = my_mac[i];
    }
    
    eth->type = htons(type);
    
    for (int i = 0; i < len; i++) {
        eth->payload[i] = data[i];
    }
    
    e1000_send_packet(buffer, sizeof(eth_frame_t) + len);
}

// Lookup MAC for IP
static uint8_t* arp_lookup(uint32_t ip) {
    for (int i = 0; i < arp_table_count; i++) {
        if (arp_table[i].ip == ip) {
            return arp_table[i].mac;
        }
    }
    return NULL;
}

// Send IP packet (определена ДО того, как используется)
static void ipv4_send(uint32_t dest_ip, uint8_t protocol, uint8_t* data, uint16_t len) {
    uint8_t buffer[1514];
    ipv4_header_t* ip = (ipv4_header_t*)buffer;
    
    ip->version_ihl = 0x45;
    ip->dscp_ecn = 0;
    ip->total_len = htons(sizeof(ipv4_header_t) + len);
    ip->id = htons(1);
    ip->flags_frag = 0;
    ip->ttl = 64;
    ip->protocol = protocol;
    ip->checksum = 0;
    ip->src_ip = htonl(my_ip);
    ip->dest_ip = htonl(dest_ip);
    
    ip->checksum = checksum((uint16_t*)ip, sizeof(ipv4_header_t));
    
    for (int i = 0; i < len; i++) {
        buffer[sizeof(ipv4_header_t) + i] = data[i];
    }
    
    uint8_t* dest_mac = arp_lookup(dest_ip);
    if (!dest_mac) {
        arp_request(dest_ip);
        return;
    }
    
    ethernet_send(dest_mac, ETH_TYPE_IP, buffer, sizeof(ipv4_header_t) + len);
}

// ICMP: send echo reply
static void icmp_send_reply(icmp_header_t* icmp, uint32_t src_ip, uint16_t len) {
    icmp->type = 0;
    icmp->code = 0;
    icmp->checksum = 0;
    icmp->checksum = checksum((uint16_t*)icmp, len);
    
    ipv4_send(src_ip, IP_PROTO_ICMP, (uint8_t*)icmp, len);
}

// ICMP: send echo request (определена ДО arp_handler)
static void icmp_send_request(uint32_t dest_ip, uint16_t id, uint16_t seq) {
    uint8_t packet[64];
    icmp_header_t* icmp = (icmp_header_t*)packet;
    
    icmp->type = 8;
    icmp->code = 0;
    icmp->checksum = 0;
    icmp->id = htons(id);
    icmp->sequence = htons(seq);
    
    for (int i = 0; i < 56; i++) {
        packet[sizeof(icmp_header_t) + i] = i;
    }
    
    icmp->checksum = checksum((uint16_t*)icmp, sizeof(icmp_header_t) + 56);
    
    ipv4_send(dest_ip, IP_PROTO_ICMP, packet, sizeof(icmp_header_t) + 56);
}

// ARP: send request
void arp_request(uint32_t ip) {
    arp_packet_t arp;
    
    arp.hw_type = htons(1);
    arp.proto_type = htons(0x0800);
    arp.hw_len = 6;
    arp.proto_len = 4;
    arp.opcode = htons(ARP_OP_REQUEST);
    
    for (int i = 0; i < 6; i++) {
        arp.sender_mac[i] = my_mac[i];
        arp.target_mac[i] = 0;
    }
    
    arp.sender_ip = htonl(my_ip);
    arp.target_ip = htonl(ip);
    
    uint8_t broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    ethernet_send(broadcast, ETH_TYPE_ARP, (uint8_t*)&arp, sizeof(arp));
    print("ARP: Request sent for ");
    print_ip(ip);
    print("\n");
}

// ARP: handle incoming packet
static void arp_handler(arp_packet_t* arp) {
    uint16_t opcode = ntohs(arp->opcode);
    uint32_t sender_ip = ntohl(arp->sender_ip);
    uint32_t target_ip = ntohl(arp->target_ip);
    
    // Add to ARP table
    if (arp_table_count < 16) {
        arp_table[arp_table_count].ip = sender_ip;
        for (int i = 0; i < 6; i++) {
            arp_table[arp_table_count].mac[i] = arp->sender_mac[i];
        }
        arp_table[arp_table_count].age = 0;
        arp_table_count++;
    }
    
    // If request for our IP, send reply
    if (opcode == ARP_OP_REQUEST && target_ip == my_ip) {
        arp_packet_t reply;
        
        reply.hw_type = htons(1);
        reply.proto_type = htons(0x0800);
        reply.hw_len = 6;
        reply.proto_len = 4;
        reply.opcode = htons(ARP_OP_REPLY);
        
        for (int i = 0; i < 6; i++) {
            reply.sender_mac[i] = my_mac[i];
            reply.target_mac[i] = arp->sender_mac[i];
        }
        
        reply.sender_ip = htonl(my_ip);
        reply.target_ip = htonl(sender_ip);
        
        ethernet_send(arp->sender_mac, ETH_TYPE_ARP, (uint8_t*)&reply, sizeof(reply));
        print("ARP: Reply sent to ");
        print_ip(sender_ip);
        print("\n");
    }
    
    // If this is a reply to our ping request
    if (opcode == ARP_OP_REPLY && ping_request.waiting && sender_ip == ping_request.ip) {
        print("ARP: Got reply, sending ping...\n");
        icmp_send_request(htonl(sender_ip), ping_request.id, ping_request.seq);
    }
}

// ICMP handler
static void icmp_handler(icmp_header_t* icmp, uint32_t src_ip, uint16_t len) {
    if (icmp->type == 8) { // Echo request
        print("ICMP: Ping from ");
        print_ip(src_ip);
        print("\n");
        icmp_send_reply(icmp, src_ip, len);
    } else if (icmp->type == 0) { // Echo reply
        if (ping_request.waiting && src_ip == ping_request.ip) {
            print("ICMP: Reply from ");
            print_ip(src_ip);
            print(" seq=");
            print_dec(ntohs(icmp->sequence));
            print("\n");
            ping_request.waiting = 0;
        }
    }
}

// IP handler
static void ipv4_handler(uint8_t* packet, uint16_t len) {
    ipv4_header_t* ip = (ipv4_header_t*)packet;
    uint32_t dest_ip = ntohl(ip->dest_ip);
    
    if (dest_ip != my_ip && dest_ip != 0xFFFFFFFF) {
        return;
    }
    
    uint16_t total_len = ntohs(ip->total_len);
    uint8_t* data = packet + sizeof(ipv4_header_t);
    uint16_t data_len = total_len - sizeof(ipv4_header_t);
    
    switch (ip->protocol) {
        case IP_PROTO_ICMP:
            icmp_handler((icmp_header_t*)data, ntohl(ip->src_ip), data_len);
            break;
    }
}

// Ethernet handler
static void ethernet_handler(uint8_t* packet, uint16_t len) {
    eth_frame_t* eth = (eth_frame_t*)packet;
    uint16_t type = ntohs(eth->type);
    
    switch (type) {
        case ETH_TYPE_ARP:
            arp_handler((arp_packet_t*)eth->payload);
            break;
        case ETH_TYPE_IP:
            ipv4_handler(eth->payload, len - sizeof(eth_frame_t));
            break;
    }
}

// Network handler (called from kernel main loop)
void network_handler(void) {
    uint8_t buffer[2048];
    int len = e1000_recv_packet(buffer);
    
    if (len > 0) {
        ethernet_handler(buffer, len);
    }
}

// Network initialization
void network_init(void) {
    print("Network: Initializing...\n");
    e1000_init(0xF0000000, 11);
    print("Network: Ready. IP=");
    print_ip(my_ip);
    print("\n");
    
    ping_request.waiting = 0;
}

// Ping command
void ping(const char* ip_str) {
    uint32_t ip = 0;
    int octet = 0;
    int shift = 24;
    
    for (int i = 0; ip_str[i]; i++) {
        if (ip_str[i] >= '0' && ip_str[i] <= '9') {
            octet = octet * 10 + (ip_str[i] - '0');
        } else if (ip_str[i] == '.') {
            ip |= (octet << shift);
            shift -= 8;
            octet = 0;
        }
    }
    ip |= (octet << shift);
    
    print("Pinging ");
    print_ip(ip);
    print("...\n");
    
    ping_request.ip = ip;
    ping_request.id++;
    ping_request.seq = 1;
    ping_request.tries = 0;
    ping_request.waiting = 1;
    
    arp_request(ip);
}

// Ifconfig command
void ifconfig(void) {
    print("eth0: ");
    print_mac(my_mac);
    print("  IP: ");
    print_ip(my_ip);
    print("  Gateway: ");
    print_ip(gateway_ip);
    print("\n");
    
    print("ARP table:\n");
    for (int i = 0; i < arp_table_count; i++) {
        print("  ");
        print_ip(arp_table[i].ip);
        print(" -> ");
        print_mac(arp_table[i].mac);
        print("\n");
    }
}

// Netcat command (simplified UDP/TCP tool)
void netcat(const char* args) {
    print("Netcat: ");
    print(args);
    print("\n");
    print("Usage: nc <host> <port>\n");
}
