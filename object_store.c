/*
	Progetto SOL AA 2018/2019
	Alessio Galatolo
*/

//server

//added for handling signals
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <math.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>

//my includes
#include <utils.h>
#include <threadpool.h>

//a macro to avoid freeing 2 times the same variable
#define FREE(x)\
	if((x) != NULL){free((x)); x = NULL;}

//a macro to avoid closing 2 times the same fd
#define CLOSE(x)\
	if(x != -1){\
		if(close(x) == -1){perror("close"); return 0;}\
		x = -1;}

//macro to test return values of readline/writeobj
#define IO_CHECK(x, m)\
	if(x == 0){\
		FREE(m);\
		m = malloc(sizeof(char) * (KO_LEN + 1 + strlen(ERR_WRITE)));\
		if(m == NULL)\
			m = MALLOC_KO;\
		else{\
			strcpy(m, KO);\
			strcat(m, ERR_WRITE);\
		}goto error;}

/*
	a macro to be used inside threads/function where it is important
	not to terminate the whole process in case of error
*/
#define SYS_CALL_NONFATAL(x, s, m)\
	if((x) < 0){\
		m = malloc(sizeof(char) * (strlen(s) + 1 + KO_LEN));\
		if(m == NULL)\
			m = MALLOC_KO;\
		else{\
		strcpy(m, KO);\
		strcat(m, s);\
		}goto error;}


//tries to malloc for MAX_ATTEMPTS, if unsuccessful writes KO to client and returns
#define MALLOC_NONFATAL(x, size, fd)\
	{int ind = 0;\
	x = NULL;\
	while((x) == NULL && ind++ < MAX_ATTEMPTS)\
		x = malloc(size);\
	if((x) == NULL){\
		writeobj(fd, MALLOC_KO, MALLOC_KO_LEN);\
		return 0;\
	}}

//Same as MALLOC_NONFATAL except uses realloc
#define REALLOC_NONFATAL(x, size, fd)\
	{int ind = 0;\
	do x = realloc((x), size);\
	while(x == NULL && ind++ < MAX_ATTEMPTS);\
	if(x == NULL){writeobj(fd, MALLOC_KO, MALLOC_KO_LEN); return 0;}}

//checks the calls return value, in case of error shuts down everything
#define SYS_CALL(x, s)\
	if((x) < 0){perror(s); exit(errno);}
#define NULL_CHECK(x, s)\
	if((x) == NULL){perror(s); exit(errno);}
#define THREAD_CHECK(x, s)\
	if((x) != 0){perror(s); exit(errno);}

//fun declaration
void* request_manager_thread(void*);
void* signal_handler_thread(void*);
int elaborate_message(int, char*, void**, size_t*, char*);
char* getname(char*, int);
int request_type(char*);
char* readline(int, void**, size_t*);
int writeobj(int, void*, size_t);

//utility functions
off_t getfilesize(char*); //get file size pointed by fd
ssize_t newlineindex(char*, size_t); //gets the index of '\n' in the string
void init_objsize(char*); //initializes the store size with the already existing objects
void rem_objsize(size_t s); //increases objects_size var using a mutex
void add_objsize(size_t s); //decreases objects_size var using a mutex
void add_conn(); //increases connection number using a mutex
void rem_conn(); //decreases connection number using a mutex
int socket_check(int); //uses a select to check for reads on fd, return 1 -> free to read; 0 -> process is being terminated


//global variables
int terminate = 0; //does not create race conditions(is set only by a thread)
static size_t objects_n = 0; //number of objects
static size_t objects_size = 0; //total size of the store
static size_t conn_n = 0; //number of connections
pthread_mutex_t mutex_objects = PTHREAD_MUTEX_INITIALIZER; //mutex to access objects_n, objects_size
pthread_mutex_t mutex_conn = PTHREAD_MUTEX_INITIALIZER; //mute to access conn_n


