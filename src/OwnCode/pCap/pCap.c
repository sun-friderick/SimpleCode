#include <stdio.h>
#include <stdlib.h>
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
#include <unistd.h>

#define __FAVOR_BSD
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netdb.h>

/**
 *  -a  display arp packet      
 *   -t  display tcp packet    
 *   -f  one of RST FIN SYN PUSH ACK URG   display only tcpflag tcp packet     
 *   -u  display udp packet     
 *   -i  display icmp packet      
 *   -g  display igmp packet       
 *   -A  display all packet       
 *   -l  also display lo layer packet    
 *   -s srcip    display srcip packet   
 *   -d dstip    display dstip packet 
 */

#if 0
#define  PACKET_FLAG_RESIVED     0
#define  PACKET_FLAG_ALL     1
#define  PACKET_FLAG_ARP     2
#define  PACKET_FLAG_RARP     3
#define  PACKET_FLAG_ICMP     4
#define  PACKET_FLAG_IGMP     5
#define  PACKET_FLAG_IPV6     6
#define  PACKET_FLAG_TCP     7
#define  PACKET_FLAG_UDP     8
#define  PACKET_FLAG_LO     9
#define  PACKET_FLAG_MAX  10
#endif

#define FLAG_ENABLE           1
#define FLAG_UNENABLE       0

#define DEFAULT_DEVICE_ETH0    "eth0"
#define DEFAULT_DEVICE_WLAN0   "wlan0"

#define PACKET_SIZE             	4096	//数据包大小
#define ETH_HW_ADDR_LEN     	6	//mac地址 长度
#define IP_ADDR_LEN             	4   	//ip地址 长度

#define  LENGTH_ETH_HDR 	14     //数据链路层保温首部长度
#define  LENGTH_IP_HDR	20    //IP 数据报首部长度

#define  LENGTH_UDP_HDR 	8    	//UDP报文首部长度
#define  LENGTH_TCP_HDR 	20  	//TCP报文首部长度
#define  LENGTH_ICMP_HDR 	12    	//ICMP(网际控制报文协议)报文首部长度  
#define  LENGTH_IGMP_HDR 	8  	//IGMP(网际组管理协议)报文首部长度  

#define  LENGTH_ARP_HDR 	8   	//ARP报文首部长度
#define  LENGTH_ARP 		28   	//以太网ARP字段长度
#define  LENGTH_RARP_HDR 	28  	//RARP报文首部长度

#define  PACKETSFILE_PATH "./myPcap.pcap"

#define MAX(a,b)       ((a)>(b)?(a):(b))
#define MIN(a,b)       ((a)<(b)?(a):(b))


enum __FLAG_POSITION {
	FLAG_POSITION_RESIVED=0,
	FLAG_POSITION_ALL,
	FLAG_POSITION_ARP,
	FLAG_POSITION_RARP,     
	FLAG_POSITION_ICMP,    
	FLAG_POSITION_IGMP,    
	FLAG_POSITION_IPV6,    
	FLAG_POSITION_LO,     
	FLAG_POSITION_PPP,     
	FLAG_POSITION_802Q,     
	FLAG_POSITION_TCP,     
	FLAG_POSITION_UDP,    
	FLAG_POSITION_MAX
};

