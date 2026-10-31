# Portable tests

`SFEditorTests` follows the SF3KUtils pattern: `Main.c`, `Tests.h`,
`test_groups`, `unit_tests`, ordinary `assert` checks and Fortify scopes.
One invocation runs every unit suite and the game-data loading suite.
Assertions remain enabled in Release builds.

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

## Performance suites

FPerf and BPerf use arithmetic patterns to distinguish ship records and enum-based
comparison helpers to check unaffected fields. Cases cover setter boundaries,
round trips, corruption and truncation. BPerf also checks remote shields and
switching back to normal shields.

## Small model suites

Separate CoarseCoord, Filenames, Player, Defenc, Pyram and Clouds suites exercise
coordinate conversions, filename slots, equipment fields, defence parameters,
pyramid positions and cloud colours. Values are checked with arithmetic patterns
and explicit field expectations, without reference implementations. Player
coverage here concerns equipment and its main record; docking relationships
remain outside this suite.

## Text tests

Repeating alphabet and digit patterns check normalisation. Cases also cover
defaults, punctuation, unsupported characters, replacement, display parameters,
offset progression and round trips at varied string lengths. Fortify builds
exercise allocation failures and replacement of an existing string.

## Serialized input

Inputs come from production writers, and lengths come from stream positions.
The tests do not depend on encoded sizes, field offsets, padding, byte order or
character codes. Each prefix is tried when testing truncation; a successful read
must recover the complete expected model.

For filenames, player data, defences, pyramid positions, text and performance
records, every serialized byte is replaced with every possible byte value in
turn. Each suite requires all its expected public bad-data error codes to occur.
BPerf also corrupts successive suffixes to exercise conditions that require
several bytes to change together, including the legacy negative ship count.

Fixed buffer sizes are capacities, and pointer-to-array parameters preserve their
bounds. Read-only arrays use CBUtilLib's C23_CONST macro.

## Map and object model tests

Tests initialise base grids to zero and overlays to mask through model setters.
They then assign a coordinate-derived repeating pattern to three quarters of the
cells, leaving forbidden object rows clear. Validation calculates expected values
at each coordinate and checks that untouched cells retain zero or mask. Updates,
wrapping and mask restoration are also covered.

Base and overlay round trips use production DFile readers and writers with
memory streams. Every reloaded cell is checked against the pattern. Object tests
also check that loading clears forbidden rows while preserving the final row.
No additional input files are needed.

## Vertex and bounding-box tests

Vertex cases cover component-wise arithmetic, integer division truncation,
scalar and per-axis power-of-two scaling, negative exponents and equality.
Arithmetic inputs avoid signed overflow and left shifts of negative values.

Bounding-box cases cover corner accessors, validity, sizes, expansion from the
invalid sentinel, containment, translation and intersection. Upper bounds are
exclusive: sizes are `max - min`. Touching intersections have zero extent but
remain valid boxes; disjoint intersections are invalid. In-place translation
and aliased intersection outputs are also exercised.

## Shape tests

Simple rectangles, lines, circles and triangles use exact expected pixels.
Awkward strokes and triangles use a 1.25-pixel boundary band: pixels well inside
must be drawn, and distant pixels must remain untouched. Circle tests allow a
0.75-pixel band. Collinear triangles require bounded nonempty output without
prescribing perfect coverage. These checks accommodate the existing rasteriser's
imperfections rather than freezing every boundary pixel.

Flood fill is checked against an independent four-neighbour queue search for
holes, disconnected regions, narrow passages, branching regions and wrapping.
Fortify builds additionally exercise allocation failure and stack growth failure.

## Coordinate tests

Rectangle cases cover inclusive bounds, containment, intersection, in-place
transforms, signed rounding, rotation and iteration. Wrapped splitting is checked
against a small grid built by independently wrapping each input cell, including
seams, negative coordinates, full spans and oversized spans.

## Area-collection tests

Cases cover containment, overlap, cascading merges, capacity pressure and scaled
area costs. At capacity, tests check coverage and the size bound without relying
on tie-breaking between equally good merge candidates.

## Selection-model tests

Bitmask cases cover empty and full capacity, membership, callbacks, copying,
index changes after insertion and deletion, iteration, change enumeration, and
deleting or moving the current iterator item. Insertion and deletion are checked
at every valid position against a boolean-array reference.

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
shared-resource caching, editor metadata, or application save workflows.

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
