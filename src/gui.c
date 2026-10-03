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

    /*
     * R-tree 查询区域
     *
     * query_selecting:
     *     是否正在拖拽选择区域
     *
     * query_area_valid:
     *     是否已经存在一个有效查询区域
     *
     * drag_start_x/y:
     *     鼠标按下时的屏幕坐标
     *
     * drag_end_x/y:
     *     鼠标当前/松开时的屏幕坐标
     *
     * query_mbr:
     *     最终转换得到的地图坐标区域
     */
    gboolean query_selecting;
    gboolean query_area_valid;

    double drag_start_x;
    double drag_start_y;

    double drag_end_x;
    double drag_end_y;

    MBR query_mbr;

    /*
     * 保存 R-tree 最近一次查询得到的订单。
     *
     * 这里只保存指针，不拥有 Order 的内存。
     */
    Order **query_results;
    int query_result_count;
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
     * 暂时不调用 rtree_free()。
     *
     * 因为目前你的 R-tree 没有可靠的释放函数，
     * 而且之前加入 rtree_free() 后出现了堆损坏。
     *
     * 这里直接创建新的根节点。
     */
    view->rtree->root =
        create_node(1);

    if (view->rtree->root == NULL) {
        printf("R-tree 根节点创建失败\n");
        return;
    }

    /*
     * 把当前所有订单重新插入。
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
 * 6. 判断一个 Order 是否在查询结果中
 * ========================================================= */
static gboolean is_query_result(
    MapView *view,
    Order *order)
{
    for (int i = 0;
         i < view->query_result_count;
         i++)
    {
        if (view->query_results[i] == order)
            return TRUE;
    }

    return FALSE;
}


/* =========================================================
 * 7. 绘制地图
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


        /*
         * 如果这个订单在 R-tree
         * 查询结果中，就高亮。
         */
        if (is_query_result(view, order)) {

            cairo_arc(
                cr,
                x,
                y,
                8.0,
                0,
                2 * 3.141592653589793
            );

            cairo_fill(cr);

        } else {

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
    }


    /* =====================================================
     * 绘制 R-tree 查询矩形
     * ===================================================== */
    if (view->query_selecting ||
        view->query_area_valid)
    {
        double x1 =
            view->drag_start_x;

        double y1 =
            view->drag_start_y;

        double x2 =
            view->drag_end_x;

        double y2 =
            view->drag_end_y;

        double rect_x =
            MIN(x1, x2);

        double rect_y =
            MIN(y1, y2);

        double rect_width =
            fabs(x2 - x1);

        double rect_height =
            fabs(y2 - y1);


        /*
         * 画矩形边框
         */
        cairo_rectangle(
            cr,
            rect_x,
            rect_y,
            rect_width,
            rect_height
        );

        cairo_stroke(cr);
    }


    return FALSE;
}


/* =========================================================
 * 8. 随机生成订单
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
     * 清除旧的查询结果
     */
    free(view->query_results);

    view->query_results = NULL;
    view->query_result_count = 0;


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
 * 9. 鼠标按下
 *
 * 点击：
 *     记录起点
 *
 * 后续如果发生移动：
 *     就变成框选查询区域
 * ========================================================= */
static gboolean map_button_press_callback(
    GtkWidget *widget,
    GdkEventButton *event,
    gpointer data)
{
    MapView *view =
        (MapView *)data;

    if (event->button != 1)
        return FALSE;


    view->query_selecting = TRUE;

    view->drag_start_x =
        event->x;

    view->drag_start_y =
        event->y;

    view->drag_end_x =
        event->x;

    view->drag_end_y =
        event->y;


    /*
     * 开始新的框选时，
     * 清除之前的查询结果。
     */
    free(view->query_results);

    view->query_results = NULL;
    view->query_result_count = 0;

    view->query_area_valid = FALSE;


    gtk_widget_queue_draw(
        view->drawing_area
    );

    return TRUE;
}


