#ifndef __PCAP_H__
#define __PCAP_H__

#if 0
//=============================================================================
NOTE: 
      1, 创建socket,从数据链路层接收分组：
            一种为fd=socket(PF_PACKET,SOCK_RAW,htons(ETH_P_ALL));
            另一种为fd=socket(AF_INET,SOCK_PACKET,htons(ETH_P_ALL));
            其中协议族为PF_PACKET套接字使用较多。
            ETH_P_ALL自身定义于  /usr/include/linux/if_ether.h中，ETH_P_ALL占两个字节值为0x0003

      2, 从sock接受数据: 
            datalen = recvfrom(fd_recv, (char *)buf_recv, 4096, 0, &from, &from_len);

      3, 按照 struct ethhdr结构接受到的数据包, 并判断由  struct ethhdr结构中的h_proto 决定的网络层所使用的协议类型, 根据不同的协议类型,对数据包进行操作;

      4, 网络层所使用的协议类型定义在include/linux/ether.h; 常用的类型:
            #define ETH_P_LOOP      0x0060          /* Ethernet Loopback packet     */
             #define ETH_P_IP        0x0800          /* Internet Protocol packet     */
            #define ETH_P_ARP       0x0806          /* Address Resolution packet    */
            #define ETH_P_RARP      0x8035          /* Reverse Addr Res packet      */

            #define ETH_P_8021Q     0x8100          /* 802.1Q VLAN Extended Header  */
            #define ETH_P_8021AD    0x88A8          /* 802.1ad Service VLAN         */
            #define ETH_P_802_EX1   0x88B5          /* 802.1 Local Experimental 1.  */
            #define ETH_P_8021AH    0x88E7          /* 802.1ah Backbone Service Tag */
            #define ETH_P_802_3     0x0001          /* Dummy type for 802.3 frames  */
            #define ETH_P_802_2     0x0004          /* 802.2 frames                 */
            #define ETH_P_TR_802_2  0x0011          /* 802.2 frames                 */

            #define ETH_P_IPV6      0x86DD          /* IPv6 over bluebook           */
            #define ETH_P_PPP_DISC  0x8863          /* PPPoE discovery messages     */
            #define ETH_P_PPP_SES   0x8864          /* PPPoE session messages       */
            #define ETH_P_PPP_MP    0x0008          /* Dummy type for PPP MP frames */

            #define ETH_P_IEEE802154 0x00F6         /* IEEE802.15.4 frame           */



      5, 根据上一步得到的协议类型, 做进一步区分;
            若是 //0x0800 //IP协议, 则判断是不是ip协议组的子协议:
            IPPROTO_IP = 0,               /* Dummy protocol for TCP               */
            IPPROTO_ICMP = 1,             /* Internet Control Message Protocol    */
            IPPROTO_IGMP = 2,             /* Internet Group Management Protocol   */
            IPPROTO_IPIP = 4,             /* IPIP tunnels (older KA9Q tunnels use 94) */
            IPPROTO_TCP = 6,              /* Transmission Control Protocol        */
            IPPROTO_UDP = 17,             /* User Datagram Protocol               */
            IPPROTO_IPV6 = 41,            /* IPv6-in-IPv4 tunnelling              */
            IPPROTO_RAW = 255,            /* Raw IP packets                       */

//=============================================================================
#endif






int capture(int argc, char* argv[]);















#endif