```text
HTTP1.0-Web-Server
├── Makefile
├── README.md
├── build/              # Stores compiled executable files
├── config/             # Stores nginx config files
│   └── config.conf
├── include/            # Header files
│   ├── main.h
│   ├── parser.h
│   ├── socket.h
│   ├── thread_pool.h
│   └── whitelist.h
├── src/                # Source files
│   ├── main.c
│   ├── parser.c
│   ├── socket.c
│   ├── thread_pool.c
│   └── whitelist.c
├── whitelist/          # Stores allowed paths to prevent path injection
│   └── whitelist.txt
└── www/                # Input files for parsing
```
