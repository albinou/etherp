#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <netinet/if_ether.h>
#include <netpacket/packet.h>
#include <signal.h>
#include <sys/time.h>
#include <zlib.h>

#include "etherp.h"

static struct option etherp_send_options[] = {
	{ "interface", required_argument, NULL, 'I' },
	{ "interval", required_argument, NULL, 'i' },
	{ "count", required_argument, NULL, 'c' },
	{ "size", required_argument, NULL, 's' },
	{ "vary-size", no_argument, NULL, 'V' },
	{ "verbose", no_argument, NULL, 'v' },
	{ "help", no_argument, NULL, 'h' },
	{ NULL, 0, NULL, 0 }
};

static char *etherp_send_options_help[][2] = {
	{ "IF", "interface used to send raw data" },
	{ "US", "wait US microseconds between frames" },
	{ "NB", "send NB frames and exit" },
	{ "SIZE", "send the size of frames (default 1500)" },
	{ NULL, "send frames with various sizes" },
	{ NULL, "activate the verbose" },
	{ NULL, "display this help and exit" },
	{ NULL, NULL }
};

static int etherp_verbose = 0;  /**< This program will verbose
                                   if (verbose != 0) */
static int etherp_quit = 0;     /**< if set, send the last frame and exit */
static unsigned long long etherp_nb_bytes = 0;

static void etherp_send_usage(const char *prog_name)
{
	int i;
	int tmp;
	char spaces[16] = "                ";

	printf("Usage: %s [OPTION] MAC_ADDRESS\n", prog_name);
	printf("Send Ethernet frames (\"pings\") to MAC_ADDRESS\n");
	printf("\n");
	for (i = 0; etherp_send_options[i].name != NULL; ++i) {
		if (etherp_send_options_help[i][0] == NULL) {
			printf("  -%c, --%-15s %s\n", etherp_send_options[i].val,
			       etherp_send_options[i].name, etherp_send_options_help[i][1]);
		} else {
			tmp = strlen(etherp_send_options[i].name) +
				strlen(etherp_send_options_help[i][0]);
			tmp = (tmp > 0) ? (15 - tmp) : 0;
			spaces[tmp] = '\0';
			printf("  -%c, --%s=%s%s%s\n", etherp_send_options[i].val,
			       etherp_send_options[i].name, etherp_send_options_help[i][0],
			       spaces, etherp_send_options_help[i][1]);
			spaces[tmp] = ' ';
		}
	}
}

static int etherp_if_mac_addr(int s, const char *ifname,
                              unsigned char mac_addr[ETH_ALEN])
{
	struct ifreq ifr;
	int i;

	strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
	if (ioctl(s, SIOCGIFHWADDR, &ifr) == -1) {
		perror("Cannot get MAC address");
		return errno;
	}

	for (i = 0; i < ETH_ALEN; ++i)
		mac_addr[i] = (unsigned char) ifr.ifr_hwaddr.sa_data[i];

	return 0;
}

static int etherp_mac_str2array(const char *macstr,
                                unsigned char mac_addr[ETH_ALEN])
{
	if (sscanf(macstr, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
	           &mac_addr[0],
	           &mac_addr[1],
	           &mac_addr[2],
	           &mac_addr[3],
	           &mac_addr[4],
	           &mac_addr[5]) != 6)
		return -1;
	else
		return 0;
}

static void etherp_signal_display_bitrate(int sig)
{
	printf("Sending at %lld mbit/s (%.2f MB/s)        \r",
	       etherp_nb_bytes * 8 / 1000000, etherp_nb_bytes / 1000000.);
	etherp_nb_bytes = 0;
	fflush(stdout);
}

static void etherp_signal_quit(int sig)
{
	etherp_quit = 1;
}

