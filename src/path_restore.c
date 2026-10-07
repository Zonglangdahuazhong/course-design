#include <stdio.h>
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