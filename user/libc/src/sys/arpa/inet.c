#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

in_addr_t	inet_addr(const char *cp){

     unsigned int ip = 0;
    unsigned int a,b,c,d;

    if(sscanf(cp,"%u.%u.%u.%u",&a,&b,&c,&d) != 4)
        return INADDR_NONE;

    ip = (a << 24) |
         (b << 16) |
         (c << 8 ) |
         (d);

    return htonl(ip);
}


in_addr_t inet_lnaof(struct in_addr in) {
    unsigned int ip = ntohl(in.s_addr);
    // assume classe A/B/C padrão
    if ((ip & 0x80000000) == 0)        // Classe A
        return ip & 0x00FFFFFF;
    else if ((ip & 0xC0000000) == 0x80000000) // Classe B
        return ip & 0x0000FFFF;
    else                                // Classe C
        return ip & 0x000000FF;
}

in_addr_t inet_netof(struct in_addr in) {
    unsigned int ip = ntohl(in.s_addr);
    if ((ip & 0x80000000) == 0)         // Classe A
        return ip >> 24;
    else if ((ip & 0xC0000000) == 0x80000000) // Classe B
        return ip >> 16;
    else                                 // Classe C
        return ip >> 8;
}

struct in_addr inet_makeaddr(in_addr_t net, in_addr_t lna) {
    struct in_addr addr;
    addr.s_addr = htonl(net | lna);
    return addr;
}

in_addr_t inet_network(const char *cp) {
    unsigned int a,b,c,d;
    a=b=c=d=0;

    int n = sscanf(cp,"%u.%u.%u.%u",&a,&b,&c,&d);
    if(n <= 0) return INADDR_NONE;

    unsigned int net = 0;
    if(n==1) net = a;
    else if(n==2) net = (a<<8) | b;
    else if(n==3) net = (a<<16)|(b<<8)|c;
    else net = (a<<24)|(b<<16)|(c<<8)|d;

    return net;
}

char *inet_ntoa(struct in_addr in) {
    static char str[16]; // precisa ser estático
    unsigned char *bytes = (unsigned char*)&in.s_addr;

    snprintf(str, sizeof(str), "%u.%u.%u.%u",
             bytes[0], bytes[1], bytes[2], bytes[3]);

    return str;
}

/*
 * inet_pton simplificado (IPv4 apenas)
 * af = AF_INET (IPv4)
 * src = string "x.x.x.x"
 * dst = ponteiro para struct in_addr (ou uint32_t)
 */
int inet_pton(int af, const char *src, void *dst) {
    if (!src || !dst) return -1;

    if (af != 2) return -1; // AF_INET = 2

    unsigned int b[4];
    char dummy;

    // sscanf tenta ler 4 octetos (0-255) e garantir que nada extra exista
    if (sscanf(src, "%u.%u.%u.%u%c", &b[0], &b[1], &b[2], &b[3], &dummy) != 4){
        return 0; // string inválida
    }

    for (int i = 0; i < 4; i++) {
        if (b[i] > 255) return 0; // octeto inválido
    }

    uint8_t *bytes = (uint8_t*)dst;
    bytes[0] = b[0];
    bytes[1] = b[1];
    bytes[2] = b[2];
    bytes[3] = b[3];

    return 1; // sucesso
}