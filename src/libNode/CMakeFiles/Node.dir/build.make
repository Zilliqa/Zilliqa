# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.5

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:


# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list


# Suppress display of executed commands.
$(VERBOSE).SILENT:


# A target that is always out of date.
cmake_force:

.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/junhao/Desktop/octcoin/other_br/production/zilliqa

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/junhao/Desktop/octcoin/other_br/production/zilliqa

# Include any dependencies generated for this target.
include src/libNode/CMakeFiles/Node.dir/depend.make

# Include the progress variables for this target.
include src/libNode/CMakeFiles/Node.dir/progress.make

# Include the compile flags for this target's objects.
include src/libNode/CMakeFiles/Node.dir/flags.make

src/libNode/CMakeFiles/Node.dir/DSBlockProcessing.cpp.o: src/libNode/CMakeFiles/Node.dir/flags.make
src/libNode/CMakeFiles/Node.dir/DSBlockProcessing.cpp.o: src/libNode/DSBlockProcessing.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/junhao/Desktop/octcoin/other_br/production/zilliqa/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object src/libNode/CMakeFiles/Node.dir/DSBlockProcessing.cpp.o"
	cd /home/junhao/Desktop/octcoin/other_br/production/zilliqa/src/libNode && /usr/bin/c++   $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/Node.dir/DSBlockProcessing.cpp.o -c /home/junhao/Desktop/octcoin/other_br/production/zilliqa/src/libNode/DSBlockProcessing.cpp

src/libNode/CMakeFiles/Node.dir/DSBlockProcessing.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/Node.dir/DSBlockProcessing.cpp.i"
	cd /home/junhao/Desktop/octcoin/other_br/production/zilliqa/src/libNode && /usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/junhao/Desktop/octcoin/other_br/production/zilliqa/src/libNode/DSBlockProcessing.cpp > CMakeFiles/Node.dir/DSBlockProcessing.cpp.i

src/libNode/CMakeFiles/Node.dir/DSBlockProcessing.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/Node.dir/DSBlockProcessing.cpp.s"
	cd /home/junhao/Desktop/octcoin/other_br/production/zilliqa/src/libNode && /usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/junhao/Desktop/octcoin/other_br/production/zilliqa/src/libNode/DSBlockProcessing.cpp -o CMakeFiles/Node.dir/DSBlockProcessing.cpp.s

src/libNode/CMakeFiles/Node.dir/DSBlockProcessing.cpp.o.requires:

.PHONY : src/libNode/CMakeFiles/Node.dir/DSBlockProcessing.cpp.o.requires

src/libNode/CMakeFiles/Node.dir/DSBlockProcessing.cpp.o.provides: src/libNode/CMakeFiles/Node.dir/DSBlockProcessing.cpp.o.requires
	$(MAKE) -f src/libNode/CMakeFiles/Node.dir/build.make src/libNode/CMakeFiles/Node.dir/DSBlockProcessing.cpp.o.provides.build
.PHONY : src/libNode/CMakeFiles/Node.dir/DSBlockProcessing.cpp.o.provides

src/libNode/CMakeFiles/Node.dir/DSBlockProcessing.cpp.o.provides.build: src/libNode/CMakeFiles/Node.dir/DSBlockProcessing.cpp.o


src/libNode/CMakeFiles/Node.dir/FinalBlockProcessing.cpp.o: src/libNode/CMakeFiles/Node.dir/flags.make
src/libNode/CMakeFiles/Node.dir/FinalBlockProcessing.cpp.o: src/libNode/FinalBlockProcessing.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/junhao/Desktop/octcoin/other_br/production/zilliqa/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Building CXX object src/libNode/CMakeFiles/Node.dir/FinalBlockProcessing.cpp.o"
	cd /home/junhao/Desktop/octcoin/other_br/production/zilliqa/src/libNode && /usr/bin/c++   $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/Node.dir/FinalBlockProcessing.cpp.o -c /home/junhao/Desktop/octcoin/other_br/production/zilliqa/src/libNode/FinalBlockProcessing.cpp

src/libNode/CMakeFiles/Node.dir/FinalBlockProcessing.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/Node.dir/FinalBlockProcessing.cpp.i"
	cd /home/junhao/Desktop/octcoin/other_br/production/zilliqa/src/libNode && /usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/junhao/Desktop/octcoin/other_br/production/zilliqa/src/libNode/FinalBlockProcessing.cpp > CMakeFiles/Node.dir/FinalBlockProcessing.cpp.i

