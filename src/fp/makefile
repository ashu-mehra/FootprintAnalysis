CPP_FILES := $(wildcard ../common/*.cpp) $(wildcard *.cpp)
OBJ_FILES := $(addprefix obj/,$(notdir $(CPP_FILES:.cpp=.o)))
CC_FLAGS := -Wall -std=c++11 -O2
LD_FLAGS := 
DEBUG = 1
TARGET = footprintAnalysis.linux
$(TARGET): $(OBJ_FILES)
	g++ $(LD_FLAGS) -o $@ $^


obj/%.o: src/%.cpp
	g++ $(CC_FLAGS) -c -o $@ $<
debug: $(TARGET)

clean:
	$(RM) $(TARGET) $(OBJ_FILES)

# Auomatic dependency graph generation
CC_FLAGS += -MMD
-include $(OBJFILES:.o=.d)
# Automatic variables explained
# https://www.gnu.org/software/make/manual/html_node/Automatic-Variables.html
#
