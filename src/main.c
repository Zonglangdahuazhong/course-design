#include <stdio.h>
#include <stdlib.h>
#include <float.h>

#include "../include/graph.h"
#include "../include/dijkstra.h"

int main()
{
    Graph *graph = mapload("data//map/hit.txt");

    if (graph == NULL) {
        printf("地图加载失败\n");
        return 1;
    }

    // 从 0 号节点出发
    int start = 0;

    double *dist = malloc(graph->sum * sizeof(double));
    int *prev = malloc(graph->sum * sizeof(int));

    if (dist == NULL || prev == NULL) {
        printf("内存分配失败\n");
        return 1;
    }

    dijkstra(graph, start, dist, prev);

    // 输出前 20 个节点的最短距离
    for (int i = 0; i < 20 && i < graph->sum; i++) {

        if (dist[i] == DBL_MAX) {
            printf("%d -> %d : 不可达\n",
                   start, i);
        }
        else {
            printf("%d -> %d : %.2f\n",
                   start, i, dist[i]);
        }
    }

    free(dist);
    free(prev);

    return 0;
}