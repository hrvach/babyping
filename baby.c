#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/in.h>
#include <linux/icmp.h>
#include "baby.h"


static __always_inline __u16 checksum(void *icmph, int len)
{
    __u32 sum = 0;

    for (int i = 0; i < len; i += 2)
    {
        sum += *(__u16 *)(icmph + i);
        sum = (sum >> 16) + (sum & 0xffff);
    }
    return ~(sum + (sum >> 16));
}

static __always_inline enum xdp_action single_step(cpu_frame *frame)
{
    if (++frame->pc > 31)
        return XDP_PASS;

    __u8 addr = (__u8)(frame->mem[frame->pc] & 0x1f);
    __u8 opcode = (frame->mem[frame->pc] >> 13) & 0x07;

    switch (opcode)
    {
    case JMP:
        frame->pc = frame->mem[addr];
        break;
    case JRP:
        frame->pc += frame->mem[addr];
        break;
    case LDN:
        frame->acc = (__u32)(~frame->mem[addr] + 1);
        break;
    case STO:
        frame->mem[addr] = frame->acc;
        break;
    case SUB:
    case SB2:
        frame->acc = frame->acc - frame->mem[addr];
        break;
    case CMP:
        frame->pc += (frame->acc >> 31);
        break;
    case STP:
        return XDP_PASS;
    }
    return XDP_TX;
}

SEC("ssem")
int ssem_prog(struct xdp_md *ctx)
{
    void *data = (void *)(long)ctx->data;
    void *data_end = (void *)(long)ctx->data_end;

    struct ethhdr *eth = data;
    struct iphdr *ip = data + sizeof(*eth);
    struct icmphdr *icmp = data + sizeof(*eth) + sizeof(*ip);
    unsigned char mac_addr[6] = {0};
    unsigned char display_bit[2] = {'.', 'o'};
    __u32 *payload = (void *)icmp + sizeof(*icmp);

    cpu_frame frame;

    // Verifier safety
    if ((void *)icmp + ICMP_ECHO_LEN > data_end)
        return XDP_PASS;

    if (ip->protocol != IPPROTO_ICMP || icmp->type != ICMP_ECHOREPLY)
        return XDP_PASS;

    if (swab16(ip->tot_len) - sizeof(*ip) != ICMP_ECHO_LEN)
        return XDP_PASS;

    // Fix message type and sequence number (it's network byte order)
    icmp->type = ICMP_ECHO;
    icmp->un.echo.sequence = swab16(swab16(icmp->un.echo.sequence) + 1);

    // Swap incoming and outgoing IPs
    __be32 tmp_addr = ip->saddr;
    ip->saddr = ip->daddr;
    ip->daddr = tmp_addr;

    // Swap MAC addresses
    memcpy(mac_addr, eth->h_source, 6);
    memcpy(eth->h_source, eth->h_dest, 6);
    memcpy(eth->h_dest, mac_addr, 6);

    // Process CPU frame
    memcpy(&frame, payload, sizeof(cpu_frame));
    
    if (single_step(&frame) == XDP_PASS)
	    return XDP_PASS;

    memcpy(payload, &frame, sizeof(cpu_frame));

    // "Display" - write Williams tube memory bits as ASCII data in ICMP payload
    for (__u16 i = 0; i < 32; i++)
        for (__u16 j = 0; j < 33; j++)
            *((__u8 *)payload + crt_offset(i, j)) = j < 32 ? display_bit[(frame.mem[i] >> j) & 1] : 0x0a;

    // Fix checksums
    icmp->checksum = 0;
    icmp->checksum = checksum(icmp, ICMP_ECHO_LEN);

    ip->check = 0;
    ip->check = checksum(ip, sizeof(*ip));

    return XDP_TX;
}

char _license[] SEC("license") = "GPL";
