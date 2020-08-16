VPATH = src/common:src/fp:src/pm
FP_CPP_FILES := $(wildcard src/common/*.cpp) $(wildcard src/fp/*.cpp)
FP_OBJ_FILES := $(addprefix obj/,$(notdir $(FP_CPP_FILES:.cpp=.o)))
PM_CPP_FILES := $(wildcard src/common/*.cpp) $(wildcard src/pm/*.cpp)
PM_OBJ_FILES := $(addprefix obj/,$(notdir $(PM_CPP_FILES:.cpp=.o)))

CC_FLAGS := -Wall -std=c++11 -O0 -g
INCLUDE_FLAGS := -Isrc/common
LD_FLAGS := 
DEBUG = 1
TARGET = footprintAnalysis.linux

$(TARGET): $(FP_OBJ_FILES)
	g++ $(LD_FLAGS) -o $@ $^

createpagemap: $(PM_OBJ_FILES)
	g++ $(LD_FLAGS) -o $@ $^

obj/%.o: %.cpp
	g++ $(CC_FLAGS) ${INCLUDE_FLAGS} -c -o $@ $<
debug: $(TARGET)

clean:
	$(RM) $(TARGET) $(FP_OBJ_FILES) $(PM_OBJ_FILES)

# Auomatic dependency graph generation
CC_FLAGS += -MMD
-include $(OBJFILES:.o=.d)
# Automatic variables explained
# https://www.gnu.org/software/make/manual/html_node/Automatic-Variables.html
#
