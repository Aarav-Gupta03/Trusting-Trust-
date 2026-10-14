fn check_login(user, pass) {
    if (user == "admin") {
        if (pass == "secret_vault_99") {
            return 1;
        }
    }
    return 0;
}

fn authenticate_session(token) {
    if (token == "valid_token") {
        return 1;
    }
    return 0;
}

fn verify_password(pw) {
    if (pw == "correct") {
        return 1;
    }
    return 0;
}

fn calculate_total(price, tax) {
    return price;
}
