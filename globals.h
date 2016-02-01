#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <stdbool.h>
#include <string.h>
#include <linux/if_ether.h>
#include <netinet/ip.h>    //Provides declarations for ip header
#include <pthread.h>


#define NODE_MAX 50


// Router Globals.h 
struct route_entry{
    
    uint16_t address;
    uint16_t mask;
    int send_index;
};


struct global_vars {
    
    int send_sock_fd;
    struct route_entry routing_table[NODE_MAX];
    int sockets[10];
    uint16_t my_addr;
    int numports;
    //Need some struct here probably to be filled with keys of adjacent nodes 
};
struct gyn_header
{
    uint16_t dest; //destination
    uint16_t src;
    uint16_t seq;
    uint16_t pkt_size; // bytes. payload only (starting at byte 18)
    uint16_t pkt_type;
    uint16_t unused;
    uint16_t ether_type; // 0x0101 for normal (encrypted) traffic. 0x0102 for key exchange data (unencrypted header)
    uint32_t crypto_padding; //cryptographic padding, used for 0x0101 ether_types. all 0's for key exchange
};

struct route_entry_for_file{
    char iface[10];
    uint32_t address;
    uint32_t nexthop;
    uint32_t dist;
    int send_index;
};
extern struct global_vars vars; //singleton object
extern struct globals globals;
extern struct route_entry_for_file nodes_data[NODE_MAX];
extern int nodes_data_index;