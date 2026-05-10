#include "server.h"
#include "debug.h"
#include "global.h"
#include "protocol.h"
#include "sigint_handler.h"
#include <sys/socket.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>

server_t *server = NULL;

// Signal handler for SIGINT
void sigint_handler(int s) {
	if (server != NULL) {
		// Sets server running to 0
		server->running = 0;

		// Closes listening socket
		if (server->listen_fd != -1) {
			close(server->listen_fd);
			server->listen_fd = -1;
		}
	}
}

int server_init(server_t *server, int port, int board_size, int max_snakes, unsigned int seed) {
	// Checks if provided pointer to server state is NULL
	if (server == NULL) {
		return -1;
	}

	// Creates TCP listening socket
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd == -1) {
		return -1;
	}
	server->listen_fd = fd;

	// Sets options for socket
	int opt = 1;
	if (setsockopt(server->listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
		close(server->listen_fd);
		server->listen_fd = -1;
		return -1;
	}

	struct sockaddr_in address;
	memset(&address, 0, sizeof(address));
	address.sin_family = AF_INET;
	address.sin_port = htons(port);
	address.sin_addr.s_addr = INADDR_ANY;

	// Binds to specified port
	if (bind(server->listen_fd, (struct sockaddr *)&address, sizeof(address)) == -1) {
		close(server->listen_fd);
		server->listen_fd = -1;
		return -1;
	}

	// Listens for connections
	if (listen(server->listen_fd, max_snakes) == -1) {
		close(server->listen_fd);
		server->listen_fd = -1;
		return -1;
	}

	// Initializes all entries in client_fds and client_snake_ids to -1
	for (int i = 0; i < MAX_PLAYERS; i++) {
		server->client_fds[i] = -1;
		server->client_snake_ids[i] = -1;
	}

	// Initializes game board
	int result = board_init(&server->board, board_size, max_snakes, seed);
	if (result == -1) {
		close(server->listen_fd);
		server->listen_fd = -1;
		return -1;
	}

	// Initializes the mutex
	if (pthread_mutex_init(&server->board_mutex, NULL) != 0) {
		board_free(&server->board);
		close(server->listen_fd);
		server->listen_fd = -1;
		return -1;
	}

	// Sets server running flag
	server->running = 1;
	return 0;
}

void *server_game_loop(void *arg) {
	// Checks if arg is NULL
	if (arg == NULL) {
		return NULL;
	}
	server_t *server = (server_t *)arg;

	// Loops until server->running is set to 0
	while (server->running == 1) {
		// Sleeps 
		usleep(TICK_INTERVAL_MS * 1000);

		snake_t snakes_before_advance[MAX_PLAYERS];
		int fds_clients[MAX_PLAYERS];
		int dead_snakes[MAX_PLAYERS];
		uint8_t buffer[GAME_STATE_BUF_SIZE];
		uint8_t dead_buf[MAX_PLAYERS][2];
		snake_t snakes_after_advance[MAX_PLAYERS];

		// Locks mutex
		pthread_mutex_lock(&server->board_mutex);

		// Iterates through all players and saves the client's fd and the snake's fields
		for (int i = 0; i < MAX_PLAYERS; i++) {
			snakes_before_advance[i] = server->board.snakes[i];
			fds_clients[i] = server->client_fds[i];
			dead_snakes[i] = 0;
		}

		// Advances all snakes
		board_tick(&server->board);

		// Iterates through all players and checks for dead snakes and if found, serializes a dead player message for that snake's client
		for (int i = 0; i < MAX_PLAYERS; i++) {
			if ((snakes_before_advance[i].alive == 1) && (server->board.snakes[i].alive == 0)) {
				dead_snakes[i] = protocol_serialize_dead(dead_buf[i], sizeof(dead_buf[i]), i);
			}
		}

		// Iterates through all players and saves snake's fields
		for (int i = 0; i < MAX_PLAYERS; i++) {
			snakes_after_advance[i] = server->board.snakes[i];
		}

		// Serializes game state 
		int byte_count = protocol_serialize_game_state(buffer, sizeof(buffer), &server->board);

		// Unlocks the mutex
		pthread_mutex_unlock(&server->board_mutex);

		// Iterates through all players and if the client's snake is dead, sends a dead message to that client
		for (int i = 0; i < MAX_PLAYERS; i++) {
			if (dead_snakes[i] > 0) {
				if (fds_clients[i] != -1) {
					send_all(fds_clients[i], dead_buf[i], dead_snakes[i]);
				}
			}
		}

		// Broadcasts serialized game state to all connected clients with valid fds
		if (byte_count > 0) {
			for (int i = 0; i < MAX_PLAYERS; i++) {
				if (fds_clients[i] != -1 && snakes_after_advance[i].alive == 1) {
					send_all(fds_clients[i], buffer, byte_count);
				}
			} 
		}
	}
	return NULL;
}

