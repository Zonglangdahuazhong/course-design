#ifndef DIJKSTRA_H
#define DIJKSTRA_H

#include "graph.h"
#include "order.h"
       
void dijkstra(Graph *graph, int start, double *dist, int *prev);
double **generateMatrix(Graph *graph,Order **orders,int order_count,int base);

void freeMatrix(double **matrix, int size);
#endif