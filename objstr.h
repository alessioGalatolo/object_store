/*
	Progetto SOL AA 2018/2019 
	Alessio Galatolo
*/

//interface to communicate with the server

#if !defined(_OBJSTR_H_)
#define _OBJSTR_H_

#include <stdlib.h>

char* get_lastservererror();
char* get_lasterror();
int os_connect(char* name);
int os_store(char* name, void* block, size_t len);
void* os_retrive(char* name);
int os_delete(char* name);
int os_disconnect();


#endif
