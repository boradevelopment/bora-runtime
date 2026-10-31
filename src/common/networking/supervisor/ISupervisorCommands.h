enum SupervisorCommandType : u32 {
    CreateClient = 0x10, // Creates a authorized client (process made by supervisor) for security.
    GetVersion = 0x01,
    Ping = 0x02,
    SendFileDescriptor = 0x03,
    FileDescriptorNotifier = 0x09,
    GetPermissions = 0x04,
    OpenFile = 0x05,
    ReadFile = 0x06,
    SeekFile = 0x09,
    CloseFile = 0x10,
    CreateFile = 0x07,
    WhoAreYou = 0x08 // this is only for debugging and logging, this should NEVER be used for serious security checking
    // Add future command types here as you expand the protocol
};