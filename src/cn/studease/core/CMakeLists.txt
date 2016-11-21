
#core files
aux_source_directory(. DIR_LIB_CORE_SRCS)

#build core
add_library(core ${DIR_LIB_CORE_SRCS})