int main(){
	//setting signal mask
	sigset_t set;
	threadpool_t threadpool;
	char sock_path[UNIX_PATH_MAX + strlen(SOCKET_PATH)]; //used to store the path of the socket, the added len will be needed later to add "/objstore.sock" for unlinking

	SYS_CALL(sigfillset(&set), "sigfillset");
	THREAD_CHECK(pthread_sigmask(SIG_SETMASK, &set, NULL), "Sigmask set mask");

	//setting threadpool and lunching thread for signal managment
	THREAD_CHECK(tp_init(&threadpool), "Threadpool init");
	THREAD_CHECK(tp_spawn(&threadpool, signal_handler_thread, NULL), "Threadpool, spawning signal manager thread");

	//socket creation
	struct sockaddr_un sockaddr;
	NULL_CHECK(memset(&sockaddr, '0', sizeof(struct sockaddr_un)), "memset for sockaddr");
	sockaddr.sun_family = AF_UNIX;
	strncpy(sockaddr.sun_path, SOCKET_PATH, strlen(SOCKET_PATH) + 1);
	int fd_skt;
	SYS_CALL(fd_skt = socket(AF_UNIX, SOCK_STREAM,0), "Socket create");
	if(bind(fd_skt, (struct sockaddr*) &sockaddr, sizeof(sockaddr)) < 0){
		if(errno == EADDRINUSE){
			SYS_CALL(unlink(SOCKET_PATH), "Socket file already existing, error while unlink");
			SYS_CALL(bind(fd_skt, (struct sockaddr*) &sockaddr, sizeof(sockaddr)), "Socket bind");
		}else{
			perror("Socket bind");
			exit(1);
		}
	}
	SYS_CALL(listen(fd_skt, SOMAXCONN), "Socket listen");

	init_objsize(DATA_DIR);

	//getting cwd to unlink socket file later on
	NULL_CHECK(getcwd(sock_path, sizeof(char) * (UNIX_PATH_MAX + strlen(SOCKET_PATH))), "getcwd");

   	strcat(sock_path, SOCKET_PATH + 1); //getting SOCKET_PATH without '.'

	//creating data directory
	if (access(DATA_DIR, F_OK) == -1)//not found
    		SYS_CALL(mkdir(DATA_DIR, 0700), "creating data folder");
	SYS_CALL(chdir(DATA_DIR), "changing cwd to data");

	//accepting new connections
	while(!terminate){
		if(socket_check(fd_skt)){
			//someone requested connection
			int *fd = NULL;
			NULL_CHECK(fd = malloc(sizeof(int)), "Malloc unsuccessful"); //add error check
			SYS_CALL(*fd = accept(fd_skt, NULL, 0), "socket accept");

			THREAD_CHECK(tp_spawn(&threadpool, request_manager_thread, (void*) fd), "Threadpool, spawning worker thread");
		}
	}

	//clean up
	THREAD_CHECK(tp_terminate(&threadpool), "Threadpool, join of all threads");
	CLOSE(fd_skt)
	SYS_CALL(unlink(sock_path), "unlinking socket file");
	return 0;
}

/*
	a thread used for signal handling
	waits for any signal, upon reciving one,
	if signal == SIGUSR1 prints some stats
	otherwise sets 'terminate' to make the server shut down
*/
void* signal_handler_thread(void* args){
	//add sys call check
	sigset_t set;
	SYS_CALL(sigfillset(&set), "sigfillset");
	while(!terminate){
		int signal = 0;
		THREAD_CHECK(sigwait(&set, &signal), "sigwait");
		if(signal == SIGUSR1){
			printf("\nNumber of connections: %ld\nNumber of objects: %ld\nSize of store: %ld bytes\n",
			 	conn_n, objects_n, objects_size);
		}else{
			terminate = 1;
			return (void*) 0;
		}
	}
	return (void*) 0;
}

