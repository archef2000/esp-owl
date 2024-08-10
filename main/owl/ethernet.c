
#include <sys/types.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

//extern char *ether_ntoa(const struct ether_addr *__addr);
struct ether_addr
{
  uint8_t ether_addr_octet[6];
};

char *ether_ntoa(struct ether_addr *addr)
{
  static char buf[18];
  sprintf (buf, "%02x:%02x:%02x:%02x:%02x:%02x",
	   addr->ether_addr_octet[0], addr->ether_addr_octet[1],
	   addr->ether_addr_octet[2], addr->ether_addr_octet[3],
	   addr->ether_addr_octet[4], addr->ether_addr_octet[5]);
  return buf;
}

