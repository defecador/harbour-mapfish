project(harbour-comaps)

include(FindPkgConfig)
pkg_search_module(SAILFISHAPP sailfishapp REQUIRED)

set(APP_SHARE_DIR share/${PROJECT_NAME})
set(APP_LIB_DIR ${APP_SHARE_DIR}/lib)

# The bundled ICU's data stub lives in icui18n but is needed by icuuc, which links after it.
# Android's lld doesn't care about archive order; GNU ld does, so declare the (cyclic) dependency.
set_property(TARGET icuuc APPEND PROPERTY INTERFACE_LINK_LIBRARIES icui18n)

# All of the app lives in a shared library: see src/main.cpp for why the executable can't.
add_library(comaps_app SHARED
  src/app_main.cpp
  src/country_model.cpp
  src/country_model.hpp
  src/egl_context_factory.cpp
  src/egl_context_factory.hpp
  src/map_item.cpp
  src/map_item.hpp
  src/place_page.cpp
  src/place_page.hpp
  src/search_model.cpp
  src/search_model.hpp
)
set_target_properties(comaps_app PROPERTIES AUTOMOC ON OUTPUT_NAME comaps)
target_compile_options(comaps_app PRIVATE ${OMIM_WARNING_FLAGS})
target_include_directories(comaps_app PRIVATE ${SAILFISHAPP_INCLUDE_DIRS})
target_link_libraries(comaps_app
  PRIVATE
    map
    drape_frontend
    drape
    platform
    Qt5::Quick
    Qt5::Qml
    Qt5::Gui
    ${SAILFISHAPP_LIBRARIES}
    EGL
    GLESv2
)

add_executable(${PROJECT_NAME} src/main.cpp)
# sailfishapp's flags carry -pie -rdynamic: the launcher's booster dlopens the executable.
target_link_libraries(${PROJECT_NAME} PRIVATE comaps_app ${SAILFISHAPP_LDFLAGS})
set_target_properties(${PROJECT_NAME} PROPERTIES INSTALL_RPATH ${CMAKE_INSTALL_PREFIX}/${APP_LIB_DIR})

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
install(TARGETS comaps_app LIBRARY DESTINATION ${APP_LIB_DIR})
install(DIRECTORY qml DESTINATION ${APP_SHARE_DIR})
install(FILES ${PROJECT_NAME}.desktop DESTINATION share/applications)
foreach(size 86 108 128 172)
  install(FILES icons/${size}x${size}/${PROJECT_NAME}.png DESTINATION share/icons/hicolor/${size}x${size}/apps)
endforeach()

# Runtime data: the same set the Android app bundles as assets, plus the world overview maps.
# configure.sh generates the styles and symbols on the host before this build runs.
set(DATA_DIR ${OMIM_ROOT}/data)
set(DATA_FILES
  categories.txt
  categories_brands.txt
  classificator.txt
  colors.txt
  copyright.html
  countries-strings
  countries.txt
  countries_meta.txt
  editor.config
  fonts
  icudt75l.dat
  languages.txt
  mapcss-mapping.csv
  packed_polygons.bin
  patterns.txt
  sound-strings
  subtypes.csv
  symbols
  transit_colors.txt
  types.txt
  welcome.html
  World.mwm
  WorldCoasts.mwm
)
file(GLOB DRULES_FILES RELATIVE ${DATA_DIR} ${DATA_DIR}/drules_proto*.bin)
if (NOT DRULES_FILES)
  message(FATAL_ERROR "No drules_proto*.bin in ${DATA_DIR}: run ./configure.sh on the host first")
endif()

foreach(file ${DATA_FILES} ${DRULES_FILES})
  set(path ${DATA_DIR}/${file})
  if (NOT EXISTS ${path})
    message(FATAL_ERROR "Missing ${path}: run ./configure.sh on the host first")
  endif()
  if (IS_DIRECTORY ${path})
    install(DIRECTORY ${path} DESTINATION ${APP_SHARE_DIR}/data)
  else()
    # World.mwm and WorldCoasts.mwm are symlinks into data/world_mwm/<version>/.
    get_filename_component(real_path ${path} REALPATH)
    install(FILES ${real_path} DESTINATION ${APP_SHARE_DIR}/data RENAME ${file})
  endif()
endforeach()
