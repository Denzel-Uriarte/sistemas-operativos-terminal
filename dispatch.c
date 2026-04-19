#include "graph.h"
#include "terminal.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>

extern char **environ;

/*
 * Ejecuta un nodo del grafo: parsea su comando, resuelve la ruta,
 * y crea un proceso hijo con fork + execve.
 * Retorna 0 si el hijo se lanzo correctamente.
 */
static int exec_node(struct node *n)
{
	char cmd_buf[MAX_CMD];
	char *argv[MAX_ARGS];
	char path[MAX_PATH_LEN];

	strncpy(cmd_buf, n->cmd, sizeof(cmd_buf) - 1);
	cmd_buf[sizeof(cmd_buf) - 1] = '\0';

	if (parse_args(cmd_buf, argv, MAX_ARGS) == 0) {
		fprintf(stderr, "[dispatch] nodo '%s': comando vacio\n", n->name);
		return -1;
	}

	if (resolve_path(argv[0], path, sizeof(path)) != 0) {
		fprintf(stderr, "[dispatch] nodo '%s': comando no encontrado: %s\n",
		        n->name, argv[0]);
		return -1;
	}

	pid_t pid = fork();
	if (pid < 0) {
		perror("[dispatch] fork");
		return -1;
	}

	if (pid == 0) {
		/* Los nodos del grafo son autocontenidos: redirigir stdin
		   a /dev/null para que procesos paralelos no compitan
		   por la entrada de la terminal. */
		int null_fd = open("/dev/null", O_RDONLY);
		if (null_fd >= 0) {
			dup2(null_fd, STDIN_FILENO);
			close(null_fd);
		}
		execve(path, argv, environ);
		perror(path);
		_exit(127);
	}

	n->pid    = pid;
	n->status = STATUS_RUNNING;
	fprintf(stderr, "[dispatch] iniciado '%s' (pid %d): %s\n", n->name, pid, n->cmd);
	return 0;
}

/* Busca el nodo que corresponde a un PID de proceso hijo */
static struct node *find_by_pid(struct graph *g, pid_t pid)
{
	for (int i = 0; i < g->count; i++)
		if (g->nodes[i].pid == pid)
			return &g->nodes[i];
	return NULL;
}

/* Propaga el fallo de un nodo a todos sus dependientes transitivos:
   si un nodo falla, ninguno de sus sucesores puede ejecutarse. */
static void propagate_failure(struct graph *g, int node_id, int *completed)
{
	for (int j = 0; j < g->nodes[node_id].edge_count; j++) {
		int dep = g->nodes[node_id].edges[j];
		if (g->nodes[dep].status == STATUS_PENDING) {
			g->nodes[dep].status = STATUS_FAILED;
			(*completed)++;
			fprintf(stderr, "[dispatch] '%s' cancelado (dependencia '%s' fallo)\n",
			        g->nodes[dep].name, g->nodes[node_id].name);
			propagate_failure(g, dep, completed);
		}
	}
}

/*
 * Maneja la terminacion de un proceso hijo: actualiza el estado del nodo,
 * y si termino bien libera a sus dependientes (decrementa rem[]),
 * o si fallo propaga el fallo a todos sus sucesores.
 */
static void handle_exit(struct graph *g, pid_t pid, int wstatus,
                        int *rem, int *completed, int *running)
{
	struct node *n = find_by_pid(g, pid);
	if (!n) return;

	int ok = WIFEXITED(wstatus) && WEXITSTATUS(wstatus) == 0;
	int code = WIFEXITED(wstatus) ? WEXITSTATUS(wstatus) : -1;
	(*running)--;

	if (ok) {
		n->status = STATUS_DONE;
		(*completed)++;
		fprintf(stderr, "[dispatch] '%s' completado (exit %d)\n", n->name, code);
		/* decrementar dependencias pendientes de cada sucesor */
		for (int j = 0; j < n->edge_count; j++)
			rem[n->edges[j]]--;
	} else {
		n->status = STATUS_FAILED;
		(*completed)++;
		fprintf(stderr, "[dispatch] '%s' FALLO (exit %d)\n", n->name, code);
		propagate_failure(g, n->id, completed);
	}
}

/*
 * Despacho principal: recorre el grafo en orden topologico.
 * Lanza en paralelo todos los nodos cuyas dependencias ya se cumplieron,
 * espera a que terminen, y libera a sus sucesores.
 */
int dispatch(struct graph *g)
{
	int total     = g->count;
	int completed = 0;
	int running   = 0;

	/* rem[i] = cantidad de dependencias pendientes del nodo i */
	int *rem = calloc((size_t)total, sizeof(int));
	if (!rem) return -1;
	for (int i = 0; i < total; i++)
		rem[i] = g->nodes[i].dep_count;

	while (completed < total) {
		/* lanzar todos los nodos cuyas dependencias estan satisfechas */
		for (int i = 0; i < total; i++) {
			if (g->nodes[i].status != STATUS_PENDING || rem[i] > 0)
				continue;
			if (exec_node(&g->nodes[i]) == 0) {
				running++;
			} else {
				g->nodes[i].status = STATUS_FAILED;
				completed++;
				propagate_failure(g, i, &completed);
			}
		}

		if (running == 0)
			break;

		/* esperar a que al menos un hijo termine (bloqueante) */
		int wstatus;
		pid_t pid = waitpid(-1, &wstatus, 0);
		if (pid < 0) {
			if (errno == ECHILD) break;
			perror("[dispatch] waitpid");
			break;
		}
		handle_exit(g, pid, wstatus, rem, &completed, &running);

		/* recolectar otros hijos que ya terminaron (no bloqueante) */
		while ((pid = waitpid(-1, &wstatus, WNOHANG)) > 0)
			handle_exit(g, pid, wstatus, rem, &completed, &running);
	}

	free(rem);

	/* contar nodos fallidos para el reporte final */
	int failed = 0;
	for (int i = 0; i < total; i++)
		if (g->nodes[i].status == STATUS_FAILED)
			failed++;

	if (failed) {
		fprintf(stderr, "[dispatch] %d/%d nodo(s) fallaron\n", failed, total);
		return -1;
	}
	fprintf(stderr, "[dispatch] los %d nodo(s) se completaron exitosamente\n", total);
	return 0;
}
