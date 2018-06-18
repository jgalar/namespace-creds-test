#define _GNU_SOURCE
#include "common.h"
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <errno.h>

static
int connect_unix_sock(const char *pathname)
{
	struct sockaddr_un s_un;
	int fd, ret, closeret;

	fd = socket(PF_UNIX, SOCK_STREAM, 0);
	if (fd < 0) {
		perror("socket");
		ret = -1;
		goto error;
	}

	memset(&s_un, 0, sizeof(s_un));
	s_un.sun_family = AF_UNIX;
	strncpy(s_un.sun_path, pathname, sizeof(s_un.sun_path));
	s_un.sun_path[sizeof(s_un.sun_path) - 1] = '\0';

	ret = connect(fd, (struct sockaddr *) &s_un, sizeof(s_un));
	if (ret < 0) {
		perror("connect");
		goto error_connect;
	}

	return fd;

error_connect:
	closeret = close(fd);
	if (closeret) {
		perror("close");
	}
error:
	return ret;
}

static
int send_creds(int unix_sock_fd)
{
	struct msghdr msg = { 0 };
	struct iovec iov;
	ssize_t ret = -1;
	struct cmsghdr *cmptr;
	struct ucred *ucp;
	char buf[] = "A";
	union {
		struct cmsghdr cmh;
		char control[CMSG_SPACE(sizeof(struct ucred))];
	} control_un;

	iov.iov_base = buf;
	iov.iov_len = sizeof(buf);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	msg.msg_control = control_un.control;
	msg.msg_controllen = sizeof(control_un.control);

	cmptr = CMSG_FIRSTHDR(&msg);
	cmptr->cmsg_len = CMSG_LEN(sizeof(struct ucred));
	cmptr->cmsg_level = SOL_SOCKET;
	cmptr->cmsg_type = SCM_CREDENTIALS;
	ucp = (struct ucred *) CMSG_DATA(CMSG_FIRSTHDR(&msg));

	ucp->pid = getpid();
	ucp->uid = getuid();
	ucp->gid = getgid();
	printf("Client sending credentials pid = %d, uid = %d, gid = %d\n",
			ucp->pid, ucp->uid, ucp->gid);

	do {
		ret = sendmsg(unix_sock_fd, &msg, 0);
	} while (ret < 0 && errno == EINTR);
	if (ret < 0) {
		perror("sendmsg");
	}
	ret = 0;
	return ret;
}

int main(int argc, char **argv)
{
	int ret;
	int sock_fd = -1;

	ret = connect_unix_sock(SOCK_PATH);
	if (ret < 0) {
		goto end;
	}
	sock_fd = ret;

	ret = send_creds(sock_fd);
	if (ret < 0) {
		goto end;
	}
	/* Wait for the daemon to consume the message before exiting. */
	sleep(100);
end:
	if (sock_fd >= 0) {
		(void) close(sock_fd);
	}
	return ret;
}
