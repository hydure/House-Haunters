# include/rapidxml/

Vendored copy of [RapidXML](http://rapidxml.sourceforge.net/) 1.13 —
Marcin Kalicinski's single-header, in-place, zero-allocation DOM-style XML
parser. Used by:

* [src/engine/ClueReader.cpp](../../src/engine/ClueReader.cpp) — parses
  `resources/items.xml` for clue tiers.
* [src/engine/ModConfig.cpp](../../src/engine/ModConfig.cpp) — parses
  `resources/mods.xml` for the modding configuration.

## Contents

| File | Purpose |
|---|---|
| `rapidxml.hpp` | The parser itself. `xml_document::parse<Flags>(char* buffer)` parses in place; `xml_node::first_node(name)`, `first_attribute(name)`, `next_sibling(name)` walk the result. |
| `rapidxml_iterators.hpp` | Optional STL-iterator adapters for nodes / attributes. |
| `rapidxml_print.hpp` | Optional re-serialization (write XML back out). Not currently used by the project. |
| `rapidxml_utils.hpp` | `file<>` helper that reads a whole file into a buffer. Not currently used (we read with `std::ifstream` for cleaner error paths). |

## Critical usage notes

* **Buffer lifetime.** `parse<0>` is non-owning: every `xml_node` stores
  raw `char*` pointers into the source buffer. The buffer must outlive
  the `xml_document`. Both `ClueReader` and `ModConfig` keep an
  instance-scoped `std::string` (or `std::vector<char>`) for exactly this
  reason — do the same if you add a new XML loader.
* **Mutable buffer.** `parse<0>` writes null terminators into the
  buffer to terminate tag/attribute names. Never pass a `const char*`
  or a string literal directly; copy into a mutable buffer first.
* **Exceptions.** RapidXML throws `rapidxml::parse_error` on malformed
  input. Our loaders catch this and convert it into a `false` return so
  a bad mod / clue file is non-fatal at runtime.

## Don't edit

This is third-party code; do not modify in place. If RapidXML ever needs
an upgrade, drop a fresh copy of the official release tarball over the
top of this directory in one commit and audit our two call sites.

## License

RapidXML is dual-licensed under the Boost Software License and the MIT
License. The license headers are inline in `rapidxml.hpp`.
