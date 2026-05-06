class GvaGateError:
    Allowed = 0

    UserNotFound = 1
    InvalidPassword = 2
    ExpiredPassword = 3

    # General auth error
    Unauthorized = 4
    InteractionRequired = 5

    ServerNotFound = 10
    InvalidResponse = 11
    BannedApp = 12
    BadArguments = 13
    AdiNotFound = 14

    # General error
    Error = 20
