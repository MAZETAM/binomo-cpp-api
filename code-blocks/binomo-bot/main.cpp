#include <iostream>
#include <fstream>
#include <winsock2.h>
#include <windows.h>
#include <conio.h>

#include "bot\binomo-bot.hpp"

#define PROGRAM_VERSION "1.3"
#define PROGRAM_DATE "12.12.2020"

int main(int argc, char **argv) {
    std::cout << "binomo bot " << PROGRAM_VERSION << " ";
    std::cout << PROGRAM_DATE << std::endl << std::endl;

    binomo_bot::Settings settings(argc, argv);
    if(settings.is_error) {
        std::cout << "binomo bot: settings error!" << std::endl;
        std::system("pause");
        return EXIT_FAILURE;
    }

    std::cout << "binomo bot: initialization start" << std::endl << std::endl;

    binomo_bot::BinomoBot bot;

    /* инициализируем основные настройки */
    if(!bot.init_main(settings)) {
        std::cout << "binomo bot: BinomoBot--->init_main() error" << std::endl;
        std::system("pause");
        return EXIT_FAILURE;
    }

    if(!bot.init_candles_stream_mt4(settings)) {
        std::cout << "binomo bot: BinomoBot--->init_candles_stream_mt4() error" << std::endl;
        std::system("pause");
        return EXIT_FAILURE;
    }

    if(!bot.init_pipe_server(settings)) {
        std::cout << "binomo bot: BinomoBot--->init_pipe_server() error" << std::endl;
        std::system("pause");
        return EXIT_FAILURE;
    }

    std::cout << "binomo bot: initialization complete" << std::endl << std::endl;
    std::cout << "binomo bot: press space bar to exit the program" << std::endl << std::endl;

    //std::system("pause");
    /* обрабатываем нажатие клавиши для выхода из программы */
    while(true) {
        bool is_exit = false;
        if(kbhit()) {
            int c = getch();
            switch(c) {
                case VK_SPACE:
                is_exit = true;
                std::cout << "exit" << std::endl;
                break;
                default:
                if(settings.hotkeys.is_use) {
                    for(size_t i = 0; i < settings.hotkeys.hotkey.size(); ++i) {
                        std::string keys = settings.hotkeys.hotkey[i].key;
                        for(size_t k = 0; k < keys.size(); ++k) {
                            if(keys[k] == c) {
                                bot.open_bo(
                                    settings.hotkeys.hotkey[i].symbol,
                                    settings.hotkeys.hotkey[i].amount,
                                    settings.hotkeys.hotkey[i].direction,
                                    settings.hotkeys.hotkey[i].duration,
                                    settings);

                                binomo_api::common::PrintThread{}
                                    << "binomo bot: press hot key: " << c
                                    << std::endl;

                                binomo_api::common::PrintThread{}
                                    << settings.hotkeys.hotkey[i].symbol
                                    << ", amount = "
                                    << settings.hotkeys.hotkey[i].amount
                                    << ", duration = "
                                    << settings.hotkeys.hotkey[i].duration
                                    << ", direction = "
                                    << settings.hotkeys.hotkey[i].direction
                                    << std::endl;
                                break;
                            }
                        }
                    }
                } else {
                    std::cout << "binomo bot: press key: " << c << std::endl;
                }
                break;
            }
        } // if
        if(is_exit) break;
        const int DELAY = 100;
        bot.update_ping(DELAY);
        bot.update_connection();
        std::this_thread::sleep_for(std::chrono::milliseconds(DELAY));
    }
    return 0;
}
