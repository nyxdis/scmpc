/**
 * mpd.c: MPD backend.
 *
 * ==================================================================
 * Copyright (c) 2008 Christoph Mende <angelos@unkreativ.org>
 * Based on Jonathan Coome's work on scmpc
 *
 * This file is part of scmpc.
 *
 * scmpc is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * scmpc is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with scmpc; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 * ==================================================================
 */


#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/un.h>
#include <netinet/in.h>

#include "misc.h"
#include "mpd.h"
#include "preferences.h"

static int server_connect_unix(const char *path)
{
	int sockfd;
	struct sockaddr_un addr;
	unsigned int len;

	sockfd = socket(AF_UNIX,SOCK_STREAM,0);
	if(sockfd < 0)
		return -1;

	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path,path,strlen(path) + 1);
	len = strlen(addr.sun_path) + sizeof(addr.sun_family);
	if(connect(sockfd,(struct sockaddr *)&addr,len) < 0)
		return -1;

	return sockfd;
}

static int server_connect_tcp(const char *host, int port)
{
	int sockfd;
	struct sockaddr_in addr;
	struct hostent *he;
	unsigned int len;

	sockfd = socket(AF_INET,SOCK_STREAM,0);
	if(sockfd < 0)
		return -1;

	he = gethostbyname(host);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	memcpy((char *)&addr.sin_addr.s_addr,(char *)he->h_addr,he->h_length);
	len = sizeof(addr);
	if(connect(sockfd,(struct sockaddr *)&addr,len) < 0)
		return -1;

	return sockfd;
}

void mpd_connect(void)
{
	char *tmp;

	mpd_info = calloc(sizeof(struct mpd_info),1);
	if(strncmp(prefs.mpd_hostname,"/",1) == 0)
		mpd_info->sockfd = server_connect_unix(prefs.mpd_hostname);
	else
		mpd_info->sockfd = server_connect_tcp(prefs.mpd_hostname,prefs.mpd_port);

	if(mpd_info->sockfd < 0) {
		scmpc_log(ERROR,"Failed to connect to MPD: %s",strerror(errno));
		exit(EXIT_FAILURE);
	}

	if(strlen(prefs.mpd_password) > 0) {
		asprintf(&tmp,"password %s\n",prefs.mpd_password);
		if(write(mpd_info->sockfd,tmp,strlen(tmp)) < 0)
			scmpc_log(ERROR,"Failed to write to MPD: %s",strerror(errno));
		free(tmp);
	}

	if(fcntl(mpd_info->sockfd,F_SETFL,fcntl(mpd_info->sockfd,F_GETFL,0) | O_NONBLOCK) < 0)
		exit(EXIT_FAILURE);
}

void mpd_parse(char *buf)
{
	char *saveptr, *line;

	line = strtok_r(buf,"\n",&saveptr);
	do {
		if(strncmp(line,"OK MPD",6) == 0)
			sscanf(line,"%*s %*s %d.%d.%d",&mpd_info->version[0],&mpd_info->version[1],&mpd_info->version[2]);
		printf("mpd said: %s\n",line);
	} while((line = strtok_r(NULL,"\n",&saveptr)) != NULL);
}

void mpd_cleanup(void)
{
	close(mpd_info->sockfd);
	free(mpd_info);
}
