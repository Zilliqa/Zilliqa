#TODO move this file out of the project root
set(GRAPHVIZ_GRAPH_HEADER "node [fontsize = \"80\"];")

set(GRAPHVIZ_EXECUTABLES OFF)   # Export executables (default: ON)
set(GRAPHVIZ_STATIC_LIBS ON)    # Export static libs (default: ON)
set(GRAPHVIZ_SHARED_LIBS ON)    # Export shared libs (default: ON)
set(GRAPHVIZ_MODULE_LIBS ON)    # Export modules (default: ON)
set(GRAPHVIZ_EXTERNAL_LIBS OFF) # Export external libs (default: ON)
set(GRAPHVIZ_IGNORE_TARGETS "") # Do not export targets in the given list. Since 2.8.5 this supports regular expressions.
