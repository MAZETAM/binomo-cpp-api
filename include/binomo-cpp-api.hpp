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
#ifndef PRIME_XBT_API_HPP_INCLUDED
#define PRIME_XBT_API_HPP_INCLUDED

#include "binomo-cpp-api-common.hpp"
#include "server_wss.hpp"
#include <openssl/ssl.h>
#include <wincrypt.h>
#include <xtime.hpp>
#include <nlohmann/json.hpp>
#include <mutex>
#include <atomic>
#include <future>
//#include <cstdlib>

namespace binomo_api {

    /** \brief Класс API брокера Binomo
     *
     * Тут подробное описание
     */
    class BinomoApi {
    public:
        using json = nlohmann::json;
        using WsServer = SimpleWeb::SocketServer<SimpleWeb::WS>;

        std::function<void()> on_start = nullptr;
        std::function<void()> on_update_account = nullptr;
        std::function<void()> on_update_payout = nullptr;

    private:
        uint32_t api_port = 8082;		                                        /**< Порт для подключения к расширению в браузере */

        const uint32_t join_ref = 5;

        std::mutex request_future_mutex;
        std::vector<std::future<void>> request_future;
        std::atomic<bool> is_request_future_shutdown = ATOMIC_VAR_INIT(false);

        /** \brief Очистить список запросов
         */
        void clear_request_future() {
            std::lock_guard<std::mutex> lock(request_future_mutex);
            size_t index = 0;
            while(index < request_future.size()) {
                try {
                    if(request_future[index].valid()) {
                        std::future_status status = request_future[index].wait_for(std::chrono::milliseconds(0));
                        if(status == std::future_status::ready) {
                            request_future[index].get();
                            request_future.erase(request_future.begin() + index);
                            continue;
                        }
                    }
                }
                catch(const std::exception &e) {
                    std::cerr << "binomo api: server error in function clear_request_future, what: " << e.what() << std::endl;
                }
                catch(...) {
                    std::cerr << "binomo api: server error in function clear_request_future()" << std::endl;
                }
                ++index;
            }
        }

		std::future<void> server_future;
        std::shared_ptr<WsServer> server;           				            /**< WS-Сервер */
		std::mutex server_mutex;
        std::shared_ptr<WsServer::Connection> current_connection;               /**< Текущее соединение */
		std::mutex current_connection_mutex;

		std::atomic<uint64_t> ref_counter = ATOMIC_VAR_INIT(5);                 /**< Счетчик запросов. Начинается с 5 и увеличивается с каждым запросом */

		//std::atomic<bool> is_command_server_stop = ATOMIC_VAR_INIT(false);      /**< Команда на остановку сервера */
		std::atomic<bool> is_shutdown = ATOMIC_VAR_INIT(false);                 /**< Команда на остановку сервера */
        std::atomic<bool> is_cout_log = ATOMIC_VAR_INIT(false);                 /**< Флаг вывода логов на экран */
		std::atomic<bool> is_connected = ATOMIC_VAR_INIT(false);                /**< Флаг установленного соединения */
        std::atomic<bool> is_open_connect = ATOMIC_VAR_INIT(false);             /**<  */
        std::atomic<bool> is_error = ATOMIC_VAR_INIT(false);                    /**<  */

        std::atomic<double> bets_last_timestamp = ATOMIC_VAR_INIT(0.0d);	    /**< Последняя метка времени открытия сделки */
        std::atomic<double> bets_delay = ATOMIC_VAR_INIT(1.5d);         	    /**< Задержка между открытием сделок */

        /* все для расчета смещения времени */
        std::atomic<double> last_timestamp = ATOMIC_VAR_INIT(0.0);
        const uint32_t array_offset_timestamp_size = 256;
        std::array<xtime::ftimestamp_t, 256> array_offset_timestamp;            /**< Массив смещения метки времени */
        uint8_t index_array_offset_timestamp = 0;                               /**< Индекс элемента массива смещения метки времени */
        uint32_t index_array_offset_timestamp_count = 0;
        xtime::ftimestamp_t last_offset_timestamp_sum = 0;
        std::atomic<double> offset_timestamp = ATOMIC_VAR_INIT(0.0);            /**< Смещение метки времени */

        std::atomic<double> last_server_timestamp;

        /** \brief Обновить смещение метки времени
         *
         * Данный метод использует оптимизированное скользящее среднее
         * для выборки из 256 элеметов для нахождения смещения метки времени сервера
         * относительно времени компьютера
         * \param offset смещение метки времени
         */
        inline void update_offset_timestamp(const xtime::ftimestamp_t &offset) {
            if(index_array_offset_timestamp_count != array_offset_timestamp_size) {
                array_offset_timestamp[index_array_offset_timestamp] = offset;
                index_array_offset_timestamp_count = (uint32_t)index_array_offset_timestamp + 1;
                last_offset_timestamp_sum += offset;
                offset_timestamp = last_offset_timestamp_sum / (xtime::ftimestamp_t)index_array_offset_timestamp_count;
                ++index_array_offset_timestamp;
                return;
            }
            /* находим скользящее среднее смещения метки времени сервера относительно компьютера */
            last_offset_timestamp_sum = last_offset_timestamp_sum +
                (offset - array_offset_timestamp[index_array_offset_timestamp]);
            array_offset_timestamp[index_array_offset_timestamp++] = offset;
            offset_timestamp = last_offset_timestamp_sum/
                (xtime::ftimestamp_t)array_offset_timestamp_size;
        }

        /* параметры аккаунта */
		common::AccountConfig account_config;                                   /**< Параметры аккаунта (баланс счета и прочее) */
		std::mutex account_config_mutex;

        std::map<std::string, uint64_t> uuid_to_bet_id;                         /**< Карта преобразования UUID в BET ID сделки */
        std::mutex uuid_to_bet_id_mutex;

        std::map<uint32_t, uint32_t> ref_to_bet_id;                             /**< Карта преобразования RID в BET ID сделки */
		std::mutex ref_to_bet_id_mutex;

