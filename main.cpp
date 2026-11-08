// Cube World Alpha - Inventory Sort Mod
//
// Sorts every inventory tab into a fixed order, from a button at the top right
// of the inventory window or the sort key while the inventory is open. Inside a
// group, the strongest item comes first: higher rarity, then higher level.

// ---- build requirements -----------------------------------------------------
#include <stdint.h>

#if !defined(_WIN32)
#  error "Target must be Windows: this is a DLL injected into Cube.exe. Cross-compile, e.g. i686-w64-mingw32-g++."
#endif
#if UINTPTR_MAX != 0xFFFFFFFFu
#  error "Target must be 32-bit: Cube.exe is PE32."
#endif
#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ < 8
#  error "GCC 8 or newer: older GCC silently IGNORES __attribute__((naked)) on x86 and builds a broken DLL."
#endif

#include <windows.h>
#include <string.h>
#include <stdlib.h>

#include "cube.h"

// virtual-key code that sorts the inventory while the inventory is open
static const uint8_t SORT_KEY = 'N';

// makefile passes -DHOOK_KEY, -DHOOK_BUTTON or both, one DLL each
#ifndef HOOK_KEY
#  define HOOK_KEY 0
#endif
#ifndef HOOK_BUTTON
#  define HOOK_BUTTON 0
#endif
#if !HOOK_KEY && !HOOK_BUTTON
#  error "Pass -DHOOK_KEY, -DHOOK_BUTTON or both: without one, the DLL hooks nothing."
#endif

// ---- game functions ---------------------------------------------------------

static const uint32_t SOUND_TAB = 0x56; // sound of a tab click
static const uint32_t EVENT_POINTER_PRESSED = 2;

// tab row layout of InventoryWidget::setTabNodes, the first tab sits at (TAB_MARGIN, -height - TAB_GAP)
static const float TAB_MARGIN = 4.0f; // from the grid's side edge
static const float TAB_GAP    = 3.0f; // between the tabs and the grid's top edge

typedef void  (__attribute__((thiscall)) *RefreshFn)(InventoryWidget* grid);
typedef void  (__attribute__((thiscall)) *PlaySoundFn)(GameController* gc, uint32_t sound);
typedef void  (__attribute__((thiscall)) *DrawFn)(InventoryWidget* grid);
typedef Node* (__attribute__((thiscall)) *CloneFn)(Node* node, Node* parent);
typedef void  (__attribute__((thiscall)) *AddChildFn)(Node* node, Node* child);
typedef Node* (__attribute__((thiscall)) *FindNodeFn)(Node* node, void* name);
typedef void  (__attribute__((thiscall)) *WStringFn)(void* string, const wchar_t* text);
typedef void  (__attribute__((thiscall)) *WStringFreeFn)(void* string);
typedef void  (__attribute__((thiscall)) *SetColorFn)(uint8_t* fill, const float* rgba);
typedef bool  (__attribute__((thiscall)) *IsPointerOverFn)(Widget* widget);
typedef float (__attribute__((thiscall)) *SizeFn)(Widget* widget);
typedef void  (__attribute__((thiscall)) *MoveToFn)(Widget* widget, float x, float y, bool absolute);
typedef void  (__attribute__((thiscall)) *ConnectFn)(Widget* widget, uint32_t event, void* object,
                                                     void* method, uint32_t adjust, bool recursive,
                                                     bool enabledOnly);

static const uint32_t RVA_REFRESH_A = 0x000C6350;       // grid refresh, called after opening the window
static const uint32_t RVA_REFRESH_B = 0x000C64C0;
static const uint32_t RVA_PLAY_SOUND = 0x00084320;      // ui sound, thiscall on the GameController
static const uint32_t RVA_NODE_CLONE = 0x002326D0;      // Node::clone(parent)
static const uint32_t RVA_ADD_CHILD = 0x00230BE0;       // Node::addChild(child)
static const uint32_t RVA_FIND_NODE = 0x00233D70;       // Node::findNode(const std::wstring&)
static const uint32_t RVA_WSTRING  = 0x0000EB60;        // std::wstring(const wchar_t*)
static const uint32_t RVA_WSTRING_FREE = 0x00193E50;    // ~std::wstring
static const uint32_t RVA_SET_COLOR = 0x000288E0;       // fill colour = rgba
static const uint32_t RVA_POINTER_OVER = 0x002294C0;    // Widget::isPointerOver
static const uint32_t RVA_VIEW_WIDTH = 0x0022F600;      // Widget::viewWidth
static const uint32_t RVA_VIEW_HEIGHT = 0x002291D0;     // Widget::getViewHeight
static const uint32_t RVA_MOVE_TO = 0x0022A650;         // Widget::moveTo(x, y, absolute)
static const uint32_t RVA_CONNECT = 0x000C1A90;         // Widget::connect, the InventoryWidget instance

