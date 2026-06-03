#pragma once

#include <functional>
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <csignal>
#include <future>

#include <console.hpp>
#include <option.hpp>
#include <ansi_terminal.hpp>

#include <webhook.hpp>

namespace InternalOptions {
    inline Option Webhooks = {
        "Webhooks",
        0,
        [](ConsoleHelper* console) -> void {
            std::string WEBHOOK_URL = console->input("Webhook URL: ");

            console->log("Initializing webhook...");
            moon::Webhook hook(WEBHOOK_URL);

            const std::string WEBHOOK_NAME = hook.get_name();
            if (WEBHOOK_NAME.empty())
            {
                console->error("This webhook appears to be non-existent or broken. get_name() error: " + hook.last_error());
                ansi::pause();
                return;
            }

            const std::string WebhooksBanner = R"(
██╗    ██╗███████╗██████╗ ██╗  ██╗ ██████╗  ██████╗ ██╗  ██╗███████╗
██║    ██║██╔════╝██╔══██╗██║  ██║██╔═══██╗██╔═══██╗██║ ██╔╝██╔════╝
██║ █╗ ██║█████╗  ██████╔╝███████║██║   ██║██║   ██║█████╔╝ ███████╗
██║███╗██║██╔══╝  ██╔══██╗██╔══██║██║   ██║██║   ██║██╔═██╗ ╚════██║
╚███╔███╔╝███████╗██████╔╝██║  ██║╚██████╔╝╚██████╔╝██║  ██╗███████║
 ╚══╝╚══╝ ╚══════╝╚═════╝ ╚═╝  ╚═╝ ╚═════╝  ╚═════╝ ╚═╝  ╚═╝╚══════╝
        MoonHook V4
            )";

//---------------------------------- SUB OPTIONS -------------------------------------------------//
            std::vector<Option> sub_options = {
                {
                    "Send Message",
                    1,
                    [&hook](ConsoleHelper* c) -> void {
                        std::string TEXT = c->input("Text to send: ");
                        bool success = hook.send(TEXT);
                        if (success)
                        {
                            c->log("Sent successfully!");
                        } else {
                            c->error("Failed to send webhook! Error: " + hook.last_error());
                        }
                    }
                },
                {
                    "Spam Webhook",
                    1,
                    [&hook](ConsoleHelper* c) -> void {
                        std::string TEXT = c->input("Text to spam: ");
                        int DELAY = c->int_input("Delay (seconds): ");
                        if (DELAY <= 0) DELAY = 1;

                        static std::atomic<bool> g_spamming{true};
                        g_spamming = true;

                        std::signal(SIGINT, [](int) {
                            g_spamming = false;
                        });

                        c->log("Spamming... Press CTRL+C to stop.");

                        std::thread t([&]() {
                            while (g_spamming)
                            {
                                bool resp = hook.send(TEXT);
                                if (resp)
                                {
                                    c->log("Sent successfully!");
                                } else {
                                    c->error("Failed to send! Error: " + hook.last_error());
                                }
                                std::this_thread::sleep_for(std::chrono::seconds(DELAY));
                            }
                            c->log("Stopped spamming.");
                        });

                        t.join();
                    }
                },
                {
                    "Set Webhook Name",
                    1,
                    [&hook](ConsoleHelper* c) -> void {
                        std::string NAME = c->input("New webhook name: ");
                        c->log("Attempting to set webhook name...");
                        bool success = hook.set_name(NAME);
                        if (success)
                        {
                            c->log("Success!");
                        } else {
                            c->error("Failed to set webhook name! Error: " + hook.last_error());
                        }
                    }
                },
                {
                    "Set Webhook Avatar",
                    1,
                    [&hook](ConsoleHelper* c) -> void {
                        int file_or_url = c->int_input("Set from file or URL? (1-file, 2-url): ");
                        std::string source = c->input("Source (path or URL): ");

                        c->log("Attempting to set avatar...");

                        bool success = false;
                        if (file_or_url == 1)
                        {
                            success = hook.set_avatar_from_file(source);
                        } else if (file_or_url == 2)
                        {
                            success = hook.set_avatar_from_url(source);
                        } else {
                            c->error("Invalid option! Expected 1 for file or 2 for URL.");
                            return;
                        }

                        if (success)
                        {
                            c->log("Success!");
                        } else {
                            c->error("Failed! Error: " + hook.last_error());
                        }
                    }
                },
                {
                    "Delete Webhook",
                    1,
                    [&hook, &WEBHOOK_NAME](ConsoleHelper* c) -> void {
                        std::string confirmation = c->input("Are you sure you want to delete \"" + WEBHOOK_NAME + "\"? (Y/n): ");
                        if (confirmation == "Y" || confirmation == "y")
                        {
                            c->log("Attempting to delete...");
                            bool result = hook.delete_webhook();
                            if (result)
                            {
                                c->log("Deleted! Returning to main menu...");
                                ansi::pause();
                                ansi::clearConsole();
                                throw RestartException();
                            } else {
                                c->error("Failed! Error: " + hook.last_error());
                            }
                        } else {
                            c->log("Cancelled.");
                        }
                    }
                }
            };
//------------------------------------------------------------------------------------------------//

            while (true)
            {
                ansi::clearConsole();
                ansi::print_gradient_ascii(WebhooksBanner, console->gmain);
                std::cout << "Webhook: " << WEBHOOK_NAME << "\n\n";

                for (size_t i = 0; i < sub_options.size(); i++)
                {
                    std::cout << "  " << (i + 1) << ". " << sub_options[i].name << "\n";
                }
                std::cout << "  " << (sub_options.size() + 1) << ". Back\n\n";

                std::cout << ansi::rgb_to_ansi(255, 255, 0)
                          << "Select an option (1-" << (sub_options.size() + 1) << "): "
                          << ansi::ColorReset();

                std::string selection;
                std::getline(std::cin, selection);

                int idx = -1;
                try {
                    idx = std::stoi(selection) - 1;
                } catch (...) {}

                if (idx < 0 || idx > (int)sub_options.size())
                {
                    console->error("Invalid selection!");
                    ansi::pause();
                    continue;
                }

                if (idx == (int)sub_options.size())
                {
                    return;
                }

                try {
                    sub_options[idx].run(console);
                } catch (const RestartException&) {
                    throw; 
                } catch (...) {
                    console->error("Option encountered an error!");
                }

                ansi::pause();
            }
        }
    };
}
