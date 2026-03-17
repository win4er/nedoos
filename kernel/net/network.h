#ifndef NETWORK_H
#define NETWORK_H

#include <stdint.h>

#define ETH_TYPE_IP  0x0800
#define ETH_TYPE_ARP 0x0806

#define IP_PROTO_ICMP 1
#define IP_PROTO_TCP  6
#define IP_PROTO_UDP  17

#define ARP_OP_REQUEST 1
#define ARP_OP_REPLY   2

// Ethernet frame
typedef struct {
    uint8_t dest_mac[6];
    uint8_t src_mac[6];
    uint16_t type;
    uint8_t payload[];
} __attribute__((packed)) eth_frame_t;

// ARP packet
typedef struct {
    uint16_t hw_type;
    uint16_t proto_type;
    uint8_t hw_len;
    uint8_t proto_len;
    uint16_t opcode;
    uint8_t sender_mac[6];
    uint32_t sender_ip;
    uint8_t target_mac[6];
    uint32_t target_ip;
} __attribute__((packed)) arp_packet_t;

// IP header
typedef struct {
    uint8_t version_ihl;
    uint8_t dscp_ecn;
    uint16_t total_len;
    uint16_t id;
    uint16_t flags_frag;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t checksum;
    uint32_t src_ip;
    uint32_t dest_ip;
} __attribute__((packed)) ipv4_header_t;

// ICMP header
typedef struct {
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    uint16_t id;
    uint16_t sequence;
} __attribute__((packed)) icmp_header_t;

// ARP table entry
typedef struct {
    uint32_t ip;
    uint8_t mac[6];
    int age;
} arp_entry_t;

// Network functions
void network_init(void);
void network_handler(void);
void arp_request(uint32_t ip);
void arp_handler(arp_packet_t* arp);
void ipv4_handler(uint8_t* packet, uint16_t len);
void icmp_handler(icmp_header_t* icmp, uint32_t src_ip, uint16_t len);

#endif
