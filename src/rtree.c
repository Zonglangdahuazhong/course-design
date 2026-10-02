#include"stdio.h"
#include"rtree.h"
#include"stdlib.h"


MBR order_mbr(const Order *order, const Graph *graph){


int i = order->pointid - 1;//数组下标与我随机生成的订单的节点标号差1 因为一个1开始  一个0开始和最开始生成数据结构时的from-1同理


    double x = graph->points[i].x;
    double y = graph->points[i].y;
    MBR mbr;
    mbr.min_x = x;
    mbr.max_x = x;
    mbr.min_y = y;
    mbr.max_y = y;

    return mbr;
}

//接下来实现mbr的合并  最小外包矩形


MBR mbr_combine(MBR a, MBR b)
{
MBR result;

    result.min_x = (a.min_x < b.min_x) ? a.min_x : b.min_x;
    result.min_y = (a.min_y < b.min_y) ? a.min_y : b.min_y;
    result.max_x = (a.max_x > b.max_x) ? a.max_x : b.max_x;
    result.max_y = (a.max_y > b.max_y) ? a.max_y : b.max_y;

    return result;



}//小的是两者之中最小的，大的是两者之中最大的


//接下来判断矩形是否相交  无非四种情况  A在B右 A在B左 A在B上 A在B下 分别对应下边if里的条件

int is_over(MBR a, MBR b)
  { 
 
    if(a.min_x >b.max_x || a.max_x < b.min_x || a.min_y > b.max_y || a.max_y < b.min_y)
    {
        return 0; // 不相交
    }
    else
    {
        return 1; // 相交
    }
}

RTreeNode *create_node(int is_leaf)
{
    RTreeNode *node = malloc(sizeof(RTreeNode));

    if (node == NULL) {
        return NULL;
    }

    node->is_leaf = is_leaf;
    node->count = 0;

    return node;
}//生成节点


//把订单插入叶子节点
void insert_order(RTreeNode *node, const Order *order, const Graph *graph)
{
    if (node->count >= MAX_ENTRIES) {
        return;
    }

    RTreeEntry *entry = &node->entries[node->count];//新建node时设置的count为0

    entry->mbr = order_mbr(order, graph);
    entry->child = (void *)order;//先录入叶子节点，

    node->count++;
}       







//接下来实现分裂算法   我选择二次分裂 后边有时间优化就尝试一下R*
/*

两两组合计算“浪费面积”，把组合后浪费面积最大的一对做“种子”，剩余条目逐个分配。

*/
//所以先计算面积
double R_area(MBR mbr)
{
    return (mbr.max_x - mbr.min_x) * (mbr.max_y - mbr.min_y);
}
//计算浪费面积
double mbr_waste(MBR a, MBR b)
{
    return R_area(mbr_combine(a, b)) - R_area(a) - R_area(b);
}
//接下来选择seed

void pick_seeds(RTreeEntry *entries, int count, int *seed1, int *seed2){
    double max_waste = -1.0;
    int i, j;

    for (i = 0; i < count; i++) {
        for (j = i + 1; j < count; j++) {
            double waste = mbr_waste(entries[i].mbr, entries[j].mbr);
            if (waste > max_waste) {
                max_waste = waste;
                *seed1 = i;
                *seed2 = j;
            }
        }
    }




}

//找到seeds之后  就要考虑剩下的entry怎么分配
//对于R树算法  面积越小肯定越好
/*
两组各自的外接矩形（MBR）面积之和尽可能小：说明数据聚类紧密，空洞少。

两组外接矩形的重叠面积（Overlap）尽可能小：重叠越小，后续查找时需要同时深入遍历的分支就越少。

*/
//所以  考虑如何分配  就是考虑如何使面积最小   也就是如果放进去后面积增量较小  就选择这个

double mbr_enlargement(MBR group_mbr, MBR entry_mbr)  // grop_mbr是组的MBR  entry_mbr要进入的单个mbr
{
    MBR combined = mbr_combine(group_mbr, entry_mbr);

    return R_area(combined) - R_area(group_mbr);
}

MBR node_mbr(const RTreeNode *node)
{
    MBR result;

    if (node->count == 0) {
        result.min_x = 0;
        result.min_y = 0;
        result.max_x = 0;
        result.max_y = 0;
        return result;
    }

    result = node->entries[0].mbr;

    for (int i = 1; i < node->count; i++) {
        result = mbr_combine(
            result,
            node->entries[i].mbr
        );
    }

    return result;
}//把一个node下的所有entry的MBR合并成一个大的MBR

void insert_entry(RTreeNode *node, RTreeEntry entry)
{
    if (node->count >= MAX_ENTRIES) {
        return;
    }

    node->entries[node->count] = entry;//从0开始，node中加一个entry就count加1

    node->count++;
}
//区分它于insert_order  insert_orser是指插入订单到entry，insert_entry已经有了entry然后实现插入到node中
//insert_entry(group_a, entries[2]);   把 Entry 2 放进 Group A

