/*
 * Austin Walter
 * Brennen Miller
 * CSCI 446 S16
 * Program 2
 * server.c
*/


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define BACKLOG 10	 // how many pending connections queue will hold

void sigchld_handler(int s)
{
	// waitpid() might overwrite errno, so we save and restore it:
	int saved_errno = errno;

	while(waitpid(-1, NULL, WNOHANG) > 0);

	errno = saved_errno;
}


// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[])
{
	int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr; // connector's address information
	socklen_t sin_size;
	struct sigaction sa;
	int yes=1;
	char s[INET6_ADDRSTRLEN];
	int rv, numbytes;
  int bytes_read = 0;
  int bytes_sent;
  char buf[100];
  int size = 256;

  if (argc != 2) {
    fprintf(stderr,"usage: please enter a port number\n");
    exit(1);
  }

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	if ((rv = getaddrinfo(NULL, argv[1], &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("server: socket");
			continue;
		}

		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
				sizeof(int)) == -1) {
			perror("setsockopt");
			exit(1);
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("server: bind");
			continue;
		}

		break;
	}

	freeaddrinfo(servinfo); // all done with this structure

	if (p == NULL)  {
		fprintf(stderr, "server: failed to bind\n");
		exit(1);
	}

	if (listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}

	sa.sa_handler = sigchld_handler; // reap all dead processes
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}

	while(1) {  // main accept() loop
    sin_size = sizeof their_addr;
		new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
		if (new_fd == -1) {
			perror("accept");
			continue;
		}

		inet_ntop(their_addr.ss_family,
			get_in_addr((struct sockaddr *)&their_addr),
			s, sizeof s);

    if (!fork())
    { // this is the child process
      close(sockfd); // child doesn't need the listener
      if ((numbytes = recv(new_fd, buf, 99, 0)) == -1) {
        send(new_fd, "server: receiving error    ", 26, 0);
        exit(1);
      }

      buf[numbytes] = '\0';
      // Open the file
      int file_desc = open(buf, O_RDONLY);
      
      // Check to see if there were any errors opening the file
      if(file_desc < 0)
      {
        send(new_fd, "server: error reading file", 26, 0);
        exit(1);
      }

      struct stat st;
      // stat() returns -1 on error. Skipping check in this example
      stat(buf, &st);
      int sizef = (int) st.st_size;
      char buffer[sizef];
      int el = 0;
      char *ptr;
      // loop until end of file
      while(bytes_read < sizef)
      {
        ptr = &buffer[el];
        // Read the file contents
        bytes_read += read(file_desc, ptr, size);
        
        // Check if there were any errors reading the file
        if(bytes_read < 0)
        {
          send(new_fd, "server: error reading file", 26, 0);
        }
        
        el = bytes_read / sizeof buffer[0];
      }

        buffer[el] = '\0';
        send(new_fd, "server: all good          ", 26, 0);
        while(bytes_sent != sizef)
        {
          if((bytes_sent = send(new_fd, buffer, sizef, 0)) == -1)
          {
            send(new_fd, "server: error sending     ", 26, 0);
            exit(1);
          }
        }
      close(new_fd);
      exit(0);
    }  
    close(new_fd);  // parent doesn't need this
    exit(0);
  }

	return 0;
}

