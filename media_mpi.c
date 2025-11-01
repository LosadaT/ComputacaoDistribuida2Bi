//Francisco Losada Totaro - 10364673
//Pedro Henrique L. Moreiras - 10441998
//Compilar - mpirun -np 4 ./media_mpi 1000     
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (argc != 2) {
        if (rank == 0) fprintf(stderr, "Uso: %s N\n  N = tamanho do vetor local por processo\n", argv[0]);
        MPI_Finalize();
        return EXIT_FAILURE;
    }

    long N = atol(argv[1]);
    if (N <= 0) {
        if (rank == 0) fprintf(stderr, "N deve ser um inteiro positivo.\n");
        MPI_Finalize();
        return EXIT_FAILURE;
    }

    /* allocate local vector */
    double *local_vec = (double*) malloc(sizeof(double) * N);
    if (!local_vec) {
        fprintf(stderr, "Processo %d: erro ao alocar vetor de tamanho %ld\n", rank, N);
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }

    /* seed random generator differently for each rank */
    srand((unsigned int)(time(NULL) + rank));

    double local_sum = 0.0;
    for (long i = 0; i < N; ++i) {
        local_vec[i] = rand() / (double)RAND_MAX; /* in [0,1] */
        local_sum += local_vec[i];
    }
    double local_mean = local_sum / (double)N;

    /* Reduce to compute global sum at rank 0 */
    double global_sum = 0.0;
    MPI_Reduce(&local_sum, &global_sum, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

    /* Gather local sums and means to rank 0 for ordered printing */
    double *all_sums = NULL;
    double *all_means = NULL;
    if (rank == 0) {
        all_sums = (double*) malloc(sizeof(double) * size);
        all_means = (double*) malloc(sizeof(double) * size);
        if (!all_sums || !all_means) {
            fprintf(stderr, "Erro ao alocar arrays de coleta no rank 0\n");
            MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }
    }

    MPI_Gather(&local_sum, 1, MPI_DOUBLE, all_sums, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    MPI_Gather(&local_mean, 1, MPI_DOUBLE, all_means, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        for (int r = 0; r < size; ++r) {
            printf("[Processo %d] Soma local: %.3f, Média local: %.4f\n", r, all_sums[r], all_means[r]);
        }

        double global_mean = global_sum / (double)(N * size);
        printf("\n[Soma global] %.3f\n", global_sum);
        printf("[Média global] %.4f\n", global_mean);

        free(all_sums);
        free(all_means);
    }

    free(local_vec);

    MPI_Finalize();
    return EXIT_SUCCESS;
}
