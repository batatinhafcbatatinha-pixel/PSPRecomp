# Third-party notices

Components taken from other projects, with source, licence and what was taken. Per-profile bundles (FFmpeg, SMAA,
HDR shaders, CloudWorks) are listed in `profiles/vcs/THIRD_PARTY.md` and sit beside the profile that uses them.

## ThirteenAG / WidescreenFixesPack

- Source: https://github.com/ThirteenAG/WidescreenFixesPack (plugin `GTAVCS.WidescreenFix`, the VCS Project2DFX
  sources and the "modern control scheme" behaviour).
- Taken: on 2026-08-12, in the `initial release` commit `37e5469` of the upstream PSPRecomp project (author
  Jessica_Natalia), before this fork existed. The git history of this repository shows nothing from this source added
  later, apart from fork copies of the same files into `profiles/ctw` (commits `dbf6f0e`, `ec905b8`, 2026-11-07).
  The follow-up commit `c792253` (2026-08-13, "widescreen fix crop") only edited a generated unit, which is removed.
- What: the widescreen frustum/HUD-rectangle correction idea and arithmetic (`profiles/*/host/ge_renderer.cpp`,
  `vcs_config.*`), the vehicle/camera input scheme and pad layout (`vcs_vehicle_input.*`, `vcs_camera_input.*`), the
  entity LOD multiplier hook (`vcs_draw_distance_patch.cpp`) and the VCS LOD-light table
  (`profiles/vcs/data/VCSProject2DFX_Lights.bin`, converted by `profiles/vcs/tools/import_project2dfx_lights.py`
  from the sources pinned at WidescreenFixesPack commit `37057d3883bd607ae047c9f031fe083c32b21c88`).
- Licence at the time it was taken: **MIT, Copyright (c) 2018 ThirteenAG**. Since 2026-11-08 (WidescreenFixesPack
  commit `0328eb3`, as reported by the project's licence file) the project is **GPL-3.0**. Material already taken
  under MIT stays under MIT; **nothing newer than that may be taken into this MIT repository** (see
  `docs/SOURCE_PROVENANCE.md`). The MIT text is also kept in `profiles/*/third_party/project2dfx/LICENSE.txt`.

```
MIT License

Copyright (c) 2018 ThirteenAG

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

Note for review: the version of the files taken was identified from this repository's history and the pinned
commit above; it was not re-compared against the upstream tree (no network comparison was made).
