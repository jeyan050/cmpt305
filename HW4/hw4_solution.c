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

    struct EventQueue* eventList;   // Event List

    double cumulative_response;  // Accumulated response time for all effective departures
    double cumulative_waiting;  // Accumulated waiting time for all effective departures
    double cumulative_idle_times;  // Accumulated times when the system is idle, i.e., no customers in the system
    double cumulative_number;   // Accumulated number of customers in the system
};

// Event List
struct EventQueueNode {
    double event_time; // event start time
    int event_type;   // Event type. 1: Arrival; 2: Start Service; 3: Departure
    struct QueueNode* qnode;
    struct EventQueueNode *next;  // pointer to next event
};

struct EventQueue {
    struct EventQueueNode* head;
    struct EventQueueNode* tail;
};


// ------------Global variables------------------------------------------------------
// Feel free to add or remove 
static double computed_stats[4];  // Store computed statistics: [E(n), E(r), E(w), p0]
static double simulated_stats[4]; // Store simulated statistics [n, r, w, sim_p0]
int departure_count = 0;         // current number of departures from queue
double current_time = 0;          // current time during simulation
double previousTime = 0;
bool inService1 = false;
bool inService2 = false;
int lastDepartureForPeriod = 0;
struct QueueNode* nextNodeToAddQueue;
struct QueueNode* nodeInService1;
struct QueueNode* nodeInService2;
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
  int totalService = 0;
  if (inService1 == true){
    totalService++;
  }
  if (inService2 == true){
    totalService++;
  }

  elementQ->cumulative_number +=  (elementQ->waiting_count+totalService) * (current_time - previousTime);
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

    struct EventQueue* EList = (struct EventQueue*) malloc(sizeof(struct EventQueue));
    Q->eventList = EList;
    Q->eventList->head = NULL;               // Event List
    Q->eventList->tail = NULL;


    double randValARRIVAL;                    // Temporary values to keep track of interarrival and service times
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

  double trafficIntensity = (double) lambda / (double)(2 * (double) mu);              // P value for calculations
  
  // p0 - Probability of having 0 customers
  computed_stats[3] = (double) (1 / (1 + ( (pow((2 * trafficIntensity), 2)) / (2 * (1 - trafficIntensity)) + (2 * trafficIntensity))));

  double probWait = (double) (computed_stats[3] * ( (pow((trafficIntensity*2), 2)) / (2 * (1 - trafficIntensity))));

  // E(n) - Average Number in System  
  computed_stats[0] = ( (trafficIntensity * probWait) / (1 - trafficIntensity) + (2 * trafficIntensity));

  // E(r) - Average response time
  computed_stats[1] = (double) (computed_stats[0] / lambda);

  // E(w) - Average waiting time
  computed_stats[2] = (double) ( probWait / (2*mu*(1-trafficIntensity)));

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

// This function is used to add a new event into the event list
// 1 = Arrival ; 2 = StartProcess ; 3 = Departure
void addToEventList(struct Queue* elementQ, struct QueueNode* node, double eventTime, int event){
  
  struct EventQueueNode* newEvent = (struct EventQueueNode*) malloc(sizeof(struct EventQueueNode));
  newEvent->event_time = eventTime;
  newEvent->event_type = event;
  newEvent->qnode = node;
  newEvent->next = NULL;

  if (elementQ->eventList->head == NULL && elementQ->eventList->tail == NULL){
    elementQ->eventList->head = newEvent;
    elementQ->eventList->tail = newEvent;

    struct EventQueueNode* check = elementQ->eventList->head;
    printf("Add : (");
    while (check != NULL){
      printf("%d (%f), ", check->event_type, check->event_time);
      check = check->next;
    }
    printf(")\n");
  } else {
    struct EventQueueNode* currentEvent = elementQ->eventList->head;

    while (currentEvent != NULL){
      if (currentEvent->event_time == newEvent->event_time){
        if ((currentEvent->event_type == 3 && newEvent->event_type == 2) || (currentEvent->event_type == 3 && newEvent->event_type == 1)){    // Departure -> Arrival (Corner case)
          newEvent->next = currentEvent->next;
          currentEvent->next = newEvent;
          return;
        } else {                                                            // Any other combo ()
          newEvent->next = currentEvent;
          if (currentEvent == elementQ->eventList->head){
            elementQ->eventList->head = newEvent;
          }

          struct EventQueueNode* check = elementQ->eventList->head;
          printf("Add : (");
          while (check != NULL){
            printf("%d (%f), ", check->event_type, check->event_time);
            check = check->next;
          }
          printf(")\n");

          return;
        }
      } else if (currentEvent->event_time > newEvent->event_time){
        newEvent->next = currentEvent;
          if (currentEvent == elementQ->eventList->head){
            elementQ->eventList->head = newEvent;
        }

        struct EventQueueNode* check = elementQ->eventList->head;
        printf("Add : (");
        while (check != NULL){
          printf("%d (%f), ", check->event_type, check->event_time);
          check = check->next;
        }
        printf(")\n");

        return;
      } else {
        if (currentEvent == elementQ->eventList->tail){
          currentEvent->next = newEvent;
          elementQ->eventList->tail = newEvent;

          struct EventQueueNode* check = elementQ->eventList->head;
          printf("Add : (");
          while (check != NULL){
            printf("%d (%f), ", check->event_type, check->event_time);
            check = check->next;
          }
          printf(")\n");

          return;
        }
        else 
          currentEvent = currentEvent->next;
      }
    }
  }

  return;
}

