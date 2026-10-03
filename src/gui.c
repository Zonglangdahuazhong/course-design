#include <gtk/gtk.h>
#include <cairo.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "../include/gui.h"
#include "../include/graph.h"
#include "../include/order.h"
#include "../include/rtree.h"


typedef struct {
    Graph *graph;

    double min_x;
    double max_x;
    double min_y;
    double max_y;

    Order *orders;
    int order_count;

    RTree *rtree;

    GtkWidget *drawing_area;
    GtkWidget *entry;
    GtkWidget *status_label;
} MapView;


/* =========================================================
 * 1. 计算地图经纬度范围
 * ========================================================= */
static void calculate_bounds(MapView *view)
{
    Graph *graph = view->graph;

    view->min_x = graph->points[0].x;
    view->max_x = graph->points[0].x;
    view->min_y = graph->points[0].y;
    view->max_y = graph->points[0].y;

    for (int i = 1; i < graph->sum; i++) {

        if (graph->points[i].x < view->min_x)
            view->min_x = graph->points[i].x;

        if (graph->points[i].x > view->max_x)
            view->max_x = graph->points[i].x;

        if (graph->points[i].y < view->min_y)
            view->min_y = graph->points[i].y;

        if (graph->points[i].y > view->max_y)
            view->max_y = graph->points[i].y;
    }
}


/* =========================================================
 * 2. 地图坐标 → 屏幕坐标
 * ========================================================= */
static double to_screen_x(
    MapView *view,
    double x,
    double width)
{
    double range_x = view->max_x - view->min_x;

    if (range_x == 0)
        return width / 2.0;

    return (x - view->min_x) / range_x * width;
}


static double to_screen_y(
    MapView *view,
    double y,
    double height)
{
    double range_y = view->max_y - view->min_y;

    if (range_y == 0)
        return height / 2.0;

    /*
     * Cairo 的 y 轴向下，
     * 地图的 y 轴向上，
     * 所以这里需要反过来。
     */
    return height -
           (y - view->min_y) / range_y * height;
}


/* =========================================================
 * 3. 屏幕坐标 → 地图坐标
 * ========================================================= */
static double to_map_x(
    MapView *view,
    double screen_x,
    double width)
{
    double range_x = view->max_x - view->min_x;

    if (width == 0)
        return view->min_x;

    return view->min_x +
           screen_x / width * range_x;
}


static double to_map_y(
    MapView *view,
    double screen_y,
    double height)
{
    double range_y = view->max_y - view->min_y;

    if (height == 0)
        return view->min_y;

    return view->min_y +
           (height - screen_y) / height * range_y;
}


/* =========================================================
 * 4. 根据鼠标位置寻找最近地图节点
 *
 * 返回：
 *   0 ~ graph->sum-1
 *
 * 这是 Graph.points[] 的数组下标，
 * 不是 Order.pointid。
 * ========================================================= */
static int find_nearest_node(
    MapView *view,
    double mouse_x,
    double mouse_y)
{
    Graph *graph = view->graph;

    double width =
        gtk_widget_get_allocated_width(
            view->drawing_area);

    double height =
        gtk_widget_get_allocated_height(
            view->drawing_area);

    double map_x =
        to_map_x(view, mouse_x, width);

    double map_y =
        to_map_y(view, mouse_y, height);

    int nearest = -1;

    double min_distance = 0;

    for (int i = 0; i < graph->sum; i++) {

        double dx =
            graph->points[i].x - map_x;

        double dy =
            graph->points[i].y - map_y;

        double distance =
            dx * dx + dy * dy;

        if (nearest == -1 ||
            distance < min_distance)
        {
            nearest = i;
            min_distance = distance;
        }
    }

    return nearest;
}


/* =========================================================
 * 5. 重新建立 R-tree
 *
 * 为什么需要这个函数？
 *
 * 因为 orders 是一块动态数组。
 *
 * realloc() 之后：
 *
 *     原来的 Order * 地址
 *
 * 可能发生改变。
 *
 * 而你的 R-tree 中：
 *
 *     entry->child
 *
 * 保存的就是 Order *。
 *
 * 所以如果 orders 地址改变，
 * 原来的 R-tree 就会保存悬空指针。
 *
 * 最简单可靠的处理方式：
 *
 *     订单数组变化
 *          ↓
 *     重新建立 R-tree
 * ========================================================= */
