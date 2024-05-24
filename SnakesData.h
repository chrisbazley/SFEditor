/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Snakes tool data
 *  Copyright (C) 2019 Chris Bazley
 */

#ifndef SnakesData_h
#define SnakesData_h

struct Snakes
{
  size_t count;
  void  *data_anchor; /* flex anchor for array of 32-byte
                         snake definition blocks, one for
                         each snake. */
};

#endif
