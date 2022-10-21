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
	uint8_t ordinal;
	uint16_t char0[5];
	uint8_t attr;
	uint8_t type;
	uint8_t checksum;
	uint16_t char1[6];
	uint8_t zero[2];
	uint16_t char2[2];
} __attribute__((packed));

struct entry_union {
	uint8_t entry_type;
	union {
		struct file_entry file_entry;
		struct lfn_entry lfn_entry;
	};
};

struct entry_node {
	struct file_entry entry;
	uint64_t _first_total;
	uint64_t _first_lfn;
	uint64_t _last;
	uint64_t _total_len;
	uint64_t _used_len;
	uint8_t attr;
	char name[1665];
};

uint64_t next_entry_raw(struct fat_device *self, uint64_t *entry,
						struct file_entry *out);
uint64_t next_entry(struct fat_device *self, uint64_t *entry,
					struct entry_union *out);
uint64_t cluster_first_entry(struct fat_device *self,
							 uint64_t cluster);
uint64_t read_next_entry(struct fat_device *self, uint64_t *entry,
						 struct entry_union *out);
uint64_t next_node(struct fat_device *self, uint64_t *entry,
				   struct entry_node *out);
uint64_t advance_entry(struct fat_device *self, uint64_t *entry,
					  int64_t n);
uint64_t dir_entries(struct fat_device *self,
					 struct entry_node *node);
uint64_t find_file(struct fat_device *self, struct entry_node *node,
				   const char *filename); // TODO
void destruct_entry(struct fat_device *self, uint64_t entry,
					uint64_t *cluster, uint64_t *offset);
uint64_t entry_mut(struct fat_device *self, uint64_t entry,
				   struct file_entry **out);
uint64_t find_file_start(struct fat_device *self, uint64_t dir_start,
						 uint64_t entry, uint64_t *first_entry,
						 uint64_t *first_used);

uint64_t delete_file(struct fat_device *self, uint64_t dir_start,
					 uint64_t entry);

uint64_t entry_diff(struct fat_device *self, uint64_t a, uint64_t b,
					uint64_t *diff);
uint64_t first_matching(struct fat_device *self, uint64_t dir_start,
						uint64_t len, uint64_t *ofe);

uint64_t create_file(struct fat_device *self, uint64_t dir_start,
					 char *name, uint8_t flags);

#define READ_ONLY 0x01
#define HIDDEN 0x02
#define SYSTEM 0x04
#define VOLUME_ID 0x08
#define DIRECTORY 0x10
#define ARCHIVE 0x20
#define LFN_FLAGS (READ_ONLY | HIDDEN | SYSTEM | VOLUME_ID)

#define ENTRY_LAST 0
#define ENTRY_FILE 1
#define ENTRY_LFN 2
#define ENTRY_UNUSED 3
