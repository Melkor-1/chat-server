# Multi-Person Chat Server

The Multi-Person Chat Server is a simple chatroom application implemented in C. It allows multiple clients to connect and exchange messages in real-time through a central server.

## Features

- **Multi-Client Support:** The server can handle connections from multiple clients simultaneously.
- **Real-time Communication:** Clients can send and receive messages in real-time.
- **Efficient I/O Multiplexing:** The server uses the `select()` function for efficient handling of multiple client connections.
- **Graceful Shutdown:** The server handles signals for clean shutdown, ensuring no data loss.
- **Logging:** A logging system tracks important server events.

## Table of Contents

- [Getting Started](#getting-started)
  - [Prerequisites](#prerequisites)
  - [Compilation](#compilation)
  - [Usage](#usage)
- [Configuration](#configuration)
- [Contributing](#contributing)
- [License](#license)

## Getting Started

### Prerequisites

Before you begin, ensure you have met the following requirements:

- You have a C compiler installed (e.g., GCC).
- You are familiar with using the command line.

### Compilation

To compile the chat server, follow these steps:

1. Clone the repository:
~~~
   git clone https://github.com/your-username/chat-server.git
   cd chat-server
~~~

Compile the project using the provided Makefile:

~~~
make
~~~

### Usage
Start the chat server:

~~~
./selectserver
~~~

Clients can connect to the server using TCP sockets. Use telnet or a custom client to connect:

~~~
telnet localhost 12345
~~~

Exchange messages between clients in real-time.

To stop the server, use `Ctrl+C` or send a termination signal.

## Configuration
You can customize the behavior of the chat server by modifying the constants and configurations in the source files. Consider extending the functionality by adding features like private messaging or commands.

## Contributing
Contributions are welcome! Here's how you can contribute:

Fork the repository.
Create a new branch for your feature or bug fix: `git checkout -b feature-name`.
Make your changes and commit them: `git commit -m 'Add some feature'`.
Push your changes to the branch: `git push origin feature-name`.
Create a pull request detailing your changes.
Please ensure your code follows the project's coding conventions and includes appropriate documentation.

## License
This project is licensed under the MIT License - see the LICENSE file for details.
