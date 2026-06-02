// omg moonhook v4
#define MOONHOOK_VERSION "v4.0.0"

#include <iostream>
#include <functional>
#include <string>
#include <vector>

#include <ansi_terminal.hpp>
#include <consolerandom.hpp>
#include <console.hpp>
#include <option.hpp>
#include <internal_options.hpp>

int main()
{
    ansi::enableANSI();
    if (!ansi::supportsColor())
    {
        std::cout << "Your terminal does not support ANSI, please use a different terminal emulator to get better output.\n";
    }

    while (true) 
    {
        ansi::clearConsole();

        const std::string& MoonhookBanner = ConsoleRandom::GetBanner();
        const ansi::Gradient& MainGradient = ConsoleRandom::GetGradient();
        ConsoleHelper console(MainGradient, MoonhookBanner);

        console.printbanner();
        
        std::cout << "----------------------------------------------------------------\n";
        std::cout << "Moonhook " << std::string(MOONHOOK_VERSION) << "!" << std::endl;
        std::cout << ConsoleRandom::GetText() << std::endl;
        std::cout << "----------------------------------------------------------------\n\n";

        std::vector<Option> main_menu_options = {
            InternalOptions::Webhooks,
            InternalOptions::Bots
        };

        for (size_t i = 0; i < main_menu_options.size(); i++)
        {
            Option* o = &main_menu_options[i];
            std::cout << "  " << std::to_string(i + 1) << ". " << o->name << std::endl;
        }
        
        int exit_option_num = main_menu_options.size() + 1;
        std::cout << "  " << std::to_string(exit_option_num) << ". Exit\n";

        std::cout << ansi::rgb_to_ansi(255, 255, 0) << "Select an option(1-" << exit_option_num << "): " << ansi::ColorReset();
        
        std::string selection;
        std::getline(std::cin, selection);
        
        int idx = 0;
        try {
            idx = std::stoi(selection) - 1;
            
            if (idx == exit_option_num - 1)
            {
                std::cout << "Exiting...\n";
                return 0; 
            }

            if (idx < 0 || idx >= (int)main_menu_options.size())
            {
                console.error("Invalid selection!");
                ansi::pause();
                continue; 
            }
        }
        catch (...)
        {
            console.error("Invalid selection!");
            ansi::pause();
            continue; 
        }

        Option selected = main_menu_options[idx];
        try {
            ConsoleHelper* c = &console;
            selected.run(c);
        }
        catch (const RestartException&)
        {
           
        }
        catch (...)
        {
            std::cout << ansi::rgb_to_ansi(255, 0, 0) << "This option had errored!\n" << ansi::ColorReset();
            ansi::pause();
        }
    }

    return 0;
}