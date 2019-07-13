# Object Store
This is a project done for the course "Operative systems and laboratory" during my bachelor in Computer science at the university of Pisa.

The project consists of creating a client-server system where the server can memorize various objects for the client and retrive/delete them when needed.
The connection is enstablished using a socket and client-server are meant to be on the same machine. The server should operate multi-threaded and handle multiple request at once. The server also provides custom behavior to signals.

The client should use the functions provided in "objstr.c" which grant the use of the right communication protocols.

"client_test.c", "test.sh", "testsum.sh" are used to test the server (call 'make test') while there is also a very simple library for thread creation.
Compilation of all the file needed can be done using makefile (make all) and clean up can be done with "make clean".

The project uses POSIX standard and should be portable across linux environments.

A long description, in italian, can be found in "info (italian).pdf".
