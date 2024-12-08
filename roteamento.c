#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <mpi.h>

#define INF INT_MAX // Valor que representa infinito

// Função para ler o grafo do arquivo configGrafo.txt
void leGrafo(const char *nomeArquivo, int tabela[][7], int *numNos) {
    FILE *arquivo = fopen(nomeArquivo, "r");
    if (!arquivo) {
        perror("Erro ao abrir arquivo de configuração do grafo");
        exit(EXIT_FAILURE);
    }

    fscanf(arquivo, "%d", numNos); // Lê o número de nós
    for (int i = 0; i < *numNos; i++) {
        for (int j = 0; j < *numNos; j++) {
            char valor[16];
            fscanf(arquivo, "%s", valor);
            if (strcmp(valor, "-") == 0) {
                tabela[i][j] = INF; // Define como infinito
            } else {
                tabela[i][j] = atoi(valor); // Converte para inteiro
            }
        }
    }

    fclose(arquivo);
}

// Função para imprimir a tabela de roteamento
void imprimeTabela(int tabela[][7], int numNos, const char *nomeArquivo) {
    FILE *arquivo = fopen(nomeArquivo, "w");
    if (!arquivo) {
        perror("Erro ao abrir arquivo de saída");
        exit(EXIT_FAILURE);
    }

    fprintf(arquivo, "Tabela de roteamento final:\n");
    for (int i = 0; i < numNos; i++) {
        fprintf(arquivo, "Nó %c: ", 'A' + i);
        for (int j = 0; j < numNos; j++) {
            if (tabela[i][j] == INF) {
                fprintf(arquivo, "- ");
            } else {
                fprintf(arquivo, "%d ", tabela[i][j]);
            }
        }
        fprintf(arquivo, "\n");
    }

    fclose(arquivo);
}

// Função para calcular a tabela de roteamento com algoritmo de vetor de distância
void calculaRoteamento(int minhaLinha[], int numNos, int rank, MPI_Comm comm) {
    int tabelaLocal[numNos];
    memcpy(tabelaLocal, minhaLinha, sizeof(int) * numNos); // Copia inicial da linha

    for (int step = 0; step < numNos - 1; step++) {
        for (int vizinho = 0; vizinho < numNos; vizinho++) {
            if (vizinho != rank && minhaLinha[vizinho] != INF) {
                int linhaVizinho[numNos];
                MPI_Sendrecv(tabelaLocal, numNos, MPI_INT, vizinho, 0,
                             linhaVizinho, numNos, MPI_INT, vizinho, 0, comm, MPI_STATUS_IGNORE);

                // Atualizar os custos com base nas tabelas recebidas
                for (int j = 0; j < numNos; j++) {
                    if (linhaVizinho[j] != INF && tabelaLocal[j] > minhaLinha[vizinho] + linhaVizinho[j]) {
                        tabelaLocal[j] = minhaLinha[vizinho] + linhaVizinho[j];
                    }
                }
            }
        }
    }

    memcpy(minhaLinha, tabelaLocal, sizeof(int) * numNos); // Atualiza a linha final
}

int main(int argc, char *argv[]) {
    int rank, size, numNos;
    int tabela[7][7] = {0}; // Suporte para até 7 nós

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (rank == 0) {
        leGrafo("configGrafo.txt", tabela, &numNos);
        if (size != numNos) {
            fprintf(stderr, "Erro: Número de processos (%d) deve ser igual ao número de nós (%d).\n", size, numNos);
            MPI_Finalize();
            return EXIT_FAILURE;
        }
    }

    MPI_Bcast(&numNos, 1, MPI_INT, 0, MPI_COMM_WORLD); // Broadcast do número de nós
    MPI_Bcast(tabela, 7 * 7, MPI_INT, 0, MPI_COMM_WORLD); // Broadcast da tabela inicial

    int minhaLinha[7];
    MPI_Scatter(tabela, 7, MPI_INT, minhaLinha, 7, MPI_INT, 0, MPI_COMM_WORLD); // Distribui linhas para processos

    calculaRoteamento(minhaLinha, numNos, rank, MPI_COMM_WORLD); // Calcula a tabela de roteamento

    MPI_Gather(minhaLinha, 7, MPI_INT, tabela, 7, MPI_INT, 0, MPI_COMM_WORLD); // Coleta as linhas no processo 0

    if (rank == 0) {
        imprimeTabela(tabela, numNos, "resposta.txt");
    }

    MPI_Finalize();
    return 0;
}
