/************************************************************\
 * Copyright 2024 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

/* vcpu.c -- a test to make sure physical cores are not oversubscribed
 * in an MPI job.
 *
 * PASS: each integer returned by sched_getcpu() is unique within a
 * node.
 * 
 * FAIL: one or more integers returned by sched_getcpu() are the same
 * within a single node.
 */

#define _GNU_SOURCE

#include <mpi.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <sched.h>

#define MASTER 0

int check_uniqueness (int *array, int size)
{
    /* This is an O(nlogn) solution for checking the uniqueness of an array that
     * should only be run on one MPI rank (MASTER) per node.
     */
    for (int i = 0; i < size; ++i) {
        for (int j = i + 1; j < size; ++j) {
            if (array[i] == array[j]) {
                return -1;
            }
        }
    }
    return 0;
}

int main (int argc, char *argv[])
{
    int numtasks, len, globalRank, localRank, cpu, numlocaltasks;
    char hostname[MPI_MAX_PROCESSOR_NAME];

    MPI_Init (&argc, &argv);

    /* Split the communication into global communication and local
     * communication. The local communication should be per node.
     * https://stackoverflow.com/questions/35626377/get-nodes-with-mpi-program-in-c
     */
    MPI_Comm nodeComm, masterComm;

    MPI_Comm_rank (MPI_COMM_WORLD, &globalRank);
    MPI_Comm_size (MPI_COMM_WORLD, &numtasks);

    MPI_Comm_split_type (MPI_COMM_WORLD,
                         MPI_COMM_TYPE_SHARED,
                         globalRank,
                         MPI_INFO_NULL,
                         &nodeComm);

    /* Get the local rank from the node communicator.
     */
    MPI_Comm_rank (nodeComm, &localRank);

    MPI_Comm_split (MPI_COMM_WORLD, localRank, globalRank, &masterComm);
    MPI_Get_processor_name (hostname, &len);

    /* Fetch the real core number in each rank and print it, along with
     * some other information.
     */
    cpu = sched_getcpu ();
    printf ("Hello from local rank %d (global rank %d) on %s vcpu %d\n",
            localRank,
            globalRank,
            hostname,
            cpu);

    /* Wait until all nodes have fetched their rank, hostname and cpu
     * number. Otherwise they could put null data into the array.
     * Initialize the array on each rank so that the MPI_Allgather
     * can send all the data to each rank. This effectively helps us
     * test point-to-point communication as well as all-to-all communication.
     */
    MPI_Barrier (nodeComm);

    MPI_Comm_size (nodeComm, &numlocaltasks);
    int *per_node_cpus = malloc (sizeof (int) * numlocaltasks);

    /* Have each MPI rank report its core number on each node separately.
     * Gather them all on each node in an array, then have one rank on the
     * machine check their uniqueness. Call MPI_Abort both locally and globally
     * to report an error if there are multiple ranks on the same core.
     */
    MPI_Allgather (&cpu, 1, MPI_INT, per_node_cpus, 1, MPI_INT, nodeComm);
    MPI_Barrier (MPI_COMM_WORLD);

    if (localRank == MASTER) {
        if (check_uniqueness (per_node_cpus, numlocaltasks) != 0) {
            MPI_Abort (nodeComm, -1);
            MPI_Abort (MPI_COMM_WORLD, -1);
        }
    }

    /* Memory and communicator cleanup.
     */
    MPI_Comm_free (&nodeComm);
    MPI_Comm_free (&masterComm);
    free (per_node_cpus);

    MPI_Finalize ();

    return 0;
}