/*
	thread manager: registers the client and then enters a loop
	listening for request where it delegates the execution to 'elaborate_message'

	returns 0 -> success; 1 -> otherwise
*/
void *request_manager_thread(void *arg){
	int fd = *((int*) arg);
	FREE(arg);
	add_conn(); //add a connection to count
	void *obj = NULL;
	size_t len;
	char* adress = NULL;
	char* feedback_message = NULL;
	char* name = NULL;

	//prepare to receive connection request:
	char* message = readline(fd, &obj, &len); //should be register request
	if(message == NULL){
		CLOSE(fd);
		return (void*) 1;//cannot go on
	}

	if(strncmp(message, REG, REG_LEN) != 0){ //it is not a connection request
		MALLOC_NONFATAL(feedback_message, sizeof(char) * (KO_LEN + strlen(ERR_NOT_CONNECTED) + 1), fd);
		strcpy(feedback_message, KO);
		strcat(feedback_message, ERR_NOT_CONNECTED);
		goto error;
	}

	name = getname(message, fd);
	FREE(message);
	size_t length = sizeof(char) * (strlen(OK) + 1);
	MALLOC_NONFATAL(message, length, fd);
	strcpy(message, OK);
	IO_CHECK(writeobj(fd, message, length - sizeof(char)), feedback_message);
	FREE(feedback_message);
	FREE(message);

	//creating client directory
	length = strlen(name) + 4;
	MALLOC_NONFATAL(adress, sizeof(char) * length, fd);
	adress[0] = '.';
	adress[1] = '/';
	strcpy(adress + 2, name);
	if (access(adress, F_OK) == -1)
    	SYS_CALL(mkdir(adress, 0700), "creating name folder");
	//changing adress to be reused
	adress[length - 2] = '/';
	adress[length - 1] = '\0';

	//conncetion completed

	int go_on = 1; //1 -> go on; 0 -> client requested disconnection
	while(go_on && !terminate){
		message = readline(fd, &obj, &len); //reads message and obj if found
		if(message == NULL)
			goto error;
		go_on = elaborate_message(fd, message, &obj, &len, adress);
	}

	rem_conn(); //remove connection
	FREE(name);
	FREE(adress);
	FREE(feedback_message);
	CLOSE(fd)
	return (void*) 0;

	error:
		writeobj(fd, feedback_message, strlen(feedback_message));
		rem_conn();
		FREE(feedback_message);
		FREE(message);
		FREE(name);
		FREE(adress);
		CLOSE(fd);
		return (void*) 1;
}