static uint32_t        base;
static RefreshFn       RefreshA;
static RefreshFn       RefreshB;
static PlaySoundFn     PlayUiSound; // not PlaySound, windows.h has a macro of that name
static CloneFn         CloneNode;
static AddChildFn      AddChild;
static ConnectFn       Connect;
static WStringFn       WStringCreate;
static WStringFreeFn   WStringFree;
static FindNodeFn      FindNode;
static SetColorFn      SetColor;
static IsPointerOverFn IsPointerOver;
static SizeFn          ViewWidth;
static SizeFn          ViewHeight;
static MoveToFn        MoveTo;

// ---- sort order -------------------------------------------------------------

// group of an item, lower comes first; the groups of one tab never mix with
// another tab, since the game files items into tabs by type
static int GroupOf(const Item* item)
{
    switch (item->type) {
        // equipment
        case 23: return 0;                                  // hang glider, boat
        case 3:  return 1;                                  // weapons
        case 4:  return 2;                                  // chest
        case 5:  return 3;                                  // gloves
        case 6:  return 4;                                  // boots
        case 7:  return 5;                                  // shoulders
        case 8:  return 6;                                  // amulets
        case 9:  return 7;                                  // rings
        case 24: return 8;                                  // lamps
        // items
        case 1:
            if (item->subType >= 1 && item->subType <= 3) {
                return 10;                                  // life, cactus, mana potion
            }
            if (item->subType == 7) {
                return 12;                                  // bombs
            }
            return 11;                                      // other food
        case 2:  return 13;                                 // formulas
        case 25: return 14;                                 // mana cubes
        case 21: return 15;                                 // quest items
        // ingredients
        case 11:
            if (item->subType == 0) {
                return 21;                                  // nuggets and gems
            }
            if (item->subType == 10) {
                return 22;                                  // processed cubes
            }
            if (item->subType == 14) {
                return 23;                                  // spirits
            }
            return 20;                                      // raw materials
        // pets
        case 19: return 30;                                 // pets
        case 20: return 31;                                 // pet food
    }
    return 40;                                              // anything else, by type
}

// in sort order, the gear of a class comes before that of the next
enum class PlayerClass { Warrior, Ranger, Mage, Rogue, Any };

// what a class uses, weapons in sort order
struct ClassGear
{
    uint8_t armorMaterial;
    uint8_t weaponCount;
    uint8_t weapons[7];

    // the count is taken from the list, so it cannot be wrong
    template <size_t N>
    constexpr ClassGear(uint8_t material, const uint8_t (&list)[N])
        : armorMaterial(material), weaponCount(N), weapons{}
    {
        static_assert(N <= sizeof(weapons), "more weapons than fit, raise the size of weapons");
        for (size_t i = 0; i < N; i++) {
            weapons[i] = list[i];
        }
    }
};

// indexed by PlayerClass
static const ClassGear CLASS_GEAR[] = {
    { 1,  { 0, 1, 2, 13, 15, 16, 17 } },    // warrior: iron; sword, axe, mace, shield, greatsword, greataxe, greatmace
    { 26, { 6, 7, 8 } },                    // ranger: linen; bow, crossbow, boomerang
    { 25, { 10, 11, 12 } },                 // mage: silk; staff, wand, bracelet
    { 27, { 3, 4, 5 } },                    // rogue: cotton; dagger, fist, longsword
};
static_assert(sizeof(CLASS_GEAR) / sizeof(CLASS_GEAR[0]) == (size_t)PlayerClass::Any, "one entry per class");

// class that uses a weapon sub type, and its position in that class's list
static PlayerClass WeaponClass(uint8_t subType, int* index)
{
    for (int c = 0; c < (int)PlayerClass::Any; c++) {
        for (int i = 0; i < CLASS_GEAR[c].weaponCount; i++) {
            if (CLASS_GEAR[c].weapons[i] == subType) {
                *index = i;
                return (PlayerClass)c;
            }
        }
    }
    return PlayerClass::Any;
}

