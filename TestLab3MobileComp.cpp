#include <chrono>
#include <thread>
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <pigpio.h>
#include <cpprest/http_msg.h>

using namespace std::chrono_literals;

#include "metronome.hpp"
#include "rest.hpp"

#define LED_RED   17
#define LED_GREEN 27
#define BTN_MODE  23
#define BTN_TAP   24

metronome my_metronome;
size_t prev_bpm = 0, max_bpm = 0, min_bpm = INT_MAX;
size_t current_bpm = 0;

void metronome::start_timing()
{
    m_beat_count = 0;
    m_timing = true;
    my_metronome.startTiming();
}

void metronome::stop_timing()
{
    m_timing = false;
    my_metronome.startTiming();
    size_t curr_bpm = 0;
    curr_bpm = my_metronome.get_bpm();

    printf("CURRENT BPM:- %d\n", curr_bpm);

    if (curr_bpm != 0 && curr_bpm < min_bpm)
    {
        min_bpm = curr_bpm;
    }
    if (curr_bpm != 0 && curr_bpm > max_bpm)
    {
        max_bpm = curr_bpm;
    }
    if (curr_bpm != 0)
    {
        current_bpm = curr_bpm;
    }
}

void metronome::tap()
{
    size_t time_current = gpioTick();
    if (m_beat_count >= 4)
    {
        for (int i = 0; i < m_beat_count - 1; i++)
        {
            m_beats[i] = m_beats[i + 1];
        }
        m_beats[3] = time_current;
    }
    else
        m_beats[m_beat_count++] = time_current;

    printf("time  %d\n", time_current);
}

size_t metronome::get_bpm() const
{
    int i = 0;
    size_t avg_bpm = 0;
    size_t sum_bpm = 0;
    if (m_beat_count < 4)
    {
        return prev_bpm;
    }
    else
    {
        for (i = 0; i < m_beat_count - 1; i++)
        {
            sum_bpm = sum_bpm + m_beats[i + 1] - m_beats[i];
        }

        avg_bpm = (size_t)sum_bpm / (m_beat_count - 1);

        prev_bpm = avg_bpm;
        return 60000 / prev_bpm;
    }
}

void on_mode()
{
    if (my_metronome.is_timing() == true)
    {
        gpioWrite(LED_RED, LOW);
        my_metronome.stop_timing();
    }
    else
    {
        gpioWrite(LED_GREEN, LOW);
        my_metronome.start_timing();
    }
}

void on_tap()
{
    if (my_metronome.is_timing() == true)
    {
        my_metronome.tap();
    }
}

volatile bool btn_mode_pressed = false;
volatile bool btn_tap_pressed = false;

volatile bool tap_button_state = false;
volatile bool tap_last_state = false;

void run_on_tap()
{
    while (true)
    {
        if (my_metronome.is_timing() == true)
        {
            tap_button_state = gpioRead(BTN_TAP);
            if (tap_button_state != tap_last_state)
            {
                std::this_thread::sleep_for(20ms);
                if (tap_button_state == HIGH)
                {
                    gpioWrite(LED_RED, HIGH);
                    on_tap();
                }
                else
                {
                    gpioWrite(LED_RED, LOW);
                }
            }
            tap_last_state = tap_button_state;
        }
        else
        {
            // play mode logic
        }
    }
}

volatile bool mode_last_state = false;
volatile bool mode_button_state = false;

void blink()
{
    bool on = false;
    while (true)
    {
        std::this_thread::sleep_for(1s);

        if (btn_mode_pressed)
            on = !on;
        else
            on = false;

        gpioWrite(LED_RED, (on) ? HIGH : LOW);
    }
}

void get42(web::http::http_request msg)
{
    web::http::http_response response(200);
    response.headers().add("Access-Control-Allow-Origin", "*");
    response.set_body(web::json::value::number(current_bpm));
    msg.reply(response);
}

void put_restbpm(web::http::http_request msg)
{
    size_t input_bpm;
    std::string input_str = msg.content_ready().get().extract_json(true).get().serialize();
    input_bpm = std::stoi(input_str);

    current_bpm = input_bpm;

    if (current_bpm != 0 && current_bpm < min_bpm)
    {
        min_bpm = current_bpm;
    }
    if (current_bpm != 0 && current_bpm > max_bpm)
    {
        max_bpm = current_bpm;
    }

    web::http::http_response response(200);
    response.headers().add("Access-Control-Allow-Origin", "*");
    response.set_body(web::json::value::string("test"));
    msg.reply(response);
}

void get_min(web::http::http_request msg)
{
    web::http::http_response response(200);
    response.headers().add("Access-Control-Allow-Origin", "*");
    response.set_body(web::json::value::number(min_bpm));
    msg.reply(response);
}

void delete_min(web::http::http_request msg)
{
    min_bpm = INT_MAX;
    web::http::http_response response(200);
    response.headers().add("Access-Control-Allow-Origin", "*");
    response.set_body(web::json::value::string("delete successfully!"));
    msg.reply(response);
}

void get_max(web::http::http_request msg)
{
    web::http::http_response response(200);
    response.headers().add("Access-Control-Allow-Origin", "*");
    response.set_body(web::json::value::number(max_bpm));
    msg.reply(response);
}

void delete_max(web::http::http_request msg)
{
    max_bpm = 0;
    web::http::http_response response(200);
    response.headers().add("Access-Control-Allow-Origin", "*");
    response.set_body(web::json::value::string("delete successfully!"));
    msg.reply(response);
}

int main()
{
    gpioInitialise();

    gpioSetMode(LED_RED, PI_OUTPUT);
    gpioSetMode(BTN_MODE, PI_INPUT);
    gpioSetMode(LED_GREEN, PI_OUTPUT);
    gpioSetMode(BTN_TAP, PI_INPUT);

    auto get42_rest = rest::make_endpoint("/answer");
    get42_rest.support(web::http::methods::GET, get42);

    auto bpm_rest = rest::make_endpoint("/bpm");