void deleteInEventList(struct Queue* elementQ){
  struct EventQueueNode* removedEvent = elementQ->eventList->head;
  if (removedEvent->next != NULL)
    elementQ->eventList->head = removedEvent->next;
  else {
    elementQ->eventList->head = NULL;
    elementQ->eventList->tail = NULL;
  }
  free(removedEvent);

  struct EventQueueNode* check = elementQ->eventList->head;
  printf("Delete : (");
  while (check != NULL){
    printf("%d (%f), ", check->event_type, check->event_time);
    check = check->next;
  }
  printf(")\n");

  return;
}

// This function is called from simulator if the next event is an arrival
// Should update simulated statistics based on new arrival
// Should update current queue nodes and various queue member variables
// *arrival points to queue node that arrived
// Returns pointer to node that will arrive next
struct QueueNode* ProcessArrival(struct Queue* elementQ, struct QueueNode* arrival){
  if (elementQ->eventList->head != NULL)
    deleteInEventList(elementQ);

  elementQ->waiting_count++;              // Increment number of nodes in system

  if (elementQ->first == NULL)            // If no nodes in system, set new node to be start of waitlist in system
    elementQ->first = arrival;
  
  elementQ->last = arrival;               // Add node to be back of queue

  if (arrival->next != NULL){
    struct QueueNode* nextArrival = arrival->next;
    addToEventList(elementQ, nextArrival, nextArrival->arrival_time, 1);
  }

  if (inService1 == false || inService2 == false){
    addToEventList(elementQ, elementQ->first, elementQ->first->arrival_time, 2);
  }

  if (arrival->next == NULL)  // Set next element to be either next element in waitlist of system
    return NULL;         // If no other nodes, then just return same node
  else 
    return arrival->next;
}

// This function is called from simulator if next event is "start_service"
//  Should update queue statistics
struct QueueNode* StartService(struct Queue* elementQ, int serverNum, double timeStarted){
  // Need a way to idenfity which node is being worked on
  struct QueueNode* nodeInService = elementQ->first;    // Set node at start of waitlist in system to be node being worked on 
  deleteInEventList(elementQ); 

  if (elementQ->first != elementQ->last)                // Sets next node to be added into server
    elementQ->first = elementQ->first->next;
  else {                                                // If nodeInService was last node in waitlist
    elementQ->first = NULL;
    elementQ->last = NULL;
  }

  elementQ->waiting_count--;                  // Decrement the numbers of nodes in system, since one left

  if (nodeInService != NULL)
    addToEventList(elementQ, nodeInService, (nodeInService->service_time+timeStarted), 3);   // Add departure

  if (serverNum == 1)
    inService1 = true;                                     // Set system that its working on node, and returns that node
  else 
    inService2 = true;
  
  return nodeInService;
}

