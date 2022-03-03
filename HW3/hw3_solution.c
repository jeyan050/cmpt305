#include<stdio.h>
#include<time.h>
#include<math.h>
#include<stdlib.h>
#include<unistd.h>
#include<assert.h>
#include<float.h>
#include<stdbool.h>


// Definition of a Queue Node including arrival and service time
struct QueueNode {
    double arrival_time;  // customer arrival time, measured from time t=0, inter-arrival times exponential
    double service_time;  // customer service time (exponential) 
    struct QueueNode *next;  // next element in line; NULL if this is the last element
};

// Suggested queue definition
// Feel free to use some of the functions you implemented in HW2 to manipulate the queue
// You can change the queue definition and add/remove member variables to suit your implementation
struct Queue {

    struct QueueNode* head;    // Point to the first node of the element queue
    struct QueueNode* tail;    // Point to the tail node of the element queue

    struct QueueNode* first;  // Point to the first arrived customer that is waiting for service
    struct QueueNode* last;   // Point to the last arrrived customer that is waiting for service
    int waiting_count;     // Current number of customers waiting for service

    double cumulative_response;  // Accumulated response time for all effective departures
    double cumulative_waiting;  // Accumulated waiting time for all effective departures
    double cumulative_idle_times;  // Accumulated times when the system is idle, i.e., no customers in the system
    double cumulative_number;   // Accumulated number of customers in the system
};


// ------------Global variables------------------------------------------------------
// Feel free to add or remove 
static double computed_stats[4];  // Store computed statistics: [E(n), E(r), E(w), p0]
static double simulated_stats[4]; // Store simulated statistics [n, r, w, sim_p0]
int departure_count = 0;         // current number of departures from queue
double current_time = 0;          // current time during simulation
double previousTime = 0;
bool inService = false;
int lastDepartureForPeriod = 0;
struct QueueNode* nextNodeToAddQueue;
struct QueueNode* nodeInService;
//-----------------Queue Functions------------------------------------
// Feel free to add more functions or redefine the following functions

// Function to add nodes into the queue with its own service time and arrival time
void Insert(struct Queue *q, double arrival_time, double service_time) {      
    struct QueueNode *Node = malloc(sizeof(struct QueueNode));	
    Node->arrival_time = arrival_time;          // Stores arrival and service time in node
    Node->service_time = service_time;
    Node->next = NULL;		                      // Set next node to be NULL (so no next node)

    if (q->head == NULL && q->tail == NULL){    // If empty queue, set head & tail to new node
        q->head = Node; 
        q->tail = Node;
    } else {                                    // else add node at end of queue
        q->tail->next = Node;   
        q->tail = Node;
    }
    q->cumulative_number++;                     // increment the number of nodes in the system (nodes waiting in service + node being serviced)
    return;
}

// Function to add the number of elements in system x that time period, into cumulative amount of elements that are in the system
void addToCumulativeNumber(struct Queue *elementQ){
  elementQ->cumulative_number +=  elementQ->waiting_count * (current_time - previousTime);
  return;
}

// The following function initializes all "D" (i.e., total_departure) elements in the queue
// 1. It uses the seed value to initialize random number generator
// 2. Generates "D" exponentially distributed inter-arrival times based on lambda
//    And inserts "D" elements in queue with the correct arrival times
//    Arrival time for element i is computed based on the arrival time of element i-1 added to element i's generated inter-arrival time
//    Arrival time for first element is just that element's generated inter-arrival time
// 3. Generates "D" exponentially distributed service times based on mu
//    And updates each queue element's service time in order based on generated service times
// 4. Returns a pointer to the generated queue
struct Queue* InitializeQueue(int seed, float lambda, float mu, int total_departures){
    struct Queue* Q = (struct Queue*) malloc(sizeof(struct Queue));     // Initalizing queue
    Q->head = NULL;
    Q->tail = NULL;
    Q->first = NULL;
    Q->last = NULL;
    Q->waiting_count = 0;
    Q->cumulative_response = 0;
    Q->cumulative_waiting = 0;
    Q->cumulative_idle_times = 0;
    Q->cumulative_number = 0;

