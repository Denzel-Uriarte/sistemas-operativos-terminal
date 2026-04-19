#ifndef GRAPH_H
#define GRAPH_H

#include <stdio.h>
#include <sys/types.h>

#define MAX_NAME  32
#define MAX_CMD   4096

enum node_status {
	STATUS_PENDING  =  0,
	STATUS_RUNNING  =  1,
	STATUS_DONE     =  2,
	STATUS_FAILED   = -1
};

struct node {
	char              name[MAX_NAME];
	char              cmd[MAX_CMD];
	int               id;
	int               scc_id;
	int               target;       /* destino de ejecucion (0 = local) */
	enum node_status  status;
	pid_t             pid;

	int  *edges;                    /* aristas salientes: nodos que dependen de este */
	int   edge_count;
	int   edge_cap;

	int  *deps;                     /* aristas entrantes: nodos de los que este depende */
	int   dep_count;
	int   dep_cap;

	/* Estado del algoritmo de Tarjan para SCC */
	int   tj_index;
	int   tj_lowlink;
	int   on_stack;
};

struct graph {
	struct node *nodes;
	int          count;
	int          cap;
	int          scc_count;
};

/* Ciclo de vida del grafo */
void  graph_init(struct graph *g);
void  graph_free(struct graph *g);

/* generateGraph: parsea el JDL desde un FILE* y llena el grafo.
   Retorna 0 en exito. */
int   generate_graph(struct graph *g, FILE *f);

/* SCC: algoritmo de Tarjan. Retorna 0 si es DAG (sin ciclos),
   -1 si se detectaron ciclos. */
int   scc_tag(struct graph *g);

/* mapGraph: asigna cada nodo a un destino de ejecucion (local por ahora). */
void  map_graph(struct graph *g);

/* dispatch: ejecuta el grafo respetando dependencias.
   Retorna 0 en exito. */
int   dispatch(struct graph *g);

/* Utilidades */
int   graph_find_node(const struct graph *g, const char *name);
void  graph_print(const struct graph *g);

#endif
