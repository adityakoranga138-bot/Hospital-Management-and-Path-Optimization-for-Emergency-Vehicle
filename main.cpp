#include<iostream>
#include<string>
#include<vector>
#include<queue>
using namespace std;

class SmartCareException : public runtime_error {
public:
    SmartCareException(const string &msg):runtime_error(msg) {
    }
};
class WardFullException : public SmartCareException {
public:
    WardFullException(const string &msg):SmartCareException("[WARD SATURATED] "+msg) {
    }
};
class InvalidNodeException : public SmartCareException {
public:
    InvalidNodeException(const string &msg):SmartCareException("[MAP ERROR] "+msg) {
    }
};
class PatientNotFoundException : public SmartCareException {
public:
    PatientNotFoundException(const string& msg):SmartCareException("[RECORD ERROR] "+msg) {
    }
};

class Patient {
private:
    int id;
    string name;
    int priority;
    int bedType;
    int ambulanceFare;
    int pickupLocation;

public:
    Patient() {
        id = 0;
        name = "EMPTY";
        priority = 2;
        bedType = 0;
        ambulanceFare = 0;
        pickupLocation = 0;
    }

    Patient(int pId, string pName, int pPriority, int bType, int fare, int pLoc = 0) {
        id = pId;
        name = pName;
        priority = pPriority;
        bedType = bType;
        ambulanceFare = fare;
        pickupLocation = pLoc;
    }

    int getId() {
        return id;
    }

    string getName() {
        return name;
    }

    int getPriority() {
        return priority;
    }

    int getBedType() {
        return bedType;
    }

    int getAmbulanceFare() {
        return ambulanceFare;
    }

    int getPickupLocation() {
        return pickupLocation;
    }

    void setAmbulanceFare(int fare) {
        ambulanceFare = fare;
    }
};

struct CompareTriagePriority {
    bool operator()(Patient a, Patient b) {
        return a.getPriority() > b.getPriority();
    }
};

class Medicine {
private:
    string name;
    int stock;
    float price;

public:
    Medicine() {
        name = "";
        stock = 0;
        price = 0.0;
    }

    Medicine(string mName, int mStock, float mPrice) {
        name = mName;
        stock = mStock;
        price = mPrice;
    }

    string getName() {
        return name;
    }

    int getStock() {
        return stock;
    }

    float getPrice() {
        return price;
    }

    bool dispense(int qty) {
        if (stock >= qty) {
            stock -= qty;
            return true;
        }
        return false;
    }
};

const int NUM_LOCATIONS = 10;
const int INF = 9999;

class CityMap {
private:
    int roadMatrix[NUM_LOCATIONS][NUM_LOCATIONS];
    string locationNames[NUM_LOCATIONS];

public:
    CityMap() {
        locationNames[0] = "Rajpur_road";
        locationNames[1] = "Paltan_Bazar";
        locationNames[2] = "Clock_tower";
        locationNames[3] = "Prem_nagar";
        locationNames[4] = "Clement_Town";
        locationNames[5] = "Sahastradhara";
        locationNames[6] = "Jakhan";
        locationNames[7] = "Vasant_Vihar";
        locationNames[8] = "Patel_Nagar";
        locationNames[9] = "ISBT";
        int defaultDistances[NUM_LOCATIONS][NUM_LOCATIONS] = {
            {0, 4, 6, INF, INF, INF, INF, INF, INF, INF},
            {4, 0, INF, 5, 7, INF, INF, INF, INF, INF},
            {6, INF, 0, 3, INF, 8, INF, INF, INF, INF},
            {INF, 5, 3, 0, 4, 6, INF, INF, INF, INF},
            {INF, 7, INF, 4, 0, INF, 3, 6, INF, INF},
            {INF, INF, 8, 6, INF, 0, INF, 5, 7, INF},
            {INF, INF, INF, INF, 3, INF, 0, INF, INF, 4},
            {INF, INF, INF, INF, 6, 5, INF, 0, 4, 5},
            {INF, INF, INF, INF, INF, 7, INF, 4, 0, 6},
            {INF, INF, INF, INF, INF, INF, 4, 5, 6, 0}
        };

        for (int i = 0; i < NUM_LOCATIONS; i++) {
            for (int j = 0; j < NUM_LOCATIONS; j++) {
                roadMatrix[i][j] = defaultRoads[i][j];
            }
        }
    }

    string getLocationName(int index) {
        if (index >= 0 && index < NUM_LOCATIONS) {
            return locationNames[index];
        }
        return "Unknown Location";
    }