struct ethernet_header { 
	unsigned char dest_mac[ETH_HW_ADDR_LEN];  	//目标主机MAC地址 
	unsigned char source_mac[ETH_HW_ADDR_LEN]; 	//源端MAC地址 
	unsigned short eth_type;		//以太网类型 
}; 
struct arp_header  { 
	unsigned short hardware_type; 	//硬件类型：以太网接口类型为1 
	unsigned short protocol_type; 	//协议类型：IP协议类型为0X0800 
	unsigned char add_len; 	//硬件地址长度：MAC地址长度为6B 
	unsigned char pro_len; 	//协议地址长度：IP地址长度为4B 
	unsigned short option;	//操作：ARP请求为1，ARP应答为2 
	unsigned char src_addr[ETH_HW_ADDR_LEN]; 	//源MAC地址：发送方的MAC地址 
	unsigned long src_ip[IP_ADDR_LEN];	 		//源IP地址：发送方的IP地址 
	unsigned char dst_addr[ETH_HW_ADDR_LEN]; 	//目的MAC地址：ARP请求中该字段没有意义；ARP响应中为接收方的MAC地址 
	unsigned long dst_ip[IP_ADDR_LEN]; 			//目的IP地址：ARP请求中为请求解析的IP地址；ARP响应中为接收方的IP地址
	unsigned char padding[18]; 
}; 
struct arp_packet{ //最终arp包结构 
	struct ethernet_header eth; //以太网头部 
	struct arp_header arp; //arp数据包头部 
}; 



const char help[] = " \
				-a  display arp packet \n \
				-t  display tcp packet \n \
				-f  one of RST FIN SYN PUSH ACK URG   display only tcpflag tcp packet \n \
				-u  display udp packet \n \
				-i  display icmp packet \n \
				-r  display rarp packet \n \
				-g  display igmp packet \n \
				-A  display all packet \n \
				-l  also display lo layer packet \n \
				-s  srcip    display srcip packet \n \
				-d  dstip    display dstip packet \n \
				Welcome to use myxray which writed by sztcww Good Luck \n\n";

int g_optionsFlagArray[16] = {  	FLAG_UNENABLE, FLAG_UNENABLE, FLAG_UNENABLE, FLAG_UNENABLE, FLAG_UNENABLE, FLAG_UNENABLE, 
							FLAG_UNENABLE, FLAG_UNENABLE, FLAG_UNENABLE, FLAG_UNENABLE, FLAG_UNENABLE, FLAG_UNENABLE, 
                                                              	FLAG_UNENABLE, FLAG_UNENABLE,  FLAG_UNENABLE, FLAG_UNENABLE
                                                            	};


static int createSockets(char* deviceName);
static int getDataPackets(int fdSock, u_char* bufRecv );
static int dealArpPackets();      //ethprotol == 0x0806   //地址解析协议(Address Resolution Protocol)
static int dealRarpPackets();
static int dealTcpPackets(u_char* bufIP);
static int dealUdpPackets(u_char* bufIP);
static int dealIcmpPackets(u_char* bufIP);
static int dealIgmpPackets(u_char* bufIP);
static int dealLoopbackPackets();
static int dealIpv6Packets();
static int dealDefaultPackets();
static int parseIpPackets(u_char* bufRecv);
static int parseDataPackets(u_char* bufRecv);


static int openPacketsFile();
static int closePacketsFile();
static int writePacketsFile(FILE *fd, char *p);

static FILE *myPcapFd;
static int g_fdSock = -1;

static int g_totalCount = 0;
static int g_tcpCount = 0;
static int g_udpCount = 0;
static int g_icmpCount = 0;
static int g_igmpCount = 0;

static int g_arpCount = 0;
static int g_rarpCount = 0;
static int g_loopCount = 0;
static int g_ipv6Count = 0;
static int g_pppdiscCount = 0;
static int g_pppsecCount = 0;
static int g_otherCount = 0;
static int g_8021qCount = 0;

