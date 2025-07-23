# Elevator Simulation Project

## 📖 Overview

This project is a comprehensive elevator control system designed for a multi-story building, developed as a major project for CAB403. It simulates the operation of multiple elevator components that communicate via TCP/IP and POSIX shared memory. The system is comprised of several distinct components, each responsible for a specific aspect of the elevator's functionality.

### Key Components

* **Car**: Manages the operations of a single elevator car, including movement, door control, and status updates.
* **Controller**: The central control system that schedules and manages all elevators in the network.
* **Call Pad**: A command-line tool to simulate user requests for an elevator from different floors.
* **Internal Controls**: A command-line interface to simulate button presses inside the elevator, such as opening/closing doors and activating service mode.
* **Safety System**: A safety-critical component that monitors the elevator's internal state and engages emergency mode if necessary.

---

## ✨ Features

* **Multi-Component Architecture**: The system is divided into logical components that communicate through a defined protocol, ensuring modularity and scalability.
* **Inter-Process Communication**: Utilizes TCP/IP for network communication between the car, call pad, and controller, and POSIX shared memory for communication between the car, internal controls, and safety system.
* **Advanced Scheduling**: The controller employs a sophisticated scheduling algorithm to manage multiple cars and user requests efficiently.
* **Safety-Critical Design**: The safety system is developed in accordance with MISRA C guidelines to ensure high standards of safety and reliability.

---

## 🛠️ Getting Started

### Prerequisites

* GCC compiler
* Make build automation tool
* Linux environment (for POSIX shared memory)

### Installation

1.  **Clone the repository:**
    ```sh
    git clone [https://github.com/your-username/elevator-simulation.git](https://github.com/your-username/elevator-simulation.git)
    cd elevator-simulation
    ```

2.  **Build the project:**
    ```sh
    make all
    ```
    This will compile all the necessary components and place the executables in the `bin/` directory.

### Running the Simulation

1.  **Start the controller:**
    ```sh
    ./bin/controller
    ```

2.  **Start the elevator car(s):**
    ```sh
    ./bin/car {name} {lowest_floor} {highest_floor} {delay}
    ```
    * `{name}`: The name of the car (e.g., A, B, Service).
    * `{lowest_floor}`: The lowest floor the car can access (e.g., 1, B1).
    * `{highest_floor}`: The highest floor the car can access (e.g., 10, 20).
    * `{delay}`: The delay in milliseconds for car operations.

3.  **Simulate a call request:**
    ```sh
    ./bin/call {source_floor} {destination_floor}
    ```

4.  **Use internal controls:**
    ```sh
    ./bin/internal {car_name} {operation}
    ```
    * `{operation}` can be `open`, `close`, `stop`, `service_on`, `service_off`, `up`, or `down`.

---

## 💻 Technical Details

### Communication Protocols

* **TCP/IP**: The controller acts as a TCP server on port 3000. The car and call pad components connect to this server to send and receive messages. Messages are prefixed with a 32-bit unsigned integer in network byte order to indicate the message length.
* **POSIX Shared Memory**: Each car creates a shared memory segment named `/car{name}` to store its state. This allows the internal controls and safety system to interact with the car in real-time. A mutex and condition variable are used to ensure data consistency.

### Safety System

The safety system is designed to be a safety-critical component and adheres to MISRA C guidelines. It monitors the shared memory for inconsistencies and safety hazards, such as door obstructions or emergency stop requests, and can put the car into emergency mode to prevent accidents.

---

## 📜 License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for more details.
