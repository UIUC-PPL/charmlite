# CharmLite

Status:
- Requires that `${env:CHARM_HOME}` points to a valid Charm++ build.
    - The lightest possible build is:
    `./build AMPI-only <triplet> <compiler>? --with-production -DCSD_NO_IDLE_TRACING=1 -DCSD_NO_PERIODIC=1`
- Only tested with non-SMP builds, SMP builds currently crash:
    - This can be fixed by correctly isolating globals as Csv/Cpv.
- Minimal support for collective communication:
    - Broadcasts only work for groups.
        - Plan to use Hypercomm distributed tree creation scheme for chare-arrays:
            - [Google doc write-up.](https://docs.google.com/document/d/1hv-9qm1dXR8R1VJXgtyFHuhTUoa_izrm-jDXPqqkpas/edit?usp=sharing)
            - [Hypercomm implementation.](https://github.com/jszaday/hypercomm/blob/main/include/hypercomm/tree_builder/tree_builder.hpp)
    - Reductions currently rely on Converse:
        - This means contributions can't be made out-of-order!
