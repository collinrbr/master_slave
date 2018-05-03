#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <mpi.h>
#include <time.h>

int DEBUG = 1;
int size;
MPI_Datatype mpi_struct;

typedef struct montcar{
  int hits; 
  double out; //best guess at pi
  int num_sims;
} mntcr;

void commit_struct()
{
	const int num_elements=3;
	int element_lens[3] = {1,1,1};
	MPI_Datatype types[3] = {MPI_INT, MPI_DOUBLE, MPI_INT};
	MPI_Aint offsets[3];

	offsets[0] = offsetof(mntcr, hits);
	offsets[1] = offsetof(mntcr, out);
	offsets[2] = offsetof(mntcr, num_sims);

	MPI_Type_create_struct(num_elements, element_lens, offsets, types, &mpi_struct);
	MPI_Type_commit(&mpi_struct);
}

void send_home(int rank){
	int tag = 0;

	if(DEBUG == 1)
	{
		printf("Rank %d is sending everyone home.\n", rank);
	}

	for(int j = 1; j < size; j++)
	{
		int cmd = 1;
		int size = 1;
		int dest = j;
		MPI_Send(&cmd, size, MPI_INT, dest, tag, MPI_COMM_WORLD);
	}
}


//takes some structs as input and runs them
//structs should hold variables to store output
//note: structs may not be returned in the order they are given
void run(void* structs, int n)
{
	if(DEBUG == 1) printf("Starting run with %d structs\n", n);

	int cmd = 3;

	int tasks_completed = 0;
	int task_index = 0;
	//give everyone their first task
	for(int i = 0; i < size-1 && i < n; i++)
	{
		struct montcar strc = *(((struct montcar*) structs) + task_index);

		MPI_Send(&cmd, 1, MPI_INT, i+1, 0, MPI_COMM_WORLD); //send command number
		if (DEBUG == 1) printf("Sending struct %d with command %d\n", i, cmd);
		MPI_Send(&strc, 1, mpi_struct, i+1, 0, MPI_COMM_WORLD); //send struct

		task_index++;
	}


	MPI_Status status;
	//wait for responses and give next task while there are still more tasks to give
	while(task_index < n)
	{

		//MPI_Recv(&task, 1, MPI_INT, src, tag, MPI_COMM_WORLD, &status);
		MPI_Recv((((struct montcar*) structs) + tasks_completed), 1, mpi_struct, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
		if(DEBUG == 1) printf("Got a result! Sending another! task_index: %d tasks_completed: %d \n", task_index, tasks_completed);
		if(DEBUG == 1) printf("From this result, we got PI EST: %f\n", (((struct montcar*) structs) + tasks_completed)->out);
		tasks_completed++;



		struct montcar strc = *(((struct montcar*) structs) + task_index);

		MPI_Send(&cmd, 1, MPI_INT, status.MPI_SOURCE, 0, MPI_COMM_WORLD); //send command number
		MPI_Send(&strc, 1, mpi_struct, status.MPI_SOURCE, 0, MPI_COMM_WORLD);
		task_index++;
	}

	//wait for responses
	while(tasks_completed < n) 
	{
		MPI_Recv((((struct montcar*) structs) + tasks_completed), 1, mpi_struct, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
		if(DEBUG == 1) printf("Got a results! task_index: %d tasks_completed: %d\n", task_index, tasks_completed);
		if(DEBUG == 1) printf("From this result, we got PI EST: %f\n", (((struct montcar*) structs) + tasks_completed)->out);
		tasks_completed++;

		
	}
}

//prepares structs to be ran
void start_run(int rank)
{
	int num_tests = (size-1)*40;

	if (DEBUG == 1) printf("Starting run with size %d\n", size);
	struct montcar* tests = malloc(sizeof(struct montcar)*num_tests);

	for(int i = 0; i < num_tests; i++)
	{
		tests[i].hits = 0;
		tests[i].out = 0;
		tests[i].num_sims = 1000000000;
	}

	run((void *) tests, num_tests);

	//average pi (assume all have the same num_sims)
	double average = 0;
	for(int i = 0; i < num_tests; i++)
	{
		average = average + (((struct montcar*) tests) + i)->out;
		if(DEBUG == 1) printf("Result for test[%d]: PI est=%f\n", i, tests[i].out);
	}
	average = average / ((double) num_tests);

	printf("\n\n\nPi estimate: %f\n\n\n", average);

	free(tests);

	send_home(rank);
}


//Waits for task from src
void select_task(int rank, int src)
{
	int tag = 0;

	MPI_Status status;

	//Recieve task you want to do
	int task = 0;
	MPI_Recv(&task, 1, MPI_INT, src, tag, MPI_COMM_WORLD, &status);

	if (DEBUG == 1) printf("Rank %d got task %d\n", rank, task);

	switch(task)
	{
		//Go home
		case 1:
			if(DEBUG == 1)
			{
				printf("Rank %d is going home\n", rank);
			}
			return;
			break;

		//Test communication
		case 2:
			printf("Rank %d was told to test communication by %d\n", rank, src);
			break;

		//Run montcar simulation
		case 3:
		{
			struct montcar strc = (struct montcar) {0.0,0,0};
			
			MPI_Recv(&strc, 1, mpi_struct, src, tag, MPI_COMM_WORLD, &status);

			srand(time(NULL));

			printf("Running a m.c. sim!\n");

			strc.hits = 0;
			for(int i = 0; i < strc.num_sims; i++)
			{
				double x = ((double) rand()) / ((double) RAND_MAX);
				double y = ((double) rand()) / ((double) RAND_MAX);

				if(x*x+y*y <= 1)
				{
					strc.hits++;
				}
			}

			double pi = 4.0 * (((double)strc.hits) /((double) strc.num_sims)); //for some reason this is necessary
			strc.out = pi;

			if (DEBUG == 1) printf("For this sim: Num hits: %d => Pi est: %f\n", strc.hits, pi);

			MPI_Send(&strc, 1, mpi_struct, src, 0, MPI_COMM_WORLD); //send struct

			break;
		}

	}

	select_task(rank, src);
}


void assign_task(int rank, int num_cores) //num_cores includes master core
{
	MPI_Status status;
	if(DEBUG == 1)
	{
		printf("Assign_task was called with: rank: %d num_cores: %d\n", rank, num_cores);
	}

	int tag = 0;

	for(int i = 0; i < 10; i++)
	{
		for(int j = 1; j < num_cores; j++)
		{
			int cmd = 2;
			int size = 1;
			int dest = j;
			MPI_Send(&cmd, size, MPI_INT, dest, tag, MPI_COMM_WORLD);
		}
	}

	if(DEBUG == 1)
	{
		printf("Rank %d is going home.\n", rank);
	}
}

void go_to_work(int rank, int master_rank)
{
	if(rank == 0) //Rank 0 should be master node by default
	{
		//assign_task(rank, size);
		start_run(rank);
	}
	else
	{
		if(DEBUG == 1) printf("Rank %d is going to wait for a task!\n", rank);
		select_task(rank, 0);
	}

}


int main(int argc, char ** argv)
{
	int ierr;
    ierr = MPI_Init (&argc, &argv);

    int rank;
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    commit_struct();

    printf("Rank %d has been inititated.\n", rank);

    go_to_work(rank, 0);

    MPI_Finalize();
    return 0;
}