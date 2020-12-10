/*
* binomo-cpp-api - C ++ API client for binomo
*
* Copyright (c) 2019 Elektro Yar. Email: git.electroyar@gmail.com
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/
#ifndef BINOMO_BOT_SETTINGS_HPP_INCLUDED
#define BINOMO_BOT_SETTINGS_HPP_INCLUDED

#include "../binomo-cpp-api-common.hpp"
#include <nlohmann/json.hpp>

namespace binomo_bot {
    using json = nlohmann::json;

    class BinomoSettings {
    public:
        uint32_t port = 8082;
        std::string sert_file = "curl-ca-bundle.crt";       /**< Файл сертификата */
        std::string cookie_file = "binomo.cookie";     /**< Файл cookie */

        bool is_demo_account = true;    /**< Флаг использования демо счета */

        bool parser(json &j) {
            try {
                json j_binomo = j["binomo"];
                if(j_binomo["port"] != nullptr) port = j_binomo["port"];
                if(j_binomo["cookie_file"] != nullptr) cookie_file = j_binomo["cookie_file"];
                if(j_binomo["sert_file"] != nullptr) sert_file = j_binomo["sert_file"];
                if(j_binomo["demo"] != nullptr) is_demo_account = j_binomo["demo"];
                if(j_binomo["demo_account"] != nullptr) is_demo_account = j_binomo["demo_account"];
            }
            catch(const json::parse_error& e) {
                std::cerr << "binomo bot: BinomoSettings json::parse_error, what: " << e.what()
                   << " exception_id: " << e.id << std::endl;
                return false;
            }
            catch(json::out_of_range& e) {
                std::cerr << "binomo bot: BinomoSettings json::out_of_range, what:" << e.what()
                   << " exception_id: " << e.id << std::endl;
                return false;
            }
            catch(json::type_error& e) {
                std::cerr << "binomo bot: BinomoSettings json::type_error, what:" << e.what()
                   << " exception_id: " << e.id << std::endl;
                return false;
            }
            catch(...) {
                std::cerr << "binomo bot: BinomoSettings json error" << std::endl;
                return false;
            }
            return true;
        }
    };

    class QuotesStreamSettings {
    public:
        std::string path;
        //std::string sert_file = "curl-ca-bundle.crt";       /**< Файл сертификата */
        //std::string cookie_file = "binomo.cookie";          /**< Файл cookie */
        std::string symbol_hst_suffix;                      /**< Суффикс имени символа автономных графиков */

        std::vector<std::pair<std::string, uint32_t>> symbols;

        uint32_t candles = 1440;                            /**< Количество баров истории */
        uint32_t max_precisions = 6;                        /**< Максимальная точность котировок, выступает в роли ограничителя */
        int64_t timezone = 0;                               /**< Часовой пояс - смещение метки времени котировок на указанное число секунд */
        int volume_mode = 0;                                /**< Режим работы объемов (0 - отключено, 1 - подсчет тиков, 2 - взвешенный подсчет тиков) */

        bool is_use = false;

        bool parser(json &j) {
            try {
                json j_quotes = j["quotes"];
                if(j_quotes["volume_mode"] != nullptr) volume_mode = j_quotes["volume_mode"];
                if(j_quotes["symbol_hst_suffix"] != nullptr) symbol_hst_suffix = j_quotes["symbol_hst_suffix"];
                if(j_quotes["candles"] != nullptr) candles = j_quotes["candles"];
                if(j_quotes["max_precisions"] != nullptr) max_precisions = j_quotes["max_precisions"];
                if(j_quotes["timezone"] != nullptr) timezone = j_quotes["timezone"];
                if(j_quotes["path"] != nullptr) path = j_quotes["path"];
                if(j_quotes["symbols"] != nullptr && j_quotes["symbols"].is_array()) {
                    const size_t symbols_size = j_quotes["symbols"].size();
                    for(size_t i = 0; i < symbols_size; ++i) {
                        const std::string symbol = j_quotes["symbols"][i]["symbol"];
                        const uint32_t period = j_quotes["symbols"][i]["period"];
                        symbols.push_back(std::make_pair(symbol,period));
                    }
                }
                if(j_quotes["use"] != nullptr) is_use = j_quotes["use"];
                if(is_use && (path.size() == 0 || symbols.size() == 0)) return false;
            }
            catch(const json::parse_error& e) {
                std::cerr << "binomo bot: QuotesStreamSettings json::parse_error, what: " << e.what()
                   << " exception_id: " << e.id << std::endl;
                return false;
            }
            catch(json::out_of_range& e) {
                std::cerr << "binomo bot: QuotesStreamSettings json::out_of_range, what:" << e.what()
                   << " exception_id: " << e.id << std::endl;
                return false;
            }
            catch(json::type_error& e) {
                std::cerr << "binomo bot: QuotesStreamSettings json::type_error, what:" << e.what()
                   << " exception_id: " << e.id << std::endl;
                return false;
            }
            catch(...) {
                std::cerr << "binomo bot: QuotesStreamSettings json error" << std::endl;
                return false;
            }
            return true;
        }
    };

    class BotSettings {
    public:
        std::string named_pipe = "binomo_api_bot";          /**< Имя именованного канала */
        //std::string sert_file = "curl-ca-bundle.crt";       /**< Файл сертификата */
        //std::string cookie_file = "binomo.cookie";          /**< Файл cookie */
        uint32_t repeated_bet_attempts_delay_ms = 1000;     /**< Задержка между попытками повторных сделок, в мс */
        uint32_t delay_bets_ms = 1000;                      /**< Задержка между сделками, в мс */

        bool parser(json &j) {
            try {
                json j_bot = j["bot"];
                if(j_bot["named_pipe"] != nullptr) named_pipe = j_bot["named_pipe"];
                if(j_bot["delay_bets_ms"] != nullptr) delay_bets_ms = j_bot["delay_bets_ms"];
                if(j_bot["repeated_bet_attempts_delay_ms"] != nullptr) repeated_bet_attempts_delay_ms = j_bot["repeated_bet_attempts_delay_ms"];
                if(j_bot["repeated_bet_attempts_delay"] != nullptr) repeated_bet_attempts_delay_ms = j_bot["repeated_bet_attempts_delay"];
                if(j_bot["bet_attempts_delay"] != nullptr) repeated_bet_attempts_delay_ms = j_bot["bet_attempts_delay"];
                if(j_bot["bet_attempts_delay_ms"] != nullptr) repeated_bet_attempts_delay_ms = j_bot["bet_attempts_delay_ms"];
                if(j_bot["repeated_bet_delay_ms"] != nullptr) repeated_bet_attempts_delay_ms = j_bot["repeated_bet_delay_ms"];
                if(j_bot["repeated_bet_delay"] != nullptr) repeated_bet_attempts_delay_ms = j_bot["repeated_bet_delay"];
            }
            catch(const json::parse_error& e) {
                std::cerr << "binomo bot: BotSettings json::parse_error, what: " << e.what()
                   << " exception_id: " << e.id << std::endl;
                return false;
            }
            catch(json::out_of_range& e) {
                std::cerr << "binomo bot: BotSettings json::out_of_range, what:" << e.what()
                   << " exception_id: " << e.id << std::endl;
                return false;
            }
            catch(json::type_error& e) {
                std::cerr << "binomo bot: BotSettings json::type_error, what:" << e.what()
                   << " exception_id: " << e.id << std::endl;
                return false;
            }
            catch(...) {
                std::cerr << "binomo bot: BotSettings json error" << std::endl;
                return false;
            }
            return true;
        }
    };

    class HotkeySettings {
    public:
        std::string key;
        std::string symbol;
        double amount = 0;
        uint32_t duration = 1;
        int32_t direction = 0;

        bool parser(json &j) {
            try {
                key = j["key"];
                symbol = j["symbol"];
                amount = j["amount"];
                duration = j["duration"];
                direction = j["direction"];
            }
            catch(const json::parse_error& e) {
                std::cerr << "binomo bot: HotkeySettings json::parse_error, what: " << e.what()
                   << " exception_id: " << e.id << std::endl;
                return false;
            }
            catch(json::out_of_range& e) {
                std::cerr << "binomo bot: HotkeySettings json::out_of_range, what:" << e.what()
                   << " exception_id: " << e.id << std::endl;
                return false;
            }
            catch(json::type_error& e) {
                std::cerr << "binomo bot: HotkeySettings json::type_error, what:" << e.what()
                   << " exception_id: " << e.id << std::endl;
                return false;
            }
            catch(...) {
                std::cerr << "binomo bot: HotkeySettings json error" << std::endl;
                return false;
            }
            return true;
        }
    };

    class HotkeysSettings {
    public:
        std::vector<HotkeySettings> hotkey;
        bool is_use = false;

        bool parser(json &j) {
            try {
                json j_hotkeys = j["hotkeys"];
                json j_array_hotkeys = j_hotkeys["array"];
                hotkey.resize(j_array_hotkeys.size());
                for(size_t i = 0; i < j_array_hotkeys.size(); ++i) {
                    hotkey[i].parser(j_array_hotkeys[i]);
                }
                if(j_hotkeys["use"] != nullptr) is_use = j_hotkeys["use"];
            }
            catch(const json::parse_error& e) {
                std::cerr << "binomo bot: HotkeysSettings json::parse_error, what: " << e.what()
                   << " exception_id: " << e.id << std::endl;
                return false;
            }
            catch(json::out_of_range& e) {
                std::cerr << "binomo bot: HotkeysSettings json::out_of_range, what:" << e.what()
                   << " exception_id: " << e.id << std::endl;
                return false;
            }
            catch(json::type_error& e) {
                std::cerr << "binomo bot: HotkeysSettings json::type_error, what:" << e.what()
                   << " exception_id: " << e.id << std::endl;
                return false;
            }
            catch(...) {
                std::cerr << "binomo bot: HotkeysSettings json error" << std::endl;
                return false;
            }
            return true;
        }
    };

    /** \brief Класс настроек
     */
    class Settings {
    public:
        std::string json_settings_file;

        BinomoSettings binomo;
        QuotesStreamSettings quotes_stream;
        BotSettings bot;
        HotkeysSettings hotkeys;

        bool is_error = false;

        Settings() {};

        Settings(const int argc, char **argv) {
            /* обрабатываем аргументы командой строки */
            json j;
            bool is_default = false;
            if(!binomo_api::common::process_arguments(
                    argc,
                    argv,
                    [&](
                        const std::string &key,
                        const std::string &value) {
                /* аргумент json_file указываает на файл с настройками json */
                if(key == "json_settings_file" || key == "jsf" || key == "jf") {
                    json_settings_file = value;
                }
            })) {
                /* параметры не были указаны */
                if(!binomo_api::common::open_json_file("config.json", j)) {
                    is_error = true;
                    return;
                }
                is_default = true;
            }

            if(!is_default && !binomo_api::common::open_json_file(json_settings_file, j)) {
                is_error = true;
                return;
            }

            /* разбираем json сообщение */
            if(!binomo.parser(j)) is_error = true;
            if(!quotes_stream.parser(j)) is_error = true;
            if(!bot.parser(j)) is_error = true;
            if(!hotkeys.parser(j)) is_error = true;
        }
    };
}

#endif // BINOMO_BOT_SETTINGS_HPP_INCLUDED
