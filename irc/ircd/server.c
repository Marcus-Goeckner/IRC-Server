/*
 * server.c
 * CS3337
 * Written by: Marcus Goeckner
 * Professor McGregor
 * 2/21/2020
 *
 * Creates a TCP server socket that binds to an IP address and port 
 * to listen for connections. Each connected client is put on an individual thread for communication.
*/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <pthread.h>
#include "linked_list.h"
#include "client_handler.h"
#include "logger.h"
#include "config.h"
#include "commands.h"
#include "channel.h"

void *communicate(void *);

void error(char *message) {
    fprintf(stderr, message);
    logger_write(message);
    exit(1);
}

int main(int argc, char *argv[]) {
	// output error message if no port argument is provided 
	if (argc == 2 && strcmp(argv[1], "help") == 0) {
		printf("Usage: %s (port) (overridableconfigsetting) (configvalue)\n", argv[0]);
		printf("Overridable settings: \'logfilelocation\' \'logginglevel\' \'maxnumclients\' \'buffsize\'\n");
		printf("NOTICE: command line arguments that override configuration file settings do not save after exit.\n");
		exit(1);
	} else if (argc < 2) {
	    printf("Usage: %s (port) (configoverride) (configvalue)\nUse %s help for more info\n", argv[0], argv[0]);
	    exit(1);
	}

	// establish server settings ***(need to be read in from config file)***
	config_readConfigFile("./ircd/server.conf");

	// allow for overriding of config file settings
	if (argc == 4) {
        if (strcmp(argv[2], "logfilelocation") == 0) {
            logger_log_file_location = argv[3];
        } else if (strcmp(argv[2], "logginglevel") == 0) {
            logger_logging_level = atoi(argv[3]);
        } else if (strcmp(argv[2], "maxnumclients") == 0) {
            client_handler_max_connections = atoi(argv[3]);
        } else if (strcmp(argv[2], "buffsize") == 0) {
            client_handler_buffer_size = atoi(argv[3]);
        } else {
            printf("Invalid config setting\n");
        }
	} else if (argc == 3 || argc > 4) {
        printf("Usage: %s (port) (configoverride) (configvalue)\nUse: %s help for more info\n", argv[0], argv[0]);
        exit(1);
	}

	int port_num = atoi(argv[1]);

	// create a new stream (TCP) socket
	int server_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (server_socket < 0) {
		error("ERROR: could not create server socket\n");
	}

	// define server address and declare a client address to be used 
	// when accepting connections
	struct sockaddr_in server_address, client_address;
	bzero((char *) &server_address, sizeof(server_address));
	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(port_num);
	server_address.sin_addr.s_addr = INADDR_ANY;

    // allow immediate reuse of this address
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0) {
        error("ERROR: setsockopt(SO_REUSEADDR) failed\n");
    }

    // allow immediate reuse of this port
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEPORT, &(int){1}, sizeof(int)) < 0) {
        error("ERROR: setsockopt(SO_REUSEPORT) failed\n");
    }

	// bind the socket to the specified IP address and port
	bind(server_socket, (struct sockaddr *) &server_address, sizeof(server_address));
	
	// listen for connections
	listen(server_socket, client_handler_max_connections);
	int client_len = sizeof(client_address);
	// loop to continuously accept connections

    // initialize mutex to lock critical regions for threads
    if(pthread_mutex_init(&client_handler_connections_mutex, NULL) != 0) {
        error("ERROR: mutex init failed\n");
    }

	while(client_handler_num_connections <= client_handler_max_connections) {
		// accept connections on the server socket and create new threads
		// to deal with clients
		struct Client *new_client = client_handler_getClientConnection(server_socket, &client_address, client_len);

		// Add new client to running list of clients
		linked_list_push(&client_list_head, new_client);

        client_handler_num_connections = linked_list_size(client_list_head);

		if (new_client->client_fd < 0) {
			error("ERROR: could not accept new connection\n");
		}
		char new_client_info[256];
		sprintf(new_client_info, "A new client has joined the server.\n"
                           "New client nick: %s\nNew client fd: %d\nNew client op status: %d\nConnected clients: %d\n",
		        new_client->nick, new_client->client_fd, new_client->is_op, client_handler_num_connections);
		printf("%s\n", new_client_info);
		logger_write(new_client_info);

		pthread_t thread;
		pthread_create(&thread, NULL, communicate, (void *)new_client); // spin off new thread for client
		pthread_detach(thread);

	}
	return 0;
}

