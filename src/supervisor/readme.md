This manages the BORA Supervisor service which pretty much acts as the source of truth and manager for BORA as a whole

The BORA Supervisor is a process that is launched at startup that accepts any message and returns data.
Supervisor uses IPC communication always as a process to recieve messages and send messages.

This allows external processes like BORA to tell it to for example open the settings interface or a specific
application asking it for permission information or create a request.

The supervisor manages how BORA is updated. 
During an BORA update, it will download from the bora 
