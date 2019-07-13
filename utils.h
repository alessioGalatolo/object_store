/*
	Progetto SOL AA 2018/2019
	Alessio Galatolo
*/

//a collection of utilities for the communication client-server

#if !defined(_COMM_COMM_)

#define _COMM_COMM_

//utils

//string base length
#define BASE_LENGTH 100

#define SOCKET_PATH "./objstore.sock"
#define DATA_DIR "./data"
#define UNIX_PATH_MAX 108

//max length of a filename
#define MAX_FILENAME 512

//max attempts for a malloc or similar
#define MAX_ATTEMPTS 3

//communication types: each name is represented as a tuple <id, name, length>
#define REGN 1
#define REG "REGISTER "
#define REG_LEN 9

#define STON 2
#define STO "STORE "
#define STO_LEN 6

#define RETN 3
#define RET "RETRIVE "
#define RET_LEN 8

#define DELN 4
#define DEL "DELETE "
#define DEL_LEN 7

#define LEAN 5
#define LEA "LEAVE "
#define LEA_LEN 6

#define DATN 6
#define DAT "DATA "
#define DAT_LEN 5

#define OKN 7
#define OK "OK \n"
#define OK_LEN 4

#define KON 8
#define KO "KO "
#define KO_LEN 3



//list of possible errors by the server
#define ERR_ALREADY_CONNECTED "The client is already connected \n"
#define ERR_NOT_CONNECTED "Request sent before connection\n"
#define ERR_FOPEN "Cannot open file for storage \n"
#define ERR_FWRITE "Cannot write to file \n"
#define ERR_FREAD "Cannot read from file \n"
#define ERR_WRITE "Cannot write to socket \n"
#define ERR_READ "Cannot read from socket \n"
#define ERR_MALLOC "Cannot allocate memory \n"
#define ERR_404 "Object file not found \n"
#define ERR_FDEL "Cannot delete file \n"
#define ERR_NULL_REQ "Request sent was found NULL\n"
#define ERR_EINVAL "Pointer passed as param is NULL\n"

//error to be sent when malloc cannot be performed
#define MALLOC_KO "KO cannot allocate memory \n"
#define MALLOC_KO_LEN 27

#endif