// This function is called from simulator if the next event is a departure
// Should update simulated queue statistics 
// Should update current queue nodes and various queue member variables
void ProcessDeparture(struct Queue* elementQ, struct QueueNode* departure, int serverNum){   

  deleteInEventList(elementQ);
  // If node finish with time greater than arrival time + service time, then extra time was the time node was waiting to be serviced
  if (current_time > departure->arrival_time + departure->service_time)
    elementQ->cumulative_waiting += (current_time - (departure->arrival_time+departure->service_time));
  
  elementQ->cumulative_response += (current_time - departure->arrival_time);  // Adding response time to cumalutive response total

  if (elementQ->first != NULL)
    addToEventList(elementQ, elementQ->first, current_time , 2);   // Add start service

  departure_count++;                          // Increment Departure count, and note that system is not working on any nodes currently
  
  if (serverNum == 1)
    inService1 = false;                                     // Set system that its working on node, and returns that node
  else 
    inService2 = false;
  
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
  
  double timeStartedService1;        // Keep track on when node started service
  double timeStartedService2;

  while (departure_count <= total_departures){    // Goes through each iteration of departure count
    currentNode = elementQ->head;                     // Resets all variables before starting (next) iteration
    nextNode = currentNode->next;
    nodeInService1 = NULL;
    nodeInService2 = NULL;
    nextNodeToAddQueue = elementQ->head;

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
    inService1 = false;
    inService2 = false;
    timeStartedService1 = 0;
    timeStartedService2 = 0;

    // Simulation Loop of going through iteration of required departures
    while(departure_count < lastDepartureForPeriod){     
      // Start of loop, jumps to the first node's arrival time
      if (current_time == 0){
        printf("First Time: %f\n", current_time);
        previousTime = current_time;                      // Keeps track of previous time, for future uses
        nextNodeToAddQueue = ProcessArrival(elementQ, currentNode);

        addToCumulativeNumber(elementQ);                  // Updates cumulative_number total (since its curently 0, its wouldnt affect it)
        
        current_time = elementQ->eventList->head->event_time;
        continue;
      }

      // If on very last departure and on last departure count (lastDepartureForPeriod == total_departures)
      if (departure_count == total_departures-1 || departure_count == total_departures-2){
        printf("End Time: %f\n", current_time);
        if ( (nodeInService1->service_time + timeStartedService1) == current_time){
          ProcessDeparture(elementQ, nodeInService1, 1);
        } else if ( (nodeInService2->service_time + timeStartedService2) == current_time){
          ProcessDeparture(elementQ, nodeInService2, 2);
        }

        if ( (nodeInService1->service_time+timeStartedService1) >= (nodeInService2->service_time+timeStartedService2) )
          current_time = nodeInService1->service_time+timeStartedService1;
        else
          current_time = nodeInService2->service_time+timeStartedService2;
    
        addToCumulativeNumber(elementQ);                  // Updates cumulative_number total (since its curently 0, its wouldnt affect it)
        continue;
      }

      // If at end of list, then we just only need to process all the queued nodes
      if (currentNode == NULL && nextNode == NULL){
        // Possible Scenerios:
        //  1. A node is already in service, so got to jump to node's finish time
        //  2. Add a node into service
        //  3. Or Both 

        // If departure
        // printf("Departures: %d\n", departure_count);
        if (elementQ->eventList->head != NULL){
          if (elementQ->eventList->head->event_type == 3){
            if (nodeInService1 == elementQ->eventList->head->qnode){
              if (current_time == elementQ->eventList->head->event_time){
                ProcessDeparture(elementQ, nodeInService1, 1);
                nodeInService1 = NULL;
              }
            }
            if (nodeInService2 == elementQ->eventList->head->qnode){
              if (current_time == elementQ->eventList->head->event_time){
                ProcessDeparture(elementQ, nodeInService2, 2);
                nodeInService2 = NULL;
              }
            }
          }
        }

        // If start service
        else {
          if (inService1 == false || inService2 == false){
            if (inService1 == false){
              timeStartedService1 = current_time;
              nodeInService1 = StartService(elementQ, 1, timeStartedService1);
            }
            if (inService2 == false){
              timeStartedService2 = current_time;
              nodeInService2 = StartService(elementQ, 2, timeStartedService2);
            }
          }
        }

        previousTime = current_time;
        if (elementQ->eventList->head != NULL){
          current_time = elementQ->eventList->head->event_time;
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

        // If departure
        printf("Last Node Time: %f\n", current_time);
        if (elementQ->eventList->head->event_type == 3){
          if (nodeInService1 == elementQ->eventList->head->qnode){
            if (current_time == elementQ->eventList->head->event_time){
              ProcessDeparture(elementQ, nodeInService1, 1);
              nodeInService1 = NULL;
            }
          }
          if (nodeInService2 == elementQ->eventList->head->qnode){
            if (current_time == elementQ->eventList->head->event_time){
              ProcessDeparture(elementQ, nodeInService2, 2);
              nodeInService2 = NULL;
            }
          }
        }

        // If start service
        else if (elementQ->eventList->head->event_type == 2){
          if (inService1 == false || inService2 == false){
            if (inService1 == false){
              timeStartedService1 = current_time;
              nodeInService1 = StartService(elementQ, 1, timeStartedService1);
            }
            if (inService2 == false){
              timeStartedService2 = current_time;
              nodeInService2 = StartService(elementQ, 2, timeStartedService2);
            }
          }
        } 
        
        // If add arrival
        else {  // elementQ->eventList->head->event_type == 1
          if (nextNodeToAddQueue != NULL){
            if (current_time == nextNodeToAddQueue->arrival_time){
              nextNodeToAddQueue = ProcessArrival(elementQ, currentNode);
            }
          }
        }

        previousTime = current_time;
        current_time = elementQ->eventList->head->event_time;
        currentNode = currentNode->next;
        addToCumulativeNumber(elementQ);  // Updates cumulative_number total 
        continue;      
      }


      // Normal Execution
      printf("Normal Time: %f\n", current_time);
      if (elementQ->eventList->head->event_type == 3){
        printf("  Entered Departure\n");
        if (nodeInService1 == elementQ->eventList->head->qnode){
          if (current_time == elementQ->eventList->head->event_time){
            printf("    Left Server 1\n");
            ProcessDeparture(elementQ, nodeInService1, 1);
            nodeInService1 = NULL;
          }
        }
        else if (nodeInService2 == elementQ->eventList->head->qnode){
          if (current_time == elementQ->eventList->head->event_time){
            printf("    Left Server 2\n");
            ProcessDeparture(elementQ, nodeInService2, 2);
            nodeInService2 = NULL;
          }
        }

        previousTime = current_time;
        current_time = elementQ->eventList->head->event_time;
        addToCumulativeNumber(elementQ);

        continue;
      }

      else if (elementQ->eventList->head->event_type == 2){
        printf("  Entered Start Service\n");
        if (inService1 == false || inService2 == false){
          if (inService1 == false){
            printf("    Used Server 1\n");
            timeStartedService1 = current_time;
            nodeInService1 = StartService(elementQ, 1, timeStartedService1);
          }
          else if (inService2 == false){
            printf("    Used Server 2\n");
            timeStartedService2 = current_time;
            nodeInService2 = StartService(elementQ, 2, timeStartedService2);
          }
        }

        previousTime = current_time;
        current_time = elementQ->eventList->head->event_time;
        addToCumulativeNumber(elementQ);

        continue;
      }

      // arrrival of new node into waitlist
      else {
        printf("  Entered Add Arrival\n");
        if (nextNodeToAddQueue != NULL){
          printf("    Went Through Not NULL\n");
          if (current_time == nextNodeToAddQueue->arrival_time){
            printf("    Added Arrival\n");
            nextNodeToAddQueue = ProcessArrival(elementQ, currentNode);
          }
        }
      }

      previousTime = current_time;
      current_time = elementQ->eventList->head->event_time;
      addToCumulativeNumber(elementQ);
          
      if (currentNode != NULL)            // While theres still another node next to it, change current to it
        currentNode = currentNode->next;
      if (nextNode != NULL)               // Same thing for nextNode
        nextNode = nextNode->next;
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

    struct QueueNode* test = elementQ->head;
    while (test != NULL){
      printf("[(%f), (%f)]\n", test->arrival_time, test->service_time);
      test = test->next;
    }

    Simulation(elementQ, lambda, mu, print_period, total_departures);
    FreeQueue(elementQ);
    free(elementQ);
	}
	else printf("Insufficient number of arguments provided!\n");
   
	return 0;
}