		std::map<uint32_t, uint32_t> broker_bet_id_to_bet_id;                   /**< Карта преобразования BROKER BET ID в BET ID сделки */
		std::mutex broker_bet_id_to_bet_id_mutex;

        std::map<uint64_t, common::Bet> array_bets;
		std::mutex array_bets_mutex;

        uint64_t bets_id_counter = 0;                                           /**< Счетчик номера сделок, открытых через API */
		std::mutex bets_id_counter_mutex;

        //std::map<std::string, PayoutConfig> payout_config;
        std::mutex payout_config_mutex;
        std::atomic<bool> is_init_payout_config = ATOMIC_VAR_INIT(false);

        //std::map<std::string, SymbolConfig> symbols_config;
		std::mutex symbols_config_mutex;
		std::atomic<bool> is_init_symbols_config = ATOMIC_VAR_INIT(false);

		/* параметры экспираций */
		//DurationConfig duration_config;
		std::mutex duration_config_mutex;
		std::atomic<bool> is_init_duration_config = ATOMIC_VAR_INIT(false);

        /** \brief Отправить сообщение
         * \param message Сообщение
         */
        void send(const std::string &message) {
            std::lock_guard<std::mutex> lock(current_connection_mutex);
            if(!current_connection) return;
            current_connection->send(message, [&](const SimpleWeb::error_code &ec) {
                if(ec) {
                    // See http://www.boost.org/doc/libs/1_55_0/doc/html/boost_asio/reference.html, Error Codes for error code meanings
                    if(is_cout_log) {
                        std::cout << "binomo api: server error sending message, "
                            << ec << ", error message: " << ec.message() << std::endl;
                    }
                }
            });
        }

