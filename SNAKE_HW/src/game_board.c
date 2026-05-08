#include "game_board.h"
#include "debug.h"
#include "global.h"
#include <stdbool.h>

int board_init(game_board_t *board, int size, int max_snakes, unsigned int seed) {
	// Checks if provided board is NULL, provided size is invalid, or provided maximum snakes is invalid
	if (board == NULL || size < BOARD_SIZE_MIN || size > BOARD_SIZE_MAX || max_snakes < MAX_PLAYERS_MIN || max_snakes > MAX_PLAYERS_MAX) {
		return -1;
	}

	// Allocates the cells array and checks if allocation failed
	int total_cells = size * size;
	cell_t *board_cells = malloc(sizeof(cell_t) * total_cells);
	if (board_cells == NULL) {
		return -1;
	}

	// Sets fields of game board with provided arguments
	board->size = size;
	board->cells = board_cells;
	board->max_snakes = max_snakes;

	// Iterates through all cells and sets it to CELL_EMPTY
	for (int i = 0; i < total_cells; i++) {
		board->cells[i] = CELL_EMPTY;
	}

	// Initializes all snakes to a default state
	for (int i = 0; i < MAX_PLAYERS; i++) {
		board->snakes[i].alive = 0;
		board->snakes[i].length = 0;
	}
	
	// Sets number of snakes field to 0 and stores random seed in rng_state
	board->num_snakes = 0;
	board->rng_state = seed;

	// Places walls around the border of the board
	for (int y = 0; y < size; y++) {
		for (int x = 0; x < size; x++) {
			if (x == 0 || x == (size - 1) || y == 0 || y == (size - 1)) {
				board->cells[y * size + x] = CELL_WALL;
			}
		}	
	}

	// Places first apple and checks if it failed
	if (board_place_apple(board) == -1) {
		board_free(board);
		return -1;
	}

	return 0;
}

void board_free(game_board_t *board) { 
	// Checks if provided board is NULL
	if (board == NULL) {
		return ; 
	}

	// Checks if pointer to cells array is not NULL and then frees dynamically allocated cells array and sets pointer to NULL
	if (board->cells !=  NULL) {
		free(board->cells);
		board->cells = NULL;
	}
}

unsigned int board_random(game_board_t *board) {
	board->rng_state = board->rng_state * 1103515245 + 12345;
	return (board->rng_state / 65536) % 32768;
}

int board_place_apple(game_board_t *board) {
	// Checks if provided board is NULL or if pointer to cells array is NULL
	if (board == NULL || board->cells == NULL) {
		return -1;
	}

	// Gets the size of the board and calculates the total number of cells
	int board_size = board->size;
	int total_cells = board_size * board_size;
	int empty_count = 0;

	// Iterates through all cells in the board and counts total number of empty cells
	for (int i = 0; i < total_cells; i++) {
		if (board->cells[i] == CELL_EMPTY) {
			empty_count += 1;
		}
	}

	// Checks if there are no empty cells
	if (empty_count == 0) {
		return -1;
	}

	// Gets a random number and computes target index
	unsigned int random_value = board_random(board);
	int target_index = random_value % empty_count;

	// Iterates through the cells array
	int count = 0;
	for (int y = 0; y < board_size; y++) {
		for (int x = 0; x < board_size; x++) {
			// Checks if current cell is an empty cell
			if (board->cells[y * board_size + x] == CELL_EMPTY) {
				// Checks if target index reached
				if (count == target_index) {
					// Place CELL_APPLE at cell and store position
					board->cells[y * board_size + x] = CELL_APPLE;
					board->apple.x = x;
					board->apple.y = y;
					return 0;
				// If target index not reached, increment count of empty cells reached by one
				} else {
					count += 1;
				}
			}
		}
	}
	return -1;
}

