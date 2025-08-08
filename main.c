#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <mpi.h>

#define QUEUE_SIZE 5
#define CLOCK_SIZE 3

typedef struct {
    int vector[CLOCK_SIZE];
} VectorClock;

typedef struct {
    int sender;
    int receiver;
    int value;
    VectorClock clock;
} Message;

typedef struct {
    Message messages[QUEUE_SIZE];
    int front, rear, count;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} MessageQueue;

void mq_init(MessageQueue* q) {
    q->front = q->rear = q->count = 0;
    pthread_mutex_init(&q->mutex, NULL);
    pthread_cond_init(&q->not_empty, NULL);
    pthread_cond_init(&q->not_full, NULL);
}

void mq_enqueue(MessageQueue* q, Message msg) {
    pthread_mutex_lock(&q->mutex);
    while (q->count == QUEUE_SIZE)
        pthread_cond_wait(&q->not_full, &q->mutex);

    q->messages[q->rear] = msg;
    q->rear = (q->rear + 1) % QUEUE_SIZE;
    q->count++;

    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->mutex);
}

Message mq_dequeue(MessageQueue* q) {
    pthread_mutex_lock(&q->mutex);
    while (q->count == 0)
        pthread_cond_wait(&q->not_empty, &q->mutex);

    Message msg = q->messages[q->front];
    q->front = (q->front + 1) % QUEUE_SIZE;
    q->count--;

    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->mutex);

    return msg;
}

MessageQueue entrada_relogio_queue;
MessageQueue relogio_saida_queue;

int rank_global;

void print_clock(VectorClock clk) {
    printf("[Clock: ");
    for (int i = 0; i < CLOCK_SIZE; i++) {
        printf("%d", clk.vector[i]);
        if (i < CLOCK_SIZE - 1) printf(", ");
    }
    printf("]");
}

void* thread_entrada(void* arg) {
    int rank = rank_global;
    int prev = (rank + CLOCK_SIZE - 1) % CLOCK_SIZE;
    MPI_Status status;

    while (1) {
        Message msg;
        MPI_Recv(&msg, sizeof(Message), MPI_BYTE, prev, 0, MPI_COMM_WORLD, &status);

        printf("[Entrada %d] Recebida mensagem de %d com valor %d ", rank, msg.sender, msg.value);
        print_clock(msg.clock);
        printf("\n");

        mq_enqueue(&entrada_relogio_queue, msg);
    }
    return NULL;
}

// alterar essa funcao para ficar igual a etapa um, no send ela manda para a saida e no recv ele recebe de qlqr um em qlqr ordem
void* thread_relogio(void* arg) {
    int rank = rank_global;

    while (1) {
        Message msg = mq_dequeue(&entrada_relogio_queue);

        for (int i = 0; i < CLOCK_SIZE; i++) {
            if (msg.clock.vector[i] < 0)
                msg.clock.vector[i] = 0;
        }
        msg.clock.vector[rank]++;

        printf("[Relógio %d] Atualizou mensagem com valor %d ", rank, msg.value);
        print_clock(msg.clock);
        printf("\n");

        msg.sender = rank;
        msg.receiver = (rank + 1) % CLOCK_SIZE;

        mq_enqueue(&relogio_saida_queue, msg);
    }
    return NULL;
}

void* thread_saida(void* arg) {
    int rank = rank_global;
    int next = (rank + 1) % CLOCK_SIZE;

    while (1) {
        Message msg = mq_dequeue(&relogio_saida_queue);

        printf("[Saída %d] Enviando mensagem para %d com valor %d ", rank, next, msg.value);
        print_clock(msg.clock);
        printf("\n");

        MPI_Send(&msg, sizeof(Message), MPI_BYTE, next, 0, MPI_COMM_WORLD);

        usleep(200000);
    }
    return NULL;
}

int main(int argc, char* argv[]) {
    MPI_Init(&argc, &argv);

    MPI_Comm_rank(MPI_COMM_WORLD, &rank_global);
    MPI_Comm_size(MPI_COMM_WORLD, &argc);

    if (argc != CLOCK_SIZE) {
        if (rank_global == 0) {
            printf("Execute o programa com %d processos.\n", CLOCK_SIZE);
        }
        MPI_Finalize();
        return 1;
    }

    mq_init(&entrada_relogio_queue);
    mq_init(&relogio_saida_queue);

    pthread_t t_entrada, t_relogio, t_saida;

    if (rank_global == 0) {
        Message initial_msg;
        initial_msg.sender = 0;
        initial_msg.receiver = 1;
        initial_msg.value = 1;
        for (int i = 0; i < CLOCK_SIZE; i++)
            initial_msg.clock.vector[i] = 0;
        initial_msg.clock.vector[0] = 1;

        MPI_Send(&initial_msg, sizeof(Message), MPI_BYTE, 1, 0, MPI_COMM_WORLD);
        printf("[Main %d] Enviada mensagem inicial para 1\n", rank_global);
    }

    pthread_create(&t_entrada, NULL, thread_entrada, NULL);
    pthread_create(&t_relogio, NULL, thread_relogio, NULL);
    pthread_create(&t_saida, NULL, thread_saida, NULL);

    pthread_join(t_entrada, NULL);
    pthread_join(t_relogio, NULL);
    pthread_join(t_saida, NULL);

    MPI_Finalize();
    return 0;
}