        /** \brief Открыть сделку в асинхронном режиме
         * \param symbol_name Имя символа
         * \param note Заметка пользователя для ставки
         * \param amount Размер ставки
         * \param is_demo Флаг демо аккаунта
         * \param contract_type Направление ставки
         * \param timestamp Метка времени открытия
         * \param expire_at_timestamp Экспирация опциона
         * \param api_bet_id API BET ID сделки
         * \param callback Функция обратного вызова
         * \return Код ошибки
         */
        int async_open_bo(
                const std::string &symbol_name,
                const std::string &note,
                const double amount,
                const bool is_demo,
                const int contract_type,
                const double timestamp,
                const xtime::timestamp_t expire_at_timestamp,
                uint64_t &api_bet_id,
                std::function<void(const common::Bet &bet)> callback = nullptr) {
            if(!is_connected) return common::AUTHORIZATION_ERROR;

            if (contract_type != common::ContractType::BUY &&
				contract_type != common::ContractType::SELL) return common::INVALID_CONTRACT_TYPE;
            if(timestamp == 0 || expire_at_timestamp == 0) return common::INVALID_CONTRACT_TYPE;

            std::string symbol = common::normalize_symbol_name(symbol_name);
			auto it_ric = common::normalize_name_to_ric.find(symbol);
            if(it_ric == common::normalize_name_to_ric.end()) return common::DATA_NOT_AVAILABLE;

            auto it_name = common::normalize_name_to_name.find(symbol);
            if(it_name == common::normalize_name_to_name.end()) return common::DATA_NOT_AVAILABLE;

            auto it_id = common::normalize_name_to_id.find(symbol);
            if(it_id == common::normalize_name_to_id.end()) return common::DATA_NOT_AVAILABLE;

			/* отправим
                {
                    "topic":"base",
                    "event":"create_deal",
                    "payload":{
                        "asset":"Z-CRY/IDX",
                        "asset_id":347,
                        "asset_name":"Crypto IDX",
                        "amount":100,
                        "source":"mouse",
                        "trend":"put",
                        "expire_at":1602514200,
                        "created_at":1602514126419,
                        "option_type":"turbo",
                        "deal_type":"demo",
                        "tournament_id":null},
                    "ref":"274",
                    "join_ref":"5"
                }
             */
            // {"event":"phx_reply","payload":{"response":{"uuid":"ea101909-5373-44e9-b807-694629d2f0d2"},"status":"ok"},"ref":"274","topic":"base"}
            // {"event":"deal_created","payload":{"amount":100,"asset_id":347,"asset_name":"Crypto IDX","asset_ric":"Z-CRY/IDX","close_quote_created_at":"2020-10-12T14:50:00Z","close_rate":0.0,"created_at":"2020-10-12T14:48:46.618757Z","deal_type":"demo","id":1825087908,"name":"Crypto IDX","open_quote_created_at":"2020-10-12T14:48:47.000000Z","open_rate":641.86854915,"option_type":"turbo","payment":183.0,"payment_rate":83,"requested_at":"2020-10-12T14:48:46.604253","ric":"Z-CRY/IDX","status":"open","trend":"put","uuid":"ea101909-5373-44e9-b807-694629d2f0d2","win":0},"ref":null,"topic":"base"}
            // {"event":"change_balance","payload":{"balance":0,"balance_version":0,"bonus":null,"demo_balance":99583,"demo_balance_version":8,"trading_accounts":[{"balance":0,"balance_version":0,"type":"real"},{"balance":99583,"balance_version":8,"type":"demo"}]},"ref":null,"topic":"base"}
            // отправим {"topic":"phoenix","event":"heartbeat","payload":{},"ref":"275"}
            // отправим {"topic":"base","event":"ping","payload":{},"ref":"276","join_ref":"5"}
            // {"event":"majority_opinion","payload":{"asset":"EUR/GBP","call":66,"put":34},"ref":null,"topic":"base"}
            // {"event":"phx_reply","payload":{"response":{},"status":"ok"},"ref":"275","topic":"phoenix"}
            // {"event":"phx_reply","payload":{"response":{"now":"2020-10-12T14:48:55.932967Z"},"status":"ok"},"ref":"276","topic":"base"}
            // {"event":"close_deal_batch","payload":{"end_rate":641.868549545,"finished_at":"2020-10-12T14:54:00Z","ric":"Z-CRY/IDX"},"ref":null,"topic":"base"}

            /* увеличиваем номер запроса,
             * запомним его позже, уже внутри потока
             */
            const uint64_t current_ref = ref_counter++;

            json j;
            j["topic"] = "base";
            j["event"] = "create_deal";
            j["payload"]["asset"] = it_ric->second;
            j["payload"]["asset_id"] = it_id->second;
            j["payload"]["asset_name"] = it_name->second;
            j["payload"]["amount"] = (uint64_t)(amount * 100.0d);
            j["payload"]["source"] = "mouse";
            if(contract_type == common::ContractType::BUY) j["payload"]["trend"] = "call";
            else if(contract_type == common::ContractType::SELL) j["payload"]["trend"] = "put";
            j["payload"]["expire_at"] = expire_at_timestamp;
            j["payload"]["created_at"] = (uint64_t)(timestamp * 1000.0d);
            j["payload"]["option_type"] = "turbo";
            if(is_demo) j["payload"]["deal_type"] = "demo";
            else j["payload"]["deal_type"] = "real";
            j["payload"]["tournament_id"] = nullptr;
            j["ref"] = current_ref;
            j["join_ref"] = join_ref;

            common::Bet user_bet;

            /* запоминаем и увеличиваем счетчик ID сделки внутри API */
            {
                std::lock_guard<std::mutex> lock(bets_id_counter_mutex);
                user_bet.api_bet_id = bets_id_counter;
                api_bet_id = user_bet.api_bet_id;
                ++bets_id_counter;
            }

            user_bet.symbol_name = symbol;
            user_bet.note = note;
            user_bet.contract_type = contract_type;
            //user_bet.duration = duration;
            user_bet.amount = amount;
            user_bet.opening_timestamp = timestamp;
            user_bet.closing_timestamp = expire_at_timestamp;
            user_bet.is_demo = is_demo;
            user_bet.bet_status = common::BetStatus::UNKNOWN_STATE;

            /* запускаем асинхронное открытие сделки */
            {
                std::lock_guard<std::mutex> lock(request_future_mutex);
                request_future.resize(request_future.size() + 1);
                request_future.back() = std::async(std::launch::async,
                        [&, j, user_bet, current_ref, api_bet_id,
                         expire_at_timestamp, callback] {

                    /* проверяем, не надо ли подождать перед открытием сделки */
                    if(bets_last_timestamp > 0) {
                        while(xtime::get_ftimestamp() < (bets_last_timestamp + bets_delay)) {
                            if(is_shutdown) return;
                            std::this_thread::sleep_for(std::chrono::milliseconds(50));
                        };
                    }

                    common::Bet bet = user_bet;

                    /* время открытия сделки */
                    bet.send_timestamp = get_server_timestamp();
                    bets_last_timestamp = xtime::get_ftimestamp();

                    /* запоминаем сделку */
                    {
                        std::lock_guard<std::mutex> lock(array_bets_mutex);
                        array_bets[api_bet_id] = bet;
                    }

                    /* запоминаем соотношение запрос - номер сделки */
                    {
                        std::lock_guard<std::mutex> lock(ref_to_bet_id_mutex);
                        ref_to_bet_id[current_ref] = api_bet_id;
                    }

                    /* отправляем запрос */
                    send(j.dump());

                    if(callback != nullptr) callback(bet);

                    /* ждем в цикле, пока сделка не закроется */
                    common::BetStatus last_bet_status = common::BetStatus::UNKNOWN_STATE;
                    while(!is_request_future_shutdown) {
                        const int DELAY = 50;
                        std::this_thread::yield();
                        std::this_thread::sleep_for(std::chrono::milliseconds(DELAY));
                        {
                            std::lock_guard<std::mutex> lock(array_bets_mutex);
                            auto it_array_bets = array_bets.find(api_bet_id);
                            if(it_array_bets == array_bets.end()) {
                                /* сделка не найдена, значит ошибка проверки */
                                bet.bet_status = common::BetStatus::CHECK_ERROR;
                            } else {
                                /* сделка найдена, но состояние не поменялось */
                                if(it_array_bets->second.bet_status == last_bet_status) continue;
                                last_bet_status = it_array_bets->second.bet_status;

                                const uint64_t stop_timestamp = user_bet.closing_timestamp + xtime::SECONDS_IN_MINUTE;
                                const uint64_t server_timestamp = get_server_timestamp();
                                if(server_timestamp > stop_timestamp) {
                                    bet.bet_status = common::BetStatus::CHECK_ERROR;
                                } else {
                                    bet = it_array_bets->second;
                                }
                            }
                        }

                        if(callback != nullptr) callback(bet);

                        if (bet.bet_status != common::BetStatus::WAITING_COMPLETION &&
                            bet.bet_status != common::BetStatus::UNKNOWN_STATE) {

                            /* чистим память от ненужных данных */
                            if (bet.broker_bet_id != 0) {
                                std::lock_guard<std::mutex> lock(broker_bet_id_to_bet_id_mutex);
                                auto it_broker_bet_id = broker_bet_id_to_bet_id.find(bet.broker_bet_id);
                                if(it_broker_bet_id != broker_bet_id_to_bet_id.end()) broker_bet_id_to_bet_id.erase(it_broker_bet_id);
                            }

                            /* удаляем соотношение номер запроса - номер сделки */
                            {
                                std::lock_guard<std::mutex> lock(ref_to_bet_id_mutex);
                                auto it_ref_to_bet_id = ref_to_bet_id.find(current_ref);
                                if(it_ref_to_bet_id != ref_to_bet_id.end()) ref_to_bet_id.erase(it_ref_to_bet_id);
                            }

                            /* удаляем сделку из массива сделок */
                            {
                                std::lock_guard<std::mutex> lock(array_bets_mutex);
                                auto it_api_bet_id = array_bets.find(api_bet_id);
                                if(it_api_bet_id != array_bets.end()) array_bets.erase(api_bet_id);
                            }
                            break;
                        }
                    };
                });
            }
            clear_request_future();
            return common::OK;
        };