int board_add_snake(game_board_t *board, int *out_id) {
	// Checks if provided board or pointer to integer is NULL
	if (board == NULL || out_id == NULL) {
		return -1;
	}

	// Finds first snake slot that is dead
	bool dead_snake = false;
	int snake_id;
	for (int i = 0; i < board->max_snakes; i++) {
		if (board->snakes[i].alive == 0) {
			dead_snake = true;
			snake_id = i;
			break;
		}
	}

	// Checks if no snake slot is dead
	if (dead_snake == false) {
		return -1;
	}

	// Calculates starting position based on the snake's id
	int x, y;
	if ((snake_id % 4) == 0) {
		x = board->size / 4;
		y = board->size / 4;
	} else if ((snake_id % 4) == 1) {
		x = (3 * board->size) / 4;
		y = board->size / 4;
	} else if ((snake_id % 4) == 2) {
		x = board->size / 4;
		y = (3 * board->size) / 4;
	} else if ((snake_id % 4) == 3) {
		x = (3 * board->size) / 4;
		y = (3 * board->size) / 4;
	}

	// Checks if starting cell is empty
	if (board->cells[y * board->size + x] == CELL_EMPTY) {
		snake_t *snake = &board->snakes[snake_id];
		// Initializes snake 
		snake->id = snake_id;
		snake->body[0].x = x;
		snake->body[0].y = y;
		snake->length = 1;
		snake->direction = DIR_RIGHT;
		snake->next_direction = DIR_RIGHT;
		snake->alive = 1;
		// Sets cell at starting position
		board->cells[y * board->size + x] = CELL_SNAKE_0 + snake->id;
		// Stores assigned id in *out_id and increments number of snakes
		*out_id = snake->id;
		board->num_snakes += 1;
		return 0;
	// Checks if starting cell is not empty
	} else {
		int total_available_cells = (board->size - 2) * (board->size - 2);
		int cell_count = 0;
		int y_pos = y;
		int x_pos = x;

		// Iterates through all cells that are not walls
		while (cell_count < total_available_cells) {
			// Checks if cell is the starting cell
			if (cell_count == 0) {
				// Checks if incrementing column will hit a wall and if so, loops back to first column
				if ((x_pos + 1) > (board->size - 2)) {
					x_pos = 1;
					// Checks if incrementing row will hit a wall and if so, loops back to first row
					if ((y_pos + 1) > (board->size - 2)) {
						y_pos = 1;
					} else {
						y_pos += 1;
					}
				} else {
					x_pos += 1;
				}
				cell_count += 1;
				continue;
			// Checks if cell is not the starting cell
			} else {
				// Checks if cell is empty
				if (board->cells[y_pos * board->size + x_pos] == CELL_EMPTY) {
					snake_t *snake = &board->snakes[snake_id];
					// Initializes snake
					snake->id = snake_id;
					snake->body[0].x = x_pos;
					snake->body[0].y = y_pos;
					snake->length = 1;
					snake->direction = DIR_RIGHT;
					snake->next_direction = DIR_RIGHT;
					snake->alive = 1;
					// Sets cell at starting position
					board->cells[y_pos * board->size + x_pos] = CELL_SNAKE_0 + snake->id;
					// Stores assigned id in *out_id and increments number of snakes
					*out_id = snake->id;
					board->num_snakes += 1;
					return 0;
				// Checks if cell is not empty
				} else {
					// Checks if incrementing column will hit a wall and if so, loops back to first column
					if ((x_pos + 1) > (board->size - 2)) {
						x_pos = 1;
						// Checks if incrementing row will hit a wall and if so, loops back to first row
						if ((y_pos + 1) > (board->size - 2)) {
							y_pos = 1;
						} else {
							y_pos += 1;
						}
					} else {
						x_pos += 1;
					}
					cell_count += 1;
				}
			}
		}
	}
	// No empty cells
	return -1;
}

int board_remove_snake(game_board_t *board, int snake_id) {
	// Checks if provided board is NULL or if provided id is invalid
	if (board == NULL || snake_id < 0 || snake_id >= MAX_PLAYERS) {
		return -1;
	}

	// Checks if snake is not alive
	if (board->snakes[snake_id].alive == 0) {
		return -1;
	}

	// Iterates through all cells occupied by the snake
	for (int i = 0; i < board->snakes[snake_id].length; i++) {
		snake_t *snake = &board->snakes[snake_id];
		// Gets the cell position and sets the cell to empty
		int x_pos = snake->body[i].x;
		int y_pos = snake->body[i].y;
		board->cells[y_pos * board->size + x_pos] = CELL_EMPTY;
	}

	// Sets alive to 0, length to 0, and decrements number of snakes by one
	board->snakes[snake_id].alive = 0;
	board->snakes[snake_id].length = 0;
	board->num_snakes -= 1;
	return 0;
}

int board_tick(game_board_t *board) {
	// Checks if provided board is NULL
	if (board == NULL) {
		return -1;
	}

	// Iterates through all snakes in order of id
	for (int i = 0; i < MAX_PLAYERS; i++) {
		snake_t *snake = &board->snakes[i];
		// Checks if snake is alive and if so, advances snake
		if (snake->alive == 1) {
			snake_advance(board, i);
		}
	}

	return 0;
}
