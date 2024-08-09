This project implements a distributed system for calculating prime numbers using multiple processes in a Unix-like environment. The system leverages inter-process communication (IPC) via System V message queues to coordinate and manage tasks. The primary goal is to explore concepts related to process creation, synchronization, and communication in operating systems.

## Objectives

- **Process Management**: Demonstrates how to create and manage multiple child processes that work concurrently on a common task.
- **Inter-Process Communication (IPC)**: Uses a message queue to facilitate communication between a central server process and several child processes, coordinating work distribution and result collection.
- **Synchronization**: Ensures that the server process waits for all child processes to complete their tasks before proceeding.
- **Signal Handling and Timing**: Sets up a timer in the root process to periodically check the computation progress using signal handling for time-based events.

## How It Works

1. **Initialization**:
   - Starts by creating child processes (calculators) and a server process.
   - The server assigns each calculator a specific range of numbers to check for primality.

2. **Communication**:
   - Each calculator process communicates with the server via a message queue, sending messages when a prime number is found and when it finishes its calculations.
   - The server collects these results and writes the prime numbers to a file.

3. **Synchronization and Coordination**:
   - The server ensures all child processes have completed their work before terminating.
   - The root process monitors overall progress by periodically checking in with the server process.

## Compilation
To compile and run the program, use the .sh file