static void rebuild_rtree(MapView *view)
{
    if (view->rtree == NULL) {
        view->rtree =
            malloc(sizeof(RTree));

        if (view->rtree == NULL) {
            perror("malloc failed");
            return;
        }

        view->rtree->root = NULL;
    }

    /*
     * 释放旧树的节点。
     *
     * 注意：
     * rtree_free() 只释放 R-tree 节点，
     * 不释放 Order。
     */
    //rtree_free(view->rtree);

    /*
     * 创建新的叶子根节点
     */
    view->rtree->root =
        create_node(1);

    if (view->rtree->root == NULL) {
        printf("R-tree 根节点创建失败\n");
        return;
    }

    /*
     * 把当前所有订单重新插入
     */
    for (int i = 0;
         i < view->order_count;
         i++)
    {
        rtree_insert(
            view->rtree,
            &view->orders[i],
            view->graph
        );
    }
}


/* =========================================================
 * 6. 绘制地图
 * ========================================================= */
static gboolean draw_map(
    GtkWidget *widget,
    cairo_t *cr,
    gpointer data)
{
    MapView *view =
        (MapView *)data;

    Graph *graph =
        view->graph;

    double width =
        gtk_widget_get_allocated_width(widget);

    double height =
        gtk_widget_get_allocated_height(widget);


    /* -------------------------
     * 绘制道路
     * ------------------------- */
    for (int i = 0;
         i < graph->sum;
         i++)
    {
        Point *from =
            &graph->points[i];

        double x1 =
            to_screen_x(
                view,
                from->x,
                width
            );

        double y1 =
            to_screen_y(
                view,
                from->y,
                height
            );

        Edge *edge =
            graph->adj[i];

        while (edge != NULL) {

            Point *to =
                &graph->points[edge->to];

            double x2 =
                to_screen_x(
                    view,
                    to->x,
                    width
                );

            double y2 =
                to_screen_y(
                    view,
                    to->y,
                    height
                );

            cairo_move_to(
                cr,
                x1,
                y1
            );

            cairo_line_to(
                cr,
                x2,
                y2
            );

            cairo_stroke(cr);

            edge = edge->next;
        }
    }


    /* -------------------------
     * 绘制订单
     * ------------------------- */
    for (int i = 0;
         i < view->order_count;
         i++)
    {
        Order *order =
            &view->orders[i];

        /*
         * Order.pointid 是 1-based
         *
         * Graph.points[] 是 0-based
         */
        int point_index =
            order->pointid - 1;

        if (point_index < 0 ||
            point_index >= graph->sum)
        {
            continue;
        }

        Point *point =
            &graph->points[point_index];

        double x =
            to_screen_x(
                view,
                point->x,
                width
            );

        double y =
            to_screen_y(
                view,
                point->y,
                height
            );

        cairo_arc(
            cr,
            x,
            y,
            5.0,
            0,
            2 * 3.141592653589793
        );

        cairo_fill(cr);
    }

    return FALSE;
}


/* =========================================================
 * 7. 随机生成订单
 * ========================================================= */
static void generate_orders_callback(
    GtkWidget *button,
    gpointer data)
{
    MapView *view =
        (MapView *)data;

    const char *text =
        gtk_entry_get_text(
            GTK_ENTRY(view->entry)
        );

    int count =
        atoi(text);

    if (count <= 0) {

        gtk_label_set_text(
            GTK_LABEL(view->status_label),
            "请输入大于 0 的订单数量"
        );

        return;
    }


    /*
     * 释放旧订单
     */
    free_orders(view->orders);

    view->orders = NULL;
    view->order_count = 0;


    /*
     * 生成新订单
     */
    view->orders =
        generate(
            view->graph,
            count
        );

    if (view->orders == NULL) {

        gtk_label_set_text(
            GTK_LABEL(view->status_label),
            "订单生成失败"
        );

        return;
    }

    view->order_count =
        count;


    /*
     * 根据新的订单数组
     * 重新建立 R-tree
     */
    rebuild_rtree(view);


    char status[100];

    snprintf(
        status,
        sizeof(status),
        "当前订单：%d",
        count
    );

    gtk_label_set_text(
        GTK_LABEL(view->status_label),
        status
    );


    gtk_widget_queue_draw(
        view->drawing_area
    );
}


/* =========================================================
 * 8. 鼠标点击地图 → 创建订单
 * ========================================================= */
