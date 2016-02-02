//有记录几种攻击的功能，具体什么攻击看了程序就知道了。：）

//myxray.c V0.2
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <netinet/ip.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/ip_icmp.h>
#include <netinet/igmp.h>

#define __FAVOR_BSD
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netdb.h>


#define PACKET_SIZE 4096
#define ETH_HW_ADDR_LEN 6
#define IP_ADDR_LEN 4
#define LOWCOUNTER 10
#define MAX(a,b) ((a)>(b)?(a):(b))



struct arp_packet {
         u_char targ_hw_addr[ETH_HW_ADDR_LEN];
         u_char src_hw_addr[ETH_HW_ADDR_LEN];
         u_short frame_type;
         u_short hw_type;
         u_short prot_type;
         u_char hw_addr_size;
         u_char prot_addr_size;
         u_short op;
         u_char sndr_hw_addr[ETH_HW_ADDR_LEN];
         u_char sndr_ip_addr[IP_ADDR_LEN];
         u_char rcpt_hw_addr[ETH_HW_ADDR_LEN];
         u_char rcpt_ip_addr[IP_ADDR_LEN];
         u_char padding[18];
} ;

static void leave(void);
static char *hwaddr(unsigned char *, char *);
static void writearpbuf(FILE *, char *, int); //写一个arpbuf到fd中
static void writetcpbuf(FILE *, char *, int);
static void writeudpbuf(FILE *, char *, int);
static void writeigmpbuf(FILE *, char *, int);
static void writeicmpbuf(FILE *, char *, int);

int fd_recv = -1 ;
time_t one, two, temptime;
char arpbufone[PACKET_SIZE];
char tcpbufone[PACKET_SIZE];
char udpbufone[PACKET_SIZE];
char igmpbufone[PACKET_SIZE];
char icmpbufone[PACKET_SIZE];
int arpcounter = 0, tcpcounter = 0, igmpcounter = 0, icmpcounter = 0, udpcounter = 0;
char writebuf[PACKET_SIZE];

FILE *waringfd;
unsigned int total = 0, totalarp = 0, disarp = 0, totaltcp = 0, distcp = 0, totaludp = 0, disudp = 0, totalicmp = 0, disicmp = 0, totaligmp = 0, disigmp = 0;
struct ifreq ifr, ifr_old;


#define DEFAULT_DEVICE_ETH0  "eth0"
#define DEFAULT_DEVICE_WLAN0  "wlan0"

