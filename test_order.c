
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#include "map.h"
#include "order.h"
#include "dijkstra.h"
#include "distance.h"
#include "tsp.h"
#include "path_restore.h"


// 打印Dijkstra最短路径
void printPath(const int path[], int start, int end)
{
    int route[MAX];
    int count = 0;
    int current = end;

    // 从终点沿着前驱数组回溯
    while(current != -1 && count < MAX)
    {
        if(current < 0 || current >= MAX)
        {
            printf("路径编号异常\n");
            return;
        }

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

    // 倒序输出
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


// 打印订单访问路线
void printRoute(const int route[], int count)
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


// 根据距离矩阵重新计算路线长度
double calculateRouteDistance(
    double matrix[MAX_ORDERS][MAX_ORDERS],
    const int route[],
    int count)
{
    double total = 0.0;

    for(int i = 0; i < count; i++)
    {
        total += matrix[route[i]][route[i + 1]];
    }

    return total;
}


int main(void)
{
    // ==================================
    // 1. 读取校园道路图
    // ==================================

    Graph g;

    if(loadMap(&g, "data/map/a.txt") == 0)
    {
        printf("地图读取失败！\n");
        return 1;
    }

    printf("\n========== 地图信息 ==========\n");
    printf("顶点数量：%d\n", g.vertexCount);
    printf("道路数量：%d\n", g.edgeCount);


    // ==================================
    // 2. 随机生成5个订单
    // ==================================

    srand((unsigned int)time(NULL));

    int count = 5;

    if(count > MAX_ORDERS)
    {
        printf("订单数量超过上限！\n");
        return 1;
    }

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

        if(point < 0 || point >= g.vertexCount)
        {
            printf("订单%d的顶点编号非法！\n", i + 1);
            free_orders(orders);
            return 1;
        }

        printf(
            "订单%d -> 顶点%d 坐标(%.2f, %.2f)\n",
            orders[i].id,
            point,
            g.vertices[point].x,
            g.vertices[point].y
        );
    }


    // ==================================
    // 3. 单独测试Dijkstra
    // ==================================

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


    // ==================================
    // 4. 生成订单距离矩阵
    // ==================================

    static double matrix[MAX_ORDERS][MAX_ORDERS];

    if(buildDistanceMatrix(&g, orders, count, matrix) == 0)
    {
        printf("距离矩阵生成失败！\n");
        free_orders(orders);
        return 1;
    }

    printDistanceMatrix(matrix, count);


    // ==================================
    // 5. 检验距离矩阵
    // ==================================

    printf("\n========== 距离矩阵检查 ==========\n");

    int matrixOk = 1;

    for(int i = 0; i < count; i++)
    {
        // 对角线必须为0
        if(fabs(matrix[i][i]) > 1e-6)
        {
            printf("错误：matrix[%d][%d]不为0\n", i, i);
            matrixOk = 0;
        }

        for(int j = 0; j < count; j++)
        {
            // 检查是否存在不可达订单
            if(matrix[i][j] >= INF)
            {
                printf("订单%d到订单%d不可达\n", i + 1, j + 1);
                matrixOk = 0;
            }

            // 当前Graph为双向等权图，检查对称性
            if(j > i &&
               fabs(matrix[i][j] - matrix[j][i]) > 1e-6)
            {
                printf("错误：matrix[%d][%d]不对称\n", i, j);
                matrixOk = 0;
            }
        }
    }

    if(!matrixOk)
    {
        printf("距离矩阵检查未通过！\n");
        free_orders(orders);
        return 1;
    }

    printf("距离矩阵检查通过！\n");


    // ==================================
    // 6. 最近邻TSP
    // ==================================

    int route[MAX_ORDERS + 1];

    double nearestDistance =
        nearestNeighbor(matrix, count, route);

    if(nearestDistance < 0 || nearestDistance >= INF)
    {
        printf("最近邻算法失败！\n");
        free_orders(orders);
        return 1;
    }

    printf("\n========== 最近邻TSP ==========\n");

    printf("订单数组下标路线：");
    printRoute(route, count);

    printf("实际订单访问顺序：");

    for(int i = 0; i <= count; i++)
    {
        printf("%d", orders[route[i]].id);

        if(i < count)
        {
            printf(" -> ");
        }
    }

    printf("\n最近邻总距离：%.2f\n", nearestDistance);


    // ==================================
    // 7. 2-opt优化
    // ==================================

    double optimizedDistance =
        twoOpt(matrix, count, route);

    printf("\n========== 2-opt优化 ==========\n");

    printf("优化后订单数组下标路线：");
    printRoute(route, count);

    printf("优化后实际订单访问顺序：");

    for(int i = 0; i <= count; i++)
    {
        printf("%d", orders[route[i]].id);

        if(i < count)
        {
            printf(" -> ");
        }
    }

    printf("\n2-opt总距离：%.2f\n", optimizedDistance);


    // ==================================
    // 8. 验证路线合法性与总距离
    // ==================================

    printf("\n========== 路线验证 ==========\n");

    int routeOk = 1;
    int visited[MAX_ORDERS] = {0};

    if(route[0] != 0 || route[count] != 0)
    {
        printf("错误：路线未从0出发并返回0！\n");
        routeOk = 0;
    }

    for(int i = 0; i < count; i++)
    {
        int index = route[i];

        if(index < 0 || index >= count)
        {
            printf("错误：路线中存在非法订单下标！\n");
            routeOk = 0;
            break;
        }

        if(visited[index])
        {
            printf("错误：订单下标%d被重复访问！\n", index);
            routeOk = 0;
        }

        visited[index] = 1;
    }

    for(int i = 0; i < count; i++)
    {
        if(!visited[i])
        {
            printf("错误：订单下标%d没有被访问！\n", i);
            routeOk = 0;
        }
    }

    if(!routeOk)
    {
        free_orders(orders);
        return 1;
    }

    double checkDistance =
        calculateRouteDistance(matrix, route, count);

    printf("最近邻距离：%.2f\n", nearestDistance);
    printf("2-opt距离：%.2f\n", optimizedDistance);
    printf("重新计算距离：%.2f\n", checkDistance);

    if(fabs(checkDistance - optimizedDistance) > 1e-6)
    {
        printf("错误：优化后距离与重新计算结果不一致！\n");
        free_orders(orders);
        return 1;
    }

    if(optimizedDistance > nearestDistance + 1e-6)
    {
        printf("错误：2-opt优化后距离增加！\n");
        free_orders(orders);
        return 1;
    }

    printf("路线和总距离检查通过！\n");


    // ==================================
    // 9. 还原完整道路路径
    // ==================================

    printFullRoute(&g, orders, count, route);


    // ==================================
    // 10. 释放内存
    // ==================================

    free_orders(orders);

    printf("\n========== 全部测试完成 ==========\n");

    return 0;
}
