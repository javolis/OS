/* dns.c - minimal DNS resolver: build an A-record query, send it to the
 * configured DNS server over UDP, and parse the first A answer. Handles
 * compressed names by skipping (we never need to read the answer name). */
#include <stddef.h>
#include <stdint.h>

#include "dns.h"
#include "net.h"
#include "udp.h"

/* Encode "a.b.c" as length-prefixed labels terminated by a 0 byte. Returns
 * the encoded length, or -1 on overflow / a bad label. */
static int encode_name(const char *name, uint8_t *buf, int max) {
    int o = 0;
    const char *p = name;
    while (*p) {
        const char *start = p;
        int label = 0;
        while (*p && *p != '.') {
            p++;
            label++;
        }
        if (label == 0 || label > 63 || o + 1 + label >= max)
            return -1;
        buf[o++] = (uint8_t)label;
        for (int i = 0; i < label; i++)
            buf[o++] = (uint8_t)start[i];
        if (*p == '.')
            p++;
    }
    if (o + 1 > max)
        return -1;
    buf[o++] = 0;
    return o;
}

/* Advance past a (possibly compressed) DNS name starting at offset p. */
static int skip_name(const uint8_t *m, int len, int p) {
    while (p < len) {
        uint8_t b = m[p];
        if (b == 0)
            return p + 1;
        if ((b & 0xC0) == 0xC0)
            return p + 2; /* a pointer terminates the name */
        p += b + 1;
    }
    return p;
}

int dns_resolve(const char *name, ipaddr_t *out) {
    uint8_t q[300];
    q[0] = 0x12;
    q[1] = 0x34; /* transaction id */
    q[2] = 0x01;
    q[3] = 0x00; /* flags: recursion desired */
    q[4] = 0x00;
    q[5] = 0x01; /* qdcount = 1 */
    q[6] = 0x00;
    q[7] = 0x00; /* ancount */
    q[8] = 0x00;
    q[9] = 0x00; /* nscount */
    q[10] = 0x00;
    q[11] = 0x00; /* arcount */
    int o = 12;
    int n = encode_name(name, q + o, (int)sizeof(q) - o - 4);
    if (n < 0)
        return 0;
    o += n;
    q[o++] = 0x00;
    q[o++] = 0x01; /* qtype A */
    q[o++] = 0x00;
    q[o++] = 0x01; /* qclass IN */

    uint8_t resp[600];
    int rlen = udp_request(net_dns(), 53, 0xC000, q, (uint16_t)o, resp,
                           (uint16_t)sizeof(resp));
    if (rlen < 12)
        return 0;
    int qd = (resp[4] << 8) | resp[5];
    int an = (resp[6] << 8) | resp[7];
    if (an < 1)
        return 0;
    int p = 12;
    for (int i = 0; i < qd; i++) {
        p = skip_name(resp, rlen, p);
        p += 4; /* qtype + qclass */
    }
    for (int i = 0; i < an; i++) {
        p = skip_name(resp, rlen, p);
        if (p + 10 > rlen)
            return 0;
        int type = (resp[p] << 8) | resp[p + 1];
        int rdlen = (resp[p + 8] << 8) | resp[p + 9];
        p += 10;
        if (type == 1 && rdlen == 4 && p + 4 <= rlen) {
            *out = ((ipaddr_t)resp[p] << 24) | ((ipaddr_t)resp[p + 1] << 16) |
                   ((ipaddr_t)resp[p + 2] << 8) | (ipaddr_t)resp[p + 3];
            return 1;
        }
        p += rdlen;
    }
    return 0;
}