/* =========================================================
 * 10. 鼠标移动
 *
 * 如果正在按住左键，
 * 更新矩形右下角。
 * ========================================================= */
static gboolean map_motion_callback(
    GtkWidget *widget,
    GdkEventMotion *event,
    gpointer data)
{
    MapView *view =
        (MapView *)data;

    if (!view->query_selecting)
        return FALSE;


    view->drag_end_x =
        event->x;

    view->drag_end_y =
        event->y;


    gtk_widget_queue_draw(
        view->drawing_area
    );

    return TRUE;
}


/* =========================================================
 * 11. 鼠标松开
 *
 * 判断：
 *
 *     如果移动距离很小
 *         → 普通点击
 *         → 创建订单
 *
 *     如果移动距离明显
 *         → 框选查询区域
 * ========================================================= */
static gboolean map_button_release_callback(
    GtkWidget *widget,
    GdkEventButton *event,
    gpointer data)
{
    MapView *view =
        (MapView *)data;

    if (event->button != 1)
        return FALSE;


    view->drag_end_x =
        event->x;

    view->drag_end_y =
        event->y;


    view->query_selecting = FALSE;


    /*
     * 计算鼠标移动距离
     */
    double dx =
        view->drag_end_x -
        view->drag_start_x;

    double dy =
        view->drag_end_y -
        view->drag_start_y;

    double distance =
        sqrt(dx * dx + dy * dy);


    /*
     * 移动距离小于 5 像素：
     *
     * 认为这是普通点击。
     */
    if (distance < 5.0) {

        /*
         * 找到最近节点
         */
        int node_index =
            find_nearest_node(
                view,
                event->x,
                event->y
            );

        if (node_index < 0)
            return TRUE;


        /*
         * 增加 Order
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

            return TRUE;
        }

        view->orders =
            new_orders;


        int index =
            view->order_count;

        view->orders[index].id =
            index + 1;

        view->orders[index].pointid =
            node_index + 1;

        view->order_count++;


        /*
         * 订单数组地址可能发生变化，
         * 所以重新建立 R-tree。
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


    /*
     * ================================================
     * 真正的拖拽：
     * 建立 R-tree 查询区域
     * ================================================
     */

    double width =
        gtk_widget_get_allocated_width(
            view->drawing_area);

    double height =
        gtk_widget_get_allocated_height(
            view->drawing_area);


    /*
     * 把屏幕坐标转换成地图坐标
     */
    double map_x1 =
        to_map_x(
            view,
            view->drag_start_x,
            width
        );

    double map_y1 =
        to_map_y(
            view,
            view->drag_start_y,
            height
        );

    double map_x2 =
        to_map_x(
            view,
            view->drag_end_x,
            width
        );

    double map_y2 =
        to_map_y(
            view,
            view->drag_end_y,
            height
        );


    /*
     * 因为用户可以从左上往右下，
     * 也可以从右下往左上拖，
     *
     * 所以需要重新计算 min/max。
     */
    view->query_mbr.min_x =
        MIN(map_x1, map_x2);

    view->query_mbr.max_x =
        MAX(map_x1, map_x2);

    view->query_mbr.min_y =
        MIN(map_y1, map_y2);

    view->query_mbr.max_y =
        MAX(map_y1, map_y2);


    view->query_area_valid = TRUE;


    /*
     * 打印查询区域，
     * 方便调试。
     */
    printf("\n");
    printf("===== 设置 R-tree 查询区域 =====\n");

    printf(
        "(%f, %f) ~ (%f, %f)\n",
        view->query_mbr.min_x,
        view->query_mbr.min_y,
        view->query_mbr.max_x,
        view->query_mbr.max_y
    );


    gtk_label_set_text(
        GTK_LABEL(view->status_label),
        "查询区域已设置，请点击“R树查询”"
    );


    gtk_widget_queue_draw(
        view->drawing_area
    );


    return TRUE;
}


/* =========================================================
 * 12. R-tree 查询
 *
 * 使用鼠标拖拽得到的 MBR。
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
     * 用户还没有设置查询区域
     */
    if (!view->query_area_valid) {

        gtk_label_set_text(
            GTK_LABEL(view->status_label),
            "请先在地图上拖拽设置查询区域"
        );

        return;
    }


    /*
     * 最多保存 2000 个结果
     */
    Order *results[2000];


    int count =
        rtree_query(
            view->rtree->root,
            view->query_mbr,
            results,
            2000
        );


    /*
     * 保存查询结果，
     * 用于 GUI 高亮。
     */
    free(view->query_results);

    view->query_results = NULL;
    view->query_result_count = 0;


    if (count > 0) {

        view->query_results =
            malloc(
                count * sizeof(Order *)
            );

        if (view->query_results != NULL) {

            for (int i = 0;
                 i < count;
                 i++)
            {
                view->query_results[i] =
                    results[i];
            }

            view->query_result_count =
                count;
        }
    }


    /*
     * 终端输出
     */
    printf("\n");
    printf("============================\n");
    printf("R-tree 查询\n");
    printf("============================\n");

    printf(
        "查询区域：\n"
        "(%f, %f) ~ (%f, %f)\n",
        view->query_mbr.min_x,
        view->query_mbr.min_y,
        view->query_mbr.max_x,
        view->query_mbr.max_y
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


    /*
     * 让查询结果立即高亮
     */
    gtk_widget_queue_draw(
        view->drawing_area
    );
}


/* =========================================================
 * 13. 窗口关闭
 * ========================================================= */
static void destroy_callback(
    GtkWidget *widget,
    gpointer data)
{
    MapView *view =
        (MapView *)data;


    /*
     * 注意：
     *
     * 现在暂时不能使用 rtree_free()。
     *
     * 这里只释放 RTree 外层结构。
     *
     * R-tree 内部节点目前由程序结束时
     * 操作系统统一回收。
     */
    if (view->rtree != NULL) {

        free(view->rtree);

        view->rtree = NULL;
    }


    /*
     * 查询结果只是 Order* 指针数组，
     * 不拥有 Order。
     */
    free(view->query_results);

    view->query_results = NULL;


    /*
     * 释放订单数组
     */
    free_orders(
        view->orders
    );

    view->orders = NULL;


    gtk_main_quit();
}


/* =========================================================
 * 14. GUI 主函数
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

    /*
     * 初始化查询状态
     */
    view.query_selecting = FALSE;
    view.query_area_valid = FALSE;

    view.drag_start_x = 0;
    view.drag_start_y = 0;
    view.drag_end_x = 0;
    view.drag_end_y = 0;

    view.query_mbr.min_x = 0;
    view.query_mbr.min_y = 0;
    view.query_mbr.max_x = 0;
    view.query_mbr.max_y = 0;

    view.query_results = NULL;
    view.query_result_count = 0;


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
     * 需要：
     *
     * BUTTON_PRESS
     * BUTTON_RELEASE
     * POINTER_MOTION
     */
    gtk_widget_add_events(
        view.drawing_area,
        GDK_BUTTON_PRESS_MASK |
        GDK_BUTTON_RELEASE_MASK |
        GDK_POINTER_MOTION_MASK
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
     * 鼠标按下
     */
    g_signal_connect(
        view.drawing_area,
        "button-press-event",
        G_CALLBACK(map_button_press_callback),
        &view
    );


    /*
     * 鼠标移动
     */
    g_signal_connect(
        view.drawing_area,
        "motion-notify-event",
        G_CALLBACK(map_motion_callback),
        &view
    );


    /*
     * 鼠标松开
     */
    g_signal_connect(
        view.drawing_area,
        "button-release-event",
        G_CALLBACK(map_button_release_callback),
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