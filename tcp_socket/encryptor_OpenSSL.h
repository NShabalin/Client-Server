#pragma once

#include <openssl/evp.h>
#include <vector>
#include <iostream>
#include <fstream>
#include <chrono>
#include <iomanip>

class encryptorOpenSSL {
    private:
    const unsigned char key[32] = {
        0xC0, 0x6C, 0x40, 0xD9, 0x78, 0x7E, 0x00, 0x00,
        0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0,
        0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
        0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00
    };
    const unsigned char iv[16] = {
        0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
        0x10, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE
    };

    std::string logsPath;
    std::string logsEndline;

    public:
    encryptorOpenSSL(std::string logsPath, std::string logsEndline) {
        this->logsPath = logsPath;
        this->logsEndline = logsEndline;
    }

    bool encrypt(std::string &text, std::vector<char> &result) {
        std::vector<unsigned char> textEncrypted;

        // создание новой шифровки
        EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
        if (!ctx) {
            log("Error while creating a cypher." + logsEndline);
            return false;
        }

        // установка типа шифровки AES-256-CBC, движок по умолчанию
        if (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, key, iv) != 1) {
            log("Error while setting cypher parameters." + logsEndline);
            EVP_CIPHER_CTX_free(ctx);
            return false;
        }

        textEncrypted.resize(text.size() + EVP_CIPHER_block_size(EVP_aes_256_cbc()));
        int lengthWritten = 0, textEncryptedLength = 0;

        // шифровка строки
        if (EVP_EncryptUpdate(ctx,
                              textEncrypted.data(),
                              &lengthWritten,
                              reinterpret_cast<const unsigned char*>(text.data()),
                              text.size()) != 1) {
            log("Error while encrypting." + logsEndline);
            EVP_CIPHER_CTX_free(ctx);
            return false;
        }
        textEncryptedLength = lengthWritten;

        // заканчивает шифровку, дошифровывает оставшиеся данные
        if (EVP_EncryptFinal_ex(ctx, textEncrypted.data() + lengthWritten, &lengthWritten) != 1) {
            log("Error with final encryption." + logsEndline);
            EVP_CIPHER_CTX_free(ctx);
            return false;
        }
        textEncryptedLength += lengthWritten;

        textEncrypted.resize(textEncryptedLength);
        EVP_CIPHER_CTX_free(ctx);

        result = std::vector<char>(textEncrypted.begin(), textEncrypted.end());
        
        return true;
    }

    bool decrypt(std::string &text, std::vector<char> &textEnc) {
        std::vector<unsigned char> textEncrypted(textEnc.begin(), textEnc.end());

        // создание новой шифровки 
        EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
        if (!ctx) {
            log("Error while creating a cypher." + logsEndline);
            return false;
        }

        // установка типа шифровки AES-256-CBC, движок по умолчанию
        if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, key, iv) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return false;
        }

        // баффер для расшифровки текста
        std::vector<unsigned char> buffer(textEncrypted.size() + EVP_CIPHER_block_size(EVP_aes_256_cbc()));
        int lengthWritten = 0, textLength = 0;

        // расшифровка текста
        if (EVP_DecryptUpdate(ctx, 
                              buffer.data(), 
                              &lengthWritten, 
                              textEncrypted.data(), 
                              textEncrypted.size()) != 1) {
            log("Error while decrypting" + logsEndline);
            EVP_CIPHER_CTX_free(ctx);
            return false;
        }
        textLength = lengthWritten;

        // заканчивает расшифровку, дорасшифровывает оставшиеся данные
        if (EVP_DecryptFinal_ex(ctx, buffer.data() + lengthWritten, &lengthWritten) != 1) {
            log("Error with final decryption" + logsEndline);
            EVP_CIPHER_CTX_free(ctx);
            return false;
        }
        textLength += lengthWritten;

        
        text.assign(reinterpret_cast<char*>(buffer.data()), textLength);
        EVP_CIPHER_CTX_free(ctx);
        
        return true;
    }

    void log(const std::string message) {
        std::ofstream logFile(logsPath, std::ios_base::app);
        if (logFile.is_open()) {
            logFile << get_current_time() << ": " << message << "\n";
        }
    }

    std::string get_current_time() {
        // текущее время
        auto now = std::chrono::system_clock::now();
        std::time_t now_time = std::chrono::system_clock::to_time_t(now);
        // конвертация в локальное время
        std::tm local_time = *std::localtime(&now_time);
        // в формат "DD-MM-YYYY_HH-MM-SS"
        std::ostringstream oss;
        oss << std::put_time(&local_time, "%d-%m-%Y_%H-%M-%S");

        return oss.str();
    }
};