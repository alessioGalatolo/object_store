/*
	Progetto SOL AA 2018/2019 
	Alessio Galatolo
*/

//An interface to communicate with the server

//added for handling signals
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

//my includes
#include <objstr.h>
#include <utils.h>

#define SYS_CALL(x, s)\
	if((x) < 0){last_error = s; return 0;}
#define SOCK_IO(x, s,  cleanup)\
	if((x) < 0){last_error = s; cleanup; return 0;}
#define NULL_CHECK(x, s)\
	if((x) == NULL){last_error = s; return 0;}

#define FREE(x)\
	if((x) != NULL){free((x)); x = NULL;}

//used to check write-readline funs 
#define IO_CHECK(x)\
	if((x) == 0){goto error;}

//Checks if the connection has already been made
#define FD_INIT_CHECK\
	if(fd == -1){errno = EADDRNOTAVAIL; return 0;}

//checks if the string passed as parameter is null
#define STRING_NULL_CHECK(s)\
	if(s == NULL){errno = EINVAL; return 0;}

#define RET_SUCCESS\
	last_error = NULL;\
	return 1;

//reads response from server, if = KO generates error
#define READ_RESPONSE(m)\
	m = readline(NULL);\
	if(strncmp(m, KO, KO_LEN) == 0){FREE(last_server_error); last_server_error = m; goto error;}\
	else{FREE(last_server_error); last_server_error = m;}


static int fd = -1;
static char* last_error = NULL;
static char* last_server_error = NULL;

//to be used in combination with errno, returns last error occurred inside the library
char* get_lasterror(){
	if(last_error == NULL)
		return "Last call were successful";
	return last_error;
}

//returns last error received inside KO message from server
char* get_lastservererror(){
	if(last_server_error == NULL)
		return "no error registered";
	return last_server_error;
}


int writeobj(void* _obj, size_t len){
	char* obj = _obj;	
	size_t written = 0;

	struct sigaction newsa;
	struct sigaction oldsa;
	SYS_CALL(sigaction(SIGPIPE, NULL, &oldsa), "sigaction, getting old sa");
	newsa = oldsa;
	newsa.sa_handler = SIG_IGN;
	SYS_CALL(sigaction(SIGPIPE, &newsa, NULL), "sigaction, setting new sa");

	while(written < len){
		ssize_t write_n = 0;
		SOCK_IO(write_n = write(fd, obj + written, sizeof(char) * (len - written)), "Write obj to socket", SYS_CALL(sigaction(SIGPIPE, &oldsa, NULL), "sigaction, setting old sa"));
		written += write_n;
	}

	SYS_CALL(sigaction(SIGPIPE, &oldsa, NULL), "sigaction, setting old sa");

	RET_SUCCESS;
}

int newlineindex(char* s, size_t len){
	int i = 0;
	while(i < len && s[i] != '\n')
		i++;
	return i == len ? -1: i;
}

/*
	reads from file fd, char by char until new line
	if the server sends binary code it is put in obj
	returns the message read, NULL in case of error
*/
char* readline(void** obj){
	char* buf = NULL;
	size_t cur_len = BASE_LENGTH;
	NULL_CHECK(buf = malloc(sizeof(char) * cur_len), "buf malloc");
	size_t readen = 0;
	ssize_t read_n = 0;

	struct sigaction newsa;
	struct sigaction oldsa;
	SYS_CALL(sigaction(SIGPIPE, NULL, &oldsa), "sigaction, getting old sa");
	newsa = oldsa;
	newsa.sa_handler = SIG_IGN;
	SYS_CALL(sigaction(SIGPIPE, &newsa, NULL), "sigaction, setting new sa");

	do{
		if(readen >= cur_len - cur_len/5){
			NULL_CHECK(buf = realloc(buf, sizeof(char) * (cur_len + BASE_LENGTH)), "realloc buffer for bigger length");
			cur_len += BASE_LENGTH;
		}
		SOCK_IO(read_n = read(fd, buf + readen, sizeof(char) * (cur_len - readen)), "readline, read string", SYS_CALL(sigaction(SIGPIPE, &oldsa, NULL), "sigaction, setting old sa"));
		readen += read_n;
	}while(read_n != 0 && newlineindex(buf, readen) == -1);

	int newlineindex1 = newlineindex(buf, readen);
	buf[newlineindex1] = '\0';

	if(strncmp(buf, DAT, DAT_LEN) == 0){//need to read object
		size_t length = strtol(buf + DAT_LEN, NULL, 10);
		if(obj == NULL)
			return NULL;
		size_t obj_readen = 0;
		NULL_CHECK(*obj = malloc(length), "object malloc");

		//copy what's exceeding on buf to obj
		for(int i = newlineindex1 + 1; i < readen; i++){
			((char*) *obj)[obj_readen] = buf[i];
			obj_readen++;
		}

		while(obj_readen < length){
			SOCK_IO(read_n = read(fd, ((char*) (*obj)) + obj_readen, sizeof(char) * (length - obj_readen)), "Read obj from socket", sigaction(SIGPIPE, &oldsa, NULL));
			obj_readen += read_n;
		}
	}
	last_error = NULL;
	SYS_CALL(sigaction(SIGPIPE, &oldsa, NULL), "sigaction, setting old sa");
	return buf;
}


