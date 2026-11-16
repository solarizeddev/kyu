find_package(Qt6 6.4 REQUIRED COMPONENTS Quick QuickControls2)

# Logic, testable without QML.
add_library(kyupanel STATIC panelcontroller.cpp updatesmodel.cpp outputmodel.cpp flatpakfollower.cpp)
target_include_directories(kyupanel PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(kyupanel PUBLIC kyucore kyuclient Qt6::Gui Qt6::DBus)

# The panel window.
add_executable(kyu main.cpp singleinstance.cpp testdriver.cpp)
target_link_libraries(kyu PRIVATE kyupanel Qt6::Quick Qt6::QuickControls2 Qt6::Widgets Qt6::DBus)
target_compile_definitions(kyu PRIVATE KYU_VERSION="${PROJECT_VERSION}" KYU_APP_ID="${KYU_APP_ID}")
# Every QML file and script under qml/ (both looks: qml/plasma, qml/terminal).
file(GLOB_RECURSE KYU_QML_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CONFIGURE_DEPENDS qml/*.qml qml/*.js)
qt_add_resources(kyu panel_qml PREFIX "/" FILES ${KYU_QML_FILES})
install(TARGETS kyu DESTINATION ${CMAKE_INSTALL_BINDIR})

# Optional: lets a tray click focus an already open panel on Wayland.
find_package(KF6WindowSystem QUIET)
if(KF6WindowSystem_FOUND)
    target_link_libraries(kyu PRIVATE KF6::WindowSystem)
    target_compile_definitions(kyu PRIVATE KYU_HAVE_KWINDOWSYSTEM)
endif()

# Development only: a stand-in for the few Kirigami types the panel uses, so
# the panel runs (and can be screenshotted) where KF6 is not installed. Never
# enable this for a real build: the real Kirigami follows the user's theme.
option(KYU_PREVIEW "Build the panel with a Kirigami stand-in and screenshot options (development)" OFF)
if(KYU_PREVIEW)
    target_sources(kyu PRIVATE preview/preview.cpp)
    target_compile_definitions(kyu PRIVATE KYU_PREVIEW)
    qt_add_resources(kyu preview_qml PREFIX "/" FILES
        preview/kirigami/Icon.qml
        preview/kirigami/Heading.qml
        preview/kirigami/Separator.qml
        preview/kirigami/LinkButton.qml
    )
endif()
