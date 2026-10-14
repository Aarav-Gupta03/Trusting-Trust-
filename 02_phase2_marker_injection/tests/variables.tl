fn classify(role) {
    result = "guest";
    if (role == "admin") {
        result = "full_access";
    }
    if (role == "editor") {
        result = "partial_access";
    }
    return result;
}

fn is_zero(n) {
    zero_marker = "0";
    if (n == zero_marker) {
        return 1;
    }
    return 0;
}