    int findFastestRoute(int startNode, int destNode, bool printPath = false) {  
        if (startNode<0 || startNode>=NUM_LOCATIONS || destNode<0 || destNode>=NUM_LOCATIONS) {
            throw InvalidNodeException("Invalid node coordinates");
        }
        int d[NUM_LOCATIONS];
        bool visited[NUM_LOCATIONS];
        int parent[NUM_LOCATIONS];
        for (int i = 0; i < NUM_LOCATIONS; i++) {
            d[i] = INF;
            visited[i] = false;
            parent[i] = -1;
        }

        d[startNode] = 0;

        for (int count = 0; count < NUM_LOCATIONS - 1; count++) {
            int minVal = INF;
            int u = -1;

            for (int i = 0; i < NUM_LOCATIONS; i++) {
                if (!visited[i] && d[i] < minVal) {
                    minVal = d[i];
                    u = i;
                }
            }

            if (u == -1)
                break;

            visited[u] = true;

            for (int v = 0; v < NUM_LOCATIONS; v++) {
                if (!visited[v] && roadMatrix[u][v] != INF && d[u] != INF) {
                    if (d[u] + roadMatrix[u][v] < d[v]) {
                        d[v] = d[u] + roadMatrix[u][v];
                        parent[v] = u;
                    }
                }
            }
        }

        if (d[destNode] == INF) {
            throw InvalidNodeException("No reachable road path found between selected origin and destination.");
        }
        
        if (printPath) {
            vector<int> path;
            int curr = destNode;

            while (curr != -1) {
                path.push_back(curr);
                curr = parent[curr];
            }

            cout << "Route Traversed: ";

            for (int i = path.size() - 1; i >= 0; i--) {
                cout << locationNames[path[i]];
                if (i > 0)
                    cout << " -> ";
            }

            cout << endl;
        }

        return d[destNode];
    }
};

int MAX_BEDS = 100;

class HospitalSystem {
private:
    vector<Patient> admittedPatients;
    priority_queue<Patient, vector<Patient>, CompareTriagePriority> dispatchQueue;
    CityMap cityMap;
    vector<Medicine> pharmacy;

    const string PATIENT_FILE = "admitted_patients.txt";
    const string PHARMACY_FILE = "pharmacy_stock.txt";
    const string INVOICE_LOG = "invoices_log.txt";

public:
    HospitalSystem() {
        loadPharmacyFromFile();
        loadPatientsFromFile();
    }

    ~HospitalSystem() {
        savePatientsToFile();
        savePharmacyToFile();
    }

    void savePatientsToFile() {
        ofstream outFile(PATIENT_FILE);
        if (!outFile) return;
        outFile << admittedPatients.size() << "\n";
        for (const auto& p : admittedPatients) {
            outFile << p.getId() << " "
                    << p.getName() << " "
                    << p.getPriority() << " "
                    << p.getBedType() << " "
                    << p.getAmbulanceFare() << " "
                    << p.getPickupLocation() << "\n";
        }
        outFile.close();
    }

    void loadPatientsFromFile() {
        ifstream inFile(PATIENT_FILE);
        if (!inFile) return;
        int totalCount = 0;
        if (inFile >> totalCount) {
            admittedPatients.clear();
            for (int i = 0; i < totalCount; i++) {
                int id, priority, bedType, fare, loc;
                string name;
                if (inFile >> id >> name >> priority >> bedType >> fare >> loc) {
                    admittedPatients.push_back(Patient(id, name, priority, bedType, fare, loc));
                }
            }
        }
        inFile.close();
    }

    void savePharmacyToFile() {
        ofstream outFile(PHARMACY_FILE);
        if (!outFile) return;
        outFile << pharmacy.size() << "\n";
        for (const auto& m : pharmacy) {
            outFile << m.getName() << " " << m.getStock() << " " << m.getPrice() << "\n";
        }
        outFile.close();
    }

    void loadPharmacyFromFile() {
        ifstream inFile(PHARMACY_FILE);
        if (!inFile) {
            pharmacy.push_back(Medicine("Paracetamol", 50, 5.0f));
            pharmacy.push_back(Medicine("Amoxicillin", 30, 15.0f));
            pharmacy.push_back(Medicine("Ibuprofen", 40, 8.0f));
            savePharmacyToFile();
            return;
        }
        int totalCount = 0;
        if (inFile >> totalCount) {
            pharmacy.clear();
            for (int i = 0; i < totalCount; i++) {
                string name;
                int stock;
                float price;
                if (inFile >> name >> stock >> price) {
                    pharmacy.push_back(Medicine(name, stock, price));
                }
            }
        }
        inFile.close();
    }

    bool hasBedAvailable() {
        return admittedPatients.size() < MAX_BEDS;
    }

    int getOccupiedCount() {
        return admittedPatients.size();
    }

