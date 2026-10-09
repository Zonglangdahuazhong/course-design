#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "rtree.h"

int main(void){
    Graph *g=malloc(sizeof(*g));
    if(!g)return 1;
    if(!loadMap(g,"data/map/a.txt")){puts("地图读取失败");free(g);return 1;}
    srand((unsigned)time(NULL));
    const int count=30;
    Order *orders=generate(g,count);
    if(!orders){puts("订单生成失败");free(g);return 1;}
    RTree tree;initRTree(&tree);
    for(int i=0;i<count;i++){
        if(!insertOrder(&tree,g,orders,i)){
            printf("插入订单%d失败\n",i+1);
            freeRTree(&tree);free_orders(orders);free(g);return 1;
        }
    }
    Rect q={0,0,50,50};
    int hits[count];
    int found=rangeQuery(&tree,q,hits,count);
    if(found<0||found>count){puts("查询异常");freeRTree(&tree);free_orders(orders);free(g);return 1;}
    int expected[count],seen[count];
    for(int i=0;i<count;i++){expected[i]=0;seen[i]=0;}
    int brute=0;
    for(int i=0;i<count;i++){
        int id=orders[i].pointid;
        double x=g->vertices[id].x,y=g->vertices[id].y;
        if(x>=q.xmin&&x<=q.xmax&&y>=q.ymin&&y<=q.ymax){expected[i]=1;brute++;}
    }
    printf("插入订单：%d\nR树查询命中：%d\n暴力查询命中：%d\n",count,found,brute);
    int ok=(found==brute);
    for(int i=0;i<found;i++){
        int k=hits[i];
        if(k<0||k>=count){ok=0;continue;}
        if(seen[k]++||!expected[k])ok=0;
        int id=orders[k].pointid;
        printf("订单%d 顶点%d 坐标(%.2f, %.2f)\n",orders[k].id,id,g->vertices[id].x,g->vertices[id].y);
    }
    for(int i=0;i<count;i++)if(expected[i]&&!seen[i])ok=0;
    printf("交叉验证：%s\n",ok?"通过":"失败");
    freeRTree(&tree);free_orders(orders);free(g);
    return ok?0:1;
}
