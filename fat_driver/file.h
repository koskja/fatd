#include "fat.h"

struct file_entry {
    uint8_t filename[11];
    uint8_t attributes;
    uint8_t flags;
    uint8_t create_cs;
    uint16_t create_time;
    uint16_t create_date;
    uint16_t last_date;
    uint16_t entry_hi;
    uint16_t mod_time;
    uint16_t mod_date;
    uint16_t entry_lo;
    uint32_t size;
} __attribute__((packed));

struct lfn_entry {
    uint8_t order;
    uint8_t char0[10];
    uint8_t attr;
    uint8_t type;
    uint8_t checksum;
    uint8_t char1[12];
    uint8_t zero[2];
    uint8_t char2[4];
} __attribute__((packed));

uint64_t next_entry(struct fat_device *self, uint64_t *entry, struct file_entry *out);
uint64_t cluster_first_entry(struct fat_device *self, uint64_t cluster);
