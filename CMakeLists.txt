cmake_minimum_required(VERSION 3.6)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
  else()
    message(FATAL_ERROR "Please define VITASDK to point to your SDK path!")
  endif()
endif()

project(vitabtfix)
include("${VITASDK}/share/vita.cmake" REQUIRED)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -std=gnu99 -Wall -Wextra -fno-builtin")

add_executable(${PROJECT_NAME}
  src/main.c
  src/log.c
  src/config.c
)

include_directories(src)

target_link_libraries(${PROJECT_NAME}
  SceIofilemgrForDriver_stub
  # 3.60-only build: ksceKernelGetModuleInfo is 0xD269F915 in the 3.60 NID
  # set (SceModulemgrForKernel), vs 0xDAA90093 on 3.63+. Linking the _363
  # stub leaves the import unresolved on 3.60 and the plugin silently fails
  # to load.
  SceModulemgrForKernel_stub
  SceSysclibForDriver_stub
  SceSysmemForDriver_stub
  SceThreadmgrForDriver_stub
  taihenForKernel_stub
)

set_target_properties(${PROJECT_NAME}
  PROPERTIES LINK_FLAGS "-nostdlib"
)

vita_create_self(${PROJECT_NAME}.skprx ${PROJECT_NAME}
  CONFIG vitabtfix.yml
  UNSAFE
)