        bool parse_change_balance(json &j) {
            // {"event":"change_balance","payload":{"balance":0,"balance_version":0,"bonus":null,"demo_balance":99809,"demo_balance_version":64,"trading_accounts":[{"balance":0,"balance_version":0,"type":"real"},{"balance":99809,"balance_version":64,"type":"demo"}]},"ref":null,"topic":"base"}
            try {
                if (j["event"] == "change_balance" && j["topic"] == "base") {
                    json j_payload = j["payload"];
                    json j_trading_accounts = j_payload["trading_accounts"];
                    for(size_t i = 0; i < j_trading_accounts.size(); ++i) {
                        if(j_trading_accounts[i]["type"] == "real") {
                            std::lock_guard<std::mutex> lock(account_config_mutex);
                            account_config.real_balance = ((double)j_trading_accounts[i]["balance"]) / 100.0d;
                        } else
                        if(j_trading_accounts[i]["type"] == "demo") {
                            std::lock_guard<std::mutex> lock(account_config_mutex);
                            account_config.demo_balance = ((double)j_trading_accounts[i]["balance"]) / 100.0d;
                        }
                    }
                    return true;
                }
            }
            catch(...) {}
            return false;
        }

        bool parse_phx_reply(json &j) {
            // {"event":"phx_reply","payload":{"response":{"uuid":"ea101909-5373-44e9-b807-694629d2f0d2"},"status":"ok"},"ref":"274","topic":"base"}
            // {"event":"phx_reply","payload":{"response":{"reason":"unmatchedtopic"},"status":"error"},"ref":"274","topic":"base"}
            // {"event":"phx_reply","payload":{"response":{"reasons":[{"field":"expire_at","validation":"asset_unavailable_at_expire_time"}]},"status":"error"},"ref":7,"topic":"base"}

            try {
                if (j["event"] == "phx_reply" && j["topic"] == "base") {
                    json j_payload = j["payload"];
                    const uint32_t ref_id = j["ref"];
                    uint32_t api_bet_id = 0;
                    {
                        std::lock_guard<std::mutex> lock(ref_to_bet_id_mutex);
                        auto it_ref = ref_to_bet_id.find(ref_id);
                        if(it_ref == ref_to_bet_id.end()) return true;
                        api_bet_id = it_ref->second;
                    }
                    if(j_payload["status"] == "ok") {
                        /* запоминаем, какой UUID соответствует сделке */
                        const std::string uuid = j_payload["response"]["uuid"];
                        std::lock_guard<std::mutex> lock(uuid_to_bet_id_mutex);
                        uuid_to_bet_id[uuid] = api_bet_id;
                    } else {
                        /* находим сделку и помечаем ее как с ошибкой */
                        std::lock_guard<std::mutex> lock(array_bets_mutex);
                        auto it_array_bets = array_bets.find(api_bet_id);
                        if(it_array_bets == array_bets.end()) {
                           return true;
                        } else {
                            it_array_bets->second.bet_status = common::BetStatus::OPENING_ERROR;
                        }
                    }
                    return true;
                }
            }
            catch(...) {}
            return false;
        }

        bool parse_deal_created(json &j) {
            /* {
                "event":"deal_created",
                "payload":{
                    "amount":100,
                    "asset_id":347,
                    "asset_name":"Crypto IDX",
                    "asset_ric":"Z-CRY/IDX",
                    "close_quote_created_at":"2020-10-12T14:50:00Z",
                    "close_rate":0.0,
                    "created_at":"2020-10-12T14:48:46.618757Z",
                    "deal_type":"demo",
                    "id":1825087908,
                    "name":"Crypto IDX",
                    "open_quote_created_at":"2020-10-12T14:48:47.000000Z",
                    "open_rate":641.86854915,
                    "option_type":"turbo",
                    "payment":183.0,
                    "payment_rate":83,
                    "requested_at":"2020-10-12T14:48:46.604253",
                    "ric":"Z-CRY/IDX",
                    "status":"open",
                    "trend":"put",
                    "uuid":"ea101909-5373-44e9-b807-694629d2f0d2",
                    "win":0
                },
                "ref":null,
                "topic":"base"
            }
            */
            try {
                if (j["event"] == "deal_created" && j["topic"] == "base") {
                    json j_payload = j["payload"];

                    const std::string uuid = j_payload["uuid"];
                    const uint64_t broker_bet_id  = j_payload["id"];

                    uint32_t api_bet_id = 0;
                    {
                        std::lock_guard<std::mutex> lock(uuid_to_bet_id_mutex);
                        auto it_uuid = uuid_to_bet_id.find(uuid);
                        if(it_uuid == uuid_to_bet_id.end()) return true;
                        api_bet_id = it_uuid->second;
                    }
                    {
                        std::lock_guard<std::mutex> lock(broker_bet_id_to_bet_id_mutex);
                        broker_bet_id_to_bet_id[broker_bet_id] = api_bet_id;
                    }
                    {
                        std::lock_guard<std::mutex> lock(array_bets_mutex);
                        auto it_array_bets = array_bets.find(api_bet_id);
                        if(it_array_bets == array_bets.end()) {
                           return true;
                        } else {
                            it_array_bets->second.broker_bet_id = broker_bet_id;
                            const std::string open_quote_created_at = j_payload["open_quote_created_at"];
                            const std::string close_quote_created_at = j_payload["close_quote_created_at"];
                            const std::string created_at = j_payload["created_at"];
                            const std::string requested_at = j_payload["requested_at"];
                            ///
                            xtime::DateTime open_date_time;
                            xtime::DateTime close_date_time;
                            xtime::DateTime requested_date_time;
                            if(!xtime::convert_iso(created_at, open_date_time)) {
                                it_array_bets->second.bet_status = common::BetStatus::CHECK_ERROR;
                                return true;
                            }
                            it_array_bets->second.opening_timestamp = open_date_time.get_ftimestamp();
                            ///
                            if(!xtime::convert_iso(close_quote_created_at, close_date_time)) {
                                it_array_bets->second.bet_status = common::BetStatus::CHECK_ERROR;
                                return true;
                            }
                            it_array_bets->second.closing_timestamp = close_date_time.get_ftimestamp();
                            ///
                            if(!xtime::convert_iso(requested_at, requested_date_time)) {
                                it_array_bets->second.bet_status = common::BetStatus::CHECK_ERROR;
                                return true;
                            }
                            it_array_bets->second.requested_timestamp = requested_date_time.get_ftimestamp();
                            ///
                            it_array_bets->second.amount = ((double)j_payload["amount"]) / 100.0d;
                            it_array_bets->second.payment = ((double)j_payload["payment"]) / 100.0d;
                            it_array_bets->second.payout = ((double)j_payload["payment_rate"]) / 100.0d;
                            it_array_bets->second.open_price = j_payload["open_rate"];
                            it_array_bets->second.bet_status = common::BetStatus::WAITING_COMPLETION;
                        }
                    }
                    return true;
                }
            }
            catch(...) {
                std::cerr << "binomo api: parse_settings--->json::error" << std::endl;
                return true;
            }
            return false;
        }

