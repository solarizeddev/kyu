add_executable(kyu-tray
    main.cpp
    badge.cpp
    statewatcher.cpp
    trayicon.cpp
    checkscheduler.cpp
)
target_link_libraries(kyu-tray PRIVATE kyucore Qt6::Gui Qt6::Widgets)
target_compile_definitions(kyu-tray PRIVATE
    KYU_VERSION="${PROJECT_VERSION}"
    KYU_APP_ID="${KYU_APP_ID}"
    KYU_CHECK_BINARY="${CMAKE_INSTALL_FULL_BINDIR}/kyu-check")
if(KF6StatusNotifierItem_FOUND)
    target_compile_definitions(kyu-tray PRIVATE KYU_HAVE_KSNI)
    target_link_libraries(kyu-tray PRIVATE KF6::StatusNotifierItem)
endif()
install(TARGETS kyu-tray DESTINATION ${CMAKE_INSTALL_BINDIR})
