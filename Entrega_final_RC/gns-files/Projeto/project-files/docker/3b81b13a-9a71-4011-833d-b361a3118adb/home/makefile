all: class_server class_client class_admin configs


server: class_server configs
	# Create server only

client: class_client
	# Create client only

admin: class_admin
	# Create admin only



configs:
	# Resets configs to default
	echo "micas;castela;aluno\njoel;1234;professor\nadam;eve;administrator" > class_config.config




test: file_manager_test.o file_manager.o
	gcc -Wall -Wextra file_manager_test.o file_manager.o -o file_manager_test.exe





class_server: class_server.o commands_server.o class_struct.o file_manager.o
	gcc -Wall -Wextra class_server.o commands_server.o class_struct.o file_manager.o -pthread -o class_server

class_client: class_client.o
	gcc -Wall -Wextra class_client.o -pthread -o class_client

class_admin: class_admin.o
	gcc -Wall -Wextra class_admin.o -pthread -o class_admin



class_server.o: class_server.c
	gcc -Wall -Wextra -c class_server.c

class_client.o: class_client.c
	gcc -Wall -Wextra -c class_client.c

commands_server.o: commands_server.c commands_server.h
	gcc -Wall -Wextra -c commands_server.c

class_struct.o: class_struct.c class_struct.h
	gcc -Wall -Wextra -c class_struct.c

file_manager.o: file_manager.c file_manager.h
	gcc -Wall -Wextra -c file_manager.c

class_admin.o: class_admin.c
	gcc -Wall -Wextra -c class_admin.c


file_manager_test.o: file_manager_test.c
	gcc -Wall -Wextra -c file_manager_test.c



clean:
	rm -f *.o
	rm -f class_server class_client class_admin file_manager_test.exe