int capture2(int argc, char *argv[])
{
         char device[] = DEFAULT_DEVICE_WLAN0; // ethernet device name
         char protocol[16];
         u_char buf_recv[PACKET_SIZE]; // buffer for receive
         int ihl;
         int ethprotol;
         int ipfragoff, df, mf;
         int protol;
         char buf1[20], buf2[20], buf3[20], buf4[20];
         char strptr[255];
         char *srcaddr, *dstaddr;
         struct ethhdr *eth ;
         struct arp_packet *arphead;
         struct udphdr *udp;
         struct icmp *icmphdr;
         struct iphdr *ip;
         struct tcphdr *tcp1;
         char *flagsmes;
         int flags;
         struct igmp *igmp1;
         struct in_addr in1;
         struct in_addr in2;
         int from_len, datalen;
         struct sockaddr from;
         int arpenable = 0;
         int tcpenable = 0;
         int icmpenable = 0;
         int igmpenable = 0;
         int loenable = 0;
         int udpenable = 0;
         char *tcpflag = NULL;
         int c;
         char *dstip = NULL;
         char *srcip = NULL;
         extern char *optarg;
         extern int optind;
         const char message[] = " -a  display arp packet \n \
                                                        -t  display tcp packet \n \
                                                        -fone of RST FIN SYN PUSH ACK URG   display only tcpflag tcp packet \n \
                                                        -u  display udp packet \n \
                                                        -i  display icmp packet \n \
                                                        -g  display igmp packet \n \
                                                        -A  display all packet \n \
                                                        -l  also display lo layer packet \n \
                                                        -s srcip    display srcip packet \n \
                                                        -d dstip    display dstip packet \n \
                                                        Welcome to use myxray which writed by sztcww Good Luck \n\n";
         if (argc == 1) {
                  printf("%s\n", message);
                  exit(-1);
         }
         while ((c = getopt(argc, argv, "Aatugil?s:d:f:")) != EOF) {
                  switch (c) {
                  case 'A':
                           tcpenable = 1;
                           udpenable = 1;
                           icmpenable = 1;
                           igmpenable = 1;
                           arpenable = 1;
                           break;
                  case 'd':
                           dstip = optarg;
                           break;
                  case 's':
                           srcip = optarg;
                           break;
                  case 'f':
                           tcpflag = optarg;
                           break;
                  case 'a':
                           arpenable = 1;
                           break;
                  case 't':
                           tcpenable = 1;
                           break;
                  case 'i':
                           icmpenable = 1;
                           break;
                  case 'g':
                           igmpenable = 1;
                           break;
                  case 'u':
                           udpenable = 1;
                           break;
                  case 'l':
                           loenable = 1;
                           break;
                  case '?':
                           printf("%s\n", message);
                           exit(-1);
                  }
         }

         if ( (waringfd = fopen("myxray.waring", "a+")) == NULL) {
                  perror("fopen");
                  exit(-1);
         }
         printf("=====waringfd[%p]\n", waringfd);
         char tmpbuf[] = "dasffafwqefasfafasfaefwedasduiuewdfgqyudfqwducwqdyufqwtqwfeqtfetqwyefqwy\n";
         fwrite(tmpbuf, strlen(tmpbuf), 1, waringfd);

         fd_recv = socket(AF_INET, SOCK_PACKET, htons(0x0003));
         if (fd_recv < 0) {
                  perror( "packet socket error");
                  exit(-1);
         }

         strcpy(ifr.ifr_name, device);
         if (ioctl(fd_recv, SIOCGIFFLAGS, &ifr) < 0 ) {
                  perror("ioctl SIOCGIFFLAGS error");
                  if (fd_recv >= 0)
                           close(fd_recv);
                  exit(-1);
         }

         ifr_old = ifr;
         ifr.ifr_flags |= IFF_PROMISC;
         if (ioctl(fd_recv, SIOCSIFFLAGS, &ifr) < 0 ) {
                  perror("ioctl SIOCSIFFLAGS error");
                  if (fd_recv >= 0)
                           close(fd_recv);
                  exit(-1);
         }

         signal(SIGINT, leave);
         signal(SIGTERM, leave);
         one = time(NULL);

AGAIN:
         bzero(&from, sizeof(from));
         from_len = sizeof(from);
         bzero(buf_recv, PACKET_SIZE);
         datalen = recvfrom(fd_recv, (char *)buf_recv, 4096, 0, &from, &from_len);
         if (datalen < 0) {
                  perror("recvfrom error");
                  exit(-1);
         }
         printf("========== datalen[%d]\n", datalen);

         total++;
         buf_recv[datalen] = '\0';
         if (loenable == 0) {
                  if (strcmp(device, from.sa_data) != 0){
                            printf("11111111111111\n");
                           goto AGAIN;
                   }
         }
         eth = (struct ethhdr *)buf_recv;
         ethprotol = ntohs(eth->h_proto);

         if (ethprotol == 0x0806) {  //0x0806  //地址解析协议(Address Resolution Protocol)
                  arphead = (struct arp_packet *)buf_recv;
                  memcpy(&in1, arphead->sndr_ip_addr, IP_ADDR_LEN);
                  memcpy(&in2, arphead->rcpt_ip_addr, IP_ADDR_LEN);
                  srcaddr = inet_ntoa(in1);
                  dstaddr = (char *)inet_ntop(AF_INET, &in2, strptr, sizeof(strptr));
                  totalarp++;
                  if ( (!memcmp(srcaddr, dstaddr, MAX(strlen(srcaddr), strlen(dstaddr)))) && (memcmp(hwaddr(arphead->rcpt_hw_addr, buf1), "00:00:00:00:00:00", 17)) ) {
                           printf("1111111111 writearpbuf\n");
                           writearpbuf(waringfd, buf_recv, 1);
                           /*纪录arp包中srcaddr,dstaddr相同的数据包*/
                  } else {
                           printf("22222 memcmp 222222222 \n");
                           if ( memcmp(buf_recv, arpbufone, datalen) == 0 )
                                    arpcounter++;
                           else {
                                    if (arpcounter > LOWCOUNTER) {
                                            printf("22222222222222 writearpbuf\n");
                                             /*纪录连续抓到的,arpcounter>LOWCOUNTER的arp包 */
                                             writearpbuf(waringfd, arpbufone, arpcounter + 1);
                                             arpcounter = 0;
                                    }
                                    memcpy(arpbufone, buf_recv, datalen);
                                    //arpcounter = 0;
                           }//end else
                  }//end else
         }

         if ((ethprotol == 0x0806) && (arpenable)) { //arp
                   printf(" arp====\n");
                  if (((dstip == NULL) || (strcmp(dstaddr, dstip) == 0)) && ((srcip == NULL) || (strcmp(srcaddr, srcip) == 0))) {
                           temptime = time(NULL);
                           printf("Recorded %sethhdr\nsrchw:%s--->dsthw:%s proto:%xH\n", ctime(&temptime), hwaddr(eth->h_source, buf1), hwaddr(eth->h_dest, buf2), ethprotol );
                           printf("arphdr\nhwtype:%d protol:%xH hw_size:%d pro_size:%d op:%d\ns_ha:% s s_ip:%s\nd_ha:%s d_ip:%s\n",
                                          ntohs(arphead->hw_type), ntohs(arphead->prot_type), arphead->hw_addr_size, arphead->prot_addr_size,
                                          ntohs(arphead->op), hwaddr(arphead->sndr_hw_addr, buf1), srcaddr, hwaddr(arphead->rcpt_hw_addr, buf2), dstaddr
                                      );

                           printf("-----------------------------------------------------------\n");
                           disarp++;
                  }
                  printf("2222222222\n");
                  goto AGAIN;
         }//end arp

         if (ethprotol == 0x0800) {     //0x0800 //IP协议
                   printf(" ip====\n");
                  ip = (struct iphdr *)&buf_recv[14];
                  ihl = (int)ip->ihl << 2;
                  in1.s_addr = ip->saddr;
                  in2.s_addr = ip->daddr;
                  srcaddr = inet_ntoa(in1);
                  dstaddr = (char *)inet_ntop(AF_INET, &in2, strptr, sizeof(strptr));

                  //iphdr
                  protol = ip->protocol;
                  ipfragoff = ntohs(ip->frag_off);
                  df = ipfragoff & IP_DF;
                  mf = ipfragoff & IP_MF;
                  if (df != 0)
                           df = 1;
                  if (mf != 0)
                           mf = 1;
                  protol = ip->protocol;

                  switch (protol) {
                  case 6 :  //tcp
                            printf(" tcp====\n");
                           totaltcp++;
                           tcp1 = (struct tcphdr *)&buf_recv[14 + ihl];
                           flags = tcp1->th_flags;
                           if (flags & TH_PUSH)
                                    flagsmes = "PUSH";
                           if (flags & TH_ACK)
                                    flagsmes = "ACK";
                           if (flags & TH_URG)
                                    flagsmes = "URG";
                           if (flags & TH_FIN)
                                    flagsmes = "FIN";
                           if (flags & TH_SYN)
                                    flagsmes = "SYN";
                           if (flags & TH_RST)
                                    flagsmes = "RST";

                            printf("333333333 tcpcounter[%d]\n", tcpcounter);
                           if ( !memcmp(buf_recv, tcpbufone, datalen)) {
                                    tcpcounter++;
                           } else {
                                    if (tcpcounter > LOWCOUNTER) {
                                            printf("333333333 writetcpbuf\n");
                                             writetcpbuf(waringfd, tcpbufone, tcpcounter);
                                    }
                                    tcpcounter = 0;
                                    memcpy(tcpbufone, buf_recv, datalen);
                           }
                           break;

                  case 1 :  //icmp
                           printf(" icmp====\n");
                           totalicmp++;
                           icmphdr = (struct icmp *)&buf_recv[14 + ihl];

                           printf("444444444444 icmpcounter[%d]\n", icmpcounter);
                           if (!memcmp(buf_recv, icmpbufone, datalen)) {
                                    icmpcounter++;
                           } else {
                                    if (icmpcounter > LOWCOUNTER) {
                                            printf("444444444444 writeicmpbuf\n");
                                             writeicmpbuf(waringfd, icmpbufone, icmpcounter);
                                    }
                                    icmpcounter = 0;
                                    memcpy(icmpbufone, buf_recv, datalen);
                           }
                           break;

                  case 17: //udp
                            printf(" udp====\n");
                           totaludp++;
                           udp = (struct udphdr *)&buf_recv[14 + ihl];

                           printf("555555555 udpcounter[%d]\n", udpcounter);
                           printf("555555555 buf_recv[%s]\n", buf_recv);
                           printf("555555555 udpbufone[%s]\n", udpbufone);
                           if ( !memcmp(buf_recv, udpbufone, datalen)){
                                     printf("555555555 udpcounter++   \n");
                                    udpcounter++;
                           } else {
                                    printf("555555555+++++11111 udpcounter[%d]\n", udpcounter);
                                    if (udpcounter > LOWCOUNTER) {
                                            printf("555555555 writeudpbuf\n");
                                             writeudpbuf(waringfd, udpbufone, udpcounter);
                                             udpcounter = 0;
                                    }
                                    //udpcounter = 0;
                                    memcpy(udpbufone, buf_recv, datalen);
                           }
                           break;

                  case 2 :  //igmp
                            printf(" igmp====\n");
                           totaligmp++;
                           igmp1 = (struct igmp *)&buf_recv[14 + ihl];

                           printf("666666666666 igmpcounter[%d]\n", igmpcounter);
                           if (!memcmp(inet_ntoa(igmp1->igmp_group), "0.0.0.0", 7))
                                    writeigmpbuf(waringfd, buf_recv, 1);
                           else {
                                    if (!memcmp(buf_recv, igmpbufone, datalen))
                                             igmpcounter++;
                                    else {
                                             if (igmpcounter > LOWCOUNTER) {
                                                    printf("666666666666 writeigmpbuf\n");
                                                      writeigmpbuf(waringfd, igmpbufone, igmpcounter);
                                             }
                                             igmpcounter = 0;
                                             memcpy(igmpbufone, buf_recv, datalen);
                                    }
                           }
                           break;
                  } //end switch


                  if (((dstip == NULL) || (strcmp(dstaddr, dstip) == 0)) && ((srcip == NULL) || (strcmp(srcaddr, srcip) == 0))) {
                           if ((protol == 6) && (tcpenable)) { //tcp
                                    if (tcpflag != NULL) {
                                             if (strcmp(tcpflag, flagsmes))
                                                       printf("333333333333\n");
                                                      goto AGAIN;
                                    }

                                    //do with ethhdr data
                                    temptime = time(NULL);
                                    printf("Recorded %sethhdr\nsrchw:%s--->dsthw:%s proto:%xH\n", ctime(&temptime), hwaddr(eth->h_source, buf1), hwaddr(eth->h_dest, buf2), ethprotol);

                                    //iphdr
                                    printf("iphdr\nver:%d ihl:%d tos:%d tot_len:%d id:%d df:%d mf:%d fragoff:%d TTL:%d proto:%d\nsrcaddr:%s dstaddr:%s\n",
                                                    ip->version, ip->ihl, ip->tos, ntohs(ip->tot_len), ntohs(ip->id), df, mf, ipfragoff & IP_OFFMASK, ip->ttl,  protol, srcaddr, dstaddr
                                          );

                                    //tcphdr
                                    printf("tcphdr\nsrcport:%d dstport:%d seq:%u ack:%u off:%d flag:%s win:%d\n",
                                                    ntohs(tcp1->th_sport), ntohs(tcp1->th_dport), ntohl(tcp1->th_seq), ntohl(tcp1->th_ack), tcp1->th_off, flagsmes, ntohs(tcp1->th_win)
                                          );
                                    printf("-----------------------------------------------------------\n");
                                    distcp++;
                           }//end tcp

                           if ((protol == 1) && (icmpenable)) { //icmp
                                    temptime = time(NULL);
                                    printf("Recorded %sethhdr\nsrchw:%s--->dsthw:%s proto:%xH\n", ctime(&temptime), hwaddr(eth->h_source, buf1), hwaddr(eth->h_dest, buf2), ethprotol);
                                    printf("iphdr\nver:%d ihl:%d tos:%d tot_len:%d id:%d df:%d mf:%d fragoff:%d TTL:%d proto:%d\nsrcaddr:%s dstaddr:%s\n",
                                                   ip->version, ip->ihl, ip->tos, ntohs(ip->tot_len), ntohs(ip->id), df, mf, ipfragoff & IP_OFFMASK, ip->ttl, protol, srcaddr, dstaddr
                                                 );
                                    printf("icmp\ntype:%d code:%d\n", icmphdr->icmp_type, icmphdr->icmp_code);
                                    printf("-----------------------------------------------------------\n");
                                    disicmp++;
                           }//end icmp

                           if ((protol == 17) && (udpenable)) { //udphdr
                                    temptime = time(NULL);
                                    printf("Recored %sethhdr\nsrchw:%s--->dsthw:%s proto:%xH\n", ctime(&temptime), hwaddr(eth->h_source, buf1), hwaddr(eth->h_dest, buf2), ethprotol);
                                    printf("iphdr\nver:%d ihl:%d tos:%d tot_len:%d id:%d df:%d mf:%d fragoff:%d TTL:%d proto:%d\nsrcaddr:%s dstaddr:%s\n",
                                                   ip->version, ip->ihl, ip->tos, ntohs(ip->tot_len), ntohs(ip->id), df, mf, ipfragoff & IP_OFFMASK, ip->ttl, protol, srcaddr, dstaddr
                                              );
                                    printf("udphdr\nsport:%d dsport:%d len:%d\n", ntohs(udp->uh_sport), ntohs(udp->uh_dport), ntohs(udp->uh_ulen));
                                    printf("-----------------------------------------------------------\n");
                                    disudp++;
                           }//end udp

                           if ((protol == 2) && (igmpenable)) { //igmp
                                    temptime = time(NULL);
                                    printf("Recored %sethhdr\nsrchw:%s--->dsthw:%s proto:%xH\n", ctime(&temptime), hwaddr(eth->h_source, buf1), hwaddr(eth->h_dest, buf2), ethprotol);
                                    printf("iphdr\nver:%d ihl:%d tos:%d tot_len:%d id:%d df:%d mf:%d fragoff:%d TTL:%d proto:%d\nsrcaddr:%s dstaddr:%s\n",
                                                    ip->version, ip->ihl, ip->tos, ntohs(ip->tot_len), ntohs(ip->id), df, mf, ipfragoff & IP_OFFMASK, ip->ttl,  protol, srcaddr, dstaddr
                                                );
                                    printf("igmphdr\ntype:%d code:%d group_addr:%s\n", igmp1->igmp_type, igmp1->igmp_code, inet_ntoa(igmp1->igmp_group));
                                    printf("-----------------------------------------------------------\n");
                                    disigmp++;
                           }//end igmp

                           fflush(stdout);
                  } //endif strcmp
         }//endif ip

         if (ethprotol == 0x8035) {     //0x8035  //返向地址解析协议(Reverse Address Resolution Protocol)
               printf("RARP,  Reverse Address Resolution Protocol,  ethprotol == 0x8035\n");
         }
         if (ethprotol == 0x86DD) {     //0x86DD  //IPV6协议
               printf("IPV6, IP V6 Protocol,  ethprotol == 0x86DD\n");
         }


         printf("444444444444\n");
         goto AGAIN;
}//end main



