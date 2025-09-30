#include "overlay.hpp"
#include "generator.hpp"
#include <thread>
#include <chrono>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <string>

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

Overlay::Overlay() 
    : m_pd3d_device(nullptr)
    , m_pd3d_device_context(nullptr)
    , m_p_swap_chain(nullptr)
    , m_main_render_target_view(nullptr)
    , m_hwnd(nullptr)
    , m_show_demo_window(false)
    , m_show_account_window(true) {
}

Overlay::~Overlay() {
    shutdown();
}

bool Overlay::initialize(const std::string& api_key) {
    // Try to load API key from config file first
    std::string config_api_key = load_api_key_from_config();
    
    if (!config_api_key.empty()) {
        m_api_key = config_api_key;
        std::cout << "API key loaded from config.ini\n";
    } else if (!api_key.empty()) {
        m_api_key = api_key;
        // Save the provided API key to config
        save_api_key_to_config(api_key);
        std::cout << "API key saved to config.ini\n";
    } else {
        // Prompt user for API key
        m_api_key = prompt_user_for_api_key();
        if (m_api_key.empty()) {
            std::cout << "No API key provided. Cannot initialize.\n";
            return false;
        }
        // Save the user-provided API key to config
        save_api_key_to_config(m_api_key);
        std::cout << "API key saved to config.ini\n";
    }
    
    // Load existing accounts
    load_accounts_from_file();
    
    // Create application window
    ZeroMemory(&m_wc, sizeof(m_wc));
    m_wc.cbSize = sizeof(m_wc);
    m_wc.style = CS_CLASSDC;
    m_wc.lpfnWndProc = wnd_proc;
    m_wc.cbClsExtra = 0L;
    m_wc.cbWndExtra = 0L;
    m_wc.hInstance = GetModuleHandle(nullptr);
    m_wc.hIcon = nullptr;
    m_wc.hCursor = nullptr;
    m_wc.hbrBackground = nullptr;
    m_wc.lpszMenuName = nullptr;
    m_wc.lpszClassName = L"DaD Account Generator Overlay";
    m_wc.hIconSm = nullptr;
    ::RegisterClassExW(&m_wc);
    // Create a borderless window for ImGui-only interface
    m_hwnd = ::CreateWindowW(m_wc.lpszClassName, L"DaD Account Generator", 
        WS_POPUP | WS_VISIBLE, 
        100, 100, 1280, 800, 
        nullptr, nullptr, m_wc.hInstance, nullptr);

    // Initialize Direct3D
    if (!create_device_d3d()) {
        cleanup_device_d3d();
        ::UnregisterClassW(m_wc.lpszClassName, m_wc.hInstance);
        return false;
    }

    // Show the window
    ::ShowWindow(m_hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(m_hwnd);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;    // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(m_hwnd);
    ImGui_ImplDX11_Init(m_pd3d_device, m_pd3d_device_context);

    return true;
}

void Overlay::run() {
    // Main loop
    bool done = false;
    while (!done) {
        // Poll and handle messages (inputs, window resize, etc.)
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        // Handle window resize
        if (m_pd3d_device != nullptr) {
            // Start the Dear ImGui frame
            ImGui_ImplDX11_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();

            // Render our windows
            render_account_window();

            // Rendering
            ImGui::Render();
            const float clear_color[4] = { 0.45f, 0.55f, 0.60f, 1.00f };
            m_pd3d_device_context->OMSetRenderTargets(1, &m_main_render_target_view, nullptr);
            m_pd3d_device_context->ClearRenderTargetView(m_main_render_target_view, clear_color);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

            m_p_swap_chain->Present(1, 0); // Present with vsync
        }
    }
}

void Overlay::shutdown() {
    // Cleanup
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    cleanup_device_d3d();
    ::DestroyWindow(m_hwnd);
    ::UnregisterClassW(m_wc.lpszClassName, m_wc.hInstance);
    m_hwnd = nullptr;
}

void Overlay::render_menu_bar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Refresh Accounts")) {
                refresh_accounts();
            }
            if (ImGui::MenuItem("Generate New Account")) {
                generate_new_account();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit")) {
                PostQuitMessage(0);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Account Window", nullptr, &m_show_account_window);
            ImGui::MenuItem("Demo Window", nullptr, &m_show_demo_window);
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    if (m_show_demo_window) {
        ImGui::ShowDemoWindow(&m_show_demo_window);
    }
}