static gboolean map_button_press_callback(
    GtkWidget *widget,
    GdkEventButton *event,
    gpointer data)
{
    MapView *view =
        (MapView *)data;


    /*
     * 只处理鼠标左键
     */
    if (event->button != 1) {
        return FALSE;
    }


    printf(
        "鼠标点击：x=%f y=%f\n",
        event->x,
        event->y
    );


    /*
     * 找到最近节点
     */
    int node_index =
        find_nearest_node(
            view,
            event->x,
            event->y
        );

    if (node_index < 0) {
        return FALSE;
    }


    printf(
        "最近节点：%d\n",
        node_index
    );


    /*
     * 增加一个 Order
     *
     * 注意：
     * realloc 之后 orders 的地址
     * 可能发生变化。
     */
    Order *new_orders =
        realloc(
            view->orders,
            (view->order_count + 1)
            * sizeof(Order)
        );

    if (new_orders == NULL) {

        gtk_label_set_text(
            GTK_LABEL(view->status_label),
            "创建订单失败"
        );

        return FALSE;
    }

    view->orders =
        new_orders;


    /*
     * 创建新订单
     *
     * Order.id 从 1 开始
     */
    int index =
        view->order_count;

    view->orders[index].id =
        index + 1;

    /*
     * node_index 是 0-based
     *
     * Order.pointid 是 1-based
     *
     * 所以 +1
     */
    view->orders[index].pointid =
        node_index + 1;


    view->order_count++;


    /*
     * 订单数组发生了 realloc，
     * 所以必须重新建立 R-tree。
     *
     * 不能直接向旧 R-tree 插入，
     * 因为旧 R-tree 中的 Order *
     * 可能已经失效。
     */
    rebuild_rtree(view);


    printf(
        "创建订单：id=%d pointid=%d\n",
        view->orders[index].id,
        view->orders[index].pointid
    );


    char status[100];

    snprintf(
        status,
        sizeof(status),
        "当前订单：%d",
        view->order_count
    );

    gtk_label_set_text(
        GTK_LABEL(view->status_label),
        status
    );


    gtk_widget_queue_draw(
        view->drawing_area
    );

    return TRUE;
}


/* =========================================================
 * 9. R-tree 查询
 *
 * 当前先查询整个地图。
 *
 * 这一步主要是验证：
 *
 * GUI
 *   ↓
 * Order
 *   ↓
 * R-tree
 *   ↓
 * query
 *
 * 后面再把它改成鼠标框选区域。
 * ========================================================= */
static void query_rtree_callback(
    GtkWidget *button,
    gpointer data)
{
    MapView *view =
        (MapView *)data;

    if (view->rtree == NULL ||
        view->rtree->root == NULL)
    {
        gtk_label_set_text(
            GTK_LABEL(view->status_label),
            "R-tree 为空"
        );

        return;
    }


    /*
     * 查询整个地图范围
     */
    MBR query;

    query.min_x =
        view->min_x;

    query.min_y =
        view->min_y;

    query.max_x =
        view->max_x;

    query.max_y =
        view->max_y;


    /*
     * 最多保存 2000 个结果。
     *
     * 你的项目目前订单数量远低于这个值，
     * 所以足够。
     */
    Order *results[2000];


    int count =
        rtree_query(
            view->rtree->root,
            query,
            results,
            2000
        );


    printf("\n");
    printf("============================\n");
    printf("R-tree 查询\n");
    printf("============================\n");

    printf(
        "查询区域：\n"
        "(%f, %f) ~ (%f, %f)\n",
        query.min_x,
        query.min_y,
        query.max_x,
        query.max_y
    );

    printf(
        "找到 %d 个订单\n",
        count
    );


    for (int i = 0;
         i < count;
         i++)
    {
        printf(
            "Order: id=%d, pointid=%d\n",
            results[i]->id,
            results[i]->pointid
        );
    }

    printf("============================\n");
    printf("\n");


    char status[100];

    snprintf(
        status,
        sizeof(status),
        "R-tree 查询到 %d 个订单",
        count
    );

    gtk_label_set_text(
        GTK_LABEL(view->status_label),
        status
    );
}


/* =========================================================
 * 10. 窗口关闭
 * ========================================================= */
static void destroy_callback(
    GtkWidget *widget,
    gpointer data)
{
    MapView *view =
        (MapView *)data;


    /*
     * 先释放 R-tree
     *
     * R-tree 中的 child 指向 Order，
     * 所以这里不能释放 Order。
     */
    if (view->rtree != NULL) {

        //rtree_free(
        //    view->rtree
        //);

        free(view->rtree);

        view->rtree = NULL;
    }


    /*
     * 再释放订单数组
     */
    free_orders(
        view->orders
    );

    view->orders = NULL;


    gtk_main_quit();
}


