#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
//#include <linux/if_ether.h>
#include <arpa/inet.h>
#include <linux/if_packet.h>
#include <netinet/ip.h> 
#include <netinet/ether.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <net/if.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include "globals.h"

struct route_entry_for_file nodes_data[50];
struct global_vars vars;
struct route_entry_for_file routing_data[NODE_MAX];
// For sending
int router_id;
int own_nodes_count;
struct sockaddr_ll sockaddrll;
int my_nodes[NODE_MAX];
int my_nodes_distance[NODE_MAX];
int nodes_data_index;
// For receving
int received_my_nodes[NODE_MAX] ={0};
int received_my_nodes_distance[NODE_MAX] = {0};

// Usage:
// sudo ./rip Router# Node_count Router_count node1,2,3... router1,2,3....
//              [1]      [2]          [3]       [4]....
// limit routers ID 1-10
// Each router will have its nodes ID correspond to its Router ID.
// For example: Router 2 has 3 nodes. Then three nodes will be set at 20,21,22 in order of interface specified
//              Router 1 has 2 nodes. Its node is at 10,11

void init_send_socket(){
    int fd = socket(AF_PACKET,SOCK_RAW,htons(ETH_P_ALL));
    if(fd<0){
        perror("Error creating socket\n");
    }   
    vars.send_sock_fd=fd;
    printf("Send Socket initialized....\n");
}

void init_recv_sockets(char* iface, int index){
    int sock = socket(AF_PACKET,SOCK_RAW,htons(ETH_P_ALL));
    vars.sockets[index]=sock;
    struct sockaddr_ll raw_address;
    struct ifreq ifr;
    raw_address.sll_family=AF_PACKET;
    raw_address.sll_protocol=htons(ETH_P_ALL);
    memset(&ifr,0,sizeof(struct ifreq));
    strncpy(ifr.ifr_name,iface,IFNAMSIZ-1);
    ioctl(sock,SIOCGIFINDEX,&ifr);
    raw_address.sll_ifindex=ifr.ifr_ifindex;
    routing_data[index].send_index=ifr.ifr_ifindex;
    strncpy((char*)&routing_data[index].iface, iface, 10);
    printf("Routing table index:%d\n", index);
    printf("Setting socket interface %s at interface index:%d\n", iface, routing_data[index].send_index);
    int bind1=bind(sock,(const struct sockaddr*)&raw_address,sizeof(struct sockaddr_ll));       
    if(bind1<0){
        printf("bind issue\n");
    }
    else{
//      printf("Listener bound to %s.....\n",iface);
    }
    /*Set sock to promiscuous*/
    struct packet_mreq mr;
    memset(&mr,0,sizeof(mr));
    mr.mr_ifindex=ifr.ifr_ifindex;
    mr.mr_type=PACKET_MR_PROMISC;
    if(setsockopt(sock,SOL_PACKET,PACKET_ADD_MEMBERSHIP,&mr,sizeof(mr))<0){
        perror("Setsockopt(PROMISC) failed");
    }
    else{
//      printf("Interface %s set to promiscuous....\n",iface);
    }
    fcntl(sock,F_SETFL,O_NONBLOCK);
}

void *loop_send(){
    // Packet processing ======================================
    unsigned char rip_update_packet[1024];
    // Set the header, we are only checking for ether_type
    struct gyn_header rip_packet_header;
    rip_packet_header.ether_type = 0x1234;
    while(1){
        // Build the route update packet
        // Node processing ========================================
        printf("\n=============Inside sending thread===============\n");
        int j;
        printf("Sending packet with destination nodes at [");
        for (j=0;j<nodes_data_index;j++){
            my_nodes[j] = nodes_data[j].address;
            my_nodes_distance[j] = nodes_data[j].dist;
            printf(" %d", my_nodes[j]);
        }
        printf(" ]\n");
        // Packet should include my router ID and my nodes' ID
        memcpy(rip_update_packet, &rip_packet_header, sizeof(struct gyn_header));
        memcpy(&rip_update_packet[sizeof(struct gyn_header)], &router_id, sizeof(int));
        memcpy(&rip_update_packet[sizeof(struct gyn_header) + sizeof(int)], &nodes_data_index, sizeof(int));    // Node count
        memcpy(&rip_update_packet[sizeof(struct gyn_header) + (2 * sizeof(int))], &my_nodes, sizeof(my_nodes));
        memcpy(&rip_update_packet[sizeof(my_nodes) +sizeof(struct gyn_header) + (2 * sizeof(int))], &my_nodes_distance, sizeof(my_nodes_distance));

        // Send update packets to other routers
        int i;
        for(i=0;i<vars.numports;i++){
            // Get the interface index from global vars, this is acquired at init_recv_sockets()
            sockaddrll.sll_ifindex = routing_data[i].send_index;
            //printf("Sending to Router at interface:%d\n",sockaddrll.sll_ifindex);
            int bytes_sent=0;
            bytes_sent = sendto(vars.send_sock_fd,rip_update_packet,sizeof(rip_update_packet),0,(struct sockaddr*)&sockaddrll,sizeof(struct sockaddr_ll));
            //printf("bytes_sent: %d\n",bytes_sent);
            if (bytes_sent < 0){
                perror("Sending:");
            }
        }
        printf("===================================================\n");
        sleep(3);
    }
}

