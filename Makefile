CXX = g++

CXXFLAGS = -O2 -masm=intel -Wall
LDFLAGS = -shared -static -static-libgcc -static-libstdc++ -s

.PHONY: all
all: InventorySortMod.dll InventorySortKeyMod.dll InventorySortButtonMod.dll

InventorySortMod.dll: main.cpp cube.h
	$(CXX) $(CXXFLAGS) -DHOOK_KEY -DHOOK_BUTTON -o $@ $< $(LDFLAGS)

InventorySortKeyMod.dll: main.cpp cube.h
	$(CXX) $(CXXFLAGS) -DHOOK_KEY -o $@ $< $(LDFLAGS)

InventorySortButtonMod.dll: main.cpp cube.h
	$(CXX) $(CXXFLAGS) -DHOOK_BUTTON -o $@ $< $(LDFLAGS)
