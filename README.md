# CharmLite

Status:
- Requires that `${env:CHARM_HOME}` points to a valid Charm++ build.
    - The lightest possible build is:
    `./build AMPI-only <triplet> <compiler>? --with-production -DCSD_NO_IDLE_TRACING=1 -DCSD_NO_PERIODIC=1`
    - Binaries should be compiled with `-fno-exceptions -fno-unwind-tables` to further minimize overheads.
- Only tested with non-SMP builds, SMP builds ~~currently crash~~:
    - ~~This can be fixed by correctly isolating globals as Csv/Cpv.~~
    - Probably fixed but needs more testing.
- Uses Hypercomm distributed tree creation scheme for chare-array collectives:
    - [Google doc write-up.](https://docs.google.com/document/d/1hv-9qm1dXR8R1VJXgtyFHuhTUoa_izrm-jDXPqqkpas/edit?usp=sharing)
    - [Hypercomm implementation.](https://github.com/jszaday/hypercomm/blob/main/include/hypercomm/tree_builder/tree_builder.hpp)
- Add support for "location records" that indicate migratibility.
    - How should users specify whether elements can/not migrate?

Overall... need more examples; feel free to _try_ porting your favorite example. Agenda:
- Jacobi2d
