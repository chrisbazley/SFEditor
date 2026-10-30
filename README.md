# SFEditor
A RISC OS desktop map/mission editor for the futuristic aerial combat game 'Star Fighter 3000'

The CMake option `PER_VIEW_SELECT` controls whether each editing window has
its own selection (`ON`, the default) or views of a file share one selection
(`OFF`, retained for legacy configuration testing). Per-view selection follows
the RISC OS selection model described in Acorn Application Note 240.
Both native makefiles also enable per-view selection. With GMakefile, use
`make -f GMakefile PER_VIEW_SELECT=0` to select the legacy mode; with CMake,
use `-DPER_VIEW_SELECT=OFF`. CI compiles both modes, including with the
_Optional checker. Host builds do not run SFEditor's user interface.