/*
	elaborates the request from the client
	at the end frees the request and *obj
	returns 0 if client requested disconnection, 1 otherwise
*/
int elaborate_message(int fd, char* request, void** obj, size_t* obj_len, char* storage_dir){
	if(request == NULL){
		char* m;
		size_t length = sizeof(char) * (strlen(ERR_NULL_REQ) + 1 + KO_LEN);
		MALLOC_NONFATAL(m, length, fd);
		strcpy(m, KO);
		strcat(m, ERR_NULL_REQ);
		writeobj(fd, m, length - sizeof(char));
		FREE(m);
		return 0;
	}
	char* feedback_message = NULL;
	char* name = NULL; //used to store name if needed
	char* full_path = NULL; //used to store the path of the object
	size_t slength = 0; //used to store the length of the strings
	int file_fd = -1; //used to store file descriptor

	switch(request_type(request)){
		case REGN:
			//register request, registration should be already done
			MALLOC_NONFATAL(feedback_message, sizeof(char) * (KO_LEN + strlen(ERR_ALREADY_CONNECTED) + 1), fd);
			strcpy(feedback_message, KO);
			strcat(feedback_message, ERR_ALREADY_CONNECTED);
			goto error;
		case STON:
			//store obj in folder

			//setting obj file path
			name = getname(request, fd);
			MALLOC_NONFATAL(full_path, sizeof(char) * (strlen(storage_dir) + strlen(name) + 1), fd);
			strcpy(full_path, storage_dir);
			strcat(full_path, name);

			//if already exists remove size from total before override
			rem_objsize(getfilesize(full_path));

			//opening (and, eventually, overriding) file, writing obj
			SYS_CALL_NONFATAL(file_fd = open(full_path, O_WRONLY | O_CREAT, 0666), ERR_FOPEN, feedback_message);

			//write object to file
			if(writeobj(file_fd, *obj, *obj_len) == 0)//write obj to file
				goto error;

			add_objsize(*obj_len); //add obj_len to total count

			//send response
			slength = sizeof(char) * (OK_LEN + 1);
			MALLOC_NONFATAL(feedback_message, slength, fd);
			strcpy(feedback_message, OK);
			IO_CHECK(writeobj(fd, feedback_message, slength - sizeof(char)), feedback_message);
			break;
		case RETN:
			//get obj by its name

			//setting obj file path
			name = getname(request, fd);
			MALLOC_NONFATAL(full_path, sizeof(char) * (strlen(storage_dir) + strlen(name) + 1), fd);
			strcpy(full_path, storage_dir);
			strcat(full_path, name);

			//checking existance than opening to read obj
			SYS_CALL_NONFATAL(access(full_path, F_OK), ERR_404, feedback_message); //checks if the file exists
			SYS_CALL_NONFATAL(file_fd = open(full_path, O_RDONLY, 0), ERR_FOPEN, feedback_message);
			*obj_len = getfilesize(full_path);
			MALLOC_NONFATAL(*obj, *obj_len, fd);

			//read from file
			size_t toread = *obj_len;
			ssize_t read_n = 0;
			while(toread > 0){
				SYS_CALL_NONFATAL(read_n = read(file_fd, *obj, *obj_len), ERR_FREAD, feedback_message);
				toread -= read_n;
			}

			//send response
			slength = sizeof(char) * (DAT_LEN + 1 + (long) log2((double) *obj_len));
			MALLOC_NONFATAL(feedback_message, slength, fd);
			strcpy(feedback_message, DAT);
			sprintf(feedback_message + DAT_LEN, "%ld\n", *obj_len);
			IO_CHECK(writeobj(fd, feedback_message, strlen(feedback_message)), feedback_message);
			IO_CHECK(writeobj(fd, *obj, *obj_len), feedback_message);
			break;
		case DELN:
			//del obj by its name

			//setting obj file path
			name = getname(request, fd);
			MALLOC_NONFATAL(full_path, sizeof(char) * (strlen(storage_dir) + strlen(name) + 1), fd);
			strcpy(full_path, storage_dir);
			strcat(full_path, name);

			//checking existance than unlinking file
			SYS_CALL_NONFATAL(access(full_path, F_OK), ERR_404, feedback_message); //checks if the file exists
			*obj_len = getfilesize(full_path);
			SYS_CALL_NONFATAL(unlink(full_path), ERR_FDEL, feedback_message);
			rem_objsize(*obj_len); //remove obj_len from total count

			//send response
			slength = sizeof(char) * (OK_LEN + 1);
			MALLOC_NONFATAL(feedback_message, slength, fd);
			strcpy(feedback_message, OK);
			IO_CHECK(writeobj(fd, feedback_message, slength - sizeof(char)), feedback_message);
			break;
		case LEAN:
			//send leave message
			slength = sizeof(char) * (OK_LEN + 1);
			MALLOC_NONFATAL(feedback_message, slength, fd);
			strcpy(feedback_message, OK);
			IO_CHECK(writeobj(fd, feedback_message, slength - sizeof(char)), feedback_message);
			FREE(feedback_message);
			FREE(*obj);
			FREE(request);
			return 0; //makes thread terminate
			break;
		default:
			goto error;
	}
	CLOSE(file_fd);
	FREE(feedback_message);
	FREE(*obj);
	FREE(request);
	FREE(name);
	FREE(full_path);
	return 1;

	error:
		//send error message set in feedback_message
		writeobj(fd, feedback_message, strlen(feedback_message));
		CLOSE(file_fd);
		FREE(*obj);
		FREE(request);
		FREE(feedback_message);
		FREE(name);
		FREE(full_path);
		return 0;
}


/*
	returns the name contained in m
	if an error occurs returns NULL
*/
char* getname(char* m, int fd){
	if(m != NULL && m[0] != '\0'){
		int name_start = 0;
		while(m[name_start++] != ' '); //gets to the start

		int length = 0; //length of the name
		while(m[name_start + length] != ' ' && m[name_start + length] != '\n' && m[name_start + length] != '\0')
			length++;
		char* name = NULL;
		MALLOC_NONFATAL(name, sizeof(char) * (length + 1), fd);
		strncpy(name, m + name_start, length);
		name[length] = '\0';
		return name;
	}
	return NULL;
}


