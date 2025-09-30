#include "generator.hpp"
#include <openssl/md5.h>
#include <openssl/evp.h>
#include <sstream>
#include <iomanip>
#include <thread>
#include <regex>

// Constructor implementation
DaDAccountGenerator::DaDAccountGenerator(const std::string& api_key) : api_key_(api_key) {
    curl_global_init(CURL_GLOBAL_ALL);
}

// Helper function for CURL write callback
size_t DaDAccountGenerator::write_callback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    userp->append((char*)contents, size * nmemb);
    return size * nmemb;
}

// Generate random string
std::string DaDAccountGenerator::generate_random_string(size_t length) {
    static const std::string chars = "abcdefghijklmnopqrstuvwxyz0123456789";
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, chars.size() - 1);

    std::string result;
    result.reserve(length);
    for (size_t i = 0; i < length; ++i) {
        result += chars[dis(gen)];
    }
    return result;
}

// Generate MD5 hash
std::string DaDAccountGenerator::generate_md5_hash(const std::string& input) {
    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    const EVP_MD* md = EVP_md5();
    unsigned char digest[MD5_DIGEST_LENGTH];
    unsigned int digest_len;

    EVP_DigestInit_ex(mdctx, md, NULL);
    EVP_DigestUpdate(mdctx, input.c_str(), input.length());
    EVP_DigestFinal_ex(mdctx, digest, &digest_len);
    EVP_MD_CTX_free(mdctx);

    std::stringstream ss;
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)digest[i];
    }
    return ss.str();
}

// Generate strong password
std::string DaDAccountGenerator::generate_strong_password(size_t length) {
    if (length < 8) {
        throw std::invalid_argument("Password length must be at least 8 characters");
    }

    static const std::string lowercase = "abcdefghijklmnopqrstuvwxyz";
    static const std::string uppercase = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    static const std::string digits = "0123456789";
    static const std::string special = "!@#$%^&*()_+-=[]{}|;:,.<>?";

    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 255);

    std::string password;
    password.reserve(length);

    // Ensure at least one of each required character type
    password += lowercase[dis(gen) % lowercase.length()];
    password += uppercase[dis(gen) % uppercase.length()];
    password += digits[dis(gen) % digits.length()];
    password += special[dis(gen) % special.length()];

    // Fill the rest with random characters
    const std::string all_chars = lowercase + uppercase + digits + special;
    for (size_t i = 4; i < length; ++i) {
        password += all_chars[dis(gen) % all_chars.length()];
    }

    // Shuffle the password
    std::shuffle(password.begin(), password.end(), gen);
    return password;
}

// Extract verification code from email text
std::string DaDAccountGenerator::extract_verification_code(const std::string& mail_text) {
    std::regex pattern(R"(\b[A-Za-z0-9]{6}\b)");
    std::smatch matches;
    if (std::regex_search(mail_text, matches, pattern)) {
        return matches[0];
    }
    return "";
}

// Get domain list from API
std::vector<std::string> DaDAccountGenerator::get_domain_list() {
    std::cout << "Fetching available email domains...\n";
    CURL* curl = curl_easy_init();
    std::string response;
    std::vector<std::string> domains;

    if (curl) {
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, ("x-rapidapi-key: " + api_key_).c_str());
        headers = curl_slist_append(headers, "x-rapidapi-host: privatix-temp-mail-v1.p.rapidapi.com");

        // First try with SSL verification
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
        curl_easy_setopt(curl, CURLOPT_CAINFO, NULL);
        curl_easy_setopt(curl, CURLOPT_CAPATH, NULL);
        
        curl_easy_setopt(curl, CURLOPT_URL, "https://privatix-temp-mail-v1.p.rapidapi.com/request/domains/");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        //curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

        CURLcode res = curl_easy_perform(curl);
        
        // If SSL verification fails, try without verification
        if (res == CURLE_SSL_CONNECT_ERROR) {
            std::cout << "SSL verification failed. Trying without verification...\n";
            response.clear();
            
            // Disable SSL verification
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
            
            res = curl_easy_perform(curl);
        }
        
        if (res != CURLE_OK) {
            std::cout << "Failed to fetch domains. Error: " << curl_easy_strerror(res) << "\n";
            
            // Get SSL error details if available
            if (res == CURLE_SSL_CONNECT_ERROR) {
                long verify_result;
                curl_easy_getinfo(curl, CURLINFO_SSL_VERIFYRESULT, &verify_result);
                std::cout << "SSL verify result: " << verify_result << "\n";
            }
        } else {
            try {
                auto json = nlohmann::json::parse(response);
                for (const auto& domain : json) {
                    domains.push_back(domain.get<std::string>());
                }
                std::cout << "Found " << domains.size() << " available domains\n";
            }
            catch (const std::exception& e) {
                std::cout << "Error parsing domain list: " << e.what() << "\n";
                std::cout << "Raw response: " << response << "\n";
            }
        }
        
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    } else {
        std::cout << "Failed to initialize CURL\n";
    }
    

	if (domains.empty()) {
		std::cout << "No domains found. Please check your API key or network connection.\n";
	}
	else {
		std::cout << "Available domains:\n";
		for (const auto& domain : domains) {
			std::cout << "- " << domain << "\n";
		}
	}
    return domains;
}

