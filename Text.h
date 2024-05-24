/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Text string read/write
 *  Copyright (C) 2020 Christopher Bazley
 */

#ifndef Text_h
#define Text_h

#include <stdint.h>

struct Reader;
struct Writer;

#include "SFError.h"

enum {
  Text_ScreenWidth = 256, // Don't let text spill into the right margin
  Text_ScreenHeight = 256,
  Text_CharWidth = 4,
  Text_CharHeight = 6,
  Text_NumColumns = Text_ScreenWidth / Text_CharWidth,
  Text_NumRows = Text_ScreenHeight / Text_CharHeight,
  Text_NoYClip = 255,
};

typedef enum {
  CursorType_None,
  CursorType_Line,
  CursorType_Block,
  CursorType_Count,
} CursorType;

typedef struct {
  int32_t duration;
  int32_t delay;
  int32_t speed;
  int32_t y_pos;
  unsigned char x_pos;
  unsigned char y_clip;
  unsigned char colour;
  bool repeat;
  CursorType cursor_type;
} TextParams;

typedef struct Text Text;

void text_init(Text *text);
void text_destroy(Text *text);

char const *text_get_string(Text const *text);
SFError text_set_string(Text *text, char const *string);

void text_set_params(Text *text, TextParams const *params);
void text_get_params(Text const *text, TextParams *params);

void text_write_offset(Text const *text, struct Writer *writer, int *offset);
void text_write_block(Text const *text, struct Writer *writer);
SFError text_read_block(Text *text, struct Reader *reader);

#endif
