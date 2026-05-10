#include "global.h"
#include "server.h"
#include "sigint_handler.h"
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
	int port = -1; 
	// Sets the default for each option
	int board_size = BOARD_SIZE_DEFAULT;
	unsigned int seed = (unsigned int)time(NULL);
	int max_players = MAX_PLAYERS_DEFAULT;
	
	// Iterates through all arguments and if "-h" is found, prints help message
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-h") == 0) {
			PRINT_USAGE();
			return EXIT_SUCCESS;
		}
	}

	int opt;
	// Iterates through all arguments
	while ((opt = getopt(argc, argv, "p:b:s:m:h")) != -1) {
		switch (opt) {
			// If "-p" is found
			case 'p': {
				int port_num = atoi(optarg);
				// Checks if provided port is a valid number and if not, prints error message
				if (port_num < 1 || port_num > 65535) {
					ERR_INVALID_PORT(optarg);
					return EXIT_FAILURE;
				}
				port = port_num;
				break;
			}
			// If "-b" is found
			case 'b': {
				int size = atoi(optarg);
				// Checks if provided board size is a valid size and if not, prints error message
				if (size < BOARD_SIZE_MIN || size > BOARD_SIZE_MAX) {
					ERR_INVALID_BOARD_SIZE(size);
					return EXIT_FAILURE;
				}
				board_size = size;
				break;
			}
			// If "-s" is found
			case 's': {
				seed = (unsigned int)atoi(optarg);
				break;
			}
			// If "-m" is found
			case 'm': {
				int players = atoi(optarg);
				// Checks if provided max players is a valid number and if not, prints error message
				if (players < MAX_PLAYERS_MIN || players > MAX_PLAYERS_MAX) {
					ERR_INVALID_MAX_PLAYERS(players);
					return EXIT_FAILURE;
				}
				max_players = players;
				break;
			}
			// If "-h" is found
			case 'h':
				break;
			// If an unknown option is found, prints help message
			default:
				PRINT_USAGE();
				return EXIT_FAILURE;
		}
	}
	// Checks if port number was not provided and if so, prints error message
	if (port == -1) {
		ERR_PORT_REQUIRED();
		return EXIT_FAILURE;
	}

	server_t snake_server;
	// Initializes server and if it fails, prints error message
	int result = server_init(&snake_server, port, board_size, max_players, seed);
	if (result < 0) {
		ERR_BIND_FAILED(port);
		return EXIT_FAILURE;
	}

	server = &snake_server;

	// Installs signal handler for SIGINT
	signal(SIGINT, sigint_handler);

	// Prints message if successful startup
	PRINT_SERVER_STARTED(port, board_size, max_players);
	
	// Starts the main server loop and if it fails, cleans up all server resources
	int res = server_start(&snake_server);
	if (res < 0) {
		server_cleanup(&snake_server);
		return EXIT_FAILURE;
	}

	// Cleans up all server resources
	server_cleanup(&snake_server);
	return EXIT_SUCCESS;
}