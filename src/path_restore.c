#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include <math.h>
#include "path_restore.h"
#include "dijkstra.h"
//打印一条路径
static void printOnePath(const Graph *g,int start,int end){
    double dist[MAX];
    int path[MAX];
    dijkstra(g,start,dist,path);//调用dijstra算出dist 和path
    if(dist[end]>=INF){
        printf("不可达");
        return;
    }
    int temp[MAX];//记录当前路径
    int length=0;
    int current =end;
    //从后往前遍历，把路径存入temp
    while(current !=-1&&length<MAX){
        temp[length]=current;
        length++;
        if(current==start){
            break;
        }
        current =path[current];

    }
    if (length==0||temp[length-1]!=start){
        printf("路径不存在");
        return;
    }
    //开始再倒叙打印，负负得正。
    for(int i=length-1;i>=0;i--){
        printf("%d",temp[i]);
        if(i>0){
            printf(" -> ");
        }
    }

}
void printFullRoute(const Graph *g,const Order orders[],int count ,const int route[]){
     printf("\n========== 完整道路路径 ==========\n");
     for (int i=0;i<count ;i++){
        //记录相邻订单的编号地点
        int fromOrder =route[i];
        int toOrder =route[i+1];
        int start=orders[fromOrder].pointid;
        int end=orders[toOrder].pointid;
        printf("\n订单%d(顶点%d) -> 订单%d(顶点%d)\n",orders[fromOrder].id,start,orders[toOrder].id,end);
        printf("道路路径：");
        printOnePath(g,start,end);//调用函数打印相邻两个订单的实际路径
        printf("\n");
     }
}

static int appendPoint( DeliveryRoute *result, int *capacity,int point)
{
    // 如果数组容量不足进行扩容
    if(result->count >= *capacity)
    {
        int newCapacity;
//初始分配16个元素
        if(*capacity == 0){
            newCapacity = 16;
        }
        else{
            if(*capacity > INT_MAX / 2)//容量翻倍后若不超过int最大值就进行翻倍
                return 0;
            newCapacity = (*capacity) * 2;
        }

        if((size_t)newCapacity > SIZE_MAX / sizeof(int))//可能溢出扩容失败
            return 0;

        int *newPoints = realloc( result->points, (size_t)newCapacity * sizeof(int));
        //重新分配内存

        if(newPoints == NULL)
            return 0;

        result->points = newPoints;
        *capacity = newCapacity;
    }
//更新
    result->points[result->count] = point;

    result->count++;

    return 1;
}

int buildFullRoute( const Graph *g,const Order orders[],int count,const int route[],DeliveryRoute *result)
{
    if(result == NULL)
        return 0;

    // 检查调用前result是否已初始化
    if(result->points != NULL)
        return 0;

    result->count = 0;
    result->distance = 0.0;

    if(g == NULL || orders == NULL || route == NULL || count <= 0 || g->vertexCount <= 0 ||g->vertexCount > MAX){
        return 0;
    }

    int capacity = 0;
    double dist[MAX];
    int path[MAX];
    int temp[MAX];

    // 逐段处理TSP路线
    for(int i = 0; i < count; i++){
        int fromOrder = route[i];
        int toOrder = route[i + 1];

        // 检查订单下标
        if(fromOrder < 0 || fromOrder >= count || toOrder < 0 || toOrder >= count)
            goto fail;

        int start = orders[fromOrder].pointid;
        int end = orders[toOrder].pointid;

        // 检查地图顶点下标
        if(start < 0 || start >= g->vertexCount || end < 0 || end >= g->vertexCount)
            goto fail;

        // 调用dijkstra计算当前两订单之间的道路最短路
        dijkstra(g, start, dist, path);
        if(dist[end] >= INF)
            goto fail;
        // 累加每段最短距离
        result->distance += dist[end];
        // 从终点沿着path[]倒推回起点
        int length = 0;
        int current = end;

        while(current != -1 && length < g->vertexCount)
        {
            if(current < 0 || current >= g->vertexCount)//检查
                goto fail;

            temp[length++] = current;

            if(current == start)
                break;

            current = path[current];
        }

        // 必须成功回溯到当前起点
        if(length == 0 || temp[length - 1] != start)
            goto fail;

        // 将倒序路径恢复成正序并添加到结果
        for(int j = length - 1; j >= 0; j--)
        {
            int point = temp[j];

            // 只跳过相邻路段交界处的重复顶点
            if(result->count > 0 &&j == length - 1 && result->points[result->count - 1] == point){
                continue;
            }

            if(!appendPoint(result, &capacity, point))
                goto fail;
        }
    }
    return 1;
fail:
    freeDeliveryRoute(result);
    return 0;
}
void freeDeliveryRoute(DeliveryRoute *result){
    if(result == NULL)
        return;

    free(result->points);

    result->points = NULL;
    result->count = 0;
    result->distance = 0.0;
}

int printDeliveryRoute( const Graph *g, const DeliveryRoute *result, double tspDistance){
    if(g == NULL || result == NULL ||result->points == NULL || result->count <= 0){
        return 0;
    }

    double actualDistance = 0.0;

    printf("\n========== 完整道路路线 ==========\n");
    printf("经过道路顶点数量：%d\n", result->count);

    for(int i = 0; i < result->count; i++){
        int u = result->points[i];

        if(u < 0 || u >= g->vertexCount) {
            printf("顶点编号错误！\n");
            return 0;
        }

        printf("第%d个点：顶点%d 坐标(%.8f, %.8f)\n",i + 1,u,g->vertices[u].x,g->vertices[u].y);

        // 第一个点前面没有道路
        if(i == 0)
            continue;

        int v = result->points[i - 1];

        // 在邻接表中查找v到u的道路
        const Edge *p = g->vertices[v].first;

        double weight = INF;

        while(p != NULL){
            if(p->index == u && p->weight < weight){
                weight = p->weight;
            }
            p = p->next;
        }

        if(weight >= INF){
            printf("错误：顶点%d到顶点%d没有直接道路！\n", v, u);
            return 0;
        }
        actualDistance += weight;
    }

    printf("\n========== 距离验证 ==========\n");

    printf("TSP优化距离：%.2f\n", tspDistance);
    printf("Dijkstra累计距离：%.2f\n", result->distance);
    printf("实际边权累计距离：%.2f\n", actualDistance);

    if(!isfinite(tspDistance) ||
       !isfinite(result->distance) ||
       !isfinite(actualDistance) ||
       fabs(tspDistance - result->distance) > 1e-6 ||
       fabs(result->distance - actualDistance) > 1e-6) {
        printf("验证失败：三种距离不一致！\n");
        return 0;
    }

    printf("验证通过：三种距离一致！\n");

    return 1;
}
