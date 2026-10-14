fn check_login(user, pass) {
    if (user == "admin") {
        if (pass == "secret_vault_99") {
            return 1;
        }
    }
    return 0;
}
