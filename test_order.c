#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "map.h"
#include "order.h"
#include "dijkstra.h"


// 打印从start到end的最短路径
void printPath(int path[], int start, int end)
{
    int route[MAX];
    int count = 0;

    int current = end;

    // 从终点沿着前驱数组往回寻找
    while(current != -1 && count < MAX)
    {
        route[count] = current;
        count++;

        if(current == start)
        {
            break;
        }

        current = path[current];
    }

    // 检查是否成功回到起点
    if(count == 0 || route[count - 1] != start)
    {
        printf("路径不存在\n");
        return;
    }

    // 倒序打印，转换为起点到终点
    for(int i = count - 1; i >= 0; i--)
    {
        printf("%d", route[i]);

        if(i > 0)
        {
            printf(" -> ");
        }
    }

    printf("\n");
}


int main(void)
{
    Graph g;

    // 1. 读取地图
    if(loadMap(&g, "data/map/a.txt") == 0)
    {
        printf("地图读取失败！\n");
        return 1;
    }

    printf("地图读取成功！\n");

    printf("顶点数量：%d\n", g.vertexCount);
    printf("道路数量：%d\n", g.edgeCount);


    // 2. 生成5个随机订单
    srand((unsigned int)time(NULL));

    int count = 5;

    Order *orders = generate(&g, count);

    if(orders == NULL)
    {
        printf("订单生成失败！\n");
        return 1;
    }


    printf("\n========== 随机订单 ==========\n");

    for(int i = 0; i < count; i++)
    {
        int index = orders[i].pointid;

        printf("订单%d：顶点%d 坐标(%.2f, %.2f)\n",
               orders[i].id,
               index,
               g.vertices[index].x,
               g.vertices[index].y);
    }


    // 3. 以订单1的配送点作为Dijkstra起点
    int start = orders[0].pointid;

    double dist[MAX];
    int path[MAX];

    dijkstra(&g, start, dist, path);


    // 4. 打印订单1到其他订单的最短距离
    printf("\n========== 最短路径测试 ==========\n");

    for(int i = 1; i < count; i++)
    {
        int end = orders[i].pointid;

        printf("\n订单1 -> 订单%d\n", orders[i].id);

        if(dist[end] >= INF)
        {
            printf("两个配送点之间不存在道路！\n");
            continue;
        }

        printf("最短距离：%.2f\n", dist[end]);

        printf("经过的顶点：");

        printPath(path, start, end);
    }


    // 5. 释放订单动态内存
    free_orders(orders);

    return 0;
}