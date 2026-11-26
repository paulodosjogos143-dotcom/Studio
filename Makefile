# ROXBOX - Linux headless verification build (no GPU/driver needed)
# Compiles the full engine + Lua + smoke tests with a stub GL backend.
CXX      ?= g++
CC       ?= gcc
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wno-unused-parameter -DROXBOX_HEADLESS
CFLAGS   ?= -O2 -Wall -fPIC
INCLUDES  = -Iengine -Iengine/render -Ithird_party/lua

LUA_SRCS = lapi.c lauxlib.c lbaselib.c lcode.c lcorolib.c lctype.c ldblib.c ldebug.c \
           ldo.c ldump.c lfunc.c lgc.c linit.c liolib.c llex.c lmathlib.c lmem.c \
           loadlib.c lobject.c lopcodes.c loslib.c lparser.c lstate.c lstring.c \
           lstrlib.c ltable.c ltablib.c ltm.c lvm.c lutf8lib.c lundump.c lzio.c

ENGINE_SRCS = \
	engine/core/log.cpp \
	engine/core/math.cpp \
	engine/core/time.cpp \
	engine/core/fs.cpp \
	engine/world/voxelworld.cpp \
	engine/mesh/mesher.cpp \
	engine/render/camera.cpp \
	engine/render/textureatlas.cpp \
	engine/render/quality.cpp \
	engine/render/gl_backend.cpp \
	engine/render/renderer.cpp \
	engine/scripting/luahost.cpp \
	engine/scripting/robloxapi.cpp \
	engine/ui/font.cpp \
	engine/ui/ui.cpp \
	engine/platform/null_window.cpp \
	engine/engine.cpp

OBJS = $(ENGINE_SRCS:.cpp=.o) $(addprefix third_party/lua/,$(LUA_SRCS:.c=.o))

all: smoke_test studio_headless player_headless

smoke_test: tests/smoke_test.cpp $(OBJS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ tests/smoke_test.cpp $(OBJS)

studio_headless: studio/main.cpp studio/studio.cpp $(OBJS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ studio/main.cpp studio/studio.cpp $(OBJS)

player_headless: player/main.cpp $(OBJS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ player/main.cpp $(OBJS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

third_party/lua/%.o: third_party/lua/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

run: smoke_test
	./smoke_test

clean:
	rm -f $(OBJS) smoke_test

.PHONY: all run clean
