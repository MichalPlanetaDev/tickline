#include "tickline/security/sha256.hpp"

#include <cstddef>
#include <exception>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

void expect(
    const bool condition,
    const std::string_view message)
{
    if (!condition) {
        throw std::runtime_error{
            std::string{message}};
    }
}

[[nodiscard]] tickline::security::Sha256Digest
digest_text(const std::string_view text)
{
    return tickline::security::sha256(
        std::as_bytes(
            std::span{
                text.data(),
                text.size(),
            }));
}

void test_empty_message()
{
    expect(
        digest_text("").to_hex() ==
            "e3b0c44298fc1c149afbf4c8996fb924"
            "27ae41e4649b934ca495991b7852b855",
        "empty SHA-256 vector should match");
}

void test_abc_message()
{
    expect(
        digest_text("abc").to_hex() ==
            "ba7816bf8f01cfea414140de5dae2223"
            "b00361a396177a9cb410ff61f20015ad",
        "abc SHA-256 vector should match");
}

void test_multiblock_message()
{
    constexpr std::string_view message{
        "abcdbcdecdefdefgefghfghighij"
        "hijkijkljklmklmnlmnomnopnopq",
    };

    expect(
        digest_text(message).to_hex() ==
            "248d6a61d20638b8e5c026930c3e6039"
            "a33ce45964ff2167f6ecedd419db06c1",
        "multiblock SHA-256 vector should match");
}

void test_digest_is_deterministic()
{
    const auto first =
        digest_text("tickline-evidence");

    const auto second =
        digest_text("tickline-evidence");

    const auto different =
        digest_text("tickline-evidencf");

    expect(
        first == second,
        "identical inputs should produce identical digests");

    expect(
        first != different,
        "different inputs should produce different test digests");
}

}

int main()
{
    try {
        test_empty_message();
        test_abc_message();
        test_multiblock_message();
        test_digest_is_deterministic();
    } catch (const std::exception& error) {
        std::cerr
            << "SHA-256 test failure: "
            << error.what()
            << '\n';

        return 1;
    }

    return 0;
}