/*
	compares the request type in m and returns the respective id
*/
int request_type(char* m){
	if(m != NULL){
		if(strncmp(m, REG, REG_LEN) == 0)
			return REGN;
		if(strncmp(m, STO, STO_LEN) == 0)
			return STON;
		if(strncmp(m, RET, RET_LEN) == 0)
			return RETN;
		if(strncmp(m, DEL, DEL_LEN) == 0)
			return DELN;
		if(strncmp(m, LEA, LEA_LEN) == 0)
			return LEAN;
	}
	return -1;
}


/*
	reads from file fd, char by char until new line
	if the client sends binary code it is put in obj, put obj len inside len
	returns the message read, NULL in case of error
*/
char* readline(int fd, void** obj, size_t* len){
	char* buf = NULL;
	char* error_message = NULL;//to be used in case of error
	size_t cur_len = BASE_LENGTH;
	MALLOC_NONFATAL(buf, cur_len * sizeof(char), fd);
	size_t readen = 0;
	ssize_t read_n = 0;

	if(!socket_check(fd)) //waits for read
		goto error;

	do{
		if(readen >= cur_len - cur_len/5){ //expand buf
			REALLOC_NONFATAL(buf, sizeof(char) * (cur_len + BASE_LENGTH), fd);
			cur_len += BASE_LENGTH;
		}
		//reading for buf length
		SYS_CALL_NONFATAL(read_n = read(fd, buf + readen, sizeof(char) * (cur_len - readen)), ERR_READ, error_message);
		readen += read_n;
	}while(read_n != 0 && newlineindex(buf, readen) == -1); //if '\n' is not found, keep reading

	ssize_t newlineindex1 = newlineindex(buf, readen);
	if(newlineindex1 == -1){ // '\n' not found
		FREE(buf);
		return NULL;
	}
	buf[newlineindex1] = '\0';

	if(strncmp(buf, STO, STO_LEN) == 0){//need to read object
		int buf_index = STO_LEN; //the index where the name starts
		while(buf[buf_index] != ' ' && buf[buf_index] != '\0')buf_index++; //gets where the length starts

		size_t length = strtol(buf + buf_index, NULL, 10);
		size_t obj_readen = 0;

		if(obj == NULL)
			return NULL;
		MALLOC_NONFATAL((*obj), length, fd);
		*len = length;

		//copy what's exceeding on buf to obj
		for(ssize_t i = newlineindex1 + 1; i < readen; i++){
			((char*) *obj)[obj_readen] = buf[i];
			obj_readen++;
		}

		if(!socket_check(fd))
			goto error;
		while(obj_readen < length){
			SYS_CALL_NONFATAL(read_n = read(fd, ((char*) (*obj)) + obj_readen, sizeof(char) * (length - obj_readen)), ERR_READ, error_message);
			obj_readen += read_n;
		}
	}

	return buf;

	error:
		writeobj(fd, error_message, strlen(error_message));
		FREE(error_message);
		FREE(buf);
		FREE(*obj);
		return NULL;
}

/*
	writes the obj to fd for len bytes
	returns 0 in case of error, 1 otherwise
*/
int writeobj(int fd, void* _obj, size_t len){
	if(_obj == NULL)
		return 0;

	char* obj = _obj;
	char* error_message = NULL; //to be used in case of error
	size_t written = 0;

	while(written < len){
		ssize_t write_n = 0;
		SYS_CALL_NONFATAL(write_n = write(fd, obj + written, sizeof(char) * (len - written)), ERR_WRITE, error_message);
		written += write_n;
	}

	return 1;

	error:
		fprintf(stderr, "%s\n", error_message);
		FREE(error_message);
		return 0;
}


/*
	uses a select with a timeout to listen for read on fd
	returns 1 -> ready to read; 0 -> process is being terminated
*/
int socket_check(int fd){
	struct timeval m_tv = {0, 500};
	fd_set m_set;
	FD_ZERO(&m_set);
	FD_SET(fd, &m_set);
	fd_set set;
	do{
		set = m_set;
		struct timeval tv = m_tv;
		if(select(fd + 1, &set, NULL, NULL, &tv) < 0)
			return 0;
		if(terminate)
			return 0;
	}while(!FD_ISSET(fd, &set));
	
	return 1;
}

