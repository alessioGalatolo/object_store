/*
	Progetto SOL AA 2018/2019 
	Alessio Galatolo
*/

#include <stdio.h>
#include <objstr.h>
#include <stdlib.h>
#include <string.h>

#define BOUND 100000
#define BASE 100
#define TESTS 20
//INTERVAL = (BOUND + BASE) / TESTS;
#define INTERVAL (BOUND + BASE) / TESTS

//a name to be used for the objects
#define GENERAL_NAME "objectName"


#define NULL_CHECK(x)\
	if((x) == NULL){perror("Malloc error"); exit(1);}

/*
	creates an object of type int* / char* based on the parity of the index
	object contains preset values also based on the index

	returns always void* to the obj
*/
void* object_factory(int index, size_t* size){
	if(index % 2 == 0){//if even creates stream of ints
		*size = BASE + INTERVAL * index; //number of bytes to create
		while((*size) % sizeof(int) != 0)*size = (*size) + 1; //increase until multiple of sizeof(int)

		int len = (*size) / sizeof(int); //length of array
		int* array = NULL;
		NULL_CHECK(array = malloc(*size));
		for(int i = 0; i < len; i++){
			array[i] = i % BASE; //a number from 0 to BASE
		}
		return array;
	}else{//if odd creates stream of chars
		*size = BASE + INTERVAL * index; //bytes to create
		while((*size) % sizeof(char) != 0)*size = (*size) + 1; //increase until multiple of sizeof(char)

		int len = *size / sizeof(char); //length of array
		char* array = NULL;
		NULL_CHECK(array = malloc(*size));
		for(int i = 0; i < len; i++){
			array[i] = (index % 25) + 97; //a letter from the alphabet
		}
		return array;
	}
}

/*
	gets the expected obj by calling object_factory 
	checks every byte with the obj passed as parameter

	returns 1 -> ok; 0 -> mismatch
*/
int objcmp(void* _obj, int index){
	char* obj = _obj; //no need to cast to int* the evens because sizeof(char) < sizeof(int) && sizeof(char) % sizeof(int) == 0
	size_t len;
	char* array = object_factory(index, &len); //get the expected obj
	len /= sizeof(char); //get true length
	for(int i = 0; i < len; i++)
		if(obj[i] != array[i]){
			free(array);
			return 0;//mismatch
		}
	free(array);
	
	return 1; //ok
}

int main(int argc, char** argv){
	if(argc != 3 ){
		fprintf(stderr, "usage: %s <client name> <1/2/3>\n", argv[0]);
		exit(1);
	}
	int test = strtol(argv[2], NULL, 10); //gets test number
	if(test > 3 || test < 1){
		fprintf(stderr, "usage: %s <client name> <1/2/3>\nGot test != 1/2/3\n", argv[0]);
		exit(1);
	}

	//creating general object name
	char* general_name = GENERAL_NAME;
	int len = strlen(general_name);
	char* obj_name = malloc(sizeof(char) * (len + 3));
	NULL_CHECK(obj_name);
	strncpy(obj_name, general_name, len);
	obj_name[len] = '0';
	obj_name[len + 1] = '0';
	obj_name[len + 2] = '\0';

	int outcome = 0; //keeps track of the tests done
	if((outcome = os_connect(argv[1])) == 0)
		goto error;

	switch(test){
		case 1:
			for(int i = 0; i < TESTS; i++){
				//setting the obj number, obj_name = "general_name$i"
				obj_name[len] = (i / 10) + 48;
				obj_name[len + 1] = (i % 10) + 48; 

				size_t size;
				void *obj = object_factory(i, &size); //getting obj
				if(os_store(obj_name, obj, size) == 0)
					goto error;
				free(obj);
				outcome++; //successful try
			}
			break;
		case 2:
			for(int i = 0; i < TESTS; i++){
				obj_name[len] = (i / 10) + 48;
				obj_name[len + 1] = (i % 10) + 48; //setting the obj number
				void* obj = os_retrive(obj_name);
				if(obj == NULL)
					goto error;
				if(objcmp(obj, i) == 0)
					goto error;
				free(obj);
				outcome++;
			}
			break;
		case 3:
			for(int i = 0; i < TESTS; i++){
				obj_name[len] = (i / 10) + 48;
				obj_name[len + 1] = (i % 10) + 48; //setting the obj number
				if(os_delete(obj_name) == 0)
					goto error;
				outcome++;
			}
			break;
		default:
			fprintf(stderr, "Unexpexted case\n");
			exit(1);
	}
	
	if(os_disconnect() == 0)
		goto error;
	outcome++;
	
	free(obj_name);
	printf("Test number: %d\nTests done: %d\nTests succeded: %d\nTests failed: %d\n", test, outcome, outcome, 0);
	return 0;

	error:
		free(obj_name);
		printf("Test number: %d\nTests done: %d\nTests succeded: %d\nTests failed: %d\n", test, outcome + 1, outcome, 1);
		return -1;
}