static int setOptionsFlag(int argc, char* argv[])
{
	int c;

	if (argc == 1) {
		printf("%s\n", help);
		exit(-1);
	}

	while ((c = getopt(argc, argv, "Aatugil?s:d:f:")) != EOF) {
		switch (c) {
		case 'A':
			g_optionsFlagArray[FLAG_POSITION_ARP] =    FLAG_ENABLE;
			g_optionsFlagArray[FLAG_POSITION_RARP] = FLAG_ENABLE;
			g_optionsFlagArray[FLAG_POSITION_ICMP] = FLAG_ENABLE;
			g_optionsFlagArray[FLAG_POSITION_IGMP] = FLAG_ENABLE;
			g_optionsFlagArray[FLAG_POSITION_IPV6] = FLAG_ENABLE;
			g_optionsFlagArray[FLAG_POSITION_TCP] =   FLAG_ENABLE;
			g_optionsFlagArray[FLAG_POSITION_UDP] =   FLAG_ENABLE;
			g_optionsFlagArray[FLAG_POSITION_LO] =     FLAG_ENABLE;
			g_optionsFlagArray[FLAG_POSITION_PPP] =   FLAG_ENABLE;
			g_optionsFlagArray[FLAG_POSITION_802Q] =     FLAG_ENABLE;
			break;
		case 'd':
			printf("%s\n", "d");
			break;
		case 's':
			printf("%s\n", "s");
			break;
		case 'f':
			printf("%s\n", "f");
			break;
		case 'a':
			g_optionsFlagArray[FLAG_POSITION_ARP] = FLAG_ENABLE;
			break;
		case 'r':
			g_optionsFlagArray[FLAG_POSITION_RARP] = FLAG_ENABLE;
			break;
		case 't':
			g_optionsFlagArray[FLAG_POSITION_TCP] = FLAG_ENABLE;
			break;
		case 'i':
			g_optionsFlagArray[FLAG_POSITION_ICMP] = FLAG_ENABLE;
			break;
		case 'g':
			g_optionsFlagArray[FLAG_POSITION_IGMP] = FLAG_ENABLE;
			break;
		case 'u':
			g_optionsFlagArray[FLAG_POSITION_UDP] = FLAG_ENABLE;
			break;
		case 'l':
			g_optionsFlagArray[FLAG_POSITION_LO] = FLAG_ENABLE;
			break;
		case 'p':
			g_optionsFlagArray[FLAG_POSITION_PPP] = FLAG_ENABLE;
			break;
		case 'q':
			g_optionsFlagArray[FLAG_POSITION_802Q] = FLAG_ENABLE;
			break;
		case '?':
			printf("%s\n", help);
			exit(-1);
		}  //end  switch
	}  // end while

	printf("setOptionsFlag OK\n");
	return 0;
}


void sig_handler(int sig)
{
	struct ifreq ethreq;

	if(sig == SIGTERM)
		printf("SIGTERM recieved, exiting...\n");
	else if(sig == SIGINT)
		printf("SIGINT recieved, exiting...\n");
	else if(sig == SIGQUIT)
		printf("SIGQUIT recieved, exiting...\n");
	else
		printf("\n");
	
	printf("g_totalCount[%d]\n", g_totalCount);
	printf("g_arpCount[%d]\n", g_arpCount);
	printf("g_rarpCount[%d]\n", g_rarpCount);
	printf("g_loopCount[%d]\n", g_loopCount);
	printf("g_ipv6Count[%d]\n", g_ipv6Count);

	printf("g_tcpCount[%d]\n", g_tcpCount);
	printf("g_udpCount[%d]\n", g_udpCount);
	printf("g_icmpCount[%d]\n", g_icmpCount);
	printf("g_igmpCount[%d]\n", g_igmpCount);
	printf("g_pppdiscCount[%d]\n", g_pppdiscCount);
	printf("g_pppsecCount[%d]\n", g_pppsecCount);
	printf("g_8021qCount[%d]\n", g_8021qCount);

	printf("g_otherCount[%d]\n", g_otherCount);

	// turn off the PROMISCOUS mode
	strncpy(ethreq.ifr_name,  DEFAULT_DEVICE_WLAN0, IFNAMSIZ);
	if(ioctl(g_fdSock, SIOCGIFFLAGS, &ethreq) != -1) {
		ethreq.ifr_flags &= ~IFF_PROMISC;
		ioctl(g_fdSock, SIOCSIFFLAGS, &ethreq);
	}

	closePacketsFile();
	close(g_fdSock);
	exit(0);
}