// Get newest email content
nlohmann::json DaDAccountGenerator::get_newest_email_content(const std::string& mail_id) {
    CURL* curl = curl_easy_init();
    std::string response;

    if (curl) {
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, ("x-rapidapi-key: " + api_key_).c_str());
        headers = curl_slist_append(headers, "x-rapidapi-host: privatix-temp-mail-v1.p.rapidapi.com");

        // Set SSL options
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        curl_easy_setopt(curl, CURLOPT_CAINFO, NULL);
        curl_easy_setopt(curl, CURLOPT_CAPATH, NULL);

        std::string url = "https://privatix-temp-mail-v1.p.rapidapi.com/request/mail/id/" + mail_id + "/";
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        CURLcode res = curl_easy_perform(curl);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        if (res == CURLE_OK) {
            try {
                return nlohmann::json::parse(response);
            }
            catch (const std::exception& e) {
                std::cout << "Error parsing email content: " << e.what() << "\n";
            }
        }
        else {
            std::cout << "Failed to fetch email content. Error: " << curl_easy_strerror(res) << "\n";
        }
    }
    return nlohmann::json();
}

// Check for verification email
std::pair<std::string, std::string> DaDAccountGenerator::check_for_verification_email(
    const std::string& email_address, int check_interval) {

    std::string email_hash = generate_md5_hash(email_address);
    std::cout << "Checking for verification email for: " << email_address << " (hash: " << email_hash << ")\n";
    
    int attempts = 0;
    const int max_attempts = 60; // 5 minutes max wait time
    
    while (attempts < max_attempts) {
        std::cout << "Attempt " << (attempts + 1) << "/" << max_attempts << " - Checking for emails...\n";
        
        auto emails = get_newest_email_content(email_hash);
        std::cout << "Email response: " << emails.dump() << "\n";
        
        if (!emails.empty()) {
            std::cout << "Found emails. JSON structure: " << emails.dump(2) << "\n";
            
            // Check if emails is an array
            if (emails.is_array()) {
                std::cout << "Found " << emails.size() << " emails\n";
                for (const auto& email : emails) {
                    try {
                        if (email.contains("mail_subject")) {
                            std::cout << "Email subject: " << email["mail_subject"] << "\n";
                            if (email["mail_subject"] == "Verify email") {
                                if (email.contains("mail_text")) {
                                    std::string verification_code = extract_verification_code(email["mail_text"]);
                                    std::cout << "Found verification email! Code: " << verification_code << "\n";
                                    if (!verification_code.empty()) {
                                        return { email_hash, verification_code };
                                    }
                                }
                            }
                        } else {
                            std::cout << "Email missing 'mail_subject' field\n";
                        }
                    } catch (const std::exception& e) {
                        std::cout << "Error processing email: " << e.what() << "\n";
                        std::cout << "Email JSON: " << email.dump() << "\n";
                    }
                }
            } else {
                std::cout << "Emails is not an array. Type: " << emails.type_name() << "\n";
                // Try to access as single object
                try {
                    if (emails.contains("mail_subject")) {
                        std::cout << "Single email subject: " << emails["mail_subject"] << "\n";
                        if (emails["mail_subject"] == "Verify email") {
                            if (emails.contains("mail_text")) {
                                std::string verification_code = extract_verification_code(emails["mail_text"]);
                                std::cout << "Found verification email! Code: " << verification_code << "\n";
                                if (!verification_code.empty()) {
                                    return { email_hash, verification_code };
                                }
                            }
                        }
                    }
                } catch (const std::exception& e) {
                    std::cout << "Error processing single email: " << e.what() << "\n";
                }
            }
        } else {
            std::cout << "No emails found yet...\n";
        }
        
        attempts++;
        if (attempts < max_attempts) {
            std::cout << "Waiting " << check_interval << " seconds before next check...\n";
            std::this_thread::sleep_for(std::chrono::seconds(check_interval));
        }
    }
    
    std::cout << "Timeout reached. No verification email found.\n";
    return { "", "" };
}

