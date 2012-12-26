/*
 * Common declarations of the Etherp project
 *
 * Copyright 2012 Albin Kauffmann <albin.kauffmann@gmail.com>
 *
 * This file is part of Etherp.
 *
 * Ardumotics is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Ardumotics is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Ardumotics.  If not, see <http://www.gnu.org/licenses/>.
 */

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
