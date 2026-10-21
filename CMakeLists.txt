cmake_minimum_required(VERSION 3.24)
project(MMC VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network Concurrent)

qt_standard_project_setup()

# ---------------------------------------------------------------------------
# mmc_core : everything that is not a widget (crypto, storage, protocols)
# ---------------------------------------------------------------------------
add_library(mmc_core STATIC
    src/core/Crypto.cpp
    src/core/Vault.cpp
    src/core/Importer.cpp
    src/core/ProviderIcons.cpp
    src/net/Mime.cpp
    src/net/ImapClient.cpp
    src/net/SmtpClient.cpp
    src/net/OAuth2.cpp
    src/net/Providers.cpp
    src/net/MailService.cpp
    resources/resources.qrc
)
target_include_directories(mmc_core PUBLIC src)
target_link_libraries(mmc_core PUBLIC Qt6::Core Qt6::Gui Qt6::Network Qt6::Concurrent bcrypt)

# ---------------------------------------------------------------------------
# mmc : the application
# ---------------------------------------------------------------------------
qt_add_executable(MMC
    src/main.cpp
    src/ui/Theme.cpp
    src/ui/Icons.cpp
    src/ui/IconData.cpp
    src/ui/VaultDialogs.cpp
    src/ui/MainWindow.cpp
    src/ui/MailboxDialog.cpp
    src/ui/ComposeDialog.cpp
    src/ui/Sidebar.cpp
    src/ui/MessageList.cpp
    src/ui/Reader.cpp
    src/ui/EmptyState.cpp
    src/ui/Toast.cpp
    src/ui/SettingsDialog.cpp
    src/ui/AccountManager.cpp
)
target_link_libraries(MMC PRIVATE mmc_core Qt6::Widgets dwmapi)

# ---------------------------------------------------------------------------
# tests
# ---------------------------------------------------------------------------
enable_testing()
add_executable(mmc_tests
    test/test_main.cpp
    test/test_crypto.cpp
    test/test_vault.cpp
    test/test_mime.cpp
    test/test_imap.cpp
    test/test_importer.cpp
    test/test_icons.cpp
)
target_link_libraries(mmc_tests PRIVATE mmc_core)
add_test(NAME mmc_tests COMMAND mmc_tests)
