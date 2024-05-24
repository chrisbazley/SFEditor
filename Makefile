# Project:   SFEditor

# Toolflags:
CCCommonFlags = -c -depend !Depend -IC: -throwback -W -fahi -apcs 3/32/fpe2/swst/fp/nofpr -memaccess -L22-S22-L41 -DPER_VIEW_SELECT=1
CCflags = $(CCCommonFlags) -DNDEBUG -Otime
CCDebugFlags = $(CCCommonFlags) -DDEBUG_OUTPUT -DFORTIFY -g
C++flags = -c -depend !Depend -IC: -throwback
Linkflags = -aif -o $@ 
LinkDebugFlags = -d $(Linkflags)
ObjAsmflags = -throwback -NoCache -depend !Depend
CMHGflags = 
LibFileflags = -c -o $@
Squeezeflags = -v -o $@
ASMflags =

include MakeCommon

DebugList = $(addprefix debug.,$(ObjectList))
ReleaseList = $(addprefix o.,$(ObjectList))
LibList = C:o.toolboxlib C:o.eventlib C:o.wimplib
DebugLibs = C:debug.CBLib C:debug.CBOSLib C:debug.CBUtilLib C:debug.StreamLib C:o.GKeyLib C:o.CBDebugLib $(LibList) Fortify:o.fortify C:o.stubs
ReleaseLibs = C:o.flexlib C:o.CBLib C:o.CBOSLib C:o.CBUtilLib C:o.StreamLib C:o.GKeyLib $(LibList) C:o.stubsG
DebugObjects = $(DebugList) $(DebugLibs)
ReleaseObjects = $(ReleaseList) C:o.ErrNotRec $(ReleaseLibs)

# Final targets:
all: @.!RunImageD @.!RunImage

@.!RunImage: $(ReleaseObjects)
        link $(LinkFlags) $(ReleaseObjects)

@.!RunImageD: $(DebugObjects)
        link $(LinkDebugFlags) $(DebugObjects)

# User-editable dependencies:
.SUFFIXES: .o .c .debug .s
.c.debug:; cc $(CCDebugFlags) -o $@ $<
.c.o:; cc $(CCFlags) -o $@ $<
.s.debug:; objasm $(ObjAsmFlags) -from $< -to $@
.s.o:; objasm $(ObjAsmFlags) -from $< -to $@

# Static dependencies:

# Dynamic dependencies:
