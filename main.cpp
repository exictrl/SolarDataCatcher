#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <curl/curl.h>
#include "nlohmann/json.hpp"
#include <arpa/inet.h>
#include <unistd.h>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <csignal>

volatile sig_atomic_t running = 0;

void signalHandler(int signal) {
    running = 0;
    std::cout << "\nTermination signal received. Terminating..." << std::endl;
}

using json = nlohmann::json;

// Структура для хранения всех солнечных данных + флаги валидности
struct SolarData {
    float density = 0.0f;
    float speed = 0.0f;
    float temperature = 0.0f;
    int m_class = 0;
    int x_class = 0;
    float lon_gsm = 0.0f;
    float bt = 0.0f;
    float bz_gsm = 0.0f;
    float kp = 0.0f;
    
    bool density_valid = false;
    bool speed_valid = false;
    bool temperature_valid = false;
    bool m_class_valid = false;
    bool x_class_valid = false;
    bool lon_gsm_valid = false;
    bool bt_valid = false;
    bool bz_gsm_valid = false;
    bool kp_valid = false;
};

// Глобальный объект для хранения последних валидных данных
SolarData lastValidData;

// Простой и чистый вывод всех данных
void printSolarData(const SolarData& data) {
    auto now = std::chrono::system_clock::now();
    std::time_t nowTime = std::chrono::system_clock::to_time_t(now);
    std::tm* localTime = std::localtime(&nowTime);
    char timeBuffer[9];
    std::strftime(timeBuffer, sizeof(timeBuffer), "%H:%M:%S", localTime);
    
    std::cout << "\n";
    std::cout << "─────────────────────────────────────────" << std::endl;
    std::cout << "  SOLAR DATA UPDATE: " << timeBuffer << std::endl;
    std::cout << "─────────────────────────────────────────" << std::endl;
    
    // Solar Wind
    std::cout << "  SOLAR WIND                                         " << std::endl;
    std::cout << "    • Density:     " << std::fixed << std::setprecision(2) 
              << std::setw(6) << data.density << " p/cc" << std::endl;
    std::cout << "    • Speed:       " << std::fixed << std::setprecision(1) 
              << std::setw(6) << data.speed << " km/s" << std::endl;
    
    // Temperature formatting
    std::string tempStr;
    std::stringstream tempSs;
    if (data.temperature >= 1000.0f) {
        tempSs << std::fixed << std::setprecision(1) << (data.temperature / 1000.0f) << "k";
        tempStr = tempSs.str();
    } else {
        tempSs << std::fixed << std::setprecision(0) << data.temperature;
        tempStr = tempSs.str();
    }
    std::cout << "    • Temperature: " << std::setw(6) << tempStr << " K" << std::endl;
    
    std::cout << "                                                    " << std::endl;
    
    // Flares
    std::cout << "  SOLAR FLARES (1-day probability)                  " << std::endl;
    std::cout << "    • M-class:     " << std::setw(3) << data.m_class << "%" << std::endl;
    std::cout << "    • X-class:     " << std::setw(3) << data.x_class << "%" << std::endl;
    
    std::cout << "                                                    " << std::endl;
    
    // Magnetometer
    std::cout << "  MAGNETOMETER                                      " << std::endl;
    std::cout << "    • Phi GSM:     " << std::fixed << std::setprecision(2) 
              << std::setw(6) << data.lon_gsm << "°" << std::endl;
    std::cout << "    • Bt:          " << std::fixed << std::setprecision(2) 
              << std::setw(6) << data.bt << " nT" << std::endl;
    std::cout << "    • Bz GSM:      " << std::fixed << std::setprecision(2) 
              << std::setw(6) << data.bz_gsm << " nT" << std::endl;
    
    std::cout << "                                                    " << std::endl;
    
    // Kp-index
    std::cout << "  PLANETARY K-INDEX                                 " << std::endl;
    std::cout << "    • Kp:          " << std::fixed << std::setprecision(1) 
              << std::setw(4) << data.kp << std::endl;
    
    std::cout << "─────────────────────────────────────────" << std::endl;
}