int capture(int argc, char* argv[])
{
	char device[] =  DEFAULT_DEVICE_WLAN0;
	u_char bufRecv[PACKET_SIZE]; // buffer for receive
	struct sigaction sighandle;
	int dataLen = 0;


   	//TODO: 获取输入配置选项, 设置 g_optionsFlagArray 数组
   	setOptionsFlag( argc,   argv);

	//TODO: 创建socket,从数据链路层接收分组： fd=socket(PF_PACKET,SOCK_RAW,htons(ETH_P_ALL));
   	g_fdSock = createSockets(device);

   	openPacketsFile();

   	//TODO: 信号处理
	sighandle.sa_flags = 0;
	sighandle.sa_handler = sig_handler;
	sigemptyset(&sighandle.sa_mask);
	//sigaddset(&sighandle.sa_mask, SIGTERM);
	//sigaddset(&sighandle.sa_mask, SIGINT);
	//sigaddset(&sighandle.sa_mask, SIGQUIT);
	sigaction(SIGTERM, &sighandle, NULL);
	sigaction(SIGINT, &sighandle, NULL);
	sigaction(SIGQUIT, &sighandle, NULL);

         //TODO: 循环捕获处理包
   	for( ; ; ){
   		//TODO: 从sock接受数据:  
	   	dataLen = getDataPackets( g_fdSock, bufRecv);
	   	if (dataLen <= 0) {
	   		printf("invalid packet/n");
                           continue;
                  	}

                  	g_totalCount ++;

		//TODO:  由  struct ethhdr结构中的h_proto 判断网络层所使用的协议类型
		//            根据上一步得到的协议类型, 做进一步区分; 若是 //0x0800 //IP协议, 则判断是不是ip协议组的子协议:
	   	parseDataPackets( bufRecv);
	}

   	return 0;
}

/**
 * [createSockets description]
 * @param  deviceName [description]
 * @return            [description]
 */
static int createSockets(char* deviceName)
{
	int fdRecv = -1;
	struct ifreq  ethreq;

	printf("createSockets deviceName [%s]\n", deviceName);
	if ( (fdRecv = socket(AF_INET, SOCK_PACKET, htons(ETH_P_ALL))) < 0) {  //ETH_P_ALL:: 0x0003
		perror( "packet socket error");
		exit(-1);
	}

	strncpy(ethreq.ifr_name, deviceName, IFNAMSIZ);
	if (ioctl(fdRecv, SIOCGIFFLAGS, &ethreq) < 0 ) {  //获得flag
		perror("ioctl SIOCGIFFLAGS error 151...");
		close(fdRecv);
		exit(-1);
	}

	////修改网卡成PROMISC(混杂)模式 
	ethreq.ifr_flags |= IFF_PROMISC;   //重置flag标志
	if (ioctl(fdRecv, SIOCSIFFLAGS, &ethreq) < 0 ) {  //改变模式
		perror("ioctl SIOCSIFFLAGS error 158...");
		close(fdRecv);
		exit(-1);
	}

   	return fdRecv;
}

/**
 * [getDataPackets description]
 * @param  fdSock  [description]
 * @param  bufRecv [description]
 * @return         [description]
 */
static int getDataPackets(int fdSock, u_char* bufRecv )
{
	struct sockaddr recvSockAddr;
	int recvSockAddrLen;
	int recvDataLen;

	bzero(&recvSockAddr, sizeof(recvSockAddr)); 
	recvSockAddrLen = sizeof(recvSockAddr);
	bzero(bufRecv, PACKET_SIZE);
	recvDataLen = recvfrom(fdSock, (char *)bufRecv, PACKET_SIZE, 0, &recvSockAddr, (socklen_t *)&recvSockAddrLen);   //捕获数据

	/* Check to see if the packet contains at least complete Ethernet(14), IP(20) and TCP/UDP(8) headers. */
	if (recvDataLen < (LENGTH_ETH_HDR + LENGTH_IP_HDR + MIN(LENGTH_UDP_HDR,LENGTH_TCP_HDR))) {      
		perror("recvfrom error");
		close(fdSock);
		exit(-1);
	}
	bufRecv[recvDataLen] = '\0';

	printf("================getDataPackets  recvDataLen[%d]\n", recvDataLen);
	return recvDataLen;
}



