# Portable tests

`SFEditorTests` follows the SF3KUtils pattern: `Main.c`, `Tests.h`,
`test_groups`, `unit_tests`, ordinary `assert` checks and Fortify scopes.
One invocation runs the tile selection, object selection and game-data loading
suites. Assertions remain enabled in Release builds.

After configuring and building normally, run:

```
ctest --test-dir build --output-on-failure -R '^SFEditorTests$'
```

The executable needs only a `LandScapes` directory containing the game data in
its working directory. Resource names and subpaths are hardwired in the loading
suite. It takes no arguments and one invocation runs every test group:

```
/path/to/SFEditorTests
```

CTest's CMake wrapper downloads and verifies `sf3000.zip` if necessary, extracts
it and invokes the executable once from the directory containing `LandScapes`.
This fixture preparation is separate from the C test program. For offline CTest
runs, configure with `-DSFEDITOR_GAME_ARCHIVE=/absolute/path/to/sf3000.zip`.
CI caches the verified archive between runs. Downloads use a temporary file
and up to three attempts, so failure cannot leave a partial cached archive.

For a multi-configuration generator, also specify `-C Debug` or `-C Release`.
`BUILD_TESTING=OFF` disables the test. Native RISC OS builds do not build the
host test executable.

## Selection-model tests

The suites check membership across the complete grid against explicit expected
sets, counts, bounds after deselection, empty and full selections, idempotent
operations, overlapping areas, inversion, coordinate wrapping, iteration and
redraw notifications. Map tests cover rectangle, line, circle and triangle
selection entry points. Object tests additionally cover independent copies,
callback preservation, and clipped change enumeration, including a disjoint
clipping rectangle.

## Game-data loading tests

The loading suite loads and destroys the supported resources from the original
`sf3000.zip` using the production parsers and Gordon Key decompressor, without
opening any windows. It shares the same executable and host facilities as the
selection suites.

## Inputs

| Resource | Files |
| --- | ---: |
| Base ground maps | 14 |
| Base object maps | 11 |
| Polygon graphics sets | 6 |
| Ground tile bitmap sets | 7 |
| Polygon colour tables | 5 |
| Hill colour tables | 6 |
| Missions (all three difficulty levels) | 108 |

The tests explicitly load every required file, so an empty directory or a
missing resource cannot silently reduce coverage. The complete mission set is cheap
to load; no hand-picked subset is needed. This covers all those archive files,
not every parser branch or malformed input.

Each load must succeed, release its model and reader, and close its input.
When Fortify is active, memory diagnostics, damaged blocks and changes in the
outstanding allocation total fail the test.

## Host facilities and limits

`HostSupport.c` provides in-memory mode-13 sprite storage for the tile loader,
no-op hourglass/budge operations, and fatal error-reporting entry points. It
contains no game-file parsing. Flex allocations use CBPseudoLib. Each tile
bitmap set is loaded with an all-black palette and an all-white palette; all
tile brightness flags must match the chosen palette.

The test target compiles the production model and loading source files,
without the separate UI/rendering modules. It does not test rendering,
shared-resource caching, editor metadata, or saving.

SFEditor currently treats sky colours and sky images as absent resources in
`Session.c:get_shared_file`: it does not parse the `Sky` or `Planets` files.
There is consequently no editor loader for these tests to exercise. Adding
separate sky/planet parsing would extend the application rather than test its
existing loading behaviour.

## CI diagnostics

Every CI job uploads its build log, verbose CTest log, JUnit results and
complete program stdout/stderr logs as a `diagnostics-*` artifact, including after
failure. Resource counts and outcomes appear in the job summary. Program output stays in the artifacts; a failure prints the last 30 lines of
each log. Every input filename is logged before loading it.

The GCC Debug job with per-view selection enabled also instruments the single
portable test target and uploads browsable HTML source coverage and Cobertura XML.
The report measures the compiled model, loader and selection modules, including
untested
editing, saving and error paths. Its overall percentage is not a measure of
parser completeness. Configure
`-DSFEDITOR_TEST_COVERAGE=ON` to reproduce instrumentation locally with GCC.