        /** \brief Парсер сообщения о хакрытии серии сделок
         */
        bool parse_close_deal_batch(json &j) {
             // {"event":"close_deal_batch","payload":{"end_rate":641.868549545,"finished_at":"2020-10-12T14:54:00Z","ric":"Z-CRY/IDX"},"ref":null,"topic":"base"}
            try {
                if (j["event"] == "close_deal_batch" && j["topic"] == "base") {
                    json j_payload = j["payload"];
                    const double end_rate = j_payload["end_rate"];
                    const std::string finished_at = j_payload["finished_at"];
                    const std::string ric = j_payload["ric"];
                    auto it_symbol = common::ric_to_normalize_name.find(ric);
                    if(it_symbol == common::ric_to_normalize_name.end()) return true;
                    std::string symbol_name = it_symbol->second;

                    xtime::DateTime close_date_time;
                    if(!xtime::convert_iso(finished_at, close_date_time)) {
                        return true;
                    }
                    xtime::ftimestamp_t closing_timestamp = close_date_time.get_ftimestamp();

                    //std::cout << "close_deal_batch " << symbol_name << " end_rate " << end_rate << " closing_timestamp " << closing_timestamp << std::endl;

                    std::lock_guard<std::mutex> lock(array_bets_mutex);
                    for(auto &bet : array_bets) {
                        if(bet.second.symbol_name != symbol_name) continue;
                        const uint64_t t1 = (uint64_t)(bet.second.closing_timestamp + 0.5d);
                        const uint64_t t2 = (uint64_t)(closing_timestamp + 0.5d);
                        if(t1 != t2) continue;
                        bet.second.close_price = end_rate;
                        if(bet.second.contract_type == common::BUY) {
                            if(bet.second.close_price > bet.second.open_price) {
                                bet.second.bet_status = common::BetStatus::WIN;
                                bet.second.profit = bet.second.payment;
                            } else {
                                bet.second.bet_status = common::BetStatus::LOSS;
                            }
                        } else
                        if(bet.second.contract_type == common::SELL) {
                            if(bet.second.close_price < bet.second.open_price) {
                                bet.second.bet_status = common::BetStatus::WIN;
                                bet.second.profit = bet.second.payment;
                            } else {
                                bet.second.bet_status = common::BetStatus::LOSS;
                            }
                        } else {
                            bet.second.bet_status = common::BetStatus::CHECK_ERROR;
                        }
                    }
                    return true;
                }
            }
            catch(...) {
                std::cerr << "binomo api: parse_close_deal_batch--->json::error" << std::endl;
                return true;
            }
            return false;
        }

        bool parse_socket(json &j) {
            try {
				if(j["event"] == "socket" && j["body"]["status"] != nullptr) {
					/* подключение только что произошло, обнуляем параметры */
                    {
                        is_connected = false;
                        is_init_payout_config = false;
                    }
					if(j["body"]["status"] == "open") {
                        {
                            std::lock_guard<std::mutex> lock(account_config_mutex);
                            account_config.autchtoken = j["body"]["authtoken"];
                            account_config.device_id = j["body"]["device_id"];
                        }
                        // {"topic":"base","event":"phx_join","payload":{},"ref":"5","join_ref":"5"}
                        // {"topic":"base","event":"ping","payload":{},"ref":"7","join_ref":"5"}
                        {
                            ref_counter = join_ref;
                            const uint64_t current_ref = ref_counter++;
                            json j;
                            j["topic"] = "base";
                            j["event"] = "phx_join";
                            j["payload"] = json::object();
                            j["ref"] = current_ref;
                            j["join_ref"] = join_ref;
                            send(j.dump());
                        }
                        {
                            const uint64_t current_ref = ref_counter++;
                            json j;
                            j["topic"] = "base";
                            j["event"] = "ping";
                            j["payload"] = json::object();
                            j["ref"] = current_ref;
                            j["join_ref"] = join_ref;
                            send(j.dump());
                        }

                        std::cerr << "binomo api: connection with the broker is open" << std::endl;
					}
                    return true;
				}
			}
			catch(json::out_of_range& e) {
                std::cerr << "binomo api: parse_socket->json::out_of_range, what:" << e.what()
                   << " exception_id: " << e.id << std::endl;
            }
            catch(json::type_error& e) {
                std::cerr << "binomo api: parse_socket->json::type_error, what: " << e.what()
                   << " exception_id: " << e.id << std::endl;
            }
			catch(...) {
                std::cerr << "binomo api: parse_socket->json::error" << std::endl;
			}
            return false;
        }

