# Copyright (C) 2026 The Qt Company Ltd.
# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.21)

project(QmllsHelpPluginWorkaround LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(Qt6 REQUIRED COMPONENTS Core Help)

if(NOT DEFINED QTDECLARATIVE_SOURCE_DIR)
    get_filename_component(QTDECLARATIVE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
endif()

add_library(QmllsHelpPluginWorkaround MODULE
    helpplugin.cpp
)

target_include_directories(QmllsHelpPluginWorkaround PRIVATE
    "${QTDECLARATIVE_SOURCE_DIR}/src/qmlls"
)

target_link_libraries(QmllsHelpPluginWorkaround PRIVATE
    Qt6::Core
    Qt6::Help
)

set_target_properties(QmllsHelpPluginWorkaround PROPERTIES
    OUTPUT_NAME helpplugin
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins/help"
)