src/libNode/CMakeFiles/Node.dir/FinalBlockProcessing.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/Node.dir/FinalBlockProcessing.cpp.s"
	cd /home/junhao/Desktop/octcoin/other_br/production/zilliqa/src/libNode && /usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/junhao/Desktop/octcoin/other_br/production/zilliqa/src/libNode/FinalBlockProcessing.cpp -o CMakeFiles/Node.dir/FinalBlockProcessing.cpp.s

src/libNode/CMakeFiles/Node.dir/FinalBlockProcessing.cpp.o.requires:

.PHONY : src/libNode/CMakeFiles/Node.dir/FinalBlockProcessing.cpp.o.requires

src/libNode/CMakeFiles/Node.dir/FinalBlockProcessing.cpp.o.provides: src/libNode/CMakeFiles/Node.dir/FinalBlockProcessing.cpp.o.requires
	$(MAKE) -f src/libNode/CMakeFiles/Node.dir/build.make src/libNode/CMakeFiles/Node.dir/FinalBlockProcessing.cpp.o.provides.build
.PHONY : src/libNode/CMakeFiles/Node.dir/FinalBlockProcessing.cpp.o.provides

src/libNode/CMakeFiles/Node.dir/FinalBlockProcessing.cpp.o.provides.build: src/libNode/CMakeFiles/Node.dir/FinalBlockProcessing.cpp.o


src/libNode/CMakeFiles/Node.dir/MicroBlockProcessing.cpp.o: src/libNode/CMakeFiles/Node.dir/flags.make
src/libNode/CMakeFiles/Node.dir/MicroBlockProcessing.cpp.o: src/libNode/MicroBlockProcessing.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/junhao/Desktop/octcoin/other_br/production/zilliqa/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Building CXX object src/libNode/CMakeFiles/Node.dir/MicroBlockProcessing.cpp.o"
	cd /home/junhao/Desktop/octcoin/other_br/production/zilliqa/src/libNode && /usr/bin/c++   $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/Node.dir/MicroBlockProcessing.cpp.o -c /home/junhao/Desktop/octcoin/other_br/production/zilliqa/src/libNode/MicroBlockProcessing.cpp

src/libNode/CMakeFiles/Node.dir/MicroBlockProcessing.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/Node.dir/MicroBlockProcessing.cpp.i"
	cd /home/junhao/Desktop/octcoin/other_br/production/zilliqa/src/libNode && /usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/junhao/Desktop/octcoin/other_br/production/zilliqa/src/libNode/MicroBlockProcessing.cpp > CMakeFiles/Node.dir/MicroBlockProcessing.cpp.i

src/libNode/CMakeFiles/Node.dir/MicroBlockProcessing.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/Node.dir/MicroBlockProcessing.cpp.s"
	cd /home/junhao/Desktop/octcoin/other_br/production/zilliqa/src/libNode && /usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/junhao/Desktop/octcoin/other_br/production/zilliqa/src/libNode/MicroBlockProcessing.cpp -o CMakeFiles/Node.dir/MicroBlockProcessing.cpp.s

src/libNode/CMakeFiles/Node.dir/MicroBlockProcessing.cpp.o.requires:

.PHONY : src/libNode/CMakeFiles/Node.dir/MicroBlockProcessing.cpp.o.requires

src/libNode/CMakeFiles/Node.dir/MicroBlockProcessing.cpp.o.provides: src/libNode/CMakeFiles/Node.dir/MicroBlockProcessing.cpp.o.requires
	$(MAKE) -f src/libNode/CMakeFiles/Node.dir/build.make src/libNode/CMakeFiles/Node.dir/MicroBlockProcessing.cpp.o.provides.build
.PHONY : src/libNode/CMakeFiles/Node.dir/MicroBlockProcessing.cpp.o.provides

src/libNode/CMakeFiles/Node.dir/MicroBlockProcessing.cpp.o.provides.build: src/libNode/CMakeFiles/Node.dir/MicroBlockProcessing.cpp.o


src/libNode/CMakeFiles/Node.dir/Node.cpp.o: src/libNode/CMakeFiles/Node.dir/flags.make
src/libNode/CMakeFiles/Node.dir/Node.cpp.o: src/libNode/Node.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/junhao/Desktop/octcoin/other_br/production/zilliqa/CMakeFiles --progress-num=$(CMAKE_PROGRESS_4) "Building CXX object src/libNode/CMakeFiles/Node.dir/Node.cpp.o"
	cd /home/junhao/Desktop/octcoin/other_br/production/zilliqa/src/libNode && /usr/bin/c++   $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/Node.dir/Node.cpp.o -c /home/junhao/Desktop/octcoin/other_br/production/zilliqa/src/libNode/Node.cpp

