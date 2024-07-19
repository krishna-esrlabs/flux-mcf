function(install_targets)
    set(options)
    set(oneValueArgs EXPORT_TARGET_NAME)
    set(multiValueArgs ADDITIONAL_TARGET_NAMES)
    cmake_parse_arguments(install_targets "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    message(STATUS "Export object name: " ${install_targets_EXPORT_TARGET_NAME})
    message(STATUS "Additional object names: " ${install_targets_ADDITIONAL_TARGET_NAMES})

    include(GNUInstallDirs)

    set(TARGETS_NAME        ${install_targets_EXPORT_TARGET_NAME}Targets)
    set(INSTALL_CONFIGDIR   ${CMAKE_INSTALL_LIBDIR}/cmake/${install_targets_EXPORT_TARGET_NAME})
    set(CONFIG_FILE_IN      ${CMAKE_CURRENT_LIST_DIR}/cmake/${install_targets_EXPORT_TARGET_NAME}Config.cmake.in)
    set(CONFIG_FILE         ${CMAKE_CURRENT_BINARY_DIR}/${install_targets_EXPORT_TARGET_NAME}Config.cmake)

    # Specify rules for installing project targets e.g. libraries, executables etc. and associate them
    # with the export.
    install(TARGETS ${install_targets_EXPORT_TARGET_NAME} ${install_targets_ADDITIONAL_TARGET_NAMES}
        EXPORT ${TARGETS_NAME}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    # Install the target header files
    install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

    # Install the export specified above to a file which specifies how other packages can import the targets.
    install(EXPORT ${TARGETS_NAME}
        FILE
            ${TARGETS_NAME}.cmake
        NAMESPACE
            ${install_targets_EXPORT_TARGET_NAME}::
        DESTINATION
            ${INSTALL_CONFIGDIR}
    )

    # Include package to get access to some helper functions for creating config files
    include(CMakePackageConfigHelpers)

    # Generate a config file so that other packages can find this project using find_package()
    configure_package_config_file(${CONFIG_FILE_IN}
            ${CONFIG_FILE}
        INSTALL_DESTINATION 
            ${INSTALL_CONFIGDIR}
    )

    # Install the config
    install(
        FILES
            ${CONFIG_FILE}
        DESTINATION
            ${INSTALL_CONFIGDIR}
    )
endfunction()