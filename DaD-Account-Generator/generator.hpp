#pragma once

#include <string>
#include <vector>
#include <random>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iostream>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

// Windows system libraries
#pragma comment(lib, "ws2_32.lib")        // Windows Sockets
#pragma comment(lib, "crypt32.lib")       // Cryptography API
#pragma comment(lib, "wldap32.lib")       // Windows LDAP
#pragma comment(lib, "normaliz.lib")      // IDN normalization
#pragma comment(lib, "libcurl.lib")       // libcurl
#pragma comment(lib, "libssl.lib")        // OpenSSL SSL
#pragma comment(lib, "libcrypto.lib")     // OpenSSL Crypto

class DaDAccountGenerator {
private:
    std::string api_key_;
    std::string user_agent_ = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/127.0.6533.89 Safari/537.36";

    // Helper functions
    static size_t write_callback(void* contents, size_t size, size_t nmemb, std::string* userp);
    std::string generate_random_string(size_t length = 10);
    std::string generate_md5_hash(const std::string& input);
    std::string generate_strong_password(size_t length = 8);
    std::string extract_verification_code(const std::string& mail_text);

    // API functions
    std::vector<std::string> get_domain_list();
    nlohmann::json get_newest_email_content(const std::string& mail_id);
    std::pair<std::string, std::string> check_for_verification_email(const std::string& email_address, int check_interval = 1);
    std::pair<std::string, std::string> check_for_last_verification_code(const std::string& email_address, int check_interval = 1);
    bool verify_email(const std::string& email_address, const std::string& verification_code);
    void log_account_info(const std::string& username, const std::string& email,
        const std::string& password, const std::string& cookie,
        const std::string& verification_code, const std::string& mail_id);

public:
    // Constructor
    explicit DaDAccountGenerator(const std::string& api_key);

    // Main functionality
    bool generate_new_account(bool prompt_exit = true);
    void generate_multiple_accounts(int num_accounts);
    void grab_verification_code();
    
    // Public wrapper for getting latest verification code
    std::string get_latest_verification_code(const std::string& email_address);

    // Getters
    std::string get_api_key() const { return api_key_; }
    std::string get_user_agent() const { return user_agent_; }

    // Setters
    void set_api_key(const std::string& api_key) { api_key_ = api_key; }
    void set_user_agent(const std::string& user_agent) { user_agent_ = user_agent; }
};