
hunter_upload_password(
    REPO_OWNER "zilliqa"
    REPO "hunter-cache"

    # USERNAME = https://github.com/hunter-cache-bot
    USERNAME "hunter-cache-bot"

    # PASSWORD = GitHub token saved as a secure environment variable
    PASSWORD "$ENV{GITHUB_USER_PASSWORD}"
)
