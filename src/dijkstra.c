#include <stdio.h>
#include <stdlib.h>
#include <float.h>

#include "../include/dijkstra.h"

void dijkstra(Graph *graph, int start, double *dist, int *prev)
{
    int n = graph->sum;

    int *visited = calloc(n, sizeof(int));//全部初始化为0，也就是最开始都没有访问  其实后边初始化里的visited[i]=0可以不写，calloc已经完成了初始化，但是malloc就必须自行初始化

    if (visited == NULL) {
        printf("failed to allocate memory for visited array\n");
        return;
    }

    // 初始化
    for (int i = 0; i < n; i++) {
        dist[i] = DBL_MAX;//最开始距离都当作是∞
        visited[i]=0;
        prev[i] = -1;
    }

    // 起点到自己的距离为 0
    dist[start] = 0;

    // Dijkstra 主循环
    for (int i = 0; i < n; i++) {

        // 找当前距离最小的未访问节点
        int u = -1;//目前找到的距离最小的节点的下标

        for (int j = 0; j < n; j++) {

            if (!visited[j] &&
                dist[j] != DBL_MAX && 
                (u == -1 || dist[j] < dist[u]))//短路求值  u==-1时数组不会越界访问dist[-1]
                
                {

                u = j;
            }
        }

        // 没有可以继续访问的节点
        if (u == -1) {
            break;
        }

        // 确定 u
        visited[u] = 1;

        // 遍历 u 的所有邻接边  
        for (Edge *edge = graph->adj[u];
             edge != NULL;
             edge = edge->next) {

            int v = edge->to;

            double new_dist =
                dist[u] + edge->distance;

            // 松弛
            if (new_dist < dist[v]) {
                dist[v] = new_dist;
                prev[v] = u;
            }
        }
    }

    free(visited);
}