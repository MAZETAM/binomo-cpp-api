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
#ifndef BINOMO_BOT_HPP_INCLUDED
#define BINOMO_BOT_HPP_INCLUDED

#include "binomo-bot-settings.hpp"
#include "..\binomo-cpp-api.hpp"
#include "..\binomo-cpp-api-http.hpp"
#include "..\binomo-cpp-api-websocket.hpp"
#include "named-pipe-server.hpp"
#include "..\tools\binomo-cpp-api-mql-hst.hpp"

namespace binomo_bot {
    using json = nlohmann::json;

    class BinomoBot {
    private:
        std::shared_ptr<binomo_api::BinomoApi> api;
		std::mutex api_mutex;

        std::shared_ptr<binomo_api::BinomoApiPriceStream<>> candlestick_streams;/**< Поток котировок */
		std::mutex candlestick_streams_mutex;

        std::shared_ptr<binomo_api::BinomoApiHttp<>> binomo_http_api;
		std::mutex binomo_http_api_mutex;

        std::shared_ptr<SimpleNamedPipe::NamedPipeServer> pipe_server;
		std::mutex pipe_server_mutex;

        std::vector<std::shared_ptr<binomo_api::MqlHst<>>> mql_history;
		std::mutex mql_history_mutex;

		std::atomic<int> update_ping_tick = ATOMIC_VAR_INIT(0);

        std::atomic<bool> is_pipe_server = ATOMIC_VAR_INIT(false);

        std::atomic<bool> is_last_connected = ATOMIC_VAR_INIT(false);

        template <typename T>
        struct atomwrapper {
            std::atomic<T> _a;

            atomwrapper()
            :_a() {}

            atomwrapper(const std::atomic<T> &a)
            :_a(a.load()) {}

            atomwrapper(const atomwrapper &other)
            :_a(other._a.load()) {}

            atomwrapper &operator=(const atomwrapper &other) {
                _a.store(other._a.load());
                return _a.load();
            }

            atomwrapper &operator=(const bool other) {
                _a.store(other);
                //return this;
            }

            bool operator==(const bool other) {
                return (_a.load() == other);
            }

            bool operator!=(const bool other) {
                return (!(_a.load() == other));
            }
        };

        std::vector<atomwrapper<bool>> is_init_mql_history;
        std::vector<atomwrapper<bool>> is_once_mql_history;

        std::mutex request_future_mutex;
        std::vector<std::future<void>> request_future;
        std::atomic<bool> is_future_shutdown = ATOMIC_VAR_INIT(false);

