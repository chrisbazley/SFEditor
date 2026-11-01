/* SFEditor test: test suite definitions
 * Copyright (C) 2026 Christopher Bazley
 */
#ifndef Tests_h
#define Tests_h

void FPerf_tests(void);
void BPerf_tests(void);
void CoarseCoord_tests(void);
void Filenames_tests(void);
void Pyram_tests(void);
void Clouds_tests(void);
void Player_tests(void);
void Defenc_tests(void);
void DataUtils_tests(void);
void Paths_tests(void);
void Briefing_tests(void);
void Text_tests(void);
void Map_tests(void);
void Obj_tests(void);
void Vertex_tests(void);
void Shapes_tests(void);
void MapCoord_tests(void);
void MapAreaCol_tests(void);
void SelBitmask_tests(void);
void MapEditSel_tests(void);
void ObjEditSel_tests(void);
void LoadGameData_tests(void);

#ifdef FORTIFY
#include "fortify.h"
#else
#define Fortify_EnterScope()
#define Fortify_LeaveScope()
#define Fortify_OutputStatistics()
#endif

#endif /* Tests_h */
