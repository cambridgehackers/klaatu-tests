/*
  Look up a host name
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <stdio.h>  /* for printf */
#include <string.h> /* for memset */
#include <stdlib.h>
#include <netinet/in.h>  /* for IPPROTO_TCP */

const char PORT_NUMBER[] = "80";

static void write_data(char *name, struct addrinfo *rp)
{
/*
    printf("flags=%d", rp->ai_flags);

    if (rp->ai_family == AF_INET)       printf(" AF_INET");
    else if (rp->ai_family == AF_INET6) printf(" AF_INET6");
    else                                printf(" family=%d\n", rp->ai_family);

    if (rp->ai_socktype == SOCK_DGRAM)       printf(" DGRAM");
    else if (rp->ai_socktype == SOCK_STREAM) printf(" STREAM");
    else                                     printf(" socktype=%d\n", rp->ai_socktype);
	
    if (rp->ai_protocol == IPPROTO_UDP)      printf(" UDP");
    else if (rp->ai_protocol == IPPROTO_TCP) printf(" TCP");
    else                                     printf(" protocol=%d\n", rp->ai_protocol);

    if (rp->ai_canonname)
	printf(" CanonicalName='%s'", rp->ai_canonname);
*/
    if (rp->ai_family == AF_INET) {
	char buf[INET_ADDRSTRLEN];
	struct sockaddr_in *addr = (struct sockaddr_in *) rp->ai_addr;
	inet_ntop(AF_INET, &addr->sin_addr, buf, INET_ADDRSTRLEN);
	printf("%s has address %s\n", name, buf);
    }
    else if (rp->ai_family == AF_INET6) {
	char buf[INET6_ADDRSTRLEN];
	struct sockaddr_in6 *addr = (struct sockaddr_in6 *) rp->ai_addr;
	inet_ntop(AF_INET6, &addr->sin6_addr, buf, INET6_ADDRSTRLEN);
	printf("%s has IPv6 address %s\n", name, buf);
    }
    else {
	//printf(" unknown-address\n");
    }
}

int main(int argc, char **argv)
{
    struct addrinfo  hints;
    struct addrinfo* res;
    int              ret;

    if (argc != 2) {
	printf("Must supply a hostname\n");
	exit(0);
    }

    // In theory this avoids the caching DNS - which is in Java, I believe
    setenv("ANDROID_DNS_MODE", "local", 1);

    memset(&hints, 0, sizeof(hints));
//    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    /* first, try without any hints */
    ret = getaddrinfo( argv[1], PORT_NUMBER, &hints, &res);
    if (ret != 0) {
        printf("first getaddrinfo returned error: %s\n", gai_strerror(ret));
        return 1;
    }

    for (struct addrinfo *rp = res ; rp != NULL ; rp = rp->ai_next) 
	write_data(argv[1], rp);

    freeaddrinfo(res);
    return 0;
}

