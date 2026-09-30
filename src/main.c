#include <stdio.h>
#include "../include/graph.h"

int main(void)
{
    Graph *graph = mapload("data/map/hit.txt");

    if (graph == NULL) {
        printf("地图加载失败\n");
        return 1;
    }

    printf("地图加载成功！\n");
    printf("节点数量：%d\n", graph->sum);

    // 测试第一个节点
    printf("\n第一个节点：\n");
    printf("id = %d\n", graph->points[0].id);
    printf("x = %.8f\n", graph->points[0].x);
    printf("y = %.8f\n", graph->points[0].y);

    // 测试第一个节点的邻接边
    printf("\n节点 %d 的边：\n", graph->points[0].id);

    Edge *edge = graph->adj[0];

    while (edge != NULL) {
        printf("-> %d, distance = %.2f\n",
               edge->to,
               edge->distance);

        edge = edge->next;
    }

    return 0;
}