// class that wears armour of a material
static PlayerClass ArmorClass(uint8_t material)
{
    for (int c = 0; c < (int)PlayerClass::Any; c++) {
        if (CLASS_GEAR[c].armorMaterial == material) {
            return (PlayerClass)c;
        }
    }
    return PlayerClass::Any;
}

// weapons by class, then by the class's list; unlisted ones after the listed ones
static int WeaponOrder(uint8_t subType)
{
    int index;
    PlayerClass cls = WeaponClass(subType, &index);
    if (cls == PlayerClass::Any) {
        return 0x100 + subType;
    }
    return (int)cls * 0x10 + index;
}

// class that uses the item a formula makes: recipe type + sub type + material
static PlayerClass FormulaClass(const Item* formula)
{
    int index;
    switch (formula->recipeType) {
        case 3: // weapons
            return WeaponClass(formula->subType, &index);
        case 4: case 5: case 6: case 7: // armour
            return ArmorClass(formula->material);
    }
    return PlayerClass::Any;
}

// position of an item inside its group, lower comes first
static int SubTypeOrder(const Item* item)
{
    switch (item->type) {
        case 2:
            return (int)FormulaClass(item);                 // formulas by class first
        case 3:
            return WeaponOrder(item->subType);
        case 11:
            if (item->subType == 14) {
                return item->material;                      // spirits by element: fire, unholy, ice, wind
            }
            break;
    }
    return item->subType;
}

// position of the item a formula makes, in the equipment order
static int FormulaTargetOrder(const Item* formula)
{
    Item target;
    memset(&target, 0, sizeof(target)); // only type and sub type are read
    target.type = (uint8_t)formula->recipeType;
    target.subType = formula->subType;
    return GroupOf(&target) * 0x1000 + SubTypeOrder(&target);
}

static int CompareStacks(const void* pa, const void* pb)
{
    const ItemStack* a = (const ItemStack*)pa;
    const ItemStack* b = (const ItemStack*)pb;

    // empty slots last
    bool emptyA = a->quantity <= 0;
    bool emptyB = b->quantity <= 0;
    if (emptyA != emptyB) return emptyA ? 1 : -1;
    if (emptyA) return 0;

    const Item* x = &a->item;
    const Item* y = &b->item;
    int groupX = GroupOf(x);
    int groupY = GroupOf(y);
    if (groupX != groupY) return groupX < groupY ? -1 : 1;
    if (x->type != y->type) return x->type < y->type ? -1 : 1;
    int subX = SubTypeOrder(x);
    int subY = SubTypeOrder(y);
    if (subX != subY) return subX < subY ? -1 : 1;

    // strongest first
    if (x->rarity != y->rarity) return x->rarity > y->rarity ? -1 : 1;
    if (x->level != y->level) return x->level > y->level ? -1 : 1;

    // formulas of the same class, tier and power by what they make
    if (x->type == 2) {
        int targetX = FormulaTargetOrder(x);
        int targetY = FormulaTargetOrder(y);
        if (targetX != targetY) return targetX < targetY ? -1 : 1;
    }

    // armour of a class before that of the next
    if (x->type >= 4 && x->type <= 7) {
        PlayerClass classX = ArmorClass(x->material);
        PlayerClass classY = ArmorClass(y->material);
        if (classX != classY) return classX < classY ? -1 : 1;
    }

    if (x->material != y->material) return x->material < y->material ? -1 : 1;

    // everything else, so the order never depends on the previous one
    int rest = memcmp(x, y, sizeof(Item));
    if (rest != 0) return rest;
    if (a->quantity != b->quantity) return a->quantity > b->quantity ? -1 : 1;
    return 0;
}

// ---- sorting ----------------------------------------------------------------

static bool IsVisible(Node* node)
{
    Display* display = node->display;
    return display->visibleValues[display->visibleFrame] != 0;
}