src/libNode/CMakeFiles/Node.dir/Node.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/Node.dir/Node.cpp.i"
	cd /home/junhao/Desktop/octcoin/other_br/production/zilliqa/src/libNode && /usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/junhao/Desktop/octcoin/other_br/production/zilliqa/src/libNode/Node.cpp > CMakeFiles/Node.dir/Node.cpp.i

src/libNode/CMakeFiles/Node.dir/Node.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/Node.dir/Node.cpp.s"
	cd /home/junhao/Desktop/octcoin/other_br/production/zilliqa/src/libNode && /usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/junhao/Desktop/octcoin/other_br/production/zilliqa/src/libNode/Node.cpp -o CMakeFiles/Node.dir/Node.cpp.s

src/libNode/CMakeFiles/Node.dir/Node.cpp.o.requires:

.PHONY : src/libNode/CMakeFiles/Node.dir/Node.cpp.o.requires

src/libNode/CMakeFiles/Node.dir/Node.cpp.o.provides: src/libNode/CMakeFiles/Node.dir/Node.cpp.o.requires
	$(MAKE) -f src/libNode/CMakeFiles/Node.dir/build.make src/libNode/CMakeFiles/Node.dir/Node.cpp.o.provides.build
.PHONY : src/libNode/CMakeFiles/Node.dir/Node.cpp.o.provides

src/libNode/CMakeFiles/Node.dir/Node.cpp.o.provides.build: src/libNode/CMakeFiles/Node.dir/Node.cpp.o


src/libNode/CMakeFiles/Node.dir/PoW1Processing.cpp.o: src/libNode/CMakeFiles/Node.dir/flags.make
src/libNode/CMakeFiles/Node.dir/PoW1Processing.cpp.o: src/libNode/PoW1Processing.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/junhao/Desktop/octcoin/other_br/production/zilliqa/CMakeFiles --progress-num=$(CMAKE_PROGRESS_5) "Building CXX object src/libNode/CMakeFiles/Node.dir/PoW1Processing.cpp.o"
	cd /home/junhao/Desktop/octcoin/other_br/production/zilliqa/src/libNode && /usr/bin/c++   $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/Node.dir/PoW1Processing.cpp.o -c /home/junhao/Desktop/octcoin/other_br/production/zilliqa/src/libNode/PoW1Processing.cpp

src/libNode/CMakeFiles/Node.dir/PoW1Processing.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/Node.dir/PoW1Processing.cpp.i"
	cd /home/junhao/Desktop/octcoin/other_br/production/zilliqa/src/libNode && /usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/junhao/Desktop/octcoin/other_br/production/zilliqa/src/libNode/PoW1Processing.cpp > CMakeFiles/Node.dir/PoW1Processing.cpp.i

src/libNode/CMakeFiles/Node.dir/PoW1Processing.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/Node.dir/PoW1Processing.cpp.s"
	cd /home/junhao/Desktop/octcoin/other_br/production/zilliqa/src/libNode && /usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/junhao/Desktop/octcoin/other_br/production/zilliqa/src/libNode/PoW1Processing.cpp -o CMakeFiles/Node.dir/PoW1Processing.cpp.s

src/libNode/CMakeFiles/Node.dir/PoW1Processing.cpp.o.requires:

.PHONY : src/libNode/CMakeFiles/Node.dir/PoW1Processing.cpp.o.requires

src/libNode/CMakeFiles/Node.dir/PoW1Processing.cpp.o.provides: src/libNode/CMakeFiles/Node.dir/PoW1Processing.cpp.o.requires
	$(MAKE) -f src/libNode/CMakeFiles/Node.dir/build.make src/libNode/CMakeFiles/Node.dir/PoW1Processing.cpp.o.provides.build
.PHONY : src/libNode/CMakeFiles/Node.dir/PoW1Processing.cpp.o.provides

src/libNode/CMakeFiles/Node.dir/PoW1Processing.cpp.o.provides.build: src/libNode/CMakeFiles/Node.dir/PoW1Processing.cpp.o


src/libNode/CMakeFiles/Node.dir/PoW2Processing.cpp.o: src/libNode/CMakeFiles/Node.dir/flags.make
src/libNode/CMakeFiles/Node.dir/PoW2Processing.cpp.o: src/libNode/PoW2Processing.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/junhao/Desktop/octcoin/other_br/production/zilliqa/CMakeFiles --progress-num=$(CMAKE_PROGRESS_6) "Building CXX object src/libNode/CMakeFiles/Node.dir/PoW2Processing.cpp.o"
	cd /home/junhao/Desktop/octcoin/other_br/production/zilliqa/src/libNode && /usr/bin/c++   $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/Node.dir/PoW2Processing.cpp.o -c /home/junhao/Desktop/octcoin/other_br/production/zilliqa/src/libNode/PoW2Processing.cpp

