cmake_minimum_required(VERSION 3.19)
project(calc_module_interface LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets)

qt_standard_project_setup()

qt_add_executable(calc_module_interface
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui

    core/aux_module.cpp
    core/rhythmic_delivery.cpp
    core/pcplp.cpp
    core/aux_module.h
    core/rhythmic_delivery.h
    core/pcplp.h
    plotwindow.h plotwindow.cpp plotwindow.ui
)

target_include_directories(calc_module_interface PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/core
)

target_link_libraries(calc_module_interface
    PRIVATE
        Qt::Core
        Qt::Widgets
)

include(GNUInstallDirs)

install(TARGETS calc_module_interface
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET calc_module_interface
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Charts)

target_link_libraries(calc_module_interface
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt::Charts
)
