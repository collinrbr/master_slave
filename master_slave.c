#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <mpi.h>

int DEBUG = 0;
int size;
MPI_Datatype mpi_struct;

typedef struct sample{
  double in;
  double out;
  int index;
} smpl;

void commit_struct()
{
	const int num_elements=3;
	int element_lens[3] = {1,1,1};
	MPI_Datatype types[3] = {MPI_DOUBLE, MPI_DOUBLE, MPI_INT};
	MPI_Aint offsets[3];

	offsets[0] = offsetof(smpl, in);
	offsets[1] = offsetof(smpl, out);
	offsets[2] = offsetof(smpl, index);

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
	int cmd = 3;

	int tasks_completed = 0;
	int task_index = 0;
	//give everyone their first task
	for(int i = 1; i < size && i < n; i++)
	{
		struct sample strc = *(((struct sample*) structs) + task_index);

		MPI_Send(&cmd, 1, MPI_INT, i, 0, MPI_COMM_WORLD); //send command number
		printf("Sending struct\n");
		MPI_Send(&strc, 1, mpi_struct, i, 0, MPI_COMM_WORLD); //send struct

		task_index++;
	}


	MPI_Status status;
	//wait for responses and give next task while there are still more tasks to give
	while(task_index < n)
	{

		//MPI_Recv(&task, 1, MPI_INT, src, tag, MPI_COMM_WORLD, &status);
		MPI_Recv((((struct sample*) structs) + tasks_completed), 1, mpi_struct, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
		tasks_completed++;

		struct sample strc = *(((struct sample*) structs) + task_index);

		MPI_Send(&cmd, 1, MPI_INT, status.MPI_SOURCE, 0, MPI_COMM_WORLD); //send command number
		MPI_Send(&strc, 1, mpi_struct, status.MPI_SOURCE, 0, MPI_COMM_WORLD);

		task_index++;
	}

	//wait for responses
	while(tasks_completed < n) 
	{
		MPI_Recv((((struct sample*) structs) + tasks_completed), 1, mpi_struct, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
		tasks_completed++;
	}
}

//prepares structs to be ran
void start_run(int rank)
{
	struct sample* tests = malloc(sizeof(struct sample)*100);

	printf("before==============================================\n");
	for(int i = 0; i < 100; i++)
	{
		tests[i].in = i;
		tests[i].out = 0;
		tests[i].index = i;

		printf("in: %f out: %f index: %d\n", tests[i].in, tests[i].out, tests[i].index);
	}

	run((void *) tests, 100);

	printf("after==============================================\n");
	for(int i = 0; i < 100; i++)
	{
		printf("in: %f out: %f index: %d\n", tests[i].in, tests[i].out, tests[i].index);
	}

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

		//Run sample simulation
		case 3:
		{
			struct sample strc = (struct sample) {0,0,0};
			
			MPI_Recv(&strc, 1, mpi_struct, src, tag, MPI_COMM_WORLD, &status);

			//Test: out = 1000 + in
			strc.out = strc.in + 1000;
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