void *server_client_handler(void *arg) {
	// Checks if arg is NULL
	if (arg == NULL) {
		return NULL;
	}
	// Defines client handler struct to pass server pointer and client fd
	client_handler_arg_t *client_handler = (client_handler_arg_t *)arg;

	// Checks if client handler has received 2 bytes and if not, closes socket and frees argument struct
	uint8_t recv_buffer[2];
	int result = recv_exact(client_handler->client_fd, recv_buffer, sizeof(recv_buffer));
	if (result < 0) {
		close(client_handler->client_fd);
		free(client_handler);
		return NULL;
	}

	// Checks if first message is not JOIN 
	uint8_t out_type, out_payload;
	int res = protocol_deserialize_client_msg(recv_buffer, sizeof(recv_buffer), &out_type, &out_payload);
	if (res < 0) {
		// Sends an error
		uint8_t error_buf[2];
		int bytes = protocol_serialize_error(error_buf, sizeof(error_buf), ERR_INVALID_MSG);
		if (bytes > 0) {
			send_all(client_handler->client_fd, error_buf, bytes);
		}
		// Closes socket and frees argument struct
		close(client_handler->client_fd);
		free(client_handler);
		return NULL;
	}

	// Checks if first message is not JOIN 
	if (out_type != MSG_JOIN) {
		// Sends an error
		uint8_t error_buf[2];
		int bytes = protocol_serialize_error(error_buf, sizeof(error_buf), ERR_INVALID_MSG);
		if (bytes > 0) {
			send_all(client_handler->client_fd, error_buf, bytes);
		}
		// Closes socket and frees argument struct
		close(client_handler->client_fd);
		free(client_handler);
		return NULL;
	}

	// Locks mutex
	pthread_mutex_lock(&client_handler->server->board_mutex);

	int out_id;
	// Checks if adding player's snake to game board fails
	if (board_add_snake(&client_handler->server->board, &out_id) < 0) {
		// Serializes an error message saying board is full
		uint8_t error_buf[2];
		int bytes = protocol_serialize_error(error_buf, sizeof(error_buf), ERR_GAME_FULL);

		// Unlocks mutex
		pthread_mutex_unlock(&client_handler->server->board_mutex);

		// Sends error message 
		if (bytes > 0) {
			send_all(client_handler->client_fd, error_buf, bytes);
		}

		// Closes socket and frees argument struct
		close(client_handler->client_fd);
		free(client_handler);
		return NULL;
	}

	// Serializes welcome message 
	uint8_t welcome_buf[4];
	int bytes_num = protocol_serialize_welcome(welcome_buf, sizeof(welcome_buf), out_id, client_handler->server->board.size, client_handler->server->board.max_snakes);

	// Registers client fd and snake id
	client_handler->server->client_fds[out_id] = client_handler->client_fd;
	client_handler->server->client_snake_ids[out_id] = out_id;
	
	// Unlocks mutex
	pthread_mutex_unlock(&client_handler->server->board_mutex);

	// Sends welcome message
	if (bytes_num > 0) {
		send_all(client_handler->client_fd, welcome_buf, bytes_num);
	}

	// Iterates until server stops running, client leaves or client disconnects
	while (client_handler->server->running == 1) {
		// Reads 2 byte messages from client socket
		uint8_t recv_buff[2];
		if (recv_exact(client_handler->client_fd, recv_buff, sizeof(recv_buff)) < 0) {
			break;
		}

		// Deserializes messages and if message is invalid, sends an error message
		uint8_t type, payload;
		int num = protocol_deserialize_client_msg(recv_buff, sizeof(recv_buff), &type, &payload);
		if (num < 0) {
			uint8_t error_buf[2];
			int bytes = protocol_serialize_error(error_buf, sizeof(error_buf), ERR_INVALID_MSG);
			if (bytes > 0) {
				send_all(client_handler->client_fd, error_buf, bytes);
			}
			continue;
		}

		// Checks if it is a direction message
		if (type == MSG_DIRECTION) {
			// Locks mutex
			pthread_mutex_lock(&client_handler->server->board_mutex);

			// Updates player's direction
			if (client_handler->server->board.snakes[out_id].alive == 1) {
				snake_set_direction(&client_handler->server->board.snakes[out_id], (direction_t) payload);
			}

			// Unlocks mutex
			pthread_mutex_unlock(&client_handler->server->board_mutex);
		// Checks if it is a leave message and if so, breaks out of loop
		} else if (type == MSG_LEAVE) {
			break;
		// Checks if it an invalid message and if so, sends error message
		} else {
			uint8_t error_buf[2];
			int bytes = protocol_serialize_error(error_buf, sizeof(error_buf), ERR_INVALID_MSG);
			if (bytes > 0) {
				send_all(client_handler->client_fd, error_buf, bytes);
			}
			continue;
		}
	}

	// Locks mutex
	pthread_mutex_lock(&client_handler->server->board_mutex);

	// Removes player's snake from game board
	board_remove_snake(&client_handler->server->board, out_id);

	// Clears entries to -1
	client_handler->server->client_fds[out_id] = -1;
	client_handler->server->client_snake_ids[out_id] = -1;

	// Unlocks mutex
	pthread_mutex_unlock(&client_handler->server->board_mutex);

	// Closes client socket and frees argument struct
	close(client_handler->client_fd);
	free(client_handler);
	return NULL;
}

