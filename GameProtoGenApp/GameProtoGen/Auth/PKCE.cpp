#include "Pkce.h"
#include <random>
#include <array>
#include <picosha2.h>
#include <cppcodec/base64_url_unpadded.hpp>

static std::string random_verifier(size_t len = 64) {
    static const char chars[] =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-._~";
    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_int_distribution<size_t> dist(0, sizeof(chars) - 2);
    std::string out; out.reserve(len);
    for (size_t i = 0; i < len; i++) out.push_back(chars[dist(rng)]);
    return out;
}

PkcePair GeneratePkcePair() {
    PkcePair p{};
    p.verifier = random_verifier(64);
    std::array<unsigned char, picosha2::k_digest_size> hash{};
    picosha2::hash256(p.verifier.begin(), p.verifier.end(), hash.begin(), hash.end());
    p.challenge = cppcodec::base64_url_unpadded::encode(hash.data(), hash.size());
    return p;
}
