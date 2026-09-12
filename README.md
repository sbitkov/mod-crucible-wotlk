# Crucible

Crucible is an AzerothCore module that changes individual character progression by allowing characters to permanently absorb stats from items.

It is designed primarily with solo play in mind, giving the player another form of long-term progression beyond ordinary gear upgrades.

## Preface

The module was inspired by the "Attunement" subsystem of the Synastria private server.

However, Crucible is a completely new implementation that does not copy Synastria's attunement system and was developed without relying on Synastria's source code.

### AI usage

Most of the module's source code was generated with the assistance of AI.

The resulting code was reviewed, tested, debugged, and integrated by a human developer (myself).

## Functionality

Crucible is an in-world object that allows the player to "absorb" the essence of an item. For now, it is possible to summon the Crucible with the chat command: `.gobject add temp 900000`.

When an item is successfully absorbed, the physical item is destroyed and its essence is permanently stored for that character inside the Crucible.

The first absorption grants the character 20% of the item's supported stats and is saved as part of permanent character progression.

Mastery of an already absorbed essence can then be improved further:

- 20%
- 40%
- 60%
- 80%
- 100%

Further mastery upgrades require gold and enchanting materials.

### Essence components

Since v0.7, Crucible distinguishes between several components of an item:

- `BASE` — stats belonging to the base item itself.
- `RANDOM_PROPERTY` — a specific random property rolled on that specific base item.
- `RANDOM_SUFFIX` — a specific random suffix rolled on that specific base item.

These components are stored and mastered independently.

For example, if the player has already absorbed the base essence of an item but later finds another copy with a new random affix, Crucible can absorb only that new affix component.

Likewise, a random affix can be improved from 20% to 100% without changing the mastery level of the base item.

An item is not destroyed if it is not eligible for absorption or if it contains no new absorbable essence components.

### Supported items

Crucible currently supports equippable armor items of Uncommon quality or higher.

Eligibility follows the character's normal class equipment progression. For example:

- Warriors and Paladins use Mail before level 40 and Plate afterwards.
- Hunters and Shamans use Leather before level 40 and Mail afterwards.
- Rogues and Druids use Leather.
- Mages, Priests, and Warlocks use Cloth.
- Death Knights use Plate.
- Cloaks are treated as universally usable armor.
- Shields and class-specific equipment still respect normal usability rules.

The item must also be usable by the character under the current Crucible eligibility rules.

Weapons are not currently supported.

### Stat absorption

Ordinary supported stats use the following mastery progression:

| Mastery | Absorbed value |
|---|---:|
| 20% | 20% of the source stat |
| 40% | 40% |
| 60% | 60% |
| 80% | 80% |
| 100% | 100% |

Fractional contributions are stored in the database.

They are added together before the final value is converted to the integer value used by the WoW 3.3.5a core.

For example, two stored contributions of `+0.6 Stamina` and `+0.4 Stamina` become `+1 Stamina` rather than both being individually truncated to zero.

Shield Armor is intentionally scaled differently:

| Mastery | Shield Armor absorbed |
|---|---:|
| 20% | 4% |
| 40% | 8% |
| 60% | 12% |
| 80% | 16% |
| 100% | 20% |

Other supported shield stats follow the ordinary mastery progression.

### Current mastery costs

The current mastery upgrade path is implemented for items with `RequiredLevel` 1–19.

| Upgrade | Cost |
|---|---|
| 20% → 40% | 5 silver |
| 40% → 60% | 10 silver + 2 Strange Dust |
| 60% → 80% | 15 silver + 1 Greater Magic Essence |
| 80% → 100% | 25 silver + 1 Small Glimmering Shard |

This limited level bracket is intentional. It provides a small, ChromieCraft-esque vertical slice in which the progression system can be tested before costs and progression are extended to the rest of the game.

## User interface

The user interface is provided by the included `CrucibleUI` addon.

The addon is located in:

`CrucibleUI/`

Copy the entire directory into the WoW client:

`<WoW client>/Interface/AddOns/CrucibleUI`

The resulting layout should look like:

```text
Interface/
└── AddOns/
    └── CrucibleUI/
        ├── CrucibleUI.toc
        └── CrucibleUI.lua
```

The addon contains the absorption interface, Stored Essences catalogue, mastery interface, and progression overview.

Progression data remains server-authoritative. The addon requests data from the server and does not calculate permanent Crucible stats itself.

## Dependencies / Compatibility

### Server

Crucible is developed for:

- AzerothCore
- World of Warcraft: Wrath of the Lich King 3.3.5a

The module should be placed inside the AzerothCore `modules` directory and compiled together with AzerothCore.

The repository is developed and tested against a current AzerothCore checkout. Compatibility with older AzerothCore revisions is not guaranteed.

### Client

The current Crucible addon is developed for a WoW 3.3.5a client, build `12340`.

It currently relies on:

- WarcraftXL
- WrathClassicAPI through WarcraftXL integration

WrathClassicAPI is used to obtain the GUID of a concrete physical item instance through APIs such as `C_Item.GetItemGUID()`.

These client-side dependencies are not bundled with this repository.

## Installation

### Server installation

1. Clone or copy the repository into the AzerothCore modules directory:

```text
<AzerothCore>/modules/mod-crucible
```

2. Re-run CMake so AzerothCore detects the module.

3. Build AzerothCore normally.

4. Install the newly built server binaries.

5. Start `worldserver`.

The module contains its character database schema and AzerothCore database update files. Database migrations are therefore handled through AzerothCore's normal module database update system.

### Client installation

1. Install WarcraftXL and its WrathClassicAPI integration.