src/libNode/CMakeFiles/Node.dir/PoW2Processing.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/Node.dir/PoW2Processing.cpp.i"
	cd /home/junhao/Desktop/octcoin/other_br/production/zilliqa/src/libNode && /usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/junhao/Desktop/octcoin/other_br/production/zilliqa/src/libNode/PoW2Processing.cpp > CMakeFiles/Node.dir/PoW2Processing.cpp.i

src/libNode/CMakeFiles/Node.dir/PoW2Processing.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/Node.dir/PoW2Processing.cpp.s"
	cd /home/junhao/Desktop/octcoin/other_br/production/zilliqa/src/libNode && /usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/junhao/Desktop/octcoin/other_br/production/zilliqa/src/libNode/PoW2Processing.cpp -o CMakeFiles/Node.dir/PoW2Processing.cpp.s

src/libNode/CMakeFiles/Node.dir/PoW2Processing.cpp.o.requires:

.PHONY : src/libNode/CMakeFiles/Node.dir/PoW2Processing.cpp.o.requires

src/libNode/CMakeFiles/Node.dir/PoW2Processing.cpp.o.provides: src/libNode/CMakeFiles/Node.dir/PoW2Processing.cpp.o.requires
	$(MAKE) -f src/libNode/CMakeFiles/Node.dir/build.make src/libNode/CMakeFiles/Node.dir/PoW2Processing.cpp.o.provides.build
.PHONY : src/libNode/CMakeFiles/Node.dir/PoW2Processing.cpp.o.provides

src/libNode/CMakeFiles/Node.dir/PoW2Processing.cpp.o.provides.build: src/libNode/CMakeFiles/Node.dir/PoW2Processing.cpp.o


src/libNode/CMakeFiles/Node.dir/ShardingInfoProcessing.cpp.o: src/libNode/CMakeFiles/Node.dir/flags.make
src/libNode/CMakeFiles/Node.dir/ShardingInfoProcessing.cpp.o: src/libNode/ShardingInfoProcessing.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/junhao/Desktop/octcoin/other_br/production/zilliqa/CMakeFiles --progress-num=$(CMAKE_PROGRESS_7) "Building CXX object src/libNode/CMakeFiles/Node.dir/ShardingInfoProcessing.cpp.o"
	cd /home/junhao/Desktop/octcoin/other_br/production/zilliqa/src/libNode && /usr/bin/c++   $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/Node.dir/ShardingInfoProcessing.cpp.o -c /home/junhao/Desktop/octcoin/other_br/production/zilliqa/src/libNode/ShardingInfoProcessing.cpp

src/libNode/CMakeFiles/Node.dir/ShardingInfoProcessing.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/Node.dir/ShardingInfoProcessing.cpp.i"
	cd /home/junhao/Desktop/octcoin/other_br/production/zilliqa/src/libNode && /usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/junhao/Desktop/octcoin/other_br/production/zilliqa/src/libNode/ShardingInfoProcessing.cpp > CMakeFiles/Node.dir/ShardingInfoProcessing.cpp.i

src/libNode/CMakeFiles/Node.dir/ShardingInfoProcessing.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/Node.dir/ShardingInfoProcessing.cpp.s"
	cd /home/junhao/Desktop/octcoin/other_br/production/zilliqa/src/libNode && /usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/junhao/Desktop/octcoin/other_br/production/zilliqa/src/libNode/ShardingInfoProcessing.cpp -o CMakeFiles/Node.dir/ShardingInfoProcessing.cpp.s

src/libNode/CMakeFiles/Node.dir/ShardingInfoProcessing.cpp.o.requires:

.PHONY : src/libNode/CMakeFiles/Node.dir/ShardingInfoProcessing.cpp.o.requires

src/libNode/CMakeFiles/Node.dir/ShardingInfoProcessing.cpp.o.provides: src/libNode/CMakeFiles/Node.dir/ShardingInfoProcessing.cpp.o.requires
	$(MAKE) -f src/libNode/CMakeFiles/Node.dir/build.make src/libNode/CMakeFiles/Node.dir/ShardingInfoProcessing.cpp.o.provides.build
.PHONY : src/libNode/CMakeFiles/Node.dir/ShardingInfoProcessing.cpp.o.provides