//选择去哪一个
int choose_group(
    const RTreeNode *group_a, const RTreeNode *group_b,const RTreeEntry *entry)
{
    MBR group_a_mbr = node_mbr(group_a);
    MBR group_b_mbr = node_mbr(group_b);//这个group_a group_b是两个seed分别插入并成为node

    double enlargement_a =
        mbr_enlargement(group_a_mbr, entry->mbr);

    double enlargement_b =
        mbr_enlargement(group_b_mbr, entry->mbr);

    if (enlargement_a < enlargement_b) {
        return 0;
    }

    if (enlargement_b < enlargement_a) {
        return 1;
    }

    // 如果扩展面积相同，则比较当前 MBR 面积
    double area_a = R_area(group_a_mbr);
    double area_b = R_area(group_b_mbr);

    if (area_a < area_b) {
        return 0;
    }

    return 1;
}  // 0则插入第一个   1则插入第二个

  


//已经吧基本函数完成   接下来实现二次分裂


void quadratic_split(RTreeEntry *entries,int count,RTreeNode **group_a,RTreeNode **group_b) // 这里使用二级指针的原因：因为 quadratic_split() 要在函数内部创建 Node，并把这个“新 Node 的地址”修改到函数外面的 group_a 变量里。
{
    int seed1;
    int seed2;

    // 1. 选择两个 Seed
    pick_seeds(entries,count, &seed1,&seed2
    );

    // 2. 创建两个新的 Group
    *group_a = create_node(1);
    *group_b = create_node(1);

    // 3. 分别放入两个 Seed
    insert_entry(*group_a,entries[seed1]
    );

    insert_entry(*group_b,entries[seed2]
    );

    // 4. 把剩余 Entry 分配到两个 Group

    int assigned = 2;
    for (int i = 0; i < count; i++) {

        // Seed 已经处理过
        if (i == seed1 || i == seed2) {
            continue;
        }


 int remaining = count - assigned;
 // A 必须拿剩余 Entry，否则达不到最小数量
    if ((*group_a)->count + remaining == MIN_ENTRIES) {

        insert_entry(*group_a, entries[i]);
    }
    // B 必须拿剩余 Entry，否则达不到最小数量
    else if ((*group_b)->count + remaining == MIN_ENTRIES) {

        insert_entry(*group_b, entries[i]);
    }

else{
        int group = choose_group(*group_a,*group_b,&entries[i]);

        if (group == 0) {
insert_entry(*group_a,entries[i]);
        }
        else {
            insert_entry(*group_b,entries[i]);
        }
    }

    assigned++;
}


}

//  二次分裂算法的结构
/*
                  5 Entries
                     │
                     ↓
              ┌──────────────┐
              │ pick_seeds() │
              └──────┬───────┘
                     │
              ┌──────┴──────┐
              ↓             ↓
           Seed 1         Seed 2
              ↓             ↓
          Group A         Group B
              │             │
              └──────┬──────┘
                     ↓
              剩余 Entry
                     │
                     ↓
             choose_group()
                 /       \
                /         \
               ↓           ↓
          Group A       Group B
               \           /
                \         /
                 ↓       ↓
              insert_entry()


*/
// 为了使两个node都有足够的 entries  应该规定一个最小数量

RTreeEntry node_to_entry(RTreeNode *node)
{
    RTreeEntry entry;

    entry.mbr = node_mbr(node);
    entry.child = (void *)node;

    return entry;
} 
//把分好的group_a  group_b 变成entry（entry指向他们，即entry的child指向这些node，这样就可以递归给父亲，形成树结构），  也就是接下来开始建立树结构
/*
Leaf Node
├── Entry 1
│   ├── mbr ───→ 一个订单的位置（一个点）
│   └── child ─→ Order 1
│
├── Entry 2
│   ├── mbr ───→ 一个订单的位置（一个点）
│   └── child ─→ Order 2
│
└── Entry 3
    ├── mbr ───→ 一个订单的位置（一个点）
    └── child ─→ Order 3

*/

/*
Internal Node
├── Entry 1
│   ├── mbr ───→ Child Node 的整体范围
│   └── child ─→ Child Node 1
│
├── Entry 2
│   ├── mbr ───→ Child Node 的整体范围
│   └── child ─→ Child Node 2
│
└── Entry 3
    ├── mbr ───→ Child Node 的整体范围
    └── child ─→ Child Node 3
*/
RTreeNode *create_root(RTreeNode *group_a, RTreeNode *group_b)
{
    RTreeNode *root = create_node(0);

    if (root == NULL)
        return NULL;

    RTreeEntry entry_a = node_to_entry(group_a);
    RTreeEntry entry_b = node_to_entry(group_b);

    insert_entry(root, entry_a);
    insert_entry(root, entry_b);

    return root;
}
// 创建根节点 加入二次分裂得到的两个group便可形成基本的树结构

