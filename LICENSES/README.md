# License bundle

The root [LICENSE](../LICENSE) is the Expat (MIT) license for the library and
supporting code that are marked with that license. It is not a re-licensing of
files carrying another notice.

- `GPL-3.0-only.txt` is the complete GPL version 3 text for the GPL-3.0-only
  WeActCLI adaptation and its manual page.
- `LGPL-2.1-or-later.txt` accompanies the retained libqrencode declaration
  notice. The libqrencode implementation is not included: the program uses
  the distribution's dynamically linked `libqrencode4`.
- `BSD-3-Clause-Zint.txt` is the retained notice for the Zint-derived
  Code 128 and Code 39 run-width tables in `Source/codes/linear_tables.h`.
  The Zint library itself is not included or linked.

`debian/copyright` is the authoritative machine-readable file classification
for the Debian source package. `WeAct Display Tools Contributors` is the
project's collective attribution for its own code; according to the project
owner, the original PureBasic project and all own C code were created for this
project. This statement does not alter the separate notices for Zint and
libqrencode. Copyright ownership and permissions remain subject to the project
owner's publication review.
