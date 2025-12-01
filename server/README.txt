# Messaging Server

This is a simple multi-client messaging server written in C. It supports open chat, closed chat, and semi-closed chat modes.

---

## How to Compile and Run

### Prerequisites
- GCC or Clang compiler
- SQLite3 development libraries (`libsqlite3-dev` on Linux)
- POSIX-compatible OS (Linux, macOS)


### Compilation

Open a terminal in the project directory and run:

make

./server 5050 messages.db

This will open the server



Then open a new terminal window:

nc <IP Address> 5050  

or 

nc localhost 5050  



## User Guide

After login this should appear:

Type 'help' for commands.
Available commands:
 - Chat <user> <message>    (open mode)
 - getmessages <user>
 - deletemessages <user>
 - getuserlist
 - Menu   (interactive chatrooms)
 - select <username>   (enter closed chat)
 - open   (go to open mode)
 - /help  (in closed mode show this help)
 - exit

Just follow the instructions given

If you want to close the server use "control" + "c"