static char *hwaddr (unsigned char *s, char *d)
{
         sprintf (d, "%02X:%02X:%02X:%02X:%02X:%02X", s[0], s[1], s[2], s[3], s[4], s[5]);

         return d;
}


static void leave(void)
{
         if (ioctl(fd_recv, SIOCSIFFLAGS, &ifr_old) < 0) {
                  perror("ioctl SIOCSIFFLAGS error");
         }
         if (fd_recv > 0)
                  close(fd_recv);

         two = time(NULL);
         fclose(waringfd);

         printf("total received %u packets\n", total);
         printf("tcppackets %u, display %u, speed is %u /second\n", totaltcp, distcp, totaltcp / (two - one));
         printf("udppackets %u, display %u, speed is %u /second\n", totaludp, disudp, totaludp / (two - one));
         printf("arppackets %u, display %u, speed is %u /second\n", totalarp, disarp, totalarp / (two - one));
         printf("icmp packets %u, display %u, speed is %u /second\n", totalicmp, disicmp, totalicmp / (two - one));
         printf("igmp packets %u, display %u, speed is %u /second\n", totaligmp, disigmp, totaligmp / (two - one));
         printf("process terminamted.\n");

         exit(0);
}


static void writearpbuf(FILE *fd, char *p, int counter)
{
         struct arp_packet *arphead;
         struct in_addr in2, in1;
         char *srcaddr, *dstaddr;
         time_t temptime;
         char strptr[255];
         char buf1[20], buf2[20], buf3[20], buf4[20];
         int ethprotol;

         printf("===writearpbuf==111111 fd[%p]\n", fd);

         temptime = time(NULL);
         arphead = (struct arp_packet *)p;
         ethprotol = ntohs(arphead->frame_type);
         memcpy(&in1, arphead->sndr_ip_addr, IP_ADDR_LEN);
         memcpy(&in2, arphead->rcpt_ip_addr, IP_ADDR_LEN);
         srcaddr = inet_ntoa(in1);
         dstaddr = (char *)inet_ntop(AF_INET, &in2, strptr, sizeof(strptr));

         snprintf(writebuf, PACKET_SIZE, "This arp packet catch %d times continuous\nrecorded %sethhdr\nsrchw:%s--->dsthw:%s proto:%xH arphdr\nhwtype:%d protol:%xH hw_size:%d pro_size:%d op:%d\ns_ha:% s s_ip:%s\nd_ha:%s d_ip:%s -----------------------------------------------------------\n",
                  counter, ctime(&temptime), hwaddr(arphead->src_hw_addr, buf1), hwaddr(arphead->targ_hw_addr, buf2), ethprotol, ntohs(arphead->hw_type), ntohs(arphead->prot_type),
                  arphead->hw_addr_size, arphead->prot_addr_size, ntohs(arphead->op), hwaddr(arphead->sndr_hw_addr, buf3), srcaddr, hwaddr(arphead->rcpt_hw_addr, buf4), dstaddr
                 );
         printf("=====111111 fd[%p]\n", fd);
         fwrite(writebuf, strlen(writebuf), 1, fd);
}


