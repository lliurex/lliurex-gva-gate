// SPDX-FileCopyrightText: 2022 Enrique M.G. <quiqueiii@gmail.com>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "hash.hpp"

#include <openssl/evp.h>

using namespace std;

string lliurex::hash::sha1(string input)
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

    string ret((char*)md_value);

    return ret;
}
