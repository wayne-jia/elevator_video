/*
 * Copyright (C) 2018 Microchip Technology Inc.  All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <chrono>
#include <cxxopts.hpp>
#include <egt/detail/string.h>
#include <egt/ui>
#include <fstream>
#include <iostream>
#include <sstream>

#define PRJ_PATH "./"
#define SCR_RES_W         768
#define SCR_RES_H         1024
#define SINGLE_NUM_X      353
#define SINGLE_NUM_MID_X  (SINGLE_NUM_X - 51)

typedef enum
{
    APP_STATE_INIT=0,
    APP_STATE_DOWN_TO_10,
    APP_STATE_DOWN_TO_1,
    APP_STATE_UP_TO_30,   
    APP_STATE_UP_TO_68,
    APP_STATE_DOOR_OPEN,
    APP_STATE_DOOR_CLOSE,
    APP_STATE_RUNNING,
    APP_STATE_OVERLOAD,
} APP_STATES;

static APP_STATES appState = APP_STATE_INIT;
static uint32_t prev_sec_tick = 0, sec_tick = 0;
static uint32_t sec2 = 0;
static bool door_open = false;
static bool overloaded = false;
static uint32_t current_floor = 13;


static const std::string digi_str[] = {
    "file:0.png",
    "file:1.png",
    "file:2.png",
    "file:3.png",
    "file:4.png",
    "file:5.png",
    "file:6.png",
    "file:7.png",
    "file:8.png",
    "file:9.png",
};

// warning: not UTF-8 safe
static std::string line_break(const std::string& in, size_t width = 50)
{
    std::string out;
    std::string tmp;
    char last = '\0';
    size_t i = 0;

    for (auto& cur : in)
    {
        if (++i == width)
        {
            tmp = egt::detail::ltrim(tmp);
            out += "\n" + tmp;
            i = tmp.length();
            tmp.clear();
        }
        else if (isspace(cur) && !isspace(last))
        {
            out += tmp;
            tmp.clear();
        }
        tmp += cur;
        last = cur;
    }
    return out + tmp;
}

template<class T>
static inline T ns2ms(T n)
{
    return n / 1000000UL;
}

static bool is_target_sama5d4()
{
    std::ifstream infile("/proc/device-tree/model");
    if (infile.is_open())
    {
        std::string line;
        while (getline(infile, line))
        {
            if (line.find("SAMA5D4") != std::string::npos)
            {
                infile.close();
                return true;
            }
        }
        infile.close();
    }
    return false;
}



int main(int argc, char** argv)
{
    std::vector<std::string> ad_lines;
    egt::AnimationSequence ad_sequence{true};

    cxxopts::Options options(argv[0], "play video file");
    options.add_options()
    ("h,help", "Show help")
    ("i,input", "URI to video file. If there is a '&' in the URI, escape it with \\ or surround the URI with double quotes.", cxxopts::value<std::string>())
    ("width", "Width of the stream", cxxopts::value<int>()->default_value("320"))
    ("height", "Height of the stream", cxxopts::value<int>()->default_value("192"))
    ("f,format", "Pixel format", cxxopts::value<std::string>()->default_value(is_target_sama5d4() ? "xrgb8888" : "yuv420"), "[egt::PixelFormat]");
    auto args = options.parse(argc, argv);

    if (args.count("help") ||
        !args.count("input"))
    {
        std::cout << options.help() << std::endl;
        return 0;
    }

    egt::Size size(args["width"].as<int>(), args["height"].as<int>());
    auto format = egt::detail::enum_from_string<egt::PixelFormat>(args["format"].as<std::string>());
    auto input(args["input"].as<std::string>());

    egt::Application app(argc, argv);

    egt::add_search_path(PRJ_PATH);

    egt::TopWindow win;
    win.background(egt::Image("file:bg.png"));

    auto ad_box = std::make_shared<egt::Frame>();
    // ad_box->fill_flags(egt::Theme::FillFlag::blend);
    // ad_box->color(egt::Palette::ColorId::label_bg, egt::Color(0x00008096));
    ad_box->resize(egt::Size(680, 95));
    ad_box->move(egt::Point(30, 900));
    win.add(ad_box);

    egt::ImageLabel logo(egt::Image("file:logo.png"));
    logo.move(egt::Point(28, 32));
    win.add(logo);

    auto arrow = std::make_shared<egt::ImageLabel>(egt::Image("file:down.png"));
    arrow->move(egt::Point(73, 116));
    win.add(arrow);

    auto tendigi = std::make_shared<egt::ImageLabel>(egt::Image("file:1.png"));
    tendigi->move(egt::Point(243, 80));
    win.add(tendigi);

    auto sigledigi = std::make_shared<egt::ImageLabel>(egt::Image("file:3.png"));
    sigledigi->move(egt::Point(SINGLE_NUM_X, 80));
    win.add(sigledigi);

    auto week = std::make_shared<egt::Label>("星期三");
    week->font(egt::Font("Noto Sans CJK SC", 30));
    week->move(egt::Point(570, 84));
    win.add(week);

    auto date = std::make_shared<egt::Label>("2025-01-01");
    date->font(egt::Font(25));
    date->move(egt::Point(552, 155));
    win.add(date);

    auto overload = std::make_shared<egt::Label>("超载");
    overload->font(egt::Font("Noto Sans CJK SC", 24));
    overload->move(egt::Point(590, 216));
    //overload->hide();
    win.add(overload);

    egt::Label errlabel;
    errlabel.color(egt::Palette::ColorId::label_text, egt::Palette::white);
    errlabel.align(egt::AlignFlag::expand);
    errlabel.text_align(egt::AlignFlag::center_horizontal | egt::AlignFlag::top);
    win.add(errlabel);

    // player after label to handle drag
    egt::VideoWindow player(size, format, egt::WindowHint::overlay);
    //player.move_to_center(win.center());
    player.move(egt::Point(15, 281));
    player.volume(5);
    win.add(player);


    egt::Label cpulabel("CPU: 0%");
    cpulabel.color(egt::Palette::ColorId::label_text, egt::Palette::white);
    cpulabel.margin(25);
    bottom(left(cpulabel));
    win.add(cpulabel);

    egt::experimental::CPUMonitorUsage tools;
    egt::PeriodicTimer cputimer(std::chrono::seconds(1));
    cputimer.on_timeout([&cpulabel, &tools]()
    {
        tools.update();
        std::ostringstream ss;
        ss << "CPU: " << static_cast<int>(tools.usage()) << "%";
        cpulabel.text(ss.str());
    });
    cputimer.start();

    // wait to start playing the video until the window is shown
    win.on_show([&player, input]()
    {
        player.media(input);

        player.play();
        player.loopback(true);
    });

    player.on_error([&errlabel](const std::string & err)
    {
        errlabel.text(line_break(err));
    });

    auto show_floor_num = [&tendigi, &sigledigi](){
        uint8_t ten = current_floor/10;
        uint8_t single = current_floor%10;

        if (current_floor < 10)
        {
            tendigi->hide();
            sigledigi->x(SINGLE_NUM_MID_X);
            sigledigi->uri(digi_str[single]);
        }
        else 
        {
            tendigi->show();
            tendigi->uri(digi_str[ten]);
            sigledigi->x(SINGLE_NUM_X);
            sigledigi->uri(digi_str[single]);
        }
    };

    auto down_floor = [&show_floor_num](){
        if (current_floor > 0)
            current_floor--;
        show_floor_num();
    };

    auto up_floor = [&show_floor_num](){
        if (current_floor < 68)
            current_floor++;
        show_floor_num();
    };

    ///=============Handle ad scrolling  text================
    auto scrolling_ad = [&ad_lines, &ad_box, &ad_sequence](){
        std::string line;

        std::ifstream inf(PRJ_PATH"taglines.txt", std::ios::binary);
        if (!inf.is_open())
        {
            std::cerr << "taglines.txt open failed!" << std::endl;
            return;
        }

        while (std::getline(inf, line))
        {
            if (!line.empty())
                ad_lines.push_back(line);

            //std::cout << line << std::endl;
        }

        if (!ad_lines.empty())
        {

            auto label = std::make_shared<egt::Label>();
            label->color(egt::Palette::ColorId::label_text, egt::Palette::white);
            label->font(egt::Font("Noto Sans CJK SC", 26));
            ad_box->add(label);
#ifdef LABEL_BG_COLOR
            label->fill_flags(egt::Theme::FillFlag::blend);
            //label->border(3);
            label->border_radius(10);
            label->color(egt::Palette::ColorId::label_text, egt::Palette::white);
            label->color(egt::Palette::ColorId::label_bg, egt::Color(0x00008096));
            label->color(egt::Palette::ColorId::border, egt::Color(0x000080ff));
#endif

            auto minx = 0 - ad_box->width();
            auto maxx = SCR_RES_W;
            auto half = (SCR_RES_W - ad_box->width()) / 2;

            auto in = std::make_shared<egt::PropertyAnimator>(maxx, half,
                                                                std::chrono::seconds(3),
                                                                egt::easing_exponential_easeout);
            in->on_change([&ad_box](int value)
            {
                ad_box->x(value);
            });

            auto delay1 = std::make_shared<egt::AnimationDelay>(std::chrono::seconds(2));

            auto out = std::make_shared<egt::PropertyAnimator>(half + 1, minx,
                                                                std::chrono::seconds(3),
                                                                egt::easing_exponential_easeout);
            out->reverse(true);
            out->on_change([&ad_box, &ad_lines, out, label](int value)
            {
                ad_box->x(value);

                static size_t index = 0;
                if (egt::detail::float_equal(value, out->ending()))
                {
                    label->text(ad_lines[index]);
                    if (++index >= ad_lines.size())
                        index = 0;
                }
            });

            auto delay2 = std::make_shared<egt::AnimationDelay>(std::chrono::seconds(2));

            ad_sequence.add(in);
            ad_sequence.add(delay1);
            ad_sequence.add(out);
            ad_sequence.add(delay2);
            ad_sequence.start();
        }
    };

    // One second periodic timer
    auto sec_timer = std::make_shared<egt::PeriodicTimer>(std::chrono::milliseconds(1000));
    sec_timer->on_timeout([](){ sec_tick++; });

    
    ///============ Main timer for state machine =============
    egt::PeriodicTimer main_timer(std::chrono::milliseconds(50));
    main_timer.on_timeout([&]() 
    {
        switch (appState)
        {
            case APP_STATE_INIT:
            {
                sec_tick = 0;
                sec_timer->start();
                scrolling_ad();
                appState = APP_STATE_DOOR_OPEN; 
                break;
            }
            case APP_STATE_DOWN_TO_10:
            {
                if (sec_tick != prev_sec_tick)
                {            
                    prev_sec_tick = sec_tick; 
                    down_floor();
                    if (10 == current_floor)
                    {
                        appState = APP_STATE_DOOR_OPEN; 
                    }
                }
                  
                break;
            }
            case APP_STATE_DOWN_TO_1:
            {
                if (sec_tick != prev_sec_tick)
                {             
                    prev_sec_tick = sec_tick; 
                    down_floor();
                    if (1 == current_floor)
                    {
                        appState = APP_STATE_DOOR_OPEN; 
                    }
                }

                break;
            }  
            case APP_STATE_UP_TO_30:
            {
                if (sec_tick != prev_sec_tick)
                {             
                    prev_sec_tick = sec_tick; 
                    up_floor();
                    if (30 == current_floor)
                    {
                        appState = APP_STATE_DOOR_OPEN; 
                    }
                }

                break;
            }
            case APP_STATE_UP_TO_68:
            {
                if (sec_tick != prev_sec_tick)
                {             
                    prev_sec_tick = sec_tick; 
                    up_floor();
                    if (68 == current_floor)
                    {
                        appState = APP_STATE_DOOR_OPEN; 
                    }
                }

                break;
            }
            case APP_STATE_DOOR_OPEN:
            {
                if (!door_open)
                {
                    overload->text("开门");
                    door_open = true;
                }

                if (sec_tick != prev_sec_tick)
                {   
                    sec2++;            
                    prev_sec_tick = sec_tick; 

                    if (sec2 > 1)
                    {
                        sec2 = 0;
                        appState = APP_STATE_OVERLOAD; 
                    }
                }

                break;
            }
            case APP_STATE_DOOR_CLOSE:
            {
                if (door_open)
                {
                    overload->text("关门");
                    door_open = false;
                    overloaded = false;
                }

                if (sec_tick != prev_sec_tick)
                {   
                    sec2++;            
                    prev_sec_tick = sec_tick; 

                    if (sec2 > 1)
                    {
                        sec2 = 0;
                        appState = APP_STATE_RUNNING; 
                    }
                }

                break;
            }
            case APP_STATE_RUNNING:
            {
                switch (current_floor) 
                {
                    case 13:
                    case 68:
                        arrow->uri("file:down.png");
                        appState = APP_STATE_DOWN_TO_10;
                        break;
                    case 10:
                        appState = APP_STATE_DOWN_TO_1;
                        break;
                    case 1:
                        arrow->uri("file:up.png");
                        appState = APP_STATE_UP_TO_30;
                        break;
                    case 30:
                        appState = APP_STATE_UP_TO_68;
                        break;
                    default:
                        break;
                }
                
                break;
            }
            case APP_STATE_OVERLOAD:
            {
                if (!overloaded)
                {
                    overload->text("超载");
                    overloaded = true;
                }

                if (sec_tick != prev_sec_tick)
                {   
                    sec2++;            
                    prev_sec_tick = sec_tick; 

                    if (sec2 > 1)
                    {
                        sec2 = 0;
                        appState = APP_STATE_DOOR_CLOSE; 
                    }
                }

                break;
            }
            default:
                break;
        }
    });
    main_timer.start();
    ///============ Main timer for state machine end =============

    win.show();
    //win.layout();
    player.show();

    return app.run();
}
