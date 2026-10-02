#include <stdio.h>
#include <stdlib.h>
#include "../include/graph.h"

Graph *mapload(const char *filename){
    FILE *fp = fopen(filename, "r");
  // 以只读形式打开文件  并将文件指针赋值给fp以便于后续操作。

    if (fp == NULL) {
        perror("Failed to open file");//如果打开失败 打印出操作系统认为的原因（并不会报为何错，比如文件不存在，并不会报是你的路径写错了）
        return NULL;}




    Graph *graph = malloc(sizeof(Graph));
    if (graph==NULL){


perror("Failed to allocate memory for graph");
        fclose(fp);
        return NULL;
    }

//运行到这里说明图已经分配好了

//接下来真正开始读取数据
 int edgenum;
 if(fscanf(fp, "%d %d", &graph->sum, &edgenum)!=2){
  fclose(fp);
    free(graph);
    perror("Failed to read sum and edgenum ");
    return NULL;
    }            // 注意&  因为fcanf需要传入变量的地址以便于修改变量的值  并且如果等于2，就说明已经读取完了（也就是他会先读取sum和edgenum，如果读取成功就返回2，如果读取失败就返回0或者1）  
    // 如果不等于2，就说明读取失败了，直接报错退出

//已经有了节点的数量，分配内存
printf("sum: %d, edgenum: %d\n", graph->sum, edgenum); //检查一下数值是否正确，顺带测试一下前边的代码是否正确
   graph->points =malloc(graph->sum * sizeof(Point));

if (graph->points == NULL) {
    perror("Failed to allocate memory for points");
        fclose(fp);
        free(graph);
        return NULL;
    }
//已经有了节点的数量，接下来就可以读取节点的信息了
  for (int i = 0; i < graph->sum; i++) {

        if (fscanf(fp, "%d %lf %lf",&graph->points[i].id,&graph->points[i].x,&graph->points[i].y) != 3) {

            fclose(fp);
            free(graph->points);
            free(graph);
            return NULL;
        }
    }

    //接下来读取边的信息
    graph->adj = malloc(graph->sum * sizeof(Edge *));

    if (graph->adj == NULL) //分配指向邻接表的指针的内存，如果分配失败就报错退出
    {
        perror("Failed to allocate memory for adj list");
        fclose(fp);
        free(graph->points);
        free(graph);
        return NULL;
    }
for (int i = 0; i < graph->sum; i++) {
    graph->adj[i] = NULL;//初始化邻接表的指针为NULL，表示该点还没有边
}



    for (int i=0;i<edgenum;i++)
{
  int from;
        int to;
        double distance;  //在for循环里定义是因为他的值每次都会更新
        

if (fscanf(fp, "%d %d %lf",&from,&to,&distance) != 3) {
            fclose(fp);
            free(graph->adj);
            free(graph->points);
            free(graph);
            return NULL;}   //读取边的信息，如果读取失败就报错退出

 Edge *edge = malloc(sizeof(Edge));

        if (edge == NULL) {
            perror("Failed to allocate memory for edge");
            fclose(fp);
            free(graph->adj);
            free(graph->points);
            free(graph);
            return NULL;
        }  //分配内存给边，如果分配失败就报错退出
       

        edge->to = to-1;
        edge->distance = distance;
      
       /*for (int i = 0; i < graph->sum; i++) {
    graph->adj[i] = NULL;
}*/    //这个不应该写在这里  很严重的问题

edge->next = graph->adj[from-1];
graph->adj[from-1] = edge;  //这里最开始写错了  我写的from而不是from-1  因为数组是从0开始的，而节点的id是从1开始的，所以要减1

}
    fclose(fp);
    return graph;
}
  
//注意文件的读取位置是和光标有关的  不过本例是按照顺序依次读的，并不涉及