void Overlay::render_account_window() {
    if (!m_show_account_window) return;

    // Make the window fill the entire screen and act as the main window
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    //ImGui::SetNextWindowViewport(viewport->ID);
    
    ImGui::Begin("DaD Account Manager", &m_show_account_window, 
        ImGuiWindowFlags_NoCollapse | 
        ImGuiWindowFlags_NoResize | 
        ImGuiWindowFlags_NoMove | 
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse);
    
    // Custom title bar
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
    
    ImGui::BeginChild("TitleBar", ImVec2(0, 30), true);
    ImGui::Text("DaD Account Manager");
    ImGui::SameLine(ImGui::GetWindowWidth() - 100);
    if (ImGui::Button("X", ImVec2(30, 20))) {
        PostQuitMessage(0);
    }
    ImGui::EndChild();
    
    ImGui::PopStyleColor(3);
    
    // Header with controls
    ImGui::Text("Generated Accounts: %d", (int)m_accounts.size());
    ImGui::SameLine();
    if (ImGui::Button("Generate New Account")) {
        generate_new_account();
    }
    ImGui::SameLine();
    if (ImGui::Button("Refresh")) {
        refresh_accounts();
    }
    
    // Search bar
    ImGui::SameLine();
    ImGui::Text("Search:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200);
    if (ImGui::InputText("##Search", m_search_buffer, sizeof(m_search_buffer))) {
        filter_accounts();
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        strcpy_s(m_search_buffer, "");
        filter_accounts();
    }
    
    ImGui::Separator();
    
    // Account list
    if (m_accounts.empty()) {
        ImGui::Text("No accounts generated yet. Click 'Generate New Account' to start.");
    } else {
        // Create a table for accounts with proper column sizing
        if (ImGui::BeginTable("Accounts", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY)) {
            ImGui::TableSetupColumn("Username", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn("Email", ImGuiTableColumnFlags_WidthStretch, 1.5f);
            ImGui::TableSetupColumn("Password", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn("Legendary", ImGuiTableColumnFlags_WidthFixed, 100.0f);
            ImGui::TableSetupColumn("Created", ImGuiTableColumnFlags_WidthStretch, 1.2f);
            ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 300.0f);
            ImGui::TableHeadersRow();

            // Use filtered accounts for display
            const auto& display_accounts = m_filtered_accounts.empty() && strlen(m_search_buffer) == 0 ? m_accounts : m_filtered_accounts;
            
            for (size_t i = 0; i < display_accounts.size(); ++i) {
                const auto& account = display_accounts[i];
                
                ImGui::TableNextRow();
                
                // Username
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%s", account.username.c_str());
                
                // Email
                ImGui::TableSetColumnIndex(1);
                
                // Find the index in the main accounts list for email visibility
                size_t email_main_index = 0;
                for (size_t j = 0; j < m_accounts.size(); ++j) {
                    if (m_accounts[j].username == account.username && m_accounts[j].email == account.email) {
                        email_main_index = j;
                        break;
                    }
                }
                
                // Ensure email visibility array is large enough
                if (email_main_index >= m_email_visible.size()) {
                    m_email_visible.resize(email_main_index + 1, false);
                }
                
                // Display email (blurred or visible)
                if (m_email_visible[email_main_index]) {
                    // Show actual email
                    ImGui::Text("%s", account.email.c_str());
                } else {
                    // Show blurred email with first 5 characters visible
                    std::string display_email;
                    if (account.email.length() <= 5) {
                        display_email = account.email; // Show full email if 5 chars or less
                    } else {
                        display_email = account.email.substr(0, 5);
                        display_email += std::string(account.email.length() - 5, '*');
                    }
                    ImGui::Text("%s", display_email.c_str());
                }
                
                // Make the email clickable to toggle visibility
                if (ImGui::IsItemClicked()) {
                    m_email_visible[email_main_index] = !m_email_visible[email_main_index];
                }
                
                // Password
                ImGui::TableSetColumnIndex(2);
                
                // Find the index in the main accounts list for password visibility
                size_t password_main_index = 0;
                for (size_t j = 0; j < m_accounts.size(); ++j) {
                    if (m_accounts[j].username == account.username && m_accounts[j].email == account.email) {
                        password_main_index = j;
                        break;
                    }
                }
                
                // Ensure password visibility array is large enough
                if (password_main_index >= m_password_visible.size()) {
                    m_password_visible.resize(password_main_index + 1, false);
                }
                
                // Display password (blurred or visible)
                if (m_password_visible[password_main_index]) {
                    // Show actual password
                    ImGui::Text("%s", account.password.c_str());
                } else {
                    // Show blurred password
                    std::string blurred_password(account.password.length(), '*');
                    ImGui::Text("%s", blurred_password.c_str());
                }
                
                // Make the password clickable to toggle visibility
                if (ImGui::IsItemClicked()) {
                    m_password_visible[password_main_index] = !m_password_visible[password_main_index];
                }
                
                // Legendary status display
                ImGui::TableSetColumnIndex(3);
                std::string legendary_button_id = "##Legendary" + std::to_string(i);
                std::string legendary_text = account.isLegendary ? "Legendary" : "Free";
                if (ImGui::Button((legendary_text + legendary_button_id).c_str())) {
                    // Toggle legendary status when clicked
                    // Find the account in the main accounts list and update it
                    for (auto& main_account : m_accounts) {
                        if (main_account.username == account.username && main_account.email == account.email) {
                            main_account.isLegendary = !main_account.isLegendary;
                            break;
                        }
                    }
                    save_accounts_to_file();
                }
                
                // Creation Time
                ImGui::TableSetColumnIndex(4);
                ImGui::Text("%s", account.creationTime.c_str());
                
                // Actions - Use vertical layout for better fit
                ImGui::TableSetColumnIndex(5);
                
                // First row of buttons
                if (ImGui::Button(("Copy Email##" + std::to_string(i)).c_str())) {
                    copy_to_clipboard(account.email);
                }
                ImGui::SameLine();
                if (ImGui::Button(("Copy Pass##" + std::to_string(i)).c_str())) {
                    copy_to_clipboard(account.password);
                }
                
                // Second row of buttons
                if (ImGui::Button(("Grab Code##" + std::to_string(i)).c_str())) {
                    std::string latest_code = get_latest_verification_code(account.email);
                    if (!latest_code.empty()) {
                        copy_to_clipboard(latest_code);
                        // Update the account with the latest code
                        m_accounts[i].verificationCode = latest_code;
                        save_accounts_to_file();
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button(("Delete##" + std::to_string(i)).c_str())) {
                    // Add delete functionality if needed
                    ImGui::OpenPopup(("Delete Account##" + std::to_string(i)).c_str());
                }
                
                // Delete confirmation popup
                if (ImGui::BeginPopupModal(("Delete Account##" + std::to_string(i)).c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                    ImGui::Text("Are you sure you want to delete this account?");
                    ImGui::Text("Username: %s", account.username.c_str());
                    ImGui::Text("Email: %s", account.email.c_str());
                    ImGui::Separator();
                    
                    if (ImGui::Button("Yes, Delete", ImVec2(120, 0))) {
                        m_accounts.erase(m_accounts.begin() + i);
                        save_accounts_to_file();
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::SetItemDefaultFocus();
                    ImGui::SameLine();
                    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::EndPopup();
                }
            }
            ImGui::EndTable();
        }
    }
    
    ImGui::End();
}

void Overlay::load_accounts_from_file() {
    m_accounts.clear();
    std::ifstream file("DaDAccounts.txt");
    if (!file.is_open()) return;
    
    std::string line;
    AccountInfo current_account;
    bool in_account = false;
    
    while (std::getline(file, line)) {
        if (line.find("Username: ") == 0) {
            if (in_account) {
                m_accounts.push_back(current_account);
            }
            current_account = AccountInfo();
            current_account.username = line.substr(10);
            in_account = true;
        } else if (line.find("Email: ") == 0) {
            current_account.email = line.substr(7);
        } else if (line.find("Password: ") == 0) {
            current_account.password = line.substr(9);
        } else if (line.find("Verification Code: ") == 0) {
            current_account.verificationCode = line.substr(19);
        } else if (line.find("Cookie: ") == 0) {
            current_account.cookie = line.substr(8);
        } else if (line.find("MD5 Hash of Email: ") == 0) {
            current_account.emailHash = line.substr(19);
        } else if (line.find("Creation Time: ") == 0) {
            current_account.creationTime = line.substr(15);
        } else if (line.find("Legendary: ") == 0) {
            std::string legendary_str = line.substr(11);
            current_account.isLegendary = (legendary_str == "Yes" || legendary_str == "true" || legendary_str == "1");
        }
    }
    
    // Add the last account if we were in the middle of reading one
    if (in_account) {
        m_accounts.push_back(current_account);
    }
    
    file.close();
    
    // Initialize visibility arrays
    m_password_visible.resize(m_accounts.size(), false);
    m_email_visible.resize(m_accounts.size(), false);
    
    // Apply current search filter
    filter_accounts();
}

void Overlay::filter_accounts() {
    m_filtered_accounts.clear();
    
    if (strlen(m_search_buffer) == 0) {
        return; // Show all accounts when search is empty
    }
    
    std::string search_term = m_search_buffer;
    std::transform(search_term.begin(), search_term.end(), search_term.begin(), ::tolower);
    
    for (const auto& account : m_accounts) {
        std::string username = account.username;
        std::string email = account.email;
        std::string password = account.password;
        std::string legendary = account.isLegendary ? "legendary" : "free";
        
        std::transform(username.begin(), username.end(), username.begin(), ::tolower);
        std::transform(email.begin(), email.end(), email.begin(), ::tolower);
        std::transform(password.begin(), password.end(), password.begin(), ::tolower);
        
        if (username.find(search_term) != std::string::npos ||
            email.find(search_term) != std::string::npos ||
            password.find(search_term) != std::string::npos ||
            legendary.find(search_term) != std::string::npos) {
            m_filtered_accounts.push_back(account);
        }
    }
}

void Overlay::save_accounts_to_file() {
    std::ofstream file("DaDAccounts.txt");
    if (!file.is_open()) return;
    
    for (const auto& account : m_accounts) {
        file << "Username: " << account.username << "\n"
             << "Email: " << account.email << "\n"
             << "Password: " << account.password << "\n"
             << "Verification Code: " << account.verificationCode << "\n"
             << "Cookie: " << account.cookie << "\n"
             << "MD5 Hash of Email: " << account.emailHash << "\n"
             << "Creation Time: " << account.creationTime << "\n"
             << "Legendary: " << (account.isLegendary ? "Yes" : "No") << "\n"
             << "_____________________________________________________________________\n\n";
    }
    
    file.close();
}

void Overlay::copy_to_clipboard(const std::string& text) {
    if (OpenClipboard(m_hwnd)) {
        EmptyClipboard();
        HGLOBAL hClipboardData = GlobalAlloc(GMEM_DDESHARE, text.length() + 1);
        if (hClipboardData) {
            char* pchData = (char*)GlobalLock(hClipboardData);
            if (pchData) {
                strcpy_s(pchData, text.length() + 1, text.c_str());
                GlobalUnlock(hClipboardData);
                SetClipboardData(CF_TEXT, hClipboardData);
            }
        }
        CloseClipboard();
    }
}

std::string Overlay::get_latest_verification_code(const std::string& email) {
    try {
        DaDAccountGenerator generator(m_api_key);
        return generator.get_latest_verification_code(email);
    } catch (const std::exception& e) {
        std::cout << "Error getting latest verification code: " << e.what() << std::endl;
        return "";
    }
}

void Overlay::add_account(const AccountInfo& account) {
    m_accounts.push_back(account);
    m_password_visible.push_back(false); // New accounts start with hidden passwords
    m_email_visible.push_back(false); // New accounts start with hidden emails
    save_accounts_to_file();
}

void Overlay::refresh_accounts() {
    load_accounts_from_file();
}

void Overlay::generate_new_account() {
    // Run account generation in a separate thread to avoid blocking the UI
    std::thread([this]() {
        try {
            DaDAccountGenerator generator(m_api_key);
            bool success = generator.generate_new_account(false);
            
            if (success) {
                // Reload accounts from file
                load_accounts_from_file();
            }
        } catch (const std::exception& e) {
            std::cout << "Error generating account: " << e.what() << std::endl;
        }
    }).detach();
}

bool Overlay::create_device_d3d() {
    // Setup swap chain
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = m_hwnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    if (D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &m_p_swap_chain, &m_pd3d_device, &featureLevel, &m_pd3d_device_context) != S_OK)
        return false;

    create_render_target();
    return true;
}

void Overlay::cleanup_device_d3d() {
    cleanup_render_target();
    if (m_p_swap_chain) { m_p_swap_chain->Release(); m_p_swap_chain = nullptr; }
    if (m_pd3d_device_context) { m_pd3d_device_context->Release(); m_pd3d_device_context = nullptr; }
    if (m_pd3d_device) { m_pd3d_device->Release(); m_pd3d_device = nullptr; }
}

void Overlay::create_render_target() {
    ID3D11Texture2D* pBackBuffer;
    m_p_swap_chain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    m_pd3d_device->CreateRenderTargetView(pBackBuffer, nullptr, &m_main_render_target_view);
    pBackBuffer->Release();
}

void Overlay::cleanup_render_target() {
    if (m_main_render_target_view) { m_main_render_target_view->Release(); m_main_render_target_view = nullptr; }
}

LRESULT WINAPI Overlay::wnd_proc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED) {
            // Handle window resize - we'll need to get the instance
            // For now, just return 0 to let the default handler work
            return 0;
        }
        return 0;
    case WM_GETMINMAXINFO:
        {
            LPMINMAXINFO lpMMI = (LPMINMAXINFO)lParam;
            lpMMI->ptMinTrackSize.x = 800;  // Minimum width
            lpMMI->ptMinTrackSize.y = 600;   // Minimum height
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}

std::string Overlay::load_api_key_from_config() {
    std::ifstream configFile("config.ini");
    if (!configFile.is_open()) {
        return "";
    }
    
    std::string line;
    while (std::getline(configFile, line)) {
        // Look for API_KEY line
        if (line.find("API_KEY=") == 0) {
            std::string apiKey = line.substr(8); // Remove "API_KEY=" prefix
            // Remove any trailing whitespace
            apiKey.erase(apiKey.find_last_not_of(" \t\r\n") + 1);
            configFile.close();
            return apiKey;
        }
    }
    
    configFile.close();
    return "";
}

void Overlay::save_api_key_to_config(const std::string& api_key) {
    std::ofstream configFile("config.ini");
    if (configFile.is_open()) {
        configFile << "[TEMP_MAIL]\n";
        configFile << "API_KEY=" << api_key << "\n";
        configFile.close();
    }
}

std::string Overlay::prompt_user_for_api_key() {
    std::cout << "\n=== API Key Configuration ===\n";
    std::cout << "No API key found in config.ini\n";
    std::cout << "Please enter your Temp Mail API key: ";
    
    std::string api_key;
    std::getline(std::cin, api_key);
    
    // Remove any trailing whitespace
    api_key.erase(api_key.find_last_not_of(" \t\r\n") + 1);
    
    if (api_key.empty()) {
        std::cout << "No API key provided.\n";
        return "";
    }
    
    std::cout << "API key received. Saving to config.ini...\n";
    return api_key;
}