/**
 * [parseIpPackets description]
 * @param  bufRecv [description]
 * @return         [description]
 */
static int parseIpPackets(u_char* bufRecv)
{
	struct iphdr * bufIP;
	unsigned char *iphead;

	bufIP = (struct iphdr *)&bufRecv[LENGTH_ETH_HDR];   
	iphead = (unsigned char *)&bufRecv[LENGTH_ETH_HDR];   

	// header length as 32-bit
	printf("IP: Version:%d HeaderLen:%d[%d] TotalLen:%d", (*iphead>>4), (*iphead & 0x0f), (*iphead & 0x0f)*4, (iphead[2]<<8|iphead[3]));
	printf(" iphead:[%d.%d.%d.%d]->[%d.%d.%d.%d]", iphead[12], iphead[13], iphead[14], iphead[15],   iphead[16], iphead[17], iphead[18], iphead[19]);
	printf(" Protocol:%d  \n", bufIP->protocol);

	switch (bufIP->protocol) {   //根据不同协议判断指针类型
	case IPPROTO_TCP:
		printf("[TCP] ");
		if(g_optionsFlagArray[FLAG_POSITION_TCP] ==  FLAG_ENABLE){
			dealTcpPackets((unsigned char *)bufIP);
			g_tcpCount++;
		}
		break;
	case IPPROTO_UDP:
		printf("[UDP]");
		if(g_optionsFlagArray[FLAG_POSITION_UDP] ==  FLAG_ENABLE){
			dealUdpPackets((unsigned char *)bufIP);
			g_udpCount++;
		}
		break;
	case IPPROTO_ICMP:
		printf("[ICMP]");
		if(g_optionsFlagArray[FLAG_POSITION_ICMP] ==  FLAG_ENABLE){
			dealIcmpPackets((unsigned char *)bufIP);
			g_icmpCount++;
		}
		break;
	case IPPROTO_IGMP:
		printf("[IGMP]");
		if(g_optionsFlagArray[FLAG_POSITION_IGMP] ==  FLAG_ENABLE){
			dealIgmpPackets((unsigned char *)bufIP);
			g_igmpCount++;
		}
		break;
	default:
		printf("Unkown pkt, protocl:%d\n", bufIP->protocol);
		dealDefaultPackets();
		break;
	} //end switch
	printf(" PORT [%d]->[%d]\n", (iphead[20]<<8|iphead[21]), (iphead[22]<<8|iphead[23]));

	writePacketsFile(myPcapFd, (char *)bufRecv);

	printf("====parseIpPackets  end...\n");
	return 0;
}

/**
 * [parseDataPackets description]
 * @param  bufRecv [description]
 * @return         [description]
 */
