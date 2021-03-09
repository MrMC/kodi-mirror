//
// Created by Francesco Laurita on 6/8/16.
//

#ifndef SPEEDTEST_DATATYPES_H
#define SPEEDTEST_DATATYPES_H
#include <iostream>
#include <stdio.h>
#include <stdlib.h>

#define SPEED_TEST_USER_AGENT "Mozilla/5.0 Darwin-20.3.0; U; x86_64; en-us (KHTML, like Gecko) SpeedTest++/1.14"
#define SPEED_TEST_SERVER_LIST_URL "https://www.speedtest.net/speedtest-servers.php"

#define SPEED_TEST_IP_INFO_API_URL "http://speedtest.ookla.com/api/ipaddress.php"
#define SPEED_TEST_API_URL "http://www.speedtest.net/api/api.php"
#define SPEED_TEST_API_REFERER "http://c.speedtest.net/flash/speedtest.swf"
#define SPEED_TEST_API_KEY "297aae72"
#define SPEED_TEST_MIN_SERVER_VERSION 2.3
#define SPEED_TEST_LATENCY_SAMPLE_SIZE 80

static const float EARTH_RADIUS_KM = 6371.0;

typedef struct ip_info_t {
    std::string ip_address;
    std::string isp;
    float lat;
    float lon;
} IPInfo;

typedef struct server_info_t {
    std::string url;
    std::string name;
    std::string country;
    std::string country_code;
    std::string host;
    std::string sponsor;
    int   id;
    float lat;
    float lon;
    float distance;

} ServerInfo;

typedef struct test_config_t {
    long start_size;
    long max_size;
    long incr_size;
    long buff_size;
    long min_test_time_ms;
    int  concurrency;
    std::string label;
} TestConfig;

const TestConfig broadbandConfigDownloadNew = {
        10000000,   // start_size
        120000000, // max_size
        10000000,   // inc_size
        65536,     // buff_size
        20000,     // min_test_time_ms
        2,        // concurrency
        "Broadband line type detected: profile selected broadband"

};

const TestConfig broadbandConfigUploadNew = {
        10000000,  // start_size
        120000000, // max_size
        10000000,   // inc_size
        65536,    // buff_size
        20000,    // min_test_time_ms
        2,        // concurrency
        "Broadband line type detected: profile selected broadband"
};

const TestConfig preflightConfigDownload = {
         600000, // start_size
        2000000, // max_size
         125000, // inc_size
           4096, // buff_size
          10000, // min_test_time_ms
              2, // Concurrency
        "Preflight check"
};

const TestConfig slowConfigDownload = {
         100000, // start_size
         500000, // max_size
          10000, // inc_size
           1024, // buff_size
          20000, // min_test_time_ms
              2, // Concurrency
         "Very-slow-line line type detected: profile selected slowband"
};

const TestConfig slowConfigUpload = {
          50000, // start_size
          80000, // max_size
           1000, // inc_size
           1024, // buff_size
          20000, // min_test_time_ms
              2, // Concurrency
          "Very-slow-line line type detected: profile selected slowband"
};


const TestConfig narrowConfigDownload = {
          1000000, // start_size
        100000000, // max_size
           750000, // inc_size
            4096, // buff_size
            20000, // min_test_time_ms
                2, // Concurrency
          "Buffering-lover line type detected: profile selected narrowband"
};

const TestConfig narrowConfigUpload = {
        1000000, // start_size
        100000000, // max_size
        550000, // inc_size
        4096, // buff_size
        20000, // min_test_time_ms
        2, // Concurrency
        "Buffering-lover line type detected: profile selected narrowband"
};

const TestConfig broadbandConfigDownload = {
        1000000,   // start_size
        100000000, // max_size
        750000,    // inc_size
        65536,     // buff_size
        20000,     // min_test_time_ms
        32,        // concurrency
        "Broadband line type detected: profile selected broadband"

};

const TestConfig broadbandConfigUpload = {
        1000000,  // start_size
        70000000, // max_size
        250000,   // inc_size
        65536,    // buff_size
        20000,    // min_test_time_ms
        8,        // concurrency
        "Broadband line type detected: profile selected broadband"
};

const TestConfig fiberConfigDownload = {
        5000000,   // start_size
        120000000, // max_size
        950000,    // inc_size
        65536,     // buff_size
        20000,     // min_test_time_ms
        32,        // concurrency
        "Fiber / Lan line type detected: profile selected fiber"
};

const TestConfig fiberConfigUpload = {
        1000000,  // start_size
        70000000, // max_size
        250000,   // inc_size
        65536,    // buff_size
        20000,    // min_test_time_ms
        12,       // concurrency
        "Fiber / Lan line type detected: profile selected fiber"
};

#endif //SPEEDTEST_DATATYPES_H
