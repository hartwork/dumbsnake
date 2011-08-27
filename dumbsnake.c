/*
** Copyright (C) Sebastian Pipping <sebastian@pipping.org>
** Licensed under GPL v3 or later
**
** 2011-08-27 23:31 UTC+2
*/

#include <ncurses.h>
#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>

#define DS_MIN(a, b)  ((a) < (b) ? (a) : (b))


/* Config */
#define DS_DELAY_MILLIS     50
#define DS_FULL_GROWN_LEN   15

#define DS_CHAR_SNAKE_BODY  'X'
#define DS_CHAR_FLOOR       ' '

#define DS_CLOCK_ID         CLOCK_PROCESS_CPUTIME_ID


typedef struct _board_t {
	char * text;
	char ** array;
	size_t height;
	size_t width;
} board_t;

typedef struct _pos_t {
	size_t x;
	size_t y;
} pos_t;

typedef struct _snake_part_t {
	pos_t pos;
	struct _snake_part_t * prev;
	struct _snake_part_t * next;
} snake_part_t;

typedef struct _snake_t {
	size_t len;
	snake_part_t * head;
	snake_part_t * tail;
} snake_t;


static void
ds_curses_start() {
	initscr();
	cbreak();
	noecho();
	keypad(stdscr, TRUE);
	nodelay(stdscr, TRUE);

	refresh();
}

static void
ds_curses_stop() {
	endwin();
}

static board_t *
ds_board_create(size_t width, size_t height) {
	assert(width > 0);
	assert(height > 0);

	board_t * board = malloc(sizeof(board_t));
	assert(board);

	board->height = height;
	board->width = width;

	/* Allow interfacing a single flat string as a true 2D array */
	const size_t len = width * height;
	board->text = malloc(len + 1);
	assert(board->text);
	board->text[len] = '\0';

	board->array = malloc(sizeof(char *) * height);
	assert(board->array);
	size_t x;
	size_t y = 0;
	for (; y < height; y++) {
		board->array[y] = board->text + y * width;
		for (x = 0; x < width; x++) {
			board->array[y][x] = DS_CHAR_FLOOR;
		}
	}

	/* Print help right into the board */
	const char * const help = "Press 'q' to quit, 'p' to pause.";
	strncpy(board->text, help, DS_MIN(strlen(help), len));

	return board;
}

static void
ds_board_destroy(board_t * board) {
	free(board->text);
	free(board->array);
	free(board);
}

static void
ds_board_print(board_t * board) {
	erase();
	printw("%s", board->text);
	refresh();
}

static snake_t *
ds_snake_create(int x, int y) {
	snake_t * const snake = malloc(sizeof(snake_t));
	assert(snake);

	snake_part_t * const part = malloc(sizeof(snake_part_t));
	assert(part);
	part->pos.x = x;
	part->pos.y = y;
	part->prev = NULL;
	part->next = NULL;

	snake->len = 1;
	snake->head = part;
	snake->tail = part;

	return snake;
}

static void
ds_snake_destroy(snake_t * snake) {
	snake_part_t * part = snake->head;
	snake_part_t * next_backup;
	for (; part; part = next_backup) {
		next_backup = part->next;
		free(part);
	}
	free(snake);
}

static void
ds_board_put_snake(board_t * board, const snake_t * snake) {
	const snake_part_t * part = snake->head;
	for (; part; part = part->next) {
		board->array[part->pos.y][part->pos.x] = DS_CHAR_SNAKE_BODY;
	}
}

