# SmartCare – Emergency Hospital Management System

A C++-based Emergency Hospital Management System designed to streamline emergency patient registration, ambulance dispatch, hospital bed allocation, pharmacy inventory management, and patient discharge. The system uses Data Structures and Algorithms (DSA), Object-Oriented Programming (OOP), and a web-based dashboard connected to a C++ backend through an HTTP server.

## Features

* Emergency patient registration with condition-based triage priority.
* Priority-based emergency queue using `std::priority_queue`.
* Ambulance dispatch based on emergency priority.
* Shortest route calculation using Dijkstra's algorithm on a weighted city graph.
* Route distance, estimated transit time, and ambulance fare calculation.
* Automatic hospital bed allocation based on availability and ward requirements.
* Hospital ward occupancy and bed availability tracking.
* Pharmacy inventory management with medicine stock and pricing.
* Patient discharge and automated billing.
* Individual patient receipt generation.
* File handling for storing patient records, pharmacy stock, and invoices.
* Interactive web dashboard for accessing hospital management modules.
* C++ backend integrated with the frontend using the `cpp-httplib` HTTP library.

## Tech Stack

* **Programming Language**: C++ (C++11)
* **Frontend**: HTML, CSS, JavaScript
* **Backend**: C++ HTTP Server using `cpp-httplib`
* **Data Structures**: Priority Queue, Weighted Graph, Adjacency Matrix, Vectors, Maps
* **Algorithms**: Dijkstra's Shortest Path Algorithm
* **OOP Concepts**: Classes, Objects, Encapsulation, Constructors, Destructors, Exception Handling
* **File Handling**: C++ File I/O

## Run on Windows (MinGW / g++)

Make sure MinGW g++ is installed and available in your system PATH.



Open the project folder in VS Code or Cursor and run the following command in the terminal:

```bash
g++ -std=c++11 smartcare_server.cpp -o smartcare_server.exe -lws2_32
```

After successful compilation, start the server:

```bash
.\smartcare_server.exe
```

The C++ backend server will start locally.

Open the following address in your browser:

[http://127.0.0.1:8080]

Keep the terminal running while using the dashboard. To stop the server, press Ctrl + C.

## Dashboard Modules

The dashboard provides the following sections:

* **Overview**: Displays occupied beds, available beds, triage queue status, and next dispatch information.
* **City Map & Routes**: Provides access to city locations and route calculations.
* **Register Emergency**: Allows registration of emergency patient details.
* **Ambulance Dispatch**: Processes emergency calls and displays dispatch information.
* **Ward Beds**: Displays patient admission and hospital bed occupancy.
* **Pharmacy**: Displays medicine inventory and stock information.
* **Discharge & Billing**: Handles patient discharge and generates billing receipts.

## API Endpoints

The frontend communicates with the C++ backend through the following HTTP endpoints:

* `GET /api/locations` — Retrieve available city locations.
* `GET /api/status` — Retrieve hospital status and emergency queue information.
* `GET /api/beds` — Retrieve ward occupancy and patient bed information.
* `GET /api/pharmacy` — Retrieve pharmacy inventory.
* `GET /api/route?from=<location>` — Calculate the shortest route from a selected location.
* `POST /api/emergency` — Register a new emergency patient.
* `POST /api/dispatch` — Process the next emergency ambulance dispatch.
* `POST /api/discharge` — Discharge a patient and generate the billing receipt.

## Project Structure

```
PBL-Project/
│
├── main.cpp
├── smartcare_server.cpp
├── httplib.h
├── pharmacy_stock.txt
├── README.md
│
└── dashboard/
    ├── index.html
    ├── styles.css
    └── app.js
```

## DSA and OOP Implementation

* **Priority Queue**: Used to manage emergency patients according to their triage priority.
* **Weighted Graph**: Represents the city road network with distances between locations.
* **Dijkstra's Algorithm**: Calculates the shortest route for ambulance dispatch.
* **Vectors and Maps**: Used for storing and managing relevant hospital and graph data.
* **Exception Handling**: Handles exceptional situations such as unavailable beds, invalid locations, and missing patients.
* **File Handling**: Maintains patient records, pharmacy inventory, and invoice information.

## Notes

* This project is developed for academic and educational purposes.
* The application runs locally using a C++ HTTP server.
* The dashboard requires the C++ backend server to be running.
* The `httplib.h` header provides the HTTP server functionality.
* Patient records, pharmacy inventory, and invoices use local file storage.
* The system is an academic demonstration and is not intended to replace a real hospital emergency management system.
* AI tools were used as an aid in understanding and implementing parts of the backend integration due to the team's limited prior experience with backend development.

## Future Scope

* Integration with a real-time GPS and mapping service.
* Improved patient record management and authentication.
* Real-time ambulance tracking.
* Online appointment and hospital registration facilities.
* Enhanced reporting and analytics for hospital administration.

## Team Project

Developed as part of the Problem-Based Learning (PBL) project.

**Project**: SmartCare – Emergency Hospital Management System

**Repository**: PBL-Project