/* =========================================================
 * 11. GUI 主函数
 * ========================================================= */
void gui_start(Graph *graph)
{
    MapView view;

    view.graph = graph;

    view.orders = NULL;
    view.order_count = 0;

    view.rtree =
        malloc(sizeof(RTree));

    if (view.rtree == NULL) {

        perror("malloc failed");

        return;
    }

    view.rtree->root = NULL;

    view.drawing_area = NULL;
    view.entry = NULL;
    view.status_label = NULL;


    calculate_bounds(&view);


    gtk_init(NULL, NULL);


    /* -------------------------
     * 主窗口
     * ------------------------- */
    GtkWidget *window =
        gtk_window_new(
            GTK_WINDOW_TOPLEVEL
        );

    gtk_window_set_title(
        GTK_WINDOW(window),
        "校园送 · 配送调度系统"
    );

    gtk_window_set_default_size(
        GTK_WINDOW(window),
        1200,
        800
    );


    /* -------------------------
     * 主布局
     * ------------------------- */
    GtkWidget *main_box =
        gtk_box_new(
            GTK_ORIENTATION_VERTICAL,
            5
        );

    gtk_container_add(
        GTK_CONTAINER(window),
        main_box
    );


    /* -------------------------
     * 控制栏
     * ------------------------- */
    GtkWidget *control_box =
        gtk_box_new(
            GTK_ORIENTATION_HORIZONTAL,
            5
        );

    gtk_box_pack_start(
        GTK_BOX(main_box),
        control_box,
        FALSE,
        FALSE,
        5
    );


    /* 订单数量 */
    GtkWidget *count_label =
        gtk_label_new(
            "订单数量："
        );

    gtk_box_pack_start(
        GTK_BOX(control_box),
        count_label,
        FALSE,
        FALSE,
        5
    );


    /* 输入框 */
    view.entry =
        gtk_entry_new();

    gtk_entry_set_text(
        GTK_ENTRY(view.entry),
        "10"
    );

    gtk_entry_set_width_chars(
        GTK_ENTRY(view.entry),
        8
    );

    gtk_box_pack_start(
        GTK_BOX(control_box),
        view.entry,
        FALSE,
        FALSE,
        5
    );


    /* 生成订单按钮 */
    GtkWidget *generate_button =
        gtk_button_new_with_label(
            "生成订单"
        );

    gtk_box_pack_start(
        GTK_BOX(control_box),
        generate_button,
        FALSE,
        FALSE,
        5
    );


    /*
     * R-tree 查询按钮
     */
    GtkWidget *query_button =
        gtk_button_new_with_label(
            "R树查询"
        );

    gtk_box_pack_start(
        GTK_BOX(control_box),
        query_button,
        FALSE,
        FALSE,
        5
    );


    /* 状态 */
    view.status_label =
        gtk_label_new(
            "当前订单：0"
        );

    gtk_box_pack_start(
        GTK_BOX(control_box),
        view.status_label,
        FALSE,
        FALSE,
        20
    );


    /* -------------------------
     * 地图绘图区
     * ------------------------- */
    view.drawing_area =
        gtk_drawing_area_new();


    /*
     * 允许接收鼠标点击事件
     */
    gtk_widget_add_events(
        view.drawing_area,
        GDK_BUTTON_PRESS_MASK
    );


    gtk_box_pack_start(
        GTK_BOX(main_box),
        view.drawing_area,
        TRUE,
        TRUE,
        5
    );


    /* -------------------------
     * 信号
     * ------------------------- */

    /*
     * 绘制地图
     */
    g_signal_connect(
        view.drawing_area,
        "draw",
        G_CALLBACK(draw_map),
        &view
    );


    /*
     * 点击地图
     */
    g_signal_connect(
        view.drawing_area,
        "button-press-event",
        G_CALLBACK(map_button_press_callback),
        &view
    );


    /*
     * 生成订单
     */
    g_signal_connect(
        generate_button,
        "clicked",
        G_CALLBACK(generate_orders_callback),
        &view
    );


    /*
     * R-tree 查询
     */
    g_signal_connect(
        query_button,
        "clicked",
        G_CALLBACK(query_rtree_callback),
        &view
    );


    /*
     * 关闭窗口
     */
    g_signal_connect(
        window,
        "destroy",
        G_CALLBACK(destroy_callback),
        &view
    );


    /* -------------------------
     * 显示窗口
     * ------------------------- */
    gtk_widget_show_all(window);

    gtk_main();
}