src/libNode/CMakeFiles/Node.dir/ShardingInfoProcessing.cpp.o.provides.build: src/libNode/CMakeFiles/Node.dir/ShardingInfoProcessing.cpp.o


# Object files for target Node
Node_OBJECTS = \
"CMakeFiles/Node.dir/DSBlockProcessing.cpp.o" \
"CMakeFiles/Node.dir/FinalBlockProcessing.cpp.o" \
"CMakeFiles/Node.dir/MicroBlockProcessing.cpp.o" \
"CMakeFiles/Node.dir/Node.cpp.o" \
"CMakeFiles/Node.dir/PoW1Processing.cpp.o" \
"CMakeFiles/Node.dir/PoW2Processing.cpp.o" \
"CMakeFiles/Node.dir/ShardingInfoProcessing.cpp.o"

# External object files for target Node
Node_EXTERNAL_OBJECTS =

src/libNode/libNode.a: src/libNode/CMakeFiles/Node.dir/DSBlockProcessing.cpp.o
src/libNode/libNode.a: src/libNode/CMakeFiles/Node.dir/FinalBlockProcessing.cpp.o
src/libNode/libNode.a: src/libNode/CMakeFiles/Node.dir/MicroBlockProcessing.cpp.o
src/libNode/libNode.a: src/libNode/CMakeFiles/Node.dir/Node.cpp.o
src/libNode/libNode.a: src/libNode/CMakeFiles/Node.dir/PoW1Processing.cpp.o
src/libNode/libNode.a: src/libNode/CMakeFiles/Node.dir/PoW2Processing.cpp.o
src/libNode/libNode.a: src/libNode/CMakeFiles/Node.dir/ShardingInfoProcessing.cpp.o
src/libNode/libNode.a: src/libNode/CMakeFiles/Node.dir/build.make
src/libNode/libNode.a: src/libNode/CMakeFiles/Node.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/junhao/Desktop/octcoin/other_br/production/zilliqa/CMakeFiles --progress-num=$(CMAKE_PROGRESS_8) "Linking CXX static library libNode.a"
	cd /home/junhao/Desktop/octcoin/other_br/production/zilliqa/src/libNode && $(CMAKE_COMMAND) -P CMakeFiles/Node.dir/cmake_clean_target.cmake
	cd /home/junhao/Desktop/octcoin/other_br/production/zilliqa/src/libNode && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/Node.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
src/libNode/CMakeFiles/Node.dir/build: src/libNode/libNode.a

.PHONY : src/libNode/CMakeFiles/Node.dir/build

src/libNode/CMakeFiles/Node.dir/requires: src/libNode/CMakeFiles/Node.dir/DSBlockProcessing.cpp.o.requires
src/libNode/CMakeFiles/Node.dir/requires: src/libNode/CMakeFiles/Node.dir/FinalBlockProcessing.cpp.o.requires
src/libNode/CMakeFiles/Node.dir/requires: src/libNode/CMakeFiles/Node.dir/MicroBlockProcessing.cpp.o.requires
src/libNode/CMakeFiles/Node.dir/requires: src/libNode/CMakeFiles/Node.dir/Node.cpp.o.requires
src/libNode/CMakeFiles/Node.dir/requires: src/libNode/CMakeFiles/Node.dir/PoW1Processing.cpp.o.requires
src/libNode/CMakeFiles/Node.dir/requires: src/libNode/CMakeFiles/Node.dir/PoW2Processing.cpp.o.requires
src/libNode/CMakeFiles/Node.dir/requires: src/libNode/CMakeFiles/Node.dir/ShardingInfoProcessing.cpp.o.requires

.PHONY : src/libNode/CMakeFiles/Node.dir/requires

src/libNode/CMakeFiles/Node.dir/clean:
	cd /home/junhao/Desktop/octcoin/other_br/production/zilliqa/src/libNode && $(CMAKE_COMMAND) -P CMakeFiles/Node.dir/cmake_clean.cmake
.PHONY : src/libNode/CMakeFiles/Node.dir/clean

src/libNode/CMakeFiles/Node.dir/depend:
	cd /home/junhao/Desktop/octcoin/other_br/production/zilliqa && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/junhao/Desktop/octcoin/other_br/production/zilliqa /home/junhao/Desktop/octcoin/other_br/production/zilliqa/src/libNode /home/junhao/Desktop/octcoin/other_br/production/zilliqa /home/junhao/Desktop/octcoin/other_br/production/zilliqa/src/libNode /home/junhao/Desktop/octcoin/other_br/production/zilliqa/src/libNode/CMakeFiles/Node.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : src/libNode/CMakeFiles/Node.dir/depend

