#include <stdio.h>
#include <stdlib.h>
#include <float.h>

#include "../include/tsp.h"


/* ============================================================
 * 计算路线总距离
 * ============================================================ */

static double calculate_route_distance(
    int *route,
    int n,
    double **matrix)
{
    double total = 0.0;

    for (int i = 0; i < n - 1; i++) {

        total += matrix[
            route[i]
        ][
            route[i + 1]
        ];
    }

    return total;
}


/* ============================================================
 * 最近邻算法
 *
 * 从 start 出发，每次选择最近的未访问节点
 * ============================================================ */

static Route *nearest_neighbor(
    double **matrix,
    int n,
    int start)
{
    Route *route = malloc(sizeof(Route));

    if (route == NULL) {
        return NULL;
    }


    route->route = malloc(sizeof(int) * n);

    if (route->route == NULL) {
        free(route);
        return NULL;
    }


    route->count = n;


    /*
     * visited[i]
     *
     * 0：没有访问
     * 1：已经访问
     */
    int *visited = calloc(n, sizeof(int));

    if (visited == NULL) {
        free(route->route);
        free(route);
        return NULL;
    }


    /* 从 start 开始 */
    route->route[0] = start;

    visited[start] = 1;

    int current = start;


    /*
     * 每次找距离 current 最近的节点
     */
    for (int i = 1; i < n; i++) {

        int next = -1;

        double min_distance = DBL_MAX;


        for (int j = 0; j < n; j++) {

            /* 已经访问过 */
            if (visited[j]) {
                continue;
            }


            /* 找最近节点 */
            if (matrix[current][j] < min_distance) {

                min_distance =
                    matrix[current][j];

                next = j;
            }
        }


        /*
         * 没找到下一个节点
         */
        if (next == -1) {

            free(visited);
            free(route->route);
            free(route);

            return NULL;
        }


        route->route[i] = next;

        visited[next] = 1;

        current = next;
    }


    route->distance =
        calculate_route_distance(
            route->route,
            n,
            matrix
        );


    free(visited);

    return route;
}


/* ============================================================
 * 反转 route[left ... right]
 *
 * 例如：
 *
 * 0 1 2 3 4 5
 *   ^^^^^
 *
 * 变成：
 *
 * 0 4 3 2 1 5
 * ============================================================ */

static void reverse_route(
    int *route,
    int left,
    int right)
{
    while (left < right) {

        int temp = route[left];

        route[left] = route[right];

        route[right] = temp;

        left++;
        right--;
    }
}


/* ============================================================
 * 2-opt
 * ============================================================ */

static void two_opt(
    Route *route,
    double **matrix)
{
    int n = route->count;

    int improved = 1;


    /*
     * 只要还能找到更短的路线
     * 就继续优化
     */
    while (improved) {

        improved = 0;


        /*
         * 选择两条边：
         *
         * a -> b
         * c -> d
         *
         * 尝试改成：
         *
         * a -> c
         * b -> d
         */
        for (int i = 0; i < n - 2; i++) {

            for (int j = i + 2; j < n - 1; j++) {

                int a = route->route[i];

                int b = route->route[i + 1];

                int c = route->route[j];

                int d = route->route[j + 1];


                double old_distance =
                    matrix[a][b] +
                    matrix[c][d];


                double new_distance =
                    matrix[a][c] +
                    matrix[b][d];


                /*
                 * 如果新的连接更短
                 */
                if (new_distance < old_distance) {

                    reverse_route(
                        route->route,
                        i + 1,
                        j
                    );


                    route->distance =
                        calculate_route_distance(
                            route->route,
                            n,
                            matrix
                        );


                    improved = 1;
                }
            }
        }
    }
}


/* ============================================================
 * TSP 总入口
 *
 * 外界只需要调用：
 *
 * solve_tsp(matrix, n)
 *
 * ============================================================ */

Route *solve_tsp(double **matrix,int n)
{
    if (matrix == NULL || n <= 0) {
         printf("solve_tsp: matrix == NULL\n");
        return NULL;
    }


    /*
     * 默认：
     *
     * 节点 0 = 配送中心
     *
     * 所以从 0 开始
     */
    Route *route =
        nearest_neighbor(
            matrix,
            n,
            0
        );


    if (route == NULL) {
        return NULL;
    }


    /*
     * 最近邻得到初始解
     */
    double before =
        route->distance;


    /*
     * 2-opt优化
     */
    two_opt(
        route,
        matrix
    );


    /*
     * 最终 route->distance
     * 就是优化后的距离
     */
    (void)before;

    return route;
}


/* ============================================================
 * 打印结果
 * ============================================================ */

void print_route(Route *route)
{
    if (route == NULL) {
        return;
    }


    printf("访问顺序：");

    for (int i = 0; i < route->count; i++) {

        printf("%d", route->route[i]);

        if (i < route->count - 1) {
            printf(" -> ");
        }
    }

    printf("\n");

    printf(
        "总距离：%.2f\n",
        route->distance
    );
}


/* ============================================================
 * 释放
 * ============================================================ */

void free_route(Route *route)
{
    if (route == NULL) {
        return;
    }

    free(route->route);

    free(route);
}