static int etherp_send_frames(const char *macstr_dst, const char *ifname,
                              long int interval, int count, int size)
{
	int s;
	int ret;
	int i;
	struct sockaddr_ll s_addr;
	void *buf;
	unsigned char *etherhead;
	unsigned char *data;
	struct ethhdr *eh;
	struct etherp_hdr *etherph;
	unsigned char mac_dst[ETH_ALEN];
	unsigned char mac_src[ETH_ALEN];
	uint32_t id = 0;
	size_t sz = (size == -1) ? ETH_DATA_LEN_MIN : size;
	int quit_next_loop = 0;
	unsigned long long nb_bytes = 0;
	unsigned long long nb_frames = 0;

	s = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (s == -1) {
		perror("Cannot open the raw socket");
		return errno;
	}

	buf = malloc(ETH_FRAME_LEN);
	etherhead = buf;
	data = buf + ETH_HLEN;
	eh = (struct ethhdr*) etherhead;
	etherph = (struct etherp_hdr *) data;

	if ((ret = etherp_if_mac_addr(s, ifname, mac_src)))
		return ret;

	if ((ret = etherp_mac_str2array(macstr_dst, mac_dst)))
		return ret;

	s_addr.sll_family = PF_PACKET;
	s_addr.sll_protocol = htons(ETHERTYPE_ETHERP);
	s_addr.sll_ifindex = if_nametoindex(ifname);
	s_addr.sll_hatype = ARPHRD_ETHER;
	s_addr.sll_pkttype = PACKET_OTHERHOST;
	s_addr.sll_halen = ETH_ALEN;
	for (i = 0; i < ETH_ALEN; ++i)
		s_addr.sll_addr[i] = mac_dst[i];

	memcpy(&(eh->h_dest), mac_dst, ETH_ALEN);
	memcpy(&(eh->h_source), mac_src, ETH_ALEN);
	eh->h_proto = htons(ETHERTYPE_ETHERP);

	while (1) {

		etherph->id = htonl(id);
		etherph->stop = (count == 1) || quit_next_loop;

		for (i = sizeof (struct etherp_hdr); i < sz; ++i) {
			data[i] = (unsigned char)((int) (255. * rand() / RAND_MAX));
		}
		etherph->crc32 = crc32(crc32(0L, Z_NULL, 0), data + sizeof (struct etherp_hdr),
		                       sz - sizeof (struct etherp_hdr));

		ETHERP_VPRINT("Sending frame to %s ID=%u\n", macstr_dst, id);
		if (((ret = sendto(s, buf, ETH_HLEN + sz, 0, (struct sockaddr*) &s_addr,
		                   sizeof (s_addr))) == -1) &&
		    !etherp_quit) {
			perror("Can't send Ethernet frame");
			return errno;
		}

		++id;
		etherp_nb_bytes += ETH_HLEN + sz;
		nb_bytes += ETH_HLEN + sz;
		nb_frames += 1;

		if (count != -1)
			--count;
		if ((count  == 0) || quit_next_loop)
			break;
		quit_next_loop = etherp_quit;

		if (size == -1) {
			++sz;
			if (sz > ETH_DATA_LEN)
				sz = ETH_DATA_LEN_MIN;
		}
		if ((interval > 0) && !quit_next_loop)
			usleep(interval);
	}

	close(s);

	printf("\n\n");
	printf("--- etherp-send statictics ---\n");
	printf("%llu frames sent, %.02f MB sent\n", nb_frames, nb_bytes / 1000000.);

	return 0;
}

int main(int argc, char *argv[])
{
	int c;
	char *endstr;
	const char *ifname = "eth0";  /**< Interface used to send frames */
	long int interval = -1;       /**< Wait interval us between each sent frame
	                                 if (interval == -1), don't wait */
	int count = -1;               /**< Nb of frames to send
	                                 if (count == -1), send indefinitely */
	int size = ETH_DATA_LEN;      /**< Size of frames to send
	                                 if (size == -1), vary the size */
	struct sigaction sa_sigint;
	struct sigaction sa_sigalrm;
	struct itimerval itv;

	while (1) {
		c = getopt_long(argc, argv, "hI:i:c:s:Vv", etherp_send_options, NULL);

		if (c == -1)
			break;

		switch (c) {
			case 'h':
				etherp_send_usage(argv[0]);
				return 0;
				break;
			case 'I':
				ifname = optarg;
				break;
			case 'i':
				interval = strtoll(optarg, &endstr, 10);
				if ((*endstr != '\0') || (interval <= 0)) {
					fprintf(stderr, "The interval must an integer greater than 0\n");
					return 1;
				}
				break;
			case 'c':
				count = strtol(optarg, &endstr, 10);
				if ((*endstr != '\0') || (count <= 0)) {
					fprintf(stderr, "The count must an integer greater than 0\n");
					return 1;
				}
				break;
			case 's':
				size = strtol(optarg, &endstr, 10);
				if ((*endstr != '\0') ||
				    (size < ETH_DATA_LEN_MIN) || (size > ETH_DATA_LEN)) {
					fprintf(stderr, "The size must an integer between %d and %d\n",
					        ETH_DATA_LEN_MIN, ETH_DATA_LEN);
					return 1;
				}
				break;
			case 'V':
				size = -1;
				break;
			case 'v':
				etherp_verbose = 1;
				break;
			default:
				etherp_send_usage(argv[0]);
				return 1;
				break;
		}
	}

	if ((argc - optind) != 1) {
		etherp_send_usage(argv[0]);
		return 1;
	}

	sa_sigint.sa_handler = etherp_signal_quit;
	sigemptyset(&sa_sigint.sa_mask);
	sigaction(SIGINT, &sa_sigint, NULL);
	if (!etherp_verbose) {
		/* Install a timer to display the bitrate every 1 second */
		sa_sigalrm.sa_handler = etherp_signal_display_bitrate;
		sigemptyset(&sa_sigalrm.sa_mask);
		sa_sigalrm.sa_flags = SA_RESTART;
		sigaction(SIGALRM, &sa_sigalrm, NULL);
		itv.it_value.tv_sec = 1;
		itv.it_value.tv_usec = 0;
		itv.it_interval.tv_sec = 1;
		itv.it_interval.tv_usec = 0;
		setitimer(ITIMER_REAL, &itv, NULL);
	}

	return etherp_send_frames(argv[optind], ifname,
	                          interval, count, size) ? 2 : 0;
}