// function to communicate with each client in their own thread
void* communicate(void* arg) {
	char buffer[client_handler_buffer_size];
	int conn_status = 0;
	struct Client *new_client = (struct Client *)arg;
	int new_client_socket = new_client->client_fd;
	char sender[256];
	char error_message[256];
	char command[100];
    int command_status = 0;
	while(conn_status >= 0) {
		bzero(buffer, client_handler_buffer_size);
		bzero(sender, 256);
		sprintf(sender, "Message From: ");
		conn_status = read(new_client_socket, buffer, client_handler_buffer_size - 1);
		if (conn_status < 0) {
			error("ERROR: could not read from socket\n");
		} else if (conn_status == 0) {  // client has disconnected
		    close(new_client_socket);

            pthread_mutex_lock(&client_handler_connections_mutex); // lock critical region
            struct LinkedListNode *node = linked_list_get(client_list_head, new_client);
            linked_list_delete(&client_list_head, node);    // remove client from list of clients
            client_handler_num_connections = linked_list_size(client_list_head);
            pthread_mutex_unlock(&client_handler_connections_mutex); // unlock critical region

            bzero(error_message, 256);
            sprintf(error_message, "%s has disconnected\n\n", new_client->nick);
            node = client_list_head;
            struct Client *client;

            pthread_mutex_lock(&client_handler_connections_mutex); // lock critical region
            while (node != NULL) {
                client = node->data;
                if (client->client_fd != new_client_socket && client->channel == new_client->channel) {
                    conn_status = write(client->client_fd, error_message, 100); // write disconnect message to clients
                }
                if (conn_status < 0) {
                    error("ERROR: could not write to socket");
                }
                node = node->next;
            }
            pthread_mutex_unlock(&client_handler_connections_mutex); // unlock critical region

            printf(error_message);
            logger_write(error_message);
		    pthread_exit(0);
		}

		if (strncmp(buffer, "/", 1) == 0) { // parse command from client
            bzero(command, 100);
            strcpy(command, buffer+1);

            pthread_mutex_lock(&client_handler_connections_mutex); // lock critical region
            command_status = commands_getCommand(command, new_client);  // call the right command based off client message
            commands_checkCommandStatus(command_status, new_client);
            pthread_mutex_unlock(&client_handler_connections_mutex); // unlock critical region

		} else if (strncmp(buffer, "\0", 1) != 0) { // prevent blank messages from flooding server
		    printf("Message from %s: %s\n", new_client->nick, buffer);
            struct LinkedListNode *node = client_list_head;
            struct Client *client;

            pthread_mutex_lock(&client_handler_connections_mutex); // lock critical region
            while(node != NULL) {
                client = node->data;
                if (client->client_fd == new_client_socket) {
                    strcat(sender, client->nick);   // get the name of the sender of a message
                }
                node = node->next;
            }
            node = client_list_head;
            // Loop through list of clients and broadcast received messages to all other clients
            while(node != NULL) {
                client = node->data;
                if (client->client_fd != new_client_socket && client->channel == new_client->channel && client->channel != NULL) {
                    conn_status = write(client->client_fd, &sender, 255);
                }
                if (client->client_fd != new_client_socket && client->channel == new_client->channel && client->channel != NULL) {
                    conn_status = write(client->client_fd, &buffer, 255);
                }
                if (conn_status < 0) {
                    error("ERROR: could not  write to socket");
                }
                node = node->next;
            }
            pthread_mutex_unlock(&client_handler_connections_mutex); // unlock critical region
		}
	}
	close(new_client_socket);
	return 0;
}