    double randValARRIVAL;                   // Temporary values to keep track of interarrival and service times
    double interArrivalV;
    double randValSERVICE;
    double previousAT = 0;

    double *arrivalTimes = malloc(total_departures * sizeof(double));   // Arrays to store individual nodes' arrival and service times
    double *serviceTimes = malloc(total_departures * sizeof(double));

    srand(seed);                            // Seed to get different values every simulation

    for (int i = 0; i < total_departures; i++){                   // Loop to create arrival times for nodes
      randValARRIVAL = (double) rand() / (double) (RAND_MAX);             // Get a random value between [0,1)
      interArrivalV = (-1) * (log(1-randValARRIVAL) / (double) lambda);   // Calculates interarrival time

      arrivalTimes[i] = interArrivalV + previousAT;                       // Calculates service time = inter arrival time + previous node's arrival time
      previousAT = arrivalTimes[i];       
    }

    for (int i = 0; i < total_departures; i++){                   // Loop for service times, simular loop but uses mu instead of lambda
      randValSERVICE = (double) rand() / (double) (RAND_MAX);
      serviceTimes[i] = (-1) * (log(1-randValSERVICE) / (double) mu);
    }

    for (int i = 0; i < total_departures; i++){                   // Creates nodes and adds into queue with arrival and service times
      Insert(Q, arrivalTimes[i], serviceTimes[i]);
    }

    free(arrivalTimes);
    free(serviceTimes);
    return Q;
}

// Use the M/M/1 formulas from class to compute E(n), E(r), E(w), p0
void GenerateComputedStatistics(float lambda, float mu){

  double trafficIntensity = (double) lambda / (double) mu;              // P value for calculations

  // E(n) - Average Number in System  
  computed_stats[0] = trafficIntensity / (1 - trafficIntensity);

  // E(r) - Average response time
  computed_stats[1] = 1 / (mu * (1-trafficIntensity));

  // E(w) - Average waiting time
  computed_stats[2] = trafficIntensity / (mu * (1 - trafficIntensity));

  // p0 - Probability of having 0 customers
  computed_stats[3] = 1 - trafficIntensity;

  return;
}

// This function should be called to print periodic and/or end-of-simulation statistics
// Do not modify output format
void PrintStatistics(struct Queue* elementQ, int total_departures, int print_period, float lambda){

  printf("\n");
  if(departure_count == total_departures) printf("End of Simulation - after %d departures\n", departure_count);
  else printf("After %d departures\n", departure_count);

  printf("Mean n = %.4f (Simulated) and %.4f (Computed)\n", simulated_stats[0], computed_stats[0]);
  printf("Mean r = %.4f (Simulated) and %.4f (Computed)\n", simulated_stats[1], computed_stats[1]);
  printf("Mean w = %.4f (Simulated) and %.4f (Computed)\n", simulated_stats[2], computed_stats[2]);
  printf("p0 = %.4f (Simulated) and %.4f (Computed)\n", simulated_stats[3], computed_stats[3]);
}

// This function is called from simulator if the next event is an arrival
// Should update simulated statistics based on new arrival
// Should update current queue nodes and various queue member variables
// *arrival points to queue node that arrived
// Returns pointer to node that will arrive next
struct QueueNode* ProcessArrival(struct Queue* elementQ, struct QueueNode* arrival){
  elementQ->waiting_count++;              // Increment number of nodes in system

  if (elementQ->first == NULL)            // If no nodes in system, set new node to be start of waitlist in system
    elementQ->first = arrival;
  
  elementQ->last = arrival;               // Add node to be back of queue

  if (elementQ->first != elementQ->last)  // Set next element to be either next element in waitlsit of system
    return elementQ->first->next;         // If no other nodes, then just return same node
  else 
    return elementQ->first;
}

// This function is called from simulator if next event is "start_service"
//  Should update queue statistics
struct QueueNode* StartService(struct Queue* elementQ){
  struct QueueNode* nodeInService = elementQ->first;    // Set node at start of waitlist in system to be node being worked on  
  inService = true;                                     // Set system that its working on node, and returns that node
  return nodeInService;
}

