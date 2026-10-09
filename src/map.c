#include "map.h"//链接上map.h
#include <stdio.h>
#include <stdlib.h>
// 读图函数
int loadMap(Graph *g,char *filename){
    FILE *fp=fopen(filename,"r");
    if(fp==NULL){
        printf("文件打开失败\n");
        return 0;
    }
    int vertexNum;
    int edgeNum;
    fscanf(fp,"%d %d",&vertexNum,&edgeNum);//读取点总数和边总数
    initGraph(g);
    //读取点
    for(int i=0;i<vertexNum;i++){
        int id;
        double x;
        double y;
        fscanf(fp,"%d %lf %lf",&id,&x,&y);
        addVertex(g,x,y);
    }
    //读取边
    for (int i=0;i<edgeNum;i++){
        int start ;
        int end ;
        double weight;
         
        fscanf(fp,"%d %d %lf",&start,&end, &weight);
        addEdge(g,start-1,end-1,weight);
    }
    fclose(fp);
    return 1;

}
void initGraph(Graph *c)//初始化图c
{
    c->vertexCount =0;

    c->edgeCount =0;
    for(int i =0;i<MAX_VERTICES;i++){
        c->vertices[i].first=NULL;
    }
}//初始点和边数量都为零，每个点的第一条边为空。
void addVertex(Graph *c,double x,double y)
{
    if(c->vertexCount>=MAX_VERTICES){
        return;
    }//点的数量最大为MAX
    c->vertices[c->vertexCount].id=c->vertexCount;
    c->vertices[c->vertexCount].x=x;
    c->vertices[c->vertexCount].y=y;
    //录入该点的坐标
    c->vertices[c->vertexCount].first=NULL;
    c->vertexCount++;//录入一次后点的总数加1.

}
void addEdge(Graph *c,int start, int end,double weight)//加边
{   Edge *p;//创建一个边节点
    p=(Edge*)malloc(sizeof(Edge));//申请一块内存
    p-> index=end;//边的终点
    p-> weight= weight;//边长
    p->next= c->vertices[start].first;
    c->vertices[start].first=p;//头插法，此处生成的链表会把新插入的放在最前面。
    //上面实现从A->B，下面实现从B->A
    p=(Edge*)malloc(sizeof(Edge));
    p-> index=start;
    p-> weight= weight;
    p->next= c->vertices[end].first;
    c->vertices[end].first=p;
    c->edgeCount++;
}
void printGraph(Graph *c)
{

    for(int i=0;i<c->vertexCount;i++)
    {

        printf("点%d(%.2lf,%.2lf):",
               c->vertices[i].id,
               c->vertices[i].x,
               c->vertices[i].y);



        Edge *p=c->vertices[i].first;


        while(p!=NULL)
        {

            printf(" -> 点%d 距离%.8f",
                   c->vertices[p->index].id,
                   p->weight);


            p=p->next;

        }


        printf("\n");

    }

}