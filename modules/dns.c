#include <module.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

MODULE_DEFAULT(dns);

void module_enter(irc_t *irc, const char *channel, const char *user, const char *message) {
    irc_write(irc, channel, "%s: Please wait, resolving: %s", user, message);

    struct addrinfo *res;
    struct addrinfo hints = {
        .ai_family   = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM
    };

    int status;
    if ((status = getaddrinfo(message, NULL, &hints, &res)) != 0) {
        irc_write(irc, channel, "%s: domain name resolution failed", message);
        return;
    }

    char ipbuffer[INET6_ADDRSTRLEN];
    for (struct addrinfo *p = res; p; p = p->ai_next) {
        void *addr;

        if (p->ai_family == AF_INET) {
            // ipv4
            struct sockaddr_in *ipv4 = (struct sockaddr_in*)p->ai_addr;
            addr = &(ipv4->sin_addr);
        } else {
            // ipv6
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6*)p->ai_addr;
            addr = &(ipv6->sin6_addr);
        }

        inet_ntop(p->ai_family, addr, ipbuffer, sizeof(ipbuffer));
        irc_write(irc, channel, "%s: %s", user, ipbuffer);
    }

    freeaddrinfo(res);
}

void module_close(irc_t *irc) {
    // nothing to do
}
