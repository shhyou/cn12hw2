/*
 * =====================================================================================
 *
 *       Filename:  UDPProxy.c
 *
 *    Description:  UDP Proxy
 *
 *        Version:  1.0
 *        Created:  2012年11月15日 09時07分21秒
 *       Compiler:  gcc
 *
 *         Author:  CSIE R00944014 Simon Yeh 
 *   Organization:  CSIE Newslab VOIP Group
 *
 * =====================================================================================
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <arpa/inet.h>
#include <netinet/in.h>


int usage(int argc, char **argv) {
    if(argc == 1)
        return 0;
    else if(argc == 2 && strcmp(argv[1], "-m")==0)
        return 1;
    printf("Usage: %s [-m]\n\t-m\tConfigure settings manually(optional)\n\n", argv[0]);
    return -1;
}

void selectSet(int sock1, int sock2, fd_set *rdfds_ptr, struct timeval *tv_ptr) {
    FD_ZERO(rdfds_ptr);
    FD_SET(0, rdfds_ptr);
    FD_SET(sock1, rdfds_ptr);
    FD_SET(sock2, rdfds_ptr);
    tv_ptr->tv_sec = 5;
    tv_ptr->tv_usec = 0;
}

int main(int argc, char **argv) {
    FILE* fp;
    char str[101];
    int manual;
    int socket1, socket2, maxfd=0;
    struct sockaddr_in addr1, addr2, src_addr1, src_addr2;
    unsigned int addr_len1, addr_len2;
    struct hostent *hp;
    int packet_size = 4;
    char *packet;
    int lossRate, errorRate;


    if((manual = usage(argc, argv))<0)
        return -1;

    if(manual == 0) {
        fp = fopen("udpproxy.conf", "r");
        if(fp == NULL) {
            printf("\033[1;33mudpproxy.conf not found\033[m\n");
            if(argc==1)
                printf("\033[1;32mChange to manual mode.\033[m\n");
            manual = 1;
        }
        else 
            printf("\033[1;32mOpen udpproxy.conf OK.\033[m\n");
    }

    socket1 = socket(AF_INET, SOCK_DGRAM, 0);
    if(socket1 < 0) {
        printf("\033[1;31mGet socket error.\033[m\n");
        return -1;
    }
    if(socket1 > maxfd)
        maxfd = socket1;
    memset(&addr1, 0, sizeof(addr1));
    addr1.sin_family = AF_INET;
    addr1.sin_addr.s_addr = htonl(INADDR_ANY);
    int port;
    if(manual==1){
        do {
            printf("listen port:");
            scanf("%s", str);
            port = atoi(str);
            if(port==0)
                printf("\033[1;31matoi return 0\033[m\n");
        }while(port==0);
    }
    else {
        if(fgets(str, 100, fp)==NULL) 
            port=0;
        else
            port = atoi(str);
        if(port==0) {
            printf("\033[1;31mParse port number failed.\033[m\n");
            do {
                printf("listen port:");
                scanf("%s", str);
                port = atoi(str);
                if(port==0)
                    printf("\033[1;31matoi return 0\033[m\n");
            }while(port==0);
        }
        else
            printf("Get port \033[1;32m%d\033[m from config file.\n", port);
    }
    addr1.sin_port = htons(port);

    while(bind(socket1, (struct sockaddr*)&addr1, sizeof(addr1))<0) {
        printf("\033[1;31mBind failed\033[m, please try again\n");
        printf("listen port:");
        scanf("%d", &port);
        addr1.sin_port = htons(port);
    }
    printf("\033[1;32mBind Ok.\033[m\n");
    printf("\033[1;36mSocket1 Ready.\033[m\n");

    memset(&addr2, 0, sizeof(addr2));
    addr2.sin_family = AF_INET;
    if(manual == 1) {
        do {
            printf("IP/host name:");
            scanf("%s", str);
            hp = gethostbyname(str);
            if(hp == NULL) {
                printf("\033[1;31mCan't obtain IP\033[m, please try again.\n");
            }
        }while(hp==NULL);
    }
    else{
        if(fgets(str, 100, fp)==NULL)
            strcpy(str, "");
        else {
            if(str[strlen(str)-1]=='\n')
                str[strlen(str)-1] = '\0';
            printf("Get hostname: \033[1;32m%s\033[m\n", str);
        }
        hp = gethostbyname(str);
        while(hp==NULL) {
            printf("\033[1;31mCan't obtain IP\033[m, please try again.\n");
            printf("IP/host name:");
            scanf("%s", str);
            hp = gethostbyname(str);
        }
    }
    memcpy(&addr2.sin_addr, hp->h_addr_list[0], hp->h_length);
    printf("Obtain IP address \033[1;32m%s\033[m OK.\n", inet_ntoa(addr2.sin_addr));

    if(manual==1){
        do {
            printf("target port:");
            scanf("%s", str);
            port = atoi(str);
            if(port==0)
                printf("\033[1;31matoi return 0\033[m\n");
        }while(port==0);
    }
    else {
        if(fgets(str, 100, fp)==NULL)
            port = 0;
        else
            port = atoi(str);
        if(port==0) {
            printf("\033[1;31mParse port number failed.\033[m\n");
            do {
                printf("target port:");
                scanf("%s", str);
                port = atoi(str);
                if(port==0)
                    printf("\033[1;31matoi return 0\033[m\n");
            }while(port==0);
        }
        else
            printf("Get port \033[1;32m%d\033[m from config file.\n", port);
    }
    addr2.sin_port = htons(port);
    socket2 = socket(AF_INET, SOCK_DGRAM, 0);
    if(socket2 < 0) {
        printf("\033[1;31mGet socket error.\033[m\n");
        return -1;
    }
    if(socket2 > maxfd)
        maxfd = socket2;
    printf("\033[1;36mSocket2 Ready.\033[m\n");

    if(manual == 1) {
        do {
            printf("Loss Rate(%%):");
            scanf("%s", str);
            lossRate = atoi(str);
            if(lossRate == 0 && strcmp(str, "0")!=0)
                printf("\033[1;31matoi return 0\033[m\n");
        }while(lossRate == 0 && strcmp(str, "0")!=0);
    }
    else {
        if(fgets(str, 100, fp)==NULL)
            lossRate = 0;
        else
            lossRate = atoi(str);
        if(lossRate==0 && strcmp(str, "0\n")!=0) {
            printf("\033[1;31mParse Loss Rate failed.\033[m\n");
            do {
                printf("Loss Rate(%%):");
                scanf("%s", str);
                lossRate = atoi(str);
                if(lossRate == 0 && strcmp(str, "0")!=0)
                    printf("\033[1;31matoi return 0\033[m\n");
            }while(lossRate==0 && strcmp(str, "0")!=0);
        }
    }
    printf("Loss Rate = \033[1;32m%d%%\033[m\n", lossRate);

    if(manual == 1) {
        do {
            printf("Error Rate(%%):");
            scanf("%s", str);
            errorRate = atoi(str);
            if(errorRate == 0 && strcmp(str, "0")!=0)
                printf("\033[1;31matoi return 0\033[m\n");
        }while(errorRate==0 && strcmp(str, "0")!=0);
    }
    else {
        if(fgets(str, 100, fp)==NULL)
            errorRate = 0;
        else
            errorRate = atoi(str);
        if(errorRate==0 && strcmp(str, "0\n")!=0) {
            printf("\033[1;31mParse Error Rate failed.\033[m\n");
            do {
                printf("Error Rate(%%):");
                scanf("%s", str);
                errorRate = atoi(str);
                if(errorRate == 0 && strcmp(str, "0")!=0)
                    printf("\033[1;31matoi return 0\033[m\n");
            }while(errorRate==0 && strcmp(str, "0")!=0);
        }
    }
    printf("Error Rate = \033[1;32m%d%%\033[m\n", errorRate);
    if(manual==0)
        fclose(fp);

    fd_set rdfds;
    struct timeval tv;
    int ret;

    selectSet(socket1, socket2, &rdfds, &tv);
    srand(514514);
    while((ret = select(maxfd+1, &rdfds, NULL, NULL, &tv))>=0) {
        if(FD_ISSET(0, &rdfds)>0) {
            char buf[1024];
            read(0, buf, 1024);
            break;
        }
        if(FD_ISSET(socket1, &rdfds)>0) {
            packet_size = 0;
            recvfrom(socket1, &packet_size, 1, MSG_PEEK, (struct sockaddr*)&src_addr1, &addr_len1);
            packet_size++;
//            printf("packet size: %d\n", packet_size);
            addr_len1 = sizeof(src_addr1);
            packet = (char*)malloc(sizeof(char)*packet_size);
            recvfrom(socket1, packet, packet_size, 0, (struct sockaddr*)&src_addr1, &addr_len1);
            if(rand()%100 >= lossRate) {
                if(rand()%100 < errorRate){
                    printf("\033[1;33mMutate packet from socket1\033[m\n");
                    packet[(rand()%(packet_size-1))+1] = rand()%256;
                }
                sendto(socket2, packet, packet_size, 0, (struct sockaddr*)&addr2, sizeof(addr2));
                printf("\033[1;32mForward from socket1\033[m\n");
            }
            else {
                printf("\033[1;31mDrop from socket1\033[m\n");
            }
            free(packet);
        }
        if(FD_ISSET(socket2, &rdfds)>0) {
            packet_size = 0;
            recvfrom(socket2, &packet_size, 1, MSG_PEEK, (struct sockaddr*)&src_addr2, &addr_len2);
            packet_size++;
            addr_len2 = sizeof(src_addr2);
            packet = (char*)malloc(sizeof(char)*packet_size);
            recvfrom(socket2, packet, packet_size, 0, (struct sockaddr*)&src_addr2, &addr_len2);
            if(rand()%100 >= lossRate) {
                if(rand()%100 < errorRate) {
                    printf("\033[1;33mMutate packet from socket2\033[m\n");
                    packet[(rand()%(packet_size-1))+1] = rand()%256;
                }
                sendto(socket1, packet, packet_size, 0, (struct sockaddr*)&src_addr1, sizeof(src_addr1));
                printf("\033[1;32mForward from socket2\033[m\n");
            }
            else {
                printf("\033[1;31mDrop from socket2\033[m\n");
            }
            free(packet);
        }
        selectSet(socket1, socket2, &rdfds, &tv);
    }
    close(socket1);
    close(socket2);
    if(ret < 0){
        printf("\033[1;31mSelect error.\033[m\n");
        return -1;
    }
    printf("%s finished.\n", argv[0]);
    return 0;
}