static void SortInventory(GameController* gc)
{
    Creature* player = gc->player;
    if (!player) {
        return;
    }
    Tab* tabsBegin = player->tabs.begin;
    Tab* tabsEnd = player->tabs.end;

    // the upgrade window keeps a raw pointer into a tab, even after closing
    UpgradeWindow* upgrade = gc->upgrade;
    uint8_t* upgradeItem = upgrade ? (uint8_t*)upgrade->item : NULL;

    for (Tab* tab = tabsBegin; tab < tabsEnd; tab++) {
        size_t count = tab->end - tab->begin;
        if (upgradeItem >= (uint8_t*)tab->begin && upgradeItem < (uint8_t*)tab->end) {
            upgrade->item = NULL;
        }
        if (count > 1) {
            // the actual sort happens here, with our callback that defines the order
            qsort(tab->begin, count, sizeof(ItemStack), CompareStacks);
        }
    }

    // forget slot indices of the old order
    InventoryWidget* grid = gc->inventoryGrid;
    grid->hoverTab = -1;
    grid->hoverIndex = -1;

    IdentifyWindow* identify = gc->identify;
    if (identify) {
        identify->tab = -1;
        identify->index = -1;
    }

    AdaptionWindow* adaption = gc->adaption;
    if (adaption) {
        adaption->tab = -1;
        adaption->index = -1;
    }

    // recalculate scroll bar length
    RefreshA(grid);
    // reposition scroll bar thumb
    RefreshB(grid);
    PlayUiSound(gc, SOUND_TAB);
}

// ---- hook bodies ------------------------------------------------------------

extern "C" void OnKey(GameController* gc, uint8_t key)
{
    if (key != SORT_KEY) {
        return;
    }
    if (IsVisible(gc->inventory)) {
        SortInventory(gc);
    }
}

// ---- sort button ------------------------------------------------------------

static DrawFn DrawOriginal;

static Node* button;
static Node* buttonFrame;
static float buttonX = -1.0f;
static float buttonY = -1.0f;
static bool isButtonCreated = false;

// i-th node of a std::list<Node*>, 0 if the list is shorter
static Node* ListAt(NodeList* list, int index)
{
    NodeListEntry* entry = list->head->next;
    for (int i = 0; i < index && entry != list->head; i++) {
        entry = entry->next;
    }
    return entry == list->head ? NULL : entry->value;
}

extern "C" __attribute__((thiscall)) void OnSortButton(InventoryWidget* grid)
{
    SortInventory(grid->game);
}

static void CreateButton(InventoryWidget* grid)
{
    InventoryWidget* trade = grid->game->tradeGrid;
    Node* buyBack = trade ? ListAt(&trade->tabNodes, 1) : NULL;
    if (!buyBack) {
        return;
    }

    // we clone the buyback node because we want to re-use the icon
    Node* node = CloneNode(buyBack, NULL);
    AddChild(grid->node, node);
    // connect sort event to this node
    Connect(node->widget, EVENT_POINTER_PRESSED, grid, (void*)&OnSortButton, 0, true, true);

    uint8_t name[0x20]; // std::wstring, 0x18 bytes
    WStringCreate(name, L"frame");
    buttonFrame = FindNode(node, name);
    WStringFree(name);

    button = node;
}

static void SetFill(Node* node, float r, float g, float b)
{
    const float rgba[4] = { r, g, b, 1.0f };
    SetColor(node->display->fill, rgba);
}

static void UpdateButton(InventoryWidget* grid)
{
    Widget* widget = button->widget;

    // the tabs' row, right aligned, as far from the edge as the first tab
    float x = ViewWidth(grid) - ViewWidth(widget) - TAB_MARGIN;
    float y = -ViewHeight(widget) - TAB_GAP;
    if (x != buttonX || y != buttonY) {
        MoveTo(widget, x, y, true);
        buttonX = x;
        buttonY = y;
    }

    bool isPointerHovering = IsPointerOver(widget);

    // white while hovered, grey otherwise
    float shade = isPointerHovering ? 1.0f : 0.8f;
    SetFill(button, shade, shade, shade);

    if (buttonFrame) {
        // lack of red turns white into cyan
        float red = isPointerHovering ? 0.0f : 1.0f;
        // cyan while hovered, white otherwise
        SetFill(buttonFrame, red, 1.0f, 1.0f);
    }
}

extern "C" __attribute__((thiscall)) void DrawHook(InventoryWidget* grid)
{
    // we need to draw the original because we replaced that call
    DrawOriginal(grid);

    // the hook sits in the class, also the trade and crafting windows draw here
    GameController* gc = grid->game;
    if (!gc || gc->inventoryGrid != grid) {
        return;
    }

    if (!isButtonCreated) {
        CreateButton(grid);
        isButtonCreated = true;
    }
    if (button) {
        UpdateButton(grid);
    }
}

// ---- hooks ------------------------------------------------------------------

struct HookSite {
    uint32_t rva;
    int length;
    uint8_t expected[10];
};

