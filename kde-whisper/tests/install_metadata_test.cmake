cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED SOURCE_DIR OR NOT DEFINED BUILD_DIR)
    message(FATAL_ERROR "SOURCE_DIR and BUILD_DIR are required")
endif()

set(DESKTOP_FILE "${SOURCE_DIR}/org.kwispr.KdeWhisper.desktop")
set(METAINFO_FILE "${SOURCE_DIR}/org.kwispr.KdeWhisper.metainfo.xml")
set(ICON_FILE "${SOURCE_DIR}/icons/org.kwispr.KdeWhisper.svg")

foreach(path IN ITEMS "${DESKTOP_FILE}" "${METAINFO_FILE}" "${ICON_FILE}")
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "Required metadata file is missing: ${path}")
    endif()
endforeach()

execute_process(
    COMMAND desktop-file-validate "${DESKTOP_FILE}"
    RESULT_VARIABLE desktop_validate_result
    OUTPUT_VARIABLE desktop_validate_out
    ERROR_VARIABLE desktop_validate_err
)
if(NOT desktop_validate_result EQUAL 0)
    message(FATAL_ERROR "desktop-file-validate failed: ${desktop_validate_out}${desktop_validate_err}")
endif()

file(READ "${DESKTOP_FILE}" desktop_text)
if(NOT desktop_text MATCHES "Exec=kde-whisper")
    message(FATAL_ERROR ".desktop must execute kde-whisper")
endif()
if(NOT desktop_text MATCHES "Tray" OR NOT desktop_text MATCHES "Settings")
    message(FATAL_ERROR ".desktop must advertise tray/settings utility")
endif()
if(desktop_text MATCHES "kwispr.sh toggle")
    message(FATAL_ERROR ".desktop must not hijack the existing hotkey/toggle command")
endif()

set(DESTDIR "${BUILD_DIR}/install-metadata-dest")
file(REMOVE_RECURSE "${DESTDIR}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env "DESTDIR=${DESTDIR}" "${CMAKE_COMMAND}" --install "${BUILD_DIR}" --prefix /usr
    RESULT_VARIABLE install_result
    OUTPUT_VARIABLE install_out
    ERROR_VARIABLE install_err
)
if(NOT install_result EQUAL 0)
    message(FATAL_ERROR "cmake --install failed: ${install_out}${install_err}")
endif()

set(expected_files
    "${DESTDIR}/usr/bin/kde-whisper"
    "${DESTDIR}/usr/share/applications/org.kwispr.KdeWhisper.desktop"
    "${DESTDIR}/usr/share/metainfo/org.kwispr.KdeWhisper.metainfo.xml"
    "${DESTDIR}/usr/share/icons/hicolor/scalable/apps/org.kwispr.KdeWhisper.svg"
)
foreach(path IN LISTS expected_files)
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "Installed file missing: ${path}")
    endif()
endforeach()
