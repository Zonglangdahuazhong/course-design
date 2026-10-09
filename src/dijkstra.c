#include "dijkstra.h"
#include <stdio.h>
void dijkstra(const Graph *g,int start ,double dist[], int path []){
    // 初始化
    int visited[MAX_VERTICES]={0};
    // 创建一个标记数组
    for (int i=0; i<g->vertexCount;i++){
        dist[i]=INF;
        path[i]=-1;
    }//初始最短路径未知
    if(start < 0 || start >= g->vertexCount)//检验初始点
    {
        return;
    }
    dist[start]=0.0;//初始点到自己的距离为0
    for(int i=0;i<g->vertexCount;i++)
    //最多循环点数量次，因为每次确定一个点
    {
        double min=INF;

        int u=-1;
        for (int j=0;j<g->vertexCount;j++){
            if(!visited[j]&&dist[j]<min)//j未被标记并且最短距离小于min
            
            {
                min=dist[j];
                u=j;
            }
        }
        if(u==-1)
          break;
        visited[u]=1;
        const Edge *p=g->vertices[u].first;//找到u的第一条边
        
        while(p!=NULL)
        {
            int v=p->index;
            if(!visited[v]&&dist[u]+p->weight<dist[v]){
                dist[v]=dist[u]+p->weight;
                path[v]=u;
            }
            p=p->next;
        }
    }
}