2. Copy:

```text
mod-crucible/CrucibleUI
```

to:

```text
<WoW client>/Interface/AddOns/CrucibleUI
```

3. Restart the client if the addon was installed or replaced while the game was running.

4. Make sure `CrucibleUI` is enabled in the addon list.

### Spawning the Crucible

The Crucible uses gameobject entry:

```text
900000
```

A server administrator can spawn it using the normal AzerothCore gameobject commands or place it permanently into the world.

Interaction with the physical Crucible object is required for item absorption and mastery operations.

## Character progression storage

Crucible progression is stored per character.

The character database stores both:

- which essence components have been absorbed;
- the fractional stat contribution belonging to each component.

Since v0.7, the identity of an essence is composed of:

```text
character
essence type
item entry
affix id
```

This allows a base item and its individual random affixes to coexist as separate permanent progression records.

The accumulated bonuses are recalculated from the database and applied to the character by the server.

## Technical overview

The module is divided into several layers.

### AzerothCore module

The server-side implementation is written as an AzerothCore C++ module.

The core gameplay logic is responsible for:

- item eligibility;
- extracting supported stats;
- extracting random properties and random suffixes;
- storing absorbed components;
- mastery upgrades;
- aggregating permanent character stats;
- applying and removing those stats from the player.

Persistent data is stored in the AzerothCore character database.

The two principal Crucible tables are:

```text
character_crucible_absorption
character_crucible_contribution
```

`character_crucible_absorption` stores the identity and mastery level of an absorbed essence.

`character_crucible_contribution` stores the individual fractional stat contributions produced by that essence.

### Random affix extraction

Random-affix support uses the WotLK DBC data already available to AzerothCore.

For a concrete item instance, Crucible reads its random property identifier and suffix factor.

Random Properties are resolved through `ItemRandomProperties` and `SpellItemEnchantment` data.

Random Suffixes are resolved through `ItemRandomSuffix`, including their allocation values and the concrete item's suffix factor.

Only contributions that Crucible understands are stored.

A physical item may therefore contain:

- a BASE essence;
- a RANDOM_PROPERTY essence;
- a RANDOM_SUFFIX essence;

depending on the item.

### Stat application

Stored fractional values are aggregated by stat before being applied to the player.

The module uses AzerothCore's existing player/stat APIs where possible, including normal stat modifiers, combat ratings, attack power, spell power, resistances, armor, and related player modifiers.

This is intentional: Crucible attempts to use the existing AzerothCore stat system rather than implementing a parallel combat-stat system.

### Player lifecycle

Permanent Crucible bonuses are recalculated from persistent character data when necessary.

The module keeps track of the bonuses it has applied during the current player session so that they can be removed and recalculated without stacking duplicate copies of the same progression.

### Client/server communication

The addon communicates with the server using addon messages with the `CRUCIBLE` prefix.

The server remains authoritative.

The client can request operations such as:

- previewing an absorption;
- confirming absorption;
- requesting stored essences;
- requesting mastery information;
- upgrading mastery;
- requesting accumulated progression.

The client sends the GUID of the concrete physical item instance rather than merely an item entry when performing normal UI absorption.

This is important for random-affix support, because two physical copies of the same base item may contain different essences.

## Developer and debug commands

Crucible includes several GM/debug commands intended primarily for testing and module development.

They use the `.crucible` command namespace.

### `.crucible inspect <item_entry>`

Displays the supported BASE contributions Crucible sees on an item template.

Useful when checking whether a particular item's ordinary stats are understood by the extractor.

### `.crucible absorb <item_entry>`

Performs a server-side absorption attempt for the specified item entry.

This is primarily a development shortcut. Normal gameplay should use the Crucible world object and addon interface.

### `.crucible absorb-test <item_entry>`

Tests the absorption path without using the normal addon workflow.

Useful for debugging eligibility and contribution extraction.

### `.crucible unabsorb <item_entry>`

Removes the stored absorption record for the specified item during testing.

This modifies persistent Crucible progression and should therefore be used carefully.

### `.crucible upgrade <item_entry>`

Attempts a mastery upgrade through the debug command interface.

This command currently targets the BASE essence of the specified item.

Random-property and random-suffix mastery should normally be tested through the component-aware addon UI.

### `.crucible reset`

Deletes all Crucible absorption/contribution records for the current character and recalculates its bonuses.

This is destructive.

### `.crucible stats`

Displays the character's accumulated Crucible stats.

For each stat it reports the stored fractional total and the integer amount currently applied to the character.

### `.crucible apply`

Applies/recalculates the character's stored Crucible bonuses.

Primarily useful for debugging the stat application lifecycle.

### `.crucible unapply`

Temporarily removes the currently applied Crucible bonuses from the character without deleting the stored database progression.

### `.crucible recalc`

Removes the currently tracked Crucible bonuses and rebuilds them from the current database state.

This is useful after manually inspecting or modifying Crucible database records during development.

## Known limitations

Crucible is still an early version of the module, so several limitations currently exist.

- Weapons are not currently supported for absorption.
- The current mastery upgrade cost progression is implemented only for items with `RequiredLevel` 1–19.
- Not every possible WotLK item effect can currently be converted into permanent Crucible progression.
- Some random affixes therefore produce no supported Crucible contribution.
- School-specific spell damage bonuses, such as Frost Spell Damage, are not currently absorbed. AzerothCore represents these bonuses through spell-school aura mechanics rather than the normal permanent stat APIs used by Crucible, so support has been deliberately postponed rather than implemented through a fragile workaround.
- The project is still under active development. Bugs are expected, and bug reports are appreciated.
