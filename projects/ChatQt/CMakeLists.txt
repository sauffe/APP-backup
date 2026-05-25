cmake_minimum_required(VERSION 3.19)
project(ChatQt LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets)
find_package(Qt6 REQUIRED COMPONENTS Widgets WebSockets)
find_package(Qt6 REQUIRED COMPONENTS Widgets WebSockets Network)

qt_standard_project_setup()

qt_add_executable(ChatQt
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    privatechatdialog.h
    privatechatdialog.cpp
)


target_link_libraries(ChatQt
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt6::WebSockets
        Qt6::Network
)

include(GNUInstallDirs)

install(TARGETS ChatQt
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET ChatQt
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
