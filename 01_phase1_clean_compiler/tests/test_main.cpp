// test_main.cpp
// Verifies the C++ code generated from auth.tl and variables.tl actually
// behaves the way the TinyLang source describes. Including the generated
// .cpp files directly (rather than linking) keeps this harness self-
// contained for a small student project.

#include <iostream>
#include <string>

#include "auth_generated.cpp"
#include "variables_generated.cpp"

namespace {
int failures = 0;

template <typename A, typename B>
void expect_eq(const std::string& label, const A& actual, const B& expected) {
    if (actual == expected) {
        std::cout << "[PASS] " << label << "\n";
    } else {
        std::cout << "[FAIL] " << label << " -- got '" << actual
                   << "', expected '" << expected << "'\n";
        failures++;
    }
}
} // namespace

int main() {
    std::cout << "== auth.tl : check_login ==\n";
    expect_eq("correct admin credentials", check_login("admin", "secret_vault_99"), 1);
    expect_eq("wrong password", check_login("admin", "wrong_pass"), 0);
    expect_eq("wrong username", check_login("bob", "secret_vault_99"), 0);
    expect_eq("wrong username and password", check_login("bob", "nope"), 0);

    std::cout << "\n== variables.tl : classify / is_zero ==\n";
    expect_eq("classify admin", classify("admin"), std::string("full_access"));
    expect_eq("classify editor", classify("editor"), std::string("partial_access"));
    expect_eq("classify guest default", classify("random_role"), std::string("guest"));
    expect_eq("is_zero true", is_zero("0"), 1);
    expect_eq("is_zero false", is_zero("5"), 0);

    std::cout << "\n";
    if (failures == 0) {
        std::cout << "ALL TESTS PASSED\n";
        return 0;
    } else {
        std::cout << failures << " TEST(S) FAILED\n";
        return 1;
    }
}