static int parseDataPackets(u_char* bufRecv)
{
	struct ethhdr *eth ;
	int ethprotol;

	eth = (struct ethhdr *)bufRecv;
	ethprotol = ntohs(eth->h_proto);

	switch(ethprotol){
		case ETH_P_LOOP:   	// 0x0060 /* Ethernet Loopback packet     */
			printf("ETH_P_LOOP eth Protol[0x%0x]....\n", ethprotol);
			if(g_optionsFlagArray[FLAG_POSITION_LO] ==  FLAG_ENABLE){
				dealLoopbackPackets();
				g_loopCount++;
			}
			break;
		case ETH_P_IP :   	// 0x0800  /* Internet Protocol packet     */
			printf("ETH_P_IP eth Protol[0x%0x]....\n", ethprotol);
			parseIpPackets( bufRecv);
			break;
		case ETH_P_ARP :  	// 0x0806 /* Address Resolution packet    */
			printf("ETH_P_ARP eth Protol[0x%0x]....\n", ethprotol);
			if(g_optionsFlagArray[FLAG_POSITION_ARP] ==  FLAG_ENABLE){
				dealArpPackets();
				g_arpCount++;
			}
			break;
		case ETH_P_RARP :  	// 0x8035  /* Reverse Addr Res packet      */
			printf("ETH_P_RARP eth Protol[0x%0x]....\n", ethprotol);
			if(g_optionsFlagArray[FLAG_POSITION_RARP] ==  FLAG_ENABLE){
				dealRarpPackets();
				g_rarpCount++;
			}
			break;
		case  ETH_P_IPV6 : 	// 0x86DD /* IPv6 over bluebook           */
			printf("ETH_P_IPV6 eth Protol[0x%0x]....\n", ethprotol);
			if(g_optionsFlagArray[FLAG_POSITION_IPV6] ==  FLAG_ENABLE){
				dealIpv6Packets();
				g_ipv6Count++;
			}
			break;
		case  ETH_P_PPP_DISC : 	// 0x8863  /* PPPoE discovery messages     */
			printf("ETH_P_PPP_DISC eth Protol[0x%0x]....\n", ethprotol);
			if(g_optionsFlagArray[FLAG_POSITION_PPP] ==  FLAG_ENABLE){
				//dealIpv6Packets();
				g_pppdiscCount++;
			}
			break;
		case  ETH_P_PPP_SES : 	// 0x8864 /* PPPoE session messages       */
			printf("ETH_P_PPP_SES eth Protol[0x%0x]....\n", ethprotol);
			if(g_optionsFlagArray[FLAG_POSITION_PPP] ==  FLAG_ENABLE){
				//dealIpv6Packets();
				g_pppsecCount++;
			}
			break;
		case  ETH_P_8021Q : 	// 0x86DD /* 802.1Q VLAN Extended Header  */
			printf("ETH_P_8021Q eth Protol[0x%0x]....\n", ethprotol);
			if(g_optionsFlagArray[FLAG_POSITION_802Q] ==  FLAG_ENABLE){
				//dealIpv6Packets();
				g_8021qCount++;
			}
			break;

		default:
			printf("default  eth Protol[0x%0x]....\n", ethprotol);
			g_otherCount++;
			break;
	}

	printf("===========================parseDataPackets  end...\n");
	return 0;
}


/**
 * [dealArpPackets description]
 * 	ARP首部和请求应答的数据结构:
	 	以太网    | 以太网 | 帧    | 硬件 | 协议| 硬件 | 协议 | OP| 发送端       |发送端|目的以太|目的|
		目的地址| 源地址 | 类型| 类型 | 类型| 长度 | 长度 |      |以太网地址|  IP     |网地址    | IP  |
	  		6 	      6           2         2          2         1          1         2           6                 4               6           4
		|<------以太网首部------->|<------------------------------28字节的ARP请求/应答------------------------>|

 * @param  bufRecv [description]
 * @return         [description]
 */
