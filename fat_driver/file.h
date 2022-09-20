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


struct entry_union {
    uint8_t entry_type;
    union {
        struct file_entry file_entry;
        struct lfn_entry lfn_entry;
    };
};

uint64_t next_entry_raw(struct fat_device *self, uint64_t *entry, struct file_entry *out);
uint64_t next_entry(struct fat_device *self, uint64_t *entry, struct entry_union *out);
uint64_t cluster_first_entry(struct fat_device *self, uint64_t cluster);
uint64_t read_next_entry(struct fat_device *self, uint64_t *entry, struct entry_union *out);

#define READ_ONLY 0x01
#define HIDDEN    0x02
#define SYSTEM    0x04
#define VOLUME_ID 0x08
#define DIRECTORY 0x10
#define ARCHIVE   0x20
#define LFN_FLAGS (READ_ONLY | HIDDEN | SYSTEM | VOLUME_ID)

#define ENTRY_LAST 0
#define ENTRY_FILE 1
#define ENTRY_LFN  2
#define ENTRY_UNUSED 3   