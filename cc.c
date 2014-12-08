#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <stdarg.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <termios.h>

char PORT[30] = {};

/*
	Credits to cplusplus.com for getch() and hide(), used for showing asterisks
*/

int getch(){
	int ch;
	struct termios t_old, t_new;
	tcgetattr(STDIN_FILENO, &t_old);
	t_new = t_old;
	t_new.c_lflag &= ~(ICANON | ECHO);
	tcsetattr(STDIN_FILENO, TCSANOW, &t_new);
	ch = getchar();
	tcsetattr(STDIN_FILENO, TCSANOW, &t_old);
	return ch;
}	

char * hide(char * ret){
	const char BACKSPACE = 127;
	const char RETURN = 10;
	
	char * buff = ret;
	int len = 0;
	
	unsigned char ch = 0;
	while((ch=getch())!=RETURN){
		if(ch == BACKSPACE){
			if(len!=0){
				printf("\b \b");
				buff[len-1] = 0;
				len--;
			}
		} else {
			buff[len] = ch;
			len++;
			printf("*");
		}

	} printf("\n");
	buff[len] = 0;
	
	return buff;
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

char * getTime(char * ret, int n){
	time_t rawtime;
	struct tm * timeinfo;
	time(&rawtime);
	timeinfo = localtime(&rawtime);
	strftime(ret, n, "%I:%M %p", timeinfo);
	return ret;
}

void msend(int sockfd, char * data){
	if(send(sockfd, data, strlen(data), 0) == -1){
		perror("send");
	}
	//printf("Sending '%s'\n", data);
}	
int mrecv(int sockfd, char * data, int n){
	int numbytes;
	if((numbytes=recv(sockfd, data, n-1, 0)) == -1 ){
		perror("recv");
	}
	data[numbytes] = 0;
	return numbytes;
}
char * mgets(char * data, int n){
	fgets(data, n, stdin);
	data[strlen(data)-1] = 0;
	return data;
}
int mstrncmp(char * a, char * b){
	return strncmp(a,b,strlen(b));
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

void readData(char * data){
	char head[200];
	char tail[600];
	int offset = 0;
	offset += getNext(data+offset, head);
	if(!strcmp(head, "MSG")){
		char user[200];
		char timestamp[50];
		char msg[200];
		offset += getNext(offset+data, user);
		offset += getNext(offset+data, timestamp);
		offset += getNext(offset+data, msg);
		printf("[%s] %s: %s\n", timestamp, user, msg);
	} else if(!strcmp(head, "ONLINE")){
		printf("[ONLINE]\n");
		int len = strlen(data);
		char user[200];
		while(offset < len){
			offset += getNext(offset+data, user);
			printf("\t%s\n", user);
		}
	} else if(!strcmp(head, "JOINED")) {
		char user[200];
		char timestamp[50];
		offset += getNext(offset+data, user);
		offset += getNext(offset+data, timestamp);
		printf("[%s] %s has joined chat.\n", timestamp, user);
	} else if(!strcmp(head, "LEFT")) {
		char user[200];
		char timestamp[50];
		offset += getNext(offset+data, user);
		offset += getNext(offset+data, timestamp);
		printf("[%s] %s has left chat.\n", timestamp, user);
	} else if(!strcmp(head, "SHUTDOWN")){
		printf("Server is shutting down. Please log out.\n");
	} else if(!strcmp(head, "PMR")){
		char user[200];
		char timestamp[50];
		char msg[200];
		offset += getNext(offset+data, user);
		offset += getNext(offset+data, timestamp);
		offset += getNext(offset+data, msg);
		printf("[%s] [PM] %s: %s\n", timestamp, user, msg);
		
	}
}

volatile sig_atomic_t got_int = 0;

void sigint_handler(int sig){
	got_int = 1;
}

char * parsePM(char * raw, char * ret, char * from){
	int i;
	char timestamp[50];
	getTime(timestamp, sizeof(timestamp));
	char user[200] = {};
	char msg[200] = {};
	int index = 0;
	int len = strlen(raw);
	for(i = 4; i<len; i++){
		if(raw[i] == ' ') continue;
		else break;
	}
	for(i; i<len; i++){
		if(raw[i] == ' '){
			user[index] = 0;
			index = 0;
			i+=1;
			break;
		} else {
			user[index] = raw[i];
			index++;
		}
	}
	
	for(i; i<len; i++){
		if(raw[i] == ' ') continue;
		else break;
	}
	for(i; i<=len; i++){
		if(!raw[i]){
			msg[index] = 0;
			index = 0;
			break;
		} else {
			msg[index] = raw[i];
			index++;
		}
	}	
	return strjoin(ret, "[PM] [", user, "] [", from, "] [", timestamp, "] [", msg, "]\n", NULL); 
}



int main(int argc, char * argv[]){
	if(argc!=3){
		printf("Usage: ./client <ip address> <port>\n");
		exit(1);
	}
	strcpy(PORT, argv[2]);

	char buff[800];
	int sockfd, numbytes;
	struct addrinfo hints, *servinfo, *p;
	char server_ip[INET6_ADDRSTRLEN];
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	int rv;
	if ( (rv = getaddrinfo(argv[1], argv[2], &hints, &servinfo))){
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}
	
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ( (sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			perror("client: socket");
			continue;
		}
		
		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("client: connect");
			continue;
		}
		
		break;
	}	
	
	if (p == NULL) {
		fprintf(stderr, "client: failed to connect\n");
		return 2;
	}
	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), server_ip, sizeof server_ip);
	printf("client: connecting to %s\n", server_ip);
	freeaddrinfo(servinfo);
	
	char input[500];
	start:
	printf("(1) Login or (2) Create account: ");
	mgets(input, sizeof input);
	if(strcmp(input, "1") && strcmp(input, "2")){
		system("clear");
		printf("Enter \"1\" or \"2\" to continue\n");
		goto start;
	}
	
	
	if(atoi(input) == 1){ // Login
		
		login_start:
		printf("Enter username: ");
		char user[200];
		char pass[200];
		mgets(user, sizeof user);
		strjoin(buff, "[CHK_USR] [", user, "]\n", NULL);
		msend(sockfd, buff);
		mrecv(sockfd, buff, sizeof(buff));
		if(!strncmp(buff, "[USR_CHK_0]", 11)){
			system("clear");
			printf("User does not exist\n");
			goto login_start;
		} 
		printf("Enter password: ");
		//mgets(pass, sizeof pass);
		hide(pass);
		strjoin(buff, "[LOGIN] [", user, "] [", pass, "]\n", NULL);
		msend(sockfd, buff);
		//printf("Client sends '%s'\n", buff);
		mrecv(sockfd, buff, sizeof buff);
		//printf("Client received '%s'\n", buff);
		
		if(!mstrncmp(buff, "[LOGIN_CHK_1]")){
			printf("Entering chat...\n");
			
			struct sigaction sa;
			
			
			int pid;
			if(!fork()){ // Listener
				sa.sa_handler = sigint_handler;
				sa.sa_flags = SA_RESTART;
				sigemptyset(&sa.sa_mask);
				if (sigaction(SIGINT, &sa, NULL) == -1) {
					perror("sigaction");
					exit(1);
				}	
			
			
				while(mrecv(sockfd, buff, sizeof buff) > 0){
					if(got_int){	
						
						close(sockfd);
						exit(0);
					}
					readData(buff);	
				}
			} else {
				sa.sa_handler = sigint_handler;
				sa.sa_flags = 0;
				sigemptyset(&sa.sa_mask);
				if (sigaction(SIGINT, &sa, NULL) == -1) {
					perror("sigaction");
					exit(1);
				}	
			
				char timestamp[50];
				getTime(timestamp, sizeof timestamp);
				strjoin(buff, "[JOIN] [", user, "] [", timestamp, "]\n");
				msend(sockfd, buff);
				while(mgets(input, 142)!=NULL){
					if(got_int){
						getTime(timestamp, sizeof timestamp);
						strjoin(buff, "[LEAVE] [", user, "] [", timestamp, "]\n");
						msend(sockfd, buff);
						printf("\nExiting chat...\n");
						close(sockfd); 
						exit(0);
					}
				
				
					if(!strcasecmp(input, "/whosonline")){
						msend(sockfd, "[OL]\n");
					} else if(!strncasecmp(input, "/pm", 3) &&strlen(input) >= 7){
						parsePM(input, buff, user);
						//printf("Got pm\n%s", buff);
						msend(sockfd, buff);
					} else {
						getTime(timestamp, sizeof timestamp);
						strjoin(buff, "[MSG] [", user, "] [", timestamp, "] [", input, "]\n", NULL);
						msend(sockfd, buff);
					}
				}
			}
			
			
		} else {
			system("clear");
			printf("Wrong username/password combination\n");
			goto login_start;
		}
		
	} else { // Create acct
		create_account_start:
		printf("Enter username: ");
		mgets(input, sizeof input);
		if(!strlen(input)){
			system("clear");
			printf("Username can't be blank.\n");
			goto create_account_start;
		}
		int i;
		for(i = 0; input[i]; i++){
			if(!isalnum(input[i]) && input[i] != '_'){
				system("clear");
				printf("Username can only contain alphanumeric characters and underscores\n");
				goto create_account_start;
			}
		}
	
		strjoin(buff, "[CHK_USR] [", input, "]\n", NULL);
		msend(sockfd, buff);
		mrecv(sockfd, buff, sizeof(buff));
		if(!strncmp(buff, "[USR_CHK_1]", 11)){
			system("clear");
			printf("Username exists, please try another! :)\n");
			goto create_account_start;
		} 
		char user1[200];
		strcpy(user1, input);
		char pass1[200] = {};
		char pass2[200];
		password_start:
		printf("Enter password: ");
		//mgets(pass1, 200);
		hide(pass1);
		if(!strlen(pass1)){
			system("clear");
			printf("Password can't be blank.\n");
			goto password_start;
		}
		printf("Verify password: ");
		//mgets(pass2, 200);
		hide(pass2);
		if(strcmp(pass1, pass2)){
			system("clear");
			printf("Passwords don't match.\n");
			goto password_start;
		}
		strjoin(buff, "[CREATE_ACCT] [", user1, "] [", pass1,"]\n", NULL);
		msend(sockfd, buff);
		mrecv(sockfd, buff, sizeof(buff));
		if(!strncmp(buff, "[ACCT_CREATED]", 14)){
			printf("Account succesfully created. Press [ENTER] to continue...\n");
			mgets(buff, sizeof buff);
			goto start;
		}
		
		
	}
	
	/*if ((numbytes = recv(sockfd, buff, sizeof(buff)-1, 0)) == -1){
		perror("recv");
		exit(1);
	}
	buff[numbytes] = 0;
	
	printf("client: received '%s'\n", buff);*/
	close(sockfd);
	return 0;
}
