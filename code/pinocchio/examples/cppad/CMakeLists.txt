#
# Copyright (c) 2022 INRIA
#

if(BUILD_WITH_AUTODIFF_SUPPORT AND NOT BUILD_STANDALONE_PYTHON_INTERFACE)
    function(ADD_PINOCCHIO_CPP_AUTODIFF_EXAMPLE EXAMPLE)
        include_directories(SYSTEM ${cppadcg_INCLUDE_DIR})
        add_pinocchio_cpp_example(${EXAMPLE} CPPAD)
        target_link_libraries(
            ${PROJECT_NAME}-example-cpp-${EXAMPLE}
            PUBLIC ${CMAKE_DL_LIBS}
        )
    endfunction()

    add_pinocchio_cpp_autodiff_example(autodiff-rnea)

    if(BUILD_PYTHON_INTERFACE)
        set(${PROJECT_NAME}_AUTODIFF_PYTHON_EXAMPLES autodiff-rnea)

        foreach(EXAMPLE ${${PROJECT_NAME}_AUTODIFF_PYTHON_EXAMPLES})
            set(EXAMPLE_NAME "${PROJECT_NAME}-example-py-${EXAMPLE}")
            add_python_unit_test(${EXAMPLE_NAME} "examples/cppad/${EXAMPLE}.py" "bindings/python")
            add_windows_dll_path_to_test(${EXAMPLE_NAME})
        endforeach()
    endif()
endif()
