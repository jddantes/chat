#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdarg.h>

#define PORT "5555"
#define DB "db.txt"
#define max(a,b) (a)>(b) ? (a) : (b)

#define BROADCAST_SELF 0
#define BROADCAST_ONE 1
#define BROADCAST_OTHERS 2
#define BROADCAST_ALL 3

typedef int bool;

int listener;
fd_set master, read_fds;
int fdmax = 0;
char ip_list[200][INET6_ADDRSTRLEN] = {};
char user_list[200][50] = {};
bool ol_list[200] = {};

void msend(int sockfd, const void * buf, size_t len, int flags){
	if(send(sockfd, buf, len, flags) == -1){
		perror("send");
	}
}

char * strjoin(char * dest, ...){
	va_list vl;
	va_start(vl, dest);
	char * str;
	int i;
	for(i = 0; ; i++){
	str = va_arg(vl, char *);
	if(str == NULL){
	break;
	}
	i ? strcat(dest, str) : strcpy(dest, str);
	}
	va_end(vl);
	return dest;
}
char * strapp(char * dest, ...){
	va_list vl;
	va_start(vl, dest);
	char * str;
	int i;
	for(i = 0; ; i++){
	str = va_arg(vl, char *);
	if( str == NULL ){
	break;
	}
	strcat(dest, str);
	}
	va_end(vl);
	return dest;
}

void *get_in_addr(struct sockaddr *sa){
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}
	
	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}	

void broadcastAll(char * data){
	int len = strlen(data);
	int j;
	for(j = 0; j<=fdmax; j++){
		if (FD_ISSET(j, &master) && j!=listener && ol_list[j]) {
			msend(j, data, len, 0);
		}
	}
}

void broadcastOthers(char * data, int me){
	int len = strlen(data);
	int j;
	for(j = 0; j<=fdmax; j++){
		if(FD_ISSET(j, &master) && j!=me && j!=listener && ol_list[j]){
			msend(j, data, len, 0);
		}
	}
}

void broadcastOne(char * data, char * user){
	int len = strlen(data);
	int j;
	for(j = 0; j<=fdmax; j++){
		if(FD_ISSET(j, &master) && j!=listener && ol_list[j]){
			if(user_list[j][0] && !strcmp(user, user_list[j])){
				msend(j, data, len, 0);
			}
		}
	}
}

void pollOnline(char * ret){
	int j;
	for(j = 0; j<=fdmax; j++){
		if (FD_ISSET(j, &master) && j!=listener && ol_list[j]) {
			//printf("Polled %d %s\n", j, user_list[j]);
			strapp(ret, " [", user_list[j], "]", NULL);
		}
	}
	strcat(ret, "\n");
}

int getNext(char * data, char * ret){
	/*
		For data in the form of: [a] [b] [c]\n
		copies the contents of the block into ret
		and returns the index/offset of the next block (2 + index of current ']')=
	*/
	
	int i = 1;
	char buff[200];
	int index = 0;
	for(i; data[i]; i++){
		if(data[i] == ']'){
			buff[index] = 0;
			index = 0;
			break;
		} else {
			buff[index] = data[i];
			index++;
		}
	}
	strcpy(ret, buff);
	return i+2;
}

int verifyLogin(char * user, char *pass){
	FILE *fp = fopen(DB, "r");
	if(fp == NULL){
		perror("opening db");
		exit(5);
	}
	char u[200];
	char p[200];
	while(fgets(u, 200, fp)!= NULL){
		fgets(p, 200, fp);
		u[strlen(u)-1] = 0;
		p[strlen(p)-1] = 0;
		if(!strcmp(u, user) && !strcmp(p, pass)){
			return 1;
		}
	}
	fclose(fp);
	return 0;
}

int userCheck(char * user){
	char buff[200];
	FILE *fp = fopen(DB, "r");
	if(fp == NULL){
		perror("opening db");
		exit(5);
	}
	char u[200];
	char p[200];
	while(fgets(u, 200, fp)!= NULL){
		fgets(p, 200, fp);
		u[strlen(u)-1] = 0;
		p[strlen(p)-1] = 0;
		if(!strcmp(u, user)){
			return 1;
		}
	}
	fclose(fp);

	return 0;
}

void createAcct(char * user, char *pass){
	FILE * fp = fopen(DB, "a");
	if(fp == NULL){
		perror("opening db");
		exit(5);
	}
	fprintf(fp,"%s\n%s\n", user, pass);
	fclose(fp);
}

