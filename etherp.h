#ifndef ETHERP_H_
# define ETHERP_H_

# include <stdint.h>

# define ETH_DATA_LEN_MIN (ETH_ZLEN - ETH_HLEN)

# define ETHERTYPE_ETHERP 0x4242

# define ETHERP_VPRINT(fmt, args...) do { \
		if (etherp_verbose) \
			printf(fmt, ## args); \
	} while (0);

struct etherp_hdr {
	uint32_t      id;             /**< ID of the frame */
	uint32_t      crc32;          /**< CRC32 of the frame's data */
	uint8_t       stop;           /**< last frame if (stop == 1) */
} __attribute__((packed));

#endif /* ETHERP_H_ */
