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


/*
 * 打印单次Dijkstra最短路径
 */
void printPath(const int path[], int start, int end)
{
    int temp[MAX];

    int count = 0;

    int current = end;


    while(current != -1 && count < MAX)
    {
        temp[count] = current;

        count++;


        if(current == start)
        {
            break;
        }


        current = path[current];
    }


    /*
     * 如果最终没有回溯到start，
     * 说明路径不存在
     */
    if(count == 0 || temp[count - 1] != start)
    {
        printf("路径不存在\n");

        return;
    }


    /*
     * path[]记录的是前驱，
     * 因此temp[]里面是反过来的。
     *
     * 从后向前打印即可恢复：
     *
     * start -> ... -> end
     */
    for(int i = count - 1; i >= 0; i--)
    {
        printf("%d", temp[i]);


        if(i > 0)
        {
            printf(" -> ");
        }
    }


    printf("\n");
}



/*
 * 打印TSP的订单数组下标路线
 *
 * route有count+1个元素：
 *
 * 0 -> ... -> 0
 */
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



/*
 * 根据route[]和距离矩阵
 * 重新计算一次总距离
 *
 * 用于检查TSP和2-opt结果是否正确
 */
double calculateRouteDistance(
    double matrix[MAX_ORDERS][MAX_ORDERS],
    const int route[],
    int count)
{
    double totalDistance = 0.0;


    for(int i = 0; i < count; i++)
    {
        totalDistance +=
            matrix[route[i]][route[i + 1]];
    }


    return totalDistance;
}



