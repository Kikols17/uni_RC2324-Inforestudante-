all: class_server class_client



class_server: class_server.o commands_server.o
	gcc -Wall -Wextra class_server.o commands_server.o -pthread -o class_server

class_client: class_client.o
	gcc -Wall -Wextra class_client.o -o class_client



class_server.o: class_server.c
	gcc -Wall -Wextra -c class_server.c

class_client.o: class_client.c
	gcc -Wall -Wextra -c class_client.c

commands_server.o: commands_server.c commands_server.h
	gcc -Wall -Wextra -c commands_server.c



clear:
	rm -f *.o
	rm -f class_server class_client
