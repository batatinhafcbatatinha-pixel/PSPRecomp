# Source provenance and license boundaries

PSPRecomp separates independently written framework/profile code from third-party components with their own licenses.

## Framework

The reusable PSPRecomp runtime, decoder, analyzer and code generator in the repository root are distributed under the MIT License. Game-specific addresses and implementations are not accepted in the reusable core.

The project may use public hardware documentation, observable program behavior and other implementations as technical references. Reference material is used to understand behavior and architecture; source code from incompatible copyleft projects is not imported into the MIT framework.

## Decryption

The repository does not contain an EBOOT/PRX decryption implementation. A profile may document the input format it expects, but users are responsible for preparing their own legally obtained executable outside PSPRecomp.

## Profile code

A profile owns its generated AOT corpus, address-specific lowering, HLE behavior and native fast paths. Those files remain isolated under `profiles/<id>` so they do not become hidden dependencies of the generic framework.

## Third-party components

Third-party source, binary dependencies, shader code and notices stay beside the profile that needs them. Their original copyright and license notices must be preserved. The VCS profile summarizes its bundled components in `profiles/vcs/THIRD_PARTY.md`.

## Contribution rule

Do not paste or adapt source from a project whose license is incompatible with the destination file. Reimplement required behavior from specifications, observations or independently documented semantics, and record the source of third-party material when it is intentionally included under a compatible license.

## Generated and game-derived code

Output of the recompiler for a game's code (`profiles/*/generated/`, `build/gen`, overlay translations), dumps,
captures, game data and disassembly never enter the repository; they are produced on the user's machine from the
user's own files and are git-ignored. The checked-in `profiles/vcs/generated/` inherited from upstream was removed
for this reason. Host code must be written from observed behaviour and public documents, not by transcribing game
routines (a CTW inflate file that said so was removed; a replacement is to be written from RFC 1951 or zlib and
recorded in `THIRD_PARTY_NOTICES.md`).

## WidescreenFixesPack (ThirteenAG)

Code and data derived from WidescreenFixesPack were taken on 2026-08-12 (`37e5469`) when that project was MIT
licensed; see `THIRD_PARTY_NOTICES.md`. The project changed to GPL-3.0 on 2026-11-08. **Nothing newer than what is
already in the tree may be taken from it into this MIT repository**, and no new code may be derived from its current
sources. Behaviour may still be reimplemented from observation of the game and from public documentation.
