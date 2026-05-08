#include "protocol.h"
#include "debug.h"
#include "game_board.h"
#include <string.h>
#include <arpa/inet.h>

int protocol_serialize_welcome(uint8_t *buf, size_t buf_len, int player_id, int board_size, int max_players) {
	// Checks if buffer is NULL or if buffer is too small
	if (buf == NULL || buf_len < 4) {
		return -1;
	}

	// Checks for invalid values
	if (player_id < 0 || player_id >= MAX_PLAYERS_MAX || board_size < BOARD_SIZE_MIN || board_size > BOARD_SIZE_MAX || max_players < MAX_PLAYERS_MIN || max_players > MAX_PLAYERS_MAX) {
		return -1;
	}

	// Writes fields to bytes
	buf[0] = (uint8_t) MSG_WELCOME;
	buf[1] = (uint8_t) player_id;
	buf[2] = (uint8_t) board_size;
	buf[3] = (uint8_t) max_players;

	return 4;
}

int protocol_serialize_game_state(uint8_t *buf, size_t buf_len, const game_board_t *board) {
	// Checks if buffer is NULL, buffer is too small, or if board is NULL
	if (buf == NULL || buf_len < 6 || board == NULL) {
		return -1;
	}

	size_t current_len = 6;
	uint8_t alive_snakes_count = 0;
	// Iterates through all snakes and computes number of alive snakes
	for (int i = 0; i < MAX_PLAYERS_MAX; i++) {
		if (board->snakes[i].alive == 1) {
			alive_snakes_count += 1;
		}
	}

	// Converts from host byte order to network byte order
	uint16_t apple_x_pos = htons(board->apple.x);
	uint16_t apple_y_pos = htons(board->apple.y);

	// Writes fields to bytes
	buf[0] = (uint8_t) MSG_GAME_STATE;
	buf[1] = alive_snakes_count;
	memcpy(buf + 2, &apple_x_pos, 2);
	memcpy(buf + 4, &apple_y_pos, 2);

	// Iterates through all snakes
	for (int i = 0; i < MAX_PLAYERS_MAX; i++) {
		// Checks if snake is alive
		if (board->snakes[i].alive == 1) {
			if (buf_len < (current_len + 4)) {
				return -1;
			}
			// Converts from host byte order to network byte order
			uint16_t snake_length = htons(board->snakes[i].length);
			// Writes fields to bytes
			buf[current_len] = (uint8_t) board->snakes[i].id;
			memcpy(buf + (current_len + 1), &snake_length, 2);
			buf[current_len + 3] = (uint8_t) board->snakes[i].direction;
			current_len += 4;

			// Iterates through each cell of snake
			for (int j = 0; j < board->snakes[i].length; j++) {
				// Converts from host byte order to network byte order
				uint16_t x_pos = htons(board->snakes[i].body[j].x);
				uint16_t y_pos = htons(board->snakes[i].body[j].y);

				if (buf_len < (current_len + 4)) {
					return -1;
				}
				// Writes fields to bytes
				memcpy(buf + (current_len), &x_pos, 2);
				memcpy(buf + (current_len + 2), &y_pos, 2);
				current_len += 4;
			}
		}
	}
	return (int)current_len;
}

int protocol_serialize_dead(uint8_t *buf, size_t buf_len, int player_id) {
	// Checks if buffer is NULL or if buffer is too small
	if (buf == NULL || buf_len < 2) {
		return -1;
	}

	// Checks for invalid values
	if (player_id < 0 || player_id >= MAX_PLAYERS_MAX) {
		return -1;
	} 

	// Writes fields to bytes
	buf[0] = (uint8_t) MSG_PLAYER_DEAD;
	buf[1] = (uint8_t) player_id;

	return 2;
}

int protocol_serialize_game_over(uint8_t *buf, size_t buf_len, int winner_id) {
	// Checks if buffer is NULL or if buffer is too small
	if (buf == NULL || buf_len < 2) {
		return -1;
	}

	// Checks for invalid values
	if ((winner_id < 0 || winner_id >= MAX_PLAYERS_MAX) && winner_id != 0xFF) {
		return -1;
	} 

	// Writes fields to bytes
	buf[0] = (uint8_t) MSG_GAME_OVER;
	buf[1] = (uint8_t) winner_id;

	return 2;
}


int protocol_serialize_error(uint8_t *buf, size_t buf_len, uint8_t error_code) {
	// Checks if buffer is NULL or if buffer is too small
	if (buf == NULL || buf_len < 2) {
		return -1;
	}

	// Checks for invalid error codes
	if ((error_code != ERR_GAME_FULL) && (error_code != ERR_ALREADY_JOINED) && (error_code != ERR_INVALID_MSG)) {
		return -1;
	}

	// Writes fields to bytes
	buf[0] = (uint8_t) MSG_ERROR;
	buf[1] = (uint8_t) error_code;

	return 2;
}

int protocol_deserialize_client_msg(const uint8_t *buf, size_t buf_len, uint8_t *out_type, uint8_t *out_payload) {
	// Checks if buffer is NULL, buffer is too small, out_type is NULL, or out_payload is NULL
	if (buf == NULL || buf_len < 2 || out_type == NULL || out_payload == NULL) {
		return -1;
	}

	// Parses the client message
	uint8_t message_type = buf[0];
	uint8_t message_payload = buf[1];

	// Checks if message type is unknown
	if ((message_type != MSG_JOIN) && (message_type != MSG_DIRECTION) && (message_type != MSG_LEAVE)) {
		return -1;
	}

	// Checks if message type is join or leave
	if ((message_type == MSG_JOIN) || (message_type == MSG_LEAVE)) {
		// Checks if payload is 0x00
		if (message_payload != 0x00) {
			return -1;
		}
	// Checks if message type is direction
	} else if (message_type == MSG_DIRECTION) {
		// Checks if payload is a valid direction
		if ((message_payload != DIR_UP) && (message_payload != DIR_DOWN) && (message_payload != DIR_LEFT) && (message_payload != DIR_RIGHT)) {
			return -1;
		}
	}

	// Stores the message type and payload
	*out_type = message_type;
	*out_payload = message_payload;

	return 0;
}
