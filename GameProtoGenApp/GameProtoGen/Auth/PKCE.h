#pragma once
#include <string>

// PKCE = Proof Key for Code Exchange

struct PkcePair {
    std::string verifier;   // high-entropy random string (43..128)
    std::string challenge;  // base64url(SHA256(verifier))
};

PkcePair GeneratePkcePair();
