// SPDX-FileCopyrightText: 2022 Enrique M.G. <quiqueiii@gmail.com>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "http.hpp"

#include <json.hpp>

#include <curl/curl.h>

#include <exception>

#include <iostream>

using namespace lliurex::http;

using namespace edupals;
using namespace edupals::variant;

using namespace std;

size_t response_cb(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    stringstream* in=static_cast<stringstream*>(userdata);

    for (size_t n=0;n<nmemb;n++) {
        in->put(ptr[n]);
    }

    return nmemb;
}

struct Curl
{
    CURL* curl;
    struct curl_slist* headers;
};

static Curl prepare_request(string url, Response& response)
{
    Curl handler {0};

    curl_global_init(CURL_GLOBAL_ALL);

    handler.curl = curl_easy_init();

    if (!handler.curl) {
        throw runtime_error("Failed to initialize curl");
    }

    handler.headers = curl_slist_append(handler.headers, "User-Agent: GVA-GATE");
    curl_easy_setopt(handler.curl, CURLOPT_HTTPHEADER, handler.headers);
    curl_easy_setopt(handler.curl, CURLOPT_URL, url.c_str());

    curl_easy_setopt(handler.curl, CURLOPT_TIMEOUT, 2L);

    curl_easy_setopt(handler.curl, CURLOPT_WRITEDATA,&response.content);
    curl_easy_setopt(handler.curl, CURLOPT_WRITEFUNCTION,response_cb);

    return handler;
}

static void free_request(Curl handler)
{
    curl_easy_cleanup(handler.curl);
    curl_slist_free_all(handler.headers);
    curl_global_cleanup();
}

Variant Response::parse()
{
    Variant ret;

    try {
        ret = json::load(content);
    }
    catch (std::exception& e) {
        throw runtime_error(e.what());
    }

    return ret;
}

Client::Client(string url) : server(url)
{
    
}

Response Client::get(string what)
{
    Response response;

    Curl handler = prepare_request(server+"/"+what,response);

    CURLcode status = curl_easy_perform(handler.curl);

    if (status != 0) {
        throw runtime_error("Client::get::curl_easy_perform("+std::to_string(status)+")");
    }

    curl_easy_getinfo(handler.curl, CURLINFO_RESPONSE_CODE, &response.status);

    free_request(handler);

    return response;
}

Response Client::post(string what,map<string,string> fields)
{
    Response response;

    Curl handler = prepare_request(server+"/"+what,response);

    stringstream postfields;

    auto it = fields.begin();
    bool more = false;

    while (it!=fields.end()) {
        if (more) {
            postfields<<"&";
        }
        postfields<<it->first<<"="<<it->second;
        it++;
        more=true;
    }

    string pdata = postfields.str();

    curl_easy_setopt(handler.curl, CURLOPT_POSTFIELDS, pdata.c_str());

    CURLcode status = curl_easy_perform(handler.curl);

    if (status != 0) {
        throw runtime_error("Client::post::curl_easy_perform("+std::to_string(status)+")");
    }

    curl_easy_getinfo(handler.curl, CURLINFO_RESPONSE_CODE, &response.status);

    free_request(handler);

    return response;
}
