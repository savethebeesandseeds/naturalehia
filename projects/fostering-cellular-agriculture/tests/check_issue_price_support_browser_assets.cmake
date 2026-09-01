# SPDX-License-Identifier: MIT

foreach(required_variable IN ITEMS
        BUILT_MJS
        BUILT_WASM
        PUBLISHED_MJS
        PUBLISHED_WASM)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required")
    endif()
endforeach()

function(compare_published_browser_asset label built_file published_file)
    if(NOT EXISTS "${built_file}")
        message(FATAL_ERROR
            "freshly built ${label} is missing: ${built_file}\n"
            "Build cf_issue_price_support_browser before running this test.")
    endif()
    if(NOT EXISTS "${published_file}")
        message(FATAL_ERROR
            "published ${label} is missing: ${published_file}\n"
            "Review the build output, then explicitly build "
            "cf_issue_price_support_browser_assets to publish it.")
    endif()

    execute_process(
        COMMAND
            "${CMAKE_COMMAND}" -E compare_files
            "${built_file}" "${published_file}"
        RESULT_VARIABLE comparison_result
        ERROR_VARIABLE comparison_error
    )
    if(NOT comparison_result EQUAL 0)
        file(SHA256 "${built_file}" built_sha256)
        file(SHA256 "${published_file}" published_sha256)
        message(FATAL_ERROR
            "published ${label} is stale or differs from the current build\n"
            "built:     ${built_file}\n"
            "built sha: ${built_sha256}\n"
            "published: ${published_file}\n"
            "asset sha: ${published_sha256}\n"
            "Review the change, then explicitly build "
            "cf_issue_price_support_browser_assets to publish it.\n"
            "compare_files diagnostic: ${comparison_error}")
    endif()
endfunction()

compare_published_browser_asset("ES module" "${BUILT_MJS}" "${PUBLISHED_MJS}")
compare_published_browser_asset("WebAssembly binary" "${BUILT_WASM}" "${PUBLISHED_WASM}")
