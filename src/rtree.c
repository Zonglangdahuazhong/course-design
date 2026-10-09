#include "rtree.h"
#include <stdlib.h>
#include <float.h>
#include <math.h>

#define N (RTREE_MAX_ENTRIES+1)
static RTreeNode *newNode(int leaf){
    RTreeNode *p=calloc(1,sizeof(*p));
    if(p) p->isLeaf=leaf;
    return p;
}
Rect makePointRect(double x,double y){Rect r={x,y,x,y};return r;}
int rectIntersect(Rect a,Rect b){return a.xmin<=b.xmax&&a.xmax>=b.xmin&&a.ymin<=b.ymax&&a.ymax>=b.ymin;}
static Rect combine(Rect a,Rect b){
    Rect r={fmin(a.xmin,b.xmin),fmin(a.ymin,b.ymin),fmax(a.xmax,b.xmax),fmax(a.ymax,b.ymax)};
    return r;
}
static double area(Rect r){return (r.xmax-r.xmin)*(r.ymax-r.ymin);}
static Rect bounds(const RTreeNode *n){
    Rect r=n->entries[0].rect;
    for(int i=1;i<n->count;i++)r=combine(r,n->entries[i].rect);
    return r;
}
static double enlargement(Rect a,Rect b){return area(combine(a,b))-area(a);}
static void append(RTreeNode *n,RTreeEntry e){n->entries[n->count++]=e;}
/* Split full node plus new entry into two nodes. Allocation happens before mutation. */
static RTreeNode *split(RTreeNode *node,RTreeEntry extra){
    RTreeNode *other=newNode(node->isLeaf);
    if(!other)return NULL;
    RTreeEntry all[N];
    for(int i=0;i<RTREE_MAX_ENTRIES;i++)all[i]=node->entries[i];
    all[RTREE_MAX_ENTRIES]=extra;
    int s1=0,s2=1;
    double worst=-1.0;
    for(int i=0;i<N;i++)for(int j=i+1;j<N;j++){
        double waste=area(combine(all[i].rect,all[j].rect))-area(all[i].rect)-area(all[j].rect);
        if(waste>worst){worst=waste;s1=i;s2=j;}
    }
    int used[N]={0};used[s1]=used[s2]=1;
    node->count=0;append(node,all[s1]);append(other,all[s2]);
    int remaining=N-2;
    while(remaining){
        if(node->count+remaining==RTREE_MIN_ENTRIES){
            for(int i=0;i<N;i++)if(!used[i]){append(node,all[i]);used[i]=1;remaining--;}
            break;
        }
        if(other->count+remaining==RTREE_MIN_ENTRIES){
            for(int i=0;i<N;i++)if(!used[i]){append(other,all[i]);used[i]=1;remaining--;}
            break;
        }
        Rect a=bounds(node),b=bounds(other);
        int pick=-1;double biggest=-1;
        for(int i=0;i<N;i++)if(!used[i]){
            double diff=fabs(enlargement(a,all[i].rect)-enlargement(b,all[i].rect));
            if(diff>biggest){biggest=diff;pick=i;}
        }
        double da=enlargement(a,all[pick].rect),db=enlargement(b,all[pick].rect);
        if(da<db || (da==db && (area(a)<area(b) || (area(a)==area(b)&&node->count<=other->count))))
            append(node,all[pick]);
        else append(other,all[pick]);
        used[pick]=1;remaining--;
    }
    return other;
}
/* 0=allocation failure, 1=success; splitOut is sibling or NULL. */
static int insertRecursive(RTreeNode *node,RTreeEntry item,RTreeNode **splitOut){
    *splitOut=NULL;
    if(node->isLeaf){
        if(node->count<RTREE_MAX_ENTRIES){append(node,item);return 1;}
        *splitOut=split(node,item);
        return *splitOut!=NULL;
    }
    int best=0;double bestGrow=DBL_MAX,bestArea=DBL_MAX;
    for(int i=0;i<node->count;i++){
        double grow=enlargement(node->entries[i].rect,item.rect);
        double a=area(node->entries[i].rect);
        if(grow<bestGrow||(grow==bestGrow&&a<bestArea)){
            best=i;bestGrow=grow;bestArea=a;
        }
    }
    RTreeNode *sibling=NULL;
    if(!insertRecursive(node->entries[best].child,item,&sibling))return 0;
    node->entries[best].rect=bounds(node->entries[best].child);
    if(!sibling)return 1;
    RTreeEntry e={bounds(sibling),-1,sibling};
    if(node->count<RTREE_MAX_ENTRIES){append(node,e);return 1;}
    *splitOut=split(node,e);
    return *splitOut!=NULL;
}
void initRTree(RTree *tree){if(tree)tree->root=NULL;}
int insertOrder(RTree *tree,const Graph *g,const Order orders[],int orderIndex){
    if(!tree||!g||!orders||orderIndex<0)return 0;
    int id=orders[orderIndex].pointid;
    if(id<0||id>=g->vertexCount)return 0;
    double x=g->vertices[id].x,y=g->vertices[id].y;
    if(!isfinite(x)||!isfinite(y))return 0;
    RTreeEntry item={makePointRect(x,y),orderIndex,NULL};
    if(!tree->root){
        tree->root=newNode(1);
        if(!tree->root)return 0;
    }
    /* Reserve a potential new root before mutation to avoid allocation failure after root split. */
    RTreeNode *reserved=NULL;
    if(tree->root->count==RTREE_MAX_ENTRIES){
        reserved=newNode(0);
        if(!reserved)return 0;
    }
    RTreeNode *sibling=NULL;
    int ok=insertRecursive(tree->root,item,&sibling);
    if(!ok){free(reserved);return 0;}
    if(sibling){
        if(!reserved){reserved=newNode(0);if(!reserved)return 0;}
        RTreeEntry a={bounds(tree->root),-1,tree->root};
        RTreeEntry b={bounds(sibling),-1,sibling};
        append(reserved,a);append(reserved,b);
        tree->root=reserved;
    }else free(reserved);
    return 1;
}
static void queryNode(const RTreeNode *node,Rect q,int out[],int cap,int *found){
    if(!node)return;
    for(int i=0;i<node->count;i++){
        const RTreeEntry *e=&node->entries[i];
        if(!rectIntersect(e->rect,q))continue;
        if(node->isLeaf){if(*found<cap)out[*found]=e->orderIndex;(*found)++;}
        else queryNode(e->child,q,out,cap,found);
    }
}
int rangeQuery(const RTree *tree,Rect query,int result[],int capacity){
    if(!tree||capacity<0||(capacity>0&&!result)||query.xmin>query.xmax||query.ymin>query.ymax)return -1;
    int found=0;queryNode(tree->root,query,result,capacity,&found);
    return found; /* May exceed capacity: only first capacity indices are written. */
}
static void freeNode(RTreeNode *node){
    if(!node)return;
    if(!node->isLeaf)for(int i=0;i<node->count;i++)freeNode(node->entries[i].child);
    free(node);
}
void freeRTree(RTree *tree){if(tree){freeNode(tree->root);tree->root=NULL;}}