// Function to write data from curl response
size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* buffer) {
    size_t totalSize = size * nmemb;
    if (buffer) {
        buffer->append((char*)contents, totalSize);
        return totalSize;
    }
    return 0;
}

std::string getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t nowTime = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&nowTime), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

// Function to fetch data from the API
std::string fetchData(const std::string& url) {
    CURL* curl;
    CURLcode res;
    std::string response;

    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
        }
        curl_easy_cleanup(curl);
    }
    return response;
}

std::vector<char> createOSCMessage(const std::string& addressPattern, const std::string& arguments) {
    std::vector<char> message;

    // Add address pattern (e.g., "/density") and pad to 4-byte alignment
    std::string address = addressPattern;
    while (address.size() % 4 != 0) {
        address += '\0';
    }
    message.insert(message.end(), address.begin(), address.end());

    // Add type tag string ",s" (indicating a single string argument) and pad to 4-byte alignment
    std::string typeTag = ",s";
    while (typeTag.size() % 4 != 0) {
        typeTag += '\0';
    }
    message.insert(message.end(), typeTag.begin(), typeTag.end());

    // Add argument (string value) and pad to 4-byte alignment
    std::string argumentStr = arguments + '\0';
    while (argumentStr.size() % 4 != 0) {
        argumentStr += '\0';
    }
    message.insert(message.end(), argumentStr.begin(), argumentStr.end());

    return message;
}

void sendOSCMessage(const std::string& address, const std::string& value, const std::string& ip, int port) {
    int sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockfd < 0) {
        std::cerr << "Socket creation failed" << std::endl;
        return;
    }

    sockaddr_in server_addr = {};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip.c_str(), &server_addr.sin_addr) <= 0) {
        std::cerr << "Invalid address" << std::endl;
        close(sockfd);
        return;
    }

    std::vector<char> oscMessage = createOSCMessage(address, value);

    int sendResult = sendto(sockfd, oscMessage.data(), oscMessage.size(), 0,
                            (sockaddr*)&server_addr, sizeof(server_addr));
    if (sendResult < 0) {
        std::cerr << "sendto failed" << std::endl;
    }

    close(sockfd);
}

// Функция для безопасного парсинга с сохранением старого значения
float parseValueSafely(const json& value, float& lastValidValue, bool& validFlag) {
    try {
        if (value.is_string()) {
            std::string strVal = value.get<std::string>();
            if (!strVal.empty() && strVal != "null" && strVal != "NULL") {
                float newValue = std::stof(strVal);
                lastValidValue = newValue;
                validFlag = true;
                return newValue;
            }
        } else if (value.is_number()) {
            float newValue = value.get<float>();
            lastValidValue = newValue;
            validFlag = true;
            return newValue;
        }
    } catch (const std::exception& e) {
        std::cerr << "Invalid value encountered: " << e.what() << std::endl;
    }
    
    // Если не удалось получить новое значение, используем последнее валидное
    if (validFlag) {
        return lastValidValue;
    }
    return 0.0f;
}

