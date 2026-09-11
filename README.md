# mod-crucible-wotlk

AzerothCore WotLK module that adds the **Crucible**: a permanent character-progression system based on absorbing equipment.

The project deliberately scales the **player upward** instead of scaling the world downward.

## Preface

Crucible is **inspired by the progression ideas of the Synastria private server**. I liked the general idea of turning equipment into permanent character progression and wanted to explore my own implementation of that concept for AzerothCore.

This project is an independent implementation. It was designed and developed separately for this module without relying on Synastria's source code. The mechanics, server-side architecture, item-instance transport, preview protocol, and client UI in this repository were built specifically for this project.

## Status

Current version: **v0.2**

The current implementation supports:

- a physical Crucible game object (`entry 900000`);
- per-character, per-`item_entry` absorption tracking;
- permanent stat contributions stored with fractional precision;
- aggregate-then-truncate application of permanent bonuses;
- exact physical item-instance selection by GUID;
- server-authoritative absorption;
- server-authoritative preview of the stats an item will grant before absorption;
- a client UI with a virtual item slot, item icon, item name, preview, reservation feedback, and an `Absorb` button;
- explicit server results such as `SUCCESS`, `ALREADY_ABSORBED`, `WEAPON_UNSUPPORTED`, and `NO_SUPPORTED_STATS`.

Weapons are intentionally unsupported at this stage.

## How absorption works

For supported item stats, the Crucible currently stores:

```text
absorbed value = source value Р“вЂ” 0.20
```

The fractional values are persisted in the character database.

Permanent bonuses are aggregated first and only then converted to the integer values applied by the WoW 3.3.5a core. This means separate contributions such as `+0.6 Stamina` and `+0.4 Stamina` combine into `+1 Stamina` rather than being truncated independently.

The server is authoritative. The client UI identifies a concrete physical item by GUID, requests a preview from the server, and only sends the absorption request when the player confirms it.

## Repository layout

```text
mod-crucible-wotlk/
РІвЂќСљРІвЂќР‚РІвЂќР‚ CrucibleUI/                  Client addon
РІвЂќвЂљ   РІвЂќСљРІвЂќР‚РІвЂќР‚ CrucibleUI.toc
РІвЂќвЂљ   РІвЂќвЂќРІвЂќР‚РІвЂќР‚ CrucibleUI.lua
РІвЂќСљРІвЂќР‚РІвЂќР‚ data/
РІвЂќвЂљ   РІвЂќвЂќРІвЂќР‚РІвЂќР‚ sql/
РІвЂќвЂљ       РІвЂќвЂќРІвЂќР‚РІвЂќР‚ db-characters/
РІвЂќвЂљ           РІвЂќвЂќРІвЂќР‚РІвЂќР‚ base/
РІвЂќвЂљ               РІвЂќвЂќРІвЂќР‚РІвЂќР‚ crucible.sql
РІвЂќСљРІвЂќР‚РІвЂќР‚ src/                         AzerothCore module source
РІвЂќСљРІвЂќР‚РІвЂќР‚ apps/                        CI helpers
РІвЂќСљРІвЂќР‚РІвЂќР‚ .github/                     GitHub workflows/templates
РІвЂќСљРІвЂќР‚РІвЂќР‚ LICENSE
РІвЂќвЂќРІвЂќР‚РІвЂќР‚ README.md
```

## Requirements

### Server

- AzerothCore WotLK 3.3.5a
- module installed under the AzerothCore `modules` directory

### Client

The current Crucible UI relies on the client-side environment used during development:

- WarcraftXL
- WrathClassicAPI through the WarcraftXL integration
- WoW 3.3.5a client build 12340

`WrathClassicAPI` provides the physical item-instance API used by the addon, including `C_Item.GetItemGUID()` and `C_Item.GetItemLocation()`.

Those projects are **dependencies** and are not vendored into this repository.

## Client addon installation

Copy the repository's `CrucibleUI` directory to:

```text
<WoW client>\Interface\AddOns\CrucibleUI
```

The final structure should be:

```text
Interface\AddOns\CrucibleUI\CrucibleUI.toc
Interface\AddOns\CrucibleUI\CrucibleUI.lua
```

Restart the client after replacing the addon during development.

## Development notes

The transport currently uses WoW addon messages with the `CRUCIBLE` prefix.

Important messages include:

```text
Client -> Server
CRUCIBLE    PREVIEW    <item GUID>
CRUCIBLE    ABSORB     <item GUID>

Server -> Client
CRUCIBLE    PREVIEW_BEGIN    <result>    <item GUID>
CRUCIBLE    PREVIEW_STAT     <item GUID> <stat name> <value>
CRUCIBLE    PREVIEW_END      <item GUID>
CRUCIBLE    RESULT           <result>    <item GUID>
```

The client does not calculate Crucible contributions itself. Preview values and actual absorption use the same server-side contribution logic.

## Development process

Development of this project was carried out with the assistance of AI tools. AI was used for code drafting, research, debugging support, and discussion of implementation approaches.

AI output was not treated as authoritative or accepted blindly. I manually reviewed the code, built the project myself, and tested the implemented behavior in-game throughout development. Functional changes were verified through hands-on testing before being accepted into the project.

## License

This project is licensed under **GNU General Public License version 2 or, at your option, any later version** (`GPL-2.0-or-later`).

### Licensing history

The `v0.1` tag accidentally inherited the MIT `LICENSE` file from the AzerothCore module skeleton used to bootstrap the repository. That historical tag is intentionally left unchanged.

Starting with the v0.2 development line, the project is explicitly licensed as `GPL-2.0-or-later`.

Third-party dependencies retain their own licenses.
