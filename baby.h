#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/in.h>
#include <linux/icmp.h>

#define ICMP_ECHO_LEN 1200
#define MEM_CRT_OFFSET 136
#define MEM_LINES 32

#define memcpy __builtin_memcpy
#define swab16(x) ___constant_swab16(x)
#define crt_offset(x, y) (MEM_CRT_OFFSET + MEM_LINES * x + y + x)

enum opcodes
{
    JMP,
    JRP,
    LDN,
    STO,
    SUB,
    SB2,
    CMP,
    STP
};

typedef struct
{
    __u32 mem[32];
    __u32 acc;
    __u32 pc;
} cpu_frame;