// Check for last verification code
std::pair<std::string, std::string> DaDAccountGenerator::check_for_last_verification_code(
    const std::string& email_address, int check_interval) {

    std::string email_hash = generate_md5_hash(email_address);
    while (true) {
        auto emails = get_newest_email_content(email_hash);
        if (!emails.empty()) {
            std::string last_code;
            for (const auto& email : emails) {
                std::string code = extract_verification_code(email["mail_text"]);
                if (!code.empty()) {
                    last_code = code;
                }
            }
            if (!last_code.empty()) {
                return { email_hash, last_code };
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(check_interval));
    }
}

// Verify email
bool DaDAccountGenerator::verify_email(const std::string& email_address, const std::string& verification_code) {
    CURL* curl = curl_easy_init();
    std::string response;

    if (curl) {
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        headers = curl_slist_append(headers, ("User-Agent: " + user_agent_).c_str());
        headers = curl_slist_append(headers, "Origin: https://darkanddarker.com");
        headers = curl_slist_append(headers, "Referer: https://darkanddarker.com/user/register");

        // Set SSL options - disable verification for development
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        curl_easy_setopt(curl, CURLOPT_CAINFO, NULL);
        curl_easy_setopt(curl, CURLOPT_CAPATH, NULL);

        nlohmann::json data = {
            {"email", email_address},
            {"code", verification_code}
        };
        std::string json_data = data.dump();
        std::cout << "Sending verification JSON: " << json_data << std::endl;

        curl_easy_setopt(curl, CURLOPT_URL, "https://darkanddarker.com/auth/regist/email/verify");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POST, 1);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, json_data.length());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        CURLcode res = curl_easy_perform(curl);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        if (res == CURLE_OK) {
            std::cout << "Verification response: " << response << std::endl;
            try {
                auto json = nlohmann::json::parse(response);
                bool success = json["result"] == 0;
                std::cout << "Verification result: " << (success ? "SUCCESS" : "FAILED") << std::endl;
                return success;
            }
            catch (const std::exception& e) {
                std::cout << "Error parsing verification response: " << e.what() << "\n";
                std::cout << "Raw response: " << response << "\n";
                return false;
            }
        }
        else {
            std::cout << "Failed to verify email. Error: " << curl_easy_strerror(res) << "\n";
        }
    }
    return false;
}

// Log account information
void DaDAccountGenerator::log_account_info(const std::string& username, const std::string& email,
    const std::string& password, const std::string& cookie,
    const std::string& verification_code, const std::string& email_hash) {

    std::ofstream file("DaDAccounts.txt", std::ios::app);
    if (file.is_open()) {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);

        file << "Username: " << username << "\n"
            << "Email: " << email << "\n"
            << "Password: " << password << "\n"
            << "Verification Code: " << verification_code << "\n"
            << "Cookie: " << cookie << "\n"
            << "MD5 Hash of Email: " << email_hash << "\n"
            << "Creation Time: " << std::ctime(&time)
            << "Legendary: No\n"
            << "_____________________________________________________________________\n\n";
    }
}

