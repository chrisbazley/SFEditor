/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Coarse coordinates type definition
 *  Copyright (C) 2020 Christopher Bazley
 */

#ifndef CoarseCoord_h
#define CoarseCoord_h

#include "Debug.h"
#include <stdbool.h>

struct Reader;
struct Writer;

typedef unsigned char CoarseCoord;
#define PRICoarseCoord "d"

typedef struct
{
  CoarseCoord x, y, z;
}
CoarsePoint3d;

typedef struct
{
  CoarseCoord x, y;
}
CoarsePoint2d;

typedef unsigned long int FineCoord;
#define PRIFineCoord "lu"

typedef struct
{
  FineCoord x, y, z;
}
FinePoint3d;

static inline CoarseCoord CoarseCoord_from_fine(FineCoord const coord)
{
  return (CoarseCoord)(coord / (1ul << 24));
}

CoarsePoint3d CoarsePoint3d_from_fine(FinePoint3d point);
FinePoint3d FinePoint3d_from_coarse(CoarsePoint3d point);

bool CoarsePoint3d_read(CoarsePoint3d *point, struct Reader *reader);
void CoarsePoint3d_write(CoarsePoint3d point, struct Writer *writer);

static inline FineCoord FineCoord_from_coarse(CoarseCoord const coord)
{
  return (FineCoord)coord * (1ul << 24);
}

bool FinePoint3d_read(FinePoint3d *point, struct Reader *reader);
void FinePoint3d_write(FinePoint3d point, struct Writer *writer);

bool CoarsePoint2d_read(CoarsePoint2d *point, struct Reader *reader);
void CoarsePoint2d_write(CoarsePoint2d point, struct Writer *writer);

static inline bool CoarsePoint2d_compare(CoarsePoint2d a, CoarsePoint2d b)
{
  bool const equal = a.x == b.x && a.y == b.y;
  DEBUGF("%d,%d %s %d,%d\n", a.x, a.y, equal ? "==" : "!=", b.x, b.y);
  return equal;
}

#endif