        std::atomic<bool> is_error = ATOMIC_VAR_INIT(false);

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
                    std::cerr <<"binomo bot: error in BinanceApi::clear_request_future(), what: " << e.what() << std::endl;
                }
                catch(...) {
                    std::cerr <<"binomo bot: error in BinanceApi::clear_request_future()" << std::endl;
                }
                ++index;
            }
        }

    public:

        /** \brief Инициализация главных компонент API
         * \param settings Настройки API
         * \return Вернет true в случае успеха
         */
        bool init_main(Settings &settings) {
            {
                std::lock_guard<std::mutex> lock(binomo_http_api_mutex);
                binomo_http_api = std::make_shared<binomo_api::BinomoApiHttp<>>(
                        settings.binomo.sert_file,
                        settings.binomo.cookie_file);
            }
            {
                std::lock_guard<std::mutex> lock(api_mutex);
                api = std::make_shared<binomo_api::BinomoApi>(
                        settings.binomo.port);
                api->start();
            }
            return true;
        }

        bool init_candles_stream_mt4(Settings &settings) {
            if (!settings.quotes_stream.is_use) return true;
            {
				std::lock_guard<std::mutex> lock(binomo_http_api_mutex);
				if(!binomo_http_api) return false;
			}

            if(is_error) return false;

			{
				std::lock_guard<std::mutex> lock(candlestick_streams_mutex);
				candlestick_streams = std::make_shared<binomo_api::BinomoApiPriceStream<>>(settings.binomo.sert_file);
				candlestick_streams->set_volume_mode(settings.quotes_stream.volume_mode);
			}

            /* проверяем параметры символов */
            for(size_t i = 0; i < settings.quotes_stream.symbols.size(); ++i) {
				std::string s = binomo_api::common::normalize_symbol_name(settings.quotes_stream.symbols[i].first);
				auto it = binomo_api::common::normalize_name_to_ric.find(s);
				if(it == binomo_api::common::normalize_name_to_ric.end()) {
					std::cerr << "binomo bot: symbol " << settings.quotes_stream.symbols[i].first << " does not exist!" << std::endl;
				}
				switch(settings.quotes_stream.symbols[i].second) {
				case 1:
				case 5:
				case 15:
				case 30:
                    std::cerr << "binomo bot: period " << settings.quotes_stream.symbols[i].second << " does not exist!" << std::endl;
					return false;
                    break;
				case xtime::SECONDS_IN_MINUTE:
				case (5 * xtime::SECONDS_IN_MINUTE):
				case (15 * xtime::SECONDS_IN_MINUTE):
				case (30 * xtime::SECONDS_IN_MINUTE):
				case xtime::SECONDS_IN_HOUR:
				case (3*xtime::SECONDS_IN_HOUR):
				case xtime::SECONDS_IN_DAY:
					continue;
				default:
					std::cerr << "binomo bot: period " << settings.quotes_stream.symbols[i].second << " does not exist!" << std::endl;
					return false;
					break;
				}
            }

            /* узнаем точность символов */
            std::vector<uint32_t> precisions;
            for(size_t i = 0; i < settings.quotes_stream.symbols.size(); ++i) {
                auto it = binomo_api::common::normalize_name_to_precision.find(settings.quotes_stream.symbols[i].first);
				if(it == binomo_api::common::normalize_name_to_precision.end()) {
					std::cerr << "binomo bot: precision " << settings.quotes_stream.symbols[i].second << " does not exist!" << std::endl;
					return false;
				}
				precisions.push_back(it->second);
				if(precisions.back() <= settings.quotes_stream.max_precisions) {
                    binomo_api::common::PrintThread{}
                        << "binomo bot: symbol " << settings.quotes_stream.symbols[i].first
                        << " period " << settings.quotes_stream.symbols[i].second
                        << " precision " << precisions.back()
                        << std::endl;
                } else {
                    binomo_api::common::PrintThread{}
                        << "binomo bot: symbol " << settings.quotes_stream.symbols[i].first
                        << " period " << settings.quotes_stream.symbols[i].second
                        << " precision " << precisions.back()
                        << " fix precision " << settings.quotes_stream.max_precisions
                        << std::endl;
                }
            }

            /* инициализируем исторические данные MQL */
            for(size_t i = 0; i < settings.quotes_stream.symbols.size(); ++i) {
                std::string mql_symbol_name = settings.quotes_stream.symbols[i].first + settings.quotes_stream.symbol_hst_suffix;
                if(mql_symbol_name.size() >= 11) mql_symbol_name = mql_symbol_name.substr(0,11);
                mql_history.push_back(std::make_shared<binomo_api::MqlHst<>>(
                    mql_symbol_name,
                    settings.quotes_stream.path,
                    settings.quotes_stream.symbols[i].second/xtime::SECONDS_IN_MINUTE,
                    std::min(precisions[i], settings.quotes_stream.max_precisions),
                    settings.quotes_stream.timezone));
            }

            is_init_mql_history.resize(settings.quotes_stream.symbols.size());
            is_once_mql_history.resize(settings.quotes_stream.symbols.size());
            for(size_t i = 0; i < settings.quotes_stream.symbols.size(); ++i) {
                is_init_mql_history.push_back(std::atomic<bool>(false));
                is_once_mql_history.push_back(std::atomic<bool>(false));
                //is_init_mql_history[i] = false;
                //is_once_mql_history[i] = false;
            }

            /* инициализируем callback функцию потока котировок */
            candlestick_streams->on_candle = [&](
                    const std::string &symbol,
                    const binomo_api::common::Candle &candle,
                    const uint32_t period,
                    const bool close_candle) {

                //std::cout << "--symbol00 " << symbol << std::endl;

                /* получаем размер массива исторических даных MQL */
                size_t mql_history_size = 0;
                {
                    std::lock_guard<std::mutex> lock(mql_history_mutex);
                    mql_history_size = mql_history.size();
                }

                if(mql_history_size != 0) {
                    for(size_t i = 0; i < mql_history_size; ++i) {
                        /* проверяем соответствие параметров символа */
                        if (settings.quotes_stream.symbols[i].first != symbol ||
                            settings.quotes_stream.symbols[i].second != period) continue;

                        /* проверяем наличие инициализации исторических данных */
                        if(is_init_mql_history[i] == false) continue;

                        /* получаем последнюю метку времени исторических данных */
                        xtime::timestamp_t last_timestamp = 0;
                        {
                            std::lock_guard<std::mutex> lock(mql_history_mutex);
                            last_timestamp = mql_history[i]->get_last_timestamp();
                        }

                        if(last_timestamp == 0) continue;

                        /* проверяем, была ли только что загрузка исторических данных */
                        if(is_once_mql_history[i] == false) {
                            ///std::cout << "TIME once last_timestamp " << xtime::get_str_date_time(last_timestamp) << std::endl;
                            ///std::cout << "TIME once candle.timestamp " << xtime::get_str_date_time(candle.timestamp) << " close_candle " << close_candle << std::endl;
                            /* проверяем, не успел ли прийти новый бар,
                             * пока мы загружали исторические данных
                             */
                            if(candle.timestamp > last_timestamp) {
                                /* добавляем пропущенные исторические данные, не включая текущий бар */
                                const xtime::timestamp_t step_time = period;// * xtime::SECONDS_IN_MINUTE;
                                //std::cout << "step_time " << step_time << std::endl;
                                for(xtime::timestamp_t t = last_timestamp; t < candle.timestamp; t += step_time) {
                                    binomo_api::common::Candle streams_old_candle = candlestick_streams->get_timestamp_candle(symbol, period, t);
                                    std::lock_guard<std::mutex> lock(mql_history_mutex);
                                    mql_history[i]->add_new_candle_with_memory(streams_old_candle);
                                    ///std::cout << "TIME once " << xtime::get_str_date_time(t) << std::endl;
                                }
                            }
                            {
                                std::lock_guard<std::mutex> lock(mql_history_mutex);
                                mql_history[i]->update_candle_with_memory(candle);
                                if(close_candle) {
                                    mql_history[i]->add_new_candle_with_memory(candle);
                                }
                            }
                            is_once_mql_history[i] = true;
                            continue;
                        }

                        /* если параметры соответствуют, обновляем исторические данные */
                        if(close_candle) {
                            std::lock_guard<std::mutex> lock(mql_history_mutex);
                            mql_history[i]->add_new_candle_with_memory(candle);
                        } else {
                            std::lock_guard<std::mutex> lock(mql_history_mutex);
                            mql_history[i]->update_candle_with_memory(candle);
                        }
                    }
                }

#               if(0)
                /* выводим сообщение о символе */
                std::cout
                    << symbol
                    //<< " o: " << candle.open
                    << " c: " << candle.close
                    //<< " h: " << candle.high
                    //<< " l: " << candle.low
                    << " v: " << candle.volume
                    << " p: " << period
                    << " t: " << xtime::get_str_time(candle.timestamp)
                    << " cc: " << close_candle
                    << std::endl;
#               endif
            };

            /* инициализируем потоки котировок */
            candlestick_streams->add_candles_stream(settings.quotes_stream.symbols);
            candlestick_streams->start();
            candlestick_streams->wait();

            /* ждем, чтобы котировки прогрузились */
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));

            /* загружаем исторические данные */
            for(size_t i = 0; i < settings.quotes_stream.symbols.size(); ++i) {
                const xtime::timestamp_t stop_date = xtime::get_first_timestamp_minute();
                const xtime::timestamp_t start_date = stop_date - (settings.quotes_stream.symbols[i].second * settings.quotes_stream.candles);
                std::vector<binomo_api::common::Candle> candles;

                int err = binomo_http_api->get_historical_data(
                    candles,
                    settings.quotes_stream.symbols[i].first,
                    settings.quotes_stream.symbols[i].second,
                    start_date,
                    stop_date);

                for(size_t c = 0; c < candles.size(); ++c) {
                    binomo_api::common::Candle candle = candles[c];
                    /* добавляем все новые бары, за исключением последнего,
                     * так как он может быть изменен за время загрузки истории
                     */
                    if(c < (candles.size() - 1)) mql_history[i]->add_new_candle_with_memory(candle);
                    else mql_history[i]->update_candle_with_memory(candle); // добавили последний бар
                }

                /* ставим флаг инициализации исторических данных */
                is_init_mql_history[i] = true;

                std::string mql_symbol_name = settings.quotes_stream.symbols[i].first + settings.quotes_stream.symbol_hst_suffix;
                if(mql_symbol_name.size() >= 11) mql_symbol_name = mql_symbol_name.substr(0,11);

                binomo_api::common::PrintThread{}
                    << "binomo bot: "
                    << settings.quotes_stream.symbols[i].first
                    << " initialized as " << mql_symbol_name
                    << ", candles = " << candles.size()
                    << ", error code = " << err << std::endl;
            }
            return true;
        }

        bool init_pipe_server(Settings &settings) {
            {
				std::lock_guard<std::mutex> lock(api_mutex);
				if(!api) return false;
			}
            if(is_error) return false;

            const size_t buffer_size = 1024*10;

			std::lock_guard<std::mutex> lock(pipe_server_mutex);
			pipe_server = std::make_shared<SimpleNamedPipe::NamedPipeServer>(
				settings.bot.named_pipe,
				buffer_size);


            pipe_server->on_open = [&](SimpleNamedPipe::NamedPipeServer::Connection* connection) {
                {
                    std::lock_guard<std::mutex> lock(api_mutex);
                    if(api) {
                        is_last_connected = api->connected();
                        if(is_last_connected) {
                            std::lock_guard<std::mutex> lock(pipe_server_mutex);
                            pipe_server->send_all("{\"connection\":1}");
                        } else {
                            std::lock_guard<std::mutex> lock(pipe_server_mutex);
                            pipe_server->send_all("{\"connection\":0}");
                        }
                    } else {
                        std::lock_guard<std::mutex> lock(pipe_server_mutex);
                        pipe_server->send_all("{\"connection\":0}");
                    }
                }
                binomo_api::common::PrintThread{} << "binomo bot: named pipe open, handle = " << connection->get_handle() << std::endl;
            };

            pipe_server->on_message = [&,settings](SimpleNamedPipe::NamedPipeServer::Connection* connection, const std::string &in_message) {
                /* обрабатываем входящие сообщения */

                //std::cout << "message " << in_message << ", handle: " << connection->get_handle() << std::endl;

                /* парисм */
                try {
                    json j = json::parse(in_message);

                    if(j.find("pong") != j.end()) {
                        binomo_api::common::PrintThread{} << "binomo bot: MT4 send pong" << std::endl;
                    } else
                    if(j.find("ping") != j.end()) {
                        binomo_api::common::PrintThread{} << "binomo bot: MT4 send ping" << std::endl;
                    } else
                    if(j.find("contract") != j.end()) {
                        /* пришел запрос на открытие сделки */

                        json j_contract = j["contract"];

                        /* параметры опциона */
                        std::string symbol;
                        std::string note;
                        double amount = 0;
                        int contract_type = binomo_api::common::INVALID_CONTRACT_TYPE;
                        uint32_t duration = 0;
                        xtime::timestamp_t date_expiry = 0;
                        uint64_t api_bet_id = 0;

                        /* получаем все параметры опциона */
                        if(j_contract.find("s") != j_contract.end()) {
                            symbol = j_contract["s"];
                            symbol.erase(std::remove(symbol.begin(),symbol.end(), '/'), symbol.end());
                            symbol.erase(std::remove(symbol.begin(),symbol.end(), '\\'), symbol.end());
                            if(symbol.size() > 6) symbol = symbol.substr(0,6);
                        }
                        if(j_contract.find("note") != j_contract.end()) {
                            note = j_contract["note"];
                        }
                        if(j_contract.find("a") != j_contract.end()) {
                            amount = j_contract["a"];
                        }
                        if(j_contract.find("dir") != j_contract.end()) {
                            if(j_contract["dir"] == "BUY") contract_type = binomo_api::common::BUY;
                            else if(j_contract["dir"] == "SELL") contract_type = binomo_api::common::SELL;
                        }
                        if(j_contract.find("exp") != j_contract.end()) {
                            if(j_contract["exp"].is_number()) {
                                /* проверяем инициализацию api и блокируем умную ссылку */
                                std::lock_guard<std::mutex> lock(api_mutex);
                                if(!api) {
                                    const xtime::timestamp_t timestamp_server = api->get_server_timestamp();
                                    xtime::timestamp_t temp_date_expiry = j_contract["exp"];
                                    if(temp_date_expiry < xtime::SECONDS_IN_DAY) {
                                        /* пользователь задал время экспирации */
                                        date_expiry = api->get_classic_bo_closing_timestamp(
                                            timestamp_server,
                                            (temp_date_expiry / xtime::SECONDS_IN_MINUTE));
                                    } else {
                                        /* пользователь задал дату экспирации */
                                        date_expiry = temp_date_expiry;
                                    }
                                }
                                /* */
                            }
                        } else
                        if(j_contract.find("dur") != j_contract.end()) {
                            if(j_contract["dur"].is_number()) {
                                duration = j_contract["dur"];
                            }
                        }

                        /* фильтр времени торговли */
                        if(settings.time_filter.periods.size() != 0 && settings.time_filter.is_use) {

                            /* проверяем инициализацию api и блокируем умную ссылку */
                            std::lock_guard<std::mutex> lock(api_mutex);
                            if(!api) return;
                            /* */

                            const uint32_t second_day = xtime::get_second_day(api->get_server_timestamp());
                            bool is_found_time = false;
                            for(size_t i = 0; i < settings.time_filter.periods.size(); ++i) {
                                const uint32_t start_second_day = settings.time_filter.periods[i].first;
                                const uint32_t stop_second_day = settings.time_filter.periods[i].second;

                                if (start_second_day <= stop_second_day &&
                                    second_day >= start_second_day &&
                                    second_day <= stop_second_day) {
                                    is_found_time = true;
                                } else
                                if (start_second_day > stop_second_day &&
                                    (second_day >= start_second_day ||
                                    second_day <= stop_second_day)) {
                                    is_found_time = true;
                                }
                            }
                            if(!is_found_time) {
                                binomo_api::common::PrintThread{}
                                    << "binomo bot: skip bet " << symbol
                                    << ", time filter triggered, time = " << xtime::get_str_time_ms(api->get_server_timestamp())
                                    << std::endl;
                                return;
                            }
                        }

                        if (symbol.size() > 0 && amount > 0 && duration > 0 &&
                            (contract_type == binomo_api::common::BUY ||
                            contract_type == binomo_api::common::SELL)) {

                            /* проверяем инициализацию api и блокируем умную ссылку */
                            std::lock_guard<std::mutex> lock(api_mutex);
                            if(!api) return;
                            /* */
                            api->open_bo(
                                symbol,
                                amount,
                                contract_type,
                                duration,
                                settings.binomo.is_demo_account,
                                [&](const binomo_api::common::Bet &bet){
                                switch(bet.bet_status) {
                                    case binomo_api::common::BetStatus::UNKNOWN_STATE:
                                        //std::cout << "UNKNOWN_STATE" << std::endl;
                                    break;
                                    case binomo_api::common::BetStatus::CHECK_ERROR:
                                        //std::cout << "CHECK_ERROR" << std::endl;
                                        binomo_api::common::PrintThread{}
                                            << "binomo bot: bo-bet check error (server response), symbol = "
                                            << symbol << std::endl;
                                    break;
                                    case binomo_api::common::BetStatus::OPENING_ERROR:
                                        //std::cout << "CHECK_ERROR" << std::endl;
                                        binomo_api::common::PrintThread{}
                                            << "binomo bot: bo-bet opennig error (server response), symbol = "
                                            << symbol << std::endl;
                                    break;
                                    case binomo_api::common::BetStatus::STANDOFF:
                                        //std::cout << "STANDOFF" << std::endl;
                                    break;
                                    case binomo_api::common::BetStatus::WIN:
                                        //std::cout << "WIN" << std::endl;
                                        binomo_api::common::PrintThread{} << "binomo bot: " << bet.symbol_name << " win, id = " << bet.broker_bet_id << std::endl;
                                    break;
                                    case binomo_api::common::BetStatus::LOSS:
                                        //std::cout << "LOSS" << std::endl;
                                        binomo_api::common::PrintThread{} << "binomo bot: " << bet.symbol_name << " loss, id = " << bet.broker_bet_id << std::endl;
                                    break;
                                    case binomo_api::common::BetStatus::WAITING_COMPLETION:
                                        //std::cout << "WAITING_COMPLETION" << std::endl;
                                        binomo_api::common::PrintThread{}
                                                    << "binomo bot: bo-bet, symbol = "
                                                    << bet.symbol_name
                                                    << ", id = " << bet.broker_bet_id
                                                    << ", open time " << xtime::get_str_date_time(bet.opening_timestamp)
                                                    << std::endl;
                                    break;
                                };
                            });
                        } // if (symbol.size() > 0 && amount > 0 && duration > 0 &&
                          // (contract_type == intrade_bar_common::BUY ||
                          // contract_type == intrade_bar_common::SELL))
                    } // if(j.find("contract") != j.end())
                } // try
                catch(...) {
                    binomo_api::common::PrintThread{} << "binomo bot: named pipe error, json::parse" << std::endl;
                }
            };

            pipe_server->on_close = [&](SimpleNamedPipe::NamedPipeServer::Connection* connection) {
                binomo_api::common::PrintThread{} << "binomo bot: named pipe close, handle = " << connection->get_handle() << std::endl;
            };

            pipe_server->on_error = [&](SimpleNamedPipe::NamedPipeServer::Connection* connection, const std::error_code &ec) {
                binomo_api::common::PrintThread{} << "binomo bot: named pipe error, handle = " << connection->get_handle() << ", what " << ec.value() << std::endl;
            };

            /* запускаем сервер */
            pipe_server->start();
            is_pipe_server = true;
            binomo_api::common::PrintThread{} << "binomo bot: start named pipe server" << std::endl;
            return true;
        }

        /** \brief Обновить пинг
         * \param delay Задержка между вызовом метода
         */
        void update_ping(const int delay) {
            const int MAX_TICK = 30000;
            update_ping_tick += delay;
            if(update_ping_tick > MAX_TICK) {
                update_ping_tick = 0;
                std::lock_guard<std::mutex> lock(pipe_server_mutex);
                if(pipe_server) {
                    pipe_server->send_all("{\"ping\":1}");
                    binomo_api::common::PrintThread{} << "binomo bot: send ping" << std::endl;
                }
            }
        }

        /** \brief Обновить состояние соединения
         */
        void update_connection() {
            {
                std::lock_guard<std::mutex> lock(api_mutex);
                if(!api) return;
                if(is_last_connected == api->connected()) return;
                is_last_connected = api->connected();
            }
            if(is_last_connected) {
                std::lock_guard<std::mutex> lock(pipe_server_mutex);
                if(pipe_server) pipe_server->send_all("{\"connection\":1}");
            } else {
                std::lock_guard<std::mutex> lock(pipe_server_mutex);
                if(pipe_server) pipe_server->send_all("{\"connection\":0}");
            }
            binomo_api::common::PrintThread{} << "binomo bot: connection: " << is_last_connected << std::endl;
        }

        bool open_bo(
                const std::string &symbol,
                const double amount,
                const int contract_type,
                const uint32_t duration,
                Settings &settings) {
            std::lock_guard<std::mutex> lock(api_mutex);
            if(api) {
                api->open_bo(
                    symbol,
                    amount,
                    contract_type,
                    duration,
                    settings.binomo.is_demo_account,
                    [&](const binomo_api::common::Bet &bet){
                    switch(bet.bet_status) {
                        case binomo_api::common::BetStatus::UNKNOWN_STATE:
                            //std::cout << "UNKNOWN_STATE" << std::endl;
                        break;
                        case binomo_api::common::BetStatus::CHECK_ERROR:
                            //std::cout << "CHECK_ERROR" << std::endl;
                            binomo_api::common::PrintThread{}
                                << "binomo bot: bo-bet check error (server response), symbol = "
                                << symbol << std::endl;
                        break;
                        case binomo_api::common::BetStatus::OPENING_ERROR:
                            //std::cout << "CHECK_ERROR" << std::endl;
                            binomo_api::common::PrintThread{}
                                << "binomo bot: bo-bet opennig error (server response), symbol = "
                                << symbol << std::endl;
                        break;
                        case binomo_api::common::BetStatus::STANDOFF:
                            //std::cout << "STANDOFF" << std::endl;
                        break;
                        case binomo_api::common::BetStatus::WIN:
                            //std::cout << "WIN" << std::endl;
                            binomo_api::common::PrintThread{} << "binomo bot: " << bet.symbol_name << " win, id = " << bet.broker_bet_id << std::endl;
                        break;
                        case binomo_api::common::BetStatus::LOSS:
                            //std::cout << "LOSS" << std::endl;
                            binomo_api::common::PrintThread{} << "binomo bot: " << bet.symbol_name << " loss, id = " << bet.broker_bet_id << std::endl;
                        break;
                        case binomo_api::common::BetStatus::WAITING_COMPLETION:
                            //std::cout << "WAITING_COMPLETION" << std::endl;
                            binomo_api::common::PrintThread{}
                                        << "binomo bot: bo-bet, symbol = "
                                        << bet.symbol_name
                                        << ", id = " << bet.broker_bet_id
                                        << ", open time " << xtime::get_str_date_time(bet.opening_timestamp)
                                        << std::endl;
                        break;
                    };
                });
                return true;
            }
            return false;
        }

#if(0)
        /** \brief Получить метку времени сервера
         * \return Метка времени сервера
         */
        inline xtime::ftimestamp_t get_server_ftimestamp() {
            if(cand) return binance_http_fapi->get_server_ftimestamp();
            return xtime::get_ftimestamp();
        }
#endif

        BinomoBot() {

        };

        ~BinomoBot() {
            if(is_error) return;
            is_future_shutdown = true;

            /* закрываем все потоки */
            {
                std::lock_guard<std::mutex> lock(request_future_mutex);
                for(size_t i = 0; i < request_future.size(); ++i) {
                    if(request_future[i].valid()) {
                        try {
                            request_future[i].wait();
                            request_future[i].get();
                        }
                        catch(const std::exception &e) {
                            std::cerr <<"binomo bot: error in ~BinomoBot(), waht: request_future, exception: " << e.what() << std::endl;
                        }
                        catch(...) {
                            std::cerr <<"binomo bot: error in ~BinomoBot(), waht: request_future" << std::endl;
                        }
                    } // if
                } // for i
            }
        } //
    };

}

#endif // BINOMO_BOT_HPP_INCLUDED
