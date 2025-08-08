#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>

#define QUEUE_SIZE 10
#define CLOCK_SIZE 3

#define RED     "\033[0;31m"
#define GREEN   "\033[0;32m"
#define YELLOW  "\033[1;33m"
#define BLUE    "\033[0;34m"
#define MAGENTA "\033[0;35m"
#define CYAN    "\033[0;36m"
#define RESET   "\033[0m"

typedef struct {
    int vector[CLOCK_SIZE];
} VectorClock;

typedef struct {
    int sender;
    int receiver;
    int value;
    VectorClock clock;
} Message;

Message queue[QUEUE_SIZE];
int front = 0, rear = 0, count = 0;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_not_full = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_not_empty = PTHREAD_COND_INITIALIZER;

void enqueue(Message msg) {
    if (count == QUEUE_SIZE) {
        printf(MAGENTA "[Fila] Fila cheia!\n" RESET);
        return;
    }
    queue[rear] = msg;
    rear = (rear + 1) % QUEUE_SIZE;
    count++;
}

Message dequeue() {
    if (count == 0) {
        printf(MAGENTA "[Fila] Fila vazia!\n" RESET);
        Message empty_msg = {0};
        return empty_msg;
    }
    Message msg = queue[front];
    front = (front + 1) % QUEUE_SIZE;
    count--;
    return msg;
}

void print_clock(VectorClock clk, int rank) {
    printf("[Rank %d] Clock: [", rank);
    for (int i = 0; i < CLOCK_SIZE; i++) {
        printf("%d", clk.vector[i]);
        if (i < CLOCK_SIZE - 1) printf(", ");
    }
    printf("]\n");
}

void* thread_entrada(void* arg) {
    int rank = *(int*)arg;
    while (1) {
        sleep(1);

        pthread_mutex_lock(&mutex);

        if (count > 0) {
            Message msg = dequeue();
            if (msg.receiver == rank) {
                printf(CYAN "[Thread Entrada %d] Recebida mensagem de %d com valor %d\n" RESET, rank, msg.sender, msg.value);

                // Passar para relógio
                pthread_mutex_unlock(&mutex);
                pthread_mutex_lock(&mutex);

                printf(YELLOW "[Thread Relógio %d] Processada mensagem valor %d\n" RESET, rank, msg.value);
                for (int i = 0; i < CLOCK_SIZE; i++) {
                    if (i != rank)
                        msg.clock.vector[i] = (msg.clock.vector[i] > i ? msg.clock.vector[i] : i);
                }
                msg.clock.vector[rank]++;

                print_clock(msg.clock, rank);

                pthread_mutex_unlock(&mutex);

                // Enviar para próximo
                pthread_mutex_lock(&mutex);
                int next = (rank + 1) % CLOCK_SIZE;
                msg.sender = rank;
                msg.receiver = next;
                enqueue(msg);
                printf(GREEN "[Thread Saída %d] Enviada mensagem para %d com valor %d\n" RESET, rank, next, msg.value);
                pthread_mutex_unlock(&mutex);
            } else {
                enqueue(msg); // Reenfileira se não for para o rank atual
            }
        }

        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}

int main() {
    pthread_t threads[CLOCK_SIZE];
    int ranks[CLOCK_SIZE] = {0, 1, 2};

    // Inicializar fila com primeira mensagem
    VectorClock initial_clock = {{1, 0, 0}};
    Message initial_msg = {0, 1, 100, initial_clock};
    enqueue(initial_msg);

    for (int i = 0; i < CLOCK_SIZE; i++) {
        pthread_create(&threads[i], NULL, thread_entrada, &ranks[i]);
    }

    for (int i = 0; i < CLOCK_SIZE; i++) {
        pthread_join(threads[i], NULL);
    }

    return 0;
}