void *loop_refresh(){
    // Packet processing ======================================
    int my_original_nodes[NODE_MAX];
    int my_original_nodes_distance[NODE_MAX];
    while(1){
        // Node processing ========================================
        printf("\n*************Inside refreshing thread***********\n");
        int j;
        // Delete all nodes except my own node
        for (j=own_nodes_count; j < nodes_data_index; j++){
            strncpy((char*)&nodes_data[j].iface, "", 10);
            nodes_data[j].address = 0;
            nodes_data[j].dist = 0;
            nodes_data[j].send_index = 0;
            nodes_data[j].nexthop = 0;
        }
        printf("Recreating Network Map... Done\n");
        nodes_data_index = own_nodes_count;
        printf("Current Network Map:\n");
        for (j=0;j<nodes_data_index;j++){
            my_original_nodes[j] = nodes_data[j].address;
            my_original_nodes_distance[j] = nodes_data[j].dist;
            printf("\tNodes[%d]: Destination:%d Distance:%d\n", j, my_original_nodes[j], my_original_nodes_distance[j]);
        }
        printf("************************************************\n\n");
        sleep(12);
    }
}

void insert_my_nodes(char* router, char* node_count, char** input1){
    if (atoi(router) > 10 || atoi(router) < 1 || atoi(node_count) > 9){
        printf("Unknown Router..Quit\n");
        exit(1);
    }
    int i;
    for (i=0; i<atoi(node_count); i++){
        struct ifreq ifr;
        strncpy(ifr.ifr_name, input1[4+i] ,IFNAMSIZ-1);
        if(ioctl(vars.send_sock_fd,SIOCGIFINDEX,&ifr)<0){
                printf("IOCTL ERROR \n");
            }
        int *index = malloc(sizeof(int));
        *index = ifr.ifr_ifindex; // Capture the interface index
        int node_index = (atoi(router)*10);
        strncpy((char*)&nodes_data[nodes_data_index].iface, input1[4+i], 10);
        nodes_data[nodes_data_index].send_index = ifr.ifr_ifindex;
        nodes_data[nodes_data_index].nexthop = node_index +i;
        nodes_data[nodes_data_index].address = node_index + i;
        nodes_data[nodes_data_index].dist = 0;
        //printf("Added new data at [%d] iface:%s add:%d nexthop:%d distance:%d\n", nodes_data_index, nodes_data[nodes_data_index].iface
        //                        , nodes_data[nodes_data_index].address, nodes_data[nodes_data_index].nexthop, nodes_data[nodes_data_index].dist);
        nodes_data_index++;
    }
}


