/* SFEditor: private polygonal object representation shared by loading and drawing. */
#ifndef ObjGfxMeshInternal_h
#define ObjGfxMeshInternal_h
#include "MapCoord.h"
#include "ObjVertex.h"
#include "ObjPolygon.h"

typedef enum
{
  CoordinateScale_Small,
  CoordinateScale_Medium,
  CoordinateScale_Large
}
CoordinateScale;

enum {
  PlotCommands_OperandMask = 0x1fu,
  PlotCommands_OperandShift = 0,
};

typedef struct {
  CoordinateScale scale;
  unsigned char coll_x, coll_y;
  unsigned char plot_type;
  unsigned short clip_size_x, clip_size_y;
  unsigned short score;
  long int clip_dist;
  MapArea bounding_box[MapAngle_Count];
  long int pal_dist;
} ObjMisc;

struct ObjGfxMesh {
  ObjMisc misc;
  ObjVertices varray;
  ObjPolygons polygons;
};

#endif
