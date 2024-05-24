/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Snakes tool implementation
 *  Copyright (C) 2001 Christopher Bazley
 */

#ifndef Snakes_h
#define Snakes_h

#include <stdio.h>
#include <stdbool.h>

#include "SFError.h"
#include "MapCoord.h"

typedef struct Snakes Snakes;

struct SnakeContext;
typedef void SnakesWriteFunction(MapPoint, size_t, struct SnakeContext *);
typedef size_t SnakesReadFunction(MapPoint, struct SnakeContext *);

typedef enum {
  DIR_NORTH, DIR_EAST, DIR_SOUTH, DIR_WEST, DIR_START
} Direction;

typedef struct SnakeContext {
  Snakes *snakes_data;
  MapPoint map_pos;
  size_t snake;
  unsigned int default_piece;
  Direction major_direct;
  SnakesReadFunction *read;
  SnakesWriteFunction *write;
} SnakeContext;

void Snakes_init(Snakes *snakes_data);

size_t Snakes_get_count(const Snakes *snakes_data);

void Snakes_get_name(const Snakes *snakes_data, size_t snake,
  char *snake_name, size_t n);

bool Snakes_has_junctions(const Snakes *snakes_data, size_t snake);
bool Snakes_has_bends(const Snakes *snakes_data, size_t snake);

enum {
  ErrBufferSize = 64,
};

SFError Snakes_load(FILE *const file, Snakes *const snakes_data,
  size_t const nobj, char *const err_buf);

size_t Snakes_begin_line(SnakeContext *ctx,
  Snakes *snakes_data, MapPoint map_pos, size_t snake,
  bool inside, SnakesReadFunction *read,
  SnakesWriteFunction *write);

void Snakes_plot_line(SnakeContext *ctx, MapPoint end);

void Snakes_free(Snakes *snakes_data);

#endif
