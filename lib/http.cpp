// SPDX-FileCopyrightText: 2022 Enrique M.G. <quiqueiii@gmail.com>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "http.hpp"

#include <json.hpp>

#include <curl/curl.h>

#include <exception>
#include <sstream>
#include <iostream>

using namespace lliurex;

using namespace edupals;
using namespace edupals::variant;

using namespace std;

HttpClient::HttpClient(string url) : server(url)
{
    
}

size_t response_cb(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    stringstream* in=static_cast<stringstream*>(userdata);
    
    for (size_t n=0;n<nmemb;n++) {
        in->put(ptr[n]);
    }
    
    return nmemb;
}

Variant HttpClient::request(string what)
{
    Variant ret;
    stringstream input;
    
    CURL* curl = nullptr;
    struct curl_slist* headers = nullptr;
    CURLcode status;
    
    curl_global_init(CURL_GLOBAL_ALL);
    
    curl = curl_easy_init();
    
    if (!curl) {
        throw runtime_error("Failed to initialize curl");
    }
    
    string address = server+"/"+what;
    
    headers = curl_slist_append(headers, "User-Agent: NSS-HTTP");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_URL, address.c_str());
    
    /* two seconds timeout */
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 2L);
    
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,&input);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,response_cb);
    
    status = curl_easy_perform(curl);

    if (status != 0) {
        throw runtime_error("curl_easy_perform("+std::to_string(status)+")");
    }
    
    // free curl resources
    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);
    curl_global_cleanup();
    
    try {
        ret = json::load(input);
    }
    catch (std::exception& e) {
        throw runtime_error(e.what());
    }
    
    return ret;
}
