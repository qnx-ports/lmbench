/*
 * lat_sem.c - semaphore test
 *
 * usage: lat_sem [-P <parallelism>] [-W <warmup>] [-N <repetitions>]
 *
 * Copyright (c) 2000 Carl Staelin.
 * Copyright (c) 1994 Larry McVoy.  Distributed under the FSF GPL with
 * additional restriction that results may published only if
 * (1) the benchmark is unmodified, and
 * (2) the version in the sccsid below is included in the report.
 * Support for this development by Sun Microsystems is gratefully acknowledged.
 */
char	*id = "$Id$\n";

#include "bench.h"
#ifdef __QNX__
#include <semaphore.h>
#include <sys/mman.h>
#else
#include <sys/sem.h>
#endif

void initialize(iter_t iterations, void *cookie);
void cleanup(iter_t iterations, void *cookie);
void doit(iter_t iterations, void *cookie);
#ifdef __QNX__
void writer(sem_t* sid);
#else
void writer(int sid);
#endif

typedef struct _state {
	int	pid;
#ifdef __QNX__
	sem_t* semid;
#else
	int	semid;
#endif
} state_t;

int 
main(int ac, char **av)
{
	state_t state;
	int parallel = 1;
	int warmup = 0;
	int repetitions = TRIES;
	int c;
	char* usage = "[-P <parallelism>] [-W <warmup>] [-N <repetitions>]\n";

	while (( c = getopt(ac, av, "P:W:N:")) != EOF) {
		switch(c) {
		case 'P':
			parallel = atoi(optarg);
			if (parallel <= 0) lmbench_usage(ac, av, usage);
			break;
		case 'W':
			warmup = atoi(optarg);
			break;
		case 'N':
			repetitions = atoi(optarg);
			break;
		default:
			lmbench_usage(ac, av, usage);
			break;
		}
	}
	if (optind < ac) {
		lmbench_usage(ac, av, usage);
	}

	state.pid = 0;

	benchmp(initialize, doit, cleanup, SHORT, parallel, 
		warmup, repetitions, &state);
	micro("Semaphore latency", get_n() * 2);
	return (0);
}

void 
initialize(iter_t iterations, void* cookie)
{
	char	c;
	state_t * state = (state_t *)cookie;

	if (iterations) return;

#ifdef __QNX__
	state->semid = (sem_t*)mmap(0, 2 * sizeof(sem_t), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANON, -1, 0);
	sem_init(&state->semid[0], 1, 0);
	sem_init(&state->semid[1], 1, 0);
#else
	state->semid = semget(IPC_PRIVATE, 2, IPC_CREAT | IPC_EXCL | 0600);
	semctl(state->semid, 0, SETVAL, 0);
	semctl(state->semid, 1, SETVAL, 0);
#endif

	handle_scheduler(benchmp_childid(), 0, 1);
	switch (state->pid = fork()) {
	    case 0:
		signal(SIGTERM, exit);
		handle_scheduler(benchmp_childid(), 1, 1);
		writer(state->semid);
		return;

	    case -1:
		perror("fork");
		return;

	    default:
		break;
	}
}

void 
cleanup(iter_t iterations, void* cookie)
{
	state_t * state = (state_t *)cookie;

	if (iterations) return;

	if (state->pid) {
		kill(state->pid, SIGKILL);
		waitpid(state->pid, NULL, 0);
		state->pid = 0;
	}
	/* free the semaphores */
#ifdef __QNX__
	sem_destroy(&state->semid[0]);
	sem_destroy(&state->semid[1]);
	munmap(state->semid, 2 * sizeof(sem_t));
#else
	semctl(state->semid, 0, IPC_RMID);
#endif
}

void 
doit(register iter_t iterations, void *cookie)
{
	state_t *state = (state_t *) cookie;
#ifndef __QNX__
	struct sembuf sop[2];

	sop[0].sem_num = 1;
	sop[0].sem_op = -1;
	sop[0].sem_flg = 0;

	sop[1].sem_num = 0;
	sop[1].sem_op = 1;
	sop[1].sem_flg = 0;
#endif

	while (iterations-- > 0) {
#ifdef __QNX__
		if (sem_wait(&state->semid[1]) < 0) {
			perror("(r) error on semaphore");
			exit(1);
		}
		if (sem_post(&state->semid[0]) < 0) {
			perror("(r) error on semaphore");
			exit(1);
		}
#else
		if (semop(state->semid, sop, 2) < 0) {
			perror("(r) error on semaphore");
			exit(1);
		}
#endif
	}
}

#ifdef __QNX__
void 
writer(sem_t* sid)
{
	if (sem_post(&sid[1]) < 0) {
		perror("(w) error on initial semaphore");
		exit(1);
	}

	for ( ;; ) {
		if (sem_wait(&sid[0]) < 0) {
			perror("(w) error on semaphore");
			exit(1);
		}
		if (sem_post(&sid[1]) < 0) {
			perror("(w) error on semaphore");
			exit(1);
		}
	}
}
#else
void 
writer(register int sid)
{
	struct sembuf sop[2];

	sop[0].sem_num = 1;
	sop[0].sem_op = 1;
	sop[0].sem_flg = 0;

	if (semop(sid, sop, 1) < 0) {
		perror("(w) error on initial semaphore");
		exit(1);
	}

	sop[0].sem_num = 0;
	sop[0].sem_op = -1;
	sop[0].sem_flg = 0;

	sop[1].sem_num = 1;
	sop[1].sem_op = 1;
	sop[1].sem_flg = 0;

	for ( ;; ) {
		if (semop(sid, sop, 2) < 0) {
			perror("(w) error on semaphore");
			exit(1);
		}
	}
}
#endif
