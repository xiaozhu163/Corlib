# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.22

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:

# Disable VCS-based implicit rules.
% : %,v

# Disable VCS-based implicit rules.
% : RCS/%

# Disable VCS-based implicit rules.
% : RCS/%,v

# Disable VCS-based implicit rules.
% : SCCS/s.%

# Disable VCS-based implicit rules.
% : s.%

.SUFFIXES: .hpux_make_needs_suffix_list

# Command-line flag to silence nested $(MAKE).
$(VERBOSE)MAKESILENT = -s

#Suppress display of executed commands.
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
RM = /usr/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /root/share/coroutine-lib/test/libco

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /root/share/coroutine-lib/test/libco/build

# Include any dependencies generated for this target.
include CMakeFiles/example_specific.dir/depend.make
# Include any dependencies generated by the compiler for this target.
include CMakeFiles/example_specific.dir/compiler_depend.make

# Include the progress variables for this target.
include CMakeFiles/example_specific.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/example_specific.dir/flags.make

CMakeFiles/example_specific.dir/example_specific.cpp.o: CMakeFiles/example_specific.dir/flags.make
CMakeFiles/example_specific.dir/example_specific.cpp.o: ../example_specific.cpp
CMakeFiles/example_specific.dir/example_specific.cpp.o: CMakeFiles/example_specific.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/root/share/coroutine-lib/test/libco/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object CMakeFiles/example_specific.dir/example_specific.cpp.o"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/example_specific.dir/example_specific.cpp.o -MF CMakeFiles/example_specific.dir/example_specific.cpp.o.d -o CMakeFiles/example_specific.dir/example_specific.cpp.o -c /root/share/coroutine-lib/test/libco/example_specific.cpp

CMakeFiles/example_specific.dir/example_specific.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/example_specific.dir/example_specific.cpp.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /root/share/coroutine-lib/test/libco/example_specific.cpp > CMakeFiles/example_specific.dir/example_specific.cpp.i

CMakeFiles/example_specific.dir/example_specific.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/example_specific.dir/example_specific.cpp.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /root/share/coroutine-lib/test/libco/example_specific.cpp -o CMakeFiles/example_specific.dir/example_specific.cpp.s

# Object files for target example_specific
example_specific_OBJECTS = \
"CMakeFiles/example_specific.dir/example_specific.cpp.o"

# External object files for target example_specific
example_specific_EXTERNAL_OBJECTS =

example_specific: CMakeFiles/example_specific.dir/example_specific.cpp.o
example_specific: CMakeFiles/example_specific.dir/build.make
example_specific: libcolib.so
example_specific: CMakeFiles/example_specific.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/root/share/coroutine-lib/test/libco/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking CXX executable example_specific"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/example_specific.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/example_specific.dir/build: example_specific
.PHONY : CMakeFiles/example_specific.dir/build

CMakeFiles/example_specific.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/example_specific.dir/cmake_clean.cmake
.PHONY : CMakeFiles/example_specific.dir/clean

CMakeFiles/example_specific.dir/depend:
	cd /root/share/coroutine-lib/test/libco/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /root/share/coroutine-lib/test/libco /root/share/coroutine-lib/test/libco /root/share/coroutine-lib/test/libco/build /root/share/coroutine-lib/test/libco/build /root/share/coroutine-lib/test/libco/build/CMakeFiles/example_specific.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/example_specific.dir/depend

