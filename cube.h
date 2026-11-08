// Cube World Alpha - game objects as far as this mod reads them
#ifndef CUBE_H
#define CUBE_H

#include <stdint.h>
#include <stddef.h>

// cube::Item
struct Item
{
    uint8_t  type;
    uint8_t  subType;
    uint8_t  _pad0[2];
    uint32_t modifier;
    uint32_t recipeType;
    uint8_t  rarity;
    uint8_t  material;
    uint8_t  flags;
    uint8_t  _pad1[1];
    uint16_t level;
    uint8_t  _pad2[2];
    uint8_t  spirits[0x100];
    uint32_t spiritCount;
};
static_assert(sizeof(Item) == 0x118, "Item layout");

// cube::ItemStack, one inventory slot, quantity 0 is an empty slot
struct ItemStack
{
    int32_t quantity;
    Item    item;
};
static_assert(sizeof(ItemStack) == 0x11C, "ItemStack layout");

// MSVC std::vector<ItemStack>, one inventory tab
struct Tab
{
    ItemStack* begin;
    ItemStack* end;
    ItemStack* capacity;
};

// MSVC std::vector<Tab>, all tabs of an inventory
struct Tabs
{
    Tab* begin;
    Tab* end;
    Tab* capacity;
};
static_assert(sizeof(Tab) == 12 && sizeof(Tabs) == 12, "std::vector is 3 pointers");

// cube::Creature
struct Creature
{
    uint8_t _pad0[0x11DC];
    Tabs    tabs;
};
static_assert(offsetof(Creature, tabs) == 0x11DC, "Creature layout");

// windows remembering an inventory slot across frames

// spirit upgrade window
struct UpgradeWindow
{
    uint8_t _pad0[0x160];
    Item*   item;           // raw pointer into a tab or an equipment slot
};
static_assert(offsetof(UpgradeWindow, item) == 0x160, "UpgradeWindow layout");

// cube::EnchantWidget, identify window of the identifier NPC
struct IdentifyWindow
{
    uint8_t _pad0[0x160];
    int32_t tab;
    int32_t index;
};
static_assert(offsetof(IdentifyWindow, tab) == 0x160, "IdentifyWindow layout");
static_assert(offsetof(IdentifyWindow, index) == 0x164, "IdentifyWindow layout");

// cube::AdaptionWidget
struct AdaptionWindow
{
    uint8_t _pad0[0x160];
    Item*   item;           // set for equipment slots, 0 for inventory slots
    int32_t tab;
    int32_t index;
};
static_assert(offsetof(AdaptionWindow, item) == 0x160, "AdaptionWindow layout");
static_assert(offsetof(AdaptionWindow, tab) == 0x164, "AdaptionWindow layout");
static_assert(offsetof(AdaptionWindow, index) == 0x168, "AdaptionWindow layout");

// plasma::Widget, only passed to the game's functions
struct Widget
{
};

// plasma::Display
struct Display
{
    uint8_t  _pad0[0x68];
    int32_t  visibleFrame;          // visibility attribute, index of the current keyframe
    uint8_t  _pad1[0x94 - 0x6C];
    int32_t* visibleValues;         // visibility attribute, one int per keyframe
    uint8_t  _pad2[0xF8 - 0x98];
    uint8_t  fill[0x58];            // fill colour, a plasma::Attribute
};
static_assert(offsetof(Display, visibleFrame) == 0x68, "Display layout");
static_assert(offsetof(Display, visibleValues) == 0x94, "Display layout");
static_assert(offsetof(Display, fill) == 0xF8, "Display layout");

// plasma::Node
struct Node
{
    uint8_t  _pad0[0x3C];
    Display* display;
    Widget*  widget;
};
static_assert(offsetof(Node, display) == 0x3C, "Node layout");
static_assert(offsetof(Node, widget) == 0x40, "Node layout");

// MSVC std::list<Node*>
struct NodeListEntry
{
    NodeListEntry* next;
    NodeListEntry* prev;
    Node*          value;
};

struct NodeList
{
    NodeListEntry* head;    // sentinel, not a node
    uint32_t       size;
};

struct GameController;

// cube::InventoryWidget
struct InventoryWidget : Widget
{
    uint8_t         _pad0[0x148];
    Node*           node;
    uint8_t         _pad1[0x164 - 0x14C];
    GameController* game;
    uint8_t         _pad2[0x17C - 0x168];
    NodeList        tabNodes;
    int32_t         hoverTab;
    int32_t         hoverIndex;
};
static_assert(offsetof(InventoryWidget, node) == 0x148, "InventoryWidget layout");
static_assert(offsetof(InventoryWidget, game) == 0x164, "InventoryWidget layout");
static_assert(offsetof(InventoryWidget, tabNodes) == 0x17C, "InventoryWidget layout");
static_assert(offsetof(InventoryWidget, hoverTab) == 0x184, "InventoryWidget layout");
static_assert(offsetof(InventoryWidget, hoverIndex) == 0x188, "InventoryWidget layout");

// cube::GameController
struct GameController
{
    uint8_t          _pad0[0x8006D0];
    Creature*        player;                    // local player
    uint8_t          _pad1[0x8008BC - 0x8006D4];
    Node*            inventory;                 // equipment window
    uint8_t          _pad2[0x8008F4 - 0x8008C0];
    UpgradeWindow*   upgrade;
    uint8_t          _pad3[0x8008FC - 0x8008F8];
    IdentifyWindow*  identify;
    uint8_t          _pad4[0x800904 - 0x800900];
    AdaptionWindow*  adaption;
    uint8_t          _pad5[0x800954 - 0x800908];
    InventoryWidget* inventoryGrid;             // item grid of the equipment window
    uint8_t          _pad6[0x80095C - 0x800958];
    InventoryWidget* tradeGrid;                 // item grid of the trade window
};
static_assert(offsetof(GameController, player) == 0x8006D0, "GameController layout");
static_assert(offsetof(GameController, inventory) == 0x8008BC, "GameController layout");
static_assert(offsetof(GameController, upgrade) == 0x8008F4, "GameController layout");
static_assert(offsetof(GameController, identify) == 0x8008FC, "GameController layout");
static_assert(offsetof(GameController, adaption) == 0x800904, "GameController layout");
static_assert(offsetof(GameController, inventoryGrid) == 0x800954, "GameController layout");
static_assert(offsetof(GameController, tradeGrid) == 0x80095C, "GameController layout");

#endif