// Generate new account
bool DaDAccountGenerator::generate_new_account(bool prompt_exit) {
    std::cout << "\n=== Starting New Account Generation ===\n";
    
    auto domains = get_domain_list();
    if (domains.empty()) {
        std::cout << "No domains available. Cannot proceed.\n";
        return false;
    }

    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, domains.size() - 1);
    std::string selected_domain = domains[dis(gen)];

    std::string local_part = generate_random_string();
    std::string email_address = local_part + selected_domain;
    std::string email_hash = generate_md5_hash(email_address);

    std::cout << "Generated email address: " << email_address << "\n";

    // Send registration request
    std::cout << "Sending registration request...\n";
    CURL* curl = curl_easy_init();
    std::string response;

    if (curl) {
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        headers = curl_slist_append(headers, ("User-Agent: " + user_agent_).c_str());
        headers = curl_slist_append(headers, "Origin: https://darkanddarker.com");
        headers = curl_slist_append(headers, "Referer: https://darkanddarker.com/user/register");

        // Temporarily disable SSL verification for development
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        
        // Enable verbose output for debugging
        //curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

        nlohmann::json data = {
            {"email", email_address}
        };
        std::string json_data = data.dump();
        std::cout << "Sending JSON data: " << json_data << std::endl;

        curl_easy_setopt(curl, CURLOPT_URL, "https://darkanddarker.com/auth/regist/email/code");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POST, 1);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, json_data.length());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        CURLcode res = curl_easy_perform(curl);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK) {
            std::cout << "Failed to send registration request. Error: " << curl_easy_strerror(res) << "\n";
            return false;
        }

        try {
            std::cout << "Response: " << response << std::endl;
            auto json = nlohmann::json::parse(response);
            if (json["result"] != 0) {
                std::cout << "Registration request failed. Server response: " << json.dump() << "\n";
                return false;
            }
            std::cout << "Registration request sent successfully. Waiting for verification email...\n";
        }
        catch (const std::exception& e) {
            std::cout << "Error parsing registration response: " << e.what() << "\n";
            return false;
        }
    }

    // Wait for verification email and verify
    std::cout << "Checking for verification email...\n";
    auto [mail_id, verification_code] = check_for_verification_email(email_address);
    
    if (verification_code.empty()) {
        std::cout << "No verification email received within timeout period.\n";
        return false;
    }
    
    std::cout << "Verification code received: " << verification_code << "\n";
    
    std::cout << "Verifying email...\n";
    if (!verify_email(email_address, verification_code)) {
        std::cout << "Email verification failed.\n";
        return false;
    }
    std::cout << "Email verified successfully.\n";

    // Generate password and complete registration
    std::string password = generate_strong_password();
    std::cout << "Generated password: " << password << "\n";

    std::cout << "Completing registration...\n";
    curl = curl_easy_init();
    if (curl) {
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        headers = curl_slist_append(headers, ("User-Agent: " + user_agent_).c_str());
        headers = curl_slist_append(headers, "Origin: https://darkanddarker.com");
        headers = curl_slist_append(headers, "Referer: https://darkanddarker.com/user/register");

        // Temporarily disable SSL verification for development
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        
        // Enable verbose output for debugging
        //curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

        nlohmann::json data = {
            {"email", email_address},
            {"username", local_part},
            {"password", password}
        };
        std::string json_data = data.dump();
        std::cout << "Sending final registration JSON: " << json_data << std::endl;

        curl_easy_setopt(curl, CURLOPT_URL, "https://darkanddarker.com/auth/regist");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POST, 1);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, json_data.length());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        CURLcode res = curl_easy_perform(curl);
        std::string cookie;
        if (res == CURLE_OK) {
            char* cookieHeader = nullptr;
            curl_easy_getinfo(curl, CURLINFO_COOKIELIST, &cookieHeader);
            if (cookieHeader) {
                cookie = cookieHeader;
            }
        }
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        if (res == CURLE_OK) {
            std::cout << "Final registration response: " << response << std::endl;
            try {
                // Handle concatenated JSON responses by finding the last complete JSON object
                std::string jsonResponse = response;
                
                // If we have concatenated JSON, try to find the last complete object
                if (jsonResponse.find("}{") != std::string::npos) {
                    std::cout << "Detected concatenated JSON response, extracting last object...\n";
                    size_t lastBrace = jsonResponse.find_last_of('}');
                    if (lastBrace != std::string::npos) {
                        // Find the start of the last JSON object
                        size_t startPos = jsonResponse.find_last_of('{', lastBrace);
                        if (startPos != std::string::npos) {
                            jsonResponse = jsonResponse.substr(startPos);
                            std::cout << "Extracted JSON: " << jsonResponse << std::endl;
                        }
                    }
                }
                
                auto json = nlohmann::json::parse(jsonResponse);
                if (json["result"] == 0) {
                    std::string server_username = json["username"];
                    std::cout << "Account created successfully!\n";
                    std::cout << "Username: " << server_username << "\n";
                    std::cout << "Saving account details to file...\n";
                    log_account_info(server_username, email_address, password, cookie, verification_code, email_hash);
                    std::cout << "Account details saved to DaDAccounts.txt\n";
                    return true;
                }
                else {
                    std::cout << "Registration failed. Server response: " << json.dump() << "\n";
                }
            }
            catch (const std::exception& e) {
                std::cout << "Error parsing registration response: " << e.what() << "\n";
                std::cout << "Raw response: " << response << "\n";
            }
        }
        else {
            std::cout << "Failed to complete registration. Error: " << curl_easy_strerror(res) << "\n";
        }
    }

    return false;
}

// Generate multiple accounts
void DaDAccountGenerator::generate_multiple_accounts(int num_accounts) {
    for (int i = 0; i < num_accounts; ++i) {
        std::cout << "\nGenerating account " << (i + 1) << " of " << num_accounts << "...\n";
        generate_new_account(false);
        if (i < num_accounts - 1) {
            std::cout << "Waiting 1 second before generating the next account...\n";
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

// Grab verification code
void DaDAccountGenerator::grab_verification_code() {
    std::string email_address;
    std::cout << "Enter the email address: ";
    std::getline(std::cin, email_address);

    auto [mail_id, verification_code] = check_for_last_verification_code(email_address);
    if (!verification_code.empty()) {
        std::cout << "Last verification code for " << email_address << ": " << verification_code << "\n";
    }
    else {
        std::cout << "Failed to retrieve last verification code.\n";
    }
    std::cout << "Press Enter to exit...";
    std::cin.ignore();
}

// Public wrapper for getting latest verification code
std::string DaDAccountGenerator::get_latest_verification_code(const std::string& email_address) {
    auto [mail_id, verification_code] = check_for_last_verification_code(email_address);
    return verification_code;
}