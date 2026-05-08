#include "snake.h"
#include "game_board.h"
#include "server.h"

int snake_set_direction(snake_t *snake, direction_t dir) {
	// Checks if provided pointer to snake is NULL
	if (snake == NULL) {
		return -1;
	}

	// Checks if requested direction is an invalid value
	if ((dir != DIR_UP) && (dir != DIR_DOWN) && (dir != DIR_LEFT) && (dir != DIR_RIGHT)) {
		return -1;
	}

	// Checks if requested direction is opposite to current direction and if so, does nothing, else sets next direction to requested direction
	if (dir == DIR_UP) {
		if (snake->direction == DIR_DOWN) {
			return 0;
		} else {
			snake->next_direction = DIR_UP;
		}
	} else if (dir == DIR_DOWN) {
		if (snake->direction == DIR_UP) {
			return 0;
		} else {
			snake->next_direction = DIR_DOWN;
		}
	} else if (dir == DIR_LEFT) {
		if (snake->direction == DIR_RIGHT) {
			return 0;
		} else {
			snake->next_direction = DIR_LEFT;
		}
	} else if (dir == DIR_RIGHT) {
		if (snake->direction == DIR_LEFT) {
			return 0;
		} else {
			snake->next_direction = DIR_RIGHT;
		}
	}
	return 0;
}

int snake_advance(struct game_board *board, int snake_id) {
	// Checks if provided board is NULL or if provided id is invalid
	if (board == NULL || snake_id < 0 || snake_id >= MAX_PLAYERS) {
		return -1;
	}

	// Checks if snake is not alive
	if (board->snakes[snake_id].alive == 0) {
		return -1;
	}

	// Copies next direction into direction
	board->snakes[snake_id].direction = board->snakes[snake_id].next_direction;

	snake_t *snake = &board->snakes[snake_id];
	int head_x = snake->body[0].x;
	int head_y = snake->body[0].y;

	int new_x = head_x;
	int new_y = head_y;
	// Computes new head position
	if (snake->direction == DIR_UP) {
		new_y = head_y - 1;
	} else if (snake->direction == DIR_DOWN) {
		new_y = head_y + 1;
	} else if (snake->direction == DIR_LEFT) {
		new_x = head_x - 1;
	} else if (snake->direction == DIR_RIGHT) {
		new_x = head_x + 1;
	}

	cell_t new_head = board->cells[new_y * board->size + new_x];
	// Checks if new head position is a cell wall or another snake
	if (new_head == CELL_WALL || (new_head >= CELL_SNAKE_0 && new_head <= CELL_SNAKE_7)) {
		// Snake dies
		board_remove_snake(board, snake_id);
		return 1;
	// Checks if new head position is an apple
	} else if (new_head == CELL_APPLE) {
		// Checks if length of snake is less than max length
		if (snake->length < MAX_SNAKE_LENGTH) {
			// Moves body forward
			for (int i = snake->length; i > 0; i--) {
				snake->body[i] = snake->body[i - 1];
			}
			// Sets body[0] to new head
			snake->body[0].x = new_x;
			snake->body[0].y = new_y;
			// Sets cell to snake's cell value
			board->cells[new_y * board->size + new_x] = CELL_SNAKE_0 + snake_id;
			// Increments snake's length by 1 and spawns a new apple
			snake->length += 1;
			board_place_apple(board);
			return 2;
		// Checks if length of snake is greater than or equal to max length
		} else {
			// Gets tail cell and clears it to empty
			position_t tail = snake->body[snake->length - 1];
			int tail_x = tail.x;
			int tail_y = tail.y;
			board->cells[tail_y * board->size + tail_x] = CELL_EMPTY;
			// Shifts all body segments down by one
			for (int i = snake->length - 1; i > 0; i--) {
				snake->body[i] = snake->body[i - 1];
			}
			// Sets body[0] to new head 
			snake->body[0].x = new_x;
			snake->body[0].y = new_y;
			// Sets new head cell to snake's cell value and spawns a new apple
			board->cells[new_y * board->size + new_x] = CELL_SNAKE_0 + snake_id;
			board_place_apple(board);
			return 2;
		}
	// Checks if new head position is empty
	} else if (new_head == CELL_EMPTY) {
		// Gets tail cell and clears it to empty
		position_t tail = snake->body[snake->length - 1];
		int tail_x = tail.x;
		int tail_y = tail.y;
		board->cells[tail_y * board->size + tail_x] = CELL_EMPTY;
		// Shifts all body segments down by one
		for (int i = snake->length - 1; i > 0; i--) {
			snake->body[i] = snake->body[i - 1];
		}
		// Sets body[0] to new head
		snake->body[0].x = new_x;
		snake->body[0].y = new_y;
		// Sets new head cell to snake's cell value
		board->cells[new_y * board->size + new_x] = CELL_SNAKE_0 + snake_id;
		return 0;
	}
	return -1;
}