int currfd; // For setting user upon [JOINED] 
int handle_data(char * data, char * ret){
	/*
		Data structured as: [head] tail
		Returns 1 to self-broadcast
			2 to broadcast to all
			
		
	*/
	char head[200];
	char tail[600];
	int offset = 0;
	offset += getNext(data+offset, head);
	if(!strcmp(head, "LOGIN")){
		char user[200];
		char pass[200];
		offset += getNext(data+offset, user);
		offset += getNext(data+offset, pass);
		int login = verifyLogin(user, pass);
		if(login) {
			strcpy(ret, "[LOGIN_CHK_1] [You have logged in]\n");	
			printf("server: %s logged in\n", user);
		} else {
			strcpy(ret, "[LOGIN_CHK_0] [Incorrect username/password]\n");
		}
		return BROADCAST_SELF;
	} else if(!strcmp(head, "CREATE_ACCT")) {
		char user[200];
		char pass[200];
		offset += getNext(data+offset, user);
		offset += getNext(data+offset, pass);
		createAcct(user, pass);
		strcpy(ret, "[ACCT_CREATED] [Account succesfully created]\n");
		return BROADCAST_SELF;
	} else if(!strcmp(head, "CHK_USR")){
		char user[200];
		offset += getNext(data+offset, user);
		int user_exst = userCheck(user);
		if(user_exst){
			strcpy(ret, "[USR_CHK_1] [User exists]\n");
		} else {
			strcpy(ret, "[USR_CHK_0] [Username is valid]\n");
		}	
		return BROADCAST_SELF;
	} else if(!strcmp(head, "JOIN")){
		char user[200];
		char ts[50];
		offset += getNext(data+offset, user);
		offset += getNext(data+offset, ts);
		strjoin(ret, "[JOINED] [", user, "] [", ts, "]\n", NULL);
		strcpy(user_list[currfd], user);
		ol_list[currfd] = 1;
		//printf("Setting user %d to %s\n", currfd, user);
		return BROADCAST_OTHERS;
	} else if(!strcmp(head, "LEAVE")){
		char user[200];
		char ts[50];
		offset += getNext(data+offset, user);
		offset += getNext(data+offset, ts);
		strjoin(ret, "[LEFT] [", user, "] [", ts, "]\n", NULL);
		user_list[currfd][0] = 0;
		ol_list[currfd] = 0;
		return BROADCAST_OTHERS;
	
	 } else if(!strcmp(head, "MSG")){
		char user[200];
		char ts[50];
		char msg[200];
		offset += getNext(data+offset, user);
		offset += getNext(data+offset, ts);
		offset += getNext(data+offset, msg);
		strjoin(ret, "[MSG] [", user, "] [", ts, "] [", msg, "]\n", NULL);
		return BROADCAST_ALL;
	} else if(!strcmp(head, "OL")){
		strcpy(ret, "[ONLINE]");
		pollOnline(ret);
		return BROADCAST_SELF;
	}
	
}

int main(){
	
	int newfd;
	struct sockaddr_storage remoteaddr;
	socklen_t addrlen;
	
	char buf[800];
	int nbytes;
	
	char remoteIP[INET6_ADDRSTRLEN];
	
	int yes = 1;
	int i, j;
	
	struct addrinfo hints, *ai, *ai_itr;
	
	FD_ZERO(&master);
	FD_ZERO(&read_fds);
	
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	
	int rv;
	if (rv = getaddrinfo(NULL, PORT, &hints, &ai)){
		fprintf(stderr, "server: %s\n", gai_strerror(rv));
		exit(1);
	}
	
	for(ai_itr = ai; ai_itr!=NULL; ai_itr = ai_itr->ai_next) {
		listener = socket(ai_itr->ai_family, ai_itr->ai_socktype, ai_itr->ai_protocol);
		if(listener < 0) {
			continue;		
		}
		setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
		
		if (bind(listener, ai_itr->ai_addr, ai_itr->ai_addrlen) <0 ){
			close(listener);
			continue;
		}
		break;
	}
	
	if (ai_itr == NULL) {
		fprintf(stderr, "server: failed to bind\n");
		exit(2);
	}
	
	freeaddrinfo(ai);
	
	if (listen(listener, 10) == -1) {
		perror("listen");
		exit(3);
	}
	
	FD_SET(listener, &master);
	fdmax = max(fdmax, listener);
	
	while(1){
		read_fds = master;
		if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
			perror("select");
			exit(4);
		}
		
		for(i = 0; i<=fdmax; i++) {
			if (FD_ISSET(i, &read_fds)) {
				if (i == listener) { // new connection
					addrlen = sizeof remoteaddr;
					newfd = accept(listener, (struct sockaddr *)&remoteaddr, &addrlen);
					if(newfd == -1) {
						perror("accept");
					} else {
						FD_SET(newfd, &master);
						fdmax = max(fdmax, newfd);
						printf("server: %s connected on socket %d\n", inet_ntop(remoteaddr.ss_family, get_in_addr((struct sockaddr *)&remoteaddr), remoteIP, INET6_ADDRSTRLEN), newfd);
						strcpy(ip_list[newfd], remoteIP);
						user_list[newfd][0] = 0;
						ol_list[newfd] = 0;
						/*char tempusr[] = "user0";
						tempusr[4] += newfd;
						strcpy(user_list[newfd], tempusr);*/
					}
					
					
				} else { // handle data from client
					if ( (nbytes = recv(i, buf, sizeof buf, 0)) <= 0  ) {
						if (nbytes == 0 ){
							// connection closed
							printf("server: %s on socket %d hung up\n", ip_list[i], i);
							
							// broadcast here
							
						} else {
							perror("recv");
						}
						close(i);
						ip_list[i][0] = 0;
						user_list[i][0] = 0;
						FD_CLR(i, &master);
					} else {
						// broadcast here
						char ret[800];
						currfd = i;
						int bm = handle_data(buf, ret);
						if(bm == BROADCAST_SELF){
							msend(i, ret, strlen(ret), 0);
						} else if(bm == BROADCAST_OTHERS){
							
							broadcastOthers(ret, i);	
						} else if(bm == BROADCAST_ALL){
							broadcastAll(ret);
						} else {
							
						}
					}
				} // END handle data from client
			} 
		} // END for loop iteration through file descriptors
	} // END while loop
	return 0;
}

