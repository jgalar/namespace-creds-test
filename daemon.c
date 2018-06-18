#define _GNU_SOURCE
#include "common.h"
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <errno.h>

static
int create_unix_sock(const char *path)
{
	struct sockaddr_un s_un = { 0 };
	int fd = -1;
	int ret = -1;

	if ((fd = socket(PF_UNIX, SOCK_STREAM, 0)) < 0) {
		perror("socket");
		goto error;
	}

	s_un.sun_family = AF_UNIX;
	strncpy(s_un.sun_path, path, sizeof(s_un.sun_path));
	s_un.sun_path[sizeof(s_un.sun_path) - 1] = '\0';

	(void) unlink(path);
	ret = bind(fd, (struct sockaddr *) &s_un, sizeof(s_un));
	if (ret < 0) {
		perror("bind");
		goto error;
	}

	return fd;

error:
	if (fd >= 0) {
		if (close(fd) < 0) {
			perror("close");
		}
	}
return ret;
}

static
int recv_creds(int unix_sock_fd)
{
	struct msghdr msg = { 0 };
	struct iovec iov;
	ssize_t ret = -1;
	struct cmsghdr *cmptr;
	struct ucred *ucp;
	int buf;
	union {
		struct cmsghdr cmh;
		char control[CMSG_SPACE(sizeof(struct ucred))];
	} control_un;
	char name[255];

	msg.msg_control = control_un.control;
	msg.msg_controllen = sizeof(control_un.control);

	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	iov.iov_base = &buf;
	iov.iov_len = sizeof(buf);

	msg.msg_name = name;
	msg.msg_namelen = sizeof(name);

	/* Receive real plus ancillary data */
	ret = recvmsg(unix_sock_fd, &msg, 0);
	if (ret < 0) {
		perror("recvmsg");
		goto end;
	}

	cmptr = CMSG_FIRSTHDR(&msg);
	if (cmptr == NULL || cmptr->cmsg_len != CMSG_LEN(sizeof(struct ucred))) {
		puts("bad cmsg header / message length");
		ret = -1;
		goto end;
	}
	if (cmptr->cmsg_level != SOL_SOCKET) {
		puts("cmsg_level != SOL_SOCKET");
		ret = -1;
		goto end;
	}
	if (cmptr->cmsg_type != SCM_CREDENTIALS) {
		puts("cmsg_type != SCM_CREDENTIALS");
		ret = -1;
		goto end;
	}

	ucp = (struct ucred *) CMSG_DATA(cmptr);
	printf("Received credentials pid = %d, uid = %d, gid = %d\n",
			ucp->pid, ucp->uid, ucp->gid);
	ret = 0;
end:
	return ret;
}

int main(int argc, char **argv)
{
	int ret, sock_fd = -1, conn_fd = -1, on = 1;

	ret = create_unix_sock(SOCK_PATH);
	if (ret < 0) {
		goto end;
	}
	sock_fd = ret;

	puts("listening on UNIX socket...");
	ret = listen(sock_fd, SOMAXCONN);
	if (ret) {
		perror("listen");
	}

	puts("accepting...");
	ret = accept(sock_fd, NULL, NULL);
	if (ret < 0) {
		perror("accept");
		goto end;
	}
	conn_fd = ret;

	ret = setsockopt(conn_fd, SOL_SOCKET, SO_PASSCRED, &on, sizeof(on));
	if (ret < 0) {
		perror("setsockopt");
	}

	ret = recv_creds(conn_fd);
	if (ret < 0) {
		goto end;
	}
end:
	if (sock_fd >= 0) {
		(void) close(sock_fd);
	}
	if (conn_fd >= 0) {
		(void) close(conn_fd);
	}
	return ret;
}