    bool queueEmergencyCall(int pickupLoc, int id, string name, int priority) {
        if (pickupLoc < 0 || pickupLoc > 2) {
            cout << "[ERROR] Invalid pickup location index! Node not found." << endl;
            return false;
        }

        int bedType = (priority == 1) ? 2 : 1;
        dispatchQueue.push(Patient(id, name, priority, bedType, 0, pickupLoc));

        cout << "[TRIAGE QUEUE] Emergency call logged for " << name
             << " (Severity Rank: " << (priority == 1 ? "1 - Critical" : "2 - Normal") << ")" << endl;

        return true;
    }

    void processNextDispatch() {
        if (dispatchQueue.empty()) {
            cout << "[QUEUE EMPTY] No pending emergency vehicle calls." << endl;
            return;
        }

        if (!hasBedAvailable()) {
            cout << "[ALERT] All " << MAX_BEDS << " hospital beds are occupied!" << endl;
            cout << "[ACTION] Emergency vehicle NOT sent. Patient diverted to external facility." << endl;
            return;
        }

        Patient nextPatient = dispatchQueue.top();
        dispatchQueue.pop();

        cout << endl << "============================================" << endl;
        cout << "  EMERGENCY VEHICLE DISPATCHED & PATIENT ADMITTED" << endl;
        cout << "============================================" << endl;

        int travelTime = cityMap.findFastestRoute(nextPatient.getPickupLocation(), 3, true);
        int fare = travelTime * 50;
        nextPatient.setAmbulanceFare(fare);

        admittedPatients.push_back(nextPatient);

        cout << "Name : " << nextPatient.getName() << endl;
        cout << "Patient ID : " << nextPatient.getId() << endl;
        cout << "Condition Triage : " << (nextPatient.getPriority() == 1 ? "PRIORITY 1 - Critical (Heart Attack)" : "PRIORITY 2 - Normal (Fever)") << endl;
        cout << "Pickup Point : " << cityMap.getLocationName(nextPatient.getPickupLocation()) << endl;
        cout << "Estimated Transit : " << travelTime << " minutes" << endl;
        cout << "Ambulance Fare : Rs. " << fare << endl;
        cout << "Reserved Bed Slot : Bed " << admittedPatients.size() << " (" << (nextPatient.getBedType() == 1 ? "General Ward" : "ICU") << ")" << endl;
        cout << "============================================" << endl;
    }

    void displayQueueStatus() {
        cout << endl << "--------------------------------------------" << endl;
        cout << "  PENDING EMERGENCY CALLS (" << dispatchQueue.size() << " in queue)" << endl;
        cout << "--------------------------------------------" << endl;

        if (dispatchQueue.empty()) {
            cout << "No pending calls waiting for dispatch." << endl;
            return;
        }

        Patient topPatient = dispatchQueue.top();

        cout << "Next Dispatched Call -> " << topPatient.getName()
             << " | Priority Rank: " << topPatient.getPriority() << endl;
        cout << "--------------------------------------------" << endl;
    }

    void displayBeds() {
        cout << endl << "--------------------------------------------" << endl;
        cout << "  HOSPITAL WARD OCCUPANCY (" << admittedPatients.size() << "/" << MAX_BEDS << ")" << endl;
        cout << "--------------------------------------------" << endl;

        if (admittedPatients.empty()) {
            cout << "All beds are currently vacant." << endl;
            return;
        }

        for (size_t i = 0; i < admittedPatients.size(); i++) {
            cout << "Bed " << (i + 1)
                 << " | ID: " << admittedPatients[i].getId()
                 << " | Name: " << admittedPatients[i].getName()
                 << " | Severity: " << (admittedPatients[i].getPriority() == 1 ? "Critical" : "Normal")
                 << " | Ward: " << (admittedPatients[i].getBedType() == 1 ? "General Ward (Rs. 4000/day)" : "ICU (Rs. 35000/day)")
                 << " | Inbound Fare: Rs. " << admittedPatients[i].getAmbulanceFare() << endl;
        }

        cout << "--------------------------------------------" << endl;
    }

    void displayPharmacy() {
        cout << endl << "--------------------------------------------" << endl;
        cout << "  PHARMACY INVENTORY" << endl;
        cout << "--------------------------------------------" << endl;

        for (size_t i = 0; i < pharmacy.size(); i++) {
            cout << (i + 1) << ". " << pharmacy[i].getName()
                 << " | Stock: " << pharmacy[i].getStock()
                 << " | Unit Price: Rs. " << pharmacy[i].getPrice() << endl;
        }

        cout << "--------------------------------------------" << endl;
    }

