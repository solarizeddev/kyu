add_executable(kyu-tray
    main.cpp
    badge.cpp
    systemwatcher.cpp
    trayicon.cpp
)
target_link_libraries(kyu-tray PRIVATE kyucore Qt6::Gui Qt6::Widgets Qt6::DBus)
target_compile_definitions(kyu-tray PRIVATE
    KYU_VERSION="${PROJECT_VERSION}"
    KYU_APP_ID="${KYU_APP_ID}"
    KYU_CARD_BINARY="${CMAKE_INSTALL_FULL_BINDIR}/kyu")
if(KF6StatusNotifierItem_FOUND)
    target_compile_definitions(kyu-tray PRIVATE KYU_HAVE_KSNI)
    target_link_libraries(kyu-tray PRIVATE KF6::StatusNotifierItem)
endif()
# Optional: the "News" notification (kyu.notifyrc).
if(KF6Notifications_FOUND)
    target_compile_definitions(kyu-tray PRIVATE KYU_HAVE_KNOTIFICATIONS)
    target_link_libraries(kyu-tray PRIVATE KF6::Notifications)
endif()
install(TARGETS kyu-tray DESTINATION ${CMAKE_INSTALL_BINDIR})