static void writetcpbuf(FILE *fd, char *p, int counter)
{
         struct ethhdr *eth ;
         struct iphdr *ip;
         struct tcphdr *tcp1;
         char *flagsmes;
         int flags;
         struct in_addr in1, in2;
         int ethprotol;
         int ihl, df, mf, ipfragoff;
         char *srcaddr, *dstaddr;
         int protol;
         char strptr[255];
         char buf1[20], buf2[20];


         eth = (struct ethhdr *)p;
         ethprotol = ntohs(eth->h_proto);
         ip = (struct iphdr *)(p + 14);
         ihl = (int)ip->ihl << 2;
         in1.s_addr = ip->saddr;
         in2.s_addr = ip->daddr;
         srcaddr = inet_ntoa(in1);
         dstaddr = (char *)inet_ntop(AF_INET, &in2, strptr, sizeof(strptr));

         //iphdr
         protol = ip->protocol;
         ipfragoff = ntohs(ip->frag_off);
         df = ipfragoff & IP_DF;
         mf = ipfragoff & IP_MF;
         if (df != 0)
                  df = 1;
         if (mf != 0)
                  mf = 1;
         protol = ip->protocol;
         tcp1 = (struct tcphdr *)(p + 14 + ihl);
         flags = tcp1->th_flags;

         if (flags & TH_PUSH)
                  flagsmes = "PUSH";
         if (flags & TH_ACK)
                  flagsmes = "ACK";
         if (flags & TH_URG)
                  flagsmes = "URG";
         if (flags & TH_FIN)
                  flagsmes = "FIN";
         if (flags & TH_SYN)
                  flagsmes = "SYN";
         if (flags & TH_RST)
                  flagsmes = "RST";
         temptime = time(NULL);

         snprintf(writebuf, PACKET_SIZE, "This tcp packet catch %d times continuous\nrecorded %sethhdr\nsrchw:%s--->dsthw:%s proto:%xH iphdr\nver:%d ihl:%d tos:%d tot_len:%d id:%d df:%d mf:%d fragoff:%d TTL:%d proto:%d\nsrcaddr:%s dstaddr:%s tcphdr\nsrcport:%d dstport:%d seq:%u ack:%u off:%d flag:%s win:%d -----------------------------------------------------------\n",
                  counter + 1, ctime(&temptime), hwaddr(eth->h_source, buf1), hwaddr(eth->h_dest, buf2), ethprotol, ip->version, ip->ihl, ip->tos, ntohs(ip->tot_len), ntohs(ip->id), df, mf,
                  ipfragoff & IP_OFFMASK, ip->ttl, protol, srcaddr, dstaddr,  ntohs(tcp1->th_sport), ntohs(tcp1->th_dport), ntohl(tcp1->th_seq), ntohl(tcp1->th_ack), tcp1->th_off, flagsmes, ntohs(tcp1->th_win)
                 );
         printf("=====222222222 fd[%p]\n", fd);
         fwrite(writebuf, strlen(writebuf), 1, fd);

         return 0;
}


