//Francisco Losada Totaro - 10364673
//Pedro Henrique L. Moreiras - 10441998
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

#define DATA_SIZE 100
#define REQUIRED_PROCS 5
#define CHUNK_SIZE (DATA_SIZE / REQUIRED_PROCS)

int main(int argc, char *argv[]) {
    int rank, size;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (size != REQUIRED_PROCS) {
        if (rank == 0) {
            fprintf(stderr, "Este programa deve ser executado com exatamente %d processos (foi fornecido %d)\n", REQUIRED_PROCS, size);
        }
        MPI_Abort(MPI_COMM_WORLD, 1);
        return 1;
    }

    int *data = NULL;
    int *result = NULL;

    if (rank == 0) {
        data = (int *)malloc(sizeof(int) * DATA_SIZE);
        result = (int *)malloc(sizeof(int) * DATA_SIZE);
        if (!data || !result) {
            fprintf(stderr, "Erro ao alocar memória no processo 0\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        for (int i = 0; i < DATA_SIZE; ++i) {
            data[i] = i + 1;
        }

        printf("[Processo %d] Vetor original: [", rank);
        for (int i = 0; i < DATA_SIZE; ++i) {
            printf("%d", data[i]);
            if (i != DATA_SIZE - 1) printf(", ");
        }
        printf("]\n");
        fflush(stdout);
    }

    int local_buf[CHUNK_SIZE];

    MPI_Barrier(MPI_COMM_WORLD);
    double t_start = MPI_Wtime();

    MPI_Scatter(data, CHUNK_SIZE, MPI_INT, local_buf, CHUNK_SIZE, MPI_INT, 0, MPI_COMM_WORLD);

    for (int i = 0; i < CHUNK_SIZE; ++i) {
        int x = local_buf[i];
        local_buf[i] = x * x;
    }

    MPI_Gather(local_buf, CHUNK_SIZE, MPI_INT, result, CHUNK_SIZE, MPI_INT, 0, MPI_COMM_WORLD);

    MPI_Barrier(MPI_COMM_WORLD);
    double t_end = MPI_Wtime();
    double elapsed = t_end - t_start;
    if (rank == 0) {
        printf("[Processo %d] Tempo total (s): %f\n", rank, elapsed);
    }

    if (rank == 0) {
        printf("[Processo %d] Vetor transformado: [", rank);
        for (int i = 0; i < DATA_SIZE; ++i) {
            printf("%d", result[i]);
            if (i != DATA_SIZE - 1) printf(", ");
        }
        printf("]\n");
        free(data);
        free(result);
    }
    MPI_Finalize();
    return 0;
}