int main(int argc, char *argv[]){
    nodes_data_index = 0; // Keep this index for nodes
    printf("Starting RIP Protocol....\n");

    init_send_socket();
    ///                                Router#   Node_count     Node2
    insert_my_nodes(argv[1], argv[2],       argv);
    router_id = atoi(argv[1]);
    own_nodes_count = atoi(argv[2]);
    // Now we have two nodes in our hashmap, we can get them by calling 

    // Initialize Receive socket to listen for other routers
    int i;
    for (i=0; i<atoi(argv[3]); i++){
        init_recv_sockets(argv[4+atoi(argv[2])+i], i); // Parameter are offseted behind nodes interface
    }
    // Router count will be the total number of ports
    vars.numports = atoi(argv[3]);
    // Now the non-blocking promiscous receving sockets are stored at vars.sockets[index], index is 0 or 1

    //Prepare sockaddr
    sockaddrll.sll_family=AF_PACKET;
    sockaddrll.sll_protocol=htons(ETH_P_ALL);
    sockaddrll.sll_halen=6;
    sockaddrll.sll_addr[0]=0x00;
    sockaddrll.sll_addr[1]=0x00;
    sockaddrll.sll_addr[2]=0x00;
    sockaddrll.sll_addr[3]=0x00;
    sockaddrll.sll_addr[4]=0x00;
    sockaddrll.sll_addr[5]=0x00;
    sockaddrll.sll_addr[6]=0x00;
    // Start threading
    pthread_t sending_thread,refreshing_thread;
    int rc;
    rc = pthread_create(&refreshing_thread, NULL, loop_refresh, NULL);
    if (rc){
        printf("ERROR; return code from pthread_create() is %d\n", rc);
        exit(-1);
    }
    sleep(1);
    rc = pthread_create(&sending_thread, NULL, loop_send, NULL);
    if (rc){
        printf("ERROR; return code from pthread_create() is %d\n", rc);
        exit(-1);
    }
    sleep(1);
    unsigned char recvbuf[1024];
    memset(recvbuf,'\0',1024);
    int recv_size;
    struct sockaddr_ll newsockaddr;
    int sock_size= sizeof(newsockaddr);
    while(1){
        // Recevies route update packet from other routers
        for(i=0;i<vars.numports;i++){
            recv_size = recvfrom(vars.sockets[i],recvbuf,1024,0,(struct sockaddr*)&newsockaddr,(socklen_t*)&sock_size);
            if(recv_size>0 && newsockaddr.sll_pkttype!=PACKET_OUTGOING){
                // printf("Recevied something\n");
                struct gyn_header *gyn;
                gyn = (struct gyn_header*)(recvbuf);
                int receive_router_id;
                int receive_node_count;
                memcpy(&receive_router_id, &recvbuf[sizeof(struct gyn_header)], sizeof(int));
                // Check if the ether_type is for RIP 
                if (gyn->ether_type == 0x1234) {
                    memcpy(&receive_node_count, &recvbuf[sizeof(struct gyn_header) + sizeof(int)], sizeof(int));
                    memcpy(&received_my_nodes, &recvbuf[sizeof(struct gyn_header) + (2*sizeof(int))], sizeof(received_my_nodes));
                    memcpy(&received_my_nodes_distance, &recvbuf[sizeof(received_my_nodes) +sizeof(struct gyn_header) + (2 * sizeof(int))], sizeof(received_my_nodes_distance));
                    printf("\n#######################################################\n");
                    printf("Received packet from Router ID: %d\n", receive_router_id);
                    // printf("Interface:%s and Destination has %d nodes\n", routing_data[i].iface, receive_node_count);
                    // Print out the received stuff
                    int j;
                    printf("\t Received Destinations:");
                    for (j=0;j<receive_node_count;j++){
                        printf(" %d", received_my_nodes[j]);
                    }
                    printf("\n");
                    // Add the nodes behind the routers to the node map
                    int n;
                    for (n=0; n < receive_node_count;n++){
                        // Check if the node is already stored, compared their distance and pick the closer one
                        int stored_index;
                        char found_match = 0;
                        for(stored_index=0; stored_index <= nodes_data_index; stored_index++){
                            if (received_my_nodes[n] == 0 || nodes_data[stored_index].address == 0){
                                continue;
                            }
                            if (received_my_nodes[n] == nodes_data[stored_index].address){
                                // The nodes is stored, compared their distance, only add when we have a closer distance
                                found_match = 1;
                                if (received_my_nodes_distance[n] + 1 < nodes_data[stored_index].dist) {
                                    strncpy((char*)&nodes_data[stored_index].iface, routing_data[i].iface, 10);
                                    nodes_data[stored_index].send_index = routing_data[i].send_index;
                                    nodes_data[stored_index].nexthop = receive_router_id;
                                    nodes_data[stored_index].address = received_my_nodes[n];
                                    nodes_data[stored_index].dist = received_my_nodes_distance[n] + 1;
                                    //printf("Closer node found at [%d] iface:%s add:%d nexthop:%d distance:%d\n", stored_index, nodes_data[stored_index].iface
                                    //    , nodes_data[stored_index].address, nodes_data[stored_index].nexthop, nodes_data[stored_index].dist);
                                    break;
                                }
                            } 
                        }
                        if (found_match == 0){ // When we didnt found it in our list
                            // If we dont have the node already in our list, add it
                            strncpy((char*)&nodes_data[nodes_data_index].iface, routing_data[i].iface, 10);
                            nodes_data[nodes_data_index].send_index = routing_data[i].send_index;
                            nodes_data[nodes_data_index].nexthop = receive_router_id;
                            nodes_data[nodes_data_index].address = received_my_nodes[n];
                            nodes_data[nodes_data_index].dist = received_my_nodes_distance[n]+1;
                            //printf("Added new data at [%d] iface:%s add:%d nexthop:%d distance:%d\n", nodes_data_index, nodes_data[nodes_data_index].iface
                            //    , nodes_data[nodes_data_index].address, nodes_data[nodes_data_index].nexthop, nodes_data[nodes_data_index].dist);
                            nodes_data_index++;
                        }
                            


                    }     

                    // Write into file
                    FILE *f = fopen("/tmp/rtr.config", "w");
                    if (f == NULL)
                    {
                        printf("Error opening file!\n");
                        exit(1);
                    }
                    for (j=0; j<nodes_data_index; j++){
                        fprintf(f,"%s %d %d\n", nodes_data[j].iface, nodes_data[j].address, nodes_data[j].nexthop);
                    }
                    fclose(f);
                    // Update our current node list
                    printf("#######################################################\n");
                } else if (gyn->ether_type == 0x1234){
                    // Refreesh packet received


                }
            }
        
        }
    }


}