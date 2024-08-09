#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MSG_LENGTH 100
#define MSG_INFO_ERR_LENGTH 200

// Error codes
#define ERR_GET_QUEUE_KEY  1
#define ERR_CREATE_QUEUE   2
#define ERR_SEND           3
#define ERR_RECV           4
#define ERR_OUTPUT_FILE    5
#define ERR_MSG_NOT_EXPECTED 6

#define MSG_CODE_IM_HERE 2
#define MSG_CODE_LIMITS 3
#define MSG_CODE_RESULTS 4
#define MSG_CODE_FIN 5

#define OUTPUT_FILE_NAME "primes.txt"
#define PRIME_COUNT_FILE_NAME "primeCount.txt"
#define PRIME_COUNT_WRITE_FREQUENCY 5

#define TIMER_INTERVAL 5

typedef struct {
    long msgType;
    char msgText[MSG_LENGTH];
} TMsgBuffer;

// Prototypes
int parseInteger(int pos, char *argv[]);
int isPrime(long int number);
void printProcessHierarchy(int rootPid, int serverPid, int *childPids, int numChildren);
void logMessage(char *text, int verbose);
long int countLines();
void alarmHandler(int signo);

// Globals
int totalTimeSeconds = 0;
int messageQueueId;

// Main
int main(int argc, char *argv[]) {
    int rootPid, serverPid, currentPid, parentPid, pid, childPid;
    int *childPids;
    int i, j, numChildren;
    int verbosity;
    int base, range, limit, searchRange;

    key_t keyQueue;
    TMsgBuffer message;

    time_t startTime, endTime;

    long int childInterval, childBase, childLimit;

    char info[MSG_INFO_ERR_LENGTH];

    int finishedChildren = 0;
    long int primesFound = 0;

    FILE *outputFile, *primeCountFile;

    if (argc != 5) {
        printf("Incorrect number of parameters.\n");
        return 1;
    } else {
        base = parseInteger(1, argv);
        range = parseInteger(2, argv);
        numChildren = parseInteger(3, argv);
        verbosity = parseInteger(4, argv);

        limit = base + range;
        searchRange = limit - base;

        rootPid = getpid();
        pid = fork();
        if (pid == 0) {
            serverPid = getpid();
            currentPid = serverPid;

            if ((keyQueue = ftok("/tmp", 'C')) == -1) {
                perror("Error creating System V IPC key");
                exit(ERR_GET_QUEUE_KEY);
            }

            if ((messageQueueId = msgget(keyQueue, 0777 | IPC_CREAT)) == -1) {
                perror("Failed to create System V IPC queue");
                exit(ERR_CREATE_QUEUE);
            }

            printf("Server: Message queue id = %u\n", messageQueueId);

            i = 0;
            while (i < numChildren) {
                if (pid > 0) {
                    pid = fork();
                    if (pid == 0) {
                        parentPid = getppid();
                        currentPid = getpid();
                    }
                }
                i++;
            }

            if (currentPid != serverPid) { // Calculator process
                message.msgType = MSG_CODE_IM_HERE;
                sprintf(message.msgText, "%d", currentPid);
                msgsnd(messageQueueId, &message, sizeof(message), IPC_NOWAIT);

                message.msgType = MSG_CODE_LIMITS;
                msgrcv(messageQueueId, &message, sizeof(message), MSG_CODE_LIMITS, 0);
                sscanf(message.msgText, "%ld %ld", &childBase, &childLimit);
                printf("Child: Received search interval [%ld, %ld)\n", childBase, childLimit);

                for (long int number = childBase; number < childLimit; number++) {
                    if (isPrime(number)) {
                        message.msgType = MSG_CODE_RESULTS;
                        sprintf(message.msgText, "%d %ld", currentPid, number);
                        msgsnd(messageQueueId, &message, sizeof(message), IPC_NOWAIT);
                    }
                }

                message.msgType = MSG_CODE_FIN;
                sprintf(message.msgText, "%d", currentPid);
                msgsnd(messageQueueId, &message, sizeof(message), IPC_NOWAIT);
            } else { // Server process
                childPids = (int *)malloc(numChildren * sizeof(int));
                printf("Children created, waiting to assign work...\n");

                for (j = 0; j < numChildren; j++) {
                    msgrcv(messageQueueId, &message, sizeof(message), MSG_CODE_IM_HERE, 0);
                    sscanf(message.msgText, "%d", &childPids[j]);
                }

                childInterval = searchRange / numChildren;

                message.msgType = MSG_CODE_LIMITS;
                for (j = 0; j < numChildren; j++) {
                    childBase = base + (childInterval * j);
                    childLimit = childBase + childInterval;

                    if (j == (numChildren - 1)) childLimit = limit + 1;

                    sprintf(message.msgText, "%ld %ld", childBase, childLimit);
                    msgsnd(messageQueueId, &message, sizeof(message), IPC_NOWAIT);
                    printf("Server: Sent limits to child %d, [%ld, %ld)\n", j, childBase, childLimit);
                }

                printProcessHierarchy(rootPid, serverPid, childPids, numChildren);

                if ((outputFile = fopen(OUTPUT_FILE_NAME, "w")) == NULL) {
                    perror("Error creating output file");
                    exit(ERR_OUTPUT_FILE);
                }

                printf("Output file %s created.\n", OUTPUT_FILE_NAME);
                time(&startTime);

                while (finishedChildren < numChildren) {
                    if (msgrcv(messageQueueId, &message, sizeof(message), 0, 0) == -1) {
                        perror("Server: msgrcv failed\n");
                        exit(ERR_RECV);
                    }

                    if (message.msgType == MSG_CODE_RESULTS) {
                        int primeNumber;
                        sscanf(message.msgText, "%d %d", &childPid, &primeNumber);
                        sprintf(info, "MSG %ld: %s\n", ++primesFound, message.msgText);
                        logMessage(info, verbosity);
                        fprintf(outputFile, "%d\n", primeNumber);

                        if (primesFound % PRIME_COUNT_WRITE_FREQUENCY == 0) {
                            primeCountFile = fopen(PRIME_COUNT_FILE_NAME, "w");
                            fprintf(primeCountFile, "%ld\n", primesFound);
                            fclose(primeCountFile);
                        }
                    } else if (message.msgType == MSG_CODE_FIN) {
                        finishedChildren++;
                        sprintf(info, "FIN %d %s\n", finishedChildren, message.msgText);
                        logMessage(info, verbosity);
                    } else {
                        perror("Server encountered an unexpected message\n");
                        exit(ERR_MSG_NOT_EXPECTED);
                    }
                }

                time(&endTime);
                double totalTime = difftime(endTime, startTime);
                printf("Server: Total computation time: %.2lf seconds.\n", totalTime);

                msgctl(messageQueueId, IPC_RMID, NULL);
                fflush(outputFile);
                fclose(outputFile);
                exit(0);
            }
        } else { // Root process
            alarm(TIMER_INTERVAL);
            signal(SIGALRM, alarmHandler);
            wait(NULL);
            printf("Result: %ld primes detected\n", countLines());
            exit(0);
        }
    }
}

