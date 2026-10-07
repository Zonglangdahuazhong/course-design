#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "map.h"
#include "order.h"
#include "dijkstra.h"
#include "distance.h"
#include "tsp.h"


// 打印Dijkstra路径
void printPath(int path[], int start, int end)
{
    int route[MAX];
    int count = 0;

    int current = end;

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


    if(count == 0 || route[count - 1] != start)
    {
        printf("路径不存在\n");
        return;
    }


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


// 打印TSP路线
void printRoute(int route[], int count)
{
    for(int i = 0; i <= count; i++)
    {
        printf("%d", route[i]);

        if(i < count)
        {
            printf(" -> ");
        }
    }

    printf("\n");
}


int main(void)
{
    Graph g;


    // =========================
    // 1. 读取地图
    // =========================

    if(loadMap(&g, "data/map/a.txt") == 0)
    {
        printf("地图读取失败！\n");

        return 1;
    }


    printf("========== 地图信息 ==========\n");

    printf("顶点数量：%d\n", g.vertexCount);

    printf("道路数量：%d\n", g.edgeCount);



    // =========================
    // 2. 生成随机订单
    // =========================

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
        int point = orders[i].pointid;


        printf(
            "订单%d -> 顶点%d 坐标(%.2f, %.2f)\n",
            orders[i].id,
            point,
            g.vertices[point].x,
            g.vertices[point].y
        );
    }



    // =========================
    // 3. 单独测试Dijkstra
    // =========================

    printf("\n========== Dijkstra测试 ==========\n");


    int start = orders[0].pointid;


    double dist[MAX];

    int path[MAX];


    dijkstra(&g, start, dist, path);


    for(int i = 1; i < count; i++)
    {
        int end = orders[i].pointid;


        printf(
            "\n订单1(顶点%d) -> 订单%d(顶点%d)\n",
            start,
            orders[i].id,
            end
        );


        if(dist[end] >= INF)
        {
            printf("不可达\n");

            continue;
        }


        printf("最短距离：%.2f\n", dist[end]);


        printf("最短路径：");

        printPath(path, start, end);
    }



    // =========================
    // 4. 生成订单距离矩阵
    // =========================

    double matrix[MAX_ORDERS][MAX_ORDERS];


    if(buildDistanceMatrix(
        &g,
        orders,
        count,
        matrix
    ) == 0)
    {
        printf("距离矩阵生成失败！\n");

        free_orders(orders);

        return 1;
    }


    printDistanceMatrix(matrix, count);



    // =========================
    // 5. 检查距离矩阵
    // =========================

    printf("\n========== 距离矩阵检查 ==========\n");


    int matrixOk = 1;


    for(int i = 0; i < count; i++)
    {
        // 自己到自己必须为0
        if(matrix[i][i] != 0.0)
        {
            printf(
                "错误：matrix[%d][%d]不是0\n",
                i,
                i
            );

            matrixOk = 0;
        }
    }


    if(matrixOk)
    {
        printf("矩阵主对角线检查通过！\n");
    }



    // =========================
    // 6. 最近邻TSP
    // =========================

    int route[MAX_ORDERS + 1];


    double nearestDistance =
        nearestNeighbor(
            matrix,
            count,
            route
        );


    if(nearestDistance < 0)
    {
        printf("最近邻算法失败！\n");

        free_orders(orders);

        return 1;
    }


    printf("\n========== 最近邻TSP ==========\n");


    printf("路线：");

    printRoute(route, count);


    printf(
        "总距离：%.2f\n",
        nearestDistance
    );



    // =========================
    // 7. 2-opt优化
    // =========================

    double optimizedDistance =
        twoOpt(
            matrix,
            count,
            route
        );


    printf("\n========== 2-opt优化 ==========\n");


    printf("优化路线：");

    printRoute(route, count);


    printf(
        "优化后总距离：%.2f\n",
        optimizedDistance
    );



    // =========================
    // 8. 检查2-opt是否合理
    // =========================

    printf("\n========== 优化结果检查 ==========\n");


    if(optimizedDistance <= nearestDistance)
    {
        printf(
            "优化成功：%.2f -> %.2f\n",
            nearestDistance,
            optimizedDistance
        );
    }
    else
    {
        printf(
            "警告：优化后距离反而增加！\n"
        );
    }



    // =========================
    // 9. 检查最终路线是否回到起点
    // =========================

    if(route[0] == 0 &&
       route[count] == 0)
    {
        printf("路线起点和终点检查通过！\n");
    }
    else
    {
        printf(
            "错误：路线没有正确返回0号起点！\n"
        );
    }



    // =========================
    // 10. 验证最终路线总距离
    // =========================

    double checkDistance = 0.0;


    for(int i = 0; i < count; i++)
    {
        checkDistance +=
            matrix[route[i]]
                  [route[i + 1]];
    }


    printf(
        "重新计算路线总距离：%.2f\n",
        checkDistance
    );


    if(checkDistance == optimizedDistance)
    {
        printf("总距离验证通过！\n");
    }
    else
    {
        printf(
            "警告：总距离验证不一致！\n"
        );
    }



    // =========================
    // 11. 释放内存
    // =========================

    free_orders(orders);


    printf("\n========== 全部测试结束 ==========\n");


    return 0;
}