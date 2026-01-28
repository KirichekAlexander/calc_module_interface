# cmake_minimum_required(VERSION 3.19)
# project(calc_module_interface LANGUAGES CXX)

# find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets)

# qt_standard_project_setup()

# qt_add_executable(calc_module_interface
#     WIN32 MACOSX_BUNDLE
#     main.cpp
#     mainwindow.cpp
#     mainwindow.h
#     mainwindow.ui

#     core/aux_module.cpp
#     core/rhythmic_delivery.cpp
#     core/pcplp.cpp
#     core/aux_module.h
#     core/rhythmic_delivery.h
#     core/pcplp.h
#     plotwindow.h plotwindow.cpp plotwindow.ui
# )

# target_include_directories(calc_module_interface PRIVATE
#     ${CMAKE_CURRENT_SOURCE_DIR}/core
# )

# target_link_libraries(calc_module_interface
#     PRIVATE
#         Qt::Core
#         Qt::Widgets
# )

# include(GNUInstallDirs)

# install(TARGETS calc_module_interface
#     BUNDLE  DESTINATION .
#     RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
#     LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
# )

# qt_generate_deploy_app_script(
#     TARGET calc_module_interface
#     OUTPUT_SCRIPT deploy_script
#     NO_UNSUPPORTED_PLATFORM_ERROR
# )
# install(SCRIPT ${deploy_script})

# find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Charts)

# target_link_libraries(calc_module_interface
#     PRIVATE
#         Qt::Core
#         Qt::Widgets
#         Qt::Charts
# )



cmake_minimum_required(VERSION 3.13)
project(calc_module_interface LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt5 REQUIRED COMPONENTS Core Widgets Charts)

add_executable(calc_module_interface
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
)

target_link_libraries(calc_module_interface
    PRIVATE Qt5::Core Qt5::Widgets Qt5::Charts
)

include(GNUInstallDirs)

install(TARGETS calc_module_interface
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# ---- .deb через CPack (для Astra удобно) ----
set(CPACK_PACKAGE_NAME "calc_module_interface")
set(CPACK_PACKAGE_VERSION "0.1.0")
set(CPACK_GENERATOR "DEB")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Kiricek Aleksandr")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)  # зависимости посчитает автоматически
include(CPack)