    bool dischargePatient(int patientId, int daysStayed) {
        int foundIndex = -1;

        for (size_t i = 0; i < admittedPatients.size(); i++) {
            if (admittedPatients[i].getId() == patientId) {
                foundIndex = i;
                break;
            }
        }

        if (foundIndex == -1) {
            cout << "[ERROR] Patient ID " << patientId << " not found in active ward records!" << endl;
            return false;
        }

        Patient p = admittedPatients[foundIndex];
        int dailyRate = (p.getBedType() == 1) ? 4000 : 35000;
        int roomRent = dailyRate * daysStayed;
        int totalBill = p.getAmbulanceFare() + roomRent;

        cout << endl << "============================================" << endl;
        cout << "  DISCHARGE & FINAL INVOICE" << endl;
        cout << "============================================" << endl;
        cout << "Patient ID       : " << p.getId() << endl;
        cout << "Patient Name     : " << p.getName() << endl;
        cout << "Medical Severity : " << (p.getPriority() == 1 ? "Critical (Heart Attack)" : "Normal (Fever)") << endl;
        cout << "Ward Category    : " << (p.getBedType() == 1 ? "General Ward (Rs. 4000/day)" : "ICU (Rs. 35000/day)") << endl;
        cout << "Stay Duration    : " << daysStayed << " day(s)" << endl;
        cout << "--------------------------------------------" << endl;
        cout << "Ambulance Transit: Rs. " << p.getAmbulanceFare() << endl;
        cout << "Room Stay Charge : Rs. " << roomRent << endl;
        cout << "TOTAL PAYABLE    : Rs. " << totalBill << endl;
        cout << "============================================" << endl;

        admittedPatients.erase(admittedPatients.begin() + foundIndex);
        cout << "[SYSTEM] Bed slot deallocated. Record safely cleared." << endl;

        return true;
    }
};

int main() {
    HospitalSystem hospital;
    CityMap map;
    int choice = 0;

    while (choice != 7) {
        cout << endl << "========== SMARTCARE EMERGENCY SYSTEM ==========" << endl;
        cout << "1. View City Locations & Test Route (Dijkstra's Algorithm)" << endl;
        cout << "2. Register Emergency Call (Add to Priority Queue)" << endl;
        cout << "3. Dispatch Ambulance (Highest Priority First)" << endl;
        cout << "4. View Ward Beds & Call Queue Status" << endl;
        cout << "5. View Pharmacy Inventory" << endl;
        cout << "6. Discharge Patient & Generate Final Bill" << endl;
        cout << "7. Exit System" << endl;
        cout << "Enter your choice (1-7): ";

        if (!(cin >> choice)) {
            cin.clear();
            cin.ignore(1000, '\n');
            cout << "[ERROR] Invalid numeric choice." << endl;
            continue;
        }

        switch (choice) {
            case 1: {
                cout << endl << "[CITY NODES]" << endl;

                for (int i = 0; i < NUM_LOCATIONS; i++) {
                    cout << i << ". " << map.getLocationName(i) << endl;
                }

                int fromNode;
                cout << endl << "Enter pickup node (0-2) to test transit to Central Hospital (3): ";
                cin >> fromNode;

                if (fromNode < 0 || fromNode > 2) {
                    cout << "[ERROR] Node does not exist on map." << endl;
                } else {
                    cout << endl;
                    int time = map.findFastestRoute(fromNode, 3, true);
                    cout << "Dijkstra optimal transit time: " << time << " mins." << endl;
                }

                break;
            }

            case 2: {
                int pId, pickupLoc, condition;
                string pName;

                cout << endl << "--- REGISTER EMERGENCY CALL ---" << endl;
                cout << "Select Pickup Location (0: Spot A, 1: Junction B, 2: City Center C): ";
                cin >> pickupLoc;
                cout << "Enter Patient ID: ";
                cin >> pId;
                cout << "Enter Patient Name: ";
                cin >> pName;
                cout << "Condition Severity (1 = Critical/Heart Attack, 2 = Normal/Fever): ";
                cin >> condition;

                hospital.queueEmergencyCall(pickupLoc, pId, pName, condition);
                break;
            }

            case 3: {
                cout << endl << "[DISPATCH] Checking ward capacity and retrieving priority call..." << endl;
                hospital.processNextDispatch();
                break;
            }

            case 4:
                hospital.displayBeds();
                hospital.displayQueueStatus();
                break;

            case 5:
                hospital.displayPharmacy();
                break;

            case 6: {
                int pId, days;

                cout << endl << "--- PATIENT DISCHARGE & BILLING ---" << endl;
                cout << "Enter Patient ID to discharge: ";
                cin >> pId;
                cout << "Enter total days of hospital stay: ";
                cin >> days;

                if (days < 1)
                    days = 1;

                hospital.dischargePatient(pId, days);
                break;
            }

            case 7:
                cout << endl << "Exiting SmartCare System. Prototype active." << endl;
                break;

            default:
                cout << endl << "Invalid choice. Please choose between 1 and 7." << endl;
                break;
        }
    }

    return 0;
}