        void init_main_thread(const uint32_t port) {
            is_shutdown = false;
            is_cout_log = false;
            is_connected = false;
            is_error = false;

            server_future = std::async(std::launch::async,[&, port]() {
                while(!is_shutdown) {
                    is_open_connect = false;
                    {
                        std::lock_guard<std::mutex> lock(server_mutex);
                        server = std::make_shared<WsServer>();
                        server->config.port = port;
                        auto &binomo = server->endpoint["^/binomo-api/?$"];

                        /* принимаем сообщения */
                        binomo.on_message = [&](std::shared_ptr<WsServer::Connection> connection, std::shared_ptr<WsServer::InMessage> in_message) {
                            auto out_message = in_message->string();
                            if(is_cout_log) std::cout << "binomo server: message received: \"" << out_message << "\" from " << connection.get() << std::endl;
                            try {
                                json j = json::parse(out_message);
                                while(true) {
                                    if(parse_phx_reply(j)) break;
                                    if(parse_deal_created(j)) break;
                                    if(parse_close_deal_batch(j)) break;
                                    if(parse_change_balance(j)) break;
                                    if(parse_socket(j)) break;

                                    /*
                                    if(!j.is_array() && j["connection_status"] == "ok") {
                                        is_connected = true;
                                        is_error = false;
                                        if(on_start != nullptr) on_start();
                                        break;
                                    } else
                                    if(!j.is_array() && j["connection_status"] == "error") {
                                        is_error = true;
                                        is_connected = false;
                                        break;
                                    } else
                                    if(!j.is_array() && j["candle-history"] == "error") {
                                        is_error_hist_candles = true;
                                        break;
                                    }
                                    */
                                    break;
                                }
                                if (!is_connected// &&
                                    //is_init_duration_config &&
                                    //is_init_payout_config &&
                                    //is_init_symbols_config// &&
                                    //account_config.is_amount_init &&
                                    //account_config.is_btc_balance_init &&
                                    //account_config.is_usd_balance_init
                                    ) {
                                    is_connected = true;

                                    std::lock_guard<std::mutex> lock(request_future_mutex);
                                    request_future.resize(request_future.size() + 1);
                                    request_future.back() = std::async(std::launch::async,
                                            [&] {
                                        if(on_start != nullptr) on_start();
                                        if(on_update_account != nullptr) on_update_account();
                                    });
                                }
                                clear_request_future();
                            }
                            catch(const json::parse_error& e) {
                                is_error = true;
                                std::string temp;
                                if(out_message.size() > 128) {
                                    temp = out_message.substr(0,128);
                                    temp += "...";
                                } else {
                                    temp = out_message;
                                }
                                std::cerr << "binomo api: json::parse_error, what: " << e.what()
                                   << " exception_id: " << e.id << " message: "
                                   << std::endl << temp << std::endl;
                            }
                            catch(json::out_of_range& e) {
                                is_error = true;
                                std::string temp;
                                if(out_message.size() > 128) {
                                    temp = out_message.substr(0,128);
                                    temp += "...";
                                } else {
                                    temp = out_message;
                                }
                                std::cerr << "binomo api: json::out_of_range, what:" << e.what()
                                   << " exception_id: " << e.id << " message: "
                                   << std::endl << temp << std::endl;
                            }
                            catch(json::type_error& e) {
                                is_error = true;
                                std::string temp;
                                if(out_message.size() > 128) {
                                    temp = out_message.substr(0,128);
                                    temp += "...";
                                } else {
                                    temp = out_message;
                                }
                                std::cerr << "binomo api: json::type_error, what: " << e.what()
                                   << " exception_id: " << e.id << " message: "
                                   << std::endl << temp << std::endl;
                            }
                            catch(...) {
                                is_error = true;
                                std::string temp;
                                if(out_message.size() > 128) {
                                    temp = out_message.substr(0,128);
                                    temp += "...";
                                } else {
                                    temp = out_message;
                                }
                                std::cerr << "binomo api: json error," << " message: "
                                   << std::endl << temp << std::endl;
                            }
                        };

                        binomo.on_open = [&](std::shared_ptr<WsServer::Connection> connection) {
                            {
                                std::lock_guard<std::mutex> lock(current_connection_mutex);
                                current_connection = connection;
                            }
                            if(is_cout_log) std::cout << "binomo api: opened connection: " << connection.get() << std::endl;
                            is_open_connect = true;
                        };

                        // See RFC 6455 7.4.1. for status codes
                        binomo.on_close = [&](std::shared_ptr<WsServer::Connection> connection, int status, const std::string & /*reason*/) {
                            {
                                std::lock_guard<std::mutex> lock(current_connection_mutex);
                                if(current_connection.get() == connection.get()) {
                                    is_connected = false;
                                    current_connection.reset();
                                }
                            }
                            if(is_cout_log) std::cout << "binomo api: closed connection: " << connection.get() << " with status code: " << status << std::endl;
                        };
                        // Can modify handshake response headers here if needed
                        binomo.on_handshake = [](std::shared_ptr<WsServer::Connection> /*connection*/, SimpleWeb::CaseInsensitiveMultimap & /*response_header*/) {
                            return SimpleWeb::StatusCode::information_switching_protocols; // Upgrade to websocket
                        };

                        // See http://www.boost.org/doc/libs/1_55_0/doc/html/boost_asio/reference.html, Error Codes for error code meanings
                        binomo.on_error = [&](std::shared_ptr<WsServer::Connection> connection, const SimpleWeb::error_code &ec) {
                            {
                                std::lock_guard<std::mutex> lock(current_connection_mutex);
                                if(current_connection.get() == connection.get()) {
                                    is_connected = false;
                                    current_connection.reset();
                                }
                            }
                            is_error = true;
                            if(is_cout_log) std::cout << "binomo api: error in connection " << connection.get() << ". "
                                << "Error: " << ec << ", error message: " << ec.message() << std::endl;
                        };
                    }

                    server->start([&](unsigned short port) {
                        if(is_cout_log) std::cout << "binomo api: start" << std::endl;
                    });
                    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                }
            });

            /* запускаем асинхронное открытие сделки */
            {
                std::lock_guard<std::mutex> lock(request_future_mutex);
                request_future.resize(request_future.size() + 1);
                request_future.back() = std::async(std::launch::async,
                        [&] {
                    while(!is_shutdown) {
                        if(is_connected) {
                            const uint64_t current_ref = ref_counter++;
                            json j;
                            j["topic"] = "base";
                            j["event"] = "ping";
                            j["payload"] = json::object();
                            j["ref"] = current_ref;
                            j["join_ref"] = join_ref;
                            send(j.dump());
                        }
                        std::this_thread::sleep_for(std::chrono::milliseconds(10000));
                    }
                });
            }
        }

