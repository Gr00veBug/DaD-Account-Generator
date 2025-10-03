#pragma once

#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <d3dcompiler.h>
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>

// DirectX libraries
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

struct AccountInfo {
    std::string username;
    std::string email;
    std::string password;
    std::string verificationCode;
    std::string cookie;
    std::string emailHash;
    std::string creationTime;
    bool isLegendary = false;
    bool isBanned = false;
    bool isTempBanned = false;
    std::string notes;
};

class Overlay {
private:
    // DirectX 11 objects
    ID3D11Device* m_pd3d_device = nullptr;
    ID3D11DeviceContext* m_pd3d_device_context = nullptr;
    IDXGISwapChain* m_p_swap_chain = nullptr;
    ID3D11RenderTargetView* m_main_render_target_view = nullptr;

    // Window objects
    HWND m_hwnd = nullptr;
    WNDCLASSEXW m_wc = {};

    // ImGui state
    bool m_show_demo_window = false;
    bool m_show_account_window = true;
    
    // Account management
    std::vector<AccountInfo> m_accounts;
    std::vector<AccountInfo> m_filtered_accounts;
    std::string m_api_key;
    char m_search_buffer[256] = "";
    std::vector<bool> m_password_visible;
    std::vector<bool> m_email_visible;
    std::vector<std::vector<char>> m_note_buffers;
    bool m_has_active_filter = false;

    struct OverlaySettings {
        bool highlightBanned = true;
        bool enableFilters = false;
        bool filterShowBanned = true;
        bool filterShowLegendary = true;
        bool filterShowFree = true;
        bool filterShowTempBanned = true;
        ImVec4 bannedColor = ImVec4(1.0f, 0.2f, 0.2f, 1.0f);
        ImVec4 tempBannedColor = ImVec4(1.0f, 0.6f, 0.0f, 1.0f);
        ImVec4 legendaryColor = ImVec4(0.9f, 0.8f, 0.2f, 1.0f);
        ImVec4 freeColor = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
    } m_settings;

    // Helper functions
    bool create_device_d3d();
    void cleanup_device_d3d();
    void create_render_target();
    void cleanup_render_target();
    static LRESULT WINAPI wnd_proc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    
    // Account management functions
    void load_accounts_from_file();
    void save_accounts_to_file();
    void filter_accounts();
    void copy_to_clipboard(const std::string& text);
    std::string get_latest_verification_code(const std::string& email);
    void sync_note_buffers();
    void update_note_buffer(size_t index);
    std::string serialize_notes(const std::string& notes) const;
    std::string deserialize_notes(const std::string& serialized) const;
    size_t find_account_index(const AccountInfo& account) const;
    std::string get_current_timestamp() const;

    // Config management functions
    std::string load_api_key_from_config();
    void save_api_key_to_config(const std::string& api_key);
    std::string prompt_user_for_api_key();

    // ImGui rendering
    void render_account_window();
    void render_menu_bar();
    void render_accounts_tab();
    void render_settings_tab();

public:
    Overlay();
    ~Overlay();
    
    // Main functions
    bool initialize(const std::string& api_key);
    void run();
    void shutdown();
    
    // Account management
    void add_account(const AccountInfo& account);
    void refresh_accounts();
    void generate_new_account();
    
    // Getters
    const std::vector<AccountInfo>& get_accounts() const { return m_accounts; }
    bool is_running() const { return m_hwnd != nullptr; }
};