static void writeigmpbuf(FILE *fd, char *p, int counter)
{
         struct ethhdr *eth ;
         struct iphdr *ip;
         struct in_addr in1, in2;
         int ethprotol;
         int ihl, df, mf, ipfragoff;
         char *srcaddr, *dstaddr;
         int protol;
         char strptr[255];
         char buf1[20], buf2[20];
         struct igmp *igmp1;


         eth = (struct ethhdr *)p;
         ethprotol = ntohs(eth->h_proto);
         ip = (struct iphdr *)(p + 14);
         ihl = (int)ip->ihl << 2;
         in1.s_addr = ip->saddr;
         in2.s_addr = ip->daddr;
         srcaddr = inet_ntoa(in1);
         dstaddr = (char *)inet_ntop(AF_INET, &in2, strptr, sizeof(strptr));

         //iphdr
         protol = ip->protocol;
         ipfragoff = ntohs(ip->frag_off);
         df = ipfragoff & IP_DF;
         mf = ipfragoff & IP_MF;
         if (df != 0)
                  df = 1;
         if (mf != 0)
                  mf = 1;
         protol = ip->protocol;
         igmp1 = (struct igmp *)(p + 14 + ihl);
         temptime = time(NULL);

         snprintf(writebuf, PACKET_SIZE, "This packet catch %d times continuous\nrecorded %sethhdr\nsrchw:%s--->dsthw:%s proto:%xH iphdr\nver:%d ihl:%d tos:%d tot_len:%d id:%d df:%d mf:%d fragoff:%d TTL:%d proto:%d\nsrcaddr:%s dstaddr:%s igmphdr\ntype:%d code:%d group_addr:%s -----------------------------------------------------------\n",
                  counter + 1, ctime(&temptime), hwaddr(eth->h_source, buf1), hwaddr(eth->h_dest, buf2), ethprotol, ip->version, ip->ihl,
                  ip->tos, ntohs(ip->tot_len), ntohs(ip->id), df, mf, ipfragoff & IP_OFFMASK, ip->ttl, protol, srcaddr, dstaddr, igmp1->igmp_type, igmp1->igmp_code, inet_ntoa(igmp1->igmp_group)
                 );
         printf("=====333333333333 fd[%p]\n", fd);
         fwrite(writebuf, strlen(writebuf), 1, fd);
}