static void
ds_snake_move(snake_t * snake, board_t * board, int dx, int dy) {
	assert(snake);
	assert(board);
	assert((dx == 0) || (dx == +1) || (dx == -1));
	assert((dy == 0) || (dy == +1) || (dy == -1));
	assert((dx == 0) != (dy == 0));

	/* Add new head */
	snake_part_t * part = malloc(sizeof(snake_part_t));
	assert(part);
	part->pos.x = (snake->head->pos.x + dx + board->width) % board->width;
	part->pos.y = (snake->head->pos.y + dy + board->height) % board->height;
	part->prev = NULL;
	part->next = snake->head;

	board->array[part->pos.y][part->pos.x] = DS_CHAR_SNAKE_BODY;

	snake->head->prev = part;
	snake->head = part;
	snake->len += 1;

	/* Cut off tail */
	if (snake->len > DS_FULL_GROWN_LEN) {
		snake_part_t * const tail_backup = snake->tail;
		snake->tail->prev->next = NULL;
		snake->tail = snake->tail->prev;
		board->array[tail_backup->pos.y][tail_backup->pos.x] = DS_CHAR_FLOOR;
		free(tail_backup);
		snake->len -= 1;
	}
}

static long
ds_nano_diff(const struct timespec * before, const struct timespec * after) {
	long nano_before = before->tv_nsec + 1000*1000*1000 * before->tv_sec;
	long nano_after = after->tv_nsec + 1000*1000*1000 * after->tv_sec;
	return nano_after - nano_before;
}

static void
game() {
	int prev_COLS = COLS;
	int prev_LINES = LINES;
	
	/* Init game data */
	board_t * board = ds_board_create(COLS, LINES);
	snake_t * snake = ds_snake_create(COLS / 2, LINES / 2);
	ds_board_put_snake(board, snake);
	int dx = 0;
	int dy = -1;

	/* Init game rythm */
	struct timespec delay;
	struct timespec timestamp;
	struct timespec prev_timestamp;
	clock_gettime(DS_CLOCK_ID, &timestamp);
	prev_timestamp = timestamp;

	/* Main loop */
	int key;
	int prev_key;
	bool quit = false;
	bool paused = false;
	while (! quit) {
		/* Reset on window size change */
		if ((COLS != prev_COLS) || (prev_LINES != LINES)) {
			ds_board_destroy(board);
			board = ds_board_create(COLS, LINES);
			
			ds_snake_destroy(snake);
			snake = ds_snake_create(COLS / 2, LINES / 2);
			
			prev_COLS = COLS;
			prev_LINES = LINES;
		}
		
		/* Do a single step */
		if (! paused)
			ds_snake_move(snake, board, dx, dy);
		ds_board_print(board);

		/* Handle keys */
		prev_key = -1;
		while (! quit) {
			key = getch();
			if (key == -1) {
				break;
			}

			/* Each key only once (if hold or repeated) */
			if (key == prev_key) {
				continue;
			}
			prev_key = key;

			switch (key) {
			case 'q':
				quit = true;
				break;
			case 'p':
				paused = !paused;
				break;
			case KEY_LEFT:
				if (dx == 0) {
					dx = -1;
					dy = 0;
				}
				break;
			case KEY_RIGHT:
				if (dx == 0) {
					dx = +1;
					dy = 0;
				}
				break;
			case KEY_UP:
				if (dy == 0) {
					dx = 0;
					dy = -1;
				}
				break;
			case KEY_DOWN:
				if (dy == 0) {
					dx = 0;
					dy = +1;
				}
				break;
			}
		}

		/* Apply rythm */
		clock_gettime(DS_CLOCK_ID, &timestamp);
		const long nanosecs_passed = ds_nano_diff(&prev_timestamp, &timestamp);
		const long nanos_to_sleep = DS_DELAY_MILLIS*1000*1000 - nanosecs_passed;
		if (nanos_to_sleep > 0) {
			delay.tv_sec = nanos_to_sleep / (1000*1000*1000);
			delay.tv_nsec = nanos_to_sleep - 1000*1000*1000 * delay.tv_sec;
			nanosleep(&delay, NULL);
		}
		prev_timestamp = timestamp;
	}

	ds_board_destroy(board);
	ds_snake_destroy(snake);
}

int
main() {
	/* Init curses */
	const int atexit_error = atexit(ds_curses_stop);
	if (atexit_error) {
		return atexit_error;
	}

	ds_curses_start();
	assert(COLS > 0);
	assert(LINES > 0);

	game();
	
	return 0;
}
