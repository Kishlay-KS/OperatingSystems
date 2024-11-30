#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <wait.h>

key_t sem_key[5];
key_t shm_key[5];
key_t shared_table;

int sem_id[5];
int shm_id[5];
int shared_table_id;

struct Table
{
    char name[50];
    int pid[10];
};

// Function to perform semaphore wait
int sem_wait(int sem_id)
{
    struct sembuf sops = {0, -1, 0};

    if (semop(sem_id, &sops, 1) == -1)
    {
        perror("sem_wait failed");
        return -1;
    }

    return 0;
}

// Function to perform semaphore signal
int sem_signal(int sem_id)
{
    struct sembuf sops = {0, 1, 0};

    if (semop(sem_id, &sops, 1) == -1)
    {
        perror("sem_signal failed");
        return -1;
    }

    return 0;
}

int helper(int idx, int round)
{
    // Define which tables this helper can access
    int left_table = (idx + round) % 5;
    int right_table = (idx + round + 1) % 5;
    if (idx % 2)
    {
        sem_wait(left_table);
        sem_wait(right_table);
    }
    else
    {
        sem_wait(right_table);
        sem_wait(left_table);
    }

    printf("Transaction %d holding the semaphores.\n", getpid());

    int temp = left_table;
    struct Table *table1 = (struct Table *)shmat(shm_id[temp], NULL, 0);
    for (int i = 0; i < 10; i++)
    {
        if (table1->pid[i] == -1)
        {
            table1->pid[i] = getpid();
            break;
        }
    }

    temp = right_table;
    struct Table *table2 = (struct Table *)shmat(shm_id[temp], NULL, 0);
    for (int i = 0; i < 10; i++)
    {
        if (table2->pid[i] == -1)
        {
            table2->pid[i] = getpid();
            break;
        }
    }

    printf("Transaction %d: operating on %s - %s.\n", getpid(), table1->name, table2->name);
    sleep(10);

    // Increment counter
    int *counter = (int *)shmat(shared_table_id, NULL, 0);
    (*counter)++;

    // Release semaphores
    printf("Transaction %d released the semaphores.\n", getpid());
    sem_signal(left_table);
    sem_signal(right_table);

    shmdt(table1);
    shmdt(table2);

    sleep(2);

    return 0;
}

int main()
{

    for (int i = 0; i < 5; i++)
    {
        sem_key[i] = ftok("week9.c", i);
    }

    for (int i = 0; i < 5; i++)
    {
        sem_id[i] = semget(sem_key[i], 1, IPC_CREAT | 0666);
        semctl(sem_id[i], 0, SETVAL, 1);
    }

    for (int i = 0; i < 5; i++)
    {
        shm_key[i] = ftok("week9.c", i + 5);
    }

    shared_table = ftok("week9.c", 20);
    shared_table_id = shmget(shared_table, sizeof(int), IPC_CREAT | 0666);
    int *counter = (int *)shmat(shared_table_id, NULL, 0);

    *counter = 0;

    // Create and attach shared memory
    for (int i = 0; i < 5; i++)
    {
        shm_id[i] = shmget(shm_key[i], sizeof(struct Table), IPC_CREAT | 0666);
        struct Table *table = (struct Table *)shmat(shm_id[i], NULL, 0);

        // Initialize the table with table number
        snprintf(table->name, sizeof(table->name), "Table initialised with number %d", i + 1);
        for (int j = 0; j < 10; j++)
        {
            table->pid[j] = -1;
        }

        shmdt(table);
    }

    for (int i = 0; i < 5; i++)
    {
        pid_t pid = fork();
        if (pid == 0)
        {
            for (int j = 0; j < 5; j++)
            {
                helper(i, j);
                int *counter = (int *)shmat(shared_table_id, NULL, 0);
                while (*counter < 5 * (j + 1))
                    ;
            }
            exit(1);
        }
    }

    for (int i = 0; i < 5; i++)
    {
        wait(NULL);
    }

    for (int i = 0; i < 5; i++)
    {
        struct Table *table = (struct Table *)shmat(shm_id[i], NULL, 0);
        printf("\nTable name is %s:\n", table->name);
        printf("\nTable pid is :\n");
        for (int j = 0; j < 10; j++)
        {
            printf("%d ", table->pid[j]);
        }
        printf("\n");

        shmdt(table);
    }

    for (int i = 0; i < 5; i++)
    {
        shmctl(shm_id[i], IPC_RMID, NULL);
    }

    shmdt(counter);
    shmctl(shared_table_id, IPC_RMID, NULL);

    return 0;
}