int parseInteger(int pos, char *argv[]) {
    return strtol(argv[pos], NULL, 10);
}

int isPrime(long int number) {
    if (number <= 1) return 0;
    if (number == 4) return 0;

    for (int x = 2; x <= number / 2; x++) if (number % x == 0) return 0;
    
    return 1;
}

void printProcessHierarchy(int rootPid, int serverPid, int *childPids, int numChildren) {
    printf("\nROOT\tSERVER\tCALC\n");
    printf("%d\t%d\t%d\n", rootPid, serverPid, childPids[0]);

    for (int k = 1; k < numChildren; k++) printf("\t\t\t%d\n", childPids[k]);
    
    printf("\n");
}

void logMessage(char *text, int verbose) {
    if (verbose) printf("%s", text);
}

long int countLines() {
    long int count = 0;
    long int primeNumber;
    FILE *primeFile = fopen(OUTPUT_FILE_NAME, "r");

    while (fscanf(primeFile, "%ld", &primeNumber) != EOF) count++;
    
    fclose(primeFile);
    return count;
}

void alarmHandler(int signo) {
    FILE *primeCountFile;
    totalTimeSeconds += TIMER_INTERVAL;

    primeCountFile = fopen(PRIME_COUNT_FILE_NAME, "a");
    fprintf(primeCountFile, "Time: %d seconds. Calculating primes...\n", totalTimeSeconds);
    fclose(primeCountFile);

    alarm(TIMER_INTERVAL);
}
