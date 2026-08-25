# Firmware version, derived from the git tag.
#
# Two values have to agree and historically did not: PROJECT_VER became the
# human-readable Matter SoftwareVersionString, PROJECT_VER_NUMBER became the
# SoftwareVersion integer. They were hand-set to "0.1" and 1 while the repo was
# tagged v0.6.0. Deriving both from one source removes the drift.
#
# Encoding: MAJOR*10000 + MINOR*100 + PATCH, so v1.2.3 -> 10203.
#
# SoftwareVersion is a uint32 that OTA compares numerically. It MUST increase
# monotonically or a controller silently declines the update -- there is no
# error, the device simply never updates, which is the hardest kind of failure
# to notice.
#
# The number moves ONLY at tags, which is exactly when an OTA can happen.
# Untagged dev builds keep the tag's number and get a "-dev" suffix on the
# string; they are flashed over USB, never OTA'd, so the number need not move.
#
# Deliberately NOT folding commits-since-tag into PATCH. v0.6.0 plus 63 commits
# would give 663, and a later genuine v0.6.1 would give 601 -- lower than what
# is already on the device. OTA would then refuse, silently, forever.

function(homecadia_derive_version out_ver out_num)
    set(_desc "")
    set(_rc 1)

    find_package(Git QUIET)
    if(GIT_FOUND)
        # safe.directory: the repo is bind-mounted into the build container and
        # owned by a different uid than the container user, which git refuses to
        # act on. Scoped to this one invocation via -c, not persisted.
        execute_process(
            COMMAND ${GIT_EXECUTABLE} -c safe.directory=*
                    describe --tags --long --dirty --match "v[0-9]*"
            WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
            OUTPUT_VARIABLE _desc
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
            RESULT_VARIABLE _rc)
    endif()

    if(NOT _rc EQUAL 0 OR _desc STREQUAL "")
        # No git, no tags, or a shallow clone without tags. Build, but make the
        # version obviously wrong rather than plausibly wrong: 0 is below any
        # deployed version, so an OTA built from this tree is declined instead
        # of shipping an unidentifiable image.
        message(WARNING
            "homecadia: no git tag found (shallow clone? missing tags?). "
            "Using version 0.0.0-untagged / 0. Do NOT build an OTA image from "
            "this tree -- fetch tags first (actions/checkout needs fetch-depth: 0).")
        set(${out_ver} "0.0.0-untagged" PARENT_SCOPE)
        set(${out_num} 0 PARENT_SCOPE)
        return()
    endif()

    if(NOT _desc MATCHES "^v([0-9]+)\\.([0-9]+)\\.([0-9]+)-([0-9]+)-g([0-9a-f]+)(-dirty)?$")
        message(FATAL_ERROR
            "homecadia: cannot parse git describe output '${_desc}'. "
            "Tags must look like vMAJOR.MINOR.PATCH (e.g. v0.7.0).")
    endif()

    set(_major ${CMAKE_MATCH_1})
    set(_minor ${CMAKE_MATCH_2})
    set(_patch ${CMAKE_MATCH_3})
    set(_ahead ${CMAKE_MATCH_4})
    set(_sha   ${CMAKE_MATCH_5})
    set(_dirty ${CMAKE_MATCH_6})

    if(_minor GREATER 99 OR _patch GREATER 99)
        message(FATAL_ERROR
            "homecadia: MINOR and PATCH must each stay under 100 for the "
            "MAJOR*10000 + MINOR*100 + PATCH encoding; got ${_desc}.")
    endif()

    math(EXPR _num "${_major} * 10000 + ${_minor} * 100 + ${_patch}")

    if(_ahead EQUAL 0 AND _dirty STREQUAL "")
        set(_ver "${_major}.${_minor}.${_patch}")
    else()
        set(_ver "${_major}.${_minor}.${_patch}-dev.${_ahead}+${_sha}${_dirty}")
    endif()

    # esp_app_desc_t.version is a 32-byte field; keep clear of the edge.
    string(LENGTH "${_ver}" _len)
    if(_len GREATER 31)
        string(SUBSTRING "${_ver}" 0 31 _ver)
    endif()

    set(${out_ver} "${_ver}" PARENT_SCOPE)
    set(${out_num} ${_num} PARENT_SCOPE)
endfunction()