static void writeudpbuf(FILE *fd, char *p, int counter)
{
         struct ethhdr *eth ;
         struct iphdr *ip;
         struct in_addr in1, in2;
         int ethprotol;
         int ihl, df, mf, ipfragoff;
         char *srcaddr, *dstaddr;
         int protol;
         char strptr[255];
         char buf1[20], buf2[20];
         struct udphdr *udp;


         eth = (struct ethhdr *)p;
         ethprotol = ntohs(eth->h_proto);
         ip = (struct iphdr *)(p + 14);
         ihl = (int)ip->ihl << 2;
         in1.s_addr = ip->saddr;
         in2.s_addr = ip->daddr;
         srcaddr = inet_ntoa(in1);
         dstaddr = (char *)inet_ntop(AF_INET, &in2, strptr, sizeof(strptr));

         //iphdr
         protol = ip->protocol;
         ipfragoff = ntohs(ip->frag_off);
         df = ipfragoff & IP_DF;
         mf = ipfragoff & IP_MF;
         if (df != 0)
                  df = 1;
         if (mf != 0)
                  mf = 1;
         protol = ip->protocol;
         udp = (struct udphdr *)(p + 14 + ihl);
         temptime = time(NULL);

         snprintf(writebuf, PACKET_SIZE, "This udp packet catch %d times continuous\nrecorded %sethhdr\nsrchw:%s--->dsthw:%s proto:%xH iphdr\nver:%d ihl:%d tos:%d tot_len:%d id:%d df:%d mf:%d fragoff:%d TTL:%d proto:%d\nsrcaddr:%s dstaddr:%s udphdr\nsport:%d dsport:%d len:%d -----------------------------------------------------------\n",
                          counter + 1, ctime(&temptime), hwaddr(eth->h_source, buf1), hwaddr(eth->h_dest, buf2), ethprotol, ip->version, ip->ihl,
                          ip->tos, ntohs(ip->tot_len), ntohs(ip->id), df, mf, ipfragoff & IP_OFFMASK, ip->ttl, protol, srcaddr, dstaddr, ntohs(udp->uh_sport), ntohs(udp->uh_dport), ntohs(udp->uh_ulen)
                 );
         printf("=====444444444444 fd[%p]\n", fd);
         fwrite(writebuf, strlen(writebuf), 1, fd);
}