// Аналогичная функция для целых чисел
int parseIntValueSafely(const json& value, int& lastValidValue, bool& validFlag) {
    try {
        if (value.is_number_integer()) {
            int newValue = value.get<int>();
            lastValidValue = newValue;
            validFlag = true;
            return newValue;
        } else if (value.is_string()) {
            std::string strVal = value.get<std::string>();
            if (!strVal.empty() && strVal != "null" && strVal != "NULL") {
                int newValue = std::stoi(strVal);
                lastValidValue = newValue;
                validFlag = true;
                return newValue;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Invalid integer value encountered: " << e.what() << std::endl;
    }
    
    // Если не удалось получить новое значение, используем последнее валидное
    if (validFlag) {
        return lastValidValue;
    }
    return 0;
}

void processData(const std::string& jsonData, SolarData& data) {
    try {
        json parsedData = json::parse(jsonData);

        if (parsedData.size() < 2) {
            std::cerr << "No solar wind data available. Using last valid values." << std::endl;
            // Используем последние валидные значения
            data.density = lastValidData.density_valid ? lastValidData.density : 0.0f;
            data.speed = lastValidData.speed_valid ? lastValidData.speed : 0.0f;
            data.temperature = lastValidData.temperature_valid ? lastValidData.temperature : 0.0f;
            return;
        }

        auto latestData = parsedData[parsedData.size() - 1];

        data.density = parseValueSafely(latestData[1], lastValidData.density, lastValidData.density_valid);
        data.speed = parseValueSafely(latestData[2], lastValidData.speed, lastValidData.speed_valid);
        data.temperature = parseValueSafely(latestData[3], lastValidData.temperature, lastValidData.temperature_valid);

        std::stringstream ssDensity, ssSpeed, ssTemperature;

        // Форматируем и отправляем только если есть валидные данные
        if (lastValidData.density_valid) {
            ssDensity << std::fixed << std::setprecision(3) << data.density;
            sendOSCMessage("/dens", ssDensity.str(), "127.0.0.1", 6000);
            sendOSCMessage("/dens", ssDensity.str(), "127.0.0.1", 6001);
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
        
        if (lastValidData.speed_valid) {
            ssSpeed << std::fixed << std::setprecision(2) << data.speed;
            sendOSCMessage("/speed", ssSpeed.str(), "127.0.0.1", 6000);
            sendOSCMessage("/speed", ssSpeed.str(), "127.0.0.1", 6001);
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
        
        if (lastValidData.temperature_valid) {
            ssTemperature << std::fixed << std::setprecision(3) << data.temperature;
            sendOSCMessage("/temp", ssTemperature.str(), "127.0.0.1", 6000);
            sendOSCMessage("/temp", ssTemperature.str(), "127.0.0.1", 6001);
        }

    } catch (const std::exception& e) {
        std::cerr << "Error parsing solar wind JSON: " << e.what() << std::endl;
        // Используем последние валидные значения при ошибке
        data.density = lastValidData.density_valid ? lastValidData.density : 0.0f;
        data.speed = lastValidData.speed_valid ? lastValidData.speed : 0.0f;
        data.temperature = lastValidData.temperature_valid ? lastValidData.temperature : 0.0f;
    }
}

void processSolarProbabilities(const std::string& jsonData, SolarData& data) {
    try {
        json parsedData = json::parse(jsonData);

        if (parsedData.is_array() && !parsedData.empty()) {
            auto todayData = parsedData[0];

            data.m_class = parseIntValueSafely(todayData["m_class_1_day"], lastValidData.m_class, lastValidData.m_class_valid);
            data.x_class = parseIntValueSafely(todayData["x_class_1_day"], lastValidData.x_class, lastValidData.x_class_valid);

            // Отправляем только если есть валидные данные
            if (lastValidData.m_class_valid) {
                sendOSCMessage("/m_xray", std::to_string(data.m_class), "127.0.0.1", 6000);
                sendOSCMessage("/m_xray", std::to_string(data.m_class), "127.0.0.1", 6001);
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }
            
            if (lastValidData.x_class_valid) {
                sendOSCMessage("/x_xray", std::to_string(data.x_class), "127.0.0.1", 6000);
                sendOSCMessage("/x_xray", std::to_string(data.m_class), "127.0.0.1", 6001);
            }

        } else {
            std::cerr << "No solar probabilities data available. Using last valid values." << std::endl;
            data.m_class = lastValidData.m_class_valid ? lastValidData.m_class : 0;
            data.x_class = lastValidData.x_class_valid ? lastValidData.x_class : 0;
        }

    } catch (const std::exception& e) {
        std::cerr << "Error processing solar probabilities JSON: " << e.what() << std::endl;
        data.m_class = lastValidData.m_class_valid ? lastValidData.m_class : 0;
        data.x_class = lastValidData.x_class_valid ? lastValidData.x_class : 0;
    }
}

void processMagData(const std::string& jsonData, SolarData& data) {
    try {
        json parsedData = json::parse(jsonData);

        if (parsedData.size() < 2) {
            std::cerr << "No magnetometer data available. Using last valid values." << std::endl;
            data.lon_gsm = lastValidData.lon_gsm_valid ? lastValidData.lon_gsm : 0.0f;
            data.bt = lastValidData.bt_valid ? lastValidData.bt : 0.0f;
            data.bz_gsm = lastValidData.bz_gsm_valid ? lastValidData.bz_gsm : 0.0f;
            return;
        }

        auto latestData = parsedData[parsedData.size() - 1];

        data.lon_gsm = parseValueSafely(latestData[4], lastValidData.lon_gsm, lastValidData.lon_gsm_valid);
        data.bt = parseValueSafely(latestData[6], lastValidData.bt, lastValidData.bt_valid);
        data.bz_gsm = parseValueSafely(latestData[3], lastValidData.bz_gsm, lastValidData.bz_gsm_valid);

        std::stringstream ssLonGsm, ssBt, ssBzGsm;

        // Отправляем только если есть валидные данные
        if (lastValidData.lon_gsm_valid) {
            ssLonGsm << std::fixed << std::setprecision(3) << data.lon_gsm;
            sendOSCMessage("/phiGSM", ssLonGsm.str(), "127.0.0.1", 6000);
            sendOSCMessage("/phiGSM", ssLonGsm.str(), "127.0.0.1", 6001);
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
        
        if (lastValidData.bt_valid) {
            ssBt << std::fixed << std::setprecision(2) << data.bt;
            sendOSCMessage("/bt", ssBt.str(), "127.0.0.1", 6000);
            sendOSCMessage("/bt", ssBt.str(), "127.0.0.1", 6001);
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
        
        if (lastValidData.bz_gsm_valid) {
            ssBzGsm << std::fixed << std::setprecision(3) << data.bz_gsm;
            sendOSCMessage("/bzGSM", ssBzGsm.str(), "127.0.0.1", 6000);
            sendOSCMessage("/bzGSM", ssBzGsm.str(), "127.0.0.1", 6001);

        }

    } catch (const std::exception& e) {
        std::cerr << "Error parsing magnetometer JSON: " << e.what() << std::endl;
        data.lon_gsm = lastValidData.lon_gsm_valid ? lastValidData.lon_gsm : 0.0f;
        data.bt = lastValidData.bt_valid ? lastValidData.bt : 0.0f;
        data.bz_gsm = lastValidData.bz_gsm_valid ? lastValidData.bz_gsm : 0.0f;
    }
}

void processKpIndexData(const std::string& jsonData, SolarData& data) {
    try {
        json parsedData = json::parse(jsonData);

        if (parsedData.size() < 2) {
            std::cerr << "No Kp-index data available. Using last valid values." << std::endl;
            data.kp = lastValidData.kp_valid ? lastValidData.kp : 0.0f;
            return;
        }

        auto latestData = parsedData[parsedData.size() - 1];
        data.kp = parseValueSafely(latestData[1], lastValidData.kp, lastValidData.kp_valid);

        if (lastValidData.kp_valid) {
            std::stringstream ssKp;
            ssKp << std::fixed << std::setprecision(2) << data.kp;
            sendOSCMessage("/kp", ssKp.str(), "127.0.0.1", 6000);
            sendOSCMessage("/kp", ssKp.str(), "127.0.0.1", 6001);
        }

    } catch (const std::exception& e) {
        std::cerr << "Error parsing Kp-index JSON: " << e.what() << std::endl;
        data.kp = lastValidData.kp_valid ? lastValidData.kp : 0.0f;
    }
}

int main() {
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    running = 1;

    std::cout << "\n";
    std::cout << "─────────────────────────────────────────\n";
    std::cout << "  SolarDataCatcher v2.0\n";
    std::cout << "  Developed by Elizaveta Fomina\n";
    std::cout << "  KVEF art & science research group\n";

    std::cout << "─────────────────────────────────────────\n";
    std::cout << "✓ Sending data to: 127.0.0.1:6000 & 127.0.0.1:6001\n";
    std::cout << "✓ Update interval: every 60 seconds\n";
    std::cout << "✓ Using last valid values when API unavailable\n";
    std::cout << "✓ Press Ctrl+C to stop\n";
    std::cout << "─────────────────────────────────────────\n";
    std::cout << "Starting data collection...\n";
    
    const std::string apiUrl = "https://services.swpc.noaa.gov/products/solar-wind/plasma-5-minute.json";
    const std::string solarRegionsUrl = "https://services.swpc.noaa.gov/json/solar_regions.json";
    const std::string magApiUrl = "https://services.swpc.noaa.gov/products/solar-wind/mag-5-minute.json";
    const std::string kpIndexUrl = "https://services.swpc.noaa.gov/products/noaa-planetary-k-index.json";
    const std::string solarProbabilitiesUrl = "https://services.swpc.noaa.gov/json/solar_probabilities.json";

    while (running) {
        SolarData currentData;
        
        // Получаем временную метку
        std::string timestamp = getCurrentTimestamp();
        
        // Fetch and process solar wind data
        std::string jsonData = fetchData(apiUrl);
        if (!jsonData.empty()) {
            processData(jsonData, currentData);
        } else {
            std::cerr << "Failed to fetch solar wind data. Using last valid values." << std::endl;
            currentData.density = lastValidData.density_valid ? lastValidData.density : 0.0f;
            currentData.speed = lastValidData.speed_valid ? lastValidData.speed : 0.0f;
            currentData.temperature = lastValidData.temperature_valid ? lastValidData.temperature : 0.0f;
        }

        // Fetch and process solar probabilities data
        std::string solarProbabilitiesData = fetchData(solarProbabilitiesUrl);
        if (!solarProbabilitiesData.empty()) {
            processSolarProbabilities(solarProbabilitiesData, currentData);
        } else {
            std::cerr << "Failed to fetch solar probabilities data. Using last valid values." << std::endl;
            currentData.m_class = lastValidData.m_class_valid ? lastValidData.m_class : 0;
            currentData.x_class = lastValidData.x_class_valid ? lastValidData.x_class : 0;
        }

        // Fetch and process magnetometer data
        std::string magData = fetchData(magApiUrl);
        if (!magData.empty()) {
            processMagData(magData, currentData);
        } else {
            std::cerr << "Failed to fetch magnetometer data. Using last valid values." << std::endl;
            currentData.lon_gsm = lastValidData.lon_gsm_valid ? lastValidData.lon_gsm : 0.0f;
            currentData.bt = lastValidData.bt_valid ? lastValidData.bt : 0.0f;
            currentData.bz_gsm = lastValidData.bz_gsm_valid ? lastValidData.bz_gsm : 0.0f;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        // Fetch and process Kp-index data
        std::string kpData = fetchData(kpIndexUrl);
        if (!kpData.empty()) {
            processKpIndexData(kpData, currentData);
        } else {
            std::cerr << "Failed to fetch Kp-index data. Using last valid values." << std::endl;
            currentData.kp = lastValidData.kp_valid ? lastValidData.kp : 0.0f;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        // Print all data in clean format
        printSolarData(currentData);
        
        // Wait for next update
        for (int i = 0; i < 60 && running; ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    
    std::cout << "The program terminated correctly." << std::endl;
    return 0;
}