// This function is called from simulator if the next event is a departure
// Should update simulated queue statistics 
// Should update current queue nodes and various queue member variables
void ProcessDeparture(struct Queue* elementQ, struct QueueNode* departure){   
  // If node finish with time greater than arrival time + service time, then extra time was the time node was waiting to be serviced
  if (current_time > departure->arrival_time + departure->service_time)
    elementQ->cumulative_waiting += (current_time - (departure->arrival_time+departure->service_time));
  
  elementQ->cumulative_response += (current_time - departure->arrival_time);  // Adding response time to cumalutive response total

  if (elementQ->first == elementQ->last){     // If node was last node in system, then set everything NULL
    elementQ->first = NULL;
    elementQ->last = NULL;
    nodeInService = NULL;
    nextNodeToAddQueue = NULL;
  } else {
    elementQ->first = elementQ->first->next;  // else, Set the next node to be worked on to be first node in position to be worked on from system
  }

  elementQ->waiting_count--;                  // Decrement the numbers of nodes in system, since one left
  departure_count++;                          // Increment Departure count, and note that system is not working on any nodes currently
  inService = false;
  return;
}

// This is the main simulator function
// Should run until departure_count == total_departures
// Determines what the next event is based on current_time
// Calls appropriate function based on next event: ProcessArrival(), StartService(), ProcessDeparture()
// Advances current_time to next event
// Updates queue statistics if needed
// Print statistics if departure_count is a multiple of print_period
// Print statistics at end of simulation (departure_count == total_departure) 
void Simulation(struct Queue* elementQ, float lambda, float mu, int print_period, int total_departures){
  struct QueueNode* currentNode;    // Temperary nodes to keep track on which node in queue is at now, and the one after
  struct QueueNode* nextNode;
  
  double timeStartedService;        // Keep track on when node started service

  while (departure_count <= total_departures){    // Goes through each iteration of departure count
    currentNode = elementQ->head;                     // Resets all variables before starting (next) iteration
    nextNode = currentNode->next;
    nodeInService = NULL;
    nextNodeToAddQueue = NULL;

    elementQ->first = NULL;
    elementQ->last = NULL;

    elementQ->waiting_count = 0;
    elementQ->cumulative_idle_times = 0;
    elementQ->cumulative_number = 0;
    elementQ->cumulative_response = 0;
    elementQ->cumulative_waiting = 0;

    for (int i = 0 ; i < 4; i++)
      simulated_stats[i] = 0;

    if (lastDepartureForPeriod + print_period > total_departures)
      lastDepartureForPeriod = total_departures;
    else
      lastDepartureForPeriod += print_period;
    
    departure_count = 0;
    current_time = 0;
    previousTime = 0;
    inService = false;

    // Simulation Loop of going through iteration of required departures
    while(departure_count < lastDepartureForPeriod){     
      // Start of loop, jumps to the first node's arrival time
      if (current_time == 0){
        previousTime = current_time;                      // Keeps track of previous time, for future uses
        current_time = elementQ->head->arrival_time;      // Set current time to be arrival time of first node
        elementQ->cumulative_idle_times += current_time;  // Add the amount of time to cumulative_idle

        addToCumulativeNumber(elementQ);                  // Updates cumulative_number total (since its curently 0, its wouldnt affect it)
        continue;
      }

      // If on very last departure and on last departure count (lastDepartureForPeriod == total_departures)
      if (departure_count == total_departures-1 && inService == true){
        if ((nodeInService->service_time + timeStartedService) == current_time){    // Check if it's at node's departure time, if so, removes node from service
          ProcessDeparture(elementQ, nodeInService);
        } else {                                                                    // Else, it jumps to finish time of that node
          previousTime = current_time;
          current_time = timeStartedService + nodeInService->service_time;
        }
        addToCumulativeNumber(elementQ);                                            // Updates cumulative_number total
        continue;
      }

      // If at end of list, then we just only need to process all the queued nodes
      if (currentNode == NULL && nextNode == NULL){
        // Possible Scenerios:
        //  1. A node is already in service, so got to jump to node's finish time
        //  2. Add a node into service
        //  3. Or Both 

        if (inService == true){                                                         // Node is already in service
          if (current_time == timeStartedService + nodeInService->service_time){            // Check if node is finished, if so remove it
            ProcessDeparture(elementQ, nodeInService);
          }
        }
        if (nextNodeToAddQueue != NULL){                                                // If there are still node to add to queue
          if (current_time >= nextNodeToAddQueue->arrival_time && inService == false){      // Check if it can be added into service
            StartService(elementQ);
            timeStartedService = current_time;
          }
        }

        // Change Time
        // Possible Scenerios:
        //  1. Node is in service, so jump to its finish time
        //  2. Jump to next node in waiting queue's arrival time
        if (inService == true){                                                                 // If node is being worked on, if so jump to finish time
          previousTime = current_time;
          current_time = timeStartedService + nodeInService->service_time;
        } else {                                                                                // else, go to next node's arrival time thats on the waitlist
          if (current_time < nextNodeToAddQueue->arrival_time)
            elementQ->cumulative_idle_times += nextNodeToAddQueue->arrival_time - current_time; // if time difference, then update total idle time
          previousTime = current_time;
          current_time = nextNodeToAddQueue->arrival_time;
        }
        addToCumulativeNumber(elementQ);      // Updates cumulative_number total 
        continue;
      }

      // If next node is NULL or current node was last node in queue
      if (nextNode == NULL){
        // Possible Scenerios:
        //  1. Add current Node into queue, and change current node to NULL
        //  2. Finish node in service
        //  3. Add node into service
        //  4. Or All 2/3
        if (inService == true){                                                     // If a node is currently being worked on
          if (current_time == (timeStartedService + nodeInService->service_time)){    // Check if node is finished, if so remove it
            ProcessDeparture(elementQ, nodeInService);

            if (current_time >= nextNodeToAddQueue->arrival_time){                    // After departure, check if can add next node into it
              nodeInService = StartService(elementQ);                                 // If so, then add to server, and updated what time it started
              timeStartedService = current_time;
            }
          } else {                                                                    // Else if server is still occupied, then check if can add node into system
            if (current_time == currentNode->arrival_time){
              nextNodeToAddQueue = ProcessArrival(elementQ, currentNode);
            }
          }
        } else {                                                                    // If no node is currently being worked on
          if (nextNodeToAddQueue != NULL){                                            // Check if can add next node into server, if so update variables
            if (current_time >= nextNodeToAddQueue->arrival_time){
              nodeInService = StartService(elementQ);
              timeStartedService = current_time;
            }
          }
          if (current_time == currentNode->arrival_time){                             // Also check if can add node into system
            nextNodeToAddQueue = ProcessArrival(elementQ, currentNode);
          }
        }

        // Change Time
        // Possible Scenerios:
        //  1. Change time to finish time of service
        //      - Check if in service
        //  2. Change time to start time of next node in waiting queue
        //      - Check if queue is empty or not
        if (inService == true){                                             // If node is being worked on/ occupying server
          previousTime = current_time;                                          // Node down time, and jump to finish time of that node
          current_time = timeStartedService + nodeInService->service_time;
        } else {                                                            // Else, jump to next node's arrival time to be added into system
          previousTime = current_time;
          current_time = nextNodeToAddQueue->arrival_time;
        }
        addToCumulativeNumber(elementQ);  // Updates cumulative_number total 
        continue;      
      }
      
      // If server is occupied, and a node is currently being worked on
      if (inService == true){   
        // Scenerios could happen: 
        //  1. Node is still in service, but can another node into waiting queue
        //      - Check if a node can also be added into service at same time
        //  2. Node is finished in service and needs to be taken out
        //      - Can add node into queue
        //  3. Or Both

        if (current_time == currentNode->arrival_time){                           // Check if can add current node into system/waitlist
          nextNodeToAddQueue = ProcessArrival(elementQ, currentNode);
        } 
        if ((nodeInService->service_time + timeStartedService) == current_time){  // Check if node being worked on is finished
          ProcessDeparture(elementQ, nodeInService);                                  // If so, remove it 
          if (nextNodeToAddQueue != NULL){                                            // Check if can add next node in waitlist/system
            if (current_time >= nextNodeToAddQueue->arrival_time){                        // If so, update values and start node's service
              nodeInService = StartService(elementQ);
              timeStartedService = current_time;
            }
          }
        }

        // Change Current Time
        // Possible Scenerios:
        //  1. Go to node's finish time
        //  2. Go to next node to add to waiting queue
        //  (If both at same time, then it wont matter since its going to same time)
        if (inService == true){                                                               // If node is currently being worked on
          if (nodeInService != NULL){
            if ((timeStartedService + nodeInService->service_time) < nextNode->arrival_time){   // Check if finish time of node is earlier than arrival time of next node to add into system/waitlist
              previousTime = current_time;                                                        // Update time values
              current_time = timeStartedService + nodeInService->service_time;
              addToCumulativeNumber(elementQ);                                                    // Updates cumulative_number total 
              continue;
            } else {                                                                            // Else, go to next node's arrival time to be added into queue
              previousTime = current_time;
              current_time = nextNode->arrival_time;
            }
          } else {                                                                              // If waitlist of nodes is empty (no next node after node in server)
            previousTime = current_time;                                                          // Go to next node to be added into waitlist/server
            current_time = nextNode->arrival_time;
          }
        } else {                                                                                      // No node is being worked on 
          if (nextNodeToAddQueue != NULL){                                                              // If theres a node in the waitlist
            if (nextNodeToAddQueue->arrival_time < nextNode->arrival_time){                             // If so, then check if arrival time is less than next node's arrival time in queue to be added to wailist
              if (current_time < nextNodeToAddQueue->arrival_time)                                        // If is less, then update idle time if current time less
                elementQ->cumulative_idle_times += (nextNodeToAddQueue->arrival_time - current_time);
              current_time = nextNodeToAddQueue->arrival_time;                                            // Change current time to first in waitlist node's arrival time
            } else {                                                                                    // If not less than
              previousTime = current_time;                                                                // Then jump to time to next node to add in waitlist
              if (current_time < nextNode->arrival_time)                                                  // Updates cumulative_idle time total
                elementQ->cumulative_idle_times += (nextNode->arrival_time - current_time);
              current_time = nextNode->arrival_time;
            }
          } else {                                                                                      // If no other node in waitlist
            previousTime = current_time;                                                                  // Check if requires to jump to arrival time, if so add difference to cumulative_idle time
            if (current_time < nextNode->arrival_time)
              elementQ->cumulative_idle_times += (nextNode->arrival_time - current_time);
            current_time = nextNode->arrival_time;                                                        // Jump to node's arrival time to be added into waitlist
          }
        }

      // If no node is being worked on
      } else  if (inService == false){                   
        // Scenerios could happen:
        //  1. Time to add node into service
        //  2. Add node to waiting queue
        //  3, Need to server to idle until next node in queue
        //  4. Both Idle server and add node into Queue

        if (nextNodeToAddQueue != NULL){                                    // If there is a node in waitlist that can be added into service
          if (current_time == nextNodeToAddQueue->arrival_time)               // If so, check if arrival time is equal to current time, if so add it to service
            nodeInService = StartService(elementQ);
        } else {                                                            // If no node is in the waitlist
          if (current_time == currentNode->arrival_time){                     // Check if node next in queue can be added to waitlist  , if so add it
            nextNodeToAddQueue = ProcessArrival(elementQ, currentNode);        
            if (inService == false){                                          // If no node is being worked on, add new node that was added in waitlist to be worked on
              nodeInService = StartService(elementQ);
              timeStartedService = current_time;
            }
          }
        }

        // Change Current Time
        // Possible Scenerios:
        //  1. Go to first node in queue's arrival time (could mean server will idle)
        //  2. Go to next node to add to waiting queue (could also idle)
        //      - Need to do a check if waiting queue is empty
        //  (If both at same time, then it wont matter since its going to same time)
        if (inService == true){                                                             // If node is being worked on currently
          if (nextNode->arrival_time < (nodeInService->service_time+timeStartedService)){     // Check if finish time of node in service is greater tha next node to queue's arrival time
            previousTime = current_time;                                                      // If so, change time to jump to next node in queue arrival time
            current_time = nextNode->arrival_time;
          } else {                                                                            // Else, jump to finish time of node in server currently
            previousTime = current_time;
            current_time = current_time + nodeInService->service_time;
            addToCumulativeNumber(elementQ);                                                  // Updates cumulative_number total 
            continue;
          }
        } else if (nextNodeToAddQueue == NULL){                                             // If waiting list queue is empty
          elementQ->cumulative_idle_times += (nextNode->arrival_time - current_time);         // Change time to next node to add into waitlist arrival time
          previousTime = current_time;                                                        // Update idle time with time jump
          current_time = nextNode->arrival_time;

        } else {                                                                            // If no node is in service
          if (nextNodeToAddQueue->arrival_time < nextNode->arrival_time){                     // Check if next node to add to service's arrival time is less than next node to add to queue arrvial time
            if (current_time < nextNodeToAddQueue->arrival_time){                             // If so, add time different to cumulative idle total time
              elementQ->cumulative_idle_times += (nextNodeToAddQueue->arrival_time - current_time); 
            }
            previousTime = current_time;                                                      // Change time to next node to service's arrival time
            current_time = nextNodeToAddQueue->arrival_time;
          } else {                                                                          // Else, add node into waiting queue
            if (current_time < nextNode->arrival_time)                                        // Add time different to cumulative idle total time
              elementQ->cumulative_idle_times += (nextNode->arrival_time - current_time);
            previousTime = current_time;
            current_time = nextNode->arrival_time;
          }
        }
      }
          
      if (currentNode != NULL)            // While theres still another node next to it, change current to it
        currentNode = currentNode->next;
      if (nextNode != NULL)               // Same thing for nextNode
        nextNode = nextNode->next;
      addToCumulativeNumber(elementQ);
    }

    // Calculating simulated values
    simulated_stats[0] = elementQ->cumulative_number / current_time;              // Simulated n
    simulated_stats[1] = elementQ->cumulative_response / lastDepartureForPeriod;  // Simulated r
    simulated_stats[2] = elementQ->cumulative_waiting / lastDepartureForPeriod;   // Simulated w
    simulated_stats[3] = elementQ->cumulative_idle_times / current_time;          // Simulated p0

    PrintStatistics(elementQ, total_departures, print_period, lambda);            // Print Statistics

    if (lastDepartureForPeriod == total_departures)     // If at final iteration, then stop
      break;
  }

}