static int dealArpPackets(u_char* bufRecv)      //ethprotol == 0x0806   //地址解析协议(Address Resolution Protocol)
{
	struct arp_packet *arphead;

	arphead = (struct arp_packet *)bufRecv;

#if 0
	memcpy(&sndrAddr, arphead->arp.src_ip, IP_ADDR_LEN);
	memcpy(&rcptAddr, arphead->arp.dst_ip, IP_ADDR_LEN);
	srcAddr = inet_ntoa(sndrAddr);
	dstAddr = inet_ntoa(rcptAddr);
#endif
	time_t temptime = time(NULL);
	printf("Recorded Time:%s \n ", ctime(&temptime) );
	//printf(" Protocol:0x%x \n",  (arphead->eth).eth_type );

#if 0
	printf(" srcMAC:%s--->dstMAC:%s Protocol:0x%x \n",  (arphead->eth).source_mac, (arphead->eth).dest_mac, (arphead->eth).eth_type );

	printf(" srcMAC:[%x:%x:%x:%x:%x:%x]--->dstMAC:[%x:%x:%x:%x:%x:%x] Protocol:0x%x \n", 
			(arphead->eth).source_mac[0], (arphead->eth).source_mac[1], (arphead->eth).source_mac[2], (arphead->eth).source_mac[3], (arphead->eth).source_mac[4], (arphead->eth).source_mac[5] 
			(arphead->eth).dest_mac[0], (arphead->eth).dest_mac[1], (arphead->eth).dest_mac[2], (arphead->eth).dest_mac[3], (arphead->eth).dest_mac[4], (arphead->eth).dest_mac[5],
			(arphead->eth).eth_type );
 
	printf("arpHWR:  hwType:%d Protol:0x%x  hwAddrSize:%d  protocolAddrSize:%d  option:%d    srcIP:%s  dstMac:%s \n",
			ntohs(arphead->arp.hardware_type), ntohs(arphead->arp.protocol_type), arphead->arp.add_len, arphead->arp.pro_len,
			ntohs(arphead->arp.option), arphead->arp.src_addr,    arphead->arp.dst_addr );

	printf("arpHWR:  hwType:%d Protol:0x%x  hwAddrSize:%d  protocolAddrSize:%d  option:%d  srcMac:%s  srcIP:%s  dstMac:%s  dstIP:%s\n",
			ntohs(arphead->arp.hardware_type), ntohs(arphead->arp.protocol_type), arphead->arp.add_len, arphead->arp.pro_len,
			ntohs(arphead->arp.option), arphead->arp.src_addr, arphead->arp.src_ip,  arphead->arp.dst_addr,  arphead->arp.dst_ip);
#endif
	writePacketsFile(myPcapFd, (char *)bufRecv);

	printf("====dealArpPackets  end...\n");
	return 0;
}

/**
 * [dealRarpPackets description]
 * @param  bufRecv [description]
 * @return         [description]
 */
static int dealRarpPackets(u_char* bufRecv)
{

	writePacketsFile(myPcapFd, (char *)bufRecv);

	printf("====dealRarpPackets  end...\n");
	return 0;
}

/**
 * [dealTcpPackets description]
 * @param  bufIP [description]
 * @return       [description]
 */
static int dealTcpPackets(u_char* bufIP)
{
	struct tcphdr *bufTCP;
	char *flagStr;
	int flag;

	bufTCP = (struct tcphdr *)&bufIP[LENGTH_IP_HDR];  //指向tcp头部
	flag = bufTCP->th_flags;
	if (flag & TH_PUSH)   	      //PSH表示有DATA数据传输, PSH(push传送)
		flagStr = "PUSH";
	else if (flag & TH_ACK)   //ACK表示响应, ACK(acknowledgement 确认) 
		flagStr = "ACK";
	else if (flag & TH_URG)   //URG(urgent紧急)
		flagStr = "URG";
	else if (flag & TH_FIN)   //FIN表示关闭连接, FIN(finish结束) 
		flagStr = "FIN";
	else if (flag & TH_SYN)  //SYN表示建立连接, SYN(synchronous建立联机)
		flagStr = "SYN";     
	else if (flag & TH_RST)   //RST(reset重置) 
		flagStr = "RST";
	else
		flagStr = "DEFAULT";

	printf("TCP pkt:  [%s] :FORM:[%s:%d]  TO:[%s:%d]\n",  flagStr, 
			inet_ntoa(*(struct in_addr *) & ( ((struct iphdr *)bufIP)->saddr)), ntohs(bufTCP->source), 
			inet_ntoa(*(struct in_addr *) & ( ((struct iphdr *)bufIP)->daddr)), ntohs(bufTCP->dest));


	printf("====dealTcpPackets  end...\n");
	return 0;
}

