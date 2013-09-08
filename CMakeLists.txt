set(LLVM_LINK_COMPONENTS
  ${LLVM_TARGETS_TO_BUILD}
  asmparser
  bitreader
  support
  mc
  )

ADD_DEFINITIONS("-std=c++11")
set(CMAKE_CXX_FLAGS
   "${CMAKE_CXX_FLAGS} ${LLVM_COMPILE_FLAGS} ${LLVM_LDFLAGS} -std=c++11"
   )

add_clang_executable(clang-scope
    ClangScope.cpp
    Indexer.cpp
  )

target_link_libraries(clang-scope
  clangTooling
  clangBasic
  )

install(TARGETS clang-scope
  RUNTIME DESTINATION bin)