// Free memory allocated for queue at the end of simulation
void FreeQueue(struct Queue* elementQ) {
  struct QueueNode* current;                  // Variable for current node
  while (elementQ->tail != NULL){             // While it not at end of tail
    current = elementQ->head;                   // Set temp node to be head
    if (current == elementQ->tail)              // If last node, then set tail NULL
      elementQ->tail = NULL;
        
    if (current->next != NULL)                  // While next node isnt NULL, set new head
      elementQ->head = elementQ->head->next;
    free(current);                              // Delete old head node
  }
}

// Program's main function
int main(int argc, char* argv[]){

	// input arguments lambda, mu, D1, D, S
	if(argc >= 6){

    float lambda = atof(argv[1]);
    float mu = atof(argv[2]);
    int print_period = atoi(argv[3]);
    int total_departures = atoi(argv[4]);
    int random_seed = atoi(argv[5]);

    // Add error checks for input variables here, exit if incorrect input
    if (print_period > total_departures){
      printf("Print Period value cannot be larger than Total Departures.\n");
      return 0;
    }
    if (lambda <= 0){
      printf("Lambda cannot be 0 or negative.\n");
      return 0;
    } 
    if (mu <= 0){
      printf("Mu cannot be 0 or negative.\n");
      return 0;
    }

    // If no input errors, generate M/M/1 computed statistics based on formulas from class
    GenerateComputedStatistics(lambda, mu);

    // Start Simulation
    printf("Simulating M/M/1 queue with lambda = %f, mu = %f, D1 = %d, D = %d, S = %d\n", lambda, mu, print_period, total_departures, random_seed); 
    struct Queue* elementQ = InitializeQueue(random_seed, lambda, mu, total_departures); 
    Simulation(elementQ, lambda, mu, print_period, total_departures);
    FreeQueue(elementQ);
    free(elementQ);
	}
	else printf("Insufficient number of arguments provided!\n");
   
	return 0;
}
