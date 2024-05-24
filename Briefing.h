/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Mission briefing read/write
 *  Copyright (C) 2020 Christopher Bazley
 */

#ifndef Briefing_h
#define Briefing_h

#include "SFError.h"

enum {
  BriefingMin = 1, // 1st is title
  BriefingMax = 7, // based on existing missions
};

struct Reader;
struct Writer;

typedef struct BriefingData BriefingData;

void briefing_init(BriefingData *briefing);

void briefing_destroy(BriefingData *briefing);

int briefing_write_text_offsets(BriefingData *briefing, struct Writer *writer,
  int offset);

void briefing_write_texts(BriefingData *briefing, struct Writer *writer);

SFError briefing_read_texts(BriefingData *briefing,
                            long int const *offsets, size_t count,
                            struct Reader *reader);

size_t briefing_get_text_count(BriefingData const *briefing);
char const *briefing_get_text(BriefingData const *briefing, size_t n);
SFError briefing_add_text(BriefingData *briefing, char const *string);

char const *briefing_get_title(BriefingData const *briefing);

#endif
