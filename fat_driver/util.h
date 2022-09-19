#ifndef FAT_UTIL_H
#define FAT_UTIL_H
// Allocate a block of `size` bytes and assign it to `variable`.
#define MALLOC_ASSIGN_SIZE(var, ...) ((var) = malloc(__VA_ARGS__))
// Fill a pointer with an allocation of the same size as its type.
#define MALLOC_ASSIGN(ptr) ((ptr) = malloc(sizeof(*(ptr))))
// Calls return on its parameter if it is not zero.
#define PROPAGATE_ERRNO(...)                \
	do {                                    \
		uint64_t __prop_errno;              \
		if ((__prop_errno = __VA_ARGS__)) { \
			return __prop_errno;            \
		}                                   \
	} while (0)

#endif