/**
 * [dealUdpPackets description]
 * @param  bufIP [description]
 * @return       [description]
 */
static int dealUdpPackets(u_char* bufIP)
{
	struct udphdr *bufUDP;

	bufUDP = (struct udphdr *)&bufIP[LENGTH_IP_HDR]; //指向udp头部
	printf("UDP pkt:   PayloadLen:%d FORM [%s:%d] TO [%s:%d]\n", ntohs(bufUDP->len), 
			inet_ntoa(*(struct in_addr *) & ( ((struct iphdr *)bufIP)->saddr)), ntohs(bufUDP->source), 
			inet_ntoa(*(struct in_addr *) & ( ((struct iphdr *)bufIP)->daddr)), ntohs(bufUDP->dest) );


	printf("====dealUdpPackets  end...\n");
	return 0;
}

/**
 * [dealIcmpPackets description]
 * @param  bufIP [description]
 * @return       [description]
 */
static int dealIcmpPackets(u_char* bufIP)
{
	struct icmp * bufICMP;

	bufICMP = (struct icmp *)&bufIP[LENGTH_IP_HDR];
	printf("ICMP pkt:%s\n", inet_ntoa(*(struct in_addr *) & (((struct iphdr *)bufIP)->saddr)));

	printf("====dealIcmpPackets  end...\n");
	return 0;
}

/**
 * [dealIgmpPackets description]
 * @param  bufIP [description]
 * @return       [description]
 */
static int dealIgmpPackets(u_char* bufIP)
{
	struct igmp *bufIGMP;

	bufIGMP = (struct igmp *)&bufIP[LENGTH_IP_HDR];
	printf("IGMP pkt:%s\n", inet_ntoa(*(struct in_addr *) & (((struct iphdr *)bufIP)->saddr)));

	printf("====dealIgmpPackets  end...\n");
	return 0;
}

/**
 * [dealLoopbackPackets description]
 * @param  bufRecv [description]
 * @return         [description]
 */
static int dealLoopbackPackets(u_char* bufRecv)
{
	writePacketsFile(myPcapFd, (char *)bufRecv);

	printf("====dealLoopbackPackets  end...\n");
	return 0;
}

/**
 * [dealIpv6Packets description]
 * @param  bufRecv [description]
 * @return         [description]
 */
static int dealIpv6Packets(u_char* bufRecv)
{
	writePacketsFile(myPcapFd, (char *)bufRecv);

	printf("====dealIpv6Packets  end...\n");
	return 0;
}

/**
 * [dealDefaultPackets description]
 * @param  bufRecv [description]
 * @return         [description]
 */
static int dealDefaultPackets(u_char* bufRecv)
{
	writePacketsFile(myPcapFd, (char *)bufRecv);

	printf("====dealDefaultPackets  end...\n");
	return 0;
}


/**
 * [openPacketsFile description]
 * @return [description]
 */
static int openPacketsFile()
{
	if ( (myPcapFd = fopen(PACKETSFILE_PATH, "ab+")) == NULL) {
		perror("fopen");
		exit(-1);
	}

	printf("====openPacketsFile  end...\n");
	return 0;
}

/**
 * [writePacketsFile description]
 * @param  fd [description]
 * @param  p  [description]
 * @return    [description]
 */
static int writePacketsFile(FILE *fd, char *p)
{
	char writeBuf[PACKET_SIZE];
	time_t temptime;

	bzero(writeBuf, PACKET_SIZE);
	snprintf(writeBuf, PACKET_SIZE, "This  packet catch times %s \n",  ctime(&temptime));

	fwrite(writeBuf, strlen(writeBuf), 1, fd);

	printf("====int writePacketsFile  end...\n");
	return 0;
}

/**
 * [closePacketsFile description]
 * @return [description]
 */
static int closePacketsFile()
{
	fclose(myPcapFd);

	printf("====closePacketsFile  end...\n");
	return 0;
}
