/*
                         Root
                  ┌────────────────┐
                  │ is_leaf = 0    │
                  │ count = 2      │
                  ├────────────────┤
                  │ Entry A        │
                  │  ├─ mbr        │
                  │  └─ child ──────────┐
                  │                     │
                  │ Entry B             │
                  │  ├─ mbr             │
                  │  └─ child ───────┐  │
                  └──────────────────┘  │  │
                                        │  │
                         ┌──────────────┘  └──────────────┐
                         ↓                               ↓
                      Group A                         Group B
                    is_leaf = 1                     is_leaf = 1
                    count = 3                       count = 2
                    ┌───────┐                       ┌───────┐
                    │ E1    │                       │ E5    │
                    │ child ─────→ Order 1          │ child ─────→ Order 5
                    │ E2    │                       │ E4    │
                    │ child ─────→ Order 2          │ child ─────→ Order 4
                    │ E3    │
                    │ child ─────→ Order 3
                    └───────┘


*/

//接下来是普通插入


int choose_subtree(const RTreeNode *node,const RTreeEntry *entry)
{
    int best = 0;

    MBR first_mbr = node->entries[0].mbr;

    double best_enlargement =
        mbr_enlargement(first_mbr, entry->mbr);

    double best_area = R_area(first_mbr);

    for (int i = 1; i < node->count; i++) {

        MBR current_mbr = node->entries[i].mbr;

        double enlargement =
            mbr_enlargement(current_mbr, entry->mbr);

        double area = R_area(current_mbr);

        if (enlargement < best_enlargement) {
            best = i;
            best_enlargement = enlargement;
            best_area = area;
        }
        else if (enlargement == best_enlargement &&
                 area < best_area) {
            best = i;
            best_area = area;
        }
    }

    return best;
}
//假设插入第0个entry对应的node 计算出面积以及面积增量  然后依次对比插入第1，2，3个entry对应的node 比出最好的，返回entry的下标


/*
split_node()
    │
    ├── 收集旧 Entry
    ├── 加入新 Entry
    │
    └── quadratic_split()
             │
             ├── pick_seeds()
             ├── choose_group()
             └── 分成 A / B

*/  //相当于把当时完成二次分裂的测试程序封装了一下     为后续的普通插入做准备
//判断一个node满的时候才进入这个函数，也就是说node里是4个entry  然后还有一个新的entry  总共五个
void split_node(RTreeNode *node,RTreeEntry new_entry,RTreeNode **group_a,RTreeNode **group_b)
{
    RTreeEntry entries[MAX_ENTRIES + 1];

    // 复制原 Node 中的 Entry
    for (int i = 0; i < node->count; i++) {
        entries[i] = node->entries[i];
    }

    // 加入新的 Entry
    entries[node->count] = new_entry;

    // 对所有 Entry 进行二次分裂
    quadratic_split(entries,node->count + 1,group_a,group_b);
}
//接下来封装分裂后的两个group回到树的函数

/*
过程大致演示：
原本
Entry 0 → A
Entry 1 → B   ← 这个要 Split
Entry 2 → C

变成：

Entry 0 → A
Entry 1 → B1
Entry 2 → B2
Entry 3 → C


*/
// 这个函数需要两个基础的功能   1是把entry从node中删除  2是把entry插入到node的指定位置
//1、


void remove_entry(RTreeNode *node, int index)
{
    if (index < 0 || index >= node->count)
        return;

    for (int i = index; i < node->count - 1; i++) {
        node->entries[i] = node->entries[i + 1];
    }

    node->count--;
}

/*
删除 Entry 1

Entry 0
Entry 1 ← 删除
Entry 2
Entry 3

        ↓ 往前移动

Entry 0
Entry 2
Entry 3
*/

//2、 
void insert_entry_at(RTreeNode *node,int index,RTreeEntry entry )
{
    if (node == NULL)
        return;

    if (index < 0 || index > node->count)
        return;// 索引超出范围

    if (node->count >= MAX_ENTRIES)
        return;//需要去分裂

    // 从后往前移动，给新 Entry 腾出位置
    for (int i = node->count; i > index; i--) {
        node->entries[i] = node->entries[i - 1];
    }

    // 插入新 Entry
    node->entries[index] = entry;

    node->count++;
}
/*
  for (int i = node->count; i > index; i--)注意这个！
  一定要从后边开始移动
正确：
先：
[A][C][D]

D 往后：
[A][C][D][D]

C 往后：
[A][C][C][D]

最后：
[A][B][C][D]

如果从前往后：

[A][C][D]

C → 后面
[A][C][C]

D → 后面
[A][C][C][C]

*/

void replace_entry_with_split(RTreeNode *parent,int index,RTreeNode *group_a,RTreeNode *group_b)
{
    if (parent == NULL)
        return;

    if (index < 0 || index >= parent->count)
        return;

    // 1. 删除原来的 Entry
    remove_entry(parent, index);

    // 2. 把两个分裂后的 Node 转换成 Entry
    RTreeEntry entry_a = node_to_entry(group_a);
    RTreeEntry entry_b = node_to_entry(group_b);

    // 3. 在原来的位置插入两个新的 Entry
    insert_entry_at(parent, index, entry_a);
    insert_entry_at(parent, index + 1, entry_b);
}