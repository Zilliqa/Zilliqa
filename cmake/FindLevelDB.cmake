# Find LevelDB

find_path(
	LEVELDB_INCLUDE_DIR
	NAMES leveldb/db.h
    DOC "LevelDB include directory"
)

find_library(
	LEVELDB_LIBRARY
	NAMES leveldb
    DOC "LevelDB library"
)

set(LEVELDB_INCLUDE_DIRS ${LEVELDB_INCLUDE_DIR})
set(LEVELDB_LIBRARIES ${LEVELDB_LIBRARY})

if (NOT BUILD_SHARED_LIBS AND APPLE)
	find_path(SNAPPY_INCLUDE_DIR snappy.h PATH_SUFFIXES snappy)
	find_library(SNAPPY_LIBRARY snappy)
	set(LEVELDB_INCLUDE_DIRS ${LEVELDB_INCLUDE_DIR} ${SNAPPY_INCLUDE_DIR})
	set(LEVELDB_LIBRARIES ${LEVELDB_LIBRARY} ${SNAPPY_LIBRARY})
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(leveldb DEFAULT_MSG
	LEVELDB_LIBRARY LEVELDB_INCLUDE_DIR)
