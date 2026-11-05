Bosch Sensortec BMI270 SensorAPI v2.86.1
======================================

Upstream: https://github.com/boschsensortec/BMI270_SensorAPI

Release: `v2.86.1`, commit `d270cdee303b2ed9ea4c43fb7064da8ded8a48b4`.
Files obtained from the release on 2026-11-04: `bmi2.c`, `bmi2.h`,
`bmi2_defs.h`, `bmi270.c`, `bmi270.h`, and `LICENSE`.

The BSD-3-Clause license and upstream copyright notices are retained.
`bmi270.c` contains the official complete 8192-byte configuration image;
do not replace it with the distinct maximum-FIFO/context/legacy variants.

Local change: rename only the upstream function definition in `bmi270.c`
and declaration in `bmi270.h` from `bmi270_init` to `bosch_bmi270_init`.
This avoids a symbol collision with the project adapter in `../bmi270.c`.
All other source bytes, including upstream line endings, are unchanged.

Original upstream SHA-256 checksums (before the two-line symbol rename):

```text
c3f912bc3033b411fa8df67dbdf66f5b3d74cf203b2e8aa4a8c834821116c2b3  bmi2.c
81334388e9467c8dfefa439b0e2888a3fd93a4eb8de6eb6f62840a6e7e12bd50  bmi2.h
2e2474895d636bc46e1f735a24c380ed4e5356746caf928d0b9a50e6d8fbfbf3  bmi2_defs.h
f6446867378bbcc90ea97ef6593555de8767ca20b799355c1b37c2cabb7b7b41  bmi270.c
7979ec4c943e2825351bd9d3d267b63abc6f8314c1f13f1c37e95cabc3fc82c8  bmi270.h
56df116398e0b72a12ae353d93002d29175583590283ced2c2510e3d8075546f  LICENSE
```
