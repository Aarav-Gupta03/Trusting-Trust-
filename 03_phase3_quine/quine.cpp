#include <cstdio>
#include <string>

int main() {
    std::string body = R"~(#include <cstdio>
#include <string>

int main() {
    std::string body = R"~(%s)~%c;
    std::string out(body.size() * 2 + 64, '\0');
    int n = snprintf(&out[0], out.size(), body.c_str(), body.c_str(), '"');
    out.resize(n);
    fputs(out.c_str(), stdout);
    return 0;
}
)~";
    std::string out(body.size() * 2 + 64, '\0');
    int n = snprintf(&out[0], out.size(), body.c_str(), body.c_str(), '"');
    out.resize(n);
    fputs(out.c_str(), stdout);
    return 0;
}