static const HookSite SITE_KEY = {
    0x0007E753,
    6,
    { 0x0F, 0xB6, 0xC3, 0x83, 0xC0, 0xF7 }
};

extern "C" {
    uint32_t jmp_back_key = 0;
}

struct VtableSite {
    uint32_t rva;
    uint32_t expected;
};

// InventoryWidget vtable 0x702BB4, slot 1 = draw at 0x4C2050
static const VtableSite SITE_DRAW = { 0x00302BB8, 0x000C2050 };

extern "C" __attribute__((naked)) void HookStubKey()
{
    asm("pushad");                              // push registers to stack
    asm("push ebx");                            // key, 2nd argument for next function call
    asm("push edi");                            // GameController, 1st argument for next function call
    asm("call _OnKey");                         // call function to sort on the sort key
    asm("add esp, 8");                          // remove the arguments from the stack
    asm("popad");                               // pops stack back to registers
    asm("movzx eax, bl");                       // original code
    asm("add eax, -9");                         // original code
    asm("jmp dword ptr [_jmp_back_key]");       // jump back
}

// ---- patching ---------------------------------------------------------------
static bool WriteHook(const HookSite& site, void* target)
{
    BYTE* location = (BYTE*)(base + site.rva);
    DWORD oldProtection;
    if (!VirtualProtect(location, site.length, PAGE_EXECUTE_READWRITE, &oldProtection)) {
        return false;
    }
    location[0] = 0xE9; // jmp rel32
    *(uint32_t*)(location + 1) = (uint32_t)target - (uint32_t)location - 5;
    for (int i = 5; i < site.length; i++) {
        location[i] = 0x90; // no-op the rest
    }
    VirtualProtect(location, site.length, oldProtection, &oldProtection);
    FlushInstructionCache(GetCurrentProcess(), location, site.length);
    return true;
}

static void* WriteVtableHook(const VtableSite& site, void* target)
{
    uint32_t* slot = (uint32_t*)(base + site.rva);
    if (*slot != base + site.expected) {
        return NULL;
    }
    DWORD oldProtection;
    if (!VirtualProtect(slot, 4, PAGE_READWRITE, &oldProtection)) {
        return NULL;
    }
    void* original = (void*)*slot;
    *slot = (uint32_t)target;
    VirtualProtect(slot, 4, oldProtection, &oldProtection);
    return original;
}

extern "C" __declspec(dllexport) BOOL APIENTRY DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    if (fdwReason != DLL_PROCESS_ATTACH) {
        return TRUE;
    }

    base = (uint32_t)GetModuleHandle(NULL);
    RefreshA      = (RefreshFn)      (base + RVA_REFRESH_A);
    RefreshB      = (RefreshFn)      (base + RVA_REFRESH_B);
    PlayUiSound   = (PlaySoundFn)    (base + RVA_PLAY_SOUND);
    CloneNode     = (CloneFn)        (base + RVA_NODE_CLONE);
    AddChild      = (AddChildFn)     (base + RVA_ADD_CHILD);
    Connect       = (ConnectFn)      (base + RVA_CONNECT);
    WStringCreate = (WStringFn)      (base + RVA_WSTRING);
    WStringFree   = (WStringFreeFn)  (base + RVA_WSTRING_FREE);
    FindNode      = (FindNodeFn)     (base + RVA_FIND_NODE);
    SetColor      = (SetColorFn)     (base + RVA_SET_COLOR);
    IsPointerOver = (IsPointerOverFn)(base + RVA_POINTER_OVER);
    ViewWidth     = (SizeFn)         (base + RVA_VIEW_WIDTH);
    ViewHeight    = (SizeFn)         (base + RVA_VIEW_HEIGHT);
    MoveTo        = (MoveToFn)       (base + RVA_MOVE_TO);

    // each hook on its own, only patched if its site is valid
    if (HOOK_KEY && memcmp((BYTE*)(base + SITE_KEY.rva), SITE_KEY.expected, SITE_KEY.length) == 0) {
        jmp_back_key = base + SITE_KEY.rva + SITE_KEY.length;
        WriteHook(SITE_KEY, (void*)&HookStubKey);
    }

    // the button draws itself from the inventory window's draw
    if (HOOK_BUTTON) {
        DrawOriginal = (DrawFn)WriteVtableHook(SITE_DRAW, (void*)&DrawHook);
    }
    return TRUE;
}
