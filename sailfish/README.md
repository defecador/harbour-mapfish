# Mapfish

Mapfish is a native [Sailfish OS](https://sailfishos.org/) offline-maps and navigation app. It is
**highly inspired by, and built directly on top of, [CoMaps](https://codeberg.org/comaps/comaps)**
(itself a fork of [Organic Maps](https://organicmaps.app/)) — this directory (`sailfish/`) is a
Sailfish Silica UI and platform layer added to an otherwise unmodified checkout of CoMaps' own
C++ engine (`libs/`), which does all of the map rendering, search, routing and data handling.

Mapfish is an independent, unofficial port. It is not affiliated with, endorsed by, or supported
by the CoMaps or Organic Maps projects.

## License and attribution

CoMaps (and therefore the engine Mapfish is built on) is licensed under the
[Apache License, Version 2.0](../LICENSE). Mapfish's own Sailfish-specific code in this directory
is released under the same license. In particular:

- The `LICENSE` and `NOTICE` files at the repository root apply to this whole project, including
  Mapfish's own additions.
- Map data is © [OpenStreetMap](https://www.openstreetmap.org/) contributors,
  [ODbL](https://opendatacommons.org/licenses/odbl/). The in-app copyright/attribution screen
  (`data/copyright.html`) ships unmodified.
- See `../data/copyright.html` for the full list of third-party licenses and attributions bundled
  with the app.

If you redistribute Mapfish, please keep this attribution and the above notices intact, per the
Apache License's terms.

## What's Sailfish-specific

Everything under `sailfish/` (this directory): the Silica QML UI, the Qt/QML glue code in
`sailfish/src/`, the packaging (`sailfish/rpm/`), and this README. Everything else in the
repository is CoMaps' own engine, used as-is.

## Building

See the build notes in the repository's main `INSTALL.md` for the general CoMaps toolchain, and
use the Sailfish SDK (`sfdk`) with the `sailfish/rpm/harbour-mapfish.spec` spec file to build and
package the Sailfish app itself.