static void writeicmpbuf(FILE *fd, char *p, int counter)
{
         struct ethhdr *eth ;
         struct iphdr *ip;
         struct in_addr in1, in2;
         int ethprotol;
         int ihl, df, mf, ipfragoff;
         char *srcaddr, *dstaddr;
         int protol;
         char strptr[255];
         char buf1[20], buf2[20];
         struct icmp *icmphdr;


         eth = (struct ethhdr *)p;
         ethprotol = ntohs(eth->h_proto);
         ip = (struct iphdr *)(p + 14);
         ihl = (int)ip->ihl << 2;
         in1.s_addr = ip->saddr;
         in2.s_addr = ip->daddr;
         srcaddr = inet_ntoa(in1);
         dstaddr = (char *)inet_ntop(AF_INET, &in2, strptr, sizeof(strptr));

         //iphdr
         protol = ip->protocol;
         ipfragoff = ntohs(ip->frag_off);
         df = ipfragoff & IP_DF;
         mf = ipfragoff & IP_MF;
         if (df != 0)
                  df = 1;
         if (mf != 0)
                  mf = 1;
         protol = ip->protocol;
         icmphdr = (struct icmp *)(p + 14 + ihl);
         temptime = time(NULL);

         snprintf(writebuf, PACKET_SIZE, "This icmp packet catch %d times continuous\nrecorded %sethhdr\nsrchw:%s--->dsthw:%s proto:%xH iphdr\nver:%d ihl:%d tos:%d tot_len:%d id:%d df:%d mf:%d fragoff:%d TTL:%d proto:%d\nsrcaddr:%s dstaddr:%s icmp\ntype:%d code:%d -----------------------------------------------------------\n",
                          counter + 1, ctime(&temptime), hwaddr(eth->h_source, buf1), hwaddr(eth->h_dest, buf2), ethprotol, ip->version, ip->ihl,
                          ip->tos, ntohs(ip->tot_len), ntohs(ip->id), df, mf, ipfragoff & IP_OFFMASK, ip->ttl, protol, srcaddr, dstaddr, icmphdr->icmp_type, icmphdr->icmp_code
                 );
         printf("=====555555 fd[%p]\n", fd);
         fwrite(writebuf, strlen(writebuf), 1, fd);
}



