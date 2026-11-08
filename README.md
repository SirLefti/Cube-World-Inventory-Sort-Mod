# Cube World Inventory Sort Mod

Sorts your inventory tabs by pressing a button on the top right of your
inventory window or by pressing `N` while the inventory is open.

This mod is for the **alpha** build of Cube World.

## Sorting rules

Inside a group, items are sorted by rarity, then by power.

### Equipment
1. Glider, Boat
2. Weapons, by class, then by weapon type:
   - Warrior: sword, axe, mace, shield, greatsword, greataxe, greatmace
   - Ranger: bow, crossbow, boomerang
   - Mage: staff, wand, bracelet
   - Rogue: dagger, fist, longsword
3. Armour, by slot: chest, gloves, boots, shoulder
4. Amulets
5. Rings
6. Lamps

### Items
1. Potions: life, cactus, mana
2. Other Food
3. Bombs
4. Formulas, by class: warrior, ranger, mage, rogue, other

### Ingredients
1. Raw materials
2. Nuggets and Gems
3. Processed cubes
4. Spirit cubes, by element: fire, unholy, ice, wind

### Pets
1. Pets
2. Pet Food

## Installation

* requires [coremaze Cube-World-Mod-Launcher](https://github.com/coremaze/Cube-World-Mod-Launcher/releases/tag/v1.5)
* unpack Mod Launcher in the game directory
* put one of the three versions into `Mods`: `InventorySortMod.dll` (button and
  key), `InventorySortButtonMod.dll` (button only) or `InventorySortKeyMod.dll`
  (key only)
* start the game via `CubeModLauncher.exe`

## Compilation

Requires a compiler that targets 32-bit Windows. Due to the `asm` style, `MSVC`
will not work, use `GCC` or `Clang`. Run `make` in this directory and specify
the compiler as `CXX` variable if necessary (e.g.
`make CXX=i686-w64-mingw32-g++`). `make` builds all three versions; when
compiling by hand, pass `-DHOOK_KEY`, `-DHOOK_BUTTON` or both.
