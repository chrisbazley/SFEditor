/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Desktop screen mode variables
 *  Copyright (C) 2019 Christopher Bazley
 */

#ifndef Desktop_h
#define Desktop_h

#include "Vertex.h"

void Desktop_init(void);
void Desktop_invalidate(void);
Vertex Desktop_get_size_px(void), Desktop_get_eigen_factors(void), Desktop_get_size_os(void);
void *Desktop_get_trans_table(void);
void Desktop_put_trans_table(void *trans_table);
int Desktop_get_screen_mode(void);

#endif
