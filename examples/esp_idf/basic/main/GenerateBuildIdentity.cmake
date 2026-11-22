get_filename_component(root "${SOURCE_ROOT}" REALPATH)
set(commit "unknown")
set(status "unknown")
find_package(Git QUIET)
if(GIT_FOUND)
  execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${root}" rev-parse --show-toplevel
    OUTPUT_VARIABLE top OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE top_result ERROR_QUIET TIMEOUT 2)
  if(top_result STREQUAL "0")
    get_filename_component(top "${top}" REALPATH)
    if(top STREQUAL root)
      execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${root}" rev-parse --short HEAD
        OUTPUT_VARIABLE head OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE head_result ERROR_QUIET TIMEOUT 2)
      execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${root}" status --porcelain --untracked-files=no
        OUTPUT_VARIABLE changes OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE status_result ERROR_QUIET TIMEOUT 2)
      if(head_result STREQUAL "0" AND status_result STREQUAL "0" AND head MATCHES "^[0-9a-f]+$")
        set(commit "${head}")
        if(changes STREQUAL "")
          set(status "clean")
        else()
          set(status "dirty")
        endif()
      endif()
    endif()
  endif()
endif()
string(TIMESTAMP build_time "%Y-%m-%d %H:%M:%S UTC" UTC)
file(CONFIGURE OUTPUT "${OUTPUT_HEADER}" CONTENT [=[
#pragma once
namespace exampleBuild {
inline constexpr char COMMIT[] = "@commit@";
inline constexpr char STATUS[] = "@status@";
inline constexpr char TIMESTAMP[] = "@build_time@";
}
]=] @ONLY NEWLINE_STYLE UNIX)
