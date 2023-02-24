// SPDX-FileCopyrightText: 2022 Enrique M.G. <quiqueiii@gmail.com>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "hash.hpp"

#include <cstdint>
#include <openssl/evp.h>

using namespace std;

vector<uint8_t> lliurex::hash::sha1(string input)
{
    const EVP_MD* md;
    EVP_MD_CTX* mdctx;

    unsigned char md_value[EVP_MAX_MD_SIZE];
    unsigned int md_len;

    md = EVP_sha1();
    mdctx = EVP_MD_CTX_create();

    EVP_DigestInit_ex(mdctx, md, nullptr);
    EVP_DigestUpdate(mdctx, input.c_str(), input.size());

    EVP_DigestFinal_ex(mdctx,md_value,&md_len);
    EVP_MD_CTX_destroy(mdctx);

    EVP_cleanup();

    vector<uint8_t> ret(md_value,md_value + md_len);

    return ret;
}

ostream& lliurex::hash::operator<<(ostream& os,vector<uint8_t> value)
{
    std::ios_base::fmtflags state = os.flags();
    os<<std::hex;
    for (uint8_t v:value) {
        os<<(int)v;
    }

    os.flags(state);

    return os;
}