    public:

        /** \brief Конструктор класса API
         * \param user_port Порт
         */
        BinomoApi(const uint32_t user_port) :
            api_port(user_port) {
        }

        ~BinomoApi() {
            is_shutdown = true;
            is_request_future_shutdown = true;
            {
                std::lock_guard<std::mutex> lock(server_mutex);
                if(server) server->stop();
            }
            {
                std::lock_guard<std::mutex> lock(request_future_mutex);
                for(size_t i = 0; i < request_future.size(); ++i) {
                    if(request_future[i].valid()) {
                        try {
                            request_future[i].wait();
                            request_future[i].get();
                        }
                        catch(const std::exception &e) {
                            std::cerr << "binomo api: error in ~BinomoApi(), what: " << e.what() << std::endl;
                        }
                        catch(...) {
                            std::cerr << "binomo api: error in ~BinomoApi()" << std::endl;
                        }
                    }
                }
            }
            if(server_future.valid()) {
                try {
                    server_future.wait();
                    server_future.get();
                }
                catch(const std::exception &e) {
                    std::cerr << "binomo api: error in ~BinomoApi(), what: " << e.what() << std::endl;
                }
                catch(...) {
                    std::cerr << "binomo api: error in ~BinomoApi()" << std::endl;
                }
            }
        }

        void start() {
            init_main_thread(api_port);
        }

        /** \brief Очистить массив сделок
         */
        void clear_bets_array() {
            {
                std::lock_guard<std::mutex> lock(bets_id_counter_mutex);
                std::lock_guard<std::mutex> lock2(array_bets_mutex);
                array_bets.clear();
                //bet_id_to_uuid.clear();
                bets_id_counter = 0;
            }
            //std::lock_guard<std::mutex> lock(broker_bet_id_to_uuid_mutex);
            //broker_bet_id_to_uuid.clear();
        }

        /** \brief Получтить ставку
         * \param bet Класс ставки, сюда будут загружены все параметры ставки
         * \param api_bet_id Уникальный номер ставки, который возвращает метод async_open_bo
         * \return Код ошибки или 0 в случае успеха
         */
        int get_bet(common::Bet &bet, const uint64_t api_bet_id) {
            /*
            std::lock_guard<std::mutex> lock(array_bets_mutex);
            auto &it_bet_id_to_uuid = bet_id_to_uuid.find(api_bet_id);
            if(it_bet_id_to_uuid == bet_id_to_uuid.end()) return DATA_NOT_AVAILABLE;
            std::string uuid = it_bet_id_to_uuid->second;
            auto &it_array_bets = array_bets.find(uuid);
            if(it_array_bets == array_bets.end()) return DATA_NOT_AVAILABLE;
            bet = it_array_bets->second;
            */
            return common::OK;

        }

        /** \brief Получить ID реального аккаунта
         * \return ID реального аккаунта
         */
       // inline uint64_t get_account_id() {
       //     return account_config.account_id;
       // }

        /** \brief Проверить, является ли аккаунт Demo
         * \return Вернет true если demo аккаунт
         */
       // inline bool demo_account() {
       //     return account_config.is_demo;
       // }

       // inline bool usd_account() {
       //     return account_config.is_usd;
       // }

      //  inline double get_min_amount(const bool is_usd) {
      //      if(!account_config.is_amount_init) return 0;
      //      if(is_usd) return account_config.min_usd_amount;
      //      return account_config.min_btc_amount;
      //  }

      //  inline double get_min_amount() {
     //       if(!account_config.is_amount_init) return 0;
      //      if(account_config.is_usd) return account_config.min_usd_amount;
      //      return account_config.min_btc_amount;
      //  }

        /** \brief Установить задержку между открытием сделок
         * \param delay Задержка между открытием сделок
         */
        inline void set_bets_delay(const double delay) {
            bets_delay = delay;
        }

        /** \brief Получить метку времени сервера
         *
         * Данный метод возвращает метку времени сервера. Часовая зона: UTC/GMT
         * \return метка времени сервера
         */
        inline xtime::ftimestamp_t get_server_timestamp() {
            return xtime::get_ftimestamp() + offset_timestamp;
        }

        /** \brief Проверить соединение
         * \return Вернет true, если соединение установлено
         */
        inline bool connected() {
            return is_connected;
        }

        /** \brief Подождать соединение
         *
         * Данный метод ждет, пока не установится соединение
         * \return вернет true, если соединение есть, иначе произошла ошибка
         */
        inline bool wait() {
            xtime::timestamp_t timestamp_start = 0;
            while(!is_error && !is_connected) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                if(is_open_connect) {
                    if(timestamp_start == 0) timestamp_start = xtime::get_timestamp();
                    if(((int64_t)xtime::get_timestamp() - (int64_t)timestamp_start) >
                        (int64_t)xtime::SECONDS_IN_MINUTE) {
                        std::lock_guard<std::mutex> lock(server_mutex);
                        if(server) server->stop();
                        timestamp_start = 0;
                        std::cerr << "binomo api: eror in wait() timeout exceeded" << std::endl;
                    }
                }
                if(is_shutdown) return false;
            }
            return is_connected;
        }

        /** \brief Получить баланс счета
         * \param is_demo Флаг демо аккаунта
         * \return Баланс аккаунта
         */
        inline double get_balance(const bool is_demo) {
            if(!is_connected) return 0.0;
            return is_demo ? account_config.demo_balance : account_config.real_balance;
        }

