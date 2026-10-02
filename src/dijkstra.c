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

//接下来把结果存储起来   以便后续算最优访问路径时使用
double **generateMatrix(Graph *graph,Order **orders,int order_count,int base)
{
    int target_count = order_count + 1;

    // targets[0] 是配送中心
    // targets[1...] 是订单节点
    int *targets = malloc(
        target_count * sizeof(int)
    );

    if (targets == NULL) {
        printf("targets 内存分配失败\n");
        return NULL;
    }

    targets[0] = base;

    for (int i = 0; i < order_count; i++) {
        targets[i + 1] = orders[i]->pointid-1;//这句
    }

    // 创建二维距离矩阵
    double **matrix = malloc(
        target_count * sizeof(double *)
    );
    /*
    matrix
  ↓
+----------+       +---------------------------+
| matrix[0] | ----→ | double double double ... |
+----------+       +---------------------------+
| matrix[1] | ----→ | double double double ... |
+----------+       +---------------------------+
| matrix[2] | ----→ | double double double ... |
+----------+       +---------------------------+
| matrix[3] | ----→ | double double double ... |
+----------+       +---------------------------+
    
    */

    if (matrix == NULL) {
        free(targets);
        return NULL;
    }

    for (int i = 0; i < target_count; i++) {

        matrix[i] = malloc(
            target_count * sizeof(double)
        );

        if (matrix[i] == NULL) {

            for (int j = 0; j < i; j++) {
                free(matrix[j]);
            }

            free(matrix);
            free(targets);

            return NULL;
        }
    }

    // Dijkstra 使用的数组
    double *dist = malloc(
        graph->sum * sizeof(double)
    );

    int *prev = malloc(
        graph->sum * sizeof(int)
    );

    if (dist == NULL || prev == NULL) {

        free(dist);
        free(prev);

        freeMatrix(
            matrix,
            target_count
        );

        free(targets);

        return NULL;
    }

    // 对每一个目标节点运行一次 Dijkstra
    for (int i = 0; i < target_count; i++) {

        int start = targets[i];

        dijkstra(graph,start,dist,prev);

        // 只提取其他目标节点的距离
        for (int j = 0; j < target_count; j++) {

            matrix[i][j] =
                dist[targets[j]];
        }
    }

    free(dist);
    free(prev);
    free(targets);

    return matrix;
}



void freeMatrix(
    double **matrix,
    int size
)
{
    if (matrix == NULL) {
        return;
    }

    for (int i = 0; i < size; i++) {
        free(matrix[i]);
    }

    free(matrix);
}