/*
	from the realtive path, gets the file size
	returns 0 in case of error
*/
off_t getfilesize(char* rel_path){
	struct stat statbuf;

	size_t path_len = strlen(rel_path);

	char* full = malloc(sizeof(char) * (UNIX_PATH_MAX + 1 + path_len));
	if(full == NULL)
		return 0;
	if(getcwd(full, sizeof(char) * (UNIX_PATH_MAX + 1 + path_len)) == 0)
		return 0;

	strcat(full, rel_path + 1); //rel_path + 1 -> ignoring '.' at the beginning

	if(stat(full, &statbuf) < 0){
		FREE(full);
		return 0;
	}
	FREE(full);
	return statbuf.st_size;
}

/*
	looks for '\n' in s
	returns -1 in case of error
*/
ssize_t newlineindex(char* s, size_t len){
	size_t i = 0;
	while(i < len && s[i] != '\n')
		i++;
	return i == len ? -1: i;
}

//adds a connect to total count
void add_conn(){
	THREAD_CHECK(pthread_mutex_lock(&mutex_conn), "Lock acquire");
	conn_n++;
	THREAD_CHECK(pthread_mutex_unlock(&mutex_conn), "Lock release");
}

//removes a connection from total count
void rem_conn(){
	THREAD_CHECK(pthread_mutex_lock(&mutex_conn), "Lock acquire");
	conn_n--;
	THREAD_CHECK(pthread_mutex_unlock(&mutex_conn), "Lock release");
}


//increases objects_size var using a mutex
void add_objsize(size_t s){
	if(s != 0){
		THREAD_CHECK(pthread_mutex_lock(&mutex_objects), "Lock acquire");
		objects_n++;
		objects_size += s;
		THREAD_CHECK(pthread_mutex_unlock(&mutex_objects), "Lock release");
	}
}

//decreases objects_size var using a mutex
void rem_objsize(size_t s){
	if(s != 0){
		THREAD_CHECK(pthread_mutex_lock(&mutex_objects), "Lock acquire");
		objects_n--;
		objects_size -= s;
		THREAD_CHECK(pthread_mutex_unlock(&mutex_objects), "Lock release");
	}
}


/*
	in the case the server has been running for some time, shut down and restarted
	it initializes the val of objstore_size to the previous size
	(computes the size of the data folder)

	rec fun, adds the size of every obj in the folder
	and calls itself if another folder is found

	if an error occurs it return immediatly after printing the error
*/
void init_objsize(char* dirname){
	struct stat statbuf;
	if(stat(dirname, &statbuf) < 0){
    	if(strcmp(dirname, DATA_DIR) == 0)
    		return; //error accessing data dir -> not found -> no need to look for objects
    	perror("Init_objsize: Cannot access dir");
    	return;
	}

	DIR* dir;

	if ((dir = opendir(dirname)) == NULL) {
		perror("Opendir");
		return;
	} else {
		struct dirent *cur_file;

    	errno = 0;
		while((cur_file = readdir(dir)) != NULL) {
			struct stat statbuf;
			char filename[MAX_FILENAME];
			if ((strlen(dirname) + strlen(cur_file -> d_name) + 2) > MAX_FILENAME) {
				fprintf(stderr, "Init_objsize: length needed is greater than MAXFILENAME\n");
				return;
			}

			strcpy(filename, dirname);
			strcat(filename, "/");
			strncat(filename, cur_file -> d_name, strlen(cur_file -> d_name) + 1);

			if (stat(filename, &statbuf) == -1) {
				perror("Init_objsize: Cannot access file");
				return;
			}

			if(S_ISDIR(statbuf.st_mode)) {
		    		//is dir
		    		int dirlen = strlen(filename);
					if(dirlen > 0 && filename[dirlen - 1] != '.')//dir to be checked
						init_objsize(filename); //recursive call
			}else{
		 	//is file
		 		add_objsize(statbuf.st_size);
			}
		}
		if(errno != 0)
			perror("readdir");
		if(closedir(dir) < 0)
			perror("close dir");
    }
}