        /** \brief Получить баланс счета
         * \return Баланс аккаунта
         */
        inline double get_balance() {
            if(!is_connected) return 0.0;
            return account_config.is_demo ? account_config.demo_balance : account_config.real_balance;
        }

        inline std::string get_autchtoken() {
            std::lock_guard<std::mutex> lock(account_config_mutex);
            return account_config.autchtoken;
        }

        inline std::string get_device_id() {
            std::lock_guard<std::mutex> lock(account_config_mutex);
            return account_config.device_id;
        }

        /** \brief Получить процент выплаты
         * \param symbol_name Имя символа
         * \param duration Длительность экспирации
         * \param is_up Вверх ставка или вниз
         * \return Процент выплаты (от 0 до 1)
         */
         /*
        inline double get_payout(const std::string &symbol_name, const uint32_t duration, const bool is_up = true) {
            if(!is_connected) return 0.0;
            if(!is_init_payout_config) return 0.0;
            std::lock_guard<std::mutex> lock(payout_config_mutex);
            auto it_payout_config = payout_config.find(normalize_symbol_name(symbol_name));
            if(it_payout_config == payout_config.end()) return 0.0;
            auto it_payout = it_payout_config->second.payout.find(duration);
            if(it_payout == it_payout_config->second.payout.end()) return 0.0;
            if(is_up) return it_payout->second.first;
            return it_payout->second.second;
        }
        */

        /** \brief Открыть бинарный опцион
         *
         * Данный метод открывает бинарный опцион типа Спринт
         * \param symbol Символ
         * \param note Заметка сделки
         * \param amount Размер ставки
         * \param is_usd Флаг для переключения между ставкой USD и BTC
         * \param contract_type Тип контракта (BUY или SELL)
         * \param duration Длительность экспирации опциона
         * \param open_timestamp_offset Смещение времени открытия
         * \param is_demo_account Торговать демо аккаунт
         * \param api_bet_id Уникальный номер сделки внутри данной библиотеки
         * \param callback Функция для обратного вызова
         * \return Код ошибки
         */
         /*
        int open_bo(
                const std::string &symbol,
                const std::string &note,
                const double amount,
                const bool is_usd,
                const int contract_type,
                const uint32_t duration,
                uint64_t &api_bet_id,
                std::function<void(const common::Bet &bet)> callback = nullptr) {
            return async_open_bo(
                symbol,
                note,
                amount,
                is_usd,
                contract_type,
                duration,
                api_bet_id,
                callback);
        }
        */

        /** \brief Открыть бинарный опцион
         *
         * Данный метод открывает бинарный опцион типа Спринт
         * \param symbol Символ
         * \param note Заметка сделки
         * \param amount Размер ставки
         * \param contract_type Тип контракта (BUY или SELL)
         * \param duration Длительность экспирации опциона
         * \param open_timestamp_offset Смещение времени открытия
         * \param is_demo_account Торговать демо аккаунт
         * \param api_bet_id Уникальный номер сделки внутри данной библиотеки
         * \param callback Функция для обратного вызова
         * \return Код ошибки
         */
         /*
        int open_bo(
                const std::string &symbol,
                const std::string &note,
                const double amount,
                const int contract_type,
                const uint32_t duration,
                uint64_t &api_bet_id,
                std::function<void(const common::Bet &bet)> callback = nullptr) {
			bool is_usd = false;
			{
                std::lock_guard<std::mutex> lock(account_config_mutex);
                is_usd = account_config.is_usd;
                if(!account_config.check_amount(amount, is_usd)) return INVALID_AMOUNT;
			}
            return async_open_bo(
                symbol,
                note,
                amount,
                is_usd,
                contract_type,
                duration,
                api_bet_id,
                callback);
        }
        */

        /** \brief Получить метку времени закрытия CLASSIC бинарного опциона
        * \param timestamp Метка времени (в секундах)
        * \param expiration Экспирация (в минутах)
        * \return Вернет метку времени закрытия CLASSIC бинарного опциона либо 0, если ошибка.
        */
        xtime::timestamp_t get_classic_bo_closing_timestamp(
                const xtime::timestamp_t user_timestamp,
                const uint32_t user_expiration) {
            if (user_expiration < 1 ||
                (user_expiration > 5 && user_expiration % 15 != 0) ||
                user_expiration > 60) return 0;
            if(user_expiration <= 5) {
                const xtime::timestamp_t classic_bet_timestamp_future =
                    (xtime::timestamp_t)(user_timestamp + (user_expiration * 60 + 30));
                return (classic_bet_timestamp_future - classic_bet_timestamp_future % (60));
            }
            if(user_expiration > 5) {
                const xtime::timestamp_t classic_bet_timestamp_future =
                    (xtime::timestamp_t)(user_timestamp + (user_expiration + 5) * 60);
                return (classic_bet_timestamp_future - classic_bet_timestamp_future % (5 * 60));
            }
            return 0;
        }

        /** \brief Открыть бинарный опцион
         *
         * Данный метод открывает бинарный опцион типа Спринт
         * \param symbol Символ
         * \param note Заметка сделки
         * \param amount Размер ставки
         * \param contract_type Тип контракта (BUY или SELL)
         * \param duration Длительность экспирации опциона (секунды)
         * \param open_timestamp_offset Смещение времени открытия
         * \param is_demo_account Торговать демо аккаунт
         * \param callback Функция для обратного вызова
         * \return Код ошибки
         */
        int open_bo(
                const std::string &symbol,
                const double amount,
                const int contract_type,
                const uint32_t duration,
                const bool is_demo,
                std::function<void(const common::Bet &bet)> callback = nullptr) {
            uint64_t api_bet_id = 0;
            std::string note;
            const double timestamp = get_server_timestamp();
            return async_open_bo(
                symbol,
                note,
                amount,
                is_demo,
                contract_type,
                timestamp,
                get_classic_bo_closing_timestamp(timestamp, duration / xtime::SECONDS_IN_MINUTE),
                api_bet_id,
                callback);
        }
    };
}

#endif // PRIME_XBT_API_HPP_INCLUDED
