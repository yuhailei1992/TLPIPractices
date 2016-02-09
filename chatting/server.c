/*************************************************************************\
*                  Copyright (C) Hailei Yu, 2016.                         *
*                                                                         *
* This program is free software. You may use, modify, and redistribute it *
* under the terms of the GNU General Public License as published by the   *
* Free Software Foundation, either version 3 or (at your option) any      *
* later version. This program is distributed without any warranty.  See   *
* the file COPYING.gpl-v3 for details.                                    *
\*************************************************************************/

/* server.c

   The server side of a simple command line chatting software.

   NOTE: this program must be run under a root login, in order to allow the
   "echo" port (7) to be bound. Alternatively, for test purposes, you can
   replace the SERVICE name below with a suitable unreserved port number
   (e.g., "51000"), and make a corresponding change in the client.

   Reference: Michael Kerrisk TLPI source code listing 60-4.
*/

#include <signal.h>
#include <syslog.h>
#include <sys/wait.h>
#include <inet_sockets.h>       /* Declarations of inet*() socket functions */
#include <tlpi_hdr.h>

#define SERVICE "echo"          /* Name of TCP service */
#define BUF_SIZE 4096

static void             /* SIGCHLD handler to reap dead child processes */
grimReaper(int sig)
{
    int savedErrno;             /* Save 'errno' in case changed here */

    savedErrno = errno;
    while (waitpid(-1, NULL, WNOHANG) > 0)
        continue;
    errno = savedErrno;
}

int
main(int argc, char *argv[])
{
    int lfd, cfd;               /* Listening and connected sockets */
    struct sigaction sa;
    char buf[BUF_SIZE];

    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sa.sa_handler = grimReaper;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        syslog(LOG_ERR, "Error from sigaction(): %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    lfd = inetListen(SERVICE, 10, NULL);
    if (lfd == -1) {
        syslog(LOG_ERR, "Could not create server socket (%s)", strerror(errno));
        exit(EXIT_FAILURE);
    }

    cfd = accept(lfd, NULL, NULL);
    if (cfd == -1) {
        syslog(LOG_ERR, "Failure in accept(): %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    switch (fork()) {
    case -1:
        syslog(LOG_ERR, "Cannot create child (%s)", strerror(errno));
        close(cfd);
        break;
    case 0: /* Child process reads from socket and print to STDOUT */
        for (;;) {
            int numRead = read(cfd, buf, BUF_SIZE);
            if (numRead <= 0)
                break;
            printf("%.*s", (int)numRead, buf);
        }
        exit(EXIT_SUCCESS);

    default: /* Parent process reads from STDIN and write to socket */
        for(;;) {
            int numRead = read(STDIN_FILENO, buf, BUF_SIZE);
            if (numRead <= 0)
                break;
            if (write(cfd, buf, numRead) != numRead)
                fatal("write() failed");
        }

        if (shutdown(cfd, SHUT_WR) == -1)
            errExit("shutdown");
        exit(EXIT_SUCCESS);
    }
}

