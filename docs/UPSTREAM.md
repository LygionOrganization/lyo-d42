# Upstream provenance and compatibility

Imported firmware, libraries, web console, generic examples and regression tools from:

- https://github.com/LygionOrganization/robocore_s3
- Commit: `8b1327d18432b566c70d0f7782f6a93a00fc0920`
- License: AGPL-3.0; see repository LICENSE-AGPL and individual library licenses.

Wheel installation signs and optional RC example derive from the local lymino project. Lyo-D42 uses the user-confirmed159mm wheel track rather than the old source's157mm constant.

The original `docs/ROBOCORE_*` documents describe the upstream controller and are retained as generic-device references. They are **not** the Lyo-D42 product specification. Lyo-D42 overrides their single-mailbox policy with bounded dual slots and reserves chassis resources. `docs/LYO_D42_API.md` and the project README take precedence for this product.

No prebuilt upstream factory downloads were copied or represented as Lyo-D42 releases. `.upstream/` is an ignored reference checkout; `.pio/` contains local build/verification artifacts. Repository packaging should use Lyo-D42 sources and freshly verified builds; legacy RoboCore packaging/wiki-publishing scripts are upstream references, not Lyo-D42 release commands.

The original project GPLv3 LICENSE is preserved. LICENSE-AGPL retains the upstream RoboCore license separately; individual inherited files and library notices remain applicable. The standalone SDKs use the existing project GPLv3 license, included in each package.