int server_start(server_t *server) {
	// Checks if pointer to server is NULL
	if (server == NULL) {
		return -1;
	}

	// Creates game loop thread
	pthread_t game_loop_thread;
	if (pthread_create(&game_loop_thread, NULL, server_game_loop, server) != 0) {
		return -1;
	}

	// Detaches game loop thread
	pthread_detach(game_loop_thread);
	// Loops until server->running is 0
	while (server->running == 1) {
		// Accepts new client connection
		int fd = accept(server->listen_fd, NULL, NULL);
		if (fd == -1) {
			continue;
		}

		// Allocates argument struct
		client_handler_arg_t *client_handler = malloc(sizeof(client_handler_arg_t));
		if (client_handler == NULL) {
			close(fd);
			continue;
		}

		// Initializes argument struct with server pointer and client fd
		client_handler->server = server;
		client_handler->client_fd = fd;

		// Creates detached thread
		pthread_t detached_thread;
		if (pthread_create(&detached_thread, NULL, server_client_handler, client_handler) != 0) {
			close(fd);
			free(client_handler);
			continue;
		}

		// Detaches detached thread
		pthread_detach(detached_thread);
	}

	return 0;
}

void server_cleanup(server_t *server) { 
	// Checks if pointer to server is NULL
    if (server == NULL) {
		return ;
	}

	// Sets running to 0
	server->running = 0;

	// Locks mutex
	pthread_mutex_lock(&server->board_mutex);

	// Closes all client sockets and sets all ids to -1
	for (int i = 0; i < MAX_PLAYERS; i++) {
		if (server->client_fds[i] != -1) {
			close(server->client_fds[i]);
			server->client_fds[i] = -1;
		}
		server->client_snake_ids[i] = -1;
	}

	// Unlocks mutex
	pthread_mutex_unlock(&server->board_mutex);

	// Closes listening socket
	if (server->listen_fd != -1) {
		close(server->listen_fd);
		server->listen_fd = -1;
	}

	// Frees the board
	board_free(&server->board);

	// Releases synchronization resources
	pthread_mutex_destroy(&server->board_mutex);
}

int recv_exact(int fd, uint8_t *buf, size_t len) {
	size_t bytes_read = 0;
	size_t bytes_remaining = len;
	// Keeps receiving bytes until reached expected number of bytes or recv returns less than or equal to 0 
	while (bytes_read != len) {
		ssize_t bytes = recv(fd, buf + bytes_read, bytes_remaining, 0);
		if (bytes > 0) {
			bytes_read += bytes;
			bytes_remaining -= bytes;
		} else {
			return -1;
		}
	}
	return 0;
}

int send_all(int fd, const uint8_t *buf, size_t len) {
	size_t bytes_written = 0;
	size_t bytes_remaining = len;
	// Keeps sending bytes until reached expected number of bytes or send returns less than or equal to 0 
	while (bytes_written != len) {
		ssize_t bytes = send(fd, buf + bytes_written, bytes_remaining, 0);
		if (bytes > 0) {
			bytes_written += bytes;
			bytes_remaining -= bytes;
		} else {
			return -1;
		}
	}
	return 0;
}