int os_connect(char* name){
	STRING_NULL_CHECK(name);

	//connecting
	struct sockaddr_un myaddr;
	NULL_CHECK(memset(&myaddr, '0', sizeof(struct sockaddr_un)), "memset of sockaddr");
	myaddr.sun_family = AF_UNIX;
	strncpy(myaddr.sun_path, SOCKET_PATH, UNIX_PATH_MAX);
	int fd_skt = socket(AF_UNIX, SOCK_STREAM,0);
	SYS_CALL(fd_skt, "Socket create");

	while(connect(fd_skt, (struct sockaddr*) &myaddr, sizeof(myaddr)) < 0){ //waiting server to be up
		if(errno != EINTR && errno != ECONNREFUSED /*&& errno != ENOENT*/){
			last_server_error = "error connecting";
			return 0;
		}
	}
	fd = fd_skt;

	//send register message
	size_t slength = sizeof(char) * (REG_LEN + strlen(name) + 2);
	char* message = malloc(slength);
	NULL_CHECK(message, "malloc for answer inside os_connect");
	strcpy(message, REG);
	sprintf(message + REG_LEN, "%s\n", name);
	IO_CHECK(writeobj(message, slength - sizeof(char)));
	FREE(message);

	READ_RESPONSE(message);

	RET_SUCCESS;

	error:
		FREE(message);
		return 0;
}

int os_store(char* name, void* block, size_t len){
	FD_INIT_CHECK;
	STRING_NULL_CHECK(name);

	char* message = malloc(sizeof(char) * (STO_LEN + strlen(name) + ((long) log2((double) len)) + 3));
	NULL_CHECK(message, "malloc of message: os store");
	strcpy(message, STO);
	sprintf(message + STO_LEN, "%s %ld\n", name, len);
	IO_CHECK(writeobj(message, strlen(message)));
	IO_CHECK(writeobj(block, len));
	FREE(message);

	READ_RESPONSE(message);

	RET_SUCCESS;

	error:
		FREE(message);
		return 0;
}

void* os_retrive(char* name){
	FD_INIT_CHECK;
	STRING_NULL_CHECK(name);
	
	int len = RET_LEN + strlen(name);
	char* message = malloc(sizeof(char) * (len + 2));
	NULL_CHECK(message, "malloc request inside os_retrive");
	strcpy(message, RET);
	strcat(message, name);
	message[len] = '\n';
	message[len + 1] = '\0';
	IO_CHECK(writeobj(message, len + sizeof(char)));
	FREE(message);
	
	void* obj = NULL;
	FREE(last_server_error);
	IO_CHECK(last_server_error = readline(&obj));
	last_error = NULL;
	return obj;

	return NULL;

	error:
		FREE(message);
		FREE(obj);
		return NULL;

}

int os_delete(char* name){
	FD_INIT_CHECK;
	STRING_NULL_CHECK(name);

	size_t slength = sizeof(char) * (DEL_LEN + strlen(name) + 2);
	char* message = malloc(slength);
	NULL_CHECK(message, "malloc for request inside os_delete");
	strcpy(message, DEL);
	sprintf(message + DEL_LEN, "%s\n", name);
	IO_CHECK(writeobj(message, slength - sizeof(char)));
	FREE(message);
	
	READ_RESPONSE(message);

	RET_SUCCESS;
	
	error:
		FREE(message);
		return 0;
}

int os_disconnect(){
	FD_INIT_CHECK;

	size_t slength = sizeof(char) * (LEA_LEN + 2);
	char* message = malloc(slength);
	NULL_CHECK(message, "malloc request insied os_disconnect");
	strcpy(message, LEA);
	sprintf(message + LEA_LEN, "\n");
	IO_CHECK(writeobj(message, slength - sizeof(char)));
	FREE(message);
	IO_CHECK(message = readline(NULL));
	SYS_CALL(close(fd), "closing socket");
	fd = -1;
	FREE(message);
	FREE(last_server_error);
	return 1;
	
	error:
		FREE(message);
		close(fd);
		return 0;
}