int main(void)
{
    /*
     * ============================================
     * 1. 创建并读取地图
     * ============================================
     */

    Graph g;


    if(loadMap(&g, "data/map/a.txt") == 0)
    {
        printf("地图读取失败！\n");

        return 1;
    }


    printf("\n========== 地图信息 ==========\n");

    printf("顶点数量：%d\n",
           g.vertexCount);

    printf("道路数量：%d\n",
           g.edgeCount);



    /*
     * ============================================
     * 2. 随机生成订单
     * ============================================
     */

    srand((unsigned int)time(NULL));


    int count = 5;


    /*
     * 防止订单数量超过距离矩阵最大值
     */
    if(count <= 0 || count > MAX_ORDERS)
    {
        printf("订单数量非法！\n");

        return 1;
    }


    Order *orders =
        generate(&g, count);


    if(orders == NULL)
    {
        printf("订单生成失败！\n");

        return 1;
    }


    printf("\n========== 随机订单 ==========\n");


    for(int i = 0; i < count; i++)
    {
        int point =
            orders[i].pointid;


        /*
         * 检查随机生成的pointid是否合法
         */
        if(point < 0 ||
           point >= g.vertexCount)
        {
            printf(
                "订单%d的配送点编号非法！\n",
                orders[i].id
            );


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



    /*
     * ============================================
     * 3. 单独验证Dijkstra
     *
     * 使用第一个订单对应顶点作为起点
     * ============================================
     */

    printf("\n========== Dijkstra测试 ==========\n");


    int start =
        orders[0].pointid;


    double dist[MAX];

    int path[MAX];


    dijkstra(
        &g,
        start,
        dist,
        path
    );


    /*
     * 计算订单1到其他订单的最短路
     */
    for(int i = 1; i < count; i++)
    {
        int end =
            orders[i].pointid;


        printf(
            "\n订单%d(顶点%d) -> 订单%d(顶点%d)\n",
            orders[0].id,
            start,
            orders[i].id,
            end
        );


        if(dist[end] >= INF)
        {
            printf("不可达！\n");

            continue;
        }


        printf(
            "最短距离：%.2f\n",
            dist[end]
        );


        printf("最短道路路径：");


        printPath(
            path,
            start,
            end
        );
    }



    /*
     * ============================================
     * 4. 创建订单距离矩阵
     * ============================================
     *
     * 使用static避免二维数组过大导致栈溢出
     */

    static double
        matrix[MAX_ORDERS][MAX_ORDERS];


    if(buildDistanceMatrix(
           &g,
           orders,
           count,
           matrix
       ) == 0)
    {
        printf(
            "\n距离矩阵生成失败！\n"
        );


        free_orders(orders);

        return 1;
    }


    printDistanceMatrix(
        matrix,
        count
    );



    /*
     * ============================================
     * 5. 检查距离矩阵
     * ============================================
     */

    printf(
        "\n========== 距离矩阵检查 ==========\n"
    );


    int matrixOk = 1;


    for(int i = 0; i < count; i++)
    {
        /*
         * 自己到自己应该为0
         */
        if(fabs(matrix[i][i]) > 1e-6)
        {
            printf(
                "错误：matrix[%d][%d]不是0！\n",
                i,
                i
            );


            matrixOk = 0;
        }


        for(int j = 0; j < count; j++)
        {
            /*
             * 检查订单之间是否不可达
             */
            if(matrix[i][j] >= INF)
            {
                printf(
                    "错误：订单%d到订单%d不可达！\n",
                    i + 1,
                    j + 1
                );


                matrixOk = 0;
            }


            /*
             * 你的Graph目前为无向图，
             * 所以理论上矩阵应该对称
             */
            if(j > i)
            {
                if(fabs(
                       matrix[i][j] -
                       matrix[j][i]
                   ) > 1e-6)
                {
                    printf(
                        "错误：matrix[%d][%d]和"
                        "matrix[%d][%d]不对称！\n",
                        i,
                        j,
                        j,
                        i
                    );


                    matrixOk = 0;
                }
            }
        }
    }


    if(matrixOk == 0)
    {
        printf(
            "距离矩阵检查失败！\n"
        );


        free_orders(orders);

        return 1;
    }


    printf(
        "距离矩阵检查通过！\n"
    );



    /*
     * ============================================
     * 6. 最近邻TSP
     * ============================================
     *
     * 规则：
     *
     * route[0] = 0
     *
     * 距离矩阵第0个订单
     * 默认作为配送起始点
     *
     * 最后重新返回0
     */

    int route[MAX_ORDERS + 1];


    double nearestDistance =
        nearestNeighbor(
            matrix,
            count,
            route
        );


    if(nearestDistance < 0.0 ||
       nearestDistance >= INF)
    {
        printf(
            "\n最近邻算法失败！\n"
        );


        free_orders(orders);

        return 1;
    }


    printf(
        "\n========== 最近邻TSP ==========\n"
    );


    printf(
        "订单数组下标路线："
    );


    printRoute(
        route,
        count
    );


    /*
     * 转换成真正的订单编号打印
     */
    printf(
        "实际订单访问顺序："
    );


    for(int i = 0; i <= count; i++)
    {
        printf(
            "%d",
            orders[route[i]].id
        );


        if(i < count)
        {
            printf(" -> ");
        }
    }


    printf("\n");


    printf(
        "最近邻总距离：%.2f\n",
        nearestDistance
    );



    /*
     * ============================================
     * 7. 2-opt路线优化
     * ============================================
     */

    double optimizedDistance =
        twoOpt(
            matrix,
            count,
            route
        );


    printf(
        "\n========== 2-opt优化 ==========\n"
    );


    printf(
        "优化后数组下标路线："
    );


    printRoute(
        route,
        count
    );


    printf(
        "优化后实际订单访问顺序："
    );


    for(int i = 0; i <= count; i++)
    {
        printf(
            "%d",
            orders[route[i]].id
        );


        if(i < count)
        {
            printf(" -> ");
        }
    }


    printf("\n");


    printf(
        "2-opt优化后总距离：%.2f\n",
        optimizedDistance
    );



    /*
     * ============================================
     * 8. 验证TSP路线
     * ============================================
     */

    printf(
        "\n========== TSP路线检查 ==========\n"
    );


    int routeOk = 1;


    int visited[MAX_ORDERS] = {0};


    /*
     * 必须从0号订单开始
     * 并且最后回到0
     */
    if(route[0] != 0 ||
       route[count] != 0)
    {
        printf(
            "错误：路线没有从0开始并返回0！\n"
        );


        routeOk = 0;
    }


    /*
     * 检查每个订单只访问一次
     */
    for(int i = 0; i < count; i++)
    {
        int index =
            route[i];


        if(index < 0 ||
           index >= count)
        {
            printf(
                "错误：路线中存在非法订单编号！\n"
            );


            routeOk = 0;

            break;
        }


        if(visited[index])
        {
            printf(
                "错误：订单下标%d被重复访问！\n",
                index
            );


            routeOk = 0;
        }


        visited[index] = 1;
    }


    /*
     * 检查有没有漏掉订单
     */
    for(int i = 0; i < count; i++)
    {
        if(!visited[i])
        {
            printf(
                "错误：订单下标%d没有访问！\n",
                i
            );


            routeOk = 0;
        }
    }


    if(routeOk == 0)
    {
        free_orders(orders);

        return 1;
    }


    /*
     * 使用最终route重新累计距离
     */
    double checkDistance =
        calculateRouteDistance(
            matrix,
            route,
            count
        );


    printf(
        "最近邻距离：%.2f\n",
        nearestDistance
    );


    printf(
        "2-opt距离：%.2f\n",
        optimizedDistance
    );


    printf(
        "重新计算路线距离：%.2f\n",
        checkDistance
    );


    /*
     * double不要直接用==
     */
    if(fabs(
           checkDistance -
           optimizedDistance
       ) > 1e-6)
    {
        printf(
            "错误：2-opt距离与重新计算结果不一致！\n"
        );


        free_orders(orders);

        return 1;
    }


    /*
     * 2-opt不应该比最近邻更差
     */
    if(optimizedDistance >
       nearestDistance + 1e-6)
    {
        printf(
            "错误：2-opt优化后距离反而增加！\n"
        );


        free_orders(orders);

        return 1;
    }


    printf(
        "TSP路线检查通过！\n"
    );



    /*
     * ============================================
     * 9. 构建完整实际道路路线
     * ============================================
     *
     * TSP：
     *
     * 0 -> 3 -> 1 -> 4 -> ...
     *
     * 转换为：
     *
     * 道路顶点22
     * ->
     * 道路顶点23
     * ->
     * 道路顶点33
     * ...
     */


    DeliveryRoute fullRoute = {0};


    if(buildFullRoute(
           &g,
           orders,
           count,
           route,
           &fullRoute
       ) == 0)
    {
        printf(
            "\n完整道路路线构建失败！\n"
        );


        free_orders(orders);

        return 1;
    }



    /*
     * ============================================
     * 10. 打印完整道路顶点和坐标
     *     同时验证距离
     * ============================================
     */


    int verified =
        printDeliveryRoute(
            &g,
            &fullRoute,
            optimizedDistance
        );


    if(verified == 0)
    {
        printf(
            "\n完整道路路线验证失败！\n"
        );


        freeDeliveryRoute(
            &fullRoute
        );


        free_orders(
            orders
        );


        return 1;
    }



    /*
     * ============================================
     * 11. 单独查看DeliveryRoute内容
     * ============================================
     */


    printf(
        "\n========== DeliveryRoute检查 ==========\n"
    );


    printf(
        "完整道路顶点数量：%d\n",
        fullRoute.count
    );


    printf(
        "完整路线Dijkstra累计距离：%.2f\n",
        fullRoute.distance
    );


    printf(
        "完整道路顶点编号：\n"
    );


    for(int i = 0;
        i < fullRoute.count;
        i++)
    {
        printf(
            "%d",
            fullRoute.points[i]
        );


        if(i <
           fullRoute.count - 1)
        {
            printf(" -> ");
        }
    }


    printf("\n");



    /*
     * ============================================
     * 12. 释放完整道路路线
     * ============================================
     */


    freeDeliveryRoute(
        &fullRoute
    );


    /*
     * 验证释放后的状态
     */
    if(fullRoute.points == NULL &&
       fullRoute.count == 0)
    {
        printf(
            "DeliveryRoute内存释放成功！\n"
        );
    }
    else
    {
        printf(
            "警告：DeliveryRoute释放状态异常！\n"
        );
    }



    /*
     * ============================================
     * 13. 释放订单
     * ============================================
     */


    free_orders(
        orders
    );


    printf(
        "\n========== 全部测试成功 ==========\n"
    );


    return 0;
}