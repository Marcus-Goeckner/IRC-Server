#file to compile irc project source code files
#once compiled, run server and client executables in this directory (same directory as Makefile) for testing

all : server client

server: ./ircd/server.c
	gcc -o server ./ircd/server.c ./ircd/linked_list.c ./ircd/client_handler.c ./ircd/channel.c ./ircd/logger.c ./ircd/config.c -lpthread ./ircd/commands.c

client: ./ircc/client.c
	gcc -o client ./ircc/client.c -lpthread

clean:
	rm server client 

#target: dependencies
#	action
