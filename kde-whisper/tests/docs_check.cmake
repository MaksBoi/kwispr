set(REQUIRED_FILES
    "${SOURCE_DIR}/../README.md"
    "${SOURCE_DIR}/../docs/kde-whisper.md"
    "${SOURCE_DIR}/README.md"
    "${SOURCE_DIR}/scripts/podman-test.sh"
)

foreach(path IN LISTS REQUIRED_FILES)
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "Required docs/check file is missing: ${path}")
    endif()
endforeach()

function(require_contains path needle)
    file(READ "${path}" content)
    string(FIND "${content}" "${needle}" found_at)
    if(found_at EQUAL -1)
        message(FATAL_ERROR "${path} must contain: ${needle}")
    endif()
endfunction()

set(ROOT_README "${SOURCE_DIR}/../README.md")
set(DOCS_MD "${SOURCE_DIR}/../docs/kde-whisper.md")
set(APP_README "${SOURCE_DIR}/README.md")

foreach(path IN ITEMS "${ROOT_README}" "${DOCS_MD}" "${APP_README}")
    require_contains("${path}" "./kde-whisper/scripts/podman-test.sh")
    require_contains("${path}" "kde-whisper")
    require_contains("${path}" "kwispr.sh toggle")
    require_contains("${path}" "kwispr-models.py download")
endforeach()

require_contains("${DOCS_MD}" "cmake --install")
require_contains("${DOCS_MD}" "optional")
require_contains("${APP_README}" "QT_QPA_PLATFORM=offscreen")
require_contains("${APP_README}" "KStatusNotifierItem")
require_contains("${SOURCE_DIR}/scripts/podman-test.sh" "QT_QPA_PLATFORM")
